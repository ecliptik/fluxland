/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * view_xdg.c - xdg-shell backend for views
 *
 * Wayland toplevels (xdg_toplevel) as wm_views: the backend operations
 * (wm_xdg_view_impl), the toplevel event handlers, and popups. The
 * window management they share with X11 windows lives in view.c.
 */

#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <string.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>

#include "decoration.h"
#include "menu.h"
#include "rules.h"
#include "server.h"
#include "slit.h"
#include "view.h"

/* --- xdg-shell backend --- */

static struct wlr_surface *
xdg_view_surface(struct wm_view *view)
{
	return view->xdg_toplevel ? view->xdg_toplevel->base->surface : NULL;
}

static void
xdg_view_content_geometry(struct wm_view *view, struct wlr_box *geo)
{
	*geo = view->xdg_toplevel->base->geometry;
}

/*
 * A configure can go to the client: not before its first commit, nor
 * once it has unmapped (wlroots marks the surface uninitialized then,
 * and asserts) -- a window that is closing may still be in a tab group
 * or hold the keyboard focus for a moment.
 */
static bool
configurable(struct wm_view *view)
{
	return view->xdg_toplevel && view->xdg_toplevel->base->initialized;
}

static void
xdg_view_set_size(struct wm_view *view, int width, int height)
{
	if (configurable(view))
		wlr_xdg_toplevel_set_size(view->xdg_toplevel, width, height);
}

static void
xdg_view_set_activated(struct wm_view *view, bool activated)
{
	if (configurable(view))
		wlr_xdg_toplevel_set_activated(view->xdg_toplevel, activated);
}

static void
xdg_view_set_maximized(struct wm_view *view, bool maximized)
{
	if (configurable(view))
		wlr_xdg_toplevel_set_maximized(view->xdg_toplevel, maximized);
}

static void
xdg_view_set_fullscreen(struct wm_view *view, bool fullscreen)
{
	if (configurable(view))
		wlr_xdg_toplevel_set_fullscreen(view->xdg_toplevel,
			fullscreen);
}

static void
xdg_view_set_tiled(struct wm_view *view, uint32_t edges)
{
	if (configurable(view))
		wlr_xdg_toplevel_set_tiled(view->xdg_toplevel, edges);
}

static void
xdg_view_close(struct wm_view *view)
{
	wlr_xdg_toplevel_send_close(view->xdg_toplevel);
}

static struct wl_client *
xdg_view_client(struct wm_view *view)
{
	struct wl_resource *res = view->xdg_toplevel->base->resource;
	return res ? wl_resource_get_client(res) : NULL;
}

static pid_t
xdg_view_pid(struct wm_view *view)
{
	struct wl_client *client = xdg_view_client(view);
	pid_t pid = 0;
	if (client)
		wl_client_get_credentials(client, &pid, NULL, NULL);
	return pid;
}

static struct wm_view *
xdg_view_parent(struct wm_view *view)
{
	struct wlr_xdg_toplevel *parent = view->xdg_toplevel->parent;
	if (!parent)
		return NULL;
	struct wm_view *v;
	wl_list_for_each(v, &view->server->views, link) {
		if (v->xdg_toplevel == parent)
			return v;
	}
	return NULL;
}

const struct wm_view_impl wm_xdg_view_impl = {
	.surface = xdg_view_surface,
	.content_geometry = xdg_view_content_geometry,
	.set_size = xdg_view_set_size,
	.moved = NULL,
	.set_activated = xdg_view_set_activated,
	.set_maximized = xdg_view_set_maximized,
	.set_fullscreen = xdg_view_set_fullscreen,
	.set_tiled = xdg_view_set_tiled,
	.close = xdg_view_close,
	.client = xdg_view_client,
	.pid = xdg_view_pid,
	.parent = xdg_view_parent,
};

/* --- Toplevel events --- */

static void
handle_xdg_toplevel_map(struct wl_listener *listener, void *data)
{
	struct wm_view *view = wl_container_of(listener, view, map);

	/* Check if this view should be docked into the slit */
	if (view->server->slit &&
	    wm_rules_should_dock(&view->server->rules, view)) {
		view->mapped = true;
		wm_slit_add_native_client(view->server->slit,
			view->xdg_toplevel, view->scene_tree);
		view->docked = true;
		wlr_scene_node_set_enabled(
			&view->scene_tree->node, true);
		wlr_log(WLR_INFO, "view docked to slit: %s",
			view->app_id ? view->app_id : "(null)");
		return;
	}

	wm_view_map_common(view, view->xdg_toplevel->requested.maximized,
		view->xdg_toplevel->requested.fullscreen);
}

static void
handle_xdg_toplevel_unmap(struct wl_listener *listener, void *data)
{
	struct wm_view *view = wl_container_of(listener, view, unmap);

	/* Docked views: slit handles unmap via its own listener */
	if (view->docked) {
		view->mapped = false;
		return;
	}
	wm_view_unmap_common(view);
}

static void
handle_xdg_toplevel_commit(struct wl_listener *listener, void *data)
{
	struct wm_view *view = wl_container_of(listener, view, commit);
	struct wlr_xdg_surface *xdg_surface = view->xdg_toplevel->base;

	if (xdg_surface->initial_commit) {
		/*
		 * On initial commit, send a configure so the client
		 * knows it can map. This is required by wlroots 0.18+.
		 */
		wlr_xdg_toplevel_set_size(view->xdg_toplevel, 0, 0);
		return;
	}

	/* Refresh decoration geometry when client commits a new size */
	if (view->decoration && view->server->style) {
		wm_decoration_refresh_geometry(view->decoration,
			view->server->style);
	}
}

static void
handle_xdg_toplevel_destroy(struct wl_listener *listener, void *data)
{
	struct wm_view *view = wl_container_of(listener, view, destroy);

	wl_list_remove(&view->map.link);
	wl_list_remove(&view->unmap.link);
	wl_list_remove(&view->commit.link);
	wl_list_remove(&view->destroy.link);
	wl_list_remove(&view->request_move.link);
	wl_list_remove(&view->request_resize.link);
	wl_list_remove(&view->request_maximize.link);
	wl_list_remove(&view->request_fullscreen.link);
	wl_list_remove(&view->request_minimize.link);
	wl_list_remove(&view->request_show_window_menu.link);
	wl_list_remove(&view->set_title.link);
	wl_list_remove(&view->set_app_id.link);

	wm_view_destroy_common(view);
	free(view);
}

static void
handle_xdg_toplevel_request_move(struct wl_listener *listener, void *data)
{
	struct wm_view *view = wl_container_of(listener, view, request_move);
	wm_view_begin_interactive(view, WM_CURSOR_MOVE, 0);
}

static void
handle_xdg_toplevel_request_resize(struct wl_listener *listener, void *data)
{
	struct wlr_xdg_toplevel_resize_event *event = data;
	struct wm_view *view = wl_container_of(listener, view,
		request_resize);
	wm_view_begin_interactive(view, WM_CURSOR_RESIZE, event->edges);
}

static void
handle_xdg_toplevel_request_maximize(struct wl_listener *listener,
	void *data)
{
	struct wm_view *view = wl_container_of(listener, view,
		request_maximize);
	/* Ignore pre-initial-commit requests — state is stored in
	 * toplevel->requested and applied at map time */
	if (!view->xdg_toplevel->base->initialized)
		return;
	if (view->xdg_toplevel->requested.maximized != view->maximized)
		wm_view_toggle_maximize(view);
	else
		/* xdg-shell requires a configure in reply to every request */
		wlr_xdg_surface_schedule_configure(view->xdg_toplevel->base);
}

static void
handle_xdg_toplevel_request_fullscreen(struct wl_listener *listener,
	void *data)
{
	struct wm_view *view = wl_container_of(listener, view,
		request_fullscreen);
	if (!view->xdg_toplevel->base->initialized)
		return;
	if (view->xdg_toplevel->requested.fullscreen != view->fullscreen)
		wm_view_toggle_fullscreen(view);
	else
		wlr_xdg_surface_schedule_configure(view->xdg_toplevel->base);
}

static void
handle_xdg_toplevel_request_minimize(struct wl_listener *listener,
	void *data)
{
	struct wm_view *view = wl_container_of(listener, view,
		request_minimize);
	wm_view_minimize(view);
}

static void
handle_xdg_toplevel_set_title(struct wl_listener *listener, void *data)
{
	struct wm_view *view = wl_container_of(listener, view, set_title);
	wm_view_set_title(view, view->xdg_toplevel->title);
}

static void
handle_xdg_toplevel_set_app_id(struct wl_listener *listener, void *data)
{
	struct wm_view *view = wl_container_of(listener, view, set_app_id);
	wm_view_set_app_id(view, view->xdg_toplevel->app_id);
}

static void
handle_xdg_toplevel_request_show_window_menu(struct wl_listener *listener,
	void *data)
{
	struct wm_view *view = wl_container_of(listener, view,
		request_show_window_menu);
	struct wlr_xdg_toplevel_show_window_menu_event *event = data;

	/* The client's menu button: Fluxbox's window menu at that spot */
	struct wlr_box geo;
	wm_view_content_geometry(view, &geo);
	int left = 0, top = 0;
	if (view->decoration && view->show_decoration) {
		int bottom, right;
		wm_decoration_get_extents(view->decoration,
			&top, &bottom, &left, &right);
	}
	if (view != view->server->focused_view) {
		view->server->focus_user_initiated = true;
		wm_focus_view(view, wm_view_surface(view));
	}
	view->server->menu_view = view;
	wm_menu_show_window(view->server,
		view->x + left + event->x - geo.x,
		view->y + top + event->y - geo.y);
}

/* --- New XDG toplevel handler --- */

static void
handle_new_xdg_toplevel(struct wl_listener *listener, void *data)
{
	struct wm_server *server =
		wl_container_of(listener, server, new_xdg_toplevel);
	struct wlr_xdg_toplevel *xdg_toplevel = data;

	struct wm_view *view = calloc(1, sizeof(*view));
	if (!view) {
		wlr_log(WLR_ERROR, "%s", "allocation failed");
		return;
	}

	view->type = WM_VIEW_XDG;
	view->impl = &wm_xdg_view_impl;
	view->xdg_toplevel = xdg_toplevel;

	if (!wm_view_init_common(view, server)) {
		free(view);
		return;
	}

	/* Attach the XDG surface to the scene tree */
	struct wlr_scene_tree *surface_tree =
		wlr_scene_xdg_surface_create(view->scene_tree,
			xdg_toplevel->base);
	if (!surface_tree) {
		wl_list_remove(&view->link);
		wlr_scene_node_destroy(&view->scene_tree->node);
		free(view);
		return;
	}

	/* Popups of this toplevel are created under its surface tree */
	xdg_toplevel->base->data = surface_tree;

	/* Connect XDG toplevel signals */
	view->map.notify = handle_xdg_toplevel_map;
	wl_signal_add(&xdg_toplevel->base->surface->events.map,
		&view->map);

	view->unmap.notify = handle_xdg_toplevel_unmap;
	wl_signal_add(&xdg_toplevel->base->surface->events.unmap,
		&view->unmap);

	view->commit.notify = handle_xdg_toplevel_commit;
	wl_signal_add(&xdg_toplevel->base->surface->events.commit,
		&view->commit);

	view->destroy.notify = handle_xdg_toplevel_destroy;
	wl_signal_add(&xdg_toplevel->events.destroy, &view->destroy);

	view->request_move.notify = handle_xdg_toplevel_request_move;
	wl_signal_add(&xdg_toplevel->events.request_move,
		&view->request_move);

	view->request_resize.notify = handle_xdg_toplevel_request_resize;
	wl_signal_add(&xdg_toplevel->events.request_resize,
		&view->request_resize);

	view->request_maximize.notify =
		handle_xdg_toplevel_request_maximize;
	wl_signal_add(&xdg_toplevel->events.request_maximize,
		&view->request_maximize);

	view->request_fullscreen.notify =
		handle_xdg_toplevel_request_fullscreen;
	wl_signal_add(&xdg_toplevel->events.request_fullscreen,
		&view->request_fullscreen);

	view->request_minimize.notify =
		handle_xdg_toplevel_request_minimize;
	wl_signal_add(&xdg_toplevel->events.request_minimize,
		&view->request_minimize);

	view->request_show_window_menu.notify =
		handle_xdg_toplevel_request_show_window_menu;
	wl_signal_add(&xdg_toplevel->events.request_show_window_menu,
		&view->request_show_window_menu);

	view->set_title.notify = handle_xdg_toplevel_set_title;
	wl_signal_add(&xdg_toplevel->events.set_title,
		&view->set_title);

	view->set_app_id.notify = handle_xdg_toplevel_set_app_id;
	wl_signal_add(&xdg_toplevel->events.set_app_id,
		&view->set_app_id);

	wlr_log(WLR_DEBUG, "new xdg toplevel: %s (%s)",
		xdg_toplevel->title ? xdg_toplevel->title : "(untitled)",
		xdg_toplevel->app_id ? xdg_toplevel->app_id : "(no app_id)");
}

/* --- XDG popups --- */

struct wm_popup {
	struct wm_server *server;
	struct wlr_xdg_popup *popup;
	struct wl_listener commit;
	struct wl_listener destroy;
};

/*
 * Keep a popup on screen: constrain it to the output its window (or
 * layer surface) is on. Popup coordinates are relative to the tree the
 * first popup of the chain hangs off: the toplevel's xdg tree (its
 * window-geometry origin) or the layer surface's popup tree.
 */
static void
popup_unconstrain(struct wm_popup *p)
{
	struct wlr_xdg_popup *top = p->popup;
	struct wlr_xdg_surface *xs;
	while ((xs = wlr_xdg_surface_try_from_wlr_surface(top->parent)) &&
	       xs->role == WLR_XDG_SURFACE_ROLE_POPUP && xs->popup)
		top = xs->popup;

	struct wlr_scene_tree *top_tree = top->base->data;
	if (!top_tree || !top_tree->node.parent)
		return;
	int lx, ly;
	if (!wlr_scene_node_coords(&top_tree->node.parent->node, &lx, &ly))
		return;

	struct wlr_output *output = NULL;
	struct wlr_layer_surface_v1 *layer =
		wlr_layer_surface_v1_try_from_wlr_surface(top->parent);
	if (layer) {
		output = layer->output;
	} else if (xs && xs->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
		struct wlr_box geo;
		geo = xs->geometry;
		output = wlr_output_layout_output_at(p->server->output_layout,
			lx + geo.width / 2.0, ly + geo.height / 2.0);
	}
	if (!output)
		output = wlr_output_layout_output_at(p->server->output_layout,
			p->server->cursor->x, p->server->cursor->y);
	if (!output)
		return;

	struct wlr_box ob;
	wlr_output_layout_get_box(p->server->output_layout, output, &ob);
	struct wlr_box box = {
		.x = ob.x - lx,
		.y = ob.y - ly,
		.width = ob.width,
		.height = ob.height,
	};
	wlr_xdg_popup_unconstrain_from_box(p->popup, &box);
}

static void
handle_popup_commit(struct wl_listener *listener, void *data)
{
	struct wm_popup *p = wl_container_of(listener, p, commit);
	if (p->popup->base->initial_commit)
		popup_unconstrain(p);
}

static void
handle_popup_destroy(struct wl_listener *listener, void *data)
{
	struct wm_popup *p = wl_container_of(listener, p, destroy);
	wl_list_remove(&p->commit.link);
	wl_list_remove(&p->destroy.link);
	free(p);
}

static void
handle_new_xdg_popup(struct wl_listener *listener, void *data)
{
	struct wm_server *server =
		wl_container_of(listener, server, new_xdg_popup);
	struct wlr_xdg_popup *popup = data;

	/*
	 * Layer-shell popups have no parent yet (it is assigned later by
	 * get_popup); layer_shell.c adopts them via the layer surface's
	 * new_popup signal.
	 */
	if (!popup->parent)
		return;
	struct wlr_xdg_surface *parent =
		wlr_xdg_surface_try_from_wlr_surface(popup->parent);
	if (!parent || !parent->data)
		return;

	/* Nest under the parent's tree so it moves with the window */
	struct wlr_scene_tree *parent_tree = parent->data;
	struct wlr_scene_tree *popup_tree =
		wlr_scene_xdg_surface_create(parent_tree, popup->base);
	if (!popup_tree)
		return;
	popup->base->data = popup_tree;
	wm_xdg_popup_track(server, popup);
}

void
wm_xdg_popup_track(struct wm_server *server, struct wlr_xdg_popup *popup)
{
	struct wm_popup *p = calloc(1, sizeof(*p));
	if (!p)
		return;
	p->server = server;
	p->popup = popup;
	p->commit.notify = handle_popup_commit;
	wl_signal_add(&popup->base->surface->events.commit, &p->commit);
	p->destroy.notify = handle_popup_destroy;
	wl_signal_add(&popup->events.destroy, &p->destroy);
}

/* --- Module initialization --- */

void
wm_view_init(struct wm_server *server)
{
	server->new_xdg_toplevel.notify = handle_new_xdg_toplevel;
	wl_signal_add(&server->xdg_shell->events.new_toplevel,
		&server->new_xdg_toplevel);

	server->new_xdg_popup.notify = handle_new_xdg_popup;
	wl_signal_add(&server->xdg_shell->events.new_popup,
		&server->new_xdg_popup);
}

void
wm_view_finish(struct wm_server *server)
{
	wl_list_remove(&server->new_xdg_toplevel.link);
	wl_list_remove(&server->new_xdg_popup.link);
}
