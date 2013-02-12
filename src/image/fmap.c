/*
 * Copyright 2012 Google Inc.
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

#include <assert.h>
#include <libpayload.h>

#include "base/init_funcs.h"
#include "config.h"
#include "image/fmap.h"

static const Fmap * const main_fmap = (Fmap *)(uintptr_t)CONFIG_FMAP_ADDRESS;
uintptr_t main_rom_base;

static const char *fmap_ro_fwid_cache;
static int fmap_ro_fwid_size_cache;
static const char *fmap_rwa_fwid_cache;
static int fmap_rwa_fwid_size_cache;
static const char *fmap_rwb_fwid_cache;
static int fmap_rwb_fwid_size_cache;

const char *fmap_ro_fwid(void)
{
	return fmap_ro_fwid_cache;
}

int fmap_ro_fwid_size(void)
{
	return fmap_ro_fwid_size_cache;
}

const char *fmap_rwa_fwid(void)
{
	return fmap_rwa_fwid_cache;
}

int fmap_rwa_fwid_size(void)
{
	return fmap_rwa_fwid_size_cache;
}

const char *fmap_rwb_fwid(void)
{
	return fmap_rwb_fwid_cache;
}

int fmap_rwb_fwid_size(void)
{
	return fmap_rwb_fwid_size_cache;
}

const Fmap *fmap_base(void)
{
	return main_fmap;
}

const char *fmap_find_string(const char *name, int *size)
{
	assert(size);

	const FmapArea *area = fmap_find_area(name);
	if (!area) {
		*size = 0;
		return NULL;
	}
	*size = area->size;
	return (const char *)(uintptr_t)(main_rom_base + area->offset);
}

static int fmap_check_signature(void)
{
	return memcmp(main_fmap->signature, (uint8_t *)FMAP_SIGNATURE,
		      sizeof(main_fmap->signature));
}

int fmap_init(void)
{
	if (fmap_check_signature()) {
		printf("Bad signature on the FMAP.\n");
		return 1;
	}

	main_rom_base = (uintptr_t)(-main_fmap->size);

	fmap_ro_fwid_cache = fmap_find_string("RO_FRID",
					      &fmap_ro_fwid_size_cache);
	fmap_rwa_fwid_cache = fmap_find_string("RW_FWID_A",
					       &fmap_rwa_fwid_size_cache);
	fmap_rwb_fwid_cache = fmap_find_string("RW_FWID_B",
					       &fmap_rwb_fwid_size_cache);
	return 0;
}

INIT_FUNC(fmap_init);

const FmapArea *fmap_find_area(const char *name)
{
	for (int i = 0; i < main_fmap->nareas; i++) {
		const FmapArea *area = &(main_fmap->areas[i]);
		if (!strncmp(name, (const char *)area->name,
				sizeof(area->name))) {
			return area;
		}
	}
	return NULL;
}
