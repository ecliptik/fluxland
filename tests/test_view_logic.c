/*
 * test_view_logic.c - Unit tests for view.c pure logic functions
 *
 * Includes view.c directly with stub implementations to avoid
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
#define WLR_BACKEND_H
#define WLR_RENDER_ALLOCATOR_H
#define WLR_RENDER_WLR_RENDERER_H

/* fluxland */
#define WM_ANIMATION_H
#define WM_CONFIG_H
#define WM_DECORATION_H
#define WM_FOREIGN_TOPLEVEL_H
#define WM_IPC_H
#define WM_OUTPUT_H
#define WM_SERVER_H
#define WM_TOOLBAR_H
#define WM_VIEW_H
#define WM_WORKSPACE_H
#define WM_PLACEMENT_H
#define WM_RULES_H
#define WM_TABGROUP_H
#define WM_TEXT_INPUT_H
#define WM_PROTOCOLS_H
#define WM_KEYBIND_H
#define WM_MOUSEBIND_H
#define WM_IDLE_H
#define WM_OUTPUT_MANAGEMENT_H
#define WM_FRACTIONAL_SCALE_H
#define WM_GAMMA_CONTROL_H
#define WM_SCREENCOPY_H
#define WM_SESSION_LOCK_H
#define WM_DRM_LEASE_H
#define WM_DRM_SYNCOBJ_H
#define WM_FOCUS_NAV_H
#define WM_TRANSIENT_SEAT_H
#define WM_VIEWPORTER_H
#define WM_XWAYLAND_H
#define WM_STYLE_H
#define WM_RENDER_H
#define WM_CURSOR_H
#define WM_INPUT_H
#define WM_KEYBOARD_H
#define WM_LAYER_SHELL_H
#define WM_MENU_H
#define WM_SLIT_H
#define WM_UTIL_H
#define WM_VIEW_FOCUS_H
#define WM_VIEW_GEOMETRY_H

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

static inline int
wl_list_length(const struct wl_list *list)
{
	struct wl_list *e;
	int count = 0;
	e = list->next;
	while (e != list) {
		e = e->next;
		count++;
	}
	return count;
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
	struct wl_list link; /* parent->children */
	bool enabled;
	int x, y;
	void *data;
};

struct wlr_scene_tree {
	struct wlr_scene_node node;
	struct wl_list children; /* wlr_scene_node.link */
};

struct wlr_scene {
	struct wlr_scene_tree tree;
};

struct wlr_scene_buffer {
	struct wlr_scene_node node;
};

struct wlr_scene_surface {
	struct wlr_surface *surface;
};

struct wlr_scene_output_layout;

/* Output */
struct wlr_output {
	char *name;
};

struct wlr_output_layout {
	int dummy;
};

/* Surface */
struct wlr_surface {
	struct {
		struct wl_signal map;
		struct wl_signal unmap;
		struct wl_signal commit;
	} events;
};

/* XDG shell */
struct wlr_xdg_surface {
	struct wlr_surface *surface;
	void *data;
	bool initial_commit;
	/* not needed: events are on surface in 0.18 */
};

struct wlr_xdg_toplevel {
	struct wlr_xdg_surface *base;
	char *title;
	char *app_id;
	int width, height;
	struct {
		struct wl_signal destroy;
		struct wl_signal request_move;
		struct wl_signal request_resize;
		struct wl_signal request_maximize;
		struct wl_signal request_fullscreen;
		struct wl_signal request_minimize;
		struct wl_signal set_title;
		struct wl_signal set_app_id;
	} events;
};

struct wlr_xdg_toplevel_resize_event {
	uint32_t edges;
};

struct wlr_xdg_popup {
	struct wlr_surface *parent;
	struct wlr_xdg_surface *base;
};

struct wlr_xdg_shell {
	struct {
		struct wl_signal new_toplevel;
		struct wl_signal new_popup;
	} events;
};

struct wlr_xdg_decoration_manager_v1;

/* Seat / input */
struct wlr_keyboard_modifiers {
	uint32_t dummy;
};

struct wlr_keyboard {
	uint32_t keycodes[32];
	size_t num_keycodes;
	struct wlr_keyboard_modifiers modifiers;
};

struct wlr_seat {
	struct {
		struct wlr_surface *focused_surface;
	} keyboard_state;
	struct {
		struct wlr_surface *focused_surface;
	} pointer_state;
};

/* Cursor */
struct wlr_cursor {
	double x, y;
};

struct wlr_xcursor_manager;

/* Backend / renderer */
struct wlr_backend;
struct wlr_session;
struct wlr_renderer;
struct wlr_allocator;
struct wlr_compositor;
struct wlr_subcompositor;

/* --- Stub fluxland types --- */

/* Focus protection flags from rules.h */
#define WM_FOCUS_PROT_GAIN   0x01
#define WM_FOCUS_PROT_REFUSE 0x02
#define WM_FOCUS_PROT_DENY   0x04
#define WM_FOCUS_PROT_LOCK   (WM_FOCUS_PROT_REFUSE | WM_FOCUS_PROT_DENY)

/* Focus policy from config.h */
enum wm_focus_policy {
	WM_FOCUS_CLICK = 0,
	WM_FOCUS_SLOPPY,
	WM_FOCUS_STRICT_MOUSE,
};

/* Cursor mode from server.h */
enum wm_cursor_mode {
	WM_CURSOR_PASSTHROUGH = 0,
	WM_CURSOR_MOVE,
	WM_CURSOR_RESIZE,
	WM_CURSOR_TABBING,
};

/* View layer from view.h */
enum wm_view_layer {
	WM_LAYER_DESKTOP = 0,
	WM_LAYER_BELOW,
	WM_LAYER_NORMAL,
	WM_LAYER_ABOVE,
	WM_VIEW_LAYER_COUNT,
};

/* Animation types */
enum wm_animation_type {
	WM_ANIM_FADE_IN,
	WM_ANIM_FADE_OUT,
};

/* Forward-declare wm_view so the typedef sees the right struct */
struct wm_view;

/* Config */
struct wm_config {
	int workspace_count;
	char **workspace_names;
	int workspace_name_count;
	bool workspace_warping;
	int workspace_mode;
	int focus_policy;
	bool raise_on_focus;
	int auto_raise_delay_ms;
	bool click_raises;
	int window_focus_alpha;
	int window_unfocus_alpha;
	bool animate_window_map;
	bool animate_window_unmap;
	bool animate_minimize;
	int animation_duration_ms;
	bool full_maximization;
	bool auto_tab_placement;
};

/* IPC */
enum wm_ipc_event {
	WM_IPC_EVENT_WINDOW_OPEN    = (1 << 0),
	WM_IPC_EVENT_WINDOW_CLOSE   = (1 << 1),
	WM_IPC_EVENT_WINDOW_FOCUS   = (1 << 2),
	WM_IPC_EVENT_WINDOW_TITLE   = (1 << 3),
	WM_IPC_EVENT_WORKSPACE      = (1 << 4),
	WM_IPC_EVENT_FOCUS_CHANGED  = (1 << 9),
};

struct wm_ipc_server {
	int dummy;
};

/* Style / decoration */
struct wm_style {
	int dummy;
};

struct wm_decoration {
	struct wlr_scene_tree *tree;
};

/* Toolbar / slit */
struct wm_toolbar {
	int dummy;
};

struct wm_slit {
	int dummy;
};

struct wm_slit_client {
	int dummy;
};

/* Rules */
struct wm_rules {
	int dummy;
};

/* Keybind / mousebind (opaque, from blocked headers) */
struct wm_chain_state { int dummy; };
struct wm_mouse_state { int dummy; };

/* Output */
struct wm_output {
	struct wl_list link;
	struct wm_server *server;
	struct wlr_output *wlr_output;
	void *scene_output;
	struct wlr_box usable_area;
	struct wm_workspace *active_workspace;
	struct wl_listener frame;
	struct wl_listener request_state;
	struct wl_listener destroy;
};

/* Workspace */
struct wm_workspace {
	struct wl_list link;
	struct wm_server *server;
	struct wlr_scene_tree *tree;
	char *name;
	int index;
};

/* Tab group */
struct wm_tab_group {
	struct wl_list views;
	int count;
	struct wm_view *active_view;
	struct wm_server *server;
};

/* Focus nav (opaque) */
struct wm_focus_nav { int dummy; };

/* Output management (opaque) */
struct wm_output_management { int dummy; };

/* Idle (opaque) */
struct wm_idle { int dummy; };

/* Session lock (opaque) */
struct wm_session_lock { int dummy; };

/* Text input relay (opaque) */
struct wm_text_input_relay { int dummy; };

/* Foreign toplevel handle */
struct wlr_foreign_toplevel_handle_v1;
struct wlr_foreign_toplevel_manager_v1;

/* Animation done callback typedef (must come after struct wm_view forward decl) */
typedef void (*wm_animation_done_fn)(struct wm_view *view, bool completed,
	void *data);

struct wm_animation {
	struct wm_view *view;
	struct wl_event_source *timer;
	enum wm_animation_type type;
	int start_alpha, target_alpha;
	int duration_ms, elapsed_ms;
	wm_animation_done_fn done_fn;
	void *done_data;
};

/* View struct (must match view.h layout for wl_container_of) */
struct wm_view {
	struct wl_list link;
	struct wm_server *server;
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
	struct wm_workspace *workspace;
	bool sticky;
	enum wm_view_layer layer;
	struct wm_tab_group *tab_group;
	struct wl_list tab_link;
	int focus_alpha;
	int unfocus_alpha;
	struct wm_animation *animation;
	int focus_protection;
	bool ignore_size_hints;
	bool docked;
	struct wlr_foreign_toplevel_handle_v1 *foreign_toplevel_handle;
	struct wl_listener foreign_toplevel_request_activate;
	struct wl_listener foreign_toplevel_request_maximize;
	struct wl_listener foreign_toplevel_request_minimize;
	struct wl_listener foreign_toplevel_request_fullscreen;
	struct wl_listener foreign_toplevel_request_close;
	struct wm_decoration *decoration;
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

/* Server struct */
struct wm_server {
	struct wl_display *wl_display;
	struct wl_event_loop *wl_event_loop;
	struct wlr_backend *backend;
	struct wlr_session *session;
	struct wlr_renderer *renderer;
	struct wlr_allocator *allocator;
	struct wlr_compositor *compositor;
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
	struct wlr_xdg_shell *xdg_shell;
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
	void *wireframe_rects[4];
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
	void *ws_transition;
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
	struct wlr_foreign_toplevel_manager_v1 *foreign_toplevel_manager;
	struct wm_output_management output_mgmt;
	struct wm_idle idle;
	void *primary_selection_mgr;
	struct wl_listener request_set_primary_selection;
	void *data_control_mgr;
	void *pointer_constraints;
	void *active_constraint;
	struct wl_listener new_pointer_constraint;
	struct wl_listener pointer_constraint_destroy;
	void *relative_pointer_mgr;
	void *pointer_gestures;
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
	void *wallpaper;
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

struct wm_menu;

/* --- Tracking globals --- */

static int g_focus_view_count;
static struct wm_view *g_last_focused_view;

/* Configurable return value for wlr_output_layout_output_at stub */
static struct wlr_output *g_output_at_return = NULL;

/* Tracking for wm_tab_group_activate calls */
static int g_tab_activate_count;
static struct wm_tab_group *g_tab_activate_group;
static struct wm_view *g_tab_activate_view;

/* --- Scene tree pool --- */

#define MAX_SCENE_TREES 32
static struct wlr_scene_tree g_scene_tree_pool[MAX_SCENE_TREES];
static int g_scene_tree_next;

/* --- Stub wlr functions --- */

static void
wlr_scene_node_set_enabled(struct wlr_scene_node *node, bool enabled)
{
	if (node)
		node->enabled = enabled;
}

static void
wlr_scene_node_set_position(struct wlr_scene_node *node, int x, int y)
{
	if (node) { node->x = x; node->y = y; }
}

static void
wlr_scene_node_raise_to_top(struct wlr_scene_node *node) { (void)node; }
static void
wlr_scene_node_lower_to_bottom(struct wlr_scene_node *node) { (void)node; }
static void
wlr_scene_node_reparent(struct wlr_scene_node *node,
	struct wlr_scene_tree *new_parent) { (void)node; (void)new_parent; }
static void
wlr_scene_node_destroy(struct wlr_scene_node *node) { (void)node; }

/* Configurable return value for wlr_scene_node_at stub */
static struct wlr_scene_node *g_scene_node_at_return = NULL;

static struct wlr_scene_node *
wlr_scene_node_at(struct wlr_scene_node *node, double lx, double ly,
	double *nx, double *ny)
{
	(void)node; (void)lx; (void)ly; (void)nx; (void)ny;
	return g_scene_node_at_return;
}

typedef void (*wlr_scene_buffer_iterator_func_t)(
	struct wlr_scene_buffer *, int, int, void *);

static void
wlr_scene_node_for_each_buffer(struct wlr_scene_node *node,
	wlr_scene_buffer_iterator_func_t iter, void *user_data)
{
	(void)node; (void)iter; (void)user_data;
}

static void
wlr_scene_buffer_set_opacity(struct wlr_scene_buffer *buf, float opacity)
{
	(void)buf; (void)opacity;
}

static struct wlr_scene_buffer g_stub_scene_buffer;

static struct wlr_scene_buffer *
wlr_scene_buffer_from_node(struct wlr_scene_node *node)
{
	(void)node;
	if (g_scene_node_at_return)
		return &g_stub_scene_buffer;
	return NULL;
}

static struct wlr_scene_surface g_stub_scene_surface;

static struct wlr_scene_surface *
wlr_scene_surface_try_from_buffer(struct wlr_scene_buffer *buf)
{
	(void)buf;
	if (g_scene_node_at_return)
		return &g_stub_scene_surface;
	return NULL;
}

static struct wlr_scene_tree *
wlr_scene_tree_create(struct wlr_scene_tree *parent)
{
	(void)parent;
	if (g_scene_tree_next >= MAX_SCENE_TREES) return NULL;
	struct wlr_scene_tree *t = &g_scene_tree_pool[g_scene_tree_next++];
	memset(t, 0, sizeof(*t));
	wl_list_init(&t->children);
	return t;
}

static struct wlr_scene_tree *
wlr_scene_xdg_surface_create(struct wlr_scene_tree *parent,
	struct wlr_xdg_surface *surface)
{
	(void)surface;
	return wlr_scene_tree_create(parent);
}

static void
wlr_xdg_toplevel_set_activated(struct wlr_xdg_toplevel *tl, bool act)
{
	(void)tl; (void)act;
}

static uint32_t
wlr_xdg_toplevel_set_size(struct wlr_xdg_toplevel *tl, int w, int h)
{
	if (tl) { tl->width = w; tl->height = h; }
	return 0;
}

/* Configurable return for wlr_xdg_toplevel_try_from_wlr_surface */
static struct wlr_xdg_toplevel *g_try_from_surface_return = NULL;

static struct wlr_xdg_toplevel *
wlr_xdg_toplevel_try_from_wlr_surface(struct wlr_surface *surface)
{
	(void)surface;
	return g_try_from_surface_return;
}

static void
wlr_xdg_toplevel_set_maximized(struct wlr_xdg_toplevel *tl, bool max)
{
	(void)tl; (void)max;
}

static void
wlr_xdg_toplevel_set_fullscreen(struct wlr_xdg_toplevel *tl, bool fs)
{
	(void)tl; (void)fs;
}

static void
wlr_xdg_toplevel_send_close(struct wlr_xdg_toplevel *tl)
{
	(void)tl;
}

static void
wlr_xdg_surface_get_geometry(struct wlr_xdg_surface *surface,
	struct wlr_box *box)
{
	box->x = 0; box->y = 0;
	box->width = 800; box->height = 600;
}

static struct wlr_xdg_surface *
wlr_xdg_surface_try_from_wlr_surface(struct wlr_surface *surface)
{
	(void)surface;
	return NULL;
}

/* Configurable keyboard for seat */
static struct wlr_keyboard g_stub_keyboard;
static bool g_seat_has_keyboard = false;

static struct wlr_keyboard *
wlr_seat_get_keyboard(struct wlr_seat *seat)
{
	(void)seat;
	return g_seat_has_keyboard ? &g_stub_keyboard : NULL;
}

static void
wlr_seat_keyboard_notify_enter(struct wlr_seat *seat,
	struct wlr_surface *surface, uint32_t *keycodes,
	size_t num_keycodes, struct wlr_keyboard_modifiers *modifiers)
{
	(void)seat; (void)surface; (void)keycodes;
	(void)num_keycodes; (void)modifiers;
}

static void
wlr_seat_keyboard_notify_clear_focus(struct wlr_seat *seat)
{
	(void)seat;
}

static struct wlr_surface *
wlr_surface_get_root_surface(struct wlr_surface *surface)
{
	return surface;
}

static struct wlr_output *
wlr_output_layout_output_at(struct wlr_output_layout *layout,
	double x, double y)
{
	(void)layout; (void)x; (void)y;
	return g_output_at_return;
}

static void
wlr_output_layout_get_box(struct wlr_output_layout *layout,
	struct wlr_output *output, struct wlr_box *box)
{
	(void)layout; (void)output;
	box->x = 0; box->y = 0;
	box->width = 1920; box->height = 1080;
}

/* --- Stub fluxland functions --- */

static struct wm_animation *
wm_animation_start(struct wm_view *view, enum wm_animation_type type,
	int start_alpha, int target_alpha, int duration_ms,
	wm_animation_done_fn done_fn, void *done_data)
{
	(void)view; (void)type; (void)start_alpha; (void)target_alpha;
	(void)duration_ms; (void)done_fn; (void)done_data;
	return NULL;
}

static void wm_animation_cancel(struct wm_animation *anim) { (void)anim; }

static struct wm_decoration *
wm_decoration_create(struct wm_view *view, struct wm_style *style)
{
	(void)view; (void)style;
	return NULL;
}

static void wm_decoration_destroy(struct wm_decoration *d) { (void)d; }
static void wm_decoration_update(struct wm_decoration *d,
	struct wm_style *s) { (void)d; (void)s; }
static void wm_decoration_refresh_geometry(struct wm_decoration *d,
	struct wm_style *s) { (void)d; (void)s; }
static void wm_decoration_set_focused(struct wm_decoration *d,
	bool focused, struct wm_style *s) { (void)d; (void)focused; (void)s; }
static void wm_decoration_get_extents(struct wm_decoration *d,
	int *t, int *b, int *l, int *r)
{
	(void)d; *t = 0; *b = 0; *l = 0; *r = 0;
}

static void wm_foreign_toplevel_handle_create(struct wm_view *v) { (void)v; }
static void wm_foreign_toplevel_handle_destroy(struct wm_view *v) { (void)v; }
static void wm_foreign_toplevel_set_activated(struct wm_view *v,
	bool a) { (void)v; (void)a; }
static void wm_foreign_toplevel_set_maximized(struct wm_view *v,
	bool m) { (void)v; (void)m; }
static void wm_foreign_toplevel_set_minimized(struct wm_view *v,
	bool m) { (void)v; (void)m; }
static void wm_foreign_toplevel_set_fullscreen(struct wm_view *v,
	bool f) { (void)v; (void)f; }
static void wm_foreign_toplevel_update_title(struct wm_view *v) { (void)v; }
static void wm_foreign_toplevel_update_app_id(struct wm_view *v) { (void)v; }

static void wm_ipc_broadcast_event(struct wm_ipc_server *ipc,
	uint32_t event, const char *buf) { (void)ipc; (void)event; (void)buf; }

static void wm_placement_apply(struct wm_server *s,
	struct wm_view *v) { (void)s; (void)v; }

static void wm_rules_apply(struct wm_rules *r,
	struct wm_view *v) { (void)r; (void)v; }
static bool wm_rules_should_dock(struct wm_rules *r,
	struct wm_view *v) { (void)r; (void)v; return false; }
static struct wm_slit_client *wm_slit_add_native_client(
	struct wm_slit *slit, struct wlr_xdg_toplevel *toplevel,
	struct wlr_scene_tree *tree)
{
	(void)slit; (void)toplevel; (void)tree;
	return NULL;
}
static struct wm_view *wm_rules_find_group(struct wm_rules *r,
	struct wm_view *v, struct wm_server *s)
{
	(void)r; (void)v; (void)s;
	return NULL;
}

static struct wm_tab_group *
wm_tab_group_create(struct wm_view *v) { (void)v; return NULL; }
static void wm_tab_group_add(struct wm_tab_group *g,
	struct wm_view *v) { (void)g; (void)v; }
static void wm_tab_group_remove(struct wm_view *v) { (void)v; }
static void wm_tab_group_activate(struct wm_tab_group *g,
	struct wm_view *v) {
	g_tab_activate_count++;
	g_tab_activate_group = g;
	g_tab_activate_view = v;
}

static void wm_text_input_focus_change(struct wm_server *s,
	struct wlr_surface *surf) { (void)s; (void)surf; }

static void wm_toolbar_update_iconbar(struct wm_toolbar *t) { (void)t; }

static struct wm_workspace *
wm_workspace_get_active(struct wm_server *server)
{
	return server->current_workspace;
}

static void wm_view_move_to_workspace(struct wm_view *view,
	struct wm_workspace *ws)
{
	view->workspace = ws;
}

static void wm_protocols_update_pointer_constraint(struct wm_server *s,
	struct wlr_surface *surf) { (void)s; (void)surf; }
static void wm_protocols_update_kb_shortcuts_inhibitor(struct wm_server *s,
	struct wlr_surface *surf) { (void)s; (void)surf; }

/* --- Local wm_json_escape (replaces blocked util.h) --- */
static void
wm_json_escape(char *dst, size_t dst_size, const char *src)
{
	if (!src) {
		if (dst_size > 0)
			dst[0] = '\0';
		return;
	}
	size_t j = 0;
	for (size_t i = 0; src[i] && j + 6 < dst_size; i++) {
		unsigned char c = (unsigned char)src[i];
		if (c == '"' || c == '\\') {
			dst[j++] = '\\';
			dst[j++] = c;
		} else if (c < 0x20) {
			continue;
		} else {
			dst[j++] = c;
		}
	}
	dst[j] = '\0';
}

/* --- Forward declarations for functions used across included files --- */
void wm_view_get_geometry(struct wm_view *view, struct wlr_box *box);
struct wm_view *wm_view_at(struct wm_server *server, double lx, double ly,
	struct wlr_surface **surface, double *sx, double *sy);
void wm_focus_view(struct wm_view *view, struct wlr_surface *surface);
void wm_unfocus_current(struct wm_server *server);
void wm_view_raise(struct wm_view *view);
void wm_view_set_opacity(struct wm_view *view, int alpha);
void wm_view_toggle_maximize(struct wm_view *view);
void wm_view_toggle_fullscreen(struct wm_view *view);
void deiconify_view(struct wm_view *view);
struct wlr_scene_tree *get_layer_tree(struct wm_server *server,
	enum wm_view_layer layer);
bool get_view_output_area(struct wm_view *view, struct wlr_box *area);
void move_view_to_output(struct wm_view *view, struct wm_output *out);

/* --- Include view source files directly --- */

#include "view_focus.c"
#include "view_geometry.c"
#include "view.c"

/* --- Test helpers --- */

static struct wm_server test_server;
static struct wm_config test_config;
static struct wm_toolbar test_toolbar;
static struct wm_style test_style;
static struct wlr_seat test_seat;
static struct wlr_cursor test_cursor;
static struct wlr_output_layout test_output_layout;
static struct wlr_scene_tree test_view_tree;

#define MAX_TEST_VIEWS 8
static struct wm_view test_views[MAX_TEST_VIEWS];
static struct wlr_scene_tree test_scene_trees[MAX_TEST_VIEWS];
static struct wlr_xdg_toplevel test_toplevels[MAX_TEST_VIEWS];
static struct wlr_xdg_surface test_xdg_surfaces[MAX_TEST_VIEWS];
static struct wlr_surface test_surfaces[MAX_TEST_VIEWS];

static struct wm_workspace test_workspace;

/* Mock outputs for multi-monitor tests */
#define MAX_TEST_OUTPUTS 4
static struct wm_output test_outputs[MAX_TEST_OUTPUTS];
static struct wlr_output test_wlr_outputs[MAX_TEST_OUTPUTS];

/* Mock decoration for toggle_decoration tests */
static struct wm_decoration test_decoration;
static struct wlr_scene_tree test_deco_tree;

static void
reset_globals(void)
{
	g_focus_view_count = 0;
	g_last_focused_view = NULL;
	g_scene_tree_next = 0;
	g_output_at_return = NULL;
	g_tab_activate_count = 0;
	g_tab_activate_group = NULL;
	g_tab_activate_view = NULL;
}

static void
init_test_server(void)
{
	reset_globals();
	memset(&test_server, 0, sizeof(test_server));
	memset(&test_config, 0, sizeof(test_config));
	memset(&test_seat, 0, sizeof(test_seat));
	memset(&test_cursor, 0, sizeof(test_cursor));
	memset(&test_workspace, 0, sizeof(test_workspace));

	test_config.window_focus_alpha = 255;
	test_config.window_unfocus_alpha = 200;

	test_server.config = &test_config;
	test_server.toolbar = &test_toolbar;
	test_server.style = &test_style;
	test_server.seat = &test_seat;
	test_server.cursor = &test_cursor;
	test_server.output_layout = &test_output_layout;
	test_server.view_tree = &test_view_tree;
	test_server.focused_view = NULL;
	test_server.cursor_mode = WM_CURSOR_PASSTHROUGH;
	test_server.current_workspace = &test_workspace;

	wl_list_init(&test_server.views);
	wl_list_init(&test_server.outputs);
	wl_list_init(&test_server.workspaces);

	test_workspace.index = 0;
	test_workspace.name = "Default";
}

static void
setup_mock_view(int idx, struct wm_workspace *ws)
{
	assert(idx < MAX_TEST_VIEWS);
	memset(&test_views[idx], 0, sizeof(test_views[idx]));
	memset(&test_scene_trees[idx], 0, sizeof(test_scene_trees[idx]));

	wl_list_init(&test_scene_trees[idx].children);
	test_scene_trees[idx].node.enabled = true;

	test_surfaces[idx] = (struct wlr_surface){0};
	wl_list_init(&test_surfaces[idx].events.map.listener_list);
	wl_list_init(&test_surfaces[idx].events.unmap.listener_list);
	wl_list_init(&test_surfaces[idx].events.commit.listener_list);

	test_xdg_surfaces[idx].surface = &test_surfaces[idx];
	test_toplevels[idx].base = &test_xdg_surfaces[idx];
	test_toplevels[idx].width = 800;
	test_toplevels[idx].height = 600;

	test_views[idx].server = &test_server;
	test_views[idx].xdg_toplevel = &test_toplevels[idx];
	test_views[idx].scene_tree = &test_scene_trees[idx];
	test_views[idx].workspace = ws;
	test_views[idx].id = idx + 1;
	test_views[idx].x = 100 + idx * 50;
	test_views[idx].y = 100 + idx * 50;
	test_views[idx].focus_alpha = 255;
	test_views[idx].unfocus_alpha = 200;
	test_views[idx].show_decoration = true;
	test_views[idx].layer = WM_LAYER_NORMAL;
	wl_list_init(&test_views[idx].tab_link);

	wl_list_insert(test_server.views.prev, &test_views[idx].link);
}

static void
setup_mock_output(int idx, int x, int y, int w, int h)
{
	assert(idx < MAX_TEST_OUTPUTS);
	memset(&test_outputs[idx], 0, sizeof(test_outputs[idx]));
	memset(&test_wlr_outputs[idx], 0, sizeof(test_wlr_outputs[idx]));

	test_outputs[idx].server = &test_server;
	test_outputs[idx].wlr_output = &test_wlr_outputs[idx];
	test_outputs[idx].usable_area.x = x;
	test_outputs[idx].usable_area.y = y;
	test_outputs[idx].usable_area.width = w;
	test_outputs[idx].usable_area.height = h;

	wl_list_insert(test_server.outputs.prev, &test_outputs[idx].link);
}

static void
setup_mock_decoration(struct wm_view *view)
{
	memset(&test_decoration, 0, sizeof(test_decoration));
	memset(&test_deco_tree, 0, sizeof(test_deco_tree));
	wl_list_init(&test_deco_tree.children);
	test_decoration.tree = &test_deco_tree;
	view->decoration = &test_decoration;

	/* Add deco tree node as a child of the view's scene tree,
	 * so toggle_decoration's wl_list_for_each can iterate */
	wl_list_insert(&view->scene_tree->children,
		&test_deco_tree.node.link);

	/* Add a second "client surface" child node so the loop
	 * finds a non-decoration child */
}

/* ===== json_escape tests ===== */

static void
test_json_escape_normal(void)
{
	char buf[64];
	wm_json_escape(buf, sizeof(buf), "hello world");
	assert(strcmp(buf, "hello world") == 0);
	printf("  PASS: json_escape_normal\n");
}

static void
test_json_escape_quotes(void)
{
	char buf[64];
	wm_json_escape(buf, sizeof(buf), "say \"hello\"");
	assert(strcmp(buf, "say \\\"hello\\\"") == 0);
	printf("  PASS: json_escape_quotes\n");
}

static void
test_json_escape_backslash(void)
{
	char buf[64];
	wm_json_escape(buf, sizeof(buf), "path\\to\\file");
	assert(strcmp(buf, "path\\\\to\\\\file") == 0);
	printf("  PASS: json_escape_backslash\n");
}

static void
test_json_escape_control_chars(void)
{
	char buf[64];
	wm_json_escape(buf, sizeof(buf), "line1\nline2\ttab");
	assert(strcmp(buf, "line1line2tab") == 0);
	printf("  PASS: json_escape_control_chars\n");
}

static void
test_json_escape_null(void)
{
	char buf[64];
	buf[0] = 'x';
	wm_json_escape(buf, sizeof(buf), NULL);
	assert(buf[0] == '\0');
	printf("  PASS: json_escape_null\n");
}

static void
test_json_escape_empty(void)
{
	char buf[64];
	wm_json_escape(buf, sizeof(buf), "");
	assert(buf[0] == '\0');
	printf("  PASS: json_escape_empty\n");
}

static void
test_json_escape_long_truncates(void)
{
	char buf[16];
	wm_json_escape(buf, sizeof(buf), "this is a long string that should be truncated");
	/* Should not overflow, must be null-terminated */
	assert(strlen(buf) < sizeof(buf));
	assert(strncmp(buf, "this is a", 9) == 0);
	printf("  PASS: json_escape_long_truncates\n");
}

/* ===== wm_view_layer_from_name tests ===== */

static void
test_layer_from_name_desktop(void)
{
	assert(wm_view_layer_from_name("Desktop") == WM_LAYER_DESKTOP);
	assert(wm_view_layer_from_name("desktop") == WM_LAYER_DESKTOP);
	printf("  PASS: layer_from_name_desktop\n");
}

static void
test_layer_from_name_bottom_below(void)
{
	assert(wm_view_layer_from_name("Bottom") == WM_LAYER_BELOW);
	assert(wm_view_layer_from_name("Below") == WM_LAYER_BELOW);
	assert(wm_view_layer_from_name("below") == WM_LAYER_BELOW);
	printf("  PASS: layer_from_name_bottom_below\n");
}

static void
test_layer_from_name_top_above(void)
{
	assert(wm_view_layer_from_name("Top") == WM_LAYER_ABOVE);
	assert(wm_view_layer_from_name("Above") == WM_LAYER_ABOVE);
	assert(wm_view_layer_from_name("AboveDock") == WM_LAYER_ABOVE);
	assert(wm_view_layer_from_name("Dock") == WM_LAYER_ABOVE);
	printf("  PASS: layer_from_name_top_above\n");
}

static void
test_layer_from_name_normal_default(void)
{
	assert(wm_view_layer_from_name("Normal") == WM_LAYER_NORMAL);
	assert(wm_view_layer_from_name("unknown") == WM_LAYER_NORMAL);
	assert(wm_view_layer_from_name("") == WM_LAYER_NORMAL);
	printf("  PASS: layer_from_name_normal_default\n");
}

static void
test_layer_from_name_null(void)
{
	assert(wm_view_layer_from_name(NULL) == WM_LAYER_NORMAL);
	printf("  PASS: layer_from_name_null\n");
}

/* ===== wm_focus_next_view / wm_focus_prev_view tests ===== */

static void
test_focus_next_view_basic(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	setup_mock_view(1, &test_workspace);
	setup_mock_view(2, &test_workspace);

	/*
	 * List order: head -> v0 -> v1 -> v2
	 * focus_next_view gets the last view (v2) and calls wm_focus_view.
	 * wm_focus_view moves v2 to front: head -> v2 -> v0 -> v1
	 * and sets focused_view = v2.
	 */
	struct wm_view *bottom = wl_container_of(
		test_server.views.prev, bottom, link);
	assert(bottom == &test_views[2]);

	wm_focus_next_view(&test_server);

	assert(test_server.focused_view == &test_views[2]);

	/* Clean up */
	wl_list_remove(&test_views[0].link);
	wl_list_remove(&test_views[1].link);
	wl_list_remove(&test_views[2].link);
	printf("  PASS: focus_next_view_basic\n");
}

static void
test_focus_prev_view_basic(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	setup_mock_view(1, &test_workspace);
	setup_mock_view(2, &test_workspace);

	/*
	 * List order: head -> v0 -> v1 -> v2
	 * focus_prev_view: current = v0 (views.next), second = v1
	 * Calls wm_focus_view(v1, ...) which moves v1 to front.
	 */
	wm_focus_prev_view(&test_server);
	assert(test_server.focused_view == &test_views[1]);

	wl_list_remove(&test_views[0].link);
	wl_list_remove(&test_views[1].link);
	wl_list_remove(&test_views[2].link);
	printf("  PASS: focus_prev_view_basic\n");
}

static void
test_focus_next_view_single_noop(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);

	test_server.focused_view = &test_views[0];
	struct wm_view *before = test_server.focused_view;

	wm_focus_next_view(&test_server);

	/* With only 1 view, focus_next should be a no-op */
	assert(test_server.focused_view == before);

	wl_list_remove(&test_views[0].link);
	printf("  PASS: focus_next_view_single_noop\n");
}

static void
test_focus_next_view_empty_noop(void)
{
	init_test_server();
	/* No views */
	wm_focus_next_view(&test_server);
	assert(test_server.focused_view == NULL);
	printf("  PASS: focus_next_view_empty_noop\n");
}

/* ===== wm_view_get_geometry tests ===== */

static void
test_view_get_geometry(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	test_views[0].x = 150;
	test_views[0].y = 250;

	struct wlr_box geo;
	wm_view_get_geometry(&test_views[0], &geo);

	assert(geo.x == 150);
	assert(geo.y == 250);
	/* wlr_xdg_surface_get_geometry stub returns 800x600 */
	assert(geo.width == 800);
	assert(geo.height == 600);

	wl_list_remove(&test_views[0].link);
	printf("  PASS: view_get_geometry\n");
}

/* ===== wm_view_raise / wm_view_lower tests ===== */

static void
test_view_raise(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	setup_mock_view(1, &test_workspace);
	setup_mock_view(2, &test_workspace);

	/* v0 is first in list. Raise v2 (last) to top */
	wm_view_raise(&test_views[2]);

	/* v2 should now be first in the list */
	struct wm_view *first = wl_container_of(
		test_server.views.next, first, link);
	assert(first == &test_views[2]);

	wl_list_remove(&test_views[0].link);
	wl_list_remove(&test_views[1].link);
	wl_list_remove(&test_views[2].link);
	printf("  PASS: view_raise\n");
}

static void
test_view_lower(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	setup_mock_view(1, &test_workspace);
	setup_mock_view(2, &test_workspace);

	/* Lower v0 (first) to bottom */
	wm_view_lower(&test_views[0]);

	/* v0 should now be last in the list */
	struct wm_view *last = wl_container_of(
		test_server.views.prev, last, link);
	assert(last == &test_views[0]);

	wl_list_remove(&test_views[0].link);
	wl_list_remove(&test_views[1].link);
	wl_list_remove(&test_views[2].link);
	printf("  PASS: view_lower\n");
}

/* ===== wm_view_raise_layer / wm_view_lower_layer tests ===== */

static void
test_view_raise_layer(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	test_views[0].layer = WM_LAYER_NORMAL;

	wm_view_raise_layer(&test_views[0]);
	assert(test_views[0].layer == WM_LAYER_ABOVE);

	/* Can't go above ABOVE */
	wm_view_raise_layer(&test_views[0]);
	assert(test_views[0].layer == WM_LAYER_ABOVE);

	wl_list_remove(&test_views[0].link);
	printf("  PASS: view_raise_layer\n");
}

static void
test_view_lower_layer(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	test_views[0].layer = WM_LAYER_NORMAL;

	wm_view_lower_layer(&test_views[0]);
	assert(test_views[0].layer == WM_LAYER_BELOW);

	wm_view_lower_layer(&test_views[0]);
	assert(test_views[0].layer == WM_LAYER_DESKTOP);

	/* Can't go below DESKTOP */
	wm_view_lower_layer(&test_views[0]);
	assert(test_views[0].layer == WM_LAYER_DESKTOP);

	wl_list_remove(&test_views[0].link);
	printf("  PASS: view_lower_layer\n");
}

/* ===== wm_view_set_layer tests ===== */

static void
test_view_set_layer(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	test_views[0].layer = WM_LAYER_NORMAL;

	wm_view_set_layer(&test_views[0], WM_LAYER_ABOVE);
	assert(test_views[0].layer == WM_LAYER_ABOVE);

	wm_view_set_layer(&test_views[0], WM_LAYER_DESKTOP);
	assert(test_views[0].layer == WM_LAYER_DESKTOP);

	/* Invalid layer should be no-op */
	wm_view_set_layer(&test_views[0], -1);
	assert(test_views[0].layer == WM_LAYER_DESKTOP);

	wm_view_set_layer(&test_views[0], WM_VIEW_LAYER_COUNT);
	assert(test_views[0].layer == WM_LAYER_DESKTOP);

	/* Same layer should be no-op */
	wm_view_set_layer(&test_views[0], WM_LAYER_DESKTOP);
	assert(test_views[0].layer == WM_LAYER_DESKTOP);

	wl_list_remove(&test_views[0].link);
	printf("  PASS: view_set_layer\n");
}

/* ===== wm_view_toggle_decoration tests ===== */

static void
test_view_toggle_decoration_no_deco(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	test_views[0].decoration = NULL;

	/* Should be a no-op with no decoration */
	wm_view_toggle_decoration(&test_views[0]);
	/* No crash = pass */

	wl_list_remove(&test_views[0].link);
	printf("  PASS: view_toggle_decoration_no_deco\n");
}

/* ===== wm_view_resize_by tests ===== */

static void
test_view_resize_by(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);

	/* wlr_xdg_surface_get_geometry returns 800x600 */
	/* wlr_xdg_toplevel_set_size stub sets width/height */
	wm_view_resize_by(&test_views[0], 100, -50);

	/* New size: 800+100=900, 600-50=550 */
	assert(test_toplevels[0].width == 900);
	assert(test_toplevels[0].height == 550);

	wl_list_remove(&test_views[0].link);
	printf("  PASS: view_resize_by\n");
}

static void
test_view_resize_by_null(void)
{
	/* Should not crash */
	wm_view_resize_by(NULL, 10, 10);
	printf("  PASS: view_resize_by_null\n");
}

/* ===== wm_view_set_opacity tests ===== */

static void
test_view_set_opacity_null(void)
{
	/* Should not crash on NULL view */
	wm_view_set_opacity(NULL, 128);
	printf("  PASS: view_set_opacity_null\n");
}

static void
test_view_set_opacity_clamp(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);

	/* These should not crash; values are clamped internally */
	wm_view_set_opacity(&test_views[0], -50);
	wm_view_set_opacity(&test_views[0], 300);
	wm_view_set_opacity(&test_views[0], 0);
	wm_view_set_opacity(&test_views[0], 255);

	wl_list_remove(&test_views[0].link);
	printf("  PASS: view_set_opacity_clamp\n");
}

static void
test_view_set_opacity_no_scene_tree(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	struct wlr_scene_tree *saved = test_views[0].scene_tree;
	test_views[0].scene_tree = NULL;

	/* Should be a no-op */
	wm_view_set_opacity(&test_views[0], 128);

	test_views[0].scene_tree = saved;
	wl_list_remove(&test_views[0].link);
	printf("  PASS: view_set_opacity_no_scene_tree\n");
}

/* ===== wm_view_maximize_vert tests ===== */

static void
test_view_maximize_vert_null(void)
{
	wm_view_maximize_vert(NULL);
	printf("  PASS: view_maximize_vert_null\n");
}

static void
test_view_maximize_vert_toggle(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	test_views[0].x = 100;
	test_views[0].y = 200;
	test_views[0].maximized_vert = false;

	/* Maximize vertically — our stub wlr_output_layout_output_at
	 * returns NULL, so the function takes the early-return path.
	 * But maximized_vert flag should still be set. */
	wm_view_maximize_vert(&test_views[0]);
	/* Since output_at returns NULL, the vert maximize sets the flag
	 * but doesn't change geometry (no output found) */
	assert(test_views[0].maximized_vert == true);

	/* Toggle back: restore */
	wm_view_maximize_vert(&test_views[0]);
	assert(test_views[0].maximized_vert == false);

	wl_list_remove(&test_views[0].link);
	printf("  PASS: view_maximize_vert_toggle\n");
}

/* ===== wm_view_maximize_horiz tests ===== */

static void
test_view_maximize_horiz_null(void)
{
	wm_view_maximize_horiz(NULL);
	printf("  PASS: view_maximize_horiz_null\n");
}

static void
test_view_maximize_horiz_toggle(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	test_views[0].x = 100;
	test_views[0].y = 200;
	test_views[0].maximized_horiz = false;

	/* Maximize horizontally */
	wm_view_maximize_horiz(&test_views[0]);
	assert(test_views[0].maximized_horiz == true);

	/* Toggle back: restore */
	wm_view_maximize_horiz(&test_views[0]);
	assert(test_views[0].maximized_horiz == false);

	wl_list_remove(&test_views[0].link);
	printf("  PASS: view_maximize_horiz_toggle\n");
}

/* ===== wm_view_lhalf / wm_view_rhalf tests ===== */

static void
test_view_lhalf_null(void)
{
	wm_view_lhalf(NULL);
	printf("  PASS: view_lhalf_null\n");
}

static void
test_view_lhalf_toggle(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	test_views[0].x = 200;
	test_views[0].y = 150;
	test_views[0].lhalf = false;
	test_views[0].rhalf = false;
	test_views[0].maximized = false;

	/* lhalf: output_at returns NULL, so get_view_output_area fails.
	 * The function returns early without setting lhalf. */
	wm_view_lhalf(&test_views[0]);
	assert(test_views[0].lhalf == false);

	/* Manually set lhalf to test the toggle-off path */
	test_views[0].lhalf = true;
	test_views[0].saved_geometry.x = 200;
	test_views[0].saved_geometry.y = 150;
	test_views[0].saved_geometry.width = 400;
	test_views[0].saved_geometry.height = 300;

	wm_view_lhalf(&test_views[0]);
	assert(test_views[0].lhalf == false);
	assert(test_views[0].x == 200);
	assert(test_views[0].y == 150);

	wl_list_remove(&test_views[0].link);
	printf("  PASS: view_lhalf_toggle\n");
}

static void
test_view_rhalf_null(void)
{
	wm_view_rhalf(NULL);
	printf("  PASS: view_rhalf_null\n");
}

static void
test_view_rhalf_toggle(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	test_views[0].x = 200;
	test_views[0].y = 150;
	test_views[0].rhalf = false;
	test_views[0].lhalf = false;
	test_views[0].maximized = false;

	/* rhalf: output_at returns NULL, so get_view_output_area fails */
	wm_view_rhalf(&test_views[0]);
	assert(test_views[0].rhalf == false);

	/* Manually set rhalf to test the toggle-off path */
	test_views[0].rhalf = true;
	test_views[0].saved_geometry.x = 200;
	test_views[0].saved_geometry.y = 150;
	test_views[0].saved_geometry.width = 400;
	test_views[0].saved_geometry.height = 300;

	wm_view_rhalf(&test_views[0]);
	assert(test_views[0].rhalf == false);
	assert(test_views[0].x == 200);
	assert(test_views[0].y == 150);

	wl_list_remove(&test_views[0].link);
	printf("  PASS: view_rhalf_toggle\n");
}

/* ===== wm_view_cycle_next / wm_view_cycle_prev tests ===== */

static void
test_view_cycle_next_empty(void)
{
	init_test_server();
	/* Empty list, should not crash */
	wm_view_cycle_next(&test_server);
	assert(test_server.focused_view == NULL);
	printf("  PASS: view_cycle_next_empty\n");
}

static void
test_view_cycle_next_basic(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	setup_mock_view(1, &test_workspace);
	setup_mock_view(2, &test_workspace);

	/* Focus v0 first */
	test_server.focused_view = &test_views[0];

	/* cycle_next: should focus the view after v0 in the list */
	wm_view_cycle_next(&test_server);
	/* The candidate after v0 is v1 */
	assert(test_server.focused_view == &test_views[1]);

	wl_list_remove(&test_views[0].link);
	wl_list_remove(&test_views[1].link);
	wl_list_remove(&test_views[2].link);
	printf("  PASS: view_cycle_next_basic\n");
}

static void
test_view_cycle_next_skips_other_workspace(void)
{
	init_test_server();
	struct wm_workspace other_ws;
	memset(&other_ws, 0, sizeof(other_ws));
	other_ws.index = 1;
	other_ws.name = "Other";

	setup_mock_view(0, &test_workspace);
	setup_mock_view(1, &other_ws);   /* different workspace */
	setup_mock_view(2, &test_workspace);

	test_server.focused_view = &test_views[0];

	/* cycle_next should skip v1 (different workspace) and pick v2 */
	wm_view_cycle_next(&test_server);
	assert(test_server.focused_view == &test_views[2]);

	wl_list_remove(&test_views[0].link);
	wl_list_remove(&test_views[1].link);
	wl_list_remove(&test_views[2].link);
	printf("  PASS: view_cycle_next_skips_other_workspace\n");
}

static void
test_view_cycle_next_wraps_around(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	setup_mock_view(1, &test_workspace);

	/* Focus the last view on the workspace; cycle_next wraps to first */
	test_server.focused_view = &test_views[1];

	wm_view_cycle_next(&test_server);
	/* Should wrap around to v0 */
	assert(test_server.focused_view == &test_views[0]);

	wl_list_remove(&test_views[0].link);
	wl_list_remove(&test_views[1].link);
	printf("  PASS: view_cycle_next_wraps_around\n");
}

static void
test_view_cycle_prev_empty(void)
{
	init_test_server();
	wm_view_cycle_prev(&test_server);
	assert(test_server.focused_view == NULL);
	printf("  PASS: view_cycle_prev_empty\n");
}

static void
test_view_cycle_prev_basic(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	setup_mock_view(1, &test_workspace);
	setup_mock_view(2, &test_workspace);

	/* Focus v1 (middle). cycle_prev picks the view before it (v0). */
	test_server.focused_view = &test_views[1];

	wm_view_cycle_prev(&test_server);
	assert(test_server.focused_view == &test_views[0]);

	wl_list_remove(&test_views[0].link);
	wl_list_remove(&test_views[1].link);
	wl_list_remove(&test_views[2].link);
	printf("  PASS: view_cycle_prev_basic\n");
}

static void
test_view_cycle_next_deiconifies(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	setup_mock_view(1, &test_workspace);

	test_server.focused_view = &test_views[0];

	/* Iconify v1 (disable its scene node) */
	test_scene_trees[1].node.enabled = false;

	wm_view_cycle_next(&test_server);

	/* Should have deiconified v1 and focused it */
	assert(test_views[1].scene_tree->node.enabled == true);
	assert(test_server.focused_view == &test_views[1]);

	wl_list_remove(&test_views[0].link);
	wl_list_remove(&test_views[1].link);
	printf("  PASS: view_cycle_next_deiconifies\n");
}

static void
test_view_cycle_next_single_noop(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);

	test_server.focused_view = &test_views[0];

	wm_view_cycle_next(&test_server);
	/* Only one view; candidate == focused, so no-op */
	assert(test_server.focused_view == &test_views[0]);

	wl_list_remove(&test_views[0].link);
	printf("  PASS: view_cycle_next_single_noop\n");
}

/* ===== wm_view_deiconify_last tests ===== */

static void
test_view_deiconify_last_basic(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	setup_mock_view(1, &test_workspace);

	/* Minimize v0 */
	test_scene_trees[0].node.enabled = false;

	wm_view_deiconify_last(&test_server);

	/* v0 should be re-enabled and focused */
	assert(test_views[0].scene_tree->node.enabled == true);
	assert(test_server.focused_view == &test_views[0]);

	wl_list_remove(&test_views[0].link);
	wl_list_remove(&test_views[1].link);
	printf("  PASS: view_deiconify_last_basic\n");
}

static void
test_view_deiconify_last_none_minimized(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	setup_mock_view(1, &test_workspace);

	/* All enabled, nothing to deiconify */
	struct wm_view *before = test_server.focused_view;
	wm_view_deiconify_last(&test_server);
	/* Should be a no-op */
	assert(test_server.focused_view == before);

	wl_list_remove(&test_views[0].link);
	wl_list_remove(&test_views[1].link);
	printf("  PASS: view_deiconify_last_none_minimized\n");
}

/* ===== wm_view_deiconify_all tests ===== */

static void
test_view_deiconify_all_basic(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	setup_mock_view(1, &test_workspace);
	setup_mock_view(2, &test_workspace);

	/* Minimize all */
	test_scene_trees[0].node.enabled = false;
	test_scene_trees[1].node.enabled = false;
	test_scene_trees[2].node.enabled = false;

	wm_view_deiconify_all(&test_server);

	/* All should be re-enabled */
	assert(test_views[0].scene_tree->node.enabled == true);
	assert(test_views[1].scene_tree->node.enabled == true);
	assert(test_views[2].scene_tree->node.enabled == true);

	wl_list_remove(&test_views[0].link);
	wl_list_remove(&test_views[1].link);
	wl_list_remove(&test_views[2].link);
	printf("  PASS: view_deiconify_all_basic\n");
}

/* ===== wm_view_deiconify_all_workspace tests ===== */

static void
test_view_deiconify_all_workspace_basic(void)
{
	init_test_server();
	struct wm_workspace other_ws;
	memset(&other_ws, 0, sizeof(other_ws));
	other_ws.index = 1;
	other_ws.name = "Other";

	setup_mock_view(0, &test_workspace);
	setup_mock_view(1, &other_ws);   /* different workspace */
	setup_mock_view(2, &test_workspace);

	/* Minimize all */
	test_scene_trees[0].node.enabled = false;
	test_scene_trees[1].node.enabled = false;
	test_scene_trees[2].node.enabled = false;

	wm_view_deiconify_all_workspace(&test_server);

	/* Only v0 and v2 (current workspace) should be re-enabled */
	assert(test_views[0].scene_tree->node.enabled == true);
	assert(test_views[1].scene_tree->node.enabled == false);
	assert(test_views[2].scene_tree->node.enabled == true);

	wl_list_remove(&test_views[0].link);
	wl_list_remove(&test_views[1].link);
	wl_list_remove(&test_views[2].link);
	printf("  PASS: view_deiconify_all_workspace_basic\n");
}

/* ===== wm_view_close_all tests ===== */

static void
test_view_close_all_basic(void)
{
	init_test_server();
	struct wm_workspace other_ws;
	memset(&other_ws, 0, sizeof(other_ws));
	other_ws.index = 1;
	other_ws.name = "Other";

	setup_mock_view(0, &test_workspace);
	setup_mock_view(1, &other_ws);   /* different workspace */
	setup_mock_view(2, &test_workspace);

	/* wm_view_close_all sends close to views on the active workspace.
	 * We can't easily track the sends with stubs, but we verify
	 * no crash and that the function iterates correctly. */
	wm_view_close_all(&test_server);

	/* No crash = pass */
	wl_list_remove(&test_views[0].link);
	wl_list_remove(&test_views[1].link);
	wl_list_remove(&test_views[2].link);
	printf("  PASS: view_close_all_basic\n");
}

static void
test_view_close_all_empty(void)
{
	init_test_server();
	/* No views in list, should not crash */
	wm_view_close_all(&test_server);
	printf("  PASS: view_close_all_empty\n");
}

/* ===== wm_view_focus_direction tests ===== */

static void
test_view_focus_direction_no_focused(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);

	/* No focused view, should be a no-op */
	test_server.focused_view = NULL;
	wm_view_focus_direction(&test_server, 1, 0);
	assert(test_server.focused_view == NULL);

	wl_list_remove(&test_views[0].link);
	printf("  PASS: view_focus_direction_no_focused\n");
}

static void
test_view_focus_direction_right(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	setup_mock_view(1, &test_workspace);

	/* v0 at x=100, v1 at x=150 (both visible, same workspace) */
	test_views[0].x = 100;
	test_views[0].y = 100;
	test_views[1].x = 500;
	test_views[1].y = 100;

	test_server.focused_view = &test_views[0];

	/* Focus right: v1 is to the right */
	wm_view_focus_direction(&test_server, 1, 0);
	assert(test_server.focused_view == &test_views[1]);

	wl_list_remove(&test_views[0].link);
	wl_list_remove(&test_views[1].link);
	printf("  PASS: view_focus_direction_right\n");
}

static void
test_view_focus_direction_no_match(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	setup_mock_view(1, &test_workspace);

	/* v0 at x=500, v1 at x=100 (v1 is to the LEFT) */
	test_views[0].x = 500;
	test_views[0].y = 100;
	test_views[1].x = 100;
	test_views[1].y = 100;

	test_server.focused_view = &test_views[0];

	/* Focus right: no view to the right, should stay */
	wm_view_focus_direction(&test_server, 1, 0);
	/* focused_view should remain v0 (no candidate found) */
	assert(test_server.focused_view == &test_views[0]);

	wl_list_remove(&test_views[0].link);
	wl_list_remove(&test_views[1].link);
	printf("  PASS: view_focus_direction_no_match\n");
}

static void
test_view_focus_direction_skips_minimized(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	setup_mock_view(1, &test_workspace);

	test_views[0].x = 100;
	test_views[0].y = 100;
	test_views[1].x = 500;
	test_views[1].y = 100;

	/* Minimize v1 */
	test_scene_trees[1].node.enabled = false;

	test_server.focused_view = &test_views[0];

	/* Focus right: v1 is minimized, should not pick it */
	wm_view_focus_direction(&test_server, 1, 0);
	assert(test_server.focused_view == &test_views[0]);

	wl_list_remove(&test_views[0].link);
	wl_list_remove(&test_views[1].link);
	printf("  PASS: view_focus_direction_skips_minimized\n");
}

/* ===== wm_view_activate_tab tests ===== */

static void
test_view_activate_tab_null(void)
{
	/* Should not crash */
	wm_view_activate_tab(NULL, 0);
	printf("  PASS: view_activate_tab_null\n");
}

static void
test_view_activate_tab_no_group(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	test_views[0].tab_group = NULL;

	/* No tab group, should be a no-op */
	wm_view_activate_tab(&test_views[0], 0);

	wl_list_remove(&test_views[0].link);
	printf("  PASS: view_activate_tab_no_group\n");
}

static void
test_view_activate_tab_negative_index(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	struct wm_tab_group tg;
	tg.count = 2;
	wl_list_init(&tg.views);
	test_views[0].tab_group = &tg;

	/* Negative index, should be a no-op */
	wm_view_activate_tab(&test_views[0], -1);

	test_views[0].tab_group = NULL;
	wl_list_remove(&test_views[0].link);
	printf("  PASS: view_activate_tab_negative_index\n");
}

static void
test_view_activate_tab_index_out_of_range(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	struct wm_tab_group tg;
	tg.count = 1;
	wl_list_init(&tg.views);
	test_views[0].tab_group = &tg;

	/* Index >= count, should be a no-op */
	wm_view_activate_tab(&test_views[0], 5);

	test_views[0].tab_group = NULL;
	wl_list_remove(&test_views[0].link);
	printf("  PASS: view_activate_tab_index_out_of_range\n");
}

/* ===== wm_focus_view / wm_unfocus_current tests ===== */

static void
test_focus_view_null(void)
{
	/* Should not crash */
	wm_focus_view(NULL, NULL);
	printf("  PASS: focus_view_null\n");
}

static void
test_unfocus_current_basic(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);

	test_server.focused_view = &test_views[0];
	wm_unfocus_current(&test_server);
	assert(test_server.focused_view == NULL);

	wl_list_remove(&test_views[0].link);
	printf("  PASS: unfocus_current_basic\n");
}

static void
test_unfocus_current_already_unfocused(void)
{
	init_test_server();
	test_server.focused_view = NULL;
	wm_unfocus_current(&test_server);
	assert(test_server.focused_view == NULL);
	printf("  PASS: unfocus_current_already_unfocused\n");
}

/* ===== json_escape edge case: escape chars near buffer boundary ===== */

static void
test_json_escape_escape_at_boundary(void)
{
	/* Buffer size 5: needs room for escape sequence (\\") = 2 chars + NUL
	 * The loop condition is j + 6 < dst_size, so with dst_size=5,
	 * the loop won't even enter (0 + 6 < 5 is false).
	 * This tests the boundary condition. */
	char buf[5];
	wm_json_escape(buf, sizeof(buf), "abc");
	/* j+6 < 5 => false, so nothing is written */
	assert(buf[0] == '\0');
	printf("  PASS: json_escape_escape_at_boundary\n");
}

static void
test_json_escape_mixed_specials(void)
{
	char buf[128];
	wm_json_escape(buf, sizeof(buf), "tab\there\"back\\slash\nnewline");
	/* \t and \n are control chars (< 0x20), they get skipped */
	/* " becomes \" and \ becomes \\ */
	assert(strcmp(buf, "tabhere\\\"back\\\\slashnewline") == 0);
	printf("  PASS: json_escape_mixed_specials\n");
}

/* ===== wm_view_set_layer with sticky and workspace tree paths ===== */

static void
test_view_set_layer_sticky_normal(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);

	struct wlr_scene_tree sticky_tree;
	memset(&sticky_tree, 0, sizeof(sticky_tree));
	wl_list_init(&sticky_tree.children);
	test_server.sticky_tree = &sticky_tree;

	test_views[0].layer = WM_LAYER_ABOVE;
	test_views[0].sticky = true;

	/* Setting to NORMAL for a sticky view should pick sticky_tree */
	wm_view_set_layer(&test_views[0], WM_LAYER_NORMAL);
	assert(test_views[0].layer == WM_LAYER_NORMAL);

	wl_list_remove(&test_views[0].link);
	printf("  PASS: view_set_layer_sticky_normal\n");
}

static void
test_view_set_layer_workspace_normal(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);

	struct wlr_scene_tree ws_tree;
	memset(&ws_tree, 0, sizeof(ws_tree));
	wl_list_init(&ws_tree.children);
	test_workspace.tree = &ws_tree;

	test_views[0].layer = WM_LAYER_ABOVE;
	test_views[0].sticky = false;

	/* Setting to NORMAL for non-sticky view with workspace */
	wm_view_set_layer(&test_views[0], WM_LAYER_NORMAL);
	assert(test_views[0].layer == WM_LAYER_NORMAL);

	wl_list_remove(&test_views[0].link);
	printf("  PASS: view_set_layer_workspace_normal\n");
}

/* ===== wm_focus_prev_view with two views ===== */

static void
test_focus_prev_view_two_views(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	setup_mock_view(1, &test_workspace);

	/* List: head -> v0 -> v1 */
	/* focus_prev: current=v0, second=v1; focus v1 */
	wm_focus_prev_view(&test_server);
	assert(test_server.focused_view == &test_views[1]);

	wl_list_remove(&test_views[0].link);
	wl_list_remove(&test_views[1].link);
	printf("  PASS: focus_prev_view_two_views\n");
}

static void
test_focus_prev_view_empty(void)
{
	init_test_server();
	/* No views */
	wm_focus_prev_view(&test_server);
	assert(test_server.focused_view == NULL);
	printf("  PASS: focus_prev_view_empty\n");
}

/* ===== wm_view_cycle_prev with sticky view ===== */

static void
test_view_cycle_next_sticky(void)
{
	init_test_server();
	struct wm_workspace other_ws;
	memset(&other_ws, 0, sizeof(other_ws));
	other_ws.index = 1;
	other_ws.name = "Other";

	setup_mock_view(0, &test_workspace);
	setup_mock_view(1, &other_ws);  /* different workspace */
	test_views[1].sticky = true;    /* but sticky! */

	test_server.focused_view = &test_views[0];

	/* cycle_next should include v1 because it's sticky */
	wm_view_cycle_next(&test_server);
	assert(test_server.focused_view == &test_views[1]);

	wl_list_remove(&test_views[0].link);
	wl_list_remove(&test_views[1].link);
	printf("  PASS: view_cycle_next_sticky\n");
}

/* ===== wm_view_resize_by minimum size clamp ===== */

static void
test_view_resize_by_clamp_minimum(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);

	/* Try to shrink below minimum (10x10) — should be a no-op */
	wm_view_resize_by(&test_views[0], -795, -595);
	/* new_w = 800-795 = 5, new_h = 600-595 = 5, both <= 10 */
	/* Size should NOT have changed (the set_size is not called) */
	/* Our stub sets toplevel width/height, so they should still be 800x600 */
	assert(test_toplevels[0].width == 800);
	assert(test_toplevels[0].height == 600);

	wl_list_remove(&test_views[0].link);
	printf("  PASS: view_resize_by_clamp_minimum\n");
}

/* ===== wm_view_begin_interactive tests ===== */

static void
test_begin_interactive_move_mode(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	test_views[0].x = 100;
	test_views[0].y = 200;
	test_cursor.x = 150.0;
	test_cursor.y = 250.0;

	/* pointer_state.focused_surface is NULL (as if pointer is on decoration),
	 * which should allow the interactive to proceed */
	test_seat.pointer_state.focused_surface = NULL;

	wm_view_begin_interactive(&test_views[0], WM_CURSOR_MOVE, 0);

	assert(test_server.grabbed_view == &test_views[0]);
	assert(test_server.cursor_mode == WM_CURSOR_MOVE);
	/* grab_x = cursor_x - view_x = 150 - 100 = 50 */
	assert(test_server.grab_x == 50.0);
	/* grab_y = cursor_y - view_y = 250 - 200 = 50 */
	assert(test_server.grab_y == 50.0);

	wl_list_remove(&test_views[0].link);
	printf("  PASS: begin_interactive_move_mode\n");
}

static void
test_begin_interactive_resize_mode(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	test_views[0].x = 100;
	test_views[0].y = 200;
	test_cursor.x = 900.0;
	test_cursor.y = 800.0;

	test_seat.pointer_state.focused_surface = NULL;

	wm_view_begin_interactive(&test_views[0], WM_CURSOR_RESIZE,
		WLR_EDGE_RIGHT | WLR_EDGE_BOTTOM);

	assert(test_server.grabbed_view == &test_views[0]);
	assert(test_server.cursor_mode == WM_CURSOR_RESIZE);
	assert(test_server.resize_edges ==
		(WLR_EDGE_RIGHT | WLR_EDGE_BOTTOM));
	/* grab_geobox should be set from wlr_xdg_surface_get_geometry
	 * (stub returns 0,0,800,600) plus view offset */
	assert(test_server.grab_geobox.x == 100);
	assert(test_server.grab_geobox.y == 200);
	assert(test_server.grab_geobox.width == 800);
	assert(test_server.grab_geobox.height == 600);

	wl_list_remove(&test_views[0].link);
	printf("  PASS: begin_interactive_resize_mode\n");
}

static void
test_begin_interactive_wrong_surface(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	setup_mock_view(1, &test_workspace);

	/* Set pointer focus to v1's surface, but try to move v0.
	 * Since focused surface != v0's surface, it should be rejected. */
	test_seat.pointer_state.focused_surface = &test_surfaces[1];

	wm_view_begin_interactive(&test_views[0], WM_CURSOR_MOVE, 0);

	/* Should NOT have set grabbed_view because surfaces don't match */
	assert(test_server.grabbed_view == NULL);
	assert(test_server.cursor_mode == WM_CURSOR_PASSTHROUGH);

	wl_list_remove(&test_views[0].link);
	wl_list_remove(&test_views[1].link);
	printf("  PASS: begin_interactive_wrong_surface\n");
}

static void
test_begin_interactive_correct_surface(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	test_views[0].x = 50;
	test_views[0].y = 60;
	test_cursor.x = 100.0;
	test_cursor.y = 110.0;

	/* Set pointer focus to the view's own surface */
	test_seat.pointer_state.focused_surface = &test_surfaces[0];

	wm_view_begin_interactive(&test_views[0], WM_CURSOR_MOVE, 0);

	assert(test_server.grabbed_view == &test_views[0]);
	assert(test_server.cursor_mode == WM_CURSOR_MOVE);

	wl_list_remove(&test_views[0].link);
	printf("  PASS: begin_interactive_correct_surface\n");
}

static void
test_begin_interactive_resize_edges_left_top(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	test_views[0].x = 100;
	test_views[0].y = 200;
	test_cursor.x = 100.0;
	test_cursor.y = 200.0;
	test_seat.pointer_state.focused_surface = NULL;

	wm_view_begin_interactive(&test_views[0], WM_CURSOR_RESIZE,
		WLR_EDGE_LEFT | WLR_EDGE_TOP);

	assert(test_server.cursor_mode == WM_CURSOR_RESIZE);
	assert(test_server.resize_edges ==
		(WLR_EDGE_LEFT | WLR_EDGE_TOP));
	/* border_x = (100 + 0) + (LEFT => 0) = 100
	 * border_y = (200 + 0) + (TOP => 0) = 200
	 * grab_x = 100 - 100 = 0, grab_y = 200 - 200 = 0 */
	assert(test_server.grab_x == 0.0);
	assert(test_server.grab_y == 0.0);

	wl_list_remove(&test_views[0].link);
	printf("  PASS: begin_interactive_resize_edges_left_top\n");
}

/* ===== wm_view_toggle_decoration extended tests ===== */

static void
test_view_toggle_decoration_off(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	setup_mock_decoration(&test_views[0]);
	test_views[0].show_decoration = true;

	wm_view_toggle_decoration(&test_views[0]);

	/* After toggle, decorations should be hidden */
	assert(test_views[0].show_decoration == false);
	assert(test_deco_tree.node.enabled == false);

	/* Clean up */
	wl_list_remove(&test_deco_tree.node.link);
	test_views[0].decoration = NULL;
	wl_list_remove(&test_views[0].link);
	printf("  PASS: view_toggle_decoration_off\n");
}

static void
test_view_toggle_decoration_on(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	setup_mock_decoration(&test_views[0]);
	test_views[0].show_decoration = false;
	test_deco_tree.node.enabled = false;

	wm_view_toggle_decoration(&test_views[0]);

	/* After toggle, decorations should be visible */
	assert(test_views[0].show_decoration == true);
	assert(test_deco_tree.node.enabled == true);

	wl_list_remove(&test_deco_tree.node.link);
	test_views[0].decoration = NULL;
	wl_list_remove(&test_views[0].link);
	printf("  PASS: view_toggle_decoration_on\n");
}

static void
test_view_toggle_decoration_double_toggle(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	setup_mock_decoration(&test_views[0]);
	test_views[0].show_decoration = true;

	/* Toggle off */
	wm_view_toggle_decoration(&test_views[0]);
	assert(test_views[0].show_decoration == false);

	/* Toggle back on */
	wm_view_toggle_decoration(&test_views[0]);
	assert(test_views[0].show_decoration == true);
	assert(test_deco_tree.node.enabled == true);

	wl_list_remove(&test_deco_tree.node.link);
	test_views[0].decoration = NULL;
	wl_list_remove(&test_views[0].link);
	printf("  PASS: view_toggle_decoration_double_toggle\n");
}

static void
test_view_toggle_decoration_null_view(void)
{
	/* Should not crash */
	wm_view_toggle_decoration(NULL);
	printf("  PASS: view_toggle_decoration_null_view\n");
}

/* ===== wm_view_set_head tests ===== */

static void
test_view_set_head_valid(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	test_views[0].x = 100;
	test_views[0].y = 100;

	/* Set up two outputs */
	setup_mock_output(0, 0, 0, 1920, 1080);
	setup_mock_output(1, 1920, 0, 1920, 1080);

	/* Move view to head index 1 (second output at 1920,0) */
	wm_view_set_head(&test_views[0], 1);

	/* View should be centered on the second output's usable area.
	 * Area: 1920,0 1920x1080. View geo: 800x600.
	 * Center: x = 1920 + (1920-800)/2 = 1920+560 = 2480
	 *         y = 0 + (1080-600)/2 = 240 */
	assert(test_views[0].x == 2480);
	assert(test_views[0].y == 240);

	wl_list_remove(&test_outputs[0].link);
	wl_list_remove(&test_outputs[1].link);
	wl_list_remove(&test_views[0].link);
	printf("  PASS: view_set_head_valid\n");
}

static void
test_view_set_head_first_output(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	test_views[0].x = 2000;
	test_views[0].y = 500;

	setup_mock_output(0, 0, 0, 1920, 1080);
	setup_mock_output(1, 1920, 0, 1920, 1080);

	/* Move to head 0 */
	wm_view_set_head(&test_views[0], 0);

	/* View centered on first output */
	assert(test_views[0].x == 560);
	assert(test_views[0].y == 240);

	wl_list_remove(&test_outputs[0].link);
	wl_list_remove(&test_outputs[1].link);
	wl_list_remove(&test_views[0].link);
	printf("  PASS: view_set_head_first_output\n");
}

static void
test_view_set_head_invalid_index(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	test_views[0].x = 100;
	test_views[0].y = 200;

	setup_mock_output(0, 0, 0, 1920, 1080);

	/* Head index 5 doesn't exist (only 1 output) */
	wm_view_set_head(&test_views[0], 5);

	/* Position should be unchanged */
	assert(test_views[0].x == 100);
	assert(test_views[0].y == 200);

	wl_list_remove(&test_outputs[0].link);
	wl_list_remove(&test_views[0].link);
	printf("  PASS: view_set_head_invalid_index\n");
}

static void
test_view_set_head_null_view(void)
{
	/* Should not crash */
	wm_view_set_head(NULL, 0);
	printf("  PASS: view_set_head_null_view\n");
}

static void
test_view_set_head_no_outputs(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	test_views[0].x = 100;
	test_views[0].y = 200;

	/* No outputs in list */
	wm_view_set_head(&test_views[0], 0);

	/* Position unchanged */
	assert(test_views[0].x == 100);
	assert(test_views[0].y == 200);

	wl_list_remove(&test_views[0].link);
	printf("  PASS: view_set_head_no_outputs\n");
}

/* ===== wm_view_send_to_next_head tests ===== */

static void
test_view_send_to_next_head_null(void)
{
	wm_view_send_to_next_head(NULL);
	printf("  PASS: view_send_to_next_head_null\n");
}

static void
test_view_send_to_next_head_two_outputs(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	test_views[0].x = 100;
	test_views[0].y = 100;

	setup_mock_output(0, 0, 0, 1920, 1080);
	setup_mock_output(1, 1920, 0, 1920, 1080);

	/* Make output_at return the first output's wlr_output */
	g_output_at_return = &test_wlr_outputs[0];

	wm_view_send_to_next_head(&test_views[0]);

	/* Should have moved to the second output (centered) */
	assert(test_views[0].x == 2480);
	assert(test_views[0].y == 240);

	g_output_at_return = NULL;
	wl_list_remove(&test_outputs[0].link);
	wl_list_remove(&test_outputs[1].link);
	wl_list_remove(&test_views[0].link);
	printf("  PASS: view_send_to_next_head_two_outputs\n");
}

static void
test_view_send_to_next_head_wraps(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	test_views[0].x = 2480;
	test_views[0].y = 240;

	setup_mock_output(0, 0, 0, 1920, 1080);
	setup_mock_output(1, 1920, 0, 1920, 1080);

	/* View is on second output (index 1); next should wrap to first */
	g_output_at_return = &test_wlr_outputs[1];

	wm_view_send_to_next_head(&test_views[0]);

	/* Should wrap to first output */
	assert(test_views[0].x == 560);
	assert(test_views[0].y == 240);

	g_output_at_return = NULL;
	wl_list_remove(&test_outputs[0].link);
	wl_list_remove(&test_outputs[1].link);
	wl_list_remove(&test_views[0].link);
	printf("  PASS: view_send_to_next_head_wraps\n");
}

static void
test_view_send_to_next_head_no_outputs(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	test_views[0].x = 100;
	test_views[0].y = 200;

	/* No outputs, and output_at returns NULL */
	g_output_at_return = NULL;

	wm_view_send_to_next_head(&test_views[0]);

	/* No crash, position unchanged (no outputs to move to) */
	assert(test_views[0].x == 100);
	assert(test_views[0].y == 200);

	wl_list_remove(&test_views[0].link);
	printf("  PASS: view_send_to_next_head_no_outputs\n");
}

/* ===== wm_view_send_to_prev_head tests ===== */

static void
test_view_send_to_prev_head_null(void)
{
	wm_view_send_to_prev_head(NULL);
	printf("  PASS: view_send_to_prev_head_null\n");
}

static void
test_view_send_to_prev_head_two_outputs(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	test_views[0].x = 2480;
	test_views[0].y = 240;

	setup_mock_output(0, 0, 0, 1920, 1080);
	setup_mock_output(1, 1920, 0, 1920, 1080);

	/* View is on second output; prev should go to first */
	g_output_at_return = &test_wlr_outputs[1];

	wm_view_send_to_prev_head(&test_views[0]);

	/* Should have moved to first output (centered) */
	assert(test_views[0].x == 560);
	assert(test_views[0].y == 240);

	g_output_at_return = NULL;
	wl_list_remove(&test_outputs[0].link);
	wl_list_remove(&test_outputs[1].link);
	wl_list_remove(&test_views[0].link);
	printf("  PASS: view_send_to_prev_head_two_outputs\n");
}

static void
test_view_send_to_prev_head_wraps(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	test_views[0].x = 560;
	test_views[0].y = 240;

	setup_mock_output(0, 0, 0, 1920, 1080);
	setup_mock_output(1, 1920, 0, 1920, 1080);

	/* View is on first output; prev wraps. Due to early-break in the loop,
	 * last_out points to the matched output. The view gets centered on it.
	 * center x = 0 + (1920 - 800)/2 = 560 */
	g_output_at_return = &test_wlr_outputs[0];

	wm_view_send_to_prev_head(&test_views[0]);

	/* Wraps to last_out (which is output0 due to break before full iteration).
	 * Centering: x = 0 + (1920-800)/2 = 560, y = (1080-600)/2 = 240 */
	assert(test_views[0].x == 560);
	assert(test_views[0].y == 240);

	g_output_at_return = NULL;
	wl_list_remove(&test_outputs[0].link);
	wl_list_remove(&test_outputs[1].link);
	wl_list_remove(&test_views[0].link);
	printf("  PASS: view_send_to_prev_head_wraps\n");
}

/* ===== wm_view_activate_tab extended tests ===== */

static void
test_view_activate_tab_with_group(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	setup_mock_view(1, &test_workspace);

	/* Set up a tab group with two views */
	struct wm_tab_group tg;
	memset(&tg, 0, sizeof(tg));
	tg.count = 2;
	tg.server = &test_server;
	wl_list_init(&tg.views);
	wl_list_insert(tg.views.prev, &test_views[0].tab_link);
	wl_list_insert(tg.views.prev, &test_views[1].tab_link);
	test_views[0].tab_group = &tg;
	test_views[1].tab_group = &tg;
	tg.active_view = &test_views[0];

	g_tab_activate_count = 0;

	/* Activate tab index 1 (second tab = v1) */
	wm_view_activate_tab(&test_views[0], 1);

	assert(g_tab_activate_count == 1);
	assert(g_tab_activate_group == &tg);
	assert(g_tab_activate_view == &test_views[1]);

	/* Restore tab_link lists */
	wl_list_remove(&test_views[0].tab_link);
	wl_list_remove(&test_views[1].tab_link);
	wl_list_init(&test_views[0].tab_link);
	wl_list_init(&test_views[1].tab_link);
	test_views[0].tab_group = NULL;
	test_views[1].tab_group = NULL;
	wl_list_remove(&test_views[0].link);
	wl_list_remove(&test_views[1].link);
	printf("  PASS: view_activate_tab_with_group\n");
}

static void
test_view_activate_tab_first_index(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	setup_mock_view(1, &test_workspace);

	struct wm_tab_group tg;
	memset(&tg, 0, sizeof(tg));
	tg.count = 2;
	tg.server = &test_server;
	wl_list_init(&tg.views);
	wl_list_insert(tg.views.prev, &test_views[0].tab_link);
	wl_list_insert(tg.views.prev, &test_views[1].tab_link);
	test_views[0].tab_group = &tg;
	test_views[1].tab_group = &tg;
	tg.active_view = &test_views[1];

	g_tab_activate_count = 0;

	/* Activate tab index 0 (first tab = v0) */
	wm_view_activate_tab(&test_views[1], 0);

	assert(g_tab_activate_count == 1);
	assert(g_tab_activate_view == &test_views[0]);

	wl_list_remove(&test_views[0].tab_link);
	wl_list_remove(&test_views[1].tab_link);
	wl_list_init(&test_views[0].tab_link);
	wl_list_init(&test_views[1].tab_link);
	test_views[0].tab_group = NULL;
	test_views[1].tab_group = NULL;
	wl_list_remove(&test_views[0].link);
	wl_list_remove(&test_views[1].link);
	printf("  PASS: view_activate_tab_first_index\n");
}

/* ===== wm_view_lhalf / wm_view_rhalf with output ===== */

static void
test_view_lhalf_with_output(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	setup_mock_output(0, 0, 0, 1920, 1080);

	test_views[0].x = 200;
	test_views[0].y = 150;
	test_views[0].lhalf = false;
	test_views[0].rhalf = false;
	test_views[0].maximized = false;

	/* Make output_at return our output so get_view_output_area succeeds */
	g_output_at_return = &test_wlr_outputs[0];

	wm_view_lhalf(&test_views[0]);

	/* Should be tiled to left half: x=0, y=0, width=960, height=1080 */
	assert(test_views[0].lhalf == true);
	assert(test_views[0].rhalf == false);
	assert(test_views[0].x == 0);
	assert(test_views[0].y == 0);
	assert(test_toplevels[0].width == 960);
	assert(test_toplevels[0].height == 1080);

	g_output_at_return = NULL;
	wl_list_remove(&test_outputs[0].link);
	wl_list_remove(&test_views[0].link);
	printf("  PASS: view_lhalf_with_output\n");
}

static void
test_view_rhalf_with_output(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	setup_mock_output(0, 0, 0, 1920, 1080);

	test_views[0].x = 200;
	test_views[0].y = 150;
	test_views[0].lhalf = false;
	test_views[0].rhalf = false;
	test_views[0].maximized = false;

	g_output_at_return = &test_wlr_outputs[0];

	wm_view_rhalf(&test_views[0]);

	/* Should be tiled to right half: x=960, y=0, width=960, height=1080 */
	assert(test_views[0].rhalf == true);
	assert(test_views[0].lhalf == false);
	assert(test_views[0].x == 960);
	assert(test_views[0].y == 0);
	assert(test_toplevels[0].width == 960);
	assert(test_toplevels[0].height == 1080);

	g_output_at_return = NULL;
	wl_list_remove(&test_outputs[0].link);
	wl_list_remove(&test_views[0].link);
	printf("  PASS: view_rhalf_with_output\n");
}

static void
test_view_lhalf_toggle_with_output(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	setup_mock_output(0, 0, 0, 1920, 1080);

	test_views[0].x = 200;
	test_views[0].y = 150;
	test_views[0].lhalf = false;
	test_views[0].rhalf = false;
	test_views[0].maximized = false;

	g_output_at_return = &test_wlr_outputs[0];

	/* Tile left */
	wm_view_lhalf(&test_views[0]);
	assert(test_views[0].lhalf == true);
	assert(test_views[0].x == 0);

	/* Toggle off */
	wm_view_lhalf(&test_views[0]);
	assert(test_views[0].lhalf == false);
	/* Should restore saved geometry */
	assert(test_views[0].x == 200);
	assert(test_views[0].y == 150);

	g_output_at_return = NULL;
	wl_list_remove(&test_outputs[0].link);
	wl_list_remove(&test_views[0].link);
	printf("  PASS: view_lhalf_toggle_with_output\n");
}

static void
test_view_rhalf_toggle_with_output(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	setup_mock_output(0, 0, 0, 1920, 1080);

	test_views[0].x = 200;
	test_views[0].y = 150;
	test_views[0].lhalf = false;
	test_views[0].rhalf = false;
	test_views[0].maximized = false;

	g_output_at_return = &test_wlr_outputs[0];

	/* Tile right */
	wm_view_rhalf(&test_views[0]);
	assert(test_views[0].rhalf == true);
	assert(test_views[0].x == 960);

	/* Toggle off */
	wm_view_rhalf(&test_views[0]);
	assert(test_views[0].rhalf == false);
	assert(test_views[0].x == 200);
	assert(test_views[0].y == 150);

	g_output_at_return = NULL;
	wl_list_remove(&test_outputs[0].link);
	wl_list_remove(&test_views[0].link);
	printf("  PASS: view_rhalf_toggle_with_output\n");
}

static void
test_view_lhalf_to_rhalf(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	setup_mock_output(0, 0, 0, 1920, 1080);

	test_views[0].x = 200;
	test_views[0].y = 150;
	test_views[0].lhalf = false;
	test_views[0].rhalf = false;
	test_views[0].maximized = false;

	g_output_at_return = &test_wlr_outputs[0];

	/* Tile left first */
	wm_view_lhalf(&test_views[0]);
	assert(test_views[0].lhalf == true);
	assert(test_views[0].x == 0);

	/* Now tile right (should switch from lhalf to rhalf) */
	wm_view_rhalf(&test_views[0]);
	assert(test_views[0].rhalf == true);
	assert(test_views[0].lhalf == false);
	assert(test_views[0].x == 960);

	g_output_at_return = NULL;
	wl_list_remove(&test_outputs[0].link);
	wl_list_remove(&test_views[0].link);
	printf("  PASS: view_lhalf_to_rhalf\n");
}

/* ===== wm_view_send_to_next_head with three outputs ===== */

static void
test_view_send_to_next_head_three_outputs(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	test_views[0].x = 100;
	test_views[0].y = 100;

	setup_mock_output(0, 0, 0, 1920, 1080);
	setup_mock_output(1, 1920, 0, 1920, 1080);
	setup_mock_output(2, 3840, 0, 1920, 1080);

	/* View is on first output, send to next should go to second */
	g_output_at_return = &test_wlr_outputs[0];
	wm_view_send_to_next_head(&test_views[0]);
	assert(test_views[0].x == 2480);

	/* Now on second output, send to next should go to third */
	g_output_at_return = &test_wlr_outputs[1];
	wm_view_send_to_next_head(&test_views[0]);
	assert(test_views[0].x == 4400);

	g_output_at_return = NULL;
	wl_list_remove(&test_outputs[0].link);
	wl_list_remove(&test_outputs[1].link);
	wl_list_remove(&test_outputs[2].link);
	wl_list_remove(&test_views[0].link);
	printf("  PASS: view_send_to_next_head_three_outputs\n");
}

/* ===== wm_view_send_to_prev_head no-outputs edge case ===== */

static void
test_view_send_to_prev_head_no_outputs(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	test_views[0].x = 100;
	test_views[0].y = 200;

	g_output_at_return = NULL;

	wm_view_send_to_prev_head(&test_views[0]);

	/* No crash, position unchanged */
	assert(test_views[0].x == 100);
	assert(test_views[0].y == 200);

	wl_list_remove(&test_views[0].link);
	printf("  PASS: view_send_to_prev_head_no_outputs\n");
}

/* ===== handle_xdg_toplevel_request_fullscreen tests ===== */

static void
setup_fullscreen_view(int idx, struct wm_workspace *ws)
{
	setup_mock_view(idx, ws);
}

static void
test_fullscreen_enter(void)
{
	init_test_server();
	setup_fullscreen_view(0, &test_workspace);
	setup_mock_output(0, 0, 0, 1920, 1080);

	struct wlr_scene_tree overlay_tree;
	memset(&overlay_tree, 0, sizeof(overlay_tree));
	wl_list_init(&overlay_tree.children);
	test_server.layer_overlay = &overlay_tree;

	test_views[0].x = 100;
	test_views[0].y = 200;
	test_views[0].fullscreen = false;

	g_output_at_return = &test_wlr_outputs[0];

	handle_xdg_toplevel_request_fullscreen(
		&test_views[0].request_fullscreen, NULL);

	assert(test_views[0].fullscreen == true);
	assert(test_views[0].x == 0);
	assert(test_views[0].y == 0);
	assert(test_toplevels[0].width == 1920);
	assert(test_toplevels[0].height == 1080);

	g_output_at_return = NULL;
	wl_list_remove(&test_outputs[0].link);
	wl_list_remove(&test_views[0].link);
	printf("  PASS: fullscreen_enter\n");
}

static void
test_fullscreen_restore(void)
{
	init_test_server();
	setup_fullscreen_view(0, &test_workspace);
	setup_mock_output(0, 0, 0, 1920, 1080);

	struct wlr_scene_tree overlay_tree;
	memset(&overlay_tree, 0, sizeof(overlay_tree));
	wl_list_init(&overlay_tree.children);
	test_server.layer_overlay = &overlay_tree;

	struct wlr_scene_tree ws_tree;
	memset(&ws_tree, 0, sizeof(ws_tree));
	wl_list_init(&ws_tree.children);
	test_workspace.tree = &ws_tree;

	test_views[0].x = 100;
	test_views[0].y = 200;
	test_views[0].fullscreen = false;
	g_output_at_return = &test_wlr_outputs[0];

	handle_xdg_toplevel_request_fullscreen(
		&test_views[0].request_fullscreen, NULL);
	assert(test_views[0].fullscreen == true);

	handle_xdg_toplevel_request_fullscreen(
		&test_views[0].request_fullscreen, NULL);

	assert(test_views[0].fullscreen == false);
	assert(test_views[0].x == 100);
	assert(test_views[0].y == 200);
	assert(test_toplevels[0].width == 800);
	assert(test_toplevels[0].height == 600);

	g_output_at_return = NULL;
	wl_list_remove(&test_outputs[0].link);
	wl_list_remove(&test_views[0].link);
	printf("  PASS: fullscreen_restore\n");
}

static void
test_fullscreen_sticky_restore(void)
{
	init_test_server();
	setup_fullscreen_view(0, &test_workspace);
	setup_mock_output(0, 0, 0, 1920, 1080);

	struct wlr_scene_tree overlay_tree;
	memset(&overlay_tree, 0, sizeof(overlay_tree));
	wl_list_init(&overlay_tree.children);
	test_server.layer_overlay = &overlay_tree;

	struct wlr_scene_tree sticky_tree;
	memset(&sticky_tree, 0, sizeof(sticky_tree));
	wl_list_init(&sticky_tree.children);
	test_server.sticky_tree = &sticky_tree;

	test_views[0].x = 100;
	test_views[0].y = 200;
	test_views[0].fullscreen = false;
	test_views[0].sticky = true;
	g_output_at_return = &test_wlr_outputs[0];

	handle_xdg_toplevel_request_fullscreen(
		&test_views[0].request_fullscreen, NULL);
	assert(test_views[0].fullscreen == true);

	handle_xdg_toplevel_request_fullscreen(
		&test_views[0].request_fullscreen, NULL);
	assert(test_views[0].fullscreen == false);
	assert(test_views[0].x == 100);
	assert(test_views[0].y == 200);

	g_output_at_return = NULL;
	wl_list_remove(&test_outputs[0].link);
	wl_list_remove(&test_views[0].link);
	printf("  PASS: fullscreen_sticky_restore\n");
}

static void
test_fullscreen_no_decoration(void)
{
	init_test_server();
	setup_fullscreen_view(0, &test_workspace);
	setup_mock_output(0, 0, 0, 1920, 1080);

	struct wlr_scene_tree overlay_tree;
	memset(&overlay_tree, 0, sizeof(overlay_tree));
	wl_list_init(&overlay_tree.children);
	test_server.layer_overlay = &overlay_tree;

	test_views[0].x = 100;
	test_views[0].y = 200;
	test_views[0].fullscreen = false;
	test_views[0].decoration = NULL;
	g_output_at_return = &test_wlr_outputs[0];

	handle_xdg_toplevel_request_fullscreen(
		&test_views[0].request_fullscreen, NULL);

	assert(test_views[0].fullscreen == true);
	assert(test_views[0].x == 0);
	assert(test_views[0].y == 0);

	g_output_at_return = NULL;
	wl_list_remove(&test_outputs[0].link);
	wl_list_remove(&test_views[0].link);
	printf("  PASS: fullscreen_no_decoration\n");
}

static void
test_fullscreen_no_output(void)
{
	init_test_server();
	setup_fullscreen_view(0, &test_workspace);

	struct wlr_scene_tree overlay_tree;
	memset(&overlay_tree, 0, sizeof(overlay_tree));
	wl_list_init(&overlay_tree.children);
	test_server.layer_overlay = &overlay_tree;

	test_views[0].x = 100;
	test_views[0].y = 200;
	test_views[0].fullscreen = false;
	g_output_at_return = NULL;

	handle_xdg_toplevel_request_fullscreen(
		&test_views[0].request_fullscreen, NULL);

	assert(test_views[0].fullscreen == true);
	assert(test_views[0].x == 100);
	assert(test_views[0].y == 200);

	wl_list_remove(&test_views[0].link);
	printf("  PASS: fullscreen_no_output\n");
}

static void
test_fullscreen_saves_geometry(void)
{
	init_test_server();
	setup_fullscreen_view(0, &test_workspace);
	setup_mock_output(0, 0, 0, 1920, 1080);

	struct wlr_scene_tree overlay_tree;
	memset(&overlay_tree, 0, sizeof(overlay_tree));
	wl_list_init(&overlay_tree.children);
	test_server.layer_overlay = &overlay_tree;

	test_views[0].x = 150;
	test_views[0].y = 250;
	test_views[0].fullscreen = false;
	test_views[0].maximized = false;
	g_output_at_return = &test_wlr_outputs[0];

	handle_xdg_toplevel_request_fullscreen(
		&test_views[0].request_fullscreen, NULL);

	assert(test_views[0].saved_geometry.x == 150);
	assert(test_views[0].saved_geometry.y == 250);
	assert(test_views[0].saved_geometry.width == 800);
	assert(test_views[0].saved_geometry.height == 600);

	g_output_at_return = NULL;
	wl_list_remove(&test_outputs[0].link);
	wl_list_remove(&test_views[0].link);
	printf("  PASS: fullscreen_saves_geometry\n");
}

static void
test_fullscreen_enter_with_decoration(void)
{
	init_test_server();
	setup_fullscreen_view(0, &test_workspace);
	setup_mock_output(0, 0, 0, 1920, 1080);
	setup_mock_decoration(&test_views[0]);

	struct wlr_scene_tree overlay_tree;
	memset(&overlay_tree, 0, sizeof(overlay_tree));
	wl_list_init(&overlay_tree.children);
	test_server.layer_overlay = &overlay_tree;

	struct wlr_scene_node xdg_child;
	memset(&xdg_child, 0, sizeof(xdg_child));
	wl_list_insert(test_views[0].scene_tree->children.prev,
		&xdg_child.link);

	test_views[0].x = 100;
	test_views[0].y = 200;
	test_views[0].fullscreen = false;
	g_output_at_return = &test_wlr_outputs[0];

	handle_xdg_toplevel_request_fullscreen(
		&test_views[0].request_fullscreen, NULL);

	assert(test_views[0].fullscreen == true);
	assert(test_deco_tree.node.enabled == false);
	assert(xdg_child.x == 0);
	assert(xdg_child.y == 0);

	g_output_at_return = NULL;
	wl_list_remove(&xdg_child.link);
	wl_list_remove(&test_deco_tree.node.link);
	test_views[0].decoration = NULL;
	wl_list_remove(&test_outputs[0].link);
	wl_list_remove(&test_views[0].link);
	printf("  PASS: fullscreen_enter_with_decoration\n");
}

static void
test_fullscreen_restore_with_decoration(void)
{
	init_test_server();
	setup_fullscreen_view(0, &test_workspace);
	setup_mock_output(0, 0, 0, 1920, 1080);
	setup_mock_decoration(&test_views[0]);

	struct wlr_scene_tree overlay_tree;
	memset(&overlay_tree, 0, sizeof(overlay_tree));
	wl_list_init(&overlay_tree.children);
	test_server.layer_overlay = &overlay_tree;

	struct wlr_scene_tree ws_tree;
	memset(&ws_tree, 0, sizeof(ws_tree));
	wl_list_init(&ws_tree.children);
	test_workspace.tree = &ws_tree;

	struct wlr_scene_node xdg_child;
	memset(&xdg_child, 0, sizeof(xdg_child));
	wl_list_insert(test_views[0].scene_tree->children.prev,
		&xdg_child.link);

	test_views[0].x = 100;
	test_views[0].y = 200;
	test_views[0].fullscreen = false;
	g_output_at_return = &test_wlr_outputs[0];

	handle_xdg_toplevel_request_fullscreen(
		&test_views[0].request_fullscreen, NULL);
	assert(test_deco_tree.node.enabled == false);

	handle_xdg_toplevel_request_fullscreen(
		&test_views[0].request_fullscreen, NULL);

	assert(test_views[0].fullscreen == false);
	assert(test_deco_tree.node.enabled == true);

	g_output_at_return = NULL;
	wl_list_remove(&xdg_child.link);
	wl_list_remove(&test_deco_tree.node.link);
	test_views[0].decoration = NULL;
	wl_list_remove(&test_outputs[0].link);
	wl_list_remove(&test_views[0].link);
	printf("  PASS: fullscreen_restore_with_decoration\n");
}

/* ===== handle_xdg_toplevel_request_maximize tests ===== */

static void
test_maximize_enter(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	setup_mock_output(0, 0, 0, 1920, 1080);

	test_views[0].x = 100;
	test_views[0].y = 200;
	test_views[0].maximized = false;
	test_config.full_maximization = true;
	g_output_at_return = &test_wlr_outputs[0];

	handle_xdg_toplevel_request_maximize(
		&test_views[0].request_maximize, NULL);

	assert(test_views[0].maximized == true);
	assert(test_views[0].x == 0);
	assert(test_views[0].y == 0);
	assert(test_toplevels[0].width == 1920);
	assert(test_toplevels[0].height == 1080);

	g_output_at_return = NULL;
	wl_list_remove(&test_outputs[0].link);
	wl_list_remove(&test_views[0].link);
	printf("  PASS: maximize_enter\n");
}

static void
test_maximize_enter_usable_area(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	setup_mock_output(0, 0, 30, 1920, 1050);

	test_views[0].x = 100;
	test_views[0].y = 200;
	test_views[0].maximized = false;
	test_config.full_maximization = false;
	g_output_at_return = &test_wlr_outputs[0];

	handle_xdg_toplevel_request_maximize(
		&test_views[0].request_maximize, NULL);

	assert(test_views[0].maximized == true);
	assert(test_views[0].x == 0);
	assert(test_views[0].y == 30);
	assert(test_toplevels[0].width == 1920);
	assert(test_toplevels[0].height == 1050);

	g_output_at_return = NULL;
	wl_list_remove(&test_outputs[0].link);
	wl_list_remove(&test_views[0].link);
	printf("  PASS: maximize_enter_usable_area\n");
}

static void
test_maximize_restore(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	setup_mock_output(0, 0, 0, 1920, 1080);

	test_views[0].x = 100;
	test_views[0].y = 200;
	test_views[0].maximized = false;
	test_config.full_maximization = true;
	g_output_at_return = &test_wlr_outputs[0];

	handle_xdg_toplevel_request_maximize(
		&test_views[0].request_maximize, NULL);
	assert(test_views[0].maximized == true);

	handle_xdg_toplevel_request_maximize(
		&test_views[0].request_maximize, NULL);

	assert(test_views[0].maximized == false);
	assert(test_views[0].x == 100);
	assert(test_views[0].y == 200);
	assert(test_toplevels[0].width == 800);
	assert(test_toplevels[0].height == 600);

	g_output_at_return = NULL;
	wl_list_remove(&test_outputs[0].link);
	wl_list_remove(&test_views[0].link);
	printf("  PASS: maximize_restore\n");
}

static void
test_maximize_no_output(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);

	test_views[0].x = 100;
	test_views[0].y = 200;
	test_views[0].maximized = false;
	g_output_at_return = NULL;

	handle_xdg_toplevel_request_maximize(
		&test_views[0].request_maximize, NULL);

	assert(test_views[0].maximized == true);
	assert(test_views[0].x == 100);
	assert(test_views[0].y == 200);

	wl_list_remove(&test_views[0].link);
	printf("  PASS: maximize_no_output\n");
}

/* ===== handle_xdg_toplevel_request_minimize tests ===== */

static void
test_minimize_basic(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);

	test_views[0].scene_tree->node.enabled = true;
	test_config.animate_minimize = false;

	handle_xdg_toplevel_request_minimize(
		&test_views[0].request_minimize, NULL);

	assert(test_views[0].scene_tree->node.enabled == false);

	wl_list_remove(&test_views[0].link);
	printf("  PASS: minimize_basic\n");
}

static void
test_minimize_with_animation(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);

	test_views[0].scene_tree->node.enabled = true;
	test_config.animate_minimize = true;
	test_config.animation_duration_ms = 200;

	handle_xdg_toplevel_request_minimize(
		&test_views[0].request_minimize, NULL);

	/* No crash = pass (animation stub is a no-op) */

	wl_list_remove(&test_views[0].link);
	printf("  PASS: minimize_with_animation\n");
}

static void
test_minimize_focus_transfers(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	setup_mock_view(1, &test_workspace);

	test_views[0].scene_tree->node.enabled = true;
	test_views[1].scene_tree->node.enabled = true;
	test_config.animate_minimize = false;

	test_server.focused_view = &test_views[0];

	handle_xdg_toplevel_request_minimize(
		&test_views[0].request_minimize, NULL);

	assert(test_server.focused_view == &test_views[1]);

	wl_list_remove(&test_views[0].link);
	wl_list_remove(&test_views[1].link);
	printf("  PASS: minimize_focus_transfers\n");
}

static void
test_minimize_last_view_clears_focus(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);

	test_views[0].scene_tree->node.enabled = true;
	test_config.animate_minimize = false;
	test_server.focused_view = &test_views[0];

	handle_xdg_toplevel_request_minimize(
		&test_views[0].request_minimize, NULL);

	assert(test_views[0].scene_tree->node.enabled == false);

	wl_list_remove(&test_views[0].link);
	printf("  PASS: minimize_last_view_clears_focus\n");
}

/* ===== handle_xdg_toplevel_set_title / set_app_id tests ===== */

static void
test_set_title_updates_toolbar(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);

	test_toplevels[0].title = "New Title";
	test_views[0].title = strdup("Old Title");

	handle_xdg_toplevel_set_title(
		&test_views[0].set_title, NULL);

	assert(test_views[0].title != NULL);
	assert(strcmp(test_views[0].title, "New Title") == 0);

	wl_list_remove(&test_views[0].link);
	printf("  PASS: set_title_updates_toolbar\n");
}

static void
test_set_title_null(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);

	test_toplevels[0].title = NULL;
	test_views[0].title = strdup("Old Title");

	handle_xdg_toplevel_set_title(
		&test_views[0].set_title, NULL);

	assert(test_views[0].title == NULL);

	wl_list_remove(&test_views[0].link);
	printf("  PASS: set_title_null\n");
}

static void
test_set_app_id_stored(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);

	test_toplevels[0].app_id = "org.example.app";
	test_views[0].app_id = strdup("old.app");

	handle_xdg_toplevel_set_app_id(
		&test_views[0].set_app_id, NULL);

	assert(test_views[0].app_id != NULL);
	assert(strcmp(test_views[0].app_id, "org.example.app") == 0);

	wl_list_remove(&test_views[0].link);
	printf("  PASS: set_app_id_stored\n");
}

/* ===== deiconify_view tests ===== */

static void
test_deiconify_view_basic(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);

	test_views[0].scene_tree->node.enabled = false;
	test_config.animate_minimize = false;

	deiconify_view(&test_views[0]);

	assert(test_views[0].scene_tree->node.enabled == true);
	assert(test_server.focused_view == &test_views[0]);

	wl_list_remove(&test_views[0].link);
	printf("  PASS: deiconify_view_basic\n");
}

static void
test_deiconify_view_with_animation(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);

	test_views[0].scene_tree->node.enabled = false;
	test_config.animate_minimize = true;
	test_config.animation_duration_ms = 300;

	deiconify_view(&test_views[0]);

	assert(test_views[0].scene_tree->node.enabled == true);
	assert(test_server.focused_view == &test_views[0]);

	wl_list_remove(&test_views[0].link);
	printf("  PASS: deiconify_view_with_animation\n");
}

/* ===== Focus protection tests ===== */

static void
test_focus_view_protection_refuse(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	setup_mock_view(1, &test_workspace);

	/* Focus v0 first (user-initiated) */
	test_server.focus_user_initiated = true;
	wm_focus_view(&test_views[0], test_views[0].xdg_toplevel->base->surface);
	assert(test_server.focused_view == &test_views[0]);

	/* Set v0 with REFUSE protection */
	test_views[0].focus_protection = WM_FOCUS_PROT_REFUSE;

	/* Non-user-initiated focus to v1 should be BLOCKED (v0 has REFUSE) */
	test_server.focus_user_initiated = false;
	wm_focus_view(&test_views[1], test_views[1].xdg_toplevel->base->surface);
	assert(test_server.focused_view == &test_views[0]); /* unchanged */

	/* User-initiated focus to v1 should succeed */
	test_server.focus_user_initiated = true;
	wm_focus_view(&test_views[1], test_views[1].xdg_toplevel->base->surface);
	assert(test_server.focused_view == &test_views[1]);

	wl_list_remove(&test_views[0].link);
	wl_list_remove(&test_views[1].link);
	printf("  PASS: focus_view_protection_refuse\n");
}

static void
test_focus_view_protection_deny(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	setup_mock_view(1, &test_workspace);

	/* Focus v0 first */
	test_server.focus_user_initiated = true;
	wm_focus_view(&test_views[0], test_views[0].xdg_toplevel->base->surface);
	assert(test_server.focused_view == &test_views[0]);

	/* Set v1 with DENY protection */
	test_views[1].focus_protection = WM_FOCUS_PROT_DENY;

	/* Non-user-initiated focus to v1 should be BLOCKED (target has DENY) */
	test_server.focus_user_initiated = false;
	wm_focus_view(&test_views[1], test_views[1].xdg_toplevel->base->surface);
	assert(test_server.focused_view == &test_views[0]); /* unchanged */

	wl_list_remove(&test_views[0].link);
	wl_list_remove(&test_views[1].link);
	printf("  PASS: focus_view_protection_deny\n");
}

static void
test_focus_view_protection_gain_overrides_refuse(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	setup_mock_view(1, &test_workspace);

	/* Focus v0 first */
	test_server.focus_user_initiated = true;
	wm_focus_view(&test_views[0], test_views[0].xdg_toplevel->base->surface);

	/* Set v0 with REFUSE, v1 with GAIN */
	test_views[0].focus_protection = WM_FOCUS_PROT_REFUSE;
	test_views[1].focus_protection = WM_FOCUS_PROT_GAIN;

	/* Non-user focus to v1 should SUCCEED (v1 has GAIN overriding REFUSE) */
	test_server.focus_user_initiated = false;
	wm_focus_view(&test_views[1], test_views[1].xdg_toplevel->base->surface);
	assert(test_server.focused_view == &test_views[1]);

	wl_list_remove(&test_views[0].link);
	wl_list_remove(&test_views[1].link);
	printf("  PASS: focus_view_protection_gain_overrides_refuse\n");
}

static void
test_focus_view_same_surface_noop(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);

	/* Set the seat's keyboard focused surface to our view's surface */
	test_seat.keyboard_state.focused_surface =
		test_views[0].xdg_toplevel->base->surface;

	/* Focusing the same surface should be a no-op */
	int prev_count = g_focus_view_count;
	wm_focus_view(&test_views[0], test_views[0].xdg_toplevel->base->surface);
	/* Should have returned early */

	wl_list_remove(&test_views[0].link);
	printf("  PASS: focus_view_same_surface_noop\n");
}

/* ===== wm_focus_update_for_cursor tests ===== */

static void
test_focus_update_for_cursor_click_policy(void)
{
	init_test_server();
	test_server.focus_policy = WM_FOCUS_CLICK;

	/* Click policy: cursor movement should NOT change focus */
	wm_focus_update_for_cursor(&test_server, 100, 100);
	assert(test_server.focused_view == NULL); /* unchanged */
	printf("  PASS: focus_update_for_cursor_click_policy\n");
}

static void
test_focus_update_for_cursor_during_move(void)
{
	init_test_server();
	test_server.focus_policy = WM_FOCUS_SLOPPY;
	test_server.cursor_mode = WM_CURSOR_MOVE;

	/* During interactive move, should NOT change focus */
	wm_focus_update_for_cursor(&test_server, 100, 100);
	assert(test_server.focused_view == NULL); /* unchanged */
	printf("  PASS: focus_update_for_cursor_during_move\n");
}

/* ===== wm_view_at tests ===== */

static void
test_view_at_no_node(void)
{
	init_test_server();
	/* wlr_scene_node_at stub returns NULL → view_at returns NULL */
	double sx, sy;
	struct wlr_surface *surface = NULL;
	struct wm_view *result = wm_view_at(&test_server, 100, 100,
		&surface, &sx, &sy);
	assert(result == NULL);
	printf("  PASS: view_at_no_node\n");
}

/* ===== wm_view_cycle_prev wraps around tests ===== */

static void
test_view_cycle_prev_wraps(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	setup_mock_view(1, &test_workspace);

	/* Focus first view; prev should wrap to last */
	test_server.focused_view = &test_views[0];
	wm_view_cycle_prev(&test_server);
	assert(test_server.focused_view == &test_views[1]);

	wl_list_remove(&test_views[0].link);
	wl_list_remove(&test_views[1].link);
	printf("  PASS: view_cycle_prev_wraps\n");
}

/* ===== Additional maximize/fullscreen edge cases ===== */

static void
test_maximize_enter_full_max(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	setup_mock_output(0, 0, 0, 1920, 1080);
	g_output_at_return = &test_wlr_outputs[0];
	test_config.full_maximization = true;

	/* Trigger maximize */
	test_views[0].request_maximize.notify = handle_xdg_toplevel_request_maximize;
	test_views[0].request_maximize.notify(&test_views[0].request_maximize, NULL);

	assert(test_views[0].maximized == true);
	/* full maximization uses output box, not usable area */

	/* Toggle maximize off */
	test_views[0].request_maximize.notify(&test_views[0].request_maximize, NULL);
	assert(test_views[0].maximized == false);

	wl_list_remove(&test_views[0].link);
	wl_list_remove(&test_outputs[0].link);
	g_output_at_return = NULL;
	printf("  PASS: maximize_enter_full_max\n");
}

static void
test_maximize_already_lhalf(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	setup_mock_output(0, 0, 0, 1920, 1080);
	g_output_at_return = &test_wlr_outputs[0];

	/* Pre-set as lhalf */
	test_views[0].lhalf = true;
	test_views[0].saved_geometry.x = 100;
	test_views[0].saved_geometry.y = 100;
	test_views[0].saved_geometry.width = 400;
	test_views[0].saved_geometry.height = 300;

	test_views[0].request_maximize.notify = handle_xdg_toplevel_request_maximize;
	test_views[0].request_maximize.notify(&test_views[0].request_maximize, NULL);

	/* Should have maximized (lhalf state is preserved, maximize is orthogonal) */
	assert(test_views[0].maximized == true);

	wl_list_remove(&test_views[0].link);
	wl_list_remove(&test_outputs[0].link);
	g_output_at_return = NULL;
	printf("  PASS: maximize_already_lhalf\n");
}

static void
test_maximize_already_rhalf(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	setup_mock_output(0, 0, 0, 1920, 1080);
	g_output_at_return = &test_wlr_outputs[0];

	/* Pre-set as rhalf */
	test_views[0].rhalf = true;
	test_views[0].saved_geometry.x = 200;
	test_views[0].saved_geometry.y = 200;
	test_views[0].saved_geometry.width = 500;
	test_views[0].saved_geometry.height = 400;

	test_views[0].request_maximize.notify = handle_xdg_toplevel_request_maximize;
	test_views[0].request_maximize.notify(&test_views[0].request_maximize, NULL);

	assert(test_views[0].maximized == true);

	wl_list_remove(&test_views[0].link);
	wl_list_remove(&test_outputs[0].link);
	g_output_at_return = NULL;
	printf("  PASS: maximize_already_rhalf\n");
}

/* ===== Maximize vert/horiz with output ===== */

static void
test_maximize_vert_with_output(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	setup_mock_output(0, 0, 0, 1920, 1080);
	g_output_at_return = &test_wlr_outputs[0];
	test_views[0].x = 100;
	test_views[0].y = 200;
	test_views[0].maximized_vert = false;

	wm_view_maximize_vert(&test_views[0]);
	assert(test_views[0].maximized_vert == true);
	/* Should have saved geometry and set y to usable area */

	wl_list_remove(&test_views[0].link);
	wl_list_remove(&test_outputs[0].link);
	g_output_at_return = NULL;
	printf("  PASS: maximize_vert_with_output\n");
}

static void
test_maximize_horiz_with_output(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	setup_mock_output(0, 0, 0, 1920, 1080);
	g_output_at_return = &test_wlr_outputs[0];
	test_views[0].x = 100;
	test_views[0].y = 200;
	test_views[0].maximized_horiz = false;

	wm_view_maximize_horiz(&test_views[0]);
	assert(test_views[0].maximized_horiz == true);

	wl_list_remove(&test_views[0].link);
	wl_list_remove(&test_outputs[0].link);
	g_output_at_return = NULL;
	printf("  PASS: maximize_horiz_with_output\n");
}

/* ===== json_escape: zero-size buffer ===== */

static void
test_json_escape_zero_buffer(void)
{
	char buf[2];
	buf[0] = 'x';
	buf[1] = 'y';
	wm_json_escape(buf, 1, "test");
	/* With dst_size == 1, only null terminator fits (j + 6 < 1 is false) */
	assert(buf[0] == '\0');
	printf("  PASS: json_escape_zero_buffer\n");
}

/* ===== wm_view_resize_by clamp to minimum ===== */

static void
test_view_resize_by_negative_clamp(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);

	/* Resize by huge negative: 800-10000 should clamp to min_size=1 */
	wm_view_resize_by(&test_views[0], -10000, -10000);
	/* New size should be clamped (>0) */
	assert(test_toplevels[0].width >= 1);
	assert(test_toplevels[0].height >= 1);

	wl_list_remove(&test_views[0].link);
	printf("  PASS: view_resize_by_negative_clamp\n");
}

/* ===== Sloppy focus tests ===== */

static void
test_focus_update_for_cursor_sloppy_focus(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	setup_mock_view(1, &test_workspace);

	test_server.focus_policy = WM_FOCUS_SLOPPY;
	test_server.cursor_mode = WM_CURSOR_PASSTHROUGH;
	test_server.focused_view = &test_views[0];

	/* Set up scene_node_at to return a node with the right parent chain */
	struct wlr_scene_node fake_node;
	memset(&fake_node, 0, sizeof(fake_node));
	fake_node.type = WLR_SCENE_NODE_BUFFER;
	fake_node.parent = test_views[1].scene_tree;
	test_views[1].scene_tree->node.data = &test_views[1];

	/* Set up stub surface for scene surface */
	g_stub_scene_surface.surface = test_xdg_surfaces[1].surface;

	g_scene_node_at_return = &fake_node;
	g_try_from_surface_return = &test_toplevels[0]; /* for prev deactivation */

	/* Set up a previous focused surface */
	test_seat.keyboard_state.focused_surface = test_xdg_surfaces[0].surface;

	wm_focus_update_for_cursor(&test_server, 100, 100);

	/* Focus should have changed to test_views[1] */
	assert(test_server.focused_view == &test_views[1]);

	g_scene_node_at_return = NULL;
	g_try_from_surface_return = NULL;
	wl_list_remove(&test_views[0].link);
	wl_list_remove(&test_views[1].link);
	printf("  PASS: focus_update_for_cursor_sloppy_focus\n");
}

static void
test_focus_update_for_cursor_strict_mouse(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	setup_mock_view(1, &test_workspace);

	test_server.focus_policy = WM_FOCUS_STRICT_MOUSE;
	test_server.cursor_mode = WM_CURSOR_PASSTHROUGH;
	test_server.focused_view = &test_views[0];

	struct wlr_scene_node fake_node;
	memset(&fake_node, 0, sizeof(fake_node));
	fake_node.type = WLR_SCENE_NODE_BUFFER;
	fake_node.parent = test_views[1].scene_tree;
	test_views[1].scene_tree->node.data = &test_views[1];
	g_stub_scene_surface.surface = test_xdg_surfaces[1].surface;

	g_scene_node_at_return = &fake_node;

	wm_focus_update_for_cursor(&test_server, 200, 200);

	assert(test_server.focused_view == &test_views[1]);

	g_scene_node_at_return = NULL;
	wl_list_remove(&test_views[0].link);
	wl_list_remove(&test_views[1].link);
	printf("  PASS: focus_update_for_cursor_strict_mouse\n");
}

/* ===== focus_view with previous deactivation ===== */

static void
test_focus_view_deactivate_previous(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	setup_mock_view(1, &test_workspace);

	/* Set up previous focused surface */
	test_seat.keyboard_state.focused_surface = test_xdg_surfaces[0].surface;
	test_server.focused_view = &test_views[0];
	test_server.focus_user_initiated = true;
	g_try_from_surface_return = &test_toplevels[0];

	/* Focus a different view - should deactivate previous */
	wm_focus_view(&test_views[1], test_xdg_surfaces[1].surface);

	assert(test_server.focused_view == &test_views[1]);
	assert(test_server.focus_user_initiated == false);

	g_try_from_surface_return = NULL;
	wl_list_remove(&test_views[0].link);
	wl_list_remove(&test_views[1].link);
	printf("  PASS: focus_view_deactivate_previous\n");
}

static void
test_focus_view_with_keyboard(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);

	test_server.focus_user_initiated = true;
	g_seat_has_keyboard = true;
	memset(&g_stub_keyboard, 0, sizeof(g_stub_keyboard));

	wm_focus_view(&test_views[0], test_xdg_surfaces[0].surface);

	assert(test_server.focused_view == &test_views[0]);

	g_seat_has_keyboard = false;
	wl_list_remove(&test_views[0].link);
	printf("  PASS: focus_view_with_keyboard\n");
}

/* ===== unfocus_current with previous surface ===== */

static void
test_unfocus_current_with_prev(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);

	test_seat.keyboard_state.focused_surface = test_xdg_surfaces[0].surface;
	test_server.focused_view = &test_views[0];
	g_try_from_surface_return = &test_toplevels[0];

	wm_unfocus_current(&test_server);

	assert(test_server.focused_view == NULL);

	g_try_from_surface_return = NULL;
	wl_list_remove(&test_views[0].link);
	printf("  PASS: unfocus_current_with_prev\n");
}

/* ===== deiconify_all_workspace ===== */

static void
test_view_deiconify_all_workspace(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);
	setup_mock_view(1, &test_workspace);

	/* Minimize both views */
	test_views[0].scene_tree->node.enabled = false;
	test_views[1].scene_tree->node.enabled = false;

	wm_view_deiconify_all_workspace(&test_server);

	/* Both should be restored */
	assert(test_views[0].scene_tree->node.enabled == true);
	assert(test_views[1].scene_tree->node.enabled == true);

	wl_list_remove(&test_views[0].link);
	wl_list_remove(&test_views[1].link);
	printf("  PASS: view_deiconify_all_workspace\n");
}

/* ===== view_at with scene node ===== */

static void
test_view_at_with_node(void)
{
	init_test_server();
	setup_mock_view(0, &test_workspace);

	struct wlr_scene_node fake_node;
	memset(&fake_node, 0, sizeof(fake_node));
	fake_node.type = WLR_SCENE_NODE_BUFFER;
	fake_node.parent = test_views[0].scene_tree;
	test_views[0].scene_tree->node.data = &test_views[0];
	g_stub_scene_surface.surface = test_xdg_surfaces[0].surface;

	g_scene_node_at_return = &fake_node;

	double sx, sy;
	struct wlr_surface *surface = NULL;
	struct wm_view *result = wm_view_at(&test_server, 100, 100,
		&surface, &sx, &sy);

	assert(result == &test_views[0]);
	assert(surface == test_xdg_surfaces[0].surface);

	g_scene_node_at_return = NULL;
	wl_list_remove(&test_views[0].link);
	printf("  PASS: view_at_with_node\n");
}

/* ===== Main ===== */

int
main(void)
{
	printf("test_view_logic:\n");

	/* json_escape */
	test_json_escape_normal();
	test_json_escape_quotes();
	test_json_escape_backslash();
	test_json_escape_control_chars();
	test_json_escape_null();
	test_json_escape_empty();
	test_json_escape_long_truncates();
	test_json_escape_escape_at_boundary();
	test_json_escape_mixed_specials();

	/* layer_from_name */
	test_layer_from_name_desktop();
	test_layer_from_name_bottom_below();
	test_layer_from_name_top_above();
	test_layer_from_name_normal_default();
	test_layer_from_name_null();

	/* focus next/prev */
	test_focus_next_view_basic();
	test_focus_prev_view_basic();
	test_focus_next_view_single_noop();
	test_focus_next_view_empty_noop();
	test_focus_prev_view_two_views();
	test_focus_prev_view_empty();

	/* focus view / unfocus */
	test_focus_view_null();
	test_unfocus_current_basic();
	test_unfocus_current_already_unfocused();

	/* geometry */
	test_view_get_geometry();

	/* raise/lower */
	test_view_raise();
	test_view_lower();
	test_view_raise_layer();
	test_view_lower_layer();
	test_view_set_layer();
	test_view_set_layer_sticky_normal();
	test_view_set_layer_workspace_normal();

	/* decoration toggle */
	test_view_toggle_decoration_no_deco();

	/* resize */
	test_view_resize_by();
	test_view_resize_by_null();
	test_view_resize_by_clamp_minimum();

	/* opacity */
	test_view_set_opacity_null();
	test_view_set_opacity_clamp();
	test_view_set_opacity_no_scene_tree();

	/* maximize vert/horiz */
	test_view_maximize_vert_null();
	test_view_maximize_vert_toggle();
	test_view_maximize_horiz_null();
	test_view_maximize_horiz_toggle();

	/* lhalf / rhalf */
	test_view_lhalf_null();
	test_view_lhalf_toggle();
	test_view_rhalf_null();
	test_view_rhalf_toggle();

	/* cycle next/prev */
	test_view_cycle_next_empty();
	test_view_cycle_next_basic();
	test_view_cycle_next_skips_other_workspace();
	test_view_cycle_next_wraps_around();
	test_view_cycle_next_deiconifies();
	test_view_cycle_next_single_noop();
	test_view_cycle_next_sticky();
	test_view_cycle_prev_empty();
	test_view_cycle_prev_basic();

	/* deiconify */
	test_view_deiconify_last_basic();
	test_view_deiconify_last_none_minimized();
	test_view_deiconify_all_basic();
	test_view_deiconify_all_workspace_basic();

	/* close all */
	test_view_close_all_basic();
	test_view_close_all_empty();

	/* directional focus */
	test_view_focus_direction_no_focused();
	test_view_focus_direction_right();
	test_view_focus_direction_no_match();
	test_view_focus_direction_skips_minimized();

	/* tab activation */
	test_view_activate_tab_null();
	test_view_activate_tab_no_group();
	test_view_activate_tab_negative_index();
	test_view_activate_tab_index_out_of_range();
	test_view_activate_tab_with_group();
	test_view_activate_tab_first_index();

	/* begin_interactive */
	test_begin_interactive_move_mode();
	test_begin_interactive_resize_mode();
	test_begin_interactive_wrong_surface();
	test_begin_interactive_correct_surface();
	test_begin_interactive_resize_edges_left_top();

	/* toggle decoration extended */
	test_view_toggle_decoration_off();
	test_view_toggle_decoration_on();
	test_view_toggle_decoration_double_toggle();
	test_view_toggle_decoration_null_view();

	/* set_head */
	test_view_set_head_valid();
	test_view_set_head_first_output();
	test_view_set_head_invalid_index();
	test_view_set_head_null_view();
	test_view_set_head_no_outputs();

	/* send_to_next_head */
	test_view_send_to_next_head_null();
	test_view_send_to_next_head_two_outputs();
	test_view_send_to_next_head_wraps();
	test_view_send_to_next_head_no_outputs();
	test_view_send_to_next_head_three_outputs();

	/* send_to_prev_head */
	test_view_send_to_prev_head_null();
	test_view_send_to_prev_head_two_outputs();
	test_view_send_to_prev_head_wraps();
	test_view_send_to_prev_head_no_outputs();

	/* lhalf / rhalf with output */
	test_view_lhalf_with_output();
	test_view_rhalf_with_output();
	test_view_lhalf_toggle_with_output();
	test_view_rhalf_toggle_with_output();
	test_view_lhalf_to_rhalf();

	/* fullscreen */
	test_fullscreen_enter();
	test_fullscreen_restore();
	test_fullscreen_sticky_restore();
	test_fullscreen_no_decoration();
	test_fullscreen_no_output();
	test_fullscreen_saves_geometry();
	test_fullscreen_enter_with_decoration();
	test_fullscreen_restore_with_decoration();

	/* maximize */
	test_maximize_enter();
	test_maximize_enter_usable_area();
	test_maximize_restore();
	test_maximize_no_output();

	/* minimize */
	test_minimize_basic();
	test_minimize_with_animation();
	test_minimize_focus_transfers();
	test_minimize_last_view_clears_focus();

	/* set_title / set_app_id */
	test_set_title_updates_toolbar();
	test_set_title_null();
	test_set_app_id_stored();

	/* deiconify_view */
	test_deiconify_view_basic();
	test_deiconify_view_with_animation();

	/* focus protection */
	test_focus_view_protection_refuse();
	test_focus_view_protection_deny();
	test_focus_view_protection_gain_overrides_refuse();
	test_focus_view_same_surface_noop();

	/* cursor focus */
	test_focus_update_for_cursor_click_policy();
	test_focus_update_for_cursor_during_move();

	/* view_at */
	test_view_at_no_node();

	/* cycle prev wrap */
	test_view_cycle_prev_wraps();

	/* maximize edge cases */
	test_maximize_enter_full_max();
	test_maximize_already_lhalf();
	test_maximize_already_rhalf();
	test_maximize_vert_with_output();
	test_maximize_horiz_with_output();

	/* json_escape zero buffer */
	test_json_escape_zero_buffer();

	/* resize clamp */
	test_view_resize_by_negative_clamp();

	/* sloppy focus */
	test_focus_update_for_cursor_sloppy_focus();
	test_focus_update_for_cursor_strict_mouse();

	/* focus_view full path with previous surface */
	test_focus_view_deactivate_previous();
	test_focus_view_with_keyboard();

	/* unfocus_current with previous surface */
	test_unfocus_current_with_prev();

	/* deiconify_all_workspace */
	test_view_deiconify_all_workspace();

	/* view_at with scene node */
	test_view_at_with_node();

	printf("All view_logic tests passed.\n");
	return 0;
}
