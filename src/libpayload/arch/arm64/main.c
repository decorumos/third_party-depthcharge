/*
 * Copyright (C) 2008 Advanced Micro Devices, Inc.
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

#include <arch/mmu.h>
#include <exception.h>
#include <libpayload.h>
#include <stdio.h>
#include <stdlib.h>

#include "module/module.h"

/*
 * Func: pre_sysinfo_scan_mmu_setup
 * Desc: We need to setup and enable MMU before we can go to scan coreboot
 * tables. However, we are not sure what all memory regions to map. Thus,
 * initializing minimum required memory ranges
 */
static void pre_sysinfo_scan_mmu_setup(void)
{
	uint64_t start = (uint64_t)&_start;
	uint64_t end = (uint64_t)&_end;

	/* Memory range 1: Covers the area occupied by payload */
	mmu_presysinfo_memory_used(start, end - start);

	/*
	 * Memory range 2: Coreboot tables
	 *
	 * Maximum size is assumed 2 pages in case it crosses the GRANULE_SIZE
	 * boundary
	 */
	mmu_presysinfo_memory_used((uintptr_t)cb_header_ptr,
				   2 * GRANULE_SIZE);

	mmu_presysinfo_enable();
}

/*
 * Func: post_sysinfo_scan_mmu_setup
 * Desc: Once we have scanned coreboot tables, we have complete information
 * about different memory ranges. Thus, we can perform a complete mmu
 * initialization. Also, this takes care of DMA area setup
 */
static void post_sysinfo_scan_mmu_setup(void)
{
	struct memrange *ranges = &lib_sysinfo.memrange[0];
	uint64_t nranges = lib_sysinfo.n_memranges;
	struct mmu_ranges mmu_ranges;
	struct mmu_memrange *dma_range;

	/* Get memory ranges for mmu init from lib_sysinfo memrange */
	dma_range = mmu_init_ranges_from_sysinfo(ranges, nranges, &mmu_ranges);

	/* Disable mmu */
	mmu_disable();

	/* Init mmu */
	mmu_init(&mmu_ranges);

	/* Enable mmu */
	mmu_enable();

	/* Init dma memory */
	init_dma_memory((void *)dma_range->base, dma_range->size);
}

void start_main(void) __attribute__((noreturn));
void start_main(void)
{
	pre_sysinfo_scan_mmu_setup();

	// Get information from the coreboot tables if they exist.
	get_coreboot_info(&lib_sysinfo);

	post_sysinfo_scan_mmu_setup();

	exception_init();

	module_main();
	printf("Returned from module_main.\n");
	halt();
}
