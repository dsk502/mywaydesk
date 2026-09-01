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

#include "dock.hpp"

//Create the dock
/*static struct dock*
dock_create(struct desktop* desktop, struct output* output)*/
//Todo: fix the constructor
Dock::Dock(Desktop *desktop, Output *output)
{
	struct weston_config_section *s;

	//dock = xzalloc(sizeof *dock);
	this->owner = output;

	//configure
	this->base.configure = dock_configure;
	this->window = window_create_custom(desktop->display);
	this->widget = window_add_widget(this->window, this);

	//wl_list_init
	wl_list_init(&this->launcher_list);

	window_set_title(this->window, "dock");
	//Todo: confirm this
	//window_set_user_data(this->window, this);
	window_set_user_data(this->window, &this->base);

	//Redraw and resize handler
	widget_set_redraw_handler(this->widget, dock_redraw_handler);
	widget_set_resize_handler(this->widget, dock_resize_handler);

	//Dock position
	this->dock_position = desktop->dock_position;

	//Read the configuration file
	s = weston_config_get_section(desktop->config, "shell", NULL, NULL);
	//Get color, currently dock-color = panel-color
	weston_config_section_get_color(s, "panel-color", &this->color, 0xaa000000);
	
	//Todo: Add_launcher

}

//Destroy the dock
/*static void
dock_destroy(struct dock *dock)*/
Dock::~Dock()
{
	DockLauncher *tmp;
	DockLauncher *launcher;

	wl_list_for_each_safe(launcher, tmp, &this->launcher_list, link)
		delete launcher;

	widget_destroy(this->widget);
	window_destroy(this->window);

	//free(dock);
}

void
Dock::dock_configure(void *data,
		struct weston_desktop_shell *desktop_shell,
		uint32_t edges, struct window *window,
		int32_t width, int32_t height)
{
	Desktop *desktop = static_cast<Desktop *>(data);
	struct surface *surface = static_cast<struct surface *>(window_get_user_data(window));
	Dock *dock = container_of(surface, Dock, base);
	Output *owner;

	if (width < 1 || height < 1) {
		/* Shell plugin configures 0x0 for redundant dock. */
		owner = dock->owner;
		//Destroy the dock
		delete dock;
		owner->dock = NULL;
		return;
	}

	switch (desktop->dock_position) {
		case WESTON_DESKTOP_SHELL_DOCK_POSITION_TOP:
		case WESTON_DESKTOP_SHELL_DOCK_POSITION_BOTTOM:
			height = 64;
			break;
		case WESTON_DESKTOP_SHELL_DOCK_POSITION_LEFT:
		case WESTON_DESKTOP_SHELL_DOCK_POSITION_RIGHT:
			//Todo here
			break;
	}
	window_schedule_resize(dock->window, width, height);
}

void
Dock::dock_redraw_handler(struct widget *widget, void *data)
{
	cairo_surface_t *surface;
	cairo_t *cr;
	Dock *dock = static_cast<Dock *>(data);

	cr = widget_cairo_create(dock->widget);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	set_hex_color(cr, dock->color);
	cairo_paint(cr);

	cairo_destroy(cr);
	surface = window_get_surface(dock->window);
	cairo_surface_destroy(surface);
	dock->painted = 1;
	check_desktop_ready(dock->window);
}

void
Dock::dock_resize_handler(struct widget *widget,
		     int32_t width, int32_t height, void *data)
{
	//DockLauncher *launcher;
	Dock *dock = static_cast<Dock *>(data);
	int x = 0;
	int y = 0;
	int w = height > width ? width : height;
	int h = w;
	int horizontal;

	if (dock->dock_position == WESTON_DESKTOP_SHELL_DOCK_POSITION_BOTTOM ||
		dock->dock_position == WESTON_DESKTOP_SHELL_DOCK_POSITION_TOP) {
		horizontal = 1;
	} else {
		horizontal = 0;
	}

	int first_pad_h = horizontal ? 0 : DEFAULT_SPACING / 2;
	int first_pad_w = horizontal ? DEFAULT_SPACING / 2 : 0;

	w = 170;

	if (horizontal)
		x = width - w;
	else
		y = height - (h = DEFAULT_SPACING * 3);

}

/*
static void
dock_destroy_launcher(struct dock_launcher *launcher)*/
DockLauncher::~DockLauncher()
{
	custom_env_fini(&this->env);

	free(this->path);
	free(this->displayname);

	cairo_surface_destroy(this->icon);

	widget_destroy(this->widget);
	wl_list_remove(&this->link);

}

int
DockLauncher::dock_launcher_enter_handler(struct widget *widget, struct input *input, float x, float y, void *data)
{
	DockLauncher *launcher = static_cast<DockLauncher *>(data);

	launcher->focused = 1;
	widget_schedule_redraw(widget);

	return CURSOR_LEFT_PTR;
}