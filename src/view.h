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

	/* Metadata */
	char *title;
	char *app_id;

	/* Workspace tracking */
	struct wm_workspace *workspace;
	bool sticky; /* visible on all workspaces */

	/* Tab group (Fluxbox-style window grouping) */
	struct wm_tab_group *tab_group;
	struct wl_list tab_link; /* wm_tab_group.views */

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

#endif /* WM_VIEW_H */
