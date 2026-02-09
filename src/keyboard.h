/*
 * wm-wayland - A Fluxbox-inspired Wayland compositor
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

#endif /* WM_KEYBOARD_H */
