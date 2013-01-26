#include <linux/leds.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/earlysuspend.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/ctype.h>
#include <mach/mpp.h>
#include <linux/timer.h>   
#include <linux/hrtimer.h> 
#include <asm/gpio.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <mach/cci_hw_id.h> //2011/11/08 JiaHan_Li, Bug 1291 - [DA80][BSP][LED]New LED on DVT2 and further version device.

//TCA6507 register macros
#define SALVE_ID_W              							0x8A
#define SALVE_ID_R              							0x8B
#define NLED_SEL0               							0x00
#define NLED_SEL1               							0x01
#define NLED_SEL2              	 						0x02
#define NLED_FADE_ON_TIME        						0x03
#define NLED_FULLY_ON_TIME							0x04
#define NLED_FADE_OFF_TIME      						0x05
#define NLED_FIRST_FULLY_OFF_TIME					0x06
#define NLED_SECON_FULLY_OFF_TIME					0x07
#define NLED_MAX_INTENSITY      						0x08
#define NLED_ONESHOT_INTENSITY  					0x09
#define NLED_INITIALIZATION     						0x10

// LED mapping color
//B 2011/11/08 JiaHan_Li, Bug 1291 - [DA80][BSP][LED]New LED on DVT2 and further version device.
/*
#define NLED_RED_BIT_SET        (1<<0)
#define NLED_BLUE_BIT_SET      (1<<1)
#define NLED_GREEN_BIT_SET      (1<<2)

typedef enum
{
    NLED_RED = NLED_RED_BIT_SET,
    NLED_GREEN = NLED_GREEN_BIT_SET,
    NLED_BLUE = NLED_BLUE_BIT_SET
}NLED_COLOR;
*/
#define NLED_RED (1<<0)
#define NLED_GREEN (1<<2)
#define NLED_BLUE (1<<1)
#define NLED_DVT2_GREEN (1<<1)
#define NLED_DVT2_WHITE (1<<2)
//E 2011/11/08 JiaHan_Li, Bug 1291 - [DA80][BSP][LED]New LED on DVT2 and further version device.

typedef enum {
	NLED_OFF = 0,
	NLED_ON,
	NLED_FULLY_ON,
	NLED_BLINKING
}NLED_CONTROL_TYPE;

struct i2c_client *tca6507_i2c_client = NULL;

struct tca6507_led_data {
	struct led_classdev leds[3];	/* blue, green, red */
	struct i2c_client *client;
}*private_data;

struct LED_STATE {
	u8 on;
	u8 blink;
	u8 brightness;
	u8 level;
	u8 freq;
	u8 pwm;
	u8 color;//2011/05/13 JiaHan_Li, Bug 89-[BSP][LED]LED can't be triggered correctly.
} nled_state;

#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend NLED_suspend;
//B 2011/05/13 JiaHan_Li, Bug 89-[BSP][LED]LED can't be triggered correctly.
extern struct workqueue_struct *suspend_work_queue;
//B 2011/06/22 JiaHan_Li, Bug 339 - [DA80][BSP][LED] BUG: scheduling while atomic: suspend/12/0x00000002
struct deferred_led_change {
	struct work_struct led_change_work;
	struct led_classdev *led_cdev;
	int blink;
	int color;
	int freq;
	int pwm;
};
//E 2011/06/22 JiaHan_Li, Bug 339 - [DA80][BSP][LED] BUG: scheduling while atomic: suspend/12/0x00000002
//E 2011/05/13 JiaHan_Li, Bug 89-[BSP][LED]LED can't be triggered correctly.
#endif
//B 2011/05/13 JiaHan_Li, Bug 89-[BSP][LED]LED can't be triggered correctly.
static void enable_tca6507(void)
{
		gpio_direction_output(0, 1);          //GPIO output=0

	return;
}

static void disable_tca6507(void)
{
	if (!nled_state.on && !nled_state.blink) {
		gpio_direction_output(0, 0);          //GPIO output=0
	}
	return;
}
//E 2011/05/13 JiaHan_Li, Bug 89-[BSP][LED]LED can't be triggered correctly.
static int led_i2c_write(struct i2c_client *client, u8 cReg, u8 data)
{
	u8 buf[2];

	memset(buf, 0X0, sizeof(buf));

	buf[0] = cReg;
	buf[1] = data;

	return i2c_master_send(client, buf, 2);
}

u8 time_parameter(int time) {
	u16 time_param[16] = {0, 64, 128, 192, 256, 384, 512, 768, 1024, 1536, 2048, 3072, 4096, 5760, 8128, 16320};
	u8 i=0;

	for (i=1; i<16; i++) {
		if (time_param[i] > time) {
			if (time_param[i] - time > time - time_param[i-1])	
				i--;
			break;
		}
	}
	return i;
}
//B 2011/05/13 JiaHan_Li, Bug 89-[BSP][LED]LED can't be triggered correctly.
void led_brightness_set(struct led_classdev *led_cdev, enum led_brightness brightness)
{
	int color = 0, bright = 0;
    //B 2011/11/08 JiaHan_Li, Bug 1291 - [DA80][BSP][LED]New LED on DVT2 and further version device.
	if (cci_hw_id <= DVT1) {
	if (!strcmp(led_cdev->name, "red")) {
		color = NLED_RED;
	} else if (!strcmp(led_cdev->name, "blue")) {
		color = NLED_BLUE;
	} else if (!strcmp(led_cdev->name, "green")) {
		color = NLED_GREEN;
	}
	} else {
		if (!strcmp(led_cdev->name, "green")) {
			color = NLED_DVT2_GREEN;
		} else if (!strcmp(led_cdev->name, "white")) {
			color = NLED_DVT2_WHITE;
		}
	}
    //E 2011/11/08 JiaHan_Li, Bug 1291 - [DA80][BSP][LED]New LED on DVT2 and further version device.

	if (brightness == 0) {
		if (nled_state.color & color)
			nled_state.color ^= color;
		//2011/06/13 JiaHan Li, Bug 258 [DA80][BSP][LED]Cannot change the brightness of NLEDs.
		if (nled_state.level != 0 && nled_state.on == 0 && nled_state.color == 0)
				nled_state.level = 0;
	} else {
		bright = (brightness > 255) ? 255 : brightness;
		nled_state.color |= color;
		nled_state.level = (bright - 1)/17 + 1;		
	}
	nled_state.brightness = brightness;

}

void led_blink_set(struct led_classdev *led_cdev, int blink, int color)
{
	struct tca6507_led_data *led_data;
	int on_time = 0, off_time = 0, total_time = 0;
	//2011/06/13 JiaHan Li, Bug 258 [DA80][BSP][LED]Cannot change the brightness of NLEDs.
	u8 data[11] = {0x10, 0x00, 0x00, 0x00, 0x40, 0x46, 0x40, 0x46, 0x40, 0xff, 0x35};

	//B 2011/11/08 JiaHan_Li, Bug 1291 - [DA80][BSP][LED]New LED on DVT2 and further version device.
	led_data = (cci_hw_id <= DVT1) ? container_of(led_cdev, struct tca6507_led_data, leds[color-1]) :
				container_of(led_cdev, struct tca6507_led_data, leds[color-2]) ;
    //E 2011/11/08 JiaHan_Li, Bug 1291 - [DA80][BSP][LED]New LED on DVT2 and further version device.

	enable_tca6507();
	if (blink > 0) {
		nled_state.on = 0;
		nled_state.blink = nled_state.color;
	data[NLED_SEL1+1] = nled_state.blink;
	data[NLED_SEL2+1] = nled_state.blink;
		if (nled_state.freq && nled_state.pwm) {
		total_time = nled_state.freq * 50;
		on_time = (nled_state.pwm*total_time)/255;
		off_time = total_time - on_time;
		data[NLED_FULLY_ON_TIME+1] = 0x40 | time_parameter(on_time);
		data[NLED_FIRST_FULLY_OFF_TIME+1] = 0x40 | time_parameter(off_time);
		}
	} else {
		nled_state.freq = 0;
		nled_state.pwm = 0;

		if (nled_state.color == nled_state.blink) {
			nled_state.on = 0;
			nled_state.level = 0;
		} else {
			nled_state.on = nled_state.color;
		}
		
		nled_state.blink = 0;
		data[NLED_SEL1+1] = nled_state.on;
	}
	//2011/06/13 JiaHan Li, Bug 258 [DA80][BSP][LED]Cannot change the brightness of NLEDs.
	data[NLED_ONESHOT_INTENSITY+1] = 0x30 | nled_state.level;
	i2c_master_send(led_data->client, data, 11);
	disable_tca6507();
	nled_state.color = 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void change_blink(struct work_struct *blink_change_data)
{
    //2011/06/22 JiaHan_Li, Bug 339 - [DA80][BSP][LED] BUG: scheduling while atomic: suspend/12/0x00000002
	struct deferred_led_change *blink_change = container_of(blink_change_data, struct deferred_led_change, led_change_work);
	struct led_classdev *led_cdev = blink_change->led_cdev;
	int blink = blink_change->blink;
	int color = blink_change->color;

	led_blink_set(led_cdev, blink, color);

	/* Free up memory for the blink_change structure. */
	kfree(blink_change);
}

int queue_blink_change(struct led_classdev *led_cdev, int blink, int color)
{
	/* Initialize the led_change_work and its super-struct. */
    //2011/06/22 JiaHan_Li, Bug 339 - [DA80][BSP][LED] BUG: scheduling while atomic: suspend/12/0x00000002
	struct deferred_led_change *blink_change = kzalloc(sizeof(struct deferred_led_change), GFP_KERNEL);

	if (!blink_change)
		return -ENOMEM;

	blink_change->led_cdev = led_cdev;
	blink_change->blink = blink;
	blink_change->color = color;
    //B 2011/06/22 JiaHan_Li, Bug 339 - [DA80][BSP][LED] BUG: scheduling while atomic: suspend/12/0x00000002
	INIT_WORK(&(blink_change->led_change_work), change_blink);
	queue_work(suspend_work_queue, &(blink_change->led_change_work));
    //E 2011/06/22 JiaHan_Li, Bug 339 - [DA80][BSP][LED] BUG: scheduling while atomic: suspend/12/0x00000002
	return 0;

}
#endif

static ssize_t led_blink_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", (nled_state.blink) ? 1 : 0);
}
//B 20110520 Jiahan_Li, Bug 120 Integrate TCA6507 LED driver into BSP2150
static ssize_t led_blink_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	ssize_t ret = -EINVAL;
	char *after;
	int blink = simple_strtoul(buf, &after, 10);
	int color = 0;
	size_t count = after - buf;

	if (isspace(*after))
		count++;

	if (count == size) {
		ret = count;
        //B 2011/11/08 JiaHan_Li, Bug 1291 - [DA80][BSP][LED]New LED on DVT2 and further version device.
		if (cci_hw_id <= DVT1) {
		if (!strcmp(led_cdev->name, "red")) {
			color = NLED_RED;
		} else if (!strcmp(led_cdev->name, "blue")) {
			color = NLED_BLUE;
		} else if (!strcmp(led_cdev->name, "green")) {
			color = NLED_GREEN;
		}
		} else {
			if (!strcmp(led_cdev->name, "green")) {
				color = NLED_DVT2_GREEN;
			} else if (!strcmp(led_cdev->name, "white")) {
				color = NLED_DVT2_WHITE;
			}
		}
        //E 2011/11/08 JiaHan_Li, Bug 1291 - [DA80][BSP][LED]New LED on DVT2 and further version device.

		if (!(led_cdev->flags & LED_SUSPENDED)) {
#ifdef CONFIG_HAS_EARLYSUSPEND
			if (queue_blink_change(led_cdev, blink, color) != 0)
#endif
			led_blink_set(led_cdev, blink, color);
		}
	}
	return ret;
}
//E 2011/05/13 JiaHan_Li, Bug 89-[BSP][LED]LED can't be triggered correctly.
static DEVICE_ATTR(blink, 0644, led_blink_show, led_blink_store);
//B 2011/06/22 JiaHan_Li, Bug 339 - [DA80][BSP][LED] BUG: scheduling while atomic: suspend/12/0x00000002
void led_freq_set(struct led_classdev *led_cdev, int freq)
{
	nled_state.freq = freq;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void change_freq(struct work_struct *freq_change_data)
{
	struct deferred_led_change *freq_change = container_of(freq_change_data, struct deferred_led_change, led_change_work);
	struct led_classdev *led_cdev = freq_change->led_cdev;
	int freq = freq_change->freq;

	led_freq_set(led_cdev, freq);

	/* Free up memory for the freq_change structure. */
	kfree(freq_change);
}

int queue_freq_change(struct led_classdev *led_cdev, int freq)
{
	/* Initialize the led_change_work and its super-struct. */
	struct deferred_led_change *freq_change = kzalloc(sizeof(struct deferred_led_change), GFP_KERNEL);

	if (!freq_change)
		return -ENOMEM;

	freq_change->led_cdev = led_cdev;
	freq_change->freq = freq;

	INIT_WORK(&(freq_change->led_change_work), change_freq);
	queue_work(suspend_work_queue, &(freq_change->led_change_work));

	return 0;

}
#endif
//E 2011/06/22 JiaHan_Li, Bug 339 - [DA80][BSP][LED] BUG: scheduling while atomic: suspend/12/0x00000002

static ssize_t led_grpfreq_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", nled_state.freq);
}

static ssize_t led_grpfreq_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
    //2011/06/22 JiaHan_Li, Bug 339 - [DA80][BSP][LED] BUG: scheduling while atomic: suspend/12/0x00000002
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	ssize_t ret = -EINVAL;
	char *after;
	u32 freq = simple_strtoul(buf, &after, 10);
	size_t count = after - buf;

	if (isspace(*after))
		count++;

	if (count == size) {
		ret = count;
		
//B 2011/06/22 JiaHan_Li, Bug 339 - [DA80][BSP][LED] BUG: scheduling while atomic: suspend/12/0x00000002	
		if (!(led_cdev->flags & LED_SUSPENDED)) {
#ifdef CONFIG_HAS_EARLYSUSPEND
			if (queue_freq_change(led_cdev,freq) != 0)
#endif
				led_freq_set(led_cdev, freq);
			
		}
//E 2011/06/22 JiaHan_Li, Bug 339 - [DA80][BSP][LED] BUG: scheduling while atomic: suspend/12/0x00000002
	}

	return ret;
}

static DEVICE_ATTR(grpfreq, 0644, led_grpfreq_show, led_grpfreq_store);
//B 2011/06/22 JiaHan_Li, Bug 339 - [DA80][BSP][LED] BUG: scheduling while atomic: suspend/12/0x00000002
void led_pwm_set(struct led_classdev *led_cdev, int pwm)
{
	nled_state.pwm = pwm;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void change_pwm(struct work_struct *pwm_change_data)
{
	struct deferred_led_change *pwm_change = container_of(pwm_change_data, struct deferred_led_change, led_change_work);
	struct led_classdev *led_cdev = pwm_change->led_cdev;
	int pwm = pwm_change->pwm;

	led_pwm_set(led_cdev, pwm);

	/* Free up memory for the pwm_change structure. */
	kfree(pwm_change);
}

int queue_pwm_change(struct led_classdev *led_cdev, int pwm)
{
	/* Initialize the led_change_work and its super-struct. */
	struct deferred_led_change *pwm_change = kzalloc(sizeof(struct deferred_led_change), GFP_KERNEL);

	if (!pwm_change)
		return -ENOMEM;

	pwm_change->led_cdev = led_cdev;
	pwm_change->pwm = pwm;

	INIT_WORK(&(pwm_change->led_change_work), change_pwm);
	queue_work(suspend_work_queue, &(pwm_change->led_change_work));

	return 0;

}
#endif
//E 2011/06/22 JiaHan_Li, Bug 339 - [DA80][BSP][LED] BUG: scheduling while atomic: suspend/12/0x00000002

static ssize_t led_grppwm_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", nled_state.pwm);
}

static ssize_t led_grppwm_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
    //2011/06/22 JiaHan_Li, Bug 339 - [DA80][BSP][LED] BUG: scheduling while atomic: suspend/12/0x00000002
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	ssize_t ret = -EINVAL;
	char *after;
	u32 pwm = simple_strtoul(buf, &after, 10);
	size_t count = after - buf;

	if (isspace(*after))
		count++;

	if (count == size) {
		ret = count;

//B 2011/06/22 JiaHan_Li, Bug 339 - [DA80][BSP][LED] BUG: scheduling while atomic: suspend/12/0x00000002
		if (!(led_cdev->flags & LED_SUSPENDED)) {
#ifdef CONFIG_HAS_EARLYSUSPEND
//B 2011/07/08 JiaHan_Li, Bug 438 - [DA80][BSP][LED][MILBAIDU-99]Can't change the blinking rate of the red LED.
			if (queue_pwm_change(led_cdev, pwm) != 0)
#endif
				led_pwm_set(led_cdev, pwm);
//E 2011/07/08 JiaHan_Li, Bug 438 - [DA80][BSP][LED][MILBAIDU-99]Can't change the blinking rate of the red LED.			
		}	
//E 2011/06/22 JiaHan_Li, Bug 339 - [DA80][BSP][LED] BUG: scheduling while atomic: suspend/12/0x00000002
	}
	return ret;
}
//E 20110520 Jiahan_Li, Bug 120 Integrate TCA6507 LED driver into BSP2150
static DEVICE_ATTR(grppwm, 0644, led_grppwm_show, led_grppwm_store);

static int tca6507_remove(struct i2c_client *client)
{
	int i;
	struct tca6507_led_data *data = i2c_get_clientdata(client);
	printk("%s\n", __func__);
	
    //B 2011/11/08 JiaHan_Li, Bug 1291 - [DA80][BSP][LED]New LED on DVT2 and further version device.
	if (cci_hw_id <= DVT1) {
	for (i = 0; i < 3; i++) {
		led_classdev_unregister(&data->leds[i]);
	}
	} else {
	        for (i = 0; i < 2; i++) {
                     led_classdev_unregister(&data->leds[i]);
               }
	}
    //E 2011/11/08 JiaHan_Li, Bug 1291 - [DA80][BSP][LED]New LED on DVT2 and further version device.
	kfree(data);
	i2c_set_clientdata(client, NULL);

	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
void NLED_early_suspend(struct early_suspend *h)
{
	disable_tca6507();    //cut TCA6507 power
}

void NLED_later_resume(struct early_suspend *h)
{
	enable_tca6507();
}
#endif


static int __devinit leds_tca6507_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct tca6507_led_data *data;
	int rc = 0;
	int ret = 0;
	int i, j;

	printk("%s\n", __func__);
	if (!(data = kmalloc(sizeof(struct tca6507_led_data), GFP_KERNEL))) {
		printk("NLED %s: error to malloc client memory!\n", __func__);
		rc = -ENOMEM;
		goto err_alloc_failed;
	}

	ret = led_i2c_write(client, NLED_SEL0, 0x00);
	if (ret < 0) goto err_probe_failed;
	ret = led_i2c_write(client, NLED_SEL1, 0x00);
	if (ret < 0) goto err_probe_failed;
	ret = led_i2c_write(client, NLED_SEL2, 0x00);
	if (ret < 0) goto err_probe_failed;
	
	tca6507_i2c_client = client;
	data->client = client;
	i2c_set_clientdata(client, data);
    //B 2011/11/08 JiaHan_Li, Bug 1291 - [DA80][BSP][LED]New LED on DVT2 and further version device.
	if (cci_hw_id <= DVT1) {
	data->leds[0].name = "red";
	data->leds[0].brightness_set = led_brightness_set;
	data->leds[1].name = "blue";
	data->leds[1].brightness_set = led_brightness_set;
	data->leds[2].name = "green";
	data->leds[2].brightness_set = led_brightness_set;

	for (i = 0; i < 3; i++) {	/* red, green, blue */
		ret = led_classdev_register(NULL, &data->leds[i]);
		if (ret) {
			printk(KERN_ERR "tca6507: led_classdev_register failed\n");
			goto err_led_classdev_register_failed;
		}
	}
	} else {
		data->leds[0].name = "green";
		data->leds[0].brightness_set = led_brightness_set;
		data->leds[1].name = "white";
		data->leds[1].brightness_set = led_brightness_set;

		for (i = 0; i < 2; i++) {	/* green, white */
			ret = led_classdev_register(NULL, &data->leds[i]);
			if (ret) {
				printk(KERN_ERR "tca6507: led_classdev_register failed\n");
				goto err_led_classdev_register_failed;
			}
		}

	}
    //E 2011/11/08 JiaHan_Li, Bug 1291 - [DA80][BSP][LED]New LED on DVT2 and further version device.
//B 2011/05/13 JiaHan_Li, Bug 89-[BSP][LED]LED can't be triggered correctly.
	for (i = 0; i < 1; i++) {
		ret = device_create_file(data->leds[i].dev, &dev_attr_blink);
		if (ret) {
			printk(KERN_ERR "tca6507: device_create_file failed\n");
			goto err_out_attr_blink;
		}
		ret = sysfs_create_link(&data->leds[i].dev->kobj, &data->leds[i].dev->kobj, "device");
		if (ret) {
			printk(KERN_ERR "tca6507: sysfs_create_link failed\n");
			goto err_out_attr_link;
		}

		ret = device_create_file(data->leds[i].dev, &dev_attr_grppwm);
	if (ret) {
		printk(KERN_ERR "tca6507: create dev_attr_grppwm failed\n");
		goto err_out_attr_grppwm;
	}
		ret = device_create_file(data->leds[i].dev, &dev_attr_grpfreq);
	if (ret) {
		printk(KERN_ERR "tca6507: create dev_attr_grpfreq failed\n");
		goto err_out_attr_grpfreq;
	}

	}
//E 2011/05/13 JiaHan_Li, Bug 89-[BSP][LED]LED can't be triggered correctly.
#ifdef CONFIG_HAS_EARLYSUSPEND
	NLED_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB;
	NLED_suspend.suspend = NLED_early_suspend;
	NLED_suspend.resume = NLED_later_resume;
	register_early_suspend(&NLED_suspend);
#endif

	private_data = data;
       return 0;

err_out_attr_grpfreq:
	device_remove_file(data->leds[0].dev, &dev_attr_grppwm);
err_out_attr_grppwm:
err_out_attr_link:
err_out_attr_blink:
	for (j = 0; j < i; j++)
		device_remove_file(data->leds[j].dev, &dev_attr_blink);
	i = (cci_hw_id <= DVT1) ? 3: 2;//2011/11/08 JiaHan_Li, Bug 1291 - [DA80][BSP][LED]New LED on DVT2 and further version device.
err_led_classdev_register_failed:
	for (j = 0; j < i; j++) {
		led_classdev_unregister(&data->leds[j]);
	}
err_probe_failed:
	kfree(data);
	i2c_set_clientdata(client, NULL);
	
err_alloc_failed:		
	printk("NLED %s: - probe_failed...\n", __func__);
       return rc;

}

static const struct i2c_device_id tca6507_id[] = {
	{ "tca6507", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, tca6507_id);

static struct i2c_driver tca6507_i2c_driver = {
        .driver = {
//		.owner	= THIS_MODULE,
		.name = "tca6507",
	}, 
        .probe  = leds_tca6507_probe,
        .remove = __devexit_p(tca6507_remove),
        .id_table = tca6507_id,
};

static int __init leds_tca6507_init(void)
{
       int ret = 0;
	printk("%s\n", __func__);
	if (gpio_request(0, "TCA6507_EN"))
			printk(KERN_ERR "failed to request gpio TCA6507_EN\n");
	gpio_direction_output(0, 1);          //GPIO output=0
       ret = i2c_add_driver(&tca6507_i2c_driver);
	if (ret) {
		printk(KERN_ERR "leds_tca6507_init: i2c_add_driver() failed\n");
	}
	return 0;
}

static void __exit leds_tca6507_exit(void)
{
	printk("%s\n", __func__);
	i2c_del_driver(&tca6507_i2c_driver);
	if (tca6507_i2c_client) {
		kfree(tca6507_i2c_client);
	}
	tca6507_i2c_client = NULL;
}

module_init(leds_tca6507_init);
module_exit(leds_tca6507_exit);

MODULE_DESCRIPTION("LEDs driver for Qualcomm MSM8660 on DA80/82");
MODULE_AUTHOR("Jia-Han Li");
MODULE_LICENSE("GPL v2");

