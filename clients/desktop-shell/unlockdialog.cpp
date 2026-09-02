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

#include "unlockdialog.hpp"

UnlockDialog::UnlockDialog(Desktop *desktop)
{
	struct display *display = desktop->display;
	//struct unlock_dialog *dialog;
	struct wl_surface *surface;

	//dialog = xzalloc(sizeof *dialog);

	this->window = window_create_custom(display);
	this->widget = window_frame_create(this->window, this);
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

void unlock_dialog_redraw_handler(struct widget *widget, void *data)
{
	UnlockDialog *dialog = static_cast<UnlockDialog *>(data);
	struct rectangle allocation;
	cairo_surface_t *surface;
	cairo_t *cr;
	cairo_pattern_t *pat;
	double cx, cy, r, f;

	cr = widget_cairo_create(widget);

	widget_get_allocation(dialog->widget, &allocation);
	cairo_rectangle(cr, allocation.x, allocation.y,
			allocation.width, allocation.height);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba(cr, 0, 0, 0, 0.6);
	cairo_fill(cr);

	cairo_translate(cr, allocation.x, allocation.y);
	if (dialog->button_focused)
		f = 1.0;
	else
		f = 0.7;

	cx = allocation.width / 2.0;
	cy = allocation.height / 2.0;
	r = (cx < cy ? cx : cy) * 0.4;
	pat = cairo_pattern_create_radial(cx, cy, r * 0.7, cx, cy, r);
	cairo_pattern_add_color_stop_rgb(pat, 0.0, 0, 0.86 * f, 0);
	cairo_pattern_add_color_stop_rgb(pat, 0.85, 0.2 * f, f, 0.2 * f);
	cairo_pattern_add_color_stop_rgb(pat, 1.0, 0, 0.86 * f, 0);
	cairo_set_source(cr, pat);
	cairo_pattern_destroy(pat);
	cairo_arc(cr, cx, cy, r, 0.0, 2.0 * M_PI);
	cairo_fill(cr);

	widget_set_allocation(dialog->button,
			      allocation.x + cx - r,
			      allocation.y + cy - r, 2 * r, 2 * r);

	cairo_destroy(cr);

	surface = window_get_surface(dialog->window);
	cairo_surface_destroy(surface);
}

void unlock_dialog_button_handler(struct widget *widget,
			     struct input *input, uint32_t time,
			     uint32_t button,
			     enum wl_pointer_button_state state, void *data)
{
	UnlockDialog *dialog = static_cast<UnlockDialog *>(data);
	Desktop *desktop = dialog->desktop;

	if (button == BTN_LEFT) {
		if (state == WL_POINTER_BUTTON_STATE_RELEASED &&
		    !dialog->closing) {
			display_defer(desktop->display, &desktop->unlock_task);
			dialog->closing = 1;
		}
	}
}

void unlock_dialog_touch_down_handler(struct widget *widget, struct input *input,
		   uint32_t serial, uint32_t time, int32_t id,
		   float x, float y, void *data)
{
	UnlockDialog *dialog = static_cast<UnlockDialog *>(data);

	dialog->button_focused = 1;
	widget_schedule_redraw(widget);
}

void unlock_dialog_touch_up_handler(struct widget *widget, struct input *input,
				uint32_t serial, uint32_t time, int32_t id,
				void *data)
{
	UnlockDialog *dialog = static_cast<UnlockDialog *>(data);
	Desktop *desktop = dialog->desktop;

	dialog->button_focused = 0;
	widget_schedule_redraw(widget);
	display_defer(desktop->display, &desktop->unlock_task);
	dialog->closing = 1;
}

void unlock_dialog_keyboard_focus_handler(struct window *window,
				     struct input *device, void *data)
{
	window_schedule_redraw(window);
}

int unlock_dialog_widget_enter_handler(struct widget *widget,
				   struct input *input,
				   float x, float y, void *data)
{
	UnlockDialog *dialog = static_cast<UnlockDialog *>(data);

	dialog->button_focused = 1;
	widget_schedule_redraw(widget);

	return CURSOR_LEFT_PTR;
}

void
unlock_dialog_widget_leave_handler(struct widget *widget,
				   struct input *input, void *data)
{
	UnlockDialog *dialog = static_cast<UnlockDialog *>(data);

	dialog->button_focused = 0;
	widget_schedule_redraw(widget);
}

void
unlock_dialog_finish(struct task *task, uint32_t events)
{
	Desktop *desktop =
		container_of(task, Desktop, unlock_task);

	weston_desktop_shell_unlock(desktop->shell);
	delete (desktop->unlock_dialog);
	desktop->unlock_dialog = NULL;
}