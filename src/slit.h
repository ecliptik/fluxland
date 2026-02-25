/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 *
 * slit.h - Fluxbox slit dockapp container
 *
 * The slit is a container panel that holds small dockapp utilities.
 * In X11 Fluxbox, withdrawn windows dock into the slit.  For our
 * Wayland compositor, XWayland withdrawn/dock windows are routed here.
 *
 * Configurable placement (12 positions around screen edges), direction
 * (horizontal/vertical), and auto-hide support.
 */

#ifndef WM_SLIT_H
#define WM_SLIT_H

#include <stdbool.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/box.h>

struct wm_server;

/* Slit placement around screen edges (Fluxbox-compatible) */
enum wm_slit_placement {
	WM_SLIT_TOP_LEFT,
	WM_SLIT_TOP_CENTER,
	WM_SLIT_TOP_RIGHT,
	WM_SLIT_RIGHT_TOP,
	WM_SLIT_RIGHT_CENTER,
	WM_SLIT_RIGHT_BOTTOM,
	WM_SLIT_BOTTOM_LEFT,
	WM_SLIT_BOTTOM_CENTER,
	WM_SLIT_BOTTOM_RIGHT,
	WM_SLIT_LEFT_TOP,
	WM_SLIT_LEFT_CENTER,
	WM_SLIT_LEFT_BOTTOM,
};

/* Slit layout direction */
enum wm_slit_direction {
	WM_SLIT_VERTICAL,
	WM_SLIT_HORIZONTAL,
};

/* A single client docked into the slit */
struct wm_slit_client {
	struct wm_slit *slit;
	struct wlr_scene_tree *scene_tree;

#ifdef WM_HAS_XWAYLAND
	struct wlr_xwayland_surface *xsurface;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener destroy;
	struct wl_listener configure;
#endif

	int width, height;
	bool mapped;

	struct wl_list link; /* wm_slit.clients */
};

/* The slit container */
struct wm_slit {
	struct wm_server *server;

	/* Scene graph nodes */
	struct wlr_scene_tree *scene_tree;
	struct wlr_scene_rect *bg_rect;
	struct wlr_scene_rect *border_rect;

	/* Configuration */
	enum wm_slit_placement placement;
	enum wm_slit_direction direction;
	bool auto_hide;
	int on_head; /* output index, 0 = primary */
	int alpha;   /* 0-255, default 255 */

	/* Geometry */
	int x, y;
	int width, height;
	bool visible;
	bool hidden; /* auto-hide: currently hidden */

	/* Docked clients */
	struct wl_list clients; /* wm_slit_client.link */
	int client_count;

	/* Auto-hide timer */
	struct wl_event_source *hide_timer;

	/* MaxOver usable_area tracking */
	int reserved_space; /* pixels subtracted from usable_area */

#ifdef WM_HAS_XWAYLAND
	/* Slitlist: ordered WM_CLASS names for persistent dockapp ordering */
	char **slitlist;
	int slitlist_count;
#endif
};

/* Create the slit and add it to the scene graph */
struct wm_slit *wm_slit_create(struct wm_server *server);

/* Destroy the slit and free resources */
void wm_slit_destroy(struct wm_slit *slit);

/* Add a client window to the slit */
struct wm_slit_client *wm_slit_add_client(struct wm_slit *slit,
	void *surface);

/* Remove a client from the slit */
void wm_slit_remove_client(struct wm_slit_client *client);

/* Recalculate slit layout after clients are added/removed */
void wm_slit_reconfigure(struct wm_slit *slit);

/* Handle pointer entering slit area (for auto-hide) */
bool wm_slit_handle_pointer_enter(struct wm_slit *slit,
	double lx, double ly);

/* Handle pointer leaving slit area (for auto-hide) */
void wm_slit_handle_pointer_leave(struct wm_slit *slit);

/* Toggle slit between top layer and overlay layer (above windows) */
void wm_slit_toggle_above(struct wm_slit *slit);

/* Toggle slit auto-hide on/off */
void wm_slit_toggle_hidden(struct wm_slit *slit);

#endif /* WM_SLIT_H */
