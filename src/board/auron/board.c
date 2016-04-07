/*
 * Copyright 2014 Google Inc.
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

#include <pci.h>

#include "base/init_funcs.h"
#include "base/list.h"
#include "board/board.h"
#include "board/board_helpers.h"
#include "drivers/ec/cros/lpc.h"
#include "drivers/flash/flash.h"
#include "drivers/flash/memmapped.h"
#include "drivers/gpio/lynxpoint_lp.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/pch.h"
#include "drivers/sound/hda_codec.h"
#include "drivers/sound/sound.h"
#include "drivers/storage/ahci.h"
#include "drivers/storage/blockdev.h"
#include "drivers/tpm/lpc.h"
#include "drivers/tpm/tpm.h"
#include "drivers/uart/8250.h"
#include "vboot/util/flag.h"

static int board_setup(void)
{
	sysinfo_install_flags(NULL);

	LpPchGpio *ec_in_rw = new_lp_pch_gpio_input(14);
	flag_install(FLAG_ECINRW, &ec_in_rw->ops);

	CrosEcLpcBus *cros_ec_lpc_bus =
		new_cros_ec_lpc_bus(CROS_EC_LPC_BUS_GENERIC);
	cros_ec_set_bus(&cros_ec_lpc_bus->ops);

	flash_set_ops(&new_mem_mapped_flash(0xff800000, 0x800000)->ops);

	HdaCodec *codec = new_hda_codec();
	sound_set_ops(&codec->ops);

	// The realtek codec doesn't report its beep_nid (NID 1)
	set_hda_beep_nid_override(codec, 1);

	AhciCtrlr *ahci = new_ahci_ctrlr(PCI_DEV(0, 31, 2));
	list_insert_after(&ahci->ctrlr.list_node, &fixed_block_dev_controllers);

	tpm_set_ops(&new_lpc_tpm((void *)(uintptr_t)0xfed40000)->ops);

	return 0;
}

PUB_STAT(power, &pch_power_ops)

PUB_DYN(debug_uart, &new_uart_8250_io(0x3f8)->uart.ops)

INIT_FUNC(board_setup);
