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

depthcharge-$(CONFIG_DRIVER_AHCI) += ahci.c
depthcharge-y += bdev_stream.c
depthcharge-y += blockdev.c
depthcharge-$(CONFIG_DRIVER_BLOCKDEV_MMC) += mmc.c
depthcharge-$(CONFIG_DRIVER_BLOCKDEV_MMC_DW) += dw_mmc.c
depthcharge-$(CONFIG_DRIVER_BLOCKDEV_MMC_ROCKCHIP) += rk_mmc.c
depthcharge-$(CONFIG_DRIVER_BLOCKDEV_MMC_TEGRA) += tegra_mmc.c bouncebuf.c
depthcharge-$(CONFIG_DRIVER_BLOCKDEV_MSHC_S5P) += s5p_mshc.c
depthcharge-$(CONFIG_DRIVER_BLOCKDEV_SDHCI) += sdhci.c mem_sdhci.c
depthcharge-$(CONFIG_DRIVER_BLOCKDEV_SDHCI_PCI) += pci_sdhci.c
depthcharge-$(CONFIG_DRIVER_BLOCKDEV_UEFI) += uefi.c
depthcharge-$(CONFIG_DRIVER_BLOCKDEV_USB) += usb.c

libc-$(CONFIG_USB_MSC) += usbmsc.c
