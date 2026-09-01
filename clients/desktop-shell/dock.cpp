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

//Create the dock
/*static struct dock*
dock_create(struct desktop* desktop, struct output* output)*/
//Todo: fix the constructor
Dock::Dock(Desktop *desktop, Output *output)
{
	struct dock *dock;
	struct weston_config_section *s;

	dock = xzalloc(sizeof *dock);
	dock->owner = output;

	//configure
	dock->base.configure = dock_configure;
	dock->window = window_create_custom(desktop->display);
	dock->widget = window_add_widget(dock->window, dock);

	//wl_list_init
	wl_list_init(&dock->launcher_list);

	window_set_title(dock->window, "dock");
	window_set_user_data(dock->window, dock);

	//Redraw and resize handler
	widget_set_redraw_handler(dock->widget, dock_redraw_handler);
	widget_set_resize_handler(dock->widget, dock_resize_handler);

	//Dock position
	dock->dock_position = desktop->dock_position;

	//Read the configuration file
	s = weston_config_get_section(desktop->config, "shell", NULL, NULL);
	//Get color, currently dock-color = panel-color
	weston_config_section_get_color(s, "panel-color", &dock->color, 0xaa000000);
	
	//Todo: Add_launcher

	return dock;
}

//Destroy the dock
/*static void
dock_destroy(struct dock *dock)*/
Dock::~Dock()
{
	struct dock_launcher *tmp;
	struct dock_launcher *launcher;

	wl_list_for_each_safe(launcher, tmp, &this->launcher_list, link)
		dock_destroy_launcher(launcher);

	widget_destroy(this->widget);
	window_destroy(this->window);

	//free(dock);
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

	//free(launcher);
}