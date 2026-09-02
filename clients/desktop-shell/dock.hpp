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

#ifndef _DS_DOCK_HPP_	//desktop-shell/dock.hpp
#define _DS_DOCK_HPP_

#include "desktop.hpp"
#include "output.hpp"
#include "common.hpp"

class Output;

//Dock
class Dock {
public:
	struct surface base;

    Output *owner;

    struct wl_list launcher_list;
    
	struct window *window;
	struct widget *widget;

	//Display
	enum weston_desktop_shell_dock_position dock_position;
	int painted;
	uint32_t color;

	Dock(Desktop *desktop, Output *output);
	~Dock();

	//Handlers (callbacks)
	static void dock_configure(void *data,
				struct weston_desktop_shell *desktop_shell,
				uint32_t edges, struct window *window,
				int32_t width, int32_t height);
	static void dock_redraw_handler(struct widget *widget, void *data);
	static void dock_resize_handler(struct widget *widget,
		     	int32_t width, int32_t height, void *data);
};

//Dock launcher
class DockLauncher {
public:
	struct widget *widget;
	Dock *dock;
	cairo_surface_t *icon;
	int focused, pressed;
	char *path;
	char *displayname;
	struct wl_list link;
	struct custom_env env;
	char * const *argp;
	char * const *envp;

	~DockLauncher();
	static int dock_launcher_enter_handler(struct widget *widget, struct input *input, float x, float y, void *data);

};

#endif