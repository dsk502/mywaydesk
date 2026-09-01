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