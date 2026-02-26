/*
 * test_xwayland_logic.c - Unit tests for xwayland.c classification logic
 *
 * Tests window type classification, slit docking decisions, and
 * coordinate clamping. Includes xwayland.c directly with stubs
 * for all wlroots/wayland/XCB dependencies.
 *
 * Requires WM_HAS_XWAYLAND to be defined (the real code is behind
 * that guard).
 */

#define _POSIX_C_SOURCE 200809L
#define WM_HAS_XWAYLAND

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

/* --- Block real headers via their include guards --- */

/* wayland */
#define WAYLAND_SERVER_CORE_H
#define WAYLAND_SERVER_H
#define WAYLAND_UTIL_H
#define WAYLAND_SERVER_PROTOCOL_H

/* wlroots */
#define WLR_UTIL_LOG_H
#define WLR_UTIL_BOX_H
#define WLR_TYPES_WLR_SCENE_H
#define WLR_TYPES_WLR_CURSOR_H
#define WLR_TYPES_WLR_SEAT_H
#define WLR_TYPES_WLR_XDG_SHELL_H
#define WLR_TYPES_WLR_OUTPUT_LAYOUT_H
#define WLR_TYPES_WLR_OUTPUT_H
#define WLR_TYPES_WLR_COMPOSITOR_H
#define WLR_XWAYLAND_H
#define WLR_XWAYLAND_XWAYLAND_H

/* xcb */
#define __XCB_H__

/* fluxland */
#define WM_SERVER_H
#define WM_SLIT_H
#define WM_VIEW_H
#define WM_WORKSPACE_H
#define WM_XWAYLAND_H

/* --- Stub wayland types --- */

struct wl_display;
struct wl_event_loop;
struct wl_event_source;

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

struct wl_listener {
	struct wl_list link;
	void (*notify)(struct wl_listener *, void *);
};

struct wl_signal {
	struct wl_list listener_list;
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

#define wl_container_of(ptr, sample, member) \
	(__typeof__(sample))((char *)(ptr) - \
		offsetof(__typeof__(*sample), member))

#define wl_list_for_each(pos, head, member) \
	for (pos = wl_container_of((head)->next, pos, member); \
	     &pos->member != (head); \
	     pos = wl_container_of(pos->member.next, pos, member))

/* --- Stub wlr types --- */

#define wlr_log(verb, fmt, ...) ((void)0)
#define WLR_ERROR 1
#define WLR_INFO 5
#define WLR_DEBUG 7

struct wlr_box {
	int x, y, width, height;
};

/* Scene graph */
enum wlr_scene_node_type {
	WLR_SCENE_NODE_TREE,
	WLR_SCENE_NODE_BUFFER,
};

struct wlr_scene_tree;

struct wlr_scene_node {
	enum wlr_scene_node_type type;
	struct wlr_scene_tree *parent;
	struct wl_list link;
	bool enabled;
	int x, y;
	void *data;
};

struct wlr_scene_tree {
	struct wlr_scene_node node;
	struct wl_list children;
};

struct wlr_scene {
	struct wlr_scene_tree tree;
};

/* Surface */
struct wlr_surface {
	struct {
		struct wl_signal map;
		struct wl_signal unmap;
	} events;
};

/* XDG shell */
struct wlr_xdg_surface { struct wlr_surface *surface; };
struct wlr_xdg_toplevel {
	struct wlr_xdg_surface *base;
};

/* Seat */
struct wlr_keyboard {
	uint32_t keycodes[32];
	size_t num_keycodes;
	struct { uint32_t mods; } modifiers;
};

struct wlr_seat {
	struct {
		struct wlr_surface *focused_surface;
	} keyboard_state;
};

struct wlr_cursor { double x, y; };
struct wlr_output { char *name; };
struct wlr_output_layout { int dummy; };

/* XCB stubs */
typedef uint32_t xcb_atom_t;
typedef struct xcb_connection_t xcb_connection_t;

#define XCB_ATOM_NONE 0

typedef struct {
	unsigned int sequence;
} xcb_intern_atom_cookie_t;

typedef struct {
	xcb_atom_t atom;
} xcb_intern_atom_reply_t;

static xcb_intern_atom_cookie_t
xcb_intern_atom(xcb_connection_t *conn, uint8_t only_if_exists,
	uint16_t name_len, const char *name)
{
	(void)conn; (void)only_if_exists; (void)name_len; (void)name;
	xcb_intern_atom_cookie_t cookie = {0};
	return cookie;
}

static xcb_intern_atom_reply_t *
xcb_intern_atom_reply(xcb_connection_t *conn,
	xcb_intern_atom_cookie_t cookie, void *error)
{
	(void)conn; (void)cookie; (void)error;
	return NULL;
}

/* XWayland stubs */
struct wlr_xwayland_surface;

typedef struct {
	int initial_state;
} xcb_icccm_wm_hints_t;

struct wlr_xwayland_surface {
	uint32_t window_id;
	bool override_redirect;
	xcb_atom_t *window_type;
	size_t window_type_len;
	xcb_icccm_wm_hints_t *hints;
	struct wlr_xwayland_surface *parent;
	char *title;
	char *class;
	int x, y;
	int width, height;
	struct wlr_surface *surface;
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
		struct wl_signal set_hints;
		struct wl_signal set_override_redirect;
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

struct wlr_xwayland {
	struct wl_display *display;
	char *display_name;
	struct {
		struct wl_signal new_surface;
		struct wl_signal ready;
	} events;
};

/* wlr_xwayland function stubs */
static struct wlr_xwayland *
wlr_xwayland_create(struct wl_display *display, void *compositor, bool lazy)
{
	(void)display; (void)compositor; (void)lazy;
	return NULL;
}

static void wlr_xwayland_destroy(struct wlr_xwayland *xwl) { (void)xwl; }

static void wlr_xwayland_set_seat(struct wlr_xwayland *xwl,
	struct wlr_seat *seat)
{
	(void)xwl; (void)seat;
}

static xcb_connection_t *
wlr_xwayland_get_xwm_connection(struct wlr_xwayland *xwl)
{
	(void)xwl;
	return NULL;
}

static void wlr_xwayland_surface_activate(
	struct wlr_xwayland_surface *surface, bool activated)
{
	(void)surface; (void)activated;
}

static void wlr_xwayland_surface_configure(
	struct wlr_xwayland_surface *surface,
	int16_t x, int16_t y, uint16_t w, uint16_t h)
{
	(void)surface; (void)x; (void)y; (void)w; (void)h;
}

static void wlr_xwayland_surface_set_maximized(
	struct wlr_xwayland_surface *surface, bool maximized)
{
	(void)surface; (void)maximized;
}

static void wlr_xwayland_surface_set_fullscreen(
	struct wlr_xwayland_surface *surface, bool fullscreen)
{
	(void)surface; (void)fullscreen;
}

static void wlr_xwayland_surface_set_minimized(
	struct wlr_xwayland_surface *surface, bool minimized)
{
	(void)surface; (void)minimized;
}

static struct wlr_xwayland_surface *
wlr_xwayland_surface_try_from_wlr_surface(struct wlr_surface *surface)
{
	(void)surface;
	return NULL;
}

static struct wlr_xdg_toplevel *
wlr_xdg_toplevel_try_from_wlr_surface(struct wlr_surface *surface)
{
	(void)surface;
	return NULL;
}

/* Scene graph function stubs */
static void wlr_scene_node_destroy(struct wlr_scene_node *node) { (void)node; }
static void wlr_scene_node_raise_to_top(struct wlr_scene_node *node) { (void)node; }
static void wlr_scene_node_set_position(struct wlr_scene_node *node,
	int x, int y)
{
	node->x = x; node->y = y;
}
static void wlr_scene_node_set_enabled(struct wlr_scene_node *node, bool e)
{
	node->enabled = e;
}

static struct wlr_scene_tree *
wlr_scene_tree_create(struct wlr_scene_tree *parent) { (void)parent; return NULL; }

static struct wlr_scene_tree *
wlr_scene_subsurface_tree_create(struct wlr_scene_tree *parent,
	struct wlr_surface *surface)
{
	(void)parent; (void)surface;
	return NULL;
}

static struct wlr_keyboard *
wlr_seat_get_keyboard(struct wlr_seat *seat)
{
	(void)seat;
	return NULL;
}

static void wlr_seat_keyboard_notify_enter(struct wlr_seat *seat,
	struct wlr_surface *surface, uint32_t *keycodes, size_t num_keycodes,
	void *modifiers)
{
	(void)seat; (void)surface; (void)keycodes;
	(void)num_keycodes; (void)modifiers;
}

static void wlr_seat_keyboard_notify_clear_focus(struct wlr_seat *seat)
{
	(void)seat;
}

static struct wlr_output *
wlr_output_layout_output_at(struct wlr_output_layout *layout,
	double x, double y)
{
	(void)layout; (void)x; (void)y;
	return NULL;
}

static void wlr_output_layout_get_box(struct wlr_output_layout *layout,
	struct wlr_output *output, struct wlr_box *box)
{
	(void)layout; (void)output;
	box->x = 0; box->y = 0; box->width = 1920; box->height = 1080;
}

static void wlr_xdg_toplevel_set_activated(struct wlr_xdg_toplevel *tl,
	bool activated)
{
	(void)tl; (void)activated;
}

/* --- Stub fluxland types and functions --- */

/* Cursor mode enum */
enum wm_cursor_mode {
	WM_CURSOR_PASSTHROUGH = 0,
	WM_CURSOR_MOVE,
	WM_CURSOR_RESIZE,
	WM_CURSOR_TABBING,
};

/* View (minimal) */
struct wm_view {
	struct wlr_xdg_toplevel *xdg_toplevel;
	struct wlr_scene_tree *scene_tree;
	int x, y;
	struct wl_list link;
};

/* Workspace (minimal) */
struct wm_workspace {
	struct wlr_scene_tree *tree;
	struct wl_list link;
};

/* Slit */
struct wm_slit { int dummy; };

static void wm_slit_add_client(struct wm_slit *slit,
	struct wlr_xwayland_surface *surface)
{
	(void)slit; (void)surface;
}

/* Atoms struct — from xwayland.h */
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
	WM_XW_UNMANAGED,
};

struct wm_xwayland_view {
	struct wm_server *server;
	struct wlr_xwayland_surface *xsurface;
	struct wlr_scene_tree *scene_tree;
	int x, y;
	bool mapped;
	struct wlr_box saved_geometry;
	bool maximized;
	bool fullscreen;
	enum wm_xw_window_class window_class;
	char *title;
	char *app_id;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener destroy;
	struct wl_listener associate;
	struct wl_listener dissociate;
	struct wl_listener request_configure;
	struct wl_listener request_move;
	struct wl_listener request_resize;
	struct wl_listener request_maximize;
	struct wl_listener request_fullscreen;
	struct wl_listener request_minimize;
	struct wl_listener request_activate;
	struct wl_listener set_title;
	struct wl_listener set_class;
	struct wl_listener set_hints;
	struct wl_listener set_override_redirect;
	struct wl_list link;
};

/* Server (minimal, with the fields xwayland.c needs) */
struct wm_server {
	struct wl_display *wl_display;
	struct wlr_scene *scene;
	struct wlr_scene_tree *view_tree;
	struct wlr_scene_tree *xdg_popup_tree;
	struct wlr_output_layout *output_layout;
	struct wlr_cursor *cursor;
	struct wlr_seat *seat;
	struct wlr_compositor *compositor;

	struct wm_slit *slit;

	enum wm_cursor_mode cursor_mode;
	struct wm_view *grabbed_view;
	struct wm_view *focused_view;

	struct wl_list views;
	struct wl_list xwayland_views;

	struct wlr_xwayland *xwayland;
	struct wm_xwayland_atoms xwayland_atoms;
	struct wl_listener xwayland_new_surface;
	struct wl_listener xwayland_ready;
};

static void wm_focus_view(struct wm_view *view, struct wlr_surface *surface)
{
	(void)view; (void)surface;
}

static struct wm_workspace *wm_workspace_get_active(struct wm_server *server)
{
	(void)server;
	return NULL;
}

/* --- Now include xwayland.c to get the static functions --- */
#include "xwayland.c"

/* ================================================================ */
/*  Test helpers                                                     */
/* ================================================================ */

static int tests_run = 0;
static int tests_passed = 0;

#define RUN_TEST(fn) do { \
	tests_run++; \
	printf("  %-55s ", #fn); \
	fn(); \
	tests_passed++; \
	printf("PASS\n"); \
} while (0)

/* Build a test atoms struct with unique, non-zero atom values */
static struct wm_xwayland_atoms
make_test_atoms(void)
{
	struct wm_xwayland_atoms atoms = {
		.net_wm_window_type_normal       = 100,
		.net_wm_window_type_dialog       = 101,
		.net_wm_window_type_utility      = 102,
		.net_wm_window_type_toolbar      = 103,
		.net_wm_window_type_splash       = 104,
		.net_wm_window_type_menu         = 105,
		.net_wm_window_type_dropdown_menu = 106,
		.net_wm_window_type_popup_menu   = 107,
		.net_wm_window_type_tooltip      = 108,
		.net_wm_window_type_notification = 109,
		.net_wm_window_type_dock         = 110,
		.net_wm_window_type_desktop      = 111,
	};
	return atoms;
}

/* Build a minimal xwayland surface */
static struct wlr_xwayland_surface
make_xsurface(void)
{
	struct wlr_xwayland_surface xs;
	memset(&xs, 0, sizeof(xs));
	xs.override_redirect = false;
	xs.window_type = NULL;
	xs.window_type_len = 0;
	xs.hints = NULL;
	xs.parent = NULL;
	xs.title = NULL;
	xs.class = NULL;
	return xs;
}

/* ================================================================ */
/*  Tests: classify_window                                           */
/* ================================================================ */

static void test_classify_override_redirect_is_unmanaged(void)
{
	struct wm_xwayland_atoms atoms = make_test_atoms();
	struct wlr_xwayland_surface xs = make_xsurface();
	xs.override_redirect = true;

	enum wm_xw_window_class cls = classify_window(&xs, &atoms);
	assert(cls == WM_XW_UNMANAGED);
}

static void test_classify_normal_type(void)
{
	struct wm_xwayland_atoms atoms = make_test_atoms();
	struct wlr_xwayland_surface xs = make_xsurface();
	xcb_atom_t types[] = { atoms.net_wm_window_type_normal };
	xs.window_type = types;
	xs.window_type_len = 1;

	enum wm_xw_window_class cls = classify_window(&xs, &atoms);
	assert(cls == WM_XW_MANAGED);
}

static void test_classify_dialog_is_floating(void)
{
	struct wm_xwayland_atoms atoms = make_test_atoms();
	struct wlr_xwayland_surface xs = make_xsurface();
	xcb_atom_t types[] = { atoms.net_wm_window_type_dialog };
	xs.window_type = types;
	xs.window_type_len = 1;

	enum wm_xw_window_class cls = classify_window(&xs, &atoms);
	assert(cls == WM_XW_FLOATING);
}

static void test_classify_utility_is_floating(void)
{
	struct wm_xwayland_atoms atoms = make_test_atoms();
	struct wlr_xwayland_surface xs = make_xsurface();
	xcb_atom_t types[] = { atoms.net_wm_window_type_utility };
	xs.window_type = types;
	xs.window_type_len = 1;

	enum wm_xw_window_class cls = classify_window(&xs, &atoms);
	assert(cls == WM_XW_FLOATING);
}

static void test_classify_toolbar_is_floating(void)
{
	struct wm_xwayland_atoms atoms = make_test_atoms();
	struct wlr_xwayland_surface xs = make_xsurface();
	xcb_atom_t types[] = { atoms.net_wm_window_type_toolbar };
	xs.window_type = types;
	xs.window_type_len = 1;

	enum wm_xw_window_class cls = classify_window(&xs, &atoms);
	assert(cls == WM_XW_FLOATING);
}

static void test_classify_menu_is_floating(void)
{
	struct wm_xwayland_atoms atoms = make_test_atoms();
	struct wlr_xwayland_surface xs = make_xsurface();
	xcb_atom_t types[] = { atoms.net_wm_window_type_menu };
	xs.window_type = types;
	xs.window_type_len = 1;

	enum wm_xw_window_class cls = classify_window(&xs, &atoms);
	assert(cls == WM_XW_FLOATING);
}

static void test_classify_splash_type(void)
{
	struct wm_xwayland_atoms atoms = make_test_atoms();
	struct wlr_xwayland_surface xs = make_xsurface();
	xcb_atom_t types[] = { atoms.net_wm_window_type_splash };
	xs.window_type = types;
	xs.window_type_len = 1;

	enum wm_xw_window_class cls = classify_window(&xs, &atoms);
	assert(cls == WM_XW_SPLASH);
}

static void test_classify_tooltip_is_unmanaged(void)
{
	struct wm_xwayland_atoms atoms = make_test_atoms();
	struct wlr_xwayland_surface xs = make_xsurface();
	xcb_atom_t types[] = { atoms.net_wm_window_type_tooltip };
	xs.window_type = types;
	xs.window_type_len = 1;

	enum wm_xw_window_class cls = classify_window(&xs, &atoms);
	assert(cls == WM_XW_UNMANAGED);
}

static void test_classify_notification_is_unmanaged(void)
{
	struct wm_xwayland_atoms atoms = make_test_atoms();
	struct wlr_xwayland_surface xs = make_xsurface();
	xcb_atom_t types[] = { atoms.net_wm_window_type_notification };
	xs.window_type = types;
	xs.window_type_len = 1;

	enum wm_xw_window_class cls = classify_window(&xs, &atoms);
	assert(cls == WM_XW_UNMANAGED);
}

static void test_classify_popup_menu_is_unmanaged(void)
{
	struct wm_xwayland_atoms atoms = make_test_atoms();
	struct wlr_xwayland_surface xs = make_xsurface();
	xcb_atom_t types[] = { atoms.net_wm_window_type_popup_menu };
	xs.window_type = types;
	xs.window_type_len = 1;

	enum wm_xw_window_class cls = classify_window(&xs, &atoms);
	assert(cls == WM_XW_UNMANAGED);
}

static void test_classify_dropdown_menu_is_unmanaged(void)
{
	struct wm_xwayland_atoms atoms = make_test_atoms();
	struct wlr_xwayland_surface xs = make_xsurface();
	xcb_atom_t types[] = { atoms.net_wm_window_type_dropdown_menu };
	xs.window_type = types;
	xs.window_type_len = 1;

	enum wm_xw_window_class cls = classify_window(&xs, &atoms);
	assert(cls == WM_XW_UNMANAGED);
}

static void test_classify_dock_is_unmanaged(void)
{
	struct wm_xwayland_atoms atoms = make_test_atoms();
	struct wlr_xwayland_surface xs = make_xsurface();
	xcb_atom_t types[] = { atoms.net_wm_window_type_dock };
	xs.window_type = types;
	xs.window_type_len = 1;

	enum wm_xw_window_class cls = classify_window(&xs, &atoms);
	assert(cls == WM_XW_UNMANAGED);
}

static void test_classify_desktop_is_unmanaged(void)
{
	struct wm_xwayland_atoms atoms = make_test_atoms();
	struct wlr_xwayland_surface xs = make_xsurface();
	xcb_atom_t types[] = { atoms.net_wm_window_type_desktop };
	xs.window_type = types;
	xs.window_type_len = 1;

	enum wm_xw_window_class cls = classify_window(&xs, &atoms);
	assert(cls == WM_XW_UNMANAGED);
}

static void test_classify_no_type_no_parent_is_managed(void)
{
	struct wm_xwayland_atoms atoms = make_test_atoms();
	struct wlr_xwayland_surface xs = make_xsurface();
	/* No window_type, no parent → managed (default) */

	enum wm_xw_window_class cls = classify_window(&xs, &atoms);
	assert(cls == WM_XW_MANAGED);
}

static void test_classify_no_type_with_parent_is_floating(void)
{
	struct wm_xwayland_atoms atoms = make_test_atoms();
	struct wlr_xwayland_surface parent = make_xsurface();
	struct wlr_xwayland_surface xs = make_xsurface();
	xs.parent = &parent;

	enum wm_xw_window_class cls = classify_window(&xs, &atoms);
	assert(cls == WM_XW_FLOATING);
}

static void test_classify_override_redirect_ignores_type(void)
{
	struct wm_xwayland_atoms atoms = make_test_atoms();
	struct wlr_xwayland_surface xs = make_xsurface();
	/* Even with normal type, override-redirect wins */
	xcb_atom_t types[] = { atoms.net_wm_window_type_normal };
	xs.window_type = types;
	xs.window_type_len = 1;
	xs.override_redirect = true;

	enum wm_xw_window_class cls = classify_window(&xs, &atoms);
	assert(cls == WM_XW_UNMANAGED);
}

static void test_classify_first_match_wins(void)
{
	struct wm_xwayland_atoms atoms = make_test_atoms();
	struct wlr_xwayland_surface xs = make_xsurface();
	/* First type is splash, second is normal — splash should win */
	xcb_atom_t types[] = {
		atoms.net_wm_window_type_splash,
		atoms.net_wm_window_type_normal,
	};
	xs.window_type = types;
	xs.window_type_len = 2;

	enum wm_xw_window_class cls = classify_window(&xs, &atoms);
	assert(cls == WM_XW_SPLASH);
}

static void test_classify_unknown_type_falls_through(void)
{
	struct wm_xwayland_atoms atoms = make_test_atoms();
	struct wlr_xwayland_surface xs = make_xsurface();
	/* Unknown atom value not matching any known type */
	xcb_atom_t types[] = { 999 };
	xs.window_type = types;
	xs.window_type_len = 1;
	/* No parent → managed by default */

	enum wm_xw_window_class cls = classify_window(&xs, &atoms);
	assert(cls == WM_XW_MANAGED);
}

/* ================================================================ */
/*  Tests: should_dock_in_slit                                       */
/* ================================================================ */

static void test_dock_type_should_dock(void)
{
	struct wm_xwayland_atoms atoms = make_test_atoms();
	struct wlr_xwayland_surface xs = make_xsurface();
	xcb_atom_t types[] = { atoms.net_wm_window_type_dock };
	xs.window_type = types;
	xs.window_type_len = 1;

	assert(should_dock_in_slit(&xs, &atoms) == true);
}

static void test_normal_type_should_not_dock(void)
{
	struct wm_xwayland_atoms atoms = make_test_atoms();
	struct wlr_xwayland_surface xs = make_xsurface();
	xcb_atom_t types[] = { atoms.net_wm_window_type_normal };
	xs.window_type = types;
	xs.window_type_len = 1;

	assert(should_dock_in_slit(&xs, &atoms) == false);
}

static void test_withdrawn_dockapp_should_dock(void)
{
	struct wm_xwayland_atoms atoms = make_test_atoms();
	struct wlr_xwayland_surface xs = make_xsurface();
	/* Classic dockapp: no type, not OR, withdrawn state hint */
	xcb_icccm_wm_hints_t hints = { .initial_state = 0 };
	xs.hints = &hints;

	assert(should_dock_in_slit(&xs, &atoms) == true);
}

static void test_no_type_no_hints_should_not_dock(void)
{
	struct wm_xwayland_atoms atoms = make_test_atoms();
	struct wlr_xwayland_surface xs = make_xsurface();
	/* No type, no hints → don't dock */

	assert(should_dock_in_slit(&xs, &atoms) == false);
}

static void test_override_redirect_with_withdrawn_should_not_dock(void)
{
	struct wm_xwayland_atoms atoms = make_test_atoms();
	struct wlr_xwayland_surface xs = make_xsurface();
	xcb_icccm_wm_hints_t hints = { .initial_state = 0 };
	xs.hints = &hints;
	xs.override_redirect = true;
	/* OR windows are excluded from withdrawn dockapp check */

	assert(should_dock_in_slit(&xs, &atoms) == false);
}

static void test_has_type_with_withdrawn_should_not_dock(void)
{
	struct wm_xwayland_atoms atoms = make_test_atoms();
	struct wlr_xwayland_surface xs = make_xsurface();
	/* Has a (non-dock) type, and hints with withdrawn — type check
	 * doesn't match dock, and withdrawn check requires no types */
	xcb_atom_t types[] = { atoms.net_wm_window_type_normal };
	xs.window_type = types;
	xs.window_type_len = 1;
	xcb_icccm_wm_hints_t hints = { .initial_state = 0 };
	xs.hints = &hints;

	assert(should_dock_in_slit(&xs, &atoms) == false);
}

static void test_non_withdrawn_state_should_not_dock(void)
{
	struct wm_xwayland_atoms atoms = make_test_atoms();
	struct wlr_xwayland_surface xs = make_xsurface();
	/* Hints with NormalState (1), not WithdrawnState (0) */
	xcb_icccm_wm_hints_t hints = { .initial_state = 1 };
	xs.hints = &hints;

	assert(should_dock_in_slit(&xs, &atoms) == false);
}

/* ================================================================ */
/*  Tests: clamp_coord / clamp_size                                  */
/* ================================================================ */

static void test_clamp_coord_normal_value(void)
{
	assert(clamp_coord(100) == 100);
	assert(clamp_coord(-100) == -100);
	assert(clamp_coord(0) == 0);
}

static void test_clamp_coord_extreme_positive(void)
{
	/* Values > XWAYLAND_COORD_MAX (32767) are clamped */
	int16_t result = clamp_coord(32767);
	assert(result == 32767);
	/* 32767 is the max int16_t, so it's already at the limit */
}

static void test_clamp_coord_extreme_negative(void)
{
	int16_t result = clamp_coord(-32767);
	assert(result == -32767);
}

static void test_clamp_size_normal(void)
{
	assert(clamp_size(800) == 800);
	assert(clamp_size(1) == 1);
}

static void test_clamp_size_zero_becomes_one(void)
{
	assert(clamp_size(0) == 1);
}

static void test_clamp_size_large_is_clamped(void)
{
	/* XWAYLAND_SIZE_MAX is 16384 */
	assert(clamp_size(16384) == 16384);
	assert(clamp_size(16385) == 16384);
	assert(clamp_size(60000) == 16384);
}

/* ================================================================ */
/*  main                                                             */
/* ================================================================ */

int main(void)
{
	printf("test_xwayland_logic: xwayland.c classification & clamping tests\n");

	printf("\n--- classify_window ---\n");
	RUN_TEST(test_classify_override_redirect_is_unmanaged);
	RUN_TEST(test_classify_normal_type);
	RUN_TEST(test_classify_dialog_is_floating);
	RUN_TEST(test_classify_utility_is_floating);
	RUN_TEST(test_classify_toolbar_is_floating);
	RUN_TEST(test_classify_menu_is_floating);
	RUN_TEST(test_classify_splash_type);
	RUN_TEST(test_classify_tooltip_is_unmanaged);
	RUN_TEST(test_classify_notification_is_unmanaged);
	RUN_TEST(test_classify_popup_menu_is_unmanaged);
	RUN_TEST(test_classify_dropdown_menu_is_unmanaged);
	RUN_TEST(test_classify_dock_is_unmanaged);
	RUN_TEST(test_classify_desktop_is_unmanaged);
	RUN_TEST(test_classify_no_type_no_parent_is_managed);
	RUN_TEST(test_classify_no_type_with_parent_is_floating);
	RUN_TEST(test_classify_override_redirect_ignores_type);
	RUN_TEST(test_classify_first_match_wins);
	RUN_TEST(test_classify_unknown_type_falls_through);

	printf("\n--- should_dock_in_slit ---\n");
	RUN_TEST(test_dock_type_should_dock);
	RUN_TEST(test_normal_type_should_not_dock);
	RUN_TEST(test_withdrawn_dockapp_should_dock);
	RUN_TEST(test_no_type_no_hints_should_not_dock);
	RUN_TEST(test_override_redirect_with_withdrawn_should_not_dock);
	RUN_TEST(test_has_type_with_withdrawn_should_not_dock);
	RUN_TEST(test_non_withdrawn_state_should_not_dock);

	printf("\n--- clamp_coord / clamp_size ---\n");
	RUN_TEST(test_clamp_coord_normal_value);
	RUN_TEST(test_clamp_coord_extreme_positive);
	RUN_TEST(test_clamp_coord_extreme_negative);
	RUN_TEST(test_clamp_size_normal);
	RUN_TEST(test_clamp_size_zero_becomes_one);
	RUN_TEST(test_clamp_size_large_is_clamped);

	printf("\n%d/%d tests passed\n", tests_passed, tests_run);
	return tests_passed == tests_run ? 0 : 1;
}
