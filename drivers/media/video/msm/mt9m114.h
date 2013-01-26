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

#ifndef CAMSENSOR_MT9M114
#define CAMSENSOR_MT9M114

#include <linux/types.h>
#include <mach/camera.h>

extern struct mt9m114_reg mt9m114_regs;

enum mt9m114_width {
	WORD_LEN,
	BYTE_LEN
};

struct mt9m114_i2c_reg_conf {
	unsigned short waddr;
	unsigned short wdata;
	enum mt9m114_width width;
	unsigned short mdelay_time;
};

struct mt9m114_reg {
	const struct mt9m114_i2c_reg_conf *pll_tbl;
	uint16_t pll_tbl_size;	
	const struct mt9m114_i2c_reg_conf *timing_setting_tbl;
	uint16_t timing_setting_tbl_size;	
	const struct mt9m114_i2c_reg_conf *sensor_optimization_tbl;
	uint16_t sensor_optimization_tbl_size;		
	const struct mt9m114_i2c_reg_conf *apga_tbl;
	uint16_t apga_tbl_size;	
	const struct mt9m114_i2c_reg_conf *awb_ccm_tbl;
	uint16_t awb_ccm_tbl_size;	
	const struct mt9m114_i2c_reg_conf *awb_tbl;
	uint16_t awb_tbl_size;		
	const struct mt9m114_i2c_reg_conf *cpipe_preference_tbl;
	uint16_t cpipe_preference_tbl_size;	
	const struct mt9m114_i2c_reg_conf *features_tbl;
	uint16_t features_tbl_size;	
	const struct mt9m114_i2c_reg_conf *mipi_setting_tbl;
	uint16_t mipi_setting_tbl_size;	
	const struct mt9m114_i2c_reg_conf *dual_mipi_init_enter_suspend_tbl;
	uint16_t dual_mipi_init_enter_suspend_tbl_size;	
	const struct mt9m114_i2c_reg_conf *dual_mipi_init_leave_suspend_tbl;
	uint16_t dual_mipi_init_leave_suspend_tbl_size;	
	const struct mt9m114_i2c_reg_conf *mt9m114_vga_tbl;
	uint16_t mt9m114_vga_tbl_size;	
	const struct mt9m114_i2c_reg_conf *mt9m114_720p_tbl;
	uint16_t mt9m114_720p_tbl_size;	
	const struct mt9m114_i2c_reg_conf *mt9m114_1280x960_tbl;
	uint16_t mt9m114_1280x960_tbl_size;	
	const struct mt9m114_i2c_reg_conf *soft_reset_tbl;
	uint16_t soft_reset_tbl_size;	
};
#endif /* CAMSENSOR_MT9M114 */
