/*
 * wm-wayland - A Fluxbox-inspired Wayland compositor
 * server.h - Core compositor state
 */

#ifndef WM_SERVER_H
#define WM_SERVER_H

#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/box.h>

#include "keybind.h"

/* Forward declarations for types defined by other modules */
struct wm_view;
struct wm_workspace;
struct wm_config;
struct wm_style;

#define WM_XCURSOR_DEFAULT "left_ptr"
#define WM_XCURSOR_SIZE 24

enum wm_cursor_mode {
	WM_CURSOR_PASSTHROUGH = 0,
	WM_CURSOR_MOVE,
	WM_CURSOR_RESIZE,
	WM_CURSOR_TABBING, /* dragging a tab onto another window */
};

struct wm_server {
	struct wl_display *wl_display;
	struct wl_event_loop *wl_event_loop;

	struct wlr_backend *backend;
	struct wlr_session *session;
	struct wlr_renderer *renderer;
	struct wlr_allocator *allocator;
	struct wlr_compositor *compositor;

	struct wlr_scene *scene;
	struct wlr_scene_output_layout *scene_layout;

	/* Scene tree layers (bottom to top) */
	struct wlr_scene_tree *view_tree;
	struct wlr_scene_tree *xdg_popup_tree;

	struct wlr_xdg_shell *xdg_shell;
	struct wl_listener new_xdg_toplevel;
	struct wl_listener new_xdg_popup;

	/* xdg-decoration protocol (SSD negotiation) */
	struct wlr_xdg_decoration_manager_v1 *xdg_decoration_mgr;
	struct wl_listener new_xdg_decoration;

	/* Theme/style for decorations */
	struct wm_style *style;

	struct wl_list outputs; /* wm_output.link */
	struct wl_listener new_output;
	struct wlr_output_layout *output_layout;

	/* Input / seat */
	struct wlr_seat *seat;
	struct wl_listener new_input;
	struct wl_listener request_cursor;
	struct wl_listener request_set_selection;

	struct wlr_cursor *cursor;
	struct wlr_xcursor_manager *cursor_mgr;
	struct wl_listener cursor_motion;
	struct wl_listener cursor_motion_absolute;
	struct wl_listener cursor_button;
	struct wl_listener cursor_axis;
	struct wl_listener cursor_frame;

	/* Interactive move/resize state */
	enum wm_cursor_mode cursor_mode;
	struct wm_view *grabbed_view;
	double grab_x, grab_y;
	struct wlr_box grab_geobox;
	uint32_t resize_edges;

	/* Focus policy (click-to-focus, sloppy) */
	int focus_policy; /* enum wm_focus_policy from view.h */
	struct wm_view *focused_view;

	/* Managed windows */
	struct wl_list views; /* wm_view.link */

	/* Workspaces */
	struct wl_list workspaces; /* wm_workspace.link */
	struct wm_workspace *current_workspace;
	int workspace_count;
	struct wlr_scene_tree *sticky_tree; /* always-visible views */

	/* Input devices */
	struct wl_list keyboards; /* wm_keyboard.link */

	/* Configuration and keybindings */
	struct wm_config *config;
	struct wl_list keybindings; /* wm_keybind.link (legacy flat list) */
	struct wl_list keymodes;    /* wm_keymode.link */
	char *current_keymode;      /* active keymode name ("default") */
	struct wm_chain_state chain_state; /* multi-key chain state */

	/* Signal handlers */
	struct wl_event_source *sigint_source;
	struct wl_event_source *sigterm_source;
	struct wl_event_source *sighup_source;

	const char *socket;
};

/* Initialize the server (create display, backend, renderer, scene, etc.) */
bool wm_server_init(struct wm_server *server);

/* Start the backend and bind socket */
bool wm_server_start(struct wm_server *server);

/* Run the event loop */
void wm_server_run(struct wm_server *server);

/* Tear down and free resources */
void wm_server_destroy(struct wm_server *server);

#endif /* WM_SERVER_H */
