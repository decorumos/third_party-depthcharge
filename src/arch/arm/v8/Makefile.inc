##
## Copyright 2013 Google Inc.
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; version 2 of the License.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
##

subdirs-y += handoff lib

depthcharge-y += boot_asm.S
depthcharge-y += boot.c
libc-y += cache.c
libc-y += cpu.S
libc-y += exception.c
libc-y += exception_asm.S
libc-y += memcpy.S
libc-y += memmove.S
libc-y += memset.S
libc-y += mmu.c
depthcharge-y += physmem_arm.c

gdb-y += gdb.c

src-includes-y += includes

ARCH_CFLAGS += -mgeneral-regs-only -march=armv8-a

TC_ARCH = arm64
