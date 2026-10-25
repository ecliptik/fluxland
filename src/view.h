/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * view.h - Window/view management (XDG shell clients)
 */

#ifndef WM_VIEW_H
#define WM_VIEW_H

#include <stdint.h>
#include <sys/types.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/box.h>

#include "server.h"
#include "view_layer.h"
struct wm_workspace;
struct wm_tab_group;
struct wm_decoration;
struct wm_animation;
struct wlr_foreign_toplevel_handle_v1;
struct wlr_ext_foreign_toplevel_handle_v1;


struct wlr_xwayland_surface;
struct wm_view;

enum wm_view_type {
	WM_VIEW_XDG,        /* xdg-shell toplevel (view.c) */
	WM_VIEW_XWAYLAND,   /* managed X11 window (xwayland.c) */
};

/*
 * Backend operations. Window management code calls the wm_view_*()
 * wrappers below and never touches xdg_toplevel/xsurface directly.
 */
struct wm_view_impl {
	struct wlr_surface *(*surface)(struct wm_view *view);
	/* Visible content: offset within the surface, and size */
	void (*content_geometry)(struct wm_view *view, struct wlr_box *geo);
	/* Ask the client for a new content size (0 = client's choice) */
	void (*set_size)(struct wm_view *view, int width, int height);
	/* view->x/y changed (X11 clients must learn their position) */
	void (*moved)(struct wm_view *view);
	void (*set_activated)(struct wm_view *view, bool activated);
	void (*set_maximized)(struct wm_view *view, bool maximized);
	void (*set_fullscreen)(struct wm_view *view, bool fullscreen);
	void (*set_tiled)(struct wm_view *view, uint32_t edges);
	void (*close)(struct wm_view *view);
	struct wl_client *(*client)(struct wm_view *view);
	pid_t (*pid)(struct wm_view *view);
	struct wm_view *(*parent)(struct wm_view *view);
	/* Optional: tell the client it was (un)minimized */
	void (*set_minimized)(struct wm_view *view, bool minimized);
};

struct wm_view {
	struct wl_list link; /* wm_server.views */
	struct wm_server *server;
	enum wm_view_type type;
	const struct wm_view_impl *impl;  /* NULL = xdg-shell */
	struct wlr_xdg_toplevel *xdg_toplevel;    /* WM_VIEW_XDG */
	struct wlr_xwayland_surface *xsurface;    /* WM_VIEW_XWAYLAND */
	struct wlr_scene_tree *scene_tree;

	/* Unique view identifier for IPC (monotonically increasing) */
	uint32_t id;

	/* Geometry (layout coordinates) */
	int x, y;

	/* Saved geometry for maximize/fullscreen restore */
	struct wlr_box saved_geometry;
	bool maximized;
	bool fullscreen;
	bool maximized_vert;
	bool maximized_horiz;
	bool lhalf;
	bool rhalf;

	/* Decoration visibility toggle */
	bool show_decoration; /* default true */

	/* Metadata */
	char *title;
	bool title_override;	/* SetTitle: the client's titles are ignored */
	char *app_id;     /* xdg app_id; X11 WM_CLASS class */
	char *instance;   /* X11 WM_CLASS instance (NULL for xdg) */
	char *role;       /* X11 WM_WINDOW_ROLE (NULL for xdg) */
	bool app_id_at_map; /* had an app_id when rules were matched */

	/* Window-type hints from the backend (e.g. X11 splash/tooltip) */
	bool no_decoration;
	bool no_focus;
	/* Set by window rules before the window is shown; realized at map */
	bool pending_maximize, pending_fullscreen, pending_minimize;
	bool pending_maximize_horiz, pending_maximize_vert;
	/* [Maximized] {no} / [Fullscreen] {no}: refuse the client's request */
	bool refuse_maximize, refuse_fullscreen;
	/* [Position] placed the window: no automatic placement */
	bool rule_placed;
	/* The apps-file entry this window matched (see rules.c) */
	struct wm_window_rule *rule;
	/* xdg-dialog: a modal dialog takes focus from its parent */
	bool modal;
	/*
	 * Iconbar icon (WM_ICON_SIZE px, cairo_surface_t): the client's own
	 * (xdg-toplevel-icon) or its desktop file's.
	 */
	struct _cairo_surface *icon;
	bool icon_from_client;
	/* xdg-toplevel-tag: what the app calls this window (matchable) */
	char *tag;
	/* Window capture source (made on first request; dies with the node) */
	struct wlr_ext_image_capture_source_v1 *capture_source;
	int pending_head; /* [Head] rule: 0 = none */

	/* Workspace tracking */
	struct wm_workspace *workspace;
	bool sticky; /* visible on all workspaces */

	/* Layer stacking (Fluxbox layer model) */
	enum wm_view_layer layer;

	/* Tab group (Fluxbox-style window grouping) */
	struct wm_tab_group *tab_group;
	struct wl_list tab_link; /* wm_tab_group.views */

	/* Window opacity (0-255, 255 = fully opaque) */
	int focus_alpha;
	int unfocus_alpha;

	/* Active animation (NULL if none) */
	struct wm_animation *animation;

	/* Focus protection (bitmask of WM_FOCUS_PROT_* flags, 0 = none) */
	int focus_protection;
	int64_t last_key_ms;    /* its last key typed, for noFocusWhileTypingDelay */

	/* Ignore size hints from xdg_toplevel min/max constraints */
	bool ignore_size_hints;

	/* True if this view is docked into the slit (skip normal WM handling) */
	bool docked;

	/* Fluxbox apps attributes: skip in focus cycling / iconbar */
	bool focus_hidden;
	bool icon_hidden;

	/* Demands attention (denied activation, X11 urgency hint) */
	bool urgent;
	bool attention_state;   /* flashing focused while urgent */
	struct wl_event_source *attention_timer;

	/* Surface is mapped (minimized views stay mapped) */
	bool mapped;

	/* Focus order: higher = more recently focused (see focus cycling) */
	uint64_t focus_seq;

	/* Foreign toplevel handle (for external taskbars) */
	struct wlr_foreign_toplevel_handle_v1 *foreign_toplevel_handle;
	struct wlr_ext_foreign_toplevel_handle_v1 *ext_toplevel_handle;
	/* Output the taskbar handle was told about, and the box it was for */
	struct wlr_output *foreign_output;
	struct wlr_box foreign_box;
	struct wl_listener foreign_toplevel_request_activate;
	struct wl_listener foreign_toplevel_request_maximize;
	struct wl_listener foreign_toplevel_request_minimize;
	struct wl_listener foreign_toplevel_request_fullscreen;
	struct wl_listener foreign_toplevel_request_close;

	/* Server-side decorations */
	struct wm_decoration *decoration;

	/* XDG toplevel listeners */
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener commit;
	struct wl_listener destroy;
	struct wl_listener request_move;
	struct wl_listener request_resize;
	struct wl_listener request_maximize;
	struct wl_listener request_fullscreen;
	struct wl_listener request_minimize;
	struct wl_listener request_show_window_menu;
	struct wl_listener set_title;
	struct wl_listener set_app_id;
};

/*
 * Initialize the XDG shell listeners on the server.
 * Must be called after wm_server_init().
 */
void wm_view_init(struct wm_server *server);
void wm_view_finish(struct wm_server *server);

/*
 * Find the topmost view at the given layout coordinates.
 * Returns the view and sets surface/sx/sy for the sub-surface hit.
 */
struct wm_view *wm_view_at(struct wm_server *server, double lx, double ly,
	struct wlr_surface **surface, double *sx, double *sy);

/*
 * Focus the given view. Deactivates the previously focused view,
 * raises view to top, and sends keyboard enter.
 */
void wm_focus_view(struct wm_view *view, struct wlr_surface *surface);

/*
 * Remove focus from the currently focused view and clear keyboard focus.
 */
/* Focus without raising (focus follows mouse) */
void wm_focus_view_noraise(struct wm_view *view);

void wm_unfocus_current(struct wm_server *server);

/*
 * Focus the next or previous view in the stacking order.
 * Used for Alt-Tab / window cycling.
 */
void wm_focus_next_view(struct wm_server *server);

/*
 * Focus the most recently used visible view on the active workspace,
 * skipping `except` (may be NULL). Clears focus if there is none.
 */
void wm_focus_fallback(struct wm_server *server, struct wm_view *except);

/* Minimize (iconify) a view, moving focus away if it was focused. */
void wm_view_minimize(struct wm_view *view);

/* Restore a minimized view, raise and focus it. */
void wm_view_unminimize(struct wm_view *view);

/* Mark a window as demanding attention (shown in the iconbar) */
void wm_view_set_urgent(struct wm_view *view, bool urgent);

/* Icon size kept for the iconbar (scaled down when drawn) */
#define WM_ICON_SIZE 32

/*
 * Set the window's icon (takes a reference; NULL clears). from_client:
 * the client sent it, so app_id changes don't replace it.
 */
void wm_view_set_icon(struct wm_view *view, struct _cairo_surface *icon,
	bool from_client);

/* Use the desktop file icon for the app_id (unless the client set one) */
void wm_view_update_icon(struct wm_view *view);

/*
 * Bring a window to the user: switch to its workspace, make it the
 * visible tab, restore it if minimized, raise and focus it.
 */
void wm_view_present(struct wm_view *view);

/*
 * A client asked for its window to be activated (xdg-activation,
 * _NET_ACTIVE_WINDOW). allowed: the request passed focus-stealing
 * prevention. Presents the window, or marks it urgent when refused (or
 * blocked by [FocusProtection] Deny/Lock). Returns true if presented.
 */
bool wm_view_request_activate(struct wm_view *view, bool allowed);

/*
 * Focus a window that has just opened, if focusNewWindows and its
 * [FocusNewWindow]/[FocusProtection] say so and the focused window
 * doesn't hold on to the focus (Lock, fullscreen); otherwise it opens
 * lowered and wanting attention.
 */
void wm_view_focus_new(struct wm_view *view);

/* A key the focused window got (an xkb keysym), for
 * noFocusWhileTypingDelay */
void wm_view_note_key(struct wm_view *view, uint32_t sym);

/*
 * GoToWindow: bring forward the num-th window of a window list (from the
 * end when num is negative).
 */
struct wm_window_list_spec;
void wm_focus_goto_window(struct wm_server *server,
	const struct wm_window_list_spec *spec, int num);
void wm_focus_prev_view(struct wm_server *server);

struct wm_window_list_spec;

/*
 * NextWindow/PrevWindow: focus the next/previous window in focus order
 * (or creation order with {static}) that matches the spec (NULL = all
 * windows). While modifiers are held the order stays frozen, so
 * repeated presses walk the list; it is committed on release.
 */
void wm_focus_cycle(struct wm_server *server,
	const struct wm_window_list_spec *spec, bool reverse);

/* End a cycle (all modifiers released): commit the focus order. */
void wm_focus_cycle_end(struct wm_server *server);

/*
 * Restore (de-iconify) the most recently minimized view on the
 * current workspace. Focuses and raises the restored view.
 */
void wm_view_deiconify_last(struct wm_server *server);

/*
 * Restore all minimized views across all workspaces.
 */
void wm_view_deiconify_all(struct wm_server *server);

/*
 * Restore all minimized views on the current workspace.
 */
void wm_view_deiconify_all_workspace(struct wm_server *server);

/*
 * Raise the given view to the top of the stacking order.
 */
void wm_view_raise(struct wm_view *view);

/*
 * Lower the given view to the bottom of its layer's stacking order.
 */
void wm_view_lower(struct wm_view *view);

/*
 * Set the view's layer and reparent its scene node accordingly.
 */
void wm_view_set_layer(struct wm_view *view, enum wm_view_layer layer);

/*
 * Move the view up one layer (e.g. Normal -> Above).
 */
void wm_view_raise_layer(struct wm_view *view);

/*
 * Move the view down one layer (e.g. Normal -> Below).
 */
void wm_view_lower_layer(struct wm_view *view);

/*
 * Parse a layer name string (e.g. "Above", "Normal") to enum.
 * Returns WM_LAYER_NORMAL if unrecognized.
 */
enum wm_view_layer wm_view_layer_from_name(const char *name);

/*
 * Called on cursor motion: if sloppy focus is enabled, focus
 * the view under the pointer without raising it.
 */
void wm_focus_update_for_cursor(struct wm_server *server,
	double cursor_x, double cursor_y);

/*
 * Get the bounding box of the view (including XDG geometry offset).
 */
void wm_view_get_geometry(struct wm_view *view, struct wlr_box *box);

/* xdg-shell backend (view_xdg.c); views with impl == NULL use it */
extern const struct wm_view_impl wm_xdg_view_impl;

/*
 * Keep an xdg popup on screen (unconstrained to its output on its first
 * commit). Its scene tree must already be in popup->base->data, nested
 * under the window's or layer surface's tree.
 */
struct wlr_xdg_popup;
void wm_xdg_popup_track(struct wm_server *server, struct wlr_xdg_popup *popup);

/* --- Lifecycle shared by the xdg-shell and XWayland backends --- */

/* Set defaults, create the (hidden) scene tree, add to server->views */
bool wm_view_init_common(struct wm_view *view, struct wm_server *server);
/* Decorations, rules, placement, focus, IPC and taskbar on map */
void wm_view_map_common(struct wm_view *view, bool want_maximized,
	bool want_fullscreen);
void wm_view_unmap_common(struct wm_view *view);
/* Release shared state; the backend frees the view itself */
void wm_view_destroy_common(struct wm_view *view);
void wm_view_set_title(struct wm_view *view, const char *title);
void wm_view_set_app_id(struct wm_view *view, const char *app_id);

/* --- Backend-neutral accessors (xdg-shell or XWayland) --- */

struct wlr_surface *wm_view_surface(struct wm_view *view);
void wm_view_content_geometry(struct wm_view *view, struct wlr_box *geo);
void wm_view_set_size(struct wm_view *view, int width, int height);
void wm_view_set_activated(struct wm_view *view, bool activated);
void wm_view_set_maximized_state(struct wm_view *view, bool maximized);
void wm_view_set_fullscreen_state(struct wm_view *view, bool fullscreen);
void wm_view_set_tiled_state(struct wm_view *view, uint32_t edges);
void wm_view_set_minimized_state(struct wm_view *view, bool minimized);
/* Ask the window to close (WM_DELETE_WINDOW / xdg close) */
void wm_view_close(struct wm_view *view);
/* Disconnect the client (X11: kill the owning process) */
void wm_view_kill(struct wm_view *view);
struct wl_client *wm_view_client(struct wm_view *view);
pid_t wm_view_pid(struct wm_view *view);
/* Transient-for parent, or NULL */
struct wm_view *wm_view_parent(struct wm_view *view);
/* The managed window whose main surface is `surface`, or NULL */
struct wm_view *wm_view_from_surface(struct wm_server *server,
	struct wlr_surface *surface);
/* Move the frame to layout coordinates (x, y) */
void wm_view_move(struct wm_view *view, int x, int y);

/*
 * Begin an interactive move or resize for the given view.
 */
void wm_view_begin_interactive(struct wm_view *view,
	enum wm_cursor_mode mode, uint32_t edges);

/*
 * Set the opacity of a view's scene buffers.
 * alpha is 0-255 (0 = transparent, 255 = fully opaque).
 */
void wm_view_set_opacity(struct wm_view *view, int alpha);

/*
 * Toggle vertical maximization (fill output height, keep x/width).
 */
void wm_view_maximize_vert(struct wm_view *view);

/*
 * Toggle horizontal maximization (fill output width, keep y/height).
 */
void wm_view_maximize_horiz(struct wm_view *view);

/*
 * Toggle maximize on/off for the given view.
 */
void wm_view_toggle_maximize(struct wm_view *view);

/*
 * Toggle fullscreen on/off for the given view.
 */
void wm_view_toggle_fullscreen(struct wm_view *view);

/*
 * Toggle decoration visibility on/off.
 */
void wm_view_toggle_decoration(struct wm_view *view);

/*
 * Activate the nth tab (0-based index) in the view's tab group.
 */
void wm_view_activate_tab(struct wm_view *view, int index);

/*
 * Tile the view to the left half of the output (toggle).
 */
void wm_view_lhalf(struct wm_view *view);

/*
 * Tile the view to the right half of the output (toggle).
 */
void wm_view_rhalf(struct wm_view *view);

/*
 * Resize the view by a relative delta (in pixels).
 */
void wm_view_resize_by(struct wm_view *view, int dw, int dh);

/*
 * Focus the nearest view in the given direction.
 * dx/dy: (-1,0)=left, (1,0)=right, (0,-1)=up, (0,1)=down
 */
void wm_view_focus_direction(struct wm_server *server, int dx, int dy);

/*
 * SetHead N: move the window to head N (Fluxbox numbering, from 1).
 */
void wm_view_set_head(struct wm_view *view, int head);

/*
 * SendToNextHead / SendToPrevHead [offset]: move the window offset heads
 * on (negative: back), wrapping around.
 */
void wm_view_send_to_head_offset(struct wm_view *view, int offset);

/*
 * Close all windows on the current workspace.
 */
void wm_view_close_all(struct wm_server *server);

#endif /* WM_VIEW_H */
