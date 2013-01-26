//--------------- Header files ------------------//
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/proc_fs.h>
#include <asm/atomic.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/switch.h>

//-------------- Define ---------------//
#define DEBUG 0
#define IF_I2C_DEVICE 0
#define LOG_TAG "EM6781"
#define SENSOR_NAME "dock"
#define HALL_SENSOR_GPIO 128
#define DOCK_ID 0
#define DOCK_ON 2
#define DOCK_OFF 0

//-------------- Define Sensor Data Struct ---------------//
struct sensor_data {
    struct switch_dev sdev;
    struct work_struct work;
    struct device *dev;
    struct platform_device *pdev;	
    atomic_t state;
    unsigned gpio;
    int irq;
};

//-------------- Global para. --------------//
struct sensor_data *this_data = NULL;
static struct platform_device *sensor_pdev = NULL;

//-------------- Global functions ---------------//
static int sensor_read_gpio_data(unsigned gpio_number, const char* label)
{
	int gpio_data = 0;
#if DEBUG
	printk("[hall_sensor][em6781]>>>sensor_read_gpio_data\n");
#endif
	gpio_data = gpio_get_value(gpio_number);

	return gpio_data;	
}

//-------------- irq func ---------------//
static irqreturn_t gpio_irq_handler(int irq, void *dev_id)
{
#if DEBUG
	printk("[hall_sensor][em6781]>>>gpio_irq_handler\n");
#endif
	schedule_work(&this_data->work);
	return IRQ_HANDLED;
}

//-------------- Work func ---------------//
static void switch_work_func(struct work_struct *work)
{
	int state;
	state = sensor_read_gpio_data(HALL_SENSOR_GPIO, SENSOR_NAME);
#if DEBUG
	printk("[hall_sensor][em6781]>>>switch_work_func\n");
#endif
	if(state == 1)
		switch_set_state(&this_data->sdev, DOCK_OFF);
	else if(state == 0)
		switch_set_state(&this_data->sdev, DOCK_ON);
}

//-------------- Probe ---------------//
static int sensor_probe(struct platform_device *pdev)
{
	int rt;
	struct sensor_data *data = NULL;

	//gpio config pull high
	rt = gpio_tlmm_config(GPIO_CFG(HALL_SENSOR_GPIO, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	if(rt)
		printk("gpio_direction_input fail\n");

	//gpio setting
	rt = gpio_request(HALL_SENSOR_GPIO, SENSOR_NAME);
	if(rt < 0)
		printk("[hall_sensor][em6781]>>>gpio_request fail\n");
	rt = gpio_direction_input(HALL_SENSOR_GPIO);
	if(rt < 0)
		printk("[hall_sensor][em6781]>>>gpio_direction_input fail\n");
	
	//switch device registing	
	data = kzalloc(sizeof(struct sensor_data), GFP_KERNEL);
	if (!data) {
        	rt = -ENOMEM;
        	printk("[em6781]sensor_probe>>>kzalloc fail\n");
		return rt;
    	}
	this_data = data;
	data->sdev.name = SENSOR_NAME;
	data->gpio = HALL_SENSOR_GPIO;	

	rt = switch_dev_register(&data->sdev);
	if (rt != 0)
		printk("[em6781]sensor_probe>>>switch_dev_register fail\n");

	INIT_WORK(&data->work, switch_work_func);

	data->irq = gpio_to_irq(data->gpio);
	if (data->irq < 0)
		printk("[em6781]sensor_probe>>>gpio_to_irq fail\n");

	rt = request_irq(data->irq, gpio_irq_handler, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, data->sdev.name, data);
	if (rt < 0)
		printk("[em6781]sensor_probe>>>request_irq fail\n");

	switch_work_func(&data->work);

	return 0;
}

//-------------- Remove ---------------//
static int sensor_remove(struct platform_device *pdev)
{
	cancel_work_sync(&this_data->work);
	gpio_free(HALL_SENSOR_GPIO);
	switch_dev_unregister(&this_data->sdev);
	kfree(this_data);
	return 0;
}

//-------------- Suspend ---------------//
static int sensor_suspend(struct platform_device *pdev, pm_message_t state)
{
#if DEBUG
	printk("[em6781][suspend]!!!!!!\n");
#endif
	cancel_work_sync(&this_data->work);
	/*
	gpio_free(HALL_SENSOR_GPIO);
	*/
	return 0;
}

//-------------- Resume ---------------//
static int sensor_resume(struct platform_device *pdev)
{
	//int rt;
#if DEBUG	
	printk("[em6781][resume]!!!!!!\n");
#endif
    	//gpio_request
    	/*
	rt = gpio_request(HALL_SENSOR_GPIO, SENSOR_NAME);
	if(rt < 0)
		printk("[em6781][resume]>>>gpio_request fail\n");
	*/
	schedule_work(&this_data->work);
	return 0;
}

static struct platform_driver sensor_driver = {
    .probe      = sensor_probe,
    .remove     = sensor_remove,
    .suspend    = sensor_suspend,
    .resume     = sensor_resume,
    .driver = {
        .name   = SENSOR_NAME,
        .owner  = THIS_MODULE,
    },
};

//[Bug 861] Luke 2011.0908 T1 boot fail due to sensors
extern int cci_ftm_boot; 
static int __init sensor_init(void)
{
    int rc;

    //[Bug 861] Luke 2011.0908 T1 boot fail due to sensors
    if( cci_ftm_boot )    
    {
        printk(KERN_INFO "[em6781]FTM returnl\n");
	 return -1;
    }
   	
    sensor_pdev = platform_device_register_simple(SENSOR_NAME, 0, NULL, 0);
    if (IS_ERR(sensor_pdev)) {
	printk(KERN_ERR "[%s]%s - device register FAIL!\n", LOG_TAG, __FUNCTION__);
        return -1;
    }
    rc = platform_driver_register(&sensor_driver);
    if (rc < 0) {
	printk(KERN_ERR "[%s]%s - driver register FAIL(%d)!\n", LOG_TAG, __FUNCTION__, rc);         
    }
    return rc;	  
}

static void __exit sensor_exit(void)
{
    platform_driver_unregister(&sensor_driver);
    platform_device_unregister(sensor_pdev);
}

module_init(sensor_init);
module_exit(sensor_exit);

MODULE_DESCRIPTION("EM6781 hall driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRIVER_VERSION);
