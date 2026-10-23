/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * foreign_toplevel.c - wlr-foreign-toplevel-management-v1 for external taskbars
 *
 * Provides the wlr-foreign-toplevel-management protocol so that external
 * taskbar clients (waybar, sfwbar, etc.) can list windows, see their state,
 * and control them (activate, close, maximize, minimize, fullscreen).
 */

#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <wlr/types/wlr_ext_foreign_toplevel_list_v1.h>
#include <wlr/types/wlr_foreign_toplevel_management_v1.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>

#include "foreign_toplevel.h"
#include "output.h"
#include "server.h"
#include "view.h"

/* --- Request handlers (taskbar → compositor) --- */

static void
handle_request_activate(struct wl_listener *listener, void *data)
{
	struct wm_view *view =
		wl_container_of(listener, view, foreign_toplevel_request_activate);
	wm_focus_view(view, wm_view_surface(view));
}

static void
handle_request_maximize(struct wl_listener *listener, void *data)
{
	struct wm_view *view =
		wl_container_of(listener, view, foreign_toplevel_request_maximize);
	struct wlr_foreign_toplevel_handle_v1_maximized_event *event = data;

	if (event->maximized == view->maximized) {
		return;
	}

	wm_view_toggle_maximize(view);
}

static void
handle_request_minimize(struct wl_listener *listener, void *data)
{
	struct wm_view *view =
		wl_container_of(listener, view, foreign_toplevel_request_minimize);
	struct wlr_foreign_toplevel_handle_v1_minimized_event *event = data;

	if (event->minimized)
		wm_view_minimize(view);
	else
		wm_view_unminimize(view);
}

static void
handle_request_fullscreen(struct wl_listener *listener, void *data)
{
	struct wm_view *view =
		wl_container_of(listener, view, foreign_toplevel_request_fullscreen);
	struct wlr_foreign_toplevel_handle_v1_fullscreen_event *event = data;

	if (event->fullscreen == view->fullscreen) {
		return;
	}

	wm_view_toggle_fullscreen(view);
}

static void
handle_request_close(struct wl_listener *listener, void *data)
{
	struct wm_view *view =
		wl_container_of(listener, view, foreign_toplevel_request_close);
	wm_view_close(view);
}

/* --- Public API --- */

void
wm_foreign_toplevel_init(struct wm_server *server)
{
	server->foreign_toplevel_manager =
		wlr_foreign_toplevel_manager_v1_create(server->wl_display);
	if (!server->foreign_toplevel_manager) {
		wlr_log(WLR_ERROR, "%s", "failed to create foreign toplevel manager");
	}
}

/* Send output_enter/leave if the window moved to another output */
static void
sync_output(struct wm_view *view)
{
	struct wlr_box box;
	wm_view_get_geometry(view, &box);
	if (view->foreign_output && wlr_box_equal(&box, &view->foreign_box))
		return;
	view->foreign_box = box;

	struct wm_output *out = wm_output_for_view(view);
	struct wlr_output *o = out ? out->wlr_output : NULL;
	if (o == view->foreign_output)
		return;
	if (view->foreign_output)
		wlr_foreign_toplevel_handle_v1_output_leave(
			view->foreign_toplevel_handle, view->foreign_output);
	if (o)
		wlr_foreign_toplevel_handle_v1_output_enter(
			view->foreign_toplevel_handle, o);
	view->foreign_output = o;
}

void
wm_foreign_toplevel_handle_create(struct wm_view *view)
{
	struct wm_server *server = view->server;
	if (!server->foreign_toplevel_manager) {
		return;
	}

	struct wlr_foreign_toplevel_handle_v1 *handle =
		wlr_foreign_toplevel_handle_v1_create(
			server->foreign_toplevel_manager);
	if (!handle) {
		wlr_log(WLR_ERROR,
			"%s", "failed to create foreign toplevel handle");
		return;
	}

	view->foreign_toplevel_handle = handle;

	/* Set initial metadata */
	if (view->title) {
		wlr_foreign_toplevel_handle_v1_set_title(handle,
			view->title);
	}
	if (view->app_id) {
		wlr_foreign_toplevel_handle_v1_set_app_id(handle,
			view->app_id);
	}

	/* Set initial state */
	wlr_foreign_toplevel_handle_v1_set_maximized(handle,
		view->maximized);
	wlr_foreign_toplevel_handle_v1_set_fullscreen(handle,
		view->fullscreen);
	wlr_foreign_toplevel_handle_v1_set_activated(handle,
		view == server->focused_view);

	/* The output it is on (kept up to date by sync_outputs) */
	view->foreign_output = NULL;
	view->foreign_box = (struct wlr_box){0};
	sync_output(view);

	/* ext-foreign-toplevel-list: the read-only window list */
	if (server->ext_foreign_toplevel_list) {
		struct wlr_ext_foreign_toplevel_handle_v1_state state = {
			.title = view->title ? view->title : "",
			.app_id = view->app_id ? view->app_id : "",
		};
		view->ext_toplevel_handle =
			wlr_ext_foreign_toplevel_handle_v1_create(
				server->ext_foreign_toplevel_list, &state);
	}

	/* Listen for taskbar requests */
	view->foreign_toplevel_request_activate.notify =
		handle_request_activate;
	wl_signal_add(&handle->events.request_activate,
		&view->foreign_toplevel_request_activate);

	view->foreign_toplevel_request_maximize.notify =
		handle_request_maximize;
	wl_signal_add(&handle->events.request_maximize,
		&view->foreign_toplevel_request_maximize);

	view->foreign_toplevel_request_minimize.notify =
		handle_request_minimize;
	wl_signal_add(&handle->events.request_minimize,
		&view->foreign_toplevel_request_minimize);

	view->foreign_toplevel_request_fullscreen.notify =
		handle_request_fullscreen;
	wl_signal_add(&handle->events.request_fullscreen,
		&view->foreign_toplevel_request_fullscreen);

	view->foreign_toplevel_request_close.notify =
		handle_request_close;
	wl_signal_add(&handle->events.request_close,
		&view->foreign_toplevel_request_close);

	wlr_log(WLR_DEBUG, "foreign toplevel handle created for %s",
		view->title ? view->title : "(untitled)");
}

void
wm_foreign_toplevel_handle_destroy(struct wm_view *view)
{
	if (!view->foreign_toplevel_handle) {
		return;
	}

	wl_list_remove(&view->foreign_toplevel_request_activate.link);
	wl_list_remove(&view->foreign_toplevel_request_maximize.link);
	wl_list_remove(&view->foreign_toplevel_request_minimize.link);
	wl_list_remove(&view->foreign_toplevel_request_fullscreen.link);
	wl_list_remove(&view->foreign_toplevel_request_close.link);

	wlr_foreign_toplevel_handle_v1_destroy(
		view->foreign_toplevel_handle);
	view->foreign_toplevel_handle = NULL;
	view->foreign_output = NULL;

	if (view->ext_toplevel_handle) {
		wlr_ext_foreign_toplevel_handle_v1_destroy(
			view->ext_toplevel_handle);
		view->ext_toplevel_handle = NULL;
	}
}

/* Title and app_id of the ext-foreign-toplevel-list handle */
static void
update_ext_state(struct wm_view *view)
{
	if (!view->ext_toplevel_handle)
		return;
	struct wlr_ext_foreign_toplevel_handle_v1_state state = {
		.title = view->title ? view->title : "",
		.app_id = view->app_id ? view->app_id : "",
	};
	wlr_ext_foreign_toplevel_handle_v1_update_state(
		view->ext_toplevel_handle, &state);
}

void
wm_foreign_toplevel_sync_outputs(struct wm_server *server)
{
	struct wm_view *view;
	wl_list_for_each(view, &server->views, link) {
		if (view->foreign_toplevel_handle)
			sync_output(view);
	}
}

void
wm_foreign_toplevel_output_destroyed(struct wm_server *server,
	struct wlr_output *output)
{
	/* wlroots drops the output from the handles itself */
	struct wm_view *view;
	wl_list_for_each(view, &server->views, link) {
		if (view->foreign_output == output)
			view->foreign_output = NULL;
	}
}

void
wm_foreign_toplevel_update_title(struct wm_view *view)
{
	if (!view->foreign_toplevel_handle) {
		return;
	}
	wlr_foreign_toplevel_handle_v1_set_title(
		view->foreign_toplevel_handle,
		view->title ? view->title : "");
	update_ext_state(view);
}

void
wm_foreign_toplevel_update_app_id(struct wm_view *view)
{
	if (!view->foreign_toplevel_handle) {
		return;
	}
	wlr_foreign_toplevel_handle_v1_set_app_id(
		view->foreign_toplevel_handle,
		view->app_id ? view->app_id : "");
	update_ext_state(view);
}

void
wm_foreign_toplevel_set_activated(struct wm_view *view, bool activated)
{
	if (!view->foreign_toplevel_handle) {
		return;
	}
	wlr_foreign_toplevel_handle_v1_set_activated(
		view->foreign_toplevel_handle, activated);
}

void
wm_foreign_toplevel_set_maximized(struct wm_view *view, bool maximized)
{
	if (!view->foreign_toplevel_handle) {
		return;
	}
	wlr_foreign_toplevel_handle_v1_set_maximized(
		view->foreign_toplevel_handle, maximized);
}

void
wm_foreign_toplevel_set_fullscreen(struct wm_view *view, bool fullscreen)
{
	if (!view->foreign_toplevel_handle) {
		return;
	}
	wlr_foreign_toplevel_handle_v1_set_fullscreen(
		view->foreign_toplevel_handle, fullscreen);
}

void
wm_foreign_toplevel_set_minimized(struct wm_view *view, bool minimized)
{
	if (!view->foreign_toplevel_handle) {
		return;
	}
	wlr_foreign_toplevel_handle_v1_set_minimized(
		view->foreign_toplevel_handle, minimized);
}

