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

#include "command.h"
#include "config.h"
#include "keyboard_actions.h"
#include "cursor.h"
#include "cursor_actions.h"
#include "server.h"
#include "decoration.h"
#include "keybind.h"
#include "menu.h"
#include "mousebind.h"
#include "slit.h"
#include "tabgroup.h"
#include "toolbar.h"
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
 * The mouse contexts of a decoration region (FbWinFrame::getContext):
 * the label holds the tabs, so it is OnTab and OnTitlebar; the middle
 * of the handle is OnWindowBorder and OnWindow. Titlebar buttons have no
 * context of their own until they are released (OnWinButton...).
 */
uint32_t
region_to_context(enum wm_decor_region region)
{
	switch (region) {
	case WM_DECOR_REGION_TITLEBAR:
		return WM_MOUSE_CTX_TITLEBAR;
	case WM_DECOR_REGION_LABEL:
		return WM_MOUSE_CTX_TITLEBAR | WM_MOUSE_CTX_TAB;
	case WM_DECOR_REGION_HANDLE:
		return WM_MOUSE_CTX_WINDOW_BORDER | WM_MOUSE_CTX_WINDOW;
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
	case WM_DECOR_REGION_BUTTON:
	case WM_DECOR_REGION_NONE:
	default:
		return 0;
	}
}

/*
 * The mouse contexts at the cursor, and the view under it. The desktop
 * is OnDesktop and also where bindings without On* apply.
 */
uint32_t
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
		return WM_MOUSE_CTX_GLOBAL | WM_MOUSE_CTX_DESKTOP;
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
		return wlr_keyboard_get_modifiers(keyboard) &
			~WM_MODIFIERS_IGNORED;
	return 0;
}

/*
 * Execute a single action on the window under the cursor. Mouse
 * bindings share the dispatcher (and its action set) with key
 * bindings and IPC.
 */
void
execute_mouse_action(struct wm_server *server,
	enum wm_action action, const char *argument, struct wm_view *view)
{
	wm_execute_action_on(server, action, argument, view);
}

/*
 * Dispatch a mouse binding. Window commands act on the window that
 * was clicked (Fluxbox's OnWindow semantics), not the focused one.
 */
void
execute_mousebind(struct wm_server *server, struct wm_mousebind *bind,
	struct wm_view *view)
{
	wm_command_execute(server, bind->command, view);
}

void
wm_cursor_run_event(struct wm_server *server, uint32_t context,
	int event, struct wm_view *view)
{
	/* Commands here can focus or move the pointer's window, which
	 * fires events again: X delivers those later, here they nest */
	static int depth;
	if (depth > 4)
		return;
	uint32_t mods = event == WM_MOUSE_OVER || event == WM_MOUSE_OUT ?
		get_keyboard_modifiers(server) : 0;
	struct wm_mousebind *bind = mousebind_find(&server->mousebindings,
		context, (enum wm_mouse_event)event, 0, mods);
	if (!bind)
		return;
	depth++;
	execute_mousebind(server, bind, view);
	depth--;
}

void
update_pointer_crossing(struct wm_server *server, struct wm_view *view)
{
	struct wm_mouse_state *ms = &server->mouse_state;
	double cx = server->cursor->x, cy = server->cursor->y;
	uint32_t ctx = 0;
	if (wm_toolbar_contains(server->toolbar, cx, cy)) {
		ctx = WM_MOUSE_CTX_TOOLBAR;
		view = NULL;
	} else if (view) {
		ctx = WM_MOUSE_CTX_WINDOW;
	} else if (server->slit && server->slit->visible &&
		   cx >= server->slit->x &&
		   cx < server->slit->x + server->slit->width &&
		   cy >= server->slit->y &&
		   cy < server->slit->y + server->slit->height) {
		ctx = WM_MOUSE_CTX_SLIT;
	}
	if (ctx == ms->over_context && view == ms->over_view)
		return;

	uint32_t old_ctx = ms->over_context;
	struct wm_view *old_view = ms->over_view;
	ms->over_context = ctx;
	ms->over_view = view;
	if (old_ctx)
		wm_cursor_run_event(server, old_ctx, WM_MOUSE_OUT, old_view);
	/* The MouseOut command may have closed the new window */
	if (ctx && ms->over_context == ctx && ms->over_view == view)
		wm_cursor_run_event(server, ctx, WM_MOUSE_OVER, view);
}
