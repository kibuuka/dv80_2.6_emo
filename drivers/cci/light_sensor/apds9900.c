#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <asm/atomic.h>
#include <linux/earlysuspend.h>
#include <linux/wakelock.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>

#define APDS9900_DRV_NAME	"apds9900"
#define DRIVER_VERSION		"1.0.1"
#define DEFAULT_DELAY_TIME 	500
#define APDS990_ALS		(1 << 0)
#define APDS990_PROXIMITY	(1 << 1)
#define APDS990_ALL		(APDS990_ALS | APDS990_PROXIMITY)

#define prox	1	/* for p-sensor input device*/

//#define GPIO_LIGHT XXX
#define GPIO_PROX 52

#define PROXIMITY_ADC_NEAR	300//150
#define PROXIMITY_ADC_FAR	250//50

//-------------- Define i2c Command ---------------//
#define APDS9900_ENABLE_REG	0x00
#define APDS9900_ATIME_REG	0x01
#define APDS9900_PTIME_REG	0x02
#define APDS9900_WTIME_REG	0x03
#define APDS9900_AILTL_REG	0x04
#define APDS9900_AILTH_REG	0x05
#define APDS9900_AIHTL_REG	0x06
#define APDS9900_AIHTH_REG	0x07
#define APDS9900_PILTL_REG	0x08
#define APDS9900_PILTH_REG	0x09
#define APDS9900_PIHTL_REG	0x0A
#define APDS9900_PIHTH_REG	0x0B
#define APDS9900_PERS_REG	0x0C
#define APDS9900_CONFIG_REG	0x0D
#define APDS9900_PPCOUNT_REG	0x0E
#define APDS9900_CONTROL_REG	0x0F
#define APDS9900_REV_REG	0x11
#define APDS9900_ID_REG		0x12
#define APDS9900_STATUS_REG	0x13
#define APDS9900_CDATAL_REG	0x14
#define APDS9900_CDATAH_REG	0x15
#define APDS9900_IRDATAL_REG	0x16
#define APDS9900_IRDATAH_REG	0x17
#define APDS9900_PDATAL_REG	0x18
#define APDS9900_PDATAH_REG	0x19

#define CMD_BYTE		0x80
#define CMD_WORD		0xA0
#define CMD_SPECIAL		0xE0

#define CMD_CLR_PS_INT		0xE5
#define CMD_CLR_ALS_INT		0xE6
#define CMD_CLR_PS_ALS_INT	0xE7

#define ENABLE_REG_BIT_PON	(1 << 0)
#define ENABLE_REG_BIT_AEN	(1 << 1)
#define ENABLE_REG_BIT_PEN	(1 << 2)
#define ENABLE_REG_BIT_WEN	(1 << 3)
#define ENABLE_REG_BIT_AIEN	(1 << 4)
#define ENABLE_REG_BIT_PIEN	(1 << 5)

//-------------- early suspend and late resume ---------------//
//[Bug 798] Luke, 2011,0902,Turn off sensors power when device suspend
#define APDS9900_EARLY_SUSPEND     1
//[Bug 798] Luke, 2011,0902,Turn off sensors power when device suspend
#if APDS9900_EARLY_SUSPEND
static void apds9900_early_suspend(struct early_suspend *h);
static void apds9900_late_resume(struct early_suspend *h);
#endif

//-------------- Define for debug ---------------//
#define dm1 0	/*for set_XXX func.*/
#define dm2 0	/*for store and show func.*/
#define dm3 0	/*for get_sensor_light_data*/

//-------------- Define Sensor Data Struct ---------------//
struct apds9900_data {
	struct i2c_client *client;
	struct mutex update_lock;
	struct mutex data_mutex;
	struct work_struct prox_work;
	struct delayed_work work;
	struct input_dev *input;

	atomic_t enable;
	atomic_t delay;
	atomic_t last;
	atomic_t last_status;

	unsigned int atime;
	unsigned int ptime;
	unsigned int wtime;
	unsigned int ailt;
	unsigned int aiht;
	unsigned int pilt;
	unsigned int piht;
	unsigned int pers;
	unsigned int config;
	unsigned int ppcount;
	unsigned int control;
//[Bug 798] Luke, 2011,0902,Turn off sensors power when device suspend
#if APDS9900_EARLY_SUSPEND
	struct early_suspend early_suspend;
#endif	
};


//-------------- Global para. --------------//
static struct i2c_client *this_client;
static struct apds9900_data *this_data;
static struct apds9900_data *this_data_p;
static int last_enable_value = 0;
static int read_count = 0, read_count_p = 0;
#if prox
static struct regulator *pm8058_l10;
static struct wake_lock wakelock_prox;
static int proximity_enable_wakeup = 1;
static int prox_irq = -1;
static int prox_value = 0;
//static int suspend_mode = 0;
#endif // #if prox

//-------------- Global functions ---------------//
static int apds9900_set_enable(struct i2c_client *client, unsigned int power, unsigned int interrupt)
{
	struct apds9900_data *data = i2c_get_clientdata(client);
	unsigned int enable_reg_value = 0;
	int ret;

	mutex_lock(&data->update_lock);

	if(power & APDS990_ALS || power & APDS990_PROXIMITY)
	{
		enable_reg_value |= (ENABLE_REG_BIT_PON | ENABLE_REG_BIT_WEN);
		if(power & APDS990_ALS)
		{
			enable_reg_value |= ENABLE_REG_BIT_AEN;
		}
		if(power & APDS990_PROXIMITY)
		{
			enable_reg_value |= ENABLE_REG_BIT_PEN;
		}

		if(interrupt & APDS990_ALS && interrupt & APDS990_PROXIMITY)
		{
			ret = i2c_smbus_write_byte(client, CMD_CLR_PS_ALS_INT);
			enable_reg_value |= (ENABLE_REG_BIT_AIEN | ENABLE_REG_BIT_PIEN);
		}
		else if(interrupt & APDS990_ALS)
		{
			ret = i2c_smbus_write_byte(client, CMD_CLR_ALS_INT);
			enable_reg_value |= ENABLE_REG_BIT_AIEN;
		}
		else if(interrupt & APDS990_PROXIMITY)
		{
			ret = i2c_smbus_write_byte(client, CMD_CLR_PS_INT);
			enable_reg_value |= ENABLE_REG_BIT_PIEN;
		}
	}

	ret = i2c_smbus_write_byte_data(client, CMD_BYTE | APDS9900_ENABLE_REG, enable_reg_value);

	mutex_unlock(&data->update_lock);

	atomic_set(&data->enable, enable_reg_value);

#if dm1
	printk(KERN_ERR "[apds9900]apds9900_set_enable>>>enable:0x%X\n", enable_reg_value);
#endif

	return ret;
}

static int apds9900_set_atime(struct i2c_client *client, int atime)
{
	struct apds9900_data *data = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_byte_data(client, CMD_BYTE | APDS9900_ATIME_REG, atime);
	mutex_unlock(&data->update_lock);

	switch(atime)
	{
		case 0xff:
			data->atime = 272;	/*2.72ms*/
			break;

		case 0xf6:
			data->atime = 2720;	/*27.20ms*/
			break;

		case 0xde:
			data->atime = 10064;	/*100.64ms*/
			break;

		case 0xc0:
			data->atime = 17680;	/*176.80ms*/
			break;

		case 0x00:
			data->atime = 69632;	/*696.32ms*/
			break;
	}

#if dm1
	printk(KERN_ERR "[apds9900]apds9900_set_atime>>>atime:%d\n", data->atime);
#endif

	return ret;
}

static int apds9900_get_atime(struct i2c_client *client)
{
	struct apds9900_data *data = i2c_get_clientdata(client);
	int atime;

	atime = data->atime;

	return atime;
}

static int apds9900_set_ptime(struct i2c_client *client, int ptime)
{
	struct apds9900_data *data = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_byte_data(client, CMD_BYTE | APDS9900_PTIME_REG, ptime);
	mutex_unlock(&data->update_lock);

	if(ptime == 0xff)
	{
		data->ptime = 272;	/*2.72ms*/
	}

#if dm1
	printk(KERN_ERR "[apds9900]apds9900_set_ptime>>>ptime:%d\n", data->ptime);
#endif

	return ret;
}

static int apds9900_get_ptime(struct i2c_client *client)
{
	struct apds9900_data *data = i2c_get_clientdata(client);
	int ptime;

	ptime = data->ptime;

	return ptime;
}

static int apds9900_set_wtime(struct i2c_client *client, int wtime)
{
	struct apds9900_data *data = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_byte_data(client, CMD_BYTE | APDS9900_WTIME_REG, wtime);
	mutex_unlock(&data->update_lock);

	switch(wtime)
	{
		case 0xff:
			data->wtime = 272;	/*2.72ms*/
			break;

		case 0xb6:
			data->wtime = 20129;	/*201.29ms*/
			break;

		case 0x00:
			data->wtime = 69632;	/*696.32ms*/
			break;
	}

#if dm1
	printk(KERN_ERR "[apds9900]apds9900_set_wtime>>>wtime:%d\n", data->wtime);
#endif

	return ret;
}

static int apds9900_get_wtime(struct i2c_client *client)
{
	struct apds9900_data *data = i2c_get_clientdata(client);
	int wtime;

	wtime = data->wtime;

	return wtime;
}

static int apds9900_set_ailt(struct i2c_client *client, int threshold)
{
	struct apds9900_data *data = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_word_data(client, CMD_WORD | APDS9900_AILTL_REG, threshold);
	mutex_unlock(&data->update_lock);

	data->ailt = threshold;

#if dm1
	printk(KERN_ERR "[apds9900]apds9900_set_ailt>>>threshold:%d\n", data->ailt);
#endif

	return ret;
}

static int apds9900_set_aiht(struct i2c_client *client, int threshold)
{
	struct apds9900_data *data = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_word_data(client, CMD_WORD | APDS9900_AIHTL_REG, threshold);
	mutex_unlock(&data->update_lock);

	data->aiht = threshold;

#if dm1
	printk(KERN_ERR "[apds9900]apds9900_set_aiht>>>threshold:%d\n", data->aiht);
#endif

	return ret;
}

static int apds9900_set_pilt(struct i2c_client *client, int threshold)
{
	struct apds9900_data *data = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_word_data(client, CMD_WORD | APDS9900_PILTL_REG, threshold);
	mutex_unlock(&data->update_lock);

	data->pilt = threshold;

#if dm1
	printk(KERN_ERR "[apds9900]apds9900_set_pilt>>>threshold:%d\n", data->pilt);
#endif

	return ret;
}

static int apds9900_set_piht(struct i2c_client *client, int threshold)
{
	struct apds9900_data *data = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_word_data(client, CMD_WORD | APDS9900_PIHTL_REG, threshold);
	mutex_unlock(&data->update_lock);

	data->piht = threshold;

#if dm1
	printk(KERN_ERR "[apds9900]apds9900_set_piht>>>threshold:%d\n", data->piht);
#endif

	return ret;
}

static int apds9900_set_pers(struct i2c_client *client, int pers)
{
	struct apds9900_data *data = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_byte_data(client, CMD_BYTE | APDS9900_PERS_REG, pers);
	mutex_unlock(&data->update_lock);

	data->pers = pers;

#if dm1
	printk(KERN_ERR "[apds9900]apds9900_set_pers>>>pers:%d\n", data->pers);
#endif

	return ret;
}

static int apds9900_set_config(struct i2c_client *client, int config)
{
	struct apds9900_data *data = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_byte_data(client, CMD_BYTE | APDS9900_CONFIG_REG, config);
	mutex_unlock(&data->update_lock);

	data->config = config;

#if dm1
	printk(KERN_ERR "[apds9900]apds9900_set_config>>>config:%d\n", data->config);
#endif

	return ret;
}

static int apds9900_set_ppcount(struct i2c_client *client, int ppcount)
{
	struct apds9900_data *data = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_byte_data(client, CMD_BYTE | APDS9900_PPCOUNT_REG, ppcount);
	mutex_unlock(&data->update_lock);

	data->ppcount = ppcount;

#if dm1
	printk(KERN_ERR "[apds9900]apds9900_set_ppcount>>>ppcount:%d\n", data->ppcount);
#endif

	return ret;
}

static int apds9900_set_control(struct i2c_client *client, int control)
{
	struct apds9900_data *data = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_byte_data(client, CMD_BYTE | APDS9900_CONTROL_REG, control);
	mutex_unlock(&data->update_lock);

	data->control = control;

#if dm1
	printk(KERN_ERR "[apds9900]apds9900_set_control>>>control:%d\n", data->control);
#endif

	return ret;
}

static int apds9900_get_control(struct i2c_client *client)
{
	struct apds9900_data *data = i2c_get_clientdata(client);
	int control;

	control = data->control;

	return control;
}

//-------------- Init para. function ---------------//
static int apds9900_set_registers(struct i2c_client *client)
{
	int ret = 0;

	printk("[prox]%s():apds9900_set_enable(client, 0, 0)\n", __func__);
	ret = apds9900_set_enable(client, 0, 0);
	if(ret < 0)
	{
		goto err_enable;
	}
	ret = apds9900_set_atime(client, 0xff);
	if(ret < 0)
	{
		goto err_atime;
	}
	ret = apds9900_set_ptime(client, 0xff);
	if(ret < 0)
	{
		goto err_ptime;
	}
	ret = apds9900_set_wtime(client, 0xff);
	if(ret < 0)
	{
		goto err_wtime;
	}
	ret = apds9900_set_ailt(client, 0);
	if(ret < 0)
	{
		goto err_ailt;
	}
	ret = apds9900_set_aiht(client, 0);
	if(ret < 0)
	{
		goto err_aiht;
	}
//	ret = apds9900_set_pilt(client, 0);//disable
	ret = apds9900_set_pilt(client, 0);//close trigger
//	ret = apds9900_set_pilt(client, PROXIMITY_ADC_FAR);//leave trigger
	if(ret < 0)
	{
		goto err_pilt;
	}
//	ret = apds9900_set_piht(client, 0);//disable
	ret = apds9900_set_piht(client, PROXIMITY_ADC_NEAR);//close trigger
//	ret = apds9900_set_piht(client, 0);//leave trigger
	if(ret < 0)
	{
		goto err_piht;
	}
	ret = apds9900_set_pers(client, 0x10);
	if(ret < 0)
	{
		goto err_pers;
	}
	ret = apds9900_set_config(client, 0);
	if(ret < 0)
	{
		goto err_config;
	}
	ret = apds9900_set_ppcount(client, 1);
	if(ret < 0)
	{
		goto err_ppcount;
	}
	ret = apds9900_set_control(client, 32); //0x20
	if(ret < 0)
	{
		goto err_control;
	}

	return ret;

err_enable:
	printk(KERN_INFO "apds9900_set_enable fail\n");
	return ret;
err_atime:
	printk(KERN_INFO "apds9900_set_atime fail\n");
	return ret;
err_ptime:
	printk(KERN_INFO "apds9900_set_ptime fail\n");
	return ret;
err_wtime:
	printk(KERN_INFO "apds9900_set_wtime fail\n");
	return ret;
err_ailt:
	printk(KERN_INFO "apds9900_set_ailt fail\n");
	return ret;
err_aiht:
	printk(KERN_INFO "apds9900_set_aiht fail\n");
	return ret;
err_pilt:
	printk(KERN_INFO "apds9900_set_pilt fail\n");
	return ret;
err_piht:
	printk(KERN_INFO "apds9900_set_piht fail\n");
	return ret;
err_pers:
	printk(KERN_INFO "apds9900_set_pers fail\n");
	return ret;
err_config:
	printk(KERN_INFO "apds9900_set_config fail\n");
	return ret;
err_ppcount:
	printk(KERN_INFO "apds9900_set_ppcount fail\n");
	return ret;
err_control:
	printk(KERN_INFO "apds9900_set_control fail\n");
	return ret;
}

//-------------- ATTR functions ---------------//
//Enable
static ssize_t apds9900_show_enable(struct device *dev, struct device_attribute *attr, char *buf)
{
	//struct i2c_client *client = to_i2c_client(dev);
	struct apds9900_data *data = i2c_get_clientdata(this_client);
	int enable = atomic_read(&data->enable);

#if dm2
	printk(KERN_ERR "[apds9900]apds9900_show_enable=0x%X\n", (enable & (APDS990_ALL << 1)) >> 1);
#endif

	return sprintf(buf, "%d\n", (enable & (APDS990_ALL << 1)) >> 1);
}

static ssize_t apds9900_store_enable(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
        // [Bug 784] Luke 2011,0901,
	//struct i2c_client *client = to_i2c_client(dev);
	unsigned long val = simple_strtoul(buf, NULL, 10);
	int ret = 0;
	int delay = DEFAULT_DELAY_TIME;

	printk(KERN_ERR "[apds9900]apds9900_store_enable:val=0x%lX, last_enable_value=0x%X\n", val, last_enable_value);

	if(val != last_enable_value)//sensor power has been changed
	{
		if((val & APDS990_ALS) != (last_enable_value & APDS990_ALS))//APDS990_ALS
		{
			if(last_enable_value & APDS990_ALS)
			{
				cancel_delayed_work_sync(&this_data->work);
			}
			else
			{
				schedule_delayed_work(&this_data->work, msecs_to_jiffies(delay) + 1);
			}

			if(val & APDS990_ALS)
			{
				atomic_set(&this_data->last_status, 1);
			}
			else
			{
				atomic_set(&this_data->last_status, 0);
			}
		}

		if((val & APDS990_PROXIMITY) != (last_enable_value & APDS990_PROXIMITY))//APDS990_PROXIMITY
		{
			if(last_enable_value & APDS990_PROXIMITY)
			{
				cancel_delayed_work_sync(&this_data_p->work);
			}
			else
			{
				schedule_delayed_work(&this_data_p->work, msecs_to_jiffies(delay) + 1);
			}

			if(val & APDS990_PROXIMITY)
			{
				atomic_set(&this_data_p->last_status, 1);
			}
			else
			{
				atomic_set(&this_data_p->last_status, 0);
			}
		}

		last_enable_value = val;

		if(proximity_enable_wakeup)
		{
			printk("[prox]%s():apds9900_set_enable(this_client, 0x%lX, 0x%lX)\n", __func__, val, val & APDS990_PROXIMITY);
			ret = apds9900_set_enable(this_client, val, val & APDS990_PROXIMITY);
		}
		else
		{
			printk("[prox]%s():apds9900_set_enable(this_client, 0x%lX, 0)\n", __func__, val);
			ret = apds9900_set_enable(this_client, val, 0);
		}
		if(ret < 0)
		{
			return ret;
		}
	}

#if dm2
	printk(KERN_ERR "[apds9900]apds9900_store_enable\n");
#endif

	return ret;
}

//REV: Revision Number
static ssize_t apds9900_show_rev(struct device *dev, struct device_attribute *attr, char *buf)
{
	//struct i2c_client *client = to_i2c_client(dev);
	struct apds9900_data *data = i2c_get_clientdata(this_client);
	int rev;

	mutex_lock(&data->update_lock);
	rev = i2c_smbus_read_byte_data(this_client, CMD_BYTE | APDS9900_REV_REG);
	mutex_unlock(&data->update_lock);

#if dm2
	printk(KERN_ERR "[apds9900]apds9900_show_rev\n");
#endif

	return sprintf(buf, "%d\n", rev);
}
//ID: device id
static ssize_t apds9900_show_devid(struct device *dev, struct device_attribute *attr, char *buf)
{
	//struct i2c_client *client = to_i2c_client(dev);
	struct apds9900_data *data = i2c_get_clientdata(this_client);
	int id;

	mutex_lock(&data->update_lock);
	id = i2c_smbus_read_byte_data(this_client, CMD_BYTE | APDS9900_ID_REG);
	mutex_unlock(&data->update_lock);

#if dm2
	printk(KERN_ERR "[apds9900]apds9900_show_devid\n");
#endif

	return sprintf(buf, "%d\n", id);
}
//Status
static ssize_t apds9900_show_status(struct device *dev, struct device_attribute *attr, char *buf)
{
	//struct i2c_client *client = to_i2c_client(dev);
	struct apds9900_data *data = i2c_get_clientdata(this_client);
	int status;

	mutex_lock(&data->update_lock);
	status = i2c_smbus_read_byte_data(this_client, CMD_BYTE | APDS9900_STATUS_REG);
	mutex_unlock(&data->update_lock);

#if dm2
	printk(KERN_ERR "[apds9900]apds9900_show_status\n");
#endif
	
	return sprintf(buf, "%d\n", status);
}
//Clear data
static ssize_t apds9900_show_cdata(struct device *dev, struct device_attribute *attr, char *buf)
{
	//struct i2c_client *client = to_i2c_client(dev);
	struct apds9900_data *data = i2c_get_clientdata(this_client);
	int cdata;

	mutex_lock(&data->update_lock);
	cdata = i2c_smbus_read_word_data(this_client, CMD_WORD | APDS9900_CDATAL_REG);
	mutex_unlock(&data->update_lock);

#if dm2
	printk(KERN_ERR "[apds9900]apds9900_show_cdata>>>cdata:%d\n", cdata);
#endif
	
	return sprintf(buf, "%d\n", cdata);
}
//IR data
static ssize_t apds9900_show_irdata(struct device *dev, struct device_attribute *attr, char *buf)
{
	//struct i2c_client *client = to_i2c_client(dev);
	struct apds9900_data *data = i2c_get_clientdata(this_client);
	int irdata;

	mutex_lock(&data->update_lock);
	irdata = i2c_smbus_read_word_data(this_client, CMD_WORD | APDS9900_IRDATAL_REG);
	mutex_unlock(&data->update_lock);

#if dm2
	printk(KERN_ERR "[apds9900]apds9900_show_irdata>>>irdata:%d\n", irdata);
#endif

	return sprintf(buf, "%d\n", irdata);
}
//Prox. data
static ssize_t apds9900_show_pdata(struct device *dev, struct device_attribute *attr, char *buf)
{
	//struct i2c_client *client = to_i2c_client(dev);
	struct apds9900_data *data = i2c_get_clientdata(this_client);
	int pdata;

	mutex_lock(&data->update_lock);
	pdata = i2c_smbus_read_word_data(this_client, CMD_WORD | APDS9900_PDATAL_REG);
	mutex_unlock(&data->update_lock);

#if dm2
	printk(KERN_ERR "[apds9900]apds9900_show_pdata>>>pdata:%d\n", pdata);
#endif

	return sprintf(buf, "%d\n", pdata);
}

//ppcount
static ssize_t apds9900_show_ppcount(struct device *dev, struct device_attribute *attr, char *buf)
{
	//struct i2c_client *client = to_i2c_client(dev);
	struct apds9900_data *data = i2c_get_clientdata(this_client);

#if dm2
	printk(KERN_ERR "[apds9900]apds9900_show_ppcount>>>ppcount:%d\n", data->ppcount);
#endif

	return sprintf(buf, "%d\n", data->ppcount);
}

static ssize_t apds9900_store_ppcount(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	//struct i2c_client *client = to_i2c_client(dev);
	unsigned long val = simple_strtoul(buf, NULL, 10);
	int ret;

	ret = apds9900_set_ppcount(this_client, val);
	if(ret < 0)
	{
		return ret;
	}

#if dm2
	printk(KERN_ERR "[apds9900]apds9900_store_ppcount\n");
#endif

	return count;
}

//Control register
static ssize_t apds9900_show_control(struct device *dev, 	struct device_attribute *attr, char *buf)
{
	//struct i2c_client *client = to_i2c_client(dev);
	//struct apds9900_data *data = i2c_get_clientdata(this_client);
	int control;
	control = apds9900_get_control(this_client);

#if dm2
	printk(KERN_ERR "[apds9900]apds9900_show_control>>>control:%d\n", control);
#endif

	return sprintf(buf, "%d\n", control);
}

static ssize_t apds9900_store_control(struct device *dev, 	struct device_attribute *attr, const char *buf, size_t count)
{
	//struct i2c_client *client = to_i2c_client(dev);
	unsigned long val = simple_strtoul(buf, NULL, 10);
	int ret;

	ret = apds9900_set_control(this_client, val);
	if(ret < 0)
	{
		return ret;
	}

#if dm2
	printk(KERN_ERR "[apds9900]apds9900_store_control\n");
#endif

	return count;
}

//-------------- ATTR list ---------------//
static DEVICE_ATTR(enable, S_IWUSR | S_IRUGO, apds9900_show_enable, apds9900_store_enable);
static DEVICE_ATTR(rev, S_IRUGO, apds9900_show_rev, NULL);
static DEVICE_ATTR(devid, S_IRUGO, apds9900_show_devid, NULL);
static DEVICE_ATTR(status, S_IRUGO, apds9900_show_status, NULL);
static DEVICE_ATTR(cdata, S_IRUGO, apds9900_show_cdata, NULL);
static DEVICE_ATTR(irdata, S_IRUGO, apds9900_show_irdata, NULL);
static DEVICE_ATTR(pdata, S_IRUGO, apds9900_show_pdata, NULL);
static DEVICE_ATTR(ppcount, S_IWUSR | S_IRUGO, apds9900_show_ppcount, apds9900_store_ppcount);
static DEVICE_ATTR(control, S_IWUSR | S_IRUGO, apds9900_show_control, apds9900_store_control);

//---------------------------------------//
static struct attribute *apds9900_attributes[] = {
	&dev_attr_enable.attr,
	&dev_attr_rev.attr,
	&dev_attr_devid.attr,
	&dev_attr_status.attr,
	&dev_attr_cdata.attr,
	&dev_attr_irdata.attr,
	&dev_attr_pdata.attr,
	&dev_attr_ppcount.attr,
	&dev_attr_control.attr,
	NULL
};
//---------------------------------------//
static const struct attribute_group apds9900_attr_group = {
	.attrs = apds9900_attributes,
};

//-------------- Work func ---------------//
static int round_off(int dividend, int divisor)
{
	int result;
	int quotient;
	int quotient_mul_10;
	int base = 10;

	quotient = dividend / divisor;
	quotient_mul_10 = (dividend * base) / divisor;

	if((quotient_mul_10 - quotient * base) >= base / 2)
	{
		result = quotient + 1;
	}
	else
	{
		result = quotient;
	}

	return result;
}
static int get_sensor_light_data(void)
{
	char buf[20];
	int clear_data = 0;
	int ir_data = 0;
	int iac_1 = 0;
	int iac_2 = 0;
	int iac_max = 0;
	int dev_factor = 52;
	int atime = 0;
	int ptime = 0;
	int wtime = 0;
	int alsit = 0;
	int als_gain = 1; //because control register equals 0x00
	int cpl = 0;
	int lux = 0;

	atime = apds9900_get_atime(this_client);
	ptime = apds9900_get_ptime(this_client);
	wtime = apds9900_get_wtime(this_client);
	//als_gain = apds9900_get_control(this_client);

	apds9900_show_cdata(&this_client->dev, NULL, buf);
	clear_data = simple_strtoul(buf, NULL, 10);

	apds9900_show_irdata(&this_client->dev, NULL, buf);
	ir_data = simple_strtoul(buf, NULL, 10);

	//------------ atime <---> alsit ------------//
	//atime = 256 - alsit /2.72(ms)
	//alsit = 2.72*(atime-256)
	alsit = round_off(272 * (atime - 25600), 10000);

	//lux calculating
	//max(iac_1, iac_2, 0)
	iac_1 = 10 * clear_data - 20 * ir_data;  //iac_1 = cdata - 2*irdata
	iac_2 = 8 * clear_data - 14 * ir_data;   //iac_2 = 0.8*cdata - 1.4*irdata

	if(iac_1 > 0 || iac_2 > 0)
	{
		if(iac_1 >= iac_2)
		{
			iac_max = round_off(iac_1, 10);
		}
		else if(iac_1 < iac_2)
		{
			iac_max = round_off(iac_2, 10);
		}
	}
	else
	{
		iac_max = 0;
	}
	//cpl = (alsit*als_gain)/(dev_factory*0.46)
	//glass attenuation factory = 0.46  >>> it should be changed by the real condition
	cpl = (round_off((alsit * als_gain), (dev_factor * 46))) * 100;
	//lux = iac_max/cpl
	lux = round_off(iac_max, cpl);

#if dm3
	printk(KERN_INFO "[apds9900]get_sensor_light_data(1)>>>(atime, ptime, wtime)=(%d, %d, %d)\n", atime, ptime, wtime);
	printk(KERN_INFO "[apds9900]get_sensor_light_data(2)>>>(clear_data, ir_data)=(%d, %d)\n", clear_data, ir_data);
	printk(KERN_INFO "[apds9900]get_sensor_light_data(3)>>>(iac_1 x10, iac_2 x10, iac_max)=(%d, %d, %d)\n", iac_1, iac_2, iac_max);
	printk(KERN_INFO "[apds9900]get_sensor_light_data(4)>>>(cpl, lux)=(%d, %d)\n", cpl, lux);
#endif

	lux = clear_data; /*0~1023*/
	return lux;
}

static void apds9900_work_func(struct work_struct *work)
{
	struct apds9900_data *data = container_of((struct delayed_work *)work, struct apds9900_data, work);
	int value;
	int delay = DEFAULT_DELAY_TIME;
	int adj = 1;//1000;
	value = adj * (get_sensor_light_data());
	//value = read_count;
	if(read_count == 50)
	{
		printk(KERN_INFO "[apds9900]>>>light sensor value:%d\n", value);
		read_count = 0;
	}
	else
	{
		read_count++;
	}

	input_report_abs(data->input, ABS_MISC, value);
	//input_report_abs(data->input, ABS_MISC, value);
	input_sync(data->input);
	atomic_set(&data->last, value);
	schedule_delayed_work(&data->work, msecs_to_jiffies(delay)+1);
}

#if prox
static void apds9900_work_func_p(struct work_struct *work)
{
	struct apds9900_data *data = container_of((struct delayed_work *)work, struct apds9900_data, work);
	int value;
	int delay = DEFAULT_DELAY_TIME;
	int adj = 1;//1000;
	char buf[20] = {0};
	apds9900_show_pdata(&this_client->dev,NULL,buf);
	value = adj*(simple_strtoul(buf, NULL, 10));
	if(read_count_p == 50)
	{
		printk(KERN_INFO "[apds9900]>>>prox sensor value:%d, GPIO:%d\n", value, gpio_get_value(GPIO_PROX));
		read_count_p = 0;
	}
	else
	{
		read_count_p++;
	}
	//input_report_abs(data->input, ABS_X, value);
	input_report_abs(data->input, ABS_DISTANCE, value);
	input_sync(data->input);
	atomic_set(&data->last, value);
	schedule_delayed_work(&data->work, msecs_to_jiffies(delay)+1);
}

static void prox_work_func(struct work_struct *work)
{
	struct apds9900_data *data = container_of((struct work_struct *)work, struct apds9900_data, prox_work);
//	int last_stat_p = atomic_read(&this_data_p->last_status);
	int adj = 1;//1000;
	char buf[20] = {0};
	int ret = 0;

	disable_irq(prox_irq);
	ret = i2c_smbus_write_byte_data(this_client, CMD_BYTE | APDS9900_ENABLE_REG, 0x0F);
	ret = i2c_smbus_write_byte(this_client, CMD_CLR_PS_INT);

	apds9900_show_pdata(&this_client->dev, NULL, buf);
	prox_value = adj * (simple_strtoul(buf, NULL, 10));
	printk(KERN_INFO "[prox]prox_work_func():ADC:%d, GPIO:%d\n", prox_value, gpio_get_value(GPIO_PROX));

//	ret = apds9900_set_pilt(client, 0);//disable
//	ret = apds9900_set_piht(client, 0);//disable
//	ret = apds9900_set_pilt(client, 0);//close trigger
//	ret = apds9900_set_piht(client, PROXIMITY_ADC_NEAR);//close trigger
//	ret = apds9900_set_pilt(client, PROXIMITY_ADC_FAR);//leave trigger
//	ret = apds9900_set_piht(client, 0);//leave trigger
	if(prox_value > PROXIMITY_ADC_NEAR)//high trigger points
	{
//set trigger point for far
		ret = apds9900_set_pilt(this_client, PROXIMITY_ADC_FAR);
		ret = apds9900_set_piht(this_client, 0);

//wake_unlock
		if(wake_lock_active(&wakelock_prox))
		{
			printk("[prox]NEAR(>%d):wake_unlock prox\n", PROXIMITY_ADC_NEAR);
			wake_unlock(&wakelock_prox);
		}
		else
		{
			printk("[prox]NEAR(>%d):not locked\n", PROXIMITY_ADC_NEAR);
		}
	}
	else
	{
		if(prox_value < PROXIMITY_ADC_FAR)//low trigger points
		{
//set trigger point for near
			ret = apds9900_set_pilt(this_client, 0);
			ret = apds9900_set_piht(this_client, PROXIMITY_ADC_NEAR);

			printk("[prox]FAR(<%d):wake_lock prox\n", PROXIMITY_ADC_FAR);
		}
		else//between high and low trigger points
		{
			printk("[prox]FAR(%d>=x>=%d):wake_lock prox\n", PROXIMITY_ADC_NEAR, PROXIMITY_ADC_FAR);
		}
//wake_lock
		wake_lock_timeout(&wakelock_prox, 3 * HZ);
	}

	ret = i2c_smbus_write_byte_data(this_client, CMD_BYTE | APDS9900_ENABLE_REG, 0x2F);

	input_report_abs(data->input, ABS_DISTANCE, prox_value);
	input_sync(data->input);
	atomic_set(&data->last, prox_value);

	enable_irq(prox_irq);
}

static irqreturn_t gpio_irq_handler(int irq, void *dev_id)
{
	if(irq < 0)
	{
		printk("[prox]invalid irq:%d\n", irq);
	}
	else
	{
//		printk("[prox]gpio_irq_handler:%d\n", irq);
		schedule_work(&this_data_p->prox_work);
	}

	return IRQ_HANDLED;
}
#endif
//-------------- Input dev init and finish ---------------//
static int apds9900_input_init(struct apds9900_data *data, char *sensor_name)
{
	struct input_dev *dev;
    int err;

    dev = input_allocate_device();
    if (!dev) {
        return -ENOMEM;
    }
    dev->name = sensor_name;
    dev->id.bustype = BUS_I2C;

    input_set_capability(dev, EV_ABS, ABS_MISC);
    input_set_capability(dev, EV_ABS, ABS_DISTANCE);

    input_set_drvdata(dev, data);

    err = input_register_device(dev);
    if (err < 0) {
        input_free_device(dev);
        printk(KERN_INFO "[apds9900]Input dev init fail(1)\n");
        return err;
    }
    data->input = dev;

    return 0;
}

static void apds9900_input_finish(struct apds9900_data *data)
{
	struct input_dev *dev = data->input;

	input_unregister_device(dev);
	input_free_device(dev);
}

//-------------- Initialization function---------------//
static int apds9900_init_client(struct i2c_client *client)
{
	struct apds9900_data *data = i2c_get_clientdata(client);
	int ret;
	//turn on
        // [Bug 784] Luke 2011,0901,
	if(proximity_enable_wakeup)
	{
		printk("[prox]%s():apds9900_set_enable(client, APDS990_ALL, APDS990_PROXIMITY)\n", __func__);
		ret = apds9900_set_enable(client, APDS990_ALL, APDS990_PROXIMITY); //turn all on
	}
	else
	{
		printk("[prox]%s():apds9900_set_enable(client, APDS990_ALL, 0)\n", __func__);
		ret = apds9900_set_enable(client, APDS990_ALL, 0); //turn all on
	}
	if(ret < 0)
	{
		return ret;
	}

	atomic_set(&data->enable, 63);
	mdelay(1);

	mutex_lock(&data->update_lock);
	ret = i2c_smbus_read_byte_data(client, APDS9900_ENABLE_REG);
	if(ret != 0)
	{
		return -ENODEV;
	}

	mutex_unlock(&data->update_lock);
	//turn off
	printk("[prox]%s():apds9900_set_enable(client, 0, 0)\n", __func__);
	ret = apds9900_set_enable(client, 0, 0);
	if(ret < 0)
	{
		return ret;
	}

	atomic_set(&data->enable, 0);	

	printk(KERN_ERR "[apds9900]apds9900_init_client");

	return 0;
}

//[Bug 798] Luke, 2011,0902,Turn off sensors power when device suspend -->
//-------------- EarlySuspend and LateResume ---------------//
#if APDS9900_EARLY_SUSPEND
static void apds9900_early_suspend(struct early_suspend *h)
{
#if 0
	int last_stat_als= atomic_read(&this_data->last_status);
	int last_stat_p = atomic_read(&this_data_p->last_status);
	printk(KERN_ERR "[apds9900]apds9900_early_suspend\n");

        // [Bug 784] Luke 2011,0901,
	if( last_stat_p  )    cancel_delayed_work_sync(&this_data_p->work);
	if( last_stat_als)    cancel_delayed_work_sync(&this_data->work);

	apds9900_set_enable(client, 0);
#endif
}

static char adps9900_has_suspend =0;

static void apds9900_late_resume(struct early_suspend *h)
{
	int delay = DEFAULT_DELAY_TIME;
	int last_stat_als;
	int last_stat_p;
	int ret;
	printk(KERN_ERR "[apds9900]apds9900_late_resume\n");

       if( adps9900_has_suspend == 0 )    return;

	last_stat_als = atomic_read(&this_data->last_status);
	last_stat_p = atomic_read(&this_data_p->last_status);

	//init register
	ret = apds9900_set_registers(this_client);
	if (ret < 0) {
		goto error_3;
	}
        // [Bug 784] Luke 2011,0901,
	//apds9900_set_enable(client, 1);

	if(proximity_enable_wakeup)
	{
		printk("[prox]%s():apds9900_set_enable(this_client, 0x%X, 0x%X)\n", __func__, (last_stat_p << 1) | last_stat_als, (last_stat_p << 1));
		apds9900_set_enable(this_client, (last_stat_p << 1) | last_stat_als, last_stat_p << 1);
	}
	else
	{
		printk("[prox]%s():apds9900_set_enable(this_client, 0x%X, 0)\n", __func__, (last_stat_p << 1) | last_stat_als);
		apds9900_set_enable(this_client, (last_stat_p << 1) | last_stat_als, 0);
	}

	if( last_stat_p  )    schedule_delayed_work(&this_data_p->work, msecs_to_jiffies(delay) + 1);
	if( last_stat_als )    schedule_delayed_work(&this_data->work, msecs_to_jiffies(delay) + 1);

	return;
error_3:
	printk(KERN_INFO "[apds9900]set register fail\n");
//	kfree(this_data);
}
#endif
//[Bug 798] Luke, 2011,0902,Turn off sensors power when device suspend <--

//-------------- Probe ---------------//
static int __devinit apds9900_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct apds9900_data *apds = NULL;
#if prox
	struct apds9900_data *apds_p = NULL;
#endif
	int ret;

#if prox
//PM8058_L10(VREG_L10A)
	pm8058_l10 = regulator_get(NULL, "8058_l10");
	if(IS_ERR(pm8058_l10))
	{
		printk("[prox]regulator_get fail\n");
	}
	else
	{
//		printk("[prox]regulator_get ok\n");
		ret = regulator_set_voltage(pm8058_l10, 2600000, 2600000);
		if(ret)
		{
			printk("[prox]regulator_set_voltage fail\n");
		}
		else
		{
//	    		printk("[prox]regulator_set_voltage ok\n");
			ret = regulator_enable(pm8058_l10);
			if(ret)
			{
		    		printk("[prox]regulator_enable fail\n");
			}
			else
			{
//		    		printk("[prox]regulator_enable ok\n");
			}
		}
	}

	//gpio config pull high
	ret = gpio_tlmm_config(GPIO_CFG(GPIO_PROX, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	if(ret)
	{
		printk("[prox]gpio_tlmm_config fail\n");
	}
	else
	{
//		printk("[prox]gpio_tlmm_config ok\n");
	}

	//gpio setting
	ret = gpio_request(GPIO_PROX, "prox");
	if(ret < 0)
	{
		printk("[prox]gpio_request fail\n");
	}
	else
	{
		printk("[prox]gpio_request ok\n");
	}

	ret = gpio_direction_input(GPIO_PROX);
	if(ret < 0)
	{
		printk("[prox]gpio_direction_input fail\n");
	}
	else
	{
//		printk("[prox]gpio_direction_input ok\n");
	}
#endif // #if prox

	//setup private data
    apds = kzalloc(sizeof(struct apds9900_data), GFP_KERNEL);
	if (!apds) {
		ret = -ENOMEM;
        	goto error_0;
	}
#if prox
	apds_p = kzalloc(sizeof(struct apds9900_data), GFP_KERNEL);
	if (!apds_p) {
		ret = -ENOMEM;
        	goto error_0;
	}
#endif
	//pass para.
	this_client = client;
	this_data = apds;	/*for setting schedule time at enable*/
#if prox
	this_data_p = apds_p;
#endif

	//init mutex and para.
	mutex_init(&apds->update_lock);
	mutex_init(&apds->data_mutex);
	atomic_set(&apds->enable, 0);
	atomic_set(&apds->delay, DEFAULT_DELAY_TIME);
	atomic_set(&apds->last, 0);
	atomic_set(&apds->last_status, 0);
#if prox
	mutex_init(&apds_p->update_lock);
	mutex_init(&apds_p->data_mutex);
	atomic_set(&apds_p->enable, 0);
	atomic_set(&apds_p->delay, DEFAULT_DELAY_TIME);
	atomic_set(&apds_p->last, 0);
	atomic_set(&apds_p->last_status, 0);
#endif

	//setup i2c client
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE)) {
        	ret = -ENODEV;
        	goto error_1;
    	}

	i2c_set_clientdata(client, apds);
	apds->client = client;

	// Initialize the APDS9900 chip
	ret = apds9900_init_client(client);
	if (ret < 0) {
		goto error_4;
	}

	//init register
	ret = apds9900_set_registers(client);
	if (ret < 0) {
		goto error_3;
	}

	//func work queue
	INIT_DELAYED_WORK(&apds->work, apds9900_work_func);
#if prox
	INIT_DELAYED_WORK(&apds_p->work, apds9900_work_func_p);
	INIT_WORK(&apds_p->prox_work, prox_work_func);
#endif

	//init input device
	ret = apds9900_input_init(apds, "light");

	if (ret < 0) {
		goto error_1;
	}
#if prox
	ret = apds9900_input_init(apds_p, "proximity");

	if (ret < 0) {
		goto error_1;
	}
#endif

	//sysfs hook
	ret = sysfs_create_group(&apds->input->dev.kobj, &apds9900_attr_group);

	if (ret < 0) {
		goto error_2;
	}
#if prox
	ret = sysfs_create_group(&apds_p->input->dev.kobj, &apds9900_attr_group);

	if (ret < 0) {
		goto error_2;
	}
#endif

//[Bug 798] Luke, 2011,0902,Turn off sensors power when device suspend -->
#if APDS9900_EARLY_SUSPEND
	apds->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;//EARLY_SUSPEND_LEVEL_STOP_DRAWING + 0;
	apds->early_suspend.suspend = apds9900_early_suspend;
	apds->early_suspend.resume = apds9900_late_resume;
	register_early_suspend(&apds->early_suspend);
#endif

#if prox
	wake_lock_init(&wakelock_prox, WAKE_LOCK_SUSPEND, "prox");

	prox_irq = gpio_to_irq(GPIO_PROX);
	if(prox_irq < 0)
	{
		printk("[prox]gpio_to_irq fail\n");
	}
	else
	{
//		printk("[prox]gpio_to_irq ok:%d\n", prox_irq);

//		cci_gpio_dump_setting(GPIO_PROX);
//		printk("[prox]GPIO:%d\n", gpio_get_value(GPIO_PROX));

		ret = request_irq(prox_irq, gpio_irq_handler, IRQF_TRIGGER_FALLING | IRQF_NO_SUSPEND, "prox", apds_p);
		if(ret < 0)
		{
			printk("[prox]request_irq fail\n");
		}
		else
		{
			printk("[prox]request_irq ok\n");
		}
	}

//	cci_gpio_dump_setting(GPIO_PROX);
#endif // #if prox

	printk(KERN_INFO "[apds9900]>>>apds9900_probe\n");
	return 0;

error_0:
	printk(KERN_INFO "[apds9900]setup private data fail\n");
	return ret;
error_1:
	printk(KERN_INFO "[apds9900]i2c check fail\n");
	kfree(apds);
	return ret;
error_2:
	printk(KERN_INFO "[apds9900]sysfs hook fail\n");
	apds9900_input_finish(apds);
	return ret;
error_3:
	printk(KERN_INFO "[apds9900]set register fail\n");
	kfree(apds);
	return ret;
error_4:
	printk(KERN_INFO "[apds9900]Initialize the APDS9900 chip fail\n");
	kfree(apds);
	return ret;
}

//-------------- Remove ---------------//
static int apds9900_remove(struct i2c_client *client)
{
	struct apds9900_data *data = i2c_get_clientdata(client);
	int ret;

	printk("[prox]%s():apds9900_set_enable(0, 0)\n", __func__);
	ret = apds9900_set_enable(client, 0, 0);

	free_irq(prox_irq, NULL);
	gpio_free(GPIO_PROX);
	wake_lock_destroy(&wakelock_prox);
	ret = regulator_disable(pm8058_l10);
	if(ret)
	{
    		printk("[prox]regulator_disable fail\n");
	}
	else
	{
    		printk("[prox]regulator_disable ok\n");
	}

	sysfs_remove_group(&data->input->dev.kobj, &apds9900_attr_group);
	apds9900_input_finish(data);
        //[Bug 798] Luke, 2011,0902,Turn off sensors power when device suspend -->
        #if APDS9900_EARLY_SUSPEND
	unregister_early_suspend(&data->early_suspend);
#endif
	kfree(data);

	return ret;
}

//[Bug 798] Luke, 2011,0902,Turn off sensors power when device suspend -->
extern char Mpu_Temp_Power_Control;
//-------------- Suspend ---------------//
static int apds9900_suspend(struct i2c_client *client, pm_message_t mesg)
{
//[Bug 798] Luke, 2011,0902,Turn off sensors power when device suspend -->
#if 1// !APDS9900_EARLY_SUSPEND
	int last_stat_als = atomic_read(&this_data->last_status);
	int last_stat_p = atomic_read(&this_data_p->last_status);
	int ret = 0;

	printk(KERN_ERR "[apds9900]apds9900_suspend:ALS=%d, Prox=%d\n", last_stat_als, last_stat_p);

	adps9900_has_suspend = 1;

        // [Bug 784] Luke 2011,0901,
	if( last_stat_p  )    cancel_delayed_work_sync(&this_data_p->work);
	if( last_stat_als )    cancel_delayed_work_sync(&this_data->work);

//       if( Mpu_Temp_Power_Control )
//	apds9900_set_enable(client, 0);

	if(last_stat_p)
	{
		printk("[prox]%s():apds9900_set_enable(client, APDS990_PROXIMITY, APDS990_PROXIMITY)\n", __func__);
		apds9900_set_enable(client, APDS990_PROXIMITY, APDS990_PROXIMITY);
		ret = enable_irq_wake(prox_irq);
		if(ret)
		{
			printk("[prox]enable_irq_wake %d failed\n", prox_irq);
		}
		else
		{
			printk("[prox]enable_irq_wake %d ok\n", prox_irq);
		}
//		cci_gpio_dump_setting(GPIO_PROX);
	}
#endif
	return ret;
}



//-------------- Resume ---------------//
static int apds9900_resume(struct i2c_client *client)
{
//[Bug 798] Luke, 2011,0902,Turn off sensors power when device suspend -->
	int last_stat_als = atomic_read(&this_data->last_status);
	int last_stat_p = atomic_read(&this_data_p->last_status);
	int ret = 0;

	printk(KERN_ERR "[apds9900]apds9900_resume:ALS=%d, Prox=%d\n", last_stat_als, last_stat_p);
#if 0//!APDS9900_EARLY_SUSPEND
	int delay = DEFAULT_DELAY_TIME;
	int last_stat_als= atomic_read(&this_data->last_status);
	int last_stat_p = atomic_read(&this_data_p->last_status);
	int ret;
	printk(KERN_ERR "[apds9900]apds9900_resume\n");


	//init register
	ret = apds9900_set_registers(this_client);
	if (ret < 0) {
		goto error_3;
	}
        // [Bug 784] Luke 2011,0901,
	//apds9900_set_enable(client, 1);

	apds9900_set_enable( client, (last_stat_als<<1) | last_stat_p );

	if( last_stat_p  )    schedule_delayed_work(&this_data_p->work, msecs_to_jiffies(delay) + 1);
	if( last_stat_als)    schedule_delayed_work(&this_data->work, msecs_to_jiffies(delay) + 1);

	return 0;
error_3:
	printk(KERN_INFO "[apds9900]set register fail\n");
	kfree(this_data);
#endif

	if(last_stat_p)
	{
		schedule_work(&this_data_p->prox_work);
//		cci_gpio_dump_setting(GPIO_PROX);
		ret = disable_irq_wake(prox_irq);
		if(ret)
		{
			printk("[prox]disable_irq_wake %d failed\n", prox_irq);
		}
		else
		{
			printk("[prox]disable_irq_wake %d ok\n", prox_irq);
		}
	}

	return ret;
}

//-----------------------------------------------------//
static const struct i2c_device_id apds9900_id[] = {
    {APDS9900_DRV_NAME, 0},
    {},
};

MODULE_DEVICE_TABLE(i2c, apds9900_id);

static struct i2c_driver apds9900_driver = {
    .driver = {
        .name = "light",
        .owner = THIS_MODULE,
    },
    .probe = apds9900_probe,
    .remove = __devexit_p(apds9900_remove),
    .suspend = apds9900_suspend,
    .resume = apds9900_resume,
    .id_table = apds9900_id,
};

//[Bug 861] Luke 2011.0908 T1 boot fail due to sensors
extern int cci_ftm_boot; 

static int __init apds9900_init(void)
{
    //[Bug 861] Luke 2011.0908 T1 boot fail due to sensors
    if( cci_ftm_boot )    
    {
        printk(KERN_INFO "[apds9900]FTM returnl\n");
        return -1;
    }
	
    return i2c_add_driver(&apds9900_driver);
}

static void __exit apds9900_exit(void)
{
    i2c_del_driver(&apds9900_driver);
}

module_init(apds9900_init);
module_exit(apds9900_exit);

MODULE_DESCRIPTION("APDS9900 light and prox driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRIVER_VERSION);
