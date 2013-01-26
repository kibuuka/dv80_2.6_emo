/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
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
#include <mach/cci_hw_id.h>//CH EVT2 MIPI Samsung brightness setting

static struct msm_panel_info pinfo;

static struct mipi_dsi_phy_ctrl dsi_cmd_mode_phy_db = {
/* DSI_BIT_CLK at 500MHz, 2 lane, RGB888 */
		{0x03, 0x01, 0x01, 0x00},	/* regulator */
		/* timing   */
		{0xB4, 0x8D, 0x1D, 0x00, 0x20, 0x94, 0x20,
		0x8F, 0x20, 0x03, 0x04},
		{0x7f, 0x00, 0x00, 0x00},	/* phy ctrl */
		{0xee, 0x02, 0x86, 0x00},	/* strength */
		/* pll control */
		{0x40, 0xf9, 0xb0, 0xda, 0x00, 0x50, 0x48, 0x63,
#if defined(SAMSUNG_TWO_LANE)
		0x30, 0x07, 0x03,
#else           /* default set to 1 lane */
		0x30, 0x07, 0x07,
#endif
		0x05, 0x14, 0x03, 0x0, 0x0, 0x54, 0x06, 0x10, 0x04, 0x0},
};

static int __init mipi_cmd_samsung_qhd_pt_init(void)
{
	int ret;

#ifdef CONFIG_FB_MSM_MIPI_PANEL_DETECT
	if (msm_fb_detect_client("mipi_cmd_samsung_qhd"))
		return 0;
#endif

	pinfo.xres = 540;
	pinfo.yres = 960;
	pinfo.type = MIPI_CMD_PANEL;
	pinfo.pdest = DISPLAY_1;
	pinfo.wait_cycle = 0;
	pinfo.bpp = 24;
	pinfo.lcdc.h_back_porch = 50;
	pinfo.lcdc.h_front_porch = 50;
	pinfo.lcdc.h_pulse_width = 20;
	pinfo.lcdc.v_back_porch = 11;
	pinfo.lcdc.v_front_porch = 10;
	pinfo.lcdc.v_pulse_width = 5;
	pinfo.lcdc.border_clr = 0;	/* blk */
	pinfo.lcdc.underflow_clr = 0xff;	/* blue */
	pinfo.lcdc.hsync_skew = 0;
	//B: CH EVT2 MIPI Samsung brightness setting
	if((cci_hw_id != EVT0) && (cci_hw_id != EVT1))
	pinfo.bl_max = 18;//change levels
	else
	pinfo.bl_max = 15;
	//E: CH EVT2 MIPI Samsung brightness setting
	pinfo.bl_min = 1;
	pinfo.fb_num = 2;
	pinfo.clk_rate = 500000000;//454000000; //LCM Shift

//Enable VSYNC for Camer tearing issue--Taylor--Begin--20111031
#ifdef USE_HW_VSYNC
	pinfo.lcd.vsync_enable = TRUE;
	pinfo.lcd.hw_vsync_mode = TRUE;
#endif

	pinfo.lcd.v_back_porch=pinfo.lcdc.v_back_porch;
	pinfo.lcd.v_front_porch=pinfo.lcdc.v_front_porch;
	pinfo.lcd.v_pulse_width=pinfo.lcdc.v_pulse_width;

//Enable VSYNC for Camer tearing issue--Taylor--End--20111031	
	pinfo.lcd.refx100 = 6000; /* adjust refx100 to prevent tearing */

	pinfo.mipi.mode = DSI_CMD_MODE;
	pinfo.mipi.dst_format = DSI_CMD_DST_FORMAT_RGB888;
	pinfo.mipi.vc = 0;
	pinfo.mipi.rgb_swap = DSI_RGB_SWAP_BGR;
	pinfo.mipi.data_lane0 = TRUE;
#if defined(SAMSUNG_TWO_LANE)
	pinfo.mipi.data_lane1 = TRUE;
#endif
	pinfo.mipi.t_clk_post = 0x22;
	pinfo.mipi.t_clk_pre = 0x3f;
	pinfo.mipi.stream = 0;	/* dma_p */
	pinfo.mipi.mdp_trigger = DSI_CMD_TRIGGER_SW;
	pinfo.mipi.dma_trigger = DSI_CMD_TRIGGER_SW;
	pinfo.mipi.te_sel = 1; /* TE from vsycn gpio */
	pinfo.mipi.interleave_max = 1;
	pinfo.mipi.insert_dcs_cmd = TRUE;
	pinfo.mipi.wr_mem_continue = 0x3c;
	pinfo.mipi.wr_mem_start = 0x2c;
	pinfo.mipi.dsi_phy_db = &dsi_cmd_mode_phy_db;

	ret = mipi_samsung_device_register(&pinfo, MIPI_DSI_PRIM,
						MIPI_DSI_PANEL_WVGA_PT);
	if (ret)
		pr_err("%s: failed to register device!\n", __func__);

	return ret;
}

module_init(mipi_cmd_samsung_qhd_pt_init);