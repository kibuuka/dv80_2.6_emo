//***************************************************************************
//!file     hal_timers.c
//!brief    AT80C51ID2 HAL timer support functions.
//
// No part of this work may be reproduced, modified, distributed, 
// transmitted, transcribed, or translated into any language or computer 
// format, in any form or by any means without written permission of 
// Silicon Image, Inc., 1060 East Arques Avenue, Sunnyvale, California 94085
//
// Copyright 2008-2009, Silicon Image, Inc.  All rights reserved.
//***************************************************************************/
//#include <stdio.h>
//#include <MCU_Regs.h>

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
#include "si_datatypes.h"
#include "sii_hal_timers.h"
#include "si_drvisrconfig.h"
volatile unsigned long g_ulTickCounter = 0;

//------------------------------------------------------------------------------
// Array of timer values
//------------------------------------------------------------------------------

uint16_t g_timerCounters[ TIMER_COUNT ];

uint16_t g_timerElapsed;
uint16_t g_elapsedTick;
uint16_t g_timerElapsedGranularity;

uint16_t g_timerElapsed1;
uint16_t g_elapsedTick1;
uint16_t g_timerElapsedGranularity1;

//
// OSL module should provide this handler.
// It should be prepared to reinitialize TIMER_0 to make it effective.
// First call may come without any ticks due to value being 0.
//
extern	void	SiiOslHalTimer(void);

extern struct delayed_work sii_9244_hpd_timer;
extern bool mhl_cable_connected;

//==============================================================================

//------------------------------------------------------------------------------
// Function:    HalTimerWait
// Description: Waits for the specified number of milliseconds, using timer 0.
//------------------------------------------------------------------------------
// Luke, fix HDMI sometimes hang when cable plug-in
void HalTimerWait_usleep ( uint16_t ms )
{
   // unsigned long flags;

   // local_irq_save(flags);
   usleep((unsigned long)ms*1000L); 
   // local_irq_restore(flags);
}

void HalTimerWait_msleep ( uint16_t ms )
{
   // mdelay(ms); 
   msleep(ms); 
#if 0
	int i;

	//
	// For generating wake up and other pulses, we need a CPU spin instead of
	// interrupt based routines.
	// We would disable the interrupts to have more predictable time.
	//
    EA = 0;             // Disable interrupts while waiting

	i = ms * 100;

	while (i)
	{
		i--;
	}
	// restore interrupts after waiting
    EA = 1;
#endif	
}

//------------------------------------------------------------------------------
// Function:    HalTimerSet
// Description:
//------------------------------------------------------------------------------

void HalTimerSet (uint8_t index, uint16_t m_sec)
{
    //EA = 0;                             // Disable interrupts while updating

    switch (index)
    {
    	case ELAPSED_TIMER:
        	g_timerElapsedGranularity = m_sec;
        	g_timerElapsed = 0;
        	g_elapsedTick = 0;
        	break;

    	case ELAPSED_TIMER1:
        	g_timerElapsedGranularity1 = m_sec;
        	g_timerElapsed1 = 0;
        	g_elapsedTick1 = 0;
        	break;
        default:
        	g_timerCounters[index] = m_sec;
        	break;
    }

    //EA = 1;
}

//------------------------------------------------------------------------------
// Function:    HalTimerExpired
// Description: Returns > 0 if specified timer has expired.
//------------------------------------------------------------------------------

uint8_t HalTimerExpired (uint8_t timer)
{
    if (timer < TIMER_COUNT)
    {
        return(g_timerCounters[timer] == 0);
    }

    return(0);
}

//------------------------------------------------------------------------------
// Function:    HalTimerElapsed
// Description: Returns current timer tick.  Rollover depends on the
//              granularity specified in the SetTimer() call.
//------------------------------------------------------------------------------

uint16_t HalTimerElapsed ( uint8_t index )
{
    uint16_t elapsedTime;
    
    //EA = 0;                             // Disable interrupts while updating
    if ( index == ELAPSED_TIMER )
        elapsedTime = g_timerElapsed;
    else
        elapsedTime = g_timerElapsed1;
    //EA = 1;
    
    return( elapsedTime );
}


//==============================================================================

//------------------------------------------------------------------------------
// Function: TimerTickHandler
// Description:
//------------------------------------------------------------------------------
#if 0
void TimerTickHandler (struct work_struct *work)
//static void TimerTickHandler ( void ) interrupt 1
{
    uint8_t i;

    //restart timer immediately
    //TR0 = 0;        // stop timer tick
    //TL0 = 0x66;     // reloader timer value  64614
    //TH0 = 0xFC;

    //TR0 = 1;        // start timer tick

    g_ulTickCounter++;              // Increment the system tick count.

    //decrement all active timers in array

    for ( i = 0; i < TIMER_COUNT; i++ )
    {
        if ( g_timerCounters[ i ] > 0 )
        {
            g_timerCounters[ i ]--;
	    if (0 == g_timerCounters[i])
	    {
               CALL_SII_MHL_TX_DEVICE_TIMER_ISR(i)
             }
        }
    }
    g_elapsedTick++;
    if ( g_elapsedTick == g_timerElapsedGranularity )
    {
        g_timerElapsed++;
        g_elapsedTick = 0;
    }
    g_elapsedTick1++;
    if ( g_elapsedTick1 == g_timerElapsedGranularity1 )
    {
        g_timerElapsed1++;
        g_elapsedTick1 = 0;
    }

    //if(mhl_cable_connected == 1){
    schedule_delayed_work(&sii_9244_hpd_timer, msecs_to_jiffies(1));
    //}
    //else{
    //   printk("g_timerCounters[TIMER_DEFER_RSEN_SAMPLING] = %d guanyi\n",g_timerCounters[TIMER_DEFER_RSEN_SAMPLING]);
    //}	
}
#endif
//------------------------------------------------------------------------------
// Function: HalTimerInit
// Description:
//------------------------------------------------------------------------------

void HalTimerInit ( void )
{
    uint8_t i;

    //initializer timer counters in array
    for ( i = 0; i < TIMER_COUNT; i++ )
    {
        g_timerCounters[ i ] = 0;
    }
    g_timerElapsed  = 0;
    g_timerElapsed1 = 0;
    g_elapsedTick   = 0;
    g_elapsedTick1  = 0;
    g_timerElapsedGranularity   = 0;
    g_timerElapsedGranularity1  = 0;

    // Set up Timer 0 for timer tick

    //TMOD |= 0x01;   // put timer 0 in Mode 1 (16-bit timer)

    //TL0 = 0x66;     // set timer count for interrupt every 1ms (based on 11Mhz crystal)
    //TH0 = 0xFC;     // each count = internal clock/12

    //TR0 = 1;        // start the timer
    //ET0 = 1;        // timer interrupts enable

}
//------------------------------------------------------------------------------
// Function:    HalTimerSysTicks
// Description: Returns the current number of system ticks since we started
// Parameters:
// Returns:
//------------------------------------------------------------------------------

uint32_t HalTimerSysTicks (void)
{
    return g_ulTickCounter;
}

