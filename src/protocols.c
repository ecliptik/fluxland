/*
 * wm-wayland - A Fluxbox-inspired Wayland compositor
 * protocols.c - Primary selection, pointer constraints, relative pointer
 */

#define _POSIX_C_SOURCE 200809L
#include <wlr/types/wlr_pointer_constraints_v1.h>
#include <wlr/types/wlr_primary_selection.h>
#include <wlr/types/wlr_primary_selection_v1.h>
#include <wlr/types/wlr_relative_pointer_v1.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/util/log.h>

#include "protocols.h"
#include "server.h"

/* --- Primary selection --- */

static void
handle_request_set_primary_selection(struct wl_listener *listener, void *data)
{
	struct wm_server *server = wl_container_of(listener, server,
		request_set_primary_selection);
	struct wlr_seat_request_set_primary_selection_event *event = data;

	wlr_seat_set_primary_selection(server->seat, event->source,
		event->serial);
}

/* --- Pointer constraints --- */

static void
handle_constraint_destroy(struct wl_listener *listener, void *data)
{
	struct wm_server *server = wl_container_of(listener, server,
		pointer_constraint_destroy);

	server->active_constraint = NULL;
	wl_list_remove(&server->pointer_constraint_destroy.link);
	wl_list_init(&server->pointer_constraint_destroy.link);
}

static void
handle_new_constraint(struct wl_listener *listener, void *data)
{
	struct wm_server *server = wl_container_of(listener, server,
		new_pointer_constraint);
	struct wlr_pointer_constraint_v1 *constraint = data;

	/*
	 * If the constrained surface already has keyboard focus,
	 * activate the constraint immediately.
	 */
	struct wlr_surface *focused =
		server->seat->keyboard_state.focused_surface;
	if (focused == constraint->surface) {
		if (server->active_constraint) {
			wlr_pointer_constraint_v1_send_deactivated(
				server->active_constraint);
		}
		server->active_constraint = constraint;
		wlr_pointer_constraint_v1_send_activated(constraint);

		wl_list_remove(&server->pointer_constraint_destroy.link);
		server->pointer_constraint_destroy.notify =
			handle_constraint_destroy;
		wl_signal_add(&constraint->events.destroy,
			&server->pointer_constraint_destroy);
	}
}

/* --- Public API --- */

void
wm_protocols_update_pointer_constraint(struct wm_server *server,
	struct wlr_surface *new_focus)
{
	/* Deactivate old constraint */
	if (server->active_constraint) {
		if (!new_focus ||
		    server->active_constraint->surface != new_focus) {
			wlr_pointer_constraint_v1_send_deactivated(
				server->active_constraint);
			server->active_constraint = NULL;
			wl_list_remove(
				&server->pointer_constraint_destroy.link);
			wl_list_init(
				&server->pointer_constraint_destroy.link);
		}
	}

	/* Activate new constraint if one exists for the focused surface */
	if (new_focus && server->pointer_constraints) {
		struct wlr_pointer_constraint_v1 *constraint =
			wlr_pointer_constraints_v1_constraint_for_surface(
				server->pointer_constraints, new_focus,
				server->seat);
		if (constraint) {
			server->active_constraint = constraint;
			wlr_pointer_constraint_v1_send_activated(constraint);

			wl_list_remove(
				&server->pointer_constraint_destroy.link);
			server->pointer_constraint_destroy.notify =
				handle_constraint_destroy;
			wl_signal_add(&constraint->events.destroy,
				&server->pointer_constraint_destroy);
		}
	}
}

void
wm_protocols_init(struct wm_server *server)
{
	/* Primary selection (middle-click paste) */
	server->primary_selection_mgr =
		wlr_primary_selection_v1_device_manager_create(
			server->wl_display);

	server->request_set_primary_selection.notify =
		handle_request_set_primary_selection;
	wl_signal_add(&server->seat->events.request_set_primary_selection,
		&server->request_set_primary_selection);

	/* Pointer constraints (lock/confine for games) */
	server->pointer_constraints =
		wlr_pointer_constraints_v1_create(server->wl_display);
	server->active_constraint = NULL;

	server->new_pointer_constraint.notify = handle_new_constraint;
	wl_signal_add(&server->pointer_constraints->events.new_constraint,
		&server->new_pointer_constraint);

	wl_list_init(&server->pointer_constraint_destroy.link);

	/* Relative pointer (unaccelerated motion for games) */
	server->relative_pointer_mgr =
		wlr_relative_pointer_manager_v1_create(server->wl_display);

	wlr_log(WLR_INFO, "%s",
		"initialized primary selection, "
		"pointer constraints, relative pointer protocols");
}

void
wm_protocols_finish(struct wm_server *server)
{
	wl_list_remove(&server->request_set_primary_selection.link);
	wl_list_remove(&server->new_pointer_constraint.link);
	wl_list_remove(&server->pointer_constraint_destroy.link);
}
