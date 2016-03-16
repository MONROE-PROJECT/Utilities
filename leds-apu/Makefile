# LEDs driver for PCEngines apu series module Makefile
#
# This file is in the public domain. Permission to use, copy, modify, and
# distribute this software and its documentation for any purpose and without
# fee is hereby granted, without any conditions or restrictions.
# This software is provided “as is” without express or implied warranty.
#
# Christian Herzog (daduke) <daduke@daduke.org>, 2013-12-10, based on
# Adam Cécile (Le_Vert) <gandalf@le-vert.net>, 2008-01-18

KERNEL_VERSION  := `uname -r`
KERNEL_DIR      := /lib/modules/$(KERNEL_VERSION)/build
INSTALL_MOD_DIR := kernel/drivers/leds
PWD             := $(shell pwd)

obj-m := leds-apu.o apu_button.o
SOURCE = $(subst .o,.c,$(obj-m))
MODULE = $(subst .o,.ko,$(obj-m))

all: $(MODULE)
	gcc -o apuled apuled.c

$(MODULE): $(SOURCE)
	$(MAKE) -C $(KERNEL_DIR) SUBDIRS=$(PWD) modules

install: $(MODULE)
	$(MAKE) -C $(KERNEL_DIR) SUBDIRS=$(PWD) INSTALL_MOD_DIR=$(INSTALL_MOD_DIR) modules_install
	depmod -ae

uninstall:
	rm -f /lib/modules/`uname -r`/$(INSTALL_MOD_DIR)/$(MODULE)
	depmod -ae

distclean: clean

clean:
	rm -f *.o *.ko .*.cmd .*.flags *.mod.c Module.symvers
	rm -rf .tmp_versions
	rm -f apuled modules.order
