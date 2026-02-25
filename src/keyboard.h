/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * keyboard.h - Keyboard input handling
 */

#ifndef WM_KEYBOARD_H
#define WM_KEYBOARD_H

#include <wayland-server-core.h>

struct wm_server;
struct wlr_keyboard;

struct wm_keyboard {
	struct wl_list link; /* wm_server.keyboards */
	struct wm_server *server;
	struct wlr_keyboard *wlr_keyboard;

	struct wl_listener modifiers;
	struct wl_listener key;
	struct wl_listener destroy;
};

/* Set up a new keyboard device */
void wm_keyboard_setup(struct wm_server *server,
	struct wlr_input_device *device);

/* Switch to the next/previous XKB layout group on all keyboards */
void wm_keyboard_next_layout(struct wm_server *server);
void wm_keyboard_prev_layout(struct wm_server *server);

/* Recreate keymaps on all keyboards (e.g., after config reload) */
void wm_keyboard_apply_config(struct wm_server *server);

#endif /* WM_KEYBOARD_H */
