/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * cursor_actions.c - Cursor context detection and mouse action dispatch
 *
 * Determines click context from scene-graph hit-testing and decoration
 * regions, and dispatches mouse binding actions.
 */

#define _GNU_SOURCE
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/edges.h>

#include "config.h"
#include "cursor_actions.h"
#include "server.h"
#include "decoration.h"
#include "keybind.h"
#include "menu.h"
#include "mousebind.h"
#include "slit.h"
#include "tabgroup.h"
#include "util.h"
#include "view.h"
#include "workspace.h"

/*
 * Validate that node.data is actually a wm_view by checking the
 * server's view list. Returns the view if valid, NULL otherwise.
 * This prevents type confusion when node.data is set by layer_shell,
 * toolbar, menu, or other non-view scene nodes.
 */
static struct wm_view *
validate_view_data(struct wm_server *server, void *data)
{
	if (!data)
		return NULL;
	struct wm_view *v;
	wl_list_for_each(v, &server->views, link) {
		if (v == data)
			return v;
	}
	return NULL;
}

/*
 * Find the view under the cursor, returning the surface and
 * surface-local coordinates. Returns NULL if no view is found.
 */
struct wm_view *
view_at(struct wm_server *server, double lx, double ly,
	struct wlr_surface **surface, double *sx, double *sy)
{
	struct wlr_scene_node *node =
		wlr_scene_node_at(&server->scene->tree.node, lx, ly, sx, sy);
	if (!node || node->type != WLR_SCENE_NODE_BUFFER) {
		return NULL;
	}

	struct wlr_scene_buffer *scene_buffer =
		wlr_scene_buffer_from_node(node);
	struct wlr_scene_surface *scene_surface =
		wlr_scene_surface_try_from_buffer(scene_buffer);
	if (!scene_surface) {
		/* Could be a decoration buffer — walk up to find view */
		struct wlr_scene_tree *tree = node->parent;
		while (tree && !tree->node.data) {
			tree = tree->node.parent;
		}
		if (surface)
			*surface = NULL;
		return tree ? validate_view_data(server,
			tree->node.data) : NULL;
	}

	*surface = scene_surface->surface;

	struct wlr_scene_tree *tree = node->parent;
	while (tree && !tree->node.data) {
		tree = tree->node.parent;
	}
	return tree ? validate_view_data(server,
		tree->node.data) : NULL;
}

/*
 * Map a decoration region to a mouse context.
 */
enum wm_mouse_context
region_to_context(enum wm_decor_region region)
{
	switch (region) {
	case WM_DECOR_REGION_TITLEBAR:
	case WM_DECOR_REGION_LABEL:
	case WM_DECOR_REGION_BUTTON:
		return WM_MOUSE_CTX_TITLEBAR;
	case WM_DECOR_REGION_HANDLE:
		return WM_MOUSE_CTX_WINDOW_BORDER;
	case WM_DECOR_REGION_GRIP_LEFT:
		return WM_MOUSE_CTX_LEFT_GRIP;
	case WM_DECOR_REGION_GRIP_RIGHT:
		return WM_MOUSE_CTX_RIGHT_GRIP;
	case WM_DECOR_REGION_BORDER_TOP:
	case WM_DECOR_REGION_BORDER_BOTTOM:
	case WM_DECOR_REGION_BORDER_LEFT:
	case WM_DECOR_REGION_BORDER_RIGHT:
		return WM_MOUSE_CTX_WINDOW_BORDER;
	case WM_DECOR_REGION_CLIENT:
		return WM_MOUSE_CTX_WINDOW;
	case WM_DECOR_REGION_NONE:
	default:
		return WM_MOUSE_CTX_NONE;
	}
}

/*
 * Determine the mouse context at the current cursor position.
 * Returns the context and optionally the view under the cursor.
 */
enum wm_mouse_context
get_cursor_context(struct wm_server *server, struct wm_view **out_view)
{
	double sx, sy;
	struct wlr_surface *surface = NULL;
	struct wm_view *view = view_at(server,
		server->cursor->x, server->cursor->y, &surface, &sx, &sy);

	if (out_view)
		*out_view = view;

	if (!view) {
		/* Check if cursor is over the slit */
		if (server->slit && server->slit->visible &&
		    !server->slit->hidden) {
			double cx = server->cursor->x;
			double cy = server->cursor->y;
			if (cx >= server->slit->x &&
			    cx < server->slit->x + server->slit->width &&
			    cy >= server->slit->y &&
			    cy < server->slit->y + server->slit->height) {
				return WM_MOUSE_CTX_SLIT;
			}
		}
		return WM_MOUSE_CTX_DESKTOP;
	}

	/* Check decoration hit region */
	if (view->decoration) {
		/* Convert layout coords to decoration-local coords */
		double dx = server->cursor->x - view->x;
		double dy = server->cursor->y - view->y;
		enum wm_decor_region region =
			wm_decoration_region_at(view->decoration, dx, dy);
		if (region != WM_DECOR_REGION_NONE &&
		    region != WM_DECOR_REGION_CLIENT) {
			return region_to_context(region);
		}
	}

	/* On the client surface itself */
	return WM_MOUSE_CTX_WINDOW;
}

/*
 * Get current keyboard modifiers from the first keyboard.
 */
uint32_t
get_keyboard_modifiers(struct wm_server *server)
{
	struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(server->seat);
	if (keyboard)
		return wlr_keyboard_get_modifiers(keyboard);
	return 0;
}

/*
 * Execute a mouse action (simplified version for cursor dispatch).
 */
void
execute_mouse_action(struct wm_server *server,
	enum wm_action action, const char *argument, struct wm_view *view)
{
	switch (action) {
	case WM_ACTION_RAISE:
		if (view)
			wm_view_raise(view);
		break;

	case WM_ACTION_FOCUS:
		if (view) {
			server->focus_user_initiated = true;
			wm_focus_view(view, NULL);
		}
		break;

	case WM_ACTION_START_MOVING:
		if (view)
			wm_view_begin_interactive(view, WM_CURSOR_MOVE, 0);
		break;

	case WM_ACTION_START_RESIZING:
		if (view) {
			/* Determine edges based on cursor position */
			struct wlr_box geo;
			wm_view_get_geometry(view, &geo);
			double cx = server->cursor->x - view->x;
			double cy = server->cursor->y - view->y;
			uint32_t edges = 0;
			if (cx < geo.width / 2)
				edges |= WLR_EDGE_LEFT;
			else
				edges |= WLR_EDGE_RIGHT;
			if (cy < geo.height / 2)
				edges |= WLR_EDGE_TOP;
			else
				edges |= WLR_EDGE_BOTTOM;
			wm_view_begin_interactive(view,
				WM_CURSOR_RESIZE, edges);
		}
		break;

	case WM_ACTION_START_TABBING:
		if (view)
			wm_view_begin_interactive(view,
				WM_CURSOR_TABBING, 0);
		break;

	case WM_ACTION_ACTIVATE_TAB:
		if (view && view->tab_group &&
		    view->tab_group->count > 1 && view->decoration) {
			double dx = server->cursor->x - view->x;
			double dy = server->cursor->y - view->y;
			int tab_idx = wm_decoration_tab_at(
				view->decoration, dx, dy);
			if (tab_idx >= 0) {
				int i = 0;
				struct wm_view *tab_view;
				wl_list_for_each(tab_view,
						&view->tab_group->views,
						tab_link) {
					if (i == tab_idx) {
						wm_tab_group_activate(
							view->tab_group,
							tab_view);
						break;
					}
					i++;
				}
			}
		}
		break;

	case WM_ACTION_CLOSE:
		if (view)
			wlr_xdg_toplevel_send_close(view->xdg_toplevel);
		break;

	case WM_ACTION_MAXIMIZE:
		if (view) {
			struct wl_listener *l = &view->request_maximize;
			l->notify(l, NULL);
		}
		break;

	case WM_ACTION_MINIMIZE:
		if (view) {
			struct wl_listener *l = &view->request_minimize;
			l->notify(l, NULL);
		}
		break;

	case WM_ACTION_SHADE:
		if (view && view->decoration && server->style)
			wm_decoration_set_shaded(view->decoration,
				!view->decoration->shaded, server->style);
		break;

	case WM_ACTION_STICK:
		if (view)
			wm_view_set_sticky(view, !view->sticky);
		break;

	case WM_ACTION_LOWER:
		if (view)
			wm_view_lower(view);
		break;

	case WM_ACTION_NEXT_WORKSPACE:
		wm_workspace_switch_next(server);
		break;

	case WM_ACTION_PREV_WORKSPACE:
		wm_workspace_switch_prev(server);
		break;

	case WM_ACTION_WORKSPACE:
		if (argument) {
			int ws;
			if (safe_atoi(argument, &ws))
				wm_workspace_switch(server, ws - 1);
		}
		break;

	case WM_ACTION_EXEC:
		wm_spawn_command(argument);
		break;

	case WM_ACTION_ROOT_MENU:
		wm_menu_show_root(server,
			(int)server->cursor->x, (int)server->cursor->y);
		break;

	case WM_ACTION_WINDOW_MENU:
		wm_menu_show_window(server,
			(int)server->cursor->x, (int)server->cursor->y);
		break;

	case WM_ACTION_HIDE_MENUS:
		wm_menu_hide_all(server);
		break;

	default:
		/* Remaining actions are stubs or keyboard-only */
		break;
	}
}

/*
 * Dispatch a mouse binding: MacroCmd and ToggleCmd are handled here,
 * other actions delegate to execute_mouse_action.
 */
void
execute_mousebind(struct wm_server *server, struct wm_mousebind *bind,
	struct wm_view *view)
{
	if (bind->action == WM_ACTION_MACRO_CMD) {
		struct wm_subcmd *cmd = bind->subcmds;
		while (cmd) {
			execute_mouse_action(server, cmd->action,
				cmd->argument, view);
			cmd = cmd->next;
		}
		return;
	}

	if (bind->action == WM_ACTION_TOGGLE_CMD) {
		if (bind->subcmd_count > 0) {
			struct wm_subcmd *cmd = bind->subcmds;
			int idx = bind->toggle_index % bind->subcmd_count;
			for (int i = 0; i < idx && cmd; i++)
				cmd = cmd->next;
			if (cmd) {
				execute_mouse_action(server, cmd->action,
					cmd->argument, view);
			}
			bind->toggle_index =
				(bind->toggle_index + 1) % bind->subcmd_count;
		}
		return;
	}

	execute_mouse_action(server, bind->action, bind->argument, view);
}
