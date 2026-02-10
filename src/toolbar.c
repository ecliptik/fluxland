/*
 * wm-wayland - A Fluxbox-inspired Wayland compositor
 *
 * toolbar.c - Built-in toolbar with workspace switcher, window title, and clock
 */

#define _POSIX_C_SOURCE 200809L
#include <cairo.h>
#include <drm_fourcc.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/log.h>

#include "toolbar.h"
#include "config.h"
#include "output.h"
#include "render.h"
#include "server.h"
#include "style.h"
#include "view.h"
#include "workspace.h"

/* --- Cairo-to-wlr_buffer bridge (same as decoration.c) --- */

struct wm_pixel_buffer {
	struct wlr_buffer base;
	void *data;
	uint32_t format;
	size_t stride;
};

static void pixel_buffer_destroy(struct wlr_buffer *wlr_buffer)
{
	struct wm_pixel_buffer *buffer =
		wl_container_of(wlr_buffer, buffer, base);
	free(buffer->data);
	free(buffer);
}

static bool pixel_buffer_begin_data_ptr_access(struct wlr_buffer *wlr_buffer,
	uint32_t flags, void **data, uint32_t *format, size_t *stride)
{
	struct wm_pixel_buffer *buffer =
		wl_container_of(wlr_buffer, buffer, base);
	if (flags & WLR_BUFFER_DATA_PTR_ACCESS_WRITE) {
		return false;
	}
	*data = buffer->data;
	*format = buffer->format;
	*stride = buffer->stride;
	return true;
}

static void pixel_buffer_end_data_ptr_access(struct wlr_buffer *wlr_buffer)
{
	/* nothing to do */
}

static const struct wlr_buffer_impl pixel_buffer_impl = {
	.destroy = pixel_buffer_destroy,
	.begin_data_ptr_access = pixel_buffer_begin_data_ptr_access,
	.end_data_ptr_access = pixel_buffer_end_data_ptr_access,
};

static struct wlr_buffer *
wlr_buffer_from_cairo(cairo_surface_t *surface)
{
	if (!surface) {
		return NULL;
	}

	cairo_surface_flush(surface);

	int width = cairo_image_surface_get_width(surface);
	int height = cairo_image_surface_get_height(surface);
	int stride = cairo_image_surface_get_stride(surface);
	unsigned char *src = cairo_image_surface_get_data(surface);

	if (width <= 0 || height <= 0 || !src) {
		cairo_surface_destroy(surface);
		return NULL;
	}

	size_t size = (size_t)stride * height;
	void *data = malloc(size);
	if (!data) {
		cairo_surface_destroy(surface);
		return NULL;
	}
	memcpy(data, src, size);

	struct wm_pixel_buffer *buffer = calloc(1, sizeof(*buffer));
	if (!buffer) {
		free(data);
		cairo_surface_destroy(surface);
		return NULL;
	}

	wlr_buffer_init(&buffer->base, &pixel_buffer_impl, width, height);
	buffer->data = data;
	buffer->format = DRM_FORMAT_ARGB8888;
	buffer->stride = stride;

	cairo_surface_destroy(surface);
	return &buffer->base;
}

/* --- Helper: get primary output --- */

static struct wm_output *
get_primary_output(struct wm_server *server)
{
	if (wl_list_empty(&server->outputs)) {
		return NULL;
	}
	return wl_container_of(server->outputs.next, (struct wm_output *)NULL,
		link);
}

/* --- Render workspace buttons --- */

static struct wlr_buffer *
render_workspace_buttons(struct wm_toolbar *toolbar, int width, int height)
{
	struct wm_server *server = toolbar->server;
	struct wm_style *style = server->style;
	int ws_count = server->workspace_count;
	int current = server->current_workspace ?
		server->current_workspace->index : 0;

	cairo_surface_t *surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(surface);
		return NULL;
	}

	cairo_t *cr = cairo_create(surface);

	/* Clear */
	cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
	cairo_paint(cr);
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

	int btn_width = 0;
	if (ws_count > 0) {
		btn_width = width / ws_count;
	}

	/* Free old hit boxes */
	free(toolbar->ws_boxes);
	toolbar->ws_boxes = calloc(ws_count, sizeof(struct wlr_box));
	toolbar->ws_box_count = ws_count;

	struct wm_workspace *ws;
	int i = 0;
	wl_list_for_each(ws, &server->workspaces, link) {
		int bx = i * btn_width;
		int bw = (i == ws_count - 1) ? (width - bx) : btn_width;

		/* Render button background */
		if (i == current) {
			/* Active workspace: use toolbar texture with
			 * brightened appearance */
			cairo_surface_t *bg =
				wm_render_texture(&style->toolbar_texture,
					bw, height, 1.0f);
			if (bg) {
				/* Brighten active button */
				cairo_t *bcr = cairo_create(bg);
				cairo_set_source_rgba(bcr, 1, 1, 1, 0.15);
				cairo_paint(bcr);
				cairo_destroy(bcr);
				cairo_set_source_surface(cr, bg, bx, 0);
				cairo_paint(cr);
				cairo_surface_destroy(bg);
			} else {
				cairo_set_source_rgba(cr, 0.4, 0.4, 0.5, 1.0);
				cairo_rectangle(cr, bx, 0, bw, height);
				cairo_fill(cr);
			}
		} else {
			/* Inactive workspace: slightly darker */
			cairo_set_source_rgba(cr, 0, 0, 0, 0.2);
			cairo_rectangle(cr, bx, 0, bw, height);
			cairo_fill(cr);
		}

		/* Draw separator line */
		if (i > 0) {
			cairo_set_source_rgba(cr,
				style->toolbar_text_color.r / 255.0,
				style->toolbar_text_color.g / 255.0,
				style->toolbar_text_color.b / 255.0, 0.3);
			cairo_set_line_width(cr, 1.0);
			cairo_move_to(cr, bx + 0.5, 2);
			cairo_line_to(cr, bx + 0.5, height - 2);
			cairo_stroke(cr);
		}

		/* Draw workspace name/number */
		const char *name = ws->name;
		char fallback[8];
		if (!name || !*name) {
			snprintf(fallback, sizeof(fallback), "%d", i + 1);
			name = fallback;
		}

		int tw, th;
		cairo_surface_t *text = wm_render_text(name,
			&style->toolbar_font, &style->toolbar_text_color,
			bw - 4, &tw, &th, WM_JUSTIFY_CENTER, 1.0f);
		if (text) {
			int tx = bx + (bw - tw) / 2;
			int ty = (height - th) / 2;
			if (ty < 0) ty = 0;
			cairo_set_source_surface(cr, text, tx, ty);
			cairo_paint(cr);
			cairo_surface_destroy(text);
		}

		/* Store hit box (in toolbar-local coords) */
		if (toolbar->ws_boxes) {
			toolbar->ws_boxes[i].x = bx;
			toolbar->ws_boxes[i].y = 0;
			toolbar->ws_boxes[i].width = bw;
			toolbar->ws_boxes[i].height = height;
		}

		i++;
	}

	cairo_destroy(cr);
	return wlr_buffer_from_cairo(surface);
}

/* --- Render window title --- */

static struct wlr_buffer *
render_title(struct wm_toolbar *toolbar, int width, int height)
{
	struct wm_server *server = toolbar->server;
	struct wm_style *style = server->style;

	const char *title = "";
	if (server->focused_view && server->focused_view->title) {
		title = server->focused_view->title;
	}

	/* Render background (transparent, text only) */
	cairo_surface_t *surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(surface);
		return NULL;
	}

	cairo_t *cr = cairo_create(surface);
	cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
	cairo_paint(cr);
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

	if (*title) {
		int tw, th;
		cairo_surface_t *text = wm_render_text(title,
			&style->toolbar_font, &style->toolbar_text_color,
			width - 8, &tw, &th, WM_JUSTIFY_CENTER, 1.0f);
		if (text) {
			int tx = (width - tw) / 2;
			int ty = (height - th) / 2;
			if (tx < 0) tx = 0;
			if (ty < 0) ty = 0;
			cairo_set_source_surface(cr, text, tx, ty);
			cairo_paint(cr);
			cairo_surface_destroy(text);
		}
	}

	cairo_destroy(cr);
	return wlr_buffer_from_cairo(surface);
}

/* --- Render clock --- */

static struct wlr_buffer *
render_clock(struct wm_toolbar *toolbar, int width, int height)
{
	struct wm_style *style = toolbar->server->style;

	time_t now = time(NULL);
	struct tm tm;
	localtime_r(&now, &tm);

	char timebuf[64];
	strftime(timebuf, sizeof(timebuf), WM_TOOLBAR_CLOCK_FMT, &tm);

	/* Cache to avoid redraw if time string unchanged */
	if (strcmp(timebuf, toolbar->cached_clock) == 0) {
		return NULL; /* no change */
	}
	strncpy(toolbar->cached_clock, timebuf, sizeof(toolbar->cached_clock));
	toolbar->cached_clock[sizeof(toolbar->cached_clock) - 1] = '\0';

	cairo_surface_t *surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(surface);
		return NULL;
	}

	cairo_t *cr = cairo_create(surface);
	cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
	cairo_paint(cr);
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

	int tw, th;
	cairo_surface_t *text = wm_render_text(timebuf,
		&style->toolbar_font, &style->toolbar_text_color,
		width - 4, &tw, &th, WM_JUSTIFY_CENTER, 1.0f);
	if (text) {
		int tx = (width - tw) / 2;
		int ty = (height - th) / 2;
		if (tx < 0) tx = 0;
		if (ty < 0) ty = 0;
		cairo_set_source_surface(cr, text, tx, ty);
		cairo_paint(cr);
		cairo_surface_destroy(text);
	}

	cairo_destroy(cr);
	return wlr_buffer_from_cairo(surface);
}

/* --- Full toolbar render --- */

static void
toolbar_render(struct wm_toolbar *toolbar)
{
	if (!toolbar || !toolbar->visible) {
		return;
	}

	struct wm_server *server = toolbar->server;
	struct wm_style *style = server->style;
	int w = toolbar->width;
	int h = toolbar->height;

	if (w <= 0 || h <= 0) {
		return;
	}

	/* Background */
	struct wlr_buffer *bg = wlr_buffer_from_cairo(
		wm_render_texture(&style->toolbar_texture, w, h, 1.0f));
	wlr_scene_buffer_set_buffer(toolbar->bg_buf, bg);
	if (bg) {
		wlr_buffer_drop(bg);
	}

	/* Layout: workspace buttons (left 25%), title (center 50%),
	 * clock (right 25%) */
	int ws_width = w / 4;
	int clock_width = w / 6;
	int title_width = w - ws_width - clock_width;

	if (ws_width < 60) ws_width = 60;
	if (clock_width < 50) clock_width = 50;
	title_width = w - ws_width - clock_width;
	if (title_width < 0) title_width = 0;

	/* Workspace buttons */
	struct wlr_buffer *ws_buf = render_workspace_buttons(toolbar,
		ws_width, h);
	wlr_scene_buffer_set_buffer(toolbar->workspace_buf, ws_buf);
	if (ws_buf) {
		wlr_buffer_drop(ws_buf);
	}
	wlr_scene_node_set_position(&toolbar->workspace_buf->node, 0, 0);

	/* Title */
	struct wlr_buffer *title = render_title(toolbar, title_width, h);
	wlr_scene_buffer_set_buffer(toolbar->title_buf, title);
	if (title) {
		wlr_buffer_drop(title);
	}
	wlr_scene_node_set_position(&toolbar->title_buf->node, ws_width, 0);

	/* Clock */
	toolbar->cached_clock[0] = '\0'; /* force redraw */
	struct wlr_buffer *clock = render_clock(toolbar, clock_width, h);
	wlr_scene_buffer_set_buffer(toolbar->clock_buf, clock);
	if (clock) {
		wlr_buffer_drop(clock);
	}
	wlr_scene_node_set_position(&toolbar->clock_buf->node,
		w - clock_width, 0);
}

/* --- Clock timer callback --- */

static int
clock_timer_cb(void *data)
{
	struct wm_toolbar *toolbar = data;

	if (!toolbar->visible) {
		/* Re-arm even when hidden */
		wl_event_source_timer_update(toolbar->clock_timer, 1000);
		return 0;
	}

	int w = toolbar->width;
	int clock_width = w / 6;
	if (clock_width < 50) clock_width = 50;

	struct wlr_buffer *clock = render_clock(toolbar, clock_width,
		toolbar->height);
	if (clock) {
		/* Only update if render_clock returned a new buffer
		 * (time string changed) */
		wlr_scene_buffer_set_buffer(toolbar->clock_buf, clock);
		wlr_buffer_drop(clock);
	}

	/* Re-arm timer */
	wl_event_source_timer_update(toolbar->clock_timer, 1000);
	return 0;
}

/* --- Public API --- */

struct wm_toolbar *
wm_toolbar_create(struct wm_server *server)
{
	struct wm_toolbar *toolbar = calloc(1, sizeof(*toolbar));
	if (!toolbar) {
		return NULL;
	}

	toolbar->server = server;
	toolbar->visible = server->config ?
		server->config->toolbar_visible : true;
	toolbar->height = WM_TOOLBAR_HEIGHT;
	toolbar->cached_ws_index = -1;
	toolbar->cached_clock[0] = '\0';

	/* Create scene tree in layer_top so it renders above windows */
	toolbar->scene_tree = wlr_scene_tree_create(server->layer_top);
	if (!toolbar->scene_tree) {
		free(toolbar);
		return NULL;
	}

	/* Create scene buffer nodes */
	toolbar->bg_buf = wlr_scene_buffer_create(toolbar->scene_tree, NULL);
	toolbar->workspace_buf = wlr_scene_buffer_create(
		toolbar->scene_tree, NULL);
	toolbar->title_buf = wlr_scene_buffer_create(
		toolbar->scene_tree, NULL);
	toolbar->clock_buf = wlr_scene_buffer_create(
		toolbar->scene_tree, NULL);

	/* Set visibility */
	wlr_scene_node_set_enabled(&toolbar->scene_tree->node,
		toolbar->visible);

	/* Position and render */
	wm_toolbar_relayout(toolbar);

	/* Start clock timer (1-second interval) */
	toolbar->clock_timer = wl_event_loop_add_timer(
		server->wl_event_loop, clock_timer_cb, toolbar);
	if (toolbar->clock_timer) {
		wl_event_source_timer_update(toolbar->clock_timer, 1000);
	}

	wlr_log(WLR_INFO, "toolbar created (visible=%d)", toolbar->visible);
	return toolbar;
}

void
wm_toolbar_destroy(struct wm_toolbar *toolbar)
{
	if (!toolbar) {
		return;
	}

	if (toolbar->clock_timer) {
		wl_event_source_remove(toolbar->clock_timer);
	}

	if (toolbar->scene_tree) {
		wlr_scene_node_destroy(&toolbar->scene_tree->node);
	}

	free(toolbar->ws_boxes);
	free(toolbar->cached_title);
	free(toolbar);
}

void
wm_toolbar_update_workspace(struct wm_toolbar *toolbar)
{
	if (!toolbar || !toolbar->visible) {
		return;
	}

	int w = toolbar->width;
	int ws_width = w / 4;
	if (ws_width < 60) ws_width = 60;

	struct wlr_buffer *ws_buf = render_workspace_buttons(toolbar,
		ws_width, toolbar->height);
	wlr_scene_buffer_set_buffer(toolbar->workspace_buf, ws_buf);
	if (ws_buf) {
		wlr_buffer_drop(ws_buf);
	}
}

void
wm_toolbar_update_title(struct wm_toolbar *toolbar)
{
	if (!toolbar || !toolbar->visible) {
		return;
	}

	int w = toolbar->width;
	int ws_width = w / 4;
	int clock_width = w / 6;
	if (ws_width < 60) ws_width = 60;
	if (clock_width < 50) clock_width = 50;
	int title_width = w - ws_width - clock_width;
	if (title_width < 0) title_width = 0;

	struct wlr_buffer *title = render_title(toolbar, title_width,
		toolbar->height);
	wlr_scene_buffer_set_buffer(toolbar->title_buf, title);
	if (title) {
		wlr_buffer_drop(title);
	}
}

void
wm_toolbar_relayout(struct wm_toolbar *toolbar)
{
	if (!toolbar) {
		return;
	}

	struct wm_output *output = get_primary_output(toolbar->server);
	if (!output) {
		return;
	}

	int output_width = output->wlr_output->width;

	toolbar->width = output_width;
	toolbar->x = 0;
	/* Position at bottom of output */
	toolbar->y = output->wlr_output->height - toolbar->height;

	wlr_scene_node_set_position(&toolbar->scene_tree->node,
		toolbar->x, toolbar->y);

	/* Adjust usable area: reduce height from bottom */
	if (toolbar->visible) {
		output->usable_area.height -= toolbar->height;
	}

	toolbar_render(toolbar);
}

bool
wm_toolbar_handle_click(struct wm_toolbar *toolbar, double lx, double ly)
{
	if (!toolbar || !toolbar->visible) {
		return false;
	}

	/* Convert layout coords to toolbar-local coords */
	double local_x = lx - toolbar->x;
	double local_y = ly - toolbar->y;

	/* Check bounds */
	if (local_x < 0 || local_x >= toolbar->width ||
	    local_y < 0 || local_y >= toolbar->height) {
		return false;
	}

	/* Check workspace buttons */
	for (int i = 0; i < toolbar->ws_box_count; i++) {
		struct wlr_box *box = &toolbar->ws_boxes[i];
		if (local_x >= box->x && local_x < box->x + box->width &&
		    local_y >= box->y && local_y < box->y + box->height) {
			wm_workspace_switch(toolbar->server, i);
			return true;
		}
	}

	return false;
}
