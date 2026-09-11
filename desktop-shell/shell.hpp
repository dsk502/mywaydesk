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

#ifndef _DS_SHELL_HPP_	//desktop-shell/shell.hpp
#define _DS_SHELL_HPP_

extern "C" {
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <linux/input.h>
#include <assert.h>
#include <signal.h>
#include <math.h>
#include <sys/types.h>
#include <time.h>

#include <libweston/libweston.h>
#include <libweston/xwayland-api.h>
#include <libweston/config-parser.h>
#include <libweston/shell-utils.h>
#include <libweston/desktop.h>

#include "frontend/weston.h"
#include "shared/helpers.h"
#include "shared/timespec-util.h"

#include "weston-desktop-shell-server-protocol.h"
}

enum animation_type {
	ANIMATION_NONE,

	ANIMATION_ZOOM,
	ANIMATION_FADE,
	ANIMATION_DIM_LAYER,
};

enum fade_type {
	FADE_IN,
	FADE_OUT
};

struct shell_grab {
	struct weston_pointer_grab grab;
	ShellSurface *shsurf;
	struct wl_listener shsurf_destroy_listener;
};

struct shell_touch_grab {
	struct weston_touch_grab grab;
	ShellSurface *shsurf;
	struct wl_listener shsurf_destroy_listener;
	struct weston_touch *touch;
};

struct shell_tablet_tool_grab {
	struct weston_tablet_tool_grab grab;
	ShellSurface *shsurf;
	struct wl_listener shsurf_destroy_listener;
	struct weston_tablet_tool *tool;
};

struct weston_move_grab {
	struct shell_grab base;
	struct weston_coord_global delta;
	bool client_initiated;
};

struct weston_touch_move_grab {
	struct shell_touch_grab base;
	int active;
	struct weston_coord_global delta;
};

struct weston_tablet_tool_move_grab {
	struct shell_tablet_tool_grab base;
	wl_fixed_t dx, dy;
};

struct rotate_grab {
	struct shell_grab base;
	struct weston_matrix rotation;
	struct {
		float x;
		float y;
	} center;
};

struct weston_resize_grab {
	struct shell_grab base;
	uint32_t edges;
	int32_t width, height;
};

struct shell_seat {
	struct weston_seat *seat;
	struct wl_listener seat_destroy_listener;
	struct weston_surface *focused_surface;

	struct wl_listener caps_changed_listener;
	struct wl_listener pointer_focus_listener;
	struct wl_listener keyboard_focus_listener;
	struct wl_listener tablet_tool_added_listener;

	struct wl_list link;	/** shell::seat_list */
};

struct tablet_tool_listener {
	struct wl_listener base;
	struct wl_listener removed_listener;
};

struct focus_surface {
	struct weston_curtain *curtain;
};

class Workspace {
public:
	struct weston_layer layer;

	struct wl_list focus_list;
	struct wl_listener seat_destroyed_listener;

	struct focus_surface *fsurf_front;
	struct focus_surface *fsurf_back;
	struct weston_view_animation *focus_animation;

	//Constructor and Deconstructor
	Workspace(DesktopShell *shell);
	~Workspace();
};

class ShellOutput {
public:
	DesktopShell  *shell;
	struct weston_output  *output;
	struct wl_listener    destroy_listener;
	struct wl_list        link;

	struct weston_surface *panel_surface;
	struct weston_view *panel_view;
	struct wl_listener panel_surface_listener;
	struct weston_coord_global panel_offset;

	//Add dock
	struct weston_surface *dock_surface;
	struct weston_view *dock_view;
	struct wl_listener dock_surface_listener;
	struct weston_coord_global dock_offset;

	struct weston_surface *background_surface;
	struct weston_view *background_view;
	struct wl_listener background_surface_listener;

	ShellOutput(DesktopShell *shell, struct weston_output *output);

};

struct weston_desktop;

class DesktopShell {
public:
	struct weston_compositor *compositor;
	struct weston_desktop *desktop;
	const struct weston_xwayland_surface_api *xwayland_surface_api;

	struct wl_listener idle_listener;
	struct wl_listener wake_listener;
	struct wl_listener transform_listener;
	struct wl_listener resized_listener;
	struct wl_listener destroy_listener;
	struct wl_listener show_input_panel_listener;
	struct wl_listener hide_input_panel_listener;
	struct wl_listener update_input_panel_listener;
	struct wl_listener session_listener;

	struct weston_layer fullscreen_layer;
	struct weston_layer panel_layer;
	struct weston_layer background_layer;
	struct weston_layer lock_layer;
	struct weston_layer input_panel_layer;

	struct wl_listener pointer_focus_listener;
	struct weston_surface *grab_surface;

	struct {
		struct wl_client *client;
		struct wl_resource *desktop_shell;
		struct wl_listener client_destroy_listener;

		unsigned deathcount;
		struct timespec deathstamp;
	} child;

	bool locked;
	bool showing_input_panels;
	bool prepare_event_sent;

	struct text_backend *text_backend;

	struct {
		struct weston_surface *surface;
		pixman_box32_t cursor_rectangle;
	} text_input;

	struct weston_surface *lock_surface;
	struct wl_listener lock_surface_listener;
	struct weston_view *lock_view;

	Workspace workspace;

	struct {
		struct wl_resource *binding;
		struct wl_list surfaces;
	} input_panel;

	struct {
		struct weston_curtain *curtain;
		struct weston_view_animation *animation;
		enum fade_type type;
		struct wl_event_source *startup_timer;
	} fade;

	bool allow_zap;
	uint32_t binding_modifier;
	enum animation_type win_animation_type;
	enum animation_type win_close_animation_type;
	enum animation_type startup_animation_type;
	enum animation_type focus_animation_type;

	struct weston_layer minimized_layer;

	struct wl_listener seat_create_listener;
	struct wl_listener output_create_listener;
	struct wl_listener output_move_listener;
	struct wl_list output_list;
	struct wl_list seat_list;
	struct wl_list shsurf_list;

	enum weston_desktop_shell_panel_position panel_position;
	enum weston_desktop_shell_dock_position dock_position;	//Added

	char *client;

	struct timespec startup_time;

	//Constructor and deconstructor
	DesktopShell();
	~DesktopShell();

	//Member functions
	//void workspace_create();
	bool shell_configuration();
	void shell_fade(enum fade_type type);
	void shell_fade_init();
	void shell_fade_startup();

	void lock();
	void unlock();
	void resume_desktop();
	Workspace * get_current_workspace();
};

class ShellSurface {
public:
	struct wl_signal destroy_signal;

	struct weston_desktop_surface *desktop_surface;
	struct weston_view *view;
	struct weston_surface *wsurface_anim_fade;
	struct weston_view *wview_anim_fade;
	int32_t last_width, last_height;

	class DesktopShell *shell;

	struct wl_list children_list;
	struct wl_list children_link;

	struct weston_coord_global saved_pos;
	bool saved_position_valid;
	bool saved_rotation_valid;
	int unresponsive, grabbed;
	uint32_t resize_edges;
	uint32_t orientation;

	struct {
		struct weston_transform transform;
		struct weston_matrix rotation;
	} rotation;

	struct {
		struct weston_curtain *black_view;
	} fullscreen;

	struct weston_output *fullscreen_output;
	struct weston_output *output;
	struct wl_listener output_destroy_listener;

	struct surface_state {
		bool fullscreen;
		bool maximized;
		bool lowered;
	} state;

	struct {
		bool is_set;
		struct weston_coord_global pos;
	} xwayland;

	int focus_count;

	bool destroying;
	struct wl_list link;	// desktop_shell::shsurf_list

	//Constructor
	ShellSurface();
	~ShellSurface();

	//Methods
	DesktopShell *shell_surface_get_shell();
	void get_maximized_size(int32_t *width, int32_t *height);
    void set_busy_cursor(weston_pointer *pointer);
    int surface_move(weston_pointer *pointer, bool client_initiated);
    void surface_rotate(weston_pointer *pointer);

    bool shsurf_is_max_or_fullscreen();
    void set_shsurf_size_maximized_or_fullscreen(bool max_requested, bool fullscreen_requested);
    void set_fullscreen(bool fullscreen, weston_output *output);
    void unset_fullscreen();
	void set_maximized(bool maximized);
    void unset_maximized();
    void shell_set_view_fullscreen();

    struct weston_layer_entry * shell_surface_calculate_layer_link();
    void shell_surface_activate();
	void shell_surface_deactivate();

	void shell_surface_update_child_surface_layers();
	void shell_surface_update_layer();
	void shell_surface_set_output(struct weston_output *output);

	bool has_keyboard_focused_child();
	int surface_resize(struct weston_pointer *pointer, uint32_t edges);
};

class Switcher {
public:
	DesktopShell *shell;
	struct weston_view *current;
	struct wl_listener listener;
	struct weston_keyboard_grab grab;
	struct wl_array minimized_array;

	Switcher();
	~Switcher();

    void switcher_next();
    static void switcher_handle_view_destroy(wl_listener *listener, void *data);
};

/*
 * Common variables (only declarations)
 */

extern const struct weston_pointer_grab_interface busy_cursor_grab_interface;

extern const struct weston_pointer_grab_interface resize_grab_interface;

extern const struct weston_pointer_grab_interface rotate_grab_interface;

extern const struct weston_pointer_grab_interface move_grab_interface;

/*
 * Tool methods
 */

struct weston_output *
get_default_output(struct weston_compositor *compositor);

struct weston_view *
get_default_view(struct weston_surface *surface);

ShellSurface *
get_shell_surface(struct weston_surface *surface);

void
get_output_work_area(DesktopShell *shell,
		     struct weston_output *output,
		     pixman_rectangle32_t *area);

void
lower_fullscreen_layer(DesktopShell *shell,
		       struct weston_output *lowering_output);

void
activate(DesktopShell *shell, struct weston_view *view,
	 struct weston_seat *seat, uint32_t flags);

int
input_panel_setup(DesktopShell *shell);

void
input_panel_destroy(DesktopShell *shell);

typedef void (*shell_for_each_layer_func_t)(DesktopShell *,
					    struct weston_layer *, void *);

void
shell_for_each_layer(DesktopShell *shell,
		     shell_for_each_layer_func_t func,
		     void *data);

void
black_surface_committed(struct weston_surface *es,
			struct weston_coord_surface new_origin);

bool
is_black_surface_view(struct weston_view *view, struct weston_view **fs_view);

void
shell_grab_start(struct shell_grab *grab,
		 const struct weston_pointer_grab_interface *interface,
		 ShellSurface *shsurf,
		 struct weston_pointer *pointer,
		 enum weston_desktop_shell_cursor cursor);

bool
is_focus_view(struct weston_view *view);

void
fade_out_done(struct weston_view_animation *animation, void *data);

void
restore_focus_state(DesktopShell *shell, Workspace *ws);

void
weston_view_set_initial_position(struct weston_view *view,
				 DesktopShell *shell);

void
unfocus_all_seats(DesktopShell *shell);

int
black_surface_get_label(struct weston_surface *surface, char *buf, size_t len);

void
shell_output_changed_move_layer(DesktopShell *shell,
				struct weston_layer *layer,
				void *data);

void
handle_output_destroy(struct wl_listener *listener, void *data);

#endif