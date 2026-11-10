/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * systray.c - System tray using StatusNotifierItem D-Bus protocol
 *
 * Implements a StatusNotifierWatcher on the session D-Bus. Applications
 * register themselves via RegisterStatusNotifierItem; we query their
 * icon properties and render them as scene buffers in the toolbar.
 */

#ifdef WM_HAS_SYSTRAY

#define _POSIX_C_SOURCE 200809L
#include <cairo.h>
#include <drm_fourcc.h>
#include <errno.h>
#include <poll.h>
#include <time.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <systemd/sd-bus.h>
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/log.h>

#include "config.h"
#include "pixel_buffer.h"
#include "protocols.h"
#include "tray_grant.h"
#include "systray.h"
#include "systray_menu.h"
#include "server.h"
#include "toolbar.h"

/* --- Icon loading helpers --- */

static bool
file_exists(const char *path)
{
	struct stat st;
	return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

/* A PNG file as a surface, or NULL when it can't be read */
static cairo_surface_t *
load_png(const char *path)
{
	cairo_surface_t *img = cairo_image_surface_create_from_png(path);
	if (cairo_surface_status(img) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(img);
		return NULL;
	}
	return img;
}

/*
 * An icon from a directory laid out as an icon theme is (size/context/
 * name.png): the sizes nearest the tray's first, and the status context,
 * where tray icons live, with the others the applications put theirs in.
 */
static cairo_surface_t *
load_from_theme_dir(const char *base, const char *icon_name)
{
	static const char *const sizes[] = {
		"22x22", "24x24", "16x16", "32x32", "48x48", "64x64",
		"96x96", "128x128", "256x256",
	};
	static const char *const contexts[] = {
		"status", "apps", "devices", "panel",
	};

	char path[1024];
	for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
		for (size_t j = 0; j < sizeof(contexts) / sizeof(contexts[0]);
		     j++) {
			snprintf(path, sizeof(path), "%s/%s/%s/%s.png", base,
				sizes[i], contexts[j], icon_name);
			if (!file_exists(path)) {
				continue;
			}
			cairo_surface_t *img = load_png(path);
			if (img) {
				wlr_log(WLR_DEBUG, "systray: loaded icon %s",
					path);
				return img;
			}
		}
	}
	return NULL;
}

/*
 * The icon of an item named by IconThemePath, the directory of icons an
 * application brings along (Spotify's): the file itself, or a theme
 * inside it, with or without the hicolor directory in between.
 */
static cairo_surface_t *
load_from_item_dir(const char *dir, const char *icon_name)
{
	if (!dir || dir[0] != '/') {
		return NULL;
	}

	char path[1024];
	snprintf(path, sizeof(path), "%s/%s.png", dir, icon_name);
	if (file_exists(path)) {
		cairo_surface_t *img = load_png(path);
		if (img) {
			wlr_log(WLR_DEBUG, "systray: loaded icon %s", path);
			return img;
		}
	}

	snprintf(path, sizeof(path), "%s/hicolor", dir);
	cairo_surface_t *img = load_from_theme_dir(path, icon_name);
	return img ? img : load_from_theme_dir(dir, icon_name);
}

/*
 * Try to load an icon by name: from the directory the application named
 * (IconThemePath), else from the standard icon directories.
 * Returns a cairo surface, or NULL.
 */
static cairo_surface_t *
load_icon_by_name(const char *icon_name, int icon_size,
	const char *icon_theme_path)
{
	if (!icon_name || !*icon_name) {
		return NULL;
	}

	/* If the icon_name is an absolute path, try it directly */
	if (icon_name[0] == '/') {
		return file_exists(icon_name) ? load_png(icon_name) : NULL;
	}

	/* A name from D-Bus is a file name in the theme, never a path */
	if (strchr(icon_name, '/')) {
		return NULL;
	}

	cairo_surface_t *own = load_from_item_dir(icon_theme_path, icon_name);
	if (own) {
		return own;
	}

	/* The hicolor theme (PNG only, no SVG) */
	cairo_surface_t *themed = load_from_theme_dir(
		"/usr/share/icons/hicolor", icon_name);
	if (themed) {
		return themed;
	}

	char path[512];
	snprintf(path, sizeof(path), "/usr/share/pixmaps/%s.png", icon_name);
	if (file_exists(path)) {
		return load_png(path);
	}

	return NULL;
}

/*
 * Create a default placeholder icon (colored square).
 */
static cairo_surface_t *
create_placeholder_icon(int size)
{
	cairo_surface_t *surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size, size);
	cairo_t *cr = cairo_create(surface);

	/* Draw a small colored square */
	cairo_set_source_rgba(cr, 0.5, 0.6, 0.8, 0.8);
	cairo_rectangle(cr, 2, 2, size - 4, size - 4);
	cairo_fill(cr);

	/* Thin border */
	cairo_set_source_rgba(cr, 0.3, 0.3, 0.4, 1.0);
	cairo_set_line_width(cr, 1.0);
	cairo_rectangle(cr, 2.5, 2.5, size - 5, size - 5);
	cairo_stroke(cr);

	cairo_destroy(cr);
	return surface;
}

/*
 * Scale a cairo surface to the target size.
 * Returns a new surface; caller must destroy the original if needed.
 */
static cairo_surface_t *
scale_icon(cairo_surface_t *src, int target_size)
{
	if (!src) {
		return NULL;
	}

	int src_w = cairo_image_surface_get_width(src);
	int src_h = cairo_image_surface_get_height(src);

	if (src_w <= 0 || src_h <= 0) {
		return NULL;
	}

	if (src_w == target_size && src_h == target_size) {
		return src; /* no scaling needed */
	}

	cairo_surface_t *dst = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, target_size, target_size);
	cairo_t *cr = cairo_create(dst);

	double sx = (double)target_size / src_w;
	double sy = (double)target_size / src_h;
	double scale = sx < sy ? sx : sy;

	double ox = (target_size - src_w * scale) / 2.0;
	double oy = (target_size - src_h * scale) / 2.0;

	cairo_translate(cr, ox, oy);
	cairo_scale(cr, scale, scale);
	cairo_set_source_surface(cr, src, 0, 0);
	cairo_paint(cr);

	cairo_destroy(cr);
	cairo_surface_destroy(src);
	return dst;
}

/* --- Systray item management --- */

static struct wm_systray_item *
find_item_by_bus_name(struct wm_systray *systray, const char *bus_name)
{
	struct wm_systray_item *item;
	wl_list_for_each(item, &systray->items, link) {
		if (strcmp(item->bus_name, bus_name) == 0) {
			return item;
		}
	}
	return NULL;
}

static void
systray_item_destroy(struct wm_systray_item *item)
{
	if (!item) {
		return;
	}

	wl_list_remove(&item->link);
	item->systray->item_count--;

	if (item->icon_buf) {
		wlr_scene_node_destroy(&item->icon_buf->node);
	}
	if (item->icon_pixmap) {
		cairo_surface_destroy(item->icon_pixmap);
	}
	free(item->bus_name);
	free(item->object_path);
	free(item->id);
	free(item->icon_name);
	free(item->icon_theme_path);
	free(item->title);
	free(item->tooltip);
	free(item->menu_path);
	free(item);
}

static void
systray_item_update_scene(struct wm_systray_item *item)
{
	struct wm_systray *systray = item->systray;
	int size = systray->icon_size;

	/* Get or create icon surface */
	cairo_surface_t *icon = NULL;
	if (item->icon_name && *item->icon_name) {
		icon = load_icon_by_name(item->icon_name, size,
			item->icon_theme_path);
	}

	if (!icon && item->icon_pixmap) {
		/* Use the pixmap already decoded from D-Bus */
		cairo_surface_reference(item->icon_pixmap);
		icon = item->icon_pixmap;
	}

	if (!icon) {
		icon = create_placeholder_icon(size);
	}

	/* Scale to icon_size */
	icon = scale_icon(icon, size);
	if (!icon) {
		return;
	}

	/* Convert to wlr_buffer and set on scene buffer */
	struct wlr_buffer *buf = wlr_buffer_from_cairo(icon);
	if (!buf) {
		return;
	}

	if (!item->icon_buf) {
		item->icon_buf = wlr_scene_buffer_create(
			systray->scene_tree, NULL);
	}

	if (item->icon_buf) {
		wlr_scene_buffer_set_buffer(item->icon_buf, buf);
	}
	wlr_buffer_drop(buf);
}

/*
 * Where systray.pinLeft and pinRight put an id (TrayWindow::pinByClassname):
 * the further left in pinLeft the more negative, the further right in
 * pinRight the more positive, 0 when in neither.  Case doesn't matter.
 */
int
wm_systray_pin_order(const char *pin_left, const char *pin_right,
	const char *id)
{
	if (!id)
		return 0;
	const char *lists[2] = { pin_left, pin_right };
	for (int l = 0; l < 2; l++) {
		if (!lists[l])
			continue;
		char *copy = strdup(lists[l]);
		if (!copy)
			continue;
		int n = 0, at = -1;
		char *save = NULL;
		for (char *tok = strtok_r(copy, " ,", &save); tok;
		     tok = strtok_r(NULL, " ,", &save)) {
			if (at < 0 && strcasecmp(tok, id) == 0)
				at = n;
			n++;
		}
		free(copy);
		if (at >= 0)
			return l == 0 ? -(n - at) : at + 1;
	}
	return 0;
}

/* The item to its place by its order (SystemTray::sortClients, stable) */
static void
sort_item(struct wm_systray_item *item)
{
	struct wm_systray *systray = item->systray;
	struct wm_config *cfg = systray->server->config;
	item->order = cfg ? wm_systray_pin_order(cfg->systray_pin_left,
		cfg->systray_pin_right, item->id) : 0;
	wl_list_remove(&item->link);
	struct wm_systray_item *it;
	wl_list_for_each(it, &systray->items, link) {
		if (it->order > item->order) {
			wl_list_insert(it->link.prev, &item->link);
			return;
		}
	}
	wl_list_insert(systray->items.prev, &item->link);
}

/* --- D-Bus property query callbacks --- */

/*
 * An asynchronous property query.  The item can go (its application
 * quits) before the reply comes, so the reply finds it again in the list.
 */
struct item_query {
	struct wm_systray *systray;
	struct wm_systray_item *item;
};

/* The item of a reply, or NULL when it is gone; frees the query */
static struct wm_systray_item *
query_take_item(struct item_query *q)
{
	struct wm_systray_item *found = NULL, *it;
	wl_list_for_each(it, &q->systray->items, link) {
		if (it == q->item) {
			found = it;
			break;
		}
	}
	free(q);
	return found;
}

static void
query_property(struct wm_systray_item *item, const char *property,
	sd_bus_message_handler_t handler)
{
	struct item_query *q = malloc(sizeof(*q));
	if (!q) {
		return;
	}
	q->systray = item->systray;
	q->item = item;
	int r = sd_bus_call_method_async(item->systray->bus, NULL,
		item->bus_name, item->object_path,
		"org.freedesktop.DBus.Properties", "Get",
		handler, q,
		"ss", "org.kde.StatusNotifierItem", property);
	if (r < 0) {
		free(q);
	}
	wm_systray_bus_kick(item->systray);
}

/* Draw the item again, and the toolbar around it */
static void
refresh_item(struct wm_systray_item *item)
{
	struct wm_systray *systray = item->systray;

	systray_item_update_scene(item);
	wm_systray_layout(systray, systray->x, systray->y, systray->width,
		systray->height);
	if (systray->server->toolbar) {
		wm_toolbar_relayout(systray->server->toolbar);
	}
}

static int
handle_get_id(sd_bus_message *reply, void *userdata,
	sd_bus_error *ret_error)
{
	struct wm_systray_item *item = query_take_item(userdata);

	if (!item || sd_bus_message_is_method_error(reply, NULL)) {
		return 0;
	}

	const char *val = NULL;
	int r = sd_bus_message_read(reply, "v", "s", &val);
	if (r >= 0 && val) {
		free(item->id);
		item->id = strdup(val);
		wlr_log(WLR_DEBUG, "systray: item %s id=%s",
			item->bus_name, item->id);
		sort_item(item);
	}

	return 0;
}

static int
handle_get_icon_name(sd_bus_message *reply, void *userdata,
	sd_bus_error *ret_error)
{
	struct wm_systray_item *item = query_take_item(userdata);

	if (!item) {
		return 0;
	}

	if (!sd_bus_message_is_method_error(reply, NULL)) {
		const char *val = NULL;
		int r = sd_bus_message_read(reply, "v", "s", &val);
		if (r >= 0 && val) {
			/* Empty: the icon is in IconPixmap (or changed to) */
			free(item->icon_name);
			item->icon_name = *val ? strdup(val) : NULL;
			wlr_log(WLR_DEBUG, "systray: item %s icon_name=%s",
				item->bus_name, val);
		}
	}

	refresh_item(item);
	return 0;
}

static int
handle_get_icon_theme_path(sd_bus_message *reply, void *userdata,
	sd_bus_error *ret_error)
{
	struct wm_systray_item *item = query_take_item(userdata);

	if (!item) {
		return 0;
	}

	if (!sd_bus_message_is_method_error(reply, NULL)) {
		const char *val = NULL;
		int r = sd_bus_message_read(reply, "v", "s", &val);
		if (r >= 0 && val) {
			free(item->icon_theme_path);
			item->icon_theme_path = *val ? strdup(val) : NULL;
			wlr_log(WLR_DEBUG, "systray: item %s icon_theme_path=%s",
				item->bus_name, val);
		}
	}

	/* The icon may be one that is there */
	refresh_item(item);
	return 0;
}

/*
 * IconPixmap's pixels are ARGB, a byte each, most significant first;
 * cairo wants them premultiplied, in the machine's byte order.
 */
static cairo_surface_t *
pixmap_to_surface(const uint8_t *argb, int w, int h)
{
	cairo_surface_t *surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
	if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(surface);
		return NULL;
	}

	cairo_surface_flush(surface);
	uint8_t *dst = cairo_image_surface_get_data(surface);
	int stride = cairo_image_surface_get_stride(surface);
	for (int y = 0; y < h; y++) {
		uint32_t *row = (uint32_t *)(dst + (size_t)y * stride);
		for (int x = 0; x < w; x++) {
			const uint8_t *px = argb + ((size_t)y * w + x) * 4;
			uint32_t a = px[0];
			uint32_t r = (px[1] * a + 127) / 255;
			uint32_t g = (px[2] * a + 127) / 255;
			uint32_t b = (px[3] * a + 127) / 255;
			row[x] = (a << 24) | (r << 16) | (g << 8) | b;
		}
	}
	cairo_surface_mark_dirty(surface);
	return surface;
}

static int
handle_get_icon_pixmap(sd_bus_message *reply, void *userdata,
	sd_bus_error *ret_error)
{
	struct wm_systray_item *item = query_take_item(userdata);

	if (!item || sd_bus_message_is_method_error(reply, NULL)) {
		return 0;
	}

	if (sd_bus_message_enter_container(reply, 'v', "a(iiay)") < 0 ||
	    sd_bus_message_enter_container(reply, 'a', "(iiay)") < 0) {
		return 0;
	}

	/* The smallest picture that covers the icon size, else the largest */
	int want = item->systray->icon_size;
	const uint8_t *best = NULL;
	int best_w = 0, best_h = 0;
	while (sd_bus_message_enter_container(reply, 'r', "iiay") > 0) {
		int32_t w = 0, h = 0;
		const void *data = NULL;
		size_t len = 0;
		if (sd_bus_message_read(reply, "ii", &w, &h) < 0 ||
		    sd_bus_message_read_array(reply, 'y', &data, &len) < 0) {
			break;
		}
		sd_bus_message_exit_container(reply);

		if (w <= 0 || h <= 0 || w > 1024 || h > 1024 ||
		    len != (size_t)w * h * 4) {
			continue;
		}
		bool covers = w >= want && h >= want;
		bool best_covers = best_w >= want && best_h >= want;
		bool better = !best ||
			(covers && (!best_covers || w * h < best_w * best_h)) ||
			(!covers && !best_covers && w * h > best_w * best_h);
		if (better) {
			best = data;
			best_w = w;
			best_h = h;
		}
	}

	/* No picture (any more): the icon is by name */
	if (item->icon_pixmap) {
		cairo_surface_destroy(item->icon_pixmap);
		item->icon_pixmap = NULL;
	}
	if (best) {
		item->icon_pixmap = pixmap_to_surface(best, best_w, best_h);
		wlr_log(WLR_DEBUG, "systray: item %s icon_pixmap %dx%d",
			item->bus_name, best_w, best_h);
	}

	refresh_item(item);
	return 0;
}

/* The text of a tooltip: the title, then the description on its lines */
static void
set_tooltip(struct wm_systray_item *item, const char *title,
	const char *description)
{
	free(item->tooltip);
	item->tooltip = NULL;

	bool has_title = title && *title;
	bool has_description = description && *description;
	if (!has_title && !has_description) {
		return;
	}

	size_t len = (has_title ? strlen(title) : 0) +
		(has_description ? strlen(description) : 0) + 3;
	char *text = malloc(len);
	if (!text) {
		return;
	}
	snprintf(text, len, "%s%s%s", has_title ? title : "",
		has_title && has_description ? "\n" : "",
		has_description ? description : "");
	/* Lines end with \n here (the tooltip draws a line to each) */
	char *out = text;
	for (const char *c = text; *c; c++) {
		if (*c != '\r') {
			*out++ = *c;
		}
	}
	*out = '\0';
	while (out > text && out[-1] == '\n') {
		*--out = '\0';
	}
	item->tooltip = text;
}

static int
handle_get_tooltip(sd_bus_message *reply, void *userdata,
	sd_bus_error *ret_error)
{
	struct wm_systray_item *item = query_take_item(userdata);

	if (!item || sd_bus_message_is_method_error(reply, NULL)) {
		return 0;
	}

	/* (icon name, icon pictures, title, description) */
	const char *title = NULL, *description = NULL;
	if (sd_bus_message_enter_container(reply, 'v', "(sa(iiay)ss)") < 0 ||
	    sd_bus_message_enter_container(reply, 'r', "sa(iiay)ss") < 0 ||
	    sd_bus_message_skip(reply, "s") < 0 ||
	    sd_bus_message_skip(reply, "a(iiay)") < 0 ||
	    sd_bus_message_read(reply, "ss", &title, &description) < 0) {
		return 0;
	}

	set_tooltip(item, title, description);
	return 0;
}

static int
handle_get_title(sd_bus_message *reply, void *userdata,
	sd_bus_error *ret_error)
{
	struct wm_systray_item *item = query_take_item(userdata);

	if (!item || sd_bus_message_is_method_error(reply, NULL)) {
		return 0;
	}

	const char *val = NULL;
	if (sd_bus_message_read(reply, "v", "s", &val) >= 0 && val) {
		free(item->title);
		item->title = *val ? strdup(val) : NULL;
	}
	return 0;
}

static int
handle_get_menu(sd_bus_message *reply, void *userdata, sd_bus_error *ret_error)
{
	struct wm_systray_item *item = query_take_item(userdata);

	if (!item || sd_bus_message_is_method_error(reply, NULL)) {
		return 0;
	}

	const char *val = NULL;
	if (sd_bus_message_read(reply, "v", "o", &val) >= 0 && val) {
		free(item->menu_path);
		/* "/" is what an item without a menu says */
		item->menu_path = strcmp(val, "/") != 0 ? strdup(val) : NULL;
	}
	return 0;
}

static int
handle_get_is_menu(sd_bus_message *reply, void *userdata,
	sd_bus_error *ret_error)
{
	struct wm_systray_item *item = query_take_item(userdata);

	if (!item || sd_bus_message_is_method_error(reply, NULL)) {
		return 0;
	}

	int val = 0;
	if (sd_bus_message_read(reply, "v", "b", &val) >= 0) {
		item->is_menu = val;
	}
	return 0;
}

static void
query_item_properties(struct wm_systray *systray, struct wm_systray_item *item)
{
	query_property(item, "Id", handle_get_id);
	query_property(item, "IconThemePath", handle_get_icon_theme_path);
	query_property(item, "IconName", handle_get_icon_name);
	query_property(item, "IconPixmap", handle_get_icon_pixmap);
	query_property(item, "ToolTip", handle_get_tooltip);
	query_property(item, "Title", handle_get_title);
	query_property(item, "Menu", handle_get_menu);
	query_property(item, "ItemIsMenu", handle_get_is_menu);
}

/*
 * NewIcon, NewToolTip and NewTitle: an item (the battery's charge) says
 * what it shows changed, so we ask again.  The signal's sender is the
 * item's unique name, which is its bus_name when it registered with that.
 */
static struct wm_systray_item *
item_of_signal(struct wm_systray *systray, sd_bus_message *msg)
{
	const char *sender = sd_bus_message_get_sender(msg);
	const char *path = sd_bus_message_get_path(msg);
	if (!sender) {
		return NULL;
	}

	struct wm_systray_item *item;
	wl_list_for_each(item, &systray->items, link) {
		if (strcmp(item->bus_name, sender) == 0 &&
		    (!path || strcmp(item->object_path, path) == 0)) {
			return item;
		}
	}
	return NULL;
}

static int
handle_new_icon(sd_bus_message *msg, void *userdata, sd_bus_error *ret_error)
{
	struct wm_systray_item *item = item_of_signal(userdata, msg);
	if (item) {
		query_property(item, "IconThemePath", handle_get_icon_theme_path);
		query_property(item, "IconName", handle_get_icon_name);
		query_property(item, "IconPixmap", handle_get_icon_pixmap);
	}
	return 0;
}

static int
handle_new_tooltip(sd_bus_message *msg, void *userdata,
	sd_bus_error *ret_error)
{
	struct wm_systray_item *item = item_of_signal(userdata, msg);
	if (item) {
		query_property(item, "ToolTip", handle_get_tooltip);
	}
	return 0;
}

static int
handle_new_title(sd_bus_message *msg, void *userdata,
	sd_bus_error *ret_error)
{
	struct wm_systray_item *item = item_of_signal(userdata, msg);
	if (item) {
		query_property(item, "Title", handle_get_title);
	}
	return 0;
}

/* --- StatusNotifierWatcher D-Bus interface --- */

static int
method_register_item(sd_bus_message *msg, void *userdata,
	sd_bus_error *ret_error)
{
	struct wm_systray *systray = userdata;
	const char *service = NULL;

	int r = sd_bus_message_read(msg, "s", &service);
	if (r < 0) {
		return r;
	}

	/*
	 * The service argument can be:
	 * - A bus name like ":1.42" (unique connection name)
	 * - A well-known name like "org.kde.StatusNotifierItem-PID-ID"
	 * - Just an object path like "/StatusNotifierItem"
	 */
	const char *bus_name;
	const char *object_path = "/StatusNotifierItem";

	if (service[0] == '/') {
		/* It's an object path; use the sender as bus name */
		bus_name = sd_bus_message_get_sender(msg);
		object_path = service;
	} else {
		bus_name = service;
	}

	if (!bus_name) {
		return sd_bus_reply_method_errorf(msg,
			SD_BUS_ERROR_INVALID_ARGS,
			"Cannot determine bus name");
	}

	/* Check for duplicate */
	if (find_item_by_bus_name(systray, bus_name)) {
		wlr_log(WLR_DEBUG, "systray: item %s already registered",
			bus_name);
		return sd_bus_reply_method_return(msg, "");
	}

	/* Create new item */
	struct wm_systray_item *item = calloc(1, sizeof(*item));
	if (!item) {
		return -ENOMEM;
	}

	item->systray = systray;
	item->bus_name = strdup(bus_name);
	item->object_path = strdup(object_path);
	if (!item->bus_name || !item->object_path) {
		free(item->bus_name);
		free(item->object_path);
		free(item);
		return -ENOMEM;
	}

	wl_list_insert(systray->items.prev, &item->link);
	systray->item_count++;

	wlr_log(WLR_INFO, "systray: registered item %s at %s",
		bus_name, object_path);

	/* Query item properties asynchronously */
	query_item_properties(systray, item);

	/* Emit StatusNotifierItemRegistered signal */
	sd_bus_emit_signal(systray->bus,
		"/StatusNotifierWatcher",
		"org.kde.StatusNotifierWatcher",
		"StatusNotifierItemRegistered",
		"s", bus_name);

	return sd_bus_reply_method_return(msg, "");
}

static int
method_register_host(sd_bus_message *msg, void *userdata,
	sd_bus_error *ret_error)
{
	/* We are the host — just acknowledge */
	return sd_bus_reply_method_return(msg, "");
}

static int
property_get_registered_items(sd_bus *bus, const char *path,
	const char *interface, const char *property,
	sd_bus_message *reply, void *userdata,
	sd_bus_error *ret_error)
{
	struct wm_systray *systray = userdata;

	int r = sd_bus_message_open_container(reply, 'a', "s");
	if (r < 0) return r;

	struct wm_systray_item *item;
	wl_list_for_each(item, &systray->items, link) {
		r = sd_bus_message_append(reply, "s", item->bus_name);
		if (r < 0) return r;
	}

	return sd_bus_message_close_container(reply);
}

static int
property_get_host_registered(sd_bus *bus, const char *path,
	const char *interface, const char *property,
	sd_bus_message *reply, void *userdata,
	sd_bus_error *ret_error)
{
	return sd_bus_message_append(reply, "b", true);
}

static int
property_get_protocol_version(sd_bus *bus, const char *path,
	const char *interface, const char *property,
	sd_bus_message *reply, void *userdata,
	sd_bus_error *ret_error)
{
	return sd_bus_message_append(reply, "i", 0);
}

static const sd_bus_vtable watcher_vtable[] = {
	SD_BUS_VTABLE_START(0),
	SD_BUS_METHOD("RegisterStatusNotifierItem", "s", "",
		method_register_item,
		SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("RegisterStatusNotifierHost", "s", "",
		method_register_host,
		SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_PROPERTY("RegisteredStatusNotifierItems", "as",
		property_get_registered_items,
		0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
	SD_BUS_PROPERTY("IsStatusNotifierHostRegistered", "b",
		property_get_host_registered,
		0, SD_BUS_VTABLE_PROPERTY_CONST),
	SD_BUS_PROPERTY("ProtocolVersion", "i",
		property_get_protocol_version,
		0, SD_BUS_VTABLE_PROPERTY_CONST),
	SD_BUS_SIGNAL("StatusNotifierItemRegistered", "s", 0),
	SD_BUS_SIGNAL("StatusNotifierItemUnregistered", "s", 0),
	SD_BUS_VTABLE_END,
};

/* --- NameOwnerChanged monitor for item disappearance --- */

static int
handle_name_owner_changed(sd_bus_message *msg, void *userdata,
	sd_bus_error *ret_error)
{
	struct wm_systray *systray = userdata;
	const char *name = NULL;
	const char *old_owner = NULL;
	const char *new_owner = NULL;

	int r = sd_bus_message_read(msg, "sss", &name, &old_owner, &new_owner);
	if (r < 0) {
		return 0;
	}

	/* Only interested when a name disappears (new_owner is empty) */
	if (!new_owner || new_owner[0] != '\0') {
		return 0;
	}
	if (!old_owner || old_owner[0] == '\0') {
		return 0;
	}

	/* Check if this was a registered tray item */
	struct wm_systray_item *item, *tmp;
	wl_list_for_each_safe(item, tmp, &systray->items, link) {
		if (strcmp(item->bus_name, name) == 0 ||
		    strcmp(item->bus_name, old_owner) == 0) {
			wlr_log(WLR_INFO, "systray: item %s vanished",
				item->bus_name);

			/* Emit unregistered signal */
			sd_bus_emit_signal(systray->bus,
				"/StatusNotifierWatcher",
				"org.kde.StatusNotifierWatcher",
				"StatusNotifierItemUnregistered",
				"s", item->bus_name);

			systray_item_destroy(item);

			/* Relayout */
			wm_systray_layout(systray, systray->x, systray->y,
				systray->width, systray->height);
			if (systray->server->toolbar) {
				wm_toolbar_relayout(
					systray->server->toolbar);
			}
			break;
		}
	}

	return 0;
}

/* --- wl_event_loop integration --- */

/*
 * What sd-bus wants of the event loop changes with what it has to send and
 * wait for: readable always, writable only while a message is half sent,
 * and a timer for its method call timeouts.  (A socket is nearly always
 * writable: asking for that all the time wakes the loop without end.)
 */
static void
bus_sync_sources(struct wm_systray *systray)
{
	if (!systray->bus || !systray->bus_event)
		return;
	int events = sd_bus_get_events(systray->bus);
	if (events < 0)
		return;
	uint32_t mask = WL_EVENT_HANGUP | WL_EVENT_ERROR;
	if (events & POLLIN)
		mask |= WL_EVENT_READABLE;
	if (events & POLLOUT)
		mask |= WL_EVENT_WRITABLE;
	wl_event_source_fd_update(systray->bus_event, mask);

	if (!systray->bus_timer)
		return;
	uint64_t usec = UINT64_MAX;
	if (sd_bus_get_timeout(systray->bus, &usec) < 0 ||
	    usec == UINT64_MAX) {
		wl_event_source_timer_update(systray->bus_timer, 0);
		return;
	}
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	uint64_t now_usec = (uint64_t)now.tv_sec * 1000000 +
		(uint64_t)now.tv_nsec / 1000;
	uint64_t wait = usec > now_usec ? usec - now_usec : 0;
	uint64_t ms = (wait + 999) / 1000;
	if (ms < 1)
		ms = 1;
	if (ms > INT32_MAX)
		ms = INT32_MAX;
	wl_event_source_timer_update(systray->bus_timer, (int)ms);
}

static int
bus_dispatch(int fd, uint32_t mask, void *data)
{
	struct wm_systray *systray = data;

	/* Process all pending D-Bus messages */
	int r;
	do {
		r = sd_bus_process(systray->bus, NULL);
	} while (r > 0);

	if (r < 0) {
		wlr_log(WLR_ERROR, "systray: sd_bus_process failed: %s",
			strerror(-r));
	}

	bus_sync_sources(systray);
	return 0;
}

static int
bus_timeout(void *data)
{
	bus_dispatch(-1, 0, data);
	return 0;
}

static void
bus_idle_sync(void *data)
{
	struct wm_systray *systray = data;
	systray->bus_idle = NULL;
	bus_sync_sources(systray);
}

void
wm_systray_bus_kick(struct wm_systray *systray)
{
	/* Something was sent: whether it all went is for the next sync */
	if (systray && !systray->bus_idle && systray->server &&
	    systray->server->wl_event_loop)
		systray->bus_idle = wl_event_loop_add_idle(
			systray->server->wl_event_loop, bus_idle_sync,
			systray);
}

/* --- Public API --- */

struct wm_systray *
wm_systray_create(struct wm_server *server)
{
	struct wm_systray *systray = calloc(1, sizeof(*systray));
	if (!systray) {
		return NULL;
	}

	systray->server = server;
	systray->icon_size = WM_SYSTRAY_ICON_SIZE;
	wl_list_init(&systray->items);
	wl_list_init(&systray->menu_requests);

	/* Connect to session bus */
	int r = sd_bus_open_user(&systray->bus);
	if (r < 0) {
		wlr_log(WLR_ERROR, "systray: failed to connect to session "
			"D-Bus: %s", strerror(-r));
		free(systray);
		return NULL;
	}

	/* Register StatusNotifierWatcher interface */
	r = sd_bus_add_object_vtable(systray->bus, &systray->watcher_slot,
		"/StatusNotifierWatcher",
		"org.kde.StatusNotifierWatcher",
		watcher_vtable, systray);
	if (r < 0) {
		wlr_log(WLR_ERROR, "systray: failed to add vtable: %s",
			strerror(-r));
		sd_bus_unref(systray->bus);
		free(systray);
		return NULL;
	}

	/* Request the watcher bus name */
	r = sd_bus_request_name(systray->bus,
		"org.kde.StatusNotifierWatcher",
		SD_BUS_NAME_REPLACE_EXISTING |
		SD_BUS_NAME_ALLOW_REPLACEMENT | SD_BUS_NAME_QUEUE);
	/* (Queued: asked to be the owner of a name that another keeps; and a
	 * tray that later takes it from us, and goes, gives it back.) */
	if (r <= 0) {
		wlr_log(WLR_INFO, "systray: could not acquire "
			"org.kde.StatusNotifierWatcher: %s (another tray "
			"may be running)", r < 0 ? strerror(-r) : "in use");
		sd_bus_slot_unref(systray->watcher_slot);
		sd_bus_unref(systray->bus);
		free(systray);
		return NULL;
	}

	/* Monitor NameOwnerChanged for item disappearance */
	r = sd_bus_match_signal(systray->bus, &systray->name_slot,
		"org.freedesktop.DBus",
		"/org/freedesktop/DBus",
		"org.freedesktop.DBus",
		"NameOwnerChanged",
		handle_name_owner_changed, systray);
	if (r < 0) {
		wlr_log(WLR_ERROR, "systray: failed to add match: %s",
			strerror(-r));
		/* Non-fatal, continue without disappearance tracking */
	}

	/* The icon, tooltip and title of an item changing (non-fatal too) */
	static const struct {
		const char *member;
		sd_bus_message_handler_t handler;
	} signals[] = {
		{ "NewIcon", handle_new_icon },
		{ "NewToolTip", handle_new_tooltip },
		{ "NewTitle", handle_new_title },
	};
	for (size_t i = 0; i < sizeof(signals) / sizeof(signals[0]); i++) {
		r = sd_bus_match_signal(systray->bus,
			&systray->signal_slots[i], NULL, NULL,
			"org.kde.StatusNotifierItem", signals[i].member,
			signals[i].handler, systray);
		if (r < 0) {
			wlr_log(WLR_ERROR, "systray: failed to watch %s: %s",
				signals[i].member, strerror(-r));
		}
	}

	/* Integrate sd-bus fd with wayland event loop */
	int fd = sd_bus_get_fd(systray->bus);
	if (fd >= 0) {
		systray->bus_event = wl_event_loop_add_fd(
			server->wl_event_loop, fd,
			WL_EVENT_READABLE | WL_EVENT_HANGUP | WL_EVENT_ERROR,
			bus_dispatch, systray);
		systray->bus_timer = wl_event_loop_add_timer(
			server->wl_event_loop, bus_timeout, systray);
		bus_sync_sources(systray);
	}

	/* Create scene tree in layer_top (same parent as toolbar) */
	systray->scene_tree = wlr_scene_tree_create(server->layer_top);

	wlr_log(WLR_INFO, "%s",
		"systray: StatusNotifierWatcher active on D-Bus");
	return systray;
}

void
wm_systray_destroy(struct wm_systray *systray)
{
	if (!systray) {
		return;
	}

	wm_systray_menu_cancel_all(systray);

	/* Destroy all items */
	struct wm_systray_item *item, *tmp;
	wl_list_for_each_safe(item, tmp, &systray->items, link) {
		systray_item_destroy(item);
	}

	if (systray->bus_idle) {
		wl_event_source_remove(systray->bus_idle);
	}
	if (systray->bus_timer) {
		wl_event_source_remove(systray->bus_timer);
	}
	if (systray->bus_event) {
		wl_event_source_remove(systray->bus_event);
	}

	if (systray->name_slot) {
		sd_bus_slot_unref(systray->name_slot);
	}
	if (systray->watcher_slot) {
		sd_bus_slot_unref(systray->watcher_slot);
	}
	for (size_t i = 0; i < sizeof(systray->signal_slots) /
	     sizeof(systray->signal_slots[0]); i++) {
		sd_bus_slot_unref(systray->signal_slots[i]);
	}
	if (systray->bus) {
		sd_bus_release_name(systray->bus,
			"org.kde.StatusNotifierWatcher");
		sd_bus_flush_close_unref(systray->bus);
	}

	if (systray->scene_tree) {
		wlr_scene_node_destroy(&systray->scene_tree->node);
	}

	free(systray);
}

void
wm_systray_layout(struct wm_systray *systray, int x, int y,
	int max_width, int height)
{
	if (!systray) {
		return;
	}

	systray->x = x;
	systray->y = y;
	systray->height = height;

	int icon_size = systray->icon_size;
	int padding = WM_SYSTRAY_ICON_PADDING;
	int offset = padding;

	struct wm_systray_item *item;
	wl_list_for_each(item, &systray->items, link) {
		if (!item->icon_buf) {
			continue;
		}

		int iy = (height - icon_size) / 2;
		if (iy < 0) iy = 0;

		/* Down a turned toolbar, height is its thickness */
		if (systray->vertical)
			wlr_scene_node_set_position(&item->icon_buf->node,
				iy, offset);
		else
			wlr_scene_node_set_position(&item->icon_buf->node,
				offset, iy);
		wlr_scene_node_set_enabled(&item->icon_buf->node, true);

		offset += icon_size + padding;
	}

	systray->width = offset;

	/* Position the scene tree */
	if (systray->scene_tree) {
		wlr_scene_node_set_position(&systray->scene_tree->node,
			x, y);
	}
}

int
wm_systray_get_width(struct wm_systray *systray)
{
	if (!systray || systray->item_count == 0) {
		return 0;
	}

	int padding = WM_SYSTRAY_ICON_PADDING;
	return systray->item_count * (systray->icon_size + padding) + padding;
}

/*
 * The icon at (local_x, local_y), from the tray's top left corner: where
 * wm_systray_layout puts them, which is down the tray on a turned toolbar.
 */
static struct wm_systray_item *
item_at(struct wm_systray *systray, double local_x, double local_y)
{
	int padding = WM_SYSTRAY_ICON_PADDING;
	int icon_size = systray->icon_size;
	int offset = padding;
	int iy = (systray->height - icon_size) / 2;
	if (iy < 0) iy = 0;

	struct wm_systray_item *item;
	wl_list_for_each(item, &systray->items, link) {
		if (!item->icon_buf) {
			continue;
		}

		int ix = systray->vertical ? iy : offset;
		int jy = systray->vertical ? offset : iy;
		if (local_x >= ix && local_x < ix + icon_size &&
		    local_y >= jy && local_y < jy + icon_size) {
			return item;
		}

		offset += icon_size + padding;
	}

	return NULL;
}

/*
 * The user just acted on an item (a click on it, or on an entry of its
 * menu): give it an xdg-activation token of that click, as KDE's
 * StatusNotifierItem has its host do (ProvideXdgActivationToken), so that
 * what it does about it, such as raising and focusing its window, is the
 * user's doing and not a window taking the focus by itself.  The token
 * belongs to the seat, as a client's token from an input event does, and
 * goes unused (and expires) with an item that has no such method; calls on
 * the bus arrive in order, so it is there for the one that follows.
 */
void
wm_systray_provide_token(struct wm_systray *systray, const char *bus_name,
	const char *object_path)
{
	const char *token = wm_activation_token_create(systray->server);
	if (!token) {
		return;
	}
	sd_bus_call_method_async(systray->bus, NULL, bus_name, object_path,
		"org.kde.StatusNotifierItem", "ProvideXdgActivationToken",
		NULL, NULL, "s", token);
}

/* The same for an entry of an item's menu (dbusmenu), found by the menu */
void
wm_systray_provide_menu_token(struct wm_systray *systray,
	const char *bus_name, const char *menu_path)
{
	struct wm_systray_item *item;
	wl_list_for_each(item, &systray->items, link) {
		if (item->bus_name && item->menu_path &&
		    strcmp(item->bus_name, bus_name) == 0 &&
		    strcmp(item->menu_path, menu_path) == 0) {
			wm_systray_provide_token(systray, item->bus_name,
				item->object_path);
			return;
		}
	}
}

/*
 * systray.activationGrace: a click was passed on to a program, which will
 * ask for its window to be raised on its own, with nothing to show it was
 * asked to.  Until the call that passed it on is answered (and a grace
 * after) a window of that program may take the focus: the reply ends the
 * grant, with id (0: none) as its user data.
 */
uint64_t
wm_systray_grant_begin(struct wm_systray *systray, const char *bus_name)
{
	struct wm_config *config = systray->server->config;
	if (!config || config->systray_activation_grace_ms <= 0) {
		return 0;
	}
	sd_bus_creds *creds = NULL;
	pid_t pid = 0;
	if (sd_bus_get_name_creds(systray->bus, bus_name, SD_BUS_CREDS_PID,
		    &creds) >= 0) {
		sd_bus_creds_get_pid(creds, &pid);
		sd_bus_creds_unref(creds);
	}
	return wm_tray_grant_begin(pid, wm_tray_grant_now_ms(),
		config->systray_activation_grace_ms);
}

int
wm_systray_grant_reply(sd_bus_message *reply, void *userdata,
	sd_bus_error *ret_error)
{
	(void)reply;
	(void)ret_error;
	wm_tray_grant_end((uint64_t)(uintptr_t)userdata,
		wm_tray_grant_now_ms());
	return 0;
}

void
wm_systray_call_item_method(struct wm_systray *systray, const char *bus_name,
	const char *object_path, const char *method, int x, int y)
{
	wm_systray_provide_token(systray, bus_name, object_path);
	uint64_t grant = wm_systray_grant_begin(systray, bus_name);
	sd_bus_call_method_async(systray->bus, NULL, bus_name, object_path,
		"org.kde.StatusNotifierItem", method,
		grant ? wm_systray_grant_reply : NULL,
		grant ? (void *)(uintptr_t)grant : NULL,
		"ii", (int32_t)x, (int32_t)y);
	wm_systray_bus_kick(systray);
	wlr_log(WLR_DEBUG, "systray: %s on %s", method, bus_name);
}

/* The left button on an item that has a menu and may have no Activate */
struct activate_query {
	struct item_query q;	/* first: query_take_item() frees it all */
	int x, y;
	uint64_t grant;		/* systray.activationGrace's, ended by the reply */
};

static int
handle_activate_reply(sd_bus_message *reply, void *userdata,
	sd_bus_error *ret_error)
{
	struct activate_query *aq = userdata;
	struct wm_systray *systray = aq->q.systray;
	int x = aq->x, y = aq->y;
	wm_tray_grant_end(aq->grant, wm_tray_grant_now_ms());
	struct wm_systray_item *item = query_take_item(&aq->q);
	if (!item || !item->menu_path)
		return 0;

	/*
	 * It has no Activate: libappindicator's items have only a menu (and a
	 * secondary action), and it is for the tray to show it
	 */
	if (sd_bus_message_is_method_error(reply,
		    "org.freedesktop.DBus.Error.UnknownMethod") ||
	    sd_bus_message_is_method_error(reply,
		    "org.freedesktop.DBus.Error.UnknownInterface") ||
	    sd_bus_message_is_method_error(reply,
		    "org.freedesktop.DBus.Error.UnknownObject"))
		wm_systray_menu_open(systray, item->bus_name,
			item->object_path, item->menu_path,
			item->title && *item->title ? item->title : item->id,
			x, y);
	return 0;
}

bool
wm_systray_handle_click(struct wm_systray *systray,
	double local_x, double local_y, uint32_t button)
{
	if (!systray || systray->item_count == 0) {
		return false;
	}

	struct wm_systray_item *item = item_at(systray, local_x, local_y);
	if (!item) {
		return false;
	}

	/* The pointer's place, for the item's menu or window */
	int x = (int)(systray->x + local_x);
	int y = (int)(systray->y + local_y);

	/*
	 * An application with a menu on its icon doesn't draw it: it
	 * expects the tray to (right button, or the left one for an icon
	 * that is only a menu)
	 */
	if (item->menu_path && (button == 0x111 /* BTN_RIGHT */ ||
	    (button == 0x110 /* BTN_LEFT */ && item->is_menu))) {
		wm_systray_menu_open(systray, item->bus_name,
			item->object_path, item->menu_path,
			item->title && *item->title ? item->title : item->id,
			x, y);
		return true;
	}

	/*
	 * An icon with a menu that is not only a menu: Activate it, and if the
	 * application has no such method the menu is what the click is for
	 */
	if (button == 0x110 /* BTN_LEFT */ && item->menu_path) {
		struct activate_query *aq = malloc(sizeof(*aq));
		if (aq) {
			aq->q.systray = systray;
			aq->q.item = item;
			aq->x = x;
			aq->y = y;
			wm_systray_provide_token(systray, item->bus_name,
				item->object_path);
			aq->grant = wm_systray_grant_begin(systray,
				item->bus_name);
			if (sd_bus_call_method_async(systray->bus, NULL,
				    item->bus_name, item->object_path,
				    "org.kde.StatusNotifierItem", "Activate",
				    handle_activate_reply, aq, "ii",
				    (int32_t)x, (int32_t)y) >= 0) {
				wm_systray_bus_kick(systray);
				return true;
			}
			free(aq);
		}
	}

	const char *method = NULL;
	if (button == 0x110) { /* BTN_LEFT */
		method = "Activate";
	} else if (button == 0x111) { /* BTN_RIGHT */
		method = "ContextMenu";
	} else if (button == 0x112) { /* BTN_MIDDLE */
		method = "SecondaryActivate";
	}

	if (method) {
		wm_systray_call_item_method(systray, item->bus_name,
			item->object_path, method, x, y);
	}
	return true;
}

const char *
wm_systray_tooltip_at(struct wm_systray *systray, double lx, double ly)
{
	if (!systray || systray->item_count == 0) {
		return NULL;
	}

	struct wm_systray_item *item =
		item_at(systray, lx - systray->x, ly - systray->y);
	if (!item) {
		return NULL;
	}
	return item->tooltip ? item->tooltip :
		item->title ? item->title : item->id;
}

#endif /* WM_HAS_SYSTRAY */
