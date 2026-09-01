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

#include "desktop-shell/common.hpp"

void
check_desktop_ready(struct window *window)
{
	struct display *display;
	struct desktop *desktop;

	display = window_get_display(window);
	desktop = display_get_user_data(display);

	if (!desktop->painted && is_desktop_painted(desktop)) {
		desktop->painted = 1;

		weston_desktop_shell_desktop_ready(desktop->shell);
	}
}

void
set_hex_color(cairo_t *cr, uint32_t color)
{
	cairo_set_source_rgba(cr,
			      ((color >> 16) & 0xff) / 255.0,
			      ((color >>  8) & 0xff) / 255.0,
			      ((color >>  0) & 0xff) / 255.0,
			      ((color >> 24) & 0xff) / 255.0);
}
