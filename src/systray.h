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
#include <systemd/sd-bus.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_scene.h>

struct wm_server;

#define WM_SYSTRAY_ICON_SIZE 18
#define WM_SYSTRAY_ICON_PADDING 2

/* A single tray icon item */
struct wm_systray_item {
	char *bus_name;        /* D-Bus sender / unique name */
	char *object_path;     /* /StatusNotifierItem or custom path */
	char *id;              /* application identifier */
	char *icon_name;       /* named icon (freedesktop) */
	cairo_surface_t *icon_pixmap; /* rendered icon surface */
	struct wlr_scene_buffer *icon_buf;
	struct wm_systray *systray;
	struct wl_list link;   /* wm_systray.items */
};

/* System tray manager */
struct wm_systray {
	struct wm_server *server;
	sd_bus *bus;
	struct wl_event_source *bus_event;
	struct wl_list items;  /* wm_systray_item.link */
	int item_count;
	struct wlr_scene_tree *scene_tree;
	sd_bus_slot *watcher_slot;
	sd_bus_slot *name_slot;
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

/* Handle a click on the systray area; returns true if consumed */
bool wm_systray_handle_click(struct wm_systray *systray,
	double local_x, double local_y, uint32_t button);

#endif /* WM_HAS_SYSTRAY */
#endif /* WM_SYSTRAY_H */
