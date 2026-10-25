/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * cursor.h - Pointer/cursor input handling
 */

#ifndef WM_CURSOR_H
#define WM_CURSOR_H

#include <stdint.h>

struct wm_server;
struct wm_view;

/* Register cursor event listeners on server->cursor */
void wm_cursor_init(struct wm_server *server);

/* Remove cursor listeners */
void wm_cursor_finish(struct wm_server *server);

/*
 * Run the keys file's binding for one of Fluxbox's events (event is an
 * enum wm_mouse_event: FocusIn, FocusOut, MouseOver, MouseOut) in
 * context, on view (NULL: the focused window).
 */
void wm_cursor_run_event(struct wm_server *server, uint32_t context,
	int event, struct wm_view *view);

#endif /* WM_CURSOR_H */
