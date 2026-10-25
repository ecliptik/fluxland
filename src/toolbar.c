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
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/log.h>

#include "toolbar.h"
#include "menu.h"
#include "command.h"
#include "util.h"
#include "config.h"
#include "i18n.h"
#include "focus_nav.h"
#include "output.h"
#include "pixel_buffer.h"
#include "render.h"
#include "server.h"
#include "style.h"
#ifdef WM_HAS_SYSTRAY
#include "systray.h"
#endif
#include "view.h"
#include "view_focus.h"
#include "workspace.h"
#include "client_pattern.h"
#include "tabgroup.h"

/* --- Helper: get primary output --- */

/* The toolbar's head (session.screen0.toolbar.onhead, from 1) */
static struct wm_output *
toolbar_output(struct wm_server *server)
{
	return wm_output_by_head(server,
		server->config ? server->config->toolbar_on_head : 0);
}

/* The scene layer for toolbar.layer (config.c's parse_dock_layer) */
static void
apply_toolbar_layer(struct wm_toolbar *toolbar)
{
	struct wm_server *server = toolbar->server;
	int layer = server->config ? server->config->toolbar_layer : 4;
	struct wlr_scene_tree *parent = server->layer_top;
	if (toolbar->raised || layer == 6)
		parent = server->layer_overlay;
	else if (layer == 2)
		parent = server->layer_bottom;
	if (toolbar->scene_tree && parent &&
	    toolbar->scene_tree->node.parent != parent)
		wlr_scene_node_reparent(&toolbar->scene_tree->node, parent);
}

/* --- Tool parsing --- */

static bool
parse_tool_name(const char *name, enum wm_toolbar_tool_type *out)
{
	static const struct {
		const char *name;
		enum wm_toolbar_tool_type type;
	} tools[] = {
		{ "prevworkspace", WM_TOOL_PREV_WORKSPACE },
		{ "nextworkspace", WM_TOOL_NEXT_WORKSPACE },
		{ "workspacename", WM_TOOL_WORKSPACE_NAME },
		{ "iconbar",       WM_TOOL_ICONBAR },
		{ "clock",         WM_TOOL_CLOCK },
		{ "prevwindow",    WM_TOOL_PREV_WINDOW },
		{ "nextwindow",    WM_TOOL_NEXT_WINDOW },
		{ "systemtray",    WM_TOOL_SYSTRAY },
	};
	for (size_t i = 0; i < sizeof(tools) / sizeof(tools[0]); i++) {
		if (strcasecmp(name, tools[i].name) == 0) {
			*out = tools[i].type;
			return true;
		}
	}
	return false;
}

/*
 * A tool of toolbar.tools: Fluxbox's names, spacer (sharing what is
 * left) or spacer_N (N pixels), and button.NAME with its label and
 * commands from the init; false for one that isn't.
 */
static bool
parse_tool(struct wm_toolbar *toolbar, const char *name,
	struct wm_toolbar_tool *tool)
{
	memset(tool, 0, sizeof(*tool));
	if (parse_tool_name(name, &tool->type))
		return true;
	if (strncasecmp(name, "spacer", 6) == 0) {
		int size = 0;
		if (name[6] && (name[6] != '_' || !safe_atoi(name + 7, &size) ||
		    size < 1))
			return false;
		tool->type = WM_TOOL_SPACER;
		tool->spacer_size = size;
		return true;
	}
	struct wm_config *config = toolbar->server->config;
	if (strncasecmp(name, "button.", 7) == 0 && config) {
		for (int i = 0; i < config->toolbar_button_count; i++) {
			struct wm_toolbar_button_cfg *b =
				&config->toolbar_buttons[i];
			if (strcasecmp(b->name, name + 7) != 0 || !b->label ||
			    !*b->label)
				continue;
			tool->type = WM_TOOL_BUTTON;
			tool->label = strdup(b->label);
			tool->commands = strdup(b->commands ? b->commands : "");
			return true;
		}
	}
	return false;
}

static void
parse_toolbar_tools(struct wm_toolbar *toolbar, const char *tools_str)
{
	toolbar->tools = NULL;
	toolbar->tool_count = 0;
	if (!tools_str || !*tools_str)
		return;
	char *copy = strdup(tools_str);
	if (!copy)
		return;
	struct wm_toolbar_tool tools[WM_TOOLBAR_MAX_TOOLS];
	int count = 0;
	char *saveptr = NULL;
	/* Fluxbox separates them with commas, fluxland 1.x with spaces */
	for (char *tok = strtok_r(copy, ", \t", &saveptr);
	     tok && count < WM_TOOLBAR_MAX_TOOLS;
	     tok = strtok_r(NULL, ", \t", &saveptr)) {
		if (parse_tool(toolbar, tok, &tools[count]))
			count++;
		else
			wlr_log(WLR_INFO, "toolbar: unknown tool '%s', skipping",
				tok);
	}
	free(copy);
	if (count == 0)
		return;

	toolbar->tools = calloc(count, sizeof(struct wm_toolbar_tool));
	if (!toolbar->tools) {
		for (int i = 0; i < count; i++) {
			free(tools[i].label);
			free(tools[i].commands);
		}
		return;
	}
	toolbar->tool_count = count;
	for (int i = 0; i < count; i++) {
		struct wm_toolbar_tool *tool = &toolbar->tools[i];
		*tool = tools[i];
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

static const char *
clock_format(struct wm_toolbar *toolbar)
{
	struct wm_config *config = toolbar->server->config;
	return config && config->clock_format ?
		config->clock_format : WM_TOOLBAR_CLOCK_FMT;
}

static int
tool_border(struct wm_toolbar *toolbar, enum wm_toolbar_tool_type type)
{
	struct wm_style *style = toolbar->server->style;
	if (tool_is_button(type) || type == WM_TOOL_BUTTON)
		return style->toolbar_button.border_width;
	switch (type) {
	case WM_TOOL_WORKSPACE_NAME:
		return style->toolbar_workspace.border_width;
	case WM_TOOL_CLOCK:
		return style->toolbar_clock.border_width;
	case WM_TOOL_ICONBAR:
		return style->toolbar_iconbar_border_width;
	default:
		return 0;
	}
}

/*
 * A fixed tool's width: the widest workspace name plus 2, the clock as
 * wide as its text with its digits as 0 and two 0s more (Fluxbox's
 * WorkspaceNameTool and ClockTool); 0 for a tool that isn't shown.
 */
static int
tool_fixed_width(struct wm_toolbar *toolbar, struct wm_toolbar_tool *tool)
{
	struct wm_server *server = toolbar->server;
	struct wm_style *style = server->style;
	switch (tool->type) {
	case WM_TOOL_WORKSPACE_NAME:
		if (toolbar->cached_ws_name_max_width < 0) {
			int max_tw = 0;
			struct wm_workspace *ws;
			wl_list_for_each(ws, &server->workspaces, link) {
				const char *name = ws->name && *ws->name ?
					ws->name : "1";
				int tw = wm_measure_text_width(name,
					&style->toolbar_workspace.font, 1.0f);
				if (tw > max_tw)
					max_tw = tw;
			}
			toolbar->cached_ws_name_max_width = max_tw;
		}
		return toolbar->cached_ws_name_max_width + 2;
	case WM_TOOL_CLOCK: {
		time_t now = time(NULL);
		struct tm tm;
		localtime_r(&now, &tm);
		char timebuf[64];
		size_t n = strftime(timebuf, sizeof(timebuf) - 2,
			clock_format(toolbar), &tm);
		for (size_t i = 0; i < n; i++) {
			if (isdigit((unsigned char)timebuf[i]))
				timebuf[i] = '0';
		}
		memcpy(timebuf + n, "00", 3);
		return wm_measure_text_width(timebuf,
			&style->toolbar_clock.font, 1.0f);
	}
	case WM_TOOL_SYSTRAY:
#ifdef WM_HAS_SYSTRAY
		if (server->systray)
			return wm_systray_get_width(server->systray);
#endif
		return 0;
	case WM_TOOL_SPACER:
		return tool->spacer_size;
	case WM_TOOL_BUTTON:
		return wm_measure_text_width(tool->label,
			&style->toolbar_button.font, 1.0f) + 2;
	default:
		return 0;
	}
}

/* Tools that share out what the fixed ones leave: the iconbar, and a
 * spacer without a size */
static bool
tool_is_relative(const struct wm_toolbar_tool *tool)
{
	return tool->type == WM_TOOL_ICONBAR ||
		(tool->type == WM_TOOL_SPACER && tool->spacer_size <= 0);
}

/*
 * The toolbar's height, as Fluxbox's Toolbar::reconfigure: the tallest
 * tool font plus 2, or the style's toolbar.height, or the init's, then
 * the bevel above and below and the border.
 */
static int
toolbar_compute_height(struct wm_toolbar *toolbar)
{
	struct wm_style *style = toolbar->server->style;
	struct wm_config *config = toolbar->server->config;
	if (!style)
		return WM_TOOLBAR_HEIGHT;
	int h = 0;
	const struct wm_font *fonts[] = { &style->toolbar_clock.font,
		&style->toolbar_iconbar_focused.font,
		&style->toolbar_iconbar_unfocused.font,
		&style->toolbar_workspace.font };
	for (size_t i = 0; i < sizeof(fonts) / sizeof(fonts[0]); i++) {
		int fh = wm_font_height(fonts[i], 1.0f);
		if (fh > h)
			h = fh;
	}
	h += 2;
	if (style->toolbar_height > 0)
		h = style->toolbar_height;
	if (config && config->toolbar_height > 0 && config->toolbar_height < 100)
		h = config->toolbar_height;
	return h + 2 * style->toolbar_bevel_width +
		2 * style->toolbar_border_width;
}

/* The tools to lay out: the configured ones, and the system tray after
 * them when they don't list it */
static int
layout_items(struct wm_toolbar *toolbar, struct wm_toolbar_tool **items)
{
	int n = 0;
	bool tray = false;
	for (int i = 0; i < toolbar->tool_count; i++) {
		items[n++] = &toolbar->tools[i];
		tray |= toolbar->tools[i].type == WM_TOOL_SYSTRAY;
	}
	if (!tray) {
		toolbar->systray_slot.type = WM_TOOL_SYSTRAY;
		items[n++] = &toolbar->systray_slot;
	}
	return n;
}

/*
 * Place the tools as Fluxbox's Toolbar::rearrangeItems does: bevelWidth
 * apart and in from the edges, each in its own border; with no bevel,
 * neighbours share a border and the outer ones fall outside.  Buttons
 * are square, the iconbar takes what is left.
 */
static void
compute_tool_layout(struct wm_toolbar *toolbar, int total_width)
{
	struct wm_style *style = toolbar->server->style;
	struct wm_toolbar_tool *items[WM_TOOLBAR_MAX_TOOLS + 1];
	int count = layout_items(toolbar, items);
	int tbw = style->toolbar_border_width;
	int bevel = style->toolbar_bevel_width;
	int width = total_width - 2 * tbw;
	int height = toolbar->height - 2 * tbw;
	int pref[WM_TOOLBAR_MAX_TOOLS + 1];

	int fixed = bevel, relative = 0, last_bw = 0;
	bool first = true;
	for (int i = 0; i < count; i++) {
		struct wm_toolbar_tool *tool = items[i];
		pref[i] = tool_fixed_width(toolbar, tool);
		tool->border = tool_border(toolbar, tool->type);
		if (tool->type == WM_TOOL_SYSTRAY && pref[i] <= 0)
			continue;	/* not shown */
		int bw = tool->border;
		if (bevel > 0) {
			fixed += bevel + 2 * bw;
		} else if (!first) {
			fixed += bw > last_bw ? bw : last_bw;
		}
		first = false;
		last_bw = bw;
		if (tool_is_button(tool->type)) {
			fixed += height - (bevel ? 2 * (bw + bevel) : 0);
		} else if (tool_is_relative(tool)) {
			relative++;
		} else {
			fixed += pref[i];
		}
	}
	int stretch = relative ? (width - fixed) / relative : 0;
	if (stretch < 0)
		stretch = 0;

	int next_x = 0;
	last_bw = 0;
	first = true;
	for (int i = 0; i < count; i++) {
		struct wm_toolbar_tool *tool = items[i];
		if (tool->type == WM_TOOL_SYSTRAY && pref[i] <= 0) {
			tool->width = tool->height = 0;
			continue;
		}
		int bw = tool->border;
		int offset = bevel, size_offset = 2 * (bw + bevel);
		if (bevel == 0) {
			offset = -bw;
			size_offset = 0;
			if (first)
				next_x = -bw;
			next_x += bw > last_bw ? bw : last_bw;
		}
		first = false;
		last_bw = bw;

		int w, h = height - size_offset;
		if (tool_is_relative(tool))
			w = stretch;
		else if (tool_is_button(tool->type))
			w = h;
		else
			w = pref[i];
		if (w < 1)
			w = 1;
		if (h < 1)
			h = 1;

		/* The frame, then what shows of it inside the border */
		tool->frame_width = w + 2 * bw;
		tool->frame_height = h + 2 * bw;
		int fx = tbw + next_x + offset, fy = tbw + offset;
		int x0 = fx < tbw ? tbw : fx;
		int y0 = fy < tbw ? tbw : fy;
		int x1 = fx + tool->frame_width;
		int y1 = fy + tool->frame_height;
		if (x1 > tbw + width)
			x1 = tbw + width;
		if (y1 > tbw + height)
			y1 = tbw + height;
		tool->x = x0;
		tool->y = y0;
		tool->width = x1 > x0 ? x1 - x0 : 0;
		tool->height = y1 > y0 ? y1 - y0 : 0;
		tool->frame_x = fx - x0;
		tool->frame_y = fy - y0;

		next_x += w + bevel;
		if (bevel != 0)
			next_x += 2 * bw;
	}
}

/* --- Per-tool render functions --- */

static cairo_surface_t *
new_surface(int width, int height)
{
	cairo_surface_t *surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(surface);
		return NULL;
	}
	return surface;
}

/*
 * A box of a tool: its border, and its texture inside; a ParentRelative
 * texture leaves the inside clear, so what is behind shows.
 */
static void
draw_box(cairo_t *cr, const struct wm_texture *tex, int border,
	const struct wm_color *border_color, int x, int y, int w, int h)
{
	if (w <= 0 || h <= 0)
		return;
	if (border > 0) {
		cairo_save(cr);
		cairo_set_source_rgba(cr, border_color->r / 255.0,
			border_color->g / 255.0, border_color->b / 255.0, 1.0);
		cairo_rectangle(cr, x, y, w, h);
		cairo_rectangle(cr, x + border, y + border,
			w - 2 * border, h - 2 * border);
		cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
		cairo_fill(cr);
		cairo_restore(cr);
	}
	int iw = w - 2 * border, ih = h - 2 * border;
	if (iw <= 0 || ih <= 0)
		return;
	cairo_surface_t *bg = wm_render_texture(tex, iw, ih, 1.0f);
	if (bg) {
		cairo_set_source_surface(cr, bg, x + border, y + border);
		cairo_paint(cr);
		cairo_surface_destroy(bg);
	}
}

/* Text in a box, as FbTk::TextButton: justified, a pixel in from the
 * sides, centred up and down */
static void
draw_text(cairo_t *cr, const char *text, const struct wm_font *font,
	const struct wm_color *color, enum wm_justify justify,
	int x, int y, int w, int h)
{
	int pad = 1;
	if (!text || w <= 0)
		return;
	/* The tools are sized to their text and 2, so the text may use
	 * the padding before it is cut short */
	int tw, th;
	cairo_surface_t *ts = wm_render_text(text, font, color,
		w, &tw, &th, justify, 1.0f);
	if (!ts)
		return;
	int tx = x + pad;
	if (justify == WM_JUSTIFY_CENTER)
		tx = x + (w - tw) / 2;
	else if (justify == WM_JUSTIFY_RIGHT)
		tx = x + w - pad - tw;
	if (tx + tw > x + w)
		tx = x + w - tw;
	if (tx < x)
		tx = x;
	int ty = y + (h - th) / 2;
	if (ty < y)
		ty = y;
	cairo_set_source_surface(cr, ts, tx, ty);
	cairo_paint(cr);
	cairo_surface_destroy(ts);
}

/* A tool's frame drawn into the surface of what shows of it */
static cairo_t *
begin_tool(struct wm_toolbar_tool *tool, cairo_surface_t **surface,
	const struct wm_texture *tex, const struct wm_tool_style *ts,
	int *cx, int *cy, int *cw, int *ch)
{
	*surface = new_surface(tool->width, tool->height);
	if (!*surface)
		return NULL;
	cairo_t *cr = cairo_create(*surface);
	draw_box(cr, tex, tool->border, &ts->border_color, tool->frame_x,
		tool->frame_y, tool->frame_width, tool->frame_height);
	*cx = tool->frame_x + tool->border;
	*cy = tool->frame_y + tool->border;
	*cw = tool->frame_width - 2 * tool->border;
	*ch = tool->frame_height - 2 * tool->border;
	return cr;
}

/* A filled triangle as FbDrawable::drawTriangle draws an arrow */
static void
draw_arrow(cairo_t *cr, enum wm_toolbar_tool_type type, int x, int y,
	int w, int h, int scale, const struct wm_color *color)
{
	if (scale < 100)
		scale = 100;
	if (scale > 10000)
		scale = 10000;
	int ax = 100 * w / scale, ay = 100 * h / scale;
	ax -= ax % 2;
	ay -= ay % 2;
	double cx = x + w / 2, cy = y + h / 2;
	bool left = type == WM_TOOL_PREV_WORKSPACE ||
		type == WM_TOOL_PREV_WINDOW;
	cairo_set_source_rgba(cr, color->r / 255.0, color->g / 255.0,
		color->b / 255.0, 1.0);
	if (left) {
		cairo_move_to(cr, cx - ax / 2, cy);
		cairo_line_to(cr, cx + ax / 2, cy - ay / 2);
		cairo_line_to(cr, cx + ax / 2, cy + ay / 2);
	} else {
		cairo_move_to(cr, cx + ax / 2, cy);
		cairo_line_to(cr, cx - ax / 2, cy + ay / 2);
		cairo_line_to(cr, cx - ax / 2, cy - ay / 2);
	}
	cairo_close_path(cr);
	cairo_fill(cr);
}

static struct wlr_buffer *
render_button_tool(struct wm_toolbar *toolbar, struct wm_toolbar_tool *tool,
	bool pressed)
{
	struct wm_style *style = toolbar->server->style;
	cairo_surface_t *surface;
	int cx, cy, cw, ch;
	cairo_t *cr = begin_tool(tool, &surface, pressed ?
		&style->toolbar_button_pressed : &style->toolbar_button.texture,
		&style->toolbar_button, &cx, &cy, &cw, &ch);
	if (!cr)
		return NULL;
	draw_arrow(cr, tool->type, cx, cy, cw, ch, style->toolbar_button_scale,
		&style->toolbar_button_pic_color);
	cairo_destroy(cr);
	return wlr_buffer_from_cairo(surface);
}

/* A button.NAME tool: its label in the button style (FbTk::TextButton) */
static struct wlr_buffer *
render_text_button(struct wm_toolbar *toolbar, struct wm_toolbar_tool *tool,
	bool pressed)
{
	struct wm_style *style = toolbar->server->style;
	cairo_surface_t *surface;
	int cx, cy, cw, ch;
	cairo_t *cr = begin_tool(tool, &surface, pressed ?
		&style->toolbar_button_pressed : &style->toolbar_button.texture,
		&style->toolbar_button, &cx, &cy, &cw, &ch);
	if (!cr)
		return NULL;
	draw_text(cr, tool->label, &style->toolbar_button.font,
		&style->toolbar_button.text_color, WM_JUSTIFY_CENTER,
		cx, cy, cw, ch);
	cairo_destroy(cr);
	return wlr_buffer_from_cairo(surface);
}

static struct wlr_buffer *
render_workspace_name_tool(struct wm_toolbar *toolbar,
	struct wm_toolbar_tool *tool)
{
	struct wm_server *server = toolbar->server;
	struct wm_style *style = server->style;
	struct wm_workspace *active_ws = wm_workspace_get_active(server);

	/* Single hit box covering the whole tool — allocate once, reuse */
	if (!tool->hit_boxes) {
		tool->hit_boxes = calloc(1, sizeof(struct wlr_box));
		if (!tool->hit_boxes) {
			tool->hit_box_count = 0;
			return NULL;
		}
	}
	tool->hit_box_count = 1;
	tool->hit_boxes[0] = (struct wlr_box){ 0, 0, tool->width, tool->height };

	cairo_surface_t *surface;
	int cx, cy, cw, ch;
	cairo_t *cr = begin_tool(tool, &surface,
		&style->toolbar_workspace.texture, &style->toolbar_workspace,
		&cx, &cy, &cw, &ch);
	if (!cr)
		return NULL;

	const char *name = active_ws ? active_ws->name : NULL;
	char fallback[16];
	if (!name || !*name) {
		snprintf(fallback, sizeof(fallback), "%d",
			active_ws ? active_ws->index + 1 : 1);
		name = fallback;
	}
	draw_text(cr, name, &style->toolbar_workspace.font,
		&style->toolbar_workspace.text_color,
		style->toolbar_workspace.justify, cx, cy, cw, ch);

	cairo_destroy(cr);
	return wlr_buffer_from_cairo(surface);
}

/* --- Collect icon bar entries based on the current mode --- */

/*
 * iconbar.mode is a window list (IconbarTool::setMode): "{static
 * groups} (workspace)" and the like; "none" lists nothing.
 */
static void
set_iconbar_list(struct wm_toolbar *toolbar, const char *spec)
{
	if (toolbar->iconbar_list) {
		wm_window_list_finish(toolbar->iconbar_list);
		free(toolbar->iconbar_list);
		toolbar->iconbar_list = NULL;
	}
	if (!spec || strcasecmp(spec, "none") == 0)
		return;
	struct wm_window_list_spec *list = calloc(1, sizeof(*list));
	if (!list)
		return;
	if (!wm_window_list_parse(spec, list)) {
		wlr_log(WLR_ERROR, "toolbar: bad iconbar.mode '%s'", spec);
		/* What Fluxbox writes by default */
		wm_window_list_parse("{static groups} (workspace)", list);
	}
	toolbar->iconbar_list = list;
}

/* Iconbar order: creation with {static}, else most recently focused */
static bool
iconbar_before(const struct wm_iconbar_entry *a,
	const struct wm_iconbar_entry *b, bool static_order)
{
	return static_order ? a->view->id < b->view->id :
		a->view->focus_seq > b->view->focus_seq;
}

static void
sort_iconbar(struct wm_iconbar_entry *entries, int count, bool static_order)
{
	for (int i = 1; i < count; i++) {
		struct wm_iconbar_entry e = entries[i];
		int j = i - 1;
		while (j >= 0 && iconbar_before(&e, &entries[j], static_order)) {
			entries[j + 1] = entries[j];
			j--;
		}
		entries[j + 1] = e;
	}
}

static int
collect_iconbar_entries(struct wm_toolbar *toolbar)
{
	struct wm_server *server = toolbar->server;
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

	const struct wm_window_list_spec *list = toolbar->iconbar_list;
	struct wm_view *view;
	wl_list_for_each(view, &server->views, link) {
		if (!list || count >= WM_TOOLBAR_ICONBAR_MAX) {
			break;
		}

		/* Mapped, managed windows only; apps [IconHidden] skips */
		if (!view->mapped || view->docked || view->icon_hidden) {
			continue;
		}
		/* {groups}: a tab group is one entry, its current tab */
		if (list->groups && view->tab_group &&
		    view->tab_group->active_view != view) {
			continue;
		}
		if (list->pattern &&
		    !wm_client_pattern_match(server, list->pattern, view)) {
			continue;
		}

		struct wm_iconbar_entry *entry = &toolbar->ib_entries[count];
		entry->view = view;
		entry->focused = (view == server->focused_view);
		entry->iconified = !view->scene_tree->node.enabled;
		entry->icon = view->icon;
		count++;
	}

	sort_iconbar(toolbar->ib_entries, count, list && list->static_order);
	toolbar->ib_count = count;
	return count;
}

/* --- Render icon bar (window list) --- */

/* An iconbar entry's title: an iconified window's through
 * iconifiedPattern */
static void
entry_title(struct wm_toolbar *toolbar, struct wm_iconbar_entry *entry,
	char *title_buf, size_t title_size)
{
	struct wm_config *cfg = toolbar->server->config;
	const char *raw_title = entry->view->title;
	if (!raw_title || !*raw_title)
		raw_title = _("(untitled)");
	if (!entry->iconified) {
		snprintf(title_buf, title_size, "%s", raw_title);
		return;
	}

	const char *pattern = cfg ? cfg->iconbar_iconified_pattern : NULL;
	if (!pattern)
		pattern = "( %t )";  /* Fluxbox's */
	/* The title goes where %t is (Fluxbox; fluxland 1.x wrote %s);
	 * the pattern is data, not a format */
	size_t plen = strlen(pattern);
	const char *pct_s = strstr(pattern, "%t");
	if (!pct_s)
		pct_s = strstr(pattern, "%s");
	if (pct_s) {
		size_t prefix = (size_t)(pct_s - pattern);
		size_t suffix = plen - prefix - 2;
		snprintf(title_buf, title_size, "%.*s%s%.*s", (int)prefix,
			pattern, raw_title, (int)suffix, pct_s + 2);
	} else {
		snprintf(title_buf, title_size, "%s", pattern);
	}
}

/*
 * The iconbar: its empty texture, and the entries as FbTk::Container
 * places them, side by side with their borders shared and the outer
 * ones outside; Relative shares the width out, the other alignments
 * make them iconWidth wide.
 */
static struct wlr_buffer *
render_iconbar(struct wm_toolbar *toolbar, struct wm_toolbar_tool *tool)
{
	struct wm_server *server = toolbar->server;
	struct wm_style *style = server->style;
	struct wm_config *cfg = server->config;

	collect_iconbar_entries(toolbar);

	struct wm_tool_style frame = {
		.border_color = style->toolbar_iconbar_border_color,
	};
	cairo_surface_t *surface;
	int cx, cy, cw, ch;
	cairo_t *cr = begin_tool(tool, &surface, &style->toolbar_iconbar_empty,
		&frame, &cx, &cy, &cw, &ch);
	if (!cr)
		return NULL;

	int count = toolbar->ib_count;
	if (count == 0 || cw <= 0) {
		cairo_destroy(cr);
		return wlr_buffer_from_cairo(surface);
	}
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

	const struct wm_tool_style *first = toolbar->ib_entries[0].focused ?
		&style->toolbar_iconbar_focused :
		&style->toolbar_iconbar_unfocused;
	int bw = first->border_width;
	int alignment = cfg ? cfg->iconbar_alignment : 1;
	int total = cw, maxw, extra = 0;
	if (alignment == 1) {
		maxw = (cw - (count - 1) * bw) / count;
		extra = cw - ((maxw + bw) * count - bw);
	} else {
		maxw = cfg && cfg->iconbar_icon_width > 0 ?
			cfg->iconbar_icon_width : 128;
		total = (maxw + bw) * count - bw;
		if (total > cw) {
			total = cw;
			maxw = (cw - (count - 1) * bw) / count;
		}
	}
	if (maxw < 1)
		maxw = 1;
	int next_x = -bw;
	if (alignment == 2)
		next_x += cw - total;
	else if (alignment == 3)
		next_x += (cw - total) / 2;

	cairo_save(cr);
	cairo_rectangle(cr, cx, cy, cw, ch);
	cairo_clip(cr);
	for (int i = 0; i < count; i++) {
		struct wm_iconbar_entry *entry = &toolbar->ib_entries[i];
		const struct wm_tool_style *ts = entry->focused ?
			&style->toolbar_iconbar_focused :
			&style->toolbar_iconbar_unfocused;
		int w = maxw + (i < extra ? 1 : 0);
		int ex = cx + next_x + bw;	/* inside the entry's border */
		draw_box(cr, &ts->texture, ts->border_width, &ts->border_color,
			ex - ts->border_width, cy - ts->border_width,
			w + 2 * ts->border_width, ch + 2 * ts->border_width);

		/* Icon (session.screen0.iconbar.usePixmap), then the title,
		 * iconTextPadding (10) around it */
		int icon_w = 0;
		cairo_surface_t *icon = entry->view->icon;
		int isize = ch - 2;
		if (icon && (!cfg || cfg->iconbar_use_pixmap) && isize >= 8 &&
		    w > isize * 2) {
			int isrc = cairo_image_surface_get_width(icon);
			cairo_save(cr);
			cairo_translate(cr, ex + 1, cy + (ch - isize) / 2);
			cairo_scale(cr, (double)isize / isrc,
				(double)isize / isrc);
			cairo_set_source_surface(cr, icon, 0, 0);
			cairo_pattern_set_filter(cairo_get_source(cr),
				CAIRO_FILTER_GOOD);
			cairo_paint_with_alpha(cr, entry->iconified ? 0.5 : 1.0);
			cairo_restore(cr);
			icon_w = isize + 2;
		}
		char title[256];
		entry_title(toolbar, entry, title, sizeof(title));
		draw_text(cr, title, &ts->font, &ts->text_color, ts->justify,
			ex + icon_w + 5, cy, w - icon_w - 10, ch);

		toolbar->ib_boxes[i] = (struct wlr_box){ ex, 0, w, tool->height };
		next_x += w + bw;
	}
	cairo_restore(cr);

	cairo_destroy(cr);
	return wlr_buffer_from_cairo(surface);
}

/* --- Render clock --- */

static struct wlr_buffer *
render_clock(struct wm_toolbar *toolbar, struct wm_toolbar_tool *tool)
{
	struct wm_style *style = toolbar->server->style;

	time_t now = time(NULL);
	struct tm tm;
	localtime_r(&now, &tm);
	char timebuf[64];
	strftime(timebuf, sizeof(timebuf), clock_format(toolbar), &tm);

	/* Cache to avoid redraw if time string unchanged */
	if (strcmp(timebuf, toolbar->cached_clock) == 0)
		return NULL;
	snprintf(toolbar->cached_clock, sizeof(toolbar->cached_clock), "%s",
		timebuf);

	cairo_surface_t *surface;
	int cx, cy, cw, ch;
	cairo_t *cr = begin_tool(tool, &surface, &style->toolbar_clock.texture,
		&style->toolbar_clock, &cx, &cy, &cw, &ch);
	if (!cr)
		return NULL;
	draw_text(cr, timebuf, &style->toolbar_clock.font,
		&style->toolbar_clock.text_color, style->toolbar_clock.justify,
		cx, cy, cw, ch);
	cairo_destroy(cr);
	return wlr_buffer_from_cairo(surface);
}

/* --- Render a single tool --- */

static void
render_tool(struct wm_toolbar *toolbar, struct wm_toolbar_tool *tool)
{
	struct wlr_buffer *buf = NULL;

	if (tool->type == WM_TOOL_SYSTRAY) {
#ifdef WM_HAS_SYSTRAY
		if (toolbar->server->systray && tool->width > 0)
			wm_systray_layout(toolbar->server->systray,
				toolbar->x + tool->x, toolbar->y + tool->y,
				tool->width, tool->height);
#endif
		return;
	}
	if (!tool->buf)
		return;
	if (tool->width <= 0 || tool->height <= 0) {
		wlr_scene_node_set_enabled(&tool->buf->node, false);
		return;
	}
	wlr_scene_node_set_enabled(&tool->buf->node, true);

	switch (tool->type) {
	case WM_TOOL_PREV_WORKSPACE:
	case WM_TOOL_NEXT_WORKSPACE:
	case WM_TOOL_PREV_WINDOW:
	case WM_TOOL_NEXT_WINDOW:
		buf = render_button_tool(toolbar, tool, toolbar->tools &&
			tool - toolbar->tools == toolbar->pressed_tool);
		break;
	case WM_TOOL_WORKSPACE_NAME:
		buf = render_workspace_name_tool(toolbar, tool);
		break;
	case WM_TOOL_ICONBAR:
		buf = render_iconbar(toolbar, tool);
		break;
	case WM_TOOL_CLOCK:
		toolbar->cached_clock[0] = '\0'; /* force redraw */
		buf = render_clock(toolbar, tool);
		break;
	case WM_TOOL_BUTTON:
		buf = render_text_button(toolbar, tool, toolbar->tools &&
			tool - toolbar->tools == toolbar->pressed_tool);
		break;
	case WM_TOOL_SPACER:
	case WM_TOOL_SYSTRAY:
		break;
	}

	if (buf) {
		wlr_scene_buffer_set_buffer(tool->buf, buf);
		wlr_buffer_drop(buf);
	}
	wlr_scene_node_set_position(&tool->buf->node, tool->x, tool->y);
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

	/* Background: the toolbar's border and texture */
	cairo_surface_t *bg_surface = new_surface(w, h);
	if (bg_surface) {
		cairo_t *bcr = cairo_create(bg_surface);
		draw_box(bcr, &style->toolbar_texture,
			style->toolbar_border_width,
			&style->toolbar_border_color, 0, 0, w, h);
		cairo_destroy(bcr);
	}
	struct wlr_buffer *bg = wlr_buffer_from_cairo(bg_surface);
	wlr_scene_buffer_set_buffer(toolbar->bg_buf, bg);
	if (bg) {
		wlr_buffer_drop(bg);
	}

	/* Compute layout for all tools */
	compute_tool_layout(toolbar, w);

	/* Render each tool, and the system tray where it goes */
	for (int i = 0; i < toolbar->tool_count; i++) {
		render_tool(toolbar, &toolbar->tools[i]);
	}
	if (toolbar->systray_slot.type == WM_TOOL_SYSTRAY)
		render_tool(toolbar, &toolbar->systray_slot);

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
			/* The focused iconbar entry's colour */
			uint32_t fc =
				style->toolbar_iconbar_focused.texture.color;
			fc_r = ((fc >> 16) & 0xFF) / 255.0;
			fc_g = ((fc >> 8) & 0xFF) / 255.0;
			fc_b = (fc & 0xFF) / 255.0;
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
			toolbar->clock_tool);
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
	toolbar->height = toolbar_compute_height(toolbar);
	toolbar->pressed_tool = -1;
	toolbar->cached_ws_index = -1;
	toolbar->cached_clock[0] = '\0';
	toolbar->cached_ws_name_max_width = -1;

	/* Pre-allocate iconbar arrays to avoid malloc/free churn on
	 * every focus change, title change, and workspace switch. */
	toolbar->ib_entries = calloc(WM_TOOLBAR_ICONBAR_MAX,
		sizeof(struct wm_iconbar_entry));
	toolbar->ib_boxes = calloc(WM_TOOLBAR_ICONBAR_MAX,
		sizeof(struct wlr_box));
	toolbar->ib_cached_entries = calloc(WM_TOOLBAR_ICONBAR_MAX,
		sizeof(struct wm_iconbar_entry));
	toolbar->ib_cached_count = -1;

	/* Read placement config */
	if (server->config) {
		enum wm_toolbar_placement p = server->config->toolbar_placement;
		toolbar->on_top = (p == WM_TOOLBAR_TOP_LEFT ||
			p == WM_TOOLBAR_TOP_CENTER ||
			p == WM_TOOLBAR_TOP_RIGHT);
		toolbar->auto_hide = server->config->toolbar_auto_hide;
		toolbar->iconbar_mode = server->config->iconbar_mode;
		set_iconbar_list(toolbar, server->config->iconbar_mode_spec);
	}

	/* Auto-hide: start hidden */
	toolbar->shown = !toolbar->auto_hide;

	/* Create scene tree in layer_top so it renders above windows */
	toolbar->scene_tree = wlr_scene_tree_create(server->layer_top);
	if (!toolbar->scene_tree) {
		free(toolbar);
		return NULL;
	}
	apply_toolbar_layer(toolbar);

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
	set_iconbar_list(toolbar, "none");

	if (toolbar->hide_timer) {
		wl_event_source_remove(toolbar->hide_timer);
	}

	if (toolbar->scene_tree) {
		wlr_scene_node_destroy(&toolbar->scene_tree->node);
	}

	/* Free tool hit boxes */
	for (int i = 0; i < toolbar->tool_count; i++) {
		free(toolbar->tools[i].hit_boxes);
		free(toolbar->tools[i].label);
		free(toolbar->tools[i].commands);
	}
	free(toolbar->tools);

	free(toolbar->ib_boxes);
	free(toolbar->ib_entries);
	free(toolbar->ib_cached_entries);
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

/* Check if iconbar entries match the cached state */
static bool
iconbar_entries_match_cache(struct wm_toolbar *toolbar)
{
	if (toolbar->ib_cached_count < 0) {
		return false;
	}
	if (toolbar->ib_count != toolbar->ib_cached_count) {
		return false;
	}
	for (int i = 0; i < toolbar->ib_count; i++) {
		struct wm_iconbar_entry *cur = &toolbar->ib_entries[i];
		struct wm_iconbar_entry *cached = &toolbar->ib_cached_entries[i];
		if (cur->view != cached->view ||
		    cur->focused != cached->focused ||
		    cur->iconified != cached->iconified ||
		    cur->icon != cached->icon) {
			return false;
		}
		/* Compare titles: pointer match is fast path */
		const char *cur_title = cur->view ? cur->view->title : NULL;
		const char *cached_title = cached->view ? cached->view->title : NULL;
		if (cur_title != cached_title) {
			if (!cur_title || !cached_title ||
			    strcmp(cur_title, cached_title) != 0) {
				return false;
			}
		}
	}
	return true;
}

/* Snapshot current entries into the cache */
static void
iconbar_snapshot_cache(struct wm_toolbar *toolbar)
{
	if (!toolbar->ib_cached_entries) {
		toolbar->ib_cached_entries = calloc(WM_TOOLBAR_ICONBAR_MAX,
			sizeof(struct wm_iconbar_entry));
		if (!toolbar->ib_cached_entries) {
			return;
		}
	}
	memcpy(toolbar->ib_cached_entries, toolbar->ib_entries,
		toolbar->ib_count * sizeof(struct wm_iconbar_entry));
	toolbar->ib_cached_count = toolbar->ib_count;
}

void
wm_toolbar_update_iconbar(struct wm_toolbar *toolbar)
{
	if (!toolbar || !toolbar->visible) {
		return;
	}

	if (toolbar->iconbar_tool) {
		/* Collect entries and check if anything changed */
		collect_iconbar_entries(toolbar);
		if (iconbar_entries_match_cache(toolbar)) {
			return;
		}

		struct wlr_buffer *ib_buf = render_iconbar(toolbar,
			toolbar->iconbar_tool);
		wlr_scene_buffer_set_buffer(toolbar->iconbar_tool->buf,
			ib_buf);
		if (ib_buf) {
			wlr_buffer_drop(ib_buf);
		}

		iconbar_snapshot_cache(toolbar);
	}
}

void
wm_toolbar_relayout(struct wm_toolbar *toolbar)
{
	if (!toolbar) {
		return;
	}

	struct wm_output *output = toolbar_output(toolbar->server);
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
		set_iconbar_list(toolbar, config->iconbar_mode_spec);
		apply_toolbar_layer(toolbar);

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

	struct wlr_box ob;
	wm_output_layout_box(output, &ob);
	int output_width = ob.width, output_height = ob.height;

	/* Apply width percentage */
	int width_pct = config ? config->toolbar_width_percent : 100;
	/* Fluxbox: the percentage of the head less the border, which
	 * goes around it */
	int tbw = toolbar->server->style ?
		toolbar->server->style->toolbar_border_width : 0;
	toolbar->height = toolbar_compute_height(toolbar);
	toolbar->width = (output_width - 2 * tbw) * width_pct / 100 + 2 * tbw;
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

	/* Output-relative so far; the scene works in layout coordinates */
	toolbar->x += ob.x;
	toolbar->y += ob.y;
	wlr_scene_node_set_position(&toolbar->scene_tree->node,
		toolbar->x, toolbar->y);

	toolbar_render(toolbar);

	/* Windows make room for it (wm_toolbar_reserve) */
	struct wm_output *o;
	wl_list_for_each(o, &toolbar->server->outputs, link)
		wm_output_arrange(o);
}

void
wm_toolbar_reserve(struct wm_toolbar *toolbar, struct wm_output *output,
	struct wlr_box *usable)
{
	/*
	 * Like Fluxbox: a hidden, auto-hiding or maxOver toolbar leaves
	 * the space to windows.
	 */
	if (!toolbar || !toolbar->visible || toolbar->auto_hide ||
	    output != toolbar_output(toolbar->server))
		return;
	struct wm_config *config = toolbar->server->config;
	if (config && config->toolbar_max_over)
		return;

	if (toolbar->on_top) {
		int bottom = toolbar->y + toolbar->height;
		if (bottom > usable->y) {
			usable->height -= bottom - usable->y;
			usable->y = bottom;
		}
	} else if (toolbar->y < usable->y + usable->height) {
		usable->height = toolbar->y - usable->y;
	}
	if (usable->height < 0)
		usable->height = 0;
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
wm_toolbar_contains(struct wm_toolbar *toolbar, double lx, double ly)
{
	if (!toolbar || !toolbar->visible ||
	    (toolbar->auto_hide && !toolbar->shown))
		return false;
	double x = lx - toolbar->x, y = ly - toolbar->y;
	return x >= 0 && x < toolbar->width && y >= 0 && y < toolbar->height;
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

	/* Toolbar::buttonPressEvent: the middle button opens its menu
	 * (Fluxbox's only on the toolbar's own background; here on any
	 * tool without a middle-button command of its own) */
	if (button == 274) {
		struct wm_toolbar_tool *t = NULL;
		for (int i = 0; i < toolbar->tool_count; i++) {
			if (local_x >= toolbar->tools[i].x && local_x <
			    toolbar->tools[i].x + toolbar->tools[i].width)
				t = &toolbar->tools[i];
		}
		if (!t || t->type != WM_TOOL_BUTTON ||
		    !strchr(t->commands ? t->commands : "", ':')) {
			wm_menu_show_toolbar_menu(toolbar->server, (int)lx,
				(int)ly);
			return true;
		}
	}

	/* Find which tool was clicked */
	for (int i = 0; i < toolbar->tool_count; i++) {
		struct wm_toolbar_tool *tool = &toolbar->tools[i];
		if (local_x < tool->x || local_x >= tool->x + tool->width) {
			continue;
		}

		/* Arrow buttons show toolbar.button.pressed while held
		 * and act on release (wm_toolbar_handle_release), as
		 * Fluxbox's do; only the left one */
		if (tool_is_button(tool->type) && button != 272)
			return true;
		if (tool_is_button(tool->type) ||
		    tool->type == WM_TOOL_BUTTON) {
			toolbar->pressed_tool = i;
			toolbar->pressed_button = button;
			render_tool(toolbar, tool);
			return true;
		}

		switch (tool->type) {
		case WM_TOOL_PREV_WORKSPACE:
		case WM_TOOL_NEXT_WORKSPACE:
		case WM_TOOL_PREV_WINDOW:
		case WM_TOOL_NEXT_WINDOW:
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
					if (!view)
						return true;
					/* IconbarTool: the right button
					 * opens its window menu; the left
					 * one iconifies the focused
					 * window, focuses the others */
					if (button == 273) {
						toolbar->server->menu_view = view;
						wm_menu_show_window(
							toolbar->server,
							(int)lx, (int)ly);
					} else if (button != 272) {
						/* nothing */
					} else if (!view->scene_tree->node.enabled) {
						deiconify_view(view);
					} else if (view ==
						   toolbar->server->focused_view) {
						wm_view_minimize(view);
					} else {
						wm_focus_view(view, NULL);
						wm_view_raise(view);
					}
					return true;
				}
			}
			return true;
		}

		case WM_TOOL_WORKSPACE_NAME:
			/* ToolFactory: prevworkspace on the left button,
			 * nextworkspace on the right */
			if (button == 272)
				wm_workspace_switch_prev(toolbar->server);
			else if (button == 273)
				wm_workspace_switch_next(toolbar->server);
			return true;

		case WM_TOOL_CLOCK:
		case WM_TOOL_SYSTRAY:
			/* No click action */
			return true;
		}
	}

	return false;
}

/* A button.NAME's command for a mouse button: the Nth of its commands
 * separated by ':' (left 1, middle 2, right 3), on the focused window */
void
wm_toolbar_run_button(struct wm_toolbar *toolbar, struct wm_toolbar_tool *tool,
	uint32_t button)
{
	int n = button == 272 ? 1 : button == 274 ? 2 : button == 273 ? 3 : 0;
	if (n == 0 || !tool->commands)
		return;
	char *copy = strdup(tool->commands);
	if (!copy)
		return;
	char *save = NULL, *cmd = strtok_r(copy, ":", &save);
	for (int i = 1; cmd && i < n; i++)
		cmd = strtok_r(NULL, ":", &save);
	struct wm_command *parsed = cmd ? wm_command_parse(cmd) : NULL;
	if (parsed) {
		wm_command_execute(toolbar->server, parsed,
			toolbar->server->focused_view);
		wm_command_destroy(parsed);
	}
	free(copy);
}

bool
wm_toolbar_handle_release(struct wm_toolbar *toolbar, double lx, double ly,
	uint32_t button)
{
	if (!toolbar || toolbar->pressed_tool < 0 ||
	    toolbar->pressed_tool >= toolbar->tool_count)
		return false;
	struct wm_toolbar_tool *tool = &toolbar->tools[toolbar->pressed_tool];
	toolbar->pressed_tool = -1;
	render_tool(toolbar, tool);
	if (button != toolbar->pressed_button)
		return true;

	/* Released over the button it was pressed on */
	double x = lx - toolbar->x, y = ly - toolbar->y;
	if (x < tool->x || x >= tool->x + tool->width ||
	    y < tool->y || y >= tool->y + tool->height)
		return true;
	switch (tool->type) {
	case WM_TOOL_PREV_WORKSPACE:
		wm_workspace_switch_prev(toolbar->server);
		break;
	case WM_TOOL_NEXT_WORKSPACE:
		wm_workspace_switch_next(toolbar->server);
		break;
	case WM_TOOL_PREV_WINDOW:
		wm_focus_prev_view(toolbar->server);
		break;
	case WM_TOOL_NEXT_WINDOW:
		wm_focus_next_view(toolbar->server);
		break;
	case WM_TOOL_BUTTON:
		wm_toolbar_run_button(toolbar, tool, button);
		break;
	default:
		break;
	}
	return true;
}

void
wm_toolbar_notify_pointer_motion(struct wm_toolbar *toolbar,
	double lx, double ly)
{
	if (!toolbar || !toolbar->visible || !toolbar->auto_hide) {
		return;
	}

	struct wm_output *output = toolbar_output(toolbar->server);
	if (!output) {
		return;
	}

	struct wlr_box ob;
	wm_output_layout_box(output, &ob);

	/* Define trigger zone: 1px strip at the edge where toolbar lives */
	bool in_trigger_zone = false;
	bool in_columns = lx >= ob.x && lx < ob.x + ob.width;
	if (toolbar->on_top) {
		in_trigger_zone = in_columns && ly <= ob.y + 1 && ly >= ob.y;
	} else {
		in_trigger_zone = in_columns &&
			ly >= ob.y + ob.height - 1 && ly < ob.y + ob.height;
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

	/* Above the windows, and back to toolbar.layer (Fluxbox's
	 * Toolbar::toggleAboveDock) */
	toolbar->raised = !toolbar->raised;
	apply_toolbar_layer(toolbar);

	wlr_log(WLR_INFO, "toolbar toggled above: %d", toolbar->raised);
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
