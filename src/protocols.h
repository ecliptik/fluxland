/*
 * wm-wayland - A Fluxbox-inspired Wayland compositor
 * protocols.h - Primary selection, pointer constraints, relative pointer
 */

#ifndef WM_PROTOCOLS_H
#define WM_PROTOCOLS_H

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

#endif /* WM_PROTOCOLS_H */
