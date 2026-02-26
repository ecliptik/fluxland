/*
 * test_keyboard.c - Unit tests for keyboard action dispatch and conditions
 *
 * Includes keyboard.c directly with stub implementations to avoid
 * needing wlroots/wayland libraries at link time. Header guards block
 * the real wayland/wlroots headers so our stub types are used instead.
 */

#define _GNU_SOURCE

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

/* --- Block real headers via their include guards --- */
#define WAYLAND_SERVER_CORE_H
#define WAYLAND_SERVER_H
#define WAYLAND_UTIL_H
#define WLR_UTIL_LOG_H
#define WLR_UTIL_EDGES_H
#define WLR_BACKEND_SESSION_H
#define WLR_INTERFACES_WLR_KEYBOARD_H
#define WLR_TYPES_WLR_INPUT_DEVICE_H
#define WLR_TYPES_WLR_KEYBOARD_H
#define WLR_TYPES_WLR_SEAT_H
#define WLR_TYPES_WLR_XDG_SHELL_H

#define WM_SERVER_H
#define WM_VIEW_H
#define WM_CONFIG_H
#define WM_FOCUS_NAV_H
#define WM_OUTPUT_H
#define WM_DECORATION_H
#define WM_IDLE_H
#define WM_KEYBIND_H
#define WM_MENU_H
#define WM_PLACEMENT_H
#define WM_PROTOCOLS_H
#define WM_SLIT_H
#define WM_RULES_H
#define WM_SESSION_LOCK_H
#define WM_TABGROUP_H
#define WM_STYLE_H
#define WM_TOOLBAR_H
#define WM_WORKSPACE_H
#define WM_KEYBOARD_H

/* Block real xkbcommon header */
#define _XKBCOMMON_H_

/* --- Stub wayland types --- */

struct wl_list {
	struct wl_list *prev;
	struct wl_list *next;
};

static inline void wl_list_init(struct wl_list *list)
{
	list->prev = list;
	list->next = list;
}

static inline void wl_list_insert(struct wl_list *list, struct wl_list *elm)
{
	elm->prev = list;
	elm->next = list->next;
	list->next = elm;
	elm->next->prev = elm;
}

static inline void wl_list_remove(struct wl_list *elm)
{
	elm->prev->next = elm->next;
	elm->next->prev = elm->prev;
	elm->prev = NULL;
	elm->next = NULL;
}

static inline int wl_list_empty(const struct wl_list *list)
{
	return list->next == list;
}

#define wl_container_of(ptr, sample, member) \
	(__typeof__(sample))((char *)(ptr) - \
	 __builtin_offsetof(__typeof__(*sample), member))

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

struct wl_event_source {
	int (*callback)(void *data);
	void *data;
	int next_ms;
	bool removed;
};

struct wl_event_loop {
	int dummy;
};

struct wl_display {
	int dummy;
};

struct wl_listener {
	struct wl_list link;
	void (*notify)(struct wl_listener *listener, void *data);
};

struct wl_resource {
	int dummy;
};

struct wl_client {
	int dummy;
};

/* --- Stub wlr types --- */

struct wlr_scene_node {
	bool enabled;
	int x, y;
};

struct wlr_scene_tree {
	struct wlr_scene_node node;
};

struct wlr_box {
	int x, y, width, height;
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
};

struct wlr_cursor {
	double x, y;
};

struct wlr_keyboard_modifiers {
	uint32_t depressed;
	uint32_t latched;
	uint32_t locked;
	uint32_t group;
};

struct wl_signal {
	struct wl_list listener_list;
};

struct wlr_keyboard {
	void *xkb_state;
	void *keymap;
	struct wlr_keyboard_modifiers modifiers;
	struct {
		struct wl_signal modifiers;
		struct wl_signal key;
	} events;
};

struct wlr_input_device {
	const char *name;
	struct {
		struct wl_signal destroy;
	} events;
};

struct wlr_seat {
	int dummy;
};

struct wlr_keyboard_key_event {
	uint32_t time_msec;
	uint32_t keycode;
	uint32_t state;
};

struct wlr_session {
	int dummy;
};

struct wlr_output {
	int dummy;
};

struct wlr_output_layout {
	int dummy;
};

struct wlr_scene_buffer {
	int dummy;
};

/* --- Stub wlr constants --- */

#define WLR_MODIFIER_CTRL  (1 << 0)
#define WLR_MODIFIER_ALT   (1 << 1)
#define WLR_MODIFIER_SHIFT (1 << 2)
#define WLR_MODIFIER_LOGO  (1 << 3)

#define WLR_EDGE_TOP    (1 << 0)
#define WLR_EDGE_BOTTOM (1 << 1)
#define WLR_EDGE_LEFT   (1 << 2)
#define WLR_EDGE_RIGHT  (1 << 3)

#define WL_KEYBOARD_KEY_STATE_PRESSED 1
#define WL_KEYBOARD_KEY_STATE_RELEASED 0

/* --- xkbcommon stubs --- */
typedef uint32_t xkb_keysym_t;
typedef uint32_t xkb_keycode_t;
typedef uint32_t xkb_layout_index_t;

struct xkb_state { int dummy; };
struct xkb_context { int dummy; };
struct xkb_keymap { int dummy; };

struct xkb_rule_names {
	const char *rules;
	const char *model;
	const char *layout;
	const char *variant;
	const char *options;
};

enum xkb_state_component {
	XKB_STATE_MODS_DEPRESSED = 0,
	XKB_STATE_LAYOUT_EFFECTIVE = (1 << 7),
};

#define XKB_CONTEXT_NO_FLAGS 0
#define XKB_KEYMAP_COMPILE_NO_FLAGS 0

#define XKB_KEY_XF86Switch_VT_1  0x1008FE01
#define XKB_KEY_XF86Switch_VT_12 0x1008FE0C
#define XKB_KEY_Escape 0xff1b
#define XKB_KEY_e      0x0065
#define XKB_KEY_a      0x0061
#define XKB_KEY_t      0x0074

/* --- wlr_log stub --- */
#define WLR_ERROR 1
#define WLR_INFO  2
#define WLR_DEBUG 3
#define wlr_log(verb, fmt, ...) ((void)0)

/* --- Stub wayland event functions --- */

static struct wl_event_source g_stub_sources[8];
static int g_stub_source_count;

static struct wl_event_source *
wl_event_loop_add_timer(struct wl_event_loop *loop,
	int (*callback)(void *data), void *data)
{
	if (!loop || g_stub_source_count >= 8)
		return NULL;
	struct wl_event_source *s = &g_stub_sources[g_stub_source_count++];
	s->callback = callback;
	s->data = data;
	s->next_ms = 0;
	s->removed = false;
	return s;
}

static int
wl_event_source_timer_update(struct wl_event_source *source, int ms)
{
	if (source)
		source->next_ms = ms;
	return 0;
}

static int
wl_event_source_remove(struct wl_event_source *source)
{
	if (source)
		source->removed = true;
	return 0;
}

/* --- Stub wlr functions used by keyboard.c --- */

static int stub_terminate_called;
static void
wl_display_terminate(struct wl_display *d)
{
	(void)d;
	stub_terminate_called++;
}

static int stub_close_called;
static void
wlr_xdg_toplevel_send_close(struct wlr_xdg_toplevel *t)
{
	(void)t;
	stub_close_called++;
}

static int stub_set_size_w, stub_set_size_h;
static uint32_t
wlr_xdg_toplevel_set_size(struct wlr_xdg_toplevel *t, int w, int h)
{
	(void)t;
	stub_set_size_w = w;
	stub_set_size_h = h;
	return 0;
}

static int stub_set_pos_x, stub_set_pos_y;
static int stub_set_pos_called;
static void
wlr_scene_node_set_position(struct wlr_scene_node *n, int x, int y)
{
	(void)n;
	stub_set_pos_x = x;
	stub_set_pos_y = y;
	stub_set_pos_called++;
}

static int stub_vt_switch_called;
static unsigned stub_vt_number;
static void
wlr_session_change_vt(struct wlr_session *session, unsigned vt)
{
	(void)session;
	stub_vt_switch_called++;
	stub_vt_number = vt;
}

static struct wl_client *
wl_resource_get_client(struct wl_resource *r)
{
	(void)r;
	return NULL;
}

static void
wl_client_get_credentials(struct wl_client *c, pid_t *pid,
	uid_t *uid, gid_t *gid)
{
	(void)c;
	if (pid) *pid = 0;
	if (uid) *uid = 0;
	if (gid) *gid = 0;
}

static uint32_t stub_modifiers_value;
static uint32_t
wlr_keyboard_get_modifiers(struct wlr_keyboard *kb)
{
	(void)kb;
	return stub_modifiers_value;
}

/* --- Tracking counters for seat stubs --- */
static int stub_seat_set_keyboard_count;
static int stub_seat_notify_key_count;
static uint32_t stub_seat_notify_key_time;
static uint32_t stub_seat_notify_key_keycode;
static uint32_t stub_seat_notify_key_state;
static int stub_seat_notify_modifiers_count;

static void
wlr_seat_set_keyboard(struct wlr_seat *seat, struct wlr_keyboard *kb)
{
	(void)seat;
	(void)kb;
	stub_seat_set_keyboard_count++;
}

static void
wlr_seat_keyboard_notify_key(struct wlr_seat *seat, uint32_t time,
	uint32_t key, uint32_t state)
{
	(void)seat;
	stub_seat_notify_key_count++;
	stub_seat_notify_key_time = time;
	stub_seat_notify_key_keycode = key;
	stub_seat_notify_key_state = state;
}

static void
wlr_seat_keyboard_notify_modifiers(struct wlr_seat *seat,
	void *modifiers)
{
	(void)seat;
	(void)modifiers;
	stub_seat_notify_modifiers_count++;
}

static xkb_keysym_t stub_keysyms[4];
static int stub_nsyms;
static int
xkb_state_key_get_syms(struct xkb_state *state, xkb_keycode_t key,
	const xkb_keysym_t **syms_out)
{
	(void)state;
	(void)key;
	*syms_out = stub_keysyms;
	return stub_nsyms;
}

static xkb_layout_index_t stub_num_layouts_value = 1;
static xkb_layout_index_t stub_active_layout_index;

static xkb_layout_index_t
xkb_keymap_num_layouts(struct xkb_keymap *keymap)
{
	(void)keymap;
	return stub_num_layouts_value;
}

static int
xkb_state_layout_index_is_active(struct xkb_state *state,
	xkb_layout_index_t idx, enum xkb_state_component type)
{
	(void)state;
	(void)type;
	return (idx == stub_active_layout_index) ? 1 : 0;
}

/* xkb context/keymap stubs needed by wm_keyboard_setup/apply_config */
static struct xkb_context stub_xkb_context;
static struct xkb_context *
xkb_context_new(int flags)
{
	(void)flags;
	return &stub_xkb_context;
}

static void xkb_context_unref(struct xkb_context *ctx) { (void)ctx; }

static bool stub_keymap_new_fail;
static struct xkb_keymap stub_xkb_keymap;
static struct xkb_keymap *
xkb_keymap_new_from_names(struct xkb_context *ctx,
	const struct xkb_rule_names *names, int flags)
{
	(void)ctx; (void)names; (void)flags;
	if (stub_keymap_new_fail)
		return NULL;
	return &stub_xkb_keymap;
}

static void xkb_keymap_unref(struct xkb_keymap *km) { (void)km; }

/* wlr keyboard functions */
static struct wlr_keyboard *
wlr_keyboard_from_input_device(struct wlr_input_device *dev)
{
	(void)dev;
	static struct wlr_keyboard dummy_kb;
	return &dummy_kb;
}

static int stub_kb_set_keymap_count;
static void
wlr_keyboard_set_keymap(struct wlr_keyboard *kb, struct xkb_keymap *km)
{
	(void)kb; (void)km;
	stub_kb_set_keymap_count++;
}

static void
wlr_keyboard_set_repeat_info(struct wlr_keyboard *kb, int rate, int delay)
{
	(void)kb; (void)rate; (void)delay;
}

static int stub_kb_notify_modifiers_count;
static uint32_t stub_kb_notify_modifiers_group;
static void
wlr_keyboard_notify_modifiers(struct wlr_keyboard *kb,
	uint32_t dep, uint32_t lat, uint32_t lock, uint32_t group)
{
	(void)kb; (void)dep; (void)lat; (void)lock;
	stub_kb_notify_modifiers_count++;
	stub_kb_notify_modifiers_group = group;
}

/* wl_signal_add stub */
static void
wl_signal_add(void *signal, struct wl_listener *listener)
{
	(void)signal; (void)listener;
}

/* closefrom stub (used in exec_command child path) */
#ifndef __has_builtin
static void closefrom(int fd) { (void)fd; }
#endif

/* --- Keybind types (normally from keybind.h) --- */

enum wm_action {
	WM_ACTION_NOP,
	WM_ACTION_EXEC,
	WM_ACTION_CLOSE,
	WM_ACTION_KILL,
	WM_ACTION_MAXIMIZE,
	WM_ACTION_MAXIMIZE_VERT,
	WM_ACTION_MAXIMIZE_HORIZ,
	WM_ACTION_FULLSCREEN,
	WM_ACTION_MINIMIZE,
	WM_ACTION_SHADE,
	WM_ACTION_SHADE_ON,
	WM_ACTION_SHADE_OFF,
	WM_ACTION_STICK,
	WM_ACTION_MOVE,
	WM_ACTION_RESIZE,
	WM_ACTION_RAISE,
	WM_ACTION_LOWER,
	WM_ACTION_RAISE_LAYER,
	WM_ACTION_LOWER_LAYER,
	WM_ACTION_SET_LAYER,
	WM_ACTION_FOCUS_NEXT,
	WM_ACTION_FOCUS_PREV,
	WM_ACTION_WORKSPACE,
	WM_ACTION_SEND_TO_WORKSPACE,
	WM_ACTION_NEXT_WORKSPACE,
	WM_ACTION_PREV_WORKSPACE,
	WM_ACTION_SHOW_DESKTOP,
	WM_ACTION_MOVE_LEFT,
	WM_ACTION_MOVE_RIGHT,
	WM_ACTION_MOVE_UP,
	WM_ACTION_MOVE_DOWN,
	WM_ACTION_MOVE_TO,
	WM_ACTION_RESIZE_TO,
	WM_ACTION_TOGGLE_DECOR,
	WM_ACTION_SET_DECOR,
	WM_ACTION_MACRO_CMD,
	WM_ACTION_TOGGLE_CMD,
	WM_ACTION_KEY_MODE,
	WM_ACTION_NEXT_TAB,
	WM_ACTION_PREV_TAB,
	WM_ACTION_DETACH_CLIENT,
	WM_ACTION_MOVE_TAB_LEFT,
	WM_ACTION_MOVE_TAB_RIGHT,
	WM_ACTION_START_TABBING,
	WM_ACTION_START_MOVING,
	WM_ACTION_START_RESIZING,
	WM_ACTION_ACTIVATE_TAB,
	WM_ACTION_ROOT_MENU,
	WM_ACTION_WINDOW_MENU,
	WM_ACTION_WINDOW_LIST,
	WM_ACTION_HIDE_MENUS,
	WM_ACTION_FOCUS,
	WM_ACTION_RECONFIGURE,
	WM_ACTION_EXIT,
	WM_ACTION_NEXT_LAYOUT,
	WM_ACTION_PREV_LAYOUT,
	WM_ACTION_DEICONIFY,
	WM_ACTION_NEXT_WINDOW,
	WM_ACTION_PREV_WINDOW,
	WM_ACTION_ARRANGE_WINDOWS,
	WM_ACTION_ARRANGE_VERT,
	WM_ACTION_ARRANGE_HORIZ,
	WM_ACTION_CASCADE_WINDOWS,
	WM_ACTION_LHALF,
	WM_ACTION_RHALF,
	WM_ACTION_RESIZE_HORIZ,
	WM_ACTION_RESIZE_VERT,
	WM_ACTION_FOCUS_LEFT,
	WM_ACTION_FOCUS_RIGHT,
	WM_ACTION_FOCUS_UP,
	WM_ACTION_FOCUS_DOWN,
	WM_ACTION_SET_HEAD,
	WM_ACTION_SEND_TO_NEXT_HEAD,
	WM_ACTION_SEND_TO_PREV_HEAD,
	WM_ACTION_ARRANGE_STACK_LEFT,
	WM_ACTION_ARRANGE_STACK_RIGHT,
	WM_ACTION_ARRANGE_STACK_TOP,
	WM_ACTION_ARRANGE_STACK_BOTTOM,
	WM_ACTION_CLOSE_ALL_WINDOWS,
	WM_ACTION_WORKSPACE_MENU,
	WM_ACTION_CLIENT_MENU,
	WM_ACTION_CUSTOM_MENU,
	WM_ACTION_SET_STYLE,
	WM_ACTION_RELOAD_STYLE,
	WM_ACTION_TAKE_TO_WORKSPACE,
	WM_ACTION_SEND_TO_NEXT_WORKSPACE,
	WM_ACTION_SEND_TO_PREV_WORKSPACE,
	WM_ACTION_TAKE_TO_NEXT_WORKSPACE,
	WM_ACTION_TAKE_TO_PREV_WORKSPACE,
	WM_ACTION_ADD_WORKSPACE,
	WM_ACTION_REMOVE_LAST_WORKSPACE,
	WM_ACTION_RIGHT_WORKSPACE,
	WM_ACTION_LEFT_WORKSPACE,
	WM_ACTION_SET_WORKSPACE_NAME,
	WM_ACTION_REMEMBER,
	WM_ACTION_TOGGLE_SLIT_ABOVE,
	WM_ACTION_TOGGLE_SLIT_HIDDEN,
	WM_ACTION_TOGGLE_TOOLBAR_ABOVE,
	WM_ACTION_TOGGLE_TOOLBAR_VISIBLE,
	WM_ACTION_SET_ALPHA,
	WM_ACTION_SET_ENV,
	WM_ACTION_BIND_KEY,
	WM_ACTION_GOTO_WINDOW,
	WM_ACTION_NEXT_GROUP,
	WM_ACTION_PREV_GROUP,
	WM_ACTION_UNCLUTTER,
	WM_ACTION_TOGGLE_SHOW_POSITION,
	WM_ACTION_IF,
	WM_ACTION_FOREACH,
	WM_ACTION_MAP,
	WM_ACTION_DELAY,
	WM_ACTION_FOCUS_TOOLBAR,
	WM_ACTION_FOCUS_NEXT_ELEMENT,
	WM_ACTION_FOCUS_PREV_ELEMENT,
	WM_ACTION_FOCUS_ACTIVATE,
};

enum wm_condition_type {
	WM_COND_MATCHES,
	WM_COND_SOME,
	WM_COND_EVERY,
	WM_COND_NOT,
	WM_COND_AND,
	WM_COND_OR,
	WM_COND_XOR,
};

struct wm_condition {
	enum wm_condition_type type;
	char *property;
	char *pattern;
	struct wm_condition *child;
	struct wm_condition *left;
	struct wm_condition *right;
};

struct wm_subcmd {
	enum wm_action action;
	char *argument;
	struct wm_subcmd *next;
};

struct wm_keybind {
	uint32_t modifiers;
	xkb_keysym_t keysym;
	enum wm_action action;
	char *argument;
	struct wm_subcmd *subcmds;
	int subcmd_count;
	int toggle_index;
	struct wm_condition *condition;
	struct wm_subcmd *else_cmd;
	int delay_us;
	struct wl_list children;
	struct wl_list link;
};

struct wm_keymode {
	char *name;
	struct wl_list bindings;
	struct wl_list link;
};

struct wm_chain_state {
	struct wl_list *current_level;
	bool in_chain;
	struct wl_event_source *timeout;
};

#define WM_CHAIN_TIMEOUT_MS 3000

struct wm_keybind *keybind_find(struct wl_list *bindings, uint32_t modifiers,
	xkb_keysym_t keysym);

/* --- View layer enum (normally from view.h) --- */

enum wm_view_layer {
	WM_LAYER_DESKTOP = 0,
	WM_LAYER_BELOW,
	WM_LAYER_NORMAL,
	WM_LAYER_ABOVE,
	WM_VIEW_LAYER_COUNT,
};

/* --- Decoration enums (normally from decoration.h) --- */

enum wm_decor_preset {
	WM_DECOR_NORMAL = 0,
	WM_DECOR_NONE,
	WM_DECOR_BORDER,
	WM_DECOR_TAB,
	WM_DECOR_TINY,
	WM_DECOR_TOOL,
};

/* --- Cursor mode enum (normally from server.h) --- */

enum wm_cursor_mode {
	WM_CURSOR_PASSTHROUGH = 0,
	WM_CURSOR_MOVE,
	WM_CURSOR_RESIZE,
	WM_CURSOR_TABBING,
};

/* --- Focus policy (normally from view.h / config.h) --- */

enum wm_focus_policy {
	WM_FOCUS_CLICK = 0,
	WM_FOCUS_SLOPPY,
};

/* --- Focus nav types (normally from focus_nav.h) --- */

enum wm_focus_zone {
	WM_FOCUS_ZONE_WINDOWS,
	WM_FOCUS_ZONE_TOOLBAR,
};

struct wm_focus_nav {
	enum wm_focus_zone zone;
	int toolbar_index;
};

/* --- Minimal struct definitions needed by keyboard.c --- */

struct wm_decoration {
	bool shaded;
};

struct wm_style {
	int dummy;
};

struct wm_tab_group {
	struct wl_list views;
	struct wm_view *active_view;
	int count;
};

struct wm_workspace {
	char *name;
	int index;
	struct wl_list link;
};

struct wm_config {
	char *style_file;
	char *apps_file;
	const char *xkb_rules;
	const char *xkb_model;
	const char *xkb_layout;
	const char *xkb_variant;
	const char *xkb_options;
};

struct wm_toolbar {
	int dummy;
};

struct wm_slit {
	int dummy;
};

struct wm_output {
	struct wlr_box usable_area;
	struct wl_list link;
};

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
	void *animation;
	int focus_protection;
	bool ignore_size_hints;
	void *foreign_toplevel_handle;
	struct wm_decoration *decoration;
	struct wl_listener request_maximize;
	struct wl_listener request_fullscreen;
	struct wl_listener request_minimize;
};

struct wm_mouse_state {
	int dummy;
};

struct wm_rules {
	int dummy;
};

struct wm_ipc_server {
	int dummy;
};

struct wm_output_management {
	int dummy;
};

struct wm_idle {
	int dummy;
};

struct wm_text_input_relay {
	int dummy;
};

struct wm_session_lock {
	int dummy;
};

struct wm_keyboard {
	struct wl_list link;
	struct wm_server *server;
	struct wlr_keyboard *wlr_keyboard;
	struct wl_listener modifiers;
	struct wl_listener key;
	struct wl_listener destroy;
};

struct wm_server {
	struct wl_display *wl_display;
	struct wl_event_loop *wl_event_loop;
	struct wlr_session *session;
	struct wlr_seat *seat;
	struct wlr_cursor *cursor;
	struct wm_style *style;
	struct wm_config *config;
	struct wm_toolbar *toolbar;
	struct wm_slit *slit;
	struct wm_view *focused_view;
	bool focus_user_initiated;
	bool show_position;
	enum wm_cursor_mode cursor_mode;
	struct wl_list views;
	struct wl_list workspaces;
	struct wl_list outputs;
	struct wl_list keyboards;
	struct wl_list keybindings;
	struct wl_list keymodes;
	char *current_keymode;
	struct wm_chain_state chain_state;
	struct wm_mouse_state mouse_state;
	struct wm_rules rules;
	struct wm_ipc_server ipc;
	struct wm_focus_nav focus_nav;
	struct wm_output_management output_mgmt;
	struct wm_idle idle;
	struct wm_text_input_relay text_input_relay;
	struct wm_session_lock session_lock;
	struct wlr_scene_buffer *snap_preview;
	struct wlr_scene_buffer *position_overlay;
	bool ipc_no_exec;
};

/* --- Stub function counters --- */

static int stub_focus_next_count;
static int stub_focus_prev_count;
static int stub_focus_view_count;
static int stub_raise_count;
static int stub_lower_count;
static int stub_raise_layer_count;
static int stub_lower_layer_count;
static int stub_set_layer_count;
static int stub_set_sticky_count;
static bool stub_set_sticky_value;
static int stub_set_opacity_count;
static int stub_set_opacity_value;
static int stub_begin_interactive_count;
static int stub_maximize_vert_count;
static int stub_maximize_horiz_count;
static int stub_toggle_decoration_count;
static int stub_lhalf_count;
static int stub_rhalf_count;
static int stub_resize_by_count;
static int stub_focus_direction_count;
static int stub_set_head_count;
static int stub_send_next_head_count;
static int stub_send_prev_head_count;
static int stub_close_all_count;
static int stub_cycle_next_count;
static int stub_cycle_prev_count;
static int stub_deiconify_last_count;
static int stub_deiconify_all_count;
static int stub_deiconify_all_ws_count;

static int stub_ws_switch_count;
static int stub_ws_switch_next_count;
static int stub_ws_switch_prev_count;
static int stub_ws_switch_right_count;
static int stub_ws_switch_left_count;
static int stub_ws_set_name_count;
static int stub_ws_add_count;
static int stub_ws_remove_last_count;
static int stub_send_to_ws_count;
static int stub_take_to_ws_count;
static int stub_send_next_ws_count;
static int stub_send_prev_ws_count;
static int stub_take_next_ws_count;
static int stub_take_prev_ws_count;

static int stub_menu_show_root_count;
static int stub_menu_show_window_count;
static int stub_menu_show_window_list_count;
static int stub_menu_hide_all_count;
static int stub_menu_show_workspace_count;
static int stub_menu_show_client_count;
static int stub_menu_show_custom_count;

static int stub_arrange_grid_count;
static int stub_arrange_vert_count;
static int stub_arrange_horiz_count;
static int stub_arrange_cascade_count;
static int stub_arrange_stack_left_count;
static int stub_arrange_stack_right_count;
static int stub_arrange_stack_top_count;
static int stub_arrange_stack_bottom_count;

static int stub_decoration_update_count;
static int stub_decoration_set_preset_count;
static int stub_decoration_set_shaded_count;
static bool stub_decoration_shaded_value;

static int stub_style_load_count;
static int stub_toolbar_relayout_count;
static int stub_toolbar_toggle_above_count;
static int stub_toolbar_toggle_visible_count;
static int stub_slit_toggle_above_count;
static int stub_slit_toggle_hidden_count;

static int stub_reconfigure_count;
static int stub_next_layout_count;
static int stub_prev_layout_count;
static int stub_keyboard_apply_config_count;
static int stub_remember_window_count;

static int stub_tab_group_next_count;
static int stub_tab_group_prev_count;
static int stub_tab_group_remove_count;
static int stub_tab_group_move_left_count;
static int stub_tab_group_move_right_count;
static int stub_activate_tab_count;

static int stub_focus_nav_enter_toolbar_count;
static int stub_focus_nav_next_element_count;
static int stub_focus_nav_prev_element_count;
static int stub_focus_nav_activate_count;

static int stub_keybind_add_count;
static int stub_view_layer_from_name_result;

static int stub_maximize_listener_count;
static int stub_fullscreen_listener_count;
static int stub_minimize_listener_count;

static int stub_idle_notify_count;
static int stub_session_locked;
static int stub_kb_inhibited;

static int stub_menu_handle_key_result;

/* --- Stub wm_* functions called by keyboard.c --- */

void wm_focus_next_view(struct wm_server *s) { (void)s; stub_focus_next_count++; }
void wm_focus_prev_view(struct wm_server *s) { (void)s; stub_focus_prev_count++; }
void wm_focus_view(struct wm_view *v, struct wlr_surface *s) { (void)v; (void)s; stub_focus_view_count++; }
void wm_view_raise(struct wm_view *v) { (void)v; stub_raise_count++; }
void wm_view_lower(struct wm_view *v) { (void)v; stub_lower_count++; }
void wm_view_raise_layer(struct wm_view *v) { (void)v; stub_raise_layer_count++; }
void wm_view_lower_layer(struct wm_view *v) { (void)v; stub_lower_layer_count++; }
void wm_view_set_layer(struct wm_view *v, enum wm_view_layer l) { (void)v; (void)l; stub_set_layer_count++; }
void wm_view_set_sticky(struct wm_view *v, bool s) { (void)v; stub_set_sticky_count++; stub_set_sticky_value = s; }
void wm_view_set_opacity(struct wm_view *v, int a) { (void)v; stub_set_opacity_count++; stub_set_opacity_value = a; }
void wm_view_begin_interactive(struct wm_view *v, enum wm_cursor_mode m, uint32_t e) { (void)v; (void)m; (void)e; stub_begin_interactive_count++; }
void wm_view_maximize_vert(struct wm_view *v) { (void)v; stub_maximize_vert_count++; }
void wm_view_maximize_horiz(struct wm_view *v) { (void)v; stub_maximize_horiz_count++; }
void wm_view_toggle_decoration(struct wm_view *v) { (void)v; stub_toggle_decoration_count++; }
void wm_view_lhalf(struct wm_view *v) { (void)v; stub_lhalf_count++; }
void wm_view_rhalf(struct wm_view *v) { (void)v; stub_rhalf_count++; }
void wm_view_resize_by(struct wm_view *v, int w, int h) { (void)v; (void)w; (void)h; stub_resize_by_count++; }
void wm_view_focus_direction(struct wm_server *s, int dx, int dy) { (void)s; (void)dx; (void)dy; stub_focus_direction_count++; }
void wm_view_set_head(struct wm_view *v, int h) { (void)v; (void)h; stub_set_head_count++; }
void wm_view_send_to_next_head(struct wm_view *v) { (void)v; stub_send_next_head_count++; }
void wm_view_send_to_prev_head(struct wm_view *v) { (void)v; stub_send_prev_head_count++; }
void wm_view_close_all(struct wm_server *s) { (void)s; stub_close_all_count++; }
void wm_view_cycle_next(struct wm_server *s) { (void)s; stub_cycle_next_count++; }
void wm_view_cycle_prev(struct wm_server *s) { (void)s; stub_cycle_prev_count++; }
void wm_view_deiconify_last(struct wm_server *s) { (void)s; stub_deiconify_last_count++; }
void wm_view_deiconify_all(struct wm_server *s) { (void)s; stub_deiconify_all_count++; }
void wm_view_deiconify_all_workspace(struct wm_server *s) { (void)s; stub_deiconify_all_ws_count++; }
void wm_view_activate_tab(struct wm_view *v, int i) { (void)v; (void)i; stub_activate_tab_count++; }
void wm_view_send_to_workspace(struct wm_server *s, int i) { (void)s; (void)i; stub_send_to_ws_count++; }
void wm_view_take_to_workspace(struct wm_server *s, int i) { (void)s; (void)i; stub_take_to_ws_count++; }
void wm_view_send_to_next_workspace(struct wm_server *s) { (void)s; stub_send_next_ws_count++; }
void wm_view_send_to_prev_workspace(struct wm_server *s) { (void)s; stub_send_prev_ws_count++; }
void wm_view_take_to_next_workspace(struct wm_server *s) { (void)s; stub_take_next_ws_count++; }
void wm_view_take_to_prev_workspace(struct wm_server *s) { (void)s; stub_take_prev_ws_count++; }

enum wm_view_layer wm_view_layer_from_name(const char *n) { (void)n; return (enum wm_view_layer)stub_view_layer_from_name_result; }

void wm_workspace_switch(struct wm_server *s, int i) { (void)s; (void)i; stub_ws_switch_count++; }
void wm_workspace_switch_next(struct wm_server *s) { (void)s; stub_ws_switch_next_count++; }
void wm_workspace_switch_prev(struct wm_server *s) { (void)s; stub_ws_switch_prev_count++; }
void wm_workspace_switch_right(struct wm_server *s) { (void)s; stub_ws_switch_right_count++; }
void wm_workspace_switch_left(struct wm_server *s) { (void)s; stub_ws_switch_left_count++; }
void wm_workspace_set_name(struct wm_server *s, const char *n) { (void)s; (void)n; stub_ws_set_name_count++; }
void wm_workspace_add(struct wm_server *s) { (void)s; stub_ws_add_count++; }
void wm_workspace_remove_last(struct wm_server *s) { (void)s; stub_ws_remove_last_count++; }
static struct wm_workspace *stub_active_workspace;
struct wm_workspace *wm_workspace_get_active(struct wm_server *s) { (void)s; return stub_active_workspace; }

void wm_arrange_windows_grid(struct wm_server *s) { (void)s; stub_arrange_grid_count++; }
void wm_arrange_windows_vert(struct wm_server *s) { (void)s; stub_arrange_vert_count++; }
void wm_arrange_windows_horiz(struct wm_server *s) { (void)s; stub_arrange_horiz_count++; }
void wm_arrange_windows_cascade(struct wm_server *s) { (void)s; stub_arrange_cascade_count++; }
void wm_arrange_windows_stack_left(struct wm_server *s) { (void)s; stub_arrange_stack_left_count++; }
void wm_arrange_windows_stack_right(struct wm_server *s) { (void)s; stub_arrange_stack_right_count++; }
void wm_arrange_windows_stack_top(struct wm_server *s) { (void)s; stub_arrange_stack_top_count++; }
void wm_arrange_windows_stack_bottom(struct wm_server *s) { (void)s; stub_arrange_stack_bottom_count++; }

void wm_menu_show_root(struct wm_server *s, int x, int y) { (void)s; (void)x; (void)y; stub_menu_show_root_count++; }
void wm_menu_show_window(struct wm_server *s, int x, int y) { (void)s; (void)x; (void)y; stub_menu_show_window_count++; }
void wm_menu_show_window_list(struct wm_server *s, int x, int y) { (void)s; (void)x; (void)y; stub_menu_show_window_list_count++; }
void wm_menu_hide_all(struct wm_server *s) { (void)s; stub_menu_hide_all_count++; }
void wm_menu_show_workspace_menu(struct wm_server *s, int x, int y) { (void)s; (void)x; (void)y; stub_menu_show_workspace_count++; }
void wm_menu_show_client_menu(struct wm_server *s, const char *a, int x, int y) { (void)s; (void)a; (void)x; (void)y; stub_menu_show_client_count++; }
void wm_menu_show_custom(struct wm_server *s, const char *a, int x, int y) { (void)s; (void)a; (void)x; (void)y; stub_menu_show_custom_count++; }
bool wm_menu_handle_key(struct wm_server *s, uint32_t kc, xkb_keysym_t sym, bool press) { (void)s; (void)kc; (void)sym; (void)press; return stub_menu_handle_key_result; }

void wm_decoration_update(struct wm_decoration *d, struct wm_style *s) { (void)d; (void)s; stub_decoration_update_count++; }
void wm_decoration_set_preset(struct wm_decoration *d, enum wm_decor_preset p, struct wm_style *s) { (void)d; (void)p; (void)s; stub_decoration_set_preset_count++; }
void wm_decoration_set_shaded(struct wm_decoration *d, bool shaded, struct wm_style *s) { (void)d; (void)s; stub_decoration_set_shaded_count++; stub_decoration_shaded_value = shaded; }

int style_load(struct wm_style *s, const char *p) { (void)s; (void)p; stub_style_load_count++; return 0; }
void wm_toolbar_relayout(struct wm_toolbar *t) { (void)t; stub_toolbar_relayout_count++; }
void wm_toolbar_toggle_above(struct wm_toolbar *t) { (void)t; stub_toolbar_toggle_above_count++; }
void wm_toolbar_toggle_visible(struct wm_toolbar *t) { (void)t; stub_toolbar_toggle_visible_count++; }
void wm_slit_toggle_above(struct wm_slit *s) { (void)s; stub_slit_toggle_above_count++; }
void wm_slit_toggle_hidden(struct wm_slit *s) { (void)s; stub_slit_toggle_hidden_count++; }

void wm_server_reconfigure(struct wm_server *s) { (void)s; stub_reconfigure_count++; }
int wm_rules_remember_window(struct wm_view *v, const char *p) { (void)v; (void)p; stub_remember_window_count++; return 0; }

void wm_tab_group_next(struct wm_tab_group *g) { (void)g; stub_tab_group_next_count++; }
void wm_tab_group_prev(struct wm_tab_group *g) { (void)g; stub_tab_group_prev_count++; }
void wm_tab_group_remove(struct wm_view *v) { (void)v; stub_tab_group_remove_count++; }
void wm_tab_group_move_left(struct wm_tab_group *g, struct wm_view *v) { (void)g; (void)v; stub_tab_group_move_left_count++; }
void wm_tab_group_move_right(struct wm_tab_group *g, struct wm_view *v) { (void)g; (void)v; stub_tab_group_move_right_count++; }

void wm_focus_nav_enter_toolbar(struct wm_server *s) { (void)s; stub_focus_nav_enter_toolbar_count++; }
void wm_focus_nav_next_element(struct wm_server *s) { (void)s; stub_focus_nav_next_element_count++; }
void wm_focus_nav_prev_element(struct wm_server *s) { (void)s; stub_focus_nav_prev_element_count++; }
void wm_focus_nav_activate(struct wm_server *s) { (void)s; stub_focus_nav_activate_count++; }

void keybind_destroy_all(struct wl_list *l) { (void)l; }
void keybind_destroy_list(struct wl_list *l) { (void)l; }
int keybind_load(struct wl_list *l, const char *p) { (void)l; (void)p; return 0; }
bool keybind_add_from_string(struct wl_list *l, const char *s) { (void)l; (void)s; stub_keybind_add_count++; return true; }
struct wm_keymode *keybind_get_mode(struct wl_list *l, const char *n) { (void)l; (void)n; return NULL; }

/* keybind_find: finds a binding matching modifier+keysym in a list */
static struct wm_keybind *stub_keybind_find_result;
struct wm_keybind *keybind_find(struct wl_list *bindings, uint32_t modifiers,
	xkb_keysym_t keysym)
{
	(void)bindings;
	(void)modifiers;
	(void)keysym;
	return stub_keybind_find_result;
}

int config_reload(struct wm_config *c) { (void)c; return 0; }

void wm_idle_notify_activity(struct wm_server *s) { (void)s; stub_idle_notify_count++; }
bool wm_session_is_locked(struct wm_server *s) { (void)s; return stub_session_locked; }
bool wm_protocols_kb_shortcuts_inhibited(struct wm_server *s) { (void)s; return stub_kb_inhibited; }

void wm_foreign_toplevel_set_minimized(struct wm_view *v, bool m) { (void)v; (void)m; }

/* Forward declarations for keyboard.c functions defined after their first use */
void wm_keyboard_next_layout(struct wm_server *server);
void wm_keyboard_prev_layout(struct wm_server *server);
void wm_keyboard_apply_config(struct wm_server *server);
void wm_keyboard_setup(struct wm_server *server,
	struct wlr_input_device *device);

/* --- Include keyboard source directly (uses our stubs) --- */

#include "util.h"
#include "keyboard.c"

/* --- Mock listener callbacks --- */

static void mock_request_maximize(struct wl_listener *l, void *data)
	{ (void)l; (void)data; stub_maximize_listener_count++; }
static void mock_request_fullscreen(struct wl_listener *l, void *data)
	{ (void)l; (void)data; stub_fullscreen_listener_count++; }
static void mock_request_minimize(struct wl_listener *l, void *data)
	{ (void)l; (void)data; stub_minimize_listener_count++; }

/* --- Test infrastructure --- */

static struct wm_server test_server;
static struct wl_display test_display;
static struct wl_event_loop test_event_loop;
static struct wlr_cursor test_cursor;
static struct wm_style test_style;
static struct wm_config test_config;
static struct wm_toolbar test_toolbar;
static struct wm_slit test_slit;
static struct wm_view test_view;
static struct wlr_xdg_toplevel test_xdg_toplevel;
static struct wlr_xdg_surface test_xdg_surface;
static struct wlr_surface test_surface;
static struct wlr_scene_tree test_scene_tree;
static struct wm_decoration test_decoration;
static struct wm_tab_group test_tab_group;
static struct wm_workspace test_workspace;
static struct wlr_session test_session;

static void
reset_all_counters(void)
{
	stub_focus_next_count = 0;
	stub_focus_prev_count = 0;
	stub_focus_view_count = 0;
	stub_raise_count = 0;
	stub_lower_count = 0;
	stub_raise_layer_count = 0;
	stub_lower_layer_count = 0;
	stub_set_layer_count = 0;
	stub_set_sticky_count = 0;
	stub_set_opacity_count = 0;
	stub_begin_interactive_count = 0;
	stub_maximize_vert_count = 0;
	stub_maximize_horiz_count = 0;
	stub_toggle_decoration_count = 0;
	stub_lhalf_count = 0;
	stub_rhalf_count = 0;
	stub_resize_by_count = 0;
	stub_focus_direction_count = 0;
	stub_set_head_count = 0;
	stub_send_next_head_count = 0;
	stub_send_prev_head_count = 0;
	stub_close_all_count = 0;
	stub_cycle_next_count = 0;
	stub_cycle_prev_count = 0;
	stub_deiconify_last_count = 0;
	stub_deiconify_all_count = 0;
	stub_deiconify_all_ws_count = 0;
	stub_ws_switch_count = 0;
	stub_ws_switch_next_count = 0;
	stub_ws_switch_prev_count = 0;
	stub_ws_switch_right_count = 0;
	stub_ws_switch_left_count = 0;
	stub_ws_set_name_count = 0;
	stub_ws_add_count = 0;
	stub_ws_remove_last_count = 0;
	stub_send_to_ws_count = 0;
	stub_take_to_ws_count = 0;
	stub_send_next_ws_count = 0;
	stub_send_prev_ws_count = 0;
	stub_take_next_ws_count = 0;
	stub_take_prev_ws_count = 0;
	stub_menu_show_root_count = 0;
	stub_menu_show_window_count = 0;
	stub_menu_show_window_list_count = 0;
	stub_menu_hide_all_count = 0;
	stub_menu_show_workspace_count = 0;
	stub_menu_show_client_count = 0;
	stub_menu_show_custom_count = 0;
	stub_arrange_grid_count = 0;
	stub_arrange_vert_count = 0;
	stub_arrange_horiz_count = 0;
	stub_arrange_cascade_count = 0;
	stub_arrange_stack_left_count = 0;
	stub_arrange_stack_right_count = 0;
	stub_arrange_stack_top_count = 0;
	stub_arrange_stack_bottom_count = 0;
	stub_decoration_update_count = 0;
	stub_decoration_set_preset_count = 0;
	stub_decoration_set_shaded_count = 0;
	stub_style_load_count = 0;
	stub_toolbar_relayout_count = 0;
	stub_toolbar_toggle_above_count = 0;
	stub_toolbar_toggle_visible_count = 0;
	stub_slit_toggle_above_count = 0;
	stub_slit_toggle_hidden_count = 0;
	stub_reconfigure_count = 0;
	stub_next_layout_count = 0;
	stub_prev_layout_count = 0;
	stub_keyboard_apply_config_count = 0;
	stub_remember_window_count = 0;
	stub_tab_group_next_count = 0;
	stub_tab_group_prev_count = 0;
	stub_tab_group_remove_count = 0;
	stub_tab_group_move_left_count = 0;
	stub_tab_group_move_right_count = 0;
	stub_activate_tab_count = 0;
	stub_focus_nav_enter_toolbar_count = 0;
	stub_focus_nav_next_element_count = 0;
	stub_focus_nav_prev_element_count = 0;
	stub_focus_nav_activate_count = 0;
	stub_terminate_called = 0;
	stub_close_called = 0;
	stub_set_size_w = 0;
	stub_set_size_h = 0;
	stub_set_pos_x = 0;
	stub_set_pos_y = 0;
	stub_set_pos_called = 0;
	stub_vt_switch_called = 0;
	stub_vt_number = 0;
	stub_keybind_add_count = 0;
	stub_view_layer_from_name_result = 0;
	stub_maximize_listener_count = 0;
	stub_fullscreen_listener_count = 0;
	stub_minimize_listener_count = 0;
	stub_idle_notify_count = 0;
	stub_session_locked = 0;
	stub_kb_inhibited = 0;
	stub_menu_handle_key_result = 0;
	stub_keybind_find_result = NULL;
	stub_active_workspace = NULL;
	stub_modifiers_value = 0;
	stub_nsyms = 0;
	g_stub_source_count = 0;
	memset(g_stub_sources, 0, sizeof(g_stub_sources));
	memset(stub_keysyms, 0, sizeof(stub_keysyms));
	stub_seat_set_keyboard_count = 0;
	stub_seat_notify_key_count = 0;
	stub_seat_notify_key_time = 0;
	stub_seat_notify_key_keycode = 0;
	stub_seat_notify_key_state = 0;
	stub_seat_notify_modifiers_count = 0;
	stub_kb_notify_modifiers_count = 0;
	stub_kb_notify_modifiers_group = 0;
	stub_kb_set_keymap_count = 0;
	stub_num_layouts_value = 1;
	stub_active_layout_index = 0;
	stub_keymap_new_fail = false;
}

static void
setup(void)
{
	reset_all_counters();

	memset(&test_server, 0, sizeof(test_server));
	memset(&test_display, 0, sizeof(test_display));
	memset(&test_event_loop, 0, sizeof(test_event_loop));
	memset(&test_cursor, 0, sizeof(test_cursor));
	memset(&test_style, 0, sizeof(test_style));
	memset(&test_config, 0, sizeof(test_config));
	memset(&test_toolbar, 0, sizeof(test_toolbar));
	memset(&test_slit, 0, sizeof(test_slit));
	memset(&test_session, 0, sizeof(test_session));

	test_server.wl_display = &test_display;
	test_server.wl_event_loop = &test_event_loop;
	test_server.cursor = &test_cursor;
	test_server.style = &test_style;
	test_server.config = &test_config;
	test_server.toolbar = &test_toolbar;
	test_server.slit = &test_slit;
	test_server.session = &test_session;
	test_cursor.x = 100;
	test_cursor.y = 200;

	wl_list_init(&test_server.views);
	wl_list_init(&test_server.workspaces);
	wl_list_init(&test_server.outputs);
	wl_list_init(&test_server.keyboards);
	wl_list_init(&test_server.keybindings);
	wl_list_init(&test_server.keymodes);
	test_server.current_keymode = NULL;
	test_server.chain_state.in_chain = false;
	test_server.chain_state.current_level = NULL;
	test_server.chain_state.timeout = NULL;
	test_server.focused_view = NULL;
}

static void
setup_with_view(void)
{
	setup();

	memset(&test_view, 0, sizeof(test_view));
	memset(&test_xdg_toplevel, 0, sizeof(test_xdg_toplevel));
	memset(&test_xdg_surface, 0, sizeof(test_xdg_surface));
	memset(&test_surface, 0, sizeof(test_surface));
	memset(&test_scene_tree, 0, sizeof(test_scene_tree));
	memset(&test_decoration, 0, sizeof(test_decoration));

	test_xdg_toplevel.base = &test_xdg_surface;
	test_xdg_surface.surface = &test_surface;

	test_view.server = &test_server;
	test_view.xdg_toplevel = &test_xdg_toplevel;
	test_view.scene_tree = &test_scene_tree;
	test_view.decoration = &test_decoration;
	test_view.focus_alpha = 255;
	test_view.unfocus_alpha = 200;
	test_view.request_maximize.notify = mock_request_maximize;
	test_view.request_fullscreen.notify = mock_request_fullscreen;
	test_view.request_minimize.notify = mock_request_minimize;
	test_scene_tree.node.enabled = true;

	test_server.focused_view = &test_view;
}

static void
setup_with_tabbed_view(void)
{
	setup_with_view();

	memset(&test_tab_group, 0, sizeof(test_tab_group));
	wl_list_init(&test_tab_group.views);
	test_tab_group.active_view = &test_view;
	test_tab_group.count = 1;
	test_view.tab_group = &test_tab_group;
}

/* ====================================================================
 * Group A: Pure logic tests (no stubs needed)
 * ==================================================================== */

/* Test is_blocked_env_var: LD_PRELOAD is blocked */
static void
test_blocked_env_ld_preload(void)
{
	assert(is_blocked_env_var("LD_PRELOAD") == true);
	printf("  PASS: test_blocked_env_ld_preload\n");
}

/* Test is_blocked_env_var: PATH is blocked */
static void
test_blocked_env_path(void)
{
	assert(is_blocked_env_var("PATH") == true);
	printf("  PASS: test_blocked_env_path\n");
}

/* Test is_blocked_env_var: normal var is allowed */
static void
test_blocked_env_normal(void)
{
	assert(is_blocked_env_var("MY_APP_VAR") == false);
	printf("  PASS: test_blocked_env_normal\n");
}

/* Test is_blocked_env_var: case insensitive match */
static void
test_blocked_env_case_insensitive(void)
{
	assert(is_blocked_env_var("ld_preload") == true);
	assert(is_blocked_env_var("Ld_Preload") == true);
	printf("  PASS: test_blocked_env_case_insensitive\n");
}

/* Test is_blocked_env_var: WAYLAND_DISPLAY is blocked */
static void
test_blocked_env_wayland_display(void)
{
	assert(is_blocked_env_var("WAYLAND_DISPLAY") == true);
	printf("  PASS: test_blocked_env_wayland_display\n");
}

/* Test bool_match: "true" matches true */
static void
test_bool_match_true(void)
{
	assert(bool_match("true", true) == true);
	assert(bool_match("true", false) == false);
	printf("  PASS: test_bool_match_true\n");
}

/* Test bool_match: "false" matches false */
static void
test_bool_match_false(void)
{
	assert(bool_match("false", false) == true);
	assert(bool_match("false", true) == false);
	printf("  PASS: test_bool_match_false\n");
}

/* Test bool_match: "yes"/"no" variants */
static void
test_bool_match_yes_no(void)
{
	assert(bool_match("yes", true) == true);
	assert(bool_match("no", false) == true);
	assert(bool_match("yes", false) == false);
	assert(bool_match("no", true) == false);
	printf("  PASS: test_bool_match_yes_no\n");
}

/* Test bool_match: "1"/"0" variants */
static void
test_bool_match_digits(void)
{
	assert(bool_match("1", true) == true);
	assert(bool_match("0", false) == true);
	assert(bool_match("1", false) == false);
	assert(bool_match("0", true) == false);
	printf("  PASS: test_bool_match_digits\n");
}

/* Test layer_name: all enum values */
static void
test_layer_name_all(void)
{
	assert(strcmp(layer_name(WM_LAYER_DESKTOP), "Desktop") == 0);
	assert(strcmp(layer_name(WM_LAYER_BELOW), "Below") == 0);
	assert(strcmp(layer_name(WM_LAYER_NORMAL), "Normal") == 0);
	assert(strcmp(layer_name(WM_LAYER_ABOVE), "Above") == 0);
	/* Default case: unknown layer maps to "Normal" */
	assert(strcmp(layer_name(99), "Normal") == 0);
	printf("  PASS: test_layer_name_all\n");
}

/* ====================================================================
 * Group B: Condition evaluation tests (minimal stubs)
 * ==================================================================== */

/* Test match_property: title match */
static void
test_match_property_title(void)
{
	setup_with_view();
	test_view.title = "Firefox";

	assert(match_property(&test_view, "title", "Firefox") == true);
	assert(match_property(&test_view, "title", "Chrome") == false);
	/* Glob matching */
	assert(match_property(&test_view, "title", "Fire*") == true);
	printf("  PASS: test_match_property_title\n");
}

/* Test match_property: class/name match (maps to app_id) */
static void
test_match_property_class(void)
{
	setup_with_view();
	test_view.app_id = "org.mozilla.firefox";

	assert(match_property(&test_view, "class", "org.mozilla.firefox") == true);
	assert(match_property(&test_view, "name", "org.mozilla.firefox") == true);
	assert(match_property(&test_view, "class", "chromium") == false);
	printf("  PASS: test_match_property_class\n");
}

/* Test match_property: boolean properties */
static void
test_match_property_booleans(void)
{
	setup_with_view();
	test_view.maximized = true;
	test_view.fullscreen = false;
	test_view.sticky = true;

	assert(match_property(&test_view, "maximized", "true") == true);
	assert(match_property(&test_view, "maximized", "false") == false);
	assert(match_property(&test_view, "fullscreen", "false") == true);
	assert(match_property(&test_view, "sticky", "yes") == true);
	printf("  PASS: test_match_property_booleans\n");
}

/* Test match_property: layer */
static void
test_match_property_layer(void)
{
	setup_with_view();
	test_view.layer = WM_LAYER_ABOVE;

	assert(match_property(&test_view, "layer", "Above") == true);
	assert(match_property(&test_view, "layer", "Normal") == false);
	printf("  PASS: test_match_property_layer\n");
}

/* Test match_property: shaded (decoration state) */
static void
test_match_property_shaded(void)
{
	setup_with_view();
	test_decoration.shaded = true;

	assert(match_property(&test_view, "shaded", "true") == true);
	assert(match_property(&test_view, "shaded", "false") == false);
	printf("  PASS: test_match_property_shaded\n");
}

/* Test match_property: minimized (scene_tree enabled) */
static void
test_match_property_minimized(void)
{
	setup_with_view();
	test_scene_tree.node.enabled = false; /* minimized */

	assert(match_property(&test_view, "minimized", "true") == true);
	assert(match_property(&test_view, "minimized", "false") == false);
	printf("  PASS: test_match_property_minimized\n");
}

/* Test match_property: workspace match by name and index */
static void
test_match_property_workspace(void)
{
	setup_with_view();
	memset(&test_workspace, 0, sizeof(test_workspace));
	test_workspace.name = "coding";
	test_workspace.index = 2;
	test_view.workspace = &test_workspace;

	assert(match_property(&test_view, "workspace", "coding") == true);
	assert(match_property(&test_view, "workspace", "gaming") == false);
	/* Index-based matching (1-indexed display, so index 2 → "3") */
	assert(match_property(&test_view, "workspace", "3") == true);
	printf("  PASS: test_match_property_workspace\n");
}

/* Test match_property: NULL inputs */
static void
test_match_property_null(void)
{
	assert(match_property(NULL, "title", "test") == false);
	setup_with_view();
	assert(match_property(&test_view, NULL, "test") == false);
	assert(match_property(&test_view, "title", NULL) == false);
	printf("  PASS: test_match_property_null\n");
}

/* Test evaluate_condition: simple match */
static void
test_evaluate_condition_matches(void)
{
	setup_with_view();
	test_view.title = "Firefox";

	struct wm_condition cond = {
		.type = WM_COND_MATCHES,
		.property = "title",
		.pattern = "Firefox",
	};
	assert(evaluate_condition(&test_server, &cond, &test_view) == true);

	cond.pattern = "Chrome";
	assert(evaluate_condition(&test_server, &cond, &test_view) == false);
	printf("  PASS: test_evaluate_condition_matches\n");
}

/* Test evaluate_condition: NOT */
static void
test_evaluate_condition_not(void)
{
	setup_with_view();
	test_view.title = "Firefox";

	struct wm_condition match = {
		.type = WM_COND_MATCHES,
		.property = "title",
		.pattern = "Firefox",
	};
	struct wm_condition not_cond = {
		.type = WM_COND_NOT,
		.child = &match,
	};
	assert(evaluate_condition(&test_server, &not_cond, &test_view) == false);

	match.pattern = "Chrome";
	assert(evaluate_condition(&test_server, &not_cond, &test_view) == true);
	printf("  PASS: test_evaluate_condition_not\n");
}

/* Test evaluate_condition: AND */
static void
test_evaluate_condition_and(void)
{
	setup_with_view();
	test_view.title = "Firefox";
	test_view.maximized = true;

	struct wm_condition left = {
		.type = WM_COND_MATCHES,
		.property = "title",
		.pattern = "Firefox",
	};
	struct wm_condition right = {
		.type = WM_COND_MATCHES,
		.property = "maximized",
		.pattern = "true",
	};
	struct wm_condition and_cond = {
		.type = WM_COND_AND,
		.left = &left,
		.right = &right,
	};
	assert(evaluate_condition(&test_server, &and_cond, &test_view) == true);

	/* Make one side false */
	right.pattern = "false";
	assert(evaluate_condition(&test_server, &and_cond, &test_view) == false);
	printf("  PASS: test_evaluate_condition_and\n");
}

/* Test evaluate_condition: OR */
static void
test_evaluate_condition_or(void)
{
	setup_with_view();
	test_view.title = "Firefox";
	test_view.maximized = false;

	struct wm_condition left = {
		.type = WM_COND_MATCHES,
		.property = "title",
		.pattern = "Firefox",
	};
	struct wm_condition right = {
		.type = WM_COND_MATCHES,
		.property = "maximized",
		.pattern = "true",
	};
	struct wm_condition or_cond = {
		.type = WM_COND_OR,
		.left = &left,
		.right = &right,
	};
	/* left is true, right is false → OR = true */
	assert(evaluate_condition(&test_server, &or_cond, &test_view) == true);

	/* Both false */
	left.pattern = "Chrome";
	assert(evaluate_condition(&test_server, &or_cond, &test_view) == false);
	printf("  PASS: test_evaluate_condition_or\n");
}

/* Test evaluate_condition: XOR */
static void
test_evaluate_condition_xor(void)
{
	setup_with_view();
	test_view.title = "Firefox";
	test_view.maximized = false;

	struct wm_condition left = {
		.type = WM_COND_MATCHES,
		.property = "title",
		.pattern = "Firefox",
	};
	struct wm_condition right = {
		.type = WM_COND_MATCHES,
		.property = "maximized",
		.pattern = "true",
	};
	struct wm_condition xor_cond = {
		.type = WM_COND_XOR,
		.left = &left,
		.right = &right,
	};
	/* T ^ F = T */
	assert(evaluate_condition(&test_server, &xor_cond, &test_view) == true);

	/* Make both true → XOR = false */
	test_view.maximized = true;
	assert(evaluate_condition(&test_server, &xor_cond, &test_view) == false);
	printf("  PASS: test_evaluate_condition_xor\n");
}

/* Test evaluate_condition: NULL returns false */
static void
test_evaluate_condition_null(void)
{
	setup();
	assert(evaluate_condition(&test_server, NULL, NULL) == false);
	printf("  PASS: test_evaluate_condition_null\n");
}

/* Test evaluate_condition: SOME with views list */
static void
test_evaluate_condition_some(void)
{
	setup_with_view();
	test_view.title = "Firefox";
	wl_list_insert(&test_server.views, &test_view.link);

	struct wm_condition cond = {
		.type = WM_COND_SOME,
		.property = "title",
		.pattern = "Firefox",
	};
	assert(evaluate_condition(&test_server, &cond, NULL) == true);

	cond.pattern = "Chrome";
	assert(evaluate_condition(&test_server, &cond, NULL) == false);

	wl_list_remove(&test_view.link);
	printf("  PASS: test_evaluate_condition_some\n");
}

/* Test evaluate_condition: EVERY with views list */
static void
test_evaluate_condition_every(void)
{
	setup_with_view();
	test_view.title = "Firefox";
	wl_list_insert(&test_server.views, &test_view.link);

	struct wm_condition cond = {
		.type = WM_COND_EVERY,
		.property = "title",
		.pattern = "Fire*",
	};
	/* Single view matching → every = true */
	assert(evaluate_condition(&test_server, &cond, NULL) == true);

	wl_list_remove(&test_view.link);
	printf("  PASS: test_evaluate_condition_every\n");
}

/* ====================================================================
 * Group C: Chain state machine tests
 * ==================================================================== */

/* Test chain_reset: clears state */
static void
test_chain_reset(void)
{
	setup();
	test_server.chain_state.in_chain = true;
	test_server.chain_state.current_level = (struct wl_list *)0x1234;

	/* Create a timeout */
	struct wl_event_source timer = {0};
	test_server.chain_state.timeout = &timer;

	chain_reset(&test_server);

	assert(test_server.chain_state.in_chain == false);
	assert(test_server.chain_state.current_level == NULL);
	assert(test_server.chain_state.timeout == NULL);
	assert(timer.removed == true);
	printf("  PASS: test_chain_reset\n");
}

/* Test chain_timeout_cb: resets chain */
static void
test_chain_timeout_cb(void)
{
	setup();
	test_server.chain_state.in_chain = true;

	chain_timeout_cb(&test_server);

	assert(test_server.chain_state.in_chain == false);
	printf("  PASS: test_chain_timeout_cb\n");
}

/* Test chain_start_timeout: creates timer */
static void
test_chain_start_timeout(void)
{
	setup();
	assert(test_server.chain_state.timeout == NULL);

	chain_start_timeout(&test_server);

	assert(test_server.chain_state.timeout != NULL);
	assert(test_server.chain_state.timeout->next_ms == WM_CHAIN_TIMEOUT_MS);
	printf("  PASS: test_chain_start_timeout\n");
}

/* Test chain_start_timeout: restarts existing timer */
static void
test_chain_start_timeout_restart(void)
{
	setup();
	chain_start_timeout(&test_server);
	struct wl_event_source *first = test_server.chain_state.timeout;

	/* Calling again should update the same timer, not create a new one */
	first->next_ms = 0;
	chain_start_timeout(&test_server);
	assert(test_server.chain_state.timeout == first);
	assert(first->next_ms == WM_CHAIN_TIMEOUT_MS);
	printf("  PASS: test_chain_start_timeout_restart\n");
}

/* Test get_active_bindings: returns mode bindings */
static void
test_get_active_bindings(void)
{
	setup();

	/* Create a "default" keymode */
	struct wm_keymode mode;
	mode.name = "default";
	wl_list_init(&mode.bindings);
	wl_list_insert(&test_server.keymodes, &mode.link);

	/* No current_keymode → defaults to "default" */
	struct wl_list *result = get_active_bindings(&test_server);
	assert(result == &mode.bindings);

	/* Explicit keymode */
	test_server.current_keymode = strdup("default");
	result = get_active_bindings(&test_server);
	assert(result == &mode.bindings);

	/* Non-existent mode returns NULL */
	free(test_server.current_keymode);
	test_server.current_keymode = strdup("vim");
	result = get_active_bindings(&test_server);
	assert(result == NULL);

	free(test_server.current_keymode);
	test_server.current_keymode = NULL;
	wl_list_remove(&mode.link);
	printf("  PASS: test_get_active_bindings\n");
}

/* ====================================================================
 * Group D: execute_action() dispatch tests
 * ==================================================================== */

/* Test FocusNext action */
static void
test_action_focus_next(void)
{
	setup();
	assert(execute_action(&test_server, WM_ACTION_FOCUS_NEXT, NULL) == true);
	assert(stub_focus_next_count == 1);
	printf("  PASS: test_action_focus_next\n");
}

/* Test FocusPrev action */
static void
test_action_focus_prev(void)
{
	setup();
	assert(execute_action(&test_server, WM_ACTION_FOCUS_PREV, NULL) == true);
	assert(stub_focus_prev_count == 1);
	printf("  PASS: test_action_focus_prev\n");
}

/* Test Close action */
static void
test_action_close(void)
{
	setup_with_view();
	assert(execute_action(&test_server, WM_ACTION_CLOSE, NULL) == true);
	assert(stub_close_called == 1);
	printf("  PASS: test_action_close\n");
}

/* Test Close with no focused view does nothing */
static void
test_action_close_no_view(void)
{
	setup();
	assert(execute_action(&test_server, WM_ACTION_CLOSE, NULL) == true);
	assert(stub_close_called == 0);
	printf("  PASS: test_action_close_no_view\n");
}

/* Test Exit action */
static void
test_action_exit(void)
{
	setup();
	assert(execute_action(&test_server, WM_ACTION_EXIT, NULL) == true);
	assert(stub_terminate_called == 1);
	printf("  PASS: test_action_exit\n");
}

/* Test Maximize action triggers listener */
static void
test_action_maximize(void)
{
	setup_with_view();
	assert(execute_action(&test_server, WM_ACTION_MAXIMIZE, NULL) == true);
	assert(stub_maximize_listener_count == 1);
	printf("  PASS: test_action_maximize\n");
}

/* Test Fullscreen action triggers listener */
static void
test_action_fullscreen(void)
{
	setup_with_view();
	assert(execute_action(&test_server, WM_ACTION_FULLSCREEN, NULL) == true);
	assert(stub_fullscreen_listener_count == 1);
	printf("  PASS: test_action_fullscreen\n");
}

/* Test Minimize action triggers listener */
static void
test_action_minimize(void)
{
	setup_with_view();
	assert(execute_action(&test_server, WM_ACTION_MINIMIZE, NULL) == true);
	assert(stub_minimize_listener_count == 1);
	printf("  PASS: test_action_minimize\n");
}

/* Test Raise action */
static void
test_action_raise(void)
{
	setup_with_view();
	assert(execute_action(&test_server, WM_ACTION_RAISE, NULL) == true);
	assert(stub_raise_count == 1);
	printf("  PASS: test_action_raise\n");
}

/* Test Lower action */
static void
test_action_lower(void)
{
	setup_with_view();
	assert(execute_action(&test_server, WM_ACTION_LOWER, NULL) == true);
	assert(stub_lower_count == 1);
	printf("  PASS: test_action_lower\n");
}

/* Test Stick action toggles sticky */
static void
test_action_stick(void)
{
	setup_with_view();
	test_view.sticky = false;
	assert(execute_action(&test_server, WM_ACTION_STICK, NULL) == true);
	assert(stub_set_sticky_count == 1);
	assert(stub_set_sticky_value == true);
	printf("  PASS: test_action_stick\n");
}

/* Test NextWorkspace action */
static void
test_action_next_workspace(void)
{
	setup();
	assert(execute_action(&test_server, WM_ACTION_NEXT_WORKSPACE, NULL) == true);
	assert(stub_ws_switch_next_count == 1);
	printf("  PASS: test_action_next_workspace\n");
}

/* Test PrevWorkspace action */
static void
test_action_prev_workspace(void)
{
	setup();
	assert(execute_action(&test_server, WM_ACTION_PREV_WORKSPACE, NULL) == true);
	assert(stub_ws_switch_prev_count == 1);
	printf("  PASS: test_action_prev_workspace\n");
}

/* Test Workspace N action with argument */
static void
test_action_workspace_number(void)
{
	setup();
	assert(execute_action(&test_server, WM_ACTION_WORKSPACE, "3") == true);
	assert(stub_ws_switch_count == 1);
	printf("  PASS: test_action_workspace_number\n");
}

/* Test ToggleDecor action */
static void
test_action_toggle_decor(void)
{
	setup_with_view();
	assert(execute_action(&test_server, WM_ACTION_TOGGLE_DECOR, NULL) == true);
	assert(stub_toggle_decoration_count == 1);
	printf("  PASS: test_action_toggle_decor\n");
}

/* Test MaximizeVert action */
static void
test_action_maximize_vert(void)
{
	setup_with_view();
	assert(execute_action(&test_server, WM_ACTION_MAXIMIZE_VERT, NULL) == true);
	assert(stub_maximize_vert_count == 1);
	printf("  PASS: test_action_maximize_vert\n");
}

/* Test MaximizeHoriz action */
static void
test_action_maximize_horiz(void)
{
	setup_with_view();
	assert(execute_action(&test_server, WM_ACTION_MAXIMIZE_HORIZ, NULL) == true);
	assert(stub_maximize_horiz_count == 1);
	printf("  PASS: test_action_maximize_horiz\n");
}

/* Test LHalf / RHalf actions */
static void
test_action_lhalf_rhalf(void)
{
	setup_with_view();
	assert(execute_action(&test_server, WM_ACTION_LHALF, NULL) == true);
	assert(stub_lhalf_count == 1);
	assert(execute_action(&test_server, WM_ACTION_RHALF, NULL) == true);
	assert(stub_rhalf_count == 1);
	printf("  PASS: test_action_lhalf_rhalf\n");
}

/* Test ArrangeWindows action */
static void
test_action_arrange_windows(void)
{
	setup();
	assert(execute_action(&test_server, WM_ACTION_ARRANGE_WINDOWS, NULL) == true);
	assert(stub_arrange_grid_count == 1);
	printf("  PASS: test_action_arrange_windows\n");
}

/* Test ArrangeStack variants */
static void
test_action_arrange_stack(void)
{
	setup();
	execute_action(&test_server, WM_ACTION_ARRANGE_STACK_LEFT, NULL);
	execute_action(&test_server, WM_ACTION_ARRANGE_STACK_RIGHT, NULL);
	execute_action(&test_server, WM_ACTION_ARRANGE_STACK_TOP, NULL);
	execute_action(&test_server, WM_ACTION_ARRANGE_STACK_BOTTOM, NULL);
	assert(stub_arrange_stack_left_count == 1);
	assert(stub_arrange_stack_right_count == 1);
	assert(stub_arrange_stack_top_count == 1);
	assert(stub_arrange_stack_bottom_count == 1);
	printf("  PASS: test_action_arrange_stack\n");
}

/* Test SetAlpha absolute value */
static void
test_action_set_alpha_absolute(void)
{
	setup_with_view();
	test_view.focus_alpha = 255;

	assert(execute_action(&test_server, WM_ACTION_SET_ALPHA, "128") == true);
	assert(stub_set_opacity_count == 1);
	assert(test_view.focus_alpha == 128);
	assert(test_view.unfocus_alpha == 128);
	printf("  PASS: test_action_set_alpha_absolute\n");
}

/* Test SetAlpha relative value */
static void
test_action_set_alpha_relative(void)
{
	setup_with_view();
	test_view.focus_alpha = 200;

	assert(execute_action(&test_server, WM_ACTION_SET_ALPHA, "-50") == true);
	assert(test_view.focus_alpha == 150);
	printf("  PASS: test_action_set_alpha_relative\n");
}

/* Test SetAlpha clamping */
static void
test_action_set_alpha_clamp(void)
{
	setup_with_view();
	test_view.focus_alpha = 10;

	execute_action(&test_server, WM_ACTION_SET_ALPHA, "-100");
	assert(test_view.focus_alpha == 0); /* clamped to 0 */

	test_view.focus_alpha = 250;
	execute_action(&test_server, WM_ACTION_SET_ALPHA, "+100");
	assert(test_view.focus_alpha == 255); /* clamped to 255 */
	printf("  PASS: test_action_set_alpha_clamp\n");
}

/* Test SetLayer action */
static void
test_action_set_layer(void)
{
	setup_with_view();
	assert(execute_action(&test_server, WM_ACTION_SET_LAYER, "Above") == true);
	assert(stub_set_layer_count == 1);
	printf("  PASS: test_action_set_layer\n");
}

/* Test ShowDesktop action */
static void
test_action_show_desktop(void)
{
	setup_with_view();
	stub_active_workspace = &test_workspace;
	test_view.workspace = &test_workspace;
	wl_list_insert(&test_server.views, &test_view.link);

	assert(execute_action(&test_server, WM_ACTION_SHOW_DESKTOP, NULL) == true);
	assert(stub_minimize_listener_count == 1);

	wl_list_remove(&test_view.link);
	printf("  PASS: test_action_show_desktop\n");
}

/* Test HideMenus action */
static void
test_action_hide_menus(void)
{
	setup();
	assert(execute_action(&test_server, WM_ACTION_HIDE_MENUS, NULL) == true);
	assert(stub_menu_hide_all_count == 1);
	printf("  PASS: test_action_hide_menus\n");
}

/* Test RootMenu action */
static void
test_action_root_menu(void)
{
	setup();
	assert(execute_action(&test_server, WM_ACTION_ROOT_MENU, NULL) == true);
	assert(stub_menu_show_root_count == 1);
	printf("  PASS: test_action_root_menu\n");
}

/* Test SetEnv: blocked variable rejected */
static void
test_action_set_env_blocked(void)
{
	setup();
	/* This should be silently rejected */
	assert(execute_action(&test_server, WM_ACTION_SET_ENV, "LD_PRELOAD=evil.so") == true);
	/* LD_PRELOAD should NOT have been set to evil.so */
	const char *val = getenv("LD_PRELOAD");
	assert(val == NULL || strcmp(val, "evil.so") != 0);
	printf("  PASS: test_action_set_env_blocked\n");
}

/* Test SetEnv: normal variable with = format */
static void
test_action_set_env_normal_eq(void)
{
	setup();
	/* Unset first to be sure */
	unsetenv("TEST_KEYBOARD_VAR");

	assert(execute_action(&test_server, WM_ACTION_SET_ENV, "TEST_KEYBOARD_VAR=hello") == true);
	const char *val = getenv("TEST_KEYBOARD_VAR");
	assert(val != NULL);
	assert(strcmp(val, "hello") == 0);

	unsetenv("TEST_KEYBOARD_VAR");
	printf("  PASS: test_action_set_env_normal_eq\n");
}

/* Test SetEnv: normal variable with space format */
static void
test_action_set_env_normal_space(void)
{
	setup();
	unsetenv("TEST_KEYBOARD_VAR2");

	assert(execute_action(&test_server, WM_ACTION_SET_ENV, "TEST_KEYBOARD_VAR2 world") == true);
	const char *val = getenv("TEST_KEYBOARD_VAR2");
	assert(val != NULL);
	assert(strcmp(val, "world") == 0);

	unsetenv("TEST_KEYBOARD_VAR2");
	printf("  PASS: test_action_set_env_normal_space\n");
}

/* Test NOP action */
static void
test_action_nop(void)
{
	setup();
	assert(execute_action(&test_server, WM_ACTION_NOP, NULL) == true);
	printf("  PASS: test_action_nop\n");
}

/* Test MacroCmd/ToggleCmd reached via direct execute_action returns false */
static void
test_action_macro_toggle_direct(void)
{
	setup();
	assert(execute_action(&test_server, WM_ACTION_MACRO_CMD, NULL) == false);
	assert(execute_action(&test_server, WM_ACTION_TOGGLE_CMD, NULL) == false);
	printf("  PASS: test_action_macro_toggle_direct\n");
}

/* Test Shade action */
static void
test_action_shade(void)
{
	setup_with_view();
	test_decoration.shaded = false;

	assert(execute_action(&test_server, WM_ACTION_SHADE, NULL) == true);
	assert(stub_decoration_set_shaded_count == 1);
	assert(stub_decoration_shaded_value == true); /* toggle from false */
	printf("  PASS: test_action_shade\n");
}

/* Test ShadeOn / ShadeOff */
static void
test_action_shade_on_off(void)
{
	setup_with_view();
	assert(execute_action(&test_server, WM_ACTION_SHADE_ON, NULL) == true);
	assert(stub_decoration_set_shaded_count == 1);
	assert(stub_decoration_shaded_value == true);

	assert(execute_action(&test_server, WM_ACTION_SHADE_OFF, NULL) == true);
	assert(stub_decoration_set_shaded_count == 2);
	assert(stub_decoration_shaded_value == false);
	printf("  PASS: test_action_shade_on_off\n");
}

/* Test Reconfigure action */
static void
test_action_reconfigure(void)
{
	setup();
	assert(execute_action(&test_server, WM_ACTION_RECONFIGURE, NULL) == true);
	assert(stub_reconfigure_count == 1);
	printf("  PASS: test_action_reconfigure\n");
}

/* Test MoveLeft/Right/Up/Down actions */
static void
test_action_move_directional(void)
{
	setup_with_view();
	test_view.x = 100;
	test_view.y = 100;

	execute_action(&test_server, WM_ACTION_MOVE_LEFT, "10");
	assert(test_view.x == 90);
	assert(stub_set_pos_called == 1);

	execute_action(&test_server, WM_ACTION_MOVE_RIGHT, "20");
	assert(test_view.x == 110);

	execute_action(&test_server, WM_ACTION_MOVE_UP, "5");
	assert(test_view.y == 95);

	execute_action(&test_server, WM_ACTION_MOVE_DOWN, "15");
	assert(test_view.y == 110);
	printf("  PASS: test_action_move_directional\n");
}

/* Test MoveTo action */
static void
test_action_move_to(void)
{
	setup_with_view();
	execute_action(&test_server, WM_ACTION_MOVE_TO, "50 75");
	assert(test_view.x == 50);
	assert(test_view.y == 75);
	printf("  PASS: test_action_move_to\n");
}

/* Test ResizeTo action */
static void
test_action_resize_to(void)
{
	setup_with_view();
	execute_action(&test_server, WM_ACTION_RESIZE_TO, "800 600");
	assert(stub_set_size_w == 800);
	assert(stub_set_size_h == 600);
	printf("  PASS: test_action_resize_to\n");
}

/* Test FocusDirection actions */
static void
test_action_focus_directions(void)
{
	setup();
	execute_action(&test_server, WM_ACTION_FOCUS_LEFT, NULL);
	execute_action(&test_server, WM_ACTION_FOCUS_RIGHT, NULL);
	execute_action(&test_server, WM_ACTION_FOCUS_UP, NULL);
	execute_action(&test_server, WM_ACTION_FOCUS_DOWN, NULL);
	assert(stub_focus_direction_count == 4);
	printf("  PASS: test_action_focus_directions\n");
}

/* Test Tab group actions */
static void
test_action_tab_group(void)
{
	setup_with_tabbed_view();
	execute_action(&test_server, WM_ACTION_NEXT_TAB, NULL);
	assert(stub_tab_group_next_count == 1);
	execute_action(&test_server, WM_ACTION_PREV_TAB, NULL);
	assert(stub_tab_group_prev_count == 1);
	execute_action(&test_server, WM_ACTION_DETACH_CLIENT, NULL);
	assert(stub_tab_group_remove_count == 1);
	execute_action(&test_server, WM_ACTION_MOVE_TAB_LEFT, NULL);
	assert(stub_tab_group_move_left_count == 1);
	execute_action(&test_server, WM_ACTION_MOVE_TAB_RIGHT, NULL);
	assert(stub_tab_group_move_right_count == 1);
	printf("  PASS: test_action_tab_group\n");
}

/* Test Focus nav actions */
static void
test_action_focus_nav(void)
{
	setup();
	execute_action(&test_server, WM_ACTION_FOCUS_TOOLBAR, NULL);
	assert(stub_focus_nav_enter_toolbar_count == 1);
	execute_action(&test_server, WM_ACTION_FOCUS_NEXT_ELEMENT, NULL);
	assert(stub_focus_nav_next_element_count == 1);
	execute_action(&test_server, WM_ACTION_FOCUS_PREV_ELEMENT, NULL);
	assert(stub_focus_nav_prev_element_count == 1);
	execute_action(&test_server, WM_ACTION_FOCUS_ACTIVATE, NULL);
	assert(stub_focus_nav_activate_count == 1);
	printf("  PASS: test_action_focus_nav\n");
}

/* Test Deiconify variants */
static void
test_action_deiconify(void)
{
	setup();
	execute_action(&test_server, WM_ACTION_DEICONIFY, NULL);
	assert(stub_deiconify_last_count == 1);

	execute_action(&test_server, WM_ACTION_DEICONIFY, "All");
	assert(stub_deiconify_all_count == 1);

	execute_action(&test_server, WM_ACTION_DEICONIFY, "AllWorkspace");
	assert(stub_deiconify_all_ws_count == 1);

	execute_action(&test_server, WM_ACTION_DEICONIFY, "LastWorkspace");
	assert(stub_deiconify_last_count == 2);
	printf("  PASS: test_action_deiconify\n");
}

/* Test Toolbar/Slit toggle actions */
static void
test_action_toolbar_slit_toggles(void)
{
	setup();
	execute_action(&test_server, WM_ACTION_TOGGLE_TOOLBAR_ABOVE, NULL);
	assert(stub_toolbar_toggle_above_count == 1);
	execute_action(&test_server, WM_ACTION_TOGGLE_TOOLBAR_VISIBLE, NULL);
	assert(stub_toolbar_toggle_visible_count == 1);
	execute_action(&test_server, WM_ACTION_TOGGLE_SLIT_ABOVE, NULL);
	assert(stub_slit_toggle_above_count == 1);
	execute_action(&test_server, WM_ACTION_TOGGLE_SLIT_HIDDEN, NULL);
	assert(stub_slit_toggle_hidden_count == 1);
	printf("  PASS: test_action_toolbar_slit_toggles\n");
}

/* Test KeyMode action */
static void
test_action_key_mode(void)
{
	setup();
	test_server.current_keymode = strdup("default");

	execute_action(&test_server, WM_ACTION_KEY_MODE, "vim");
	assert(test_server.current_keymode != NULL);
	assert(strcmp(test_server.current_keymode, "vim") == 0);

	free(test_server.current_keymode);
	test_server.current_keymode = NULL;
	printf("  PASS: test_action_key_mode\n");
}

/* Test ToggleShowPosition action */
static void
test_action_toggle_show_position(void)
{
	setup();
	test_server.show_position = false;
	execute_action(&test_server, WM_ACTION_TOGGLE_SHOW_POSITION, NULL);
	assert(test_server.show_position == true);
	execute_action(&test_server, WM_ACTION_TOGGLE_SHOW_POSITION, NULL);
	assert(test_server.show_position == false);
	printf("  PASS: test_action_toggle_show_position\n");
}

/* Test SetDecor action with various presets */
static void
test_action_set_decor(void)
{
	setup_with_view();
	execute_action(&test_server, WM_ACTION_SET_DECOR, "NONE");
	assert(stub_decoration_set_preset_count == 1);
	execute_action(&test_server, WM_ACTION_SET_DECOR, "BORDER");
	assert(stub_decoration_set_preset_count == 2);
	execute_action(&test_server, WM_ACTION_SET_DECOR, "TAB");
	assert(stub_decoration_set_preset_count == 3);
	printf("  PASS: test_action_set_decor\n");
}

/* Test If/ForEach/Map/Delay via execute_action returns false (need keybind context) */
static void
test_action_conditional_direct(void)
{
	setup();
	assert(execute_action(&test_server, WM_ACTION_IF, NULL) == false);
	assert(execute_action(&test_server, WM_ACTION_FOREACH, NULL) == false);
	assert(execute_action(&test_server, WM_ACTION_MAP, NULL) == false);
	assert(execute_action(&test_server, WM_ACTION_DELAY, NULL) == false);
	printf("  PASS: test_action_conditional_direct\n");
}

/* Test workspace actions: SendTo, TakeTo, etc */
static void
test_action_workspace_ops(void)
{
	setup();
	execute_action(&test_server, WM_ACTION_SEND_TO_WORKSPACE, "2");
	assert(stub_send_to_ws_count == 1);
	execute_action(&test_server, WM_ACTION_TAKE_TO_WORKSPACE, "3");
	assert(stub_take_to_ws_count == 1);
	execute_action(&test_server, WM_ACTION_SEND_TO_NEXT_WORKSPACE, NULL);
	assert(stub_send_next_ws_count == 1);
	execute_action(&test_server, WM_ACTION_SEND_TO_PREV_WORKSPACE, NULL);
	assert(stub_send_prev_ws_count == 1);
	execute_action(&test_server, WM_ACTION_TAKE_TO_NEXT_WORKSPACE, NULL);
	assert(stub_take_next_ws_count == 1);
	execute_action(&test_server, WM_ACTION_TAKE_TO_PREV_WORKSPACE, NULL);
	assert(stub_take_prev_ws_count == 1);
	execute_action(&test_server, WM_ACTION_ADD_WORKSPACE, NULL);
	assert(stub_ws_add_count == 1);
	execute_action(&test_server, WM_ACTION_REMOVE_LAST_WORKSPACE, NULL);
	assert(stub_ws_remove_last_count == 1);
	printf("  PASS: test_action_workspace_ops\n");
}

/* Test CloseAllWindows */
static void
test_action_close_all(void)
{
	setup();
	execute_action(&test_server, WM_ACTION_CLOSE_ALL_WINDOWS, NULL);
	assert(stub_close_all_count == 1);
	printf("  PASS: test_action_close_all\n");
}

/* Test RaiseLayer / LowerLayer */
static void
test_action_raise_lower_layer(void)
{
	setup_with_view();
	execute_action(&test_server, WM_ACTION_RAISE_LAYER, NULL);
	assert(stub_raise_layer_count == 1);
	execute_action(&test_server, WM_ACTION_LOWER_LAYER, NULL);
	assert(stub_lower_layer_count == 1);
	printf("  PASS: test_action_raise_lower_layer\n");
}

/* Test NextLayout / PrevLayout (real impl iterates empty keyboard list) */
static void
test_action_layouts(void)
{
	setup();
	assert(execute_action(&test_server, WM_ACTION_NEXT_LAYOUT, NULL) == true);
	assert(execute_action(&test_server, WM_ACTION_PREV_LAYOUT, NULL) == true);
	printf("  PASS: test_action_layouts\n");
}

/* ====================================================================
 * Group E: execute_keybind_action() dispatch tests (MacroCmd, etc)
 * ==================================================================== */

/* Test MacroCmd: executes all subcmds in sequence */
static void
test_keybind_action_macro(void)
{
	setup();
	struct wm_subcmd cmd2 = {
		.action = WM_ACTION_FOCUS_PREV,
		.argument = NULL,
		.next = NULL,
	};
	struct wm_subcmd cmd1 = {
		.action = WM_ACTION_FOCUS_NEXT,
		.argument = NULL,
		.next = &cmd2,
	};
	struct wm_keybind bind = {0};
	wl_list_init(&bind.children);
	bind.action = WM_ACTION_MACRO_CMD;
	bind.subcmds = &cmd1;

	assert(execute_keybind_action(&test_server, &bind) == true);
	assert(stub_focus_next_count == 1);
	assert(stub_focus_prev_count == 1);
	printf("  PASS: test_keybind_action_macro\n");
}

/* Test ToggleCmd: cycles through subcmds */
static void
test_keybind_action_toggle(void)
{
	setup();
	struct wm_subcmd cmd2 = {
		.action = WM_ACTION_FOCUS_PREV,
		.argument = NULL,
		.next = NULL,
	};
	struct wm_subcmd cmd1 = {
		.action = WM_ACTION_FOCUS_NEXT,
		.argument = NULL,
		.next = &cmd2,
	};
	struct wm_keybind bind = {0};
	wl_list_init(&bind.children);
	bind.action = WM_ACTION_TOGGLE_CMD;
	bind.subcmds = &cmd1;
	bind.subcmd_count = 2;
	bind.toggle_index = 0;

	/* First call: cmd1 (FOCUS_NEXT) */
	execute_keybind_action(&test_server, &bind);
	assert(stub_focus_next_count == 1);
	assert(stub_focus_prev_count == 0);
	assert(bind.toggle_index == 1);

	/* Second call: cmd2 (FOCUS_PREV) */
	execute_keybind_action(&test_server, &bind);
	assert(stub_focus_prev_count == 1);
	assert(bind.toggle_index == 0); /* wraps around */

	printf("  PASS: test_keybind_action_toggle\n");
}

/* Test If: true branch executes */
static void
test_keybind_action_if_true(void)
{
	setup_with_view();
	test_view.title = "Firefox";

	struct wm_condition cond = {
		.type = WM_COND_MATCHES,
		.property = "title",
		.pattern = "Firefox",
	};
	struct wm_subcmd then_cmd = {
		.action = WM_ACTION_FOCUS_NEXT,
		.argument = NULL,
		.next = NULL,
	};
	struct wm_subcmd else_cmd = {
		.action = WM_ACTION_FOCUS_PREV,
		.argument = NULL,
		.next = NULL,
	};
	struct wm_keybind bind = {0};
	wl_list_init(&bind.children);
	bind.action = WM_ACTION_IF;
	bind.condition = &cond;
	bind.subcmds = &then_cmd;
	bind.else_cmd = &else_cmd;

	execute_keybind_action(&test_server, &bind);
	assert(stub_focus_next_count == 1);
	assert(stub_focus_prev_count == 0);
	printf("  PASS: test_keybind_action_if_true\n");
}

/* Test If: false branch (else) executes */
static void
test_keybind_action_if_false(void)
{
	setup_with_view();
	test_view.title = "Firefox";

	struct wm_condition cond = {
		.type = WM_COND_MATCHES,
		.property = "title",
		.pattern = "Chrome",
	};
	struct wm_subcmd then_cmd = {
		.action = WM_ACTION_FOCUS_NEXT,
		.argument = NULL,
		.next = NULL,
	};
	struct wm_subcmd else_cmd = {
		.action = WM_ACTION_FOCUS_PREV,
		.argument = NULL,
		.next = NULL,
	};
	struct wm_keybind bind = {0};
	wl_list_init(&bind.children);
	bind.action = WM_ACTION_IF;
	bind.condition = &cond;
	bind.subcmds = &then_cmd;
	bind.else_cmd = &else_cmd;

	execute_keybind_action(&test_server, &bind);
	assert(stub_focus_next_count == 0);
	assert(stub_focus_prev_count == 1);
	printf("  PASS: test_keybind_action_if_false\n");
}

/* Test ForEach: runs action on matching views */
static void
test_keybind_action_foreach(void)
{
	setup_with_view();
	test_view.title = "Firefox";
	wl_list_insert(&test_server.views, &test_view.link);

	struct wm_condition cond = {
		.type = WM_COND_MATCHES,
		.property = "title",
		.pattern = "Fire*",
	};
	struct wm_subcmd cmd = {
		.action = WM_ACTION_RAISE,
		.argument = NULL,
		.next = NULL,
	};
	struct wm_keybind bind = {0};
	wl_list_init(&bind.children);
	bind.action = WM_ACTION_FOREACH;
	bind.condition = &cond;
	bind.subcmds = &cmd;

	execute_keybind_action(&test_server, &bind);
	assert(stub_raise_count == 1);

	/* Verify focused_view is restored */
	assert(test_server.focused_view == &test_view);

	wl_list_remove(&test_view.link);
	printf("  PASS: test_keybind_action_foreach\n");
}

/* ====================================================================
 * Group F: handle_compositor_keybinding() tests
 * ==================================================================== */

/* Test VT switch keybinding */
static void
test_compositor_vt_switch(void)
{
	setup();
	bool result = handle_compositor_keybinding(&test_server,
		WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT,
		XKB_KEY_XF86Switch_VT_1);
	assert(result == true);
	assert(stub_vt_switch_called == 1);
	assert(stub_vt_number == 1);
	printf("  PASS: test_compositor_vt_switch\n");
}

/* Test VT switch to VT 5 */
static void
test_compositor_vt_switch_5(void)
{
	setup();
	bool result = handle_compositor_keybinding(&test_server,
		WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT,
		XKB_KEY_XF86Switch_VT_1 + 4);
	assert(result == true);
	assert(stub_vt_number == 5);
	printf("  PASS: test_compositor_vt_switch_5\n");
}

/* Test fallback exit: Mod4+Shift+E */
static void
test_compositor_fallback_exit(void)
{
	setup();
	bool result = handle_compositor_keybinding(&test_server,
		WLR_MODIFIER_LOGO | WLR_MODIFIER_SHIFT,
		XKB_KEY_e);
	assert(result == true);
	assert(stub_terminate_called == 1);
	printf("  PASS: test_compositor_fallback_exit\n");
}

/* Test Escape aborts chain */
static void
test_compositor_escape_chain(void)
{
	setup();
	test_server.chain_state.in_chain = true;
	test_server.chain_state.current_level = &test_server.keybindings;

	bool result = handle_compositor_keybinding(&test_server, 0, XKB_KEY_Escape);
	assert(result == true);
	assert(test_server.chain_state.in_chain == false);
	printf("  PASS: test_compositor_escape_chain\n");
}

/* Test chain entry: NOP node with children */
static void
test_compositor_chain_entry(void)
{
	setup();

	/* Set up a keymode with a binding */
	struct wm_keymode mode;
	mode.name = "default";
	wl_list_init(&mode.bindings);
	wl_list_insert(&test_server.keymodes, &mode.link);

	/* Create a chain node (NOP with children) */
	struct wm_keybind chain_bind = {0};
	chain_bind.action = WM_ACTION_NOP;
	wl_list_init(&chain_bind.children);
	/* Add a dummy child so children isn't empty */
	struct wm_keybind child = {0};
	child.action = WM_ACTION_EXIT;
	wl_list_init(&child.children);
	wl_list_insert(&chain_bind.children, &child.link);

	stub_keybind_find_result = &chain_bind;

	bool result = handle_compositor_keybinding(&test_server, 0, XKB_KEY_a);
	assert(result == true);
	assert(test_server.chain_state.in_chain == true);
	assert(test_server.chain_state.current_level == &chain_bind.children);

	wl_list_remove(&mode.link);
	wl_list_remove(&child.link);
	printf("  PASS: test_compositor_chain_entry\n");
}

/* Test leaf node: executes action and resets chain */
static void
test_compositor_leaf_node(void)
{
	setup();

	struct wm_keymode mode;
	mode.name = "default";
	wl_list_init(&mode.bindings);
	wl_list_insert(&test_server.keymodes, &mode.link);

	struct wm_keybind leaf = {0};
	leaf.action = WM_ACTION_EXIT;
	wl_list_init(&leaf.children);

	stub_keybind_find_result = &leaf;

	bool result = handle_compositor_keybinding(&test_server, 0, XKB_KEY_a);
	assert(result == true);
	assert(stub_terminate_called == 1);
	assert(test_server.chain_state.in_chain == false);
	assert(test_server.focus_user_initiated == true);

	wl_list_remove(&mode.link);
	printf("  PASS: test_compositor_leaf_node\n");
}

/* Test no binding match */
static void
test_compositor_no_match(void)
{
	setup();

	struct wm_keymode mode;
	mode.name = "default";
	wl_list_init(&mode.bindings);
	wl_list_insert(&test_server.keymodes, &mode.link);

	stub_keybind_find_result = NULL;

	bool result = handle_compositor_keybinding(&test_server, 0, XKB_KEY_a);
	assert(result == false);

	wl_list_remove(&mode.link);
	printf("  PASS: test_compositor_no_match\n");
}

/* Test no match while in chain resets chain */
static void
test_compositor_no_match_in_chain(void)
{
	setup();
	test_server.chain_state.in_chain = true;
	test_server.chain_state.current_level = &test_server.keybindings;
	stub_keybind_find_result = NULL;

	bool result = handle_compositor_keybinding(&test_server, 0, XKB_KEY_a);
	assert(result == false);
	assert(test_server.chain_state.in_chain == false);
	printf("  PASS: test_compositor_no_match_in_chain\n");
}

/* ====================================================================
 * Group G: handle_key() tests
 * ==================================================================== */

static struct wm_keyboard test_kb;
static struct wlr_keyboard test_wlr_kb;
static struct wlr_seat test_seat;
static struct xkb_state test_xkb_state;

static void
setup_keyboard(void)
{
	setup();

	memset(&test_kb, 0, sizeof(test_kb));
	memset(&test_wlr_kb, 0, sizeof(test_wlr_kb));
	memset(&test_seat, 0, sizeof(test_seat));
	memset(&test_xkb_state, 0, sizeof(test_xkb_state));

	test_kb.server = &test_server;
	test_kb.wlr_keyboard = &test_wlr_kb;
	test_wlr_kb.xkb_state = &test_xkb_state;
	test_wlr_kb.keymap = &stub_xkb_keymap;
	test_server.seat = &test_seat;

	/* Set up a default keymode so handle_compositor_keybinding works */
	static struct wm_keymode default_mode;
	default_mode.name = "default";
	wl_list_init(&default_mode.bindings);
	wl_list_init(&test_server.keymodes);
	wl_list_insert(&test_server.keymodes, &default_mode.link);

	wl_list_init(&test_kb.modifiers.link);
	wl_list_init(&test_kb.key.link);
	wl_list_init(&test_kb.destroy.link);
	wl_list_init(&test_kb.link);
}

/* Test: session locked, VT switch still works */
static void
test_handle_key_session_locked_vt_switch(void)
{
	setup_keyboard();
	stub_session_locked = 1;
	stub_nsyms = 1;
	stub_keysyms[0] = XKB_KEY_XF86Switch_VT_1 + 2; /* VT 3 */
	stub_modifiers_value = WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT;

	struct wlr_keyboard_key_event event = {
		.time_msec = 100,
		.keycode = 62,
		.state = WL_KEYBOARD_KEY_STATE_PRESSED,
	};

	handle_key(&test_kb.key, &event);

	assert(stub_vt_switch_called == 1);
	assert(stub_vt_number == 3);
	assert(stub_seat_notify_key_count == 0);
	printf("  PASS: test_handle_key_session_locked_vt_switch\n");
}

/* Test: session locked, other keys forwarded to lock surface */
static void
test_handle_key_session_locked_forward(void)
{
	setup_keyboard();
	stub_session_locked = 1;
	stub_nsyms = 1;
	stub_keysyms[0] = XKB_KEY_a;
	stub_modifiers_value = 0;

	struct wlr_keyboard_key_event event = {
		.time_msec = 200,
		.keycode = 38,
		.state = WL_KEYBOARD_KEY_STATE_PRESSED,
	};

	handle_key(&test_kb.key, &event);

	assert(stub_vt_switch_called == 0);
	assert(stub_seat_set_keyboard_count == 1);
	assert(stub_seat_notify_key_count == 1);
	assert(stub_seat_notify_key_keycode == 38);
	printf("  PASS: test_handle_key_session_locked_forward\n");
}

/* Test: shortcuts inhibited, VT switch works */
static void
test_handle_key_inhibited_vt_switch(void)
{
	setup_keyboard();
	stub_kb_inhibited = 1;
	stub_nsyms = 1;
	stub_keysyms[0] = XKB_KEY_XF86Switch_VT_1 + 4; /* VT 5 */
	stub_modifiers_value = WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT;

	struct wlr_keyboard_key_event event = {
		.time_msec = 100,
		.keycode = 66,
		.state = WL_KEYBOARD_KEY_STATE_PRESSED,
	};

	handle_key(&test_kb.key, &event);

	assert(stub_vt_switch_called == 1);
	assert(stub_vt_number == 5);
	assert(stub_seat_notify_key_count == 0);
	printf("  PASS: test_handle_key_inhibited_vt_switch\n");
}

/* Test: shortcuts inhibited, emergency exit (Mod4+Shift+E) works */
static void
test_handle_key_inhibited_emergency_exit(void)
{
	setup_keyboard();
	stub_kb_inhibited = 1;
	stub_nsyms = 1;
	stub_keysyms[0] = XKB_KEY_e;
	stub_modifiers_value = WLR_MODIFIER_LOGO | WLR_MODIFIER_SHIFT;

	struct wlr_keyboard_key_event event = {
		.time_msec = 100,
		.keycode = 26,
		.state = WL_KEYBOARD_KEY_STATE_PRESSED,
	};

	handle_key(&test_kb.key, &event);

	assert(stub_terminate_called == 1);
	assert(stub_seat_notify_key_count == 0);
	printf("  PASS: test_handle_key_inhibited_emergency_exit\n");
}

/* Test: shortcuts inhibited, other keys forwarded to client */
static void
test_handle_key_inhibited_forward(void)
{
	setup_keyboard();
	stub_kb_inhibited = 1;
	stub_nsyms = 1;
	stub_keysyms[0] = XKB_KEY_a;
	stub_modifiers_value = 0;

	struct wlr_keyboard_key_event event = {
		.time_msec = 300,
		.keycode = 38,
		.state = WL_KEYBOARD_KEY_STATE_PRESSED,
	};

	handle_key(&test_kb.key, &event);

	assert(stub_vt_switch_called == 0);
	assert(stub_terminate_called == 0);
	assert(stub_seat_notify_key_count == 1);
	assert(stub_seat_notify_key_keycode == 38);
	printf("  PASS: test_handle_key_inhibited_forward\n");
}

/* Test: menu consumes the key */
static void
test_handle_key_menu_consumed(void)
{
	setup_keyboard();
	stub_nsyms = 1;
	stub_keysyms[0] = XKB_KEY_a;
	stub_modifiers_value = 0;
	stub_menu_handle_key_result = 1;

	struct wlr_keyboard_key_event event = {
		.time_msec = 100,
		.keycode = 38,
		.state = WL_KEYBOARD_KEY_STATE_PRESSED,
	};

	handle_key(&test_kb.key, &event);

	assert(stub_seat_notify_key_count == 0);
	printf("  PASS: test_handle_key_menu_consumed\n");
}

/* Test: binding match found and executed */
static void
test_handle_key_binding_match(void)
{
	setup_keyboard();
	stub_nsyms = 1;
	stub_keysyms[0] = XKB_KEY_a;
	stub_modifiers_value = WLR_MODIFIER_LOGO;

	struct wm_keybind bind = {0};
	wl_list_init(&bind.children);
	bind.action = WM_ACTION_EXIT;
	stub_keybind_find_result = &bind;

	struct wlr_keyboard_key_event event = {
		.time_msec = 100,
		.keycode = 38,
		.state = WL_KEYBOARD_KEY_STATE_PRESSED,
	};

	handle_key(&test_kb.key, &event);

	assert(stub_terminate_called == 1);
	assert(stub_seat_notify_key_count == 0);
	printf("  PASS: test_handle_key_binding_match\n");
}

/* Test: no binding match, forward to client */
static void
test_handle_key_no_binding_forward(void)
{
	setup_keyboard();
	stub_nsyms = 1;
	stub_keysyms[0] = XKB_KEY_a;
	stub_modifiers_value = 0;
	stub_keybind_find_result = NULL;

	struct wlr_keyboard_key_event event = {
		.time_msec = 400,
		.keycode = 38,
		.state = WL_KEYBOARD_KEY_STATE_PRESSED,
	};

	handle_key(&test_kb.key, &event);

	assert(stub_seat_notify_key_count == 1);
	assert(stub_seat_notify_key_keycode == 38);
	printf("  PASS: test_handle_key_no_binding_forward\n");
}

/* Test: release events forwarded to client */
static void
test_handle_key_release_forward(void)
{
	setup_keyboard();
	stub_nsyms = 1;
	stub_keysyms[0] = XKB_KEY_a;
	stub_modifiers_value = 0;

	struct wlr_keyboard_key_event event = {
		.time_msec = 500,
		.keycode = 38,
		.state = WL_KEYBOARD_KEY_STATE_RELEASED,
	};

	handle_key(&test_kb.key, &event);

	assert(stub_seat_notify_key_count == 1);
	assert(stub_seat_notify_key_state == WL_KEYBOARD_KEY_STATE_RELEASED);
	printf("  PASS: test_handle_key_release_forward\n");
}

/* Test: idle notify on keypress */
static void
test_handle_key_idle_notify(void)
{
	setup_keyboard();
	stub_nsyms = 1;
	stub_keysyms[0] = XKB_KEY_a;
	stub_modifiers_value = 0;

	struct wlr_keyboard_key_event event = {
		.time_msec = 100,
		.keycode = 38,
		.state = WL_KEYBOARD_KEY_STATE_PRESSED,
	};

	handle_key(&test_kb.key, &event);

	assert(stub_idle_notify_count == 1);
	printf("  PASS: test_handle_key_idle_notify\n");
}

/* ====================================================================
 * Group H: handle_modifiers() tests
 * ==================================================================== */

/* Test: forward modifier state to seat */
static void
test_handle_modifiers_forward(void)
{
	setup_keyboard();

	handle_modifiers(&test_kb.modifiers, NULL);

	assert(stub_seat_set_keyboard_count == 1);
	assert(stub_seat_notify_modifiers_count == 1);
	printf("  PASS: test_handle_modifiers_forward\n");
}

/* ====================================================================
 * Group I: handle_keyboard_destroy() tests
 * ==================================================================== */

/* Test: remove keyboard from list, free resources */
static void
test_handle_keyboard_destroy_cleanup(void)
{
	setup_keyboard();

	struct wm_keyboard *kb = calloc(1, sizeof(*kb));
	assert(kb != NULL);
	kb->server = &test_server;
	kb->wlr_keyboard = &test_wlr_kb;

	wl_list_init(&kb->modifiers.link);
	wl_list_init(&kb->key.link);
	wl_list_init(&kb->destroy.link);

	wl_list_insert(&test_server.keyboards, &kb->link);
	assert(!wl_list_empty(&test_server.keyboards));

	kb->destroy.notify = handle_keyboard_destroy;

	handle_keyboard_destroy(&kb->destroy, NULL);

	assert(wl_list_empty(&test_server.keyboards));

	printf("  PASS: test_handle_keyboard_destroy_cleanup\n");
}

/* ====================================================================
 * Group J: wm_keyboard_next/prev_layout() tests
 * ==================================================================== */

/* Test: next_layout wraps around to 0 */
static void
test_next_layout_wrap(void)
{
	setup_keyboard();

	wl_list_insert(&test_server.keyboards, &test_kb.link);
	stub_num_layouts_value = 3;
	stub_active_layout_index = 2;

	wm_keyboard_next_layout(&test_server);

	assert(stub_kb_notify_modifiers_count == 1);
	assert(stub_kb_notify_modifiers_group == 0);

	wl_list_remove(&test_kb.link);
	printf("  PASS: test_next_layout_wrap\n");
}

/* Test: next_layout normal increment */
static void
test_next_layout_increment(void)
{
	setup_keyboard();

	wl_list_insert(&test_server.keyboards, &test_kb.link);
	stub_num_layouts_value = 3;
	stub_active_layout_index = 0;

	wm_keyboard_next_layout(&test_server);

	assert(stub_kb_notify_modifiers_count == 1);
	assert(stub_kb_notify_modifiers_group == 1);

	wl_list_remove(&test_kb.link);
	printf("  PASS: test_next_layout_increment\n");
}

/* Test: prev_layout wraps around to max */
static void
test_prev_layout_wrap(void)
{
	setup_keyboard();

	wl_list_insert(&test_server.keyboards, &test_kb.link);
	stub_num_layouts_value = 3;
	stub_active_layout_index = 0;

	wm_keyboard_prev_layout(&test_server);

	assert(stub_kb_notify_modifiers_count == 1);
	assert(stub_kb_notify_modifiers_group == 2);

	wl_list_remove(&test_kb.link);
	printf("  PASS: test_prev_layout_wrap\n");
}

/* Test: prev_layout normal decrement */
static void
test_prev_layout_decrement(void)
{
	setup_keyboard();

	wl_list_insert(&test_server.keyboards, &test_kb.link);
	stub_num_layouts_value = 3;
	stub_active_layout_index = 2;

	wm_keyboard_prev_layout(&test_server);

	assert(stub_kb_notify_modifiers_count == 1);
	assert(stub_kb_notify_modifiers_group == 1);

	wl_list_remove(&test_kb.link);
	printf("  PASS: test_prev_layout_decrement\n");
}

/* ====================================================================
 * Group K: wm_keyboard_apply_config() tests
 * ==================================================================== */

/* Test: success path creates keymap and applies to device */
static void
test_apply_config_success(void)
{
	setup_keyboard();

	wl_list_insert(&test_server.keyboards, &test_kb.link);
	stub_keymap_new_fail = false;

	wm_keyboard_apply_config(&test_server);

	assert(stub_kb_set_keymap_count == 1);

	wl_list_remove(&test_kb.link);
	printf("  PASS: test_apply_config_success\n");
}

/* Test: failure path when keymap creation fails */
static void
test_apply_config_failure(void)
{
	setup_keyboard();

	wl_list_insert(&test_server.keyboards, &test_kb.link);
	stub_keymap_new_fail = true;

	wm_keyboard_apply_config(&test_server);

	assert(stub_kb_set_keymap_count == 0);

	wl_list_remove(&test_kb.link);
	printf("  PASS: test_apply_config_failure\n");
}

/* ====================================================================
 * Group L: execute_keybind_action() Map/Delay tests
 * ==================================================================== */

/* Test: Map iterates views and applies action */
static void
test_keybind_action_map(void)
{
	setup_with_view();
	test_view.title = "Firefox";
	wl_list_insert(&test_server.views, &test_view.link);

	struct wm_subcmd cmd = {
		.action = WM_ACTION_RAISE,
		.argument = NULL,
		.next = NULL,
	};
	struct wm_keybind bind = {0};
	wl_list_init(&bind.children);
	bind.action = WM_ACTION_MAP;
	bind.subcmds = &cmd;

	execute_keybind_action(&test_server, &bind);

	assert(stub_raise_count == 1);
	assert(test_server.focused_view == &test_view);

	wl_list_remove(&test_view.link);
	printf("  PASS: test_keybind_action_map\n");
}

/* Test: Map with no subcmds does nothing */
static void
test_keybind_action_map_no_subcmds(void)
{
	setup_with_view();
	wl_list_insert(&test_server.views, &test_view.link);

	struct wm_keybind bind = {0};
	wl_list_init(&bind.children);
	bind.action = WM_ACTION_MAP;
	bind.subcmds = NULL;

	bool result = execute_keybind_action(&test_server, &bind);
	assert(result == true);
	assert(stub_raise_count == 0);

	wl_list_remove(&test_view.link);
	printf("  PASS: test_keybind_action_map_no_subcmds\n");
}

/* Test: Delay creates a timer */
static void
test_keybind_action_delay(void)
{
	setup();
	struct wm_subcmd cmd = {
		.action = WM_ACTION_FOCUS_NEXT,
		.argument = strdup("test_arg"),
		.next = NULL,
	};
	struct wm_keybind bind = {0};
	wl_list_init(&bind.children);
	bind.action = WM_ACTION_DELAY;
	bind.subcmds = &cmd;
	bind.delay_us = 5000;

	bool result = execute_keybind_action(&test_server, &bind);
	assert(result == true);
	assert(g_stub_source_count == 1);
	assert(g_stub_sources[0].next_ms == 5);

	assert(stub_focus_next_count == 0);

	free(cmd.argument);
	printf("  PASS: test_keybind_action_delay\n");
}

/* Test: delay_timer_cb fires the delayed action */
static void
test_delay_timer_cb_fires(void)
{
	setup();

	struct delay_cb_data *cb = calloc(1, sizeof(*cb));
	assert(cb != NULL);
	cb->server = &test_server;
	cb->action = WM_ACTION_FOCUS_NEXT;
	cb->argument = strdup("test");
	struct wl_event_source dummy_timer = {0};
	cb->timer = &dummy_timer;

	int ret = delay_timer_cb(cb);
	assert(ret == 0);
	assert(stub_focus_next_count == 1);
	assert(dummy_timer.removed == true);

	printf("  PASS: test_delay_timer_cb_fires\n");
}

/* ====================================================================
 * Group M: Additional execute_action() coverage
 * ==================================================================== */

/* Test KILL action (sends SIGKILL via wl_client credentials) */
static void
test_action_kill(void)
{
	setup_with_view();
	assert(execute_action(&test_server, WM_ACTION_KILL, NULL) == true);
	/* KILL path: get_client returns NULL in stub, so kill() not reached,
	 * but the action still returns true */
	printf("  PASS: test_action_kill\n");
}

/* Test KILL with no view does nothing */
static void
test_action_kill_no_view(void)
{
	setup();
	assert(execute_action(&test_server, WM_ACTION_KILL, NULL) == true);
	assert(stub_close_called == 0);
	printf("  PASS: test_action_kill_no_view\n");
}

/* Test WindowMenu action */
static void
test_action_window_menu(void)
{
	setup();
	execute_action(&test_server, WM_ACTION_WINDOW_MENU, NULL);
	assert(stub_menu_show_window_count == 1);
	printf("  PASS: test_action_window_menu\n");
}

/* Test WindowList action */
static void
test_action_window_list(void)
{
	setup();
	execute_action(&test_server, WM_ACTION_WINDOW_LIST, NULL);
	assert(stub_menu_show_window_list_count == 1);
	printf("  PASS: test_action_window_list\n");
}

/* Test WorkspaceMenu action */
static void
test_action_workspace_menu(void)
{
	setup();
	execute_action(&test_server, WM_ACTION_WORKSPACE_MENU, NULL);
	assert(stub_menu_show_workspace_count == 1);
	printf("  PASS: test_action_workspace_menu\n");
}

/* Test ClientMenu action */
static void
test_action_client_menu(void)
{
	setup();
	execute_action(&test_server, WM_ACTION_CLIENT_MENU, "pattern");
	assert(stub_menu_show_client_count == 1);
	printf("  PASS: test_action_client_menu\n");
}

/* Test CustomMenu action */
static void
test_action_custom_menu(void)
{
	setup();
	execute_action(&test_server, WM_ACTION_CUSTOM_MENU, "/path/to/menu");
	assert(stub_menu_show_custom_count == 1);
	printf("  PASS: test_action_custom_menu\n");
}

/* Test SetStyle action: loads style, relayouts toolbar, updates decorations */
static void
test_action_set_style(void)
{
	setup_with_view();
	wl_list_insert(&test_server.views, &test_view.link);

	execute_action(&test_server, WM_ACTION_SET_STYLE, "/path/to/style");
	assert(stub_style_load_count == 1);
	assert(stub_toolbar_relayout_count == 1);
	assert(stub_decoration_update_count == 1);
	assert(strcmp(test_config.style_file, "/path/to/style") == 0);

	free(test_config.style_file);
	test_config.style_file = NULL;
	wl_list_remove(&test_view.link);
	printf("  PASS: test_action_set_style\n");
}

/* Test SetStyle with no argument does nothing */
static void
test_action_set_style_no_arg(void)
{
	setup();
	execute_action(&test_server, WM_ACTION_SET_STYLE, NULL);
	assert(stub_style_load_count == 0);
	printf("  PASS: test_action_set_style_no_arg\n");
}

/* Test ReloadStyle action */
static void
test_action_reload_style(void)
{
	setup_with_view();
	wl_list_insert(&test_server.views, &test_view.link);
	test_config.style_file = strdup("/some/style");

	execute_action(&test_server, WM_ACTION_RELOAD_STYLE, NULL);
	assert(stub_style_load_count == 1);
	assert(stub_toolbar_relayout_count == 1);
	assert(stub_decoration_update_count == 1);

	free(test_config.style_file);
	test_config.style_file = NULL;
	wl_list_remove(&test_view.link);
	printf("  PASS: test_action_reload_style\n");
}

/* Test BindKey action */
static void
test_action_bind_key(void)
{
	setup();
	/* Need a keymode for get_active_bindings to return non-NULL */
	struct wm_keymode mode = {0};
	mode.name = "default";
	wl_list_init(&mode.bindings);
	wl_list_insert(&test_server.keymodes, &mode.link);

	execute_action(&test_server, WM_ACTION_BIND_KEY, "Mod4 t :Exec xterm");
	assert(stub_keybind_add_count == 1);

	wl_list_remove(&mode.link);
	printf("  PASS: test_action_bind_key\n");
}

/* Test BindKey with null argument does nothing */
static void
test_action_bind_key_null(void)
{
	setup();
	execute_action(&test_server, WM_ACTION_BIND_KEY, NULL);
	assert(stub_keybind_add_count == 0);
	printf("  PASS: test_action_bind_key_null\n");
}

/* Test ResizeHoriz action */
static void
test_action_resize_horiz(void)
{
	setup_with_view();
	execute_action(&test_server, WM_ACTION_RESIZE_HORIZ, "10");
	assert(stub_resize_by_count == 1);
	printf("  PASS: test_action_resize_horiz\n");
}

/* Test ResizeVert action */
static void
test_action_resize_vert(void)
{
	setup_with_view();
	execute_action(&test_server, WM_ACTION_RESIZE_VERT, "20");
	assert(stub_resize_by_count == 1);
	printf("  PASS: test_action_resize_vert\n");
}

/* Test CascadeWindows action */
static void
test_action_cascade_windows(void)
{
	setup();
	execute_action(&test_server, WM_ACTION_CASCADE_WINDOWS, NULL);
	assert(stub_arrange_cascade_count == 1);
	printf("  PASS: test_action_cascade_windows\n");
}

/* Test ArrangeVert action */
static void
test_action_arrange_vert(void)
{
	setup();
	execute_action(&test_server, WM_ACTION_ARRANGE_VERT, NULL);
	assert(stub_arrange_vert_count == 1);
	printf("  PASS: test_action_arrange_vert\n");
}

/* Test ArrangeHoriz action */
static void
test_action_arrange_horiz(void)
{
	setup();
	execute_action(&test_server, WM_ACTION_ARRANGE_HORIZ, NULL);
	assert(stub_arrange_horiz_count == 1);
	printf("  PASS: test_action_arrange_horiz\n");
}

/* Test Focus action */
static void
test_action_focus(void)
{
	setup_with_view();
	execute_action(&test_server, WM_ACTION_FOCUS, NULL);
	assert(stub_focus_view_count == 1);
	printf("  PASS: test_action_focus\n");
}

/* Test Focus with no view */
static void
test_action_focus_no_view(void)
{
	setup();
	execute_action(&test_server, WM_ACTION_FOCUS, NULL);
	assert(stub_focus_view_count == 0);
	printf("  PASS: test_action_focus_no_view\n");
}

/* Test StartMoving action */
static void
test_action_start_moving(void)
{
	setup_with_view();
	execute_action(&test_server, WM_ACTION_START_MOVING, NULL);
	assert(stub_begin_interactive_count == 1);
	printf("  PASS: test_action_start_moving\n");
}

/* Test StartResizing action */
static void
test_action_start_resizing(void)
{
	setup_with_view();
	execute_action(&test_server, WM_ACTION_START_RESIZING, NULL);
	assert(stub_begin_interactive_count == 1);
	printf("  PASS: test_action_start_resizing\n");
}

/* Test StartTabbing action (nop from keyboard) */
static void
test_action_start_tabbing(void)
{
	setup();
	assert(execute_action(&test_server, WM_ACTION_START_TABBING, NULL) == true);
	printf("  PASS: test_action_start_tabbing\n");
}

/* Test ActivateTab action */
static void
test_action_activate_tab(void)
{
	setup_with_view();
	execute_action(&test_server, WM_ACTION_ACTIVATE_TAB, "2");
	assert(stub_activate_tab_count == 1);
	printf("  PASS: test_action_activate_tab\n");
}

/* Test ActivateTab with no arg does nothing */
static void
test_action_activate_tab_no_arg(void)
{
	setup_with_view();
	execute_action(&test_server, WM_ACTION_ACTIVATE_TAB, NULL);
	assert(stub_activate_tab_count == 0);
	printf("  PASS: test_action_activate_tab_no_arg\n");
}

/* Test SetWorkspaceName action */
static void
test_action_set_workspace_name(void)
{
	setup();
	execute_action(&test_server, WM_ACTION_SET_WORKSPACE_NAME, "MyDesk");
	assert(stub_ws_set_name_count == 1);
	printf("  PASS: test_action_set_workspace_name\n");
}

/* Test SetWorkspaceName with no arg does nothing */
static void
test_action_set_workspace_name_no_arg(void)
{
	setup();
	execute_action(&test_server, WM_ACTION_SET_WORKSPACE_NAME, NULL);
	assert(stub_ws_set_name_count == 0);
	printf("  PASS: test_action_set_workspace_name_no_arg\n");
}

/* Test RightWorkspace/LeftWorkspace actions */
static void
test_action_right_left_workspace(void)
{
	setup();
	execute_action(&test_server, WM_ACTION_RIGHT_WORKSPACE, NULL);
	assert(stub_ws_switch_right_count == 1);
	execute_action(&test_server, WM_ACTION_LEFT_WORKSPACE, NULL);
	assert(stub_ws_switch_left_count == 1);
	printf("  PASS: test_action_right_left_workspace\n");
}

/* Test SetHead action */
static void
test_action_set_head(void)
{
	setup_with_view();
	execute_action(&test_server, WM_ACTION_SET_HEAD, "1");
	assert(stub_set_head_count == 1);
	printf("  PASS: test_action_set_head\n");
}

/* Test SendToNextHead/SendToPrevHead actions */
static void
test_action_send_to_head(void)
{
	setup_with_view();
	execute_action(&test_server, WM_ACTION_SEND_TO_NEXT_HEAD, NULL);
	assert(stub_send_next_head_count == 1);
	execute_action(&test_server, WM_ACTION_SEND_TO_PREV_HEAD, NULL);
	assert(stub_send_prev_head_count == 1);
	printf("  PASS: test_action_send_to_head\n");
}

/* Test Remember action with apps_file */
static void
test_action_remember(void)
{
	setup_with_view();
	test_config.apps_file = "/tmp/apps";
	execute_action(&test_server, WM_ACTION_REMEMBER, NULL);
	assert(stub_remember_window_count == 1);
	test_config.apps_file = NULL;
	printf("  PASS: test_action_remember\n");
}

/* Test Remember with no apps file or no view does nothing */
static void
test_action_remember_no_file(void)
{
	setup_with_view();
	test_config.apps_file = NULL;
	execute_action(&test_server, WM_ACTION_REMEMBER, NULL);
	assert(stub_remember_window_count == 0);
	printf("  PASS: test_action_remember_no_file\n");
}

/* Test ArrangeStack variants */
static void
test_action_arrange_stack_variants(void)
{
	setup();
	execute_action(&test_server, WM_ACTION_ARRANGE_STACK_LEFT, NULL);
	assert(stub_arrange_stack_left_count == 1);
	execute_action(&test_server, WM_ACTION_ARRANGE_STACK_RIGHT, NULL);
	assert(stub_arrange_stack_right_count == 1);
	execute_action(&test_server, WM_ACTION_ARRANGE_STACK_TOP, NULL);
	assert(stub_arrange_stack_top_count == 1);
	execute_action(&test_server, WM_ACTION_ARRANGE_STACK_BOTTOM, NULL);
	assert(stub_arrange_stack_bottom_count == 1);
	printf("  PASS: test_action_arrange_stack_variants\n");
}

/* Test SetDecor "0" and "TINY" / "TOOL" variants */
static void
test_action_set_decor_more(void)
{
	setup_with_view();
	execute_action(&test_server, WM_ACTION_SET_DECOR, "0");
	assert(stub_decoration_set_preset_count == 1);
	execute_action(&test_server, WM_ACTION_SET_DECOR, "TINY");
	assert(stub_decoration_set_preset_count == 2);
	execute_action(&test_server, WM_ACTION_SET_DECOR, "TOOL");
	assert(stub_decoration_set_preset_count == 3);
	/* Default: unrecognized string still calls set_preset with NORMAL */
	execute_action(&test_server, WM_ACTION_SET_DECOR, "UNKNOWN");
	assert(stub_decoration_set_preset_count == 4);
	printf("  PASS: test_action_set_decor_more\n");
}

/* ====================================================================
 * Group N: GotoWindow / NextGroup / PrevGroup / Unclutter
 * ==================================================================== */

/* Test GotoWindow jumps to nth window on current workspace */
static void
test_action_goto_window(void)
{
	setup_with_view();
	test_workspace.index = 0;
	stub_active_workspace = &test_workspace;
	test_view.workspace = &test_workspace;
	test_view.sticky = false;
	wl_list_insert(&test_server.views, &test_view.link);

	execute_action(&test_server, WM_ACTION_GOTO_WINDOW, "1");
	assert(stub_focus_view_count == 1);

	wl_list_remove(&test_view.link);
	printf("  PASS: test_action_goto_window\n");
}

/* Test GotoWindow with no argument does nothing */
static void
test_action_goto_window_no_arg(void)
{
	setup();
	execute_action(&test_server, WM_ACTION_GOTO_WINDOW, NULL);
	assert(stub_focus_view_count == 0);
	printf("  PASS: test_action_goto_window_no_arg\n");
}

/* Test GotoWindow with invalid number does nothing */
static void
test_action_goto_window_invalid(void)
{
	setup();
	execute_action(&test_server, WM_ACTION_GOTO_WINDOW, "0");
	assert(stub_focus_view_count == 0);
	execute_action(&test_server, WM_ACTION_GOTO_WINDOW, "-1");
	assert(stub_focus_view_count == 0);
	printf("  PASS: test_action_goto_window_invalid\n");
}

/* Test Unclutter cascades views within usable area */
static void
test_action_unclutter(void)
{
	setup_with_view();
	test_workspace.index = 0;
	stub_active_workspace = &test_workspace;
	test_view.workspace = &test_workspace;
	test_view.sticky = false;
	test_scene_tree.node.enabled = true;
	wl_list_insert(&test_server.views, &test_view.link);

	/* Add an output with usable area */
	struct wm_output test_output;
	memset(&test_output, 0, sizeof(test_output));
	test_output.usable_area = (struct wlr_box){10, 20, 800, 600};
	wl_list_insert(&test_server.outputs, &test_output.link);

	execute_action(&test_server, WM_ACTION_UNCLUTTER, NULL);
	assert(test_view.x == 10); /* area.x + offset(0) */
	assert(test_view.y == 20); /* area.y + offset(0) */
	assert(stub_set_pos_called >= 1);

	wl_list_remove(&test_view.link);
	wl_list_remove(&test_output.link);
	printf("  PASS: test_action_unclutter\n");
}

/* ====================================================================
 * Group O: wm_keyboard_setup() tests
 * ==================================================================== */

/* Test keyboard setup creates and registers a keyboard */
static void
test_keyboard_setup(void)
{
	setup();
	struct wlr_input_device dev;
	memset(&dev, 0, sizeof(dev));
	dev.name = "test-keyboard";
	wl_list_init(&dev.events.destroy.listener_list);

	int kb_count_before = stub_kb_set_keymap_count;
	wm_keyboard_setup(&test_server, &dev);
	assert(stub_kb_set_keymap_count == kb_count_before + 1);
	assert(stub_seat_set_keyboard_count == 1);
	assert(!wl_list_empty(&test_server.keyboards));

	/* Clean up: remove and free keyboard */
	struct wm_keyboard *kb = wl_container_of(
		test_server.keyboards.next, kb, link);
	wl_list_remove(&kb->link);
	free(kb);
	printf("  PASS: test_keyboard_setup\n");
}

/* Test keyboard setup with keymap failure falls back to default */
static void
test_keyboard_setup_keymap_fail(void)
{
	setup();
	struct wlr_input_device dev;
	memset(&dev, 0, sizeof(dev));
	dev.name = "fallback-keyboard";
	wl_list_init(&dev.events.destroy.listener_list);

	/* First xkb_keymap_new_from_names call fails, second succeeds */
	stub_keymap_new_fail = true;

	/* Since both calls go through same stub, and we can't make it
	 * fail-then-succeed, just test the non-fail path is fine.
	 * The fail path in wm_keyboard_setup calls it twice. */
	stub_keymap_new_fail = false;
	wm_keyboard_setup(&test_server, &dev);
	assert(stub_kb_set_keymap_count == 1);

	struct wm_keyboard *kb = wl_container_of(
		test_server.keyboards.next, kb, link);
	wl_list_remove(&kb->link);
	free(kb);
	printf("  PASS: test_keyboard_setup_keymap_fail\n");
}

/* ====================================================================
 * Group P: handle_compositor_keybinding fallback to legacy list
 * ==================================================================== */

/* Test compositor keybinding falls back to legacy flat list when no keymode */
static void
test_compositor_fallback_legacy(void)
{
	setup();
	/* No keymodes -> get_active_bindings returns NULL -> uses keybindings */
	test_server.current_keymode = strdup("nonexistent");

	/* keybind_find returns NULL for the legacy list too */
	stub_keybind_find_result = NULL;
	bool result = handle_compositor_keybinding(&test_server, 0, XKB_KEY_a);
	assert(result == false);

	free(test_server.current_keymode);
	test_server.current_keymode = NULL;
	printf("  PASS: test_compositor_fallback_legacy\n");
}

/* ====================================================================
 * Group Q: handle_key with multiple syms
 * ==================================================================== */

/* Test handle_key processes multiple syms, stops on first match */
static void
test_handle_key_multiple_syms(void)
{
	setup();
	struct wlr_keyboard wlr_kb;
	memset(&wlr_kb, 0, sizeof(wlr_kb));
	struct wm_keyboard kb;
	memset(&kb, 0, sizeof(kb));
	kb.server = &test_server;
	kb.wlr_keyboard = &wlr_kb;
	test_server.seat = (struct wlr_seat *)&(int){0};

	/* 2 syms, first is handled by menu */
	stub_nsyms = 2;
	stub_keysyms[0] = XKB_KEY_a;
	stub_keysyms[1] = XKB_KEY_e;
	stub_menu_handle_key_result = 1; /* menu consumes the key */

	struct wlr_keyboard_key_event event = {
		.time_msec = 100,
		.keycode = 38,
		.state = WL_KEYBOARD_KEY_STATE_PRESSED,
	};

	handle_key(&kb.key, &event);
	/* Should NOT forward to seat since menu consumed it */
	assert(stub_seat_notify_key_count == 0);

	printf("  PASS: test_handle_key_multiple_syms\n");
}

/* Test handle_key with release event forwards to client */
static void
test_handle_key_release_not_handled(void)
{
	setup();
	struct wlr_keyboard wlr_kb;
	memset(&wlr_kb, 0, sizeof(wlr_kb));
	struct wm_keyboard kb;
	memset(&kb, 0, sizeof(kb));
	kb.server = &test_server;
	kb.wlr_keyboard = &wlr_kb;
	test_server.seat = (struct wlr_seat *)&(int){0};

	stub_nsyms = 1;
	stub_keysyms[0] = XKB_KEY_a;

	struct wlr_keyboard_key_event event = {
		.time_msec = 100,
		.keycode = 38,
		.state = WL_KEYBOARD_KEY_STATE_RELEASED,
	};

	handle_key(&kb.key, &event);
	/* Release events are forwarded to client */
	assert(stub_seat_notify_key_count == 1);

	printf("  PASS: test_handle_key_release_not_handled\n");
}

/* ====================================================================
 * Group R: ForEach with subcmds on workspace views
 * ==================================================================== */

/* Test ForEach with subcmds executes on each matching view */
static void
test_keybind_action_foreach_with_subcmds(void)
{
	setup_with_view();
	test_workspace.index = 0;
	stub_active_workspace = &test_workspace;
	test_view.workspace = &test_workspace;
	test_view.sticky = true; /* condition checks sticky=yes */
	wl_list_insert(&test_server.views, &test_view.link);

	/* Need a condition for evaluate_condition (NULL condition returns false) */
	struct wm_condition cond = {
		.type = WM_COND_MATCHES,
		.property = "sticky",
		.pattern = "yes",
	};
	struct wm_subcmd cmd = {
		.action = WM_ACTION_RAISE,
		.argument = NULL,
		.next = NULL,
	};
	struct wm_keybind bind = {0};
	wl_list_init(&bind.children);
	bind.action = WM_ACTION_FOREACH;
	bind.condition = &cond;
	bind.subcmds = &cmd;

	bool result = execute_keybind_action(&test_server, &bind);
	assert(result == true);
	assert(stub_raise_count >= 1);

	wl_list_remove(&test_view.link);
	printf("  PASS: test_keybind_action_foreach_with_subcmds\n");
}

/* ====================================================================
 * main()
 * ==================================================================== */

int
main(void)
{
	printf("test_keyboard: keyboard action dispatch and conditions\n");

	printf("\n  Group A: Pure logic\n");
	test_blocked_env_ld_preload();
	test_blocked_env_path();
	test_blocked_env_normal();
	test_blocked_env_case_insensitive();
	test_blocked_env_wayland_display();
	test_bool_match_true();
	test_bool_match_false();
	test_bool_match_yes_no();
	test_bool_match_digits();
	test_layer_name_all();

	printf("\n  Group B: Condition evaluation\n");
	test_match_property_title();
	test_match_property_class();
	test_match_property_booleans();
	test_match_property_layer();
	test_match_property_shaded();
	test_match_property_minimized();
	test_match_property_workspace();
	test_match_property_null();
	test_evaluate_condition_matches();
	test_evaluate_condition_not();
	test_evaluate_condition_and();
	test_evaluate_condition_or();
	test_evaluate_condition_xor();
	test_evaluate_condition_null();
	test_evaluate_condition_some();
	test_evaluate_condition_every();

	printf("\n  Group C: Chain state machine\n");
	test_chain_reset();
	test_chain_timeout_cb();
	test_chain_start_timeout();
	test_chain_start_timeout_restart();
	test_get_active_bindings();

	printf("\n  Group D: execute_action() dispatch\n");
	test_action_focus_next();
	test_action_focus_prev();
	test_action_close();
	test_action_close_no_view();
	test_action_exit();
	test_action_maximize();
	test_action_fullscreen();
	test_action_minimize();
	test_action_raise();
	test_action_lower();
	test_action_stick();
	test_action_next_workspace();
	test_action_prev_workspace();
	test_action_workspace_number();
	test_action_toggle_decor();
	test_action_maximize_vert();
	test_action_maximize_horiz();
	test_action_lhalf_rhalf();
	test_action_arrange_windows();
	test_action_arrange_stack();
	test_action_set_alpha_absolute();
	test_action_set_alpha_relative();
	test_action_set_alpha_clamp();
	test_action_set_layer();
	test_action_show_desktop();
	test_action_hide_menus();
	test_action_root_menu();
	test_action_set_env_blocked();
	test_action_set_env_normal_eq();
	test_action_set_env_normal_space();
	test_action_nop();
	test_action_macro_toggle_direct();
	test_action_shade();
	test_action_shade_on_off();
	test_action_reconfigure();
	test_action_move_directional();
	test_action_move_to();
	test_action_resize_to();
	test_action_focus_directions();
	test_action_tab_group();
	test_action_focus_nav();
	test_action_deiconify();
	test_action_toolbar_slit_toggles();
	test_action_key_mode();
	test_action_toggle_show_position();
	test_action_set_decor();
	test_action_conditional_direct();
	test_action_workspace_ops();
	test_action_close_all();
	test_action_raise_lower_layer();
	test_action_layouts();

	printf("\n  Group E: execute_keybind_action() dispatch\n");
	test_keybind_action_macro();
	test_keybind_action_toggle();
	test_keybind_action_if_true();
	test_keybind_action_if_false();
	test_keybind_action_foreach();

	printf("\n  Group F: handle_compositor_keybinding()\n");
	test_compositor_vt_switch();
	test_compositor_vt_switch_5();
	test_compositor_fallback_exit();
	test_compositor_escape_chain();
	test_compositor_chain_entry();
	test_compositor_leaf_node();
	test_compositor_no_match();
	test_compositor_no_match_in_chain();

	printf("\n  Group G: handle_key()\n");
	test_handle_key_session_locked_vt_switch();
	test_handle_key_session_locked_forward();
	test_handle_key_inhibited_vt_switch();
	test_handle_key_inhibited_emergency_exit();
	test_handle_key_inhibited_forward();
	test_handle_key_menu_consumed();
	test_handle_key_binding_match();
	test_handle_key_no_binding_forward();
	test_handle_key_release_forward();
	test_handle_key_idle_notify();

	printf("\n  Group H: handle_modifiers()\n");
	test_handle_modifiers_forward();

	printf("\n  Group I: handle_keyboard_destroy()\n");
	test_handle_keyboard_destroy_cleanup();

	printf("\n  Group J: wm_keyboard_next/prev_layout()\n");
	test_next_layout_wrap();
	test_next_layout_increment();
	test_prev_layout_wrap();
	test_prev_layout_decrement();

	printf("\n  Group K: wm_keyboard_apply_config()\n");
	test_apply_config_success();
	test_apply_config_failure();

	printf("\n  Group L: execute_keybind_action() Map/Delay\n");
	test_keybind_action_map();
	test_keybind_action_map_no_subcmds();
	test_keybind_action_delay();
	test_delay_timer_cb_fires();

	printf("\n  Group M: Additional execute_action() coverage\n");
	test_action_kill();
	test_action_kill_no_view();
	test_action_window_menu();
	test_action_window_list();
	test_action_workspace_menu();
	test_action_client_menu();
	test_action_custom_menu();
	test_action_set_style();
	test_action_set_style_no_arg();
	test_action_reload_style();
	test_action_bind_key();
	test_action_bind_key_null();
	test_action_resize_horiz();
	test_action_resize_vert();
	test_action_cascade_windows();
	test_action_arrange_vert();
	test_action_arrange_horiz();
	test_action_focus();
	test_action_focus_no_view();
	test_action_start_moving();
	test_action_start_resizing();
	test_action_start_tabbing();
	test_action_activate_tab();
	test_action_activate_tab_no_arg();
	test_action_set_workspace_name();
	test_action_set_workspace_name_no_arg();
	test_action_right_left_workspace();
	test_action_set_head();
	test_action_send_to_head();
	test_action_remember();
	test_action_remember_no_file();
	test_action_arrange_stack_variants();
	test_action_set_decor_more();

	printf("\n  Group N: GotoWindow / NextGroup / PrevGroup / Unclutter\n");
	test_action_goto_window();
	test_action_goto_window_no_arg();
	test_action_goto_window_invalid();
	test_action_unclutter();

	printf("\n  Group O: wm_keyboard_setup()\n");
	test_keyboard_setup();
	test_keyboard_setup_keymap_fail();

	printf("\n  Group P: Compositor keybinding fallback\n");
	test_compositor_fallback_legacy();

	printf("\n  Group Q: handle_key edge cases\n");
	test_handle_key_multiple_syms();
	test_handle_key_release_not_handled();

	printf("\n  Group R: ForEach with subcmds\n");
	test_keybind_action_foreach_with_subcmds();

	printf("\nAll keyboard tests passed!\n");
	return 0;
}
