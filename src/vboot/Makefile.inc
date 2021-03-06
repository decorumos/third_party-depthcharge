##
## Copyright 2012 Google Inc.
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

subdirs-y := callbacks crossystem stages util

depthcharge-y += stages.c
depthcharge-y += firmware_id.c
depthcharge-y += screens.c
depthcharge-y += vbnv.c

VB_LIB = $(obj)/external/vboot/vboot_fw.a

VB_FIRMWARE_ARCH-$(CONFIG_ARCH_ARM) = arm
VB_FIRMWARE_ARCH-$(CONFIG_ARCH_X86) = x86

# The vboot library source is external to depthcharge.
$(VB_LIB):
ifndef VB_SOURCE
	$(error Set VB_SOURCE to point to the vboot source directory)
else
	@printf "    MAKE       $(subst $(obj)/,,$(@))\n"
	$(Q)FIRMWARE_ARCH=$(VB_FIRMWARE_ARCH-y) \
		CC="$(CC)" \
		CFLAGS="$(CFLAGS) -DVBOOT_DEBUG" \
		$(MAKE) -C $(VB_SOURCE) \
		MOCK_TPM=$(CONFIG_MOCK_TPM) \
		REGION_READ=y \
		BUILD=$(dir $(VB_LIB)) \
		V=$(V) \
		fwlib
endif

.PHONY: $(VB_LIB)
