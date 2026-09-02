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

#include "panel.hpp"

/*static struct panel *
panel_create(struct desktop *desktop, struct output *output)*/
Panel::Panel(Desktop *desktop, Output *output)
{
	//struct panel *panel;
	struct weston_config_section *s;

	//panel = xzalloc(sizeof *panel);

	this->owner = output;
	//Set base configure function for panel
	this->base.configure = panel_configure;
	this->window = window_create_custom(desktop->display);
	this->widget = window_add_widget(this->window, this);
	wl_list_init(&this->launcher_list);

	window_set_title(this->window, "panel");
	//Todo: fix problems of void *data (this)
	window_set_user_data(this->window, this);

	//Set redraw and resize handlers
	widget_set_redraw_handler(this->widget, panel_redraw_handler);
	widget_set_resize_handler(this->widget, panel_resize_handler);

	//Panel position
	this->panel_position = desktop->panel_position;

	//Clock
	//Todo: fix the clock addition
	this->clock_format = desktop->clock_format;
	if (this->clock_format != CLOCK_FORMAT_NONE)
		panel_add_clock();

	//Read the configuration file
	s = weston_config_get_section(desktop->config, "shell", NULL, NULL);
	weston_config_section_get_color(s, "panel-color",
					&this->color, 0xaa000000);

	//Todo: check the invoke
	panel_add_launchers(desktop);

	//return panel;
}

Panel::~Panel()
{
	PanelLauncher *tmp;
	PanelLauncher *launcher;

	if (this->clock)
		delete (this->clock);
		//panel_destroy_clock(this->clock);

	wl_list_for_each_safe(launcher, tmp, &this->launcher_list, link)
		delete launcher;
		//panel_destroy_launcher(launcher);

	widget_destroy(this->widget);
	window_destroy(this->window);

	//free(this);
}

void
Panel::panel_add_launchers(Desktop *desktop)
{
	struct weston_config_section *s;
	char *icon, *path, *displayname;
	const char *name;
	int count;

	count = 0;
	s = NULL;
	//Iterate the configuration file weston.ini, find installed applications mentioned in the file, and add them to the panel
	while (weston_config_next_section(desktop->config, &s, &name)) {
		if (strcmp(name, "launcher") != 0)
			continue;

		weston_config_section_get_string(s, "icon", &icon, NULL);
		weston_config_section_get_string(s, "path", &path, NULL);
		weston_config_section_get_string(s, "displayname", &displayname, NULL);
		if (displayname == NULL)
			displayname = static_cast<char *>(xstrdup(basename(path)));

		if (icon != NULL && path != NULL) {
			panel_add_launcher(icon, path, displayname);
			count++;
		} else {
			fprintf(stderr, "invalid launcher section\n");
		}

		free(icon);
		free(path);
		free(displayname);
	}

	if (count == 0) {
		char *name = file_name_with_datadir("terminal.png");

		/* add default launcher */
		panel_add_launcher(name,
				   BINDIR "/weston-terminal",
				   "Terminal");
		free(name);
	}
}

/*
static void
panel_add_launcher(struct panel *panel, const char *icon, const char *path, const char *displayname)
*/

void
Panel::panel_add_launcher(const char *icon, const char *path, const char *displayname)
{
	//Todo: fix the memory problem of PanelLauncher
	//Todo: edit the callbacks
	PanelLauncher *launcher = new PanelLauncher();

	//launcher = xzalloc(sizeof *launcher);
	launcher->icon = load_icon_or_fallback(icon);
	launcher->path = static_cast<char *>(xstrdup(path));
	launcher->displayname = static_cast<char *>(xstrdup(displayname));

	custom_env_init_from_environ(&launcher->env);
	custom_env_add_from_exec_string(&launcher->env, launcher->path);
	launcher->envp = custom_env_get_envp(&launcher->env);
	launcher->argp = custom_env_get_argp(&launcher->env);

	launcher->panel = this;
	wl_list_insert(this->launcher_list.prev, &launcher->link);

	launcher->widget = widget_add_widget(this->widget, launcher);
	widget_set_enter_handler(launcher->widget,
				PanelLauncher::panel_launcher_enter_handler);
	widget_set_leave_handler(launcher->widget,
				PanelLauncher::panel_launcher_leave_handler);
	widget_set_button_handler(launcher->widget,
				PanelLauncher::panel_launcher_button_handler);
	widget_set_touch_down_handler(launcher->widget,
				PanelLauncher::panel_launcher_touch_down_handler);
	widget_set_touch_up_handler(launcher->widget,
				PanelLauncher::panel_launcher_touch_up_handler);
	widget_set_tablet_tool_up_handler(launcher->widget,
				PanelLauncher::panel_launcher_tablet_tool_up_handler);
	widget_set_tablet_tool_proximity_handlers(launcher->widget,
				PanelLauncher::panel_launcher_tablet_tool_proximity_in_handler,
				PanelLauncher::panel_launcher_tablet_tool_proximity_out_handler);
	widget_set_tablet_tool_button_handler(launcher->widget,
				PanelLauncher::panel_launcher_tablet_tool_button_handler);
	widget_set_redraw_handler(launcher->widget,
				PanelLauncher::panel_launcher_redraw_handler);
	widget_set_motion_handler(launcher->widget,
				PanelLauncher::panel_launcher_motion_handler);
}

void
Panel::panel_add_clock()
{
	PanelClock *clock = new PanelClock();

	clock->panel = this;
	this->clock = clock;

	switch (this->clock_format) {
	case CLOCK_FORMAT_MINUTES:
		clock->format_string = "%a %b %d, %I:%M %p";
		clock->refresh_timer = 60;
		break;
	case CLOCK_FORMAT_SECONDS:
		clock->format_string = "%a %b %d, %I:%M:%S %p";
		clock->refresh_timer = 1;
		break;
	case CLOCK_FORMAT_MINUTES_24H:
		clock->format_string = "%a %b %d, %H:%M";
		clock->refresh_timer = 60;
		break;
	case CLOCK_FORMAT_SECONDS_24H:
		clock->format_string = "%a %b %d, %H:%M:%S";
		clock->refresh_timer = 1;
		break;
	case CLOCK_FORMAT_NONE:
		assert(!"not reached");
	}

	toytimer_init(&clock->timer, CLOCK_MONOTONIC,
		      window_get_display(this->window), clock_func);
	clock->clock_timer_reset();

	clock->widget = widget_add_widget(this->widget, clock);
	widget_set_redraw_handler(clock->widget, PanelClock::panel_clock_redraw_handler);
}

void
clock_func(struct toytimer *tt)
{
	PanelClock *clock = container_of(tt, PanelClock, timer);

	widget_schedule_redraw(clock->widget);

	clock->clock_timer_reset();
}

void
Panel::panel_redraw_handler(struct widget *widget, void *data)
{
	cairo_surface_t *surface;
	cairo_t *cr;
	Panel *panel = static_cast<Panel *>(data);

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

void
Panel::panel_resize_handler(struct widget *widget,
		     int32_t width, int32_t height, void *data)
{
	PanelLauncher *launcher;
	Panel *panel = static_cast<Panel *>(data);
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

void
Panel::panel_configure(void *data,
		struct weston_desktop_shell *desktop_shell,
		uint32_t edges, struct window *window,
		int32_t width, int32_t height)
{
	Desktop *desktop = static_cast<Desktop *>(data);
	struct surface *surface = static_cast<struct surface *>(window_get_user_data(window));
	Panel *panel = container_of(surface, Panel, base);
	Output *owner;

	if (width < 1 || height < 1) {
		/* Shell plugin configures 0x0 for redundant panel. */
		owner = panel->owner;
		delete panel;
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

int
PanelClock::clock_timer_reset()
{
	struct itimerspec its;
	struct timespec ts;
	struct tm *tm;

	clock_gettime(CLOCK_REALTIME, &ts);
	tm = localtime(&ts.tv_sec);

	its.it_interval.tv_sec = 0;
	its.it_interval.tv_nsec = 0;
	its.it_value.tv_sec = this->refresh_timer - tm->tm_sec % this->refresh_timer;
	its.it_value.tv_nsec = 10000000; /* 10 ms late to ensure the clock digit has actually changed */
	timespec_add_nsec(&its.it_value, &its.it_value, -ts.tv_nsec);

	toytimer_arm(&this->timer, &its);
	return 0;
}

/*static void
panel_destroy_clock(struct panel_clock *clock)*/
PanelClock::~PanelClock()
{
	widget_destroy(this->widget);
	toytimer_fini(&this->timer);
	//free(clock);
}

void
PanelClock::panel_clock_redraw_handler(struct widget *widget, void *data)
{
	PanelClock *clock = static_cast<PanelClock *>(data);
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

/*void
panel_destroy_launcher()*/
PanelLauncher::~PanelLauncher()
{
	custom_env_fini(&this->env);

	free(this->path);
	free(this->displayname);

	cairo_surface_destroy(this->icon);

	widget_destroy(this->widget);
	wl_list_remove(&this->link);

	free(this);
}

void
PanelLauncher::panel_launcher_activate()
{
	pid_t pid;

	pid = fork();
	if (pid < 0) {
		fprintf(stderr, "fork failed: %s\n", strerror(errno));
		return;
	}

	if (pid)
		return;

	if (setsid() == -1)
		exit(EXIT_FAILURE);

	if (execve(this->argp[0], this->argp, this->envp) < 0) {
		fprintf(stderr, "execl '%s' failed: %s\n", this->argp[0],
			strerror(errno));
		exit(1);
	}
}

/*void
PanelLauncher::panel_launcher_redraw_handler(struct widget *widget, void *data)*/
void
PanelLauncher::panel_launcher_redraw_handler(struct widget *widget, void *data)
{
	PanelLauncher *launcher = static_cast<PanelLauncher *>(data);
	struct rectangle allocation;
	cairo_t *cr;

	cr = widget_cairo_create(launcher->panel->widget);

	widget_get_allocation(widget, &allocation);
	allocation.x += allocation.width / 2 -
		cairo_image_surface_get_width(launcher->icon) / 2;
	if (allocation.width > allocation.height)
		allocation.x += allocation.width / 2 - allocation.height / 2;
	allocation.y += allocation.height / 2 -
		cairo_image_surface_get_height(launcher->icon) / 2;
	if (allocation.height > allocation.width)
		allocation.y += allocation.height / 2 - allocation.width / 2;
	if (launcher->pressed) {
		allocation.x++;
		allocation.y++;
	}

	cairo_set_source_surface(cr, launcher->icon,
				 allocation.x, allocation.y);
	cairo_paint(cr);

	if (launcher->focused) {
		cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.4);
		cairo_mask_surface(cr, launcher->icon,
				   allocation.x, allocation.y);
	}

	cairo_destroy(cr);
}

/*int
PanelLauncher::panel_launcher_motion_handler(struct widget *widget, struct input *input,
			      uint32_t time, float x, float y, void *data)*/
int
PanelLauncher::panel_launcher_motion_handler(struct widget *widget, struct input *input,
			      uint32_t time, float x, float y, void *data)
{
	PanelLauncher *launcher = static_cast<PanelLauncher *>(data);

	widget_set_tooltip(widget, launcher->displayname, x, y);

	return CURSOR_LEFT_PTR;
}

/*int
PanelLauncher::panel_launcher_enter_handler(struct widget *widget, struct input *input,
			     float x, float y, void *data)*/
int
PanelLauncher::panel_launcher_enter_handler(struct widget *widget, struct input *input,
			     float x, float y, void *data)
{
	PanelLauncher *launcher = static_cast<PanelLauncher *>(data);

	launcher->focused = 1;
	widget_schedule_redraw(widget);

	return CURSOR_LEFT_PTR;
}

/*void
PanelLauncher::panel_launcher_leave_handler(struct widget *widget,
			     struct input *input, void *data)*/
void
PanelLauncher::panel_launcher_leave_handler(struct widget *widget,
			     struct input *input, void *data)
{
	PanelLauncher *launcher = static_cast<PanelLauncher *>(data);

	launcher->focused = 0;
	widget_destroy_tooltip(widget);
	widget_schedule_redraw(widget);
}

/*void
PanelLauncher::panel_launcher_button_handler(struct widget *widget,
			      struct input *input, uint32_t time,
			      uint32_t button,
			      enum wl_pointer_button_state state, void *data)*/
void
PanelLauncher::panel_launcher_button_handler(struct widget *widget,
			      struct input *input, uint32_t time,
			      uint32_t button,
			      enum wl_pointer_button_state state, void *data)
{
	PanelLauncher *launcher;

	launcher = static_cast<PanelLauncher *>(widget_get_user_data(widget));
	widget_schedule_redraw(widget);
	if (state == WL_POINTER_BUTTON_STATE_RELEASED)
		launcher->panel_launcher_activate();

}

/*void
PanelLauncher::panel_launcher_touch_down_handler(struct widget *widget, struct input *input,
				  uint32_t serial, uint32_t time, int32_t id,
				  				  float x, float y, void *data)*/
void
PanelLauncher::panel_launcher_touch_down_handler(struct widget *widget, struct input *input,
			uint32_t serial, uint32_t time, int32_t id,
			float x, float y, void *data)
{
	PanelLauncher *launcher;

	launcher = static_cast<PanelLauncher *>(widget_get_user_data(widget));
	launcher->focused = 1;
	widget_schedule_redraw(widget);
}

void
PanelLauncher::panel_launcher_touch_up_handler(struct widget *widget, struct input *input,
				uint32_t serial, uint32_t time, int32_t id,
				void *data)
{
	PanelLauncher *launcher;

	launcher = static_cast<PanelLauncher *>(widget_get_user_data(widget));
	launcher->focused = 0;
	widget_schedule_redraw(widget);
	launcher->panel_launcher_activate();
}

void
PanelLauncher::panel_launcher_tablet_tool_proximity_in_handler(struct widget *widget,
						struct tablet_tool *tool,
						struct tablet *tablet, void *data)
{
	PanelLauncher *launcher;

	launcher = static_cast<PanelLauncher *>(widget_get_user_data(widget));
	launcher->focused = 1;
	widget_schedule_redraw(widget);
}

void
PanelLauncher::panel_launcher_tablet_tool_proximity_out_handler(struct widget *widget,
						 struct tablet_tool *tool, void *data)
{
	PanelLauncher *launcher;

	launcher = static_cast<PanelLauncher *>(widget_get_user_data(widget));
	launcher->focused = 0;
	widget_schedule_redraw(widget);
}

/*void
PanelLauncher::panel_launcher_tablet_tool_up_handler(struct widget *widget,
				      struct tablet_tool *tool,
				      void *data)*/
void
PanelLauncher::panel_launcher_tablet_tool_up_handler(struct widget *widget,
				      struct tablet_tool *tool,
				      void *data)
{
	PanelLauncher *launcher;

	launcher = static_cast<PanelLauncher *>(widget_get_user_data(widget));
	launcher->panel_launcher_activate();
}

/*
void
PanelLauncher::panel_launcher_tablet_tool_button_handler(struct widget *widget,
					  struct tablet_tool *tool,
					  uint32_t button,
					  uint32_t state_w,
					  void *data)*/
void
PanelLauncher::panel_launcher_tablet_tool_button_handler(struct widget *widget,
					  struct tablet_tool *tool,
					  uint32_t button,
					  uint32_t state_w,
					  void *data)
{
	PanelLauncher *launcher;
	enum zwp_tablet_tool_v2_button_state state = static_cast<enum zwp_tablet_tool_v2_button_state>(state_w);

	launcher = static_cast<PanelLauncher *>(widget_get_user_data(widget));

	if (state == ZWP_TABLET_TOOL_V2_BUTTON_STATE_RELEASED)
		launcher->panel_launcher_activate();
}