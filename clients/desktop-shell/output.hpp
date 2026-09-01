/*
 * Copyright © 2011 Kristian Høgsberg
 * Copyright © 2011 Collabora, Ltd.
 * Copyright © 2026 Shengkang Duan
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef _DS_OUTPUT_HPP_ //desktop-shell/output.cpp
#define _DS_OUTPUT_HPP_

#include "config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <cairo.h>
#include <sys/wait.h>
#include <linux/input.h>
#include <libgen.h>
#include <ctype.h>
#include <time.h>
#include <assert.h>

#include <wayland-client.h>

#include <libweston/config-parser.h>
#include <libweston/zalloc.h>
#include "shared/helpers.h"
#include "shared/xalloc.h"
#include "shared/cairo-util.h"
#include "shared/file-util.h"
#include "shared/process-util.h"
#include "shared/timespec-util.h"

#include "window.h"

#include "tablet-unstable-v2-client-protocol.h"
#include "weston-desktop-shell-client-protocol.h"

#include "desktop.hpp"

#define DEFAULT_CLOCK_FORMAT CLOCK_FORMAT_MINUTES
#define DEFAULT_SPACING 10

enum clock_format {
	CLOCK_FORMAT_MINUTES,
	CLOCK_FORMAT_SECONDS,
	CLOCK_FORMAT_MINUTES_24H,
	CLOCK_FORMAT_SECONDS_24H,
	CLOCK_FORMAT_NONE
};

class Output {
public:
	struct wl_output *output;
	uint32_t server_output_id;
	struct wl_list link;

	int x;
	int y;
	Panel *panel;
	Dock *dock;
	Background *background

	Output(Desktop *desktop);
	~Output();
};

Output::Output(Desktop *desktop) {
	struct wl_surface *surface;

	if (desktop->want_panel) {
		this->panel = panel_create(desktop, this);
		surface = window_get_wl_surface(this->panel->window);
		weston_desktop_shell_set_panel(desktop->shell,
					       this->output, surface);
		
		//Init the dock in the output layer
		this->dock = dock_create(desktop, this);
		surface = window_get_wl_surface(this->dock->window);
		weston_desktop_shell_set_dock(desktop->shell, this->output, surface);
	}

	this->background = background_create(desktop, this);
	surface = window_get_wl_surface(this->background->window);
	weston_desktop_shell_set_background(desktop->shell,
					    this->output, surface);
}

#endif