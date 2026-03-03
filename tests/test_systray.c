/*
 * test_systray.c - Unit tests for system tray layout, hit-testing, and icon management
 *
 * Includes systray.c directly with stub implementations to avoid needing
 * wlroots/wayland/cairo libraries at link time. The real systemd/sd-bus
 * headers are used for type definitions and vtable macros, but all sd-bus
 * functions are stubbed.
 */

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Enable systray code paths */
#ifndef WM_HAS_SYSTRAY
#define WM_HAS_SYSTRAY
#endif

/* --- Block real headers via their include guards --- */
/* wlroots unstable headers check this before guard, so define it */
#ifndef WLR_USE_UNSTABLE
#define WLR_USE_UNSTABLE
#endif

#define WAYLAND_SERVER_CORE_H
#define WAYLAND_SERVER_H
#define WAYLAND_UTIL_H
#define WLR_UTIL_LOG_H
#define WLR_TYPES_WLR_SCENE_H
#define WLR_TYPES_WLR_BUFFER_H
#define WLR_INTERFACES_WLR_BUFFER_H
#define WLR_UTIL_BOX_H
#define WM_SERVER_H
#define WM_TOOLBAR_H
#define WM_SYSTRAY_H
#define DRM_FOURCC_H
#define CAIRO_H

/* Wayland event loop constants needed by systray.c */
#define WL_EVENT_READABLE 0x01
#define WL_EVENT_WRITABLE 0x02
#define WL_EVENT_HANGUP   0x04
#define WL_EVENT_ERROR    0x08

/* --- Stub wayland types --- */

struct wl_list {
	struct wl_list *prev;
	struct wl_list *next;
};

static inline void
wl_list_init(struct wl_list *list)
{
	list->prev = list;
	list->next = list;
}

static inline void
wl_list_insert(struct wl_list *list, struct wl_list *elm)
{
	elm->prev = list;
	elm->next = list->next;
	list->next = elm;
	elm->next->prev = elm;
}

static inline void
wl_list_remove(struct wl_list *elm)
{
	elm->prev->next = elm->next;
	elm->next->prev = elm->prev;
	elm->prev = NULL;
	elm->next = NULL;
}

static inline int
wl_list_empty(const struct wl_list *list)
{
	return list->next == list;
}

#include <stddef.h>
#define wl_container_of(ptr, sample, member) \
	(__typeof__(sample))((char *)(ptr) - offsetof(__typeof__(*sample), member))

#define wl_list_for_each(pos, head, member) \
	for (pos = wl_container_of((head)->next, pos, member); \
	     &pos->member != (head); \
	     pos = wl_container_of(pos->member.next, pos, member))

#define wl_list_for_each_safe(pos, tmp, head, member) \
	for (pos = wl_container_of((head)->next, pos, member), \
	     tmp = wl_container_of(pos->member.next, tmp, member); \
	     &pos->member != (head); \
	     pos = tmp, \
	     tmp = wl_container_of(pos->member.next, tmp, member))

struct wl_event_source {
	int dummy;
};

struct wl_event_loop {
	int dummy;
};

/* --- Stub wlr types --- */

struct wlr_scene_node {
	int enabled;
	int x, y;
};

struct wlr_scene_tree {
	struct wlr_scene_node node;
};

struct wlr_scene_buffer {
	struct wlr_scene_node node;
};

struct wlr_box {
	int x, y, width, height;
};

/* wlr_buffer stubs */
#define WLR_BUFFER_DATA_PTR_ACCESS_WRITE 2

struct wlr_buffer {
	int width, height;
	int ref_count;
};

struct wlr_buffer_impl {
	void (*destroy)(struct wlr_buffer *);
	bool (*begin_data_ptr_access)(struct wlr_buffer *, uint32_t,
		void **, uint32_t *, size_t *);
	void (*end_data_ptr_access)(struct wlr_buffer *);
};

static void
wlr_buffer_init(struct wlr_buffer *buf, const struct wlr_buffer_impl *impl,
	int width, int height)
{
	(void)impl;
	buf->width = width;
	buf->height = height;
	buf->ref_count = 1;
}

static void
wlr_buffer_drop(struct wlr_buffer *buf)
{
	(void)buf;
	/* no-op in test */
}

/* wlr_log no-op */
#define wlr_log(verb, fmt, ...) ((void)0)
#define WLR_ERROR 1
#define WLR_INFO 2
#define WLR_DEBUG 7

/* DRM format constant */
#define DRM_FORMAT_ARGB8888 0x34325241

/* --- Stub cairo types and functions --- */

typedef enum {
	CAIRO_FORMAT_ARGB32 = 0,
} cairo_format_t;

typedef enum {
	CAIRO_STATUS_SUCCESS = 0,
	CAIRO_STATUS_INVALID_SIZE,
} cairo_status_t;

typedef struct _cairo_surface cairo_surface_t;
typedef struct _cairo cairo_t;

struct _cairo_surface {
	int width, height;
	int stride;
	unsigned char *data;
	int refcount;
	cairo_status_t status;
};

struct _cairo {
	cairo_surface_t *target;
};

/* Global tracking for cairo calls */
static int g_cairo_surface_create_count;
static int g_cairo_destroy_count;
static int g_cairo_surface_destroy_count;

static cairo_surface_t *
cairo_image_surface_create(cairo_format_t format, int width, int height)
{
	(void)format;
	g_cairo_surface_create_count++;
	cairo_surface_t *s = calloc(1, sizeof(*s));
	s->width = width;
	s->height = height;
	s->stride = width * 4;
	s->data = calloc(1, (size_t)(s->stride * height));
	s->refcount = 1;
	s->status = CAIRO_STATUS_SUCCESS;
	return s;
}

static cairo_surface_t *
cairo_image_surface_create_from_png(const char *filename)
{
	(void)filename;
	/* In tests we never actually load PNG files */
	cairo_surface_t *s = calloc(1, sizeof(*s));
	s->width = 0;
	s->height = 0;
	s->stride = 0;
	s->data = NULL;
	s->refcount = 1;
	s->status = CAIRO_STATUS_INVALID_SIZE;
	return s;
}

static int
cairo_image_surface_get_width(cairo_surface_t *s)
{
	return s ? s->width : 0;
}

static int
cairo_image_surface_get_height(cairo_surface_t *s)
{
	return s ? s->height : 0;
}

static int
cairo_image_surface_get_stride(cairo_surface_t *s)
{
	return s ? s->stride : 0;
}

static unsigned char *
cairo_image_surface_get_data(cairo_surface_t *s)
{
	return s ? s->data : NULL;
}

static cairo_status_t
cairo_surface_status(cairo_surface_t *s)
{
	return s ? s->status : CAIRO_STATUS_INVALID_SIZE;
}

static void
cairo_surface_flush(cairo_surface_t *s)
{
	(void)s;
}

static void
cairo_surface_destroy(cairo_surface_t *s)
{
	if (!s) return;
	g_cairo_surface_destroy_count++;
	s->refcount--;
	if (s->refcount <= 0) {
		free(s->data);
		free(s);
	}
}

static void
cairo_surface_reference(cairo_surface_t *s)
{
	if (s) s->refcount++;
}

static cairo_t *
cairo_create(cairo_surface_t *s)
{
	cairo_t *cr = calloc(1, sizeof(*cr));
	cr->target = s;
	return cr;
}

static void
cairo_destroy(cairo_t *cr)
{
	g_cairo_destroy_count++;
	free(cr);
}

static void cairo_set_source_rgba(cairo_t *cr, double r, double g,
	double b, double a)
{
	(void)cr; (void)r; (void)g; (void)b; (void)a;
}
static void cairo_rectangle(cairo_t *cr, double x, double y,
	double w, double h)
{
	(void)cr; (void)x; (void)y; (void)w; (void)h;
}
static void cairo_fill(cairo_t *cr) { (void)cr; }
static void cairo_stroke(cairo_t *cr) { (void)cr; }
static void cairo_set_line_width(cairo_t *cr, double w)
{
	(void)cr; (void)w;
}
static void cairo_translate(cairo_t *cr, double x, double y)
{
	(void)cr; (void)x; (void)y;
}
static void cairo_scale(cairo_t *cr, double sx, double sy)
{
	(void)cr; (void)sx; (void)sy;
}
static void cairo_set_source_surface(cairo_t *cr, cairo_surface_t *s,
	double x, double y)
{
	(void)cr; (void)s; (void)x; (void)y;
}
static void cairo_paint(cairo_t *cr) { (void)cr; }

/* --- Stub wlr scene functions --- */

static int g_scene_node_set_position_count;
static int g_scene_node_set_enabled_count;
static int g_scene_node_destroy_count;
static int g_scene_buffer_create_count;
static int g_scene_buffer_set_buffer_count;

static void
wlr_scene_node_set_position(struct wlr_scene_node *node, int x, int y)
{
	if (node) {
		node->x = x;
		node->y = y;
	}
	g_scene_node_set_position_count++;
}

static void
wlr_scene_node_set_enabled(struct wlr_scene_node *node, bool enabled)
{
	if (node) {
		node->enabled = enabled ? 1 : 0;
	}
	g_scene_node_set_enabled_count++;
}

static void
wlr_scene_node_destroy(struct wlr_scene_node *node)
{
	(void)node;
	g_scene_node_destroy_count++;
}

static struct wlr_scene_buffer *g_scene_buffers[64];
static int g_scene_buffer_next;

static struct wlr_scene_buffer *
wlr_scene_buffer_create(struct wlr_scene_tree *parent, void *buffer)
{
	(void)parent; (void)buffer;
	g_scene_buffer_create_count++;
	if (g_scene_buffer_next < 64) {
		struct wlr_scene_buffer *buf = calloc(1, sizeof(*buf));
		g_scene_buffers[g_scene_buffer_next++] = buf;
		return buf;
	}
	return NULL;
}

static void
wlr_scene_buffer_set_buffer(struct wlr_scene_buffer *scene_buf,
	struct wlr_buffer *buffer)
{
	(void)scene_buf; (void)buffer;
	g_scene_buffer_set_buffer_count++;
}

static struct wlr_scene_tree *
wlr_scene_tree_create(struct wlr_scene_tree *parent)
{
	(void)parent;
	return calloc(1, sizeof(struct wlr_scene_tree));
}

/* --- Stub sd-bus types and functions --- */

/*
 * We include the real systemd headers for the vtable macros and type
 * definitions. The sd-bus headers only declare function prototypes;
 * we provide stub implementations below that override at link time.
 */
#include <systemd/sd-bus.h>

/* Provide the vtable format reference symbol */
const unsigned sd_bus_object_vtable_format = 0;

/* --- Configurable sd-bus stub globals --- */
static int g_sdbus_read_ret = -1;
static const char *g_sdbus_read_str1 = NULL;
static const char *g_sdbus_read_str2 = NULL;
static const char *g_sdbus_read_str3 = NULL;
static const char *g_sdbus_get_sender_ret = NULL;
static int g_sdbus_is_method_error_ret = 0;
static int g_sdbus_process_ret = 0;
static int g_sdbus_open_user_ret = -1;
static int g_sdbus_emit_signal_count = 0;
static int g_sdbus_call_method_async_count = 0;
static int g_sdbus_reply_method_return_count = 0;
static int g_sdbus_reply_method_errorf_count = 0;

/* sd-bus function stubs (configurable for test purposes) */

int sd_bus_open_user(sd_bus **ret)
{
	(void)ret;
	return g_sdbus_open_user_ret;
}

int sd_bus_add_object_vtable(sd_bus *bus, sd_bus_slot **slot,
	const char *path, const char *interface,
	const sd_bus_vtable *vtable, void *userdata)
{
	(void)bus; (void)slot; (void)path; (void)interface;
	(void)vtable; (void)userdata;
	return 0;
}

int sd_bus_request_name(sd_bus *bus, const char *name, uint64_t flags)
{
	(void)bus; (void)name; (void)flags;
	return 0;
}

int sd_bus_match_signal(sd_bus *bus, sd_bus_slot **slot,
	const char *sender, const char *path,
	const char *interface, const char *member,
	sd_bus_message_handler_t callback, void *userdata)
{
	(void)bus; (void)slot; (void)sender; (void)path;
	(void)interface; (void)member; (void)callback; (void)userdata;
	return 0;
}

int sd_bus_get_fd(sd_bus *bus)
{
	(void)bus;
	return -1;
}

int sd_bus_process(sd_bus *bus, sd_bus_message **ret)
{
	(void)bus; (void)ret;
	return g_sdbus_process_ret;
}

int sd_bus_call_method_async(sd_bus *bus, sd_bus_slot **slot,
	const char *destination, const char *path,
	const char *interface, const char *member,
	sd_bus_message_handler_t callback, void *userdata,
	const char *types, ...)
{
	(void)bus; (void)slot; (void)destination; (void)path;
	(void)interface; (void)member; (void)callback; (void)userdata;
	(void)types;
	g_sdbus_call_method_async_count++;
	return 0;
}

int sd_bus_emit_signal(sd_bus *bus, const char *path,
	const char *interface, const char *member,
	const char *types, ...)
{
	(void)bus; (void)path; (void)interface; (void)member; (void)types;
	g_sdbus_emit_signal_count++;
	return 0;
}

int sd_bus_reply_method_return(sd_bus_message *call, const char *types, ...)
{
	(void)call; (void)types;
	g_sdbus_reply_method_return_count++;
	return 0;
}

int sd_bus_reply_method_errorf(sd_bus_message *call, const char *name,
	const char *format, ...)
{
	(void)call; (void)name; (void)format;
	g_sdbus_reply_method_errorf_count++;
	return 0;
}

int sd_bus_message_read(sd_bus_message *m, const char *types, ...)
{
	(void)m;
	if (g_sdbus_read_ret < 0) return g_sdbus_read_ret;

	va_list ap;
	va_start(ap, types);

	if (strcmp(types, "s") == 0) {
		const char **p = va_arg(ap, const char **);
		*p = g_sdbus_read_str1;
	} else if (strcmp(types, "sss") == 0) {
		const char **p1 = va_arg(ap, const char **);
		const char **p2 = va_arg(ap, const char **);
		const char **p3 = va_arg(ap, const char **);
		*p1 = g_sdbus_read_str1;
		*p2 = g_sdbus_read_str2;
		*p3 = g_sdbus_read_str3;
	} else if (strcmp(types, "v") == 0) {
		const char *vtype = va_arg(ap, const char *);
		if (strcmp(vtype, "s") == 0) {
			const char **p = va_arg(ap, const char **);
			*p = g_sdbus_read_str1;
		}
	}

	va_end(ap);
	return g_sdbus_read_ret;
}

const char *sd_bus_message_get_sender(sd_bus_message *m)
{
	(void)m;
	return g_sdbus_get_sender_ret;
}

int sd_bus_message_is_method_error(sd_bus_message *m, const char *name)
{
	(void)m; (void)name;
	return g_sdbus_is_method_error_ret;
}

int sd_bus_message_open_container(sd_bus_message *m, char type,
	const char *contents)
{
	(void)m; (void)type; (void)contents;
	return 0;
}

int sd_bus_message_close_container(sd_bus_message *m)
{
	(void)m;
	return 0;
}

int sd_bus_message_append(sd_bus_message *m, const char *types, ...)
{
	(void)m; (void)types;
	return 0;
}

sd_bus_slot *sd_bus_slot_unref(sd_bus_slot *slot)
{
	(void)slot;
	return NULL;
}

sd_bus *sd_bus_unref(sd_bus *bus)
{
	(void)bus;
	return NULL;
}

int sd_bus_release_name(sd_bus *bus, const char *name)
{
	(void)bus; (void)name;
	return 0;
}

sd_bus *sd_bus_flush_close_unref(sd_bus *bus)
{
	(void)bus;
	return NULL;
}

/* --- Stub wl_event_loop functions --- */

typedef int (*wl_event_loop_fd_func_t)(int fd, uint32_t mask, void *data);

static struct wl_event_source *
wl_event_loop_add_fd(struct wl_event_loop *loop, int fd,
	uint32_t mask, wl_event_loop_fd_func_t func, void *data)
{
	(void)loop; (void)fd; (void)mask; (void)func; (void)data;
	return NULL;
}

static void
wl_event_source_remove(struct wl_event_source *source)
{
	(void)source;
}

/* --- Stub wm types for server.h and toolbar.h --- */

struct wm_toolbar {
	int dummy;
};

static int g_toolbar_relayout_count;

static void
wm_toolbar_relayout(struct wm_toolbar *toolbar)
{
	(void)toolbar;
	g_toolbar_relayout_count++;
}

struct wm_ipc_server {
	int dummy;
};

struct wm_server {
	struct wl_event_loop *wl_event_loop;
	struct wm_toolbar *toolbar;
	struct wlr_scene_tree *layer_top;
};

/* --- Systray header contents (normally from systray.h) --- */

#define WM_SYSTRAY_ICON_SIZE 18
#define WM_SYSTRAY_ICON_PADDING 2

struct wm_systray_item {
	char *bus_name;
	char *object_path;
	char *id;
	char *icon_name;
	cairo_surface_t *icon_pixmap;
	struct wlr_scene_buffer *icon_buf;
	struct wm_systray *systray;
	struct wl_list link;
};

struct wm_systray {
	struct wm_server *server;
	sd_bus *bus;
	struct wl_event_source *bus_event;
	struct wl_list items;
	int item_count;
	struct wlr_scene_tree *scene_tree;
	sd_bus_slot *watcher_slot;
	sd_bus_slot *name_slot;
	int icon_size;
	int x, y, width, height;
};

/* Forward declarations of public API (normally from systray.h) */
struct wm_systray *wm_systray_create(struct wm_server *server);
void wm_systray_destroy(struct wm_systray *systray);
void wm_systray_layout(struct wm_systray *systray, int x, int y,
	int max_width, int height);
int wm_systray_get_width(struct wm_systray *systray);
bool wm_systray_handle_click(struct wm_systray *systray,
	double local_x, double local_y, uint32_t button);

/* --- Include systray.c source directly --- */
#include "pixel_buffer.c"
#include "../src/systray.c"

/* --- Test helpers --- */

static struct wm_server test_server;
static struct wm_toolbar test_toolbar;
static struct wlr_scene_tree test_layer_top;

static void
reset_globals(void)
{
	g_scene_node_set_position_count = 0;
	g_scene_node_set_enabled_count = 0;
	g_scene_node_destroy_count = 0;
	g_scene_buffer_create_count = 0;
	g_scene_buffer_set_buffer_count = 0;
	g_scene_buffer_next = 0;
	g_toolbar_relayout_count = 0;
	g_cairo_surface_create_count = 0;
	g_cairo_destroy_count = 0;
	g_cairo_surface_destroy_count = 0;
	g_sdbus_read_ret = -1;
	g_sdbus_read_str1 = NULL;
	g_sdbus_read_str2 = NULL;
	g_sdbus_read_str3 = NULL;
	g_sdbus_get_sender_ret = NULL;
	g_sdbus_is_method_error_ret = 0;
	g_sdbus_process_ret = 0;
	g_sdbus_open_user_ret = -1;
	g_sdbus_emit_signal_count = 0;
	g_sdbus_call_method_async_count = 0;
	g_sdbus_reply_method_return_count = 0;
	g_sdbus_reply_method_errorf_count = 0;
}

/*
 * Create a minimal systray struct for testing without D-Bus.
 * This bypasses wm_systray_create() which requires a real D-Bus connection.
 */
static struct wm_systray *
make_test_systray(void)
{
	struct wm_systray *systray = calloc(1, sizeof(*systray));
	assert(systray);

	memset(&test_server, 0, sizeof(test_server));
	test_server.toolbar = &test_toolbar;
	test_server.layer_top = &test_layer_top;

	systray->server = &test_server;
	systray->icon_size = WM_SYSTRAY_ICON_SIZE;
	wl_list_init(&systray->items);
	systray->scene_tree = calloc(1, sizeof(struct wlr_scene_tree));

	return systray;
}

/*
 * Add a test item to the systray with a fake icon_buf.
 * The icon_buf is needed for layout and click hit testing.
 */
static struct wm_systray_item *
add_test_item(struct wm_systray *systray, const char *bus_name, bool with_icon)
{
	struct wm_systray_item *item = calloc(1, sizeof(*item));
	assert(item);

	item->systray = systray;
	item->bus_name = strdup(bus_name);
	item->object_path = strdup("/StatusNotifierItem");
	item->id = NULL;
	item->icon_name = NULL;
	item->icon_pixmap = NULL;

	if (with_icon) {
		item->icon_buf = calloc(1, sizeof(struct wlr_scene_buffer));
	} else {
		item->icon_buf = NULL;
	}

	wl_list_insert(systray->items.prev, &item->link);
	systray->item_count++;

	return item;
}

/*
 * Destroy a test systray and all its items.
 * Cannot use wm_systray_destroy() because it calls sd_bus functions and
 * wlr_scene_node_destroy on scene_tree, which we allocated differently.
 */
static void
destroy_test_systray(struct wm_systray *systray)
{
	if (!systray) return;

	struct wm_systray_item *item, *tmp;
	wl_list_for_each_safe(item, tmp, &systray->items, link) {
		wl_list_remove(&item->link);
		free(item->bus_name);
		free(item->object_path);
		free(item->id);
		free(item->icon_name);
		if (item->icon_pixmap) {
			cairo_surface_destroy(item->icon_pixmap);
		}
		free(item->icon_buf);
		free(item);
	}
	free(systray->scene_tree);
	free(systray);
}

/* ===== Tests ===== */

/*
 * Test 1: wm_systray_get_width with zero items returns 0.
 */
static void
test_get_width_empty(void)
{
	struct wm_systray *systray = make_test_systray();
	reset_globals();

	int width = wm_systray_get_width(systray);
	assert(width == 0);

	/* NULL systray also returns 0 */
	assert(wm_systray_get_width(NULL) == 0);

	destroy_test_systray(systray);
	printf("  PASS: test_get_width_empty\n");
}

/*
 * Test 2: wm_systray_get_width with N items returns correct total.
 * Formula: item_count * (icon_size + padding) + padding
 */
static void
test_get_width_with_items(void)
{
	struct wm_systray *systray = make_test_systray();
	reset_globals();

	int icon_size = WM_SYSTRAY_ICON_SIZE; /* 18 */
	int padding = WM_SYSTRAY_ICON_PADDING; /* 2 */

	/* 1 item: 1 * (18 + 2) + 2 = 22 */
	add_test_item(systray, ":1.100", true);
	int width = wm_systray_get_width(systray);
	assert(width == 1 * (icon_size + padding) + padding);

	/* 2 items: 2 * (18 + 2) + 2 = 42 */
	add_test_item(systray, ":1.101", true);
	width = wm_systray_get_width(systray);
	assert(width == 2 * (icon_size + padding) + padding);

	/* 5 items: 5 * (18 + 2) + 2 = 102 */
	add_test_item(systray, ":1.102", true);
	add_test_item(systray, ":1.103", true);
	add_test_item(systray, ":1.104", true);
	width = wm_systray_get_width(systray);
	assert(width == 5 * (icon_size + padding) + padding);

	destroy_test_systray(systray);
	printf("  PASS: test_get_width_with_items\n");
}

/*
 * Test 3: wm_systray_layout positions icons correctly.
 * Each icon should be at x offset = padding + i*(icon_size+padding),
 * vertically centered in the given height.
 */
static void
test_layout_positions_icons(void)
{
	struct wm_systray *systray = make_test_systray();
	reset_globals();

	int icon_size = WM_SYSTRAY_ICON_SIZE; /* 18 */
	int padding = WM_SYSTRAY_ICON_PADDING; /* 2 */
	int bar_height = 24;

	struct wm_systray_item *item1 = add_test_item(systray, ":1.10", true);
	struct wm_systray_item *item2 = add_test_item(systray, ":1.11", true);
	struct wm_systray_item *item3 = add_test_item(systray, ":1.12", true);

	wm_systray_layout(systray, 100, 50, 200, bar_height);

	/* Check that systray stored position info */
	assert(systray->x == 100);
	assert(systray->y == 50);
	assert(systray->height == bar_height);

	/* Verify icon positions: each icon_buf->node should be positioned.
	 * Items are iterated in list order. wl_list_insert with items.prev
	 * appends to end, so order is: item1, item2, item3.
	 *
	 * item1: offset=2, iy=(24-18)/2=3
	 * item2: offset=2+18+2=22, iy=3
	 * item3: offset=22+18+2=42, iy=3
	 */
	int expected_iy = (bar_height - icon_size) / 2; /* 3 */
	assert(expected_iy == 3);

	assert(item1->icon_buf->node.x == padding);
	assert(item1->icon_buf->node.y == expected_iy);

	assert(item2->icon_buf->node.x == padding + (icon_size + padding));
	assert(item2->icon_buf->node.y == expected_iy);

	assert(item3->icon_buf->node.x == padding + 2 * (icon_size + padding));
	assert(item3->icon_buf->node.y == expected_iy);

	/* Scene tree should be positioned at (100, 50) */
	assert(systray->scene_tree->node.x == 100);
	assert(systray->scene_tree->node.y == 50);

	/* width should be set by layout */
	int expected_width = padding + 3 * (icon_size + padding);
	assert(systray->width == expected_width);

	destroy_test_systray(systray);
	printf("  PASS: test_layout_positions_icons\n");
}

/*
 * Test 4: wm_systray_layout with NULL systray is a no-op.
 */
static void
test_layout_null_safety(void)
{
	reset_globals();

	/* Should not crash */
	wm_systray_layout(NULL, 0, 0, 100, 24);

	assert(g_scene_node_set_position_count == 0);

	printf("  PASS: test_layout_null_safety\n");
}

/*
 * Test 5: wm_systray_layout skips items without icon_buf.
 */
static void
test_layout_skips_no_icon(void)
{
	struct wm_systray *systray = make_test_systray();
	reset_globals();

	int icon_size = WM_SYSTRAY_ICON_SIZE;
	int padding = WM_SYSTRAY_ICON_PADDING;

	/* item1 has icon, item2 does not, item3 has icon */
	struct wm_systray_item *item1 = add_test_item(systray, ":1.20", true);
	add_test_item(systray, ":1.21", false); /* no icon_buf */
	struct wm_systray_item *item3 = add_test_item(systray, ":1.22", true);

	wm_systray_layout(systray, 0, 0, 200, 24);

	/* Only items with icon_buf get positioned.
	 * item1 at offset=2, item3 at offset=22 (skip item2) */
	assert(item1->icon_buf->node.x == padding);
	assert(item3->icon_buf->node.x == padding + (icon_size + padding));

	/* width should only account for the 2 visible icons */
	int expected_width = padding + 2 * (icon_size + padding);
	assert(systray->width == expected_width);

	destroy_test_systray(systray);
	printf("  PASS: test_layout_skips_no_icon\n");
}

/*
 * Test 6: wm_systray_handle_click hits the correct icon.
 */
static void
test_handle_click_hit(void)
{
	struct wm_systray *systray = make_test_systray();
	reset_globals();

	int bar_height = 24;

	add_test_item(systray, ":1.30", true);
	add_test_item(systray, ":1.31", true);
	add_test_item(systray, ":1.32", true);

	/* Layout with height=24 so we know icon positions */
	wm_systray_layout(systray, 0, 0, 200, bar_height);

	/* Click in the middle of the first icon:
	 * icon1 at x=[2..20), y=[3..21)
	 * Click at (10, 10) should hit icon1 */
	bool hit = wm_systray_handle_click(systray, 10.0, 10.0, 0x110);
	assert(hit == true);

	/* Click in the middle of the second icon:
	 * icon2 at x=[22..40), y=[3..21)
	 * Click at (30, 10) should hit icon2 */
	hit = wm_systray_handle_click(systray, 30.0, 10.0, 0x110);
	assert(hit == true);

	/* Click in the third icon:
	 * icon3 at x=[42..60), y=[3..21)
	 * Click at (50, 10) should hit icon3 */
	hit = wm_systray_handle_click(systray, 50.0, 10.0, 0x110);
	assert(hit == true);

	/* Click in padding between icons (x=20, y=10) - should miss */
	hit = wm_systray_handle_click(systray, 20.0, 10.0, 0x110);
	assert(hit == false);

	/* Click above icons (y=1, well above iy=3) - should miss */
	hit = wm_systray_handle_click(systray, 10.0, 1.0, 0x110);
	assert(hit == false);

	/* Click below icons (y=22, below iy+icon_size=21) - should miss */
	hit = wm_systray_handle_click(systray, 10.0, 22.0, 0x110);
	assert(hit == false);

	/* Click way past all icons (x=100) - should miss */
	hit = wm_systray_handle_click(systray, 100.0, 10.0, 0x110);
	assert(hit == false);

	/* Negative coordinates - should miss */
	hit = wm_systray_handle_click(systray, -5.0, 10.0, 0x110);
	assert(hit == false);

	destroy_test_systray(systray);
	printf("  PASS: test_handle_click_hit\n");
}

/*
 * Test 7: wm_systray_handle_click with empty systray and NULL.
 */
static void
test_handle_click_empty(void)
{
	struct wm_systray *systray = make_test_systray();
	reset_globals();

	/* Empty systray - should return false */
	bool hit = wm_systray_handle_click(systray, 5.0, 5.0, 0x110);
	assert(hit == false);

	/* NULL systray - should return false */
	hit = wm_systray_handle_click(NULL, 5.0, 5.0, 0x110);
	assert(hit == false);

	destroy_test_systray(systray);
	printf("  PASS: test_handle_click_empty\n");
}

/*
 * Test 8: wm_systray_handle_click with different button codes
 * (BTN_LEFT=0x110, BTN_RIGHT=0x111, BTN_MIDDLE=0x112).
 */
static void
test_handle_click_buttons(void)
{
	struct wm_systray *systray = make_test_systray();
	reset_globals();

	add_test_item(systray, ":1.40", true);
	wm_systray_layout(systray, 0, 0, 200, 24);

	/* All standard button codes should return true on a hit */
	bool hit;

	hit = wm_systray_handle_click(systray, 10.0, 10.0, 0x110); /* LEFT */
	assert(hit == true);

	hit = wm_systray_handle_click(systray, 10.0, 10.0, 0x111); /* RIGHT */
	assert(hit == true);

	hit = wm_systray_handle_click(systray, 10.0, 10.0, 0x112); /* MIDDLE */
	assert(hit == true);

	/* An unknown button code should still return true (icon was hit)
	 * because the hit test succeeds even if no method is dispatched */
	hit = wm_systray_handle_click(systray, 10.0, 10.0, 0x999);
	assert(hit == true);

	destroy_test_systray(systray);
	printf("  PASS: test_handle_click_buttons\n");
}

/*
 * Test 9: find_item_by_bus_name locates existing items and returns
 * NULL for non-existent ones.
 */
static void
test_find_item_by_bus_name(void)
{
	struct wm_systray *systray = make_test_systray();
	reset_globals();

	struct wm_systray_item *a = add_test_item(systray, ":1.50", true);
	struct wm_systray_item *b = add_test_item(systray, ":1.51", true);
	struct wm_systray_item *c = add_test_item(systray, "org.app.Tray", true);

	/* Find each by name */
	assert(find_item_by_bus_name(systray, ":1.50") == a);
	assert(find_item_by_bus_name(systray, ":1.51") == b);
	assert(find_item_by_bus_name(systray, "org.app.Tray") == c);

	/* Non-existent name returns NULL */
	assert(find_item_by_bus_name(systray, ":1.99") == NULL);
	assert(find_item_by_bus_name(systray, "") == NULL);

	destroy_test_systray(systray);
	printf("  PASS: test_find_item_by_bus_name\n");
}

/*
 * Test 10: systray_item_destroy properly removes item and decrements count.
 */
static void
test_item_destroy(void)
{
	struct wm_systray *systray = make_test_systray();
	reset_globals();

	add_test_item(systray, ":1.60", true);
	struct wm_systray_item *item2 = add_test_item(systray, ":1.61", true);
	add_test_item(systray, ":1.62", true);
	assert(systray->item_count == 3);

	/* Destroy the middle item */
	systray_item_destroy(item2);

	assert(systray->item_count == 2);

	/* item2's bus_name should no longer be findable */
	assert(find_item_by_bus_name(systray, ":1.61") == NULL);

	/* The other two should still be there */
	assert(find_item_by_bus_name(systray, ":1.60") != NULL);
	assert(find_item_by_bus_name(systray, ":1.62") != NULL);

	/* systray_item_destroy(NULL) should be safe */
	systray_item_destroy(NULL);

	destroy_test_systray(systray);
	printf("  PASS: test_item_destroy\n");
}

/*
 * Test 11: wm_systray_layout with small height clamps iy to 0.
 */
static void
test_layout_small_height(void)
{
	struct wm_systray *systray = make_test_systray();
	reset_globals();

	int icon_size = WM_SYSTRAY_ICON_SIZE; /* 18 */

	struct wm_systray_item *item = add_test_item(systray, ":1.70", true);

	/* Height smaller than icon size: iy = (10-18)/2 = -4, clamped to 0 */
	wm_systray_layout(systray, 0, 0, 200, 10);

	assert(item->icon_buf->node.y == 0);

	/* Height equal to icon size: iy = (18-18)/2 = 0 */
	wm_systray_layout(systray, 0, 0, 200, icon_size);
	assert(item->icon_buf->node.y == 0);

	destroy_test_systray(systray);
	printf("  PASS: test_layout_small_height\n");
}

/*
 * Test 12: wm_systray_handle_click correctly handles items without icon_buf
 * (they should be skipped, not crash).
 */
static void
test_handle_click_skips_no_icon(void)
{
	struct wm_systray *systray = make_test_systray();
	reset_globals();

	/* item1 has no icon, item2 has icon */
	add_test_item(systray, ":1.80", false);
	add_test_item(systray, ":1.81", true);

	wm_systray_layout(systray, 0, 0, 200, 24);

	/* The item without icon_buf is skipped in layout, so the one with
	 * icon_buf starts at offset=2. Click at (10, 10) should hit it. */
	bool hit = wm_systray_handle_click(systray, 10.0, 10.0, 0x110);
	assert(hit == true);

	/* Click at a position that would correspond to where the first icon
	 * would be if it weren't skipped - the iconless item doesn't consume
	 * any space, so the same position still hits item2. */

	destroy_test_systray(systray);
	printf("  PASS: test_handle_click_skips_no_icon\n");
}

/*
 * Test 13: create_placeholder_icon returns a non-NULL surface with correct
 * dimensions.
 */
static void
test_create_placeholder_icon(void)
{
	reset_globals();

	int size = 24;
	cairo_surface_t *surface = create_placeholder_icon(size);
	assert(surface != NULL);
	assert(cairo_image_surface_get_width(surface) == size);
	assert(cairo_image_surface_get_height(surface) == size);
	assert(surface->status == CAIRO_STATUS_SUCCESS);

	/* Verify a cairo context was created and destroyed (draw ops) */
	assert(g_cairo_surface_create_count == 1);
	assert(g_cairo_destroy_count == 1);

	cairo_surface_destroy(surface);

	/* Also test with the default systray icon size */
	reset_globals();
	surface = create_placeholder_icon(WM_SYSTRAY_ICON_SIZE);
	assert(surface != NULL);
	assert(cairo_image_surface_get_width(surface) == WM_SYSTRAY_ICON_SIZE);
	assert(cairo_image_surface_get_height(surface) == WM_SYSTRAY_ICON_SIZE);

	cairo_surface_destroy(surface);
	printf("  PASS: test_create_placeholder_icon\n");
}

/*
 * Test 14: scale_icon with NULL input returns NULL.
 */
static void
test_scale_icon_null_input(void)
{
	reset_globals();

	cairo_surface_t *result = scale_icon(NULL, 24);
	assert(result == NULL);

	printf("  PASS: test_scale_icon_null_input\n");
}

/*
 * Test 15: scale_icon when source is already the target size returns
 * the same surface (no scaling needed).
 */
static void
test_scale_icon_same_size(void)
{
	reset_globals();

	int size = 18;
	cairo_surface_t *src = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, size, size);
	assert(src != NULL);

	cairo_surface_t *result = scale_icon(src, size);
	/* When src_w == target_size && src_h == target_size, returns src */
	assert(result == src);

	cairo_surface_destroy(result);
	printf("  PASS: test_scale_icon_same_size\n");
}

/*
 * Test 16: scale_icon scales a larger surface down to the target size.
 * The output surface should have target_size dimensions.
 */
static void
test_scale_icon_needs_scaling(void)
{
	reset_globals();

	/* Source is 48x48, target is 18 */
	cairo_surface_t *src = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, 48, 48);
	assert(src != NULL);

	cairo_surface_t *result = scale_icon(src, 18);
	assert(result != NULL);
	/* Result should not be the same pointer as src (new surface) */
	assert(result != src);
	assert(cairo_image_surface_get_width(result) == 18);
	assert(cairo_image_surface_get_height(result) == 18);

	/* src was destroyed by scale_icon (line 246), so only destroy result */
	cairo_surface_destroy(result);

	/* Also test scaling up: 8x8 to 24 */
	reset_globals();
	src = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 8, 8);
	result = scale_icon(src, 24);
	assert(result != NULL);
	assert(cairo_image_surface_get_width(result) == 24);
	assert(cairo_image_surface_get_height(result) == 24);

	cairo_surface_destroy(result);
	printf("  PASS: test_scale_icon_needs_scaling\n");
}

/*
 * Test 17: scale_icon with zero-dimension source returns NULL.
 */
static void
test_scale_icon_zero_dimensions(void)
{
	reset_globals();

	/* Create a surface with zero dimensions by manipulating directly */
	cairo_surface_t *src = calloc(1, sizeof(*src));
	src->width = 0;
	src->height = 0;
	src->stride = 0;
	src->data = NULL;
	src->refcount = 1;
	src->status = CAIRO_STATUS_SUCCESS;

	cairo_surface_t *result = scale_icon(src, 18);
	assert(result == NULL);

	/* src was not destroyed by scale_icon in this path, clean up */
	free(src);

	/* Also test with negative-like zero: width=0, height=10 */
	src = calloc(1, sizeof(*src));
	src->width = 0;
	src->height = 10;
	src->stride = 0;
	src->data = NULL;
	src->refcount = 1;
	src->status = CAIRO_STATUS_SUCCESS;

	result = scale_icon(src, 18);
	assert(result == NULL);
	free(src);

	printf("  PASS: test_scale_icon_zero_dimensions\n");
}

/*
 * Test 18: systray_item_update_scene takes the placeholder path when
 * no icon_name and no pixmap data.
 */
static void
test_item_update_scene_placeholder(void)
{
	struct wm_systray *systray = make_test_systray();
	reset_globals();

	struct wm_systray_item *item = add_test_item(systray, ":1.90", false);
	/* Ensure no icon_name and no icon_pixmap */
	assert(item->icon_name == NULL);
	assert(item->icon_pixmap == NULL);
	assert(item->icon_buf == NULL);

	systray_item_update_scene(item);

	/* Should have created a placeholder icon, scaled it, converted to
	 * buffer, and created a scene buffer. */
	assert(g_scene_buffer_create_count == 1);
	assert(g_scene_buffer_set_buffer_count == 1);

	/* The item should now have an icon_buf */
	assert(item->icon_buf != NULL);

	destroy_test_systray(systray);
	printf("  PASS: test_item_update_scene_placeholder\n");
}

/*
 * Test 19: load_icon_by_name with NULL icon name returns NULL.
 */
static void
test_load_icon_null_name(void)
{
	reset_globals();

	cairo_surface_t *result = load_icon_by_name(NULL, 18);
	assert(result == NULL);

	printf("  PASS: test_load_icon_null_name\n");
}

/*
 * Test 20: load_icon_by_name with empty string returns NULL.
 */
static void
test_load_icon_empty_name(void)
{
	reset_globals();

	cairo_surface_t *result = load_icon_by_name("", 18);
	assert(result == NULL);

	printf("  PASS: test_load_icon_empty_name\n");
}

/*
 * Test 21: wlr_buffer_from_cairo with a valid surface.
 */
static void
test_buffer_from_cairo_valid(void)
{
	reset_globals();

	cairo_surface_t *surface = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, 18, 18);
	assert(surface != NULL);

	struct wlr_buffer *buf = wlr_buffer_from_cairo(surface);
	assert(buf != NULL);
	assert(buf->width == 18);
	assert(buf->height == 18);

	/* Surface should have been destroyed by wlr_buffer_from_cairo */
	wlr_buffer_drop(buf);
	/* Free the pixel_buffer (it allocated data + struct) */
	struct wm_pixel_buffer *pb = wl_container_of(buf, pb, base);
	free(pb->data);
	free(pb);

	printf("  PASS: test_buffer_from_cairo_valid\n");
}

/*
 * Test 22: wlr_buffer_from_cairo with NULL surface.
 */
static void
test_buffer_from_cairo_null(void)
{
	reset_globals();

	struct wlr_buffer *buf = wlr_buffer_from_cairo(NULL);
	assert(buf == NULL);

	printf("  PASS: test_buffer_from_cairo_null\n");
}

/*
 * Test 23: wlr_buffer_from_cairo with zero-dimension surface.
 */
static void
test_buffer_from_cairo_zero(void)
{
	reset_globals();

	/* Create a surface with zero dimensions */
	cairo_surface_t *surface = calloc(1, sizeof(*surface));
	surface->width = 0;
	surface->height = 0;
	surface->stride = 0;
	surface->data = NULL;
	surface->refcount = 1;
	surface->status = CAIRO_STATUS_SUCCESS;

	struct wlr_buffer *buf = wlr_buffer_from_cairo(surface);
	/* Should return NULL because width/height/stride are 0 */
	assert(buf == NULL);
	/* Surface was destroyed inside wlr_buffer_from_cairo */

	printf("  PASS: test_buffer_from_cairo_zero\n");
}

/*
 * Test 24: pixel_buffer_begin_data_ptr_access rejects write access.
 */
static void
test_pixel_buffer_write_reject(void)
{
	reset_globals();

	/* Create a pixel buffer via wlr_buffer_from_cairo */
	cairo_surface_t *surface = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, 4, 4);
	struct wlr_buffer *buf = wlr_buffer_from_cairo(surface);
	assert(buf != NULL);

	/* Try write access - should fail */
	void *data;
	uint32_t format;
	size_t stride;
	bool ok = pixel_buffer_begin_data_ptr_access(buf,
		WLR_BUFFER_DATA_PTR_ACCESS_WRITE, &data, &format, &stride);
	assert(ok == false);

	/* Try read access - should succeed */
	ok = pixel_buffer_begin_data_ptr_access(buf, 0, &data, &format, &stride);
	assert(ok == true);
	assert(data != NULL);
	assert(format == DRM_FORMAT_ARGB8888);
	assert(stride > 0);

	pixel_buffer_end_data_ptr_access(buf);

	struct wm_pixel_buffer *pb = wl_container_of(buf, pb, base);
	free(pb->data);
	free(pb);

	printf("  PASS: test_pixel_buffer_write_reject\n");
}

/*
 * Test 25: pixel_buffer_destroy frees data.
 */
static void
test_pixel_buffer_destroy(void)
{
	reset_globals();

	cairo_surface_t *surface = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, 4, 4);
	struct wlr_buffer *buf = wlr_buffer_from_cairo(surface);
	assert(buf != NULL);

	/* Calling destroy should free the buffer's data and struct */
	pixel_buffer_destroy(buf);
	/* If we get here without crash, it worked */

	printf("  PASS: test_pixel_buffer_destroy\n");
}

/*
 * Test 26: bus_dispatch calls sd_bus_process.
 */
static void
test_bus_dispatch(void)
{
	reset_globals();

	struct wm_systray *systray = make_test_systray();

	/* sd_bus_process returns 0 (no more to process) */
	g_sdbus_process_ret = 0;
	int ret = bus_dispatch(3, 0, systray);
	assert(ret == 0);

	/* sd_bus_process returns error */
	g_sdbus_process_ret = -1;
	ret = bus_dispatch(3, 0, systray);
	assert(ret == 0);

	destroy_test_systray(systray);
	printf("  PASS: test_bus_dispatch\n");
}

/*
 * Test 27: wm_systray_destroy cleans up items properly.
 * We test the real destroy function by giving it a systray with
 * manually allocated items that match what destroy expects.
 */
static void
test_systray_destroy_with_items(void)
{
	reset_globals();

	struct wm_systray *systray = make_test_systray();
	/* Add items with icon_buf that systray_item_destroy will call
	 * wlr_scene_node_destroy on */
	struct wm_systray_item *item1 = add_test_item(systray, ":1.a1", false);
	(void)item1;
	struct wm_systray_item *item2 = add_test_item(systray, ":1.a2", false);
	(void)item2;

	assert(systray->item_count == 2);

	/* Call wm_systray_destroy - it will call systray_item_destroy for each,
	 * then clean up bus/slot/scene_tree.
	 * Our bus/slots are NULL so sd_bus functions will handle NULL gracefully. */
	wm_systray_destroy(systray);

	/* If we got here without crash, the destroy worked */
	assert(g_scene_node_destroy_count >= 1);

	printf("  PASS: test_systray_destroy_with_items\n");
}

/*
 * Test 28: wm_systray_destroy with NULL is safe.
 */
static void
test_systray_destroy_null(void)
{
	reset_globals();
	wm_systray_destroy(NULL);
	/* Should not crash */
	printf("  PASS: test_systray_destroy_null\n");
}

/*
 * Test 29: handle_get_id with successful read sets item->id.
 */
static void
test_handle_get_id_success(void)
{
	reset_globals();
	struct wm_systray *systray = make_test_systray();
	struct wm_systray_item *item = add_test_item(systray, ":1.b0", false);
	assert(item->id == NULL);

	/* Configure sd_bus_message_read to succeed and return "myapp" */
	g_sdbus_read_ret = 0;
	g_sdbus_read_str1 = "myapp";
	g_sdbus_is_method_error_ret = 0;

	int ret = handle_get_id(NULL, item, NULL);
	assert(ret == 0);
	assert(item->id != NULL);
	assert(strcmp(item->id, "myapp") == 0);

	destroy_test_systray(systray);
	printf("  PASS: test_handle_get_id_success\n");
}

/*
 * Test 30: handle_get_id with method error returns early.
 */
static void
test_handle_get_id_error(void)
{
	reset_globals();
	struct wm_systray *systray = make_test_systray();
	struct wm_systray_item *item = add_test_item(systray, ":1.b1", false);

	g_sdbus_is_method_error_ret = 1;

	int ret = handle_get_id(NULL, item, NULL);
	assert(ret == 0);
	assert(item->id == NULL); /* not set */

	destroy_test_systray(systray);
	printf("  PASS: test_handle_get_id_error\n");
}

/*
 * Test 31: handle_get_icon_name with successful read sets icon_name
 * and triggers update_scene + layout + toolbar relayout.
 */
static void
test_handle_get_icon_name_success(void)
{
	reset_globals();
	struct wm_systray *systray = make_test_systray();
	struct wm_systray_item *item = add_test_item(systray, ":1.c0", false);
	assert(item->icon_name == NULL);

	g_sdbus_read_ret = 0;
	g_sdbus_read_str1 = "network-wireless";
	g_sdbus_is_method_error_ret = 0;

	int ret = handle_get_icon_name(NULL, item, NULL);
	assert(ret == 0);
	assert(item->icon_name != NULL);
	assert(strcmp(item->icon_name, "network-wireless") == 0);

	/* update_scene should have been called (creates scene buffer) */
	assert(g_scene_buffer_create_count >= 1);
	/* toolbar relayout should have been called */
	assert(g_toolbar_relayout_count == 1);

	destroy_test_systray(systray);
	printf("  PASS: test_handle_get_icon_name_success\n");
}

/*
 * Test 32: handle_get_icon_name with method error still triggers update_scene.
 */
static void
test_handle_get_icon_name_error(void)
{
	reset_globals();
	struct wm_systray *systray = make_test_systray();
	struct wm_systray_item *item = add_test_item(systray, ":1.c1", false);

	g_sdbus_is_method_error_ret = 1;

	int ret = handle_get_icon_name(NULL, item, NULL);
	assert(ret == 0);
	assert(item->icon_name == NULL);

	/* update_scene and layout should still be called on error path */
	assert(g_scene_buffer_create_count >= 1);

	destroy_test_systray(systray);
	printf("  PASS: test_handle_get_icon_name_error\n");
}

/*
 * Test 33: method_register_item with a bus name string.
 */
static void
test_register_item_bus_name(void)
{
	reset_globals();
	struct wm_systray *systray = make_test_systray();

	g_sdbus_read_ret = 0;
	g_sdbus_read_str1 = ":1.42";

	int ret = method_register_item(NULL, systray, NULL);
	assert(ret == 0);
	assert(systray->item_count == 1);
	assert(g_sdbus_emit_signal_count == 1);
	assert(g_sdbus_reply_method_return_count == 1);

	/* Verify the item was created with correct bus_name */
	struct wm_systray_item *item = find_item_by_bus_name(systray, ":1.42");
	assert(item != NULL);
	assert(strcmp(item->object_path, "/StatusNotifierItem") == 0);

	/* query_item_properties should have been called (2 async calls) */
	assert(g_sdbus_call_method_async_count == 2);

	destroy_test_systray(systray);
	printf("  PASS: test_register_item_bus_name\n");
}

/*
 * Test 34: method_register_item with an object path (uses sender).
 */
static void
test_register_item_object_path(void)
{
	reset_globals();
	struct wm_systray *systray = make_test_systray();

	g_sdbus_read_ret = 0;
	g_sdbus_read_str1 = "/org/custom/StatusNotifierItem";
	g_sdbus_get_sender_ret = ":1.99";

	int ret = method_register_item(NULL, systray, NULL);
	assert(ret == 0);
	assert(systray->item_count == 1);

	struct wm_systray_item *item = find_item_by_bus_name(systray, ":1.99");
	assert(item != NULL);
	assert(strcmp(item->object_path, "/org/custom/StatusNotifierItem") == 0);

	destroy_test_systray(systray);
	printf("  PASS: test_register_item_object_path\n");
}

/*
 * Test 35: method_register_item with object path but NULL sender fails.
 */
static void
test_register_item_null_sender(void)
{
	reset_globals();
	struct wm_systray *systray = make_test_systray();

	g_sdbus_read_ret = 0;
	g_sdbus_read_str1 = "/StatusNotifierItem";
	g_sdbus_get_sender_ret = NULL;

	int ret = method_register_item(NULL, systray, NULL);
	/* Should return an error reply */
	assert(systray->item_count == 0);
	assert(g_sdbus_reply_method_errorf_count == 1);
	(void)ret;

	destroy_test_systray(systray);
	printf("  PASS: test_register_item_null_sender\n");
}

/*
 * Test 36: method_register_item with duplicate bus name.
 */
static void
test_register_item_duplicate(void)
{
	reset_globals();
	struct wm_systray *systray = make_test_systray();

	/* Register first item */
	add_test_item(systray, ":1.50", true);

	/* Try to register same bus name */
	g_sdbus_read_ret = 0;
	g_sdbus_read_str1 = ":1.50";

	int ret = method_register_item(NULL, systray, NULL);
	assert(ret == 0);
	/* Should not add a duplicate */
	assert(systray->item_count == 1);
	assert(g_sdbus_reply_method_return_count == 1);
	/* No signal emitted for duplicate */
	assert(g_sdbus_emit_signal_count == 0);

	destroy_test_systray(systray);
	printf("  PASS: test_register_item_duplicate\n");
}

/*
 * Test 37: method_register_item with sd_bus_message_read failure.
 */
static void
test_register_item_read_fail(void)
{
	reset_globals();
	struct wm_systray *systray = make_test_systray();

	g_sdbus_read_ret = -1;

	int ret = method_register_item(NULL, systray, NULL);
	assert(ret == -1);
	assert(systray->item_count == 0);

	destroy_test_systray(systray);
	printf("  PASS: test_register_item_read_fail\n");
}

/*
 * Test 38: method_register_host just acknowledges.
 */
static void
test_register_host(void)
{
	reset_globals();

	int ret = method_register_host(NULL, NULL, NULL);
	assert(ret == 0);
	assert(g_sdbus_reply_method_return_count == 1);

	printf("  PASS: test_register_host\n");
}

/*
 * Test 39: property_get_host_registered returns true.
 */
static void
test_property_host_registered(void)
{
	reset_globals();

	int ret = property_get_host_registered(NULL, NULL, NULL, NULL,
		NULL, NULL, NULL);
	assert(ret == 0);

	printf("  PASS: test_property_host_registered\n");
}

/*
 * Test 40: property_get_protocol_version returns 0.
 */
static void
test_property_protocol_version(void)
{
	reset_globals();

	int ret = property_get_protocol_version(NULL, NULL, NULL, NULL,
		NULL, NULL, NULL);
	assert(ret == 0);

	printf("  PASS: test_property_protocol_version\n");
}

/*
 * Test 41: handle_name_owner_changed with matching item removes it.
 */
static void
test_name_owner_changed_match(void)
{
	reset_globals();
	struct wm_systray *systray = make_test_systray();
	add_test_item(systray, ":1.d0", true);
	add_test_item(systray, ":1.d1", true);
	assert(systray->item_count == 2);

	/* Simulate name ":1.d0" vanishing (old_owner non-empty, new_owner empty) */
	g_sdbus_read_ret = 0;
	g_sdbus_read_str1 = ":1.d0";  /* name */
	g_sdbus_read_str2 = ":1.d0";  /* old_owner */
	g_sdbus_read_str3 = "";       /* new_owner (empty = vanished) */

	int ret = handle_name_owner_changed(NULL, systray, NULL);
	assert(ret == 0);
	assert(systray->item_count == 1);
	assert(find_item_by_bus_name(systray, ":1.d0") == NULL);
	assert(find_item_by_bus_name(systray, ":1.d1") != NULL);
	/* Unregistered signal should be emitted */
	assert(g_sdbus_emit_signal_count == 1);
	/* Toolbar relayout should be called */
	assert(g_toolbar_relayout_count == 1);

	destroy_test_systray(systray);
	printf("  PASS: test_name_owner_changed_match\n");
}

/*
 * Test 42: handle_name_owner_changed with non-matching name is no-op.
 */
static void
test_name_owner_changed_no_match(void)
{
	reset_globals();
	struct wm_systray *systray = make_test_systray();
	add_test_item(systray, ":1.e0", true);

	g_sdbus_read_ret = 0;
	g_sdbus_read_str1 = ":1.unknown";
	g_sdbus_read_str2 = ":1.unknown";
	g_sdbus_read_str3 = "";

	int ret = handle_name_owner_changed(NULL, systray, NULL);
	assert(ret == 0);
	assert(systray->item_count == 1);

	destroy_test_systray(systray);
	printf("  PASS: test_name_owner_changed_no_match\n");
}

/*
 * Test 43: handle_name_owner_changed with non-empty new_owner (name transfer)
 * is ignored.
 */
static void
test_name_owner_changed_transfer(void)
{
	reset_globals();
	struct wm_systray *systray = make_test_systray();
	add_test_item(systray, ":1.f0", true);

	g_sdbus_read_ret = 0;
	g_sdbus_read_str1 = ":1.f0";
	g_sdbus_read_str2 = ":1.f0";
	g_sdbus_read_str3 = ":1.f1"; /* new owner non-empty = transfer */

	int ret = handle_name_owner_changed(NULL, systray, NULL);
	assert(ret == 0);
	/* Item should NOT be removed */
	assert(systray->item_count == 1);
	assert(find_item_by_bus_name(systray, ":1.f0") != NULL);

	destroy_test_systray(systray);
	printf("  PASS: test_name_owner_changed_transfer\n");
}

/*
 * Test 44: handle_name_owner_changed with read failure is no-op.
 */
static void
test_name_owner_changed_read_fail(void)
{
	reset_globals();
	struct wm_systray *systray = make_test_systray();
	add_test_item(systray, ":1.g0", true);

	g_sdbus_read_ret = -1;

	int ret = handle_name_owner_changed(NULL, systray, NULL);
	assert(ret == 0);
	assert(systray->item_count == 1);

	destroy_test_systray(systray);
	printf("  PASS: test_name_owner_changed_read_fail\n");
}

/*
 * Test 45: systray_item_update_scene with icon_pixmap set uses the pixmap.
 */
static void
test_item_update_scene_with_pixmap(void)
{
	reset_globals();
	struct wm_systray *systray = make_test_systray();
	struct wm_systray_item *item = add_test_item(systray, ":1.h0", false);

	/* Give it a pixmap (same size as icon_size) */
	item->icon_pixmap = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, WM_SYSTRAY_ICON_SIZE, WM_SYSTRAY_ICON_SIZE);
	assert(item->icon_pixmap != NULL);

	systray_item_update_scene(item);

	/* Should have created a scene buffer using the pixmap path */
	assert(g_scene_buffer_create_count == 1);
	assert(g_scene_buffer_set_buffer_count == 1);
	assert(item->icon_buf != NULL);

	destroy_test_systray(systray);
	printf("  PASS: test_item_update_scene_with_pixmap\n");
}

/*
 * Test 46: systray_item_update_scene with existing icon_buf reuses it.
 */
static void
test_item_update_scene_existing_buf(void)
{
	reset_globals();
	struct wm_systray *systray = make_test_systray();
	struct wm_systray_item *item = add_test_item(systray, ":1.i0", true);
	/* item already has icon_buf from add_test_item */
	assert(item->icon_buf != NULL);

	systray_item_update_scene(item);

	/* Should NOT create a new scene buffer (reuses existing) */
	assert(g_scene_buffer_create_count == 0);
	/* But should set the buffer on the existing one */
	assert(g_scene_buffer_set_buffer_count == 1);

	destroy_test_systray(systray);
	printf("  PASS: test_item_update_scene_existing_buf\n");
}

/*
 * Test 47: query_item_properties makes two async calls.
 */
static void
test_query_item_properties(void)
{
	reset_globals();
	struct wm_systray *systray = make_test_systray();
	struct wm_systray_item *item = add_test_item(systray, ":1.j0", false);

	query_item_properties(systray, item);

	/* Should have made 2 async method calls (Id + IconName) */
	assert(g_sdbus_call_method_async_count == 2);

	destroy_test_systray(systray);
	printf("  PASS: test_query_item_properties\n");
}

/*
 * Test 48: file_exists returns false for non-existent path.
 */
static void
test_file_exists_nonexistent(void)
{
	reset_globals();

	bool exists = file_exists("/nonexistent/path/to/nothing");
	assert(exists == false);

	printf("  PASS: test_file_exists_nonexistent\n");
}

/*
 * Test 49: handle_name_owner_changed with empty old_owner is ignored.
 */
static void
test_name_owner_changed_empty_old(void)
{
	reset_globals();
	struct wm_systray *systray = make_test_systray();
	add_test_item(systray, ":1.k0", true);

	g_sdbus_read_ret = 0;
	g_sdbus_read_str1 = ":1.k0";
	g_sdbus_read_str2 = "";       /* old_owner empty = new name appearance */
	g_sdbus_read_str3 = "";

	int ret = handle_name_owner_changed(NULL, systray, NULL);
	assert(ret == 0);
	/* Should not remove any items because old_owner is empty */
	assert(systray->item_count == 1);

	destroy_test_systray(systray);
	printf("  PASS: test_name_owner_changed_empty_old\n");
}

/*
 * Test 50: property_get_registered_items iterates items.
 */
static void
test_property_get_registered_items(void)
{
	reset_globals();
	struct wm_systray *systray = make_test_systray();
	add_test_item(systray, ":1.l0", true);
	add_test_item(systray, ":1.l1", true);

	int ret = property_get_registered_items(NULL, NULL, NULL, NULL,
		NULL, systray, NULL);
	assert(ret == 0);

	destroy_test_systray(systray);
	printf("  PASS: test_property_get_registered_items\n");
}

int
main(void)
{
	printf("test_systray:\n");

	test_get_width_empty();
	test_get_width_with_items();
	test_layout_positions_icons();
	test_layout_null_safety();
	test_layout_skips_no_icon();
	test_handle_click_hit();
	test_handle_click_empty();
	test_handle_click_buttons();
	test_find_item_by_bus_name();
	test_item_destroy();
	test_layout_small_height();
	test_handle_click_skips_no_icon();
	test_create_placeholder_icon();
	test_scale_icon_null_input();
	test_scale_icon_same_size();
	test_scale_icon_needs_scaling();
	test_scale_icon_zero_dimensions();
	test_item_update_scene_placeholder();
	test_load_icon_null_name();
	test_load_icon_empty_name();
	test_buffer_from_cairo_valid();
	test_buffer_from_cairo_null();
	test_buffer_from_cairo_zero();
	test_pixel_buffer_write_reject();
	test_pixel_buffer_destroy();
	test_bus_dispatch();
	test_systray_destroy_with_items();
	test_systray_destroy_null();
	test_handle_get_id_success();
	test_handle_get_id_error();
	test_handle_get_icon_name_success();
	test_handle_get_icon_name_error();
	test_register_item_bus_name();
	test_register_item_object_path();
	test_register_item_null_sender();
	test_register_item_duplicate();
	test_register_item_read_fail();
	test_register_host();
	test_property_host_registered();
	test_property_protocol_version();
	test_name_owner_changed_match();
	test_name_owner_changed_no_match();
	test_name_owner_changed_transfer();
	test_name_owner_changed_read_fail();
	test_item_update_scene_with_pixmap();
	test_item_update_scene_existing_buf();
	test_query_item_properties();
	test_file_exists_nonexistent();
	test_name_owner_changed_empty_old();
	test_property_get_registered_items();

	printf("All systray tests passed.\n");
	return 0;
}
