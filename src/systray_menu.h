/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * systray_menu.h - A tray icon's menu (com.canonical.dbusmenu)
 *
 * Applications that put a menu on their tray icon (Steam, Plex, Blueman)
 * don't draw it: they publish its entries over D-Bus and expect the tray
 * to show them.  The entries are fetched with GetLayout, shown as one of
 * fluxland's own menus, and a chosen entry is sent back as an Event.
 */

#ifndef WM_SYSTRAY_MENU_H
#define WM_SYSTRAY_MENU_H

#ifdef WM_HAS_SYSTRAY

struct wm_server;
struct wm_systray;

/*
 * Fetch the dbusmenu at menu_path of the application at bus_name and show
 * it at (x, y).  The menu is titled title.  When it can't be had (the
 * application is gone, or it has no entries) the item's ContextMenu
 * method, at item_path, is called instead.
 */
void wm_systray_menu_open(struct wm_systray *systray, const char *bus_name,
	const char *item_path, const char *menu_path, const char *title,
	int x, int y);

/* An entry of such a menu was chosen: send its click (spec is the menu
 * item's command) */
void wm_systray_menu_activate(struct wm_server *server, const char *spec);

/* The tray goes: forget the menus still being fetched */
void wm_systray_menu_cancel_all(struct wm_systray *systray);

#endif /* WM_HAS_SYSTRAY */
#endif /* WM_SYSTRAY_MENU_H */
