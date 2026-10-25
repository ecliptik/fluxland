/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * cursor.c - Pointer/cursor input handling
 *
 * Handles mouse events with context-sensitive binding dispatch.
 * Determines click context from decoration hit-testing, looks up
 * mouse bindings, and dispatches actions.
 */

#define _GNU_SOURCE
#include <math.h>
#include <stdlib.h>
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
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/util/edges.h>
#include <wlr/util/log.h>
#include <cairo.h>
#include <drm_fourcc.h>

#include "config.h"
#include "cursor.h"
#include "command.h"
#include "cursor_actions.h"
#include "cursor_snap.h"
#include "server.h"
#include "decoration.h"
#include "render.h"
#include "idle.h"
#include "menu.h"
#include "mousebind.h"
#include "output.h"
#include "placement.h"
#include "session_lock.h"
#include "slit.h"
#ifdef WM_HAS_SYSTRAY
#include "systray.h"
#endif
#include "tabgroup.h"
#include "toolbar.h"
#include "util.h"
#include "view.h"
#include "view_geometry.h"
#include "workspace.h"

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

	/*
	 * Workspace edge warping: when dragging a window and the
	 * cursor hits the left or right edge of the output, switch
	 * to the prev/next workspace and teleport the cursor to
	 * the opposite edge.
	 */
	if (server->config && server->config->workspace_warping) {
		struct wlr_output *output =
			wlr_output_layout_output_at(server->output_layout,
				server->cursor->x, server->cursor->y);
		if (output) {
			struct wlr_box obox;
			wlr_output_layout_get_box(server->output_layout,
				output, &obox);
			int cx = (int)server->cursor->x;
			int edge_margin = 1;

			if (cx <= obox.x + edge_margin) {
				/* Left edge: switch to previous workspace */
				wm_workspace_switch_prev(server);
				/* Warp cursor to right side */
				double warp_x = obox.x + obox.width - 2;
				wlr_cursor_warp(server->cursor, NULL,
					warp_x, server->cursor->y);
				/* Adjust grab so the view position stays
				 * relative to the cursor */
				server->grab_x = server->cursor->x -
					new_x + server->grab_geobox.x;
				return;
			}
			if (cx >= obox.x + obox.width - edge_margin - 1) {
				/* Right edge: switch to next workspace */
				wm_workspace_switch_next(server);
				/* Warp cursor to left side */
				double warp_x = obox.x + 2;
				wlr_cursor_warp(server->cursor, NULL,
					warp_x, server->cursor->y);
				server->grab_x = server->cursor->x -
					new_x + server->grab_geobox.x;
				return;
			}
		}
	}

	int snap_x = (int)new_x;
	int snap_y = (int)new_y;
	wm_snap_edges(server, view, &snap_x, &snap_y);

	if (server->config && !server->config->opaque_move) {
		/* Wireframe mode: show outline instead of moving view */
		struct wlr_box geo;
		wm_view_get_geometry(view, &geo);
		wireframe_show(server, snap_x, snap_y,
			geo.width, geo.height);
	} else {
		/* Opaque mode: move the actual view */
		view->x = snap_x;
		view->y = snap_y;
		wlr_scene_node_set_position(&view->scene_tree->node,
			view->x, view->y);
	}

	/* Update position overlay */
	if (server->show_position) {
		char buf[64];
		int pos_x = server->wireframe_active ?
			server->wireframe_x : view->x;
		int pos_y = server->wireframe_active ?
			server->wireframe_y : view->y;
		snprintf(buf, sizeof(buf), "%d, %d", pos_x, pos_y);
		position_overlay_update(server, buf);
	}

	/* Check for window snap zones */
	snap_zone_check(server);
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

	if (server->config && !server->config->opaque_resize) {
		/* Wireframe mode: show outline instead of resizing view */
		wireframe_show(server, new_x, new_y, new_w, new_h);
	} else {
		/* Opaque mode: resize the actual view */
		view->x = new_x;
		view->y = new_y;
		wlr_scene_node_set_position(&view->scene_tree->node,
			new_x, new_y);
		wm_view_set_size(view, new_w, new_h);
	}

	if (server->show_position) {
		char buf[64];
		int disp_w = server->wireframe_active ?
			server->wireframe_w : new_w;
		int disp_h = server->wireframe_active ?
			server->wireframe_h : new_h;
		snprintf(buf, sizeof(buf), "%d x %d", disp_w, disp_h);
		position_overlay_update(server, buf);
	}
}

static void
process_cursor_tabbing(struct wm_server *server, uint32_t time)
{
	(void)time;
	if (!server->grabbed_view) {
		return;
	}

	struct wm_view *view = server->grabbed_view;

	/* Move the dragged window along with cursor */
	double new_x = server->cursor->x - server->grab_x;
	double new_y = server->cursor->y - server->grab_y;

	view->x = (int)new_x;
	view->y = (int)new_y;
	wlr_scene_node_set_position(&view->scene_tree->node,
		view->x, view->y);

	/* Find if cursor is over another view's titlebar */
	double sx, sy;
	struct wlr_surface *surface = NULL;
	struct wm_view *target = view_at(server,
		server->cursor->x, server->cursor->y, &surface, &sx, &sy);

	if (target && target != view && target->decoration) {
		double dx = server->cursor->x - target->x;
		double dy = server->cursor->y - target->y;
		enum wm_decor_region region =
			wm_decoration_region_at(target->decoration, dx, dy);
		if (region == WM_DECOR_REGION_TITLEBAR ||
		    region == WM_DECOR_REGION_LABEL) {
			wlr_cursor_set_xcursor(server->cursor,
				server->cursor_mgr, "dnd-link");
			return;
		}
	}

	wlr_cursor_set_xcursor(server->cursor,
		server->cursor_mgr, "dnd-none");
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
	 * When locked, don't interact with normal views. Lock surfaces
	 * (e.g. gtklock's buttons) still get pointer input.
	 */
	if (wm_session_is_locked(server)) {
		double sx, sy;
		struct wlr_surface *ls = wm_session_lock_surface_at(server,
			server->cursor->x, server->cursor->y, &sx, &sy);
		if (ls) {
			wlr_seat_pointer_notify_enter(server->seat, ls, sx, sy);
			wlr_seat_pointer_notify_motion(server->seat, time,
				sx, sy);
		} else {
			wlr_seat_pointer_clear_focus(server->seat);
		}
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
	if (server->cursor_mode == WM_CURSOR_TABBING) {
		process_cursor_tabbing(server, time);
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
			/* A drag: Move bindings of where it started */
			struct wm_view *view = ms->pressed_view;
			uint32_t mods = get_keyboard_modifiers(server);

			struct wm_mousebind *bind = mousebind_find(
				&server->mousebindings, ms->press_context,
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

	/* MouseTabFocus: hover over a tab to activate it */
	if (view && view->decoration && view->tab_group &&
	    view->tab_group->count > 1 &&
	    server->config && server->config->tab_focus_model == 1) {
		double dx = server->cursor->x - view->x;
		double dy = server->cursor->y - view->y;
		int tab_idx = wm_decoration_tab_at(view->decoration, dx, dy);
		if (tab_idx >= 0) {
			int i = 0;
			struct wm_view *tab_view;
			wl_list_for_each(tab_view, &view->tab_group->views,
					tab_link) {
				if (i == tab_idx) {
					if (tab_view !=
					    view->tab_group->active_view) {
						wm_tab_group_activate(
							view->tab_group,
							tab_view);
					}
					break;
				}
				i++;
			}
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

/* The mouse contexts at the cursor: the toolbar's, or a window's... */
static uint32_t
pointer_context(struct wm_server *server, struct wm_view **view)
{
	if (wm_toolbar_contains(server->toolbar, server->cursor->x,
	    server->cursor->y)) {
		*view = NULL;
		return WM_MOUSE_CTX_TOOLBAR;
	}
	return get_cursor_context(server, view);
}

/*
 * A titlebar button's own action (WinButton): close, iconify, shade and
 * stick on the left button; maximize fully, vertically or horizontally
 * on the left, middle or right one.
 */
static void
decor_button_action(struct wm_server *server, struct wm_view *view,
	enum wm_button_type type, uint32_t button)
{
	enum wm_action action = WM_ACTION_NOP;
	switch (type) {
	case WM_BUTTON_CLOSE:
		action = button == BTN_LEFT ? WM_ACTION_CLOSE : WM_ACTION_NOP;
		break;
	case WM_BUTTON_MAXIMIZE:
		action = button == BTN_LEFT ? WM_ACTION_MAXIMIZE :
			button == BTN_MIDDLE ? WM_ACTION_MAXIMIZE_VERT :
			button == BTN_RIGHT ? WM_ACTION_MAXIMIZE_HORIZ :
			WM_ACTION_NOP;
		break;
	case WM_BUTTON_ICONIFY:
		action = button == BTN_LEFT ? WM_ACTION_MINIMIZE : WM_ACTION_NOP;
		break;
	case WM_BUTTON_SHADE:
		action = button == BTN_LEFT ? WM_ACTION_SHADE : WM_ACTION_NOP;
		break;
	case WM_BUTTON_STICK:
		action = button == BTN_LEFT ? WM_ACTION_STICK : WM_ACTION_NOP;
		break;
	case WM_BUTTON_MENUICON:
		action = button == BTN_LEFT ? WM_ACTION_WINDOW_MENU :
			WM_ACTION_NOP;
		break;
	case WM_BUTTON_LHALF:
	case WM_BUTTON_RHALF:
		/* Fluxbox's own commands for them (Window.cc) */
		if (button == BTN_LEFT) {
			struct wm_command *cmd = wm_command_parse(
				type == WM_BUTTON_LHALF ?
				"MacroCmd {MoveTo 0 0} {ResizeTo 50% 100%}" :
				"MacroCmd {MoveTo 50% 0} {ResizeTo 50% 100%}");
			if (cmd) {
				wm_command_execute(server, cmd, view);
				wm_command_destroy(cmd);
			}
		}
		break;
	}
	if (action != WM_ACTION_NOP)
		execute_mouse_action(server, action, NULL, view);
}

/*
 * A press and release in one place: Click bindings, or on a titlebar
 * button pressed and released over, OnWinButton/OnMinButton/OnMaxButton
 * Click bindings and otherwise the button's own action.
 */
static void
handle_click(struct wm_server *server, uint32_t button)
{
	struct wm_mouse_state *ms = &server->mouse_state;
	struct wm_view *view = NULL;
	uint32_t ctx = pointer_context(server, &view);
	uint32_t mods = get_keyboard_modifiers(server);

	if (ms->pressed_decor_button >= 0 && view && view->decoration &&
	    view == ms->pressed_view) {
		struct wm_decor_button *btn = wm_decoration_button_at(
			view->decoration, server->cursor->x - view->x,
			server->cursor->y - view->y);
		int type = ms->pressed_decor_button;
		ms->pressed_decor_button = -1;
		if (btn && (int)btn->type == type) {
			ctx = type == WM_BUTTON_ICONIFY ? WM_MOUSE_CTX_MINBUTTON :
				type == WM_BUTTON_MAXIMIZE ?
				WM_MOUSE_CTX_MAXBUTTON : WM_MOUSE_CTX_WINBUTTON;
			struct wm_mousebind *bind = mousebind_find(
				&server->mousebindings, ctx, WM_MOUSE_CLICK,
				button, mods);
			if (bind)
				execute_mousebind(server, bind, view);
			else
				decor_button_action(server, view, btn->type,
					button);
			return;
		}
	}

	struct wm_mousebind *bind = mousebind_find(&server->mousebindings,
		ctx, WM_MOUSE_CLICK, button, mods);
	if (bind)
		execute_mousebind(server, bind, view);
}

static void
handle_cursor_button(struct wl_listener *listener, void *data)
{
	struct wm_server *server =
		wl_container_of(listener, server, cursor_button);
	struct wlr_pointer_button_event *event = data;
	struct wm_mouse_state *ms = &server->mouse_state;

	wm_idle_notify_activity(server);

	/* When locked, buttons only reach the lock surface (if focused) */
	if (wm_session_is_locked(server)) {
		wlr_seat_pointer_notify_button(server->seat,
			event->time_msec, event->button, event->state);
		return;
	}

	if (event->state == WL_POINTER_BUTTON_STATE_RELEASED) {
		/* A titlebar button shows window.button.pressed while held */
		if (ms->pressed_decor_button >= 0 && ms->pressed_view &&
		    ms->pressed_view->decoration)
			wm_decoration_set_pressed(ms->pressed_view->decoration,
				NULL, server->style);

		/* An arrow button of the toolbar acts on release */
		if (wm_toolbar_handle_release(server->toolbar,
				server->cursor->x, server->cursor->y,
				event->button)) {
			ms->button_pressed = false;
			wlr_seat_pointer_notify_button(server->seat,
				event->time_msec, event->button,
				event->state);
			return;
		}

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

			if (dist <= WM_CLICK_MOVE_THRESHOLD)
				handle_click(server, event->button);
			ms->button_pressed = false;
			ms->pressed_view = NULL;
		}

		/* End any interactive mode on button release */
		if (server->cursor_mode != WM_CURSOR_PASSTHROUGH) {
			/* Handle tab drop for tabbing mode */
			if (server->cursor_mode == WM_CURSOR_TABBING &&
			    server->grabbed_view) {
				struct wm_view *dragged =
					server->grabbed_view;
				double sx2, sy2;
				struct wlr_surface *surf2 = NULL;
				struct wm_view *target = view_at(server,
					server->cursor->x,
					server->cursor->y,
					&surf2, &sx2, &sy2);

				if (target && target != dragged &&
				    target->decoration) {
					double tdx = server->cursor->x -
						target->x;
					double tdy = server->cursor->y -
						target->y;
					enum wm_decor_region region =
						wm_decoration_region_at(
							target->decoration,
							tdx, tdy);
					if (region ==
					    WM_DECOR_REGION_TITLEBAR ||
					    region ==
					    WM_DECOR_REGION_LABEL) {
						/* Drop on titlebar:
						 * create/join tab group */
						if (!target->tab_group) {
							wm_tab_group_create(
								target);
						}
						if (target->tab_group) {
							wm_tab_group_add(
								target->
								tab_group,
								dragged);
						}
						server->cursor_mode =
							WM_CURSOR_PASSTHROUGH;
						server->grabbed_view = NULL;
						wlr_cursor_set_xcursor(
							server->cursor,
							server->cursor_mgr,
							"default");
						wlr_seat_pointer_notify_button(
							server->seat,
							event->time_msec,
							event->button,
							event->state);
						return;
					}
				}

				/* No valid drop target: restore position */
				dragged->x =
					server->grab_geobox.x;
				dragged->y =
					server->grab_geobox.y;
				wlr_scene_node_set_position(
					&dragged->scene_tree->node,
					dragged->x, dragged->y);
				server->cursor_mode =
					WM_CURSOR_PASSTHROUGH;
				server->grabbed_view = NULL;
				wlr_cursor_set_xcursor(server->cursor,
					server->cursor_mgr, "default");
				wlr_seat_pointer_notify_button(
					server->seat,
					event->time_msec,
					event->button,
					event->state);
				return;
			}

			/* Apply snap zone geometry if snapping is active */
			if (server->cursor_mode == WM_CURSOR_MOVE &&
			    server->snap_zone != WM_SNAP_ZONE_NONE &&
			    server->grabbed_view) {
				struct wm_view *sv =
					server->grabbed_view;
				struct wlr_box sb =
					server->snap_preview_box;

				/* Save original geometry for restore */
				if (!sv->maximized && !sv->fullscreen &&
				    !sv->lhalf && !sv->rhalf) {
					struct wlr_box geo;
					wm_view_get_geometry(sv, &geo);
					sv->saved_geometry.x = sv->x;
					sv->saved_geometry.y = sv->y;
					sv->saved_geometry.width =
						geo.width;
					sv->saved_geometry.height =
						geo.height;
				}

				/* The whole frame fills the zone */
				wm_view_set_frame_box(sv, &sb);

				snap_preview_destroy(server);
				if (server->wireframe_active)
					wireframe_hide(server);
				server->cursor_mode =
					WM_CURSOR_PASSTHROUGH;
				server->grabbed_view = NULL;
				position_overlay_destroy(server);

				wlr_seat_pointer_notify_button(
					server->seat,
					event->time_msec,
					event->button,
					event->state);
				return;
			}

			/* Clean up snap preview if no zone matched */
			snap_preview_destroy(server);

			/* Apply wireframe geometry if active */
			if (server->wireframe_active &&
			    server->grabbed_view) {
				struct wm_view *wf_view =
					server->grabbed_view;
				if (server->cursor_mode == WM_CURSOR_MOVE) {
					wf_view->x = server->wireframe_x;
					wf_view->y = server->wireframe_y;
					wlr_scene_node_set_position(
						&wf_view->scene_tree->node,
						wf_view->x, wf_view->y);
				} else if (server->cursor_mode ==
					   WM_CURSOR_RESIZE) {
					wf_view->x = server->wireframe_x;
					wf_view->y = server->wireframe_y;
					wlr_scene_node_set_position(
						&wf_view->scene_tree->node,
						wf_view->x, wf_view->y);
					wm_view_set_size(wf_view, server->wireframe_w,
						server->wireframe_h);
				}
				wireframe_hide(server);
			}
			server->cursor_mode = WM_CURSOR_PASSTHROUGH;
			server->grabbed_view = NULL;
			position_overlay_destroy(server);
		}

		/* A press a binding took doesn't reach the client: nor its release */
		if (ms->press_consumed) {
			ms->press_consumed = false;
			return;
		}
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

#ifdef WM_HAS_SYSTRAY
	/* Check if click is in the systray area */
	if (server->systray && server->toolbar &&
	    server->toolbar->visible) {
		struct wm_toolbar *tb = server->toolbar;
		double cx = server->cursor->x;
		double cy = server->cursor->y;
		double tb_local_x = cx - tb->x;
		double tb_local_y = cy - tb->y;
		if (tb_local_x >= 0 && tb_local_x < tb->width &&
		    tb_local_y >= 0 && tb_local_y < tb->height) {
			/* Compute systray x offset within toolbar */
			int systray_x = 0;
			if (tb->tool_count > 0) {
				struct wm_toolbar_tool *last =
					&tb->tools[tb->tool_count - 1];
				systray_x = last->x + last->width;
			}
			double st_local_x = tb_local_x - systray_x;
			int st_width = wm_systray_get_width(
				server->systray);
			if (st_local_x >= 0 && st_local_x < st_width) {
				if (wm_systray_handle_click(
					server->systray,
					st_local_x, tb_local_y,
					event->button)) {
					wlr_seat_pointer_notify_button(
						server->seat,
						event->time_msec,
						event->button,
						event->state);
					return;
				}
			}
		}
	}
#endif

	/* Where it happened, for Fluxbox's contexts */
	struct wm_view *view = NULL;
	uint32_t ctx = pointer_context(server, &view);
	uint32_t mods = get_keyboard_modifiers(server);

	/* Track press state for click/move detection */
	ms->button_pressed = true;
	ms->pressed_button = event->button;
	ms->press_x = server->cursor->x;
	ms->press_y = server->cursor->y;
	ms->press_context = ctx;
	ms->pressed_view = view;
	ms->press_consumed = false;
	ms->pressed_decor_button = -1;

	/* Check for double click */
	bool is_double = false;
	uint32_t dclick_ms = WM_DOUBLE_CLICK_MS;
	if (server->config)
		dclick_ms = (uint32_t)server->config->double_click_interval;
	if (event->button == ms->last_button &&
	    (event->time_msec - ms->last_time_msec) < dclick_ms) {
		is_double = true;
	}
	ms->last_button = event->button;
	ms->last_time_msec = event->time_msec;

	/*
	 * Bindings first (Keys::doAction): a double click tries Double
	 * bindings, then the single ones. A press a binding takes, or that
	 * starts a Move binding, isn't passed to the client.
	 */
	struct wm_mousebind *bind = NULL;
	if (is_double)
		bind = mousebind_find(&server->mousebindings, ctx,
			WM_MOUSE_DOUBLE, event->button, mods);
	if (!bind)
		bind = mousebind_find(&server->mousebindings, ctx,
			WM_MOUSE_PRESS, event->button, mods);
	if (bind) {
		execute_mousebind(server, bind, view);
		ms->press_consumed = true;
		return;
	}
	if (mousebind_has_move(&server->mousebindings, ctx, event->button,
	    mods)) {
		ms->press_consumed = true;
		return;
	}

	/* The toolbar's own tools */
	if (ctx & WM_MOUSE_CTX_TOOLBAR) {
		wm_toolbar_handle_button(server->toolbar, server->cursor->x,
			server->cursor->y, event->button);
		return;
	}

	/* A titlebar button acts when it is released over (handle_click) */
	if (view && view->decoration) {
		struct wm_decor_button *btn = wm_decoration_button_at(
			view->decoration, server->cursor->x - view->x,
			server->cursor->y - view->y);
		if (btn) {
			ms->pressed_decor_button = (int)btn->type;
			wm_decoration_set_pressed(view->decoration, btn,
				server->style);
		}
	}

	/* No binding: focus, and raise unless clickRaises is off */
	if (view) {
		server->focus_user_initiated = true;
		wm_focus_view(view, NULL);
		bool on_client = ctx == WM_MOUSE_CTX_WINDOW;
		if (!on_client || !server->config ||
		    server->config->click_raises)
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
		server->focus_user_initiated = true;
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

	/* Check if this gesture should be intercepted by the compositor */
	if (server->config &&
	    server->config->gesture_workspace_switch &&
	    (int)event->fingers == server->config->gesture_workspace_fingers) {
		server->gesture_state.active = true;
		server->gesture_state.fingers = event->fingers;
		server->gesture_state.dx_accum = 0;
		server->gesture_state.dy_accum = 0;
		server->gesture_state.consumed = false;
		return; /* don't forward to clients */
	}

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

	if (server->gesture_state.active) {
		if (server->gesture_state.consumed)
			return; /* already triggered action */

		server->gesture_state.dx_accum += event->dx;
		server->gesture_state.dy_accum += event->dy;

		double threshold = server->config
			? server->config->gesture_swipe_threshold : 100.0;
		double dx = server->gesture_state.dx_accum;
		double dy = server->gesture_state.dy_accum;

		/* Check if threshold crossed */
		if (dx > threshold) {
			/* Swipe right -> next workspace */
			wm_workspace_switch_next(server);
			server->gesture_state.consumed = true;
		} else if (dx < -threshold) {
			/* Swipe left -> previous workspace */
			wm_workspace_switch_prev(server);
			server->gesture_state.consumed = true;
		} else if (dy < -threshold && server->config &&
			   server->config->gesture_overview) {
			/* Swipe up -> show window list */
			wm_menu_show_window_list(server,
				(int)server->cursor->x,
				(int)server->cursor->y);
			server->gesture_state.consumed = true;
		}
		return;
	}

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

	if (server->gesture_state.active) {
		server->gesture_state.active = false;
		server->gesture_state.consumed = false;
		return; /* don't forward to clients */
	}

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

/*
 * A mouse wheel step as Fluxbox sees it: a press of button 4 (up), 5
 * (down), 6 (left) or 7 (right). If a Mouse or Click binding takes it,
 * the scroll goes no further. High-resolution wheels report fractions
 * of a step (120 a step); touchpads and other scrolling without steps
 * stay scrolling.
 */
static bool
handle_wheel_binding(struct wm_server *server,
	struct wlr_pointer_axis_event *event)
{
	if (event->delta_discrete == 0 ||
	    event->source != WL_POINTER_AXIS_SOURCE_WHEEL)
		return false;
	bool vertical = event->orientation == WL_POINTER_AXIS_VERTICAL_SCROLL;
	uint32_t button = vertical ?
		(event->delta_discrete < 0 ? WM_BUTTON_WHEEL_UP :
			WM_BUTTON_WHEEL_DOWN) :
		(event->delta_discrete < 0 ? WM_BUTTON_WHEEL_LEFT :
			WM_BUTTON_WHEEL_RIGHT);

	struct wm_view *view = NULL;
	uint32_t ctx = pointer_context(server, &view);
	uint32_t mods = get_keyboard_modifiers(server);
	struct wm_mousebind *bind = mousebind_find(&server->mousebindings,
		ctx, WM_MOUSE_PRESS, button, mods);
	if (!bind)
		bind = mousebind_find(&server->mousebindings, ctx,
			WM_MOUSE_CLICK, button, mods);
	struct wm_mouse_state *ms = &server->mouse_state;
	int axis = vertical ? 0 : 1;
	if (!bind) {
		ms->wheel_steps[axis] = 0;
		return false;
	}

	/* A whole step in this direction? */
	int acc = ms->wheel_steps[axis];
	if ((acc < 0) != (event->delta_discrete < 0))
		acc = 0;
	acc += event->delta_discrete;
	while (acc >= 120 || acc <= -120) {
		execute_mousebind(server, bind, view);
		acc += acc > 0 ? -120 : 120;
	}
	ms->wheel_steps[axis] = acc;
	return true;
}

static void
handle_cursor_axis(struct wl_listener *listener, void *data)
{
	struct wm_server *server =
		wl_container_of(listener, server, cursor_axis);
	struct wlr_pointer_axis_event *event = data;

	/* The wheel is buttons 4-7 to mouse bindings */
	if (handle_wheel_binding(server, event))
		return;

	/* Check if toolbar consumes this scroll event */
	if (event->orientation == WL_POINTER_AXIS_VERTICAL_SCROLL &&
	    wm_toolbar_handle_scroll(server->toolbar,
		    server->cursor->x, server->cursor->y, event->delta)) {
		return;
	}

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
