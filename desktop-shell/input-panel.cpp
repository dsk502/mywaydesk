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

extern "C" {
#include "config.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "input-method-unstable-v1-server-protocol.h"
#include "shared/helpers.h"
}

#include "shell.hpp"

class InputPanelSurface {
public:
	struct wl_resource *resource;
	struct wl_signal destroy_signal;

	DesktopShell *shell;

	struct wl_list link;
	struct weston_surface *surface;
	struct weston_view *view;
	struct wl_listener surface_destroy_listener;

	struct weston_view_animation *anim;

	struct weston_output *output;
	uint32_t panel;

	InputPanelSurface(DesktopShell *shell, struct weston_surface *surface);
	~InputPanelSurface();

	int calc_input_panel_position(struct weston_coord_global *out_pos);
	void show_input_panel_surface();
	
};

/*
static InputPanelSurface *
create_input_panel_surface(DesktopShell *shell,
			   struct weston_surface *surface)*/
InputPanelSurface::InputPanelSurface(DesktopShell *shell, struct weston_surface *surface)
	:resource(nullptr), destroy_signal({0}), shell(nullptr), link({0}),
	surface(nullptr), view(nullptr), surface_destroy_listener({0}),
	anim(nullptr), output(nullptr), panel(0)
{
	//InputPanelSurface *input_panel_surface = new InputPanelSurface();

	//input_panel_surface = calloc(1, sizeof *input_panel_surface);
	//if (!input_panel_surface)
		//return NULL;

	surface->committed = input_panel_committed;
	surface->committed_private = this;
	weston_surface_set_label_func(surface, input_panel_get_label);

	this->shell = shell;

	this->surface = surface;
	this->view = weston_view_create(surface);

	wl_signal_init(&this->destroy_signal);
	this->surface_destroy_listener.notify = input_panel_handle_surface_destroy;
	wl_signal_add(&surface->destroy_signal,
		      &this->surface_destroy_listener);

	wl_list_init(&this->link);
}

/*
static void
destroy_input_panel_surface(InputPanelSurface *input_panel_surface)*/
InputPanelSurface::~InputPanelSurface()
{
	wl_signal_emit(&this->destroy_signal, this);

	wl_list_remove(&this->surface_destroy_listener.link);
	wl_list_remove(&this->link);

	this->surface->committed = nullptr;
	weston_surface_set_label_func(this->surface, nullptr);
	weston_view_destroy(this->view);
}

static void
input_panel_slide_done(struct weston_view_animation *animation, void *data)
{
	InputPanelSurface *ipsurf = static_cast<InputPanelSurface *>(data);

	ipsurf->anim = NULL;
}

int
InputPanelSurface::calc_input_panel_position(struct weston_coord_global *out_pos)
{
	DesktopShell *shell = this->shell;
	struct weston_coord_global pos;

	if (this->panel) {
		struct weston_view *view = get_default_view(shell->text_input.surface);
		if (view == nullptr)
			return -1;
		pos = weston_view_get_pos_offset_global(view);
		pos.c.x += shell->text_input.cursor_rectangle.x2;
		pos.c.y += shell->text_input.cursor_rectangle.y2;
	} else {
		pos = this->output->pos;
		pos.c.x += (this->output->width - this->surface->width) / 2;
		pos.c.y += this->output->height - this->surface->height;
	}
	*out_pos = pos;
	return 0;
}

void
InputPanelSurface::show_input_panel_surface()
{
	DesktopShell *shell = this->shell;
	struct weston_seat *seat;
	struct weston_surface *focus;
	struct weston_coord_global pos;

	if (!weston_surface_is_mapped(this->surface))
		return;

	if (weston_view_is_mapped(this->view))
		return;

	wl_list_for_each(seat, &shell->compositor->seat_list, link) {
		struct weston_keyboard *keyboard =
			weston_seat_get_keyboard(seat);

		if (!keyboard || !keyboard->focus)
			continue;
		focus = weston_surface_get_main_surface(keyboard->focus);
		if (!focus)
			continue;
		this->output = focus->output;
		if (calc_input_panel_position(&pos))
			continue;

		weston_view_set_position(this->view, pos);
		weston_view_move_to_layer(this->view,
					  &shell->input_panel_layer.view_list);
		break;
	}

	if (this->anim)
		weston_view_animation_destroy(this->anim);

	this->anim =
		weston_slide_run(this->view,
				 this->surface->height * 0.9, 0,
				 input_panel_slide_done, this);
}

static void
show_input_panels(struct wl_listener *listener, void *data)
{
	DesktopShell *shell =
		container_of(listener, DesktopShell,
			     show_input_panel_listener);
	InputPanelSurface *ipsurf, *next;

	shell->text_input.surface = (struct weston_surface*)data;

	if (shell->showing_input_panels)
		return;

	shell->showing_input_panels = true;

	if (!shell->locked)
		weston_layer_set_position(&shell->input_panel_layer,
					  WESTON_LAYER_POSITION_TOP_UI);

	wl_list_for_each_safe(ipsurf, next,
			      &shell->input_panel.surfaces, link) {
		ipsurf->show_input_panel_surface();
	}
}

void
hide_input_panels(struct wl_listener *listener, void *data)
{
	DesktopShell *shell =
		container_of(listener, DesktopShell,
			     hide_input_panel_listener);
	struct weston_view *view, *next;

	if (!shell->showing_input_panels)
		return;

	shell->showing_input_panels = false;

	if (!shell->locked)
		weston_layer_unset_position(&shell->input_panel_layer);

	wl_list_for_each_safe(view, next,
			      &shell->input_panel_layer.view_list.link,
			      layer_link.link)
		weston_view_move_to_layer(view, NULL);
}

void
update_input_panels(struct wl_listener *listener, void *data)
{
	DesktopShell *shell =
		container_of(listener, DesktopShell,
			     update_input_panel_listener);

	memcpy(&shell->text_input.cursor_rectangle, data, sizeof(pixman_box32_t));
}

static int
input_panel_get_label(struct weston_surface *surface, char *buf, size_t len)
{
	return snprintf(buf, len, "input panel");
}

static void
input_panel_committed(struct weston_surface *surface,
		      struct weston_coord_surface new_origin)
{
	InputPanelSurface *ip_surface = static_cast<InputPanelSurface *>(surface->committed_private);
	DesktopShell *shell = ip_surface->shell;

	if (!weston_surface_has_content(surface))
		return;

	if (weston_surface_is_mapped(surface))
		return;

	weston_surface_map(surface);

	if (shell->showing_input_panels)
		ip_surface->show_input_panel_surface();
}



static InputPanelSurface *
get_input_panel_surface(struct weston_surface *surface)
{
	if (surface->committed == input_panel_committed) {
		return static_cast<InputPanelSurface *>(surface->committed_private);
	} else {
		return nullptr;
	}
}

static void
input_panel_handle_surface_destroy(struct wl_listener *listener, void *data)
{
	InputPanelSurface *ipsurface = container_of(listener,
							     InputPanelSurface,
							     surface_destroy_listener);

	if (ipsurface->resource) {
		wl_resource_destroy(ipsurface->resource);
	} else {
		delete ipsurface;
	}
}

static void
input_panel_surface_set_toplevel(struct wl_client *client,
				 struct wl_resource *resource,
				 struct wl_resource *output_resource,
				 uint32_t position)
{
	InputPanelSurface *input_panel_surface =
		static_cast<InputPanelSurface *>(wl_resource_get_user_data(resource));
	DesktopShell *shell = input_panel_surface->shell;
	struct weston_head *head = weston_head_from_resource(output_resource);

	if (head) {
		wl_list_insert(&shell->input_panel.surfaces,
			&input_panel_surface->link);

		input_panel_surface->output = head->output;
		input_panel_surface->panel = 0;
	}
}

static void
input_panel_surface_set_overlay_panel(struct wl_client *client,
				      struct wl_resource *resource)
{
	InputPanelSurface *input_panel_surface =
		static_cast<InputPanelSurface *>(wl_resource_get_user_data(resource));
	DesktopShell *shell = input_panel_surface->shell;

	wl_list_insert(&shell->input_panel.surfaces,
		       &input_panel_surface->link);

	input_panel_surface->panel = 1;
}

static const struct zwp_input_panel_surface_v1_interface input_panel_surface_implementation = {
	input_panel_surface_set_toplevel,
	input_panel_surface_set_overlay_panel
};

static void
destroy_input_panel_surface_resource(struct wl_resource *resource)
{
	InputPanelSurface *ipsurf =
		static_cast<InputPanelSurface *>(wl_resource_get_user_data(resource));

	delete ipsurf;
}

static void
input_panel_get_input_panel_surface(struct wl_client *client,
				    struct wl_resource *resource,
				    uint32_t id,
				    struct wl_resource *surface_resource)
{
	struct weston_surface *surface =
		static_cast<struct weston_surface *>(wl_resource_get_user_data(surface_resource));
	DesktopShell *shell = static_cast<DesktopShell *>(wl_resource_get_user_data(resource));
	InputPanelSurface *ipsurf;

	if (get_input_panel_surface(surface)) {
		wl_resource_post_error(surface_resource,
				       WL_DISPLAY_ERROR_INVALID_OBJECT,
				       "wl_input_panel::get_input_panel_surface already requested");
		return;
	}

	ipsurf = new InputPanelSurface(shell, surface);
	if (!ipsurf) {
		wl_resource_post_error(surface_resource,
				       WL_DISPLAY_ERROR_INVALID_OBJECT,
				       "surface->committed already set");
		return;
	}

	ipsurf->resource =
		wl_resource_create(client,
				   &zwp_input_panel_surface_v1_interface,
				   1,
				   id);
	wl_resource_set_implementation(ipsurf->resource,
				       &input_panel_surface_implementation,
				       ipsurf,
				       destroy_input_panel_surface_resource);
}

static const struct zwp_input_panel_v1_interface input_panel_implementation = {
	input_panel_get_input_panel_surface
};

static void
unbind_input_panel(struct wl_resource *resource)
{
	DesktopShell *shell = static_cast<DesktopShell *>(wl_resource_get_user_data(resource));

	shell->input_panel.binding = NULL;
}

static void
bind_input_panel(struct wl_client *client,
	      void *data, uint32_t version, uint32_t id)
{
	DesktopShell *shell = static_cast<DesktopShell *>(data);
	struct wl_resource *resource;

	resource = wl_resource_create(client,
				      &zwp_input_panel_v1_interface, 1, id);

	if (shell->input_panel.binding == NULL) {
		wl_resource_set_implementation(resource,
					       &input_panel_implementation,
					       shell, unbind_input_panel);
		shell->input_panel.binding = resource;
		return;
	}

	wl_resource_post_error(resource, WL_DISPLAY_ERROR_INVALID_OBJECT,
			       "interface object already bound");
}


void
input_panel_destroy(DesktopShell *shell)
{
	wl_list_remove(&shell->show_input_panel_listener.link);
	wl_list_remove(&shell->hide_input_panel_listener.link);
}

int
input_panel_setup(DesktopShell *shell)
{
	struct weston_compositor *ec = shell->compositor;

	shell->show_input_panel_listener.notify = show_input_panels;
	wl_signal_add(&ec->show_input_panel_signal,
		      &shell->show_input_panel_listener);
	shell->hide_input_panel_listener.notify = hide_input_panels;
	wl_signal_add(&ec->hide_input_panel_signal,
		      &shell->hide_input_panel_listener);
	shell->update_input_panel_listener.notify = update_input_panels;
	wl_signal_add(&ec->update_input_panel_signal,
		      &shell->update_input_panel_listener);

	wl_list_init(&shell->input_panel.surfaces);

	if (wl_global_create(shell->compositor->wl_display,
			     &zwp_input_panel_v1_interface, 1,
			     shell, bind_input_panel) == NULL)
		return -1;

	return 0;
}
