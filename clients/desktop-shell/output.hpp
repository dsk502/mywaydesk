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

#include "desktop-shell/common.hpp"
#include "desktop.hpp"
#include "panel.hpp"
#include "dock.hpp"
#include "background.hpp"

class Output {
public:
	struct wl_output *output;
	uint32_t server_output_id;
	struct wl_list link;

	int x;
	int y;
	Panel *panel;
	Dock *dock;
	Background *background;

	Output() {};
	void output_init(Desktop *desktop);
	~Output();
};

void output_handle_geometry(void *data,
                       struct wl_output *wl_output,
                       int x, int y,
                       int physical_width,
                       int physical_height,
                       int subpixel,
                       const char *make,
                       const char *model,
                       int transform);

void output_handle_mode(void *data,
		   struct wl_output *wl_output,
		   uint32_t flags,
		   int width,
		   int height,
		   int refresh);

void output_handle_done(void *data,
                   struct wl_output *wl_output);

void output_handle_scale(void *data,
                    struct wl_output *wl_output,
                    int32_t scale);

const struct wl_output_listener output_listener = {
	output_handle_geometry,
	output_handle_mode,
	output_handle_done,
	output_handle_scale
};

#endif