/*
 * wm-wayland - A Fluxbox-inspired Wayland compositor
 * session_lock.h - ext-session-lock-v1 protocol support
 *
 * Handles screen lockers (swaylock, waylock) by providing the
 * ext-session-lock-v1 protocol. When locked, all normal input
 * is blocked and only the lock surface receives keyboard events.
 */

#ifndef WM_SESSION_LOCK_H
#define WM_SESSION_LOCK_H

#include <stdbool.h>
#include <wayland-server-core.h>

struct wm_server;

struct wm_lock_surface {
	struct wlr_session_lock_surface_v1 *lock_surface;
	struct wlr_scene_tree *scene_tree;
	struct wlr_scene_tree *background; /* solid color behind surface */

	struct wl_listener map;
	struct wl_listener destroy;
	struct wl_listener surface_commit;
	struct wl_listener output_mode;

	struct wl_list link; /* wm_session_lock.lock_surfaces */
};

struct wm_session_lock {
	struct wm_server *server;
	struct wlr_session_lock_manager_v1 *manager;
	struct wlr_session_lock_v1 *active_lock;
	bool locked;

	struct wl_listener new_lock;
	struct wl_listener manager_destroy;

	/* Per-lock listeners (only active when locked) */
	struct wl_listener lock_new_surface;
	struct wl_listener lock_unlock;
	struct wl_listener lock_destroy;

	struct wl_list lock_surfaces; /* wm_lock_surface.link */
};

/*
 * Initialize session lock protocol support.
 * Creates the wlr_session_lock_manager_v1 global and sets up listeners.
 * Must be called during server init.
 */
void wm_session_lock_init(struct wm_server *server);

/*
 * Clean up session lock resources.
 */
void wm_session_lock_finish(struct wm_server *server);

/*
 * Check if the session is currently locked.
 * Used by input handlers to guard normal operations.
 */
bool wm_session_is_locked(struct wm_server *server);

#endif /* WM_SESSION_LOCK_H */
