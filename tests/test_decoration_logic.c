/*
 * test_decoration_logic.c - Unit tests for decoration hit-testing and layout
 *
 * Includes decoration.c directly with stub implementations to avoid
 * needing wlroots/wayland/Pango/Cairo libraries at link time.
 */

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* --- Block real headers via their include guards --- */
#define CAIRO_H
#define DRM_FOURCC_H
#define WAYLAND_SERVER_CORE_H
#define WAYLAND_SERVER_H
#define WAYLAND_UTIL_H
#define WLR_UTIL_LOG_H
#define WLR_TYPES_WLR_SCENE_H
#define WLR_TYPES_WLR_BUFFER_H
#define WLR_INTERFACES_WLR_BUFFER_H
#define WLR_TYPES_WLR_XDG_SHELL_H
#define WLR_TYPES_WLR_XDG_DECORATION_V1
#define WLR_UTIL_BOX_H
#define WM_VIEW_H
#define WM_SERVER_H
#define WM_RENDER_H
#define WM_CONFIG_H
#define WM_STYLE_H
#define WM_TABGROUP_H

/* Constants from drm_fourcc.h */
#define DRM_FORMAT_ARGB8888 0x34325241

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

#define wl_list_for_each(pos, head, member)				\
	for (pos = wl_container_of((head)->next, pos, member);		\
	     &(pos)->member != (head);					\
	     pos = wl_container_of((pos)->member.next, pos, member))

#define wl_list_for_each_safe(pos, tmp, head, member)			\
	for (pos = wl_container_of((head)->next, pos, member),		\
	     tmp = wl_container_of((pos)->member.next, tmp, member);	\
	     &(pos)->member != (head);					\
	     pos = tmp,							\
	     tmp = wl_container_of((pos)->member.next, tmp, member))

struct wl_listener {
	struct wl_list link;
	void (*notify)(struct wl_listener *, void *);
};

struct wl_signal {
	struct wl_list listener_list;
};

struct wl_display {
	int dummy;
};

/* wlr types */
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

#define WLR_BUFFER_DATA_PTR_ACCESS_WRITE 2

struct wlr_scene_node {
	int enabled;
	int x, y;
	struct wl_list link;
	struct wlr_scene_tree *parent;
	int type;
};

#define WLR_SCENE_NODE_TREE 0

struct wlr_scene_tree {
	struct wlr_scene_node node;
	struct wl_list children;
};

struct wlr_scene_rect {
	struct wlr_scene_node node;
	int width, height;
};

struct wlr_scene_buffer {
	struct wlr_scene_node node;
	struct wlr_buffer *buffer;
};

struct wlr_box {
	int x, y, width, height;
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

struct wlr_xdg_toplevel_decoration_v1 {
	struct {
		struct wl_signal destroy;
		struct wl_signal request_mode;
	} events;
};

#define WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE 2

struct wlr_xdg_decoration_manager_v1 {
	struct {
		struct wl_signal new_toplevel_decoration;
	} events;
};

/* wlr_log no-op */
enum wlr_log_importance { WLR_SILENT = 0, WLR_ERROR = 1, WLR_INFO = 2, WLR_DEBUG = 3 };
#define wlr_log(verb, fmt, ...) ((void)0)

/* --- Stub style types --- */

enum wm_texture_appearance { WM_TEX_FLAT, WM_TEX_RAISED, WM_TEX_SUNKEN };
enum wm_texture_fill { WM_TEX_SOLID, WM_TEX_GRADIENT, WM_TEX_PIXMAP, WM_TEX_PARENT_RELATIVE };
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
	int shadow_x, shadow_y;
	struct wm_color shadow_color;
	bool halo;
	struct wm_color halo_color;
};

enum wm_justify {
	WM_JUSTIFY_LEFT,
	WM_JUSTIFY_CENTER,
	WM_JUSTIFY_RIGHT,
};

#define WM_CORNER_TOP_LEFT     (1 << 0)
#define WM_CORNER_TOP_RIGHT    (1 << 1)
#define WM_CORNER_BOTTOM_LEFT  (1 << 2)
#define WM_CORNER_BOTTOM_RIGHT (1 << 3)

struct wm_style {
	struct wm_texture window_title_focus;
	struct wm_texture window_title_unfocus;
	int window_title_height;
	struct wm_texture window_label_focus;
	struct wm_texture window_label_unfocus;
	struct wm_texture window_label_active;
	struct wm_color window_label_focus_text_color;
	struct wm_color window_label_unfocus_text_color;
	struct wm_color window_label_active_text_color;
	struct wm_font window_label_focus_font;
	struct wm_font window_label_unfocus_font;
	enum wm_justify window_justify;
	struct wm_texture window_button_focus;
	struct wm_texture window_button_unfocus;
	struct wm_texture window_button_pressed;
	struct wm_color window_button_focus_pic_color;
	struct wm_color window_button_unfocus_pic_color;
	struct wm_color window_button_pressed_pic_color;
	char *window_close_pixmap, *window_close_unfocus_pixmap, *window_close_pressed_pixmap;
	char *window_maximize_pixmap, *window_maximize_unfocus_pixmap, *window_maximize_pressed_pixmap;
	char *window_iconify_pixmap, *window_iconify_unfocus_pixmap, *window_iconify_pressed_pixmap;
	char *window_shade_pixmap, *window_shade_unfocus_pixmap, *window_shade_pressed_pixmap;
	char *window_stick_pixmap, *window_stick_unfocus_pixmap, *window_stick_pressed_pixmap;
	struct wm_texture window_handle_focus;
	struct wm_texture window_handle_unfocus;
	int window_handle_width;
	struct wm_texture window_grip_focus;
	struct wm_texture window_grip_unfocus;
	struct wm_color window_frame_focus_color;
	struct wm_color window_frame_unfocus_color;
	struct wm_color window_border_color;
	int window_border_width;
	int window_bevel_width;
	uint8_t window_round_corners;
	int window_focus_border_width;
	struct wm_color window_focus_border_color;
	struct wm_texture menu_title;
	struct wm_color menu_title_text_color;
	struct wm_font menu_title_font;
	enum wm_justify menu_title_justify;
	struct wm_texture menu_frame;
	struct wm_color menu_frame_text_color;
	struct wm_font menu_frame_font;
	struct wm_texture menu_hilite;
	struct wm_color menu_hilite_text_color;
	struct wm_color menu_border_color;
	int menu_border_width;
	char *menu_bullet;
	enum wm_justify menu_bullet_position;
	int menu_item_height;
	int menu_title_height;
	struct wm_texture slit_texture;
	struct wm_color slit_border_color;
	int slit_border_width;
	struct wm_texture toolbar_texture;
	struct wm_color toolbar_text_color;
	struct wm_font toolbar_font;
	struct wm_color toolbar_iconbar_focused_color;
	struct wm_color toolbar_iconbar_focused_text_color;
	struct wm_color toolbar_iconbar_unfocused_color;
	struct wm_color toolbar_iconbar_unfocused_text_color;
	bool toolbar_iconbar_has_focused_color;
	bool toolbar_iconbar_has_unfocused_color;
	char *style_dir;
	char *style_path;
};

/* --- Button types --- */

enum wm_button_type {
	WM_BUTTON_CLOSE,
	WM_BUTTON_MAXIMIZE,
	WM_BUTTON_ICONIFY,
	WM_BUTTON_SHADE,
	WM_BUTTON_STICK,
};

/* --- Tab group --- */

/* enum wm_tab_bar_placement comes from decoration.h (not blocked) */

struct wm_tab_group {
	struct wl_list views;
	struct wm_view *active_view;
	int count;
	struct wm_server *server;
};

/* --- Config --- */

struct wm_config {
	char *titlebar_left;
	char *titlebar_right;
	bool tabs_intitlebar;
	int tab_padding;
	int tab_width;
	int tab_placement;
};

/* --- View --- */

struct wm_decoration;

struct wm_view {
	struct wl_list link;
	struct wm_server *server;
	struct wlr_xdg_toplevel *xdg_toplevel;
	struct wlr_scene_tree *scene_tree;
	char *title;
	char *app_id;
	struct wm_tab_group *tab_group;
	struct wl_list tab_link;
	struct wm_decoration *decoration;
};

/* --- Server --- */

struct wm_server {
	struct wl_display *wl_display;
	struct wm_config *config;
	struct wm_style *style;
	struct wlr_xdg_decoration_manager_v1 *xdg_decoration_mgr;
	struct wl_listener new_xdg_decoration;
};

/* --- Stub wlroots functions --- */

static void wlr_buffer_init(struct wlr_buffer *buf,
	const struct wlr_buffer_impl *impl, int w, int h)
{
	(void)impl;
	buf->width = w;
	buf->height = h;
	buf->ref_count = 1;
}

static void wlr_buffer_drop(struct wlr_buffer *buf)
{
	if (buf) buf->ref_count--;
}

static struct wlr_scene_tree *
wlr_scene_tree_create(struct wlr_scene_tree *parent)
{
	(void)parent;
	struct wlr_scene_tree *tree = calloc(1, sizeof(*tree));
	if (tree) {
		wl_list_init(&tree->children);
		wl_list_init(&tree->node.link);
	}
	return tree;
}

static struct wlr_scene_rect *
wlr_scene_rect_create(struct wlr_scene_tree *parent, int w, int h,
	const float color[4])
{
	(void)parent; (void)color;
	struct wlr_scene_rect *rect = calloc(1, sizeof(*rect));
	if (rect) {
		rect->width = w;
		rect->height = h;
		wl_list_init(&rect->node.link);
	}
	return rect;
}

static struct wlr_scene_buffer *
wlr_scene_buffer_create(struct wlr_scene_tree *parent, struct wlr_buffer *buf)
{
	(void)parent; (void)buf;
	struct wlr_scene_buffer *sb = calloc(1, sizeof(*sb));
	if (sb) {
		wl_list_init(&sb->node.link);
	}
	return sb;
}

static void wlr_scene_buffer_set_buffer(struct wlr_scene_buffer *sb,
	struct wlr_buffer *buf)
{
	if (sb) sb->buffer = buf;
}

static void wlr_scene_rect_set_size(struct wlr_scene_rect *rect, int w, int h)
{
	if (rect) { rect->width = w; rect->height = h; }
}

static void wlr_scene_rect_set_color(struct wlr_scene_rect *rect,
	const float color[4])
{
	(void)rect; (void)color;
}

static void wlr_scene_node_set_enabled(struct wlr_scene_node *node, bool en)
{
	if (node) node->enabled = en ? 1 : 0;
}

static void wlr_scene_node_set_position(struct wlr_scene_node *node,
	int x, int y)
{
	if (node) { node->x = x; node->y = y; }
}

static void wlr_scene_node_raise_to_top(struct wlr_scene_node *node)
{
	(void)node;
}

static void wlr_scene_node_lower_to_bottom(struct wlr_scene_node *node)
{
	(void)node;
}

static void wlr_scene_node_destroy(struct wlr_scene_node *node)
{
	(void)node;
}

static struct wlr_scene_tree *
wlr_scene_tree_from_node(struct wlr_scene_node *node)
{
	return (struct wlr_scene_tree *)node;
}

static void wlr_xdg_surface_get_geometry(struct wlr_xdg_surface *surface,
	struct wlr_box *box)
{
	(void)surface;
	box->x = 0;
	box->y = 0;
	box->width = 800;
	box->height = 600;
}

static uint32_t wlr_xdg_toplevel_set_size(struct wlr_xdg_toplevel *tl,
	int w, int h)
{
	(void)tl; (void)w; (void)h;
	return 0;
}

static void wl_signal_add(struct wl_signal *signal, struct wl_listener *listener)
{
	(void)signal; (void)listener;
}

static void wlr_xdg_toplevel_decoration_v1_set_mode(
	struct wlr_xdg_toplevel_decoration_v1 *deco, int mode)
{
	(void)deco; (void)mode;
}

static struct wlr_xdg_decoration_manager_v1 *
wlr_xdg_decoration_manager_v1_create(struct wl_display *display)
{
	(void)display;
	return NULL;
}

/* --- Stub Cairo functions --- */

typedef struct { int dummy; } cairo_surface_t;
typedef struct { int dummy; } cairo_t;
typedef enum { CAIRO_FORMAT_ARGB32 = 0 } cairo_format_t;
typedef enum { CAIRO_STATUS_SUCCESS = 0 } cairo_status_t;

static cairo_surface_t g_stub_surface;

static cairo_surface_t *
cairo_image_surface_create(cairo_format_t fmt, int w, int h)
{
	(void)fmt; (void)w; (void)h;
	return &g_stub_surface;
}

static cairo_status_t cairo_surface_status(cairo_surface_t *s)
{
	(void)s; return CAIRO_STATUS_SUCCESS;
}

static void cairo_surface_flush(cairo_surface_t *s) { (void)s; }
static void cairo_surface_destroy(cairo_surface_t *s) { (void)s; }

static int cairo_image_surface_get_width(cairo_surface_t *s)
{
	(void)s; return 100;
}

static int cairo_image_surface_get_height(cairo_surface_t *s)
{
	(void)s; return 100;
}

static int cairo_image_surface_get_stride(cairo_surface_t *s)
{
	(void)s; return 400;
}

static unsigned char *cairo_image_surface_get_data(cairo_surface_t *s)
{
	(void)s;
	static unsigned char dummy_data[400 * 100];
	return dummy_data;
}

static cairo_t g_stub_cairo;

static cairo_t *cairo_create(cairo_surface_t *s) { (void)s; return &g_stub_cairo; }
static void cairo_destroy(cairo_t *cr) { (void)cr; }
static void cairo_set_source_rgba(cairo_t *cr, double r, double g, double b, double a)
{
	(void)cr; (void)r; (void)g; (void)b; (void)a;
}
static void cairo_paint(cairo_t *cr) { (void)cr; }
static void cairo_set_source_surface(cairo_t *cr, cairo_surface_t *s, double x, double y)
{
	(void)cr; (void)s; (void)x; (void)y;
}
static void cairo_rectangle(cairo_t *cr, double x, double y, double w, double h)
{
	(void)cr; (void)x; (void)y; (void)w; (void)h;
}
static void cairo_fill(cairo_t *cr) { (void)cr; }
static void cairo_set_fill_rule(cairo_t *cr, int rule) { (void)cr; (void)rule; }
static void cairo_new_sub_path(cairo_t *cr) { (void)cr; }
static void cairo_save(cairo_t *cr) { (void)cr; }
static void cairo_restore(cairo_t *cr) { (void)cr; }
static void cairo_translate(cairo_t *cr, double x, double y) { (void)cr; (void)x; (void)y; }
static void cairo_rotate(cairo_t *cr, double angle) { (void)cr; (void)angle; }

#define CAIRO_FILL_RULE_EVEN_ODD 1

/* --- Stub render functions --- */

static cairo_surface_t *
wm_render_texture(const struct wm_texture *tex, int w, int h, float scale)
{
	(void)tex; (void)w; (void)h; (void)scale;
	return NULL;
}

static cairo_surface_t *
wm_render_text(const char *text, const struct wm_font *font,
	const struct wm_color *color, int max_w, int *out_w, int *out_h,
	enum wm_justify justify, float scale)
{
	(void)text; (void)font; (void)color; (void)max_w;
	(void)justify; (void)scale;
	if (out_w) *out_w = 50;
	if (out_h) *out_h = 12;
	return &g_stub_surface;
}

static cairo_surface_t *
wm_render_button_glyph(enum wm_button_type type, const struct wm_color *color,
	int size, float scale)
{
	(void)type; (void)color; (void)size; (void)scale;
	return NULL;
}

static void
wm_render_rounded_rect_path(cairo_t *cr, double x, double y,
	double w, double h, double r, uint8_t corners)
{
	(void)cr; (void)x; (void)y; (void)w; (void)h; (void)r; (void)corners;
}

/* --- Include decoration.c --- */

/* We need decoration.h declarations before including .c */
#include "decoration.c"

/* ===================== Test helpers ===================== */

static struct wm_server test_server;
static struct wm_style test_style;
static struct wm_config test_config;
static struct wlr_scene_tree test_scene_tree;
static struct wlr_xdg_toplevel test_toplevel;
static struct wlr_xdg_surface test_xdg_surface;
static struct wlr_surface test_surface;

static void
setup(void)
{
	memset(&test_server, 0, sizeof(test_server));
	memset(&test_style, 0, sizeof(test_style));
	memset(&test_config, 0, sizeof(test_config));
	memset(&test_scene_tree, 0, sizeof(test_scene_tree));

	wl_list_init(&test_scene_tree.children);
	wl_list_init(&test_scene_tree.node.link);

	test_server.config = &test_config;
	test_server.style = &test_style;

	test_xdg_surface.surface = &test_surface;
	test_toplevel.base = &test_xdg_surface;
	test_toplevel.width = 800;
	test_toplevel.height = 600;

	/* Default style dimensions */
	test_style.window_title_height = 20;
	test_style.window_handle_width = 6;
	test_style.window_border_width = 1;
	test_style.window_label_focus_font.size = 12;
}

static struct wm_view test_view;

static void
setup_view(void)
{
	memset(&test_view, 0, sizeof(test_view));
	test_view.server = &test_server;
	test_view.xdg_toplevel = &test_toplevel;
	test_view.scene_tree = &test_scene_tree;
	test_view.title = "Test Window";
	test_view.tab_group = NULL;
	wl_list_init(&test_view.tab_link);
}

/*
 * Create a decoration with specified dimensions without needing wlroots
 * scene graph. Used for testing hit-test functions that only check
 * geometry fields.
 */
static struct wm_decoration *
make_test_decoration(enum wm_decor_preset preset, int cw, int ch)
{
	struct wm_decoration *deco = calloc(1, sizeof(*deco));
	assert(deco != NULL);

	setup_view();
	deco->view = &test_view;
	deco->server = &test_server;
	deco->focused = true;
	deco->preset = preset;
	deco->titlebar_height = 20;
	deco->handle_height = 6;
	deco->border_width = 1;
	deco->grip_width = 20;
	deco->content_width = cw;
	deco->content_height = ch;
	deco->tab_bar_size = 0;
	deco->content_area = (struct wlr_box){
		.x = 1, .y = 21, .width = cw, .height = ch};
	deco->buttons_left = NULL;
	deco->buttons_left_count = 0;
	deco->buttons_right = NULL;
	deco->buttons_right_count = 0;

	return deco;
}

static void
free_test_decoration(struct wm_decoration *deco)
{
	free(deco->buttons_left);
	free(deco->buttons_right);
	free(deco);
}

/* ===================== Tests ===================== */

/* --- parse_button_type --- */

static void
test_parse_button_type_close(void)
{
	enum wm_button_type type;
	bool ok = parse_button_type("Close", &type);
	assert(ok == true);
	assert(type == WM_BUTTON_CLOSE);
	printf("  PASS: parse_button_type_close\n");
}

static void
test_parse_button_type_maximize(void)
{
	enum wm_button_type type;
	bool ok = parse_button_type("Maximize", &type);
	assert(ok == true);
	assert(type == WM_BUTTON_MAXIMIZE);
	printf("  PASS: parse_button_type_maximize\n");
}

static void
test_parse_button_type_minimize(void)
{
	enum wm_button_type type;
	bool ok = parse_button_type("Minimize", &type);
	assert(ok == true);
	assert(type == WM_BUTTON_ICONIFY);

	ok = parse_button_type("Iconify", &type);
	assert(ok == true);
	assert(type == WM_BUTTON_ICONIFY);
	printf("  PASS: parse_button_type_minimize\n");
}

static void
test_parse_button_type_shade(void)
{
	enum wm_button_type type;
	bool ok = parse_button_type("Shade", &type);
	assert(ok == true);
	assert(type == WM_BUTTON_SHADE);
	printf("  PASS: parse_button_type_shade\n");
}

static void
test_parse_button_type_stick(void)
{
	enum wm_button_type type;
	bool ok = parse_button_type("Stick", &type);
	assert(ok == true);
	assert(type == WM_BUTTON_STICK);
	printf("  PASS: parse_button_type_stick\n");
}

static void
test_parse_button_type_case_insensitive(void)
{
	enum wm_button_type type;
	bool ok = parse_button_type("cLoSe", &type);
	assert(ok == true);
	assert(type == WM_BUTTON_CLOSE);
	printf("  PASS: parse_button_type_case_insensitive\n");
}

static void
test_parse_button_type_unknown(void)
{
	enum wm_button_type type;
	bool ok = parse_button_type("Foobar", &type);
	assert(ok == false);
	printf("  PASS: parse_button_type_unknown\n");
}

/* --- parse_button_layout --- */

static void
test_parse_button_layout_single(void)
{
	int count = 0;
	struct wm_decor_button *buttons = parse_button_layout("Close", &count);
	assert(buttons != NULL);
	assert(count == 1);
	assert(buttons[0].type == WM_BUTTON_CLOSE);
	free(buttons);
	printf("  PASS: parse_button_layout_single\n");
}

static void
test_parse_button_layout_multiple(void)
{
	int count = 0;
	struct wm_decor_button *buttons = parse_button_layout(
		"Shade Minimize Maximize Close", &count);
	assert(buttons != NULL);
	assert(count == 4);
	assert(buttons[0].type == WM_BUTTON_SHADE);
	assert(buttons[1].type == WM_BUTTON_ICONIFY);
	assert(buttons[2].type == WM_BUTTON_MAXIMIZE);
	assert(buttons[3].type == WM_BUTTON_CLOSE);
	free(buttons);
	printf("  PASS: parse_button_layout_multiple\n");
}

static void
test_parse_button_layout_empty(void)
{
	int count = 0;
	struct wm_decor_button *buttons = parse_button_layout("", &count);
	assert(buttons == NULL);
	assert(count == 0);
	printf("  PASS: parse_button_layout_empty\n");
}

static void
test_parse_button_layout_null(void)
{
	int count = 0;
	struct wm_decor_button *buttons = parse_button_layout(NULL, &count);
	assert(buttons == NULL);
	assert(count == 0);
	printf("  PASS: parse_button_layout_null\n");
}

static void
test_parse_button_layout_invalid_tokens(void)
{
	int count = 0;
	struct wm_decor_button *buttons = parse_button_layout(
		"Foo Bar Baz", &count);
	assert(buttons == NULL);
	assert(count == 0);
	printf("  PASS: parse_button_layout_invalid_tokens\n");
}

static void
test_parse_button_layout_mixed_valid_invalid(void)
{
	int count = 0;
	struct wm_decor_button *buttons = parse_button_layout(
		"Close Blah Shade", &count);
	assert(buttons != NULL);
	assert(count == 2);
	assert(buttons[0].type == WM_BUTTON_CLOSE);
	assert(buttons[1].type == WM_BUTTON_SHADE);
	free(buttons);
	printf("  PASS: parse_button_layout_mixed_valid_invalid\n");
}

/* --- configure_buttons --- */

static void
test_configure_buttons(void)
{
	setup();
	struct wm_decoration *deco = make_test_decoration(WM_DECOR_NORMAL, 800, 600);

	wm_decoration_configure_buttons(deco, "Stick", "Shade Minimize Maximize Close");

	assert(deco->buttons_left_count == 1);
	assert(deco->buttons_left[0].type == WM_BUTTON_STICK);
	assert(deco->buttons_right_count == 4);
	assert(deco->buttons_right[3].type == WM_BUTTON_CLOSE);

	free_test_decoration(deco);
	printf("  PASS: configure_buttons\n");
}

static void
test_configure_buttons_null(void)
{
	setup();
	/* Should not crash */
	wm_decoration_configure_buttons(NULL, "Close", "Close");
	printf("  PASS: configure_buttons_null\n");
}

/* --- button_at --- */

static void
test_button_at_hit(void)
{
	setup();
	struct wm_decoration *deco = make_test_decoration(WM_DECOR_NORMAL, 800, 600);

	/* Set up a left button with known box */
	free(deco->buttons_left);
	deco->buttons_left = calloc(1, sizeof(struct wm_decor_button));
	deco->buttons_left_count = 1;
	deco->buttons_left[0].type = WM_BUTTON_STICK;
	deco->buttons_left[0].box = (struct wlr_box){5, 5, 12, 12};

	struct wm_decor_button *btn = wm_decoration_button_at(deco, 10, 10);
	assert(btn != NULL);
	assert(btn->type == WM_BUTTON_STICK);

	free_test_decoration(deco);
	printf("  PASS: button_at_hit\n");
}

static void
test_button_at_miss(void)
{
	setup();
	struct wm_decoration *deco = make_test_decoration(WM_DECOR_NORMAL, 800, 600);

	free(deco->buttons_left);
	deco->buttons_left = calloc(1, sizeof(struct wm_decor_button));
	deco->buttons_left_count = 1;
	deco->buttons_left[0].type = WM_BUTTON_STICK;
	deco->buttons_left[0].box = (struct wlr_box){5, 5, 12, 12};

	/* Click outside button area */
	struct wm_decor_button *btn = wm_decoration_button_at(deco, 50, 50);
	assert(btn == NULL);

	free_test_decoration(deco);
	printf("  PASS: button_at_miss\n");
}

static void
test_button_at_right_button(void)
{
	setup();
	struct wm_decoration *deco = make_test_decoration(WM_DECOR_NORMAL, 800, 600);

	free(deco->buttons_right);
	deco->buttons_right = calloc(1, sizeof(struct wm_decor_button));
	deco->buttons_right_count = 1;
	deco->buttons_right[0].type = WM_BUTTON_CLOSE;
	deco->buttons_right[0].box = (struct wlr_box){780, 5, 12, 12};

	struct wm_decor_button *btn = wm_decoration_button_at(deco, 785, 10);
	assert(btn != NULL);
	assert(btn->type == WM_BUTTON_CLOSE);

	free_test_decoration(deco);
	printf("  PASS: button_at_right_button\n");
}

static void
test_button_at_null(void)
{
	struct wm_decor_button *btn = wm_decoration_button_at(NULL, 0, 0);
	assert(btn == NULL);
	printf("  PASS: button_at_null\n");
}

/* --- region_at --- */

static void
test_region_at_titlebar(void)
{
	setup();
	struct wm_decoration *deco = make_test_decoration(WM_DECOR_NORMAL, 800, 600);

	/* Titlebar is at y=[border_width, border_width+titlebar_height) = [1, 21) */
	enum wm_decor_region region = wm_decoration_region_at(deco, 400, 10);
	assert(region == WM_DECOR_REGION_TITLEBAR);

	free_test_decoration(deco);
	printf("  PASS: region_at_titlebar\n");
}

static void
test_region_at_border_top(void)
{
	setup();
	struct wm_decoration *deco = make_test_decoration(WM_DECOR_NORMAL, 800, 600);

	/* Top border at y=[0, border_width) = [0, 1) */
	enum wm_decor_region region = wm_decoration_region_at(deco, 400, 0);
	assert(region == WM_DECOR_REGION_BORDER_TOP);

	free_test_decoration(deco);
	printf("  PASS: region_at_border_top\n");
}

static void
test_region_at_border_bottom(void)
{
	setup();
	struct wm_decoration *deco = make_test_decoration(WM_DECOR_NORMAL, 800, 600);

	/* Outer height = bw + th + ch + hh + bw = 1 + 20 + 600 + 6 + 1 = 628 */
	/* Bottom border at y=[628-1, 628) = [627, 628) */
	enum wm_decor_region region = wm_decoration_region_at(deco, 400, 627);
	assert(region == WM_DECOR_REGION_BORDER_BOTTOM);

	free_test_decoration(deco);
	printf("  PASS: region_at_border_bottom\n");
}

static void
test_region_at_border_left(void)
{
	setup();
	struct wm_decoration *deco = make_test_decoration(WM_DECOR_NORMAL, 800, 600);

	/* Left border at x=[0, 1) for y not in top/bottom border */
	enum wm_decor_region region = wm_decoration_region_at(deco, 0, 100);
	assert(region == WM_DECOR_REGION_BORDER_LEFT);

	free_test_decoration(deco);
	printf("  PASS: region_at_border_left\n");
}

static void
test_region_at_border_right(void)
{
	setup();
	struct wm_decoration *deco = make_test_decoration(WM_DECOR_NORMAL, 800, 600);

	/* Outer width = cw + 2*bw = 800 + 2 = 802 */
	/* Right border at x=[801, 802) */
	enum wm_decor_region region = wm_decoration_region_at(deco, 801, 100);
	assert(region == WM_DECOR_REGION_BORDER_RIGHT);

	free_test_decoration(deco);
	printf("  PASS: region_at_border_right\n");
}

static void
test_region_at_handle(void)
{
	setup();
	struct wm_decoration *deco = make_test_decoration(WM_DECOR_NORMAL, 800, 600);

	/* Handle y range: bw + th + ch = 1+20+600 = 621 to 621+6 = 627 */
	/* Handle x range: bw+gw to bw+cw-gw = 1+20=21 to 1+800-20=781 */
	enum wm_decor_region region = wm_decoration_region_at(deco, 400, 623);
	assert(region == WM_DECOR_REGION_HANDLE);

	free_test_decoration(deco);
	printf("  PASS: region_at_handle\n");
}

static void
test_region_at_grip_left(void)
{
	setup();
	struct wm_decoration *deco = make_test_decoration(WM_DECOR_NORMAL, 800, 600);

	/* Grip left at handle y, x < bw + gw = 1+20 = 21 */
	enum wm_decor_region region = wm_decoration_region_at(deco, 5, 623);
	assert(region == WM_DECOR_REGION_GRIP_LEFT);

	free_test_decoration(deco);
	printf("  PASS: region_at_grip_left\n");
}

static void
test_region_at_grip_right(void)
{
	setup();
	struct wm_decoration *deco = make_test_decoration(WM_DECOR_NORMAL, 800, 600);

	/* Grip right at handle y, x >= bw + cw - gw = 1+800-20 = 781 */
	enum wm_decor_region region = wm_decoration_region_at(deco, 790, 623);
	assert(region == WM_DECOR_REGION_GRIP_RIGHT);

	free_test_decoration(deco);
	printf("  PASS: region_at_grip_right\n");
}

static void
test_region_at_client(void)
{
	setup();
	struct wm_decoration *deco = make_test_decoration(WM_DECOR_NORMAL, 800, 600);

	/* Client area: after titlebar, within borders, before handle */
	enum wm_decor_region region = wm_decoration_region_at(deco, 400, 300);
	assert(region == WM_DECOR_REGION_CLIENT);

	free_test_decoration(deco);
	printf("  PASS: region_at_client\n");
}

static void
test_region_at_outside(void)
{
	setup();
	struct wm_decoration *deco = make_test_decoration(WM_DECOR_NORMAL, 800, 600);

	enum wm_decor_region region = wm_decoration_region_at(deco, -10, -10);
	assert(region == WM_DECOR_REGION_NONE);

	region = wm_decoration_region_at(deco, 1000, 1000);
	assert(region == WM_DECOR_REGION_NONE);

	free_test_decoration(deco);
	printf("  PASS: region_at_outside\n");
}

static void
test_region_at_null(void)
{
	enum wm_decor_region region = wm_decoration_region_at(NULL, 0, 0);
	assert(region == WM_DECOR_REGION_NONE);
	printf("  PASS: region_at_null\n");
}

/* --- Preset-dependent regions --- */

static void
test_region_at_no_handle_tiny(void)
{
	setup();
	struct wm_decoration *deco = make_test_decoration(WM_DECOR_TINY, 800, 600);

	/* TINY has titlebar but no handle. Where the handle would be
	 * should be client area or border */
	enum wm_decor_region region = wm_decoration_region_at(deco, 400, 623);
	/* No handle => out of bounds or client area (depends on outer_h) */
	/* outer_h for TINY: bw + th + ch + bw = 1+20+600+1 = 622 */
	/* y=623 is outside */
	assert(region == WM_DECOR_REGION_NONE);

	free_test_decoration(deco);
	printf("  PASS: region_at_no_handle_tiny\n");
}

static void
test_region_at_no_titlebar_border(void)
{
	setup();
	struct wm_decoration *deco = make_test_decoration(WM_DECOR_BORDER, 800, 600);

	/* BORDER has no titlebar. y=10 should be in client or border area */
	enum wm_decor_region region = wm_decoration_region_at(deco, 400, 10);
	assert(region == WM_DECOR_REGION_CLIENT);

	free_test_decoration(deco);
	printf("  PASS: region_at_no_titlebar_border\n");
}

/* --- get_extents --- */

static void
test_get_extents_normal(void)
{
	setup();
	struct wm_decoration *deco = make_test_decoration(WM_DECOR_NORMAL, 800, 600);

	int top, bottom, left, right;
	wm_decoration_get_extents(deco, &top, &bottom, &left, &right);

	/* NORMAL: top = bw + th = 1+20=21, bottom = bw + hh = 1+6=7 */
	assert(top == 21);
	assert(bottom == 7);
	assert(left == 1);
	assert(right == 1);

	free_test_decoration(deco);
	printf("  PASS: get_extents_normal\n");
}

static void
test_get_extents_none(void)
{
	setup();
	struct wm_decoration *deco = make_test_decoration(WM_DECOR_NONE, 800, 600);

	int top, bottom, left, right;
	wm_decoration_get_extents(deco, &top, &bottom, &left, &right);

	assert(top == 0);
	assert(bottom == 0);
	assert(left == 0);
	assert(right == 0);

	free_test_decoration(deco);
	printf("  PASS: get_extents_none\n");
}

static void
test_get_extents_border(void)
{
	setup();
	struct wm_decoration *deco = make_test_decoration(WM_DECOR_BORDER, 800, 600);

	int top, bottom, left, right;
	wm_decoration_get_extents(deco, &top, &bottom, &left, &right);

	/* BORDER: top = bw = 1, bottom = bw = 1 (no titlebar, no handle) */
	assert(top == 1);
	assert(bottom == 1);
	assert(left == 1);
	assert(right == 1);

	free_test_decoration(deco);
	printf("  PASS: get_extents_border\n");
}

static void
test_get_extents_tab(void)
{
	setup();
	struct wm_decoration *deco = make_test_decoration(WM_DECOR_TAB, 800, 600);

	int top, bottom, left, right;
	wm_decoration_get_extents(deco, &top, &bottom, &left, &right);

	/* TAB: has titlebar, no handle, no borders in extent computation */
	/* Wait - TAB actually has no borders? Let's check: */
	/* TAB is NOT in has_borders check (only NORMAL and BORDER) */
	/* So TAB: top = bw + th = 1+20=21, bottom = bw = 1, left = bw=1, right=bw=1 */
	assert(top == 21);
	assert(bottom == 1);
	assert(left == 1);
	assert(right == 1);

	free_test_decoration(deco);
	printf("  PASS: get_extents_tab\n");
}

static void
test_get_extents_tiny(void)
{
	setup();
	struct wm_decoration *deco = make_test_decoration(WM_DECOR_TINY, 800, 600);

	int top, bottom, left, right;
	wm_decoration_get_extents(deco, &top, &bottom, &left, &right);

	/* TINY: titlebar yes, handle no, borders no */
	assert(top == 21);
	assert(bottom == 1);
	assert(left == 1);
	assert(right == 1);

	free_test_decoration(deco);
	printf("  PASS: get_extents_tiny\n");
}

static void
test_get_extents_tool(void)
{
	setup();
	struct wm_decoration *deco = make_test_decoration(WM_DECOR_TOOL, 800, 600);

	int top, bottom, left, right;
	wm_decoration_get_extents(deco, &top, &bottom, &left, &right);

	/* TOOL: same as TINY */
	assert(top == 21);
	assert(bottom == 1);
	assert(left == 1);
	assert(right == 1);

	free_test_decoration(deco);
	printf("  PASS: get_extents_tool\n");
}

static void
test_get_extents_null(void)
{
	int top = -1, bottom = -1, left = -1, right = -1;
	wm_decoration_get_extents(NULL, &top, &bottom, &left, &right);
	assert(top == 0);
	assert(bottom == 0);
	assert(left == 0);
	assert(right == 0);
	printf("  PASS: get_extents_null\n");
}

/* --- Style helpers --- */

static void
test_style_titlebar_height_default(void)
{
	setup();
	test_style.window_title_height = 0;
	test_style.window_label_focus_font.size = 14;

	int h = style_titlebar_height(&test_style);
	assert(h == 14 + 8); /* font size + padding */
	printf("  PASS: style_titlebar_height_default\n");
}

static void
test_style_titlebar_height_explicit(void)
{
	setup();
	test_style.window_title_height = 30;

	int h = style_titlebar_height(&test_style);
	assert(h == 30);
	printf("  PASS: style_titlebar_height_explicit\n");
}

static void
test_style_handle_height_default(void)
{
	setup();
	test_style.window_handle_width = 0;

	int h = style_handle_height(&test_style);
	assert(h == DEFAULT_HANDLE_HEIGHT); /* 6 */
	printf("  PASS: style_handle_height_default\n");
}

static void
test_style_border_width_default(void)
{
	setup();
	test_style.window_border_width = -1;

	int w = style_border_width(&test_style);
	assert(w == DEFAULT_BORDER_WIDTH); /* 1 */
	printf("  PASS: style_border_width_default\n");
}

/* --- wm_color_to_float4 --- */

static void
test_color_to_float4(void)
{
	struct wm_color c = {255, 0, 128, 255};
	float out[4];
	wm_color_to_float4(&c, out);
	assert(out[0] == 1.0f);
	assert(out[1] == 0.0f);
	assert(out[3] == 1.0f);
	printf("  PASS: color_to_float4\n");
}

/* --- tab_at (in-titlebar) --- */

static void
test_tab_at_no_group(void)
{
	setup();
	struct wm_decoration *deco = make_test_decoration(WM_DECOR_NORMAL, 800, 600);

	int idx = wm_decoration_tab_at(deco, 100, 10);
	assert(idx == -1);

	free_test_decoration(deco);
	printf("  PASS: tab_at_no_group\n");
}

static void
test_tab_at_outside_titlebar(void)
{
	setup();
	struct wm_decoration *deco = make_test_decoration(WM_DECOR_NORMAL, 800, 600);

	/* Set up a fake tab group */
	struct wm_tab_group tg;
	tg.count = 2;
	tg.active_view = &test_view;
	wl_list_init(&tg.views);
	test_view.tab_group = &tg;
	deco->view = &test_view;

	/* Click outside titlebar y range (y=300 is in client area) */
	int idx = wm_decoration_tab_at(deco, 100, 300);
	assert(idx == -1);

	test_view.tab_group = NULL;
	free_test_decoration(deco);
	printf("  PASS: tab_at_outside_titlebar\n");
}

static void
test_tab_at_null(void)
{
	int idx = wm_decoration_tab_at(NULL, 0, 0);
	assert(idx == -1);
	printf("  PASS: tab_at_null\n");
}

static void
test_tab_at_multiple_intitlebar(void)
{
	setup();
	test_config.tabs_intitlebar = true;

	/* Create decoration first (setup_view is called inside) */
	struct wm_decoration *deco = make_test_decoration(WM_DECOR_NORMAL, 600, 400);
	deco->tab_bar_size = 0; /* in-titlebar tabs */

	struct wm_view view2 = {0};
	view2.server = &test_server;
	view2.title = "Tab 2";
	wl_list_init(&view2.tab_link);
	struct wm_view view3 = {0};
	view3.server = &test_server;
	view3.title = "Tab 3";
	wl_list_init(&view3.tab_link);

	/* Set up tab group after make_test_decoration (which calls setup_view) */
	struct wm_tab_group tg;
	wl_list_init(&tg.views);
	tg.count = 3;
	tg.active_view = &test_view;
	/* Insert in reverse for correct order: test_view, view2, view3 */
	wl_list_insert(&tg.views, &view3.tab_link);
	wl_list_insert(&tg.views, &view2.tab_link);
	wl_list_insert(&tg.views, &test_view.tab_link);
	test_view.tab_group = &tg;

	/* No buttons => label_width = content_width = 600
	 * tab_width = 600/3 = 200
	 * Titlebar y range: [bw=1, bw+th=21)
	 * label_x = bw = 1
	 * Tab 0: lx [0, 200)   => x=[1, 201)
	 * Tab 1: lx [200, 400) => x=[201, 401)
	 * Tab 2: lx [400, 600) => x=[401, 601)
	 */
	int idx = wm_decoration_tab_at(deco, 100, 10);
	assert(idx == 0);

	idx = wm_decoration_tab_at(deco, 300, 10);
	assert(idx == 1);

	idx = wm_decoration_tab_at(deco, 500, 10);
	assert(idx == 2);

	test_view.tab_group = NULL;
	free_test_decoration(deco);
	printf("  PASS: tab_at_multiple_intitlebar\n");
}

static void
test_tab_at_no_titlebar_preset(void)
{
	setup();
	struct wm_decoration *deco = make_test_decoration(WM_DECOR_BORDER, 800, 600);

	struct wm_tab_group tg;
	wl_list_init(&tg.views);
	tg.count = 2;
	tg.active_view = &test_view;
	wl_list_insert(&tg.views, &test_view.tab_link);
	test_view.tab_group = &tg;

	/* BORDER preset has no titlebar */
	int idx = wm_decoration_tab_at(deco, 100, 10);
	assert(idx == -1);

	test_view.tab_group = NULL;
	free_test_decoration(deco);
	printf("  PASS: tab_at_no_titlebar_preset\n");
}

/* --- tab_at (external tab bar) --- */

static void
test_tab_at_external_horizontal(void)
{
	setup();
	test_config.tabs_intitlebar = false;
	test_config.tab_padding = 2;

	struct wm_decoration *deco = make_test_decoration(WM_DECOR_NORMAL, 400, 300);
	deco->tab_bar_size = 20;
	deco->tab_bar_placement = WM_TAB_BAR_TOP;
	deco->tab_bar_box = (struct wlr_box){1, 21, 400, 20};

	struct wm_view view2 = {0};
	view2.server = &test_server;
	wl_list_init(&view2.tab_link);

	struct wm_tab_group tg;
	wl_list_init(&tg.views);
	tg.count = 2;
	tg.active_view = &test_view;
	wl_list_insert(&tg.views, &view2.tab_link);
	wl_list_insert(&tg.views, &test_view.tab_link);
	test_view.tab_group = &tg;

	/* Horizontal: 2 tabs, padding=2, total_pad=2, seg_w=(400-2)/2=199
	 * Tab 0: x relative to box.x in [0, 199+2=201)
	 * Tab 1: x relative to box.x in [201, 400) */
	int idx = wm_decoration_tab_at(deco, 100, 30);
	assert(idx == 0);

	idx = wm_decoration_tab_at(deco, 350, 30);
	assert(idx == 1);

	test_view.tab_group = NULL;
	free_test_decoration(deco);
	printf("  PASS: tab_at_external_horizontal\n");
}

static void
test_tab_at_external_vertical(void)
{
	setup();
	test_config.tabs_intitlebar = false;
	test_config.tab_padding = 0;

	struct wm_decoration *deco = make_test_decoration(WM_DECOR_NORMAL, 400, 300);
	deco->tab_bar_size = 30;
	deco->tab_bar_placement = WM_TAB_BAR_LEFT;
	deco->tab_bar_box = (struct wlr_box){1, 21, 30, 300};

	struct wm_view view2 = {0};
	view2.server = &test_server;
	wl_list_init(&view2.tab_link);
	struct wm_view view3 = {0};
	view3.server = &test_server;
	wl_list_init(&view3.tab_link);

	struct wm_tab_group tg;
	wl_list_init(&tg.views);
	tg.count = 3;
	tg.active_view = &test_view;
	wl_list_insert(&tg.views, &view3.tab_link);
	wl_list_insert(&tg.views, &view2.tab_link);
	wl_list_insert(&tg.views, &test_view.tab_link);
	test_view.tab_group = &tg;

	/* Vertical: 3 tabs, padding=0, seg_h=300/3=100
	 * Tab 0: ly [0, 100)
	 * Tab 1: ly [100, 200)
	 * Tab 2: ly [200, 300) */
	int idx = wm_decoration_tab_at(deco, 15, 21 + 50);
	assert(idx == 0);

	idx = wm_decoration_tab_at(deco, 15, 21 + 150);
	assert(idx == 1);

	idx = wm_decoration_tab_at(deco, 15, 21 + 250);
	assert(idx == 2);

	test_view.tab_group = NULL;
	free_test_decoration(deco);
	printf("  PASS: tab_at_external_vertical\n");
}

/* --- get_extents with external tab bar --- */

static void
test_get_extents_ext_tab_bar_top(void)
{
	setup();
	struct wm_decoration *deco = make_test_decoration(WM_DECOR_NORMAL, 800, 600);
	deco->tab_bar_size = 25;
	deco->tab_bar_placement = WM_TAB_BAR_TOP;

	int top, bottom, left, right;
	wm_decoration_get_extents(deco, &top, &bottom, &left, &right);

	/* NORMAL + top tab bar: top = bw(1) + th(20) + tab(25) = 46 */
	assert(top == 46);
	assert(bottom == 7);
	assert(left == 1);
	assert(right == 1);

	free_test_decoration(deco);
	printf("  PASS: get_extents_ext_tab_bar_top\n");
}

static void
test_get_extents_ext_tab_bar_left(void)
{
	setup();
	struct wm_decoration *deco = make_test_decoration(WM_DECOR_NORMAL, 800, 600);
	deco->tab_bar_size = 30;
	deco->tab_bar_placement = WM_TAB_BAR_LEFT;

	int top, bottom, left, right;
	wm_decoration_get_extents(deco, &top, &bottom, &left, &right);

	assert(top == 21);
	assert(bottom == 7);
	assert(left == 31); /* bw(1) + tab(30) */
	assert(right == 1);

	free_test_decoration(deco);
	printf("  PASS: get_extents_ext_tab_bar_left\n");
}

static void
test_get_extents_ext_tab_bar_right(void)
{
	setup();
	struct wm_decoration *deco = make_test_decoration(WM_DECOR_NORMAL, 800, 600);
	deco->tab_bar_size = 30;
	deco->tab_bar_placement = WM_TAB_BAR_RIGHT;

	int top, bottom, left, right;
	wm_decoration_get_extents(deco, &top, &bottom, &left, &right);

	assert(left == 1);
	assert(right == 31); /* bw(1) + tab(30) */

	free_test_decoration(deco);
	printf("  PASS: get_extents_ext_tab_bar_right\n");
}

static void
test_get_extents_ext_tab_bar_bottom(void)
{
	setup();
	struct wm_decoration *deco = make_test_decoration(WM_DECOR_NORMAL, 800, 600);
	deco->tab_bar_size = 25;
	deco->tab_bar_placement = WM_TAB_BAR_BOTTOM;

	int top, bottom, left, right;
	wm_decoration_get_extents(deco, &top, &bottom, &left, &right);

	assert(top == 21);
	assert(bottom == 32); /* bw(1) + hh(6) + tab(25) */

	free_test_decoration(deco);
	printf("  PASS: get_extents_ext_tab_bar_bottom\n");
}

static void
test_get_extents_null_params(void)
{
	setup();
	struct wm_decoration *deco = make_test_decoration(WM_DECOR_NORMAL, 800, 600);

	/* Should not crash with NULL output pointers */
	wm_decoration_get_extents(deco, NULL, NULL, NULL, NULL);

	int top;
	wm_decoration_get_extents(deco, &top, NULL, NULL, NULL);
	assert(top == 21);

	free_test_decoration(deco);
	printf("  PASS: get_extents_null_params\n");
}

/* --- configure_buttons_replace --- */

static void
test_configure_buttons_replace(void)
{
	setup();
	struct wm_decoration *deco = make_test_decoration(WM_DECOR_NORMAL, 800, 600);

	wm_decoration_configure_buttons(deco, "Close", "Maximize");
	assert(deco->buttons_left_count == 1);
	assert(deco->buttons_right_count == 1);

	/* Replace with different layout */
	wm_decoration_configure_buttons(deco, "Stick Shade", "Close");
	assert(deco->buttons_left_count == 2);
	assert(deco->buttons_right_count == 1);
	assert(deco->buttons_left[0].type == WM_BUTTON_STICK);
	assert(deco->buttons_left[1].type == WM_BUTTON_SHADE);

	free_test_decoration(deco);
	printf("  PASS: configure_buttons_replace\n");
}

static void
test_configure_buttons_empty(void)
{
	setup();
	struct wm_decoration *deco = make_test_decoration(WM_DECOR_NORMAL, 800, 600);

	wm_decoration_configure_buttons(deco, "", "");
	assert(deco->buttons_left_count == 0);
	assert(deco->buttons_left == NULL);
	assert(deco->buttons_right_count == 0);
	assert(deco->buttons_right == NULL);

	free_test_decoration(deco);
	printf("  PASS: configure_buttons_empty\n");
}

/* --- region_at with button --- */

static void
test_region_at_button(void)
{
	setup();
	struct wm_decoration *deco = make_test_decoration(WM_DECOR_NORMAL, 800, 600);

	free(deco->buttons_left);
	deco->buttons_left = calloc(1, sizeof(struct wm_decor_button));
	deco->buttons_left_count = 1;
	deco->buttons_left[0].type = WM_BUTTON_CLOSE;
	deco->buttons_left[0].box = (struct wlr_box){5, 5, 12, 12};

	/* Hit inside button */
	enum wm_decor_region region = wm_decoration_region_at(deco, 10, 10);
	assert(region == WM_DECOR_REGION_BUTTON);

	free_test_decoration(deco);
	printf("  PASS: region_at_button\n");
}

/* --- region_at with NONE preset --- */

static void
test_region_at_none_preset(void)
{
	setup();
	struct wm_decoration *deco = make_test_decoration(WM_DECOR_NONE, 800, 600);

	/* NONE: no titlebar, no handle.
	 * outer_h = 0 + 600 + 0 + 2*1 = 602, outer_w = 802
	 * Interior should be CLIENT */
	enum wm_decor_region region = wm_decoration_region_at(deco, 400, 300);
	assert(region == WM_DECOR_REGION_CLIENT);

	/* Border still present */
	region = wm_decoration_region_at(deco, 400, 0);
	assert(region == WM_DECOR_REGION_BORDER_TOP);

	free_test_decoration(deco);
	printf("  PASS: region_at_none_preset\n");
}

/* --- style helpers: additional --- */

static void
test_style_titlebar_height_fallback(void)
{
	setup();
	test_style.window_title_height = 0;
	test_style.window_label_focus_font.size = 0;

	/* Fallback: 12 + 8 = 20 */
	int h = style_titlebar_height(&test_style);
	assert(h == 20);
	printf("  PASS: style_titlebar_height_fallback\n");
}

static void
test_style_handle_height_explicit(void)
{
	setup();
	test_style.window_handle_width = 10;

	int h = style_handle_height(&test_style);
	assert(h == 10);
	printf("  PASS: style_handle_height_explicit\n");
}

static void
test_style_border_width_zero(void)
{
	setup();
	test_style.window_border_width = 0;

	/* 0 is >= 0, so returns 0 */
	int w = style_border_width(&test_style);
	assert(w == 0);
	printf("  PASS: style_border_width_zero\n");
}

static void
test_style_border_width_explicit(void)
{
	setup();
	test_style.window_border_width = 3;

	int w = style_border_width(&test_style);
	assert(w == 3);
	printf("  PASS: style_border_width_explicit\n");
}

/* --- decoration_create / destroy full path --- */

static void
test_decoration_create_and_destroy(void)
{
	setup();
	setup_view();

	struct wm_decoration *deco = wm_decoration_create(&test_view,
		&test_style);
	assert(deco != NULL);
	assert(deco->view == &test_view);
	assert(deco->server == &test_server);
	assert(deco->focused == true);
	assert(deco->shaded == false);
	assert(deco->preset == WM_DECOR_NORMAL);
	assert(deco->titlebar_height == 20);
	assert(deco->handle_height == 6);
	assert(deco->border_width == 1);
	assert(deco->content_width == 800);
	assert(deco->content_height == 600);
	assert(deco->tree != NULL);
	assert(deco->titlebar_tree != NULL);

	wm_decoration_destroy(deco);
	printf("  PASS: decoration_create_and_destroy\n");
}

static void
test_decoration_destroy_null(void)
{
	/* Should not crash */
	wm_decoration_destroy(NULL);
	printf("  PASS: decoration_destroy_null\n");
}

/* --- decoration_update --- */

static void
test_decoration_update_null_decoration(void)
{
	setup();
	/* Should not crash */
	wm_decoration_update(NULL, &test_style);
	printf("  PASS: decoration_update_null_decoration\n");
}

static void
test_decoration_update_null_style(void)
{
	setup();
	setup_view();
	struct wm_decoration *deco = wm_decoration_create(&test_view,
		&test_style);
	assert(deco != NULL);

	/* Should not crash */
	wm_decoration_update(deco, NULL);

	wm_decoration_destroy(deco);
	printf("  PASS: decoration_update_null_style\n");
}

static void
test_decoration_update_recalculates_dimensions(void)
{
	setup();
	setup_view();
	struct wm_decoration *deco = wm_decoration_create(&test_view,
		&test_style);
	assert(deco != NULL);
	assert(deco->titlebar_height == 20);

	/* Change style and update */
	test_style.window_title_height = 30;
	test_style.window_handle_width = 10;
	test_style.window_border_width = 2;
	wm_decoration_update(deco, &test_style);

	assert(deco->titlebar_height == 30);
	assert(deco->handle_height == 10);
	assert(deco->border_width == 2);

	wm_decoration_destroy(deco);
	printf("  PASS: decoration_update_recalculates_dimensions\n");
}

/* --- decoration_set_focused --- */

static void
test_set_focused_changes_state(void)
{
	setup();
	setup_view();
	struct wm_decoration *deco = wm_decoration_create(&test_view,
		&test_style);
	assert(deco != NULL);
	assert(deco->focused == true);

	wm_decoration_set_focused(deco, false, &test_style);
	assert(deco->focused == false);

	wm_decoration_set_focused(deco, true, &test_style);
	assert(deco->focused == true);

	wm_decoration_destroy(deco);
	printf("  PASS: set_focused_changes_state\n");
}

static void
test_set_focused_noop_same_state(void)
{
	setup();
	setup_view();
	struct wm_decoration *deco = wm_decoration_create(&test_view,
		&test_style);
	assert(deco != NULL);
	assert(deco->focused == true);

	/* Setting to same state should be a no-op (no crash, no change) */
	wm_decoration_set_focused(deco, true, &test_style);
	assert(deco->focused == true);

	wm_decoration_destroy(deco);
	printf("  PASS: set_focused_noop_same_state\n");
}

static void
test_set_focused_null(void)
{
	setup();
	/* Should not crash */
	wm_decoration_set_focused(NULL, true, &test_style);
	printf("  PASS: set_focused_null\n");
}

/* --- decoration_set_shaded --- */

static void
test_set_shaded_saves_geometry(void)
{
	setup();
	setup_view();
	struct wm_decoration *deco = wm_decoration_create(&test_view,
		&test_style);
	assert(deco != NULL);
	assert(deco->shaded == false);
	assert(deco->content_width == 800);
	assert(deco->content_height == 600);

	wm_decoration_set_shaded(deco, true, &test_style);
	assert(deco->shaded == true);
	/* Geometry should be saved */
	assert(deco->shaded_saved_geometry.width == 800);
	assert(deco->shaded_saved_geometry.height == 600);

	wm_decoration_destroy(deco);
	printf("  PASS: set_shaded_saves_geometry\n");
}

static void
test_set_shaded_unshade_restores(void)
{
	setup();
	setup_view();
	struct wm_decoration *deco = wm_decoration_create(&test_view,
		&test_style);
	assert(deco != NULL);

	wm_decoration_set_shaded(deco, true, &test_style);
	assert(deco->shaded == true);

	/* Unshade should restore geometry */
	wm_decoration_set_shaded(deco, false, &test_style);
	assert(deco->shaded == false);
	assert(deco->content_width == 800);
	assert(deco->content_height == 600);

	wm_decoration_destroy(deco);
	printf("  PASS: set_shaded_unshade_restores\n");
}

static void
test_set_shaded_noop_same_state(void)
{
	setup();
	setup_view();
	struct wm_decoration *deco = wm_decoration_create(&test_view,
		&test_style);
	assert(deco != NULL);

	/* Shading when already not shaded is a no-op */
	wm_decoration_set_shaded(deco, false, &test_style);
	assert(deco->shaded == false);

	wm_decoration_destroy(deco);
	printf("  PASS: set_shaded_noop_same_state\n");
}

static void
test_set_shaded_null(void)
{
	/* Should not crash */
	wm_decoration_set_shaded(NULL, true, &test_style);
	printf("  PASS: set_shaded_null\n");
}

/* --- decoration_set_preset --- */

static void
test_set_preset_transitions(void)
{
	setup();
	setup_view();
	struct wm_decoration *deco = wm_decoration_create(&test_view,
		&test_style);
	assert(deco != NULL);
	assert(deco->preset == WM_DECOR_NORMAL);

	/* Transition to NONE */
	wm_decoration_set_preset(deco, WM_DECOR_NONE, &test_style);
	assert(deco->preset == WM_DECOR_NONE);

	/* Transition to BORDER */
	wm_decoration_set_preset(deco, WM_DECOR_BORDER, &test_style);
	assert(deco->preset == WM_DECOR_BORDER);

	/* Transition to TAB */
	wm_decoration_set_preset(deco, WM_DECOR_TAB, &test_style);
	assert(deco->preset == WM_DECOR_TAB);

	/* Transition to TINY */
	wm_decoration_set_preset(deco, WM_DECOR_TINY, &test_style);
	assert(deco->preset == WM_DECOR_TINY);

	/* Transition to TOOL */
	wm_decoration_set_preset(deco, WM_DECOR_TOOL, &test_style);
	assert(deco->preset == WM_DECOR_TOOL);

	/* Back to NORMAL */
	wm_decoration_set_preset(deco, WM_DECOR_NORMAL, &test_style);
	assert(deco->preset == WM_DECOR_NORMAL);

	wm_decoration_destroy(deco);
	printf("  PASS: set_preset_transitions\n");
}

static void
test_set_preset_noop_same(void)
{
	setup();
	setup_view();
	struct wm_decoration *deco = wm_decoration_create(&test_view,
		&test_style);
	assert(deco != NULL);
	assert(deco->preset == WM_DECOR_NORMAL);

	/* Same preset is a no-op */
	wm_decoration_set_preset(deco, WM_DECOR_NORMAL, &test_style);
	assert(deco->preset == WM_DECOR_NORMAL);

	wm_decoration_destroy(deco);
	printf("  PASS: set_preset_noop_same\n");
}

static void
test_set_preset_null(void)
{
	setup();
	/* Should not crash */
	wm_decoration_set_preset(NULL, WM_DECOR_NONE, &test_style);
	printf("  PASS: set_preset_null\n");
}

/* --- region_at with TOOL preset --- */

static void
test_region_at_tool_has_titlebar(void)
{
	setup();
	struct wm_decoration *deco = make_test_decoration(WM_DECOR_TOOL, 800, 600);

	/* TOOL has titlebar but no handle */
	enum wm_decor_region region = wm_decoration_region_at(deco, 400, 10);
	assert(region == WM_DECOR_REGION_TITLEBAR);

	/* outer_h for TOOL: bw(1) + th(20) + ch(600) + bw(1) = 622 */
	/* y=623 is outside */
	region = wm_decoration_region_at(deco, 400, 623);
	assert(region == WM_DECOR_REGION_NONE);

	free_test_decoration(deco);
	printf("  PASS: region_at_tool_has_titlebar\n");
}

/* --- region_at with external tab bar region --- */

static void
test_region_at_external_tab_bar(void)
{
	setup();
	struct wm_decoration *deco = make_test_decoration(WM_DECOR_NORMAL, 800, 600);
	deco->tab_bar_size = 25;
	deco->tab_bar_placement = WM_TAB_BAR_TOP;
	deco->tab_bar_box = (struct wlr_box){1, 21, 800, 25};

	/* Click in the external tab bar area => should be TITLEBAR */
	enum wm_decor_region region = wm_decoration_region_at(deco, 400, 30);
	assert(region == WM_DECOR_REGION_TITLEBAR);

	free_test_decoration(deco);
	printf("  PASS: region_at_external_tab_bar\n");
}

/* --- button_at edge boundary --- */

static void
test_button_at_edge_boundary(void)
{
	setup();
	struct wm_decoration *deco = make_test_decoration(WM_DECOR_NORMAL, 800, 600);

	free(deco->buttons_left);
	deco->buttons_left = calloc(1, sizeof(struct wm_decor_button));
	deco->buttons_left_count = 1;
	deco->buttons_left[0].type = WM_BUTTON_CLOSE;
	deco->buttons_left[0].box = (struct wlr_box){10, 5, 12, 12};

	/* Exact left edge of button - should hit */
	struct wm_decor_button *btn = wm_decoration_button_at(deco, 10, 10);
	assert(btn != NULL);

	/* Exact top edge - should hit */
	btn = wm_decoration_button_at(deco, 15, 5);
	assert(btn != NULL);

	/* One pixel past right edge (10+12=22) - should miss */
	btn = wm_decoration_button_at(deco, 22, 10);
	assert(btn == NULL);

	/* One pixel past bottom edge (5+12=17) - should miss */
	btn = wm_decoration_button_at(deco, 15, 17);
	assert(btn == NULL);

	/* One pixel before left edge - should miss */
	btn = wm_decoration_button_at(deco, 9, 10);
	assert(btn == NULL);

	free_test_decoration(deco);
	printf("  PASS: button_at_edge_boundary\n");
}

/* --- button_at with multiple left and right buttons --- */

static void
test_button_at_multiple_buttons(void)
{
	setup();
	struct wm_decoration *deco = make_test_decoration(WM_DECOR_NORMAL, 800, 600);

	free(deco->buttons_left);
	deco->buttons_left = calloc(2, sizeof(struct wm_decor_button));
	deco->buttons_left_count = 2;
	deco->buttons_left[0].type = WM_BUTTON_STICK;
	deco->buttons_left[0].box = (struct wlr_box){5, 5, 12, 12};
	deco->buttons_left[1].type = WM_BUTTON_SHADE;
	deco->buttons_left[1].box = (struct wlr_box){21, 5, 12, 12};

	free(deco->buttons_right);
	deco->buttons_right = calloc(2, sizeof(struct wm_decor_button));
	deco->buttons_right_count = 2;
	deco->buttons_right[0].type = WM_BUTTON_MAXIMIZE;
	deco->buttons_right[0].box = (struct wlr_box){760, 5, 12, 12};
	deco->buttons_right[1].type = WM_BUTTON_CLOSE;
	deco->buttons_right[1].box = (struct wlr_box){776, 5, 12, 12};

	/* Hit second left button */
	struct wm_decor_button *btn = wm_decoration_button_at(deco, 25, 10);
	assert(btn != NULL);
	assert(btn->type == WM_BUTTON_SHADE);

	/* Hit first right button */
	btn = wm_decoration_button_at(deco, 765, 10);
	assert(btn != NULL);
	assert(btn->type == WM_BUTTON_MAXIMIZE);

	/* Hit second right button */
	btn = wm_decoration_button_at(deco, 780, 10);
	assert(btn != NULL);
	assert(btn->type == WM_BUTTON_CLOSE);

	free_test_decoration(deco);
	printf("  PASS: button_at_multiple_buttons\n");
}

/* --- get_extents with zero border width --- */

static void
test_get_extents_zero_border(void)
{
	setup();
	struct wm_decoration *deco = make_test_decoration(WM_DECOR_NORMAL, 800, 600);
	deco->border_width = 0;

	int top, bottom, left, right;
	wm_decoration_get_extents(deco, &top, &bottom, &left, &right);

	/* NORMAL with bw=0: top = 0 + th(20) = 20, bottom = 0 + hh(6) = 6 */
	assert(top == 20);
	assert(bottom == 6);
	assert(left == 0);
	assert(right == 0);

	free_test_decoration(deco);
	printf("  PASS: get_extents_zero_border\n");
}

/* --- get_extents with large border width --- */

static void
test_get_extents_large_border(void)
{
	setup();
	struct wm_decoration *deco = make_test_decoration(WM_DECOR_NORMAL, 800, 600);
	deco->border_width = 10;

	int top, bottom, left, right;
	wm_decoration_get_extents(deco, &top, &bottom, &left, &right);

	/* NORMAL with bw=10: top = 10+20=30, bottom = 10+6=16 */
	assert(top == 30);
	assert(bottom == 16);
	assert(left == 10);
	assert(right == 10);

	free_test_decoration(deco);
	printf("  PASS: get_extents_large_border\n");
}

/* --- region_at with zero border width --- */

static void
test_region_at_zero_border(void)
{
	setup();
	struct wm_decoration *deco = make_test_decoration(WM_DECOR_NORMAL, 800, 600);
	deco->border_width = 0;
	deco->content_area.x = 0;
	deco->content_area.y = 20;

	/* With bw=0, there are no border regions; y=0 is titlebar */
	enum wm_decor_region region = wm_decoration_region_at(deco, 400, 0);
	assert(region == WM_DECOR_REGION_TITLEBAR);

	/* Client area */
	region = wm_decoration_region_at(deco, 400, 300);
	assert(region == WM_DECOR_REGION_CLIENT);

	free_test_decoration(deco);
	printf("  PASS: region_at_zero_border\n");
}

/* --- decoration_create with custom config buttons --- */

static void
test_decoration_create_custom_buttons(void)
{
	setup();
	setup_view();
	test_config.titlebar_left = "Close Shade";
	test_config.titlebar_right = "Minimize";

	struct wm_decoration *deco = wm_decoration_create(&test_view,
		&test_style);
	assert(deco != NULL);
	assert(deco->buttons_left_count == 2);
	assert(deco->buttons_left[0].type == WM_BUTTON_CLOSE);
	assert(deco->buttons_left[1].type == WM_BUTTON_SHADE);
	assert(deco->buttons_right_count == 1);
	assert(deco->buttons_right[0].type == WM_BUTTON_ICONIFY);

	wm_decoration_destroy(deco);
	/* Reset config */
	test_config.titlebar_left = NULL;
	test_config.titlebar_right = NULL;
	printf("  PASS: decoration_create_custom_buttons\n");
}

/* --- pixel_buffer_begin_data_ptr_access --- */

static void
test_pixel_buffer_write_rejected(void)
{
	setup();
	/* Test that wlr_buffer_from_cairo produces a buffer that rejects writes */
	cairo_surface_t *surface = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, 10, 10);
	struct wlr_buffer *buf = wlr_buffer_from_cairo(surface);
	assert(buf != NULL);

	/* The pixel_buffer should reject write access */
	void *data;
	uint32_t format;
	size_t stride;
	bool ok = pixel_buffer_begin_data_ptr_access(buf,
		WLR_BUFFER_DATA_PTR_ACCESS_WRITE, &data, &format, &stride);
	assert(ok == false);

	/* Read access should succeed */
	ok = pixel_buffer_begin_data_ptr_access(buf, 0, &data, &format, &stride);
	assert(ok == true);
	assert(format == DRM_FORMAT_ARGB8888);
	assert(data != NULL);

	pixel_buffer_end_data_ptr_access(buf);
	pixel_buffer_destroy(buf);
	printf("  PASS: pixel_buffer_write_rejected\n");
}

/* --- wlr_buffer_from_cairo null --- */

static void
test_wlr_buffer_from_cairo_null(void)
{
	struct wlr_buffer *buf = wlr_buffer_from_cairo(NULL);
	assert(buf == NULL);
	printf("  PASS: wlr_buffer_from_cairo_null\n");
}

/* --- layout_and_render dimension calculation via update with NORMAL --- */

static void
test_update_normal_content_area(void)
{
	setup();
	setup_view();
	struct wm_decoration *deco = wm_decoration_create(&test_view,
		&test_style);
	assert(deco != NULL);

	/* NORMAL: content_area.x = bw + ext_left = 1 + 0 = 1 */
	/* content_area.y = bw + top_height + ext_top = 1 + 20 + 0 = 21 */
	assert(deco->content_area.x == 1);
	assert(deco->content_area.y == 21);
	assert(deco->content_area.width == 800);
	assert(deco->content_area.height == 600);

	wm_decoration_destroy(deco);
	printf("  PASS: update_normal_content_area\n");
}

/* --- layout_and_render with TAB preset --- */

static void
test_update_tab_preset_content_area(void)
{
	setup();
	setup_view();
	struct wm_decoration *deco = wm_decoration_create(&test_view,
		&test_style);
	assert(deco != NULL);

	/* Switch to TAB preset: has titlebar, no handle */
	wm_decoration_set_preset(deco, WM_DECOR_TAB, &test_style);

	/* TAB: has_titlebar=true, has_handle=false, has_borders=false */
	/* content_area.x = bw = 1, content_area.y = bw + th = 21 */
	assert(deco->content_area.x == 1);
	assert(deco->content_area.y == 21);
	assert(deco->content_area.width == 800);
	assert(deco->content_area.height == 600);

	wm_decoration_destroy(deco);
	printf("  PASS: update_tab_preset_content_area\n");
}

/* --- layout_and_render with NONE preset --- */

static void
test_update_none_preset_disables_elements(void)
{
	setup();
	setup_view();
	struct wm_decoration *deco = wm_decoration_create(&test_view,
		&test_style);
	assert(deco != NULL);

	/* Switch to NONE: all disabled */
	wm_decoration_set_preset(deco, WM_DECOR_NONE, &test_style);

	/* Borders disabled */
	assert(deco->border_top->node.enabled == 0);
	assert(deco->border_bottom->node.enabled == 0);
	assert(deco->border_left->node.enabled == 0);
	assert(deco->border_right->node.enabled == 0);

	/* Titlebar disabled */
	assert(deco->titlebar_tree->node.enabled == 0);

	/* Handle and grips disabled */
	assert(deco->handle_buf->node.enabled == 0);
	assert(deco->grip_left->node.enabled == 0);
	assert(deco->grip_right->node.enabled == 0);

	wm_decoration_destroy(deco);
	printf("  PASS: update_none_preset_disables_elements\n");
}

/* --- layout_and_render with BORDER preset --- */

static void
test_update_border_preset_no_titlebar_no_handle(void)
{
	setup();
	setup_view();
	struct wm_decoration *deco = wm_decoration_create(&test_view,
		&test_style);
	assert(deco != NULL);

	wm_decoration_set_preset(deco, WM_DECOR_BORDER, &test_style);

	/* BORDER: has_borders=true, has_titlebar=false, has_handle=false */
	/* Borders should be enabled */
	assert(deco->border_top->node.enabled == 1);
	assert(deco->border_bottom->node.enabled == 1);
	assert(deco->border_left->node.enabled == 1);
	assert(deco->border_right->node.enabled == 1);

	/* Titlebar should be disabled */
	assert(deco->titlebar_tree->node.enabled == 0);

	/* Handle/grips should be disabled */
	assert(deco->handle_buf->node.enabled == 0);
	assert(deco->grip_left->node.enabled == 0);
	assert(deco->grip_right->node.enabled == 0);

	/* content_area: x = bw=1, y = bw + 0 = 1 (no titlebar) */
	assert(deco->content_area.x == 1);
	assert(deco->content_area.y == 1);

	wm_decoration_destroy(deco);
	printf("  PASS: update_border_preset_no_titlebar_no_handle\n");
}

/* --- layout_and_render with TINY preset --- */

static void
test_update_tiny_preset_has_titlebar_no_handle(void)
{
	setup();
	setup_view();
	struct wm_decoration *deco = wm_decoration_create(&test_view,
		&test_style);
	assert(deco != NULL);

	wm_decoration_set_preset(deco, WM_DECOR_TINY, &test_style);

	/* TINY: has_titlebar=true, has_handle=false, has_borders=false */
	assert(deco->titlebar_tree->node.enabled == 1);
	assert(deco->handle_buf->node.enabled == 0);
	assert(deco->grip_left->node.enabled == 0);
	assert(deco->grip_right->node.enabled == 0);

	wm_decoration_destroy(deco);
	printf("  PASS: update_tiny_preset_has_titlebar_no_handle\n");
}

/* --- layout_and_render NORMAL borders enabled with position check --- */

static void
test_update_normal_border_positions(void)
{
	setup();
	setup_view();
	struct wm_decoration *deco = wm_decoration_create(&test_view,
		&test_style);
	assert(deco != NULL);

	/* NORMAL: borders enabled, check positions */
	/* Top border at (0, 0), size outer_width x bw */
	assert(deco->border_top->node.enabled == 1);
	assert(deco->border_top->node.x == 0);
	assert(deco->border_top->node.y == 0);

	/* Left border at (0, 0) */
	assert(deco->border_left->node.enabled == 1);
	assert(deco->border_left->node.x == 0);
	assert(deco->border_left->node.y == 0);

	/* Right border at (outer_width - bw, 0) = (800+2-1, 0) = (801, 0) */
	assert(deco->border_right->node.enabled == 1);
	assert(deco->border_right->node.x == 801);
	assert(deco->border_right->node.y == 0);

	/* Bottom border at (0, total_h - bw) */
	/* total_h = th(20) + ch(600) + hh(6) + 2*bw(2) = 628 */
	assert(deco->border_bottom->node.enabled == 1);
	assert(deco->border_bottom->node.x == 0);
	assert(deco->border_bottom->node.y == 627);

	wm_decoration_destroy(deco);
	printf("  PASS: update_normal_border_positions\n");
}

/* --- layout_and_render border size checks --- */

static void
test_update_normal_border_sizes(void)
{
	setup();
	setup_view();
	test_style.window_border_width = 3;
	struct wm_decoration *deco = wm_decoration_create(&test_view,
		&test_style);
	assert(deco != NULL);

	/* outer_width = cw + 2*bw = 800 + 6 = 806 */
	/* total_h = th(20) + ch(600) + hh(6) + 2*bw(6) = 632 */
	/* Top border: 806 x 3 */
	assert(deco->border_top->width == 806);
	assert(deco->border_top->height == 3);

	/* Bottom border: 806 x 3 */
	assert(deco->border_bottom->width == 806);
	assert(deco->border_bottom->height == 3);

	/* Left border: 3 x 632 */
	assert(deco->border_left->width == 3);
	assert(deco->border_left->height == 632);

	/* Right border: 3 x 632 */
	assert(deco->border_right->width == 3);
	assert(deco->border_right->height == 632);

	wm_decoration_destroy(deco);
	printf("  PASS: update_normal_border_sizes\n");
}

/* --- wm_decoration_create with all buttons on one side --- */

static void
test_create_all_buttons_left(void)
{
	setup();
	setup_view();
	test_config.titlebar_left = "Close Maximize Minimize Shade Stick";
	test_config.titlebar_right = "";

	struct wm_decoration *deco = wm_decoration_create(&test_view,
		&test_style);
	assert(deco != NULL);
	assert(deco->buttons_left_count == 5);
	assert(deco->buttons_right_count == 0);
	assert(deco->buttons_right == NULL);

	assert(deco->buttons_left[0].type == WM_BUTTON_CLOSE);
	assert(deco->buttons_left[1].type == WM_BUTTON_MAXIMIZE);
	assert(deco->buttons_left[2].type == WM_BUTTON_ICONIFY);
	assert(deco->buttons_left[3].type == WM_BUTTON_SHADE);
	assert(deco->buttons_left[4].type == WM_BUTTON_STICK);

	wm_decoration_destroy(deco);
	test_config.titlebar_left = NULL;
	test_config.titlebar_right = NULL;
	printf("  PASS: create_all_buttons_left\n");
}

/* --- wm_decoration_create with no config (defaults) --- */

static void
test_create_no_config(void)
{
	setup();
	setup_view();
	test_server.config = NULL;

	struct wm_decoration *deco = wm_decoration_create(&test_view,
		&test_style);
	assert(deco != NULL);
	/* Default: left="Stick", right="Shade Minimize Maximize Close" */
	assert(deco->buttons_left_count == 1);
	assert(deco->buttons_left[0].type == WM_BUTTON_STICK);
	assert(deco->buttons_right_count == 4);
	assert(deco->buttons_right[0].type == WM_BUTTON_SHADE);
	assert(deco->buttons_right[3].type == WM_BUTTON_CLOSE);

	wm_decoration_destroy(deco);
	test_server.config = &test_config;
	printf("  PASS: create_no_config\n");
}

/* --- shade/unshade full cycle verifying geometry at each step --- */

static void
test_shade_unshade_full_cycle_geometry(void)
{
	setup();
	setup_view();
	struct wm_decoration *deco = wm_decoration_create(&test_view,
		&test_style);
	assert(deco != NULL);
	assert(deco->content_width == 800);
	assert(deco->content_height == 600);
	assert(deco->shaded == false);

	/* Shade: save geometry, disable handle/grips */
	wm_decoration_set_shaded(deco, true, &test_style);
	assert(deco->shaded == true);
	assert(deco->shaded_saved_geometry.width == 800);
	assert(deco->shaded_saved_geometry.height == 600);
	assert(deco->handle_buf->node.enabled == 0);
	assert(deco->grip_left->node.enabled == 0);
	assert(deco->grip_right->node.enabled == 0);

	/* Unshade: restore geometry, update layout */
	wm_decoration_set_shaded(deco, false, &test_style);
	assert(deco->shaded == false);
	assert(deco->content_width == 800);
	assert(deco->content_height == 600);
	/* Handle/grips should be re-enabled after update */
	assert(deco->handle_buf->node.enabled == 1);
	assert(deco->grip_left->node.enabled == 1);
	assert(deco->grip_right->node.enabled == 1);

	wm_decoration_destroy(deco);
	printf("  PASS: shade_unshade_full_cycle_geometry\n");
}

/* --- shade/unshade double shade is no-op --- */

static void
test_shade_double_shade_noop(void)
{
	setup();
	setup_view();
	struct wm_decoration *deco = wm_decoration_create(&test_view,
		&test_style);
	assert(deco != NULL);

	wm_decoration_set_shaded(deco, true, &test_style);
	assert(deco->shaded == true);
	assert(deco->shaded_saved_geometry.width == 800);

	/* Shading again is a no-op */
	wm_decoration_set_shaded(deco, true, &test_style);
	assert(deco->shaded == true);
	assert(deco->shaded_saved_geometry.width == 800);

	wm_decoration_destroy(deco);
	printf("  PASS: shade_double_shade_noop\n");
}

/* --- update with round corners enables border frame --- */

static void
test_update_round_corners_border_frame(void)
{
	setup();
	setup_view();
	test_style.window_round_corners = WM_CORNER_TOP_LEFT | WM_CORNER_TOP_RIGHT;
	test_style.window_bevel_width = 4;

	struct wm_decoration *deco = wm_decoration_create(&test_view,
		&test_style);
	assert(deco != NULL);

	/* With round_corners set, border_frame should be enabled */
	assert(deco->border_frame != NULL);
	assert(deco->border_frame->node.enabled == 1);

	/* Individual border rects should be disabled */
	assert(deco->border_top->node.enabled == 0);
	assert(deco->border_bottom->node.enabled == 0);
	assert(deco->border_left->node.enabled == 0);
	assert(deco->border_right->node.enabled == 0);

	wm_decoration_destroy(deco);
	test_style.window_round_corners = 0;
	test_style.window_bevel_width = 0;
	printf("  PASS: update_round_corners_border_frame\n");
}

/* --- update without round corners disables border frame --- */

static void
test_update_no_round_corners_rect_borders(void)
{
	setup();
	setup_view();
	test_style.window_round_corners = 0;

	struct wm_decoration *deco = wm_decoration_create(&test_view,
		&test_style);
	assert(deco != NULL);

	/* No round corners: border_frame disabled, rect borders enabled */
	assert(deco->border_frame->node.enabled == 0);
	assert(deco->border_top->node.enabled == 1);
	assert(deco->border_bottom->node.enabled == 1);
	assert(deco->border_left->node.enabled == 1);
	assert(deco->border_right->node.enabled == 1);

	wm_decoration_destroy(deco);
	printf("  PASS: update_no_round_corners_rect_borders\n");
}

/* --- update with external tab bar (top placement) --- */

static void
test_update_ext_tab_bar_top_content_area(void)
{
	setup();
	setup_view();
	test_config.tabs_intitlebar = false;
	test_config.tab_width = 25;
	test_config.tab_placement = WM_TAB_BAR_TOP;

	/* Create a tab group with 2 views */
	struct wm_view view2 = {0};
	view2.server = &test_server;
	view2.title = "Tab 2";
	wl_list_init(&view2.tab_link);

	struct wm_tab_group tg;
	wl_list_init(&tg.views);
	tg.count = 2;
	tg.active_view = &test_view;
	tg.server = &test_server;

	setup_view();
	wl_list_insert(&tg.views, &view2.tab_link);
	wl_list_insert(&tg.views, &test_view.tab_link);
	test_view.tab_group = &tg;

	struct wm_decoration *deco = wm_decoration_create(&test_view,
		&test_style);
	assert(deco != NULL);

	/* With ext tab bar top (size=25):
	 * content_area.y = bw(1) + th(20) + ext_top(25) = 46 */
	assert(deco->tab_bar_size == 25);
	assert(deco->content_area.y == 46);
	assert(deco->content_area.x == 1);

	/* Tab bar buf should be enabled */
	assert(deco->tab_bar_buf->node.enabled == 1);

	test_view.tab_group = NULL;
	wm_decoration_destroy(deco);
	test_config.tabs_intitlebar = true;
	test_config.tab_width = 0;
	test_config.tab_placement = 0;
	printf("  PASS: update_ext_tab_bar_top_content_area\n");
}

/* --- update with external tab bar (left placement) --- */

static void
test_update_ext_tab_bar_left_content_area(void)
{
	setup();
	setup_view();
	test_config.tabs_intitlebar = false;
	test_config.tab_width = 30;
	test_config.tab_placement = WM_TAB_BAR_LEFT;

	struct wm_view view2 = {0};
	view2.server = &test_server;
	view2.title = "Tab 2";
	wl_list_init(&view2.tab_link);

	struct wm_tab_group tg;
	wl_list_init(&tg.views);
	tg.count = 2;
	tg.active_view = &test_view;
	tg.server = &test_server;

	setup_view();
	wl_list_insert(&tg.views, &view2.tab_link);
	wl_list_insert(&tg.views, &test_view.tab_link);
	test_view.tab_group = &tg;

	struct wm_decoration *deco = wm_decoration_create(&test_view,
		&test_style);
	assert(deco != NULL);

	/* With ext tab bar left (size=30):
	 * content_area.x = bw(1) + ext_left(30) = 31 */
	assert(deco->tab_bar_size == 30);
	assert(deco->content_area.x == 31);
	assert(deco->content_area.y == 21); /* bw + th + 0 (no ext_top) */

	test_view.tab_group = NULL;
	wm_decoration_destroy(deco);
	test_config.tabs_intitlebar = true;
	test_config.tab_width = 0;
	test_config.tab_placement = 0;
	printf("  PASS: update_ext_tab_bar_left_content_area\n");
}

/* --- focus border enabled when focused and width > 0 --- */

static void
test_update_focus_border_enabled(void)
{
	setup();
	setup_view();
	test_style.window_focus_border_width = 3;
	test_style.window_focus_border_color = (struct wm_color){255, 0, 0, 255};

	struct wm_decoration *deco = wm_decoration_create(&test_view,
		&test_style);
	assert(deco != NULL);
	assert(deco->focused == true);

	/* Focus border should be enabled (focused + width > 0 + has_titlebar) */
	assert(deco->focus_border_buf != NULL);
	assert(deco->focus_border_buf->node.enabled == 1);

	/* Position should be at (-fbw, -fbw) = (-3, -3) */
	assert(deco->focus_border_buf->node.x == -3);
	assert(deco->focus_border_buf->node.y == -3);

	wm_decoration_destroy(deco);
	test_style.window_focus_border_width = 0;
	printf("  PASS: update_focus_border_enabled\n");
}

/* --- focus border disabled when unfocused --- */

static void
test_update_focus_border_disabled_unfocused(void)
{
	setup();
	setup_view();
	test_style.window_focus_border_width = 3;
	test_style.window_focus_border_color = (struct wm_color){255, 0, 0, 255};

	struct wm_decoration *deco = wm_decoration_create(&test_view,
		&test_style);
	assert(deco != NULL);
	assert(deco->focused == true);
	assert(deco->focus_border_buf->node.enabled == 1);

	/* Unfocus the decoration */
	wm_decoration_set_focused(deco, false, &test_style);
	assert(deco->focused == false);

	/* Focus border should now be disabled */
	assert(deco->focus_border_buf->node.enabled == 0);

	wm_decoration_destroy(deco);
	test_style.window_focus_border_width = 0;
	printf("  PASS: update_focus_border_disabled_unfocused\n");
}

/* --- focus border disabled when width is 0 --- */

static void
test_update_focus_border_disabled_zero_width(void)
{
	setup();
	setup_view();
	test_style.window_focus_border_width = 0;

	struct wm_decoration *deco = wm_decoration_create(&test_view,
		&test_style);
	assert(deco != NULL);
	assert(deco->focused == true);

	/* Focus border should be disabled (width == 0) */
	assert(deco->focus_border_buf->node.enabled == 0);

	wm_decoration_destroy(deco);
	printf("  PASS: update_focus_border_disabled_zero_width\n");
}

/* --- update with different style re-renders with new dimensions --- */

static void
test_update_style_change_dimensions(void)
{
	setup();
	setup_view();
	struct wm_decoration *deco = wm_decoration_create(&test_view,
		&test_style);
	assert(deco != NULL);
	assert(deco->titlebar_height == 20);
	assert(deco->handle_height == 6);
	assert(deco->border_width == 1);

	/* Change style */
	struct wm_style new_style = test_style;
	new_style.window_title_height = 40;
	new_style.window_handle_width = 12;
	new_style.window_border_width = 5;

	wm_decoration_update(deco, &new_style);
	assert(deco->titlebar_height == 40);
	assert(deco->handle_height == 12);
	assert(deco->border_width == 5);

	/* Content area should reflect new dimensions */
	/* content_area.x = bw(5), content_area.y = bw(5) + th(40) = 45 */
	assert(deco->content_area.x == 5);
	assert(deco->content_area.y == 45);

	wm_decoration_destroy(deco);
	printf("  PASS: update_style_change_dimensions\n");
}

/* --- handle and grip positions in NORMAL layout --- */

static void
test_update_normal_handle_grip_positions(void)
{
	setup();
	setup_view();
	struct wm_decoration *deco = wm_decoration_create(&test_view,
		&test_style);
	assert(deco != NULL);

	/* Handle y = bw(1) + th(20) + ext_top(0) + ch(600) + ext_bottom(0) = 621 */
	/* Handle x = bw(1) + gw(20) = 21 */
	assert(deco->handle_buf->node.enabled == 1);
	assert(deco->handle_buf->node.y == 621);
	assert(deco->handle_buf->node.x == 21);

	/* Left grip: x = bw(1), y = 621 */
	assert(deco->grip_left->node.enabled == 1);
	assert(deco->grip_left->node.x == 1);
	assert(deco->grip_left->node.y == 621);

	/* Right grip: x = bw(1) + inner_width(800) - gw(20) = 781, y = 621 */
	assert(deco->grip_right->node.enabled == 1);
	assert(deco->grip_right->node.x == 781);
	assert(deco->grip_right->node.y == 621);

	wm_decoration_destroy(deco);
	printf("  PASS: update_normal_handle_grip_positions\n");
}

/* --- titlebar tree position --- */

static void
test_update_titlebar_tree_position(void)
{
	setup();
	setup_view();
	struct wm_decoration *deco = wm_decoration_create(&test_view,
		&test_style);
	assert(deco != NULL);

	/* Titlebar tree at (bw, bw) = (1, 1) */
	assert(deco->titlebar_tree->node.x == 1);
	assert(deco->titlebar_tree->node.y == 1);

	wm_decoration_destroy(deco);
	printf("  PASS: update_titlebar_tree_position\n");
}

/* --- render_button direct tests --- */

static void
test_render_button_fallback_bg(void)
{
	struct wm_texture bg_tex = {0};
	struct wm_color pic_color = {255, 255, 255, 255};

	/* wm_render_texture returns NULL, so render_button uses fallback path */
	struct wlr_buffer *buf = render_button(&bg_tex, &pic_color,
		WM_BUTTON_CLOSE, DEFAULT_BUTTON_SIZE);
	assert(buf != NULL);
	wlr_buffer_drop(buf);
	printf("  PASS: render_button_fallback_bg\n");
}

static void
test_render_button_small_glyph(void)
{
	struct wm_texture bg_tex = {0};
	struct wm_color pic_color = {255, 0, 0, 255};

	/* size=6: glyph_size = 6-4 = 2, which is < 4, so glyph_size = size */
	struct wlr_buffer *buf = render_button(&bg_tex, &pic_color,
		WM_BUTTON_MAXIMIZE, 6);
	assert(buf != NULL);
	wlr_buffer_drop(buf);
	printf("  PASS: render_button_small_glyph\n");
}

/* --- render_label direct tests --- */

static void
test_render_label_center_justify(void)
{
	struct wm_texture bg = {0};
	struct wm_font font = {.size = 12};
	struct wm_color color = {255, 255, 255, 255};

	struct wlr_buffer *buf = render_label(&bg, "Test Title", &font,
		&color, WM_JUSTIFY_CENTER, 200, 20);
	assert(buf != NULL);
	wlr_buffer_drop(buf);
	printf("  PASS: render_label_center_justify\n");
}

static void
test_render_label_right_justify(void)
{
	struct wm_texture bg = {0};
	struct wm_font font = {.size = 12};
	struct wm_color color = {255, 255, 255, 255};

	struct wlr_buffer *buf = render_label(&bg, "Test Title", &font,
		&color, WM_JUSTIFY_RIGHT, 200, 20);
	assert(buf != NULL);
	wlr_buffer_drop(buf);
	printf("  PASS: render_label_right_justify\n");
}

static void
test_render_label_null_title(void)
{
	struct wm_texture bg = {0};
	struct wm_font font = {.size = 12};
	struct wm_color color = {255, 255, 255, 255};

	/* NULL title should skip text rendering */
	struct wlr_buffer *buf = render_label(&bg, NULL, &font,
		&color, WM_JUSTIFY_LEFT, 200, 20);
	assert(buf != NULL);
	wlr_buffer_drop(buf);
	printf("  PASS: render_label_null_title\n");
}

static void
test_render_label_empty_title(void)
{
	struct wm_texture bg = {0};
	struct wm_font font = {.size = 12};
	struct wm_color color = {255, 255, 255, 255};

	/* Empty string: title && *title is false */
	struct wlr_buffer *buf = render_label(&bg, "", &font,
		&color, WM_JUSTIFY_LEFT, 200, 20);
	assert(buf != NULL);
	wlr_buffer_drop(buf);
	printf("  PASS: render_label_empty_title\n");
}

/* --- render_rounded_border_frame direct tests --- */

static void
test_render_rounded_border_frame_basic(void)
{
	struct wm_color color = {128, 128, 128, 255};

	struct wlr_buffer *buf = render_rounded_border_frame(
		100, 100, 2, 8.0,
		WM_CORNER_TOP_LEFT | WM_CORNER_TOP_RIGHT, &color);
	assert(buf != NULL);
	wlr_buffer_drop(buf);
	printf("  PASS: render_rounded_border_frame_basic\n");
}

static void
test_render_rounded_border_frame_invalid(void)
{
	struct wm_color color = {128, 128, 128, 255};

	/* Zero width */
	assert(render_rounded_border_frame(0, 100, 2, 8.0, 0xF, &color) == NULL);
	/* Zero height */
	assert(render_rounded_border_frame(100, 0, 2, 8.0, 0xF, &color) == NULL);
	/* Zero border width */
	assert(render_rounded_border_frame(100, 100, 0, 8.0, 0xF, &color) == NULL);
	/* Negative width */
	assert(render_rounded_border_frame(-1, 100, 2, 8.0, 0xF, &color) == NULL);
	printf("  PASS: render_rounded_border_frame_invalid\n");
}

/* --- set_shaded with scene tree children --- */

static void
test_set_shaded_with_scene_children(void)
{
	setup();
	setup_view();

	/* Add a child scene tree to simulate the client surface */
	struct wlr_scene_tree child_tree;
	memset(&child_tree, 0, sizeof(child_tree));
	child_tree.node.type = WLR_SCENE_NODE_TREE;
	child_tree.node.enabled = 1;
	wl_list_init(&child_tree.children);
	wl_list_init(&child_tree.node.link);
	wl_list_insert(&test_scene_tree.children, &child_tree.node.link);

	struct wm_decoration *deco = wm_decoration_create(&test_view,
		&test_style);
	assert(deco != NULL);

	/* Shade: non-decoration children should be disabled */
	wm_decoration_set_shaded(deco, true, &test_style);
	assert(child_tree.node.enabled == 0);

	/* Unshade: children should be re-enabled */
	wm_decoration_set_shaded(deco, false, &test_style);
	assert(child_tree.node.enabled == 1);

	wl_list_remove(&child_tree.node.link);
	wm_decoration_destroy(deco);
	printf("  PASS: set_shaded_with_scene_children\n");
}

/* --- ext tab bar bottom placement --- */

static void
test_update_ext_tab_bar_bottom(void)
{
	setup();
	setup_view();
	test_config.tabs_intitlebar = false;
	test_config.tab_width = 25;
	test_config.tab_placement = WM_TAB_BAR_BOTTOM;

	struct wm_view view2 = {0};
	view2.server = &test_server;
	view2.title = "Tab 2";
	wl_list_init(&view2.tab_link);

	struct wm_tab_group tg;
	wl_list_init(&tg.views);
	tg.count = 2;
	tg.active_view = &test_view;
	tg.server = &test_server;

	setup_view();
	wl_list_insert(&tg.views, &view2.tab_link);
	wl_list_insert(&tg.views, &test_view.tab_link);
	test_view.tab_group = &tg;

	struct wm_decoration *deco = wm_decoration_create(&test_view,
		&test_style);
	assert(deco != NULL);

	assert(deco->tab_bar_size == 25);
	assert(deco->tab_bar_placement == WM_TAB_BAR_BOTTOM);
	assert(deco->tab_bar_buf->node.enabled == 1);

	/* Bottom tab bar: content_area.y unchanged, but extents add tab bar */
	int top, bottom, left, right;
	wm_decoration_get_extents(deco, &top, &bottom, &left, &right);
	assert(bottom == 1 + 6 + 25); /* bw + hh + tab_bar */

	test_view.tab_group = NULL;
	wm_decoration_destroy(deco);
	test_config.tabs_intitlebar = true;
	test_config.tab_width = 0;
	test_config.tab_placement = 0;
	printf("  PASS: update_ext_tab_bar_bottom\n");
}

/* --- ext tab bar right placement --- */

static void
test_update_ext_tab_bar_right(void)
{
	setup();
	setup_view();
	test_config.tabs_intitlebar = false;
	test_config.tab_width = 30;
	test_config.tab_placement = WM_TAB_BAR_RIGHT;

	struct wm_view view2 = {0};
	view2.server = &test_server;
	view2.title = "Tab 2";
	wl_list_init(&view2.tab_link);

	struct wm_tab_group tg;
	wl_list_init(&tg.views);
	tg.count = 2;
	tg.active_view = &test_view;
	tg.server = &test_server;

	setup_view();
	wl_list_insert(&tg.views, &view2.tab_link);
	wl_list_insert(&tg.views, &test_view.tab_link);
	test_view.tab_group = &tg;

	struct wm_decoration *deco = wm_decoration_create(&test_view,
		&test_style);
	assert(deco != NULL);

	assert(deco->tab_bar_size == 30);
	assert(deco->tab_bar_placement == WM_TAB_BAR_RIGHT);
	assert(deco->tab_bar_buf->node.enabled == 1);

	/* Content area x unchanged, right extent has tab bar */
	int top, bottom, left, right;
	wm_decoration_get_extents(deco, &top, &bottom, &left, &right);
	assert(right == 1 + 30); /* bw + tab_bar */
	assert(left == 1); /* just bw */

	test_view.tab_group = NULL;
	wm_decoration_destroy(deco);
	test_config.tabs_intitlebar = true;
	test_config.tab_width = 0;
	test_config.tab_placement = 0;
	printf("  PASS: update_ext_tab_bar_right\n");
}

/* --- in-titlebar composite tab label rendering --- */

static void
test_update_intitlebar_tabs_composite(void)
{
	setup();
	setup_view();
	test_config.tabs_intitlebar = true;

	struct wm_view view2 = {0};
	view2.server = &test_server;
	view2.title = "Second Tab";
	wl_list_init(&view2.tab_link);
	struct wm_view view3 = {0};
	view3.server = &test_server;
	view3.title = "Third Tab";
	wl_list_init(&view3.tab_link);

	struct wm_tab_group tg;
	wl_list_init(&tg.views);
	tg.count = 3;
	tg.active_view = &test_view;
	tg.server = &test_server;

	setup_view();
	wl_list_insert(&tg.views, &view3.tab_link);
	wl_list_insert(&tg.views, &view2.tab_link);
	wl_list_insert(&tg.views, &test_view.tab_link);
	test_view.tab_group = &tg;

	struct wm_decoration *deco = wm_decoration_create(&test_view,
		&test_style);
	assert(deco != NULL);

	/* In-titlebar tabs: tab_bar_size should be 0 (no external bar) */
	assert(deco->tab_bar_size == 0);
	/* Titlebar should be enabled with composite label */
	assert(deco->titlebar_tree->node.enabled == 1);
	/* Label buf should have content */
	assert(deco->label_buf != NULL);

	test_view.tab_group = NULL;
	wm_decoration_destroy(deco);
	printf("  PASS: update_intitlebar_tabs_composite\n");
}

/* --- xdg decoration init (error path) --- */

static void
test_xdg_decoration_init_error(void)
{
	setup();
	/* wlr_xdg_decoration_manager_v1_create stub returns NULL */
	wm_xdg_decoration_init(&test_server);
	assert(test_server.xdg_decoration_mgr == NULL);
	printf("  PASS: xdg_decoration_init_error\n");
}

/* --- xdg decoration request_mode handler --- */

static void
test_xdg_decoration_request_mode_handler(void)
{
	struct wlr_xdg_toplevel_decoration_v1 wlr_deco;
	memset(&wlr_deco, 0, sizeof(wlr_deco));

	struct wm_xdg_decoration *deco = calloc(1, sizeof(*deco));
	assert(deco != NULL);
	deco->wlr_decoration = &wlr_deco;
	deco->server = &test_server;
	wl_list_init(&deco->destroy.link);
	wl_list_init(&deco->request_mode.link);

	/* Call request_mode handler — should set server-side mode (stub no-op) */
	handle_xdg_decoration_request_mode(&deco->request_mode, NULL);

	free(deco);
	printf("  PASS: xdg_decoration_request_mode_handler\n");
}

/* --- xdg decoration destroy handler --- */

static void
test_xdg_decoration_destroy_handler(void)
{
	struct wlr_xdg_toplevel_decoration_v1 wlr_deco;
	memset(&wlr_deco, 0, sizeof(wlr_deco));

	struct wm_xdg_decoration *deco = calloc(1, sizeof(*deco));
	assert(deco != NULL);
	deco->wlr_decoration = &wlr_deco;
	deco->server = &test_server;
	/* Must init links so wl_list_remove doesn't crash */
	wl_list_init(&deco->destroy.link);
	wl_list_init(&deco->request_mode.link);

	/* Destroy handler frees deco */
	handle_xdg_decoration_destroy(&deco->destroy, NULL);
	/* deco is now freed — do not access */
	printf("  PASS: xdg_decoration_destroy_handler\n");
}

/* --- ext tab bar auto size from font --- */

static void
test_update_ext_tab_bar_auto_size(void)
{
	setup();
	setup_view();
	test_config.tabs_intitlebar = false;
	test_config.tab_width = 0; /* auto-size from font */
	test_config.tab_placement = WM_TAB_BAR_TOP;
	test_style.window_label_focus_font.size = 14;

	struct wm_view view2 = {0};
	view2.server = &test_server;
	view2.title = "Tab 2";
	wl_list_init(&view2.tab_link);

	struct wm_tab_group tg;
	wl_list_init(&tg.views);
	tg.count = 2;
	tg.active_view = &test_view;
	tg.server = &test_server;

	setup_view();
	wl_list_insert(&tg.views, &view2.tab_link);
	wl_list_insert(&tg.views, &test_view.tab_link);
	test_view.tab_group = &tg;

	struct wm_decoration *deco = wm_decoration_create(&test_view,
		&test_style);
	assert(deco != NULL);

	/* Auto-size: font.size(14) + 8 = 22 */
	assert(deco->tab_bar_size == 22);

	test_view.tab_group = NULL;
	wm_decoration_destroy(deco);
	test_config.tabs_intitlebar = true;
	test_config.tab_width = 0;
	test_config.tab_placement = 0;
	printf("  PASS: update_ext_tab_bar_auto_size\n");
}

/* ===================== Main ===================== */

int
main(void)
{
	printf("test_decoration_logic:\n");

	/* parse_button_type */
	test_parse_button_type_close();
	test_parse_button_type_maximize();
	test_parse_button_type_minimize();
	test_parse_button_type_shade();
	test_parse_button_type_stick();
	test_parse_button_type_case_insensitive();
	test_parse_button_type_unknown();

	/* parse_button_layout */
	test_parse_button_layout_single();
	test_parse_button_layout_multiple();
	test_parse_button_layout_empty();
	test_parse_button_layout_null();
	test_parse_button_layout_invalid_tokens();
	test_parse_button_layout_mixed_valid_invalid();

	/* configure_buttons */
	test_configure_buttons();
	test_configure_buttons_null();

	/* button_at */
	test_button_at_hit();
	test_button_at_miss();
	test_button_at_right_button();
	test_button_at_null();

	/* region_at */
	test_region_at_titlebar();
	test_region_at_border_top();
	test_region_at_border_bottom();
	test_region_at_border_left();
	test_region_at_border_right();
	test_region_at_handle();
	test_region_at_grip_left();
	test_region_at_grip_right();
	test_region_at_client();
	test_region_at_outside();
	test_region_at_null();

	/* Preset-dependent regions */
	test_region_at_no_handle_tiny();
	test_region_at_no_titlebar_border();

	/* get_extents */
	test_get_extents_normal();
	test_get_extents_none();
	test_get_extents_border();
	test_get_extents_tab();
	test_get_extents_tiny();
	test_get_extents_tool();
	test_get_extents_null();

	/* Style helpers */
	test_style_titlebar_height_default();
	test_style_titlebar_height_explicit();
	test_style_handle_height_default();
	test_style_border_width_default();

	/* Misc */
	test_color_to_float4();

	/* tab_at */
	test_tab_at_no_group();
	test_tab_at_outside_titlebar();
	test_tab_at_null();
	test_tab_at_multiple_intitlebar();
	test_tab_at_no_titlebar_preset();

	/* tab_at (external tab bar) */
	test_tab_at_external_horizontal();
	test_tab_at_external_vertical();

	/* get_extents (external tab bar) */
	test_get_extents_ext_tab_bar_top();
	test_get_extents_ext_tab_bar_left();
	test_get_extents_ext_tab_bar_right();
	test_get_extents_ext_tab_bar_bottom();
	test_get_extents_null_params();

	/* configure_buttons additional */
	test_configure_buttons_replace();
	test_configure_buttons_empty();

	/* region_at additional */
	test_region_at_button();
	test_region_at_none_preset();

	/* Style helpers additional */
	test_style_titlebar_height_fallback();
	test_style_handle_height_explicit();
	test_style_border_width_zero();
	test_style_border_width_explicit();

	/* create / destroy */
	test_decoration_create_and_destroy();
	test_decoration_destroy_null();

	/* update */
	test_decoration_update_null_decoration();
	test_decoration_update_null_style();
	test_decoration_update_recalculates_dimensions();

	/* set_focused */
	test_set_focused_changes_state();
	test_set_focused_noop_same_state();
	test_set_focused_null();

	/* set_shaded */
	test_set_shaded_saves_geometry();
	test_set_shaded_unshade_restores();
	test_set_shaded_noop_same_state();
	test_set_shaded_null();

	/* set_preset */
	test_set_preset_transitions();
	test_set_preset_noop_same();
	test_set_preset_null();

	/* region_at: TOOL preset and external tab bar */
	test_region_at_tool_has_titlebar();
	test_region_at_external_tab_bar();

	/* button_at: edge boundary and multiple buttons */
	test_button_at_edge_boundary();
	test_button_at_multiple_buttons();

	/* get_extents: edge cases */
	test_get_extents_zero_border();
	test_get_extents_large_border();

	/* region_at: zero border */
	test_region_at_zero_border();

	/* create with custom config buttons */
	test_decoration_create_custom_buttons();

	/* pixel_buffer */
	test_pixel_buffer_write_rejected();
	test_wlr_buffer_from_cairo_null();

	/* layout_and_render dimension paths via update */
	test_update_normal_content_area();
	test_update_tab_preset_content_area();
	test_update_none_preset_disables_elements();
	test_update_border_preset_no_titlebar_no_handle();
	test_update_tiny_preset_has_titlebar_no_handle();
	test_update_normal_border_positions();
	test_update_normal_border_sizes();

	/* create with custom button configs */
	test_create_all_buttons_left();
	test_create_no_config();

	/* shade/unshade full cycle */
	test_shade_unshade_full_cycle_geometry();
	test_shade_double_shade_noop();

	/* round corners and tab bar */
	test_update_round_corners_border_frame();
	test_update_no_round_corners_rect_borders();
	test_update_ext_tab_bar_top_content_area();
	test_update_ext_tab_bar_left_content_area();

	/* focus border rendering paths */
	test_update_focus_border_enabled();
	test_update_focus_border_disabled_unfocused();
	test_update_focus_border_disabled_zero_width();

	/* style change + positions */
	test_update_style_change_dimensions();
	test_update_normal_handle_grip_positions();
	test_update_titlebar_tree_position();

	/* render_button / render_label direct */
	test_render_button_fallback_bg();
	test_render_button_small_glyph();
	test_render_label_center_justify();
	test_render_label_right_justify();
	test_render_label_null_title();
	test_render_label_empty_title();

	/* render_rounded_border_frame direct */
	test_render_rounded_border_frame_basic();
	test_render_rounded_border_frame_invalid();

	/* shade with scene children */
	test_set_shaded_with_scene_children();

	/* ext tab bar bottom/right placements */
	test_update_ext_tab_bar_bottom();
	test_update_ext_tab_bar_right();

	/* in-titlebar composite tab rendering */
	test_update_intitlebar_tabs_composite();

	/* ext tab bar auto-size from font */
	test_update_ext_tab_bar_auto_size();

	/* xdg decoration protocol */
	test_xdg_decoration_init_error();
	test_xdg_decoration_request_mode_handler();
	test_xdg_decoration_destroy_handler();

	printf("All decoration logic tests passed.\n");
	return 0;
}
