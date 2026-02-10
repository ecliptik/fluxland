/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * layer_shell.c - wlr-layer-shell-unstable-v1 protocol support
 *
 * Implements the layer shell protocol for external panels, wallpaper
 * setters, screen lockers, and notification daemons. Layer surfaces
 * are arranged into four layers (background, bottom, top, overlay)
 * with support for exclusive zones that reserve screen edge space.
 */

#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>

#include "layer_shell.h"
#include "output.h"
#include "server.h"

/*
 * Get the scene tree for a given layer shell layer.
 */
static struct wlr_scene_tree *
get_layer_tree(struct wm_server *server,
	enum zwlr_layer_shell_v1_layer layer)
{
	switch (layer) {
	case ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND:
		return server->layer_background;
	case ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM:
		return server->layer_bottom;
	case ZWLR_LAYER_SHELL_V1_LAYER_TOP:
		return server->layer_top;
	case ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY:
		return server->layer_overlay;
	default:
		return server->layer_top;
	}
}

/*
 * Find the wm_output that corresponds to a wlr_output.
 */
static struct wm_output *
output_from_wlr(struct wm_server *server, struct wlr_output *wlr_output)
{
	struct wm_output *output;
	wl_list_for_each(output, &server->outputs, link) {
		if (output->wlr_output == wlr_output)
			return output;
	}
	return NULL;
}

/*
 * Arrange all layer surfaces on a given output.
 * Iterates through each layer in order, calling
 * wlr_scene_layer_surface_v1_configure() which handles positioning
 * based on anchors/margins and updates the usable_area for
 * exclusive zones.
 */
void
wm_layer_shell_arrange(struct wm_output *output)
{
	struct wlr_box full_area = {0};
	wlr_output_effective_resolution(output->wlr_output,
		&full_area.width, &full_area.height);

	struct wlr_box usable_area = full_area;

	struct wm_server *server = output->server;
	struct wlr_scene_tree *layers[] = {
		server->layer_overlay,
		server->layer_top,
		server->layer_bottom,
		server->layer_background,
	};

	/*
	 * First pass: arrange surfaces with exclusive zones.
	 * These reserve edge space (e.g., a panel at the top).
	 */
	for (int i = 0; i < 4; i++) {
		struct wlr_scene_node *node;
		wl_list_for_each(node, &layers[i]->children, link) {
			struct wlr_scene_tree *tree;
			struct wlr_scene_layer_surface_v1 *scene_layer;

			if (node->type != WLR_SCENE_NODE_TREE)
				continue;
			tree = wl_container_of(node, tree, node);
			scene_layer = tree->node.data;
			if (!scene_layer)
				continue;

			struct wlr_layer_surface_v1 *lsurface =
				scene_layer->layer_surface;
			if (lsurface->output != output->wlr_output)
				continue;
			if (lsurface->current.exclusive_zone <= 0)
				continue;

			wlr_scene_layer_surface_v1_configure(
				scene_layer, &full_area, &usable_area);
		}
	}

	/*
	 * Second pass: arrange surfaces without exclusive zones.
	 * These use the remaining usable area.
	 */
	for (int i = 0; i < 4; i++) {
		struct wlr_scene_node *node;
		wl_list_for_each(node, &layers[i]->children, link) {
			struct wlr_scene_tree *tree;
			struct wlr_scene_layer_surface_v1 *scene_layer;

			if (node->type != WLR_SCENE_NODE_TREE)
				continue;
			tree = wl_container_of(node, tree, node);
			scene_layer = tree->node.data;
			if (!scene_layer)
				continue;

			struct wlr_layer_surface_v1 *lsurface =
				scene_layer->layer_surface;
			if (lsurface->output != output->wlr_output)
				continue;
			if (lsurface->current.exclusive_zone > 0)
				continue;

			wlr_scene_layer_surface_v1_configure(
				scene_layer, &full_area, &usable_area);
		}
	}

	output->usable_area = usable_area;
}

/*
 * Focus the topmost layer surface that wants keyboard interactivity.
 * Overlay and top layers are checked first (lock screens, panels).
 */
bool
wm_layer_shell_focus_if_requested(struct wm_server *server)
{
	struct wlr_scene_tree *layers[] = {
		server->layer_overlay,
		server->layer_top,
	};

	for (int i = 0; i < 2; i++) {
		struct wlr_scene_node *node;
		wl_list_for_each_reverse(node, &layers[i]->children, link) {
			struct wlr_scene_tree *tree;
			struct wlr_scene_layer_surface_v1 *scene_layer;

			if (node->type != WLR_SCENE_NODE_TREE)
				continue;
			tree = wl_container_of(node, tree, node);
			scene_layer = tree->node.data;
			if (!scene_layer)
				continue;

			struct wlr_layer_surface_v1 *lsurface =
				scene_layer->layer_surface;
			if (!lsurface->surface->mapped)
				continue;

			enum zwlr_layer_surface_v1_keyboard_interactivity ki =
				lsurface->current.keyboard_interactive;
			if (ki == ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE)
				continue;

			/* Exclusive takes focus unconditionally */
			if (ki == ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE) {
				wlr_seat_keyboard_notify_enter(
					server->seat, lsurface->surface,
					NULL, 0, NULL);
				return true;
			}

			/* On-demand: only focus if no toplevel is focused */
			if (ki == ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND) {
				if (!server->focused_view) {
					wlr_seat_keyboard_notify_enter(
						server->seat,
						lsurface->surface,
						NULL, 0, NULL);
					return true;
				}
			}
		}
	}

	return false;
}

static void
handle_layer_surface_commit(struct wl_listener *listener, void *data)
{
	struct wm_layer_surface *ls =
		wl_container_of(listener, ls, commit);
	struct wlr_layer_surface_v1 *layer_surface = ls->layer_surface;

	if (!layer_surface->initialized)
		return;

	/*
	 * On initial commit, the layer surface is not yet configured.
	 * Send the initial configure with the desired size (or output
	 * dimensions if size is 0).
	 */
	if (layer_surface->initial_commit) {
		uint32_t width = layer_surface->current.desired_width;
		uint32_t height = layer_surface->current.desired_height;

		/* If anchored to opposite edges, use the output dimension */
		if (layer_surface->output) {
			struct wlr_box box;
			wlr_output_effective_resolution(
				layer_surface->output,
				&box.width, &box.height);

			uint32_t anchor = layer_surface->current.anchor;
			if (width == 0 &&
			    (anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT) &&
			    (anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT)) {
				width = (uint32_t)box.width;
			}
			if (height == 0 &&
			    (anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP) &&
			    (anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM)) {
				height = (uint32_t)box.height;
			}
		}

		wlr_layer_surface_v1_configure(layer_surface, width, height);
		return;
	}

	/* Re-arrange layers when state changes */
	if (layer_surface->output) {
		struct wm_output *output = output_from_wlr(
			ls->server, layer_surface->output);
		if (output) {
			wm_layer_shell_arrange(output);
		}
	}

	/* Handle focus changes */
	wm_layer_shell_focus_if_requested(ls->server);
}

static void
handle_layer_surface_map(struct wl_listener *listener, void *data)
{
	struct wm_layer_surface *ls =
		wl_container_of(listener, ls, map);

	wlr_log(WLR_INFO, "layer surface mapped: %s (layer %d)",
		ls->layer_surface->namespace,
		ls->layer_surface->current.layer);

	/* Arrange to account for new exclusive zones */
	if (ls->layer_surface->output) {
		struct wm_output *output = output_from_wlr(
			ls->server, ls->layer_surface->output);
		if (output)
			wm_layer_shell_arrange(output);
	}

	/* Check if this surface wants keyboard focus */
	wm_layer_shell_focus_if_requested(ls->server);
}

static void
handle_layer_surface_unmap(struct wl_listener *listener, void *data)
{
	struct wm_layer_surface *ls =
		wl_container_of(listener, ls, unmap);

	wlr_log(WLR_INFO, "layer surface unmapped: %s",
		ls->layer_surface->namespace);

	/* Re-arrange to reclaim exclusive zone space */
	if (ls->layer_surface->output) {
		struct wm_output *output = output_from_wlr(
			ls->server, ls->layer_surface->output);
		if (output)
			wm_layer_shell_arrange(output);
	}

	/* If this surface had keyboard focus, try to re-focus */
	struct wlr_surface *focused =
		ls->server->seat->keyboard_state.focused_surface;
	if (focused == ls->layer_surface->surface) {
		/* Try to focus another layer surface first, then fall back */
		if (!wm_layer_shell_focus_if_requested(ls->server)) {
			/* Clear keyboard focus - let normal focus logic take over */
			wlr_seat_keyboard_clear_focus(ls->server->seat);
		}
	}
}

static void
handle_layer_surface_destroy(struct wl_listener *listener, void *data)
{
	struct wm_layer_surface *ls =
		wl_container_of(listener, ls, destroy);

	wl_list_remove(&ls->map.link);
	wl_list_remove(&ls->unmap.link);
	wl_list_remove(&ls->destroy.link);
	wl_list_remove(&ls->new_popup.link);
	wl_list_remove(&ls->commit.link);
	wl_list_remove(&ls->link);

	free(ls);
}


/*
 * Handle a new popup created by a layer surface (e.g., a panel menu).
 */
static void
handle_layer_new_popup(struct wl_listener *listener, void *data)
{
	struct wm_layer_surface *ls =
		wl_container_of(listener, ls, new_popup);
	struct wlr_xdg_popup *popup = data;

	/* Create the popup's scene tree under the layer surface's popup tree */
	struct wlr_scene_tree *popup_tree =
		wlr_scene_xdg_surface_create(ls->popup_tree,
			popup->base);
	if (!popup_tree) {
		wlr_log(WLR_ERROR, "%s", "failed to create popup scene tree");
		return;
	}

	/* Position the popup relative to the layer surface */
	struct wlr_scene_node *layer_node = &ls->scene->tree->node;
	int lx = layer_node->x;
	int ly = layer_node->y;
	wlr_scene_node_set_position(&popup_tree->node, lx, ly);

	wlr_log(WLR_DEBUG, "new layer popup for %s",
		ls->layer_surface->namespace);
}

/*
 * Handle a new layer surface being created.
 * Assigns it to an output, creates the scene node, and sets up listeners.
 */
static void
handle_new_layer_surface(struct wl_listener *listener, void *data)
{
	struct wm_server *server =
		wl_container_of(listener, server, new_layer_surface);
	struct wlr_layer_surface_v1 *layer_surface = data;

	/*
	 * If the client didn't specify an output, assign the first
	 * available one (typically the focused output).
	 */
	if (!layer_surface->output) {
		struct wm_output *output;
		wl_list_for_each(output, &server->outputs, link) {
			layer_surface->output = output->wlr_output;
			break;
		}
		if (!layer_surface->output) {
			wlr_log(WLR_ERROR,
				"no output for layer surface %s",
				layer_surface->namespace);
			wlr_layer_surface_v1_destroy(layer_surface);
			return;
		}
	}

	struct wm_layer_surface *ls = calloc(1, sizeof(*ls));
	if (!ls) {
		wlr_log(WLR_ERROR,
			"%s", "failed to allocate layer surface");
		wlr_layer_surface_v1_destroy(layer_surface);
		return;
	}

	ls->server = server;
	ls->layer_surface = layer_surface;

	/* Determine which scene tree layer to place this in */
	struct wlr_scene_tree *parent_tree =
		get_layer_tree(server, layer_surface->pending.layer);

	/* Create the scene layer surface node */
	ls->scene = wlr_scene_layer_surface_v1_create(
		parent_tree, layer_surface);
	if (!ls->scene) {
		wlr_log(WLR_ERROR,
			"%s", "failed to create scene layer surface");
		free(ls);
		wlr_layer_surface_v1_destroy(layer_surface);
		return;
	}

	/* Store back-reference for hit testing */
	ls->scene->tree->node.data = ls->scene;

	/* Create a tree for popups from this layer surface */
	ls->popup_tree = wlr_scene_tree_create(parent_tree);

	/* Set up event listeners */
	ls->map.notify = handle_layer_surface_map;
	wl_signal_add(&layer_surface->surface->events.map, &ls->map);

	ls->unmap.notify = handle_layer_surface_unmap;
	wl_signal_add(&layer_surface->surface->events.unmap, &ls->unmap);

	ls->destroy.notify = handle_layer_surface_destroy;
	wl_signal_add(&layer_surface->events.destroy, &ls->destroy);

	ls->new_popup.notify = handle_layer_new_popup;
	wl_signal_add(&layer_surface->events.new_popup, &ls->new_popup);

	ls->commit.notify = handle_layer_surface_commit;
	wl_signal_add(&layer_surface->surface->events.commit, &ls->commit);

	wl_list_insert(&server->layer_surfaces, &ls->link);

	wlr_log(WLR_INFO,
		"new layer surface: %s (layer %d, output %s)",
		layer_surface->namespace,
		layer_surface->pending.layer,
		layer_surface->output->name);
}

void
wm_layer_shell_init(struct wm_server *server)
{
	wl_list_init(&server->layer_surfaces);

	/* Create the layer shell protocol global (version 4) */
	server->layer_shell = wlr_layer_shell_v1_create(
		server->wl_display, 4);
	if (!server->layer_shell) {
		wlr_log(WLR_ERROR, "%s", "failed to create layer shell");
		return;
	}

	server->new_layer_surface.notify = handle_new_layer_surface;
	wl_signal_add(&server->layer_shell->events.new_surface,
		&server->new_layer_surface);

	wlr_log(WLR_INFO, "%s", "layer shell protocol initialized");
}

void
wm_layer_shell_finish(struct wm_server *server)
{
	wl_list_remove(&server->new_layer_surface.link);
}
