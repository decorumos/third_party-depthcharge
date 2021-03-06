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

#include <stdlib.h>

#include "base/timestamp.h"
#include "drivers/board/board.h"
#include "module/module.h"
#include "vboot/stages.h"
#include "vboot/util/commonparams.h"

void module_main(void)
{
	timestamp_add_now(TS_RO_VB_INIT);

	// Initialize vboot.
	if (vboot_init())
		halt();

	timestamp_add_now(TS_RO_VB_SELECT_FIRMWARE);

	// Select firmware.
	DcModule *rwa = new_dc_module(board_storage_main_fw_a());
	DcModule *rwb = new_dc_module(board_storage_main_fw_b());
	if (vboot_select_firmware(&rwa->ops, &rwb->ops))
		halt();

	timestamp_add_now(TS_RO_VB_SELECT_AND_LOAD_KERNEL);

	// Select a kernel and boot it.
	if (vboot_select_and_load_kernel())
		halt();
}
