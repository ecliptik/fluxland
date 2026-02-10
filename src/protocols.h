/*
 * wm-wayland - A Fluxbox-inspired Wayland compositor
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

#endif /* WM_PROTOCOLS_H */
