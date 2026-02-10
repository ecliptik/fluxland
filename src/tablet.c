/*
 * wm-wayland - A Fluxbox-inspired Wayland compositor
 * tablet.c - Tablet/stylus input support via tablet-v2 protocol
 *
 * Handles tablet tool (stylus) and tablet pad devices using the
 * wlr_tablet_v2 protocol. Tablet tool events (axis, tip, proximity,
 * button) are forwarded to clients via the tablet-v2 protocol, and
 * axis events also move the compositor cursor for seamless integration
 * with pointer-based window management.
 */

#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_tablet_pad.h>
#include <wlr/types/wlr_tablet_tool.h>
#include <wlr/types/wlr_tablet_v2.h>
#include <wlr/util/log.h>

#include "idle.h"
#include "server.h"
#include "tablet.h"
#include "view.h"

/*
 * Per-tablet device state. We listen on the wlr_tablet's events (axis,
 * proximity, tip, button) and forward them to clients via the v2
 * protocol. The wlr_tablet_v2_tablet_tool is created lazily per
 * physical wlr_tablet_tool and stored in tool->data.
 */
struct wm_tablet {
	struct wm_server *server;
	struct wlr_tablet_v2_tablet *tablet_v2;
	struct wlr_tablet *wlr_tablet;

	struct wl_listener tool_axis;
	struct wl_listener tool_proximity;
	struct wl_listener tool_tip;
	struct wl_listener tool_button;
	struct wl_listener destroy;

	struct wl_list link; /* wm_server.tablets */
};

/*
 * Per-tablet-pad state.
 */
struct wm_tablet_pad {
	struct wm_server *server;
	struct wlr_tablet_v2_tablet_pad *pad_v2;

	struct wl_listener pad_button;
	struct wl_listener pad_ring;
	struct wl_listener pad_strip;
	struct wl_listener pad_attach;
	struct wl_listener destroy;

	struct wl_list link; /* wm_server.tablet_pads */
};

/*
 * Find the view under the given layout coordinates, returning the
 * wlr_surface and surface-local coordinates.
 */
static struct wm_view *
tablet_view_at(struct wm_server *server, double lx, double ly,
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
 * Get or create a wlr_tablet_v2_tablet_tool for a physical tool.
 * Each wlr_tablet_tool (physical stylus) gets exactly one v2 tool
 * instance, stored in its data pointer.
 */
static struct wlr_tablet_v2_tablet_tool *
get_or_create_tool_v2(struct wm_server *server,
	struct wlr_tablet_tool *wlr_tool)
{
	if (wlr_tool->data) {
		return wlr_tool->data;
	}

	struct wlr_tablet_v2_tablet_tool *tool_v2 = wlr_tablet_tool_create(
		server->tablet_manager, server->seat, wlr_tool);
	if (!tool_v2) {
		wlr_log(WLR_ERROR, "failed to create tablet_tool_v2");
		return NULL;
	}

	wlr_tool->data = tool_v2;
	return tool_v2;
}

/*
 * Tablet tool axis event: move the cursor and forward tool data to
 * the focused client via the tablet-v2 protocol.
 */
static void
handle_tool_axis(struct wl_listener *listener, void *data)
{
	struct wm_tablet *tablet =
		wl_container_of(listener, tablet, tool_axis);
	struct wm_server *server = tablet->server;
	struct wlr_tablet_tool_axis_event *event = data;

	wm_idle_notify_activity(server);

	struct wlr_tablet_v2_tablet_tool *tool_v2 =
		get_or_create_tool_v2(server, event->tool);
	if (!tool_v2) {
		return;
	}

	/*
	 * Map tablet coordinates to layout coordinates and warp the
	 * compositor cursor so that pointer-based window management
	 * (focus-follows-mouse, menus) works naturally with the stylus.
	 */
	if (event->updated_axes &
	    (WLR_TABLET_TOOL_AXIS_X | WLR_TABLET_TOOL_AXIS_Y)) {
		wlr_cursor_warp_absolute(server->cursor,
			&event->tablet->base, event->x, event->y);
	}

	double lx = server->cursor->x;
	double ly = server->cursor->y;

	double sx, sy;
	struct wlr_surface *surface = NULL;
	tablet_view_at(server, lx, ly, &surface, &sx, &sy);

	if (surface && wlr_surface_accepts_tablet_v2(
			tablet->tablet_v2, surface)) {
		if (tool_v2->focused_surface != surface) {
			wlr_send_tablet_v2_tablet_tool_proximity_in(
				tool_v2, tablet->tablet_v2, surface);
		}
		wlr_send_tablet_v2_tablet_tool_motion(tool_v2, sx, sy);
	} else if (tool_v2->focused_surface) {
		wlr_send_tablet_v2_tablet_tool_proximity_out(tool_v2);
	}

	/* Forward pressure, distance, tilt, rotation, slider, wheel */
	if (event->updated_axes & WLR_TABLET_TOOL_AXIS_PRESSURE) {
		wlr_send_tablet_v2_tablet_tool_pressure(
			tool_v2, event->pressure);
	}
	if (event->updated_axes & WLR_TABLET_TOOL_AXIS_DISTANCE) {
		wlr_send_tablet_v2_tablet_tool_distance(
			tool_v2, event->distance);
	}
	if (event->updated_axes &
	    (WLR_TABLET_TOOL_AXIS_TILT_X | WLR_TABLET_TOOL_AXIS_TILT_Y)) {
		wlr_send_tablet_v2_tablet_tool_tilt(
			tool_v2, event->tilt_x, event->tilt_y);
	}
	if (event->updated_axes & WLR_TABLET_TOOL_AXIS_ROTATION) {
		wlr_send_tablet_v2_tablet_tool_rotation(
			tool_v2, event->rotation);
	}
	if (event->updated_axes & WLR_TABLET_TOOL_AXIS_SLIDER) {
		wlr_send_tablet_v2_tablet_tool_slider(
			tool_v2, event->slider);
	}
	if (event->updated_axes & WLR_TABLET_TOOL_AXIS_WHEEL) {
		wlr_send_tablet_v2_tablet_tool_wheel(
			tool_v2, event->wheel_delta, 0);
	}

	/* Also notify the seat pointer so focus-follows-mouse works */
	if (surface) {
		wlr_seat_pointer_notify_enter(server->seat, surface, sx, sy);
		wlr_seat_pointer_notify_motion(server->seat,
			event->time_msec, sx, sy);
	} else {
		wlr_seat_pointer_clear_focus(server->seat);
	}
}

/*
 * Tablet tool proximity event: handle tool entering/leaving detection
 * range above the tablet surface.
 */
static void
handle_tool_proximity(struct wl_listener *listener, void *data)
{
	struct wm_tablet *tablet =
		wl_container_of(listener, tablet, tool_proximity);
	struct wm_server *server = tablet->server;
	struct wlr_tablet_tool_proximity_event *event = data;

	wm_idle_notify_activity(server);

	struct wlr_tablet_v2_tablet_tool *tool_v2 =
		get_or_create_tool_v2(server, event->tool);
	if (!tool_v2) {
		return;
	}

	if (event->state == WLR_TABLET_TOOL_PROXIMITY_OUT) {
		wlr_send_tablet_v2_tablet_tool_proximity_out(tool_v2);
		return;
	}

	/* Proximity in: warp cursor and find surface */
	wlr_cursor_warp_absolute(server->cursor,
		&event->tablet->base, event->x, event->y);

	double sx, sy;
	struct wlr_surface *surface = NULL;
	tablet_view_at(server, server->cursor->x, server->cursor->y,
		&surface, &sx, &sy);

	if (surface && wlr_surface_accepts_tablet_v2(
			tablet->tablet_v2, surface)) {
		wlr_send_tablet_v2_tablet_tool_proximity_in(
			tool_v2, tablet->tablet_v2, surface);
	}
}

/*
 * Tablet tool tip event: stylus touching / lifting from the tablet
 * surface. This is analogous to a pointer button press/release.
 */
static void
handle_tool_tip(struct wl_listener *listener, void *data)
{
	struct wm_tablet *tablet =
		wl_container_of(listener, tablet, tool_tip);
	struct wm_server *server = tablet->server;
	struct wlr_tablet_tool_tip_event *event = data;

	wm_idle_notify_activity(server);

	struct wlr_tablet_v2_tablet_tool *tool_v2 =
		get_or_create_tool_v2(server, event->tool);
	if (!tool_v2) {
		return;
	}

	if (event->state == WLR_TABLET_TOOL_TIP_DOWN) {
		wlr_send_tablet_v2_tablet_tool_down(tool_v2);

		/* Focus the view under the stylus on tip-down */
		double sx, sy;
		struct wlr_surface *surface = NULL;
		struct wm_view *view = tablet_view_at(server,
			server->cursor->x, server->cursor->y,
			&surface, &sx, &sy);
		if (view) {
			wm_focus_view(view, surface);
		}
	} else {
		wlr_send_tablet_v2_tablet_tool_up(tool_v2);
	}
}

/*
 * Tablet tool button event: stylus side buttons.
 */
static void
handle_tool_button(struct wl_listener *listener, void *data)
{
	struct wm_tablet *tablet =
		wl_container_of(listener, tablet, tool_button);
	struct wlr_tablet_tool_button_event *event = data;

	wm_idle_notify_activity(tablet->server);

	struct wlr_tablet_v2_tablet_tool *tool_v2 =
		get_or_create_tool_v2(tablet->server, event->tool);
	if (!tool_v2) {
		return;
	}

	wlr_send_tablet_v2_tablet_tool_button(tool_v2,
		event->button,
		event->state == WLR_BUTTON_PRESSED ?
			ZWP_TABLET_PAD_V2_BUTTON_STATE_PRESSED :
			ZWP_TABLET_PAD_V2_BUTTON_STATE_RELEASED);
}

/*
 * Clean up when the underlying tablet device is destroyed.
 */
static void
handle_tablet_destroy(struct wl_listener *listener, void *data)
{
	(void)data;
	struct wm_tablet *tablet =
		wl_container_of(listener, tablet, destroy);

	wl_list_remove(&tablet->tool_axis.link);
	wl_list_remove(&tablet->tool_proximity.link);
	wl_list_remove(&tablet->tool_tip.link);
	wl_list_remove(&tablet->tool_button.link);
	wl_list_remove(&tablet->destroy.link);
	wl_list_remove(&tablet->link);
	free(tablet);
}

/*
 * Set up a new tablet tool (stylus) device.
 */
void
wm_tablet_tool_setup(struct wm_server *server,
	struct wlr_input_device *device)
{
	struct wlr_tablet *wlr_tablet = wlr_tablet_from_input_device(device);

	struct wlr_tablet_v2_tablet *tablet_v2 = wlr_tablet_create(
		server->tablet_manager, server->seat, device);
	if (!tablet_v2) {
		wlr_log(WLR_ERROR, "failed to create tablet_v2 for %s",
			device->name);
		return;
	}

	struct wm_tablet *tablet = calloc(1, sizeof(*tablet));
	if (!tablet) {
		wlr_log(WLR_ERROR, "failed to allocate tablet state");
		return;
	}
	tablet->server = server;
	tablet->tablet_v2 = tablet_v2;
	tablet->wlr_tablet = wlr_tablet;

	tablet->tool_axis.notify = handle_tool_axis;
	wl_signal_add(&wlr_tablet->events.axis, &tablet->tool_axis);

	tablet->tool_proximity.notify = handle_tool_proximity;
	wl_signal_add(&wlr_tablet->events.proximity,
		&tablet->tool_proximity);

	tablet->tool_tip.notify = handle_tool_tip;
	wl_signal_add(&wlr_tablet->events.tip, &tablet->tool_tip);

	tablet->tool_button.notify = handle_tool_button;
	wl_signal_add(&wlr_tablet->events.button, &tablet->tool_button);

	tablet->destroy.notify = handle_tablet_destroy;
	wl_signal_add(&device->events.destroy, &tablet->destroy);

	wl_list_insert(&server->tablets, &tablet->link);

	wlr_log(WLR_INFO, "new tablet: %s", device->name);
}

/*
 * Tablet pad button event.
 */
static void
handle_pad_button(struct wl_listener *listener, void *data)
{
	struct wm_tablet_pad *pad =
		wl_container_of(listener, pad, pad_button);
	struct wlr_tablet_pad_button_event *event = data;

	wm_idle_notify_activity(pad->server);

	wlr_send_tablet_v2_tablet_pad_button(pad->pad_v2,
		event->button, event->time_msec,
		event->state == WLR_BUTTON_PRESSED ?
			ZWP_TABLET_PAD_V2_BUTTON_STATE_PRESSED :
			ZWP_TABLET_PAD_V2_BUTTON_STATE_RELEASED);
}

/*
 * Tablet pad ring event.
 */
static void
handle_pad_ring(struct wl_listener *listener, void *data)
{
	struct wm_tablet_pad *pad =
		wl_container_of(listener, pad, pad_ring);
	struct wlr_tablet_pad_ring_event *event = data;

	wm_idle_notify_activity(pad->server);

	wlr_send_tablet_v2_tablet_pad_ring(pad->pad_v2,
		event->ring, event->position,
		event->source == WLR_TABLET_PAD_RING_SOURCE_FINGER,
		event->time_msec);
}

/*
 * Tablet pad strip event.
 */
static void
handle_pad_strip(struct wl_listener *listener, void *data)
{
	struct wm_tablet_pad *pad =
		wl_container_of(listener, pad, pad_strip);
	struct wlr_tablet_pad_strip_event *event = data;

	wm_idle_notify_activity(pad->server);

	wlr_send_tablet_v2_tablet_pad_strip(pad->pad_v2,
		event->strip, event->position,
		event->source == WLR_TABLET_PAD_STRIP_SOURCE_FINGER,
		event->time_msec);
}

/*
 * Tablet pad attach event: a tool was associated with the pad.
 */
static void
handle_pad_attach(struct wl_listener *listener, void *data)
{
	(void)listener;
	(void)data;
}

/*
 * Clean up when the underlying tablet pad device is destroyed.
 */
static void
handle_pad_destroy(struct wl_listener *listener, void *data)
{
	(void)data;
	struct wm_tablet_pad *pad =
		wl_container_of(listener, pad, destroy);

	wl_list_remove(&pad->pad_button.link);
	wl_list_remove(&pad->pad_ring.link);
	wl_list_remove(&pad->pad_strip.link);
	wl_list_remove(&pad->pad_attach.link);
	wl_list_remove(&pad->destroy.link);
	wl_list_remove(&pad->link);
	free(pad);
}

/*
 * Set up a new tablet pad device.
 */
void
wm_tablet_pad_setup(struct wm_server *server,
	struct wlr_input_device *device)
{
	struct wlr_tablet_pad *wlr_pad =
		wlr_tablet_pad_from_input_device(device);

	struct wlr_tablet_v2_tablet_pad *pad_v2 = wlr_tablet_pad_create(
		server->tablet_manager, server->seat, device);
	if (!pad_v2) {
		wlr_log(WLR_ERROR, "failed to create tablet_pad_v2 for %s",
			device->name);
		return;
	}

	struct wm_tablet_pad *pad = calloc(1, sizeof(*pad));
	if (!pad) {
		wlr_log(WLR_ERROR, "failed to allocate tablet pad state");
		return;
	}
	pad->server = server;
	pad->pad_v2 = pad_v2;

	pad->pad_button.notify = handle_pad_button;
	wl_signal_add(&wlr_pad->events.button, &pad->pad_button);

	pad->pad_ring.notify = handle_pad_ring;
	wl_signal_add(&wlr_pad->events.ring, &pad->pad_ring);

	pad->pad_strip.notify = handle_pad_strip;
	wl_signal_add(&wlr_pad->events.strip, &pad->pad_strip);

	pad->pad_attach.notify = handle_pad_attach;
	wl_signal_add(&wlr_pad->events.attach_tablet, &pad->pad_attach);

	pad->destroy.notify = handle_pad_destroy;
	wl_signal_add(&device->events.destroy, &pad->destroy);

	wl_list_insert(&server->tablet_pads, &pad->link);

	wlr_log(WLR_INFO, "new tablet pad: %s (%zu buttons, %zu rings, "
		"%zu strips)", device->name, wlr_pad->button_count,
		wlr_pad->ring_count, wlr_pad->strip_count);
}

void
wm_tablet_init(struct wm_server *server)
{
	server->tablet_manager = wlr_tablet_v2_create(server->wl_display);
	wl_list_init(&server->tablets);
	wl_list_init(&server->tablet_pads);

	wlr_log(WLR_INFO, "tablet-v2 protocol initialized");
}

void
wm_tablet_finish(struct wm_server *server)
{
	struct wm_tablet *tablet, *tablet_tmp;
	wl_list_for_each_safe(tablet, tablet_tmp,
			&server->tablets, link) {
		wl_list_remove(&tablet->tool_axis.link);
		wl_list_remove(&tablet->tool_proximity.link);
		wl_list_remove(&tablet->tool_tip.link);
		wl_list_remove(&tablet->tool_button.link);
		wl_list_remove(&tablet->destroy.link);
		wl_list_remove(&tablet->link);
		free(tablet);
	}

	struct wm_tablet_pad *pad, *pad_tmp;
	wl_list_for_each_safe(pad, pad_tmp,
			&server->tablet_pads, link) {
		wl_list_remove(&pad->pad_button.link);
		wl_list_remove(&pad->pad_ring.link);
		wl_list_remove(&pad->pad_strip.link);
		wl_list_remove(&pad->pad_attach.link);
		wl_list_remove(&pad->destroy.link);
		wl_list_remove(&pad->link);
		free(pad);
	}
}
