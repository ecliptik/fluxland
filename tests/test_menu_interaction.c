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
#define WM_KEYBOARD_ACTIONS_H
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
	int focus_policy;
	bool focus_new_windows;
	bool opaque_move;
	bool opaque_resize;
	bool workspace_warping;
	bool full_maximization;
	char *style_file;
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
	int content_height;
	bool shaded;
};

struct wm_view {
	struct wl_list link;
	struct wm_server *server;
	struct wlr_xdg_toplevel *xdg_toplevel;
	struct wlr_scene_tree *scene_tree;
	uint32_t id;
	int x, y;
	bool sticky;
	bool maximized;
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
	WM_IPC_EVENT_FOCUS_CHANGED = 1,
	WM_IPC_EVENT_CONFIG_RELOADED = 2,
	WM_IPC_EVENT_STYLE_CHANGED = 3,
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
	int focus_policy;
	struct wm_toolbar *toolbar;
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

static struct wlr_output g_stub_wlr_output_obj;
static struct wlr_output *g_stub_output_at_result = NULL;

static struct wlr_output *
wlr_output_layout_output_at(struct wlr_output_layout *layout,
	double lx, double ly)
{
	(void)layout; (void)lx; (void)ly;
	return g_stub_output_at_result;
}

static struct wlr_box g_stub_output_box = {0, 0, 0, 0};

static void wlr_output_layout_get_box(struct wlr_output_layout *layout,
	struct wlr_output *output, struct wlr_box *box)
{
	(void)layout; (void)output;
	*box = g_stub_output_box;
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
#define XKB_KEY_B         0x0042

/* --- Include menu.c --- */

/* iconv stub: just return label unchanged */
#include <iconv.h>

void wm_spawn_command(const char *cmd) { (void)cmd; }

/* --- Action dispatch stubs (for menu.c WM_MENU_COMMAND dispatch) --- */

enum wm_action {
	WM_ACTION_NOP = 0,
	WM_ACTION_MAXIMIZE,
	WM_ACTION_MINIMIZE,
	WM_ACTION_SHADE,
	WM_ACTION_RECONFIGURE,
};

static enum wm_action wm_action_from_name(const char *name)
{
	(void)name;
	return WM_ACTION_NOP;
}

static bool wm_execute_action(struct wm_server *server,
	enum wm_action action, const char *argument)
{
	(void)server; (void)action; (void)argument;
	return true;
}

static void wm_server_reconfigure(struct wm_server *server)
{
	(void)server;
}

struct wm_toolbar;
static void wm_toolbar_relayout(struct wm_toolbar *toolbar)
{
	(void)toolbar;
}

static void wm_decoration_update(struct wm_decoration *deco,
	struct wm_style *style)
{
	(void)deco; (void)style;
}

/* --- Perf stubs (menu_render.c includes perf.h) --- */
#include "perf.h"
#ifdef WM_PERF_ENABLE
void wm_perf_probe_init(struct wm_perf_probe *probe, const char *label) {
	(void)probe; (void)label;
}
void wm_perf_probe_record(struct wm_perf_probe *probe, uint64_t ns) {
	(void)probe; (void)ns;
}
void wm_perf_probe_report(struct wm_perf_probe *probe) {
	(void)probe;
}
void wm_perf_probe_reset(struct wm_perf_probe *probe) {
	(void)probe;
}
#endif

/* find_base_dir is now extern in render.h — provide stub using test's pango stubs */
PangoDirection find_base_dir(const char *text)
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

#include "pixel_buffer.c"
#include "menu_parse.c"
#include "menu_render.c"
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

/* --- Execute menu item: reconfig --- */

static void
test_execute_reconfig(void)
{
	setup_server();
	struct wm_menu *menu = menu_create(&test_server, "Test");
	struct wm_menu_item *item = menu_item_create(WM_MENU_RECONFIG, "Reconfig");
	wl_list_insert(menu->items.prev, &item->link);

	execute_menu_item(menu, item);
	/* Should not crash; just logs */

	wm_menu_destroy(menu);
	printf("  PASS: execute_reconfig\n");
}

/* --- Execute menu item: restart --- */

static void
test_execute_restart(void)
{
	setup_server();
	struct wm_menu *menu = menu_create(&test_server, "Test");
	struct wm_menu_item *item = menu_item_create(WM_MENU_RESTART, "Restart");
	item->command = strdup("fluxland");
	wl_list_insert(menu->items.prev, &item->link);

	execute_menu_item(menu, item);
	/* Should not crash; just logs */

	wm_menu_destroy(menu);
	printf("  PASS: execute_restart\n");
}

/* --- Execute menu item: shade (with decoration) --- */

static void
test_execute_shade(void)
{
	setup_server();
	struct wm_view view;
	memset(&view, 0, sizeof(view));
	struct wm_decoration deco;
	memset(&deco, 0, sizeof(deco));
	view.decoration = &deco;
	test_server.focused_view = &view;

	struct wm_menu *menu = menu_create(&test_server, "Test");
	struct wm_menu_item *item = menu_item_create(WM_MENU_SHADE, "Shade");
	wl_list_insert(menu->items.prev, &item->link);

	execute_menu_item(menu, item);
	/* Should not crash; just logs toggle shade */

	test_server.focused_view = NULL;
	wm_menu_destroy(menu);
	printf("  PASS: execute_shade\n");
}

/* --- Execute menu item: shade (no decoration, no focused view) --- */

static void
test_execute_shade_no_view(void)
{
	setup_server();
	test_server.focused_view = NULL;

	struct wm_menu *menu = menu_create(&test_server, "Test");
	struct wm_menu_item *item = menu_item_create(WM_MENU_SHADE, "Shade");
	wl_list_insert(menu->items.prev, &item->link);

	execute_menu_item(menu, item);
	/* Should not crash; no focused_view */

	wm_menu_destroy(menu);
	printf("  PASS: execute_shade_no_view\n");
}

/* --- Execute menu item: maximize --- */

static void
test_execute_maximize(void)
{
	setup_server();

	struct wm_menu *menu = menu_create(&test_server, "Test");
	struct wm_menu_item *item = menu_item_create(WM_MENU_MAXIMIZE, "Maximize");
	wl_list_insert(menu->items.prev, &item->link);

	execute_menu_item(menu, item);
	/* Should not crash; currently a stub */

	wm_menu_destroy(menu);
	printf("  PASS: execute_maximize\n");
}

/* --- Execute menu item: iconify --- */

static void
test_execute_iconify(void)
{
	setup_server();

	struct wm_menu *menu = menu_create(&test_server, "Test");
	struct wm_menu_item *item = menu_item_create(WM_MENU_ICONIFY, "Iconify");
	wl_list_insert(menu->items.prev, &item->link);

	execute_menu_item(menu, item);
	/* Should not crash; currently a stub */

	wm_menu_destroy(menu);
	printf("  PASS: execute_iconify\n");
}

/* --- Execute menu item: close --- */

static void
test_execute_close(void)
{
	setup_server();
	struct wm_view view;
	memset(&view, 0, sizeof(view));
	struct wlr_xdg_toplevel toplevel;
	memset(&toplevel, 0, sizeof(toplevel));
	view.xdg_toplevel = &toplevel;
	test_server.focused_view = &view;

	struct wm_menu *menu = menu_create(&test_server, "Test");
	struct wm_menu_item *item = menu_item_create(WM_MENU_CLOSE, "Close");
	wl_list_insert(menu->items.prev, &item->link);

	execute_menu_item(menu, item);
	/* Should call wlr_xdg_toplevel_send_close stub */

	test_server.focused_view = NULL;
	wm_menu_destroy(menu);
	printf("  PASS: execute_close\n");
}

/* --- Execute menu item: close (no focused view) --- */

static void
test_execute_close_no_view(void)
{
	setup_server();
	test_server.focused_view = NULL;

	struct wm_menu *menu = menu_create(&test_server, "Test");
	struct wm_menu_item *item = menu_item_create(WM_MENU_CLOSE, "Close");
	wl_list_insert(menu->items.prev, &item->link);

	execute_menu_item(menu, item);
	/* Should not crash */

	wm_menu_destroy(menu);
	printf("  PASS: execute_close_no_view\n");
}

/* --- Execute menu item: kill --- */

static void
test_execute_kill(void)
{
	setup_server();
	struct wm_view view;
	memset(&view, 0, sizeof(view));
	struct wlr_xdg_toplevel toplevel;
	memset(&toplevel, 0, sizeof(toplevel));
	struct wlr_xdg_surface xdg_surface;
	memset(&xdg_surface, 0, sizeof(xdg_surface));
	struct wl_resource resource;
	memset(&resource, 0, sizeof(resource));
	xdg_surface.resource = &resource;
	toplevel.base = &xdg_surface;
	view.xdg_toplevel = &toplevel;
	test_server.focused_view = &view;

	struct wm_menu *menu = menu_create(&test_server, "Test");
	struct wm_menu_item *item = menu_item_create(WM_MENU_KILL, "Kill");
	wl_list_insert(menu->items.prev, &item->link);

	execute_menu_item(menu, item);
	/* Should call wl_resource_get_client + wl_client_destroy stubs */

	test_server.focused_view = NULL;
	wm_menu_destroy(menu);
	printf("  PASS: execute_kill\n");
}

/* --- Execute menu item: kill (no focused view) --- */

static void
test_execute_kill_no_view(void)
{
	setup_server();
	test_server.focused_view = NULL;

	struct wm_menu *menu = menu_create(&test_server, "Test");
	struct wm_menu_item *item = menu_item_create(WM_MENU_KILL, "Kill");
	wl_list_insert(menu->items.prev, &item->link);

	execute_menu_item(menu, item);
	/* Should not crash */

	wm_menu_destroy(menu);
	printf("  PASS: execute_kill_no_view\n");
}

/* --- Execute menu item: window entry (valid target) --- */

static void
test_execute_window_entry(void)
{
	setup_server();

	struct wlr_scene_tree view_tree;
	memset(&view_tree, 0, sizeof(view_tree));
	wl_list_init(&view_tree.children);
	wl_list_init(&view_tree.node.link);
	view_tree.node.enabled = 1;

	struct wlr_xdg_surface xdg_surface;
	memset(&xdg_surface, 0, sizeof(xdg_surface));
	struct wlr_surface surface;
	memset(&surface, 0, sizeof(surface));
	xdg_surface.surface = &surface;

	struct wlr_xdg_toplevel toplevel;
	memset(&toplevel, 0, sizeof(toplevel));
	toplevel.base = &xdg_surface;

	struct wm_view target;
	memset(&target, 0, sizeof(target));
	target.scene_tree = &view_tree;
	target.xdg_toplevel = &toplevel;
	target.title = strdup("Target");
	wl_list_init(&target.link);
	wl_list_insert(test_server.views.prev, &target.link);

	struct wm_menu *menu = menu_create(&test_server, "Test");
	struct wm_menu_item *item = menu_item_create(WM_MENU_WINDOW_ENTRY, "Target");
	item->data = &target;
	item->command = strdup("0");
	wl_list_insert(menu->items.prev, &item->link);

	execute_menu_item(menu, item);
	/* Should switch workspace, focus and raise target */

	wl_list_remove(&target.link);
	free(target.title);
	wm_menu_destroy(menu);
	printf("  PASS: execute_window_entry\n");
}

/* --- Execute menu item: window entry (iconified target) --- */

static void
test_execute_window_entry_iconified(void)
{
	setup_server();

	struct wlr_scene_tree view_tree;
	memset(&view_tree, 0, sizeof(view_tree));
	wl_list_init(&view_tree.children);
	wl_list_init(&view_tree.node.link);
	view_tree.node.enabled = 0; /* iconified */

	struct wlr_xdg_surface xdg_surface;
	memset(&xdg_surface, 0, sizeof(xdg_surface));
	struct wlr_surface surface;
	memset(&surface, 0, sizeof(surface));
	xdg_surface.surface = &surface;

	struct wlr_xdg_toplevel toplevel;
	memset(&toplevel, 0, sizeof(toplevel));
	toplevel.base = &xdg_surface;

	struct wm_view target;
	memset(&target, 0, sizeof(target));
	target.scene_tree = &view_tree;
	target.xdg_toplevel = &toplevel;
	target.title = strdup("Iconified");
	wl_list_init(&target.link);
	wl_list_insert(test_server.views.prev, &target.link);

	struct wm_menu *menu = menu_create(&test_server, "Test");
	struct wm_menu_item *item = menu_item_create(WM_MENU_WINDOW_ENTRY, "Iconified");
	item->data = &target;
	item->command = strdup("0");
	wl_list_insert(menu->items.prev, &item->link);

	execute_menu_item(menu, item);
	/* Should de-iconify: enable node and call foreign_toplevel_set_minimized */
	assert(view_tree.node.enabled == 1);

	wl_list_remove(&target.link);
	free(target.title);
	wm_menu_destroy(menu);
	printf("  PASS: execute_window_entry_iconified\n");
}

/* --- Execute menu item: window entry (null target) --- */

static void
test_execute_window_entry_null(void)
{
	setup_server();

	struct wm_menu *menu = menu_create(&test_server, "Test");
	struct wm_menu_item *item = menu_item_create(WM_MENU_WINDOW_ENTRY, "None");
	item->data = NULL;
	wl_list_insert(menu->items.prev, &item->link);

	execute_menu_item(menu, item);
	/* Should not crash; null target early exit */

	wm_menu_destroy(menu);
	printf("  PASS: execute_window_entry_null\n");
}

/* --- Execute menu item: window entry (stale view no longer in list) --- */

static void
test_execute_window_entry_stale(void)
{
	setup_server();

	struct wm_view stale;
	memset(&stale, 0, sizeof(stale));
	/* stale view NOT in server views list */

	struct wm_menu *menu = menu_create(&test_server, "Test");
	struct wm_menu_item *item = menu_item_create(WM_MENU_WINDOW_ENTRY, "Stale");
	item->data = &stale;
	item->command = strdup("0");
	wl_list_insert(menu->items.prev, &item->link);

	execute_menu_item(menu, item);
	/* Should not crash; view not found in list */

	wm_menu_destroy(menu);
	printf("  PASS: execute_window_entry_stale\n");
}

/* --- submenu_position: flip when overflowing right edge --- */

static void
test_submenu_position_flip(void)
{
	setup_server();

	/* Set up an output so submenu_position detects boundary */
	struct wm_output wm_out;
	memset(&wm_out, 0, sizeof(wm_out));
	wm_out.wlr_output = &g_stub_wlr_output_obj;
	wm_out.usable_area.x = 0;
	wm_out.usable_area.y = 0;
	wm_out.usable_area.width = 500;
	wm_out.usable_area.height = 400;
	wl_list_init(&wm_out.link);
	wl_list_insert(test_server.outputs.prev, &wm_out.link);

	/* Make the output_at stub return our output */
	g_stub_output_at_result = &g_stub_wlr_output_obj;
	g_stub_output_box = (struct wlr_box){0, 0, 500, 400};

	struct wm_menu *menu = make_test_menu(3);
	prepare_menu(menu, 400, 50); /* x=400, close to right edge */

	int sub_x, sub_y;
	submenu_position(menu, 0, &sub_x, &sub_y);

	/* Menu width ~128, at x=400. sub_x = 400 + 128 = 528 > 500, so should flip.
	 * Flipped: sub_x = 400 - 128 = 272 */
	assert(sub_x < 400); /* Should have flipped to left side */

	/* Reset stubs */
	g_stub_output_at_result = NULL;
	g_stub_output_box = (struct wlr_box){0, 0, 0, 0};
	wl_list_remove(&wm_out.link);
	wm_menu_destroy(menu);
	printf("  PASS: submenu_position_flip\n");
}

/* --- strcmp_qsort comparison function --- */

static void
test_strcmp_qsort(void)
{
	const char *arr[] = {"cherry", "apple", "banana"};
	qsort(arr, 3, sizeof(arr[0]), strcmp_qsort);
	assert(strcmp(arr[0], "apple") == 0);
	assert(strcmp(arr[1], "banana") == 0);
	assert(strcmp(arr[2], "cherry") == 0);
	printf("  PASS: strcmp_qsort\n");
}

/* --- add_stylesdir_entries: scans directory --- */

static void
test_add_stylesdir_entries(void)
{
	setup_server();

	/* Create a temp directory with some "style" files */
	const char *tmpdir = "/tmp/fluxland_test_stylesdir";
	char cmd[256];
	snprintf(cmd, sizeof(cmd),
		"rm -rf %s && mkdir -p %s", tmpdir, tmpdir);
	system(cmd);

	/* Create some files */
	snprintf(cmd, sizeof(cmd), "touch %s/dark %s/light", tmpdir, tmpdir);
	system(cmd);

	struct wm_menu *menu = menu_create(&test_server, "Styles");
	add_stylesdir_entries(menu, tmpdir);

	int count = 0;
	struct wm_menu_item *item;
	wl_list_for_each(item, &menu->items, link) {
		count++;
		assert(item->type == WM_MENU_STYLE);
		assert(item->command != NULL);
	}
	assert(count == 2);

	/* Clean up */
	snprintf(cmd, sizeof(cmd), "rm -rf %s", tmpdir);
	system(cmd);
	wm_menu_destroy(menu);
	printf("  PASS: add_stylesdir_entries\n");
}

/* --- add_stylesdir_entries: nonexistent directory --- */

static void
test_add_stylesdir_nonexistent(void)
{
	setup_server();

	struct wm_menu *menu = menu_create(&test_server, "Styles");
	add_stylesdir_entries(menu, "/tmp/fluxland_no_such_dir_12345");

	/* No entries added */
	assert(wl_list_empty(&menu->items));

	wm_menu_destroy(menu);
	printf("  PASS: add_stylesdir_nonexistent\n");
}

/* --- add_wallpaper_entries: scans for image files --- */

static void
test_add_wallpaper_entries(void)
{
	setup_server();

	const char *tmpdir = "/tmp/fluxland_test_wallpapers";
	char cmd[256];
	snprintf(cmd, sizeof(cmd),
		"rm -rf %s && mkdir -p %s", tmpdir, tmpdir);
	system(cmd);

	/* Create some image and non-image files */
	snprintf(cmd, sizeof(cmd),
		"touch %s/beach.png %s/mountain.jpg %s/readme.txt %s/.hidden.png",
		tmpdir, tmpdir, tmpdir, tmpdir);
	system(cmd);

	struct wm_menu *menu = menu_create(&test_server, "Wallpapers");
	add_wallpaper_entries(menu, tmpdir);

	int count = 0;
	struct wm_menu_item *item;
	wl_list_for_each(item, &menu->items, link) {
		count++;
		assert(item->type == WM_MENU_EXEC);
		assert(item->command != NULL);
		/* Commands should contain swaybg */
		assert(strstr(item->command, "swaybg") != NULL);
	}
	/* Should find beach.png and mountain.jpg (not .txt, not .hidden.png) */
	assert(count == 2);

	snprintf(cmd, sizeof(cmd), "rm -rf %s", tmpdir);
	system(cmd);
	wm_menu_destroy(menu);
	printf("  PASS: add_wallpaper_entries\n");
}

/* --- add_wallpaper_entries: empty directory shows (empty) --- */

static void
test_add_wallpaper_entries_empty(void)
{
	setup_server();

	const char *tmpdir = "/tmp/fluxland_test_wall_empty";
	char cmd[256];
	snprintf(cmd, sizeof(cmd),
		"rm -rf %s && mkdir -p %s", tmpdir, tmpdir);
	system(cmd);

	struct wm_menu *menu = menu_create(&test_server, "Wallpapers");
	add_wallpaper_entries(menu, tmpdir);

	/* Should have one NOP "(empty)" entry */
	int count = 0;
	struct wm_menu_item *item;
	wl_list_for_each(item, &menu->items, link) {
		count++;
		assert(item->type == WM_MENU_NOP);
		assert(strcmp(item->label, "(empty)") == 0);
	}
	assert(count == 1);

	snprintf(cmd, sizeof(cmd), "rm -rf %s", tmpdir);
	system(cmd);
	wm_menu_destroy(menu);
	printf("  PASS: add_wallpaper_entries_empty\n");
}

/* --- add_wallpaper_entries: nonexistent directory shows (empty) --- */

static void
test_add_wallpaper_entries_nonexistent(void)
{
	setup_server();

	struct wm_menu *menu = menu_create(&test_server, "Wallpapers");
	add_wallpaper_entries(menu, "/tmp/fluxland_no_such_wall_dir");

	int count = 0;
	struct wm_menu_item *item;
	wl_list_for_each(item, &menu->items, link) {
		count++;
		assert(item->type == WM_MENU_NOP);
	}
	assert(count == 1);

	wm_menu_destroy(menu);
	printf("  PASS: add_wallpaper_entries_nonexistent\n");
}

/* --- add_wallpaper_entries: path with single quote rejected --- */

static void
test_add_wallpaper_entries_quote_path(void)
{
	setup_server();

	struct wm_menu *menu = menu_create(&test_server, "Wallpapers");
	/* Path containing single quote should be rejected */
	add_wallpaper_entries(menu, "/tmp/fluxland_it's_bad");

	int count = 0;
	struct wm_menu_item *item;
	wl_list_for_each(item, &menu->items, link) {
		count++;
		assert(item->type == WM_MENU_NOP);
		assert(strcmp(item->label, "(invalid path)") == 0);
	}
	assert(count == 1);

	wm_menu_destroy(menu);
	printf("  PASS: add_wallpaper_entries_quote_path\n");
}

/* --- add_stylesmenu_entries: scans directory --- */

static void
test_add_stylesmenu_entries(void)
{
	setup_server();

	const char *tmpdir = "/tmp/fluxland_test_stylesmenu";
	char cmd[256];
	snprintf(cmd, sizeof(cmd),
		"rm -rf %s && mkdir -p %s", tmpdir, tmpdir);
	system(cmd);

	snprintf(cmd, sizeof(cmd),
		"touch %s/modern %s/classic %s/.hidden",
		tmpdir, tmpdir, tmpdir);
	system(cmd);

	struct wm_menu *menu = menu_create(&test_server, "Styles");
	add_stylesmenu_entries(menu, tmpdir);

	int count = 0;
	struct wm_menu_item *item;
	wl_list_for_each(item, &menu->items, link) {
		count++;
		assert(item->type == WM_MENU_STYLE);
		assert(item->command != NULL);
	}
	/* Should find modern and classic (not .hidden) */
	assert(count == 2);

	snprintf(cmd, sizeof(cmd), "rm -rf %s", tmpdir);
	system(cmd);
	wm_menu_destroy(menu);
	printf("  PASS: add_stylesmenu_entries\n");
}

/* --- add_stylesmenu_entries: nonexistent directory --- */

static void
test_add_stylesmenu_nonexistent(void)
{
	setup_server();

	struct wm_menu *menu = menu_create(&test_server, "Styles");
	add_stylesmenu_entries(menu, "/tmp/fluxland_no_such_styles_dir");

	/* No entries added when dir doesn't exist */
	assert(wl_list_empty(&menu->items));

	wm_menu_destroy(menu);
	printf("  PASS: add_stylesmenu_nonexistent\n");
}

/* --- parse_menu_items_depth: encoding block --- */

static void
test_parse_encoding_block(void)
{
	setup_server();

	const char *tmpfile = "/tmp/fluxland_test_menu_encoding.txt";
	FILE *fp = fopen(tmpfile, "w");
	assert(fp != NULL);
	fprintf(fp, "[begin] (Test)\n");
	fprintf(fp, "[encoding] {ISO-8859-1}\n");
	fprintf(fp, "[exec] (Hello) {echo hi}\n");
	fprintf(fp, "[endencoding]\n");
	fprintf(fp, "[exec] (World) {echo world}\n");
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

	wm_menu_destroy(menu);
	remove(tmpfile);
	printf("  PASS: parse_encoding_block\n");
}

/* --- parse_menu_items_depth: include (valid file under /tmp) --- */

static void
test_parse_include(void)
{
	setup_server();

	/* Create the included file */
	const char *inc_file = "/tmp/fluxland_test_menu_inc_part.txt";
	FILE *fp = fopen(inc_file, "w");
	assert(fp != NULL);
	fprintf(fp, "[exec] (Included) {echo included}\n");
	fclose(fp);

	/* Create the main menu file that includes it */
	const char *tmpfile = "/tmp/fluxland_test_menu_include.txt";
	fp = fopen(tmpfile, "w");
	assert(fp != NULL);
	fprintf(fp, "[begin] (Test)\n");
	fprintf(fp, "[include] (%s)\n", inc_file);
	fprintf(fp, "[exec] (Main) {echo main}\n");
	fprintf(fp, "[end]\n");
	fclose(fp);

	struct wm_menu *menu = wm_menu_load(&test_server, tmpfile);
	assert(menu != NULL);

	int count = 0;
	struct wm_menu_item *item;
	wl_list_for_each(item, &menu->items, link) {
		count++;
	}
	assert(count == 2); /* included + main */

	/* Verify included item */
	item = menu_get_item(menu, 0);
	assert(item->type == WM_MENU_EXEC);
	assert(strcmp(item->label, "Included") == 0);

	wm_menu_destroy(menu);
	remove(inc_file);
	remove(tmpfile);
	printf("  PASS: parse_include\n");
}

/* --- parse_menu_items_depth: include with '..' rejected --- */

static void
test_parse_include_dotdot(void)
{
	setup_server();

	const char *tmpfile = "/tmp/fluxland_test_menu_dotdot.txt";
	FILE *fp = fopen(tmpfile, "w");
	assert(fp != NULL);
	fprintf(fp, "[begin] (Test)\n");
	fprintf(fp, "[include] (/tmp/../etc/passwd)\n");
	fprintf(fp, "[exec] (Only) {echo only}\n");
	fprintf(fp, "[end]\n");
	fclose(fp);

	struct wm_menu *menu = wm_menu_load(&test_server, tmpfile);
	assert(menu != NULL);

	/* Only the exec item, include was rejected */
	int count = 0;
	struct wm_menu_item *item;
	wl_list_for_each(item, &menu->items, link) {
		count++;
	}
	assert(count == 1);

	wm_menu_destroy(menu);
	remove(tmpfile);
	printf("  PASS: parse_include_dotdot\n");
}

/* --- parse_menu_items_depth: max depth limit --- */

static void
test_parse_max_depth(void)
{
	setup_server();

	/* Create a file that includes itself recursively */
	const char *tmpfile = "/tmp/fluxland_test_menu_depth.txt";
	FILE *fp = fopen(tmpfile, "w");
	assert(fp != NULL);
	fprintf(fp, "[exec] (Item) {echo item}\n");
	fprintf(fp, "[include] (%s)\n", tmpfile);
	fclose(fp);

	/* Load it at depth 0 - it will recurse until MENU_MAX_DEPTH */
	struct wm_menu *menu = menu_create(&test_server, "Depth Test");
	fp = fopen(tmpfile, "r");
	assert(fp != NULL);
	parse_menu_items_depth(menu, fp, &test_server, 0);
	fclose(fp);

	/* Should have some items but not infinite (capped by depth) */
	int count = 0;
	struct wm_menu_item *item;
	wl_list_for_each(item, &menu->items, link) {
		count++;
	}
	assert(count > 0);
	assert(count < 1000); /* Shouldn't be infinite */

	wm_menu_destroy(menu);
	remove(tmpfile);
	printf("  PASS: parse_max_depth\n");
}

/* --- parse_menu_items_depth: stylesdir tag --- */

static void
test_parse_stylesdir_tag(void)
{
	setup_server();

	const char *tmpdir = "/tmp/fluxland_test_parse_stylesdir";
	char cmd[256];
	snprintf(cmd, sizeof(cmd),
		"rm -rf %s && mkdir -p %s && touch %s/dark %s/light",
		tmpdir, tmpdir, tmpdir, tmpdir);
	system(cmd);

	const char *tmpfile = "/tmp/fluxland_test_menu_stylesdir.txt";
	FILE *fp = fopen(tmpfile, "w");
	assert(fp != NULL);
	fprintf(fp, "[begin] (Root)\n");
	fprintf(fp, "[stylesdir] (%s)\n", tmpdir);
	fprintf(fp, "[end]\n");
	fclose(fp);

	struct wm_menu *menu = wm_menu_load(&test_server, tmpfile);
	assert(menu != NULL);

	int count = 0;
	struct wm_menu_item *item;
	wl_list_for_each(item, &menu->items, link) {
		count++;
		assert(item->type == WM_MENU_STYLE);
	}
	assert(count == 2);

	snprintf(cmd, sizeof(cmd), "rm -rf %s", tmpdir);
	system(cmd);
	wm_menu_destroy(menu);
	remove(tmpfile);
	printf("  PASS: parse_stylesdir_tag\n");
}

/* --- parse_menu_items_depth: wallpapers tag --- */

static void
test_parse_wallpapers_tag(void)
{
	setup_server();

	const char *tmpdir = "/tmp/fluxland_test_parse_wallpapers";
	char cmd[256];
	snprintf(cmd, sizeof(cmd),
		"rm -rf %s && mkdir -p %s && touch %s/sky.png",
		tmpdir, tmpdir, tmpdir);
	system(cmd);

	const char *tmpfile = "/tmp/fluxland_test_menu_wallpapers.txt";
	FILE *fp = fopen(tmpfile, "w");
	assert(fp != NULL);
	fprintf(fp, "[begin] (Root)\n");
	fprintf(fp, "[wallpapers] (Backgrounds) {%s}\n", tmpdir);
	fprintf(fp, "[end]\n");
	fclose(fp);

	struct wm_menu *menu = wm_menu_load(&test_server, tmpfile);
	assert(menu != NULL);

	/* Should have one submenu item */
	struct wm_menu_item *item = menu_get_item(menu, 0);
	assert(item != NULL);
	assert(item->type == WM_MENU_SUBMENU);
	assert(strcmp(item->label, "Backgrounds") == 0);
	assert(item->submenu != NULL);

	/* Submenu should have wallpaper entries */
	int sub_count = 0;
	struct wm_menu_item *sub;
	wl_list_for_each(sub, &item->submenu->items, link) {
		sub_count++;
	}
	assert(sub_count == 1); /* sky.png */

	snprintf(cmd, sizeof(cmd), "rm -rf %s", tmpdir);
	system(cmd);
	wm_menu_destroy(menu);
	remove(tmpfile);
	printf("  PASS: parse_wallpapers_tag\n");
}

/* --- parse_menu_items_depth: stylesmenu tag --- */

static void
test_parse_stylesmenu_tag(void)
{
	setup_server();

	const char *tmpdir = "/tmp/fluxland_test_parse_stylesmenu";
	char cmd[256];
	snprintf(cmd, sizeof(cmd),
		"rm -rf %s && mkdir -p %s && touch %s/retro %s/modern",
		tmpdir, tmpdir, tmpdir, tmpdir);
	system(cmd);

	const char *tmpfile = "/tmp/fluxland_test_menu_stylesmenu.txt";
	FILE *fp = fopen(tmpfile, "w");
	assert(fp != NULL);
	fprintf(fp, "[begin] (Root)\n");
	fprintf(fp, "[stylesmenu] (My Styles) {%s}\n", tmpdir);
	fprintf(fp, "[end]\n");
	fclose(fp);

	struct wm_menu *menu = wm_menu_load(&test_server, tmpfile);
	assert(menu != NULL);

	struct wm_menu_item *item = menu_get_item(menu, 0);
	assert(item != NULL);
	assert(item->type == WM_MENU_SUBMENU);
	assert(strcmp(item->label, "My Styles") == 0);
	assert(item->submenu != NULL);

	/* Submenu should have style entries */
	int sub_count = 0;
	struct wm_menu_item *sub;
	wl_list_for_each(sub, &item->submenu->items, link) {
		sub_count++;
		assert(sub->type == WM_MENU_STYLE);
	}
	assert(sub_count == 2);

	snprintf(cmd, sizeof(cmd), "rm -rf %s", tmpdir);
	system(cmd);
	wm_menu_destroy(menu);
	remove(tmpfile);
	printf("  PASS: parse_stylesmenu_tag\n");
}

/* --- maybe_convert_label: actual ISO-8859-1 conversion --- */

static void
test_maybe_convert_label_latin1(void)
{
	/* Convert ISO-8859-1 byte 0xE9 (e-acute) to UTF-8 */
	char *label = strdup("\xe9");
	char *r = maybe_convert_label(label, "ISO-8859-1");
	assert(r != NULL);
	/* UTF-8 encoding of U+00E9 is 0xC3 0xA9 */
	assert((unsigned char)r[0] == 0xC3);
	assert((unsigned char)r[1] == 0xA9);
	assert(r[2] == '\0');
	free(r);
	printf("  PASS: maybe_convert_label_latin1\n");
}

/* --- wlr_buffer_from_cairo: null surface --- */

static void
test_wlr_buffer_from_cairo_null(void)
{
	struct wlr_buffer *buf = wlr_buffer_from_cairo(NULL);
	assert(buf == NULL);
	printf("  PASS: wlr_buffer_from_cairo_null\n");
}

/* --- wm_menu_show_custom --- */

static void
test_show_custom_menu(void)
{
	setup_server();

	const char *tmpfile = "/tmp/fluxland_test_custom_menu.txt";
	FILE *fp = fopen(tmpfile, "w");
	assert(fp != NULL);
	fprintf(fp, "[begin] (Custom)\n");
	fprintf(fp, "[exec] (Item1) {echo 1}\n");
	fprintf(fp, "[end]\n");
	fclose(fp);

	wm_menu_show_custom(&test_server, tmpfile, 10, 10);
	assert(test_server.custom_menu != NULL);
	assert(test_server.custom_menu->visible == true);

	wm_menu_destroy(test_server.custom_menu);
	test_server.custom_menu = NULL;
	remove(tmpfile);
	printf("  PASS: show_custom_menu\n");
}

/* --- wm_menu_show_custom: null/empty path --- */

static void
test_show_custom_null(void)
{
	setup_server();

	wm_menu_show_custom(&test_server, NULL, 10, 10);
	assert(test_server.custom_menu == NULL);

	wm_menu_show_custom(&test_server, "", 10, 10);
	assert(test_server.custom_menu == NULL);

	wm_menu_show_custom(NULL, "/tmp/x", 10, 10);
	/* Should not crash */

	printf("  PASS: show_custom_null\n");
}

/* --- cancel_pending_submenu --- */

static void
test_cancel_pending_submenu(void)
{
	/* Test that cancel works without crashing */
	cancel_pending_submenu();
	/* Should not crash even when no timer is set */
	printf("  PASS: cancel_pending_submenu\n");
}

/* --- parse long line skip --- */

static void
test_parse_long_line(void)
{
	setup_server();

	const char *tmpfile = "/tmp/fluxland_test_menu_longline.txt";
	FILE *fp = fopen(tmpfile, "w");
	assert(fp != NULL);
	fprintf(fp, "[begin] (Root)\n");
	/* Write a line longer than 1024 chars (buffer size in parser) */
	fprintf(fp, "[exec] (");
	for (int i = 0; i < 1100; i++) {
		fputc('A', fp);
	}
	fprintf(fp, ") {echo long}\n");
	fprintf(fp, "[exec] (Normal) {echo normal}\n");
	fprintf(fp, "[end]\n");
	fclose(fp);

	struct wm_menu *menu = wm_menu_load(&test_server, tmpfile);
	assert(menu != NULL);

	/* The long line should be skipped, only Normal should be parsed */
	int count = 0;
	struct wm_menu_item *item;
	wl_list_for_each(item, &menu->items, link) {
		count++;
	}
	assert(count == 1);
	item = menu_get_item(menu, 0);
	assert(strcmp(item->label, "Normal") == 0);

	wm_menu_destroy(menu);
	remove(tmpfile);
	printf("  PASS: parse_long_line\n");
}

/* --- parse window menu tags (shade, maximize, etc.) --- */

static void
test_parse_window_menu_tags(void)
{
	setup_server();

	const char *tmpfile = "/tmp/fluxland_test_menu_wintags.txt";
	FILE *fp = fopen(tmpfile, "w");
	assert(fp != NULL);
	fprintf(fp, "[begin] (Window)\n");
	fprintf(fp, "[shade] (Shade)\n");
	fprintf(fp, "[maximize] (Maximize)\n");
	fprintf(fp, "[iconify] (Iconify)\n");
	fprintf(fp, "[close] (Close)\n");
	fprintf(fp, "[kill] (Kill)\n");
	fprintf(fp, "[raise] (Raise)\n");
	fprintf(fp, "[lower] (Lower)\n");
	fprintf(fp, "[end]\n");
	fclose(fp);

	struct wm_menu *menu = wm_menu_load(&test_server, tmpfile);
	assert(menu != NULL);

	struct wm_menu_item *item;
	item = menu_get_item(menu, 0);
	assert(item->type == WM_MENU_SHADE);
	item = menu_get_item(menu, 1);
	assert(item->type == WM_MENU_MAXIMIZE);
	item = menu_get_item(menu, 2);
	assert(item->type == WM_MENU_ICONIFY);
	item = menu_get_item(menu, 3);
	assert(item->type == WM_MENU_CLOSE);
	item = menu_get_item(menu, 4);
	assert(item->type == WM_MENU_KILL);
	item = menu_get_item(menu, 5);
	assert(item->type == WM_MENU_RAISE);
	item = menu_get_item(menu, 6);
	assert(item->type == WM_MENU_LOWER);

	wm_menu_destroy(menu);
	remove(tmpfile);
	printf("  PASS: parse_window_menu_tags\n");
}

/* --- wm_menu_show_window: with and without custom menu file --- */

static void
test_show_window_menu(void)
{
	setup_server();

	/* Create a window menu */
	test_server.window_menu = wm_menu_create_window_menu(&test_server);
	assert(test_server.window_menu != NULL);

	wm_menu_show_window(&test_server, 10, 10);
	assert(test_server.window_menu->visible == true);

	wm_menu_hide_all(&test_server);
	wm_menu_destroy(test_server.window_menu);
	test_server.window_menu = NULL;
	printf("  PASS: show_window_menu\n");
}

static void
test_show_window_menu_no_menu(void)
{
	setup_server();
	test_server.window_menu = NULL;

	/* Should not crash */
	wm_menu_show_window(&test_server, 10, 10);
	/* Still NULL because no window_menu_file configured */
	assert(test_server.window_menu == NULL);

	printf("  PASS: show_window_menu_no_menu\n");
}

static void
test_show_window_menu_custom_file(void)
{
	setup_server();

	/* Create a custom window menu file */
	const char *tmpfile = "/tmp/fluxland_test_win_menu_custom.txt";
	FILE *fp = fopen(tmpfile, "w");
	assert(fp != NULL);
	fprintf(fp, "[begin] (Custom Window)\n");
	fprintf(fp, "[close] (Close)\n");
	fprintf(fp, "[end]\n");
	fclose(fp);

	test_config.window_menu_file = strdup(tmpfile);

	/* Should reload from custom file */
	wm_menu_show_window(&test_server, 10, 10);
	assert(test_server.window_menu != NULL);
	assert(test_server.window_menu->visible == true);
	assert(strcmp(test_server.window_menu->title, "Custom Window") == 0);

	wm_menu_hide_all(&test_server);
	wm_menu_destroy(test_server.window_menu);
	test_server.window_menu = NULL;
	free(test_config.window_menu_file);
	test_config.window_menu_file = NULL;
	remove(tmpfile);
	printf("  PASS: show_window_menu_custom_file\n");
}

/* --- wm_menu_show_window_list --- */

static void
test_show_window_list_menu(void)
{
	setup_server();

	struct wm_workspace ws0 = { .name = strdup("Main"), .index = 0 };
	wl_list_init(&ws0.link);
	wl_list_insert(test_server.workspaces.prev, &ws0.link);

	struct wlr_scene_tree view_tree;
	memset(&view_tree, 0, sizeof(view_tree));
	wl_list_init(&view_tree.children);
	wl_list_init(&view_tree.node.link);
	view_tree.node.enabled = 1;

	struct wm_view view;
	memset(&view, 0, sizeof(view));
	view.title = strdup("Test App");
	view.workspace = &ws0;
	view.scene_tree = &view_tree;
	wl_list_init(&view.link);
	wl_list_insert(test_server.views.prev, &view.link);

	wm_menu_show_window_list(&test_server, 10, 10);
	assert(test_server.window_list_menu != NULL);
	assert(test_server.window_list_menu->visible == true);

	wm_menu_hide_all(&test_server);
	/* hide_all destroys window_list_menu */
	test_server.window_list_menu = NULL;

	wl_list_remove(&ws0.link);
	wl_list_remove(&view.link);
	free(ws0.name);
	free(view.title);
	printf("  PASS: show_window_list_menu\n");
}

/* --- wm_menu_show_workspace_menu --- */

static void
test_show_workspace_switching_menu(void)
{
	setup_server();

	struct wm_workspace ws0 = { .name = strdup("Main"), .index = 0 };
	struct wm_workspace ws1 = { .name = strdup("Code"), .index = 1 };
	wl_list_init(&ws0.link);
	wl_list_init(&ws1.link);
	wl_list_insert(test_server.workspaces.prev, &ws0.link);
	wl_list_insert(test_server.workspaces.prev, &ws1.link);

	wm_menu_show_workspace_menu(&test_server, 10, 10);
	assert(test_server.workspace_menu != NULL);
	assert(test_server.workspace_menu->visible == true);

	/* Should have 2 items */
	int count = 0;
	struct wm_menu_item *item;
	wl_list_for_each(item, &test_server.workspace_menu->items, link) {
		count++;
		assert(item->type == WM_MENU_COMMAND);
		assert(item->command != NULL);
		/* Workspace switching commands should be "Workspace N" */
		assert(strstr(item->command, "Workspace") != NULL);
	}
	assert(count == 2);

	wm_menu_hide_all(&test_server);
	test_server.workspace_menu = NULL;

	wl_list_remove(&ws0.link);
	wl_list_remove(&ws1.link);
	free(ws0.name);
	free(ws1.name);
	printf("  PASS: show_workspace_switching_menu\n");
}

/* --- wm_menu_hide_all: window_list_menu, workspace_menu, etc. --- */

static void
test_hide_all_dynamic_menus(void)
{
	setup_server();

	/* Create dynamic menus that hide_all should destroy */
	test_server.window_list_menu = menu_create(&test_server, "WL");
	test_server.workspace_menu = menu_create(&test_server, "WS");
	test_server.client_menu = menu_create(&test_server, "CL");
	test_server.custom_menu = menu_create(&test_server, "CU");

	wm_menu_hide_all(&test_server);

	/* Dynamic menus should be destroyed (set to NULL) */
	assert(test_server.window_list_menu == NULL);
	assert(test_server.workspace_menu == NULL);
	assert(test_server.client_menu == NULL);
	assert(test_server.custom_menu == NULL);

	printf("  PASS: hide_all_dynamic_menus\n");
}

/* --- wm_menu_handle_key dispatches to window_list_menu and window_menu --- */

static void
test_handle_key_dispatches_to_window_list(void)
{
	setup_server();
	struct wm_menu *wl_menu = make_test_menu(3);
	prepare_menu(wl_menu, 100, 100);
	test_server.window_list_menu = wl_menu;

	bool consumed = wm_menu_handle_key(&test_server, 0, XKB_KEY_Down, true);
	assert(consumed == true);
	assert(wl_menu->selected_index == 0);

	test_server.window_list_menu = NULL;
	wm_menu_destroy(wl_menu);
	printf("  PASS: handle_key_dispatches_to_window_list\n");
}

static void
test_handle_key_dispatches_to_window_menu(void)
{
	setup_server();
	struct wm_menu *win_menu = make_test_menu(3);
	prepare_menu(win_menu, 100, 100);
	test_server.window_menu = win_menu;

	bool consumed = wm_menu_handle_key(&test_server, 0, XKB_KEY_Down, true);
	assert(consumed == true);
	assert(win_menu->selected_index == 0);

	test_server.window_menu = NULL;
	wm_menu_destroy(win_menu);
	printf("  PASS: handle_key_dispatches_to_window_menu\n");
}

/* --- wm_menu_handle_button dispatches to window_list_menu and window_menu --- */

static void
test_handle_button_dispatches_to_window_list(void)
{
	setup_server();
	struct wm_menu *wl_menu = make_test_menu(3);
	prepare_menu(wl_menu, 100, 100);
	wl_menu->scene_tree = wlr_scene_tree_create(&test_scene.tree);
	test_server.window_list_menu = wl_menu;

	/* Click on first item */
	int bw = wl_menu->border_width;
	double lx = 100 + bw + 5;
	double ly = 100 + bw + wl_menu->title_height + 5;

	bool consumed = wm_menu_handle_button(&test_server, lx, ly, 0x110, true);
	assert(consumed == true);

	test_server.window_list_menu = NULL;
	wm_menu_destroy(wl_menu);
	printf("  PASS: handle_button_dispatches_to_window_list\n");
}

static void
test_handle_button_dispatches_to_window_menu(void)
{
	setup_server();
	struct wm_menu *win_menu = make_test_menu(3);
	prepare_menu(win_menu, 100, 100);
	win_menu->scene_tree = wlr_scene_tree_create(&test_scene.tree);
	test_server.window_menu = win_menu;

	/* Click on title area */
	int bw = win_menu->border_width;
	double lx = 100 + bw + 5;
	double ly = 100 + bw + 5;

	bool consumed = wm_menu_handle_button(&test_server, lx, ly, 0x110, true);
	assert(consumed == true);

	test_server.window_menu = NULL;
	wm_menu_destroy(win_menu);
	printf("  PASS: handle_button_dispatches_to_window_menu\n");
}

/* --- wm_menu_handle_motion dispatches to window_list_menu and window_menu --- */

static void
test_handle_motion_dispatches_to_window_list(void)
{
	setup_server();
	struct wm_menu *wl_menu = make_test_menu(3);
	prepare_menu(wl_menu, 100, 100);
	test_server.window_list_menu = wl_menu;

	int bw = wl_menu->border_width;
	double lx = 100 + bw + 5;
	double ly = 100 + bw + wl_menu->title_height + 5;

	bool consumed = wm_menu_handle_motion(&test_server, lx, ly);
	assert(consumed == true);
	assert(wl_menu->selected_index == 0);

	test_server.window_list_menu = NULL;
	wm_menu_destroy(wl_menu);
	printf("  PASS: handle_motion_dispatches_to_window_list\n");
}

static void
test_handle_motion_dispatches_to_window_menu(void)
{
	setup_server();
	struct wm_menu *win_menu = make_test_menu(3);
	prepare_menu(win_menu, 100, 100);
	test_server.window_menu = win_menu;

	int bw = win_menu->border_width;
	double lx = 100 + bw + 5;
	double ly = 100 + bw + win_menu->title_height + 5;

	bool consumed = wm_menu_handle_motion(&test_server, lx, ly);
	assert(consumed == true);
	assert(win_menu->selected_index == 0);

	test_server.window_menu = NULL;
	wm_menu_destroy(win_menu);
	printf("  PASS: handle_motion_dispatches_to_window_menu\n");
}

/* --- vim-style l and h keys --- */

static void
test_key_l_enters_submenu(void)
{
	setup_server();
	struct wm_menu *menu = make_menu_with_submenu();
	prepare_menu(menu, 100, 100);
	menu->selected_index = 1; /* Submenu item */

	bool consumed = wm_menu_handle_key_for(menu, XKB_KEY_l);
	assert(consumed == true);

	struct wm_menu_item *sub_item = menu_get_item(menu, 1);
	assert(sub_item->submenu->visible == true);

	wm_menu_destroy(menu);
	printf("  PASS: key_l_enters_submenu\n");
}

static void
test_key_h_leaves_submenu(void)
{
	setup_server();
	struct wm_menu *menu = make_menu_with_submenu();
	prepare_menu(menu, 100, 100);

	/* Open submenu first */
	menu->selected_index = 1;
	wm_menu_handle_key_for(menu, XKB_KEY_Right);

	struct wm_menu_item *sub_item = menu_get_item(menu, 1);
	assert(sub_item->submenu->visible == true);

	/* h should close it */
	bool consumed = wm_menu_handle_key_for(menu, XKB_KEY_h);
	assert(consumed == true);
	assert(sub_item->submenu->visible == false);

	wm_menu_destroy(menu);
	printf("  PASS: key_h_leaves_submenu\n");
}

/* --- Key KP_Enter activates item --- */

static void
test_key_kp_enter_activates(void)
{
	setup_server();
	struct wm_menu *menu = make_test_menu(3);
	prepare_menu(menu, 100, 100);
	menu->scene_tree = wlr_scene_tree_create(&test_scene.tree);
	menu->selected_index = 0;

	bool consumed = wm_menu_handle_key_for(menu, XKB_KEY_KP_Enter);
	assert(consumed == true);
	assert(menu->visible == false);

	wm_menu_destroy(menu);
	printf("  PASS: key_kp_enter_activates\n");
}

/* --- Key Return with no selection (selected_index = -1) --- */

static void
test_key_return_no_selection(void)
{
	setup_server();
	struct wm_menu *menu = make_test_menu(3);
	prepare_menu(menu, 100, 100);
	menu->selected_index = -1;

	bool consumed = wm_menu_handle_key_for(menu, XKB_KEY_Return);
	assert(consumed == true);
	/* Menu should stay visible since no item was selected */
	assert(menu->visible == true);

	wm_menu_destroy(menu);
	printf("  PASS: key_return_no_selection\n");
}

/* --- Type-ahead with digits and uppercase --- */

static void
test_type_ahead_digit(void)
{
	setup_server();
	test_config.menu_search = WM_MENU_SEARCH_ITEMSTART;

	struct wm_menu *menu = menu_create(&test_server, "Test");
	struct wm_menu_item *i0 = menu_item_create(WM_MENU_EXEC, "Apple");
	struct wm_menu_item *i1 = menu_item_create(WM_MENU_EXEC, "3D Viewer");
	wl_list_insert(menu->items.prev, &i0->link);
	wl_list_insert(menu->items.prev, &i1->link);
	prepare_menu(menu, 0, 0);

	/* Type '3' via key handler - should select "3D Viewer" */
	bool consumed = wm_menu_handle_key_for(menu, XKB_KEY_0 + 3);
	assert(consumed == true);
	assert(menu->selected_index == 1);

	wm_menu_destroy(menu);
	printf("  PASS: type_ahead_digit\n");
}

static void
test_type_ahead_uppercase(void)
{
	setup_server();
	test_config.menu_search = WM_MENU_SEARCH_ITEMSTART;

	struct wm_menu *menu = menu_create(&test_server, "Test");
	struct wm_menu_item *i0 = menu_item_create(WM_MENU_EXEC, "Alpha");
	struct wm_menu_item *i1 = menu_item_create(WM_MENU_EXEC, "Beta");
	wl_list_insert(menu->items.prev, &i0->link);
	wl_list_insert(menu->items.prev, &i1->link);
	prepare_menu(menu, 0, 0);

	/* Type 'B' (uppercase) via key handler */
	bool consumed = wm_menu_handle_key_for(menu, XKB_KEY_B);
	assert(consumed == true);
	assert(menu->selected_index == 1);

	wm_menu_destroy(menu);
	printf("  PASS: type_ahead_uppercase\n");
}

static void
test_type_ahead_space(void)
{
	setup_server();
	test_config.menu_search = WM_MENU_SEARCH_SOMEWHERE;

	struct wm_menu *menu = menu_create(&test_server, "Test");
	struct wm_menu_item *i0 = menu_item_create(WM_MENU_EXEC, "Hello World");
	struct wm_menu_item *i1 = menu_item_create(WM_MENU_EXEC, "FooBar");
	wl_list_insert(menu->items.prev, &i0->link);
	wl_list_insert(menu->items.prev, &i1->link);
	prepare_menu(menu, 0, 0);

	/* Type 'h' then space via key handler */
	wm_menu_handle_key_for(menu, XKB_KEY_h);
	/* Reset search_len to test space standalone */
	menu->search_len = 0;
	menu->search_buf[0] = '\0';

	bool consumed = wm_menu_handle_key_for(menu, XKB_KEY_space);
	assert(consumed == true);
	/* Space should match somewhere in "Hello World" */

	wm_menu_destroy(menu);
	printf("  PASS: type_ahead_space\n");
}

/* --- Type-ahead: unknown key returns false --- */

static void
test_type_ahead_unknown_key(void)
{
	setup_server();
	test_config.menu_search = WM_MENU_SEARCH_ITEMSTART;

	struct wm_menu *menu = make_test_menu(3);
	prepare_menu(menu, 0, 0);

	/* A key that's not a printable character or navigation key */
	bool consumed = wm_menu_handle_key_for(menu, 0xffe1); /* Shift_L */
	assert(consumed == false);

	wm_menu_destroy(menu);
	printf("  PASS: type_ahead_unknown_key\n");
}

/* --- wm_menu_show: output clamping --- */

static void
test_menu_show_output_clamping(void)
{
	setup_server();

	struct wm_output wm_out;
	memset(&wm_out, 0, sizeof(wm_out));
	wm_out.wlr_output = &g_stub_wlr_output_obj;
	wm_out.usable_area.x = 0;
	wm_out.usable_area.y = 0;
	wm_out.usable_area.width = 200;
	wm_out.usable_area.height = 200;
	wl_list_init(&wm_out.link);
	wl_list_insert(test_server.outputs.prev, &wm_out.link);

	g_stub_output_at_result = &g_stub_wlr_output_obj;
	g_stub_output_box = (struct wlr_box){0, 0, 200, 200};

	struct wm_menu *menu = make_test_menu(3);

	/* Show at position that would overflow right and bottom */
	wm_menu_show(menu, 180, 180);
	assert(menu->visible == true);
	/* Menu should be clamped to fit within 200x200 output */
	assert(menu->x + menu->width <= 200);
	assert(menu->y + menu->height <= 200);

	wm_menu_hide(menu);

	/* Show at negative position -- should clamp to 0,0 */
	wm_menu_show(menu, -50, -50);
	assert(menu->visible == true);
	assert(menu->x >= 0);
	assert(menu->y >= 0);

	wm_menu_hide(menu);

	g_stub_output_at_result = NULL;
	g_stub_output_box = (struct wlr_box){0, 0, 0, 0};
	wl_list_remove(&wm_out.link);
	wm_menu_destroy(menu);
	printf("  PASS: menu_show_output_clamping\n");
}

/* --- submenu_position: flip to left side but clamp to out_left --- */

static void
test_submenu_position_flip_clamp(void)
{
	setup_server();

	struct wm_output wm_out;
	memset(&wm_out, 0, sizeof(wm_out));
	wm_out.wlr_output = &g_stub_wlr_output_obj;
	wm_out.usable_area.x = 0;
	wm_out.usable_area.y = 0;
	wm_out.usable_area.width = 200;
	wm_out.usable_area.height = 400;
	wl_list_init(&wm_out.link);
	wl_list_insert(test_server.outputs.prev, &wm_out.link);

	g_stub_output_at_result = &g_stub_wlr_output_obj;
	g_stub_output_box = (struct wlr_box){0, 0, 200, 400};

	struct wm_menu *menu = make_test_menu(3);
	/* Place parent menu at x=50, very narrow output (200px).
	 * Menu width ~128. sub_x = 50+128=178, 178+128=306 > 200 => flip.
	 * Flipped: sub_x = 50-128 = -78 < 0 => clamp to 0 */
	prepare_menu(menu, 50, 50);

	int sub_x, sub_y;
	submenu_position(menu, 0, &sub_x, &sub_y);

	/* After flip + clamp, sub_x should be >= 0 */
	assert(sub_x >= 0);

	g_stub_output_at_result = NULL;
	g_stub_output_box = (struct wlr_box){0, 0, 0, 0};
	wl_list_remove(&wm_out.link);
	wm_menu_destroy(menu);
	printf("  PASS: submenu_position_flip_clamp\n");
}

/* --- submenu_delay_cb --- */

static void
test_submenu_delay_cb(void)
{
	setup_server();

	/* Set up pending_submenu with a submenu item */
	struct wm_menu *submenu = menu_create(&test_server, "Delayed Sub");
	struct wm_menu_item *i0 = menu_item_create(WM_MENU_EXEC, "Child");
	wl_list_insert(submenu->items.prev, &i0->link);

	struct wm_menu_item mock_item;
	memset(&mock_item, 0, sizeof(mock_item));
	mock_item.submenu = submenu;

	pending_submenu.item = &mock_item;
	pending_submenu.sub_x = 50;
	pending_submenu.sub_y = 50;

	submenu_delay_cb(NULL);

	assert(submenu->visible == true);
	assert(pending_submenu.item == NULL);

	wm_menu_destroy(submenu);
	printf("  PASS: submenu_delay_cb\n");
}

/* --- submenu_delay_cb with null item --- */

static void
test_submenu_delay_cb_null(void)
{
	pending_submenu.item = NULL;
	submenu_delay_cb(NULL);
	/* Should not crash */
	assert(pending_submenu.item == NULL);
	printf("  PASS: submenu_delay_cb_null\n");
}

/* --- Motion: auto-open submenu with delay --- */

static void
test_motion_submenu_with_delay(void)
{
	setup_server();
	test_config.menu_delay = 100; /* 100ms delay */

	struct wm_menu *menu = make_menu_with_submenu();
	prepare_menu(menu, 100, 100);

	int bw = menu->border_width;
	/* Move over submenu item (index 1) */
	double lx = 100 + bw + 5;
	double ly = 100 + bw + menu->title_height + menu->item_height + 5;

	bool consumed = wm_menu_handle_motion_for(menu, lx, ly);
	assert(consumed == true);
	assert(menu->selected_index == 1);

	/* With delay > 0, submenu should NOT be immediately visible.
	 * It should be pending. */
	struct wm_menu_item *sub_item = menu_get_item(menu, 1);
	assert(pending_submenu.item == sub_item);

	/* Reset */
	cancel_pending_submenu();
	test_config.menu_delay = 0;
	wm_menu_destroy(menu);
	printf("  PASS: motion_submenu_with_delay\n");
}

/* --- Motion: auto-open submenu without delay (immediate) --- */

static void
test_motion_submenu_immediate(void)
{
	setup_server();
	test_config.menu_delay = 0;

	struct wm_menu *menu = make_menu_with_submenu();
	prepare_menu(menu, 100, 100);

	int bw = menu->border_width;
	/* Move over submenu item (index 1) */
	double lx = 100 + bw + 5;
	double ly = 100 + bw + menu->title_height + menu->item_height + 5;

	bool consumed = wm_menu_handle_motion_for(menu, lx, ly);
	assert(consumed == true);
	assert(menu->selected_index == 1);

	/* With delay = 0, submenu should be immediately visible */
	struct wm_menu_item *sub_item = menu_get_item(menu, 1);
	assert(sub_item->submenu->visible == true);

	wm_menu_destroy(menu);
	printf("  PASS: motion_submenu_immediate\n");
}

/* --- Motion: moving from submenu item to non-submenu closes submenu --- */

static void
test_motion_closes_old_submenu(void)
{
	setup_server();
	test_config.menu_delay = 0;

	struct wm_menu *menu = make_menu_with_submenu();
	prepare_menu(menu, 100, 100);

	int bw = menu->border_width;
	/* First move over submenu item (index 1) to open it */
	double lx = 100 + bw + 5;
	double ly = 100 + bw + menu->title_height + menu->item_height + 5;
	wm_menu_handle_motion_for(menu, lx, ly);

	struct wm_menu_item *sub_item = menu_get_item(menu, 1);
	assert(sub_item->submenu->visible == true);

	/* Now move to first item (index 0) - should close submenu */
	ly = 100 + bw + menu->title_height + 5;
	wm_menu_handle_motion_for(menu, lx, ly);

	assert(menu->selected_index == 0);
	assert(sub_item->submenu->visible == false);

	wm_menu_destroy(menu);
	printf("  PASS: motion_closes_old_submenu\n");
}

/* --- Window list with iconified and focused views --- */

static void
test_create_window_list_with_states(void)
{
	setup_server();

	struct wm_workspace ws0 = { .name = strdup("Main"), .index = 0 };
	wl_list_init(&ws0.link);
	wl_list_insert(test_server.workspaces.prev, &ws0.link);

	/* Normal view */
	struct wlr_scene_tree tree1;
	memset(&tree1, 0, sizeof(tree1));
	wl_list_init(&tree1.children);
	wl_list_init(&tree1.node.link);
	tree1.node.enabled = 1;

	struct wm_view view1;
	memset(&view1, 0, sizeof(view1));
	view1.title = strdup("Normal");
	view1.workspace = &ws0;
	view1.scene_tree = &tree1;
	wl_list_init(&view1.link);
	wl_list_insert(test_server.views.prev, &view1.link);

	/* Iconified view */
	struct wlr_scene_tree tree2;
	memset(&tree2, 0, sizeof(tree2));
	wl_list_init(&tree2.children);
	wl_list_init(&tree2.node.link);
	tree2.node.enabled = 0; /* iconified */

	struct wm_view view2;
	memset(&view2, 0, sizeof(view2));
	view2.title = strdup("Minimized");
	view2.workspace = &ws0;
	view2.scene_tree = &tree2;
	wl_list_init(&view2.link);
	wl_list_insert(test_server.views.prev, &view2.link);

	/* Focused view */
	struct wlr_scene_tree tree3;
	memset(&tree3, 0, sizeof(tree3));
	wl_list_init(&tree3.children);
	wl_list_init(&tree3.node.link);
	tree3.node.enabled = 1;

	struct wm_view view3;
	memset(&view3, 0, sizeof(view3));
	view3.title = strdup("Focused");
	view3.workspace = &ws0;
	view3.scene_tree = &tree3;
	wl_list_init(&view3.link);
	wl_list_insert(test_server.views.prev, &view3.link);
	test_server.focused_view = &view3;

	struct wm_menu *menu = wm_menu_create_window_list(&test_server);
	assert(menu != NULL);

	/* Should have ws header + 3 entries = 4 items */
	int count = 0;
	struct wm_menu_item *item;
	wl_list_for_each(item, &menu->items, link) {
		count++;
	}
	assert(count == 4);

	/* Check that iconified view has bracket format [Minimized] */
	bool found_iconified = false;
	bool found_focused = false;
	wl_list_for_each(item, &menu->items, link) {
		if (item->type == WM_MENU_WINDOW_ENTRY) {
			if (strstr(item->label, "[Minimized]") != NULL) {
				found_iconified = true;
			}
			if (strstr(item->label, "* Focused") != NULL) {
				found_focused = true;
			}
		}
	}
	assert(found_iconified == true);
	assert(found_focused == true);

	test_server.focused_view = NULL;
	wl_list_remove(&ws0.link);
	wl_list_remove(&view1.link);
	wl_list_remove(&view2.link);
	wl_list_remove(&view3.link);
	free(ws0.name);
	free(view1.title);
	free(view2.title);
	free(view3.title);
	wm_menu_destroy(menu);
	printf("  PASS: create_window_list_with_states\n");
}

/* --- Window list: untitled view fallback --- */

static void
test_create_window_list_untitled(void)
{
	setup_server();

	struct wm_workspace ws0 = { .name = strdup("Main"), .index = 0 };
	wl_list_init(&ws0.link);
	wl_list_insert(test_server.workspaces.prev, &ws0.link);

	struct wlr_scene_tree tree;
	memset(&tree, 0, sizeof(tree));
	wl_list_init(&tree.children);
	wl_list_init(&tree.node.link);
	tree.node.enabled = 1;

	struct wm_view view;
	memset(&view, 0, sizeof(view));
	view.title = NULL; /* no title */
	view.app_id = strdup("myapp");
	view.workspace = &ws0;
	view.scene_tree = &tree;
	wl_list_init(&view.link);
	wl_list_insert(test_server.views.prev, &view.link);

	struct wm_menu *menu = wm_menu_create_window_list(&test_server);
	assert(menu != NULL);

	/* Check that it falls back to app_id */
	struct wm_menu_item *item;
	wl_list_for_each(item, &menu->items, link) {
		if (item->type == WM_MENU_WINDOW_ENTRY) {
			assert(strstr(item->label, "myapp") != NULL);
		}
	}

	wl_list_remove(&ws0.link);
	wl_list_remove(&view.link);
	free(ws0.name);
	free(view.app_id);
	wm_menu_destroy(menu);
	printf("  PASS: create_window_list_untitled\n");
}

/* --- Escape from root menu (not in submenu) --- */

static void
test_key_escape_root(void)
{
	setup_server();
	struct wm_menu *menu = make_test_menu(3);
	prepare_menu(menu, 100, 100);
	menu->scene_tree = wlr_scene_tree_create(&test_scene.tree);
	menu->parent = NULL; /* root menu has no parent */

	bool consumed = wm_menu_handle_key_for(menu, XKB_KEY_Escape);
	assert(consumed == true);
	assert(menu->visible == false);

	wm_menu_destroy(menu);
	printf("  PASS: key_escape_root\n");
}

/* --- Escape from submenu (goes back to parent) --- */

static void
test_key_escape_submenu(void)
{
	setup_server();
	struct wm_menu *menu = make_menu_with_submenu();
	prepare_menu(menu, 100, 100);

	/* Open submenu */
	menu->selected_index = 1;
	wm_menu_handle_key_for(menu, XKB_KEY_Right);

	struct wm_menu_item *sub_item = menu_get_item(menu, 1);
	assert(sub_item->submenu->visible == true);

	/* Escape in the submenu should close only the submenu */
	bool consumed = wm_menu_handle_key_for(menu, XKB_KEY_Escape);
	assert(consumed == true);
	assert(sub_item->submenu->visible == false);
	assert(menu->visible == true); /* Parent should stay visible */

	wm_menu_destroy(menu);
	printf("  PASS: key_escape_submenu\n");
}

/* --- pixel_buffer_end_data_ptr_access --- */

static void
test_pixel_buffer_end_access(void)
{
	cairo_surface_t *surface = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, 10, 10);
	struct wlr_buffer *buf = wlr_buffer_from_cairo(surface);
	assert(buf != NULL);

	/* end_data_ptr_access is a no-op but should not crash */
	pixel_buffer_end_data_ptr_access(buf);

	pixel_buffer_destroy(buf);
	printf("  PASS: pixel_buffer_end_access\n");
}

/* --- menu_compute_layout: submenu arrow width accounting --- */

static void
test_menu_layout_submenu_arrow_width(void)
{
	setup_server();
	struct wm_menu *menu = menu_create(&test_server, "Test");

	/* Add a submenu item -- its width should account for arrow */
	struct wm_menu_item *sub = menu_item_create(WM_MENU_SUBMENU, "Sub Menu Item");
	sub->submenu = menu_create(&test_server, "SubTitle");
	wl_list_insert(menu->items.prev, &sub->link);

	/* Add a sendto item */
	struct wm_menu_item *sendto = menu_item_create(WM_MENU_SENDTO, "Send To...");
	wl_list_insert(menu->items.prev, &sendto->link);

	/* Add a layer item */
	struct wm_menu_item *layer = menu_item_create(WM_MENU_LAYER, "Layer");
	wl_list_insert(menu->items.prev, &layer->link);

	menu_compute_layout(menu, &test_style);
	assert(menu->item_count == 3);
	/* Width should be at least MENU_MIN_WIDTH */
	assert(menu->width >= MENU_MIN_WIDTH);

	wm_menu_destroy(menu);
	printf("  PASS: menu_layout_submenu_arrow_width\n");
}

/* --- menu_compute_layout: icon column width --- */

static void
test_menu_layout_icon_column(void)
{
	setup_server();
	struct wm_menu *menu = menu_create(&test_server, "Icons");

	struct wm_menu_item *i0 = menu_item_create(WM_MENU_EXEC, "With Icon");
	i0->icon_path = strdup("/path/to/icon.png");
	i0->command = strdup("echo test");
	wl_list_insert(menu->items.prev, &i0->link);

	struct wm_menu_item *i1 = menu_item_create(WM_MENU_EXEC, "No Icon");
	i1->command = strdup("echo test2");
	wl_list_insert(menu->items.prev, &i1->link);

	menu_compute_layout(menu, &test_style);
	assert(menu->item_count == 2);
	/* Width includes icon column for all items since one has an icon */
	assert(menu->width >= MENU_MIN_WIDTH);

	wm_menu_destroy(menu);
	printf("  PASS: menu_layout_icon_column\n");
}

/* --- parse_menu_items_depth: stylesdir with brace syntax --- */

static void
test_parse_stylesdir_brace(void)
{
	setup_server();

	const char *tmpdir = "/tmp/fluxland_test_parse_stylesdir_brace";
	char cmd[256];
	snprintf(cmd, sizeof(cmd),
		"rm -rf %s && mkdir -p %s && touch %s/minimal",
		tmpdir, tmpdir, tmpdir);
	system(cmd);

	const char *tmpfile = "/tmp/fluxland_test_menu_stylesdir_brace.txt";
	FILE *fp = fopen(tmpfile, "w");
	assert(fp != NULL);
	fprintf(fp, "[begin] (Root)\n");
	fprintf(fp, "[stylesdir] {%s}\n", tmpdir); /* brace syntax instead of paren */
	fprintf(fp, "[end]\n");
	fclose(fp);

	struct wm_menu *menu = wm_menu_load(&test_server, tmpfile);
	assert(menu != NULL);

	int count = 0;
	struct wm_menu_item *item;
	wl_list_for_each(item, &menu->items, link) {
		count++;
		assert(item->type == WM_MENU_STYLE);
	}
	assert(count == 1);

	snprintf(cmd, sizeof(cmd), "rm -rf %s", tmpdir);
	system(cmd);
	wm_menu_destroy(menu);
	remove(tmpfile);
	printf("  PASS: parse_stylesdir_brace\n");
}

/* --- parse_menu_items_depth: wallpapers with no label and no dir --- */

static void
test_parse_wallpapers_defaults(void)
{
	setup_server();

	const char *tmpfile = "/tmp/fluxland_test_menu_wall_default.txt";
	FILE *fp = fopen(tmpfile, "w");
	assert(fp != NULL);
	fprintf(fp, "[begin] (Root)\n");
	fprintf(fp, "[wallpapers]\n"); /* No label, no dir */
	fprintf(fp, "[end]\n");
	fclose(fp);

	struct wm_menu *menu = wm_menu_load(&test_server, tmpfile);
	assert(menu != NULL);

	/* Should have one submenu labeled "Wallpapers" (default) */
	struct wm_menu_item *item = menu_get_item(menu, 0);
	assert(item != NULL);
	assert(item->type == WM_MENU_SUBMENU);
	assert(strcmp(item->label, "Wallpapers") == 0);

	wm_menu_destroy(menu);
	remove(tmpfile);
	printf("  PASS: parse_wallpapers_defaults\n");
}

/* --- parse_menu_items_depth: stylesmenu with no dir --- */

static void
test_parse_stylesmenu_no_dir(void)
{
	setup_server();

	const char *tmpfile = "/tmp/fluxland_test_menu_stymenu_nodir.txt";
	FILE *fp = fopen(tmpfile, "w");
	assert(fp != NULL);
	fprintf(fp, "[begin] (Root)\n");
	fprintf(fp, "[stylesmenu] (My Styles)\n"); /* No brace dir */
	fprintf(fp, "[end]\n");
	fclose(fp);

	struct wm_menu *menu = wm_menu_load(&test_server, tmpfile);
	assert(menu != NULL);

	/* Should have one submenu but with no entries (no dir) */
	struct wm_menu_item *item = menu_get_item(menu, 0);
	assert(item != NULL);
	assert(item->type == WM_MENU_SUBMENU);
	assert(item->submenu != NULL);
	/* Submenu should be empty since no dir was provided */
	assert(wl_list_empty(&item->submenu->items));

	wm_menu_destroy(menu);
	remove(tmpfile);
	printf("  PASS: parse_stylesmenu_no_dir\n");
}

/* --- parse_menu_items_depth: simple tags with default labels --- */

static void
test_parse_simple_tags_default_labels(void)
{
	setup_server();

	const char *tmpfile = "/tmp/fluxland_test_menu_default_labels.txt";
	FILE *fp = fopen(tmpfile, "w");
	assert(fp != NULL);
	fprintf(fp, "[begin] (Root)\n");
	fprintf(fp, "[config]\n");       /* No label => default "config" */
	fprintf(fp, "[exit]\n");         /* No label => default "exit" */
	fprintf(fp, "[restart]\n");      /* No label => default "Restart" */
	fprintf(fp, "[end]\n");
	fclose(fp);

	struct wm_menu *menu = wm_menu_load(&test_server, tmpfile);
	assert(menu != NULL);

	struct wm_menu_item *item;
	item = menu_get_item(menu, 0);
	assert(item->type == WM_MENU_CONFIG);
	assert(strcmp(item->label, "config") == 0);

	item = menu_get_item(menu, 1);
	assert(item->type == WM_MENU_EXIT);
	assert(strcmp(item->label, "exit") == 0);

	item = menu_get_item(menu, 2);
	assert(item->type == WM_MENU_RESTART);
	assert(strcmp(item->label, "Restart") == 0);

	wm_menu_destroy(menu);
	remove(tmpfile);
	printf("  PASS: parse_simple_tags_default_labels\n");
}

/* --- null safety for server-level handlers --- */

static void
test_handle_key_null_server(void)
{
	bool consumed = wm_menu_handle_key(NULL, 0, XKB_KEY_Down, true);
	assert(consumed == false);
	printf("  PASS: handle_key_null_server\n");
}

static void
test_handle_motion_null_server(void)
{
	bool consumed = wm_menu_handle_motion(NULL, 0, 0);
	assert(consumed == false);
	printf("  PASS: handle_motion_null_server\n");
}

static void
test_handle_button_null_server(void)
{
	bool consumed = wm_menu_handle_button(NULL, 0, 0, 0x110, true);
	assert(consumed == false);
	printf("  PASS: handle_button_null_server\n");
}

/* --- wm_menu_hide_all null safety --- */

static void
test_hide_all_null(void)
{
	wm_menu_hide_all(NULL);
	/* Should not crash */
	printf("  PASS: hide_all_null\n");
}

/* --- wm_menu_show null safety --- */

static void
test_show_null_menu(void)
{
	wm_menu_show(NULL, 0, 0);
	/* Should not crash */
	printf("  PASS: show_null_menu\n");
}

/* --- wm_menu_hide null/not-visible safety --- */

static void
test_hide_null_menu(void)
{
	wm_menu_hide(NULL);
	/* Should not crash */

	setup_server();
	struct wm_menu *menu = make_test_menu(1);
	menu->visible = false;
	wm_menu_hide(menu);
	/* Should return early for not-visible menu */

	wm_menu_destroy(menu);
	printf("  PASS: hide_null_menu\n");
}

/* --- menu_item_at: null menu --- */

static void
test_menu_item_at_null(void)
{
	int idx = menu_item_at(NULL, 0, 0);
	assert(idx == -1);
	printf("  PASS: menu_item_at_null\n");
}

/* --- find_deepest_visible: null/not-visible --- */

static void
test_find_deepest_visible_null(void)
{
	struct wm_menu *result = find_deepest_visible(NULL);
	assert(result == NULL);

	setup_server();
	struct wm_menu *menu = make_test_menu(1);
	menu->visible = false;
	result = find_deepest_visible(menu);
	assert(result == NULL);

	wm_menu_destroy(menu);
	printf("  PASS: find_deepest_visible_null\n");
}

/* --- Button: click on NOP item does not close menu --- */

static void
test_button_click_nop_stays_open(void)
{
	setup_server();
	struct wm_menu *menu = menu_create(&test_server, "Test");
	struct wm_menu_item *nop = menu_item_create(WM_MENU_NOP, "Info");
	wl_list_insert(menu->items.prev, &nop->link);
	prepare_menu(menu, 100, 100);

	int bw = menu->border_width;
	double lx = 100 + bw + 5;
	double ly = 100 + bw + menu->title_height + 5;

	bool consumed = wm_menu_handle_button_for(menu, lx, ly, 0x110, true);
	assert(consumed == true);
	/* Menu stays visible when clicking on NOP */
	assert(menu->visible == true);

	wm_menu_destroy(menu);
	printf("  PASS: button_click_nop_stays_open\n");
}

/* --- Button: click on separator stays open --- */

static void
test_button_click_separator_stays_open(void)
{
	setup_server();
	struct wm_menu *menu = menu_create(&test_server, "Test");
	struct wm_menu_item *sep = menu_item_create(WM_MENU_SEPARATOR, NULL);
	wl_list_insert(menu->items.prev, &sep->link);
	prepare_menu(menu, 100, 100);

	int bw = menu->border_width;
	double lx = 100 + bw + 5;
	double ly = 100 + bw + menu->title_height + 2;

	bool consumed = wm_menu_handle_button_for(menu, lx, ly, 0x110, true);
	assert(consumed == true);
	/* Menu should remain visible */
	assert(menu->visible == true);

	wm_menu_destroy(menu);
	printf("  PASS: button_click_separator_stays_open\n");
}

/* --- wm_menu_show_window_list: null server --- */

static void
test_show_window_list_null(void)
{
	wm_menu_show_window_list(NULL, 10, 10);
	/* Should not crash */
	printf("  PASS: show_window_list_null\n");
}

/* --- wm_menu_show_workspace_menu: null server --- */

static void
test_show_workspace_menu_null(void)
{
	wm_menu_show_workspace_menu(NULL, 10, 10);
	/* Should not crash */
	printf("  PASS: show_workspace_menu_null\n");
}

/* --- wm_menu_show_root: null server and null root_menu --- */

static void
test_show_root_null(void)
{
	wm_menu_show_root(NULL, 10, 10);
	/* Should not crash */

	setup_server();
	test_server.root_menu = NULL;
	wm_menu_show_root(&test_server, 10, 10);
	/* Should not crash */

	printf("  PASS: show_root_null\n");
}

/* --- wm_menu_show_window: null server --- */

static void
test_show_window_null(void)
{
	wm_menu_show_window(NULL, 10, 10);
	/* Should not crash */
	printf("  PASS: show_window_null\n");
}

/* --- client_menu: iconified view in client menu --- */

static void
test_client_menu_iconified(void)
{
	setup_server();

	struct wlr_scene_tree view_tree;
	memset(&view_tree, 0, sizeof(view_tree));
	wl_list_init(&view_tree.children);
	wl_list_init(&view_tree.node.link);
	view_tree.node.enabled = 0; /* iconified */

	struct wm_workspace ws0 = { .name = strdup("Main"), .index = 0 };
	wl_list_init(&ws0.link);
	wl_list_insert(test_server.workspaces.prev, &ws0.link);

	struct wm_view view;
	memset(&view, 0, sizeof(view));
	view.title = strdup("Minimized App");
	view.app_id = strdup("minimized");
	view.workspace = &ws0;
	view.scene_tree = &view_tree;
	wl_list_init(&view.link);
	wl_list_insert(test_server.views.prev, &view.link);

	wm_menu_show_client_menu(&test_server, NULL, 10, 10);
	assert(test_server.client_menu != NULL);

	/* Check the entry label uses bracket format for iconified */
	struct wm_menu_item *item;
	wl_list_for_each(item, &test_server.client_menu->items, link) {
		assert(strstr(item->label, "[Minimized App]") != NULL);
	}

	wl_list_remove(&ws0.link);
	wl_list_remove(&view.link);
	free(ws0.name);
	free(view.title);
	free(view.app_id);
	wm_menu_destroy(test_server.client_menu);
	test_server.client_menu = NULL;
	printf("  PASS: client_menu_iconified\n");
}

/* --- client_menu: view with no title and no app_id --- */

static void
test_client_menu_untitled(void)
{
	setup_server();

	struct wlr_scene_tree view_tree;
	memset(&view_tree, 0, sizeof(view_tree));
	wl_list_init(&view_tree.children);
	wl_list_init(&view_tree.node.link);
	view_tree.node.enabled = 1;

	struct wm_workspace ws0 = { .name = strdup("Main"), .index = 0 };
	wl_list_init(&ws0.link);
	wl_list_insert(test_server.workspaces.prev, &ws0.link);

	struct wm_view view;
	memset(&view, 0, sizeof(view));
	view.title = NULL;
	view.app_id = NULL;
	view.workspace = &ws0;
	view.scene_tree = &view_tree;
	wl_list_init(&view.link);
	wl_list_insert(test_server.views.prev, &view.link);

	wm_menu_show_client_menu(&test_server, NULL, 10, 10);
	assert(test_server.client_menu != NULL);

	/* Should fall back to "(untitled)" */
	struct wm_menu_item *item;
	wl_list_for_each(item, &test_server.client_menu->items, link) {
		assert(strstr(item->label, "(untitled)") != NULL);
	}

	wl_list_remove(&ws0.link);
	wl_list_remove(&view.link);
	free(ws0.name);
	wm_menu_destroy(test_server.client_menu);
	test_server.client_menu = NULL;
	printf("  PASS: client_menu_untitled\n");
}

/* --- wm_menu_handle_key: no visible menus --- */

static void
test_handle_key_no_visible(void)
{
	setup_server();
	/* No menus set */
	bool consumed = wm_menu_handle_key(&test_server, 0, XKB_KEY_Down, true);
	assert(consumed == false);
	printf("  PASS: handle_key_no_visible\n");
}

/* --- wm_menu_handle_button: not-visible menus --- */

static void
test_handle_button_not_visible(void)
{
	setup_server();
	bool consumed = wm_menu_handle_button_for(NULL, 0, 0, 0x110, true);
	assert(consumed == false);
	printf("  PASS: handle_button_not_visible\n");
}

/* --- wm_menu_handle_motion: not-visible menu --- */

static void
test_handle_motion_not_visible(void)
{
	setup_server();
	bool consumed = wm_menu_handle_motion_for(NULL, 0, 0);
	assert(consumed == false);
	printf("  PASS: handle_motion_not_visible\n");
}

/* --- Key Right on non-submenu item (no-op) --- */

static void
test_key_right_no_submenu(void)
{
	setup_server();
	struct wm_menu *menu = make_test_menu(3);
	prepare_menu(menu, 100, 100);
	menu->selected_index = 0; /* Regular exec item, no submenu */

	bool consumed = wm_menu_handle_key_for(menu, XKB_KEY_Right);
	assert(consumed == true);
	/* Should not crash, just return true */

	wm_menu_destroy(menu);
	printf("  PASS: key_right_no_submenu\n");
}

/* --- Key Left at root (no parent) --- */

static void
test_key_left_at_root(void)
{
	setup_server();
	struct wm_menu *menu = make_test_menu(3);
	prepare_menu(menu, 100, 100);
	menu->parent = NULL;

	bool consumed = wm_menu_handle_key_for(menu, XKB_KEY_Left);
	assert(consumed == true);
	/* Menu should still be visible */
	assert(menu->visible == true);

	wm_menu_destroy(menu);
	printf("  PASS: key_left_at_root\n");
}

/* --- render_menu_title: with different justify modes --- */

static void
test_render_menu_title_justify(void)
{
	setup_server();

	struct wm_menu *menu = make_test_menu(1);
	menu_compute_layout(menu, &test_style);

	/* Left justify (default) */
	test_style.menu_title_justify = WM_JUSTIFY_LEFT;
	cairo_surface_t *s = render_menu_title(menu, &test_style);
	/* Surface is stub, but should not crash */
	(void)s;

	/* Center justify */
	test_style.menu_title_justify = WM_JUSTIFY_CENTER;
	s = render_menu_title(menu, &test_style);
	(void)s;

	/* Right justify */
	test_style.menu_title_justify = WM_JUSTIFY_RIGHT;
	s = render_menu_title(menu, &test_style);
	(void)s;

	wm_menu_destroy(menu);
	printf("  PASS: render_menu_title_justify\n");
}

/* --- render_menu_title: empty title --- */

static void
test_render_menu_title_empty(void)
{
	setup_server();

	struct wm_menu *menu = menu_create(&test_server, NULL);
	struct wm_menu_item *i0 = menu_item_create(WM_MENU_EXEC, "Item");
	wl_list_insert(menu->items.prev, &i0->link);
	menu_compute_layout(menu, &test_style);

	cairo_surface_t *s = render_menu_title(menu, &test_style);
	/* Should handle NULL title */
	(void)s;

	wm_menu_destroy(menu);
	printf("  PASS: render_menu_title_empty\n");
}

/* --- render_menu_items: with selected item --- */

static void
test_render_menu_items_selected(void)
{
	setup_server();

	struct wm_menu *menu = make_test_menu(3);
	menu_compute_layout(menu, &test_style);
	menu->selected_index = 1;

	cairo_surface_t *s = render_menu_items(menu, &test_style);
	/* Should handle selected item highlight rendering */
	(void)s;

	wm_menu_destroy(menu);
	printf("  PASS: render_menu_items_selected\n");
}

/* --- render_menu_items: with icon item --- */

static void
test_render_menu_items_with_icon(void)
{
	setup_server();

	struct wm_menu *menu = menu_create(&test_server, "Icons");
	struct wm_menu_item *i0 = menu_item_create(WM_MENU_EXEC, "Icon Item");
	i0->icon_path = strdup("/path/icon.png");
	i0->command = strdup("test");
	wl_list_insert(menu->items.prev, &i0->link);
	menu_compute_layout(menu, &test_style);

	cairo_surface_t *s = render_menu_items(menu, &test_style);
	(void)s;

	wm_menu_destroy(menu);
	printf("  PASS: render_menu_items_with_icon\n");
}

/* --- render_menu_items: NOP item (disabled/greyed out) --- */

static void
test_render_menu_items_nop(void)
{
	setup_server();

	struct wm_menu *menu = menu_create(&test_server, "NOP Test");
	struct wm_menu_item *nop = menu_item_create(WM_MENU_NOP, "Disabled");
	wl_list_insert(menu->items.prev, &nop->link);
	menu_compute_layout(menu, &test_style);

	cairo_surface_t *s = render_menu_items(menu, &test_style);
	(void)s;

	wm_menu_destroy(menu);
	printf("  PASS: render_menu_items_nop\n");
}

/* --- render_menu_items: submenu bullet shapes --- */

static void
test_render_menu_items_bullet_shapes(void)
{
	setup_server();

	struct wm_menu *menu = menu_create(&test_server, "Bullets");
	struct wm_menu_item *sub = menu_item_create(WM_MENU_SUBMENU, "Sub");
	sub->submenu = menu_create(&test_server, "Child");
	wl_list_insert(menu->items.prev, &sub->link);
	menu_compute_layout(menu, &test_style);

	/* Triangle (default) */
	test_style.menu_bullet = NULL;
	cairo_surface_t *s = render_menu_items(menu, &test_style);
	(void)s;

	/* Square */
	test_style.menu_bullet = "square";
	s = render_menu_items(menu, &test_style);
	(void)s;

	/* Diamond */
	test_style.menu_bullet = "diamond";
	s = render_menu_items(menu, &test_style);
	(void)s;

	/* Empty (skip drawing) */
	test_style.menu_bullet = "empty";
	s = render_menu_items(menu, &test_style);
	(void)s;

	test_style.menu_bullet = NULL;
	wm_menu_destroy(menu);
	printf("  PASS: render_menu_items_bullet_shapes\n");
}

/* --- render_menu_items: bullet position left --- */

static void
test_render_menu_items_bullet_left(void)
{
	setup_server();

	struct wm_menu *menu = menu_create(&test_server, "BulletLeft");
	struct wm_menu_item *sub = menu_item_create(WM_MENU_SUBMENU, "Sub");
	sub->submenu = menu_create(&test_server, "Child");
	wl_list_insert(menu->items.prev, &sub->link);
	menu_compute_layout(menu, &test_style);

	test_style.menu_bullet_position = WM_JUSTIFY_LEFT;
	cairo_surface_t *s = render_menu_items(menu, &test_style);
	(void)s;

	test_style.menu_bullet_position = WM_JUSTIFY_RIGHT;
	wm_menu_destroy(menu);
	printf("  PASS: render_menu_items_bullet_left\n");
}

/* --- maybe_convert_label: unknown encoding --- */

static void
test_maybe_convert_label_unknown(void)
{
	char *label = strdup("test");
	char *r = maybe_convert_label(label, "BOGUS-ENCODING-XYZ");
	/* Should return label unchanged on error */
	assert(r == label);
	assert(strcmp(r, "test") == 0);
	free(r);
	printf("  PASS: maybe_convert_label_unknown\n");
}

/* --- menu_type_ahead: search buffer full --- */

static void
test_type_ahead_buffer_full(void)
{
	setup_server();
	test_config.menu_search = WM_MENU_SEARCH_ITEMSTART;

	struct wm_menu *menu = menu_create(&test_server, "Test");
	struct wm_menu_item *i0 = menu_item_create(WM_MENU_EXEC, "Item");
	wl_list_insert(menu->items.prev, &i0->link);
	prepare_menu(menu, 0, 0);

	/* Fill the search buffer to capacity */
	for (int i = 0; i < 63; i++) {
		menu->search_buf[i] = 'a';
	}
	menu->search_buf[63] = '\0';
	menu->search_len = 63;

	/* Trying to add more should return false */
	bool consumed = menu_type_ahead(menu, 'z');
	assert(consumed == false);

	wm_menu_destroy(menu);
	printf("  PASS: type_ahead_buffer_full\n");
}

/* --- wm_menu_handle_key: key dispatch priority order --- */

static void
test_handle_key_priority_order(void)
{
	setup_server();

	/* Set up all three menus visible; window_list_menu should get priority */
	struct wm_menu *root = make_test_menu(3);
	struct wm_menu *win = make_test_menu(3);
	struct wm_menu *wl = make_test_menu(3);
	prepare_menu(root, 100, 100);
	prepare_menu(win, 100, 100);
	prepare_menu(wl, 100, 100);

	test_server.root_menu = root;
	test_server.window_menu = win;
	test_server.window_list_menu = wl;

	bool consumed = wm_menu_handle_key(&test_server, 0, XKB_KEY_Down, true);
	assert(consumed == true);
	/* window_list_menu should have received the key */
	assert(wl->selected_index == 0);
	/* Others should be unchanged */
	assert(win->selected_index == -1);
	assert(root->selected_index == -1);

	test_server.root_menu = NULL;
	test_server.window_menu = NULL;
	test_server.window_list_menu = NULL;
	wm_menu_destroy(root);
	wm_menu_destroy(win);
	wm_menu_destroy(wl);
	printf("  PASS: handle_key_priority_order\n");
}

/* ===================== Additional coverage tests ===================== */

/* Test wm_menu_show_root toggle behavior: show then hide */
static void
test_show_root_toggle_on_off(void)
{
	setup_server();
	struct wm_menu *root = make_test_menu(3);
	prepare_menu(root, 50, 50);
	test_server.root_menu = root;

	/* First call: menu is already visible, should hide it */
	wm_menu_show_root(&test_server, 60, 70);
	assert(root->visible == false);

	/* Second call: menu is hidden, should show it */
	wm_menu_show_root(&test_server, 60, 70);
	assert(root->visible == true);

	test_server.root_menu = NULL;
	wm_menu_destroy(root);
	printf("  PASS: show_root_toggle_on_off\n");
}

/* Test wm_menu_show_window with no window menu - shows nothing */
static void
test_show_window_no_menu_available(void)
{
	setup_server();
	test_server.window_menu = NULL;
	test_config.window_menu_file = NULL;
	wm_menu_show_window(&test_server, 10, 20);
	/* Should not crash, no menu shown */
	assert(test_server.window_menu == NULL);
	printf("  PASS: show_window_no_menu_available\n");
}

/* Test wm_menu_show_window_list creates dynamic list */
static void
test_show_window_list_dynamic(void)
{
	setup_server();

	/* Add a view */
	struct wm_view view1;
	memset(&view1, 0, sizeof(view1));
	view1.title = "Terminal";
	view1.app_id = "xterm";
	struct wlr_scene_tree scene1;
	memset(&scene1, 0, sizeof(scene1));
	scene1.node.enabled = 1;
	view1.scene_tree = &scene1;
	struct wlr_xdg_toplevel tl1;
	memset(&tl1, 0, sizeof(tl1));
	struct wlr_xdg_surface xs1;
	memset(&xs1, 0, sizeof(xs1));
	struct wlr_surface surf1;
	memset(&surf1, 0, sizeof(surf1));
	xs1.surface = &surf1;
	tl1.base = &xs1;
	view1.xdg_toplevel = &tl1;
	wl_list_insert(&test_server.views, &view1.link);

	wm_menu_show_window_list(&test_server, 100, 200);
	/* A window list menu should have been created */
	assert(test_server.window_list_menu != NULL);

	/* Cleanup */
	wl_list_remove(&view1.link);
	wm_menu_destroy(test_server.window_list_menu);
	test_server.window_list_menu = NULL;
	printf("  PASS: show_window_list_dynamic\n");
}

/* Test wm_menu_show_workspace_menu creates workspace list */
static void
test_show_workspace_menu_dynamic(void)
{
	setup_server();

	/* Add two workspaces */
	struct wm_workspace ws1 = { .name = "Desktop 1", .index = 0 };
	struct wm_workspace ws2 = { .name = "Desktop 2", .index = 1 };
	wl_list_init(&ws1.link);
	wl_list_init(&ws2.link);
	wl_list_insert(&test_server.workspaces, &ws2.link);
	wl_list_insert(&test_server.workspaces, &ws1.link);

	wm_menu_show_workspace_menu(&test_server, 50, 50);
	assert(test_server.workspace_menu != NULL);

	wl_list_remove(&ws1.link);
	wl_list_remove(&ws2.link);
	wm_menu_destroy(test_server.workspace_menu);
	test_server.workspace_menu = NULL;
	printf("  PASS: show_workspace_menu_dynamic\n");
}

/* Test wm_menu_show_custom with null/empty path does nothing */
static void
test_show_custom_empty_path(void)
{
	setup_server();
	wm_menu_show_custom(&test_server, NULL, 10, 20);
	assert(test_server.custom_menu == NULL);
	wm_menu_show_custom(&test_server, "", 10, 20);
	assert(test_server.custom_menu == NULL);
	printf("  PASS: show_custom_empty_path\n");
}

/* Test submenu_delay_cb fires when item has submenu */
static void
test_submenu_delay_cb_fires(void)
{
	setup_server();
	struct wm_menu *menu = make_menu_with_submenu();
	prepare_menu(menu, 100, 100);

	/* Get the submenu item */
	struct wm_menu_item *sub_item = menu_get_item(menu, 1);
	assert(sub_item != NULL);
	assert(sub_item->submenu != NULL);

	/* Set up pending submenu state directly */
	pending_submenu.item = sub_item;
	pending_submenu.sub_x = 200;
	pending_submenu.sub_y = 120;

	submenu_delay_cb(NULL);
	/* After callback, submenu should be shown and pending cleared */
	assert(pending_submenu.item == NULL);
	assert(sub_item->submenu->visible == true);

	wm_menu_destroy(menu);
	printf("  PASS: submenu_delay_cb_fires\n");
}

/* Test submenu_delay_cb with no pending submenu */
static void
test_submenu_delay_cb_no_item(void)
{
	pending_submenu.item = NULL;
	submenu_delay_cb(NULL);
	/* No crash */
	printf("  PASS: submenu_delay_cb_no_item\n");
}

/* Test motion handler with menu_delay > 0 schedules a timer */
static void
test_motion_with_delay(void)
{
	setup_server();
	test_config.menu_delay = 100; /* 100ms delay */

	struct wm_menu *menu = make_menu_with_submenu();
	prepare_menu(menu, 100, 100);
	test_server.root_menu = menu;

	/* Move to the submenu item (index 1) */
	/* The item is at y = menu->y + border + title_height + item_height */
	int item_y = menu->y + menu->border_width + menu->title_height +
		menu->item_height;
	bool handled = wm_menu_handle_motion_for(menu,
		menu->x + 10, item_y + 5);
	assert(handled == true);
	assert(menu->selected_index == 1);

	test_config.menu_delay = 0;
	test_server.root_menu = NULL;
	wm_menu_destroy(menu);
	printf("  PASS: motion_with_delay\n");
}

/* Test h-key at root menu level (no parent) */
static void
test_key_h_at_root(void)
{
	setup_server();
	struct wm_menu *menu = make_test_menu(3);
	prepare_menu(menu, 100, 100);
	menu->selected_index = 0;

	/* h at root has no parent to go back to */
	bool handled = wm_menu_handle_key_for(menu, XKB_KEY_h);
	assert(handled == true);
	/* Menu should remain visible */
	assert(menu->visible == true);

	wm_menu_destroy(menu);
	printf("  PASS: key_h_at_root\n");
}

/* Test wm_menu_handle_button with window_list_menu priority */
static void
test_button_window_list_priority(void)
{
	setup_server();
	struct wm_menu *root = make_test_menu(3);
	struct wm_menu *wl = make_test_menu(3);
	prepare_menu(root, 100, 100);
	prepare_menu(wl, 100, 100);

	test_server.root_menu = root;
	test_server.window_list_menu = wl;

	/* Click in menu area - window_list_menu takes priority */
	bool handled = wm_menu_handle_button(&test_server,
		wl->x + 10, wl->y + wl->border_width + wl->title_height + 5,
		0x110, true);
	assert(handled == true);

	test_server.root_menu = NULL;
	test_server.window_list_menu = NULL;
	wm_menu_destroy(root);
	wm_menu_destroy(wl);
	printf("  PASS: button_window_list_priority\n");
}

/* Test wm_menu_handle_motion with window_list_menu priority */
static void
test_motion_window_list_priority(void)
{
	setup_server();
	struct wm_menu *root = make_test_menu(3);
	struct wm_menu *wl = make_test_menu(3);
	prepare_menu(root, 100, 100);
	prepare_menu(wl, 100, 100);

	test_server.root_menu = root;
	test_server.window_list_menu = wl;

	bool handled = wm_menu_handle_motion(&test_server,
		wl->x + 10, wl->y + wl->border_width + wl->title_height + 5);
	assert(handled == true);

	test_server.root_menu = NULL;
	test_server.window_list_menu = NULL;
	wm_menu_destroy(root);
	wm_menu_destroy(wl);
	printf("  PASS: motion_window_list_priority\n");
}

/* Test wm_menu_handle_motion with null server */
static void
test_motion_null_server(void)
{
	bool handled = wm_menu_handle_motion(NULL, 0, 0);
	assert(handled == false);
	printf("  PASS: motion_null_server\n");
}

/* Test wm_menu_handle_button with null server */
static void
test_button_null_server(void)
{
	bool handled = wm_menu_handle_button(NULL, 0, 0, 0, true);
	assert(handled == false);
	printf("  PASS: button_null_server\n");
}

/* Test wm_menu_handle_motion with window_menu priority */
static void
test_motion_window_menu_priority(void)
{
	setup_server();
	struct wm_menu *root = make_test_menu(3);
	struct wm_menu *win = make_test_menu(3);
	prepare_menu(root, 100, 100);
	prepare_menu(win, 100, 100);

	test_server.root_menu = root;
	test_server.window_menu = win;

	bool handled = wm_menu_handle_motion(&test_server,
		win->x + 10, win->y + win->border_width + win->title_height + 5);
	assert(handled == true);

	test_server.root_menu = NULL;
	test_server.window_menu = NULL;
	wm_menu_destroy(root);
	wm_menu_destroy(win);
	printf("  PASS: motion_window_menu_priority\n");
}

/* Test wm_menu_handle_button with window_menu priority */
static void
test_button_window_menu_priority(void)
{
	setup_server();
	struct wm_menu *root = make_test_menu(3);
	struct wm_menu *win = make_test_menu(3);
	prepare_menu(root, 100, 100);
	prepare_menu(win, 100, 100);

	test_server.root_menu = root;
	test_server.window_menu = win;

	bool handled = wm_menu_handle_button(&test_server,
		win->x + 10, win->y + win->border_width + win->title_height + 5,
		0x110, true);
	assert(handled == true);

	test_server.root_menu = NULL;
	test_server.window_menu = NULL;
	wm_menu_destroy(root);
	wm_menu_destroy(win);
	printf("  PASS: button_window_menu_priority\n");
}

/* Test client menu with a view */
static void
test_show_client_menu_with_views(void)
{
	setup_server();

	struct wm_view view1;
	memset(&view1, 0, sizeof(view1));
	view1.title = "Browser";
	view1.app_id = "firefox";
	struct wlr_scene_tree scene1;
	memset(&scene1, 0, sizeof(scene1));
	scene1.node.enabled = 1;
	view1.scene_tree = &scene1;
	wl_list_insert(&test_server.views, &view1.link);

	wm_menu_show_client_menu(&test_server, NULL, 50, 50);
	assert(test_server.client_menu != NULL);

	wl_list_remove(&view1.link);
	wm_menu_destroy(test_server.client_menu);
	test_server.client_menu = NULL;
	printf("  PASS: show_client_menu_with_views\n");
}

/* Test client menu with filter pattern */
static void
test_show_client_menu_filtered(void)
{
	setup_server();

	struct wm_view view1;
	memset(&view1, 0, sizeof(view1));
	view1.title = "Browser";
	view1.app_id = "firefox";
	struct wlr_scene_tree scene1;
	memset(&scene1, 0, sizeof(scene1));
	scene1.node.enabled = 1;
	view1.scene_tree = &scene1;

	struct wm_view view2;
	memset(&view2, 0, sizeof(view2));
	view2.title = "Editor";
	view2.app_id = "vim";
	struct wlr_scene_tree scene2;
	memset(&scene2, 0, sizeof(scene2));
	scene2.node.enabled = 1;
	view2.scene_tree = &scene2;

	wl_list_insert(&test_server.views, &view2.link);
	wl_list_insert(&test_server.views, &view1.link);

	/* Filter for "fire" should only include firefox */
	wm_menu_show_client_menu(&test_server, "fire", 50, 50);
	assert(test_server.client_menu != NULL);

	wl_list_remove(&view1.link);
	wl_list_remove(&view2.link);
	wm_menu_destroy(test_server.client_menu);
	test_server.client_menu = NULL;
	printf("  PASS: show_client_menu_filtered\n");
}

/* Test type-ahead with space character */
static void
test_type_ahead_space_key(void)
{
	setup_server();
	test_config.menu_search = WM_MENU_SEARCH_SOMEWHERE;

	struct wm_menu *menu = menu_create(&test_server, "Test");
	struct wm_menu_item *i0 = menu_item_create(WM_MENU_EXEC, "No Match");
	struct wm_menu_item *i1 = menu_item_create(WM_MENU_EXEC, "Has Space Here");
	wl_list_insert(menu->items.prev, &i0->link);
	wl_list_insert(menu->items.prev, &i1->link);
	prepare_menu(menu, 0, 0);
	menu->selected_index = -1;

	/* Type space character */
	bool handled = wm_menu_handle_key_for(menu, XKB_KEY_space);
	assert(handled == true);

	wm_menu_destroy(menu);
	printf("  PASS: type_ahead_space_key\n");
}

/* Test motion handler returns false when no menu at position */
static void
test_motion_for_no_hit(void)
{
	setup_server();
	struct wm_menu *menu = make_test_menu(3);
	prepare_menu(menu, 100, 100);

	/* Motion way outside menu */
	bool handled = wm_menu_handle_motion_for(menu, 9999, 9999);
	assert(handled == false);

	wm_menu_destroy(menu);
	printf("  PASS: motion_for_no_hit\n");
}

/* Test button release inside menu returns true (no action) */
static void
test_button_release_inside_menu(void)
{
	setup_server();
	struct wm_menu *menu = make_test_menu(3);
	prepare_menu(menu, 100, 100);
	test_server.root_menu = menu;

	/* Release (not press) inside menu */
	bool handled = wm_menu_handle_button_for(menu,
		menu->x + 10, menu->y + menu->border_width + menu->title_height + 5,
		0x110, false);
	/* Release inside menu should return true (found menu) */
	assert(handled == true);

	test_server.root_menu = NULL;
	wm_menu_destroy(menu);
	printf("  PASS: button_release_inside_menu\n");
}

/* Test wm_menu_show_client_menu with null server */
static void
test_show_client_menu_null_server(void)
{
	wm_menu_show_client_menu(NULL, NULL, 0, 0);
	/* No crash */
	printf("  PASS: show_client_menu_null_server\n");
}

/* Test search_timer_cb resets search state */
static void
test_search_timer_cb_reset(void)
{
	setup_server();
	struct wm_menu *menu = make_test_menu(3);
	prepare_menu(menu, 100, 100);

	/* Manually set search state */
	menu->search_len = 5;
	strcpy(menu->search_buf, "hello");

	search_timer_cb(menu);
	assert(menu->search_len == 0);
	assert(menu->search_buf[0] == '\0');

	wm_menu_destroy(menu);
	printf("  PASS: search_timer_cb_reset\n");
}

/* Test button click on title area (idx < 0) returns true */
static void
test_button_on_title_area(void)
{
	setup_server();
	struct wm_menu *menu = make_test_menu(3);
	prepare_menu(menu, 100, 100);
	test_server.root_menu = menu;

	/* Click on title bar area (y within border + title_height) */
	bool handled = wm_menu_handle_button_for(menu,
		menu->x + 10, menu->y + 5, 0x110, true);
	assert(handled == true);
	/* Menu should still be visible */
	assert(menu->visible == true);

	test_server.root_menu = NULL;
	wm_menu_destroy(menu);
	printf("  PASS: button_on_title_area\n");
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

	/* Execute action types: remaining coverage */
	test_execute_reconfig();
	test_execute_restart();
	test_execute_shade();
	test_execute_shade_no_view();
	test_execute_maximize();
	test_execute_iconify();
	test_execute_close();
	test_execute_close_no_view();
	test_execute_kill();
	test_execute_kill_no_view();
	test_execute_window_entry();
	test_execute_window_entry_iconified();
	test_execute_window_entry_null();
	test_execute_window_entry_stale();

	/* Submenu position flip */
	test_submenu_position_flip();

	/* Directory scanning helpers */
	test_strcmp_qsort();
	test_add_stylesdir_entries();
	test_add_stylesdir_nonexistent();
	test_add_wallpaper_entries();
	test_add_wallpaper_entries_empty();
	test_add_wallpaper_entries_nonexistent();
	test_add_wallpaper_entries_quote_path();
	test_add_stylesmenu_entries();
	test_add_stylesmenu_nonexistent();

	/* Parser: encoding, include, depth, stylesdir, wallpapers, stylesmenu */
	test_parse_encoding_block();
	test_parse_include();
	test_parse_include_dotdot();
	test_parse_max_depth();
	test_parse_stylesdir_tag();
	test_parse_wallpapers_tag();
	test_parse_stylesmenu_tag();

	/* Encoding conversion */
	test_maybe_convert_label_latin1();

	/* Buffer helpers */
	test_wlr_buffer_from_cairo_null();

	/* Custom menu */
	test_show_custom_menu();
	test_show_custom_null();

	/* Misc helpers */
	test_cancel_pending_submenu();
	test_parse_long_line();
	test_parse_window_menu_tags();

	/* Show/hide lifecycle extensions */
	test_show_root_toggle_on_off();
	test_show_window_no_menu_available();
	test_show_window_list_dynamic();
	test_show_workspace_menu_dynamic();
	test_show_custom_empty_path();

	/* Submenu delay */
	test_submenu_delay_cb_fires();
	test_submenu_delay_cb_no_item();
	test_motion_with_delay();

	/* Key navigation extensions */
	test_key_h_at_root();

	/* Type-ahead extensions */
	test_type_ahead_space_key();

	/* Button/Motion priority dispatch */
	test_button_window_list_priority();
	test_motion_window_list_priority();
	test_motion_null_server();
	test_button_null_server();
	test_motion_window_menu_priority();
	test_button_window_menu_priority();
	test_motion_for_no_hit();
	test_button_release_inside_menu();

	/* Client menu */
	test_show_client_menu_with_views();
	test_show_client_menu_filtered();

	/* Additional coverage */
	test_show_client_menu_null_server();
	test_search_timer_cb_reset();
	test_button_on_title_area();

	printf("All menu interaction tests passed.\n");
	return 0;
}
