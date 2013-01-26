/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include "msm_fb.h"
#include "mipi_dsi.h"
#include "mipi_samsung.h"
#include <mach/gpio.h>
#include <linux/delay.h>
#include <linux/device.h> /* for vreg.h */
#include <linux/regulator/consumer.h>
#include <mach/cci_hw_id.h>//CH EVT2 Initial Code
// BugID 626, Luke 2011.8.10, Device hang when Video playing and enter suspend
#include "mdp4.h"

#include <linux/msm-charger.h>

static struct msm_panel_common_pdata *mipi_samsung_pdata;
static struct dsi_buf samsung_tx_buf;
static struct dsi_buf samsung_rx_buf;

/************************* EVT1 init code ************************/
//B: CH samsung mipi new intial code v01
/************************* Power ON Command **********************/
/*1*/
static char evt1_etc_set1[3] = {0xf0,
    0x5a, 0x5a}; 
static char evt1_etc_set2[3] = {0xf1,
    0x5a, 0x5a}; 
static char evt1_etc_set3[3] = {0xfc, 
    0x5a, 0x5a}; 
/*2*/
static char evt1_gamma_set1_300[26] = {0xfa, 
    0x02, 0x10, 0x10, 0x10, 0xba, 0x7f, 0xc6, 0xb5, 0xab, 0xb8,
    0xc8, 0xc6, 0xc9, 0x9f, 0x9e, 0x9c, 0xb7, 0xb8, 0xb5, 0x00,
    0x9d, 0x00, 0x78, 0x00, 0xc1}; 
static char evt1_gamma_set2[2] = {0xfa, 
    0x03}; 
/*3*/
/*4*/
static char evt1_etc_set2_1[4] = {0xf6, 
    0x00, 0x84, 0x09}; 
static char evt1_etc_set2_2[2] = {0xb0, 
    0x09}; 
static char evt1_etc_set2_3[2] = {0xd5, 
    0x64}; 
static char evt1_etc_set2_4[2] = {0xb0, 
    0x0b}; 
static char evt1_etc_set2_5[4] = {0xd5, 
    0xa4, 0x7e, 0x20}; 
static char evt1_etc_set2_9[2] = {0xb0, 
    0x08}; 
static char evt1_etc_set2_10[2] = {0xfd, 
    0xf8}; 
static char evt1_etc_set2_11[2] = {0xb0, 
    0x04}; 
static char evt1_etc_set2_12[2] = {0xf2, 
    0x4d};  
static char evt1_etc_set2_13[4] = {0xb1, 
    0x01, 0x00, 0x16}; 
static char evt1_etc_set2_14[5] = {0xb2, 
    0x06, 0x06, 0x06, 0x06}; 
static char evt1_etc_set2_15[2] = {0x11, 
    0x00};
/*5*/
static char evt1_mem_set1_1[5] = {0x2a, 
    0x00, 0x00, 0x02, 0x57}; 
static char evt1_mem_set1_2[5] = {0x2b, 
    0x00, 0x00, 0x03, 0xff}; 
/*6*/
static char evt1_mem_set2_1[2] = {0x35, 
    0x00}; 
static char evt1_mem_set2_2[5] = {0x2a, 
    0x00, 0x1e, 0x02, 0x39}; 
static char evt1_mem_set2_3[5] = {0x2b, 
    0x00, 0x00, 0x03, 0xbf}; 
static char evt1_mem_set2_4[2] = {0xd1, 
    0x8a}; 
static char evt1_mem_set2_5[2] = {0x29, 
    0x00}; 
//B:CH 20110713 Fix power on snow picture issue
static struct dsi_cmd_desc evt1_samsung_cmd_on_cmds[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(evt1_etc_set1), evt1_etc_set1},
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(evt1_etc_set2), evt1_etc_set2},
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(evt1_etc_set3), evt1_etc_set3},
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(evt1_gamma_set1_300), evt1_gamma_set1_300},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(evt1_gamma_set2), evt1_gamma_set2},
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(evt1_etc_set2_1), evt1_etc_set2_1},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(evt1_etc_set2_2), evt1_etc_set2_2},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(evt1_etc_set2_3), evt1_etc_set2_3},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(evt1_etc_set2_4), evt1_etc_set2_4},
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(evt1_etc_set2_5), evt1_etc_set2_5},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(evt1_etc_set2_9), evt1_etc_set2_9},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(evt1_etc_set2_10), evt1_etc_set2_10},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(evt1_etc_set2_11), evt1_etc_set2_11},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(evt1_etc_set2_12), evt1_etc_set2_12},
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(evt1_etc_set2_13), evt1_etc_set2_13},
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(evt1_etc_set2_14), evt1_etc_set2_14},
    {DTYPE_DCS_WRITE, 1, 0, 0, 150, sizeof(evt1_etc_set2_15), evt1_etc_set2_15},
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(evt1_mem_set1_1), evt1_mem_set1_1},
    {DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(evt1_mem_set1_2), evt1_mem_set1_2},
    {DTYPE_DCS_WRITE, 1, 0, 0, 0, sizeof(evt1_mem_set2_1), evt1_mem_set2_1},
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(evt1_mem_set2_2), evt1_mem_set2_2},
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(evt1_mem_set2_3), evt1_mem_set2_3},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(evt1_mem_set2_4), evt1_mem_set2_4},
};
static struct dsi_cmd_desc evt1_samsung_cmd_on_cmds_end[] = {
    {DTYPE_DCS_WRITE, 1, 0, 0, 0, sizeof(evt1_mem_set2_5), evt1_mem_set2_5},
};
static int firstTimeLcdON = 0;
//E:CH 20110713 Fix power on snow picture issue

/************************* Power OFF Command **********************/
/*1*/
static char evt1_stabdby_off[2] = {0x10, 
    0x00}; 
    
static struct dsi_cmd_desc evt1_samsung_display_off_cmds[] = {
    {DTYPE_DCS_WRITE, 1, 0, 0, 10, sizeof(evt1_stabdby_off), evt1_stabdby_off},//CH, 20110620, Samsung MIPI panel power sequence
};
//E: CH samsung mipi new intial code v01

/************************* EVT2 init code ************************/
/************************* Power ON Command **********************/
/*1*/
static char evt2_etc_set1[3] = {0xf0,
    0x5a, 0x5a}; 
static char evt2_etc_set2[3] = {0xf1,
    0x5a, 0x5a}; 
static char evt2_etc_set3[3] = {0xfc, 
    0x5a, 0x5a}; 
/*2*/
static char evt2_gamma_set1_300[26] = {0xfa, 
    0x02, 0x10, 0x10, 0x10, 0xec, 0xb7, 0xef, 0xd1, 0xca, 0xd1,
    0xdb, 0xda, 0xd8, 0xb5, 0xb8, 0xb0, 0xc5, 0xc8, 0xbf, 0x00,
    0xb9, 0x00, 0x93, 0x00, 0xd9}; 
static char evt2_gamma_set70[26] = {0xfa, 
    0x02, 0x10, 0x10, 0x10, 0xd1, 0x34, 0xd0, 0xd6, 0xba, 0xdc,
    0xe0, 0xd9, 0xe2, 0xc2, 0xc0, 0xbf, 0xd4, 0xd5, 0xd0, 0x00,
    0x73, 0x00, 0x59, 0x00, 0x82};
static char evt2_gamma_set2[2] = {0xfa, 
    0x03}; 
/*3*/
static char evt2_panel_set1[14] = {0xf8, 
    0x27, 0x27, 0x08, 0x08, 0x4e, 0xaa, 0x5e, 0x8a, 0x10, 0x3f,
    0x10, 0x00, 0x00}; 
/*4*/
static char evt2_etc_set2_1[4] = {0xf6, 
    0x00, 0x84, 0x09}; 
static char evt2_etc_set2_2[2] = {0xb0, 
    0x09}; 
static char evt2_etc_set2_3[2] = {0xd5, 
    0x64}; 
static char evt2_etc_set2_4[2] = {0xb0, 
    0x0b}; 
static char evt2_etc_set2_5[4] = {0xd5, 
    0xa4, 0x7e, 0x20}; 
static char evt2_etc_set2_6[2] = {0xb0, 
    0x08}; 
static char evt2_etc_set2_7[2] = {0xfd, 
    0xf8}; 
static char evt2_etc_set2_8[2] = {0xb0, 
    0x04}; 
static char evt2_etc_set2_9[2] = {0xf2, 
    0x4d}; 
static char evt2_etc_set2_10[2] = {0xb0, 
    0x05}; 
static char evt2_etc_set2_11[2] = {0xfd, 
    0x1f}; 
static char evt2_etc_set2_12[4] = {0xb1, 
    0x01, 0x00, 0x16}; 
static char evt2_etc_set2_13[5] = {0xb2, 
    0x06, 0x06, 0x06, 0x06}; 
static char evt2_etc_set2_14[2] = {0x11, 
    0x00}; 
/*5*/
static char evt2_mem_set1_1[5] = {0x2a, 
    0x00, 0x00, 0x02, 0x57}; 
static char evt2_mem_set1_2[5] = {0x2b, 
    0x00, 0x00, 0x03, 0xff}; 
/*6*/
static char evt2_mem_set2_1[2] = {0x35, 
    0x00}; 
static char evt2_mem_set2_2[5] = {0x2a, 
    0x00, 0x1e, 0x02, 0x39}; 
static char evt2_mem_set2_3[5] = {0x2b, 
    0x00, 0x00, 0x03, 0xbf}; 
static char evt2_mem_set2_4[2] = {0xd1, 
    0x8a}; 
static char evt2_mem_set2_5[2] = {0x29, 
    0x00}; 

static struct dsi_cmd_desc evt2_samsung_cmd_on_cmds[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(evt2_etc_set1), evt2_etc_set1},
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(evt2_etc_set2), evt2_etc_set2},
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(evt2_etc_set3), evt2_etc_set3},
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(evt2_gamma_set70), evt2_gamma_set70},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(evt2_gamma_set2), evt2_gamma_set2},
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(evt2_panel_set1), evt2_panel_set1},
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(evt2_etc_set2_1), evt2_etc_set2_1},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(evt2_etc_set2_2), evt2_etc_set2_2},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(evt2_etc_set2_3), evt2_etc_set2_3},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(evt2_etc_set2_4), evt2_etc_set2_4},
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(evt2_etc_set2_5), evt2_etc_set2_5},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(evt2_etc_set2_6), evt2_etc_set2_6},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(evt2_etc_set2_7), evt2_etc_set2_7},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(evt2_etc_set2_8), evt2_etc_set2_8},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(evt2_etc_set2_9), evt2_etc_set2_9},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(evt2_etc_set2_10), evt2_etc_set2_10},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(evt2_etc_set2_11), evt2_etc_set2_11},
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(evt2_etc_set2_12), evt2_etc_set2_12},
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(evt2_etc_set2_13), evt2_etc_set2_13},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 120, sizeof(evt2_etc_set2_14), evt2_etc_set2_14},
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(evt2_mem_set1_1), evt2_mem_set1_1},
    {DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(evt2_mem_set1_2), evt2_mem_set1_2},
    {DTYPE_DCS_WRITE, 1, 0, 0, 0, sizeof(evt2_mem_set2_1), evt2_mem_set2_1},
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(evt2_mem_set2_2), evt2_mem_set2_2},
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(evt2_mem_set2_3), evt2_mem_set2_3},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(evt2_mem_set2_4), evt2_mem_set2_4},
};
static struct dsi_cmd_desc evt2_samsung_cmd_on_cmds_end[] = {
    {DTYPE_DCS_WRITE, 1, 0, 0, 0, sizeof(evt2_mem_set2_5), evt2_mem_set2_5},
};

/************************* Power OFF Command **********************/
/*1*/
static char evt2_stabdby_off[2] = {0x10, 
    0x00}; 
    
static struct dsi_cmd_desc evt2_samsung_display_off_cmds[] = {
    {DTYPE_DCS_WRITE, 1, 0, 0, 0, sizeof(evt2_stabdby_off), evt2_stabdby_off},
};

//B:CH EVT2 MIPI Samsung brightness setting
#if 1
static char evt2_gamma_set290[26] = {0xfa, 
    0x02, 0x10, 0x10, 0x10, 0xec, 0xb7, 0xee, 0xd0, 0xc8, 0xd1,
    0xdb, 0xda, 0xd9, 0xb6, 0xb8, 0xb0, 0xc5, 0xc9, 0xc1, 0x00,
    0xb7, 0x00, 0x91, 0x00, 0xd5}; 

static char evt2_gamma_set280[26] = {0xfa, 
    0x02, 0x10, 0x10, 0x10, 0xed, 0xb6, 0xf0, 0xd1, 0xc9, 0xd2,
    0xdb, 0xda, 0xd9, 0xb6, 0xb8, 0xaf, 0xc7, 0xca, 0xc2, 0x00,
    0xb4, 0x00, 0x8f, 0x00, 0xd3}; 

static char evt2_gamma_set270[26] = {0xfa, 
    0x02, 0x10, 0x10, 0x10, 0xec, 0xb0, 0xef, 0xd1, 0xc9, 0xd2,
    0xdb, 0xda, 0xd9, 0xb7, 0xb9, 0xb1, 0xc6, 0xc9, 0xc0, 0x00,
    0xb2, 0x00, 0x8d, 0x00, 0xd1}; 

static char evt2_gamma_set260[26] = {0xfa, 
    0x02, 0x10, 0x10, 0x10, 0xeb, 0xb2, 0xef, 0xd2, 0xc9, 0xd3,
    0xdb, 0xda, 0xd9, 0xb6, 0xb9, 0xba, 0xc7, 0xca, 0xc1, 0x00,
    0xb0, 0x00, 0x8b, 0x00, 0xce}; 

static char evt2_gamma_set250[26] = {0xfa, 
    0x02, 0x10, 0x10, 0x10, 0xed, 0xa8, 0xef, 0xd2, 0xc8, 0xd2,
    0xda, 0xd9, 0xd8, 0xb7, 0xb9, 0xb2, 0xc8, 0xcb, 0xc2, 0x00,
    0xad, 0x00, 0x89, 0x00, 0xcb}; 

static char evt2_gamma_set240[26] = {0xfa, 
    0x02, 0x10, 0x10, 0x10, 0xed, 0xa4, 0xee, 0xd2, 0xc9, 0xd3,
    0xdc, 0xd9, 0xd9, 0xb7, 0xba, 0xb3, 0xc8, 0xcb, 0xc2, 0x00,
    0xab, 0x00, 0x87, 0x00, 0xc8}; 

static char evt2_gamma_set230[26] = {0xfa, 
    0x02, 0x10, 0x10, 0x10, 0xee, 0xa3, 0xef, 0xd2, 0xc9, 0xd3,
    0xdb, 0xd9, 0xd9, 0xb9, 0xba, 0xb3, 0xc8, 0xcc, 0xc4, 0x00,
    0xa8, 0x00, 0x85, 0x00, 0xc4}; 

static char evt2_gamma_set220[26] = {0xfa, 
    0x02, 0x10, 0x10, 0x10, 0xed, 0x99, 0xee, 0xd3, 0xc8, 0xd3,
    0xdb, 0xd9, 0xd9, 0xb8, 0xbb, 0xb4, 0xc9, 0xcc, 0xc4, 0x00,
    0xa6, 0x00, 0x83, 0x00, 0xc1}; 

static char evt2_gamma_set210[26] = {0xfa, 
    0x02, 0x10, 0x10, 0x10, 0xef, 0x96, 0xef, 0xd1, 0xc7, 0xd3,
    0xdb, 0xd9, 0xd9, 0xb9, 0xbb, 0xb4, 0xca, 0xcc, 0xc6, 0x00,
    0xa3, 0x00, 0x81, 0x00, 0xbd}; 

static char evt2_gamma_set200[26] = {0xfa, 
    0x02, 0x10, 0x10, 0x10, 0xed, 0x8f, 0xed, 0xd2, 0xc7, 0xd5,
    0xdc, 0xda, 0xda, 0xba, 0xbb, 0xb5, 0xca, 0xcc, 0xc4, 0x00,
    0xa0, 0x00, 0x7f, 0x00, 0xbb};

static char evt2_gamma_set190[26] = {0xfa, 
    0x02, 0x10, 0x10, 0x10, 0xed, 0x90, 0xec, 0xd4, 0xc9, 0xd7,
    0xdd, 0xdb, 0xdb, 0xba, 0xbd, 0xb7, 0xca, 0xcd, 0xc5, 0x00,
    0x9f, 0x00, 0x7c, 0x00, 0xb5};

static char evt2_gamma_set180[26] = {0xfa, 
    0x02, 0x10, 0x10, 0x10, 0xf0, 0x8c, 0xed, 0xd5, 0xca, 0xd8,
    0xdc, 0xdb, 0xdc, 0xbb, 0xbd, 0xb7, 0xcb, 0xcd, 0xc5, 0x00,
    0x9c, 0x00, 0x7a, 0x00, 0xb2};

static char evt2_gamma_set170[26] = {0xfa, 
    0x02, 0x10, 0x10, 0x10, 0xf1, 0x81, 0xee, 0xd4, 0xc9, 0xd9,
    0xdd, 0xdb, 0xdb, 0xbb, 0xbd, 0xb7, 0xcc, 0xce, 0xc8, 0x00,
    0x99, 0x00, 0x78, 0x00, 0xae};

static char evt2_gamma_set160[26] = {0xfa, 
    0x02, 0x10, 0x10, 0x10, 0xf0, 0x61, 0xee, 0xd5, 0xc9, 0xd9,
    0xdd, 0xda, 0xdb, 0xbc, 0xbe, 0xb9, 0xcc, 0xcf, 0xc8, 0x00,
    0x96, 0x00, 0x75, 0x00, 0xaa};

static char evt2_gamma_set150[26] = {0xfa, 
    0x02, 0x10, 0x10, 0x10, 0xea, 0x57, 0xe9, 0xd6, 0xc9, 0xda,
    0xdd, 0xda, 0xdc, 0xbc, 0xbe, 0xb8, 0xce, 0xd0, 0xca, 0x00,
    0x92, 0x00, 0x72, 0x00, 0xa6};

static char evt2_gamma_set140[26] = {0xfa, 
    0x02, 0x10, 0x10, 0x10, 0xe8, 0x51, 0xe4, 0xd6, 0xc9, 0xdb,
    0xdc, 0xd9, 0xdc, 0xbe, 0xbf, 0xba, 0xcd, 0xd0, 0xc9, 0x00,
    0x90, 0x00, 0x70, 0x00, 0xa3};

static char evt2_gamma_set130[26] = {0xfa, 
    0x02, 0x10, 0x10, 0x10, 0xe6, 0x4d, 0xe3, 0xd5, 0xc5, 0xda,
    0xdd, 0xda, 0xdd, 0xbe, 0xbe, 0xba, 0xce, 0xd1, 0xca, 0x00,
    0x8c, 0x00, 0x6d, 0x00, 0x9f};

static char evt2_gamma_set120[26] = {0xfa, 
    0x02, 0x10, 0x10, 0x10, 0xe5, 0x48, 0xe4, 0xd5, 0xc5, 0xdb,
    0xde, 0xda, 0xdd, 0xbe, 0xbf, 0xbb, 0xd0, 0xd2, 0xcc, 0x00,
    0x88, 0x00, 0x6a, 0x00, 0x9a};

static char evt2_gamma_set110[26] = {0xfa, 
    0x02, 0x10, 0x10, 0x10, 0xe1, 0x43, 0xe2, 0xd6, 0xc5, 0xdc,
    0xde, 0xda, 0xdf, 0xbf, 0xbf, 0xbb, 0xd0, 0xd3, 0xcd, 0x00,
    0x85, 0x00, 0x67, 0x00, 0x96};

static char evt2_gamma_set100[26] = {0xfa, 
    0x02, 0x10, 0x10, 0x10, 0xdd, 0x3a, 0xe3, 0xd7, 0xc5, 0xdd,
    0xdf, 0xda, 0xdf, 0xc0, 0xbf, 0xbc, 0xd0, 0xd3, 0xcd, 0x00,
    0x81, 0x00, 0x64, 0x00, 0x92};

static char evt2_gamma_set90[26] = {0xfa, 
    0x02, 0x10, 0x10, 0x10, 0xd7, 0x39, 0xd5, 0xd5, 0xbf, 0xdc,
    0xdf, 0xda, 0xe0, 0xc1, 0xc0, 0xbd, 0xd2, 0xd4, 0xcf, 0x00,
    0x7c, 0x00, 0x60, 0x00, 0x8c};

static char evt2_gamma_set80[26] = {0xfa, 
    0x02, 0x10, 0x10, 0x10, 0xd7, 0x39, 0xd6, 0xd6, 0xbf, 0xdd,
    0xe1, 0xda, 0xe2, 0xc0, 0xbf, 0xbd, 0xd3, 0xd5, 0xcf, 0x00,
    0x78, 0x00, 0x5d, 0x00, 0x88};

static char evt2_gamma_set55[26] = {0xfa, 
    0x02, 0x10, 0x10, 0x10, 0xca, 0x2e, 0xc9, 0xd5, 0xb6, 0xdc,
    0xe2, 0xd9, 0xe3, 0xc1, 0xc0, 0xc0, 0xd6, 0xd7, 0xd2, 0x00,
    0x6e, 0x00, 0x51, 0x00, 0x79};

static struct dsi_cmd_desc evt2_samsung_set_bkl1[] = {
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(evt2_gamma_set55), evt2_gamma_set55},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 10, sizeof(evt2_gamma_set2), evt2_gamma_set2},
};
static struct dsi_cmd_desc evt2_samsung_set_bkl2[] = {
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(evt2_gamma_set70), evt2_gamma_set70},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 10, sizeof(evt2_gamma_set2), evt2_gamma_set2},
};
static struct dsi_cmd_desc evt2_samsung_set_bkl3[] = {
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(evt2_gamma_set80), evt2_gamma_set80},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 10, sizeof(evt2_gamma_set2), evt2_gamma_set2},
};
static struct dsi_cmd_desc evt2_samsung_set_bkl4[] = {
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(evt2_gamma_set90), evt2_gamma_set90},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 10, sizeof(evt2_gamma_set2), evt2_gamma_set2},
};
static struct dsi_cmd_desc evt2_samsung_set_bkl5[] = {
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(evt2_gamma_set100), evt2_gamma_set100},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 10, sizeof(evt2_gamma_set2), evt2_gamma_set2},
};
static struct dsi_cmd_desc evt2_samsung_set_bkl6[] = {
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(evt2_gamma_set110), evt2_gamma_set110},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 10, sizeof(evt2_gamma_set2), evt2_gamma_set2},
};
static struct dsi_cmd_desc evt2_samsung_set_bkl7[] = {
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(evt2_gamma_set120), evt2_gamma_set120},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 10, sizeof(evt2_gamma_set2), evt2_gamma_set2},
};
static struct dsi_cmd_desc evt2_samsung_set_bkl8[] = {
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(evt2_gamma_set130), evt2_gamma_set130},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 10, sizeof(evt2_gamma_set2), evt2_gamma_set2},
};
static struct dsi_cmd_desc evt2_samsung_set_bkl9[] = {
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(evt2_gamma_set140), evt2_gamma_set140},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 10, sizeof(evt2_gamma_set2), evt2_gamma_set2},
};
static struct dsi_cmd_desc evt2_samsung_set_bkl10[] = {
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(evt2_gamma_set150), evt2_gamma_set150},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 10, sizeof(evt2_gamma_set2), evt2_gamma_set2},
};
static struct dsi_cmd_desc evt2_samsung_set_bkl11[] = {
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(evt2_gamma_set160), evt2_gamma_set160},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 10, sizeof(evt2_gamma_set2), evt2_gamma_set2},
};
static struct dsi_cmd_desc evt2_samsung_set_bkl12[] = {
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(evt2_gamma_set170), evt2_gamma_set170},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 10, sizeof(evt2_gamma_set2), evt2_gamma_set2},
};
static struct dsi_cmd_desc evt2_samsung_set_bkl13[] = {
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(evt2_gamma_set180), evt2_gamma_set180},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 10, sizeof(evt2_gamma_set2), evt2_gamma_set2},
};
static struct dsi_cmd_desc evt2_samsung_set_bkl14[] = {
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(evt2_gamma_set190), evt2_gamma_set190},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 10, sizeof(evt2_gamma_set2), evt2_gamma_set2},
};
static struct dsi_cmd_desc evt2_samsung_set_bkl15[] = {
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(evt2_gamma_set200), evt2_gamma_set200},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 10, sizeof(evt2_gamma_set2), evt2_gamma_set2},
};
static struct dsi_cmd_desc evt2_samsung_set_bkl16[] = {
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(evt2_gamma_set210), evt2_gamma_set210},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 10, sizeof(evt2_gamma_set2), evt2_gamma_set2},
};
static struct dsi_cmd_desc evt2_samsung_set_bkl17[] = {
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(evt2_gamma_set220), evt2_gamma_set220},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 10, sizeof(evt2_gamma_set2), evt2_gamma_set2},
};
static struct dsi_cmd_desc evt2_samsung_set_bkl18[] = {
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(evt2_gamma_set230), evt2_gamma_set230},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 10, sizeof(evt2_gamma_set2), evt2_gamma_set2},
};
static struct dsi_cmd_desc evt2_samsung_set_bkl19[] = {
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(evt2_gamma_set240), evt2_gamma_set240},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 10, sizeof(evt2_gamma_set2), evt2_gamma_set2},
};
static struct dsi_cmd_desc evt2_samsung_set_bkl20[] = {
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(evt2_gamma_set250), evt2_gamma_set250},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 10, sizeof(evt2_gamma_set2), evt2_gamma_set2},
};
static struct dsi_cmd_desc evt2_samsung_set_bkl21[] = {
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(evt2_gamma_set260), evt2_gamma_set260},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 10, sizeof(evt2_gamma_set2), evt2_gamma_set2},
};
static struct dsi_cmd_desc evt2_samsung_set_bkl22[] = {
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(evt2_gamma_set270), evt2_gamma_set270},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 10, sizeof(evt2_gamma_set2), evt2_gamma_set2},
};
static struct dsi_cmd_desc evt2_samsung_set_bkl23[] = {
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(evt2_gamma_set280), evt2_gamma_set280},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 10, sizeof(evt2_gamma_set2), evt2_gamma_set2},
};
static struct dsi_cmd_desc evt2_samsung_set_bkl24[] = {
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(evt2_gamma_set290), evt2_gamma_set290},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 10, sizeof(evt2_gamma_set2), evt2_gamma_set2},
};
static struct dsi_cmd_desc evt2_samsung_set_bkl25[] = {
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(evt2_gamma_set1_300), evt2_gamma_set1_300},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 10, sizeof(evt2_gamma_set2), evt2_gamma_set2},
};
#endif
//E:CH EVT2 MIPI Samsung brightness setting
//B : CH Mutex protect
static int backlight_lock=0;
static int bl_level_old=0;
//E : CH Mutex protect
extern int cci_fb_UpdateDone;//CH: fix snow picture issue
static void mipi_samsung_panel_set_backlight(struct msm_fb_data_type *mfd)
{
    int bl_level;

//B:CH 20110713 Fix power on snow picture issue
    if (!mfd)        return;
    if (mfd->key != MFD_KEY)        return;
    //B : CH Mutex protect
//B: CH: fix snow picture issue
if( (firstTimeLcdON) && (cci_fb_UpdateDone) )
{
//restore backlight for current setting
mfd->bl_level = bl_level_old;
}
else
{
    if (bl_level_old == mfd->bl_level)    return;
    if (backlight_lock)    return;
}
//E: CH: fix snow picture issue
    //E : CH Mutex protect
//B:CH EVT2 MIPI Samsung brightness setting
    if (!mfd->panel_power_on)        return;

    bl_level = mfd->bl_level;
    printk(KERN_INFO "[LCM] mipi_samsung_panel_set_backlight level=%d max=%d\n", bl_level, mfd->panel_info.bl_max);


	mutex_lock(&mfd->dma->ov_mutex);
        backlight_lock=1;//CH Mutex protect
	/* mdp4_dsi_cmd_busy_wait: will turn on dsi clock also */
	mdp4_dsi_cmd_dma_busy_wait(mfd);
	mdp4_dsi_blt_dmap_busy_wait(mfd);
	
    if((cci_hw_id != EVT0) && (cci_hw_id != EVT1))
    {
//B: CH temp for solution for darkness backlight level
    if(( bl_level == 0 )||( bl_level == 1 ) )
        mipi_dsi_cmds_tx(mfd, &samsung_tx_buf, evt2_samsung_set_bkl1, ARRAY_SIZE(evt2_samsung_set_bkl1));
    else if( bl_level == 2 )
        mipi_dsi_cmds_tx(mfd, &samsung_tx_buf, evt2_samsung_set_bkl2, ARRAY_SIZE(evt2_samsung_set_bkl2));
    else if( bl_level == 3 )
        mipi_dsi_cmds_tx(mfd, &samsung_tx_buf, evt2_samsung_set_bkl3, ARRAY_SIZE(evt2_samsung_set_bkl3));
    else if( bl_level == 4 )
        mipi_dsi_cmds_tx(mfd, &samsung_tx_buf, evt2_samsung_set_bkl4, ARRAY_SIZE(evt2_samsung_set_bkl4));
    else if( bl_level == 5 )
        mipi_dsi_cmds_tx(mfd, &samsung_tx_buf, evt2_samsung_set_bkl5, ARRAY_SIZE(evt2_samsung_set_bkl5));
    else if( bl_level == 6 )
        mipi_dsi_cmds_tx(mfd, &samsung_tx_buf, evt2_samsung_set_bkl6, ARRAY_SIZE(evt2_samsung_set_bkl6));
    else if( bl_level == 7 )
        mipi_dsi_cmds_tx(mfd, &samsung_tx_buf, evt2_samsung_set_bkl7, ARRAY_SIZE(evt2_samsung_set_bkl7));
    else if( bl_level == 8 )
        mipi_dsi_cmds_tx(mfd, &samsung_tx_buf, evt2_samsung_set_bkl8, ARRAY_SIZE(evt2_samsung_set_bkl8));
    else if( bl_level == 9 )
        mipi_dsi_cmds_tx(mfd, &samsung_tx_buf, evt2_samsung_set_bkl9, ARRAY_SIZE(evt2_samsung_set_bkl9));
    else if( bl_level == 10 )
        mipi_dsi_cmds_tx(mfd, &samsung_tx_buf, evt2_samsung_set_bkl10, ARRAY_SIZE(evt2_samsung_set_bkl10));
    else if( bl_level == 11 )
        mipi_dsi_cmds_tx(mfd, &samsung_tx_buf, evt2_samsung_set_bkl11, ARRAY_SIZE(evt2_samsung_set_bkl11));
    else if( bl_level == 12 )
        mipi_dsi_cmds_tx(mfd, &samsung_tx_buf, evt2_samsung_set_bkl12, ARRAY_SIZE(evt2_samsung_set_bkl12));
    else if( bl_level == 13 )
        mipi_dsi_cmds_tx(mfd, &samsung_tx_buf, evt2_samsung_set_bkl13, ARRAY_SIZE(evt2_samsung_set_bkl19));
    else if( bl_level == 14 )
        mipi_dsi_cmds_tx(mfd, &samsung_tx_buf, evt2_samsung_set_bkl14, ARRAY_SIZE(evt2_samsung_set_bkl20));
    else if( bl_level == 15 )
        mipi_dsi_cmds_tx(mfd, &samsung_tx_buf, evt2_samsung_set_bkl15, ARRAY_SIZE(evt2_samsung_set_bkl21));
    else if( bl_level == 16 )
        mipi_dsi_cmds_tx(mfd, &samsung_tx_buf, evt2_samsung_set_bkl16, ARRAY_SIZE(evt2_samsung_set_bkl22));
    else if( bl_level == 17 )
        mipi_dsi_cmds_tx(mfd, &samsung_tx_buf, evt2_samsung_set_bkl17, ARRAY_SIZE(evt2_samsung_set_bkl23));
    else if( bl_level == 18 )
        mipi_dsi_cmds_tx(mfd, &samsung_tx_buf, evt2_samsung_set_bkl18, ARRAY_SIZE(evt2_samsung_set_bkl24));
    else
        mipi_dsi_cmds_tx(mfd, &samsung_tx_buf, evt2_samsung_set_bkl18, ARRAY_SIZE(evt2_samsung_set_bkl25));
    }
//E: CH temp for solution for darkness backlight level
//E:CH EVT2 MIPI Samsung brightness setting
    if( (firstTimeLcdON) && (cci_fb_UpdateDone) )//CH: fix snow picture issue
    {
        printk("MIPI::%s: firstTimeLcdON\n", __func__);
        if((cci_hw_id != EVT0) && (cci_hw_id != EVT1))
        mipi_dsi_cmds_tx(mfd, &samsung_tx_buf, evt2_samsung_cmd_on_cmds_end, ARRAY_SIZE(evt2_samsung_cmd_on_cmds_end));
        else
        mipi_dsi_cmds_tx(mfd, &samsung_tx_buf, evt1_samsung_cmd_on_cmds_end, ARRAY_SIZE(evt1_samsung_cmd_on_cmds_end));
        printk("MIPI::%s: firstTimeLcdONDone\n", __func__);
        firstTimeLcdON=0;
    }
mutex_unlock(&mfd->dma->ov_mutex);
//B : CH Mutex protect
bl_level_old = mfd->bl_level;
backlight_lock=0;
//E : CH Mutex protect
//E:CH 20110713 Fix power on snow picture issue
}  

extern int system_loading_update(enum module_loading_op action, int id, int loading);

static int mipi_samsung_lcd_on(struct platform_device *pdev)
{
    struct msm_fb_data_type *mfd;
    struct mipi_panel_info *mipi;
	
    printk("MIPI::%s: \n", __func__);
    mdelay(10);
    mfd = platform_get_drvdata(pdev);
    if (!mfd)
        return -ENODEV;
    if (mfd->key != MFD_KEY)
        return -EINVAL;

    /* mdp4_dsi_cmd_busy_wait: will turn on dsi clock also */
    mdp4_dsi_cmd_dma_busy_wait(mfd);
    mdp4_dsi_blt_dmap_busy_wait(mfd);

    mipi  = &mfd->panel_info.mipi;
    {
        if((cci_hw_id != EVT0) && (cci_hw_id != EVT1))
        mipi_dsi_cmds_tx(mfd, &samsung_tx_buf, evt2_samsung_cmd_on_cmds,	ARRAY_SIZE(evt2_samsung_cmd_on_cmds));
        else
        mipi_dsi_cmds_tx(mfd, &samsung_tx_buf, evt1_samsung_cmd_on_cmds,	ARRAY_SIZE(evt1_samsung_cmd_on_cmds));
    }
    printk("MIPI::%s: Done\n", __func__);
    firstTimeLcdON=1;//CH 20110713 Fix power on snow picture issue
    cci_fb_UpdateDone = 0;//CH: fix snow picture issue
    bl_level_old=0;

	
    system_loading_update(LOADING_CLR, 3, 0);
    system_loading_update(LOADING_SET, 3, 75);

    return 0;
}

static int mipi_samsung_lcd_off(struct platform_device *pdev)
{
    struct msm_fb_data_type *mfd;

    printk("MIPI::%s: \n", __func__);
    mfd = platform_get_drvdata(pdev);

    if (!mfd)
        return -ENODEV;
    if (mfd->key != MFD_KEY)
        return -EINVAL;

    /* mdp4_dsi_cmd_busy_wait: will turn on dsi clock also */
    mdp4_dsi_cmd_dma_busy_wait(mfd);
    mdp4_dsi_blt_dmap_busy_wait(mfd);

    if((cci_hw_id != EVT0) && (cci_hw_id != EVT1))
    mipi_dsi_cmds_tx(mfd, &samsung_tx_buf, evt2_samsung_display_off_cmds, ARRAY_SIZE(evt2_samsung_display_off_cmds));
    else
    mipi_dsi_cmds_tx(mfd, &samsung_tx_buf, evt1_samsung_display_off_cmds, ARRAY_SIZE(evt1_samsung_display_off_cmds));

    printk("MIPI::%s: Done\n", __func__);
    firstTimeLcdON=0;//CH 20110713 MIPI Fix power on snow picture issue
    bl_level_old=0;

	system_loading_update(LOADING_CLR, 3, 0);

    return 0;
}

static int __devinit mipi_samsung_lcd_probe(struct platform_device *pdev)
{
    printk("MIPI::%s: \n", __func__);
    if (pdev->id == 0) {
        mipi_samsung_pdata = pdev->dev.platform_data;
        return 0;
    }

    msm_fb_add_device(pdev);

    printk("MIPI::%s: Done\n", __func__);
    return 0;
}

static struct platform_driver this_driver = {
    .probe  = mipi_samsung_lcd_probe,
    .driver = {
        .name   = "mipi_samsung",
    },
};

static struct msm_fb_panel_data samsung_panel_data = {
    .on    = mipi_samsung_lcd_on,
    .off    = mipi_samsung_lcd_off,
    .set_backlight    = mipi_samsung_panel_set_backlight,
};

static int ch_used[3];

int mipi_samsung_device_register(struct msm_panel_info *pinfo,
					u32 channel, u32 panel)
{
    struct platform_device *pdev = NULL;
    int ret;

    printk(KERN_ERR "[LCM] mipi_samsung_device_register\n" );

    if ((channel >= 3) || ch_used[channel])
        return -ENODEV;

    ch_used[channel] = TRUE;

    pdev = platform_device_alloc("mipi_samsung", (panel << 8)|channel);
    if (!pdev)
        return -ENOMEM;

    samsung_panel_data.panel_info = *pinfo;

    ret = platform_device_add_data(pdev, &samsung_panel_data, sizeof(samsung_panel_data));
    if (ret) {
        printk(KERN_ERR"%s: platform_device_add_data failed!\n", __func__);
        goto err_device_put;
    }

    ret = platform_device_add(pdev);
    if (ret) {
        printk(KERN_ERR"%s: platform_device_register failed!\n", __func__);
        goto err_device_put;
    }

    return 0;

err_device_put:
    platform_device_put(pdev);
    return ret;
}

//B: Luke MIPI display hang
extern int wait_mipi_dma;
extern void force_dsi_dma_comp(void);

static struct delayed_work mipi_detect;
int count = 0;

static void mipi_detect_work(struct work_struct *work)
{
     printk(" %s -in mutex \n", __func__);

     if( count < 3 )
     {
        if( wait_mipi_dma ==1 )
        count++;
        if( ( wait_mipi_dma ==0 ) || ( wait_mipi_dma ==1 ) ) 
         schedule_delayed_work(&mipi_detect, msecs_to_jiffies(1000)); //schedule the next poll operation
     }
     else   
     {
         wait_mipi_dma = 2;//fail vaule
	  force_dsi_dma_comp();	 
     }

     printk(" %s -out mutex, count=%d wait_mipi_dma=%d\n", __func__, count, wait_mipi_dma);
}
//E: Luke MIPI display hang
static int __init mipi_samsung_lcd_init(void)
{
    printk(KERN_INFO "[LCM] mipi_samsung_lcd_init\n" );
    mipi_dsi_buf_alloc(&samsung_tx_buf, DSI_BUF_SIZE);
    mipi_dsi_buf_alloc(&samsung_rx_buf, DSI_BUF_SIZE);

    //B: Luke MIPI display hang
    INIT_DELAYED_WORK(&mipi_detect, mipi_detect_work);	
    schedule_delayed_work(&mipi_detect, msecs_to_jiffies(100)); //schedule the next poll operation	
    //E: Luke MIPI display hang
    return platform_driver_register(&this_driver);
}

module_init(mipi_samsung_lcd_init);
