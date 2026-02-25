/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * transient_seat.c - ext-transient-seat-v1 protocol support
 *
 * Allows clients (e.g. remote desktop tools) to create temporary seats
 * for additional input sources. Each transient seat gets a new wlr_seat.
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_transient_seat_v1.h>
#include <wlr/util/log.h>

#include "transient_seat.h"
#include "server.h"

static int transient_seat_counter = 0;

static void
handle_create_seat(struct wl_listener *listener, void *data)
{
	struct wm_server *server =
		wl_container_of(listener, server, transient_seat_create);
	struct wlr_transient_seat_v1 *tseat = data;

	/*
	 * Create a new wlr_seat for this transient seat request.
	 * Each transient seat gets a unique name.
	 */
	char name[64];
	snprintf(name, sizeof(name), "transient-seat-%d",
		transient_seat_counter++);

	struct wlr_seat *seat = wlr_seat_create(server->wl_display, name);
	if (!seat) {
		wlr_log(WLR_ERROR, "failed to create transient seat '%s'",
			name);
		wlr_transient_seat_v1_deny(tseat);
		return;
	}

	wlr_transient_seat_v1_ready(tseat, seat);
	wlr_log(WLR_INFO, "created transient seat '%s'", name);
}

void
wm_transient_seat_init(struct wm_server *server)
{
	server->transient_seat_mgr =
		wlr_transient_seat_manager_v1_create(server->wl_display);
	if (!server->transient_seat_mgr) {
		wlr_log(WLR_ERROR, "%s",
			"failed to create transient seat manager");
		return;
	}

	server->transient_seat_create.notify = handle_create_seat;
	wl_signal_add(
		&server->transient_seat_mgr->events.create_seat,
		&server->transient_seat_create);

	wlr_log(WLR_INFO, "%s",
		"initialized ext-transient-seat-v1 protocol");
}

void
wm_transient_seat_finish(struct wm_server *server)
{
	if (server->transient_seat_mgr) {
		wl_list_remove(&server->transient_seat_create.link);
	}
}
