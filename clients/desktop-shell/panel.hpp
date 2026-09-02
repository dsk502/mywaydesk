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

#ifndef _DS_PANEL_HPP_	//desktop-shell/panel.cpp
#define _DS_PANEL_HPP_

#include "output.hpp"
#include "desktop.hpp"
#include "common.hpp"

class PanelClock;
class Output;
class Desktop;

class Panel {
public:
	struct surface base;

	Output *owner;	//output is the display device (screen)

	struct window *window;	//window is the root wayland window of this shell
	struct widget *widget;	//widget is the UI component in the wayland window
	struct wl_list launcher_list;
	PanelClock *clock;
	int painted;
	enum weston_desktop_shell_panel_position panel_position;
	enum clock_format clock_format;
	uint32_t color;

	//Methods
	Panel(Desktop *desktop, Output *output);
	~Panel();

	void panel_add_launchers(Desktop *desktop);
	void panel_add_launcher(const char *icon, const char *path, const char *displayname);
	void panel_add_clock();

	static void panel_redraw_handler(struct widget *widget, void *data);
	static void panel_resize_handler(struct widget *widget,
		     	int32_t width, int32_t height, void *data);
	static void panel_configure(void *data,
				struct weston_desktop_shell *desktop_shell,
				uint32_t edges, struct window *window,
				int32_t width, int32_t height);
};

class PanelClock {
public:
	struct widget *widget;
	Panel *panel;
	struct toytimer timer;
	char *format_string;
	time_t refresh_timer;

	~PanelClock();

	int clock_timer_reset();

	static void panel_clock_redraw_handler(struct widget *widget, void *data);
};

class PanelLauncher {
public:
	//Datafields
	struct widget *widget;
	Panel *panel;
	cairo_surface_t *icon;
	int focused, pressed;
	char *path;
	char *displayname;
	struct wl_list link;
	struct custom_env env;
	char * const *argp;
	char * const *envp;

	//Methods
	~PanelLauncher();
	void panel_launcher_activate();

	//Handlers (callbacks)
	static void panel_launcher_redraw_handler(struct widget *widget, void *data);
	static int panel_launcher_motion_handler(struct widget *widget, struct input *input,
				uint32_t time, float x, float y, void *data);

	static int panel_launcher_enter_handler(struct widget *widget, struct input *input,
				float x, float y, void *data);
	static void panel_launcher_leave_handler(struct widget *widget,
				struct input *input, void *data);

	static void panel_launcher_button_handler(struct widget *widget,
				struct input *input, uint32_t time,
			    uint32_t button,
			    enum wl_pointer_button_state state, void *data);

	static void panel_launcher_touch_down_handler(struct widget *widget, struct input *input,
				uint32_t serial, uint32_t time, int32_t id,
				float x, float y, void *data);
    static void panel_launcher_touch_up_handler(struct widget *widget, struct input *input,
				uint32_t serial, uint32_t time, int32_t id,
				void *data);

	static void panel_launcher_tablet_tool_proximity_in_handler(struct widget *widget,
				struct tablet_tool *tool,
				struct tablet *tablet, void *data);
	static void panel_launcher_tablet_tool_proximity_out_handler(struct widget *widget,
				struct tablet_tool *tool, void *data);

	static void panel_launcher_tablet_tool_up_handler(struct widget *widget,
				struct tablet_tool *tool,
				void *data);
	
	static void panel_launcher_tablet_tool_button_handler(struct widget *widget,
				struct tablet_tool *tool,
				uint32_t button,
				uint32_t state_w,
				void *data);
	
};

void clock_func(struct toytimer *tt);

#endif