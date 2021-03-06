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
#include <stdint.h>
#include <vboot_api.h>
#include <vboot_nvstorage.h>

#include "base/xalloc.h"
#include "base/power.h"
#include "base/timestamp.h"
#include "boot/commandline.h"
#include "drivers/blockdev/blockdev.h"
#include "drivers/board/board.h"
#include "drivers/keyboard/keyboard.h"
#include "module/module.h"
#include "module/symbols.h"
#include "vboot/boot.h"
#include "vboot/stages.h"
#include "vboot/crossystem/crossystem.h"
#include "vboot/util/commonparams.h"
#include "vboot/util/ec.h"
#include "vboot/util/memory.h"
#include "vboot/vbnv.h"

enum {
	CmdLineSize = 4096,
	CrosParamSize = 4096,
};

int vboot_init(void)
{
	VbInitParams iparams = {
		.flags = 0
	};

	int dev_switch = board_flag_developer_mode();
	int rec_switch = board_flag_recovery();
	int wp_switch = board_flag_write_protect();
	int lid_switch = board_flag_lid_open();
	int oprom_loaded = 0;
	if (CONFIG_OPROM_MATTERS)
		oprom_loaded = board_flag_option_roms_loaded();

	printf("%s:%d dev %d, rec %d, wp %d, lid %d, oprom %d\n",
	       __func__, __LINE__, dev_switch, rec_switch,
	       wp_switch, lid_switch, oprom_loaded);

	if (dev_switch < 0 || rec_switch < 0 || lid_switch < 0 ||
	    wp_switch < 0 || oprom_loaded < 0) {
		// An error message should have already been printed.
		return 1;
	}

	// Decide what flags to pass into VbInit.
	iparams.flags |= VB_INIT_FLAG_RO_NORMAL_SUPPORT;
	/* Don't fail the boot process when lid switch is closed.
	 * The OS might not have enough time to log success before
	 * shutting down.
	 */
	if (!lid_switch)
		iparams.flags |= VB_INIT_FLAG_NOFAIL_BOOT;
	if (dev_switch)
		iparams.flags |= VB_INIT_FLAG_DEV_SWITCH_ON;
	if (rec_switch)
		iparams.flags |= VB_INIT_FLAG_REC_BUTTON_PRESSED;
	if (wp_switch)
		iparams.flags |= VB_INIT_FLAG_WP_ENABLED;
	if (oprom_loaded)
		iparams.flags |= VB_INIT_FLAG_OPROM_LOADED;
	if (CONFIG_OPROM_MATTERS)
		iparams.flags |= VB_INIT_FLAG_OPROM_MATTERS;
	if (CONFIG_VIRTUAL_DEV_SWITCH)
		iparams.flags |= VB_INIT_FLAG_VIRTUAL_DEV_SWITCH;
	if (CONFIG_EC_SOFTWARE_SYNC)
		iparams.flags |= VB_INIT_FLAG_EC_SOFTWARE_SYNC;
        if (!CONFIG_PHYSICAL_REC_SWITCH)
                iparams.flags |= VB_INIT_FLAG_VIRTUAL_REC_SWITCH;

	if (common_params_init())
		return 1;

	printf("Calling VbInit().\n");
	VbError_t res = VbInit(&cparams, &iparams);
	if (res != VBERROR_SUCCESS) {
		printf("VbInit returned %d, Doing a cold reboot.\n", res);
		if (cold_reboot())
			return 1;
	}

	return vboot_do_init_out_flags(iparams.out_flags);
};

int vboot_do_init_out_flags(uint32_t out_flags)
{
	if (out_flags & VB_INIT_OUT_CLEAR_RAM) {
		if (memory_wipe_unused())
			return 1;
	}
	/*
	 * If in developer mode or recovery mode, assume we're going to need
	 * input. We'll want it up and responsive by the time we present
	 * prompts to the user, so get it going ahead of time.
	 */
	if (out_flags & (VB_INIT_OUT_ENABLE_DEVELOPER |
			 VB_INIT_OUT_ENABLE_RECOVERY))
		keyboard_prepare();

	return 0;
}

int vboot_select_firmware(DcModuleOps *rwa, DcModuleOps *rwb)
{
	if (common_params_init())
		return 1;

	StorageOps *storage_a = board_storage_vblock_a();
	StorageOps *storage_b = board_storage_vblock_b();

	int size_a = storage_size(storage_a);
	int size_b = storage_size(storage_b);
	if (size_a < 0 || size_b < 0)
		return 1;

	void *vblock_a = xmalloc(size_a);
	void *vblock_b = xmalloc(size_b);

	if (storage_read(storage_a, vblock_a, 0, size_a) ||
	    storage_read(storage_b, vblock_b, 0, size_b)) {
		free(vblock_a);
		free(vblock_b);
		return 1;
	}

	VbSelectFirmwareParams fparams = {
		.verification_block_A = vblock_a,
		.verification_block_B = vblock_b,
		.verification_size_A = size_a,
		.verification_size_B = size_b
	};

	printf("Calling VbSelectFirmware().\n");
	VbError_t res = VbSelectFirmware(&cparams, &fparams);
	free(vblock_a);
	free(vblock_b);
	if (res != VBERROR_SUCCESS) {
		printf("VbSelectFirmware returned %d, "
		       "Doing a cold reboot.\n", res);
		if (cold_reboot())
			return 1;
	}

	enum VbSelectFirmware_t select = fparams.selected_firmware;

	// If an RW firmware was selected, start it.
	if (select == VB_SELECT_FIRMWARE_A)
		return dc_module_start(rwa);
	else if (select == VB_SELECT_FIRMWARE_B)
		return dc_module_start(rwb);

	return 0;
}

int vboot_select_and_load_kernel(void)
{
	static char cmd_line_buf[2 * CmdLineSize];

	VbSelectAndLoadKernelParams kparams;
	if (CONFIG_HOSTED) {
		// If we're hosted, stay in our own address space for now.
		static uint8_t hosted_kernel_buffer[CONFIG_KERNEL_SIZE];
		kparams.kernel_buffer = hosted_kernel_buffer;
		kparams.kernel_buffer_size = sizeof(hosted_kernel_buffer);
	} else {
		// If we're not hosted, set the kernel up in place.
		kparams.kernel_buffer = (void *)(uintptr_t)CONFIG_KERNEL_START;
		kparams.kernel_buffer_size = CONFIG_KERNEL_SIZE;
	};

	if (common_params_init())
		return 1;

	printf("Calling VbSelectAndLoadKernel().\n");
	VbError_t res = VbSelectAndLoadKernel(&cparams, &kparams);
	if (res == VBERROR_EC_REBOOT_TO_RO_REQUIRED) {
		printf("Rebooting the EC to RO.\n");
		reboot_ec_to_ro();
		if (power_off())
			return 1;
	} else if (res == VBERROR_SHUTDOWN_REQUESTED) {
		printf("Powering off.\n");
		if (power_off())
			return 1;
	}
	if (res != VBERROR_SUCCESS) {
		printf("VbSelectAndLoadKernel returned %d, "
		       "Doing a cold reboot.\n", res);
		if (cold_reboot())
			return 1;
	}

	timestamp_add_now(TS_CROSSYSTEM_DATA);

	// The scripts that packaged the kernel assumed it was going to end
	// up at 1MB which is frequently not right. The address of the 
	// "loader", which isn't actually used any more, is set based on that
	// assumption. We have to subtract the 1MB offset from it, and then add
	// the actual load address to figure ou thwere it actually is,
	// or would be if it existed.
	void *kernel = kparams.kernel_buffer;
	void *loader = (uint8_t *)kernel +
		(kparams.bootloader_address - 0x100000);
	void *params = (uint8_t *)loader - CrosParamSize;
	void *orig_cmd_line = (uint8_t *)params - CmdLineSize;

	BlockDev *bdev = (BlockDev *)kparams.disk_handle;

	if (commandline_subst(orig_cmd_line, cmd_line_buf,
			      sizeof(cmd_line_buf), 0,
			      kparams.partition_number + 1,
			      kparams.partition_guid, bdev->external_gpt))
		return 1;

	if (crossystem_setup())
		return 1;

	boot(kernel, cmd_line_buf, params, loader);

	return 1;
}
