/* Copyright (c) 2009, Code Aurora Forum. All rights reserved.
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
 *
 */

/*===========================================================================
				INCLUDE FILES
===========================================================================*/
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/device.h> /* for vreg.h */
#include <linux/regulator/consumer.h>
#include <linux/slab.h>

#include <mach/gpio.h>
#include <mach/camera.h>
#include <mach/board.h>
#include <mach/msm_battery.h>
#include <mach/vreg.h>

#include <media/msm_camera.h>
#include "mt9m114.h"
#include <mach/cci_hw_id.h>//[bug868] Fix front camera module mount angle for DVT sample,cci 20110908

#include <linux/msm-charger.h>
/*===========================================================================
				MACRO DEFINITIONS
===========================================================================*/
/* Aptina MT9M114 product ID register address */
#define REG_MT9M114_MODEL_ID	0x0000

/* Aptina MT9M114 product ID */
#define MT9M114_MODEL_ID 0x2481

#define MT9M114_BRIGHTNESS_DEGREE 11
#define MT9M114_DEFAULT_CLOCK_RATE  24000000

#define ENABLE_ADJUSTMENT

#define EXPOSURE_NEGATIVE_2EV	(-12)
#define EXPOSURE_NEGATIVE_1EV	(-6)
#define EXPOSURE_DEFAULT_0EV	(0)
#define EXPOSURE_POSITIVE_1EV	(6)
#define EXPOSURE_POSITIVE_2EV	(12)

/*B:[Camera]  porting mt9m114, CCI 20110127 */
#define  CAMERA_WB_MIN_MINUS_1         (-1)
#define  CAMERA_WB_AUTO                 1
#define  CAMERA_WB_CUSTOM               2
#define  CAMERA_WB_INCANDESCENT         3
#define  CAMERA_WB_FLUORESCENT          4
#define  CAMERA_WB_DAYLIGHT             5
#define  CAMERA_WB_CLOUDY_DAYLIGHT      6
#define  CAMERA_WB_TWILIGHT             7
#define  CAMERA_WB_SHADE                8
#define  CAMERA_WB_OFF                9
#define  CAMERA_WB_MAX_PLUS_1           10

#define  CAMERA_ISO_AUTO		0
#define  CAMERA_ISO_DEBLUR		1
#define  CAMERA_ISO_100			2
#define  CAMERA_ISO_200			3
#define  CAMERA_ISO_400			4
#define  CAMERA_ISO_800			5
#define  CAMERA_ISO_1600		6
#define  CAMERA_ISO_MAX                 7

#define  CAMERA_AEC_FRAME_AVERAGE       0
#define  CAMERA_AEC_CENTER_WEIGHTED     1
#define  CAMERA_AEC_SPOT_METERING       2
#define  CAMERA_AEC_MAX_MODES           3

#define  CAMERA_ANTIBANDING_OFF         0
#define  CAMERA_ANTIBANDING_60HZ        1
#define  CAMERA_ANTIBANDING_50HZ        2
#define  CAMERA_ANTIBANDING_AUTO        3
#define  CAMERA_MAX_ANTIBANDING         4

#define  CAMERA_BESTSHOT_OFF            0
#define  CAMERA_BESTSHOT_LANDSCAPE      1
#define  CAMERA_BESTSHOT_SNOW           2
#define  CAMERA_BESTSHOT_BEACH          3
#define  CAMERA_BESTSHOT_SUNSET         4
#define  CAMERA_BESTSHOT_NIGHT          5
#define  CAMERA_BESTSHOT_PORTRAIT       6
#define  CAMERA_BESTSHOT_BACKLIGHT      7
#define  CAMERA_BESTSHOT_SPORTS         8
#define  CAMERA_BESTSHOT_ANTISHAKE      9
#define  CAMERA_BESTSHOT_FLOWERS       10
#define  CAMERA_BESTSHOT_CANDLELIGHT   11
#define  CAMERA_BESTSHOT_FIREWORKS           12
#define  CAMERA_BESTSHOT_PARTY       13
#define  CAMERA_BESTSHOT_NIGHT_PORTRAIT   14
#define  CAMERA_BESTSHOT_THEATRE           15
#define  CAMERA_BESTSHOT_ACTION       16
#define  CAMERA_BESTSHOT_AR   17
#define  CAMERA_BESTSHOT_MAX           18
/*E:[Camera]  porting mt9m114, CCI 20110127 */

/*===========================================================================
				TYPE DEFINITIONS
===========================================================================*/
struct mt9m114_work {
    struct work_struct work;
};

struct mt9m114_ctrl {
    const struct msm_camera_sensor_info *sensordata;

    int sensormode;
    uint32_t fps_divider; /* init to 1 * 0x00000400 */
    uint32_t pict_fps_divider; /* init to 1 * 0x00000400 */

    uint16_t curr_lens_pos;
    uint16_t init_curr_lens_pos;
    uint16_t my_reg_gain;
    uint32_t my_reg_line_count;

    enum msm_s_resolution prev_res;
    enum msm_s_resolution pict_res;
    enum msm_s_resolution curr_res;
    enum msm_s_test_mode  set_test;
};

typedef  struct
{
    uint16_t	addr;
    uint8_t		val;
} CAM_REG_ADDR_VAL_TYPE;

typedef enum
{
    //Preview Size
    MT9M114_VGA_PREVIEW = 0x01,
    MT9M114_720P_PREVIEW = 0x02, /* 1280 x 720 */
    MT9M114_HD_PREVIEW = 0x04, /* 1280 x 960 */

    //Output data lane number
    MT9M114_OUTPUT_1_LANE = 0x08,
    MT9M114_OUTPUT_2_LANE = 0x10,

    //Feature
    MT9M114_IMAGE_QUALITY_TUNING = 0x20,
    MT9M114_FIX_AEC_AGC = 0x40,
    MT9M114_ENABLE_ADJUSTMENT = 0x80,
} MT9M114_SETTING;

/*===========================================================================
				VARIABLE DECLARATIONS
===========================================================================*/
static struct mt9m114_work *mt9m114_sensorw;
static struct i2c_client *mt9m114_client;
static struct mt9m114_ctrl *mt9m114_ctrl;

static bool MT9M114_CSI_CONFIG = 0;

static int mt9m114_cur_setting = (MT9M114_HD_PREVIEW|MT9M114_OUTPUT_1_LANE|/*MT9M114_FIX_AEC_AGC|*/MT9M114_ENABLE_ADJUSTMENT);
//static int mt9m114_cur_setting = (MT9M114_VGA_PREVIEW|MT9M114_OUTPUT_1_LANE|/*MT9M114_FIX_AEC_AGC|*/MT9M114_ENABLE_ADJUSTMENT);

//static int mt9m114_brightness_level;//Ouyang@CCI

static struct regulator *vreg_AVDD_L19;
static struct regulator *vreg_VDDIO_S3;

static DECLARE_WAIT_QUEUE_HEAD(mt9m114_wait_queue);
DEFINE_MUTEX(mt9m114_mutex);

extern int system_loading_update(enum module_loading_op action, int id, int loading);
/*===========================================================================
	                     EXTERNAL DECLARATIONS
===========================================================================*/
extern struct mt9m114_reg mt9m114_regs;

/*===========================================================================
				DATA DECLARATIONS
===========================================================================*/
//[----------------------------------------------------------------------]
//[Exposure]
//[----------------------------------------------------------------------]

static const struct mt9m114_i2c_reg_conf const exposure_positive_2ev [] = {
    //[Ev+2]//140
    {0x098E, 0xC87A, WORD_LEN, 0}, 	// LOGICAL_ADDRESS_ACCESS [CAM_AET_TARGET_AVERAGE_LUMA]
    {0xC87A, 0x4B, BYTE_LEN, 0},	// CAM_AET_TARGET_AVERAGE_LUMA
};

static const struct mt9m114_i2c_reg_conf const exposure_positive_1ev [] = {
    //[Ev+1]//130
    {0x098E, 0xC87A, WORD_LEN, 0}, 	// LOGICAL_ADDRESS_ACCESS [CAM_AET_TARGET_AVERAGE_LUMA]
    {0xC87A, 0x42, BYTE_LEN, 0},	// CAM_AET_TARGET_AVERAGE_LUMA
};

static const struct mt9m114_i2c_reg_conf const exposure_default_0ev [] = {
    //[Ev+0]//120
    {0x098E, 0xC87A, WORD_LEN, 0}, 	// LOGICAL_ADDRESS_ACCESS [CAM_AET_TARGET_AVERAGE_LUMA]
    {0xC87A, 0x3C, BYTE_LEN, 0},	// CAM_AET_TARGET_AVERAGE_LUMA
};

static const struct mt9m114_i2c_reg_conf const exposure_negative_1ev [] = {
    //[Ev-1]//110
    {0x098E, 0xC87A, WORD_LEN, 0}, 	// LOGICAL_ADDRESS_ACCESS [CAM_AET_TARGET_AVERAGE_LUMA]
    {0xC87A, 0x36, BYTE_LEN, 0},	// CAM_AET_TARGET_AVERAGE_LUMA
};

static const struct mt9m114_i2c_reg_conf const exposure_negative_2ev [] = {
    //[Ev-2]//100
    {0x098E, 0xC87A, WORD_LEN, 0}, 	// LOGICAL_ADDRESS_ACCESS [CAM_AET_TARGET_AVERAGE_LUMA]
    {0xC87A, 0x32, BYTE_LEN, 0},	// CAM_AET_TARGET_AVERAGE_LUMA
};

//[----------------------------------------------------------------------]
//[Color effect]
//[----------------------------------------------------------------------]

static const struct mt9m114_i2c_reg_conf const color_effects_none [] = {
    //[None]
    {0x098E, 0xC874, WORD_LEN, 0}, 	// LOGICAL_ADDRESS_ACCESS [CAM_SFX_CONTROL]
    {0xC874, 0x00, BYTE_LEN, 0},	// CAM_SFX_CONTROL
    {0xDC00, 0x28, BYTE_LEN, 0},	// SYSMGR_NEXT_STATE
    {0x0080, 0x8004, WORD_LEN, 0}	// COMMAND_REGISTER
};

static const struct mt9m114_i2c_reg_conf const color_effects_mono [] = {
    //[Mono]
    {0x098E, 0xC874, WORD_LEN, 0}, 	// LOGICAL_ADDRESS_ACCESS [CAM_SFX_CONTROL]
    {0xC874, 0x01, BYTE_LEN, 0},	// CAM_SFX_CONTROL
    {0xDC00, 0x28, BYTE_LEN, 0},	// SYSMGR_NEXT_STATE
    {0x0080, 0x8004, WORD_LEN, 0}	// COMMAND_REGISTER
};

static const struct mt9m114_i2c_reg_conf const color_effects_sepia [] = {
    //[Sepia]
    {0x098E, 0xC874, WORD_LEN, 0}, 	// LOGICAL_ADDRESS_ACCESS [CAM_SFX_CONTROL]
    {0xC874, 0x02, BYTE_LEN, 0},	// CAM_SFX_CONTROL
    {0xC876, 0x23, BYTE_LEN, 0},	// CAM_SFX_SEPIA_CR
    {0xC877, 0xB2, BYTE_LEN, 0},	// CAM_SFX_SEPIA_CB
    {0xDC00, 0x28, BYTE_LEN, 0},	// SYSMGR_NEXT_STATE
    {0x0080, 0x8004, WORD_LEN, 0} 	// COMMAND_REGISTER
};

static const struct mt9m114_i2c_reg_conf const color_effects_negative [] = {
    //[Negative]
    {0x098E, 0xC874, WORD_LEN, 0}, 	// LOGICAL_ADDRESS_ACCESS [CAM_SFX_CONTROL]
    {0xC874, 0x03, BYTE_LEN, 0},	// CAM_SFX_CONTROL
    {0xDC00, 0x28, BYTE_LEN, 0},	// SYSMGR_NEXT_STATE
    {0x0080, 0x8004, WORD_LEN, 0} 	// COMMAND_REGISTER
};

static const struct mt9m114_i2c_reg_conf const color_effects_solarization [] = {
    //[Solarization with unmodified UV]
    {0x098E, 0xC874, WORD_LEN, 0}, 	// LOGICAL_ADDRESS_ACCESS [CAM_SFX_CONTROL]
    {0xC874, 0x04, BYTE_LEN, 0},	// CAM_SFX_CONTROL
    {0xDC00, 0x28, BYTE_LEN, 0},	// SYSMGR_NEXT_STATE
    {0x0080, 0x8004, WORD_LEN, 0} 	// COMMAND_REGISTER
};

static const struct mt9m114_i2c_reg_conf const color_effects_solarization2 [] = {
    //[Solarization with -UV]
    {0x098E, 0xC874, WORD_LEN, 0}, 	// LOGICAL_ADDRESS_ACCESS [CAM_SFX_CONTROL]
    {0xC874, 0x05, BYTE_LEN, 0},	// CAM_SFX_CONTROL
    {0xDC00, 0x28, BYTE_LEN, 0},	// SYSMGR_NEXT_STATE
    {0x0080, 0x8004, WORD_LEN, 0} 	// COMMAND_REGISTER
};

static const struct mt9m114_i2c_reg_conf const color_effects_aqua [] = {
    //[Aqua]
    {0x098E, 0xC874, WORD_LEN, 0}, 	// LOGICAL_ADDRESS_ACCESS [CAM_SFX_CONTROL]
    {0xC874, 0x02, BYTE_LEN, 0},	// CAM_SFX_CONTROL
    {0xC876, 0xDB, BYTE_LEN, 0},	// CAM_SFX_SEPIA_CR
    {0xC877, 0x27, BYTE_LEN, 0},	// CAM_SFX_SEPIA_CB    
    {0xDC00, 0x28, BYTE_LEN, 0},	// SYSMGR_NEXT_STATE
    {0x0080, 0x8004, WORD_LEN, 0} 	// COMMAND_REGISTER
};


//[----------------------------------------------------------------------]
//[Manual White Balance]
//[----------------------------------------------------------------------]

static const struct mt9m114_i2c_reg_conf const white_balance_auto [] = {
    //[Auto]
    {0x098E, 0xC909, WORD_LEN, 0}, 	// LOGICAL_ADDRESS_ACCESS [CAM_AWB_AWBMODE]
    {0xC909, 0x03, BYTE_LEN, 0}	// CAM_AWB_AWBMODE
};

static const struct mt9m114_i2c_reg_conf const white_balance_daylight [] = {
    //[Daylight]
    {0x098E, 0x0000, WORD_LEN, 0}, 	// LOGICAL_ADDRESS_ACCESS
    {0xC909, 0x01, BYTE_LEN, 0}, 	// CAM_AWB_AWBMODE
    {0xC8F0, 0x1964, WORD_LEN, 0} 	// CAM_AWB_COLOR_TEMPERATURE
};

static const struct mt9m114_i2c_reg_conf const white_balance_fluorescent [] = {
    //[Fluorescent]
    {0x098E, 0x0000, WORD_LEN, 0}, 	// LOGICAL_ADDRESS_ACCESS
    {0xC909, 0x01, BYTE_LEN, 0}, 	// CAM_AWB_AWBMODE
    {0xC8F0, 0x113D, WORD_LEN, 0} 	// CAM_AWB_COLOR_TEMPERATURE
};

static const struct mt9m114_i2c_reg_conf const white_balance_cloudy [] = {
    //[Cloudy]
    {0x098E, 0x0000, WORD_LEN, 0},	// LOGICAL_ADDRESS_ACCESS
    {0xC909, 0x01, BYTE_LEN, 0}, 	// CAM_AWB_AWBMODE
    {0xC8F0, 0x0DAC, WORD_LEN, 0} 	// CAM_AWB_COLOR_TEMPERATURE
};

static const struct mt9m114_i2c_reg_conf const white_balance_incandescent [] = {
    //[Incandescent]
    {0x098E, 0x0000, WORD_LEN, 0}, 	// LOGICAL_ADDRESS_ACCESS
    {0xC909, 0x01, BYTE_LEN, 0}, 	// CAM_AWB_AWBMODE
    {0xC8F0, 0x0AF0, WORD_LEN, 0} 	// CAM_AWB_COLOR_TEMPERATURE
};

//Enable mt9m114 antibanding
//[----------------------------------------------------------------------]
//[Anti banding]
//[----------------------------------------------------------------------]
static const struct mt9m114_i2c_reg_conf const anti_banding_50hz [] = {
    //[50Hz]
    {0x098E, 0xCC03, WORD_LEN, 0}, 	// // LOGICAL_ADDRESS_ACCESS [UVC_POWER_LINE_FREQUENCY_CONTROL]
    {0xCC03, 0x01, BYTE_LEN, 0}	// UVC_POWER_LINE_FREQUENCY_CONTROL
};

static const struct mt9m114_i2c_reg_conf const anti_banding_60hz [] = {
    //[60Hz]
    {0x098E, 0xCC03, WORD_LEN, 0}, 	// // LOGICAL_ADDRESS_ACCESS [UVC_POWER_LINE_FREQUENCY_CONTROL]
    {0xCC03, 0x02, BYTE_LEN, 0}	// UVC_POWER_LINE_FREQUENCY_CONTROL
};

/*===========================================================================
				FUNCTION BODY
===========================================================================*/
int mt9m114_gpio_set_value(unsigned gpio, int value)
{
    int rc = 0;

    printk("mt9m114_gpio_set_value(): set GPIO(%d) to (%d)\n", gpio, value);

    rc = gpio_request(gpio, "mt9m114");

    if (!rc || rc == -EBUSY)
        gpio_direction_output(gpio, value);
    else
        printk(KERN_ERR "mt9m114_gpio_set_value: set GPIO(%d) failed, rc = (%d)\n", gpio, rc);

    gpio_free(gpio);

    return rc;
}

static int32_t mt9m114_power_enable(void)
{
    CDBG("[CL]-%s: ++\n", __func__);

    system_loading_update(LOADING_CLR, 4, 0);
    system_loading_update(LOADING_SET, 4, 260);
	
    vreg_VDDIO_S3 = regulator_get(NULL, "8058_s3");
    if (IS_ERR(vreg_VDDIO_S3)) {
        pr_err("%s: VREG 8058_s3 get failed\n", __func__);
        vreg_VDDIO_S3 = NULL;
        return 0;
    }
    if (regulator_set_voltage(vreg_VDDIO_S3, 1800000, 1800000)) {
        pr_err("%s: VREG 8058_s3 set voltage failed\n",  __func__);
        goto s3_disable;
    }
    if (regulator_enable(vreg_VDDIO_S3)) {
        pr_err("%s: VREG 8058_s3 enable failed\n", __func__);
        goto s3_put;
    }
    
    vreg_AVDD_L19 = regulator_get(NULL, "8058_l19");
    if (IS_ERR(vreg_AVDD_L19)) {
        pr_err("%s: VREG LDO19 get failed\n", __func__);
        vreg_AVDD_L19 = NULL;
        return 0;
    }
    if (regulator_set_voltage(vreg_AVDD_L19, 2800000, 2800000)) {
        pr_err("%s: VREG LDO19 set voltage failed\n",  __func__);
        goto ldo19_disable;
    }
    if (regulator_enable(vreg_AVDD_L19)) {
        pr_err("%s: VREG LDO19 enable failed\n", __func__);
        goto ldo19_put;
    }
    return 0;

ldo19_disable:
	regulator_disable(vreg_AVDD_L19);
ldo19_put:
	regulator_put(vreg_AVDD_L19);
s3_disable:
	regulator_disable(vreg_VDDIO_S3);
s3_put:
	regulator_put(vreg_VDDIO_S3);
    
    return 0;
}
static int32_t mt9m114_power_disable(void)
{
    CDBG("[CL]-%s: ++\n", __func__);

    system_loading_update(LOADING_CLR, 4, 0);
	
    if (vreg_AVDD_L19) {
        regulator_disable(vreg_AVDD_L19);
        regulator_put(vreg_AVDD_L19);
        vreg_AVDD_L19 = NULL;
    }
    if (vreg_VDDIO_S3) {
        regulator_disable(vreg_VDDIO_S3);
        regulator_put(vreg_VDDIO_S3);
        vreg_VDDIO_S3 = NULL;
    }
    /*B [bug390] [bug745] refine the gpio setting usage, CCI 20110829 */
    gpio_set_value(45, 0);//reset
    gpio_set_value(46, 0);//pwdn
    /*E [bug390] [bug745] refine the gpio setting usage, CCI 20110829 */
    
    return 0;
}

static int mt9m114_i2c_rxdata(unsigned short saddr, unsigned char *rxdata,
                              int length)
{
    struct i2c_msg msgs[] = {
        {
            .addr   = saddr,
            .flags = 0,
            .len   = 2,
            .buf   = rxdata,
        },
        {
            .addr   = saddr,
            .flags = I2C_M_RD,
            .len   = length,
            .buf   = rxdata,
        },
    };

    if (i2c_transfer(mt9m114_client->adapter, msgs, 2) < 0) {
        printk("mt9m114_i2c_rxdata failed!\n");
        return -EIO;
    }

    return 0;
}

static int32_t mt9m114_i2c_txdata(unsigned short saddr,
                                  unsigned char *txdata, int length)
{
    struct i2c_msg msg[] = {
        {
            .addr  = saddr,
            .flags = 0,
            .len = length,
            .buf = txdata,
        },
    };

    if (i2c_transfer(mt9m114_client->adapter, msg, 1) < 0) {
        printk("mt9m114_i2c_txdata failed!\n");
        return -EIO;
    }

    return 0;
}


static int32_t mt9m114_i2c_write(unsigned short saddr, unsigned short waddr,
                                 unsigned short wdata, enum mt9m114_width width)
{
    int32_t rc = -EIO;
    unsigned char buf[4];

    memset(buf, 0, sizeof(buf));

    switch (width) {
    case WORD_LEN: {
        buf[0] = (waddr & 0xFF00)>>8;
        buf[1] = (waddr & 0x00FF);
        buf[2] = (wdata & 0xFF00)>>8;
        buf[3] = (wdata & 0x00FF);
        rc = mt9m114_i2c_txdata(saddr, buf, 4);
    }
    break;

    case BYTE_LEN: {
        buf[0] = (waddr & 0xFF00)>>8;
        buf[1] = (waddr & 0x00FF);
        buf[2] = wdata;
        rc = mt9m114_i2c_txdata(saddr, buf, 3);
    }
    break;

    default:
        break;
    }

    if (rc < 0)
        printk("[camera] mt9m114_i2c_write(): failed, addr = 0x%x, val = 0x%04x!\n",
               waddr, wdata);

    return rc;
}

static int32_t mt9m114_i2c_write_table(
    struct mt9m114_i2c_reg_conf const *reg_conf_tbl,
    int num_of_items_in_table)
{
    int i;
    int32_t rc = -EIO;

    for (i = 0; i < num_of_items_in_table; i++) {
        rc = mt9m114_i2c_write(mt9m114_client->addr,reg_conf_tbl->waddr, reg_conf_tbl->wdata,
                               reg_conf_tbl->width);
        if (rc < 0)
            break;
        if (reg_conf_tbl->mdelay_time != 0)
        {
            printk("[camera] mt9m114_i2c_write_table(): delay = %d ms\n",
                   reg_conf_tbl->mdelay_time);
            msleep(reg_conf_tbl->mdelay_time);
        }
        reg_conf_tbl++;
    }

    return rc;
}

static int32_t mt9m114_i2c_read(unsigned short saddr, unsigned short raddr,
                                unsigned short *rdata)
{
    int32_t rc = 0;
    unsigned char buf[4];

    if (!rdata)
        return -EIO;

    memset(buf, 0, sizeof(buf));

    buf[0] = (raddr & 0xFF00)>>8;
    buf[1] = (raddr & 0x00FF);

    rc = mt9m114_i2c_rxdata(saddr, buf, 2);

    if (rc < 0)
        return rc;

    *rdata = (buf[0] << 8) | (buf[1]);

    if (rc < 0)
        printk("[camera] mt9m114_i2c_read(): failed, addr = 0x%x!\n",
               raddr);

    return rc;
}

static int32_t camsensor_mt9m114_initial_reg_setup(void)
{
    int32_t rc = 0;

    printk("[camera] camsensor_mt9m114_initial_reg_setup()\n");

    /* [Reset] */
    rc = mt9m114_i2c_write(mt9m114_client->addr, 0x301A, 0x0234, WORD_LEN); // RESET_REGISTER

    if (rc < 0)
        return rc;

    msleep(10);

    /* [PLL setting] */
    printk("[camera] camsensor_mt9m114_initial_reg_setup(): PLL setting\n");
    rc = mt9m114_i2c_write_table(&mt9m114_regs.pll_tbl[0],
                                 mt9m114_regs.pll_tbl_size);
    if (rc < 0)
        return rc;

    /* [Timing setting] */
    printk("[camera] camsensor_mt9m114_initial_reg_setup(): Timing setting\n");
    rc = mt9m114_i2c_write_table(&mt9m114_regs.timing_setting_tbl[0],
                                 mt9m114_regs.timing_setting_tbl_size);
    if (rc < 0)
        return rc;

    /* [Sensor optimization] */
    printk("[camera] camsensor_mt9m114_initial_reg_setup(): Sensor optimization\n");
    rc = mt9m114_i2c_write_table(&mt9m114_regs.sensor_optimization_tbl[0],
                                 mt9m114_regs.sensor_optimization_tbl_size);
    if (rc < 0)
        return rc;

    /* [APGA] */
    printk("[camera] camsensor_mt9m114_initial_reg_setup(): APGA\n");
    rc = mt9m114_i2c_write_table(&mt9m114_regs.apga_tbl[0],
                                 mt9m114_regs.apga_tbl_size);
    if (rc < 0)
        return rc;

    /* [AWB_CCM] */
    printk("[camera] camsensor_mt9m114_initial_reg_setup(): AWB_CCM\n");
    rc = mt9m114_i2c_write_table(&mt9m114_regs.awb_ccm_tbl[0],
                                 mt9m114_regs.awb_ccm_tbl_size);
    if (rc < 0)
        return rc;

    /* [AWB] */
    printk("[camera] camsensor_mt9m114_initial_reg_setup(): AWB\n");
    rc = mt9m114_i2c_write_table(&mt9m114_regs.awb_tbl[0],
                                 mt9m114_regs.awb_tbl_size);
    if (rc < 0)
        return rc;

    /* [CPIPE_Preference] */
    printk("[camera] camsensor_mt9m114_initial_reg_setup(): CPIPE_Preference\n");
    rc = mt9m114_i2c_write_table(&mt9m114_regs.cpipe_preference_tbl[0],
                                 mt9m114_regs.cpipe_preference_tbl_size);
    if (rc < 0)
        return rc;

    /* [Features] */
    printk("[camera] camsensor_mt9m114_initial_reg_setup(): Features\n");
    rc = mt9m114_i2c_write_table(&mt9m114_regs.features_tbl[0],
                                 mt9m114_regs.features_tbl_size);
    if (rc < 0)
        return rc;
#if 1
    /* [MIPI enable] */
    /* [MIPI setting for SOC1040] */
    printk("[camera] camsensor_mt9m114_preview_reg_setup(): MIPI setting for SOC1040\n");
    rc = mt9m114_i2c_write_table(&mt9m114_regs.mipi_setting_tbl[0],
                                 mt9m114_regs.mipi_setting_tbl_size);
    if (rc < 0)
        return rc;
#endif


    return rc;
}


static int mt9m114_probe_init_done(const struct msm_camera_sensor_info *data)
{
    int rc = 0;

    printk("[camera] mt9m114_probe_init_done()\n");

#if 0
    rc = gpio_request(data->sensor_pwd, "mt9m114");

    printk("[camera] mt9m114_probe_init_done(): GPIO(%d) active low=%d\n", data->sensor_pwd, rc);
    if (!rc)
        gpio_direction_output(data->sensor_pwd, 0);
    else printk(KERN_ERR "[camera] mt9m114_probe_init_done(): request GPIO(%d) failed: "
                    "%d\n", data->sensor_pwd, rc);

    gpio_free(data->sensor_pwd);

    rc = gpio_request(data->sensor_reset, "mt9m114");

    printk("[camera] mt9m114_probe_init_done(): GPIO(%d) active low=%d\n", data->sensor_reset, rc);
    if (!rc || rc == -EBUSY)
        gpio_direction_output(data->sensor_reset, 0);
    else printk(KERN_ERR "[camera] mt9m114_probe_init_done(): request GPIO(%d) failed: "
                    "%d\n", data->sensor_reset, rc);

    gpio_free(data->sensor_reset);
#else
#endif

    return rc;
}

static int mt9m114_probe_init_sensor(const struct msm_camera_sensor_info *data)
{

    int32_t  rc;
    uint16_t model_id = 0;

    printk("[camera] mt9m114_probe_init_sensor()\n");

    /* Read sensor Model ID: */
    rc = mt9m114_i2c_read(mt9m114_client->addr, REG_MT9M114_MODEL_ID, &model_id);
    printk("[camera] mt9m114_probe_init_sensor(): camsensor_mt9m114_i2c_read(0x%04X)=%d\n", REG_MT9M114_MODEL_ID, rc);
    if (rc < 0)
        goto init_probe_fail;

    printk("mt9m114 model_id = 0x%x\n", model_id);

    /* Compare sensor ID to MT9M114 ID: */
    if (model_id != MT9M114_MODEL_ID) {
        rc = -ENODEV;
        goto init_probe_fail;
    }

    msleep(10);
    goto init_probe_done;

init_probe_fail:
    printk("[camera] mt9m114_probe_init_sensor(): FAIL!!\n");
    mt9m114_probe_init_done(data);

init_probe_done:
    return rc;
}

static int mt9m114_init_client(struct i2c_client *client)
{
    /* Initialize the MSM_CAMI2C Chip */
    init_waitqueue_head(&mt9m114_wait_queue);
    return 0;
}

static const struct i2c_device_id mt9m114_i2c_id[] = {
    { "mt9m114", 0},
    { }
};

static int mt9m114_i2c_probe(struct i2c_client *client,
                             const struct i2c_device_id *id)
{
    int rc = 0;
    printk("mt9m114_i2c_probe(): called!\n");

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        printk("i2c_check_functionality failed\n");
        goto probe_failure;
    }

    mt9m114_sensorw = kzalloc(sizeof(struct mt9m114_work), GFP_KERNEL);
    if (!mt9m114_sensorw) {
        printk("kzalloc failed.\n");
        rc = -ENOMEM;
        goto probe_failure;
    }

    i2c_set_clientdata(client, mt9m114_sensorw);
    mt9m114_init_client(client);
    mt9m114_client = client;

    msleep(50);

    printk("mt9m114_i2c_probe(): successed! rc = %d\n", rc);
    return 0;

probe_failure:
    printk("mt9m114_i2c_probe(): failed! rc = %d\n", rc);
    return rc;
}

static struct i2c_driver mt9m114_i2c_driver = {
    .id_table = mt9m114_i2c_id,
    .probe  = mt9m114_i2c_probe,
    .remove = __exit_p(mt9m114_i2c_remove),
    .driver = {
        .name = "mt9m114",
    },
};

#if 0 //UNUSED SYMBOL
static int32_t mt9m114_write_AWB_reg(void)
{
    int32_t rc = 0;
    return rc;
}

static int32_t mt9m114_write_WB_cloudy_reg(void)
{
    int32_t rc = 0;
    return rc;
}

static int32_t mt9m114_write_WB_horizon_reg(void)
{
    int32_t rc = 0;
    return rc;
}

static int32_t mt9m114_write_WB_fluorescent_reg(void)
{
    int32_t rc = 0;
    return rc;
}

static int32_t mt9m114_write_WB_tungsten_reg(void)
{
    int32_t rc = 0;
    return rc;
}

static int32_t mt9m114_write_WB_daylight_reg(void)
{
    int32_t rc = 0;
    return rc;
}

static int32_t mt9m114_write_bestshot_mode_beach_reg(void)
{
    int32_t rc = 0;
    return rc;
}

static int32_t mt9m114_write_bestshot_mode_night_reg(void)
{
    int32_t rc = 0;
    return rc;
}

static int32_t mt9m114_write_bestshot_mode_night_break_reg(void)
{
    int32_t rc = 0;
    return rc;
}
#endif

static int32_t camsensor_mt9m114_capture_reg_setup(void)
{
    int32_t rc = 0;

    if (mt9m114_cur_setting & MT9M114_720P_PREVIEW)
    {
        printk("[camera] camsensor_mt9m114_capture_reg_setup(): 720P\n");
        //Do nothing
    }
    else
    {
        printk("[camera] camsensor_mt9m114_capture_reg_setup(): HD\n");
        //Do nothing
        /*B:[bug756] Front camera initial code update to Ver.2-20110824, CCI 20110830 */
        #if 0
        rc = mt9m114_i2c_write_table(&mt9m114_regs.mt9m114_1280x960_tbl[0],
                                     mt9m114_regs.mt9m114_1280x960_tbl_size);
        #endif
        /*E:[bug756] Front camera initial code update to Ver.2-20110824, CCI 20110830 */
    }

    return rc;
}

static int32_t camsensor_mt9m114_preview_reg_setup(void)
{
    int32_t rc = 0;

    printk("[camera] camsensor_mt9m114_preview_reg_setup()\n");

    if (mt9m114_cur_setting & MT9M114_VGA_PREVIEW)
    {
        /* [VGA] */
        printk("[camera] camsensor_mt9m114_preview_reg_setup(): VGA\n");
        rc = mt9m114_i2c_write_table(&mt9m114_regs.mt9m114_vga_tbl[0],
                                     mt9m114_regs.mt9m114_vga_tbl_size);
        if (rc < 0)
            return rc;
    }
    else if (mt9m114_cur_setting & MT9M114_720P_PREVIEW)
    {
        /* [720P] */
        printk("[camera] camsensor_mt9m114_preview_reg_setup(): 720P\n");
        rc = mt9m114_i2c_write_table(&mt9m114_regs.mt9m114_720p_tbl[0],
                                     mt9m114_regs.mt9m114_720p_tbl_size);
        if (rc < 0)
            return rc;
    }
    else if (mt9m114_cur_setting & MT9M114_HD_PREVIEW)
    {
        /* [1280x960] */
        printk("[camera] camsensor_mt9m114_preview_reg_setup(): 1280x960\n");
        rc = mt9m114_i2c_write_table(&mt9m114_regs.mt9m114_1280x960_tbl[0],
                                     mt9m114_regs.mt9m114_1280x960_tbl_size);
        if (rc < 0)
            return rc;
    }
    else
    {
        //Error handle here
    }

#if 0
    /* [MIPI enable] */
    /* [MIPI setting for SOC1040] */
    printk("[camera] camsensor_mt9m114_preview_reg_setup(): MIPI setting for SOC1040\n");
    rc = mt9m114_i2c_write_table(&mt9m114_regs.mipi_setting_tbl[0],
                                 mt9m114_regs.mipi_setting_tbl_size);
    if (rc < 0)
        return rc;
#endif

    return rc;
}

static int32_t mt9m114_setting(enum msm_s_reg_update rupdate,
                               enum msm_s_setting rt)
{
    int32_t rc = 0;
    struct msm_camera_csi_params mt9m114_csi_params;

    printk("#### %s(%d %d) ####\n", __FUNCTION__, rupdate, rt);

    switch (rupdate)
    {
    case S_UPDATE_PERIODIC:

        if (rt == S_RES_PREVIEW || rt == S_RES_CAPTURE)
        {
            if(MT9M114_CSI_CONFIG == 0)
            {
                printk("[camera] mt9m114_setting(): config CSI\n");
    
                mt9m114_csi_params.data_format = CSI_8BIT;

                if (0 != (mt9m114_cur_setting & MT9M114_OUTPUT_1_LANE))
                    mt9m114_csi_params.lane_cnt = 1;// 1 lane
                else
                    mt9m114_csi_params.lane_cnt = 2;// 2 lane

                mt9m114_csi_params.lane_assign = 0xe4;//0xe4: no swap physical lane (0, 1, 2, 3) and hardware lane (0, 1, 2, 3).
                mt9m114_csi_params.settle_cnt = 0x14;
                mt9m114_csi_params.dpcm_scheme = 0;
                rc = msm_camio_csi_config(&mt9m114_csi_params);
                msleep(10);
                
                MT9M114_CSI_CONFIG = 1;
            }
        }

        /*lint -restore */
        switch (rt)
        {
        case S_RES_CAPTURE:
            rc = camsensor_mt9m114_capture_reg_setup();
            if(rc < 0)
            {
                return rc;
            }
            msleep(100);
            break;

        case S_RES_PREVIEW:
            rc = camsensor_mt9m114_preview_reg_setup();
            if(rc < 0)
            {
                return rc;
            }
            msleep(100);
            break;

        default:
            return rc;
        } /* rt */
        break; /* case S_UPDATE_PERIODIC: */

    case S_REG_INIT:
        rc = camsensor_mt9m114_initial_reg_setup();
        if(rc < 0)
            return rc;

        msleep(10);
        break; /* case S_REG_INIT: */

    default:
        rc = -EINVAL;
        break;
    } /* switch (rupdate) */

    return rc;
}

static int mt9m114_sensor_open_init(const struct msm_camera_sensor_info *info)
{
    int32_t  rc;

    printk("[camera] mt9m114_sensor_open_init()\n");

    mt9m114_ctrl = kzalloc(sizeof(struct mt9m114_ctrl), GFP_KERNEL);

    if (!mt9m114_ctrl) {
        printk("mt9m114_init failed!\n");
        rc = -ENOMEM;
        goto init_done;
    }

    mt9m114_ctrl->fps_divider = 1 * 0x00000400;
    mt9m114_ctrl->pict_fps_divider = 1 * 0x00000400;
    mt9m114_ctrl->set_test = S_TEST_OFF;
    mt9m114_ctrl->prev_res = S_QTR_SIZE;
    mt9m114_ctrl->pict_res = S_FULL_SIZE;
    MT9M114_CSI_CONFIG = 0;

    if (info)
        mt9m114_ctrl->sensordata = info;


    /***** Power-UP *****/

    //enable power
    mt9m114_power_enable();
    msleep(30);//Cloony refine power-on sequence

    //set MCLK
    msm_camio_clk_rate_set(MT9M114_DEFAULT_CLOCK_RATE);

    /***** Hard Reset *****/
    /*B [bug390] [bug745] refine the gpio setting usage, CCI 20110829 */
    printk("[camera]mt9m114_sensor_open_init, sensor_reset = %d\n", info->sensor_reset);
    gpio_set_value(info->sensor_pwd, 1);
    gpio_set_value(info->sensor_reset, 1);
    msleep(10);
    gpio_set_value(info->sensor_reset, 0);
    msleep(10);
    gpio_set_value(info->sensor_reset, 1);
    /*E [bug390] [bug745] refine the gpio setting usage, CCI 20110829 */
    //Internal boot time
    msleep(50);

    rc = mt9m114_probe_init_sensor(info);
    if (rc < 0)
        goto init_fail;

    if (mt9m114_ctrl->prev_res == S_QTR_SIZE)
        rc = mt9m114_setting(S_REG_INIT, S_RES_PREVIEW);
    else
        rc = mt9m114_setting(S_REG_INIT, S_RES_CAPTURE);

    if (rc < 0) {
        printk("mt9m114_setting failed. rc = %d\n", rc);
        goto init_fail;
    }

    goto init_done;

init_fail:
    printk("[camera] mt9m114_sensor_open_init(): FAIL\n");
    mt9m114_probe_init_done(info);
    kfree(mt9m114_ctrl);

init_done:
    return rc;
}

static int32_t mt9m114_power_down(void)
{
    int32_t rc = 0;
    return rc;
}

static int mt9m114_sensor_release(void)
{
    int rc = -EBADF;

    mutex_lock(&mt9m114_mutex);

    mt9m114_power_down();


    mt9m114_power_disable();
    msleep(10);//Cloony refine power-on sequence
    
    kfree(mt9m114_ctrl);
    mt9m114_ctrl = NULL;

    printk("mt9m114_release completed\n");

    mutex_unlock(&mt9m114_mutex);
    return rc;
}

static int32_t mt9m114_set_brightness (int8_t brightness)
{
    int32_t rc = 0;
    printk("[camera] %s: brightness - %d\n", __func__ , brightness);
    return rc;
}

static int32_t mt9m114_set_effect (int8_t effect)
{
    int32_t rc = 0;

    printk("[camera] %s: effect - %d\n", __func__ , effect);

#ifdef ENABLE_ADJUSTMENT
    switch(effect)
    {
    case CAMERA_EFFECT_SOLARIZE:
        rc = mt9m114_i2c_write_table(&color_effects_solarization[0],
                                     ARRAY_SIZE(color_effects_solarization));
        if (rc < 0)
            goto mt9m114_set_effect_fail;
        break;
	
    case CAMERA_EFFECT_NEGATIVE:
        rc = mt9m114_i2c_write_table(&color_effects_negative[0],
                                     ARRAY_SIZE(color_effects_negative));
        if (rc < 0)
            goto mt9m114_set_effect_fail;
        break;

    case CAMERA_EFFECT_SEPIA:
        rc = mt9m114_i2c_write_table(&color_effects_sepia[0],
                                     ARRAY_SIZE(color_effects_sepia));
        if (rc < 0)
            goto mt9m114_set_effect_fail;
        break;

    case CAMERA_EFFECT_MONO:
        rc = mt9m114_i2c_write_table(&color_effects_mono[0],
                                     ARRAY_SIZE(color_effects_mono));
        if (rc < 0)
            goto mt9m114_set_effect_fail;
        break;

    case CAMERA_EFFECT_AQUA:
        rc = mt9m114_i2c_write_table(&color_effects_aqua[0],
                                     ARRAY_SIZE(color_effects_aqua));
        if (rc < 0)
            goto mt9m114_set_effect_fail;
        break;

    case CAMERA_EFFECT_OFF:
    default:
        rc = mt9m114_i2c_write_table(&color_effects_none[0],
                                     ARRAY_SIZE(color_effects_none));
        if (rc < 0)
            goto mt9m114_set_effect_fail;
        break;
    }

    return rc;

mt9m114_set_effect_fail :
    printk("#### mt9m114_set_effect_fail ####\n");

#endif //ENABLE_ADJUSTMENT

    return rc;
}


//Enable mt9m114 antibanding
static int32_t mt9m114_set_antibanding (int8_t antibanding)
{
    int32_t rc = 0;
    printk("[camera] %s: antibanding - %d\n", __func__ , antibanding);

    switch(antibanding)
    {
    case CAMERA_ANTIBANDING_60HZ:
    case CAMERA_ANTIBANDING_OFF:
        rc = mt9m114_i2c_write_table(&anti_banding_60hz[0],
                                     ARRAY_SIZE(anti_banding_60hz));
        if (rc < 0)
            goto mt9m114_set_antibanding_fail;
        break;

    case CAMERA_ANTIBANDING_50HZ:
        rc = mt9m114_i2c_write_table(&anti_banding_50hz[0],
                                     ARRAY_SIZE(anti_banding_50hz));
        if (rc < 0)
            goto mt9m114_set_antibanding_fail ;
        break;

    case CAMERA_ANTIBANDING_AUTO:
    default:
        goto mt9m114_set_antibanding_fail ;
        break;
    }

    return rc;

mt9m114_set_antibanding_fail :
    printk("#### mt9m114_set_antibanding ####\n");
    return rc;
}

static int32_t mt9m114_set_wb (int8_t wb)
{
    int32_t rc = 0;

    printk("[camera] %s: white banlance - %d\n", __func__ , wb);

#ifdef ENABLE_ADJUSTMENT
    switch(wb)
    {
    case CAMERA_WB_DAYLIGHT:
        rc = mt9m114_i2c_write_table(&white_balance_daylight[0],
                                     ARRAY_SIZE(white_balance_daylight));
        if (rc < 0)
            goto mt9m114_set_WB_fail;
        break;

    case CAMERA_WB_FLUORESCENT:
        rc = mt9m114_i2c_write_table(&white_balance_fluorescent[0],
                                     ARRAY_SIZE(white_balance_fluorescent));
        if (rc < 0)
            goto mt9m114_set_WB_fail;
        break;

    case CAMERA_WB_INCANDESCENT:
        rc = mt9m114_i2c_write_table(&white_balance_incandescent[0],
                                     ARRAY_SIZE(white_balance_incandescent));
        if (rc < 0)
            goto mt9m114_set_WB_fail;
        break;

    case CAMERA_WB_CLOUDY_DAYLIGHT:
        rc = mt9m114_i2c_write_table(&white_balance_cloudy[0],
                                     ARRAY_SIZE(white_balance_cloudy));
        if (rc < 0)
            goto mt9m114_set_WB_fail ;
        break;

    case CAMERA_WB_AUTO:
    default:
        rc = mt9m114_i2c_write_table(&white_balance_auto[0],
                                     ARRAY_SIZE(white_balance_auto));
        if (rc < 0)
            goto mt9m114_set_WB_fail ;
        break;
    }

    return rc;

mt9m114_set_WB_fail :
    printk("#### mt9m114_set_WB_fail ####\n");

#endif //ENABLE_ADJUSTMENT

    return rc;
}

static int32_t mt9m114_set_exposure_compensation(int8_t exposure_compensation)
{
    int32_t rc = 0;

    printk("[camera] %s: exposure compensation - %d\n", __func__ , exposure_compensation);

#ifdef ENABLE_ADJUSTMENT
    switch(exposure_compensation)
    {
    case EXPOSURE_NEGATIVE_2EV:
        rc = mt9m114_i2c_write_table(&exposure_negative_2ev[0],
                                     ARRAY_SIZE(exposure_negative_2ev));
        if(rc < 0)
            goto mt9m114_set_exposure_fail;
        break;

    case EXPOSURE_NEGATIVE_1EV:
        rc = mt9m114_i2c_write_table(&exposure_negative_1ev[0],
                                     ARRAY_SIZE(exposure_negative_1ev));
        if(rc < 0)
            goto mt9m114_set_exposure_fail;
        break;

    case EXPOSURE_POSITIVE_1EV:
        rc = mt9m114_i2c_write_table(&exposure_positive_1ev[0],
                                     ARRAY_SIZE(exposure_positive_1ev));
        if(rc < 0)
            goto mt9m114_set_exposure_fail;
        break;

    case EXPOSURE_POSITIVE_2EV:
        rc = mt9m114_i2c_write_table(&exposure_positive_2ev[0],
                                     ARRAY_SIZE(exposure_positive_2ev));
        if(rc < 0)
            goto mt9m114_set_exposure_fail;
        break;

    case EXPOSURE_DEFAULT_0EV:
    default:
        rc = mt9m114_i2c_write_table(&exposure_default_0ev[0],
                                     ARRAY_SIZE(exposure_default_0ev));
        if(rc < 0)
            goto mt9m114_set_exposure_fail;
        break;
    }

    return rc;

mt9m114_set_exposure_fail :
    printk("#### mt9m114_set_exposure_compensation fail ####\n");
#endif //ENABLE_ADJUSTMENT

    return rc;
}

static int32_t mt9m114_video_config(int mode, int res)
{
    int32_t rc = 0;

    switch (res) {
    case S_QTR_SIZE:
        rc = mt9m114_setting(S_UPDATE_PERIODIC, S_RES_PREVIEW);
        if (rc < 0)
            return rc;
        break;

    case S_FULL_SIZE:
        rc = mt9m114_setting(S_UPDATE_PERIODIC, S_RES_CAPTURE);
        if (rc < 0)
            return rc;
        break;

    default:
        return 0;
    } /* switch */

    mt9m114_ctrl->prev_res = res;
    mt9m114_ctrl->curr_res = res;
    mt9m114_ctrl->sensormode = mode;

    printk("mt9m114_video_config() done!\n");

    return rc;
}

static int32_t mt9m114_snapshot_config(int mode)
{
    int32_t rc = 0;

    rc = mt9m114_setting(S_UPDATE_PERIODIC, S_RES_CAPTURE);
    if (rc < 0)
        return rc;

    mt9m114_ctrl->curr_res = mt9m114_ctrl->pict_res;
    mt9m114_ctrl->sensormode = mode;

    return rc;
}

static int32_t mt9m114_raw_snapshot_config(int mode)
{
    int32_t rc = 0;

    rc = mt9m114_setting(S_UPDATE_PERIODIC, S_RES_CAPTURE);
    if (rc < 0)
        return rc;

    mt9m114_ctrl->curr_res = mt9m114_ctrl->pict_res;
    mt9m114_ctrl->sensormode = mode;

    return rc;
}

static int32_t mt9m114_set_sensor_mode(int mode, int res)
{
    int32_t rc = 0;

    switch (mode) {
    case SENSOR_PREVIEW_MODE:
        rc = mt9m114_video_config(mode, res);
        break;

    case SENSOR_SNAPSHOT_MODE:
        rc = mt9m114_snapshot_config(mode);
        break;

    case SENSOR_RAW_SNAPSHOT_MODE:
        rc = mt9m114_raw_snapshot_config(mode);
        break;

    default:
        rc = -EINVAL;
        break;
    }

    return rc;
}

static int mt9m114_sensor_config(void __user *argp)
{
    struct sensor_cfg_data cdata;
    long   rc = 0;

    if (copy_from_user(&cdata,
                       (void *)argp,
                       sizeof(struct sensor_cfg_data)))
        return -EFAULT;

    mutex_lock(&mt9m114_mutex);

    printk("%s: cfgtype = %d\n", __func__, cdata.cfgtype);
    switch (cdata.cfgtype) {

    case CFG_SET_MODE:
        rc = mt9m114_set_sensor_mode(
                 cdata.mode, cdata.rs);
        break;

    case CFG_PWR_DOWN:
        rc = mt9m114_power_down();
        break;

    case CFG_SET_EFFECT:
        rc = mt9m114_set_effect(
                 cdata.cfg.effect);
        break;

    case CFG_SET_WB:
        rc = mt9m114_set_wb(
                 cdata.cfg.wb);
        break;

    case CFG_SET_BRIGHTNESS:
        rc = mt9m114_set_brightness(
                 cdata.cfg.brightness);
        break;

    case CFG_SET_ANTIBANDING:
        rc = mt9m114_set_antibanding(
                 cdata.cfg.antibanding);
        break;

    case CFG_SET_EXPOSURE_COMPENSATION:
        rc = mt9m114_set_exposure_compensation(
                 cdata.cfg.exposure_compensation);
        break;

    default:
        rc = -EINVAL;
        break;
    }

    mutex_unlock(&mt9m114_mutex);
    return rc;
}

static int mt9m114_sensor_probe(const struct msm_camera_sensor_info *info,
                                struct msm_sensor_ctrl *s)
{
    int rc = 0;

    printk("[camera] mt9m114_sensor_probe()\n");


    /***** Power-UP *****/
    //pull up reset
    /*B [bug390] [bug745] refine the gpio setting usage, CCI 20110829 */
    gpio_tlmm_config(GPIO_CFG(info->sensor_reset,  0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
    gpio_tlmm_config(GPIO_CFG(info->sensor_pwd,  0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
    /*E [bug390][bug745]  refine the gpio setting usage, CCI 20110829 */


    //enable power
    mt9m114_power_enable();
    msleep(30);//Cloony refine power-on sequence

    //set MCLK
    msm_camio_clk_rate_set(MT9M114_DEFAULT_CLOCK_RATE);

    /***** Hard Reset *****/
    /*B [bug390] [bug745] refine the gpio setting usage, CCI 20110829 */
    printk("[camera] sensor_reset = %d\n", info->sensor_reset);
    gpio_set_value(info->sensor_pwd, 1);
    gpio_set_value(info->sensor_reset, 1);
    msleep(10);
    gpio_set_value(info->sensor_reset, 0);
    msleep(10);
    gpio_set_value(info->sensor_reset, 1);
    /*E [bug390] [bug745] refine the gpio setting usage, CCI 20110829 */

    //Internal boot time
    msleep(50);

    printk("[camera] mt9m114_sensor_probe(): i2c_add_driver \n");
    rc = i2c_add_driver(&mt9m114_i2c_driver);
    if (mt9m114_client == NULL)
        printk("[camera] mt9m114_sensor_probe(): i2c_add_driver rc =%d, mt9m114_client=NULL\n",rc);
    else
        printk("[camera] mt9m114_sensor_probe(): i2c_add_driver rc =%d, mt9m114_client not NULL\n",rc);
    
    if (rc < 0 || mt9m114_client == NULL) {
        rc = -ENOTSUPP;
        goto probe_fail;
    }
    printk("[camera] mt9m114_sensor_probe(): i2c_add_driver=%d\n", rc);

    rc = mt9m114_probe_init_sensor(info);
    printk("[camera] mt9m114_sensor_probe(): mt9m114_probe_init_sensor=%d\n", rc);
    if (rc < 0)
        goto probe_fail;

    s->s_init = mt9m114_sensor_open_init;
    s->s_release = mt9m114_sensor_release;
    s->s_config  = mt9m114_sensor_config;

    if( cci_hw_id >= DVT1 ) 
        s->s_mount_angle  = 270;//[bug868] Fix front camera module mount angle for DVT sample,cci 20110908
    else
        s->s_mount_angle  = 0;

    s->s_camera_type = FRONT_CAMERA_2D;
    mt9m114_probe_init_done(info);
    mt9m114_power_disable();
    return rc;

probe_fail:
    mt9m114_power_disable();
    printk("[camera] mt9m114_sensor_probe(): FAIL!!\n");
    return rc;
}

static int __mt9m114_probe(struct platform_device *pdev)
{
    return msm_camera_drv_start(pdev, mt9m114_sensor_probe);
}

static struct platform_driver msm_camera_driver = {
    .probe = __mt9m114_probe,
    .driver = {
        .name = "msm_camera_mt9m114",
        .owner = THIS_MODULE,
    },
};

static int __init mt9m114_init(void)
{
    return platform_driver_register(&msm_camera_driver);
}

module_init(mt9m114_init);


/*===========================================================================
				DEBUG
===========================================================================*/
//#ifdef ENABLE_MT9M114_DEBUG
#if 0
#define GPIO_WEB_CAM_PWD	105
#define GPIO_WEB_CAM_RST	103

// ADB i2c
static int mt9m114_adb_powon_function(const char *val, struct kernel_param *kp)
{
    printk(KERN_ERR " mt9m114_adb_powon_function()\n");
    param_set_int(val, kp);
    mt9m114_power_enable();
    return 0;
}

static int mt9m114_adb_powoff_function(const char *val, struct kernel_param *kp)
{
    printk(KERN_ERR " mt9m114_adb_powoff_function()\n");
    param_set_int(val, kp);
    mt9m114_power_disable();
    return 0;
}

static int mt9m114_adb_gpiohigh_function(const char *val, struct kernel_param *kp)
{
    int rc = 0;
    param_set_int(val, kp);

    printk(KERN_ERR " mt9m114_adb_gpiohigh_function()\n");

    if (board_hw_id == HW_CP16)
    {
        //1. GPIO_CAM_EN_WEB_CAM_CP16
        rc = mt9m114_gpio_set_value(GPIO_CAM_EN_WEB_CAM_CP16, 1);
    }
    else if (board_hw_id == HW_CP18)
    {
        //1. GPIO_CAM_EN_WEB_CAM
        rc = mt9m114_gpio_set_value(GPIO_CAM_EN_WEB_CAM, 1);
    }

    //2. GPIO_WEB_CAM_PWD
    rc = mt9m114_gpio_set_value(GPIO_WEB_CAM_PWD, 1);

    //3. GPIO_WEB_CAM_RST
    rc = mt9m114_gpio_set_value(GPIO_WEB_CAM_RST, 1);

    return 0;
}

static int mt9m114_adb_gpiolow_function(const char *val, struct kernel_param *kp)
{
    int rc = 0;
    param_set_int(val, kp);

    printk(KERN_ERR " mt9m114_adb_gpiolow_function()\n");

    if (board_hw_id == HW_CP16)
    {
        //1. GPIO_CAM_EN_WEB_CAM_CP16
        rc = mt9m114_gpio_set_value(GPIO_CAM_EN_WEB_CAM_CP16, 0);
    }
    else if (board_hw_id == HW_CP18)
    {
        //1. GPIO_CAM_EN_WEB_CAM
        rc = mt9m114_gpio_set_value(GPIO_CAM_EN_WEB_CAM, 0);
    }

    //2. GPIO_WEB_CAM_PWD
    rc = mt9m114_gpio_set_value(GPIO_WEB_CAM_PWD, 0);

    //3. GPIO_WEB_CAM_RST
    rc = mt9m114_gpio_set_value(GPIO_WEB_CAM_RST, 0);

    return 0;
}

static int mt9m114_adb_gpioshow_function(const char *val, struct kernel_param *kp)
{
    int rc = 0;
    param_set_int(val, kp);

    if (board_hw_id == HW_CP16)
    {
        //1. GPIO_CAM_EN_WEB_CAM_CP16
        rc = gpio_request(GPIO_CAM_EN_WEB_CAM_CP16, "mt9m114");

        printk(KERN_ERR " mt9m114_adb_gpioshow_function(): GPIO(%d) request=%d\n", GPIO_CAM_EN_WEB_CAM_CP16, rc);

        if (!rc )
            rc = gpio_get_value(GPIO_CAM_EN_WEB_CAM_CP16);

        printk(KERN_ERR " mt9m114_adb_gpioshow_function(): GPIO(%d)=%d\n", GPIO_CAM_EN_WEB_CAM_CP16, rc);

        gpio_free(GPIO_CAM_EN_WEB_CAM_CP16);
    }
    else if (board_hw_id == HW_CP18)
    {
        //1. GPIO_CAM_EN_WEB_CAM
        rc = gpio_request(GPIO_CAM_EN_WEB_CAM, "mt9m114");

        printk(KERN_ERR " mt9m114_adb_gpioshow_function(): GPIO(%d) request=%d\n", GPIO_CAM_EN_WEB_CAM, rc);

        if (!rc )
            rc = gpio_get_value(GPIO_CAM_EN_WEB_CAM);

        printk(KERN_ERR " mt9m114_adb_gpioshow_function(): GPIO(%d)=%d\n", GPIO_CAM_EN_WEB_CAM, rc);

        gpio_free(GPIO_CAM_EN_WEB_CAM);
    }

    //2. GPIO_WEB_CAM_PWD
    rc = gpio_request(GPIO_WEB_CAM_PWD, "mt9m114");

    printk(KERN_ERR " mt9m114_adb_gpioshow_function(): GPIO(%d) request=%d\n", GPIO_WEB_CAM_PWD, rc);

    if (!rc )
        rc = gpio_get_value(GPIO_WEB_CAM_PWD);

    printk(KERN_ERR " mt9m114_adb_gpioshow_function(): GPIO(%d)=%d\n", GPIO_WEB_CAM_PWD, rc);

    gpio_free(GPIO_WEB_CAM_PWD);


    //3. GPIO_WEB_CAM_RST
    rc = gpio_request(GPIO_WEB_CAM_RST, "mt9m114");

    printk(KERN_ERR " mt9m114_adb_gpioshow_function(): GPIO(%d) request=%d\n", GPIO_WEB_CAM_RST, rc);

    if (!rc )
        rc = gpio_get_value(GPIO_WEB_CAM_RST);

    printk(KERN_ERR " mt9m114_adb_gpioshow_function(): GPIO(%d)=%d\n", GPIO_WEB_CAM_RST, rc);

    gpio_free(GPIO_WEB_CAM_RST);

    return 0;
}


static int powon_reg_addr,powoff_reg_addr, gpiohigh_reg_addr, gpiolow_reg_addr, gpioshow_reg_addr;
module_param_call(on, mt9m114_adb_powon_function, param_get_int, &powon_reg_addr, S_IWUSR | S_IRUGO);
module_param_call(off, mt9m114_adb_powoff_function, param_get_int, &powoff_reg_addr, S_IWUSR | S_IRUGO);
module_param_call(high, mt9m114_adb_gpiohigh_function, param_get_int, &gpiohigh_reg_addr, S_IWUSR | S_IRUGO);
module_param_call(low, mt9m114_adb_gpiolow_function, param_get_int, &gpiolow_reg_addr, S_IWUSR | S_IRUGO);
module_param_call(show, mt9m114_adb_gpioshow_function, param_get_int, &gpioshow_reg_addr, S_IWUSR | S_IRUGO);
#endif

