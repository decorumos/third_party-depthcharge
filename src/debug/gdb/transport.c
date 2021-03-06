/*
 * Copyright 2014 Google Inc.
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
 * Foundation, Inc.
 */

#include <endian.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "base/algorithm.h"
#include "base/die.h"
#include "base/io.h"
#include "base/time.h"
#include "debug/gdb/gdb_int.h"
#include "drivers/board/board.h"

/* MMIO word size is not standardized, but *usually* 32 (even on ARM64) */
typedef uint32_t mmio_word_t;

static const int timeout_us = 100 * 1000;
static const char output_overrun[] = "GDB output buffer overrun (try "
				     "increasing reply.size)!\n";
static const char input_underrun[] = "GDB input message truncated (bug or "
				     "communication problem)?\n";

/* Serial-specific glue code... add more transport layers here when desired. */

static void gdb_raw_putchar(uint8_t c)
{
	UartOps *uart = board_debug_uart();
	uart->put_char(uart, c);
}

static int gdb_raw_getchar(void)
{
	UartOps *uart = board_debug_uart();

	uint64_t start = time_us(0);
	while (!uart->have_char(uart))
		if (time_us(start) > timeout_us)
			return -1;

	return uart->get_char(uart);
}

/* Hex digit character <-> number conversion (illegal chars undefined!). */

static uint8_t from_hex(unsigned char c)
{
	static const int8_t values[] = {
		-1, 10, 11, 12, 13, 14, 15, -1,
		-1, -1, -1, -1, -1, -1, -1, -1,
		 0,  1,  2,  3,  4,  5,  6,  7,
		 8,  9, -1, -1, -1, -1, -1, -1,
	};

	return values[c & 0x1f];
}

static char to_hex(uint8_t v)
{
	static const char digits[] = "0123456789abcdef";

	return digits[v & 0xf];
}

/* Message encode/decode functions (must access whole aligned words for MMIO) */

void gdb_message_encode_bytes(GdbMessage *message, const void *data, int length)
{
	die_if(message->used + length * 2 > message->size, output_overrun);
	const mmio_word_t *aligned =
		(mmio_word_t *)ALIGN_DOWN((uintptr_t)data, sizeof(*aligned));
	mmio_word_t word = be32toh(read32(aligned++));
	while (length--) {
		uint8_t byte = (word >> ((((void *)aligned - data) - 1) * 8));
		message->buf[message->used++] = to_hex(byte >> 4);
		message->buf[message->used++] = to_hex(byte & 0xf);
		if (length && ++data == (void *)aligned)
			word = be32toh(read32(aligned++));
	}
}

void gdb_message_decode_bytes(const GdbMessage *message, int offset,
			      void *data, int length)
{
	die_if(offset + 2 * length > message->used, "Decode overrun in GDB "
		"message: %.*s", message->used, message->buf);
	mmio_word_t *aligned =
		(mmio_word_t *)ALIGN_DOWN((uintptr_t)data, sizeof(*aligned));
	int shift = ((void *)(aligned + 1) - data) * 8;
	mmio_word_t word = be32toh(read32(aligned)) >> shift;
	while (length--) {
		word <<= 8;
		word |= from_hex(message->buf[offset++]) << 4;
		word |= from_hex(message->buf[offset++]);
		if (++data - (void *)aligned == sizeof(*aligned))
			write32(aligned++, htobe32(word));
	}
	if (data != (void *)aligned) {
		shift = ((void *)(aligned + 1) - data) * 8;
		clrsetbits_be32(aligned, ~((1 << shift) - 1), word << shift);
	}
}

void gdb_message_encode_zero_bytes(GdbMessage *message, int length)
{
	die_if(message->used + length * 2 > message->size, output_overrun);
	memset(message->buf + message->used, '0', length * 2);
	message->used += length * 2;
}

void gdb_message_add_string(GdbMessage *message, const char *string)
{
	message->used += strlcpy((char *)message->buf + message->used,
			     string, message->size - message->used);

	/* Check >= instead of > to account for strlcpy's trailing '\0'. */
	die_if(message->used >= message->size, output_overrun);
}

void gdb_message_encode_int(GdbMessage *message, uintptr_t val)
{
	int length = sizeof(uintptr_t) * 2 - CLZ(val) / 4;
	die_if(message->used + length > message->size, output_overrun);
	while (length--)
		message->buf[message->used++] =
			to_hex((val >> length * 4) & 0xf);
}

uintptr_t gdb_message_decode_int(const GdbMessage *message, int offset,
				 int length)
{
	uintptr_t val = 0;

	die_if(length > sizeof(uintptr_t) * 2, "GDB decoding invalid number: "
	       "%.*s", message->used, message->buf);

	while (length--) {
		val <<= 4;
		val |= from_hex(message->buf[offset++]);
	}

	return val;
}

/* Like strtok/strsep: writes back offset argument, returns original offset. */
int gdb_message_tokenize(const GdbMessage *message, int *offset)
{
	int token = *offset;
	while (!strchr(",;:", message->buf[(*offset)++]))
		die_if(*offset >= message->used, "Undelimited token in GDB "
				"message at offset %d: %.*s",
				token, message->used, message->buf);
	return token;
}

/* High-level send/receive functions. */

void gdb_get_command(GdbMessage *command)
{
	enum command_state {
		STATE_WAITING,
		STATE_COMMAND,
		STATE_CHECKSUM0,
		STATE_CHECKSUM1,
	};

	uint8_t checksum = 0;
	uint8_t running_checksum = 0;
	enum command_state state = STATE_WAITING;

	while (1) {
		int c = gdb_raw_getchar();
		if (c < 0) {
			/*
			 * Timeout waiting for a byte. Reset the
			 * state machine.
			 */
			state = STATE_WAITING;
			continue;
		}

		switch (state) {
		case STATE_WAITING:
			if (c == '$') {
				running_checksum = 0;
				command->used = 0;
				state = STATE_COMMAND;
			}
			break;
		case STATE_COMMAND:
			if (c == '#') {
				state = STATE_CHECKSUM0;
				break;
			}
			die_if(command->used >= command->size, "GDB input buf"
			       "fer overrun (try increasing command.size)!\n");
			command->buf[command->used++] = c;
			running_checksum += c;
			break;
		case STATE_CHECKSUM0:
			checksum = from_hex(c) << 4;
			state = STATE_CHECKSUM1;
			break;
		case STATE_CHECKSUM1:
			checksum += from_hex(c);
			if (running_checksum == checksum) {
				gdb_raw_putchar('+');
				return;
			} else {
				state = STATE_WAITING;
				gdb_raw_putchar('-');
			}
			break;
		}
	}
}

void gdb_send_reply(const GdbMessage *reply)
{
	int i;
	int retries = 1 * 1000 * 1000 / timeout_us;
	uint8_t checksum = 0;

	for (i = 0; i < reply->used; i++)
		checksum += reply->buf[i];

	do {
		gdb_raw_putchar('$');
		for (i = 0; i < reply->used; i++)
			gdb_raw_putchar(reply->buf[i]);
		gdb_raw_putchar('#');
		gdb_raw_putchar(to_hex(checksum >> 4));
		gdb_raw_putchar(to_hex(checksum & 0xf));
	} while (gdb_raw_getchar() != '+' && retries--);
}
