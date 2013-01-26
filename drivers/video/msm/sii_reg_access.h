/***********************************************************************************/
/*  Copyright (c) 2002-2009, 2011 Silicon Image, Inc.  All rights reserved.        */
/*  No part of this work may be reproduced, modified, distributed, transmitted,    */
/*  transcribed, or translated into any language or computer format, in any form   */
/*  or by any means without written permission of: Silicon Image, Inc.,            */
/*  1060 East Arques Avenue, Sunnyvale, California 94085                           */
/***********************************************************************************/

// Direct register access

uint8_t		I2C_ReadByte(uint8_t deviceID, uint8_t offset);
void		I2C_WriteByte(uint8_t deviceID, uint8_t offset, uint8_t value);

uint8_t		ReadBytePage0 (uint8_t Offset);
void 		WriteBytePage0 (uint8_t Offset, uint8_t Data);
void 		ReadModifyWritePage0 (uint8_t Offset, uint8_t Mask, uint8_t Data);

uint8_t 	ReadByteCBUS (uint8_t Offset);
void 		WriteByteCBUS (uint8_t Offset, uint8_t Data);
void 		ReadModifyWriteCBUS(uint8_t Offset, uint8_t Mask, uint8_t Value);

//
// Slave addresses used in Sii 9244.
//
#define	PAGE_0_0X72			(0x72>>1)
#define	PAGE_1_0x7A			(0x7A>>1)
#define	PAGE_2_0x92			(0x92>>1)
#define	PAGE_CBUS_0XC8		(0xC8>>1)


void sii9224_I2C_SetClient( void *hclient_ );

