
#define NAME_SIZE				32

#define KLOG_IOCTL_ID				0xD1
#define KLOG_IOCTL_GET_HEADER			_IO(KLOG_IOCTL_ID, 1)
#define KLOG_IOCTL_GET_KERNEL			_IO(KLOG_IOCTL_ID, 2)
#define KLOG_IOCTL_GET_ANDROID_MAIN		_IO(KLOG_IOCTL_ID, 3)
#define KLOG_IOCTL_GET_ANDROID_SYSTEM		_IO(KLOG_IOCTL_ID, 4)
#define KLOG_IOCTL_GET_ANDROID_RADIO		_IO(KLOG_IOCTL_ID, 5)
#define KLOG_IOCTL_GET_ANDROID_EVENTS		_IO(KLOG_IOCTL_ID, 6)
#define KLOG_IOCTL_GET_RESERVE			_IO(KLOG_IOCTL_ID, 7)
#define KLOG_IOCTL_CHECK_OLD_LOG		_IO(KLOG_IOCTL_ID, 8)
#define KLOG_IOCTL_CLEAR_LOG			_IO(KLOG_IOCTL_ID, 9)
#define KLOG_IOCTL_RECORD_SYSINFO		_IO(KLOG_IOCTL_ID, 10)
#define KLOG_IOCTL_FORCE_PANIC			_IO(KLOG_IOCTL_ID, 11)

#define DEV_PATH				"/dev/cklc"
#define VALID_FILENAME_PATTERN			"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"
#define KLOG_PATH_USERDATA_PARTITION		"/data"
#define KLOG_PATH_USERDATA			"/data/klog"
#define KLOG_PATH_INTERNAL			"/mnt/sdcard"
#define KLOG_PATH_EXTERNAL			"/mnt/sdcard/sdcard2"
#define KLOG_FILENAME				"klog-"
#define KLOG_FILES_MAX				100
#define KLOG_PROPERTY_MAX_NAME			"persist.klog.max"
#define KLOG_PROPERTY_MAX_VALUE			"10"
#define KLOG_PROPERTY_COUNT_NAME		"persist.klog.count"
#define KLOG_PROPERTY_COUNT_VALUE		"-1"

//klog info(84)
#define KLOG_MAGIC_TOTAL_LENGTH			10		//magic*6+state*3+'\0'
#define KLOG_MAGIC_LENGTH			6
#define KLOG_STATE_LENGTH			3
#define KLOG_KERNEL_TIME_LENGTH			20
#define KLOG_FIRST_RTC_TIMESTAMP_LENGTH		20
#define KLOG_LAST_RTC_TIMESTAMP_LENGTH		20
#define KLOG_USERDATA_COUNT_LENGTH		4
#define KLOG_ISD_COUNT_LENGTH			4
#define KLOG_ESD_COUNT_LENGTH			4
#define KLOG_NORMAL_BOOT_LENGTH			2
#define KLOG_INFO_LENGTH			(KLOG_MAGIC_TOTAL_LENGTH + KLOG_KERNEL_TIME_LENGTH + KLOG_FIRST_RTC_TIMESTAMP_LENGTH + KLOG_LAST_RTC_TIMESTAMP_LENGTH + KLOG_USERDATA_COUNT_LENGTH + KLOG_ISD_COUNT_LENGTH + KLOG_ESD_COUNT_LENGTH + KLOG_NORMAL_BOOT_LENGTH)
//image info(152)
#define KLOG_AMSS_VERSION_LENGTH		15
#define KLOG_ANDROID_VERSION_LENGTH		15
#define KLOG_FLEX_VERSION_LENGTH		25
#define KLOG_FLEX_CHECKSUM_LENGTH		11
#define KLOG_BUILD_DATE_LENGTH			31
#define KLOG_BUILD_TYPE_LENGTH			10
#define KLOG_BUILD_USER_LENGTH			10
#define KLOG_BUILD_HOST_LENGTH			20
#define KLOG_BUILD_KEY_LENGTH			13
#define KLOG_SECURE_MODE_LENGTH			2
#define KLOG_IMAGE_INFO_LENGTH			(KLOG_AMSS_VERSION_LENGTH + KLOG_ANDROID_VERSION_LENGTH + KLOG_FLEX_VERSION_LENGTH + KLOG_FLEX_CHECKSUM_LENGTH + KLOG_BUILD_DATE_LENGTH + KLOG_BUILD_TYPE_LENGTH + KLOG_BUILD_USER_LENGTH + KLOG_BUILD_HOST_LENGTH + KLOG_BUILD_KEY_LENGTH + KLOG_SECURE_MODE_LENGTH)
//HW info(19)
#define KLOG_HW_ID_LENGTH			3
#define KLOG_CPUINFO_MAX_FREQ_LENGTH		8
#define KLOG_SCALING_MAX_FREQ_LENGTH		8
#define KLOG_HW_INFO				(KLOG_HW_ID_LENGTH + KLOG_CPUINFO_MAX_FREQ_LENGTH + KLOG_SCALING_MAX_FREQ_LENGTH)

//do not over than 6 characters
#define KLOG_MAGIC_NONE				""		//not to update magic
#define KLOG_MAGIC_INIT				"CKLOGC"	//default magic
#define KLOG_MAGIC_FORCE_CLEAR			"FRCCLR"	//force clear
#define KLOG_MAGIC_MARM_FATAL			"FATALM"	//mARM fatal error
#define KLOG_MAGIC_AARM_PANIC			"PANICA"	//aARM kernel panic
#define KLOG_MAGIC_DOWNLOAD_MODE		"DLMODE"	//normal download mode
#define KLOG_MAGIC_POWER_OFF			"PWROFF"	//power off
#define KLOG_MAGIC_REBOOT			"REBOOT"	//normal reboot
#define KLOG_MAGIC_BOOTLOADER			"BOTLDR"	//boot into bootloader(fastboot mode)
#define KLOG_MAGIC_RECOVERY			"RCOVRY"	//boot into recovery mode
#define KLOG_MAGIC_OEM_COMMAND			"OEM-"		//boot into oem customized mode, 2-digit hex attached to the tail
#define KLOG_MAGIC_APPSBL			"APPSBL"	//AppSBL magic to indicate

#define KLOG_PRIORITY_INVALID			-1		//any value which not defined in klog_magic_list
#define KLOG_PRIORITY_HIGHEST			0
#define KLOG_PRIORITY_LOWEST			255

#define KLOG_PRIORITY_FORCE_CLEAR		KLOG_PRIORITY_HIGHEST
#define KLOG_PRIORITY_MARM_FATAL		1
#define KLOG_PRIORITY_AARM_PANIC		2
#define KLOG_PRIORITY_DOWNLOAD_MODE		3
#define KLOG_PRIORITY_POWER_OFF			4
#define KLOG_PRIORITY_NATIVE_COMMAND		5
#define KLOG_PRIORITY_OEM_COMMAND		6
#define KLOG_PRIORITY_KLOG_INIT			KLOG_PRIORITY_LOWEST

//do not over than 3 digits(hex)
#define KLOG_STATE_INIT_CODE			"###"
#define KLOG_STATE_INIT				-1
#define KLOG_STATE_NONE				0x000
#define KLOG_STATE_MARM_FATAL			0x001
#define KLOG_STATE_AARM_PANIC			0x002
#define KLOG_STATE_DOWNLOAD_MODE		0x004

enum cklc_category
{
	KLOG_KERNEL = 0,
	KLOG_ANDROID_MAIN,
	KLOG_ANDROID_SYSTEM,
	KLOG_ANDROID_RADIO,
	/* KLOG_ANDROID_EVENTS,*/
	/* Do not touch */
	KLOG_MAX_NUM
};

struct cklog_t
{
	unsigned char 	buffer[CONFIG_CCI_KLOG_CATEGORY_SIZE];	/* ring buffer */
	unsigned char	name[NAME_SIZE];			/* log name */
	size_t		w_idx;					/* write index/offset of current log */
	unsigned int	overload;				/* 0:indicate the log read head is 0, 1:indicate the log read head in not 0(should be w_off+1) */
};

struct klog_magic_list
{
	unsigned char	name[KLOG_MAGIC_LENGTH + 1];
	int		priority;
};

struct system_information
{
	char magic[KLOG_MAGIC_TOTAL_LENGTH];
	char kernel_time[KLOG_KERNEL_TIME_LENGTH];
	char first_rtc[KLOG_FIRST_RTC_TIMESTAMP_LENGTH];
	char last_rtc[KLOG_LAST_RTC_TIMESTAMP_LENGTH];
	char klog_userdata_count[KLOG_USERDATA_COUNT_LENGTH];
	char klog_isd_count[KLOG_ISD_COUNT_LENGTH];
	char klog_esd_count[KLOG_ESD_COUNT_LENGTH];
	char normal_boot[KLOG_NORMAL_BOOT_LENGTH];
	char amss_version[KLOG_AMSS_VERSION_LENGTH];
	char android_version[KLOG_ANDROID_VERSION_LENGTH];
	char flex_version[KLOG_FLEX_VERSION_LENGTH];
	char flex_checksum[KLOG_FLEX_CHECKSUM_LENGTH];
	char build_date[KLOG_BUILD_DATE_LENGTH];
	char build_type[KLOG_BUILD_TYPE_LENGTH];
	char build_user[KLOG_BUILD_USER_LENGTH];
	char build_host[KLOG_BUILD_HOST_LENGTH];
	char build_key[KLOG_BUILD_KEY_LENGTH];
	char secure_mode[KLOG_SECURE_MODE_LENGTH];
	char hw_id[KLOG_HW_ID_LENGTH];
	char cpuinfo_max_freq[KLOG_CPUINFO_MAX_FREQ_LENGTH];
	char scaling_max_freq[KLOG_SCALING_MAX_FREQ_LENGTH];
};

#ifdef CONFIG_CCI_KLOG_COLLECTOR

void cklc_append_kernel_raw_char(unsigned char c);
void cklc_append_char(unsigned int category, unsigned char c);
void cklc_append_str(unsigned int category, unsigned char *str, size_t len);
void cklc_append_newline(unsigned int category);
void cklc_append_separator(unsigned int category);
void cklc_append_time_header(unsigned int category);
void show_android_log_to_console(void);
void cklc_append_android_log(unsigned int category, const unsigned char *priority, const char * const tag, const int tag_bytes, const char * const msg, const int msg_bytes);
void cklc_save_magic(char *magic, int state);
void cklc_set_memory_ready(void);
int check_old_log(void);
int get_magic_priority(char *magic);
int atoi(char *str);

#else

#error
#define cklc_append_kernel_raw_char(c)		do {} while (0)
#define cklc_append_char(category, c)		do {} while (0)
#define cklc_append_str(category, str, len)	do {} while (0)
#define cklc_append_newline(category)		do {} while (0)
#define cklc_append_separator(category)		do {} while (0)
#define cklc_append_time_header(category)	do {} while (0)
#define cklc_save_magic				do {} while (0)
#define cklc_set_memory_ready			do {} while (0)

#endif

