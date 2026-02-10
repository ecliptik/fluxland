/*
 * wm-wayland - A Fluxbox-inspired Wayland compositor
 * view.h - Window/view management (XDG shell clients)
 */

#ifndef WM_VIEW_H
#define WM_VIEW_H

#include <wayland-server-core.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/box.h>

struct wm_server;
struct wm_workspace;
struct wm_tab_group;
struct wm_decoration;
struct wlr_foreign_toplevel_handle_v1;

/* Fluxbox-inspired window layers (bottom to top) */
enum wm_view_layer {
	WM_LAYER_DESKTOP = 0,
	WM_LAYER_BELOW,
	WM_LAYER_NORMAL,
	WM_LAYER_ABOVE,
	WM_VIEW_LAYER_COUNT, /* sentinel */
};

struct wm_view {
	struct wl_list link; /* wm_server.views */
	struct wm_server *server;
	struct wlr_xdg_toplevel *xdg_toplevel;
	struct wlr_scene_tree *scene_tree;

	/* Geometry (layout coordinates) */
	int x, y;

	/* Saved geometry for maximize/fullscreen restore */
	struct wlr_box saved_geometry;
	bool maximized;
	bool fullscreen;
	bool maximized_vert;
	bool maximized_horiz;

	/* Decoration visibility toggle */
	bool show_decoration; /* default true */

	/* Metadata */
	char *title;
	char *app_id;

	/* Workspace tracking */
	struct wm_workspace *workspace;
	bool sticky; /* visible on all workspaces */

	/* Layer stacking (Fluxbox layer model) */
	enum wm_view_layer layer;

	/* Tab group (Fluxbox-style window grouping) */
	struct wm_tab_group *tab_group;
	struct wl_list tab_link; /* wm_tab_group.views */

	/* Window opacity (0-255, 255 = fully opaque) */
	int focus_alpha;
	int unfocus_alpha;

	/* Foreign toplevel handle (for external taskbars) */
	struct wlr_foreign_toplevel_handle_v1 *foreign_toplevel_handle;
	struct wl_listener foreign_toplevel_request_activate;
	struct wl_listener foreign_toplevel_request_maximize;
	struct wl_listener foreign_toplevel_request_minimize;
	struct wl_listener foreign_toplevel_request_fullscreen;
	struct wl_listener foreign_toplevel_request_close;

	/* Server-side decorations */
	struct wm_decoration *decoration;

	/* XDG toplevel listeners */
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener commit;
	struct wl_listener destroy;
	struct wl_listener request_move;
	struct wl_listener request_resize;
	struct wl_listener request_maximize;
	struct wl_listener request_fullscreen;
	struct wl_listener request_minimize;
	struct wl_listener set_title;
	struct wl_listener set_app_id;
};

/*
 * Initialize the XDG shell listeners on the server.
 * Must be called after wm_server_init().
 */
void wm_view_init(struct wm_server *server);

/*
 * Find the topmost view at the given layout coordinates.
 * Returns the view and sets surface/sx/sy for the sub-surface hit.
 */
struct wm_view *wm_view_at(struct wm_server *server, double lx, double ly,
	struct wlr_surface **surface, double *sx, double *sy);

/*
 * Focus the given view. Deactivates the previously focused view,
 * raises view to top, and sends keyboard enter.
 */
void wm_focus_view(struct wm_view *view, struct wlr_surface *surface);

/*
 * Remove focus from the currently focused view and clear keyboard focus.
 */
void wm_unfocus_current(struct wm_server *server);

/*
 * Focus the next or previous view in the stacking order.
 * Used for Alt-Tab / window cycling.
 */
void wm_focus_next_view(struct wm_server *server);

/*
 * Raise the given view to the top of the stacking order.
 */
void wm_view_raise(struct wm_view *view);

/*
 * Lower the given view to the bottom of its layer's stacking order.
 */
void wm_view_lower(struct wm_view *view);

/*
 * Set the view's layer and reparent its scene node accordingly.
 */
void wm_view_set_layer(struct wm_view *view, enum wm_view_layer layer);

/*
 * Move the view up one layer (e.g. Normal -> Above).
 */
void wm_view_raise_layer(struct wm_view *view);

/*
 * Move the view down one layer (e.g. Normal -> Below).
 */
void wm_view_lower_layer(struct wm_view *view);

/*
 * Parse a layer name string (e.g. "Above", "Normal") to enum.
 * Returns WM_LAYER_NORMAL if unrecognized.
 */
enum wm_view_layer wm_view_layer_from_name(const char *name);

/*
 * Called on cursor motion: if sloppy focus is enabled, focus
 * the view under the pointer without raising it.
 */
void wm_focus_update_for_cursor(struct wm_server *server,
	double cursor_x, double cursor_y);

/*
 * Get the bounding box of the view (including XDG geometry offset).
 */
void wm_view_get_geometry(struct wm_view *view, struct wlr_box *box);

/*
 * Begin an interactive move or resize for the given view.
 */
void wm_view_begin_interactive(struct wm_view *view,
	enum wm_cursor_mode mode, uint32_t edges);

/*
 * Set the opacity of a view's scene buffers.
 * alpha is 0-255 (0 = transparent, 255 = fully opaque).
 */
void wm_view_set_opacity(struct wm_view *view, int alpha);

/*
 * Toggle vertical maximization (fill output height, keep x/width).
 */
void wm_view_maximize_vert(struct wm_view *view);

/*
 * Toggle horizontal maximization (fill output width, keep y/height).
 */
void wm_view_maximize_horiz(struct wm_view *view);

/*
 * Toggle decoration visibility on/off.
 */
void wm_view_toggle_decoration(struct wm_view *view);

/*
 * Activate the nth tab (0-based index) in the view's tab group.
 */
void wm_view_activate_tab(struct wm_view *view, int index);

#endif /* WM_VIEW_H */
