/*
 * This file is part of the libpayload project.
 *
 * Copyright (C) 2008-2010 coresystems GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef __DRIVERS_STORAGE_USBMSC_H__
#define __DRIVERS_STORAGE_USBMSC_H__

#include <stdint.h>
#include <usb/usb.h>

typedef struct {
	unsigned int blocksize;
	unsigned int numblocks;
	UsbEndpoint *bulk_in;
	UsbEndpoint *bulk_out;
	uint8_t usbdisk_created;
	int8_t ready;
	uint8_t lun;
	uint8_t num_luns;
	void *data; // User defined data.
} usbmsc_inst_t;

// Possible values for ready field.
enum {
	USB_MSC_DETACHED = -1, // Disk detached or out to lunch.
	USB_MSC_NOT_READY = 0, // Disk not ready yet -- empty card reader.
	USB_MSC_READY = 1,     // Disk ready to communicate.
};

#define MSC_INST(dev) ((usbmsc_inst_t*)(dev)->data)

typedef enum {
	cbw_direction_data_in = 0x80,
	cbw_direction_data_out = 0
} cbw_direction;

int readwrite_blocks_512(UsbDev *dev, int start, int n, cbw_direction dir,
			 uint8_t *buf);
int readwrite_blocks(UsbDev *dev, int start, int n, cbw_direction dir,
		     uint8_t *buf);

#endif /* __DRIVERS_STORAGE_USBMSC_H__ */
