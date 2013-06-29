#Android makefile to build kernel as a part of Android Build

ifeq ($(TARGET_PREBUILT_KERNEL),)

KERNEL_OUT := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ
#KERNEL_CONFIG := $(KERNEL_OUT)/.config
KERNEL_CONFIG := da80_defconfig
TARGET_PREBUILT_INT_KERNEL := $(KERNEL_OUT)/arch/arm/boot/zImage
KERNEL_HEADERS_INSTALL := $(KERNEL_OUT)/usr
KERNEL_MODULES_INSTALL := system
KERNEL_MODULES_OUT := $(TARGET_OUT)/lib/modules

#[DA80] ===> BugID#789 : CCI KLog Collector, added by Jimmy@CCI
ifeq ($(CCI_TARGET_KLOG_COLLECTOR),true)
  CCI_KLOG_COLLECTOR := 1
  CCI_KLOG_START_ADDR_PHYSICAL := $(CCI_TARGET_KLOG_START_ADDR_PHYSICAL)
  CCI_KLOG_START_ADDR_VIRTUAL := $(CCI_TARGET_KLOG_START_ADDR_VIRTUAL)
  CCI_KLOG_SIZE := $(CCI_TARGET_KLOG_SIZE)
  CCI_KLOG_HEADER_SIZE := $(CCI_TARGET_KLOG_HEADER_SIZE)
  CCI_KLOG_CATEGORY_SIZE := $(CCI_TARGET_KLOG_CATEGORY_SIZE)
ifeq ($(TARGET_BUILD_VARIANT),eng)
  CCI_KLOG_SUPPORT_STORAGE := 1
  CCI_KLOG_ALLOW_FORCE_PANIC := 1
endif # ifeq ($(TARGET_BUILD_VARIANT),eng)
endif # ifeq ($(CCI_TARGET_KLOG_COLLECTOR),true)
#[DA80] <=== BugID#789 : CCI KLog Collector, added by Jimmy@CCI

ifeq ($(TARGET_USES_UNCOMPRESSED_KERNEL),true)
$(info Using uncompressed kernel)
TARGET_PREBUILT_KERNEL := $(KERNEL_OUT)/piggy
else
TARGET_PREBUILT_KERNEL := $(TARGET_PREBUILT_INT_KERNEL)
endif

define mv-modules
mdpath=`find $(KERNEL_MODULES_OUT) -type f -name modules.dep`;\
if [ "$$mdpath" != "" ];then\
mpath=`dirname $$mdpath`;\
ko=`find $$mpath/kernel -type f -name *.ko`;\
for i in $$ko; do mv $$i $(KERNEL_MODULES_OUT)/; done;\
fi
endef

define clean-module-folder
mdpath=`find $(KERNEL_MODULES_OUT) -type f -name modules.dep`;\
if [ "$$mdpath" != "" ];then\
mpath=`dirname $$mdpath`; rm -rf $$mpath;\
fi
endef

$(KERNEL_OUT):
	mkdir -p $(KERNEL_OUT)

$(KERNEL_CONFIG): $(KERNEL_OUT)
	$(MAKE) -C kernel O=../$(KERNEL_OUT) ARCH=arm CROSS_COMPILE=arm-eabi- $(KERNEL_DEFCONFIG)

$(KERNEL_OUT)/piggy : $(TARGET_PREBUILT_INT_KERNEL)
	$(hide) gunzip -c $(KERNEL_OUT)/arch/arm/boot/compressed/piggy.gzip > $(KERNEL_OUT)/piggy

$(TARGET_PREBUILT_INT_KERNEL): CONFIG_SECURE $(KERNEL_OUT) $(KERNEL_CONFIG) $(KERNEL_HEADERS_INSTALL)
#[DA80] ===> BugID#789 : CCI KLog Collector, added by Jimmy@CCI
ifeq ($(CCI_KLOG_COLLECTOR),1)
	$(MAKE) -C kernel O=../$(KERNEL_OUT) ARCH=arm CROSS_COMPILE=arm-eabi- CCI_KLOG_COLLECTOR=$(CCI_KLOG_COLLECTOR) CCI_KLOG_START_ADDR_PHYSICAL=$(CCI_KLOG_START_ADDR_PHYSICAL) CCI_KLOG_START_ADDR_VIRTUAL=$(CCI_KLOG_START_ADDR_VIRTUAL) CCI_KLOG_SIZE=$(CCI_KLOG_SIZE) CCI_KLOG_HEADER_SIZE=$(CCI_KLOG_HEADER_SIZE) CCI_KLOG_CATEGORY_SIZE=$(CCI_KLOG_CATEGORY_SIZE) CCI_KLOG_SUPPORT_STORAGE=$(CCI_KLOG_SUPPORT_STORAGE) CCI_KLOG_ALLOW_FORCE_PANIC=$(CCI_KLOG_ALLOW_FORCE_PANIC)
else # ifeq ($(CCI_KLOG_COLLECTOR),1)
	$(MAKE) -C kernel O=../$(KERNEL_OUT) ARCH=arm CROSS_COMPILE=arm-eabi-
endif # ifeq ($(CCI_KLOG_COLLECTOR),1)
#[DA80] <=== BugID#789 : CCI KLog Collector, added by Jimmy@CCI
	$(MAKE) -C kernel O=../$(KERNEL_OUT) ARCH=arm CROSS_COMPILE=arm-eabi- modules
	mkdir -p $(KERNEL_OUT)/../../system/etc/
	cp $(KERNEL_OUT)/../system/wlan/broadcom/src/dhd.ko $(KERNEL_OUT)/../../system/etc/dhd.ko
	$(MAKE) -C kernel O=../$(KERNEL_OUT) INSTALL_MOD_PATH=../../$(KERNEL_MODULES_INSTALL) ARCH=arm CROSS_COMPILE=arm-eabi- modules_install
	$(mv-modules)
	$(clean-module-folder)

$(KERNEL_HEADERS_INSTALL): $(KERNEL_OUT) $(KERNEL_CONFIG)
	$(MAKE) -C kernel O=../$(KERNEL_OUT) ARCH=arm CROSS_COMPILE=arm-eabi- headers_install

kerneltags: $(KERNEL_OUT) $(KERNEL_CONFIG)
	$(MAKE) -C kernel O=../$(KERNEL_OUT) ARCH=arm CROSS_COMPILE=arm-eabi- tags

kernelconfig: $(KERNEL_OUT) $(KERNEL_CONFIG)
	env KCONFIG_NOTIMESTAMP=true \
	     $(MAKE) -C kernel O=../$(KERNEL_OUT) ARCH=arm CROSS_COMPILE=arm-eabi- menuconfig
	cp $(KERNEL_OUT)/.config kernel/arch/arm/configs/$(KERNEL_DEFCONFIG)

endif

#Add CCI_SECURE_MODE config to disable some config in user mode--Taylor--20110907--Begin--Merge BSP3134
KERNEL_DEFCONFIG_FILE := `pwd`/kernel/arch/arm/configs/$(KERNEL_DEFCONFIG)

CONFIG_SECURE:
ifeq ($(CCI_SECURE_MODE), true)
	@echo "$(KERNEL_DEFCONFIG): set CONFIG_CCI_SECURE_MODE"
	@sed -i 's/CONFIG_MSM_SMD_DEBUG=y/# CONFIG_MSM_SMD_DEBUG is not set/g' $(KERNEL_DEFCONFIG_FILE)
	@sed -i 's/CONFIG_MSM_ONCRPCROUTER_DEBUG=y/# CONFIG_MSM_ONCRPCROUTER_DEBUG is not set/g' $(KERNEL_DEFCONFIG_FILE)
#	@sed -i 's/CONFIG_MSM_GSBI9_UART=y/# CONFIG_MSM_GSBI9_UART is not set/g' $(KERNEL_DEFCONFIG_FILE)
	@sed -i 's/CONFIG_SERIAL_MSM_HSL_CONSOLE=y/# CONFIG_SERIAL_MSM_HSL_CONSOLE is not set/g' $(KERNEL_DEFCONFIG_FILE)
else
	@echo "$(KERNEL_DEFCONFIG): unset CONFIG_CCI_SECURE_MODE"
	@sed -i 's/# CONFIG_MSM_ONCRPCROUTER_DEBUG is not set/CONFIG_MSM_ONCRPCROUTER_DEBUG=y/g' $(KERNEL_DEFCONFIG_FILE)
	@sed -i 's/# CONFIG_MSM_SMD_DEBUG is not set/CONFIG_MSM_SMD_DEBUG=y/g' $(KERNEL_DEFCONFIG_FILE)
#	@sed -i 's/# CONFIG_MSM_GSBI9_UART is not set/CONFIG_MSM_GSBI9_UART=y/g' $(KERNEL_DEFCONFIG_FILE)
	@sed -i 's/# CONFIG_SERIAL_MSM_HSL_CONSOLE is not set/CONFIG_SERIAL_MSM_HSL_CONSOLE=y/g' $(KERNEL_DEFCONFIG_FILE)
endif
#Add CCI_SECURE_MODE config to disable some config in user mode--Taylor--20110907--End--Merge BSP3134
