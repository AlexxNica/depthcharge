/*
 * Copyright 2013 Google Inc.
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <libpayload.h>
#include <pci/pci.h>

#include "sdhci.h"

#define IF_NAME_SIZE	0x20

typedef struct {
	SdhciHost sdhci_host;
	pcidev_t sdhci_dev;
	char dev_name[IF_NAME_SIZE];
} PciSdhciHost;

/* Discover the register file address of the PCI SDHCI device. */
static int attach_device(SdhciHost *host)
{
	PciSdhciHost *pci_host;
	uint32_t addr;

	pci_host = container_of(host, PciSdhciHost, sdhci_host);
	addr = pci_read_config32(pci_host->sdhci_dev, PCI_BASE_ADDRESS_0);

	if (addr == ((uint32_t)~0)) {
		printf("%s: Error: %s not found\n",
		       __func__, pci_host->dev_name);
		return -1;
	}

	host->ioaddr = (void *) (addr & ~0xf);

	return 0;
}

/* Initialize an HDHCI port */
SdhciHost *new_pci_sdhci_host(pcidev_t dev, int removable,
			      int clock_min, int clock_max)
{
	PciSdhciHost *host;

	/* Allow room to store the interface name and and the PCI device ID */
	host = (PciSdhciHost *)malloc(sizeof(PciSdhciHost));

	if (!host) {
		printf("%s: malloc failed!\n", __func__);
		return NULL;
	}

	memset(host, 0, sizeof(*host));

	host->sdhci_dev = dev;
	snprintf(host->dev_name, sizeof(host->dev_name), "PCI SDHCI %d.%d.%d",
		 PCI_BUS(dev), PCI_SLOT(dev), PCI_FUNC(dev));

	host->sdhci_host.quirks = SDHCI_QUIRK_NO_HISPD_BIT |
		SDHCI_QUIRK_NO_SIMULT_VDD_AND_POWER;

	host->sdhci_host.version = sdhci_readw(&host->sdhci_host,
					       SDHCI_HOST_VERSION) & 0xff;

	host->sdhci_host.attach = attach_device;
	host->sdhci_host.clock_f_min = clock_min;
	host->sdhci_host.clock_f_max = clock_max;
	host->sdhci_host.removable = removable;

	/*
	 * The value translates to 'block access mode, supporting 1.7..1.95
	 * and 2.7..3.6 voltage ranges, which is typical for eMMC devices.
	 */
	host->sdhci_host.mmc_ctrlr.hardcoded_voltage = 0x40ff8080;

	add_sdhci(&host->sdhci_host);

	return &host->sdhci_host;
}