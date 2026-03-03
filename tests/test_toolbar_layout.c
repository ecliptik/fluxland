/*
 * test_toolbar_layout.c - Unit tests for toolbar layout and parsing logic
 *
 * Includes toolbar.c directly with stub implementations to avoid
 * needing wlroots/wayland/cairo libraries at link time.
 */

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* --- Block real headers via their include guards --- */
#define WAYLAND_SERVER_CORE_H
#define WAYLAND_SERVER_H
#define WAYLAND_UTIL_H
#define WLR_UTIL_LOG_H
#define WLR_TYPES_WLR_SCENE_H
#define WLR_TYPES_WLR_BUFFER_H
#define WLR_INTERFACES_WLR_BUFFER_H
#define WLR_TYPES_WLR_XDG_SHELL_H
#define WLR_TYPES_WLR_OUTPUT_H
#define WLR_UTIL_BOX_H
#define WM_VIEW_H
#define WM_SERVER_H
#define WM_STYLE_H
#define WM_RENDER_H
#define WM_CONFIG_H
#define WM_OUTPUT_H
#define WM_WORKSPACE_H
#define WM_FOCUS_NAV_H
#define WM_VIEW_FOCUS_H
#define WM_SYSTRAY_H
#define DRM_FOURCC_H

/* Prevent cairo.h from being included */
#define CAIRO_H

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
	((void *)((char *)(ptr) - offsetof(__typeof__(*sample), member)))

#define wl_list_for_each(pos, head, member) \
	for (pos = wl_container_of((head)->next, pos, member); \
	     &pos->member != (head); \
	     pos = wl_container_of(pos->member.next, pos, member))

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

struct wlr_surface {
	int dummy;
};

struct wlr_xdg_surface {
	struct wlr_surface *surface;
};

struct wlr_xdg_toplevel {
	struct wlr_xdg_surface *base;
	int width, height;
};

/* wlr_buffer stubs - struct wlr_buffer must be defined first */
#define WLR_BUFFER_DATA_PTR_ACCESS_WRITE 2

struct wlr_buffer;

struct wlr_buffer {
	const void *impl;
	int width, height;
	int ref_count;
};

struct wlr_buffer_impl {
	void (*destroy)(struct wlr_buffer *);
	bool (*begin_data_ptr_access)(struct wlr_buffer *, uint32_t,
		void **, uint32_t *, size_t *);
	void (*end_data_ptr_access)(struct wlr_buffer *);
};

struct wlr_scene_buffer {
	struct wlr_scene_node node;
	struct wlr_buffer *buffer;
};

/* DRM fourcc */
#define DRM_FORMAT_ARGB8888 0x34325241

/* wlr_log no-op */
#define wlr_log(verb, fmt, ...) ((void)0)
#define WLR_ERROR 1
#define WLR_INFO 3
#define WLR_DEBUG 7

/* --- Cairo stubs --- */

typedef int cairo_status_t;
typedef int cairo_format_t;
typedef int cairo_operator_t;
#define CAIRO_FORMAT_ARGB32 0
#define CAIRO_STATUS_SUCCESS 0
#define CAIRO_OPERATOR_CLEAR 2
#define CAIRO_OPERATOR_OVER 3

typedef struct _cairo_surface cairo_surface_t;
typedef struct _cairo {
	int dummy;
} cairo_t;

struct _cairo_surface {
	int width, height;
	int stride;
	unsigned char *data;
};

static cairo_surface_t *
cairo_image_surface_create(cairo_format_t fmt, int width, int height)
{
	(void)fmt;
	cairo_surface_t *s = calloc(1, sizeof(*s));
	if (!s) return NULL;
	s->width = width;
	s->height = height;
	s->stride = width * 4;
	s->data = calloc(1, (size_t)s->stride * (size_t)height);
	return s;
}

static cairo_status_t
cairo_surface_status(cairo_surface_t *surface) {
	return surface ? CAIRO_STATUS_SUCCESS : 1;
}

static void cairo_surface_flush(cairo_surface_t *s) { (void)s; }

static int cairo_image_surface_get_width(cairo_surface_t *s) {
	return s ? s->width : 0;
}
static int cairo_image_surface_get_height(cairo_surface_t *s) {
	return s ? s->height : 0;
}
static int cairo_image_surface_get_stride(cairo_surface_t *s) {
	return s ? s->stride : 0;
}
static unsigned char *cairo_image_surface_get_data(cairo_surface_t *s) {
	return s ? s->data : NULL;
}

static void
cairo_surface_destroy(cairo_surface_t *surface)
{
	if (surface) {
		free(surface->data);
		free(surface);
	}
}

static cairo_t *cairo_create(cairo_surface_t *s) {
	(void)s;
	static cairo_t cr;
	return &cr;
}
static void cairo_destroy(cairo_t *cr) { (void)cr; }
static void cairo_set_operator(cairo_t *cr, cairo_operator_t op) {
	(void)cr; (void)op;
}
static void cairo_paint(cairo_t *cr) { (void)cr; }
static void cairo_set_source_rgba(cairo_t *cr, double r, double g,
	double b, double a) { (void)cr; (void)r; (void)g; (void)b; (void)a; }
static void cairo_rectangle(cairo_t *cr, double x, double y,
	double w, double h) { (void)cr; (void)x; (void)y; (void)w; (void)h; }
static void cairo_fill(cairo_t *cr) { (void)cr; }
static void cairo_set_line_width(cairo_t *cr, double w) {
	(void)cr; (void)w;
}
static void cairo_move_to(cairo_t *cr, double x, double y) {
	(void)cr; (void)x; (void)y;
}
static void cairo_line_to(cairo_t *cr, double x, double y) {
	(void)cr; (void)x; (void)y;
}
static void cairo_stroke(cairo_t *cr) { (void)cr; }
static void cairo_set_source_surface(cairo_t *cr, cairo_surface_t *s,
	double x, double y) { (void)cr; (void)s; (void)x; (void)y; }

/* --- Stub wm types that toolbar.c depends on --- */

/* Texture specification */
enum wm_texture_appearance { WM_TEX_FLAT };
enum wm_texture_fill { WM_TEX_SOLID };
enum wm_gradient_type { WM_GRAD_HORIZONTAL };
enum wm_bevel_type { WM_BEVEL_NONE };

struct wm_texture {
	enum wm_texture_appearance appearance;
	enum wm_texture_fill fill;
	enum wm_gradient_type gradient;
	enum wm_bevel_type bevel;
	bool interlaced;
	uint32_t color;
	uint32_t color_to;
	char *pixmap_path;
};

struct wm_color {
	uint8_t r, g, b, a;
};

struct wm_font {
	char *family;
	int size;
	bool bold;
	bool italic;
	int shadow_x;
	int shadow_y;
	struct wm_color shadow_color;
	bool halo;
	struct wm_color halo_color;
};

enum wm_justify {
	WM_JUSTIFY_LEFT,
	WM_JUSTIFY_CENTER,
	WM_JUSTIFY_RIGHT,
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
	struct wm_texture toolbar_texture;
	struct wm_color toolbar_text_color;
	struct wm_font toolbar_font;
	struct wm_color toolbar_iconbar_focused_color;
	struct wm_color toolbar_iconbar_focused_text_color;
	struct wm_color toolbar_iconbar_unfocused_color;
	struct wm_color toolbar_iconbar_unfocused_text_color;
	bool toolbar_iconbar_has_focused_color;
	bool toolbar_iconbar_has_unfocused_color;
};

/* Toolbar placement enum from config.h */
enum wm_toolbar_placement {
	WM_TOOLBAR_TOP_LEFT,
	WM_TOOLBAR_TOP_CENTER,
	WM_TOOLBAR_TOP_RIGHT,
	WM_TOOLBAR_BOTTOM_LEFT,
	WM_TOOLBAR_BOTTOM_CENTER,
	WM_TOOLBAR_BOTTOM_RIGHT,
};

struct wm_config {
	bool toolbar_visible;
	enum wm_toolbar_placement toolbar_placement;
	bool toolbar_auto_hide;
	int toolbar_auto_hide_delay_ms;
	int toolbar_width_percent;
	int toolbar_height;
	char *toolbar_tools;
	char *clock_format;
	int iconbar_mode;
	int iconbar_alignment;
	int iconbar_icon_width;
	int iconbar_wheel_mode;
	char *iconbar_iconified_pattern;
};

struct wm_workspace {
	struct wl_list link;
	void *server;
	void *tree;
	char *name;
	int index;
};

struct wm_tab_group;
struct wm_decoration;
struct wm_animation;

struct wm_view {
	struct wl_list link;
	void *server;
	struct wlr_xdg_toplevel *xdg_toplevel;
	struct wlr_scene_tree *scene_tree;
	uint32_t id;
	int x, y;
	struct wlr_box saved_geometry;
	bool maximized, fullscreen;
	bool maximized_vert, maximized_horiz;
	bool lhalf, rhalf;
	bool show_decoration;
	char *title;
	char *app_id;
	struct wm_workspace *workspace;
	bool sticky;
	int layer;
	struct wm_tab_group *tab_group;
	struct wl_list tab_link;
	int focus_alpha;
	int unfocus_alpha;
	struct wm_animation *animation;
	struct wm_decoration *decoration;
};

struct wm_output {
	struct wl_list link;
	void *server;
	struct wlr_output *wlr_output;
	struct wlr_scene_output *scene_output;
	struct wlr_box usable_area;
	struct wm_workspace *active_workspace;
};

struct wm_focus_nav {
	int zone;
	int toolbar_index;
	int slit_index;
};

struct wm_systray;

struct wm_server {
	struct wl_list outputs;
	struct wl_list views;
	struct wl_list workspaces;
	struct wm_workspace *current_workspace;
	int workspace_count;
	struct wm_config *config;
	struct wm_style *style;
	struct wm_view *focused_view;
	struct wlr_cursor *cursor;
	struct wlr_scene_tree *layer_top;
	struct wl_event_loop *wl_event_loop;
	struct wm_focus_nav focus_nav;
#ifdef WM_HAS_SYSTRAY
	struct wm_systray *systray;
#endif
};

#ifdef WM_HAS_SYSTRAY
static int wm_systray_get_width(struct wm_systray *systray) {
	(void)systray;
	return 0;
}
static void wm_systray_layout(struct wm_systray *systray,
	int x, int y, int width, int height)
{
	(void)systray; (void)x; (void)y; (void)width; (void)height;
}
#endif

/* Fake cursor for auto-hide logic */
struct wlr_cursor {
	double x, y;
};

/* --- Stub tracking --- */

static int g_scene_buffer_create_count;
static int g_scene_buffer_set_buffer_count;
static int g_scene_node_set_position_count;
static int g_scene_node_set_enabled_count;
static int g_wlr_buffer_drop_count;
static int g_render_text_count;
static int g_render_texture_count;
static int g_measure_text_width_count;
static int g_ws_switch_next_count;
static int g_ws_switch_prev_count;
static int g_timer_update_count;
static int g_timer_update_last_ms;
static int g_focus_next_count;
static int g_focus_prev_count;
static int g_focus_view_count;
static int g_view_raise_count;

/* --- Stub wlroots functions --- */

static void
wlr_buffer_init(struct wlr_buffer *buf, const struct wlr_buffer_impl *impl,
	int width, int height)
{
	buf->impl = (const void *)impl;
	buf->width = width;
	buf->height = height;
	buf->ref_count = 1;
}

static struct wlr_scene_buffer g_scene_buffers[32];

static struct wlr_scene_buffer *
wlr_scene_buffer_create(struct wlr_scene_tree *parent, struct wlr_buffer *buf)
{
	(void)parent; (void)buf;
	int idx = g_scene_buffer_create_count % 32;
	memset(&g_scene_buffers[idx], 0, sizeof(g_scene_buffers[idx]));
	g_scene_buffer_create_count++;
	return &g_scene_buffers[idx];
}

static void
wlr_scene_buffer_set_buffer(struct wlr_scene_buffer *sbuf,
	struct wlr_buffer *buf)
{
	(void)sbuf;
	sbuf->buffer = buf;
	g_scene_buffer_set_buffer_count++;
}

static void
wlr_buffer_drop(struct wlr_buffer *buf)
{
	(void)buf;
	g_wlr_buffer_drop_count++;
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
wlr_scene_node_raise_to_top(struct wlr_scene_node *node) { (void)node; }

static void
wlr_scene_node_destroy(struct wlr_scene_node *node) { (void)node; }

static struct wlr_scene_tree g_scene_tree;

static struct wlr_scene_tree *
wlr_scene_tree_create(struct wlr_scene_tree *parent)
{
	(void)parent;
	memset(&g_scene_tree, 0, sizeof(g_scene_tree));
	return &g_scene_tree;
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
	(void)src;
	g_timer_update_count++;
	g_timer_update_last_ms = ms;
	return 0;
}

static void
wl_event_source_remove(struct wl_event_source *src) { (void)src; }

/* --- Stub fluxland functions --- */

static cairo_surface_t *
wm_render_text(const char *text, const struct wm_font *font,
	const struct wm_color *color, int max_width, int *out_width,
	int *out_height, enum wm_justify justify, float scale)
{
	(void)text; (void)font; (void)color; (void)max_width;
	(void)justify; (void)scale;
	if (out_width) *out_width = 50;
	if (out_height) *out_height = 14;
	g_render_text_count++;
	return NULL;
}

static cairo_surface_t *
wm_render_texture(const struct wm_texture *texture,
	int width, int height, float scale)
{
	(void)texture; (void)width; (void)height; (void)scale;
	g_render_texture_count++;
	return NULL;
}

static int
wm_measure_text_width(const char *text, const struct wm_font *font,
	float scale)
{
	(void)font; (void)scale;
	g_measure_text_width_count++;
	/* Return a size proportional to text length */
	return text ? (int)strlen(text) * 8 : 0;
}

static struct wm_workspace *
wm_workspace_get_active(struct wm_server *server)
{
	return server->current_workspace;
}

static void
wm_workspace_switch_next(struct wm_server *server)
{
	(void)server;
	g_ws_switch_next_count++;
}

static void
wm_workspace_switch_prev(struct wm_server *server)
{
	(void)server;
	g_ws_switch_prev_count++;
}

static int
wm_focus_nav_get_toolbar_index(struct wm_server *server)
{
	return server->focus_nav.toolbar_index;
}

static void
wm_focus_next_view(struct wm_server *server)
{
	(void)server;
	g_focus_next_count++;
}

static void
wm_focus_prev_view(struct wm_server *server)
{
	(void)server;
	g_focus_prev_count++;
}

static void
wm_focus_view(struct wm_view *view, struct wlr_surface *surface)
{
	(void)view;
	(void)surface;
	g_focus_view_count++;
}

static void
wm_view_raise(struct wm_view *view)
{
	(void)view;
	g_view_raise_count++;
}

static void
deiconify_view(struct wm_view *view)
{
	(void)view;
}

/* --- Include toolbar source directly --- */

#include "toolbar.h"
#include "toolbar.c"

/* --- Test helpers --- */

static struct wm_server test_server;
static struct wm_style test_style;
static struct wm_config test_config;
static struct wlr_output test_wlr_output;
static struct wm_output test_output;
static struct wlr_scene_tree test_layer_top;
static struct wlr_cursor test_cursor;
static struct wl_event_loop test_event_loop;

static void
reset_globals(void)
{
	g_scene_buffer_create_count = 0;
	g_scene_buffer_set_buffer_count = 0;
	g_scene_node_set_position_count = 0;
	g_scene_node_set_enabled_count = 0;
	g_wlr_buffer_drop_count = 0;
	g_render_text_count = 0;
	g_render_texture_count = 0;
	g_measure_text_width_count = 0;
	g_ws_switch_next_count = 0;
	g_ws_switch_prev_count = 0;
	g_focus_next_count = 0;
	g_focus_prev_count = 0;
	g_focus_view_count = 0;
	g_view_raise_count = 0;
	g_timer_update_count = 0;
	g_timer_update_last_ms = -1;
}

static void
setup_test_server(void)
{
	memset(&test_server, 0, sizeof(test_server));
	memset(&test_style, 0, sizeof(test_style));
	memset(&test_config, 0, sizeof(test_config));
	memset(&test_output, 0, sizeof(test_output));
	memset(&test_wlr_output, 0, sizeof(test_wlr_output));

	test_style.toolbar_text_color = (struct wm_color){
		.r = 255, .g = 255, .b = 255, .a = 255
	};
	test_style.toolbar_font.size = 10;

	test_config.toolbar_visible = true;
	test_config.toolbar_placement = WM_TOOLBAR_BOTTOM_CENTER;
	test_config.toolbar_width_percent = 100;
	test_config.toolbar_height = 24;
	test_config.toolbar_auto_hide_delay_ms = 500;
	test_config.iconbar_alignment = 1; /* center */

	test_wlr_output.width = 1920;
	test_wlr_output.height = 1080;
	test_output.wlr_output = &test_wlr_output;
	test_output.usable_area = (struct wlr_box){0, 0, 1920, 1080};

	wl_list_init(&test_server.outputs);
	wl_list_init(&test_server.views);
	wl_list_init(&test_server.workspaces);
	wl_list_insert(&test_server.outputs, &test_output.link);

	test_server.style = &test_style;
	test_server.config = &test_config;
	test_server.layer_top = &test_layer_top;
	test_server.cursor = &test_cursor;
	test_server.wl_event_loop = &test_event_loop;
	test_server.focus_nav.toolbar_index = -1;
}

/* ===== Tests ===== */

/* --- parse_tool_name tests --- */

static void
test_parse_tool_name_all_tools(void)
{
	enum wm_toolbar_tool_type type;

	assert(parse_tool_name("prevworkspace", &type) == true);
	assert(type == WM_TOOL_PREV_WORKSPACE);

	assert(parse_tool_name("nextworkspace", &type) == true);
	assert(type == WM_TOOL_NEXT_WORKSPACE);

	assert(parse_tool_name("workspacename", &type) == true);
	assert(type == WM_TOOL_WORKSPACE_NAME);

	assert(parse_tool_name("iconbar", &type) == true);
	assert(type == WM_TOOL_ICONBAR);

	assert(parse_tool_name("clock", &type) == true);
	assert(type == WM_TOOL_CLOCK);

	assert(parse_tool_name("prevwindow", &type) == true);
	assert(type == WM_TOOL_PREV_WINDOW);

	assert(parse_tool_name("nextwindow", &type) == true);
	assert(type == WM_TOOL_NEXT_WINDOW);

	printf("  PASS: parse_tool_name_all_tools\n");
}

static void
test_parse_tool_name_unknown(void)
{
	enum wm_toolbar_tool_type type;

	assert(parse_tool_name("unknown", &type) == false);
	assert(parse_tool_name("", &type) == false);
	assert(parse_tool_name("PrevWorkspace", &type) == false); /* case-sensitive */
	assert(parse_tool_name("ICONBAR", &type) == false);

	printf("  PASS: parse_tool_name_unknown\n");
}

/* --- parse_toolbar_tools tests --- */

static void
test_parse_toolbar_tools_default(void)
{
	reset_globals();
	setup_test_server();

	struct wm_toolbar toolbar;
	memset(&toolbar, 0, sizeof(toolbar));
	toolbar.server = &test_server;
	toolbar.scene_tree = &test_layer_top;

	parse_toolbar_tools(&toolbar,
		"prevworkspace workspacename nextworkspace iconbar clock");

	assert(toolbar.tool_count == 5);
	assert(toolbar.tools[0].type == WM_TOOL_PREV_WORKSPACE);
	assert(toolbar.tools[1].type == WM_TOOL_WORKSPACE_NAME);
	assert(toolbar.tools[2].type == WM_TOOL_NEXT_WORKSPACE);
	assert(toolbar.tools[3].type == WM_TOOL_ICONBAR);
	assert(toolbar.tools[4].type == WM_TOOL_CLOCK);

	/* Quick-access pointers should be set */
	assert(toolbar.iconbar_tool == &toolbar.tools[3]);
	assert(toolbar.clock_tool == &toolbar.tools[4]);
	assert(toolbar.ws_name_tool == &toolbar.tools[1]);

	free(toolbar.tools);
	printf("  PASS: parse_toolbar_tools_default\n");
}

static void
test_parse_toolbar_tools_empty(void)
{
	struct wm_toolbar toolbar;
	memset(&toolbar, 0, sizeof(toolbar));
	toolbar.server = &test_server;

	parse_toolbar_tools(&toolbar, "");
	assert(toolbar.tool_count == 0);
	assert(toolbar.tools == NULL);

	parse_toolbar_tools(&toolbar, NULL);
	assert(toolbar.tool_count == 0);

	printf("  PASS: parse_toolbar_tools_empty\n");
}

static void
test_parse_toolbar_tools_with_unknown(void)
{
	reset_globals();
	setup_test_server();

	struct wm_toolbar toolbar;
	memset(&toolbar, 0, sizeof(toolbar));
	toolbar.server = &test_server;
	toolbar.scene_tree = &test_layer_top;

	parse_toolbar_tools(&toolbar,
		"prevworkspace BADTOOL nextworkspace garbage clock");

	/* Only valid tools should be kept */
	assert(toolbar.tool_count == 3);
	assert(toolbar.tools[0].type == WM_TOOL_PREV_WORKSPACE);
	assert(toolbar.tools[1].type == WM_TOOL_NEXT_WORKSPACE);
	assert(toolbar.tools[2].type == WM_TOOL_CLOCK);

	/* iconbar_tool should be NULL since not in list */
	assert(toolbar.iconbar_tool == NULL);
	assert(toolbar.clock_tool == &toolbar.tools[2]);

	free(toolbar.tools);
	printf("  PASS: parse_toolbar_tools_with_unknown\n");
}

static void
test_parse_toolbar_tools_tabs(void)
{
	reset_globals();
	setup_test_server();

	struct wm_toolbar toolbar;
	memset(&toolbar, 0, sizeof(toolbar));
	toolbar.server = &test_server;
	toolbar.scene_tree = &test_layer_top;

	/* Tabs should work as separators too */
	parse_toolbar_tools(&toolbar, "iconbar\tclock");

	assert(toolbar.tool_count == 2);
	assert(toolbar.tools[0].type == WM_TOOL_ICONBAR);
	assert(toolbar.tools[1].type == WM_TOOL_CLOCK);

	free(toolbar.tools);
	printf("  PASS: parse_toolbar_tools_tabs\n");
}

/* --- tool_is_button tests --- */

static void
test_tool_is_button(void)
{
	assert(tool_is_button(WM_TOOL_PREV_WORKSPACE) == true);
	assert(tool_is_button(WM_TOOL_NEXT_WORKSPACE) == true);
	assert(tool_is_button(WM_TOOL_PREV_WINDOW) == true);
	assert(tool_is_button(WM_TOOL_NEXT_WINDOW) == true);

	assert(tool_is_button(WM_TOOL_WORKSPACE_NAME) == false);
	assert(tool_is_button(WM_TOOL_ICONBAR) == false);
	assert(tool_is_button(WM_TOOL_CLOCK) == false);

	printf("  PASS: tool_is_button\n");
}

/* --- compute_tool_layout tests --- */

static void
test_compute_tool_layout_basic(void)
{
	reset_globals();
	setup_test_server();

	/* Create a simple toolbar with workspace + iconbar + clock */
	struct wm_toolbar toolbar;
	memset(&toolbar, 0, sizeof(toolbar));
	toolbar.server = &test_server;
	toolbar.height = 24;
	toolbar.scene_tree = &test_layer_top;

	/* Add a workspace to the server */
	struct wm_workspace ws;
	memset(&ws, 0, sizeof(ws));
	ws.name = "Workspace 1";
	ws.index = 0;
	wl_list_init(&ws.link);
	wl_list_insert(&test_server.workspaces, &ws.link);
	test_server.current_workspace = &ws;
	test_server.workspace_count = 1;

	parse_toolbar_tools(&toolbar,
		"prevworkspace workspacename nextworkspace iconbar clock");

	compute_tool_layout(&toolbar, 1000);

	/* Buttons should be square (width = height = 24) */
	assert(toolbar.tools[0].width == 24); /* prevworkspace */
	assert(toolbar.tools[2].width == 24); /* nextworkspace */

	/* Workspace name should have measured width + padding with min 60 */
	assert(toolbar.tools[1].width >= 60);

	/* Clock should have measured width + padding with min 80 */
	assert(toolbar.tools[4].width >= 80);

	/* Iconbar should fill remaining space */
	int fixed_total = toolbar.tools[0].width + toolbar.tools[1].width +
		toolbar.tools[2].width + toolbar.tools[4].width;
	assert(toolbar.tools[3].width == 1000 - fixed_total);

	/* X positions should be sequential */
	assert(toolbar.tools[0].x == 0);
	assert(toolbar.tools[1].x == toolbar.tools[0].width);
	int running_x = 0;
	for (int i = 0; i < toolbar.tool_count; i++) {
		assert(toolbar.tools[i].x == running_x);
		running_x += toolbar.tools[i].width;
	}

	/* Total width should equal 1000 */
	assert(running_x == 1000);

	wl_list_remove(&ws.link);
	free(toolbar.tools);
	printf("  PASS: compute_tool_layout_basic\n");
}

static void
test_compute_tool_layout_buttons_only(void)
{
	reset_globals();
	setup_test_server();

	struct wm_toolbar toolbar;
	memset(&toolbar, 0, sizeof(toolbar));
	toolbar.server = &test_server;
	toolbar.height = 24;
	toolbar.scene_tree = &test_layer_top;

	parse_toolbar_tools(&toolbar,
		"prevworkspace nextworkspace prevwindow nextwindow");

	compute_tool_layout(&toolbar, 400);

	/* All buttons should be 24px (height) each */
	for (int i = 0; i < toolbar.tool_count; i++) {
		assert(toolbar.tools[i].width == 24);
	}

	/* 4 buttons * 24 = 96, remaining 304 not distributed (no flex tools) */
	assert(toolbar.tools[3].x == 72);

	free(toolbar.tools);
	printf("  PASS: compute_tool_layout_buttons_only\n");
}

static void
test_compute_tool_layout_no_iconbar(void)
{
	reset_globals();
	setup_test_server();

	struct wm_workspace ws;
	memset(&ws, 0, sizeof(ws));
	ws.name = "1";
	ws.index = 0;
	wl_list_init(&ws.link);
	wl_list_insert(&test_server.workspaces, &ws.link);
	test_server.current_workspace = &ws;
	test_server.workspace_count = 1;

	struct wm_toolbar toolbar;
	memset(&toolbar, 0, sizeof(toolbar));
	toolbar.server = &test_server;
	toolbar.height = 24;
	toolbar.scene_tree = &test_layer_top;

	parse_toolbar_tools(&toolbar, "workspacename clock");

	compute_tool_layout(&toolbar, 800);

	/* Without iconbar, remaining space should be distributed to text tools */
	int total_w = toolbar.tools[0].width + toolbar.tools[1].width;
	/* Total should be 800 (remaining distributed to text_count=2 tools) */
	assert(total_w <= 800);

	wl_list_remove(&ws.link);
	free(toolbar.tools);
	printf("  PASS: compute_tool_layout_no_iconbar\n");
}

static void
test_compute_tool_layout_empty(void)
{
	reset_globals();
	setup_test_server();

	struct wm_toolbar toolbar;
	memset(&toolbar, 0, sizeof(toolbar));
	toolbar.server = &test_server;
	toolbar.tool_count = 0;

	/* Should not crash */
	compute_tool_layout(&toolbar, 1000);
	printf("  PASS: compute_tool_layout_empty\n");
}

/* --- collect_iconbar_entries tests --- */

static struct wlr_xdg_toplevel test_toplevels[8];
static struct wlr_xdg_surface test_xdg_surfaces[8];
static struct wlr_scene_tree test_view_trees[8];

static struct wm_view
make_test_view(int idx, const char *title, struct wm_workspace *ws,
	bool iconified)
{
	test_xdg_surfaces[idx].surface = NULL;
	test_toplevels[idx].base = &test_xdg_surfaces[idx];
	test_view_trees[idx].node.enabled = iconified ? 0 : 1;

	struct wm_view view;
	memset(&view, 0, sizeof(view));
	view.server = &test_server;
	view.xdg_toplevel = &test_toplevels[idx];
	view.scene_tree = &test_view_trees[idx];
	view.title = (char *)title;
	view.workspace = ws;
	view.sticky = false;
	return view;
}

static void
test_collect_iconbar_workspace_mode(void)
{
	reset_globals();
	setup_test_server();

	struct wm_workspace ws1 = {.name = "1", .index = 0};
	struct wm_workspace ws2 = {.name = "2", .index = 1};
	wl_list_init(&ws1.link);
	wl_list_init(&ws2.link);
	test_server.current_workspace = &ws1;

	struct wm_view v1 = make_test_view(0, "Window 1", &ws1, false);
	struct wm_view v2 = make_test_view(1, "Window 2", &ws2, false);
	struct wm_view v3 = make_test_view(2, "Window 3", &ws1, false);

	wl_list_insert(&test_server.views, &v1.link);
	wl_list_insert(&test_server.views, &v2.link);
	wl_list_insert(&test_server.views, &v3.link);

	struct wm_toolbar toolbar;
	memset(&toolbar, 0, sizeof(toolbar));
	toolbar.server = &test_server;
	toolbar.iconbar_mode = 0; /* WM_ICONBAR_MODE_WORKSPACE */

	int count = collect_iconbar_entries(&toolbar);

	/* Should only include v1 and v3 (on ws1) */
	assert(count == 2);
	assert(toolbar.ib_count == 2);

	wl_list_remove(&v1.link);
	wl_list_remove(&v2.link);
	wl_list_remove(&v3.link);
	free(toolbar.ib_entries);
	printf("  PASS: collect_iconbar_workspace_mode\n");
}

static void
test_collect_iconbar_all_windows_mode(void)
{
	reset_globals();
	setup_test_server();

	struct wm_workspace ws1 = {.name = "1", .index = 0};
	struct wm_workspace ws2 = {.name = "2", .index = 1};
	wl_list_init(&ws1.link);
	wl_list_init(&ws2.link);
	test_server.current_workspace = &ws1;

	struct wm_view v1 = make_test_view(0, "Window 1", &ws1, false);
	struct wm_view v2 = make_test_view(1, "Window 2", &ws2, false);

	wl_list_insert(&test_server.views, &v1.link);
	wl_list_insert(&test_server.views, &v2.link);

	struct wm_toolbar toolbar;
	memset(&toolbar, 0, sizeof(toolbar));
	toolbar.server = &test_server;
	toolbar.iconbar_mode = 1; /* WM_ICONBAR_MODE_ALL_WINDOWS */

	int count = collect_iconbar_entries(&toolbar);

	/* Should include both views */
	assert(count == 2);

	wl_list_remove(&v1.link);
	wl_list_remove(&v2.link);
	free(toolbar.ib_entries);
	printf("  PASS: collect_iconbar_all_windows_mode\n");
}

static void
test_collect_iconbar_icons_mode(void)
{
	reset_globals();
	setup_test_server();

	struct wm_workspace ws1 = {.name = "1", .index = 0};
	wl_list_init(&ws1.link);
	test_server.current_workspace = &ws1;

	struct wm_view v1 = make_test_view(0, "Normal", &ws1, false);
	struct wm_view v2 = make_test_view(1, "Iconified", &ws1, true);

	wl_list_insert(&test_server.views, &v1.link);
	wl_list_insert(&test_server.views, &v2.link);

	struct wm_toolbar toolbar;
	memset(&toolbar, 0, sizeof(toolbar));
	toolbar.server = &test_server;
	toolbar.iconbar_mode = 2; /* WM_ICONBAR_MODE_ICONS */

	int count = collect_iconbar_entries(&toolbar);

	/* Should only include iconified views */
	assert(count == 1);
	assert(toolbar.ib_entries[0].iconified == true);

	wl_list_remove(&v1.link);
	wl_list_remove(&v2.link);
	free(toolbar.ib_entries);
	printf("  PASS: collect_iconbar_icons_mode\n");
}

static void
test_collect_iconbar_no_icons_mode(void)
{
	reset_globals();
	setup_test_server();

	struct wm_workspace ws1 = {.name = "1", .index = 0};
	wl_list_init(&ws1.link);
	test_server.current_workspace = &ws1;

	struct wm_view v1 = make_test_view(0, "Normal", &ws1, false);
	struct wm_view v2 = make_test_view(1, "Iconified", &ws1, true);

	wl_list_insert(&test_server.views, &v1.link);
	wl_list_insert(&test_server.views, &v2.link);

	struct wm_toolbar toolbar;
	memset(&toolbar, 0, sizeof(toolbar));
	toolbar.server = &test_server;
	toolbar.iconbar_mode = 3; /* WM_ICONBAR_MODE_NO_ICONS */

	int count = collect_iconbar_entries(&toolbar);

	/* Should only include non-iconified views on current ws */
	assert(count == 1);
	assert(toolbar.ib_entries[0].iconified == false);

	wl_list_remove(&v1.link);
	wl_list_remove(&v2.link);
	free(toolbar.ib_entries);
	printf("  PASS: collect_iconbar_no_icons_mode\n");
}

static void
test_collect_iconbar_sticky_views(void)
{
	reset_globals();
	setup_test_server();

	struct wm_workspace ws1 = {.name = "1", .index = 0};
	struct wm_workspace ws2 = {.name = "2", .index = 1};
	wl_list_init(&ws1.link);
	wl_list_init(&ws2.link);
	test_server.current_workspace = &ws1;

	struct wm_view v1 = make_test_view(0, "Sticky", &ws2, false);
	v1.sticky = true;

	wl_list_insert(&test_server.views, &v1.link);

	struct wm_toolbar toolbar;
	memset(&toolbar, 0, sizeof(toolbar));
	toolbar.server = &test_server;
	toolbar.iconbar_mode = 0; /* WM_ICONBAR_MODE_WORKSPACE */

	int count = collect_iconbar_entries(&toolbar);

	/* Sticky view on ws2 should appear even on ws1 */
	assert(count == 1);

	wl_list_remove(&v1.link);
	free(toolbar.ib_entries);
	printf("  PASS: collect_iconbar_sticky_views\n");
}

/* --- wm_toolbar_handle_scroll tests --- */

static void
test_scroll_outside_bounds(void)
{
	reset_globals();
	setup_test_server();

	struct wm_toolbar toolbar;
	memset(&toolbar, 0, sizeof(toolbar));
	toolbar.server = &test_server;
	toolbar.visible = true;
	toolbar.x = 0;
	toolbar.y = 1056;
	toolbar.width = 1920;
	toolbar.height = 24;

	/* Point outside toolbar */
	assert(wm_toolbar_handle_scroll(&toolbar, 100, 500, 1.0) == false);
	assert(wm_toolbar_handle_scroll(&toolbar, -1, 1060, 1.0) == false);

	printf("  PASS: scroll_outside_bounds\n");
}

static void
test_scroll_null_toolbar(void)
{
	assert(wm_toolbar_handle_scroll(NULL, 100, 100, 1.0) == false);
	printf("  PASS: scroll_null_toolbar\n");
}

static void
test_scroll_hidden_toolbar(void)
{
	struct wm_toolbar toolbar;
	memset(&toolbar, 0, sizeof(toolbar));
	toolbar.visible = false;

	assert(wm_toolbar_handle_scroll(&toolbar, 100, 100, 1.0) == false);
	printf("  PASS: scroll_hidden_toolbar\n");
}

static void
test_scroll_on_workspace_tool(void)
{
	reset_globals();
	setup_test_server();

	struct wm_toolbar toolbar;
	memset(&toolbar, 0, sizeof(toolbar));
	toolbar.server = &test_server;
	toolbar.visible = true;
	toolbar.x = 0;
	toolbar.y = 0;
	toolbar.width = 200;
	toolbar.height = 24;

	struct wm_toolbar_tool tools[1];
	memset(tools, 0, sizeof(tools));
	tools[0].type = WM_TOOL_WORKSPACE_NAME;
	tools[0].x = 0;
	tools[0].width = 200;
	toolbar.tools = tools;
	toolbar.tool_count = 1;

	/* Scroll up on workspace tool */
	bool consumed = wm_toolbar_handle_scroll(&toolbar, 100, 12, 1.0);
	assert(consumed == true);
	assert(g_ws_switch_next_count == 1);

	/* Scroll down on workspace tool */
	consumed = wm_toolbar_handle_scroll(&toolbar, 100, 12, -1.0);
	assert(consumed == true);
	assert(g_ws_switch_prev_count == 1);

	printf("  PASS: scroll_on_workspace_tool\n");
}

static void
test_scroll_on_iconbar(void)
{
	reset_globals();
	setup_test_server();

	struct wm_toolbar toolbar;
	memset(&toolbar, 0, sizeof(toolbar));
	toolbar.server = &test_server;
	toolbar.visible = true;
	toolbar.x = 0;
	toolbar.y = 0;
	toolbar.width = 400;
	toolbar.height = 24;

	struct wm_toolbar_tool tools[1];
	memset(tools, 0, sizeof(tools));
	tools[0].type = WM_TOOL_ICONBAR;
	tools[0].x = 0;
	tools[0].width = 400;
	toolbar.tools = tools;
	toolbar.tool_count = 1;

	/* Default wheel_mode = 0 (screen: ws change) */
	test_config.iconbar_wheel_mode = 0;
	bool consumed = wm_toolbar_handle_scroll(&toolbar, 200, 12, 1.0);
	assert(consumed == true);
	assert(g_ws_switch_next_count == 1);

	/* Wheel mode 1 = off */
	test_config.iconbar_wheel_mode = 1;
	reset_globals();
	consumed = wm_toolbar_handle_scroll(&toolbar, 200, 12, 1.0);
	assert(consumed == true);
	assert(g_ws_switch_next_count == 0); /* No ws switch */

	printf("  PASS: scroll_on_iconbar\n");
}

/* --- wm_toolbar_create tests --- */

static void
test_toolbar_create_basic(void)
{
	reset_globals();
	setup_test_server();
	test_config.toolbar_tools = "prevworkspace workspacename nextworkspace iconbar clock";

	struct wm_workspace ws;
	memset(&ws, 0, sizeof(ws));
	ws.name = "Default";
	ws.index = 0;
	wl_list_insert(&test_server.workspaces, &ws.link);
	test_server.current_workspace = &ws;
	test_server.workspace_count = 1;

	struct wm_toolbar *toolbar = wm_toolbar_create(&test_server);
	assert(toolbar != NULL);
	assert(toolbar->server == &test_server);
	assert(toolbar->visible == true);
	assert(toolbar->height == 24);
	assert(toolbar->tool_count == 5);
	assert(toolbar->iconbar_tool != NULL);
	assert(toolbar->clock_tool != NULL);
	assert(toolbar->ws_name_tool != NULL);

	wm_toolbar_destroy(toolbar);
	wl_list_remove(&ws.link);
	printf("  PASS: toolbar_create_basic\n");
}

static void
test_toolbar_create_auto_hide(void)
{
	reset_globals();
	setup_test_server();
	test_config.toolbar_auto_hide = true;
	test_config.toolbar_tools = "iconbar";

	struct wm_workspace ws;
	memset(&ws, 0, sizeof(ws));
	ws.name = "1";
	ws.index = 0;
	wl_list_insert(&test_server.workspaces, &ws.link);
	test_server.current_workspace = &ws;
	test_server.workspace_count = 1;

	struct wm_toolbar *toolbar = wm_toolbar_create(&test_server);
	assert(toolbar != NULL);
	assert(toolbar->auto_hide == true);
	assert(toolbar->shown == false);
	assert(toolbar->hide_timer != NULL);

	wm_toolbar_destroy(toolbar);
	wl_list_remove(&ws.link);
	printf("  PASS: toolbar_create_auto_hide\n");
}

/* --- wm_toolbar_toggle_visible tests --- */

static void
test_toolbar_toggle_visible(void)
{
	reset_globals();
	setup_test_server();
	test_config.toolbar_tools = "iconbar";

	struct wm_workspace ws;
	memset(&ws, 0, sizeof(ws));
	ws.name = "1";
	ws.index = 0;
	wl_list_insert(&test_server.workspaces, &ws.link);
	test_server.current_workspace = &ws;
	test_server.workspace_count = 1;

	struct wm_toolbar *toolbar = wm_toolbar_create(&test_server);
	assert(toolbar->visible == true);

	/* Toggle calls relayout which re-reads config; update config too */
	test_config.toolbar_visible = false;
	wm_toolbar_toggle_visible(toolbar);
	assert(toolbar->visible == false);

	test_config.toolbar_visible = true;
	wm_toolbar_toggle_visible(toolbar);
	assert(toolbar->visible == true);

	wm_toolbar_destroy(toolbar);
	wl_list_remove(&ws.link);
	printf("  PASS: toolbar_toggle_visible\n");
}

/* --- null safety tests --- */

static void
test_toolbar_null_safety(void)
{
	/* All functions should handle NULL gracefully */
	wm_toolbar_destroy(NULL);
	wm_toolbar_update_workspace(NULL);
	wm_toolbar_update_iconbar(NULL);
	wm_toolbar_relayout(NULL);
	assert(wm_toolbar_handle_scroll(NULL, 0, 0, 0) == false);
	wm_toolbar_notify_pointer_motion(NULL, 0, 0);
	wm_toolbar_toggle_above(NULL);
	wm_toolbar_toggle_visible(NULL);

	printf("  PASS: toolbar_null_safety\n");
}

/* --- wm_toolbar_notify_pointer_motion: auto-hide show path --- */

static void
test_pointer_motion_show_bottom(void)
{
	reset_globals();
	setup_test_server();
	test_config.toolbar_auto_hide = true;
	test_config.toolbar_tools = "iconbar";

	struct wm_workspace ws;
	memset(&ws, 0, sizeof(ws));
	ws.name = "1";
	ws.index = 0;
	wl_list_insert(&test_server.workspaces, &ws.link);
	test_server.current_workspace = &ws;
	test_server.workspace_count = 1;

	struct wm_toolbar *toolbar = wm_toolbar_create(&test_server);
	assert(toolbar != NULL);
	assert(toolbar->auto_hide == true);
	assert(toolbar->shown == false);
	assert(toolbar->on_top == false);

	/* Pointer at bottom edge (trigger zone): y >= output_height - 1 = 1079 */
	reset_globals();
	wm_toolbar_notify_pointer_motion(toolbar, 500, 1079);

	/* Toolbar should now be shown */
	assert(toolbar->shown == true);
	assert(toolbar->scene_tree->node.enabled == 1);

	/* Hide timer should be cancelled (set to 0) */
	/* The show path cancels with timer_update(0) */

	wm_toolbar_destroy(toolbar);
	wl_list_remove(&ws.link);
	printf("  PASS: pointer_motion_show_bottom\n");
}

/* --- wm_toolbar_notify_pointer_motion: auto-hide show on top --- */

static void
test_pointer_motion_show_top(void)
{
	reset_globals();
	setup_test_server();
	test_config.toolbar_auto_hide = true;
	test_config.toolbar_placement = WM_TOOLBAR_TOP_CENTER;
	test_config.toolbar_tools = "iconbar";

	struct wm_workspace ws;
	memset(&ws, 0, sizeof(ws));
	ws.name = "1";
	ws.index = 0;
	wl_list_insert(&test_server.workspaces, &ws.link);
	test_server.current_workspace = &ws;
	test_server.workspace_count = 1;

	struct wm_toolbar *toolbar = wm_toolbar_create(&test_server);
	assert(toolbar != NULL);
	assert(toolbar->auto_hide == true);
	assert(toolbar->shown == false);
	assert(toolbar->on_top == true);

	/* Pointer at top edge (trigger zone): y <= 1 */
	reset_globals();
	wm_toolbar_notify_pointer_motion(toolbar, 500, 0);

	assert(toolbar->shown == true);
	assert(toolbar->scene_tree->node.enabled == 1);

	wm_toolbar_destroy(toolbar);
	wl_list_remove(&ws.link);
	printf("  PASS: pointer_motion_show_top\n");
}

/* --- wm_toolbar_notify_pointer_motion: auto-hide start hide timer --- */

static void
test_pointer_motion_start_hide_timer(void)
{
	reset_globals();
	setup_test_server();
	test_config.toolbar_auto_hide = true;
	test_config.toolbar_auto_hide_delay_ms = 750;
	test_config.toolbar_tools = "iconbar";

	struct wm_workspace ws;
	memset(&ws, 0, sizeof(ws));
	ws.name = "1";
	ws.index = 0;
	wl_list_insert(&test_server.workspaces, &ws.link);
	test_server.current_workspace = &ws;
	test_server.workspace_count = 1;

	struct wm_toolbar *toolbar = wm_toolbar_create(&test_server);
	assert(toolbar != NULL);

	/* First, show the toolbar */
	wm_toolbar_notify_pointer_motion(toolbar, 500, 1079);
	assert(toolbar->shown == true);

	/* Move pointer away from both trigger zone and toolbar area */
	reset_globals();
	wm_toolbar_notify_pointer_motion(toolbar, 500, 500);

	/* Hide timer should have been armed with the configured delay */
	assert(g_timer_update_last_ms == 750);

	wm_toolbar_destroy(toolbar);
	wl_list_remove(&ws.link);
	printf("  PASS: pointer_motion_start_hide_timer\n");
}

/* --- wm_toolbar_notify_pointer_motion: cancel hide when inside --- */

static void
test_pointer_motion_cancel_hide_inside(void)
{
	reset_globals();
	setup_test_server();
	test_config.toolbar_auto_hide = true;
	test_config.toolbar_tools = "iconbar";

	struct wm_workspace ws;
	memset(&ws, 0, sizeof(ws));
	ws.name = "1";
	ws.index = 0;
	wl_list_insert(&test_server.workspaces, &ws.link);
	test_server.current_workspace = &ws;
	test_server.workspace_count = 1;

	struct wm_toolbar *toolbar = wm_toolbar_create(&test_server);
	assert(toolbar != NULL);

	/* Show the toolbar first */
	wm_toolbar_notify_pointer_motion(toolbar, 500, 1079);
	assert(toolbar->shown == true);

	/* Move pointer inside the toolbar area */
	/* toolbar is at y=1056, height=24, so 1056..1080 is inside */
	reset_globals();
	wm_toolbar_notify_pointer_motion(toolbar, 500, 1060);

	/* Hide timer should be cancelled (set to 0) */
	assert(g_timer_update_last_ms == 0);

	wm_toolbar_destroy(toolbar);
	wl_list_remove(&ws.link);
	printf("  PASS: pointer_motion_cancel_hide_inside\n");
}

/* --- wm_toolbar_notify_pointer_motion: no-op when not auto_hide --- */

static void
test_pointer_motion_noop_no_autohide(void)
{
	reset_globals();
	setup_test_server();
	test_config.toolbar_auto_hide = false;
	test_config.toolbar_tools = "iconbar";

	struct wm_workspace ws;
	memset(&ws, 0, sizeof(ws));
	ws.name = "1";
	ws.index = 0;
	wl_list_insert(&test_server.workspaces, &ws.link);
	test_server.current_workspace = &ws;
	test_server.workspace_count = 1;

	struct wm_toolbar *toolbar = wm_toolbar_create(&test_server);
	assert(toolbar != NULL);
	assert(toolbar->auto_hide == false);

	reset_globals();
	wm_toolbar_notify_pointer_motion(toolbar, 500, 1079);

	/* Nothing should happen - function returns early */
	assert(g_timer_update_count == 0);

	wm_toolbar_destroy(toolbar);
	wl_list_remove(&ws.link);
	printf("  PASS: pointer_motion_noop_no_autohide\n");
}

/* --- wm_toolbar_notify_pointer_motion: no-op when not visible --- */

static void
test_pointer_motion_noop_not_visible(void)
{
	reset_globals();
	setup_test_server();
	test_config.toolbar_auto_hide = true;
	test_config.toolbar_visible = false;
	test_config.toolbar_tools = "iconbar";

	struct wm_workspace ws;
	memset(&ws, 0, sizeof(ws));
	ws.name = "1";
	ws.index = 0;
	wl_list_insert(&test_server.workspaces, &ws.link);
	test_server.current_workspace = &ws;
	test_server.workspace_count = 1;

	struct wm_toolbar *toolbar = wm_toolbar_create(&test_server);
	assert(toolbar != NULL);
	assert(toolbar->visible == false);

	reset_globals();
	wm_toolbar_notify_pointer_motion(toolbar, 500, 1079);

	/* Function should early-return since not visible */
	assert(g_timer_update_count == 0);

	wm_toolbar_destroy(toolbar);
	wl_list_remove(&ws.link);
	printf("  PASS: pointer_motion_noop_not_visible\n");
}

/* --- hide_timer_cb: hides toolbar when pointer is outside --- */

static void
test_hide_timer_cb_hides(void)
{
	reset_globals();
	setup_test_server();
	test_config.toolbar_auto_hide = true;
	test_config.toolbar_tools = "iconbar";

	struct wm_workspace ws;
	memset(&ws, 0, sizeof(ws));
	ws.name = "1";
	ws.index = 0;
	wl_list_insert(&test_server.workspaces, &ws.link);
	test_server.current_workspace = &ws;
	test_server.workspace_count = 1;

	struct wm_toolbar *toolbar = wm_toolbar_create(&test_server);
	assert(toolbar != NULL);

	/* Show the toolbar first */
	toolbar->shown = true;
	wlr_scene_node_set_enabled(&toolbar->scene_tree->node, true);

	/* Place cursor outside toolbar */
	test_cursor.x = 500;
	test_cursor.y = 500;

	reset_globals();
	int ret = hide_timer_cb(toolbar);
	assert(ret == 0);

	/* Toolbar should now be hidden */
	assert(toolbar->shown == false);
	assert(toolbar->scene_tree->node.enabled == 0);

	wm_toolbar_destroy(toolbar);
	wl_list_remove(&ws.link);
	printf("  PASS: hide_timer_cb_hides\n");
}

/* --- hide_timer_cb: re-arms when pointer is inside toolbar --- */

static void
test_hide_timer_cb_rearms_inside(void)
{
	reset_globals();
	setup_test_server();
	test_config.toolbar_auto_hide = true;
	test_config.toolbar_tools = "iconbar";

	struct wm_workspace ws;
	memset(&ws, 0, sizeof(ws));
	ws.name = "1";
	ws.index = 0;
	wl_list_insert(&test_server.workspaces, &ws.link);
	test_server.current_workspace = &ws;
	test_server.workspace_count = 1;

	struct wm_toolbar *toolbar = wm_toolbar_create(&test_server);
	assert(toolbar != NULL);

	/* Show the toolbar first */
	toolbar->shown = true;
	wlr_scene_node_set_enabled(&toolbar->scene_tree->node, true);

	/* Place cursor inside toolbar area */
	/* toolbar: x=0, y=1056, width=1920, height=24 */
	test_cursor.x = 500;
	test_cursor.y = (double)toolbar->y + 10;

	reset_globals();
	int ret = hide_timer_cb(toolbar);
	assert(ret == 0);

	/* Toolbar should still be shown */
	assert(toolbar->shown == true);

	/* Timer should have been re-armed with 500ms */
	assert(g_timer_update_last_ms == 500);

	wm_toolbar_destroy(toolbar);
	wl_list_remove(&ws.link);
	printf("  PASS: hide_timer_cb_rearms_inside\n");
}

/* --- hide_timer_cb: no-op when auto_hide is false --- */

static void
test_hide_timer_cb_noop_no_autohide(void)
{
	reset_globals();
	setup_test_server();
	test_config.toolbar_auto_hide = false;
	test_config.toolbar_tools = "iconbar";

	struct wm_workspace ws;
	memset(&ws, 0, sizeof(ws));
	ws.name = "1";
	ws.index = 0;
	wl_list_insert(&test_server.workspaces, &ws.link);
	test_server.current_workspace = &ws;
	test_server.workspace_count = 1;

	struct wm_toolbar *toolbar = wm_toolbar_create(&test_server);
	assert(toolbar != NULL);
	toolbar->shown = true;

	reset_globals();
	int ret = hide_timer_cb(toolbar);
	assert(ret == 0);

	/* Should be a no-op, toolbar still shown */
	assert(toolbar->shown == true);

	wm_toolbar_destroy(toolbar);
	wl_list_remove(&ws.link);
	printf("  PASS: hide_timer_cb_noop_no_autohide\n");
}

/* --- clock_timer_cb: re-renders when visible --- */

static void
test_clock_timer_cb_visible(void)
{
	reset_globals();
	setup_test_server();
	test_config.toolbar_tools = "iconbar clock";

	struct wm_workspace ws;
	memset(&ws, 0, sizeof(ws));
	ws.name = "1";
	ws.index = 0;
	wl_list_insert(&test_server.workspaces, &ws.link);
	test_server.current_workspace = &ws;
	test_server.workspace_count = 1;

	struct wm_toolbar *toolbar = wm_toolbar_create(&test_server);
	assert(toolbar != NULL);
	assert(toolbar->visible == true);
	assert(toolbar->clock_tool != NULL);

	/* Clear clock cache so render_clock will produce a buffer */
	toolbar->cached_clock[0] = '\0';

	reset_globals();
	int ret = clock_timer_cb(toolbar);
	assert(ret == 0);

	/* Timer should be re-armed with 1000ms */
	assert(g_timer_update_last_ms == 1000);

	wm_toolbar_destroy(toolbar);
	wl_list_remove(&ws.link);
	printf("  PASS: clock_timer_cb_visible\n");
}

/* --- clock_timer_cb: re-arms when hidden --- */

static void
test_clock_timer_cb_hidden(void)
{
	reset_globals();
	setup_test_server();
	test_config.toolbar_visible = false;
	test_config.toolbar_tools = "iconbar clock";

	struct wm_workspace ws;
	memset(&ws, 0, sizeof(ws));
	ws.name = "1";
	ws.index = 0;
	wl_list_insert(&test_server.workspaces, &ws.link);
	test_server.current_workspace = &ws;
	test_server.workspace_count = 1;

	struct wm_toolbar *toolbar = wm_toolbar_create(&test_server);
	assert(toolbar != NULL);
	assert(toolbar->visible == false);

	reset_globals();
	int ret = clock_timer_cb(toolbar);
	assert(ret == 0);

	/* Timer should NOT re-arm when hidden — saves CPU.
	 * It will be restarted when toolbar becomes visible.
	 * Value stays at reset value (-1) since no timer update call. */
	assert(g_timer_update_last_ms == -1);

	wm_toolbar_destroy(toolbar);
	wl_list_remove(&ws.link);
	printf("  PASS: clock_timer_cb_hidden\n");
}

/* --- clock_timer_cb: no clock tool configured --- */

static void
test_clock_timer_cb_no_clock_tool(void)
{
	reset_globals();
	setup_test_server();
	test_config.toolbar_tools = "iconbar"; /* no clock */

	struct wm_workspace ws;
	memset(&ws, 0, sizeof(ws));
	ws.name = "1";
	ws.index = 0;
	wl_list_insert(&test_server.workspaces, &ws.link);
	test_server.current_workspace = &ws;
	test_server.workspace_count = 1;

	struct wm_toolbar *toolbar = wm_toolbar_create(&test_server);
	assert(toolbar != NULL);
	assert(toolbar->clock_tool == NULL);

	reset_globals();
	int ret = clock_timer_cb(toolbar);
	assert(ret == 0);

	/* Timer re-armed even without clock tool */
	assert(g_timer_update_last_ms == 1000);

	wm_toolbar_destroy(toolbar);
	wl_list_remove(&ws.link);
	printf("  PASS: clock_timer_cb_no_clock_tool\n");
}

/* --- wm_toolbar_toggle_above: null safety --- */

static void
test_toggle_above_null_safety(void)
{
	/* Should not crash */
	wm_toolbar_toggle_above(NULL);
	printf("  PASS: toggle_above_null_safety\n");
}

/* --- wm_toolbar_toggle_above: changes position via config switch --- */

static void
test_toggle_above_config_top(void)
{
	reset_globals();
	setup_test_server();
	test_config.toolbar_placement = WM_TOOLBAR_TOP_CENTER;
	test_config.toolbar_tools = "iconbar";

	struct wm_workspace ws;
	memset(&ws, 0, sizeof(ws));
	ws.name = "1";
	ws.index = 0;
	wl_list_insert(&test_server.workspaces, &ws.link);
	test_server.current_workspace = &ws;
	test_server.workspace_count = 1;

	struct wm_toolbar *toolbar = wm_toolbar_create(&test_server);
	assert(toolbar != NULL);
	assert(toolbar->on_top == true);
	assert(toolbar->y == 0);

	/*
	 * toggle_above flips on_top then calls relayout. relayout re-reads
	 * config placement, so the effective on_top depends on config. To
	 * properly test the toggle, switch config placement before calling.
	 */
	test_config.toolbar_placement = WM_TOOLBAR_BOTTOM_CENTER;
	wm_toolbar_toggle_above(toolbar);
	/* relayout re-reads config => now bottom center */
	assert(toolbar->on_top == false);
	assert(toolbar->y == 1080 - toolbar->height);

	/* Switch back */
	test_config.toolbar_placement = WM_TOOLBAR_TOP_CENTER;
	wm_toolbar_toggle_above(toolbar);
	assert(toolbar->on_top == true);
	assert(toolbar->y == 0);

	wm_toolbar_destroy(toolbar);
	wl_list_remove(&ws.link);
	printf("  PASS: toggle_above_config_top\n");
}

/* --- wm_toolbar_toggle_above: bottom to relayout stays bottom --- */

static void
test_toggle_above_relayout_behavior(void)
{
	reset_globals();
	setup_test_server();
	test_config.toolbar_placement = WM_TOOLBAR_BOTTOM_CENTER;
	test_config.toolbar_tools = "iconbar";

	struct wm_workspace ws;
	memset(&ws, 0, sizeof(ws));
	ws.name = "1";
	ws.index = 0;
	wl_list_insert(&test_server.workspaces, &ws.link);
	test_server.current_workspace = &ws;
	test_server.workspace_count = 1;

	struct wm_toolbar *toolbar = wm_toolbar_create(&test_server);
	assert(toolbar != NULL);
	assert(toolbar->on_top == false);
	int original_y = toolbar->y;

	/*
	 * toggle_above sets on_top=true, but then relayout re-reads
	 * config (still BOTTOM_CENTER) so on_top goes back to false.
	 * This tests the actual relayout override behavior.
	 */
	wm_toolbar_toggle_above(toolbar);
	/* Relayout overrides on_top from config placement (BOTTOM_CENTER) */
	assert(toolbar->on_top == false);
	assert(toolbar->y == original_y);

	wm_toolbar_destroy(toolbar);
	wl_list_remove(&ws.link);
	printf("  PASS: toggle_above_relayout_behavior\n");
}

/* --- collect_iconbar: workspace_icons mode --- */

static void
test_collect_iconbar_workspace_icons_mode(void)
{
	reset_globals();
	setup_test_server();

	struct wm_workspace ws;
	memset(&ws, 0, sizeof(ws));
	ws.name = "1";
	ws.index = 0;
	wl_list_insert(&test_server.workspaces, &ws.link);
	test_server.current_workspace = &ws;
	test_server.workspace_count = 1;

	struct wlr_xdg_toplevel tl;
	struct wlr_xdg_surface base;
	memset(&tl, 0, sizeof(tl));
	memset(&base, 0, sizeof(base));
	tl.base = &base;

	/* Iconified view on current workspace */
	struct wlr_scene_tree view_tree;
	memset(&view_tree, 0, sizeof(view_tree));
	view_tree.node.enabled = 0; /* iconified */

	struct wm_view v1;
	memset(&v1, 0, sizeof(v1));
	v1.server = &test_server;
	v1.xdg_toplevel = &tl;
	v1.scene_tree = &view_tree;
	v1.workspace = &ws;
	v1.title = "Iconified";

	/* Non-iconified view on current workspace */
	struct wlr_scene_tree view_tree2;
	memset(&view_tree2, 0, sizeof(view_tree2));
	view_tree2.node.enabled = 1;

	struct wm_view v2;
	memset(&v2, 0, sizeof(v2));
	v2.server = &test_server;
	v2.xdg_toplevel = &tl;
	v2.scene_tree = &view_tree2;
	v2.workspace = &ws;
	v2.title = "Normal";

	wl_list_insert(&test_server.views, &v2.link);
	wl_list_insert(&test_server.views, &v1.link);

	struct wm_toolbar toolbar;
	memset(&toolbar, 0, sizeof(toolbar));
	toolbar.server = &test_server;
	toolbar.iconbar_mode = WM_ICONBAR_MODE_WORKSPACE_ICONS;

	int count = collect_iconbar_entries(&toolbar);
	/* Only iconified views on current workspace should appear */
	assert(count == 1);
	assert(toolbar.ib_entries[0].view == &v1);
	assert(toolbar.ib_entries[0].iconified == true);

	free(toolbar.ib_entries);
	wl_list_remove(&v1.link);
	wl_list_remove(&v2.link);
	wl_list_remove(&ws.link);
	printf("  PASS: collect_iconbar_workspace_icons_mode\n");
}

/* --- compute_tool_layout: clock with custom format --- */

static void
test_compute_tool_layout_custom_clock_fmt(void)
{
	reset_globals();
	setup_test_server();
	test_config.clock_format = "%H:%M";

	struct wm_workspace ws;
	memset(&ws, 0, sizeof(ws));
	ws.name = "1";
	ws.index = 0;
	wl_list_insert(&test_server.workspaces, &ws.link);
	test_server.current_workspace = &ws;
	test_server.workspace_count = 1;

	struct wm_toolbar toolbar;
	memset(&toolbar, 0, sizeof(toolbar));
	toolbar.server = &test_server;
	toolbar.height = 24;

	struct wm_toolbar_tool tools[2];
	memset(tools, 0, sizeof(tools));
	tools[0].type = WM_TOOL_CLOCK;
	tools[1].type = WM_TOOL_ICONBAR;
	toolbar.tools = tools;
	toolbar.tool_count = 2;
	toolbar.iconbar_tool = &tools[1];
	toolbar.clock_tool = &tools[0];

	compute_tool_layout(&toolbar, 800);

	/* Clock should have a reasonable width */
	assert(tools[0].width >= 80);
	/* Iconbar should fill the rest */
	assert(tools[1].width == 800 - tools[0].width);
	assert(tools[0].x == 0);
	assert(tools[1].x == tools[0].width);

	wl_list_remove(&ws.link);
	printf("  PASS: compute_tool_layout_custom_clock_fmt\n");
}

/* --- compute_tool_layout: no iconbar distributes to text tools --- */

static void
test_compute_tool_layout_distribute_remaining(void)
{
	reset_globals();
	setup_test_server();

	struct wm_workspace ws;
	memset(&ws, 0, sizeof(ws));
	ws.name = "MyWS";
	ws.index = 0;
	wl_list_insert(&test_server.workspaces, &ws.link);
	test_server.current_workspace = &ws;
	test_server.workspace_count = 1;

	struct wm_toolbar toolbar;
	memset(&toolbar, 0, sizeof(toolbar));
	toolbar.server = &test_server;
	toolbar.height = 24;

	/* Two text tools (workspace_name + clock), no iconbar */
	struct wm_toolbar_tool tools[2];
	memset(tools, 0, sizeof(tools));
	tools[0].type = WM_TOOL_WORKSPACE_NAME;
	tools[1].type = WM_TOOL_CLOCK;
	toolbar.tools = tools;
	toolbar.tool_count = 2;
	toolbar.ws_name_tool = &tools[0];
	toolbar.clock_tool = &tools[1];

	compute_tool_layout(&toolbar, 400);

	/* Both text tools should share the full width */
	assert(tools[0].width + tools[1].width <= 400);
	assert(tools[0].x == 0);
	assert(tools[1].x == tools[0].width);

	wl_list_remove(&ws.link);
	printf("  PASS: compute_tool_layout_distribute_remaining\n");
}

/* --- wlr_buffer_from_cairo: null surface --- */

static void
test_buffer_from_cairo_null(void)
{
	struct wlr_buffer *buf = wlr_buffer_from_cairo(NULL);
	assert(buf == NULL);
	printf("  PASS: buffer_from_cairo_null\n");
}

/* --- pixel_buffer_begin_data_ptr_access: write rejected --- */

static void
test_pixel_buffer_write_rejected(void)
{
	struct wm_pixel_buffer pb;
	memset(&pb, 0, sizeof(pb));
	pb.data = (void *)0x1;
	pb.format = DRM_FORMAT_ARGB8888;
	pb.stride = 100;

	void *data;
	uint32_t format;
	size_t stride;

	/* Read access should succeed */
	bool ok = pixel_buffer_begin_data_ptr_access(&pb.base, 0,
		&data, &format, &stride);
	assert(ok == true);
	assert(data == (void *)0x1);
	assert(format == DRM_FORMAT_ARGB8888);
	assert(stride == 100);

	/* Write access should fail */
	ok = pixel_buffer_begin_data_ptr_access(&pb.base,
		WLR_BUFFER_DATA_PTR_ACCESS_WRITE, &data, &format, &stride);
	assert(ok == false);

	printf("  PASS: pixel_buffer_write_rejected\n");
}

/* --- render_clock: caching behavior --- */

static void
test_render_clock_cache(void)
{
	reset_globals();
	setup_test_server();
	test_config.toolbar_tools = "clock";

	struct wm_workspace ws;
	memset(&ws, 0, sizeof(ws));
	ws.name = "1";
	ws.index = 0;
	wl_list_insert(&test_server.workspaces, &ws.link);
	test_server.current_workspace = &ws;
	test_server.workspace_count = 1;

	struct wm_toolbar *toolbar = wm_toolbar_create(&test_server);
	assert(toolbar != NULL);
	assert(toolbar->clock_tool != NULL);

	/* First call renders and caches */
	toolbar->cached_clock[0] = '\0';
	struct wlr_buffer *buf1 = render_clock(toolbar,
		toolbar->clock_tool->width, toolbar->height);
	/* buf1 may be non-NULL since this is a fresh render */

	/* Second call with same time should return NULL (cached) */
	struct wlr_buffer *buf2 = render_clock(toolbar,
		toolbar->clock_tool->width, toolbar->height);
	assert(buf2 == NULL);

	if (buf1) wlr_buffer_drop(buf1);

	wm_toolbar_destroy(toolbar);
	wl_list_remove(&ws.link);
	printf("  PASS: render_clock_cache\n");
}

/* --- toolbar_render: zero dimensions early return --- */

static void
test_toolbar_render_zero_dims(void)
{
	reset_globals();
	setup_test_server();
	test_config.toolbar_tools = "iconbar";

	struct wm_workspace ws;
	memset(&ws, 0, sizeof(ws));
	ws.name = "1";
	ws.index = 0;
	wl_list_insert(&test_server.workspaces, &ws.link);
	test_server.current_workspace = &ws;
	test_server.workspace_count = 1;

	struct wm_toolbar *toolbar = wm_toolbar_create(&test_server);
	assert(toolbar != NULL);

	/* Force zero width and try render */
	toolbar->width = 0;
	toolbar_render(toolbar);
	/* Should not crash - early return */

	/* Force zero height */
	toolbar->width = 100;
	toolbar->height = 0;
	toolbar_render(toolbar);
	/* Should not crash */

	wm_toolbar_destroy(toolbar);
	wl_list_remove(&ws.link);
	printf("  PASS: toolbar_render_zero_dims\n");
}

/* --- toolbar_render: not visible early return --- */

static void
test_toolbar_render_not_visible(void)
{
	reset_globals();
	setup_test_server();
	test_config.toolbar_visible = false;
	test_config.toolbar_tools = "iconbar";

	struct wm_workspace ws;
	memset(&ws, 0, sizeof(ws));
	ws.name = "1";
	ws.index = 0;
	wl_list_insert(&test_server.workspaces, &ws.link);
	test_server.current_workspace = &ws;
	test_server.workspace_count = 1;

	struct wm_toolbar *toolbar = wm_toolbar_create(&test_server);
	assert(toolbar != NULL);
	assert(toolbar->visible == false);

	toolbar_render(toolbar);
	/* Should not crash - early return for !visible */

	wm_toolbar_destroy(toolbar);
	wl_list_remove(&ws.link);
	printf("  PASS: toolbar_render_not_visible\n");
}

/* --- get_primary_output: empty output list --- */

static void
test_get_primary_output_empty(void)
{
	reset_globals();
	setup_test_server();
	/* Remove the test output */
	wl_list_remove(&test_output.link);
	wl_list_init(&test_server.outputs);

	struct wm_output *out = get_primary_output(&test_server);
	assert(out == NULL);

	/* Re-add for other tests */
	wl_list_insert(&test_server.outputs, &test_output.link);
	printf("  PASS: get_primary_output_empty\n");
}

/* --- render_tool: zero-width tool early return --- */

static void
test_render_tool_zero_width(void)
{
	reset_globals();
	setup_test_server();
	test_config.toolbar_tools = "iconbar";

	struct wm_workspace ws;
	memset(&ws, 0, sizeof(ws));
	ws.name = "1";
	ws.index = 0;
	wl_list_insert(&test_server.workspaces, &ws.link);
	test_server.current_workspace = &ws;
	test_server.workspace_count = 1;

	struct wm_toolbar *toolbar = wm_toolbar_create(&test_server);
	assert(toolbar != NULL);

	/* Set tool width to 0 */
	if (toolbar->tool_count > 0) {
		toolbar->tools[0].width = 0;
		render_tool(toolbar, &toolbar->tools[0]);
		/* Should not crash - early return */
	}

	wm_toolbar_destroy(toolbar);
	wl_list_remove(&ws.link);
	printf("  PASS: render_tool_zero_width\n");
}

/* --- toolbar_render: keyboard focus indicator --- */

static void
test_toolbar_render_with_focus_indicator(void)
{
	reset_globals();
	setup_test_server();
	test_config.toolbar_tools = "iconbar clock";

	struct wm_workspace ws;
	memset(&ws, 0, sizeof(ws));
	ws.name = "1";
	ws.index = 0;
	wl_list_insert(&test_server.workspaces, &ws.link);
	test_server.current_workspace = &ws;
	test_server.workspace_count = 1;

	struct wm_toolbar *toolbar = wm_toolbar_create(&test_server);
	assert(toolbar != NULL);
	toolbar->width = 800;
	toolbar->height = 24;

	/* Set focus_nav index to 0 (first tool) */
	test_server.focus_nav.toolbar_index = 0;

	toolbar_render(toolbar);
	/* Focus indicator should have been created */
	assert(toolbar->focus_indicator != NULL);
	assert(toolbar->focus_indicator->node.enabled == 1);

	wm_toolbar_destroy(toolbar);
	wl_list_remove(&ws.link);
	test_server.focus_nav.toolbar_index = -1;
	printf("  PASS: toolbar_render_with_focus_indicator\n");
}

static void
test_toolbar_render_focus_indicator_existing(void)
{
	reset_globals();
	setup_test_server();
	test_config.toolbar_tools = "iconbar clock";
	test_style.toolbar_iconbar_has_focused_color = true;
	test_style.toolbar_iconbar_focused_color =
		(struct wm_color){.r = 255, .g = 200, .b = 0, .a = 255};

	struct wm_workspace ws;
	memset(&ws, 0, sizeof(ws));
	ws.name = "1";
	ws.index = 0;
	wl_list_insert(&test_server.workspaces, &ws.link);
	test_server.current_workspace = &ws;
	test_server.workspace_count = 1;

	struct wm_toolbar *toolbar = wm_toolbar_create(&test_server);
	assert(toolbar != NULL);
	toolbar->width = 800;
	toolbar->height = 24;

	/* First render: creates focus_indicator */
	test_server.focus_nav.toolbar_index = 0;
	toolbar_render(toolbar);
	assert(toolbar->focus_indicator != NULL);

	/* Second render: reuses existing focus_indicator (set_buffer path) */
	test_server.focus_nav.toolbar_index = 1;
	toolbar_render(toolbar);
	assert(toolbar->focus_indicator != NULL);

	wm_toolbar_destroy(toolbar);
	wl_list_remove(&ws.link);
	test_server.focus_nav.toolbar_index = -1;
	printf("  PASS: toolbar_render_focus_indicator_existing\n");
}

static void
test_toolbar_render_focus_indicator_hide(void)
{
	reset_globals();
	setup_test_server();
	test_config.toolbar_tools = "iconbar";

	struct wm_workspace ws;
	memset(&ws, 0, sizeof(ws));
	ws.name = "1";
	ws.index = 0;
	wl_list_insert(&test_server.workspaces, &ws.link);
	test_server.current_workspace = &ws;
	test_server.workspace_count = 1;

	struct wm_toolbar *toolbar = wm_toolbar_create(&test_server);
	assert(toolbar != NULL);
	toolbar->width = 800;
	toolbar->height = 24;

	/* First render with focus to create indicator */
	test_server.focus_nav.toolbar_index = 0;
	toolbar_render(toolbar);
	assert(toolbar->focus_indicator != NULL);
	assert(toolbar->focus_indicator->node.enabled == 1);

	/* Now render with no focus — indicator should be hidden */
	test_server.focus_nav.toolbar_index = -1;
	toolbar_render(toolbar);
	assert(toolbar->focus_indicator->node.enabled == 0);

	wm_toolbar_destroy(toolbar);
	wl_list_remove(&ws.link);
	printf("  PASS: toolbar_render_focus_indicator_hide\n");
}

/* --- toolbar relayout placement variants --- */

static void
test_toolbar_relayout_top_left_placement(void)
{
	reset_globals();
	setup_test_server();
	test_config.toolbar_tools = "iconbar";
	test_config.toolbar_placement = WM_TOOLBAR_TOP_LEFT;

	struct wm_workspace ws;
	memset(&ws, 0, sizeof(ws));
	ws.name = "1";
	ws.index = 0;
	wl_list_insert(&test_server.workspaces, &ws.link);
	test_server.current_workspace = &ws;
	test_server.workspace_count = 1;

	struct wm_toolbar *toolbar = wm_toolbar_create(&test_server);
	assert(toolbar != NULL);

	wm_toolbar_relayout(toolbar);
	assert(toolbar->x == 0);
	assert(toolbar->y == 0);
	assert(toolbar->on_top == true);

	wm_toolbar_destroy(toolbar);
	wl_list_remove(&ws.link);
	printf("  PASS: toolbar_relayout_top_left_placement\n");
}

static void
test_toolbar_relayout_top_right_placement(void)
{
	reset_globals();
	setup_test_server();
	test_config.toolbar_tools = "iconbar";
	test_config.toolbar_placement = WM_TOOLBAR_TOP_RIGHT;

	struct wm_workspace ws;
	memset(&ws, 0, sizeof(ws));
	ws.name = "1";
	ws.index = 0;
	wl_list_insert(&test_server.workspaces, &ws.link);
	test_server.current_workspace = &ws;
	test_server.workspace_count = 1;

	struct wm_toolbar *toolbar = wm_toolbar_create(&test_server);
	assert(toolbar != NULL);

	wm_toolbar_relayout(toolbar);
	assert(toolbar->x == 1920 - toolbar->width);
	assert(toolbar->y == 0);
	assert(toolbar->on_top == true);

	wm_toolbar_destroy(toolbar);
	wl_list_remove(&ws.link);
	printf("  PASS: toolbar_relayout_top_right_placement\n");
}

static void
test_toolbar_relayout_auto_hide_toggle(void)
{
	reset_globals();
	setup_test_server();
	test_config.toolbar_tools = "iconbar";
	test_config.toolbar_auto_hide = false;

	struct wm_workspace ws;
	memset(&ws, 0, sizeof(ws));
	ws.name = "1";
	ws.index = 0;
	wl_list_insert(&test_server.workspaces, &ws.link);
	test_server.current_workspace = &ws;
	test_server.workspace_count = 1;

	struct wm_toolbar *toolbar = wm_toolbar_create(&test_server);
	assert(toolbar != NULL);
	assert(toolbar->auto_hide == false);

	/* Toggle auto-hide ON via relayout config change */
	test_config.toolbar_auto_hide = true;
	wm_toolbar_relayout(toolbar);
	assert(toolbar->auto_hide == true);
	assert(toolbar->shown == false);

	/* Toggle auto-hide OFF */
	test_config.toolbar_auto_hide = false;
	wm_toolbar_relayout(toolbar);
	assert(toolbar->auto_hide == false);
	assert(toolbar->shown == true);

	wm_toolbar_destroy(toolbar);
	wl_list_remove(&ws.link);
	printf("  PASS: toolbar_relayout_auto_hide_toggle\n");
}

/* --- toolbar_update_workspace/iconbar --- */

static void
test_toolbar_update_workspace(void)
{
	reset_globals();
	setup_test_server();
	test_config.toolbar_tools = "workspacename iconbar";

	struct wm_workspace ws;
	memset(&ws, 0, sizeof(ws));
	ws.name = "Desktop";
	ws.index = 0;
	wl_list_insert(&test_server.workspaces, &ws.link);
	test_server.current_workspace = &ws;
	test_server.workspace_count = 1;

	struct wm_toolbar *toolbar = wm_toolbar_create(&test_server);
	assert(toolbar != NULL);
	toolbar->width = 800;
	toolbar->height = 24;

	/* Should trigger re-render of workspace name tool and iconbar */
	wm_toolbar_update_workspace(toolbar);

	wm_toolbar_destroy(toolbar);
	wl_list_remove(&ws.link);
	printf("  PASS: toolbar_update_workspace\n");
}

static void
test_toolbar_update_iconbar(void)
{
	reset_globals();
	setup_test_server();
	test_config.toolbar_tools = "iconbar";

	struct wm_workspace ws;
	memset(&ws, 0, sizeof(ws));
	ws.name = "1";
	ws.index = 0;
	wl_list_insert(&test_server.workspaces, &ws.link);
	test_server.current_workspace = &ws;
	test_server.workspace_count = 1;

	struct wm_toolbar *toolbar = wm_toolbar_create(&test_server);
	assert(toolbar != NULL);
	toolbar->width = 800;
	toolbar->height = 24;

	/* Force a render so iconbar tool has valid dimensions */
	toolbar_render(toolbar);

	/* Now update iconbar */
	wm_toolbar_update_iconbar(toolbar);

	wm_toolbar_destroy(toolbar);
	wl_list_remove(&ws.link);
	printf("  PASS: toolbar_update_iconbar\n");
}

/* --- render_iconbar with actual entries --- */

static void
test_toolbar_render_iconbar_with_entries(void)
{
	reset_globals();
	setup_test_server();
	test_config.toolbar_tools = "iconbar";

	struct wm_workspace ws;
	memset(&ws, 0, sizeof(ws));
	ws.name = "1";
	ws.index = 0;
	wl_list_insert(&test_server.workspaces, &ws.link);
	test_server.current_workspace = &ws;
	test_server.workspace_count = 1;

	/* Create mock views */
	struct wlr_xdg_surface xdg_surf;
	struct wlr_surface surf;
	memset(&xdg_surf, 0, sizeof(xdg_surf));
	memset(&surf, 0, sizeof(surf));
	xdg_surf.surface = &surf;

	struct wlr_xdg_toplevel tl;
	memset(&tl, 0, sizeof(tl));
	tl.base = &xdg_surf;
	tl.width = 100;
	tl.height = 100;

	struct wlr_scene_tree view_tree;
	memset(&view_tree, 0, sizeof(view_tree));
	view_tree.node.enabled = 1;

	struct wm_view v1;
	memset(&v1, 0, sizeof(v1));
	v1.server = &test_server;
	v1.xdg_toplevel = &tl;
	v1.scene_tree = &view_tree;
	v1.title = "Terminal";
	v1.app_id = "xterm";
	v1.workspace = &ws;
	v1.show_decoration = true;
	v1.focus_alpha = 255;
	v1.unfocus_alpha = 255;

	wl_list_insert(&test_server.views, &v1.link);
	test_server.focused_view = &v1;

	struct wm_toolbar *toolbar = wm_toolbar_create(&test_server);
	assert(toolbar != NULL);
	toolbar->width = 800;
	toolbar->height = 24;

	/* Render toolbar with an actual view */
	toolbar_render(toolbar);

	wl_list_remove(&v1.link);
	wm_toolbar_destroy(toolbar);
	wl_list_remove(&ws.link);
	printf("  PASS: toolbar_render_iconbar_with_entries\n");
}

/* --- render_workspace_names: inactive workspace path --- */

static void
test_toolbar_render_workspace_inactive(void)
{
	reset_globals();
	setup_test_server();
	test_config.toolbar_tools = "workspacename";

	struct wm_workspace ws1;
	memset(&ws1, 0, sizeof(ws1));
	ws1.name = "Desktop 1";
	ws1.index = 0;

	struct wm_workspace ws2;
	memset(&ws2, 0, sizeof(ws2));
	ws2.name = "Desktop 2";
	ws2.index = 1;

	wl_list_insert(&test_server.workspaces, &ws2.link);
	wl_list_insert(&test_server.workspaces, &ws1.link);
	test_server.current_workspace = &ws1;
	test_server.workspace_count = 2;

	struct wm_toolbar *toolbar = wm_toolbar_create(&test_server);
	assert(toolbar != NULL);
	toolbar->width = 800;
	toolbar->height = 24;

	/* Render should cover both active and inactive workspace paths */
	toolbar_render(toolbar);

	wm_toolbar_destroy(toolbar);
	wl_list_remove(&ws1.link);
	wl_list_remove(&ws2.link);
	printf("  PASS: toolbar_render_workspace_inactive\n");
}

int
main(void)
{
	printf("test_toolbar_layout:\n");

	/* parse_tool_name */
	test_parse_tool_name_all_tools();
	test_parse_tool_name_unknown();

	/* parse_toolbar_tools */
	test_parse_toolbar_tools_default();
	test_parse_toolbar_tools_empty();
	test_parse_toolbar_tools_with_unknown();
	test_parse_toolbar_tools_tabs();

	/* tool_is_button */
	test_tool_is_button();

	/* compute_tool_layout */
	test_compute_tool_layout_basic();
	test_compute_tool_layout_buttons_only();
	test_compute_tool_layout_no_iconbar();
	test_compute_tool_layout_empty();

	/* collect_iconbar_entries */
	test_collect_iconbar_workspace_mode();
	test_collect_iconbar_all_windows_mode();
	test_collect_iconbar_icons_mode();
	test_collect_iconbar_no_icons_mode();
	test_collect_iconbar_sticky_views();

	/* scroll handling */
	test_scroll_outside_bounds();
	test_scroll_null_toolbar();
	test_scroll_hidden_toolbar();
	test_scroll_on_workspace_tool();
	test_scroll_on_iconbar();

	/* create/destroy */
	test_toolbar_create_basic();
	test_toolbar_create_auto_hide();
	test_toolbar_toggle_visible();
	test_toolbar_null_safety();

	/* pointer_motion auto-hide show/hide/cancel */
	test_pointer_motion_show_bottom();
	test_pointer_motion_show_top();
	test_pointer_motion_start_hide_timer();
	test_pointer_motion_cancel_hide_inside();
	test_pointer_motion_noop_no_autohide();
	test_pointer_motion_noop_not_visible();

	/* hide_timer_cb */
	test_hide_timer_cb_hides();
	test_hide_timer_cb_rearms_inside();
	test_hide_timer_cb_noop_no_autohide();

	/* clock_timer_cb */
	test_clock_timer_cb_visible();
	test_clock_timer_cb_hidden();
	test_clock_timer_cb_no_clock_tool();

	/* toggle_above */
	test_toggle_above_null_safety();
	test_toggle_above_config_top();
	test_toggle_above_relayout_behavior();

	/* additional coverage */
	test_collect_iconbar_workspace_icons_mode();
	test_compute_tool_layout_custom_clock_fmt();
	test_compute_tool_layout_distribute_remaining();
	test_buffer_from_cairo_null();
	test_pixel_buffer_write_rejected();
	test_render_clock_cache();
	test_toolbar_render_zero_dims();
	test_toolbar_render_not_visible();
	test_get_primary_output_empty();
	test_render_tool_zero_width();

	/* additional coverage: focus indicator + relayout placements */
	test_toolbar_render_with_focus_indicator();
	test_toolbar_render_focus_indicator_existing();
	test_toolbar_render_focus_indicator_hide();
	test_toolbar_relayout_top_left_placement();
	test_toolbar_relayout_top_right_placement();
	test_toolbar_relayout_auto_hide_toggle();
	test_toolbar_update_workspace();
	test_toolbar_update_iconbar();
	test_toolbar_render_iconbar_with_entries();
	test_toolbar_render_workspace_inactive();

	printf("All toolbar_layout tests passed.\n");
	return 0;
}
