/* drivers/input/touchscreen/atmel_mxt224.c - ATMEL Touch driver
*
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/platform_device.h>
#include <linux/freezer.h>
#include <linux/proc_fs.h>
#include <linux/clk.h>
#include <linux/i2c.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/err.h>
#include <asm/io.h>
#include <asm/gpio.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <mach/board.h>
#include <asm/mach-types.h>
#include <linux/jiffies.h>
#include <linux/poll.h>
#include <linux/kfifo.h>
#include <linux/version.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/mfd/pmic8058.h>
#include <linux/atmel-mxt224.h>
#include <linux/regulator/consumer.h>

//Henry_lin, 20110727, [BugID 540] Touch IC report frequency too slow.
#if 0
#include <linux/time.h>
#endif

//Henry_lin, 20110815, [BugID 641] Enable/disable Home key wake up system ability.
#include <linux/pmic8058-pwrkey.h>


//#define PMIC_INT_GPIO
#define DEBUG
#ifdef DEBUG
	#define TS_DEBUG(fmt,args...)  printk( KERN_DEBUG "[egalax_i2c]: " fmt, ## args)
	#define DBG() printk("[%s]:%d => \n",__FUNCTION__,__LINE__)
#else
	#define TS_DEBUG(fmt,args...)
	#define DBG()
#endif


/*---------------------  Static Definitions -------------------------*/
#define HENRY_DEBUG 0   //0:disable, 1:enable
#if(HENRY_DEBUG)
    #define Printhh(string, args...)    printk("Henry(K)=> "string, ##args);
#else
    #define Printhh(string, args...)
#endif

#define HENRY_TIP 1 //give RD information. Set 1 if develop,and set 0 when release.
#if(HENRY_TIP)
    #define PrintTip(string, args...)    printk("Henry(K)=> "string, ##args);
#else
    #define PrintTip(string, args...)
#endif

#define HENRY_DEBUG_VK 1   //0:disable, 1:enable; 
//when set 0 REMEBER to mask chmod 0644 /sys/touch/vk_status in file system\core\rootdir\Init.rc


/*---------------------  Static Classes  ----------------------------*/

//#define _NON_INPUT_DEV // define this to disable register input device
//#ifdef CONFIG_CCI_PRODUCT_DA80
//#else   //for CA80
//    #define D_CCI_TOUCH_KEY
//#endif

#define D_TOUCH_KEY_NUM 3
#define D_TOUCH_KEY_DISPLAY_EXTEND 80
#define D_TOUCH_KEY_Y_CLIP 15
#define D_LCD_DISPLAY_WIDTH 480
#define D_LCD_DISPLAY_HIGH 800
#define D_TOUCH_KEY_BOUNDRY D_LCD_DISPLAY_HIGH
#define D_TOUCH_KEY_HIGHT (D_TOUCH_KEY_DISPLAY_EXTEND/2)
#define D_TOUCH_KEY_WIDTH (D_LCD_DISPLAY_WIDTH/4)
#define D_TOUCH_KEY_LK_X_POS (D_LCD_DISPLAY_WIDTH/6)
#define D_TOUCH_KEY_MK_X_POS (D_TOUCH_KEY_LK_X_POS+(D_LCD_DISPLAY_WIDTH/D_TOUCH_KEY_NUM))
#define D_TOUCH_KEY_RK_X_POS (D_TOUCH_KEY_MK_X_POS+(D_LCD_DISPLAY_WIDTH/D_TOUCH_KEY_NUM))  
#define D_TOUCH_KEY_Y_MIN (D_LCD_DISPLAY_HIGH+D_TOUCH_KEY_Y_CLIP)
#define D_TOUCH_KEY_Y_MAX (D_LCD_DISPLAY_HIGH+D_TOUCH_KEY_DISPLAY_EXTEND-D_TOUCH_KEY_Y_CLIP )  
     
//Macros
#define MAX_I2C_LEN		10
#define MAX_SUPPORT_POINT	10
#define PM8058_IRQ_BASE				(256 + 173)
#define TOUCH_GPIO 	 10//PM8058_GPIO_IRQ(PM8058_IRQ_BASE, 11)
#define TOUCH_RST 		 126//PM8058_GPIO_IRQ(PM8058_IRQ_BASE, 127)

#define REPORTID_MOUSE		0x01
#define REPORTID_VENDOR		0x03
#define REPORTID_MTOUCH		0x04

//Macros
#define ATMEL_EN_SYSFS
#define ATMEL_I2C_RETRY_TIMES 10

//#define ENABLE_IME_IMPROVEMENT
#define TS_RESET 49// evb 31

#define KB_HOME1   102
#define KB_MENU   139
#define KB_BACK   158
#define KB_SEARCH 217

//Variables
#ifdef CONFIG_FB_MSM_MDDI_CCI_TPO_WVGA
extern int DVT_1_3;
extern int HW_ID4;
#endif

int TS_CHG = 61;//evb 82;//or 61?
//For preventing malfunctions.
//extern unsigned long ts_active_time;
//extern unsigned long key_active_time;
char g_caButton[10] = {0};


struct cci_virtual_key {
    int keyid;
    int x_min;
    int x_max;
    int y_min;
    int y_max;        
};
#define D_IS_EVENT_HANDLED 1
static int handleTouchKeyEvent(struct atmel_finger_data* pData, int fingerNum ); 

#ifdef D_CCI_TOUCH_KEY
struct  cci_virtual_key tKeyAttr[ D_TOUCH_KEY_NUM] = 
  {
  {1,D_TOUCH_KEY_LK_X_POS-(D_TOUCH_KEY_WIDTH/2), D_TOUCH_KEY_LK_X_POS+(D_TOUCH_KEY_WIDTH/2),D_TOUCH_KEY_Y_MIN,D_TOUCH_KEY_Y_MAX },
  {4,D_TOUCH_KEY_MK_X_POS-(D_TOUCH_KEY_WIDTH/2), D_TOUCH_KEY_MK_X_POS+(D_TOUCH_KEY_WIDTH/2), D_TOUCH_KEY_Y_MIN,  D_TOUCH_KEY_Y_MAX },
  {2,D_TOUCH_KEY_RK_X_POS-(D_TOUCH_KEY_WIDTH/2),D_TOUCH_KEY_RK_X_POS+(D_TOUCH_KEY_WIDTH/2), D_TOUCH_KEY_Y_MIN,  D_TOUCH_KEY_Y_MAX },
};    
static int isTouchKeyPressed = 0;
#endif
  
//Functions
#ifdef CONFIG_HAS_EARLYSUSPEND
static void atmel_ts_early_suspend(struct early_suspend *h);
static void atmel_ts_late_resume(struct early_suspend *h);
#endif

static int atmel_ts_power(int on);


//Structures
#ifdef CONFIG_SLATE_TEST
extern struct input_dev *ts_input_dev;
#endif

#if defined CONFIG_KEYBOARD_PMIC8058
extern struct input_dev *tskpdev;
#endif
#ifdef PMIC_INT_GPIO
extern int cci_pm8058_gpio_set(int gpio, int value);
extern int cci_pm8058_gpio_get(int gpio);
#endif

struct atmel_ts_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct workqueue_struct *atmel_wq;
	struct work_struct work;
	int (*power) (int on);
	struct early_suspend early_suspend;
	struct info_id_t *id;
	struct object_t *object_table;
	uint8_t finger_count;
	uint16_t abs_x_min;
	uint16_t abs_x_max;
	uint16_t abs_y_min;
	uint16_t abs_y_max;
	uint8_t abs_pressure_min;
	uint8_t abs_pressure_max;
	uint8_t abs_width_min;
	uint8_t abs_width_max;
	uint8_t first_pressed;
	uint8_t debug_log_level;
	struct atmel_finger_data finger_data[10];
	uint8_t finger_type;
	uint8_t finger_support;
	uint16_t finger_pressed;
	uint8_t face_suppression;
	uint8_t grip_suppression;
	uint8_t noise_status[2];
	uint16_t *filter_level;
	uint8_t calibration_confirm;
	uint64_t timestamp;
	struct atmel_config_data config_setting[2];
	uint8_t status;
	uint8_t GCAF_sample;
	uint8_t *GCAF_level;
	uint8_t noisethr;
#ifdef ATMEL_EN_SYSFS
	struct device dev;
#endif
#ifdef ENABLE_IME_IMPROVEMENT
	int display_width; /* display width in pixel */
	int display_height; /* display height in pixel */
	int ime_threshold_pixel; /* threshold in pixel */
	int ime_threshold[2]; /* threshold X & Y in raw data */
	int ime_area_pixel[4]; /* ime area in pixel */
	int ime_area_pos[4]; /* ime area in raw data */
	int ime_finger_report[2];
	int ime_move;
	#endif

};


//Henry_lin, 20110717, [BugID 476]  Supply Atmel mxt224E support.
#if 1   //for 224E config Sansumg
struct atmel_i2c_platform_data atmel_platform_data_224E = {
	.version = 0x016,
	.display_width = 540,
	.display_height = 960,
	.abs_x_min = 0,
 	.abs_x_max = 539,
 	.abs_y_min = 0,
 	.abs_y_max = 959,
 	.abs_pressure_min = 0,
	.abs_pressure_max = 255,
	.abs_width_min = 0,
	.abs_width_max = 20,
	.power = atmel_ts_power,
	.config_T6 = {0, 0, 0, 0, 0, 0},
//       .config_T7 = {32,16,50},
//       .config_T7 = {255,255,10},
       .config_T7 = {30,255,50},
//       .config_T8={10,0,5,5,0,0,15,20},
//       .config_T8={30,0,20,20,0,0,20,0},
       .config_T8={28,0,20,20,0,0,20,5},

//config_T9={B0,XlineStartPos,YlineStartPos,XNum,YNum,B5,gain,TCHTHR,B8,B9,B10,B11,B12,B13,       NumTouch,B15,B16,B17,XRange,XRange,YRange,YRange,XLoClip,XHiClip,YLoClip,YHiClip,     XEDGE_CTRL, XEDGE_DIST, Y, Y}
// 	.config_T9 = {131, 0, 0, 19, 11, 0, 0, 25, 2, 7, 0, 10, 2, 16,         4, 5, 5, 0, 192, 3, 28, 2, 0, 0, 0, 0,         138, 50, 138, 50, 40}, // X resolution = 960 Y=540
// 	.config_T9 = {3, 0, 0, 19, 11, 0, 32, 75, 3, 5, 0, 1, 1, 0,         4, 10, 10, 10, 192, 3, 28, 2, 11, 11, 15, 15,         151, 43, 145, 80, 100}, // X resolution = 960 Y=540
       //Henry_lin, 20110719, [BugID 488] Tune touch value for edge position.
       //Henry_lin, 20110810, [BugID 488] Tune touch value for edge position.
  	.config_T9 = {131, 0, 0, 19, 11, 0, 32, 50, 2, 7, 0, 3, 1, 0,         4, 10, 10, 10, 192, 3, 28, 2, 18, 24, 34, 35,         136, 45, 140, 65, 10}, // X resolution = 960 Y=540
	.config_T15 ={0},
       .config_T18 = {0, 0},
	.config_T19 = {0},
       .config_T20 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
       .config_T22 = { 5, 0, 0, 25, -25, 4, 20, 0, 1, 10, 15, 20, 25, 30, 4, 0, 0},
	.config_T23 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	.config_T24 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	.config_T25 = {0},
       .config_T27 = {0, 0, 0, 0, 0, 0 ,0},
       .config_T28 = {0,0,3,16,32,60},
        //Decken_Kang, 20110914, [BugID 890] Implement 224E new register for noise suppression
       .config_T40 = {0,0,0,0,0},	//new 224E
       .config_T42 = {0,0,0,0,0,0,0,0},	 //new 224E
       .config_T46 = {0,3,16,32,0,0,1,0,0},	 //new 224E, replace T28
       .config_T47 = {0,20,50,5,2,40,40,180,0,100},
       .config_T48 = {1,4,64,0,0,0,0,0,0,0,0,0,0,6,6,0,0,100,4,64,10,0,20,5,0,38,0,5,0,0,0,0,0,0,16,50,2,3,1,0,5,10,40,-28,-32,0,0,148,60,140,65,0,0,3},   //new 224E, replace T22
       //Decken_Kang
	.object_crc = {0x1A, 0x27, 0x8E},
	.cable_config = {30, 30, 8, 16},
	.GCAF_level = {20, 24, 28, 40, 63},
};
#endif
//Henry_lin


#if 0   //for 224E config ATMEL demo board
struct atmel_i2c_platform_data atmel_platform_data = {
	.version = 0x016,
	.display_width = 540,
	.display_height = 960,
	.abs_x_min = 0,
 	.abs_x_max = 539,
 	.abs_y_min = 0,
 	.abs_y_max = 959,
 	.abs_pressure_min = 0,
	.abs_pressure_max = 255,
	.abs_width_min = 0,
	.abs_width_max = 20,
	.power = atmel_ts_power,
	.config_T6 = {0, 0, 0, 0, 0, 0},
//       .config_T7 = {32,16,50},
       .config_T7 = {255,255,10},
//       .config_T8={10,0,5,5,0,0,15,20},
       .config_T8={30,0,20,20,0,0,20,0},

//config_T9={B0,XlineStartPos,YlineStartPos,XNum,YNum,B5,gain,TCHTHR,B8,B9,B10,B11,B12,B13,       NumTouch,B15,B16,B17,XRange,XRange,YRange,YRange,XLoClip,XHiClip,YLoClip,YHiClip,     XEDGE_CTRL, XEDGE_DIST, Y, Y}
// 	.config_T9 = {131, 0, 0, 19, 11, 0, 0, 25, 2, 7, 0, 10, 2, 16,         4, 5, 5, 0, 192, 3, 28, 2, 0, 0, 0, 0,         138, 50, 138, 50, 40}, // X resolution = 960 Y=540
 	.config_T9 = {3, 0, 0, 19, 11, 0, 32, 75, 3, 5, 0, 1, 1, 0,         4, 10, 10, 10, 192, 3, 28, 2, 11, 11, 15, 15,         151, 43, 145, 80, 100}, // X resolution = 960 Y=540
 	.config_T15 ={0},
       .config_T18 = {0, 0},
	.config_T19 = {0},
       .config_T20 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
       .config_T22 = { 5, 0, 0, 25, -25, 4, 20, 0, 1, 10, 15, 20, 25, 30, 4, 0, 0},
	.config_T23 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	.config_T24 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	.config_T25 = {0},
       .config_T27 = {0, 0, 0, 0, 0, 0 ,0},
       .config_T28 = {0,0,3,16,32,60},
       .config_T42 = {0},   //new 224E
       .config_T46 = {0},   //new 224E
       .config_T47 = {0},   //new 224E
       .config_T48 = {0},   //new 224E
	.object_crc = {0x1A, 0x27, 0x8E},
	.cable_config = {30, 30, 8, 16},
	.GCAF_level = {20, 24, 28, 40, 63},
};
#endif

// For Atmel mxt224 used
struct atmel_i2c_platform_data atmel_platform_data = {
	.version = 0x016,
	.display_width = 540,
	.display_height = 960,
	.abs_x_min = 0,
 	.abs_x_max = 539,
 	.abs_y_min = 0,
 	.abs_y_max = 959,
 	.abs_pressure_min = 0,
	.abs_pressure_max = 255,
	.abs_width_min = 0,
	.abs_width_max = 20,
	//.gpio_irq = 19,
	.power = atmel_ts_power,
	//.debug_log_level = 2,
	.config_T6 = {0, 0, 0, 0, 0, 0},
	//evb	.config_T7 = {50, 15, 25},
       //Henry_lin, 20110727, [BugID 540] Touch IC report frequency too slow.
       //.config_T7 = {32,16,50},
       .config_T7 = {32,255,50},
	//.config_T8 = {9, 0, 0, 0, 0, 0, 10, 15},
	// evb .config_T8 = {8, 0, 20, 10, 0, 0, 20, 40},       // 0901 for Atmel
        .config_T8={10,0,5,5,0,0,15,20},

//config_T9={B0,XlineStartPos,YlineStartPos,XNum,YNum,B5,gain,TCHTHR,B8,B9,B10,B11,B12,B13,       NumTouch,B15,B16,B17,XRange,XRange,YRange,YRange,XLoClip,XHiClip,YLoClip,YHiClip,     XEDGE_CTRL, XEDGE_DIST, Y, Y}
       //Henry_lin, 20110518, [BugID 111] Support 4 touches to reported.
       //Henry_lin, 20110527, [BugID 158] Change Touch more smooth and virtual key config.
       //Henry_lin, 20110607, [BugID 204] Tune touch point position.
       //Henry_lin, 20110607, [BugID 404] Movement slow issue.
       //Henry_lin, 20110719, [BugID 488] Tune touch value for edge position.
 	.config_T9 = {131, 0, 0, 19, 11, 0, 0, 25, 2, 7, 0, 10, 2, 16,         4, 5, 5, 0, 192, 3, 28, 2, 10, 10, 30, 30,         138, 50, 138, 50, 40}, // X resolution = 960 Y=540
 	.config_T15 ={0},
        .config_T18 = {0, 0},
	//evb.config_T19 = {1, 0, 0, 48, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	.config_T19 = {0},
        .config_T20 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
 // evb .config_T22 = {15, 0, 0, 0, 20, 1, 10, 20, 30, 255, 255, 0, 0, 0, 0, 0, 0},
        .config_T22 = { 5, 0, 0, 25, -25, 4, 20, 0, 1, 10, 15, 20, 25, 30, 4, 0, 0},
	.config_T23 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	.config_T24 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	//evb .config_T25 = {3, 0, 0xa4,0x38, 0x28,0x23,0x04,0x29,0x58, 0x1b,0 ,0, 0, 0},
	.config_T25 = {0},
        .config_T27 = {0, 0, 0, 0, 0, 0 ,0},
	// evb.config_T28 = {0, 0, 2, 4, 8, 0},
        .config_T28 = {0,0,3,16,32,60},
//	.object_crc = {0x4e, 0x7, 0x6f},
	.object_crc = {0x4e, 0x7, 0x6e},
	.cable_config = {30, 30, 8, 16},
	.GCAF_level = {20, 24, 28, 40, 63},
	//.filter_level = {46, 100, 923, 978},
};

enum tp_key_array {
	BACK_BTN,
	MENU_BTN,
	HOME_BTN,
	SEARCH_BTN, // For DA80
};

static struct atmel_ts_data *private_ts;


static int atmel_ts_power(int on)
{
//Henry_lin, 20110902, [BugID 796] Remove unused regulator code.
#if 0
    struct regulator* vreg_s3=NULL;


    //printk(KERN_INFO "da80 atmel mxt224 power(%d)\n", on);
    vreg_s3 = regulator_get(NULL, "8058_l16");

    regulator_set_voltage(vreg_s3, 1800000, 1800000);
    regulator_enable(vreg_s3);
#endif

    gpio_direction_output(TS_RESET, 1);
    	
    mdelay(200);

    if (on) {
        //printk("%s: TS_RESET = %d\n",__func__,gpio_get_value(TS_RESET) );
        gpio_set_value(TS_RESET, 1);
        //printk("%s: TS_RESET = %d\n",__func__,gpio_get_value(TS_RESET) );
        msleep(5);
        gpio_set_value(TS_RESET, 0);
        //printk("%s: TS_RESET = %d\n",__func__,gpio_get_value(TS_RESET) );
        msleep(5);
        gpio_set_value(TS_RESET, 1); 
        //printk("%s: TS_RESET = %d\n",__func__,gpio_get_value(TS_RESET) );
        msleep(64);
        //printk("%s: TS_RESET = %d\n",__func__,gpio_get_value(TS_RESET) );
    }
    else 
    {
        gpio_set_value(TS_RESET, 0);
        msleep(2);
    }

    return 0;
}


int i2c_atmel_read(struct i2c_client *client, uint16_t address, uint8_t *data, uint8_t length)
{
	int retry;
	uint8_t addr[2];

	struct i2c_msg msg[] = {
	    {
	        .addr = client->addr,
	        .flags = 0,
	        .len = 2,
	        .buf = addr,
	    },
	    {
	        .addr = client->addr,
	        .flags = I2C_M_RD,
	        .len = length,
	        .buf = data,
	    }
	};
	addr[0] = address & 0xFF;
	addr[1] = (address >> 8) & 0xFF;

	for (retry = 0; retry < ATMEL_I2C_RETRY_TIMES; retry++) {
		if (i2c_transfer(client->adapter, msg, 2) == 2)
			break;
		mdelay(10);
	}
	if (retry == ATMEL_I2C_RETRY_TIMES) {
		printk(KERN_ERR "i2c_read_block retry over %d\n",
		ATMEL_I2C_RETRY_TIMES);
		return -EIO;
	}
	return 0;

}

int i2c_atmel_write(struct i2c_client *client, uint16_t address, uint8_t *data, uint8_t length)
{
	int retry, loop_i;
	uint8_t buf[length + 2];

	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = length + 2,
			.buf = buf,
		}
	};
	
	buf[0] = address & 0xFF;
	buf[1] = (address >> 8) & 0xFF;

	for (loop_i = 0; loop_i < length; loop_i++)
		buf[loop_i + 2] = data[loop_i];
	for (retry = 0; retry < ATMEL_I2C_RETRY_TIMES; retry++) {
		if (i2c_transfer(client->adapter, msg, 1) == 1)
			break;
		mdelay(10);
	}

	if (retry == ATMEL_I2C_RETRY_TIMES) {
		printk(KERN_ERR "i2c_write_block retry over %d\n",ATMEL_I2C_RETRY_TIMES);
		return -EIO;
	}
	return 0;

}

int i2c_atmel_write_byte_data(struct i2c_client *client, uint16_t address, uint8_t value)
{
	i2c_atmel_write(client, address, &value, 1);
	return 0;
}

uint16_t get_object_address(struct atmel_ts_data *ts, uint8_t object_type)
{
	uint8_t loop_i;
	for (loop_i = 0; loop_i < ts->id->num_declared_objects; loop_i++) {
		if (ts->object_table[loop_i].object_type == object_type)
			return ts->object_table[loop_i].i2c_address;
	}
	return 0;
}
uint8_t get_object_size(struct atmel_ts_data *ts, uint8_t object_type)
{
	uint8_t loop_i;
	for (loop_i = 0; loop_i < ts->id->num_declared_objects; loop_i++) {
		if (ts->object_table[loop_i].object_type == object_type)
			return ts->object_table[loop_i].size;
	}
	return 0;
}

uint8_t get_report_ids_size(struct atmel_ts_data *ts, uint8_t object_type)
{
	uint8_t loop_i;
	for (loop_i = 0; loop_i < ts->id->num_declared_objects; loop_i++) {
		if (ts->object_table[loop_i].object_type == object_type)
			return ts->object_table[loop_i].report_ids;
	}
	return 0;
}

#ifdef ATMEL_EN_SYSFS
static ssize_t atmel_gpio_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	struct atmel_ts_data *ts_data;
	struct atmel_i2c_platform_data *pdata;

	ts_data = private_ts;
	pdata = ts_data->client->dev.platform_data;

	ret = gpio_get_value(pdata->gpio_irq);
	printk(KERN_DEBUG "GPIO_TP_INT_N=%d\n", pdata->gpio_irq);
	sprintf(buf, "GPIO_TP_INT_N=%d\n", ret);
	ret = strlen(buf) + 1;
	return ret;
}
static DEVICE_ATTR(gpio, 0444, atmel_gpio_show, NULL);
static ssize_t atmel_vendor_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	struct atmel_ts_data *ts_data;
	ts_data = private_ts;
	sprintf(buf, "%s_x%4.4X_x%4.4X\n", "ATMEL",ts_data->id->family_id, ts_data->id->version);
	ret = strlen(buf) + 1;
	return ret;
}

static DEVICE_ATTR(vendor, 0444, atmel_vendor_show, NULL);

static uint16_t atmel_reg_addr;

static ssize_t atmel_register_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	int ret = 0;
	uint8_t ptr[1];
	struct atmel_ts_data *ts_data;
	ts_data = private_ts;
	if (i2c_atmel_read(ts_data->client, atmel_reg_addr, ptr, 1) < 0) {
		printk(KERN_WARNING "%s: read fail\n", __func__);
		return ret;
	}
	ret += sprintf(buf, "addr: %d, data: %d\n", atmel_reg_addr, ptr[0]);
	return ret;
}

static ssize_t atmel_register_store(struct device *dev,struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	struct atmel_ts_data *ts_data;
	char buf_tmp[4], buf_zero[200];
	uint8_t write_da;

	ts_data = private_ts;
	memset(buf_tmp, 0x0, sizeof(buf_tmp));
	if ((buf[0] == 'r' || buf[0] == 'w') && buf[1] == ':' && (buf[5] == ':' || buf[5] == '\n')) {
		memcpy(buf_tmp, buf + 2, 3);
		atmel_reg_addr = simple_strtol(buf_tmp, NULL, 10);
		printk(KERN_DEBUG "read addr: 0x%X\n", atmel_reg_addr);
		if (!atmel_reg_addr) {
			printk(KERN_WARNING "%s: string to number fail\n",	__func__);
			return count;
		}
		printk(KERN_DEBUG "%s: set atmel_reg_addr is: %d\n",
		__func__, atmel_reg_addr);
		if (buf[0] == 'w' && buf[5] == ':' && buf[9] == '\n') {
			memcpy(buf_tmp, buf + 6, 3);
			write_da = simple_strtol(buf_tmp, NULL, 10);
			printk(KERN_DEBUG "write addr: 0x%X, data: 0x%X\n",
				atmel_reg_addr, write_da);
			ret = i2c_atmel_write_byte_data(ts_data->client,
				atmel_reg_addr, write_da);
			if (ret < 0) {
				printk(KERN_ERR "%s: write fail(%d)\n",__func__, ret);
			}
		}
	}
	if ((buf[0] == '0') && (buf[1] == ':') && (buf[5] == ':')) {
		memcpy(buf_tmp, buf + 2, 3);
		atmel_reg_addr = simple_strtol(buf_tmp, NULL, 10);
		memcpy(buf_tmp, buf + 6, 3);
		memset(buf_zero, 0x0, sizeof(buf_zero));
		ret = i2c_atmel_write(ts_data->client, atmel_reg_addr,
		buf_zero, simple_strtol(buf_tmp, NULL, 10) - atmel_reg_addr + 1);
		if (buf[9] == 'r') {
                    printk(KERN_INFO "[%s] Backup NVM and Reset device!!!!\n", __FUNCTION__);
			i2c_atmel_write_byte_data(ts_data->client,
				get_object_address(ts_data, GEN_COMMANDPROCESSOR_T6) + 1, 0x55);
			i2c_atmel_write_byte_data(ts_data->client,
				get_object_address(ts_data, GEN_COMMANDPROCESSOR_T6), 0x11);
		}
	}

return count;
}

static DEVICE_ATTR(register, 0644, atmel_register_show, atmel_register_store);

static ssize_t atmel_regdump_show(struct device *dev,struct device_attribute *attr, char *buf)
{
    //Decken_Kang, 20110914, [BugID 890] Implement 224E new register for noise suppression
    int count = 0, ret_t = 0;
    struct atmel_ts_data *ts_data;
    uint16_t loop_i;
    uint8_t ptr[1];
    int iStartAddr = 248, iEndAddr = 410 + 0x36 - 1; 


    ts_data = private_ts;
    if (ts_data->id->version >= 0x14) {
        if(ts_data->id->family_id == 0x81 && ts_data->id->variant_id == 0x01)
        {
            //224E
            for (loop_i = iStartAddr; loop_i <= iEndAddr; loop_i++) {
                ret_t = i2c_atmel_read(ts_data->client, loop_i, ptr, 1);
                if (ret_t < 0) {
                    printk(KERN_WARNING "dump fail, addr: %d\n",  loop_i);
                }
                count += sprintf(buf + count, "addr[%3d]: %3d, ", loop_i , *ptr);
                if (((loop_i - iStartAddr) % 4) == 3)
                    count += sprintf(buf + count, "\n");
            }
            count += sprintf(buf + count, "\n");
            goto finish;
        }
        for (loop_i = 251; loop_i <= 425; loop_i++) {
            ret_t = i2c_atmel_read(ts_data->client, loop_i, ptr, 1);
            if (ret_t < 0) {
                printk(KERN_WARNING "dump fail, addr: %d\n",  loop_i);
            }
            count += sprintf(buf + count, "addr[%3d]: %3d, ", loop_i , *ptr);
            if (((loop_i - 251) % 4) == 3)
                count += sprintf(buf + count, "\n");
        }
        count += sprintf(buf + count, "\n");
    }

finish:
    return count;
   //Decken_Kang
}

static ssize_t atmel_regdump_dump(struct device *dev,struct device_attribute *attr, const char *buf, size_t count)
{
	struct atmel_ts_data *ts_data;
	ts_data = private_ts;
	if (buf[0] >= '0' && buf[0] <= '9' && buf[1] == '\n')
		ts_data->debug_log_level = buf[0] - 0x30;

	return count;

}

static DEVICE_ATTR(regdump, 0644, atmel_regdump_show, atmel_regdump_dump);

static ssize_t atmel_debug_level_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct atmel_ts_data *ts_data;
	size_t count = 0;
	ts_data = private_ts;

	count += sprintf(buf, "%d\n", ts_data->debug_log_level);

	return count;
}

static ssize_t atmel_debug_level_dump(struct device *dev,struct device_attribute *attr, const char *buf, size_t count)
{
	struct atmel_ts_data *ts_data;
	ts_data = private_ts;
	if (buf[0] >= '0' && buf[0] <= '9' && buf[1] == '\n')
		ts_data->debug_log_level = buf[0] - 0x30;

	return count;
}

static DEVICE_ATTR(debug_level, 0644, atmel_debug_level_show, atmel_debug_level_dump);


static ssize_t atmel_register_table(struct device *dev, struct device_attribute *attr, char *buf)
{
    int ii, iCnt = 0;


    for (ii = 0; ii < private_ts->id->num_declared_objects; ii++) {
        iCnt += sprintf(buf + iCnt, "Type: %2.2X, Start: %4.4d, Size: %2X, Instance: %2X, RD#: %2X, %2X\n",
            private_ts->object_table[ii].object_type , private_ts->object_table[ii].i2c_address,
            private_ts->object_table[ii].size, private_ts->object_table[ii].instances,
            private_ts->object_table[ii].num_report_ids, private_ts->object_table[ii].report_ids);
    }

    return iCnt;
}
static DEVICE_ATTR(regtable, 0444, atmel_register_table, NULL);


//Henry_lin, 20110531, [BugID 153] Implement testing AT command for keypad.
int g_iCode = 0;
static ssize_t atmel_at_cmd_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct atmel_ts_data *ts_data;
	size_t count = 0;
	ts_data = private_ts;

	count += sprintf(buf, "%d\n", g_iCode);

	return count;
}

static ssize_t atmel_at_cmd_set(struct device *dev,struct device_attribute *attr, const char *buf, size_t count)
{
    struct atmel_ts_data *ts_data;
    char buf_tmp[4];
    int iCode;


    ts_data = private_ts;
    memset(buf_tmp, 0x0, sizeof(buf_tmp));
    memcpy(buf_tmp, buf, count);
    iCode = simple_strtol(buf_tmp, NULL, 10);
    Printhh("[%s] iCode = %d\n", __FUNCTION__, iCode);
    g_iCode = iCode;

#if defined CONFIG_KEYBOARD_PMIC8058
    input_report_key(tskpdev, iCode, 1);
    input_sync(tskpdev);
    input_report_key(tskpdev, iCode, 0);
    input_sync(tskpdev);
#endif
    return count;
}

static DEVICE_ATTR(at_cmd, 0666, atmel_at_cmd_show, atmel_at_cmd_set);
//Henry_lin



//Henry_lin, 20110815, [BugID 641] Enable/disable Home key wake up system ability.
int g_iHomeKeyWakeUpEn = 1;
int g_iHomeKeyBkp = 0;
extern int g_iIrqCanWakeSys[MAX_WAKE_KEY_NUM];  // define in file Msm8x60-keypad.c (drivers\input\misc)

static ssize_t home_key_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	size_t count = 0;

	count += sprintf(buf, "g_iHomeKeyWakeUpEn = %d\n", g_iHomeKeyWakeUpEn);
	return count;
}

static ssize_t home_key_set(struct device *dev,struct device_attribute *attr, const char *buf, size_t count)
{
    char buf_tmp[4];
    int iHomeKeyWakeUpEn;


    memset(buf_tmp, 0x0, sizeof(buf_tmp));
    memcpy(buf_tmp, buf, count);
    iHomeKeyWakeUpEn = simple_strtol(buf_tmp, NULL, 10);
    Printhh("[%s] iHomeKeyWakeUpEn = %d\n", __FUNCTION__, iHomeKeyWakeUpEn);
    if(g_iHomeKeyWakeUpEn == iHomeKeyWakeUpEn)
        return count;   // exit function
        
    g_iHomeKeyWakeUpEn = iHomeKeyWakeUpEn;

    // use for disable then enable it
    if(g_iHomeKeyBkp == 0)
        g_iHomeKeyBkp = g_iIrqCanWakeSys[0];    // [0] is home key

    
    if(g_iHomeKeyWakeUpEn == 0)
        g_iIrqCanWakeSys[0] = -1;
    else
        g_iIrqCanWakeSys[0] = g_iHomeKeyBkp;
    
    return count;
}

static DEVICE_ATTR(home_key_wake_en, 0666, home_key_show, home_key_set);
//Henry_lin


//Henry_lin, 20110829, [BugID 712] Enable/disable virtual key reset feature.
int g_iVirtualKeyResetEn = 0;
static ssize_t virtual_key_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	size_t count = 0;

	count += sprintf(buf, "g_iVirtualKeyResetEn = %d\n", g_iVirtualKeyResetEn);
	return count;
}

static ssize_t virtual_key_set(struct device *dev,struct device_attribute *attr, const char *buf, size_t count)
{
    char buf_tmp[4];
    int iVirtualKeyResetEn;


    memset(buf_tmp, 0x0, sizeof(buf_tmp));
    memcpy(buf_tmp, buf, count);
    iVirtualKeyResetEn = simple_strtol(buf_tmp, NULL, 10);
    Printhh("[%s] iVirtualKeyResetEn = %d\n", __FUNCTION__, iVirtualKeyResetEn);
    if(g_iVirtualKeyResetEn == iVirtualKeyResetEn)
        return count;   // exit function
        
    g_iVirtualKeyResetEn = iVirtualKeyResetEn;
    return count;
}

static DEVICE_ATTR(virtual_key_reset_en, 0666, virtual_key_show, virtual_key_set);
//Henry_lin


#if(HENRY_DEBUG_VK)
static ssize_t virtual_key_status_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	size_t count = 0;
	unsigned int auiStatus[4];

#if 0
#define CCI_BACK_KEY_GPIO       39
#define CCI_MENU_KEY_GPIO       29
#define CCI_HOME_KEY_GPIO       51
#define CCI_SEARCH_KEY_GPIO     57
#endif
      auiStatus[0] = (unsigned int) gpio_get_value(39);
      auiStatus[1] = (unsigned int) gpio_get_value(29);
      auiStatus[2] = (unsigned int) gpio_get_value(51);
      auiStatus[3] = (unsigned int) gpio_get_value(57);

	count += sprintf(buf, "back=%d menu=%d home=%d search=%d (0:press)\n", auiStatus[0], auiStatus[1], auiStatus[2], auiStatus[3]);
	return count;
}
static DEVICE_ATTR(vk_status, 0666, virtual_key_status_show, NULL);
#endif


#ifdef ENABLE_IME_IMPROVEMENT
static ssize_t ime_threshold_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct atmel_ts_data *ts = private_ts;

	return sprintf(buf, "%d\n", ts->ime_threshold_pixel);
}

static ssize_t ime_threshold_store(struct device *dev,struct device_attribute *attr,const char *buf, size_t count)
{
	struct atmel_ts_data *ts = private_ts;
	char *ptr_data = (char *)buf;
	unsigned long val;

	val = simple_strtoul(ptr_data, NULL, 10);

	if (val >= 0 && val <= max(ts->display_width, ts->display_height))
		ts->ime_threshold_pixel = val;
	else
		ts->ime_threshold_pixel = 0;

	ts->ime_threshold[0] = ts->ime_threshold_pixel * (ts->abs_x_max - ts->abs_x_min) / ts->display_width;
	ts->ime_threshold[1] = ts->ime_threshold_pixel * (ts->abs_y_max - ts->abs_y_min) / ts->display_height;

	return count;
}

static ssize_t ime_work_area_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct atmel_ts_data *ts = private_ts;

	return sprintf(buf, "%d,%d,%d,%d\n", ts->ime_area_pixel[0],
		ts->ime_area_pixel[1], ts->ime_area_pixel[2], ts->ime_area_pixel[3]);
}

static ssize_t ime_work_area_store(struct device *dev,struct device_attribute *attr,const char *buf, size_t count)
{
	struct atmel_ts_data *ts = private_ts;
	char *ptr_data = (char *)buf;
	char *p;
	int pt_count = 0;
	unsigned long val[4];

	while ((p = strsep(&ptr_data, ","))) {
		if (!*p)
		break;

		if (pt_count >= 4)
		break;

		val[pt_count] = simple_strtoul(p, NULL, 10);

		pt_count++;
	}

	if (pt_count >= 4 && ts->display_width && ts->display_height) {
		ts->ime_area_pixel[0] = val[0]; /* Left */
		ts->ime_area_pixel[1] = val[1]; /* Right */
		ts->ime_area_pixel[2] = val[2]; /* Top */
		ts->ime_area_pixel[3] = val[3]; /* Bottom */

		if (val[0] < 0 || val[0] > ts->display_width)
			ts->ime_area_pos[0] = 0;
		else
			ts->ime_area_pos[0] = val[0] * (ts->abs_x_max - ts->abs_x_min) / ts->display_width + ts->abs_x_min;

		if (val[1] < 0 || val[1] > ts->display_width)
			ts->ime_area_pos[1] = ts->abs_x_max;
		else
			ts->ime_area_pos[1] = val[1] * (ts->abs_x_max - ts->abs_x_min) / ts->display_width + ts->abs_x_min;

		if (val[2] < 0 || val[2] > ts->display_height)
			ts->ime_area_pos[2] = 0;
		else
			ts->ime_area_pos[2] = val[2] * (ts->abs_y_max - ts->abs_y_min) / ts->display_height + ts->abs_y_min;

		if (val[3] < 0 || val[3] > ts->display_height)
			ts->ime_area_pos[3] = ts->abs_y_max;
		else
			ts->ime_area_pos[3] = val[3] * (ts->abs_y_max - ts->abs_y_min) / ts->display_height + ts->abs_y_min;
	}

	return count;
}


static int ime_report_filter(struct atmel_ts_data *ts, uint8_t *data)
{
	int dx = 0;
	int dy = 0;
	int x = data[2] << 2 | data[4] >> 6;
	int y = data[3] << 2 | (data[4] & 0x0C) >> 2;
	uint8_t report_type = data[0] - ts->finger_type;
	if (data[1] & 0x20) {
		if (report_type == ts->ime_finger_report[1]) {
			ts->ime_finger_report[1] = -1;
			return 0;
		} else if (report_type == ts->ime_finger_report[0]) {
			if (ts->ime_finger_report[1] < 0) {
				input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
				input_sync(ts->input_dev);

				ts->ime_finger_report[0] = -1;
				return 0;
			} else {
				ts->ime_finger_report[0] = ts->ime_finger_report[1];
				report_type = ts->ime_finger_report[1];
				ts->ime_finger_report[1] = -1;
			}
		}
	} else if (data[1] & 0xC0) {
		if ((x >= ts->ime_area_pos[0] && x <= ts->ime_area_pos[1]) &&
			(y >= ts->ime_area_pos[2] && y <= ts->ime_area_pos[3])) {
		if (ts->ime_finger_report[0] == report_type) {
			dx = abs(x - ts->finger_data[report_type].x);
			dy = abs(y - ts->finger_data[report_type].y);
			if (dx < ts->ime_threshold[0] && dy < ts->ime_threshold[1] && !ts->ime_move)
				return 0;
			else
				ts->ime_move = 1;
			}
		}
		ts->finger_data[report_type].x = x;
		ts->finger_data[report_type].y = y;
		ts->finger_data[report_type].w = data[5];
		ts->finger_data[report_type].z = data[6];

		if (ts->ime_finger_report[0] < 0) {
			ts->ime_finger_report[0] = report_type;
			ts->ime_move = 0;
		} else if (ts->ime_finger_report[0] != report_type &&
			(ts->ime_finger_report[1] < 0 || ts->ime_finger_report[1] == report_type)) {
			ts->ime_finger_report[1] = report_type;
			return 0;
		}
	}

	input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR,
		ts->finger_data[report_type].z);
	input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR,
		ts->finger_data[report_type].w);
	input_report_abs(ts->input_dev, ABS_MT_POSITION_X,
		ts->finger_data[report_type].x);
	input_report_abs(ts->input_dev, ABS_MT_POSITION_Y,
		ts->finger_data[report_type].y);
	input_mt_sync(ts->input_dev);
	input_sync(ts->input_dev);

	return 0;
}


/* sys/class/input/inputX/ime_threshold */
static DEVICE_ATTR(ime_threshold, 0666, ime_threshold_show,
ime_threshold_store);
static DEVICE_ATTR(ime_work_area, 0666, ime_work_area_show,
ime_work_area_store);
#endif


static struct kobject *android_touch_kobj;

static int atmel_touch_sysfs_init(void)
{
	int ret;
	android_touch_kobj = kobject_create_and_add("touch", NULL);
	if (android_touch_kobj == NULL) {
		printk(KERN_ERR "%s: subsystem_register failed\n", __func__);
		ret = -ENOMEM;
		return ret;
	}
	ret = sysfs_create_file(android_touch_kobj, &dev_attr_gpio.attr);
	if (ret) {
		printk(KERN_ERR "%s: sysfs_create_file failed\n", __func__);
		return ret;
	}
	ret = sysfs_create_file(android_touch_kobj, &dev_attr_vendor.attr);
	if (ret) {
		printk(KERN_ERR "%s: sysfs_create_file failed\n", __func__);
		return ret;
	}
	atmel_reg_addr = 0;
	ret = sysfs_create_file(android_touch_kobj, &dev_attr_register.attr);
	if (ret) {
		printk(KERN_ERR "%s: sysfs_create_file failed\n", __func__);
		return ret;
	}
	ret = sysfs_create_file(android_touch_kobj, &dev_attr_regdump.attr);
	if (ret) {
		printk(KERN_ERR "%s: sysfs_create_file failed\n", __func__);
		return ret;
	}
	ret = sysfs_create_file(android_touch_kobj, &dev_attr_debug_level.attr);
	if (ret) {
		printk(KERN_ERR "%s: sysfs_create_file failed\n", __func__);
		return ret;
	}
	ret = sysfs_create_file(android_touch_kobj, &dev_attr_regtable.attr);
	if (ret) {
		printk(KERN_ERR "%s: sysfs_create_file failed\n", __func__);
		return ret;
	}
	//Henry_lin, 20110531, [BugID 153] Implement testing AT command for keypad.
	ret = sysfs_create_file(android_touch_kobj, &dev_attr_at_cmd.attr);
	if (ret) {
		printk(KERN_ERR "%s: sysfs_create_file failed\n", __func__);
		return ret;
	}
	//Henry_lin


	//Henry_lin, 20110815, [BugID 641] Enable/disable Home key wake up system ability.
	ret = sysfs_create_file(android_touch_kobj, &dev_attr_home_key_wake_en.attr);
	if (ret) {
		printk(KERN_ERR "%s: sysfs_create_file failed\n", __func__);
		return ret;
	}
	//Henry_lin


	//Henry_lin, 20110829, [BugID 712] Enable/disable virtual key reset feature.
	ret = sysfs_create_file(android_touch_kobj, &dev_attr_virtual_key_reset_en.attr);
	if (ret) {
		printk(KERN_ERR "%s: sysfs_create_file failed\n", __func__);
		return ret;
	}
	//Henry_lin


#if(HENRY_DEBUG_VK)
	ret = sysfs_create_file(android_touch_kobj, &dev_attr_vk_status.attr);
	if (ret) {
		printk(KERN_ERR "%s: sysfs_create_file failed\n", __func__);
		return ret;
	}
#endif

	return 0;
}

static void atmel_touch_sysfs_deinit(void)
{

#if(HENRY_DEBUG_VK)
	sysfs_remove_file(android_touch_kobj, &dev_attr_vk_status.attr);
#endif

	//Henry_lin, 20110829, [BugID 712] Enable/disable virtual key reset feature.
	sysfs_remove_file(android_touch_kobj, &dev_attr_virtual_key_reset_en.attr);
	//Henry_lin

	//Henry_lin, 20110815, [BugID 641] Enable/disable Home key wake up system ability.
	sysfs_remove_file(android_touch_kobj, &dev_attr_home_key_wake_en.attr);
	//Henry_lin

	//Henry_lin, 20110531, [BugID 153] Implement testing AT command for keypad.
	sysfs_remove_file(android_touch_kobj, &dev_attr_at_cmd.attr);
	//Henry_lin
	sysfs_remove_file(android_touch_kobj, &dev_attr_regtable.attr);
	sysfs_remove_file(android_touch_kobj, &dev_attr_regdump.attr);
	sysfs_remove_file(android_touch_kobj, &dev_attr_register.attr);
	sysfs_remove_file(android_touch_kobj, &dev_attr_vendor.attr);
	sysfs_remove_file(android_touch_kobj, &dev_attr_gpio.attr);
	kobject_del(android_touch_kobj);
}

#endif
static void check_calibration(struct atmel_ts_data*ts)
{
	uint8_t data[82];
	uint8_t loop_i, loop_j, x_limit = 0, check_mask, tch_ch = 0, atch_ch = 0;

	memset(data, 0xFF, sizeof(data));
	i2c_atmel_write_byte_data(ts->client,
		get_object_address(ts, GEN_COMMANDPROCESSOR_T6) + 5, 0xF3); //0xF3??

	for (loop_i = 0; !(data[0] == 0xF3 && data[1] == 0x00) && loop_i < 10; loop_i++) {
		msleep(5);
		i2c_atmel_read(ts->client, get_object_address(ts, DIAGNOSTIC_T37), data, 2);
	}

	if (loop_i == 10)
		printk(KERN_ERR "%s: Diag data not ready\n", __func__);

	i2c_atmel_read(ts->client, get_object_address(ts, DIAGNOSTIC_T37), data, 82);
	if (data[0] == 0xF3 && data[1] == 0x00) {
		x_limit = 16 + ts->config_setting[0].config_T28[2];
		x_limit = x_limit << 1;

		for (loop_i = 0; loop_i < x_limit; loop_i += 2) {
			for (loop_j = 0; loop_j < 8; loop_j++) {
				check_mask = 1 << loop_j;
				if (data[2 + loop_i] & check_mask)
					tch_ch++;
				if (data[3 + loop_i] & check_mask)
					tch_ch++;
				if (data[42 + loop_i] & check_mask)
					atch_ch++;
				if (data[43 + loop_i] & check_mask)
					atch_ch++;
			}
		}
	}
	i2c_atmel_write_byte_data(ts->client,
	get_object_address(ts, GEN_COMMANDPROCESSOR_T6) + 5, 0x01);

        //determinat to do recalibration or only set value of antitouch calibration suspend time/threshhold
	if (tch_ch && (atch_ch == 0)) {
		if (jiffies > (ts->timestamp + HZ/2) && (ts->calibration_confirm == 1)) {
			ts->calibration_confirm = 2;
			printk(KERN_INFO "%s: calibration confirm\n", __func__);
			i2c_atmel_write_byte_data(ts->client,
				get_object_address(ts, GEN_ACQUISITIONCONFIG_T8) + 6,
				ts->config_setting[ts->status].config_T8[6]);
			i2c_atmel_write_byte_data(ts->client,
				get_object_address(ts, GEN_ACQUISITIONCONFIG_T8) + 7,
				ts->config_setting[ts->status].config_T8[7]);
		}
		if (ts->calibration_confirm < 2)
			ts->calibration_confirm = 1;
		ts->timestamp = jiffies;
	} else if ((tch_ch - 25) <= atch_ch && (tch_ch || atch_ch)) {
		ts->calibration_confirm = 0;
		i2c_atmel_write_byte_data(ts->client,
			get_object_address(ts, GEN_COMMANDPROCESSOR_T6) + 2, 0x55);
	}
}

//static void s_vKeyArray(int iKeyId)
void s_vKeyArray(int iKeyId)
{
#if defined CONFIG_KEYBOARD_PMIC8058
	if (tskpdev == NULL) {
		printk(KERN_ERR "[%s] tskpdev == NULL\n", __FUNCTION__);
		return;
	}

	if (iKeyId == -1) {
		if (g_caButton[BACK_BTN] == 1) {
			//printk(KERN_INFO "[%s] Release <BACK> Key\n", __FUNCTION__);
			input_report_key(tskpdev, KB_BACK, 0);
			input_sync(tskpdev);
			g_caButton[BACK_BTN] = 0;
		} else if (g_caButton[MENU_BTN] == 1) {
			//printk(KERN_INFO "[%s] Release <MENU> Key\n", __FUNCTION__);
			input_report_key(tskpdev, KB_MENU, 0);
			input_sync(tskpdev);
			g_caButton[MENU_BTN] = 0;
		} else if (g_caButton[HOME_BTN] == 1) {
			//printk(KERN_INFO "[%s] Release <HOME> Key\n", __FUNCTION__);
			input_report_key(tskpdev, KB_HOME1, 0);
			input_sync(tskpdev);
			g_caButton[HOME_BTN] = 0;
		}
            #if 1   //for DA80
		else if (g_caButton[SEARCH_BTN] == 1) {
			//printk(KERN_INFO "[%s] Release <SEARCH> Key\n", __FUNCTION__);
			input_report_key(tskpdev, KB_SEARCH, 0);
			input_sync(tskpdev);
			g_caButton[SEARCH_BTN] = 0;
		}
            #endif
	} 
	else {
		if (iKeyId == 0x2) {
			//printk(KERN_INFO "[%s] Key(%#x) is <BACK>\n", __FUNCTION__, (char)iKeyId);
			input_report_key(tskpdev, KB_BACK, 1);
			input_sync(tskpdev);
			g_caButton[BACK_BTN] = 1;
		} else if (iKeyId == 0x1) {
			//printk(KERN_INFO "[%s] Key(%#x) is <MENU>\n", __FUNCTION__, (char)iKeyId);
			input_report_key(tskpdev, KB_MENU, 1);
			input_sync(tskpdev);
			g_caButton[MENU_BTN] = 1;
        	} else if (iKeyId == 0x4) {
			//printk(KERN_INFO "[%s] Key(%#x) is <HOME>\n", __FUNCTION__, (char)iKeyId);
			input_report_key(tskpdev, KB_HOME1, 1);
			input_sync(tskpdev);
			g_caButton[HOME_BTN] = 1;
		}
            #if 1   //for DA80
        	else if (iKeyId == 0x8) {
			//printk(KERN_INFO "[%s] Key(%#x) is <SEARCH>\n", __FUNCTION__, (char)iKeyId);
			input_report_key(tskpdev, KB_SEARCH, 1);
			input_sync(tskpdev);
			g_caButton[SEARCH_BTN] = 1;
		}
            #endif
	}    
#endif
}
EXPORT_SYMBOL(s_vKeyArray);


static void atmel_ts_work_func(struct work_struct *work)
{
	int ret;
	struct atmel_ts_data *ts = container_of(work, struct atmel_ts_data, work);
	uint8_t data[ts->finger_support * 9];
	uint8_t loop_i, loop_j, report_type = 0, msg_num, msg_byte_num = 8, finger_report;
	static uint32_t touch_down = 0;
	#if 0
	static u32 u32StartTime, u32EndTime;
	struct timeval sTimeTmp1, sTimeTmp2;
	static int iCnt = 0;
	#endif

	/*
	if (ts_active_time - jiffies > 2000)
		ts_active_time = INITIAL_JIFFIES;//2010/07/20 Jiahan_Li [TK11887][BugID 544]Specifications for preventing malfunctions.
	if (key_active_time - jiffies > 2000)
		key_active_time = INITIAL_JIFFIES;//2010/07/20 Jiahan_Li [TK11887][BugID 544]Specifications for preventing malfunctions.
	*/
#ifdef ENABLE_IME_IMPROVEMENT
	msg_num = (ts->finger_count && ts->id->version >= 0x15 && ts->ime_threshold_pixel)
				? ts->finger_count : 1;
#else
        //Henry_lin, 20110901, [BugID 786]  Fix multitouch bug.
	msg_num = 1;
#endif

    //PrintTip("[%s] msg_num = %d, ts->finger_count=%d\n", __FUNCTION__, msg_num, ts->finger_count);

	ret = i2c_atmel_read(ts->client, get_object_address(ts,
		GEN_MESSAGEPROCESSOR_T5), data, msg_num * 9 - 2);
#if 0
	ts->debug_log_level |= 2;
#endif

	//if (ts->debug_log_level & 0x1) {
		//for (loop_i = 0; loop_i < msg_num * 9 - 2; loop_i++)
			//printk("0x%2.2X ", data[loop_i]);
		//printk("\n");
	//}

	if (ts->id->version >= 0x15) 
	{
		for (loop_i = 0; loop_i < msg_num; loop_i++) 
		{
			report_type = data[loop_i * 9] - ts->finger_type;
			if (report_type < ts->finger_support) 
			{
				if (ts->calibration_confirm < 2 && ts->id->version >= 0x16)
					check_calibration(ts);
#ifdef ENABLE_IME_IMPROVEMENT
				if (ts->ime_threshold_pixel > 0) {
					ime_report_filter(ts, &data[0]);
					break;
				}
#endif
				ts->finger_data[report_type].x = data[loop_i * 9 + 2] << 2 | data[loop_i * 9 + 4] >> 6;
				ts->finger_data[report_type].y = data[loop_i * 9 + 3] << 2 | (data[loop_i * 9 + 4] & 0x0C) >> 2;
				ts->finger_data[report_type].w = data[loop_i * 9 + 5];
				ts->finger_data[report_type].z = data[loop_i * 9 + 6];
				if (data[loop_i * 9 + 1] & 0x20) 
				{ //0x20 is RELEASE bit
                                #if 0
                                do_gettimeofday(&sTimeTmp2);
                                u32EndTime = (sTimeTmp2.tv_sec * 1000) + (sTimeTmp2.tv_usec / 1000);
                                PrintTip("[%s] End time  = %d, iCnt=%d \n", __FUNCTION__, u32EndTime, iCnt);
                                PrintTip("[%s] Elapse  = %d, one event =%d ms\n", __FUNCTION__, u32EndTime-u32StartTime, (u32EndTime-u32StartTime)/iCnt);
                                #endif	
					if ((ts->grip_suppression >> report_type) & 1)
						ts->grip_suppression &= ~(1 << report_type);
					if (((ts->finger_pressed >> report_type) & 1) == 1) {
						ts->finger_count--;
						ts->finger_pressed &= ~(1 << report_type);
						if (!ts->first_pressed) {
							if (!ts->finger_count)
								ts->first_pressed = 1;
							printk(KERN_INFO "E%d@%d,%d\n", report_type + 1,
								ts->finger_data[report_type].x, ts->finger_data[report_type].y);
						}
					}
				} 
				else if ((data[loop_i * 9 + 1] & 0xC0) && (((ts->finger_pressed >> report_type) & 1) == 0)) 
				{ //0xC0 are DETECT and PRESS bit
					if (ts->filter_level[0]) {
						if (ts->finger_data[report_type].x < ts->filter_level[0] || ts->finger_data[report_type].x > ts->filter_level[3])
							ts->grip_suppression |= 1 << report_type;
						else if ((ts->finger_data[report_type].x < ts->filter_level[1] || ts->finger_data[report_type].x > ts->filter_level[2])
								&& ((ts->grip_suppression >> report_type) & 1))
							ts->grip_suppression |= 1 << report_type;
						else if (ts->finger_data[report_type].x > ts->filter_level[1] && ts->finger_data[report_type].x < ts->filter_level[2])
							ts->grip_suppression &= ~(1 << report_type);
					}
					if (((ts->grip_suppression >> report_type) & 1) == 0) {
						if (!ts->first_pressed)
							printk(KERN_INFO "S%d@%d,%d\n", report_type + 1,
								ts->finger_data[report_type].x, ts->finger_data[report_type].y);
						ts->finger_count++;
						ts->finger_pressed |= 1 << report_type;
						#if 0
						do_gettimeofday(&sTimeTmp1);
						u32StartTime = (sTimeTmp1.tv_sec * 1000) + (sTimeTmp1.tv_usec / 1000);
						iCnt = 0;
						PrintTip("[%s] Start time  = %d, iCnt=%d \n", __FUNCTION__, u32StartTime, iCnt);
						#endif	

					}
				}
			} 
			else 
			{
                                //No message from T9 object
				if (data[loop_i * 9] == get_report_ids_size(ts, GEN_COMMANDPROCESSOR_T6)) {
					#if 0   //henry mask
					printk(KERN_INFO "Touch Status: ");
					#endif
					msg_byte_num = 5;
				} else if (data[loop_i * 9] == get_report_ids_size(ts, PROCI_GRIPFACESUPPRESSION_T20)) {
					if (ts->calibration_confirm < 2 && ts->id->version >= 0x16)
						check_calibration(ts);
					ts->face_suppression = data[loop_i * 9 + 1];
					printk(KERN_INFO "Touch Face suppression %s: ",
						ts->face_suppression ? "Active" : "Inactive");
						msg_byte_num = 2;
				} else if (data[loop_i * 9] == get_report_ids_size(ts, PROCG_NOISESUPPRESSION_T22)) {
					if (data[loop_i * 9 + 1] == 0x10) // reduce message print
						msg_byte_num = 0;
					else {
				                //filters the acquisition data received from sensor
						printk(KERN_INFO "Touch Noise suppression: ");
						msg_byte_num = 4;
						if (ts->status && data[loop_i * 9 + 2] >= ts->GCAF_sample) {
							i2c_atmel_write_byte_data(ts->client,
								get_object_address(ts, GEN_POWERCONFIG_T7), 0x08);
							i2c_atmel_write_byte_data(ts->client,
								get_object_address(ts, GEN_POWERCONFIG_T7) + 1, 0x08);
							for (loop_j = 0; loop_j < 5; loop_j++) {
								if (ts->GCAF_sample < ts->GCAF_level[loop_j]) {
									ts->GCAF_sample = ts->GCAF_level[loop_j];
									break;
								}
							}
							if (loop_j == 5)
								ts->GCAF_sample += 24;
							if (ts->GCAF_sample >= 63) {
								ts->GCAF_sample = 63;
								i2c_atmel_write_byte_data(ts->client,
									get_object_address(ts, PROCG_NOISESUPPRESSION_T22) + 8,
									ts->config_setting[1].config[1]);
								i2c_atmel_write_byte_data(ts->client,
									get_object_address(ts, GEN_ACQUISITIONCONFIG_T8) + 7, 0x1);
							}
							i2c_atmel_write_byte_data(ts->client,
								get_object_address(ts, SPT_CTECONFIG_T28) + 4, ts->GCAF_sample);
						}
						if (data[loop_i * 9 + 1] & 0x0C && ts->GCAF_sample == 63) {
							//ts->noisethr += 30;
							//if (ts->noisethr >= 255)
							//	ts->noisethr = 255;
							if (ts->noisethr + 30 > 255)
								ts->noisethr = 255;
							else
								ts->noisethr += 30;
							i2c_atmel_write_byte_data(ts->client,
								get_object_address(ts, PROCG_NOISESUPPRESSION_T22) + 8, ts->noisethr);
						}
					}
				} 
				else if (data[loop_i * 9] == get_report_ids_size(ts, TOUCH_KEYARRAY_T15)) 
				{
					msg_byte_num = 6;
					if (0){//time_after(key_active_time, jiffies)) {
						if ((data[loop_i * 9 + 1] & 0x80) == 0x00)
							s_vKeyArray(-1);
						goto Exit;
					} else {
						if ((data[loop_i * 9 + 1] & 0x80) == 0x80) {
							//printk(KERN_INFO "[%s] Press Key-array\n", __FUNCTION__);
							s_vKeyArray(data[loop_i * 9 + 2]);
						} else if ((data[loop_i * 9 + 1] & 0x80) == 0x00) {
							//printk(KERN_INFO "[%s] Release Key-array\n", __FUNCTION__);
							s_vKeyArray(-1);
						}
					}
				}
                    
        		if (data[loop_i * 9] != 0xFF) {
                    //if (ts->debug_log_level & 0x4) 
           			//    Printhh("[%s] Not handle message from object (%#x)\n", __FUNCTION__, data[loop_i * 9]);
       				if (ts->debug_log_level & 0x2)
       				{
               			for (loop_j = 0; loop_j < msg_byte_num; loop_j++)
               				printk("0x%2.2X ", data[loop_i * 9 + loop_j]);
               			if (msg_byte_num)
               				printk("\n");
       				}
       			}
			}
			
		if (0){//time_after(ts_active_time, jiffies)) {
				if (touch_down) {
					input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
					touch_down = 0;
				}
				goto Exit;
			} else {
				if (loop_i == msg_num - 1) 
				{
					if (!ts->finger_count || ts->face_suppression) 
					{
					    #ifdef D_CCI_TOUCH_KEY
                                    if(isTouchKeyPressed  ){
                                        printk("touch key release2" );   
                                        s_vKeyArray( -1);
                                        isTouchKeyPressed = 0;
                                    }
                                    #endif   
                                    ts->finger_pressed = 0;
					    ts->finger_count = 0;
					    input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
					    touch_down = 0;
					     if (ts->debug_log_level & 0x2)
							printk(KERN_INFO "Finger leave\n");
					 } 
					 else 
					 {    
                                      int isTouchKeyPressEvt = 0;                 
						for (loop_i = 0, finger_report = 0; loop_i < ts->finger_support; loop_i++) 
						{
							if (((ts->finger_pressed >> loop_i) & 1) == 1) 
							{
								if(handleTouchKeyEvent(	ts->finger_data,loop_i )!=  D_IS_EVENT_HANDLED   )
                                                   {  
                                                       finger_report++;
								    input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR,
									    ts->finger_data[loop_i].z);
								    input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR,
									    ts->finger_data[loop_i].w);
								    input_report_abs(ts->input_dev, ABS_MT_POSITION_X,
									    ts->finger_data[loop_i].x);
								    input_report_abs(ts->input_dev, ABS_MT_POSITION_Y,
									    ts->finger_data[loop_i].y);
								    input_mt_sync(ts->input_dev);
								    #if 0
								    iCnt++;
								    #endif	

								    if (ts->debug_log_level & 0x2)
									    printk(KERN_INFO "Finger %d=> X:%d, Y:%d w:%d, z:%d, F:%d\n",
										    loop_i + 1, ts->finger_data[loop_i].x,
										    ts->finger_data[loop_i].y, ts->finger_data[loop_i].w,
										    ts->finger_data[loop_i].z, ts->finger_count);
                                                    }
                                                    else  {
                                                      isTouchKeyPressEvt = 1;
                                                    }   
							}
						}
                                        #ifdef D_CCI_TOUCH_KEY
                                        if((isTouchKeyPressEvt == 0) && isTouchKeyPressed  ){
                                          printk("touch key release" );   
                                          s_vKeyArray( -1);
                                          isTouchKeyPressed = 0;
                                        }
                                        #endif   
                                        if(finger_report )   
                                            touch_down = 1;
                                        else 
                                            goto Exit;
					}
					input_sync(ts->input_dev);
				}
			}
		}
	}
Exit:
	enable_irq(ts->client->irq);
}


static irqreturn_t atmel_ts_irq_handler(int irq, void *dev_id)
{
	struct atmel_ts_data *ts = dev_id;
	static int ircount = 0;
        if( 0)
	{
	  ircount++;
          if(ircount>10)
	    { ircount = 0;
            printk(KERN_INFO "%s: enter\n", __func__);
	    }
	}
	disable_irq_nosync(ts->client->irq);
	queue_work(ts->atmel_wq, &ts->work);    //atmel_ts_work_func
	return IRQ_HANDLED;
}

static int atmel_ts_probe(struct i2c_client *client,const struct i2c_device_id *id)
{
	struct atmel_ts_data *ts;
	struct atmel_i2c_platform_data *pdata;
	int ret = 0, i = 0, intr = 0;
		int rc;
	uint8_t loop_i;
	struct i2c_msg msg[2];
	uint8_t data[16];
	uint8_t type_count = 0, CRC_check = 0;
//	extern int cci_config_input(int gpio);

        
	Printhh("[%s] enter...\n", __FUNCTION__);
#ifndef PMIC_INT_GPIO
       	printk("gpio_tlmm_config - for gpio TS_CHG = %d\n",TS_CHG);
	if (gpio_tlmm_config(GPIO_CFG(TS_CHG, 0, GPIO_CFG_INPUT,GPIO_CFG_NO_PULL,GPIO_CFG_2MA),GPIO_CFG_ENABLE)) {
				printk(KERN_ERR "%s: Err: Config GPIO-84 \n",	__func__);
	}

	rc = gpio_request(TS_CHG, "touch_int");
	if (rc < 0)
		printk(KERN_ERR"fail to request gpio for touch_int! error=%d\n", rc);
	#endif
	rc = gpio_direction_input(TS_CHG);
	if (rc <0)
                printk(KERN_ERR "%s: fail to set input = %d\n",__func__,rc );
	 //	   msleep(5);
		     
		   //  gpio_set_value(TS_CHG, 0) ;
	 //     printk("%s: TS_CHG = %d\n",__func__,gpio_get_value(TS_CHG) );
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		printk(KERN_ERR"%s: need I2C_FUNC_I2C\n", __func__);
		ret = -ENODEV;
		goto err_check_functionality_failed;
	}

	ts = kzalloc(sizeof(struct atmel_ts_data), GFP_KERNEL);
	if (ts == NULL) {
		printk(KERN_ERR"%s: allocate atmel_ts_data failed\n", __func__);
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}

	ts->atmel_wq = create_singlethread_workqueue("atmel_wq");
	if (!ts->atmel_wq) {
		printk(KERN_ERR"%s: create workqueue failed\n", __func__);
		ret = -ENOMEM;
		goto err_create_wq_failed;
	}
	INIT_WORK(&ts->work, atmel_ts_work_func);
	
        #ifdef PMIC_INT_GPIO
	atmel_platform_data.gpio_irq = TOUCH_GPIO;
        client->irq =  PM8058_GPIO_IRQ(PM8058_IRQ_BASE, TOUCH_GPIO);//MSM_GPIO_TO_INT(TS_CHG);
	#else
	atmel_platform_data.gpio_irq = TS_CHG;
        client->irq =  gpio_to_irq(TS_CHG);
        //Henry_lin, 20110717, [BugID 476]  Supply Atmel mxt224E support.
        atmel_platform_data_224E.gpio_irq = TS_CHG;
        #endif
        client->dev.platform_data = &atmel_platform_data;   // default for 224, update later
	
	ts->client = client;
	i2c_set_clientdata(client, ts);
	pdata = client->dev.platform_data;

	if (pdata) {
		ts->power = pdata->power;
		intr = pdata->gpio_irq;
	}
	if (ts->power) {
		ret = ts->power(1);
		msleep(2);
		if (ret < 0) {
			printk(KERN_ERR "%s:power on failed\n", __func__);
			goto err_power_failed;
		}
	}

	for (loop_i = 0; loop_i < 10; loop_i++) {
	  	if (!gpio_get_value(intr))
	  	 break;
		msleep(10);
	}

	if (loop_i == 10)
		printk(KERN_ERR "No Messages\n");

	/* read message*/
	msg[0].addr = ts->client->addr;
	msg[0].flags = I2C_M_RD;
	msg[0].len = 7;
	msg[0].buf = data;
	ret = i2c_transfer(client->adapter, msg, 1);
        
	if (ret < 0) {	       
        	printk(KERN_INFO "No Atmel chip inside\n");
    		goto err_detect_failed;
	}
	printk(KERN_INFO "Touch: 0x%2.2X 0x%2.2X 0x%2.2X 0x%2.2X 0x%2.2X 0x%2.2X 0x%2.2X\n",
		data[0], data[1], data[2], data[3], data[4], data[5], data[6]);

if (data[1] & 0x24) {
		printk(KERN_INFO "atmel_ts_probe(): init err: %x\n", data[1]);
		goto err_detect_failed;
	}
	else {
		for (loop_i = 0; loop_i < 10; loop_i++) {
			if (gpio_get_value(intr)) {
				printk(KERN_INFO "Touch: No more message\n");
				break;
			}
			ret = i2c_transfer(client->adapter, msg, 1);
			printk(KERN_INFO "Touch: 0x%2.2X 0x%2.2X 0x%2.2X 0x%2.2X 0x%2.2X 0x%2.2X 0x%2.2X\n",
				data[0], data[1], data[2], data[3], data[4], data[5], data[6]);
			msleep(10);
		}
	}

	/* Read the info block data. */
	ts->id = kzalloc(sizeof(struct info_id_t), GFP_KERNEL);
	if (ts->id == NULL) {
		printk(KERN_ERR"%s: allocate info_id_t failed\n", __func__);
		goto err_alloc_failed;
	}
	ret = i2c_atmel_read(client, 0x00, data, 7);

	//Henry_lin, 20110717, [BugID 476]  Supply Atmel mxt224E support.
	PrintTip("[%s] 224 chip information block:\n", __FUNCTION__);
	PrintTip("[%s] family_id = %#x\n", __FUNCTION__, data[0]);
	PrintTip("[%s] variant_id = %#x\n", __FUNCTION__, data[1]);
	PrintTip("[%s] version = %#x\n", __FUNCTION__, data[2]);
	PrintTip("[%s] build = %#x\n", __FUNCTION__, data[3]);
	PrintTip("[%s] num_declared_objects = %#x\n", __FUNCTION__, data[6]);


	ts->id->family_id = data[0];
	ts->id->variant_id = data[1];
	if (ts->id->family_id == 0x80 && ts->id->variant_id == 0x10){
		ts->id->version = data[2] + 6;
	}
	//Henry_lin, 20110717, [BugID 476]  Supply Atmel mxt224E support.
	else if (ts->id->family_id == 0x81 && ts->id->variant_id == 0x01)
	{
		PrintTip("[%s] Is 224E!!\n", __FUNCTION__);
		ts->id->version = data[2] + 6;
		client->dev.platform_data = &atmel_platform_data_224E;   // for 224E data
		pdata = client->dev.platform_data;
		//PrintTip("[%s] client->dev.platform_data address is %#x\n", __FUNCTION__, (unsigned int)client->dev.platform_data);
	}
	else
		ts->id->version = data[2];

	ts->id->build = data[3];
	ts->id->matrix_x_size = data[4];
	ts->id->matrix_y_size = data[5];
	ts->id->num_declared_objects = data[6];

	printk(KERN_INFO "info block: 0x%2.2X 0x%2.2X 0x%2.2X 0x%2.2X 0x%2.2X 0x%2.2X 0x%2.2X\n",
		ts->id->family_id, ts->id->variant_id,
		ts->id->version, ts->id->build,
		ts->id->matrix_x_size, ts->id->matrix_y_size,
		ts->id->num_declared_objects);

	// Read object table.
	ts->object_table = kzalloc(sizeof(struct object_t)*ts->id->num_declared_objects, GFP_KERNEL);
	if (ts->object_table == NULL) {
		printk(KERN_ERR"%s: allocate object_table failed\n", __func__);
		goto err_alloc_failed;
	}
	for (i = 0; i < ts->id->num_declared_objects; i++) {
		ret = i2c_atmel_read(client, i * 6 + 0x07, data, 6);
		ts->object_table[i].object_type = data[0];
		ts->object_table[i].i2c_address = data[1] | data[2] << 8;
		ts->object_table[i].size = data[3] + 1;
		ts->object_table[i].instances = data[4];
		ts->object_table[i].num_report_ids = data[5];
		if (data[5]) {
			ts->object_table[i].report_ids = type_count + 1;
			type_count += data[5];
		}
		if (data[0] == 9)
			ts->finger_type = ts->object_table[i].report_ids;

		printk(KERN_INFO "Type: %2.2X, Start: %4.4X, Size: %2X, Instance: %2X, RD#: %2X, %2X\n",
			ts->object_table[i].object_type , ts->object_table[i].i2c_address,
			ts->object_table[i].size, ts->object_table[i].instances,
			ts->object_table[i].num_report_ids, ts->object_table[i].report_ids);
	}

	if (pdata) {
        	//printk(KERN_INFO "[%s] pdata->version = %#x  ts->id->version = %#x\n", __FUNCTION__, pdata->version, ts->id->version);
		#if 0   // we only has one config, pdata++ maybe refer an invalid address
		while (pdata->version > ts->id->version)
			pdata++;
		#endif

		if (pdata->source) {
			i2c_atmel_write_byte_data(client,
				get_object_address(ts, SPT_GPIOPWM_T19), 0x7);
			for (loop_i = 0; loop_i < 10; loop_i++) {
				if (!gpio_get_value(intr))
					break;
				msleep(10);
			}
			if (loop_i == 10)
				printk(KERN_ERR "No Messages when check source\n");
			for (loop_i = 0; loop_i < 10; loop_i++) {
				i2c_atmel_read(ts->client, get_object_address(ts,
					GEN_MESSAGEPROCESSOR_T5), data, 2);
				if (data[0] == get_report_ids_size(ts, SPT_GPIOPWM_T19)) {
					while ((data[1] >> 3) != pdata->source)
						pdata++;
					break;
				}
			}
		}

		ts->finger_support = pdata->config_T9[14];  // =5 or ...
		printk(KERN_INFO"finger_type: %d, max finger: %d\n", ts->finger_type, ts->finger_support);

		/* infoamtion block CRC check */
		if (pdata->object_crc[0]) {
			ret = i2c_atmel_write_byte_data(client,
					get_object_address(ts, GEN_COMMANDPROCESSOR_T6) + 2, 0x55);
			msleep(32);
			for (loop_i = 0; loop_i < 10; loop_i++) {
				ret = i2c_atmel_read(ts->client, get_object_address(ts,
						GEN_MESSAGEPROCESSOR_T5), data, 9);
				if (data[0] == get_report_ids_size(ts, GEN_COMMANDPROCESSOR_T6))    //henry : data[0] == 6 ??
					break;
				msleep(5);
			}
			for (loop_i = 0; loop_i < 3; loop_i++) {
				if (pdata->object_crc[loop_i] != data[loop_i + 2]) {
					printk(KERN_ERR"CRC Error: my config:%x, chip config:%x\n", pdata->object_crc[loop_i], data[loop_i + 2]);
					break;
				}
			}
			printk(KERN_INFO "[%s] pdata->object_crc[0-2] = %#x %#x %#x\n", __FUNCTION__, pdata->object_crc[0], pdata->object_crc[1], pdata->object_crc[2]);
			printk(KERN_INFO "[%s] new crc data[2-4] = %#x %#x %#x\n", __FUNCTION__, data[2], data[3], data[4]);
			if (loop_i == 3) {
				printk(KERN_INFO "CRC passed: ");
				for (loop_i = 0; loop_i < 3; loop_i++)
					printk("0x%2.2X ", pdata->object_crc[loop_i]);
				printk("\n");
				CRC_check = 1;
			}
		}

		//assign value from pdata to ts
		ts->abs_x_min = pdata->abs_x_min;
		ts->abs_x_max = pdata->abs_x_max;
		ts->abs_y_min = pdata->abs_y_min;
		ts->abs_y_max = pdata->abs_y_max;
		ts->abs_pressure_min = pdata->abs_pressure_min;
		ts->abs_pressure_max = pdata->abs_pressure_max;
		ts->abs_width_min = pdata->abs_width_min;
		ts->abs_width_max = pdata->abs_width_max;
		ts->GCAF_level = pdata->GCAF_level;

		#if 0
		printk(KERN_INFO "GCAF_level: %d, %d, %d, %d, %d\n",
				ts->GCAF_level[0], ts->GCAF_level[1], ts->GCAF_level[2],
				ts->GCAF_level[3], ts->GCAF_level[4]);
		#endif
		ts->filter_level = pdata->filter_level; //TBD??
		#if 0
		printk(KERN_INFO "filter_level: %d, %d, %d, %d\n",
				ts->filter_level[0], ts->filter_level[1], ts->filter_level[2], ts->filter_level[3]);
		#endif
#ifdef ENABLE_IME_IMPROVEMENT
		ts->display_width = pdata->display_width;
		ts->display_height = pdata->display_height;
		if (!ts->display_width || !ts->display_height)
			ts->display_width = ts->display_height = 1;
#endif
		ts->config_setting[0].config_T7 = ts->config_setting[1].config_T7 = pdata->config_T7;
		ts->config_setting[0].config_T8 = ts->config_setting[1].config_T8 = pdata->config_T8;
		ts->config_setting[0].config_T9 = pdata->config_T9;
		ts->config_setting[0].config_T22 = pdata->config_T22;
		ts->config_setting[0].config_T28 = pdata->config_T28;

		if (pdata->cable_config[0]) {
			ts->config_setting[0].config[0] = pdata->config_T9[7];
			ts->config_setting[0].config[1] = pdata->config_T22[8];
			ts->config_setting[0].config[2] = pdata->config_T28[3];
			ts->config_setting[0].config[3] = pdata->config_T28[4];
			for (loop_i = 0; loop_i < 4; loop_i++)
				ts->config_setting[1].config[loop_i] = pdata->cable_config[loop_i];
			ts->GCAF_sample = ts->config_setting[1].config[3];
			ts->noisethr = pdata->cable_config[1];
		} else {
			if (pdata->cable_config_T7[0])
				ts->config_setting[1].config_T7 = pdata->cable_config_T7;
			if (pdata->cable_config_T8[0])
				ts->config_setting[1].config_T8 = pdata->cable_config_T8;
			if (pdata->cable_config_T9[0]) {
				ts->config_setting[1].config_T9 = pdata->cable_config_T9;
				ts->config_setting[1].config_T22 = pdata->cable_config_T22;
				ts->config_setting[1].config_T28 = pdata->cable_config_T28;
				ts->GCAF_sample = ts->config_setting[ts->status].config_T28[4];
			}
		}
        
		//printk(KERN_INFO"[%s] ts->status = %d\n", __FUNCTION__, ts->status);
		if (!CRC_check) {
			// write default configuration to register
			printk(KERN_INFO "Touch: Config reload\n");

			i2c_atmel_write(ts->client, get_object_address(ts, SPT_CTECONFIG_T28),
				pdata->config_T28, get_object_size(ts, SPT_CTECONFIG_T28));
            
			ret = i2c_atmel_write_byte_data(client, get_object_address(ts, GEN_COMMANDPROCESSOR_T6) + 1, 0x55);
			msleep(10);

			ret = i2c_atmel_write_byte_data(client, get_object_address(ts, GEN_COMMANDPROCESSOR_T6), 0x11);
			msleep(64);

			i2c_atmel_write(ts->client, get_object_address(ts, GEN_COMMANDPROCESSOR_T6),
				pdata->config_T6, get_object_size(ts, GEN_COMMANDPROCESSOR_T6));
			i2c_atmel_write(ts->client, get_object_address(ts, GEN_POWERCONFIG_T7),
				pdata->config_T7, get_object_size(ts, GEN_POWERCONFIG_T7));
			i2c_atmel_write(ts->client, get_object_address(ts, GEN_ACQUISITIONCONFIG_T8),
				pdata->config_T8, get_object_size(ts, GEN_ACQUISITIONCONFIG_T8));
			i2c_atmel_write(ts->client, get_object_address(ts, TOUCH_MULTITOUCHSCREEN_T9),
				pdata->config_T9, get_object_size(ts, TOUCH_MULTITOUCHSCREEN_T9));
			i2c_atmel_write(ts->client, get_object_address(ts, TOUCH_KEYARRAY_T15),
				pdata->config_T15, get_object_size(ts, TOUCH_KEYARRAY_T15));
			i2c_atmel_write(ts->client, get_object_address(ts, SPT_GPIOPWM_T19),
				pdata->config_T19, get_object_size(ts, SPT_GPIOPWM_T19));
			i2c_atmel_write(ts->client, get_object_address(ts, PROCI_GRIPFACESUPPRESSION_T20),
				pdata->config_T20, get_object_size(ts, PROCI_GRIPFACESUPPRESSION_T20));

			//printk("[%s] T22 Address:%d\n", __FUNCTION__, get_object_address(ts, PROCG_NOISESUPPRESSION_T22));

			//Decken_Kang, 20110914, [BugID 890] Implement 224E new register for noise suppression
			// In 224, PROCG_NOISESUPPRESSION is at T22
			if (ts->id->family_id == 0x80 && ts->id->variant_id == 0x10){
			    i2c_atmel_write(ts->client, get_object_address(ts, PROCG_NOISESUPPRESSION_T22),
				    pdata->config_T22, get_object_size(ts, PROCG_NOISESUPPRESSION_T22));
			}
			// In 224E, PROCG_NOISESUPPRESSION is at T48
			else if (ts->id->family_id == 0x81 && ts->id->variant_id == 0x01) {
				i2c_atmel_write(ts->client, get_object_address(ts, PROCG_NOISESUPPRESSION_T48),
				pdata->config_T48, get_object_size(ts, PROCG_NOISESUPPRESSION_T48));
			}
			//Decken_Kang
			i2c_atmel_write(ts->client, get_object_address(ts, TOUCH_PROXIMITY_T23),
				pdata->config_T23, get_object_size(ts, TOUCH_PROXIMITY_T23));
			i2c_atmel_write(ts->client, get_object_address(ts, PROCI_ONETOUCHGESTUREPROCESSOR_T24),
				pdata->config_T24, get_object_size(ts, PROCI_ONETOUCHGESTUREPROCESSOR_T24));
			i2c_atmel_write(ts->client, get_object_address(ts, SPT_SELFTEST_T25),
				pdata->config_T25, get_object_size(ts, SPT_SELFTEST_T25));
			i2c_atmel_write(ts->client, get_object_address(ts, PROCI_TWOTOUCHGESTUREPROCESSOR_T27),
				pdata->config_T27, get_object_size(ts, PROCI_TWOTOUCHGESTUREPROCESSOR_T27));

			/*printk("[%s] T28 Address:%d\n", __FUNCTION__, get_object_address(ts, SPT_CTECONFIG_T28));			
			printk("[%s] T40 Address:%d\n", __FUNCTION__, get_object_address(ts, PROCI_GRIPSUPPRESSION_T40));
			printk("[%s] T42 Address:%d\n", __FUNCTION__, get_object_address(ts, PROCI_TOUCHSUPPRESSION_T42));
			printk("[%s] T46 Address:%d\n", __FUNCTION__, get_object_address(ts, SPT_CTECONFIG_T46));
			printk("[%s] T47 Address:%d\n", __FUNCTION__, get_object_address(ts, PROCI_STYLUS_T47));
			printk("[%s] T48 Address:%d\n", __FUNCTION__, get_object_address(ts, PROCG_NOISESUPPRESSION_T48));*/

			//Decken_Kang, 20110914, [BugID 890] Implement 224E new register for noise suppression
			// In 224, SPT_CTECONFIG is at T28
			if (ts->id->family_id == 0x80 && ts->id->variant_id == 0x10){		
			i2c_atmel_write(ts->client, get_object_address(ts, SPT_CTECONFIG_T28),
				pdata->config_T28, get_object_size(ts, SPT_CTECONFIG_T28));
			}
			// In 224E, SPT_CTECONFIG is at T46
			else if (ts->id->family_id == 0x81 && ts->id->variant_id == 0x01) {
				i2c_atmel_write(ts->client, get_object_address(ts, SPT_CTECONFIG_T46),
					pdata->config_T46, get_object_size(ts, SPT_CTECONFIG_T46));
				i2c_atmel_write(ts->client, get_object_address(ts, PROCI_GRIPSUPPRESSION_T40),
					pdata->config_T40, get_object_size(ts, PROCI_GRIPSUPPRESSION_T40));
				i2c_atmel_write(ts->client, get_object_address(ts, PROCI_TOUCHSUPPRESSION_T42),
					pdata->config_T42, get_object_size(ts, PROCI_TOUCHSUPPRESSION_T42));
				i2c_atmel_write(ts->client, get_object_address(ts, PROCI_STYLUS_T47),
					pdata->config_T47, get_object_size(ts, PROCI_STYLUS_T47));

			}
			//Decken_Kang

		
			// TBD! below command will backup setting to non-volatile memory (NVM)
			ret = i2c_atmel_write_byte_data(client, get_object_address(ts, GEN_COMMANDPROCESSOR_T6) + 1, 0x55);

			for (loop_i = 0; loop_i < 10; loop_i++) {
				if (!gpio_get_value(intr))
					break;

				printk(KERN_INFO "Touch: wait for Message(%d)\n", loop_i + 1);
				msleep(10);
			}

			i2c_atmel_read(client, get_object_address(ts, GEN_MESSAGEPROCESSOR_T5), data, 8);
			printk(KERN_INFO "Touch: 0x%2.2X 0x%2.2X 0x%2.2X 0x%2.2X 0x%2.2X 0x%2.2X 0x%2.2X\n",
				data[0], data[1], data[2], data[3], data[4], data[5], data[6]);

			ret = i2c_atmel_write_byte_data(client, get_object_address(ts, GEN_COMMANDPROCESSOR_T6), 0x11); //henry: reset device
			msleep(64);
		}


		if (ts->status) {        
			printk(KERN_INFO "Touch: set cable config\n");
			if (ts->config_setting[1].config[0]) {
				i2c_atmel_write_byte_data(ts->client,
					get_object_address(ts, TOUCH_MULTITOUCHSCREEN_T9) + 7,
					ts->config_setting[1].config[0]);
				i2c_atmel_write_byte_data(ts->client,
					get_object_address(ts, SPT_CTECONFIG_T28) + 3,
					ts->config_setting[1].config[2]);
				i2c_atmel_write_byte_data(ts->client,
					get_object_address(ts, SPT_CTECONFIG_T28) + 4,
					ts->config_setting[1].config[3]);
			} else {
				if (ts->config_setting[1].config_T7 != NULL)
					i2c_atmel_write(ts->client,
						get_object_address(ts, GEN_POWERCONFIG_T7),
						ts->config_setting[ts->status].config_T7,
						get_object_size(ts, GEN_POWERCONFIG_T7));
				if (ts->config_setting[1].config_T8 != NULL)
					i2c_atmel_write(ts->client,
						get_object_address(ts, GEN_ACQUISITIONCONFIG_T8),
						ts->config_setting[1].config_T8,
						get_object_size(ts, GEN_ACQUISITIONCONFIG_T8));
				if (ts->config_setting[1].config_T9 != NULL)
					i2c_atmel_write(ts->client,
						get_object_address(ts, TOUCH_MULTITOUCHSCREEN_T9),
						ts->config_setting[1].config_T9,
						get_object_size(ts, TOUCH_MULTITOUCHSCREEN_T9));
				if (ts->config_setting[1].config_T22 != NULL)
					i2c_atmel_write(ts->client,
						get_object_address(ts, PROCG_NOISESUPPRESSION_T22),
						ts->config_setting[1].config_T22,
					get_object_size(ts, PROCG_NOISESUPPRESSION_T22));
				if (ts->config_setting[1].config_T28 != NULL) {
					i2c_atmel_write(ts->client,
						get_object_address(ts, SPT_CTECONFIG_T28),
						ts->config_setting[1].config_T28,
						get_object_size(ts, SPT_CTECONFIG_T28));
				}
			}
		}
	}
	
	ts->input_dev = input_allocate_device();
	if (ts->input_dev == NULL) {
		ret = -ENOMEM;
		dev_err(&ts->client->dev, "Failed to allocate input device\n");
		goto err_input_dev_alloc_failed;
	}
	ts->input_dev->name = "atmel-touchscreen";
	ts->input_dev->id.bustype = BUS_I2C;
	ts->input_dev->dev.parent = &ts->client->dev;
	set_bit(EV_SYN, ts->input_dev->evbit);
	set_bit(EV_KEY, ts->input_dev->evbit);
	set_bit(BTN_TOUCH, ts->input_dev->keybit);
	set_bit(BTN_2, ts->input_dev->keybit);
	set_bit(EV_ABS, ts->input_dev->evbit);
    //  tskpdev = ts->input_dev;
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X,
		ts->abs_x_min, ts->abs_x_max, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y,
		ts->abs_y_min, ts->abs_y_max, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR,
		ts->abs_pressure_min, ts->abs_pressure_max,0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR,
		ts->abs_width_min, ts->abs_width_max, 0, 0);
	
	input_set_drvdata(ts->input_dev, ts);
	//printk(KERN_INFO "irq no:%d", client->irq);
	ret = request_irq(client->irq, atmel_ts_irq_handler, IRQF_TRIGGER_LOW,client->name, ts);
	if (ret) {
		dev_err(&client->dev, "request_irq failed\n");
		goto err_input_register_device_failed;
	}

	ret = input_register_device(ts->input_dev);
	if (ret) {
		dev_err(&client->dev,
			"atmel_ts_probe: Unable to register %s input device\n",
			ts->input_dev->name);
		goto err_input_register_device_failed;
	}
#ifdef ENABLE_IME_IMPROVEMENT
	ts->ime_threshold_pixel = 0;
	ts->ime_finger_report[0] = -1;
	ts->ime_finger_report[1] = -1;
	ret = device_create_file(&ts->input_dev->dev, &dev_attr_ime_threshold);
	if (ret) {
		printk(KERN_ERR "ENABLE_IME_IMPROVEMENT: "
			"Error to create ime_threshold\n");
		goto err_input_register_device_failed;
	}
	ret = device_create_file(&ts->input_dev->dev, &dev_attr_ime_work_area);
	if (ret) {
		printk(KERN_ERR "ENABLE_IME_IMPROVEMENT: "
			"Error to create ime_work_area\n");
		device_remove_file(&ts->input_dev->dev,&dev_attr_ime_threshold);
		goto err_input_register_device_failed;
	}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_STOP_DRAWING - 1;
	ts->early_suspend.suspend = atmel_ts_early_suspend;
	ts->early_suspend.resume = atmel_ts_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif

#ifdef CONFIG_SLATE_TEST
	ts_input_dev = ts->input_dev;
#endif

	private_ts = ts;
#ifdef ATMEL_EN_SYSFS
	atmel_touch_sysfs_init();
#endif

	dev_info(&client->dev, "Start touchscreen %s in interrupt mode\n",	ts->input_dev->name);

	return 0;

err_input_register_device_failed:
	printk(KERN_ERR "err_input_register_device_failed\n");
	input_free_device(ts->input_dev);

err_input_dev_alloc_failed:
err_alloc_failed:
err_detect_failed:
err_power_failed:	
	printk(KERN_ERR "err_power_failed\n");
	destroy_workqueue(ts->atmel_wq);

err_create_wq_failed:
	printk(KERN_ERR "err_err_cread_wq_failed\n");
	kfree(ts);
	gpio_free(TS_RESET);
	gpio_free(TS_CHG);
	i2c_set_clientdata(client, NULL);

err_alloc_data_failed:
err_check_functionality_failed:

	return ret;
}

static int atmel_ts_remove(struct i2c_client *client)
{
	struct atmel_ts_data *ts = i2c_get_clientdata(client);

        printk(KERN_INFO "%s\n ",__func__);
#ifdef ATMEL_EN_SYSFS
	atmel_touch_sysfs_deinit();
#endif
#ifdef ENABLE_IME_IMPROVEMENT
	device_remove_file(&ts->input_dev->dev, &dev_attr_ime_threshold);
	device_remove_file(&ts->input_dev->dev, &dev_attr_ime_work_area);
#endif

	unregister_early_suspend(&ts->early_suspend);
	free_irq(client->irq, ts);

	destroy_workqueue(ts->atmel_wq);
	input_unregister_device(ts->input_dev);
	kfree(ts);

	return 0;
}

static int atmel_ts_suspend(struct i2c_client *client, pm_message_t mesg)
{
    int ret;
    struct atmel_ts_data *ts = i2c_get_clientdata(client);

    //printk(KERN_INFO "%s: enter\n", __func__);

    disable_irq(client->irq);

    ret = cancel_work_sync(&ts->work);
    if (ret)
        enable_irq(client->irq);

    ts->finger_pressed = 0;
    ts->finger_count = 0;
    ts->first_pressed = 0;

    i2c_atmel_write_byte_data(client,
        get_object_address(ts, GEN_POWERCONFIG_T7), 0x0);		// Idle Acquisition Interval = 0
    i2c_atmel_write_byte_data(client,
        get_object_address(ts, GEN_POWERCONFIG_T7) + 1, 0x0);	// Active Acquisition Interval = 0

#ifdef ENABLE_IME_IMPROVEMENT
    ts->ime_finger_report[0] = -1;
    ts->ime_finger_report[1] = -1;
#endif

    //Henry_lin, 20100906, [BugID 625] Electric current over spec when suspend.
    #if 1   // When suspend, mXT224 4 pin(RESET, CHG, DATA, CLK) status must be HIGH
    gpio_direction_output(TS_RESET, 1);
#if 0     // Bug 901, Henry GPIO 61 tuning
    gpio_direction_output(TS_CHG, 1);  // CHG pin must be high, TBD??
#endif
    //GPIO_CFG(19, 0, BSP_GPIO_INPUT, BSP_GPIO_PULL_UP, BSP_GPIO_2MA, HAL_TLMM_OUTPUT_LOW),
    #endif
    //Henry_lin

#if 1   //// Bug 901, Henry GPIO 61 tuning
    ret = gpio_tlmm_config(GPIO_CFG(TS_CHG, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_DISABLE);
    if (ret)
        PrintTip("[%s] Failed to configure GPIO %d\n", __FUNCTION__, ret);
#endif

    return 0;
}


static int atmel_ts_resume(struct i2c_client *client)
{
    struct atmel_ts_data *ts = i2c_get_clientdata(client);

    //Henry_lin, 20100906, [BugID 625] Electric current over spec when suspend.
    #if 1   // Correct pin status
    gpio_direction_output(TS_RESET, 1);
    //mdelay(200);
#if 0   // Bug 901, Henry GPIO 61 tuning
    gpio_direction_input(TS_CHG);
#endif
    #endif
    //Henry_lin

    i2c_atmel_write(ts->client, get_object_address(ts, GEN_POWERCONFIG_T7),
        ts->config_setting[ts->status].config_T7, get_object_size(ts, GEN_POWERCONFIG_T7));

    if (ts->status)
        ts->calibration_confirm = 2;
    else {
        ts->calibration_confirm = 0;
        i2c_atmel_write_byte_data(client,
            get_object_address(ts, GEN_COMMANDPROCESSOR_T6) + 2, 0x55);	// do calibration
        i2c_atmel_write_byte_data(client,
            get_object_address(ts, GEN_ACQUISITIONCONFIG_T8) + 6, 0x0);
        i2c_atmel_write_byte_data(client,
            get_object_address(ts, GEN_ACQUISITIONCONFIG_T8) + 7, 0x0);
    }
	
    enable_irq(client->irq);

    return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void atmel_ts_early_suspend(struct early_suspend *h)
{
	struct atmel_ts_data *ts;
	ts = container_of(h, struct atmel_ts_data, early_suspend);
	atmel_ts_suspend(ts->client, PMSG_SUSPEND);
}

static void atmel_ts_late_resume(struct early_suspend *h)
{
	struct atmel_ts_data *ts;
	ts = container_of(h, struct atmel_ts_data, early_suspend);
	atmel_ts_resume(ts->client);
}
#endif

static const struct i2c_device_id atml_ts_i2c_id[] = {
{ ATMEL_MXT224_NAME, 0 },
{ }
};

static struct i2c_driver atmel_ts_driver = {
	.id_table = atml_ts_i2c_id,
	.probe = atmel_ts_probe,
	.remove = atmel_ts_remove,
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend = atmel_ts_suspend,
	.resume = atmel_ts_resume,
#endif
	.driver = {
		.name = ATMEL_MXT224_NAME,
	},
};


static int __devinit atmel_ts_init(void)
{
  //  	struct pm8058_gpio touch_gpio;
  //    int rc = 0;
	Printhh("[%s] enter...\n", __FUNCTION__);

	if(1) {
		//printk("gpio_tlmm_config - for gpio TS_RESET = %d\n",TS_RESET);
		if (gpio_tlmm_config(GPIO_CFG(TS_RESET, 0,GPIO_CFG_OUTPUT,  GPIO_CFG_PULL_UP ,GPIO_CFG_2MA),GPIO_CFG_ENABLE)) {
			printk(KERN_ERR "%s: Err: Config GPIO-84 \n",	__func__);
		}
	
		if (gpio_request(TS_RESET, "TS_RESET"))
			printk(KERN_ERR "failed to request gpio TS_RESET\n");
		//	gpio_configure(TS_RESET,GPIOF_DRIVE_OUTPUT);
		   gpio_direction_output(TS_RESET,1);
		//printk("%s: TS_RESET = %d\n",__func__,gpio_get_value(TS_RESET) );
	}
	return i2c_add_driver(&atmel_ts_driver);
}


static void __exit atmel_ts_exit(void)
{
	printk(KERN_INFO "%s\n ",__func__);
	i2c_del_driver(&atmel_ts_driver);
}


static int handleTouchKeyEvent(struct atmel_finger_data* pData, int fingerNum ) {
#ifndef D_CCI_TOUCH_KEY
    return 0;
#else
    int i = 0;
    if(pData == NULL )
        return 0;
    if(pData[fingerNum].y < D_LCD_DISPLAY_HIGH)
        return 0;
    printk(KERN_INFO "handleTouchKeyEvent(): x:%d, y:%d, fingerNum:%d",pData[fingerNum].x, pData[fingerNum].y, fingerNum  );     
    for(i=0; i < D_TOUCH_KEY_NUM; i++ ) {
        if( pData[fingerNum].y >= tKeyAttr[i].y_min &&
            pData[fingerNum].y <= tKeyAttr[i].y_max &&
            pData[fingerNum].x >= tKeyAttr[i].x_min &&
            pData[fingerNum].x <= tKeyAttr[i].x_max )   
          {
            if(isTouchKeyPressed == 0 ) { 
              printk(KERN_INFO "handleTouchKeyEvent(): key pressed:%d, id:%d",i, tKeyAttr[i].keyid  );       
                s_vKeyArray(tKeyAttr[i].keyid );   
                isTouchKeyPressed = 1;
            }
            return 1;
          }
    }
         
    return 0;
#endif
}


module_init(atmel_ts_init);
module_exit(atmel_ts_exit);

MODULE_DESCRIPTION("ATMEL Touch driver");
MODULE_LICENSE("GPL");
