/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 *
 * slit.c - Fluxbox slit dockapp container
 *
 * Implements the slit: a container panel that holds small dockapp
 * utilities arranged horizontally or vertically at a configurable
 * screen edge position.  Supports auto-hide.
 */

#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <string.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/log.h>

#include "slit.h"
#include "config.h"
#include "output.h"
#include "server.h"
#include "style.h"

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

	/* Apply slit config options */
	if (server->config) {
		slit->auto_hide = server->config->slit_auto_hide;
		slit->placement = server->config->slit_placement;
		slit->direction = server->config->slit_direction;
		slit->on_head = server->config->slit_on_head;
	}
	slit->width = WM_SLIT_MIN_SIZE;
	slit->height = WM_SLIT_MIN_SIZE;
	wl_list_init(&slit->clients);

	/* Create scene tree in layer_top */
	slit->scene_tree = wlr_scene_tree_create(server->layer_top);
	if (!slit->scene_tree) {
		free(slit);
		return NULL;
	}

	/* Background rect using style colors */
	float bg_color[4] = {0.2f, 0.2f, 0.2f, 0.85f};
	if (server->style) {
		struct wm_color tc = wm_argb_to_color(
			server->style->toolbar_texture.color);
		bg_color[0] = tc.r / 255.0f;
		bg_color[1] = tc.g / 255.0f;
		bg_color[2] = tc.b / 255.0f;
		bg_color[3] = 0.9f;
	}
	slit->bg_rect = wlr_scene_rect_create(slit->scene_tree,
		slit->width, slit->height, bg_color);

	/* Auto-hide timer */
	slit->hide_timer = wl_event_loop_add_timer(
		server->wl_event_loop, slit_hide_timer_cb, slit);

	/* Position the slit */
	wm_slit_reconfigure(slit);

	/* Hide initially if no clients */
	wlr_scene_node_set_enabled(&slit->scene_tree->node, false);

	wlr_log(WLR_INFO, "slit created (placement=%d, direction=%d, "
		"auto_hide=%d)", slit->placement, slit->direction,
		slit->auto_hide);

	return slit;
}

void
wm_slit_destroy(struct wm_slit *slit)
{
	if (!slit) {
		return;
	}

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

	wl_list_insert(slit->clients.prev, &client->link);
	slit->client_count++;

	wlr_log(WLR_INFO, "slit: added client (total %d)",
		slit->client_count);

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
	wl_list_remove(&client->map.link);
	wl_list_remove(&client->unmap.link);
	wl_list_remove(&client->destroy.link);
	wl_list_remove(&client->configure.link);
#endif

	if (client->scene_tree) {
		wlr_scene_node_destroy(&client->scene_tree->node);
	}

	wl_list_remove(&client->link);
	slit->client_count--;
	free(client);

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

	/* Update background rect size */
	if (slit->bg_rect) {
		wlr_scene_rect_set_size(slit->bg_rect,
			slit->width, slit->height);
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
}

bool
wm_slit_handle_pointer_enter(struct wm_slit *slit, double lx, double ly)
{
	if (!slit || !slit->auto_hide) {
		return false;
	}

	/* Check if pointer is in the slit's hot zone */
	struct wlr_box box = {
		.x = slit->x, .y = slit->y,
		.width = slit->width, .height = slit->height,
	};

	/* Extend hit area slightly for hidden slit */
	if (slit->hidden) {
		struct wm_output *output = get_slit_output(slit);
		if (!output) {
			return false;
		}

		int out_w, out_h;
		wlr_output_effective_resolution(output->wlr_output,
			&out_w, &out_h);

		/* Create a thin trigger zone at the edge */
		int trigger = 2;
		switch (slit->placement) {
		case WM_SLIT_RIGHT_TOP:
		case WM_SLIT_RIGHT_CENTER:
		case WM_SLIT_RIGHT_BOTTOM:
			box.x = out_w - trigger;
			box.width = trigger;
			break;
		case WM_SLIT_LEFT_TOP:
		case WM_SLIT_LEFT_CENTER:
		case WM_SLIT_LEFT_BOTTOM:
			box.x = 0;
			box.width = trigger;
			break;
		case WM_SLIT_TOP_LEFT:
		case WM_SLIT_TOP_CENTER:
		case WM_SLIT_TOP_RIGHT:
			box.y = 0;
			box.height = trigger;
			break;
		case WM_SLIT_BOTTOM_LEFT:
		case WM_SLIT_BOTTOM_CENTER:
		case WM_SLIT_BOTTOM_RIGHT:
			box.y = out_h - trigger;
			box.height = trigger;
			break;
		}
	}

	if (lx >= box.x && lx < box.x + box.width &&
	    ly >= box.y && ly < box.y + box.height) {
		/* Cancel hide timer */
		if (slit->hide_timer) {
			wl_event_source_timer_update(slit->hide_timer, 0);
		}

		/* Show the slit */
		if (slit->hidden) {
			slit->hidden = false;
			wlr_scene_node_set_enabled(
				&slit->scene_tree->node, true);
		}
		return true;
	}

	return false;
}

void
wm_slit_handle_pointer_leave(struct wm_slit *slit)
{
	if (!slit || !slit->auto_hide || slit->hidden) {
		return;
	}

	/* Start hide timer */
	if (slit->hide_timer) {
		wl_event_source_timer_update(slit->hide_timer,
			WM_SLIT_HIDE_DELAY_MS);
	}
}

void
wm_slit_toggle_above(struct wm_slit *slit)
{
	if (!slit || !slit->scene_tree) {
		return;
	}

	struct wm_server *server = slit->server;

	/*
	 * Toggle between layer_top (normal) and layer_overlay (above all).
	 * Reparent the slit scene tree to the other layer.
	 */
	struct wlr_scene_tree *current_parent =
		(struct wlr_scene_tree *)slit->scene_tree->node.parent;

	if (current_parent == server->layer_overlay) {
		wlr_scene_node_reparent(&slit->scene_tree->node,
			server->layer_top);
		wlr_log(WLR_INFO, "%s", "slit moved to normal layer");
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
