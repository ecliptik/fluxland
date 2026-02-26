/*
 * test_server.c - Unit tests for server.c signal handlers, reconfigure, and lifecycle
 *
 * Includes server.c directly with stub implementations to avoid needing
 * wlroots/wayland libraries at link time.
 */

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <signal.h>
#include <sys/wait.h>

/* --- Block real headers via their include guards --- */
#ifndef WLR_USE_UNSTABLE
#define WLR_USE_UNSTABLE
#endif

/* wayland */
#define WAYLAND_SERVER_CORE_H
#define WAYLAND_SERVER_H
#define WAYLAND_UTIL_H
#define WAYLAND_SERVER_PROTOCOL_H

/* wlroots */
#define WLR_UTIL_LOG_H
#define WLR_UTIL_BOX_H
#define WLR_UTIL_EDGES_H
#define WLR_BACKEND_H
#define WLR_ALLOCATOR_H
#define WLR_RENDER_WLR_RENDERER_H
#define WLR_TYPES_WLR_COMPOSITOR_H
#define WLR_TYPES_WLR_CURSOR_H
#define WLR_TYPES_WLR_DATA_DEVICE_H
#define WLR_TYPES_WLR_DATA_CONTROL_V1_H
#define WLR_TYPES_WLR_OUTPUT_LAYOUT_H
#define WLR_TYPES_WLR_OUTPUT_H
#define WLR_TYPES_WLR_SCENE_H
#define WLR_TYPES_WLR_SEAT_H
#define WLR_TYPES_WLR_SUBCOMPOSITOR_H
#define WLR_TYPES_WLR_XCURSOR_MANAGER_H
#define WLR_TYPES_WLR_LAYER_SHELL_V1_H
#define WLR_TYPES_WLR_XDG_DECORATION_V1
#define WLR_TYPES_WLR_XDG_SHELL_H

/* fluxland */
#define WM_SERVER_H
#define WM_CONFIG_H
#define WM_DECORATION_H
#define WM_KEYBOARD_H
#define WM_FOREIGN_TOPLEVEL_H
#define WM_IPC_H
#define WM_KEYBIND_H
#define WM_LAYER_SHELL_H
#define WM_MENU_H
#define WM_OUTPUT_H
#define WM_INPUT_H
#define WM_RULES_H
#define WM_IDLE_H
#define WM_OUTPUT_MANAGEMENT_H
#define WM_PROTOCOLS_H
#define WM_GAMMA_CONTROL_H
#define WM_PRESENTATION_H
#define WM_SCREENCOPY_H
#define WM_SESSION_LOCK_H
#define WM_SLIT_H
#define WM_STYLE_H
#define WM_SYSTRAY_H
#define WM_DRM_LEASE_H
#define WM_DRM_SYNCOBJ_H
#define WM_TEXT_INPUT_H
#define WM_TOOLBAR_H
#define WM_TRANSIENT_SEAT_H
#define WM_VIEW_H
#define WM_VIEWPORTER_H
#define WM_WORKSPACE_H
#define WM_TABLET_H
#define WM_XWAYLAND_H
#define WM_FRACTIONAL_SCALE_H
#define WM_MOUSEBIND_H
#define WM_FOCUS_NAV_H
#define WM_RENDER_H
#define WM_CURSOR_H
#define WM_ANIMATION_H
#define WM_TABGROUP_H
#define WM_PLACEMENT_H

/* --- Stub wayland types --- */

struct wl_display {
	int terminated;
};

struct wl_event_loop {
	int dummy;
};

struct wl_event_source {
	int dummy;
};

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

/* --- Stub wlr types --- */

struct wlr_scene_node {
	int enabled;
	int x, y;
};

struct wlr_scene_tree {
	struct wlr_scene_node node;
};

struct wlr_scene {
	struct wlr_scene_tree tree;
};

struct wlr_box {
	int x, y, width, height;
};

struct wlr_backend { int dummy; };
struct wlr_session { int dummy; };
struct wlr_renderer { int dummy; };
struct wlr_allocator { int dummy; };
struct wlr_compositor { int dummy; };
struct wlr_cursor { int dummy; };
struct wlr_xcursor_manager { int dummy; };
struct wlr_xdg_shell { int dummy; };
struct wlr_seat { int dummy; };
struct wlr_output_layout { int dummy; };
struct wlr_scene_output_layout;
struct wlr_scene_buffer;
struct wlr_scene_rect;
struct wlr_xdg_decoration_manager_v1;
struct wlr_layer_shell_v1;
struct wlr_foreign_toplevel_manager_v1;
struct wlr_primary_selection_v1_device_manager;
struct wlr_pointer_constraints_v1;
struct wlr_pointer_constraint_v1;
struct wlr_relative_pointer_manager_v1;
struct wlr_fractional_scale_manager_v1;
struct wlr_cursor_shape_manager_v1;
struct wlr_viewporter;
struct wlr_single_pixel_buffer_manager_v1;
struct wlr_gamma_control_manager_v1;
struct wlr_data_control_manager_v1;
struct wlr_presentation;
struct wlr_virtual_keyboard_manager_v1;
struct wlr_virtual_pointer_manager_v1;
struct wlr_keyboard_shortcuts_inhibit_manager_v1;
struct wlr_keyboard_shortcuts_inhibitor_v1;
struct wlr_pointer_gestures_v1;
struct wlr_xdg_activation_v1;
struct wlr_tearing_control_manager_v1;
struct wlr_output_power_manager_v1;
struct wlr_tablet_manager_v2;
struct wlr_content_type_manager_v1;
struct wlr_alpha_modifier_v1;
struct wlr_security_context_manager_v1;
struct wlr_linux_dmabuf_v1;
struct wlr_xdg_output_manager_v1;
struct wlr_xdg_foreign_registry;
struct wlr_xdg_foreign_v1;
struct wlr_xdg_foreign_v2;
struct wlr_ext_foreign_toplevel_list_v1;
struct wlr_drm_lease_v1_manager;
struct wlr_transient_seat_manager_v1;
struct wlr_linux_drm_syncobj_manager_v1;
struct wlr_text_input_manager_v3;
struct wlr_input_method_manager_v2;
struct wlr_input_method_v2;

#define WLR_BUFFER_DATA_PTR_ACCESS_WRITE 2

struct wlr_buffer {
	int width, height;
};

/* wlr_log no-op */
#define wlr_log(verb, fmt, ...) ((void)0)
#define WLR_ERROR 1
#define WLR_INFO 2
#define WLR_DEBUG 7

struct wl_listener {
	struct wl_list link;
	void (*notify)(struct wl_listener *, void *);
};

/* --- Stub fluxland types --- */

/* keybind.h types */
struct wm_keybind {
	struct wl_list link;
};

struct wm_chain_state {
	struct wl_list *current_level;
	bool in_chain;
	struct wl_event_source *timeout;
};

/* mousebind.h types */
struct wm_mousebind {
	struct wl_list link;
};

struct wm_mouse_state {
	uint32_t last_button;
	uint32_t last_time_msec;
	double press_x, press_y;
	bool button_pressed;
	uint32_t pressed_button;
};

/* ipc.h types */
enum wm_ipc_event_type {
	WM_IPC_EVENT_STYLE_CHANGED  = (1 << 7),
	WM_IPC_EVENT_CONFIG_RELOADED = (1 << 8),
};

struct ipc_event_throttle {
	/* simplified */
	long last_sent;
};

struct wm_ipc_server {
	struct wm_server *server;
	int socket_fd;
	char *socket_path;
	struct wl_event_source *event_source;
	struct wl_list clients;
	int client_count;
	struct ipc_event_throttle event_throttle[32];
};

/* rules.h types */
struct wm_rules {
	struct wl_list window_rules;
	struct wl_list group_rules;
};

/* idle.h types */
struct wm_idle {
	int dummy;
};

/* output_management.h types */
struct wm_output_management {
	int dummy;
};

/* protocols.h types (empty for test) */

/* session_lock.h types */
struct wm_session_lock {
	int dummy;
};

/* text_input.h types */
struct wm_text_input_relay {
	struct wm_server *server;
	struct wl_list text_inputs;
	void *text_input_mgr;
	void *input_method_mgr;
	void *input_method;
};

/* focus_nav.h types */
enum wm_focus_zone {
	WM_FOCUS_ZONE_WINDOWS = 0,
	WM_FOCUS_ZONE_TOOLBAR,
};

struct wm_focus_nav {
	enum wm_focus_zone zone;
	int toolbar_index;
};

/* config.h types */
enum wm_focus_policy {
	WM_FOCUS_CLICK = 0,
	WM_FOCUS_SLOPPY,
};

enum wm_workspace_mode {
	WM_WORKSPACE_GLOBAL = 0,
	WM_WORKSPACE_PER_OUTPUT,
};

#define WM_DEFAULT_WORKSPACE_COUNT 4

struct wm_config {
	int focus_policy;
	bool show_window_position;
	char *keys_file;
	char *style_file;
	char *style_overlay;
	int workspace_name_count;
	char **workspace_names;
	enum wm_workspace_mode workspace_mode;
	int mouse_button_map;
	char *apps_file;
	char *menu_file;
	char *config_dir;
	int workspace_count;
};

/* workspace.h types */
struct wm_workspace {
	struct wl_list link;
	struct wm_server *server;
	struct wlr_scene_tree *tree;
	char *name;
	int index;
};

/* view.h types */
enum wm_cursor_mode {
	WM_CURSOR_PASSTHROUGH = 0,
	WM_CURSOR_MOVE,
	WM_CURSOR_RESIZE,
	WM_CURSOR_TABBING,
};

struct wm_decoration { int dummy; };

struct wm_view {
	struct wl_list link;
	struct wm_decoration *decoration;
};

/* Forward declares for pointer types */
struct wm_style { int dummy; };
struct wm_menu { int dummy; };
struct wm_toolbar { int dummy; };
struct wm_slit { int dummy; };
struct wm_systray { int dummy; };

#define WM_XCURSOR_DEFAULT "left_ptr"
#define WM_XCURSOR_SIZE 24
#define WM_WLR_COMPOSITOR_VERSION 6

/* --- The wm_server struct (normally from server.h) --- */

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

	struct wlr_xdg_decoration_manager_v1 *xdg_decoration_mgr;
	struct wl_listener new_xdg_decoration;

	struct wlr_layer_shell_v1 *layer_shell;
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

	struct wlr_tablet_manager_v2 *tablet_manager;
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

	struct wlr_foreign_toplevel_manager_v1 *foreign_toplevel_manager;
	struct wm_output_management output_mgmt;
	struct wm_idle idle;

	struct wlr_primary_selection_v1_device_manager *primary_selection_mgr;
	struct wl_listener request_set_primary_selection;

	struct wlr_data_control_manager_v1 *data_control_mgr;

	struct wlr_pointer_constraints_v1 *pointer_constraints;
	struct wlr_pointer_constraint_v1 *active_constraint;
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

	struct wlr_fractional_scale_manager_v1 *fractional_scale_mgr;

	struct wlr_cursor_shape_manager_v1 *cursor_shape_mgr;
	struct wl_listener cursor_shape_request;

	struct wlr_viewporter *viewporter;
	struct wlr_single_pixel_buffer_manager_v1 *single_pixel_buffer_mgr;

	struct wlr_gamma_control_manager_v1 *gamma_control_mgr;
	struct wl_listener gamma_set_gamma;

	struct wlr_presentation *presentation;

	struct wm_text_input_relay text_input_relay;

	struct wlr_virtual_keyboard_manager_v1 *virtual_keyboard_mgr;
	struct wl_listener new_virtual_keyboard;

	struct wlr_virtual_pointer_manager_v1 *virtual_pointer_mgr;
	struct wl_listener new_virtual_pointer;

	struct wlr_keyboard_shortcuts_inhibit_manager_v1 *kb_shortcuts_inhibit_mgr;
	struct wl_listener new_kb_shortcuts_inhibitor;
	struct wlr_keyboard_shortcuts_inhibitor_v1 *active_kb_inhibitor;
	struct wl_listener kb_inhibitor_destroy;

	struct wlr_xdg_activation_v1 *xdg_activation;
	struct wl_listener xdg_activation_request;

	struct wlr_tearing_control_manager_v1 *tearing_control_mgr;
	struct wl_listener tearing_new_object;

	struct wlr_output_power_manager_v1 *output_power_mgr;
	struct wl_listener output_power_set_mode;

	struct wlr_content_type_manager_v1 *content_type_mgr;
	struct wlr_alpha_modifier_v1 *alpha_modifier;
	struct wlr_security_context_manager_v1 *security_context_mgr;
	struct wlr_linux_dmabuf_v1 *linux_dmabuf;
	struct wlr_xdg_output_manager_v1 *xdg_output_mgr;

	struct wlr_xdg_foreign_registry *xdg_foreign_registry;
	struct wlr_xdg_foreign_v1 *xdg_foreign_v1;
	struct wlr_xdg_foreign_v2 *xdg_foreign_v2;

	struct wlr_ext_foreign_toplevel_list_v1 *ext_foreign_toplevel_list;
	struct wm_session_lock session_lock;

	struct wlr_drm_lease_v1_manager *drm_lease_mgr;
	struct wl_listener drm_lease_request;

	struct wlr_transient_seat_manager_v1 *transient_seat_mgr;
	struct wl_listener transient_seat_create;

	struct wlr_linux_drm_syncobj_manager_v1 *syncobj_mgr;

	struct wm_toolbar *toolbar;
	struct wm_slit *slit;

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

/* Disable systray code paths in server.c (separate test covers systray) */
#undef WM_HAS_SYSTRAY

/* Forward declarations (normally from server.h) */
bool wm_server_init(struct wm_server *server);
bool wm_server_start(struct wm_server *server);
void wm_server_run(struct wm_server *server);
void wm_server_reconfigure(struct wm_server *server);
void wm_server_destroy(struct wm_server *server);

/* --- Stub tracking globals --- */

static int g_config_reload_count;
static int g_ipc_broadcast_count;
static int g_keybind_destroy_all_count;
static int g_keybind_destroy_list_count;
static int g_keybind_load_count;
static int g_keybind_load_return;
static int g_mousebind_destroy_list_count;
static int g_mousebind_load_count;
static int g_style_load_count;
static int g_style_load_overlay_count;
static int g_menu_hide_all_count;
static int g_menu_destroy_count;
static int g_menu_load_count;
static int g_menu_create_window_menu_count;
static int g_rules_finish_count;
static int g_rules_init_count;
static int g_rules_load_count;
static int g_keyboard_apply_config_count;
static int g_toolbar_relayout_count;
static int g_slit_relayout_count;
static int g_decoration_update_count;
static int g_scene_node_set_enabled_count;
static int g_wl_event_source_remove_count;

/* --- wlr scene stubs --- */

static void
wlr_scene_node_set_enabled(struct wlr_scene_node *node, bool enabled)
{
	if (node) {
		node->enabled = enabled ? 1 : 0;
	}
	g_scene_node_set_enabled_count++;
}

static void
wlr_scene_node_destroy(struct wlr_scene_node *node)
{
	(void)node;
}

static struct wlr_scene_tree *
wlr_scene_tree_create(struct wlr_scene_tree *parent)
{
	(void)parent;
	return calloc(1, sizeof(struct wlr_scene_tree));
}

/* --- wayland stubs --- */

static struct wl_display g_wl_display;
static struct wl_event_loop g_wl_event_loop;
static struct wl_event_source g_wl_event_sources[16];
static int g_event_source_idx;

static int g_display_terminate_count;

static void
wl_display_terminate(struct wl_display *display)
{
	display->terminated = 1;
	g_display_terminate_count++;
}

static void
wl_display_run(struct wl_display *display)
{
	(void)display;
}

static void
wl_display_destroy(struct wl_display *display)
{
	(void)display;
}

static void
wl_display_destroy_clients(struct wl_display *display)
{
	(void)display;
}

static bool g_display_create_fail;

static struct wl_display *
wl_display_create(void)
{
	if (g_display_create_fail) return NULL;
	memset(&g_wl_display, 0, sizeof(g_wl_display));
	return &g_wl_display;
}

static struct wl_event_loop *
wl_display_get_event_loop(struct wl_display *display)
{
	(void)display;
	return &g_wl_event_loop;
}

typedef int (*wl_event_loop_signal_func_t)(int, void *);
typedef int (*wl_event_loop_fd_func_t)(int, uint32_t, void *);

static struct wl_event_source *
wl_event_loop_add_signal(struct wl_event_loop *loop, int signal_number,
	wl_event_loop_signal_func_t func, void *data)
{
	(void)loop; (void)signal_number; (void)func; (void)data;
	if (g_event_source_idx < 16)
		return &g_wl_event_sources[g_event_source_idx++];
	return NULL;
}

static void
wl_event_source_remove(struct wl_event_source *source)
{
	(void)source;
	g_wl_event_source_remove_count++;
}

static const char *
wl_display_add_socket_auto(struct wl_display *display)
{
	(void)display;
	return "wayland-test";
}

/* --- wlroots function stubs --- */

static bool g_backend_create_fail;
static struct wlr_backend g_backend;

static struct wlr_backend *
wlr_backend_autocreate(struct wl_event_loop *loop, struct wlr_session **session)
{
	(void)loop; (void)session;
	if (g_backend_create_fail) return NULL;
	return &g_backend;
}

static bool g_backend_start_fail;

static bool
wlr_backend_start(struct wlr_backend *backend)
{
	(void)backend;
	return !g_backend_start_fail;
}

static void
wlr_backend_destroy(struct wlr_backend *backend)
{
	(void)backend;
}

static bool g_renderer_create_fail;
static struct wlr_renderer g_renderer;

static struct wlr_renderer *
wlr_renderer_autocreate(struct wlr_backend *backend)
{
	(void)backend;
	if (g_renderer_create_fail) return NULL;
	return &g_renderer;
}

static void
wlr_renderer_init_wl_shm(struct wlr_renderer *renderer,
	struct wl_display *display)
{
	(void)renderer; (void)display;
}

static void
wlr_renderer_destroy(struct wlr_renderer *renderer)
{
	(void)renderer;
}

static bool g_allocator_create_fail;
static struct wlr_allocator g_allocator;

static struct wlr_allocator *
wlr_allocator_autocreate(struct wlr_backend *backend,
	struct wlr_renderer *renderer)
{
	(void)backend; (void)renderer;
	if (g_allocator_create_fail) return NULL;
	return &g_allocator;
}

static void
wlr_allocator_destroy(struct wlr_allocator *allocator)
{
	(void)allocator;
}

static bool g_compositor_create_fail;
static struct wlr_compositor g_compositor;

static struct wlr_compositor *
wlr_compositor_create(struct wl_display *display, uint32_t version,
	struct wlr_renderer *renderer)
{
	(void)display; (void)version; (void)renderer;
	if (g_compositor_create_fail) return NULL;
	return &g_compositor;
}

static void
wlr_subcompositor_create(struct wl_display *display)
{
	(void)display;
}

static void
wlr_data_device_manager_create(struct wl_display *display)
{
	(void)display;
}

static struct wlr_data_control_manager_v1 *
wlr_data_control_manager_v1_create(struct wl_display *display)
{
	(void)display;
	return NULL;
}

static bool g_scene_create_fail;

static struct wlr_scene *
wlr_scene_create(void)
{
	if (g_scene_create_fail) return NULL;
	return calloc(1, sizeof(struct wlr_scene));
}

static bool g_xdg_shell_create_fail;
static struct wlr_xdg_shell g_xdg_shell;

static struct wlr_xdg_shell *
wlr_xdg_shell_create(struct wl_display *display, uint32_t version)
{
	(void)display; (void)version;
	if (g_xdg_shell_create_fail) return NULL;
	return &g_xdg_shell;
}

static bool g_seat_create_fail;
static struct wlr_seat g_seat;

static struct wlr_seat *
wlr_seat_create(struct wl_display *display, const char *name)
{
	(void)display; (void)name;
	if (g_seat_create_fail) return NULL;
	return &g_seat;
}

static struct wlr_cursor g_cursor;

static struct wlr_cursor *
wlr_cursor_create(void)
{
	return &g_cursor;
}

static void
wlr_cursor_attach_output_layout(struct wlr_cursor *cursor,
	struct wlr_output_layout *layout)
{
	(void)cursor; (void)layout;
}

static void
wlr_cursor_destroy(struct wlr_cursor *cursor)
{
	(void)cursor;
}

static struct wlr_xcursor_manager *
wlr_xcursor_manager_create(const char *name, uint32_t size)
{
	(void)name; (void)size;
	/* Return a non-NULL dummy */
	static int dummy;
	return (struct wlr_xcursor_manager *)&dummy;
}

static void
wlr_xcursor_manager_destroy(struct wlr_xcursor_manager *mgr)
{
	(void)mgr;
}

/* --- fluxland module stubs --- */

static void config_reload(struct wm_config *config)
{
	(void)config;
	g_config_reload_count++;
}

static struct wm_config *config_create(void)
{
	struct wm_config *c = calloc(1, sizeof(*c));
	if (c) {
		c->workspace_count = WM_DEFAULT_WORKSPACE_COUNT;
	}
	return c;
}

static void config_load(struct wm_config *config, const char *path)
{
	(void)config; (void)path;
}

static void config_destroy(struct wm_config *config)
{
	if (config) {
		free(config->keys_file);
		free(config->style_file);
		free(config->style_overlay);
		free(config->apps_file);
		free(config->menu_file);
		free(config->config_dir);
		if (config->workspace_names) {
			for (int i = 0; i < config->workspace_name_count; i++)
				free(config->workspace_names[i]);
			free(config->workspace_names);
		}
		free(config);
	}
}

static void wm_ipc_broadcast_event(struct wm_ipc_server *ipc,
	int event_type, const char *json)
{
	(void)ipc; (void)event_type; (void)json;
	g_ipc_broadcast_count++;
}

static void keybind_destroy_all(struct wl_list *keymodes)
{
	(void)keymodes;
	g_keybind_destroy_all_count++;
}

static void keybind_destroy_list(struct wl_list *keybindings)
{
	(void)keybindings;
	g_keybind_destroy_list_count++;
}

static int keybind_load(struct wl_list *keymodes, const char *path)
{
	(void)keymodes; (void)path;
	g_keybind_load_count++;
	return g_keybind_load_return;
}

static void mousebind_destroy_list(struct wl_list *mousebindings)
{
	(void)mousebindings;
	g_mousebind_destroy_list_count++;
}

static void mousebind_load(struct wl_list *mousebindings,
	const char *path, int button_map)
{
	(void)mousebindings; (void)path; (void)button_map;
	g_mousebind_load_count++;
}

static struct wm_style g_style;

static struct wm_style *style_create(void)
{
	return &g_style;
}

static int style_load(struct wm_style *style, const char *path)
{
	(void)style; (void)path;
	g_style_load_count++;
	return 0;
}

static void style_load_overlay(struct wm_style *style, const char *path)
{
	(void)style; (void)path;
	g_style_load_overlay_count++;
}

static void style_destroy(struct wm_style *style)
{
	(void)style;
}

static void wm_menu_hide_all(struct wm_server *server)
{
	(void)server;
	g_menu_hide_all_count++;
}

static void wm_menu_destroy(struct wm_menu *menu)
{
	(void)menu;
	g_menu_destroy_count++;
}

static struct wm_menu *wm_menu_load(struct wm_server *server, const char *path)
{
	(void)server; (void)path;
	g_menu_load_count++;
	/* Return a non-NULL dummy */
	static int dummy_menu;
	return (struct wm_menu *)&dummy_menu;
}

static struct wm_menu *wm_menu_create_window_menu(struct wm_server *server)
{
	(void)server;
	g_menu_create_window_menu_count++;
	static int dummy_wm;
	return (struct wm_menu *)&dummy_wm;
}

static void wm_rules_init(struct wm_rules *rules)
{
	wl_list_init(&rules->window_rules);
	wl_list_init(&rules->group_rules);
	g_rules_init_count++;
}

static void wm_rules_finish(struct wm_rules *rules)
{
	(void)rules;
	g_rules_finish_count++;
}

static int wm_rules_load(struct wm_rules *rules, const char *path)
{
	(void)rules; (void)path;
	g_rules_load_count++;
	return 1;
}

static void wm_keyboard_apply_config(struct wm_server *server)
{
	(void)server;
	g_keyboard_apply_config_count++;
}

static struct wm_toolbar *wm_toolbar_create(struct wm_server *server)
{
	(void)server;
	static struct wm_toolbar dummy;
	return &dummy;
}

static void wm_toolbar_relayout(struct wm_toolbar *toolbar)
{
	(void)toolbar;
	g_toolbar_relayout_count++;
}

static void wm_toolbar_destroy(struct wm_toolbar *toolbar)
{
	(void)toolbar;
}

static struct wm_slit *wm_slit_create(struct wm_server *server)
{
	(void)server;
	static struct wm_slit dummy;
	return &dummy;
}

static void wm_slit_relayout(struct wm_slit *slit)
{
	(void)slit;
	g_slit_relayout_count++;
}

static void wm_slit_destroy(struct wm_slit *slit)
{
	(void)slit;
}

static void wm_decoration_update(struct wm_decoration *dec,
	struct wm_style *style)
{
	(void)dec; (void)style;
	g_decoration_update_count++;
}

/* Init/finish stubs for all modules called from wm_server_init/destroy */
static void wm_output_init(struct wm_server *s) { (void)s; }
static void wm_output_finish(struct wm_server *s) { (void)s; }
static void wm_view_init(struct wm_server *s) { (void)s; }
static void wm_xdg_decoration_init(struct wm_server *s) { (void)s; }
static void wm_layer_shell_init(struct wm_server *s) { (void)s; }
static void wm_layer_shell_finish(struct wm_server *s) { (void)s; }
static void wm_session_lock_init(struct wm_server *s) { (void)s; }
static void wm_session_lock_finish(struct wm_server *s) { (void)s; }
static void wm_idle_init(struct wm_server *s) { (void)s; }
static void wm_idle_finish(struct wm_server *s) { (void)s; }
static void wm_output_management_init(struct wm_server *s) { (void)s; }
static void wm_output_management_finish(struct wm_server *s) { (void)s; }
static void wm_screencopy_init(struct wm_server *s) { (void)s; }
static void wm_gamma_control_init(struct wm_server *s) { (void)s; }
static void wm_fractional_scale_init(struct wm_server *s) { (void)s; }
static void wm_foreign_toplevel_init(struct wm_server *s) { (void)s; }
static void wm_viewporter_init(struct wm_server *s) { (void)s; }
static void wm_presentation_init(struct wm_server *s) { (void)s; }
static void wm_input_init(struct wm_server *s) { (void)s; }
static void wm_input_finish(struct wm_server *s) { (void)s; }
static void wm_protocols_init(struct wm_server *s) { (void)s; }
static void wm_protocols_finish(struct wm_server *s) { (void)s; }
static void wm_text_input_init(struct wm_server *s) { (void)s; }
static void wm_text_input_finish(struct wm_server *s) { (void)s; }
static void wm_tablet_init(struct wm_server *s) { (void)s; }
static void wm_tablet_finish(struct wm_server *s) { (void)s; }
static void wm_drm_lease_init(struct wm_server *s) { (void)s; }
static void wm_drm_lease_finish(struct wm_server *s) { (void)s; }
static void wm_transient_seat_init(struct wm_server *s) { (void)s; }
static void wm_transient_seat_finish(struct wm_server *s) { (void)s; }
static void wm_drm_syncobj_init(struct wm_server *s) { (void)s; }
static void wm_drm_syncobj_finish(struct wm_server *s) { (void)s; }
static void wm_xwayland_init(struct wm_server *s) { (void)s; }
static void wm_xwayland_finish(struct wm_server *s) { (void)s; }
static void wm_ipc_init(struct wm_ipc_server *ipc, struct wm_server *s)
{
	(void)ipc; (void)s;
}
static void wm_ipc_destroy(struct wm_ipc_server *ipc) { (void)ipc; }

static void wm_focus_nav_init(struct wm_focus_nav *nav)
{
	nav->zone = WM_FOCUS_ZONE_WINDOWS;
	nav->toolbar_index = -1;
}

static void wm_workspaces_init(struct wm_server *server, int count)
{
	wl_list_init(&server->workspaces);
	server->workspace_count = count;
	for (int i = 0; i < count; i++) {
		struct wm_workspace *ws = calloc(1, sizeof(*ws));
		ws->server = server;
		ws->index = i;
		ws->tree = calloc(1, sizeof(struct wlr_scene_tree));
		wl_list_insert(server->workspaces.prev, &ws->link);
		if (i == 0) {
			server->current_workspace = ws;
		}
	}
}

static void wm_workspaces_finish(struct wm_server *server)
{
	struct wm_workspace *ws, *tmp;
	wl_list_for_each_safe(ws, tmp, &server->workspaces, link) {
		wl_list_remove(&ws->link);
		free(ws->name);
		free(ws->tree);
		free(ws);
	}
}

/* --- Include server.c source directly --- */
#include "../src/server.c"

/* --- Test helpers --- */

static void
reset_globals(void)
{
	g_config_reload_count = 0;
	g_ipc_broadcast_count = 0;
	g_keybind_destroy_all_count = 0;
	g_keybind_destroy_list_count = 0;
	g_keybind_load_count = 0;
	g_keybind_load_return = 0;
	g_mousebind_destroy_list_count = 0;
	g_mousebind_load_count = 0;
	g_style_load_count = 0;
	g_style_load_overlay_count = 0;
	g_menu_hide_all_count = 0;
	g_menu_destroy_count = 0;
	g_menu_load_count = 0;
	g_menu_create_window_menu_count = 0;
	g_rules_finish_count = 0;
	g_rules_init_count = 0;
	g_rules_load_count = 0;
	g_keyboard_apply_config_count = 0;
	g_toolbar_relayout_count = 0;
	g_slit_relayout_count = 0;
	g_decoration_update_count = 0;
	g_scene_node_set_enabled_count = 0;
	g_wl_event_source_remove_count = 0;
	g_display_terminate_count = 0;
	g_display_create_fail = false;
	g_backend_create_fail = false;
	g_backend_start_fail = false;
	g_renderer_create_fail = false;
	g_allocator_create_fail = false;
	g_compositor_create_fail = false;
	g_scene_create_fail = false;
	g_xdg_shell_create_fail = false;
	g_seat_create_fail = false;
	g_event_source_idx = 0;
}

static struct wm_server *
make_test_server(void)
{
	struct wm_server *server = calloc(1, sizeof(*server));
	assert(server);

	server->wl_display = &g_wl_display;
	server->wl_event_loop = &g_wl_event_loop;
	wl_list_init(&server->views);
	wl_list_init(&server->workspaces);
	wl_list_init(&server->keybindings);
	wl_list_init(&server->keymodes);
	wl_list_init(&server->mousebindings);
	wm_rules_init(&server->rules);
	server->current_keymode = strdup("default");

	return server;
}

static void
destroy_test_server(struct wm_server *server)
{
	if (!server) return;
	wm_workspaces_finish(server);
	free(server->current_keymode);
	config_destroy(server->config);
	free(server);
}

/* ===== Tests ===== */

/*
 * Test 1: handle_signal calls wl_display_terminate.
 */
static void
test_handle_signal(void)
{
	reset_globals();
	struct wl_display display = {0};

	int ret = handle_signal(SIGINT, &display);
	assert(ret == 0);
	assert(display.terminated == 1);
	assert(g_display_terminate_count == 1);

	printf("  PASS: test_handle_signal\n");
}

/*
 * Test 2: handle_sighup calls wm_server_reconfigure.
 */
static void
test_handle_sighup(void)
{
	reset_globals();
	struct wm_server *server = make_test_server();

	/* Give it a config so reconfigure does something */
	server->config = config_create();
	server->config->keys_file = strdup("/tmp/keys");

	int ret = handle_sighup(SIGHUP, server);
	assert(ret == 0);
	assert(g_config_reload_count == 1);

	destroy_test_server(server);
	printf("  PASS: test_handle_sighup\n");
}

/*
 * Test 3: handle_sigchld is a no-op.
 */
static void
test_handle_sigchld(void)
{
	reset_globals();

	int ret = handle_sigchld(SIGCHLD, NULL);
	assert(ret == 0);

	printf("  PASS: test_handle_sigchld\n");
}

/*
 * Test 4: wm_server_reconfigure with NULL config.
 */
static void
test_reconfigure_null_config(void)
{
	reset_globals();
	struct wm_server *server = make_test_server();
	server->config = NULL;

	wm_server_reconfigure(server);

	/* config_reload should not be called */
	assert(g_config_reload_count == 0);
	/* IPC broadcast should still happen (config_reloaded event) */
	assert(g_ipc_broadcast_count == 1);
	/* keybind_load not called without keys_file */
	assert(g_keybind_load_count == 0);

	destroy_test_server(server);
	printf("  PASS: test_reconfigure_null_config\n");
}

/*
 * Test 5: wm_server_reconfigure with full config.
 */
static void
test_reconfigure_full_config(void)
{
	reset_globals();
	struct wm_server *server = make_test_server();

	server->config = config_create();
	server->config->keys_file = strdup("/tmp/keys");
	server->config->style_file = strdup("/tmp/style");
	server->config->style_overlay = strdup("/tmp/overlay");
	server->config->apps_file = strdup("/tmp/apps");
	server->config->menu_file = strdup("/tmp/menu");
	server->config->focus_policy = WM_FOCUS_SLOPPY;
	server->config->show_window_position = true;
	server->config->workspace_mode = WM_WORKSPACE_GLOBAL;
	server->style = &g_style;
	server->root_menu = (struct wm_menu *)0x1;
	server->window_menu = (struct wm_menu *)0x2;
	server->toolbar = (struct wm_toolbar *)0x3;
	server->slit = (struct wm_slit *)0x4;

	/* Add a workspace */
	wm_workspaces_init(server, 2);

	wm_server_reconfigure(server);

	/* Verify all subsystems were reloaded */
	assert(g_config_reload_count == 1);
	assert(g_ipc_broadcast_count == 2); /* config_reloaded + style_changed */
	assert(g_keybind_destroy_all_count == 1);
	assert(g_keybind_destroy_list_count == 1);
	assert(g_keybind_load_count == 1);
	assert(g_mousebind_destroy_list_count == 1);
	assert(g_mousebind_load_count == 1);
	assert(g_style_load_count == 1);
	assert(g_style_load_overlay_count == 1);
	assert(g_menu_hide_all_count == 1);
	assert(g_menu_destroy_count == 2); /* root + window */
	assert(g_menu_load_count == 1);
	assert(g_menu_create_window_menu_count == 1);
	assert(g_rules_finish_count == 1);
	assert(g_rules_init_count >= 1);
	assert(g_rules_load_count == 1);
	assert(g_keyboard_apply_config_count == 1);
	assert(g_toolbar_relayout_count == 1);
	assert(g_slit_relayout_count == 1);

	/* Focus policy should be updated */
	assert(server->focus_policy == WM_FOCUS_SLOPPY);
	assert(server->show_position == true);

	destroy_test_server(server);
	printf("  PASS: test_reconfigure_full_config\n");
}

/*
 * Test 6: wm_server_reconfigure without style_overlay.
 */
static void
test_reconfigure_no_overlay(void)
{
	reset_globals();
	struct wm_server *server = make_test_server();

	server->config = config_create();
	server->config->keys_file = strdup("/tmp/keys");
	server->config->style_file = strdup("/tmp/style");
	server->config->style_overlay = NULL;
	server->style = &g_style;
	wm_workspaces_init(server, 1);

	wm_server_reconfigure(server);

	assert(g_style_load_count == 1);
	assert(g_style_load_overlay_count == 0);

	destroy_test_server(server);
	printf("  PASS: test_reconfigure_no_overlay\n");
}

/*
 * Test 7: wm_server_reconfigure with chain_state.timeout set.
 */
static void
test_reconfigure_clears_chain_timeout(void)
{
	reset_globals();
	struct wm_server *server = make_test_server();
	server->config = config_create();

	struct wl_event_source timeout_source = {0};
	server->chain_state.in_chain = true;
	server->chain_state.current_level = (struct wl_list *)0x1;
	server->chain_state.timeout = &timeout_source;

	wm_workspaces_init(server, 1);
	wm_server_reconfigure(server);

	/* Chain state should be cleared */
	assert(server->chain_state.in_chain == false);
	assert(server->chain_state.current_level == NULL);
	assert(server->chain_state.timeout == NULL);
	/* Timeout should have been removed */
	assert(g_wl_event_source_remove_count >= 1);

	destroy_test_server(server);
	printf("  PASS: test_reconfigure_clears_chain_timeout\n");
}

/*
 * Test 8: wm_server_reconfigure with workspace_mode = PER_OUTPUT.
 * All workspace trees should be enabled.
 */
static void
test_reconfigure_per_output_mode(void)
{
	reset_globals();
	struct wm_server *server = make_test_server();
	server->config = config_create();
	server->config->workspace_mode = WM_WORKSPACE_PER_OUTPUT;
	wm_workspaces_init(server, 3);

	wm_server_reconfigure(server);

	/* All workspace trees should be enabled */
	struct wm_workspace *ws;
	wl_list_for_each(ws, &server->workspaces, link) {
		assert(ws->tree->node.enabled == 1);
	}

	destroy_test_server(server);
	printf("  PASS: test_reconfigure_per_output_mode\n");
}

/*
 * Test 9: wm_server_reconfigure with workspace_mode = GLOBAL.
 * Only current workspace tree should be enabled.
 */
static void
test_reconfigure_global_mode(void)
{
	reset_globals();
	struct wm_server *server = make_test_server();
	server->config = config_create();
	server->config->workspace_mode = WM_WORKSPACE_GLOBAL;
	wm_workspaces_init(server, 3);

	wm_server_reconfigure(server);

	/* Only current workspace should be enabled */
	struct wm_workspace *ws;
	wl_list_for_each(ws, &server->workspaces, link) {
		if (ws == server->current_workspace)
			assert(ws->tree->node.enabled == 1);
		else
			assert(ws->tree->node.enabled == 0);
	}

	destroy_test_server(server);
	printf("  PASS: test_reconfigure_global_mode\n");
}

/*
 * Test 10: wm_server_reconfigure updates workspace names.
 */
static void
test_reconfigure_workspace_names(void)
{
	reset_globals();
	struct wm_server *server = make_test_server();
	server->config = config_create();
	server->config->workspace_name_count = 2;
	server->config->workspace_names = calloc(2, sizeof(char *));
	server->config->workspace_names[0] = strdup("Work");
	server->config->workspace_names[1] = strdup("Play");
	wm_workspaces_init(server, 3);

	wm_server_reconfigure(server);

	/* First two workspaces should have names, third unchanged */
	int idx = 0;
	struct wm_workspace *ws;
	wl_list_for_each(ws, &server->workspaces, link) {
		if (idx == 0) {
			assert(ws->name != NULL);
			assert(strcmp(ws->name, "Work") == 0);
		} else if (idx == 1) {
			assert(ws->name != NULL);
			assert(strcmp(ws->name, "Play") == 0);
		}
		idx++;
	}

	destroy_test_server(server);
	printf("  PASS: test_reconfigure_workspace_names\n");
}

/*
 * Test 11: wm_server_reconfigure refreshes view decorations.
 */
static void
test_reconfigure_refreshes_decorations(void)
{
	reset_globals();
	struct wm_server *server = make_test_server();
	server->config = config_create();
	server->style = &g_style;
	wm_workspaces_init(server, 1);

	/* Add two views with decorations and one without */
	struct wm_view view1 = {0};
	struct wm_decoration dec1;
	view1.decoration = &dec1;
	wl_list_insert(&server->views, &view1.link);

	struct wm_view view2 = {0};
	view2.decoration = NULL;
	wl_list_insert(&server->views, &view2.link);

	struct wm_view view3 = {0};
	struct wm_decoration dec3;
	view3.decoration = &dec3;
	wl_list_insert(&server->views, &view3.link);

	wm_server_reconfigure(server);

	/* Only views with decorations should be updated */
	assert(g_decoration_update_count == 2);

	/* Clean up views from list */
	wl_list_remove(&view1.link);
	wl_list_remove(&view2.link);
	wl_list_remove(&view3.link);

	destroy_test_server(server);
	printf("  PASS: test_reconfigure_refreshes_decorations\n");
}

/*
 * Test 12: wm_server_reconfigure without toolbar and slit.
 */
static void
test_reconfigure_no_toolbar_slit(void)
{
	reset_globals();
	struct wm_server *server = make_test_server();
	server->config = config_create();
	server->toolbar = NULL;
	server->slit = NULL;
	wm_workspaces_init(server, 1);

	wm_server_reconfigure(server);

	assert(g_toolbar_relayout_count == 0);
	assert(g_slit_relayout_count == 0);

	destroy_test_server(server);
	printf("  PASS: test_reconfigure_no_toolbar_slit\n");
}

/*
 * Test 13: wm_server_reconfigure with NULL style skips style reload.
 */
static void
test_reconfigure_null_style(void)
{
	reset_globals();
	struct wm_server *server = make_test_server();
	server->config = config_create();
	server->config->style_file = strdup("/tmp/style");
	server->style = NULL;
	wm_workspaces_init(server, 1);

	wm_server_reconfigure(server);

	assert(g_style_load_count == 0);
	assert(g_style_load_overlay_count == 0);
	/* No style_changed IPC broadcast either */
	assert(g_ipc_broadcast_count == 1); /* only config_reloaded */

	destroy_test_server(server);
	printf("  PASS: test_reconfigure_null_style\n");
}

/*
 * Test 14: wm_server_reconfigure without menu_file skips menu load.
 */
static void
test_reconfigure_no_menu_file(void)
{
	reset_globals();
	struct wm_server *server = make_test_server();
	server->config = config_create();
	server->config->menu_file = NULL;
	wm_workspaces_init(server, 1);

	wm_server_reconfigure(server);

	assert(g_menu_load_count == 0);
	/* Window menu is always created */
	assert(g_menu_create_window_menu_count == 1);

	destroy_test_server(server);
	printf("  PASS: test_reconfigure_no_menu_file\n");
}

/*
 * Test 15: wm_server_reconfigure without apps_file skips rules load.
 */
static void
test_reconfigure_no_apps_file(void)
{
	reset_globals();
	struct wm_server *server = make_test_server();
	server->config = config_create();
	server->config->apps_file = NULL;
	wm_workspaces_init(server, 1);

	wm_server_reconfigure(server);

	assert(g_rules_load_count == 0);
	assert(g_rules_finish_count == 1);
	assert(g_rules_init_count >= 1);

	destroy_test_server(server);
	printf("  PASS: test_reconfigure_no_apps_file\n");
}

/*
 * Test 16: wm_server_init succeeds with all stubs returning success.
 */
static void
test_server_init_success(void)
{
	reset_globals();
	struct wm_server server;
	memset(&server, 0, sizeof(server));

	bool ok = wm_server_init(&server);
	assert(ok == true);
	assert(server.wl_display != NULL);
	assert(server.backend != NULL);
	assert(server.renderer != NULL);
	assert(server.allocator != NULL);
	assert(server.compositor != NULL);
	assert(server.scene != NULL);
	assert(server.xdg_shell != NULL);
	assert(server.seat != NULL);
	assert(server.current_keymode != NULL);
	assert(strcmp(server.current_keymode, "default") == 0);

	wm_server_destroy(&server);
	printf("  PASS: test_server_init_success\n");
}

/*
 * Test 17: wm_server_init fails when wl_display_create fails.
 */
static void
test_server_init_display_fail(void)
{
	reset_globals();
	g_display_create_fail = true;

	struct wm_server server;
	memset(&server, 0, sizeof(server));

	bool ok = wm_server_init(&server);
	assert(ok == false);

	printf("  PASS: test_server_init_display_fail\n");
}

/*
 * Test 18: wm_server_init fails when backend fails.
 */
static void
test_server_init_backend_fail(void)
{
	reset_globals();
	g_backend_create_fail = true;

	struct wm_server server;
	memset(&server, 0, sizeof(server));

	bool ok = wm_server_init(&server);
	assert(ok == false);

	printf("  PASS: test_server_init_backend_fail\n");
}

/*
 * Test 19: wm_server_init fails when renderer fails.
 */
static void
test_server_init_renderer_fail(void)
{
	reset_globals();
	g_renderer_create_fail = true;

	struct wm_server server;
	memset(&server, 0, sizeof(server));

	bool ok = wm_server_init(&server);
	assert(ok == false);

	printf("  PASS: test_server_init_renderer_fail\n");
}

/*
 * Test 20: wm_server_init fails when allocator fails.
 */
static void
test_server_init_allocator_fail(void)
{
	reset_globals();
	g_allocator_create_fail = true;

	struct wm_server server;
	memset(&server, 0, sizeof(server));

	bool ok = wm_server_init(&server);
	assert(ok == false);

	printf("  PASS: test_server_init_allocator_fail\n");
}

/*
 * Test 21: wm_server_init fails when compositor fails.
 */
static void
test_server_init_compositor_fail(void)
{
	reset_globals();
	g_compositor_create_fail = true;

	struct wm_server server;
	memset(&server, 0, sizeof(server));

	bool ok = wm_server_init(&server);
	assert(ok == false);

	printf("  PASS: test_server_init_compositor_fail\n");
}

/*
 * Test 22: wm_server_init fails when scene fails.
 */
static void
test_server_init_scene_fail(void)
{
	reset_globals();
	g_scene_create_fail = true;

	struct wm_server server;
	memset(&server, 0, sizeof(server));

	bool ok = wm_server_init(&server);
	assert(ok == false);

	printf("  PASS: test_server_init_scene_fail\n");
}

/*
 * Test 23: wm_server_init fails when xdg_shell fails.
 */
static void
test_server_init_xdg_shell_fail(void)
{
	reset_globals();
	g_xdg_shell_create_fail = true;

	struct wm_server server;
	memset(&server, 0, sizeof(server));

	bool ok = wm_server_init(&server);
	assert(ok == false);

	printf("  PASS: test_server_init_xdg_shell_fail\n");
}

/*
 * Test 24: wm_server_init fails when seat fails.
 */
static void
test_server_init_seat_fail(void)
{
	reset_globals();
	g_seat_create_fail = true;

	struct wm_server server;
	memset(&server, 0, sizeof(server));

	bool ok = wm_server_init(&server);
	assert(ok == false);

	printf("  PASS: test_server_init_seat_fail\n");
}

/*
 * Test 25: wm_server_start succeeds.
 */
static void
test_server_start_success(void)
{
	reset_globals();
	struct wm_server server;
	memset(&server, 0, sizeof(server));
	server.wl_display = &g_wl_display;

	bool ok = wm_server_start(&server);
	assert(ok == true);
	assert(server.socket != NULL);

	printf("  PASS: test_server_start_success\n");
}

/*
 * Test 26: wm_server_start fails when backend_start fails.
 */
static void
test_server_start_backend_fail(void)
{
	reset_globals();
	g_backend_start_fail = true;

	struct wm_server server;
	memset(&server, 0, sizeof(server));
	server.wl_display = &g_wl_display;

	bool ok = wm_server_start(&server);
	assert(ok == false);

	printf("  PASS: test_server_start_backend_fail\n");
}

/*
 * Test 27: wm_server_destroy runs without crashing.
 */
static void
test_server_destroy(void)
{
	reset_globals();

	/* Create a server via init, then destroy it */
	struct wm_server server;
	memset(&server, 0, sizeof(server));
	bool ok = wm_server_init(&server);
	assert(ok == true);

	wm_server_destroy(&server);

	/* Verify signal sources were removed (4 signals) */
	assert(g_wl_event_source_remove_count >= 4);

	printf("  PASS: test_server_destroy\n");
}

/*
 * Test 28: wm_server_reconfigure with keybind_load failure path.
 */
static void
test_reconfigure_keybind_load_fail(void)
{
	reset_globals();
	g_keybind_load_return = -1;

	struct wm_server *server = make_test_server();
	server->config = config_create();
	server->config->keys_file = strdup("/tmp/keys");
	wm_workspaces_init(server, 1);

	wm_server_reconfigure(server);

	/* keybind_load should still be called */
	assert(g_keybind_load_count == 1);
	/* The rest should proceed normally */
	assert(g_keyboard_apply_config_count == 1);

	destroy_test_server(server);
	printf("  PASS: test_reconfigure_keybind_load_fail\n");
}

/*
 * Test 29: wm_server_reconfigure without keys_file skips keybind load.
 */
static void
test_reconfigure_no_keys_file(void)
{
	reset_globals();
	struct wm_server *server = make_test_server();
	server->config = config_create();
	server->config->keys_file = NULL;
	wm_workspaces_init(server, 1);

	wm_server_reconfigure(server);

	assert(g_keybind_load_count == 0);
	assert(g_mousebind_load_count == 0);

	destroy_test_server(server);
	printf("  PASS: test_reconfigure_no_keys_file\n");
}

/*
 * Test 30: wm_server_destroy with auto_raise_timer set.
 */
static void
test_server_destroy_with_auto_raise(void)
{
	reset_globals();

	struct wm_server server;
	memset(&server, 0, sizeof(server));
	bool ok = wm_server_init(&server);
	assert(ok == true);

	/* Set auto_raise_timer */
	struct wl_event_source timer = {0};
	server.auto_raise_timer = &timer;

	wm_server_destroy(&server);

	/* auto_raise_timer should be removed too */
	assert(g_wl_event_source_remove_count >= 5);

	printf("  PASS: test_server_destroy_with_auto_raise\n");
}

/*
 * Test 31: wm_server_destroy with chain_state timeout.
 */
static void
test_server_destroy_with_chain_timeout(void)
{
	reset_globals();

	struct wm_server server;
	memset(&server, 0, sizeof(server));
	bool ok = wm_server_init(&server);
	assert(ok == true);

	struct wl_event_source chain_timer = {0};
	server.chain_state.timeout = &chain_timer;

	wm_server_destroy(&server);

	/* chain timeout should be removed too */
	assert(g_wl_event_source_remove_count >= 5);

	printf("  PASS: test_server_destroy_with_chain_timeout\n");
}

/*
 * Test 32: wm_server_reconfigure updates current_keymode.
 */
static void
test_reconfigure_resets_keymode(void)
{
	reset_globals();
	struct wm_server *server = make_test_server();
	server->config = config_create();
	wm_workspaces_init(server, 1);

	/* Set a non-default keymode */
	free(server->current_keymode);
	server->current_keymode = strdup("custom_mode");

	wm_server_reconfigure(server);

	/* Should be reset to "default" */
	assert(server->current_keymode != NULL);
	assert(strcmp(server->current_keymode, "default") == 0);

	destroy_test_server(server);
	printf("  PASS: test_reconfigure_resets_keymode\n");
}

int
main(void)
{
	printf("test_server:\n");

	test_handle_signal();
	test_handle_sighup();
	test_handle_sigchld();
	test_reconfigure_null_config();
	test_reconfigure_full_config();
	test_reconfigure_no_overlay();
	test_reconfigure_clears_chain_timeout();
	test_reconfigure_per_output_mode();
	test_reconfigure_global_mode();
	test_reconfigure_workspace_names();
	test_reconfigure_refreshes_decorations();
	test_reconfigure_no_toolbar_slit();
	test_reconfigure_null_style();
	test_reconfigure_no_menu_file();
	test_reconfigure_no_apps_file();
	test_server_init_success();
	test_server_init_display_fail();
	test_server_init_backend_fail();
	test_server_init_renderer_fail();
	test_server_init_allocator_fail();
	test_server_init_compositor_fail();
	test_server_init_scene_fail();
	test_server_init_xdg_shell_fail();
	test_server_init_seat_fail();
	test_server_start_success();
	test_server_start_backend_fail();
	test_server_destroy();
	test_reconfigure_keybind_load_fail();
	test_reconfigure_no_keys_file();
	test_server_destroy_with_auto_raise();
	test_server_destroy_with_chain_timeout();
	test_reconfigure_resets_keymode();

	printf("All server tests passed.\n");
	return 0;
}
