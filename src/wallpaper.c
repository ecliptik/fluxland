/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * wallpaper.c - Native per-workspace wallpaper support
 *
 * Renders wallpaper images (PNG via Cairo) as scene buffers under the
 * background layer.  Each workspace gets its own scene buffer node that
 * is enabled/disabled during workspace switching.
 */

#define _GNU_SOURCE

#include <cairo.h>
#include <drm_fourcc.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/log.h>

#include "wallpaper.h"
#include "style.h"
#include "config.h"
#include "output.h"
#include "server.h"

/* Per-workspace wallpaper entry */
struct wm_wp_entry {
	struct wlr_scene_buffer *scene_buf;
	struct wlr_buffer *buffer; /* NULL until loaded */
};

struct wm_wallpaper {
	struct wm_server *server;
	struct wlr_scene_tree *tree; /* child of layer_background */
	struct wm_wp_entry *entries;
	int entry_count;
	int active_index;
};

/* --- Cairo-to-wlr_buffer bridge (same pattern as toolbar.c) --- */

struct wp_pixel_buffer {
	struct wlr_buffer base;
	void *data;
	uint32_t format;
	size_t stride;
};

static void
wp_buffer_destroy(struct wlr_buffer *wlr_buffer)
{
	struct wp_pixel_buffer *buf =
		wl_container_of(wlr_buffer, buf, base);
	free(buf->data);
	free(buf);
}

static bool
wp_buffer_begin_access(struct wlr_buffer *wlr_buffer, uint32_t flags,
	void **data, uint32_t *format, size_t *stride)
{
	struct wp_pixel_buffer *buf =
		wl_container_of(wlr_buffer, buf, base);
	if (flags & WLR_BUFFER_DATA_PTR_ACCESS_WRITE)
		return false;
	*data = buf->data;
	*format = buf->format;
	*stride = buf->stride;
	return true;
}

static void
wp_buffer_end_access(struct wlr_buffer *wlr_buffer)
{
	(void)wlr_buffer;
}

static const struct wlr_buffer_impl wp_buffer_impl = {
	.destroy = wp_buffer_destroy,
	.begin_data_ptr_access = wp_buffer_begin_access,
	.end_data_ptr_access = wp_buffer_end_access,
};

static struct wlr_buffer *
wp_buffer_from_cairo(cairo_surface_t *surface)
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

	struct wp_pixel_buffer *buf = calloc(1, sizeof(*buf));
	if (!buf) {
		free(data);
		cairo_surface_destroy(surface);
		return NULL;
	}

	wlr_buffer_init(&buf->base, &wp_buffer_impl, width, height);
	buf->data = data;
	buf->format = DRM_FORMAT_ARGB8888;
	buf->stride = stride;

	cairo_surface_destroy(surface);
	return &buf->base;
}

/* --- Wallpaper rendering --- */

static bool
parse_hex_color(const char *str, float *r, float *g, float *b)
{
	if (!str || str[0] != '#' || strlen(str) < 7)
		return false;
	unsigned int rv, gv, bv;
	if (sscanf(str + 1, "%02x%02x%02x", &rv, &gv, &bv) != 3)
		return false;
	*r = (float)rv / 255.0f;
	*g = (float)gv / 255.0f;
	*b = (float)bv / 255.0f;
	return true;
}

static cairo_surface_t *
render_solid_color(const char *color_str, int width, int height)
{
	float r = 0, g = 0, b = 0;
	if (!parse_hex_color(color_str, &r, &g, &b))
		return NULL;

	cairo_surface_t *surf = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, width, height);
	if (cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(surf);
		return NULL;
	}

	cairo_t *cr = cairo_create(surf);
	cairo_set_source_rgb(cr, r, g, b);
	cairo_paint(cr);
	cairo_destroy(cr);

	return surf;
}

static cairo_surface_t *
render_wallpaper_stretch(cairo_surface_t *img, int width, int height)
{
	int iw = cairo_image_surface_get_width(img);
	int ih = cairo_image_surface_get_height(img);

	cairo_surface_t *surf = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, width, height);
	cairo_t *cr = cairo_create(surf);
	cairo_scale(cr, (double)width / iw, (double)height / ih);
	cairo_set_source_surface(cr, img, 0, 0);
	cairo_paint(cr);
	cairo_destroy(cr);
	return surf;
}

static cairo_surface_t *
render_wallpaper_center(cairo_surface_t *img, int width, int height,
	const char *bg_color)
{
	int iw = cairo_image_surface_get_width(img);
	int ih = cairo_image_surface_get_height(img);

	cairo_surface_t *surf = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, width, height);
	cairo_t *cr = cairo_create(surf);

	/* Fill background */
	float r = 0, g = 0, b = 0;
	parse_hex_color(bg_color, &r, &g, &b);
	cairo_set_source_rgb(cr, r, g, b);
	cairo_paint(cr);

	/* Center image */
	int dx = (width - iw) / 2;
	int dy = (height - ih) / 2;
	cairo_set_source_surface(cr, img, dx, dy);
	cairo_paint(cr);
	cairo_destroy(cr);
	return surf;
}

static cairo_surface_t *
render_wallpaper_tile(cairo_surface_t *img, int width, int height)
{
	int iw = cairo_image_surface_get_width(img);
	int ih = cairo_image_surface_get_height(img);
	if (iw <= 0 || ih <= 0)
		return NULL;

	cairo_surface_t *surf = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, width, height);
	cairo_t *cr = cairo_create(surf);

	cairo_pattern_t *pat = cairo_pattern_create_for_surface(img);
	cairo_pattern_set_extend(pat, CAIRO_EXTEND_REPEAT);
	cairo_set_source(cr, pat);
	cairo_paint(cr);
	cairo_pattern_destroy(pat);
	cairo_destroy(cr);
	return surf;
}

/* Scaled keeping its shape: to cover the output (Fill, cropped) or to
 * fit inside it (Fit, fbsetbg -a, with the colour around) */
static cairo_surface_t *
render_wallpaper_fill(cairo_surface_t *img, int width, int height,
	const char *bg_color, bool cover)
{
	int iw = cairo_image_surface_get_width(img);
	int ih = cairo_image_surface_get_height(img);
	if (iw <= 0 || ih <= 0)
		return NULL;

	double scale_x = (double)width / iw;
	double scale_y = (double)height / ih;
	double scale = (scale_x > scale_y) == cover ? scale_x : scale_y;

	int sw = (int)(iw * scale);
	int sh = (int)(ih * scale);
	int dx = (width - sw) / 2;
	int dy = (height - sh) / 2;

	cairo_surface_t *surf = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, width, height);
	cairo_t *cr = cairo_create(surf);

	float r = 0, g = 0, b = 0;
	parse_hex_color(bg_color, &r, &g, &b);
	cairo_set_source_rgb(cr, r, g, b);
	cairo_paint(cr);

	cairo_translate(cr, dx, dy);
	cairo_scale(cr, scale, scale);
	cairo_set_source_surface(cr, img, 0, 0);
	cairo_paint(cr);
	cairo_destroy(cr);
	return surf;
}

static struct wlr_buffer *
load_wallpaper(const char *path, enum wm_wallpaper_mode mode,
	const char *bg_color, int width, int height)
{
	if (!path || width <= 0 || height <= 0)
		return NULL;

	cairo_surface_t *img = wm_load_image(path);
	if (!img) {
		wlr_log(WLR_ERROR, "wallpaper: failed to load %s", path);
		return NULL;
	}

	cairo_surface_t *rendered = NULL;
	switch (mode) {
	case WM_WALLPAPER_STRETCH:
		rendered = render_wallpaper_stretch(img, width, height);
		break;
	case WM_WALLPAPER_CENTER:
		rendered = render_wallpaper_center(img, width, height,
			bg_color);
		break;
	case WM_WALLPAPER_TILE:
		rendered = render_wallpaper_tile(img, width, height);
		break;
	case WM_WALLPAPER_FILL:
	case WM_WALLPAPER_FIT:
		rendered = render_wallpaper_fill(img, width, height,
			bg_color, mode == WM_WALLPAPER_FILL);
		break;
	}

	cairo_surface_destroy(img);

	if (!rendered)
		return NULL;

	return wp_buffer_from_cairo(rendered);
}

/* --- The style's background (Fluxbox's RootTheme) --- */

static bool
style_background_shown(const struct wm_style *style)
{
	return style && style->background &&
		!strcasestr(style->background, "none") &&
		!strcasestr(style->background, "unset");
}

/* A random image of a directory (background: random) */
static char *
random_image(const char *dir)
{
	DIR *d = opendir(dir);
	if (!d)
		return NULL;
	char *pick = NULL;
	int seen = 0;
	struct dirent *e;
	while ((e = readdir(d))) {
		if (e->d_name[0] == '.')
			continue;
		char path[4096];
		snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
		struct stat st;
		if (stat(path, &st) != 0 || !S_ISREG(st.st_mode))
			continue;
		/* Reservoir sampling: each file as likely as the others */
		if (rand() % ++seen == 0) {
			free(pick);
			pick = strdup(path);
		}
	}
	closedir(d);
	return pick;
}

/*
 * As RootTheme::reconfigTheme has fbsetbg and fbsetroot draw it: the
 * image of background.pixmap (tiled, centered, aspect, or fullscreen),
 * a random one of its directory, or with no image the colours: mod X Y
 * lines, a gradient texture, or solid.
 */
static struct wlr_buffer *
render_style_background(const struct wm_style *style, int width, int height)
{
	const char *opts = style->background;
	char hex[8] = "#000000";
	if (style->background_has_color)
		snprintf(hex, sizeof(hex), "#%02x%02x%02x",
			style->background_color.r, style->background_color.g,
			style->background_color.b);

	const char *pm = style->background_pixmap;
	struct stat st;
	if (pm && stat(pm, &st) == 0) {
		enum wm_wallpaper_mode mode = strcasestr(opts, "tiled") ?
			WM_WALLPAPER_TILE : strcasestr(opts, "centered") ?
			WM_WALLPAPER_CENTER : strcasestr(opts, "aspect") ?
			WM_WALLPAPER_FIT : WM_WALLPAPER_STRETCH;
		if (S_ISREG(st.st_mode))
			return load_wallpaper(pm, mode, hex, width, height);
		if (S_ISDIR(st.st_mode) && strcasestr(opts, "random")) {
			char *pick = random_image(pm);
			struct wlr_buffer *buf = pick ? load_wallpaper(pick,
				WM_WALLPAPER_STRETCH, hex, width, height) : NULL;
			free(pick);
			return buf;
		}
	}

	cairo_surface_t *surf = NULL;
	if (strcasestr(opts, "mod") && style->background_mod_x > 0 &&
	    style->background_mod_y > 0) {
		/* fbsetroot -mod: the foreground every X and Y pixels */
		surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
			width, height);
		cairo_t *cr = cairo_create(surf);
		const struct wm_color *bg = style->background_has_color_to ?
			&style->background_color_to : &style->background_color;
		cairo_set_source_rgb(cr, bg->r / 255.0, bg->g / 255.0,
			bg->b / 255.0);
		cairo_paint(cr);
		const struct wm_color *fg = &style->background_color;
		cairo_set_source_rgb(cr, fg->r / 255.0, fg->g / 255.0,
			fg->b / 255.0);
		for (int x = 0; x < width; x += style->background_mod_x)
			cairo_rectangle(cr, x, 0, 1, height);
		for (int y = 0; y < height; y += style->background_mod_y)
			cairo_rectangle(cr, 0, y, width, 1);
		cairo_fill(cr);
		cairo_destroy(cr);
	} else if (strcasestr(opts, "gradient")) {
		struct wm_texture tex = style_parse_texture(opts);
		if (style->background_has_color)
			tex.color = 0xFF000000u |
				((uint32_t)style->background_color.r << 16) |
				((uint32_t)style->background_color.g << 8) |
				style->background_color.b;
		if (style->background_has_color_to)
			tex.color_to = 0xFF000000u |
				((uint32_t)style->background_color_to.r << 16) |
				((uint32_t)style->background_color_to.g << 8) |
				style->background_color_to.b;
		surf = wm_render_texture(&tex, width, height, 1.0f);
		free(tex.pixmap_path);
	} else if (style->background_has_color) {
		surf = render_solid_color(hex, width, height);
	}
	return surf ? wp_buffer_from_cairo(surf) : NULL;
}

/* --- Public API --- */

struct wm_wallpaper *
wm_wallpaper_create(struct wm_server *server)
{
	struct wm_config *config = server->config;
	if (!config)
		return NULL;

	/* The style's background shows over the last wallpaper when a
	 * style was chosen this session, or when there is none (fbsetbg
	 * -Z and -z); the init's wallpaper always wins */
	bool style_bg = style_background_shown(server->style) &&
		(!config->wallpaper || (config->wallpaper_from_last &&
		 server->style_background));

	/* Check if any wallpaper is configured */
	bool has_any = (config->wallpaper != NULL) ||
		       (config->wallpaper_color != NULL) || style_bg;
	if (!has_any) {
		for (int i = 0; i < config->wallpaper_path_count; i++) {
			if (config->wallpaper_paths[i]) {
				has_any = true;
				break;
			}
		}
	}
	if (!has_any)
		return NULL;

	struct wm_wallpaper *wp = calloc(1, sizeof(*wp));
	if (!wp)
		return NULL;

	wp->server = server;
	wp->active_index = 0;

	/* Create wallpaper tree under the background layer */
	wp->tree = wlr_scene_tree_create(server->layer_background);
	if (!wp->tree) {
		free(wp);
		return NULL;
	}

	int count = server->workspace_count;
	wp->entries = calloc(count, sizeof(*wp->entries));
	if (!wp->entries) {
		wlr_scene_node_destroy(&wp->tree->node);
		free(wp);
		return NULL;
	}
	wp->entry_count = count;

	/* Get output dimensions for wallpaper rendering */
	int width = 1920, height = 1080; /* fallback */
	struct wm_output *output;
	wl_list_for_each(output, &server->outputs, link) {
		struct wlr_box box;
		wlr_output_layout_get_box(server->output_layout,
			output->wlr_output, &box);
		if (box.width > 0 && box.height > 0) {
			width = box.width;
			height = box.height;
			break;
		}
	}

	const char *bg_color = config->wallpaper_color ?
		config->wallpaper_color : "#000000";

	for (int i = 0; i < count; i++) {
		/* Determine wallpaper source for this workspace */
		const char *path = NULL;
		if (i < config->wallpaper_path_count &&
		    config->wallpaper_paths[i]) {
			path = config->wallpaper_paths[i];
		} else if (config->wallpaper) {
			path = config->wallpaper;
		}

		struct wlr_buffer *buf = NULL;
		bool own = i < config->wallpaper_path_count &&
			config->wallpaper_paths[i];
		if (style_bg && !own)
			buf = render_style_background(server->style, width,
				height);
		if (!buf && path) {
			buf = load_wallpaper(path, config->wallpaper_mode,
				bg_color, width, height);
		}

		/* Fall back to solid color if no image */
		if (!buf && config->wallpaper_color) {
			cairo_surface_t *solid =
				render_solid_color(bg_color, width, height);
			if (solid)
				buf = wp_buffer_from_cairo(solid);
		}

		if (buf) {
			wp->entries[i].buffer = buf;
			wp->entries[i].scene_buf =
				wlr_scene_buffer_create(wp->tree, buf);
			if (wp->entries[i].scene_buf) {
				/* Only show the first workspace's wallpaper */
				wlr_scene_node_set_enabled(
					&wp->entries[i].scene_buf->node,
					i == 0);
			}
		}
	}

	wlr_log(WLR_INFO, "wallpaper: initialized %d workspace wallpapers",
		count);
	return wp;
}

void
wm_wallpaper_destroy(struct wm_wallpaper *wp)
{
	if (!wp)
		return;

	for (int i = 0; i < wp->entry_count; i++) {
		if (wp->entries[i].buffer)
			wlr_buffer_drop(wp->entries[i].buffer);
		/* scene_buf is destroyed with the tree */
	}
	free(wp->entries);

	if (wp->tree)
		wlr_scene_node_destroy(&wp->tree->node);

	free(wp);
}

void
wm_wallpaper_switch(struct wm_wallpaper *wp, int workspace_index)
{
	if (!wp || workspace_index < 0 ||
	    workspace_index >= wp->entry_count)
		return;

	/* Disable old, enable new */
	if (wp->active_index >= 0 &&
	    wp->active_index < wp->entry_count &&
	    wp->entries[wp->active_index].scene_buf) {
		wlr_scene_node_set_enabled(
			&wp->entries[wp->active_index].scene_buf->node,
			false);
	}

	if (wp->entries[workspace_index].scene_buf) {
		wlr_scene_node_set_enabled(
			&wp->entries[workspace_index].scene_buf->node,
			true);
	}

	wp->active_index = workspace_index;
}

void
wm_wallpaper_reload(struct wm_wallpaper *wp)
{
	if (!wp || !wp->server)
		return;

	int active = wp->active_index;

	/* Re-create by destroying and recreating */
	struct wm_server *server = wp->server;
	wm_wallpaper_destroy(wp);

	struct wm_wallpaper *new_wp = wm_wallpaper_create(server);
	if (new_wp) {
		wm_wallpaper_switch(new_wp, active);
		server->wallpaper = new_wp;
	} else {
		server->wallpaper = NULL;
	}
}
