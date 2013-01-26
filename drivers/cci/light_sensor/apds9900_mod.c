/*
 *  apds9900.c - Linux kernel modules for ambient light + proximity sensor
 *
 *  Copyright (C) 2010 Lee Kai Koon <kai-koon.lee@avagotech.com>
 *  Copyright (C) 2010 Avago Technologies
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <asm/atomic.h>

#define APDS9900_DRV_NAME	"apds9900"
#define DRIVER_VERSION		"1.0.1"
#define DEFAULT_DELAY_TIME 	200

/* Change History 
 *
 * 1.0.1	Functions apds9900_show_rev(), apds9900_show_id() and apds9900_show_status()
 *			have missing CMD_BYTE in the i2c_smbus_read_byte_data(). APDS-9900 needs
 *			CMD_BYTE for i2c write/read byte transaction.
 */

/*
 * Defines
 */
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

#define CMD_BYTE	0x80
#define CMD_WORD	0xA0
#define CMD_SPECIAL	0xE0

#define CMD_CLR_PS_INT	0xE5
#define CMD_CLR_ALS_INT	0xE6
#define CMD_CLR_PS_ALS_INT	0xE7

/*
 * Structs
 */

struct apds9900_data {
	struct i2c_client *client;
	struct mutex update_lock;
	struct mutex data_mutex;
	struct delayed_work work;
	struct input_dev *input;

	unsigned int enable;
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
};

/*
 * Global data
 */

/*
 * Management functions
 */
//-------------- Global para. --------------//
struct i2c_client *this_client;
static struct apds9900_data *pdata;//test code
static struct i2c_driver apds9900_driver;

//-------------- Global functions ---------------//
static int apds9900_set_command(struct i2c_client *client, int command)
{
	struct apds9900_data *data = i2c_get_clientdata(client);
	int ret;
	int clearInt;

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_set_command S\n");
	
	if (command == 0)
		clearInt = CMD_CLR_PS_INT;
	else if (command == 1)
		clearInt = CMD_CLR_ALS_INT;
	else
		clearInt = CMD_CLR_PS_ALS_INT;
		
	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_byte(client, clearInt);
	mutex_unlock(&data->update_lock);

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_set_command ret=%d E\n",ret);
	
	return ret;
}

static int apds9900_set_enable(struct i2c_client *client, int enable)
{
	struct apds9900_data *data = i2c_get_clientdata(client);
	int ret;


	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_set_enable S\n");
	
	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_byte_data(client, CMD_BYTE|APDS9900_ENABLE_REG, enable);
	mutex_unlock(&data->update_lock);

	data->enable = enable;

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_set_enable enable=%d E, ret=%d\n",data->enable,ret);
	
	return ret;
}

static int apds9900_set_atime(struct i2c_client *client, int atime)
{
	struct apds9900_data *data = i2c_get_clientdata(client);
	int ret;

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_set_atime S\n");
	
	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_byte_data(client, CMD_BYTE|APDS9900_ATIME_REG, atime);
	mutex_unlock(&data->update_lock);

	data->atime = atime;

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_set_command atime=%d, ret=%d  E\n",data->atime,ret);
	
	return ret;
}

static int apds9900_set_ptime(struct i2c_client *client, int ptime)
{
	struct apds9900_data *data = i2c_get_clientdata(client);
	int ret;

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_set_ptime S\n");
	
	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_byte_data(client, CMD_BYTE|APDS9900_PTIME_REG, ptime);
	mutex_unlock(&data->update_lock);

	data->ptime = ptime;

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_set_command ptime=%d, ret=%d  E\n",data->ptime,ret);
	
	return ret;
}

static int apds9900_set_wtime(struct i2c_client *client, int wtime)
{
	struct apds9900_data *data = i2c_get_clientdata(client);
	int ret;

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_set_wtime S\n");
	
	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_byte_data(client, CMD_BYTE|APDS9900_WTIME_REG, wtime);
	mutex_unlock(&data->update_lock);

	data->wtime = wtime;

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_set_command wtime=%d, ret=%d  E\n",data->wtime,ret);
	
	return ret;
}

static int apds9900_set_ailt(struct i2c_client *client, int threshold)
{
	struct apds9900_data *data = i2c_get_clientdata(client);
	int ret;

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_set_ailt S\n");
	
	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_word_data(client, CMD_WORD|APDS9900_AILTL_REG, threshold);
	mutex_unlock(&data->update_lock);
	
	data->ailt = threshold;

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_set_ailt ailt=%d, ret=%d  E\n",data->ailt,ret);
	
	return ret;
}

static int apds9900_set_aiht(struct i2c_client *client, int threshold)
{
	struct apds9900_data *data = i2c_get_clientdata(client);
	int ret;

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_set_aiht S\n");
	
	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_word_data(client, CMD_WORD|APDS9900_AIHTL_REG, threshold);
	mutex_unlock(&data->update_lock);
	
	data->aiht = threshold;
	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_set_aiht aiht=%d, ret=%d  E\n",data->aiht,ret);
	
	return ret;
}

static int apds9900_set_pilt(struct i2c_client *client, int threshold)
{
	struct apds9900_data *data = i2c_get_clientdata(client);
	int ret;

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_set_pilt S\n");
	
	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_word_data(client, CMD_WORD|APDS9900_PILTL_REG, threshold);
	mutex_unlock(&data->update_lock);
	
	data->ailt = threshold;
	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_set_pilt ailt=%d, ret=%d  E\n",data->ailt,ret);
	return ret;
}

static int apds9900_set_piht(struct i2c_client *client, int threshold)
{
	struct apds9900_data *data = i2c_get_clientdata(client);
	int ret;

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_set_piht S\n");
	
	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_word_data(client, CMD_WORD|APDS9900_PIHTL_REG, threshold);
	mutex_unlock(&data->update_lock);
	
	data->aiht = threshold;
	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_set_piht ailt=%d, ret=%d  E\n",data->aiht,ret);
	return ret;
}

static int apds9900_set_pers(struct i2c_client *client, int pers)
{
	struct apds9900_data *data = i2c_get_clientdata(client);
	int ret;

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_set_pers S\n");
	
	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_byte_data(client, CMD_BYTE|APDS9900_PERS_REG, pers);
	mutex_unlock(&data->update_lock);

	data->pers = pers;
	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_set_pers pers=%d, ret=%d  E\n",data->pers,ret);
	return ret;
}

static int apds9900_set_config(struct i2c_client *client, int config)
{
	struct apds9900_data *data = i2c_get_clientdata(client);
	int ret;

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_set_config S\n");
	
	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_byte_data(client, CMD_BYTE|APDS9900_CONFIG_REG, config);
	mutex_unlock(&data->update_lock);

	data->config = config;
	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_set_config config=%d, ret=%d  E\n",data->config,ret);
	return ret;
}

static int apds9900_set_ppcount(struct i2c_client *client, int ppcount)
{
	struct apds9900_data *data = i2c_get_clientdata(client);
	int ret;

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_set_ppcount S\n");
	
	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_byte_data(client, CMD_BYTE|APDS9900_PPCOUNT_REG, ppcount);
	mutex_unlock(&data->update_lock);

	data->ppcount = ppcount;
	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_set_ppcount ppcount=%d, ret=%d  E\n",data->ppcount,ret);
	return ret;
}

static int apds9900_set_control(struct i2c_client *client, int control)
{
	struct apds9900_data *data = i2c_get_clientdata(client);
	int ret;

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_set_control S\n");
	
	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_byte_data(client, CMD_BYTE|APDS9900_CONTROL_REG, control);
	mutex_unlock(&data->update_lock);

	data->control = control;
	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_set_control  control=%d, ret=%d  E\n",data->control,ret);
	return ret;
}

/*
 * SysFS support
 */
//-------------- ATTR functions ---------------//
static ssize_t apds9900_store_command(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	unsigned long val = simple_strtoul(buf, NULL, 10);
	int ret;

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_store_command S\n");
	
	if (val < 0 || val > 2)
		return -EINVAL;

	ret = apds9900_set_command(client, val);

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_store_command  ret=%d  E\n",ret);
	
	if (ret < 0)
		return ret;

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_store_command  count=%d  E\n",count);
	
	return count;
}

static DEVICE_ATTR(command, S_IWUSR,
		   NULL, apds9900_store_command);

static ssize_t apds9900_show_enable(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct apds9900_data *data = i2c_get_clientdata(client);

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_show_enable enable=%d  E\n",data->enable);
	
	return sprintf(buf, "%d\n", data->enable);
}

static ssize_t apds9900_store_enable(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	unsigned long val = simple_strtoul(buf, NULL, 10);
	int ret;

	ret = apds9900_set_enable(client, val);

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_store_enable  ret=%d  E\n",ret);
	
	if (ret < 0)
		return ret;

	return count;
}

static DEVICE_ATTR(enable, S_IWUSR | S_IRUGO,
		   apds9900_show_enable, apds9900_store_enable);

static ssize_t apds9900_show_atime(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct apds9900_data *data = i2c_get_clientdata(client);

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_show_atime  atime=%d  E\n",data->atime);
	
	return sprintf(buf, "%d\n", data->atime);
}

static ssize_t apds9900_store_atime(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	unsigned long val = simple_strtoul(buf, NULL, 10);
	int ret;

	ret = apds9900_set_atime(client, val);

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_show_atime  ret=%d  E\n",ret);
	
	if (ret < 0)
		return ret;

	return count;
}

static DEVICE_ATTR(atime, S_IWUSR | S_IRUGO,
		   apds9900_show_atime, apds9900_store_atime);

static ssize_t apds9900_show_ptime(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct apds9900_data *data = i2c_get_clientdata(client);

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_show_atime  ptime=%d  E\n",data->ptime);
	
	return sprintf(buf, "%d\n", data->ptime);
}

static ssize_t apds9900_store_ptime(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	unsigned long val = simple_strtoul(buf, NULL, 10);
	int ret;

	ret = apds9900_set_ptime(client, val);

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_store_ptime  ret=%d  E\n",ret);
	
	if (ret < 0)
		return ret;

	return count;
}

static DEVICE_ATTR(ptime, S_IWUSR | S_IRUGO,
		   apds9900_show_ptime, apds9900_store_ptime);

static ssize_t apds9900_show_wtime(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct apds9900_data *data = i2c_get_clientdata(client);

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_show_wtime  wtime=%d  E\n",data->wtime);
	
	return sprintf(buf, "%d\n", data->wtime);
}

static ssize_t apds9900_store_wtime(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	unsigned long val = simple_strtoul(buf, NULL, 10);
	int ret;

	ret = apds9900_set_wtime(client, val);

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_store_wtime  ret=%d  E\n",ret);
	
	if (ret < 0)
		return ret;

	return count;
}

static DEVICE_ATTR(wtime, S_IWUSR | S_IRUGO,
		   apds9900_show_wtime, apds9900_store_wtime);

static ssize_t apds9900_show_ailt(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct apds9900_data *data = i2c_get_clientdata(client);

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_show_ailt  data->ailt=%d  E\n",data->ailt);
	
	return sprintf(buf, "%d\n", data->ailt);
}

static ssize_t apds9900_store_ailt(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	unsigned long val = simple_strtoul(buf, NULL, 10);
	int ret;

	ret = apds9900_set_ailt(client, val);

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_store_ailt  ret=%d  E\n",ret);
	
	if (ret < 0)
		return ret;

	return count;
}

static DEVICE_ATTR(ailt, S_IWUSR | S_IRUGO,
		   apds9900_show_ailt, apds9900_store_ailt);

static ssize_t apds9900_show_aiht(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct apds9900_data *data = i2c_get_clientdata(client);

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_show_aiht  data->aiht=%d  E\n",data->aiht);
	
	return sprintf(buf, "%d\n", data->aiht);
}

static ssize_t apds9900_store_aiht(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	unsigned long val = simple_strtoul(buf, NULL, 10);
	int ret;

	ret = apds9900_set_aiht(client, val);

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_set_aiht  ret=%d  E\n",ret);
	
	if (ret < 0)
		return ret;

	return count;
}

static DEVICE_ATTR(aiht, S_IWUSR | S_IRUGO,
		   apds9900_show_aiht, apds9900_store_aiht);

static ssize_t apds9900_show_pilt(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct apds9900_data *data = i2c_get_clientdata(client);

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_show_pilt  data->pilt=%d  E\n",data->pilt);
	
	return sprintf(buf, "%d\n", data->pilt);
}

static ssize_t apds9900_store_pilt(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	unsigned long val = simple_strtoul(buf, NULL, 10);
	int ret;

	ret = apds9900_set_pilt(client, val);

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_show_pilt  ret=%d  E\n",ret);
	
	if (ret < 0)
		return ret;

	return count;
}

static DEVICE_ATTR(pilt, S_IWUSR | S_IRUGO,
		   apds9900_show_pilt, apds9900_store_pilt);

static ssize_t apds9900_show_piht(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct apds9900_data *data = i2c_get_clientdata(client);

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_show_piht  data->piht=%d  E\n",data->piht);
	
	return sprintf(buf, "%d\n", data->piht);
}

static ssize_t apds9900_store_piht(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	unsigned long val = simple_strtoul(buf, NULL, 10);
	int ret;

	ret = apds9900_set_piht(client, val);

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_store_piht  ret=%d  E\n",ret);
	
	if (ret < 0)
		return ret;

	return count;
}

static DEVICE_ATTR(piht, S_IWUSR | S_IRUGO,
		   apds9900_show_piht, apds9900_store_piht);

static ssize_t apds9900_show_pers(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct apds9900_data *data = i2c_get_clientdata(client);

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_show_pers  data->piht=%d  E\n",data->pers);
	
	return sprintf(buf, "%d\n", data->pers);
}

static ssize_t apds9900_store_pers(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	unsigned long val = simple_strtoul(buf, NULL, 10);
	int ret;

	ret = apds9900_set_pers(client, val);

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_store_pers  ret=%d  E\n",ret);
	
	if (ret < 0)
		return ret;

	return count;
}

static DEVICE_ATTR(pers, S_IWUSR | S_IRUGO,
		   apds9900_show_pers, apds9900_store_pers);

static ssize_t apds9900_show_config(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct apds9900_data *data = i2c_get_clientdata(client);

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_show_config  data->config=%d  E\n",data->config);
	
	return sprintf(buf, "%d\n", data->config);
}

static ssize_t apds9900_store_config(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	unsigned long val = simple_strtoul(buf, NULL, 10);
	int ret;

	ret = apds9900_set_config(client, val);

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_store_config  ret=%d  E\n",ret);
	if (ret < 0)
		return ret;

	return count;
}

static DEVICE_ATTR(config, S_IWUSR | S_IRUGO,
		   apds9900_show_config, apds9900_store_config);

static ssize_t apds9900_show_ppcount(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct apds9900_data *data = i2c_get_clientdata(client);

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_show_ppcount  data->ppcount=%d  E\n",data->ppcount);
	
	return sprintf(buf, "%d\n", data->ppcount);
}

static ssize_t apds9900_store_ppcount(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	unsigned long val = simple_strtoul(buf, NULL, 10);
	int ret;

	ret = apds9900_set_ppcount(client, val);

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_store_ppcount  ret=%d  E\n",ret);
	if (ret < 0)
		return ret;

	return count;
}

static DEVICE_ATTR(ppcount, S_IWUSR | S_IRUGO,
		   apds9900_show_ppcount, apds9900_store_ppcount);

static ssize_t apds9900_show_control(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct apds9900_data *data = i2c_get_clientdata(client);

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_show_control  data->control=%d  E\n",data->control);

	return sprintf(buf, "%d\n", data->control);
}

static ssize_t apds9900_store_control(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	unsigned long val = simple_strtoul(buf, NULL, 10);
	int ret;

	ret = apds9900_set_control(client, val);

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_store_control  ret=%d  E\n",ret);
	if (ret < 0)
		return ret;

	return count;
}

static DEVICE_ATTR(control, S_IWUSR | S_IRUGO,
		   apds9900_show_control, apds9900_store_control);

static ssize_t apds9900_show_rev(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct apds9900_data *data = i2c_get_clientdata(client);
	int rev;

	mutex_lock(&data->update_lock);
	rev = i2c_smbus_read_byte_data(client, CMD_BYTE|APDS9900_REV_REG);
	mutex_unlock(&data->update_lock);

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_show_rev  rev=%d  E\n",rev);

	return sprintf(buf, "%d\n", rev);
}

static DEVICE_ATTR(rev, S_IRUGO,
		   apds9900_show_rev, NULL);

static ssize_t apds9900_show_devid(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct apds9900_data *data = i2c_get_clientdata(client);
	int id;

	mutex_lock(&data->update_lock);
	id = i2c_smbus_read_byte_data(client, CMD_BYTE|APDS9900_ID_REG);
	mutex_unlock(&data->update_lock);


	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_show_devid  id=0x%x  E\n",id);


	return sprintf(buf, "%d\n", id);
}

static DEVICE_ATTR(devid, S_IRUGO,
		   apds9900_show_devid, NULL);

static ssize_t apds9900_show_status(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct apds9900_data *data = i2c_get_clientdata(client);
	int status;

	mutex_lock(&data->update_lock);
	status = i2c_smbus_read_byte_data(client, CMD_BYTE|APDS9900_STATUS_REG);
	mutex_unlock(&data->update_lock);

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_show_status  rev=%d  E\n",status);
	
	return sprintf(buf, "%d\n", status);
}

static DEVICE_ATTR(status, S_IRUGO,
		   apds9900_show_status, NULL);

static ssize_t apds9900_show_cdata(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct apds9900_data *data = i2c_get_clientdata(client);
	int cdata;

	mutex_lock(&data->update_lock);
	cdata = i2c_smbus_read_word_data(client, CMD_WORD|APDS9900_CDATAL_REG);
	mutex_unlock(&data->update_lock);

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_show_status  rev=%d  E\n",cdata);
	
	return sprintf(buf, "%d\n", cdata);
}

static DEVICE_ATTR(cdata, S_IRUGO,
		   apds9900_show_cdata, NULL);

static ssize_t apds9900_show_irdata(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct apds9900_data *data = i2c_get_clientdata(client);
	int irdata;

	mutex_lock(&data->update_lock);
	irdata = i2c_smbus_read_word_data(client, CMD_WORD|APDS9900_IRDATAL_REG);
	mutex_unlock(&data->update_lock);

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_show_irdata  rev=%d  E\n",irdata);
	
	return sprintf(buf, "%d\n", irdata);
}

static DEVICE_ATTR(irdata, S_IRUGO,
		   apds9900_show_irdata, NULL);

static ssize_t apds9900_show_pdata(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct apds9900_data *data = i2c_get_clientdata(client);
	int pdata;

	mutex_lock(&data->update_lock);
	pdata = i2c_smbus_read_word_data(client, CMD_WORD|APDS9900_PDATAL_REG);
	mutex_unlock(&data->update_lock);

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_show_pdata  rev=%d  E\n",pdata);	
	return sprintf(buf, "%d\n", pdata);
}

static DEVICE_ATTR(pdata, S_IRUGO,
		   apds9900_show_pdata, NULL);

static struct attribute *apds9900_attributes[] = {
	&dev_attr_command.attr,
	&dev_attr_enable.attr,
	&dev_attr_atime.attr,
	&dev_attr_ptime.attr,
	&dev_attr_wtime.attr,
	&dev_attr_ailt.attr,
	&dev_attr_aiht.attr,
	&dev_attr_pilt.attr,
	&dev_attr_piht.attr,
	&dev_attr_pers.attr,
	&dev_attr_config.attr,
	&dev_attr_ppcount.attr,
	&dev_attr_control.attr,
	&dev_attr_rev.attr,
	&dev_attr_devid.attr,
	&dev_attr_status.attr,
	&dev_attr_cdata.attr,
	&dev_attr_irdata.attr,
	&dev_attr_pdata.attr,
	NULL
};

static const struct attribute_group apds9900_attr_group = {
	.attrs = apds9900_attributes,
};

/*
 * Initialization function
 */

static int apds9900_init_client(struct i2c_client *client)
{
	struct apds9900_data *data = i2c_get_clientdata(client);
	int err;


	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_init_client S\n");	
	
	err = apds9900_set_enable(client, 0x3f);

	if (err < 0)
		return err;

	mdelay(1);

	mutex_lock(&data->update_lock);
	err = i2c_smbus_read_byte_data(client, APDS9900_ENABLE_REG);
	mutex_unlock(&data->update_lock);

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_init_client  err=%d  E\n",err);
	
	if (err != 0)
		return -ENODEV;

	data->enable = 0x3f;

	return 0;
}
//-------------- Work func ---------------//
static int round_off(int dividend, int divisor)
{
	int result;
	int quotient;
	int remainder;
	int base = 10;

	quotient = dividend/divisor;
	remainder = dividend%divisor;
	if(remainder >= base/2)
		result = quotient+1;
	else
		result = quotient;

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
	int als_gain = 0;
	int cpl = 0;
	int lux = 0;

	atime = 1;
	ptime = 1;
	wtime = 1;
	als_gain = 1;
	
	apds9900_show_cdata(&this_client->dev,NULL,buf);	
	clear_data = simple_strtoul(buf, NULL, 10);
	
	apds9900_show_irdata(&this_client->dev,NULL,buf);
	ir_data = simple_strtoul(buf, NULL, 10);

	//------------ atime <---> alsit ------------//
	//atime = 256 - alsit /2.72(ms)
	//alsit = 2.72*(atime-256)
	alsit = round_off(272*(atime - 256), 100);

	//lux calculating
	//max(iac_1, iac_2, 0)
	iac_1 = 10*clear_data - 20*ir_data;  //iac_1 = cdata - 2*irdata
	iac_2 = 8*clear_data - 14*ir_data;   //iac_2 = 0.8*cdata - 1.4*irdata

	if(iac_1 > 0 || iac_2 > 0){
		if(iac_1 >= iac_2)
			iac_max = round_off(iac_1, 10);
		else if(iac_1 < iac_2)
			iac_max = round_off(iac_2, 10);
	}else
		iac_max = 0;
	//cpl = (alsit*als_gain)/(dev_factory*0.46)
	//glass attenuation factory = 0.46  >>> it should be changed by the real condition
	cpl = (round_off((alsit*als_gain), (dev_factor*46)))*100;
	//lux = iac_max/cpl
	lux = round_off(iac_max, cpl);
	
	return lux;

}

static void apds9900_work_func(struct work_struct *work)
{
	struct apds9900_data *data = container_of((struct delayed_work *)work, struct apds9900_data, work);
	int value;
	int delay = DEFAULT_DELAY_TIME;
	value = get_sensor_light_data();
	printk(KERN_INFO "[apds9900]>>>light sensor value: %d \n", value);
	input_report_abs(data->input, ABS_X, value);	
	input_sync(data->input);
	//atomic_set(&data->last, value);
	schedule_delayed_work(&data->work, msecs_to_jiffies(delay)+1);
}

//-------------- Input dev init and finish ---------------//
static int apds9900_input_init(struct apds9900_data *data)
{
	struct input_dev *dev;
    int err;

    dev = input_allocate_device();
    if (!dev) {
        return -ENOMEM;
    }
    dev->name = "light";
    dev->id.bustype = BUS_I2C;

    input_set_capability(dev, EV_ABS, ABS_MISC);

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

/*static vo
apds9900_input_finish(struct apds9900_data *data)
{
	struct input_dev *dev = data->input;

	input_unregister_device(dev);
	input_free_device(dev);
}*/

/*
 * I2C init/probing/exit functions
 */

static int __devinit apds9900_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct apds9900_data *apds = NULL;
	int err = 0;

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_probe S\n");
	
	//setup i2c client
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE)) {
		err = -EIO;
		goto exit;
	}
	//setup private data
	apds = kzalloc(sizeof(struct apds9900_data), GFP_KERNEL);
	if (!apds) {
		err = -ENOMEM;
        	goto exit_kfree;
	}
	/*data = kzalloc(sizeof(struct apds9900_data), GFP_KERNEL);
	if (!data) {
		err = -ENOMEM;
	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_probe kzalloc fail\n");
		goto exit;
	}*/
	apds->client = client;
	i2c_set_clientdata(client, apds);

	apds->enable = 0;	/* default mode is standard */
	dev_info(&client->dev, "enable = %s\n",
			apds->enable ? "1" : "0");

	mutex_init(&apds->update_lock);
	mutex_init(&apds->data_mutex);
	/* Initialize the APDS9900 chip */
	err = apds9900_init_client(client);

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_probe  err=%d  E\n",err);

	pdata =apds;//test code
	
	if(1)
	{//test code
		char buf[20];
		apds9900_show_enable(&client->dev,NULL,buf);	
		
		apds9900_show_atime(&client->dev,NULL,buf);
		apds9900_set_atime(client, 0xff);
		apds9900_show_atime(&client->dev,NULL,buf);	
		
		apds9900_show_ptime(&client->dev,NULL,buf);	
		apds9900_set_ptime(client, 0xff);
		apds9900_show_ptime(&client->dev,NULL,buf);	
		
		apds9900_show_wtime(&client->dev,NULL,buf);	
		apds9900_set_wtime(client, 0xff);
		apds9900_show_wtime(&client->dev,NULL,buf);	
		
		apds9900_show_devid(&client->dev,NULL,buf);	
		apds9900_show_status(&client->dev,NULL,buf);		
	}

	//func work queue
	INIT_DELAYED_WORK(&apds->work, apds9900_work_func);
	if (err)
		goto exit_kfree;

	//init input device
	err = apds9900_input_init(apds);
	if (err < 0) {
		goto exit_kfree;
	}

	/* Register sysfs hooks */
	err = sysfs_create_group(&apds->input->dev.kobj, &apds9900_attr_group);
	if (err)
		goto exit_kfree;

	dev_info(&client->dev, "support ver. %s enabled\n", DRIVER_VERSION);

	return 0;

exit_kfree:
	kfree(apds);
exit:
	return err;
}

static int __devexit apds9900_remove(struct i2c_client *client)
{
	sysfs_remove_group(&client->dev.kobj, &apds9900_attr_group);

	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_remove S\n");
	/* Power down the device */
	apds9900_set_enable(client, 0);

	kfree(i2c_get_clientdata(client));

	return 0;
}

#ifdef CONFIG_PM

static int apds9900_suspend(struct i2c_client *client, pm_message_t mesg)
{
	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_suspend S\n");
	
	return apds9900_set_enable(client, 0);
}

static int apds9900_resume(struct i2c_client *client)
{
	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_resume S\n");
	
	return apds9900_set_enable(client, 0);
}

#else

#define apds9900_suspend	NULL
#define apds9900_resume		NULL

#endif /* CONFIG_PM */

static const struct i2c_device_id apds9900_id[] = {
	{ "apds9900", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, apds9900_id);

static struct i2c_driver apds9900_driver = {
	.driver = {
		.name	= APDS9900_DRV_NAME,
		.owner	= THIS_MODULE,
	},
	.suspend = apds9900_suspend,
	.resume	= apds9900_resume,
	.probe	= apds9900_probe,
	.remove	= __devexit_p(apds9900_remove),
	.id_table = apds9900_id,
};

static int __init apds9900_init(void)
{
	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_init S\n");
	
	return i2c_add_driver(&apds9900_driver);
}

static void __exit apds9900_exit(void)
{
	printk(KERN_ERR "[motoko][sensor][apds9900]apds9900_exit S\n");
	
	i2c_del_driver(&apds9900_driver);
}

int apds9900_command;//TEST_CODE

static int get_sensor_light_data_1(const char *val, struct kernel_param *kp)
{
	char buf[20];
		
apds9900_show_enable(&pdata->client->dev,NULL,buf);
//	if(apds9900_command == 0)
		apds9900_show_pdata(&pdata->client->dev,NULL,buf);	
	
//	if(apds9900_command == 1)
		apds9900_show_irdata(&pdata->client->dev,NULL,buf);

//	if(apds9900_command == 2)
		apds9900_show_cdata(&pdata->client->dev,NULL,buf);	
	
		apds9900_show_ppcount(&pdata->client->dev,NULL,buf);	

	return 0;	
}

module_param_call(read_light_data,get_sensor_light_data_1 , param_get_int, &apds9900_command, S_IWUSR |S_IRUGO);//TEST_CODE


MODULE_AUTHOR("Lee Kai Koon <kai-koon.lee@avagotech.com>");
MODULE_DESCRIPTION("APDS9900 ambient light + proximity sensor driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRIVER_VERSION);

module_init(apds9900_init);
module_exit(apds9900_exit);
