/* arch/arm/mach-omap2/board-gokey.c
 *
 * Copyright (C) 2011 Samsung Electronics Co, Ltd.
 *
 * Based on mach-omap2/board-tuna.c
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/ion.h>
#include <linux/memblock.h>
#include <linux/omap_ion.h>
#include <linux/ramoops.h>
#include <linux/reboot.h>
#include <linux/sysfs.h>

#include <plat/board.h>
#include <plat/common.h>
#include <plat/cpu.h>
#include <plat/remoteproc.h>
#include <plat/usb.h>

#include <mach/dmm.h>
#include <mach/omap4-common.h>
#include <mach/id.h>
#ifdef CONFIG_ION_OMAP
#include <mach/omap4_ion.h>
#endif

#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#include "board-gokey.h"
#include "control.h"
#include "mux.h"
#include "omap4-sar-layout.h"
#include "omap_muxtbl.h"

#include "sec_common.h"
#include "sec_debug.h"
#include "sec_getlog.h"
#include "sec_muxtbl.h"
#include "sec_log_buf.h"

#ifdef CONFIG_MP3_LP_MODE
#include <linux/earlysuspend.h>
#include <mach/cpufreq_limits.h>
#include <linux/cpu.h>
#include <linux/usb/otg.h>

#define PM_LPMODE_DVFS_FREQ	300000
#endif

#define GOKEY_MEM_BANK_0_SIZE	0x20000000
#define GOKEY_MEM_BANK_0_ADDR	0x80000000
#define GOKEY_MEM_BANK_1_SIZE	0x20000000
#define GOKEY_MEM_BANK_1_ADDR	0xA0000000

#define GOKEY_RAMCONSOLE_START	(PLAT_PHYS_OFFSET + SZ_512M)
#define GOKEY_RAMCONSOLE_SIZE	SZ_2M
#define GOKEY_RAMOOPS_START		(GOKEY_RAMCONSOLE_START + \
					 GOKEY_RAMCONSOLE_SIZE)
#define GOKEY_RAMOOPS_SIZE		SZ_1M

#ifdef CONFIG_MP3_LP_MODE
struct cpufreq_lpmode_info cpufreq_lpmode;
#endif

static struct resource ramconsole_resources[] = {
	{
	 .flags = IORESOURCE_MEM,
	 .start = GOKEY_RAMCONSOLE_START,
	 .end = GOKEY_RAMCONSOLE_START + GOKEY_RAMCONSOLE_SIZE - 1,
	 },
};

static struct platform_device ramconsole_device = {
	.name = "ram_console",
	.id = -1,
	.num_resources = ARRAY_SIZE(ramconsole_resources),
	.resource = ramconsole_resources,
};

static struct ramoops_platform_data ramoops_pdata = {
	.mem_size = GOKEY_RAMOOPS_SIZE,
	.mem_address = GOKEY_RAMOOPS_START,
	.record_size = SZ_32K,
	.dump_oops = 0,		/* only for panic */
};

static struct platform_device ramoops_device = {
	.name = "ramoops",
	.dev = {
		.platform_data = &ramoops_pdata,
		},
};

static struct platform_device bcm4334_bluetooth_device = {
	.name = "bcm4334_bluetooth",
	.id = -1,
};

static struct platform_device *gokey_dbg_devices[] __initdata = {
	&ramconsole_device,
	&ramoops_device,
};

static struct platform_device *gokey_devices[] __initdata = {
	&bcm4334_bluetooth_device,
};
static void omap4_gokey_early_init(void)
{
	struct omap_hwmod *uart4_hwmod;
	struct omap_hwmod *mcbsp3_hwmod;

	/* correct uart4 hwmod flag settings for gokey board. */
	uart4_hwmod = omap_hwmod_lookup("uart4");
	if (likely(uart4_hwmod))
		uart4_hwmod->flags = HWMOD_SWSUP_SIDLE;
	/* correct mcbsp3 hwmod flag settings for gokey board. */

	mcbsp3_hwmod = omap_hwmod_lookup("mcbsp3");
	if (likely(mcbsp3_hwmod))
		uart4_hwmod->flags = HWMOD_SWSUP_SIDLE;

}

static void __init gokey_init_early(void)
{
	omap2_init_common_infrastructure();
	omap2_init_common_devices(NULL, NULL);

	omap4_gokey_display_early_init();
	omap4_gokey_early_init();
}

static struct omap_musb_board_data musb_board_data = {
	.interface_type = MUSB_INTERFACE_UTMI,
#ifdef CONFIG_USB_MUSB_OTG
	.mode = MUSB_OTG,
#else
	.mode = MUSB_PERIPHERAL,
#endif
	.power = 200,
};

#ifdef CONFIG_MP3_LP_MODE
static void board_gokey_early_suspend(struct early_suspend *h)
{
	unsigned int cur;

	if (!cpufreq_lpmode.wifi_enabled  && !cpufreq_lpmode.bt_enabled
		&& is_playback_lpmode_available()
		&& (gokey_get_charging_type() != USB_EVENT_VBUS)) {

		cpufreq_lpmode.lp_mode_enabled = true;

		omap_cpufreq_min_limit(DVFS_LOCK_ID_PM, PM_LPMODE_DVFS_FREQ);
		omap_cpufreq_max_limit(DVFS_LOCK_ID_PM, PM_LPMODE_DVFS_FREQ);

		pr_info("%s: lp_mode clock limit is set\n", __func__);
	} else
		cpufreq_lpmode.lp_mode_enabled = false;
}

static void board_gokey_late_resume(struct early_suspend *h)
{
	unsigned int cur;

	if (cpufreq_lpmode.lp_mode_enabled) {

		omap_cpufreq_max_limit_free(DVFS_LOCK_ID_PM);
		omap_cpufreq_min_limit_free(DVFS_LOCK_ID_PM);
		pr_info("%s: lp_mode clock is free\n", __func__);
		cpufreq_lpmode.lp_mode_enabled = false;
	}
}

static struct early_suspend board_gokey_early_suspend_handler = {
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 60,
	.suspend = board_gokey_early_suspend,
	.resume = board_gokey_late_resume,
};
#endif
static void __init gokey_init(void)
{

#ifdef CONFIG_MP3_LP_MODE
	register_early_suspend(&board_gokey_early_suspend_handler);
#endif

	sec_common_init_early();

	omap4_gokey_emif_init();
	sec_muxtbl_init(SEC_MACHINE_GOKEY, system_rev);

	/* initialize sec common infrastructures */
	sec_common_init();
	sec_debug_init_crash_key(NULL);

	/* initialize each drivers */
	omap4_gokey_serial_init();
	omap4_gokey_charger_init();
	omap4_gokey_pmic_init();
	omap4_gokey_audio_init();
#ifdef CONFIG_ION_OMAP
	omap4_register_ion();
#endif
	platform_add_devices(gokey_devices, ARRAY_SIZE(gokey_devices));
	omap_dmm_init();
	omap4_gokey_sdio_init();
	usb_musb_init(&musb_board_data);
	omap4_gokey_connector_init();
	omap4_gokey_display_init();
	omap4_gokey_input_init();
	omap4_gokey_wifi_init();
	omap4_gokey_sensors_init();
	omap4_gokey_camera_init();

	if (sec_debug_get_level())
		platform_add_devices(gokey_dbg_devices,
				     ARRAY_SIZE(gokey_dbg_devices));

	sec_common_init_post();
}

static void __init gokey_map_io(void)
{
	omap2_set_globals_443x();
	omap44xx_map_common_io();

	sec_getlog_supply_meminfo(GOKEY_MEM_BANK_0_SIZE,
				  GOKEY_MEM_BANK_0_ADDR,
				  GOKEY_MEM_BANK_1_SIZE,
				  GOKEY_MEM_BANK_1_ADDR);
}

static void omap4_gokey_init_carveout_sizes(struct omap_ion_platform_data
					       *ion)
{
	ion->tiler1d_size = (SZ_1M * 14);
	/* WFD is not supported in gokey So the size is zero */
	ion->secure_output_wfdhdcp_size = 0;
	ion->ducati_heap_size = (SZ_1M * 65);
	ion->nonsecure_tiler2d_size = (SZ_1M * 8);
	ion->tiler2d_size = (SZ_1M * 81);
}

static void __init gokey_reserve(void)
{
	int i;
	int ret;

#ifdef CONFIG_ION_OMAP
	omap_init_ram_size();
	omap4_gokey_memory_display_init();
	omap4_gokey_init_carveout_sizes(get_omap_ion_platform_data());
	omap_ion_init();
#endif
	/* do the static reservations first */
	if (sec_debug_get_level()) {
#if defined(CONFIG_ANDROID_RAM_CONSOLE)
		memblock_remove(GOKEY_RAMCONSOLE_START,
				GOKEY_RAMCONSOLE_SIZE);
#endif
#if defined(CONFIG_RAMOOPS)
		memblock_remove(GOKEY_RAMOOPS_START, GOKEY_RAMOOPS_SIZE);
#endif
	}
	memblock_remove(PHYS_ADDR_SMC_MEM, PHYS_ADDR_SMC_SIZE);
	memblock_remove(PHYS_ADDR_DUCATI_MEM, PHYS_ADDR_DUCATI_SIZE);

	/* ipu needs to recognize secure input buffer area as well */
	omap_ipu_set_static_mempool(PHYS_ADDR_DUCATI_MEM,
				    PHYS_ADDR_DUCATI_SIZE +
				    OMAP4_ION_HEAP_SECURE_INPUT_SIZE +
				    OMAP4_ION_HEAP_SECURE_OUTPUT_WFDHDCP_SIZE);
	omap_reserve();

	sec_log_buf_reserve();
}

MACHINE_START(OMAP4_SAMSUNG, "gokey")
	/* Maintainer: Samsung Electronics Co, Ltd. */
	.boot_params = 0x80000100,
	.reserve = gokey_reserve,
	.map_io = gokey_map_io,
	.init_early = gokey_init_early,
	.init_irq = gic_init_irq,
	.init_machine = gokey_init,
	.timer = &omap_timer,
MACHINE_END
