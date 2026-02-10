/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * layer_shell.h - wlr-layer-shell-unstable-v1 protocol support
 *
 * Handles external panels (waybar), wallpaper setters (swaybg),
 * screen lockers, and notification daemons.
 */

#ifndef WM_LAYER_SHELL_H
#define WM_LAYER_SHELL_H

#include <wayland-server-core.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/box.h>

struct wm_server;
struct wm_output;

#define WM_LAYER_COUNT 4

struct wm_layer_surface {
	struct wm_server *server;
	struct wlr_layer_surface_v1 *layer_surface;
	struct wlr_scene_layer_surface_v1 *scene;
	struct wlr_scene_tree *popup_tree;

	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener destroy;
	struct wl_listener new_popup;
	struct wl_listener commit;

	struct wl_list link; /* wm_server.layer_surfaces */
};

/*
 * Initialize layer shell protocol support.
 * Creates the wlr_layer_shell_v1 global and sets up the new_surface listener.
 * Must be called during server init, after scene graph creation.
 */
void wm_layer_shell_init(struct wm_server *server);

/*
 * Clean up layer shell resources.
 */
void wm_layer_shell_finish(struct wm_server *server);

/*
 * Arrange all layer surfaces on the given output.
 * Computes exclusive zones and updates the output's usable_area.
 * Called on layer surface map/unmap/commit and output resize.
 */
void wm_layer_shell_arrange(struct wm_output *output);

/*
 * Try to focus a layer surface that requests keyboard interactivity.
 * Returns true if a layer surface was focused.
 */
bool wm_layer_shell_focus_if_requested(struct wm_server *server);

#endif /* WM_LAYER_SHELL_H */
