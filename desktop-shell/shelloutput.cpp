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

ShellOutput::ShellOutput(DesktopShell *shell, struct weston_output *output)
    : shell(nullptr), output(nullptr), destroy_listener{}, link{},
	panel_surface(nullptr), panel_view(nullptr), panel_surface_listener{}, panel_offset{},
    dock_surface(nullptr), dock_view(nullptr), dock_surface_listener{}, dock_offset{},
	background_surface(nullptr), background_view(nullptr), background_surface_listener{}
{
	//struct shell_output *shell_output;

	//shell_output = zalloc(sizeof *shell_output);
	//if (shell_output == NULL)
		//return;

	this->output = output;
	this->shell = shell;
	this->destroy_listener.notify = handle_output_destroy;
	wl_signal_add(&output->destroy_signal,
		      &this->destroy_listener);
	wl_list_insert(shell->output_list.prev, &this->link);

	if (wl_list_length(&shell->output_list) == 1)
		shell_for_each_layer(shell,
				     shell_output_changed_move_layer, NULL);
}