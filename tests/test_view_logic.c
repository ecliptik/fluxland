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
	void *slit;
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

static struct wlr_scene_node *
wlr_scene_node_at(struct wlr_scene_node *node, double lx, double ly,
	double *nx, double *ny)
{
	(void)node; (void)lx; (void)ly; (void)nx; (void)ny;
	return NULL;
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

static struct wlr_scene_buffer *
wlr_scene_buffer_from_node(struct wlr_scene_node *node)
{
	(void)node;
	return NULL;
}

static struct wlr_scene_surface *
wlr_scene_surface_try_from_buffer(struct wlr_scene_buffer *buf)
{
	(void)buf;
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

static struct wlr_xdg_toplevel *
wlr_xdg_toplevel_try_from_wlr_surface(struct wlr_surface *surface)
{
	(void)surface;
	return NULL;
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

static struct wlr_keyboard *
wlr_seat_get_keyboard(struct wlr_seat *seat)
{
	(void)seat;
	return NULL;
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
	return NULL;
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
	struct wm_view *v) { (void)g; (void)v; }

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

/* --- Forward declarations for view.c functions used before defined --- */
void wm_view_get_geometry(struct wm_view *view, struct wlr_box *box);
struct wm_view *wm_view_at(struct wm_server *server, double lx, double ly,
	struct wlr_surface **surface, double *sx, double *sy);
void wm_focus_view(struct wm_view *view, struct wlr_surface *surface);
void wm_unfocus_current(struct wm_server *server);
void wm_view_raise(struct wm_view *view);
void wm_view_set_opacity(struct wm_view *view, int alpha);

/* --- Include view source directly --- */

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

static void
reset_globals(void)
{
	g_focus_view_count = 0;
	g_last_focused_view = NULL;
	g_scene_tree_next = 0;
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

/* ===== json_escape tests ===== */

static void
test_json_escape_normal(void)
{
	char buf[64];
	json_escape(buf, sizeof(buf), "hello world");
	assert(strcmp(buf, "hello world") == 0);
	printf("  PASS: json_escape_normal\n");
}

static void
test_json_escape_quotes(void)
{
	char buf[64];
	json_escape(buf, sizeof(buf), "say \"hello\"");
	assert(strcmp(buf, "say \\\"hello\\\"") == 0);
	printf("  PASS: json_escape_quotes\n");
}

static void
test_json_escape_backslash(void)
{
	char buf[64];
	json_escape(buf, sizeof(buf), "path\\to\\file");
	assert(strcmp(buf, "path\\\\to\\\\file") == 0);
	printf("  PASS: json_escape_backslash\n");
}

static void
test_json_escape_control_chars(void)
{
	char buf[64];
	json_escape(buf, sizeof(buf), "line1\nline2\ttab");
	assert(strcmp(buf, "line1line2tab") == 0);
	printf("  PASS: json_escape_control_chars\n");
}

static void
test_json_escape_null(void)
{
	char buf[64];
	buf[0] = 'x';
	json_escape(buf, sizeof(buf), NULL);
	assert(buf[0] == '\0');
	printf("  PASS: json_escape_null\n");
}

static void
test_json_escape_empty(void)
{
	char buf[64];
	json_escape(buf, sizeof(buf), "");
	assert(buf[0] == '\0');
	printf("  PASS: json_escape_empty\n");
}

static void
test_json_escape_long_truncates(void)
{
	char buf[16];
	json_escape(buf, sizeof(buf), "this is a long string that should be truncated");
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

	/* geometry */
	test_view_get_geometry();

	/* raise/lower */
	test_view_raise();
	test_view_lower();
	test_view_raise_layer();
	test_view_lower_layer();
	test_view_set_layer();

	/* decoration toggle */
	test_view_toggle_decoration_no_deco();

	/* resize */
	test_view_resize_by();
	test_view_resize_by_null();

	printf("All view_logic tests passed.\n");
	return 0;
}
