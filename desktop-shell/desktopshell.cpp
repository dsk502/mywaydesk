/*
 * Copyright © 2010-2012 Intel Corporation
 * Copyright © 2011-2012 Collabora, Ltd.
 * Copyright © 2013 Raspberry Pi Foundation
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

DesktopShell::DesktopShell()
	:compositor(nullptr), desktop(nullptr), xwayland_surface_api(nullptr),
	idle_listener({0}), wake_listener({0}), transform_listener({0}), resized_listener({0}), destroy_listener({0}), show_input_panel_listener({0}), hide_input_panel_listener({0}), update_input_panel_listener({0}), session_listener({0}),
	fullscreen_layer({0}), panel_layer({0}), background_layer({0}), lock_layer({0}), input_panel_layer({0}),
	pointer_focus_listener({0}), grab_surface(nullptr), child({0}),
	locked(0), showing_input_panels(0), prepare_event_sent(0),
	text_backend(nullptr), text_input({0}), lock_surface(nullptr),
	lock_surface_listener({0}), lock_view(nullptr), workspace({0}),
	input_panel({0}), fade({0}), allow_zap(0), binding_modifier(0),
	win_animation_type(static_cast<animation_type>(0)), win_close_animation_type(static_cast<animation_type>(0)), startup_animation_type(static_cast<animation_type>(0)), focus_animation_type(static_cast<animation_type>(0)),
	minimized_layer({0}),
	seat_create_listener({0}), output_create_listener({0}), output_move_listener({0}),
	output_list({0}), seat_list({0}), shsurf_list({0}),
	panel_position(static_cast<weston_desktop_shell_panel_position>(0)),dock_position(static_cast<weston_desktop_shell_dock_position>(0)),
	client(nullptr), startup_time(0)
{
	//Leave empty here
}

DesktopShell::~DesktopShell()
{

}



bool
DesktopShell::shell_configuration()
{
	struct weston_config_section *section;
	struct weston_config *config;
	char *s, *client;
	bool allow_zap;

	config = wet_get_config(this->compositor);
	section = weston_config_get_section(config, "shell", NULL, NULL);
	client = wet_get_libexec_path(WESTON_SHELL_CLIENT);
	weston_config_section_get_string(section, "client", &s, client);
	free(client);
	this->client = s;

	weston_config_section_get_bool(section,
				       "allow-zap", &allow_zap, true);
	this->allow_zap = allow_zap;

	this->binding_modifier = weston_config_get_binding_modifier(config, MODIFIER_SUPER);

	weston_config_section_get_string(section, "animation", &s, "none");
	this->win_animation_type = get_animation_type(s);
	free(s);
	weston_config_section_get_string(section, "close-animation", &s, "fade");
	this->win_close_animation_type = get_animation_type(s);
	free(s);

	weston_config_section_get_string(section,
					 "startup-animation", &s, "fade");
	this->startup_animation_type = get_animation_type(s);
	if (this->startup_animation_type == ANIMATION_ZOOM) {
		weston_log("invalid startup animation type %s\n", s);
		free(s);
		return false;
	}
	free(s);

	weston_config_section_get_string(section, "focus-animation", &s, "none");
	this->focus_animation_type = get_animation_type(s);
	if (this->focus_animation_type != ANIMATION_NONE &&
	    this->focus_animation_type != ANIMATION_DIM_LAYER) {
		weston_log("invalid focus animation type %s\n", s);
		free(s);
		return false;
	}
	free(s);

	return true;
}

void
DesktopShell::shell_fade(enum fade_type type)
{
	float tint;

	switch (type) {
	case FADE_IN:
		tint = 0.0;
		break;
	case FADE_OUT:
		tint = 1.0;
		break;
	default:
		weston_log("shell: invalid fade type\n");
		return;
	}

	this->fade.type = type;

	if (this->fade.curtain == NULL) {
		this->fade.curtain = shell_fade_create_view(this);
		if (!this->fade.curtain)
			return;

		weston_view_set_alpha(this->fade.curtain->view, 1.0 - tint);
	}

	if (this->fade.animation) {
		weston_fade_update(this->fade.animation, tint);
	} else {
		this->fade.animation =
			weston_fade_run(this->fade.curtain->view,
					1.0 - tint, tint,
					shell_fade_done, this);
	}
}