/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * menu_render.c - Menu Cairo/Pango rendering
 *
 * Handles layout computation, title rendering, item rendering with
 * icons, submenu bullets (triangle/square/diamond), RTL text support,
 * and the Cairo-to-wlr_buffer bridge.
 */

#define _GNU_SOURCE
#include <cairo.h>
#include <drm_fourcc.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_scene.h>
#include <pango/pango.h>

#include "menu.h"
#include "menu_render.h"
#include "perf.h"
#include "pixel_buffer.h"
#include "render.h"
#include "server.h"
#include "style.h"

#ifdef WM_PERF_ENABLE
static struct wm_perf_probe perf_menu_render;
static bool perf_menu_render_inited;
#endif

/* --- Color helpers --- */

void
wm_color_to_float4(const struct wm_color *c, float out[4])
{
	out[0] = c->r / 255.0f;
	out[1] = c->g / 255.0f;
	out[2] = c->b / 255.0f;
	out[3] = c->a / 255.0f;
}

/* --- Layout computation --- */

static bool
item_has_submenu(const struct wm_menu_item *item)
{
	return item->type == WM_MENU_SUBMENU || item->type == WM_MENU_SENDTO ||
		item->type == WM_MENU_LAYER || item->type == WM_MENU_WORKSPACES ||
		item->type == WM_MENU_CONFIG;
}

/*
 * As Fluxbox's MenuTheme and Menu::updateMenu: items as high as the
 * frame and hilite fonts and a bevel above and below (menu.itemHeight
 * at least), the title likewise; as wide as the widest label with room
 * for the bullet and a bevel on each side (and the icon); a border
 * line between the title and the items; separators an item high.
 */
void
menu_compute_layout(struct wm_menu *menu, struct wm_style *style)
{
	int bevel = style->menu_bevel_width;
	int pad = 2 * bevel;
	int fh = wm_font_height(&style->menu_frame_font, 1.0f);
	int hh = wm_font_height(&style->menu_hilite_font, 1.0f);
	int ih = fh > hh ? fh : hh;
	ih += pad;
	if (ih < style->menu_item_height)
		ih = style->menu_item_height;
	if (ih < pad + 1)
		ih = pad + 1;
	int th = wm_font_height(&style->menu_title_font, 1.0f) + pad;
	if (th < style->menu_title_height)
		th = style->menu_title_height;
	if (th < pad + 1)
		th = pad + 1;
	menu->item_height = ih;
	menu->separator_height = ih;
	menu->title_height = th;
	int bw = menu->border_width = style->menu_border_width;

	int item_w = 1;
	if (menu->title)
		item_w = wm_measure_text_width(menu->title,
			&style->menu_title_font, 1.0f) + pad;
	int count = 0;
	struct wm_menu_item *item;
	wl_list_for_each(item, &menu->items, link) {
		count++;
		if (!item->label)
			continue;
		int tw = wm_measure_text_width(item->label,
			&style->menu_frame_font, 1.0f);
		int hw = wm_measure_text_width(item->label,
			&style->menu_hilite_font, 1.0f);
		int w = 2 * (bevel + ih) + (tw > hw ? tw : hw);
		if (item->icon_path)
			w += ih;
		if (w > item_w)
			item_w = w;
	}
	menu->item_count = count;
	menu->width = item_w + 2 * bw;
	menu->items_y = bw + th + bw;
	menu->height = menu->items_y + count * ih + bw;
	if (count == 0)
		menu->height = menu->items_y + 1 + bw;
}

/* --- Title rendering --- */

/* Menu::redrawTitle: the title texture and its text, a bevel in */
cairo_surface_t *
render_menu_title(struct wm_menu *menu, struct wm_style *style)
{
	int w = menu->width - 2 * menu->border_width;
	int th = menu->title_height;
	if (w <= 0 || th <= 0)
		return NULL;
	cairo_surface_t *surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, th);
	if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(surface);
		return NULL;
	}
	cairo_t *cr = cairo_create(surface);
	cairo_surface_t *bg = wm_render_texture(&style->menu_title, w, th,
		1.0f);
	if (bg) {
		cairo_set_source_surface(cr, bg, 0, 0);
		cairo_paint(cr);
		cairo_surface_destroy(bg);
	}
	if (menu->title && *menu->title) {
		int bevel = style->menu_bevel_width;
		int tw, tth;
		cairo_surface_t *text = wm_render_text(menu->title,
			&style->menu_title_font, &style->menu_title_text_color,
			w - 2 * bevel, &tw, &tth, style->menu_title_justify,
			1.0f);
		if (text) {
			int tx = bevel;
			if (style->menu_title_justify == WM_JUSTIFY_CENTER)
				tx = (w - tw) / 2;
			else if (style->menu_title_justify == WM_JUSTIFY_RIGHT)
				tx = w - tw - bevel;
			int ty = (th - tth) / 2;
			cairo_set_source_surface(cr, text, tx, ty < 0 ? 0 : ty);
			cairo_paint(cr);
			cairo_surface_destroy(text);
		}
	}
	cairo_destroy(cr);
	return surface;
}

/* --- Items rendering --- */

static void
set_color(cairo_t *cr, const struct wm_color *c)
{
	cairo_set_source_rgba(cr, c->r / 255.0, c->g / 255.0, c->b / 255.0,
		1.0);
}

/* A style picture for a bullet or toggle, centred in an item's square */
static bool
draw_item_pixmap(cairo_t *cr, const char *path, int x, int y, int size)
{
	if (!path)
		return false;
	cairo_surface_t *img = wm_load_image(path);
	if (!img)
		return false;
	int w = cairo_image_surface_get_width(img);
	int h = cairo_image_surface_get_height(img);
	if (w > size || h > size) {
		cairo_surface_destroy(img);
		img = wm_load_pixmap(path, size, size);
		if (!img)
			return false;
		w = h = size;
	}
	cairo_set_source_surface(cr, img, x + (w < size ? (size - w) / 2 : 0),
		y + (h < size ? (size - h) / 2 : 0));
	cairo_paint(cr);
	cairo_surface_destroy(img);
	return true;
}

/*
 * MenuItem::draw's submenu bullet in an item-high square at sel_x:
 * the style's picture, or a 33% triangle, a square outline or a small
 * diamond in the text colour.
 */
static void
draw_bullet(cairo_t *cr, struct wm_style *style, bool hilite, int sel_x,
	int y, int ih, const struct wm_color *color)
{
	const char *pic = hilite && style->menu_hilite_submenu_pixmap ?
		style->menu_hilite_submenu_pixmap : style->menu_submenu_pixmap;
	if (draw_item_pixmap(cr, pic, sel_x, y, ih))
		return;
	const char *bullet = style->menu_bullet ? style->menu_bullet : "empty";
	int half = ih / 2, quarter = ih / 4;
	set_color(cr, color);
	if (strcmp(bullet, "square") == 0) {
		cairo_set_line_width(cr, 1.0);
		cairo_rectangle(cr, sel_x + quarter + 0.5, y + quarter + 0.5,
			half, half);
		cairo_stroke(cr);
	} else if (strcmp(bullet, "triangle") == 0) {
		/* drawTriangle at scale 300: a third of the square */
		int ax = ih * 100 / 300, ay = ih * 100 / 300;
		ax -= ax % 2;
		ay -= ay % 2;
		double cx = sel_x + ih / 2, cy = y + ih / 2;
		if (style->menu_bullet_position == WM_JUSTIFY_RIGHT) {
			cairo_move_to(cr, cx + ax / 2, cy);
			cairo_line_to(cr, cx - ax / 2, cy + ay / 2);
			cairo_line_to(cr, cx - ax / 2, cy - ay / 2);
		} else {
			cairo_move_to(cr, cx - ax / 2, cy);
			cairo_line_to(cr, cx + ax / 2, cy - ay / 2);
			cairo_line_to(cr, cx + ax / 2, cy + ay / 2);
		}
		cairo_close_path(cr);
		cairo_fill(cr);
	} else if (strcmp(bullet, "diamond") == 0) {
		double cx = sel_x + half, cy = y + half;
		cairo_move_to(cr, cx - 3, cy);
		cairo_line_to(cr, cx, cy - 3);
		cairo_line_to(cr, cx + 3, cy);
		cairo_line_to(cr, cx, cy + 3);
		cairo_close_path(cr);
		cairo_fill(cr);
	}
}

cairo_surface_t *
render_menu_items(struct wm_menu *menu, struct wm_style *style)
{
	int w = menu->width - 2 * menu->border_width;
	int ih = menu->item_height;
	int total_h = menu->item_count * ih;
	if (w <= 0 || total_h <= 0)
		return NULL;
	cairo_surface_t *surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, total_h);
	if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(surface);
		return NULL;
	}
	cairo_t *cr = cairo_create(surface);
	cairo_surface_t *bg = wm_render_texture(&style->menu_frame, w, total_h,
		1.0f);
	if (bg) {
		cairo_set_source_surface(cr, bg, 0, 0);
		cairo_paint(cr);
		cairo_surface_destroy(bg);
	}

	int bevel = style->menu_bevel_width;
	bool right = style->menu_bullet_position == WM_JUSTIFY_RIGHT;
	int y = 0, index = 0;
	struct wm_menu_item *item;
	wl_list_for_each(item, &menu->items, link) {
		if (item->type == WM_MENU_SEPARATOR) {
			/* MenuSeparator::draw: a line in the text colour */
			set_color(cr, &style->menu_frame_text_color);
			cairo_rectangle(cr, bevel + ih + 1, y + ih / 2,
				w - 2 * (bevel + ih) - 1, 1);
			cairo_fill(cr);
			y += ih;
			index++;
			continue;
		}

		bool hilite = index == menu->selected_index;
		bool disabled = item->type == WM_MENU_NOP;
		if (hilite) {
			/* ParentRelative leaves the frame showing */
			cairo_surface_t *hs = wm_render_texture(
				&style->menu_hilite, w, ih, 1.0f);
			if (hs) {
				cairo_set_source_surface(cr, hs, 0, y);
				cairo_paint(cr);
				cairo_surface_destroy(hs);
			}
		}
		const struct wm_color *color = hilite ?
			&style->menu_hilite_text_color : disabled ?
			&style->menu_disable_text_color :
			&style->menu_frame_text_color;
		const struct wm_font *font = hilite ?
			&style->menu_hilite_font : &style->menu_frame_font;
		enum wm_justify justify = hilite ? style->menu_hilite_justify :
			style->menu_frame_justify;

		/* The icon, a bevel in */
		int icon_w = 0;
		if (item->icon_path && ih - 2 * bevel > 0) {
			cairo_surface_t *icon = wm_load_pixmap(item->icon_path,
				ih - 2 * bevel, ih - 2 * bevel);
			if (icon) {
				cairo_set_source_surface(cr, icon, bevel,
					y + bevel);
				cairo_paint(cr);
				cairo_surface_destroy(icon);
				icon_w = ih;
			}
		}

		if (item->label && *item->label) {
			/* RTL labels go to the right */
			if (find_base_dir(item->label) == PANGO_DIRECTION_RTL &&
			    justify == WM_JUSTIFY_LEFT)
				justify = WM_JUSTIFY_RIGHT;
			int tw, th;
			cairo_surface_t *text = wm_render_text(item->label,
				font, color, w - 2 * (bevel + ih) - icon_w,
				&tw, &th, justify, 1.0f);
			if (text) {
				int tx;
				if (justify == WM_JUSTIFY_RIGHT)
					tx = w - (ih + bevel + tw);
				else if (justify == WM_JUSTIFY_CENTER)
					tx = (w + 1 - tw) / 2;
				else
					tx = bevel + ih + 1 + icon_w;
				int ty = y + (ih - th) / 2;
				cairo_set_source_surface(cr, text, tx,
					ty < y ? y : ty);
				cairo_paint(cr);
				cairo_surface_destroy(text);
			}
		}

		/* Toggles and bullets in the item-high square on the
		 * bullet's side */
		int sel_x = right ? w - ih - bevel : 0;
		if (item->checked) {
			const char *pic = hilite &&
				style->menu_hilite_selected_pixmap ?
				style->menu_hilite_selected_pixmap :
				style->menu_selected_pixmap;
			if (!draw_item_pixmap(cr, pic, sel_x, y, ih)) {
				/* A square of the hilite texture's colour */
				uint32_t c = style->menu_hilite.color;
				struct wm_color hc = { (c >> 16) & 0xFF,
					(c >> 8) & 0xFF, c & 0xFF, 255 };
				set_color(cr, &hc);
				cairo_rectangle(cr, sel_x + ih / 4, y + ih / 4,
					ih / 2, ih / 2);
				cairo_fill(cr);
			}
		} else if (item->toggle) {
			const char *pic = hilite &&
				style->menu_hilite_unselected_pixmap ?
				style->menu_hilite_unselected_pixmap :
				style->menu_unselected_pixmap;
			draw_item_pixmap(cr, pic, sel_x, y, ih);
		}
		if (item_has_submenu(item))
			draw_bullet(cr, style, hilite, sel_x, y, ih, color);

		y += ih;
		index++;
	}
	cairo_destroy(cr);
	return surface;
}

/* --- Re-render items for selection change --- */

void
menu_update_render(struct wm_menu *menu)
{
	if (!menu || !menu->visible || !menu->scene_tree) {
		return;
	}

#ifdef WM_PERF_ENABLE
	if (!perf_menu_render_inited) {
		wm_perf_probe_init(&perf_menu_render, "menu_render");
		perf_menu_render_inited = true;
	}
	WM_PERF_BEGIN(mrender);
#endif

	struct wm_style *style = menu->server->style;

	cairo_surface_t *items_surf = render_menu_items(menu, style);
	struct wlr_buffer *items_buf = wlr_buffer_from_cairo(items_surf);
	if (menu->items_buf) {
		wlr_scene_buffer_set_buffer(menu->items_buf, items_buf);
	}
	if (items_buf) {
		wlr_buffer_drop(items_buf);
	}

#ifdef WM_PERF_ENABLE
	WM_PERF_END(mrender, &perf_menu_render);
#endif
}
