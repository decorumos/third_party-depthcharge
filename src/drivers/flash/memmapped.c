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

#include <libpayload.h>

#include "base/container_of.h"
#include "base/xalloc.h"
#include "drivers/flash/memmapped.h"

static void *mem_mapped_flash_read(FlashOps *me, uint32_t offset, uint32_t size)
{
	MemMappedFlash *flash = container_of(me, MemMappedFlash, ops);

	if (offset > flash->size || offset + size > flash->size) {
		printf("Out of bounds flash access.\n");
		return NULL;
	}

	return (void *)(uintptr_t)(flash->base + offset);
}

MemMappedFlash *new_mem_mapped_flash(uint32_t base, uint32_t size)
{
	MemMappedFlash *flash = xzalloc(sizeof(*flash));
	flash->ops.read = &mem_mapped_flash_read;
	flash->base = base;
	flash->size = size;
	return flash;
}
