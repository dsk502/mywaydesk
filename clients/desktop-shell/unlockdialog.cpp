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

/*
static struct unlock_dialog *
unlock_dialog_create(struct desktop *desktop)*/
UnlockDialog::UnlockDialog(Desktop *desktop)
{
	struct display *display = desktop->display;
	//struct unlock_dialog *dialog;
	struct wl_surface *surface;

	//dialog = xzalloc(sizeof *dialog);

	this->window = window_create_custom(display);
	this->widget = window_frame_create(dialog->window, dialog);
	window_set_title(this->window, "Unlock your desktop");

	window_set_user_data(this->window, this);
	window_set_keyboard_focus_handler(this->window,
					  unlock_dialog_keyboard_focus_handler);
	this->button = widget_add_widget(this->widget, this);
	widget_set_redraw_handler(this->widget,
				  unlock_dialog_redraw_handler);
	widget_set_enter_handler(this->button,
				 unlock_dialog_widget_enter_handler);
	widget_set_leave_handler(this->button,
				 unlock_dialog_widget_leave_handler);
	widget_set_button_handler(this->button,
				  unlock_dialog_button_handler);
	widget_set_touch_down_handler(this->button,
				      unlock_dialog_touch_down_handler);
	widget_set_touch_up_handler(this->button,
				      unlock_dialog_touch_up_handler);

	surface = window_get_wl_surface(this->window);
	weston_desktop_shell_set_lock_surface(desktop->shell, surface);

	window_schedule_resize(this->window, 260, 230);

	//return dialog;
}

/*
static void
unlock_dialog_destroy(struct unlock_dialog *dialog)*/
UnlockDialog::~UnlockDialog()
{
	window_destroy(this->window);
	//free(dialog);
}