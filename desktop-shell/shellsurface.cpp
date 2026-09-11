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

	grab = static_cast<struct shell_grab *>(malloc(sizeof *grab));
	if (!grab)
		return;

	shell_grab_start(grab, &busy_cursor_grab_interface, this, pointer,
			 WESTON_DESKTOP_SHELL_CURSOR_BUSY);
	/* Mark the shsurf as ungrabbed so that button binding is able
	 * to move it. */
	this->grabbed = 0;
}

int
ShellSurface::surface_move(struct weston_pointer *pointer,
	     bool client_initiated)
{
	struct weston_move_grab *move;

	if (!this)
		return -1;

	if (this->grabbed || this->shsurf_is_max_or_fullscreen())
		return 0;

	move = static_cast<weston_move_grab *>(malloc(sizeof *move));
	if (!move)
		return -1;

	move->delta = weston_coord_global_sub(
		weston_view_get_pos_offset_global(this->view),
		pointer->grab_pos);
	move->client_initiated = client_initiated;

	weston_desktop_surface_set_orientation(this->desktop_surface,
					       WESTON_TOP_LEVEL_TILED_ORIENTATION_NONE);
	this->orientation = WESTON_TOP_LEVEL_TILED_ORIENTATION_NONE;
	shell_grab_start(&move->base, &move_grab_interface, this,
			 pointer, WESTON_DESKTOP_SHELL_CURSOR_MOVE);

	return 0;
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

	rotate = static_cast<struct rotate_grab *>(malloc(sizeof *rotate));
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

void
ShellSurface::shell_surface_activate()
{
	if (this->focus_count++ == 0)
		sync_surface_activated_state(this);
}

void
ShellSurface::shell_surface_deactivate()
{
	if (--this->focus_count == 0)
		sync_surface_activated_state(this);
}

bool
ShellSurface::shsurf_is_max_or_fullscreen()
{
	struct weston_desktop_surface *dsurface = this->desktop_surface;
	return weston_desktop_surface_get_maximized(dsurface) ||
		weston_desktop_surface_get_fullscreen(dsurface);
}

void
ShellSurface::get_maximized_size(int32_t *width, int32_t *height)
{
	DesktopShell *shell;
	pixman_rectangle32_t area;

	shell = shell_surface_get_shell();
	get_output_work_area(shell, this->output, &area);

	*width = area.width;
	*height = area.height;
}

/*
 * helper to take into account panels and send the appropriate dimensions
 */
void
ShellSurface::set_shsurf_size_maximized_or_fullscreen(
					bool max_requested,
					bool fullscreen_requested)
{
	int width = 0; int height = 0;

	if (fullscreen_requested) {
		if (this->output) {
			width = this->output->width;
			height = this->output->height;
		}
	} else if (max_requested) {
		/* take the panels into considerations */
		get_maximized_size(&width, &height);
	}

	/* (0, 0) means we're back from one of the maximized/fullcreen states */
	weston_desktop_surface_set_size(this->desktop_surface, width, height);
}

void
ShellSurface::set_fullscreen(bool fullscreen,
	       struct weston_output *output)
{
	struct weston_desktop_surface *desktop_surface = this->desktop_surface;
	struct weston_surface *surface =
		weston_desktop_surface_get_surface(this->desktop_surface);

	weston_desktop_surface_set_fullscreen(desktop_surface, fullscreen);
	if (fullscreen) {
		/* handle clients launching in fullscreen */
		if (output == NULL && !weston_surface_is_mapped(surface)) {
			/* Set the output to the one that has focus currently. */
			output = weston_shell_utils_get_focused_output(surface->compositor);
		}

		shell_surface_set_output(output);
		this->fullscreen_output = this->output;

		weston_desktop_surface_set_orientation(this->desktop_surface,
							WESTON_TOP_LEVEL_TILED_ORIENTATION_NONE);

		set_shsurf_size_maximized_or_fullscreen(false, fullscreen);
	} else {
		int width;
		int height;

		width = 0;
		height = 0;
		/* this is a corner case where we set up the surface as
		 * maximized, then fullscreen, and back to maximized.
		 *
		 * we land here here when we're back from fullscreen and we
		 * were previously maximized: rather than sending (0, 0) send
		 * the area of the output minus the panels */
		struct weston_desktop_surface *dsurface =
			this->desktop_surface;

		if (weston_desktop_surface_get_maximized(dsurface) ||
		    weston_desktop_surface_get_pending_maximized(dsurface)) {
			get_maximized_size(&width, &height);
		}
		weston_desktop_surface_set_size(this->desktop_surface, width, height);
	}

}

void
ShellSurface::unset_fullscreen()
{
	if (this->fullscreen.black_view)
		weston_shell_utils_curtain_destroy(this->fullscreen.black_view);
	this->fullscreen.black_view = NULL;

	if (this->saved_position_valid)
		weston_view_set_position(this->view, this->saved_pos);
	else
		weston_view_set_initial_position(this->view, this->shell);
	this->saved_position_valid = false;

	weston_desktop_surface_set_orientation(this->desktop_surface,
					       static_cast<weston_top_level_tiled_orientation>(this->orientation));

	if (this->saved_rotation_valid) {
		weston_view_add_transform(this->view,
					  &this->view->geometry.transformation_list,
					  &this->rotation.transform);
		this->saved_rotation_valid = false;
	}
}

void
ShellSurface::set_maximized(bool maximized)
{
	struct weston_desktop_surface *desktop_surface = this->desktop_surface;
	struct weston_surface *surface =
		weston_desktop_surface_get_surface(this->desktop_surface);

	if (weston_desktop_surface_get_fullscreen(desktop_surface))
		return;

	if (maximized) {
		struct weston_output *output;

		if (!weston_surface_is_mapped(surface))
			output = weston_shell_utils_get_focused_output(surface->compositor);
		else
			output = surface->output;

		this->shell_surface_set_output(output);

		weston_desktop_surface_set_orientation(this->desktop_surface,
							WESTON_TOP_LEVEL_TILED_ORIENTATION_NONE);
	}
	weston_desktop_surface_set_maximized(desktop_surface, maximized);
	this->set_shsurf_size_maximized_or_fullscreen(maximized, false);
}

void
ShellSurface::unset_maximized()
{
	struct weston_surface *surface =
		weston_desktop_surface_get_surface(this->desktop_surface);

	/* undo all maximized things here */
	shell_surface_set_output(weston_shell_utils_get_default_output(surface->compositor));

	if (this->saved_position_valid)
		weston_view_set_position(this->view, this->saved_pos);
	else
		weston_view_set_initial_position(this->view, this->shell);
	this->saved_position_valid = false;

	weston_desktop_surface_set_orientation(this->desktop_surface,
					       static_cast<weston_top_level_tiled_orientation>(this->orientation));

	if (this->saved_rotation_valid) {
		weston_view_add_transform(this->view,
					  &this->view->geometry.transformation_list,
					  &this->rotation.transform);
		this->saved_rotation_valid = false;
	}
}

/* Set the shell surface as the current fullscreen view for its current output,
 * centering it with a black background */
void
ShellSurface::shell_set_view_fullscreen()
{
	struct weston_surface *surface =
		weston_desktop_surface_get_surface(this->desktop_surface);
	struct weston_compositor *ec = surface->compositor;
	struct weston_output *output = this->fullscreen_output;
	struct weston_curtain_params curtain_params = {
		.get_label = black_surface_get_label,
		.surface_committed = black_surface_committed,
		.surface_private = this->view,
		.r = 0.0, .g = 0.0, .b = 0.0, .a = 1.0,
		.pos = output->pos,
		.width = output->width, .height = output->height,
		.capture_input = true,
	};

	assert(weston_desktop_surface_get_fullscreen(this->desktop_surface));

	weston_view_move_to_layer(this->view,
				  &this->shell->fullscreen_layer.view_list);
	weston_shell_utils_center_on_output(this->view, this->fullscreen_output);

	if (!this->fullscreen.black_view) {
		this->fullscreen.black_view =
			weston_shell_utils_curtain_create(ec, &curtain_params);
	}
	weston_view_set_output(this->fullscreen.black_view->view,
			       this->fullscreen_output);
	weston_view_move_to_layer(this->fullscreen.black_view->view,
				  &this->view->layer_link);

	this->state.lowered = false;
}

/* The surface will be inserted into the list immediately after the link
 * returned by this function (i.e. will be stacked immediately above the
 * returned link). */
struct weston_layer_entry *
ShellSurface::shell_surface_calculate_layer_link ()
{
	Workspace *ws;

	if (weston_desktop_surface_get_fullscreen(this->desktop_surface) &&
	    !this->state.lowered) {
		return &this->shell->fullscreen_layer.view_list;
	}

	/* Move the surface to a normal Workspace layer so that surfaces
	 * which were previously fullscreen or transient are no longer
	 * rendered on top. */
	ws = shell->get_current_workspace();
	return &ws->layer.view_list;
}

void
ShellSurface::shell_surface_update_child_surface_layers()
{
	weston_desktop_surface_propagate_layer(this->desktop_surface);
}

/* Update the surface’s layer. Mark both the old and new views as having dirty
 * geometry to ensure the changes are redrawn.
 *
 * If any child surfaces exist and are mapped, ensure they’re in the same layer
 * as this surface. */
void
ShellSurface::shell_surface_update_layer()
{
	struct weston_layer_entry *new_layer_link;

	new_layer_link = shell_surface_calculate_layer_link();
	assert(new_layer_link);

	weston_view_move_to_layer(this->view, new_layer_link);
	shell_surface_update_child_surface_layers();
}

void
ShellSurface::shell_surface_set_output(struct weston_output *output)
{
	struct weston_surface *es =
		weston_desktop_surface_get_surface(this->desktop_surface);

	/* get the default output, if the client set it as NULL
	   check whether the output is available */
	if (output)
		this->output = output;
	else if (es->output)
		this->output = es->output;
	else
		this->output = weston_shell_utils_get_default_output(es->compositor);

	if (this->output_destroy_listener.notify) {
		wl_list_remove(&this->output_destroy_listener.link);
		this->output_destroy_listener.notify = NULL;
	}

	if (!this->output)
		return;

	this->output_destroy_listener.notify = notify_output_destroy;
	wl_signal_add(&this->output->destroy_signal,
		      &this->output_destroy_listener);
}

bool
ShellSurface::has_keyboard_focused_child()
{
	bool has_keyboard_focus = false;

	if (this->focus_count > 0)
		return true;

	weston_desktop_surface_foreach_child(this->desktop_surface,
					     has_keyboard_focused_child_callback,
					     &has_keyboard_focus);

	return has_keyboard_focus;
}

int
ShellSurface::surface_resize(struct weston_pointer *pointer, uint32_t edges)
{
	struct weston_resize_grab *resize;
	const unsigned resize_topbottom =
		WESTON_DESKTOP_SURFACE_EDGE_TOP | WESTON_DESKTOP_SURFACE_EDGE_BOTTOM;
	const unsigned resize_leftright =
		WESTON_DESKTOP_SURFACE_EDGE_LEFT | WESTON_DESKTOP_SURFACE_EDGE_RIGHT;
	const unsigned resize_any = resize_topbottom | resize_leftright;
	struct weston_geometry geometry;

	if (this->grabbed || shsurf_is_max_or_fullscreen())
		return 0;

	/* Check for invalid edge combinations. */
	if (edges == WESTON_DESKTOP_SURFACE_EDGE_NONE || edges > resize_any ||
	    (edges & resize_topbottom) == resize_topbottom ||
	    (edges & resize_leftright) == resize_leftright)
		return 0;

	resize = static_cast<struct weston_resize_grab *>(malloc(sizeof *resize));
	if (!resize)
		return -1;

	resize->edges = edges;

	geometry = weston_desktop_surface_get_geometry(this->desktop_surface);
	resize->width = geometry.width;
	resize->height = geometry.height;

	this->resize_edges = edges;
	weston_desktop_surface_set_resizing(this->desktop_surface, true);
	weston_desktop_surface_set_orientation(this->desktop_surface,
					       WESTON_TOP_LEVEL_TILED_ORIENTATION_NONE);
	this->orientation = WESTON_TOP_LEVEL_TILED_ORIENTATION_NONE;
	shell_grab_start(&resize->base, &resize_grab_interface, this,
			 pointer, static_cast<weston_desktop_shell_cursor>(edges));

	return 0;
}

/*
 * Tool methods
 */

static void
sync_surface_activated_state(ShellSurface *shsurf)
{
	struct weston_desktop_surface *surface = shsurf->desktop_surface;
	struct weston_desktop_surface *parent;
	struct weston_surface *parent_surface;

	parent = weston_desktop_surface_get_parent(surface);
	if (parent) {
		parent_surface = weston_desktop_surface_get_surface(parent);
		sync_surface_activated_state(get_shell_surface(parent_surface));
		return;
	}

	if (shsurf->has_keyboard_focused_child())
		weston_desktop_surface_set_activated(surface, true);
	else
		weston_desktop_surface_set_activated(surface, false);
}

static void
has_keyboard_focused_child_callback(struct weston_desktop_surface *surface,
				    void *user_data)
{
	struct weston_surface *es = weston_desktop_surface_get_surface(surface);
	ShellSurface *shsurf = get_shell_surface(es);
	bool *has_keyboard_focus = static_cast<bool *>(user_data);

	if (shsurf->focus_count > 0) {
		*has_keyboard_focus = true;
		return;
	}

	weston_desktop_surface_foreach_child(shsurf->desktop_surface,
					     has_keyboard_focused_child_callback,
					     &has_keyboard_focus);
}

static void
notify_output_destroy(struct wl_listener *listener, void *data)
{
	ShellSurface *shsurf =
		container_of(listener,
			     ShellSurface, output_destroy_listener);

	shsurf->output = NULL;
	shsurf->output_destroy_listener.notify = NULL;

	shsurf->fullscreen_output = NULL;
}

static bool
is_black_surface_view(struct weston_view *view, struct weston_view **fs_view)
{
	struct weston_surface *surface = view->surface;

	if (surface->committed == black_surface_committed) {
		if (fs_view)
			*fs_view = static_cast<weston_view *>(surface->committed_private);
		return true;
	}
	return false;
}