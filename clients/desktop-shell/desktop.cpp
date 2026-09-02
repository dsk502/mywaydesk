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

#include "desktop.hpp"

int
Desktop::is_desktop_painted()
{
	Output *output;

	wl_list_for_each(output, &this->outputs, link) {
		if (output->panel && !output->panel->painted)
			return 0;
		if (output->background && !output->background->painted)
			return 0;
		if (output->dock && !output->dock->painted)
    		return 0;
	}

	return 1;
}

void
Desktop::parse_panel_position(struct weston_config_section *s)
{
	char *position;

	this->want_panel = 1;

	weston_config_section_get_string(s, "panel-position", &position, "top");
	if (strcmp(position, "top") == 0) {
		this->panel_position = WESTON_DESKTOP_SHELL_PANEL_POSITION_TOP;
	} else if (strcmp(position, "bottom") == 0) {
		this->panel_position = WESTON_DESKTOP_SHELL_PANEL_POSITION_BOTTOM;
	} else if (strcmp(position, "left") == 0) {
		this->panel_position = WESTON_DESKTOP_SHELL_PANEL_POSITION_LEFT;
	} else if (strcmp(position, "right") == 0) {
		this->panel_position = WESTON_DESKTOP_SHELL_PANEL_POSITION_RIGHT;
	} else {
		/* 'none' is valid here */
		if (strcmp(position, "none") != 0)
			fprintf(stderr, "Wrong panel position: %s\n", position);
		this->want_panel = 0;
	}
	free(position);
}

void
Desktop::parse_clock_format(struct weston_config_section *s)
{
	char *clock_format;

	weston_config_section_get_string(s, "clock-format", &clock_format, "");
	if (strcmp(clock_format, "minutes") == 0)
		this->clock_format = CLOCK_FORMAT_MINUTES;
	else if (strcmp(clock_format, "seconds") == 0)
		this->clock_format = CLOCK_FORMAT_SECONDS;
	else if (strcmp(clock_format, "minutes-24h") == 0)
		this->clock_format = CLOCK_FORMAT_MINUTES_24H;
	else if (strcmp(clock_format, "seconds-24h") == 0)
		this->clock_format = CLOCK_FORMAT_SECONDS_24H;
	else if (strcmp(clock_format, "none") == 0)
		this->clock_format = CLOCK_FORMAT_NONE;
	else
		this->clock_format = DEFAULT_CLOCK_FORMAT;
	free(clock_format);
}

void
Desktop::parse_dock_position(struct weston_config_section *s)
{
	//char* position;

	//Currently, only support bottom dock
	this->dock_position = WESTON_DESKTOP_SHELL_DOCK_POSITION_BOTTOM;
}

void
Desktop::grab_surface_destroy()
{
	widget_destroy(this->grab_widget);
	window_destroy(this->grab_window);
}

void
Desktop::grab_surface_create()
{
	struct wl_surface *s;

	this->grab_window = window_create_custom(this->display);
	window_set_user_data(this->grab_window, this);

	s = window_get_wl_surface(this->grab_window);
	weston_desktop_shell_set_grab_surface(this->shell, s);

	this->grab_widget =
		window_add_widget(this->grab_window, this);
	/* We set the allocation to 1x1 at 0,0 so the fake enter event
	 * at 0,0 will go to this widget. */
	widget_set_allocation(this->grab_widget, 0, 0, 1, 1);

	widget_set_enter_handler(this->grab_widget,
				 grab_surface_enter_handler);
}

void
Desktop::create_output(uint32_t id)
{
	Output *output = new Output();

	if (!output)
		return;

	output->output = static_cast<wl_output *>(display_bind(this->display, id, &wl_output_interface, 2));
	output->server_output_id = id;

	wl_output_add_listener(output->output, &output_listener, output);

	wl_list_insert(&this->outputs, &output->link);

	/* On start up we may process an output global before the shell global
	 * in which case we can't create the panel and background just yet */
	if (this->shell)
		output->output_init(this);
}

void
Desktop::output_remove(Output *output)
{
	Output *cur;
	Output *rep = NULL;

	if (!output->background) {
		delete output;
		return;
	}

	/* Find a wl_output that is a clone of the removed wl_output.
	 * We don't want to leave the clone without a background or panel. */
	wl_list_for_each(cur, &this->outputs, link) {
		if (cur == output)
			continue;

		/* XXX: Assumes size matches. */
		if (cur->x == output->x && cur->y == output->y) {
			rep = cur;
			break;
		}
	}

	if (rep) {
		/* If found and it does not already have a background or panel,
		 * hand over the background and panel so they don't get
		 * destroyed.
		 *
		 * We never create multiple backgrounds or panels for clones,
		 * but if the compositor moves outputs, a pair of wl_outputs
		 * might become "clones". This may happen temporarily when
		 * an output is about to be removed and the rest are reflowed.
		 * In this case it is correct to let the background/panel be
		 * destroyed.
		 */

		if (!rep->background) {
			rep->background = output->background;
			output->background = NULL;
			rep->background->owner = rep;
		}

		if (!rep->panel) {
			rep->panel = output->panel;
			output->panel = NULL;
			if (rep->panel)
				rep->panel->owner = rep;
		}

		if(!rep->dock) {
			rep->dock = output->dock;
			output->dock = NULL;
			if (rep->dock)
				rep->dock->owner = rep;
		}
	}

	delete output;
}

void
Desktop::desktop_destroy_outputs()
{
	Output *tmp;
	Output *output;

	wl_list_for_each_safe(output, tmp, &this->outputs, link)
		delete output;
}

void
check_desktop_ready(struct window *window)
{
	struct display *display;
	Desktop *desktop;

	display = window_get_display(window);
	desktop = static_cast<Desktop *>(display_get_user_data(display));

	if (!desktop->painted && desktop->is_desktop_painted()) {
		desktop->painted = 1;

		weston_desktop_shell_desktop_ready(desktop->shell);
	}
}

void
desktop_shell_configure(void *data,
			struct weston_desktop_shell *desktop_shell,
			uint32_t edges,
			struct wl_surface *surface,
			int32_t width, int32_t height)
{
	struct window *window = static_cast<struct window *>(wl_surface_get_user_data(surface));
	struct surface *s = static_cast<struct surface *>(window_get_user_data(window));

	s->configure(data, desktop_shell, edges, window, width, height);
}

void
desktop_shell_prepare_lock_surface(void *data,
				   struct weston_desktop_shell *desktop_shell)
{
	Desktop *desktop = static_cast<Desktop *>(data);

	if (!desktop->locking) {
		weston_desktop_shell_unlock(desktop->shell);
		return;
	}

	if (!desktop->unlock_dialog) {
		desktop->unlock_dialog = new UnlockDialog(desktop);
		desktop->unlock_dialog->desktop = desktop;
	}
}

void
desktop_shell_grab_cursor(void *data,
			  struct weston_desktop_shell *desktop_shell,
			  uint32_t cursor)
{
	Desktop *desktop = static_cast<Desktop *>(data);

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

int
grab_surface_enter_handler(struct widget *widget, struct input *input,
			   float x, float y, void *data)
{
	Desktop *desktop = static_cast<Desktop *>(data);

	return desktop->grab_cursor;
}

void
global_handler(struct display *display, uint32_t id,
	       const char *interface, uint32_t version, void *data)
{
	Desktop *desktop = static_cast<Desktop *>(data);

	if (!strcmp(interface, "weston_desktop_shell")) {
		desktop->shell = static_cast<struct weston_desktop_shell *>(display_bind(desktop->display,
					      id, &weston_desktop_shell_interface, 1));
		weston_desktop_shell_add_listener(desktop->shell,
						  &listener,
						  desktop);
	} else if (!strcmp(interface, "wl_output")) {
		desktop->create_output(id);
	}
}

void
global_handler_remove(struct display *display, uint32_t id,
	       const char *interface, uint32_t version, void *data)
{
	Desktop *desktop = static_cast<Desktop *>(data);
	Output *output;

	if (!strcmp(interface, "wl_output")) {
		wl_list_for_each(output, &desktop->outputs, link) {
			if (output->server_output_id == id) {
				desktop->output_remove(output);
				break;
			}
		}
	}
}

const struct weston_desktop_shell_listener listener = {
	desktop_shell_configure,
	desktop_shell_prepare_lock_surface,
	desktop_shell_grab_cursor
};