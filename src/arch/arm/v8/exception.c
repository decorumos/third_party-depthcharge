/*
 * Copyright 2014 Google Inc.
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

#include <arch/arm/v8/exception.h>
#include <base/exception.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "base/die.h"

uint8_t exception_stack[0x4000] __attribute__((aligned(8)));

static ExceptionHook hook;
ExceptionState *exception_state;

static char *exception_names[EXC_COUNT] = {
	[EXC_SYNC_SP0] = "_sync_sp_el0",
	[EXC_IRQ_SP0]  = "_irq_sp_el0",
	[EXC_FIQ_SP0]  = "_fiq_sp_el0",
	[EXC_SERROR_SP0] = "_serror_sp_el0",
	[EXC_SYNC_SPX] = "_sync_spx",
	[EXC_IRQ_SPX]  = "_irq_spx",
	[EXC_FIQ_SPX]  = "_fiq_spx",
	[EXC_SERROR_SPX] = "_serror_spx",
	[EXC_SYNC_ELX_64] = "_sync_elx_64",
	[EXC_IRQ_ELX_64]  = "_irq_elx_64",
	[EXC_FIQ_ELX_64]  = "_fiq_elx_64",
	[EXC_SERROR_ELX_64] = "_serror_elx_64",
	[EXC_SYNC_ELX_32] = "_sync_elx_32",
	[EXC_IRQ_ELX_32]  = "_irq_elx_32",
	[EXC_FIQ_ELX_32]  = "_fiq_elx_32",
	[EXC_SERROR_ELX_32] = "_serror_elx_32",
};

static void print_regs(ExceptionState *state)
{
	printf("ELR = %#016llx\n", state->elr);
	printf("ESR = %#08llx\n", state->esr);
	for (int i = 0; i < 31; i++) {
		printf("X%02d = %#016llx\n", i, state->regs[i]);
	}
}

void exception_dispatch(ExceptionState *state, int idx)
{
	exception_state = state;

	if (idx >= EXC_COUNT) {
		printf("Bad exception index %d.\n", idx);
	} else {
		char *name = exception_names[idx];
		if (hook && hook(idx))
			return;

		if (name)
			printf("exception %s\n", name);
		else
			printf("exception _not_used.\n");
	}
	print_regs(state);

	halt();
}

void exception_init(void)
{
	__asm__ __volatile__(
		"msr SPSel, #1\n"	// Switch to the exception stack.
		"isb\n"
		"mov sp, %0\n"		// Set the stack pointer.
		"msr SPSel, #0\n"	// Switch back to the regular stack.
		:: "r"(exception_stack)
	);

	extern void* exception_table;
	set_vbar(&exception_table);
}

void exception_install_hook(ExceptionHook h)
{
	die_if(hook, "Implement support for a list of hooks if you need it.");
	hook = h;
}
