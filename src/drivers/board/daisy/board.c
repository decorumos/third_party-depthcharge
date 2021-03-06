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

#include "base/init_funcs.h"
#include "base/io.h"
#include "boot/fit.h"
#include "drivers/blockdev/blockdev.h"
#include "drivers/blockdev/exynos_mshc.h"
#include "drivers/board/board.h"
#include "drivers/board/board_helpers.h"
#include "drivers/board/daisy/i2c_arb.h"
#include "drivers/bus/i2c/s3c24x0.h"
#include "drivers/bus/i2s/exynos5/exynos5.h"
#include "drivers/bus/i2s/i2s.h"
#include "drivers/bus/spi/exynos5.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/ec/cros/i2c.h"
#include "drivers/flash/spi.h"
#include "drivers/gpio/exynos5250.h"
#include "drivers/gpio/fwdb.h"
#include "drivers/gpio/gpio.h"
#include "drivers/keyboard/dynamic.h"
#include "drivers/keyboard/mkbp/keyboard.h"
#include "drivers/layout/coreboot.h"
#include "drivers/power/exynos.h"
#include "drivers/sound/i2s.h"
#include "drivers/sound/max98095.h"
#include "drivers/sound/route.h"
#include "drivers/storage/flash.h"
#include "drivers/tpm/slb9635_i2c.h"
#include "drivers/tpm/tpm.h"
#include "drivers/uart/s5p.h"

static uint32_t *i2c_cfg = (uint32_t *)(0x10050000 + 0x234);

PRIV_DYN(lid_gpio, &new_exynos5250_gpio_input(GPIO_X, 3, 5)->ops)
PRIV_DYN(power_gpio, &new_exynos5250_gpio_input(GPIO_X, 1, 3)->ops)
PRIV_DYN(ec_in_rw_gpio, &new_exynos5250_gpio_input(GPIO_D, 1, 7)->ops)

PUB_STAT(flag_write_protect, gpio_get(&fwdb_gpio_wpsw.ops))
PUB_STAT(flag_recovery, gpio_get(&fwdb_gpio_recsw.ops))
PUB_STAT(flag_developer_mode, gpio_get(&fwdb_gpio_devsw.ops))
PUB_STAT(flag_option_roms_loaded, gpio_get(&fwdb_gpio_oprom.ops))
PUB_STAT(flag_lid_open, gpio_get(get_lid_gpio()))
PUB_STAT(flag_power, !gpio_get(get_power_gpio()))
PUB_STAT(flag_ec_in_rw, gpio_get(get_ec_in_rw_gpio()))

PRIV_DYN(spi1, &new_exynos5_spi(0x12d30000)->ops);

PRIV_DYN(flash, &new_spi_flash(get_spi1())->ops);
PUB_DYN(_coreboot_storage, &new_flash_storage(get_flash())->ops)

static int board_setup(void)
{
	fit_set_compat("google,snow");

	// Switch from hi speed I2C to the normal one.
	write32(i2c_cfg, 0x0);

	S3c24x0I2c *i2c3 = new_s3c24x0_i2c(0x12c90000);
	S3c24x0I2c *i2c4 = new_s3c24x0_i2c(0x12ca0000);
	S3c24x0I2c *i2c7 = new_s3c24x0_i2c(0x12cd0000);

	Exynos5250Gpio *request_gpio = new_exynos5250_gpio_output(GPIO_F, 0, 3);
	Exynos5250Gpio *grant_gpio = new_exynos5250_gpio_input(GPIO_E, 0, 4);
	SnowI2cArb *arb4 = new_snow_i2c_arb(&i2c4->ops, &request_gpio->ops,
					    &grant_gpio->ops);

	CrosEcI2cBus *cros_ec_i2c_bus = new_cros_ec_i2c_bus(&arb4->ops, 0x1e);
	cros_ec_set_bus(&cros_ec_i2c_bus->ops);

	tpm_set_ops(&new_slb9635_i2c(&i2c3->ops, 0x20)->base.ops);

	Exynos5I2s *i2s1 = new_exynos5_i2s(0x12d60000, 16, 2, 256);
	I2sSource *i2s_source = new_i2s_source(&i2s1->ops, 48000, 2, 16000);
	SoundRoute *sound_route = new_sound_route(&i2s_source->ops);
	Max98095Codec *codec = new_max98095_codec(&i2c7->ops, 0x11, 16, 48000,
						  256, 1);
	list_insert_after(&codec->component.list_node,
			  &sound_route->components);
	sound_set_ops(&sound_route->ops);

	MshciHost *emmc = new_mshci_host(0x12200000, 400000000,
					 8, 0, 0x03030001);
	MshciHost *sd_card = new_mshci_host(0x12220000, 400000000,
					    4, 1, 0x03020001);
	list_insert_after(&emmc->mmc.ctrlr.list_node,
			  &fixed_block_dev_controllers);
	list_insert_after(&sd_card->mmc.ctrlr.list_node,
			  &removable_block_dev_controllers);

	UsbHostController *usb_drd = new_usb_hc(UsbXhci, 0x12000000);
	UsbHostController *usb_host20 = new_usb_hc(UsbEhci, 0x12110000);
	UsbHostController *usb_host11 = new_usb_hc(UsbOhci, 0x12120000);

	list_insert_after(&usb_drd->list_node, &usb_host_controllers);
	list_insert_after(&usb_host20->list_node, &usb_host_controllers);
	list_insert_after(&usb_host11->list_node, &usb_host_controllers);

	return 0;
}

PUB_STAT(power, &exynos_power_ops)

PUB_DYN(debug_uart, &new_uart_s5p(0x12c00000 + 3 * 0x10000)->ops)

PUB_ARR(trusted_keyboards, &mkbp_keyboard.ops)
PUB_ARR(untrusted_keyboards, &dynamic_keyboards.ops)

INIT_FUNC(board_setup);
