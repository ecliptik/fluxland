/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 *
 * xwayland.h - XWayland bridge for X11 application support
 *
 * Compile-time optional: guarded by WM_HAS_XWAYLAND.
 * When disabled, wm_xwayland_init/finish are harmless no-ops.
 */

#ifndef WM_XWAYLAND_H
#define WM_XWAYLAND_H

#include <stdbool.h>
#include <wayland-server-core.h>

struct wm_server;

#ifdef WM_HAS_XWAYLAND

#include <wlr/xwayland.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/box.h>
#include <xcb/xcb.h>

/* Resolved X11 atom cache for window type matching */
struct wm_xwayland_atoms {
	xcb_atom_t net_wm_window_type_normal;
	xcb_atom_t net_wm_window_type_dialog;
	xcb_atom_t net_wm_window_type_utility;
	xcb_atom_t net_wm_window_type_toolbar;
	xcb_atom_t net_wm_window_type_splash;
	xcb_atom_t net_wm_window_type_menu;
	xcb_atom_t net_wm_window_type_dropdown_menu;
	xcb_atom_t net_wm_window_type_popup_menu;
	xcb_atom_t net_wm_window_type_tooltip;
	xcb_atom_t net_wm_window_type_notification;
	xcb_atom_t net_wm_window_type_dock;
	xcb_atom_t net_wm_window_type_desktop;
};

/* Classification of an X11 window for compositor management */
enum wm_xw_window_class {
	WM_XW_MANAGED,      /* normal managed view with decorations */
	WM_XW_FLOATING,     /* dialog/utility: floating, possibly modal */
	WM_XW_SPLASH,       /* splash screen: undecorated, centered */
	WM_XW_UNMANAGED,    /* tooltip/notification/OR: overlay, no stacking */
};

struct wm_xwayland_view {
	struct wm_server *server;
	struct wlr_xwayland_surface *xsurface;
	struct wlr_scene_tree *scene_tree;

	/* Position in layout coordinates */
	int x, y;
	bool mapped;

	/* Saved geometry for maximize/fullscreen restore */
	struct wlr_box saved_geometry;
	bool maximized;
	bool fullscreen;

	/* Window classification */
	enum wm_xw_window_class window_class;

	/* Metadata (mirrored from X11 properties) */
	char *title;
	char *app_id; /* from WM_CLASS */

	/* Event listeners */
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener destroy;
	struct wl_listener associate;
	struct wl_listener dissociate;
	struct wl_listener request_configure;
	struct wl_listener request_move;
	struct wl_listener request_resize;
	struct wl_listener request_maximize;
	struct wl_listener request_fullscreen;
	struct wl_listener request_minimize;
	struct wl_listener request_activate;
	struct wl_listener set_title;
	struct wl_listener set_class;
	struct wl_listener set_hints;
	struct wl_listener set_override_redirect;

	/* List membership */
	struct wl_list link; /* wm_server.xwayland_views */
};

#endif /* WM_HAS_XWAYLAND */

/*
 * Initialize XWayland support.
 * Sets up wlr_xwayland, listens for new surfaces, and sets DISPLAY.
 * No-op when compiled without WM_HAS_XWAYLAND.
 */
void wm_xwayland_init(struct wm_server *server);

/*
 * Tear down XWayland.
 * No-op when compiled without WM_HAS_XWAYLAND.
 */
void wm_xwayland_finish(struct wm_server *server);

#endif /* WM_XWAYLAND_H */
