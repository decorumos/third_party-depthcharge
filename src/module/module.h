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

#ifndef __MODULE_MODULE_H__
#define __MODULE_MODULE_H__

#include <stdint.h>

#include "drivers/storage/storage.h"

extern const char *module_title;

void module_main(void);


int start_module(const void *compressed_image, uint32_t size);


typedef struct DcModuleOps {
	int (*start)(struct DcModuleOps *me);
} DcModuleOps;

static inline int dc_module_start(DcModuleOps *me)
{
	return me->start(me);
}

typedef struct {
	DcModuleOps ops;

	StorageOps *storage;
} DcModule;

DcModule *new_dc_module(StorageOps *storage);

#endif /* __MODULE_MODULE_H__ */
