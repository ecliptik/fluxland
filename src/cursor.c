/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * cursor.c - Pointer/cursor input handling
 *
 * Handles mouse events with context-sensitive binding dispatch.
 * Determines click context from decoration hit-testing, looks up
 * mouse bindings, and dispatches actions.
 */

#define _GNU_SOURCE
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
#include "server.h"
#include "decoration.h"
#include "render.h"
#include "idle.h"
#include "menu.h"
#include "output.h"
#include "placement.h"
#include "session_lock.h"
#include "slit.h"
#ifdef WM_HAS_SYSTRAY
#include "systray.h"
#endif
#include "tabgroup.h"
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

/* --- Position overlay (pixel buffer bridge, same as menu.c) --- */

struct wm_cursor_pixel_buffer {
	struct wlr_buffer base;
	void *data;
	uint32_t format;
	size_t stride;
};

static void cursor_pixel_buffer_destroy(struct wlr_buffer *wlr_buffer)
{
	struct wm_cursor_pixel_buffer *buffer =
		wl_container_of(wlr_buffer, buffer, base);
	free(buffer->data);
	free(buffer);
}

static bool cursor_pixel_buffer_begin_data_ptr_access(
	struct wlr_buffer *wlr_buffer, uint32_t flags,
	void **data, uint32_t *format, size_t *stride)
{
	struct wm_cursor_pixel_buffer *buffer =
		wl_container_of(wlr_buffer, buffer, base);
	if (flags & WLR_BUFFER_DATA_PTR_ACCESS_WRITE)
		return false;
	*data = buffer->data;
	*format = buffer->format;
	*stride = buffer->stride;
	return true;
}

static void cursor_pixel_buffer_end_data_ptr_access(
	struct wlr_buffer *wlr_buffer)
{
	/* nothing */
}

static const struct wlr_buffer_impl cursor_pixel_buffer_impl = {
	.destroy = cursor_pixel_buffer_destroy,
	.begin_data_ptr_access = cursor_pixel_buffer_begin_data_ptr_access,
	.end_data_ptr_access = cursor_pixel_buffer_end_data_ptr_access,
};

static struct wlr_buffer *
cursor_wlr_buffer_from_cairo(cairo_surface_t *surface)
{
	if (!surface)
		return NULL;

	cairo_surface_flush(surface);

	int width = cairo_image_surface_get_width(surface);
	int height = cairo_image_surface_get_height(surface);
	int stride = cairo_image_surface_get_stride(surface);
	unsigned char *src = cairo_image_surface_get_data(surface);

	if (width <= 0 || height <= 0 || stride <= 0 || !src) {
		cairo_surface_destroy(surface);
		return NULL;
	}

	size_t size = (size_t)stride * (size_t)height;
	void *data = malloc(size);
	if (!data) {
		cairo_surface_destroy(surface);
		return NULL;
	}
	memcpy(data, src, size);

	struct wm_cursor_pixel_buffer *buffer =
		calloc(1, sizeof(*buffer));
	if (!buffer) {
		free(data);
		cairo_surface_destroy(surface);
		return NULL;
	}

	wlr_buffer_init(&buffer->base, &cursor_pixel_buffer_impl,
		width, height);
	buffer->data = data;
	buffer->format = DRM_FORMAT_ARGB8888;
	buffer->stride = stride;

	cairo_surface_destroy(surface);
	return &buffer->base;
}

static void
position_overlay_destroy(struct wm_server *server)
{
	if (server->position_overlay) {
		wlr_scene_node_destroy(
			&server->position_overlay->node);
		server->position_overlay = NULL;
	}
}

static void
position_overlay_update(struct wm_server *server, const char *text)
{
	if (!server->show_position || !server->grabbed_view)
		return;

	struct wm_style *style = server->style;
	struct wm_font font = {
		.family = "sans",
		.size = 11,
		.bold = true,
		.italic = false,
		.shadow_x = 0,
		.shadow_y = 0,
		.shadow_color = {0, 0, 0, 0xFF},
	};
	struct wm_color fg = {0xE0, 0xE0, 0xE0, 0xFF};
	if (style) {
		font = style->toolbar_font;
		fg = style->toolbar_text_color;
	}

	int tw = 0, th = 0;
	cairo_surface_t *text_surf = wm_render_text(text, &font,
		&fg, 300, &tw, &th, WM_JUSTIFY_CENTER, 1.0f);
	if (!text_surf)
		return;

	/* Create background surface with padding */
	int pad = 4;
	int w = tw + 2 * pad;
	int h = th + 2 * pad;
	cairo_surface_t *surf = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, w, h);
	if (cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(surf);
		cairo_surface_destroy(text_surf);
		return;
	}
	cairo_t *cr = cairo_create(surf);
	cairo_set_source_rgba(cr, 0.1, 0.1, 0.1, 0.85);
	cairo_paint(cr);
	cairo_set_source_surface(cr, text_surf, pad,
		(h - th) / 2);
	cairo_paint(cr);
	cairo_destroy(cr);
	cairo_surface_destroy(text_surf);

	struct wlr_buffer *buf = cursor_wlr_buffer_from_cairo(surf);
	if (!buf)
		return;

	if (server->position_overlay) {
		wlr_scene_buffer_set_buffer(
			server->position_overlay, buf);
	} else {
		server->position_overlay =
			wlr_scene_buffer_create(
				server->layer_overlay, buf);
	}
	wlr_buffer_drop(buf);

	if (server->position_overlay) {
		/* Position near the top-left corner of the window */
		struct wm_view *view = server->grabbed_view;
		int ox = view->x + 4;
		int oy = view->y - h - 2;
		if (oy < 0)
			oy = view->y + 4;
		wlr_scene_node_set_position(
			&server->position_overlay->node, ox, oy);
	}
}

/* --- Wireframe helpers --- */

#define WIREFRAME_BORDER 2

static void
wireframe_show(struct wm_server *server, int x, int y, int w, int h)
{
	if (w < 1) w = 1;
	if (h < 1) h = 1;
	float color[4] = {1.0f, 1.0f, 1.0f, 0.8f};
	int b = WIREFRAME_BORDER;

	if (!server->wireframe_rects[0]) {
		/* Create 4 rect nodes: top, right, bottom, left */
		server->wireframe_rects[0] = wlr_scene_rect_create(
			server->layer_overlay, w, b, color);
		server->wireframe_rects[1] = wlr_scene_rect_create(
			server->layer_overlay, b, h, color);
		server->wireframe_rects[2] = wlr_scene_rect_create(
			server->layer_overlay, w, b, color);
		server->wireframe_rects[3] = wlr_scene_rect_create(
			server->layer_overlay, b, h, color);
	} else {
		/* Resize existing rects */
		wlr_scene_rect_set_size(server->wireframe_rects[0], w, b);
		wlr_scene_rect_set_size(server->wireframe_rects[1], b, h);
		wlr_scene_rect_set_size(server->wireframe_rects[2], w, b);
		wlr_scene_rect_set_size(server->wireframe_rects[3], b, h);
	}

	/* Position: top, right, bottom, left */
	wlr_scene_node_set_position(
		&server->wireframe_rects[0]->node, x, y);
	wlr_scene_node_set_position(
		&server->wireframe_rects[1]->node, x + w - b, y);
	wlr_scene_node_set_position(
		&server->wireframe_rects[2]->node, x, y + h - b);
	wlr_scene_node_set_position(
		&server->wireframe_rects[3]->node, x, y);

	server->wireframe_active = true;
	server->wireframe_x = x;
	server->wireframe_y = y;
	server->wireframe_w = w;
	server->wireframe_h = h;
}

static void
wireframe_hide(struct wm_server *server)
{
	for (int i = 0; i < 4; i++) {
		if (server->wireframe_rects[i]) {
			wlr_scene_node_destroy(
				&server->wireframe_rects[i]->node);
			server->wireframe_rects[i] = NULL;
		}
	}
	server->wireframe_active = false;
}

/* --- Window snap zone helpers --- */

/*
 * Detect which snap zone the cursor is in based on its position
 * relative to the output's usable area edges.
 */
static enum wm_snap_zone
snap_zone_detect(struct wm_server *server, struct wlr_box *usable)
{
	int threshold = 10;
	if (server->config)
		threshold = server->config->snap_zone_threshold;

	int cx = (int)server->cursor->x;
	int cy = (int)server->cursor->y;

	bool at_left = (cx - usable->x) < threshold;
	bool at_right = (usable->x + usable->width - cx) < threshold;
	bool at_top = (cy - usable->y) < threshold;
	bool at_bottom = (usable->y + usable->height - cy) < threshold;

	/* Corners take priority over edges */
	if (at_left && at_top)
		return WM_SNAP_ZONE_TOPLEFT;
	if (at_right && at_top)
		return WM_SNAP_ZONE_TOPRIGHT;
	if (at_left && at_bottom)
		return WM_SNAP_ZONE_BOTTOMLEFT;
	if (at_right && at_bottom)
		return WM_SNAP_ZONE_BOTTOMRIGHT;

	/* Edges */
	if (at_left)
		return WM_SNAP_ZONE_LEFT_HALF;
	if (at_right)
		return WM_SNAP_ZONE_RIGHT_HALF;
	if (at_top)
		return WM_SNAP_ZONE_TOP_HALF;
	if (at_bottom)
		return WM_SNAP_ZONE_BOTTOM_HALF;

	return WM_SNAP_ZONE_NONE;
}

/*
 * Calculate the target geometry for a given snap zone
 * within the output's usable area.
 */
static struct wlr_box
snap_zone_geometry(enum wm_snap_zone zone, struct wlr_box *usable)
{
	struct wlr_box box = {0};
	int half_w = usable->width / 2;
	int half_h = usable->height / 2;

	switch (zone) {
	case WM_SNAP_ZONE_LEFT_HALF:
		box.x = usable->x;
		box.y = usable->y;
		box.width = half_w;
		box.height = usable->height;
		break;
	case WM_SNAP_ZONE_RIGHT_HALF:
		box.x = usable->x + half_w;
		box.y = usable->y;
		box.width = usable->width - half_w;
		box.height = usable->height;
		break;
	case WM_SNAP_ZONE_TOP_HALF:
		box.x = usable->x;
		box.y = usable->y;
		box.width = usable->width;
		box.height = half_h;
		break;
	case WM_SNAP_ZONE_BOTTOM_HALF:
		box.x = usable->x;
		box.y = usable->y + half_h;
		box.width = usable->width;
		box.height = usable->height - half_h;
		break;
	case WM_SNAP_ZONE_TOPLEFT:
		box.x = usable->x;
		box.y = usable->y;
		box.width = half_w;
		box.height = half_h;
		break;
	case WM_SNAP_ZONE_TOPRIGHT:
		box.x = usable->x + half_w;
		box.y = usable->y;
		box.width = usable->width - half_w;
		box.height = half_h;
		break;
	case WM_SNAP_ZONE_BOTTOMLEFT:
		box.x = usable->x;
		box.y = usable->y + half_h;
		box.width = half_w;
		box.height = usable->height - half_h;
		break;
	case WM_SNAP_ZONE_BOTTOMRIGHT:
		box.x = usable->x + half_w;
		box.y = usable->y + half_h;
		box.width = usable->width - half_w;
		box.height = usable->height - half_h;
		break;
	default:
		break;
	}
	return box;
}

/*
 * Destroy the snap preview overlay.
 */
static void
snap_preview_destroy(struct wm_server *server)
{
	if (server->snap_preview) {
		wlr_scene_node_destroy(&server->snap_preview->node);
		server->snap_preview = NULL;
	}
	server->snap_zone = WM_SNAP_ZONE_NONE;
}

/*
 * Create or update the snap preview overlay showing the target zone.
 * Renders a semi-transparent rectangle with a border outline.
 */
static void
snap_preview_update(struct wm_server *server, struct wlr_box *box)
{
	if (box->width < 1 || box->height < 1)
		return;

	/* Create a Cairo surface for the preview */
	cairo_surface_t *surf = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, box->width, box->height);
	if (cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(surf);
		return;
	}

	cairo_t *cr = cairo_create(surf);

	/* Fill with semi-transparent highlight (30% opacity blue) */
	cairo_set_source_rgba(cr, 0.3, 0.5, 0.8, 0.3);
	cairo_paint(cr);

	/* Draw a 3px border outline */
	double border = 3.0;
	cairo_set_source_rgba(cr, 0.3, 0.5, 0.8, 0.7);
	cairo_set_line_width(cr, border);
	cairo_rectangle(cr, border / 2.0, border / 2.0,
		box->width - border, box->height - border);
	cairo_stroke(cr);

	cairo_destroy(cr);

	struct wlr_buffer *buf = cursor_wlr_buffer_from_cairo(surf);
	if (!buf)
		return;

	if (server->snap_preview) {
		wlr_scene_buffer_set_buffer(server->snap_preview, buf);
	} else {
		server->snap_preview = wlr_scene_buffer_create(
			server->layer_overlay, buf);
	}
	wlr_buffer_drop(buf);

	if (server->snap_preview) {
		wlr_scene_node_set_position(
			&server->snap_preview->node, box->x, box->y);
	}
}

/*
 * Check cursor position against output edges during a window move
 * and show/update/hide the snap preview as appropriate.
 */
static void
snap_zone_check(struct wm_server *server)
{
	if (!server->config || !server->config->enable_window_snapping) {
		if (server->snap_zone != WM_SNAP_ZONE_NONE)
			snap_preview_destroy(server);
		return;
	}

	/* Find the output the cursor is on */
	struct wlr_output *wlr_output = wlr_output_layout_output_at(
		server->output_layout, server->cursor->x, server->cursor->y);
	if (!wlr_output) {
		if (server->snap_zone != WM_SNAP_ZONE_NONE)
			snap_preview_destroy(server);
		return;
	}

	/* Find our wm_output wrapper to get usable_area */
	struct wm_output *output;
	struct wlr_box usable = {0};
	bool found = false;
	wl_list_for_each(output, &server->outputs, link) {
		if (output->wlr_output == wlr_output) {
			usable = output->usable_area;
			found = true;
			break;
		}
	}
	if (!found) {
		if (server->snap_zone != WM_SNAP_ZONE_NONE)
			snap_preview_destroy(server);
		return;
	}

	enum wm_snap_zone zone = snap_zone_detect(server, &usable);

	if (zone == WM_SNAP_ZONE_NONE) {
		if (server->snap_zone != WM_SNAP_ZONE_NONE)
			snap_preview_destroy(server);
		return;
	}

	/* Zone changed — update the preview */
	if ((int)zone != server->snap_zone) {
		server->snap_zone = zone;
		server->snap_preview_box = snap_zone_geometry(zone, &usable);
		snap_preview_update(server, &server->snap_preview_box);
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
		wlr_xdg_toplevel_set_size(view->xdg_toplevel,
			new_w, new_h);
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

				sv->x = sb.x;
				sv->y = sb.y;
				wlr_scene_node_set_position(
					&sv->scene_tree->node,
					sv->x, sv->y);
				wlr_xdg_toplevel_set_size(
					sv->xdg_toplevel,
					sb.width, sb.height);

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
					wlr_xdg_toplevel_set_size(
						wf_view->xdg_toplevel,
						server->wireframe_w,
						server->wireframe_h);
				}
				wireframe_hide(server);
			}
			server->cursor_mode = WM_CURSOR_PASSTHROUGH;
			server->grabbed_view = NULL;
			position_overlay_destroy(server);
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
	uint32_t dclick_ms = WM_DOUBLE_CLICK_MS;
	if (server->config)
		dclick_ms = (uint32_t)server->config->double_click_interval;
	if (event->button == ms->last_button &&
	    (event->time_msec - ms->last_time_msec) < dclick_ms) {
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
		server->focus_user_initiated = true;
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
