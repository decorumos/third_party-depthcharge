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

#include <libpayload.h>
#include <vboot_api.h>
#include <vboot/util/flag.h>

#include "base/time.h"
#include "debug/gdb/gdb.h"
#include "debug/netboot.h"

#define CSI_0 0x1B
#define CSI_1 0x5B

#define KEY_DOWN 0402
#define KEY_UP 0403
#define KEY_LEFT 0404
#define KEY_RIGHT 0405

#define TIMEOUT_US (10 * 1000)	// 10ms

uint32_t VbExKeyboardRead(void)
{
	uint64_t timer_start;

	// No input, just give up.
	if (!havekey())
		return 0;

	uint32_t ch = getchar();
	switch (ch) {
	case '\n': return '\r';
	case KEY_UP: return VB_KEY_UP;
	case KEY_DOWN: return VB_KEY_DOWN;
	case KEY_RIGHT: return VB_KEY_RIGHT;
	case KEY_LEFT: return VB_KEY_LEFT;
	case CSI_0:
		timer_start = time_us(0);
		while (!havekey()) {
			if (time_us(timer_start) >= TIMEOUT_US)
				return CSI_0;
		}

		// Ignore non escape [ sequences.
		if (getchar() != CSI_1)
			return CSI_0;

		// Translate the arrow keys, and ignore everything else.
		switch (getchar()) {
		case 'A': return VB_KEY_UP;
		case 'B': return VB_KEY_DOWN;
		case 'C': return VB_KEY_RIGHT;
		case 'D': return VB_KEY_LEFT;
		default: return 0;
		}

	// These two cases only work on developer images (empty stubs otherwise)
	case 'N' & 0x1f: dc_dev_netboot();	// CTRL+N: netboot
	case 'G' & 0x1f: gdb_enter();		// CTRL+G: remote GDB mode
	// fall through for non-developer images as if these didn't exist
	default:
		return ch;
	}
}

uint32_t VbExKeyboardReadWithFlags(uint32_t *flags_ptr)
{
	uint32_t c = VbExKeyboardRead();
	if (flags_ptr) {
		*flags_ptr = 0;
		// USB keyboards definitely cannot be trusted (assuming they
		// are even keyboards).  There are other devices that also
		// cannot be trusted, but this is the best we can do for now.
		if (last_key_input_type() != CONSOLE_INPUT_TYPE_USB)
			*flags_ptr |= VB_KEY_FLAG_TRUSTED_KEYBOARD;
	}
	return c;
}
