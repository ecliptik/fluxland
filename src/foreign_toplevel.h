/*
 * wm-wayland - A Fluxbox-inspired Wayland compositor
 * foreign_toplevel.h - wlr-foreign-toplevel-management-v1 for taskbars
 */

#ifndef WM_FOREIGN_TOPLEVEL_H
#define WM_FOREIGN_TOPLEVEL_H

#include <wayland-server-core.h>

struct wm_server;
struct wm_view;

/* Initialize the foreign toplevel manager global */
void wm_foreign_toplevel_init(struct wm_server *server);

/* Create a foreign toplevel handle for a view (call on map) */
void wm_foreign_toplevel_handle_create(struct wm_view *view);

/* Destroy a view's foreign toplevel handle (call on unmap/destroy) */
void wm_foreign_toplevel_handle_destroy(struct wm_view *view);

/* Update title on the handle */
void wm_foreign_toplevel_update_title(struct wm_view *view);

/* Update app_id on the handle */
void wm_foreign_toplevel_update_app_id(struct wm_view *view);

/* Update activated state */
void wm_foreign_toplevel_set_activated(struct wm_view *view, bool activated);

/* Update maximized state */
void wm_foreign_toplevel_set_maximized(struct wm_view *view, bool maximized);

/* Update fullscreen state */
void wm_foreign_toplevel_set_fullscreen(struct wm_view *view, bool fullscreen);

/* Update minimized state */
void wm_foreign_toplevel_set_minimized(struct wm_view *view, bool minimized);

/* Update output (call when the view moves between outputs) */
void wm_foreign_toplevel_update_output(struct wm_view *view);

#endif /* WM_FOREIGN_TOPLEVEL_H */
