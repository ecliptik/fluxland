/*
 * test_menu_interaction.c - Unit tests for menu interaction logic
 *
 * Includes menu.c directly with stub implementations to avoid
 * needing wlroots/wayland/Pango/Cairo libraries at link time.
 */

#define _GNU_SOURCE

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- Block real headers via their include guards --- */
/* System/library headers */
#define CAIRO_H
#define __PANGO_H__
#define _XKBCOMMON_H_
#define _DRM_FOURCC_H_
#define WAYLAND_SERVER_CORE_H
#define WAYLAND_SERVER_H
#define WAYLAND_UTIL_H
#define WLR_UTIL_LOG_H
#define WLR_TYPES_WLR_SCENE_H
#define WLR_TYPES_WLR_BUFFER_H
#define WLR_INTERFACES_WLR_BUFFER_H
#define WLR_TYPES_WLR_XDG_SHELL_H
#define WLR_TYPES_WLR_XDG_DECORATION_V1_H
#define WLR_TYPES_WLR_OUTPUT_H
#define WLR_TYPES_WLR_OUTPUT_LAYOUT_H
#define WLR_UTIL_BOX_H

/* Project headers */
#define WM_VIEW_H
#define WM_SERVER_H
#define WM_DECORATION_H
#define WM_RENDER_H
#define WM_CONFIG_H
#define WM_STYLE_H
#define WM_OUTPUT_H
#define WM_WORKSPACE_H
#define WM_RULES_H
#define WM_IPC_H
#define WM_FOREIGN_TOPLEVEL_H
#define WM_KEYBIND_H
#define WM_I18N_H
#define WM_UTIL_H
#define WM_TABGROUP_H

/* Constants from drm_fourcc.h */
#define DRM_FORMAT_ARGB8888 0x34325241

/* --- i18n stubs --- */
#define _(x) (x)
#define N_(x) (x)

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

struct wl_event_source {
	int dummy;
};

struct wl_event_loop {
	int dummy;
};

struct wl_display {
	int dummy;
};

struct wl_resource {
	int dummy;
};

struct wl_client {
	int dummy;
};

struct wl_listener {
	struct wl_list link;
	void (*notify)(struct wl_listener *, void *);
};

struct wl_signal {
	struct wl_list listener_list;
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

struct wlr_scene {
	struct wlr_scene_tree tree;
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

struct wlr_output {
	int dummy;
};

struct wlr_output_layout {
	int dummy;
};

struct wlr_surface {
	int dummy;
};

struct wlr_xdg_surface {
	struct wlr_surface *surface;
	struct wl_resource *resource;
};

struct wlr_xdg_toplevel {
	struct wlr_xdg_surface *base;
	int width, height;
};

/* --- wlr_log no-op --- */
enum wlr_log_importance {
	WLR_SILENT = 0,
	WLR_ERROR = 1,
	WLR_INFO = 2,
	WLR_DEBUG = 3,
};
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

static inline uint32_t wm_color_to_argb(struct wm_color c)
{
	return ((uint32_t)c.a << 24) | ((uint32_t)c.r << 16) |
		((uint32_t)c.g << 8) | (uint32_t)c.b;
}

/* --- Stub config types --- */

enum wm_menu_search {
	WM_MENU_SEARCH_NOWHERE,
	WM_MENU_SEARCH_ITEMSTART,
	WM_MENU_SEARCH_SOMEWHERE,
};

struct wm_config {
	int workspace_count;
	char *titlebar_left;
	char *titlebar_right;
	enum wm_menu_search menu_search;
	int menu_delay;
	bool tabs_intitlebar;
	int tab_padding;
	int tab_width;
	int tab_placement;
	char *config_dir;
	char *apps_file;
	char *window_menu_file;
};

/* --- Stub server types --- */

struct wm_output {
	struct wl_list link;
	struct wlr_output *wlr_output;
	struct wlr_box usable_area;
};

struct wm_workspace {
	struct wl_list link;
	struct wm_server *server;
	struct wlr_scene_tree *tree;
	char *name;
	int index;
};

struct wm_tab_group {
	struct wl_list views;
	struct wm_view *active_view;
	int count;
	struct wm_server *server;
};

struct wm_decoration {
	int dummy;
};

struct wm_view {
	struct wl_list link;
	struct wm_server *server;
	struct wlr_xdg_toplevel *xdg_toplevel;
	struct wlr_scene_tree *scene_tree;
	uint32_t id;
	int x, y;
	bool sticky;
	char *title;
	char *app_id;
	struct wm_workspace *workspace;
	struct wm_tab_group *tab_group;
	struct wl_list tab_link;
	struct wm_decoration *decoration;
};

struct wm_ipc {
	int dummy;
};

enum wm_ipc_event_type {
	WM_IPC_EVENT_MENU = 0,
};

struct wm_server {
	struct wl_display *wl_display;
	struct wl_event_loop *wl_event_loop;
	struct wlr_scene *scene;
	struct wlr_output_layout *output_layout;
	struct wl_list outputs;
	struct wl_list views;
	struct wl_list workspaces;
	struct wm_config *config;
	struct wm_style *style;
	struct wm_view *focused_view;
	struct wm_ipc ipc;
	struct wm_menu *root_menu;
	struct wm_menu *window_menu;
	struct wm_menu *window_list_menu;
	struct wm_menu *workspace_menu;
	struct wm_menu *client_menu;
	struct wm_menu *custom_menu;
	struct wl_listener new_xdg_decoration;
};

/* --- Render stubs --- */

enum wm_button_type {
	WM_BUTTON_CLOSE,
	WM_BUTTON_MAXIMIZE,
	WM_BUTTON_ICONIFY,
	WM_BUTTON_SHADE,
	WM_BUTTON_STICK,
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
	/* In tests we leak scene trees for simplicity */
	(void)node;
}

static struct wlr_scene_tree *
wlr_scene_tree_from_node(struct wlr_scene_node *node)
{
	return (struct wlr_scene_tree *)node;
}

static struct wlr_output *
wlr_output_layout_output_at(struct wlr_output_layout *layout,
	double lx, double ly)
{
	(void)layout; (void)lx; (void)ly;
	return NULL; /* No output clamping in tests */
}

static void wlr_output_layout_get_box(struct wlr_output_layout *layout,
	struct wlr_output *output, struct wlr_box *box)
{
	(void)layout; (void)output;
	memset(box, 0, sizeof(*box));
}

static void wlr_xdg_toplevel_send_close(struct wlr_xdg_toplevel *tl)
{
	(void)tl;
}

static void wl_display_terminate(struct wl_display *disp)
{
	(void)disp;
}

static struct wl_client *
wl_resource_get_client(struct wl_resource *res)
{
	(void)res;
	return NULL;
}

static void wl_client_destroy(struct wl_client *client)
{
	(void)client;
}

/* --- Stub event loop timer --- */

static struct wl_event_source g_timer_source;

static struct wl_event_source *
wl_event_loop_add_timer(struct wl_event_loop *loop,
	int (*cb)(void *), void *data)
{
	(void)loop; (void)cb; (void)data;
	return &g_timer_source;
}

static int wl_event_source_timer_update(struct wl_event_source *src, int ms)
{
	(void)src; (void)ms;
	return 0;
}

static int wl_event_source_remove(struct wl_event_source *src)
{
	(void)src;
	return 0;
}

/* --- Stub Pango/GLib functions --- */

typedef unsigned int gunichar;

typedef enum {
	PANGO_DIRECTION_LTR,
	PANGO_DIRECTION_RTL,
	PANGO_DIRECTION_NEUTRAL,
} PangoDirection;

static gunichar g_utf8_get_char(const char *p)
{
	return (gunichar)(unsigned char)*p;
}

static const char *g_utf8_next_char(const char *p)
{
	return p + 1;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
static PangoDirection pango_unichar_direction(gunichar ch)
{
	(void)ch;
	return PANGO_DIRECTION_LTR;
}
#pragma GCC diagnostic pop

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
	(void)s;
	return CAIRO_STATUS_SUCCESS;
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

static cairo_surface_t *
cairo_image_surface_create_from_png(const char *path)
{
	(void)path;
	return &g_stub_surface;
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
static void cairo_stroke(cairo_t *cr) { (void)cr; }
static void cairo_set_line_width(cairo_t *cr, double w) { (void)cr; (void)w; }
static void cairo_move_to(cairo_t *cr, double x, double y) { (void)cr; (void)x; (void)y; }
static void cairo_line_to(cairo_t *cr, double x, double y) { (void)cr; (void)x; (void)y; }
static void cairo_close_path(cairo_t *cr) { (void)cr; }
static void cairo_set_fill_rule(cairo_t *cr, int rule) { (void)cr; (void)rule; }
static void cairo_new_sub_path(cairo_t *cr) { (void)cr; }
static void cairo_save(cairo_t *cr) { (void)cr; }
static void cairo_restore(cairo_t *cr) { (void)cr; }
static void cairo_translate(cairo_t *cr, double x, double y) { (void)cr; (void)x; (void)y; }
static void cairo_rotate(cairo_t *cr, double angle) { (void)cr; (void)angle; }
static void cairo_scale(cairo_t *cr, double sx, double sy) { (void)cr; (void)sx; (void)sy; }

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
	return NULL;
}

static int
wm_measure_text_width(const char *text, const struct wm_font *font,
	float scale)
{
	(void)font; (void)scale;
	/* Return approximate width: 7px per char */
	return text ? (int)strlen(text) * 7 : 0;
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

/* --- Stub cross-module functions --- */

static void wm_ipc_broadcast_event(struct wm_ipc *ipc, int type,
	const char *msg)
{
	(void)ipc; (void)type; (void)msg;
}

static void style_load(struct wm_style *style, const char *path)
{
	(void)style; (void)path;
}

static void wm_view_set_sticky(struct wm_view *view, bool sticky)
{
	if (view) view->sticky = sticky;
}

static void wm_view_raise(struct wm_view *view) { (void)view; }

static void wm_focus_view(struct wm_view *view, struct wlr_surface *surface)
{
	(void)view; (void)surface;
}

static void wm_workspace_switch(struct wm_server *server, int idx)
{
	(void)server; (void)idx;
}

static struct wm_workspace *wm_workspace_get_active(struct wm_server *server)
{
	(void)server;
	return NULL;
}

static void wm_foreign_toplevel_set_minimized(struct wm_view *view, bool min)
{
	(void)view; (void)min;
}

static void wm_rules_remember_window(struct wm_view *view, const char *path)
{
	(void)view; (void)path;
}

/* --- Stub util.h functions --- */

#include <errno.h>
#include <fcntl.h>

static inline bool safe_atoi(const char *s, int *out) {
	if (!s || !*s) return false;
	char *end;
	errno = 0;
	long v = strtol(s, &end, 10);
	if (errno || *end || v < INT32_MIN || v > INT32_MAX) return false;
	*out = (int)v;
	return true;
}

static inline FILE *fopen_nofollow(const char *path, const char *mode) {
	(void)mode;
	return fopen(path, "r");
}

/* --- xkbcommon key symbols --- */

typedef uint32_t xkb_keysym_t;

#define XKB_KEY_Up        0xff52
#define XKB_KEY_Down      0xff54
#define XKB_KEY_Left      0xff51
#define XKB_KEY_Right     0xff53
#define XKB_KEY_Return    0xff0d
#define XKB_KEY_KP_Enter  0xff8d
#define XKB_KEY_Escape    0xff1b
#define XKB_KEY_a         0x0061
#define XKB_KEY_z         0x007a
#define XKB_KEY_A         0x0041
#define XKB_KEY_Z         0x005a
#define XKB_KEY_0         0x0030
#define XKB_KEY_9         0x0039
#define XKB_KEY_space     0x0020
#define XKB_KEY_k         0x006b
#define XKB_KEY_j         0x006a
#define XKB_KEY_l         0x006c
#define XKB_KEY_h         0x0068

/* --- Include menu.c --- */

/* iconv stub: just return label unchanged */
#include <iconv.h>

#include "menu.c"

/* ===================== Test helpers ===================== */

static struct wm_server test_server;
static struct wm_style test_style;
static struct wm_config test_config;
static struct wlr_scene test_scene;
static struct wl_display test_display;
static struct wl_event_loop test_event_loop;
static struct wlr_output_layout test_output_layout;

static void
setup_server(void)
{
	memset(&test_server, 0, sizeof(test_server));
	memset(&test_style, 0, sizeof(test_style));
	memset(&test_config, 0, sizeof(test_config));
	memset(&test_scene, 0, sizeof(test_scene));
	memset(&test_display, 0, sizeof(test_display));
	memset(&test_event_loop, 0, sizeof(test_event_loop));

	wl_list_init(&test_scene.tree.children);
	wl_list_init(&test_scene.tree.node.link);
	wl_list_init(&test_server.outputs);
	wl_list_init(&test_server.views);
	wl_list_init(&test_server.workspaces);

	test_server.wl_display = &test_display;
	test_server.wl_event_loop = &test_event_loop;
	test_server.scene = &test_scene;
	test_server.output_layout = &test_output_layout;
	test_server.config = &test_config;
	test_server.style = &test_style;

	/* Set up style with defaults */
	test_style.menu_frame_font.size = 12;
	test_style.menu_title_font.size = 12;
	test_style.menu_border_width = 1;
	test_style.menu_item_height = 20;
	test_style.menu_title_height = 20;
}

/* Build a simple test menu with known items */
static struct wm_menu *
make_test_menu(int item_count)
{
	struct wm_menu *menu = menu_create(&test_server, "Test Menu");
	assert(menu != NULL);

	for (int i = 0; i < item_count; i++) {
		char label[32];
		snprintf(label, sizeof(label), "Item %d", i);
		struct wm_menu_item *item = menu_item_create(
			WM_MENU_EXEC, label);
		assert(item != NULL);
		item->command = strdup("echo test");
		wl_list_insert(menu->items.prev, &item->link);
	}

	return menu;
}

/* Build a menu with a separator at a given position */
static struct wm_menu *
make_menu_with_separator(void)
{
	struct wm_menu *menu = menu_create(&test_server, "Sep Menu");
	assert(menu != NULL);

	struct wm_menu_item *i0 = menu_item_create(WM_MENU_EXEC, "Alpha");
	struct wm_menu_item *i1 = menu_item_create(WM_MENU_SEPARATOR, NULL);
	struct wm_menu_item *i2 = menu_item_create(WM_MENU_EXEC, "Beta");

	wl_list_insert(menu->items.prev, &i0->link);
	wl_list_insert(menu->items.prev, &i1->link);
	wl_list_insert(menu->items.prev, &i2->link);

	return menu;
}

/* Build a menu with a submenu */
static struct wm_menu *
make_menu_with_submenu(void)
{
	struct wm_menu *menu = menu_create(&test_server, "Parent");

	struct wm_menu_item *i0 = menu_item_create(WM_MENU_EXEC, "First");
	struct wm_menu_item *i1 = menu_item_create(WM_MENU_SUBMENU, "Sub");
	struct wm_menu_item *i2 = menu_item_create(WM_MENU_EXEC, "Last");

	i1->submenu = menu_create(&test_server, "Sub Menu");
	i1->submenu->parent = menu;

	struct wm_menu_item *sub_item = menu_item_create(WM_MENU_EXEC, "SubItem");
	wl_list_insert(i1->submenu->items.prev, &sub_item->link);

	wl_list_insert(menu->items.prev, &i0->link);
	wl_list_insert(menu->items.prev, &i1->link);
	wl_list_insert(menu->items.prev, &i2->link);

	return menu;
}

/* Prepare a menu for interaction tests (set layout + fake visible) */
static void
prepare_menu(struct wm_menu *menu, int x, int y)
{
	menu_compute_layout(menu, &test_style);
	menu->x = x;
	menu->y = y;
	menu->visible = true;
	menu->selected_index = -1;
}

/* ===================== Tests ===================== */

/* --- String helper tests --- */

static void
test_strip_whitespace_basic(void)
{
	char buf[] = "  hello  ";
	char *result = strip_whitespace(buf);
	assert(strcmp(result, "hello") == 0);
	printf("  PASS: strip_whitespace_basic\n");
}

static void
test_strip_whitespace_no_whitespace(void)
{
	char buf[] = "hello";
	char *result = strip_whitespace(buf);
	assert(strcmp(result, "hello") == 0);
	printf("  PASS: strip_whitespace_no_whitespace\n");
}

static void
test_strip_whitespace_all_whitespace(void)
{
	char buf[] = "   ";
	char *result = strip_whitespace(buf);
	assert(strlen(result) == 0 || *result == ' ');
	printf("  PASS: strip_whitespace_all_whitespace\n");
}

static void
test_expand_path_tilde(void)
{
	char *result = expand_path("~/test");
	assert(result != NULL);
	assert(result[0] == '/'); /* expanded to absolute path */
	assert(strstr(result, "/test") != NULL);
	free(result);
	printf("  PASS: expand_path_tilde\n");
}

static void
test_expand_path_no_tilde(void)
{
	char *result = expand_path("/usr/bin/foo");
	assert(result != NULL);
	assert(strcmp(result, "/usr/bin/foo") == 0);
	free(result);
	printf("  PASS: expand_path_no_tilde\n");
}

static void
test_expand_path_null(void)
{
	char *result = expand_path(NULL);
	assert(result == NULL);
	printf("  PASS: expand_path_null\n");
}

static void
test_expand_path_tilde_only(void)
{
	char *result = expand_path("~");
	assert(result != NULL);
	free(result);
	printf("  PASS: expand_path_tilde_only\n");
}

/* --- Parser helpers --- */

static void
test_parse_paren(void)
{
	const char *str = " (hello world) rest";
	const char *pos = str;
	char *result = parse_paren(&pos);
	assert(result != NULL);
	assert(strcmp(result, "hello world") == 0);
	free(result);
	printf("  PASS: parse_paren\n");
}

static void
test_parse_paren_nested(void)
{
	const char *str = "(a(b)c)";
	const char *pos = str;
	char *result = parse_paren(&pos);
	assert(result != NULL);
	assert(strcmp(result, "a(b)c") == 0);
	free(result);
	printf("  PASS: parse_paren_nested\n");
}

static void
test_parse_brace(void)
{
	const char *str = " {command arg} ";
	const char *pos = str;
	char *result = parse_brace(&pos);
	assert(result != NULL);
	assert(strcmp(result, "command arg") == 0);
	free(result);
	printf("  PASS: parse_brace\n");
}

static void
test_parse_angle(void)
{
	const char *str = " <icon.png> ";
	const char *pos = str;
	char *result = parse_angle(&pos);
	assert(result != NULL);
	assert(strcmp(result, "icon.png") == 0);
	free(result);
	printf("  PASS: parse_angle\n");
}

static void
test_parse_paren_no_match(void)
{
	const char *str = "no parens here";
	const char *pos = str;
	char *result = parse_paren(&pos);
	assert(result == NULL);
	printf("  PASS: parse_paren_no_match\n");
}

/* --- Menu creation and layout --- */

static void
test_menu_create(void)
{
	setup_server();
	struct wm_menu *menu = menu_create(&test_server, "Test");
	assert(menu != NULL);
	assert(strcmp(menu->title, "Test") == 0);
	assert(menu->selected_index == -1);
	assert(wl_list_empty(&menu->items));
	wm_menu_destroy(menu);
	printf("  PASS: menu_create\n");
}

static void
test_menu_item_create(void)
{
	struct wm_menu_item *item = menu_item_create(WM_MENU_EXEC, "Launch");
	assert(item != NULL);
	assert(item->type == WM_MENU_EXEC);
	assert(strcmp(item->label, "Launch") == 0);
	assert(item->command == NULL);
	menu_item_destroy(item);
	printf("  PASS: menu_item_create\n");
}

static void
test_menu_compute_layout(void)
{
	setup_server();
	struct wm_menu *menu = make_test_menu(5);

	menu_compute_layout(menu, &test_style);

	assert(menu->item_count == 5);
	assert(menu->item_height == 20);
	assert(menu->title_height == 20);
	assert(menu->width >= MENU_MIN_WIDTH);
	assert(menu->height > 0);
	/* height = 2*border + title + 5*item_height */
	assert(menu->height == 2 * 1 + 20 + 5 * 20);

	wm_menu_destroy(menu);
	printf("  PASS: menu_compute_layout\n");
}

static void
test_menu_compute_layout_with_separator(void)
{
	setup_server();
	struct wm_menu *menu = make_menu_with_separator();

	menu_compute_layout(menu, &test_style);

	assert(menu->item_count == 3);
	/* height = 2*border + title + 2*item_h + separator_h */
	int expected = 2 * 1 + 20 + 2 * 20 + MENU_SEPARATOR_HEIGHT;
	assert(menu->height == expected);

	wm_menu_destroy(menu);
	printf("  PASS: menu_compute_layout_with_separator\n");
}

/* --- Hit testing --- */

static void
test_menu_item_at_first(void)
{
	setup_server();
	struct wm_menu *menu = make_test_menu(3);
	prepare_menu(menu, 100, 100);

	int bw = menu->border_width;
	/* First item starts at y = menu->y + bw + title_height */
	double lx = 100 + bw + 5; /* middle x */
	double ly = 100 + bw + menu->title_height + 5; /* first item */
	int idx = menu_item_at(menu, lx, ly);
	assert(idx == 0);

	wm_menu_destroy(menu);
	printf("  PASS: menu_item_at_first\n");
}

static void
test_menu_item_at_last(void)
{
	setup_server();
	struct wm_menu *menu = make_test_menu(3);
	prepare_menu(menu, 100, 100);

	int bw = menu->border_width;
	/* Last item at y offset = title_height + 2*item_height + 5 */
	double lx = 100 + bw + 5;
	double ly = 100 + bw + menu->title_height +
		2 * menu->item_height + 5;
	int idx = menu_item_at(menu, lx, ly);
	assert(idx == 2);

	wm_menu_destroy(menu);
	printf("  PASS: menu_item_at_last\n");
}

static void
test_menu_item_at_outside(void)
{
	setup_server();
	struct wm_menu *menu = make_test_menu(3);
	prepare_menu(menu, 100, 100);

	/* Way outside */
	int idx = menu_item_at(menu, 0, 0);
	assert(idx == -1);

	/* On title bar */
	int idx2 = menu_item_at(menu, 105, 105);
	assert(idx2 == -1);

	wm_menu_destroy(menu);
	printf("  PASS: menu_item_at_outside\n");
}

static void
test_menu_contains_point(void)
{
	setup_server();
	struct wm_menu *menu = make_test_menu(3);
	prepare_menu(menu, 100, 100);

	/* Inside */
	assert(menu_contains_point(menu, 105, 105) == true);

	/* Outside */
	assert(menu_contains_point(menu, 50, 50) == false);

	/* Just at boundary */
	assert(menu_contains_point(menu, 100, 100) == true);
	assert(menu_contains_point(menu, 100 + menu->width, 100) == false);

	wm_menu_destroy(menu);
	printf("  PASS: menu_contains_point\n");
}

static void
test_menu_contains_point_invisible(void)
{
	setup_server();
	struct wm_menu *menu = make_test_menu(3);
	prepare_menu(menu, 100, 100);
	menu->visible = false;

	assert(menu_contains_point(menu, 105, 105) == false);

	wm_menu_destroy(menu);
	printf("  PASS: menu_contains_point_invisible\n");
}

/* --- Key navigation --- */

static void
test_key_down_selects_first(void)
{
	setup_server();
	struct wm_menu *menu = make_test_menu(3);
	prepare_menu(menu, 100, 100);

	/* selected_index starts at -1, Down should select first item */
	bool consumed = wm_menu_handle_key_for(menu, XKB_KEY_Down);
	assert(consumed == true);
	assert(menu->selected_index == 0);

	wm_menu_destroy(menu);
	printf("  PASS: key_down_selects_first\n");
}

static void
test_key_down_wraps(void)
{
	setup_server();
	struct wm_menu *menu = make_test_menu(3);
	prepare_menu(menu, 100, 100);
	menu->selected_index = 2; /* last item */

	bool consumed = wm_menu_handle_key_for(menu, XKB_KEY_Down);
	assert(consumed == true);
	assert(menu->selected_index == 0); /* wrapped to first */

	wm_menu_destroy(menu);
	printf("  PASS: key_down_wraps\n");
}

static void
test_key_up_wraps(void)
{
	setup_server();
	struct wm_menu *menu = make_test_menu(3);
	prepare_menu(menu, 100, 100);
	menu->selected_index = 0;

	bool consumed = wm_menu_handle_key_for(menu, XKB_KEY_Up);
	assert(consumed == true);
	assert(menu->selected_index == 2); /* wrapped to last */

	wm_menu_destroy(menu);
	printf("  PASS: key_up_wraps\n");
}

static void
test_key_skips_separator(void)
{
	setup_server();
	struct wm_menu *menu = make_menu_with_separator();
	prepare_menu(menu, 100, 100);
	menu->selected_index = 0; /* Alpha */

	/* Down from index 0 should skip separator (index 1) to Beta (index 2) */
	bool consumed = wm_menu_handle_key_for(menu, XKB_KEY_Down);
	assert(consumed == true);
	assert(menu->selected_index == 2);

	wm_menu_destroy(menu);
	printf("  PASS: key_skips_separator\n");
}

static void
test_key_up_skips_separator(void)
{
	setup_server();
	struct wm_menu *menu = make_menu_with_separator();
	prepare_menu(menu, 100, 100);
	menu->selected_index = 2; /* Beta */

	/* Up from index 2 should skip separator (index 1) to Alpha (index 0) */
	bool consumed = wm_menu_handle_key_for(menu, XKB_KEY_Up);
	assert(consumed == true);
	assert(menu->selected_index == 0);

	wm_menu_destroy(menu);
	printf("  PASS: key_up_skips_separator\n");
}

static void
test_key_escape_hides_menu(void)
{
	setup_server();
	struct wm_menu *menu = make_test_menu(3);
	prepare_menu(menu, 100, 100);

	/* Need to set up scene tree for hide to work */
	menu->scene_tree = wlr_scene_tree_create(&test_scene.tree);

	bool consumed = wm_menu_handle_key_for(menu, XKB_KEY_Escape);
	assert(consumed == true);
	assert(menu->visible == false);

	wm_menu_destroy(menu);
	printf("  PASS: key_escape_hides_menu\n");
}

static void
test_key_right_enters_submenu(void)
{
	setup_server();
	struct wm_menu *menu = make_menu_with_submenu();
	prepare_menu(menu, 100, 100);
	menu->selected_index = 1; /* Submenu item */

	bool consumed = wm_menu_handle_key_for(menu, XKB_KEY_Right);
	assert(consumed == true);

	/* Submenu should now be visible */
	struct wm_menu_item *sub_item = menu_get_item(menu, 1);
	assert(sub_item != NULL);
	assert(sub_item->submenu != NULL);
	assert(sub_item->submenu->visible == true);

	wm_menu_destroy(menu);
	printf("  PASS: key_right_enters_submenu\n");
}

static void
test_key_left_leaves_submenu(void)
{
	setup_server();
	struct wm_menu *menu = make_menu_with_submenu();
	prepare_menu(menu, 100, 100);

	/* Open submenu */
	menu->selected_index = 1;
	wm_menu_handle_key_for(menu, XKB_KEY_Right);

	struct wm_menu_item *sub_item = menu_get_item(menu, 1);
	assert(sub_item->submenu->visible == true);

	/* Now pressing Left in the submenu should close it */
	bool consumed = wm_menu_handle_key_for(menu, XKB_KEY_Left);
	assert(consumed == true);
	assert(sub_item->submenu->visible == false);

	wm_menu_destroy(menu);
	printf("  PASS: key_left_leaves_submenu\n");
}

static void
test_key_not_visible(void)
{
	setup_server();
	struct wm_menu *menu = make_test_menu(3);
	menu->visible = false;

	bool consumed = wm_menu_handle_key_for(menu, XKB_KEY_Down);
	assert(consumed == false);

	wm_menu_destroy(menu);
	printf("  PASS: key_not_visible\n");
}

/* --- Type-ahead search --- */

static void
test_type_ahead_itemstart(void)
{
	setup_server();
	test_config.menu_search = WM_MENU_SEARCH_ITEMSTART;

	struct wm_menu *menu = menu_create(&test_server, "Search Test");
	struct wm_menu_item *i0 = menu_item_create(WM_MENU_EXEC, "Apple");
	struct wm_menu_item *i1 = menu_item_create(WM_MENU_EXEC, "Banana");
	struct wm_menu_item *i2 = menu_item_create(WM_MENU_EXEC, "Cherry");
	wl_list_insert(menu->items.prev, &i0->link);
	wl_list_insert(menu->items.prev, &i1->link);
	wl_list_insert(menu->items.prev, &i2->link);
	prepare_menu(menu, 0, 0);

	/* Type 'b' should select Banana (index 1) */
	bool consumed = menu_type_ahead(menu, 'b');
	assert(consumed == true);
	assert(menu->selected_index == 1);

	wm_menu_destroy(menu);
	printf("  PASS: type_ahead_itemstart\n");
}

static void
test_type_ahead_somewhere(void)
{
	setup_server();
	test_config.menu_search = WM_MENU_SEARCH_SOMEWHERE;

	struct wm_menu *menu = menu_create(&test_server, "Search Test");
	struct wm_menu_item *i0 = menu_item_create(WM_MENU_EXEC, "foo bar");
	struct wm_menu_item *i1 = menu_item_create(WM_MENU_EXEC, "baz qux");
	wl_list_insert(menu->items.prev, &i0->link);
	wl_list_insert(menu->items.prev, &i1->link);
	prepare_menu(menu, 0, 0);

	/* Type 'q' should match "baz qux" (index 1) with anywhere search */
	bool consumed = menu_type_ahead(menu, 'q');
	assert(consumed == true);
	assert(menu->selected_index == 1);

	wm_menu_destroy(menu);
	printf("  PASS: type_ahead_somewhere\n");
}

static void
test_type_ahead_nowhere(void)
{
	setup_server();
	test_config.menu_search = WM_MENU_SEARCH_NOWHERE;

	struct wm_menu *menu = make_test_menu(3);
	prepare_menu(menu, 0, 0);

	bool consumed = menu_type_ahead(menu, 'a');
	assert(consumed == false);

	wm_menu_destroy(menu);
	printf("  PASS: type_ahead_nowhere\n");
}

static void
test_type_ahead_skips_separator(void)
{
	setup_server();
	test_config.menu_search = WM_MENU_SEARCH_ITEMSTART;

	struct wm_menu *menu = menu_create(&test_server, "Test");
	struct wm_menu_item *i0 = menu_item_create(WM_MENU_SEPARATOR, NULL);
	struct wm_menu_item *i1 = menu_item_create(WM_MENU_EXEC, "Apple");
	wl_list_insert(menu->items.prev, &i0->link);
	wl_list_insert(menu->items.prev, &i1->link);
	prepare_menu(menu, 0, 0);

	bool consumed = menu_type_ahead(menu, 'a');
	assert(consumed == true);
	assert(menu->selected_index == 1);

	wm_menu_destroy(menu);
	printf("  PASS: type_ahead_skips_separator\n");
}

/* --- Window menu creation --- */

static void
test_create_window_menu(void)
{
	setup_server();
	wl_list_init(&test_server.workspaces);

	struct wm_menu *menu = wm_menu_create_window_menu(&test_server);
	assert(menu != NULL);
	assert(menu->title != NULL);
	assert(strcmp(menu->title, "Window") == 0);

	/* Count items */
	int count = 0;
	struct wm_menu_item *item;
	wl_list_for_each(item, &menu->items, link) {
		count++;
	}
	assert(count > 0);

	wm_menu_destroy(menu);
	printf("  PASS: create_window_menu\n");
}

/* --- menu_get_item --- */

static void
test_menu_get_item(void)
{
	setup_server();
	struct wm_menu *menu = make_test_menu(3);

	struct wm_menu_item *item0 = menu_get_item(menu, 0);
	assert(item0 != NULL);
	assert(strcmp(item0->label, "Item 0") == 0);

	struct wm_menu_item *item2 = menu_get_item(menu, 2);
	assert(item2 != NULL);
	assert(strcmp(item2->label, "Item 2") == 0);

	struct wm_menu_item *bad = menu_get_item(menu, 10);
	assert(bad == NULL);

	wm_menu_destroy(menu);
	printf("  PASS: menu_get_item\n");
}

/* --- find_deepest_visible --- */

static void
test_find_deepest_visible(void)
{
	setup_server();
	struct wm_menu *menu = make_menu_with_submenu();
	menu->visible = true;

	/* No submenu open: deepest is the menu itself */
	struct wm_menu *deepest = find_deepest_visible(menu);
	assert(deepest == menu);

	/* Open submenu */
	struct wm_menu_item *sub_item = menu_get_item(menu, 1);
	sub_item->submenu->visible = true;

	deepest = find_deepest_visible(menu);
	assert(deepest == sub_item->submenu);

	wm_menu_destroy(menu);
	printf("  PASS: find_deepest_visible\n");
}

/* --- find_menu_at --- */

static void
test_find_menu_at(void)
{
	setup_server();
	struct wm_menu *menu = make_test_menu(3);
	prepare_menu(menu, 100, 100);

	/* Point inside */
	struct wm_menu *found = find_menu_at(menu, 105, 105);
	assert(found == menu);

	/* Point outside */
	found = find_menu_at(menu, 50, 50);
	assert(found == NULL);

	wm_menu_destroy(menu);
	printf("  PASS: find_menu_at\n");
}

/* --- find_base_dir --- */

static void
test_find_base_dir_null(void)
{
	PangoDirection dir = find_base_dir(NULL);
	assert(dir == PANGO_DIRECTION_NEUTRAL);
	printf("  PASS: find_base_dir_null\n");
}

static void
test_find_base_dir_ltr(void)
{
	PangoDirection dir = find_base_dir("Hello");
	/* Our stub always returns LTR */
	assert(dir == PANGO_DIRECTION_LTR);
	printf("  PASS: find_base_dir_ltr\n");
}

/* --- wm_color_to_float4 --- */

static void
test_color_to_float4(void)
{
	struct wm_color c = {255, 128, 0, 255};
	float out[4];
	wm_color_to_float4(&c, out);
	assert(out[0] == 1.0f);
	assert(out[3] == 1.0f);
	/* 128/255 ≈ 0.502 */
	assert(out[1] > 0.49f && out[1] < 0.51f);
	assert(out[2] == 0.0f);
	printf("  PASS: color_to_float4\n");
}

/* --- Menu destroy --- */

static void
test_menu_destroy_null(void)
{
	wm_menu_destroy(NULL); /* should not crash */
	printf("  PASS: menu_destroy_null\n");
}

static void
test_menu_destroy_recursive(void)
{
	setup_server();
	struct wm_menu *menu = make_menu_with_submenu();
	/* Should free everything including submenu */
	wm_menu_destroy(menu);
	printf("  PASS: menu_destroy_recursive\n");
}

/* --- Vim-style key aliases --- */

static void
test_key_j_moves_down(void)
{
	setup_server();
	struct wm_menu *menu = make_test_menu(3);
	prepare_menu(menu, 0, 0);
	menu->selected_index = 0;

	bool consumed = wm_menu_handle_key_for(menu, XKB_KEY_j);
	assert(consumed == true);
	assert(menu->selected_index == 1);

	wm_menu_destroy(menu);
	printf("  PASS: key_j_moves_down\n");
}

static void
test_key_k_moves_up(void)
{
	setup_server();
	struct wm_menu *menu = make_test_menu(3);
	prepare_menu(menu, 0, 0);
	menu->selected_index = 1;

	bool consumed = wm_menu_handle_key_for(menu, XKB_KEY_k);
	assert(consumed == true);
	assert(menu->selected_index == 0);

	wm_menu_destroy(menu);
	printf("  PASS: key_k_moves_up\n");
}

/* --- Execute menu item: exit --- */

static void
test_execute_exit(void)
{
	setup_server();
	struct wm_menu *menu = menu_create(&test_server, "Test");
	struct wm_menu_item *item = menu_item_create(WM_MENU_EXIT, "Exit");
	wl_list_insert(menu->items.prev, &item->link);

	/* Should not crash; calls wl_display_terminate stub */
	execute_menu_item(menu, item);

	wm_menu_destroy(menu);
	printf("  PASS: execute_exit\n");
}

/* --- Execute menu item: style --- */

static void
test_execute_style(void)
{
	setup_server();
	struct wm_menu *menu = menu_create(&test_server, "Test");
	struct wm_menu_item *item = menu_item_create(WM_MENU_STYLE, "Dark");
	item->command = strdup("/usr/share/styles/dark");
	wl_list_insert(menu->items.prev, &item->link);

	/* Should call style_load stub */
	execute_menu_item(menu, item);

	wm_menu_destroy(menu);
	printf("  PASS: execute_style\n");
}

/* --- Execute menu item: stick (toggle) --- */

static void
test_execute_stick(void)
{
	setup_server();
	struct wm_view view;
	memset(&view, 0, sizeof(view));
	view.sticky = false;
	test_server.focused_view = &view;

	struct wm_menu *menu = menu_create(&test_server, "Test");
	struct wm_menu_item *item = menu_item_create(WM_MENU_STICK, "Stick");
	wl_list_insert(menu->items.prev, &item->link);

	execute_menu_item(menu, item);
	assert(view.sticky == true);

	/* Toggle again */
	execute_menu_item(menu, item);
	assert(view.sticky == false);

	test_server.focused_view = NULL;
	wm_menu_destroy(menu);
	printf("  PASS: execute_stick\n");
}

/* --- Execute menu item: raise --- */

static void
test_execute_raise(void)
{
	setup_server();
	struct wm_view view;
	memset(&view, 0, sizeof(view));
	test_server.focused_view = &view;

	struct wm_menu *menu = menu_create(&test_server, "Test");
	struct wm_menu_item *item = menu_item_create(WM_MENU_RAISE, "Raise");
	wl_list_insert(menu->items.prev, &item->link);

	execute_menu_item(menu, item);
	/* Should not crash */

	test_server.focused_view = NULL;
	wm_menu_destroy(menu);
	printf("  PASS: execute_raise\n");
}

/* --- Execute menu item: lower --- */

static void
test_execute_lower(void)
{
	setup_server();
	struct wm_view view;
	memset(&view, 0, sizeof(view));
	struct wlr_scene_tree view_tree;
	memset(&view_tree, 0, sizeof(view_tree));
	wl_list_init(&view_tree.children);
	wl_list_init(&view_tree.node.link);
	view.scene_tree = &view_tree;
	test_server.focused_view = &view;

	struct wm_menu *menu = menu_create(&test_server, "Test");
	struct wm_menu_item *item = menu_item_create(WM_MENU_LOWER, "Lower");
	wl_list_insert(menu->items.prev, &item->link);

	execute_menu_item(menu, item);
	/* Should call wlr_scene_node_lower_to_bottom stub */

	test_server.focused_view = NULL;
	wm_menu_destroy(menu);
	printf("  PASS: execute_lower\n");
}

/* --- Execute menu item: command --- */

static void
test_execute_command(void)
{
	setup_server();
	struct wm_menu *menu = menu_create(&test_server, "Test");
	struct wm_menu_item *item = menu_item_create(WM_MENU_COMMAND, "Cmd");
	item->command = strdup("Maximize");
	wl_list_insert(menu->items.prev, &item->link);

	execute_menu_item(menu, item);
	/* Should not crash */

	wm_menu_destroy(menu);
	printf("  PASS: execute_command\n");
}

/* --- Execute menu item: remember --- */

static void
test_execute_remember(void)
{
	setup_server();
	struct wm_view view;
	memset(&view, 0, sizeof(view));
	test_server.focused_view = &view;
	test_config.apps_file = strdup("/tmp/test_apps");

	struct wm_menu *menu = menu_create(&test_server, "Test");
	struct wm_menu_item *item = menu_item_create(WM_MENU_REMEMBER, "Remember");
	wl_list_insert(menu->items.prev, &item->link);

	execute_menu_item(menu, item);
	/* Should call wm_rules_remember_window stub */

	test_server.focused_view = NULL;
	free(test_config.apps_file);
	test_config.apps_file = NULL;
	wm_menu_destroy(menu);
	printf("  PASS: execute_remember\n");
}

/* --- Execute menu item: null safety --- */

static void
test_execute_null_safety(void)
{
	execute_menu_item(NULL, NULL);
	/* Should not crash */

	setup_server();
	struct wm_menu *menu = menu_create(&test_server, "Test");
	execute_menu_item(menu, NULL);
	/* Should not crash */

	wm_menu_destroy(menu);
	printf("  PASS: execute_null_safety\n");
}

/* --- Button click: activate item --- */

static void
test_button_activates_item(void)
{
	setup_server();
	struct wm_menu *menu = make_test_menu(3);
	prepare_menu(menu, 100, 100);
	menu->scene_tree = wlr_scene_tree_create(&test_scene.tree);

	int bw = menu->border_width;
	/* Click on first item */
	double lx = 100 + bw + 5;
	double ly = 100 + bw + menu->title_height + 5;

	bool consumed = wm_menu_handle_button_for(menu, lx, ly, 0x110, true);
	assert(consumed == true);
	/* Menu should be hidden after activation */
	assert(menu->visible == false);

	wm_menu_destroy(menu);
	printf("  PASS: button_activates_item\n");
}

/* --- Button click: outside closes menu --- */

static void
test_button_outside_closes(void)
{
	setup_server();
	struct wm_menu *menu = make_test_menu(3);
	prepare_menu(menu, 100, 100);
	menu->scene_tree = wlr_scene_tree_create(&test_scene.tree);

	bool consumed = wm_menu_handle_button_for(menu, 0, 0, 0x110, true);
	assert(consumed == false);
	assert(menu->visible == false);

	wm_menu_destroy(menu);
	printf("  PASS: button_outside_closes\n");
}

/* --- Button click: on title bar --- */

static void
test_button_on_title(void)
{
	setup_server();
	struct wm_menu *menu = make_test_menu(3);
	prepare_menu(menu, 100, 100);

	int bw = menu->border_width;
	/* Click on title area (within border, above items) */
	double lx = 100 + bw + 5;
	double ly = 100 + bw + 5; /* title area */

	bool consumed = wm_menu_handle_button_for(menu, lx, ly, 0x110, true);
	assert(consumed == true);
	/* Menu stays visible (clicked on title, no item) */
	assert(menu->visible == true);

	wm_menu_destroy(menu);
	printf("  PASS: button_on_title\n");
}

/* --- Button click: toggle submenu --- */

static void
test_button_toggle_submenu(void)
{
	setup_server();
	struct wm_menu *menu = make_menu_with_submenu();
	prepare_menu(menu, 100, 100);

	int bw = menu->border_width;
	/* Click on submenu item (index 1) */
	double lx = 100 + bw + 5;
	double ly = 100 + bw + menu->title_height + menu->item_height + 5;

	bool consumed = wm_menu_handle_button_for(menu, lx, ly, 0x110, true);
	assert(consumed == true);

	/* Submenu should now be visible */
	struct wm_menu_item *sub_item = menu_get_item(menu, 1);
	assert(sub_item->submenu->visible == true);

	/* Click again to close submenu */
	/* Need to prepare submenu layout for it to be found */
	prepare_menu(sub_item->submenu, sub_item->submenu->x, sub_item->submenu->y);
	consumed = wm_menu_handle_button_for(menu, lx, ly, 0x110, true);
	assert(consumed == true);
	assert(sub_item->submenu->visible == false);

	wm_menu_destroy(menu);
	printf("  PASS: button_toggle_submenu\n");
}

/* --- Button release: returns whether point is over menu --- */

static void
test_button_release_in_menu(void)
{
	setup_server();
	struct wm_menu *menu = make_test_menu(3);
	prepare_menu(menu, 100, 100);

	/* Release (not pressed) inside menu */
	bool consumed = wm_menu_handle_button_for(menu, 105, 105, 0x110, false);
	assert(consumed == true);
	/* Menu should still be visible */
	assert(menu->visible == true);

	/* Release outside */
	consumed = wm_menu_handle_button_for(menu, 0, 0, 0x110, false);
	assert(consumed == false);

	wm_menu_destroy(menu);
	printf("  PASS: button_release_in_menu\n");
}

/* --- Motion: updates selection --- */

static void
test_motion_updates_selection(void)
{
	setup_server();
	struct wm_menu *menu = make_test_menu(3);
	prepare_menu(menu, 100, 100);

	int bw = menu->border_width;
	/* Move over second item */
	double lx = 100 + bw + 5;
	double ly = 100 + bw + menu->title_height + menu->item_height + 5;

	bool consumed = wm_menu_handle_motion_for(menu, lx, ly);
	assert(consumed == true);
	assert(menu->selected_index == 1);

	/* Move to third item */
	ly = 100 + bw + menu->title_height + 2 * menu->item_height + 5;
	consumed = wm_menu_handle_motion_for(menu, lx, ly);
	assert(consumed == true);
	assert(menu->selected_index == 2);

	wm_menu_destroy(menu);
	printf("  PASS: motion_updates_selection\n");
}

/* --- Motion: outside menu returns false --- */

static void
test_motion_outside_menu(void)
{
	setup_server();
	struct wm_menu *menu = make_test_menu(3);
	prepare_menu(menu, 100, 100);

	bool consumed = wm_menu_handle_motion_for(menu, 0, 0);
	assert(consumed == false);

	wm_menu_destroy(menu);
	printf("  PASS: motion_outside_menu\n");
}

/* --- Menu show/hide lifecycle --- */

static void
test_menu_show_hide_lifecycle(void)
{
	setup_server();
	struct wm_menu *menu = make_test_menu(3);

	wm_menu_show(menu, 50, 50);
	assert(menu->visible == true);
	assert(menu->x == 50);
	assert(menu->y == 50);
	assert(menu->selected_index == -1);

	wm_menu_hide(menu);
	assert(menu->visible == false);
	assert(menu->scene_tree == NULL);

	wm_menu_destroy(menu);
	printf("  PASS: menu_show_hide_lifecycle\n");
}

/* --- Menu show re-show (already visible) --- */

static void
test_menu_show_reshow(void)
{
	setup_server();
	struct wm_menu *menu = make_test_menu(3);

	wm_menu_show(menu, 50, 50);
	assert(menu->visible == true);

	/* Re-show at different position - should hide first then show */
	wm_menu_show(menu, 200, 200);
	assert(menu->visible == true);
	assert(menu->x == 200);
	assert(menu->y == 200);

	wm_menu_destroy(menu);
	printf("  PASS: menu_show_reshow\n");
}

/* --- Menu hide_all --- */

static void
test_menu_hide_all(void)
{
	setup_server();
	struct wm_menu *root = make_test_menu(2);
	struct wm_menu *win = make_test_menu(2);

	test_server.root_menu = root;
	test_server.window_menu = win;

	wm_menu_show(root, 10, 10);
	wm_menu_show(win, 20, 20);
	assert(root->visible == true);
	assert(win->visible == true);

	wm_menu_hide_all(&test_server);
	assert(root->visible == false);
	assert(win->visible == false);

	test_server.root_menu = NULL;
	test_server.window_menu = NULL;
	wm_menu_destroy(root);
	wm_menu_destroy(win);
	printf("  PASS: menu_hide_all\n");
}

/* --- Menu hide: clears search buffer --- */

static void
test_menu_hide_clears_search(void)
{
	setup_server();
	test_config.menu_search = WM_MENU_SEARCH_ITEMSTART;

	struct wm_menu *menu = menu_create(&test_server, "Test");
	struct wm_menu_item *i0 = menu_item_create(WM_MENU_EXEC, "Apple");
	wl_list_insert(menu->items.prev, &i0->link);

	wm_menu_show(menu, 0, 0);
	menu_type_ahead(menu, 'a');
	assert(menu->search_len > 0);

	wm_menu_hide(menu);
	assert(menu->search_len == 0);
	assert(menu->search_buf[0] == '\0');

	wm_menu_destroy(menu);
	printf("  PASS: menu_hide_clears_search\n");
}

/* --- Workspace menu creation --- */

static void
test_create_workspace_menu(void)
{
	setup_server();

	/* Create some fake workspaces */
	struct wm_workspace ws0 = { .name = strdup("Main"), .index = 0 };
	struct wm_workspace ws1 = { .name = strdup("Code"), .index = 1 };
	wl_list_init(&ws0.link);
	wl_list_init(&ws1.link);
	wl_list_insert(test_server.workspaces.prev, &ws0.link);
	wl_list_insert(test_server.workspaces.prev, &ws1.link);

	struct wm_menu *menu = wm_menu_create_workspace_menu(&test_server);
	assert(menu != NULL);
	assert(strcmp(menu->title, "Workspaces") == 0);

	/* Should have 2 items */
	int count = 0;
	struct wm_menu_item *item;
	wl_list_for_each(item, &menu->items, link) {
		count++;
		assert(item->type == WM_MENU_COMMAND);
		assert(item->command != NULL);
	}
	assert(count == 2);

	wl_list_remove(&ws0.link);
	wl_list_remove(&ws1.link);
	free(ws0.name);
	free(ws1.name);
	wm_menu_destroy(menu);
	printf("  PASS: create_workspace_menu\n");
}

/* --- Window list menu creation --- */

static void
test_create_window_list(void)
{
	setup_server();

	struct wm_workspace ws0 = { .name = strdup("Main"), .index = 0,
		.server = &test_server };
	wl_list_init(&ws0.link);
	wl_list_insert(test_server.workspaces.prev, &ws0.link);

	struct wlr_scene_tree view_tree;
	memset(&view_tree, 0, sizeof(view_tree));
	wl_list_init(&view_tree.children);
	wl_list_init(&view_tree.node.link);
	view_tree.node.enabled = 1;

	struct wm_view view;
	memset(&view, 0, sizeof(view));
	view.title = strdup("Test Window");
	view.workspace = &ws0;
	view.scene_tree = &view_tree;
	wl_list_init(&view.link);
	wl_list_insert(test_server.views.prev, &view.link);

	struct wm_menu *menu = wm_menu_create_window_list(&test_server);
	assert(menu != NULL);
	assert(strcmp(menu->title, "Windows") == 0);

	/* Should have workspace header + window entry */
	int count = 0;
	struct wm_menu_item *item;
	wl_list_for_each(item, &menu->items, link) {
		count++;
	}
	assert(count == 2); /* header + entry */

	wl_list_remove(&ws0.link);
	wl_list_remove(&view.link);
	free(ws0.name);
	free(view.title);
	wm_menu_destroy(menu);
	printf("  PASS: create_window_list\n");
}

/* --- Client menu creation with pattern filter --- */

static void
test_client_menu_with_pattern(void)
{
	setup_server();

	struct wlr_scene_tree view_tree1, view_tree2;
	memset(&view_tree1, 0, sizeof(view_tree1));
	memset(&view_tree2, 0, sizeof(view_tree2));
	wl_list_init(&view_tree1.children);
	wl_list_init(&view_tree1.node.link);
	wl_list_init(&view_tree2.children);
	wl_list_init(&view_tree2.node.link);
	view_tree1.node.enabled = 1;
	view_tree2.node.enabled = 1;

	struct wm_workspace ws0 = { .name = strdup("Main"), .index = 0 };
	wl_list_init(&ws0.link);
	wl_list_insert(test_server.workspaces.prev, &ws0.link);

	struct wm_view view1, view2;
	memset(&view1, 0, sizeof(view1));
	memset(&view2, 0, sizeof(view2));
	view1.title = strdup("Firefox");
	view1.app_id = strdup("firefox");
	view1.workspace = &ws0;
	view1.scene_tree = &view_tree1;
	wl_list_init(&view1.link);

	view2.title = strdup("Terminal");
	view2.app_id = strdup("kitty");
	view2.workspace = &ws0;
	view2.scene_tree = &view_tree2;
	wl_list_init(&view2.link);

	wl_list_insert(test_server.views.prev, &view1.link);
	wl_list_insert(test_server.views.prev, &view2.link);

	/* Show client menu with pattern "fire" - should match firefox only */
	wm_menu_show_client_menu(&test_server, "fire", 10, 10);
	assert(test_server.client_menu != NULL);

	int count = 0;
	struct wm_menu_item *item;
	wl_list_for_each(item, &test_server.client_menu->items, link) {
		count++;
	}
	assert(count == 1);

	wl_list_remove(&ws0.link);
	wl_list_remove(&view1.link);
	wl_list_remove(&view2.link);
	free(ws0.name);
	free(view1.title);
	free(view1.app_id);
	free(view2.title);
	free(view2.app_id);

	wm_menu_destroy(test_server.client_menu);
	test_server.client_menu = NULL;
	printf("  PASS: client_menu_with_pattern\n");
}

/* --- Client menu creation with no pattern (all views) --- */

static void
test_client_menu_no_pattern(void)
{
	setup_server();

	struct wlr_scene_tree view_tree1;
	memset(&view_tree1, 0, sizeof(view_tree1));
	wl_list_init(&view_tree1.children);
	wl_list_init(&view_tree1.node.link);
	view_tree1.node.enabled = 1;

	struct wm_workspace ws0 = { .name = strdup("WS"), .index = 0 };
	wl_list_init(&ws0.link);
	wl_list_insert(test_server.workspaces.prev, &ws0.link);

	struct wm_view view1;
	memset(&view1, 0, sizeof(view1));
	view1.title = strdup("App");
	view1.app_id = strdup("app");
	view1.workspace = &ws0;
	view1.scene_tree = &view_tree1;
	wl_list_init(&view1.link);
	wl_list_insert(test_server.views.prev, &view1.link);

	wm_menu_show_client_menu(&test_server, NULL, 10, 10);
	assert(test_server.client_menu != NULL);

	int count = 0;
	struct wm_menu_item *item;
	wl_list_for_each(item, &test_server.client_menu->items, link) {
		count++;
	}
	assert(count == 1);

	wl_list_remove(&ws0.link);
	wl_list_remove(&view1.link);
	free(ws0.name);
	free(view1.title);
	free(view1.app_id);

	wm_menu_destroy(test_server.client_menu);
	test_server.client_menu = NULL;
	printf("  PASS: client_menu_no_pattern\n");
}

/* --- Show root: toggle behavior --- */

static void
test_show_root_toggle(void)
{
	setup_server();
	struct wm_menu *root = make_test_menu(2);
	test_server.root_menu = root;

	/* First call shows */
	wm_menu_show_root(&test_server, 10, 10);
	assert(root->visible == true);

	/* Second call hides (toggle) */
	wm_menu_show_root(&test_server, 10, 10);
	assert(root->visible == false);

	test_server.root_menu = NULL;
	wm_menu_destroy(root);
	printf("  PASS: show_root_toggle\n");
}

/* --- Key Return activates item --- */

static void
test_key_return_activates(void)
{
	setup_server();
	struct wm_menu *menu = make_test_menu(3);
	prepare_menu(menu, 100, 100);
	menu->scene_tree = wlr_scene_tree_create(&test_scene.tree);
	menu->selected_index = 0;

	bool consumed = wm_menu_handle_key_for(menu, XKB_KEY_Return);
	assert(consumed == true);
	/* Menu should be hidden after executing the item */
	assert(menu->visible == false);

	wm_menu_destroy(menu);
	printf("  PASS: key_return_activates\n");
}

/* --- Key Return on NOP item does nothing --- */

static void
test_key_return_nop_ignored(void)
{
	setup_server();
	struct wm_menu *menu = menu_create(&test_server, "Test");
	struct wm_menu_item *nop = menu_item_create(WM_MENU_NOP, "Info");
	wl_list_insert(menu->items.prev, &nop->link);
	prepare_menu(menu, 100, 100);
	menu->selected_index = 0;

	bool consumed = wm_menu_handle_key_for(menu, XKB_KEY_Return);
	assert(consumed == true);
	/* Menu should stay visible - NOP items are not activated */
	assert(menu->visible == true);

	wm_menu_destroy(menu);
	printf("  PASS: key_return_nop_ignored\n");
}

/* --- Key Return enters submenu --- */

static void
test_key_return_enters_submenu(void)
{
	setup_server();
	struct wm_menu *menu = make_menu_with_submenu();
	prepare_menu(menu, 100, 100);
	menu->selected_index = 1; /* submenu item */

	bool consumed = wm_menu_handle_key_for(menu, XKB_KEY_Return);
	assert(consumed == true);

	/* Submenu should be visible */
	struct wm_menu_item *sub = menu_get_item(menu, 1);
	assert(sub->submenu->visible == true);

	wm_menu_destroy(menu);
	printf("  PASS: key_return_enters_submenu\n");
}

/* --- Server-level key dispatcher --- */

static void
test_handle_key_dispatches_to_root(void)
{
	setup_server();
	struct wm_menu *root = make_test_menu(3);
	prepare_menu(root, 100, 100);
	test_server.root_menu = root;

	bool consumed = wm_menu_handle_key(&test_server, 0, XKB_KEY_Down, true);
	assert(consumed == true);
	assert(root->selected_index == 0);

	test_server.root_menu = NULL;
	wm_menu_destroy(root);
	printf("  PASS: handle_key_dispatches_to_root\n");
}

/* --- Server-level key: not pressed returns false --- */

static void
test_handle_key_not_pressed(void)
{
	setup_server();
	struct wm_menu *root = make_test_menu(3);
	prepare_menu(root, 100, 100);
	test_server.root_menu = root;

	bool consumed = wm_menu_handle_key(&test_server, 0, XKB_KEY_Down, false);
	assert(consumed == false);

	test_server.root_menu = NULL;
	wm_menu_destroy(root);
	printf("  PASS: handle_key_not_pressed\n");
}

/* --- Server-level button dispatcher --- */

static void
test_handle_button_dispatches(void)
{
	setup_server();
	struct wm_menu *root = make_test_menu(3);
	prepare_menu(root, 100, 100);
	root->scene_tree = wlr_scene_tree_create(&test_scene.tree);
	test_server.root_menu = root;

	/* Click outside: closes */
	bool consumed = wm_menu_handle_button(&test_server, 0, 0, 0x110, true);
	assert(consumed == false);
	assert(root->visible == false);

	test_server.root_menu = NULL;
	wm_menu_destroy(root);
	printf("  PASS: handle_button_dispatches\n");
}

/* --- Server-level motion dispatcher --- */

static void
test_handle_motion_dispatches(void)
{
	setup_server();
	struct wm_menu *root = make_test_menu(3);
	prepare_menu(root, 100, 100);
	test_server.root_menu = root;

	int bw = root->border_width;
	double lx = 100 + bw + 5;
	double ly = 100 + bw + root->title_height + 5;

	bool consumed = wm_menu_handle_motion(&test_server, lx, ly);
	assert(consumed == true);
	assert(root->selected_index == 0);

	test_server.root_menu = NULL;
	wm_menu_destroy(root);
	printf("  PASS: handle_motion_dispatches\n");
}

/* --- is_image_file helper --- */

static void
test_is_image_file(void)
{
	assert(is_image_file("photo.png") == true);
	assert(is_image_file("photo.PNG") == true);
	assert(is_image_file("photo.jpg") == true);
	assert(is_image_file("photo.jpeg") == true);
	assert(is_image_file("photo.bmp") == true);
	assert(is_image_file("photo.gif") == true);
	assert(is_image_file("photo.webp") == true);
	assert(is_image_file("photo.txt") == false);
	assert(is_image_file("noext") == false);
	assert(is_image_file("photo.c") == false);
	printf("  PASS: is_image_file\n");
}

/* --- Pixel buffer data ptr access --- */

static void
test_pixel_buffer_read_access(void)
{
	setup_server();
	/* Create a pixel buffer through wlr_buffer_from_cairo */
	cairo_surface_t *surface = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, 10, 10);
	struct wlr_buffer *buf = wlr_buffer_from_cairo(surface);
	assert(buf != NULL);
	assert(buf->width == 100); /* stub returns 100 for width */
	assert(buf->height == 100); /* stub returns 100 for height */

	/* Test read access */
	void *data;
	uint32_t format;
	size_t stride;
	bool ok = pixel_buffer_begin_data_ptr_access(buf, 0, &data, &format, &stride);
	assert(ok == true);
	assert(format == DRM_FORMAT_ARGB8888);
	assert(stride == 400); /* stub stride */

	/* Test write access fails */
	ok = pixel_buffer_begin_data_ptr_access(buf, WLR_BUFFER_DATA_PTR_ACCESS_WRITE,
		&data, &format, &stride);
	assert(ok == false);

	/* Clean up */
	pixel_buffer_destroy(buf);
	printf("  PASS: pixel_buffer_read_access\n");
}

/* --- submenu_position helper --- */

static void
test_submenu_position_basic(void)
{
	setup_server();
	struct wm_menu *menu = make_test_menu(3);
	prepare_menu(menu, 100, 100);

	int sub_x, sub_y;
	submenu_position(menu, 0, &sub_x, &sub_y);

	/* Without output clamping, submenu goes to the right */
	assert(sub_x == 100 + menu->width);
	assert(sub_y == 100 + menu->border_width + menu->title_height);

	wm_menu_destroy(menu);
	printf("  PASS: submenu_position_basic\n");
}

/* --- Menu file parsing: exec tag --- */

static void
test_parse_exec_tag(void)
{
	setup_server();

	/* Create a temp menu file */
	const char *tmpfile = "/tmp/fluxland_test_menu_exec.txt";
	FILE *fp = fopen(tmpfile, "w");
	assert(fp != NULL);
	fprintf(fp, "[begin] (Test Menu)\n");
	fprintf(fp, "[exec] (Terminal) {xterm}\n");
	fprintf(fp, "[exec] (Editor) {vim} <icon.png>\n");
	fprintf(fp, "[end]\n");
	fclose(fp);

	struct wm_menu *menu = wm_menu_load(&test_server, tmpfile);
	assert(menu != NULL);

	int count = 0;
	struct wm_menu_item *item;
	wl_list_for_each(item, &menu->items, link) {
		count++;
	}
	assert(count == 2);

	/* First item */
	item = menu_get_item(menu, 0);
	assert(item->type == WM_MENU_EXEC);
	assert(strcmp(item->label, "Terminal") == 0);
	assert(strcmp(item->command, "xterm") == 0);
	assert(item->icon_path == NULL);

	/* Second item with icon */
	item = menu_get_item(menu, 1);
	assert(item->type == WM_MENU_EXEC);
	assert(strcmp(item->label, "Editor") == 0);
	assert(strcmp(item->command, "vim") == 0);
	assert(item->icon_path != NULL);
	assert(strcmp(item->icon_path, "icon.png") == 0);

	wm_menu_destroy(menu);
	remove(tmpfile);
	printf("  PASS: parse_exec_tag\n");
}

/* --- Menu file parsing: submenu + separator + nop --- */

static void
test_parse_submenu_separator_nop(void)
{
	setup_server();

	const char *tmpfile = "/tmp/fluxland_test_menu_sub.txt";
	FILE *fp = fopen(tmpfile, "w");
	assert(fp != NULL);
	fprintf(fp, "[begin] (Root)\n");
	fprintf(fp, "[submenu] (Apps) {Applications}\n");
	fprintf(fp, "  [exec] (Firefox) {firefox}\n");
	fprintf(fp, "[end]\n");
	fprintf(fp, "[separator]\n");
	fprintf(fp, "[nop] (Info Line)\n");
	fprintf(fp, "[end]\n");
	fclose(fp);

	struct wm_menu *menu = wm_menu_load(&test_server, tmpfile);
	assert(menu != NULL);
	assert(strcmp(menu->title, "Root") == 0);

	int count = 0;
	struct wm_menu_item *item;
	wl_list_for_each(item, &menu->items, link) {
		count++;
	}
	assert(count == 3); /* submenu + separator + nop */

	/* Submenu item */
	item = menu_get_item(menu, 0);
	assert(item->type == WM_MENU_SUBMENU);
	assert(strcmp(item->label, "Apps") == 0);
	assert(item->submenu != NULL);
	assert(strcmp(item->submenu->title, "Applications") == 0);

	/* Verify submenu contents */
	int sub_count = 0;
	struct wm_menu_item *sub_item;
	wl_list_for_each(sub_item, &item->submenu->items, link) {
		sub_count++;
	}
	assert(sub_count == 1);

	/* Separator */
	item = menu_get_item(menu, 1);
	assert(item->type == WM_MENU_SEPARATOR);

	/* Nop */
	item = menu_get_item(menu, 2);
	assert(item->type == WM_MENU_NOP);
	assert(strcmp(item->label, "Info Line") == 0);

	wm_menu_destroy(menu);
	remove(tmpfile);
	printf("  PASS: parse_submenu_separator_nop\n");
}

/* --- Menu file parsing: simple tags (exit, reconfig, restart, etc.) --- */

static void
test_parse_simple_tags(void)
{
	setup_server();

	const char *tmpfile = "/tmp/fluxland_test_menu_simple.txt";
	FILE *fp = fopen(tmpfile, "w");
	assert(fp != NULL);
	fprintf(fp, "[begin] (Root)\n");
	fprintf(fp, "[exit] (Quit)\n");
	fprintf(fp, "[reconfig] (Reload)\n");
	fprintf(fp, "[restart] (Restart) {fluxland}\n");
	fprintf(fp, "[workspaces] (WS)\n");
	fprintf(fp, "[end]\n");
	fclose(fp);

	struct wm_menu *menu = wm_menu_load(&test_server, tmpfile);
	assert(menu != NULL);

	struct wm_menu_item *item;

	item = menu_get_item(menu, 0);
	assert(item->type == WM_MENU_EXIT);
	assert(strcmp(item->label, "Quit") == 0);

	item = menu_get_item(menu, 1);
	assert(item->type == WM_MENU_RECONFIG);
	assert(strcmp(item->label, "Reload") == 0);

	item = menu_get_item(menu, 2);
	assert(item->type == WM_MENU_RESTART);
	assert(strcmp(item->label, "Restart") == 0);
	assert(item->command != NULL);
	assert(strcmp(item->command, "fluxland") == 0);

	item = menu_get_item(menu, 3);
	assert(item->type == WM_MENU_WORKSPACES);

	wm_menu_destroy(menu);
	remove(tmpfile);
	printf("  PASS: parse_simple_tags\n");
}

/* --- Menu file parsing: comments and blank lines skipped --- */

static void
test_parse_comments_blanks(void)
{
	setup_server();

	const char *tmpfile = "/tmp/fluxland_test_menu_comments.txt";
	FILE *fp = fopen(tmpfile, "w");
	assert(fp != NULL);
	fprintf(fp, "[begin] (Root)\n");
	fprintf(fp, "# This is a comment\n");
	fprintf(fp, "\n");
	fprintf(fp, "   \n");
	fprintf(fp, "[exec] (Only) {true}\n");
	fprintf(fp, "[end]\n");
	fclose(fp);

	struct wm_menu *menu = wm_menu_load(&test_server, tmpfile);
	assert(menu != NULL);

	int count = 0;
	struct wm_menu_item *item;
	wl_list_for_each(item, &menu->items, link) {
		count++;
	}
	assert(count == 1);

	wm_menu_destroy(menu);
	remove(tmpfile);
	printf("  PASS: parse_comments_blanks\n");
}

/* --- Menu file parsing: style and command tags --- */

static void
test_parse_style_and_command_tags(void)
{
	setup_server();

	const char *tmpfile = "/tmp/fluxland_test_menu_style_cmd.txt";
	FILE *fp = fopen(tmpfile, "w");
	assert(fp != NULL);
	fprintf(fp, "[begin] (Root)\n");
	fprintf(fp, "[style] (Dark Theme) {/path/to/dark}\n");
	fprintf(fp, "[command] (Maximize) {Maximize}\n");
	fprintf(fp, "[end]\n");
	fclose(fp);

	struct wm_menu *menu = wm_menu_load(&test_server, tmpfile);
	assert(menu != NULL);

	struct wm_menu_item *item;

	item = menu_get_item(menu, 0);
	assert(item->type == WM_MENU_STYLE);
	assert(strcmp(item->label, "Dark Theme") == 0);
	assert(strcmp(item->command, "/path/to/dark") == 0);

	item = menu_get_item(menu, 1);
	assert(item->type == WM_MENU_COMMAND);
	assert(strcmp(item->label, "Maximize") == 0);
	assert(strcmp(item->command, "Maximize") == 0);

	wm_menu_destroy(menu);
	remove(tmpfile);
	printf("  PASS: parse_style_and_command_tags\n");
}

/* --- Menu load: null path --- */

static void
test_menu_load_null(void)
{
	struct wm_menu *menu = wm_menu_load(&test_server, NULL);
	assert(menu == NULL);
	printf("  PASS: menu_load_null\n");
}

/* --- Menu load: nonexistent file --- */

static void
test_menu_load_nonexistent(void)
{
	setup_server();
	struct wm_menu *menu = wm_menu_load(&test_server,
		"/tmp/fluxland_no_such_file_exists.txt");
	assert(menu == NULL);
	printf("  PASS: menu_load_nonexistent\n");
}

/* --- maybe_convert_label: null and UTF-8 passthrough --- */

static void
test_maybe_convert_label_passthrough(void)
{
	/* NULL label returns NULL */
	char *r = maybe_convert_label(NULL, "UTF-8");
	assert(r == NULL);

	/* NULL encoding returns label unchanged */
	char *label = strdup("Hello");
	r = maybe_convert_label(label, NULL);
	assert(r == label);
	assert(strcmp(r, "Hello") == 0);
	free(r);

	/* UTF-8 encoding returns label unchanged */
	label = strdup("World");
	r = maybe_convert_label(label, "UTF-8");
	assert(r == label);
	free(r);

	printf("  PASS: maybe_convert_label_passthrough\n");
}

/* --- search_timer_cb resets search buffer --- */

static void
test_search_timer_resets(void)
{
	setup_server();
	test_config.menu_search = WM_MENU_SEARCH_ITEMSTART;

	struct wm_menu *menu = menu_create(&test_server, "Test");
	struct wm_menu_item *i0 = menu_item_create(WM_MENU_EXEC, "Apple");
	wl_list_insert(menu->items.prev, &i0->link);
	prepare_menu(menu, 0, 0);

	menu_type_ahead(menu, 'a');
	assert(menu->search_len == 1);

	/* Simulate timer firing */
	search_timer_cb(menu);
	assert(menu->search_len == 0);
	assert(menu->search_buf[0] == '\0');

	wm_menu_destroy(menu);
	printf("  PASS: search_timer_resets\n");
}

/* --- Window menu: has SendTo and Layer submenus --- */

static void
test_window_menu_submenus(void)
{
	setup_server();

	struct wm_menu *menu = wm_menu_create_window_menu(&test_server);
	assert(menu != NULL);

	/* Find SendTo and Layer items and verify they have submenus */
	bool found_sendto = false;
	bool found_layer = false;
	struct wm_menu_item *item;
	wl_list_for_each(item, &menu->items, link) {
		if (item->type == WM_MENU_SENDTO) {
			found_sendto = true;
			/* SendTo gets a workspace submenu */
			assert(item->submenu != NULL);
		}
		if (item->type == WM_MENU_LAYER) {
			found_layer = true;
			assert(item->submenu != NULL);
			/* Layer submenu should have 5 entries */
			int layer_count = 0;
			struct wm_menu_item *li;
			wl_list_for_each(li, &item->submenu->items, link) {
				layer_count++;
			}
			assert(layer_count == 5);
		}
	}
	assert(found_sendto == true);
	assert(found_layer == true);

	wm_menu_destroy(menu);
	printf("  PASS: window_menu_submenus\n");
}

/* --- Menu compute layout: max width clamping --- */

static void
test_menu_layout_max_width(void)
{
	setup_server();
	struct wm_menu *menu = menu_create(&test_server, "Test");

	/* Create item with very long label */
	char long_label[256];
	memset(long_label, 'A', 255);
	long_label[255] = '\0';
	struct wm_menu_item *item = menu_item_create(WM_MENU_EXEC, long_label);
	item->command = strdup("test");
	wl_list_insert(menu->items.prev, &item->link);

	menu_compute_layout(menu, &test_style);
	/* Width should be capped at MENU_MAX_WIDTH */
	assert(menu->width <= MENU_MAX_WIDTH);

	wm_menu_destroy(menu);
	printf("  PASS: menu_layout_max_width\n");
}

/* --- parse_brace: nested braces --- */

static void
test_parse_brace_nested(void)
{
	const char *str = "{a{b}c}";
	const char *pos = str;
	char *result = parse_brace(&pos);
	assert(result != NULL);
	assert(strcmp(result, "a{b}c") == 0);
	free(result);
	printf("  PASS: parse_brace_nested\n");
}

/* --- parse_brace: no match --- */

static void
test_parse_brace_no_match(void)
{
	const char *str = "no braces";
	const char *pos = str;
	char *result = parse_brace(&pos);
	assert(result == NULL);
	printf("  PASS: parse_brace_no_match\n");
}

/* --- parse_angle: no match --- */

static void
test_parse_angle_no_match(void)
{
	const char *str = "no angles";
	const char *pos = str;
	char *result = parse_angle(&pos);
	assert(result == NULL);
	printf("  PASS: parse_angle_no_match\n");
}

/* ===================== Main ===================== */

int
main(void)
{
	printf("test_menu_interaction:\n");

	/* String helpers */
	test_strip_whitespace_basic();
	test_strip_whitespace_no_whitespace();
	test_strip_whitespace_all_whitespace();
	test_expand_path_tilde();
	test_expand_path_no_tilde();
	test_expand_path_null();
	test_expand_path_tilde_only();

	/* Parser helpers */
	test_parse_paren();
	test_parse_paren_nested();
	test_parse_brace();
	test_parse_angle();
	test_parse_paren_no_match();

	/* Menu creation/layout */
	test_menu_create();
	test_menu_item_create();
	test_menu_compute_layout();
	test_menu_compute_layout_with_separator();

	/* Hit testing */
	test_menu_item_at_first();
	test_menu_item_at_last();
	test_menu_item_at_outside();
	test_menu_contains_point();
	test_menu_contains_point_invisible();

	/* Key navigation */
	test_key_down_selects_first();
	test_key_down_wraps();
	test_key_up_wraps();
	test_key_skips_separator();
	test_key_up_skips_separator();
	test_key_escape_hides_menu();
	test_key_right_enters_submenu();
	test_key_left_leaves_submenu();
	test_key_not_visible();

	/* Type-ahead */
	test_type_ahead_itemstart();
	test_type_ahead_somewhere();
	test_type_ahead_nowhere();
	test_type_ahead_skips_separator();

	/* Window menu */
	test_create_window_menu();

	/* Utility functions */
	test_menu_get_item();
	test_find_deepest_visible();
	test_find_menu_at();
	test_find_base_dir_null();
	test_find_base_dir_ltr();
	test_color_to_float4();

	/* Lifecycle */
	test_menu_destroy_null();
	test_menu_destroy_recursive();

	/* Vim-style keys */
	test_key_j_moves_down();
	test_key_k_moves_up();

	/* Execute menu item actions */
	test_execute_exit();
	test_execute_style();
	test_execute_stick();
	test_execute_raise();
	test_execute_lower();
	test_execute_command();
	test_execute_remember();
	test_execute_null_safety();

	/* Button interaction */
	test_button_activates_item();
	test_button_outside_closes();
	test_button_on_title();
	test_button_toggle_submenu();
	test_button_release_in_menu();

	/* Motion interaction */
	test_motion_updates_selection();
	test_motion_outside_menu();

	/* Show/hide lifecycle */
	test_menu_show_hide_lifecycle();
	test_menu_show_reshow();
	test_menu_hide_all();
	test_menu_hide_clears_search();

	/* Menu generation */
	test_create_workspace_menu();
	test_create_window_list();
	test_client_menu_with_pattern();
	test_client_menu_no_pattern();

	/* Show root toggle */
	test_show_root_toggle();

	/* Key Return activation */
	test_key_return_activates();
	test_key_return_nop_ignored();
	test_key_return_enters_submenu();

	/* Server-level dispatchers */
	test_handle_key_dispatches_to_root();
	test_handle_key_not_pressed();
	test_handle_button_dispatches();
	test_handle_motion_dispatches();

	/* Helper functions */
	test_is_image_file();
	test_pixel_buffer_read_access();
	test_submenu_position_basic();

	/* Menu file parsing */
	test_parse_exec_tag();
	test_parse_submenu_separator_nop();
	test_parse_simple_tags();
	test_parse_comments_blanks();
	test_parse_style_and_command_tags();
	test_menu_load_null();
	test_menu_load_nonexistent();

	/* Encoding conversion */
	test_maybe_convert_label_passthrough();

	/* Search timer */
	test_search_timer_resets();

	/* Window menu details */
	test_window_menu_submenus();

	/* Layout edge cases */
	test_menu_layout_max_width();

	/* Parser edge cases */
	test_parse_brace_nested();
	test_parse_brace_no_match();
	test_parse_angle_no_match();

	printf("All menu interaction tests passed.\n");
	return 0;
}
