/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * decoration.c - Server-side window decoration rendering and management
 */

#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <cairo.h>
#include <drm_fourcc.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_server_decoration.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>

#include "config.h"
#include "decoration.h"
#include "perf.h"
#include "pixel_buffer.h"
#include "render.h"
#include "server.h"
#include "style.h"
#include "tabgroup.h"
#include "view.h"

#ifdef WM_PERF_ENABLE
static struct wm_perf_probe perf_decoration_update;
static bool perf_decoration_inited;
#endif

/* --- Default dimensions --- */

#define DEFAULT_TITLEBAR_HEIGHT 20
#define DEFAULT_HANDLE_HEIGHT 6
#define DEFAULT_BORDER_WIDTH 1
#define DEFAULT_GRIP_WIDTH 20
#define DEFAULT_BUTTON_SIZE 12
#define BUTTON_PADDING 4

/* --- Button type parsing --- */

static bool
parse_button_type(const char *name, enum wm_button_type *out)
{
	if (strcasecmp(name, "Close") == 0) {
		*out = WM_BUTTON_CLOSE;
		return true;
	}
	if (strcasecmp(name, "Maximize") == 0) {
		*out = WM_BUTTON_MAXIMIZE;
		return true;
	}
	if (strcasecmp(name, "Minimize") == 0 ||
	    strcasecmp(name, "Iconify") == 0) {
		*out = WM_BUTTON_ICONIFY;
		return true;
	}
	if (strcasecmp(name, "Shade") == 0) {
		*out = WM_BUTTON_SHADE;
		return true;
	}
	if (strcasecmp(name, "Stick") == 0) {
		*out = WM_BUTTON_STICK;
		return true;
	}
	if (strcasecmp(name, "MenuIcon") == 0) {
		*out = WM_BUTTON_MENUICON;
		return true;
	}
	if (strcasecmp(name, "LHalf") == 0) {
		*out = WM_BUTTON_LHALF;
		return true;
	}
	if (strcasecmp(name, "RHalf") == 0) {
		*out = WM_BUTTON_RHALF;
		return true;
	}
	return false;
}

/*
 * Parse a button layout string like "Shade Minimize Maximize Close"
 * into an array of wm_decor_button. Returns count via out_count.
 */
static struct wm_decor_button *
parse_button_layout(const char *str, int *out_count)
{
	*out_count = 0;
	if (!str || !*str) {
		return NULL;
	}

	/* Count tokens first */
	int count = 0;
	char *tmp = strdup(str);
	if (!tmp) {
		return NULL;
	}
	char *saveptr;
	char *token = strtok_r(tmp, " \t", &saveptr);
	while (token) {
		enum wm_button_type type;
		if (parse_button_type(token, &type)) {
			count++;
		}
		token = strtok_r(NULL, " \t", &saveptr);
	}
	free(tmp);

	if (count == 0) {
		return NULL;
	}

	struct wm_decor_button *buttons = calloc(count, sizeof(*buttons));
	if (!buttons) {
		return NULL;
	}

	/* Parse again to fill */
	tmp = strdup(str);
	if (!tmp) {
		free(buttons);
		return NULL;
	}
	int i = 0;
	token = strtok_r(tmp, " \t", &saveptr);
	while (token && i < count) {
		enum wm_button_type type;
		if (parse_button_type(token, &type)) {
			buttons[i].type = type;
			buttons[i].pressed = false;
			buttons[i].node = NULL;
			i++;
		}
		token = strtok_r(NULL, " \t", &saveptr);
	}
	free(tmp);

	*out_count = count;
	return buttons;
}

/* --- Color conversion for scene rects --- */

static void
wm_color_to_float4(const struct wm_color *c, float out[4])
{
	out[0] = c->r / 255.0f;
	out[1] = c->g / 255.0f;
	out[2] = c->b / 255.0f;
	out[3] = c->a / 255.0f;
}

/* --- Frame geometry, as Fluxbox's FbWinFrame --- */

/*
 * Where everything of a decorated window goes, in the decoration tree's
 * coordinates: the frame (border, titlebar, a border line, the client,
 * a border line, the handle) and the external tabs that hang from it.
 * Drawing, hit-testing and the extents all come from here.
 */
struct frame_geom {
	bool titlebar, handle, borders, ext_tabs;
	int bw, bevel, th, hh, gw, cw, ch;
	int fx, fy, fw, fh;		/* the frame's outer box */
	int title_x, title_y;		/* the titlebar, cw x th */
	int content_x, content_y;
	int handle_x, handle_y;		/* the handle row, cw x hh */
	int button_size;		/* square, bevel apart */
	int label_x, label_w;		/* in the titlebar, bevel down */
	/* External tabs: the container's outer box, its border (the
	 * window's) and each tab's length along it */
	int tx, ty, tw, tth, tab_len, tab_count, tab_border;
	bool tabs_vertical, tabs_left;
	int total_w, total_h;
};

static bool
preset_has_titlebar(enum wm_decor_preset p)
{
	return p == WM_DECOR_NORMAL || p == WM_DECOR_TAB ||
		p == WM_DECOR_TINY || p == WM_DECOR_TOOL;
}

/* Tabs outside the frame (session.screen0.tabs.intitlebar: false) */
static bool
deco_external_tabs(struct wm_decoration *deco)
{
	struct wm_config *config = deco->server ? deco->server->config : NULL;
	return config && !config->tabs_intitlebar &&
		(deco->preset == WM_DECOR_NORMAL ||
		 deco->preset == WM_DECOR_TAB);
}

static int
deco_tab_count(struct wm_decoration *deco)
{
	struct wm_tab_group *tg = deco->view ? deco->view->tab_group : NULL;
	return tg && tg->count > 0 ? tg->count : 1;
}

static void
frame_geometry(struct wm_decoration *deco, const struct wm_style *style,
	struct frame_geom *g)
{
	memset(g, 0, sizeof(*g));
	enum wm_decor_preset p = deco->preset;
	g->titlebar = preset_has_titlebar(p);
	g->borders = p == WM_DECOR_NORMAL || p == WM_DECOR_BORDER;
	g->bw = g->borders || g->titlebar ? deco->border_width : 0;
	g->bevel = style ? style->window_bevel_width : 0;
	g->th = deco->titlebar_height;
	g->hh = deco->handle_height;
	g->handle = p == WM_DECOR_NORMAL && !deco->shaded && g->hh > 0;
	g->gw = deco->grip_width;
	g->cw = deco->content_width;
	g->ch = deco->shaded ? 0 : deco->content_height;

	/* The frame: the titlebar's bottom border and the handle's top
	 * border are border lines inside it */
	int inner_h = g->ch;
	if (g->titlebar)
		inner_h += g->th + (deco->shaded ? 0 : g->bw);
	if (g->handle)
		inner_h += g->hh + g->bw;
	g->fw = g->cw + 2 * g->bw;
	g->fh = inner_h + 2 * g->bw;

	/* Titlebar contents (FbWinFrame::reconfigureTitlebar) */
	g->button_size = g->th - 2 * g->bevel;
	if (g->button_size < 1)
		g->button_size = 1;
	int nl = deco->buttons_left_count, nr = deco->buttons_right_count;
	g->label_x = g->bevel + nl * (g->button_size + g->bevel) + g->bevel;
	g->label_w = g->cw - g->label_x - nr * (g->button_size + g->bevel) -
		g->bevel;
	if (g->label_w <= 0)
		g->label_w = 1;

	/* External tabs (FbWinFrame::alignTabs): as thick as a button,
	 * tab.width long each, packed at the start, middle or end of a
	 * side, sharing the frame's border line */
	int tox = 0, toy = 0;
	if (deco_external_tabs(deco)) {
		struct wm_config *config = deco->server->config;
		int place = config->tab_placement;
		g->ext_tabs = true;
		g->tabs_vertical = place >= WM_TAB_LEFT_TOP;
		g->tabs_left = place >= WM_TAB_LEFT_TOP &&
			place <= WM_TAB_LEFT_BOTTOM;
		g->tab_count = deco_tab_count(deco);
		g->tab_border = style ? style->window_label_focus_border_width : 0;
		int n = g->tab_count, ib = g->tab_border;
		int frame_len = g->tabs_vertical ? inner_h : g->cw;
		g->tab_len = config->tab_width > 0 ? config->tab_width : 64;
		int total = (g->tab_len + ib) * n - ib;
		if (total > frame_len) {
			total = frame_len;
			g->tab_len = (frame_len - (n - 1) * ib) / n;
			if (g->tab_len < 1)
				g->tab_len = 1;
		}
		int thick = g->button_size;
		int cwid = g->tabs_vertical ? thick : total;	/* content */
		int chgt = g->tabs_vertical ? total : thick;
		int w = g->cw, h = inner_h, bw = g->bw;
		switch (place) {
		case WM_TAB_TOP_LEFT:	toy = -(chgt + bw); break;
		case WM_TAB_TOP:	tox = (w - cwid) / 2; toy = -(chgt + bw); break;
		case WM_TAB_TOP_RIGHT:	tox = w - cwid; toy = -(chgt + bw); break;
		case WM_TAB_BOTTOM_LEFT: toy = h + bw; break;
		case WM_TAB_BOTTOM:	tox = (w - cwid) / 2; toy = h + bw; break;
		case WM_TAB_BOTTOM_RIGHT: tox = w - cwid; toy = h + bw; break;
		case WM_TAB_LEFT_TOP:	tox = -(cwid + bw); break;
		case WM_TAB_LEFT:	tox = -(cwid + bw); toy = (h - chgt) / 2; break;
		case WM_TAB_LEFT_BOTTOM: tox = -(cwid + bw); toy = h - chgt; break;
		case WM_TAB_RIGHT_TOP:	tox = w + bw; break;
		case WM_TAB_RIGHT:	tox = w + bw; toy = (h - chgt) / 2; break;
		case WM_TAB_RIGHT_BOTTOM: tox = w + bw; toy = h - chgt; break;
		}
		g->tw = cwid + 2 * bw;
		g->tth = chgt + 2 * bw;
	}

	/* The tree's origin is the corner of the frame and tabs together */
	g->fx = tox < 0 ? -tox : 0;
	g->fy = toy < 0 ? -toy : 0;
	g->tx = g->fx + tox;
	g->ty = g->fy + toy;
	g->total_w = g->fx + g->fw;
	g->total_h = g->fy + g->fh;
	if (g->ext_tabs) {
		if (g->tx + g->tw > g->total_w)
			g->total_w = g->tx + g->tw;
		if (g->ty + g->tth > g->total_h)
			g->total_h = g->ty + g->tth;
	}

	g->title_x = g->fx + g->bw;
	g->title_y = g->fy + g->bw;
	g->content_x = g->fx + g->bw;
	g->content_y = g->fy + g->bw + (g->titlebar ? g->th + g->bw : 0);
	g->handle_x = g->content_x;
	g->handle_y = g->content_y + g->ch + g->bw;
}

/* --- Rendering helpers --- */

static const enum wm_window_pixmap button_pictures[] = {
	[WM_BUTTON_CLOSE] = WM_PIXMAP_CLOSE,
	[WM_BUTTON_MAXIMIZE] = WM_PIXMAP_MAXIMIZE,
	[WM_BUTTON_ICONIFY] = WM_PIXMAP_ICONIFY,
	[WM_BUTTON_SHADE] = WM_PIXMAP_SHADE,
	[WM_BUTTON_STICK] = WM_PIXMAP_STICK,
	[WM_BUTTON_MENUICON] = WM_PIXMAP_MENUICON,
	[WM_BUTTON_LHALF] = WM_PIXMAP_LHALF,
	[WM_BUTTON_RHALF] = WM_PIXMAP_RHALF,
};

/* Stick shows stuck, Shade unshade, for a window that is */
static bool
button_toggled(struct wm_decoration *deco, enum wm_button_type type)
{
	if (type == WM_BUTTON_STICK)
		return deco->view && deco->view->sticky;
	if (type == WM_BUTTON_SHADE)
		return deco->shaded;
	return false;
}

/*
 * A titlebar button as Fluxbox's WinButton: the style's picture for it
 * (window.close.pixmap...) when there is one, else the button texture
 * (nothing for ParentRelative, so the titlebar shows) with the picture
 * drawn in picColor.
 */
static struct wlr_buffer *
render_button(struct wm_decoration *deco, const struct wm_style *style,
	struct wm_decor_button *btn, int size)
{
	bool toggled = button_toggled(deco, btn->type);
	enum wm_window_pixmap pic = button_pictures[btn->type];
	if (toggled && pic == WM_PIXMAP_STICK)
		pic = WM_PIXMAP_STUCK;
	else if (toggled && pic == WM_PIXMAP_SHADE)
		pic = WM_PIXMAP_UNSHADE;
	enum wm_pixmap_state state = btn->pressed ? WM_PIXMAP_PRESSED :
		deco->focused ? WM_PIXMAP_FOCUS : WM_PIXMAP_UNFOCUS;
	const char *path = style->window_pixmaps[pic][state];
	if (path) {
		cairo_surface_t *img = wm_load_pixmap(path, size, size);
		if (img)
			return wlr_buffer_from_cairo(img);
	}

	const struct wm_texture *tex = btn->pressed ?
		&style->window_button_pressed : deco->focused ?
		&style->window_button_focus : &style->window_button_unfocus;
	const struct wm_color *pic_color = deco->focused ?
		&style->window_button_focus_pic_color :
		&style->window_button_unfocus_pic_color;
	cairo_surface_t *surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size, size);
	if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(surface);
		return NULL;
	}
	cairo_t *cr = cairo_create(surface);
	cairo_surface_t *bg = wm_render_texture(tex, size, size, 1.0f);
	if (bg) {
		cairo_set_source_surface(cr, bg, 0, 0);
		cairo_paint(cr);
		cairo_surface_destroy(bg);
	}
	cairo_surface_t *glyph = wm_render_button_glyph(btn->type, toggled,
		pic_color, size, size);
	if (glyph) {
		cairo_set_source_surface(cr, glyph, 0, 0);
		cairo_paint(cr);
		cairo_surface_destroy(glyph);
	}
	cairo_destroy(cr);
	return wlr_buffer_from_cairo(surface);
}

/* Text in a box, justified, two pixels in from the sides */
static void
draw_label_text(cairo_t *cr, const char *text, const struct wm_font *font,
	const struct wm_color *color, enum wm_justify justify,
	int x, int y, int w, int h)
{
	if (!text || !*text || w <= 4)
		return;
	int tw, th;
	cairo_surface_t *ts = wm_render_text(text, font, color, w - 4,
		&tw, &th, justify, 1.0f);
	if (!ts)
		return;
	int tx = x + 2;
	if (justify == WM_JUSTIFY_CENTER)
		tx = x + (w - tw) / 2;
	else if (justify == WM_JUSTIFY_RIGHT)
		tx = x + w - tw - 2;
	int ty = y + (h - th) / 2;
	cairo_set_source_surface(cr, ts, tx, ty < y ? y : ty);
	cairo_paint(cr);
	cairo_surface_destroy(ts);
}

static void
fill_rect(cairo_t *cr, const struct wm_color *c, int x, int y, int w, int h)
{
	if (w <= 0 || h <= 0)
		return;
	cairo_set_source_rgba(cr, c->r / 255.0, c->g / 255.0, c->b / 255.0, 1.0);
	cairo_rectangle(cr, x, y, w, h);
	cairo_fill(cr);
}

static void
paint_texture(cairo_t *cr, const struct wm_texture *tex, int x, int y,
	int w, int h)
{
	if (w <= 0 || h <= 0)
		return;
	cairo_surface_t *s = wm_render_texture(tex, w, h, 1.0f);
	if (s) {
		cairo_set_source_surface(cr, s, x, y);
		cairo_paint(cr);
		cairo_surface_destroy(s);
	}
}

/* The views of a window's tabs in order, or just its own */
static int
tab_views(struct wm_decoration *deco, struct wm_view **views, int max)
{
	struct wm_tab_group *tg = deco->view ? deco->view->tab_group : NULL;
	int n = 0;
	if (tg && tg->count > 0) {
		struct wm_view *v;
		wl_list_for_each(v, &tg->views, tab_link) {
			if (n < max)
				views[n++] = v;
		}
	} else if (deco->view && max > 0) {
		views[n++] = deco->view;
	}
	return n;
}

static bool
tab_is_current(struct wm_decoration *deco, struct wm_view *v)
{
	struct wm_tab_group *tg = deco->view->tab_group;
	return tg ? v == tg->active_view : v == deco->view;
}

/*
 * A row of tabs as FbTk::Container lays out IconButtons: len long each
 * (the last ones a pixel longer when shared out), borders between them
 * shared and the outer ones outside the row.  The current tab of a
 * focused window is drawn as focused, the others unfocused.
 */
static void
draw_tab_row(cairo_t *cr, struct wm_decoration *deco,
	const struct wm_style *style, struct wm_view **views, int n,
	int len, int extra, int b, int row_h, bool reverse)
{
	for (int i = 0; i < n; i++) {
		struct wm_view *v = views[reverse ? n - 1 - i : i];
		bool f = deco->focused && tab_is_current(deco, v);
		const struct wm_color *bc = f ?
			&style->window_label_focus_border_color :
			&style->window_label_unfocus_border_color;
		int w = len + (i < extra ? 1 : 0);
		int x = i * (len + b) + (i < extra ? i : extra);
		fill_rect(cr, bc, x - b, -b, w + 2 * b, row_h + 2 * b);
		paint_texture(cr, f ? &style->window_label_focus :
			&style->window_label_unfocus, x, 0, w, row_h);
		draw_label_text(cr, v->title ? v->title : "",
			f ? &style->window_label_focus_font :
			&style->window_label_unfocus_font,
			f ? &style->window_label_focus_text_color :
			&style->window_label_unfocus_text_color,
			style->window_justify, x, 0, w, row_h);
	}
}

/* The titlebar's label: the tabs sharing it out (or the title alone
 * when the tabs are outside) */
static struct wlr_buffer *
render_label(struct wm_decoration *deco, const struct wm_style *style,
	const struct frame_geom *g)
{
	int w = g->label_w, h = g->button_size;
	cairo_surface_t *surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
	if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(surface);
		return NULL;
	}
	cairo_t *cr = cairo_create(surface);
	if (g->ext_tabs) {
		paint_texture(cr, deco->focused ? &style->window_label_focus :
			&style->window_label_unfocus, 0, 0, w, h);
		draw_label_text(cr, deco->view->title ? deco->view->title : "",
			deco->focused ? &style->window_label_focus_font :
			&style->window_label_unfocus_font,
			deco->focused ? &style->window_label_focus_text_color :
			&style->window_label_unfocus_text_color,
			style->window_justify, 0, 0, w, h);
	} else {
		struct wm_view *views[64];
		int n = tab_views(deco, views, 64);
		int b = style->window_label_unfocus_border_width;
		if (n > 0 && deco->focused && tab_is_current(deco, views[0]))
			b = style->window_label_focus_border_width;
		int len = (w - (n - 1) * b) / n;
		int extra = w - ((len + b) * n - b);
		draw_tab_row(cr, deco, style, views, n, len, extra, b, h,
			false);
	}
	cairo_destroy(cr);
	return wlr_buffer_from_cairo(surface);
}

/*
 * The external tabs: the container's border and background, then the
 * tabs; along a side the row is drawn turned, reading up on the left
 * and down on the right, the first tab at the top.
 */
static struct wlr_buffer *
render_ext_tabs(struct wm_decoration *deco, const struct wm_style *style,
	const struct frame_geom *g)
{
	int bw = g->bw;
	int len_total = (g->tabs_vertical ? g->tth : g->tw) - 2 * bw;
	int thick = (g->tabs_vertical ? g->tw : g->tth) - 2 * bw;
	if (len_total <= 0 || thick <= 0)
		return NULL;

	/* The row, laid out horizontally */
	cairo_surface_t *row = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, len_total, thick);
	cairo_t *rcr = cairo_create(row);
	const struct wm_texture *bg = deco->focused ?
		&style->window_label_focus : &style->window_label_unfocus;
	if (bg->fill == WM_TEX_PARENT_RELATIVE)
		bg = deco->focused ? &style->window_title_focus :
			&style->window_title_unfocus;
	paint_texture(rcr, bg, 0, 0, len_total, thick);
	struct wm_view *views[64];
	int n = tab_views(deco, views, 64);
	draw_tab_row(rcr, deco, style, views, n, g->tab_len, 0, g->tab_border,
		thick, g->tabs_left);
	cairo_destroy(rcr);

	cairo_surface_t *out = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, g->tw, g->tth);
	cairo_t *cr = cairo_create(out);
	const struct wm_color *bc = deco->focused ?
		&style->window_border_color : &style->window_border_unfocus_color;
	fill_rect(cr, bc, 0, 0, g->tw, bw);
	fill_rect(cr, bc, 0, g->tth - bw, g->tw, bw);
	fill_rect(cr, bc, 0, bw, bw, g->tth - 2 * bw);
	fill_rect(cr, bc, g->tw - bw, bw, bw, g->tth - 2 * bw);
	cairo_translate(cr, bw, bw);
	if (g->tabs_left) {
		/* A quarter turn back: the row's end at the top */
		cairo_translate(cr, 0, len_total);
		cairo_rotate(cr, -M_PI / 2);
	} else if (g->tabs_vertical) {
		cairo_translate(cr, thick, 0);
		cairo_rotate(cr, M_PI / 2);
	}
	cairo_set_source_surface(cr, row, 0, 0);
	cairo_paint(cr);
	cairo_destroy(cr);
	cairo_surface_destroy(row);
	return wlr_buffer_from_cairo(out);
}

/*
 * The handle row: grips at each end (FbWinFrame's are 20 wide), each
 * with its border line between it and the handle.
 */
static struct wlr_buffer *
render_handle(struct wm_decoration *deco, const struct wm_style *style,
	const struct frame_geom *g)
{
	int w = g->cw, h = g->hh, gw = g->gw, bw = g->bw;
	cairo_surface_t *surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
	if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(surface);
		return NULL;
	}
	cairo_t *cr = cairo_create(surface);
	const struct wm_texture *handle = deco->focused ?
		&style->window_handle_focus : &style->window_handle_unfocus;
	const struct wm_texture *grip = deco->focused ?
		&style->window_grip_focus : &style->window_grip_unfocus;
	const struct wm_color *bc = deco->focused ?
		&style->window_border_color : &style->window_border_unfocus_color;
	if (w <= 2 * (gw + bw)) {
		paint_texture(cr, handle, 0, 0, w, h);
	} else {
		paint_texture(cr, grip, 0, 0, gw, h);
		fill_rect(cr, bc, gw, 0, bw, h);
		paint_texture(cr, handle, gw + bw, 0, w - 2 * (gw + bw), h);
		fill_rect(cr, bc, w - gw - bw, 0, bw, h);
		paint_texture(cr, grip, w - gw, 0, gw, h);
	}
	cairo_destroy(cr);
	return wlr_buffer_from_cairo(surface);
}

/* --- Dimensions from style --- */

/* FbWinFrame::reconfigureTitlebar: window.title.height, else the font
 * and the bevel above and below and 2 */
static int
style_titlebar_height(const struct wm_style *style)
{
	if (style->window_title_height > 0)
		return style->window_title_height;
	int fh = wm_font_height(&style->window_font, 1.0f);
	if (fh <= 0)
		return 16;
	return fh + 2 * style->window_bevel_width + 2;
}

static int
style_handle_height(const struct wm_style *style)
{
	return style->window_handle_width > 0 ? style->window_handle_width : 0;
}

static int
style_border_width(const struct wm_style *style)
{
	return style->window_border_width > 0 ? style->window_border_width : 0;
}

/* --- Decoration create --- */

struct wm_decoration *
wm_decoration_create(struct wm_view *view, struct wm_style *style)
{
	struct wm_decoration *deco = calloc(1, sizeof(*deco));
	if (!deco) {
		return NULL;
	}

	deco->view = view;
	deco->server = view->server;
	deco->focused = true;
	deco->shaded = false;
	deco->preset = WM_DECOR_NORMAL;

	/* Compute dimensions from style */
	deco->titlebar_height = style_titlebar_height(style);
	deco->handle_height = style_handle_height(style);
	deco->border_width = style_border_width(style);
	deco->grip_width = DEFAULT_GRIP_WIDTH;

	/* Get client surface size */
	struct wlr_box geo;
	wm_view_content_geometry(view, &geo);
	deco->content_width = geo.width > 0 ? geo.width : 640;
	deco->content_height = geo.height > 0 ? geo.height : 480;

	/*
	 * Create decoration scene tree. This is placed as a child of the
	 * view's scene tree, BELOW the client surface. We'll use
	 * wlr_scene_node_lower_to_bottom to ensure borders/titlebar are
	 * painted behind the client surface in the correct stacking.
	 *
	 * But actually, decorations frame the client surface, so the
	 * decoration tree should be at the same level. The client surface
	 * scene tree is already a child of view->scene_tree. We create
	 * decorations also as children, and position them around the surface.
	 */
	deco->tree = wlr_scene_tree_create(view->scene_tree);
	if (!deco->tree) {
		free(deco);
		return NULL;
	}

	/* Put decoration tree below client surface in z-order */
	wlr_scene_node_lower_to_bottom(&deco->tree->node);

	/* Create border rects */
	float border_color[4];
	wm_color_to_float4(&style->window_border_color, border_color);

	deco->border_top = wlr_scene_rect_create(deco->tree, 1, 1,
		border_color);
	deco->border_bottom = wlr_scene_rect_create(deco->tree, 1, 1,
		border_color);
	deco->border_left = wlr_scene_rect_create(deco->tree, 1, 1,
		border_color);
	deco->border_right = wlr_scene_rect_create(deco->tree, 1, 1,
		border_color);
	if (!deco->border_top || !deco->border_bottom ||
		!deco->border_left || !deco->border_right) {
		wlr_log(WLR_ERROR, "%s", "Failed to create decoration border rects");
		wlr_scene_node_destroy(&deco->tree->node);
		free(deco);
		return NULL;
	}

	/* The border lines under the titlebar and over the handle */
	deco->sep_top = wlr_scene_rect_create(deco->tree, 1, 1, border_color);
	deco->sep_bottom = wlr_scene_rect_create(deco->tree, 1, 1,
		border_color);

	/* Rounded border frame (initially disabled) */
	deco->border_frame = wlr_scene_buffer_create(deco->tree, NULL);
	if (deco->border_frame) {
		wlr_scene_node_set_enabled(&deco->border_frame->node, false);
	}

	/* Focus border overlay (initially disabled) */
	deco->focus_border_buf = wlr_scene_buffer_create(deco->tree, NULL);
	if (deco->focus_border_buf) {
		wlr_scene_node_set_enabled(
			&deco->focus_border_buf->node, false);
	}

	/* Create titlebar tree */
	deco->titlebar_tree = wlr_scene_tree_create(deco->tree);
	if (!deco->titlebar_tree) {
		wlr_log(WLR_ERROR, "%s", "Failed to create titlebar scene tree");
		wlr_scene_node_destroy(&deco->tree->node);
		free(deco);
		return NULL;
	}

	/* Create scene buffer nodes (initially empty) */
	deco->title_bg = wlr_scene_buffer_create(deco->titlebar_tree, NULL);
	deco->label_buf = wlr_scene_buffer_create(deco->titlebar_tree, NULL);
	deco->handle_buf = wlr_scene_buffer_create(deco->tree, NULL);
	deco->grip_left = wlr_scene_buffer_create(deco->tree, NULL);
	deco->grip_right = wlr_scene_buffer_create(deco->tree, NULL);
	if (!deco->title_bg || !deco->label_buf || !deco->handle_buf ||
		!deco->grip_left || !deco->grip_right) {
		wlr_log(WLR_ERROR, "%s", "Failed to create decoration scene buffers");
		wlr_scene_node_destroy(&deco->tree->node);
		free(deco);
		return NULL;
	}

	/* External tab bar buffer (initially hidden) */
	deco->tab_bar_buf = wlr_scene_buffer_create(deco->tree, NULL);
	if (deco->tab_bar_buf) {
		wlr_scene_node_set_enabled(
			&deco->tab_bar_buf->node, false);
	}

	/* Button layout from config or defaults */
	const char *left_str = "Stick";
	const char *right_str = "Minimize Maximize Close";
	if (view->server->config) {
		if (view->server->config->titlebar_left)
			left_str = view->server->config->titlebar_left;
		if (view->server->config->titlebar_right)
			right_str = view->server->config->titlebar_right;
	}
	wm_decoration_configure_buttons(deco, left_str, right_str);

	/* Render initial state */
	wm_decoration_update(deco, style);

	return deco;
}

/* --- Decoration destroy --- */

void
wm_decoration_destroy(struct wm_decoration *decoration)
{
	if (!decoration) {
		return;
	}

	/* Scene nodes are destroyed when their parent tree is destroyed */
	if (decoration->tree) {
		wlr_scene_node_destroy(&decoration->tree->node);
	}

	free(decoration->buttons_left);
	free(decoration->buttons_right);
	free(decoration);
}

/* --- Rounded border frame rendering --- */

/*
 * Render a border frame with rounded corners as a Cairo surface.
 * Returns a wlr_buffer that can be used in a scene buffer node.
 */
static struct wlr_buffer *
render_rounded_border_frame(int outer_w, int outer_h, int border_width,
	double radius, uint8_t corners, const struct wm_color *color)
{
	if (outer_w <= 0 || outer_h <= 0 || border_width <= 0) {
		return NULL;
	}

	cairo_surface_t *surface = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, outer_w, outer_h);
	if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(surface);
		return NULL;
	}

	cairo_t *cr = cairo_create(surface);

	double r = color->r / 255.0;
	double g = color->g / 255.0;
	double b = color->b / 255.0;
	double a = color->a / 255.0;
	cairo_set_source_rgba(cr, r, g, b, a);

	/* Outer rounded rect */
	wm_render_rounded_rect_path(cr, 0, 0,
		outer_w, outer_h, radius, corners);

	/* Inner rect (punch out the inside) */
	int bw = border_width;
	cairo_new_sub_path(cr);
	cairo_rectangle(cr, bw, bw,
		outer_w - 2 * bw, outer_h - 2 * bw);

	/* Fill using even-odd rule so the inner rect is cut out */
	cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
	cairo_fill(cr);

	cairo_destroy(cr);
	return wlr_buffer_from_cairo(surface);
}

/* --- Layout and render --- */

static void
set_rect(struct wlr_scene_rect *r, const float color[4], int x, int y,
	int w, int h)
{
	if (!r)
		return;
	if (w <= 0 || h <= 0) {
		wlr_scene_node_set_enabled(&r->node, false);
		return;
	}
	wlr_scene_rect_set_size(r, w, h);
	wlr_scene_rect_set_color(r, color);
	if (r->node.x != x || r->node.y != y)
		wlr_scene_node_set_position(&r->node, x, y);
	wlr_scene_node_set_enabled(&r->node, true);
}

static void
set_buffer(struct wlr_scene_buffer *node, struct wlr_buffer *buf, int x, int y)
{
	if (!node) {
		if (buf)
			wlr_buffer_drop(buf);
		return;
	}
	wlr_scene_buffer_set_buffer(node, buf);
	if (buf)
		wlr_buffer_drop(buf);
	if (node->node.x != x || node->node.y != y)
		wlr_scene_node_set_position(&node->node, x, y);
	wlr_scene_node_set_enabled(&node->node, buf != NULL);
}

static void
render_borders(struct wm_decoration *deco, struct wm_style *style,
	const struct frame_geom *g)
{
	const struct wm_color *color = deco->focused ?
		&style->window_border_color : &style->window_border_unfocus_color;
	float c[4];
	wm_color_to_float4(color, c);
	int bw = g->bw;

	uint8_t corners = style->window_round_corners;
	bool rounded = corners != 0 && deco->border_frame && bw > 0;
	if (rounded) {
		double radius = style->window_bevel_width * 2.0;
		if (radius < 4.0)
			radius = 4.0;
		set_buffer(deco->border_frame, render_rounded_border_frame(
			g->fw, g->fh, bw, radius, corners, color), g->fx, g->fy);
	} else if (deco->border_frame) {
		wlr_scene_node_set_enabled(&deco->border_frame->node, false);
	}
	bool edges = bw > 0 && !rounded;
	set_rect(deco->border_top, c, g->fx, g->fy, edges ? g->fw : 0, bw);
	set_rect(deco->border_bottom, c, g->fx, g->fy + g->fh - bw,
		edges ? g->fw : 0, bw);
	set_rect(deco->border_left, c, g->fx, g->fy, edges ? bw : 0, g->fh);
	set_rect(deco->border_right, c, g->fx + g->fw - bw, g->fy,
		edges ? bw : 0, g->fh);

	/* The titlebar's and handle's border lines */
	set_rect(deco->sep_top, c, g->content_x, g->content_y - bw,
		g->titlebar && !deco->shaded ? g->cw : 0, bw);
	set_rect(deco->sep_bottom, c, g->handle_x, g->handle_y - bw,
		g->handle ? g->cw : 0, bw);
}

static void
render_titlebar(struct wm_decoration *deco, struct wm_style *style,
	const struct frame_geom *g)
{
	if (!g->titlebar || g->th <= 0) {
		wlr_scene_node_set_enabled(&deco->titlebar_tree->node, false);
		for (int i = 0; i < deco->buttons_left_count; i++)
			deco->buttons_left[i].box = (struct wlr_box){ 0 };
		for (int i = 0; i < deco->buttons_right_count; i++)
			deco->buttons_right[i].box = (struct wlr_box){ 0 };
		return;
	}
	wlr_scene_node_set_enabled(&deco->titlebar_tree->node, true);
	if (deco->titlebar_tree->node.x != g->title_x ||
	    deco->titlebar_tree->node.y != g->title_y)
		wlr_scene_node_set_position(&deco->titlebar_tree->node,
			g->title_x, g->title_y);

	set_buffer(deco->title_bg, wlr_buffer_from_cairo(wm_render_texture(
		deco->focused ? &style->window_title_focus :
		&style->window_title_unfocus, g->cw, g->th, 1.0f)), 0, 0);

	int bs = g->button_size;
	for (int side = 0; side < 2; side++) {
		struct wm_decor_button *buttons = side ?
			deco->buttons_right : deco->buttons_left;
		int count = side ? deco->buttons_right_count :
			deco->buttons_left_count;
		int x = side ? g->label_x + g->label_w + g->bevel : g->bevel;
		for (int i = 0; i < count; i++, x += bs + g->bevel) {
			struct wm_decor_button *btn = &buttons[i];
			struct wlr_buffer *buf = render_button(deco, style,
				btn, bs);
			if (!btn->node)
				btn->node = wlr_scene_buffer_create(
					deco->titlebar_tree, NULL);
			set_buffer(btn->node, buf, x, g->bevel);
			btn->box = (struct wlr_box){ g->title_x + x,
				g->title_y + g->bevel, bs, bs };
		}
	}

	set_buffer(deco->label_buf, render_label(deco, style, g), g->label_x,
		g->bevel);
}

static void
render_decoration_handle(struct wm_decoration *deco, struct wm_style *style,
	const struct frame_geom *g)
{
	if (deco->grip_left)
		wlr_scene_node_set_enabled(&deco->grip_left->node, false);
	if (deco->grip_right)
		wlr_scene_node_set_enabled(&deco->grip_right->node, false);
	if (!g->handle) {
		wlr_scene_node_set_enabled(&deco->handle_buf->node, false);
		return;
	}
	set_buffer(deco->handle_buf, render_handle(deco, style, g),
		g->handle_x, g->handle_y);
}

static void
render_decoration_ext_tabs(struct wm_decoration *deco,
	struct wm_style *style, const struct frame_geom *g)
{
	if (!deco->tab_bar_buf)
		return;
	if (!g->ext_tabs) {
		wlr_scene_node_set_enabled(&deco->tab_bar_buf->node, false);
		deco->tab_bar_box = (struct wlr_box){ 0 };
		return;
	}
	set_buffer(deco->tab_bar_buf, render_ext_tabs(deco, style, g),
		g->tx, g->ty);
	deco->tab_bar_box = (struct wlr_box){ g->tx, g->ty, g->tw, g->tth };
}

/* --- Focus border rendering --- */

static void
render_decoration_focus_border(struct wm_decoration *deco,
	struct wm_style *style, const struct frame_geom *g)
{
	if (!deco->focus_border_buf)
		return;
	int fbw = style->window_focus_border_width;
	if (!deco->focused || fbw <= 0 || !g->titlebar) {
		wlr_scene_node_set_enabled(&deco->focus_border_buf->node,
			false);
		return;
	}
	int w = g->fw + 2 * fbw, h = g->fh + 2 * fbw;
	cairo_surface_t *surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
	if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(surface);
		return;
	}
	cairo_t *cr = cairo_create(surface);
	const struct wm_color *c = &style->window_focus_border_color;
	cairo_set_source_rgba(cr, c->r / 255.0, c->g / 255.0, c->b / 255.0,
		c->a / 255.0);
	cairo_rectangle(cr, 0, 0, w, h);
	cairo_rectangle(cr, fbw, fbw, g->fw, g->fh);
	cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
	cairo_fill(cr);
	cairo_destroy(cr);
	set_buffer(deco->focus_border_buf, wlr_buffer_from_cairo(surface),
		g->fx - fbw, g->fy - fbw);
	wlr_scene_node_raise_to_top(&deco->focus_border_buf->node);
}

static void
layout_and_render(struct wm_decoration *deco, struct wm_style *style)
{
	struct frame_geom g;
	frame_geometry(deco, style, &g);
	deco->content_area = (struct wlr_box){ g.content_x, g.content_y,
		g.cw, g.ch };
	deco->tab_bar_size = g.ext_tabs ? 1 : 0;

	render_borders(deco, style, &g);
	render_titlebar(deco, style, &g);
	render_decoration_handle(deco, style, &g);
	render_decoration_ext_tabs(deco, style, &g);
	render_decoration_focus_border(deco, style, &g);
}

/* --- Public API --- */

void
wm_decoration_update(struct wm_decoration *decoration,
	struct wm_style *style)
{
	if (!decoration || !style) {
		return;
	}

#ifdef WM_PERF_ENABLE
	if (!perf_decoration_inited) {
		wm_perf_probe_init(&perf_decoration_update, "decoration_update");
		perf_decoration_inited = true;
	}
	WM_PERF_BEGIN(decor);
#endif

	decoration->titlebar_height = style_titlebar_height(style);
	decoration->handle_height = style_handle_height(style);
	decoration->border_width = style_border_width(style);

	layout_and_render(decoration, style);

#ifdef WM_PERF_ENABLE
	WM_PERF_END(decor, &perf_decoration_update);
#endif
}

void
wm_decoration_refresh_geometry(struct wm_decoration *decoration,
	struct wm_style *style)
{
	if (!decoration || !style) {
		return;
	}

	/* Skip configure during unmap/teardown */
	if (!decoration->view->scene_tree->node.enabled) {
		return;
	}

	struct wlr_box geo;
	wm_view_content_geometry(decoration->view, &geo);
	if (geo.width > 0 && geo.height > 0 &&
	    (geo.width != decoration->content_width ||
	     geo.height != decoration->content_height)) {
		decoration->content_width = geo.width;
		decoration->content_height = geo.height;
		layout_and_render(decoration, style);
	}
}

void
wm_decoration_set_focused(struct wm_decoration *decoration,
	bool focused, struct wm_style *style)
{
	if (!decoration || decoration->focused == focused) {
		return;
	}
	decoration->focused = focused;
	wm_decoration_update(decoration, style);
}

void
wm_decoration_configure_buttons(struct wm_decoration *decoration,
	const char *left_str, const char *right_str)
{
	if (!decoration) {
		return;
	}

	/* Free old buttons (but not their scene nodes, we'll recreate) */
	free(decoration->buttons_left);
	free(decoration->buttons_right);

	decoration->buttons_left = parse_button_layout(left_str,
		&decoration->buttons_left_count);
	decoration->buttons_right = parse_button_layout(right_str,
		&decoration->buttons_right_count);
}

struct wm_decor_button *
wm_decoration_button_at(struct wm_decoration *decoration, double x, double y)
{
	if (!decoration) {
		return NULL;
	}

	for (int i = 0; i < decoration->buttons_left_count; i++) {
		struct wm_decor_button *btn = &decoration->buttons_left[i];
		if (x >= btn->box.x && x < btn->box.x + btn->box.width &&
		    y >= btn->box.y && y < btn->box.y + btn->box.height) {
			return btn;
		}
	}

	for (int i = 0; i < decoration->buttons_right_count; i++) {
		struct wm_decor_button *btn = &decoration->buttons_right[i];
		if (x >= btn->box.x && x < btn->box.x + btn->box.width &&
		    y >= btn->box.y && y < btn->box.y + btn->box.height) {
			return btn;
		}
	}

	return NULL;
}

enum wm_decor_region
wm_decoration_region_at(struct wm_decoration *decoration, double x, double y)
{
	if (!decoration)
		return WM_DECOR_REGION_NONE;
	struct frame_geom g;
	frame_geometry(decoration, decoration->server ?
		decoration->server->style : NULL, &g);

	/* The external tabs drag the window, as its titlebar does */
	if (g.ext_tabs && x >= g.tx && x < g.tx + g.tw &&
	    y >= g.ty && y < g.ty + g.tth)
		return WM_DECOR_REGION_TITLEBAR;

	double fx = x - g.fx, fy = y - g.fy;
	if (fx < 0 || fy < 0 || fx >= g.fw || fy >= g.fh)
		return WM_DECOR_REGION_NONE;
	if (wm_decoration_button_at(decoration, x, y))
		return WM_DECOR_REGION_BUTTON;

	int bw = g.bw;
	if (fy < bw)
		return WM_DECOR_REGION_BORDER_TOP;
	if (fy >= g.fh - bw)
		return WM_DECOR_REGION_BORDER_BOTTOM;
	if (fx < bw)
		return WM_DECOR_REGION_BORDER_LEFT;
	if (fx >= g.fw - bw)
		return WM_DECOR_REGION_BORDER_RIGHT;

	/* The titlebar, with its border line under it */
	if (g.titlebar && y < g.content_y)
		return WM_DECOR_REGION_TITLEBAR;

	/* The handle row and the border line over it */
	if (g.handle && y >= g.handle_y - bw) {
		double hx = x - g.handle_x;
		if (hx < g.gw + bw)
			return WM_DECOR_REGION_GRIP_LEFT;
		if (hx >= g.cw - g.gw - bw)
			return WM_DECOR_REGION_GRIP_RIGHT;
		return WM_DECOR_REGION_HANDLE;
	}
	return WM_DECOR_REGION_CLIENT;
}

int
wm_decoration_tab_at(struct wm_decoration *decoration, double x, double y)
{
	if (!decoration || !decoration->view || !decoration->view->tab_group ||
	    decoration->view->tab_group->count <= 0)
		return -1;
	struct wm_style *style = decoration->server ?
		decoration->server->style : NULL;
	struct frame_geom g;
	frame_geometry(decoration, style, &g);
	int n = decoration->view->tab_group->count;

	double along, len_total;
	int len, extra = 0, b;
	if (g.ext_tabs) {
		if (x < g.tx + g.bw || x >= g.tx + g.tw - g.bw ||
		    y < g.ty + g.bw || y >= g.ty + g.tth - g.bw)
			return -1;
		along = g.tabs_vertical ? y - g.ty - g.bw : x - g.tx - g.bw;
		len_total = (g.tabs_vertical ? g.tth : g.tw) - 2 * g.bw;
		len = g.tab_len;
		b = g.tab_border;
	} else {
		if (!g.titlebar)
			return -1;
		double lx = x - g.title_x - g.label_x;
		double ly = y - g.title_y - g.bevel;
		if (lx < 0 || lx >= g.label_w || ly < 0 || ly >= g.button_size)
			return -1;
		along = lx;
		len_total = g.label_w;
		b = style ? style->window_label_unfocus_border_width : 0;
		len = (g.label_w - (n - 1) * b) / n;
		extra = g.label_w - ((len + b) * n - b);
	}
	if (len < 1)
		return -1;
	/* On the left side the row is turned a quarter back with the tabs
	 * in reverse (render_ext_tabs): its end is at the top */
	bool turned = g.ext_tabs && g.tabs_left;
	double pos = turned ? len_total - 1 - along : along;
	int idx = 0;
	for (int i = 1; i < n; i++) {
		int start = i * (len + b) + (i < extra ? i : extra);
		if (pos >= start - b / 2.0)
			idx = i;
	}
	if (turned)
		idx = n - 1 - idx;
	return idx;
}

void
wm_decoration_set_pressed(struct wm_decoration *decoration,
	struct wm_decor_button *button, struct wm_style *style)
{
	if (!decoration)
		return;
	bool changed = false;
	for (int side = 0; side < 2; side++) {
		struct wm_decor_button *b = side ? decoration->buttons_right :
			decoration->buttons_left;
		int n = side ? decoration->buttons_right_count :
			decoration->buttons_left_count;
		for (int i = 0; i < n; i++) {
			bool p = &b[i] == button;
			changed |= b[i].pressed != p;
			b[i].pressed = p;
		}
	}
	if (changed && style)
		layout_and_render(decoration, style);
}

void
wm_decoration_set_shaded(struct wm_decoration *decoration,
	bool shaded, struct wm_style *style)
{
	if (!decoration || decoration->shaded == shaded) {
		return;
	}

	decoration->shaded = shaded;

	if (shaded) {
		/* Save current geometry */
		decoration->shaded_saved_geometry.width =
			decoration->content_width;
		decoration->shaded_saved_geometry.height =
			decoration->content_height;

		/*
		 * Set content height to 0 and re-render decorations.
		 * layout_and_render() checks the shaded flag to suppress
		 * the handle, so borders wrap tightly around the titlebar.
		 */
		decoration->content_height = 0;
		wm_decoration_update(decoration, style);
	} else {
		/* Restore geometry */
		decoration->content_width =
			decoration->shaded_saved_geometry.width;
		decoration->content_height =
			decoration->shaded_saved_geometry.height;
		wm_decoration_update(decoration, style);
	}

	/*
	 * Hide/show the client surface content. The client surface is a
	 * child of view->scene_tree that is NOT the decoration tree.
	 */
	struct wm_view *view = decoration->view;
	if (view && view->scene_tree) {
		struct wlr_scene_node *child;
		wl_list_for_each(child, &view->scene_tree->children, link) {
			if (child->type == WLR_SCENE_NODE_TREE) {
				struct wlr_scene_tree *subtree =
					wlr_scene_tree_from_node(child);
				if (subtree != decoration->tree) {
					wlr_scene_node_set_enabled(child,
						!shaded);
				}
			}
		}
	}
}

void
wm_decoration_set_preset(struct wm_decoration *decoration,
	enum wm_decor_preset preset, struct wm_style *style)
{
	if (!decoration || decoration->preset == preset) {
		return;
	}
	decoration->preset = preset;
	wm_decoration_update(decoration, style);
}

void
wm_decoration_get_extents(struct wm_decoration *decoration,
	int *top, int *bottom, int *left, int *right)
{
	if (!decoration) {
		if (top) *top = 0;
		if (bottom) *bottom = 0;
		if (left) *left = 0;
		if (right) *right = 0;
		return;
	}

	struct frame_geom g;
	frame_geometry(decoration, decoration->server ?
		decoration->server->style : NULL, &g);
	if (top) *top = g.content_y;
	if (bottom) *bottom = g.total_h - g.content_y - g.ch;
	if (left) *left = g.content_x;
	if (right) *right = g.total_w - g.content_x - g.cw;
}

/* --- xdg-decoration protocol --- */

static void
handle_xdg_decoration_destroy(struct wl_listener *listener, void *data)
{
	struct wm_xdg_decoration *deco =
		wl_container_of(listener, deco, destroy);
	wl_list_remove(&deco->destroy.link);
	wl_list_remove(&deco->request_mode.link);
	wl_list_remove(&deco->surface_commit.link);
	free(deco);
}

/*
 * Always server-side. The mode goes out with a configure, which an
 * xdg_surface may only get from its initial commit on.
 */
static void
xdg_decoration_send_mode(struct wm_xdg_decoration *deco)
{
	if (!deco->wlr_decoration->toplevel->base->initialized)
		return;
	wlr_xdg_toplevel_decoration_v1_set_mode(deco->wlr_decoration,
		WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
}

static void
handle_xdg_decoration_request_mode(struct wl_listener *listener, void *data)
{
	struct wm_xdg_decoration *deco =
		wl_container_of(listener, deco, request_mode);
	xdg_decoration_send_mode(deco);
}

static void
handle_xdg_decoration_surface_commit(struct wl_listener *listener,
	void *data)
{
	struct wm_xdg_decoration *deco =
		wl_container_of(listener, deco, surface_commit);
	if (!deco->wlr_decoration->toplevel->base->initial_commit)
		return;
	xdg_decoration_send_mode(deco);
	wl_list_remove(&deco->surface_commit.link);
	wl_list_init(&deco->surface_commit.link);
}

static void
handle_new_xdg_decoration(struct wl_listener *listener, void *data)
{
	struct wm_server *server =
		wl_container_of(listener, server, new_xdg_decoration);
	struct wlr_xdg_toplevel_decoration_v1 *wlr_deco = data;

	struct wm_xdg_decoration *deco = calloc(1, sizeof(*deco));
	if (!deco) {
		return;
	}

	deco->wlr_decoration = wlr_deco;
	deco->server = server;

	deco->destroy.notify = handle_xdg_decoration_destroy;
	wl_signal_add(&wlr_deco->events.destroy, &deco->destroy);

	deco->request_mode.notify = handle_xdg_decoration_request_mode;
	wl_signal_add(&wlr_deco->events.request_mode, &deco->request_mode);

	/* Before the initial commit, the mode waits for it */
	deco->surface_commit.notify = handle_xdg_decoration_surface_commit;
	if (wlr_deco->toplevel->base->initialized)
		wl_list_init(&deco->surface_commit.link);
	else
		wl_signal_add(&wlr_deco->toplevel->base->surface->events.commit,
			&deco->surface_commit);
	xdg_decoration_send_mode(deco);
}

void
wm_xdg_decoration_init(struct wm_server *server)
{
	server->xdg_decoration_mgr =
		wlr_xdg_decoration_manager_v1_create(server->wl_display);
	if (!server->xdg_decoration_mgr) {
		wlr_log(WLR_ERROR, "%s", "failed to create xdg-decoration manager");
		return;
	}

	server->new_xdg_decoration.notify = handle_new_xdg_decoration;
	wl_signal_add(
		&server->xdg_decoration_mgr->events.new_toplevel_decoration,
		&server->new_xdg_decoration);

	/*
	 * KDE's older server-decoration protocol, which GTK3 and Qt check:
	 * tell them the compositor draws the frame.
	 */
	server->kde_decoration_mgr =
		wlr_server_decoration_manager_create(server->wl_display);
	if (server->kde_decoration_mgr)
		wlr_server_decoration_manager_set_default_mode(
			server->kde_decoration_mgr,
			WLR_SERVER_DECORATION_MANAGER_MODE_SERVER);
}

void
wm_xdg_decoration_finish(struct wm_server *server)
{
	if (server->xdg_decoration_mgr)
		wl_list_remove(&server->new_xdg_decoration.link);
}
