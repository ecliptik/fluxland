/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * toolbar.c - Built-in toolbar with configurable tool layout
 */

#define _POSIX_C_SOURCE 200809L
#include <cairo.h>
#include <drm_fourcc.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/log.h>

#include "toolbar.h"
#include "config.h"
#include "i18n.h"
#include "focus_nav.h"
#include "output.h"
#include "render.h"
#include "server.h"
#include "style.h"
#ifdef WM_HAS_SYSTRAY
#include "systray.h"
#endif
#include "view.h"
#include "view_focus.h"
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

	if (width <= 0 || height <= 0 || stride <= 0 || !src) {
		cairo_surface_destroy(surface);
		return NULL;
	}

	if ((size_t)stride > SIZE_MAX / (size_t)height) {
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

/* --- Tool parsing --- */

static bool
parse_tool_name(const char *name, enum wm_toolbar_tool_type *out)
{
	if (strcmp(name, "prevworkspace") == 0) {
		*out = WM_TOOL_PREV_WORKSPACE;
		return true;
	}
	if (strcmp(name, "nextworkspace") == 0) {
		*out = WM_TOOL_NEXT_WORKSPACE;
		return true;
	}
	if (strcmp(name, "workspacename") == 0) {
		*out = WM_TOOL_WORKSPACE_NAME;
		return true;
	}
	if (strcmp(name, "iconbar") == 0) {
		*out = WM_TOOL_ICONBAR;
		return true;
	}
	if (strcmp(name, "clock") == 0) {
		*out = WM_TOOL_CLOCK;
		return true;
	}
	if (strcmp(name, "prevwindow") == 0) {
		*out = WM_TOOL_PREV_WINDOW;
		return true;
	}
	if (strcmp(name, "nextwindow") == 0) {
		*out = WM_TOOL_NEXT_WINDOW;
		return true;
	}
	return false;
}

static void
parse_toolbar_tools(struct wm_toolbar *toolbar, const char *tools_str)
{
	if (!tools_str || !*tools_str) {
		toolbar->tools = NULL;
		toolbar->tool_count = 0;
		return;
	}

	char *copy = strdup(tools_str);
	if (!copy) {
		return;
	}

	/* First pass: count tokens */
	int count = 0;
	enum wm_toolbar_tool_type types[WM_TOOLBAR_MAX_TOOLS];
	char *saveptr = NULL;
	char *tok = strtok_r(copy, " \t", &saveptr);
	while (tok && count < WM_TOOLBAR_MAX_TOOLS) {
		enum wm_toolbar_tool_type type;
		if (parse_tool_name(tok, &type)) {
			types[count++] = type;
		} else {
			wlr_log(WLR_INFO,
				"toolbar: unknown tool '%s', skipping", tok);
		}
		tok = strtok_r(NULL, " \t", &saveptr);
	}
	free(copy);

	if (count == 0) {
		toolbar->tools = NULL;
		toolbar->tool_count = 0;
		return;
	}

	toolbar->tools = calloc(count, sizeof(struct wm_toolbar_tool));
	if (!toolbar->tools) {
		toolbar->tool_count = 0;
		return;
	}
	toolbar->tool_count = count;

	/* Initialize each tool with a scene buffer */
	for (int i = 0; i < count; i++) {
		struct wm_toolbar_tool *tool = &toolbar->tools[i];
		tool->type = types[i];
		tool->buf = wlr_scene_buffer_create(toolbar->scene_tree, NULL);

		/* Set quick-access pointers */
		switch (tool->type) {
		case WM_TOOL_ICONBAR:
			toolbar->iconbar_tool = tool;
			break;
		case WM_TOOL_CLOCK:
			toolbar->clock_tool = tool;
			break;
		case WM_TOOL_WORKSPACE_NAME:
			toolbar->ws_name_tool = tool;
			break;
		default:
			break;
		}
	}
}

/* --- Layout computation --- */

static bool
tool_is_button(enum wm_toolbar_tool_type type)
{
	return type == WM_TOOL_PREV_WORKSPACE ||
		type == WM_TOOL_NEXT_WORKSPACE ||
		type == WM_TOOL_PREV_WINDOW ||
		type == WM_TOOL_NEXT_WINDOW;
}

static void
compute_tool_layout(struct wm_toolbar *toolbar, int total_width)
{
	struct wm_style *style = toolbar->server->style;
	int h = toolbar->height;
	int count = toolbar->tool_count;
	if (count == 0) {
		return;
	}

	/* Pass 1: determine fixed widths */
	int fixed_total = 0;
	int iconbar_idx = -1;
	int flex_count = 0;

	for (int i = 0; i < count; i++) {
		struct wm_toolbar_tool *tool = &toolbar->tools[i];
		if (tool_is_button(tool->type)) {
			/* Square button: width = height */
			tool->width = h;
			fixed_total += tool->width;
		} else if (tool->type == WM_TOOL_WORKSPACE_NAME) {
			/* Use cached max width to avoid N Pango measurements
			 * per toolbar layout. Invalidated on reconfigure. */
			int max_tw;
			if (toolbar->cached_ws_name_max_width >= 0) {
				max_tw = toolbar->cached_ws_name_max_width;
			} else {
				max_tw = 0;
				struct wm_workspace *ws_iter;
				wl_list_for_each(ws_iter,
					&toolbar->server->workspaces, link) {
					const char *ws_name = ws_iter->name;
					if (!ws_name || !*ws_name)
						ws_name = "1";
					int tw = wm_measure_text_width(ws_name,
						&style->toolbar_font, 1.0f);
					if (tw > max_tw) max_tw = tw;
				}
				toolbar->cached_ws_name_max_width = max_tw;
			}
			tool->width = max_tw + WM_TOOLBAR_PADDING * 4;
			if (tool->width < 60) tool->width = 60;
			fixed_total += tool->width;
		} else if (tool->type == WM_TOOL_CLOCK) {
			/* Measure clock text */
			const char *fmt = WM_TOOLBAR_CLOCK_FMT;
			if (toolbar->server->config &&
			    toolbar->server->config->clock_format) {
				fmt = toolbar->server->config->clock_format;
			}
			time_t now = time(NULL);
			struct tm tm;
			localtime_r(&now, &tm);
			char timebuf[64];
			strftime(timebuf, sizeof(timebuf), fmt, &tm);
			int tw = wm_measure_text_width(timebuf,
				&style->toolbar_font, 1.0f);
			tool->width = tw + WM_TOOLBAR_PADDING * 4;
			if (tool->width < 80) tool->width = 80;
			fixed_total += tool->width;
		} else if (tool->type == WM_TOOL_ICONBAR) {
			iconbar_idx = i;
			flex_count++;
		}
	}

	/* Reserve space for systray if present */
	int systray_width = 0;
#ifdef WM_HAS_SYSTRAY
	if (toolbar->server->systray) {
		systray_width = wm_systray_get_width(
			toolbar->server->systray);
	}
#endif
	fixed_total += systray_width;

	/* Pass 2: assign flex width to iconbar */
	int remaining = total_width - fixed_total;
	if (remaining < 0) remaining = 0;

	if (iconbar_idx >= 0) {
		toolbar->tools[iconbar_idx].width = remaining;
	} else if (remaining > 0) {
		/* No iconbar: distribute remaining space among text tools */
		int text_count = 0;
		for (int i = 0; i < count; i++) {
			if (toolbar->tools[i].type == WM_TOOL_WORKSPACE_NAME ||
			    toolbar->tools[i].type == WM_TOOL_CLOCK) {
				text_count++;
			}
		}
		if (text_count > 0) {
			int extra = remaining / text_count;
			for (int i = 0; i < count; i++) {
				if (toolbar->tools[i].type ==
				    WM_TOOL_WORKSPACE_NAME ||
				    toolbar->tools[i].type ==
				    WM_TOOL_CLOCK) {
					toolbar->tools[i].width += extra;
				}
			}
		}
	}

	/* Pass 3: assign x positions left-to-right */
	int x = 0;
	for (int i = 0; i < count; i++) {
		toolbar->tools[i].x = x;
		x += toolbar->tools[i].width;
	}
}

/* --- Per-tool render functions --- */

static struct wlr_buffer *
render_button_tool(struct wm_toolbar *toolbar, const char *label,
	int width, int height)
{
	struct wm_style *style = toolbar->server->style;

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

	/* Slightly darker background for buttons */
	cairo_set_source_rgba(cr, 0, 0, 0, 0.15);
	cairo_rectangle(cr, 0, 0, width, height);
	cairo_fill(cr);

	/* Draw separator on left edge */
	cairo_set_source_rgba(cr,
		style->toolbar_text_color.r / 255.0,
		style->toolbar_text_color.g / 255.0,
		style->toolbar_text_color.b / 255.0, 0.3);
	cairo_set_line_width(cr, 1.0);
	cairo_move_to(cr, 0.5, 2);
	cairo_line_to(cr, 0.5, height - 2);
	cairo_stroke(cr);

	/* Draw label centered */
	int tw, th;
	cairo_surface_t *text = wm_render_text(label,
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

static struct wlr_buffer *
render_workspace_name_tool(struct wm_toolbar *toolbar,
	struct wm_toolbar_tool *tool, int width, int height)
{
	struct wm_server *server = toolbar->server;
	struct wm_style *style = server->style;
	struct wm_workspace *active_ws = wm_workspace_get_active(server);

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

	/* Single hit box covering the whole tool — allocate once, reuse */
	if (!tool->hit_boxes) {
		tool->hit_boxes = calloc(1, sizeof(struct wlr_box));
		if (!tool->hit_boxes) {
			tool->hit_box_count = 0;
			cairo_destroy(cr);
			cairo_surface_destroy(surface);
			return NULL;
		}
	}
	tool->hit_box_count = 1;
	tool->hit_boxes[0].x = 0;
	tool->hit_boxes[0].y = 0;
	tool->hit_boxes[0].width = width;
	tool->hit_boxes[0].height = height;

	/* Render background with toolbar texture */
	cairo_surface_t *bg =
		wm_render_texture(&style->toolbar_texture,
			width, height, 1.0f);
	if (bg) {
		cairo_t *bcr = cairo_create(bg);
		cairo_set_source_rgba(bcr, 1, 1, 1, 0.15);
		cairo_paint(bcr);
		cairo_destroy(bcr);
		cairo_set_source_surface(cr, bg, 0, 0);
		cairo_paint(cr);
		cairo_surface_destroy(bg);
	} else {
		cairo_set_source_rgba(cr, 0.4, 0.4, 0.5, 1.0);
		cairo_rectangle(cr, 0, 0, width, height);
		cairo_fill(cr);
	}

	/* Draw active workspace name centered */
	const char *name = active_ws ? active_ws->name : NULL;
	char fallback[16];
	if (!name || !*name) {
		snprintf(fallback, sizeof(fallback), "%d",
			active_ws ? active_ws->index + 1 : 1);
		name = fallback;
	}

	int tw, th;
	cairo_surface_t *text = wm_render_text(name,
		&style->toolbar_font, &style->toolbar_text_color,
		width - 4, &tw, &th, WM_JUSTIFY_CENTER, 1.0f);
	if (text) {
		int tx = (width - tw) / 2;
		int ty = (height - th) / 2;
		if (ty < 0) ty = 0;
		cairo_set_source_surface(cr, text, tx, ty);
		cairo_paint(cr);
		cairo_surface_destroy(text);
	}

	cairo_destroy(cr);
	return wlr_buffer_from_cairo(surface);
}

/* --- Collect icon bar entries based on the current mode --- */

static int
collect_iconbar_entries(struct wm_toolbar *toolbar)
{
	struct wm_server *server = toolbar->server;
	struct wm_workspace *current_ws = wm_workspace_get_active(server);
	enum wm_iconbar_mode mode = toolbar->iconbar_mode;
	int count = 0;

	/* Reuse pre-allocated array instead of free+calloc per update.
	 * Fallback to allocation if not pre-allocated (e.g. in tests). */
	if (!toolbar->ib_entries) {
		toolbar->ib_entries = calloc(WM_TOOLBAR_ICONBAR_MAX,
			sizeof(struct wm_iconbar_entry));
		if (!toolbar->ib_entries) {
			toolbar->ib_count = 0;
			return 0;
		}
	} else {
		memset(toolbar->ib_entries, 0, WM_TOOLBAR_ICONBAR_MAX *
			sizeof(struct wm_iconbar_entry));
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
			if (!on_current_ws) {
				continue;
			}
			break;
		case WM_ICONBAR_MODE_ALL_WINDOWS:
			break;
		case WM_ICONBAR_MODE_ICONS:
			if (!is_iconified) {
				continue;
			}
			break;
		case WM_ICONBAR_MODE_NO_ICONS:
			if (!on_current_ws || is_iconified) {
				continue;
			}
			break;
		case WM_ICONBAR_MODE_WORKSPACE_ICONS:
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
		return wlr_buffer_from_cairo(surface);
	}

	/* Determine entry width: fixed iconWidth or auto (divide equally) */
	struct wm_config *cfg = server->config;
	int fixed_icon_width = cfg ? cfg->iconbar_icon_width : 0;
	int entry_width;
	if (fixed_icon_width > 0) {
		entry_width = fixed_icon_width;
	} else {
		entry_width = width / count;
	}
	if (entry_width < 1) entry_width = 1;

	/* Compute total content width and alignment offset */
	int total_content_width = entry_width * count;
	if (total_content_width > width)
		total_content_width = width;
	int align_offset = 0;
	int alignment = cfg ? cfg->iconbar_alignment : 1;
	if (total_content_width < width) {
		if (alignment == 2) /* right */
			align_offset = width - total_content_width;
		else if (alignment == 1) /* center */
			align_offset = (width - total_content_width) / 2;
		/* left: offset stays 0 */
	}

	/* Reuse pre-allocated hit boxes array.
	 * Fallback to allocation if not pre-allocated (e.g. in tests). */
	if (!toolbar->ib_boxes) {
		toolbar->ib_boxes = calloc(WM_TOOLBAR_ICONBAR_MAX,
			sizeof(struct wlr_box));
		if (!toolbar->ib_boxes) {
			cairo_destroy(cr);
			cairo_surface_destroy(surface);
			return NULL;
		}
	} else {
		memset(toolbar->ib_boxes, 0, count * sizeof(struct wlr_box));
	}

	/* Get icon bar colors (fall back to toolbar defaults) */
	struct wm_color focused_bg_color;
	struct wm_color focused_text_color;
	struct wm_color unfocused_bg_color;
	struct wm_color unfocused_text_color;

	if (style->toolbar_iconbar_has_focused_color) {
		focused_bg_color = style->toolbar_iconbar_focused_color;
	} else {
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
		int ex = align_offset + i * entry_width;
		int ew;
		if (fixed_icon_width > 0) {
			ew = entry_width;
			if (ex + ew > width)
				ew = width - ex;
		} else {
			ew = (i == count - 1) ?
				(width - (ex - align_offset) - align_offset) :
				entry_width;
		}

		/* Background */
		struct wm_color *bg_color = entry->focused ?
			&focused_bg_color : &unfocused_bg_color;
		struct wm_color *text_color = entry->focused ?
			&focused_text_color : &unfocused_text_color;

		if (entry->focused) {
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
			raw_title = _("(untitled)");
		}

		char title_buf[256];
		if (entry->iconified) {
			const char *pattern = cfg ?
				cfg->iconbar_iconified_pattern : NULL;
			if (!pattern)
				pattern = "(%s)";
			/* Use %s with the pattern as data to avoid
			 * format string injection from config */
			size_t plen = strlen(pattern);
			const char *pct_s = strstr(pattern, "%s");
			if (pct_s) {
				size_t prefix = (size_t)(pct_s - pattern);
				size_t suffix = plen - prefix - 2;
				snprintf(title_buf, sizeof(title_buf),
					"%.*s%s%.*s",
					(int)prefix, pattern,
					raw_title,
					(int)suffix, pct_s + 2);
			} else {
				snprintf(title_buf, sizeof(title_buf),
					"%s", pattern);
			}
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

	const char *fmt = WM_TOOLBAR_CLOCK_FMT;
	if (toolbar->server->config && toolbar->server->config->clock_format) {
		fmt = toolbar->server->config->clock_format;
	}

	char timebuf[64];
	strftime(timebuf, sizeof(timebuf), fmt, &tm);

	/* Cache to avoid redraw if time string unchanged */
	if (strcmp(timebuf, toolbar->cached_clock) == 0) {
		return NULL; /* no change */
	}
	strncpy(toolbar->cached_clock, timebuf, sizeof(toolbar->cached_clock) - 1);
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

/* --- Render a single tool --- */

static void
render_tool(struct wm_toolbar *toolbar, struct wm_toolbar_tool *tool)
{
	int w = tool->width;
	int h = toolbar->height;
	struct wlr_buffer *buf = NULL;

	if (w <= 0 || h <= 0) {
		return;
	}

	switch (tool->type) {
	case WM_TOOL_PREV_WORKSPACE:
		buf = render_button_tool(toolbar, "<", w, h);
		break;
	case WM_TOOL_NEXT_WORKSPACE:
		buf = render_button_tool(toolbar, ">", w, h);
		break;
	case WM_TOOL_PREV_WINDOW:
		buf = render_button_tool(toolbar, "\xe2\x97\x80", w, h);
		break;
	case WM_TOOL_NEXT_WINDOW:
		buf = render_button_tool(toolbar, "\xe2\x96\xb6", w, h);
		break;
	case WM_TOOL_WORKSPACE_NAME:
		buf = render_workspace_name_tool(toolbar, tool, w, h);
		break;
	case WM_TOOL_ICONBAR:
		buf = render_iconbar(toolbar, w, h);
		break;
	case WM_TOOL_CLOCK:
		toolbar->cached_clock[0] = '\0'; /* force redraw */
		buf = render_clock(toolbar, w, h);
		break;
	}

	if (buf) {
		wlr_scene_buffer_set_buffer(tool->buf, buf);
		wlr_buffer_drop(buf);
	}
	wlr_scene_node_set_position(&tool->buf->node, tool->x, 0);
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

	/* Compute layout for all tools */
	compute_tool_layout(toolbar, w);

	/* Render each tool */
	for (int i = 0; i < toolbar->tool_count; i++) {
		render_tool(toolbar, &toolbar->tools[i]);
	}

	/* Draw keyboard focus indicator if toolbar has focus */
	int focus_idx = wm_focus_nav_get_toolbar_index(server);
	if (focus_idx >= 0 && focus_idx < toolbar->tool_count) {
		struct wm_toolbar_tool *focused_tool =
			&toolbar->tools[focus_idx];
		int fx = focused_tool->x;
		int fw = focused_tool->width;
		int fh = h;
		if (fw > 4 && fh > 4) {
			cairo_surface_t *focus_surf =
				cairo_image_surface_create(
					CAIRO_FORMAT_ARGB32, fw, fh);
			cairo_t *cr = cairo_create(focus_surf);
			/* High-contrast yellow border by default,
			 * use focused iconbar color if available */
			double fc_r = 1.0, fc_g = 1.0, fc_b = 0.0;
			if (style->toolbar_iconbar_has_focused_color) {
				fc_r = style->toolbar_iconbar_focused_color.r
					/ 255.0;
				fc_g = style->toolbar_iconbar_focused_color.g
					/ 255.0;
				fc_b = style->toolbar_iconbar_focused_color.b
					/ 255.0;
			}
			cairo_set_source_rgba(cr, fc_r, fc_g, fc_b, 1.0);
			cairo_set_line_width(cr, 2.0);
			cairo_rectangle(cr, 1, 1, fw - 2, fh - 2);
			cairo_stroke(cr);
			cairo_destroy(cr);

			struct wlr_buffer *focus_buf =
				wlr_buffer_from_cairo(focus_surf);
			if (focus_buf) {
				if (!toolbar->focus_indicator) {
					toolbar->focus_indicator =
						wlr_scene_buffer_create(
							toolbar->scene_tree,
							focus_buf);
				} else {
					wlr_scene_buffer_set_buffer(
						toolbar->focus_indicator,
						focus_buf);
				}
				if (toolbar->focus_indicator) {
					wlr_scene_node_set_position(
						&toolbar->focus_indicator->node,
						fx, 0);
					wlr_scene_node_set_enabled(
						&toolbar->focus_indicator->node,
						true);
					wlr_scene_node_raise_to_top(
						&toolbar->focus_indicator->node);
				}
				wlr_buffer_drop(focus_buf);
			}
		}
	} else if (toolbar->focus_indicator) {
		/* No focus — hide the indicator */
		wlr_scene_node_set_enabled(
			&toolbar->focus_indicator->node, false);
	}

#ifdef WM_HAS_SYSTRAY
	/* Position systray after the last tool */
	if (server->systray) {
		int systray_x = 0;
		if (toolbar->tool_count > 0) {
			struct wm_toolbar_tool *last =
				&toolbar->tools[toolbar->tool_count - 1];
			systray_x = last->x + last->width;
		}
		wm_systray_layout(server->systray,
			toolbar->x + systray_x, toolbar->y,
			w - systray_x, h);
	}
#endif
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
		/* Don't re-arm when hidden — saves 86,400 timer
		 * fires/day. Timer restarts on visibility change. */
		return 0;
	}

	if (toolbar->clock_tool) {
		struct wlr_buffer *clock = render_clock(toolbar,
			toolbar->clock_tool->width, toolbar->height);
		if (clock) {
			wlr_scene_buffer_set_buffer(
				toolbar->clock_tool->buf, clock);
			wlr_buffer_drop(clock);
		}
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
	toolbar->cached_ws_name_max_width = -1;

	/* Pre-allocate iconbar arrays to avoid malloc/free churn on
	 * every focus change, title change, and workspace switch. */
	toolbar->ib_entries = calloc(WM_TOOLBAR_ICONBAR_MAX,
		sizeof(struct wm_iconbar_entry));
	toolbar->ib_boxes = calloc(WM_TOOLBAR_ICONBAR_MAX,
		sizeof(struct wlr_box));

	/* Read placement config */
	if (server->config) {
		enum wm_toolbar_placement p = server->config->toolbar_placement;
		toolbar->on_top = (p == WM_TOOLBAR_TOP_LEFT ||
			p == WM_TOOLBAR_TOP_CENTER ||
			p == WM_TOOLBAR_TOP_RIGHT);
		toolbar->auto_hide = server->config->toolbar_auto_hide;
		toolbar->iconbar_mode = server->config->iconbar_mode;
		if (server->config->toolbar_height > 0) {
			toolbar->height = server->config->toolbar_height;
		}
	}

	/* Auto-hide: start hidden */
	toolbar->shown = !toolbar->auto_hide;

	/* Create scene tree in layer_top so it renders above windows */
	toolbar->scene_tree = wlr_scene_tree_create(server->layer_top);
	if (!toolbar->scene_tree) {
		free(toolbar);
		return NULL;
	}

	/* Create background buffer */
	toolbar->bg_buf = wlr_scene_buffer_create(toolbar->scene_tree, NULL);
	if (!toolbar->bg_buf) {
		wlr_log(WLR_ERROR, "%s", "Failed to create toolbar background buffer");
		wlr_scene_node_destroy(&toolbar->scene_tree->node);
		free(toolbar);
		return NULL;
	}

	/* Parse toolbar tools from config */
	const char *tools_str = NULL;
	if (server->config && server->config->toolbar_tools) {
		tools_str = server->config->toolbar_tools;
	}
	if (!tools_str) {
		tools_str = "prevworkspace workspacename "
			"nextworkspace iconbar clock";
	}
	parse_toolbar_tools(toolbar, tools_str);

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

	wlr_log(WLR_INFO, "toolbar created (visible=%d, auto_hide=%d, "
		"on_top=%d, tools=%d)",
		toolbar->visible, toolbar->auto_hide, toolbar->on_top,
		toolbar->tool_count);
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

	/* Free tool hit boxes */
	for (int i = 0; i < toolbar->tool_count; i++) {
		free(toolbar->tools[i].hit_boxes);
	}
	free(toolbar->tools);

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

	/* Re-render workspace name tool if configured */
	if (toolbar->ws_name_tool) {
		compute_tool_layout(toolbar, toolbar->width);
		render_tool(toolbar, toolbar->ws_name_tool);
	}

	/* Workspace change also affects the icon bar window list */
	wm_toolbar_update_iconbar(toolbar);
}

void
wm_toolbar_update_iconbar(struct wm_toolbar *toolbar)
{
	if (!toolbar || !toolbar->visible) {
		return;
	}

	if (toolbar->iconbar_tool) {
		struct wlr_buffer *ib_buf = render_iconbar(toolbar,
			toolbar->iconbar_tool->width, toolbar->height);
		wlr_scene_buffer_set_buffer(toolbar->iconbar_tool->buf,
			ib_buf);
		if (ib_buf) {
			wlr_buffer_drop(ib_buf);
		}
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

	/* Invalidate cached measurements on relayout (style/config change) */
	toolbar->cached_ws_name_max_width = -1;

	/* Re-read configuration values so reconfigure picks up changes */
	if (config) {
		toolbar->visible = config->toolbar_visible;
		toolbar->iconbar_mode = config->iconbar_mode;
		if (config->toolbar_height > 0) {
			toolbar->height = config->toolbar_height;
		}

		/* Handle auto-hide state change */
		bool new_auto_hide = config->toolbar_auto_hide;
		if (new_auto_hide != toolbar->auto_hide) {
			toolbar->auto_hide = new_auto_hide;
			if (new_auto_hide) {
				/* Create hide timer if not already present */
				if (!toolbar->hide_timer) {
					toolbar->hide_timer =
						wl_event_loop_add_timer(
							toolbar->server->wl_event_loop,
							hide_timer_cb, toolbar);
				}
				toolbar->shown = false;
			} else {
				/* Cancel any pending hide and show toolbar */
				if (toolbar->hide_timer) {
					wl_event_source_timer_update(
						toolbar->hide_timer, 0);
				}
				toolbar->shown = true;
			}
		}
	}

	/* Update scene visibility */
	bool scene_visible = toolbar->visible &&
		(!toolbar->auto_hide || toolbar->shown);
	wlr_scene_node_set_enabled(&toolbar->scene_tree->node,
		scene_visible);

	int output_width, output_height;
	wlr_output_effective_resolution(output->wlr_output,
		&output_width, &output_height);

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
wm_toolbar_handle_scroll(struct wm_toolbar *toolbar,
	double lx, double ly, double delta)
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

	/* Find which tool was scrolled */
	for (int i = 0; i < toolbar->tool_count; i++) {
		struct wm_toolbar_tool *tool = &toolbar->tools[i];
		if (local_x < tool->x || local_x >= tool->x + tool->width) {
			continue;
		}

		switch (tool->type) {
		case WM_TOOL_ICONBAR: {
			/* Check wheel mode config */
			struct wm_config *cfg = toolbar->server->config;
			int wheel_mode = cfg ? cfg->iconbar_wheel_mode : 0;
			if (wheel_mode == 1) {
				/* Off: ignore wheel on iconbar */
				return true;
			}
			/* Screen mode: change workspace */
			if (delta > 0) {
				wm_workspace_switch_next(toolbar->server);
			} else if (delta < 0) {
				wm_workspace_switch_prev(toolbar->server);
			}
			return true;
		}

		case WM_TOOL_WORKSPACE_NAME:
		case WM_TOOL_PREV_WORKSPACE:
		case WM_TOOL_NEXT_WORKSPACE:
			/* Scroll on workspace-related tools: change ws */
			if (delta > 0) {
				wm_workspace_switch_next(toolbar->server);
			} else if (delta < 0) {
				wm_workspace_switch_prev(toolbar->server);
			}
			return true;

		default:
			/* Scroll on other tools: no action */
			return true;
		}
	}

	return false;
}

bool
wm_toolbar_handle_button(struct wm_toolbar *toolbar,
	double lx, double ly, uint32_t button)
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

	/* Only handle left-click (BTN_LEFT = 272) */
	if (button != 272) {
		return true; /* consume but ignore non-left clicks */
	}

	/* Find which tool was clicked */
	for (int i = 0; i < toolbar->tool_count; i++) {
		struct wm_toolbar_tool *tool = &toolbar->tools[i];
		if (local_x < tool->x || local_x >= tool->x + tool->width) {
			continue;
		}

		switch (tool->type) {
		case WM_TOOL_PREV_WORKSPACE:
			wm_workspace_switch_prev(toolbar->server);
			return true;

		case WM_TOOL_NEXT_WORKSPACE:
			wm_workspace_switch_next(toolbar->server);
			return true;

		case WM_TOOL_PREV_WINDOW:
			wm_focus_prev_view(toolbar->server);
			return true;

		case WM_TOOL_NEXT_WINDOW:
			wm_focus_next_view(toolbar->server);
			return true;

		case WM_TOOL_ICONBAR: {
			/* Find which iconbar entry was clicked */
			double ib_x = local_x - tool->x;
			for (int j = 0; j < toolbar->ib_count; j++) {
				struct wlr_box *box = &toolbar->ib_boxes[j];
				if (ib_x >= box->x &&
				    ib_x < box->x + box->width) {
					struct wm_view *view =
						toolbar->ib_entries[j].view;
					if (view) {
						if (!view->scene_tree->node.enabled) {
							deiconify_view(view);
						} else {
							wm_focus_view(view, NULL);
							wm_view_raise(view);
						}
					}
					return true;
				}
			}
			return true;
		}

		case WM_TOOL_WORKSPACE_NAME:
		case WM_TOOL_CLOCK:
			/* No click action */
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

	int output_width, output_height;
	wlr_output_effective_resolution(output->wlr_output,
		&output_width, &output_height);

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

		/* Restart clock timer (stopped while hidden) */
		if (toolbar->clock_timer) {
			wl_event_source_timer_update(
				toolbar->clock_timer, 1000);
		}

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

void
wm_toolbar_toggle_above(struct wm_toolbar *toolbar)
{
	if (!toolbar) {
		return;
	}

	toolbar->on_top = !toolbar->on_top;
	wm_toolbar_relayout(toolbar);

	wlr_log(WLR_INFO, "toolbar toggled above: on_top=%d", toolbar->on_top);
}

void
wm_toolbar_toggle_visible(struct wm_toolbar *toolbar)
{
	if (!toolbar) {
		return;
	}

	toolbar->visible = !toolbar->visible;

	/* Sync config so relayout doesn't revert the change */
	if (toolbar->server->config) {
		toolbar->server->config->toolbar_visible = toolbar->visible;
	}

	wlr_scene_node_set_enabled(&toolbar->scene_tree->node,
		toolbar->visible);
	wm_toolbar_relayout(toolbar);

	/* Restart clock timer when becoming visible (it stops when hidden) */
	if (toolbar->visible && toolbar->clock_timer) {
		wl_event_source_timer_update(toolbar->clock_timer, 1000);
	}

	wlr_log(WLR_INFO, "toolbar toggled visible: %d", toolbar->visible);
}
