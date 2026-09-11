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

Switcher::Switcher()
    :shell(nullptr), current(nullptr), listener{}, grab{}, minimized_array{}
{
    //Leave empty
}

void
Switcher::switcher_next()
{
	struct weston_view *view;
	struct weston_view *first = nullptr, *prev = nullptr, *next = nullptr;
	ShellSurface *shsurf;
	Workspace *ws = this->shell->get_current_workspace();

	 /* temporary re-display minimized surfaces */
	struct weston_view *tmp;
	struct weston_view **minimized;
	wl_list_for_each_safe(view, tmp, &this->shell->minimized_layer.view_list.link, layer_link.link) {
		weston_view_move_to_layer(view, &ws->layer.view_list);
		minimized = static_cast<struct weston_view **>(wl_array_add(&this->minimized_array, sizeof *minimized));
		*minimized = view;
	}

	wl_list_for_each(view, &ws->layer.view_list.link, layer_link.link) {
		shsurf = get_shell_surface(view->surface);
		if (shsurf) {
			if (first == NULL)
				first = view;
			if (prev == this->current)
				next = view;
			prev = view;
			weston_view_set_alpha(view, 0.25);
		}

		if (is_black_surface_view(view, NULL))
			weston_view_set_alpha(view, 0.25);
	}

	if (next == NULL)
		next = first;

	if (next == NULL)
		return;

	wl_list_remove(&this->listener.link);
	wl_signal_add(&next->destroy_signal, &this->listener);

	this->current = next;
	wl_list_for_each(view, &next->surface->views, surface_link)
		weston_view_set_alpha(view, 1.0);

	shsurf = get_shell_surface(this->current->surface);
	if (shsurf && weston_desktop_surface_get_fullscreen(shsurf->desktop_surface))
		weston_view_set_alpha(shsurf->fullscreen.black_view->view, 1.0);
}

void
Switcher::switcher_handle_view_destroy(struct wl_listener *listener, void *data)
{
	Switcher *switcher =
		container_of(listener, Switcher, listener);

	switcher->switcher_next();
}

/*
static void
switcher_destroy(struct switcher *switcher)
*/
Switcher::~Switcher()
{
	struct weston_view *view;
	struct weston_keyboard *keyboard = this->grab.keyboard;
	Workspace *ws = this->shell->get_current_workspace();

	wl_list_for_each(view, &ws->layer.view_list.link, layer_link.link) {
		if (is_focus_view(view))
			continue;

		weston_view_set_alpha(view, 1.0);
	}

	if (this->current && get_shell_surface(this->current->surface)) {
		activate(this->shell, this->current,
			 keyboard->seat,
			 WESTON_ACTIVATE_FLAG_CONFIGURE);
	}

	wl_list_remove(&this->listener.link);
	weston_keyboard_end_grab(keyboard);
	if (keyboard->input_method_resource)
		keyboard->grab = &keyboard->input_method_grab;

	 /* re-hide surfaces that were temporary shown during the switch */
	struct weston_view **minimized;

    for (minimized = static_cast<struct weston_view **>((&this->minimized_array)->data); 
        (&this->minimized_array)->size != 0 && (const char *) minimized < ((const char *) (&this->minimized_array)->data + (&this->minimized_array)->size); 
        (minimized)++) 
    {
        // with the exception of the current selected
		if ((*minimized)->surface == this->current->surface)
			continue;
		weston_view_move_to_layer(*minimized,
					  &this->shell->minimized_layer.view_list);
    }
	
    /*
    wl_array_for_each(minimized, &this->minimized_array) {
		// with the exception of the current selected
		if ((*minimized)->surface == this->current->surface)
			continue;
		weston_view_move_to_layer(*minimized,
					  &this->shell->minimized_layer.view_list);
	}*/

	wl_array_release(&this->minimized_array);
}



