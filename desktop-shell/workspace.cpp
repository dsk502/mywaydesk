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

//Former workspace_create()
Workspace::Workspace(DesktopShell *shell)
	:layer({0}), focus_list({0}), seat_destroyed_listener({0}),
	fsurf_front(nullptr), fsurf_back(nullptr), focus_animation(nullptr)
{
	//struct Workspace *ws = &shell->workspace;

	weston_layer_init(&this->layer, shell->compositor);
	weston_layer_set_position(&this->layer, WESTON_LAYER_POSITION_NORMAL);

	wl_list_init(&this->focus_list);
	wl_list_init(&this->seat_destroyed_listener.link);
	this->seat_destroyed_listener.notify = seat_destroyed;

	if (shell->focus_animation_type != ANIMATION_NONE) {
		struct weston_output *output =
			weston_shell_utils_get_default_output(shell->compositor);

		assert(shell->focus_animation_type == ANIMATION_DIM_LAYER);

		this->fsurf_front = create_focus_surface(shell->compositor, output);
		assert(this->fsurf_front);
		this->fsurf_back = create_focus_surface(shell->compositor, output);
		assert(this->fsurf_back);
	} else {
		this->fsurf_front = nullptr;
		this->fsurf_back = nullptr;
	}
	this->focus_animation = nullptr;
}

Workspace::~Workspace()
{
	struct focus_state *state, *next;

	wl_list_for_each_safe(state, next, &this->focus_list, link)
		focus_state_destroy(state);

	if (this->fsurf_front)
		focus_surface_destroy(this->fsurf_front);
	if (this->fsurf_back)
		focus_surface_destroy(this->fsurf_back);

	desktop_shell_destroy_layer(&this->layer);
}