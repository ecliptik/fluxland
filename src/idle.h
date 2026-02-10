/*
 * wm-wayland - A Fluxbox-inspired Wayland compositor
 * idle.h - Idle notification and idle inhibit protocol support
 *
 * Provides ext-idle-notify-v1 so idle managers (swayidle) receive
 * idle/resume events, and idle-inhibit-unstable-v1 so clients
 * (video players) can suppress idle timeouts.
 */

#ifndef WM_IDLE_H
#define WM_IDLE_H

#include <wayland-server-core.h>

struct wm_server;

struct wm_idle {
	struct wm_server *server;

	/* ext-idle-notify-v1: idle timeout notifications */
	struct wlr_idle_notifier_v1 *notifier;

	/* idle-inhibit-unstable-v1: clients can suppress idle */
	struct wlr_idle_inhibit_manager_v1 *inhibit_mgr;
	struct wl_listener new_inhibitor;
	struct wl_listener inhibit_mgr_destroy;
};

struct wm_idle_inhibitor {
	struct wlr_idle_inhibitor_v1 *inhibitor;
	struct wm_idle *idle;
	struct wl_listener destroy;
	struct wl_list link; /* wm_idle tracked list */
};

/*
 * Initialize idle notification and inhibit protocols.
 * Creates both globals and sets up inhibitor tracking.
 */
void wm_idle_init(struct wm_server *server);

/*
 * Clean up idle resources.
 */
void wm_idle_finish(struct wm_server *server);

/*
 * Notify user activity on the seat (call from input handlers).
 */
void wm_idle_notify_activity(struct wm_server *server);

#endif /* WM_IDLE_H */
