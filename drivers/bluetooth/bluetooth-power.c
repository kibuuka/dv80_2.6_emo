/* Copyright (c) 2009-2010, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */
/*
 * Bluetooth Power Switch Module
 * controls power to external Bluetooth device
 * with interface to power management device
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/rfkill.h>
#include <linux/gpio.h>
#include <linux/delay.h>


#define BT_REG_ON  131
#define BT_RST_N    73
#define WLAN_RESET  72
#define GPIO_OFF    0x32 // ignore uart

static bool previous;
static int bluetooth_power_state;
static int (*power_control)(int enable);

static DEFINE_SPINLOCK(bt_power_lock);

static bool previous;
extern void hsuart_power(int on);

static int bluetooth_toggle_radio(void *data, bool blocked)
{
	int ret = 0;
	int (*power_control)(int enable);

	spin_lock(&bt_power_lock);
	power_control = data;
	if (previous != blocked)
		ret = (*power_control)(!blocked);
	previous = blocked;
	spin_unlock(&bt_power_lock);
	return ret;
}

static const struct rfkill_ops bluetooth_power_rfkill_ops = {
	.set_block = bluetooth_toggle_radio,
};

static int bluetooth_power_rfkill_probe(struct platform_device *pdev)
{
	struct rfkill *rfkill;
	int ret;

	rfkill = rfkill_alloc("bt_power", &pdev->dev, RFKILL_TYPE_BLUETOOTH,
			      &bluetooth_power_rfkill_ops,
			      pdev->dev.platform_data);

	if (!rfkill) {
		printk(KERN_DEBUG
			"%s: rfkill allocate failed\n", __func__);
		return -ENOMEM;
	}

	/* force Bluetooth off during init to allow for user control */
	rfkill_init_sw_state(rfkill, 1);
	previous = 1;

	ret = rfkill_register(rfkill);
	if (ret) {
		printk(KERN_DEBUG
			"%s: rfkill register failed=%d\n", __func__,
			ret);
		rfkill_destroy(rfkill);
		return ret;
	}

	platform_set_drvdata(pdev, rfkill);

	return 0;
}

static void bluetooth_power_rfkill_remove(struct platform_device *pdev)
{
	struct rfkill *rfkill;

	dev_dbg(&pdev->dev, "%s\n", __func__);

	rfkill = platform_get_drvdata(pdev);
	if (rfkill)
		rfkill_unregister(rfkill);
	rfkill_destroy(rfkill);
	platform_set_drvdata(pdev, NULL);
}
static int bluetooth_power_param_set(const char *val, struct kernel_param *kp)
{
	int ret = 0;

	printk(KERN_EMERG
		"%s: previous power_state=%d val[0]  = %d\n",
		__func__, bluetooth_power_state, val[0]);
	/* lock change of state and reference */
	spin_lock(&bt_power_lock);
printk(KERN_EMERG
		"%s: val[0]  = %d\n",
		__func__, val[0]);
	if ( val[0] != GPIO_OFF ) {
	
		ret = param_set_bool(val, kp);
		
		if (power_control) {
			if (!ret){
				ret = (*power_control)(bluetooth_power_state);
	                        printk(KERN_ERR "%s: bluetooth power control, return = (%d)\n",
						__func__, ret);
	                }
			else{
				printk(KERN_ERR "%s param set bool failed (%d)\n",
						__func__, ret);
	                }
		} else {
			printk(KERN_INFO
				"%s: deferring power switch until probe\n",
				__func__);
		}
	}	else {
		bluetooth_power_state = val[0];
	}
	
	spin_unlock(&bt_power_lock);
	printk(KERN_EMERG
		"%s: current power_state=%d\n",
		__func__, bluetooth_power_state);

	
        if(bluetooth_power_state == 0){
		hsuart_power(0);

		gpio_set_value(BT_RST_N,0);
		msleep(105);
		gpio_set_value(BT_REG_ON,0);
	}
	else if (GPIO_OFF == bluetooth_power_state) {
		/* only for bluetooth test mode */
		gpio_set_value(BT_RST_N,0);
		msleep(105);
		gpio_set_value(BT_REG_ON,0);
		bluetooth_power_state = 0;
	}	
	else{
		gpio_set_value(BT_RST_N,0);
		msleep(1);
		gpio_set_value(BT_REG_ON,1);
		msleep(105);
		gpio_set_value(BT_RST_N,1);
	}
        printk(" ***** BT_REG_ON----------- --------- GPIO 131 = %d\n",gpio_get_value(131));
	printk(" ***** BT_RST_N ----------- ---------- GPIO 73 = %d\n",gpio_get_value(73));



	return ret;
}

module_param_call(power, bluetooth_power_param_set, param_get_bool,
		  &bluetooth_power_state, S_IWUSR | S_IRUGO);

static int __devinit bt_power_probe(struct platform_device *pdev)
{
	int ret = 0;

	printk(KERN_DEBUG "%s\n", __func__);

	if (!pdev->dev.platform_data) {
		printk(KERN_ERR "%s: platform data not initialized\n",
				__func__);
		return -ENOSYS;
	}

	spin_lock(&bt_power_lock);
	power_control = pdev->dev.platform_data;

	if (bluetooth_power_state) {
		printk(KERN_INFO
			"%s: handling deferred power switch\n",
			__func__);
	}
	ret = (*power_control)(bluetooth_power_state);
	spin_unlock(&bt_power_lock);

	if (!ret && !bluetooth_power_state &&
		    bluetooth_power_rfkill_probe(pdev))
		ret = -ENOMEM;

	return ret;
}

static int __devexit bt_power_remove(struct platform_device *pdev)
{
	int ret;

	printk(KERN_DEBUG "%s\n", __func__);

	bluetooth_power_rfkill_remove(pdev);

	if (!power_control) {
		printk(KERN_ERR "%s: power_control function not initialized\n",
				__func__);
		return -ENOSYS;
	}
	spin_lock(&bt_power_lock);
	bluetooth_power_state = 0;
	ret = (*power_control)(bluetooth_power_state);
	power_control = NULL;
	spin_unlock(&bt_power_lock);

	return ret;
}

static struct platform_driver bt_power_driver = {
	.probe = bt_power_probe,
	.remove = __devexit_p(bt_power_remove),
	.driver = {
		.name = "bt_power",
		.owner = THIS_MODULE,
	},
};

static int __init bluetooth_power_init(void)
{
	int ret;

	printk(KERN_DEBUG "%s\n", __func__);
	ret = platform_driver_register(&bt_power_driver);
	return ret;
}

static void __exit bluetooth_power_exit(void)
{
	printk(KERN_DEBUG "%s\n", __func__);
	platform_driver_unregister(&bt_power_driver);
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MSM Bluetooth power control driver");
MODULE_VERSION("1.40");

module_init(bluetooth_power_init);
module_exit(bluetooth_power_exit);

