/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * protocols.h - Primary selection, pointer constraints, relative pointer
 */

#ifndef WM_PROTOCOLS_H
#define WM_PROTOCOLS_H

#include <stdbool.h>

struct wm_server;
struct wlr_surface;

/* Initialize primary selection, pointer constraints, relative pointer */
void wm_protocols_init(struct wm_server *server);

/* Clean up protocol listeners */
void wm_protocols_finish(struct wm_server *server);

/*
 * Check and activate/deactivate pointer constraint when focus changes.
 * Call this whenever keyboard focus moves to a new surface.
 */
void wm_protocols_update_pointer_constraint(struct wm_server *server,
	struct wlr_surface *new_focus);

/*
 * Check and activate/deactivate keyboard shortcuts inhibitor when
 * focus changes. Call this whenever keyboard focus moves to a new surface.
 */
void wm_protocols_update_kb_shortcuts_inhibitor(struct wm_server *server,
	struct wlr_surface *new_focus);

/*
 * Check if keyboard shortcuts are currently inhibited for the
 * focused surface (e.g. for games or remote desktop apps).
 */
bool wm_protocols_kb_shortcuts_inhibited(struct wm_server *server);

/*
 * An xdg-activation token of the user's input, for something the compositor
 * hands a client when the user acts on it (a tray icon's item): bound to the
 * seat, as a client's token from an input event is, so whoever shows it asks
 * for activation with the user's say-so.  Its name, or NULL; it expires
 * unused after the activation's timeout.
 */
const char *wm_activation_token_create(struct wm_server *server);

/*
 * An X11 client names its token in _NET_STARTUP_ID before asking for
 * activation (_NET_ACTIVE_WINDOW): true if name is a token of the user's
 * input (the seat's).  A token is good for one use, so it is gone after.
 */
bool wm_activation_token_consume(struct wm_server *server, const char *name);

#endif /* WM_PROTOCOLS_H */
