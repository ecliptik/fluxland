/*
 * wm-wayland - A Fluxbox-inspired Wayland compositor
 *
 * toolbar.c - Built-in toolbar with workspace switcher, icon bar, and clock
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

/* --- Helper: compute layout widths --- */

static void
compute_layout(int total_width, int *ws_width, int *iconbar_width,
	int *clock_width)
{
	*ws_width = total_width / 4;
	*clock_width = total_width / 6;

	if (*ws_width < 60) *ws_width = 60;
	if (*clock_width < 50) *clock_width = 50;
	*iconbar_width = total_width - *ws_width - *clock_width;
	if (*iconbar_width < 0) *iconbar_width = 0;
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
	if (!toolbar->ws_boxes) {
		toolbar->ws_box_count = 0;
		cairo_destroy(cr);
		cairo_surface_destroy(surface);
		return NULL;
	}
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
		char fallback[16];
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

/* --- Collect icon bar entries based on the current mode --- */

static int
collect_iconbar_entries(struct wm_toolbar *toolbar)
{
	struct wm_server *server = toolbar->server;
	struct wm_workspace *current_ws = server->current_workspace;
	enum wm_iconbar_mode mode = toolbar->iconbar_mode;
	int count = 0;

	free(toolbar->ib_entries);
	toolbar->ib_entries = calloc(WM_TOOLBAR_ICONBAR_MAX,
		sizeof(struct wm_iconbar_entry));
	if (!toolbar->ib_entries) {
		toolbar->ib_count = 0;
		return 0;
	}

	struct wm_view *view;
	wl_list_for_each(view, &server->views, link) {
		if (count >= WM_TOOLBAR_ICONBAR_MAX) {
			break;
		}

		/* Must have a valid xdg surface to be a real window */
		if (!view->xdg_toplevel || !view->xdg_toplevel->base) {
			continue;
		}

		bool is_iconified = !view->scene_tree->node.enabled;
		bool on_current_ws = (view->workspace == current_ws ||
			view->sticky);

		/* Filter based on icon bar mode */
		switch (mode) {
		case WM_ICONBAR_MODE_WORKSPACE:
			/* Show views on current workspace (default) */
			if (!on_current_ws) {
				continue;
			}
			break;
		case WM_ICONBAR_MODE_ALL_WINDOWS:
			/* Show all views from all workspaces */
			break;
		case WM_ICONBAR_MODE_ICONS:
			/* Show only iconified/minimized views (any ws) */
			if (!is_iconified) {
				continue;
			}
			break;
		case WM_ICONBAR_MODE_NO_ICONS:
			/* Non-iconified views on current workspace */
			if (!on_current_ws || is_iconified) {
				continue;
			}
			break;
		case WM_ICONBAR_MODE_WORKSPACE_ICONS:
			/* Iconified views on current workspace */
			if (!on_current_ws || !is_iconified) {
				continue;
			}
			break;
		}

		struct wm_iconbar_entry *entry = &toolbar->ib_entries[count];
		entry->view = view;
		entry->focused = (view == server->focused_view);
		entry->iconified = is_iconified;
		count++;
	}

	toolbar->ib_count = count;
	return count;
}

/* --- Render icon bar (window list) --- */

static struct wlr_buffer *
render_iconbar(struct wm_toolbar *toolbar, int width, int height)
{
	struct wm_server *server = toolbar->server;
	struct wm_style *style = server->style;

	collect_iconbar_entries(toolbar);

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

	int count = toolbar->ib_count;
	if (count == 0) {
		/* No windows — return empty surface */
		cairo_destroy(cr);
		/* Free old hit boxes */
		free(toolbar->ib_boxes);
		toolbar->ib_boxes = NULL;
		return wlr_buffer_from_cairo(surface);
	}

	int entry_width = width / count;
	if (entry_width < 1) entry_width = 1;

	/* Allocate hit boxes */
	free(toolbar->ib_boxes);
	toolbar->ib_boxes = calloc(count, sizeof(struct wlr_box));

	/* Get icon bar colors (fall back to toolbar defaults) */
	struct wm_color focused_bg_color;
	struct wm_color focused_text_color;
	struct wm_color unfocused_bg_color;
	struct wm_color unfocused_text_color;

	if (style->toolbar_iconbar_has_focused_color) {
		focused_bg_color = style->toolbar_iconbar_focused_color;
	} else {
		/* Default: brighter variant of toolbar color */
		focused_bg_color = (struct wm_color){
			.r = 80, .g = 80, .b = 100, .a = 255
		};
	}
	if (style->toolbar_iconbar_focused_text_color.a > 0) {
		focused_text_color = style->toolbar_iconbar_focused_text_color;
	} else {
		focused_text_color = style->toolbar_text_color;
	}

	if (style->toolbar_iconbar_has_unfocused_color) {
		unfocused_bg_color = style->toolbar_iconbar_unfocused_color;
	} else {
		/* Default: transparent (no fill) */
		unfocused_bg_color = (struct wm_color){
			.r = 0, .g = 0, .b = 0, .a = 0
		};
	}
	if (style->toolbar_iconbar_unfocused_text_color.a > 0) {
		unfocused_text_color =
			style->toolbar_iconbar_unfocused_text_color;
	} else {
		unfocused_text_color = style->toolbar_text_color;
	}

	for (int i = 0; i < count; i++) {
		struct wm_iconbar_entry *entry = &toolbar->ib_entries[i];
		int ex = i * entry_width;
		int ew = (i == count - 1) ? (width - ex) : entry_width;

		/* Background */
		struct wm_color *bg_color = entry->focused ?
			&focused_bg_color : &unfocused_bg_color;
		struct wm_color *text_color = entry->focused ?
			&focused_text_color : &unfocused_text_color;

		if (entry->focused) {
			/* Active window: render with toolbar texture +
			 * bright overlay */
			cairo_surface_t *bg =
				wm_render_texture(&style->toolbar_texture,
					ew, height, 1.0f);
			if (bg) {
				cairo_t *bcr = cairo_create(bg);
				cairo_set_source_rgba(bcr, 1, 1, 1, 0.2);
				cairo_paint(bcr);
				cairo_destroy(bcr);
				cairo_set_source_surface(cr, bg, ex, 0);
				cairo_paint(cr);
				cairo_surface_destroy(bg);
			} else {
				cairo_set_source_rgba(cr,
					bg_color->r / 255.0,
					bg_color->g / 255.0,
					bg_color->b / 255.0,
					bg_color->a / 255.0);
				cairo_rectangle(cr, ex, 0, ew, height);
				cairo_fill(cr);
			}
		} else if (bg_color->a > 0) {
			cairo_set_source_rgba(cr,
				bg_color->r / 255.0,
				bg_color->g / 255.0,
				bg_color->b / 255.0,
				bg_color->a / 255.0);
			cairo_rectangle(cr, ex, 0, ew, height);
			cairo_fill(cr);
		}

		/* Separator line */
		if (i > 0) {
			cairo_set_source_rgba(cr,
				style->toolbar_text_color.r / 255.0,
				style->toolbar_text_color.g / 255.0,
				style->toolbar_text_color.b / 255.0, 0.3);
			cairo_set_line_width(cr, 1.0);
			cairo_move_to(cr, ex + 0.5, 2);
			cairo_line_to(cr, ex + 0.5, height - 2);
			cairo_stroke(cr);
		}

		/* Build display title */
		const char *raw_title = entry->view->title;
		if (!raw_title || !*raw_title) {
			raw_title = "(untitled)";
		}

		char title_buf[256];
		if (entry->iconified) {
			/* Wrap iconified window titles in parentheses */
			snprintf(title_buf, sizeof(title_buf),
				"(%s)", raw_title);
		} else {
			snprintf(title_buf, sizeof(title_buf),
				"%s", raw_title);
		}

		/* Render text */
		int tw, th;
		cairo_surface_t *text = wm_render_text(title_buf,
			&style->toolbar_font, text_color,
			ew - 6, &tw, &th, WM_JUSTIFY_CENTER, 1.0f);
		if (text) {
			int tx = ex + (ew - tw) / 2;
			int ty = (height - th) / 2;
			if (tx < ex + 2) tx = ex + 2;
			if (ty < 0) ty = 0;
			cairo_set_source_surface(cr, text, tx, ty);
			cairo_paint(cr);
			cairo_surface_destroy(text);
		}

		/* Store hit box (in iconbar-local coords) */
		if (toolbar->ib_boxes) {
			toolbar->ib_boxes[i].x = ex;
			toolbar->ib_boxes[i].y = 0;
			toolbar->ib_boxes[i].width = ew;
			toolbar->ib_boxes[i].height = height;
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

	/* Layout: workspace buttons (left), icon bar (center), clock (right) */
	int ws_width, iconbar_width, clock_width;
	compute_layout(w, &ws_width, &iconbar_width, &clock_width);

	toolbar->ib_x_offset = ws_width;

	/* Workspace buttons */
	struct wlr_buffer *ws_buf = render_workspace_buttons(toolbar,
		ws_width, h);
	wlr_scene_buffer_set_buffer(toolbar->workspace_buf, ws_buf);
	if (ws_buf) {
		wlr_buffer_drop(ws_buf);
	}
	wlr_scene_node_set_position(&toolbar->workspace_buf->node, 0, 0);

	/* Icon bar */
	struct wlr_buffer *ib_buf = render_iconbar(toolbar, iconbar_width, h);
	wlr_scene_buffer_set_buffer(toolbar->iconbar_buf, ib_buf);
	if (ib_buf) {
		wlr_buffer_drop(ib_buf);
	}
	wlr_scene_node_set_position(&toolbar->iconbar_buf->node,
		ws_width, 0);

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

/* --- Auto-hide timer callback --- */

static int
hide_timer_cb(void *data)
{
	struct wm_toolbar *toolbar = data;

	if (!toolbar->auto_hide || !toolbar->shown) {
		return 0;
	}

	/* Check if pointer is still outside the toolbar area */
	struct wm_server *server = toolbar->server;
	double cx = server->cursor->x;
	double cy = server->cursor->y;

	double local_x = cx - toolbar->x;
	double local_y = cy - toolbar->y;

	if (local_x >= 0 && local_x < toolbar->width &&
	    local_y >= 0 && local_y < toolbar->height) {
		/* Pointer is still inside — don't hide, re-arm */
		wl_event_source_timer_update(toolbar->hide_timer, 500);
		return 0;
	}

	/* Hide toolbar */
	toolbar->shown = false;
	wlr_scene_node_set_enabled(&toolbar->scene_tree->node, false);

	/* Remove exclusive zone */
	struct wm_output *output = get_primary_output(server);
	if (output) {
		wlr_output_effective_resolution(output->wlr_output,
			&output->usable_area.width,
			&output->usable_area.height);
		output->usable_area.x = 0;
		output->usable_area.y = 0;
	}

	return 0;
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

	/* Read placement config */
	if (server->config) {
		enum wm_toolbar_placement p = server->config->toolbar_placement;
		toolbar->on_top = (p == WM_TOOLBAR_TOP_LEFT ||
			p == WM_TOOLBAR_TOP_CENTER ||
			p == WM_TOOLBAR_TOP_RIGHT);
		toolbar->auto_hide = server->config->toolbar_auto_hide;
	}

	/* Auto-hide: start hidden */
	toolbar->shown = !toolbar->auto_hide;

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
	toolbar->iconbar_buf = wlr_scene_buffer_create(
		toolbar->scene_tree, NULL);
	toolbar->clock_buf = wlr_scene_buffer_create(
		toolbar->scene_tree, NULL);

	/* Set visibility: hide if auto-hide is enabled */
	bool initially_visible = toolbar->visible && toolbar->shown;
	wlr_scene_node_set_enabled(&toolbar->scene_tree->node,
		initially_visible);

	/* Position and render */
	wm_toolbar_relayout(toolbar);

	/* Start clock timer (1-second interval) */
	toolbar->clock_timer = wl_event_loop_add_timer(
		server->wl_event_loop, clock_timer_cb, toolbar);
	if (toolbar->clock_timer) {
		wl_event_source_timer_update(toolbar->clock_timer, 1000);
	}

	/* Create auto-hide timer */
	if (toolbar->auto_hide) {
		toolbar->hide_timer = wl_event_loop_add_timer(
			server->wl_event_loop, hide_timer_cb, toolbar);
	}

	wlr_log(WLR_INFO, "toolbar created (visible=%d, auto_hide=%d, on_top=%d)",
		toolbar->visible, toolbar->auto_hide, toolbar->on_top);
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

	if (toolbar->hide_timer) {
		wl_event_source_remove(toolbar->hide_timer);
	}

	if (toolbar->scene_tree) {
		wlr_scene_node_destroy(&toolbar->scene_tree->node);
	}

	free(toolbar->ws_boxes);
	free(toolbar->ib_boxes);
	free(toolbar->ib_entries);
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

	/* Workspace change also affects the icon bar window list */
	wm_toolbar_update_iconbar(toolbar);
}

void
wm_toolbar_update_title(struct wm_toolbar *toolbar)
{
	/* Title changes affect the icon bar */
	wm_toolbar_update_iconbar(toolbar);
}

void
wm_toolbar_update_iconbar(struct wm_toolbar *toolbar)
{
	if (!toolbar || !toolbar->visible) {
		return;
	}

	int w = toolbar->width;
	int ws_width, iconbar_width, clock_width;
	compute_layout(w, &ws_width, &iconbar_width, &clock_width);

	toolbar->ib_x_offset = ws_width;

	struct wlr_buffer *ib_buf = render_iconbar(toolbar, iconbar_width,
		toolbar->height);
	wlr_scene_buffer_set_buffer(toolbar->iconbar_buf, ib_buf);
	if (ib_buf) {
		wlr_buffer_drop(ib_buf);
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

	struct wm_config *config = toolbar->server->config;
	int output_width = output->wlr_output->width;
	int output_height = output->wlr_output->height;

	/* Apply width percentage */
	int width_pct = config ? config->toolbar_width_percent : 100;
	toolbar->width = output_width * width_pct / 100;
	if (toolbar->width < 1) toolbar->width = 1;

	/* Determine horizontal position based on placement */
	enum wm_toolbar_placement placement = config ?
		config->toolbar_placement : WM_TOOLBAR_BOTTOM_CENTER;

	switch (placement) {
	case WM_TOOLBAR_TOP_LEFT:
	case WM_TOOLBAR_BOTTOM_LEFT:
		toolbar->x = 0;
		break;
	case WM_TOOLBAR_TOP_RIGHT:
	case WM_TOOLBAR_BOTTOM_RIGHT:
		toolbar->x = output_width - toolbar->width;
		break;
	case WM_TOOLBAR_TOP_CENTER:
	case WM_TOOLBAR_BOTTOM_CENTER:
	default:
		toolbar->x = (output_width - toolbar->width) / 2;
		break;
	}

	/* Determine vertical position */
	toolbar->on_top = (placement == WM_TOOLBAR_TOP_LEFT ||
		placement == WM_TOOLBAR_TOP_CENTER ||
		placement == WM_TOOLBAR_TOP_RIGHT);

	if (toolbar->on_top) {
		toolbar->y = 0;
	} else {
		toolbar->y = output_height - toolbar->height;
	}

	wlr_scene_node_set_position(&toolbar->scene_tree->node,
		toolbar->x, toolbar->y);

	/* Adjust usable area (only when visible and not in auto-hide mode,
	 * or when auto-hide is active and toolbar is shown) */
	bool takes_space = toolbar->visible &&
		(!toolbar->auto_hide || toolbar->shown);
	if (takes_space) {
		if (toolbar->on_top) {
			output->usable_area.y += toolbar->height;
			output->usable_area.height -= toolbar->height;
		} else {
			output->usable_area.height -= toolbar->height;
		}
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

	/* Check icon bar entries */
	double ib_local_x = local_x - toolbar->ib_x_offset;
	for (int i = 0; i < toolbar->ib_count; i++) {
		if (!toolbar->ib_boxes) {
			break;
		}
		struct wlr_box *box = &toolbar->ib_boxes[i];
		if (ib_local_x >= box->x &&
		    ib_local_x < box->x + box->width &&
		    local_y >= box->y &&
		    local_y < box->y + box->height) {
			struct wm_view *view = toolbar->ib_entries[i].view;
			if (toolbar->ib_entries[i].iconified) {
				/* De-iconify: re-enable scene node */
				wlr_scene_node_set_enabled(
					&view->scene_tree->node, true);
			}
			wm_focus_view(view,
				view->xdg_toplevel->base->surface);
			wm_view_raise(view);
			wm_toolbar_update_iconbar(toolbar);
			return true;
		}
	}

	return false;
}

void
wm_toolbar_notify_pointer_motion(struct wm_toolbar *toolbar,
	double lx, double ly)
{
	if (!toolbar || !toolbar->visible || !toolbar->auto_hide) {
		return;
	}

	struct wm_output *output = get_primary_output(toolbar->server);
	if (!output) {
		return;
	}

	int output_height = output->wlr_output->height;
	int output_width = output->wlr_output->width;

	/* Define trigger zone: 1px strip at the edge where toolbar lives */
	bool in_trigger_zone = false;
	if (toolbar->on_top) {
		in_trigger_zone = (ly <= 1 && lx >= 0 && lx < output_width);
	} else {
		in_trigger_zone = (ly >= output_height - 1 &&
			lx >= 0 && lx < output_width);
	}

	/* Check if pointer is within the toolbar area (when shown) */
	bool in_toolbar = false;
	if (toolbar->shown) {
		double local_x = lx - toolbar->x;
		double local_y = ly - toolbar->y;
		in_toolbar = (local_x >= 0 && local_x < toolbar->width &&
			local_y >= 0 && local_y < toolbar->height);
	}

	if (!toolbar->shown && in_trigger_zone) {
		/* Show toolbar */
		toolbar->shown = true;
		wlr_scene_node_set_enabled(&toolbar->scene_tree->node, true);

		/* Recalculate usable area to include exclusive zone */
		wlr_output_effective_resolution(output->wlr_output,
			&output->usable_area.width,
			&output->usable_area.height);
		output->usable_area.x = 0;
		output->usable_area.y = 0;
		if (toolbar->on_top) {
			output->usable_area.y += toolbar->height;
			output->usable_area.height -= toolbar->height;
		} else {
			output->usable_area.height -= toolbar->height;
		}

		toolbar_render(toolbar);

		/* Cancel any pending hide */
		if (toolbar->hide_timer) {
			wl_event_source_timer_update(toolbar->hide_timer, 0);
		}
	} else if (toolbar->shown && !in_toolbar && !in_trigger_zone) {
		/* Pointer left toolbar area — start hide timer */
		int delay = 500;
		if (toolbar->server->config) {
			delay = toolbar->server->config->toolbar_auto_hide_delay_ms;
		}
		if (toolbar->hide_timer) {
			wl_event_source_timer_update(toolbar->hide_timer,
				delay);
		}
	} else if (toolbar->shown && (in_toolbar || in_trigger_zone)) {
		/* Pointer is inside toolbar — cancel any pending hide */
		if (toolbar->hide_timer) {
			wl_event_source_timer_update(toolbar->hide_timer, 0);
		}
	}
}
