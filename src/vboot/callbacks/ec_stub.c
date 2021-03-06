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

#include <stdio.h>
#include <vboot_api.h>

static void no_ec_soft_sync(void) __attribute__((noreturn));
static void no_ec_soft_sync(void)
{
	printf("EC software sync not supported.\n");
	halt();
}

int VbExTrustEC(int devidx)
{
	printf("The EC which doesn't exist isn't untrusted.\n");
	return 1;
}

VbError_t VbExEcRunningRW(int devidx, int *in_rw)
{
	no_ec_soft_sync();
}

VbError_t VbExEcJumpToRW(int devidx)
{
	no_ec_soft_sync();
}

VbError_t VbExEcDisableJump(int devidx)
{
	no_ec_soft_sync();
}

VbError_t VbExEcHashRW(int devidx, const uint8_t **hash, int *hash_size)
{
	no_ec_soft_sync();
}

VbError_t VbExEcGetExpectedRW(int devidx, enum VbSelectFirmware_t select,
			      const uint8_t **image, int *image_size)
{
	no_ec_soft_sync();
}

VbError_t VbExEcGetExpectedRWHash(int devidx, enum VbSelectFirmware_t select,
				  const uint8_t **hash, int *hash_size)
{
	no_ec_soft_sync();
}

VbError_t VbExEcUpdateRW(int devidx, const uint8_t *image, int image_size)
{
	no_ec_soft_sync();
}

VbError_t VbExEcProtectRW(int devidx)
{
	no_ec_soft_sync();
}

VbError_t VbExEcEnteringMode(int devidx, enum VbEcBootMode_t mode)
{
	return VBERROR_SUCCESS;
}

VbError_t VbExEcVbootDone(int in_recovery)
{
	return VBERROR_SUCCESS;
}
