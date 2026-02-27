/*
 * test_slit.c - Unit tests for slit dockapp container logic
 *
 * Includes slit.c directly with stub implementations to avoid
 * needing wlroots/wayland/XWayland libraries at link time.
 */

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Enable XWayland code paths for slitlist testing */
#ifndef WM_HAS_XWAYLAND
#define WM_HAS_XWAYLAND
#endif

/* --- Block real headers via their include guards --- */
#define WAYLAND_SERVER_CORE_H
#define WAYLAND_SERVER_H
#define WAYLAND_UTIL_H
#define WLR_UTIL_LOG_H
#define WLR_UTIL_BOX_H
#define WLR_TYPES_WLR_SCENE_H
#define WLR_TYPES_WLR_OUTPUT_H
#define WLR_XWAYLAND_XWAYLAND_H
#define WLR_XWAYLAND_SERVER_H
#define WLR_XWAYLAND_SHELL_H
#define WM_SERVER_H
#define WM_STYLE_H
#define WM_CONFIG_H
#define WM_OUTPUT_H

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

/*
 * wl_container_of: we need it to work for wm_slit_client, wm_output, etc.
 * Use the GCC typeof version.
 */
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

struct wl_signal {
	struct wl_list listener_list;
};

struct wl_listener {
	struct wl_list link;
	void (*notify)(struct wl_listener *, void *);
};

static inline void
wl_signal_init(struct wl_signal *signal)
{
	wl_list_init(&signal->listener_list);
}

static inline void
wl_signal_add(struct wl_signal *signal, struct wl_listener *listener)
{
	wl_list_insert(signal->listener_list.prev, &listener->link);
}

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
	void *parent;
};

struct wlr_scene_tree {
	struct wlr_scene_node node;
};

struct wlr_box {
	int x, y, width, height;
};

struct wlr_output {
	int width, height;
};

struct wlr_scene_output {
	int dummy;
};

struct wlr_scene_rect {
	struct wlr_scene_node node;
	float color[4];
	int width, height;
};

struct wlr_scene_buffer {
	struct wlr_scene_node node;
};

struct wlr_surface {
	struct {
		struct wl_signal map;
		struct wl_signal unmap;
	} events;
};

struct wlr_xwayland_surface {
	struct wlr_surface *surface;
	int width, height;
	char *class;
	char *title;
	struct {
		struct wl_signal destroy;
		struct wl_signal request_configure;
	} events;
};

struct wlr_xwayland_surface_configure_event {
	int x, y;
	uint16_t width, height;
};

/* wlr_log no-op */
#define wlr_log(verb, fmt, ...) ((void)0)
#define WLR_ERROR 1
#define WLR_INFO 3
#define WLR_DEBUG 7

/* --- Stub wm types --- */

struct wm_texture {
	int appearance;
	int fill;
	int gradient;
	int bevel;
	bool interlaced;
	uint32_t color;
	uint32_t color_to;
	char *pixmap_path;
};

struct wm_color {
	uint8_t r, g, b, a;
};

static inline struct wm_color wm_argb_to_color(uint32_t argb)
{
	return (struct wm_color){
		.r = (argb >> 16) & 0xFF,
		.g = (argb >> 8) & 0xFF,
		.b = argb & 0xFF,
		.a = (argb >> 24) & 0xFF,
	};
}

struct wm_style {
	struct wm_texture slit_texture;
	struct wm_color slit_border_color;
	int slit_border_width;
};

struct wm_config {
	bool slit_auto_hide;
	int slit_placement;
	int slit_direction;
	int slit_layer;
	int slit_on_head;
	int slit_alpha;
	bool slit_max_over;
	char *slitlist_file;
};

struct wm_workspace {
	struct wl_list link;
	void *server;
	char *name;
	int index;
};

struct wm_output {
	struct wl_list link;
	void *server;
	struct wlr_output *wlr_output;
	struct wlr_scene_output *scene_output;
	struct wlr_box usable_area;
	struct wm_workspace *active_workspace;
};

struct wm_server {
	struct wl_list outputs;
	struct wm_config *config;
	struct wm_style *style;
	struct wlr_scene_tree *layer_top;
	struct wlr_scene_tree *layer_bottom;
	struct wlr_scene_tree *layer_overlay;
	struct wl_event_loop *wl_event_loop;
};

/* --- Stub tracking --- */

static int g_scene_tree_create_count;
static int g_scene_rect_create_count;
static int g_scene_rect_set_color_count;
static int g_scene_rect_set_size_count;
static int g_scene_node_set_position_count;
static int g_scene_node_set_enabled_count;
static int g_scene_node_destroy_count;
static int g_scene_node_reparent_count;
static int g_scene_node_lower_to_bottom_count;
static int g_scene_node_for_each_buffer_count;
static int g_xwayland_configure_count;

/* --- Stub wlroots functions --- */

static struct wlr_scene_tree g_scene_trees[16];
static int g_scene_tree_idx;

static struct wlr_scene_tree *
wlr_scene_tree_create(struct wlr_scene_tree *parent)
{
	(void)parent;
	int idx = g_scene_tree_idx % 16;
	memset(&g_scene_trees[idx], 0, sizeof(g_scene_trees[idx]));
	g_scene_tree_idx++;
	g_scene_tree_create_count++;
	return &g_scene_trees[idx - 1];
}

static struct wlr_scene_rect g_scene_rects[8];
static int g_scene_rect_idx;

static struct wlr_scene_rect *
wlr_scene_rect_create(struct wlr_scene_tree *parent,
	int width, int height, const float color[4])
{
	(void)parent;
	int idx = g_scene_rect_idx % 8;
	memset(&g_scene_rects[idx], 0, sizeof(g_scene_rects[idx]));
	g_scene_rects[idx].width = width;
	g_scene_rects[idx].height = height;
	if (color) {
		memcpy(g_scene_rects[idx].color, color, sizeof(float) * 4);
	}
	g_scene_rect_idx++;
	g_scene_rect_create_count++;
	return &g_scene_rects[idx];
}

static void
wlr_scene_rect_set_color(struct wlr_scene_rect *rect, const float color[4])
{
	if (rect && color) {
		memcpy(rect->color, color, sizeof(float) * 4);
	}
	g_scene_rect_set_color_count++;
}

static void
wlr_scene_rect_set_size(struct wlr_scene_rect *rect, int width, int height)
{
	if (rect) {
		rect->width = width;
		rect->height = height;
	}
	g_scene_rect_set_size_count++;
}

static void
wlr_scene_node_set_position(struct wlr_scene_node *node, int x, int y)
{
	if (node) { node->x = x; node->y = y; }
	g_scene_node_set_position_count++;
}

static void
wlr_scene_node_set_enabled(struct wlr_scene_node *node, bool enabled)
{
	if (node) { node->enabled = enabled ? 1 : 0; }
	g_scene_node_set_enabled_count++;
}

static void
wlr_scene_node_destroy(struct wlr_scene_node *node)
{
	(void)node;
	g_scene_node_destroy_count++;
}

static void
wlr_scene_node_reparent(struct wlr_scene_node *node,
	struct wlr_scene_tree *parent)
{
	if (node) { node->parent = parent; }
	g_scene_node_reparent_count++;
}

static void
wlr_scene_node_lower_to_bottom(struct wlr_scene_node *node)
{
	(void)node;
	g_scene_node_lower_to_bottom_count++;
}

typedef void (*wlr_scene_buffer_iterator_func_t)(
	struct wlr_scene_buffer *buffer, int sx, int sy, void *user_data);

static void
wlr_scene_node_for_each_buffer(struct wlr_scene_node *node,
	wlr_scene_buffer_iterator_func_t iterator, void *user_data)
{
	(void)node; (void)iterator; (void)user_data;
	g_scene_node_for_each_buffer_count++;
}

static void
wlr_scene_buffer_set_opacity(struct wlr_scene_buffer *buffer, float opacity)
{
	(void)buffer; (void)opacity;
}

static struct wlr_scene_tree *
wlr_scene_subsurface_tree_create(struct wlr_scene_tree *parent,
	struct wlr_surface *surface)
{
	(void)parent; (void)surface;
	return wlr_scene_tree_create(parent);
}

static void
wlr_output_effective_resolution(struct wlr_output *output,
	int *width, int *height)
{
	*width = output->width;
	*height = output->height;
}

static struct wl_event_source g_event_source;

static struct wl_event_source *
wl_event_loop_add_timer(struct wl_event_loop *loop,
	int (*cb)(void *), void *data)
{
	(void)loop; (void)cb; (void)data;
	return &g_event_source;
}

static int
wl_event_source_timer_update(struct wl_event_source *src, int ms)
{
	(void)src; (void)ms;
	return 0;
}

static void
wl_event_source_remove(struct wl_event_source *src) { (void)src; }

/* XWayland stubs */
static void
wlr_xwayland_surface_configure(struct wlr_xwayland_surface *surface,
	int x, int y, int w, int h)
{
	(void)surface; (void)x; (void)y; (void)w; (void)h;
	g_xwayland_configure_count++;
}

/* --- Include slit source directly --- */

#include "slit.h"
#include "slit.c"

/* --- Test helpers --- */

static struct wm_server test_server;
static struct wm_style test_style;
static struct wm_config test_config;
static struct wlr_output test_wlr_output;
static struct wm_output test_output;
static struct wlr_scene_tree test_layer_top;
static struct wlr_scene_tree test_layer_bottom;
static struct wlr_scene_tree test_layer_overlay;
static struct wl_event_loop test_event_loop;

static void
reset_globals(void)
{
	g_scene_tree_create_count = 0;
	g_scene_tree_idx = 0;
	g_scene_rect_create_count = 0;
	g_scene_rect_idx = 0;
	g_scene_rect_set_color_count = 0;
	g_scene_rect_set_size_count = 0;
	g_scene_node_set_position_count = 0;
	g_scene_node_set_enabled_count = 0;
	g_scene_node_destroy_count = 0;
	g_scene_node_reparent_count = 0;
	g_scene_node_lower_to_bottom_count = 0;
	g_scene_node_for_each_buffer_count = 0;
	g_xwayland_configure_count = 0;
}

static void
setup_test_server(void)
{
	memset(&test_server, 0, sizeof(test_server));
	memset(&test_style, 0, sizeof(test_style));
	memset(&test_config, 0, sizeof(test_config));
	memset(&test_output, 0, sizeof(test_output));
	memset(&test_wlr_output, 0, sizeof(test_wlr_output));

	test_style.slit_texture.color = 0xFF333333;
	test_style.slit_border_color = (struct wm_color){128, 128, 128, 255};
	test_style.slit_border_width = 1;

	test_config.slit_alpha = 255;
	test_config.slit_placement = WM_SLIT_RIGHT_CENTER;
	test_config.slit_direction = WM_SLIT_VERTICAL;
	test_config.slit_max_over = true; /* don't adjust usable_area */

	test_wlr_output.width = 1920;
	test_wlr_output.height = 1080;
	test_output.wlr_output = &test_wlr_output;
	test_output.usable_area.x = 0;
	test_output.usable_area.y = 0;
	test_output.usable_area.width = 1920;
	test_output.usable_area.height = 1080;

	wl_list_init(&test_server.outputs);
	wl_list_insert(&test_server.outputs, &test_output.link);

	test_server.style = &test_style;
	test_server.config = &test_config;
	test_server.layer_top = &test_layer_top;
	test_server.layer_bottom = &test_layer_bottom;
	test_server.layer_overlay = &test_layer_overlay;
	test_server.wl_event_loop = &test_event_loop;
}

/* ===== Tests ===== */

/* --- slit_compute_position tests --- */

static void
test_slit_position_top_left(void)
{
	struct wm_slit slit;
	memset(&slit, 0, sizeof(slit));
	slit.width = 64;
	slit.height = 200;
	slit.placement = WM_SLIT_TOP_LEFT;

	slit_compute_position(&slit, 1920, 1080);
	assert(slit.x == 0);
	assert(slit.y == 0);
	printf("  PASS: slit_position_top_left\n");
}

static void
test_slit_position_top_center(void)
{
	struct wm_slit slit;
	memset(&slit, 0, sizeof(slit));
	slit.width = 200;
	slit.height = 64;
	slit.placement = WM_SLIT_TOP_CENTER;

	slit_compute_position(&slit, 1920, 1080);
	assert(slit.x == (1920 - 200) / 2);
	assert(slit.y == 0);
	printf("  PASS: slit_position_top_center\n");
}

static void
test_slit_position_top_right(void)
{
	struct wm_slit slit;
	memset(&slit, 0, sizeof(slit));
	slit.width = 64;
	slit.height = 200;
	slit.placement = WM_SLIT_TOP_RIGHT;

	slit_compute_position(&slit, 1920, 1080);
	assert(slit.x == 1920 - 64);
	assert(slit.y == 0);
	printf("  PASS: slit_position_top_right\n");
}

static void
test_slit_position_right_center(void)
{
	struct wm_slit slit;
	memset(&slit, 0, sizeof(slit));
	slit.width = 64;
	slit.height = 200;
	slit.placement = WM_SLIT_RIGHT_CENTER;

	slit_compute_position(&slit, 1920, 1080);
	assert(slit.x == 1920 - 64);
	assert(slit.y == (1080 - 200) / 2);
	printf("  PASS: slit_position_right_center\n");
}

static void
test_slit_position_bottom_center(void)
{
	struct wm_slit slit;
	memset(&slit, 0, sizeof(slit));
	slit.width = 200;
	slit.height = 64;
	slit.placement = WM_SLIT_BOTTOM_CENTER;

	slit_compute_position(&slit, 1920, 1080);
	assert(slit.x == (1920 - 200) / 2);
	assert(slit.y == 1080 - 64);
	printf("  PASS: slit_position_bottom_center\n");
}

static void
test_slit_position_left_center(void)
{
	struct wm_slit slit;
	memset(&slit, 0, sizeof(slit));
	slit.width = 64;
	slit.height = 200;
	slit.placement = WM_SLIT_LEFT_CENTER;

	slit_compute_position(&slit, 1920, 1080);
	assert(slit.x == 0);
	assert(slit.y == (1080 - 200) / 2);
	printf("  PASS: slit_position_left_center\n");
}

static void
test_slit_position_all_placements(void)
{
	/* Verify all 12 placements don't crash and produce valid coords */
	enum wm_slit_placement placements[] = {
		WM_SLIT_TOP_LEFT, WM_SLIT_TOP_CENTER, WM_SLIT_TOP_RIGHT,
		WM_SLIT_RIGHT_TOP, WM_SLIT_RIGHT_CENTER, WM_SLIT_RIGHT_BOTTOM,
		WM_SLIT_BOTTOM_LEFT, WM_SLIT_BOTTOM_CENTER, WM_SLIT_BOTTOM_RIGHT,
		WM_SLIT_LEFT_TOP, WM_SLIT_LEFT_CENTER, WM_SLIT_LEFT_BOTTOM,
	};

	for (int i = 0; i < 12; i++) {
		struct wm_slit slit;
		memset(&slit, 0, sizeof(slit));
		slit.width = 64;
		slit.height = 200;
		slit.placement = placements[i];

		slit_compute_position(&slit, 1920, 1080);

		/* All positions should be within output bounds */
		assert(slit.x >= 0);
		assert(slit.y >= 0);
		assert(slit.x + slit.width <= 1920);
		assert(slit.y + slit.height <= 1080);
	}

	printf("  PASS: slit_position_all_placements\n");
}

/* --- slit_layout_clients tests --- */

static void
test_slit_layout_vertical(void)
{
	struct wm_slit slit;
	memset(&slit, 0, sizeof(slit));
	slit.direction = WM_SLIT_VERTICAL;
	wl_list_init(&slit.clients);

	struct wm_slit_client c1 = {
		.slit = &slit, .mapped = true, .width = 64, .height = 64
	};
	struct wm_slit_client c2 = {
		.slit = &slit, .mapped = true, .width = 48, .height = 48
	};
	wl_list_insert(slit.clients.prev, &c1.link);
	wl_list_insert(slit.clients.prev, &c2.link);

	slit_layout_clients(&slit);

	/* Vertical: width = max(64,48) + 2*padding(2) = 68 */
	assert(slit.width == 64 + 2 * WM_SLIT_PADDING);

	/* Height = padding + 64 + padding + 48 + padding = 2 + 64 + 2 + 48 + 2 = 118 */
	int expected_h = WM_SLIT_PADDING + 64 + WM_SLIT_PADDING + 48 + WM_SLIT_PADDING;
	assert(slit.height == expected_h);

	printf("  PASS: slit_layout_vertical\n");
}

static void
test_slit_layout_horizontal(void)
{
	struct wm_slit slit;
	memset(&slit, 0, sizeof(slit));
	slit.direction = WM_SLIT_HORIZONTAL;
	wl_list_init(&slit.clients);

	struct wm_slit_client c1 = {
		.slit = &slit, .mapped = true, .width = 64, .height = 64
	};
	struct wm_slit_client c2 = {
		.slit = &slit, .mapped = true, .width = 48, .height = 32
	};
	wl_list_insert(slit.clients.prev, &c1.link);
	wl_list_insert(slit.clients.prev, &c2.link);

	slit_layout_clients(&slit);

	/* Horizontal: height = max(64,32) + 2*padding(2) = 68 */
	assert(slit.height == 64 + 2 * WM_SLIT_PADDING);

	/* Width = padding + 64 + padding + 48 + padding */
	int expected_w = WM_SLIT_PADDING + 64 + WM_SLIT_PADDING + 48 + WM_SLIT_PADDING;
	assert(slit.width == expected_w);

	printf("  PASS: slit_layout_horizontal\n");
}

static void
test_slit_layout_unmapped_skipped(void)
{
	struct wm_slit slit;
	memset(&slit, 0, sizeof(slit));
	slit.direction = WM_SLIT_VERTICAL;
	wl_list_init(&slit.clients);

	struct wm_slit_client c1 = {
		.slit = &slit, .mapped = true, .width = 64, .height = 64
	};
	struct wm_slit_client c2 = {
		.slit = &slit, .mapped = false, .width = 100, .height = 100
	};
	wl_list_insert(slit.clients.prev, &c1.link);
	wl_list_insert(slit.clients.prev, &c2.link);

	slit_layout_clients(&slit);

	/* Unmapped client should not affect layout */
	assert(slit.width == 64 + 2 * WM_SLIT_PADDING);
	int expected_h = WM_SLIT_PADDING + 64 + WM_SLIT_PADDING;
	assert(slit.height == expected_h);

	printf("  PASS: slit_layout_unmapped_skipped\n");
}

static void
test_slit_layout_empty(void)
{
	struct wm_slit slit;
	memset(&slit, 0, sizeof(slit));
	slit.direction = WM_SLIT_VERTICAL;
	wl_list_init(&slit.clients);

	slit_layout_clients(&slit);

	/* Empty slit should use minimum size */
	assert(slit.width >= WM_SLIT_MIN_SIZE);
	assert(slit.height >= WM_SLIT_MIN_SIZE);

	printf("  PASS: slit_layout_empty\n");
}

static void
test_slit_layout_min_size(void)
{
	struct wm_slit slit;
	memset(&slit, 0, sizeof(slit));
	slit.direction = WM_SLIT_HORIZONTAL;
	wl_list_init(&slit.clients);

	/* Only unmapped client, so no mapped content */
	struct wm_slit_client c1 = {
		.slit = &slit, .mapped = false, .width = 1, .height = 1
	};
	wl_list_insert(slit.clients.prev, &c1.link);

	slit_layout_clients(&slit);

	assert(slit.width >= WM_SLIT_MIN_SIZE);
	assert(slit.height >= WM_SLIT_MIN_SIZE);

	printf("  PASS: slit_layout_min_size\n");
}

/* --- slit_hide_timer_cb tests --- */

static void
test_slit_hide_timer_cb(void)
{
	reset_globals();

	struct wm_slit slit;
	memset(&slit, 0, sizeof(slit));
	wl_list_init(&slit.clients);

	struct wlr_scene_tree st;
	memset(&st, 0, sizeof(st));
	st.node.enabled = 1;
	slit.scene_tree = &st;

	/* auto_hide = true, hidden = false: should hide */
	slit.auto_hide = true;
	slit.hidden = false;

	int ret = slit_hide_timer_cb(&slit);
	assert(ret == 0);
	assert(slit.hidden == true);
	assert(st.node.enabled == 0);

	/* Already hidden: should be no-op */
	slit.hidden = true;
	st.node.enabled = 0;
	reset_globals();
	ret = slit_hide_timer_cb(&slit);
	assert(ret == 0);
	assert(g_scene_node_set_enabled_count == 0);

	/* auto_hide = false: should be no-op */
	slit.auto_hide = false;
	slit.hidden = false;
	st.node.enabled = 1;
	reset_globals();
	ret = slit_hide_timer_cb(&slit);
	assert(ret == 0);
	assert(slit.hidden == false);

	printf("  PASS: slit_hide_timer_cb\n");
}

/* --- slit_find_insert_position tests --- */

static void
test_slit_find_insert_null_class(void)
{
	struct wm_slit slit;
	memset(&slit, 0, sizeof(slit));
	wl_list_init(&slit.clients);
	slit.slitlist_count = 0;

	/* NULL class name should append */
	struct wl_list *pos = slit_find_insert_position(&slit, NULL);
	assert(pos == slit.clients.prev);

	printf("  PASS: slit_find_insert_null_class\n");
}

static void
test_slit_find_insert_empty_slitlist(void)
{
	struct wm_slit slit;
	memset(&slit, 0, sizeof(slit));
	wl_list_init(&slit.clients);
	slit.slitlist_count = 0;

	struct wl_list *pos = slit_find_insert_position(&slit, "MyApp");
	assert(pos == slit.clients.prev);

	printf("  PASS: slit_find_insert_empty_slitlist\n");
}

static void
test_slit_find_insert_not_in_list(void)
{
	struct wm_slit slit;
	memset(&slit, 0, sizeof(slit));
	wl_list_init(&slit.clients);

	char *sl[] = {"AppA", "AppB", "AppC"};
	slit.slitlist = sl;
	slit.slitlist_count = 3;

	/* "Unknown" not in slitlist -> append */
	struct wl_list *pos = slit_find_insert_position(&slit, "Unknown");
	assert(pos == slit.clients.prev);

	printf("  PASS: slit_find_insert_not_in_list\n");
}

static void
test_slit_find_insert_ordering(void)
{
	struct wm_slit slit;
	memset(&slit, 0, sizeof(slit));
	wl_list_init(&slit.clients);

	char *sl[] = {"AppA", "AppB", "AppC"};
	slit.slitlist = sl;
	slit.slitlist_count = 3;

	/* Set up xwayland surfaces for clients */
	struct wlr_xwayland_surface xs_c = {.class = "AppC"};
	struct wm_slit_client cc = {.slit = &slit, .xsurface = &xs_c};
	wl_list_insert(slit.clients.prev, &cc.link);

	/* AppA (idx 0) should go before AppC (idx 2) */
	struct wl_list *pos = slit_find_insert_position(&slit, "AppA");
	assert(pos == cc.link.prev); /* insert before cc */

	printf("  PASS: slit_find_insert_ordering\n");
}

/* --- Slitlist file I/O tests --- */

static void
test_slit_load_slitlist_roundtrip(void)
{
	/* Create temp file */
	char tmppath[] = "/tmp/fluxland-test-slitlist-XXXXXX";
	int fd = mkstemp(tmppath);
	assert(fd >= 0);

	/* Write test data */
	const char *data = "AppAlpha\nAppBeta\nAppGamma\n";
	write(fd, data, strlen(data));
	close(fd);

	/* Load the slitlist */
	struct wm_slit slit;
	memset(&slit, 0, sizeof(slit));
	wl_list_init(&slit.clients);

	slit_load_slitlist(&slit, tmppath);

	assert(slit.slitlist_count == 3);
	assert(slit.slitlist != NULL);
	assert(strcmp(slit.slitlist[0], "AppAlpha") == 0);
	assert(strcmp(slit.slitlist[1], "AppBeta") == 0);
	assert(strcmp(slit.slitlist[2], "AppGamma") == 0);

	/* Free slitlist */
	slit_free_slitlist(&slit);
	assert(slit.slitlist == NULL);
	assert(slit.slitlist_count == 0);

	unlink(tmppath);
	printf("  PASS: slit_load_slitlist_roundtrip\n");
}

static void
test_slit_load_slitlist_empty_lines(void)
{
	char tmppath[] = "/tmp/fluxland-test-slitlist-XXXXXX";
	int fd = mkstemp(tmppath);
	assert(fd >= 0);

	/* Some empty lines mixed in */
	const char *data = "AppA\n\nAppB\n\n\nAppC\n";
	write(fd, data, strlen(data));
	close(fd);

	struct wm_slit slit;
	memset(&slit, 0, sizeof(slit));
	wl_list_init(&slit.clients);

	slit_load_slitlist(&slit, tmppath);

	/* Empty lines should be skipped */
	assert(slit.slitlist_count == 3);
	assert(strcmp(slit.slitlist[0], "AppA") == 0);
	assert(strcmp(slit.slitlist[1], "AppB") == 0);
	assert(strcmp(slit.slitlist[2], "AppC") == 0);

	slit_free_slitlist(&slit);
	unlink(tmppath);
	printf("  PASS: slit_load_slitlist_empty_lines\n");
}

static void
test_slit_load_slitlist_null_path(void)
{
	struct wm_slit slit;
	memset(&slit, 0, sizeof(slit));
	wl_list_init(&slit.clients);

	/* Should not crash */
	slit_load_slitlist(&slit, NULL);
	assert(slit.slitlist == NULL);
	assert(slit.slitlist_count == 0);

	printf("  PASS: slit_load_slitlist_null_path\n");
}

static void
test_slit_load_slitlist_nonexistent(void)
{
	struct wm_slit slit;
	memset(&slit, 0, sizeof(slit));
	wl_list_init(&slit.clients);

	slit_load_slitlist(&slit, "/tmp/fluxland-test-nonexistent-slitlist");
	assert(slit.slitlist == NULL);
	assert(slit.slitlist_count == 0);

	printf("  PASS: slit_load_slitlist_nonexistent\n");
}

static void
test_slit_save_slitlist(void)
{
	char tmppath[] = "/tmp/fluxland-test-slitlist-XXXXXX";
	int fd = mkstemp(tmppath);
	assert(fd >= 0);
	close(fd);

	struct wm_slit slit;
	memset(&slit, 0, sizeof(slit));
	wl_list_init(&slit.clients);

	/* Create mock clients with xwayland surfaces */
	struct wlr_xwayland_surface xs1 = {.class = "DockApp1"};
	struct wlr_xwayland_surface xs2 = {.class = "DockApp2"};
	struct wm_slit_client c1 = {.slit = &slit, .xsurface = &xs1};
	struct wm_slit_client c2 = {.slit = &slit, .xsurface = &xs2};
	wl_list_insert(slit.clients.prev, &c1.link);
	wl_list_insert(slit.clients.prev, &c2.link);

	slit_save_slitlist(&slit, tmppath);

	/* Read it back */
	FILE *fp = fopen(tmppath, "r");
	assert(fp != NULL);
	char line[256];

	assert(fgets(line, sizeof(line), fp) != NULL);
	line[strcspn(line, "\n")] = '\0';
	assert(strcmp(line, "DockApp1") == 0);

	assert(fgets(line, sizeof(line), fp) != NULL);
	line[strcspn(line, "\n")] = '\0';
	assert(strcmp(line, "DockApp2") == 0);

	fclose(fp);
	unlink(tmppath);
	printf("  PASS: slit_save_slitlist\n");
}

static void
test_slit_save_slitlist_null_path(void)
{
	struct wm_slit slit;
	memset(&slit, 0, sizeof(slit));
	wl_list_init(&slit.clients);

	/* Should not crash */
	slit_save_slitlist(&slit, NULL);

	printf("  PASS: slit_save_slitlist_null_path\n");
}

/* --- wm_slit_create tests --- */

static void
test_slit_create_basic(void)
{
	reset_globals();
	setup_test_server();

	struct wm_slit *slit = wm_slit_create(&test_server);
	assert(slit != NULL);
	assert(slit->server == &test_server);
	assert(slit->placement == WM_SLIT_RIGHT_CENTER);
	assert(slit->direction == WM_SLIT_VERTICAL);
	assert(slit->alpha == 255);
	assert(slit->visible == true);
	assert(wl_list_empty(&slit->clients));

	wm_slit_destroy(slit);
	printf("  PASS: slit_create_basic\n");
}

static void
test_slit_create_with_config(void)
{
	reset_globals();
	setup_test_server();
	test_config.slit_auto_hide = true;
	test_config.slit_placement = WM_SLIT_LEFT_TOP;
	test_config.slit_direction = WM_SLIT_HORIZONTAL;
	test_config.slit_alpha = 128;

	struct wm_slit *slit = wm_slit_create(&test_server);
	assert(slit != NULL);
	assert(slit->auto_hide == true);
	assert(slit->placement == WM_SLIT_LEFT_TOP);
	assert(slit->direction == WM_SLIT_HORIZONTAL);
	assert(slit->alpha == 128);

	wm_slit_destroy(slit);
	printf("  PASS: slit_create_with_config\n");
}

/* --- wm_slit_toggle_hidden tests --- */

static void
test_slit_toggle_hidden(void)
{
	reset_globals();
	setup_test_server();

	struct wm_slit *slit = wm_slit_create(&test_server);
	assert(slit->auto_hide == false);

	wm_slit_toggle_hidden(slit);
	assert(slit->auto_hide == true);

	wm_slit_toggle_hidden(slit);
	assert(slit->auto_hide == false);
	assert(slit->hidden == false);

	wm_slit_destroy(slit);
	printf("  PASS: slit_toggle_hidden\n");
}

/* --- slit_apply_alpha tests --- */

static void
test_slit_apply_alpha_basic(void)
{
	reset_globals();
	setup_test_server();
	test_config.slit_alpha = 128;

	struct wm_slit *slit = wm_slit_create(&test_server);
	assert(slit != NULL);
	assert(slit->alpha == 128);

	/* slit_apply_alpha was called during create since alpha < 255 */
	/* bg_rect and border_rect colors should have alpha applied */
	assert(g_scene_rect_set_color_count > 0);

	wm_slit_destroy(slit);
	printf("  PASS: slit_apply_alpha_basic\n");
}

static void
test_slit_apply_alpha_no_rects(void)
{
	/* Directly call slit_apply_alpha with NULL rects */
	struct wm_slit slit;
	memset(&slit, 0, sizeof(slit));
	wl_list_init(&slit.clients);
	slit.alpha = 128;
	slit.bg_rect = NULL;
	slit.border_rect = NULL;

	reset_globals();
	slit_apply_alpha(&slit);
	/* Should not crash; no rect color calls */
	assert(g_scene_rect_set_color_count == 0);
	assert(g_scene_node_for_each_buffer_count == 0);

	printf("  PASS: slit_apply_alpha_no_rects\n");
}

static void
test_slit_apply_alpha_with_clients(void)
{
	struct wm_slit slit;
	memset(&slit, 0, sizeof(slit));
	wl_list_init(&slit.clients);
	slit.alpha = 128;
	slit.bg_rect = NULL;
	slit.border_rect = NULL;

	/* Add a mapped client with a scene_tree */
	struct wlr_scene_tree ct;
	memset(&ct, 0, sizeof(ct));
	struct wm_slit_client c1 = {
		.slit = &slit, .mapped = true, .scene_tree = &ct,
		.width = 64, .height = 64
	};
	wl_list_insert(slit.clients.prev, &c1.link);

	/* Add unmapped client — should be skipped */
	struct wm_slit_client c2 = {
		.slit = &slit, .mapped = false, .scene_tree = &ct,
		.width = 32, .height = 32
	};
	wl_list_insert(slit.clients.prev, &c2.link);

	reset_globals();
	slit_apply_alpha(&slit);

	/* Only the mapped client should have for_each_buffer called */
	assert(g_scene_node_for_each_buffer_count == 1);

	printf("  PASS: slit_apply_alpha_with_clients\n");
}

/* --- wm_slit_add_client / wm_slit_remove_client tests --- */

static void
test_slit_add_client_basic(void)
{
	reset_globals();
	setup_test_server();

	struct wm_slit *slit = wm_slit_create(&test_server);
	assert(slit != NULL);
	assert(slit->client_count == 0);

	/* Create xwayland surface for the client */
	struct wlr_surface ws;
	memset(&ws, 0, sizeof(ws));
	wl_signal_init(&ws.events.map);
	wl_signal_init(&ws.events.unmap);

	struct wlr_xwayland_surface xs;
	memset(&xs, 0, sizeof(xs));
	xs.surface = &ws;
	xs.width = 48;
	xs.height = 48;
	xs.class = "TestDock";
	wl_signal_init(&xs.events.destroy);
	wl_signal_init(&xs.events.request_configure);

	struct wm_slit_client *client = wm_slit_add_client(slit, &xs);
	assert(client != NULL);
	assert(client->slit == slit);
	assert(client->xsurface == &xs);
	assert(client->width == 48);
	assert(client->height == 48);
	assert(client->mapped == false);
	assert(slit->client_count == 1);
	assert(!wl_list_empty(&slit->clients));

	wm_slit_remove_client(client);
	assert(slit->client_count == 0);
	assert(wl_list_empty(&slit->clients));

	wm_slit_destroy(slit);
	printf("  PASS: slit_add_client_basic\n");
}

static void
test_slit_add_remove_multiple_clients(void)
{
	reset_globals();
	setup_test_server();

	struct wm_slit *slit = wm_slit_create(&test_server);

	/* Create two clients */
	struct wlr_surface ws1, ws2;
	memset(&ws1, 0, sizeof(ws1));
	memset(&ws2, 0, sizeof(ws2));
	wl_signal_init(&ws1.events.map);
	wl_signal_init(&ws1.events.unmap);
	wl_signal_init(&ws2.events.map);
	wl_signal_init(&ws2.events.unmap);

	struct wlr_xwayland_surface xs1, xs2;
	memset(&xs1, 0, sizeof(xs1));
	memset(&xs2, 0, sizeof(xs2));
	xs1.surface = &ws1;
	xs1.width = 64;
	xs1.height = 64;
	xs1.class = "Dock1";
	wl_signal_init(&xs1.events.destroy);
	wl_signal_init(&xs1.events.request_configure);
	xs2.surface = &ws2;
	xs2.width = 32;
	xs2.height = 32;
	xs2.class = "Dock2";
	wl_signal_init(&xs2.events.destroy);
	wl_signal_init(&xs2.events.request_configure);

	struct wm_slit_client *c1 = wm_slit_add_client(slit, &xs1);
	struct wm_slit_client *c2 = wm_slit_add_client(slit, &xs2);
	assert(c1 != NULL && c2 != NULL);
	assert(slit->client_count == 2);

	/* Remove the first one */
	wm_slit_remove_client(c1);
	assert(slit->client_count == 1);

	/* Remove the second one */
	wm_slit_remove_client(c2);
	assert(slit->client_count == 0);
	assert(wl_list_empty(&slit->clients));

	wm_slit_destroy(slit);
	printf("  PASS: slit_add_remove_multiple_clients\n");
}

static void
test_slit_add_client_default_size(void)
{
	reset_globals();
	setup_test_server();

	struct wm_slit *slit = wm_slit_create(&test_server);

	/* Client with zero-size xwayland surface should get default 64x64 */
	struct wlr_surface ws;
	memset(&ws, 0, sizeof(ws));
	wl_signal_init(&ws.events.map);
	wl_signal_init(&ws.events.unmap);

	struct wlr_xwayland_surface xs;
	memset(&xs, 0, sizeof(xs));
	xs.surface = &ws;
	xs.width = 0;
	xs.height = 0;
	xs.class = "ZeroSize";
	wl_signal_init(&xs.events.destroy);
	wl_signal_init(&xs.events.request_configure);

	struct wm_slit_client *client = wm_slit_add_client(slit, &xs);
	assert(client != NULL);
	assert(client->width == 64);
	assert(client->height == 64);

	wm_slit_remove_client(client);
	wm_slit_destroy(slit);
	printf("  PASS: slit_add_client_default_size\n");
}

/* --- wm_slit_reconfigure tests --- */

static void
test_slit_reconfigure_with_clients(void)
{
	reset_globals();
	setup_test_server();

	struct wm_slit *slit = wm_slit_create(&test_server);
	assert(slit != NULL);

	/* Add and "map" a client manually */
	struct wlr_surface ws;
	memset(&ws, 0, sizeof(ws));
	wl_signal_init(&ws.events.map);
	wl_signal_init(&ws.events.unmap);

	struct wlr_xwayland_surface xs;
	memset(&xs, 0, sizeof(xs));
	xs.surface = &ws;
	xs.width = 64;
	xs.height = 64;
	xs.class = "ReconfigApp";
	wl_signal_init(&xs.events.destroy);
	wl_signal_init(&xs.events.request_configure);

	struct wm_slit_client *client = wm_slit_add_client(slit, &xs);
	assert(client != NULL);

	/* Simulate map */
	client->mapped = true;

	reset_globals();
	wm_slit_reconfigure(slit);

	/* Should have repositioned and resized rect nodes */
	assert(g_scene_rect_set_size_count > 0);
	assert(g_scene_node_set_position_count > 0);
	/* Slit should be enabled since it has a mapped client */
	assert(slit->scene_tree->node.enabled == 1);

	wm_slit_remove_client(client);
	wm_slit_destroy(slit);
	printf("  PASS: slit_reconfigure_with_clients\n");
}

static void
test_slit_reconfigure_max_over_off(void)
{
	reset_globals();
	setup_test_server();
	test_config.slit_max_over = false; /* Reserve usable_area space */
	test_config.slit_placement = WM_SLIT_RIGHT_CENTER;

	struct wm_slit *slit = wm_slit_create(&test_server);
	assert(slit != NULL);

	/* Add a mapped client so slit becomes visible */
	struct wlr_surface ws;
	memset(&ws, 0, sizeof(ws));
	wl_signal_init(&ws.events.map);
	wl_signal_init(&ws.events.unmap);

	struct wlr_xwayland_surface xs;
	memset(&xs, 0, sizeof(xs));
	xs.surface = &ws;
	xs.width = 64;
	xs.height = 64;
	xs.class = "MaxOverApp";
	wl_signal_init(&xs.events.destroy);
	wl_signal_init(&xs.events.request_configure);

	struct wm_slit_client *client = wm_slit_add_client(slit, &xs);
	client->mapped = true;

	/* Store original usable area */
	int orig_width = test_output.usable_area.width;

	wm_slit_reconfigure(slit);

	/* With slit_max_over=false, usable area width should be reduced */
	assert(test_output.usable_area.width < orig_width);
	assert(slit->reserved_space > 0);

	wm_slit_remove_client(client);
	wm_slit_destroy(slit);
	printf("  PASS: slit_reconfigure_max_over_off\n");
}

static void
test_slit_reconfigure_different_placements(void)
{
	/* Test reconfigure at left placement (max_over=false) */
	reset_globals();
	setup_test_server();
	test_config.slit_max_over = false;
	test_config.slit_placement = WM_SLIT_LEFT_CENTER;

	struct wm_slit *slit = wm_slit_create(&test_server);

	struct wlr_surface ws;
	memset(&ws, 0, sizeof(ws));
	wl_signal_init(&ws.events.map);
	wl_signal_init(&ws.events.unmap);

	struct wlr_xwayland_surface xs;
	memset(&xs, 0, sizeof(xs));
	xs.surface = &ws;
	xs.width = 64;
	xs.height = 64;
	xs.class = "LeftApp";
	wl_signal_init(&xs.events.destroy);
	wl_signal_init(&xs.events.request_configure);

	struct wm_slit_client *client = wm_slit_add_client(slit, &xs);
	client->mapped = true;

	int orig_x = test_output.usable_area.x;
	wm_slit_reconfigure(slit);

	/* Left placement should shift usable_area x right */
	assert(test_output.usable_area.x > orig_x);

	wm_slit_remove_client(client);
	wm_slit_destroy(slit);
	printf("  PASS: slit_reconfigure_different_placements\n");
}

/* --- wm_slit_toggle_above tests --- */

static void
test_slit_toggle_above(void)
{
	reset_globals();
	setup_test_server();

	struct wm_slit *slit = wm_slit_create(&test_server);
	assert(slit != NULL);
	assert(slit->scene_tree != NULL);

	/* Initially parented to layer_top; set up parent pointer */
	slit->scene_tree->node.parent = &test_layer_top;

	reset_globals();
	wm_slit_toggle_above(slit);
	/* Should reparent to layer_overlay */
	assert(g_scene_node_reparent_count == 1);
	assert(slit->scene_tree->node.parent == &test_layer_overlay);

	/* Toggle again: should reparent back to base layer (layer_top) */
	reset_globals();
	wm_slit_toggle_above(slit);
	assert(g_scene_node_reparent_count == 1);
	assert(slit->scene_tree->node.parent == &test_layer_top);

	wm_slit_destroy(slit);
	printf("  PASS: slit_toggle_above\n");
}

static void
test_slit_toggle_above_bottom_layer(void)
{
	reset_globals();
	setup_test_server();
	test_config.slit_layer = 2; /* Bottom layer */

	struct wm_slit *slit = wm_slit_create(&test_server);
	slit->scene_tree->node.parent = &test_layer_bottom;

	reset_globals();
	wm_slit_toggle_above(slit);
	/* Should go to overlay */
	assert(slit->scene_tree->node.parent == &test_layer_overlay);

	/* Toggle back: should go to bottom (configured base) */
	reset_globals();
	wm_slit_toggle_above(slit);
	assert(slit->scene_tree->node.parent == &test_layer_bottom);

	wm_slit_destroy(slit);
	printf("  PASS: slit_toggle_above_bottom_layer\n");
}

/* --- wm_slit_relayout tests --- */

static void
test_slit_relayout(void)
{
	reset_globals();
	setup_test_server();

	struct wm_slit *slit = wm_slit_create(&test_server);
	assert(slit != NULL);

	/* Change config values */
	test_config.slit_placement = WM_SLIT_BOTTOM_CENTER;
	test_config.slit_direction = WM_SLIT_HORIZONTAL;
	test_config.slit_alpha = 200;

	reset_globals();
	wm_slit_relayout(slit);

	/* Should have re-read config */
	assert(slit->placement == WM_SLIT_BOTTOM_CENTER);
	assert(slit->direction == WM_SLIT_HORIZONTAL);
	assert(slit->alpha == 200);

	/* Should have updated colors */
	assert(g_scene_rect_set_color_count > 0);
	/* Should have reparented (layer didn't change, but code always calls it) */
	assert(g_scene_node_reparent_count > 0);

	wm_slit_destroy(slit);
	printf("  PASS: slit_relayout\n");
}

static void
test_slit_relayout_layer_change(void)
{
	reset_globals();
	setup_test_server();

	struct wm_slit *slit = wm_slit_create(&test_server);
	assert(slit != NULL);

	/* Change to overlay layer */
	test_config.slit_layer = 6;

	reset_globals();
	wm_slit_relayout(slit);

	/* Should reparent to overlay */
	assert(g_scene_node_reparent_count > 0);
	assert(slit->scene_tree->node.parent == &test_layer_overlay);

	wm_slit_destroy(slit);
	printf("  PASS: slit_relayout_layer_change\n");
}

/* --- slit_free_slitlist tests --- */

static void
test_slit_free_slitlist_empty(void)
{
	struct wm_slit slit;
	memset(&slit, 0, sizeof(slit));
	wl_list_init(&slit.clients);

	/* NULL slitlist should not crash */
	slit_free_slitlist(&slit);
	assert(slit.slitlist == NULL);
	assert(slit.slitlist_count == 0);

	printf("  PASS: slit_free_slitlist_empty\n");
}

static void
test_slit_free_slitlist_populated(void)
{
	struct wm_slit slit;
	memset(&slit, 0, sizeof(slit));
	wl_list_init(&slit.clients);

	/* Manually allocate a slitlist */
	slit.slitlist_count = 3;
	slit.slitlist = calloc(3, sizeof(char *));
	slit.slitlist[0] = strdup("Alpha");
	slit.slitlist[1] = strdup("Beta");
	slit.slitlist[2] = strdup("Gamma");

	slit_free_slitlist(&slit);
	assert(slit.slitlist == NULL);
	assert(slit.slitlist_count == 0);

	printf("  PASS: slit_free_slitlist_populated\n");
}

/* --- null safety tests --- */

static void
test_slit_null_safety(void)
{
	wm_slit_destroy(NULL);
	wm_slit_reconfigure(NULL);
	wm_slit_relayout(NULL);
	wm_slit_toggle_above(NULL);
	wm_slit_toggle_hidden(NULL);
	assert(wm_slit_add_client(NULL, NULL) == NULL);
	wm_slit_remove_client(NULL);

	printf("  PASS: slit_null_safety\n");
}

/* --- additional coverage: slit_compute_position remaining placements --- */

static void
test_slit_position_right_top(void)
{
	struct wm_slit slit;
	memset(&slit, 0, sizeof(slit));
	slit.width = 50;
	slit.height = 200;
	slit.placement = WM_SLIT_RIGHT_TOP;
	slit_compute_position(&slit, 1920, 1080);
	assert(slit.x == 1920 - 50);
	assert(slit.y == 0);
	printf("  PASS: slit_position_right_top\n");
}

static void
test_slit_position_right_bottom(void)
{
	struct wm_slit slit;
	memset(&slit, 0, sizeof(slit));
	slit.width = 50;
	slit.height = 200;
	slit.placement = WM_SLIT_RIGHT_BOTTOM;
	slit_compute_position(&slit, 1920, 1080);
	assert(slit.x == 1920 - 50);
	assert(slit.y == 1080 - 200);
	printf("  PASS: slit_position_right_bottom\n");
}

static void
test_slit_position_bottom_left(void)
{
	struct wm_slit slit;
	memset(&slit, 0, sizeof(slit));
	slit.width = 200;
	slit.height = 50;
	slit.placement = WM_SLIT_BOTTOM_LEFT;
	slit_compute_position(&slit, 1920, 1080);
	assert(slit.x == 0);
	assert(slit.y == 1080 - 50);
	printf("  PASS: slit_position_bottom_left\n");
}

static void
test_slit_position_bottom_right(void)
{
	struct wm_slit slit;
	memset(&slit, 0, sizeof(slit));
	slit.width = 200;
	slit.height = 50;
	slit.placement = WM_SLIT_BOTTOM_RIGHT;
	slit_compute_position(&slit, 1920, 1080);
	assert(slit.x == 1920 - 200);
	assert(slit.y == 1080 - 50);
	printf("  PASS: slit_position_bottom_right\n");
}

static void
test_slit_position_left_top(void)
{
	struct wm_slit slit;
	memset(&slit, 0, sizeof(slit));
	slit.width = 50;
	slit.height = 200;
	slit.placement = WM_SLIT_LEFT_TOP;
	slit_compute_position(&slit, 1920, 1080);
	assert(slit.x == 0);
	assert(slit.y == 0);
	printf("  PASS: slit_position_left_top\n");
}

static void
test_slit_position_left_bottom(void)
{
	struct wm_slit slit;
	memset(&slit, 0, sizeof(slit));
	slit.width = 50;
	slit.height = 200;
	slit.placement = WM_SLIT_LEFT_BOTTOM;
	slit_compute_position(&slit, 1920, 1080);
	assert(slit.x == 0);
	assert(slit.y == 1080 - 200);
	printf("  PASS: slit_position_left_bottom\n");
}

/* --- additional coverage: slit_hide_timer_cb edge cases --- */

static void
test_slit_hide_timer_no_auto_hide(void)
{
	struct wm_slit slit;
	memset(&slit, 0, sizeof(slit));
	wl_list_init(&slit.clients);
	slit.auto_hide = false;
	slit.hidden = false;

	/* Timer fires but auto_hide is false: nothing should change */
	slit_hide_timer_cb(&slit);
	assert(slit.hidden == false);
	printf("  PASS: slit_hide_timer_no_auto_hide\n");
}

static void
test_slit_hide_timer_already_hidden(void)
{
	struct wm_slit slit;
	memset(&slit, 0, sizeof(slit));
	wl_list_init(&slit.clients);
	slit.auto_hide = true;
	slit.hidden = true;

	/* Timer fires but already hidden: no-op */
	slit_hide_timer_cb(&slit);
	assert(slit.hidden == true);
	printf("  PASS: slit_hide_timer_already_hidden\n");
}

/* --- additional coverage: slit_layout_clients horizontal min_size --- */

static void
test_slit_layout_horizontal_min_size(void)
{
	struct wm_slit slit;
	memset(&slit, 0, sizeof(slit));
	wl_list_init(&slit.clients);
	slit.direction = WM_SLIT_HORIZONTAL;

	/* No mapped clients => min size applied */
	slit_layout_clients(&slit);
	assert(slit.width >= WM_SLIT_MIN_SIZE);
	assert(slit.height >= WM_SLIT_MIN_SIZE);
	printf("  PASS: slit_layout_horizontal_min_size\n");
}

/* --- additional coverage: get_slit_output with non-zero on_head --- */

static void
test_slit_get_output_second_head(void)
{
	reset_globals();
	setup_test_server();

	struct wm_slit *slit = wm_slit_create(&test_server);
	assert(slit != NULL);

	/* Add a second output */
	struct wm_output second_output;
	memset(&second_output, 0, sizeof(second_output));
	struct wlr_output second_wlr = {.width = 1920, .height = 1080};
	second_output.wlr_output = &second_wlr;
	second_output.usable_area = (struct wlr_box){0, 0, 1920, 1080};
	wl_list_insert(test_server.outputs.prev, &second_output.link);

	/* Set on_head = 1 to target the second output */
	slit->on_head = 1;
	wm_slit_reconfigure(slit);

	/* Slit should be positioned based on second output */
	/* (just verifying no crash with multi-head) */

	wl_list_remove(&second_output.link);
	wm_slit_destroy(slit);
	printf("  PASS: slit_get_output_second_head\n");
}

/* --- additional coverage: create with overlay layer --- */

static void
test_slit_create_overlay_layer(void)
{
	reset_globals();
	setup_test_server();

	test_config.slit_layer = 6; /* AboveDock/Overlay */
	struct wm_slit *slit = wm_slit_create(&test_server);
	assert(slit != NULL);

	wm_slit_destroy(slit);
	printf("  PASS: slit_create_overlay_layer\n");
}

static void
test_slit_create_bottom_layer(void)
{
	reset_globals();
	setup_test_server();

	test_config.slit_layer = 2; /* Bottom */
	struct wm_slit *slit = wm_slit_create(&test_server);
	assert(slit != NULL);

	wm_slit_destroy(slit);
	printf("  PASS: slit_create_bottom_layer\n");
}

/* --- additional coverage: create with alpha < 255 --- */

static void
test_slit_create_with_alpha(void)
{
	reset_globals();
	setup_test_server();

	test_config.slit_alpha = 128;
	struct wm_slit *slit = wm_slit_create(&test_server);
	assert(slit != NULL);
	assert(slit->alpha == 128);

	wm_slit_destroy(slit);
	printf("  PASS: slit_create_with_alpha\n");
}

/* --- additional coverage: create with border --- */

static void
test_slit_create_with_border(void)
{
	reset_globals();
	setup_test_server();

	test_style.slit_border_width = 2;
	test_style.slit_border_color = (struct wm_color){128, 128, 128, 255};
	struct wm_slit *slit = wm_slit_create(&test_server);
	assert(slit != NULL);
	assert(slit->border_rect != NULL);

	wm_slit_destroy(slit);
	printf("  PASS: slit_create_with_border\n");
}

/* --- additional coverage: reconfigure with max_over disabled + different placements --- */

static void
test_slit_reconfigure_max_over_left_placements(void)
{
	reset_globals();
	setup_test_server();
	test_config.slit_max_over = false;

	struct wm_slit *slit = wm_slit_create(&test_server);
	assert(slit != NULL);

	/* Add a mapped client so has_clients is true */
	struct wlr_surface surface;
	memset(&surface, 0, sizeof(surface));
	wl_signal_init(&surface.events.map);
	wl_signal_init(&surface.events.unmap);
	struct wlr_xwayland_surface xsurf;
	memset(&xsurf, 0, sizeof(xsurf));
	xsurf.surface = &surface;
	xsurf.width = 64;
	xsurf.height = 64;
	wl_signal_init(&xsurf.events.destroy);
	wl_signal_init(&xsurf.events.request_configure);

	struct wm_slit_client *client = wm_slit_add_client(slit, &xsurf);
	assert(client != NULL);
	client->mapped = true;

	/* Test LEFT_TOP placement */
	slit->placement = WM_SLIT_LEFT_TOP;
	wm_slit_reconfigure(slit);

	/* Test TOP_LEFT placement */
	slit->placement = WM_SLIT_TOP_LEFT;
	wm_slit_reconfigure(slit);

	/* Test BOTTOM_LEFT placement */
	slit->placement = WM_SLIT_BOTTOM_LEFT;
	wm_slit_reconfigure(slit);

	wm_slit_destroy(slit);
	printf("  PASS: slit_reconfigure_max_over_left_placements\n");
}

/* --- additional coverage: reconfigure hidden slit undoes reservation --- */

static void
test_slit_reconfigure_hidden_undo_reservation(void)
{
	reset_globals();
	setup_test_server();
	test_config.slit_max_over = false;

	struct wm_slit *slit = wm_slit_create(&test_server);
	assert(slit != NULL);

	/* Simulate previous reservation */
	slit->reserved_space = 50;
	slit->placement = WM_SLIT_LEFT_CENTER;

	/* Reconfigure with no clients => undo reservation */
	wm_slit_reconfigure(slit);

	assert(slit->reserved_space == 0);

	wm_slit_destroy(slit);
	printf("  PASS: slit_reconfigure_hidden_undo_reservation\n");
}

/* --- additional coverage: relayout with bottom layer --- */

static void
test_slit_relayout_bottom_layer(void)
{
	reset_globals();
	setup_test_server();

	struct wm_slit *slit = wm_slit_create(&test_server);
	assert(slit != NULL);

	/* Change to bottom layer */
	test_config.slit_layer = 2;

	reset_globals();
	wm_slit_relayout(slit);

	assert(g_scene_node_reparent_count > 0);

	wm_slit_destroy(slit);
	printf("  PASS: slit_relayout_bottom_layer\n");
}

/* --- additional coverage: relayout updates style colors --- */

static void
test_slit_relayout_updates_colors(void)
{
	reset_globals();
	setup_test_server();
	test_style.slit_border_width = 2;
	test_style.slit_border_color = (struct wm_color){255, 0, 0, 255};

	struct wm_slit *slit = wm_slit_create(&test_server);
	assert(slit != NULL);

	/* Change colors in style and relayout */
	test_style.slit_texture.color = 0xFF00FF00; /* green */
	test_style.slit_border_color = (struct wm_color){0, 0, 255, 255};

	reset_globals();
	wm_slit_relayout(slit);

	/* Should have called set_color for bg and border rects */
	assert(g_scene_rect_set_color_count >= 2);

	wm_slit_destroy(slit);
	printf("  PASS: slit_relayout_updates_colors\n");
}

/* --- additional coverage: relayout with alpha < 255 --- */

static void
test_slit_relayout_with_alpha(void)
{
	reset_globals();
	setup_test_server();
	test_config.slit_alpha = 200;

	struct wm_slit *slit = wm_slit_create(&test_server);
	assert(slit != NULL);

	reset_globals();
	wm_slit_relayout(slit);

	/* Should still apply alpha */
	assert(slit->alpha == 200);

	wm_slit_destroy(slit);
	printf("  PASS: slit_relayout_with_alpha\n");
}

/* --- additional coverage: toggle_above with bottom config --- */

static void
test_slit_toggle_above_overlay_to_base(void)
{
	reset_globals();
	setup_test_server();

	struct wm_slit *slit = wm_slit_create(&test_server);
	assert(slit != NULL);

	/* Move to overlay first */
	slit->scene_tree->node.parent = &test_layer_overlay;

	reset_globals();
	wm_slit_toggle_above(slit);

	/* Should reparent back to configured base layer (layer_top) */
	assert(g_scene_node_reparent_count > 0);
	assert(slit->scene_tree->node.parent == &test_layer_top);

	wm_slit_destroy(slit);
	printf("  PASS: slit_toggle_above_overlay_to_base\n");
}

/* --- additional coverage: toggle_hidden enable then disable --- */

static void
test_slit_toggle_hidden_cycle(void)
{
	reset_globals();
	setup_test_server();

	struct wm_slit *slit = wm_slit_create(&test_server);
	assert(slit != NULL);
	assert(slit->auto_hide == false);

	/* Toggle on */
	wm_slit_toggle_hidden(slit);
	assert(slit->auto_hide == true);

	/* Toggle off */
	wm_slit_toggle_hidden(slit);
	assert(slit->auto_hide == false);
	assert(slit->hidden == false);

	wm_slit_destroy(slit);
	printf("  PASS: slit_toggle_hidden_cycle\n");
}

/* ===== XWayland slit client handler tests ===== */

static void
test_slit_client_map_basic(void)
{
	reset_globals();
	setup_test_server();

	struct wm_slit *slit = wm_slit_create(&test_server);
	assert(slit != NULL);

	/* Create a fake xwayland surface */
	struct wlr_surface fake_surface;
	memset(&fake_surface, 0, sizeof(fake_surface));
	wl_signal_init(&fake_surface.events.map);
	wl_signal_init(&fake_surface.events.unmap);

	struct wlr_xwayland_surface xsurf;
	memset(&xsurf, 0, sizeof(xsurf));
	xsurf.surface = &fake_surface;
	xsurf.width = 48;
	xsurf.height = 48;
	xsurf.title = "test-dock";
	wl_signal_init(&xsurf.events.destroy);
	wl_signal_init(&xsurf.events.request_configure);

	struct wm_slit_client *client = wm_slit_add_client(slit, &xsurf);
	assert(client != NULL);
	assert(client->mapped == false);

	/* Invoke the map handler directly */
	handle_slit_client_map(&client->map, NULL);
	assert(client->mapped == true);
	assert(client->width == 48);
	assert(client->height == 48);

	/* Clean up */
	wm_slit_remove_client(client);
	wm_slit_destroy(slit);
	printf("  PASS: slit_client_map_basic\n");
}

static void
test_slit_client_map_with_alpha(void)
{
	reset_globals();
	setup_test_server();
	test_config.slit_alpha = 128; /* partial alpha */

	struct wm_slit *slit = wm_slit_create(&test_server);
	assert(slit != NULL);

	struct wlr_surface fake_surface;
	memset(&fake_surface, 0, sizeof(fake_surface));
	wl_signal_init(&fake_surface.events.map);
	wl_signal_init(&fake_surface.events.unmap);

	struct wlr_xwayland_surface xsurf;
	memset(&xsurf, 0, sizeof(xsurf));
	xsurf.surface = &fake_surface;
	xsurf.width = 64;
	xsurf.height = 64;
	wl_signal_init(&xsurf.events.destroy);
	wl_signal_init(&xsurf.events.request_configure);

	struct wm_slit_client *client = wm_slit_add_client(slit, &xsurf);
	assert(client != NULL);

	/* Give it a scene tree so the alpha path executes */
	struct wlr_scene_tree scene_tree;
	memset(&scene_tree, 0, sizeof(scene_tree));
	client->scene_tree = &scene_tree;

	handle_slit_client_map(&client->map, NULL);
	assert(client->mapped == true);
	/* Alpha < 255 and scene_tree non-NULL should trigger for_each_buffer */
	assert(g_scene_node_for_each_buffer_count > 0);

	client->scene_tree = NULL; /* prevent double-destroy */
	wm_slit_remove_client(client);
	wm_slit_destroy(slit);
	printf("  PASS: slit_client_map_with_alpha\n");
}

static void
test_slit_client_unmap_basic(void)
{
	reset_globals();
	setup_test_server();

	struct wm_slit *slit = wm_slit_create(&test_server);
	assert(slit != NULL);

	struct wlr_surface fake_surface;
	memset(&fake_surface, 0, sizeof(fake_surface));
	wl_signal_init(&fake_surface.events.map);
	wl_signal_init(&fake_surface.events.unmap);

	struct wlr_xwayland_surface xsurf;
	memset(&xsurf, 0, sizeof(xsurf));
	xsurf.surface = &fake_surface;
	xsurf.width = 32;
	xsurf.height = 32;
	wl_signal_init(&xsurf.events.destroy);
	wl_signal_init(&xsurf.events.request_configure);

	struct wm_slit_client *client = wm_slit_add_client(slit, &xsurf);
	assert(client != NULL);

	/* Map first */
	handle_slit_client_map(&client->map, NULL);
	assert(client->mapped == true);

	/* Give it a scene tree so the destroy path runs */
	struct wlr_scene_tree scene_tree;
	memset(&scene_tree, 0, sizeof(scene_tree));
	client->scene_tree = &scene_tree;

	int before_destroy = g_scene_node_destroy_count;
	handle_slit_client_unmap(&client->unmap, NULL);
	assert(client->mapped == false);
	assert(client->scene_tree == NULL);
	assert(g_scene_node_destroy_count == before_destroy + 1);

	wm_slit_remove_client(client);
	wm_slit_destroy(slit);
	printf("  PASS: slit_client_unmap_basic\n");
}

static void
test_slit_client_destroy_basic(void)
{
	reset_globals();
	setup_test_server();

	struct wm_slit *slit = wm_slit_create(&test_server);
	assert(slit != NULL);

	struct wlr_surface fake_surface;
	memset(&fake_surface, 0, sizeof(fake_surface));
	wl_signal_init(&fake_surface.events.map);
	wl_signal_init(&fake_surface.events.unmap);

	struct wlr_xwayland_surface xsurf;
	memset(&xsurf, 0, sizeof(xsurf));
	xsurf.surface = &fake_surface;
	xsurf.width = 64;
	xsurf.height = 64;
	wl_signal_init(&xsurf.events.destroy);
	wl_signal_init(&xsurf.events.request_configure);

	struct wm_slit_client *client = wm_slit_add_client(slit, &xsurf);
	assert(client != NULL);
	assert(slit->client_count == 1);

	/* Destroy handler removes client from slit */
	handle_slit_client_destroy(&client->destroy, NULL);
	assert(slit->client_count == 0);

	wm_slit_destroy(slit);
	printf("  PASS: slit_client_destroy_basic\n");
}

static void
test_slit_client_configure_basic(void)
{
	reset_globals();
	setup_test_server();

	struct wm_slit *slit = wm_slit_create(&test_server);
	assert(slit != NULL);

	struct wlr_surface fake_surface;
	memset(&fake_surface, 0, sizeof(fake_surface));
	wl_signal_init(&fake_surface.events.map);
	wl_signal_init(&fake_surface.events.unmap);

	struct wlr_xwayland_surface xsurf;
	memset(&xsurf, 0, sizeof(xsurf));
	xsurf.surface = &fake_surface;
	xsurf.width = 64;
	xsurf.height = 64;
	wl_signal_init(&xsurf.events.destroy);
	wl_signal_init(&xsurf.events.request_configure);

	struct wm_slit_client *client = wm_slit_add_client(slit, &xsurf);
	assert(client != NULL);

	/* Send configure event with new size */
	struct wlr_xwayland_surface_configure_event event = {
		.x = 100, .y = 200, .width = 96, .height = 96,
	};
	handle_slit_client_configure(&client->configure, &event);
	assert(client->width == 96);
	assert(client->height == 96);
	assert(g_xwayland_configure_count > 0);

	wm_slit_remove_client(client);
	wm_slit_destroy(slit);
	printf("  PASS: slit_client_configure_basic\n");
}

static void
test_slit_client_configure_while_mapped(void)
{
	reset_globals();
	setup_test_server();

	struct wm_slit *slit = wm_slit_create(&test_server);
	assert(slit != NULL);

	struct wlr_surface fake_surface;
	memset(&fake_surface, 0, sizeof(fake_surface));
	wl_signal_init(&fake_surface.events.map);
	wl_signal_init(&fake_surface.events.unmap);

	struct wlr_xwayland_surface xsurf;
	memset(&xsurf, 0, sizeof(xsurf));
	xsurf.surface = &fake_surface;
	xsurf.width = 64;
	xsurf.height = 64;
	wl_signal_init(&xsurf.events.destroy);
	wl_signal_init(&xsurf.events.request_configure);

	struct wm_slit_client *client = wm_slit_add_client(slit, &xsurf);
	assert(client != NULL);

	/* Map first, then configure */
	handle_slit_client_map(&client->map, NULL);
	assert(client->mapped == true);

	struct wlr_xwayland_surface_configure_event event = {
		.x = 0, .y = 0, .width = 128, .height = 128,
	};
	handle_slit_client_configure(&client->configure, &event);
	assert(client->width == 128);
	assert(client->height == 128);
	/* When mapped, reconfigure should be called */

	wm_slit_remove_client(client);
	wm_slit_destroy(slit);
	printf("  PASS: slit_client_configure_while_mapped\n");
}

static void
test_slit_client_map_no_title(void)
{
	reset_globals();
	setup_test_server();

	struct wm_slit *slit = wm_slit_create(&test_server);
	assert(slit != NULL);

	struct wlr_surface fake_surface;
	memset(&fake_surface, 0, sizeof(fake_surface));
	wl_signal_init(&fake_surface.events.map);
	wl_signal_init(&fake_surface.events.unmap);

	struct wlr_xwayland_surface xsurf;
	memset(&xsurf, 0, sizeof(xsurf));
	xsurf.surface = &fake_surface;
	xsurf.width = 32;
	xsurf.height = 32;
	xsurf.title = NULL; /* no title */
	wl_signal_init(&xsurf.events.destroy);
	wl_signal_init(&xsurf.events.request_configure);

	struct wm_slit_client *client = wm_slit_add_client(slit, &xsurf);
	assert(client != NULL);

	/* Map with NULL title (exercises the ternary in wlr_log) */
	handle_slit_client_map(&client->map, NULL);
	assert(client->mapped == true);

	wm_slit_remove_client(client);
	wm_slit_destroy(slit);
	printf("  PASS: slit_client_map_no_title\n");
}

int
main(void)
{
	printf("test_slit:\n");

	/* slit_compute_position */
	test_slit_position_top_left();
	test_slit_position_top_center();
	test_slit_position_top_right();
	test_slit_position_right_center();
	test_slit_position_bottom_center();
	test_slit_position_left_center();
	test_slit_position_all_placements();

	/* slit_layout_clients */
	test_slit_layout_vertical();
	test_slit_layout_horizontal();
	test_slit_layout_unmapped_skipped();
	test_slit_layout_empty();
	test_slit_layout_min_size();

	/* slit_hide_timer_cb */
	test_slit_hide_timer_cb();

	/* slit_find_insert_position */
	test_slit_find_insert_null_class();
	test_slit_find_insert_empty_slitlist();
	test_slit_find_insert_not_in_list();
	test_slit_find_insert_ordering();

	/* slitlist I/O */
	test_slit_load_slitlist_roundtrip();
	test_slit_load_slitlist_empty_lines();
	test_slit_load_slitlist_null_path();
	test_slit_load_slitlist_nonexistent();
	test_slit_save_slitlist();
	test_slit_save_slitlist_null_path();

	/* slit_apply_alpha */
	test_slit_apply_alpha_basic();
	test_slit_apply_alpha_no_rects();
	test_slit_apply_alpha_with_clients();

	/* add/remove client */
	test_slit_add_client_basic();
	test_slit_add_remove_multiple_clients();
	test_slit_add_client_default_size();

	/* reconfigure */
	test_slit_reconfigure_with_clients();
	test_slit_reconfigure_max_over_off();
	test_slit_reconfigure_different_placements();

	/* toggle_above */
	test_slit_toggle_above();
	test_slit_toggle_above_bottom_layer();

	/* relayout */
	test_slit_relayout();
	test_slit_relayout_layer_change();

	/* free_slitlist */
	test_slit_free_slitlist_empty();
	test_slit_free_slitlist_populated();

	/* create/destroy */
	test_slit_create_basic();
	test_slit_create_with_config();
	test_slit_toggle_hidden();
	test_slit_null_safety();

	/* additional coverage: remaining placements */
	test_slit_position_right_top();
	test_slit_position_right_bottom();
	test_slit_position_bottom_left();
	test_slit_position_bottom_right();
	test_slit_position_left_top();
	test_slit_position_left_bottom();

	/* additional coverage: timer edge cases */
	test_slit_hide_timer_no_auto_hide();
	test_slit_hide_timer_already_hidden();

	/* additional coverage: layout */
	test_slit_layout_horizontal_min_size();

	/* additional coverage: multi-head / layer / alpha / border */
	test_slit_get_output_second_head();
	test_slit_create_overlay_layer();
	test_slit_create_bottom_layer();
	test_slit_create_with_alpha();
	test_slit_create_with_border();

	/* additional coverage: reconfigure edge cases */
	test_slit_reconfigure_max_over_left_placements();
	test_slit_reconfigure_hidden_undo_reservation();

	/* additional coverage: relayout edge cases */
	test_slit_relayout_bottom_layer();
	test_slit_relayout_updates_colors();
	test_slit_relayout_with_alpha();

	/* additional coverage: toggle */
	test_slit_toggle_above_overlay_to_base();
	test_slit_toggle_hidden_cycle();

	/* XWayland slit client handler coverage */
	test_slit_client_map_basic();
	test_slit_client_map_with_alpha();
	test_slit_client_unmap_basic();
	test_slit_client_destroy_basic();
	test_slit_client_configure_basic();
	test_slit_client_configure_while_mapped();
	test_slit_client_map_no_title();

	printf("All slit tests passed.\n");
	return 0;
}
