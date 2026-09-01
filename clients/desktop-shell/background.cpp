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

#include "background.hpp"

Background::Background(Desktop *desktop, Output *output)
{
	//struct background *background;
	struct weston_config_section *s;
	char *type;

	//background = xzalloc(sizeof *background);
	this->owner = output;
	this->base.configure = background_configure;
	this->window = window_create_custom(desktop->display);
	this->widget = window_add_widget(this->window, this);
	window_set_user_data(this->window, this);
	widget_set_redraw_handler(this->widget, background_draw);
	widget_set_transparent(this->widget, 0);

	s = weston_config_get_section(desktop->config, "shell", NULL, NULL);
	weston_config_section_get_string(s, "background-image",
					 &this->image, NULL);
	weston_config_section_get_color(s, "background-color",
					&this->color, 0x00000000);

	weston_config_section_get_string(s, "background-type",
					 &type, "tile");
	if (type == NULL) {
		fprintf(stderr, "%s: out of memory\n", program_invocation_short_name);
		exit(EXIT_FAILURE);
	}

	if (strcmp(type, "scale") == 0) {
		this->type = BACKGROUND_SCALE;
	} else if (strcmp(type, "scale-crop") == 0) {
		this->type = BACKGROUND_SCALE_CROP;
	} else if (strcmp(type, "tile") == 0) {
		this->type = BACKGROUND_TILE;
	} else if (strcmp(type, "centered") == 0) {
		this->type = BACKGROUND_CENTERED;
	} else {
		this->type = -1;
		fprintf(stderr, "invalid background-type: %s\n",
			type);
	}

	free(type);

	//return background;
}

Background::~Background()
{
	widget_destroy(this->widget);
	window_destroy(this->window);

	free(this->image);
	//free(this);
}

void
Background::background_draw(struct widget *widget, void *data)
{
	Background *background = static_cast<Background *>(data);
	cairo_surface_t *surface, *image;
	cairo_pattern_t *pattern;
	cairo_matrix_t matrix;
	cairo_t *cr;
	double im_w, im_h;
	double sx, sy, s;
	double tx, ty;
	struct rectangle allocation;

	surface = window_get_surface(background->window);

	cr = widget_cairo_create(background->widget);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	if (background->color == 0)
		cairo_set_source_rgba(cr, 0.0, 0.0, 0.2, 1.0);
	else
		set_hex_color(cr, background->color);
	cairo_paint(cr);

	widget_get_allocation(widget, &allocation);
	image = NULL;
	if (background->image)
		image = load_cairo_surface(background->image);
	else if (background->color == 0) {
		char *name = file_name_with_datadir("pattern.png");

		image = load_cairo_surface(name);
		free(name);
	}

	if (image && background->type != -1) {
		im_w = cairo_image_surface_get_width(image);
		im_h = cairo_image_surface_get_height(image);
		sx = im_w / allocation.width;
		sy = im_h / allocation.height;

		pattern = cairo_pattern_create_for_surface(image);

		switch (background->type) {
		case BACKGROUND_SCALE:
			cairo_matrix_init_scale(&matrix, sx, sy);
			cairo_pattern_set_matrix(pattern, &matrix);
			cairo_pattern_set_extend(pattern, CAIRO_EXTEND_PAD);
			break;
		case BACKGROUND_SCALE_CROP:
			s = (sx < sy) ? sx : sy;
			/* align center */
			tx = (im_w - s * allocation.width) * 0.5;
			ty = (im_h - s * allocation.height) * 0.5;
			cairo_matrix_init_translate(&matrix, tx, ty);
			cairo_matrix_scale(&matrix, s, s);
			cairo_pattern_set_matrix(pattern, &matrix);
			cairo_pattern_set_extend(pattern, CAIRO_EXTEND_PAD);
			break;
		case BACKGROUND_TILE:
			cairo_pattern_set_extend(pattern, CAIRO_EXTEND_REPEAT);
			break;
		case BACKGROUND_CENTERED:
			s = (sx < sy) ? sx : sy;
			if (s < 1.0)
				s = 1.0;

			/* align center */
			tx = (im_w - s * allocation.width) * 0.5;
			ty = (im_h - s * allocation.height) * 0.5;

			cairo_matrix_init_translate(&matrix, tx, ty);
			cairo_matrix_scale(&matrix, s, s);
			cairo_pattern_set_matrix(pattern, &matrix);
			break;
		}

		cairo_set_source(cr, pattern);
		cairo_pattern_destroy (pattern);
		cairo_surface_destroy(image);
		cairo_mask(cr, pattern);
	}

	cairo_destroy(cr);
	cairo_surface_destroy(surface);

	background->painted = 1;
	check_desktop_ready(background->window);
}

void
Background::background_configure(void *data,
		     struct weston_desktop_shell *desktop_shell,
		     uint32_t edges, struct window *window,
		     int32_t width, int32_t height)
{
	Output *owner;
	Background *background = static_cast<Background *>(window_get_user_data(window));

	if (width < 1 || height < 1) {
		/* Shell plugin configures 0x0 for redundant background. */
		owner = background->owner;
		delete background;
		owner->background = NULL;
		return;
	}

	if (!background->image && background->color) {
		widget_set_viewport_destination(background->widget, width, height);
		width = 1;
		height = 1;
	}

	widget_schedule_resize(background->widget, width, height);
}