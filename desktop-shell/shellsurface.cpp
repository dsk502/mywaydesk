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

ShellSurface::ShellSurface()
	:destroy_signal({0}), desktop_surface(nullptr), view(nullptr), wsurface_anim_fade(nullptr), wview_anim_fade(nullptr),
	last_width(0), last_height(0), shell(nullptr), children_list({0}), children_link({0}),
	saved_pos({0}), saved_position_valid(0), saved_rotation_valid(0), unresponsive(0), grabbed(0),
	resize_edges(0), orientation(0), rotation({0}), fullscreen({0}),
	fullscreen_output(nullptr), output(nullptr), output_destroy_listener({0}),
	state({0}), xwayland({0}), focus_count(0), destroying(0), link({0})
{
	//Leave it empty
}

//Former desktop_shell_destroy_surface()
ShellSurface::~ShellSurface()
{
	ShellSurface *shsurf_child, *tmp;

	if (this->fullscreen.black_view)
		weston_shell_utils_curtain_destroy(this->fullscreen.black_view);

	wl_list_for_each_safe(shsurf_child, tmp, &this->children_list, children_link) {
		wl_list_remove(&shsurf_child->children_link);
		wl_list_init(&shsurf_child->children_link);
	}
	wl_list_remove(&this->children_link);
	weston_desktop_surface_unlink_view(this->view);
	wl_list_remove(&this->link);
	weston_view_destroy(this->view);

	wl_signal_emit(&this->destroy_signal, this);
	weston_surface_unref(this->wsurface_anim_fade);

	if (this->output_destroy_listener.notify) {
		wl_list_remove(&this->output_destroy_listener.link);
		this->output_destroy_listener.notify = NULL;
	}
}

DesktopShell *
ShellSurface::shell_surface_get_shell()
{
	return this->shell;
}

void
ShellSurface::set_busy_cursor(struct weston_pointer *pointer)
{
	struct shell_grab *grab;

	if (pointer->grab->interface == &busy_cursor_grab_interface)
		return;

	grab = malloc(sizeof *grab);
	if (!grab)
		return;

	shell_grab_start(grab, &busy_cursor_grab_interface, this, pointer,
			 WESTON_DESKTOP_SHELL_CURSOR_BUSY);
	/* Mark the shsurf as ungrabbed so that button binding is able
	 * to move it. */
	this->grabbed = 0;
}

void
ShellSurface::surface_rotate(struct weston_pointer *pointer)
{
	struct weston_surface *surface =
		weston_desktop_surface_get_surface(this->desktop_surface);
	struct rotate_grab *rotate;
	struct weston_coord_surface center;
	struct weston_coord_global center_g;
	float dx, dy;
	float r;

	rotate = malloc(sizeof *rotate);
	if (!rotate)
		return;

	center = weston_coord_surface(surface->width * 0.5f,
				      surface->height * 0.5f,
				      this->view->surface);
	center_g = weston_coord_surface_to_global(this->view, center);

	rotate->center.x = center_g.c.x;
	rotate->center.y = center_g.c.y;

	dx = pointer->pos.c.x - rotate->center.x;
	dy = pointer->pos.c.y - rotate->center.y;
	r = sqrtf(dx * dx + dy * dy);
	if (r > 20.0f) {
		struct weston_matrix inverse;

		weston_matrix_init(&inverse);
		weston_matrix_rotate_xy(&inverse, dx / r, -dy / r);
		weston_matrix_multiply(&this->rotation.rotation, &inverse);

		weston_matrix_init(&rotate->rotation);
		weston_matrix_rotate_xy(&rotate->rotation, dx / r, dy / r);
	} else {
		weston_matrix_init(&this->rotation.rotation);
		weston_matrix_init(&rotate->rotation);
	}

	shell_grab_start(&rotate->base, &rotate_grab_interface, this,
			 pointer, WESTON_DESKTOP_SHELL_CURSOR_ARROW);
}