/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * workspace.h - Virtual desktop / workspace management
 */

#ifndef WM_WORKSPACE_H
#define WM_WORKSPACE_H

#include <wayland-server-core.h>
#include <wlr/types/wlr_scene.h>

struct wm_server;
struct wm_view;
struct wm_output;

#define WM_DEFAULT_WORKSPACE_COUNT 4

struct wm_workspace {
	struct wl_list link; /* wm_server.workspaces */
	struct wm_server *server;
	struct wlr_scene_tree *tree; /* scene tree for this workspace */
	char *name;
	int index;
};

/*
 * Initialize workspaces. Creates workspace_count workspaces
 * under server->view_tree, sets the first one as active.
 */
void wm_workspaces_init(struct wm_server *server, int count);

/* Clean up all workspaces */
void wm_workspaces_finish(struct wm_server *server);

/*
 * Switch to a workspace by index (0-based).
 * Disables old workspace's scene tree, enables new one.
 * Focuses the topmost view on the new workspace.
 */
void wm_workspace_switch(struct wm_server *server, int index);

/* Switch to the next workspace (wraps around) */
void wm_workspace_switch_next(struct wm_server *server);

/* Switch to the previous workspace (wraps around) */
void wm_workspace_switch_prev(struct wm_server *server);

/* Get workspace by index */
struct wm_workspace *wm_workspace_get(struct wm_server *server, int index);

/* Move a view to a different workspace */
void wm_view_move_to_workspace(struct wm_view *view,
	struct wm_workspace *workspace);

/* Send the focused view to a workspace by index */
void wm_view_send_to_workspace(struct wm_server *server, int index);

/* Toggle sticky (visible on all workspaces) */
void wm_view_set_sticky(struct wm_view *view, bool sticky);

/* Send focused view to workspace, then switch to it */
void wm_view_take_to_workspace(struct wm_server *server, int index);

/* Send focused view to next/prev workspace (wrapping) */
void wm_view_send_to_next_workspace(struct wm_server *server);
void wm_view_send_to_prev_workspace(struct wm_server *server);

/* Send focused view to next/prev workspace and follow */
void wm_view_take_to_next_workspace(struct wm_server *server);
void wm_view_take_to_prev_workspace(struct wm_server *server);

/* Add a new workspace */
void wm_workspace_add(struct wm_server *server);

/* Remove the last workspace (moves views to previous) */
void wm_workspace_remove_last(struct wm_server *server);

/* Switch to the next workspace (no wrap at boundary) */
void wm_workspace_switch_right(struct wm_server *server);

/* Switch to the previous workspace (no wrap at boundary) */
void wm_workspace_switch_left(struct wm_server *server);

/* Set the name of the current workspace */
void wm_workspace_set_name(struct wm_server *server, const char *name);

/*
 * Get the effective active workspace, accounting for per-output mode.
 * In per-output mode, returns the active workspace of the focused output.
 * In global mode, returns server->current_workspace.
 */
struct wm_workspace *wm_workspace_get_active(struct wm_server *server);

#endif /* WM_WORKSPACE_H */
