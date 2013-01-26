/***********************************************************************************/
/*  Copyright (c) 2002-2009, Silicon Image, Inc.  All rights reserved.             */
/*  No part of this work may be reproduced, modified, distributed, transmitted,    */
/*  transcribed, or translated into any language or computer format, in any form   */
/*  or by any means without written permission of: Silicon Image, Inc.,            */
/*  1060 East Arques Avenue, Sunnyvale, California 94085                           */
/***********************************************************************************/

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/time.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/slab.h>

#include "defs.h"
#include "sii_reg_access.h"
#include "si_datatypes.h"


static struct i2c_client *hclient;

void sii9224_I2C_SetClient( void *hclient_ )
{
     hclient = (struct i2c_client *)hclient_;	
}

uint8_t sii9224_I2C_ReadByte(struct i2c_client *client, uint8_t addr, uint8_t reg)
{
	int err;
	struct i2c_msg msg[2];
	u8 reg_buf[2] = { 0 };
	u8 data_buf[2] = { 0 };

	if (!client->adapter)
		return -ENODEV;

	msg[0].addr = addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = reg_buf;

	msg[1].addr = addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = data_buf;
	
       reg_buf[0]= reg ;
	   
	//printk(KERN_INFO "Drv: %s: I2C read: %02X, %02X\n", __func__,  addr<<1, reg);//CH remove logs
	err = i2c_transfer(client->adapter, msg, 2);

	if (err < 0) {
		printk(KERN_INFO "Drv: %s: I2C err: %d\n", __func__, err ); 
		return 0xFF;//err;
	}

	return *data_buf;

}

static int sii9224_write_byte(struct i2c_client *client, u8 addr, u8 reg, u8 val)
{
	int err;
	struct i2c_msg msg[1];
	unsigned char data[2];
 
	if (!client->adapter)
		return -ENODEV;

	msg->addr = addr;
	msg->flags = 0;
	msg->len = 2;
	msg->buf = data;
	data[0] = reg;
	data[1] = val;

       //printk(KERN_INFO "Drv: %s: I2C write: %02X, %02X, %02X\n", __func__,  addr<<1, reg, val);  //CH Remove logs
	err = i2c_transfer(client->adapter, msg, 1);
	if (err >= 0)
		return 0;
	
	printk(KERN_INFO "Drv: %s: I2C err: %d\n", __func__, err ); 

	return err;
}

uint8_t I2C_ReadByte(uint8_t deviceID, uint8_t offset)
{
    return sii9224_I2C_ReadByte( hclient, deviceID, offset );
}

void I2C_WriteByte(uint8_t deviceID, uint8_t offset, uint8_t value)
{
     sii9224_write_byte( hclient, deviceID, offset, value );
}


//////////////////////////////////////////////////////////////////////////////
//
// FUNCTION		:	ReadBytePage0 ()
//
// PURPOSE		:	Read the value from a Page0 register.
//
// INPUT PARAMS	:	Offset	-	the offset of the Page0 register to be read.
//
// OUTPUT PARAMS:	None
//
// GLOBALS USED	:	None
//
// RETURNS		:	The value read from the Page0 register.
//
//////////////////////////////////////////////////////////////////////////////

uint8_t ReadBytePage0 (uint8_t Offset)
{
	return I2C_ReadByte(PAGE_0_0X72, Offset);
}


//////////////////////////////////////////////////////////////////////////////
//
// FUNCTION		:	WriteBytePage0 ()
//
// PURPOSE		:	Write a value to a Page0 register.
//
// INPUT PARAMS	:	Offset	-	the offset of the Page0 register to be written.
//					Data	-	the value to be written.
//
// OUTPUT PARAMS:	None
//
// GLOBALS USED	:	None
//
// RETURNS		:	void
//
//////////////////////////////////////////////////////////////////////////////

void WriteBytePage0 (uint8_t Offset, uint8_t Data)
{
	I2C_WriteByte(PAGE_0_0X72, Offset, Data);
}


//////////////////////////////////////////////////////////////////////////////
//
// FUNCTION		:	ReadModifyWritePage0 ()
//
// PURPOSE		:	Set or clear individual bits in a Page0 register.
//
// INPUT PARAMS	:	Offset	-	the offset of the Page0 register to be modified.
//					Mask	-	"1" for each Page0 register bit that needs to be
//								modified
//					Data	-	The desired value for the register bits in their
//								proper positions
//
// OUTPUT PARAMS:	None
//
// GLOBALS USED	:	None
//
// RETURNS		:	void
//
// EXAMPLE		:	If Mask of 0x0C and a 
//
//////////////////////////////////////////////////////////////////////////////

void ReadModifyWritePage0 (uint8_t Offset, uint8_t Mask, uint8_t Data)
{

	uint8_t Temp;

	Temp = ReadBytePage0(Offset);		// Read the current value of the register.
	Temp &= ~Mask;					// Clear the bits that are set in Mask.
	Temp |= (Data & Mask);			// OR in new value. Apply Mask to Value for safety.
	WriteBytePage0(Offset, Temp);		// Write new value back to register.
}


//////////////////////////////////////////////////////////////////////////////
//
// FUNCTION		:	ReadByteCBUS ()
//
// PURPOSE		:	Read the value from a CBUS register.
//
// INPUT PARAMS	:	Offset - the offset of the CBUS register to be read.
//
// OUTPUT PARAMS:	None
//
// GLOBALS USED	:	None
//
// RETURNS		:	The value read from the CBUS register.
//
//////////////////////////////////////////////////////////////////////////////

uint8_t  ReadByteCBUS (uint8_t Offset)
{
	return I2C_ReadByte(PAGE_CBUS_0XC8, Offset);
}


//////////////////////////////////////////////////////////////////////////////
//
// FUNCTION		:	WriteByteCBUS ()
//
// PURPOSE		:	Write a value to a CBUS register.
//
// INPUT PARAMS	:	Offset	-	the offset of the Page0 register to be written.
//					Data	-	the value to be written.
//
// OUTPUT PARAMS:	None
//
// GLOBALS USED	:	None
//
// RETURNS		:	void
//
//////////////////////////////////////////////////////////////////////////////

void WriteByteCBUS (uint8_t Offset, uint8_t Data)
{
	I2C_WriteByte(PAGE_CBUS_0XC8, Offset, Data);
}

//////////////////////////////////////////////////////////////////////////////
//
// FUNCTION		:	ReadModifyWriteCBUS ()
//
// PURPOSE		:	Set or clear individual bits on CBUS page.
//
// INPUT PARAMS	:	Offset	-	the offset of the CBUS register to be modified.
//					Mask	-	"1" for each CBUS register bit that needs to be
//								modified
//					Data	-	The desired value for the register bits in their
//								proper positions
//
// OUTPUT PARAMS:	None
//
// GLOBALS USED	:	None
//
// RETURNS		:	void
//
//
//////////////////////////////////////////////////////////////////////////////

void ReadModifyWriteCBUS(uint8_t Offset, uint8_t Mask, uint8_t Value)
{
    uint8_t Temp;

    Temp = ReadByteCBUS(Offset);
    Temp &= ~Mask;
	Temp |= (Value & Mask);
    WriteByteCBUS(Offset, Temp);
}

