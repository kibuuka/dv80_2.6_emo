#SRCBASE := $(shell pwd)/../../../../../../system/wlan/broadcom/src
SRCBASE := $(shell pwd)/../wlan/broadcom/src
DNGL_IMAGE_NAME ?= 4325b0/sdio-g-cdc-reclaim-wme-pktfilter-keepalive-aoe
WFLAGS := -Wall -Wstrict-prototypes

DFLAGS += -DLINUX -DSRCBASE=\"$(SRCBASE)\" -DBCMDRIVER -DBCMDONGLEHOST -DDHDTHREAD -DBCMWPA2
DFLAGS += -DUNRELEASEDCHIP
#DFLAGS += -DBCMEMBEDIMAGE -DIMAGE_NAME="$(DNGL_IMAGE_NAME)"
DFLAGS += -DCONFIG_WIRELESS_EXT

IFLAGS += -I$(LINUXDIR)/include -I$(LINUXDIR)/include/asm/mach-default -I. -I$(SRCBASE)/include -I$(SRCBASE)/shared 
IFLAGS += -I$(SRCBASE)/dhd/sys -I$(SRCBASE)/dongle -I$(SRCBASE)/wl/sys
IFLAGS += -I$(SRCBASE)/dongle/rte/wl/builds/$(DNGL_IMAGE_NAME)

# -gpl-
DFLAGS += -DDHD_GPL -DDHD_SCHED

# -cdc-
DFLAGS += -DBDC -DTOE 
DFLAGS += -DDHD_BCMEVENTS -DSHOW_EVENTS

# -sdmmc-
DFLAGS += -DBCMSDIO -DDHD_GPL -DBCMLXSDMMC -DBCMPLATFORM_BUS
DFLAGS += -g

# -OOBONLY-
DFLAGS += -DOOB_INTR_ONLY

# -ISR_THREAD-
#DFLAGS += -DSDIO_ISR_THREAD

# -PM-
DFLAGS += -DCONFIG_HAS_EARLYSUSPEND 

# -Data-
DFLAGS += -DDHD_SDALIGN=64 -DMAX_HDR_READ=64 -DDHD_FIRSTREAD=64

# -HW-
DFLAGS += -DCUSTOMER_HW2 -DEMBEDDED_PLATFORM -DMMC_SDIO_ABORT
DFLAGS += -DSOFTAP

DFLAGS += -DPKT_FILTER_SUPPORT -DKEEP_ALIVE -DARP_OFFLOAD_SUPPORT

EXTRA_CFLAGS += -fshort-wchar $(DFLAGS) $(WFLAGS) $(IFLAGS)

#obj:
#	$(OBJCOPY) --strip-unneeded dhd.ko dhd.ko.stripped

obj-m += dhd.o
dhd-objs := dhd/sys/dhd_linux.o shared/linux_osl.o shared/bcmutils.o dhd/sys/dhd_common.o dhd/sys/dhd_custom_gpio.o
dhd-objs += shared/siutils.o shared/sbutils.o shared/aiutils.o shared/hndpmu.o
dhd-objs += dhd/sys/dhd_linux_sched.o dhd/sys/dhd_cdc.o
dhd-objs += dhd/sys/dhd_sdio.o bcmsdio/sys/bcmsdh_sdmmc.o bcmsdio/sys/bcmsdh.o bcmsdio/sys/bcmsdh_linux.o
dhd-objs += bcmsdio/sys/bcmsdh_sdmmc_linux.o
dhd-objs += wl/sys/wl_iw.o shared/bcmwifi.o
