/*
 * wm-wayland - A Fluxbox-inspired Wayland compositor
 * cursor.c - Pointer/cursor input handling
 */

#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/edges.h>
#include <wlr/util/log.h>

#include "cursor.h"
#include "server.h"

/*
 * Find the view under the cursor, returning the surface and
 * surface-local coordinates.  Returns NULL if no view is found.
 *
 * This uses the scene graph to do hit-testing.
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
		return NULL;
	}

	*surface = scene_surface->surface;

	/*
	 * Walk up the scene tree to find the node that has a view
	 * attached as data.  This is set by the view/xdg module when
	 * creating scene tree nodes for toplevel windows.
	 */
	struct wlr_scene_tree *tree = node->parent;
	while (tree && !tree->node.data) {
		tree = tree->node.parent;
	}
	return tree ? tree->node.data : NULL;
}

static void
process_cursor_move(struct wm_server *server, uint32_t time)
{
	/*
	 * TODO: This will be implemented fully once views (task #4) are
	 * available. For now the grab geometry fields are set up but
	 * there's no view to move.
	 */
	(void)time;
	if (!server->grabbed_view) {
		return;
	}

	/*
	 * Move the grabbed view to the new position:
	 *   new_x = cursor_x - grab_x + grab_geobox.x
	 *   new_y = cursor_y - grab_y + grab_geobox.y
	 *
	 * The actual call to move the scene tree node will be:
	 *   wlr_scene_node_set_position(view->scene_tree->node, ...)
	 * once the wm_view struct is defined.
	 */
}

static void
process_cursor_resize(struct wm_server *server, uint32_t time)
{
	(void)time;
	if (!server->grabbed_view) {
		return;
	}
	/*
	 * TODO: Implement interactive resize once views (task #4) are
	 * available.  Resize requires adjusting the view geometry based
	 * on which edges are being grabbed, then sending a configure
	 * to the client with the new size.
	 */
}

static void
process_cursor_motion(struct wm_server *server, uint32_t time)
{
	if (server->cursor_mode == WM_CURSOR_MOVE) {
		process_cursor_move(server, time);
		return;
	}
	if (server->cursor_mode == WM_CURSOR_RESIZE) {
		process_cursor_resize(server, time);
		return;
	}

	/* Default: passthrough - find the surface under cursor */
	double sx, sy;
	struct wlr_surface *surface = NULL;
	struct wm_view *view = view_at(server,
		server->cursor->x, server->cursor->y, &surface, &sx, &sy);

	if (!view) {
		/* No view under cursor - set default cursor image */
		wlr_cursor_set_xcursor(server->cursor,
			server->cursor_mgr, "default");
	}

	if (surface) {
		/*
		 * Send pointer enter/motion to the surface.  wlr_seat
		 * handles enter/leave events automatically when the
		 * focused surface changes.
		 */
		wlr_seat_pointer_notify_enter(server->seat,
			surface, sx, sy);
		wlr_seat_pointer_notify_motion(server->seat,
			time, sx, sy);
	} else {
		/* Clear pointer focus when not over any surface */
		wlr_seat_pointer_clear_focus(server->seat);
	}
}

static void
handle_cursor_motion(struct wl_listener *listener, void *data)
{
	struct wm_server *server =
		wl_container_of(listener, server, cursor_motion);
	struct wlr_pointer_motion_event *event = data;

	wlr_cursor_move(server->cursor, &event->pointer->base,
		event->delta_x, event->delta_y);
	process_cursor_motion(server, event->time_msec);
}

static void
handle_cursor_motion_absolute(struct wl_listener *listener, void *data)
{
	struct wm_server *server =
		wl_container_of(listener, server, cursor_motion_absolute);
	struct wlr_pointer_motion_absolute_event *event = data;

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

	/* Notify seat of button event */
	wlr_seat_pointer_notify_button(server->seat,
		event->time_msec, event->button, event->state);

	if (event->state == WL_POINTER_BUTTON_STATE_RELEASED) {
		/* End any interactive move/resize on button release */
		if (server->cursor_mode != WM_CURSOR_PASSTHROUGH) {
			server->cursor_mode = WM_CURSOR_PASSTHROUGH;
			server->grabbed_view = NULL;
		}
		return;
	}

	/* On press: focus the view under the cursor */
	double sx, sy;
	struct wlr_surface *surface = NULL;
	struct wm_view *view = view_at(server,
		server->cursor->x, server->cursor->y, &surface, &sx, &sy);

	if (!view) {
		return;
	}

	/*
	 * TODO: Once views (task #4) and focus management (task #5) are
	 * implemented, this will:
	 *   1. Focus the clicked view
	 *   2. Raise it to the top of the stacking order
	 */
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

	/* Notify the client that a frame of pointer events is complete */
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
}

void
wm_cursor_finish(struct wm_server *server)
{
	wl_list_remove(&server->cursor_motion.link);
	wl_list_remove(&server->cursor_motion_absolute.link);
	wl_list_remove(&server->cursor_button.link);
	wl_list_remove(&server->cursor_axis.link);
	wl_list_remove(&server->cursor_frame.link);
}
