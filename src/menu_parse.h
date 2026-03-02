/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * menu_parse.h - Menu file parser internal API
 *
 * Internal allocation functions shared between menu_parse.c and menu.c.
 */

#ifndef WM_MENU_PARSE_H
#define WM_MENU_PARSE_H

#include "menu.h"

/*
 * Create a new menu with the given title.
 * Returns NULL on allocation failure.
 */
struct wm_menu *menu_create(struct wm_server *server, const char *title);

/*
 * Create a new menu item with the given type and label.
 * Returns NULL on allocation failure.
 */
struct wm_menu_item *menu_item_create(enum wm_menu_item_type type,
	const char *label);

/*
 * Destroy a menu item, freeing its label, command, icon, and submenu.
 */
void menu_item_destroy(struct wm_menu_item *item);

#endif /* WM_MENU_PARSE_H */
