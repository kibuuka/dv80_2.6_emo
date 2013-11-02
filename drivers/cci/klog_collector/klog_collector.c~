#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/time.h>
#include <linux/ctype.h>
#include <linux/console.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/ptrace.h>
#include <linux/spinlock.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <asm/ioctl.h>
#include <asm/cputime.h>
#include <mach/msm_iomap.h>
#include <mach/cci_hw_id.h>
#include <linux/klog_collector.h>

MODULE_AUTHOR("Kevin Chiang <Kevin_Chiang@Compalcomm.com>");
MODULE_LICENSE("Proprietary");
MODULE_DESCRIPTION("Kernel Log Collector");

/*******************************************************************************
* Local Variable/Structure Declaration
*******************************************************************************/

/* Check Invalid Category */
#define CHK_CATEGORY(x) \
				do\
				{\
					if(x == KLOG_MAX_NUM)\
					{\
						return;\
					}\
					if(unlikely(x > KLOG_MAX_NUM))\
					{\
						printk(KERN_ERR "%s:%s():%u --> Func Caller:<%p> sent an Invalid KLog Category:%u\n",\
							__FILE__, __func__, __LINE__,\
							__builtin_return_address(0),\
							x );\
						return;/* FIXME: Kevin_Chiang@CCI: Too coupling coding style */\
					}\
				}\
				while(0)

#define MSM_KLOG_MAGIC MSM_KLOG_BASE
#define MSM_KLOG_MAIN (MSM_KLOG_BASE + CONFIG_CCI_KLOG_HEADER_SIZE)
struct cklog_t * pcklog = (void *) MSM_KLOG_MAIN;	/* The collection of all logs */
char *klog_magic = (void *) MSM_KLOG_MAGIC;		/* magic number */
static int mem_ready = 0;				/* 0: memory is not ready, 1: memory is ready */
static int mem_have_clean = 0;				/* 0: memory is not clean, 1: memory has be clean */
static int magic_priority = KLOG_PRIORITY_INVALID;
static int device_info_update = 0;
static int crashing = 0;
static int RTC_synced = 0;
static struct klog_magic_list kml[] = 	{
						{KLOG_MAGIC_FORCE_CLEAR, KLOG_PRIORITY_FORCE_CLEAR},
						{KLOG_MAGIC_MARM_FATAL, KLOG_PRIORITY_MARM_FATAL},
						{KLOG_MAGIC_AARM_PANIC, KLOG_PRIORITY_AARM_PANIC},
						{KLOG_MAGIC_DOWNLOAD_MODE, KLOG_PRIORITY_DOWNLOAD_MODE},
						{KLOG_MAGIC_POWER_OFF, KLOG_PRIORITY_POWER_OFF},
						{KLOG_MAGIC_REBOOT, KLOG_PRIORITY_NATIVE_COMMAND},
						{KLOG_MAGIC_BOOTLOADER, KLOG_PRIORITY_NATIVE_COMMAND},
						{KLOG_MAGIC_RECOVERY, KLOG_PRIORITY_NATIVE_COMMAND},
						{KLOG_MAGIC_OEM_COMMAND, KLOG_PRIORITY_OEM_COMMAND},
						{KLOG_MAGIC_INIT, KLOG_PRIORITY_KLOG_INIT},
					};

/*******************************************************************************
* Local Function Declaration
*******************************************************************************/

/*******************************************************************************
* External Variable/Structure Declaration
*******************************************************************************/

/*******************************************************************************
* External Function Declaration
*******************************************************************************/

/*** Functions ***/

#ifdef CONFIG_CCI_KLOG_RECORD_KERNEL_TIMESTAMP
struct timespec klog_record_kernel_timestamp(void)
{
	struct timespec now_rtc = current_kernel_time();
	unsigned long long now_clock;
	unsigned long now_clock_ns;

	if(mem_ready && !(crashing == 0 && (check_old_log() == KLOG_PRIORITY_MARM_FATAL || check_old_log() == KLOG_PRIORITY_AARM_PANIC)))
	{
//record kernel clock time(uptime)
		now_clock = cpu_clock(UINT_MAX);
		now_clock_ns = do_div(now_clock, 1000000000);
		snprintf(klog_magic + KLOG_MAGIC_TOTAL_LENGTH, KLOG_KERNEL_TIME_LENGTH, "[%08lX.%08lX]", (unsigned long) now_clock, now_clock_ns);

//record kernel first RTC sync time
		if(RTC_synced == 0 && now_rtc.tv_sec > 30)
		{
			RTC_synced = 1;
			snprintf(klog_magic + KLOG_MAGIC_TOTAL_LENGTH + KLOG_KERNEL_TIME_LENGTH, KLOG_FIRST_RTC_TIMESTAMP_LENGTH, "[%08lX.%08lX]", now_rtc.tv_sec, now_rtc.tv_nsec);
		}

//record kernel RTC time
		snprintf(klog_magic + KLOG_MAGIC_TOTAL_LENGTH + KLOG_KERNEL_TIME_LENGTH + KLOG_FIRST_RTC_TIMESTAMP_LENGTH, KLOG_LAST_RTC_TIMESTAMP_LENGTH, "[%08lX.%08lX]", now_rtc.tv_sec, now_rtc.tv_nsec);
	}

	return now_rtc;
}
EXPORT_SYMBOL(klog_record_kernel_timestamp);
#endif // #ifdef CONFIG_CCI_KLOG_RECORD_KERNEL_TIMESTAMP

static __inline__ void __cklc_append_char(unsigned int category, unsigned char c)
{
	if(!mem_ready)
	{
		return;
	}

	if(crashing == 0 && (check_old_log() == KLOG_PRIORITY_MARM_FATAL || check_old_log() == KLOG_PRIORITY_AARM_PANIC))
	{
		return;
	}

	if(!mem_have_clean)
	{
		memset(pcklog, 0, sizeof(struct cklog_t) * KLOG_MAX_NUM);
		mem_have_clean = 1;
	}

	(pcklog + category)->buffer[(pcklog + category)->w_idx] = c;

	(pcklog + category)->w_idx = ((pcklog + category)->w_idx + 1) % CONFIG_CCI_KLOG_CATEGORY_SIZE;

	if((pcklog + category)->w_idx == 0)
	{
		(pcklog + category)->overload = 1;
	}

	return;
}

/* This func. should only be used in printk.c */
void cklc_append_kernel_raw_char(unsigned char c)
{
	__cklc_append_char(KLOG_KERNEL, c);

	return;
}
EXPORT_SYMBOL(cklc_append_kernel_raw_char);

/* Append One Character into Log[category] */
void cklc_append_char(unsigned int category, unsigned char c)
{
	CHK_CATEGORY(category);

	if(isprint(c) || c == '\0')
	{
		__cklc_append_char(category, c);
	}

	return;
}
EXPORT_SYMBOL(cklc_append_char);

static __inline__ void __cklc_append_str(unsigned int category, unsigned char *str, size_t len)
{
	int i = 0;

	for(i = 0; i < len; i++)
	{
		if(isprint(*(str + i)))
		{
			__cklc_append_char(category, *(str + i));
		}
	}

	return;
}

/* Append String into Log[category] */
void cklc_append_str(unsigned int category, unsigned char *str, size_t len)
{
	CHK_CATEGORY(category);

#if 0
	if(category == KLOG_ANDROID_EVENTS)
	{
		printk(KERN_INFO "%s:%d --> Receive str:%s\n", __func__, __LINE__, str);
	}
#endif

	__cklc_append_str(category, str, len);

	return;
}
EXPORT_SYMBOL(cklc_append_str);

static __inline__ void __cklc_append_newline(unsigned int category)
{
	__cklc_append_char(category, '\n');

	return;
}

/* Append New Line '\n' into Log[category] */
void cklc_append_newline(unsigned int category)
{
	CHK_CATEGORY(category);

	__cklc_append_newline(category);

	return;
}
EXPORT_SYMBOL(cklc_append_newline);

static __inline__ void __cklc_append_separator(unsigned int category)
{
	unsigned char *sp = " | ";

	__cklc_append_str(category, sp, strlen(sp));

	return;
}

/* Append Separator '|' into Log[category] */
void cklc_append_separator(unsigned int category)
{
	CHK_CATEGORY(category);

	__cklc_append_separator(category);

	return;
}
EXPORT_SYMBOL(cklc_append_separator);

static __inline__ void __cklc_append_time_header(unsigned int category)
{
	unsigned char tbuf[32] = {0};
	unsigned tlen = 0;
	struct timespec now;

#ifdef CONFIG_CCI_KLOG_RECORD_KERNEL_TIMESTAMP
	now = klog_record_kernel_timestamp();
#else // #ifdef CONFIG_CCI_KLOG_RECORD_KERNEL_TIMESTAMP
	now = current_kernel_time();
#endif // #ifdef CONFIG_CCI_KLOG_RECORD_KERNEL_TIMESTAMP

	tlen = snprintf(tbuf, sizeof(tbuf), "[%8lx.%08lx] ", now.tv_sec, now.tv_nsec);

	__cklc_append_str(category, tbuf, tlen);

	return;
}

/*
 * Append Unix Epoch Time as the Line Header
 * to align with other Kernel, Andorid Log for easy time tracing
 */
void cklc_append_time_header(unsigned int category)
{
	CHK_CATEGORY(category);

	__cklc_append_time_header(category);

	return;
}
EXPORT_SYMBOL(cklc_append_time_header);

/*
 * For Android Logger Version
 * drivers/misc/logger.c : d9312a065c888587f0cdd0e882bbac74bf734ea5
 */
void cklc_append_android_log(unsigned int category,
					const unsigned char *priority,
					const char * const tag,
					const int tag_bytes,
					const char * const msg,
					const int msg_bytes)
{
	int prilen = 0;
	unsigned char pribuf[8] = {0};

	__cklc_append_time_header(category);

	prilen = snprintf(pribuf, sizeof(pribuf), "<%u> ", (unsigned int)*priority);
	__cklc_append_str(category, pribuf, prilen);

	__cklc_append_str(category, (unsigned char *)tag, (unsigned int)tag_bytes);
	__cklc_append_separator(category);

	__cklc_append_str(category, (unsigned char *)msg, (unsigned int)msg_bytes);
	__cklc_append_newline(category);

	return;
}
EXPORT_SYMBOL(cklc_append_android_log);

static void console_output(unsigned char *buf, unsigned int len)
{
	struct console *con;

	for(con = console_drivers; con; con = con->next)
	{
		if((con->flags & CON_ENABLED) && con->write &&
				(cpu_online(smp_processor_id()) ||
				(con->flags & CON_ANYTIME)))
		{
			con->write(con, buf, len);
		}
	}

	return;
}

static void __show_android_log_to_console(unsigned int category)
{
	unsigned int len = 0;
	unsigned char strbuf[80] = {0};

	len = snprintf(strbuf, sizeof(strbuf), "\n\n============================= KLog Start =============================\n");
	console_output(strbuf, len);
	len = snprintf(strbuf, sizeof(strbuf), "KLog Category Name:[%u]%s\nKLog Ring Buffer:\n", category, (pcklog + category)->name);
	console_output(strbuf, len);

	if((pcklog + category)->overload == 0 && (pcklog + category)->w_idx == 0)
	{
		len = snprintf(strbuf, sizeof(strbuf), "<Empty>");
		console_output(strbuf, len);
	}
	else
	{
		if((pcklog + category)->overload == 0)
		{
			console_output(&(pcklog + category)->buffer[0], (pcklog + category)->w_idx);
		}
		else
		{
			console_output(&(pcklog + category)->buffer[(pcklog + category)->w_idx], CONFIG_CCI_KLOG_CATEGORY_SIZE - (pcklog + category)->w_idx);
			console_output(&(pcklog + category)->buffer[0], (pcklog + category)->w_idx - 1);
		}
	}

	len = snprintf(strbuf, sizeof(strbuf), "\n============================== KLog End ==============================\n");
	console_output(strbuf, len);

	return;
}

/* Show All Logs to All the Console Drivers. ex. UART */
void show_android_log_to_console(void)
{
	int i = 0;

	for(i = 0; i < KLOG_MAX_NUM; i++)
	{
		__show_android_log_to_console(i);
	}

	return;
}
EXPORT_SYMBOL(show_android_log_to_console);

int atoi(char *str)
{
	int val = 0;

	for(;; str++)
	{
		switch(*str)
		{
			case '0' ... '9':
				val = 10 * val + (*str - '0');
				break;
			default:
				return val;
		}
	}

	return 0;
}

int get_magic_priority(char *magic)
{
	char buf[KLOG_MAGIC_LENGTH + 1] = {0};

	int i;

	if(magic && strlen(magic) > 0)
	{
		strncpy(buf, magic, KLOG_MAGIC_LENGTH);

		for(i = 0; i < sizeof(kml) / sizeof(struct klog_magic_list); i++)
		{
			if(!strncmp(magic, kml[i].name, strlen(kml[i].name)))
			{
				return kml[i].priority;
			}
		}
	}

	return KLOG_PRIORITY_INVALID;
}

int check_old_log(void)
{
	if(magic_priority == KLOG_PRIORITY_INVALID)
	{
		if(klog_magic && strlen(klog_magic) > 0)
		{
			magic_priority = get_magic_priority(klog_magic);
		}
		else
		{
			magic_priority = KLOG_PRIORITY_INVALID;
		}
	}

	return magic_priority;
}

void cklc_save_magic(char *magic, int state)
{
	char buf[KLOG_MAGIC_TOTAL_LENGTH] = {0};
	int priority = KLOG_PRIORITY_INVALID;
	int magic_update = 0;

	if(klog_magic && strlen(klog_magic) > 0)
	{
		printk("[klog]klog_magic=%s\n", klog_magic);
	}

	if(magic_priority == KLOG_PRIORITY_INVALID)
	{
		magic_priority = check_old_log();
	}

	if(magic && strlen(magic) > 0)
	{
		strncpy(buf, magic, KLOG_MAGIC_LENGTH);
		strncpy(buf + KLOG_MAGIC_LENGTH, klog_magic + KLOG_MAGIC_LENGTH, KLOG_STATE_LENGTH);
		printk("[klog]preset magic(prepare):magic=%s, state=%d, buf=%s\n", magic, state, buf);

		priority = get_magic_priority(buf);

		if(priority == KLOG_PRIORITY_MARM_FATAL || priority == KLOG_PRIORITY_AARM_PANIC)//mARM fatal or aARM panic
		{
			crashing = 1;
		}
	}
	else
	{
		priority = KLOG_PRIORITY_INVALID;
	}

	printk("[klog]magic_priority=%d, priority=%d, state=%d\n", magic_priority, priority, state);

//magic
	if(magic_priority == KLOG_PRIORITY_INVALID || !strncmp(klog_magic, KLOG_MAGIC_POWER_OFF, KLOG_MAGIC_LENGTH))//cold-boot
	{
		state = 0;
		if(priority == KLOG_PRIORITY_INVALID)//invalid magic, init klog with default magic
		{
			magic_update = 1;
			magic_priority = KLOG_PRIORITY_KLOG_INIT;
			snprintf(buf, KLOG_MAGIC_TOTAL_LENGTH, "%s%s", KLOG_MAGIC_INIT, KLOG_STATE_INIT_CODE);
			printk("[klog]preset magic(cold-boot invalid):magic=%s, state=%s, buf=%s\n", KLOG_MAGIC_INIT, KLOG_STATE_INIT_CODE, buf);
		}
		else//valid magic, init klog with specified magic
		{
			magic_update = 1;
			magic_priority = priority;
			strncpy(buf + KLOG_MAGIC_LENGTH, KLOG_STATE_INIT_CODE, KLOG_STATE_LENGTH);
			printk("[klog]preset magic(cold-boot valid):magic=%s, state=%s, buf=%s\n", magic, KLOG_STATE_INIT_CODE, buf);
		}
	}
	else//warn-boot
	{
		if(priority == KLOG_PRIORITY_INVALID)//invalid magic, do not update magic
		{
			strncpy(buf, klog_magic, KLOG_MAGIC_LENGTH + KLOG_STATE_LENGTH);
			printk("[klog]preset magic(warn-boot invalid):state=%d, buf=%s\n", state, buf);
		}
		else//valid magic
		{
			if(!strncmp(buf, KLOG_MAGIC_FORCE_CLEAR, KLOG_MAGIC_LENGTH))//force clear
			{
				state = 0;
				if(crashing == 0)
				{
					magic_update = 1;
					magic_priority = KLOG_PRIORITY_KLOG_INIT;
					snprintf(buf, KLOG_MAGIC_TOTAL_LENGTH, "%s%s", KLOG_MAGIC_INIT, KLOG_STATE_INIT_CODE);
					printk("[klog]preset magic(force):magic=%s, state=%s, buf=%s\n", KLOG_MAGIC_INIT, KLOG_STATE_INIT_CODE, buf);
				}
				else
				{
					strncpy(buf, klog_magic, KLOG_MAGIC_LENGTH + KLOG_STATE_LENGTH);
					printk("[klog]preset magic(!force):magic=%s, state=%d, buf=%s\n", magic, state, buf);
				}
			}
			else if(!strncmp(buf, KLOG_MAGIC_INIT, KLOG_MAGIC_LENGTH))//klog init
			{
				state = 0;
				if(crashing == 0 && magic_priority != KLOG_PRIORITY_MARM_FATAL && magic_priority != KLOG_PRIORITY_AARM_PANIC)//only allow to clear if not panic
				{
					magic_update = 1;
					magic_priority = KLOG_PRIORITY_KLOG_INIT;
					snprintf(buf, KLOG_MAGIC_TOTAL_LENGTH, "%s%s", KLOG_MAGIC_INIT, KLOG_STATE_INIT_CODE);
					printk("[klog]preset magic(init):magic=%s, state=%s, buf=%s\n", KLOG_MAGIC_INIT, KLOG_STATE_INIT_CODE, buf);
				}
				else
				{
					strncpy(buf, klog_magic, KLOG_MAGIC_LENGTH + KLOG_STATE_LENGTH);
					printk("[klog]preset magic(!init):magic=%s, state=%d, buf=%s\n", magic, state, buf);
				}
			}
			else
			{
				if(priority < magic_priority)//higher priority magic, update magic
				{
					magic_update = 1;
					magic_priority = priority;
					printk("[klog]preset magic(warn-boot higher):magic=%s, state=%d, buf=%s\n", magic, state, buf);
				}
				else//lower or same priority magic, do not update magic
				{
					strncpy(buf, klog_magic, KLOG_MAGIC_LENGTH + KLOG_STATE_LENGTH);
					printk("[klog]preset magic(warn-boot lower):magic=%s, state=%d, buf=%s\n", magic, state, buf);
				}
			}
		}
	}

//state
	if(state != 0)
	{
		if(state == KLOG_STATE_INIT)
		{
			if(crashing == 0 && magic_priority != KLOG_PRIORITY_MARM_FATAL && magic_priority != KLOG_PRIORITY_AARM_PANIC)//only allow to clear if not panic
			{
				strncpy(buf + KLOG_MAGIC_LENGTH, KLOG_STATE_INIT_CODE, KLOG_STATE_LENGTH);
				printk("[klog]preset state(init):magic=%s, state=%s, buf=%s\n", magic, KLOG_STATE_INIT_CODE, buf);
			}
			else//panic, do not update state
			{
				strncpy(buf + KLOG_MAGIC_LENGTH, klog_magic + KLOG_MAGIC_LENGTH, KLOG_STATE_LENGTH);
				printk("[klog]preset state(panic):magic=%s, state=%s, buf=%s\n", magic, KLOG_STATE_INIT_CODE, buf);
			}
		}
		else
		{
			if(magic_priority != KLOG_PRIORITY_INVALID)//klog inited, the state should be a valid value
			{
				state = state | atoi(buf + KLOG_MAGIC_LENGTH);
			}

			state = state & 0xFFF;
			snprintf(buf + KLOG_MAGIC_LENGTH, KLOG_STATE_LENGTH + 1, "%03X", state);
			printk("[klog]preset state(update):magic=%s, state=%d, buf=%s\n", magic, state, buf);
		}
	}

//update
	snprintf(klog_magic, KLOG_MAGIC_TOTAL_LENGTH, "%s", buf);
	printk("[klog]new klog_magic:klog_magic=%s, buf=%s\n", klog_magic, buf);

//hw_id
	if(magic_update == 1 && device_info_update == 0)
	{
		device_info_update = 1;
		snprintf(klog_magic + KLOG_INFO_LENGTH + KLOG_IMAGE_INFO_LENGTH, KLOG_HW_ID_LENGTH, "%01X%01X", cci_hw_id, cci_band_id);
		printk("[klog]hw_info:cci_hw_id=%01X, cci_band_id=%01X\n", cci_hw_id, cci_band_id);
	}

#ifdef CONFIG_CCI_KLOG_RECORD_KERNEL_TIMESTAMP
	klog_record_kernel_timestamp();
#endif // #ifdef CONFIG_CCI_KLOG_RECORD_KERNEL_TIMESTAMP
}
EXPORT_SYMBOL(cklc_save_magic);

void cklc_set_memory_ready(void)
{
	mem_ready = 1;
}
EXPORT_SYMBOL(cklc_set_memory_ready);

/* ------------------------------------------------------------
 * function   : clear_klog()
 * description: ClearKlog , Reinitialize and collect log again.
 * ------------------------------------------------------------ */
void clear_klog(void)
{
	int org_mem_ready;
	int org_mem_have_clean;

	int i;

	printk(KERN_NOTICE "klog: Initialize Request.\n");

//Backup KLOG status
	org_mem_ready = mem_ready;
	org_mem_have_clean = 1;

	mem_ready = 0;//temporary change to uninitialized

	cklc_save_magic(KLOG_MAGIC_FORCE_CLEAR, KLOG_STATE_INIT);

//Clear Each KLOG Buffer .(but not initialize name area)
	for(i = 0; i < KLOG_MAX_NUM; i++)
	{
		memset((pcklog + i)->buffer , ' ' , CONFIG_CCI_KLOG_CATEGORY_SIZE);
		(pcklog + i)->w_idx = 0;
		(pcklog + i)->overload = 0;
	}

//Restore to KLOG status
	mem_have_clean = org_mem_have_clean;
	mem_ready = org_mem_ready;

	printk(KERN_NOTICE "klog: Initialize Done !\n");

	return;
}

int klog_ioctl(struct inode *inode, struct file *filp,
		                unsigned int cmd, unsigned long arg)
{
	struct system_information sysinfo;
	int flag;

	switch(cmd)
	{
		case KLOG_IOCTL_GET_HEADER:
			memcpy(&sysinfo, klog_magic, sizeof(struct system_information));
			if(copy_to_user((void *)arg, &sysinfo, sizeof(struct system_information)))
			{
				return -EFAULT;
			}
			break;

		case KLOG_IOCTL_GET_KERNEL:
			if(copy_to_user((void *)arg, pcklog + KLOG_KERNEL, sizeof(struct cklog_t)))
			{
				return -EFAULT;
			}
			break;

		case KLOG_IOCTL_GET_ANDROID_MAIN:
			if(copy_to_user((void *)arg, pcklog + KLOG_ANDROID_MAIN, sizeof(struct cklog_t)))
			{
				return -EFAULT;
			}
			break;

		case KLOG_IOCTL_GET_ANDROID_SYSTEM:
			if(copy_to_user((void *)arg, pcklog + KLOG_ANDROID_SYSTEM, sizeof(struct cklog_t)))
			{
				return -EFAULT;
			}
			break;

		case KLOG_IOCTL_GET_ANDROID_RADIO:
			if(copy_to_user((void *)arg, pcklog + KLOG_ANDROID_RADIO, sizeof(struct cklog_t)))
			{
				return -EFAULT;
			}
			break;

		case KLOG_IOCTL_GET_ANDROID_EVENTS:
/*
			if(copy_to_user((void *)arg, pcklog + KLOG_ANDROID_EVENTS, sizeof(struct cklog_t)))
			{
				return -EFAULT;
			}
*/
			break;

		case KLOG_IOCTL_GET_RESERVE:
/*
			if(copy_to_user((void *)arg, pcklog + KLOG_RESERVE, sizeof(struct cklog_t)))
			{
				return -EFAULT;
			}
*/
			break;

		case KLOG_IOCTL_CHECK_OLD_LOG:
			flag = check_old_log();
			if(copy_to_user((void *)arg, &flag ,sizeof(int)))
			{
				return -EFAULT;
			}
			break;

		case KLOG_IOCTL_CLEAR_LOG:
			clear_klog();
			break;

		case KLOG_IOCTL_RECORD_SYSINFO:
			if(copy_from_user(&sysinfo, (void __user *)arg, sizeof(struct system_information)))
			{
				return -EFAULT;
			}
//recover hw_id
			snprintf(sysinfo.hw_id, KLOG_HW_ID_LENGTH, "%01X%01X", cci_hw_id, cci_band_id);
//sysinfo
			if(crashing == 0 && magic_priority != KLOG_PRIORITY_MARM_FATAL && magic_priority != KLOG_PRIORITY_AARM_PANIC)//only allow to overwrite if not panic
			{
				printk("[klog]sysinfo:klog_count=%s, normal_boot=%s, amss_version=%s, android_version=%s, flex_version=%s, flex_checksum=%s, build_date=%s, build_type=%s, build_user=%s, build_host=%s, build_key=%s, secure_mode=%s, cpuinfo_max_freq=%s, scaling_max_freq=%s\n", sysinfo.klog_userdata_count, sysinfo.normal_boot, sysinfo.amss_version, sysinfo.android_version, sysinfo.flex_version, sysinfo.flex_checksum, sysinfo.build_date, sysinfo.build_type, sysinfo.build_user, sysinfo.build_host, sysinfo.build_key, sysinfo.secure_mode, sysinfo.cpuinfo_max_freq, sysinfo.scaling_max_freq);
				memcpy(klog_magic + KLOG_MAGIC_TOTAL_LENGTH + KLOG_KERNEL_TIME_LENGTH + KLOG_FIRST_RTC_TIMESTAMP_LENGTH + KLOG_LAST_RTC_TIMESTAMP_LENGTH, &sysinfo.klog_userdata_count, sizeof(struct system_information) - KLOG_MAGIC_TOTAL_LENGTH + KLOG_KERNEL_TIME_LENGTH + KLOG_FIRST_RTC_TIMESTAMP_LENGTH + KLOG_LAST_RTC_TIMESTAMP_LENGTH);
			}
			else
			{
				printk("[klog]sysinfo:not allow to record sysinfo\n");
			}
			break;

#ifdef CONFIG_CCI_KLOG_ALLOW_FORCE_PANIC
		case KLOG_IOCTL_FORCE_PANIC:
			panic("klog panic\n");
			break;
#endif // #ifdef CONFIG_CCI_KLOG_ALLOW_FORCE_PANIC

		default:
			return -1;
	}

	return 0;
}

static const struct file_operations klog_fops = {
	.owner		= THIS_MODULE,
	.ioctl		= klog_ioctl,
};

static struct miscdevice klog_miscdev = {
	.minor	= KLOG_MINOR,
	.name	= "cklc",
	.fops	= &klog_fops,
};

int klogmisc_init(void)
{
	int retval = 0;
	retval = misc_register(&klog_miscdev);
	if(retval)
	{
		printk(KERN_ERR "klog: cannot register miscdev on minor=%d (err=%d)\n", KLOG_MINOR, retval);
		goto outmisc;
	}

	return retval;

outmisc:
	misc_deregister(&klog_miscdev);
	return retval;
}

void klogmisc_exit(void)
{
	misc_deregister(&klog_miscdev);
}

static int __init cklc_init(void)
{
	int retval = 0;

	printk(KERN_INFO "CCI KLog Collector Init\n");

	//memset(&cklog, 0, sizeof(struct cklog_t) * KLOG_MAX_NUM);
	cklc_save_magic(KLOG_MAGIC_INIT, KLOG_STATE_INIT);

	snprintf((pcklog + KLOG_KERNEL)->name, NAME_SIZE, "kernel");
	snprintf((pcklog + KLOG_ANDROID_MAIN)->name, NAME_SIZE, "android_main");
	snprintf((pcklog + KLOG_ANDROID_SYSTEM)->name, NAME_SIZE, "android_system");
	snprintf((pcklog + KLOG_ANDROID_RADIO)->name, NAME_SIZE, "android_radio");
       /* snprintf(cklog[KLOG_ANDROID_EVENTS].name, NAME_SIZE, "android_events");*/

	retval = klogmisc_init();

	return retval;
}

static void __exit cklc_exit(void)
{
	printk(KERN_INFO "CCI KLog Collector Exit\n");
	klogmisc_exit();

	return;
}

/*
 * ------------------------------------------------------------------------------------
 * KLOG Collector : uninitialzed ram mapping
 *
 * Virtual Address      : size      : SYMBOL                    : note
 * ------------------------------------------------------------------------------------
 * 0xE1100000-0xE1100009:(0x00000A) : MSM_KLOG_MAGIC            : MAGIC:"KLGMGC###\0", please reference klog_magic_list
 * 0xE110000A-0xE110001D:(0x000014) : kernel time               : [%08lX.%08lX]\0
 * 0xE110001E-0xE1100031:(0x000014) : first RTC timestamp       : [%08lX.%08lX]\0
 * 0xE1100032-0xE1100045:(0x000014) : last RTC timestamp        : [%08lX.%08lX]\0
 * 0xE1100046-0xE11003FF:(0x0003CE) : reserved                  :
 * ------------------------------------------------------------------------------------
 * 0xE1100400-0xE1200027:(0x0FFC28) : KLOG_KERNEL               :
 *                                  : (MSM_KLOG_MAIN)           : buffer  1023 * 1024
 *                                  :                           : name             32
 *                                  :                           : w_idx             4
 *                                  :                           : overload          4
 * ------------------------------------------------------------------------------------
 * 0xE1200028-0xE12FFC4F:(0x0FFC28) : KLOG_ANDROID_MAIN         :
 *                                  :                           : buffer  1023 * 1024
 *                                  :                           : name             32
 *                                  :                           : w_idx             4
 *                                  :                           : overload          4
 * ------------------------------------------------------------------------------------
 * 0xE12FFC50-0xE13FF877:(0x0FFC28) : KLOG_ANDROID_SYSTEM       :
 *                                  :                           : buffer  1023 * 1024
 *                                  :                           : name             32
 *                                  :                           : w_idx             4
 *                                  :                           : overload          4
 * ------------------------------------------------------------------------------------
 * 0xE13FF878-0xE14FF49F:(0x0FFC28) : KLOG_ANDROID_RADIO        :
 *                                  :                           : buffer  1023 * 1024
 *                                  :                           : name             32
 *                                  :                           : w_idx             4
 *                                  :                           : overload          4
 * ------------------------------------------------------------------------------------
 * 0xE14FF4A0-0xE14FFFFF:(0x000B60) : reserved                  :
 * ------------------------------------------------------------------------------------
 */

module_init(cklc_init);
module_exit(cklc_exit);

