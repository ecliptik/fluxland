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
#include "render.h"
#include "server.h"
#include "style.h"

#ifdef WM_PERF_ENABLE
static struct wm_perf_probe perf_menu_render;
static bool perf_menu_render_inited;
#endif

/*
 * Determine the base text direction by scanning for the first character
 * with a strong directionality.  Uses pango_unichar_direction() which is
 * deprecated in Pango >= 1.44 but has no simple replacement without FriBidi.
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
static PangoDirection
find_base_dir(const char *text)
{
	if (!text)
		return PANGO_DIRECTION_NEUTRAL;
	const char *p = text;
	while (*p) {
		gunichar ch = g_utf8_get_char(p);
		PangoDirection dir = pango_unichar_direction(ch);
		if (dir == PANGO_DIRECTION_LTR ||
		    dir == PANGO_DIRECTION_RTL)
			return dir;
		p = g_utf8_next_char(p);
	}
	return PANGO_DIRECTION_NEUTRAL;
}
#pragma GCC diagnostic pop

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

struct wlr_buffer *
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

void
menu_compute_layout(struct wm_menu *menu, struct wm_style *style)
{
	const struct wm_font *title_font = &style->menu_title_font;
	const struct wm_font *frame_font = &style->menu_frame_font;

	int title_h;
	if (style->menu_title_height > 0) {
		title_h = style->menu_title_height;
	} else {
		title_h = title_font->size > 0 ? title_font->size : 12;
		title_h += 2 * MENU_TITLE_PADDING;
	}
	menu->title_height = title_h;

	int item_h;
	if (style->menu_item_height > 0) {
		item_h = style->menu_item_height;
	} else {
		item_h = frame_font->size > 0 ? frame_font->size : 12;
		item_h += 2 * MENU_ITEM_PADDING;
	}
	menu->item_height = item_h;

	menu->border_width = style->menu_border_width > 0 ?
		style->menu_border_width : 1;

	/* Compute width using Pango text measurements */
	int min_inner_w = MENU_MIN_WIDTH + 2 * MENU_ITEM_PADDING;
	int count = 0;

	/* Pre-scan: determine icon column width if any item has an icon */
	int icon_col_w = 0;
	struct wm_menu_item *item;
	wl_list_for_each(item, &menu->items, link) {
		if (item->icon_path) {
			int icon_size = item_h - 2 * MENU_ITEM_PADDING;
			if (icon_size < 1) icon_size = 1;
			icon_col_w = icon_size + MENU_ITEM_PADDING;
			break;
		}
	}

	/* Account for the title text width */
	if (menu->title) {
		int title_w = wm_measure_text_width(menu->title,
			title_font, 1.0f);
		int needed = title_w + 2 * MENU_TITLE_PADDING;
		if (needed > min_inner_w) {
			min_inner_w = needed;
		}
	}

	wl_list_for_each(item, &menu->items, link) {
		count++;
		if (item->label) {
			int text_w = wm_measure_text_width(item->label,
				frame_font, 1.0f);
			/* Add space for submenu arrow */
			if (item->type == WM_MENU_SUBMENU ||
			    item->type == WM_MENU_SENDTO ||
			    item->type == WM_MENU_LAYER ||
			    item->type == WM_MENU_WORKSPACES ||
			    item->type == WM_MENU_CONFIG) {
				text_w += MENU_ARROW_SIZE + 4;
			}
			int needed = text_w + 2 * MENU_ITEM_PADDING +
				icon_col_w;
			if (needed > min_inner_w) {
				min_inner_w = needed;
			}
		}
	}
	menu->item_count = count;

	int bw = menu->border_width;
	menu->width = min_inner_w + 2 * bw;
	if (menu->width > MENU_MAX_WIDTH) {
		menu->width = MENU_MAX_WIDTH;
	}

	/* Total height: border + title + border + items + border */
	int items_height = 0;
	wl_list_for_each(item, &menu->items, link) {
		if (item->type == WM_MENU_SEPARATOR) {
			items_height += MENU_SEPARATOR_HEIGHT;
		} else {
			items_height += item_h;
		}
	}

	menu->height = 2 * bw + title_h + items_height;
}

/* --- Title rendering --- */

cairo_surface_t *
render_menu_title(struct wm_menu *menu, struct wm_style *style)
{
	int bw = menu->border_width;
	int inner_w = menu->width - 2 * bw;
	int th = menu->title_height;

	if (inner_w <= 0 || th <= 0) {
		return NULL;
	}

	/* Render title background texture */
	cairo_surface_t *bg = wm_render_texture(&style->menu_title,
		inner_w, th, 1.0f);
	if (!bg) {
		bg = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
			inner_w, th);
		if (cairo_surface_status(bg) != CAIRO_STATUS_SUCCESS) {
			cairo_surface_destroy(bg);
			return NULL;
		}
		cairo_t *cr = cairo_create(bg);
		cairo_set_source_rgba(cr, 0.2, 0.2, 0.3, 1.0);
		cairo_paint(cr);
		cairo_destroy(cr);
	}

	/* Render title text */
	if (menu->title && *menu->title) {
		int tw, tth;
		int max_text_w = inner_w - 2 * MENU_TITLE_PADDING;
		cairo_surface_t *text = wm_render_text(menu->title,
			&style->menu_title_font,
			&style->menu_title_text_color,
			max_text_w,
			&tw, &tth, style->menu_title_justify, 1.0f);
		if (text) {
			cairo_t *cr = cairo_create(bg);
			int tx = MENU_TITLE_PADDING;
			int ty = (th - tth) / 2;
			if (style->menu_title_justify == WM_JUSTIFY_CENTER) {
				tx = (inner_w - tw) / 2;
			} else if (style->menu_title_justify ==
				   WM_JUSTIFY_RIGHT) {
				tx = inner_w - tw - MENU_TITLE_PADDING;
			}
			if (ty < 0) {
				ty = 0;
			}
			cairo_set_source_surface(cr, text, tx, ty);
			cairo_paint(cr);
			cairo_destroy(cr);
			cairo_surface_destroy(text);
		}
	}

	return bg;
}

/* --- Items rendering --- */

cairo_surface_t *
render_menu_items(struct wm_menu *menu, struct wm_style *style)
{
	int bw = menu->border_width;
	int inner_w = menu->width - 2 * bw;
	int item_h = menu->item_height;

	/* Compute total items height */
	int total_h = 0;
	struct wm_menu_item *item;
	wl_list_for_each(item, &menu->items, link) {
		if (item->type == WM_MENU_SEPARATOR) {
			total_h += MENU_SEPARATOR_HEIGHT;
		} else {
			total_h += item_h;
		}
	}

	if (inner_w <= 0 || total_h <= 0) {
		return NULL;
	}

	/* Render frame background */
	cairo_surface_t *surface = wm_render_texture(&style->menu_frame,
		inner_w, total_h, 1.0f);
	if (!surface) {
		surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
			inner_w, total_h);
		if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
			cairo_surface_destroy(surface);
			return NULL;
		}
		cairo_t *cr = cairo_create(surface);
		cairo_set_source_rgba(cr, 0.15, 0.15, 0.15, 1.0);
		cairo_paint(cr);
		cairo_destroy(cr);
	}

	cairo_t *cr = cairo_create(surface);
	int y = 0;
	int index = 0;

	/* Pre-scan: determine icon column width if any item has an icon */
	int icon_col_w = 0;
	wl_list_for_each(item, &menu->items, link) {
		if (item->icon_path) {
			int icon_size = item_h - 2 * MENU_ITEM_PADDING;
			if (icon_size < 1) icon_size = 1;
			icon_col_w = icon_size + MENU_ITEM_PADDING;
			break;
		}
	}

	wl_list_for_each(item, &menu->items, link) {
		if (item->type == WM_MENU_SEPARATOR) {
			/* Draw separator line */
			cairo_set_source_rgba(cr,
				style->menu_border_color.r / 255.0,
				style->menu_border_color.g / 255.0,
				style->menu_border_color.b / 255.0,
				0.6);
			cairo_set_line_width(cr, 1.0);
			int sep_y = y + MENU_SEPARATOR_HEIGHT / 2;
			cairo_move_to(cr, 4, sep_y + 0.5);
			cairo_line_to(cr, inner_w - 4, sep_y + 0.5);
			cairo_stroke(cr);
			y += MENU_SEPARATOR_HEIGHT;
			index++;
			continue;
		}

		bool is_selected = (index == menu->selected_index);

		/* Draw highlight background for selected item */
		if (is_selected) {
			cairo_surface_t *hilite = wm_render_texture(
				&style->menu_hilite, inner_w, item_h, 1.0f);
			if (hilite) {
				cairo_set_source_surface(cr, hilite, 0, y);
				cairo_paint(cr);
				cairo_surface_destroy(hilite);
			} else {
				cairo_set_source_rgba(cr, 0.3, 0.3, 0.5, 1.0);
				cairo_rectangle(cr, 0, y, inner_w, item_h);
				cairo_fill(cr);
			}
		}

		/* Draw label text */
		const struct wm_color *text_color = is_selected ?
			&style->menu_hilite_text_color :
			&style->menu_frame_text_color;

		bool is_disabled = (item->type == WM_MENU_NOP);
		struct wm_color disabled_color;
		if (is_disabled) {
			/* Grey out disabled items */
			disabled_color = *text_color;
			disabled_color.r = (disabled_color.r + 128) / 2;
			disabled_color.g = (disabled_color.g + 128) / 2;
			disabled_color.b = (disabled_color.b + 128) / 2;
			text_color = &disabled_color;
		}

		/* Draw icon if present and icon column is active */
		if (icon_col_w > 0 && item->icon_path) {
			int icon_size = item_h - 2 * MENU_ITEM_PADDING;
			if (icon_size > 0) {
				cairo_surface_t *icon =
					cairo_image_surface_create_from_png(
						item->icon_path);
				if (icon && cairo_surface_status(icon) ==
				    CAIRO_STATUS_SUCCESS) {
					int iw = cairo_image_surface_get_width(
						icon);
					int ih = cairo_image_surface_get_height(
						icon);
					double ix = MENU_ITEM_PADDING;
					double iy = y + MENU_ITEM_PADDING;
					cairo_save(cr);
					cairo_translate(cr, ix, iy);
					cairo_scale(cr,
						(double)icon_size / iw,
						(double)icon_size / ih);
					cairo_set_source_surface(cr, icon,
						0, 0);
					cairo_paint(cr);
					cairo_restore(cr);
				}
				if (icon)
					cairo_surface_destroy(icon);
			}
		}

		/* Draw checked state dot (Fluxbox-style indicator) */
		if (item->checked) {
			double dot_size = 6.0;
			double dot_x = MENU_ITEM_PADDING + icon_col_w;
			double dot_y = y + (item_h - dot_size) / 2.0;
			cairo_set_source_rgba(cr,
				text_color->r / 255.0,
				text_color->g / 255.0,
				text_color->b / 255.0,
				text_color->a / 255.0);
			/* Filled diamond (Fluxbox-style bullet) */
			double cx = dot_x + dot_size / 2.0;
			double cy = dot_y + dot_size / 2.0;
			double r = dot_size / 2.0;
			cairo_move_to(cr, cx, dot_y);
			cairo_line_to(cr, cx + r, cy);
			cairo_line_to(cr, cx, dot_y + dot_size);
			cairo_line_to(cr, cx - r, cy);
			cairo_close_path(cr);
			cairo_fill(cr);
		}

		if (item->label) {
			int tw, th;
			int check_pad = item->checked ? 12 : 0;
			int max_w = inner_w - 2 * MENU_ITEM_PADDING -
				icon_col_w - check_pad;
			bool has_arrow = (item->type == WM_MENU_SUBMENU ||
				item->type == WM_MENU_SENDTO ||
				item->type == WM_MENU_LAYER ||
				item->type == WM_MENU_WORKSPACES ||
				item->type == WM_MENU_CONFIG);
			if (has_arrow) {
				max_w -= MENU_ARROW_SIZE + 4;
			}
			/* Detect RTL text for menu item alignment */
			bool item_rtl = (find_base_dir(
				item->label) == PANGO_DIRECTION_RTL);
			enum wm_justify item_just = item_rtl
				? WM_JUSTIFY_RIGHT : WM_JUSTIFY_LEFT;
			cairo_surface_t *text = wm_render_text(item->label,
				&style->menu_frame_font, text_color,
				max_w, &tw, &th, item_just, 1.0f);
			if (text) {
				int tx;
				if (item_rtl) {
					tx = inner_w - MENU_ITEM_PADDING -
						tw;
					if (has_arrow)
						tx -= MENU_ARROW_SIZE + 4;
				} else {
					tx = MENU_ITEM_PADDING + icon_col_w +
						check_pad;
				}
				int ty = y + (item_h - th) / 2;
				if (ty < y) {
					ty = y;
				}
				cairo_set_source_surface(cr, text, tx, ty);
				cairo_paint(cr);
				cairo_surface_destroy(text);
			}
		}

		/* Draw submenu bullet indicator */
		bool has_submenu = (item->type == WM_MENU_SUBMENU ||
			item->type == WM_MENU_SENDTO ||
			item->type == WM_MENU_LAYER ||
			item->type == WM_MENU_WORKSPACES ||
			item->type == WM_MENU_CONFIG);
		if (has_submenu) {
			const char *bullet = style->menu_bullet;
			if (!bullet)
				bullet = "triangle";

			/* Skip drawing if "empty" */
			if (strcasecmp(bullet, "empty") != 0) {
				/* For RTL labels, mirror the arrow side */
				bool lbl_rtl = item->label &&
					(find_base_dir(item->label)
						== PANGO_DIRECTION_RTL);
				bool style_left =
					(style->menu_bullet_position ==
					 WM_JUSTIFY_LEFT);
				bool on_left = lbl_rtl
					? !style_left : style_left;
				double ax;
				if (on_left) {
					ax = MENU_ITEM_PADDING;
				} else {
					ax = inner_w - MENU_ITEM_PADDING -
						MENU_ARROW_SIZE;
				}
				double ay = y +
					(item_h - MENU_ARROW_SIZE) / 2.0;

				cairo_set_source_rgba(cr,
					text_color->r / 255.0,
					text_color->g / 255.0,
					text_color->b / 255.0,
					text_color->a / 255.0);

				if (strcasecmp(bullet, "square") == 0) {
					cairo_rectangle(cr, ax, ay,
						MENU_ARROW_SIZE,
						MENU_ARROW_SIZE);
					cairo_fill(cr);
				} else if (strcasecmp(bullet,
					   "diamond") == 0) {
					double cx = ax +
						MENU_ARROW_SIZE / 2.0;
					double cy = ay +
						MENU_ARROW_SIZE / 2.0;
					double r = MENU_ARROW_SIZE / 2.0;
					cairo_move_to(cr, cx, ay);
					cairo_line_to(cr, cx + r, cy);
					cairo_line_to(cr, cx, ay +
						MENU_ARROW_SIZE);
					cairo_line_to(cr, cx - r, cy);
					cairo_close_path(cr);
					cairo_fill(cr);
				} else {
					/* Default: triangle */
					if (on_left) {
						/* Point left */
						cairo_move_to(cr,
							ax + MENU_ARROW_SIZE,
							ay);
						cairo_line_to(cr, ax,
							ay +
							MENU_ARROW_SIZE / 2.0);
						cairo_line_to(cr,
							ax + MENU_ARROW_SIZE,
							ay + MENU_ARROW_SIZE);
					} else {
						/* Point right */
						cairo_move_to(cr, ax, ay);
						cairo_line_to(cr,
							ax + MENU_ARROW_SIZE,
							ay +
							MENU_ARROW_SIZE / 2.0);
						cairo_line_to(cr, ax,
							ay + MENU_ARROW_SIZE);
					}
					cairo_close_path(cr);
					cairo_fill(cr);
				}
			}
		}

		y += item_h;
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
