# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the kernel modules.
#

KERN_DIR := /lib/modules/$(shell uname -r)/build

obj-m += mach.o
obj-m += codec.o
obj-m += plat.o

all:
	$(MAKE) -C $(KERN_DIR) M=`pwd` modules

clean:
	$(MAKE) -C $(KERN_DIR) M=`pwd` modules clean
