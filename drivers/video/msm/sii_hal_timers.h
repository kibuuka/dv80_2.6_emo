//***************************************************************************
//!file     hal_timers.h
//!brief    AT80C51ID2 HAL timer header.
//
// No part of this work may be reproduced, modified, distributed, 
// transmitted, transcribed, or translated into any language or computer 
// format, in any form or by any means without written permission of 
// Silicon Image, Inc., 1060 East Arques Avenue, Sunnyvale, California 94085
//
// Copyright 2008-2009, Silicon Image, Inc.  All rights reserved.
//***************************************************************************/

#ifndef __HAL_TIMER_H__
#define __HAL_TIMER_H__

//***************************************************************************
//!file     si_timer_cfg.h
//!brief    Silicon Image timer definitions header.
//
// No part of this work may be reproduced, modified, distributed,
// transmitted, transcribed, or translated into any language or computer
// format, in any form or by any means without written permission of
// Silicon Image, Inc., 1140 East Arques Avenue, Sunnyvale, California 94085
//
// Copyright 2008-2010,2011 Silicon Image, Inc.  All rights reserved.
//***************************************************************************/
// put debug channel information here
//------------------------------------------------------------------------------
// Array of timer values
//------------------------------------------------------------------------------
// Timers - Target system uses these timers
#define ELAPSED_TIMER               0xFF
#define ELAPSED_TIMER1              0xFE

typedef enum TimerId
{
    TIMER_FOR_MONITORING= 0,		// HalTimerWait() is implemented using busy waiting
    TIMER_POLLING,		// Reserved for main polling loop
    TIMER_2,			// Available
    TIMER_SWWA_WRITE_STAT,
    TIMER_TO_DO_RSEN_CHK,
    TIMER_TO_DO_RSEN_DEGLITCH,
    TIMER_COUNT			// MUST BE LAST!!!!
} timerId_t;

//------------------------------------------------------------------------------
// Array of timer values
//------------------------------------------------------------------------------

extern uint16_t g_timerCounters[TIMER_COUNT];

extern uint16_t g_timerElapsed;
extern uint16_t g_elapsedTick;
extern uint16_t g_timerElapsedGranularity;

extern uint16_t g_timerElapsed1;
extern uint16_t g_elapsedTick1;
extern uint16_t g_timerElapsedGranularity1;

///void TimerTickHandler (struct work_struct *work) ;
void HalTimerInit ( void );
void HalTimerSet ( uint8_t index, uint16_t m_sec );
uint8_t HalTimerExpired ( uint8_t index );
void HalTimerWait_msleep ( uint16_t m_sec );
// Luke, fix HDMI sometimes hang when cable plug-in
void HalTimerWait_usleep ( uint16_t m_sec );
uint16_t HalTimerElapsed( uint8_t index );
uint32_t HalTimerSysTicks (void);

#endif  // __HAL_TIMER_H__
