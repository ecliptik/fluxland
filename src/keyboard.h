/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * keyboard.h - Keyboard input handling
 */

#ifndef WM_KEYBOARD_H
#define WM_KEYBOARD_H

#include <wayland-server-core.h>

struct wm_server;
struct wlr_keyboard;

/*
 * Physical keyboards are members of one wlr_keyboard_group (the seat
 * keyboard), so modifiers combine across them and they share the
 * configured keymap; their key events arrive through the group.
 * Virtual keyboards (wtype, on-screen keyboards) stay separate and keep
 * the keymap their client uploaded.
 */
struct wm_keyboard {
	struct wl_list link; /* wm_server.keyboards (not the group) */
	struct wm_server *server;
	struct wlr_keyboard *wlr_keyboard;
	bool is_virtual;
	bool is_group;

	struct wl_listener modifiers;
	struct wl_listener key;
	struct wl_listener destroy;
};

/* Set up a new keyboard device; virtual: from virtual-keyboard-v1 */
void wm_keyboard_setup(struct wm_server *server,
	struct wlr_input_device *device, bool is_virtual);

/* Create the keyboard group for physical keyboards (input init) */
void wm_keyboard_group_init(struct wm_server *server);
void wm_keyboard_group_finish(struct wm_server *server);

/* Switch to the next/previous XKB layout group on all keyboards */
void wm_keyboard_next_layout(struct wm_server *server);
void wm_keyboard_prev_layout(struct wm_server *server);

/* Recreate keymaps on all keyboards (e.g., after config reload) */
void wm_keyboard_apply_config(struct wm_server *server);

#endif /* WM_KEYBOARD_H */
