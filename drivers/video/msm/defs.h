/***********************************************************************************/
/*  Copyright (c) 2002-2009, Silicon Image, Inc.  All rights reserved.             */
/*  No part of this work may be reproduced, modified, distributed, transmitted,    */
/*  transcribed, or translated into any language or computer format, in any form   */
/*  or by any means without written permission of: Silicon Image, Inc.,            */
/*  1060 East Arques Avenue, Sunnyvale, California 94085                           */
/***********************************************************************************/

//
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Project Definitions
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#define HALTIMER

void 		HalTimerInit ( void );
void 		HalTimerSet ( uint8_t index, uint16_t m_sec );
uint8_t 	HalTimerExpired ( uint8_t index );
void		HalTimerWait ( uint16_t m_sec );
uint16_t	HalTimerElapsed( uint8_t index );


#define MHL_TX_SCHEDULER /* this symbol is defined*/ 
#define T_MONITORING_PERIOD		100
//
// This is the time in milliseconds we poll what we poll.
//
#define MONITORING_PERIOD		50

#define SiI_DEVICE_ID			0xB0

#define TX_HW_RESET_PERIOD		10


#define RCP_ENABLE 	1



