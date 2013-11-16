#huapu_cheng: start / 20110208 / sensors porting for gingerbread
#obj-$(CONFIG_CCI_ACCELERATION_SENSOR)	+=	acceleration_sensor/
#obj-$(CONFIG_CCI_GEOMAGNETIC_SENSOR)	+=	geomagnetic_sensor/
#obj-$(CONFIG_CCI_ORIENTATION_SENSOR)	+=	orientation_sensor/
obj-$(CONFIG_CCI_LIGHT_SENSOR)	+=	light_sensor/
obj-$(CONFIG_CCI_HALL_SENSOR)	+=	hall_sensor/
#huapu_cheng: end
#[DA80] ===> BugID#789 : CCI KLog Collector, added by Jimmy@CCI
obj-$(CONFIG_CCI_KLOG_COLLECTOR)	+=	klog_collector/
#[DA80] <=== BugID#789 : CCI KLog Collector, added by Jimmy@CCI
obj-m += wlan/broadcom/src/

#leon_yang: S
obj-$(CONFIG_CCI_KERNEL_INFO)	+=	cci_kernel_info/
#leon_yang: E