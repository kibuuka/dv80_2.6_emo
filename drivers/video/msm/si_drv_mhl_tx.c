/***********************************************************************************/
/*  Copyright (c) 2002-2010, Silicon Image, Inc.  All rights reserved.             */
/*  No part of this work may be reproduced, modified, distributed, transmitted,    */
/*  transcribed, or translated into any language or computer format, in any form   */
/*  or by any means without written permission of: Silicon Image, Inc.,            */
/*  1060 East Arques Avenue, Sunnyvale, California 94085                           */
/***********************************************************************************/

// Standard C Library
//#include <stdio.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/time.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/earlysuspend.h>
#include <linux/input.h>
#include <linux/mfd/pmic8058.h>

// Standard C Library
///#include "si_memsegsupport.h"
///#include "si_common.h"
#include "si_datatypes.h"
#include "si_mhl_defs.h"
#include "defs.h"
#include "sii_reg_access.h"
#include <linux/si_drv_mhl_tx.h>
#include "si_mhl_tx.h"
#include "si_mhl_tx_api.h"
#include "si_mhl_tx_base_drv_api.h"  // generic driver interface to MHL tx component
#include "si_9244_regs.h"
#include "sii_hal_timers.h"
///#include "si_osscheduler.h"
#define SILICON_IMAGE_ADOPTER_ID 322
#define TRANSCODER_DEVICE_ID 0x9244

#define  pin9290_938x   1
int settingMode9290 = 0;//pin9290_938x;

#define pinMode1x3x  1
int settingMode1X = pinMode1x3x;

int pinAllowD3 = 1;

bool mhl_cable_connected;
bool sil_suspend_flag = false;
	
//
// Software power states are a little bit different than the hardware states but
// a close resemblance exists.
//
// D3 matches well with hardware state. In this state we receive RGND interrupts
// to initiate wake up pulse and device discovery
//
// Chip wakes up in D2 mode and interrupts MCU for RGND. Firmware changes the 9244
// into D0 mode and sets its own operation mode as POWER_STATE_D0_NO_MHL because
// MHL connection has not yet completed.
//
// For all practical reasons, firmware knows only two states of hardware - D0 and D3.
//
// We move from POWER_STATE_D0_NO_MHL to POWER_STATE_D0_MHL only when MHL connection
// is established.
/*
//
//                             S T A T E     T R A N S I T I O N S
//
//
//                    POWER_STATE_D3                      POWER_STATE_D0_NO_MHL
//                   /--------------\                        /------------\
//                  /                \                      /     D0       \
//                 /                  \                \   /                \
//                /   DDDDDD  333333   \     RGND       \ /   NN  N  OOO     \
//                |   D     D     33   |-----------------|    N N N O   O     |
//                |   D     D  3333    |      IRQ       /|    N  NN  OOO      |
//                \   D     D      33  /               /  \                  /
//                 \  DDDDDD  333333  /                    \   CONNECTION   /
//                  \                /\                     /\             /
//                   \--------------/  \  TIMEOUT/         /  -------------
//                         /|\          \-------/---------/        ||
//                        / | \            500ms\                  ||
//                          |                     \                ||
//                          |  RSEN_LOW                            || MHL_EST
//                           \ (STATUS)                            ||  (IRQ)
//                            \                                    ||
//                             \      /------------\              //
//                              \    /              \            //
//                               \  /                \          //
//                                \/                  \ /      //
//                                 |    CONNECTED     |/======//    
//                                 |                  |\======/
//                                 \   (OPERATIONAL)  / \
//                                  \                /
//                                   \              /
//                                    \-----------/
//                                   POWER_STATE_D0_MHL
//
//
//
*/
#define	POWER_STATE_D3				3
#define	POWER_STATE_D0_NO_MHL		2
#define	POWER_STATE_D0_MHL			0
#define	POWER_STATE_FIRST_INIT		0xFF

#define DISCONNECT_BY_USB_EST                      0
#define DISCONNECT_BY_RENS_DEGLITCH          1
#define DISCONNECT_BY_DDC_ABORT                 2

//
// To remember the current power state.
//
static	uint8_t	fwPowerState = POWER_STATE_FIRST_INIT;

//
// This flag is set to true as soon as a INT1 RSEN CHANGE interrupt arrives and
// a deglitch timer is started.
//
// We will not get any further interrupt so the RSEN LOW status needs to be polled
// until this timer expires.
//
static	bool_t	deglitchingRsenNow = false;

//
// To serialize the RCP commands posted to the CBUS engine, this flag
// is maintained by the function SiiMhlTxDrvSendCbusCommand()
//
static	bool_t		mscCmdInProgress;	// false when it is okay to send a new command
//
// Preserve Downstream HPD status
//
static	uint8_t	dsHpdStatus = 0;
static uint8_t linkMode = 0;
static uint8_t contentOn=0;

#define	I2C_READ_MODIFY_WRITE(saddr,offset,mask)	I2C_WriteByte(saddr, offset, I2C_ReadByte(saddr, offset) | (mask));
#define ReadModifyWriteByteCBUS(offset,andMask,orMask)  WriteByteCBUS(offset,(ReadByteCBUS(offset)&andMask) | orMask)

#define	SET_BIT(saddr,offset,bitnumber)		I2C_READ_MODIFY_WRITE(saddr,offset, (1<<bitnumber))
#define	CLR_BIT(saddr,offset,bitnumber)		I2C_WriteByte(saddr, offset, I2C_ReadByte(saddr, offset) & ~(1<<bitnumber))
//
// 90[0] = Enable / Disable MHL Discovery on MHL link
//
#define	DISABLE_DISCOVERY				CLR_BIT(PAGE_0_0X72, 0x90, 0);
#define	ENABLE_DISCOVERY				SET_BIT(PAGE_0_0X72, 0x90, 0);

#define STROBE_POWER_ON                    CLR_BIT(PAGE_0_0X72, 0x90, 1);
//
//	Look for interrupts on INTR4 (Register 0x74)
//		7 = RSVD		(reserved)
//		6 = RGND Rdy	(interested)
//		5 = VBUS Low	(ignore)	
//		4 = CBUS LKOUT	(interested)
//		3 = USB EST		(interested)
//		2 = MHL EST		(interested)
//		1 = RPWR5V Change	(ignore)
//		0 = SCDT Change	(only if necessary)
//
#define	INTR_4_DESIRED_MASK				(BIT0 | BIT2 | BIT3 | BIT4 | BIT5 | BIT6)
#define	UNMASK_INTR_4_INTERRUPTS		I2C_WriteByte(PAGE_0_0X72, 0x78, INTR_4_DESIRED_MASK)
#define	MASK_INTR_4_INTERRUPTS			I2C_WriteByte(PAGE_0_0X72, 0x78, 0x00)

//	Look for interrupts on INTR_2 (Register 0x72)
//		7 = bcap done			(ignore)
//		6 = parity error		(ignore)
//		5 = ENC_EN changed		(ignore)
//		4 = no premable			(ignore)
//		3 = ACR CTS changed		(ignore)
//		2 = ACR Pkt Ovrwrt		(ignore)
//		1 = TCLK_STBL changed	(interested)
//		0 = Vsync				(ignore)
#define	INTR_2_DESIRED_MASK				(BIT1)
#define	UNMASK_INTR_2_INTERRUPTS		I2C_WriteByte(PAGE_0_0X72, 0x76, INTR_2_DESIRED_MASK)
#define	MASK_INTR_2_INTERRUPTS			I2C_WriteByte(PAGE_0_0X72, 0x76, 0x00)

//	Look for interrupts on INTR_1 (Register 0x71)
//		7 = RSVD		(reserved)
//		6 = MDI_HPD		(interested)
//		5 = RSEN CHANGED(interested)	
//		4 = RSVD		(reserved)
//		3 = RSVD		(reserved)
//		2 = RSVD		(reserved)
//		1 = RSVD		(reserved)
//		0 = RSVD		(reserved)

#define	INTR_1_DESIRED_MASK				(BIT5 | BIT6)
#define	UNMASK_INTR_1_INTERRUPTS		I2C_WriteByte(PAGE_0_0X72, 0x75, INTR_1_DESIRED_MASK)
#define	MASK_INTR_1_INTERRUPTS			I2C_WriteByte(PAGE_0_0X72, 0x75, 0x00)

//	Look for interrupts on CBUS:CBUS_INTR_STATUS [0xC8:0x08]
//		7 = RSVD			(reserved)
//		6 = MSC_RESP_ABORT	(interested)
//		5 = MSC_REQ_ABORT	(interested)	
//		4 = MSC_REQ_DONE	(interested)
//		3 = MSC_MSG_RCVD	(interested)
//		2 = DDC_ABORT		(interested)
//		1 = RSVD			(reserved)
//		0 = rsvd			(reserved)
#define	INTR_CBUS1_DESIRED_MASK			(BIT2 | BIT3 | BIT4 | BIT5 | BIT6)
#define	UNMASK_CBUS1_INTERRUPTS			I2C_WriteByte(PAGE_CBUS_0XC8, 0x09, INTR_CBUS1_DESIRED_MASK)
#define	MASK_CBUS1_INTERRUPTS			I2C_WriteByte(PAGE_CBUS_0XC8, 0x09, 0x00)

#define	INTR_CBUS2_DESIRED_MASK			(BIT0 | BIT2 | BIT3)
#define	UNMASK_CBUS2_INTERRUPTS			I2C_WriteByte(PAGE_CBUS_0XC8, 0x1F, INTR_CBUS2_DESIRED_MASK)
#define	MASK_CBUS2_INTERRUPTS			I2C_WriteByte(PAGE_CBUS_0XC8, 0x1F, 0x00)

#define I2C_INACCESSIBLE -1
#define I2C_ACCESSIBLE 1

//
// Local scope functions.
//
static	int 	Int4Isr( void );
static	void	Int1RsenIsr( void );
static	void	MhlCbusIsr( void );
static	char DeglitchRsenLow( void );

static	void	CbusReset( void );
static	void	SwitchToD0( void );
static	void	SwitchToD3( void );
static	void	WriteInitialRegisterValues ( void );
static	void	InitCBusRegs( void );
static	void	ForceUsbIdSwitchOpen ( void );
static	void	ReleaseUsbIdSwitchOpen ( void );
static	void	MhlTxDrvProcessConnection ( void );
static	char	MhlTxDrvProcessDisconnection ( char type );
///static  void    MhlTxDrvDisconnection ( void );
static	void	ApplyDdcAbortSafety(void);

#define	APPLY_PLL_RECOVERY	true

#ifdef APPLY_PLL_RECOVERY
static  void    SiiMhlTxDrvRecovery( void );
#endif

void Sil_USB_MHL_Switch( char Side );

struct hdmi_data {
	struct hdmi_sil9244_platform_data *pd;
	struct work_struct work;
};

static const struct i2c_device_id sil9244_id[] = {
	{ SIL9244_DRV_NAME , 0},
	{}
};
// Luke fix MHL crash with OTG turn on
static struct hdmi_data *dd = NULL;

struct workqueue_struct *mhl_private_wq = NULL;

#if 0 //B: Robert, 20111016, QCT8x60_CR1058 : Rollback pmic_id_notif_init related handling
static struct delayed_work usb_id_work;
#endif //E: Robert, 20111016, QCT8x60_CR1058

static struct delayed_work cable_work;
// Bug 909, Luke Implement MHL charging function
static struct delayed_work mhl_charging_work;

//struct delayed_work sii_9244_hpd_timer;
struct delayed_work sii_rsen_check_work;
struct delayed_work sii_rsen_deglitch_work;
struct delayed_work sii_switch_to_mhl_work;
struct delayed_work sii_switch_to_usb_work;


static char rsen_check_flag = false;
static char rsen_deglitch_flag = false;

static	bool_t	skip_MHL_switch = false; //Robert, 20111117, QCT8x60_CR1392 : Fix Moto charger (USB_ID low) issue

////////////////////////////////////////////////////////////////////
//
// E X T E R N A L L Y    E X P O S E D   A P I    F U N C T I O N S
//
////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxChipInitialize
//
// Chip specific initialization.
// This function is for SiI 9244 Initialization: HW Reset, Interrupt enable.
//
//
//////////////////////////////////////////////////////////////////////////////

bool_t SiiMhlTxChipInitialize ( void )
{
	int Chip_ID;

	TX_DEBUG_PRINT( ("Drv: SiiMhlTxChipInitialize\n") );

	// Toggle TX reset pin
	///pinTxHwReset = LOW;
	///HalTimerWait_msleep(TX_HW_RESET_PERIOD);
	///pinTxHwReset = HIGH;
         // BugID 625, Luke 2011.8.10, MHL chip enter sleep mode
	dd->pd->mhl_reset();
         // BugID 625, Luke 2011.8.10, MHL chip enter sleep mode

        Chip_ID = (int)I2C_ReadByte(PAGE_0_0X72, 0x03);
 
	//TX_DEBUG_PRINT( ("Drv: Read SiiMhlChipID: %02X44\n", Chip_ID) );
        printk(KERN_INFO "Drv: Read SiiMhlChipID: %02X44\n", Chip_ID); 
        if( Chip_ID != 0x92 )   return false;
	
	//
	// Setup our own timer for now. 50ms.
	//
	HalTimerSet( ELAPSED_TIMER, MONITORING_PERIOD );

        // setup device registers. Ensure RGND interrupt would happen.
	WriteInitialRegisterValues();

	I2C_WriteByte(PAGE_0_0X72, 0x71, INTR_1_DESIRED_MASK); // clear HPD & RSEN interrupts

	// Setup interrupt masks for all those we are interested.
	UNMASK_INTR_1_INTERRUPTS;
	UNMASK_INTR_2_INTERRUPTS;
	UNMASK_INTR_4_INTERRUPTS;
	///SiiOsMhlTxInterruptEnable();

	// CBUS interrupts are unmasked after performing the reset.
	// UNMASK_CBUS1_INTERRUPTS;
	// UNMASK_CBUS2_INTERRUPTS;

	//
	// Allow regular operation - i.e. pinAllowD3 is high so we do enter
	// D3 first time. Later on, SiIMon shall control this GPIO.
	//
	pinAllowD3 = 1;

	SwitchToD3();

	return true;
}
///////////////////////////////////////////////////////////////////////////////
// SiiMhlTxDeviceIsr
//
// This function must be called from a master interrupt handler or any polling
// loop in the host software. SiiMhlTxGetEvents will not look at these
// events assuming firmware is operating in interrupt driven mode. MhlTx component
// performs a check of all its internal status registers to see if a hardware event
// such as connection or disconnection has happened or an RCP message has been
// received from the connected device.
// MhlTx code will ensure concurrency by asking the host software and hardware to
// disable interrupts and restore when completed. Device interrupts are cleared by
// the MhlTx component before returning back to the caller. Any handling of
// programmable interrupt controller logic if present in the host will have to
// be done by the caller after this function returns back.

// This function has no parameters and returns nothing.
//
// This is the master interrupt handler for 9244. It calls sub handlers
// of interest. Still couple of status would be required to be picked up
// in the monitoring routine (Sii9244TimerIsr)
//
// To react in least amount of time hook up this ISR to processor's
// interrupt mechanism.
//
// Just in case environment does not provide this, set a flag so we
// call this from our monitor (Sii9244TimerIsr) in periodic fashion.
//
// Device Interrupts we would look at
//		RGND		= to wake up from D3
//		MHL_EST 	= connection establishment
//		CBUS_LOCKOUT= Service USB switch
//		RSEN_LOW	= Disconnection deglitcher
//		CBUS 		= responder to peer messages
//					  Especially for DCAP etc time based events
//
void 	SiiMhlTxDeviceIsr( void )
{
       char disconnected= false;
       static int fail_num = 0; //Robert, 20111117, QCT8x60_CR1392 : Fix Moto charger (USB_ID low) issue
	//
	// Look at discovery interrupts if not yet connected.
	//
	if( POWER_STATE_D0_MHL != fwPowerState )
	{
		//
		// Check important RGND, MHL_EST, CBUS_LOCKOUT and SCDT interrupts
		// During D3 we only get RGND but same ISR can work for both states
		//
		if (I2C_INACCESSIBLE == Int4Isr())
		{
			//B: Robert, 20111117, QCT8x60_CR1392 : Fix Moto charger (USB_ID low) issue
			fail_num++;
			if(fail_num >= 3)
				skip_MHL_switch=true;
			//E: Robert, 20111117, QCT8x60_CR1392
			return;		// don't do any more i2c traffic until next interrupt
		}
		else //B: Robert, 20111117, QCT8x60_CR1392 : Fix Moto charger (USB_ID low) issue
		{
			skip_MHL_switch=false;
			fail_num = 0;
		} //E: Robert, 20111117, QCT8x60_CR1392
	}
	else if( POWER_STATE_D0_MHL == fwPowerState )
	{
		//
		// Check RSEN LOW interrupt and apply deglitch timer for transition
		// from connected to disconnected state.
		//
		if(rsen_check_flag)///HalTimerExpired( TIMER_TO_DO_RSEN_CHK ))
		{
			//
			// If no MHL cable is connected, we may not receive interrupt for RSEN at all
			// as nothing would change. Poll the status of RSEN here.
			//
			// Also interrupt may come only once who would have started deglitch timer.
			// The following function will look for expiration of that before disconnection.
			//
			if(deglitchingRsenNow)
			{
				TX_DEBUG_PRINT(("Drv: deglitchingRsenNow.\n"));
				disconnected = DeglitchRsenLow();
			}
			else
			{
				Int1RsenIsr();
			}
		}
#ifdef	APPLY_PLL_RECOVERY
		//
		// Trigger a PLL recovery if SCDT is high or FIFO overflow has happened.
		//
		if ((MHL_STATUS_PATH_ENABLED & linkMode) && (BIT6 &dsHpdStatus) &&(contentOn))
		{
		SiiMhlTxDrvRecovery();
		}

#endif	//	APPLY_PLL_RECOVERY
		//
		// Check for any peer messages for DCAP_CHG etc
		// Dispatch to have the CBUS module working only once connected.
		//
		if( disconnected  == false )     MhlCbusIsr();
		// Call back into the MHL component to give it a chance to
		// take care of any message processing caused by this interrupt.
    	        MhlTxProcessEvents();
	}
}

///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxDrvAcquireUpstreamHPDControlDriveLow
//
// Acquire the direct control of Upstream HPD.
//
static void SiiMhlTxDrvAcquireUpstreamHPDControlDriveLow(void)
{
	// set reg_hpd_out_ovr_en to first control the hpd and clear reg_hpd_out_ovr_val
 	ReadModifyWritePage0(0x79, BIT5 | BIT4, BIT4);	// Force upstream HPD to 0 when not in MHL mode.
	TX_DEBUG_PRINT(("Drv: Upstream HPD Acquired - driven low.\n"));
}

///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxDrvReleaseUpstreamHPDControl
//
// Release the direct control of Upstream HPD.
//
static void SiiMhlTxDrvReleaseUpstreamHPDControl(void)
{
   	// Un-force HPD (it was kept low, now propagate to source
	// let HPD float by clearing reg_hpd_out_ovr_en
   	CLR_BIT(PAGE_0_0X72, 0x79, 4);
	TX_DEBUG_PRINT(("Drv: Upstream HPD released.\n"));
}

///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxDrvTmdsControl
//
// Control the TMDS output. MhlTx uses this to support RAP content on and off.
//
void	SiiMhlTxDrvTmdsControl( bool_t enable )
{
	if( enable )
	{
	    SET_BIT(PAGE_0_0X72, 0x80, 4);
	    TX_DEBUG_PRINT(("Drv: TMDS Output Enabled\n"));
            SiiMhlTxDrvReleaseUpstreamHPDControl();  // this triggers an EDID read
	}
	else
	{
	    CLR_BIT(PAGE_0_0X72, 0x80, 4);
	    TX_DEBUG_PRINT(("Drv: TMDS Output Disabled\n"));
	}
}
///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxDrvNotifyEdidChange
//
// MhlTx may need to inform upstream device of an EDID change. This can be
// achieved by toggling the HDMI HPD signal or by simply calling EDID read
// function.
//
void	SiiMhlTxDrvNotifyEdidChange ( void )
{
        TX_DEBUG_PRINT(("Drv: SiiMhlTxDrvNotifyEdidChange\n"));
	//
	// Prepare to toggle HPD to upstream
	//
        SiiMhlTxDrvAcquireUpstreamHPDControlDriveLow();

	// wait a bit
	HalTimerWait_msleep(110);

        // force upstream HPD back to high by reg_hpd_out_ovr_val = HIGH
	SET_BIT(PAGE_0_0X72, 0x79, 5);

        // release control to allow transcoder to modulate for CLR_HPD and SET_HPD
        SiiMhlTxDrvReleaseUpstreamHPDControl();

}
//------------------------------------------------------------------------------
// Function:    SiiMhlTxDrvSendCbusCommand
//
// Write the specified Sideband Channel command to the CBUS.
// Command can be a MSC_MSG command (RCP/RAP/RCPK/RCPE/RAPK), or another command 
// such as READ_DEVCAP, SET_INT, WRITE_STAT, etc.
//
// Parameters:  
//              pReq    - Pointer to a cbus_req_t structure containing the 
//                        command to write
// Returns:     true    - successful write
//              false   - write failed
//------------------------------------------------------------------------------

bool_t SiiMhlTxDrvSendCbusCommand ( cbus_req_t *pReq  )
{
    bool_t  success = true;

    uint8_t i, startbit;

    //
    // If not connected, return with error
    //
    if( (POWER_STATE_D0_MHL != fwPowerState ) || (mscCmdInProgress))
    {
	    TX_DEBUG_PRINT(("Error: Drv: fwPowerState: %02X, or CBUS(0x0A):%02X mscCmdInProgress = %d\n",
				(int) fwPowerState,
				(int) ReadByteCBUS(0x0a),
				(int) mscCmdInProgress));

   		return false;
    }
    // Now we are getting busy
    mscCmdInProgress	= true;


    TX_DEBUG_PRINT(("Drv: Sending MSC command %02X, %02X, %02X, %02X\n", 
			(int)pReq->command, 
			(int)(pReq->offsetData),
		 	(int)pReq->payload_u.msgData[0],
		 	(int)pReq->payload_u.msgData[1]));

    /****************************************************************************************/
    /* Setup for the command - write appropriate registers and determine the correct        */
    /*                         start bit.                                                   */
    /****************************************************************************************/

    // Set the offset and outgoing data byte right away
	WriteByteCBUS( (REG_CBUS_PRI_ADDR_CMD    & 0xFF), pReq->offsetData); 	// set offset
	WriteByteCBUS( (REG_CBUS_PRI_WR_DATA_1ST & 0xFF), pReq->payload_u.msgData[0]);
	
    startbit = 0x00;
    switch ( pReq->command )
    {
	case MHL_SET_INT:	// Set one interrupt register = 0x60
	     startbit = MSC_START_BIT_WRITE_REG;
	     break;

        case MHL_WRITE_STAT:	// Write one status register = 0x60 | 0x80
             startbit = MSC_START_BIT_WRITE_REG;
             break;

        case MHL_READ_DEVCAP:	// Read one device capability register = 0x61
             startbit = MSC_START_BIT_READ_REG;
             break;

 	case MHL_GET_STATE:			// 0x62 -
	case MHL_GET_VENDOR_ID:		// 0x63 - for vendor id	
	case MHL_SET_HPD:			// 0x64	- Set Hot Plug Detect in follower
	case MHL_CLR_HPD:			// 0x65	- Clear Hot Plug Detect in follower
	case MHL_GET_SC1_ERRORCODE:		// 0x69	- Get channel 1 command error code
	case MHL_GET_DDC_ERRORCODE:		// 0x6A	- Get DDC channel command error code.
	case MHL_GET_MSC_ERRORCODE:		// 0x6B	- Get MSC command error code.
	case MHL_GET_SC3_ERRORCODE:		// 0x6D	- Get channel 3 command error code.
	     WriteByteCBUS( (REG_CBUS_PRI_ADDR_CMD & 0xFF), pReq->command );
             startbit = MSC_START_BIT_MSC_CMD;
             break;

        case MHL_MSC_MSG:
			WriteByteCBUS( (REG_CBUS_PRI_WR_DATA_2ND & 0xFF), pReq->payload_u.msgData[1] );
	     WriteByteCBUS( (REG_CBUS_PRI_ADDR_CMD & 0xFF), pReq->command );
             startbit = MSC_START_BIT_VS_CMD;
             break;

        case MHL_WRITE_BURST:
            ReadModifyWriteCBUS((REG_MSC_WRITE_BURST_LEN & 0xFF),0x0F,pReq->length -1);

             // Now copy all bytes from array to local scratchpad
            if (NULL == pReq->payload_u.pdatabytes)
            {
                TX_DEBUG_PRINT(("Drv: Put pointer to WRITE_BURST data in req.pdatabytes!!!\n\n"));
            }
            else
            {
                uint8_t *pData = pReq->payload_u.pdatabytes;
                TX_DEBUG_PRINT(("Drv: Writing data into scratchpad\n\n"));
                for ( i = 0; i < pReq->length; i++ )
                {
                    WriteByteCBUS( (REG_CBUS_SCRATCHPAD_0 & 0xFF) + i, *pData++ );
                }
             }
             startbit = MSC_START_BIT_WRITE_BURST;
             break;

        default:
             success = false;
             break;
    }

    /****************************************************************************************/
    /* Trigger the CBUS command transfer using the determined start bit.                    */
    /****************************************************************************************/

    if ( success )
    {
        WriteByteCBUS( REG_CBUS_PRI_START & 0xFF, startbit );
    }
    else
    {
        TX_DEBUG_PRINT(("Drv: SiiMhlTxDrvSendCbusCommand failed\n\n"));

    }

    return( success );
}

bool_t SiiMhlTxDrvCBusBusy(void)
{
    return mscCmdInProgress ? true :false;
}

////////////////////////////////////////////////////////////////////
//
// L O C A L    F U N C T I O N S
//

////////////////////////////////////////////////////////////////////
// Int1RsenIsr
//
// This interrupt is used only to decide if the MHL is disconnected
// The disconnection is determined by looking at RSEN LOW and applying
// all MHL compliant disconnect timings and deglitch logic.
//
//	Look for interrupts on INTR_1 (Register 0x71)
//		7 = RSVD		(reserved)
//		6 = MDI_HPD		(interested)
//		5 = RSEN CHANGED(interested)	
//		4 = RSVD		(reserved)
//		3 = RSVD		(reserved)
//		2 = RSVD		(reserved)
//		1 = RSVD		(reserved)
//		0 = RSVD		(reserved)
////////////////////////////////////////////////////////////////////

void Int1ProcessRsen(uint8_t rsen)
{
              ///pinCBusToggle = 1;	// for debug measurements. RSEN intr
              //
     		// RSEN becomes LOW in SYS_STAT register 0x72:0x09[2]
		// SYS_STAT	==> bit 7 = VLOW, 6:4 = MSEL, 3 = TSEL, 2 = RSEN, 1 = HPD, 0 = TCLK STABLE
		//
		// Start countdown timer for deglitch
		// Allow RSEN to stay low this much before reacting
		//
		if(rsen == 0x00)
		{
			TX_DEBUG_PRINT (("Drv: Int1RsenIsr: Start T_SRC_RSEN_DEGLITCH (%d ms) before disconnection\n",
									 (int)(T_SRC_RSEN_DEGLITCH) ) );
			//
			// We got this interrupt due to cable removal
			// Start deglitch timer
			//
			///HalTimerSet(TIMER_TO_DO_RSEN_DEGLITCH, T_SRC_RSEN_DEGLITCH);
			rsen_deglitch_flag = false; 
			queue_delayed_work( mhl_private_wq, &sii_rsen_deglitch_work, msecs_to_jiffies(T_SRC_RSEN_DEGLITCH) );

			deglitchingRsenNow = true;
		}
		else if( deglitchingRsenNow )
		{
		        TX_DEBUG_PRINT(("Drv: Ignore now, RSEN is high. This was a glitch.\n"));
			//
			// Ignore now, this was a glitch
			//
			deglitchingRsenNow = false;
		}
}
void	Int1RsenIsr( void )
{
	uint8_t		reg71 = I2C_ReadByte(PAGE_0_0X72, 0x71);
	uint8_t		rsen  = I2C_ReadByte(PAGE_0_0X72, 0x09) & BIT2;

        // Avoid suspend enter D3
        if( sil_suspend_flag )  rsen |= BIT2;

	// Look at RSEN interrupt.
	// If RSEN interrupt is lost, check if we should deglitch using the RSEN status only.
	if (reg71 & BIT5)
	{
		TX_DEBUG_PRINT (("Drv: Got INTR_1: from reg71 = %02X, rsen = %02X\n", (int) reg71, (int) rsen));
		Int1ProcessRsen(rsen);
		// Clear MDI_RSEN interrupt
		I2C_WriteByte(PAGE_0_0X72, 0x71, BIT5);
	}
	else if ((false == deglitchingRsenNow) && (rsen == 0x00))
	{
		TX_DEBUG_PRINT (("Drv: Got INTR_1: reg71 = %02X, from rsen = %02X\n", (int) reg71, (int) rsen));
		Int1ProcessRsen(rsen);
	}
	else if( deglitchingRsenNow )
	{
		TX_DEBUG_PRINT(("Drv: Ignore now coz (reg71 & BIT5) has been cleared. This was a glitch.\n"));
		//
		// Ignore now, this was a glitch
		//
		deglitchingRsenNow = false;
	}
}

//////////////////////////////////////////////////////////////////////////////
//
// DeglitchRsenLow
//
// This function looks at the RSEN signal if it is low.
//
// The disconnection will be performed only if we were in fully MHL connected
// state for more than 400ms AND a 150ms deglitch from last interrupt on RSEN
// has expired.
//
// If MHL connection was never established but RSEN was low, we unconditionally
// and instantly process disconnection.
//
static char DeglitchRsenLow( void )
{
       char disconnected = false;
	   
	TX_DEBUG_PRINT(("Drv: DeglitchRsenLow RSEN <72:09[2]> = %02X\n", (int) (I2C_ReadByte(PAGE_0_0X72, 0x09)) ));

	if((I2C_ReadByte(PAGE_0_0X72, 0x09) & BIT2) == 0x00)
	{
		TX_DEBUG_PRINT(("Drv: RSEN is Low.\n"));
		//
		// If no MHL cable is connected or RSEN deglitch timer has started,
		// we may not receive interrupts for RSEN.
		// Monitor the status of RSEN here.
		//
		// 
		// First check means we have not received any interrupts and just started
		// but RSEN is low. Case of "nothing" connected on MHL receptacle
		//
		if( (POWER_STATE_D0_MHL == fwPowerState) && (rsen_deglitch_flag == true) )///HalTimerExpired(TIMER_TO_DO_RSEN_DEGLITCH) )
		{
			// Second condition means we were fully operational, then a RSEN LOW interrupt
			// occured and a DEGLITCH_TIMER per MHL specs started and completed.
			// We can disconnect now.
			//
			TX_DEBUG_PRINT(("Drv: Disconnection due to RSEN Low\n"));

			deglitchingRsenNow = false;

			///pinCBusToggle = 0;	// for debug measurements - disconnected due to RSEN

			// FP1226: Toggle MHL discovery to level the voltage to deterministic vale.
			DISABLE_DISCOVERY;
			ENABLE_DISCOVERY;
			//
			// We got here coz cable was never connected
			//
                     dsHpdStatus &= ~BIT6;  //cable disconnect implies downstream HPD low

                     WriteByteCBUS(0x0D,dsHpdStatus);
                     SiiMhlTxNotifyDsHpdChange( 0 );
			disconnected = MhlTxDrvProcessDisconnection( DISCONNECT_BY_RENS_DEGLITCH );

    		        // Call back into the MHL component to give it a chance to take
    		        // care of any event processing caused by this timer callback.
    		        MhlTxProcessEvents();
		}
	}
	else
	{
			//
			// Deglitch here:
			// RSEN is not low anymore. Reset the flag.
			// This flag will be now set on next interrupt.
			//
			// Stay connected
			//
			deglitchingRsenNow = false;
	}
	return  disconnected;
}
///////////////////////////////////////////////////////////////////////////
// WriteInitialRegisterValues
//
//
///////////////////////////////////////////////////////////////////////////
//Luke for DVT1 changed
#include <mach/cci_hw_id.h>

static void WriteInitialRegisterValues ( void )
{
	TX_DEBUG_PRINT(("Drv: WriteInitialRegisterValues\n"));
	// Power Up
	I2C_WriteByte(PAGE_1_0x7A, 0x3D, 0x3F);	// Power up CVCC 1.2V core
	I2C_WriteByte(PAGE_2_0x92, 0x11, 0x01);	// Enable TxPLL Clock
	I2C_WriteByte(PAGE_2_0x92, 0x12, 0x15);	// Enable Tx Clock Path & Equalizer
	I2C_WriteByte(PAGE_0_0X72, 0x08, 0x35);	// Power Up TMDS Tx Core

	if (settingMode9290 != pin9290_938x)
	{
		// Reset CBus to clear state
		CbusReset();
	}

	// Analog PLL Control
	I2C_WriteByte(PAGE_2_0x92, 0x10, 0xC1);	// bits 5:4 = 2b00 as per characterization team.
	I2C_WriteByte(PAGE_2_0x92, 0x17, 0x03);	// PLL Calrefsel
	I2C_WriteByte(PAGE_2_0x92, 0x1A, 0x20);	// VCO Cal
	I2C_WriteByte(PAGE_2_0x92, 0x22, 0x8A);	// Auto EQ
	I2C_WriteByte(PAGE_2_0x92, 0x23, 0x6A);	// Auto EQ
	I2C_WriteByte(PAGE_2_0x92, 0x24, 0xAA);	// Auto EQ
	I2C_WriteByte(PAGE_2_0x92, 0x25, 0xCA);	// Auto EQ
	I2C_WriteByte(PAGE_2_0x92, 0x26, 0xEA);	// Auto EQ
	I2C_WriteByte(PAGE_2_0x92, 0x4C, 0xA0);	// Manual zone control
	I2C_WriteByte(PAGE_2_0x92, 0x4D, 0x00);	// PLL Mode Value

	I2C_WriteByte(PAGE_0_0X72, 0x80, 0x24);	// Enable Rx PLL Clock Value, don't enable TMDS output by default
	I2C_WriteByte(PAGE_2_0x92, 0x45, 0x44);	// Rx PLL BW value from I2C
	I2C_WriteByte(PAGE_2_0x92, 0x31, 0x0A);	// Rx PLL BW ~ 4MHz
	I2C_WriteByte(PAGE_0_0X72, 0xA0, 0xD0);
	I2C_WriteByte(PAGE_0_0X72, 0xA1, 0xFC);	// Disable internal MHL driver

	if (settingMode1X == pinMode1x3x)
	{
		I2C_WriteByte(PAGE_0_0X72, 0xA3, 0xEB);
		I2C_WriteByte(PAGE_0_0X72, 0xA6, 0x0C);
	}
	else
	{	// original 3x config
		I2C_WriteByte(PAGE_0_0X72, 0xA3, 0xFA);
	}

	I2C_WriteByte(PAGE_0_0X72, 0x2B, 0x01);	// Enable HDCP Compliance safety

	//
	// CBUS & Discovery
	// CBUS discovery cycle time for each drive and float = 100us
	//
	ReadModifyWritePage0(0x90, BIT3 | BIT2, BIT2);

	// Do not perform RGND impedance detection if connected to SiI 9290
	//
	if (settingMode9290 == pin9290_938x)
	{
		I2C_WriteByte(PAGE_0_0X72, 0x91, 0xE5);		// Set bit 6 (reg_skip_rgnd)
	}
	else
	{
		I2C_WriteByte(PAGE_0_0X72, 0x91, 0xA5);		// Clear bit 6 (reg_skip_rgnd)
	}

	// Changed from 66 to 77 for 94[1:0] = 11 = 5k reg_cbusmhl_pup_sel
	// and bits 5:4 = 11 rgnd_vth_ctl
	//
	I2C_WriteByte(PAGE_0_0X72, 0x94, 0x77);			// 1.8V CBUS VTH & GND threshold

	//set bit 2 and 3, which is Initiator Timeout
	I2C_WriteByte(PAGE_CBUS_0XC8, 0x31, I2C_ReadByte(PAGE_CBUS_0XC8, 0x31) | 0x0c);

	// Establish if connected to 9290 or any other legacy product
	if (settingMode1X == pinMode1x3x)
	{
		I2C_WriteByte(PAGE_0_0X72, 0xA5, 0xA0);	// bits 4:2. rgnd_res_ctl = 3'b000.

		TX_DEBUG_PRINT(("Drv: MHL 1.0 Compliant Clock\n"));

	}
	else
	{	// Legacy 3x config
		I2C_WriteByte(PAGE_0_0X72, 0xA5, 0x00);			// RGND Hysteresis.

		TX_DEBUG_PRINT(("Drv: Legacy 3x Clock <0xA5[7:0] = 0>\n"));
	}

	// RGND & single discovery attempt (RGND blocking) , Force USB ID switch to open
	I2C_WriteByte(PAGE_0_0X72, 0x95, 0x71);

	// Use only 1K for MHL impedance. Set BIT5 for No-open-drain.
	// Default is good.
	//
	// Use 1k and 2k commented.
//	I2C_WriteByte(PAGE_0_0X72, 0x96, 0x22);

	// Use VBUS path of discovery state machine
	I2C_WriteByte(PAGE_0_0X72, 0x97, 0x00);


	//
	// For MHL compliance we need the following settings for register 93 and 94
	// Bug 20686
	//
	// To allow RGND engine to operate correctly.
	//
	// When moving the chip from D2 to D0 (power up, init regs) the values should be
	// 94[1:0] = 11  reg_cbusmhl_pup_sel[1:0] should be set for 5k
	// 93[7:6] = 10  reg_cbusdisc_pup_sel[1:0] should be set for 10k
	// 93[5:4] = 00  reg_cbusidle_pup_sel[1:0] = open (default)
	//
	if (settingMode9290 == pin9290_938x)
	{
		WriteBytePage0(0x92, 0x46);				// Force MHL mode
		WriteBytePage0(0x93, 0xDC);				// Disable CBUS pull-up during RGND measurement
	}
	else
	{
		// Luke MHL logo test 14.3 patch
		// WriteBytePage0(0x92, 0x86);				//
		WriteBytePage0(0x92, 0xA6);	
		// change from CC to 8C to match 10K
                // 0b11 is 5K, 0b10 is 10K, 0b01 is 20k and 0b00 is off
		WriteBytePage0(0x93, 0x8C);				// Disable CBUS pull-up during RGND measurement
	}

	if (settingMode9290 != pin9290_938x)
	{
                SiiMhlTxDrvAcquireUpstreamHPDControlDriveLow();
		///pinGpio1 = 0;
	}

       //Luke for DVT1 changed
	if( cci_hw_id >= DVT1 )   ReadModifyWritePage0(0x79, BIT6, 0);   // HPD set push-pull mode, need remove external pull high resistance

	HalTimerWait_msleep(25);
	ReadModifyWritePage0(0x95, BIT6, 0x00);		// Release USB ID switch

	I2C_WriteByte(PAGE_0_0X72, 0x90, 0x27);			// Enable CBUS discovery

	InitCBusRegs();

	// Enable Auto soft reset on SCDT = 0
	I2C_WriteByte(PAGE_0_0X72, 0x05, 0x04);

	// HDMI Transcode mode enable
	I2C_WriteByte(PAGE_0_0X72, 0x0D, 0x1C);

        UNMASK_INTR_1_INTERRUPTS;
	UNMASK_INTR_2_INTERRUPTS;
	UNMASK_INTR_4_INTERRUPTS;

	///pinLed3xForProbe = 0;
	///pinProbe = 0;
}

///////////////////////////////////////////////////////////////////////////
// InitCBusRegs
//
///////////////////////////////////////////////////////////////////////////
static void InitCBusRegs( void )
{
	uint8_t		regval;

	TX_DEBUG_PRINT(("Drv: InitCBusRegs\n"));
	// Increase DDC translation layer timer
	I2C_WriteByte(PAGE_CBUS_0XC8, 0x07, 0xF2);          // new default is for MHL mode
	I2C_WriteByte(PAGE_CBUS_0XC8, 0x40, 0x03); 			// CBUS Drive Strength
	I2C_WriteByte(PAGE_CBUS_0XC8, 0x42, 0x06); 			// CBUS DDC interface ignore segment pointer
	I2C_WriteByte(PAGE_CBUS_0XC8, 0x36, 0x0C);

	I2C_WriteByte(PAGE_CBUS_0XC8, 0x3D, 0xFD);	
	I2C_WriteByte(PAGE_CBUS_0XC8, 0x1C, 0x01);
	I2C_WriteByte(PAGE_CBUS_0XC8, 0x1D, 0x0F);          // MSC_RETRY_FAIL_LIM


	if (settingMode9290 == pin9290_938x)
	{
		I2C_WriteByte(PAGE_CBUS_0XC8, 0x44, 0x00);
	}
	else
	{
		I2C_WriteByte(PAGE_CBUS_0XC8, 0x44, 0x02);
	}

	// Setup our devcap
	I2C_WriteByte(PAGE_CBUS_0XC8, 0x80,0x00 );//DEVCAP_VAL_DEV_STATE       );
	I2C_WriteByte(PAGE_CBUS_0XC8, 0x81,0x11 );// DEVCAP_VAL_MHL_VERSION     );
	I2C_WriteByte(PAGE_CBUS_0XC8, 0x82,0x02 );// DEVCAP_VAL_DEV_CAT         );
        // Luke Modify ADOPTER_ID for MHL logo test
	I2C_WriteByte(PAGE_CBUS_0XC8, 0x83,0x03 );// DEVCAP_VAL_ADOPTER_ID_H    );
	I2C_WriteByte(PAGE_CBUS_0XC8, 0x84,0x72 );// DEVCAP_VAL_ADOPTER_ID_L    );
	I2C_WriteByte(PAGE_CBUS_0XC8, 0x85,0x21 );// DEVCAP_VAL_VID_LINK_MODE   );
	I2C_WriteByte(PAGE_CBUS_0XC8, 0x86,0x01 );// DEVCAP_VAL_AUD_LINK_MODE   );
	I2C_WriteByte(PAGE_CBUS_0XC8, 0x87,0x00 );// DEVCAP_VAL_VIDEO_TYPE      );
	I2C_WriteByte(PAGE_CBUS_0XC8, 0x88,0x80 );// DEVCAP_VAL_LOG_DEV_MAP     );
	I2C_WriteByte(PAGE_CBUS_0XC8, 0x89,0x0F );// DEVCAP_VAL_BANDWIDTH       );
	I2C_WriteByte(PAGE_CBUS_0XC8, 0x8A,0x07 );// DEVCAP_VAL_FEATURE_FLAG    );
	I2C_WriteByte(PAGE_CBUS_0XC8, 0x8B,0x00 );// DEVCAP_VAL_DEVICE_ID_H     );
	I2C_WriteByte(PAGE_CBUS_0XC8, 0x8C,0x00 );// DEVCAP_VAL_DEVICE_ID_L     );
	I2C_WriteByte(PAGE_CBUS_0XC8, 0x8D,0x10 ); //DEVCAP_VAL_SCRATCHPAD_SIZE );
	I2C_WriteByte(PAGE_CBUS_0XC8, 0x8E,0x33 );// DEVCAP_VAL_INT_STAT_SIZE   );
	I2C_WriteByte(PAGE_CBUS_0XC8, 0x8F,0x00 );//DEVCAP_VAL_RESERVED        );

	// Make bits 2,3 (initiator timeout) to 1,1 for register CBUS_LINK_CONTROL_2
	regval = I2C_ReadByte(PAGE_CBUS_0XC8, REG_CBUS_LINK_CONTROL_2 );
	regval = (regval | 0x0C);
	I2C_WriteByte(PAGE_CBUS_0XC8,REG_CBUS_LINK_CONTROL_2, regval);

	 // Clear legacy bit on Wolverine TX.
         regval = I2C_ReadByte(PAGE_CBUS_0XC8, REG_MSC_TIMEOUT_LIMIT);
    regval &= ~MSC_TIMEOUT_LIMIT_MSB_MASK;
    regval |= 0x0F;
         I2C_WriteByte(PAGE_CBUS_0XC8, REG_MSC_TIMEOUT_LIMIT, (regval & MSC_TIMEOUT_LIMIT_MSB_MASK));

	// Set NMax to 1
	I2C_WriteByte(PAGE_CBUS_0XC8, REG_CBUS_LINK_CONTROL_1, 0x01);
	ReadModifyWriteCBUS( REG_CBUS_LINK_CONTROL_11
	    , BIT5 | BIT4 | BIT3
	    , BIT5 | BIT4
	    );
        // Luke Modify  bit timer for MHL logo test
        I2C_WriteByte(PAGE_CBUS_0XC8,REG_CBUS_LINK_CONTROL_11, 0xA4);//0xB4(10110100)(6), 0xA4(10100100)(4), 0xAC(10101100)(5), 0x9C(10011100)(3), 0x94(10010100)(2), 

	ReadModifyWriteCBUS( REG_MSC_TIMEOUT_LIMIT, 0x0F, 0x0D);

        ReadModifyWriteCBUS(0x2E
            ,BIT4 | BIT2 | BIT0
            ,BIT4 | BIT2 | BIT0
            );

}

///////////////////////////////////////////////////////////////////////////
//
// ForceUsbIdSwitchOpen
//
///////////////////////////////////////////////////////////////////////////
static void ForceUsbIdSwitchOpen ( void )
{
        DISABLE_DISCOVERY 		// Disable CBUS discovery
	ReadModifyWritePage0(0x95, BIT6, BIT6);	// Force USB ID switch to open

	if( settingMode9290 == pin9290_938x )
	{
		WriteBytePage0(0x92, 0x46);				// Force MHL mode
	}
	else
	{
		// Luke Luke MHL logo test 14.3 patch
		// WriteBytePage0(0x92, 0x86);
		WriteBytePage0(0x92, 0xA6);

		// Force HPD to 0 when not in MHL mode.
                SiiMhlTxDrvAcquireUpstreamHPDControlDriveLow();
	}
	///pinGpio1 = 0;
}
///////////////////////////////////////////////////////////////////////////
//
// ReleaseUsbIdSwitchOpen
//
///////////////////////////////////////////////////////////////////////////
static void ReleaseUsbIdSwitchOpen ( void )
{
	HalTimerWait_msleep(50); // per spec

	// Release USB ID switch
	ReadModifyWritePage0(0x95, BIT6, 0x00);

	ENABLE_DISCOVERY;
}

/////////////////////////////////////////////////////////////////////////////
//
// FUNCTION     :   CbusWakeUpPulseGenerator ()
//
// PURPOSE      :   Generate Cbus Wake up pulse sequence using GPIO or I2C method.
//
// INPUT PARAMS :   None
//
// OUTPUT PARAMS:   None
//
// GLOBALS USED :   None
//
// RETURNS      :   None
//
//////////////////////////////////////////////////////////////////////////////

static void CbusWakeUpPulseGenerator(void)
{	
	TX_DEBUG_PRINT(("Drv: CbusWakeUpPulseGenerator\n"));

	//
	// I2C method
	//
	// Start the pulse
	I2C_WriteByte(PAGE_0_0X72, 0x96, (I2C_ReadByte(PAGE_0_0X72, 0x96) | 0xC0));
        // Luke, fix HDMI sometimes hang when cable plug-in
        HalTimerWait_usleep(T_SRC_WAKE_PULSE_WIDTH_1);	// adjust for code path

	I2C_WriteByte(PAGE_0_0X72, 0x96, (I2C_ReadByte(PAGE_0_0X72, 0x96) & 0x3F));
        // Luke, fix HDMI sometimes hang when cable plug-in
        HalTimerWait_usleep(T_SRC_WAKE_PULSE_WIDTH_1);	// adjust for code path

	I2C_WriteByte(PAGE_0_0X72, 0x96, (I2C_ReadByte(PAGE_0_0X72, 0x96) | 0xC0));
        // Luke, fix HDMI sometimes hang when cable plug-in
        HalTimerWait_usleep(T_SRC_WAKE_PULSE_WIDTH_1);	// adjust for code path

	I2C_WriteByte(PAGE_0_0X72, 0x96, (I2C_ReadByte(PAGE_0_0X72, 0x96) & 0x3F));
        // Luke, fix HDMI sometimes hang when cable plug-in
        HalTimerWait_msleep(T_SRC_WAKE_PULSE_WIDTH_2);	// adjust for code path

	I2C_WriteByte(PAGE_0_0X72, 0x96, (I2C_ReadByte(PAGE_0_0X72, 0x96) | 0xC0));
        // Luke, fix HDMI sometimes hang when cable plug-in
        HalTimerWait_usleep(T_SRC_WAKE_PULSE_WIDTH_1);	// adjust for code path

	I2C_WriteByte(PAGE_0_0X72, 0x96, (I2C_ReadByte(PAGE_0_0X72, 0x96) & 0x3F));
        // Luke, fix HDMI sometimes hang when cable plug-in
        HalTimerWait_usleep(T_SRC_WAKE_PULSE_WIDTH_1);	// adjust for code path

	I2C_WriteByte(PAGE_0_0X72, 0x96, (I2C_ReadByte(PAGE_0_0X72, 0x96) | 0xC0));
        // Luke, fix HDMI sometimes hang when cable plug-in
	HalTimerWait_msleep(T_SRC_WAKE_PULSE_WIDTH_1);

	I2C_WriteByte(PAGE_0_0X72, 0x96, (I2C_ReadByte(PAGE_0_0X72, 0x96) & 0x3F));

	HalTimerWait_msleep(T_SRC_WAKE_TO_DISCOVER);

	//
	// Toggle MHL discovery bit
	//
//	DISABLE_DISCOVERY;
//	ENABLE_DISCOVERY;

}

///////////////////////////////////////////////////////////////////////////
//
// ApplyDdcAbortSafety
//
///////////////////////////////////////////////////////////////////////////
static	void	ApplyDdcAbortSafety(void)
{
	uint8_t		bTemp, bPost;
        #if 0
	TX_DEBUG_PRINT(("Drv: Do we need DDC Abort Safety\n"));
        #endif

	WriteByteCBUS(0x29, 0xFF);  // clear the ddc abort counter
	bTemp = ReadByteCBUS(0x29);  // get the counter
	HalTimerWait_msleep(3);
	bPost = ReadByteCBUS(0x29);  // get another value of the counter

        TX_DEBUG_PRINT(("Drv: bTemp: 0x%X bPost: 0x%X\n",(int)bTemp,(int)bPost));

	if (bPost > (bTemp + 50))
	{
		TX_DEBUG_PRINT(("Drv: Applying DDC Abort Safety(SWWA 18958)\n"));

                CbusReset();

		InitCBusRegs();

		ForceUsbIdSwitchOpen();
		ReleaseUsbIdSwitchOpen();

		MhlTxDrvProcessDisconnection( DISCONNECT_BY_DDC_ABORT );
	}

}

/*
	SiiMhlTxDrvProcessMhlConnection
	optionally called by the MHL Tx Component after giving the OEM layer the
	first crack at handling the event.
*/
void SiiMhlTxDrvProcessRgndMhl( void )
{
   	// Select CBUS drive float.
   	SET_BIT(PAGE_0_0X72, 0x95, 5);

   	TX_DEBUG_PRINT(("Drv: Waiting T_SRC_VBUS_CBUS_TO_STABLE (%d ms)\n", (int)T_SRC_VBUS_CBUS_TO_STABLE));
   	HalTimerWait_msleep(T_SRC_VBUS_CBUS_TO_STABLE);

   	// Discovery enabled
   //	STROBE_POWER_ON

   	//
   	// Send slow wake up pulse using GPIO or I2C
   	//
   	CbusWakeUpPulseGenerator();
}
///////////////////////////////////////////////////////////////////////////
// ProcessRgnd
//
// H/W has detected impedance change and interrupted.
// We look for appropriate impedance range to call it MHL and enable the
// hardware MHL discovery logic. If not, disable MHL discovery to allow
// USB to work appropriately.
//
// In current chip a firmware driven slow wake up pulses are sent to the
// sink to wake that and setup ourselves for full D0 operation.
///////////////////////////////////////////////////////////////////////////
// Bug 909, Luke Implement MHL charging function -->
void msm_charger_vbus_draw(unsigned int mA);
void smb137b_otg_power(int on);
extern char cci_usb_power_plugin;
// Bug 909, Luke Implement MHL charging function <--

static void	ProcessRgnd( void )
{
	uint8_t		reg99RGNDRange;
	//
	// Impedance detection has completed - process interrupt
	//
	reg99RGNDRange = I2C_ReadByte(PAGE_0_0X72, 0x99) & 0x03;
	TX_DEBUG_PRINT(("Drv: RGND Reg 99 = %02X\n", (int)reg99RGNDRange));

	//
	// Reg 0x99
	// 00, 01 or 11 means USB.
	// 10 means 1K impedance (MHL)
	//
	// If 1K, then only proceed with wake up pulses
	if(0x02 == reg99RGNDRange)
	{
		TX_DEBUG_PRINT(("(MHL Device)\n"));
		SiiMhlTxNotifyRgndMhl(); // this will call the application and then optionally call
	}
	else
	{
		TX_DEBUG_PRINT((" Drv: USB impedance. Set for USB Established.\n"));

		CLR_BIT(PAGE_0_0X72, 0x95, 5);
	}
}

////////////////////////////////////////////////////////////////////
// SwitchToD0
// This function performs s/w as well as h/w state transitions.
//
// Chip comes up in D2. Firmware must first bring it to full operation
// mode in D0.
////////////////////////////////////////////////////////////////////
static void	SwitchToD0( void )
{
	TX_DEBUG_PRINT(("Drv: Switch To Full power mode (D0)\n"));

	//
	// WriteInitialRegisterValues switches the chip to full power mode.
	//
        //TX_DEBUG_PRINT(("Drv: D2->D0  0x90:0x%02x 0x93:0x%02x  0x94:0x%02x\n",(int)ReadBytePage0(0x90),(int)ReadBytePage0(0x93),(int)ReadBytePage0(0x94)));
	WriteInitialRegisterValues();
        //TX_DEBUG_PRINT(("Drv: D0:  0x90:0x%02x 0x93:0x%02x  0x94:0x%02x\n",(int)ReadBytePage0(0x90),(int)ReadBytePage0(0x93),(int)ReadBytePage0(0x94)));

        // Fix HDMI no output signal
        I2C_WriteByte(PAGE_2_0x92, 0x01, 0x00);

	// Force Power State to ON
	STROBE_POWER_ON

	fwPowerState = POWER_STATE_D0_NO_MHL;
}

////////////////////////////////////////////////////////////////////
// SwitchToD3
//
// This function performs s/w as well as h/w state transitions.
//
////////////////////////////////////////////////////////////////////
static void	SwitchToD3( void )
{
	if(POWER_STATE_D3 != fwPowerState)
	{
		TX_DEBUG_PRINT(("Drv: Switch To D3: pinAllowD3 = %d\n",(int) pinAllowD3 ));

		///pinM2U_VBUS_CTRL = 0;
		///pinMHLConn = 1;
		///pinUsbConn = 0;

		ForceUsbIdSwitchOpen();

		//
		// To allow RGND engine to operate correctly.
		// So when moving the chip from D0 MHL connected to D3 the values should be
		// 94[1:0] = 00  reg_cbusmhl_pup_sel[1:0] should be set for open
		// 93[7:6] = 00  reg_cbusdisc_pup_sel[1:0] should be set for open
		// 93[5:4] = 00  reg_cbusidle_pup_sel[1:0] = open (default)
		//
		// Disable CBUS pull-up during RGND measurement
		ReadModifyWritePage0(0x93, BIT7 | BIT6 | BIT5 | BIT4, 0);

		ReadModifyWritePage0(0x94, BIT1 | BIT0, 0);

		// 1.8V CBUS VTH & GND threshold

		ReleaseUsbIdSwitchOpen();

		// Force HPD to 0 when not in MHL mode.
                SiiMhlTxDrvAcquireUpstreamHPDControlDriveLow();

		// Change TMDS termination to high impedance on disconnection
		// Bits 1:0 set to 11
                // Fix HDMI no output signal
		// I2C_WriteByte(PAGE_2_0x92, 0x01, 0x03);

		//
		// GPIO controlled from SiIMon can be utilized to disallow
		// low power mode, thereby allowing SiIMon to debug register contents.
		// Otherwise SiIMon reads all registers as 0xFF
		//
		if(pinAllowD3)
		{
                        //TX_DEBUG_PRINT(("Drv: ->D3  0x90:0x%02x 0x93:0x%02x  0x94:0x%02x\n",
                                     //(int)ReadBytePage0(0x90),(int)ReadBytePage0     (0x93),(int)ReadBytePage0(0x94)));
			//
			// Change state to D3 by clearing bit 0 of 3D (SW_TPI, Page 1) register
			//
                        // Fix HDMI no output signal 
		        I2C_WriteByte(PAGE_2_0x92, 0x01, 0x03);
			CLR_BIT(PAGE_1_0x7A, 0x3D, 0);
		}
		fwPowerState = POWER_STATE_D3;
	}
}

////////////////////////////////////////////////////////////////////
// Int4Isr
//
//
//	Look for interrupts on INTR4 (Register 0x74)
//		7 = RSVD		(reserved)
//		6 = RGND Rdy	(interested)
//		5 = VBUS Low	(ignore)	
//		4 = CBUS LKOUT	(interested)
//		3 = USB EST		(interested)
//		2 = MHL EST		(interested)
//		1 = RPWR5V Change	(ignore)
//		0 = SCDT Change	(interested during D0)
////////////////////////////////////////////////////////////////////
static	int	Int4Isr( void )
{
	uint8_t		reg74;

	reg74 = I2C_ReadByte(PAGE_0_0X72, (0x74));	// read status
	// When I2C is inoperational (say in D3) and a previous interrupt brought us here, do nothing.
	if(0xFF == reg74)
	{
		TX_DEBUG_PRINT(("Int4Isr 0xFF == reg74\n"));
		return I2C_INACCESSIBLE;
	}

	TX_DEBUG_PRINT(("Int4Isr - reg74:0x%02x\n",(int)reg74));

	// process MHL_EST interrupt
	if(reg74 & BIT2) // MHL_EST_INT
	{
		MhlTxDrvProcessConnection();
	}

	// process USB_EST interrupt
	else if(reg74 & BIT3) // MHL_DISC_FAIL_INT
	{
		I2C_WriteByte(PAGE_0_0X72, (0x74), reg74);	// clear all interrupts before going to D3
		MhlTxDrvProcessDisconnection( DISCONNECT_BY_USB_EST );
		return I2C_INACCESSIBLE;
	}

	if((POWER_STATE_D3 == fwPowerState) && (reg74 & BIT6))
	{
		// process RGND interrupt

		// Switch to full power mode.
		SwitchToD0();

		//
		// If a sink is connected but not powered on, this interrupt can keep coming
		// Determine when to go back to sleep. Say after 1 second of this state.
		//
		// Check RGND register and send wake up pulse to the peer
		//
		ProcessRgnd();
		
        // Bug 909, Luke Implement MHL charging function
        queue_delayed_work( mhl_private_wq, &mhl_charging_work, msecs_to_jiffies(5000) );
	}

	// CBUS Lockout interrupt?
	if (reg74 & BIT4)
	{
		TX_DEBUG_PRINT(("Drv: CBus Lockout\n"));

		ForceUsbIdSwitchOpen();
		ReleaseUsbIdSwitchOpen();
	}

	I2C_WriteByte(PAGE_0_0X72, (0x74), reg74);	// clear all interrupts
	return I2C_ACCESSIBLE;
}

#ifdef	APPLY_PLL_RECOVERY
///////////////////////////////////////////////////////////////////////////
// FUNCTION:	ApplyPllRecovery
//
// PURPOSE:		This function helps recover PLL.
//
///////////////////////////////////////////////////////////////////////////
static void ApplyPllRecovery ( void )
{
	///pinProbe = 1;

	// Disable TMDS
	CLR_BIT(PAGE_0_0X72, 0x80, 4);

	// Enable TMDS
	SET_BIT(PAGE_0_0X72, 0x80, 4);

	// followed by a 10ms settle time
	HalTimerWait_msleep(10);

	// MHL FIFO Reset here 
	SET_BIT(PAGE_0_0X72, 0x05, 4);

	CLR_BIT(PAGE_0_0X72, 0x05, 4);

	///pinProbe = 0;

	TX_DEBUG_PRINT(("Drv: Applied PLL Recovery\n"));
}

/////////////////////////////////////////////////////////////////////////////
//
// FUNCTION     :   SiiMhlTxDrvRecovery ()
//
// PURPOSE      :   Check SCDT interrupt and PSTABLE interrupt
//
//
// DESCRIPTION :  If SCDT interrupt happened and current status
// is HIGH, irrespective of the last status (assuming we can miss an interrupt)
// go ahead and apply PLL recovery.
//
// When a PSTABLE interrupt happens, it is an indication of a possible
// FIFO overflow condition. Apply a recovery method.
//
//////////////////////////////////////////////////////////////////////////////
static void SiiMhlTxDrvRecovery( void )
{
	//
	// Detect Rising Edge of SCDT
	//
	// Check if SCDT interrupt came
	if((I2C_ReadByte(PAGE_0_0X72, (0x74)) & BIT0))
	{
		//
		// Clear this interrupt and then check SCDT.
		// if the interrupt came irrespective of what SCDT was earlier
		// and if SCDT is still high, apply workaround.
		//
		// This approach implicitly takes care of one lost interrupt.
		//
		SET_BIT(PAGE_0_0X72, (0x74), 0);


		// Read status, if it went HIGH
		if ( (((I2C_ReadByte(PAGE_0_0X72, 0x81)) & BIT1) >> 1) )
		{
			// Help probe SCDT
			///pinLed3xForProbe = 1;

			// Toggle TMDS and reset MHL FIFO.
			ApplyPllRecovery();
		}
		else
		{
			// Help probe SCDT
			///pinLed3xForProbe = 0;
		}
	}
	//
	// Check PSTABLE interrupt...reset FIFO if so.
	//
	if((I2C_ReadByte(PAGE_0_0X72, (0x72)) & BIT1))
	{

		TX_DEBUG_PRINT(("Drv: PSTABLE Interrupt\n"));

		// Toggle TMDS and reset MHL FIFO.
		ApplyPllRecovery();

		// clear PSTABLE interrupt. Do not clear this before resetting the FIFO.
		SET_BIT(PAGE_0_0X72, (0x72), 1);

	}
}
#endif // APPLY_PLL_RECOVERY

///////////////////////////////////////////////////////////////////////////
//
// MhlTxDrvProcessConnection
//
///////////////////////////////////////////////////////////////////////////
static void MhlTxDrvProcessConnection ( void )
{
	bool_t	mhlConnected = true;

	TX_DEBUG_PRINT (("Drv: MHL Cable Connected. CBUS:0x0A = %02X\n", (int) ReadByteCBUS(0x0a)));

	if( POWER_STATE_D0_MHL == fwPowerState )
	{
		return;
	}
	// VBUS control gpio
	///pinM2U_VBUS_CTRL = 1;
	///pinMHLConn = 0;
	///pinUsbConn = 1;

	//
	// Discovery over-ride: reg_disc_ovride	
	//
	I2C_WriteByte(PAGE_0_0X72, 0xA0, 0x10);

	fwPowerState = POWER_STATE_D0_MHL;

	//
	// Legacy product operates in DDC Burst mode
	//
	if (settingMode9290 == pin9290_938x)
	{
		// Increase DDC translation layer timer (burst mode)
		ReadModifyWriteByteCBUS(0x07, BIT3,0x05);  //DDC burst mode and CBUS burst length (read) is decoded by HDCP offset value
		WriteByteCBUS(0x47, 0x03);
	}
	else
	{
		//
		// Increase DDC translation layer timer (uint8_t mode)
		// Setting DDC Byte Mode
		//
                WriteByteCBUS(0x07, 0xF2);  // CBUS DDC byte handshake mode
		// Doing it this way causes problems with playstation: ReadModifyWriteByteCBUS(0x07, BIT2,0);

		// Enable segment pointer safety
		SET_BIT(PAGE_CBUS_0XC8, 0x44, 1);
	}

        // upstream HPD status should not be allowed to rise until HPD from downstream is detected.

        // TMDS should not be enabled until RSEN is high, and HPD and PATH_EN are received

	// Keep the discovery enabled. Need RGND interrupt
	ENABLE_DISCOVERY;

	// Wait T_SRC_RXSENSE_CHK ms to allow connection/disconnection to be stable (MHL 1.0 specs)
	TX_DEBUG_PRINT (("Drv: Wait T_SRC_RXSENSE_CHK (%d ms) before checking RSEN\n",
							(int) T_SRC_RXSENSE_CHK
							));

	//
	// Ignore RSEN interrupt for T_SRC_RXSENSE_CHK duration.
	// Get the timer started
	//
	///HalTimerSet(TIMER_TO_DO_RSEN_CHK, T_SRC_RXSENSE_CHK);
	rsen_check_flag = false;
	queue_delayed_work( mhl_private_wq, &sii_rsen_check_work, msecs_to_jiffies(T_SRC_RXSENSE_CHK) );


	// Notify upper layer of cable connection
	contentOn = 1;
	SiiMhlTxNotifyConnection(mhlConnected = true);

}
///////////////////////////////////////////////////////////////////////////
//
// MhlTxDrvProcessDisconnection
//
///////////////////////////////////////////////////////////////////////////
static char MhlTxDrvProcessDisconnection ( char type )
{
	bool_t	mhlConnected = false;

	TX_DEBUG_PRINT (("Drv: MhlTxDrvProcessDisconnection\n"));

	// clear all interrupts
//	I2C_WriteByte(PAGE_0_0X72, (0x74), I2C_ReadByte(PAGE_0_0X72, (0x74)));

	I2C_WriteByte(PAGE_0_0X72, 0xA0, 0xD0);

	//
	// Reset CBus to clear register contents
	// This may need some key reinitializations
	//
//	CbusReset();

	// Disable TMDS
	SiiMhlTxDrvTmdsControl( false );

	if( POWER_STATE_D0_MHL == fwPowerState )
	{
		// Notify upper layer of cable connection
		contentOn = 0;
		SiiMhlTxNotifyConnection(mhlConnected = false);
	}

	// Now put chip in sleep mode
	///// 
	SwitchToD3();
	
	//if( ( type == DISCONNECT_BY_RENS_DEGLITCH ) )//&& ( sil_suspend_flag == false ) )
	//{
	     mhl_cable_connected = false;
	     queue_delayed_work( mhl_private_wq, &sii_switch_to_usb_work, msecs_to_jiffies(T_SWITCH_TO_USB) );
	//}

       rsen_check_flag = false;
       rsen_deglitch_flag = false;

	return true;
}

// BugID 625, Luke 2011.8.10, MHL chip enter sleep mode
#if 0
static void MhlTxDrvDisconnection ( void )
{
        Sil_USB_MHL_Switch(0);
}
#endif

///////////////////////////////////////////////////////////////////////////
//
// CbusReset
//
///////////////////////////////////////////////////////////////////////////

static void CbusReset(void)
{

	uint8_t		idx;
	SET_BIT(PAGE_0_0X72, 0x05, 3);
	HalTimerWait_msleep(2);
	CLR_BIT(PAGE_0_0X72, 0x05, 3);

	mscCmdInProgress = false;

	// Adjust interrupt mask everytime reset is performed.
	UNMASK_CBUS1_INTERRUPTS;
	UNMASK_CBUS2_INTERRUPTS;

	for(idx=0; idx < 4; idx++)
	{
		// Enable WRITE_STAT interrupt for writes to all 4 MSC Status registers.
		I2C_WriteByte(PAGE_CBUS_0XC8, 0xE0 + idx, 0xFF);

		// Enable SET_INT interrupt for writes to all 4 MSC Interrupt registers.
		I2C_WriteByte(PAGE_CBUS_0XC8, 0xF0 + idx, 0xFF);
        }
}

///////////////////////////////////////////////////////////////////////////
//
// CBusProcessErrors
//
//
///////////////////////////////////////////////////////////////////////////
static uint8_t CBusProcessErrors( uint8_t intStatus )
{
    uint8_t result          = 0;
    uint8_t mscAbortReason  = 0;
    uint8_t ddcAbortReason  = 0;

    /* At this point, we only need to look at the abort interrupts. */

    intStatus &=  (BIT_MSC_ABORT | BIT_MSC_XFR_ABORT);

    if ( intStatus )
    {
//      result = ERROR_CBUS_ABORT;		// No Retry will help

        /* If transfer abort or MSC abort, clear the abort reason register. */
	if( intStatus & BIT_DDC_ABORT )
	{
	    result = ddcAbortReason = ReadByteCBUS( REG_DDC_ABORT_REASON );
	    TX_DEBUG_PRINT( ("CBUS DDC ABORT happened, reason:: %02X\n", (int)(ddcAbortReason)));
	}

        if ( intStatus & BIT_MSC_XFR_ABORT )
        {
            result = mscAbortReason = ReadByteCBUS( REG_PRI_XFR_ABORT_REASON );

            TX_DEBUG_PRINT( ("CBUS:: MSC Transfer ABORTED. Clearing 0x0D\n"));
            WriteByteCBUS( REG_PRI_XFR_ABORT_REASON, 0xFF );
        }
        if ( intStatus & BIT_MSC_ABORT )
        {
            TX_DEBUG_PRINT( ("CBUS:: MSC Peer sent an ABORT. Clearing 0x0E\n"));
            WriteByteCBUS( REG_CBUS_PRI_FWR_ABORT_REASON, 0xFF );
        }

        // Now display the abort reason.

        if ( mscAbortReason != 0 )
        {
            TX_DEBUG_PRINT( ("CBUS:: Reason for ABORT is ....0x%02X = ", (int)mscAbortReason ));

            if ( mscAbortReason & CBUSABORT_BIT_REQ_MAXFAIL)
            {
                TX_DEBUG_PRINT( ("Requestor MAXFAIL - retry threshold exceeded\n"));
            }
            if ( mscAbortReason & CBUSABORT_BIT_PROTOCOL_ERROR)
            {
                TX_DEBUG_PRINT( ("Protocol Error\n"));
            }
            if ( mscAbortReason & CBUSABORT_BIT_REQ_TIMEOUT)
            {
                TX_DEBUG_PRINT( ("Requestor translation layer timeout\n"));
            }
            if ( mscAbortReason & CBUSABORT_BIT_PEER_ABORTED)
            {
                TX_DEBUG_PRINT( ("Peer sent an abort\n"));
            }
            if ( mscAbortReason & CBUSABORT_BIT_UNDEFINED_OPCODE)
            {
                TX_DEBUG_PRINT( ("Undefined opcode\n"));
            }
        }
    }
    return( result );
}

void SiiMhlTxDrvGetScratchPad(uint8_t startReg,uint8_t *pData,uint8_t length)
{
    int i;
    uint8_t regOffset;

    for (regOffset= 0xC0 + startReg,i = 0; i < length;++i,++regOffset)
    {
        *pData++ = ReadByteCBUS( regOffset );
    }
}

///////////////////////////////////////////////////////////////////////////
//
// MhlCbusIsr
//
// Only when MHL connection has been established. This is where we have the
// first looks on the CBUS incoming commands or returned data bytes for the
// previous outgoing command.
//
// It simply stores the event and allows application to pick up the event
// and respond at leisure.
//
// Look for interrupts on CBUS:CBUS_INTR_STATUS [0xC8:0x08]
//		7 = RSVD			(reserved)
//		6 = MSC_RESP_ABORT	(interested)
//		5 = MSC_REQ_ABORT	(interested)	
//		4 = MSC_REQ_DONE	(interested)
//		3 = MSC_MSG_RCVD	(interested)
//		2 = DDC_ABORT		(interested)
//		1 = RSVD			(reserved)
//		0 = rsvd			(reserved)
///////////////////////////////////////////////////////////////////////////
static void MhlCbusIsr( void )
{
	uint8_t		cbusInt;
	uint8_t     gotData[4];	// Max four status and int registers.
	uint8_t		i;
	uint8_t		reg71 = I2C_ReadByte(PAGE_0_0X72, 0x71);

	//
	// Main CBUS interrupts on CBUS_INTR_STATUS
	//
	cbusInt = ReadByteCBUS(0x08);

	// When I2C is inoperational (say in D3) and a previous interrupt brought us here, do nothing.
	if(cbusInt == 0xFF)
	{
	   return;
	}
	if( cbusInt )
	{
		//
		// Clear all interrupts that were raised even if we did not process
		//
		WriteByteCBUS(0x08, cbusInt);

	       TX_DEBUG_PRINT(("Drv: Clear CBUS INTR_1: %02X\n", (int) cbusInt));
	}
	// Look for DDC_ABORT
	if (cbusInt & BIT2)
	{
		ApplyDdcAbortSafety();
	}
	// MSC_MSG (RCP/RAP)
	if((cbusInt & BIT3))
	{
            uint8_t mscMsg[2];
	    TX_DEBUG_PRINT(("Drv: MSC_MSG Received\n"));
	    //
	    // Two bytes arrive at registers 0x18 and 0x19
	    //
            mscMsg[0] = ReadByteCBUS( 0x18 );
            mscMsg[1] = ReadByteCBUS( 0x19 );
	    if (MHL_MSC_MSG_RAP == mscMsg[0])
	    {
		if (MHL_RAP_CONTENT_ON == mscMsg[1])
		{
			contentOn = 1;
	        }
		else if (MHL_RAP_CONTENT_OFF == mscMsg[1])
	        {
			contentOn = 0;
		}
	    }
	    TX_DEBUG_PRINT(("Drv: MSC MSG: %02X %02X\n", (int)mscMsg[0], (int)mscMsg[1] ));
	    SiiMhlTxGotMhlMscMsg( mscMsg[0], mscMsg[1] );
	}
	if((cbusInt & BIT5) || (cbusInt & BIT6))	// MSC_REQ_ABORT or MSC_RESP_ABORT
	{
		gotData[0] = CBusProcessErrors(cbusInt);
	}
	// MSC_REQ_DONE received.
	if(cbusInt & BIT4)
	{
	    TX_DEBUG_PRINT(("Drv: MSC_REQ_DONE\n"));

	    mscCmdInProgress = false;
            // only do this after cBusInt interrupts are cleared above
	    SiiMhlTxMscCommandDone( ReadByteCBUS( 0x16 ) );
	}

        if (BIT7 & cbusInt)
        {
            #define CBUS_LINK_STATUS_2 0x38
	    TX_DEBUG_PRINT(("Drv: Clearing CBUS_link_hard_err_count\n"));
            // reset the CBUS_link_hard_err_count field
            WriteByteCBUS(CBUS_LINK_STATUS_2,(uint8_t)(ReadByteCBUS(CBUS_LINK_STATUS_2) & 0xF0));
        }
	//
	// Now look for interrupts on register 0x1E. CBUS_MSC_INT2
	// 7:4 = Reserved
	//   3 = msc_mr_write_state = We got a WRITE_STAT
	//   2 = msc_mr_set_int. We got a SET_INT
	//   1 = reserved
	//   0 = msc_mr_write_burst. We received WRITE_BURST
	//
	cbusInt = ReadByteCBUS(0x1E);
	if( cbusInt )
	{
		//
		// Clear all interrupts that were raised even if we did not process
		//
		WriteByteCBUS(0x1E, cbusInt);

	    TX_DEBUG_PRINT(("Drv: Clear CBUS INTR_2: %02X\n", (int) cbusInt));
	}
        if ( BIT0 & cbusInt)
	{
           // WRITE_BURST complete
           SiiMhlTxMscWriteBurstDone( cbusInt );
        }
	if (cbusInt & BIT2)
	{
           uint8_t intr[4];
           uint8_t address;

	    TX_DEBUG_PRINT(("Drv: MHL INTR Received\n"));
   		for(i = 0,address=0xA0; i < 4; ++i,++address)
		{
			// Clear all, recording as we go
            intr[i] = ReadByteCBUS( address );
			WriteByteCBUS( address, intr[i] );
		}
		// We are interested only in first two bytes.
		SiiMhlTxGotMhlIntr( intr[0], intr[1] );

	}
	if ((cbusInt & BIT3))///||HalTimerExpired(TIMER_SWWA_WRITE_STAT))
	{
           uint8_t status[4];
           uint8_t address;

           for (i = 0,address=0xB0; i < 4;++i,++address)
          {
			// Clear all, recording as we go
                     status[i] = ReadByteCBUS( address );
			WriteByteCBUS( address , 0xFF /* future status[i] */ );
          }
          linkMode = status[1];
	   SiiMhlTxGotMhlStatus( status[0], status[1] );
          ///HalTimerSet(TIMER_SWWA_WRITE_STAT,50);
	}
	if(reg71)
	{
	       TX_DEBUG_PRINT(("Drv: INTR_1 @72:71 = %02X enable @72:75 = %02X\n", (int) reg71,(int)I2C_ReadByte(PAGE_0_0X72, 0x75) ));
		// Clear MDI_HPD interrupt
		I2C_WriteByte(PAGE_0_0X72, 0x71, INTR_1_DESIRED_MASK);
	}
	//
	// Check if a SET_HPD came from the downstream device.
	//
	cbusInt = ReadByteCBUS(0x0D);

	// CBUS_HPD status bit
	if( BIT6 & (dsHpdStatus ^ cbusInt))
	{
             uint8_t status = cbusInt & BIT6;
	      TX_DEBUG_PRINT(("Drv: Downstream HPD changed to: %02X\n", (int) cbusInt));

       	// Inform upper layer of change in Downstream HPD
       	SiiMhlTxNotifyDsHpdChange( status);
              if (status)
              {
                   SiiMhlTxDrvReleaseUpstreamHPDControl();  // this triggers an EDID read if control has not yet been released
               }

       	// Remember
       	dsHpdStatus = cbusInt;
	}
}

///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxDrvPowBitChange
//
// This function or macro is invoked from MhlTx component to
// control the VBUS power. If powerOn is sent as non-zero, one should assume
// peer does not need power so quickly remove VBUS power.
//
// if value of "powerOn" is 0, then this routine must turn the VBUS power on
// within 50ms of this call to meet MHL specs timing.
//
void	SiiMhlTxDrvPowBitChange( bool_t powerOn )
{
	if( powerOn )
	{
		TX_DEBUG_PRINT(("Drv: Peer's POW bit is set. Turn the VBUS power OFF here.\n"));
	}
	else
	{
		TX_DEBUG_PRINT(("Drv: Peer's POW bit is cleared. Turn the VBUS power ON here.\n"));
	}
}

/*
	SiMhlTxDrvSetClkMode
	-- Set the hardware this this clock mode.
 */
void SiMhlTxDrvSetClkMode(uint8_t clkMode)
{
	TX_DEBUG_PRINT(("SiMhlTxDrvSetClkMode:0x%02x\n",(int)clkMode));
	// nothing to do here since we only suport MHL_STATUS_CLK_MODE_NORMAL
	// if we supported SUPP_PPIXEL, this would be the place to write the register
}

/*
    SiiMhlTxDeviceTimerIsr

    This routine is called, every time a timer expires, by
        the macro CALL_SII_MHL_TX_DEVICE_TIMER_ISR(index)
        in si_drvisrcfg.h.

*/

void SiiMhlTxDeviceTimerIsr(uint8_t timerIndex)
{
	switch(timerIndex)
	{
	case TIMER_TO_DO_RSEN_CHK :
	case TIMER_TO_DO_RSEN_DEGLITCH:
    case TIMER_SWWA_WRITE_STAT:
		 ///SiiOsBumpMhlTxScheduler();
		 break;
	default:
		;
	}
}

//================================================================
#define	APP_DEMO_RCP_SEND_KEY_CODE 0x44  /* play */

int16_t rcpKeyCode=-1;
int16_t rcpkParam = -1;
// Luke Key dispatch for MHL log test
unsigned char mhl_caButton = 0;

void  AppNotifyMhlDownStreamHPDStatusChange(bool_t connected)
{
    // this need only be a placeholder for 9244
    ;//SiiOsDebugPrintSimple(SII_OSAL_DEBUG_TRACE_ALWAYS,"App:%d AppNotifyMhlDownStreamHPDStatusChange connected:%s\n",(int)__LINE__,connected?"yes":"no");
}

#if defined CONFIG_KEYBOARD_PMIC8058
extern struct input_dev *tskpdev;
#endif

extern void s_vKeyArray(int iKeyId);
void Arrow_Key_HDMI(int iKeyId);

void Arrow_Key_HDMI(int iKeyId)
{
#if defined CONFIG_KEYBOARD_PMIC8058

	    if (tskpdev == NULL) 
	    {
		   printk(KERN_ERR "[%s] tskpdev == NULL\n", __FUNCTION__);
		   return;
	    }
            // Luke Key dispatch for MHL log test
	    if (iKeyId == -1)    
	    {
	        if( mhl_caButton )   
	        {
	            iKeyId = mhl_caButton;
		    printk(KERN_INFO "[%s] Key release (%#x) is <SELECT>\n", __FUNCTION__, (unsigned char)mhl_caButton);
		    input_report_key(tskpdev, mhl_caButton, 0);
		    input_sync(tskpdev);
	            mhl_caButton = 0;
	        }
	        return;
	    }

		if (iKeyId == 0x01) 
		{
			printk(KERN_INFO "[%s] Key(%#x) is <SELECT>\n", __FUNCTION__, (char)iKeyId);
			input_report_key(tskpdev, KEY_ENTER, 1);
			input_sync(tskpdev);
			
			mhl_caButton = KEY_ENTER;
		} 
		else if (iKeyId == 0x02) 
		{
			printk(KERN_INFO "[%s] Key(%#x) is <UP>\n", __FUNCTION__, (char)iKeyId);
			input_report_key(tskpdev, KEY_UP, 1);
			input_sync(tskpdev);
			
			mhl_caButton = KEY_UP;
		} 
		else if (iKeyId == 0x03) 
		{
			printk(KERN_INFO "[%s] Key(%#x) is <DOWN>\n", __FUNCTION__, (char)iKeyId);
			input_report_key(tskpdev, KEY_DOWN, 1);
			input_sync(tskpdev);
			mhl_caButton = KEY_DOWN;
        } 
        	else if (iKeyId == 0x04) 
        {
			printk(KERN_INFO "[%s] Key(%#x) is <LEFT>\n", __FUNCTION__, (char)iKeyId);
			input_report_key(tskpdev, KEY_LEFT, 1);
			input_sync(tskpdev);
			
			mhl_caButton = KEY_LEFT;
		}
        	else if (iKeyId == 0x05) 
        {
			printk(KERN_INFO "[%s] Key(%#x) is <RIGHT>\n", __FUNCTION__, (char)iKeyId);
			input_report_key(tskpdev, KEY_RIGHT, 1);
			input_sync(tskpdev);
			
			mhl_caButton = KEY_RIGHT;
		}
#endif
}

#define  MHL_RCP_CMD_SELECT        0x00
#define  MHL_RCP_CMD_UP            0x01
#define  MHL_RCP_CMD_DOWN          0x02
#define  MHL_RCP_CMD_LEFT          0x03
#define  MHL_RCP_CMD_RIGHT         0x04
#define  MHL_RCP_CMD_ROOT_MENU     0x09
#define  MHL_RCP_CMD_EXIT          0x0D

static unsigned char CpProcessRcpMessage (unsigned char rcpData )
{
	TX_DEBUG_PRINT(( "CpProcessRcpMessage: %x \n", rcpData ));

    switch ( rcpData )
    {
        case MHL_RCP_CMD_SELECT:
                         Arrow_Key_HDMI(0x01);
                         mdelay(50);
                         Arrow_Key_HDMI(-1);
			 break;
        case MHL_RCP_CMD_UP:
                         // Luke Key dispatch for MHL log test
                         Arrow_Key_HDMI(0x02);
                         mdelay(50);
                         Arrow_Key_HDMI(-1);
			 break;
        case MHL_RCP_CMD_DOWN:
                         Arrow_Key_HDMI(0x03);
                         mdelay(50);
                         Arrow_Key_HDMI(-1);
			 break;
        case MHL_RCP_CMD_LEFT:
                         Arrow_Key_HDMI(0x04);
                         mdelay(50);
                         Arrow_Key_HDMI(-1);
			 break;
        case MHL_RCP_CMD_RIGHT:
                         Arrow_Key_HDMI(0x05);
                         mdelay(50);
                         Arrow_Key_HDMI(-1);
			 break;
        case MHL_RCP_CMD_ROOT_MENU:
			             s_vKeyArray( 0x04 ); // Home key
                         mdelay(50);
                         s_vKeyArray(-1);
			 break;
        case MHL_RCP_CMD_EXIT:
			             s_vKeyArray( 0x02 ); // Back key
                         mdelay(50);
                         s_vKeyArray(-1);
			 break;
	}
	return 1;
}

MhlTxNotifyEventsStatus_e AppNotifyMhlEvent(uint8_t eventCode, uint8_t eventParam)
{
    unsigned char rcpData; 
	MhlTxNotifyEventsStatus_e retVal = MHL_TX_EVENT_STATUS_PASSTHROUGH;
	
	TX_DEBUG_PRINT(( "AppNotifyMhlEvent: %x, %x \n", (int)eventCode, (int)eventParam ));
	
	switch(eventCode)
	{
	case MHL_TX_EVENT_DISCONNECTION:
		;//SiiOsDebugPrintSimple(SII_OSAL_DEBUG_TRACE_ALWAYS, "App: Got event = MHL_TX_EVENT_DISCONNECTION\n");
		break;
	case MHL_TX_EVENT_CONNECTION:
		;//SiiOsDebugPrintSimple(SII_OSAL_DEBUG_TRACE_ALWAYS, "App: Got event = MHL_TX_EVENT_CONNECTION\n");
    		SiiMhlTxSetPreferredPixelFormat(MHL_STATUS_CLK_MODE_NORMAL);

		break;
	case MHL_TX_EVENT_RCP_READY:
		if( (0 == (MHL_FEATURE_RCP_SUPPORT & eventParam)) )
		{
			;//SiiOsDebugPrintSimple(SII_OSAL_DEBUG_TRACE_ALWAYS, "App:%d Peer does NOT support RCP\n",(int)__LINE__ );
		}
              else
              {
			;//SiiOsDebugPrintSimple(SII_OSAL_DEBUG_TRACE_ALWAYS, "App:%d Peer supports RCP\n",(int)__LINE__ );
                     // Demo RCP key code Volume Up
                     rcpKeyCode = APP_DEMO_RCP_SEND_KEY_CODE;

              }
		if( (0 == (MHL_FEATURE_RAP_SUPPORT & eventParam)) )
		{
			;//SiiOsDebugPrintSimple(SII_OSAL_DEBUG_TRACE_ALWAYS, "App:%d Peer does NOT support RAP\n",(int)__LINE__ );
		}
              else
              {
			;//SiiOsDebugPrintSimple(SII_OSAL_DEBUG_TRACE_ALWAYS, "App:%d Peer supports RAP\n",(int)__LINE__ );
              }
		if( (0 == (MHL_FEATURE_SP_SUPPORT & eventParam)) )
		{
			;//SiiOsDebugPrintSimple(SII_OSAL_DEBUG_TRACE_ALWAYS, "App:%d Peer does NOT support WRITE_BURST\n",(int)__LINE__ );
		}
              else
		{
			;//SiiOsDebugPrintSimple(SII_OSAL_DEBUG_TRACE_ALWAYS, "App:%d Peer supports WRITE_BURST\n",(int)__LINE__ );
		}

		break;
	case MHL_TX_EVENT_RCP_RECEIVED :
        	//
        	// Check if we got an RCP. Application can perform the operation here
        	// and send RCPK or RCPE. For now, we send the RCPK
        	//
              ;//SiiOsDebugPrintSimple(SII_OSAL_DEBUG_TRACE_ALWAYS, "App: Received an RCP key code = %02X\n", (int)eventParam );
                rcpData = (unsigned char)(eventParam & 0x0FF);
                CpProcessRcpMessage( rcpData );
		rcpkParam = (int16_t) eventParam;
		break;
	case MHL_TX_EVENT_RCPK_RECEIVED:
		;//SiiOsDebugPrintSimple(SII_OSAL_DEBUG_TRACE_ALWAYS, "App: Received an RCPK = %02X\n", (int)eventParam);
		break;
	case MHL_TX_EVENT_RCPE_RECEIVED:
		;//SiiOsDebugPrintSimple(SII_OSAL_DEBUG_TRACE_ALWAYS, "App: Received an RCPE = %02X\n", (int)eventParam);
		break;
	case MHL_TX_EVENT_DCAP_CHG:
        	{
        	uint8_t i,myData;
    		;//SiiOsDebugPrintSimple(SII_OSAL_DEBUG_TRACE_ALWAYS, "App: MHL_TX_EVENT_DCAP_CHG: ",myData);
        	for(i=0;i<16;++i)
        	{
    			if (0 == SiiTxGetPeerDevCapEntry(i,&myData))
    			{
    				;//SiiOsDebugPrintSimple(SII_OSAL_DEBUG_TRACE_ALWAYS, "0x%02x ",(int)myData);
    			}
    			else
    			{
    				;//SiiOsDebugPrintSimple(SII_OSAL_DEBUG_TRACE_ALWAYS, "busy ");
    			}
        	}
    		;//SiiOsDebugPrintSimple(SII_OSAL_DEBUG_TRACE_ALWAYS, "\n");
        	}
		break;
	case MHL_TX_EVENT_DSCR_CHG:
		{
			ScratchPadStatus_e temp;
			uint8_t myData[16];
			temp = SiiGetScratchPadVector(0,sizeof(myData), myData);
			switch(temp)
			{
			case SCRATCHPAD_FAIL:
			case SCRATCHPAD_NOT_SUPPORTED:
			case SCRATCHPAD_BUSY:
	              case SCRATCHPAD_BAD_PARAM:
				
			    	;//SiiOsDebugPrintSimple(SII_OSAL_DEBUG_TRACE_ALWAYS, "SiiGetScratchPadVector returned 0x%02x\n",(int)temp);
				break;
			case SCRATCHPAD_SUCCESS:
				{
					uint8_t i;
			    		;//SiiOsDebugPrintSimple(SII_OSAL_DEBUG_TRACE_ALWAYS, "New ScratchPad: ",(int)temp);
					for (i=0;i<sizeof(myData);++i)
					{
					    ;//SiiOsDebugPrintSimple(SII_OSAL_DEBUG_TRACE_ALWAYS, "(%02x, %c) \n",(int)temp,(char)temp);
					}
			    		;//SiiOsDebugPrintSimple(SII_OSAL_DEBUG_TRACE_ALWAYS, "\n",(int)temp);
				}
				break;
			}
		}
		break;

	default:
		;//SiiOsDebugPrintSimple(SII_OSAL_DEBUG_TRACE_ALWAYS, "Unknown event: 0x%02x\n",(int)eventCode);

	}
	return retVal;
}




#if 0	     	

///////////////////////////////////////////////////////////////////////////////
//
// AppRcpDemo
//
// This function is supposed to provide a demo code to elicit how to call RCP
// API function.
//
#define	APP_DEMO_RCP_SEND_KEY_CODE 0x41

void	AppRcpDemo( uint8_t event, uint8_t eventParameter)
{
	uint8_t		rcpKeyCode;

//	printf("App: Got event = %02X, eventParameter = %02X\n", (int)event, (int)eventParameter);

	switch( event )
	{
		case	MHL_TX_EVENT_DISCONNECTION:
			printk(KERN_INFO "App: Got event = MHL_TX_EVENT_DISCONNECTION\n");
			break;

		case	MHL_TX_EVENT_CONNECTION:
			printk(KERN_INFO "App: Got event = MHL_TX_EVENT_CONNECTION\n");
			break;

		case	MHL_TX_EVENT_RCP_READY:

			// Demo RCP key code Volume Up
			rcpKeyCode = APP_DEMO_RCP_SEND_KEY_CODE;

			printk(KERN_INFO "App: Got event = MHL_TX_EVENT_RCP_READY...Sending RCP (%02X)\n", (int) rcpKeyCode);


			if( (0 == (MHL_FEATURE_RCP_SUPPORT & eventParameter)) )
			{
				printk(KERN_INFO  "App: Peer does NOT support RCP\n" );
			}
			if( (0 == (MHL_FEATURE_RAP_SUPPORT & eventParameter)) )
			{
				printk(KERN_INFO  "App: Peer does NOT support RAP\n" );
			}
			if( (0 == (MHL_FEATURE_SP_SUPPORT & eventParameter)) )
			{
				printk(KERN_INFO  "App: Peer does NOT support WRITE_BURST\n" );
			}


			//
			// If RCP engine is ready, send one code
			//
			if( SiiMhlTxRcpSend( rcpKeyCode ))
			{
				printk(KERN_INFO "App: SiiMhlTxRcpSend (%02X)\n", (int) rcpKeyCode);
			}
			else
			{
				printk(KERN_INFO "App: SiiMhlTxRcpSend (%02X) Returned Failure.\n", (int) rcpKeyCode);
			}
			break;

		case	MHL_TX_EVENT_RCP_RECEIVED:
			//
			// Check if we got an RCP. Application can perform the operation here
			// and send RCPK or RCPE. For now, we send the RCPK
			//
			printk(KERN_INFO "App: Received an RCP key code = %02X\n", eventParameter );
                     rcpKeyCode = eventParameter;
			SiiMhlTxRcpkSend( (int)rcpKeyCode );
			break;

		case	MHL_TX_EVENT_RCPK_RECEIVED:
			printk(KERN_INFO "App: Received an RCPK = %02X\n", (int)eventParameter);
			break;

		case	MHL_TX_EVENT_RCPE_RECEIVED:
			printk(KERN_INFO "App: Received an RCPE = %02X\n", (int)eventParameter);
			break;

		default:
			break;
	}
}

///////////////////////////////////////////////////////////////////////////////
//
// AppVbusControl
//
// This function or macro is invoked from MhlTx driver to ask application to
// control the VBUS power. If powerOn is sent as non-zero, one should assume
// peer does not need power so quickly remove VBUS power.
//
// if value of "powerOn" is 0, then application must turn the VBUS power on
// within 50ms of this call to meet MHL specs timing.
//
// Application module must provide this function.
//
void	AppVbusControl( bool_t powerOn )
{
	if( powerOn )
	{
		printk(KERN_INFO "App: Peer's POW bit is set. Turn the VBUS power OFF here.\n");
	}
	else
	{
		printk(KERN_INFO "App: Peer's POW bit is cleared. Turn the VBUS power ON here.\n");
	}
}

#endif

//===================================================================================================
void Sil_USB_ID_Int_Handle( int GPIO_Value )
{
       if( GPIO_Value == 0 )     Sil_USB_MHL_Switch( 1 ); //USB ID pin is low
}
EXPORT_SYMBOL(Sil_USB_ID_Int_Handle);

void Sil_USB_MHL_Switch( char Side )
{
       printk(KERN_INFO "%s, %d\n", __func__, Side  );
	   
       // Luke fix MHL crash with OTG turn on
       if( dd == NULL )    return;

       if( Side )
       {
            dd->pd->mhl_select( Side );
            queue_delayed_work( mhl_private_wq, &sii_switch_to_mhl_work, msecs_to_jiffies(T_SWITCH_TO_MHL) );
       }
       else
       {
            dd->pd->mhl_select( Side );
       }
}

//===================================================================================================

static void sii9244_handle_cable_work(struct work_struct *work)
{
        ///uint8_t	event;
        ///uint8_t	eventParameter;
        // BugID 625, Luke 2011.8.10, MHL chip enter sleep mode
	if(mhl_cable_connected == true) 
	{
	       SiiMhlTxDeviceIsr();

		//B: Robert, 20111117, QCT8x60_CR1392 : Fix Moto charger (USB_ID low) issue
		if(skip_MHL_switch)
		{
			mhl_cable_connected = false;
			if( dd ) dd->pd->mhl_select(0);
		}
		//E: Robert, 20111117, QCT8x60_CR1392

		if (rcpKeyCode >= 0)
		{
                 if (!MhlTxCBusBusy())
                 {

                 	;//SiiOsDebugPrintSimple(SII_OSAL_DEBUG_TRACE_ALWAYS,"App:%d Sending RCP (%02X)\n",(int)__LINE__, (int) rcpKeyCode);
                 	//
                 	// If RCP engine is ready, send one code
                 	//
                 	if( SiiMhlTxRcpSend( (uint8_t)rcpKeyCode ))
                 	{
                 		;//SiiOsDebugPrintSimple(SII_OSAL_DEBUG_TRACE_ALWAYS,"App:%d SiiMhlTxRcpSend (%02X)\n",(int)__LINE__, (int) rcpKeyCode);
                 	}
                 	else
                 	{
                 		;//SiiOsDebugPrintSimple(SII_OSAL_DEBUG_TRACE_ALWAYS,"App:%d SiiMhlTxRcpSend (%02X) Returned Failure.\n",(int)__LINE__, (int) rcpKeyCode);
                 	}
                 	rcpKeyCode = -1;
                 }
		}
		
		if (rcpkParam >= 0)
		{
		        SiiMhlTxRcpkSend( (uint8_t)rcpkParam );
		        rcpkParam = -1;
		}
	   
#if 0	     	
	   SiiMhlTxGetEvents( &event, &eventParameter );

	   if( MHL_TX_EVENT_NONE != event )
	   {
	      AppRcpDemo( event, eventParameter);
	   }
#endif	   
	}


    if(mhl_cable_connected == true)
    {  
       if( POWER_STATE_D3 == fwPowerState )   queue_delayed_work( mhl_private_wq, &cable_work, msecs_to_jiffies(T_D3_POLL_TIME) );
       else                                   queue_delayed_work( mhl_private_wq, &cable_work, msecs_to_jiffies(T_D0_POLL_TIME));
    }
}

static void sii_rsen_check_handler(struct work_struct *work)
{
    printk(" %s  \n", __func__);
    if( POWER_STATE_D0_MHL == fwPowerState )     rsen_check_flag = true;
}
	
static void sii_rsen_deglitch_handler(struct work_struct *work)
{
    printk(" %s  \n", __func__);
    if( POWER_STATE_D0_MHL == fwPowerState )	rsen_deglitch_flag = true;
}

static void sii_switch_to_usb_handler(struct work_struct *work)
{
    printk(" %s: Switch to USB  \n", __func__);
    Sil_USB_MHL_Switch( 0 );
}

static void sii_switch_to_mhl_handler(struct work_struct *work)
{
    printk(" %s set cable connect flag true\n", __func__);
    mhl_cable_connected = true;
    queue_delayed_work( mhl_private_wq, &cable_work, msecs_to_jiffies(100) );
}

// Bug 909, Luke Implement MHL charging function
static void mhl_charging_handler(struct work_struct *work)
{
    printk(" %s  \n", __func__);

    if( cci_usb_power_plugin )
    {
        msm_charger_vbus_draw( 500 );
        printk(" %s  : msm_charger_vbus_draw 500\n", __func__);
    }
    // else           smb137b_otg_power(1); 
}

#if 0 //B: Robert, 20111016, QCT8x60_CR1058 : Rollback pmic_id_notif_init related handling
static void sii9244_usb_id_handler(struct work_struct *work)
{
    printk(" %s  \n", __func__);
	
    Sil_USB_MHL_Switch(1);
}
#endif //E: Robert, 20111016, QCT8x60_CR1058

#if 0
static irqreturn_t sii9244_irq(int irq, void *dev_id)
{
    printk(" %s  \n", __func__);


    queue_delayed_work(mhl_private_wq, &cable_work, msecs_to_jiffies(10));
	
    return IRQ_HANDLED;
}
#endif

#if 0 //B: Robert, 20111016, QCT8x60_CR1058 : Rollback pmic_id_notif_init related handling
static irqreturn_t sii9244_usb_id_irq(int irq, void *dev_id)
{
    printk(" %s  \n", __func__);


    queue_delayed_work(mhl_private_wq, (&usb_id_work, msecs_to_jiffies(1)) ;
	
    return IRQ_HANDLED;
}
#endif //E: Robert, 20111016, QCT8x60_CR1058

#if 0
void sii9244_mhl_selct(int select )
{
       printk(KERN_INFO "%s, %d\n", __func__, select  );
	   
       // Luke fix MHL crash with OTG turn on
       if( dd == NULL )    return;
	   
       
       if( select )
       {
       dd->pd->mhl_select( select );
            queue_delayed_work( mhl_private_wq, &sii_switch_to_mhl_work, msecs_to_jiffies(T_SWITCH_TO_MHL) );
            //mhl_cable_connected  = true;
       }
       else
       {
            //mhl_cable_connected  = false; 
            dd->pd->mhl_select( select );
       }
}
EXPORT_SYMBOL(sii9244_mhl_selct);
#endif

// [Da80] Luke, 2011,0805  
// MILBAIDU-641, When bootup device without MHL cable connected, 
// It will indicate HDMI cable connected on status bar.
uint8_t sii9244_D3_status(void )
{
       printk(KERN_INFO "%s, %d\n", __func__, fwPowerState );
	   
       return fwPowerState;
}
EXPORT_SYMBOL(sii9244_D3_status);

struct early_suspend sil_early_suspend;

void sil_suspend(struct early_suspend *h)
{
   // Disable TMDS
   printk(KERN_INFO "%s, %d\n", __func__, fwPowerState );
   
   sil_suspend_flag = true;
   
   if( POWER_STATE_D0_MHL == fwPowerState )    
   {
     SiiMhlTxDrvTmdsControl( false );
     I2C_WriteByte(PAGE_2_0x92, 0x01, 0x03);
   }
}

void sil_resume(struct early_suspend *h)
{
   // Enable TMDS
   printk(KERN_INFO "%s, %d\n", __func__, fwPowerState );
   
   sil_suspend_flag = false;
   
   if( POWER_STATE_D0_MHL == fwPowerState )   
   {
      SiiMhlTxDrvTmdsControl( true );
      I2C_WriteByte(PAGE_2_0x92, 0x01, 0x00);
   }
}

/*------------------------------------------------------------------------------
 Function: sil9224_probe
 Description: probe driver
-------------------------------------------------------------------------------*/

static int __devinit
sil9244_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int rc;

        // Luke 2011.0725 [Bug 523] Enable/Disable MHL with HW ID
        printk(KERN_INFO "%s probe!!\n", __func__);

	dd = kzalloc(sizeof *dd, GFP_KERNEL);
	if (!dd) {
		rc = -ENOMEM;
		goto probe_exit;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		return -ENODEV;

	/// hdmi_common_state->dev = &client->dev;

	/* init real i2c_client */
	//hclient = client;
	sii9224_I2C_SetClient( client );

	i2c_set_clientdata(client, dd);
	
	dd->pd = client->dev.platform_data;
	if (!dd->pd) {
		rc = -ENODEV;
		goto probe_free;
	}

	rc = dd->pd->chip_init(); //perform the sii9244_chip_init()
	
	if (rc) {
		printk(KERN_ERR "%s: chip_init failed: %d\n", __func__, rc);
		goto probe_free;
	}

	mhl_private_wq = create_rt_workqueue( "mhl_private" );

        //hrtimer_init(&WaveTimer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
        //WaveTimer.function = WaveTimerFunc;

	INIT_DELAYED_WORK(&cable_work, sii9244_handle_cable_work);
    //INIT_DELAYED_WORK(&sii_9244_hpd_timer, TimerTickHandler);
        #if 0 //B: Robert, 20111016, QCT8x60_CR1058 : Rollback pmic_id_notif_init related handling
        INIT_DELAYED_WORK(&usb_id_work, sii9244_usb_id_handler);
        #endif //E: Robert, 20111016, QCT8x60_CR1058


        // Bug 909, Luke Implement MHL charging function
        INIT_DELAYED_WORK(&mhl_charging_work, mhl_charging_handler);
	  
    INIT_DELAYED_WORK(&sii_rsen_check_work, sii_rsen_check_handler);
    INIT_DELAYED_WORK(&sii_rsen_deglitch_work, sii_rsen_deglitch_handler);
    INIT_DELAYED_WORK(&sii_switch_to_mhl_work, sii_switch_to_mhl_handler);
    INIT_DELAYED_WORK(&sii_switch_to_usb_work, sii_switch_to_usb_handler);
    
	/*rc = request_irq( dd->pd->irq_sii_int, &sii9244_irq, IRQF_TRIGGER_FALLING, "sii9244_interrupt", dd) ;
	if (rc<0) {
		printk(KERN_ERR "%s: request_sii_irq: %d\n", __func__, rc);
		goto probe_free;
	}*/
#if 0 //B: Robert, 20111016, QCT8x60_CR1058 : Rollback pmic_id_notif_init related handling
	rc = request_threaded_irq( dd->pd->irq_usb_id, NULL, &sii9244_usb_id_irq, IRQF_TRIGGER_FALLING, "usb_id_interrupt", NULL);//dd) ;
	if (rc<0) {
		printk(KERN_ERR "%s: request_usb_id_irq: %d\n", __func__, rc);
		goto probe_free;
	}
#endif //E: Robert, 20111016, QCT8x60_CR1058
	HalTimerInit();
        HalTimerSet( TIMER_POLLING, MONITORING_PERIOD );
	   
        if( SiiMhlTxInitialize( MONITORING_PERIOD ) == false ) 
	{
		printk(KERN_ERR "%s: chip_id failed! \n", __func__);
                rc = -ENOMEM;
		goto probe_free;
	}
   
	sil_early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	sil_early_suspend.suspend = sil_suspend;
	sil_early_suspend.resume = sil_resume;
	register_early_suspend(&sil_early_suspend);


	//queue_delayed_work( mhl_private_wq, &cable_work, msecs_to_jiffies(1000) );
	//queue_delayed_work( mhl_private_wq, &sii_9244_hpd_timer, msecs_to_jiffies(1000) );
   
	return 0;

probe_free:
	i2c_set_clientdata(client, NULL);
	kfree(dd);
        dd = NULL;
probe_exit:
	return rc;

}

/*------------------------------------------------------------------------------
 Function: sil9224_remove
 Description:revise the remove function.
-------------------------------------------------------------------------------*/
static int __devexit sil9244_remove(struct i2c_client *client)
{
	int err = 0;
	if (!client->adapter) {
		pr_err("%s: No MHL Device\n", __func__); 
		return -ENODEV;
	}
	return err;
}

/* Chagnhan, revise the i2c_driver context. */
static struct i2c_driver hdmi_sil9244_i2c_driver = {
	.driver		= {
		.name   = SIL9244_DRV_NAME,
	},
	.probe		= sil9244_probe,
	.id_table	       = sil9244_id,
	.remove		= __devexit_p(sil9244_remove),
};

/*------------------------------------------------------------------------------
 Function: sil9224_init
 Description: init fuction
-------------------------------------------------------------------------------*/
static int __init sil9244_init(void)
{
	int rc;

	printk(KERN_INFO "%s\n", __func__);

	rc = i2c_add_driver(&hdmi_sil9244_i2c_driver);
	if (rc) {
		pr_err("hdmi_init FAILED: i2c_add_driver rc=%d\n", rc);
		goto init_exit;
	}

	return 0;

init_exit:
	return rc;
}

/*------------------------------------------------------------------------------
 Function: sii9224_exit
 Description: driver exit
-------------------------------------------------------------------------------*/
static void __exit sil9244_exit(void)
{
	i2c_del_driver(&hdmi_sil9244_i2c_driver);
        // Luke fix MHL crash with OTG turn on
        dd = NULL;
    if( mhl_private_wq )  
    {  
        destroy_workqueue(mhl_private_wq);
        mhl_private_wq = NULL;
    }
}

late_initcall(sil9244_init);
module_exit(sil9244_exit);
MODULE_LICENSE("GPL v2");
MODULE_VERSION("0.1");
MODULE_AUTHOR("Qualcomm Innovation Center, Inc.");
MODULE_DESCRIPTION("SIL9244 MHL driver");

