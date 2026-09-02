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

#ifndef _DS_DESKTOP_HPP_	//desktop-shell/desktop.hpp
#define _DS_DESKTOP_HPP_

#include "output.hpp"
#include "desktop-shell/common.hpp"
#include "unlockdialog.hpp"

class Desktop {
public:
	struct display *display;
	struct weston_desktop_shell *shell;
	UnlockDialog *unlock_dialog;
	struct task unlock_task;
	struct wl_list outputs;

	int want_panel;
	enum weston_desktop_shell_panel_position panel_position;
	enum weston_desktop_shell_dock_position dock_position;
	enum clock_format clock_format;

	struct window *grab_window;
	struct widget *grab_widget;

	struct weston_config *config;
	bool locking;

	enum cursor_type grab_cursor;

	int painted;

	int is_desktop_painted();
	//void check_desktop_ready(struct window *window);
	void parse_panel_position(struct weston_config_section *s);
	void parse_dock_position(struct weston_config_section *s);
	void parse_clock_format(struct weston_config_section *s);

	void grab_surface_destroy();
	void grab_surface_create();

	void create_output(uint32_t id);
	void output_remove(Output *output);
	void desktop_destroy_outputs();
};

const struct weston_desktop_shell_listener listener;

void check_desktop_ready(struct window *window);

void
desktop_shell_configure(void *data,
			struct weston_desktop_shell *desktop_shell,
			uint32_t edges,
			struct wl_surface *surface,
			int32_t width, int32_t height);

void
desktop_shell_prepare_lock_surface(void *data,
				   struct weston_desktop_shell *desktop_shell);

void
desktop_shell_grab_cursor(void *data,
			  struct weston_desktop_shell *desktop_shell,
			  uint32_t cursor);

int grab_surface_enter_handler(struct widget *widget, struct input *input,
			   float x, float y, void *data);

void global_handler(struct display *display, uint32_t id,
	       const char *interface, uint32_t version, void *data);

void global_handler_remove(struct display *display, uint32_t id,
	       const char *interface, uint32_t version, void *data);

#endif