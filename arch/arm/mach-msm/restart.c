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
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/reboot.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/mfd/pmic8058.h>
#include <linux/mfd/pmic8901.h>

#include <mach/msm_iomap.h>
#include <mach/restart.h>
#include <mach/scm-io.h>
#include <asm/mach-types.h>

//[DA80] ===> BugID#789 : Refine KLog magic, added by Jimmy@CCI
#ifdef CONFIG_CCI_KLOG_COLLECTOR
#include <linux/klog_collector.h>
#endif // #ifdef CONFIG_CCI_KLOG_COLLECTOR
//[DA80] <=== BugID#789 : Refine KLog magic, added by Jimmy@CCI

#define TCSR_WDT_CFG 0x30

#define WDT0_RST       (MSM_TMR0_BASE + 0x38)
#define WDT0_EN        (MSM_TMR0_BASE + 0x40)
#define WDT0_BARK_TIME (MSM_TMR0_BASE + 0x4C)
#define WDT0_BITE_TIME (MSM_TMR0_BASE + 0x5C)

#define PSHOLD_CTL_SU (MSM_TLMM_BASE + 0x820)

#define IMEM_BASE           0x2A05F000

#define RESTART_REASON_ADDR 0x65C
#define DLOAD_MODE_ADDR     0x0

static int restart_mode;
void *restart_reason;

#ifdef CONFIG_MSM_DLOAD_MODE
static int in_panic;
static void *dload_mode_addr;

/* Download mode master kill-switch */
static int dload_set(const char *val, struct kernel_param *kp);
static int download_mode = 1;
module_param_call(download_mode, dload_set, param_get_int,
			&download_mode, 0644);
//[DA80] ===> BugID#1212 : Add parameter for record pshold/wd state, Added by Gary	
#define DLOAD_MODE_ADDR_NEW  0x950
static void *dload_mode_addr2;
extern int modem_fatal_err_enable;
//[DA80] ===> BugID#1212 : Add parameter for modem fatal error notice, Added by Gary	
static int panic_prep_restart(struct notifier_block *this,
			      unsigned long event, void *ptr)
{
	in_panic = 1;
	return NOTIFY_DONE;
}

static struct notifier_block panic_blk = {
	.notifier_call	= panic_prep_restart,
};

static void set_dload_mode(int on)
{
	if (dload_mode_addr) 	{
//[DA80] ===> BugID#789 : Refine KLog magic, added by Jimmy@CCI
#ifdef CONFIG_CCI_KLOG_COLLECTOR

//[DA80] ===> BugID#1212 : Refine Restart/Download condition, changed by Gary		
	  if((in_panic == 0) && (modem_fatal_err_enable == 0))
	  {
		  if(on == 1)
		  {
			  cklc_save_magic(KLOG_MAGIC_DOWNLOAD_MODE, KLOG_STATE_DOWNLOAD_MODE);
			  //default reset magic numbor,normal download mode
			  writel(0xE47B337D, dload_mode_addr);
			  writel(0xCE14091A, dload_mode_addr+ sizeof(unsigned int));					   
				  writel(0xE47B337D, dload_mode_addr2+ sizeof(unsigned int));
				  writel(0xCE14091A, dload_mode_addr2+ sizeof(unsigned int)*2);	
		  }
		  else
		  {
			  //normal reset
			  writel(0, dload_mode_addr);
			  writel(0, dload_mode_addr+ sizeof(unsigned int));	
				  writel(0, dload_mode_addr2+ sizeof(unsigned int));
				  writel(0, dload_mode_addr2+ sizeof(unsigned int)*2);	
		  }
	  }
	  else
	  {
		  //cci's magic number, for SD download
		  writel(0x43434952, dload_mode_addr);
		  writel(0x414D4450, dload_mode_addr+ sizeof(unsigned int));						
			  writel(0x43434952, dload_mode_addr2+ sizeof(unsigned int));
			  writel(0x414D4450, dload_mode_addr2+ sizeof(unsigned int)*2);						
//[DA80] ===> BugID#1212 : Add parameter for modem fatal error notice, Added by Gary	
	  }
#else // #ifdef CONFIG_CCI_KLOG_COLLECTOR
		writel(on ? 0xE47B337D : 0, dload_mode_addr);
		writel(on ? 0xCE14091A : 0,
		       dload_mode_addr + sizeof(unsigned int));
#endif // #ifdef CONFIG_CCI_KLOG_COLLECTOR
//[DA80] <=== BugID#789 : Refine KLog magic, added by Jimmy@CCI
		mb();
	}
}

static int dload_set(const char *val, struct kernel_param *kp)
{
	int ret;
	int old_val = download_mode;

	ret = param_set_int(val, kp);

	if (ret)
		return ret;

	/* If download_mode is not zero or one, ignore. */
	if (download_mode >> 1) {
		download_mode = old_val;
		return -EINVAL;
	}

	set_dload_mode(download_mode);

	return 0;
}
#else
#define set_dload_mode(x) do {} while (0)
#endif

void msm_set_restart_mode(int mode)
{
	restart_mode = mode;
}
EXPORT_SYMBOL(msm_set_restart_mode);

static void msm_power_off(void)
{
	printk(KERN_NOTICE "Powering off the SoC\n");
#ifdef CONFIG_MSM_DLOAD_MODE
	set_dload_mode(0);
#endif
	pm8058_reset_pwr_off(0);
	pm8901_reset_pwr_off(0);
	writel(0, PSHOLD_CTL_SU);
	mdelay(10000);
	printk(KERN_ERR "Powering off has failed\n");
	return;
}

void arch_reset(char mode, const char *cmd)
{
//[DA80] ===> BugID#789 : Refine KLog magic, added by Jimmy@CCI
#ifdef CONFIG_CCI_KLOG_COLLECTOR
	char buf[7] = {0};
#endif // #ifdef CONFIG_CCI_KLOG_COLLECTOR
//[DA80] <=== BugID#789 : Refine KLog magic, added by Jimmy@CCI

#ifdef CONFIG_MSM_DLOAD_MODE

	/* This looks like a normal reboot at this point. */
	set_dload_mode(0);

	/* Write download mode flags if we're panic'ing */
//[DA80] ===> BugID#1212 : Refine Restart/Download condition, changed by Gary		
	//set_dload_mode(in_panic);
//[DA80] ===> BugID#1212 : Refine Restart/Download condition, changed by Gary		
	/* Write download mode flags if restart_mode says so */
	if (restart_mode == RESTART_DLOAD)
		set_dload_mode(1);
//[DA80] ===> BugID#1212 : Refine Restart/Download condition, changed by Gary		
	if((in_panic == 1) || (modem_fatal_err_enable == 1))
		set_dload_mode(1);
//[DA80] ===> BugID#1212 : Refine Restart/Download condition, changed by Gary		
	
	/* Kill download mode if master-kill switch is set */
//[DA80] ===> BugID#1212 : Refine Restart/Download condition, changed by Gary		
	//if (!download_mode)
	//	set_dload_mode(0);
//[DA80] ===> BugID#1212 : Refine Restart/Download condition, changed by Gary		
#endif

	printk(KERN_NOTICE "Going down for restart now\n");

	pm8058_reset_pwr_off(1);

//[DA80] ===> BugID#789 : Refine KLog magic, added by Jimmy@CCI
#ifdef CONFIG_CCI_KLOG_COLLECTOR
	if (cmd != NULL) {
		if (!strncmp(cmd, "bootloader", 10)) {
			writel(0x77665500, restart_reason);
			cklc_save_magic(KLOG_MAGIC_BOOTLOADER, KLOG_STATE_NONE);
		} else if (!strncmp(cmd, "recovery", 8)) {
			writel(0x77665502, restart_reason);
			cklc_save_magic(KLOG_MAGIC_RECOVERY, KLOG_STATE_NONE);
		} else if (!strncmp(cmd, "oem-", 4)) {
			unsigned long code;
			strict_strtoul(cmd + 4, 16, &code);
			code = code & 0xff;
			writel(0x6f656d00 | code, restart_reason);
			snprintf(buf, KLOG_MAGIC_LENGTH + 1, "%s%lX", KLOG_MAGIC_OEM_COMMAND, code);//7
			printk("[klog]OEM command:code=%lX, buf=%s\n", code, buf);
			cklc_save_magic(buf, KLOG_STATE_NONE);
		} else {
			writel(0x77665501, restart_reason);
			cklc_save_magic(KLOG_MAGIC_REBOOT, KLOG_STATE_NONE);
		}
	}
	else
	{
		cklc_save_magic(KLOG_MAGIC_REBOOT, KLOG_STATE_NONE);
	}
#else // #ifdef CONFIG_CCI_KLOG_COLLECTOR
	if (cmd != NULL) {
		if (!strncmp(cmd, "bootloader", 10)) {
			writel(0x77665500, restart_reason);
		} else if (!strncmp(cmd, "recovery", 8)) {
			writel(0x77665502, restart_reason);
		} else if (!strncmp(cmd, "oem-", 4)) {
			unsigned long code;
			strict_strtoul(cmd + 4, 16, &code);
			code = code & 0xff;
			writel(0x6f656d00 | code, restart_reason);
		} else {
			writel(0x77665501, restart_reason);
		}
	}
#endif // #ifdef CONFIG_CCI_KLOG_COLLECTOR
//[DA80] <=== BugID#789 : Refine KLog magic, added by Jimmy@CCI

	writel(0, WDT0_EN);
	if (!(machine_is_msm8x60_charm_surf() ||
	      machine_is_msm8x60_charm_ffa())) {
		dsb();
		writel(0, PSHOLD_CTL_SU); /* Actually reset the chip */
//[DA80] ===> BugID#1212 : Add record for pshold/wd state, Added by Gary	
#ifdef CONFIG_MSM_DLOAD_MODE
		writel(readl(PSHOLD_CTL_SU), dload_mode_addr2+ sizeof(unsigned int)*3);
		writel(readl(WDT0_EN), dload_mode_addr2+ sizeof(unsigned int)*4);		
		writel(restart_mode, dload_mode_addr2+ sizeof(unsigned int)*5);	
		writel(restart_reason, dload_mode_addr2+ sizeof(unsigned int)*6);			
		mb();		
#endif
//[DA80] ===> BugID#1212 : Add record for pshold/wd state, Added by Gary	
		mdelay(5000);
		pr_notice("PS_HOLD didn't work, falling back to watchdog\n");
	}

	__raw_writel(1, WDT0_RST);
	__raw_writel(5*0x31F3, WDT0_BARK_TIME);
	__raw_writel(0x31F3, WDT0_BITE_TIME);
	__raw_writel(3, WDT0_EN);
	secure_writel(3, MSM_TCSR_BASE + TCSR_WDT_CFG);

	mdelay(10000);
	printk(KERN_ERR "Restarting has failed\n");
}

static int __init msm_restart_init(void)
{
	void *imem = ioremap_nocache(IMEM_BASE, SZ_4K);

#ifdef CONFIG_MSM_DLOAD_MODE
	atomic_notifier_chain_register(&panic_notifier_list, &panic_blk);
	dload_mode_addr = imem + DLOAD_MODE_ADDR;
//[DA80] ===> BugID#1212 : Add record for pshold/wd state, Added by Gary	
	dload_mode_addr2 = imem + DLOAD_MODE_ADDR_NEW;
	writel(0x11223344,dload_mode_addr2);
//[DA80] ===> BugID#1212 : Add record for pshold/wd state, Added by Gary	
	/* Reset detection is switched on below.*/
	
	//S: Leon, Don't set magic number in IRAM to let HW reset (pwr_key + vol_down) can work
	//   If we don't do this, device will enter SBL3 download mode after HW reset.
	#ifdef ORI_VER
	set_dload_mode(1);
	#else
	//set_dload_mode(1);
	#endif
	//E: Leon, Don't set magic number in IRAM to let HW reset (pwr_key + vol_down) can work
	//   If we don't do this, device will enter SBL3 download mode after HW reset.
#endif
	restart_reason = imem + RESTART_REASON_ADDR;
	pm_power_off = msm_power_off;

	return 0;
}

late_initcall(msm_restart_init);
