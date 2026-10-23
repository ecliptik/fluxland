/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
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
	WM_XW_MANAGED,      /* normal window */
	WM_XW_FLOATING,     /* dialog/utility/transient: decorated, floating */
	WM_XW_SPLASH,       /* splash screen: undecorated */
	WM_XW_POPUP,        /* tooltip/notification/menu (not OR): no frame */
	WM_XW_DOCK,         /* _NET_WM_WINDOW_TYPE_DOCK: goes to the slit */
	WM_XW_DESKTOP,      /* _NET_WM_WINDOW_TYPE_DESKTOP: desktop layer */
	WM_XW_UNMANAGED,    /* override-redirect: drawn, never managed */
};

enum wm_xw_window_class wm_xwayland_classify(
	struct wlr_xwayland_surface *xsurface,
	const struct wm_xwayland_atoms *atoms);

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

/*
 * Send X11 windows their new position after they moved. Called before
 * each frame, so every code path that moves a window is covered.
 */
void wm_xwayland_sync_positions(struct wm_server *server);

#endif /* WM_XWAYLAND_H */
