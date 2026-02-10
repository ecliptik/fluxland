/*
 * wm-wayland - A Fluxbox-inspired Wayland compositor
 * idle.c - Idle notification and idle inhibit protocol support
 *
 * Implements ext-idle-notify-v1 for swayidle-style idle managers
 * and idle-inhibit-unstable-v1 for video players to prevent idle.
 */

#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <wlr/types/wlr_idle_inhibit_v1.h>
#include <wlr/types/wlr_idle_notify_v1.h>
#include <wlr/util/log.h>

#include "idle.h"
#include "server.h"

static struct wl_list inhibitors; /* wm_idle_inhibitor.link */

static void
check_inhibit_state(struct wm_idle *idle)
{
	bool dominated = !wl_list_empty(&inhibitors);
	wlr_idle_notifier_v1_set_inhibited(idle->notifier, dominated);
}

static void
handle_inhibitor_destroy(struct wl_listener *listener, void *data)
{
	struct wm_idle_inhibitor *inh =
		wl_container_of(listener, inh, destroy);

	wlr_log(WLR_DEBUG, "%s", "idle inhibitor destroyed");

	wl_list_remove(&inh->destroy.link);
	wl_list_remove(&inh->link);
	struct wm_idle *idle = inh->idle;
	free(inh);

	check_inhibit_state(idle);
}

static void
handle_new_inhibitor(struct wl_listener *listener, void *data)
{
	struct wm_idle *idle =
		wl_container_of(listener, idle, new_inhibitor);
	struct wlr_idle_inhibitor_v1 *wlr_inhibitor = data;

	struct wm_idle_inhibitor *inh = calloc(1, sizeof(*inh));
	if (!inh) {
		wlr_log(WLR_ERROR, "%s", "failed to allocate idle inhibitor");
		return;
	}

	inh->inhibitor = wlr_inhibitor;
	inh->idle = idle;

	inh->destroy.notify = handle_inhibitor_destroy;
	wl_signal_add(&wlr_inhibitor->events.destroy, &inh->destroy);

	wl_list_insert(&inhibitors, &inh->link);

	wlr_log(WLR_DEBUG, "%s", "new idle inhibitor created");
	check_inhibit_state(idle);
}

static void
handle_inhibit_mgr_destroy(struct wl_listener *listener, void *data)
{
	struct wm_idle *idle =
		wl_container_of(listener, idle, inhibit_mgr_destroy);

	wl_list_remove(&idle->new_inhibitor.link);
	wl_list_remove(&idle->inhibit_mgr_destroy.link);
	idle->inhibit_mgr = NULL;
}

void
wm_idle_init(struct wm_server *server)
{
	struct wm_idle *idle = &server->idle;
	idle->server = server;

	wl_list_init(&inhibitors);

	/* ext-idle-notify-v1 */
	idle->notifier = wlr_idle_notifier_v1_create(server->wl_display);
	if (!idle->notifier) {
		wlr_log(WLR_ERROR, "%s", "failed to create idle notifier");
		return;
	}
	wlr_log(WLR_INFO, "%s", "idle notifier (ext-idle-notify-v1) created");

	/* idle-inhibit-unstable-v1 */
	idle->inhibit_mgr = wlr_idle_inhibit_v1_create(server->wl_display);
	if (!idle->inhibit_mgr) {
		wlr_log(WLR_ERROR, "%s", "failed to create idle inhibit manager");
		return;
	}

	idle->new_inhibitor.notify = handle_new_inhibitor;
	wl_signal_add(&idle->inhibit_mgr->events.new_inhibitor,
		&idle->new_inhibitor);

	idle->inhibit_mgr_destroy.notify = handle_inhibit_mgr_destroy;
	wl_signal_add(&idle->inhibit_mgr->events.destroy,
		&idle->inhibit_mgr_destroy);

	wlr_log(WLR_INFO, "%s", "idle inhibit manager (idle-inhibit-v1) created");
}

void
wm_idle_finish(struct wm_server *server)
{
	struct wm_idle *idle = &server->idle;

	/* Clean up any remaining tracked inhibitors */
	struct wm_idle_inhibitor *inh, *tmp;
	wl_list_for_each_safe(inh, tmp, &inhibitors, link) {
		wl_list_remove(&inh->destroy.link);
		wl_list_remove(&inh->link);
		free(inh);
	}

	if (idle->inhibit_mgr) {
		wl_list_remove(&idle->new_inhibitor.link);
		wl_list_remove(&idle->inhibit_mgr_destroy.link);
	}
}

void
wm_idle_notify_activity(struct wm_server *server)
{
	if (server->idle.notifier && server->seat) {
		wlr_idle_notifier_v1_notify_activity(
			server->idle.notifier, server->seat);
	}
}
