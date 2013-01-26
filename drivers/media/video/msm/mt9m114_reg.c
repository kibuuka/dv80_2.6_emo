/* Copyright (c) 2009, Code Aurora Forum. All rights reserved.
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

#include "mt9m114.h"


/*
;**********************************************************************************************************
; Customer: CCI
; Project name: N/A
; Host: N/A
; Engineers: Vincent
; Sensor: SOC1040 Rev2, MIPI interface
; Created: Jan-31-2011
; Rev 0:
;	MCLK: 24MHz
;	MIPI Clock: PCLK: 96MHz
;	Preview: 640x480 YCbCr 30fps 
;	Capture: 1280x960 YCbCr 30fps
;**********************************************************************************************************
*/


/*
;**********************************************************************************************************
;[Section2]		// PLL and Timing settings
;**********************************************************************************************************

; [PLL setting]
; [PLL_24]
// for 24 MHz input, VCO=768 MHz
*/
#define MT9M114_REV2
/*B:[bug756] Front camera initial code update to Ver.2-20110824, CCI 20110830 */
#ifdef MT9M114_REV2
#define MT9M114_REV2_0824
#endif
#define MT9M114_REV2_1017 //to solve red circle, CCI 20111026
/*E:[bug756] Front camera initial code update to Ver.2-20110824, CCI 20110830 */

static const struct mt9m114_i2c_reg_conf const pll_setup_tbl[] = {
	// pll setting for 24 MHz input, VCO=768 MHz
	{0x098E, 0x1000, WORD_LEN, 0},	// LOGICAL_ADDRESS_ACCESS
	{0xC97E, 0x01, BYTE_LEN, 0},	// CAM_SYSCTL_PLL_ENABLE
	{0xC980, 0x0120, WORD_LEN, 0},	// CAM_SYSCTL_PLL_DIVIDER_M_N
	{0xC982, 0x0700, WORD_LEN, 0},	// CAM_SYSCTL_PLL_DIVIDER_P 
//	{0xC984, 0x8040, WORD_LEN, 0}	// CAM_PORT_OUTPUT_CONTROL
};

/*
;[Timing setting]
//Preview at 640x480 (VGA) YCbCr 30fps, capture at 2592x1944 JPEG 15fps
//[bin2_skip2_5140SoC_HP]
//Max Framerate in Preview with High Power Mode
*/

static const struct mt9m114_i2c_reg_conf const timing_setting_setup_tbl[] = {
	{0xC800, 0x0004, WORD_LEN, 0}, 	// CAM_SENSOR_CFG_Y_ADDR_START          
	{0xC802, 0x0004, WORD_LEN, 0}, 	// CAM_SENSOR_CFG_X_ADDR_START          
	{0xC804, 0x03CB, WORD_LEN, 0}, 	// CAM_SENSOR_CFG_Y_ADDR_END            
	{0xC806, 0x050B, WORD_LEN, 0}, 	// CAM_SENSOR_CFG_X_ADDR_END            
	#ifdef MT9M114_REV2_0824
	{0xC808, 0x02DC, WORD_LEN, 0}, 	// CAM_SENSOR_CFG_PIXCLK, default is 0x02DC6C00
	{0xC80A, 0x6C00, WORD_LEN, 0}, 	// CAM_SENSOR_CFG_PIXCLK, default is 0x02DC6C00
	#endif
	{0xC80C, 0x0001, WORD_LEN, 0}, 	// CAM_SENSOR_CFG_ROW_SPEED             
	{0xC80E, 0x00DB, WORD_LEN, 0}, 	// CAM_SENSOR_CFG_FINE_INTEG_TIME_MIN   
	{0xC810, 0x05B3, WORD_LEN, 0}, 	// CAM_SENSOR_CFG_FINE_INTEG_TIME_MAX   
	{0xC812, 0x03EE, WORD_LEN, 0}, 	// CAM_SENSOR_CFG_FRAME_LENGTH_LINES    
	{0xC814, 0x0636, WORD_LEN, 0}, 	// CAM_SENSOR_CFG_LINE_LENGTH_PCK       
	{0xC816, 0x0060, WORD_LEN, 0}, 	// CAM_SENSOR_CFG_FINE_CORRECTION       
	{0xC818, 0x03C3, WORD_LEN, 0}, 	// CAM_SENSOR_CFG_CPIPE_LAST_ROW        
	{0xC834, 0x0000, WORD_LEN, 0}, 	// CAM_SENSOR_CONTROL_READ_MODE         
	{0xC854, 0x0000, WORD_LEN, 0}, 	// CAM_CROP_WINDOW_XOFFSET              
	{0xC856, 0x0000, WORD_LEN, 0}, 	// CAM_CROP_WINDOW_YOFFSET              
	{0xC858, 0x0500, WORD_LEN, 0}, 	// CAM_CROP_WINDOW_WIDTH                
	{0xC85A, 0x03C0, WORD_LEN, 0}, 	// CAM_CROP_WINDOW_HEIGHT               
	{0xC85C, 0x03, BYTE_LEN, 0},	// CAM_CROP_CROPMODE         
#ifdef MT9M114_REV2
	{0xC868, 0x0500, WORD_LEN, 0}, 	// CAM_OUTPUT_WIDTH        1280             
	{0xC86A, 0x03C0, WORD_LEN, 0}, 	// CAM_OUTPUT_HEIGHT       960             
	{0xC88C, 0x1E02, WORD_LEN, 0}, 	// CAM_AET_MAX_FRAME_RATE               
	{0xC88E, 0x1E02, WORD_LEN, 0}, 	// CAM_AET_MIN_FRAME_RATE               
	{0xC914, 0x0000, WORD_LEN, 0}, 	// CAM_STAT_AWB_CLIP_WINDOW_XSTART      
	{0xC916, 0x0000, WORD_LEN, 0}, 	// CAM_STAT_AWB_CLIP_WINDOW_YSTART      
	{0xC918, 0x04FF, WORD_LEN, 0}, 	// CAM_STAT_AWB_CLIP_WINDOW_XEND        
	{0xC91A, 0x03BF, WORD_LEN, 0}, 	// CAM_STAT_AWB_CLIP_WINDOW_YEND //Fix slip of a pen, 20110728
	{0xC91C, 0x0000, WORD_LEN, 0}, 	// CAM_STAT_AE_INITIAL_WINDOW_XSTART    
	{0xC91E, 0x0000, WORD_LEN, 0}, 	// CAM_STAT_AE_INITIAL_WINDOW_YSTART    
	{0xC920, 0x00FF, WORD_LEN, 0}, 	// CAM_STAT_AE_INITIAL_WINDOW_XEND      
	{0xC922, 0x00BF, WORD_LEN, 0}, 	// CAM_STAT_AE_INITIAL_WINDOW_YEND    
#else
	{0xC868, 0x0280, WORD_LEN, 0}, 	// CAM_OUTPUT_WIDTH       640              
	{0xC86A, 0x01E0, WORD_LEN, 0}, 	// CAM_OUTPUT_HEIGHT      480              
	{0xC88C, 0x1E02, WORD_LEN, 0}, 	// CAM_AET_MAX_FRAME_RATE               
	{0xC88E, 0x1E02, WORD_LEN, 0}, 	// CAM_AET_MIN_FRAME_RATE               
	{0xC914, 0x0000, WORD_LEN, 0}, 	// CAM_STAT_AWB_CLIP_WINDOW_XSTART      
	{0xC916, 0x0000, WORD_LEN, 0}, 	// CAM_STAT_AWB_CLIP_WINDOW_YSTART      
	{0xC918, 0x027F, WORD_LEN, 0}, 	// CAM_STAT_AWB_CLIP_WINDOW_XEND        
	{0xC91A, 0x01DF, WORD_LEN, 0}, 	// CAM_STAT_AWB_CLIP_WINDOW_YEND        
	{0xC91C, 0x0000, WORD_LEN, 0}, 	// CAM_STAT_AE_INITIAL_WINDOW_XSTART    
	{0xC91E, 0x0000, WORD_LEN, 0}, 	// CAM_STAT_AE_INITIAL_WINDOW_YSTART    
	{0xC920, 0x007F, WORD_LEN, 0}, 	// CAM_STAT_AE_INITIAL_WINDOW_XEND      
	{0xC922, 0x005F, WORD_LEN, 0}, 	// CAM_STAT_AE_INITIAL_WINDOW_YEND    
#endif	
	{0xE801, 0x00, BYTE_LEN, 0},	// AUTO_BINNING_MODE                    
};


// [Step3-Recommended]
// [Sensor optimization]
static const struct mt9m114_i2c_reg_conf const sensor_optimization_setup_tbl[] = {
	{0x316A, 0x8270, WORD_LEN, 0}, 	// DAC_TXLO_ROW           
	{0x316C, 0x8270, WORD_LEN, 0}, 	// DAC_TXLO MY_S          
	{0x3ED0, 0x2305, WORD_LEN, 0}, 	// DAC_LD_4_5 MY_S        
	{0x3ED2, 0x77CF, WORD_LEN, 0}, 	// DAC_LD_6_7 MY_S        
	{0x316E, 0x8202, WORD_LEN, 0}, 	// DAC_ECL MY_S           
	{0x3180, 0x87FF, WORD_LEN, 0}, 	// DELTA_DK_CONTROL       
	{0x30D4, 0x6080, WORD_LEN, 0}, 	// COLUMN_CORRECTION MY_S 
	{0xA802, 0x0008, WORD_LEN, 0}, 	// AE_TRACK_MODE          

	// LOAD=Errata item 1
	{0x3E14, 0xFF39, WORD_LEN, 0}, 	// SAMP_COL_PUP2

	// LOAD=Errata item 2	
	{0x301A, 0x8234, WORD_LEN, 0}, 	// RESET_REGISTER

	// LOAD=Errata item 3
	// LOAD=Patch 0202; 
	// Feature Recommended; Black level correction fix	
	{0x0982, 0x0001, WORD_LEN, 0}, 	// ACCESS_CTL_STAT                                                                                     
	{0x098A, 0x5000, WORD_LEN, 0}, 	// PHYSICAL_ADDRESS_ACCESS                                                                             
	{0xD000, 0x70CF, WORD_LEN, 0},                                                                                                         
	{0xD002, 0xFFFF, WORD_LEN, 0},                                                                                                         
	{0xD004, 0xC5D4, WORD_LEN, 0},                                                                                                         
	{0xD006, 0x903A, WORD_LEN, 0},                                                                                                         
	{0xD008, 0x2144, WORD_LEN, 0},                                                                                                         
	{0xD00A, 0x0C00, WORD_LEN, 0},                                                                                                         
	{0xD00C, 0x2186, WORD_LEN, 0},                                                                                                         
	{0xD00E, 0x0FF3, WORD_LEN, 0},                                                                                                         
	{0xD010, 0xB844, WORD_LEN, 0},                                                                                                         
	{0xD012, 0xB948, WORD_LEN, 0},                                                                                                         
	{0xD014, 0xE082, WORD_LEN, 0},                                                                                                         
	{0xD016, 0x20CC, WORD_LEN, 0},                                                                                                         
	{0xD018, 0x80E2, WORD_LEN, 0},                                                                                                         
	{0xD01A, 0x21CC, WORD_LEN, 0},                                                                                                         
	{0xD01C, 0x80A2, WORD_LEN, 0},                                                                                                         
	{0xD01E, 0x21CC, WORD_LEN, 0},                                                                                                         
	{0xD020, 0x80E2, WORD_LEN, 0},                                                                                                         
	{0xD022, 0xF404, WORD_LEN, 0},                                                                                                         
	{0xD024, 0xD801, WORD_LEN, 0},                                                                                                         
	{0xD026, 0xF003, WORD_LEN, 0},                                                                                                         
	{0xD028, 0xD800, WORD_LEN, 0},                                                                                                         
	{0xD02A, 0x7EE0, WORD_LEN, 0},                                                                                                         
	{0xD02C, 0xC0F1, WORD_LEN, 0},                                                                                                         
	{0xD02E, 0x08BA, WORD_LEN, 0},
	
	{0xD030, 0x0600, WORD_LEN, 0},                                                                                                         
	{0xD032, 0xC1A1, WORD_LEN, 0},                                                                                                         
	{0xD034, 0x76CF, WORD_LEN, 0},                                                                                                         
	{0xD036, 0xFFFF, WORD_LEN, 0},                                                                                                         
	{0xD038, 0xC130, WORD_LEN, 0},                                                                                                         
	{0xD03A, 0x6E04, WORD_LEN, 0},                                                                                                         
	{0xD03C, 0xC040, WORD_LEN, 0},                                                                                                         
	{0xD03E, 0x71CF, WORD_LEN, 0},                                                                                                         
	{0xD040, 0xFFFF, WORD_LEN, 0},                                                                                                         
	{0xD042, 0xC790, WORD_LEN, 0},                                                                                                         
	{0xD044, 0x8103, WORD_LEN, 0},                                                                                                         
	{0xD046, 0x77CF, WORD_LEN, 0},                                                                                                         
	{0xD048, 0xFFFF, WORD_LEN, 0},                                                                                                         
	{0xD04A, 0xC7C0, WORD_LEN, 0},                                                                                                         
	{0xD04C, 0xE001, WORD_LEN, 0},                                                                                                         
	{0xD04E, 0xA103, WORD_LEN, 0},                                                                                                         
	{0xD050, 0xD800, WORD_LEN, 0},                                                                                                         
	{0xD052, 0x0C6A, WORD_LEN, 0},                                                                                                         
	{0xD054, 0x04E0, WORD_LEN, 0},                                                                                                         
	{0xD056, 0xB89E, WORD_LEN, 0},                                                                                                         
	{0xD058, 0x7508, WORD_LEN, 0},                                                                                                         
	{0xD05A, 0x8E1C, WORD_LEN, 0},                                                                                                         
	{0xD05C, 0x0809, WORD_LEN, 0},                                                                                                         
	{0xD05E, 0x0191, WORD_LEN, 0},
	
	{0xD060, 0xD801, WORD_LEN, 0},                                                                                                         
	{0xD062, 0xAE1D, WORD_LEN, 0},                                                                                                         
	{0xD064, 0xE580, WORD_LEN, 0},                                                                                                         
	{0xD066, 0x20CA, WORD_LEN, 0},                                                                                                         
	{0xD068, 0x0022, WORD_LEN, 0},                                                                                                         
	{0xD06A, 0x20CF, WORD_LEN, 0},                                                                                                         
	{0xD06C, 0x0522, WORD_LEN, 0},                                                                                                         
	{0xD06E, 0x0C5C, WORD_LEN, 0},                                                                                                         
	{0xD070, 0x04E2, WORD_LEN, 0},                                                                                                         
	{0xD072, 0x21CA, WORD_LEN, 0},                                                                                                         
	{0xD074, 0x0062, WORD_LEN, 0},                                                                                                         
	{0xD076, 0xE580, WORD_LEN, 0},                                                                                                         
	{0xD078, 0xD901, WORD_LEN, 0},                                                                                                         
	{0xD07A, 0x79C0, WORD_LEN, 0},                                                                                                         
	{0xD07C, 0xD800, WORD_LEN, 0},                                                                                                         
	{0xD07E, 0x0BE6, WORD_LEN, 0},                                                                                                         
	{0xD080, 0x04E0, WORD_LEN, 0},                                                                                                         
	{0xD082, 0xB89E, WORD_LEN, 0},                                                                                                         
	{0xD084, 0x70CF, WORD_LEN, 0},                                                                                                         
	{0xD086, 0xFFFF, WORD_LEN, 0},                                                                                                         
	{0xD088, 0xC8D4, WORD_LEN, 0},                                                                                                         
	{0xD08A, 0x9002, WORD_LEN, 0},                                                                                                         
	{0xD08C, 0x0857, WORD_LEN, 0},                                                                                                         
	{0xD08E, 0x025E, WORD_LEN, 0},
	
	{0xD090, 0xFFDC, WORD_LEN, 0},                                                                                                         
	{0xD092, 0xE080, WORD_LEN, 0},                                                                                                         
	{0xD094, 0x25CC, WORD_LEN, 0},                                                                                                         
	{0xD096, 0x9022, WORD_LEN, 0},                                                                                                         
	{0xD098, 0xF225, WORD_LEN, 0},                                                                                                         
	{0xD09A, 0x1700, WORD_LEN, 0},                                                                                                         
	{0xD09C, 0x108A, WORD_LEN, 0},                                                                                                         
	{0xD09E, 0x73CF, WORD_LEN, 0},                                                                                                         
	{0xD0A0, 0xFF00, WORD_LEN, 0},                                                                                                         
	{0xD0A2, 0x3174, WORD_LEN, 0},                                                                                                         
	{0xD0A4, 0x9307, WORD_LEN, 0},                                                                                                         
	{0xD0A6, 0x2A04, WORD_LEN, 0},                                                                                                         
	{0xD0A8, 0x103E, WORD_LEN, 0},                                                                                                         
	{0xD0AA, 0x9328, WORD_LEN, 0},                                                                                                         
	{0xD0AC, 0x2942, WORD_LEN, 0},                                                                                                         
	{0xD0AE, 0x7140, WORD_LEN, 0},                                                                                                         
	{0xD0B0, 0x2A04, WORD_LEN, 0},                                                                                                         
	{0xD0B2, 0x107E, WORD_LEN, 0},                                                                                                         
	{0xD0B4, 0x9349, WORD_LEN, 0},                                                                                                         
	{0xD0B6, 0x2942, WORD_LEN, 0},                                                                                                         
	{0xD0B8, 0x7141, WORD_LEN, 0},                                                                                                         
	{0xD0BA, 0x2A04, WORD_LEN, 0},                                                                                                         
	{0xD0BC, 0x10BE, WORD_LEN, 0},                                                                                                         
	{0xD0BE, 0x934A, WORD_LEN, 0},
	
	{0xD0C0, 0x2942, WORD_LEN, 0},                                                                                                         
	{0xD0C2, 0x714B, WORD_LEN, 0},                                                                                                         
	{0xD0C4, 0x2A04, WORD_LEN, 0},                                                                                                         
	{0xD0C6, 0x10BE, WORD_LEN, 0},                                                                                                         
	{0xD0C8, 0x130C, WORD_LEN, 0},                                                                                                         
	{0xD0CA, 0x010A, WORD_LEN, 0},                                                                                                         
	{0xD0CC, 0x2942, WORD_LEN, 0},                                                                                                         
	{0xD0CE, 0x7142, WORD_LEN, 0},                                                                                                         
	{0xD0D0, 0x2250, WORD_LEN, 0},                                                                                                         
	{0xD0D2, 0x13CA, WORD_LEN, 0},                                                                                                         
	{0xD0D4, 0x1B0C, WORD_LEN, 0},                                                                                                         
	{0xD0D6, 0x0284, WORD_LEN, 0},                                                                                                         
	{0xD0D8, 0xB307, WORD_LEN, 0},                                                                                                         
	{0xD0DA, 0xB328, WORD_LEN, 0},                                                                                                         
	{0xD0DC, 0x1B12, WORD_LEN, 0},                                                                                                         
	{0xD0DE, 0x02C4, WORD_LEN, 0},                                                                                                         
	{0xD0E0, 0xB34A, WORD_LEN, 0},                                                                                                         
	{0xD0E2, 0xED88, WORD_LEN, 0},                                                                                                         
	{0xD0E4, 0x71CF, WORD_LEN, 0},                                                                                                         
	{0xD0E6, 0xFF00, WORD_LEN, 0},                                                                                                         
	{0xD0E8, 0x3174, WORD_LEN, 0},                                                                                                         
	{0xD0EA, 0x9106, WORD_LEN, 0},                                                                                                         
	{0xD0EC, 0xB88F, WORD_LEN, 0},                                                                                                         
	{0xD0EE, 0xB106, WORD_LEN, 0},
	
	{0xD0F0, 0x210A, WORD_LEN, 0},                                                                                                         
	{0xD0F2, 0x8340, WORD_LEN, 0},                                                                                                         
	{0xD0F4, 0xC000, WORD_LEN, 0},                                                                                                         
	{0xD0F6, 0x21CA, WORD_LEN, 0},                                                                                                         
	{0xD0F8, 0x0062, WORD_LEN, 0},                                                                                                         
	{0xD0FA, 0x20F0, WORD_LEN, 0},                                                                                                         
	{0xD0FC, 0x0040, WORD_LEN, 0},                                                                                                         
	{0xD0FE, 0x0B02, WORD_LEN, 0},                                                                                                         
	{0xD100, 0x0320, WORD_LEN, 0},                                                                                                         
	{0xD102, 0xD901, WORD_LEN, 0},                                                                                                         
	{0xD104, 0x07F1, WORD_LEN, 0},                                                                                                         
	{0xD106, 0x05E0, WORD_LEN, 0},                                                                                                         
	{0xD108, 0xC0A1, WORD_LEN, 0},                                                                                                         
	{0xD10A, 0x78E0, WORD_LEN, 0},                                                                                                         
	{0xD10C, 0xC0F1, WORD_LEN, 0},                                                                                                         
	{0xD10E, 0x71CF, WORD_LEN, 0},                                                                                                         
	{0xD110, 0xFFFF, WORD_LEN, 0},                                                                                                         
	{0xD112, 0xC7C0, WORD_LEN, 0},                                                                                                         
	{0xD114, 0xD840, WORD_LEN, 0},                                                                                                         
	{0xD116, 0xA900, WORD_LEN, 0},                                                                                                         
	{0xD118, 0x71CF, WORD_LEN, 0},                                                                                                         
	{0xD11A, 0xFFFF, WORD_LEN, 0},                                                                                                         
	{0xD11C, 0xD02C, WORD_LEN, 0},                                                                                                         
	{0xD11E, 0xD81E, WORD_LEN, 0},
	
	{0xD120, 0x0A5A, WORD_LEN, 0},                                                                                                         
	{0xD122, 0x04E0, WORD_LEN, 0},                                                                                                         
	{0xD124, 0xDA00, WORD_LEN, 0},                                                                                                         
	{0xD126, 0xD800, WORD_LEN, 0},                                                                                                         
	{0xD128, 0xC0D1, WORD_LEN, 0},                                                                                                         
	{0xD12A, 0x7EE0, WORD_LEN, 0},                                                                                                         
	{0x098E, 0x0000, WORD_LEN, 0}, 	// LOGICAL_ADDRESS_ACCESS                                                                              
	{0xE000, 0x010C, WORD_LEN, 0}, 	// PATCHLDR_LOADER_ADDRESS                                                                             
	{0xE002, 0x0202, WORD_LEN, 0}, 	// PATCHLDR_PATCH_ID                                                                                   
	{0xE004, 0x4103, WORD_LEN, 0},	// PATCHLDR_FIRMWARE_ID  ori 0x41030202                                                                
	{0xE006, 0x0202, WORD_LEN, 0},	// PATCHLDR_FIRMWARE_ID  ori 0x41030202
	{0x0080, 0xFFF0, WORD_LEN, 10}, 	// COMMAND_REGISTER  POLL_FIELD=COMMAND_REGISTER, HOST_COMMAND_0, !=0, DELAY=10, TIMEOUT=100   
	{0x0080, 0xFFF1, WORD_LEN, 10}, 	// COMMAND_REGISTER  POLL_FIELD=COMMAND_REGISTER, HOST_COMMAND_0, !=0, DELAY=10, TIMEOUT=100            

	// LOAD=Patch 0302; Feature Recommended; Adaptive Sensitivity
	{0x0982, 0x0001, WORD_LEN, 0}, 	// ACCESS_CTL_STAT                                                                                          
	{0x098A, 0x512C, WORD_LEN, 0}, 	// PHYSICAL_ADDRESS_ACCESS                                                                                  
	{0xD12C, 0x70CF, WORD_LEN, 0},                                                                                                              
	{0xD12E, 0xFFFF, WORD_LEN, 0},                                                                                                              
	{0xD130, 0xC5D4, WORD_LEN, 0},                                                                                                              
	{0xD132, 0x903A, WORD_LEN, 0},                                                                                                              
	{0xD134, 0x2144, WORD_LEN, 0},                                                                                                              
	{0xD136, 0x0C00, WORD_LEN, 0},                                                                                                              
	{0xD138, 0x2186, WORD_LEN, 0},                                                                                                              
	{0xD13A, 0x0FF3, WORD_LEN, 0},                                                                                                              
	{0xD13C, 0xB844, WORD_LEN, 0},                                                                                                              
	{0xD13E, 0x262F, WORD_LEN, 0},                                                                                                              
	{0xD140, 0xF008, WORD_LEN, 0},                                                                                                              
	{0xD142, 0xB948, WORD_LEN, 0},                                                                                                              
	{0xD144, 0x21CC, WORD_LEN, 0},                                                                                                              
	{0xD146, 0x8021, WORD_LEN, 0},                                                                                                              
	{0xD148, 0xD801, WORD_LEN, 0},                                                                                                              
	{0xD14A, 0xF203, WORD_LEN, 0},                                                                                                              
	{0xD14C, 0xD800, WORD_LEN, 0},                                                                                                              
	{0xD14E, 0x7EE0, WORD_LEN, 0},                                                                                                              
	{0xD150, 0xC0F1, WORD_LEN, 0},                                                                                                              
	{0xD152, 0x71CF, WORD_LEN, 0},                                                                                                              
	{0xD154, 0xFFFF, WORD_LEN, 0},                                                                                                              
	{0xD156, 0xC610, WORD_LEN, 0},                                                                                                              
	{0xD158, 0x910E, WORD_LEN, 0},                                                                                                              
	{0xD15A, 0x208C, WORD_LEN, 0},                                                                                                              
	{0xD15C, 0x8014, WORD_LEN, 0},                                                                                                              
	{0xD15E, 0xF418, WORD_LEN, 0},                                                                                                              
	{0xD160, 0x910F, WORD_LEN, 0},                                                                                                              
	{0xD162, 0x208C, WORD_LEN, 0},                                                                                                              
	{0xD164, 0x800F, WORD_LEN, 0},                                                                                                              
	{0xD166, 0xF414, WORD_LEN, 0},                                                                                                              
	{0xD168, 0x9116, WORD_LEN, 0},                                                                                                              
	{0xD16A, 0x208C, WORD_LEN, 0},                                                                                                              
	{0xD16C, 0x800A, WORD_LEN, 0},                                                                                                              
	{0xD16E, 0xF410, WORD_LEN, 0},                                                                                                              
	{0xD170, 0x9117, WORD_LEN, 0},                                                                                                              
	{0xD172, 0x208C, WORD_LEN, 0},                                                                                                              
	{0xD174, 0x8807, WORD_LEN, 0},                                                                                                              
	{0xD176, 0xF40C, WORD_LEN, 0},                                                                                                              
	{0xD178, 0x9118, WORD_LEN, 0},                                                                                                              
	{0xD17A, 0x2086, WORD_LEN, 0},                                                                                                              
	{0xD17C, 0x0FF3, WORD_LEN, 0},                                                                                                              
	{0xD17E, 0xB848, WORD_LEN, 0},                                                                                                              
	{0xD180, 0x080D, WORD_LEN, 0},                                                                                                              
	{0xD182, 0x0090, WORD_LEN, 0},                                                                                                              
	{0xD184, 0xFFEA, WORD_LEN, 0},                                                                                                              
	{0xD186, 0xE081, WORD_LEN, 0},                                                                                                              
	{0xD188, 0xD801, WORD_LEN, 0},                                                                                                              
	{0xD18A, 0xF203, WORD_LEN, 0},                                                                                                              
	{0xD18C, 0xD800, WORD_LEN, 0},                                                                                                              
	{0xD18E, 0xC0D1, WORD_LEN, 0},                                                                                                              
	{0xD190, 0x7EE0, WORD_LEN, 0},                                                                                                              
	{0xD192, 0x78E0, WORD_LEN, 0},                                                                                                              
	{0xD194, 0xC0F1, WORD_LEN, 0},                                                                                                              
	{0xD196, 0x71CF, WORD_LEN, 0},                                                                                                              
	{0xD198, 0xFFFF, WORD_LEN, 0},                                                                                                              
	{0xD19A, 0xC610, WORD_LEN, 0},                                                                                                              
	{0xD19C, 0x910E, WORD_LEN, 0},                                                                                                              
	{0xD19E, 0x208C, WORD_LEN, 0},                                                                                                              
	{0xD1A0, 0x800A, WORD_LEN, 0},                                                                                                              
	{0xD1A2, 0xF418, WORD_LEN, 0},                                                                                                              
	{0xD1A4, 0x910F, WORD_LEN, 0},                                                                                                              
	{0xD1A6, 0x208C, WORD_LEN, 0},                                                                                                              
	{0xD1A8, 0x8807, WORD_LEN, 0},                                                                                                              
	{0xD1AA, 0xF414, WORD_LEN, 0},                                                                                                              
	{0xD1AC, 0x9116, WORD_LEN, 0},                                                                                                              
	{0xD1AE, 0x208C, WORD_LEN, 0},                                                                                                              
	{0xD1B0, 0x800A, WORD_LEN, 0},                                                                                                              
	{0xD1B2, 0xF410, WORD_LEN, 0},                                                                                                              
	{0xD1B4, 0x9117, WORD_LEN, 0},                                                                                                              
	{0xD1B6, 0x208C, WORD_LEN, 0},                                                                                                              
	{0xD1B8, 0x8807, WORD_LEN, 0},                                                                                                              
	{0xD1BA, 0xF40C, WORD_LEN, 0},                                                                                                              
	{0xD1BC, 0x9118, WORD_LEN, 0},                                                                                                              
	{0xD1BE, 0x2086, WORD_LEN, 0},                                                                                                              
	{0xD1C0, 0x0FF3, WORD_LEN, 0},                                                                                                              
	{0xD1C2, 0xB848, WORD_LEN, 0},                                                                                                              
	{0xD1C4, 0x080D, WORD_LEN, 0},                                                                                                              
	{0xD1C6, 0x0090, WORD_LEN, 0},                                                                                                              
	{0xD1C8, 0xFFD9, WORD_LEN, 0},                                                                                                              
	{0xD1CA, 0xE080, WORD_LEN, 0},                                                                                                              
	{0xD1CC, 0xD801, WORD_LEN, 0},                                                                                                              
	{0xD1CE, 0xF203, WORD_LEN, 0},                                                                                                              
	{0xD1D0, 0xD800, WORD_LEN, 0},                                                                                                              
	{0xD1D2, 0xF1DF, WORD_LEN, 0},                                                                                                              
	{0xD1D4, 0x9040, WORD_LEN, 0},                                                                                                              
	{0xD1D6, 0x71CF, WORD_LEN, 0},                                                                                                              
	{0xD1D8, 0xFFFF, WORD_LEN, 0},                                                                                                              
	{0xD1DA, 0xC5D4, WORD_LEN, 0},                                                                                                              
	{0xD1DC, 0xB15A, WORD_LEN, 0},                                                                                                              
	{0xD1DE, 0x9041, WORD_LEN, 0},                                                                                                              
	{0xD1E0, 0x73CF, WORD_LEN, 0},                                                                                                              
	{0xD1E2, 0xFFFF, WORD_LEN, 0},                                                                                                              
	{0xD1E4, 0xC7D0, WORD_LEN, 0},                                                                                                              
	{0xD1E6, 0xB140, WORD_LEN, 0},                                                                                                              
	{0xD1E8, 0x9042, WORD_LEN, 0},                                                                                                              
	{0xD1EA, 0xB141, WORD_LEN, 0},                                                                                                              
	{0xD1EC, 0x9043, WORD_LEN, 0},                                                                                                              
	{0xD1EE, 0xB142, WORD_LEN, 0},                                                                                                              
	{0xD1F0, 0x9044, WORD_LEN, 0},                                                                                                              
	{0xD1F2, 0xB143, WORD_LEN, 0},                                                                                                              
	{0xD1F4, 0x9045, WORD_LEN, 0},                                                                                                              
	{0xD1F6, 0xB147, WORD_LEN, 0},                                                                                                              
	{0xD1F8, 0x9046, WORD_LEN, 0},                                                                                                              
	{0xD1FA, 0xB148, WORD_LEN, 0},                                                                                                              
	{0xD1FC, 0x9047, WORD_LEN, 0},                                                                                                              
	{0xD1FE, 0xB14B, WORD_LEN, 0},                                                                                                              
	{0xD200, 0x9048, WORD_LEN, 0},                                                                                                              
	{0xD202, 0xB14C, WORD_LEN, 0},                                                                                                              
	{0xD204, 0x9049, WORD_LEN, 0},                                                                                                              
	{0xD206, 0x1958, WORD_LEN, 0},                                                                                                              
	{0xD208, 0x0084, WORD_LEN, 0},                                                                                                              
	{0xD20A, 0x904A, WORD_LEN, 0},                                                                                                              
	{0xD20C, 0x195A, WORD_LEN, 0},                                                                                                              
	{0xD20E, 0x0084, WORD_LEN, 0},                                                                                                              
	{0xD210, 0x8856, WORD_LEN, 0},                                                                                                              
	{0xD212, 0x1B36, WORD_LEN, 0},                                                                                                              
	{0xD214, 0x8082, WORD_LEN, 0},                                                                                                              
	{0xD216, 0x8857, WORD_LEN, 0},                                                                                                              
	{0xD218, 0x1B37, WORD_LEN, 0},                                                                                                              
	{0xD21A, 0x8082, WORD_LEN, 0},                                                                                                              
	{0xD21C, 0x904C, WORD_LEN, 0},                                                                                                              
	{0xD21E, 0x19A7, WORD_LEN, 0},                                                                                                              
	{0xD220, 0x009C, WORD_LEN, 0},                                                                                                              
	{0xD222, 0x881A, WORD_LEN, 0},                                                                                                              
	{0xD224, 0x7FE0, WORD_LEN, 0},                                                                                                              
	{0xD226, 0x1B54, WORD_LEN, 0},                                                                                                              
	{0xD228, 0x8002, WORD_LEN, 0},                                                                                                              
	{0xD22A, 0x78E0, WORD_LEN, 0},                                                                                                              
	{0xD22C, 0x71CF, WORD_LEN, 0},                                                                                                              
	{0xD22E, 0xFFFF, WORD_LEN, 0},                                                                                                              
	{0xD230, 0xC350, WORD_LEN, 0},                                                                                                              
	{0xD232, 0xD828, WORD_LEN, 0},                                                                                                              
	{0xD234, 0xA90B, WORD_LEN, 0},                                                                                                              
	{0xD236, 0x8100, WORD_LEN, 0},                                                                                                              
	{0xD238, 0x01C5, WORD_LEN, 0},                                                                                                              
	{0xD23A, 0x0320, WORD_LEN, 0},                                                                                                              
	{0xD23C, 0xD900, WORD_LEN, 0},                                                                                                              
	{0xD23E, 0x78E0, WORD_LEN, 0},                                                                                                              
	{0xD240, 0x220A, WORD_LEN, 0},                                                                                                              
	{0xD242, 0x1F80, WORD_LEN, 0},                                                                                                              
	{0xD244, 0xFFFF, WORD_LEN, 0},                                                                                                              
	{0xD246, 0xD4E0, WORD_LEN, 0},                                                                                                              
	{0xD248, 0xC0F1, WORD_LEN, 0},                                                                                                              
	{0xD24A, 0x0811, WORD_LEN, 0},                                                                                                              
	{0xD24C, 0x0051, WORD_LEN, 0},                                                                                                              
	{0xD24E, 0x2240, WORD_LEN, 0},                                                                                                              
	{0xD250, 0x1200, WORD_LEN, 0},                                                                                                              
	{0xD252, 0xFFE1, WORD_LEN, 0},                                                                                                              
	{0xD254, 0xD801, WORD_LEN, 0},                                                                                                              
	{0xD256, 0xF006, WORD_LEN, 0},                                                                                                              
	{0xD258, 0x2240, WORD_LEN, 0},                                                                                                              
	{0xD25A, 0x1900, WORD_LEN, 0},                                                                                                              
	{0xD25C, 0xFFDE, WORD_LEN, 0},                                                                                                              
	{0xD25E, 0xD802, WORD_LEN, 0},                                                                                                              
	{0xD260, 0x1A05, WORD_LEN, 0},                                                                                                              
	{0xD262, 0x1002, WORD_LEN, 0},                                                                                                              
	{0xD264, 0xFFF2, WORD_LEN, 0},                                                                                                              
	{0xD266, 0xF195, WORD_LEN, 0},                                                                                                              
	{0xD268, 0xC0F1, WORD_LEN, 0},                                                                                                              
	{0xD26A, 0x0E7E, WORD_LEN, 0},                                                                                                              
	{0xD26C, 0x05C0, WORD_LEN, 0},                                                                                                              
	{0xD26E, 0x75CF, WORD_LEN, 0},                                                                                                              
	{0xD270, 0xFFFF, WORD_LEN, 0},                                                                                                              
	{0xD272, 0xC84C, WORD_LEN, 0},                                                                                                              
	{0xD274, 0x9502, WORD_LEN, 0},                                                                                                              
	{0xD276, 0x77CF, WORD_LEN, 0},                                                                                                              
	{0xD278, 0xFFFF, WORD_LEN, 0},                                                                                                              
	{0xD27A, 0xC344, WORD_LEN, 0},                                                                                                              
	{0xD27C, 0x2044, WORD_LEN, 0},                                                                                                              
	{0xD27E, 0x008E, WORD_LEN, 0},                                                                                                              
	{0xD280, 0xB8A1, WORD_LEN, 0},                                                                                                              
	{0xD282, 0x0926, WORD_LEN, 0},                                                                                                              
	{0xD284, 0x03E0, WORD_LEN, 0},                                                                                                              
	{0xD286, 0xB502, WORD_LEN, 0},                                                                                                              
	{0xD288, 0x9502, WORD_LEN, 0},                                                                                                              
	{0xD28A, 0x952E, WORD_LEN, 0},                                                                                                              
	{0xD28C, 0x7E05, WORD_LEN, 0},                                                                                                              
	{0xD28E, 0xB5C2, WORD_LEN, 0},                                                                                                              
	{0xD290, 0x70CF, WORD_LEN, 0},                                                                                                              
	{0xD292, 0xFFFF, WORD_LEN, 0},                                                                                                              
	{0xD294, 0xC610, WORD_LEN, 0},                                                                                                              
	{0xD296, 0x099A, WORD_LEN, 0},                                                                                                              
	{0xD298, 0x04A0, WORD_LEN, 0},                                                                                                              
	{0xD29A, 0xB026, WORD_LEN, 0},                                                                                                              
	{0xD29C, 0x0E02, WORD_LEN, 0},                                                                                                              
	{0xD29E, 0x0560, WORD_LEN, 0},                                                                                                              
	{0xD2A0, 0xDE00, WORD_LEN, 0},                                                                                                              
	{0xD2A2, 0x0A12, WORD_LEN, 0},                                                                                                              
	{0xD2A4, 0x0320, WORD_LEN, 0},                                                                                                              
	{0xD2A6, 0xB7C4, WORD_LEN, 0},                                                                                                              
	{0xD2A8, 0x0B36, WORD_LEN, 0},                                                                                                              
	{0xD2AA, 0x03A0, WORD_LEN, 0},                                                                                                              
	{0xD2AC, 0x70C9, WORD_LEN, 0},                                                                                                              
	{0xD2AE, 0x9502, WORD_LEN, 0},                                                                                                              
	{0xD2B0, 0x7608, WORD_LEN, 0},                                                                                                              
	{0xD2B2, 0xB8A8, WORD_LEN, 0},                                                                                                              
	{0xD2B4, 0xB502, WORD_LEN, 0},                                                                                                              
	{0xD2B6, 0x70CF, WORD_LEN, 0},                                                                                                              
	{0xD2B8, 0x0000, WORD_LEN, 0},                                                                                                              
	{0xD2BA, 0x5536, WORD_LEN, 0},                                                                                                              
	{0xD2BC, 0x7860, WORD_LEN, 0},                                                                                                              
	{0xD2BE, 0x2686, WORD_LEN, 0},                                                                                                              
	{0xD2C0, 0x1FFB, WORD_LEN, 0},                                                                                                              
	{0xD2C2, 0x9502, WORD_LEN, 0},                                                                                                              
	{0xD2C4, 0x78C5, WORD_LEN, 0},                                                                                                              
	{0xD2C6, 0x0631, WORD_LEN, 0},                                                                                                              
	{0xD2C8, 0x05E0, WORD_LEN, 0},                                                                                                              
	{0xD2CA, 0xB502, WORD_LEN, 0},                                                                                                              
	{0xD2CC, 0x72CF, WORD_LEN, 0},                                                                                                              
	{0xD2CE, 0xFFFF, WORD_LEN, 0},                                                                                                              
	{0xD2D0, 0xC5D4, WORD_LEN, 0},                                                                                                              
	{0xD2D2, 0x923A, WORD_LEN, 0},                                                                                                              
	{0xD2D4, 0x73CF, WORD_LEN, 0},                                                                                                              
	{0xD2D6, 0xFFFF, WORD_LEN, 0},                                                                                                              
	{0xD2D8, 0xC7D0, WORD_LEN, 0},                                                                                                              
	{0xD2DA, 0xB020, WORD_LEN, 0},                                                                                                              
	{0xD2DC, 0x9220, WORD_LEN, 0},                                                                                                              
	{0xD2DE, 0xB021, WORD_LEN, 0},                                                                                                              
	{0xD2E0, 0x9221, WORD_LEN, 0},                                                                                                              
	{0xD2E2, 0xB022, WORD_LEN, 0},                                                                                                              
	{0xD2E4, 0x9222, WORD_LEN, 0},                                                                                                              
	{0xD2E6, 0xB023, WORD_LEN, 0},                                                                                                              
	{0xD2E8, 0x9223, WORD_LEN, 0},                                                                                                              
	{0xD2EA, 0xB024, WORD_LEN, 0},                                                                                                              
	{0xD2EC, 0x9227, WORD_LEN, 0},                                                                                                              
	{0xD2EE, 0xB025, WORD_LEN, 0},                                                                                                              
	{0xD2F0, 0x9228, WORD_LEN, 0},                                                                                                              
	{0xD2F2, 0xB026, WORD_LEN, 0},                                                                                                              
	{0xD2F4, 0x922B, WORD_LEN, 0},                                                                                                              
	{0xD2F6, 0xB027, WORD_LEN, 0},                                                                                                              
	{0xD2F8, 0x922C, WORD_LEN, 0},                                                                                                              
	{0xD2FA, 0xB028, WORD_LEN, 0},                                                                                                              
	{0xD2FC, 0x1258, WORD_LEN, 0},                                                                                                              
	{0xD2FE, 0x0101, WORD_LEN, 0},                                                                                                              
	{0xD300, 0xB029, WORD_LEN, 0},                                                                                                              
	{0xD302, 0x125A, WORD_LEN, 0},                                                                                                              
	{0xD304, 0x0101, WORD_LEN, 0},                                                                                                              
	{0xD306, 0xB02A, WORD_LEN, 0},                                                                                                              
	{0xD308, 0x1336, WORD_LEN, 0},                                                                                                              
	{0xD30A, 0x8081, WORD_LEN, 0},                                                                                                              
	{0xD30C, 0xA836, WORD_LEN, 0},                                                                                                              
	{0xD30E, 0x1337, WORD_LEN, 0},                                                                                                              
	{0xD310, 0x8081, WORD_LEN, 0},                                                                                                              
	{0xD312, 0xA837, WORD_LEN, 0},                                                                                                              
	{0xD314, 0x12A7, WORD_LEN, 0},                                                                                                              
	{0xD316, 0x0701, WORD_LEN, 0},                                                                                                              
	{0xD318, 0xB02C, WORD_LEN, 0},                                                                                                              
	{0xD31A, 0x1354, WORD_LEN, 0},                                                                                                              
	{0xD31C, 0x8081, WORD_LEN, 0},                                                                                                              
	{0xD31E, 0x7FE0, WORD_LEN, 0},                                                                                                              
	{0xD320, 0xA83A, WORD_LEN, 0},                                                                                                              
	{0xD322, 0x78E0, WORD_LEN, 0},                                                                                                              
	{0xD324, 0xC0F1, WORD_LEN, 0},                                                                                                              
	{0xD326, 0x0DC2, WORD_LEN, 0},                                                                                                              
	{0xD328, 0x05C0, WORD_LEN, 0},                                                                                                              
	{0xD32A, 0x7608, WORD_LEN, 0},                                                                                                              
	{0xD32C, 0x09BB, WORD_LEN, 0},                                                                                                              
	{0xD32E, 0x0010, WORD_LEN, 0},                                                                                                              
	{0xD330, 0x75CF, WORD_LEN, 0},                                                                                                              
	{0xD332, 0xFFFF, WORD_LEN, 0},                                                                                                              
	{0xD334, 0xD4E0, WORD_LEN, 0},                                                                                                              
	{0xD336, 0x8D21, WORD_LEN, 0},                                                                                                              
	{0xD338, 0x8D00, WORD_LEN, 0},                                                                                                              
	{0xD33A, 0x2153, WORD_LEN, 0},                                                                                                              
	{0xD33C, 0x0003, WORD_LEN, 0},                                                                                                              
	{0xD33E, 0xB8C0, WORD_LEN, 0},                                                                                                              
	{0xD340, 0x8D45, WORD_LEN, 0},                                                                                                              
	{0xD342, 0x0B23, WORD_LEN, 0},                                                                                                              
	{0xD344, 0x0000, WORD_LEN, 0},                                                                                                              
	{0xD346, 0xEA8F, WORD_LEN, 0},                                                                                                              
	{0xD348, 0x0915, WORD_LEN, 0},                                                                                                              
	{0xD34A, 0x001E, WORD_LEN, 0},                                                                                                              
	{0xD34C, 0xFF81, WORD_LEN, 0},                                                                                                              
	{0xD34E, 0xE808, WORD_LEN, 0},                                                                                                              
	{0xD350, 0x2540, WORD_LEN, 0},                                                                                                              
	{0xD352, 0x1900, WORD_LEN, 0},                                                                                                              
	{0xD354, 0xFFDE, WORD_LEN, 0},                                                                                                              
	{0xD356, 0x8D00, WORD_LEN, 0},                                                                                                              
	{0xD358, 0xB880, WORD_LEN, 0},                                                                                                              
	{0xD35A, 0xF004, WORD_LEN, 0},                                                                                                              
	{0xD35C, 0x8D00, WORD_LEN, 0},                                                                                                              
	{0xD35E, 0xB8A0, WORD_LEN, 0},                                                                                                              
	{0xD360, 0xAD00, WORD_LEN, 0},                                                                                                              
	{0xD362, 0x8D05, WORD_LEN, 0},                                                                                                              
	{0xD364, 0xE081, WORD_LEN, 0},                                                                                                              
	{0xD366, 0x20CC, WORD_LEN, 0},                                                                                                              
	{0xD368, 0x80A2, WORD_LEN, 0},                                                                                                              
	{0xD36A, 0xDF00, WORD_LEN, 0},                                                                                                              
	{0xD36C, 0xF40A, WORD_LEN, 0},                                                                                                              
	{0xD36E, 0x71CF, WORD_LEN, 0},                                                                                                              
	{0xD370, 0xFFFF, WORD_LEN, 0},                                                                                                              
	{0xD372, 0xC84C, WORD_LEN, 0},                                                                                                              
	{0xD374, 0x9102, WORD_LEN, 0},                                                                                                              
	{0xD376, 0x7708, WORD_LEN, 0},                                                                                                              
	{0xD378, 0xB8A6, WORD_LEN, 0},                                                                                                              
	{0xD37A, 0x2786, WORD_LEN, 0},                                                                                                              
	{0xD37C, 0x1FFE, WORD_LEN, 0},                                                                                                              
	{0xD37E, 0xB102, WORD_LEN, 0},                                                                                                              
	{0xD380, 0x0B42, WORD_LEN, 0},                                                                                                              
	{0xD382, 0x0180, WORD_LEN, 0},                                                                                                              
	{0xD384, 0x0E3E, WORD_LEN, 0},                                                                                                              
	{0xD386, 0x0180, WORD_LEN, 0},                                                                                                              
	{0xD388, 0x0F4A, WORD_LEN, 0},                                                                                                              
	{0xD38A, 0x0160, WORD_LEN, 0},                                                                                                              
	{0xD38C, 0x70C9, WORD_LEN, 0},                                                                                                              
	{0xD38E, 0x8D05, WORD_LEN, 0},                                                                                                              
	{0xD390, 0xE081, WORD_LEN, 0},                                                                                                              
	{0xD392, 0x20CC, WORD_LEN, 0},                                                                                                              
	{0xD394, 0x80A2, WORD_LEN, 0},                                                                                                              
	{0xD396, 0xF429, WORD_LEN, 0},                                                                                                              
	{0xD398, 0x76CF, WORD_LEN, 0},                                                                                                              
	{0xD39A, 0xFFFF, WORD_LEN, 0},                                                                                                              
	{0xD39C, 0xC84C, WORD_LEN, 0},                                                                                                              
	{0xD39E, 0x082D, WORD_LEN, 0},                                                                                                              
	{0xD3A0, 0x0051, WORD_LEN, 0},                                                                                                              
	{0xD3A2, 0x70CF, WORD_LEN, 0},                                                                                                              
	{0xD3A4, 0xFFFF, WORD_LEN, 0},                                                                                                              
	{0xD3A6, 0xC90C, WORD_LEN, 0},                                                                                                              
	{0xD3A8, 0x8805, WORD_LEN, 0},                                                                                                              
	{0xD3AA, 0x09B6, WORD_LEN, 0},                                                                                                              
	{0xD3AC, 0x0360, WORD_LEN, 0},                                                                                                              
	{0xD3AE, 0xD908, WORD_LEN, 0},                                                                                                              
	{0xD3B0, 0x2099, WORD_LEN, 0},                                                                                                              
	{0xD3B2, 0x0802, WORD_LEN, 0},                                                                                                              
	{0xD3B4, 0x9634, WORD_LEN, 0},                                                                                                              
	{0xD3B6, 0xB503, WORD_LEN, 0},                                                                                                              
	{0xD3B8, 0x7902, WORD_LEN, 0},                                                                                                              
	{0xD3BA, 0x1523, WORD_LEN, 0},                                                                                                              
	{0xD3BC, 0x1080, WORD_LEN, 0},                                                                                                              
	{0xD3BE, 0xB634, WORD_LEN, 0},                                                                                                              
	{0xD3C0, 0xE001, WORD_LEN, 0},                                                                                                              
	{0xD3C2, 0x1D23, WORD_LEN, 0},                                                                                                              
	{0xD3C4, 0x1002, WORD_LEN, 0},                                                                                                              
	{0xD3C6, 0xF00B, WORD_LEN, 0},                                                                                                              
	{0xD3C8, 0x9634, WORD_LEN, 0},                                                                                                              
	{0xD3CA, 0x9503, WORD_LEN, 0},                                                                                                              
	{0xD3CC, 0x6038, WORD_LEN, 0},                                                                                                              
	{0xD3CE, 0xB614, WORD_LEN, 0},                                                                                                              
	{0xD3D0, 0x153F, WORD_LEN, 0},                                                                                                              
	{0xD3D2, 0x1080, WORD_LEN, 0},                                                                                                              
	{0xD3D4, 0xE001, WORD_LEN, 0},                                                                                                              
	{0xD3D6, 0x1D3F, WORD_LEN, 0},                                                                                                              
	{0xD3D8, 0x1002, WORD_LEN, 0},                                                                                                              
	{0xD3DA, 0xFFA4, WORD_LEN, 0},                                                                                                              
	{0xD3DC, 0x9602, WORD_LEN, 0},                                                                                                              
	{0xD3DE, 0x7F05, WORD_LEN, 0},                                                                                                              
	{0xD3E0, 0xD800, WORD_LEN, 0},                                                                                                              
	{0xD3E2, 0xB6E2, WORD_LEN, 0},                                                                                                              
	{0xD3E4, 0xAD05, WORD_LEN, 0},                                                                                                              
	{0xD3E6, 0x0511, WORD_LEN, 0},                                                                                                              
	{0xD3E8, 0x05E0, WORD_LEN, 0},                                                                                                              
	{0xD3EA, 0xD800, WORD_LEN, 0},                                                                                                              
	{0xD3EC, 0xC0F1, WORD_LEN, 0},                                                                                                              
	{0xD3EE, 0x0CFE, WORD_LEN, 0},                                                                                                              
	{0xD3F0, 0x05C0, WORD_LEN, 0},                                                                                                              
	{0xD3F2, 0x0A96, WORD_LEN, 0},                                                                                                              
	{0xD3F4, 0x05A0, WORD_LEN, 0},                                                                                                              
	{0xD3F6, 0x7608, WORD_LEN, 0},                                                                                                              
	{0xD3F8, 0x0C22, WORD_LEN, 0},                                                                                                              
	{0xD3FA, 0x0240, WORD_LEN, 0},                                                                                                              
	{0xD3FC, 0xE080, WORD_LEN, 0},                                                                                                              
	{0xD3FE, 0x20CA, WORD_LEN, 0},                                                                                                              
	{0xD400, 0x0F82, WORD_LEN, 0},                                                                                                              
	{0xD402, 0x0000, WORD_LEN, 0},                                                                                                              
	{0xD404, 0x190B, WORD_LEN, 0},                                                                                                              
	{0xD406, 0x0C60, WORD_LEN, 0},                                                                                                              
	{0xD408, 0x05A2, WORD_LEN, 0},                                                                                                              
	{0xD40A, 0x21CA, WORD_LEN, 0},                                                                                                              
	{0xD40C, 0x0022, WORD_LEN, 0},                                                                                                              
	{0xD40E, 0x0C56, WORD_LEN, 0},                                                                                                              
	{0xD410, 0x0240, WORD_LEN, 0},                                                                                                              
	{0xD412, 0xE806, WORD_LEN, 0},                                                                                                              
	{0xD414, 0x0E0E, WORD_LEN, 0},                                                                                                              
	{0xD416, 0x0220, WORD_LEN, 0},                                                                                                              
	{0xD418, 0x70C9, WORD_LEN, 0},                                                                                                              
	{0xD41A, 0xF048, WORD_LEN, 0},                                                                                                              
	{0xD41C, 0x0896, WORD_LEN, 0},                                                                                                              
	{0xD41E, 0x0440, WORD_LEN, 0},                                                                                                              
	{0xD420, 0x0E96, WORD_LEN, 0},                                                                                                              
	{0xD422, 0x0400, WORD_LEN, 0},                                                                                                              
	{0xD424, 0x0966, WORD_LEN, 0},                                                                                                              
	{0xD426, 0x0380, WORD_LEN, 0},                                                                                                              
	{0xD428, 0x75CF, WORD_LEN, 0},                                                                                                              
	{0xD42A, 0xFFFF, WORD_LEN, 0},                                                                                                              
	{0xD42C, 0xD4E0, WORD_LEN, 0},                                                                                                              
	{0xD42E, 0x8D00, WORD_LEN, 0},                                                                                                              
	{0xD430, 0x084D, WORD_LEN, 0},                                                                                                              
	{0xD432, 0x001E, WORD_LEN, 0},                                                                                                              
	{0xD434, 0xFF47, WORD_LEN, 0},                                                                                                              
	{0xD436, 0x080D, WORD_LEN, 0},                                                                                                              
	{0xD438, 0x0050, WORD_LEN, 0},                                                                                                              
	{0xD43A, 0xFF57, WORD_LEN, 0},                                                                                                              
	{0xD43C, 0x0841, WORD_LEN, 0},                                                                                                              
	{0xD43E, 0x0051, WORD_LEN, 0},                                                                                                              
	{0xD440, 0x8D04, WORD_LEN, 0},                                                                                                              
	{0xD442, 0x9521, WORD_LEN, 0},                                                                                                              
	{0xD444, 0xE064, WORD_LEN, 0},                                                                                                              
	{0xD446, 0x790C, WORD_LEN, 0},                                                                                                              
	{0xD448, 0x702F, WORD_LEN, 0},                                                                                                              
	{0xD44A, 0x0CE2, WORD_LEN, 0},                                                                                                              
	{0xD44C, 0x05E0, WORD_LEN, 0},                                                                                                              
	{0xD44E, 0xD964, WORD_LEN, 0},                                                                                                              
	{0xD450, 0x72CF, WORD_LEN, 0},                                                                                                              
	{0xD452, 0xFFFF, WORD_LEN, 0},                                                                                                              
	{0xD454, 0xC700, WORD_LEN, 0},                                                                                                              
	{0xD456, 0x9235, WORD_LEN, 0},                                                                                                              
	{0xD458, 0x0811, WORD_LEN, 0},                                                                                                              
	{0xD45A, 0x0043, WORD_LEN, 0},                                                                                                              
	{0xD45C, 0xFF3D, WORD_LEN, 0},                                                                                                              
	{0xD45E, 0x080D, WORD_LEN, 0},                                                                                                              
	{0xD460, 0x0051, WORD_LEN, 0},                                                                                                              
	{0xD462, 0xD801, WORD_LEN, 0},                                                                                                              
	{0xD464, 0xFF77, WORD_LEN, 0},                                                                                                              
	{0xD466, 0xF025, WORD_LEN, 0},                                                                                                              
	{0xD468, 0x9501, WORD_LEN, 0},                                                                                                              
	{0xD46A, 0x9235, WORD_LEN, 0},                                                                                                              
	{0xD46C, 0x0911, WORD_LEN, 0},                                                                                                              
	{0xD46E, 0x0003, WORD_LEN, 0},                                                                                                              
	{0xD470, 0xFF49, WORD_LEN, 0},                                                                                                              
	{0xD472, 0x080D, WORD_LEN, 0},                                                                                                              
	{0xD474, 0x0051, WORD_LEN, 0},                                                                                                              
	{0xD476, 0xD800, WORD_LEN, 0},                                                                                                              
	{0xD478, 0xFF72, WORD_LEN, 0},                                                                                                              
	{0xD47A, 0xF01B, WORD_LEN, 0},                                                                                                              
	{0xD47C, 0x0886, WORD_LEN, 0},                                                                                                              
	{0xD47E, 0x03E0, WORD_LEN, 0},                                                                                                              
	{0xD480, 0xD801, WORD_LEN, 0},                                                                                                              
	{0xD482, 0x0EF6, WORD_LEN, 0},                                                                                                              
	{0xD484, 0x03C0, WORD_LEN, 0},                                                                                                              
	{0xD486, 0x0F52, WORD_LEN, 0},                                                                                                              
	{0xD488, 0x0340, WORD_LEN, 0},                                                                                                              
	{0xD48A, 0x0DBA, WORD_LEN, 0},                                                                                                              
	{0xD48C, 0x0200, WORD_LEN, 0},                                                                                                              
	{0xD48E, 0x0AF6, WORD_LEN, 0},                                                                                                              
	{0xD490, 0x0440, WORD_LEN, 0},                                                                                                              
	{0xD492, 0x0C22, WORD_LEN, 0},                                                                                                              
	{0xD494, 0x0400, WORD_LEN, 0},                                                                                                              
	{0xD496, 0x0D72, WORD_LEN, 0},                                                                                                              
	{0xD498, 0x0440, WORD_LEN, 0},                                                                                                              
	{0xD49A, 0x0DC2, WORD_LEN, 0},                                                                                                              
	{0xD49C, 0x0200, WORD_LEN, 0},                                                                                                              
	{0xD49E, 0x0972, WORD_LEN, 0},                                                                                                              
	{0xD4A0, 0x0440, WORD_LEN, 0},                                                                                                              
	{0xD4A2, 0x0D3A, WORD_LEN, 0},                                                                                                              
	{0xD4A4, 0x0220, WORD_LEN, 0},                                                                                                              
	{0xD4A6, 0xD820, WORD_LEN, 0},                                                                                                              
	{0xD4A8, 0x0BFA, WORD_LEN, 0},                                                                                                              
	{0xD4AA, 0x0260, WORD_LEN, 0},                                                                                                              
	{0xD4AC, 0x70C9, WORD_LEN, 0},                                                                                                              
	{0xD4AE, 0x0451, WORD_LEN, 0},                                                                                                              
	{0xD4B0, 0x05C0, WORD_LEN, 0},                                                                                                              
	{0xD4B2, 0x78E0, WORD_LEN, 0},                                                                                                              
	{0xD4B4, 0xD900, WORD_LEN, 0},                                                                                                              
	{0xD4B6, 0xF00A, WORD_LEN, 0},                                                                                                              
	{0xD4B8, 0x70CF, WORD_LEN, 0},                                                                                                              
	{0xD4BA, 0xFFFF, WORD_LEN, 0},                                                                                                              
	{0xD4BC, 0xD520, WORD_LEN, 0},                                                                                                              
	{0xD4BE, 0x7835, WORD_LEN, 0},                                                                                                              
	{0xD4C0, 0x8041, WORD_LEN, 0},                                                                                                              
	{0xD4C2, 0x8000, WORD_LEN, 0},                                                                                                              
	{0xD4C4, 0xE102, WORD_LEN, 0},                                                                                                              
	{0xD4C6, 0xA040, WORD_LEN, 0},                                                                                                              
	{0xD4C8, 0x09F1, WORD_LEN, 0},                                                                                                              
	{0xD4CA, 0x8114, WORD_LEN, 0},                                                                                                              
	{0xD4CC, 0x71CF, WORD_LEN, 0},                                                                                                              
	{0xD4CE, 0xFFFF, WORD_LEN, 0},                                                                                                              
	{0xD4D0, 0xD4E0, WORD_LEN, 0},                                                                                                              
	{0xD4D2, 0x70CF, WORD_LEN, 0},                                                                                                              
	{0xD4D4, 0xFFFF, WORD_LEN, 0},                                                                                                              
	{0xD4D6, 0xC594, WORD_LEN, 0},                                                                                                              
	{0xD4D8, 0xB03A, WORD_LEN, 0},                                                                                                              
	{0xD4DA, 0x7FE0, WORD_LEN, 0},                                                                                                              
	{0xD4DC, 0xD800, WORD_LEN, 0},                                                                                                              
	{0xD4DE, 0x0000, WORD_LEN, 0},                                                                                                              
	{0xD4E0, 0x0000, WORD_LEN, 0},                                                                                                              
	{0xD4E2, 0x0500, WORD_LEN, 0},                                                                                                              
	{0xD4E4, 0x0500, WORD_LEN, 0},                                                                                                              
	{0xD4E6, 0x0200, WORD_LEN, 0},                                                                                                              
	{0xD4E8, 0x0330, WORD_LEN, 0},                                                                                                              
	{0xD4EA, 0x0000, WORD_LEN, 0},                                                                                                              
	{0xD4EC, 0x0000, WORD_LEN, 0},                                                                                                              
	{0xD4EE, 0x03CD, WORD_LEN, 0},                                                                                                              
	{0xD4F0, 0x050D, WORD_LEN, 0},                                                                                                              
	{0xD4F2, 0x01C5, WORD_LEN, 0},                                                                                                              
	{0xD4F4, 0x03B3, WORD_LEN, 0},                                                                                                              
	{0xD4F6, 0x00E0, WORD_LEN, 0},                                                                                                              
	{0xD4F8, 0x01E3, WORD_LEN, 0},                                                                                                              
	{0xD4FA, 0x0280, WORD_LEN, 0},                                                                                                              
	{0xD4FC, 0x01E0, WORD_LEN, 0},                                                                                                              
	{0xD4FE, 0x0109, WORD_LEN, 0},                                                                                                              
	{0xD500, 0x0080, WORD_LEN, 0},                                                                                                              
	{0xD502, 0x0500, WORD_LEN, 0},                                                                                                              
	{0xD504, 0x0000, WORD_LEN, 0},                                                                                                              
	{0xD506, 0x0000, WORD_LEN, 0},                                                                                                              
	{0xD508, 0x0000, WORD_LEN, 0},                                                                                                              
	{0xD50A, 0x0000, WORD_LEN, 0},                                                                                                              
	{0xD50C, 0x0000, WORD_LEN, 0},                                                                                                              
	{0xD50E, 0x0000, WORD_LEN, 0},                                                                                                              
	{0xD510, 0x0000, WORD_LEN, 0},                                                                                                              
	{0xD512, 0x0000, WORD_LEN, 0},                                                                                                              
	{0xD514, 0x0000, WORD_LEN, 0},                                                                                                              
	{0xD516, 0x0000, WORD_LEN, 0},                                                                                                              
	{0xD518, 0x0000, WORD_LEN, 0},                                                                                                              
	{0xD51A, 0x0000, WORD_LEN, 0},                                                                                                              
	{0xD51C, 0x0000, WORD_LEN, 0},                                                                                                              
	{0xD51E, 0x0000, WORD_LEN, 0},                                                                                                              
	{0xD520, 0xFFFF, WORD_LEN, 0},                                                                                                              
	{0xD522, 0xC9B4, WORD_LEN, 0},                                                                                                              
	{0xD524, 0xFFFF, WORD_LEN, 0},                                                                                                              
	{0xD526, 0xD324, WORD_LEN, 0},                                                                                                              
	{0xD528, 0xFFFF, WORD_LEN, 0},                                                                                                              
	{0xD52A, 0xCA34, WORD_LEN, 0},                                                                                                              
	{0xD52C, 0xFFFF, WORD_LEN, 0},                                                                                                              
	{0xD52E, 0xD3EC, WORD_LEN, 0},                                                                                                              
	{0x098E, 0x0000, WORD_LEN, 0}, 		// LOGICAL_ADDRESS_ACCESS                                                                           
	{0xE000, 0x04B4, WORD_LEN, 0}, 		// PATCHLDR_LOADER_ADDRESS                                                                          
	{0xE002, 0x0302, WORD_LEN, 0}, 		// PATCHLDR_PATCH_ID                                                                                
	{0xE004, 0x4103, WORD_LEN, 0}, 	// PATCHLDR_FIRMWARE_ID  ori 0x41030202
	{0xE006, 0x0202, WORD_LEN, 0}, 	// PATCHLDR_FIRMWARE_ID  ori 0x41030202   	
	{0x0080, 0xFFF0, WORD_LEN, 10}, 		// COMMAND_REGISTER  POLL_FIELD=COMMAND_REGISTER, HOST_COMMAND_0, !=0, DELAY=10, TIMEOUT=100
	{0x0080, 0xFFF1, WORD_LEN, 10} 		// COMMAND_REGISTER  POLL_FIELD=COMMAND_REGISTER, HOST_COMMAND_0, !=0, DELAY=10, TIMEOUT=100      
};      
        
// LOAD=Step4-APGA			
// [Step4-APGA]1: LOAD_PROM=0xA8, PGA, FACTORY, ELSELOAD=PROM_PROMPT                                                                                   
static const struct mt9m114_i2c_reg_conf const apga_setup_tbl[] = {     
#ifdef MT9M114_REV2
  #ifdef MT9M114_REV2_0824
    #ifdef MT9M114_REV2_1017
	{0x3784, 0x0280, WORD_LEN, 0}, 	// CENTER_ROW                     
	{0x3782, 0x01EC, WORD_LEN, 0}, 	// CENTER_COLUMN                  
	{0x37C0, 0x0000, WORD_LEN, 0}, 	// P_GR_Q5
	{0x37C2, 0x0000, WORD_LEN, 0}, 	// P_RD_Q5
	{0x37C4, 0x0000, WORD_LEN, 0}, 	// P_BL_Q5
	{0x37C6, 0x0000, WORD_LEN, 0},      // P_GB_Q5
	
	{0x3640, 0x0250, WORD_LEN, 0}, 	// P_G1_P0Q0                      
	{0x3642, 0x98CA, WORD_LEN, 0}, 	// P_G1_P0Q1                      
	{0x3644, 0x52F0, WORD_LEN, 0}, 	// P_G1_P0Q2                      
	{0x3646, 0xA14C, WORD_LEN, 0}, 	// P_G1_P0Q3                      
	{0x3648, 0x6FF0, WORD_LEN, 0}, 	// P_G1_P0Q4                      
	{0x364A, 0x00F0, WORD_LEN, 0}, 	// P_R_P0Q0                       
	{0x364C, 0x8CEA, WORD_LEN, 0}, 	// P_R_P0Q1                       
	{0x364E, 0x2111, WORD_LEN, 0}, 	// P_R_P0Q2                       
	{0x3650, 0x396B, WORD_LEN, 0}, 	// P_R_P0Q3                       
	{0x3652, 0x17F1, WORD_LEN, 0}, 	// P_R_P0Q4                       
	{0x3654, 0x01B0, WORD_LEN, 0}, 	// P_B_P0Q0                       
	{0x3656, 0x628A, WORD_LEN, 0}, 	// P_B_P0Q1                       
	{0x3658, 0x3430, WORD_LEN, 0}, 	// P_B_P0Q2                       
	{0x365A, 0x9765, WORD_LEN, 0}, 	// P_B_P0Q3                       
	{0x365C, 0x6CD0, WORD_LEN, 0}, 	// P_B_P0Q4                       
	{0x365E, 0x00D0, WORD_LEN, 0}, 	// P_G2_P0Q0                      
	{0x3660, 0xC80A, WORD_LEN, 0}, 	// P_G2_P0Q1                      
	{0x3662, 0x5C50, WORD_LEN, 0}, 	// P_G2_P0Q2                      
	{0x3664, 0xA2AC, WORD_LEN, 0}, 	// P_G2_P0Q3                      
	{0x3666, 0x56B0, WORD_LEN, 0}, 	// P_G2_P0Q4                      
	{0x3680, 0xB36C, WORD_LEN, 0}, 	// P_G1_P1Q0                      
	{0x3682, 0x5CE5, WORD_LEN, 0}, 	// P_G1_P1Q1                      
	{0x3684, 0xBC2D, WORD_LEN, 0}, 	// P_G1_P1Q2                      
	{0x3686, 0x23ED, WORD_LEN, 0}, 	// P_G1_P1Q3                      
	{0x3688, 0xAA2D, WORD_LEN, 0}, 	// P_G1_P1Q4                      
	{0x368A, 0xFE6C, WORD_LEN, 0}, 	// P_R_P1Q0                       
	{0x368C, 0xBB6A, WORD_LEN, 0}, 	// P_R_P1Q1                       
	{0x368E, 0xC86E, WORD_LEN, 0}, 	// P_R_P1Q2                       
	{0x3690, 0x3CCE, WORD_LEN, 0}, 	// P_R_P1Q3                       
	{0x3692, 0x7B0B, WORD_LEN, 0}, 	// P_R_P1Q4                       
	{0x3694, 0xC3EB, WORD_LEN, 0}, 	// P_B_P1Q0                       
	{0x3696, 0xDF2C, WORD_LEN, 0}, 	// P_B_P1Q1                       
	{0x3698, 0xF0EE, WORD_LEN, 0}, 	// P_B_P1Q2                       
	{0x369A, 0x28AD, WORD_LEN, 0}, 	// P_B_P1Q3                       
	{0x369C, 0x3A2D, WORD_LEN, 0}, 	// P_B_P1Q4                       
	{0x369E, 0xF62B, WORD_LEN, 0}, 	// P_G2_P1Q0                      
	{0x36A0, 0x9FC8, WORD_LEN, 0}, 	// P_G2_P1Q1                      
	{0x36A2, 0xFCAE, WORD_LEN, 0}, 	// P_G2_P1Q2                      
	{0x36A4, 0x00ED, WORD_LEN, 0}, 	// P_G2_P1Q3                      
	{0x36A6, 0x8A8F, WORD_LEN, 0}, 	// P_G2_P1Q4                      
	{0x36C0, 0x6A10, WORD_LEN, 0}, 	// P_G1_P2Q0                      
	{0x36C2, 0xC5AB, WORD_LEN, 0}, 	// P_G1_P2Q1                      
	{0x36C4, 0x6D52, WORD_LEN, 0}, 	// P_G1_P2Q2                      
	{0x36C6, 0xE0EB, WORD_LEN, 0}, 	// P_G1_P2Q3                      
	{0x36C8, 0xC932, WORD_LEN, 0}, 	// P_G1_P2Q4                      
	{0x36CA, 0x16D1, WORD_LEN, 0}, 	// P_R_P2Q0                       
	{0x36CC, 0x512C, WORD_LEN, 0}, 	// P_R_P2Q1                       
	{0x36CE, 0x3873, WORD_LEN, 0}, 	// P_R_P2Q2                       
	{0x36D0, 0x936F, WORD_LEN, 0}, 	// P_R_P2Q3                       
	{0x36D2, 0xCF73, WORD_LEN, 0}, 	// P_R_P2Q4                       
	{0x36D4, 0x2E90, WORD_LEN, 0}, 	// P_B_P2Q0                       
	{0x36D6, 0x39AD, WORD_LEN, 0}, 	// P_B_P2Q1                       
	{0x36D8, 0x5252, WORD_LEN, 0}, 	// P_B_P2Q2                       
	{0x36DA, 0x83AF, WORD_LEN, 0}, 	// P_B_P2Q3                       
	{0x36DC, 0xFE31, WORD_LEN, 0}, 	// P_B_P2Q4                       
	{0x36DE, 0x6770, WORD_LEN, 0}, 	// P_G2_P2Q0                      
	{0x36E0, 0x8D0C, WORD_LEN, 0}, 	// P_G2_P2Q1                      
	{0x36E2, 0x6EB2, WORD_LEN, 0}, 	// P_G2_P2Q2                      
	{0x36E4, 0x228C, WORD_LEN, 0}, 	// P_G2_P2Q3                      
	{0x36E6, 0xDAB2, WORD_LEN, 0}, 	// P_G2_P2Q4                      
	{0x3700, 0x93EE, WORD_LEN, 0}, 	// P_G1_P3Q0                      
	{0x3702, 0x250D, WORD_LEN, 0}, 	// P_G1_P3Q1                      
	{0x3704, 0xB2CE, WORD_LEN, 0}, 	// P_G1_P3Q2                      
	{0x3706, 0xA72F, WORD_LEN, 0}, 	// P_G1_P3Q3                      
	{0x3708, 0x0C73, WORD_LEN, 0}, 	// P_G1_P3Q4                      
	{0x370A, 0xD08E, WORD_LEN, 0}, 	// P_R_P3Q0                       
	{0x370C, 0x256D, WORD_LEN, 0}, 	// P_R_P3Q1                       
	{0x370E, 0x284D, WORD_LEN, 0}, 	// P_R_P3Q2                       
	{0x3710, 0xCA90, WORD_LEN, 0}, 	// P_R_P3Q3                       
	{0x3712, 0x29D3, WORD_LEN, 0}, 	// P_R_P3Q4                       
	{0x3714, 0xFA4D, WORD_LEN, 0}, 	// P_B_P3Q0                       
	{0x3716, 0x0C6E, WORD_LEN, 0}, 	// P_B_P3Q1                       
	{0x3718, 0x570E, WORD_LEN, 0}, 	// P_B_P3Q2                       
	{0x371A, 0x980F, WORD_LEN, 0}, 	// P_B_P3Q3                       
	{0x371C, 0x4312, WORD_LEN, 0}, 	// P_B_P3Q4                       
	{0x371E, 0xB40D, WORD_LEN, 0}, 	// P_G2_P3Q0                      
	{0x3720, 0x284D, WORD_LEN, 0}, 	// P_G2_P3Q1                      
	{0x3722, 0xA7F0, WORD_LEN, 0}, 	// P_G2_P3Q2                      
	{0x3724, 0xD4AE, WORD_LEN, 0}, 	// P_G2_P3Q3                      
	{0x3726, 0x4573, WORD_LEN, 0}, 	// P_G2_P3Q4                      
	{0x3740, 0x3AD0, WORD_LEN, 0}, 	// P_G1_P4Q0                      
	{0x3742, 0x2A0F, WORD_LEN, 0}, 	// P_G1_P4Q1                      
	{0x3744, 0x77EF, WORD_LEN, 0}, 	// P_G1_P4Q2                      
	{0x3746, 0xB572, WORD_LEN, 0}, 	// P_G1_P4Q3                      
	{0x3748, 0xF8F5, WORD_LEN, 0}, 	// P_G1_P4Q4                      
	{0x374A, 0x61F1, WORD_LEN, 0}, 	// P_R_P4Q0                       
	{0x374C, 0x108F, WORD_LEN, 0}, 	// P_R_P4Q1                       
	{0x374E, 0xC9EF, WORD_LEN, 0}, 	// P_R_P4Q2                       
	{0x3750, 0x84D3, WORD_LEN, 0}, 	// P_R_P4Q3                       
	{0x3752, 0xBE36, WORD_LEN, 0}, 	// P_R_P4Q4                       
	{0x3754, 0x25F1, WORD_LEN, 0}, 	// P_B_P4Q0                       
	{0x3756, 0x678B, WORD_LEN, 0}, 	// P_B_P4Q1                       
	{0x3758, 0x2872, WORD_LEN, 0}, 	// P_B_P4Q2                       
	{0x375A, 0xAC72, WORD_LEN, 0}, 	// P_B_P4Q3                       
	{0x375C, 0x9616, WORD_LEN, 0}, 	// P_B_P4Q4                       
	{0x375E, 0x3370, WORD_LEN, 0}, 	// P_G2_P4Q0                      
	{0x3760, 0x0190, WORD_LEN, 0}, 	// P_G2_P4Q1                      
	{0x3762, 0x6AEE, WORD_LEN, 0}, 	// P_G2_P4Q2                      
	{0x3764, 0xDDD2, WORD_LEN, 0}, 	// P_G2_P4Q3                      
	{0x3766, 0xF5F5, WORD_LEN, 0}, 	// P_G2_P4Q4                      
	{0x37C0, 0x9F2B, WORD_LEN, 0}, 	// P_GR_Q5
	{0x37C2, 0xEEEA, WORD_LEN, 0}, 	// P_RD_Q5
	{0x37C4, 0xCFCB, WORD_LEN, 0}, 	// P_BL_Q5
	{0x37C6, 0x8DAB, WORD_LEN, 0},  // P_GB_Q5

	{0x098E, 0x0000, WORD_LEN, 0}, 	//  LOGICAL addressing
	{0xC960, 0x0AF0, WORD_LEN, 0}, 	//  CAM_PGA_L_CONFIG_COLOUR_TEMP
	{0xC962, 0x7780, WORD_LEN, 0}, 	//  CAM_PGA_L_CONFIG_GREEN_RED_Q14
	{0xC964, 0x64B4, WORD_LEN, 0}, 	//  CAM_PGA_L_CONFIG_RED_Q14
	{0xC966, 0x755E, WORD_LEN, 0},	//  CAM_PGA_L_CONFIG_GREEN_BLUE_Q14
	{0xC968, 0x7821, WORD_LEN, 0},  //  CAM_PGA_L_CONFIG_BLUE_Q14
	{0xC96A, 0x0FA0, WORD_LEN, 0},	//  CAM_PGA_M_CONFIG_COLOUR_TEMP
	{0xC96C, 0x7FB1, WORD_LEN, 0},  //  CAM_PGA_M_CONFIG_GREEN_RED_Q14
	{0xC96E, 0x8DDE, WORD_LEN, 0},	//  CAM_PGA_M_CONFIG_RED_Q14
	{0xC970, 0x7E87, WORD_LEN, 0},	//  CAM_PGA_M_CONFIG_GREEN_BLUE_Q14
	{0xC972, 0x8497, WORD_LEN, 0},	//  CAM_PGA_M_CONFIG_BLUE_Q14
	{0xC974, 0x1964, WORD_LEN, 0},  //  CAM_PGA_R_CONFIG_COLOUR_TEMP
	{0xC976, 0x7EDA, WORD_LEN, 0},	//  CAM_PGA_R_CONFIG_GREEN_RED_Q14
	{0xC978, 0x7FC8, WORD_LEN, 0},  //  CAM_PGA_R_CONFIG_RED_Q14
	{0xC97A, 0x7E82, WORD_LEN, 0},	//  CAM_PGA_R_CONFIG_GREEN_BLUE_Q14
	{0xC97C, 0x7EEB, WORD_LEN, 0},	//  CAM_PGA_R_CONFIG_BLUE_Q14
	
	{0xC95E, 0x0003, WORD_LEN, 0},	//  CAM_PGA_PGA_CONTROL
    #else
	{0x098E, 0x495E, WORD_LEN, 0}, 	// LOGICAL_ADDRESS_ACCESS [CAM_PGA_PGA_CONTROL]    
	{0xC95E, 0x0000, WORD_LEN, 0}, 	// CAM_PGA_PGA_CONTROL            
	{0x3640, 0x0230, WORD_LEN, 0}, 	// P_G1_P0Q0                      
	{0x3642, 0xF20B, WORD_LEN, 0}, 	// P_G1_P0Q1                      
	{0x3644, 0x0031, WORD_LEN, 0}, 	// P_G1_P0Q2                      
	{0x3646, 0x0F8C, WORD_LEN, 0}, 	// P_G1_P0Q3                      
	{0x3648, 0x2EF1, WORD_LEN, 0}, 	// P_G1_P0Q4                      
	{0x364A, 0x00D0, WORD_LEN, 0}, 	// P_R_P0Q0                       
	{0x364C, 0x9C8B, WORD_LEN, 0}, 	// P_R_P0Q1                       
	{0x364E, 0x7090, WORD_LEN, 0}, 	// P_R_P0Q2                       
	{0x3650, 0x0FAE, WORD_LEN, 0}, 	// P_R_P0Q3                       
	{0x3652, 0x0A12, WORD_LEN, 0}, 	// P_R_P0Q4                       
	{0x3654, 0x0110, WORD_LEN, 0}, 	// P_B_P0Q0                       
	{0x3656, 0x304A, WORD_LEN, 0}, 	// P_B_P0Q1                       
	{0x3658, 0x2290, WORD_LEN, 0}, 	// P_B_P0Q2                       
	{0x365A, 0x4F6B, WORD_LEN, 0}, 	// P_B_P0Q3                       
	{0x365C, 0x3951, WORD_LEN, 0}, 	// P_B_P0Q4                       
	{0x365E, 0x00D0, WORD_LEN, 0}, 	// P_G2_P0Q0                      
	{0x3660, 0x8E2C, WORD_LEN, 0}, 	// P_G2_P0Q1                      
	{0x3662, 0x0431, WORD_LEN, 0}, 	// P_G2_P0Q2                      
	{0x3664, 0x560B, WORD_LEN, 0}, 	// P_G2_P0Q3                      
	{0x3666, 0x1EB1, WORD_LEN, 0}, 	// P_G2_P0Q4                      
	{0x3680, 0xF3CB, WORD_LEN, 0}, 	// P_G1_P1Q0                      
	{0x3682, 0x7D0B, WORD_LEN, 0}, 	// P_G1_P1Q1                      
	{0x3684, 0x65CE, WORD_LEN, 0}, 	// P_G1_P1Q2                      
	{0x3686, 0x5FED, WORD_LEN, 0}, 	// P_G1_P1Q3                      
	{0x3688, 0x95EE, WORD_LEN, 0}, 	// P_G1_P1Q4                      
	{0x368A, 0xB64C, WORD_LEN, 0}, 	// P_R_P1Q0                       
	{0x368C, 0x084C, WORD_LEN, 0}, 	// P_R_P1Q1                       
	{0x368E, 0x6B4E, WORD_LEN, 0}, 	// P_R_P1Q2                       
	{0x3690, 0x0C2C, WORD_LEN, 0}, 	// P_R_P1Q3                       
	{0x3692, 0xB22F, WORD_LEN, 0}, 	// P_R_P1Q4                       
	{0x3694, 0xC68B, WORD_LEN, 0}, 	// P_B_P1Q0                       
	{0x3696, 0xB4ED, WORD_LEN, 0}, 	// P_B_P1Q1                       
	{0x3698, 0xACEE, WORD_LEN, 0}, 	// P_B_P1Q2                       
	{0x369A, 0x230C, WORD_LEN, 0}, 	// P_B_P1Q3                       
	{0x369C, 0xBE2A, WORD_LEN, 0}, 	// P_B_P1Q4                       
	{0x369E, 0xC446, WORD_LEN, 0}, 	// P_G2_P1Q0                      
	{0x36A0, 0x838C, WORD_LEN, 0}, 	// P_G2_P1Q1                      
	{0x36A2, 0xB3AE, WORD_LEN, 0}, 	// P_G2_P1Q2                      
	{0x36A4, 0x942C, WORD_LEN, 0}, 	// P_G2_P1Q3                      
	{0x36A6, 0xB2AF, WORD_LEN, 0}, 	// P_G2_P1Q4                      
	{0x36C0, 0x2751, WORD_LEN, 0}, 	// P_G1_P2Q0                      
	{0x36C2, 0xCF0D, WORD_LEN, 0}, 	// P_G1_P2Q1                      
	{0x36C4, 0x6532, WORD_LEN, 0}, 	// P_G1_P2Q2                      
	{0x36C6, 0x69D0, WORD_LEN, 0}, 	// P_G1_P2Q3                      
	{0x36C8, 0xF750, WORD_LEN, 0}, 	// P_G1_P2Q4                      
	{0x36CA, 0x0371, WORD_LEN, 0}, 	// P_R_P2Q0                       
	{0x36CC, 0x082A, WORD_LEN, 0}, 	// P_R_P2Q1                       
	{0x36CE, 0x6213, WORD_LEN, 0}, 	// P_R_P2Q2                       
	{0x36D0, 0x55F0, WORD_LEN, 0}, 	// P_R_P2Q3                       
	{0x36D2, 0xB513, WORD_LEN, 0}, 	// P_R_P2Q4                       
	{0x36D4, 0x3EB0, WORD_LEN, 0}, 	// P_B_P2Q0                       
	{0x36D6, 0xD3EC, WORD_LEN, 0}, 	// P_B_P2Q1                       
	{0x36D8, 0x22B3, WORD_LEN, 0}, 	// P_B_P2Q2                       
	{0x36DA, 0x1C51, WORD_LEN, 0}, 	// P_B_P2Q3                       
	{0x36DC, 0x83F3, WORD_LEN, 0}, 	// P_B_P2Q4                       
	{0x36DE, 0x2471, WORD_LEN, 0}, 	// P_G2_P2Q0                      
	{0x36E0, 0x88ED, WORD_LEN, 0}, 	// P_G2_P2Q1                      
	{0x36E2, 0x7492, WORD_LEN, 0}, 	// P_G2_P2Q2                      
	{0x36E4, 0x5930, WORD_LEN, 0}, 	// P_G2_P2Q3                      
	{0x36E6, 0xCDF1, WORD_LEN, 0}, 	// P_G2_P2Q4                      
	{0x3700, 0x816F, WORD_LEN, 0}, 	// P_G1_P3Q0                      
	{0x3702, 0xFBEC, WORD_LEN, 0}, 	// P_G1_P3Q1                      
	{0x3704, 0xD6CF, WORD_LEN, 0}, 	// P_G1_P3Q2                      
	{0x3706, 0xCACF, WORD_LEN, 0}, 	// P_G1_P3Q3                      
	{0x3708, 0xA7D1, WORD_LEN, 0}, 	// P_G1_P3Q4                      
	{0x370A, 0x95EE, WORD_LEN, 0}, 	// P_R_P3Q0                       
	{0x370C, 0xA62E, WORD_LEN, 0}, 	// P_R_P3Q1                       
	{0x370E, 0xFF90, WORD_LEN, 0}, 	// P_R_P3Q2                       
	{0x3710, 0x5F4B, WORD_LEN, 0}, 	// P_R_P3Q3                       
	{0x3712, 0x1EEF, WORD_LEN, 0}, 	// P_R_P3Q4                       
	{0x3714, 0x17ED, WORD_LEN, 0}, 	// P_B_P3Q0                       
	{0x3716, 0x62EF, WORD_LEN, 0}, 	// P_B_P3Q1                       
	{0x3718, 0x47CE, WORD_LEN, 0}, 	// P_B_P3Q2                       
	{0x371A, 0x9A71, WORD_LEN, 0}, 	// P_B_P3Q3                       
	{0x371C, 0x98F1, WORD_LEN, 0}, 	// P_B_P3Q4                       
	{0x371E, 0x9A2E, WORD_LEN, 0}, 	// P_G2_P3Q0                      
	{0x3720, 0x592D, WORD_LEN, 0}, 	// P_G2_P3Q1                      
	{0x3722, 0xD210, WORD_LEN, 0}, 	// P_G2_P3Q2                      
	{0x3724, 0xDC6E, WORD_LEN, 0}, 	// P_G2_P3Q3                      
	{0x3726, 0x3FF1, WORD_LEN, 0}, 	// P_G2_P3Q4                      
	{0x3740, 0x4A8E, WORD_LEN, 0}, 	// P_G1_P4Q0                      
	{0x3742, 0x0CB1, WORD_LEN, 0}, 	// P_G1_P4Q1                      
	{0x3744, 0x1914, WORD_LEN, 0}, 	// P_G1_P4Q2                      
	{0x3746, 0xD0D3, WORD_LEN, 0}, 	// P_G1_P4Q3                      
	{0x3748, 0xF836, WORD_LEN, 0}, 	// P_G1_P4Q4                      
	{0x374A, 0x0BB2, WORD_LEN, 0}, 	// P_R_P4Q0                       
	{0x374C, 0x0BB1, WORD_LEN, 0}, 	// P_R_P4Q1                       
	{0x374E, 0x36D3, WORD_LEN, 0}, 	// P_R_P4Q2                       
	{0x3750, 0x8754, WORD_LEN, 0}, 	// P_R_P4Q3                       
	{0x3752, 0x9537, WORD_LEN, 0}, 	// P_R_P4Q4                       
	{0x3754, 0x49B1, WORD_LEN, 0}, 	// P_B_P4Q0                       
	{0x3756, 0x2DF1, WORD_LEN, 0}, 	// P_B_P4Q1                       
	{0x3758, 0x4031, WORD_LEN, 0}, 	// P_B_P4Q2                       
	{0x375A, 0x8A14, WORD_LEN, 0}, 	// P_B_P4Q3                       
	{0x375C, 0xA356, WORD_LEN, 0}, 	// P_B_P4Q4                       
	{0x375E, 0x04CF, WORD_LEN, 0}, 	// P_G2_P4Q0                      
	{0x3760, 0x2CB1, WORD_LEN, 0}, 	// P_G2_P4Q1                      
	{0x3762, 0x04B4, WORD_LEN, 0}, 	// P_G2_P4Q2                      
	{0x3764, 0xDFF3, WORD_LEN, 0}, 	// P_G2_P4Q3                      
	{0x3766, 0xE856, WORD_LEN, 0}, 	// P_G2_P4Q4                      
	{0x3782, 0x01F4, WORD_LEN, 0}, 	// CENTER_ROW                     
	{0x3784, 0x0294, WORD_LEN, 0}, 	// CENTER_COLUMN                  
	{0x37C0, 0x940A, WORD_LEN, 0}, 	
	{0x37C2, 0xC38A, WORD_LEN, 0}, 	
	{0x37C4, 0xED0A, WORD_LEN, 0}, 	
	{0x37C6, 0xFB29, WORD_LEN, 0}, 	
	{0xC95E, 0x0000, WORD_LEN, 0}, 	// CAM_PGA_PGA_CONTROL
	{0xC95E, 0x0001, WORD_LEN, 0}, 	// CAM_PGA_PGA_CONTROL
    #endif
  #else 
	{0x098E, 0x495E, WORD_LEN, 0}, 	// LOGICAL_ADDRESS_ACCESS [CAM_PGA_PGA_CONTROL]    
	{0xC95E, 0x0000, WORD_LEN, 0}, 	// CAM_PGA_PGA_CONTROL            
	{0x3640, 0x0230, WORD_LEN, 0}, 	// P_G1_P0Q0                      
	{0x3642, 0xFFEB, WORD_LEN, 0}, 	// P_G1_P0Q1                      
	{0x3644, 0x72D0, WORD_LEN, 0}, 	// P_G1_P0Q2                      
	{0x3646, 0x51EB, WORD_LEN, 0}, 	// P_G1_P0Q3                      
	{0x3648, 0x1071, WORD_LEN, 0}, 	// P_G1_P0Q4                      
	{0x364A, 0x0110, WORD_LEN, 0}, 	// P_R_P0Q0                       
	{0x364C, 0xAAAB, WORD_LEN, 0}, 	// P_R_P0Q1                       
	{0x364E, 0x6450, WORD_LEN, 0}, 	// P_R_P0Q2                       
	{0x3650, 0x03EE, WORD_LEN, 0}, 	// P_R_P0Q3                       
	{0x3652, 0x71F1, WORD_LEN, 0}, 	// P_R_P0Q4                       
	{0x3654, 0x0170, WORD_LEN, 0}, 	// P_B_P0Q0                       
	{0x3656, 0x1FCA, WORD_LEN, 0}, 	// P_B_P0Q1                       
	{0x3658, 0x1770, WORD_LEN, 0}, 	// P_B_P0Q2                       
	{0x365A, 0x4A8A, WORD_LEN, 0}, 	// P_B_P0Q3                       
	{0x365C, 0x1F71, WORD_LEN, 0}, 	// P_B_P0Q4                       
	{0x365E, 0x00D0, WORD_LEN, 0}, 	// P_G2_P0Q0                      
	{0x3660, 0x9B0C, WORD_LEN, 0}, 	// P_G2_P0Q1                      
	{0x3662, 0x7A10, WORD_LEN, 0}, 	// P_G2_P0Q2                      
	{0x3664, 0x4B2B, WORD_LEN, 0}, 	// P_G2_P0Q3                      
	{0x3666, 0x0251, WORD_LEN, 0}, 	// P_G2_P0Q4                      
	{0x3680, 0x862C, WORD_LEN, 0}, 	// P_G1_P1Q0                      
	{0x3682, 0x508B, WORD_LEN, 0}, 	// P_G1_P1Q1                      
	{0x3684, 0x75EE, WORD_LEN, 0}, 	// P_G1_P1Q2                      
	{0x3686, 0x6E2D, WORD_LEN, 0}, 	// P_G1_P1Q3                      
	{0x3688, 0x8C0F, WORD_LEN, 0}, 	// P_G1_P1Q4                      
	{0x368A, 0xC62C, WORD_LEN, 0}, 	// P_R_P1Q0                       
	{0x368C, 0x752B, WORD_LEN, 0}, 	// P_R_P1Q1                       
	{0x368E, 0x7D6E, WORD_LEN, 0}, 	// P_R_P1Q2                       
	{0x3690, 0x790B, WORD_LEN, 0}, 	// P_R_P1Q3                       
	{0x3692, 0xCBAF, WORD_LEN, 0}, 	// P_R_P1Q4                       
	{0x3694, 0xDD0B, WORD_LEN, 0}, 	// P_B_P1Q0                       
	{0x3696, 0xAFAD, WORD_LEN, 0}, 	// P_B_P1Q1                       
	{0x3698, 0x966E, WORD_LEN, 0}, 	// P_B_P1Q2                       
	{0x369A, 0x3E6C, WORD_LEN, 0}, 	// P_B_P1Q3                       
	{0x369C, 0x986D, WORD_LEN, 0}, 	// P_B_P1Q4                       
	{0x369E, 0xD1C8, WORD_LEN, 0}, 	// P_G2_P1Q0                      
	{0x36A0, 0x80AC, WORD_LEN, 0}, 	// P_G2_P1Q1                      
	{0x36A2, 0xC9EE, WORD_LEN, 0}, 	// P_G2_P1Q2                      
	{0x36A4, 0x846C, WORD_LEN, 0}, 	// P_G2_P1Q3                      
	{0x36A6, 0x8E0F, WORD_LEN, 0}, 	// P_G2_P1Q4                      
	{0x36C0, 0x1F71, WORD_LEN, 0}, 	// P_G1_P2Q0                      
	{0x36C2, 0xF28D, WORD_LEN, 0}, 	// P_G1_P2Q1                      
	{0x36C4, 0x5712, WORD_LEN, 0}, 	// P_G1_P2Q2                      
	{0x36C6, 0x6BF0, WORD_LEN, 0}, 	// P_G1_P2Q3                      
	{0x36C8, 0x8492, WORD_LEN, 0}, 	// P_G1_P2Q4                      
	{0x36CA, 0x7870, WORD_LEN, 0}, 	// P_R_P2Q0                       
	{0x36CC, 0x1ACC, WORD_LEN, 0}, 	// P_R_P2Q1                       
	{0x36CE, 0x5EF3, WORD_LEN, 0}, 	// P_R_P2Q2                       
	{0x36D0, 0x612F, WORD_LEN, 0}, 	// P_R_P2Q3                       
	{0x36D2, 0xE793, WORD_LEN, 0}, 	// P_R_P2Q4                       
	{0x36D4, 0x3650, WORD_LEN, 0}, 	// P_B_P2Q0                       
	{0x36D6, 0xD46D, WORD_LEN, 0}, 	// P_B_P2Q1                       
	{0x36D8, 0x1633, WORD_LEN, 0}, 	// P_B_P2Q2                       
	{0x36DA, 0x3051, WORD_LEN, 0}, 	// P_B_P2Q3                       
	{0x36DC, 0x86B3, WORD_LEN, 0}, 	// P_B_P2Q4                       
	{0x36DE, 0x1CD1, WORD_LEN, 0}, 	// P_G2_P2Q0                      
	{0x36E0, 0xD14C, WORD_LEN, 0}, 	// P_G2_P2Q1                      
	{0x36E2, 0x62D2, WORD_LEN, 0}, 	// P_G2_P2Q2                      
	{0x36E4, 0x3F50, WORD_LEN, 0}, 	// P_G2_P2Q3                      
	{0x36E6, 0x9AB2, WORD_LEN, 0}, 	// P_G2_P2Q4                      
	{0x3700, 0xFC2E, WORD_LEN, 0}, 	// P_G1_P3Q0                      
	{0x3702, 0xDAC9, WORD_LEN, 0}, 	// P_G1_P3Q1                      
	{0x3704, 0x8BD0, WORD_LEN, 0}, 	// P_G1_P3Q2                      
	{0x3706, 0x9D10, WORD_LEN, 0}, 	// P_G1_P3Q3                      
	{0x3708, 0xC16F, WORD_LEN, 0}, 	// P_G1_P3Q4                      
	{0x370A, 0x854E, WORD_LEN, 0}, 	// P_R_P3Q0                       
	{0x370C, 0x91CE, WORD_LEN, 0}, 	// P_R_P3Q1                       
	{0x370E, 0x9B91, WORD_LEN, 0}, 	// P_R_P3Q2                       
	{0x3710, 0x5B2C, WORD_LEN, 0}, 	// P_R_P3Q3                       
	{0x3712, 0x6730, WORD_LEN, 0}, 	// P_R_P3Q4                       
	{0x3714, 0x12AD, WORD_LEN, 0}, 	// P_B_P3Q0                       
	{0x3716, 0x60CF, WORD_LEN, 0}, 	// P_B_P3Q1                       
	{0x3718, 0x8AAC, WORD_LEN, 0}, 	// P_B_P3Q2                       
	{0x371A, 0x9E91, WORD_LEN, 0}, 	// P_B_P3Q3                       
	{0x371C, 0xC86F, WORD_LEN, 0}, 	// P_B_P3Q4                       
	{0x371E, 0x9E6E, WORD_LEN, 0}, 	// P_G2_P3Q0                      
	{0x3720, 0x22AD, WORD_LEN, 0}, 	// P_G2_P3Q1                      
	{0x3722, 0xA370, WORD_LEN, 0}, 	// P_G2_P3Q2                      
	{0x3724, 0xD36D, WORD_LEN, 0}, 	// P_G2_P3Q3                      
	{0x3726, 0x24B1, WORD_LEN, 0}, 	// P_G2_P3Q4                      
	{0x3740, 0xDDA6, WORD_LEN, 0}, 	// P_G1_P4Q0                      
	{0x3742, 0x10D1, WORD_LEN, 0}, 	// P_G1_P4Q1                      
	{0x3744, 0x6CF3, WORD_LEN, 0}, 	// P_G1_P4Q2                      
	{0x3746, 0xD173, WORD_LEN, 0}, 	// P_G1_P4Q3                      
	{0x3748, 0xD5F6, WORD_LEN, 0}, 	// P_G1_P4Q4                      
	{0x374A, 0x79F1, WORD_LEN, 0}, 	// P_R_P4Q0                       
	{0x374C, 0x4DD0, WORD_LEN, 0}, 	// P_R_P4Q1                       
	{0x374E, 0x2EB2, WORD_LEN, 0}, 	// P_R_P4Q2                       
	{0x3750, 0xC553, WORD_LEN, 0}, 	// P_R_P4Q3                       
	{0x3752, 0x81F7, WORD_LEN, 0}, 	// P_R_P4Q4                       
	{0x3754, 0x24F1, WORD_LEN, 0}, 	// P_B_P4Q0                       
	{0x3756, 0x46D1, WORD_LEN, 0}, 	// P_B_P4Q1                       
	{0x3758, 0x1B31, WORD_LEN, 0}, 	// P_B_P4Q2                       
	{0x375A, 0x9AB4, WORD_LEN, 0}, 	// P_B_P4Q3                       
	{0x375C, 0x9FB6, WORD_LEN, 0}, 	// P_B_P4Q4                       
	{0x375E, 0x0FCD, WORD_LEN, 0}, 	// P_G2_P4Q0                      
	{0x3760, 0x2191, WORD_LEN, 0}, 	// P_G2_P4Q1                      
	{0x3762, 0x56D3, WORD_LEN, 0}, 	// P_G2_P4Q2                      
	{0x3764, 0xD5D3, WORD_LEN, 0}, 	// P_G2_P4Q3                      
	{0x3766, 0xD376, WORD_LEN, 0}, 	// P_G2_P4Q4                      
	{0x3782, 0x01F4, WORD_LEN, 0}, 	// CENTER_ROW                     
	{0x3784, 0x0294, WORD_LEN, 0}, 	// CENTER_COLUMN                  
	{0x37C0, 0xD1C9, WORD_LEN, 0}, 	
	{0x37C2, 0x9B0A, WORD_LEN, 0}, 	
	{0x37C4, 0x9F0B, WORD_LEN, 0}, 	
	{0x37C6, 0x94AA, WORD_LEN, 0}, 	
	{0xC95E, 0x0000, WORD_LEN, 0}, 	// CAM_PGA_PGA_CONTROL
	{0xC95E, 0x0001, WORD_LEN, 0}, 	// CAM_PGA_PGA_CONTROL
  #endif //MT9M114_REV2_0824
#else	
	{0xC95E, 0x0002, WORD_LEN, 0}, 	// CAM_PGA_PGA_CONTROL            
	{0x3640, 0x00B0, WORD_LEN, 0}, 	// P_G1_P0Q0                      
	{0x3642, 0x274B, WORD_LEN, 0}, 	// P_G1_P0Q1                      
	{0x3644, 0x04D1, WORD_LEN, 0}, 	// P_G1_P0Q2                      
	{0x3646, 0x63AA, WORD_LEN, 0}, 	// P_G1_P0Q3                      
	{0x3648, 0xB74D, WORD_LEN, 0}, 	// P_G1_P0Q4                      
	{0x364A, 0x00D0, WORD_LEN, 0}, 	// P_R_P0Q0                       
	{0x364C, 0x012B, WORD_LEN, 0}, 	// P_R_P0Q1                       
	{0x364E, 0x1071, WORD_LEN, 0}, 	// P_R_P0Q2                       
	{0x3650, 0x55EB, WORD_LEN, 0}, 	// P_R_P0Q3                       
	{0x3652, 0x9F4D, WORD_LEN, 0}, 	// P_R_P0Q4                       
	{0x3654, 0x0170, WORD_LEN, 0}, 	// P_B_P0Q0                       
	{0x3656, 0x64EC, WORD_LEN, 0}, 	// P_B_P0Q1                       
	{0x3658, 0x4ED0, WORD_LEN, 0}, 	// P_B_P0Q2                       
	{0x365A, 0xBB2B, WORD_LEN, 0}, 	// P_B_P0Q3                       
	{0x365C, 0xCEAD, WORD_LEN, 0}, 	// P_B_P0Q4                       
	{0x365E, 0x00B0, WORD_LEN, 0}, 	// P_G2_P0Q0                      
	{0x3660, 0x15CA, WORD_LEN, 0}, 	// P_G2_P0Q1                      
	{0x3662, 0x09B1, WORD_LEN, 0}, 	// P_G2_P0Q2                      
	{0x3664, 0x45A9, WORD_LEN, 0}, 	// P_G2_P0Q3                      
	{0x3666, 0x818E, WORD_LEN, 0}, 	// P_G2_P0Q4                      
	{0x3680, 0xA4CA, WORD_LEN, 0}, 	// P_G1_P1Q0                      
	{0x3682, 0x81C9, WORD_LEN, 0}, 	// P_G1_P1Q1                      
	{0x3684, 0x55AF, WORD_LEN, 0}, 	// P_G1_P1Q2                      
	{0x3686, 0x1C4E, WORD_LEN, 0}, 	// P_G1_P1Q3                      
	{0x3688, 0x8070, WORD_LEN, 0}, 	// P_G1_P1Q4                      
	{0x368A, 0xA76B, WORD_LEN, 0}, 	// P_R_P1Q0                       
	{0x368C, 0x386B, WORD_LEN, 0}, 	// P_R_P1Q1                       
	{0x368E, 0x75EE, WORD_LEN, 0}, 	// P_R_P1Q2                       
	{0x3690, 0x718C, WORD_LEN, 0}, 	// P_R_P1Q3                       
	{0x3692, 0xDD6E, WORD_LEN, 0}, 	// P_R_P1Q4                       
	{0x3694, 0xEE07, WORD_LEN, 0}, 	// P_B_P1Q0                       
	{0x3696, 0xA1EA, WORD_LEN, 0}, 	// P_B_P1Q1                       
	{0x3698, 0x304C, WORD_LEN, 0}, 	// P_B_P1Q2                       
	{0x369A, 0xA2AA, WORD_LEN, 0}, 	// P_B_P1Q3                       
	{0x369C, 0x8E0C, WORD_LEN, 0}, 	// P_B_P1Q4                       
	{0x369E, 0x52CA, WORD_LEN, 0}, 	// P_G2_P1Q0                      
	{0x36A0, 0x202B, WORD_LEN, 0}, 	// P_G2_P1Q1                      
	{0x36A2, 0x3AED, WORD_LEN, 0}, 	// P_G2_P1Q2                      
	{0x36A4, 0x882C, WORD_LEN, 0}, 	// P_G2_P1Q3                      
	{0x36A6, 0xCE0E, WORD_LEN, 0}, 	// P_G2_P1Q4                      
	{0x36C0, 0x0951, WORD_LEN, 0}, 	// P_G1_P2Q0                      
	{0x36C2, 0x606A, WORD_LEN, 0}, 	// P_G1_P2Q1                      
	{0x36C4, 0x3211, WORD_LEN, 0}, 	// P_G1_P2Q2                      
	{0x36C6, 0x598F, WORD_LEN, 0}, 	// P_G1_P2Q3                      
	{0x36C8, 0x8FB3, WORD_LEN, 0}, 	// P_G1_P2Q4                      
	{0x36CA, 0x1031, WORD_LEN, 0}, 	// P_R_P2Q0                       
	{0x36CC, 0x3DCD, WORD_LEN, 0}, 	// P_R_P2Q1                       
	{0x36CE, 0x30D1, WORD_LEN, 0}, 	// P_R_P2Q2                       
	{0x36D0, 0x098C, WORD_LEN, 0}, 	// P_R_P2Q3                       
	{0x36D2, 0x8F33, WORD_LEN, 0}, 	// P_R_P2Q4                       
	{0x36D4, 0x5E90, WORD_LEN, 0}, 	// P_B_P2Q0                       
	{0x36D6, 0x164E, WORD_LEN, 0}, 	// P_B_P2Q1                       
	{0x36D8, 0x13F0, WORD_LEN, 0}, 	// P_B_P2Q2                       
	{0x36DA, 0x3F2C, WORD_LEN, 0}, 	// P_B_P2Q3                       
	{0x36DC, 0xEB31, WORD_LEN, 0}, 	// P_B_P2Q4                       
	{0x36DE, 0x0A51, WORD_LEN, 0}, 	// P_G2_P2Q0                      
	{0x36E0, 0x322C, WORD_LEN, 0}, 	// P_G2_P2Q1                      
	{0x36E2, 0x3351, WORD_LEN, 0}, 	// P_G2_P2Q2                      
	{0x36E4, 0x444F, WORD_LEN, 0}, 	// P_G2_P2Q3                      
	{0x36E6, 0x9193, WORD_LEN, 0}, 	// P_G2_P2Q4                      
	{0x3700, 0x29AE, WORD_LEN, 0}, 	// P_G1_P3Q0                      
	{0x3702, 0x5D8D, WORD_LEN, 0}, 	// P_G1_P3Q1                      
	{0x3704, 0xDAF1, WORD_LEN, 0}, 	// P_G1_P3Q2                      
	{0x3706, 0x86F0, WORD_LEN, 0}, 	// P_G1_P3Q3                      
	{0x3708, 0x2512, WORD_LEN, 0}, 	// P_G1_P3Q4                      
	{0x370A, 0x632D, WORD_LEN, 0}, 	// P_R_P3Q0                       
	{0x370C, 0xD16D, WORD_LEN, 0}, 	// P_R_P3Q1                       
	{0x370E, 0xF810, WORD_LEN, 0}, 	// P_R_P3Q2                       
	{0x3710, 0x1E0D, WORD_LEN, 0}, 	// P_R_P3Q3                       
	{0x3712, 0x7250, WORD_LEN, 0}, 	// P_R_P3Q4                       
	{0x3714, 0x266E, WORD_LEN, 0}, 	// P_B_P3Q0                       
	{0x3716, 0x402C, WORD_LEN, 0}, 	// P_B_P3Q1                       
	{0x3718, 0x93B1, WORD_LEN, 0}, 	// P_B_P3Q2                       
	{0x371A, 0xA74F, WORD_LEN, 0}, 	// P_B_P3Q3                       
	{0x371C, 0x5DD1, WORD_LEN, 0}, 	// P_B_P3Q4                       
	{0x371E, 0x78EE, WORD_LEN, 0}, 	// P_G2_P3Q0                      
	{0x3720, 0xBDAB, WORD_LEN, 0}, 	// P_G2_P3Q1                      
	{0x3722, 0xB951, WORD_LEN, 0}, 	// P_G2_P3Q2                      
	{0x3724, 0x170E, WORD_LEN, 0}, 	// P_G2_P3Q3                      
	{0x3726, 0x1372, WORD_LEN, 0}, 	// P_G2_P3Q4                      
	{0x3740, 0x17F0, WORD_LEN, 0}, 	// P_G1_P4Q0                      
	{0x3742, 0xB5EC, WORD_LEN, 0}, 	// P_G1_P4Q1                      
	{0x3744, 0xE9F4, WORD_LEN, 0}, 	// P_G1_P4Q2                      
	{0x3746, 0xCF30, WORD_LEN, 0}, 	// P_G1_P4Q3                      
	{0x3748, 0x6636, WORD_LEN, 0}, 	// P_G1_P4Q4                      
	{0x374A, 0x3DD0, WORD_LEN, 0}, 	// P_R_P4Q0                       
	{0x374C, 0x91F0, WORD_LEN, 0}, 	// P_R_P4Q1                       
	{0x374E, 0xE0F4, WORD_LEN, 0}, 	// P_R_P4Q2                       
	{0x3750, 0x6171, WORD_LEN, 0}, 	// P_R_P4Q3                       
	{0x3752, 0x6396, WORD_LEN, 0}, 	// P_R_P4Q4                       
	{0x3754, 0x558F, WORD_LEN, 0}, 	// P_B_P4Q0                       
	{0x3756, 0xF1AF, WORD_LEN, 0}, 	// P_B_P4Q1                       
	{0x3758, 0xA934, WORD_LEN, 0}, 	// P_B_P4Q2                       
	{0x375A, 0x716F, WORD_LEN, 0}, 	// P_B_P4Q3                       
	{0x375C, 0x3976, WORD_LEN, 0}, 	// P_B_P4Q4                       
	{0x375E, 0x73CF, WORD_LEN, 0}, 	// P_G2_P4Q0                      
	{0x3760, 0x982E, WORD_LEN, 0}, 	// P_G2_P4Q1                      
	{0x3762, 0xE5B4, WORD_LEN, 0}, 	// P_G2_P4Q2                      
	{0x3764, 0xE0ED, WORD_LEN, 0}, 	// P_G2_P4Q3                      
	{0x3766, 0x61B6, WORD_LEN, 0}, 	// P_G2_P4Q4                      
	{0x3782, 0x0214, WORD_LEN, 0}, 	// CENTER_ROW                     
	{0x3784, 0x026C, WORD_LEN, 0}, 	// CENTER_COLUMN                  
	{0xC960, 0x0AF0, WORD_LEN, 0}, 	// CAM_PGA_L_CONFIG_COLOUR_TEMP   
	{0xC962, 0x7D31, WORD_LEN, 0}, 	// CAM_PGA_L_CONFIG_GREEN_RED_Q14 
	{0xC964, 0x6BD8, WORD_LEN, 0}, 	// CAM_PGA_L_CONFIG_RED_Q14       
	{0xC966, 0x7E0B, WORD_LEN, 0}, 	// CAM_PGA_L_CONFIG_GREEN_BLUE_Q14
	{0xC968, 0x773C, WORD_LEN, 0}, 	// CAM_PGA_L_CONFIG_BLUE_Q14      
	{0xC96A, 0x0FA0, WORD_LEN, 0}, 	// CAM_PGA_M_CONFIG_COLOUR_TEMP   
	{0xC96C, 0x7FB1, WORD_LEN, 0}, 	// CAM_PGA_M_CONFIG_GREEN_RED_Q14 
	{0xC96E, 0x7F81, WORD_LEN, 0}, 	// CAM_PGA_M_CONFIG_RED_Q14       
	{0xC970, 0x7F9D, WORD_LEN, 0}, 	// CAM_PGA_M_CONFIG_GREEN_BLUE_Q14
	{0xC972, 0x7F6D, WORD_LEN, 0}, 	// CAM_PGA_M_CONFIG_BLUE_Q14      
	{0xC974, 0x1964, WORD_LEN, 0}, 	// CAM_PGA_R_CONFIG_COLOUR_TEMP   
	{0xC976, 0x7C15, WORD_LEN, 0}, 	// CAM_PGA_R_CONFIG_GREEN_RED_Q14 
	{0xC978, 0x6BB4, WORD_LEN, 0}, 	// CAM_PGA_R_CONFIG_RED_Q14       
	{0xC97A, 0x7D13, WORD_LEN, 0}, 	// CAM_PGA_R_CONFIG_GREEN_BLUE_Q14
	{0xC97C, 0x7632, WORD_LEN, 0}, 	// CAM_PGA_R_CONFIG_BLUE_Q14      
	{0xC95E, 0x0003, WORD_LEN, 0}, 	// CAM_PGA_PGA_CONTROL            

	// [Step4-APGA]
	{0x098E, 0x0000, WORD_LEN, 0}, 	// LOGICAL_ADDRESS_ACCESS

	// [Step4-APGA]	
	{0xC95E, 0x0003, WORD_LEN, 0} 	// CAM_PGA_PGA_CONTROL       
#endif	
	
};                                                                                   
                                                                                     

// [Step5-AWB_CCM]1: LOAD=CCM                                                                                     
static const struct mt9m114_i2c_reg_conf const awb_ccm_setup_tbl[] = {    
	{0xC892, 0x0267, WORD_LEN, 0}, 	// CAM_AWB_CCM_L_0      
	{0xC894, 0xFF1A, WORD_LEN, 0}, 	// CAM_AWB_CCM_L_1      
	{0xC896, 0xFFB3, WORD_LEN, 0}, 	// CAM_AWB_CCM_L_2      
	{0xC898, 0xFF80, WORD_LEN, 0}, 	// CAM_AWB_CCM_L_3      
	{0xC89A, 0x0166, WORD_LEN, 0}, 	// CAM_AWB_CCM_L_4      
	{0xC89C, 0x0003, WORD_LEN, 0}, 	// CAM_AWB_CCM_L_5      
	{0xC89E, 0xFF9A, WORD_LEN, 0}, 	// CAM_AWB_CCM_L_6      
	{0xC8A0, 0xFEB4, WORD_LEN, 0}, 	// CAM_AWB_CCM_L_7      
	{0xC8A2, 0x024D, WORD_LEN, 0}, 	// CAM_AWB_CCM_L_8      
	{0xC8A4, 0x01BF, WORD_LEN, 0}, 	// CAM_AWB_CCM_M_0      
	{0xC8A6, 0xFF01, WORD_LEN, 0}, 	// CAM_AWB_CCM_M_1      
	{0xC8A8, 0xFFF3, WORD_LEN, 0}, 	// CAM_AWB_CCM_M_2      
	{0xC8AA, 0xFF75, WORD_LEN, 0}, 	// CAM_AWB_CCM_M_3      
	{0xC8AC, 0x0198, WORD_LEN, 0}, 	// CAM_AWB_CCM_M_4      
	{0xC8AE, 0xFFFD, WORD_LEN, 0}, 	// CAM_AWB_CCM_M_5      
	{0xC8B0, 0xFF9A, WORD_LEN, 0}, 	// CAM_AWB_CCM_M_6      
	{0xC8B2, 0xFEE7, WORD_LEN, 0}, 	// CAM_AWB_CCM_M_7      
	{0xC8B4, 0x02A8, WORD_LEN, 0}, 	// CAM_AWB_CCM_M_8      
	{0xC8B6, 0x01D9, WORD_LEN, 0}, 	// CAM_AWB_CCM_R_0      
	{0xC8B8, 0xFF26, WORD_LEN, 0}, 	// CAM_AWB_CCM_R_1      
	{0xC8BA, 0xFFF3, WORD_LEN, 0}, 	// CAM_AWB_CCM_R_2      
	{0xC8BC, 0xFFB3, WORD_LEN, 0}, 	// CAM_AWB_CCM_R_3      
	{0xC8BE, 0x0132, WORD_LEN, 0}, 	// CAM_AWB_CCM_R_4      
	{0xC8C0, 0xFFE8, WORD_LEN, 0}, 	// CAM_AWB_CCM_R_5      
	{0xC8C2, 0xFFDA, WORD_LEN, 0}, 	// CAM_AWB_CCM_R_6      
	{0xC8C4, 0xFECD, WORD_LEN, 0}, 	// CAM_AWB_CCM_R_7      
	{0xC8C6, 0x02C2, WORD_LEN, 0}, 	// CAM_AWB_CCM_R_8      
	{0xC8C8, 0x0075, WORD_LEN, 0}, 	// CAM_AWB_CCM_L_RG_GAIN
	{0xC8CA, 0x011C, WORD_LEN, 0}, 	// CAM_AWB_CCM_L_BG_GAIN
	{0xC8CC, 0x009A, WORD_LEN, 0}, 	// CAM_AWB_CCM_M_RG_GAIN
	{0xC8CE, 0x0105, WORD_LEN, 0}, 	// CAM_AWB_CCM_M_BG_GAIN
	{0xC8D0, 0x00A4, WORD_LEN, 0}, 	// CAM_AWB_CCM_R_RG_GAIN
	{0xC8D2, 0x00AC, WORD_LEN, 0}, 	// CAM_AWB_CCM_R_BG_GAIN
	{0xC8D4, 0x0A8C, WORD_LEN, 0}, 	// CAM_AWB_CCM_L_CTEMP  
	{0xC8D6, 0x0F0A, WORD_LEN, 0}, 	// CAM_AWB_CCM_M_CTEMP  
	{0xC8D8, 0x1964, WORD_LEN, 0} 	// CAM_AWB_CCM_R_CTEMP       
};                                                                                   

// LOAD=AWB 
static const struct mt9m114_i2c_reg_conf const awb_steup_tbl[] = {             
	{0xC914, 0x0000, WORD_LEN, 0}, 	// CAM_STAT_AWB_CLIP_WINDOW_XSTART
	{0xC916, 0x0000, WORD_LEN, 0}, 	// CAM_STAT_AWB_CLIP_WINDOW_YSTART
	{0xC918, 0x04FF, WORD_LEN, 0}, 	// CAM_STAT_AWB_CLIP_WINDOW_XEND  
	{0xC91A, 0x02CF, WORD_LEN, 0}, 	// CAM_STAT_AWB_CLIP_WINDOW_YEND  
	{0xC904, 0x0033, WORD_LEN, 0}, 	// CAM_AWB_AWB_XSHIFT_PRE_ADJ     
	{0xC906, 0x0040, WORD_LEN, 0}, 	// CAM_AWB_AWB_YSHIFT_PRE_ADJ     
	{0xC8F2, 0x03, BYTE_LEN, 0}, 	// CAM_AWB_AWB_XSCALE             
	{0xC8F3, 0x02, BYTE_LEN, 0}, 	// CAM_AWB_AWB_YSCALE             
	{0xC906, 0x003C, WORD_LEN, 0}, 	// CAM_AWB_AWB_YSHIFT_PRE_ADJ     
	{0xC8F4, 0x0000, WORD_LEN, 0}, 	// CAM_AWB_AWB_WEIGHTS_0          
	{0xC8F6, 0x0000, WORD_LEN, 0}, 	// CAM_AWB_AWB_WEIGHTS_1          
	{0xC8F8, 0x0000, WORD_LEN, 0}, 	// CAM_AWB_AWB_WEIGHTS_2          
	{0xC8FA, 0xE724, WORD_LEN, 0}, 	// CAM_AWB_AWB_WEIGHTS_3          
	{0xC8FC, 0x1583, WORD_LEN, 0}, 	// CAM_AWB_AWB_WEIGHTS_4          
	{0xC8FE, 0x2045, WORD_LEN, 0}, 	// CAM_AWB_AWB_WEIGHTS_5          
	{0xC900, 0x05DC, WORD_LEN, 0}, 	// CAM_AWB_AWB_WEIGHTS_6          
	{0xC902, 0x007C, WORD_LEN, 0}, 	// CAM_AWB_AWB_WEIGHTS_7          
	{0xC90C, 0x80, BYTE_LEN, 0}, 	// CAM_AWB_K_R_L                  
	{0xC90D, 0x80, BYTE_LEN, 0}, 	// CAM_AWB_K_G_L                  
	{0xC90E, 0x80, BYTE_LEN, 0}, 	// CAM_AWB_K_B_L                  
	{0xC90F, 0x88, BYTE_LEN, 0}, 	// CAM_AWB_K_R_R                  
	{0xC910, 0x80, BYTE_LEN, 0}, 	// CAM_AWB_K_G_R                  
	{0xC911, 0x80, BYTE_LEN, 0} 	// CAM_AWB_K_B_R                  
}; 


// LOAD=Step7-CPIPE_Preference	
static const struct mt9m114_i2c_reg_conf const cpipe_preference_steup_tbl[] = {             
	{0xC926, 0x0020, WORD_LEN, 0}, 	// CAM_LL_START_BRIGHTNESS
	{0xC928, 0x009A, WORD_LEN, 0}, 	// CAM_LL_STOP_BRIGHTNESS
	{0xC946, 0x0070, WORD_LEN, 0}, 	// CAM_LL_START_GAIN_METRIC
	{0xC948, 0x00F3, WORD_LEN, 0}, 	// CAM_LL_STOP_GAIN_METRIC
	{0xC952, 0x0020, WORD_LEN, 0}, 	// CAM_LL_START_TARGET_LUMA_BM
	{0xC954, 0x009A, WORD_LEN, 0}, 	// CAM_LL_STOP_TARGET_LUMA_BM
	{0xC92A, 0x80, BYTE_LEN, 0}, 	// CAM_LL_START_SATURATION
	{0xC92B, 0x4B, BYTE_LEN, 0}, 	// CAM_LL_END_SATURATION
	{0xC92C, 0x00, BYTE_LEN, 0}, 	// CAM_LL_START_DESATURATION
	{0xC92D, 0xFF, BYTE_LEN, 0}, 	// CAM_LL_END_DESATURATION
	{0xC92E, 0x3C, BYTE_LEN, 0}, 	// CAM_LL_START_DEMOSAIC
	{0xC92F, 0x02, BYTE_LEN, 0}, 	// CAM_LL_START_AP_GAIN
	{0xC930, 0x06, BYTE_LEN, 0}, 	// CAM_LL_START_AP_THRESH
	{0xC931, 0x64, BYTE_LEN, 0}, 	// CAM_LL_STOP_DEMOSAIC
	{0xC932, 0x01, BYTE_LEN, 0}, 	// CAM_LL_STOP_AP_GAIN
	{0xC933, 0x0C, BYTE_LEN, 0}, 	// CAM_LL_STOP_AP_THRESH
	{0xC934, 0x3C, BYTE_LEN, 0}, 	// CAM_LL_START_NR_RED
	{0xC935, 0x3C, BYTE_LEN, 0}, 	// CAM_LL_START_NR_GREEN
	{0xC936, 0x3C, BYTE_LEN, 0}, 	// CAM_LL_START_NR_BLUE
	{0xC937, 0x0F, BYTE_LEN, 0}, 	// CAM_LL_START_NR_THRESH
	{0xC938, 0x64, BYTE_LEN, 0}, 	// CAM_LL_STOP_NR_RED
	{0xC939, 0x64, BYTE_LEN, 0}, 	// CAM_LL_STOP_NR_GREEN
	{0xC93A, 0x64, BYTE_LEN, 0}, 	// CAM_LL_STOP_NR_BLUE
	{0xC93B, 0x32, BYTE_LEN, 0}, 	// CAM_LL_STOP_NR_THRESH
	{0xC93C, 0x0020, WORD_LEN, 0}, 	// CAM_LL_START_CONTRAST_BM
	{0xC93E, 0x009A, WORD_LEN, 0}, 	// CAM_LL_STOP_CONTRAST_BM
	{0xC940, 0x00DC, WORD_LEN, 0}, 	// CAM_LL_GAMMA
	{0xC942, 0x38, BYTE_LEN, 0}, 	// CAM_LL_START_CONTRAST_GRADIENT
	{0xC943, 0x30, BYTE_LEN, 0}, 	// CAM_LL_STOP_CONTRAST_GRADIENT
	{0xC944, 0x50, BYTE_LEN, 0}, 	// CAM_LL_START_CONTRAST_LUMA_PERCENTAGE
	{0xC945, 0x19, BYTE_LEN, 0}, 	// CAM_LL_STOP_CONTRAST_LUMA_PERCENTAGE
	{0xC94A, 0x0230, WORD_LEN, 0}, 	// CAM_LL_START_FADE_TO_BLACK_LUMA
	{0xC94C, 0x0010, WORD_LEN, 0}, 	// CAM_LL_STOP_FADE_TO_BLACK_LUMA
	{0xC94E, 0x01CD, WORD_LEN, 0}, 	// CAM_LL_CLUSTER_DC_TH_BM
	{0xC950, 0x05, BYTE_LEN, 0}, 	// CAM_LL_CLUSTER_DC_GATE_PERCENTAGE
	{0xC951, 0x40, BYTE_LEN, 0}, 	// CAM_LL_SUMMING_SENSITIVITY_FACTOR
	{0xC87B, 0x1B, BYTE_LEN, 0}, 	// CAM_AET_TARGET_AVERAGE_LUMA_DARK
	{0xC878, 0x0E, BYTE_LEN, 0}, 	// CAM_AET_AEMODE
	{0xC890, 0x0080, WORD_LEN, 0}, 	// CAM_AET_TARGET_GAIN
	{0xC886, 0x0100, WORD_LEN, 0}, 	// CAM_AET_AE_MAX_VIRT_AGAIN
	{0xC87C, 0x005A, WORD_LEN, 0}, 	// CAM_AET_BLACK_CLIPPING_TARGET
	{0xB42A, 0x05, BYTE_LEN, 0}, 	// CCM_DELTA_GAIN
	{0xA80A, 0x20, BYTE_LEN, 0} 	// AE_TRACK_AE_TRACKING_DAMPENING_SPEED            
}; 


// LOAD=Step8-Features
static const struct mt9m114_i2c_reg_conf const features_steup_tbl[] = {             
	{0x098E, 0x0000, WORD_LEN, 0}, 	// LOGICAL_ADDRESS_ACCESS 
	{0xC984, 0x8040, WORD_LEN, 0}, 	// CAM_PORT_OUTPUT_CONTROL
	{0x001E, 0x0777, WORD_LEN, 0} 	// PAD_SLEW                
}; 


// LOAD=MIPI setting for SOC1040
static const struct mt9m114_i2c_reg_conf const mipi_setting_steup_tbl[] = {             
	{0xC984, 0x8041, WORD_LEN, 0}, 	// CAM_PORT_OUTPUT_CONTROL
	{0xC988, 0x0F00, WORD_LEN, 0}, 	// CAM_PORT_MIPI_TIMING_T_HS_ZERO
	{0xC98A, 0x0B07, WORD_LEN, 0}, 	// CAM_PORT_MIPI_TIMING_T_HS_EXIT_HS_TRAIL
	{0xC98C, 0x0D01, WORD_LEN, 0}, 	// CAM_PORT_MIPI_TIMING_T_CLK_POST_CLK_PRE
	{0xC98E, 0x071D, WORD_LEN, 0}, 	// CAM_PORT_MIPI_TIMING_T_CLK_TRAIL_CLK_ZERO
	{0xC990, 0x0006, WORD_LEN, 0}, 	// CAM_PORT_MIPI_TIMING_T_LPX
	{0xC992, 0x0A0C, WORD_LEN, 0}, 	// CAM_PORT_MIPI_TIMING_INIT_TIMING
	{0x3C5A, 0x0009, WORD_LEN, 0} 	// MIPI_DELAY_TRIM           
}; 


// LOAD=Dual Lane Mipi Receiver Init
// LOAD=Enter Suspend
static const struct mt9m114_i2c_reg_conf const dual_mipi_init_enter_suspend_steup_tbl[] = {             
	{0xDC00, 0x40, BYTE_LEN, 0}, 	// SYSMGR_NEXT_STATE                                                                                 
	{0x0080, 0x8002, WORD_LEN, 10}, 	// COMMAND_REGISTER  POLL_FIELD=COMMAND_REGISTER, HOST_COMMAND_1, !=0, DELAY=10, TIMEOUT=100 
	{0xDC00, 0x34, BYTE_LEN, 0}, 	// SYSMGR_NEXT_STATE                                                                                 
	{0x0080, 0x8002, WORD_LEN, 10} 	// COMMAND_REGISTER  POLL_FIELD=COMMAND_REGISTER, HOST_COMMAND_1, !=0, DELAY=10, TIMEOUT=100              
};


//LOAD=Leave Suspend
static const struct mt9m114_i2c_reg_conf const dual_mipi_init_leave_suspend_steup_tbl[] = {             
	{0xDC00, 0x34, BYTE_LEN, 0}, 	// SYSMGR_NEXT_STATE
	{0x0080, 0x8002, WORD_LEN, 10}, 	// COMMAND_REGISTER  POLL_FIELD=COMMAND_REGISTER, HOST_COMMAND_1, !=0, DELAY=10, TIMEOUT=100	
	{0x098E, 0x2802, WORD_LEN, 0}, 	// LOGICAL_ADDRESS_ACCESS
	{0xA802, 0x0008, WORD_LEN, 0}, 	// AE_TRACK_MODE
	{0xC908, 0x01, BYTE_LEN, 0}, 	// CAM_AWB_SKIP_FRAMES
	{0xC879, 0x01, BYTE_LEN, 0}, 	// CAM_AET_SKIP_FRAMES
	{0xC909, 0x02, BYTE_LEN, 0}, 	// CAM_AWB_AWBMODE
	{0xA80A, 0x18, BYTE_LEN, 0}, 	// AE_TRACK_AE_TRACKING_DAMPENING_SPEED
	{0xA80B, 0x18, BYTE_LEN, 0}, 	// AE_TRACK_AE_DAMPENING_SPEED
	{0xAC16, 0x18, BYTE_LEN, 0}, 	// AWB_PRE_AWB_RATIOS_TRACKING_SPEED
	{0xC878, 0x0E, BYTE_LEN, 0}, 	// CAM_AET_AEMODE
	{0xDC00, 0x28, BYTE_LEN, 0}, 	// SYSMGR_NEXT_STATE
	{0x0080, 0x8002, WORD_LEN, 10} 	// COMMAND_REGISTER  POLL_FIELD=COMMAND_REGISTER, HOST_COMMAND_1, !=0, DELAY=10, TIMEOUT=100 
}; 

//[VGA]
static const struct mt9m114_i2c_reg_conf const mt9m114_vga_steup_tbl[] = {             
	{0x098E, 0x4868, WORD_LEN, 0}, 	// LOGICAL_ADDRESS_ACCESS [CAM_OUTPUT_WIDTH]
	{0xC868, 0x0280, WORD_LEN, 0}, 	// CAM_OUTPUT_WIDTH                         
	{0xC86A, 0x01E0, WORD_LEN, 0}, 	// CAM_OUTPUT_HEIGHT                        
	{0xC85C, 0x03, BYTE_LEN, 0}, 	// CAM_CROP_CROPMODE                        
	{0xC854, 0x0000, WORD_LEN, 0}, 	// CAM_CROP_WINDOW_XOFFSET                  
	{0xC856, 0x0000, WORD_LEN, 0}, 	// CAM_CROP_WINDOW_YOFFSET                  
	{0xC858, 0x0500, WORD_LEN, 0}, 	// CAM_CROP_WINDOW_WIDTH                    
	{0xC85A, 0x03C0, WORD_LEN, 0}, 	// CAM_CROP_WINDOW_HEIGHT                   
#ifdef MT9M114_REV2
	{0xC88C, 0x1E02, WORD_LEN, 0}, 	// CAM_AET_MAX_FRAME_RATE
	{0xC88E, 0x1E02, WORD_LEN, 0}, 	// CAM_AET_MIN_FRAME_RATE
#endif	
	{0xC914, 0x0000, WORD_LEN, 0}, 	// CAM_STAT_AWB_CLIP_WINDOW_XSTART          
	{0xC916, 0x0000, WORD_LEN, 0}, 	// CAM_STAT_AWB_CLIP_WINDOW_YSTART          
	{0xC918, 0x027F, WORD_LEN, 0}, 	// CAM_STAT_AWB_CLIP_WINDOW_XEND            
	{0xC91A, 0x01DF, WORD_LEN, 0}, 	// CAM_STAT_AWB_CLIP_WINDOW_YEND            
	{0xC91C, 0x0000, WORD_LEN, 0}, 	// CAM_STAT_AE_INITIAL_WINDOW_XSTART        
	{0xC91E, 0x0000, WORD_LEN, 0}, 	// CAM_STAT_AE_INITIAL_WINDOW_YSTART        
	{0xC920, 0x007F, WORD_LEN, 0}, 	// CAM_STAT_AE_INITIAL_WINDOW_XEND          
	{0xC922, 0x005F, WORD_LEN, 0}, 	// CAM_STAT_AE_INITIAL_WINDOW_YEND          
	{0xDC00, 0x28, BYTE_LEN, 0}, 	// SYSMGR_NEXT_STATE                        
	{0x0080, 0x8002, WORD_LEN, 0} 	// COMMAND_REGISTER                           
}; 


//[720P]
static const struct mt9m114_i2c_reg_conf const mt9m114_720p_steup_tbl[] = {             
	{0x098E, 0x4868, WORD_LEN, 0}, 	// LOGICAL_ADDRESS_ACCESS [CAM_OUTPUT_WIDTH]
	{0xC868, 0x0500, WORD_LEN, 0}, 	// CAM_OUTPUT_WIDTH
	{0xC86A, 0x02D0, WORD_LEN, 0}, 	// CAM_OUTPUT_HEIGHT
	{0xC85C, 0x03, BYTE_LEN, 0}, 	// CAM_CROP_CROPMODE
	{0xC854, 0x0000, WORD_LEN, 0}, 	// CAM_CROP_WINDOW_XOFFSET
	{0xC856, 0x0078, WORD_LEN, 0}, 	// CAM_CROP_WINDOW_YOFFSET
	{0xC858, 0x0500, WORD_LEN, 0}, 	// CAM_CROP_WINDOW_WIDTH
	{0xC85A, 0x02D0, WORD_LEN, 0}, 	// CAM_CROP_WINDOW_HEIGHT
#ifdef MT9M114_REV2
	{0xC88C, 0x1E02, WORD_LEN, 0}, 	// CAM_AET_MAX_FRAME_RATE
	{0xC88E, 0x1E02, WORD_LEN, 0}, 	// CAM_AET_MIN_FRAME_RATE
#endif	
	{0xC914, 0x0000, WORD_LEN, 0}, 	// CAM_STAT_AWB_CLIP_WINDOW_XSTART
	{0xC916, 0x0000, WORD_LEN, 0}, 	// CAM_STAT_AWB_CLIP_WINDOW_YSTART
	{0xC918, 0x04FF, WORD_LEN, 0}, 	// CAM_STAT_AWB_CLIP_WINDOW_XEND
	{0xC91A, 0x02CF, WORD_LEN, 0}, 	// CAM_STAT_AWB_CLIP_WINDOW_YEND
	{0xC91C, 0x0000, WORD_LEN, 0}, 	// CAM_STAT_AE_INITIAL_WINDOW_XSTART
	{0xC91E, 0x0000, WORD_LEN, 0}, 	// CAM_STAT_AE_INITIAL_WINDOW_YSTART
	{0xC920, 0x00FF, WORD_LEN, 0}, 	// CAM_STAT_AE_INITIAL_WINDOW_XEND
	{0xC922, 0x008F, WORD_LEN, 0}, 	// CAM_STAT_AE_INITIAL_WINDOW_YEND
	{0xDC00, 0x28, BYTE_LEN, 0}, 	// SYSMGR_NEXT_STATE
	{0x0080, 0x8002, WORD_LEN, 0} 	// COMMAND_REGISTER                
}; 
   
//[1280x960: fixed to 30FPS]                                                                                    
static const struct mt9m114_i2c_reg_conf const mt9m114_1280x960_steup_tbl[] = {             
	{0x098E, 0x4868, WORD_LEN, 0}, 	// LOGICAL_ADDRESS_ACCESS [CAM_OUTPUT_WIDTH]
	{0xC868, 0x0500, WORD_LEN, 0}, 	// CAM_OUTPUT_WIDTH
	{0xC86A, 0x03C0, WORD_LEN, 0}, 	// CAM_OUTPUT_HEIGHT
	{0xC85C, 0x03, BYTE_LEN, 0}, 	// CAM_CROP_CROPMODE
	{0xC854, 0x0000, WORD_LEN, 0}, 	// CAM_CROP_WINDOW_XOFFSET
	{0xC856, 0x0000, WORD_LEN, 0}, 	// CAM_CROP_WINDOW_YOFFSET
	{0xC858, 0x0500, WORD_LEN, 0}, 	// CAM_CROP_WINDOW_WIDTH
	{0xC85A, 0x03C0, WORD_LEN, 0}, 	// CAM_CROP_WINDOW_HEIGHT
#ifdef MT9M114_REV2
	{0xC88C, 0x1E02, WORD_LEN, 0}, 	// CAM_AET_MAX_FRAME_RATE
	//{0xC88E, 0x1E02, WORD_LEN, 0}, 	// CAM_AET_MIN_FRAME_RATE, fix to 30
	{0xC88E, 0x0780, WORD_LEN, 0}, 	// CAM_AET_MIN_FRAME_RATE, var from 7.5 to 30
#endif	
	{0xC914, 0x0000, WORD_LEN, 0}, 	// CAM_STAT_AWB_CLIP_WINDOW_XSTART
	{0xC916, 0x0000, WORD_LEN, 0}, 	// CAM_STAT_AWB_CLIP_WINDOW_YSTART
	{0xC918, 0x04FF, WORD_LEN, 0}, 	// CAM_STAT_AWB_CLIP_WINDOW_XEND
	{0xC91A, 0x03BF, WORD_LEN, 0}, 	// CAM_STAT_AWB_CLIP_WINDOW_YEND
	{0xC91C, 0x0000, WORD_LEN, 0}, 	// CAM_STAT_AE_INITIAL_WINDOW_XSTART
	{0xC91E, 0x0000, WORD_LEN, 0}, 	// CAM_STAT_AE_INITIAL_WINDOW_YSTART
	{0xC920, 0x00FF, WORD_LEN, 0}, 	// CAM_STAT_AE_INITIAL_WINDOW_XEND
	{0xC922, 0x00BF, WORD_LEN, 0}, 	// CAM_STAT_AE_INITIAL_WINDOW_YEND
	{0xDC00, 0x28, BYTE_LEN, 0}, 	// SYSMGR_NEXT_STATE
	{0x0080, 0x8002, WORD_LEN, 0} 	// COMMAND_REGISTER                
}; 

//[Soft-Reset]
static const struct mt9m114_i2c_reg_conf const soft_reset_steup_tbl[] = {             
	{0x001A, 0x0001, WORD_LEN, 0}, 	// FIELD_WR= RESET_AND_MISC_CONTROL, 0x0001
	{0x001A, 0x0000, WORD_LEN, 0} 	// FIELD_WR= RESET_AND_MISC_CONTROL, 0x0000     
};                                                                                  
                                                                                     
struct mt9m114_reg mt9m114_regs = {
	.pll_tbl = pll_setup_tbl,
	.pll_tbl_size = ARRAY_SIZE(pll_setup_tbl),
	.timing_setting_tbl = timing_setting_setup_tbl,
	.timing_setting_tbl_size = ARRAY_SIZE(timing_setting_setup_tbl),
	.sensor_optimization_tbl = sensor_optimization_setup_tbl,
	.sensor_optimization_tbl_size = ARRAY_SIZE(sensor_optimization_setup_tbl),
	.apga_tbl = apga_setup_tbl,
	.apga_tbl_size = ARRAY_SIZE(apga_setup_tbl),	
	.awb_ccm_tbl = awb_ccm_setup_tbl,
	.awb_ccm_tbl_size = ARRAY_SIZE(awb_ccm_setup_tbl),		
	.awb_tbl = awb_steup_tbl,
	.awb_tbl_size = ARRAY_SIZE(awb_steup_tbl),
	.cpipe_preference_tbl = cpipe_preference_steup_tbl,
	.cpipe_preference_tbl_size = ARRAY_SIZE(cpipe_preference_steup_tbl),
	.features_tbl = features_steup_tbl,
	.features_tbl_size = ARRAY_SIZE(features_steup_tbl),
	.mipi_setting_tbl = mipi_setting_steup_tbl,
	.mipi_setting_tbl_size = ARRAY_SIZE(mipi_setting_steup_tbl),
	.dual_mipi_init_enter_suspend_tbl = dual_mipi_init_enter_suspend_steup_tbl,
	.dual_mipi_init_enter_suspend_tbl_size = ARRAY_SIZE(dual_mipi_init_enter_suspend_steup_tbl),
	.dual_mipi_init_leave_suspend_tbl = dual_mipi_init_leave_suspend_steup_tbl,
	.dual_mipi_init_leave_suspend_tbl_size = ARRAY_SIZE(dual_mipi_init_leave_suspend_steup_tbl),
	.mt9m114_vga_tbl = mt9m114_vga_steup_tbl,
	.mt9m114_vga_tbl_size = ARRAY_SIZE(mt9m114_vga_steup_tbl),
	.mt9m114_720p_tbl = mt9m114_720p_steup_tbl,
	.mt9m114_720p_tbl_size = ARRAY_SIZE(mt9m114_720p_steup_tbl),
	.mt9m114_1280x960_tbl = mt9m114_1280x960_steup_tbl,
	.mt9m114_1280x960_tbl_size = ARRAY_SIZE(mt9m114_1280x960_steup_tbl),
	.soft_reset_tbl = soft_reset_steup_tbl,
	.soft_reset_tbl_size = ARRAY_SIZE(soft_reset_steup_tbl)
};      
     
