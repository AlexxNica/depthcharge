/*
 * Copyright 2014 Rockchip Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <arch/io.h>

#include "base/init_funcs.h"
#include "boot/fit.h"
#include "boot/ramoops.h"
#include "drivers/gpio/rockchip.h"
#include "drivers/bus/i2c/rockchip.h"
#include "drivers/flash/spi.h"
#include "drivers/bus/spi/rockchip.h"
#include "drivers/tpm/slb9635_i2c.h"
#include "drivers/tpm/tpm.h"
#include "drivers/power/rk808.h"
#include "drivers/power/sysinfo.h"
#include "drivers/storage/dw_mmc.h"
#include "drivers/storage/rk_mmc.h"

#include "drivers/gpio/sysinfo.h"
#include "vboot/util/flag.h"
#include "drivers/bus/i2s/rockchip.h"

#include "drivers/bus/usb/usb.h"

static void install_phys_presence_flag(void)
{
	GpioOps *phys_presence = sysinfo_lookup_gpio(
			"recovery", 1, new_rk_gpio_input_from_coreboot);

	if (!phys_presence) {
		printf("%s failed retrieving recovery GPIO\n", __func__);
		return;
	}
	flag_install(FLAG_PHYS_PRESENCE, phys_presence);
}

static int board_setup(void)
{
	fit_set_compat_by_rev("google,veyron-rialto-rev%d",
			      lib_sysinfo.board_id);

	RkSpi *spi2 = new_rockchip_spi(0xff130000);
	flash_set_ops(&new_spi_flash(&spi2->ops, 0x400000)->ops);

	sysinfo_install_flags(new_rk_gpio_input_from_coreboot);

	RkI2c *i2c1 = new_rockchip_i2c((void *)0xff140000);
	tpm_set_ops(&new_slb9635_i2c(&i2c1->ops, 0x20)->base.ops);

	RkI2c *i2c0 = new_rockchip_i2c((void *)0xff650000);
	Rk808Pmic *pmic = new_rk808_pmic(&i2c0->ops, 0x1b);
	SysinfoResetPowerOps *power = new_sysinfo_reset_power_ops(&pmic->ops,
			new_rk_gpio_output_from_coreboot);
	power_set_ops(&power->ops);

	DwmciHost *emmc = new_rkdwmci_host(0xff0f0000, 594000000, 8, 0, NULL);
	list_insert_after(&emmc->mmc.ctrlr.list_node,
			  &fixed_block_dev_controllers);

	UsbHostController *usb_host1 = new_usb_hc(DWC2, 0xff540000);
	list_insert_after(&usb_host1->list_node, &usb_host_controllers);

	UsbHostController *usb_otg = new_usb_hc(DWC2, 0xff580000);
	list_insert_after(&usb_otg->list_node, &usb_host_controllers);

	// Read the current value of the recovery button for confirmation
	// when transitioning between normal and dev mode.
	flag_replace(FLAG_RECSW, sysinfo_lookup_gpio("recovery",
				1, new_rk_gpio_input_from_coreboot));

	/* Lid always open for now. */
	flag_replace(FLAG_LIDSW, new_gpio_high());

	/* Follow Storm to use recovery button as Ctrl-U. */
	install_phys_presence_flag();

	ramoops_buffer(0x31f00000, 0x100000, 0x20000);

	return 0;
}

INIT_FUNC(board_setup);
