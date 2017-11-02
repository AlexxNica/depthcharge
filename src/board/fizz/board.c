/*
 * Copyright (C) 2017 Google Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
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
 */

#include <arch/io.h>
#include <libpayload.h>
#include <pci.h>
#include <pci/pci.h>
#include <sysinfo.h>

#include "base/init_funcs.h"
#include "base/list.h"
#include "config.h"
#include "drivers/bus/i2c/designware.h"
#include "drivers/bus/spi/intel_gspi.h"
#include "drivers/ec/cros/lpc.h"
#include "drivers/flash/flash.h"
#include "drivers/flash/memmapped.h"
#include "drivers/gpio/skylake.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/pch.h"
#include "drivers/soc/skylake.h"
#include "drivers/sound/gpio_edge_buzzer.h"
#include "drivers/storage/ahci.h"
#include "drivers/storage/nvme.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/sdhci.h"
#include "drivers/tpm/cr50_i2c.h"
#include "drivers/tpm/cr50_rec_switch.h"
#include "drivers/tpm/spi.h"
#include "drivers/tpm/tpm.h"
#include "vboot/util/flag.h"

/*
 * Clock frequencies for the eMMC and SD ports are defined below. The minimum
 * frequency is the same for both interfaces, the firmware does not run any
 * interface faster than 52 MHz, but defines maximum eMMC frequency as 200 MHz
 * for proper divider settings.
 */
#define EMMC_SD_CLOCK_MIN	400000
#define SD_CLOCK_MAX		52000000

static int cr50_irq_status(void)
{
	return skylake_get_gpe(GPE0_DW2_00);
}

static TpmOps *fizz_setup_tpm(void)
{
	if (IS_ENABLED(CONFIG_DRIVER_TPM_SPI)) {
		/* SPI TPM */
		const IntelGspiSetupParams gspi0_params = {
			.dev = PCI_DEV(0, 0x1e, 2),
			.cs_polarity = SPI_POLARITY_LOW,
			.clk_phase = SPI_CLOCK_PHASE_FIRST,
			.clk_polarity = SPI_POLARITY_LOW,
			.ref_clk_mhz = 120,
			.gspi_clk_mhz = 1,
		};
		SpiTpm *tpm = new_tpm_spi(new_intel_gspi(&gspi0_params),
					  cr50_irq_status);
		tpm_set_ops(&tpm->ops);
		return &tpm->ops;

	} else if (IS_ENABLED(CONFIG_DRIVER_TPM_CR50_I2C)) {
		DesignwareI2c *i2c1 = new_pci_designware_i2c(
			PCI_DEV(0, 0x15, 1), 400000, SKYLAKE_DW_I2C_MHZ);
		Cr50I2c *tpm = new_cr50_i2c(&i2c1->ops, 0x50,
					    &cr50_irq_status);
		tpm_set_ops(&tpm->base.ops);
		return &tpm->base.ops;
	}
}

static int board_setup(void)
{
	sysinfo_install_flags(new_skylake_gpio_input_from_coreboot);

	/* TPM */
	TpmOps *tpm_ops	= fizz_setup_tpm();
	flag_replace(FLAG_RECSW, &new_cr50_rec_switch(tpm_ops)->ops);

	/* Chrome EC (eSPI) */
	CrosEcLpcBus *cros_ec_lpc_bus =
		new_cros_ec_lpc_bus(CROS_EC_LPC_BUS_GENERIC);
	CrosEc *cros_ec = new_cros_ec(&cros_ec_lpc_bus->ops, 0, NULL);
	register_vboot_ec(&cros_ec->vboot, 0);

	/* 16MB SPI Flash */
	flash_set_ops(&new_mem_mapped_flash(0xff000000, 0x1000000)->ops);

	/* PCH Power */
	power_set_ops(&skylake_power_ops);

	/* PCIe NVME */
	NvmeCtrlr *nvme = new_nvme_ctrlr(PCI_DEV(0, 0x1c, 4));
	list_insert_after(&nvme->ctrlr.list_node, &fixed_block_dev_controllers);

	/* SATA SSD */
	AhciCtrlr *ahci = new_ahci_ctrlr(PCI_DEV(0, 0x17, 0));
	list_insert_after(&ahci->ctrlr.list_node, &fixed_block_dev_controllers);

	/* SD Card (if present) */
	pcidev_t sd_pci_dev = PCI_DEV(0, 0x1e, 6);
	uint16_t sd_vendor_id = pci_read_config32(sd_pci_dev, REG_VENDOR_ID);
	if (sd_vendor_id == PCI_VENDOR_ID_INTEL) {
		SdhciHost *sd = new_pci_sdhci_host(sd_pci_dev, 1,
					EMMC_SD_CLOCK_MIN, SD_CLOCK_MAX);
		list_insert_after(&sd->mmc_ctrlr.ctrlr.list_node,
					&removable_block_dev_controllers);
	}

	GpioCfg *sound_gpio = new_skylake_gpio_output(GPP_B14, 0);
	GpioEdgeBuzzer *buzzer = new_gpio_edge_buzzer(&sound_gpio->ops);
	sound_set_ops(&buzzer->ops);

	return 0;
}

INIT_FUNC(board_setup);
