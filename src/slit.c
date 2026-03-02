/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * slit.c - Fluxbox slit dockapp container
 *
 * Implements the slit: a container panel that holds small dockapp
 * utilities arranged horizontally or vertically at a configurable
 * screen edge position.  Supports auto-hide.
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/log.h>

#include "slit.h"
#include "config.h"
#include "output.h"
#include "server.h"
#include "style.h"
#include "util.h"

#include <wlr/types/wlr_xdg_shell.h>

#ifdef WM_HAS_XWAYLAND
#include <wlr/xwayland.h>
#endif

#define WM_SLIT_PADDING 2
#define WM_SLIT_HIDE_DELAY_MS 800
#define WM_SLIT_MIN_SIZE 8

/* --- Helper: get output for slit --- */

static struct wm_output *
get_slit_output(struct wm_slit *slit)
{
	struct wm_server *server = slit->server;
	if (wl_list_empty(&server->outputs)) {
		return NULL;
	}

	int idx = 0;
	struct wm_output *output;
	wl_list_for_each(output, &server->outputs, link) {
		if (idx == slit->on_head) {
			return output;
		}
		idx++;
	}

	/* Fallback to primary */
	return wl_container_of(server->outputs.next,
		(struct wm_output *)NULL, link);
}

/* --- Slit positioning --- */

static void
slit_compute_position(struct wm_slit *slit, int out_w, int out_h)
{
	int w = slit->width;
	int h = slit->height;

	switch (slit->placement) {
	case WM_SLIT_TOP_LEFT:
		slit->x = 0;
		slit->y = 0;
		break;
	case WM_SLIT_TOP_CENTER:
		slit->x = (out_w - w) / 2;
		slit->y = 0;
		break;
	case WM_SLIT_TOP_RIGHT:
		slit->x = out_w - w;
		slit->y = 0;
		break;
	case WM_SLIT_RIGHT_TOP:
		slit->x = out_w - w;
		slit->y = 0;
		break;
	case WM_SLIT_RIGHT_CENTER:
		slit->x = out_w - w;
		slit->y = (out_h - h) / 2;
		break;
	case WM_SLIT_RIGHT_BOTTOM:
		slit->x = out_w - w;
		slit->y = out_h - h;
		break;
	case WM_SLIT_BOTTOM_LEFT:
		slit->x = 0;
		slit->y = out_h - h;
		break;
	case WM_SLIT_BOTTOM_CENTER:
		slit->x = (out_w - w) / 2;
		slit->y = out_h - h;
		break;
	case WM_SLIT_BOTTOM_RIGHT:
		slit->x = out_w - w;
		slit->y = out_h - h;
		break;
	case WM_SLIT_LEFT_TOP:
		slit->x = 0;
		slit->y = 0;
		break;
	case WM_SLIT_LEFT_CENTER:
		slit->x = 0;
		slit->y = (out_h - h) / 2;
		break;
	case WM_SLIT_LEFT_BOTTOM:
		slit->x = 0;
		slit->y = out_h - h;
		break;
	}
}

/* --- Layout clients within slit --- */

static void
slit_layout_clients(struct wm_slit *slit)
{
	int offset = WM_SLIT_PADDING;
	int max_cross = 0;

	struct wm_slit_client *client;
	wl_list_for_each(client, &slit->clients, link) {
		if (!client->mapped) {
			continue;
		}

		if (slit->direction == WM_SLIT_VERTICAL) {
			int cx = WM_SLIT_PADDING;
			int cy = offset;
			if (client->scene_tree) {
				wlr_scene_node_set_position(
					&client->scene_tree->node,
					cx, cy);
			}
			offset += client->height + WM_SLIT_PADDING;
			if (client->width > max_cross) {
				max_cross = client->width;
			}
		} else {
			int cx = offset;
			int cy = WM_SLIT_PADDING;
			if (client->scene_tree) {
				wlr_scene_node_set_position(
					&client->scene_tree->node,
					cx, cy);
			}
			offset += client->width + WM_SLIT_PADDING;
			if (client->height > max_cross) {
				max_cross = client->height;
			}
		}
	}

	/* Compute slit dimensions */
	if (slit->direction == WM_SLIT_VERTICAL) {
		slit->width = max_cross + 2 * WM_SLIT_PADDING;
		slit->height = offset;
	} else {
		slit->width = offset;
		slit->height = max_cross + 2 * WM_SLIT_PADDING;
	}

	if (slit->width < WM_SLIT_MIN_SIZE) {
		slit->width = WM_SLIT_MIN_SIZE;
	}
	if (slit->height < WM_SLIT_MIN_SIZE) {
		slit->height = WM_SLIT_MIN_SIZE;
	}
}

/* --- Auto-hide timer callback --- */

static int
slit_hide_timer_cb(void *data)
{
	struct wm_slit *slit = data;

	if (slit->auto_hide && !slit->hidden) {
		slit->hidden = true;
		if (slit->scene_tree) {
			wlr_scene_node_set_enabled(
				&slit->scene_tree->node, false);
		}
	}

	return 0;
}

/* --- Alpha helpers --- */

static void
slit_set_buffer_opacity(struct wlr_scene_buffer *buffer, int sx, int sy,
	void *user_data)
{
	float *opacity = user_data;
	wlr_scene_buffer_set_opacity(buffer, *opacity);
}

static void
slit_apply_alpha(struct wm_slit *slit)
{
	float alpha = slit->alpha / 255.0f;

	/* Apply alpha to bg_rect color */
	if (slit->bg_rect) {
		float color[4];
		memcpy(color, slit->bg_rect->color, sizeof(color));
		color[3] *= alpha;
		wlr_scene_rect_set_color(slit->bg_rect, color);
	}

	/* Apply alpha to border_rect color */
	if (slit->border_rect) {
		float color[4];
		memcpy(color, slit->border_rect->color, sizeof(color));
		color[3] *= alpha;
		wlr_scene_rect_set_color(slit->border_rect, color);
	}

	/* Apply alpha to all client surface buffers */
	struct wm_slit_client *client;
	wl_list_for_each(client, &slit->clients, link) {
		if (client->scene_tree && client->mapped) {
			wlr_scene_node_for_each_buffer(
				&client->scene_tree->node,
				slit_set_buffer_opacity, &alpha);
		}
	}
}

/* --- Helper: get identifier name for a slit client --- */

static const char *
slit_client_name(struct wm_slit_client *client)
{
#ifdef WM_HAS_XWAYLAND
	if (client->type == WM_SLIT_CLIENT_XWAYLAND && client->xsurface)
		return client->xsurface->class;
#endif
	if (client->type == WM_SLIT_CLIENT_NATIVE && client->xdg_toplevel)
		return client->xdg_toplevel->app_id;
	return NULL;
}

/* --- Slitlist functions (persistent dockapp ordering) --- */

static void
slit_load_slitlist(struct wm_slit *slit, const char *path)
{
	if (!path)
		return;

	FILE *fp = fopen_nofollow(path, "r");
	if (!fp)
		return;

	/* Count lines first */
	int count = 0;
	char line[256];
	while (fgets(line, sizeof(line), fp)) {
		/* Strip trailing newline */
		size_t len = strlen(line);
		if (len > 0 && line[len - 1] == '\n')
			line[len - 1] = '\0';
		if (line[0] != '\0')
			count++;
	}

	if (count == 0) {
		fclose(fp);
		return;
	}

	slit->slitlist = calloc(count, sizeof(char *));
	if (!slit->slitlist) {
		fclose(fp);
		return;
	}

	rewind(fp);
	int i = 0;
	while (fgets(line, sizeof(line), fp) && i < count) {
		size_t len = strlen(line);
		if (len > 0 && line[len - 1] == '\n')
			line[len - 1] = '\0';
		if (line[0] == '\0')
			continue;
		slit->slitlist[i] = strdup(line);
		if (!slit->slitlist[i])
			break;
		i++;
	}
	slit->slitlist_count = i;

	fclose(fp);
	wlr_log(WLR_INFO, "slit: loaded slitlist with %d entries", i);
}

static void
slit_save_slitlist(struct wm_slit *slit, const char *path)
{
	if (!path)
		return;

	FILE *fp = fopen_nofollow(path, "w");
	if (!fp) {
		wlr_log(WLR_ERROR, "slit: failed to save slitlist to %s",
			path);
		return;
	}

	struct wm_slit_client *client;
	wl_list_for_each(client, &slit->clients, link) {
		const char *name = slit_client_name(client);
		if (name) {
			fprintf(fp, "%s\n", name);
		}
	}

	fclose(fp);
}

static void
slit_free_slitlist(struct wm_slit *slit)
{
	if (slit->slitlist) {
		for (int i = 0; i < slit->slitlist_count; i++)
			free(slit->slitlist[i]);
		free(slit->slitlist);
		slit->slitlist = NULL;
		slit->slitlist_count = 0;
	}
}

/*
 * Find the correct insertion point for a client based on slitlist order.
 * Returns the wl_list link to insert BEFORE, or &slit->clients (append).
 */
static struct wl_list *
slit_find_insert_position(struct wm_slit *slit, const char *class_name)
{
	if (!class_name || slit->slitlist_count == 0)
		return slit->clients.prev; /* append */

	/* Find this class's position in the slitlist */
	int target_idx = -1;
	for (int i = 0; i < slit->slitlist_count; i++) {
		if (slit->slitlist[i] &&
		    strcmp(slit->slitlist[i], class_name) == 0) {
			target_idx = i;
			break;
		}
	}

	if (target_idx < 0)
		return slit->clients.prev; /* not in list, append */

	/* Walk existing clients: find the first client whose slitlist
	 * index is greater than target_idx; insert before it */
	struct wm_slit_client *client;
	wl_list_for_each(client, &slit->clients, link) {
		const char *cname = slit_client_name(client);
		if (!cname)
			continue;

		int cidx = -1;
		for (int i = 0; i < slit->slitlist_count; i++) {
			if (slit->slitlist[i] &&
			    strcmp(slit->slitlist[i], cname) == 0) {
				cidx = i;
				break;
			}
		}

		if (cidx > target_idx) {
			/* Insert before this client */
			return client->link.prev;
		}
	}

	return slit->clients.prev; /* append */
}

/* --- Native Wayland slit client handlers --- */

static void
handle_native_slit_client_commit(struct wl_listener *listener, void *data)
{
	struct wm_slit_client *client =
		wl_container_of(listener, client, native_commit);

	if (!client->mapped || !client->xdg_toplevel)
		return;

	struct wlr_box geo;
	wlr_xdg_surface_get_geometry(client->xdg_toplevel->base, &geo);

	int new_w = geo.width > 0 ? geo.width : client->width;
	int new_h = geo.height > 0 ? geo.height : client->height;

	if (new_w != client->width || new_h != client->height) {
		client->width = new_w;
		client->height = new_h;
		wm_slit_reconfigure(client->slit);
	}
}

static void
handle_native_slit_client_unmap(struct wl_listener *listener, void *data)
{
	struct wm_slit_client *client =
		wl_container_of(listener, client, native_unmap);

	client->mapped = false;
	wm_slit_reconfigure(client->slit);
}

static void
handle_native_slit_client_destroy(struct wl_listener *listener, void *data)
{
	struct wm_slit_client *client =
		wl_container_of(listener, client, native_destroy);

	wm_slit_remove_client(client);
}

/* --- XWayland slit client handlers --- */

#ifdef WM_HAS_XWAYLAND

static void
handle_slit_client_map(struct wl_listener *listener, void *data)
{
	struct wm_slit_client *client =
		wl_container_of(listener, client, map);

	client->mapped = true;
	client->width = client->xsurface->width;
	client->height = client->xsurface->height;

	/* Create scene tree for this surface */
	if (!client->scene_tree && client->slit->scene_tree) {
		client->scene_tree = wlr_scene_subsurface_tree_create(
			client->slit->scene_tree,
			client->xsurface->surface);
	}

	/* Apply slit alpha to this client's surface */
	if (client->slit->alpha < 255 && client->scene_tree) {
		float opacity = client->slit->alpha / 255.0f;
		wlr_scene_node_for_each_buffer(
			&client->scene_tree->node,
			slit_set_buffer_opacity, &opacity);
	}

	wlr_log(WLR_INFO, "slit client mapped: %s (%dx%d)",
		client->xsurface->title ? client->xsurface->title : "(null)",
		client->width, client->height);

	wm_slit_reconfigure(client->slit);
}

static void
handle_slit_client_unmap(struct wl_listener *listener, void *data)
{
	struct wm_slit_client *client =
		wl_container_of(listener, client, unmap);

	client->mapped = false;

	if (client->scene_tree) {
		wlr_scene_node_destroy(&client->scene_tree->node);
		client->scene_tree = NULL;
	}

	wm_slit_reconfigure(client->slit);
}

static void
handle_slit_client_destroy(struct wl_listener *listener, void *data)
{
	struct wm_slit_client *client =
		wl_container_of(listener, client, destroy);

	wm_slit_remove_client(client);
}

static void
handle_slit_client_configure(struct wl_listener *listener, void *data)
{
	struct wm_slit_client *client =
		wl_container_of(listener, client, configure);
	struct wlr_xwayland_surface_configure_event *event = data;

	/* Accept the client's requested size but override position */
	wlr_xwayland_surface_configure(client->xsurface,
		event->x, event->y, event->width, event->height);

	client->width = event->width;
	client->height = event->height;

	if (client->mapped) {
		wm_slit_reconfigure(client->slit);
	}
}

#endif /* WM_HAS_XWAYLAND */

/* --- Public API --- */

struct wm_slit *
wm_slit_create(struct wm_server *server)
{
	struct wm_slit *slit = calloc(1, sizeof(*slit));
	if (!slit) {
		return NULL;
	}

	slit->server = server;
	slit->visible = true;
	slit->hidden = false;
	slit->placement = WM_SLIT_RIGHT_CENTER;
	slit->direction = WM_SLIT_VERTICAL;
	slit->auto_hide = false;
	slit->on_head = 0;
	slit->alpha = 255;

	/* Apply slit config options */
	if (server->config) {
		slit->auto_hide = server->config->slit_auto_hide;
		slit->placement = server->config->slit_placement;
		slit->direction = server->config->slit_direction;
		slit->on_head = server->config->slit_on_head;
		slit->alpha = server->config->slit_alpha;
	}
	slit->width = WM_SLIT_MIN_SIZE;
	slit->height = WM_SLIT_MIN_SIZE;
	wl_list_init(&slit->clients);

	/* Choose parent scene tree based on configured layer */
	struct wlr_scene_tree *parent_layer = server->layer_top;
	if (server->config) {
		int layer = server->config->slit_layer;
		if (layer == 6) /* AboveDock/Overlay */
			parent_layer = server->layer_overlay;
		else if (layer == 2) /* Bottom */
			parent_layer = server->layer_bottom;
		/* else Normal/Dock -> layer_top (default) */
	}

	/* Create scene tree in selected layer */
	slit->scene_tree = wlr_scene_tree_create(parent_layer);
	if (!slit->scene_tree) {
		free(slit);
		return NULL;
	}

	/* Background rect using slit style, falling back to toolbar */
	float bg_color[4] = {0.2f, 0.2f, 0.2f, 0.85f};
	if (server->style) {
		struct wm_color tc = wm_argb_to_color(
			server->style->slit_texture.color);
		bg_color[0] = tc.r / 255.0f;
		bg_color[1] = tc.g / 255.0f;
		bg_color[2] = tc.b / 255.0f;
		bg_color[3] = 0.9f;
	}
	slit->bg_rect = wlr_scene_rect_create(slit->scene_tree,
		slit->width, slit->height, bg_color);

	/* Border rect behind the background */
	if (server->style && server->style->slit_border_width > 0) {
		struct wm_color bc = server->style->slit_border_color;
		float border_color[4] = {
			bc.r / 255.0f, bc.g / 255.0f,
			bc.b / 255.0f, bc.a / 255.0f,
		};
		slit->border_rect = wlr_scene_rect_create(
			slit->scene_tree, slit->width, slit->height,
			border_color);
		/* Border rect behind bg rect */
		wlr_scene_node_lower_to_bottom(
			&slit->border_rect->node);
	}

	/* Apply alpha to rect colors */
	if (slit->alpha < 255) {
		slit_apply_alpha(slit);
	}

	/* Auto-hide timer */
	slit->hide_timer = wl_event_loop_add_timer(
		server->wl_event_loop, slit_hide_timer_cb, slit);

	/* Load slitlist for persistent dockapp ordering */
	if (server->config && server->config->slitlist_file) {
		slit_load_slitlist(slit, server->config->slitlist_file);
	}

	/* Position the slit */
	wm_slit_reconfigure(slit);

	/* Hide initially if no clients */
	wlr_scene_node_set_enabled(&slit->scene_tree->node, false);

	wlr_log(WLR_INFO, "slit created (placement=%d, direction=%d, "
		"auto_hide=%d, alpha=%d)", slit->placement, slit->direction,
		slit->auto_hide, slit->alpha);

	return slit;
}

void
wm_slit_destroy(struct wm_slit *slit)
{
	if (!slit) {
		return;
	}

	/* Save slitlist before destroying */
	if (slit->server->config && slit->server->config->slitlist_file &&
	    slit->client_count > 0) {
		slit_save_slitlist(slit,
			slit->server->config->slitlist_file);
	}
	slit_free_slitlist(slit);

	/* Remove all clients */
	struct wm_slit_client *client, *tmp;
	wl_list_for_each_safe(client, tmp, &slit->clients, link) {
		wm_slit_remove_client(client);
	}

	if (slit->hide_timer) {
		wl_event_source_remove(slit->hide_timer);
	}

	if (slit->scene_tree) {
		wlr_scene_node_destroy(&slit->scene_tree->node);
	}

	free(slit);
}

struct wm_slit_client *
wm_slit_add_client(struct wm_slit *slit, void *surface)
{
	if (!slit || !surface) {
		return NULL;
	}

	struct wm_slit_client *client = calloc(1, sizeof(*client));
	if (!client) {
		return NULL;
	}

	client->slit = slit;
	client->mapped = false;
	client->type = WM_SLIT_CLIENT_XWAYLAND;

#ifdef WM_HAS_XWAYLAND
	struct wlr_xwayland_surface *xsurface = surface;
	client->xsurface = xsurface;
	client->width = xsurface->width > 0 ? xsurface->width : 64;
	client->height = xsurface->height > 0 ? xsurface->height : 64;

	/* Listen for surface lifecycle events */
	client->map.notify = handle_slit_client_map;
	wl_signal_add(&xsurface->surface->events.map, &client->map);

	client->unmap.notify = handle_slit_client_unmap;
	wl_signal_add(&xsurface->surface->events.unmap, &client->unmap);

	client->destroy.notify = handle_slit_client_destroy;
	wl_signal_add(&xsurface->events.destroy, &client->destroy);

	client->configure.notify = handle_slit_client_configure;
	wl_signal_add(&xsurface->events.request_configure,
		&client->configure);
#endif

	/* Use slitlist ordering if available */
	{
		const char *name = slit_client_name(client);
		struct wl_list *insert_after =
			slit_find_insert_position(slit, name);
		wl_list_insert(insert_after, &client->link);
	}

	slit->client_count++;

	/* Save updated slitlist */
	if (slit->server->config && slit->server->config->slitlist_file) {
		slit_save_slitlist(slit,
			slit->server->config->slitlist_file);
	}

	wlr_log(WLR_INFO, "slit: added client (total %d)",
		slit->client_count);

#ifdef WM_HAS_XWAYLAND
	/*
	 * If the surface is already mapped (e.g. routed from the xwayland
	 * map handler), trigger map logic immediately since we missed the
	 * wl_surface map event.
	 */
	if (client->xsurface->surface) {
		client->mapped = true;
		if (client->xsurface->width > 0)
			client->width = client->xsurface->width;
		if (client->xsurface->height > 0)
			client->height = client->xsurface->height;
		if (!client->scene_tree && slit->scene_tree) {
			client->scene_tree = wlr_scene_subsurface_tree_create(
				slit->scene_tree,
				client->xsurface->surface);
		}
		wlr_log(WLR_INFO, "slit client mapped: %s (%dx%d)",
			client->xsurface->title ?
				client->xsurface->title : "(null)",
			client->width, client->height);
		wm_slit_reconfigure(slit);
	}
#endif

	return client;
}

struct wm_slit_client *
wm_slit_add_native_client(struct wm_slit *slit,
	struct wlr_xdg_toplevel *toplevel,
	struct wlr_scene_tree *scene_tree)
{
	if (!slit || !toplevel || !scene_tree) {
		return NULL;
	}

	struct wm_slit_client *client = calloc(1, sizeof(*client));
	if (!client) {
		return NULL;
	}

	client->slit = slit;
	client->type = WM_SLIT_CLIENT_NATIVE;
	client->xdg_toplevel = toplevel;
	client->mapped = true;

	/* Reparent the view's scene tree into the slit */
	wlr_scene_node_reparent(&scene_tree->node, slit->scene_tree);
	client->scene_tree = scene_tree;

	/* Get initial geometry from XDG surface */
	struct wlr_box geo;
	wlr_xdg_surface_get_geometry(toplevel->base, &geo);
	client->width = geo.width > 0 ? geo.width : 64;
	client->height = geo.height > 0 ? geo.height : 64;

	/* Set tiled hints so client fills exact size (no gaps) */
	wlr_xdg_toplevel_set_tiled(toplevel,
		WLR_EDGE_TOP | WLR_EDGE_BOTTOM |
		WLR_EDGE_LEFT | WLR_EDGE_RIGHT);

	/* Listen for surface commit (size changes) */
	client->native_commit.notify = handle_native_slit_client_commit;
	wl_signal_add(&toplevel->base->surface->events.commit,
		&client->native_commit);

	/* Listen for surface unmap */
	client->native_unmap.notify = handle_native_slit_client_unmap;
	wl_signal_add(&toplevel->base->surface->events.unmap,
		&client->native_unmap);

	/* Listen for toplevel destroy */
	client->native_destroy.notify = handle_native_slit_client_destroy;
	wl_signal_add(&toplevel->events.destroy,
		&client->native_destroy);

	/* Use slitlist ordering (app_id as identifier) */
	{
		const char *name = toplevel->app_id;
		struct wl_list *insert_after =
			slit_find_insert_position(slit, name);
		wl_list_insert(insert_after, &client->link);
	}

	slit->client_count++;

	/* Apply slit alpha to this client's surface */
	if (slit->alpha < 255 && client->scene_tree) {
		float opacity = slit->alpha / 255.0f;
		wlr_scene_node_for_each_buffer(
			&client->scene_tree->node,
			slit_set_buffer_opacity, &opacity);
	}

	/* Save updated slitlist */
	if (slit->server->config && slit->server->config->slitlist_file) {
		slit_save_slitlist(slit,
			slit->server->config->slitlist_file);
	}

	wlr_log(WLR_INFO, "slit: added native client '%s' (%dx%d, total %d)",
		toplevel->app_id ? toplevel->app_id : "(null)",
		client->width, client->height, slit->client_count);

	/* Reconfigure slit layout */
	wm_slit_reconfigure(slit);

	return client;
}

void
wm_slit_remove_client(struct wm_slit_client *client)
{
	if (!client) {
		return;
	}

	struct wm_slit *slit = client->slit;

#ifdef WM_HAS_XWAYLAND
	if (client->type == WM_SLIT_CLIENT_XWAYLAND) {
		wl_list_remove(&client->map.link);
		wl_list_remove(&client->unmap.link);
		wl_list_remove(&client->destroy.link);
		wl_list_remove(&client->configure.link);
	}
#endif

	if (client->type == WM_SLIT_CLIENT_NATIVE) {
		wl_list_remove(&client->native_commit.link);
		wl_list_remove(&client->native_unmap.link);
		wl_list_remove(&client->native_destroy.link);
	}

	if (client->scene_tree) {
		wlr_scene_node_destroy(&client->scene_tree->node);
	}

	wl_list_remove(&client->link);
	slit->client_count--;
	free(client);

	/* Save updated slitlist */
	if (slit->server->config && slit->server->config->slitlist_file) {
		slit_save_slitlist(slit,
			slit->server->config->slitlist_file);
	}

	wm_slit_reconfigure(slit);
}

void
wm_slit_reconfigure(struct wm_slit *slit)
{
	if (!slit) {
		return;
	}

	/* Layout clients and compute slit dimensions */
	slit_layout_clients(slit);

	/* Get border width from style */
	int bw = 0;
	if (slit->server->style)
		bw = slit->server->style->slit_border_width;

	/* Update background rect size (inset by border width) */
	if (slit->bg_rect) {
		wlr_scene_rect_set_size(slit->bg_rect,
			slit->width, slit->height);
		wlr_scene_node_set_position(&slit->bg_rect->node,
			bw, bw);
	}

	/* Update border rect size (full size including border) */
	if (slit->border_rect) {
		wlr_scene_rect_set_size(slit->border_rect,
			slit->width + 2 * bw,
			slit->height + 2 * bw);
	}

	/* Position based on output and placement */
	struct wm_output *output = get_slit_output(slit);
	if (output) {
		int out_w, out_h;
		wlr_output_effective_resolution(output->wlr_output,
			&out_w, &out_h);
		slit_compute_position(slit, out_w, out_h);
	}

	wlr_scene_node_set_position(&slit->scene_tree->node,
		slit->x, slit->y);

	/* Show/hide based on whether we have mapped clients */
	bool has_clients = false;
	struct wm_slit_client *client;
	wl_list_for_each(client, &slit->clients, link) {
		if (client->mapped) {
			has_clients = true;
			break;
		}
	}

	if (has_clients && !slit->hidden) {
		wlr_scene_node_set_enabled(
			&slit->scene_tree->node, true);
	} else if (!has_clients) {
		wlr_scene_node_set_enabled(
			&slit->scene_tree->node, false);
	}

	/* MaxOver: adjust output usable_area to reserve space for slit */
	if (output && slit->server->config &&
	    !slit->server->config->slit_max_over) {
		/* Undo previous reservation */
		int old_reserved = slit->reserved_space;
		int new_reserved = 0;

		if (has_clients && !slit->hidden) {
			int total_w = slit->width + 2 * bw;
			int total_h = slit->height + 2 * bw;

			switch (slit->placement) {
			case WM_SLIT_RIGHT_TOP:
			case WM_SLIT_RIGHT_CENTER:
			case WM_SLIT_RIGHT_BOTTOM:
				new_reserved = total_w;
				output->usable_area.width += old_reserved;
				output->usable_area.width -= new_reserved;
				break;
			case WM_SLIT_LEFT_TOP:
			case WM_SLIT_LEFT_CENTER:
			case WM_SLIT_LEFT_BOTTOM:
				new_reserved = total_w;
				output->usable_area.x -= old_reserved;
				output->usable_area.width += old_reserved;
				output->usable_area.x += new_reserved;
				output->usable_area.width -= new_reserved;
				break;
			case WM_SLIT_TOP_LEFT:
			case WM_SLIT_TOP_CENTER:
			case WM_SLIT_TOP_RIGHT:
				new_reserved = total_h;
				output->usable_area.y -= old_reserved;
				output->usable_area.height += old_reserved;
				output->usable_area.y += new_reserved;
				output->usable_area.height -= new_reserved;
				break;
			case WM_SLIT_BOTTOM_LEFT:
			case WM_SLIT_BOTTOM_CENTER:
			case WM_SLIT_BOTTOM_RIGHT:
				new_reserved = total_h;
				output->usable_area.height += old_reserved;
				output->usable_area.height -= new_reserved;
				break;
			}
		} else {
			/* Slit hidden/no clients: undo reservation */
			switch (slit->placement) {
			case WM_SLIT_RIGHT_TOP:
			case WM_SLIT_RIGHT_CENTER:
			case WM_SLIT_RIGHT_BOTTOM:
				output->usable_area.width += old_reserved;
				break;
			case WM_SLIT_LEFT_TOP:
			case WM_SLIT_LEFT_CENTER:
			case WM_SLIT_LEFT_BOTTOM:
				output->usable_area.x -= old_reserved;
				output->usable_area.width += old_reserved;
				break;
			case WM_SLIT_TOP_LEFT:
			case WM_SLIT_TOP_CENTER:
			case WM_SLIT_TOP_RIGHT:
				output->usable_area.y -= old_reserved;
				output->usable_area.height += old_reserved;
				break;
			case WM_SLIT_BOTTOM_LEFT:
			case WM_SLIT_BOTTOM_CENTER:
			case WM_SLIT_BOTTOM_RIGHT:
				output->usable_area.height += old_reserved;
				break;
			}
		}
		slit->reserved_space = new_reserved;
	}
}

void
wm_slit_relayout(struct wm_slit *slit)
{
	if (!slit) {
		return;
	}

	struct wm_server *server = slit->server;
	struct wm_config *config = server->config;

	/* Re-read configuration values */
	if (config) {
		slit->placement = config->slit_placement;
		slit->direction = config->slit_direction;
		slit->auto_hide = config->slit_auto_hide;
		slit->on_head = config->slit_on_head;
		slit->alpha = config->slit_alpha;

		/* Handle layer change by reparenting the scene tree */
		struct wlr_scene_tree *target_layer = server->layer_top;
		if (config->slit_layer == 6) /* AboveDock/Overlay */
			target_layer = server->layer_overlay;
		else if (config->slit_layer == 2) /* Bottom */
			target_layer = server->layer_bottom;

		if (slit->scene_tree) {
			wlr_scene_node_reparent(
				&slit->scene_tree->node, target_layer);
		}
	}

	/* Update background color from style */
	if (server->style && slit->bg_rect) {
		struct wm_color tc = wm_argb_to_color(
			server->style->slit_texture.color);
		float bg_color[4] = {
			tc.r / 255.0f, tc.g / 255.0f,
			tc.b / 255.0f, 0.9f
		};
		wlr_scene_rect_set_color(slit->bg_rect, bg_color);
	}

	/* Update border color from style */
	if (server->style && slit->border_rect) {
		struct wm_color bc = server->style->slit_border_color;
		float border_color[4] = {
			bc.r / 255.0f, bc.g / 255.0f,
			bc.b / 255.0f, bc.a / 255.0f,
		};
		wlr_scene_rect_set_color(slit->border_rect, border_color);
	}

	/* Apply alpha */
	if (slit->alpha < 255) {
		slit_apply_alpha(slit);
	}

	/* Recalculate layout and position */
	wm_slit_reconfigure(slit);
}

void
wm_slit_toggle_above(struct wm_slit *slit)
{
	if (!slit || !slit->scene_tree) {
		return;
	}

	struct wm_server *server = slit->server;

	/*
	 * Toggle between configured base layer and layer_overlay.
	 * Reparent the slit scene tree to the other layer.
	 */
	struct wlr_scene_tree *current_parent =
		(struct wlr_scene_tree *)slit->scene_tree->node.parent;

	/* Determine the configured base layer */
	struct wlr_scene_tree *base_layer = server->layer_top;
	if (server->config) {
		int layer = server->config->slit_layer;
		if (layer == 6)
			base_layer = server->layer_overlay;
		else if (layer == 2)
			base_layer = server->layer_bottom;
	}

	if (current_parent == server->layer_overlay) {
		wlr_scene_node_reparent(&slit->scene_tree->node,
			base_layer);
		wlr_log(WLR_INFO, "%s", "slit moved to configured layer");
	} else {
		wlr_scene_node_reparent(&slit->scene_tree->node,
			server->layer_overlay);
		wlr_log(WLR_INFO, "%s", "slit moved to overlay layer");
	}
}

void
wm_slit_toggle_hidden(struct wm_slit *slit)
{
	if (!slit) {
		return;
	}

	slit->auto_hide = !slit->auto_hide;

	if (slit->auto_hide) {
		/* Start hide timer immediately */
		if (slit->hide_timer) {
			wl_event_source_timer_update(slit->hide_timer,
				WM_SLIT_HIDE_DELAY_MS);
		}
		wlr_log(WLR_INFO, "%s", "slit auto-hide enabled");
	} else {
		/* Cancel hide timer and show slit */
		if (slit->hide_timer) {
			wl_event_source_timer_update(slit->hide_timer, 0);
		}
		slit->hidden = false;
		if (slit->scene_tree) {
			wlr_scene_node_set_enabled(
				&slit->scene_tree->node, true);
		}
		wlr_log(WLR_INFO, "%s", "slit auto-hide disabled");
	}
}
