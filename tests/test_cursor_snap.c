/*
 * test_cursor_snap.c - Unit tests for cursor.c snap zone detection,
 * snap geometry calculation, region-to-context mapping, and
 * interactive move/resize geometry math.
 *
 * Includes cursor.c directly with stub implementations to avoid
 * needing wlroots/wayland libraries at link time.
 */

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stddef.h>
#include <math.h>

/* --- Block real headers via their include guards --- */

/* wayland */
#define WAYLAND_SERVER_CORE_H
#define WAYLAND_SERVER_H
#define WAYLAND_UTIL_H
#define WAYLAND_SERVER_PROTOCOL_H

/* wlroots */
#define WLR_UTIL_LOG_H
#define WLR_UTIL_EDGES_H
#define WLR_UTIL_BOX_H
#define WLR_TYPES_WLR_SCENE_H
#define WLR_TYPES_WLR_CURSOR_H
#define WLR_TYPES_WLR_SEAT_H
#define WLR_TYPES_WLR_XDG_SHELL_H
#define WLR_TYPES_WLR_OUTPUT_LAYOUT_H
#define WLR_TYPES_WLR_OUTPUT_H
#define WLR_TYPES_WLR_COMPOSITOR_H
#define WLR_TYPES_WLR_SUBCOMPOSITOR_H
#define WLR_TYPES_WLR_DATA_DEVICE_H
#define WLR_TYPES_WLR_XCURSOR_MANAGER_H
#define WLR_TYPES_WLR_LAYER_SHELL_V1_H
#define WLR_TYPES_WLR_XDG_DECORATION_V1_H
#define WLR_TYPES_WLR_KEYBOARD_H
#define WLR_TYPES_WLR_TOUCH_H
#define WLR_TYPES_WLR_POINTER_GESTURES_V1_H
#define WLR_TYPES_WLR_RELATIVE_POINTER_V1_H
#define WLR_TYPES_WLR_BUFFER_H
#define WLR_INTERFACES_WLR_BUFFER_H
#define WLR_BACKEND_H
#define WLR_RENDER_ALLOCATOR_H
#define WLR_RENDER_WLR_RENDERER_H

/* fluxland */
#define WM_ANIMATION_H
#define WM_CONFIG_H
#define WM_CURSOR_H
#define WM_DECORATION_H
#define WM_FOCUS_NAV_H
#define WM_FOREIGN_TOPLEVEL_H
#define WM_FRACTIONAL_SCALE_H
#define WM_GAMMA_CONTROL_H
#define WM_IDLE_H
#define WM_IPC_H
#define WM_KEYBIND_H
#define WM_MOUSEBIND_H
#define WM_MENU_H
#define WM_OUTPUT_H
#define WM_OUTPUT_MANAGEMENT_H
#define WM_PLACEMENT_H
#define WM_PROTOCOLS_H
#define WM_RENDER_H
#define WM_RULES_H
#define WM_SCREENCOPY_H
#define WM_SERVER_H
#define WM_SESSION_LOCK_H
#define WM_SLIT_H
#define WM_STYLE_H
#define WM_TABGROUP_H
#define WM_TEXT_INPUT_H
#define WM_TOOLBAR_H
#define WM_TRANSIENT_SEAT_H
#define WM_UTIL_H
#define WM_VIEW_H
#define WM_VIEWPORTER_H
#define WM_WORKSPACE_H
#define WM_XWAYLAND_H
#define WM_DRM_LEASE_H
#define WM_DRM_SYNCOBJ_H
#define WM_INPUT_H
#define WM_KEYBOARD_H
#define WM_LAYER_SHELL_H

/* Block linux input event codes, cairo, and drm headers */
#define _INPUT_EVENT_CODES_H
#define _LINUX_INPUT_EVENT_CODES_H
#define CAIRO_H

/* Block drm_fourcc.h (system header guard is DRM_FOURCC_H) */
#define DRM_FOURCC_H

/* --- Stub wayland types --- */

struct wl_display;
struct wl_event_loop;
struct wl_event_source;
struct wl_resource;
struct wl_client;

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
wl_signal_add(struct wl_signal *signal, struct wl_listener *listener)
{
	wl_list_insert(signal->listener_list.prev, &listener->link);
}

static inline int
wl_event_source_timer_update(struct wl_event_source *source, int ms_delay)
{
	(void)source;
	(void)ms_delay;
	return 0;
}

static inline struct wl_event_source *
wl_event_loop_add_timer(struct wl_event_loop *loop,
	int (*func)(void *), void *data)
{
	(void)loop; (void)func; (void)data;
	return NULL;
}

#define wl_container_of(ptr, sample, member) \
	(__typeof__(sample))((char *)(ptr) - \
		offsetof(__typeof__(*sample), member))

#define wl_list_for_each(pos, head, member) \
	for (pos = wl_container_of((head)->next, pos, member); \
	     &pos->member != (head); \
	     pos = wl_container_of(pos->member.next, pos, member))

#define wl_list_for_each_safe(pos, tmp, head, member) \
	for (pos = wl_container_of((head)->next, pos, member), \
	     tmp = wl_container_of((pos)->member.next, tmp, member); \
	     &(pos)->member != (head); \
	     pos = tmp, \
	     tmp = wl_container_of((pos)->member.next, tmp, member))

/* --- Stub wlr types --- */

/* wlr_log no-op */
#define wlr_log(verb, fmt, ...) ((void)0)
#define WLR_ERROR 1
#define WLR_DEBUG 7

/* Edges */
enum wlr_edges {
	WLR_EDGE_NONE   = 0,
	WLR_EDGE_TOP    = 1,
	WLR_EDGE_BOTTOM = 2,
	WLR_EDGE_LEFT   = 4,
	WLR_EDGE_RIGHT  = 8,
};

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

struct wlr_scene_buffer {
	struct wlr_scene_node node;
};

struct wlr_scene_rect {
	struct wlr_scene_node node;
};

struct wlr_scene_surface {
	struct wlr_surface *surface;
};

struct wlr_scene_output_layout;

/* Buffer interface */
struct wlr_buffer {
	int width, height;
	int ref_count;
};

struct wlr_buffer_impl {
	void (*destroy)(struct wlr_buffer *buffer);
	bool (*begin_data_ptr_access)(struct wlr_buffer *buffer,
		uint32_t flags, void **data, uint32_t *format, size_t *stride);
	void (*end_data_ptr_access)(struct wlr_buffer *buffer);
};

#define WLR_BUFFER_DATA_PTR_ACCESS_WRITE 2

static inline void
wlr_buffer_init(struct wlr_buffer *buffer,
	const struct wlr_buffer_impl *impl, int width, int height)
{
	(void)impl;
	buffer->width = width;
	buffer->height = height;
	buffer->ref_count = 1;
}

static inline void
wlr_buffer_drop(struct wlr_buffer *buffer)
{
	(void)buffer;
}

/* Output */
struct wlr_output {
	char *name;
};

struct wlr_output_layout {
	int dummy;
};

/* Surface */
struct wlr_surface {
	int dummy;
};

/* XDG shell */
struct wlr_xdg_surface {
	struct wlr_surface *surface;
	void *data;
};

struct wlr_xdg_toplevel {
	struct wlr_xdg_surface *base;
	char *title;
	char *app_id;
	int width, height;
};

struct wlr_xdg_popup {
	struct wlr_surface *parent;
};

/* Cursor */
struct wlr_cursor {
	double x, y;
	struct {
		struct wl_signal motion;
		struct wl_signal motion_absolute;
		struct wl_signal button;
		struct wl_signal axis;
		struct wl_signal frame;
		struct wl_signal touch_down;
		struct wl_signal touch_up;
		struct wl_signal touch_motion;
		struct wl_signal touch_cancel;
		struct wl_signal touch_frame;
		struct wl_signal swipe_begin;
		struct wl_signal swipe_update;
		struct wl_signal swipe_end;
		struct wl_signal pinch_begin;
		struct wl_signal pinch_update;
		struct wl_signal pinch_end;
		struct wl_signal hold_begin;
		struct wl_signal hold_end;
	} events;
};

struct wlr_xcursor_manager;

struct wlr_keyboard {
	int dummy;
};

/* Seat */
struct wlr_touch_point {
	struct wl_list link;
	void *client;
};

struct wlr_seat {
	struct {
		struct wl_list touch_points;
	} touch_state;
};

/* Input event types */
struct wlr_input_device {
	int dummy;
};

struct wlr_pointer {
	struct wlr_input_device base;
};

struct wlr_touch {
	struct wlr_input_device base;
};

struct wlr_pointer_motion_event {
	struct wlr_pointer *pointer;
	uint32_t time_msec;
	double delta_x, delta_y;
	double unaccel_dx, unaccel_dy;
};

struct wlr_pointer_motion_absolute_event {
	struct wlr_pointer *pointer;
	uint32_t time_msec;
	double x, y;
};

struct wlr_pointer_button_event {
	struct wlr_pointer *pointer;
	uint32_t time_msec;
	uint32_t button;
	uint32_t state;
};

struct wlr_pointer_axis_event {
	struct wlr_pointer *pointer;
	uint32_t time_msec;
	uint32_t orientation;
	double delta;
	int32_t delta_discrete;
	uint32_t source;
	uint32_t relative_direction;
};

struct wlr_pointer_swipe_begin_event {
	uint32_t time_msec;
	uint32_t fingers;
};
struct wlr_pointer_swipe_update_event {
	uint32_t time_msec;
	double dx, dy;
};
struct wlr_pointer_swipe_end_event {
	uint32_t time_msec;
	bool cancelled;
};
struct wlr_pointer_pinch_begin_event {
	uint32_t time_msec;
	uint32_t fingers;
};
struct wlr_pointer_pinch_update_event {
	uint32_t time_msec;
	double dx, dy, scale, rotation;
};
struct wlr_pointer_pinch_end_event {
	uint32_t time_msec;
	bool cancelled;
};
struct wlr_pointer_hold_begin_event {
	uint32_t time_msec;
	uint32_t fingers;
};
struct wlr_pointer_hold_end_event {
	uint32_t time_msec;
	bool cancelled;
};

struct wlr_touch_down_event {
	struct wlr_touch *touch;
	uint32_t time_msec;
	int32_t touch_id;
	double x, y;
};
struct wlr_touch_up_event {
	struct wlr_touch *touch;
	uint32_t time_msec;
	int32_t touch_id;
};
struct wlr_touch_motion_event {
	struct wlr_touch *touch;
	uint32_t time_msec;
	int32_t touch_id;
	double x, y;
};

/* Gesture protocol */
struct wlr_pointer_gestures_v1;
struct wlr_relative_pointer_manager_v1;

/* DRM format */
#define DRM_FORMAT_ARGB8888 0x34325241

/* Wayland protocol constants */
#define WL_POINTER_AXIS_VERTICAL_SCROLL 0
#define WL_POINTER_BUTTON_STATE_RELEASED 0
#define WL_POINTER_BUTTON_STATE_PRESSED 1

/* Cairo stubs (just enough to compile cursor.c) */
typedef struct _cairo_surface_t cairo_surface_t;
typedef struct _cairo_t cairo_t;

typedef enum {
	CAIRO_FORMAT_ARGB32 = 0,
} cairo_format_t;

typedef enum {
	CAIRO_STATUS_SUCCESS = 0,
} cairo_status_t;

static cairo_surface_t *
cairo_image_surface_create(cairo_format_t f, int w, int h)
{
	(void)f; (void)w; (void)h;
	return NULL;
}

static cairo_status_t
cairo_surface_status(cairo_surface_t *s)
{
	(void)s;
	return CAIRO_STATUS_SUCCESS + 1; /* always fail */
}

static void cairo_surface_destroy(cairo_surface_t *s) { (void)s; }
static void cairo_surface_flush(cairo_surface_t *s) { (void)s; }
static int cairo_image_surface_get_width(cairo_surface_t *s) { (void)s; return 0; }
static int cairo_image_surface_get_height(cairo_surface_t *s) { (void)s; return 0; }
static int cairo_image_surface_get_stride(cairo_surface_t *s) { (void)s; return 0; }
static unsigned char *cairo_image_surface_get_data(cairo_surface_t *s)
	{ (void)s; return NULL; }
static cairo_t *cairo_create(cairo_surface_t *s) { (void)s; return NULL; }
static void cairo_destroy(cairo_t *cr) { (void)cr; }
static void cairo_paint(cairo_t *cr) { (void)cr; }
static void cairo_stroke(cairo_t *cr) { (void)cr; }
static void cairo_set_source_rgba(cairo_t *cr, double r, double g,
	double b, double a)
	{ (void)cr; (void)r; (void)g; (void)b; (void)a; }
static void cairo_set_source_surface(cairo_t *cr, cairo_surface_t *s,
	double x, double y)
	{ (void)cr; (void)s; (void)x; (void)y; }
static void cairo_set_line_width(cairo_t *cr, double w)
	{ (void)cr; (void)w; }
static void cairo_rectangle(cairo_t *cr, double x, double y,
	double w, double h)
	{ (void)cr; (void)x; (void)y; (void)w; (void)h; }

/* --- Stub fluxland enums and types that cursor.c needs --- */

/* From config.h */
enum wm_focus_policy {
	WM_FOCUS_CLICK,
	WM_FOCUS_SLOPPY,
	WM_FOCUS_STRICT_MOUSE,
};

enum wm_snap_zone {
	WM_SNAP_ZONE_NONE = 0,
	WM_SNAP_ZONE_LEFT_HALF,
	WM_SNAP_ZONE_RIGHT_HALF,
	WM_SNAP_ZONE_TOP_HALF,
	WM_SNAP_ZONE_BOTTOM_HALF,
	WM_SNAP_ZONE_TOPLEFT,
	WM_SNAP_ZONE_TOPRIGHT,
	WM_SNAP_ZONE_BOTTOMLEFT,
	WM_SNAP_ZONE_BOTTOMRIGHT,
};

struct wm_config {
	int workspace_count;
	enum wm_focus_policy focus_policy;
	bool raise_on_focus;
	int auto_raise_delay_ms;
	bool opaque_move;
	bool opaque_resize;
	int edge_snap_threshold;
	bool enable_window_snapping;
	int snap_zone_threshold;
	bool workspace_warping;
	int double_click_interval;
	bool click_raises;
	bool focus_new_windows;
	int tab_focus_model;
};

/* From decoration.h */
enum wm_decor_region {
	WM_DECOR_REGION_NONE,
	WM_DECOR_REGION_TITLEBAR,
	WM_DECOR_REGION_LABEL,
	WM_DECOR_REGION_BUTTON,
	WM_DECOR_REGION_HANDLE,
	WM_DECOR_REGION_GRIP_LEFT,
	WM_DECOR_REGION_GRIP_RIGHT,
	WM_DECOR_REGION_BORDER_TOP,
	WM_DECOR_REGION_BORDER_BOTTOM,
	WM_DECOR_REGION_BORDER_LEFT,
	WM_DECOR_REGION_BORDER_RIGHT,
	WM_DECOR_REGION_CLIENT,
};

struct wm_decoration {
	bool shaded;
	int content_width;
	int content_height;
};

/* From mousebind.h */
enum wm_mouse_context {
	WM_MOUSE_CTX_NONE = 0,
	WM_MOUSE_CTX_DESKTOP,
	WM_MOUSE_CTX_TOOLBAR,
	WM_MOUSE_CTX_TITLEBAR,
	WM_MOUSE_CTX_TAB,
	WM_MOUSE_CTX_WINDOW,
	WM_MOUSE_CTX_WINDOW_BORDER,
	WM_MOUSE_CTX_LEFT_GRIP,
	WM_MOUSE_CTX_RIGHT_GRIP,
	WM_MOUSE_CTX_SLIT,
};

enum wm_mouse_event {
	WM_MOUSE_PRESS,
	WM_MOUSE_CLICK,
	WM_MOUSE_DOUBLE,
	WM_MOUSE_MOVE,
};

struct wm_subcmd {
	int action;
	char *argument;
	struct wm_subcmd *next;
};

struct wm_mousebind {
	enum wm_mouse_context context;
	enum wm_mouse_event event;
	uint32_t button;
	uint32_t modifiers;
	int action;
	char *argument;
	struct wm_subcmd *subcmds;
	int subcmd_count;
	int toggle_index;
	struct wl_list link;
};

struct wm_mouse_state {
	uint32_t last_button;
	uint32_t last_time_msec;
	double press_x, press_y;
	bool button_pressed;
	uint32_t pressed_button;
};

#define WM_DOUBLE_CLICK_MS  300
#define WM_CLICK_MOVE_THRESHOLD 3

/* From keybind.h */
enum wm_action {
	WM_ACTION_NOP,
	WM_ACTION_EXEC,
	WM_ACTION_CLOSE,
	WM_ACTION_MAXIMIZE,
	WM_ACTION_MINIMIZE,
	WM_ACTION_SHADE,
	WM_ACTION_STICK,
	WM_ACTION_RAISE,
	WM_ACTION_LOWER,
	WM_ACTION_FOCUS,
	WM_ACTION_START_MOVING,
	WM_ACTION_START_RESIZING,
	WM_ACTION_START_TABBING,
	WM_ACTION_NEXT_WORKSPACE,
	WM_ACTION_PREV_WORKSPACE,
	WM_ACTION_WORKSPACE,
	WM_ACTION_ROOT_MENU,
	WM_ACTION_WINDOW_MENU,
	WM_ACTION_HIDE_MENUS,
	WM_ACTION_MACRO_CMD,
	WM_ACTION_TOGGLE_CMD,
	WM_ACTION_KILL,
};

struct wm_chain_state {
	struct wl_list *current_level;
	bool in_chain;
	struct wl_event_source *timeout;
};

/* From view.h */
enum wm_view_layer {
	WM_LAYER_DESKTOP = 0,
	WM_LAYER_BELOW,
	WM_LAYER_NORMAL,
	WM_LAYER_ABOVE,
};

struct wm_tab_group {
	struct wl_list views;
	int count;
	struct wm_view *active_view;
	void *server;
};

struct wm_animation;
struct wlr_foreign_toplevel_handle_v1;

struct wm_view {
	struct wl_list link;
	void *server;
	struct wlr_xdg_toplevel *xdg_toplevel;
	struct wlr_scene_tree *scene_tree;
	uint32_t id;
	int x, y;
	struct wlr_box saved_geometry;
	bool maximized;
	bool fullscreen;
	bool maximized_vert;
	bool maximized_horiz;
	bool lhalf;
	bool rhalf;
	bool show_decoration;
	char *title;
	char *app_id;
	void *workspace;
	bool sticky;
	int layer;
	struct wm_tab_group *tab_group;
	struct wl_list tab_link;
	int focus_alpha;
	int unfocus_alpha;
	struct wm_animation *animation;
	struct wm_decoration *decoration;
	int focus_protection;
	bool ignore_size_hints;
	struct wlr_foreign_toplevel_handle_v1 *foreign_toplevel_handle;
	struct wl_listener foreign_toplevel_request_activate;
	struct wl_listener foreign_toplevel_request_maximize;
	struct wl_listener foreign_toplevel_request_minimize;
	struct wl_listener foreign_toplevel_request_fullscreen;
	struct wl_listener foreign_toplevel_request_close;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener commit;
	struct wl_listener destroy;
	struct wl_listener request_move;
	struct wl_listener request_resize;
	struct wl_listener request_maximize;
	struct wl_listener request_fullscreen;
	struct wl_listener request_minimize;
	struct wl_listener set_title;
	struct wl_listener set_app_id;
};

/* From output.h */
struct wm_output {
	struct wl_list link;
	void *server;
	struct wlr_output *wlr_output;
	void *scene_output;
	struct wlr_box usable_area;
	void *active_workspace;
	struct wl_listener frame;
	struct wl_listener request_state;
	struct wl_listener destroy_listener;
};

/* From style.h / render.h */
struct wm_color { uint8_t r, g, b, a; };
struct wm_font {
	char *family;
	int size;
	bool bold;
	bool italic;
	int shadow_x;
	int shadow_y;
	struct wm_color shadow_color;
};

enum wm_justify { WM_JUSTIFY_LEFT, WM_JUSTIFY_CENTER, WM_JUSTIFY_RIGHT };

struct wm_style {
	struct wm_font toolbar_font;
	struct wm_color toolbar_text_color;
};

/* From session_lock.h */
struct wm_session_lock {
	void *server;
	void *manager;
	void *active_lock;
	bool locked;
};

/* From ipc.h */
struct wm_ipc_server { int dummy; };
enum wm_ipc_event { WM_IPC_EVENT_DUMMY };

/* From idle.h */
struct wm_idle { int dummy; };

/* From focus_nav.h */
struct wm_focus_nav { int zone; int toolbar_index; };

/* From output_management.h */
struct wm_output_management { int dummy; };

/* From text_input.h */
struct wm_text_input_relay { int dummy; };

/* From rules.h */
struct wm_rules { int dummy; };

/* From toolbar.h */
struct wm_toolbar_tool {
	int type;
	int x;
	int width;
};

struct wm_toolbar {
	bool visible;
	int x, y;
	int width, height;
	struct wm_toolbar_tool *tools;
	int tool_count;
	void *server;
};

/* From slit.h */
struct wm_slit {
	bool visible;
	bool hidden;
	int x, y;
	int width, height;
};

/* From systray.h */
struct wm_systray;

/* From menu.h */
struct wm_menu;

/* --- Stub server type (from server.h) --- */

enum wm_cursor_mode {
	WM_CURSOR_PASSTHROUGH = 0,
	WM_CURSOR_MOVE,
	WM_CURSOR_RESIZE,
	WM_CURSOR_TABBING,
};

#define WM_XCURSOR_DEFAULT "left_ptr"
#define WM_XCURSOR_SIZE 24

struct wm_workspace;

struct wm_server {
	struct wl_display *wl_display;
	struct wl_event_loop *wl_event_loop;

	void *backend;
	void *session;
	void *renderer;
	void *allocator;
	void *compositor;

	struct wlr_scene *scene;
	struct wlr_scene_output_layout *scene_layout;

	struct wlr_scene_tree *layer_background;
	struct wlr_scene_tree *layer_bottom;
	struct wlr_scene_tree *view_tree;
	struct wlr_scene_tree *view_layer_desktop;
	struct wlr_scene_tree *view_layer_below;
	struct wlr_scene_tree *view_layer_normal;
	struct wlr_scene_tree *view_layer_above;
	struct wlr_scene_tree *xdg_popup_tree;
	struct wlr_scene_tree *layer_top;
	struct wlr_scene_tree *layer_overlay;

	void *xdg_shell;
	struct wl_listener new_xdg_toplevel;
	struct wl_listener new_xdg_popup;

	void *xdg_decoration_mgr;
	struct wl_listener new_xdg_decoration;

	void *layer_shell;
	struct wl_listener new_layer_surface;
	struct wl_list layer_surfaces;

	struct wm_style *style;

	struct wl_list outputs;
	struct wl_listener new_output;
	struct wlr_output_layout *output_layout;

	struct wlr_seat *seat;
	struct wl_listener new_input;
	struct wl_listener request_cursor;
	struct wl_listener request_set_selection;

	struct wl_listener request_start_drag;
	struct wl_listener start_drag;
	struct wlr_scene_tree *drag_icon_tree;

	struct wlr_cursor *cursor;
	struct wlr_xcursor_manager *cursor_mgr;
	struct wl_listener cursor_motion;
	struct wl_listener cursor_motion_absolute;
	struct wl_listener cursor_button;
	struct wl_listener cursor_axis;
	struct wl_listener cursor_frame;

	struct wl_listener cursor_touch_down;
	struct wl_listener cursor_touch_up;
	struct wl_listener cursor_touch_motion;
	struct wl_listener cursor_touch_cancel;
	struct wl_listener cursor_touch_frame;

	void *tablet_manager;
	struct wl_list tablets;
	struct wl_list tablet_pads;

	enum wm_cursor_mode cursor_mode;
	struct wm_view *grabbed_view;
	double grab_x, grab_y;
	struct wlr_box grab_geobox;
	uint32_t resize_edges;

	bool show_position;
	struct wlr_scene_buffer *position_overlay;

	int snap_zone;
	struct wlr_box snap_preview_box;
	struct wlr_scene_buffer *snap_preview;

	struct wlr_scene_rect *wireframe_rects[4];
	bool wireframe_active;
	int wireframe_x, wireframe_y, wireframe_w, wireframe_h;

	int focus_policy;
	struct wm_view *focused_view;
	bool focus_user_initiated;

	struct wl_event_source *auto_raise_timer;
	struct wm_view *auto_raise_view;

	struct wl_list views;

	struct wl_list workspaces;
	struct wm_workspace *current_workspace;
	int workspace_count;
	struct wlr_scene_tree *sticky_tree;

	struct wl_list keyboards;

	struct wm_config *config;
	struct wl_list keybindings;
	struct wl_list keymodes;
	char *current_keymode;
	struct wm_chain_state chain_state;

	struct wl_list mousebindings;
	struct wm_mouse_state mouse_state;

	struct wm_rules rules;
	struct wm_ipc_server ipc;
	struct wm_focus_nav focus_nav;

	void *foreign_toplevel_manager;
	struct wm_output_management output_mgmt;
	struct wm_idle idle;

	void *primary_selection_mgr;
	struct wl_listener request_set_primary_selection;
	void *data_control_mgr;
	void *pointer_constraints;
	void *active_constraint;
	struct wl_listener new_pointer_constraint;
	struct wl_listener pointer_constraint_destroy;

	struct wlr_relative_pointer_manager_v1 *relative_pointer_mgr;

	struct wlr_pointer_gestures_v1 *pointer_gestures;
	struct wl_listener cursor_swipe_begin;
	struct wl_listener cursor_swipe_update;
	struct wl_listener cursor_swipe_end;
	struct wl_listener cursor_pinch_begin;
	struct wl_listener cursor_pinch_update;
	struct wl_listener cursor_pinch_end;
	struct wl_listener cursor_hold_begin;
	struct wl_listener cursor_hold_end;

	void *fractional_scale_mgr;
	void *cursor_shape_mgr;
	struct wl_listener cursor_shape_request;
	void *viewporter;
	void *single_pixel_buffer_mgr;
	void *gamma_control_mgr;
	struct wl_listener gamma_set_gamma;
	void *presentation;

	struct wm_text_input_relay text_input_relay;

	void *virtual_keyboard_mgr;
	struct wl_listener new_virtual_keyboard;
	void *virtual_pointer_mgr;
	struct wl_listener new_virtual_pointer;
	void *kb_shortcuts_inhibit_mgr;
	struct wl_listener new_kb_shortcuts_inhibitor;
	void *active_kb_inhibitor;
	struct wl_listener kb_inhibitor_destroy;
	void *xdg_activation;
	struct wl_listener xdg_activation_request;
	void *tearing_control_mgr;
	struct wl_listener tearing_new_object;
	void *output_power_mgr;
	struct wl_listener output_power_set_mode;
	void *content_type_mgr;
	void *alpha_modifier;
	void *security_context_mgr;
	void *linux_dmabuf;
	void *xdg_output_mgr;
	void *xdg_foreign_registry;
	void *xdg_foreign_v1;
	void *xdg_foreign_v2;
	void *ext_foreign_toplevel_list;

	struct wm_session_lock session_lock;

	void *drm_lease_mgr;
	struct wl_listener drm_lease_request;
	void *transient_seat_mgr;
	struct wl_listener transient_seat_create;
	void *syncobj_mgr;

	struct wm_toolbar *toolbar;
	struct wm_slit *slit;

	/* System tray (WM_HAS_SYSTRAY) */
	struct wm_systray *systray;

	struct wm_menu *root_menu;
	struct wm_menu *window_menu;
	struct wm_menu *window_list_menu;
	struct wm_menu *workspace_menu;
	struct wm_menu *client_menu;
	struct wm_menu *custom_menu;

	struct wl_event_source *sigint_source;
	struct wl_event_source *sigterm_source;
	struct wl_event_source *sighup_source;
	struct wl_event_source *sigchld_source;

	const char *socket;
	bool ipc_no_exec;
};

/* --- Stub function tracking --- */

static int g_view_raise_count;
static int g_focus_view_count;
static int g_begin_interactive_count;
static int g_set_position_count;
static int g_set_size_count;
static int g_scene_node_destroy_count;
static int g_switch_next_count;
static int g_switch_prev_count;

/* --- Stub wlroots functions used by cursor.c --- */

static struct wlr_scene_node *
wlr_scene_node_at(struct wlr_scene_node *node, double lx, double ly,
	double *sx, double *sy)
{
	(void)node; (void)lx; (void)ly;
	if (sx) *sx = 0;
	if (sy) *sy = 0;
	return NULL;
}

static struct wlr_scene_buffer *
wlr_scene_buffer_from_node(struct wlr_scene_node *node)
{
	return (struct wlr_scene_buffer *)node;
}

static struct wlr_scene_surface *
wlr_scene_surface_try_from_buffer(struct wlr_scene_buffer *buffer)
{
	(void)buffer;
	return NULL;
}

static void
wlr_scene_node_set_position(struct wlr_scene_node *node, int x, int y)
{
	if (node) { node->x = x; node->y = y; }
	g_set_position_count++;
}

static void
wlr_scene_node_set_enabled(struct wlr_scene_node *node, bool enabled)
{
	if (node) node->enabled = enabled;
}

static void
wlr_scene_node_destroy(struct wlr_scene_node *node)
{
	(void)node;
	g_scene_node_destroy_count++;
}

static struct wlr_scene_buffer *
wlr_scene_buffer_create(struct wlr_scene_tree *parent,
	struct wlr_buffer *buf)
{
	(void)parent; (void)buf;
	return NULL;
}

static void
wlr_scene_buffer_set_buffer(struct wlr_scene_buffer *sbuf,
	struct wlr_buffer *buf)
{
	(void)sbuf; (void)buf;
}

static struct wlr_scene_rect *
wlr_scene_rect_create(struct wlr_scene_tree *parent, int w, int h,
	const float color[4])
{
	(void)parent; (void)w; (void)h; (void)color;
	return NULL;
}

static void
wlr_scene_rect_set_size(struct wlr_scene_rect *rect, int w, int h)
{
	(void)rect; (void)w; (void)h;
}

static struct wlr_output *
wlr_output_layout_output_at(struct wlr_output_layout *layout,
	double x, double y)
{
	(void)layout; (void)x; (void)y;
	return NULL;
}

static void
wlr_output_layout_get_box(struct wlr_output_layout *layout,
	struct wlr_output *output, struct wlr_box *box)
{
	(void)layout; (void)output;
	memset(box, 0, sizeof(*box));
}

static void
wlr_cursor_set_xcursor(struct wlr_cursor *cursor,
	struct wlr_xcursor_manager *mgr, const char *name)
{
	(void)cursor; (void)mgr; (void)name;
}

static void
wlr_cursor_move(struct wlr_cursor *cursor, struct wlr_input_device *dev,
	double dx, double dy)
{
	(void)cursor; (void)dev; (void)dx; (void)dy;
}

static void
wlr_cursor_warp_absolute(struct wlr_cursor *cursor,
	struct wlr_input_device *dev, double x, double y)
{
	(void)cursor; (void)dev; (void)x; (void)y;
}

static void
wlr_cursor_warp(struct wlr_cursor *cursor, struct wlr_input_device *dev,
	double x, double y)
{
	(void)dev;
	cursor->x = x;
	cursor->y = y;
}

static void
wlr_cursor_absolute_to_layout_coords(struct wlr_cursor *cursor,
	struct wlr_input_device *dev, double x, double y,
	double *lx, double *ly)
{
	(void)cursor; (void)dev;
	*lx = x;
	*ly = y;
}

static struct wlr_keyboard *
wlr_seat_get_keyboard(struct wlr_seat *seat)
{
	(void)seat;
	return NULL;
}

static uint32_t
wlr_keyboard_get_modifiers(struct wlr_keyboard *keyboard)
{
	(void)keyboard;
	return 0;
}

static uint32_t
wlr_xdg_toplevel_set_size(struct wlr_xdg_toplevel *toplevel,
	int w, int h)
{
	if (toplevel) { toplevel->width = w; toplevel->height = h; }
	g_set_size_count++;
	return 0;
}

static void
wlr_xdg_toplevel_send_close(struct wlr_xdg_toplevel *toplevel)
{
	(void)toplevel;
}

static void
wlr_xdg_surface_get_geometry(struct wlr_xdg_surface *surface,
	struct wlr_box *box)
{
	(void)surface;
	box->x = 0; box->y = 0;
	box->width = 800; box->height = 600;
}

static void
wlr_seat_pointer_notify_enter(struct wlr_seat *seat,
	struct wlr_surface *surf, double sx, double sy)
{
	(void)seat; (void)surf; (void)sx; (void)sy;
}

static void
wlr_seat_pointer_notify_motion(struct wlr_seat *seat, uint32_t time,
	double sx, double sy)
{
	(void)seat; (void)time; (void)sx; (void)sy;
}

static void
wlr_seat_pointer_notify_button(struct wlr_seat *seat, uint32_t time,
	uint32_t button, uint32_t state)
{
	(void)seat; (void)time; (void)button; (void)state;
}

static void
wlr_seat_pointer_notify_axis(struct wlr_seat *seat, uint32_t time,
	uint32_t orient, double delta, int32_t delta_d,
	uint32_t src, uint32_t rel_dir)
{
	(void)seat; (void)time; (void)orient; (void)delta;
	(void)delta_d; (void)src; (void)rel_dir;
}

static void
wlr_seat_pointer_notify_frame(struct wlr_seat *seat) { (void)seat; }

static void
wlr_seat_pointer_clear_focus(struct wlr_seat *seat) { (void)seat; }

static void
wlr_seat_touch_notify_down(struct wlr_seat *seat, struct wlr_surface *surf,
	uint32_t time, int32_t id, double sx, double sy)
{
	(void)seat; (void)surf; (void)time; (void)id; (void)sx; (void)sy;
}

static void
wlr_seat_touch_notify_up(struct wlr_seat *seat, uint32_t time,
	int32_t id)
{
	(void)seat; (void)time; (void)id;
}

static void
wlr_seat_touch_notify_motion(struct wlr_seat *seat, uint32_t time,
	int32_t id, double sx, double sy)
{
	(void)seat; (void)time; (void)id; (void)sx; (void)sy;
}

static void
wlr_seat_touch_notify_cancel(struct wlr_seat *seat, void *client)
{
	(void)seat; (void)client;
}

static void
wlr_seat_touch_notify_frame(struct wlr_seat *seat) { (void)seat; }

/* Pointer gesture stubs */
static void
wlr_pointer_gestures_v1_send_swipe_begin(void *g, void *s,
	uint32_t t, uint32_t f)
{ (void)g; (void)s; (void)t; (void)f; }
static void
wlr_pointer_gestures_v1_send_swipe_update(void *g, void *s,
	uint32_t t, double dx, double dy)
{ (void)g; (void)s; (void)t; (void)dx; (void)dy; }
static void
wlr_pointer_gestures_v1_send_swipe_end(void *g, void *s,
	uint32_t t, bool c)
{ (void)g; (void)s; (void)t; (void)c; }
static void
wlr_pointer_gestures_v1_send_pinch_begin(void *g, void *s,
	uint32_t t, uint32_t f)
{ (void)g; (void)s; (void)t; (void)f; }
static void
wlr_pointer_gestures_v1_send_pinch_update(void *g, void *s,
	uint32_t t, double dx, double dy, double sc, double rot)
{ (void)g; (void)s; (void)t; (void)dx; (void)dy; (void)sc; (void)rot; }
static void
wlr_pointer_gestures_v1_send_pinch_end(void *g, void *s,
	uint32_t t, bool c)
{ (void)g; (void)s; (void)t; (void)c; }
static void
wlr_pointer_gestures_v1_send_hold_begin(void *g, void *s,
	uint32_t t, uint32_t f)
{ (void)g; (void)s; (void)t; (void)f; }
static void
wlr_pointer_gestures_v1_send_hold_end(void *g, void *s,
	uint32_t t, bool c)
{ (void)g; (void)s; (void)t; (void)c; }

/* Relative pointer stub */
static void
wlr_relative_pointer_manager_v1_send_relative_motion(void *mgr,
	void *seat, uint64_t time, double dx, double dy,
	double udx, double udy)
{
	(void)mgr; (void)seat; (void)time;
	(void)dx; (void)dy; (void)udx; (void)udy;
}

/* --- Stub fluxland functions used by cursor.c --- */

static void wm_view_raise(struct wm_view *view)
	{ (void)view; g_view_raise_count++; }
static void wm_view_lower(struct wm_view *view)
	{ (void)view; }
static void wm_focus_view(struct wm_view *view, struct wlr_surface *surface)
	{ (void)view; (void)surface; g_focus_view_count++; }
static void wm_unfocus_current(struct wm_server *server)
	{ (void)server; }
static void wm_view_begin_interactive(struct wm_view *view,
	enum wm_cursor_mode mode, uint32_t edges)
	{ (void)view; (void)mode; (void)edges; g_begin_interactive_count++; }
static void wm_view_get_geometry(struct wm_view *view, struct wlr_box *box)
	{ (void)view; box->x = 0; box->y = 0;
	  box->width = 800; box->height = 600; }
static void wm_view_set_sticky(struct wm_view *view, bool sticky)
	{ (void)view; (void)sticky; }
static void wm_workspace_switch_next(struct wm_server *server)
	{ (void)server; g_switch_next_count++; }
static void wm_workspace_switch_prev(struct wm_server *server)
	{ (void)server; g_switch_prev_count++; }
static void wm_workspace_switch(struct wm_server *server, int idx)
	{ (void)server; (void)idx; }
static enum wm_decor_region wm_decoration_region_at(
	struct wm_decoration *d, double x, double y)
	{ (void)d; (void)x; (void)y; return WM_DECOR_REGION_NONE; }
static int wm_decoration_tab_at(struct wm_decoration *d, double x, double y)
	{ (void)d; (void)x; (void)y; return -1; }
static void wm_decoration_set_shaded(struct wm_decoration *d, bool s,
	struct wm_style *st) { (void)d; (void)s; (void)st; }
static bool wm_session_is_locked(struct wm_server *server)
	{ return server->session_lock.locked; }
static void wm_idle_notify_activity(struct wm_server *server)
	{ (void)server; }
static void wm_toolbar_notify_pointer_motion(struct wm_toolbar *tb,
	double x, double y) { (void)tb; (void)x; (void)y; }
static bool wm_toolbar_handle_scroll(struct wm_toolbar *tb, double x,
	double y, double delta) { (void)tb; (void)x; (void)y; (void)delta;
	return false; }
static bool wm_menu_handle_motion(struct wm_server *s, double x, double y)
	{ (void)s; (void)x; (void)y; return false; }
static bool wm_menu_handle_button(struct wm_server *s, double x, double y,
	uint32_t btn, bool p) { (void)s; (void)x; (void)y; (void)btn;
	(void)p; return false; }
static void wm_menu_show_root(struct wm_server *s, int x, int y)
	{ (void)s; (void)x; (void)y; }
static void wm_menu_show_window(struct wm_server *s, int x, int y)
	{ (void)s; (void)x; (void)y; }
static void wm_menu_hide_all(struct wm_server *s) { (void)s; }
static struct wm_mousebind *mousebind_find(struct wl_list *bindings,
	enum wm_mouse_context ctx, enum wm_mouse_event event,
	uint32_t button, uint32_t mods)
	{ (void)bindings; (void)ctx; (void)event; (void)button;
	  (void)mods; return NULL; }
static void wm_snap_edges(struct wm_server *s, struct wm_view *v,
	int *sx, int *sy) { (void)s; (void)v; (void)sx; (void)sy; }
static void wm_focus_update_for_cursor(struct wm_server *s,
	double cx, double cy) { (void)s; (void)cx; (void)cy; }
static void wm_tab_group_activate(struct wm_tab_group *g,
	struct wm_view *v) { (void)g; (void)v; }
static cairo_surface_t *wm_render_text(const char *text,
	const struct wm_font *f, const struct wm_color *c,
	int max_w, int *ow, int *oh, enum wm_justify j, float sc)
{
	(void)text; (void)f; (void)c; (void)max_w; (void)j; (void)sc;
	if (ow)
		*ow = 0;
	if (oh)
		*oh = 0;
	return NULL;
}
static bool safe_atoi(const char *s, int *out)
	{ if (!s || !*s) return false; char *end;
	  long val = strtol(s, &end, 10);
	  if (end == s || *end != '\0') return false;
	  *out = (int)val; return true; }

static int wm_systray_get_width(struct wm_systray *systray)
	{ (void)systray; return 0; }
static bool wm_systray_handle_click(struct wm_systray *systray,
	double x, double y, uint32_t button)
	{ (void)systray; (void)x; (void)y; (void)button; return false; }

#ifdef closefrom
#undef closefrom
#endif
static void closefrom(int fd) { (void)fd; }

/* --- Include cursor.c directly --- */

#include "cursor.c"

/* --- Test helpers --- */

static struct wm_server test_server;
static struct wlr_cursor test_cursor;
static struct wlr_seat test_seat;
static struct wm_config test_config;

static void
reset_globals(void)
{
	g_view_raise_count = 0;
	g_focus_view_count = 0;
	g_begin_interactive_count = 0;
	g_set_position_count = 0;
	g_set_size_count = 0;
	g_scene_node_destroy_count = 0;
	g_switch_next_count = 0;
	g_switch_prev_count = 0;
}

static void
setup(void)
{
	memset(&test_server, 0, sizeof(test_server));
	memset(&test_cursor, 0, sizeof(test_cursor));
	memset(&test_seat, 0, sizeof(test_seat));
	memset(&test_config, 0, sizeof(test_config));

	test_server.cursor = &test_cursor;
	test_server.seat = &test_seat;
	test_server.config = &test_config;

	wl_list_init(&test_server.outputs);
	wl_list_init(&test_server.views);
	wl_list_init(&test_server.workspaces);
	wl_list_init(&test_server.keybindings);
	wl_list_init(&test_server.keymodes);
	wl_list_init(&test_server.mousebindings);
	wl_list_init(&test_server.keyboards);
	wl_list_init(&test_server.layer_surfaces);
	wl_list_init(&test_server.tablets);
	wl_list_init(&test_server.tablet_pads);
	wl_list_init(&test_seat.touch_state.touch_points);

	test_config.snap_zone_threshold = 10;
	test_config.enable_window_snapping = true;
	test_config.opaque_move = true;
	test_config.opaque_resize = true;

	reset_globals();
}

/* ===== Tests ===== */

/*
 * Test 1: region_to_context maps titlebar/label/button regions
 * to WM_MOUSE_CTX_TITLEBAR.
 */
static void
test_region_to_context_titlebar(void)
{
	assert(region_to_context(WM_DECOR_REGION_TITLEBAR) ==
		WM_MOUSE_CTX_TITLEBAR);
	assert(region_to_context(WM_DECOR_REGION_LABEL) ==
		WM_MOUSE_CTX_TITLEBAR);
	assert(region_to_context(WM_DECOR_REGION_BUTTON) ==
		WM_MOUSE_CTX_TITLEBAR);
	printf("  PASS: region_to_context_titlebar\n");
}

/*
 * Test 2: region_to_context maps handle and all border regions
 * to WM_MOUSE_CTX_WINDOW_BORDER.
 */
static void
test_region_to_context_borders(void)
{
	assert(region_to_context(WM_DECOR_REGION_HANDLE) ==
		WM_MOUSE_CTX_WINDOW_BORDER);
	assert(region_to_context(WM_DECOR_REGION_BORDER_TOP) ==
		WM_MOUSE_CTX_WINDOW_BORDER);
	assert(region_to_context(WM_DECOR_REGION_BORDER_BOTTOM) ==
		WM_MOUSE_CTX_WINDOW_BORDER);
	assert(region_to_context(WM_DECOR_REGION_BORDER_LEFT) ==
		WM_MOUSE_CTX_WINDOW_BORDER);
	assert(region_to_context(WM_DECOR_REGION_BORDER_RIGHT) ==
		WM_MOUSE_CTX_WINDOW_BORDER);
	printf("  PASS: region_to_context_borders\n");
}

/*
 * Test 3: region_to_context maps grip regions to LEFT_GRIP/RIGHT_GRIP.
 */
static void
test_region_to_context_grips(void)
{
	assert(region_to_context(WM_DECOR_REGION_GRIP_LEFT) ==
		WM_MOUSE_CTX_LEFT_GRIP);
	assert(region_to_context(WM_DECOR_REGION_GRIP_RIGHT) ==
		WM_MOUSE_CTX_RIGHT_GRIP);
	printf("  PASS: region_to_context_grips\n");
}

/*
 * Test 4: region_to_context maps CLIENT to WINDOW and NONE to NONE.
 */
static void
test_region_to_context_client_and_none(void)
{
	assert(region_to_context(WM_DECOR_REGION_CLIENT) ==
		WM_MOUSE_CTX_WINDOW);
	assert(region_to_context(WM_DECOR_REGION_NONE) ==
		WM_MOUSE_CTX_NONE);
	printf("  PASS: region_to_context_client_and_none\n");
}

/*
 * Test 5: snap_zone_detect returns NONE when cursor is in center
 * of output, far from any edge.
 */
static void
test_snap_zone_detect_center(void)
{
	setup();
	struct wlr_box usable = { .x = 0, .y = 0,
		.width = 1920, .height = 1080 };
	test_cursor.x = 960;
	test_cursor.y = 540;

	enum wm_snap_zone zone = snap_zone_detect(&test_server, &usable);
	assert(zone == WM_SNAP_ZONE_NONE);
	printf("  PASS: snap_zone_detect_center\n");
}

/*
 * Test 6: snap_zone_detect detects all four edge snap zones.
 */
static void
test_snap_zone_detect_edges(void)
{
	setup();
	struct wlr_box usable = { .x = 0, .y = 0,
		.width = 1920, .height = 1080 };

	/* Left edge */
	test_cursor.x = 5;
	test_cursor.y = 540;
	assert(snap_zone_detect(&test_server, &usable) ==
		WM_SNAP_ZONE_LEFT_HALF);

	/* Right edge */
	test_cursor.x = 1915;
	test_cursor.y = 540;
	assert(snap_zone_detect(&test_server, &usable) ==
		WM_SNAP_ZONE_RIGHT_HALF);

	/* Top edge */
	test_cursor.x = 960;
	test_cursor.y = 3;
	assert(snap_zone_detect(&test_server, &usable) ==
		WM_SNAP_ZONE_TOP_HALF);

	/* Bottom edge */
	test_cursor.x = 960;
	test_cursor.y = 1075;
	assert(snap_zone_detect(&test_server, &usable) ==
		WM_SNAP_ZONE_BOTTOM_HALF);

	printf("  PASS: snap_zone_detect_edges\n");
}

/*
 * Test 7: snap_zone_detect detects all four corner snap zones.
 * Corners take priority over edges when cursor is in both zones.
 */
static void
test_snap_zone_detect_corners(void)
{
	setup();
	struct wlr_box usable = { .x = 0, .y = 0,
		.width = 1920, .height = 1080 };

	/* Top-left corner */
	test_cursor.x = 3;
	test_cursor.y = 3;
	assert(snap_zone_detect(&test_server, &usable) ==
		WM_SNAP_ZONE_TOPLEFT);

	/* Top-right corner */
	test_cursor.x = 1915;
	test_cursor.y = 3;
	assert(snap_zone_detect(&test_server, &usable) ==
		WM_SNAP_ZONE_TOPRIGHT);

	/* Bottom-left corner */
	test_cursor.x = 3;
	test_cursor.y = 1075;
	assert(snap_zone_detect(&test_server, &usable) ==
		WM_SNAP_ZONE_BOTTOMLEFT);

	/* Bottom-right corner */
	test_cursor.x = 1915;
	test_cursor.y = 1075;
	assert(snap_zone_detect(&test_server, &usable) ==
		WM_SNAP_ZONE_BOTTOMRIGHT);

	printf("  PASS: snap_zone_detect_corners\n");
}

/*
 * Test 8: snap_zone_detect respects configurable threshold.
 */
static void
test_snap_zone_detect_threshold(void)
{
	setup();
	struct wlr_box usable = { .x = 0, .y = 0,
		.width = 1920, .height = 1080 };

	/* Default threshold is 10. Cursor at x=5 should trigger. */
	test_cursor.x = 5;
	test_cursor.y = 540;
	assert(snap_zone_detect(&test_server, &usable) ==
		WM_SNAP_ZONE_LEFT_HALF);

	/* Set threshold to 3. Cursor at x=5 should NOT trigger. */
	test_config.snap_zone_threshold = 3;
	assert(snap_zone_detect(&test_server, &usable) ==
		WM_SNAP_ZONE_NONE);

	/* Cursor at x=1 should still trigger with threshold=3 */
	test_cursor.x = 1;
	assert(snap_zone_detect(&test_server, &usable) ==
		WM_SNAP_ZONE_LEFT_HALF);

	printf("  PASS: snap_zone_detect_threshold\n");
}

/*
 * Test 9: snap_zone_detect works when usable area has non-zero offset
 * (e.g., toolbar/panel reserves top space).
 */
static void
test_snap_zone_detect_offset_usable(void)
{
	setup();
	struct wlr_box usable = { .x = 100, .y = 50,
		.width = 1720, .height = 980 };

	/* Near the left edge of usable area */
	test_cursor.x = 103;
	test_cursor.y = 540;
	assert(snap_zone_detect(&test_server, &usable) ==
		WM_SNAP_ZONE_LEFT_HALF);

	/* Well inside, no snap */
	test_cursor.x = 200;
	test_cursor.y = 540;
	assert(snap_zone_detect(&test_server, &usable) ==
		WM_SNAP_ZONE_NONE);

	printf("  PASS: snap_zone_detect_offset_usable\n");
}

/*
 * Test 10: snap_zone_detect boundary: at exactly threshold distance
 * does NOT trigger (strict less-than comparison).
 */
static void
test_snap_zone_detect_boundary(void)
{
	setup();
	struct wlr_box usable = { .x = 0, .y = 0,
		.width = 1920, .height = 1080 };
	test_config.snap_zone_threshold = 10;

	/* threshold-1 from left: triggers */
	test_cursor.x = 9;
	test_cursor.y = 540;
	assert(snap_zone_detect(&test_server, &usable) ==
		WM_SNAP_ZONE_LEFT_HALF);

	/* Exactly at threshold from left: does NOT trigger */
	test_cursor.x = 10;
	test_cursor.y = 540;
	assert(snap_zone_detect(&test_server, &usable) ==
		WM_SNAP_ZONE_NONE);

	/* Exactly at threshold from right: does NOT trigger */
	test_cursor.x = 1910;
	test_cursor.y = 540;
	assert(snap_zone_detect(&test_server, &usable) ==
		WM_SNAP_ZONE_NONE);

	/* One pixel closer to right edge: triggers */
	test_cursor.x = 1911;
	test_cursor.y = 540;
	assert(snap_zone_detect(&test_server, &usable) ==
		WM_SNAP_ZONE_RIGHT_HALF);

	printf("  PASS: snap_zone_detect_boundary\n");
}

/*
 * Test 11: snap_zone_detect uses default threshold=10 when config is NULL.
 */
static void
test_snap_zone_detect_null_config(void)
{
	setup();
	test_server.config = NULL;

	struct wlr_box usable = { .x = 0, .y = 0,
		.width = 1920, .height = 1080 };

	/* cursor at x=5 should trigger with default threshold=10 */
	test_cursor.x = 5;
	test_cursor.y = 540;
	assert(snap_zone_detect(&test_server, &usable) ==
		WM_SNAP_ZONE_LEFT_HALF);

	/* cursor at x=15 should NOT trigger */
	test_cursor.x = 15;
	test_cursor.y = 540;
	assert(snap_zone_detect(&test_server, &usable) ==
		WM_SNAP_ZONE_NONE);

	printf("  PASS: snap_zone_detect_null_config\n");
}

/*
 * Test 12: snap_zone_geometry computes correct left-half geometry.
 */
static void
test_snap_zone_geometry_left_half(void)
{
	struct wlr_box usable = { .x = 0, .y = 0,
		.width = 1920, .height = 1080 };

	struct wlr_box box = snap_zone_geometry(WM_SNAP_ZONE_LEFT_HALF,
		&usable);

	assert(box.x == 0);
	assert(box.y == 0);
	assert(box.width == 960);
	assert(box.height == 1080);
	printf("  PASS: snap_zone_geometry_left_half\n");
}

/*
 * Test 13: snap_zone_geometry computes correct right-half geometry.
 */
static void
test_snap_zone_geometry_right_half(void)
{
	struct wlr_box usable = { .x = 0, .y = 0,
		.width = 1920, .height = 1080 };

	struct wlr_box box = snap_zone_geometry(WM_SNAP_ZONE_RIGHT_HALF,
		&usable);

	assert(box.x == 960);
	assert(box.y == 0);
	assert(box.width == 960);
	assert(box.height == 1080);
	printf("  PASS: snap_zone_geometry_right_half\n");
}

/*
 * Test 14: snap_zone_geometry computes all four quarter zones.
 */
static void
test_snap_zone_geometry_quarters(void)
{
	struct wlr_box usable = { .x = 0, .y = 0,
		.width = 1920, .height = 1080 };

	struct wlr_box tl = snap_zone_geometry(WM_SNAP_ZONE_TOPLEFT,
		&usable);
	assert(tl.x == 0 && tl.y == 0);
	assert(tl.width == 960 && tl.height == 540);

	struct wlr_box tr = snap_zone_geometry(WM_SNAP_ZONE_TOPRIGHT,
		&usable);
	assert(tr.x == 960 && tr.y == 0);
	assert(tr.width == 960 && tr.height == 540);

	struct wlr_box bl = snap_zone_geometry(WM_SNAP_ZONE_BOTTOMLEFT,
		&usable);
	assert(bl.x == 0 && bl.y == 540);
	assert(bl.width == 960 && bl.height == 540);

	struct wlr_box br = snap_zone_geometry(WM_SNAP_ZONE_BOTTOMRIGHT,
		&usable);
	assert(br.x == 960 && br.y == 540);
	assert(br.width == 960 && br.height == 540);

	printf("  PASS: snap_zone_geometry_quarters\n");
}

/*
 * Test 15: snap_zone_geometry with non-zero offset usable area.
 */
static void
test_snap_zone_geometry_offset(void)
{
	struct wlr_box usable = { .x = 100, .y = 30,
		.width = 1720, .height = 1020 };

	struct wlr_box left = snap_zone_geometry(WM_SNAP_ZONE_LEFT_HALF,
		&usable);
	assert(left.x == 100);
	assert(left.y == 30);
	assert(left.width == 860);
	assert(left.height == 1020);

	struct wlr_box right = snap_zone_geometry(WM_SNAP_ZONE_RIGHT_HALF,
		&usable);
	assert(right.x == 960);
	assert(right.y == 30);
	assert(right.width == 860);
	assert(right.height == 1020);

	printf("  PASS: snap_zone_geometry_offset\n");
}

/*
 * Test 16: snap_zone_geometry with odd dimensions ensures full
 * coverage (left + right widths = total width).
 */
static void
test_snap_zone_geometry_odd_dimensions(void)
{
	struct wlr_box usable = { .x = 0, .y = 0,
		.width = 1921, .height = 1081 };

	struct wlr_box left = snap_zone_geometry(WM_SNAP_ZONE_LEFT_HALF,
		&usable);
	struct wlr_box right = snap_zone_geometry(WM_SNAP_ZONE_RIGHT_HALF,
		&usable);

	assert(left.width == 960);
	assert(right.width == 961);
	assert(left.width + right.width == usable.width);

	struct wlr_box top = snap_zone_geometry(WM_SNAP_ZONE_TOP_HALF,
		&usable);
	struct wlr_box bottom = snap_zone_geometry(WM_SNAP_ZONE_BOTTOM_HALF,
		&usable);

	assert(top.height == 540);
	assert(bottom.height == 541);
	assert(top.height + bottom.height == usable.height);

	printf("  PASS: snap_zone_geometry_odd_dimensions\n");
}

/*
 * Test 17: snap_zone_geometry returns zeroed box for NONE zone.
 */
static void
test_snap_zone_geometry_none(void)
{
	struct wlr_box usable = { .x = 0, .y = 0,
		.width = 1920, .height = 1080 };

	struct wlr_box box = snap_zone_geometry(WM_SNAP_ZONE_NONE, &usable);
	assert(box.x == 0 && box.y == 0);
	assert(box.width == 0 && box.height == 0);
	printf("  PASS: snap_zone_geometry_none\n");
}

/*
 * Test 18: snap_zone_geometry computes top and bottom halves.
 */
static void
test_snap_zone_geometry_top_bottom(void)
{
	struct wlr_box usable = { .x = 0, .y = 0,
		.width = 1920, .height = 1080 };

	struct wlr_box top = snap_zone_geometry(WM_SNAP_ZONE_TOP_HALF,
		&usable);
	assert(top.x == 0 && top.y == 0);
	assert(top.width == 1920 && top.height == 540);

	struct wlr_box bottom = snap_zone_geometry(WM_SNAP_ZONE_BOTTOM_HALF,
		&usable);
	assert(bottom.x == 0 && bottom.y == 540);
	assert(bottom.width == 1920 && bottom.height == 540);

	printf("  PASS: snap_zone_geometry_top_bottom\n");
}

int
main(void)
{
	printf("test_cursor_snap:\n");

	/* region_to_context mapping tests */
	test_region_to_context_titlebar();
	test_region_to_context_borders();
	test_region_to_context_grips();
	test_region_to_context_client_and_none();

	/* snap_zone_detect tests */
	test_snap_zone_detect_center();
	test_snap_zone_detect_edges();
	test_snap_zone_detect_corners();
	test_snap_zone_detect_threshold();
	test_snap_zone_detect_offset_usable();
	test_snap_zone_detect_boundary();
	test_snap_zone_detect_null_config();

	/* snap_zone_geometry tests */
	test_snap_zone_geometry_left_half();
	test_snap_zone_geometry_right_half();
	test_snap_zone_geometry_quarters();
	test_snap_zone_geometry_offset();
	test_snap_zone_geometry_odd_dimensions();
	test_snap_zone_geometry_none();
	test_snap_zone_geometry_top_bottom();

	printf("All cursor_snap tests passed.\n");
	return 0;
}
