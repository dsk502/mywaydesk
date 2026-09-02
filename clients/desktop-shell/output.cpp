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

#include "output.hpp"

const struct wl_output_listener output_listener = {
	output_handle_geometry,
	output_handle_mode,
	output_handle_done,
	output_handle_scale
};

void
Output::output_init(Desktop *desktop) {
	struct wl_surface *surface;

	if (desktop->want_panel) {
		this->panel = new Panel(desktop, this);
		surface = window_get_wl_surface(this->panel->window);
		weston_desktop_shell_set_panel(desktop->shell,
					       this->output, surface);
		
		//Init the dock in the output layer
		this->dock = new Dock(desktop, this);
		surface = window_get_wl_surface(this->dock->window);
		weston_desktop_shell_set_dock(desktop->shell, this->output, surface);
	}

	this->background = new Background(desktop, this);
	surface = window_get_wl_surface(this->background->window);
	weston_desktop_shell_set_background(desktop->shell,
					    this->output, surface);
}

Output::~Output()
{
	if (this->background)
		delete background;
	if (this->panel)
		delete panel;
	if (this->dock)
		delete dock;
	wl_output_destroy(this->output);
	wl_list_remove(&this->link);
}

void
output_handle_geometry(void *data,
                       struct wl_output *wl_output,
                       int x, int y,
                       int physical_width,
                       int physical_height,
                       int subpixel,
                       const char *make,
                       const char *model,
                       int transform)
{
	Output *output = static_cast<Output *>(data);

	output->x = x;
	output->y = y;

	if (output->panel)
		window_set_buffer_transform(output->panel->window, static_cast<wl_output_transform>(transform));
	if (output->background)
		window_set_buffer_transform(output->background->window, static_cast<wl_output_transform>(transform));
}

void
output_handle_mode(void *data,
		   struct wl_output *wl_output,
		   uint32_t flags,
		   int width,
		   int height,
		   int refresh)
{
}

void
output_handle_done(void *data,
                   struct wl_output *wl_output)
{
}

void
output_handle_scale(void *data,
                    struct wl_output *wl_output,
                    int32_t scale)
{
	Output *output = static_cast<Output *>(data);

	if (output->panel)
		window_set_buffer_scale(output->panel->window, scale);
	if (output->background)
		window_set_buffer_scale(output->background->window, scale);
}