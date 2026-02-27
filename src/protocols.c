/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * protocols.c - Primary selection, pointer constraints, relative pointer,
 *               XDG activation, tearing control, output power management,
 *               keyboard shortcuts inhibit
 */

#define _POSIX_C_SOURCE 200809L
#include <wlr/types/wlr_alpha_modifier_v1.h>
#include <wlr/types/wlr_content_type_v1.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_cursor_shape_v1.h>
#include <wlr/types/wlr_data_control_v1.h>
#include <wlr/types/wlr_ext_foreign_toplevel_list_v1.h>
#include <wlr/types/wlr_keyboard_shortcuts_inhibit_v1.h>
#include <wlr/types/wlr_linux_dmabuf_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_power_management_v1.h>
#include <wlr/types/wlr_pointer_constraints_v1.h>
#include <wlr/types/wlr_pointer_gestures_v1.h>
#include <wlr/types/wlr_primary_selection.h>
#include <wlr/types/wlr_primary_selection_v1.h>
#include <wlr/types/wlr_relative_pointer_v1.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_security_context_v1.h>
#include <wlr/types/wlr_tearing_control_v1.h>
#include <wlr/types/wlr_virtual_keyboard_v1.h>
#include <wlr/types/wlr_virtual_pointer_v1.h>
#include <wlr/types/wlr_xdg_activation_v1.h>
#include <wlr/types/wlr_xdg_foreign_registry.h>
#include <wlr/types/wlr_xdg_foreign_v1.h>
#include <wlr/types/wlr_xdg_foreign_v2.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/util/log.h>

#include "protocols.h"
#include "server.h"
#include "view.h"
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
	wlr_log(WLR_INFO, "%s", "new virtual keyboard");
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
	wlr_log(WLR_INFO, "%s", "new virtual pointer");
}

/* --- XDG activation --- */

static void
handle_xdg_activation_request(struct wl_listener *listener, void *data)
{
	struct wm_server *server = wl_container_of(listener, server,
		xdg_activation_request);
	struct wlr_xdg_activation_v1_request_activate_event *event = data;

	/*
	 * Focus-stealing prevention: only honor activation if the
	 * requesting token has a valid seat (i.e., originated from an
	 * app that had user interaction).
	 */
	if (!event->token->seat) {
		wlr_log(WLR_DEBUG,
			"%s", "xdg-activation: denied request without seat");
		return;
	}

	struct wlr_xdg_toplevel *toplevel =
		wlr_xdg_toplevel_try_from_wlr_surface(event->surface);
	if (!toplevel) {
		return;
	}

	/* Find the wm_view for this toplevel */
	struct wm_view *view;
	wl_list_for_each(view, &server->views, link) {
		if (view->xdg_toplevel == toplevel) {
			wlr_log(WLR_INFO,
				"xdg-activation: focusing '%s'",
				view->title ? view->title : "(untitled)");
			wm_focus_view(view,
				toplevel->base->surface);
			return;
		}
	}
}

/* --- Tearing control --- */

static void
handle_tearing_set_hint(struct wl_listener *listener, void *data)
{
	(void)listener;
	struct wlr_tearing_control_v1 *control = data;

	wlr_log(WLR_DEBUG, "tearing control: surface hint = %d",
		control->current);
}

static void
handle_tearing_ctrl_destroy(struct wl_listener *listener, void *data)
{
	(void)data;
	wl_list_remove(&listener->link);
	/* listener is part of a heap allocation, free the container */
	struct wl_listener *set_hint_listener =
		(struct wl_listener *)((char *)listener -
		sizeof(struct wl_listener));
	wl_list_remove(&set_hint_listener->link);
	free(set_hint_listener);
}

static void
handle_tearing_new_object(struct wl_listener *listener, void *data)
{
	(void)listener;
	struct wlr_tearing_control_v1 *control = data;

	/*
	 * Allocate listeners for per-surface tearing hints.
	 * Two listeners packed together: [set_hint, destroy].
	 */
	struct wl_listener *listeners =
		calloc(2, sizeof(struct wl_listener));
	if (!listeners) {
		return;
	}

	listeners[0].notify = handle_tearing_set_hint;
	wl_signal_add(&control->events.set_hint, &listeners[0]);

	listeners[1].notify = handle_tearing_ctrl_destroy;
	wl_signal_add(&control->events.destroy, &listeners[1]);

	wlr_log(WLR_INFO, "%s", "tearing control: new object for surface");
}

/* --- Output power management --- */

static void
handle_output_power_set_mode(struct wl_listener *listener, void *data)
{
	(void)listener;
	struct wlr_output_power_v1_set_mode_event *event = data;
	struct wlr_output *output = event->output;

	struct wlr_output_state state;
	wlr_output_state_init(&state);

	switch (event->mode) {
	case ZWLR_OUTPUT_POWER_V1_MODE_ON:
		wlr_log(WLR_INFO, "output power: enabling '%s'",
			output->name ? output->name : "?");
		wlr_output_state_set_enabled(&state, true);
		break;
	case ZWLR_OUTPUT_POWER_V1_MODE_OFF:
		wlr_log(WLR_INFO, "output power: disabling '%s'",
			output->name ? output->name : "?");
		wlr_output_state_set_enabled(&state, false);
		break;
	}

	wlr_output_commit_state(output, &state);
	wlr_output_state_finish(&state);
}

/* --- Keyboard shortcuts inhibit --- */

static void
handle_kb_inhibitor_destroy(struct wl_listener *listener, void *data)
{
	struct wm_server *server = wl_container_of(listener, server,
		kb_inhibitor_destroy);

	server->active_kb_inhibitor = NULL;
	wl_list_remove(&server->kb_inhibitor_destroy.link);
	wl_list_init(&server->kb_inhibitor_destroy.link);
	wlr_log(WLR_INFO, "%s", "keyboard shortcuts inhibitor destroyed");
}

static void
handle_new_kb_shortcuts_inhibitor(struct wl_listener *listener, void *data)
{
	struct wm_server *server = wl_container_of(listener, server,
		new_kb_shortcuts_inhibitor);
	struct wlr_keyboard_shortcuts_inhibitor_v1 *inhibitor = data;

	/*
	 * If the inhibited surface already has keyboard focus,
	 * activate the inhibitor immediately.
	 */
	struct wlr_surface *focused =
		server->seat->keyboard_state.focused_surface;
	if (focused == inhibitor->surface) {
		if (server->active_kb_inhibitor) {
			wlr_keyboard_shortcuts_inhibitor_v1_deactivate(
				server->active_kb_inhibitor);
		}
		server->active_kb_inhibitor = inhibitor;
		wlr_keyboard_shortcuts_inhibitor_v1_activate(inhibitor);

		wl_list_remove(&server->kb_inhibitor_destroy.link);
		server->kb_inhibitor_destroy.notify =
			handle_kb_inhibitor_destroy;
		wl_signal_add(&inhibitor->events.destroy,
			&server->kb_inhibitor_destroy);

		wlr_log(WLR_INFO,
			"%s", "keyboard shortcuts inhibitor activated");
	}
}

/* --- Public API --- */

void
wm_protocols_update_kb_shortcuts_inhibitor(struct wm_server *server,
	struct wlr_surface *new_focus)
{
	/* Deactivate old inhibitor */
	if (server->active_kb_inhibitor) {
		if (!new_focus ||
		    server->active_kb_inhibitor->surface != new_focus) {
			wlr_keyboard_shortcuts_inhibitor_v1_deactivate(
				server->active_kb_inhibitor);
			server->active_kb_inhibitor = NULL;
			wl_list_remove(
				&server->kb_inhibitor_destroy.link);
			wl_list_init(
				&server->kb_inhibitor_destroy.link);
		}
	}

	/* Activate inhibitor if one exists for the new focused surface */
	if (new_focus && server->kb_shortcuts_inhibit_mgr) {
		struct wlr_keyboard_shortcuts_inhibitor_v1 *inhibitor;
		wl_list_for_each(inhibitor,
		    &server->kb_shortcuts_inhibit_mgr->inhibitors, link) {
			if (inhibitor->surface == new_focus &&
			    inhibitor->seat == server->seat) {
				server->active_kb_inhibitor = inhibitor;
				wlr_keyboard_shortcuts_inhibitor_v1_activate(
					inhibitor);

				wl_list_remove(
					&server->kb_inhibitor_destroy.link);
				server->kb_inhibitor_destroy.notify =
					handle_kb_inhibitor_destroy;
				wl_signal_add(&inhibitor->events.destroy,
					&server->kb_inhibitor_destroy);
				break;
			}
		}
	}
}

bool
wm_protocols_kb_shortcuts_inhibited(struct wm_server *server)
{
	if (!server->active_kb_inhibitor)
		return false;

	struct wlr_surface *focused =
		server->seat->keyboard_state.focused_surface;
	return (focused && focused == server->active_kb_inhibitor->surface &&
		server->active_kb_inhibitor->active);
}

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

	/* Pointer gestures (touchpad swipe/pinch/hold) */
	server->pointer_gestures =
		wlr_pointer_gestures_v1_create(server->wl_display);

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

	/* XDG activation (app launch focus tokens) */
	server->xdg_activation =
		wlr_xdg_activation_v1_create(server->wl_display);

	server->xdg_activation_request.notify =
		handle_xdg_activation_request;
	wl_signal_add(&server->xdg_activation->events.request_activate,
		&server->xdg_activation_request);

	/* Tearing control (variable refresh rate for games) */
	server->tearing_control_mgr =
		wlr_tearing_control_manager_v1_create(
			server->wl_display, 1);

	server->tearing_new_object.notify = handle_tearing_new_object;
	wl_signal_add(
		&server->tearing_control_mgr->events.new_object,
		&server->tearing_new_object);

	/* Output power management (display on/off for swayidle) */
	server->output_power_mgr =
		wlr_output_power_manager_v1_create(server->wl_display);

	server->output_power_set_mode.notify =
		handle_output_power_set_mode;
	wl_signal_add(&server->output_power_mgr->events.set_mode,
		&server->output_power_set_mode);

	/* Keyboard shortcuts inhibit (for games, remote desktops) */
	server->kb_shortcuts_inhibit_mgr =
		wlr_keyboard_shortcuts_inhibit_v1_create(
			server->wl_display);
	server->active_kb_inhibitor = NULL;

	server->new_kb_shortcuts_inhibitor.notify =
		handle_new_kb_shortcuts_inhibitor;
	wl_signal_add(
		&server->kb_shortcuts_inhibit_mgr->events.new_inhibitor,
		&server->new_kb_shortcuts_inhibitor);

	wl_list_init(&server->kb_inhibitor_destroy.link);

	/* Content type hints (wp-content-type-v1) */
	server->content_type_mgr =
		wlr_content_type_manager_v1_create(server->wl_display, 1);

	/* Alpha modifier (wp-alpha-modifier-v1 for per-surface opacity) */
	server->alpha_modifier =
		wlr_alpha_modifier_v1_create(server->wl_display);

	/* Security context (wp-security-context-v1 for sandboxed clients) */
	server->security_context_mgr =
		wlr_security_context_manager_v1_create(server->wl_display);

	/* Linux DMA-BUF (linux-dmabuf-v1 for GPU buffer sharing) */
	server->linux_dmabuf =
		wlr_linux_dmabuf_v1_create_with_renderer(
			server->wl_display, 4, server->renderer);

	/* XDG output (xdg-output-v1 for logical output info) */
	server->xdg_output_mgr =
		wlr_xdg_output_manager_v1_create(
			server->wl_display, server->output_layout);

	/* XDG foreign (xdg-foreign-v1/v2 for cross-app surface sharing) */
	server->xdg_foreign_registry =
		wlr_xdg_foreign_registry_create(server->wl_display);
	server->xdg_foreign_v1 =
		wlr_xdg_foreign_v1_create(
			server->wl_display, server->xdg_foreign_registry);
	server->xdg_foreign_v2 =
		wlr_xdg_foreign_v2_create(
			server->wl_display, server->xdg_foreign_registry);

	/* Ext foreign toplevel list (ext-foreign-toplevel-list-v1) */
	server->ext_foreign_toplevel_list =
		wlr_ext_foreign_toplevel_list_v1_create(
			server->wl_display, 1);

	wlr_log(WLR_INFO, "%s",
		"initialized primary selection, pointer constraints, "
		"relative pointer, pointer gestures, cursor shape, "
		"virtual keyboard, virtual pointer, data control, "
		"xdg activation, tearing control, output power management, "
		"keyboard shortcuts inhibit, content type, alpha modifier, "
		"security context, linux dmabuf, xdg output, "
		"xdg foreign v1/v2, ext foreign toplevel list protocols");
}

void
wm_protocols_finish(struct wm_server *server)
{
	wl_list_remove(&server->kb_inhibitor_destroy.link);
	wl_list_remove(&server->new_kb_shortcuts_inhibitor.link);
	wl_list_remove(&server->output_power_set_mode.link);
	wl_list_remove(&server->tearing_new_object.link);
	wl_list_remove(&server->xdg_activation_request.link);
	wl_list_remove(&server->new_virtual_pointer.link);
	wl_list_remove(&server->new_virtual_keyboard.link);
	wl_list_remove(&server->cursor_shape_request.link);
	wl_list_remove(&server->request_set_primary_selection.link);
	wl_list_remove(&server->new_pointer_constraint.link);
	wl_list_remove(&server->pointer_constraint_destroy.link);
}
