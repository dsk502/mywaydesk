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

#ifndef _DS_BACKGROUND_HPP_	//desktop-shell/background.hpp
#define _DS_BACKGROUND_HPP_

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

#include "output.hpp"
#include "desktop.hpp"
#include "desktop-shell/common.hpp"

enum {
	BACKGROUND_SCALE,
	BACKGROUND_SCALE_CROP,
	BACKGROUND_TILE,
	BACKGROUND_CENTERED
};

class Background {
public:
	struct surface base;
	Output *owner;
	struct window *window;
	struct widget *widget;
	int painted;

	char *image;
	int type;
	uint32_t color;

	Background(Desktop *desktop, Output *output);
	~Background();

	//Callbacks
	static void background_draw(struct widget *widget, void *data);
	
	static void background_configure(void *data,
				struct weston_desktop_shell *desktop_shell,
				uint32_t edges, struct window *window,
				int32_t width, int32_t height);
};

#endif