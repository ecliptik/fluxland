/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * systray.h - System tray using StatusNotifierItem D-Bus protocol
 *
 * Implements a StatusNotifierWatcher that discovers and displays tray
 * icons from applications using the SNI (StatusNotifierItem) protocol
 * over D-Bus. Icons are rendered as scene buffers in the toolbar area.
 */

#ifndef WM_SYSTRAY_H
#define WM_SYSTRAY_H

#ifdef WM_HAS_SYSTRAY

#include <cairo.h>
#include <stdbool.h>
#include <stdint.h>
#include <systemd/sd-bus.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_scene.h>

struct wm_server;

/* Where systray.pinLeft/pinRight put an item id: < 0 left, > 0 right */
int wm_systray_pin_order(const char *pin_left, const char *pin_right,
	const char *id);

#define WM_SYSTRAY_ICON_SIZE 18
#define WM_SYSTRAY_ICON_PADDING 2

/* A single tray icon item */
struct wm_systray_item {
	char *bus_name;        /* D-Bus sender / unique name */
	char *object_path;     /* /StatusNotifierItem or custom path */
	char *id;              /* application identifier */
	int order;             /* systray.pinLeft (< 0) / pinRight (> 0) */
	char *icon_name;       /* named icon (freedesktop) */
	char *icon_theme_path; /* IconThemePath: the application's own icons */
	char *menu_path;       /* Menu property: its com.canonical.dbusmenu */
	bool is_menu;          /* ItemIsMenu: it has a menu, no window */
	char *title;           /* Title property, the tooltip's fallback */
	char *tooltip;         /* ToolTip title and text, on one line */
	cairo_surface_t *icon_pixmap; /* rendered icon surface */
	struct wlr_scene_buffer *icon_buf;
	struct wm_systray *systray;
	struct wl_list link;   /* wm_systray.items */
};

/*
 * Give an item an xdg-activation token of the user's click, before the call
 * it is for (see systray.c).  Does nothing if there is no seat.
 */
void wm_systray_provide_token(struct wm_systray *systray,
	const char *bus_name, const char *object_path);

/*
 * systray.activationGrace: a click is being passed on to the program at
 * bus_name.  Returns the grant's id (0: none: off, or its pid unknown), to
 * give as the user data of wm_systray_grant_reply, the reply handler of the
 * call that passes the click on.
 */
uint64_t wm_systray_grant_begin(struct wm_systray *systray,
	const char *bus_name);
int wm_systray_grant_reply(sd_bus_message *reply, void *userdata,
	sd_bus_error *ret_error);

/* ... for the item whose menu (its dbusmenu path) an entry was chosen in */
void wm_systray_provide_menu_token(struct wm_systray *systray,
	const char *bus_name, const char *menu_path);

/* System tray manager */
struct wm_systray {
	struct wm_server *server;
	sd_bus *bus;
	struct wl_event_source *bus_event;
	struct wl_event_source *bus_timer; /* sd-bus's own timeout */
	struct wl_event_source *bus_idle;  /* a sync of the above is due */
	struct wl_list items;  /* wm_systray_item.link */
	int item_count;
	bool vertical;         /* down a turned toolbar */
	struct wlr_scene_tree *scene_tree;
	sd_bus_slot *watcher_slot;
	sd_bus_slot *name_slot;
	sd_bus_slot *signal_slots[3]; /* NewIcon, NewToolTip, NewTitle */
	struct wl_list menu_requests; /* menus being fetched (systray_menu.c) */
	int icon_size;
	int x, y, width, height;
};

/* Create the system tray, connect to D-Bus, register watcher */
struct wm_systray *wm_systray_create(struct wm_server *server);

/* Destroy the system tray and free all resources */
void wm_systray_destroy(struct wm_systray *systray);

/* Position systray items at the given coordinates */
void wm_systray_layout(struct wm_systray *systray, int x, int y,
	int max_width, int height);

/* Get the total width needed for all tray icons */
int wm_systray_get_width(struct wm_systray *systray);

/*
 * Handle a click at (local_x, local_y), relative to the tray's top left
 * corner (where the systemtray tool is in the toolbar doesn't matter);
 * returns true if it was on an icon
 */
bool wm_systray_handle_click(struct wm_systray *systray,
	double local_x, double local_y, uint32_t button);

/* Call a method (Activate, ContextMenu...) of a tray item, at (x, y) */
/*
 * Something was sent on the tray's D-Bus connection: have the event loop
 * watch it for being writable if it did not all go.  (Other messages are
 * handled while the connection is read and need nothing.)
 */
void wm_systray_bus_kick(struct wm_systray *systray);

void wm_systray_call_item_method(struct wm_systray *systray,
	const char *bus_name, const char *object_path, const char *method,
	int x, int y);

/* The tooltip of the icon at layout coordinates (lx, ly), or NULL */
const char *wm_systray_tooltip_at(struct wm_systray *systray, double lx,
	double ly);

#endif /* WM_HAS_SYSTRAY */
#endif /* WM_SYSTRAY_H */
