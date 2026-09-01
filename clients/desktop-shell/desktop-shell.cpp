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

#include "desktop.cpp"

#define DEFAULT_CLOCK_FORMAT CLOCK_FORMAT_MINUTES
#define DEFAULT_SPACING 10

static void
unlock_dialog_finish(struct task *task, uint32_t events)
{
	Desktop *desktop =
		container_of(task, Desktop, unlock_task);

	weston_desktop_shell_unlock(desktop->shell);
	delete (desktop->unlock_dialog);
	desktop->unlock_dialog = NULL;
}

/*
static void
panel_add_launchers(struct panel *panel, struct desktop *desktop);
*/

static void
sigchild_handler(int s)
{
	int status;
	pid_t pid;

	while (pid = waitpid(-1, &status, WNOHANG), pid > 0)
		fprintf(stderr, "child %d exited\n", pid);
}



static void
panel_redraw_handler(struct widget *widget, void *data)
{
	cairo_surface_t *surface;
	cairo_t *cr;
	struct panel *panel = data;

	cr = widget_cairo_create(panel->widget);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	set_hex_color(cr, panel->color);
	cairo_paint(cr);

	cairo_destroy(cr);
	surface = window_get_surface(panel->window);
	cairo_surface_destroy(surface);
	panel->painted = 1;
	check_desktop_ready(panel->window);
}

//static int clock_timer_reset(struct panel_clock *clock);

static void
clock_func(struct toytimer *tt)
{
	struct panel_clock *clock = container_of(tt, struct panel_clock, timer);

	widget_schedule_redraw(clock->widget);

	clock_timer_reset(clock);
}

static void
panel_clock_redraw_handler(struct widget *widget, void *data)
{
	struct panel_clock *clock = data;
	cairo_t *cr;
	struct rectangle allocation;
	cairo_text_extents_t extents;
	time_t rawtime;
	struct tm * timeinfo;
	char string[128];

	time(&rawtime);
	timeinfo = localtime(&rawtime);
	strftime(string, sizeof string, clock->format_string, timeinfo);

	widget_get_allocation(widget, &allocation);
	if (allocation.width == 0)
		return;

	cr = widget_cairo_create(clock->panel->widget);
	cairo_set_font_size(cr, 14);
	cairo_text_extents(cr, string, &extents);
	if (allocation.x > 0)
		allocation.x +=
			allocation.width - DEFAULT_SPACING * 1.5 - extents.width;
	else
		allocation.x +=
			allocation.width / 2 - extents.width / 2;
	allocation.y += allocation.height / 2 - 1 + extents.height / 2;
	cairo_move_to(cr, allocation.x + 1, allocation.y + 1);
	cairo_set_source_rgba(cr, 0, 0, 0, 0.85);
	cairo_show_text(cr, string);
	cairo_move_to(cr, allocation.x, allocation.y);
	cairo_set_source_rgba(cr, 1, 1, 1, 0.85);
	cairo_show_text(cr, string);
	cairo_destroy(cr);
}





static void
panel_resize_handler(struct widget *widget,
		     int32_t width, int32_t height, void *data)
{
	struct panel_launcher *launcher;
	struct panel *panel = data;
	int x = 0;
	int y = 0;
	int w = height > width ? width : height;
	int h = w;
	int horizontal = panel->panel_position == WESTON_DESKTOP_SHELL_PANEL_POSITION_TOP || panel->panel_position == WESTON_DESKTOP_SHELL_PANEL_POSITION_BOTTOM;
	int first_pad_h = horizontal ? 0 : DEFAULT_SPACING / 2;
	int first_pad_w = horizontal ? DEFAULT_SPACING / 2 : 0;

	wl_list_for_each(launcher, &panel->launcher_list, link) {
		widget_set_allocation(launcher->widget, x, y,
				      w + first_pad_w + 1, h + first_pad_h + 1);
		if (horizontal)
			x += w + first_pad_w;
		else
			y += h + first_pad_h;
		first_pad_h = first_pad_w = 0;
	}

	if (panel->clock_format == CLOCK_FORMAT_SECONDS)
		w = 170;
	else /* CLOCK_FORMAT_MINUTES and 24H versions */
		w = 150;

	if (horizontal)
		x = width - w;
	else
		y = height - (h = DEFAULT_SPACING * 3);

	if (panel->clock)
		widget_set_allocation(panel->clock->widget,
				      x, y, w + 1, h + 1);
}

/*
static void
panel_destroy(struct panel *panel);*/

static void
panel_configure(void *data,
		struct weston_desktop_shell *desktop_shell,
		uint32_t edges, struct window *window,
		int32_t width, int32_t height)
{
	struct desktop *desktop = data;
	struct surface *surface = window_get_user_data(window);
	struct panel *panel = container_of(surface, struct panel, base);
	struct output *owner;

	if (width < 1 || height < 1) {
		/* Shell plugin configures 0x0 for redundant panel. */
		owner = panel->owner;
		panel_destroy(panel);
		owner->panel = NULL;
		return;
	}

	switch (desktop->panel_position) {
	case WESTON_DESKTOP_SHELL_PANEL_POSITION_TOP:
	case WESTON_DESKTOP_SHELL_PANEL_POSITION_BOTTOM:
		height = 32;
		break;
	case WESTON_DESKTOP_SHELL_PANEL_POSITION_LEFT:
	case WESTON_DESKTOP_SHELL_PANEL_POSITION_RIGHT:
		switch (desktop->clock_format) {
		case CLOCK_FORMAT_NONE:
			width = 32;
			break;
		case CLOCK_FORMAT_MINUTES:
		case CLOCK_FORMAT_MINUTES_24H:
		case CLOCK_FORMAT_SECONDS_24H:
			width = 150;
			break;
		case CLOCK_FORMAT_SECONDS:
			width = 170;
			break;
		}
		break;
	}
	window_schedule_resize(panel->window, width, height);
}

static cairo_surface_t *
load_icon_or_fallback(const char *icon)
{
	cairo_surface_t *surface = cairo_image_surface_create_from_png(icon);
	cairo_status_t status;
	cairo_t *cr;

	status = cairo_surface_status(surface);
	if (status == CAIRO_STATUS_SUCCESS)
		return surface;

	cairo_surface_destroy(surface);
	fprintf(stderr, "ERROR loading icon from file '%s', error: '%s'\n",
		icon, cairo_status_to_string(status));

	/* draw fallback icon */
	surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
					     20, 20);
	cr = cairo_create(surface);

	cairo_set_source_rgba(cr, 0.8, 0.8, 0.8, 1);
	cairo_paint(cr);

	cairo_set_source_rgba(cr, 0, 0, 0, 1);
	cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
	cairo_rectangle(cr, 0, 0, 20, 20);
	cairo_move_to(cr, 4, 4);
	cairo_line_to(cr, 16, 16);
	cairo_move_to(cr, 4, 16);
	cairo_line_to(cr, 16, 4);
	cairo_stroke(cr);

	cairo_destroy(cr);

	return surface;
}










static void
unlock_dialog_redraw_handler(struct widget *widget, void *data)
{
	struct unlock_dialog *dialog = data;
	struct rectangle allocation;
	cairo_surface_t *surface;
	cairo_t *cr;
	cairo_pattern_t *pat;
	double cx, cy, r, f;

	cr = widget_cairo_create(widget);

	widget_get_allocation(dialog->widget, &allocation);
	cairo_rectangle(cr, allocation.x, allocation.y,
			allocation.width, allocation.height);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba(cr, 0, 0, 0, 0.6);
	cairo_fill(cr);

	cairo_translate(cr, allocation.x, allocation.y);
	if (dialog->button_focused)
		f = 1.0;
	else
		f = 0.7;

	cx = allocation.width / 2.0;
	cy = allocation.height / 2.0;
	r = (cx < cy ? cx : cy) * 0.4;
	pat = cairo_pattern_create_radial(cx, cy, r * 0.7, cx, cy, r);
	cairo_pattern_add_color_stop_rgb(pat, 0.0, 0, 0.86 * f, 0);
	cairo_pattern_add_color_stop_rgb(pat, 0.85, 0.2 * f, f, 0.2 * f);
	cairo_pattern_add_color_stop_rgb(pat, 1.0, 0, 0.86 * f, 0);
	cairo_set_source(cr, pat);
	cairo_pattern_destroy(pat);
	cairo_arc(cr, cx, cy, r, 0.0, 2.0 * M_PI);
	cairo_fill(cr);

	widget_set_allocation(dialog->button,
			      allocation.x + cx - r,
			      allocation.y + cy - r, 2 * r, 2 * r);

	cairo_destroy(cr);

	surface = window_get_surface(dialog->window);
	cairo_surface_destroy(surface);
}

static void
unlock_dialog_button_handler(struct widget *widget,
			     struct input *input, uint32_t time,
			     uint32_t button,
			     enum wl_pointer_button_state state, void *data)
{
	struct unlock_dialog *dialog = data;
	struct desktop *desktop = dialog->desktop;

	if (button == BTN_LEFT) {
		if (state == WL_POINTER_BUTTON_STATE_RELEASED &&
		    !dialog->closing) {
			display_defer(desktop->display, &desktop->unlock_task);
			dialog->closing = 1;
		}
	}
}

static void
unlock_dialog_touch_down_handler(struct widget *widget, struct input *input,
		   uint32_t serial, uint32_t time, int32_t id,
		   float x, float y, void *data)
{
	struct unlock_dialog *dialog = data;

	dialog->button_focused = 1;
	widget_schedule_redraw(widget);
}

static void
unlock_dialog_touch_up_handler(struct widget *widget, struct input *input,
				uint32_t serial, uint32_t time, int32_t id,
				void *data)
{
	struct unlock_dialog *dialog = data;
	struct desktop *desktop = dialog->desktop;

	dialog->button_focused = 0;
	widget_schedule_redraw(widget);
	display_defer(desktop->display, &desktop->unlock_task);
	dialog->closing = 1;
}

static void
unlock_dialog_keyboard_focus_handler(struct window *window,
				     struct input *device, void *data)
{
	window_schedule_redraw(window);
}

static int
unlock_dialog_widget_enter_handler(struct widget *widget,
				   struct input *input,
				   float x, float y, void *data)
{
	struct unlock_dialog *dialog = data;

	dialog->button_focused = 1;
	widget_schedule_redraw(widget);

	return CURSOR_LEFT_PTR;
}

static void
unlock_dialog_widget_leave_handler(struct widget *widget,
				   struct input *input, void *data)
{
	struct unlock_dialog *dialog = data;

	dialog->button_focused = 0;
	widget_schedule_redraw(widget);
}


static void
desktop_shell_configure(void *data,
			struct weston_desktop_shell *desktop_shell,
			uint32_t edges,
			struct wl_surface *surface,
			int32_t width, int32_t height)
{
	struct window *window = wl_surface_get_user_data(surface);
	struct surface *s = window_get_user_data(window);

	s->configure(data, desktop_shell, edges, window, width, height);
}

static void
desktop_shell_prepare_lock_surface(void *data,
				   struct weston_desktop_shell *desktop_shell)
{
	struct desktop *desktop = data;

	if (!desktop->locking) {
		weston_desktop_shell_unlock(desktop->shell);
		return;
	}

	if (!desktop->unlock_dialog) {
		desktop->unlock_dialog = unlock_dialog_create(desktop);
		desktop->unlock_dialog->desktop = desktop;
	}
}

static void
desktop_shell_grab_cursor(void *data,
			  struct weston_desktop_shell *desktop_shell,
			  uint32_t cursor)
{
	struct desktop *desktop = data;

	switch (cursor) {
	case WESTON_DESKTOP_SHELL_CURSOR_NONE:
		desktop->grab_cursor = CURSOR_BLANK;
		break;
	case WESTON_DESKTOP_SHELL_CURSOR_BUSY:
		desktop->grab_cursor = CURSOR_WATCH;
		break;
	case WESTON_DESKTOP_SHELL_CURSOR_MOVE:
		desktop->grab_cursor = CURSOR_DRAGGING;
		break;
	case WESTON_DESKTOP_SHELL_CURSOR_RESIZE_TOP:
		desktop->grab_cursor = CURSOR_TOP;
		break;
	case WESTON_DESKTOP_SHELL_CURSOR_RESIZE_BOTTOM:
		desktop->grab_cursor = CURSOR_BOTTOM;
		break;
	case WESTON_DESKTOP_SHELL_CURSOR_RESIZE_LEFT:
		desktop->grab_cursor = CURSOR_LEFT;
		break;
	case WESTON_DESKTOP_SHELL_CURSOR_RESIZE_RIGHT:
		desktop->grab_cursor = CURSOR_RIGHT;
		break;
	case WESTON_DESKTOP_SHELL_CURSOR_RESIZE_TOP_LEFT:
		desktop->grab_cursor = CURSOR_TOP_LEFT;
		break;
	case WESTON_DESKTOP_SHELL_CURSOR_RESIZE_TOP_RIGHT:
		desktop->grab_cursor = CURSOR_TOP_RIGHT;
		break;
	case WESTON_DESKTOP_SHELL_CURSOR_RESIZE_BOTTOM_LEFT:
		desktop->grab_cursor = CURSOR_BOTTOM_LEFT;
		break;
	case WESTON_DESKTOP_SHELL_CURSOR_RESIZE_BOTTOM_RIGHT:
		desktop->grab_cursor = CURSOR_BOTTOM_RIGHT;
		break;
	case WESTON_DESKTOP_SHELL_CURSOR_ARROW:
	default:
		desktop->grab_cursor = CURSOR_LEFT_PTR;
	}
}

static const struct weston_desktop_shell_listener listener = {
	desktop_shell_configure,
	desktop_shell_prepare_lock_surface,
	desktop_shell_grab_cursor
};





static int
grab_surface_enter_handler(struct widget *widget, struct input *input,
			   float x, float y, void *data)
{
	struct desktop *desktop = data;

	return desktop->grab_cursor;
}

/*
static void
dock_destroy(struct dock* dock);*/

static void
desktop_destroy_outputs(struct desktop *desktop)
{
	struct output *tmp;
	struct output *output;

	wl_list_for_each_safe(output, tmp, &desktop->outputs, link)
		output_destroy(output);
}

static void
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
	struct output *output = data;

	output->x = x;
	output->y = y;

	if (output->panel)
		window_set_buffer_transform(output->panel->window, transform);
	if (output->background)
		window_set_buffer_transform(output->background->window, transform);
}

static void
output_handle_mode(void *data,
		   struct wl_output *wl_output,
		   uint32_t flags,
		   int width,
		   int height,
		   int refresh)
{
}

static void
output_handle_done(void *data,
                   struct wl_output *wl_output)
{
}

static void
output_handle_scale(void *data,
                    struct wl_output *wl_output,
                    int32_t scale)
{
	struct output *output = data;

	if (output->panel)
		window_set_buffer_scale(output->panel->window, scale);
	if (output->background)
		window_set_buffer_scale(output->background->window, scale);
}

static const struct wl_output_listener output_listener = {
	output_handle_geometry,
	output_handle_mode,
	output_handle_done,
	output_handle_scale
};

/*
static struct dock*
dock_create(struct desktop* desktop, struct output* output);*/

static void
global_handler(struct display *display, uint32_t id,
	       const char *interface, uint32_t version, void *data)
{
	struct desktop *desktop = data;

	if (!strcmp(interface, "weston_desktop_shell")) {
		desktop->shell = display_bind(desktop->display,
					      id,
					      &weston_desktop_shell_interface,
					      1);
		weston_desktop_shell_add_listener(desktop->shell,
						  &listener,
						  desktop);
	} else if (!strcmp(interface, "wl_output")) {
		create_output(desktop, id);
	}
}

static void
global_handler_remove(struct display *display, uint32_t id,
	       const char *interface, uint32_t version, void *data)
{
	struct desktop *desktop = data;
	struct output *output;

	if (!strcmp(interface, "wl_output")) {
		wl_list_for_each(output, &desktop->outputs, link) {
			if (output->server_output_id == id) {
				output_remove(desktop, output);
				break;
			}
		}
	}
}


















int main(int argc, char *argv[])
{
	Desktop desktop = { 0 };
	Output *output;
	struct weston_config_section *s;
	const char *config_file;

	desktop.unlock_task.run = unlock_dialog_finish;
	wl_list_init(&desktop.outputs);

	//Read the config file
	config_file = weston_config_get_name_from_env();
	desktop.config = weston_config_parse(config_file);
	s = weston_config_get_section(desktop.config, "shell", NULL, NULL);
	weston_config_section_get_bool(s, "locking", &desktop.locking, true);

	//Parse panel position
	parse_panel_position(&desktop, s);
	parse_clock_format(&desktop, s);
	//Parse dock position
	parse_dock_position(&desktop, s);

	desktop.display = display_create(&argc, argv);
	if (desktop.display == NULL) {
		fprintf(stderr, "failed to create display: %s\n",
			strerror(errno));
		weston_config_destroy(desktop.config);
		return -1;
	}

	display_set_user_data(desktop.display, &desktop);
	//global_handler() -> create_output() -> output_init() -> panel_create()
	display_set_global_handler(desktop.display, global_handler);	//Set and invoke global_handler()
	display_set_global_handler_remove(desktop.display, global_handler_remove);

	/* Create panel and background for outputs processed before the shell
	 * global interface was processed */
	if (desktop.want_panel) {
		weston_desktop_shell_set_panel_position(desktop.shell, desktop.panel_position);
		//Add the dock to the desktop
		weston_desktop_shell_set_dock_position(desktop.shell, desktop.dock_position);
	}
	wl_list_for_each(output, &desktop.outputs, link)
		if (!output->background)
			output_init(output, &desktop);	//output_init() -> panel_create()

	grab_surface_create(&desktop);

	signal(SIGCHLD, sigchild_handler);

	display_run(desktop.display);

	/* Cleanup */
	grab_surface_destroy(&desktop);
	desktop_destroy_outputs(&desktop);
	if (desktop.unlock_dialog)
		unlock_dialog_destroy(desktop.unlock_dialog);
	weston_desktop_shell_destroy(desktop.shell);
	display_destroy(desktop.display);
	weston_config_destroy(desktop.config);

	return 0;
}
