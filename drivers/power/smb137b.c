/* Copyright (c) 2010-2011 Code Aurora Forum. All rights reserved.
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

#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/i2c/smb137b.h>
#include <linux/power_supply.h>
#include <linux/msm-charger.h>

// modified by yafi-begin
#if defined(ORG_VER)
#else
#include <linux/msm_adc.h>
#include <linux/reboot.h>      /* For kernel_power_off() */
#include <mach/cci_hw_id.h>
#include <linux/wakelock.h>
#include <linux/rtc.h> //for clean buf guard time
#endif
// modified by yafi-end

#define SMB137B_MASK(BITS, POS)  ((unsigned char)(((1 << BITS) - 1) << POS))

#define CHG_CURRENT_REG		0x00
#define FAST_CHG_CURRENT_MASK		SMB137B_MASK(3, 5)
#define PRE_CHG_CURRENT_MASK		SMB137B_MASK(2, 3)
#define TERM_CHG_CURRENT_MASK		SMB137B_MASK(2, 1)

#define INPUT_CURRENT_LIMIT_REG	0x01
#define IN_CURRENT_MASK			SMB137B_MASK(3, 5)
#define IN_CURRENT_LIMIT_EN_BIT		BIT(2)
#define IN_CURRENT_DET_THRESH_MASK	SMB137B_MASK(2, 0)

#define FLOAT_VOLTAGE_REG	0x02
#define STAT_OUT_POLARITY_BIT		BIT(7)
#define FLOAT_VOLTAGE_MASK		SMB137B_MASK(7, 0)

#define CONTROL_A_REG		0x03
#define AUTO_RECHARGE_DIS_BIT		BIT(7)
#define CURR_CYCLE_TERM_BIT		BIT(6)
#define PRE_TO_FAST_V_MASK		SMB137B_MASK(3, 3)
#define TEMP_BEHAV_BIT			BIT(2)
#define THERM_NTC_CURR_MODE_BIT		BIT(1)
#define THERM_NTC_47KOHM_BIT		BIT(0)

#define CONTROL_B_REG		0x04
#define STAT_OUTPUT_MODE_MASK		SMB137B_MASK(2, 6)
#define BATT_OV_ENDS_CYCLE_BIT		BIT(5)
#define AUTO_PRE_TO_FAST_DIS_BIT	BIT(4)
#define SAFETY_TIMER_EN_BIT		BIT(3)
#define OTG_LBR_WD_EN_BIT		BIT(2)
#define CHG_WD_TIMER_EN_BIT		BIT(1)
#define IRQ_OP_MASK			BIT(0)

#define PIN_CTRL_REG		0x05
#define AUTO_CHG_EN_BIT			BIT(7)
#define AUTO_LBR_EN_BIT			BIT(6)
#define OTG_LBR_BIT			BIT(5)
#define I2C_PIN_BIT			BIT(4)
#define PIN_EN_CTRL_MASK		SMB137B_MASK(2, 2)
#define OTG_LBR_PIN_CTRL_MASK		SMB137B_MASK(2, 0)

#define OTG_LBR_CTRL_REG	0x06
#define BATT_MISSING_DET_EN_BIT		BIT(7)
#define AUTO_RECHARGE_THRESH_MASK	BIT(6)
#define USB_DP_DN_DET_EN_MASK		BIT(5)
#define OTG_LBR_BATT_CURRENT_LIMIT_MASK	SMB137B_MASK(2, 3)
#define OTG_LBR_UVLO_THRESH_MASK	SMB137B_MASK(3, 0)

#define FAULT_INTR_REG		0x07
#define SAFETY_TIMER_EXP_MASK		SMB137B_MASK(1, 7)
#define BATT_TEMP_UNSAFE_MASK		SMB137B_MASK(1, 6)
#define INPUT_OVLO_IVLO_MASK		SMB137B_MASK(1, 5)
#define BATT_OVLO_MASK			SMB137B_MASK(1, 4)
#define INTERNAL_OVER_TEMP_MASK		SMB137B_MASK(1, 2)
#define ENTER_TAPER_CHG_MASK		SMB137B_MASK(1, 1)
#define CHG_MASK			SMB137B_MASK(1, 0)

#define CELL_TEMP_MON_REG	0x08
#define THERMISTOR_CURR_MASK		SMB137B_MASK(2, 6)
#define LOW_TEMP_CHG_INHIBIT_MASK	SMB137B_MASK(3, 3)
#define HIGH_TEMP_CHG_INHIBIT_MASK	SMB137B_MASK(3, 0)

#define	SAFETY_TIMER_THERMAL_SHUTDOWN_REG	0x09
#define DCIN_OVLO_SEL_MASK		SMB137B_MASK(2, 7)
#define RELOAD_EN_INPUT_VOLTAGE_MASK	SMB137B_MASK(1, 6)
#define THERM_SHUTDN_EN_MASK		SMB137B_MASK(1, 5)
#define STANDBY_WD_TIMER_EN_MASK		SMB137B_MASK(1, 4)
#define COMPLETE_CHG_TMOUT_MASK		SMB137B_MASK(2, 2)
#define PRE_CHG_TMOUT_MASK		SMB137B_MASK(2, 0)

#define	VSYS_REG	0x0A
#define	VSYS_MASK			SMB137B_MASK(3, 4)

// modified by yafi-begin
#if defined(ORG_VER)
#else
#define	SLAVE_REG	0x0B

#define FAST_CURRENT_950MA 0x80
#define FAST_CURRENT_750MA 0x40
#define INPUT_CURRENT_800MA 0x20
#define TERM_CURRENT_35MA 0x06
#define TERM_CURRENT_50MA 0x00
#endif
// modified by yafi-end

#define IRQ_RESET_REG	0x30

#define COMMAND_A_REG	0x31
#define	VOLATILE_REGS_WRITE_PERM_BIT	BIT(7)
#define	POR_BIT				BIT(6)
#define	FAST_CHG_SETTINGS_BIT		BIT(5)
#define	BATT_CHG_EN_BIT			BIT(4)
#define	USBIN_MODE_500_BIT		BIT(3)
#define	USBIN_MODE_HCMODE_BIT		BIT(2)
#define	OTG_LBR_EN_BIT			BIT(1)
#define	STAT_OE_BIT			BIT(0)

#define STATUS_A_REG	0x32
#define INTERNAL_TEMP_IRQ_STAT		BIT(4)
#define DCIN_OV_IRQ_STAT		BIT(3)
#define DCIN_UV_IRQ_STAT		BIT(2)
#define USBIN_OV_IRQ_STAT		BIT(1)
#define USBIN_UV_IRQ_STAT		BIT(0)

#define STATUS_B_REG	0x33
#define USB_PIN_STAT			BIT(7)
#define USB51_MODE_STAT			BIT(6)
#define USB51_HC_MODE_STAT		BIT(5)
#define INTERNAL_TEMP_LIMIT_B_STAT	BIT(4)
#define DC_IN_OV_STAT			BIT(3)
#define DC_IN_UV_STAT			BIT(2)
#define USB_IN_OV_STAT			BIT(1)
#define USB_IN_UV_STAT			BIT(0)

#define	STATUS_C_REG	0x34
#define AUTO_IN_CURR_LIMIT_MASK		SMB137B_MASK(4, 4)
#define AUTO_IN_CURR_LIMIT_STAT		BIT(3)
#define AUTO_SOURCE_DET_COMP_STAT_MASK	SMB137B_MASK(2, 1)
#define AUTO_SOURCE_DET_RESULT_STAT	BIT(0)

#define	STATUS_D_REG	0x35
#define	VBATT_LESS_THAN_VSYS_STAT	BIT(7)
#define	USB_FAIL_STAT			BIT(6)
#define	BATT_TEMP_STAT_MASK		SMB137B_MASK(2, 4)
#define	INTERNAL_TEMP_LIMIT_STAT	BIT(2)
#define	OTG_LBR_MODE_EN_STAT		BIT(1)
#define	OTG_LBR_VBATT_UVLO_STAT		BIT(0)

#define	STATUS_E_REG	0x36
#define	CHARGE_CYCLE_COUNT_STAT		BIT(7)
#define	CHARGER_TERM_STAT		BIT(6)
#define	SAFETY_TIMER_STAT_MASK		SMB137B_MASK(2, 4)
#define	CHARGER_ERROR_STAT		BIT(3)
#define	CHARGING_STAT_E			SMB137B_MASK(2, 1)
#define	CHARGING_EN			BIT(0)

#define	STATUS_F_REG	0x37
#define	WD_IRQ_ACTIVE_STAT		BIT(7)
#define	OTG_OVERCURRENT_STAT		BIT(6)
#define	BATT_PRESENT_STAT		BIT(4)
#define	BATT_OV_LATCHED_STAT		BIT(3)
#define	CHARGER_OVLO_STAT		BIT(2)
#define	CHARGER_UVLO_STAT		BIT(1)
#define	BATT_LOW_STAT			BIT(0)

#define	STATUS_G_REG	0x38
#define	CHARGE_TIMEOUT_IRQ_STAT		BIT(7)
#define	PRECHARGE_TIMEOUT_IRQ_STAT	BIT(6)
#define	BATT_HOT_IRQ_STAT		BIT(5)
#define	BATT_COLD_IRQ_STAT		BIT(4)
#define	BATT_OV_IRQ_STAT		BIT(3)
#define	TAPER_CHG_IRQ_STAT		BIT(2)
#define	FAST_CHG_IRQ_STAT		BIT(1)
#define	CHARGING_IRQ_STAT		BIT(0)

#define	STATUS_H_REG	0x39
#define	CHARGE_TIMEOUT_STAT		BIT(7)
#define	PRECHARGE_TIMEOUT_STAT		BIT(6)
#define	BATT_HOT_STAT			BIT(5)
#define	BATT_COLD_STAT			BIT(4)
#define	BATT_OV_STAT			BIT(3)
#define	TAPER_CHG_STAT			BIT(2)
#define	FAST_CHG_STAT			BIT(1)
#define	CHARGING_STAT_H			BIT(0)

#define DEV_ID_REG	0x3B

#define COMMAND_B_REG	0x3C
#define	THERM_NTC_CURR_VERRIDE		BIT(7)

// modified by yafi-begin
#if defined(ORG_VER)
#define SMB137B_CHG_PERIOD	((HZ) * 150)
#else
#define SMB137B_CHG_PERIOD	((HZ) * 20)
#define CLEAN_BUF_GUARD_TIME 600  //seconds
#endif
// modified by yafi-end

#define INPUT_CURRENT_REG_DEFAULT	0xE1
#define INPUT_CURRENT_REG_MIN		0x01
#define	COMMAND_A_REG_DEFAULT		0xA0
#define	COMMAND_A_REG_OTG_MODE		0xA2

#define	PIN_CTRL_REG_DEFAULT		0x00
#define	PIN_CTRL_REG_CHG_OFF		0x04

#define	FAST_CHG_E_STATUS 0x2

#define SMB137B_DEFAULT_BATT_RATING   950

// modified by yafi-begin
#if defined(ORG_VER)
#else
#define CONFIG_SMB137B_DEBUG 1
#if(CONFIG_SMB137B_DEBUG)
//    #define PrintLog_DEBUG(fmt, args...)    printk(KERN_INFO "CH(L)=> "pr_fmt(fmt), ##args)
    #define PrintLog_DEBUG(fmt, args...)    pr_debug("CH(L)=> "pr_fmt(fmt), ##args)
#else
    #define PrintLog_DEBUG(fmt, args...)
#endif

#define CONFIG_SMB137B_INFO 1
#if(CONFIG_SMB137B_INFO)
    #define PrintLog_INFO(fmt, args...)    printk(KERN_INFO "CH(L)=> "pr_fmt(fmt), ##args)
#else
    #define PrintLog_INFO(fmt, args...)
#endif
#endif
// modified by yafi-end

struct smb137b_data {
	struct i2c_client *client;
	struct delayed_work charge_work;

	bool charging;
	int chgcurrent;
	int cur_charging_mode;
	int max_system_voltage;
	int min_system_voltage;

	int valid_n_gpio;

	int batt_status;
	int batt_chg_type;
	int batt_present;
	int min_design;
	int max_design;
	int batt_mah_rating;

	int usb_status;

	u8 dev_id_reg;
	struct msm_hardware_charger adapter_hw_chg;

// modified by yafi-begin
#if defined(ORG_VER)
#else
	struct mutex smb137b_reg05_mutex;
	struct delayed_work smb137b_valid_delayed_work;
	struct wake_lock smb137b_valid_wakeup;
	struct delayed_work smb137b_valid_rt_work_queue;
	struct mutex smb137b_avg_sample_vbatt_mutex;
#endif
// modified by yafi-end	
};

static unsigned int disabled;
enum charger_stat {
	SMB137B_ABSENT,
	SMB137B_PRESENT,
	SMB137B_ENUMERATED,
};

static struct smb137b_data *usb_smb137b_chg;

// added by yafi-begin
#if defined(ORG_VER)
#else
static int smb137b_read_reg(struct i2c_client *client, int reg,u8 *val);
static int smb137b_write_reg(struct i2c_client *client, int reg, u8 val);

static void smb137b_dbg_print_status_regs(struct smb137b_data *smb137b_chg);

int smb137b_get_battery_mvolts(void);
#define AVG_SAMPLE_VBAT_NUM 1
static int avg_sample_vbatt_mv[AVG_SAMPLE_VBAT_NUM];
static int avg_sample_vbatt_index = 0;
static bool avg_sample_vbatt_recycle = false;
static int smb137b_get_buffered_battery_mvolts(void);
extern void update_power_supply_batt(void);
extern int last_percent;
#define AVG_SAMPLE_VBAT_PERCENT_NUM 20
extern int avg_sample_vbatt_percent[AVG_SAMPLE_VBAT_PERCENT_NUM];
extern int avg_sample_vbatt_percent_index;
extern bool avg_sample_vbatt_percent_recycle;
extern bool over_reasonable_delta_percent;
static int smb137b_get_battery_temperature(void);
static int smb137b_is_battery_present(void);
static int smb137b_is_battery_temp_within_range(void);
static int smb137b_is_battery_id_valid(void);
// static int smb137b_is_mhl_cable_valid(void);
extern bool msm_adc_inited;
extern void kernel_power_off(void);
extern int get_rtc_timestamp(struct rtc_time* rtc_ptr);


bool is_charging_full = false;
bool is_charging_outof_temp = false;
bool is_charging_not_outof_temp = true;
bool is_charger_inserted = false;

struct rtc_time rtc_suspend_ptr;
struct rtc_time rtc_resume_ptr;
unsigned long rtc_suspend_sec_timestamp;
unsigned long rtc_resume_sec_timestamp;
// extern int min_voltage_mv_DCHG;

static struct msm_battery_gauge smb137b_batt_gauge = {
	.get_battery_mvolts = smb137b_get_buffered_battery_mvolts,
	.get_battery_temperature = smb137b_get_battery_temperature,
	.is_battery_present = smb137b_is_battery_present,
	.is_battery_temp_within_range = smb137b_is_battery_temp_within_range,
	.is_battery_id_valid = smb137b_is_battery_id_valid,
};

static bool gOTG = false;
static bool bnotifyUSB = false;

static int batt_read_adc(int channel, int *mv_reading)
{
	int ret;
	void *h;
	struct adc_chan_result adc_chan_result;
	struct completion  conv_complete_evt;

	pr_debug("%s: called for %d\n", __func__, channel);
//	PrintLog_DEBUG("%s: called for %d\n", __func__, channel);

	if (msm_adc_inited)
	{
		ret = adc_channel_open(channel, &h);
		if (ret) {
			pr_err("%s: couldnt open channel %d ret=%d\n",
						__func__, channel, ret);
			goto out;
		}

		init_completion(&conv_complete_evt);
		ret = adc_channel_request_conv(h, &conv_complete_evt);
		if (ret) {
			pr_err("%s: couldnt request conv channel %d ret=%d\n",
							__func__, channel, ret);
			goto out;
		}
		wait_for_completion(&conv_complete_evt);

		ret = adc_channel_read_result(h, &adc_chan_result);
		if (ret) {
			pr_err("%s: couldnt read result channel %d ret=%d\n",
							__func__, channel, ret);
			goto out;
		}
		ret = adc_channel_close(h);
		if (ret) {
			pr_err("%s: couldnt close channel %d ret=%d\n",
							__func__, channel, ret);
		}
		if (mv_reading)
			*mv_reading = adc_chan_result.measurement;
	}
	else
	{
		pr_err("%s: msm_adc driver haven't init\n", __func__);
		goto out;
	}

	pr_debug("%s: done for %d\n", __func__, channel);
// modified by yafi-begin
#if defined(ORG_VER)
	return adc_chan_result.physical;
#else
#if 0
	if (channel == CHANNEL_ADC_BATT_THERM)
		return adc_chan_result.adc_code;
	else
		return adc_chan_result.physical;
#endif
	return adc_chan_result.physical;
#endif
// modified by yafi-end
out:
	pr_debug("%s: done for %d\n", __func__, channel);
	return -EINVAL;
}

#define SMB137B_COMPLETE_TIMEOUT_382MIN 0x00
#define SMB137B_COMPLETE_TIMEOUT_764MIN 0x04
#define SMB137B_COMPLETE_TIMEOUT_1527MIN 0x08
#define SMB137B_PRECHARGE_TIMEOUT_48MIN 0x00
#define SMB137B_PRECHARGE_TIMEOUT_95MIN 0x01
#define SMB137B_PRECHARGE_TIMEOUT_191MIN 0x02
bool smb137b_disable_charger_timeout(bool disable)
{
	u8 temp;

	if(usb_smb137b_chg)
	{
		if(disable)
		{
			smb137b_read_reg(usb_smb137b_chg->client, SAFETY_TIMER_THERMAL_SHUTDOWN_REG, &temp);
			temp |= COMPLETE_CHG_TMOUT_MASK;
			temp |= PRE_CHG_TMOUT_MASK;
			smb137b_write_reg(usb_smb137b_chg->client, SAFETY_TIMER_THERMAL_SHUTDOWN_REG, temp);
		}
		else
		{
			smb137b_read_reg(usb_smb137b_chg->client, SAFETY_TIMER_THERMAL_SHUTDOWN_REG, &temp);
			temp &= ~COMPLETE_CHG_TMOUT_MASK;
			temp &= ~PRE_CHG_TMOUT_MASK;
			temp |= SMB137B_COMPLETE_TIMEOUT_764MIN;
			temp |= SMB137B_PRECHARGE_TIMEOUT_95MIN;
			smb137b_write_reg(usb_smb137b_chg->client, SAFETY_TIMER_THERMAL_SHUTDOWN_REG, temp);
		}
		return true;
	}
	return false;
}

#define SMB137B_SEQUENTIAL_CHARGING_FULL_COUNT 1
bool smb137b_is_charging_full(void)
{
	u8 temp = 0;
	static unsigned int sequential_charging_full_count = 0;

	if(usb_smb137b_chg)
	{
		mutex_lock(&usb_smb137b_chg->smb137b_reg05_mutex);
		usleep_range(250*1000, 300*1000);
		smb137b_read_reg(usb_smb137b_chg->client, STATUS_E_REG, &temp);
		mutex_unlock(&usb_smb137b_chg->smb137b_reg05_mutex);
		if((temp & CHARGE_CYCLE_COUNT_STAT) && (temp & CHARGER_TERM_STAT))
		{
			sequential_charging_full_count++;
			if (sequential_charging_full_count >= SMB137B_SEQUENTIAL_CHARGING_FULL_COUNT)
			{
				sequential_charging_full_count = 0;
				PrintLog_DEBUG("%s REG36=0x%x, FULL\n", __func__, temp);
				return true;
			}
			else
				return false;
		}
		PrintLog_DEBUG("%s REG36=0x%x, NOT_FULL\n", __func__, temp);
	}
	sequential_charging_full_count = 0;
	return false;
}

#define SMB137B_SEQUENTIAL_CHARGING_OUTOF_TEMP_COUNT 1
bool smb137b_is_charging_outof_temp(void)
{
	u8 temp = 0;
	static unsigned int sequential_charging_outof_temp_count = 0;

	if(usb_smb137b_chg)
	{
		if(smb137b_read_reg(usb_smb137b_chg->client, STATUS_D_REG, &temp))
		{
			sequential_charging_outof_temp_count = 0;
			return false;
		}
		if(((temp & BATT_TEMP_STAT_MASK) == 0x10) || ((temp & BATT_TEMP_STAT_MASK) == 0x20))
		{
			sequential_charging_outof_temp_count++;
			if (sequential_charging_outof_temp_count >= SMB137B_SEQUENTIAL_CHARGING_OUTOF_TEMP_COUNT)
			{
				sequential_charging_outof_temp_count = 0;
				PrintLog_DEBUG("%s REG35=0x%x, OverTEMP\n", __func__, temp);
				return true;
			}
			else
			{
				return false;
			}
		}
		PrintLog_DEBUG("%s REG35=0x%x, NOT_OverTEMP\n", __func__, temp);
	}
	sequential_charging_outof_temp_count = 0;
	return false;
}

#define SMB137B_SEQUENTIAL_CHARGING_NOT_OUTOF_TEMP_COUNT 1
bool smb137b_is_charging_not_outof_temp(void)
{
	u8 temp = 0;
	static unsigned int sequential_charging_not_outof_temp_count = 0;

	if(usb_smb137b_chg)
	{
		if(smb137b_read_reg(usb_smb137b_chg->client, STATUS_D_REG, &temp))
		{
			sequential_charging_not_outof_temp_count = 0;
			return false;
		}
		if((temp & BATT_TEMP_STAT_MASK) == 0x00)
		{
			sequential_charging_not_outof_temp_count++;
			if (sequential_charging_not_outof_temp_count >= SMB137B_SEQUENTIAL_CHARGING_NOT_OUTOF_TEMP_COUNT)
			{
				sequential_charging_not_outof_temp_count = 0;
				PrintLog_DEBUG("%s REG35=0x%x, NOT_OverTEMP\n", __func__, temp);
				return true;
			}
			else
				return false;
		}
		PrintLog_DEBUG("%s REG35=0x%x, OverTEMP\n", __func__, temp);
	}
	sequential_charging_not_outof_temp_count = 0;
	return false;
}

#define SAMPLE_VBAT_NUM_EVERY_TIME 5
#define VBAT_ADC_OFFSET 50 //YaFi-in mv
/* returns voltage in mV */
int smb137b_get_battery_mvolts(void)
{
	int vbatt_mv[SAMPLE_VBAT_NUM_EVERY_TIME], vbatt_mv_buff[SAMPLE_VBAT_NUM_EVERY_TIME], average_vbatt_mv, vbatt_mv_temp, sum_vbatt_mv = 0;
	int i, j, vbatt_mv_count = 0;
	u8 temp = 0;
//	u8 pin_ctl_reg_value;

	if(usb_smb137b_chg)
	{
//		smb137b_dbg_print_status_regs(usb_smb137b_chg);
//		if (usb_smb137b_chg->usb_status == SMB137B_PRESENT)
//		{
#if 0
			if ((usb_smb137b_chg->charging == false) && (!is_charging_full))
			{
				smb137b_read_reg(usb_smb137b_chg->client, COMMAND_A_REG, &temp);
				temp |= VOLATILE_REGS_WRITE_PERM_BIT;
				temp |= FAST_CHG_SETTINGS_BIT;
				temp |= USBIN_MODE_500_BIT;
				smb137b_write_reg(usb_smb137b_chg->client, COMMAND_A_REG, temp);
				smb137b_read_reg(usb_smb137b_chg->client, COMMAND_A_REG, &temp);
			}
#endif
			if (smb137b_read_reg(usb_smb137b_chg->client, COMMAND_A_REG, &temp))
			{
				PrintLog_DEBUG("%s: Read R31 fail\n", __func__);
			}
			else if (!(temp & VOLATILE_REGS_WRITE_PERM_BIT))
			{
				temp |= VOLATILE_REGS_WRITE_PERM_BIT;
				temp |= FAST_CHG_SETTINGS_BIT;
				temp |= USBIN_MODE_500_BIT;
				if (smb137b_write_reg(usb_smb137b_chg->client, COMMAND_A_REG, temp))
				{
					PrintLog_DEBUG("%s: Write R31 fail\n", __func__);
				}
			}
			temp = 0;
//			mutex_lock(&usb_smb137b_chg->smb137b_reg05_mutex);
//			if (smb137b_read_reg(usb_smb137b_chg->client, PIN_CTRL_REG, &temp))
//			{
//				mutex_unlock(&usb_smb137b_chg->smb137b_reg05_mutex);
//				goto out;
//			}
//			pin_ctl_reg_value = temp;
//			temp &= ~(I2C_PIN_BIT);
//			temp &= ~(PIN_EN_CTRL_MASK);
//			temp |= 0x04;
//			if (smb137b_write_reg(usb_smb137b_chg->client, PIN_CTRL_REG, temp))
//			{
//				mutex_unlock(&usb_smb137b_chg->smb137b_reg05_mutex);
//				goto out;
//			}
//			batt_read_adc(CHANNEL_ADC_VBATT, NULL);
//			batt_read_adc(CHANNEL_ADC_VBATT, NULL);
//			usleep_range(300, 400);
			for(i=0; i<SAMPLE_VBAT_NUM_EVERY_TIME; i++)
				vbatt_mv_buff[i] = batt_read_adc(CHANNEL_ADC_VBATT, NULL);
//			if (smb137b_write_reg(usb_smb137b_chg->client, PIN_CTRL_REG, pin_ctl_reg_value))
//				pr_err("%s can't recovery PIN_CTRL_REG", __func__);
//			mutex_unlock(&usb_smb137b_chg->smb137b_reg05_mutex);
//		}
//		else
//		{
//			for(i=0; i<SAMPLE_VBAT_NUM_EVERY_TIME; i++)
//				vbatt_mv_buff[i] = batt_read_adc(CHANNEL_ADC_VBATT, NULL);
//		}
		for(i=0; i<SAMPLE_VBAT_NUM_EVERY_TIME; i++)
		{
			if(vbatt_mv_buff[i] > 0)
			{
				vbatt_mv[vbatt_mv_count] = vbatt_mv_buff[i];
				vbatt_mv_count++;
			}
		}
		if(vbatt_mv_count >= 3)
		{
			for(i=0; i<vbatt_mv_count; i++)
				for(j=0; j<vbatt_mv_count; j++)
					if(vbatt_mv[i] > vbatt_mv[j])
					{
						vbatt_mv_temp = vbatt_mv[i];
						vbatt_mv[i] = vbatt_mv[j];
						vbatt_mv[j] = vbatt_mv_temp;
					}
			for(i=0;i<vbatt_mv_count;i++)
				sum_vbatt_mv += vbatt_mv[i];
			sum_vbatt_mv = sum_vbatt_mv - vbatt_mv[0] - vbatt_mv[vbatt_mv_count-1];
			average_vbatt_mv = sum_vbatt_mv/(vbatt_mv_count-2);
		}
		else if(vbatt_mv_count > 0)
		{
			for(i=0;i<vbatt_mv_count;i++)
				sum_vbatt_mv += vbatt_mv[i];
			average_vbatt_mv = sum_vbatt_mv/vbatt_mv_count;
		}
		else
			goto out;
		
		PrintLog_DEBUG("%s: vbatt_mv_count=%d vbatt_mv=%d\n", __func__, vbatt_mv_count, (average_vbatt_mv+VBAT_ADC_OFFSET));

		mutex_lock(&usb_smb137b_chg->smb137b_avg_sample_vbatt_mutex);
		avg_sample_vbatt_mv[avg_sample_vbatt_index] = average_vbatt_mv;
		if (++avg_sample_vbatt_index >= AVG_SAMPLE_VBAT_NUM)
		{
			avg_sample_vbatt_index = 0;
			avg_sample_vbatt_recycle = true;
		}
		mutex_unlock(&usb_smb137b_chg->smb137b_avg_sample_vbatt_mutex);
#if 0
		sum_vbatt_mv = 0;
		if(avg_sample_vbatt_recycle)
		{	
			for(i=0;i<AVG_SAMPLE_VBAT_NUM;i++)
				sum_vbatt_mv += avg_sample_vbatt_mv[i];
			average_vbatt_mv = sum_vbatt_mv/AVG_SAMPLE_VBAT_NUM;
		}
		else
		{
			for(i=0;i<avg_sample_vbatt_index;i++)
				sum_vbatt_mv += avg_sample_vbatt_mv[i];
			average_vbatt_mv = sum_vbatt_mv/avg_sample_vbatt_index;
		}

		if ((average_vbatt_mv+VBAT_ADC_OFFSET)>4197)
		{
			temp = 0;
			mutex_lock(&usb_smb137b_chg->smb137b_reg05_mutex);
			usleep_range(250*1000, 300*1000);
			smb137b_read_reg(usb_smb137b_chg->client, STATUS_E_REG, &temp);
			mutex_unlock(&usb_smb137b_chg->smb137b_reg05_mutex);
			PrintLog_DEBUG("%s: vbatt_mv=%d\n", __func__, ((temp & CHARGING_STAT_E)>0x04?4199:4198));
			return (temp & CHARGING_STAT_E)>0x04?4199:4198; //YaFi-4199->CV mode, 4198->CC mode.
		}
		else
		{
			PrintLog_DEBUG("%s: vbatt_mv=%d\n", __func__, (average_vbatt_mv+VBAT_ADC_OFFSET));
			return (average_vbatt_mv+VBAT_ADC_OFFSET);
		}
#endif
	}
out:
	/*
	 * return 0 to tell the upper layers
	 * we couldnt read the battery voltage
	 */
	return 0;
}

/* returns voltage in mV */
static int smb137b_get_buffered_battery_mvolts(void) //YaFi-OK
{
	int average_vbatt_mv, sum_vbatt_mv = 0;
	int i;
	u8 temp = 0;

	if(usb_smb137b_chg)
	{
		mutex_lock(&usb_smb137b_chg->smb137b_avg_sample_vbatt_mutex);
		if(avg_sample_vbatt_recycle)
		{	
			for(i=0;i<AVG_SAMPLE_VBAT_NUM;i++)
				sum_vbatt_mv += avg_sample_vbatt_mv[i];
			average_vbatt_mv = sum_vbatt_mv/AVG_SAMPLE_VBAT_NUM;
		}
		else
		{
			if (avg_sample_vbatt_index == 0)
			{
				average_vbatt_mv = sum_vbatt_mv = avg_sample_vbatt_mv[0];
			}
			else
			{
				for(i=0;i<avg_sample_vbatt_index;i++)
					sum_vbatt_mv += avg_sample_vbatt_mv[i];
				average_vbatt_mv = sum_vbatt_mv/avg_sample_vbatt_index;
			}
		}
		mutex_unlock(&usb_smb137b_chg->smb137b_avg_sample_vbatt_mutex);

		if (average_vbatt_mv <= 0)
		{
			return 0;
		}
		else if ((average_vbatt_mv+VBAT_ADC_OFFSET)>4197)
		{
			mutex_lock(&usb_smb137b_chg->smb137b_reg05_mutex);
			usleep_range(250*1000, 300*1000);
			smb137b_read_reg(usb_smb137b_chg->client, STATUS_E_REG, &temp);
			mutex_unlock(&usb_smb137b_chg->smb137b_reg05_mutex);
			PrintLog_DEBUG("%s: vbatt_mv=%d\n", __func__, ((temp & CHARGING_STAT_E)>0x04?4199:4198));
			return (temp & CHARGING_STAT_E)>0x04?4199:4198; //YaFi-4199->CV mode, 4198->CC mode.
		}
		else
		{
			PrintLog_DEBUG("%s: vbatt_mv=%d\n", __func__, (average_vbatt_mv+VBAT_ADC_OFFSET));
			return (average_vbatt_mv+VBAT_ADC_OFFSET);
		}
	}
	else
		return 0;
}

struct Tbat_mv_VS_Tbat_degC
{
	int Tbat_mv;
	int Tbat_degC;
};

static struct Tbat_mv_VS_Tbat_degC Tbat_mv_degC[]=
{
{115, 80}, {120, 79}, {124, 78}, {128, 77}, {132, 76}, 
{136, 75}, {142, 74}, {148, 73}, {153, 72}, {159, 71}, 
{165, 70}, {171, 69}, {177, 68}, {182, 67}, {188, 66}, 
{194, 65}, {201, 64}, {209, 63}, {216, 62}, {224, 61}, 
{231, 60}, {239, 59}, {248, 58}, {256, 57}, {265, 56}, 
{273, 55}, {282, 54}, {291, 53}, {300, 52}, {309, 51}, 
{318, 50}, {329, 49}, {340, 48}, {350, 47}, {361, 46}, 
{372, 45}, {384, 44}, {397, 43}, {409, 42}, {422, 41}, 
{434, 40}, {449, 39}, {464, 38}, {479, 37}, {494, 36}, 
{509, 35}, {526, 34}, {542, 33}, {559, 32}, {575, 31}, 
{592, 30}, {610, 29}, {628, 28}, {647, 27}, {665, 26}, 
{683, 25}, {704, 24}, {724, 23}, {745, 22}, {765, 21}, 
{786, 20}, {808, 19}, {831, 18}, {853, 17}, {876, 16}, 
{898, 15}, {923, 14}, {948, 13}, {973, 12}, {998, 11}, 
{1023, 10}, {1050, 9}, {1076, 8}, {1103, 7}, {1129, 6}, 
{1156, 5}, {1183, 4}, {1211, 3}, {1238, 2}, {1266, 1}, 
{1293, 0}, {1320, -1}, {1347, -2}, {1375, -3}, {1402, -4}, 
{1429, -5}, {1457, -6}, {1485, -7}, {1513, -8}, {1541, -9}, 
{1569, -10}, {1596, -11}, {1622, -12}, {1649, -13}, {1675, -14}, 
{1702, -15}, {1728, -16}, {1753, -17}, {1779, -18}, {1804, -19}, 
{1830, -20}, {1853, -21}, {1876, -22}, {1900, -23}, {1923, -24}, 
{1946, -25}, {1967, -26}, {1987, -27}, {2008, -28}, {2028, -29}, 
{2049, -30}, {2067, -31}, {2085, -32}, {2104, -33}, {2122, -34}, 
{2140, -35},
};

int get_Tbat_degC(int Tbat_mv) //Return degC * 10
{
	int Tbat_mv_degC_count = sizeof(Tbat_mv_degC)/sizeof(Tbat_mv_degC[0]);
	int index_low = 0, index_high = 0, i = 0;

	if (Tbat_mv<Tbat_mv_degC[0].Tbat_mv)
		return Tbat_mv_degC[0].Tbat_degC * 10;
	else if(Tbat_mv>Tbat_mv_degC[Tbat_mv_degC_count-1].Tbat_mv)
		return Tbat_mv_degC[Tbat_mv_degC_count-1].Tbat_degC * 10;
	
	for (i=0; i<Tbat_mv_degC_count; i++)
	{
		if (Tbat_mv >= Tbat_mv_degC[i].Tbat_mv)
		{
			if (Tbat_mv == Tbat_mv_degC[i].Tbat_mv)
				return Tbat_mv_degC[i].Tbat_degC * 10;
			index_low = i;
			continue;
		}
		else
		{
			index_high = i;
			break;
		}
	}
	
	return (Tbat_mv_degC[index_low].Tbat_degC * 10) - 
			(((Tbat_mv - Tbat_mv_degC[index_low].Tbat_mv)*10)/(Tbat_mv_degC[index_high].Tbat_mv - Tbat_mv_degC[index_low].Tbat_mv));
}

#define DEFAULT_TBAT (25*10)
#define REASONABLE_DELTA_TBAT_CELCIUS (20*10)
//#define REASONABLE_DELTA_TBAT_CELCIUS (2)
#define TBAT_ADC_OFFSET_CABLE_INSERTED 20 //YaFi-in mv
#define TBAT_ADC_OFFSET_CABLE_REMOVED 30 //YaFi-in mv
static int smb137b_get_battery_temperature(void) //YaFi-Return degC * 10
{
	int mv_reading = 0;
// modified by yafi-begin
#if defined(ORG_VER)
#else
#if 0
	int adc_reading = 0;
#endif
#endif
// modified by yafi-end
	int Tbatt_celcius = DEFAULT_TBAT;
	static int last_Tbatt_celcius = (-100*10);

// modified by yafi-begin
#if defined(ORG_VER)
		Tbatt_celcius = batt_read_adc(CHANNEL_ADC_BATT_THERM, NULL);
		PrintLog_DEBUG("%s: Tbatt_degC is %d\n", __func__, Tbatt_celcius);
		return Tbatt_celcius;
#else
#if 0
//YaFi-for debug message usage
	adc_reading = batt_read_adc(CHANNEL_ADC_BATT_THERM, &mv_reading);
	PrintLog_DEBUG("%s: Tbatt_ADC is %d, Tbatt_MV is %d\n", __func__, adc_reading, mv_reading);
	return DEFAULT_TBAT;
#endif

//	if((cci_hw_id != EVT0) && (cci_hw_id != EVT1) && (cci_hw_id != EVT2))
	if(1)
	{
		if (batt_read_adc(CHANNEL_ADC_BATT_THERM, &mv_reading) == -EINVAL)
		{
			if (last_Tbatt_celcius == (-100*10))
			{
				Tbatt_celcius = DEFAULT_TBAT;
			}
			else
			{
				Tbatt_celcius = last_Tbatt_celcius;
				PrintLog_DEBUG("%s: ADC fail, %d\n", __func__, Tbatt_celcius);
		}
		}
		else
		{
			if(usb_smb137b_chg)
			{
				if(usb_smb137b_chg->usb_status == SMB137B_PRESENT)
					mv_reading+=TBAT_ADC_OFFSET_CABLE_INSERTED;
				else
					mv_reading+=TBAT_ADC_OFFSET_CABLE_REMOVED;
			}
			Tbatt_celcius = get_Tbat_degC(mv_reading);
			if (last_Tbatt_celcius == (-100*10))
			{
			last_Tbatt_celcius = Tbatt_celcius;
		}
			else
			{
				if((Tbatt_celcius > (last_Tbatt_celcius + REASONABLE_DELTA_TBAT_CELCIUS)) || 
					(Tbatt_celcius < (last_Tbatt_celcius - REASONABLE_DELTA_TBAT_CELCIUS)))
				{
					Tbatt_celcius = last_Tbatt_celcius;
				}
				else
				{
					last_Tbatt_celcius = Tbatt_celcius;
				}
			}
		}
		PrintLog_DEBUG("%s: Tbatt_mv=%d, Tbatt_degC=%d\n", __func__, mv_reading, Tbatt_celcius);
		if((cci_hw_id != EVT0) && (cci_hw_id != EVT1) && (cci_hw_id != EVT2))
			return Tbatt_celcius;
		else
			return Tbatt_celcius/2;
	}
	else
		return DEFAULT_TBAT;
#endif
// modified by yafi-end
}

#define CHECK_BATT_EXIST 0
#define BATT_THERM_OPEN_MV  2180
static int smb137b_is_battery_present(void)
{
// modified by yafi-begin
#if CHECK_BATT_EXIST
	int mv_reading = 0;
	static bool correct_mv_reading = false;

	if((cci_hw_id != EVT0) && (cci_hw_id != EVT1))
	{
		if (batt_read_adc(CHANNEL_ADC_BATT_THERM, &mv_reading) == -EINVAL)
		{
			if (correct_mv_reading == false)
				return 1;
			else
				return 1;
		}
		else
		{
			correct_mv_reading = true;
			pr_debug("%s: Tbat_mv is %d\n", __func__, mv_reading);
			PrintLog_DEBUG("%s: Tbat_mv is %d\n", __func__, mv_reading);
			if (mv_reading > 0 && mv_reading < BATT_THERM_OPEN_MV)
				return 1;
			else
				return 0;
		}
	}
	else
		return 1;
#else
	int ret;
	u8 temp = 0;

	if (!usb_smb137b_chg)
	{
		pr_err("%s Called while charger hasn't init\n", __func__);
		return 1;
	}

	ret = smb137b_read_reg(usb_smb137b_chg->client, STATUS_F_REG, &temp);
	if (ret)
	{
		pr_err("%s couldn't read status f reg %d\n", __func__, ret);
		return 1;
	}
	if (!(temp & BATT_PRESENT_STAT))
		return 1;
	else
		return 0;
#endif
// modified by yafi-end
}

//YaFi-Tbatt_celcius unit is degC * 10
#define BATT_THERM_OPERATIONAL_MAX_CELCIUS 600
#define BATT_THERM_OPERATIONAL_MIN_CELCIUS 0
static int smb137b_is_battery_temp_within_range(void)
{
	int therm_celcius;

	therm_celcius = smb137b_get_battery_temperature();
	PrintLog_DEBUG("%s: Tbatt_celcius is %d\n", __func__, therm_celcius);

// modified by yafi-begin
#if defined(ORG_VER)
	if (therm_celcius > BATT_THERM_OPERATIONAL_MIN_CELCIUS
		&& therm_celcius < BATT_THERM_OPERATIONAL_MAX_CELCIUS)
		return 1;
	else
		return 0;
#else
	return 1;
#endif
// modified by yafi-end
}

#define CHECK_BATT_ID 0
#define BATT_ID_MAX_MV  800
#define BATT_ID_MIN_MV  600
static int smb137b_is_battery_id_valid(void)
{
// modified by yafi-begin
#if CHECK_BATT_ID
	int batt_id_mv = 0;
	static bool correct_mv_reading = false;

	if ((batt_id_mv = batt_read_adc(CHANNEL_ADC_BATT_ID, NULL)) == -EINVAL)
	{

		if (correct_mv_reading == false)
			return 1;
		else

			return 1;
	}
	else
	{
		correct_mv_reading = true;
		pr_debug("%s: IDbat_mv is %d\n", __func__, batt_id_mv);
		PrintLog_DEBUG("%s: IDbat_mv is %d\n", __func__, batt_id_mv);
		if (batt_id_mv > 0 && batt_id_mv > BATT_ID_MIN_MV && batt_id_mv < BATT_ID_MAX_MV)
			return 1;
		else
			return 0;
	}
#else
//	PrintLog_DEBUG("%s\n", __func__);
	return 1;
#endif
// modified by yafi-end
}

#define MHL_CABLE_MAX_MV  800
#define MHL_CABLE_MIN_MV  600
#if 0
static int smb137b_is_mhl_cable_valid(void)
{
	int mhl_cable_mv;

	mhl_cable_mv = batt_read_adc(CHANNEL_ADC_BATT_ID, NULL);
	pr_err("%s: mhl_cable_mv is %d\n", __func__, mhl_cable_mv);

	if (mhl_cable_mv > 0
		&& mhl_cable_mv > MHL_CABLE_MIN_MV
		&& mhl_cable_mv < MHL_CABLE_MAX_MV)
		return 1;

	return 0;
}
#endif
#endif
// added by yafi-end

static int smb137b_read_reg(struct i2c_client *client, int reg,
	u8 *val)
{
	s32 ret;
	struct smb137b_data *smb137b_chg;

	smb137b_chg = i2c_get_clientdata(client);
	ret = i2c_smbus_read_byte_data(smb137b_chg->client, reg);
	if (ret < 0) {
		dev_err(&smb137b_chg->client->dev,
			"i2c read fail: can't read from %02x: %d\n", reg, ret);
		return ret;
	} else
		*val = ret;

	return 0;
}

static int smb137b_write_reg(struct i2c_client *client, int reg,
	u8 val)
{
	s32 ret;
	struct smb137b_data *smb137b_chg;

	smb137b_chg = i2c_get_clientdata(client);
	ret = i2c_smbus_write_byte_data(smb137b_chg->client, reg, val);
	if (ret < 0) {
		dev_err(&smb137b_chg->client->dev,
			"i2c write fail: can't write %02x to %02x: %d\n",
			val, reg, ret);
		return ret;
	}
	return 0;
}

static ssize_t id_reg_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct smb137b_data *smb137b_chg;

	smb137b_chg = i2c_get_clientdata(to_i2c_client(dev));

	return sprintf(buf, "%02x\n", smb137b_chg->dev_id_reg);
}
static DEVICE_ATTR(id_reg, S_IRUGO | S_IWUSR, id_reg_show, NULL);

#define DEBUG

#ifdef DEBUG
static void smb137b_dbg_print_status_regs(struct smb137b_data *smb137b_chg)
{
	int ret;
	u8 temp;

// modified by yafi-begin
#if defined(ORG_VER)	
	ret = smb137b_read_reg(smb137b_chg->client, STATUS_A_REG, &temp);
	dev_dbg(&smb137b_chg->client->dev, "%s A=0x%x\n", __func__, temp);
	ret = smb137b_read_reg(smb137b_chg->client, STATUS_B_REG, &temp);
	dev_dbg(&smb137b_chg->client->dev, "%s B=0x%x\n", __func__, temp);
	ret = smb137b_read_reg(smb137b_chg->client, STATUS_C_REG, &temp);
	dev_dbg(&smb137b_chg->client->dev, "%s C=0x%x\n", __func__, temp);
	ret = smb137b_read_reg(smb137b_chg->client, STATUS_D_REG, &temp);
	dev_dbg(&smb137b_chg->client->dev, "%s D=0x%x\n", __func__, temp);
	ret = smb137b_read_reg(smb137b_chg->client, STATUS_E_REG, &temp);
	dev_dbg(&smb137b_chg->client->dev, "%s E=0x%x\n", __func__, temp);
	ret = smb137b_read_reg(smb137b_chg->client, STATUS_F_REG, &temp);
	dev_dbg(&smb137b_chg->client->dev, "%s F=0x%x\n", __func__, temp);
	ret = smb137b_read_reg(smb137b_chg->client, STATUS_G_REG, &temp);
	dev_dbg(&smb137b_chg->client->dev, "%s G=0x%x\n", __func__, temp);
	ret = smb137b_read_reg(smb137b_chg->client, STATUS_H_REG, &temp);
	dev_dbg(&smb137b_chg->client->dev, "%s H=0x%x\n", __func__, temp);
#else
	ret = smb137b_read_reg(smb137b_chg->client, CHG_CURRENT_REG, &temp);
	PrintLog_DEBUG("REG00=0x%x, ", temp);
	ret = smb137b_read_reg(smb137b_chg->client, INPUT_CURRENT_LIMIT_REG, &temp);
	PrintLog_DEBUG("REG01=0x%x, ", temp);
	ret = smb137b_read_reg(smb137b_chg->client, FLOAT_VOLTAGE_REG, &temp);
	PrintLog_DEBUG("REG02=0x%x, ", temp);
	ret = smb137b_read_reg(smb137b_chg->client, CONTROL_A_REG, &temp);
	PrintLog_DEBUG("REG03=0x%x\n", temp);
	ret = smb137b_read_reg(smb137b_chg->client, CONTROL_B_REG, &temp);
	PrintLog_DEBUG("REG04=0x%x, ", temp);
	mutex_lock(&smb137b_chg->smb137b_reg05_mutex);
	ret = smb137b_read_reg(smb137b_chg->client, PIN_CTRL_REG, &temp);
	mutex_unlock(&smb137b_chg->smb137b_reg05_mutex);
	PrintLog_DEBUG("REG05=0x%x, ", temp);
	ret = smb137b_read_reg(smb137b_chg->client, OTG_LBR_CTRL_REG, &temp);
	PrintLog_DEBUG("REG06=0x%x, ", temp);
	ret = smb137b_read_reg(smb137b_chg->client, FAULT_INTR_REG, &temp);
	PrintLog_DEBUG("REG07=0x%x\n", temp);
	ret = smb137b_read_reg(smb137b_chg->client, CELL_TEMP_MON_REG, &temp);
	PrintLog_DEBUG("REG08=0x%x, ", temp);
	ret = smb137b_read_reg(smb137b_chg->client, SAFETY_TIMER_THERMAL_SHUTDOWN_REG, &temp);
	PrintLog_DEBUG("REG09=0x%x, ", temp);
	ret = smb137b_read_reg(smb137b_chg->client, VSYS_REG, &temp);
	PrintLog_DEBUG("REG0A=0x%x, ", temp);
	ret = smb137b_read_reg(smb137b_chg->client, SLAVE_REG, &temp);
	PrintLog_DEBUG("REG0B=0x%x\n", temp);
//	ret = smb137b_read_reg(smb137b_chg->client, IRQ_RESET_REG, &temp);
	ret = smb137b_read_reg(smb137b_chg->client, COMMAND_A_REG, &temp);
	PrintLog_DEBUG("REG31=0x%x, ", temp);
	ret = smb137b_read_reg(smb137b_chg->client, STATUS_A_REG, &temp);
	PrintLog_DEBUG("REG32=0x%x, ", temp);
	ret = smb137b_read_reg(smb137b_chg->client, STATUS_B_REG, &temp);
	PrintLog_DEBUG("REG33=0x%x, ", temp);
	ret = smb137b_read_reg(smb137b_chg->client, STATUS_C_REG, &temp);
	PrintLog_DEBUG("REG34=0x%x\n", temp);
	ret = smb137b_read_reg(smb137b_chg->client, STATUS_D_REG, &temp);
	PrintLog_DEBUG("REG35=0x%x, ", temp);
	ret = smb137b_read_reg(smb137b_chg->client, STATUS_E_REG, &temp);
	PrintLog_DEBUG("REG36=0x%x, ", temp);
	ret = smb137b_read_reg(smb137b_chg->client, STATUS_F_REG, &temp);
	PrintLog_DEBUG("REG37=0x%x, ", temp);
	ret = smb137b_read_reg(smb137b_chg->client, STATUS_G_REG, &temp);
	PrintLog_DEBUG("REG38=0x%x\n", temp);
	ret = smb137b_read_reg(smb137b_chg->client, STATUS_H_REG, &temp);
	PrintLog_DEBUG("REG39=0x%x, ", temp);
	ret = smb137b_read_reg(smb137b_chg->client, DEV_ID_REG, &temp);
	PrintLog_DEBUG("REG3B=0x%x\n", temp);
	ret = smb137b_read_reg(smb137b_chg->client, COMMAND_B_REG, &temp);
	PrintLog_DEBUG("REG3C=0x%x\n", temp);
#endif
// modified by yafi-end
}
#else
static void smb137b_dbg_print_status_regs(struct smb137b_data *smb137b_chg)
{
}
#endif

static int smb137b_start_charging(struct msm_hardware_charger *hw_chg,
		int chg_voltage, int chg_current)
{
// modified by yafi-begin
#if defined(ORG_VER)	
	int cmd_val = COMMAND_A_REG_DEFAULT;
#else
	int cmd_val = 0xA8;
#endif
// modified by yafi-end
	u8 temp = 0;
	int ret = 0;
	struct smb137b_data *smb137b_chg;

	smb137b_chg = container_of(hw_chg, struct smb137b_data, adapter_hw_chg);
	PrintLog_DEBUG("%s disabled=%d chg_current=%d\n", __func__, disabled, chg_current);
	
// modified by yafi-begin
#if defined(ORG_VER)
#else
	if (chg_current == 500)
		bnotifyUSB = true;
	else if(chg_current == 1800)
		bnotifyUSB = false;
#endif
// modified by yafi-end
	if (disabled) {
		dev_err(&smb137b_chg->client->dev,
			"%s called when disabled\n", __func__);
		goto out;
	}

	if (smb137b_chg->charging == true
		&& smb137b_chg->chgcurrent == chg_current)
		/* we are already charging with the same current*/
		 dev_err(&smb137b_chg->client->dev,
			 "%s charge with same current %d called again\n",
			  __func__, chg_current);

	dev_dbg(&smb137b_chg->client->dev, "%s\n", __func__);
	if (chg_current < 500)
		cmd_val &= ~USBIN_MODE_500_BIT;
	else if (chg_current == 500)
		cmd_val |= USBIN_MODE_500_BIT;
	else
		cmd_val |= USBIN_MODE_HCMODE_BIT;

	smb137b_chg->chgcurrent = chg_current;
	smb137b_chg->cur_charging_mode = cmd_val;

	/* Due to non-volatile reload feature,always enable volatile
	 * mirror writes before modifying any 00h~09h control register.
	 * Current mode needs to be programmed according to what's detected
	 * Otherwise default 100mA mode might cause VOUTL drop and fail
	 * the system in case of dead battery.
	 */
// modified by yafi-begin
#if defined(ORG_VER)
	ret = smb137b_write_reg(smb137b_chg->client,
					COMMAND_A_REG, cmd_val);
#else
	ret = smb137b_write_reg(smb137b_chg->client,
					COMMAND_A_REG, 0xA8);
#endif
// modified by yafi-end
	if (ret) {
		dev_err(&smb137b_chg->client->dev,
			"%s couldn't write to command_reg\n", __func__);
		goto out;
	}
// modified by yafi-begin
#if defined(ORG_VER)
	ret = smb137b_write_reg(smb137b_chg->client,
					PIN_CTRL_REG, PIN_CTRL_REG_DEFAULT);
#else
	mutex_lock(&smb137b_chg->smb137b_reg05_mutex);
	ret = smb137b_write_reg(smb137b_chg->client,
					PIN_CTRL_REG, 0x00);
	mutex_unlock(&smb137b_chg->smb137b_reg05_mutex);
	ret |= smb137b_read_reg(smb137b_chg->client, INPUT_CURRENT_LIMIT_REG, &temp);
	temp |= IN_CURRENT_LIMIT_EN_BIT;
	ret |= smb137b_write_reg(smb137b_chg->client, INPUT_CURRENT_LIMIT_REG, temp);
#endif
// modified by yafi-end
	if (ret) {
		dev_err(&smb137b_chg->client->dev,
			"%s couldn't write to pin ctrl reg\n", __func__);
		goto out;
	}
// modified by yafi-begin
#if defined(ORG_VER)
#else
	ret = smb137b_write_reg(smb137b_chg->client, COMMAND_A_REG, cmd_val);
	if (ret) {
		dev_err(&smb137b_chg->client->dev,
			"%s couldn't write to command_reg\n", __func__);
		goto out;
	}
#endif
// modified by yafi-end
	smb137b_chg->charging = true;
	smb137b_chg->batt_status = POWER_SUPPLY_STATUS_CHARGING;
	smb137b_chg->batt_chg_type = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;

	ret = smb137b_read_reg(smb137b_chg->client, STATUS_E_REG, &temp);
	if (ret) {
		dev_err(&smb137b_chg->client->dev,
			"%s couldn't read status e reg %d\n", __func__, ret);
	} else {
		if (temp & CHARGER_ERROR_STAT) {
			dev_err(&smb137b_chg->client->dev,
				"%s chg error E=0x%x\n", __func__, temp);
			smb137b_dbg_print_status_regs(smb137b_chg);
		}
		if (((temp & CHARGING_STAT_E) >> 1) >= FAST_CHG_E_STATUS)
// modified by yafi-begin
#if defined(ORG_VER)
			smb137b_chg->batt_chg_type
						= POWER_SUPPLY_CHARGE_TYPE_FAST;
#else
		{
			smb137b_chg->batt_chg_type
						= POWER_SUPPLY_CHARGE_TYPE_FAST;
			msm_charger_notify_event(
					&smb137b_chg->adapter_hw_chg,
					CHG_BATT_BEGIN_FAST_CHARGING);
		}
#endif
// modified by yafi-end
	}
	/*schedule charge_work to keep track of battery charging state*/
// modified by yafi-begin
#if defined(ORG_VER)
	schedule_delayed_work(&smb137b_chg->charge_work, SMB137B_CHG_PERIOD);
#else
	schedule_delayed_work(&smb137b_chg->charge_work, ((HZ) * 3));
#endif
// modified by yafi-end

out:
	return ret;
}

static int smb137b_stop_charging(struct msm_hardware_charger *hw_chg)
{
	int ret = 0;
	struct smb137b_data *smb137b_chg;

	smb137b_chg = container_of(hw_chg, struct smb137b_data, adapter_hw_chg);

	dev_dbg(&smb137b_chg->client->dev, "%s\n", __func__);
	PrintLog_DEBUG("%s\n", __func__);
	if (smb137b_chg->charging == false)
		return 0;
	PrintLog_DEBUG("%s\n", __func__);
	smb137b_chg->charging = false;
	smb137b_chg->batt_status = POWER_SUPPLY_STATUS_DISCHARGING;
	smb137b_chg->batt_chg_type = POWER_SUPPLY_CHARGE_TYPE_NONE;

// modified by yafi-begin
#if defined(ORG_VER)
	ret = smb137b_write_reg(smb137b_chg->client, COMMAND_A_REG,
				smb137b_chg->cur_charging_mode);
#else
//	ret = smb137b_write_reg(smb137b_chg->client,
//					COMMAND_A_REG, 0xA8);
	ret = smb137b_write_reg(smb137b_chg->client, COMMAND_A_REG,
				smb137b_chg->cur_charging_mode);
#endif
// modified by yafi-end

	if (ret) {
		dev_err(&smb137b_chg->client->dev,
			"%s couldn't write to command_reg\n", __func__);
		goto out;
	}

// modified by yafi-begin
#if defined(ORG_VER)
	ret = smb137b_write_reg(smb137b_chg->client,
					PIN_CTRL_REG, PIN_CTRL_REG_CHG_OFF);
	if (ret)
		dev_err(&smb137b_chg->client->dev,
			"%s couldn't write to pin ctrl reg\n", __func__);
#else
	if (smb137b_chg->usb_status == SMB137B_PRESENT)
	{
		PrintLog_DEBUG("%s-cable connected\n", __func__);
		if (!is_charging_full)
		{
		PrintLog_DEBUG("%s !is_charging_full\n", __func__);	
		mutex_lock(&smb137b_chg->smb137b_reg05_mutex);
		ret = smb137b_write_reg(smb137b_chg->client,
					PIN_CTRL_REG, PIN_CTRL_REG_CHG_OFF);
		mutex_unlock(&smb137b_chg->smb137b_reg05_mutex);
		if (ret)
			dev_err(&smb137b_chg->client->dev,
				"%s couldn't write to pin ctrl reg\n", __func__);
		}
	}
#endif
// modified by yafi-end

out:
	return ret;
}

static int smb137b_charger_switch(struct msm_hardware_charger *hw_chg)
{
	struct smb137b_data *smb137b_chg;

	smb137b_chg = container_of(hw_chg, struct smb137b_data, adapter_hw_chg);
	dev_dbg(&smb137b_chg->client->dev, "%s\n", __func__);
	PrintLog_DEBUG("%s\n", __func__);
	return 0;
}

// modified by yafi-begin
#if defined(ORG_VER)
#else
static void smb137b_valid_rt_work_handler(struct work_struct *smb137b_work)
{
	struct smb137b_data *smb137b_chg;
	int ret = 0;
	u8 temp = 0;

	PrintLog_INFO("%s VBus changed, gOTG=%d\n", __func__, gOTG);
	smb137b_chg = container_of(smb137b_work, struct smb137b_data, smb137b_valid_rt_work_queue.work);
	ret = smb137b_read_reg(smb137b_chg->client, COMMAND_A_REG, &temp);
	temp |= VOLATILE_REGS_WRITE_PERM_BIT;
	temp |= FAST_CHG_SETTINGS_BIT;
	temp |= USBIN_MODE_500_BIT;
	ret |= smb137b_write_reg(smb137b_chg->client, COMMAND_A_REG, temp);

//	mutex_lock(&usb_smb137b_chg->smb137b_reg05_mutex);
//	ret = smb137b_write_reg(client, PIN_CTRL_REG, 0x00);
//	mutex_unlock(&usb_smb137b_chg->smb137b_reg05_mutex);
//	ret = smb137b_write_reg(client, COMMAND_A_REG, temp);
//	ret = smb137b_read_reg(client, COMMAND_A_REG, &temp);
	
	ret |= smb137b_read_reg(smb137b_chg->client, SAFETY_TIMER_THERMAL_SHUTDOWN_REG, &temp);
	temp |= RELOAD_EN_INPUT_VOLTAGE_MASK;
	ret |= smb137b_write_reg(smb137b_chg->client, SAFETY_TIMER_THERMAL_SHUTDOWN_REG, temp);

// YaFi-Enable Tbatt monitor.
	if((cci_hw_id != EVT0) && (cci_hw_id != EVT1) && (cci_hw_id != EVT2))
//	if((cci_hw_id != EVT0) && (cci_hw_id != EVT1))
//	if(1)
	{
		ret |= smb137b_read_reg(smb137b_chg->client, CELL_TEMP_MON_REG, &temp);
		temp &= ~THERMISTOR_CURR_MASK;
		temp &= ~LOW_TEMP_CHG_INHIBIT_MASK;
		temp &= ~HIGH_TEMP_CHG_INHIBIT_MASK;
		temp |= 0x28; //YaFi-set low temp charger inhibit as -1 degC.
		temp |= 0x02; //YaFi-set high temp charger inhibit as 45 degC.
		ret |= smb137b_write_reg(smb137b_chg->client, CELL_TEMP_MON_REG, temp);
		ret |= smb137b_read_reg(smb137b_chg->client, CONTROL_A_REG, &temp);
		temp &= ~TEMP_BEHAV_BIT;
//		temp &= ~THERM_NTC_CURR_MODE_BIT;
		ret |= smb137b_write_reg(smb137b_chg->client, CONTROL_A_REG, temp);
	}
	else
	{
		ret |= smb137b_write_reg(smb137b_chg->client, CELL_TEMP_MON_REG, 0x07);
		ret |= smb137b_read_reg(smb137b_chg->client, CONTROL_A_REG, &temp);
		temp |= TEMP_BEHAV_BIT;
//		temp &= ~THERM_NTC_CURR_MODE_BIT;
		ret |= smb137b_write_reg(smb137b_chg->client, CONTROL_A_REG, temp);
	}
	ret |= smb137b_read_reg(smb137b_chg->client, COMMAND_B_REG, &temp);
	temp |= THERM_NTC_CURR_VERRIDE;
	ret |= smb137b_write_reg(smb137b_chg->client, COMMAND_B_REG, temp);

#if 0
	ret = smb137b_read_reg(client, INPUT_CURRENT_LIMIT_REG, &temp);
	temp |= IN_CURRENT_LIMIT_EN_BIT;
	ret = smb137b_write_reg(client, INPUT_CURRENT_LIMIT_REG, temp);
#endif

	/* set fast charge current as 950mA, termination current as 50mA */
	ret |= smb137b_read_reg(smb137b_chg->client, CHG_CURRENT_REG, &temp);
	temp &= ~FAST_CHG_CURRENT_MASK;
	temp |= FAST_CURRENT_750MA;
//	temp |= FAST_CURRENT_950MA;
//	temp &= ~TERM_CHG_CURRENT_MASK;
//	temp |= TERM_CURRENT_50MA;
	ret |= smb137b_write_reg(smb137b_chg->client, CHG_CURRENT_REG, temp);

	ret |= smb137b_read_reg(smb137b_chg->client, INPUT_CURRENT_LIMIT_REG, &temp);
	temp &= ~IN_CURRENT_MASK;
	temp |= INPUT_CURRENT_800MA;
	ret |= smb137b_write_reg(smb137b_chg->client, INPUT_CURRENT_LIMIT_REG, temp);

	if (ret)
		schedule_delayed_work(&smb137b_chg->smb137b_valid_rt_work_queue, ((HZ) * 0.1));
}

static void smb137b_valid_delayed_work_handler(struct work_struct *smb137b_work)
{
	int val;
	struct smb137b_data *smb137b_chg;
	int ret = 0;

	PrintLog_INFO("%s VBus changed, gOTG=%d\n", __func__, gOTG);
	smb137b_chg = container_of(smb137b_work, struct smb137b_data,
			smb137b_valid_delayed_work.work);
	pr_debug("%s Cable Detected USB inserted\n", __func__);
	val = gpio_get_value_cansleep(smb137b_chg->valid_n_gpio);
	if (val < 0) {
		PrintLog_INFO("%s gpio_get_value failed for %d ret=%d\n", __func__, smb137b_chg->valid_n_gpio, val);
		return;
	}
	PrintLog_INFO("%s cansleep=%d, usb_status=%d\n", __func__, val, smb137b_chg->usb_status );

//	if (!gOTG)
//	{
	if (val) {
		if (smb137b_chg->usb_status != SMB137B_ABSENT) {
			smb137b_chg->usb_status = SMB137B_ABSENT;
// modified by yafi-begin
#if defined(ORG_VER)
#else
			is_charging_full = false;
			is_charging_outof_temp = false;
			is_charging_not_outof_temp = true;
			is_charger_inserted = false;
//			min_voltage_mv_DCHG = 4200;
			bnotifyUSB = false;
#endif
// modified by yafi-end	
			msm_charger_notify_event(&smb137b_chg->adapter_hw_chg,
					CHG_REMOVED_EVENT);
//			if (0)
//				kernel_power_off();
		}
	} else {
// modified by yafi-begin
#if defined(ORG_VER)
#else
#if 1
		ret = smb137b_write_reg(smb137b_chg->client, PIN_CTRL_REG, 0x00);
//		ret = smb137b_read_reg(client, INPUT_CURRENT_LIMIT_REG, &temp);
//		temp |= IN_CURRENT_LIMIT_EN_BIT;
//		ret = smb137b_write_reg(client, INPUT_CURRENT_LIMIT_REG, temp);
#endif
#endif
// modified by yafi-end
		if (smb137b_chg->usb_status == SMB137B_ABSENT) {
			smb137b_chg->usb_status = SMB137B_PRESENT;
			is_charger_inserted = true;
			msm_charger_notify_event(&smb137b_chg->adapter_hw_chg,
					CHG_INSERTED_EVENT);
		}
	}
//	}
//	enable_irq(smb137b_chg->client->irq);
}
#endif
// modified by yafi-end

// modified by yafi-begin
#if defined(ORG_VER)
static irqreturn_t smb137b_valid_handler(int irq, void *dev_id)
{
	int val;
	struct smb137b_data *smb137b_chg;
	struct i2c_client *client = dev_id;

	smb137b_chg = i2c_get_clientdata(client);

	pr_debug("%s Cable Detected USB inserted\n", __func__);
	/*extra delay needed to allow CABLE_DET_N settling down and debounce
	 before	trying to sample its correct value*/
	usleep_range(1000, 1200);
	val = gpio_get_value_cansleep(smb137b_chg->valid_n_gpio);
	if (val < 0) {
		dev_err(&smb137b_chg->client->dev,
			"%s gpio_get_value failed for %d ret=%d\n", __func__,
			smb137b_chg->valid_n_gpio, val);
		goto err;
	}
	dev_err(&smb137b_chg->client->dev, "%s cansleep=%d, usb_status=%d\n", __func__, val, smb137b_chg->usb_status );

	if (val) {
		if (smb137b_chg->usb_status != SMB137B_ABSENT) {
			smb137b_chg->usb_status = SMB137B_ABSENT;
			msm_charger_notify_event(&smb137b_chg->adapter_hw_chg,
					CHG_REMOVED_EVENT);
		}
	} else {
		if (smb137b_chg->usb_status == SMB137B_ABSENT) {
			smb137b_chg->usb_status = SMB137B_PRESENT;
			msm_charger_notify_event(&smb137b_chg->adapter_hw_chg,
					CHG_INSERTED_EVENT);
		}
	}
err:
	return IRQ_HANDLED;
}
#else
extern bool is_connected_to_usb(void);
static irqreturn_t smb137b_valid_handler(int irq, void *dev_id) //YaFi-OK
{
	struct smb137b_data *smb137b_chg;
	struct i2c_client *client = dev_id;
	smb137b_chg = i2c_get_clientdata(client);

	/* disable irq, this gets enabled in the workqueue */
//	disable_irq_nosync(client->irq);
	wake_lock_timeout(&smb137b_chg->smb137b_valid_wakeup, ((HZ) * 4));
	schedule_delayed_work(&smb137b_chg->smb137b_valid_rt_work_queue, ((HZ) * 0.1));
	if (bnotifyUSB && is_connected_to_usb())
		schedule_delayed_work(&smb137b_chg->smb137b_valid_delayed_work, ((HZ) * 0.2));
	else
		schedule_delayed_work(&smb137b_chg->smb137b_valid_delayed_work, ((HZ) * 2));
	return IRQ_HANDLED;
}
#endif
// modified by yafi-end

// added by yafi-begin
#if defined(ORG_VER)
#else
#if 0
static irqreturn_t smb137b_usbid_handler(int irq, void *dev_id)
{
	struct i2c_client *client = dev_id;
	struct smb137b_platform_data *pdata = client->dev.platform_data;
	static bool bOTG = false;

	PrintLog_DEBUG("%s\n", __func__);
	/*extra delay needed to allow CABLE_DET_N settling down and debounce
	 before	trying to sample its correct value*/
	msleep(500);

	if (!bOTG)
	{
		bOTG = true;
		PrintLog_DEBUG("%s OTG enabled\n", __func__);
		gpio_set_value(pdata->smb137b_batfeten_gpio, 1);
		usleep_range(1000, 1200);
		smb137b_otg_power(1);
	}
	else
	{
		bOTG = false;
		PrintLog_DEBUG("%s OTG disabled\n", __func__);
		gpio_set_value(pdata->smb137b_batfeten_gpio, 0);
		usleep_range(1000, 1200);
		smb137b_otg_power(0);
	}
	return IRQ_HANDLED;
}
#endif

#if 0
static irqreturn_t smb137b_stat_handler(int irq, void *dev_id)
{
	struct smb137b_data *smb137b_chg;
	struct i2c_client *client = dev_id;
	int ret = 0;
	u8 smb137b_stat_D = 0, smb137b_stat_G = 0, smb137b_stat_H = 0;

	smb137b_chg = i2c_get_clientdata(client);

	/*extra delay needed to allow CHARGER_STAT_N settling down and debounce
	 before trying to sample its correct value*/
	msleep(500);

	ret = smb137b_read_reg(client, STATUS_D_REG, &smb137b_stat_D);
	ret = smb137b_read_reg(client, STATUS_G_REG, &smb137b_stat_G);
	ret = smb137b_read_reg(client, STATUS_H_REG, &smb137b_stat_H);

	PrintLog_DEBUG("%s STATUS_D_REG=0x%x\n", __func__, (int)smb137b_stat_D);
	PrintLog_DEBUG("%s STATUS_G_REG=0x%x\n", __func__, (int)smb137b_stat_G);
	PrintLog_DEBUG("%s STATUS_H_REG=0x%x\n", __func__, (int)smb137b_stat_H);
	
	return IRQ_HANDLED;
}
#endif
#endif
// added by yafi-end

#ifdef CONFIG_DEBUG_FS
static struct dentry *dent;
static int debug_fs_otg;
static int otg_get(void *data, u64 *value)
{
	*value = debug_fs_otg;
	return 0;
}
static int otg_set(void *data, u64 value)
{
	smb137b_otg_power(debug_fs_otg);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(smb137b_otg_fops, otg_get, otg_set, "%llu\n");

static void smb137b_create_debugfs_entries(struct smb137b_data *smb137b_chg)
{
	dent = debugfs_create_dir("smb137b", NULL);
	if (dent) {
		debugfs_create_file("otg", 0644, dent, NULL, &smb137b_otg_fops);
	}
}
static void smb137b_destroy_debugfs_entries(void)
{
	if (dent)
		debugfs_remove_recursive(dent);
}
#else
static void smb137b_create_debugfs_entries(struct smb137b_data *smb137b_chg)
{
}
static void smb137b_destroy_debugfs_entries(void)
{
}
#endif

static int set_disable_status_param(const char *val, struct kernel_param *kp)
{
	int ret;

	ret = param_set_int(val, kp);
	if (ret)
		return ret;

	if (usb_smb137b_chg && disabled)
		msm_charger_notify_event(&usb_smb137b_chg->adapter_hw_chg,
				CHG_DONE_EVENT);

	pr_debug("%s disabled =%d\n", __func__, disabled);
	PrintLog_DEBUG("%s disabled =%d\n", __func__, disabled);
	return 0;
}
module_param_call(disabled, set_disable_status_param, param_get_uint,
					&disabled, 0644);
static void smb137b_charge_sm(struct work_struct *smb137b_work)
{
	int ret;
	struct smb137b_data *smb137b_chg;
	u8 temp = 0;
	int notify_batt_changed = 0;

	smb137b_chg = container_of(smb137b_work, struct smb137b_data,
			charge_work.work);

	PrintLog_DEBUG("%s charging=%d outof_temp=%d\n", __func__, smb137b_chg->charging, is_charging_outof_temp);

	/*if not charging, exit smb137b charging state transition*/
// modified by yafi-begin
#if defined(ORG_VER)
	if (!smb137b_chg->charging)
#else
	if (!smb137b_chg->charging && !is_charging_outof_temp)
#endif
// modified by yafi-end
		return;

	dev_dbg(&smb137b_chg->client->dev, "%s\n", __func__);
	PrintLog_DEBUG("%s batt_chg_type=%d\n", __func__, smb137b_chg->batt_chg_type);

//	smb137b_dbg_print_status_regs(smb137b_chg);

	ret = smb137b_read_reg(smb137b_chg->client, STATUS_F_REG, &temp);
	if (ret) {
		dev_err(&smb137b_chg->client->dev,
			"%s couldn't read status f reg %d\n", __func__, ret);
		goto out;
	}
	if (smb137b_chg->batt_present != !(temp & BATT_PRESENT_STAT)) {
		smb137b_chg->batt_present = !(temp & BATT_PRESENT_STAT);
		notify_batt_changed = 1;
	}

	if (!smb137b_chg->batt_present)
		smb137b_chg->batt_chg_type = POWER_SUPPLY_CHARGE_TYPE_NONE;

	if (!smb137b_chg->batt_present && smb137b_chg->charging)
		msm_charger_notify_event(&smb137b_chg->adapter_hw_chg,
				CHG_DONE_EVENT);

	if (smb137b_chg->batt_present
		&& smb137b_chg->charging
		&& smb137b_chg->batt_chg_type
			!= POWER_SUPPLY_CHARGE_TYPE_FAST) {
		ret = smb137b_read_reg(smb137b_chg->client,
						STATUS_E_REG, &temp);
		if (ret) {
			dev_err(&smb137b_chg->client->dev,
				"%s couldn't read cntrl reg\n", __func__);
			goto out;

		} else {
			if (temp & CHARGER_ERROR_STAT) {
				dev_err(&smb137b_chg->client->dev,
					"%s error E=0x%x\n", __func__, temp);
				smb137b_dbg_print_status_regs(smb137b_chg);
			}
			if (((temp & CHARGING_STAT_E) >> 1)
					>= FAST_CHG_E_STATUS) {
				smb137b_chg->batt_chg_type
					= POWER_SUPPLY_CHARGE_TYPE_FAST;
				notify_batt_changed = 1;
				msm_charger_notify_event(
					&smb137b_chg->adapter_hw_chg,
					CHG_BATT_BEGIN_FAST_CHARGING);
			} else {
				smb137b_chg->batt_chg_type
					= POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
			}
		}
	}

// modified by yafi-begin
#if defined(ORG_VER)
#else
	if (!is_charging_full)
	{
		PrintLog_DEBUG("Checking full\n");
		is_charging_full = smb137b_is_charging_full();
		if (is_charging_full)
		{
			msm_charger_notify_event(&smb137b_chg->adapter_hw_chg,
					CHG_DONE_EVENT);
			wake_lock_timeout(&smb137b_chg->smb137b_valid_wakeup, ((HZ) * 4));
		}
	}

	if((cci_hw_id != EVT0) && (cci_hw_id != EVT1) && (cci_hw_id != EVT2))
	{
		if (!is_charging_full && !is_charging_outof_temp && is_charging_not_outof_temp)
		{
			PrintLog_DEBUG("Checking outof temp\n");
			is_charging_outof_temp = smb137b_is_charging_outof_temp();
			if (is_charging_outof_temp)
			{
				is_charging_not_outof_temp = false;
				msm_charger_notify_event(&smb137b_chg->adapter_hw_chg,
						CHG_BATT_TEMP_OUTOFRANGE);
			}
		}
		else if(!is_charging_full && is_charging_outof_temp && !is_charging_not_outof_temp)
		{
			PrintLog_DEBUG("Checking not outof temp\n");
			is_charging_not_outof_temp = smb137b_is_charging_not_outof_temp();
			if (is_charging_not_outof_temp)
			{
				is_charging_outof_temp = false;
				msm_charger_notify_event(&smb137b_chg->adapter_hw_chg,
						CHG_BATT_TEMP_INRANGE);
			}
		}
	}
#endif
// modified by yafi-end

out:
	schedule_delayed_work(&smb137b_chg->charge_work,
					SMB137B_CHG_PERIOD);
}

static int __devinit smb137b_probe(struct i2c_client *client,
				    const struct i2c_device_id *id)
{
	const struct smb137b_platform_data *pdata;
	struct smb137b_data *smb137b_chg;
	int ret = 0;
// modified by yafi-begin
#if defined(ORG_VER)
#else
	u8 temp = 0;
#endif
// modified by yafi-end

	pdata = client->dev.platform_data;

	if (pdata == NULL) {
		dev_err(&client->dev, "%s no platform data\n", __func__);
		ret = -EINVAL;
		goto out;
	}

// added by yafi-begin
#if defined(ORG_VER)
#else
	PrintLog_INFO("%s Begin\n", __func__);
	ret = gpio_request(pdata->smb137b_batfeten_gpio, "BAT_FET_EN");
	if (ret)
	{
		pr_err("Charger(L)=> %s gpio_request failed for BAT_FET_EN ret=%d\n", __func__, ret);
		goto out;
	}
	gpio_tlmm_config(GPIO_CFG(pdata->smb137b_batfeten_gpio, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_16MA), GPIO_CFG_ENABLE);
	gpio_set_value(pdata->smb137b_batfeten_gpio, 0);
	gpio_direction_output(pdata->smb137b_batfeten_gpio, 0);
	gpio_set_value(pdata->smb137b_batfeten_gpio, 0);

	ret = gpio_request(pdata->smb137b_stat_gpio, "smb137b_stat_valid");
	if (ret)
	{
		pr_err("Charger(L)=> %s gpio_request failed for CHARGER_STAT ret=%d\n", __func__, ret);
		goto free_batfeten_gpio;
	}
	gpio_tlmm_config(GPIO_CFG(pdata->smb137b_stat_gpio, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_16MA), GPIO_CFG_ENABLE);
#endif
// added by yafi-end

	if (!i2c_check_functionality(client->adapter,
				I2C_FUNC_SMBUS_BYTE_DATA)) {
		ret = -EIO;
		goto free_stat_gpio;
	}

	smb137b_chg = kzalloc(sizeof(*smb137b_chg), GFP_KERNEL);
	if (!smb137b_chg) {
		ret = -ENOMEM;
		goto free_stat_gpio;
	}

// modified by yafi-begin
#if defined(ORG_VER)	
#else
	mutex_init(&smb137b_chg->smb137b_reg05_mutex);
	mutex_init(&smb137b_chg->smb137b_avg_sample_vbatt_mutex);
	INIT_DELAYED_WORK(&smb137b_chg->smb137b_valid_delayed_work, smb137b_valid_delayed_work_handler);
	wake_lock_init(&smb137b_chg->smb137b_valid_wakeup, WAKE_LOCK_SUSPEND, "smb137b_valid_wakeup");
	INIT_DELAYED_WORK(&smb137b_chg->smb137b_valid_rt_work_queue, smb137b_valid_rt_work_handler);
#endif
// modified by yafi-end

	INIT_DELAYED_WORK(&smb137b_chg->charge_work, smb137b_charge_sm);
	smb137b_chg->client = client;
	smb137b_chg->valid_n_gpio = pdata->valid_n_gpio;
	smb137b_chg->batt_mah_rating = pdata->batt_mah_rating;
	if (smb137b_chg->batt_mah_rating == 0)
		smb137b_chg->batt_mah_rating = SMB137B_DEFAULT_BATT_RATING;

	/*It supports USB-WALL charger and PC USB charger */
	smb137b_chg->adapter_hw_chg.type = CHG_TYPE_USB;
	smb137b_chg->adapter_hw_chg.rating = pdata->batt_mah_rating;
	smb137b_chg->adapter_hw_chg.name = "smb137b-charger";
	smb137b_chg->adapter_hw_chg.start_charging = smb137b_start_charging;
	smb137b_chg->adapter_hw_chg.stop_charging = smb137b_stop_charging;
	smb137b_chg->adapter_hw_chg.charging_switched = smb137b_charger_switch;

	if (pdata->chg_detection_config)
		ret = pdata->chg_detection_config();
	if (ret) {
		dev_err(&client->dev, "%s valid config failed ret=%d\n",
			__func__, ret);
		goto free_smb137b_chg;
	}

	ret = gpio_request(pdata->valid_n_gpio, "smb137b_charger_valid");
	if (ret) {
		dev_err(&client->dev, "%s gpio_request failed for %d ret=%d\n",
			__func__, pdata->valid_n_gpio, ret);
		goto free_smb137b_chg;
	}
// added by yafi-begin
#if defined(ORG_VER)
#else
	ret = gpio_request(pdata->smb137b_batteryid_gpio, "smb137b_usbid_valid");
	if (ret) {
		PrintLog_DEBUG("%s gpio_request(smb137b_batteryid_gpio) fail\n", __func__);
		goto free_valid_n_gpio;
	}
#endif
// added by yafi-end

	i2c_set_clientdata(client, smb137b_chg);

	ret = msm_charger_register(&smb137b_chg->adapter_hw_chg);
	if (ret) {
		dev_err(&client->dev, "%s msm_charger_register\
			failed for ret=%d\n", __func__, ret);
		goto free_valid_gpio;
	}

	ret = set_irq_wake(client->irq, 1);
	if (ret) {
		dev_err(&client->dev, "%s failed for set_irq_wake %d ret =%d\n",
			 __func__, client->irq, ret);
		goto unregister_charger;
	}

	ret = request_threaded_irq(client->irq, NULL,
				   smb137b_valid_handler,
				   IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
				   "smb137b_charger_valid", client);
	if (ret) {
		dev_err(&client->dev,
			"%s request_threaded_irq failed for %d ret =%d\n",
			__func__, client->irq, ret);
		goto disable_irq_wake;
	}

// added by yafi-begin
#if defined(ORG_VER)
#else
#if 0
	ret = request_threaded_irq(pdata->smb137b_batteryid_irq, NULL,
				   smb137b_usbid_handler,
//				   IRQF_TRIGGER_LOW,
				   IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
				   "smb137b_usbid_valid", client);
	if (ret) {
		PrintLog_DEBUG("%s request_threaded_irq(%d) fail %d\n", __func__, pdata->smb137b_batteryid_irq, ret);
	}
#endif
#endif
// added by yafi-end

// added by yafi-begin
#if defined(ORG_VER)
#else
#if 0
	ret = request_threaded_irq(pdata->smb137b_stat_irq, NULL,
				   smb137b_stat_handler,
				   IRQF_TRIGGER_LOW,
//				   IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
				   "smb137b_stat_valid", client);
	if (ret) {
		dev_err(&client->dev,
			"%s request_threaded_irq failed for smb137b_stat_handler ret =%d\n",
			__func__, ret);
		goto free_valid_irq;
	}
#endif
#endif
// added by yafi-end

	ret = gpio_get_value_cansleep(smb137b_chg->valid_n_gpio);
	if (ret < 0) {
		dev_err(&client->dev,
			"%s gpio_get_value failed for %d ret=%d\n", __func__,
			pdata->valid_n_gpio, ret);
		/* assume absent */
		ret = 1;
	}
	if (!ret) {
		is_charger_inserted = true;
		msm_charger_notify_event(&smb137b_chg->adapter_hw_chg,
			CHG_INSERTED_EVENT);
		smb137b_chg->usb_status = SMB137B_PRESENT;
	}

	/* enable the writes to non-volatile mirrors */
// modified by yafi-begin
#if defined(ORG_VER)
#else
	ret = smb137b_write_reg(client, COMMAND_A_REG, 0xA8);

	ret = smb137b_read_reg(client, SAFETY_TIMER_THERMAL_SHUTDOWN_REG, &temp);
	temp |= RELOAD_EN_INPUT_VOLTAGE_MASK;
	ret = smb137b_write_reg(client, SAFETY_TIMER_THERMAL_SHUTDOWN_REG, temp);

// YaFi-Enable Tbatt monitor.
	PrintLog_INFO("%s cci_hw_id=%x\n", __func__, cci_hw_id);
	if((cci_hw_id != EVT0) && (cci_hw_id != EVT1) && (cci_hw_id != EVT2))
//	if((cci_hw_id != EVT0) && (cci_hw_id != EVT1))
//	if(1)
	{
		ret = smb137b_read_reg(client, CELL_TEMP_MON_REG, &temp);
		temp &= ~THERMISTOR_CURR_MASK;
		temp &= ~LOW_TEMP_CHG_INHIBIT_MASK;
		temp &= ~HIGH_TEMP_CHG_INHIBIT_MASK;
		temp |= 0x28; //YaFi-set low temp charger inhibit as -1 degC.
		temp |= 0x02; //YaFi-set high temp charger inhibit as 45 degC.
		ret = smb137b_write_reg(client, CELL_TEMP_MON_REG, temp);
		ret = smb137b_read_reg(client, CONTROL_A_REG, &temp);
		temp &= ~TEMP_BEHAV_BIT;
//		temp &= ~THERM_NTC_CURR_MODE_BIT;
		ret = smb137b_write_reg(client, CONTROL_A_REG, temp);
	}
	else
	{
		ret = smb137b_write_reg(client, CELL_TEMP_MON_REG, 0x07);
		ret = smb137b_read_reg(client, CONTROL_A_REG, &temp);
		temp |= TEMP_BEHAV_BIT;
//		temp &= ~THERM_NTC_CURR_MODE_BIT;
		ret = smb137b_write_reg(client, CONTROL_A_REG, temp);
	}
	ret = smb137b_read_reg(client, COMMAND_B_REG, &temp);
	temp |= THERM_NTC_CURR_VERRIDE;
	ret = smb137b_write_reg(client, COMMAND_B_REG, temp);

//	ret = smb137b_read_reg(client, INPUT_CURRENT_LIMIT_REG, &temp);
//	temp |= IN_CURRENT_LIMIT_EN_BIT;
//	ret = smb137b_write_reg(client, INPUT_CURRENT_LIMIT_REG, temp);
#endif
// modified by yafi-end

	/* turn off charging */
// modified by yafi-begin
#if defined(ORG_VER)
	ret = smb137b_write_reg(smb137b_chg->client,
					PIN_CTRL_REG, PIN_CTRL_REG_CHG_OFF);
#else
//	ret = smb137b_write_reg(smb137b_chg->client,
//					PIN_CTRL_REG, 0x00);
#endif
// modified by yafi-end

// modified by yafi-begin
#if defined(ORG_VER)
#else
	/* set fast charge current as 950mA, termination current as 50mA */
	ret = smb137b_read_reg(smb137b_chg->client, CHG_CURRENT_REG,
			&temp);
	temp &= ~FAST_CHG_CURRENT_MASK;
	temp |= FAST_CURRENT_750MA;
//	temp |= FAST_CURRENT_950MA;
//	temp &= ~TERM_CHG_CURRENT_MASK;
//	temp |= TERM_CURRENT_50MA;
	ret = smb137b_write_reg(smb137b_chg->client,
					CHG_CURRENT_REG, temp);

	ret = smb137b_read_reg(client, INPUT_CURRENT_LIMIT_REG, &temp);
	temp &= ~IN_CURRENT_MASK;
	temp |= INPUT_CURRENT_800MA;
	ret = smb137b_write_reg(client, INPUT_CURRENT_LIMIT_REG, temp);
#endif
// modified by yafi-end

	ret = smb137b_read_reg(smb137b_chg->client, DEV_ID_REG,
			&smb137b_chg->dev_id_reg);

	ret = device_create_file(&smb137b_chg->client->dev, &dev_attr_id_reg);

	/* TODO read min_design and max_design from chip registers */
	smb137b_chg->min_design = 3200;
	smb137b_chg->max_design = 4200;

	smb137b_chg->batt_status = POWER_SUPPLY_STATUS_DISCHARGING;
	smb137b_chg->batt_chg_type = POWER_SUPPLY_CHARGE_TYPE_NONE;

	device_init_wakeup(&client->dev, 1);

	usb_smb137b_chg = smb137b_chg;

// modified by yafi-begin
#if defined(ORG_VER)
#else
	PrintLog_DEBUG("%s calling bg=0x%x\n", __func__, (int)&smb137b_batt_gauge);
	msm_battery_gauge_register(&smb137b_batt_gauge);
#endif
// modified by yafi-end
	
	smb137b_create_debugfs_entries(smb137b_chg);
	dev_dbg(&client->dev,
		"%s OK device_id = %x chg_state=%d\n", __func__,
		smb137b_chg->dev_id_reg, smb137b_chg->usb_status);
	PrintLog_DEBUG("%s OK device_id=0x%x chg_state=%d\n", __func__,
		smb137b_chg->dev_id_reg, smb137b_chg->usb_status);
	PrintLog_INFO("%s End\n", __func__);
	return 0;

// added by yafi-begin
#if defined(ORG_VER)
#else
// free_valid_irq:
// 	free_irq(client->irq, client);
#endif
// added by yafi-end
disable_irq_wake:
	set_irq_wake(client->irq, 0);
unregister_charger:
	msm_charger_unregister(&smb137b_chg->adapter_hw_chg);
free_valid_gpio:
// added by yafi-begin
#if defined(ORG_VER)
#else
	gpio_free(pdata->smb137b_batteryid_gpio);
free_valid_n_gpio:
#endif
// added by yafi-end
	gpio_free(pdata->valid_n_gpio);
free_smb137b_chg:
	kfree(smb137b_chg);
// added by yafi-begin
#if defined(ORG_VER)
#else
free_stat_gpio:
	gpio_free(pdata->smb137b_stat_gpio);
free_batfeten_gpio:
	gpio_free(pdata->smb137b_batfeten_gpio);
#endif
// added by yafi-end
out:
	return ret;
}

void smb137b_otg_power(int on)
{
// modified by yafi-begin
#if defined(ORG_VER)
	int ret;

	pr_debug("%s Enter on=%d\n", __func__, on);
	if (on) {
		ret = smb137b_write_reg(usb_smb137b_chg->client,
					PIN_CTRL_REG, PIN_CTRL_REG_CHG_OFF);
		if (ret) {
			pr_err("%s turning off charging in pin_ctrl err=%d\n",
								__func__, ret);
			/*
			 * dont change the command register if we cant
			 * overwrite pin control
			 */
			return;
		}

		ret = smb137b_write_reg(usb_smb137b_chg->client,
			COMMAND_A_REG, COMMAND_A_REG_OTG_MODE);
		if (ret)
			pr_err("%s failed turning on OTG mode ret=%d\n",
								__func__, ret);
	} else {
		ret = smb137b_write_reg(usb_smb137b_chg->client,
			COMMAND_A_REG, COMMAND_A_REG_DEFAULT);
		if (ret)
			pr_err("%s failed turning off OTG mode ret=%d\n",
								__func__, ret);
		ret = smb137b_write_reg(usb_smb137b_chg->client,
				PIN_CTRL_REG, PIN_CTRL_REG_DEFAULT);
		if (ret)
			pr_err("%s failed writing to pn_ctrl ret=%d\n",
								__func__, ret);
	}
#else
	int ret;
	static bool bOTG = false;
	u8 temp = 0;

	pr_debug("%s Enter on=%d\n", __func__, on);
	PrintLog_INFO("%s Enter on=%d\n", __func__, on);

	if(usb_smb137b_chg)
	{
		if (on && !bOTG)
		{
			bOTG=true;
			mutex_lock(&usb_smb137b_chg->smb137b_reg05_mutex);
			ret = smb137b_write_reg(usb_smb137b_chg->client,
						PIN_CTRL_REG, PIN_CTRL_REG_CHG_OFF);
			mutex_unlock(&usb_smb137b_chg->smb137b_reg05_mutex);
			if (ret) {
				pr_err("%s turning off charging in pin_ctrl err=%d\n",
									__func__, ret);
				/*
				 * dont change the command register if we cant
				 * overwrite pin control
				 */
				return;
			}
			ret = smb137b_read_reg(usb_smb137b_chg->client, COMMAND_A_REG, &temp);
			temp |= VOLATILE_REGS_WRITE_PERM_BIT;
			temp |= FAST_CHG_SETTINGS_BIT;
			temp |= USBIN_MODE_500_BIT;
			temp |= OTG_LBR_EN_BIT;
			ret = smb137b_write_reg(usb_smb137b_chg->client, COMMAND_A_REG, temp);
			if (ret)
				pr_err("%s failed turning on OTG mode ret=%d\n",
									__func__, ret);
			gOTG = true;
		}
		else if(!on && bOTG)
		{
			bOTG=false;
			ret = smb137b_read_reg(usb_smb137b_chg->client, COMMAND_A_REG, &temp);
			temp |= VOLATILE_REGS_WRITE_PERM_BIT;
			temp |= FAST_CHG_SETTINGS_BIT;
			temp |= USBIN_MODE_500_BIT;
			temp &= ~OTG_LBR_EN_BIT;
			ret = smb137b_write_reg(usb_smb137b_chg->client, COMMAND_A_REG, temp);
			if (ret)
				pr_err("%s failed turning off OTG mode ret=%d\n",
									__func__, ret);
			mutex_lock(&usb_smb137b_chg->smb137b_reg05_mutex);
			ret = smb137b_write_reg(usb_smb137b_chg->client,
					PIN_CTRL_REG, PIN_CTRL_REG_DEFAULT);
			mutex_unlock(&usb_smb137b_chg->smb137b_reg05_mutex);
			if (ret)
				pr_err("%s failed writing to pn_ctrl ret=%d\n",
									__func__, ret);
			gOTG = false;
		}
	}
	else
	{
		pr_err("%s: smb137b driver haven't init\n", __func__);
	}
#endif
// modified by yafi-end
}

static int __devexit smb137b_remove(struct i2c_client *client)
{
	const struct smb137b_platform_data *pdata;
	struct smb137b_data *smb137b_chg = i2c_get_clientdata(client);

	PrintLog_INFO("%s\n", __func__);

	pdata = client->dev.platform_data;
	device_init_wakeup(&client->dev, 0);
	set_irq_wake(client->irq, 0);
	free_irq(client->irq, client);
	gpio_free(pdata->valid_n_gpio);
	cancel_delayed_work_sync(&smb137b_chg->charge_work);
// modified by yafi-begin
#if defined(ORG_VER)
#else
	cancel_delayed_work_sync(&smb137b_chg->smb137b_valid_rt_work_queue);
	cancel_delayed_work_sync(&smb137b_chg->smb137b_valid_delayed_work);
	wake_lock_destroy(&smb137b_chg->smb137b_valid_wakeup);
#endif
// modified by yafi-end	

	msm_charger_notify_event(&smb137b_chg->adapter_hw_chg,
			 CHG_REMOVED_EVENT);
	msm_charger_unregister(&smb137b_chg->adapter_hw_chg);
	smb137b_destroy_debugfs_entries();
	kfree(smb137b_chg);
	return 0;
}

#ifdef CONFIG_PM
static int smb137b_suspend(struct device *dev)
{
// modified by yafi-begin
#if defined(ORG_VER)
#else
	int ret = 0;
	u8 temp = 0;
	int err = 0;	
#endif
// modified by yafi-end
	struct smb137b_data *smb137b_chg = dev_get_drvdata(dev);

	dev_dbg(&smb137b_chg->client->dev, "%s\n", __func__);
	PrintLog_DEBUG("%s\n", __func__);
	if (smb137b_chg->charging)
		return -EBUSY;
// modified by yafi-begin
#if defined(ORG_VER)
#else
	ret = smb137b_read_reg(smb137b_chg->client, COMMAND_B_REG, &temp);
	temp &= ~THERM_NTC_CURR_VERRIDE;
	ret = smb137b_write_reg(smb137b_chg->client, COMMAND_B_REG, temp);

	//for clean buf guard time: START
	err = get_rtc_timestamp(&rtc_suspend_ptr);
	rtc_tm_to_time(&rtc_suspend_ptr, &rtc_suspend_sec_timestamp);

	PrintLog_DEBUG("%s, err = %d, min = %d, sec = %d\n", __func__, err, rtc_suspend_ptr.tm_min, rtc_suspend_ptr.tm_sec);
	//for clean buf guard time: END
#endif
// modified by yafi-end
	return 0;
}

static int smb137b_resume(struct device *dev)
{
// modified by yafi-begin
#if defined(ORG_VER)
#else
	int i = 0;
	int ret = 0;
	u8 temp = 0;
	int err = 0;
	unsigned long timestamp_diff = 0;
#endif
// modified by yafi-end
	struct smb137b_data *smb137b_chg = dev_get_drvdata(dev);

	dev_dbg(&smb137b_chg->client->dev, "%s\n", __func__);
	PrintLog_DEBUG("%s\n", __func__);
// modified by yafi-begin
#if defined(ORG_VER)
#else
	err = get_rtc_timestamp(&rtc_resume_ptr);
	rtc_tm_to_time(&rtc_resume_ptr, &rtc_resume_sec_timestamp);

	if(rtc_resume_sec_timestamp >= rtc_suspend_sec_timestamp)
		timestamp_diff = rtc_resume_sec_timestamp - rtc_suspend_sec_timestamp;

	PrintLog_DEBUG("%s, err = %d, min = %d, sec = %d\n", __func__, err, rtc_resume_ptr.tm_min, rtc_resume_ptr.tm_sec);
	PrintLog_DEBUG("%s, sec_timestamp_diff = %lu\n", __func__, timestamp_diff);

	if(timestamp_diff > CLEAN_BUF_GUARD_TIME)
	{
		last_percent = -1;
		avg_sample_vbatt_percent_index = 0;
		avg_sample_vbatt_percent_recycle = false;
		over_reasonable_delta_percent = false;
		for(i=0;i<AVG_SAMPLE_VBAT_PERCENT_NUM;i++)
			avg_sample_vbatt_percent[i] = 0;
		mutex_lock(&smb137b_chg->smb137b_avg_sample_vbatt_mutex);
		avg_sample_vbatt_index = 0;
		avg_sample_vbatt_recycle = false;
		for(i=0;i<AVG_SAMPLE_VBAT_NUM;i++)
			avg_sample_vbatt_mv[i] = 0;
		mutex_unlock(&smb137b_chg->smb137b_avg_sample_vbatt_mutex);

		PrintLog_DEBUG("%s, Buffered vbatt data is cleared !!!!!!!\n", __func__);
	}

	ret = smb137b_read_reg(smb137b_chg->client, COMMAND_B_REG, &temp);
	temp |= THERM_NTC_CURR_VERRIDE;
	ret = smb137b_write_reg(smb137b_chg->client, COMMAND_B_REG, temp);

	temp = 0;
	ret = smb137b_read_reg(smb137b_chg->client, STATUS_B_REG, &temp);
	if ((temp & USB_IN_UV_STAT) || (ret != 0))
		is_charger_inserted = false;
	else
		is_charger_inserted = true;

	PrintLog_DEBUG("%s, ret = %d, temp = 0x%X, is_charger_insterted = %d\n", __func__, ret, temp, is_charger_inserted);

	update_power_supply_batt();
#endif
// modified by yafi-end
	return 0;
}

// modified by yafi-begin
#if defined(ORG_VER)
#else
static void __devinit smb137b_shutdown(struct i2c_client *client)
{
	int ret = 0;
	u8 temp = 0;

	ret = smb137b_read_reg(client, COMMAND_B_REG, &temp);
	temp &= ~THERM_NTC_CURR_VERRIDE;
	ret = smb137b_write_reg(client, COMMAND_B_REG, temp);
}
#endif
// modified by yafi-end

static const struct dev_pm_ops smb137b_pm_ops = {
	.suspend = smb137b_suspend,
	.resume = smb137b_resume,
};
#endif

static const struct i2c_device_id smb137b_id[] = {
	{"smb137b", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, smb137b_id);

static struct i2c_driver smb137b_driver = {
	.driver = {
		   .name = "smb137b",
		   .owner = THIS_MODULE,
#ifdef CONFIG_PM
		   .pm = &smb137b_pm_ops,
#endif
	},
	.probe = smb137b_probe,
	.remove = __devexit_p(smb137b_remove),
// modified by yafi-begin
#if defined(ORG_VER)
#else
	.shutdown = smb137b_shutdown,
#endif
// modified by yafi-end
	.id_table = smb137b_id,
};

static int __init smb137b_init(void)
{
	PrintLog_INFO("%s Begin\n", __func__);
	return i2c_add_driver(&smb137b_driver);
}

//S:[CHG], Leon, for enable/disable charging by adb shell command
#ifndef ORG_VER
int rw_info_start_charging(int chg_current)
{
	int result = 0;

	if(usb_smb137b_chg)
		result = smb137b_start_charging(&(usb_smb137b_chg->adapter_hw_chg), 5, chg_current);

	return result;
}

int rw_info_stop_charging(void)
{
	int result = 0;

	if(usb_smb137b_chg)
		result = smb137b_stop_charging(&(usb_smb137b_chg->adapter_hw_chg));
	return result;
}
#endif
//E:[CHG], Leon, for enable/disable charging by adb shell command

module_init(smb137b_init);

static void __exit smb137b_exit(void)
{
	return i2c_del_driver(&smb137b_driver);
}
module_exit(smb137b_exit);

MODULE_AUTHOR("Abhijeet Dharmapurikar <adharmap@codeaurora.org>");
MODULE_DESCRIPTION("Driver for SMB137B Charger chip");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("i2c:smb137b");
