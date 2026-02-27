/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * cursor.h - Pointer/cursor input handling
 */

#ifndef WM_CURSOR_H
#define WM_CURSOR_H

struct wm_server;

/* Register cursor event listeners on server->cursor */
void wm_cursor_init(struct wm_server *server);

/* Remove cursor listeners */
void wm_cursor_finish(struct wm_server *server);

#endif /* WM_CURSOR_H */
