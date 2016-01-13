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

#include <stdint.h>

#include "base/elf.h"
#include "module/enter_trampoline.h"
#include "module/symbols.h"

void enter_trampoline(Elf32_Ehdr *ehdr)
{
	__asm__ __volatile__(
		"mov %[new_stack], %%esp\n"
		"push %[cb_header_ptr]\n"
		"push %[ehdr]\n"
		"call tramp_load_elf\n"
		:: [new_stack]"r"(&_tramp_estack - 8), [ehdr]"a"(ehdr),
		   [cb_header_ptr]"d"(0)
		: "memory"
	);
}
