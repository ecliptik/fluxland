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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <systemd/sd-bus.h>
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/log.h>

#include "systray.h"
#include "server.h"
#include "toolbar.h"

/* --- Cairo-to-wlr_buffer bridge (same pattern as toolbar.c) --- */

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

/* --- Icon loading helpers --- */

static bool
file_exists(const char *path)
{
	struct stat st;
	return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

/*
 * Try to load an icon by name from standard icon directories.
 * Returns a cairo surface scaled to icon_size, or NULL.
 */
static cairo_surface_t *
load_icon_by_name(const char *icon_name, int icon_size)
{
	if (!icon_name || !*icon_name) {
		return NULL;
	}

	/* If the icon_name is an absolute path, try it directly */
	if (icon_name[0] == '/') {
		if (file_exists(icon_name)) {
			return cairo_image_surface_create_from_png(icon_name);
		}
		return NULL;
	}

	/* Search standard icon paths */
	const char *search_paths[] = {
		"/usr/share/pixmaps/%s.png",
		"/usr/share/icons/hicolor/24x24/apps/%s.png",
		"/usr/share/icons/hicolor/48x48/apps/%s.png",
		"/usr/share/icons/hicolor/scalable/apps/%s.svg",
		"/usr/share/icons/hicolor/22x22/apps/%s.png",
		"/usr/share/icons/hicolor/32x32/apps/%s.png",
	};

	char path[512];
	for (size_t i = 0; i < sizeof(search_paths) / sizeof(search_paths[0]); i++) {
		snprintf(path, sizeof(path), search_paths[i], icon_name);
		if (file_exists(path)) {
			/* Only load PNG files (skip SVG) */
			const char *ext = strrchr(path, '.');
			if (ext && strcmp(ext, ".svg") == 0) {
				continue;
			}
			cairo_surface_t *img =
				cairo_image_surface_create_from_png(path);
			if (cairo_surface_status(img) == CAIRO_STATUS_SUCCESS) {
				wlr_log(WLR_DEBUG, "systray: loaded icon %s",
					path);
				return img;
			}
			cairo_surface_destroy(img);
		}
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
		icon = load_icon_by_name(item->icon_name, size);
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

/* --- D-Bus property query callbacks --- */

static int
handle_get_id(sd_bus_message *reply, void *userdata,
	sd_bus_error *ret_error)
{
	struct wm_systray_item *item = userdata;

	if (sd_bus_message_is_method_error(reply, NULL)) {
		return 0;
	}

	const char *val = NULL;
	int r = sd_bus_message_read(reply, "v", "s", &val);
	if (r >= 0 && val) {
		free(item->id);
		item->id = strdup(val);
		wlr_log(WLR_DEBUG, "systray: item %s id=%s",
			item->bus_name, item->id);
	}

	return 0;
}

static int
handle_get_icon_name(sd_bus_message *reply, void *userdata,
	sd_bus_error *ret_error)
{
	struct wm_systray_item *item = userdata;

	if (sd_bus_message_is_method_error(reply, NULL)) {
		systray_item_update_scene(item);
		wm_systray_layout(item->systray, item->systray->x,
			item->systray->y, item->systray->width,
			item->systray->height);
		return 0;
	}

	const char *val = NULL;
	int r = sd_bus_message_read(reply, "v", "s", &val);
	if (r >= 0 && val && *val) {
		free(item->icon_name);
		item->icon_name = strdup(val);
		wlr_log(WLR_DEBUG, "systray: item %s icon_name=%s",
			item->bus_name, item->icon_name);
	}

	/* Now update the icon scene */
	systray_item_update_scene(item);
	wm_systray_layout(item->systray, item->systray->x,
		item->systray->y, item->systray->width,
		item->systray->height);

	/* Trigger toolbar relayout */
	if (item->systray->server->toolbar) {
		wm_toolbar_relayout(item->systray->server->toolbar);
	}

	return 0;
}

static void
query_item_properties(struct wm_systray *systray, struct wm_systray_item *item)
{
	/* Query Id property */
	sd_bus_call_method_async(systray->bus, NULL,
		item->bus_name, item->object_path,
		"org.freedesktop.DBus.Properties", "Get",
		handle_get_id, item,
		"ss", "org.kde.StatusNotifierItem", "Id");

	/* Query IconName property */
	sd_bus_call_method_async(systray->bus, NULL,
		item->bus_name, item->object_path,
		"org.freedesktop.DBus.Properties", "Get",
		handle_get_icon_name, item,
		"ss", "org.kde.StatusNotifierItem", "IconName");
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

	return 0;
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
		SD_BUS_NAME_ALLOW_REPLACEMENT);
	if (r < 0) {
		wlr_log(WLR_INFO, "systray: could not acquire "
			"org.kde.StatusNotifierWatcher: %s (another tray "
			"may be running)", strerror(-r));
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

	/* Integrate sd-bus fd with wayland event loop */
	int fd = sd_bus_get_fd(systray->bus);
	if (fd >= 0) {
		systray->bus_event = wl_event_loop_add_fd(
			server->wl_event_loop, fd,
			WL_EVENT_READABLE | WL_EVENT_WRITABLE |
			WL_EVENT_HANGUP | WL_EVENT_ERROR,
			bus_dispatch, systray);
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

	/* Destroy all items */
	struct wm_systray_item *item, *tmp;
	wl_list_for_each_safe(item, tmp, &systray->items, link) {
		systray_item_destroy(item);
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

bool
wm_systray_handle_click(struct wm_systray *systray,
	double local_x, double local_y, uint32_t button)
{
	if (!systray || systray->item_count == 0) {
		return false;
	}

	int padding = WM_SYSTRAY_ICON_PADDING;
	int icon_size = systray->icon_size;
	int offset = padding;

	struct wm_systray_item *item;
	wl_list_for_each(item, &systray->items, link) {
		if (!item->icon_buf) {
			continue;
		}

		int iy = (systray->height - icon_size) / 2;
		if (iy < 0) iy = 0;

		if (local_x >= offset && local_x < offset + icon_size &&
		    local_y >= iy && local_y < iy + icon_size) {
			/* Found the clicked item */
			const char *method = NULL;

			if (button == 0x110) { /* BTN_LEFT */
				method = "Activate";
			} else if (button == 0x111) { /* BTN_RIGHT */
				method = "ContextMenu";
			} else if (button == 0x112) { /* BTN_MIDDLE */
				method = "SecondaryActivate";
			}

			if (method) {
				sd_bus_call_method_async(systray->bus,
					NULL,
					item->bus_name,
					item->object_path,
					"org.kde.StatusNotifierItem",
					method,
					NULL, NULL,
					"ii", 0, 0);
				wlr_log(WLR_DEBUG, "systray: %s on %s",
					method, item->bus_name);
			}
			return true;
		}

		offset += icon_size + padding;
	}

	return false;
}

#endif /* WM_HAS_SYSTRAY */
