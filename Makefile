##
## This file is part of the depthcharge project.
##
## Copyright (C) 2008 Advanced Micro Devices, Inc.
## Copyright (C) 2008 Uwe Hermann <uwe@hermann-uwe.de>
## Copyright (c) 2012 The Chromium OS Authors.
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; version 2 of the License.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
##

export src := $(shell pwd)
export srctree := $(src)
export srck := $(src)/util/kconfig
export obj := $(src)/build
export objk := $(src)/build/util/kconfig

export KERNELVERSION      := 0.1.0
export KCONFIG_AUTOHEADER := $(obj)/config.h
export KCONFIG_AUTOCONFIG := $(obj)/auto.conf

CONFIG_SHELL := sh
KBUILD_DEFCONFIG := configs/defconfig
UNAME_RELEASE := $(shell uname -r)
HAVE_DOTCONFIG := $(wildcard .config)
MAKEFLAGS += -rR --no-print-directory

# Make is silent per default, but 'make V=1' will show all compiler calls.
ifneq ($(V),1)
Q := @
endif

HOSTCC = gcc
HOSTCXX = g++
HOSTCFLAGS := -I$(srck) -I$(objk)
HOSTCXXFLAGS := -I$(srck) -I$(objk)

LIBPAYLOAD_DIR := ../libpayload/install/libpayload
XCC := CC=$(CC) $(LIBPAYLOAD_DIR)/bin/lpgcc
AS = $(LIBPAYLOAD_DIR)/bin/lpas
STRIP ?= strip

INCLUDES = -Ibuild -I$(src)/include -I$(VB_INC_DIR)
ABI_FLAGS := -mpreferred-stack-boundary=2 -mregparm=3 -ffreestanding \
	-fno-builtin -fno-stack-protector
LINK_FLAGS := -Wl,--wrap=__divdi3 -Wl,--wrap=__udivdi3 \
	-Wl,--wrap=__moddi3 -Wl,--wrap=__umoddi3 $(ABI_FLAGS)
CFLAGS := -Wall -Werror -Os $(INCLUDES) -std=gnu99 $(ABI_FLAGS)
OBJECTS = depthcharge.o
OBJECTS += debug.o disk.o display.o firmware.o fmap.o gcc.o gpio.o \
	hda_codec.o keyboard.o memory.o misc.o nvstorage.o time.o tpm.o
OBJS    = $(patsubst %,$(obj)/%,$(OBJECTS))
OBJS    += $(VB_LD_DIR)/vboot_fw.a
TARGET  = $(obj)/depthcharge.elf

ifeq ($(strip $(HAVE_DOTCONFIG)),)

all: config

else

include $(src)/.config

all: $(TARGET)

$(TARGET): $(src)/.config $(OBJS) prepare
	$(Q)printf "  LD      $(subst $(shell pwd)/,,$(@))\n"
	$(Q)$(XCC) $(LINK_FLAGS) -o $@ $(OBJS)
	$(Q)printf "  STRIP   $(subst $(shell pwd)/,,$(@))\n"
	$(Q)$(STRIP) -s $@

$(obj)/%.S.o: $(src)/%.S
	$(Q)printf "  AS      $(subst $(shell pwd)/,,$(@))\n"
	$(Q)$(AS) -o $@ $<

$(obj)/%.o: $(src)/%.c
	$(Q)printf "  CC      $(subst $(shell pwd)/,,$(@))\n"
	$(Q)$(XCC) $(CFLAGS) -c -o $@ $<

endif

prepare:
	$(Q)mkdir -p $(obj)/util/kconfig/lxdialog

clean:
	$(Q)rm -rf build/*.elf build/*.o

distclean: clean
	$(Q)rm -rf build
	$(Q)rm -f .config .config.old ..config.tmp .kconfig.d .tmpconfig*

include util/kconfig/Makefile

.PHONY: $(PHONY) prepare clean distclean

