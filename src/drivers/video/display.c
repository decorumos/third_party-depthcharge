/*
 * Copyright 2014 Google Inc.
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
#include <stdio.h>

#include "base/cleanup.h"
#include "base/die.h"
#include "drivers/video/display.h"

static DisplayOps *display_ops;

static int display_cleanup(DcEvent *event)
{
	if (display_ops && display_ops->stop)
		return display_ops->stop(display_ops);
	return 0;
}

void display_set_ops(DisplayOps *ops)
{
	die_if(display_ops, "%s: Display ops already set.\n", __func__);
	display_ops = ops;

	static CleanupEvent cleanup = {
		.event = { .trigger = &display_cleanup },
		.types = CleanupOnHandoff | CleanupOnLegacy,
	};

	// Call stop() when exiting depthcharge.
	cleanup_add(&cleanup);
}

int display_init(void)
{
	if (display_ops && display_ops->init)
		return display_ops->init(display_ops);

	return 0;
}

int backlight_update(uint8_t enable)
{
	if (display_ops && display_ops->backlight_update)
		return display_ops->backlight_update(display_ops, enable);

	printf("%s called but not implemented.\n", __func__);
	return 0;
}

int display_screen(enum VbScreenType_t screen)
{
	if (display_ops && display_ops->display_screen)
		return display_ops->display_screen(display_ops, screen);

	return 0;
}

