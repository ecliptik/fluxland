/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * foreign_toplevel.c - wlr-foreign-toplevel-management-v1 for external taskbars
 *
 * Provides the wlr-foreign-toplevel-management protocol so that external
 * taskbar clients (waybar, sfwbar, etc.) can list windows, see their state,
 * and control them (activate, close, maximize, minimize, fullscreen).
 */

#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <wlr/types/wlr_foreign_toplevel_management_v1.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>

#include "foreign_toplevel.h"
#include "server.h"
#include "view.h"

/* --- Request handlers (taskbar → compositor) --- */

static void
handle_request_activate(struct wl_listener *listener, void *data)
{
	struct wm_view *view =
		wl_container_of(listener, view, foreign_toplevel_request_activate);
	wm_focus_view(view, view->xdg_toplevel->base->surface);
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

	/*
	 * Reuse the XDG toplevel maximize handler signal. Firing the request
	 * handler toggles the state since it checks view->maximized.
	 */
	wl_signal_emit_mutable(
		&view->xdg_toplevel->events.request_maximize, NULL);
}

static void
handle_request_minimize(struct wl_listener *listener, void *data)
{
	struct wm_view *view =
		wl_container_of(listener, view, foreign_toplevel_request_minimize);
	struct wlr_foreign_toplevel_handle_v1_minimized_event *event = data;

	if (event->minimized) {
		/* Minimize: hide the view and focus next */
		wlr_scene_node_set_enabled(&view->scene_tree->node, false);
		wlr_foreign_toplevel_handle_v1_set_minimized(
			view->foreign_toplevel_handle, true);

		if (view == view->server->focused_view) {
			view->server->focused_view = NULL;
			struct wm_view *next;
			wl_list_for_each(next, &view->server->views, link) {
				if (next != view &&
				    next->xdg_toplevel->base->surface) {
					wm_focus_view(next,
						next->xdg_toplevel->base->surface);
					return;
				}
			}
			wlr_seat_keyboard_notify_clear_focus(
				view->server->seat);
		}
	} else {
		/* Unminimize: show the view and focus it */
		wlr_scene_node_set_enabled(&view->scene_tree->node, true);
		wlr_foreign_toplevel_handle_v1_set_minimized(
			view->foreign_toplevel_handle, false);
		wm_focus_view(view, view->xdg_toplevel->base->surface);
	}
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

	wl_signal_emit_mutable(
		&view->xdg_toplevel->events.request_fullscreen, NULL);
}

static void
handle_request_close(struct wl_listener *listener, void *data)
{
	struct wm_view *view =
		wl_container_of(listener, view, foreign_toplevel_request_close);
	wlr_xdg_toplevel_send_close(view->xdg_toplevel);
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

	/* Set initial output */
	struct wlr_output *output = wlr_output_layout_output_at(
		server->output_layout,
		view->x + 1, view->y + 1);
	if (output) {
		wlr_foreign_toplevel_handle_v1_output_enter(handle, output);
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

