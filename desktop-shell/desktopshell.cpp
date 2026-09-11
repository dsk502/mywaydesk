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

#include "shell.hpp"

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
	client(nullptr), startup_time({0})
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

void
DesktopShell::shell_fade_init()
{
	/* Make compositor output all black, and wait for the desktop-shell
	 * client to signal it is ready, then fade in. The timer triggers a
	 * fade-in, in case the desktop-shell client takes too long.
	 */

	struct wl_event_loop *loop;

	if (this->startup_animation_type == ANIMATION_NONE)
		return;

	if (this->fade.curtain != NULL) {
		weston_log("%s: warning: fade surface already exists\n",
			   __func__);
		return;
	}

	this->fade.curtain = shell_fade_create_view(this);
	if (!this->fade.curtain)
		return;

	weston_view_update_transform(this->fade.curtain->view);
	weston_surface_damage(this->fade.curtain->view->surface);

	loop = wl_display_get_event_loop(this->compositor->wl_display);
	this->fade.startup_timer =
		wl_event_loop_add_timer(loop, fade_startup_timeout, this);
	wl_event_source_timer_update(this->fade.startup_timer, 15000);
}

void
DesktopShell::shell_fade_startup()
{
	struct wl_event_loop *loop;
	bool has_fade = false;

	if (!this->fade.startup_timer)
		return;

	wl_event_source_remove(this->fade.startup_timer);
	this->fade.startup_timer = NULL;
	has_fade = true;

	if (has_fade) {
		loop = wl_display_get_event_loop(this->compositor->wl_display);
		wl_event_loop_add_idle(loop, do_shell_fade_startup, this);
	}
}

void
DesktopShell::lock()
{
	Workspace *ws = get_current_workspace();

	if (this->locked) {
		weston_compositor_sleep(this->compositor);
		return;
	}

	this->locked = true;

	/* Hide all surfaces by removing the fullscreen, panel and
	 * toplevel layers.  This way nothing else can show or receive
	 * input events while we are locked. */

	weston_layer_unset_position(&this->panel_layer);
	weston_layer_unset_position(&this->fullscreen_layer);
	if (this->showing_input_panels)
		weston_layer_unset_position(&this->input_panel_layer);
	weston_layer_unset_position(&ws->layer);

	weston_layer_set_position(&this->lock_layer,
				  WESTON_LAYER_POSITION_LOCK);

	weston_compositor_sleep(this->compositor);

	/* Remove the keyboard focus on all seats. This will be
	 * restored to the Workspace's saved state via
	 * restore_focus_state when the compositor is unlocked */
	unfocus_all_seats(this);

	/* TODO: disable bindings that should not work while locked. */

	/* All this must be undone in resume_desktop(). */
}

void
DesktopShell::unlock()
{
	struct wl_resource *shell_resource;

	if (!this->locked || this->lock_surface) {
		shell_fade(FADE_IN);
		return;
	}

	/* If desktop-shell client has gone away, unlock immediately. */
	if (!this->child.desktop_shell) {
		resume_desktop();
		return;
	}

	if (this->prepare_event_sent)
		return;

	shell_resource = this->child.desktop_shell;
	weston_desktop_shell_send_prepare_lock_surface(shell_resource);
	this->prepare_event_sent = true;
}

void
DesktopShell::resume_desktop()
{
	Workspace *ws = get_current_workspace();

	weston_layer_unset_position(&this->lock_layer);

	if (this->showing_input_panels)
		weston_layer_set_position(&this->input_panel_layer,
					  WESTON_LAYER_POSITION_TOP_UI);
	weston_layer_set_position(&this->fullscreen_layer,
				  WESTON_LAYER_POSITION_FULLSCREEN);
	weston_layer_set_position(&this->panel_layer,
				  WESTON_LAYER_POSITION_UI);
	weston_layer_set_position(&ws->layer, WESTON_LAYER_POSITION_NORMAL);

	restore_focus_state(this, get_current_workspace());

	this->locked = false;
	shell_fade(FADE_IN);
	weston_compositor_damage_all(this->compositor);
}

Workspace *
DesktopShell::get_current_workspace()
{
	return &this->workspace;
}

/*
 * Tool methods
 */

static void
do_shell_fade_startup(void *data)
{
	DesktopShell *shell = static_cast<DesktopShell *>(data);

	assert(shell->startup_animation_type == ANIMATION_FADE ||
	       shell->startup_animation_type == ANIMATION_NONE);

	if (shell->startup_animation_type == ANIMATION_FADE)
		shell->shell_fade(FADE_IN);
}

static int
fade_startup_timeout(void *data)
{
	DesktopShell *shell = static_cast<DesktopShell *>(data);

	shell->shell_fade_startup();
	return 0;
}

static void
shell_fade_done(struct weston_view_animation *animation, void *data)
{
	DesktopShell *shell = static_cast<DesktopShell *>(data);

	shell->fade.animation = NULL;
	switch (shell->fade.type) {
	case FADE_IN:
		weston_shell_utils_curtain_destroy(shell->fade.curtain);
		shell->fade.curtain = NULL;
		break;
	case FADE_OUT:
		shell->lock();
		break;
	default:
		break;
	}
}

static int
fade_surface_get_label(struct weston_surface *surface,
		       char *buf, size_t len)
{
	return snprintf(buf, len, "desktop shell fade surface");
}

static struct weston_curtain *
shell_fade_create_view(DesktopShell *shell)
{
	struct weston_compositor *compositor = shell->compositor;
	ShellOutput *shell_output;
	struct weston_curtain_params curtain_params = {
		.get_label = fade_surface_get_label,
		.surface_committed = black_surface_committed,
		.surface_private = shell,
		.r = 0.0, .g = 0.0, .b = 0.0, .a = 1.0,
		.capture_input = true,
	};
	struct weston_curtain *curtain;
	bool first = true;
	int x1 = 0, y1 = 0, x2 = 0, y2 = 0;

	wl_list_for_each(shell_output, &shell->output_list, link) {
		struct weston_output *op = shell_output->output;

		if (first) {
			first = false;
			x1 = op->pos.c.x;
			y1 = op->pos.c.y;
			x2 = op->pos.c.x + op->width;
			y2 = op->pos.c.y + op->height;
			continue;
		}

		x1 = MIN(x1, op->pos.c.x);
		y1 = MIN(y1, op->pos.c.y);
		x2 = MAX(x2, op->pos.c.x + op->width);
		y2 = MAX(y2, op->pos.c.y + op->height);
	}
	curtain_params.pos.c.x = x1;
	curtain_params.pos.c.y = y1;
	curtain_params.width = x2 - x1;
	curtain_params.height = y2 - y1;
	curtain = weston_shell_utils_curtain_create(compositor, &curtain_params);
	assert(curtain);

	weston_view_move_to_layer(curtain->view, &compositor->fade_layer.view_list);

	return curtain;
}

static void
fade_out_done_idle_cb(void *data)
{
	ShellSurface *shsurf = static_cast<ShellSurface *>(data);
	delete (shsurf);
}

void
fade_out_done(struct weston_view_animation *animation, void *data)
{
	ShellSurface *shsurf = static_cast<ShellSurface *>(data);
	struct wl_event_loop *loop;

	loop = wl_display_get_event_loop(shsurf->shell->compositor->wl_display);

	if (weston_view_is_mapped(shsurf->wview_anim_fade)) {
		weston_view_move_to_layer(shsurf->wview_anim_fade, NULL);
		wl_event_loop_add_idle(loop, fade_out_done_idle_cb, shsurf);
	}
}

static enum animation_type
get_animation_type(char *animation)
{
	if (!animation)
		return ANIMATION_NONE;

	if (!strcmp("zoom", animation))
		return ANIMATION_ZOOM;
	else if (!strcmp("fade", animation))
		return ANIMATION_FADE;
	else if (!strcmp("dim-layer", animation))
		return ANIMATION_DIM_LAYER;
	else
		return ANIMATION_NONE;
}