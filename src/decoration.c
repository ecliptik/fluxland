/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 *
 * decoration.c - Server-side window decoration rendering and management
 */

#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <cairo.h>
#include <drm_fourcc.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>

#include "config.h"
#include "decoration.h"
#include "render.h"
#include "server.h"
#include "style.h"
#include "tabgroup.h"
#include "view.h"

/* --- Default dimensions --- */

#define DEFAULT_TITLEBAR_HEIGHT 20
#define DEFAULT_HANDLE_HEIGHT 6
#define DEFAULT_BORDER_WIDTH 1
#define DEFAULT_GRIP_WIDTH 20
#define DEFAULT_BUTTON_SIZE 12
#define BUTTON_PADDING 4

/* --- Cairo-to-wlr_buffer bridge --- */

/*
 * A wlr_buffer implementation backed by raw pixel data from a Cairo surface.
 * This lets us pass Cairo-rendered content into the wlroots scene graph.
 */
struct wm_pixel_buffer {
	struct wlr_buffer base;
	void *data;
	uint32_t format;
	size_t stride;
};

static void pixel_buffer_destroy(struct wlr_buffer *wlr_buffer)
{
	struct wm_pixel_buffer *buffer = wl_container_of(wlr_buffer, buffer, base);
	free(buffer->data);
	free(buffer);
}

static bool pixel_buffer_begin_data_ptr_access(struct wlr_buffer *wlr_buffer,
	uint32_t flags, void **data, uint32_t *format, size_t *stride)
{
	struct wm_pixel_buffer *buffer = wl_container_of(wlr_buffer, buffer, base);
	if (flags & WLR_BUFFER_DATA_PTR_ACCESS_WRITE) {
		return false; /* read-only */
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

/*
 * Create a wlr_buffer from a Cairo ARGB32 surface.
 * The Cairo surface is consumed (destroyed) by this function.
 * Returns a new wlr_buffer, or NULL on error.
 */
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
	/* Cairo ARGB32 is native-endian ARGB which is DRM_FORMAT_ARGB8888 */
	buffer->format = DRM_FORMAT_ARGB8888;
	buffer->stride = stride;

	cairo_surface_destroy(surface);
	return &buffer->base;
}

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

/* --- Rendering helpers --- */

/*
 * Render a single button: background texture + glyph overlay.
 * Returns a wlr_buffer or NULL on error.
 */
static struct wlr_buffer *
render_button(const struct wm_texture *bg_tex,
	const struct wm_color *pic_color, enum wm_button_type type,
	int size)
{
	/* Render background */
	cairo_surface_t *bg = wm_render_texture(bg_tex, size, size, 1.0f);
	if (!bg) {
		/* Fallback: solid background */
		bg = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size, size);
		if (cairo_surface_status(bg) != CAIRO_STATUS_SUCCESS) {
			cairo_surface_destroy(bg);
			return NULL;
		}
		cairo_t *cr = cairo_create(bg);
		cairo_set_source_rgba(cr,
			((bg_tex->color >> 16) & 0xFF) / 255.0,
			((bg_tex->color >> 8) & 0xFF) / 255.0,
			(bg_tex->color & 0xFF) / 255.0,
			1.0);
		cairo_paint(cr);
		cairo_destroy(cr);
	}

	/* Render glyph */
	int glyph_size = size - 4; /* some padding */
	if (glyph_size < 4) {
		glyph_size = size;
	}
	cairo_surface_t *glyph = wm_render_button_glyph(type, pic_color,
		glyph_size, 1.0f);

	/* Composite glyph onto background */
	if (glyph) {
		cairo_t *cr = cairo_create(bg);
		int offset = (size - glyph_size) / 2;
		cairo_set_source_surface(cr, glyph, offset, offset);
		cairo_paint(cr);
		cairo_destroy(cr);
		cairo_surface_destroy(glyph);
	}

	return wlr_buffer_from_cairo(bg);
}

/*
 * Render the titlebar label: background texture + title text overlay.
 */
static struct wlr_buffer *
render_label(const struct wm_texture *bg_tex, const char *title,
	const struct wm_font *font, const struct wm_color *text_color,
	enum wm_justify justify, int width, int height)
{
	/* Render background */
	cairo_surface_t *bg = wm_render_texture(bg_tex, width, height, 1.0f);
	if (!bg) {
		bg = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
			width, height);
	}

	if (cairo_surface_status(bg) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(bg);
		return NULL;
	}

	/* Render text */
	if (title && *title) {
		int text_width, text_height;
		cairo_surface_t *text_surf = wm_render_text(title, font,
			text_color, width - 4, &text_width, &text_height,
			justify, 1.0f);

		if (text_surf) {
			cairo_t *cr = cairo_create(bg);
			int text_x = 2; /* default left with padding */
			int text_y = (height - text_height) / 2;

			if (justify == WM_JUSTIFY_CENTER) {
				text_x = (width - text_width) / 2;
			} else if (justify == WM_JUSTIFY_RIGHT) {
				text_x = width - text_width - 2;
			}
			if (text_y < 0) {
				text_y = 0;
			}

			cairo_set_source_surface(cr, text_surf,
				text_x, text_y);
			cairo_paint(cr);
			cairo_destroy(cr);
			cairo_surface_destroy(text_surf);
		}
	}

	return wlr_buffer_from_cairo(bg);
}

/* --- Dimensions from style --- */

static int
style_titlebar_height(const struct wm_style *style)
{
	if (style->window_title_height > 0) {
		return style->window_title_height;
	}
	/* Auto-size from font */
	int height = style->window_label_focus_font.size;
	if (height <= 0) {
		height = 12;
	}
	return height + 8; /* padding */
}

static int
style_handle_height(const struct wm_style *style)
{
	if (style->window_handle_width > 0) {
		return style->window_handle_width;
	}
	return DEFAULT_HANDLE_HEIGHT;
}

static int
style_border_width(const struct wm_style *style)
{
	if (style->window_border_width >= 0) {
		return style->window_border_width;
	}
	return DEFAULT_BORDER_WIDTH;
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
	wlr_xdg_surface_get_geometry(view->xdg_toplevel->base, &geo);
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

	/* Create titlebar tree */
	deco->titlebar_tree = wlr_scene_tree_create(deco->tree);

	/* Create scene buffer nodes (initially empty) */
	deco->title_bg = wlr_scene_buffer_create(deco->titlebar_tree, NULL);
	deco->label_buf = wlr_scene_buffer_create(deco->titlebar_tree, NULL);
	deco->handle_buf = wlr_scene_buffer_create(deco->tree, NULL);
	deco->grip_left = wlr_scene_buffer_create(deco->tree, NULL);
	deco->grip_right = wlr_scene_buffer_create(deco->tree, NULL);

	/* Button layout from config or defaults */
	const char *left_str = "Stick";
	const char *right_str = "Shade Minimize Maximize Close";
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

/* --- Layout and render --- */

static void
layout_and_render(struct wm_decoration *deco, struct wm_style *style)
{
	int bw = deco->border_width;
	int th = deco->titlebar_height;
	int hh = deco->handle_height;
	int cw = deco->content_width;
	int ch = deco->content_height;
	int gw = deco->grip_width;

	bool has_titlebar = (deco->preset == WM_DECOR_NORMAL ||
		deco->preset == WM_DECOR_TAB ||
		deco->preset == WM_DECOR_TINY ||
		deco->preset == WM_DECOR_TOOL);
	bool has_handle = (deco->preset == WM_DECOR_NORMAL);
	bool has_borders = (deco->preset == WM_DECOR_NORMAL ||
		deco->preset == WM_DECOR_BORDER);

	if (deco->preset == WM_DECOR_NONE) {
		has_titlebar = false;
		has_handle = false;
		has_borders = false;
	}

	/* Total outer dimensions */
	int outer_width = cw + 2 * bw;
	int top_height = has_titlebar ? th : 0;
	int bottom_height = has_handle ? hh : 0;

	/* Content area position within the decoration tree */
	deco->content_area.x = bw;
	deco->content_area.y = bw + top_height;
	deco->content_area.width = cw;
	deco->content_area.height = ch;

	/* Select focused/unfocused textures */
	const struct wm_texture *title_tex = deco->focused ?
		&style->window_title_focus : &style->window_title_unfocus;
	const struct wm_texture *label_tex = deco->focused ?
		&style->window_label_focus : &style->window_label_unfocus;
	const struct wm_font *label_font = deco->focused ?
		&style->window_label_focus_font :
		&style->window_label_unfocus_font;
	const struct wm_color *text_color = deco->focused ?
		&style->window_label_focus_text_color :
		&style->window_label_unfocus_text_color;
	const struct wm_texture *handle_tex = deco->focused ?
		&style->window_handle_focus : &style->window_handle_unfocus;
	const struct wm_texture *grip_tex = deco->focused ?
		&style->window_grip_focus : &style->window_grip_unfocus;
	const struct wm_texture *button_tex = deco->focused ?
		&style->window_button_focus : &style->window_button_unfocus;
	const struct wm_color *pic_color = deco->focused ?
		&style->window_button_focus_pic_color :
		&style->window_button_unfocus_pic_color;
	const struct wm_color *border_color = &style->window_border_color;

	/* --- Borders --- */
	float bcol[4];
	wm_color_to_float4(border_color, bcol);

	if (has_borders && bw > 0) {
		int total_h = top_height + ch + bottom_height + 2 * bw;

		/* Top border */
		wlr_scene_rect_set_size(deco->border_top, outer_width, bw);
		wlr_scene_rect_set_color(deco->border_top, bcol);
		wlr_scene_node_set_position(&deco->border_top->node, 0, 0);
		wlr_scene_node_set_enabled(&deco->border_top->node, true);

		/* Bottom border */
		wlr_scene_rect_set_size(deco->border_bottom, outer_width, bw);
		wlr_scene_rect_set_color(deco->border_bottom, bcol);
		wlr_scene_node_set_position(&deco->border_bottom->node,
			0, total_h - bw);
		wlr_scene_node_set_enabled(&deco->border_bottom->node, true);

		/* Left border */
		wlr_scene_rect_set_size(deco->border_left, bw, total_h);
		wlr_scene_rect_set_color(deco->border_left, bcol);
		wlr_scene_node_set_position(&deco->border_left->node, 0, 0);
		wlr_scene_node_set_enabled(&deco->border_left->node, true);

		/* Right border */
		wlr_scene_rect_set_size(deco->border_right, bw, total_h);
		wlr_scene_rect_set_color(deco->border_right, bcol);
		wlr_scene_node_set_position(&deco->border_right->node,
			outer_width - bw, 0);
		wlr_scene_node_set_enabled(&deco->border_right->node, true);
	} else {
		wlr_scene_node_set_enabled(&deco->border_top->node, false);
		wlr_scene_node_set_enabled(&deco->border_bottom->node, false);
		wlr_scene_node_set_enabled(&deco->border_left->node, false);
		wlr_scene_node_set_enabled(&deco->border_right->node, false);
	}

	/* --- Titlebar --- */
	if (has_titlebar && th > 0) {
		int titlebar_width = cw;

		wlr_scene_node_set_enabled(&deco->titlebar_tree->node, true);
		wlr_scene_node_set_position(&deco->titlebar_tree->node,
			bw, bw);

		/* Titlebar background */
		struct wlr_buffer *tbg = wlr_buffer_from_cairo(
			wm_render_texture(title_tex, titlebar_width, th, 1.0f));
		wlr_scene_buffer_set_buffer(deco->title_bg, tbg);
		if (tbg) {
			wlr_buffer_drop(tbg);
		}
		wlr_scene_node_set_position(&deco->title_bg->node, 0, 0);

		/* Layout buttons and compute label area */
		int button_size = th - 2 * BUTTON_PADDING;
		if (button_size < 6) {
			button_size = 6;
		}

		int left_buttons_width = 0;
		int right_buttons_width = 0;

		/* Render and position left buttons */
		for (int i = 0; i < deco->buttons_left_count; i++) {
			struct wm_decor_button *btn = &deco->buttons_left[i];
			struct wlr_buffer *buf = render_button(button_tex,
				pic_color, btn->type, button_size);

			if (!btn->node) {
				btn->node = wlr_scene_buffer_create(
					deco->titlebar_tree, buf);
			} else {
				wlr_scene_buffer_set_buffer(btn->node, buf);
			}
			if (buf) {
				wlr_buffer_drop(buf);
			}

			int bx = BUTTON_PADDING +
				i * (button_size + BUTTON_PADDING);
			int by = BUTTON_PADDING;
			wlr_scene_node_set_position(&btn->node->node, bx, by);

			btn->box.x = bw + bx;
			btn->box.y = bw + by;
			btn->box.width = button_size;
			btn->box.height = button_size;

			left_buttons_width = bx + button_size + BUTTON_PADDING;
		}

		/* Render and position right buttons */
		for (int i = 0; i < deco->buttons_right_count; i++) {
			right_buttons_width += button_size + BUTTON_PADDING;
		}
		if (deco->buttons_right_count > 0) {
			right_buttons_width += BUTTON_PADDING;
		}

		int rx_start = titlebar_width - right_buttons_width;
		for (int i = 0; i < deco->buttons_right_count; i++) {
			struct wm_decor_button *btn = &deco->buttons_right[i];
			struct wlr_buffer *buf = render_button(button_tex,
				pic_color, btn->type, button_size);

			if (!btn->node) {
				btn->node = wlr_scene_buffer_create(
					deco->titlebar_tree, buf);
			} else {
				wlr_scene_buffer_set_buffer(btn->node, buf);
			}
			if (buf) {
				wlr_buffer_drop(buf);
			}

			int bx = rx_start + BUTTON_PADDING +
				i * (button_size + BUTTON_PADDING);
			int by = BUTTON_PADDING;
			wlr_scene_node_set_position(&btn->node->node, bx, by);

			btn->box.x = bw + bx;
			btn->box.y = bw + by;
			btn->box.width = button_size;
			btn->box.height = button_size;
		}

		/* Label area (between left and right buttons) */
		int label_x = left_buttons_width;
		int label_width = titlebar_width - left_buttons_width -
			right_buttons_width;
		if (label_width < 10) {
			label_width = 10;
		}

		/*
		 * Tab-aware label rendering:
		 * If the view is in a tab group with multiple tabs,
		 * divide the label area into N equal-width segments,
		 * one per tab.  Active tab uses label_active texture,
		 * focused non-active tabs use label_focus, unfocused
		 * tabs use label_unfocus.
		 */
		struct wm_tab_group *tg = deco->view->tab_group;
		if (tg && tg->count > 1) {
			/* Render composite tab label surface */
			int tab_count = tg->count;
			int tab_width = label_width / tab_count;
			int remainder = label_width % tab_count;

			cairo_surface_t *composite =
				cairo_image_surface_create(
					CAIRO_FORMAT_ARGB32,
					label_width, th);
			cairo_t *cr = cairo_create(composite);

			int tab_x = 0;
			int tab_idx = 0;
			struct wm_view *tab_view;
			wl_list_for_each(tab_view, &tg->views,
					tab_link) {
				/* Last tab absorbs remainder pixels */
				int tw = tab_width;
				if (tab_idx == tab_count - 1) {
					tw += remainder;
				}

				/* Select texture per tab state */
				const struct wm_texture *ttex;
				const struct wm_color *tcol;
				if (tab_view == tg->active_view) {
					ttex = &style->window_label_active;
					tcol = &style->
						window_label_active_text_color;
				} else if (deco->focused) {
					ttex = &style->window_label_focus;
					tcol = &style->
						window_label_focus_text_color;
				} else {
					ttex = &style->window_label_unfocus;
					tcol = &style->
						window_label_unfocus_text_color;
				}

				/* Render this tab segment */
				cairo_surface_t *seg =
					wm_render_texture(ttex, tw, th,
						1.0f);
				if (!seg) {
					seg = cairo_image_surface_create(
						CAIRO_FORMAT_ARGB32,
						tw, th);
				}

				/* Render tab title text onto segment */
				const char *tab_title =
					tab_view->title;
				if (tab_title && *tab_title) {
					int tw2, th2;
					cairo_surface_t *ts =
						wm_render_text(
							tab_title,
							label_font, tcol,
							tw - 4, &tw2,
							&th2,
							style->window_justify,
							1.0f);
					if (ts) {
						cairo_t *scr =
							cairo_create(seg);
						int tx = 2;
						int ty = (th - th2) / 2;
						if (style->window_justify ==
						    WM_JUSTIFY_CENTER) {
							tx = (tw - tw2) / 2;
						} else if (
						    style->window_justify ==
						    WM_JUSTIFY_RIGHT) {
							tx = tw - tw2 - 2;
						}
						if (ty < 0) ty = 0;
						cairo_set_source_surface(scr,
							ts, tx, ty);
						cairo_paint(scr);
						cairo_destroy(scr);
						cairo_surface_destroy(ts);
					}
				}

				/* Composite segment into label */
				cairo_set_source_surface(cr, seg,
					tab_x, 0);
				cairo_paint(cr);
				cairo_surface_destroy(seg);

				tab_x += tw;
				tab_idx++;
			}

			cairo_destroy(cr);
			struct wlr_buffer *lbl =
				wlr_buffer_from_cairo(composite);
			wlr_scene_buffer_set_buffer(deco->label_buf,
				lbl);
			if (lbl) {
				wlr_buffer_drop(lbl);
			}
		} else {
			/* Single view — render normal label */
			const char *title = deco->view->title;
			struct wlr_buffer *lbl = render_label(label_tex,
				title ? title : "", label_font, text_color,
				style->window_justify, label_width, th);
			wlr_scene_buffer_set_buffer(deco->label_buf, lbl);
			if (lbl) {
				wlr_buffer_drop(lbl);
			}
		}
		wlr_scene_node_set_position(&deco->label_buf->node,
			label_x, 0);
	} else {
		wlr_scene_node_set_enabled(&deco->titlebar_tree->node, false);
	}

	/* --- Handle and grips --- */
	if (has_handle && hh > 0) {
		int handle_width = cw - 2 * gw;
		if (handle_width < 0) {
			handle_width = cw;
		}
		int handle_y = bw + top_height + ch;

		/* Handle */
		struct wlr_buffer *hbuf = wlr_buffer_from_cairo(
			wm_render_texture(handle_tex,
				handle_width > 0 ? handle_width : 1, hh,
				1.0f));
		wlr_scene_buffer_set_buffer(deco->handle_buf, hbuf);
		if (hbuf) {
			wlr_buffer_drop(hbuf);
		}
		wlr_scene_node_set_position(&deco->handle_buf->node,
			bw + gw, handle_y);
		wlr_scene_node_set_enabled(&deco->handle_buf->node, true);

		/* Left grip */
		struct wlr_buffer *glbuf = wlr_buffer_from_cairo(
			wm_render_texture(grip_tex, gw, hh, 1.0f));
		wlr_scene_buffer_set_buffer(deco->grip_left, glbuf);
		if (glbuf) {
			wlr_buffer_drop(glbuf);
		}
		wlr_scene_node_set_position(&deco->grip_left->node,
			bw, handle_y);
		wlr_scene_node_set_enabled(&deco->grip_left->node, true);

		/* Right grip */
		struct wlr_buffer *grbuf = wlr_buffer_from_cairo(
			wm_render_texture(grip_tex, gw, hh, 1.0f));
		wlr_scene_buffer_set_buffer(deco->grip_right, grbuf);
		if (grbuf) {
			wlr_buffer_drop(grbuf);
		}
		wlr_scene_node_set_position(&deco->grip_right->node,
			bw + cw - gw, handle_y);
		wlr_scene_node_set_enabled(&deco->grip_right->node, true);
	} else {
		wlr_scene_node_set_enabled(&deco->handle_buf->node, false);
		wlr_scene_node_set_enabled(&deco->grip_left->node, false);
		wlr_scene_node_set_enabled(&deco->grip_right->node, false);
	}
}

/* --- Public API --- */

void
wm_decoration_update(struct wm_decoration *decoration,
	struct wm_style *style)
{
	if (!decoration || !style) {
		return;
	}

	decoration->titlebar_height = style_titlebar_height(style);
	decoration->handle_height = style_handle_height(style);
	decoration->border_width = style_border_width(style);

	layout_and_render(decoration, style);
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
wm_decoration_set_size(struct wm_decoration *decoration,
	int width, int height, struct wm_style *style)
{
	if (!decoration) {
		return;
	}
	if (decoration->content_width == width &&
	    decoration->content_height == height) {
		return;
	}
	decoration->content_width = width;
	decoration->content_height = height;
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
	if (!decoration) {
		return WM_DECOR_REGION_NONE;
	}

	int bw = decoration->border_width;
	int th = decoration->titlebar_height;
	int hh = decoration->handle_height;
	int cw = decoration->content_width;
	int ch = decoration->content_height;
	int gw = decoration->grip_width;

	bool has_titlebar = (decoration->preset == WM_DECOR_NORMAL ||
		decoration->preset == WM_DECOR_TAB ||
		decoration->preset == WM_DECOR_TINY ||
		decoration->preset == WM_DECOR_TOOL);
	bool has_handle = (decoration->preset == WM_DECOR_NORMAL);

	int outer_w = cw + 2 * bw;
	int top_h = has_titlebar ? th : 0;
	int bottom_h = has_handle ? hh : 0;
	int outer_h = top_h + ch + bottom_h + 2 * bw;

	/* Out of bounds */
	if (x < 0 || y < 0 || x >= outer_w || y >= outer_h) {
		return WM_DECOR_REGION_NONE;
	}

	/* Check buttons first */
	if (wm_decoration_button_at(decoration, x, y)) {
		return WM_DECOR_REGION_BUTTON;
	}

	/* Top border */
	if (y < bw) {
		return WM_DECOR_REGION_BORDER_TOP;
	}

	/* Bottom border */
	if (y >= outer_h - bw) {
		return WM_DECOR_REGION_BORDER_BOTTOM;
	}

	/* Left border */
	if (x < bw) {
		return WM_DECOR_REGION_BORDER_LEFT;
	}

	/* Right border */
	if (x >= outer_w - bw) {
		return WM_DECOR_REGION_BORDER_RIGHT;
	}

	/* Titlebar area */
	if (has_titlebar && y >= bw && y < bw + th) {
		return WM_DECOR_REGION_TITLEBAR;
	}

	/* Handle and grips */
	if (has_handle && y >= bw + top_h + ch) {
		if (x < bw + gw) {
			return WM_DECOR_REGION_GRIP_LEFT;
		}
		if (x >= bw + cw - gw) {
			return WM_DECOR_REGION_GRIP_RIGHT;
		}
		return WM_DECOR_REGION_HANDLE;
	}

	/* Client area */
	return WM_DECOR_REGION_CLIENT;
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
		 * When shaded, hide handle/grips and set content height to 0.
		 */
		wlr_scene_node_set_enabled(&decoration->handle_buf->node,
			false);
		wlr_scene_node_set_enabled(&decoration->grip_left->node,
			false);
		wlr_scene_node_set_enabled(&decoration->grip_right->node,
			false);
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

	int bw = decoration->border_width;
	bool has_titlebar = (decoration->preset == WM_DECOR_NORMAL ||
		decoration->preset == WM_DECOR_TAB ||
		decoration->preset == WM_DECOR_TINY ||
		decoration->preset == WM_DECOR_TOOL);
	bool has_handle = (decoration->preset == WM_DECOR_NORMAL);

	if (decoration->preset == WM_DECOR_NONE) {
		if (top) *top = 0;
		if (bottom) *bottom = 0;
		if (left) *left = 0;
		if (right) *right = 0;
		return;
	}

	if (top) *top = bw + (has_titlebar ? decoration->titlebar_height : 0);
	if (bottom) *bottom = bw + (has_handle ? decoration->handle_height : 0);
	if (left) *left = bw;
	if (right) *right = bw;
}

/* --- xdg-decoration protocol --- */

static void
handle_xdg_decoration_destroy(struct wl_listener *listener, void *data)
{
	struct wm_xdg_decoration *deco =
		wl_container_of(listener, deco, destroy);
	wl_list_remove(&deco->destroy.link);
	wl_list_remove(&deco->request_mode.link);
	free(deco);
}

static void
handle_xdg_decoration_request_mode(struct wl_listener *listener, void *data)
{
	struct wm_xdg_decoration *deco =
		wl_container_of(listener, deco, request_mode);

	/* Always request server-side decorations */
	wlr_xdg_toplevel_decoration_v1_set_mode(deco->wlr_decoration,
		WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
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

	/* Set initial mode to server-side */
	wlr_xdg_toplevel_decoration_v1_set_mode(wlr_deco,
		WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
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
}
