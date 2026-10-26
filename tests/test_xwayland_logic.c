/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * test_xwayland_logic.c - Unit tests for the XWayland backend
 *
 * xwayland.c is #included with wlroots/XWayland/xcb and the view.c
 * lifecycle replaced by stubs, so these tests check xwayland.c's own
 * policy: classification, routing between managed views and
 * override-redirect windows, ICCCM configure handling, requests, focus
 * stealing prevention and position sync.
 */

#define _POSIX_C_SOURCE 200809L
#define WM_HAS_XWAYLAND

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

/* --- Block real headers via their include guards --- */

#define WAYLAND_SERVER_CORE_H
#define WAYLAND_SERVER_H
#define WAYLAND_UTIL_H
#define WAYLAND_SERVER_PROTOCOL_H
#define WLR_UTIL_LOG_H
#define WLR_UTIL_BOX_H
#define WLR_UTIL_EDGES_H
#define WLR_TYPES_WLR_SCENE_H
#define WLR_TYPES_WLR_CURSOR_H
#define WLR_TYPES_WLR_SEAT_H
#define WLR_TYPES_WLR_OUTPUT_LAYOUT_H
#define WLR_TYPES_WLR_COMPOSITOR_H
#define WLR_XWAYLAND_H
#define WLR_XWAYLAND_XWAYLAND_H
#define __XCB_H__
#define __XCB_ICCCM_H__
#define WM_DECORATION_H
#define WM_SERVER_H
#define WM_SLIT_H
#define WM_TABGROUP_H
#define WM_TOOLBAR_H
#define WM_VIEW_H
#define WM_WORKSPACE_H
#define WM_XWAYLAND_H

/* --- wayland --- */

struct wl_display;
struct wl_client;

struct wl_list {
	struct wl_list *prev;
	struct wl_list *next;
};

static void
wl_list_init(struct wl_list *list)
{
	list->prev = list;
	list->next = list;
}

static void
wl_list_insert(struct wl_list *list, struct wl_list *elm)
{
	elm->prev = list;
	elm->next = list->next;
	list->next = elm;
	elm->next->prev = elm;
}

static void
wl_list_remove(struct wl_list *elm)
{
	elm->prev->next = elm->next;
	elm->next->prev = elm->prev;
	elm->prev = NULL;
	elm->next = NULL;
}

static int
wl_list_length(const struct wl_list *list)
{
	int n = 0;
	for (struct wl_list *e = list->next; e != list; e = e->next)
		n++;
	return n;
}

struct wl_listener {
	struct wl_list link;
	void (*notify)(struct wl_listener *, void *);
};

struct wl_signal {
	struct wl_list listener_list;
};

static void
wl_signal_init(struct wl_signal *signal)
{
	wl_list_init(&signal->listener_list);
}

static void
wl_signal_add(struct wl_signal *signal, struct wl_listener *listener)
{
	wl_list_insert(signal->listener_list.prev, &listener->link);
}

static void
wl_signal_emit(struct wl_signal *signal, void *data)
{
	struct wl_list *pos, *tmp;
	for (pos = signal->listener_list.next, tmp = pos->next;
	     pos != &signal->listener_list; pos = tmp, tmp = pos->next) {
		struct wl_listener *l = (struct wl_listener *)
			((char *)pos - offsetof(struct wl_listener, link));
		l->notify(l, data);
	}
}

#define wl_container_of(ptr, sample, member) \
	(__typeof__(sample))((char *)(ptr) - \
		offsetof(__typeof__(*sample), member))

#define wl_list_for_each(pos, head, member) \
	for (pos = wl_container_of((head)->next, pos, member); \
	     &pos->member != (head); \
	     pos = wl_container_of(pos->member.next, pos, member))

#define wlr_log(verb, fmt, ...) ((void)0)
#define WLR_ERROR 1
#define WLR_INFO 5
#define WLR_DEBUG 7

/* --- wlroots types --- */

struct wlr_box {
	int x, y, width, height;
};

struct wlr_scene_tree;

struct wlr_scene_node {
	struct wlr_scene_tree *parent;
	bool enabled;
	int x, y;
	void *data;
};

struct wlr_scene_tree {
	struct wlr_scene_node node;
};

struct wlr_surface {
	bool mapped;
	struct {
		struct wl_signal map;
		struct wl_signal unmap;
		struct wl_signal commit;
	} events;
};

struct wlr_keyboard_modifiers { uint32_t depressed; };

struct wlr_keyboard {
	uint32_t keycodes[32];
	size_t num_keycodes;
	struct wlr_keyboard_modifiers modifiers;
};

struct wlr_seat {
	struct {
		struct wlr_surface *focused_surface;
	} keyboard_state;
};

struct wlr_cursor { double x, y; };

/* --- xcb --- */

typedef uint32_t xcb_atom_t;
typedef uint32_t xcb_window_t;
typedef struct xcb_connection_t xcb_connection_t;
#define XCB_ATOM_NONE 0
#define XCB_STACK_MODE_ABOVE 0
#define XCB_CONFIG_WINDOW_X 1
#define XCB_CONFIG_WINDOW_Y 2
#define XCB_ICCCM_WM_HINT_STATE (1 << 1)
#define XCB_ICCCM_WM_HINT_X_URGENCY (1 << 8)
#define XCB_ICCCM_WM_STATE_WITHDRAWN 0
#define XCB_ICCCM_SIZE_HINT_US_POSITION (1 << 0)
#define XCB_ICCCM_SIZE_HINT_P_POSITION (1 << 2)
#define XCB_ICCCM_SIZE_HINT_P_MIN_SIZE (1 << 4)
#define XCB_ICCCM_SIZE_HINT_P_MAX_SIZE (1 << 5)
#define XCB_ICCCM_SIZE_HINT_P_RESIZE_INC (1 << 6)
#define XCB_ICCCM_SIZE_HINT_P_ASPECT (1 << 7)
#define XCB_ICCCM_SIZE_HINT_BASE_SIZE (1 << 8)

typedef struct { unsigned int sequence; } xcb_intern_atom_cookie_t;
typedef struct { xcb_atom_t atom; } xcb_intern_atom_reply_t;
typedef struct { uint32_t flags; int32_t initial_state; }
	xcb_icccm_wm_hints_t;
typedef struct {
	uint32_t flags;
	int32_t min_width, min_height, max_width, max_height;
	int32_t width_inc, height_inc;
	int32_t min_aspect_num, min_aspect_den, max_aspect_num, max_aspect_den;
	int32_t base_width, base_height;
} xcb_size_hints_t;

static xcb_intern_atom_cookie_t
xcb_intern_atom(xcb_connection_t *c, uint8_t o, uint16_t l, const char *n)
{
	(void)c; (void)o; (void)l; (void)n;
	xcb_intern_atom_cookie_t cookie = {0};
	return cookie;
}

static xcb_intern_atom_reply_t *
xcb_intern_atom_reply(xcb_connection_t *c, xcb_intern_atom_cookie_t k,
	void *e)
{
	(void)c; (void)k; (void)e;
	return NULL;
}

/* X properties (SetXProp, @NAME patterns): one kept here */
#define XCB_GET_PROPERTY_TYPE_ANY 0
#define XCB_PROP_MODE_REPLACE 0
typedef struct { unsigned int sequence; } xcb_get_property_cookie_t;
typedef struct { xcb_atom_t type; uint8_t format; char value[64];
	int len; } xcb_get_property_reply_t;
static char g_xprop_value[64];
static int g_xprop_len = -1;
static xcb_get_property_cookie_t
xcb_get_property(xcb_connection_t *c, uint8_t d, xcb_window_t w, xcb_atom_t p,
	xcb_atom_t t, uint32_t o, uint32_t l)
{
	(void)c; (void)d; (void)w; (void)p; (void)t; (void)o; (void)l;
	xcb_get_property_cookie_t k = {0};
	return k;
}
static xcb_get_property_reply_t *
xcb_get_property_reply(xcb_connection_t *c, xcb_get_property_cookie_t k,
	void *e)
{
	(void)c; (void)k; (void)e;
	xcb_get_property_reply_t *r = calloc(1, sizeof(*r));
	if (r && g_xprop_len >= 0) {
		r->type = 1;
		r->format = 8;
		r->len = g_xprop_len;
		memcpy(r->value, g_xprop_value, sizeof(r->value));
	}
	return r;
}
static int xcb_get_property_value_length(const xcb_get_property_reply_t *r)
	{ return r->len; }
static void *xcb_get_property_value(const xcb_get_property_reply_t *r)
	{ return (void *)r->value; }
static void
xcb_change_property(xcb_connection_t *c, uint8_t m, xcb_window_t w,
	xcb_atom_t p, xcb_atom_t t, uint8_t f, uint32_t len, const void *data)
{
	(void)c; (void)m; (void)w; (void)p; (void)t; (void)f;
	if (len >= sizeof(g_xprop_value))
		len = sizeof(g_xprop_value) - 1;
	memcpy(g_xprop_value, data, len);
	g_xprop_value[len] = '\0';
	g_xprop_len = (int)len;
}
static int xcb_flush(xcb_connection_t *c) { (void)c; return 0; }

/* --- XWayland --- */

enum wlr_xwayland_surface_decorations {
	WLR_XWAYLAND_SURFACE_DECORATIONS_ALL = 0,
	WLR_XWAYLAND_SURFACE_DECORATIONS_NO_BORDER = 1,
	WLR_XWAYLAND_SURFACE_DECORATIONS_NO_TITLE = 2,
};

enum wlr_xwayland_icccm_input_model {
	WLR_ICCCM_INPUT_MODEL_NONE = 0,
	WLR_ICCCM_INPUT_MODEL_PASSIVE = 1,
	WLR_ICCCM_INPUT_MODEL_LOCAL = 2,
	WLR_ICCCM_INPUT_MODEL_GLOBAL = 3,
};

struct wlr_xwayland_surface {
	xcb_window_t window_id;
	bool override_redirect;
	xcb_atom_t *window_type;
	size_t window_type_len;
	xcb_icccm_wm_hints_t *hints;
	xcb_size_hints_t *size_hints;
	struct wlr_xwayland_surface *parent;
	bool modal;
	char *title, *class, *instance, *role;
	pid_t pid;
	int16_t x, y;
	uint16_t width, height;
	uint32_t decorations;
	bool maximized_vert, maximized_horz, fullscreen;
	struct wlr_surface *surface;
	void *data;
	/* test knobs */
	enum wlr_xwayland_icccm_input_model input_model;
	bool or_wants_focus;
	struct {
		struct wl_signal destroy;
		struct wl_signal associate;
		struct wl_signal dissociate;
		struct wl_signal request_configure;
		struct wl_signal request_move;
		struct wl_signal request_resize;
		struct wl_signal request_maximize;
		struct wl_signal request_fullscreen;
		struct wl_signal request_minimize;
		struct wl_signal request_activate;
		struct wl_signal set_title;
		struct wl_signal set_class;
		struct wl_signal set_role;
		struct wl_signal set_hints;
		struct wl_signal set_override_redirect;
		struct wl_signal set_geometry;
	} events;
};

struct wlr_xwayland_surface_configure_event {
	struct wlr_xwayland_surface *surface;
	int16_t x, y;
	uint16_t width, height;
	uint16_t mask;
};

struct wlr_xwayland_minimize_event {
	struct wlr_xwayland_surface *surface;
	bool minimize;
};

struct wlr_xwayland_resize_event {
	struct wlr_xwayland_surface *surface;
	uint32_t edges;
};

struct wlr_xwayland {
	char *display_name;
	struct {
		struct wl_signal new_surface;
		struct wl_signal ready;
	} events;
};

/* Recorded XWayland calls */
static int g_configure_count;
static int16_t g_cfg_x, g_cfg_y;
static uint16_t g_cfg_w, g_cfg_h;
static int g_activate_count, g_restack_count, g_close_count;
static bool g_last_activated;
static int g_set_max_count, g_set_full_count, g_set_min_count;
static bool g_last_max, g_last_full, g_last_min;

static struct wlr_xwayland *
wlr_xwayland_create(struct wl_display *d, void *c, bool lazy)
{
	(void)d; (void)c; (void)lazy;
	return NULL;
}
static void wlr_xwayland_destroy(struct wlr_xwayland *x) { (void)x; }
static void wlr_xwayland_set_seat(struct wlr_xwayland *x, struct wlr_seat *s)
	{ (void)x; (void)s; }
static xcb_connection_t *
wlr_xwayland_get_xwm_connection(struct wlr_xwayland *x)
	{ (void)x; return NULL; }

static void
wlr_xwayland_surface_configure(struct wlr_xwayland_surface *xs,
	int16_t x, int16_t y, uint16_t w, uint16_t h)
{
	/* Like wlroots, the surface learns its new geometry at once */
	xs->x = x;
	xs->y = y;
	xs->width = w;
	xs->height = h;
	g_configure_count++;
	g_cfg_x = x;
	g_cfg_y = y;
	g_cfg_w = w;
	g_cfg_h = h;
}

static void
wlr_xwayland_surface_activate(struct wlr_xwayland_surface *xs, bool a)
{
	(void)xs;
	g_activate_count++;
	g_last_activated = a;
}

static void
wlr_xwayland_surface_restack(struct wlr_xwayland_surface *xs,
	struct wlr_xwayland_surface *sibling, int mode)
{
	(void)xs; (void)sibling; (void)mode;
	g_restack_count++;
}

static void
wlr_xwayland_surface_close(struct wlr_xwayland_surface *xs)
{
	(void)xs;
	g_close_count++;
}

static void
wlr_xwayland_surface_set_maximized(struct wlr_xwayland_surface *xs,
	bool horz, bool vert)
{
	(void)xs;
	g_set_max_count++;
	g_last_max = horz && vert;
}

static void
wlr_xwayland_surface_set_fullscreen(struct wlr_xwayland_surface *xs, bool f)
{
	(void)xs;
	g_set_full_count++;
	g_last_full = f;
}

static void
wlr_xwayland_surface_set_minimized(struct wlr_xwayland_surface *xs, bool m)
{
	(void)xs;
	g_set_min_count++;
	g_last_min = m;
}

static enum wlr_xwayland_icccm_input_model
wlr_xwayland_surface_icccm_input_model(const struct wlr_xwayland_surface *xs)
{
	return xs->input_model;
}

static bool
wlr_xwayland_surface_override_redirect_wants_focus(
	const struct wlr_xwayland_surface *xs)
{
	return xs->or_wants_focus;
}

/* --- Scene and seat --- */

#define MAX_TREES 64
static struct wlr_scene_tree g_trees[MAX_TREES];
static int g_tree_next;
static int g_tree_destroy_count;

static struct wlr_scene_tree *
new_tree(struct wlr_scene_tree *parent)
{
	assert(g_tree_next < MAX_TREES);
	struct wlr_scene_tree *t = &g_trees[g_tree_next++];
	memset(t, 0, sizeof(*t));
	t->node.parent = parent;
	t->node.enabled = true;
	return t;
}

static struct wlr_scene_tree *
wlr_scene_subsurface_tree_create(struct wlr_scene_tree *parent,
	struct wlr_surface *surface)
{
	(void)surface;
	return new_tree(parent);
}

static void
wlr_scene_node_destroy(struct wlr_scene_node *node)
{
	(void)node;
	g_tree_destroy_count++;
}

static void
wlr_scene_node_set_position(struct wlr_scene_node *node, int x, int y)
{
	node->x = x;
	node->y = y;
}

/* Layout coordinates: sum of positions up the tree */
static bool
wlr_scene_node_coords(struct wlr_scene_node *node, int *lx, int *ly)
{
	int x = 0, y = 0;
	bool enabled = true;
	for (struct wlr_scene_node *n = node; n;
	     n = n->parent ? &n->parent->node : NULL) {
		x += n->x;
		y += n->y;
		enabled &= n->enabled;
	}
	*lx = x;
	*ly = y;
	return enabled;
}

static struct wlr_keyboard g_keyboard;
static struct wlr_surface *g_kb_enter_surface;
static int g_kb_clear_count;

static struct wlr_keyboard *
wlr_seat_get_keyboard(struct wlr_seat *seat)
{
	(void)seat;
	return &g_keyboard;
}

static void
wlr_seat_keyboard_notify_enter(struct wlr_seat *seat,
	struct wlr_surface *surface, uint32_t *keycodes, size_t n,
	struct wlr_keyboard_modifiers *mods)
{
	(void)keycodes; (void)n; (void)mods;
	seat->keyboard_state.focused_surface = surface;
	g_kb_enter_surface = surface;
}

static void
wlr_seat_keyboard_notify_clear_focus(struct wlr_seat *seat)
{
	seat->keyboard_state.focused_surface = NULL;
	g_kb_clear_count++;
}

/* --- fluxland types (must match view.h / server.h / xwayland.h) --- */

enum wm_cursor_mode {
	WM_CURSOR_PASSTHROUGH = 0,
	WM_CURSOR_MOVE,
	WM_CURSOR_RESIZE,
	WM_CURSOR_TABBING,
};

enum wm_view_layer {
	WM_LAYER_DESKTOP = 0,
	WM_LAYER_BELOW,
	WM_LAYER_NORMAL,
	WM_LAYER_ABOVE,
};

enum wm_view_type {
	WM_VIEW_XDG,
	WM_VIEW_XWAYLAND,
};

struct wm_view;

struct wm_size_hints {
	int min_w, min_h, max_w, max_h;
	int base_w, base_h;
	int inc_w, inc_h;
	int min_aspect_x, min_aspect_y, max_aspect_x, max_aspect_y;
};
struct wm_view_impl {
	struct wlr_surface *(*surface)(struct wm_view *view);
	void (*content_geometry)(struct wm_view *view, struct wlr_box *geo);
	void (*set_size)(struct wm_view *view, int width, int height);
	void (*moved)(struct wm_view *view);
	void (*set_activated)(struct wm_view *view, bool activated);
	void (*set_maximized)(struct wm_view *view, bool maximized);
	void (*set_fullscreen)(struct wm_view *view, bool fullscreen);
	void (*set_tiled)(struct wm_view *view, uint32_t edges);
	void (*close)(struct wm_view *view);
	struct wl_client *(*client)(struct wm_view *view);
	pid_t (*pid)(struct wm_view *view);
	struct wm_view *(*parent)(struct wm_view *view);
	void (*set_minimized)(struct wm_view *view, bool minimized);
	void (*size_hints)(struct wm_view *view, struct wm_size_hints *hints);
	bool (*get_xprop)(struct wm_view *view, const char *name, char *buf,
		size_t len);
	void (*set_xprop)(struct wm_view *view, const char *name,
		const char *value);
};

struct wm_decoration { int dummy; };
struct wm_tab_group { int dummy; };

struct wm_view {
	struct wl_list link;
	struct wm_server *server;
	enum wm_view_type type;
	const struct wm_view_impl *impl;
	struct wlr_xwayland_surface *xsurface;
	struct wlr_scene_tree *scene_tree;
	int x, y;
	bool maximized, fullscreen, sticky;
	bool mapped, docked, urgent;
	bool no_decoration, no_focus, focus_hidden, icon_hidden;
	enum wm_view_layer layer;
	char *title, *app_id, *instance, *role;
	struct wm_decoration *decoration;
	struct wm_tab_group *tab_group;
};

struct wm_xwayland_atoms {
	xcb_atom_t net_wm_window_type_normal;
	xcb_atom_t net_wm_window_type_dialog;
	xcb_atom_t net_wm_window_type_utility;
	xcb_atom_t net_wm_window_type_toolbar;
	xcb_atom_t net_wm_window_type_splash;
	xcb_atom_t net_wm_window_type_menu;
	xcb_atom_t net_wm_window_type_dropdown_menu;
	xcb_atom_t net_wm_window_type_popup_menu;
	xcb_atom_t net_wm_window_type_tooltip;
	xcb_atom_t net_wm_window_type_notification;
	xcb_atom_t net_wm_window_type_dock;
	xcb_atom_t net_wm_window_type_desktop;
};

enum wm_xw_window_class {
	WM_XW_MANAGED,
	WM_XW_FLOATING,
	WM_XW_SPLASH,
	WM_XW_POPUP,
	WM_XW_DOCK,
	WM_XW_DESKTOP,
	WM_XW_UNMANAGED,
};

struct wm_slit { int dummy; };
struct wm_toolbar { int dummy; };
struct wm_style { int dummy; };

struct wm_server {
	struct wl_display *wl_display;
	void *compositor;
	struct wlr_seat *seat;
	struct wm_view *focused_view;
	struct wl_list views;
	struct wl_list xwayland_unmanaged;
	struct wm_xwayland_atoms xwayland_atoms;
	struct wlr_xwayland *xwayland;
	struct wl_listener xwayland_new_surface;
	struct wl_listener xwayland_ready;
	struct wlr_scene_tree *xdg_popup_tree;
	struct wm_slit *slit;
	struct wm_toolbar *toolbar;
	struct wm_style *style;
};

/* Prototypes xwayland.h would provide */
enum wm_xw_window_class wm_xwayland_classify(
	struct wlr_xwayland_surface *xsurface,
	const struct wm_xwayland_atoms *atoms);
void wm_xwayland_init(struct wm_server *server);
void wm_xwayland_finish(struct wm_server *server);
void wm_xwayland_sync_positions(struct wm_server *server);

/* --- view.c and friends, recorded --- */

static struct wm_server server;
static struct wlr_seat seat;
static struct wlr_scene_tree popup_tree;
static struct wm_slit slit;

static int g_map_common_count, g_unmap_common_count;
static int g_destroy_common_count;
static bool g_map_want_max, g_map_want_full;
static int g_toggle_max_count, g_toggle_full_count;
static int g_minimize_count, g_unminimize_count;
static int g_focus_count;
static struct wm_view *g_focus_view;
static int g_interactive_count;
static enum wm_cursor_mode g_interactive_mode;
static uint32_t g_interactive_edges;
static int g_slit_add_count;
static int g_refresh_geometry_count;
static int g_iconbar_count;

bool
wm_view_init_common(struct wm_view *view, struct wm_server *srv)
{
	view->server = srv;
	view->layer = WM_LAYER_NORMAL;
	view->scene_tree = new_tree(NULL);
	view->scene_tree->node.enabled = false;
	wl_list_insert(&srv->views, &view->link);
	return true;
}

void
wm_view_map_common(struct wm_view *view, bool want_max, bool want_full)
{
	view->mapped = true;
	view->scene_tree->node.enabled = true;
	g_map_common_count++;
	g_map_want_max = want_max;
	g_map_want_full = want_full;
}

void
wm_view_unmap_common(struct wm_view *view)
{
	view->mapped = false;
	view->scene_tree->node.enabled = false;
	g_unmap_common_count++;
}

void
wm_view_destroy_common(struct wm_view *view)
{
	wl_list_remove(&view->link);
	free(view->title);
	free(view->app_id);
	free(view->instance);
	free(view->role);
	g_destroy_common_count++;
}

void
wm_view_set_title(struct wm_view *view, const char *title)
{
	free(view->title);
	view->title = title ? strdup(title) : NULL;
}

void
wm_view_set_app_id(struct wm_view *view, const char *app_id)
{
	free(view->app_id);
	view->app_id = app_id ? strdup(app_id) : NULL;
}

void
wm_view_move(struct wm_view *view, int x, int y)
{
	view->x = x;
	view->y = y;
	wlr_scene_node_set_position(&view->scene_tree->node, x, y);
	if (view->impl->moved)
		view->impl->moved(view);
}

struct wlr_surface *
wm_view_surface(struct wm_view *view)
{
	return view->impl->surface(view);
}

void
wm_view_toggle_maximize(struct wm_view *view)
{
	view->maximized = !view->maximized;
	g_toggle_max_count++;
}

void
wm_view_toggle_fullscreen(struct wm_view *view)
{
	view->fullscreen = !view->fullscreen;
	g_toggle_full_count++;
}

void
wm_view_set_maximized_state(struct wm_view *view, bool m)
{
	view->impl->set_maximized(view, m);
}

void
wm_view_set_fullscreen_state(struct wm_view *view, bool f)
{
	view->impl->set_fullscreen(view, f);
}

void
wm_view_minimize(struct wm_view *view)
{
	view->scene_tree->node.enabled = false;
	g_minimize_count++;
}

void
wm_view_unminimize(struct wm_view *view)
{
	view->scene_tree->node.enabled = true;
	g_unminimize_count++;
}

void wm_focus_view(struct wm_view *view, struct wlr_surface *surface);
void wm_toolbar_update_iconbar(struct wm_toolbar *toolbar);

void
wm_view_set_urgent(struct wm_view *view, bool urgent)
{
	view->urgent = urgent;
	wm_toolbar_update_iconbar(view->server->toolbar);
}

/* view_focus.c's policy, reduced to what the X11 backend decides */
static bool g_last_activate_allowed;
bool
wm_view_request_activate(struct wm_view *view, bool allowed)
{
	g_last_activate_allowed = allowed;
	if (!allowed) {
		wm_view_set_urgent(view, true);
		return false;
	}
	if (!view->scene_tree->node.enabled)
		wm_view_unminimize(view);
	else
		wm_focus_view(view, NULL);
	return true;
}

void
wm_focus_view(struct wm_view *view, struct wlr_surface *surface)
{
	(void)surface;
	server.focused_view = view;
	g_focus_count++;
	g_focus_view = view;
}

void
wm_view_begin_interactive(struct wm_view *view, enum wm_cursor_mode mode,
	uint32_t edges)
{
	(void)view;
	g_interactive_count++;
	g_interactive_mode = mode;
	g_interactive_edges = edges;
}

void
wm_decoration_refresh_geometry(struct wm_decoration *d, struct wm_style *s)
{
	(void)d; (void)s;
	g_refresh_geometry_count++;
}

void wm_decoration_destroy(struct wm_decoration *d) { (void)d; }

void
wm_tab_group_remove(struct wm_view *view)
{
	view->tab_group = NULL;
}

void
wm_toolbar_update_iconbar(struct wm_toolbar *t)
{
	(void)t;
	g_iconbar_count++;
}

struct wm_slit_client;
struct wm_slit_client *
wm_slit_add_client(struct wm_slit *s, void *surface)
{
	(void)s; (void)surface;
	g_slit_add_count++;
	return NULL;
}

int setenv(const char *name, const char *value, int overwrite);

/* cursor.c */
static void wm_cursor_rebase(struct wm_server *s) { (void)s; }

/* rules.c: an apps entry holding a new window's geometry */
static bool g_hold_pos, g_hold_size;
static void
wm_view_rule_holds_geometry(struct wm_view *v, bool *pos, bool *size)
	{ (void)v; *pos = g_hold_pos; *size = g_hold_size; }

#include "xwayland.c"

/* --- Fixture --- */

#define ATOM_NORMAL 101
#define ATOM_DIALOG 102
#define ATOM_SPLASH 103
#define ATOM_TOOLTIP 104
#define ATOM_DOCK 105
#define ATOM_DESKTOP 106
#define ATOM_NOTIFICATION 107
#define ATOM_UTILITY 108
#define ATOM_POPUP_MENU 109

static void
reset(void)
{
	memset(&server, 0, sizeof(server));
	memset(&seat, 0, sizeof(seat));
	memset(&popup_tree, 0, sizeof(popup_tree));
	popup_tree.node.enabled = true;
	server.seat = &seat;
	server.xdg_popup_tree = &popup_tree;
	server.slit = &slit;
	wl_list_init(&server.views);
	wl_list_init(&server.xwayland_unmanaged);
	struct wm_xwayland_atoms *a = &server.xwayland_atoms;
	a->net_wm_window_type_normal = ATOM_NORMAL;
	a->net_wm_window_type_dialog = ATOM_DIALOG;
	a->net_wm_window_type_splash = ATOM_SPLASH;
	a->net_wm_window_type_tooltip = ATOM_TOOLTIP;
	a->net_wm_window_type_dock = ATOM_DOCK;
	a->net_wm_window_type_desktop = ATOM_DESKTOP;
	a->net_wm_window_type_notification = ATOM_NOTIFICATION;
	a->net_wm_window_type_utility = ATOM_UTILITY;
	a->net_wm_window_type_popup_menu = ATOM_POPUP_MENU;

	g_tree_next = 0;
	g_tree_destroy_count = 0;
	g_configure_count = 0;
	g_activate_count = g_restack_count = g_close_count = 0;
	g_set_max_count = g_set_full_count = g_set_min_count = 0;
	g_map_common_count = g_unmap_common_count = 0;
	g_destroy_common_count = 0;
	g_toggle_max_count = g_toggle_full_count = 0;
	g_minimize_count = g_unminimize_count = 0;
	g_focus_count = 0;
	g_focus_view = NULL;
	g_interactive_count = 0;
	g_slit_add_count = 0;
	g_refresh_geometry_count = 0;
	g_iconbar_count = 0;
	g_kb_enter_surface = NULL;
	g_kb_clear_count = 0;
}

struct fake_x11 {
	struct wlr_xwayland_surface xs;
	struct wlr_surface surface;
	xcb_atom_t types[4];
	xcb_icccm_wm_hints_t hints;
	xcb_size_hints_t size_hints;
};

static void
fake_init(struct fake_x11 *f, bool override_redirect)
{
	memset(f, 0, sizeof(*f));
	f->xs.override_redirect = override_redirect;
	f->xs.width = 400;
	f->xs.height = 300;
	f->xs.pid = 1000;
	f->xs.input_model = WLR_ICCCM_INPUT_MODEL_PASSIVE;
	wl_signal_init(&f->xs.events.destroy);
	wl_signal_init(&f->xs.events.associate);
	wl_signal_init(&f->xs.events.dissociate);
	wl_signal_init(&f->xs.events.request_configure);
	wl_signal_init(&f->xs.events.request_move);
	wl_signal_init(&f->xs.events.request_resize);
	wl_signal_init(&f->xs.events.request_maximize);
	wl_signal_init(&f->xs.events.request_fullscreen);
	wl_signal_init(&f->xs.events.request_minimize);
	wl_signal_init(&f->xs.events.request_activate);
	wl_signal_init(&f->xs.events.set_title);
	wl_signal_init(&f->xs.events.set_class);
	wl_signal_init(&f->xs.events.set_role);
	wl_signal_init(&f->xs.events.set_hints);
	wl_signal_init(&f->xs.events.set_override_redirect);
	wl_signal_init(&f->xs.events.set_geometry);
	wl_signal_init(&f->surface.events.map);
	wl_signal_init(&f->surface.events.unmap);
	wl_signal_init(&f->surface.events.commit);
}

static void
fake_set_type(struct fake_x11 *f, xcb_atom_t type)
{
	f->types[0] = type;
	f->xs.window_type = f->types;
	f->xs.window_type_len = 1;
}

/* new_surface + associate (+ map) as XWayland would */
static void
fake_create(struct fake_x11 *f)
{
	handle_xwayland_new_surface(&server.xwayland_new_surface, &f->xs);
	f->xs.surface = &f->surface;
	wl_signal_emit(&f->xs.events.associate, NULL);
}

static void
fake_map(struct fake_x11 *f)
{
	f->surface.mapped = true;
	wl_signal_emit(&f->surface.events.map, NULL);
}

static void
fake_unmap(struct fake_x11 *f)
{
	f->surface.mapped = false;
	wl_signal_emit(&f->surface.events.unmap, NULL);
}

static void
fake_destroy(struct fake_x11 *f)
{
	f->xs.surface = NULL;
	wl_signal_emit(&f->xs.events.dissociate, NULL);
	wl_signal_emit(&f->xs.events.destroy, NULL);
}

static struct wm_view *
view_of(struct fake_x11 *f)
{
	struct wm_xwayland_view *xview = f->xs.data;
	return xview ? &xview->view : NULL;
}

/* --- Classification --- */

static void
test_classify(void)
{
	reset();
	struct fake_x11 f;
	const struct wm_xwayland_atoms *a = &server.xwayland_atoms;

	fake_init(&f, true);
	assert(wm_xwayland_classify(&f.xs, a) == WM_XW_UNMANAGED);

	fake_init(&f, false);
	assert(wm_xwayland_classify(&f.xs, a) == WM_XW_MANAGED);
	fake_set_type(&f, ATOM_NORMAL);
	assert(wm_xwayland_classify(&f.xs, a) == WM_XW_MANAGED);
	fake_set_type(&f, ATOM_DOCK);
	assert(wm_xwayland_classify(&f.xs, a) == WM_XW_DOCK);
	fake_set_type(&f, ATOM_DESKTOP);
	assert(wm_xwayland_classify(&f.xs, a) == WM_XW_DESKTOP);
	fake_set_type(&f, ATOM_TOOLTIP);
	assert(wm_xwayland_classify(&f.xs, a) == WM_XW_POPUP);
	fake_set_type(&f, ATOM_NOTIFICATION);
	assert(wm_xwayland_classify(&f.xs, a) == WM_XW_POPUP);
	fake_set_type(&f, ATOM_POPUP_MENU);
	assert(wm_xwayland_classify(&f.xs, a) == WM_XW_POPUP);
	fake_set_type(&f, ATOM_SPLASH);
	assert(wm_xwayland_classify(&f.xs, a) == WM_XW_SPLASH);
	fake_set_type(&f, ATOM_DIALOG);
	assert(wm_xwayland_classify(&f.xs, a) == WM_XW_FLOATING);
	fake_set_type(&f, ATOM_UTILITY);
	assert(wm_xwayland_classify(&f.xs, a) == WM_XW_FLOATING);

	/* Transient or modal without a type: floating */
	struct fake_x11 parent;
	fake_init(&parent, false);
	fake_init(&f, false);
	f.xs.parent = &parent.xs;
	assert(wm_xwayland_classify(&f.xs, a) == WM_XW_FLOATING);
	fake_init(&f, false);
	f.xs.modal = true;
	assert(wm_xwayland_classify(&f.xs, a) == WM_XW_FLOATING);

	/* An unresolved atom (XCB_ATOM_NONE) never matches */
	fake_init(&f, false);
	fake_set_type(&f, XCB_ATOM_NONE);
	server.xwayland_atoms.net_wm_window_type_dock = XCB_ATOM_NONE;
	assert(wm_xwayland_classify(&f.xs, a) == WM_XW_MANAGED);
	printf("  PASS: classify\n");
}

/* --- Routing --- */

static void
test_new_surface_routing(void)
{
	reset();
	struct fake_x11 managed, or;
	fake_init(&managed, false);
	fake_init(&or, true);
	fake_create(&managed);
	fake_create(&or);

	struct wm_view *v = view_of(&managed);
	assert(v && v->type == WM_VIEW_XWAYLAND);
	assert(v->impl == &xwayland_view_impl);
	assert(v->xsurface == &managed.xs);
	assert(wl_list_length(&server.views) == 1);
	assert(wl_list_length(&server.xwayland_unmanaged) == 1);

	fake_destroy(&managed);
	fake_destroy(&or);
	assert(wl_list_length(&server.views) == 0);
	assert(wl_list_length(&server.xwayland_unmanaged) == 0);
	assert(g_destroy_common_count == 1);
	printf("  PASS: new_surface_routing\n");
}

/* --- Map --- */

static void
test_map_normal_window(void)
{
	reset();
	struct fake_x11 f;
	fake_init(&f, false);
	f.xs.title = "Terminal";
	f.xs.class = "XTerm";
	f.xs.instance = "xterm";
	f.xs.role = "main";
	f.xs.x = 300;
	f.xs.y = 200;
	fake_create(&f);
	fake_map(&f);

	struct wm_view *v = view_of(&f);
	assert(g_map_common_count == 1);
	assert(v->mapped);
	assert(strcmp(v->title, "Terminal") == 0);
	assert(strcmp(v->app_id, "XTerm") == 0);
	assert(strcmp(v->instance, "xterm") == 0);
	assert(strcmp(v->role, "main") == 0);
	assert(!v->no_decoration && !v->no_focus);
	/* No USPosition/PPosition: placement decides */
	assert(v->x == 0 && v->y == 0);
	assert(!g_map_want_max && !g_map_want_full);

	fake_unmap(&f);
	fake_destroy(&f);
	printf("  PASS: map_normal_window\n");
}

static void
test_map_keeps_client_position(void)
{
	reset();
	struct fake_x11 f;
	fake_init(&f, false);
	f.xs.x = 300;
	f.xs.y = 200;
	f.size_hints.flags = XCB_ICCCM_SIZE_HINT_US_POSITION;
	f.xs.size_hints = &f.size_hints;
	f.xs.maximized_vert = f.xs.maximized_horz = true;
	f.xs.fullscreen = true;
	fake_create(&f);
	fake_map(&f);

	struct wm_view *v = view_of(&f);
	assert(v->x == 300 && v->y == 200);
	assert(g_map_want_max && g_map_want_full);

	fake_unmap(&f);
	fake_destroy(&f);
	printf("  PASS: map_keeps_client_position\n");
}

static void
test_map_window_types(void)
{
	struct { xcb_atom_t type; bool deco; bool focus; bool hidden;
		enum wm_view_layer layer; bool sticky; } cases[] = {
		{ ATOM_SPLASH,  false, true,  true,  WM_LAYER_NORMAL,  false },
		{ ATOM_TOOLTIP, false, false, true,  WM_LAYER_ABOVE,   false },
		{ ATOM_DESKTOP, false, true,  true,  WM_LAYER_DESKTOP, true },
		{ ATOM_DIALOG,  true,  true,  false, WM_LAYER_NORMAL,  false },
	};
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		reset();
		struct fake_x11 f;
		fake_init(&f, false);
		fake_set_type(&f, cases[i].type);
		fake_create(&f);
		fake_map(&f);
		struct wm_view *v = view_of(&f);
		assert(v->no_decoration == !cases[i].deco);
		assert(v->no_focus == !cases[i].focus);
		assert(v->focus_hidden == cases[i].hidden);
		assert(v->icon_hidden == cases[i].hidden);
		assert(v->layer == cases[i].layer);
		assert(v->sticky == cases[i].sticky);
		fake_unmap(&f);
		fake_destroy(&f);
	}
	printf("  PASS: map_window_types\n");
}

static void
test_map_motif_and_input_model(void)
{
	reset();
	struct fake_x11 f;
	fake_init(&f, false);
	f.xs.decorations = WLR_XWAYLAND_SURFACE_DECORATIONS_NO_TITLE;
	f.xs.input_model = WLR_ICCCM_INPUT_MODEL_NONE;
	fake_create(&f);
	fake_map(&f);
	struct wm_view *v = view_of(&f);
	assert(v->no_decoration);
	assert(v->no_focus);
	fake_unmap(&f);
	fake_destroy(&f);
	printf("  PASS: map_motif_and_input_model\n");
}

static void
test_map_dockapps_go_to_slit(void)
{
	reset();
	struct fake_x11 dock, withdrawn;
	fake_init(&dock, false);
	fake_set_type(&dock, ATOM_DOCK);
	fake_init(&withdrawn, false);
	withdrawn.hints.flags = XCB_ICCCM_WM_HINT_STATE;
	withdrawn.hints.initial_state = XCB_ICCCM_WM_STATE_WITHDRAWN;
	withdrawn.xs.hints = &withdrawn.hints;
	fake_create(&dock);
	fake_create(&withdrawn);
	fake_map(&dock);
	fake_map(&withdrawn);
	assert(g_slit_add_count == 2);
	assert(g_map_common_count == 0);
	assert(view_of(&dock)->docked && view_of(&withdrawn)->docked);

	/* Without a slit they're ordinary windows */
	reset();
	server.slit = NULL;
	fake_init(&dock, false);
	fake_set_type(&dock, ATOM_DOCK);
	fake_create(&dock);
	fake_map(&dock);
	assert(g_slit_add_count == 0 && g_map_common_count == 1);
	fake_unmap(&dock);
	fake_destroy(&dock);
	printf("  PASS: map_dockapps_go_to_slit\n");
}

static void
test_unmap_and_remap(void)
{
	reset();
	struct fake_x11 f;
	fake_init(&f, false);
	fake_create(&f);
	fake_map(&f);
	struct wm_view *v = view_of(&f);
	int trees_before = g_tree_destroy_count;
	fake_unmap(&f);
	assert(g_unmap_common_count == 1);
	assert(!v->mapped);
	assert(g_tree_destroy_count == trees_before + 1);

	/* X11 windows can come back (withdraw + map) */
	fake_map(&f);
	assert(g_map_common_count == 2);
	assert(v->mapped);
	fake_unmap(&f);
	fake_destroy(&f);
	printf("  PASS: unmap_and_remap\n");
}

/* --- Configure requests --- */

static void
emit_configure(struct fake_x11 *f, int x, int y, int w, int h,
	uint16_t mask)
{
	struct wlr_xwayland_surface_configure_event ev = {
		.surface = &f->xs, .x = x, .y = y, .width = w, .height = h,
		.mask = mask,
	};
	wl_signal_emit(&f->xs.events.request_configure, &ev);
}

static void
test_configure_before_map(void)
{
	reset();
	struct fake_x11 f;
	fake_init(&f, false);
	fake_create(&f);
	emit_configure(&f, 10, 20, 640, 480, 0);
	assert(g_configure_count == 1);
	assert(g_cfg_x == 10 && g_cfg_y == 20);
	assert(g_cfg_w == 640 && g_cfg_h == 480);
	fake_destroy(&f);
	printf("  PASS: configure_before_map\n");
}

static void
test_configure_mapped(void)
{
	reset();
	struct fake_x11 f;
	fake_init(&f, false);
	fake_create(&f);
	fake_map(&f);
	struct wm_view *v = view_of(&f);
	struct wm_xwayland_view *xview = f.xs.data;

	/* Frame at (100, 50); client area offset by the decoration */
	wm_view_move(v, 100, 50);
	wlr_scene_node_set_position(&xview->surface_tree->node, 1, 19);
	wm_xwayland_sync_positions(&server);
	assert(f.xs.x == 101 && f.xs.y == 69);

	/* Size-only request: honoured where the window is */
	g_configure_count = 0;
	emit_configure(&f, 0, 0, 500, 350, 0);
	assert(g_configure_count == 1);
	assert(g_cfg_x == 101 && g_cfg_y == 69);
	assert(g_cfg_w == 500 && g_cfg_h == 350);

	/* Move request: the frame follows the client area */
	emit_configure(&f, 301, 219, 500, 350,
		XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y);
	assert(v->x == 300 && v->y == 200);
	assert(g_cfg_x == 301 && g_cfg_y == 219);

	/* Maximized windows keep the geometry we gave them */
	v->maximized = true;
	emit_configure(&f, 0, 0, 10, 10, 0);
	assert(g_cfg_w == 500 && g_cfg_h == 350);
	v->maximized = false;

	/* Just opened with a remembered position and size: the program's
	 * own move and resize don't take (configureRequestEvent) */
	f.xs.width = 500;
	f.xs.height = 350;
	g_hold_pos = g_hold_size = true;
	emit_configure(&f, 11, 12, 20, 30,
		XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y);
	assert(v->x == 300 && v->y == 200);
	assert(g_cfg_w == 500 && g_cfg_h == 350);
	/* Only the position held: the size goes through */
	g_hold_size = false;
	emit_configure(&f, 11, 12, 640, 480,
		XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y);
	assert(v->x == 300 && g_cfg_w == 640 && g_cfg_h == 480);
	g_hold_pos = false;

	fake_unmap(&f);
	fake_destroy(&f);
	printf("  PASS: configure_mapped\n");
}

/* --- Other requests --- */

static void
test_requests(void)
{
	reset();
	struct fake_x11 f;
	fake_init(&f, false);
	fake_create(&f);

	/* Before map: state comes from the surface at map time */
	f.xs.maximized_vert = f.xs.maximized_horz = true;
	wl_signal_emit(&f.xs.events.request_maximize, NULL);
	assert(g_toggle_max_count == 0);
	fake_map(&f);
	struct wm_view *v = view_of(&f);
	v->maximized = true;

	/* Same state again: just re-send it */
	wl_signal_emit(&f.xs.events.request_maximize, NULL);
	assert(g_toggle_max_count == 0);
	assert(g_set_max_count == 1 && g_last_max);
	/* Unmaximize */
	f.xs.maximized_vert = false;
	wl_signal_emit(&f.xs.events.request_maximize, NULL);
	assert(g_toggle_max_count == 1);

	f.xs.fullscreen = true;
	wl_signal_emit(&f.xs.events.request_fullscreen, NULL);
	assert(g_toggle_full_count == 1);

	struct wlr_xwayland_minimize_event min = { &f.xs, true };
	wl_signal_emit(&f.xs.events.request_minimize, &min);
	assert(g_minimize_count == 1);
	min.minimize = false;
	wl_signal_emit(&f.xs.events.request_minimize, &min);
	assert(g_unminimize_count == 1);

	wl_signal_emit(&f.xs.events.request_move, NULL);
	assert(g_interactive_count == 1 &&
		g_interactive_mode == WM_CURSOR_MOVE);
	struct wlr_xwayland_resize_event rs = { &f.xs, 6 };
	wl_signal_emit(&f.xs.events.request_resize, &rs);
	assert(g_interactive_mode == WM_CURSOR_RESIZE &&
		g_interactive_edges == 6);

	fake_unmap(&f);
	fake_destroy(&f);
	printf("  PASS: requests\n");
}

static void
test_activate_focus_stealing(void)
{
	reset();
	struct fake_x11 a, b, c;
	fake_init(&a, false);
	fake_init(&b, false);
	fake_init(&c, false);
	a.xs.pid = 10;
	b.xs.pid = 10;
	c.xs.pid = 20;
	fake_create(&a);
	fake_create(&b);
	fake_create(&c);
	fake_map(&a);
	fake_map(&b);
	fake_map(&c);

	/* Nothing focused: granted */
	server.focused_view = NULL;
	wl_signal_emit(&a.xs.events.request_activate, NULL);
	assert(g_focus_view == view_of(&a));

	/* Same client: granted */
	wl_signal_emit(&b.xs.events.request_activate, NULL);
	assert(g_focus_view == view_of(&b));

	/* Another client: refused, marked urgent */
	g_focus_count = 0;
	wl_signal_emit(&c.xs.events.request_activate, NULL);
	assert(g_focus_count == 0);
	assert(view_of(&c)->urgent);
	assert(g_iconbar_count > 0);

	/* A minimized window of the same client is restored */
	view_of(&a)->scene_tree->node.enabled = false;
	wl_signal_emit(&a.xs.events.request_activate, NULL);
	assert(g_unminimize_count == 1);

	fake_unmap(&a);
	fake_unmap(&b);
	fake_unmap(&c);
	fake_destroy(&a);
	fake_destroy(&b);
	fake_destroy(&c);
	printf("  PASS: activate_focus_stealing\n");
}

static void
test_urgency_hint(void)
{
	reset();
	struct fake_x11 f;
	fake_init(&f, false);
	fake_create(&f);
	fake_map(&f);
	f.hints.flags = XCB_ICCCM_WM_HINT_X_URGENCY;
	f.xs.hints = &f.hints;
	wl_signal_emit(&f.xs.events.set_hints, NULL);
	assert(view_of(&f)->urgent);
	f.hints.flags = 0;
	wl_signal_emit(&f.xs.events.set_hints, NULL);
	assert(!view_of(&f)->urgent);
	fake_unmap(&f);
	fake_destroy(&f);
	printf("  PASS: urgency_hint\n");
}

static void
test_property_changes(void)
{
	reset();
	struct fake_x11 f;
	fake_init(&f, false);
	fake_create(&f);
	fake_map(&f);
	struct wm_view *v = view_of(&f);
	f.xs.title = "New";
	wl_signal_emit(&f.xs.events.set_title, NULL);
	assert(strcmp(v->title, "New") == 0);
	f.xs.class = "Klass";
	f.xs.instance = "inst";
	wl_signal_emit(&f.xs.events.set_class, NULL);
	assert(strcmp(v->app_id, "Klass") == 0);
	assert(strcmp(v->instance, "inst") == 0);
	f.xs.role = "dialog";
	wl_signal_emit(&f.xs.events.set_role, NULL);
	assert(strcmp(v->role, "dialog") == 0);
	wl_signal_emit(&f.surface.events.commit, NULL);
	assert(g_refresh_geometry_count == 0); /* no decoration */
	fake_unmap(&f);
	fake_destroy(&f);
	printf("  PASS: property_changes\n");
}

/* --- Backend operations --- */

static void
test_backend_ops(void)
{
	reset();
	struct fake_x11 parent, child;
	fake_init(&parent, false);
	fake_init(&child, false);
	child.xs.parent = &parent.xs;
	child.xs.pid = 4242;
	fake_create(&parent);
	fake_create(&child);
	fake_map(&parent);
	fake_map(&child);
	struct wm_view *v = view_of(&child);
	const struct wm_view_impl *impl = v->impl;

	assert(impl->surface(v) == &child.surface);
	struct wlr_box geo;
	impl->content_geometry(v, &geo);
	assert(geo.x == 0 && geo.y == 0 && geo.width == 400 &&
		geo.height == 300);
	assert(impl->pid(v) == 4242);
	assert(impl->client(v) == NULL);
	assert(impl->parent(v) == view_of(&parent));
	assert(impl->parent(view_of(&parent)) == NULL);

	impl->set_activated(v, true);
	assert(g_activate_count == 1 && g_last_activated);
	assert(g_restack_count == 1);
	impl->set_activated(v, false);
	assert(g_restack_count == 1);

	impl->close(v);
	assert(g_close_count == 1);
	impl->set_minimized(v, true);
	assert(g_set_min_count == 1 && g_last_min);
	impl->set_fullscreen(v, true);
	assert(g_set_full_count == 1 && g_last_full);

	/* set_size keeps the position; <= 0 keeps that dimension */
	wm_view_move(v, 40, 30);
	impl->set_size(v, 800, 0);
	assert(g_cfg_x == 40 && g_cfg_y == 30);
	assert(g_cfg_w == 800 && g_cfg_h == 300);

	fake_unmap(&child);
	fake_unmap(&parent);
	fake_destroy(&child);
	fake_destroy(&parent);
	printf("  PASS: backend_ops\n");
}

static void
test_sync_positions(void)
{
	reset();
	struct fake_x11 f, hidden;
	fake_init(&f, false);
	fake_init(&hidden, false);
	fake_create(&f);
	fake_create(&hidden);
	fake_map(&f);

	/* Code that moves views directly (no wm_view_move) */
	struct wm_view *v = view_of(&f);
	v->x = 500;
	v->y = 400;
	wlr_scene_node_set_position(&v->scene_tree->node, 500, 400);
	g_configure_count = 0;
	wm_xwayland_sync_positions(&server);
	assert(g_configure_count == 1);
	assert(f.xs.x == 500 && f.xs.y == 400);
	/* Already in sync: nothing sent */
	wm_xwayland_sync_positions(&server);
	assert(g_configure_count == 1);

	fake_unmap(&f);
	fake_destroy(&f);
	fake_destroy(&hidden);
	printf("  PASS: sync_positions\n");
}

/* --- Override-redirect windows --- */

static void
test_unmanaged(void)
{
	reset();
	struct fake_x11 menu;
	fake_init(&menu, true);
	menu.xs.x = 120;
	menu.xs.y = 80;
	menu.xs.or_wants_focus = true;
	fake_create(&menu);
	struct wm_xwayland_unmanaged *u = menu.xs.data;

	fake_map(&menu);
	assert(u->scene_tree);
	assert(u->scene_tree->node.parent == &popup_tree);
	assert(u->scene_tree->node.x == 120 && u->scene_tree->node.y == 80);
	assert(g_kb_enter_surface == &menu.surface);
	assert(g_map_common_count == 0);

	menu.xs.x = 130;
	wl_signal_emit(&menu.xs.events.set_geometry, NULL);
	assert(u->scene_tree->node.x == 130);

	emit_configure(&menu, 140, 90, 50, 60, 0);
	assert(u->scene_tree->node.x == 140 && u->scene_tree->node.y == 90);
	assert(g_cfg_w == 50 && g_cfg_h == 60);

	/* Unmap returns the keyboard to the focused window */
	struct fake_x11 win;
	fake_init(&win, false);
	fake_create(&win);
	fake_map(&win);
	server.focused_view = view_of(&win);
	seat.keyboard_state.focused_surface = &menu.surface;
	fake_unmap(&menu);
	assert(u->scene_tree == NULL);
	assert(g_kb_enter_surface == &win.surface);

	fake_destroy(&menu);
	assert(wl_list_length(&server.xwayland_unmanaged) == 0);
	fake_unmap(&win);
	fake_destroy(&win);
	printf("  PASS: unmanaged\n");
}

static void
test_override_redirect_changes(void)
{
	reset();
	struct fake_x11 f;
	fake_init(&f, false);
	fake_create(&f);
	fake_map(&f);
	assert(wl_list_length(&server.views) == 1);

	/* Becomes override-redirect while mapped */
	f.xs.override_redirect = true;
	wl_signal_emit(&f.xs.events.set_override_redirect, NULL);
	assert(wl_list_length(&server.views) == 0);
	assert(wl_list_length(&server.xwayland_unmanaged) == 1);
	struct wm_xwayland_unmanaged *u = f.xs.data;
	assert(u && u->scene_tree);

	/* And back */
	f.xs.override_redirect = false;
	wl_signal_emit(&f.xs.events.set_override_redirect, NULL);
	assert(wl_list_length(&server.xwayland_unmanaged) == 0);
	assert(wl_list_length(&server.views) == 1);
	assert(view_of(&f)->mapped);

	fake_unmap(&f);
	fake_destroy(&f);
	assert(wl_list_length(&server.views) == 0);
	printf("  PASS: override_redirect_changes\n");
}

int
main(void)
{
	printf("test_xwayland_logic:\n");

	test_classify();
	test_new_surface_routing();
	test_map_normal_window();
	test_map_keeps_client_position();
	test_map_window_types();
	test_map_motif_and_input_model();
	test_map_dockapps_go_to_slit();
	test_unmap_and_remap();
	test_configure_before_map();
	test_configure_mapped();
	test_requests();
	test_activate_focus_stealing();
	test_urgency_hint();
	test_property_changes();
	test_backend_ops();
	test_sync_positions();
	test_unmanaged();
	test_override_redirect_changes();

	printf("All xwayland_logic tests passed.\n");
	return 0;
}
