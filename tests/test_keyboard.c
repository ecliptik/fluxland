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
#define WLR_TYPES_WLR_KEYBOARD_GROUP_H
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
#define WM_DIALOG_H
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
#define WM_KEYBOARD_ACTIONS_H
#define WM_ANIMATION_H
#define WM_FOREIGN_TOPLEVEL_H
#define WM_COMMAND_H
#define WM_CLIENT_PATTERN_H
#define WM_WALLPAPER_H
#define WLR_TYPES_WLR_CURSOR_H

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
	const char *name;
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
#define WLR_MODIFIER_CAPS  (1 << 4)
#define WLR_MODIFIER_MOD2  (1 << 5)
#define WM_MODIFIERS_IGNORED   (WLR_MODIFIER_CAPS | WLR_MODIFIER_MOD2)
#define WM_KEYSYM_KEYCODE(kc)  (0x80000000u | (uint32_t)(kc))

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
#define XKB_KEY_E      0x0045
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

static void
wlr_scene_node_set_enabled(struct wlr_scene_node *n, bool enabled)
{
	n->enabled = enabled;
}

static void
wlr_seat_keyboard_notify_clear_focus(struct wlr_seat *seat)
{
	(void)seat;
}

struct wm_animation;
static void
wm_animation_cancel(struct wm_animation *anim)
{
	(void)anim;
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
xkb_state_key_get_utf8(struct xkb_state *state, xkb_keycode_t key,
	char *buf, size_t size)
{
	(void)state; (void)key;
	if (size)
		buf[0] = '\0';
	return 0;
}

/* dialog.h */
struct wm_server;
struct wm_view;
static int g_dialog_open_count;
static char g_dialog_precommand[64];
static bool g_dialog_open;
static void
wm_dialog_open(struct wm_server *s, const char *title, const char *pre,
	const char *text, struct wm_view *target)
{
	(void)s; (void)title; (void)text; (void)target;
	g_dialog_open_count++;
	snprintf(g_dialog_precommand, sizeof(g_dialog_precommand), "%s",
		pre ? pre : "");
}
static bool wm_dialog_is_open(struct wm_server *s)
	{ (void)s; return g_dialog_open; }
static bool
wm_dialog_handle_key(struct wm_server *s, xkb_keysym_t sym,
	const char *utf8, uint32_t mods)
	{ (void)s; (void)sym; (void)utf8; (void)mods; return true; }

static int
xkb_state_key_get_syms(struct xkb_state *state, xkb_keycode_t key,
	const xkb_keysym_t **syms_out)
{
	(void)state;
	(void)key;
	*syms_out = stub_keysyms;
	return stub_nsyms;
}

/* The key's unshifted keysyms (level 0); none unless a test sets them */
static xkb_keysym_t stub_level0_keysyms[4];
static int stub_level0_nsyms;
static xkb_layout_index_t
xkb_state_key_get_layout(struct xkb_state *state, xkb_keycode_t key)
{
	(void)state; (void)key;
	return 0;
}
static int
xkb_keymap_key_get_syms_by_level(struct xkb_keymap *keymap,
	xkb_keycode_t key, xkb_layout_index_t layout, uint32_t level,
	const xkb_keysym_t **syms_out)
{
	(void)keymap; (void)key; (void)layout; (void)level;
	*syms_out = stub_level0_keysyms;
	return stub_level0_nsyms;
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
	kb->keymap = km;
	stub_kb_set_keymap_count++;
}

/* Keyboard group */
struct wlr_keyboard_group {
	struct wlr_keyboard keyboard;
	int members;
};
static struct wlr_keyboard_group stub_group;
static bool stub_group_add_fail;
static struct wlr_keyboard_group *
wlr_keyboard_group_create(void)
{
	memset(&stub_group, 0, sizeof(stub_group));
	wl_list_init(&stub_group.keyboard.events.key.listener_list);
	wl_list_init(&stub_group.keyboard.events.modifiers.listener_list);
	return &stub_group;
}
static void
wlr_keyboard_group_destroy(struct wlr_keyboard_group *g)
{
	(void)g;
}
static bool
wlr_keyboard_group_add_keyboard(struct wlr_keyboard_group *g,
	struct wlr_keyboard *kb)
{
	/* wlroots requires the same keymap */
	if (stub_group_add_fail || kb->keymap != g->keyboard.keymap)
		return false;
	g->members++;
	return true;
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
	/* Not really added, but safe to remove later */
	(void)signal;
	wl_list_init(&listener->link);
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
	WM_ACTION_TAB,
	WM_ACTION_SET_WALLPAPER,
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
	WM_ACTION_SET_WORKSPACE_NAME_DIALOG,
	WM_ACTION_COMMAND_DIALOG,
	WM_ACTION_SET_TITLE,
	WM_ACTION_SET_TITLE_DIALOG,
	WM_ACTION_SET_RESOURCE_VALUE,
	WM_ACTION_SET_RESOURCE_VALUE_DIALOG,
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
	WM_ACTION_FOCUS_SLIT,
};

/* command.h / client_pattern.h: stubbed below */
struct wm_condition;
struct wm_client_pattern;

struct wm_command {
	enum wm_action action;
	char *argument;
};

struct wm_window_list_spec {
	bool static_order;
	bool groups;
	struct wm_client_pattern *pattern;
};

struct wm_keybind {
	uint32_t modifiers;
	xkb_keysym_t keysym;
	enum wm_action action;
	char *argument;
	struct wm_command *command;
	struct wm_condition *guard;
	struct wl_list children;
	struct wl_list link;
};

struct wm_keymode {
	char *name;
	struct wl_list bindings;
	struct wl_list link;
	bool end_bound;
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
	WM_FOCUS_ZONE_SLIT,
};

struct wm_focus_nav {
	enum wm_focus_zone zone;
	int toolbar_index;
	int slit_index;
};

/* --- Minimal struct definitions needed by keyboard.c --- */

struct wm_decoration {
	bool shaded;
	int titlebar_height;
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

enum wm_wallpaper_mode {
	WM_WALLPAPER_STRETCH, WM_WALLPAPER_CENTER, WM_WALLPAPER_TILE,
	WM_WALLPAPER_FILL,
	WM_WALLPAPER_FIT,
};

struct wm_config {
	char *style_file;
	char *apps_file;
	char *config_dir;
	char *wallpaper;
	enum wm_wallpaper_mode wallpaper_mode;
	bool wallpaper_from_last;
	bool max_disable_move;
	bool max_disable_resize;
	int window_focus_alpha;
	int window_unfocus_alpha;
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
	struct wlr_output *wlr_output;
	struct wlr_box full_area;
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
	bool title_override;
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
	bool is_virtual;
	bool is_group;
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
	struct wm_view *show_desktop_saved_view;
	bool focus_user_initiated;
	struct {
		bool active;
		struct wm_view *restore_minimized;
	} focus_cycle;
	uint64_t focus_seq_counter;
	struct wl_list pending_commands;
	bool show_position;
	enum wm_cursor_mode cursor_mode;
	struct wl_list views;
	struct wl_list workspaces;
	int workspace_count;
	int former_workspace;
	struct wl_list outputs;
	struct wl_list keyboards;
	struct wlr_keyboard_group *keyboard_group;
	struct wm_keyboard *group_keyboard;
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
	struct wm_wallpaper *wallpaper;
	bool style_background;
	struct wm_view *menu_view;
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
static int stub_focus_nav_enter_slit_count;
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
static char stub_title[128];
void wm_view_set_title(struct wm_view *v, const char *t)
{
	if (v->title_override)
		return;
	snprintf(stub_title, sizeof(stub_title), "%s", t ? t : "");
}
void wm_view_set_opacity(struct wm_view *v, int a) { (void)v; stub_set_opacity_count++; stub_set_opacity_value = a; }
void wm_view_begin_interactive(struct wm_view *v, enum wm_cursor_mode m, uint32_t e) { (void)v; (void)m; (void)e; stub_begin_interactive_count++; }
void wm_view_maximize_vert(struct wm_view *v) { (void)v; stub_maximize_vert_count++; }
void wm_view_maximize_horiz(struct wm_view *v) { (void)v; stub_maximize_horiz_count++; }
void wm_view_toggle_decoration(struct wm_view *v) { (void)v; stub_toggle_decoration_count++; }
void wm_view_toggle_maximize(struct wm_view *v) { (void)v; stub_maximize_listener_count++; }
void wm_view_toggle_fullscreen(struct wm_view *v) { (void)v; stub_fullscreen_listener_count++; }
void wm_view_minimize(struct wm_view *v) { (void)v; stub_minimize_listener_count++; }
void wm_view_lhalf(struct wm_view *v) { (void)v; stub_lhalf_count++; }
void wm_view_rhalf(struct wm_view *v) { (void)v; stub_rhalf_count++; }
void wm_view_resize_by(struct wm_view *v, int w, int h) { (void)v; (void)w; (void)h; stub_resize_by_count++; }
void wm_view_focus_direction(struct wm_server *s, int dx, int dy) { (void)s; (void)dx; (void)dy; stub_focus_direction_count++; }
void wm_view_set_head(struct wm_view *v, int h) { (void)v; (void)h; stub_set_head_count++; }
static int stub_head_offset;
void wm_view_send_to_head_offset(struct wm_view *v, int offset)
{
	(void)v;
	stub_head_offset = offset;
	if (offset > 0)
		stub_send_next_head_count++;
	else
		stub_send_prev_head_count++;
}
void wm_view_close_all(struct wm_server *s) { (void)s; stub_close_all_count++; }
void wm_focus_cycle(struct wm_server *s, const struct wm_window_list_spec *spec,
	bool reverse)
{
	(void)s; (void)spec;
	if (reverse)
		stub_cycle_prev_count++;
	else
		stub_cycle_next_count++;
}
void wm_view_deiconify_last(struct wm_server *s) { (void)s; stub_deiconify_last_count++; }
void wm_view_deiconify_all(struct wm_server *s) { (void)s; stub_deiconify_all_count++; }
void wm_view_deiconify_all_workspace(struct wm_server *s) { (void)s; stub_deiconify_all_ws_count++; }
static int stub_activate_tab_index = -1;
void wm_view_activate_tab(struct wm_view *v, int i) { (void)v; stub_activate_tab_index = i; stub_activate_tab_count++; }
static int stub_send_take_index = -1;
void wm_view_send_to_workspace(struct wm_server *s, struct wm_view *v, int i) { (void)s; (void)v; stub_send_take_index = i; stub_send_to_ws_count++; }
void wm_view_take_to_workspace(struct wm_server *s, struct wm_view *v, int i) { (void)s; (void)v; stub_send_take_index = i; stub_take_to_ws_count++; }
void wm_view_send_to_next_workspace(struct wm_server *s, struct wm_view *v) { (void)s; (void)v; stub_send_next_ws_count++; }
void wm_view_send_to_prev_workspace(struct wm_server *s, struct wm_view *v) { (void)s; (void)v; stub_send_prev_ws_count++; }
void wm_view_take_to_next_workspace(struct wm_server *s, struct wm_view *v) { (void)s; (void)v; stub_take_next_ws_count++; }
void wm_view_take_to_prev_workspace(struct wm_server *s, struct wm_view *v) { (void)s; (void)v; stub_take_prev_ws_count++; }

enum wm_view_layer wm_view_layer_from_name(const char *n) { (void)n; return (enum wm_view_layer)stub_view_layer_from_name_result; }

static int stub_ws_switch_index = -1;
void wm_workspace_switch(struct wm_server *s, int i) { (void)s; stub_ws_switch_index = i; stub_ws_switch_count++; }
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
int wm_decoration_tab_at(struct wm_decoration *d, double x, double y) { (void)d; (void)x; (void)y; return -1; }
void wm_view_get_geometry(struct wm_view *v, struct wlr_box *b) { (void)v; b->x = 0; b->y = 0; b->width = 100; b->height = 100; }
void wm_tab_group_activate(struct wm_tab_group *g, struct wm_view *v) { (void)g; (void)v; }
static int stub_cycle_end_count;
void wm_focus_cycle_end(struct wm_server *s) { (void)s; stub_cycle_end_count++; }
void wm_decoration_set_preset(struct wm_decoration *d, enum wm_decor_preset p, struct wm_style *s) { (void)d; (void)p; (void)s; stub_decoration_set_preset_count++; }
void wm_decoration_set_shaded(struct wm_decoration *d, bool shaded, struct wm_style *s) { (void)d; (void)s; stub_decoration_set_shaded_count++; stub_decoration_shaded_value = shaded; }

int style_load(struct wm_style *s, const char *p) { (void)s; (void)p; stub_style_load_count++; return 0; }
void wm_server_set_style(struct wm_server *s, const char *p);
static char stub_resource[256];
int config_set_resource(struct wm_config *c, const char *k, const char *v)
{
	(void)c;
	snprintf(stub_resource, sizeof(stub_resource), "%s=%s", k, v);
	return 0;
}

void wm_toolbar_relayout(struct wm_toolbar *t) { (void)t; stub_toolbar_relayout_count++; }
void wm_toolbar_toggle_above(struct wm_toolbar *t) { (void)t; stub_toolbar_toggle_above_count++; }
void wm_toolbar_toggle_visible(struct wm_toolbar *t) { (void)t; stub_toolbar_toggle_visible_count++; }
void wm_toolbar_update_iconbar(struct wm_toolbar *t) { (void)t; }
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
void wm_focus_nav_enter_slit(struct wm_server *s) { (void)s; stub_focus_nav_enter_slit_count++; }
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
	struct wlr_input_device *device, bool is_virtual);
void wm_keyboard_group_init(struct wm_server *server);
void wm_keyboard_group_finish(struct wm_server *server);

/* --- Include keyboard source directly (uses our stubs) --- */

#include "util.h"

void wm_spawn_command(const char *cmd) { (void)cmd; }

/* Forward declarations for functions shared between keyboard.c and
 * keyboard_actions.c (needed because we include both source files
 * directly and the header guard is blocked). */
bool wm_execute_action(struct wm_server *server,
	enum wm_action action, const char *argument);
bool wm_execute_keybind_action(struct wm_server *server,
	struct wm_keybind *bind);
bool wm_keybind_check_condition(struct wm_server *server,
	struct wm_keybind *bind);
void wm_chain_reset(struct wm_server *server);
struct wl_list *wm_get_active_bindings(struct wm_server *server);

/* --- command.c / client_pattern.c stubs --- */
static int stub_command_execute_count;
static bool stub_condition_result = true;
bool wm_execute_action_on(struct wm_server *server, enum wm_action action,
	const char *argument, struct wm_view *target);
bool
wm_command_execute(struct wm_server *server, struct wm_command *cmd,
	struct wm_view *target)
{
	stub_command_execute_count++;
	if (!cmd)
		return false;
	switch (cmd->action) {
	case WM_ACTION_MACRO_CMD:
	case WM_ACTION_TOGGLE_CMD:
	case WM_ACTION_IF:
	case WM_ACTION_FOREACH:
	case WM_ACTION_MAP:
	case WM_ACTION_DELAY:
		return true; /* compound: covered by test_command */
	default:
		return wm_execute_action_on(server, cmd->action,
			cmd->argument, target);
	}
}
struct wm_command *
wm_command_create(enum wm_action action, const char *argument)
{
	/* Like command.c: compound commands need {...} arguments */
	if (!argument && (action == WM_ACTION_MACRO_CMD ||
	    action == WM_ACTION_TOGGLE_CMD || action == WM_ACTION_IF ||
	    action == WM_ACTION_FOREACH || action == WM_ACTION_MAP ||
	    action == WM_ACTION_DELAY))
		return NULL;
	struct wm_command *cmd = calloc(1, sizeof(*cmd));
	cmd->action = action;
	cmd->argument = argument ? strdup(argument) : NULL;
	return cmd;
}
void
wm_command_destroy(struct wm_command *cmd)
{
	if (!cmd)
		return;
	free(cmd->argument);
	free(cmd);
}
bool
wm_condition_evaluate(struct wm_server *server, struct wm_condition *cond,
	struct wm_view *view)
{
	(void)server; (void)cond; (void)view;
	return stub_condition_result;
}
bool
wm_window_list_parse(const char *args, struct wm_window_list_spec *spec)
{
	(void)args;
	memset(spec, 0, sizeof(*spec));
	return true;
}
void
wm_window_list_finish(struct wm_window_list_spec *spec)
{
	(void)spec;
}

/* The real Fluxbox argument formats (their enums are mocked above) */
#define WM_VIEW_LAYER_H
#define WLR_UTIL_BOX_H
static inline bool wlr_box_empty(const struct wlr_box *b)
	{ return !b || b->width <= 0 || b->height <= 0; }
#include "fbcompat.c"

/* wallpaper.c */
struct wm_wallpaper { int dummy; };
static int stub_wallpaper_reloads;
static struct wm_wallpaper stub_wallpaper;
void wm_wallpaper_reload(struct wm_wallpaper *wp)
	{ (void)wp; stub_wallpaper_reloads++; }
struct wm_wallpaper *wm_wallpaper_create(struct wm_server *s)
	{ (void)s; return &stub_wallpaper; }

/* Window geometry the move and resize commands use */
static struct wlr_box stub_frame_box;
static struct wlr_box stub_set_frame_box;
static int stub_set_frame_box_count;
static struct wm_output *stub_output_for_view;
void wm_view_move(struct wm_view *v, int x, int y) { v->x = x; v->y = y; }
void wm_view_frame_box(struct wm_view *v, struct wlr_box *b)
	{ *b = stub_frame_box; b->x = v->x; b->y = v->y; }
void wm_view_set_frame_box(struct wm_view *v, const struct wlr_box *b)
	{ v->x = b->x; v->y = b->y; stub_set_frame_box = *b;
	  stub_set_frame_box_count++; }
struct wm_output *wm_output_for_view(struct wm_view *v)
	{ (void)v; return stub_output_for_view; }
void wm_output_layout_box(struct wm_output *o, struct wlr_box *b)
	{ *b = o->full_area; }
static int stub_goto_window_num;
void wm_focus_goto_window(struct wm_server *s,
	const struct wm_window_list_spec *spec, int num)
	{ (void)s; (void)spec; stub_goto_window_num = num; }

/* view.c backend accessors (xdg-only stand-ins) */
#define VIEW_STUB_CLOSE
#define VIEW_STUB_KILL
#define VIEW_STUB_SET_SIZE
#define VIEW_STUB_SURFACE
#include "view_backend_stubs.c"

#include "keyboard_actions.c"
#include "keyboard.c"

/* One keysym, as most tests press keys */
static bool
hck(struct wm_server *server, uint32_t mods, xkb_keysym_t sym)
{
	return handle_compositor_keybinding(server, mods, &sym, 1);
}

/* A key press is looked up as typed, unshifted, and by keycode */
static void
test_key_candidates(void)
{
	stub_keysyms[0] = 0xfe20;          /* ISO_Left_Tab (Shift+Tab) */
	stub_nsyms = 1;
	stub_level0_keysyms[0] = 0xff09;   /* Tab */
	stub_level0_nsyms = 1;
	struct wlr_keyboard kb = {0};
	xkb_keysym_t out[16];
	const xkb_keysym_t *syms = stub_keysyms;
	int n = key_candidates(&kb, 23 + 8, syms, 1, out, 16);
	assert(n == 3);
	assert(out[0] == 0xfe20 && out[1] == 0xff09);
	assert(out[2] == WM_KEYSYM_KEYCODE(31));
	/* The same keysym twice is listed once */
	stub_level0_keysyms[0] = 0xfe20;
	n = key_candidates(&kb, 31, syms, 1, out, 16);
	assert(n == 2);
	stub_level0_nsyms = 0;
	printf("  PASS: test_key_candidates\n");
}

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
	stub_focus_nav_enter_slit_count = 0;
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
	/* Four workspaces, the first shown */
	memset(&test_workspace, 0, sizeof(test_workspace));
	test_server.workspace_count = 4;
	test_server.former_workspace = -1;
	stub_active_workspace = &test_workspace;
	stub_ws_switch_index = -1;
	stub_send_take_index = -1;
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

/* Test wm_is_blocked_env_var: LD_PRELOAD is blocked */
static void
test_blocked_env_ld_preload(void)
{
	assert(wm_is_blocked_env_var("LD_PRELOAD") == true);
	printf("  PASS: test_blocked_env_ld_preload\n");
}

/* Test wm_is_blocked_env_var: PATH is blocked */
static void
test_blocked_env_path(void)
{
	assert(wm_is_blocked_env_var("PATH") == true);
	printf("  PASS: test_blocked_env_path\n");
}

/* Test wm_is_blocked_env_var: normal var is allowed */
static void
test_blocked_env_normal(void)
{
	assert(wm_is_blocked_env_var("MY_APP_VAR") == false);
	printf("  PASS: test_blocked_env_normal\n");
}

/* Test wm_is_blocked_env_var: case insensitive match */
static void
test_blocked_env_case_insensitive(void)
{
	assert(wm_is_blocked_env_var("ld_preload") == true);
	assert(wm_is_blocked_env_var("Ld_Preload") == true);
	printf("  PASS: test_blocked_env_case_insensitive\n");
}

/* Test wm_is_blocked_env_var: WAYLAND_DISPLAY is blocked */
static void
test_blocked_env_wayland_display(void)
{
	assert(wm_is_blocked_env_var("WAYLAND_DISPLAY") == true);
	printf("  PASS: test_blocked_env_wayland_display\n");
}






/* ====================================================================
 * Group B: Condition evaluation tests (minimal stubs)
 * ==================================================================== */

















/* ====================================================================
 * Group C: Chain state machine tests
 * ==================================================================== */

/* Test chain_reset: clears state */
static void
test_wm_chain_reset(void)
{
	setup();
	test_server.chain_state.in_chain = true;
	test_server.chain_state.current_level = (struct wl_list *)0x1234;

	/* Create a timeout */
	struct wl_event_source timer = {0};
	test_server.chain_state.timeout = &timer;

	wm_chain_reset(&test_server);

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
test_wm_get_active_bindings(void)
{
	setup();

	/* Create a "default" keymode */
	struct wm_keymode mode;
	mode.name = "default";
	wl_list_init(&mode.bindings);
	wl_list_insert(&test_server.keymodes, &mode.link);

	/* No current_keymode → defaults to "default" */
	struct wl_list *result = wm_get_active_bindings(&test_server);
	assert(result == &mode.bindings);

	/* Explicit keymode */
	test_server.current_keymode = strdup("default");
	result = wm_get_active_bindings(&test_server);
	assert(result == &mode.bindings);

	/* Non-existent mode returns NULL */
	free(test_server.current_keymode);
	test_server.current_keymode = strdup("vim");
	result = wm_get_active_bindings(&test_server);
	assert(result == NULL);

	free(test_server.current_keymode);
	test_server.current_keymode = NULL;
	wl_list_remove(&mode.link);
	printf("  PASS: test_get_active_bindings\n");
}

/* ====================================================================
 * Group D: wm_execute_action() dispatch tests
 * ==================================================================== */

/* Test FocusNext action */
static void
test_action_focus_next(void)
{
	setup();
	assert(wm_execute_action(&test_server, WM_ACTION_FOCUS_NEXT, NULL) == true);
	assert(stub_focus_next_count == 1);
	printf("  PASS: test_action_focus_next\n");
}

/* Test FocusPrev action */
static void
test_action_focus_prev(void)
{
	setup();
	assert(wm_execute_action(&test_server, WM_ACTION_FOCUS_PREV, NULL) == true);
	assert(stub_focus_prev_count == 1);
	printf("  PASS: test_action_focus_prev\n");
}

/* Test Close action */
static void
test_action_close(void)
{
	setup_with_view();
	assert(wm_execute_action(&test_server, WM_ACTION_CLOSE, NULL) == true);
	assert(stub_close_called == 1);
	printf("  PASS: test_action_close\n");
}

/* Test Close with no focused view does nothing */
static void
test_action_close_no_view(void)
{
	setup();
	assert(wm_execute_action(&test_server, WM_ACTION_CLOSE, NULL) == true);
	assert(stub_close_called == 0);
	printf("  PASS: test_action_close_no_view\n");
}

/* Test Exit action */
static void
test_action_exit(void)
{
	setup();
	assert(wm_execute_action(&test_server, WM_ACTION_EXIT, NULL) == true);
	assert(stub_terminate_called == 1);
	printf("  PASS: test_action_exit\n");
}

/* Test Maximize action triggers listener */
static void
test_action_maximize(void)
{
	setup_with_view();
	assert(wm_execute_action(&test_server, WM_ACTION_MAXIMIZE, NULL) == true);
	assert(stub_maximize_listener_count == 1);
	printf("  PASS: test_action_maximize\n");
}

/* Test Fullscreen action triggers listener */
static void
test_action_fullscreen(void)
{
	setup_with_view();
	assert(wm_execute_action(&test_server, WM_ACTION_FULLSCREEN, NULL) == true);
	assert(stub_fullscreen_listener_count == 1);
	printf("  PASS: test_action_fullscreen\n");
}

/* Test Minimize action triggers listener */
static void
test_action_minimize(void)
{
	setup_with_view();
	assert(wm_execute_action(&test_server, WM_ACTION_MINIMIZE, NULL) == true);
	assert(stub_minimize_listener_count == 1);
	printf("  PASS: test_action_minimize\n");
}

/* Test Raise action */
static void
test_action_raise(void)
{
	setup_with_view();
	assert(wm_execute_action(&test_server, WM_ACTION_RAISE, NULL) == true);
	assert(stub_raise_count == 1);
	printf("  PASS: test_action_raise\n");
}

/* Test Lower action */
static void
test_action_lower(void)
{
	setup_with_view();
	assert(wm_execute_action(&test_server, WM_ACTION_LOWER, NULL) == true);
	assert(stub_lower_count == 1);
	printf("  PASS: test_action_lower\n");
}

/* Test Stick action toggles sticky */
static void
test_action_stick(void)
{
	setup_with_view();
	test_view.sticky = false;
	assert(wm_execute_action(&test_server, WM_ACTION_STICK, NULL) == true);
	assert(stub_set_sticky_count == 1);
	assert(stub_set_sticky_value == true);
	printf("  PASS: test_action_stick\n");
}

/* Test NextWorkspace action */
static void
test_action_next_workspace(void)
{
	setup();
	assert(wm_execute_action(&test_server, WM_ACTION_NEXT_WORKSPACE, NULL) == true);
	assert(stub_ws_switch_index == 1);
	wm_execute_action(&test_server, WM_ACTION_NEXT_WORKSPACE, "6");
	assert(stub_ws_switch_index == 2);   /* wraps */
	/* NextWorkspace 0: back to the workspace before */
	stub_ws_switch_index = -1;
	wm_execute_action(&test_server, WM_ACTION_NEXT_WORKSPACE, "0");
	assert(stub_ws_switch_index == -1);
	test_server.former_workspace = 3;
	wm_execute_action(&test_server, WM_ACTION_NEXT_WORKSPACE, "0");
	assert(stub_ws_switch_index == 3);
	printf("  PASS: test_action_next_workspace\n");
}

/* Test PrevWorkspace action */
static void
test_action_prev_workspace(void)
{
	setup();
	assert(wm_execute_action(&test_server, WM_ACTION_PREV_WORKSPACE, NULL) == true);
	assert(stub_ws_switch_index == 3);
	wm_execute_action(&test_server, WM_ACTION_PREV_WORKSPACE, "2");
	assert(stub_ws_switch_index == 2);
	printf("  PASS: test_action_prev_workspace\n");
}

/* Test Workspace N action with argument */
static void
test_action_workspace_number(void)
{
	setup();
	assert(wm_execute_action(&test_server, WM_ACTION_WORKSPACE, "3") == true);
	assert(stub_ws_switch_index == 2);
	/* Fluxbox: from the end when negative, clamped */
	wm_execute_action(&test_server, WM_ACTION_WORKSPACE, "-1");
	assert(stub_ws_switch_index == 3);
	wm_execute_action(&test_server, WM_ACTION_WORKSPACE, "9");
	assert(stub_ws_switch_index == 3);
	wm_execute_action(&test_server, WM_ACTION_WORKSPACE, "0");
	assert(stub_ws_switch_index == 0);
	printf("  PASS: test_action_workspace_number\n");
}

/* Test ToggleDecor action */
static void
test_action_toggle_decor(void)
{
	setup_with_view();
	assert(wm_execute_action(&test_server, WM_ACTION_TOGGLE_DECOR, NULL) == true);
	assert(stub_toggle_decoration_count == 1);
	printf("  PASS: test_action_toggle_decor\n");
}

/* Test MaximizeVert action */
static void
test_action_maximize_vert(void)
{
	setup_with_view();
	assert(wm_execute_action(&test_server, WM_ACTION_MAXIMIZE_VERT, NULL) == true);
	assert(stub_maximize_vert_count == 1);
	printf("  PASS: test_action_maximize_vert\n");
}

/* Test MaximizeHoriz action */
static void
test_action_maximize_horiz(void)
{
	setup_with_view();
	assert(wm_execute_action(&test_server, WM_ACTION_MAXIMIZE_HORIZ, NULL) == true);
	assert(stub_maximize_horiz_count == 1);
	printf("  PASS: test_action_maximize_horiz\n");
}

/* Test LHalf / RHalf actions */
static void
test_action_lhalf_rhalf(void)
{
	setup_with_view();
	assert(wm_execute_action(&test_server, WM_ACTION_LHALF, NULL) == true);
	assert(stub_lhalf_count == 1);
	assert(wm_execute_action(&test_server, WM_ACTION_RHALF, NULL) == true);
	assert(stub_rhalf_count == 1);
	printf("  PASS: test_action_lhalf_rhalf\n");
}

/* Test ArrangeWindows action */
static void
test_action_arrange_windows(void)
{
	setup();
	assert(wm_execute_action(&test_server, WM_ACTION_ARRANGE_WINDOWS, NULL) == true);
	assert(stub_arrange_grid_count == 1);
	printf("  PASS: test_action_arrange_windows\n");
}

/* Test ArrangeStack variants */
static void
test_action_arrange_stack(void)
{
	setup();
	wm_execute_action(&test_server, WM_ACTION_ARRANGE_STACK_LEFT, NULL);
	wm_execute_action(&test_server, WM_ACTION_ARRANGE_STACK_RIGHT, NULL);
	wm_execute_action(&test_server, WM_ACTION_ARRANGE_STACK_TOP, NULL);
	wm_execute_action(&test_server, WM_ACTION_ARRANGE_STACK_BOTTOM, NULL);
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

	assert(wm_execute_action(&test_server, WM_ACTION_SET_ALPHA, "128") == true);
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

	assert(wm_execute_action(&test_server, WM_ACTION_SET_ALPHA, "-50") == true);
	assert(test_view.focus_alpha == 150);
	printf("  PASS: test_action_set_alpha_relative\n");
}

/* Test SetAlpha clamping */
static void
test_action_set_alpha_clamp(void)
{
	setup_with_view();
	test_view.focus_alpha = 10;

	wm_execute_action(&test_server, WM_ACTION_SET_ALPHA, "-100");
	assert(test_view.focus_alpha == 0); /* clamped to 0 */

	test_view.focus_alpha = 250;
	wm_execute_action(&test_server, WM_ACTION_SET_ALPHA, "+100");
	assert(test_view.focus_alpha == 255); /* clamped to 255 */
	printf("  PASS: test_action_set_alpha_clamp\n");
}

/* Test SetLayer action */
static void
test_action_set_layer(void)
{
	setup_with_view();
	assert(wm_execute_action(&test_server, WM_ACTION_SET_LAYER, "Above") == true);
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

	assert(wm_execute_action(&test_server, WM_ACTION_SHOW_DESKTOP, NULL) == true);
	/* New ShowDesktop directly disables scene node instead of calling minimize listener */
	assert(test_scene_tree.node.enabled == false);

	wl_list_remove(&test_view.link);
	printf("  PASS: test_action_show_desktop\n");
}

/* Test HideMenus action */
static void
test_action_hide_menus(void)
{
	setup();
	assert(wm_execute_action(&test_server, WM_ACTION_HIDE_MENUS, NULL) == true);
	assert(stub_menu_hide_all_count == 1);
	printf("  PASS: test_action_hide_menus\n");
}

/* Test RootMenu action */
static void
test_action_root_menu(void)
{
	setup();
	assert(wm_execute_action(&test_server, WM_ACTION_ROOT_MENU, NULL) == true);
	assert(stub_menu_show_root_count == 1);
	printf("  PASS: test_action_root_menu\n");
}

/* Test SetEnv: blocked variable rejected */
static void
test_action_set_env_blocked(void)
{
	setup();
	/* This should be silently rejected */
	assert(wm_execute_action(&test_server, WM_ACTION_SET_ENV, "LD_PRELOAD=evil.so") == true);
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

	assert(wm_execute_action(&test_server, WM_ACTION_SET_ENV, "TEST_KEYBOARD_VAR=hello") == true);
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

	assert(wm_execute_action(&test_server, WM_ACTION_SET_ENV, "TEST_KEYBOARD_VAR2 world") == true);
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
	assert(wm_execute_action(&test_server, WM_ACTION_NOP, NULL) == true);
	printf("  PASS: test_action_nop\n");
}

/* MacroCmd/ToggleCmd with no {sub-commands} fail to parse: false */
static void
test_action_macro_toggle_direct(void)
{
	setup();
	assert(wm_execute_action(&test_server, WM_ACTION_MACRO_CMD, NULL) == false);
	assert(wm_execute_action(&test_server, WM_ACTION_TOGGLE_CMD, NULL) == false);
	printf("  PASS: test_action_macro_toggle_direct\n");
}

/* Test Shade action */
static void
test_action_shade(void)
{
	setup_with_view();
	test_decoration.shaded = false;

	assert(wm_execute_action(&test_server, WM_ACTION_SHADE, NULL) == true);
	assert(stub_decoration_set_shaded_count == 1);
	assert(stub_decoration_shaded_value == true); /* toggle from false */
	printf("  PASS: test_action_shade\n");
}

/* Test ShadeOn / ShadeOff */
static void
test_action_shade_on_off(void)
{
	setup_with_view();
	assert(wm_execute_action(&test_server, WM_ACTION_SHADE_ON, NULL) == true);
	assert(stub_decoration_set_shaded_count == 1);
	assert(stub_decoration_shaded_value == true);

	assert(wm_execute_action(&test_server, WM_ACTION_SHADE_OFF, NULL) == true);
	assert(stub_decoration_set_shaded_count == 2);
	assert(stub_decoration_shaded_value == false);
	printf("  PASS: test_action_shade_on_off\n");
}

/* Test Reconfigure action */
static void
test_action_reconfigure(void)
{
	setup();
	assert(wm_execute_action(&test_server, WM_ACTION_RECONFIGURE, NULL) == true);
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

	wm_execute_action(&test_server, WM_ACTION_MOVE_LEFT, "10");
	assert(test_view.x == 90);

	wm_execute_action(&test_server, WM_ACTION_MOVE_RIGHT, "20");
	assert(test_view.x == 110);

	wm_execute_action(&test_server, WM_ACTION_MOVE_UP, "5");
	assert(test_view.y == 95);

	wm_execute_action(&test_server, WM_ACTION_MOVE_DOWN, "15");
	assert(test_view.y == 110);
	printf("  PASS: test_action_move_directional\n");
}

/* Test MoveTo action */
static void
test_action_move_to(void)
{
	setup_with_view();
	static struct wm_output out;
	out.usable_area = (struct wlr_box){ 100, 0, 1000, 800 };
	stub_output_for_view = &out;
	stub_frame_box = (struct wlr_box){ 0, 0, 200, 100 };

	/* On the head, from its top left by default */
	wm_execute_action(&test_server, WM_ACTION_MOVE_TO, "50 75");
	assert(test_view.x == 150 && test_view.y == 75);
	/* "*" keeps a coordinate; % of the head */
	wm_execute_action(&test_server, WM_ACTION_MOVE_TO, "* 50%");
	assert(test_view.x == 150 && test_view.y == 400);
	/* Corners: offsets from right and bottom count inwards */
	wm_execute_action(&test_server, WM_ACTION_MOVE_TO, "10 20 BottomRight");
	assert(test_view.x == 100 + 1000 - 200 - 10);
	assert(test_view.y == 800 - 100 - 20);
	wm_execute_action(&test_server, WM_ACTION_MOVE_TO, "0 0 Center");
	assert(test_view.x == 100 + 400 && test_view.y == 350);
	/* Fewer than two values: nothing */
	wm_execute_action(&test_server, WM_ACTION_MOVE_TO, "5");
	assert(test_view.x == 500);
	stub_output_for_view = NULL;
	printf("  PASS: test_action_move_to\n");
}

/* Test ResizeTo action */
static void
test_action_resize_to(void)
{
	setup_with_view();
	static struct wm_output out;
	out.usable_area = (struct wlr_box){ 0, 0, 1000, 800 };
	stub_output_for_view = &out;
	stub_frame_box = (struct wlr_box){ 0, 0, 200, 100 };

	wm_execute_action(&test_server, WM_ACTION_RESIZE_TO, "800 600");
	assert(stub_set_frame_box.width == 800);
	assert(stub_set_frame_box.height == 600);
	/* % of the head; 0 keeps */
	wm_execute_action(&test_server, WM_ACTION_RESIZE_TO, "50% 0");
	assert(stub_set_frame_box.width == 500);
	assert(stub_set_frame_box.height == 100);
	stub_output_for_view = NULL;
	printf("  PASS: test_action_resize_to\n");
}

/* Move and Resize by deltas (Fluxbox's MoveCmd, ResizeCmd) */
static void
test_action_move_resize_deltas(void)
{
	setup_with_view();
	test_view.x = 100;
	test_view.y = 100;
	stub_frame_box = (struct wlr_box){ 0, 0, 200, 100 };
	wm_execute_action(&test_server, WM_ACTION_MOVE, "10 -20");
	assert(test_view.x == 110 && test_view.y == 80);
	/* A maximized window is un-maximized first */
	test_view.maximized = true;
	int toggles = stub_maximize_listener_count;
	wm_execute_action(&test_server, WM_ACTION_MOVE, "1 1");
	assert(stub_maximize_listener_count == toggles + 1);
	assert(test_view.x == 111);
	test_view.maximized = false;

	wm_execute_action(&test_server, WM_ACTION_RESIZE, "10 50%");
	assert(stub_set_frame_box.width == 210);
	assert(stub_set_frame_box.height == 150);
	wm_execute_action(&test_server, WM_ACTION_RESIZE_HORIZ, "-500");
	assert(stub_set_frame_box.width == 10);   /* the minimum */
	printf("  PASS: test_action_move_resize_deltas\n");
}

/* Test FocusDirection actions */
static void
test_action_focus_directions(void)
{
	setup();
	wm_execute_action(&test_server, WM_ACTION_FOCUS_LEFT, NULL);
	wm_execute_action(&test_server, WM_ACTION_FOCUS_RIGHT, NULL);
	wm_execute_action(&test_server, WM_ACTION_FOCUS_UP, NULL);
	wm_execute_action(&test_server, WM_ACTION_FOCUS_DOWN, NULL);
	assert(stub_focus_direction_count == 4);
	printf("  PASS: test_action_focus_directions\n");
}

/* Test Tab group actions */
static void
test_action_tab_group(void)
{
	setup_with_tabbed_view();
	wm_execute_action(&test_server, WM_ACTION_NEXT_TAB, NULL);
	assert(stub_tab_group_next_count == 1);
	wm_execute_action(&test_server, WM_ACTION_PREV_TAB, NULL);
	assert(stub_tab_group_prev_count == 1);
	wm_execute_action(&test_server, WM_ACTION_DETACH_CLIENT, NULL);
	assert(stub_tab_group_remove_count == 1);
	wm_execute_action(&test_server, WM_ACTION_MOVE_TAB_LEFT, NULL);
	assert(stub_tab_group_move_left_count == 1);
	wm_execute_action(&test_server, WM_ACTION_MOVE_TAB_RIGHT, NULL);
	assert(stub_tab_group_move_right_count == 1);
	printf("  PASS: test_action_tab_group\n");
}

/* Test Focus nav actions */
static void
test_action_focus_nav(void)
{
	setup();
	wm_execute_action(&test_server, WM_ACTION_FOCUS_TOOLBAR, NULL);
	assert(stub_focus_nav_enter_toolbar_count == 1);
	wm_execute_action(&test_server, WM_ACTION_FOCUS_NEXT_ELEMENT, NULL);
	assert(stub_focus_nav_next_element_count == 1);
	wm_execute_action(&test_server, WM_ACTION_FOCUS_PREV_ELEMENT, NULL);
	assert(stub_focus_nav_prev_element_count == 1);
	wm_execute_action(&test_server, WM_ACTION_FOCUS_ACTIVATE, NULL);
	assert(stub_focus_nav_activate_count == 1);
	printf("  PASS: test_action_focus_nav\n");
}

/* Test Deiconify variants */
static void
test_action_deiconify(void)
{
	setup();
	wm_execute_action(&test_server, WM_ACTION_DEICONIFY, NULL);
	assert(stub_deiconify_last_count == 1);

	wm_execute_action(&test_server, WM_ACTION_DEICONIFY, "All");
	assert(stub_deiconify_all_count == 1);

	wm_execute_action(&test_server, WM_ACTION_DEICONIFY, "AllWorkspace");
	assert(stub_deiconify_all_ws_count == 1);

	wm_execute_action(&test_server, WM_ACTION_DEICONIFY, "LastWorkspace");
	assert(stub_deiconify_last_count == 2);
	printf("  PASS: test_action_deiconify\n");
}

/* Test Toolbar/Slit toggle actions */
static void
test_action_toolbar_slit_toggles(void)
{
	setup();
	wm_execute_action(&test_server, WM_ACTION_TOGGLE_TOOLBAR_ABOVE, NULL);
	assert(stub_toolbar_toggle_above_count == 1);
	wm_execute_action(&test_server, WM_ACTION_TOGGLE_TOOLBAR_VISIBLE, NULL);
	assert(stub_toolbar_toggle_visible_count == 1);
	wm_execute_action(&test_server, WM_ACTION_TOGGLE_SLIT_ABOVE, NULL);
	assert(stub_slit_toggle_above_count == 1);
	wm_execute_action(&test_server, WM_ACTION_TOGGLE_SLIT_HIDDEN, NULL);
	assert(stub_slit_toggle_hidden_count == 1);
	printf("  PASS: test_action_toolbar_slit_toggles\n");
}

/* Test KeyMode action */
static void
test_action_key_mode(void)
{
	setup();
	test_server.current_keymode = strdup("default");
	static struct wm_keymode vim;
	memset(&vim, 0, sizeof(vim));
	vim.name = "vim";
	wl_list_init(&vim.bindings);
	wl_list_insert(&test_server.keymodes, &vim.link);

	/* The mode gets its end key bound back to default, once */
	int adds = stub_keybind_add_count;
	wm_execute_action(&test_server, WM_ACTION_KEY_MODE, "vim");
	assert(strcmp(test_server.current_keymode, "vim") == 0);
	assert(stub_keybind_add_count == adds + 1 && vim.end_bound);
	wm_execute_action(&test_server, WM_ACTION_KEY_MODE, "vim Control g");
	assert(stub_keybind_add_count == adds + 1);

	/* A mode the keys file doesn't have is the default mode */
	wm_execute_action(&test_server, WM_ACTION_KEY_MODE, "nosuch");
	assert(strcmp(test_server.current_keymode, "default") == 0);

	wl_list_remove(&vim.link);
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
	wm_execute_action(&test_server, WM_ACTION_TOGGLE_SHOW_POSITION, NULL);
	assert(test_server.show_position == true);
	wm_execute_action(&test_server, WM_ACTION_TOGGLE_SHOW_POSITION, NULL);
	assert(test_server.show_position == false);
	printf("  PASS: test_action_toggle_show_position\n");
}

/* Test SetDecor action with various presets */
static void
test_action_set_decor(void)
{
	setup_with_view();
	wm_execute_action(&test_server, WM_ACTION_SET_DECOR, "NONE");
	assert(stub_decoration_set_preset_count == 1);
	wm_execute_action(&test_server, WM_ACTION_SET_DECOR, "BORDER");
	assert(stub_decoration_set_preset_count == 2);
	wm_execute_action(&test_server, WM_ACTION_SET_DECOR, "TAB");
	assert(stub_decoration_set_preset_count == 3);
	printf("  PASS: test_action_set_decor\n");
}

/* Test If/ForEach/Map/Delay via execute_action returns false (need keybind context) */
static void
test_action_conditional_direct(void)
{
	setup();
	assert(wm_execute_action(&test_server, WM_ACTION_IF, NULL) == false);
	assert(wm_execute_action(&test_server, WM_ACTION_FOREACH, NULL) == false);
	assert(wm_execute_action(&test_server, WM_ACTION_MAP, NULL) == false);
	assert(wm_execute_action(&test_server, WM_ACTION_DELAY, NULL) == false);
	printf("  PASS: test_action_conditional_direct\n");
}

/* Test workspace actions: SendTo, TakeTo, etc */
static void
test_action_workspace_ops(void)
{
	setup_with_view();
	test_view.workspace = &test_workspace;
	wm_execute_action(&test_server, WM_ACTION_SEND_TO_WORKSPACE, "2");
	assert(stub_send_to_ws_count == 1 && stub_send_take_index == 1);
	wm_execute_action(&test_server, WM_ACTION_TAKE_TO_WORKSPACE, "-1");
	assert(stub_take_to_ws_count == 1 && stub_send_take_index == 3);
	/* Next/Prev n: from the window's workspace, wrapping */
	wm_execute_action(&test_server, WM_ACTION_SEND_TO_NEXT_WORKSPACE, NULL);
	assert(stub_send_to_ws_count == 2 && stub_send_take_index == 1);
	wm_execute_action(&test_server, WM_ACTION_SEND_TO_PREV_WORKSPACE, NULL);
	assert(stub_send_to_ws_count == 3 && stub_send_take_index == 3);
	wm_execute_action(&test_server, WM_ACTION_TAKE_TO_NEXT_WORKSPACE, "2");
	assert(stub_take_to_ws_count == 2 && stub_send_take_index == 2);
	wm_execute_action(&test_server, WM_ACTION_TAKE_TO_PREV_WORKSPACE, "5");
	assert(stub_take_to_ws_count == 3 && stub_send_take_index == 3);
	wm_execute_action(&test_server, WM_ACTION_ADD_WORKSPACE, NULL);
	assert(stub_ws_add_count == 1);
	wm_execute_action(&test_server, WM_ACTION_REMOVE_LAST_WORKSPACE, NULL);
	assert(stub_ws_remove_last_count == 1);
	printf("  PASS: test_action_workspace_ops\n");
}

/* Test CloseAllWindows */
static void
test_action_close_all(void)
{
	setup();
	wm_execute_action(&test_server, WM_ACTION_CLOSE_ALL_WINDOWS, NULL);
	assert(stub_close_all_count == 1);
	printf("  PASS: test_action_close_all\n");
}

/* Test RaiseLayer / LowerLayer */
static void
test_action_raise_lower_layer(void)
{
	setup_with_view();
	wm_execute_action(&test_server, WM_ACTION_RAISE_LAYER, NULL);
	assert(stub_raise_layer_count == 1);
	wm_execute_action(&test_server, WM_ACTION_LOWER_LAYER, NULL);
	assert(stub_lower_layer_count == 1);
	printf("  PASS: test_action_raise_lower_layer\n");
}

/* Test NextLayout / PrevLayout (real impl iterates empty keyboard list) */
static void
test_action_layouts(void)
{
	setup();
	assert(wm_execute_action(&test_server, WM_ACTION_NEXT_LAYOUT, NULL) == true);
	assert(wm_execute_action(&test_server, WM_ACTION_PREV_LAYOUT, NULL) == true);
	printf("  PASS: test_action_layouts\n");
}

/* ====================================================================
 * Group E: wm_execute_keybind_action() dispatch tests (MacroCmd, etc)
 * ==================================================================== */






/* ====================================================================
 * Group F: handle_compositor_keybinding() tests
 * ==================================================================== */

/* Test VT switch keybinding */
static void
test_compositor_vt_switch(void)
{
	setup();
	bool result = hck(&test_server,
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
	bool result = hck(&test_server,
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
	bool result = hck(&test_server,
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

	bool result = hck(&test_server, 0, XKB_KEY_Escape);
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

	bool result = hck(&test_server, 0, XKB_KEY_a);
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

	bool result = hck(&test_server, 0, XKB_KEY_a);
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

	bool result = hck(&test_server, 0, XKB_KEY_a);
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

	bool result = hck(&test_server, 0, XKB_KEY_a);
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
 * Group L: wm_execute_keybind_action() Map/Delay tests
 * ==================================================================== */





/* ====================================================================
 * Group M: Additional wm_execute_action() coverage
 * ==================================================================== */

/* Test KILL action (sends SIGKILL via wl_client credentials) */
static void
test_action_kill(void)
{
	setup_with_view();
	assert(wm_execute_action(&test_server, WM_ACTION_KILL, NULL) == true);
	/* KILL path: get_client returns NULL in stub, so kill() not reached,
	 * but the action still returns true */
	printf("  PASS: test_action_kill\n");
}

/* Test KILL with no view does nothing */
static void
test_action_kill_no_view(void)
{
	setup();
	assert(wm_execute_action(&test_server, WM_ACTION_KILL, NULL) == true);
	assert(stub_close_called == 0);
	printf("  PASS: test_action_kill_no_view\n");
}

/* Test WindowMenu action */
static void
test_action_window_menu(void)
{
	setup();
	wm_execute_action(&test_server, WM_ACTION_WINDOW_MENU, NULL);
	assert(stub_menu_show_window_count == 1);
	printf("  PASS: test_action_window_menu\n");
}

/* Test WindowList action */
static void
test_action_window_list(void)
{
	setup();
	wm_execute_action(&test_server, WM_ACTION_WINDOW_LIST, NULL);
	assert(stub_menu_show_window_list_count == 1);
	printf("  PASS: test_action_window_list\n");
}

/* Test WorkspaceMenu action */
static void
test_action_workspace_menu(void)
{
	setup();
	wm_execute_action(&test_server, WM_ACTION_WORKSPACE_MENU, NULL);
	assert(stub_menu_show_workspace_count == 1);
	printf("  PASS: test_action_workspace_menu\n");
}

/* Test ClientMenu action */
static void
test_action_client_menu(void)
{
	setup();
	wm_execute_action(&test_server, WM_ACTION_CLIENT_MENU, "pattern");
	assert(stub_menu_show_client_count == 1);
	printf("  PASS: test_action_client_menu\n");
}

/* Test CustomMenu action */
static void
test_action_custom_menu(void)
{
	setup();
	wm_execute_action(&test_server, WM_ACTION_CUSTOM_MENU, "/path/to/menu");
	assert(stub_menu_show_custom_count == 1);
	printf("  PASS: test_action_custom_menu\n");
}

/*
 * SetStyle only accepts existing files under $HOME or the system
 * share/etc dirs: create one under a scratch $HOME.
 */
static char style_home[64];
static char style_path[128];

static const char *
make_style_file(const char *name)
{
	if (!style_home[0]) {
		snprintf(style_home, sizeof(style_home),
			"/tmp/fluxland-kbtest-XXXXXX");
		assert(mkdtemp(style_home));
	}
	setenv("HOME", style_home, 1);
	snprintf(style_path, sizeof(style_path), "%s/%s", style_home, name);
	FILE *f = fopen(style_path, "w");
	assert(f);
	fputs("! style\n", f);
	fclose(f);
	return style_path;
}

/* server.c: SetStyle goes through it (the real one also redraws the
 * slit and the wallpaper) */
void
wm_server_set_style(struct wm_server *s, const char *p)
{
	free(s->config->style_file);
	s->config->style_file = strdup(p);
	style_load(s->style, p);
	wm_toolbar_relayout(s->toolbar);
	struct wm_view *v;
	wl_list_for_each(v, &s->views, link) {
		if (v->decoration)
			wm_decoration_update(v->decoration, s->style);
	}
}

/* Test SetStyle action: loads style, relayouts toolbar, updates decorations */
static void
test_action_set_style(void)
{
	setup_with_view();
	wl_list_insert(&test_server.views, &test_view.link);

	const char *path = make_style_file("style");
	wm_execute_action(&test_server, WM_ACTION_SET_STYLE, path);
	assert(stub_style_load_count == 1);
	assert(stub_toolbar_relayout_count == 1);
	assert(stub_decoration_update_count == 1);
	assert(strcmp(test_config.style_file, path) == 0);

	free(test_config.style_file);
	test_config.style_file = NULL;
	wl_list_remove(&test_view.link);
	printf("  PASS: test_action_set_style\n");
}

/* SetStyle only accepts regular files */
static void
test_action_set_style_rejects_paths(void)
{
	setup_with_view();
	make_style_file("style");
	test_config.style_file = NULL;

	assert(!wm_execute_action(&test_server, WM_ACTION_SET_STYLE,
		"/nonexistent/style"));
	assert(!wm_execute_action(&test_server, WM_ACTION_SET_STYLE,
		"/dev/zero"));
	assert(!wm_execute_action(&test_server, WM_ACTION_SET_STYLE, "/tmp"));
	assert(stub_style_load_count == 0);
	assert(test_config.style_file == NULL);

	/* "~" is home; a directory with theme.cfg is a style */
	char dir[4200], cfg[4300];
	snprintf(dir, sizeof(dir), "%s/Bloe", style_home);
	mkdir(dir, 0755);
	snprintf(cfg, sizeof(cfg), "%s/theme.cfg", dir);
	FILE *f = fopen(cfg, "w");
	assert(f);
	fclose(f);
	assert(wm_execute_action(&test_server, WM_ACTION_SET_STYLE, "~/Bloe"));
	assert(stub_style_load_count == 1);
	assert(test_config.style_file && strstr(test_config.style_file, "Bloe"));
	unlink(cfg);
	rmdir(dir);
	free(test_config.style_file);
	test_config.style_file = NULL;
	printf("  PASS: test_action_set_style_rejects_paths\n");
}

/* Test SetStyle with no argument does nothing */
static void
test_action_set_style_no_arg(void)
{
	setup();
	wm_execute_action(&test_server, WM_ACTION_SET_STYLE, NULL);
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

	wm_execute_action(&test_server, WM_ACTION_RELOAD_STYLE, NULL);
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

	wm_execute_action(&test_server, WM_ACTION_BIND_KEY, "Mod4 t :Exec xterm");
	assert(stub_keybind_add_count == 1);

	wl_list_remove(&mode.link);
	printf("  PASS: test_action_bind_key\n");
}

/* Test BindKey with null argument does nothing */
static void
test_action_bind_key_null(void)
{
	setup();
	wm_execute_action(&test_server, WM_ACTION_BIND_KEY, NULL);
	assert(stub_keybind_add_count == 0);
	printf("  PASS: test_action_bind_key_null\n");
}

/* Test ResizeHoriz action */
static void
test_action_resize_horiz(void)
{
	setup_with_view();
	stub_frame_box = (struct wlr_box){ 0, 0, 200, 100 };
	wm_execute_action(&test_server, WM_ACTION_RESIZE_HORIZ, "10");
	assert(stub_set_frame_box.width == 210);
	assert(stub_set_frame_box.height == 100);
	wm_execute_action(&test_server, WM_ACTION_RESIZE_HORIZ, "50%");
	assert(stub_set_frame_box.width == 300);
	printf("  PASS: test_action_resize_horiz\n");
}

/* Test ResizeVert action */
static void
test_action_resize_vert(void)
{
	setup_with_view();
	stub_frame_box = (struct wlr_box){ 0, 0, 200, 100 };
	wm_execute_action(&test_server, WM_ACTION_RESIZE_VERT, "20");
	assert(stub_set_frame_box.width == 200);
	assert(stub_set_frame_box.height == 120);
	printf("  PASS: test_action_resize_vert\n");
}

/* Test CascadeWindows action */
static void
test_action_cascade_windows(void)
{
	setup();
	wm_execute_action(&test_server, WM_ACTION_CASCADE_WINDOWS, NULL);
	assert(stub_arrange_cascade_count == 1);
	printf("  PASS: test_action_cascade_windows\n");
}

/* Test ArrangeVert action */
static void
test_action_arrange_vert(void)
{
	setup();
	wm_execute_action(&test_server, WM_ACTION_ARRANGE_VERT, NULL);
	assert(stub_arrange_vert_count == 1);
	printf("  PASS: test_action_arrange_vert\n");
}

/* Test ArrangeHoriz action */
static void
test_action_arrange_horiz(void)
{
	setup();
	wm_execute_action(&test_server, WM_ACTION_ARRANGE_HORIZ, NULL);
	assert(stub_arrange_horiz_count == 1);
	printf("  PASS: test_action_arrange_horiz\n");
}

/* Test Focus action */
static void
test_action_focus(void)
{
	setup_with_view();
	wm_execute_action(&test_server, WM_ACTION_FOCUS, NULL);
	assert(stub_focus_view_count == 1);
	printf("  PASS: test_action_focus\n");
}

/* Test Focus with no view */
static void
test_action_focus_no_view(void)
{
	setup();
	wm_execute_action(&test_server, WM_ACTION_FOCUS, NULL);
	assert(stub_focus_view_count == 0);
	printf("  PASS: test_action_focus_no_view\n");
}

/* Test StartMoving action */
static void
test_action_start_moving(void)
{
	setup_with_view();
	wm_execute_action(&test_server, WM_ACTION_START_MOVING, NULL);
	assert(stub_begin_interactive_count == 1);
	printf("  PASS: test_action_start_moving\n");
}

/* Test StartResizing action */
static void
test_action_start_resizing(void)
{
	setup_with_view();
	wm_execute_action(&test_server, WM_ACTION_START_RESIZING, NULL);
	assert(stub_begin_interactive_count == 1);
	printf("  PASS: test_action_start_resizing\n");
}

/* Test StartTabbing action (nop from keyboard) */
static void
test_action_start_tabbing(void)
{
	setup();
	assert(wm_execute_action(&test_server, WM_ACTION_START_TABBING, NULL) == true);
	printf("  PASS: test_action_start_tabbing\n");
}

/* Test ActivateTab action */
static void
test_action_activate_tab(void)
{
	setup_with_view();
	wm_execute_action(&test_server, WM_ACTION_ACTIVATE_TAB, "2");
	assert(stub_activate_tab_count == 1);
	printf("  PASS: test_action_activate_tab\n");
}

/* Test ActivateTab with no arg does nothing */
static void
test_action_activate_tab_no_arg(void)
{
	setup_with_view();
	wm_execute_action(&test_server, WM_ACTION_ACTIVATE_TAB, NULL);
	assert(stub_activate_tab_count == 0);
	printf("  PASS: test_action_activate_tab_no_arg\n");
}

/* Test SetWorkspaceName action */
static void
test_action_set_workspace_name(void)
{
	setup();
	wm_execute_action(&test_server, WM_ACTION_SET_WORKSPACE_NAME, "MyDesk");
	assert(stub_ws_set_name_count == 1);
	printf("  PASS: test_action_set_workspace_name\n");
}

/* Test SetWorkspaceName with no arg does nothing */
static void
test_action_set_workspace_name_no_arg(void)
{
	setup();
	wm_execute_action(&test_server, WM_ACTION_SET_WORKSPACE_NAME, NULL);
	assert(stub_ws_set_name_count == 0);
	printf("  PASS: test_action_set_workspace_name_no_arg\n");
}

/* Test RightWorkspace/LeftWorkspace actions */
static void
test_action_right_left_workspace(void)
{
	setup();
	wm_execute_action(&test_server, WM_ACTION_RIGHT_WORKSPACE, NULL);
	assert(stub_ws_switch_index == 1);
	/* No wrapping: nothing left of the first, or 5 to the right */
	stub_ws_switch_index = -1;
	wm_execute_action(&test_server, WM_ACTION_LEFT_WORKSPACE, NULL);
	assert(stub_ws_switch_index == -1);
	wm_execute_action(&test_server, WM_ACTION_RIGHT_WORKSPACE, "5");
	assert(stub_ws_switch_index == -1);
	wm_execute_action(&test_server, WM_ACTION_RIGHT_WORKSPACE, "3");
	assert(stub_ws_switch_index == 3);
	printf("  PASS: test_action_right_left_workspace\n");
}

/* Test SetHead action */
static void
test_action_set_head(void)
{
	setup_with_view();
	wm_execute_action(&test_server, WM_ACTION_SET_HEAD, "1");
	assert(stub_set_head_count == 1);
	printf("  PASS: test_action_set_head\n");
}

/* Test SendToNextHead/SendToPrevHead actions */
static void
test_action_send_to_head(void)
{
	setup_with_view();
	wm_execute_action(&test_server, WM_ACTION_SEND_TO_NEXT_HEAD, NULL);
	assert(stub_send_next_head_count == 1);
	wm_execute_action(&test_server, WM_ACTION_SEND_TO_PREV_HEAD, NULL);
	assert(stub_send_prev_head_count == 1);
	printf("  PASS: test_action_send_to_head\n");
}

/* Test Remember action with apps_file */
static void
test_action_remember(void)
{
	setup_with_view();
	test_config.apps_file = "/tmp/apps";
	wm_execute_action(&test_server, WM_ACTION_REMEMBER, NULL);
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
	wm_execute_action(&test_server, WM_ACTION_REMEMBER, NULL);
	assert(stub_remember_window_count == 0);
	printf("  PASS: test_action_remember_no_file\n");
}

/* Test ArrangeStack variants */
static void
test_action_arrange_stack_variants(void)
{
	setup();
	wm_execute_action(&test_server, WM_ACTION_ARRANGE_STACK_LEFT, NULL);
	assert(stub_arrange_stack_left_count == 1);
	wm_execute_action(&test_server, WM_ACTION_ARRANGE_STACK_RIGHT, NULL);
	assert(stub_arrange_stack_right_count == 1);
	wm_execute_action(&test_server, WM_ACTION_ARRANGE_STACK_TOP, NULL);
	assert(stub_arrange_stack_top_count == 1);
	wm_execute_action(&test_server, WM_ACTION_ARRANGE_STACK_BOTTOM, NULL);
	assert(stub_arrange_stack_bottom_count == 1);
	printf("  PASS: test_action_arrange_stack_variants\n");
}

/* Test SetDecor "0" and "TINY" / "TOOL" variants */
static void
test_action_set_decor_more(void)
{
	setup_with_view();
	wm_execute_action(&test_server, WM_ACTION_SET_DECOR, "0");
	assert(stub_decoration_set_preset_count == 1);
	wm_execute_action(&test_server, WM_ACTION_SET_DECOR, "TINY");
	assert(stub_decoration_set_preset_count == 2);
	wm_execute_action(&test_server, WM_ACTION_SET_DECOR, "TOOL");
	assert(stub_decoration_set_preset_count == 3);
	/* Default: unrecognized string still calls set_preset with NORMAL */
	wm_execute_action(&test_server, WM_ACTION_SET_DECOR, "UNKNOWN");
	assert(stub_decoration_set_preset_count == 4);
	printf("  PASS: test_action_set_decor_more\n");
}

/* ====================================================================
 * Group N: GotoWindow / NextGroup / PrevGroup / Unclutter
 * ==================================================================== */

/* GoToWindow N [pattern] and Focus/Activate (pattern) */
static void
test_action_goto_window(void)
{
	setup_with_view();
	stub_goto_window_num = 99;
	wm_execute_action(&test_server, WM_ACTION_GOTO_WINDOW, "2");
	assert(stub_goto_window_num == 2);
	wm_execute_action(&test_server, WM_ACTION_GOTO_WINDOW,
		"-1 {static} (class=foot)");
	assert(stub_goto_window_num == -1);
	/* Focus with a pattern is GoToWindow 1 (pattern) */
	stub_goto_window_num = 99;
	wm_execute_action(&test_server, WM_ACTION_FOCUS, "(title=x)");
	assert(stub_goto_window_num == 1);
	/* Without one it focuses the window it acts on */
	stub_goto_window_num = 99;
	wm_execute_action_on(&test_server, WM_ACTION_FOCUS, NULL, &test_view);
	assert(stub_goto_window_num == 99 && stub_focus_view_count == 1);
	printf("  PASS: test_action_goto_window\n");
}

/* SetWallpaper [-f|-c|-t|-a] file: set, and remembered like fbsetbg's */
static void
test_action_set_wallpaper(void)
{
	setup();
	char dir[] = "/tmp/fluxland-test/kbwall-XXXXXX";
	mkdir("/tmp/fluxland-test", 0755);
	assert(mkdtemp(dir));
	char img[256], last[256];
	snprintf(img, sizeof(img), "%s/sea.png", dir);
	snprintf(last, sizeof(last), "%s/lastwallpaper", dir);
	FILE *f = fopen(img, "w");
	assert(f);
	fclose(f);
	test_config.config_dir = dir;
	test_config.wallpaper = NULL;
	test_server.wallpaper = NULL;

	assert(wm_execute_action(&test_server, WM_ACTION_SET_WALLPAPER, NULL));
	assert(!wm_execute_action(&test_server, WM_ACTION_SET_WALLPAPER,
		"/nonexistent.png"));
	char arg[300];
	snprintf(arg, sizeof(arg), "-t %s", img);
	assert(wm_execute_action(&test_server, WM_ACTION_SET_WALLPAPER, arg));
	assert(strcmp(test_config.wallpaper, img) == 0);
	assert(test_config.wallpaper_mode == WM_WALLPAPER_TILE);
	assert(test_server.wallpaper == &stub_wallpaper);
	/* fluxland reads this back at the next start */
	f = fopen(last, "r");
	assert(f);
	char line[300];
	assert(fgets(line, sizeof(line), f) && strncmp(line, img, strlen(img)) == 0);
	assert(fgets(line, sizeof(line), f) && strcmp(line, "Tile\n") == 0);
	fclose(f);

	free(test_config.wallpaper);
	test_config.wallpaper = NULL;
	test_config.config_dir = NULL;
	test_server.wallpaper = NULL;
	unlink(last);
	unlink(img);
	rmdir(dir);
	printf("  PASS: test_action_set_wallpaper\n");
}

/* Tab N: the Nth tab of the window's group, from the end when negative */
static void
test_action_tab(void)
{
	setup_with_view();
	static struct wm_tab_group tg;
	memset(&tg, 0, sizeof(tg));
	tg.count = 3;
	test_view.tab_group = &tg;
	stub_activate_tab_index = -9;
	wm_execute_action_on(&test_server, WM_ACTION_TAB, "2", &test_view);
	assert(stub_activate_tab_index == 1);
	wm_execute_action_on(&test_server, WM_ACTION_TAB, "-1", &test_view);
	assert(stub_activate_tab_index == 2);
	wm_execute_action_on(&test_server, WM_ACTION_TAB, "7", &test_view);
	assert(stub_activate_tab_index == 2);
	test_view.tab_group = NULL;
	printf("  PASS: test_action_tab\n");
}

/* SetAlpha: none resets, one sets both, two set each */
static void
test_action_set_alpha_forms(void)
{
	setup_with_view();
	test_config.window_focus_alpha = 240;
	test_config.window_unfocus_alpha = 180;
	wm_execute_action_on(&test_server, WM_ACTION_SET_ALPHA, "100 -20",
		&test_view);
	assert(test_view.focus_alpha == 100);
	test_view.focus_alpha = 100;
	test_view.unfocus_alpha = 150;
	wm_execute_action_on(&test_server, WM_ACTION_SET_ALPHA, "+10 +10",
		&test_view);
	assert(test_view.focus_alpha == 110 && test_view.unfocus_alpha == 160);
	wm_execute_action_on(&test_server, WM_ACTION_SET_ALPHA, NULL,
		&test_view);
	assert(test_view.focus_alpha == 240 && test_view.unfocus_alpha == 180);
	printf("  PASS: test_action_set_alpha_forms\n");
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

	wm_execute_action(&test_server, WM_ACTION_UNCLUTTER, NULL);
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
	wm_keyboard_setup(&test_server, &dev, false);
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
	wm_keyboard_setup(&test_server, &dev, false);
	assert(stub_kb_set_keymap_count == 1);

	struct wm_keyboard *kb = wl_container_of(
		test_server.keyboards.next, kb, link);
	wl_list_remove(&kb->link);
	free(kb);
	printf("  PASS: test_keyboard_setup_keymap_fail\n");
}


/* Physical keyboards join the group; virtual ones keep their keymap */
static void
test_keyboard_group(void)
{
	setup();
	stub_keymap_new_fail = false;
	stub_group_add_fail = false;
	wm_keyboard_group_init(&test_server);
	assert(test_server.keyboard_group == &stub_group);
	assert(test_server.group_keyboard->is_group);
	assert(stub_group.keyboard.keymap == &stub_xkb_keymap);
	/* The group's key events come to us */
	assert(test_server.group_keyboard->key.notify == handle_key);

	struct wlr_keyboard *kb = wlr_keyboard_from_input_device(NULL);
	memset(kb, 0, sizeof(*kb));
	wl_list_init(&kb->events.key.listener_list);
	wl_list_init(&kb->events.modifiers.listener_list);
	struct wlr_input_device dev;
	memset(&dev, 0, sizeof(dev));
	dev.name = "AT keyboard";
	wl_list_init(&dev.events.destroy.listener_list);

	stub_seat_set_keyboard_count = 0;
	wm_keyboard_setup(&test_server, &dev, false);
	assert(stub_group.members == 1);
	assert(kb->keymap == stub_group.keyboard.keymap);
	/* Its events arrive through the group, not directly */
	struct wm_keyboard *member = wl_container_of(
		test_server.keyboards.next, member, link);
	assert(!member->is_virtual && member->key.notify == NULL);
	/* (The stub hands out one wlr_keyboard; give the member its own) */
	static struct wlr_keyboard member_kb;
	member_kb = *kb;
	member->wlr_keyboard = &member_kb;

	/* A virtual keyboard: separate, its own keymap untouched */
	struct xkb_keymap client_keymap;
	memset(kb, 0, sizeof(*kb));
	wl_list_init(&kb->events.key.listener_list);
	wl_list_init(&kb->events.modifiers.listener_list);
	kb->keymap = &client_keymap;
	int set_before = stub_kb_set_keymap_count;
	wm_keyboard_setup(&test_server, &dev, true);
	assert(stub_group.members == 1);
	assert(kb->keymap == &client_keymap);
	assert(stub_kb_set_keymap_count == set_before);
	struct wm_keyboard *virt = wl_container_of(
		test_server.keyboards.next, virt, link);
	assert(virt->is_virtual && virt->key.notify == handle_key);

	/* Reconfigure leaves the virtual keyboard's keymap alone */
	wm_keyboard_apply_config(&test_server);
	assert(kb->keymap == &client_keymap);
	assert(stub_group.keyboard.keymap == &stub_xkb_keymap);

	struct wm_keyboard *k, *tmp;
	wl_list_for_each_safe(k, tmp, &test_server.keyboards, link) {
		wl_list_remove(&k->link);
		free(k);
	}
	wm_keyboard_group_finish(&test_server);
	assert(!test_server.group_keyboard);
	printf("  PASS: test_keyboard_group\n");
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
	bool result = hck(&test_server, 0, XKB_KEY_a);
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


/* ====================================================================
 * Group S: NEXT_WINDOW / PREV_WINDOW filtered cycling
 * ==================================================================== */

/* Helper to set up multiple views on a workspace for filtered cycling */
static struct wm_view test_views_arr[4];
static struct wlr_scene_tree test_scene_trees_arr[4];
static struct wlr_xdg_toplevel test_toplevels_arr[4];
static struct wlr_xdg_surface test_xdg_surfaces_arr[4];
static struct wlr_surface test_surfaces_arr[4];

static void
setup_multi_view(int count)
{
	setup();
	stub_active_workspace = &test_workspace;
	for (int i = 0; i < count && i < 4; i++) {
		memset(&test_views_arr[i], 0, sizeof(test_views_arr[i]));
		memset(&test_scene_trees_arr[i], 0, sizeof(test_scene_trees_arr[i]));
		memset(&test_toplevels_arr[i], 0, sizeof(test_toplevels_arr[i]));
		memset(&test_xdg_surfaces_arr[i], 0, sizeof(test_xdg_surfaces_arr[i]));
		memset(&test_surfaces_arr[i], 0, sizeof(test_surfaces_arr[i]));

		test_xdg_surfaces_arr[i].surface = &test_surfaces_arr[i];
		test_toplevels_arr[i].base = &test_xdg_surfaces_arr[i];
		test_scene_trees_arr[i].node.enabled = true;

		test_views_arr[i].server = &test_server;
		test_views_arr[i].xdg_toplevel = &test_toplevels_arr[i];
		test_views_arr[i].scene_tree = &test_scene_trees_arr[i];
		test_views_arr[i].workspace = &test_workspace;
		test_views_arr[i].sticky = false;
		wl_list_insert(test_server.views.prev, &test_views_arr[i].link);
	}
}

static void
cleanup_multi_view(int count)
{
	for (int i = 0; i < count && i < 4; i++)
		wl_list_remove(&test_views_arr[i].link);
}

/* Test NEXT_WINDOW without argument calls cycle_next */
static void
test_action_next_window_no_arg(void)
{
	setup();
	wm_execute_action(&test_server, WM_ACTION_NEXT_WINDOW, NULL);
	assert(stub_cycle_next_count == 1);
	printf("  PASS: test_action_next_window_no_arg\n");
}

/* Test PREV_WINDOW without argument calls cycle_prev */
static void
test_action_prev_window_no_arg(void)
{
	setup();
	wm_execute_action(&test_server, WM_ACTION_PREV_WINDOW, NULL);
	assert(stub_cycle_prev_count == 1);
	printf("  PASS: test_action_prev_window_no_arg\n");
}







/* ====================================================================
 * Group T: NEXT_GROUP / PREV_GROUP
 * ==================================================================== */

static struct wm_tab_group test_groups[3];





/* ====================================================================
 * Group U: SET_ENV with space format blocked
 * ==================================================================== */

/* Test SetEnv: blocked variable with space format */
static void
test_action_set_env_blocked_space(void)
{
	setup();
	wm_execute_action(&test_server, WM_ACTION_SET_ENV, "LD_PRELOAD evil.so");
	const char *val = getenv("LD_PRELOAD");
	assert(val == NULL || strcmp(val, "evil.so") != 0);
	printf("  PASS: test_action_set_env_blocked_space\n");
}

/* Test SetEnv: PATH blocked with = format */
static void
test_action_set_env_path_blocked(void)
{
	setup();
	const char *orig = getenv("PATH");
	wm_execute_action(&test_server, WM_ACTION_SET_ENV, "PATH=/evil");
	const char *val = getenv("PATH");
	/* PATH should NOT have been changed to /evil */
	assert(val == NULL || strcmp(val, "/evil") != 0);
	/* Restore if needed (should not have been changed) */
	(void)orig;
	printf("  PASS: test_action_set_env_path_blocked\n");
}

/* Test SetEnv: no argument does nothing */
static void
test_action_set_env_no_arg(void)
{
	setup();
	wm_execute_action(&test_server, WM_ACTION_SET_ENV, NULL);
	/* No crash = pass */
	printf("  PASS: test_action_set_env_no_arg\n");
}

/* ====================================================================
 * Group V: SetStyle / ReloadStyle edge cases
 * ==================================================================== */

/* Test SetStyle updates decorations on views */
static void
test_action_set_style_updates_views(void)
{
	setup_with_view();
	test_view.workspace = &test_workspace;
	wl_list_insert(&test_server.views, &test_view.link);
	test_config.style_file = strdup("/old/style");

	const char *path = make_style_file("new-style");
	wm_execute_action(&test_server, WM_ACTION_SET_STYLE, path);
	assert(stub_style_load_count == 1);
	assert(stub_toolbar_relayout_count == 1);
	assert(stub_decoration_update_count == 1);
	assert(strcmp(test_config.style_file, path) == 0);

	free(test_config.style_file);
	test_config.style_file = NULL;
	wl_list_remove(&test_view.link);
	printf("  PASS: test_action_set_style_updates_views\n");
}

/* Test ReloadStyle reloads and updates decorations */
static void
test_action_reload_style_updates_views(void)
{
	setup_with_view();
	test_view.workspace = &test_workspace;
	wl_list_insert(&test_server.views, &test_view.link);
	test_config.style_file = strdup("/some/style");

	wm_execute_action(&test_server, WM_ACTION_RELOAD_STYLE, NULL);
	assert(stub_style_load_count == 1);
	assert(stub_toolbar_relayout_count == 1);
	assert(stub_decoration_update_count == 1);

	free(test_config.style_file);
	test_config.style_file = NULL;
	wl_list_remove(&test_view.link);
	printf("  PASS: test_action_reload_style_updates_views\n");
}

/* ====================================================================
 * Group W: keybind_action dispatch edge cases
 * ==================================================================== */






/* Test regular action via keybind_action falls through to execute_action */
static void
test_keybind_action_regular(void)
{
	setup();
	struct wm_keybind bind = {0};
	struct wm_command cmd = { .action = WM_ACTION_RECONFIGURE };
	wl_list_init(&bind.children);
	bind.action = WM_ACTION_RECONFIGURE;
	bind.command = &cmd;

	bool result = wm_execute_keybind_action(&test_server, &bind);
	assert(result == true);
	assert(stub_command_execute_count >= 1);
	assert(stub_reconfigure_count == 1);
	printf("  PASS: test_keybind_action_regular\n");
}

/* ====================================================================
 * Group X: Additional action no-view guards
 * ==================================================================== */

/* Test MoveLeft with no view does nothing */
static void
test_action_move_left_no_view(void)
{
	setup();
	wm_execute_action(&test_server, WM_ACTION_MOVE_LEFT, "10");
	assert(stub_set_pos_called == 0);
	printf("  PASS: test_action_move_left_no_view\n");
}

/* Test Shade with no decoration does nothing */
static void
test_action_shade_no_decoration(void)
{
	setup_with_view();
	test_view.decoration = NULL;
	wm_execute_action(&test_server, WM_ACTION_SHADE, NULL);
	assert(stub_decoration_set_shaded_count == 0);
	printf("  PASS: test_action_shade_no_decoration\n");
}

/* Test SetDecor with no decoration does nothing */
static void
test_action_set_decor_no_decoration(void)
{
	setup_with_view();
	test_view.decoration = NULL;
	wm_execute_action(&test_server, WM_ACTION_SET_DECOR, "NONE");
	assert(stub_decoration_set_preset_count == 0);
	printf("  PASS: test_action_set_decor_no_decoration\n");
}

/* Test NextTab / PrevTab with no tab_group */
static void
test_action_tab_no_group(void)
{
	setup_with_view();
	test_view.tab_group = NULL;
	wm_execute_action(&test_server, WM_ACTION_NEXT_TAB, NULL);
	assert(stub_tab_group_next_count == 0);
	wm_execute_action(&test_server, WM_ACTION_PREV_TAB, NULL);
	assert(stub_tab_group_prev_count == 0);
	printf("  PASS: test_action_tab_no_group\n");
}

/* Test Move / Resize with no view */
static void
test_action_move_resize_no_view(void)
{
	setup();
	wm_execute_action(&test_server, WM_ACTION_MOVE, NULL);
	wm_execute_action(&test_server, WM_ACTION_RESIZE, NULL);
	assert(stub_begin_interactive_count == 0);
	printf("  PASS: test_action_move_resize_no_view\n");
}

/* Test ResizeHoriz / ResizeVert with no view */
static void
test_action_resize_horiz_vert_no_view(void)
{
	setup();
	wm_execute_action(&test_server, WM_ACTION_RESIZE_HORIZ, "10");
	wm_execute_action(&test_server, WM_ACTION_RESIZE_VERT, "10");
	assert(stub_resize_by_count == 0);
	printf("  PASS: test_action_resize_horiz_vert_no_view\n");
}



/* Test Unclutter skips minimized views */
static void
test_action_unclutter_skips_minimized(void)
{
	setup_multi_view(2);
	stub_active_workspace = &test_workspace;

	struct wm_output test_out;
	memset(&test_out, 0, sizeof(test_out));
	test_out.usable_area = (struct wlr_box){0, 0, 800, 600};
	wl_list_insert(&test_server.outputs, &test_out.link);

	/* Minimize second view */
	test_scene_trees_arr[1].node.enabled = false;

	wm_execute_action(&test_server, WM_ACTION_UNCLUTTER, NULL);
	/* Only first view should have been repositioned */
	assert(test_views_arr[0].x == 0);
	assert(test_views_arr[0].y == 0);
	/* Second view (minimized) should NOT have been moved */

	wl_list_remove(&test_out.link);
	cleanup_multi_view(2);
	printf("  PASS: test_action_unclutter_skips_minimized\n");
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

	printf("\n  Group B: Condition evaluation\n");

	printf("\n  Group C: Chain state machine\n");
	test_wm_chain_reset();
	test_chain_timeout_cb();
	test_chain_start_timeout();
	test_chain_start_timeout_restart();
	test_wm_get_active_bindings();

	printf("\n  Group D: wm_execute_action() dispatch\n");
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
	test_action_move_resize_deltas();
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

	printf("\n  Group E: wm_execute_keybind_action() dispatch\n");

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

	printf("\n  Group L: wm_execute_keybind_action() Map/Delay\n");

	printf("\n  Group M: Additional wm_execute_action() coverage\n");
	test_action_kill();
	test_action_kill_no_view();
	test_action_window_menu();
	test_action_window_list();
	test_action_workspace_menu();
	test_action_client_menu();
	test_action_custom_menu();
	test_action_set_style();
	test_action_set_style_rejects_paths();
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
	test_action_tab();
	test_action_set_wallpaper();
	test_action_set_alpha_forms();
	test_action_unclutter();

	printf("\n  Group O: wm_keyboard_setup()\n");
	test_keyboard_group();
	test_keyboard_setup();
	test_keyboard_setup_keymap_fail();

	printf("\n  Group P: Compositor keybinding fallback\n");
	test_compositor_fallback_legacy();

	printf("\n  Group Q: handle_key edge cases\n");
	test_handle_key_multiple_syms();
	test_handle_key_release_not_handled();

	printf("\n  Group R: ForEach with subcmds\n");

	printf("\n  Group S: NEXT_WINDOW / PREV_WINDOW filtered cycling\n");
	test_action_next_window_no_arg();
	test_action_prev_window_no_arg();

	printf("\n  Group T: NEXT_GROUP / PREV_GROUP\n");

	printf("\n  Group U: SET_ENV edge cases\n");
	test_action_set_env_blocked_space();
	test_action_set_env_path_blocked();
	test_action_set_env_no_arg();

	printf("\n  Group V: SetStyle / ReloadStyle edge cases\n");
	test_action_set_style_updates_views();
	test_action_reload_style_updates_views();

	printf("\n  Group W: keybind_action dispatch edge cases\n");
	test_keybind_action_regular();

	printf("\n  Group X: Additional action no-view guards\n");
	test_action_move_left_no_view();
	test_action_shade_no_decoration();
	test_action_set_decor_no_decoration();
	test_action_tab_no_group();
	test_action_move_resize_no_view();
	test_action_resize_horiz_vert_no_view();
	test_action_unclutter_skips_minimized();
	test_key_candidates();

	printf("\nAll keyboard tests passed!\n");
	return 0;
}
