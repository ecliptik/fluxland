/*
 * wm-wayland - A Fluxbox-inspired Wayland compositor
 * protocols.c - Primary selection, pointer constraints, relative pointer
 */

#define _POSIX_C_SOURCE 200809L
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_cursor_shape_v1.h>
#include <wlr/types/wlr_data_control_v1.h>
#include <wlr/types/wlr_pointer_constraints_v1.h>
#include <wlr/types/wlr_primary_selection.h>
#include <wlr/types/wlr_primary_selection_v1.h>
#include <wlr/types/wlr_relative_pointer_v1.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_virtual_keyboard_v1.h>
#include <wlr/types/wlr_virtual_pointer_v1.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/util/log.h>

#include "protocols.h"
#include "server.h"
#include "keyboard.h"

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

/* --- Cursor shape --- */

static void
handle_cursor_shape_request(struct wl_listener *listener, void *data)
{
	struct wm_server *server = wl_container_of(listener, server,
		cursor_shape_request);
	struct wlr_cursor_shape_manager_v1_request_set_shape_event *event = data;

	/* Only handle pointer devices (not tablet tools) */
	if (event->device_type !=
	    WLR_CURSOR_SHAPE_MANAGER_V1_DEVICE_TYPE_POINTER) {
		return;
	}

	/* Verify the requesting client currently has pointer focus */
	struct wlr_seat_client *focused =
		server->seat->pointer_state.focused_client;
	if (focused != event->seat_client) {
		return;
	}

	const char *shape_name = wlr_cursor_shape_v1_name(event->shape);
	wlr_cursor_set_xcursor(server->cursor, server->cursor_mgr,
		shape_name);
}

/* --- Virtual keyboard --- */

static void
handle_new_virtual_keyboard(struct wl_listener *listener, void *data)
{
	struct wm_server *server = wl_container_of(listener, server,
		new_virtual_keyboard);
	struct wlr_virtual_keyboard_v1 *vkbd = data;

	/* Treat the virtual keyboard like any physical keyboard */
	wm_keyboard_setup(server, &vkbd->keyboard.base);
	wlr_log(WLR_INFO, "new virtual keyboard");
}

/* --- Virtual pointer --- */

static void
handle_new_virtual_pointer(struct wl_listener *listener, void *data)
{
	struct wm_server *server = wl_container_of(listener, server,
		new_virtual_pointer);
	struct wlr_virtual_pointer_v1_new_pointer_event *event = data;

	/* Attach the virtual pointer to the cursor like a physical pointer */
	wlr_cursor_attach_input_device(server->cursor,
		&event->new_pointer->pointer.base);
	wlr_log(WLR_INFO, "new virtual pointer");
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

	/* Cursor shape (wp-cursor-shape-v1) */
	server->cursor_shape_mgr =
		wlr_cursor_shape_manager_v1_create(server->wl_display, 1);

	server->cursor_shape_request.notify = handle_cursor_shape_request;
	wl_signal_add(&server->cursor_shape_mgr->events.request_set_shape,
		&server->cursor_shape_request);

	/* Virtual keyboard (virtual-keyboard-v1 for on-screen keyboards) */
	server->virtual_keyboard_mgr =
		wlr_virtual_keyboard_manager_v1_create(server->wl_display);

	server->new_virtual_keyboard.notify = handle_new_virtual_keyboard;
	wl_signal_add(
		&server->virtual_keyboard_mgr->events.new_virtual_keyboard,
		&server->new_virtual_keyboard);

	/* Virtual pointer (virtual-pointer-v1 for remote input/automation) */
	server->virtual_pointer_mgr =
		wlr_virtual_pointer_manager_v1_create(server->wl_display);

	server->new_virtual_pointer.notify = handle_new_virtual_pointer;
	wl_signal_add(
		&server->virtual_pointer_mgr->events.new_virtual_pointer,
		&server->new_virtual_pointer);

	/* Data control (wlr-data-control-v1 for clipboard managers) */
	server->data_control_mgr =
		wlr_data_control_manager_v1_create(server->wl_display);

	wlr_log(WLR_INFO, "%s",
		"initialized primary selection, pointer constraints, "
		"relative pointer, cursor shape, virtual keyboard, "
		"virtual pointer, data control protocols");
}

void
wm_protocols_finish(struct wm_server *server)
{
	wl_list_remove(&server->new_virtual_pointer.link);
	wl_list_remove(&server->new_virtual_keyboard.link);
	wl_list_remove(&server->cursor_shape_request.link);
	wl_list_remove(&server->request_set_primary_selection.link);
	wl_list_remove(&server->new_pointer_constraint.link);
	wl_list_remove(&server->pointer_constraint_destroy.link);
}
