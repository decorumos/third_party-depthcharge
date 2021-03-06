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

#ifndef __DRIVERS_FLASH_FLASH_H__
#define __DRIVERS_FLASH_FLASH_H__

#include <stdint.h>

typedef struct FlashOps
{
	// Return a pointer to the read data in the flash driver cache.
	void *(*read)(struct FlashOps *me, uint32_t offset, uint32_t size);
	int (*write)(struct FlashOps *me, const void *buffer,
		     uint32_t offset, uint32_t size);
	int (*erase_size)(struct FlashOps *me);
	// Offset and size must be erase_size-aligned.
	int (*erase)(struct FlashOps *me, uint32_t offset, uint32_t size);
	int (*size)(struct FlashOps *me);
} FlashOps;

static inline void *flash_read(FlashOps *me, uint32_t offset, uint32_t size)
{
	return me->read(me, offset, size);
}

static inline int flash_write(FlashOps *me, const void *buffer,
			      uint32_t offset, uint32_t size)
{
	return me->write(me, buffer, offset, size);
}

static inline int flash_erase_size(FlashOps *me)
{
	return me->erase_size(me);
}

static inline int flash_erase(FlashOps *me, uint32_t offset, uint32_t size)
{
	return me->erase(me, offset, size);
}

static inline int flash_size(FlashOps *me)
{
	return me->size(me);
}


#endif /* __DRIVERS_FLASH_FLASH_H__ */
