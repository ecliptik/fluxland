/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * xwayland_grab.c - xwayland-keyboard-grab-unstable-v1
 */

#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <stdlib.h>

#include <wayland-server-core.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/util/log.h>
#include <wlr/xwayland.h>

#include "config.h"
#include "server.h"
#include "xwayland_grab.h"
#include "xwayland-keyboard-grab-unstable-v1-protocol.h"

#ifdef WM_HAS_XWAYLAND

struct wm_xwayland_grab {
	struct wm_server *server;
	struct wl_global *global;
	struct wl_listener display_destroy;

	/* The grab in force, if any: one at a time, the latest */
	struct wl_resource *grab;
	struct wlr_surface *surface;
	struct wl_listener surface_destroy;
};

static void
grab_end(struct wm_xwayland_grab *g)
{
	if (!g->grab)
		return;
	wl_list_remove(&g->surface_destroy.link);
	g->grab = NULL;
	g->surface = NULL;
	wlr_log(WLR_DEBUG, "%s", "xwayland: keyboard grab ended");
}

static void
handle_surface_destroy(struct wl_listener *listener, void *data)
{
	struct wm_xwayland_grab *g =
		wl_container_of(listener, g, surface_destroy);
	grab_end(g);
}

static void
grab_resource_destroy(struct wl_resource *resource)
{
	struct wm_xwayland_grab *g = wl_resource_get_user_data(resource);
	if (g && g->grab == resource)
		grab_end(g);
}

static void
grab_destroy(struct wl_client *client, struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

static const struct zwp_xwayland_keyboard_grab_v1_interface grab_impl = {
	.destroy = grab_destroy,
};

struct wlr_surface *
wm_xwayland_grab_surface(struct wm_server *server)
{
	return server->xwayland_grab ? server->xwayland_grab->surface : NULL;
}

void
wm_xwayland_grab_focus(struct wm_server *server)
{
	struct wlr_surface *surface = wm_xwayland_grab_surface(server);
	if (!surface || server->seat->keyboard_state.focused_surface ==
	    surface)
		return;
	struct wlr_keyboard *kb = wlr_seat_get_keyboard(server->seat);
	wlr_seat_keyboard_notify_enter(server->seat, surface,
		kb ? kb->keycodes : NULL, kb ? kb->num_keycodes : 0,
		kb ? &kb->modifiers : NULL);
}

static void
manager_destroy(struct wl_client *client, struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

static void
manager_grab_keyboard(struct wl_client *client,
	struct wl_resource *resource, uint32_t id,
	struct wl_resource *surface_resource,
	struct wl_resource *seat_resource)
{
	struct wm_xwayland_grab *g = wl_resource_get_user_data(resource);
	struct wl_resource *grab = wl_resource_create(client,
		&zwp_xwayland_keyboard_grab_v1_interface,
		wl_resource_get_version(resource), id);
	if (!grab) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(grab, &grab_impl, g,
		grab_resource_destroy);

	/* A request the protocol lets the compositor ignore: the object is
	 * there, and does nothing */
	struct wlr_surface *surface = wlr_surface_from_resource(
		surface_resource);
	if (!surface || !g->server->config ||
	    !g->server->config->xwayland_keyboard_grab)
		return;

	grab_end(g);
	g->grab = grab;
	g->surface = surface;
	g->surface_destroy.notify = handle_surface_destroy;
	wl_signal_add(&surface->events.destroy, &g->surface_destroy);
	wlr_log(WLR_INFO, "%s", "xwayland: keyboard grabbed");
	wm_xwayland_grab_focus(g->server);
}

static const struct zwp_xwayland_keyboard_grab_manager_v1_interface
manager_impl = {
	.destroy = manager_destroy,
	.grab_keyboard = manager_grab_keyboard,
};

static void
bind_manager(struct wl_client *client, void *data, uint32_t version,
	uint32_t id)
{
	struct wm_xwayland_grab *g = data;
	struct wl_resource *resource = wl_resource_create(client,
		&zwp_xwayland_keyboard_grab_manager_v1_interface, version, id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &manager_impl, g, NULL);
}

/* Compositors are required to show this protocol to Xwayland alone */
static bool
global_filter(const struct wl_client *client, const struct wl_global *global,
	void *data)
{
	if (wl_global_get_interface(global) !=
	    &zwp_xwayland_keyboard_grab_manager_v1_interface)
		return true;
	struct wm_server *server = data;
	return server->xwayland && server->xwayland->server &&
		client == server->xwayland->server->client;
}

static void
handle_display_destroy(struct wl_listener *listener, void *data)
{
	struct wm_xwayland_grab *g =
		wl_container_of(listener, g, display_destroy);
	g->server->xwayland_grab = NULL;
	grab_end(g);
	wl_list_remove(&g->display_destroy.link);
	free(g);
}

void
wm_xwayland_grab_init(struct wm_server *server)
{
	if (server->xwayland_grab || !server->xwayland)
		return;
	struct wm_xwayland_grab *g = calloc(1, sizeof(*g));
	if (!g)
		return;
	g->server = server;
	g->global = wl_global_create(server->wl_display,
		&zwp_xwayland_keyboard_grab_manager_v1_interface, 1, g,
		bind_manager);
	if (!g->global) {
		free(g);
		return;
	}
	wl_display_set_global_filter(server->wl_display, global_filter,
		server);
	g->display_destroy.notify = handle_display_destroy;
	wl_display_add_destroy_listener(server->wl_display,
		&g->display_destroy);
	server->xwayland_grab = g;
}

void
wm_xwayland_grab_finish(struct wm_server *server)
{
	/* The display's destruction frees it: nothing to give up before */
	(void)server;
}

#else /* !WM_HAS_XWAYLAND */

void
wm_xwayland_grab_init(struct wm_server *server)
{
	(void)server;
}

void
wm_xwayland_grab_finish(struct wm_server *server)
{
	(void)server;
}

struct wlr_surface *
wm_xwayland_grab_surface(struct wm_server *server)
{
	(void)server;
	return NULL;
}

void
wm_xwayland_grab_focus(struct wm_server *server)
{
	(void)server;
}

#endif /* WM_HAS_XWAYLAND */
