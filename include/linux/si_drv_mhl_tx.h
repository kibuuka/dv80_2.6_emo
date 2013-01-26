/***********************************************************************************/
/*  Copyright (c) 2010-2011, Silicon Image, Inc.  All rights reserved.             */
/*  No part of this work may be reproduced, modified, distributed, transmitted,    */
/*  transcribed, or translated into any language or computer format, in any form   */
/*  or by any means without written permission of: Silicon Image, Inc.,            */
/*  1140 East Arques Avenue, Sunnyvale, California 94085                           */
/***********************************************************************************/
/*
	@file: si_drv_mhl_tx.h
 */


// DEVCAP we will initialize to
#define	MHL_LOGICAL_DEVICE_MAP		(MHL_DEV_LD_GUI)//(MHL_DEV_LD_AUDIO | MHL_DEV_LD_VIDEO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_GUI )

//=================================================================================================

#define SIL9244_DRV_NAME "sil9244"
#define SIL_RBUFF_SIZE		8	/* Rx buffer size */

struct hdmi_sil9244_platform_data {
	int intr;
	int (*chip_init)(void);
         // BugID 625, Luke 2011.8.10, MHL chip enter sleep mode
	int (*mhl_reset)(void);
	int (*mhl_select)(int);
         // BugID 625, Luke 2011.8.10, MHL chip enter sleep mode
	int (*pwr_on)(void);
	int (*pwr_off)(void);
	int irq_sii_int;
	int irq_usb_id;
};


