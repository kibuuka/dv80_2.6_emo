/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/mfd/pmic8058.h>
#include <linux/interrupt.h>
#include <linux/power_supply.h>
#include <linux/delay.h>
#include <linux/bitops.h>
#include <linux/debugfs.h>
#include <linux/msm-charger.h>
#include <linux/time.h>
#include <linux/slab.h>
#include <linux/wakelock.h>

#include <asm/atomic.h>

#include <mach/msm_hsusb.h>

// modified by yafi-begin
#if defined(ORG_VER)
#else
#include <mach/cci_hw_id.h>
#endif
// modified by yafi-end

#define MSM_CHG_MAX_EVENTS		16
#define CHARGING_TEOC_MS		9000000
#define UPDATE_TIME_MS			60000
#define RESUME_CHECK_PERIOD_MS		60000

#define DEFAULT_BATT_MAX_V		4200
#define DEFAULT_BATT_MIN_V		3200

#define MSM_CHARGER_GAUGE_MISSING_VOLTS 3500
#define MSM_CHARGER_GAUGE_MISSING_TEMP  35

// modified by yafi-begin
#if defined(ORG_VER)
#else
#define WAKE_LOCK_INITIALIZED            (1U << 8)

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

/**
 * enum msm_battery_status
 * @BATT_STATUS_ABSENT: battery not present
 * @BATT_STATUS_ID_INVALID: battery present but the id is invalid
 * @BATT_STATUS_DISCHARGING: battery is present and is discharging
 * @BATT_STATUS_TRKL_CHARGING: battery is being trickle charged
 * @BATT_STATUS_FAST_CHARGING: battery is being fast charged
 * @BATT_STATUS_JUST_FINISHED_CHARGING: just finished charging,
 *		battery is fully charged. Do not begin charging untill the
 *		voltage falls below a threshold to avoid overcharging
 * @BATT_STATUS_TEMPERATURE_OUT_OF_RANGE: battery present,
					no charging, temp is hot/cold
 */
enum msm_battery_status {
	BATT_STATUS_ABSENT,
	BATT_STATUS_ID_INVALID,
	BATT_STATUS_DISCHARGING,
	BATT_STATUS_TRKL_CHARGING,
	BATT_STATUS_FAST_CHARGING,
	BATT_STATUS_JUST_FINISHED_CHARGING,
	BATT_STATUS_TEMPERATURE_OUT_OF_RANGE,
};

struct msm_hardware_charger_priv {
	struct list_head list;
	struct msm_hardware_charger *hw_chg;
	enum msm_hardware_charger_state hw_chg_state;
	unsigned int max_source_current;
	struct power_supply psy;
// modified by yafi-begin
#if defined(ORG_VER)
#else
	struct power_supply psy_ac;
#endif
// modified by yafi-end
};

struct msm_charger_event {
	enum msm_hardware_charger_event event;
	struct msm_hardware_charger *hw_chg;
};

struct msm_charger_mux {
	int inited;
	struct list_head msm_hardware_chargers;
	int count_chargers;
	struct mutex msm_hardware_chargers_lock;

	struct device *dev;

	unsigned int max_voltage;
	unsigned int min_voltage;

	unsigned int safety_time;
	struct delayed_work teoc_work;

	unsigned int update_time;
	int stop_update;
	struct delayed_work update_heartbeat_work;
// modified by yafi-begin
#if defined(ORG_VER)
#else
	spinlock_t system_loading_lock;
#endif
// modified by yafi-end

	struct mutex status_lock;
	enum msm_battery_status batt_status;
	struct msm_hardware_charger_priv *current_chg_priv;
	struct msm_hardware_charger_priv *current_mon_priv;

	unsigned int (*get_batt_capacity_percent) (void);

	struct msm_charger_event *queue;
	int tail;
	int head;
	spinlock_t queue_lock;
	int queue_count;
	struct work_struct queue_work;
	struct workqueue_struct *event_wq_thread;
	struct wake_lock wl;
	struct wake_lock wl_heartbeat;
};

static struct msm_charger_mux msm_chg;

static struct msm_battery_gauge *msm_batt_gauge;

// modified by yafi-begin
#if defined(ORG_VER)
#else
static bool gmsm_charger_timeout_disable = false;
int last_percent = -1;
#define AVG_SAMPLE_VBAT_PERCENT_NUM 20
int avg_sample_vbatt_percent[AVG_SAMPLE_VBAT_PERCENT_NUM];
int avg_sample_vbatt_percent_index = 0;
bool avg_sample_vbatt_percent_recycle = false;
bool over_reasonable_delta_percent = false;
// int min_voltage_mv_DCHG = 4200;
static int gTbat = 25; //YaFi-unit in degC
#define DEFAULT_BASE_LOADING	300
static int gIsystem = DEFAULT_BASE_LOADING; //YaFi-unit in mA
#define DEFAULT_PERCENT 50
static unsigned int gPbat = DEFAULT_PERCENT; //YaFi-unit in percent of battery
extern bool is_charging_full;
extern bool is_charging_outof_temp;
extern bool is_charging_not_outof_temp;
extern bool is_charger_inserted;
extern int cci_charging_boot;
//YaFi-unit in second
#define KEEPFULLCHARGEDTIME (60*1)
static struct timespec gtime_unplugatfullcharged;
static bool inited_gtime_unplugatfullcharged = false;
static bool charging_done_event = false;
static bool charging_remove_event = false;
#endif
// modified by yafi-end

static int is_chg_capable_of_charging(struct msm_hardware_charger_priv *priv)
{
	if (priv->hw_chg_state == CHG_READY_STATE
	    || priv->hw_chg_state == CHG_CHARGING_STATE)
		return 1;

	return 0;
}

static int is_batt_status_capable_of_charging(void)
{
	if (msm_chg.batt_status == BATT_STATUS_ABSENT
	    || msm_chg.batt_status == BATT_STATUS_TEMPERATURE_OUT_OF_RANGE
	    || msm_chg.batt_status == BATT_STATUS_ID_INVALID
	    || msm_chg.batt_status == BATT_STATUS_JUST_FINISHED_CHARGING)
		return 0;
	return 1;
}

static int is_batt_status_charging(void)
{
	if (msm_chg.batt_status == BATT_STATUS_TRKL_CHARGING
	    || msm_chg.batt_status == BATT_STATUS_FAST_CHARGING)
		return 1;
	return 0;
}

static int is_battery_present(void)
{
//	PrintLog_DEBUG("%s msm_batt_gauge02=0x%x\n", __func__, (int)msm_batt_gauge);

	if (msm_batt_gauge && msm_batt_gauge->is_battery_present)
		return msm_batt_gauge->is_battery_present();
	else {
		pr_err("msm-charger: no batt gauge batt=absent\n");
		return 0;
	}
}

static int is_battery_temp_within_range(void)
{
	if (msm_batt_gauge && msm_batt_gauge->is_battery_temp_within_range)
		return msm_batt_gauge->is_battery_temp_within_range();
	else {
		pr_err("msm-charger no batt gauge batt=out_of_temperatur\n");
		return 0;
	}
}

static int is_battery_id_valid(void)
{
	if (msm_batt_gauge && msm_batt_gauge->is_battery_id_valid)
		return msm_batt_gauge->is_battery_id_valid();
	else {
		pr_err("msm-charger no batt gauge batt=id_invalid\n");
		return 0;
	}
}

static int get_prop_battery_mvolts(void)
{
	if (msm_batt_gauge && msm_batt_gauge->get_battery_mvolts)
		return msm_batt_gauge->get_battery_mvolts();
	else {
		pr_err("msm-charger no batt gauge assuming 3.5V\n");
		return MSM_CHARGER_GAUGE_MISSING_VOLTS;
	}
}

static int get_battery_temperature(void)
{
	if (msm_batt_gauge && msm_batt_gauge->get_battery_temperature)
		return msm_batt_gauge->get_battery_temperature();
	else {
		pr_err("msm-charger no batt gauge assuming 35 deg G\n");
		return MSM_CHARGER_GAUGE_MISSING_TEMP;
	}
}

static int get_prop_batt_capacity(void)
{
	if (msm_batt_gauge && msm_batt_gauge->get_batt_remaining_capacity)
		return msm_batt_gauge->get_batt_remaining_capacity();

	return msm_chg.get_batt_capacity_percent();
}

static int get_prop_batt_health(void)
{
	int status = 0;

	if (msm_chg.batt_status == BATT_STATUS_TEMPERATURE_OUT_OF_RANGE)
		status = POWER_SUPPLY_HEALTH_OVERHEAT;
	else
		status = POWER_SUPPLY_HEALTH_GOOD;

	return status;
}

static int get_prop_charge_type(void)
{
	int status = 0;

	if (msm_chg.batt_status == BATT_STATUS_TRKL_CHARGING)
		status = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
	else if (msm_chg.batt_status == BATT_STATUS_FAST_CHARGING)
		status = POWER_SUPPLY_CHARGE_TYPE_FAST;
	else
		status = POWER_SUPPLY_CHARGE_TYPE_NONE;

	return status;
}

static int get_prop_batt_status(void)
{
	int status = 0;

	if (msm_batt_gauge && msm_batt_gauge->get_battery_status) {
		status = msm_batt_gauge->get_battery_status();
		if (status == POWER_SUPPLY_STATUS_CHARGING ||
			status == POWER_SUPPLY_STATUS_FULL ||
			status == POWER_SUPPLY_STATUS_DISCHARGING)
			return status;
	}

	if (is_batt_status_charging())
		status = POWER_SUPPLY_STATUS_CHARGING;
	else if (msm_chg.batt_status ==
		 BATT_STATUS_JUST_FINISHED_CHARGING
			 && msm_chg.current_chg_priv != NULL)
		status = POWER_SUPPLY_STATUS_FULL;
	else
		status = POWER_SUPPLY_STATUS_DISCHARGING;

	return status;
}

 /* This function should only be called within handle_event or resume */
static void update_batt_status(void)
{
	PrintLog_DEBUG("%s batt_status=%d\n", __func__, msm_chg.batt_status);
	
	if (is_battery_present()) {
		if (is_battery_id_valid()) {
			if (msm_chg.batt_status == BATT_STATUS_ABSENT
				|| msm_chg.batt_status
					== BATT_STATUS_ID_INVALID) {
				msm_chg.batt_status = BATT_STATUS_DISCHARGING;
			}
		} else
			msm_chg.batt_status = BATT_STATUS_ID_INVALID;
	 } else
		msm_chg.batt_status = BATT_STATUS_ABSENT;
}

static enum power_supply_property msm_power_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
};

static char *msm_power_supplied_to[] = {
	"battery",
};

static int msm_power_get_property(struct power_supply *psy,
				  enum power_supply_property psp,
				  union power_supply_propval *val)
{
	struct msm_hardware_charger_priv *priv;
// modified by yafi-begin
#if defined(ORG_VER)
#else
	char *pstr_prop = NULL;
	char *pstr_content = NULL;
#endif
// modified by yafi-end

	priv = container_of(psy, struct msm_hardware_charger_priv, psy);
	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
// modified by yafi-begin
#if defined(ORG_VER)
		val->intval = !(priv->hw_chg_state == CHG_ABSENT_STATE);
#else
		if (priv->max_source_current != 1800)
			val->intval = !(priv->hw_chg_state == CHG_ABSENT_STATE);
		else
			val->intval = 0;
		pstr_prop = "USB_PRESENT";
		if(val->intval == 1)
			pstr_content = "PRESENT";
		else
			pstr_content = "ABSENT";
#endif
// modified by yafi-end
		break;
	case POWER_SUPPLY_PROP_ONLINE:
// modified by yafi-begin
#if defined(ORG_VER)
		val->intval = (priv->hw_chg_state == CHG_READY_STATE)
			|| (priv->hw_chg_state == CHG_CHARGING_STATE);
#else
		if (priv->max_source_current != 1800)
			val->intval = !(priv->hw_chg_state == CHG_ABSENT_STATE);
		else
			val->intval = 0;
		pstr_prop = "USB_ONLINE";
		if(val->intval == 1)
			pstr_content = "PRESENT";
		else
			pstr_content = "ABSENT";
#endif
// modified by yafi-end
		break;
	default:
		return -EINVAL;
	}

// modified by yafi-begin
#if defined(ORG_VER)
#else
	switch(psp)
	{
	case POWER_SUPPLY_PROP_ONLINE:
//		PrintLog_INFO("%s=%s\n", pstr_prop, pstr_content);
		PrintLog_DEBUG("%s=%s\n", pstr_prop, pstr_content);
		break;
	default:
		break;
	}
#endif
// modified by yafi-end
	return 0;
}

// modified by yafi-begin
#if defined(ORG_VER)
#else
static int msm_power_get_property_ac(struct power_supply *psy_ac,
				  enum power_supply_property psp,
				  union power_supply_propval *val) //YaFi-OK
{
	struct msm_hardware_charger_priv *priv;
	char *pstr_prop = NULL;
	char *pstr_content = NULL;

	priv = container_of(psy_ac, struct msm_hardware_charger_priv, psy_ac);
	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		if (priv->max_source_current == 1800)
			val->intval = !(priv->hw_chg_state == CHG_ABSENT_STATE);
		else
			val->intval = 0;
		pstr_prop = "AC_PRESENT";
		if(val->intval == 1)
			pstr_content = "PRESENT";
		else
			pstr_content = "ABSENT";
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		if (priv->max_source_current == 1800)
			val->intval = !(priv->hw_chg_state == CHG_ABSENT_STATE);
		else
			val->intval = 0;
		pstr_prop = "AC_ONLINE";
		if(val->intval == 1)
			pstr_content = "PRESENT";
		else
			pstr_content = "ABSENT";
		break;
	default:
		return -EINVAL;
	}

	switch(psp)
	{
	case POWER_SUPPLY_PROP_ONLINE:
//		PrintLog_INFO("%s=%s\n", pstr_prop, pstr_content);
		PrintLog_DEBUG("%s=%s\n", pstr_prop, pstr_content);
		break;
	default:
		break;
	}
	return 0;
}
#endif
// modified by yafi-end

static enum power_supply_property msm_batt_power_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
// modified by yafi-begin
#if defined(ORG_VER)
#else
	POWER_SUPPLY_PROP_TEMP,
#endif
// modified by yafi-end
};

static int msm_batt_power_get_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       union power_supply_propval *val)
{
// modified by yafi-begin
#if defined(ORG_VER)
#else
	char *pstr_prop = NULL;
	char *pstr_content = NULL;
	char string[5];
#endif
// modified by yafi-end

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = get_prop_batt_status();
// modified by yafi-begin
#if defined(ORG_VER)
#else
		pstr_prop = "BAT_STATUS";
		if(val->intval == POWER_SUPPLY_STATUS_CHARGING)
			pstr_content = "CHARGING";
		else if(val->intval == POWER_SUPPLY_STATUS_FULL)
			pstr_content = "FULL";
		else
			pstr_content = "DISCHARGING";
#endif
// modified by yafi-end
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = get_prop_charge_type();
// modified by yafi-begin
#if defined(ORG_VER)
#else
		pstr_prop = "BAT_TYPE";
		if(val->intval == POWER_SUPPLY_CHARGE_TYPE_TRICKLE)
			pstr_content = "TRICKLE";
		else if(val->intval == POWER_SUPPLY_CHARGE_TYPE_FAST)
			pstr_content = "FAST";
		else
			pstr_content = "NONE";
#endif
// modified by yafi-end
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = get_prop_batt_health();
// modified by yafi-begin
#if defined(ORG_VER)
#else
		pstr_prop = "BAT_HEALTH";
		if(val->intval == POWER_SUPPLY_HEALTH_OVERHEAT)
			pstr_content = "OVERHEAT";
		else
			pstr_content = "GOOD";
#endif
// modified by yafi-end
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = !(msm_chg.batt_status == BATT_STATUS_ABSENT);
// modified by yafi-begin
#if defined(ORG_VER)
#else
		pstr_prop = "BAT_PRESENT";
		if(val->intval == 1)
			pstr_content = "PRESENT";
		else
			pstr_content = "ABSENT";
#endif
// modified by yafi-end
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
// modified by yafi-begin
#if defined(ORG_VER)
#else
		pstr_prop = "BAT_TECHNOLOGY";
		pstr_content = "LION";
#endif
// modified by yafi-end
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		val->intval = msm_chg.max_voltage * 1000;
// modified by yafi-begin
#if defined(ORG_VER)
#else
		pstr_prop = "BAT_VOLTAGE_MAX_DESIGN";
		sprintf(string, "%d", val->intval/1000);
		pstr_content = string;
#endif
// modified by yafi-end
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
		val->intval = msm_chg.min_voltage * 1000;
// modified by yafi-begin
#if defined(ORG_VER)
#else
		pstr_prop = "BAT_VOLTAGE_MIN_DESIGN";
		sprintf(string, "%d", val->intval/1000);
		pstr_content = string;
#endif
// modified by yafi-end
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
// modified by yafi-begin
#if defined(ORG_VER)
		val->intval = get_prop_battery_mvolts();
#else
		val->intval = get_prop_battery_mvolts() * 1000;
		pstr_prop = "BAT_VOLTAGE";
		sprintf(string, "%d", val->intval/1000);
		pstr_content = string;
#endif
// modified by yafi-end
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = get_prop_batt_capacity();
// modified by yafi-begin
#if defined(ORG_VER)
#else
		pstr_prop = "BAT_CAPACITY";
		sprintf(string, "%d", val->intval);
		pstr_content = string;
#endif
// modified by yafi-end
		break;
// modified by yafi-begin
#if defined(ORG_VER)
#else
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = get_battery_temperature();
		gTbat = val->intval/10;
		pstr_prop = "BAT_TEMP";
		sprintf(string, "%d", val->intval);
		pstr_content = string;
		break;
#endif
// modified by yafi-end
	default:
		return -EINVAL;
	}

// modified by yafi-begin
#if defined(ORG_VER)
#else
	switch(psp)
	{
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
	case POWER_SUPPLY_PROP_HEALTH:
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
	case POWER_SUPPLY_PROP_CAPACITY:
	case POWER_SUPPLY_PROP_TEMP:
//		PrintLog_INFO("%s=%s\n", pstr_prop, pstr_content);
		PrintLog_DEBUG("%s=%s\n", pstr_prop, pstr_content);
		break;
	default:
		break;
	}
#endif
// modified by yafi-end
	return 0;
}

static struct power_supply msm_psy_batt = {
	.name = "battery",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = msm_batt_power_props,
	.num_properties = ARRAY_SIZE(msm_batt_power_props),
	.get_property = msm_batt_power_get_property,
};

static int usb_chg_current;
static struct msm_hardware_charger_priv *usb_hw_chg_priv;
static void (*notify_vbus_state_func_ptr)(int);
static int usb_notified_of_insertion;

/* this is passed to the hsusb via platform_data msm_otg_pdata */
int msm_charger_register_vbus_sn(void (*callback)(int))
{
	pr_debug(KERN_INFO "%s\n", __func__);
	PrintLog_DEBUG("%s\n", __func__);
	notify_vbus_state_func_ptr = callback;
	return 0;
}

/* this is passed to the hsusb via platform_data msm_otg_pdata */
void msm_charger_unregister_vbus_sn(void (*callback)(int))
{
	pr_debug(KERN_INFO "%s\n", __func__);
	PrintLog_DEBUG("%s\n", __func__);
	notify_vbus_state_func_ptr = NULL;
}

// Bug 909, Luke Implement MHL charging function
char cci_usb_power_plugin = 0;
static void notify_usb_of_the_plugin_event(struct msm_hardware_charger_priv
					   *hw_chg, int plugin)
{
	plugin = !!plugin;
	if (plugin == 1 && usb_notified_of_insertion == 0) {
		usb_notified_of_insertion = 1;
// modified by yafi-begin
#if defined(ORG_VER)
#else
		usb_hw_chg_priv = hw_chg;
#endif
// modified by yafi-end
		if (notify_vbus_state_func_ptr) {
// modified by yafi-begin
#if defined(ORG_VER)
			dev_err(msm_chg.dev, "%s notifying plugin\n", __func__);
#else
			PrintLog_INFO("%s notifying plugin\n", __func__);
#endif
// modified by yafi-end
			(*notify_vbus_state_func_ptr) (plugin);
		} else
			dev_err(msm_chg.dev, "%s unable to notify plugin\n",
				__func__);
// modified by yafi-begin
#if defined(ORG_VER)
		usb_hw_chg_priv = hw_chg;
#else
#endif
// modified by yafi-end
                // Bug 909, Luke Implement MHL charging function
		cci_usb_power_plugin = 1;

	}
	if (plugin == 0 && usb_notified_of_insertion == 1) {
		if (notify_vbus_state_func_ptr) {
// modified by yafi-begin
#if defined(ORG_VER)
			dev_err(msm_chg.dev, "%s notifying unplugin\n",
				__func__);
#else
			PrintLog_INFO("%s notifying unplugin\n", __func__);
#endif
// modified by yafi-end
			(*notify_vbus_state_func_ptr) (plugin);
		} else
			dev_err(msm_chg.dev, "%s unable to notify unplugin\n",
				__func__);
// modified by yafi-begin
#if defined(ORG_VER)
		usb_notified_of_insertion = 0;
		usb_hw_chg_priv = NULL;
#else
		usb_hw_chg_priv = NULL;
		usb_notified_of_insertion = 0;
#endif
// modified by yafi-end

                // Bug 909, Luke Implement MHL charging function
                cci_usb_power_plugin = 0;

	}
}

// modified by yafi-begin
#if defined(ORG_VER)
#else
struct Vbat_mv_VS_Vbat_capacity
{
	unsigned int Vbat_mv;
	unsigned int Vbat_capacity;
};

#define VBAT_ADC_MV_OFFSET 275
#if 0
static struct Vbat_mv_VS_Vbat_capacity Vbat_mv_capacity_CHG_0degC_100mA[]=
{
{2838,1},{2923,2},{3008,3},{3093,4},{3106,5},
{3119,6},{3132,7},{3146,8},{3159,9},{3172,10},
{3185,11},{3198,12},{3211,13},{3224,14},{3237,15},
{3251,16},{3264,17},{3277,18},{3290,19},{3303,20},
{3316,21},{3329,22},{3342,23},{3356,24},{3369,25},
{3382,26},{3395,27},{3408,28},{3421,29},{3434,30},
{3447,31},{3461,32},{3474,33},{3487,34},{3500,35},
{3513,36},{3521,37},{3530,38},{3538,39},{3546,40},
{3555,41},{3563,42},{3571,43},{3580,44},{3588,45},
{3596,46},{3605,47},{3613,48},{3621,49},{3630,50},
{3638,51},{3646,52},{3655,53},{3663,54},{3671,55},
{3680,56},{3688,57},{3696,58},{3705,59},{3713,60},
{3721,61},{3730,62},{3738,63},{3746,64},{3755,65},
{3763,66},{3771,67},{3780,68},{3788,69},{3796,70},
{3804,71},{3813,72},{3821,73},{3829,74},{3838,75},
{3846,76},{3854,77},{3863,78},{3871,79},{3879,80},
{3888,81},{3896,82},{3904,83},{3913,84},{3921,85},
{3929,86},{3938,87},{3946,88},{3954,89},{3963,90},
{3971,91},{3979,92},{3988,93},{3996,94},{4004,95},
{4013,96},{4021,97},{4029,98},{4038,99},{4046,100},
};
#endif
static struct Vbat_mv_VS_Vbat_capacity Vbat_mv_capacity_CHG_0degC_300mA[]=
{
{3648,1},{3714,2},{3764,3},{3803,4},{3831,5},
{3837,6},{3839,7},{3842,8},{3845,9},{3848,10},
{3852,11},{3856,12},{3861,13},{3868,14},{3876,15},
{3883,16},{3890,17},{3896,18},{3902,19},{3910,20},
{3915,21},{3921,22},{3926,23},{3931,24},{3936,25},
{3940,26},{3942,27},{3944,28},{3946,29},{3949,30},
{3951,31},{3953,32},{3955,33},{3956,34},{3958,35},
{3959,36},{3961,37},{3963,38},{3965,39},{3968,40},
{3970,41},{3971,42},{3973,43},{3975,44},{3977,45},
{3979,46},{3982,47},{3984,48},{3986,49},{3989,50},
{3991,51},{3993,52},{3995,53},{3997,54},{4001,55},
{4003,56},{4006,57},{4008,58},{4011,59},{4013,60},
{4016,61},{4018,62},{4021,63},{4025,64},{4028,65},
{4031,66},{4033,67},{4036,68},{4039,69},{4043,70},
{4046,71},{4049,72},{4053,73},{4057,74},{4061,75},
{4065,76},{4069,77},{4072,78},{4076,79},{4081,80},
{4086,81},{4091,82},{4096,83},{4100,84},{4106,85},
{4111,86},{4116,87},{4122,88},{4127,89},{4135,90},
{4141,91},{4147,92},{4153,93},{4159,94},{4167,95},
{4173,96},{4179,97},{4186,98},{4193,99},{4200,100},
};
#if 0
static struct Vbat_mv_VS_Vbat_capacity Vbat_mv_capacity_CHG_0degC_500mA[]=
{
{2838,1},{2923,2},{3008,3},{3093,4},{3106,5},
{3119,6},{3132,7},{3146,8},{3159,9},{3172,10},
{3185,11},{3198,12},{3211,13},{3224,14},{3237,15},
{3251,16},{3264,17},{3277,18},{3290,19},{3303,20},
{3316,21},{3329,22},{3342,23},{3356,24},{3369,25},
{3382,26},{3395,27},{3408,28},{3421,29},{3434,30},
{3447,31},{3461,32},{3474,33},{3487,34},{3500,35},
{3513,36},{3521,37},{3530,38},{3538,39},{3546,40},
{3555,41},{3563,42},{3571,43},{3580,44},{3588,45},
{3596,46},{3605,47},{3613,48},{3621,49},{3630,50},
{3638,51},{3646,52},{3655,53},{3663,54},{3671,55},
{3680,56},{3688,57},{3696,58},{3705,59},{3713,60},
{3721,61},{3730,62},{3738,63},{3746,64},{3755,65},
{3763,66},{3771,67},{3780,68},{3788,69},{3796,70},
{3804,71},{3813,72},{3821,73},{3829,74},{3838,75},
{3846,76},{3854,77},{3863,78},{3871,79},{3879,80},
{3888,81},{3896,82},{3904,83},{3913,84},{3921,85},
{3929,86},{3938,87},{3946,88},{3954,89},{3963,90},
{3971,91},{3979,92},{3988,93},{3996,94},{4004,95},
{4013,96},{4021,97},{4029,98},{4038,99},{4046,100},
};
static struct Vbat_mv_VS_Vbat_capacity Vbat_mv_capacity_CHG_25degC_100mA[]=
{
{3200,1},{3267,2},{3333,3},{3400,4},{3467,5},
{3533,6},{3600,7},{3606,8},{3612,9},{3618,10},
{3624,11},{3630,12},{3636,13},{3642,14},{3648,15},
{3654,16},{3659,17},{3665,18},{3671,19},{3677,20},
{3683,21},{3689,22},{3695,23},{3701,24},{3707,25},
{3713,26},{3719,27},{3725,28},{3731,29},{3737,30},
{3743,31},{3749,32},{3755,33},{3761,34},{3766,35},
{3772,36},{3778,37},{3784,38},{3790,39},{3796,40},
{3802,41},{3808,42},{3814,43},{3820,44},{3826,45},
{3832,46},{3838,47},{3844,48},{3850,49},{3856,50},
{3862,51},{3867,52},{3873,53},{3879,54},{3885,55},
{3891,56},{3897,57},{3903,58},{3909,59},{3915,60},
{3921,61},{3927,62},{3933,63},{3939,64},{3945,65},
{3951,66},{3957,67},{3963,68},{3969,69},{3974,70},
{3980,71},{3986,72},{3992,73},{3998,74},{4004,75},
{4010,76},{4016,77},{4022,78},{4028,79},{4034,80},
{4040,81},{4046,82},{4052,83},{4058,84},{4064,85},
{4070,86},{4076,87},{4081,88},{4087,89},{4093,90},
{4099,91},{4105,92},{4111,93},{4117,94},{4123,95},
{4129,96},{4135,97},{4150,98},{4165,99},{4200,100},
};
#endif
static struct Vbat_mv_VS_Vbat_capacity Vbat_mv_capacity_CHG_25degC_300mA[]=
{
{3500,1},{3586,2},{3649,3},{3697,4},{3722,5},
{3726,6},{3728,7},{3731,8},{3733,9},{3736,10},
{3740,11},{3746,12},{3752,13},{3760,14},{3768,15},
{3776,16},{3783,17},{3791,18},{3797,19},{3803,20},
{3810,21},{3817,22},{3822,23},{3824,24},{3827,25},
{3829,26},{3830,27},{3832,28},{3833,29},{3834,30},
{3834,31},{3835,32},{3836,33},{3838,34},{3839,35},
{3840,36},{3841,37},{3842,38},{3844,39},{3845,40},
{3847,41},{3849,42},{3851,43},{3853,44},{3855,45},
{3858,46},{3860,47},{3863,48},{3866,49},{3870,50},
{3874,51},{3878,52},{3884,53},{3889,54},{3896,55},
{3904,56},{3910,57},{3916,58},{3920,59},{3924,60},
{3928,61},{3932,62},{3937,63},{3941,64},{3946,65},
{3950,66},{3955,67},{3959,68},{3964,69},{3969,70},
{3975,71},{3980,72},{3985,73},{3991,74},{3997,75},
{4004,76},{4010,77},{4016,78},{4023,79},{4029,80},
{4036,81},{4043,82},{4050,83},{4057,84},{4065,85},
{4072,86},{4080,87},{4088,88},{4097,89},{4106,90},
{4114,91},{4123,92},{4132,93},{4142,94},{4151,95},
{4161,96},{4171,97},{4182,98},{4192,99},{4200,100},
};
#if 0
static struct Vbat_mv_VS_Vbat_capacity Vbat_mv_capacity_CHG_25degC_500mA[]=
{
{3200,1},{3267,2},{3333,3},{3400,4},{3467,5},
{3533,6},{3600,7},{3606,8},{3612,9},{3618,10},
{3624,11},{3630,12},{3636,13},{3642,14},{3648,15},
{3654,16},{3659,17},{3665,18},{3671,19},{3677,20},
{3683,21},{3689,22},{3695,23},{3701,24},{3707,25},
{3713,26},{3719,27},{3725,28},{3731,29},{3737,30},
{3743,31},{3749,32},{3755,33},{3761,34},{3766,35},
{3772,36},{3778,37},{3784,38},{3790,39},{3796,40},
{3802,41},{3808,42},{3814,43},{3820,44},{3826,45},
{3832,46},{3838,47},{3844,48},{3850,49},{3856,50},
{3862,51},{3867,52},{3873,53},{3879,54},{3885,55},
{3891,56},{3897,57},{3903,58},{3909,59},{3915,60},
{3921,61},{3927,62},{3933,63},{3939,64},{3945,65},
{3951,66},{3957,67},{3963,68},{3969,69},{3974,70},
{3980,71},{3986,72},{3992,73},{3998,74},{4004,75},
{4010,76},{4016,77},{4022,78},{4028,79},{4034,80},
{4040,81},{4046,82},{4052,83},{4058,84},{4064,85},
{4070,86},{4076,87},{4081,88},{4087,89},{4093,90},
{4099,91},{4105,92},{4111,93},{4117,94},{4123,95},
{4129,96},{4135,97},{4150,98},{4165,99},{4200,100},
};
static struct Vbat_mv_VS_Vbat_capacity Vbat_mv_capacity_CHG_45degC_100mA[]=
{
{3200,1},{3267,2},{3333,3},{3400,4},{3467,5},
{3533,6},{3600,7},{3606,8},{3612,9},{3618,10},
{3624,11},{3630,12},{3636,13},{3642,14},{3648,15},
{3654,16},{3659,17},{3665,18},{3671,19},{3677,20},
{3683,21},{3689,22},{3695,23},{3701,24},{3707,25},
{3713,26},{3719,27},{3725,28},{3731,29},{3737,30},
{3743,31},{3749,32},{3755,33},{3761,34},{3766,35},
{3772,36},{3778,37},{3784,38},{3790,39},{3796,40},
{3802,41},{3808,42},{3814,43},{3820,44},{3826,45},
{3832,46},{3838,47},{3844,48},{3850,49},{3856,50},
{3862,51},{3867,52},{3873,53},{3879,54},{3885,55},
{3891,56},{3897,57},{3903,58},{3909,59},{3915,60},
{3921,61},{3927,62},{3933,63},{3939,64},{3945,65},
{3951,66},{3957,67},{3963,68},{3969,69},{3974,70},
{3980,71},{3986,72},{3992,73},{3998,74},{4004,75},
{4010,76},{4016,77},{4022,78},{4028,79},{4034,80},
{4040,81},{4046,82},{4052,83},{4058,84},{4064,85},
{4070,86},{4076,87},{4081,88},{4087,89},{4093,90},
{4099,91},{4105,92},{4111,93},{4117,94},{4123,95},
{4129,96},{4135,97},{4150,98},{4165,99},{4200,100},
};
static struct Vbat_mv_VS_Vbat_capacity Vbat_mv_capacity_CHG_45degC_300mA[]=
{
{3200,1},{3267,2},{3333,3},{3400,4},{3467,5},
{3533,6},{3600,7},{3606,8},{3612,9},{3618,10},
{3624,11},{3630,12},{3636,13},{3642,14},{3648,15},
{3654,16},{3659,17},{3665,18},{3671,19},{3677,20},
{3683,21},{3689,22},{3695,23},{3701,24},{3707,25},
{3713,26},{3719,27},{3725,28},{3731,29},{3737,30},
{3743,31},{3749,32},{3755,33},{3761,34},{3766,35},
{3772,36},{3778,37},{3784,38},{3790,39},{3796,40},
{3802,41},{3808,42},{3814,43},{3820,44},{3826,45},
{3832,46},{3838,47},{3844,48},{3850,49},{3856,50},
{3862,51},{3867,52},{3873,53},{3879,54},{3885,55},
{3891,56},{3897,57},{3903,58},{3909,59},{3915,60},
{3921,61},{3927,62},{3933,63},{3939,64},{3945,65},
{3951,66},{3957,67},{3963,68},{3969,69},{3974,70},
{3980,71},{3986,72},{3992,73},{3998,74},{4004,75},
{4010,76},{4016,77},{4022,78},{4028,79},{4034,80},
{4040,81},{4046,82},{4052,83},{4058,84},{4064,85},
{4070,86},{4076,87},{4081,88},{4087,89},{4093,90},
{4099,91},{4105,92},{4111,93},{4117,94},{4123,95},
{4129,96},{4135,97},{4150,98},{4165,99},{4200,100},
};
static struct Vbat_mv_VS_Vbat_capacity Vbat_mv_capacity_CHG_45degC_500mA[]=
{
{3200,1},{3267,2},{3333,3},{3400,4},{3467,5},
{3533,6},{3600,7},{3606,8},{3612,9},{3618,10},
{3624,11},{3630,12},{3636,13},{3642,14},{3648,15},
{3654,16},{3659,17},{3665,18},{3671,19},{3677,20},
{3683,21},{3689,22},{3695,23},{3701,24},{3707,25},
{3713,26},{3719,27},{3725,28},{3731,29},{3737,30},
{3743,31},{3749,32},{3755,33},{3761,34},{3766,35},
{3772,36},{3778,37},{3784,38},{3790,39},{3796,40},
{3802,41},{3808,42},{3814,43},{3820,44},{3826,45},
{3832,46},{3838,47},{3844,48},{3850,49},{3856,50},
{3862,51},{3867,52},{3873,53},{3879,54},{3885,55},
{3891,56},{3897,57},{3903,58},{3909,59},{3915,60},
{3921,61},{3927,62},{3933,63},{3939,64},{3945,65},
{3951,66},{3957,67},{3963,68},{3969,69},{3974,70},
{3980,71},{3986,72},{3992,73},{3998,74},{4004,75},
{4010,76},{4016,77},{4022,78},{4028,79},{4034,80},
{4040,81},{4046,82},{4052,83},{4058,84},{4064,85},
{4070,86},{4076,87},{4081,88},{4087,89},{4093,90},
{4099,91},{4105,92},{4111,93},{4117,94},{4123,95},
{4129,96},{4135,97},{4150,98},{4165,99},{4200,100},
};
#endif

static struct Vbat_mv_VS_Vbat_capacity Vbat_mv_capacity_DISCHG_0degC_100mA[]=
{
{3340,1},{3383,2},{3421,3},{3449,4},{3474,5},
{3489,6},{3505,7},{3520,8},{3534,9},{3544,10},
{3554,11},{3564,12},{3570,13},{3578,14},{3586,15},
{3598,16},{3606,17},{3610,18},{3621,19},{3624,20},
{3630,21},{3640,22},{3644,23},{3652,24},{3657,25},
{3661,26},{3665,27},{3669,28},{3675,29},{3680,30},
{3682,31},{3686,32},{3686,33},{3690,34},{3695,35},
{3696,36},{3698,37},{3701,38},{3703,39},{3704,40},
{3707,41},{3707,42},{3711,43},{3715,44},{3718,45},
{3719,46},{3723,47},{3724,48},{3729,49},{3730,50},
{3733,51},{3739,52},{3744,53},{3744,54},{3751,55},
{3754,56},{3760,57},{3764,58},{3770,59},{3773,60},
{3780,61},{3783,62},{3790,63},{3794,64},{3799,65},
{3805,66},{3813,67},{3823,68},{3829,69},{3833,70},
{3839,71},{3847,72},{3855,73},{3862,74},{3865,75},
{3874,76},{3881,77},{3888,78},{3894,79},{3907,80},
{3914,81},{3921,82},{3931,83},{3941,84},{3951,85},
{3955,86},{3961,87},{3970,88},{3980,89},{3987,90},
{3999,91},{4011,92},{4021,93},{4030,94},{4040,95},
{4050,96},{4062,97},{4076,98},{4092,99},{4110,100},
};
static struct Vbat_mv_VS_Vbat_capacity Vbat_mv_capacity_DISCHG_0degC_300mA[]=
{
{2941,1},{3001,2},{3047,3},{3083,4},{3108,5},
{3129,6},{3154,7},{3176,8},{3198,9},{3216,10},
{3236,11},{3250,12},{3267,13},{3283,14},{3296,15},
{3312,16},{3322,17},{3339,18},{3346,19},{3361,20},
{3374,21},{3382,22},{3396,23},{3406,24},{3413,25},
{3422,26},{3435,27},{3440,28},{3450,29},{3458,30},
{3469,31},{3479,32},{3484,33},{3491,34},{3501,35},
{3508,36},{3515,37},{3523,38},{3535,39},{3540,40},
{3549,41},{3551,42},{3562,43},{3569,44},{3571,45},
{3577,46},{3583,47},{3588,48},{3594,49},{3603,50},
{3603,51},{3606,52},{3612,53},{3613,54},{3616,55},
{3619,56},{3623,57},{3626,58},{3627,59},{3634,60},
{3639,61},{3644,62},{3645,63},{3649,64},{3655,65},
{3657,66},{3667,67},{3668,68},{3674,69},{3682,70},
{3686,71},{3690,72},{3698,73},{3706,74},{3713,75},
{3717,76},{3724,77},{3731,78},{3738,79},{3747,80},
{3750,81},{3760,82},{3769,83},{3775,84},{3782,85},
{3794,86},{3799,87},{3813,88},{3822,89},{3834,90},
{3842,91},{3854,92},{3864,93},{3876,94},{3891,95},
{3910,96},{3924,97},{3946,98},{3963,99},{4004,100},
};
static struct Vbat_mv_VS_Vbat_capacity Vbat_mv_capacity_DISCHG_0degC_500mA[]=
{
{2775,1},{2871,2},{2935,3},{2982,4},{3018,5},
{3049,6},{3075,7},{3096,8},{3112,9},{3127,10},
{3141,11},{3154,12},{3168,13},{3181,14},{3189,15},
{3203,16},{3214,17},{3222,18},{3229,19},{3239,20},
{3250,21},{3260,22},{3265,23},{3273,24},{3284,25},
{3292,26},{3302,27},{3309,28},{3315,29},{3324,30},
{3332,31},{3341,32},{3351,33},{3357,34},{3361,35},
{3373,36},{3379,37},{3392,38},{3395,39},{3404,40},
{3410,41},{3421,42},{3427,43},{3437,44},{3443,45},
{3447,46},{3459,47},{3463,48},{3470,49},{3478,50},
{3490,51},{3493,52},{3498,53},{3507,54},{3512,55},
{3518,56},{3521,57},{3529,58},{3533,59},{3539,60},
{3541,61},{3549,62},{3552,63},{3554,64},{3560,65},
{3563,66},{3567,67},{3570,68},{3575,69},{3582,70},
{3586,71},{3589,72},{3594,73},{3602,74},{3609,75},
{3614,76},{3616,77},{3628,78},{3635,79},{3640,80},
{3648,81},{3656,82},{3661,83},{3669,84},{3675,85},
{3687,86},{3698,87},{3704,88},{3714,89},{3725,90},
{3734,91},{3748,92},{3761,93},{3777,94},{3791,95},
{3806,96},{3827,97},{3847,98},{3874,99},{3917,100},
};
static struct Vbat_mv_VS_Vbat_capacity Vbat_mv_capacity_DISCHG_0degC_800mA[]=
{
{2750,1},{2810,2},{2921,3},{2990,4},{3039,5},
{3077,6},{3100,7},{3128,8},{3152,9},{3174,10},
{3188,11},{3207,12},{3223,13},{3239,14},{3253,15},
{3264,16},{3277,17},{3290,18},{3302,19},{3314,20},
{3323,21},{3334,22},{3345,23},{3356,24},{3366,25},
{3373,26},{3382,27},{3391,28},{3400,29},{3408,30},
{3414,31},{3422,32},{3430,33},{3438,34},{3445,35},
{3450,36},{3457,37},{3463,38},{3469,39},{3475,40},
{3480,41},{3485,42},{3491,43},{3497,44},{3502,45},
{3506,46},{3511,47},{3516,48},{3520,49},{3524,50},
{3527,51},{3531,52},{3535,53},{3539,54},{3543,55},
{3546,56},{3549,57},{3553,58},{3557,59},{3561,60},
{3564,61},{3567,62},{3571,63},{3575,64},{3580,65},
{3584,66},{3588,67},{3593,68},{3599,69},{3604,70},
{3608,71},{3614,72},{3620,73},{3625,74},{3631,75},
{3636,76},{3642,77},{3649,78},{3655,79},{3662,80},
{3668,81},{3675,82},{3682,83},{3690,84},{3698,85},
{3704,86},{3713,87},{3722,88},{3732,89},{3742,90},
{3749,91},{3759,92},{3770,93},{3781,94},{3792,95},
{3802,96},{3816,97},{3836,98},{3866,99},{3926,100},
};
static struct Vbat_mv_VS_Vbat_capacity Vbat_mv_capacity_DISCHG_25degC_100mA[]=
{
{3620,1},{3645,2},{3652,3},{3655,4},{3657,5},
{3663,6},{3666,7},{3669,8},{3671,9},{3671,10},
{3676,11},{3689,12},{3699,13},{3704,14},{3709,15},
{3717,16},{3726,17},{3729,18},{3730,19},{3738,20},
{3738,21},{3744,22},{3747,23},{3748,24},{3750,25},
{3750,26},{3753,27},{3753,28},{3753,29},{3756,30},
{3757,31},{3759,32},{3760,33},{3760,34},{3765,35},
{3765,36},{3767,37},{3771,38},{3772,39},{3772,40},
{3777,41},{3778,42},{3780,43},{3783,44},{3783,45},
{3789,46},{3790,47},{3792,48},{3799,49},{3802,50},
{3806,51},{3811,52},{3813,53},{3818,54},{3827,55},
{3827,56},{3836,57},{3841,58},{3848,59},{3861,60},
{3866,61},{3873,62},{3876,63},{3882,64},{3892,65},
{3897,66},{3902,67},{3907,68},{3914,69},{3918,70},
{3926,71},{3931,72},{3937,73},{3948,74},{3948,75},
{3959,76},{3963,77},{3972,78},{3976,79},{3987,80},
{3991,81},{3998,82},{4009,83},{4015,84},{4026,85},
{4028,86},{4036,87},{4050,88},{4057,89},{4064,90},
{4073,91},{4081,92},{4091,93},{4100,94},{4108,95},
{4116,96},{4125,97},{4143,98},{4149,99},{4167,100},
};
static struct Vbat_mv_VS_Vbat_capacity Vbat_mv_capacity_DISCHG_25degC_300mA[]=
{
{3466,1},{3522,2},{3553,3},{3573,4},{3587,5},
{3599,6},{3601,7},{3609,8},{3610,9},{3616,10},
{3619,11},{3622,12},{3635,13},{3647,14},{3653,15},
{3661,16},{3666,17},{3677,18},{3678,19},{3685,20},
{3692,21},{3693,22},{3699,23},{3700,24},{3701,25},
{3703,26},{3708,27},{3709,28},{3709,29},{3710,30},
{3715,31},{3717,32},{3719,33},{3720,34},{3720,35},
{3722,36},{3726,37},{3726,38},{3732,39},{3733,40},
{3734,41},{3736,42},{3738,43},{3743,44},{3745,45},
{3753,46},{3754,47},{3755,48},{3758,49},{3764,50},
{3767,51},{3769,52},{3776,53},{3777,54},{3781,55},
{3790,56},{3795,57},{3798,58},{3804,59},{3807,60},
{3812,61},{3816,62},{3827,63},{3835,64},{3841,65},
{3844,66},{3856,67},{3862,68},{3869,69},{3874,70},
{3878,71},{3885,72},{3892,73},{3899,74},{3907,75},
{3912,76},{3924,77},{3928,78},{3933,79},{3939,80},
{3947,81},{3954,82},{3963,83},{3970,84},{3984,85},
{3987,86},{3998,87},{4002,88},{4013,89},{4022,90},
{4030,91},{4040,92},{4051,93},{4058,94},{4069,95},
{4082,96},{4089,97},{4100,98},{4113,99},{4132,100},
};
static struct Vbat_mv_VS_Vbat_capacity Vbat_mv_capacity_DISCHG_25degC_500mA[]=
{
{3349,1},{3412,2},{3454,3},{3484,4},{3506,5},
{3521,6},{3537,7},{3548,8},{3553,9},{3564,10},
{3573,11},{3576,12},{3583,13},{3594,14},{3598,15},
{3610,16},{3618,17},{3626,18},{3630,19},{3636,20},
{3645,21},{3650,22},{3650,23},{3656,24},{3660,25},
{3664,26},{3667,27},{3672,28},{3674,29},{3678,30},
{3682,31},{3682,32},{3686,33},{3686,34},{3686,35},
{3690,36},{3694,37},{3695,38},{3698,39},{3698,40},
{3698,41},{3706,42},{3709,43},{3711,44},{3712,45},
{3716,46},{3716,47},{3720,48},{3720,49},{3726,50},
{3731,51},{3731,52},{3737,53},{3741,54},{3745,55},
{3749,56},{3753,57},{3757,58},{3768,59},{3771,60},
{3773,61},{3778,62},{3785,63},{3790,64},{3796,65},
{3804,66},{3812,67},{3813,68},{3819,69},{3830,70},
{3834,71},{3843,72},{3848,73},{3855,74},{3864,75},
{3873,76},{3875,77},{3886,78},{3893,79},{3899,80},
{3905,81},{3914,82},{3925,83},{3932,84},{3940,85},
{3950,86},{3959,87},{3969,88},{3975,89},{3984,90},
{3994,91},{4005,92},{4013,93},{4021,94},{4036,95},
{4044,96},{4058,97},{4065,98},{4082,99},{4108,100},
};
static struct Vbat_mv_VS_Vbat_capacity Vbat_mv_capacity_DISCHG_25degC_800mA[]=
{
{3347,1},{3392,2},{3421,3},{3448,4},{3470,5},
{3489,6},{3504,7},{3517,8},{3525,9},{3535,10},
{3544,11},{3551,12},{3560,13},{3568,14},{3574,15},
{3582,16},{3588,17},{3594,18},{3601,19},{3606,20},
{3611,21},{3616,22},{3620,23},{3624,24},{3629,25},
{3632,26},{3635,27},{3638,28},{3641,29},{3644,30},
{3648,31},{3650,32},{3653,33},{3655,34},{3657,35},
{3660,36},{3663,37},{3665,38},{3668,39},{3670,40},
{3672,41},{3675,42},{3677,43},{3680,44},{3683,45},
{3686,46},{3689,47},{3692,48},{3695,49},{3699,50},
{3703,51},{3706,52},{3710,53},{3714,54},{3718,55},
{3723,56},{3727,57},{3732,58},{3736,59},{3741,60},
{3746,61},{3751,62},{3757,63},{3763,64},{3767,65},
{3773,66},{3779,67},{3786,68},{3792,69},{3799,70},
{3806,71},{3811,72},{3818,73},{3825,74},{3833,75},
{3841,76},{3849,77},{3855,78},{3863,79},{3871,80},
{3879,81},{3888,82},{3896,83},{3904,84},{3912,85},
{3921,86},{3930,87},{3939,88},{3949,89},{3957,90},
{3966,91},{3975,92},{3985,93},{3995,94},{4005,95},
{4017,96},{4027,97},{4040,98},{4058,99},{4090,100},
};
static struct Vbat_mv_VS_Vbat_capacity Vbat_mv_capacity_DISCHG_45degC_100mA[]=
{
{3605,1},{3645,2},{3659,3},{3660,4},{3663,5},
{3665,6},{3666,7},{3667,8},{3669,9},{3671,10},
{3676,11},{3686,12},{3693,13},{3702,14},{3708,15},
{3715,16},{3722,17},{3725,18},{3726,19},{3730,20},
{3734,21},{3738,22},{3741,23},{3742,24},{3743,25},
{3749,26},{3754,27},{3755,28},{3757,29},{3762,30},
{3763,31},{3763,32},{3764,33},{3769,34},{3769,35},
{3771,36},{3778,37},{3778,38},{3779,39},{3782,40},
{3785,41},{3787,42},{3791,43},{3793,44},{3794,45},
{3795,46},{3802,47},{3806,48},{3807,49},{3814,50},
{3815,51},{3821,52},{3822,53},{3825,54},{3836,55},
{3843,56},{3848,57},{3858,58},{3863,59},{3869,60},
{3881,61},{3883,62},{3888,63},{3897,64},{3899,65},
{3905,66},{3914,67},{3918,68},{3924,69},{3930,70},
{3936,71},{3942,72},{3952,73},{3957,74},{3960,75},
{3966,76},{3971,77},{3980,78},{3988,79},{3995,80},
{4005,81},{4009,82},{4020,83},{4029,84},{4032,85},
{4041,86},{4049,87},{4060,88},{4066,89},{4078,90},
{4082,91},{4092,92},{4102,93},{4111,94},{4120,95},
{4127,96},{4140,97},{4150,98},{4157,99},{4168,100},
};
static struct Vbat_mv_VS_Vbat_capacity Vbat_mv_capacity_DISCHG_45degC_300mA[]=
{
{3564,1},{3607,2},{3626,3},{3635,4},{3637,5},
{3644,6},{3644,7},{3646,8},{3647,9},{3647,10},
{3649,11},{3663,12},{3670,13},{3676,14},{3685,15},
{3690,16},{3695,17},{3706,18},{3708,19},{3709,20},
{3713,21},{3717,22},{3718,23},{3724,24},{3724,25},
{3725,26},{3728,27},{3734,28},{3735,29},{3736,30},
{3737,31},{3740,32},{3743,33},{3746,34},{3750,35},
{3752,36},{3753,37},{3753,38},{3757,39},{3758,40},
{3762,41},{3766,42},{3767,43},{3770,44},{3772,45},
{3776,46},{3778,47},{3786,48},{3787,49},{3791,50},
{3793,51},{3800,52},{3801,53},{3804,54},{3813,55},
{3815,56},{3822,57},{3828,58},{3834,59},{3843,60},
{3850,61},{3853,62},{3860,63},{3868,64},{3874,65},
{3882,66},{3885,67},{3892,68},{3899,69},{3904,70},
{3913,71},{3920,72},{3926,73},{3933,74},{3937,75},
{3945,76},{3952,77},{3958,78},{3963,79},{3971,80},
{3979,81},{3987,82},{3995,83},{4003,84},{4015,85},
{4023,86},{4024,87},{4037,88},{4045,89},{4053,90},
{4064,91},{4069,92},{4078,93},{4090,94},{4098,95},
{4109,96},{4115,97},{4127,98},{4139,99},{4149,100},
};
static struct Vbat_mv_VS_Vbat_capacity Vbat_mv_capacity_DISCHG_45degC_500mA[]=
{
{3520,1},{3568,2},{3594,3},{3606,4},{3616,5},
{3620,6},{3623,7},{3624,8},{3624,9},{3627,10},
{3629,11},{3633,12},{3648,13},{3655,14},{3661,15},
{3666,16},{3676,17},{3678,18},{3684,19},{3691,20},
{3694,21},{3697,22},{3699,23},{3704,24},{3704,25},
{3710,26},{3711,27},{3712,28},{3714,29},{3714,30},
{3717,31},{3719,32},{3722,33},{3723,34},{3725,35},
{3730,36},{3734,37},{3736,38},{3738,39},{3740,40},
{3741,41},{3742,42},{3746,43},{3748,44},{3755,45},
{3757,46},{3761,47},{3763,48},{3766,49},{3770,50},
{3776,51},{3779,52},{3780,53},{3785,54},{3790,55},
{3797,56},{3801,57},{3808,58},{3811,59},{3816,60},
{3824,61},{3830,62},{3838,63},{3841,64},{3847,65},
{3855,66},{3864,67},{3868,68},{3875,69},{3885,70},
{3890,71},{3894,72},{3905,73},{3911,74},{3915,75},
{3921,76},{3929,77},{3936,78},{3945,79},{3953,80},
{3959,81},{3969,82},{3976,83},{3987,84},{3994,85},
{4001,86},{4006,87},{4016,88},{4024,89},{4034,90},
{4042,91},{4052,92},{4060,93},{4070,94},{4081,95},
{4088,96},{4097,97},{4106,98},{4121,99},{4134,100},
};
static struct Vbat_mv_VS_Vbat_capacity Vbat_mv_capacity_DISCHG_45degC_800mA[]=
{
{3552,1},{3573,2},{3584,3},{3590,4},{3595,5},
{3599,6},{3602,7},{3606,8},{3609,9},{3614,10},
{3619,11},{3628,12},{3636,13},{3644,14},{3650,15},
{3655,16},{3660,17},{3665,18},{3669,19},{3673,20},
{3675,21},{3678,22},{3681,23},{3684,24},{3686,25},
{3688,26},{3691,27},{3693,28},{3695,29},{3697,30},
{3699,31},{3701,32},{3703,33},{3705,34},{3707,35},
{3709,36},{3712,37},{3714,38},{3717,39},{3720,40},
{3722,41},{3725,42},{3728,43},{3732,44},{3735,45},
{3738,46},{3742,47},{3745,48},{3749,49},{3753,50},
{3756,51},{3761,52},{3765,53},{3770,54},{3775,55},
{3778,56},{3783,57},{3789,58},{3794,59},{3800,60},
{3805,61},{3811,62},{3817,63},{3824,64},{3830,65},
{3835,66},{3842,67},{3849,68},{3856,69},{3863,70},
{3869,71},{3876,72},{3883,73},{3891,74},{3898,75},
{3904,76},{3912,77},{3920,78},{3928,79},{3936,80},
{3942,81},{3951,82},{3959,83},{3968,84},{3977,85},
{3984,86},{3993,87},{4002,88},{4012,89},{4021,90},
{4028,91},{4038,92},{4048,93},{4058,94},{4068,95},
{4076,96},{4086,97},{4097,98},{4110,99},{4131,100},
};

struct AffectFactor
{
	char * descriptor;
	int current_state_min_threshold;
	int previous_state_threshold;
	int next_state_threshold;
	void * pTable_AffectFactor;
	unsigned int entry_count;
};

enum enum_AffectFactor
{
	AFFECTFACTOR_TBAT,
	AFFECTFACTOR_ISYSTEM,
};

static struct AffectFactor Vbat_mv_capacity_DISCHG_0degC_mA[]=
{
{"DISCHG_0degC_100mA", 150, 50, 250, (struct Vbat_mv_VS_Vbat_capacity *)(&Vbat_mv_capacity_DISCHG_0degC_100mA), (unsigned int)(sizeof(Vbat_mv_capacity_DISCHG_0degC_100mA)/sizeof(Vbat_mv_capacity_DISCHG_0degC_100mA[0]))},
{"DISCHG_0degC_300mA", 250, 200, 400, (struct Vbat_mv_VS_Vbat_capacity *)(&Vbat_mv_capacity_DISCHG_0degC_300mA), (unsigned int)(sizeof(Vbat_mv_capacity_DISCHG_0degC_300mA)/sizeof(Vbat_mv_capacity_DISCHG_0degC_300mA[0]))},
{"DISCHG_0degC_500mA", 400, 350, 700, (struct Vbat_mv_VS_Vbat_capacity *)(&Vbat_mv_capacity_DISCHG_0degC_500mA), (unsigned int)(sizeof(Vbat_mv_capacity_DISCHG_0degC_500mA)/sizeof(Vbat_mv_capacity_DISCHG_0degC_500mA[0]))},
{"DISCHG_0degC_800mA", 650, 600, 900, (struct Vbat_mv_VS_Vbat_capacity *)(&Vbat_mv_capacity_DISCHG_0degC_800mA), (unsigned int)(sizeof(Vbat_mv_capacity_DISCHG_0degC_800mA)/sizeof(Vbat_mv_capacity_DISCHG_0degC_800mA[0]))},
};
static struct AffectFactor Vbat_mv_capacity_DISCHG_25degC_mA[]=
{
{"DISCHG_25degC_100mA", 150, 50, 250, (struct Vbat_mv_VS_Vbat_capacity *)(&Vbat_mv_capacity_DISCHG_25degC_100mA), (unsigned int)(sizeof(Vbat_mv_capacity_DISCHG_25degC_100mA)/sizeof(Vbat_mv_capacity_DISCHG_25degC_100mA[0]))},
{"DISCHG_25degC_300mA", 250, 200, 400, (struct Vbat_mv_VS_Vbat_capacity *)(&Vbat_mv_capacity_DISCHG_25degC_300mA), (unsigned int)(sizeof(Vbat_mv_capacity_DISCHG_25degC_300mA)/sizeof(Vbat_mv_capacity_DISCHG_25degC_300mA[0]))},
{"DISCHG_25degC_500mA", 400, 350, 700, (struct Vbat_mv_VS_Vbat_capacity *)(&Vbat_mv_capacity_DISCHG_25degC_500mA), (unsigned int)(sizeof(Vbat_mv_capacity_DISCHG_25degC_500mA)/sizeof(Vbat_mv_capacity_DISCHG_25degC_500mA[0]))},
{"DISCHG_25degC_800mA", 650, 600, 900, (struct Vbat_mv_VS_Vbat_capacity *)(&Vbat_mv_capacity_DISCHG_25degC_800mA), (unsigned int)(sizeof(Vbat_mv_capacity_DISCHG_25degC_800mA)/sizeof(Vbat_mv_capacity_DISCHG_25degC_800mA[0]))},
};
static struct AffectFactor Vbat_mv_capacity_DISCHG_45degC_mA[]=
{
{"DISCHG_45degC_100mA", 150, 50, 250, (struct Vbat_mv_VS_Vbat_capacity *)(&Vbat_mv_capacity_DISCHG_45degC_100mA), (unsigned int)(sizeof(Vbat_mv_capacity_DISCHG_45degC_100mA)/sizeof(Vbat_mv_capacity_DISCHG_45degC_100mA[0]))},
{"DISCHG_45degC_300mA", 250, 200, 400, (struct Vbat_mv_VS_Vbat_capacity *)(&Vbat_mv_capacity_DISCHG_45degC_300mA), (unsigned int)(sizeof(Vbat_mv_capacity_DISCHG_45degC_300mA)/sizeof(Vbat_mv_capacity_DISCHG_45degC_300mA[0]))},
{"DISCHG_45degC_500mA", 400, 350, 700, (struct Vbat_mv_VS_Vbat_capacity *)(&Vbat_mv_capacity_DISCHG_45degC_500mA), (unsigned int)(sizeof(Vbat_mv_capacity_DISCHG_45degC_500mA)/sizeof(Vbat_mv_capacity_DISCHG_45degC_500mA[0]))},
{"DISCHG_45degC_800mA", 650, 600, 900, (struct Vbat_mv_VS_Vbat_capacity *)(&Vbat_mv_capacity_DISCHG_45degC_800mA), (unsigned int)(sizeof(Vbat_mv_capacity_DISCHG_45degC_800mA)/sizeof(Vbat_mv_capacity_DISCHG_45degC_800mA[0]))},
};

static struct AffectFactor Vbat_mv_capacity_CHG_0degC_mA[]=
{
#if 0
{"CHG_0degC_100mA", 150, 50, 250, (struct Vbat_mv_VS_Vbat_capacity *)(&Vbat_mv_capacity_CHG_0degC_100mA), (unsigned int)(sizeof(Vbat_mv_capacity_CHG_0degC_100mA)/sizeof(Vbat_mv_capacity_CHG_0degC_100mA[0]))},
{"CHG_0degC_300mA", 250, 200, 400, (struct Vbat_mv_VS_Vbat_capacity *)(&Vbat_mv_capacity_CHG_0degC_300mA), (unsigned int)(sizeof(Vbat_mv_capacity_CHG_0degC_300mA)/sizeof(Vbat_mv_capacity_CHG_0degC_300mA[0]))},
{"CHG_0degC_500mA", 400, 350, 600, (struct Vbat_mv_VS_Vbat_capacity *)(&Vbat_mv_capacity_CHG_0degC_500mA), (unsigned int)(sizeof(Vbat_mv_capacity_CHG_0degC_500mA)/sizeof(Vbat_mv_capacity_CHG_0degC_500mA[0]))},
#else
{"CHG_0degC_300mA", 50, 200, 400, (struct Vbat_mv_VS_Vbat_capacity *)(&Vbat_mv_capacity_CHG_0degC_300mA), (unsigned int)(sizeof(Vbat_mv_capacity_CHG_0degC_300mA)/sizeof(Vbat_mv_capacity_CHG_0degC_300mA[0]))},
#endif
};
static struct AffectFactor Vbat_mv_capacity_CHG_25degC_mA[]=
{
#if 0
{"CHG_25degC_100mA", 150, 50, 250, (struct Vbat_mv_VS_Vbat_capacity *)(&Vbat_mv_capacity_CHG_25degC_100mA), (unsigned int)(sizeof(Vbat_mv_capacity_CHG_25degC_100mA)/sizeof(Vbat_mv_capacity_CHG_25degC_100mA[0]))},
{"CHG_25degC_300mA", 250, 200, 400, (struct Vbat_mv_VS_Vbat_capacity *)(&Vbat_mv_capacity_CHG_25degC_300mA), (unsigned int)(sizeof(Vbat_mv_capacity_CHG_25degC_300mA)/sizeof(Vbat_mv_capacity_CHG_25degC_300mA[0]))},
{"CHG_25degC_500mA", 400, 350, 600, (struct Vbat_mv_VS_Vbat_capacity *)(&Vbat_mv_capacity_CHG_25degC_500mA), (unsigned int)(sizeof(Vbat_mv_capacity_CHG_25degC_500mA)/sizeof(Vbat_mv_capacity_CHG_25degC_500mA[0]))},
#else
{"CHG_25degC_300mA", 50, 200, 400, (struct Vbat_mv_VS_Vbat_capacity *)(&Vbat_mv_capacity_CHG_25degC_300mA), (unsigned int)(sizeof(Vbat_mv_capacity_CHG_25degC_300mA)/sizeof(Vbat_mv_capacity_CHG_25degC_300mA[0]))},
#endif
};
#if 0
static struct AffectFactor Vbat_mv_capacity_CHG_45degC_mA[]=
{
{"CHG_45degC_100mA", 150, 50, 250, (struct Vbat_mv_VS_Vbat_capacity *)(&Vbat_mv_capacity_CHG_45degC_100mA), (unsigned int)(sizeof(Vbat_mv_capacity_CHG_45degC_100mA)/sizeof(Vbat_mv_capacity_CHG_45degC_100mA[0]))},
{"CHG_45degC_300mA", 250, 200, 400, (struct Vbat_mv_VS_Vbat_capacity *)(&Vbat_mv_capacity_CHG_45degC_300mA), (unsigned int)(sizeof(Vbat_mv_capacity_CHG_45degC_300mA)/sizeof(Vbat_mv_capacity_CHG_45degC_300mA[0]))},
{"CHG_45degC_500mA", 400, 350, 600, (struct Vbat_mv_VS_Vbat_capacity *)(&Vbat_mv_capacity_CHG_45degC_500mA), (unsigned int)(sizeof(Vbat_mv_capacity_CHG_45degC_500mA)/sizeof(Vbat_mv_capacity_CHG_45degC_500mA[0]))},
};
#endif

static struct AffectFactor Vbat_mv_capacity_DISCHG_degC_mA[]=
{
{"DISCHG_0degC_mA", 0, 0, 15, (struct AffectFactor *)(&Vbat_mv_capacity_DISCHG_0degC_mA), (unsigned int)(sizeof(Vbat_mv_capacity_DISCHG_0degC_mA)/sizeof(Vbat_mv_capacity_DISCHG_0degC_mA[0]))},
{"DISCHG_25degC_mA", 15, 10, 40, (struct AffectFactor *)(&Vbat_mv_capacity_DISCHG_25degC_mA), (unsigned int)(sizeof(Vbat_mv_capacity_DISCHG_25degC_mA)/sizeof(Vbat_mv_capacity_DISCHG_25degC_mA[0]))},
{"DISCHG_45degC_mA", 40, 35, 60, (struct AffectFactor *)(&Vbat_mv_capacity_DISCHG_45degC_mA), (unsigned int)(sizeof(Vbat_mv_capacity_DISCHG_45degC_mA)/sizeof(Vbat_mv_capacity_DISCHG_45degC_mA[0]))},
};

static struct AffectFactor Vbat_mv_capacity_CHG_degC_mA[]=
{
#if 0
{"CHG_0degC_mA", 0, 0, 15, (struct AffectFactor *)(&Vbat_mv_capacity_CHG_0degC_mA), (unsigned int)(sizeof(Vbat_mv_capacity_CHG_0degC_mA)/sizeof(Vbat_mv_capacity_CHG_0degC_mA[0]))},
{"CHG_25degC_mA", 15, 10, 40, (struct AffectFactor *)(&Vbat_mv_capacity_CHG_25degC_mA), (unsigned int)(sizeof(Vbat_mv_capacity_CHG_25degC_mA)/sizeof(Vbat_mv_capacity_CHG_25degC_mA[0]))},
{"CHG_45degC_mA", 40, 35, 60, (struct AffectFactor *)(&Vbat_mv_capacity_CHG_45degC_mA), (unsigned int)(sizeof(Vbat_mv_capacity_CHG_45degC_mA)/sizeof(Vbat_mv_capacity_CHG_45degC_mA[0]))},
#else
{"CHG_0degC_mA", 0, 0, 15, (struct AffectFactor *)(&Vbat_mv_capacity_CHG_0degC_mA), (unsigned int)(sizeof(Vbat_mv_capacity_CHG_0degC_mA)/sizeof(Vbat_mv_capacity_CHG_0degC_mA[0]))},
{"CHG_25degC_mA", 15, 10, 40, (struct AffectFactor *)(&Vbat_mv_capacity_CHG_25degC_mA), (unsigned int)(sizeof(Vbat_mv_capacity_CHG_25degC_mA)/sizeof(Vbat_mv_capacity_CHG_25degC_mA[0]))},
#endif
};

static unsigned int get_Vbat_capacity(struct Vbat_mv_VS_Vbat_capacity *pVbat_capacity_table, unsigned int entry_count, unsigned int Vbat_mv)
{
	int Vbat_mv_capacity_count = entry_count;
	int index_low = 0, index_high = 0, i = 0;

	if (Vbat_mv<pVbat_capacity_table[0].Vbat_mv)
		return 0;
	else if(Vbat_mv>pVbat_capacity_table[Vbat_mv_capacity_count-1].Vbat_mv)
		return 100;
	
	for (i=0; i<Vbat_mv_capacity_count; i++)
	{
		if (Vbat_mv >= pVbat_capacity_table[i].Vbat_mv)
		{
			if (Vbat_mv == pVbat_capacity_table[i].Vbat_mv)
				return pVbat_capacity_table[i].Vbat_capacity;
			index_low = i;
			continue;
		}
		else
		{
			index_high = i;
			break;
		}
	}

	return pVbat_capacity_table[index_low].Vbat_capacity;
}

static void * get_AffectFactor_Table(struct AffectFactor *pAffectFactor_table, unsigned int entry_count, int factor, int * last_AffectFactor_index, unsigned int * pEntryCount, char **pTable_string)
{
	int index_low = 0, index_high = 0, i = 0;

	if ((*last_AffectFactor_index < 0) || (entry_count <= *last_AffectFactor_index))
	{
		if (factor<pAffectFactor_table[0].current_state_min_threshold)
		{
			*last_AffectFactor_index = 0;
			*pEntryCount = pAffectFactor_table[0].entry_count;
			*pTable_string = pAffectFactor_table[0].descriptor;
			return (void *)(pAffectFactor_table[0].pTable_AffectFactor);
		}
		else if(factor>pAffectFactor_table[entry_count-1].current_state_min_threshold)
		{
			*last_AffectFactor_index = entry_count-1;
			*pEntryCount = pAffectFactor_table[entry_count-1].entry_count;
			*pTable_string = pAffectFactor_table[entry_count-1].descriptor;
			return (void *)(pAffectFactor_table[entry_count-1].pTable_AffectFactor);
		}

		for (i=0; i<entry_count; i++)
		{
			if (factor >= pAffectFactor_table[i].current_state_min_threshold)
			{
				if (factor == pAffectFactor_table[i].current_state_min_threshold)
				{
					*last_AffectFactor_index = i;
					*pEntryCount = pAffectFactor_table[i].entry_count;
					*pTable_string = pAffectFactor_table[i].descriptor;
					return (void *)(pAffectFactor_table[i].pTable_AffectFactor);
				}
				index_low = i;
				continue;
			}
			else
			{
				index_high = i;
				break;
			}
		}
		*last_AffectFactor_index = index_low;
		*pEntryCount = pAffectFactor_table[index_low].entry_count;
		*pTable_string = pAffectFactor_table[index_low].descriptor;
		return (void *)(pAffectFactor_table[index_low].pTable_AffectFactor);
	}
	else
	{
		if (factor < pAffectFactor_table[*last_AffectFactor_index].previous_state_threshold)
		{
			while((*last_AffectFactor_index > 0) && (factor < pAffectFactor_table[*last_AffectFactor_index].previous_state_threshold))
			{
				(*last_AffectFactor_index)--;
			};
		}
		else if(factor > pAffectFactor_table[*last_AffectFactor_index].next_state_threshold)
		{
			while((*last_AffectFactor_index < entry_count-1) && (factor > pAffectFactor_table[*last_AffectFactor_index].next_state_threshold))
			{
				(*last_AffectFactor_index)++;
			};
		}
		*pEntryCount = pAffectFactor_table[*last_AffectFactor_index].entry_count;
		*pTable_string = pAffectFactor_table[*last_AffectFactor_index].descriptor;
		return (void *)(pAffectFactor_table[*last_AffectFactor_index].pTable_AffectFactor);
	}
}

struct Module_Loading
{
	char * descriptor;
	int id;
	int min;
	int max;
	int current_loading;
};

struct Module_Loading system_module_loading[]=
{
{"BT",		0,	30,	50,	0},
{"WiFi",		1,	100,	200,	0},
{"CALL",		2,	100,	170,	0},
{"LCM",		3,	70,	80,	0},
{"CAMERA",	4,	240,	280,	0},
{"VIDEO",	5,	80,	120,	0},
{"AUDIO",	6,	5,	20,	0},
{"3G",		7,	150,	350,	0},
};
#define MAX_MODULE	((int)(sizeof(system_module_loading)/sizeof(system_module_loading[0])))

int system_loading_update(enum module_loading_op action, int id, int loading)
{
	unsigned long flags;

	spin_lock_irqsave(&msm_chg.system_loading_lock, flags);
	if (gIsystem < DEFAULT_BASE_LOADING)
	{
		spin_unlock_irqrestore(&msm_chg.system_loading_lock, flags);
		printk(KERN_ERR "%s SYS loading error %d\n", __func__, gIsystem);
		return -1;
	}

	if ((system_module_loading[id].id != id) || (id >= MAX_MODULE))
	{
		spin_unlock_irqrestore(&msm_chg.system_loading_lock, flags);
		printk(KERN_ERR "%s id error %d\n", __func__, id);
		return -1;
	}

	switch(action)
	{
	case LOADING_SET:
		if (system_module_loading[id].current_loading > 0)
		{
			spin_unlock_irqrestore(&msm_chg.system_loading_lock, flags);
			PrintLog_INFO("%s %s loading reported %d\n", __func__, system_module_loading[id].descriptor, system_module_loading[id].current_loading);
		}
		else
		{
			if ((loading >= system_module_loading[id].min) && (loading <= system_module_loading[id].max))
			{
				system_module_loading[id].current_loading = loading;
				gIsystem += loading;
				spin_unlock_irqrestore(&msm_chg.system_loading_lock, flags);
			}
			else
			{
				spin_unlock_irqrestore(&msm_chg.system_loading_lock, flags);
				printk(KERN_ERR "%s %s out of range %d\n", __func__, system_module_loading[id].descriptor, loading);
			}
		}
		break;
	case LOADING_CLR:
		if (system_module_loading[id].current_loading == 0)
		{
			spin_unlock_irqrestore(&msm_chg.system_loading_lock, flags);
			PrintLog_INFO("%s %s loading not reported\n", __func__, system_module_loading[id].descriptor);
		}
		else
		{
			gIsystem -= system_module_loading[id].current_loading;
			system_module_loading[id].current_loading = 0;
			spin_unlock_irqrestore(&msm_chg.system_loading_lock, flags);
		}
		break;
	case LOADING_GET:
		spin_unlock_irqrestore(&msm_chg.system_loading_lock, flags);
		return system_module_loading[id].current_loading;
		break;
	case LOADING_SUM:
	default:
		spin_unlock_irqrestore(&msm_chg.system_loading_lock, flags);
		break;
	}
	return gIsystem;
}
#endif
// modified by yafi-end

extern bool smb137b_is_charging_full(void);
#define AVG_SAMPLE_VBAT_PERCENT_NUM 20
#define REASONABLE_DELTA_PERCENT (1)
static unsigned int msm_chg_get_batt_capacity_percent(void)
{
// modified by yafi-begin
#if defined(ORG_VER)
	unsigned int current_voltage = get_prop_battery_mvolts();
	unsigned int low_voltage = msm_chg.min_voltage;
	unsigned int high_voltage = msm_chg.max_voltage;

	if (current_voltage <= low_voltage)
		return 0;
	else if (current_voltage >= high_voltage)
		return 100;
	else
		return (current_voltage - low_voltage) * 100
		    / (high_voltage - low_voltage);
#else
	int i, sum_vbatt_percent = 0;
	unsigned int current_voltage = 0;
	unsigned int current_percent = 0;
	struct Vbat_mv_VS_Vbat_capacity * table_Vbat_VS_Capacity = NULL;
	static struct Vbat_mv_VS_Vbat_capacity * last_table_Vbat_VS_Capacity = NULL;
	bool local_is_charger_inserted;
	unsigned int delta_between_LastPercent_CurrentPercent = 0;
	int local_current_Tbat;
	int local_current_Isystem;
	static int last_Tbat_index = -1;
	static int last_Isystem_index = -1;
	struct AffectFactor * pAffectFactor_Table = NULL;
	unsigned int entry_count = 0;
	char *pTable_string = NULL;
	
	local_is_charger_inserted = is_charger_inserted;
	local_current_Tbat = gTbat;
	local_current_Isystem = gIsystem;
	current_voltage = get_prop_battery_mvolts();
	if (current_voltage == 0)
	{
		if (last_percent < 0)
		{	
			if(cci_charging_boot == 1)
				current_percent = -1;
			else
				current_percent = DEFAULT_PERCENT;
		}
		else
			current_percent = last_percent;
	}
	else
	{
		PrintLog_DEBUG("%s Vbat=%d\n", __func__, current_voltage);
		if ((msm_chg.batt_status == BATT_STATUS_JUST_FINISHED_CHARGING && msm_chg.current_chg_priv != NULL))
		{
			current_percent = 100;
			for(i=0;i<AVG_SAMPLE_VBAT_PERCENT_NUM;i++)
				avg_sample_vbatt_percent[i] = current_percent;
			avg_sample_vbatt_percent_recycle = true;
			last_percent = current_percent;
			PrintLog_DEBUG("%s is_FULL=%d, Vbat=%d\n", __func__, (msm_chg.batt_status == BATT_STATUS_JUST_FINISHED_CHARGING), current_voltage);
			goto out;
		}
		else
		{
			if (local_is_charger_inserted)
			{
				pAffectFactor_Table = (struct AffectFactor *)get_AffectFactor_Table((struct AffectFactor *)(&Vbat_mv_capacity_CHG_degC_mA), (unsigned int)(sizeof(Vbat_mv_capacity_CHG_degC_mA)/sizeof(Vbat_mv_capacity_CHG_degC_mA[0])),  local_current_Tbat, &last_Tbat_index, &entry_count, &pTable_string);
			}
			else
			{
#if 0
				if (current_voltage > min_voltage_mv_DCHG)
					current_voltage = min_voltage_mv_DCHG;
				else
					min_voltage_mv_DCHG = current_voltage;
#endif
				pAffectFactor_Table = (struct AffectFactor *)get_AffectFactor_Table((struct AffectFactor *)(&Vbat_mv_capacity_DISCHG_degC_mA), (unsigned int)(sizeof(Vbat_mv_capacity_DISCHG_degC_mA)/sizeof(Vbat_mv_capacity_DISCHG_degC_mA[0])),  local_current_Tbat, &last_Tbat_index, &entry_count, &pTable_string);
			}
			table_Vbat_VS_Capacity = (struct Vbat_mv_VS_Vbat_capacity *)get_AffectFactor_Table((struct AffectFactor *)(pAffectFactor_Table), (unsigned int)entry_count, local_current_Isystem, &last_Isystem_index, &entry_count, &pTable_string);
			current_percent = get_Vbat_capacity((struct Vbat_mv_VS_Vbat_capacity *)(table_Vbat_VS_Capacity), entry_count, current_voltage);
			PrintLog_DEBUG("%s Table=%s vol=%d per=%d\n", __func__, pTable_string, current_voltage, current_percent);
		}

		avg_sample_vbatt_percent[avg_sample_vbatt_percent_index] = current_percent;
		if (++avg_sample_vbatt_percent_index >= AVG_SAMPLE_VBAT_PERCENT_NUM)
		{
			avg_sample_vbatt_percent_index = 0;
			avg_sample_vbatt_percent_recycle = true;
		}
		if(avg_sample_vbatt_percent_recycle)
		{	
			for(i=0;i<AVG_SAMPLE_VBAT_PERCENT_NUM;i++)
				sum_vbatt_percent += avg_sample_vbatt_percent[i];
			current_percent = sum_vbatt_percent/AVG_SAMPLE_VBAT_PERCENT_NUM;
		}
		else
		{
			for(i=0;i<avg_sample_vbatt_percent_index;i++)
				sum_vbatt_percent += avg_sample_vbatt_percent[i];
			current_percent = sum_vbatt_percent/avg_sample_vbatt_percent_index;
		}
		PrintLog_DEBUG("%s Pbat=%d LPbat=%d OP=%d, LT=0x%x, CT=0x%x\n", __func__, current_percent, last_percent, over_reasonable_delta_percent, (int)last_table_Vbat_VS_Capacity, (int)table_Vbat_VS_Capacity);
		if (last_percent < 0)
			last_percent = current_percent;
		else
		{
			if (last_table_Vbat_VS_Capacity != table_Vbat_VS_Capacity)
			{
				if (last_table_Vbat_VS_Capacity != NULL)
				{
					over_reasonable_delta_percent = true;
				}
				last_table_Vbat_VS_Capacity = table_Vbat_VS_Capacity;
			}
			PrintLog_DEBUG("%s OP=%d\n", __func__, over_reasonable_delta_percent);
			if(over_reasonable_delta_percent)
			{
				current_percent = last_percent;
			}
			else
			{
				if (local_is_charger_inserted)
				{
					if ((int)current_percent > (int)last_percent)
					{
						delta_between_LastPercent_CurrentPercent = current_percent - last_percent;
						if ((delta_between_LastPercent_CurrentPercent == 1) || (delta_between_LastPercent_CurrentPercent == 2))
							last_percent = current_percent = last_percent + 1;
						else
							last_percent = current_percent = (int)(last_percent + REASONABLE_DELTA_PERCENT)>(int)current_percent?(current_percent):(last_percent + REASONABLE_DELTA_PERCENT);
						over_reasonable_delta_percent = true;
					}
					else if ((int)current_percent < (int)last_percent)
					{
						delta_between_LastPercent_CurrentPercent = last_percent - current_percent;
						if ((delta_between_LastPercent_CurrentPercent == 1) || (delta_between_LastPercent_CurrentPercent == 2))
							last_percent = current_percent = last_percent - 1;
						else
							last_percent = current_percent = (int)(last_percent - REASONABLE_DELTA_PERCENT)>(int)current_percent?(last_percent - REASONABLE_DELTA_PERCENT):current_percent;
						over_reasonable_delta_percent = true;
					}
					else
					{
						last_percent = current_percent;
					}
				}
				else
				{
					if ((int)current_percent > (int)last_percent)
					{
						current_percent = last_percent;
						over_reasonable_delta_percent = true;
					}
					else if ((int)current_percent < (int)last_percent)
					{
						delta_between_LastPercent_CurrentPercent = last_percent - current_percent;
						if ((delta_between_LastPercent_CurrentPercent == 1) || (delta_between_LastPercent_CurrentPercent == 2))
							last_percent = current_percent = last_percent  - 1;
						else
							last_percent = current_percent = (int)(last_percent - REASONABLE_DELTA_PERCENT)>(int)current_percent?(last_percent - REASONABLE_DELTA_PERCENT):current_percent;
						over_reasonable_delta_percent = true;
					}
					else
					{
						last_percent = current_percent;
					}
				}
			}
		}
	}
out:
	gPbat = current_percent;
	PrintLog_DEBUG("%s Vbat=%d, Pbat=%d LP=%d\n", __func__, current_voltage, current_percent, last_percent);

	return current_percent;
#endif
// modified by yafi-end
}

static unsigned int msm_chg_get_buffered_batt_capacity_percent(void)
{
	struct timespec time, interval;
	
	if((cci_charging_boot == 1) && (last_percent < 0))
		return -1;
	else
	{
		if (inited_gtime_unplugatfullcharged)
		{
			do_posix_clock_monotonic_gettime(&time);
			monotonic_to_bootbased(&time);
			interval = timespec_sub(time, gtime_unplugatfullcharged);
			if (interval.tv_sec < KEEPFULLCHARGEDTIME)
			{
				return 100;
			}
			else
			{
				inited_gtime_unplugatfullcharged = false;
				return gPbat;
			}
		}
		else
		{
			return gPbat;
		}
	}
}

#ifdef DEBUG
static inline void debug_print(const char *func,
			       struct msm_hardware_charger_priv *hw_chg_priv)
{
	dev_dbg(msm_chg.dev,
		"%s current=(%s)(s=%d)(r=%d) new=(%s)(s=%d)(r=%d) batt=%d En\n",
		func,
		msm_chg.current_chg_priv ? msm_chg.current_chg_priv->
		hw_chg->name : "none",
		msm_chg.current_chg_priv ? msm_chg.
		current_chg_priv->hw_chg_state : -1,
		msm_chg.current_chg_priv ? msm_chg.current_chg_priv->
		hw_chg->rating : -1,
		hw_chg_priv ? hw_chg_priv->hw_chg->name : "none",
		hw_chg_priv ? hw_chg_priv->hw_chg_state : -1,
		hw_chg_priv ? hw_chg_priv->hw_chg->rating : -1,
		msm_chg.batt_status);
}
#else
static inline void debug_print(const char *func,
			       struct msm_hardware_charger_priv *hw_chg_priv)
{
	PrintLog_DEBUG("%s current=(%s)(s=%d)(r=%d) new=(%s)(s=%d)(r=%d) batt=%d En\n",
		func,
		msm_chg.current_chg_priv ? msm_chg.current_chg_priv->
		hw_chg->name : "none",
		msm_chg.current_chg_priv ? msm_chg.
		current_chg_priv->hw_chg_state : -1,
		msm_chg.current_chg_priv ? msm_chg.current_chg_priv->
		hw_chg->rating : -1,
		hw_chg_priv ? hw_chg_priv->hw_chg->name : "none",
		hw_chg_priv ? hw_chg_priv->hw_chg_state : -1,
		hw_chg_priv ? hw_chg_priv->hw_chg->rating : -1,
		msm_chg.batt_status);
}
#endif

static struct msm_hardware_charger_priv *find_best_charger(void)
{
	struct msm_hardware_charger_priv *hw_chg_priv;
	struct msm_hardware_charger_priv *better;
	int rating;

	better = NULL;
	rating = 0;

	list_for_each_entry(hw_chg_priv, &msm_chg.msm_hardware_chargers, list) {
		if (is_chg_capable_of_charging(hw_chg_priv)) {
			if (hw_chg_priv->hw_chg->rating > rating) {
				rating = hw_chg_priv->hw_chg->rating;
				better = hw_chg_priv;
			}
		}
	}

	return better;
}

static int msm_charging_switched(struct msm_hardware_charger_priv *priv)
{
	int ret = 0;

	if (priv->hw_chg->charging_switched)
		ret = priv->hw_chg->charging_switched(priv->hw_chg);
	return ret;
}

static int msm_stop_charging(struct msm_hardware_charger_priv *priv)
{
	int ret;

	ret = priv->hw_chg->stop_charging(priv->hw_chg);
// modified by yafi-begin
#if defined(ORG_VER)
	if (!ret)
#else
#endif
// modified by yafi-end
		wake_unlock(&msm_chg.wl);
	return ret;
}

// modified by yafi-begin
#if defined(ORG_VER)
static void msm_enable_system_current(struct msm_hardware_charger_priv *priv)
{
	if (priv->hw_chg->start_system_current)
		priv->hw_chg->start_system_current(priv->hw_chg,
					 priv->max_source_current);
}

static void msm_disable_system_current(struct msm_hardware_charger_priv *priv)
{
	if (priv->hw_chg->stop_system_current)
		priv->hw_chg->stop_system_current(priv->hw_chg);
}
#else
#endif
// modified by yafi-end

/* the best charger has been selected -start charging from current_chg_priv */
static int msm_start_charging(void)
{
	int ret;
	struct msm_hardware_charger_priv *priv;

	priv = msm_chg.current_chg_priv;

	PrintLog_DEBUG("%s, %s chg_voltage=%d chg_current=%d",
		__func__,priv->hw_chg->name, msm_chg.max_voltage, priv->max_source_current);
	
	wake_lock(&msm_chg.wl);
	ret = priv->hw_chg->start_charging(priv->hw_chg, msm_chg.max_voltage,
					 priv->max_source_current);
	if (ret) {
		wake_unlock(&msm_chg.wl);
		dev_err(msm_chg.dev, "%s couldnt start chg error = %d\n",
			priv->hw_chg->name, ret);
	} else
		priv->hw_chg_state = CHG_CHARGING_STATE;

	return ret;
}

static void handle_charging_done(struct msm_hardware_charger_priv *priv)
{
	if (msm_chg.current_chg_priv == priv) {
		if (msm_chg.current_chg_priv->hw_chg_state ==
		    CHG_CHARGING_STATE)
		{
			PrintLog_DEBUG("%s calling msm_stop_charging()\n", __func__);
			if (msm_stop_charging(msm_chg.current_chg_priv)) {
				dev_err(msm_chg.dev, "%s couldnt stop chg\n",
					msm_chg.current_chg_priv->hw_chg->name);
			}
		}
		msm_chg.current_chg_priv->hw_chg_state = CHG_READY_STATE;

		msm_chg.batt_status = BATT_STATUS_JUST_FINISHED_CHARGING;
// modified by yafi-begin
#if defined(ORG_VER)
#else
		cancel_delayed_work(&msm_chg.update_heartbeat_work);
		queue_delayed_work(msm_chg.event_wq_thread,
				&msm_chg.update_heartbeat_work,
				round_jiffies_relative(msecs_to_jiffies
				(0 * MSEC_PER_SEC)));
#endif
// modified by yafi-end
		dev_info(msm_chg.dev, "%s: stopping safety timer work\n",
				__func__);
		cancel_delayed_work(&msm_chg.teoc_work);
// modified by yafi-begin
#if defined(ORG_VER)
		if (msm_batt_gauge && msm_batt_gauge->monitor_for_recharging)
			msm_batt_gauge->monitor_for_recharging();
		else
			dev_err(msm_chg.dev,
			      "%s: no batt gauge recharge monitor\n", __func__);
#else
#endif
// modified by yafi-end
	}
}

static void teoc(struct work_struct *work)
{
	/* we have been charging too long - stop charging */
	dev_info(msm_chg.dev, "%s: safety timer work expired\n", __func__);
	PrintLog_DEBUG("%s TIMEOUT\n", __func__);

	mutex_lock(&msm_chg.status_lock);
	if (msm_chg.current_chg_priv != NULL
	    && msm_chg.current_chg_priv->hw_chg_state == CHG_CHARGING_STATE) {
		PrintLog_DEBUG("%s calling handle_charging_done()\n", __func__);
		handle_charging_done(msm_chg.current_chg_priv);
	}
	mutex_unlock(&msm_chg.status_lock);
}

static void handle_battery_inserted(void)
{
	PrintLog_DEBUG("%s current_chg_priv=0x%x\n", __func__, (int)msm_chg.current_chg_priv);
	/* if a charger is already present start charging */
	if (msm_chg.current_chg_priv != NULL &&
	    is_batt_status_capable_of_charging() &&
	    !is_batt_status_charging()) {
		if (msm_start_charging()) {
			dev_err(msm_chg.dev, "%s couldnt start chg\n",
				msm_chg.current_chg_priv->hw_chg->name);
			return;
		}
		msm_chg.batt_status = BATT_STATUS_TRKL_CHARGING;

		dev_info(msm_chg.dev, "%s: starting safety timer work\n",
				__func__);
// modified by yafi-begin
#if defined(ORG_VER)
#else
		if (!gmsm_charger_timeout_disable)
#endif
// modified by yafi-end
		queue_delayed_work(msm_chg.event_wq_thread,
					&msm_chg.teoc_work,
				      round_jiffies_relative(msecs_to_jiffies
							     (msm_chg.
							      safety_time)));
	}
}

static void handle_battery_removed(void)
{
	/* if a charger is charging the battery stop it */
	if (msm_chg.current_chg_priv != NULL
	    && msm_chg.current_chg_priv->hw_chg_state == CHG_CHARGING_STATE) {
		if (msm_stop_charging(msm_chg.current_chg_priv)) {
			dev_err(msm_chg.dev, "%s couldnt stop chg\n",
				msm_chg.current_chg_priv->hw_chg->name);
		}
		msm_chg.current_chg_priv->hw_chg_state = CHG_READY_STATE;

		dev_info(msm_chg.dev, "%s: stopping safety timer work\n",
				__func__);
		cancel_delayed_work(&msm_chg.teoc_work);
	}
}

// modified by yafi-begin
#if defined(ORG_VER)
#else
extern int smb137b_get_battery_mvolts(void);

void update_power_supply_batt(void)
{
	smb137b_get_battery_mvolts();
	msm_chg_get_batt_capacity_percent();
	/* notify that the voltage has changed
	 * the read of the capacity will trigger a
	 * voltage read*/
	power_supply_changed(&msm_psy_batt);
}
#endif
// modified by yafi-end

static void update_heartbeat(struct work_struct *work)
{
	int temperature;

	wake_lock(&msm_chg.wl_heartbeat);

	PrintLog_DEBUG("%s batt_status-01= %d\n", __func__, msm_chg.batt_status);

	if (msm_chg.batt_status == BATT_STATUS_ABSENT
		|| msm_chg.batt_status == BATT_STATUS_ID_INVALID) {
		if (is_battery_present())
			if (is_battery_id_valid()) {
				msm_chg.batt_status = BATT_STATUS_DISCHARGING;
				handle_battery_inserted();
			}
	} else {
		if (!is_battery_present()) {
			msm_chg.batt_status = BATT_STATUS_ABSENT;
			handle_battery_removed();
		}
		/*
		 * check battery id because a good battery could be removed
		 * and replaced with a invalid battery.
		 */
		if (!is_battery_id_valid()) {
			msm_chg.batt_status = BATT_STATUS_ID_INVALID;
			handle_battery_removed();
		}
	}
	pr_debug("msm-charger %s batt_status= %d\n",
				__func__, msm_chg.batt_status);
	PrintLog_DEBUG("%s batt_status-02= %d\n",
				__func__, msm_chg.batt_status);

	if (msm_chg.current_chg_priv
		&& msm_chg.current_chg_priv->hw_chg_state
			== CHG_CHARGING_STATE) {
		temperature = get_battery_temperature();
		/* TODO implement JEITA SPEC*/
	}

// modified by yafi-begin
#if defined(ORG_VER)
	/* notify that the voltage has changed
	 * the read of the capacity will trigger a
	 * voltage read*/
	power_supply_changed(&msm_psy_batt);
#else
	smb137b_get_battery_mvolts();
	over_reasonable_delta_percent = false;
	msm_chg_get_batt_capacity_percent();
	power_supply_changed(&msm_psy_batt);
#endif
// modified by yafi-end

	if (msm_chg.stop_update) {
		msm_chg.stop_update = 0;
		wake_unlock(&msm_chg.wl_heartbeat);		
		return;
	}
// modified by yafi-begin
#if defined(ORG_VER)
	queue_delayed_work(msm_chg.event_wq_thread,
				&msm_chg.update_heartbeat_work,
			      round_jiffies_relative(msecs_to_jiffies
						     (msm_chg.update_time)));
#else
	if ((cci_charging_boot == 1) && (last_percent < 0))
		queue_delayed_work(msm_chg.event_wq_thread,
				&msm_chg.update_heartbeat_work,
			      round_jiffies_relative(msecs_to_jiffies
						     (0.5 * MSEC_PER_SEC)));
	else
		queue_delayed_work(msm_chg.event_wq_thread,
				&msm_chg.update_heartbeat_work,
			      round_jiffies_relative(msecs_to_jiffies
						     (msm_chg.update_time)));

	wake_unlock(&msm_chg.wl_heartbeat);	
#endif
// modified by yafi-end
}

/* set the charger state to READY before calling this */
static void handle_charger_ready(struct msm_hardware_charger_priv *hw_chg_priv)
{
// modified by yafi-begin
#if defined(ORG_VER)
	struct msm_hardware_charger_priv *old_chg_priv = NULL;
#else
#endif
// modified by yafi-end

//	debug_print(__func__, hw_chg_priv);

	PrintLog_DEBUG("%s current_chg_priv=0x%x", __func__, (int)msm_chg.current_chg_priv);
	if((int)msm_chg.current_chg_priv)
		PrintLog_DEBUG("%s msm_chg:0x%x,%d,%d hw_chg_priv:%d\n", __func__, \
		(int)msm_chg.current_chg_priv, msm_chg.current_chg_priv->hw_chg->rating, msm_chg.current_chg_priv->hw_chg_state, \
		hw_chg_priv->hw_chg->rating);

// modified by yafi-begin
#if defined(ORG_VER)
	if (msm_chg.current_chg_priv != NULL
	    && hw_chg_priv->hw_chg->rating >
	    msm_chg.current_chg_priv->hw_chg->rating) {
#else
	if (msm_chg.current_chg_priv != NULL
	    && hw_chg_priv->hw_chg->rating >=
	    msm_chg.current_chg_priv->hw_chg->rating) {
#endif
// modified by yafi-end
		/*
		 * a better charger was found, ask the current charger
		 * to stop charging if it was charging
		 */
		if (msm_chg.current_chg_priv->hw_chg_state ==
		    CHG_CHARGING_STATE) {
			if (msm_stop_charging(msm_chg.current_chg_priv)) {
				dev_err(msm_chg.dev, "%s couldnt stop chg\n",
					msm_chg.current_chg_priv->hw_chg->name);
				return;
			}
			if (msm_charging_switched(msm_chg.current_chg_priv)) {
				dev_err(msm_chg.dev, "%s couldnt switch chg\n",
					msm_chg.current_chg_priv->hw_chg->name);
				return;
			}
		}
		msm_chg.current_chg_priv->hw_chg_state = CHG_READY_STATE;
// modified by yafi-begin
#if defined(ORG_VER)
		old_chg_priv = msm_chg.current_chg_priv;
#else
#endif
// modified by yafi-end
		msm_chg.current_chg_priv = NULL;
	}

	if (msm_chg.current_chg_priv == NULL) {
		msm_chg.current_chg_priv = hw_chg_priv;
		dev_info(msm_chg.dev,
			 "%s: best charger = %s\n", __func__,
			 msm_chg.current_chg_priv->hw_chg->name);

// modified by yafi-begin
#if defined(ORG_VER)
		msm_enable_system_current(msm_chg.current_chg_priv);
		/*
		 * since a better charger was chosen, ask the old
		 * charger to stop providing system current
		 */
		if (old_chg_priv != NULL)
			msm_disable_system_current(old_chg_priv);
#else
#endif
// modified by yafi-end

		if (!is_batt_status_capable_of_charging())
			return;

		/* start charging from the new charger */
		if (!msm_start_charging()) {
			/* if we simply switched chg continue with teoc timer
			 * else we update the batt state and set the teoc
			 * timer */
			if (!is_batt_status_charging()) {
				dev_info(msm_chg.dev,
				       "%s: starting safety timer\n", __func__);
// modified by yafi-begin
#if defined(ORG_VER)
#else
				if (!gmsm_charger_timeout_disable)
#endif
// modified by yafi-end
				queue_delayed_work(msm_chg.event_wq_thread,
							&msm_chg.teoc_work,
						      round_jiffies_relative
						      (msecs_to_jiffies
						       (msm_chg.safety_time)));
				msm_chg.batt_status = BATT_STATUS_TRKL_CHARGING;
			}
		} else {
			/* we couldnt start charging from the new readied
			 * charger */
			if (is_batt_status_charging())
				msm_chg.batt_status = BATT_STATUS_DISCHARGING;
		}
	}
}

static void handle_charger_removed(struct msm_hardware_charger_priv
				   *hw_chg_removed, int new_state)
{
	struct msm_hardware_charger_priv *hw_chg_priv;

//	debug_print(__func__, hw_chg_removed);
	PrintLog_INFO("%s current_chg_priv=0x%x", __func__, (int)msm_chg.current_chg_priv);
	if((int)msm_chg.current_chg_priv)
		PrintLog_INFO("%s hw_chg_state=%d\n", __func__, msm_chg.current_chg_priv->hw_chg_state);

	if (msm_chg.current_chg_priv == hw_chg_removed) {
		PrintLog_DEBUG("%s msm_chg.current_chg_priv=hw_chg_removed\n", __func__);
// modified by yafi-begin
#if defined(ORG_VER)
		msm_disable_system_current(hw_chg_removed);
#else
#endif
// modified by yafi-end
		if (msm_chg.current_chg_priv->hw_chg_state
						== CHG_CHARGING_STATE) {
			PrintLog_DEBUG("%s calling msm_stop_charging()\n", __func__);
			if (msm_stop_charging(hw_chg_removed)) {
				dev_err(msm_chg.dev, "%s couldnt stop chg\n",
					msm_chg.current_chg_priv->hw_chg->name);
			}
		}
		msm_chg.current_chg_priv = NULL;
	}

	hw_chg_removed->hw_chg_state = new_state;
	PrintLog_DEBUG("%s msm_chg.current_chg_priv=0x%x\n", __func__, (int)msm_chg.current_chg_priv);
	PrintLog_DEBUG("%s msm_chg.batt_status_01=%d\n", __func__, msm_chg.batt_status);
	if (msm_chg.current_chg_priv == NULL) {
		hw_chg_priv = find_best_charger();
		if (hw_chg_priv == NULL) {
			dev_info(msm_chg.dev, "%s: no chargers\n", __func__);
			/* if the battery was Just finished charging
			 * we keep that state as is so that we dont rush
			 * in to charging the battery when a charger is
			 * plugged in shortly. */
// modified by yafi-begin
#if defined(ORG_VER)
			if (is_batt_status_charging())
				msm_chg.batt_status = BATT_STATUS_DISCHARGING;
#else
			if (is_batt_status_charging() || (msm_chg.batt_status == BATT_STATUS_JUST_FINISHED_CHARGING) || (msm_chg.batt_status == BATT_STATUS_TEMPERATURE_OUT_OF_RANGE))
				msm_chg.batt_status = BATT_STATUS_DISCHARGING;
#endif
// modified by yafi-end
		} else {
			msm_chg.current_chg_priv = hw_chg_priv;
// modified by yafi-begin
#if defined(ORG_VER)
			msm_enable_system_current(hw_chg_priv);
#else
#endif
// modified by yafi-end
			dev_info(msm_chg.dev,
				 "%s: best charger = %s\n", __func__,
				 msm_chg.current_chg_priv->hw_chg->name);

			if (!is_batt_status_capable_of_charging())
				return;

			if (msm_start_charging()) {
				/* we couldnt start charging for some reason */
				msm_chg.batt_status = BATT_STATUS_DISCHARGING;
			}
		}
	}

	PrintLog_DEBUG("%s msm_chg.batt_status_02=%d\n", __func__, msm_chg.batt_status);
	/* if we arent charging stop the safety timer */
	if (!is_batt_status_charging()) {
		dev_info(msm_chg.dev, "%s: stopping safety timer work\n",
				__func__);
		cancel_delayed_work(&msm_chg.teoc_work);
	}
}

static void handle_event(struct msm_hardware_charger *hw_chg, int event)
{
	struct msm_hardware_charger_priv *priv = NULL;

	/*
	 * if hw_chg is NULL then this event comes from non-charger
	 * parties like battery gauge
	 */

	PrintLog_DEBUG("%s hw_chg=0x%x, event=%d\n", __func__, (int)hw_chg, event);
//	PrintLog_DEBUG("%s msm_batt_gauge01=0x%x\n", __func__, (int)msm_batt_gauge);
	
	if (hw_chg)
		priv = hw_chg->charger_private;

	mutex_lock(&msm_chg.status_lock);

	switch (event) {
	case CHG_INSERTED_EVENT:
		if (priv->hw_chg_state != CHG_ABSENT_STATE) {
			dev_info(msm_chg.dev,
				 "%s insertion detected when cbl present",
				 hw_chg->name);
			break;
		}
		update_batt_status();
		if (hw_chg->type == CHG_TYPE_USB) {
			priv->hw_chg_state = CHG_PRESENT_STATE;
			notify_usb_of_the_plugin_event(priv, 1);
			if (usb_chg_current) {
				priv->max_source_current = usb_chg_current;
				usb_chg_current = 0;
				/* usb has already indicated us to charge */
				priv->hw_chg_state = CHG_READY_STATE;
				handle_charger_ready(priv);
			}
		} else {
			priv->hw_chg_state = CHG_READY_STATE;
			handle_charger_ready(priv);
		}
// modified by yafi-begin
#if defined(ORG_VER)
#else
		charging_done_event = false;
		charging_remove_event = false;
		inited_gtime_unplugatfullcharged = false;
#endif
// modified by yafi-end
		break;
	case CHG_ENUMERATED_EVENT:	/* only in USB types */
		if (priv->hw_chg_state == CHG_ABSENT_STATE) {
			dev_info(msm_chg.dev, "%s enum without presence\n",
				 hw_chg->name);
			PrintLog_DEBUG("%s enum without presence",
				 hw_chg->name);
			break;
		}
		update_batt_status();
		dev_dbg(msm_chg.dev, "%s enum with %dmA to draw\n",
			 hw_chg->name, priv->max_source_current);
		PrintLog_DEBUG("%s enum with %dmA to draw",
			 hw_chg->name, priv->max_source_current);
		PrintLog_DEBUG("%s hw_chg_state=%d", __func__, priv->hw_chg_state);
		if (priv->max_source_current == 0) {
			/* usb subsystem doesnt want us to draw
			 * charging current */
			/* act as if the charge is removed */
			if (priv->hw_chg_state != CHG_PRESENT_STATE)
				handle_charger_removed(priv, CHG_PRESENT_STATE);
		} else {
			if (priv->hw_chg_state != CHG_READY_STATE) {
				priv->hw_chg_state = CHG_READY_STATE;
				handle_charger_ready(priv);
			}
		}
		break;
	case CHG_REMOVED_EVENT:
		if (priv->hw_chg_state == CHG_ABSENT_STATE) {
			dev_info(msm_chg.dev, "%s cable already removed\n",
				 hw_chg->name);
			break;
		}
		update_batt_status();
		if (hw_chg->type == CHG_TYPE_USB) {
			usb_chg_current = 0;
			notify_usb_of_the_plugin_event(priv, 0);
		}
		handle_charger_removed(priv, CHG_ABSENT_STATE);
// modified by yafi-begin
#if defined(ORG_VER)
#else
		if(charging_done_event == true)
		{
			charging_done_event = false;
			charging_remove_event = true;
			do_posix_clock_monotonic_gettime(&gtime_unplugatfullcharged);
			monotonic_to_bootbased(&gtime_unplugatfullcharged);
			inited_gtime_unplugatfullcharged = true;
		}
#endif
// modified by yafi-end
		break;
	case CHG_DONE_EVENT:
		if (priv->hw_chg_state == CHG_CHARGING_STATE)
// modified by yafi-begin
#if defined(ORG_VER)
			handle_charging_done(priv);
#else
		{
			handle_charging_done(priv);
			charging_done_event = true;
			charging_remove_event = false;
			inited_gtime_unplugatfullcharged = false;
		}
#endif
// modified by yafi-end
		break;
	case CHG_BATT_BEGIN_FAST_CHARGING:
		/* only update if we are TRKL charging */
		if (msm_chg.batt_status == BATT_STATUS_TRKL_CHARGING)
			msm_chg.batt_status = BATT_STATUS_FAST_CHARGING;
		break;
	case CHG_BATT_NEEDS_RECHARGING:
		msm_chg.batt_status = BATT_STATUS_DISCHARGING;
		handle_battery_inserted();
		priv = msm_chg.current_chg_priv;
		break;
	case CHG_BATT_TEMP_OUTOFRANGE:
		/* the batt_temp out of range can trigger
		 * when the battery is absent */
		if (!is_battery_present()
		    && msm_chg.batt_status != BATT_STATUS_ABSENT) {
			msm_chg.batt_status = BATT_STATUS_ABSENT;
			handle_battery_removed();
			break;
		}
		if (msm_chg.batt_status == BATT_STATUS_TEMPERATURE_OUT_OF_RANGE)
			break;
		msm_chg.batt_status = BATT_STATUS_TEMPERATURE_OUT_OF_RANGE;
		handle_battery_removed();
		break;
	case CHG_BATT_TEMP_INRANGE:
		if (msm_chg.batt_status != BATT_STATUS_TEMPERATURE_OUT_OF_RANGE)
			break;
		msm_chg.batt_status = BATT_STATUS_ID_INVALID;
		/* check id */
		if (!is_battery_id_valid())
			break;
		/* assume that we are discharging from the battery
		 * and act as if the battery was inserted
		 * if a charger is present charging will be resumed */
		msm_chg.batt_status = BATT_STATUS_DISCHARGING;
		handle_battery_inserted();
		break;
	case CHG_BATT_INSERTED:
		if (msm_chg.batt_status != BATT_STATUS_ABSENT)
			break;
		/* debounce */
		if (!is_battery_present())
			break;
		msm_chg.batt_status = BATT_STATUS_ID_INVALID;
		if (!is_battery_id_valid())
			break;
		/* assume that we are discharging from the battery */
		msm_chg.batt_status = BATT_STATUS_DISCHARGING;
		/* check if a charger is present */
		handle_battery_inserted();
		break;
	case CHG_BATT_REMOVED:
		if (msm_chg.batt_status == BATT_STATUS_ABSENT)
			break;
		/* debounce */
		if (is_battery_present())
			break;
		msm_chg.batt_status = BATT_STATUS_ABSENT;
		handle_battery_removed();
		break;
	case CHG_BATT_STATUS_CHANGE:
		/* TODO  battery SOC like battery-alarm/charging-full features
		can be added here for future improvement */
		break;
	}
	dev_dbg(msm_chg.dev, "%s %d done batt_status=%d\n", __func__,
		event, msm_chg.batt_status);
	PrintLog_DEBUG("%s %d done batt_status=%d\n", __func__,
		event, msm_chg.batt_status);
//	PrintLog_DEBUG("%s msm_batt_gauge=0x%x\n", __func__, (int)msm_batt_gauge);
	/* update userspace */
	
// modified by yafi-begin
#if defined(ORG_VER)
	if (msm_batt_gauge)
#else
	if (msm_batt_gauge && (msm_psy_batt.work_wake_lock.flags & WAKE_LOCK_INITIALIZED))
#endif
// modified by yafi-end
		power_supply_changed(&msm_psy_batt);
	
// modified by yafi-begin
#if defined(ORG_VER)
	if (priv)
		power_supply_changed(&priv->psy);
#else
	if (priv)
	{
		PrintLog_DEBUG("%s max_source_current=%d\n",__func__, priv->max_source_current);
		if (priv->max_source_current == 1800)
		{
			power_supply_changed(&priv->psy_ac);
		}
		else
		{
			power_supply_changed(&priv->psy);
		}
	}
#endif
// modified by yafi-end

	mutex_unlock(&msm_chg.status_lock);
}

static int msm_chg_dequeue_event(struct msm_charger_event **event)
{
	unsigned long flags;

	spin_lock_irqsave(&msm_chg.queue_lock, flags);
	if (msm_chg.queue_count == 0) {
		spin_unlock_irqrestore(&msm_chg.queue_lock, flags);
		return -EINVAL;
	}
	*event = &msm_chg.queue[msm_chg.head];
	msm_chg.head = (msm_chg.head + 1) % MSM_CHG_MAX_EVENTS;
	pr_debug("%s dequeueing %d\n", __func__, (*event)->event);
	PrintLog_DEBUG("%s dequeueing %d\n", __func__, (*event)->event);
	msm_chg.queue_count--;
	spin_unlock_irqrestore(&msm_chg.queue_lock, flags);
	return 0;
}

static int msm_chg_enqueue_event(struct msm_hardware_charger *hw_chg,
			enum msm_hardware_charger_event event)
{
	unsigned long flags;

	spin_lock_irqsave(&msm_chg.queue_lock, flags);
	if (msm_chg.queue_count == MSM_CHG_MAX_EVENTS) {
		spin_unlock_irqrestore(&msm_chg.queue_lock, flags);
		pr_err("%s: queue full cannot enqueue %d\n",
				__func__, event);
		return -EAGAIN;
	}
	pr_debug("%s queueing %d\n", __func__, event);
	PrintLog_DEBUG("%s queueing %d\n", __func__, event);
	msm_chg.queue[msm_chg.tail].event = event;
	msm_chg.queue[msm_chg.tail].hw_chg = hw_chg;
	msm_chg.tail = (msm_chg.tail + 1)%MSM_CHG_MAX_EVENTS;
	msm_chg.queue_count++;
	spin_unlock_irqrestore(&msm_chg.queue_lock, flags);
	return 0;
}

static void process_events(struct work_struct *work)
{
	struct msm_charger_event *event;
	int rc;

	do {
		rc = msm_chg_dequeue_event(&event);
		if (!rc)
		{
			PrintLog_DEBUG("%s Event_B-%d\n", __func__, event->event);
			handle_event(event->hw_chg, event->event);
			PrintLog_DEBUG("%s Event_A-%d\n", __func__, event->event);
		}
	} while (!rc);
	PrintLog_DEBUG("%s END\n", __func__);
}

/* USB calls these to tell us how much charging current we should draw */
void msm_charger_vbus_draw(unsigned int mA)
{
// modified by yafi-begin
#if defined(ORG_VER)
#else
	static bool fulled_0mA = false;

	if (is_charging_full && mA==0)
		fulled_0mA = true;
	else if (is_charging_full && mA!=0 && fulled_0mA)
	{
		fulled_0mA = false;
		is_charging_full = false;
	}
#endif
// modified by yafi-end

	if (usb_hw_chg_priv) {
// modified by yafi-begin
#if defined(ORG_VER)
		pr_err("%s %dmA\n", __func__, mA);
#else
		PrintLog_INFO("%s %dmA\n", __func__, mA);
#endif
// modified by yafi-end
		usb_hw_chg_priv->max_source_current = mA;
		msm_charger_notify_event(usb_hw_chg_priv->hw_chg,
						CHG_ENUMERATED_EVENT);
	} else
	{
		/* remember the current, to be used when charger is ready */
		usb_chg_current = mA;
// modified by yafi-begin
#if defined(ORG_VER)
		pr_err("%s called early;charger isnt initialized %dmA\n", __func__, mA);
#else
		PrintLog_INFO("%s called early;charger isnt initialized %dmA\n", __func__, mA);
#endif
// modified by yafi-end
	}
}

static int __init determine_initial_batt_status(void)
{
	int rc;

	if (is_battery_present())
		if (is_battery_id_valid())
			if (is_battery_temp_within_range())
				msm_chg.batt_status = BATT_STATUS_DISCHARGING;
			else
				msm_chg.batt_status
				    = BATT_STATUS_TEMPERATURE_OUT_OF_RANGE;
		else
			msm_chg.batt_status = BATT_STATUS_ID_INVALID;
	else
		msm_chg.batt_status = BATT_STATUS_ABSENT;

	if (is_batt_status_capable_of_charging())
		handle_battery_inserted();

	rc = power_supply_register(msm_chg.dev, &msm_psy_batt);
	if (rc < 0) {
		dev_err(msm_chg.dev, "%s: power_supply_register failed"
			" rc=%d\n", __func__, rc);
		return rc;
	}

	/* start updaing the battery powersupply every msm_chg.update_time
	 * milliseconds */
// modified by yafi-begin
#if defined(ORG_VER)
	queue_delayed_work(msm_chg.event_wq_thread,
				&msm_chg.update_heartbeat_work,
			      round_jiffies_relative(msecs_to_jiffies
						     (msm_chg.update_time)));
#else
	if (cci_charging_boot == 1)
		queue_delayed_work(msm_chg.event_wq_thread,
				&msm_chg.update_heartbeat_work,
			      round_jiffies_relative(msecs_to_jiffies
						     (0.5 * MSEC_PER_SEC)));
	else
		queue_delayed_work(msm_chg.event_wq_thread,
				&msm_chg.update_heartbeat_work,
			      round_jiffies_relative(msecs_to_jiffies
						     (msm_chg.update_time)));
#endif
// modified by yafi-end

	pr_debug("%s:OK batt_status=%d\n", __func__, msm_chg.batt_status);
	PrintLog_DEBUG("%s:OK batt_status=%d\n", __func__, msm_chg.batt_status);
	return 0;
}

static int __devinit msm_charger_probe(struct platform_device *pdev)
{
	msm_chg.dev = &pdev->dev;

	PrintLog_INFO("%s Begin\n", __func__);
	
	if (pdev->dev.platform_data) {

		unsigned int milli_secs;

		struct msm_charger_platform_data *pdata
		    =
		    (struct msm_charger_platform_data *)pdev->dev.platform_data;

		milli_secs = pdata->safety_time * 60 * MSEC_PER_SEC;
		if (milli_secs > jiffies_to_msecs(MAX_JIFFY_OFFSET)) {
			dev_warn(&pdev->dev, "%s: safety time too large"
				 "%dms\n", __func__, milli_secs);
			milli_secs = jiffies_to_msecs(MAX_JIFFY_OFFSET);
		}
		msm_chg.safety_time = milli_secs;

		milli_secs = pdata->update_time * 60 * MSEC_PER_SEC;
		if (milli_secs > jiffies_to_msecs(MAX_JIFFY_OFFSET)) {
			dev_warn(&pdev->dev, "%s: safety time too large"
				 "%dms\n", __func__, milli_secs);
			milli_secs = jiffies_to_msecs(MAX_JIFFY_OFFSET);
		}

// modified by yafi-begin
#if defined(ORG_VER)
		msm_chg.update_time = milli_secs;
#else
//		msm_chg.update_time = 10 * MSEC_PER_SEC;
		msm_chg.update_time = 20 * MSEC_PER_SEC;
#endif
// modified by yafi-end

		msm_chg.max_voltage = pdata->max_voltage;
		msm_chg.min_voltage = pdata->min_voltage;
		msm_chg.get_batt_capacity_percent =
		    pdata->get_batt_capacity_percent;
	}
	if (msm_chg.safety_time == 0)
		msm_chg.safety_time = CHARGING_TEOC_MS;
	if (msm_chg.update_time == 0)
		msm_chg.update_time = UPDATE_TIME_MS;
	if (msm_chg.max_voltage == 0)
		msm_chg.max_voltage = DEFAULT_BATT_MAX_V;
	if (msm_chg.min_voltage == 0)
		msm_chg.min_voltage = DEFAULT_BATT_MIN_V;
	if (msm_chg.get_batt_capacity_percent == NULL)
		msm_chg.get_batt_capacity_percent =
// modified by yafi-begin
#if defined(ORG_VER)
		    msm_chg_get_batt_capacity_percent;
#else
			msm_chg_get_buffered_batt_capacity_percent;
#endif
// modified by yafi-end

	mutex_init(&msm_chg.status_lock);
	INIT_DELAYED_WORK(&msm_chg.teoc_work, teoc);
	INIT_DELAYED_WORK(&msm_chg.update_heartbeat_work, update_heartbeat);
// modified by yafi-begin
#if defined(ORG_VER)
#else
	spin_lock_init(&msm_chg.system_loading_lock);
#endif
// modified by yafi-end	

	wake_lock_init(&msm_chg.wl, WAKE_LOCK_SUSPEND, "msm_charger");
	wake_lock_init(&msm_chg.wl_heartbeat, WAKE_LOCK_SUSPEND, "msm_charger_HB");
	PrintLog_INFO("%s End\n", __func__);

	return 0;
}

static int __devexit msm_charger_remove(struct platform_device *pdev)
{
	wake_lock_destroy(&msm_chg.wl);
	wake_lock_destroy(&msm_chg.wl_heartbeat);	
	mutex_destroy(&msm_chg.status_lock);
	power_supply_unregister(&msm_psy_batt);
	return 0;
}

// modified by yafi-begin
#if defined(ORG_VER)
#else
extern bool smb137b_disable_charger_timeout(bool disable);
bool msm_charger_timeout_disable(bool disable)
{
	if (disable)
	{
		gmsm_charger_timeout_disable = true;
		cancel_delayed_work(&msm_chg.teoc_work);
		if (smb137b_disable_charger_timeout(true))
			return true;
	}
	else
	{
		gmsm_charger_timeout_disable = false;
		if (is_batt_status_charging())
			queue_delayed_work(msm_chg.event_wq_thread,
								&msm_chg.teoc_work,
								round_jiffies_relative(msecs_to_jiffies(msm_chg.safety_time)));
		if (smb137b_disable_charger_timeout(false))
			return true;
	}
	return false;
}

EXPORT_SYMBOL(msm_charger_timeout_disable);
#endif
// modified by yafi-end

int msm_charger_notify_event(struct msm_hardware_charger *hw_chg,
			     enum msm_hardware_charger_event event)
{
	msm_chg_enqueue_event(hw_chg, event);
	queue_work(msm_chg.event_wq_thread, &msm_chg.queue_work);
	return 0;
}
EXPORT_SYMBOL(msm_charger_notify_event);

int msm_charger_register(struct msm_hardware_charger *hw_chg)
{
	struct msm_hardware_charger_priv *priv;
	int rc = 0;

	if (!msm_chg.inited) {
		pr_err("%s: msm_chg is NULL,Too early to register\n", __func__);
		return -EAGAIN;
	}

	if (hw_chg->start_charging == NULL
		|| hw_chg->stop_charging == NULL
		|| hw_chg->name == NULL
		|| hw_chg->rating == 0) {
		pr_err("%s: invalid hw_chg\n", __func__);
		return -EINVAL;
	}

	priv = kzalloc(sizeof *priv, GFP_KERNEL);
	if (priv == NULL) {
		dev_err(msm_chg.dev, "%s kzalloc failed\n", __func__);
		return -ENOMEM;
	}

// modified by yafi-begin
#if defined(ORG_VER)
#else
	priv->psy_ac.name = "ac";
	priv->psy_ac.type = POWER_SUPPLY_TYPE_MAINS;

	priv->psy_ac.supplied_to = msm_power_supplied_to;
	priv->psy_ac.num_supplicants = ARRAY_SIZE(msm_power_supplied_to);
	priv->psy_ac.properties = msm_power_props;
	priv->psy_ac.num_properties = ARRAY_SIZE(msm_power_props);
	priv->psy_ac.get_property = msm_power_get_property_ac;

	rc = power_supply_register(NULL, &priv->psy_ac);
	if (rc) {
		dev_err(msm_chg.dev, "%s power_supply_register AC failed\n",
			__func__);
		goto out;
	}
#endif
// modified by yafi-end

// modified by yafi-begin
#if defined(ORG_VER)
	priv->psy.name = hw_chg->name;
	if (hw_chg->type == CHG_TYPE_USB)
		priv->psy.type = POWER_SUPPLY_TYPE_USB;
	else
		priv->psy.type = POWER_SUPPLY_TYPE_MAINS;
#else
	if (hw_chg->type == CHG_TYPE_USB)
	{
		priv->psy.name = "usb";
		priv->psy.type = POWER_SUPPLY_TYPE_USB;
	}
	else
	{
		priv->psy.name = "ac";
		priv->psy.type = POWER_SUPPLY_TYPE_MAINS;
	}
#endif
// modified by yafi-end

	priv->psy.supplied_to = msm_power_supplied_to;
	priv->psy.num_supplicants = ARRAY_SIZE(msm_power_supplied_to);
	priv->psy.properties = msm_power_props;
	priv->psy.num_properties = ARRAY_SIZE(msm_power_props);
	priv->psy.get_property = msm_power_get_property;

	rc = power_supply_register(NULL, &priv->psy);
	if (rc) {
		dev_err(msm_chg.dev, "%s power_supply_register USB failed\n",
			__func__);
		goto out;
	}

	priv->hw_chg = hw_chg;
	priv->hw_chg_state = CHG_ABSENT_STATE;
	INIT_LIST_HEAD(&priv->list);
	mutex_lock(&msm_chg.msm_hardware_chargers_lock);
	list_add_tail(&priv->list, &msm_chg.msm_hardware_chargers);
	mutex_unlock(&msm_chg.msm_hardware_chargers_lock);
	hw_chg->charger_private = (void *)priv;
	return 0;

out:
// modified by yafi-begin
#if defined(ORG_VER)
#else
	wake_lock_destroy(&msm_chg.wl);
	wake_lock_destroy(&msm_chg.wl_heartbeat);
#endif
// modified by yafi-end
	kfree(priv);
	return rc;
}
EXPORT_SYMBOL(msm_charger_register);

void msm_battery_gauge_register(struct msm_battery_gauge *batt_gauge)
{
	PrintLog_DEBUG("%s entering bg=0x%x\n", __func__, (int)batt_gauge);
	
	if (msm_batt_gauge) {
		msm_batt_gauge = batt_gauge;
		pr_err("msm-charger %s multiple battery gauge called\n",
								__func__);
	} else {
		msm_batt_gauge = batt_gauge;
		determine_initial_batt_status();
	}
}
EXPORT_SYMBOL(msm_battery_gauge_register);

void msm_battery_gauge_unregister(struct msm_battery_gauge *batt_gauge)
{
	msm_batt_gauge = NULL;
}
EXPORT_SYMBOL(msm_battery_gauge_unregister);

int msm_charger_unregister(struct msm_hardware_charger *hw_chg)
{
	struct msm_hardware_charger_priv *priv;

	priv = (struct msm_hardware_charger_priv *)(hw_chg->charger_private);
	mutex_lock(&msm_chg.msm_hardware_chargers_lock);
	list_del(&priv->list);
	mutex_unlock(&msm_chg.msm_hardware_chargers_lock);
// modified by yafi-begin
#if defined(ORG_VER)
#else
	wake_lock_destroy(&msm_chg.wl);
	wake_lock_destroy(&msm_chg.wl_heartbeat);
#endif
// modified by yafi-end
	power_supply_unregister(&priv->psy);
	kfree(priv);
	return 0;
}
EXPORT_SYMBOL(msm_charger_unregister);

static int msm_charger_suspend(struct device *dev)
{
	dev_dbg(msm_chg.dev, "%s suspended\n", __func__);
	PrintLog_INFO("%s suspended\n", __func__);
	msm_chg.stop_update = 1;
	cancel_delayed_work(&msm_chg.update_heartbeat_work);
	mutex_lock(&msm_chg.status_lock);
	handle_battery_removed();
	mutex_unlock(&msm_chg.status_lock);
	return 0;
}

static int msm_charger_resume(struct device *dev)
{
	dev_dbg(msm_chg.dev, "%s resumed\n", __func__);
	PrintLog_INFO("%s resumed\n", __func__);
	msm_chg.stop_update = 0;
	/* start updaing the battery powersupply every msm_chg.update_time
	 * milliseconds */
	queue_delayed_work(msm_chg.event_wq_thread,
				&msm_chg.update_heartbeat_work,
			      round_jiffies_relative(msecs_to_jiffies
						     (msm_chg.update_time)));
	mutex_lock(&msm_chg.status_lock);
	handle_battery_inserted();
	mutex_unlock(&msm_chg.status_lock);
	return 0;
}

static SIMPLE_DEV_PM_OPS(msm_charger_pm_ops,
		msm_charger_suspend, msm_charger_resume);

static struct platform_driver msm_charger_driver = {
	.probe = msm_charger_probe,
	.remove = __devexit_p(msm_charger_remove),
	.driver = {
		   .name = "msm-charger",
		   .owner = THIS_MODULE,
		   .pm = &msm_charger_pm_ops,
	},
};

static int __init msm_charger_init(void)
{
	int rc;

	PrintLog_INFO("%s Begin\n", __func__);

	INIT_LIST_HEAD(&msm_chg.msm_hardware_chargers);
	msm_chg.count_chargers = 0;
	mutex_init(&msm_chg.msm_hardware_chargers_lock);

	msm_chg.queue = kzalloc(sizeof(struct msm_charger_event)
				* MSM_CHG_MAX_EVENTS,
				GFP_KERNEL);
	if (!msm_chg.queue) {
		rc = -ENOMEM;
		goto out;
	}
	msm_chg.tail = 0;
	msm_chg.head = 0;
	spin_lock_init(&msm_chg.queue_lock);
	msm_chg.queue_count = 0;
	INIT_WORK(&msm_chg.queue_work, process_events);
	msm_chg.event_wq_thread = create_workqueue("msm_charger_eventd");
	if (!msm_chg.event_wq_thread) {
		rc = -ENOMEM;
		goto free_queue;
	}
	rc = platform_driver_register(&msm_charger_driver);
	if (rc < 0) {
		pr_err("%s: FAIL: platform_driver_register. rc = %d\n",
		       __func__, rc);
		goto destroy_wq_thread;
	}
	msm_chg.inited = 1;
	PrintLog_INFO("%s End\n", __func__);
	return 0;

destroy_wq_thread:
	destroy_workqueue(msm_chg.event_wq_thread);
free_queue:
	kfree(msm_chg.queue);
out:
	return rc;
}

static void __exit msm_charger_exit(void)
{
	flush_workqueue(msm_chg.event_wq_thread);
	destroy_workqueue(msm_chg.event_wq_thread);
	kfree(msm_chg.queue);
	platform_driver_unregister(&msm_charger_driver);
}

module_init(msm_charger_init);
module_exit(msm_charger_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Abhijeet Dharmapurikar <adharmap@codeaurora.org>");
MODULE_DESCRIPTION("Battery driver for Qualcomm MSM chipsets.");
MODULE_VERSION("1.0");
