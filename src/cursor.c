/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * cursor.c - Pointer/cursor input handling
 *
 * Handles mouse events with context-sensitive binding dispatch.
 * Determines click context from decoration hit-testing, looks up
 * mouse bindings, and dispatches actions.
 */

#define _POSIX_C_SOURCE 200809L
#include <math.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <linux/input-event-codes.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_touch.h>
#include <wlr/types/wlr_pointer_gestures_v1.h>
#include <wlr/types/wlr_relative_pointer_v1.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/edges.h>
#include <wlr/util/log.h>

#include "config.h"
#include "cursor.h"
#include "server.h"
#include "decoration.h"
#include "idle.h"
#include "menu.h"
#include "placement.h"
#include "session_lock.h"
#include "toolbar.h"
#include "view.h"
#include "workspace.h"

/* Forward declaration — execute_action is in keyboard.c.
 * For mouse binding dispatch we replicate a simpler version here
 * that handles the mouse-specific subset of actions. */

/*
 * Find the view under the cursor, returning the surface and
 * surface-local coordinates. Returns NULL if no view is found.
 */
static struct wm_view *
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
		return tree ? tree->node.data : NULL;
	}

	*surface = scene_surface->surface;

	struct wlr_scene_tree *tree = node->parent;
	while (tree && !tree->node.data) {
		tree = tree->node.parent;
	}
	return tree ? tree->node.data : NULL;
}

/*
 * Map a decoration region to a mouse context.
 */
static enum wm_mouse_context
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
static enum wm_mouse_context
get_cursor_context(struct wm_server *server, struct wm_view **out_view)
{
	double sx, sy;
	struct wlr_surface *surface = NULL;
	struct wm_view *view = view_at(server,
		server->cursor->x, server->cursor->y, &surface, &sx, &sy);

	if (out_view)
		*out_view = view;

	if (!view)
		return WM_MOUSE_CTX_DESKTOP;

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
static uint32_t
get_keyboard_modifiers(struct wm_server *server)
{
	struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(server->seat);
	if (keyboard)
		return wlr_keyboard_get_modifiers(keyboard);
	return 0;
}

/*
 * Execute a mouse binding action (simplified version for cursor dispatch).
 * MacroCmd and ToggleCmd are handled here. Other actions delegate
 * to a simple action executor.
 */
static void execute_mouse_action(struct wm_server *server,
	enum wm_action action, const char *argument, struct wm_view *view);

static void
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

static void
execute_mouse_action(struct wm_server *server,
	enum wm_action action, const char *argument, struct wm_view *view)
{
	switch (action) {
	case WM_ACTION_RAISE:
		if (view)
			wm_view_raise(view);
		break;

	case WM_ACTION_FOCUS:
		if (view)
			wm_focus_view(view, NULL);
		break;

	case WM_ACTION_START_MOVING:
		if (view)
			wm_view_begin_interactive(view, WM_CURSOR_MOVE, 0);
		break;

	case WM_ACTION_START_RESIZING:
		if (view) {
			/* Determine edges based on cursor position */
			uint32_t edges = WLR_EDGE_BOTTOM | WLR_EDGE_RIGHT;
			struct wlr_box geo;
			wm_view_get_geometry(view, &geo);
			double cx = server->cursor->x - view->x;
			double cy = server->cursor->y - view->y;
			edges = 0;
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
			int ws = atoi(argument) - 1;
			wm_workspace_switch(server, ws);
		}
		break;

	case WM_ACTION_EXEC: {
		if (argument && *argument) {
			pid_t pid = fork();
			if (pid < 0) break;
			if (pid == 0) {
				sigset_t set;
				sigemptyset(&set);
				sigprocmask(SIG_SETMASK, &set, NULL);
				pid_t g = fork();
				if (g < 0) _exit(1);
				if (g > 0) _exit(0);
				setsid();
				closefrom(STDERR_FILENO + 1);
				execl("/bin/sh", "/bin/sh", "-c",
					argument, (char *)NULL);
				_exit(1);
			}
			waitpid(pid, NULL, 0);
		}
		break;
	}

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

static void
process_cursor_move(struct wm_server *server, uint32_t time)
{
	(void)time;
	if (!server->grabbed_view) {
		return;
	}

	double new_x = server->cursor->x - server->grab_x +
		server->grab_geobox.x;
	double new_y = server->cursor->y - server->grab_y +
		server->grab_geobox.y;
	struct wm_view *view = server->grabbed_view;
	int snap_x = (int)new_x;
	int snap_y = (int)new_y;
	wm_snap_edges(server, view, &snap_x, &snap_y);
	view->x = snap_x;
	view->y = snap_y;
	wlr_scene_node_set_position(&view->scene_tree->node,
		view->x, view->y);
}

static void
process_cursor_resize(struct wm_server *server, uint32_t time)
{
	(void)time;
	if (!server->grabbed_view) {
		return;
	}

	struct wm_view *view = server->grabbed_view;
	double dx = server->cursor->x - server->grab_x;
	double dy = server->cursor->y - server->grab_y;

	int new_x = server->grab_geobox.x;
	int new_y = server->grab_geobox.y;
	int new_w = server->grab_geobox.width;
	int new_h = server->grab_geobox.height;

	if (server->resize_edges & WLR_EDGE_RIGHT) {
		new_w = (int)(server->grab_geobox.width + dx);
	} else if (server->resize_edges & WLR_EDGE_LEFT) {
		new_x = (int)(server->grab_geobox.x + dx);
		new_w = (int)(server->grab_geobox.width - dx);
	}

	if (server->resize_edges & WLR_EDGE_BOTTOM) {
		new_h = (int)(server->grab_geobox.height + dy);
	} else if (server->resize_edges & WLR_EDGE_TOP) {
		new_y = (int)(server->grab_geobox.y + dy);
		new_h = (int)(server->grab_geobox.height - dy);
	}

	if (new_w < 1) new_w = 1;
	if (new_h < 1) new_h = 1;

	view->x = new_x;
	view->y = new_y;
	wlr_scene_node_set_position(&view->scene_tree->node, new_x, new_y);
	wlr_xdg_toplevel_set_size(view->xdg_toplevel, new_w, new_h);
}

/*
 * Cancel any pending auto-raise timer.
 */
static void
cancel_auto_raise(struct wm_server *server)
{
	if (server->auto_raise_timer) {
		wl_event_source_timer_update(server->auto_raise_timer, 0);
	}
	server->auto_raise_view = NULL;
}

/*
 * Timer callback: raise the view that was pending auto-raise,
 * but only if it's still the focused view.
 */
static int
auto_raise_timer_callback(void *data)
{
	struct wm_server *server = data;
	struct wm_view *view = server->auto_raise_view;
	server->auto_raise_view = NULL;

	if (view && view == server->focused_view) {
		wm_view_raise(view);
	}
	return 0;
}

/*
 * Schedule or perform an auto-raise for the given view.
 * Uses the configured delay; if 0, raises immediately.
 */
static void
schedule_auto_raise(struct wm_server *server, struct wm_view *view)
{
	if (!server->config || !server->config->raise_on_focus) {
		return;
	}

	int delay = server->config->auto_raise_delay_ms;

	if (delay <= 0) {
		wm_view_raise(view);
		return;
	}

	/* Create the timer lazily on first use */
	if (!server->auto_raise_timer) {
		server->auto_raise_timer = wl_event_loop_add_timer(
			server->wl_event_loop, auto_raise_timer_callback,
			server);
		if (!server->auto_raise_timer) {
			/* Fallback: raise immediately */
			wm_view_raise(view);
			return;
		}
	}

	server->auto_raise_view = view;
	wl_event_source_timer_update(server->auto_raise_timer, delay);
}

static void
process_cursor_motion(struct wm_server *server, uint32_t time)
{
	/* Update drag icon position if a drag is active */
	if (server->drag_icon_tree) {
		wlr_scene_node_set_position(
			&server->drag_icon_tree->node,
			(int)server->cursor->x,
			(int)server->cursor->y);
	}

	/*
	 * When locked, don't interact with normal views.
	 * Just clear pointer focus (lock surfaces use keyboard only).
	 */
	if (wm_session_is_locked(server)) {
		wlr_seat_pointer_clear_focus(server->seat);
		return;
	}

	if (server->cursor_mode == WM_CURSOR_MOVE) {
		process_cursor_move(server, time);
		return;
	}
	if (server->cursor_mode == WM_CURSOR_RESIZE) {
		process_cursor_resize(server, time);
		return;
	}

	/* Notify toolbar of pointer motion for auto-hide */
	wm_toolbar_notify_pointer_motion(server->toolbar,
		server->cursor->x, server->cursor->y);

	/* Dispatch motion to any open menus */
	if (wm_menu_handle_motion(server, server->cursor->x,
		server->cursor->y)) {
		return;
	}

	/* Check for drag (Move) mouse bindings */
	struct wm_mouse_state *ms = &server->mouse_state;
	if (ms->button_pressed) {
		double dx = server->cursor->x - ms->press_x;
		double dy = server->cursor->y - ms->press_y;
		double dist = sqrt(dx * dx + dy * dy);

		if (dist > WM_CLICK_MOVE_THRESHOLD) {
			/* This is a drag — check for Move binding */
			struct wm_view *view = NULL;
			enum wm_mouse_context ctx =
				get_cursor_context(server, &view);
			uint32_t mods = get_keyboard_modifiers(server);

			struct wm_mousebind *bind = mousebind_find(
				&server->mousebindings, ctx,
				WM_MOUSE_MOVE, ms->pressed_button, mods);
			if (bind) {
				execute_mousebind(server, bind, view);
				/* Clear pressed state to avoid re-triggering */
				ms->button_pressed = false;
				return;
			}
		}
	}

	/* Default: passthrough - find the surface under cursor */
	double sx, sy;
	struct wlr_surface *surface = NULL;
	struct wm_view *view = view_at(server,
		server->cursor->x, server->cursor->y, &surface, &sx, &sy);

	if (!view) {
		wlr_cursor_set_xcursor(server->cursor,
			server->cursor_mgr, "default");
		/* Pointer moved to empty space — cancel auto-raise */
		cancel_auto_raise(server);
	}

	/* Focus-follows-mouse: focus the view under the pointer */
	if (server->config &&
	    server->config->focus_policy != WM_FOCUS_CLICK &&
	    server->cursor_mode == WM_CURSOR_PASSTHROUGH) {
		if (view && view != server->focused_view) {
			wm_focus_update_for_cursor(server,
				server->cursor->x, server->cursor->y);
			schedule_auto_raise(server, view);
		} else if (!view && server->focused_view) {
			/*
			 * Strict mouse focus: unfocus when pointer moves
			 * to empty desktop area.
			 */
			if (server->config->focus_policy ==
			    WM_FOCUS_STRICT_MOUSE) {
				wm_unfocus_current(server);
			}
			cancel_auto_raise(server);
		}
	}

	if (surface) {
		wlr_seat_pointer_notify_enter(server->seat,
			surface, sx, sy);
		wlr_seat_pointer_notify_motion(server->seat,
			time, sx, sy);
	} else {
		wlr_seat_pointer_clear_focus(server->seat);
	}
}

static void
handle_cursor_motion(struct wl_listener *listener, void *data)
{
	struct wm_server *server =
		wl_container_of(listener, server, cursor_motion);
	struct wlr_pointer_motion_event *event = data;

	wm_idle_notify_activity(server);
	wlr_cursor_move(server->cursor, &event->pointer->base,
		event->delta_x, event->delta_y);

	/* Forward relative motion for games/3D apps */
	if (server->relative_pointer_mgr) {
		wlr_relative_pointer_manager_v1_send_relative_motion(
			server->relative_pointer_mgr, server->seat,
			(uint64_t)event->time_msec * 1000,
			event->delta_x, event->delta_y,
			event->unaccel_dx, event->unaccel_dy);
	}

	process_cursor_motion(server, event->time_msec);
}

static void
handle_cursor_motion_absolute(struct wl_listener *listener, void *data)
{
	struct wm_server *server =
		wl_container_of(listener, server, cursor_motion_absolute);
	struct wlr_pointer_motion_absolute_event *event = data;

	wm_idle_notify_activity(server);
	wlr_cursor_warp_absolute(server->cursor, &event->pointer->base,
		event->x, event->y);
	process_cursor_motion(server, event->time_msec);
}

static void
handle_cursor_button(struct wl_listener *listener, void *data)
{
	struct wm_server *server =
		wl_container_of(listener, server, cursor_button);
	struct wlr_pointer_button_event *event = data;
	struct wm_mouse_state *ms = &server->mouse_state;

	wm_idle_notify_activity(server);

	/* When locked, block all button interaction with normal views */
	if (wm_session_is_locked(server))
		return;

	if (event->state == WL_POINTER_BUTTON_STATE_RELEASED) {
		/* Dispatch button release to any open menus */
		if (wm_menu_handle_button(server, server->cursor->x,
			server->cursor->y, event->button, false)) {
			wlr_seat_pointer_notify_button(server->seat,
				event->time_msec, event->button,
				event->state);
			return;
		}

		/* Check for Click binding (press+release without significant move) */
		if (ms->button_pressed &&
		    ms->pressed_button == event->button) {
			double dx = server->cursor->x - ms->press_x;
			double dy = server->cursor->y - ms->press_y;
			double dist = sqrt(dx * dx + dy * dy);

			if (dist <= WM_CLICK_MOVE_THRESHOLD) {
				struct wm_view *view = NULL;
				enum wm_mouse_context ctx =
					get_cursor_context(server, &view);
				uint32_t mods =
					get_keyboard_modifiers(server);

				struct wm_mousebind *bind = mousebind_find(
					&server->mousebindings, ctx,
					WM_MOUSE_CLICK, event->button,
					mods);
				if (bind) {
					execute_mousebind(server, bind,
						view);
				}
			}
			ms->button_pressed = false;
		}

		/* End any interactive mode on button release */
		if (server->cursor_mode != WM_CURSOR_PASSTHROUGH) {
			server->cursor_mode = WM_CURSOR_PASSTHROUGH;
			server->grabbed_view = NULL;
		}

		/* Notify seat */
		wlr_seat_pointer_notify_button(server->seat,
			event->time_msec, event->button, event->state);
		return;
	}

	/* --- Button pressed --- */

	/* Dispatch button press to any open menus */
	if (wm_menu_handle_button(server, server->cursor->x,
		server->cursor->y, event->button, true)) {
		wlr_seat_pointer_notify_button(server->seat,
			event->time_msec, event->button, event->state);
		return;
	}

	/* Determine context */
	struct wm_view *view = NULL;
	enum wm_mouse_context ctx = get_cursor_context(server, &view);
	uint32_t mods = get_keyboard_modifiers(server);

	/* Track press state for click/move detection */
	ms->button_pressed = true;
	ms->pressed_button = event->button;
	ms->press_x = server->cursor->x;
	ms->press_y = server->cursor->y;

	/* Check for double click */
	bool is_double = false;
	if (event->button == ms->last_button &&
	    (event->time_msec - ms->last_time_msec) < WM_DOUBLE_CLICK_MS) {
		is_double = true;
	}
	ms->last_button = event->button;
	ms->last_time_msec = event->time_msec;

	/* Try double-click binding first */
	if (is_double) {
		struct wm_mousebind *bind = mousebind_find(
			&server->mousebindings, ctx,
			WM_MOUSE_DOUBLE, event->button, mods);
		if (bind) {
			execute_mousebind(server, bind, view);
			wlr_seat_pointer_notify_button(server->seat,
				event->time_msec, event->button,
				event->state);
			return;
		}
	}

	/* Try Mouse (press) binding */
	struct wm_mousebind *bind = mousebind_find(
		&server->mousebindings, ctx,
		WM_MOUSE_PRESS, event->button, mods);
	if (bind) {
		execute_mousebind(server, bind, view);
		wlr_seat_pointer_notify_button(server->seat,
			event->time_msec, event->button, event->state);
		return;
	}

	/* No binding matched — default behavior: focus + raise on click */
	if (view) {
		wm_focus_view(view, NULL);
		wm_view_raise(view);
	}

	wlr_seat_pointer_notify_button(server->seat,
		event->time_msec, event->button, event->state);
}

/*
 * Touch input handlers.
 * The cursor aggregates touch events from all touch devices and maps
 * coordinates via the output layout. We forward them to the seat.
 */
static void
handle_cursor_touch_down(struct wl_listener *listener, void *data)
{
	struct wm_server *server =
		wl_container_of(listener, server, cursor_touch_down);
	struct wlr_touch_down_event *event = data;

	wm_idle_notify_activity(server);

	double lx, ly;
	wlr_cursor_absolute_to_layout_coords(server->cursor,
		&event->touch->base, event->x, event->y, &lx, &ly);

	double sx, sy;
	struct wlr_surface *surface = NULL;
	struct wm_view *view = view_at(server, lx, ly, &surface, &sx, &sy);

	if (view) {
		wm_focus_view(view, surface);
	}

	if (surface) {
		wlr_seat_touch_notify_down(server->seat, surface,
			event->time_msec, event->touch_id, sx, sy);
	}
}

static void
handle_cursor_touch_up(struct wl_listener *listener, void *data)
{
	struct wm_server *server =
		wl_container_of(listener, server, cursor_touch_up);
	struct wlr_touch_up_event *event = data;

	wm_idle_notify_activity(server);
	wlr_seat_touch_notify_up(server->seat, event->time_msec,
		event->touch_id);
}

static void
handle_cursor_touch_motion(struct wl_listener *listener, void *data)
{
	struct wm_server *server =
		wl_container_of(listener, server, cursor_touch_motion);
	struct wlr_touch_motion_event *event = data;

	wm_idle_notify_activity(server);

	double lx, ly;
	wlr_cursor_absolute_to_layout_coords(server->cursor,
		&event->touch->base, event->x, event->y, &lx, &ly);

	double sx, sy;
	struct wlr_surface *surface = NULL;
	view_at(server, lx, ly, &surface, &sx, &sy);

	if (surface) {
		wlr_seat_touch_notify_motion(server->seat,
			event->time_msec, event->touch_id, sx, sy);
	}
}

static void
handle_cursor_touch_cancel(struct wl_listener *listener, void *data)
{
	struct wm_server *server =
		wl_container_of(listener, server, cursor_touch_cancel);
	(void)data;

	wm_idle_notify_activity(server);

	/*
	 * Cancel all active touch points. Walk the touch point list
	 * and cancel for each client that has active points.
	 */
	struct wlr_touch_point *point;
	wl_list_for_each(point, &server->seat->touch_state.touch_points,
			link) {
		if (point->client) {
			wlr_seat_touch_notify_cancel(server->seat,
				point->client);
			break;
		}
	}
}

static void
handle_cursor_touch_frame(struct wl_listener *listener, void *data)
{
	struct wm_server *server =
		wl_container_of(listener, server, cursor_touch_frame);
	(void)data;

	wlr_seat_touch_notify_frame(server->seat);
}

/*
 * Pointer gesture handlers.
 * Forward touchpad gestures (swipe, pinch, hold) from the cursor
 * to clients via the pointer-gestures-unstable-v1 protocol.
 */
static void
handle_cursor_swipe_begin(struct wl_listener *listener, void *data)
{
	struct wm_server *server =
		wl_container_of(listener, server, cursor_swipe_begin);
	struct wlr_pointer_swipe_begin_event *event = data;

	wlr_pointer_gestures_v1_send_swipe_begin(
		server->pointer_gestures, server->seat,
		event->time_msec, event->fingers);
}

static void
handle_cursor_swipe_update(struct wl_listener *listener, void *data)
{
	struct wm_server *server =
		wl_container_of(listener, server, cursor_swipe_update);
	struct wlr_pointer_swipe_update_event *event = data;

	wlr_pointer_gestures_v1_send_swipe_update(
		server->pointer_gestures, server->seat,
		event->time_msec, event->dx, event->dy);
}

static void
handle_cursor_swipe_end(struct wl_listener *listener, void *data)
{
	struct wm_server *server =
		wl_container_of(listener, server, cursor_swipe_end);
	struct wlr_pointer_swipe_end_event *event = data;

	wlr_pointer_gestures_v1_send_swipe_end(
		server->pointer_gestures, server->seat,
		event->time_msec, event->cancelled);
}

static void
handle_cursor_pinch_begin(struct wl_listener *listener, void *data)
{
	struct wm_server *server =
		wl_container_of(listener, server, cursor_pinch_begin);
	struct wlr_pointer_pinch_begin_event *event = data;

	wlr_pointer_gestures_v1_send_pinch_begin(
		server->pointer_gestures, server->seat,
		event->time_msec, event->fingers);
}

static void
handle_cursor_pinch_update(struct wl_listener *listener, void *data)
{
	struct wm_server *server =
		wl_container_of(listener, server, cursor_pinch_update);
	struct wlr_pointer_pinch_update_event *event = data;

	wlr_pointer_gestures_v1_send_pinch_update(
		server->pointer_gestures, server->seat,
		event->time_msec, event->dx, event->dy,
		event->scale, event->rotation);
}

static void
handle_cursor_pinch_end(struct wl_listener *listener, void *data)
{
	struct wm_server *server =
		wl_container_of(listener, server, cursor_pinch_end);
	struct wlr_pointer_pinch_end_event *event = data;

	wlr_pointer_gestures_v1_send_pinch_end(
		server->pointer_gestures, server->seat,
		event->time_msec, event->cancelled);
}

static void
handle_cursor_hold_begin(struct wl_listener *listener, void *data)
{
	struct wm_server *server =
		wl_container_of(listener, server, cursor_hold_begin);
	struct wlr_pointer_hold_begin_event *event = data;

	wlr_pointer_gestures_v1_send_hold_begin(
		server->pointer_gestures, server->seat,
		event->time_msec, event->fingers);
}

static void
handle_cursor_hold_end(struct wl_listener *listener, void *data)
{
	struct wm_server *server =
		wl_container_of(listener, server, cursor_hold_end);
	struct wlr_pointer_hold_end_event *event = data;

	wlr_pointer_gestures_v1_send_hold_end(
		server->pointer_gestures, server->seat,
		event->time_msec, event->cancelled);
}

static void
handle_cursor_axis(struct wl_listener *listener, void *data)
{
	struct wm_server *server =
		wl_container_of(listener, server, cursor_axis);
	struct wlr_pointer_axis_event *event = data;

	/* Forward scroll events to the focused client */
	wlr_seat_pointer_notify_axis(server->seat,
		event->time_msec, event->orientation, event->delta,
		event->delta_discrete, event->source,
		event->relative_direction);
}

static void
handle_cursor_frame(struct wl_listener *listener, void *data)
{
	struct wm_server *server =
		wl_container_of(listener, server, cursor_frame);

	wlr_seat_pointer_notify_frame(server->seat);
}

void
wm_cursor_init(struct wm_server *server)
{
	server->cursor_motion.notify = handle_cursor_motion;
	wl_signal_add(&server->cursor->events.motion,
		&server->cursor_motion);

	server->cursor_motion_absolute.notify = handle_cursor_motion_absolute;
	wl_signal_add(&server->cursor->events.motion_absolute,
		&server->cursor_motion_absolute);

	server->cursor_button.notify = handle_cursor_button;
	wl_signal_add(&server->cursor->events.button,
		&server->cursor_button);

	server->cursor_axis.notify = handle_cursor_axis;
	wl_signal_add(&server->cursor->events.axis,
		&server->cursor_axis);

	server->cursor_frame.notify = handle_cursor_frame;
	wl_signal_add(&server->cursor->events.frame,
		&server->cursor_frame);

	/* Touch events */
	server->cursor_touch_down.notify = handle_cursor_touch_down;
	wl_signal_add(&server->cursor->events.touch_down,
		&server->cursor_touch_down);

	server->cursor_touch_up.notify = handle_cursor_touch_up;
	wl_signal_add(&server->cursor->events.touch_up,
		&server->cursor_touch_up);

	server->cursor_touch_motion.notify = handle_cursor_touch_motion;
	wl_signal_add(&server->cursor->events.touch_motion,
		&server->cursor_touch_motion);

	server->cursor_touch_cancel.notify = handle_cursor_touch_cancel;
	wl_signal_add(&server->cursor->events.touch_cancel,
		&server->cursor_touch_cancel);

	server->cursor_touch_frame.notify = handle_cursor_touch_frame;
	wl_signal_add(&server->cursor->events.touch_frame,
		&server->cursor_touch_frame);

	/* Pointer gesture events (touchpad swipe/pinch/hold) */
	server->cursor_swipe_begin.notify = handle_cursor_swipe_begin;
	wl_signal_add(&server->cursor->events.swipe_begin,
		&server->cursor_swipe_begin);

	server->cursor_swipe_update.notify = handle_cursor_swipe_update;
	wl_signal_add(&server->cursor->events.swipe_update,
		&server->cursor_swipe_update);

	server->cursor_swipe_end.notify = handle_cursor_swipe_end;
	wl_signal_add(&server->cursor->events.swipe_end,
		&server->cursor_swipe_end);

	server->cursor_pinch_begin.notify = handle_cursor_pinch_begin;
	wl_signal_add(&server->cursor->events.pinch_begin,
		&server->cursor_pinch_begin);

	server->cursor_pinch_update.notify = handle_cursor_pinch_update;
	wl_signal_add(&server->cursor->events.pinch_update,
		&server->cursor_pinch_update);

	server->cursor_pinch_end.notify = handle_cursor_pinch_end;
	wl_signal_add(&server->cursor->events.pinch_end,
		&server->cursor_pinch_end);

	server->cursor_hold_begin.notify = handle_cursor_hold_begin;
	wl_signal_add(&server->cursor->events.hold_begin,
		&server->cursor_hold_begin);

	server->cursor_hold_end.notify = handle_cursor_hold_end;
	wl_signal_add(&server->cursor->events.hold_end,
		&server->cursor_hold_end);
}

void
wm_cursor_finish(struct wm_server *server)
{
	wl_list_remove(&server->cursor_motion.link);
	wl_list_remove(&server->cursor_motion_absolute.link);
	wl_list_remove(&server->cursor_button.link);
	wl_list_remove(&server->cursor_axis.link);
	wl_list_remove(&server->cursor_frame.link);
	wl_list_remove(&server->cursor_touch_down.link);
	wl_list_remove(&server->cursor_touch_up.link);
	wl_list_remove(&server->cursor_touch_motion.link);
	wl_list_remove(&server->cursor_touch_cancel.link);
	wl_list_remove(&server->cursor_touch_frame.link);
	wl_list_remove(&server->cursor_swipe_begin.link);
	wl_list_remove(&server->cursor_swipe_update.link);
	wl_list_remove(&server->cursor_swipe_end.link);
	wl_list_remove(&server->cursor_pinch_begin.link);
	wl_list_remove(&server->cursor_pinch_update.link);
	wl_list_remove(&server->cursor_pinch_end.link);
	wl_list_remove(&server->cursor_hold_begin.link);
	wl_list_remove(&server->cursor_hold_end.link);
}
