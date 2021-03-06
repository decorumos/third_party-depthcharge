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

#include <sysinfo.h>

#include "base/init_funcs.h"
#include "base/io.h"
#include "boot/fit.h"
#include "boot/ramoops.h"
#include "drivers/blockdev/dw_mmc.h"
#include "drivers/blockdev/rk_mmc.h"
#include "drivers/board/board.h"
#include "drivers/board/board_helpers.h"
#include "drivers/bus/i2c/rockchip.h"
#include "drivers/bus/i2s/rockchip.h"
#include "drivers/bus/spi/rockchip.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/ec/cros/spi.h"
#include "drivers/flash/spi.h"
#include "drivers/gpio/fwdb.h"
#include "drivers/gpio/gpio.h"
#include "drivers/gpio/rockchip.h"
#include "drivers/keyboard/dynamic.h"
#include "drivers/keyboard/mkbp/keyboard.h"
#include "drivers/layout/coreboot.h"
#include "drivers/power/gpio_reset.h"
#include "drivers/power/rk808.h"
#include "drivers/sound/i2s.h"
#include "drivers/sound/route.h"
#include "drivers/sound/max98090.h"
#include "drivers/storage/flash.h"
#include "drivers/tpm/slb9635_i2c.h"
#include "drivers/tpm/tpm.h"
#include "drivers/uart/8250.h"
#include "drivers/video/display.h"
#include "drivers/video/rockchip.h"

PRIV_DYN(i2c0, &new_rockchip_i2c((void *)0xff650000)->ops)

PRIV_DYN(pmic, &new_rk808_pmic(get_i2c0(), 0x1b)->ops)

PRIV_DYN(backlight_gpio, &new_rk_gpio_output(GPIO(7, A, 2))->ops)
PRIV_DYN(ec_int_gpio, &new_rk_gpio_input(GPIO(7, A, 7))->ops)
PRIV_DYN(ec_int_gpio_n, new_gpio_not(get_ec_int_gpio()))

PRIV_DYN(lid_gpio, &new_rk_gpio_input(GPIO(0, A, 6))->ops)
PRIV_DYN(power_gpio, &new_rk_gpio_input(GPIO(0, A, 5))->ops)
PRIV_DYN(ec_in_rw_gpio, &new_rk_gpio_input(GPIO(0, A, 7))->ops)

PUB_STAT(flag_write_protect, gpio_get(&fwdb_gpio_wpsw.ops))
PUB_STAT(flag_recovery, gpio_get(&fwdb_gpio_recsw.ops))
PUB_STAT(flag_developer_mode, gpio_get(&fwdb_gpio_devsw.ops))
PUB_STAT(flag_option_roms_loaded, gpio_get(&fwdb_gpio_oprom.ops))
PUB_STAT(flag_lid_open, gpio_get(get_lid_gpio()))
PUB_STAT(flag_power, !gpio_get(get_power_gpio()))
PUB_STAT(flag_ec_in_rw, gpio_get(get_ec_in_rw_gpio()))

PRIV_DYN(spi0, &new_rockchip_spi(0xff110000)->ops)
PRIV_DYN(spi2, &new_rockchip_spi(0xff130000)->ops)

PRIV_DYN(flash, &new_spi_flash(get_spi2())->ops)
PUB_DYN(_coreboot_storage, &new_flash_storage(get_flash())->ops)

static int board_setup(void)
{
	cros_ec_set_bus(&new_cros_ec_spi_bus(get_spi0())->ops);
	cros_ec_set_interrupt_gpio(get_ec_int_gpio_n());

	RkI2c *i2c1 = new_rockchip_i2c((void *)0xff140000);
	tpm_set_ops(&new_slb9635_i2c(&i2c1->ops, 0x20)->base.ops);

	RockchipI2s *i2s0 = new_rockchip_i2s(0xff890000, 16, 2, 256);
	I2sSource *i2s_source = new_i2s_source(&i2s0->ops, 48000, 2, 16000);
	SoundRoute *sound_route = new_sound_route(&i2s_source->ops);
	RkI2c *i2c2 = new_rockchip_i2c((void *)0xff660000);
	Max98090Codec *codec = new_max98090_codec(&i2c2->ops, 0x10, 16, 48000,
						  256, 1);
	list_insert_after(&codec->component.list_node,
			  &sound_route->components);
	sound_set_ops(&sound_route->ops);

	DwmciHost *emmc = new_rkdwmci_host(0xff0f0000, 594000000, 8, 0, NULL);
	list_insert_after(&emmc->mmc.ctrlr.list_node,
			  &fixed_block_dev_controllers);

	RkGpio *card_detect = new_rk_gpio_input(GPIO(7, A, 5));
	GpioOps *card_detect_ops = &card_detect->ops;
	card_detect_ops = new_gpio_not(card_detect_ops);
	DwmciHost *sd_card = new_rkdwmci_host(0xff0c0000, 594000000, 4, 1,
					      card_detect_ops);
	list_insert_after(&sd_card->mmc.ctrlr.list_node,
			  &removable_block_dev_controllers);

	UsbHostController *usb_host1 = new_usb_hc(UsbDwc2, 0xff540000);
	list_insert_after(&usb_host1->list_node, &usb_host_controllers);

	UsbHostController *usb_otg = new_usb_hc(UsbDwc2, 0xff580000);
	list_insert_after(&usb_otg->list_node, &usb_host_controllers);

	ramoops_buffer(0x31f00000, 0x100000, 0x20000);

	if (lib_sysinfo.framebuffer != NULL)
		display_set_ops(new_rockchip_display(get_backlight_gpio()));

	return 0;
}

PRIV_DYN(reset_gpio, &new_rk_gpio_output(GPIO(0, B, 5))->ops)

PUB_DYN(power, &new_gpio_reset_power_ops(get_pmic(), get_reset_gpio())->ops)

PUB_DYN(debug_uart, &new_uart_8250_mem32(0xff690000)->uart.ops)

PUB_ARR(trusted_keyboards, &mkbp_keyboard.ops)
PUB_ARR(untrusted_keyboards, &dynamic_keyboards.ops)

INIT_FUNC(board_setup);
