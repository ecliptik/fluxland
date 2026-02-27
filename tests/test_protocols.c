/*
 * test_protocols.c - Unit tests for protocols.c
 *
 * Includes protocols.c directly with stub implementations to avoid
 * needing wlroots/wayland libraries at link time. Tests event handlers,
 * public update functions, init, and finish paths.
 */

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- Block real headers via include guards --- */

/* Wayland */
#define WAYLAND_SERVER_CORE_H
#define WAYLAND_SERVER_H
#define WAYLAND_UTIL_H

/* wlroots types */
#define WLR_USE_UNSTABLE 1
#define WLR_TYPES_WLR_ALPHA_MODIFIER_V1_H
#define WLR_TYPES_WLR_CONTENT_TYPE_V1_H
#define WLR_TYPES_WLR_CURSOR_H
#define WLR_TYPES_WLR_CURSOR_SHAPE_V1_H
#define WLR_TYPES_WLR_DATA_CONTROL_V1_H
#define WLR_TYPES_WLR_FOREIGN_TOPLEVEL_LIST_V1_H
#define WLR_TYPES_WLR_KEYBOARD_SHORTCUTS_INHIBIT_V1_H
#define WLR_TYPES_WLR_LINUX_DMABUF_V1_H
#define WLR_TYPES_WLR_OUTPUT_H
#define WLR_TYPES_WLR_OUTPUT_POWER_MANAGEMENT_V1_H
#define WLR_TYPES_WLR_POINTER_CONSTRAINTS_V1_H
#define WLR_TYPES_WLR_POINTER_GESTURES_V1_H
#define WLR_TYPES_WLR_PRIMARY_SELECTION_H
#define WLR_TYPES_WLR_PRIMARY_SELECTION_V1_H
#define WLR_TYPES_WLR_RELATIVE_POINTER_V1_H
#define WLR_TYPES_WLR_SEAT_H
#define WLR_TYPES_WLR_SECURITY_CONTEXT_V1_H
#define WLR_TYPES_WLR_TEARING_CONTROL_MANAGER_V1_H
#define WLR_TYPES_WLR_VIRTUAL_KEYBOARD_V1_H
#define WLR_TYPES_WLR_VIRTUAL_POINTER_V1_H
#define WLR_TYPES_WLR_XDG_ACTIVATION_V1
#define WLR_TYPES_WLR_XDG_FOREIGN_REGISTRY_H
#define WLR_TYPES_WLR_XDG_FOREIGN_V1_H
#define WLR_TYPES_WLR_XDG_FOREIGN_V2_H
#define WLR_TYPES_WLR_XDG_OUTPUT_V1_H
#define WLR_TYPES_WLR_XDG_SHELL_H
#define WLR_TYPES_WLR_XCURSOR_MANAGER_H
#define WLR_RENDER_WLR_RENDERER_H
#define WLR_UTIL_LOG_H

/* fluxland headers */
#define WM_PROTOCOLS_H
#define WM_SERVER_H
#define WM_VIEW_H
#define WM_KEYBOARD_H

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
	(__typeof__(sample))((char *)(ptr) - \
	offsetof(__typeof__(*sample), member))

#define wl_list_for_each(pos, head, member) \
	for (pos = wl_container_of((head)->next, pos, member); \
	     &pos->member != (head); \
	     pos = wl_container_of(pos->member.next, pos, member))

struct wl_signal {
	struct wl_list listener_list;
};

struct wl_listener {
	struct wl_list link;
	void (*notify)(struct wl_listener *, void *);
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

struct wl_display { int dummy; };

/* --- wlr_log stub --- */
#define wlr_log(verb, fmt, ...) ((void)0)
#define WLR_ERROR 1
#define WLR_INFO  3
#define WLR_DEBUG 7

/* --- Stub wlr types --- */

struct wlr_surface { int id; };

struct wlr_seat_client { int id; };

struct wlr_seat_keyboard_state {
	struct wlr_surface *focused_surface;
};

struct wlr_seat_pointer_state {
	struct wlr_seat_client *focused_client;
};

struct wlr_seat {
	struct wlr_seat_pointer_state pointer_state;
	struct wlr_seat_keyboard_state keyboard_state;
	struct {
		struct wl_signal request_set_primary_selection;
	} events;
};

struct wlr_primary_selection_source { int dummy; };

struct wlr_seat_request_set_primary_selection_event {
	struct wlr_primary_selection_source *source;
	uint32_t serial;
};

struct wlr_pointer_constraint_v1 {
	struct wlr_surface *surface;
	struct {
		struct wl_signal destroy;
	} events;
};

struct wlr_pointer_constraints_v1 {
	struct {
		struct wl_signal new_constraint;
	} events;
};

enum wlr_cursor_shape_manager_v1_device_type {
	WLR_CURSOR_SHAPE_MANAGER_V1_DEVICE_TYPE_POINTER,
	WLR_CURSOR_SHAPE_MANAGER_V1_DEVICE_TYPE_TABLET_TOOL,
};

struct wlr_cursor_shape_manager_v1 {
	struct {
		struct wl_signal request_set_shape;
	} events;
};

struct wlr_cursor_shape_manager_v1_request_set_shape_event {
	struct wlr_seat_client *seat_client;
	enum wlr_cursor_shape_manager_v1_device_type device_type;
	uint32_t shape;
};

struct wlr_input_device { int type; };

struct wlr_keyboard {
	struct wlr_input_device base;
};

struct wlr_virtual_keyboard_v1 {
	struct wlr_keyboard keyboard;
};

struct wlr_pointer {
	struct wlr_input_device base;
};

struct wlr_virtual_pointer_v1 {
	struct wlr_pointer pointer;
};

struct wlr_virtual_pointer_v1_new_pointer_event {
	struct wlr_virtual_pointer_v1 *new_pointer;
};

struct wlr_virtual_keyboard_manager_v1 {
	struct {
		struct wl_signal new_virtual_keyboard;
	} events;
};

struct wlr_virtual_pointer_manager_v1 {
	struct {
		struct wl_signal new_virtual_pointer;
	} events;
};

struct wlr_xdg_activation_token_v1 {
	struct wlr_seat *seat;
};

struct wlr_xdg_surface {
	struct wlr_surface *surface;
};

struct wlr_xdg_toplevel {
	struct wlr_xdg_surface *base;
};

struct wlr_xdg_activation_v1_request_activate_event {
	struct wlr_xdg_activation_token_v1 *token;
	struct wlr_surface *surface;
};

struct wlr_xdg_activation_v1 {
	struct {
		struct wl_signal request_activate;
	} events;
};

struct wlr_tearing_control_v1 {
	uint32_t current;
	struct {
		struct wl_signal set_hint;
		struct wl_signal destroy;
	} events;
};

struct wlr_tearing_control_manager_v1 {
	struct {
		struct wl_signal new_object;
	} events;
};

enum zwlr_output_power_v1_mode {
	ZWLR_OUTPUT_POWER_V1_MODE_OFF = 0,
	ZWLR_OUTPUT_POWER_V1_MODE_ON = 1,
};

struct wlr_output {
	char *name;
};

struct wlr_output_state { bool enabled; };

struct wlr_output_power_v1_set_mode_event {
	struct wlr_output *output;
	enum zwlr_output_power_v1_mode mode;
};

struct wlr_output_power_manager_v1 {
	struct {
		struct wl_signal set_mode;
	} events;
};

struct wlr_keyboard_shortcuts_inhibitor_v1 {
	struct wlr_surface *surface;
	struct wlr_seat *seat;
	bool active;
	struct wl_list link;
	struct {
		struct wl_signal destroy;
	} events;
};

struct wlr_keyboard_shortcuts_inhibit_manager_v1 {
	struct wl_list inhibitors;
	struct {
		struct wl_signal new_inhibitor;
	} events;
};

struct wlr_cursor { int dummy; };
struct wlr_xcursor_manager { int dummy; };
struct wlr_renderer { int dummy; };
struct wlr_output_layout { int dummy; };

/* Manager types for init stubs */
struct wlr_primary_selection_v1_device_manager { int dummy; };
struct wlr_relative_pointer_manager_v1 { int dummy; };
struct wlr_pointer_gestures_v1 { int dummy; };
struct wlr_data_control_manager_v1 { int dummy; };
struct wlr_content_type_manager_v1 { int dummy; };
struct wlr_alpha_modifier_v1 { int dummy; };
struct wlr_security_context_manager_v1 { int dummy; };
struct wlr_linux_dmabuf_v1 { int dummy; };
struct wlr_xdg_output_manager_v1 { int dummy; };
struct wlr_xdg_foreign_registry { int dummy; };
struct wlr_xdg_foreign_v1 { int dummy; };
struct wlr_xdg_foreign_v2 { int dummy; };
struct wlr_ext_foreign_toplevel_list_v1 { int dummy; };

/* --- Stub wm types (needed by protocols.c via wl_container_of) --- */

struct wm_view {
	struct wl_list link;
	struct wlr_xdg_toplevel *xdg_toplevel;
	char *title;
};

struct wm_server {
	struct wl_display *wl_display;
	struct wlr_seat *seat;
	struct wlr_cursor *cursor;
	struct wlr_xcursor_manager *cursor_mgr;
	struct wlr_renderer *renderer;
	struct wlr_output_layout *output_layout;
	struct wl_list views;

	/* Primary selection */
	struct wlr_primary_selection_v1_device_manager *primary_selection_mgr;
	struct wl_listener request_set_primary_selection;

	/* Pointer constraints */
	struct wlr_pointer_constraints_v1 *pointer_constraints;
	struct wlr_pointer_constraint_v1 *active_constraint;
	struct wl_listener new_pointer_constraint;
	struct wl_listener pointer_constraint_destroy;

	/* Relative pointer */
	struct wlr_relative_pointer_manager_v1 *relative_pointer_mgr;

	/* Pointer gestures */
	struct wlr_pointer_gestures_v1 *pointer_gestures;

	/* Cursor shape */
	struct wlr_cursor_shape_manager_v1 *cursor_shape_mgr;
	struct wl_listener cursor_shape_request;

	/* Virtual keyboard */
	struct wlr_virtual_keyboard_manager_v1 *virtual_keyboard_mgr;
	struct wl_listener new_virtual_keyboard;

	/* Virtual pointer */
	struct wlr_virtual_pointer_manager_v1 *virtual_pointer_mgr;
	struct wl_listener new_virtual_pointer;

	/* Data control */
	struct wlr_data_control_manager_v1 *data_control_mgr;

	/* XDG activation */
	struct wlr_xdg_activation_v1 *xdg_activation;
	struct wl_listener xdg_activation_request;

	/* Tearing control */
	struct wlr_tearing_control_manager_v1 *tearing_control_mgr;
	struct wl_listener tearing_new_object;

	/* Output power */
	struct wlr_output_power_manager_v1 *output_power_mgr;
	struct wl_listener output_power_set_mode;

	/* Keyboard shortcuts inhibit */
	struct wlr_keyboard_shortcuts_inhibit_manager_v1 *kb_shortcuts_inhibit_mgr;
	struct wl_listener new_kb_shortcuts_inhibitor;
	struct wlr_keyboard_shortcuts_inhibitor_v1 *active_kb_inhibitor;
	struct wl_listener kb_inhibitor_destroy;

	/* Content type */
	struct wlr_content_type_manager_v1 *content_type_mgr;

	/* Alpha modifier */
	struct wlr_alpha_modifier_v1 *alpha_modifier;

	/* Security context */
	struct wlr_security_context_manager_v1 *security_context_mgr;

	/* Linux DMA-BUF */
	struct wlr_linux_dmabuf_v1 *linux_dmabuf;

	/* XDG output */
	struct wlr_xdg_output_manager_v1 *xdg_output_mgr;

	/* XDG foreign */
	struct wlr_xdg_foreign_registry *xdg_foreign_registry;
	struct wlr_xdg_foreign_v1 *xdg_foreign_v1;
	struct wlr_xdg_foreign_v2 *xdg_foreign_v2;

	/* Ext foreign toplevel list */
	struct wlr_ext_foreign_toplevel_list_v1 *ext_foreign_toplevel_list;
};

/* --- Tracking counters --- */

static int g_set_primary_selection_count;
static int g_constraint_activated_count;
static int g_constraint_deactivated_count;
static int g_cursor_set_xcursor_count;
static const char *g_cursor_shape_name;
static int g_keyboard_setup_count;
static int g_cursor_attach_count;
static int g_focus_view_count;
static struct wm_view *g_focus_view_arg;
static int g_output_state_init_count;
static int g_output_state_set_enabled_count;
static bool g_output_state_enabled_value;
static int g_output_commit_count;
static int g_output_state_finish_count;
static int g_kb_inhibitor_activate_count;
static int g_kb_inhibitor_deactivate_count;
static int g_constraint_for_surface_count;
static struct wlr_pointer_constraint_v1 *g_constraint_for_surface_result;
static struct wlr_xdg_toplevel *g_try_from_surface_result;

/* Init stubs tracking */
static int g_init_primary_sel_count;
static int g_init_pointer_constraints_count;
static int g_init_relative_pointer_count;
static int g_init_pointer_gestures_count;
static int g_init_cursor_shape_count;
static int g_init_virtual_keyboard_count;
static int g_init_virtual_pointer_count;
static int g_init_data_control_count;
static int g_init_xdg_activation_count;
static int g_init_tearing_control_count;
static int g_init_output_power_count;
static int g_init_kb_shortcuts_inhibit_count;
static int g_init_content_type_count;
static int g_init_alpha_modifier_count;
static int g_init_security_context_count;
static int g_init_linux_dmabuf_count;
static int g_init_xdg_output_count;
static int g_init_xdg_foreign_registry_count;
static int g_init_xdg_foreign_v1_count;
static int g_init_xdg_foreign_v2_count;
static int g_init_ext_foreign_toplevel_count;

/* --- Static stub instances for init --- */
static struct wlr_primary_selection_v1_device_manager g_stub_primary_sel;
static struct wlr_pointer_constraints_v1 g_stub_pointer_constraints;
static struct wlr_relative_pointer_manager_v1 g_stub_relative_ptr;
static struct wlr_pointer_gestures_v1 g_stub_pointer_gestures;
static struct wlr_cursor_shape_manager_v1 g_stub_cursor_shape;
static struct wlr_virtual_keyboard_manager_v1 g_stub_vkbd_mgr;
static struct wlr_virtual_pointer_manager_v1 g_stub_vptr_mgr;
static struct wlr_data_control_manager_v1 g_stub_data_control;
static struct wlr_xdg_activation_v1 g_stub_xdg_activation;
static struct wlr_tearing_control_manager_v1 g_stub_tearing_ctrl;
static struct wlr_output_power_manager_v1 g_stub_output_power;
static struct wlr_keyboard_shortcuts_inhibit_manager_v1 g_stub_kb_inhibit;
static struct wlr_content_type_manager_v1 g_stub_content_type;
static struct wlr_alpha_modifier_v1 g_stub_alpha_mod;
static struct wlr_security_context_manager_v1 g_stub_sec_ctx;
static struct wlr_linux_dmabuf_v1 g_stub_dmabuf;
static struct wlr_xdg_output_manager_v1 g_stub_xdg_output;
static struct wlr_xdg_foreign_registry g_stub_xdg_foreign_reg;
static struct wlr_xdg_foreign_v1 g_stub_xdg_foreign1;
static struct wlr_xdg_foreign_v2 g_stub_xdg_foreign2;
static struct wlr_ext_foreign_toplevel_list_v1 g_stub_ext_toplevel;

/* --- Stub functions called by protocols.c --- */

static void
wlr_seat_set_primary_selection(struct wlr_seat *seat,
	struct wlr_primary_selection_source *source, uint32_t serial)
{
	(void)seat; (void)source; (void)serial;
	g_set_primary_selection_count++;
}

static void
wlr_pointer_constraint_v1_send_activated(
	struct wlr_pointer_constraint_v1 *constraint)
{
	(void)constraint;
	g_constraint_activated_count++;
}

static void
wlr_pointer_constraint_v1_send_deactivated(
	struct wlr_pointer_constraint_v1 *constraint)
{
	(void)constraint;
	g_constraint_deactivated_count++;
}

static const char *
wlr_cursor_shape_v1_name(uint32_t shape)
{
	(void)shape;
	return "test_cursor";
}

static void
wlr_cursor_set_xcursor(struct wlr_cursor *cursor,
	struct wlr_xcursor_manager *mgr, const char *name)
{
	(void)cursor; (void)mgr;
	g_cursor_set_xcursor_count++;
	g_cursor_shape_name = name;
}

static void
wm_keyboard_setup(struct wm_server *server, struct wlr_input_device *device)
{
	(void)server; (void)device;
	g_keyboard_setup_count++;
}

static void
wlr_cursor_attach_input_device(struct wlr_cursor *cursor,
	struct wlr_input_device *device)
{
	(void)cursor; (void)device;
	g_cursor_attach_count++;
}

static struct wlr_xdg_toplevel *
wlr_xdg_toplevel_try_from_wlr_surface(struct wlr_surface *surface)
{
	(void)surface;
	return g_try_from_surface_result;
}

static void
wm_focus_view(struct wm_view *view, struct wlr_surface *surface)
{
	(void)surface;
	g_focus_view_count++;
	g_focus_view_arg = view;
}

static void
wlr_output_state_init(struct wlr_output_state *state)
{
	memset(state, 0, sizeof(*state));
	g_output_state_init_count++;
}

static void
wlr_output_state_set_enabled(struct wlr_output_state *state, bool enabled)
{
	state->enabled = enabled;
	g_output_state_set_enabled_count++;
	g_output_state_enabled_value = enabled;
}

static bool
wlr_output_commit_state(struct wlr_output *output,
	struct wlr_output_state *state)
{
	(void)output; (void)state;
	g_output_commit_count++;
	return true;
}

static void
wlr_output_state_finish(struct wlr_output_state *state)
{
	(void)state;
	g_output_state_finish_count++;
}

static void
wlr_keyboard_shortcuts_inhibitor_v1_activate(
	struct wlr_keyboard_shortcuts_inhibitor_v1 *inhibitor)
{
	(void)inhibitor;
	g_kb_inhibitor_activate_count++;
}

static void
wlr_keyboard_shortcuts_inhibitor_v1_deactivate(
	struct wlr_keyboard_shortcuts_inhibitor_v1 *inhibitor)
{
	(void)inhibitor;
	g_kb_inhibitor_deactivate_count++;
}

static struct wlr_pointer_constraint_v1 *
wlr_pointer_constraints_v1_constraint_for_surface(
	struct wlr_pointer_constraints_v1 *constraints,
	struct wlr_surface *surface, struct wlr_seat *seat)
{
	(void)constraints; (void)surface; (void)seat;
	g_constraint_for_surface_count++;
	return g_constraint_for_surface_result;
}

/* --- Init stubs (_create functions) --- */

static struct wlr_primary_selection_v1_device_manager *
wlr_primary_selection_v1_device_manager_create(struct wl_display *display)
{
	(void)display;
	g_init_primary_sel_count++;
	return &g_stub_primary_sel;
}

static struct wlr_pointer_constraints_v1 *
wlr_pointer_constraints_v1_create(struct wl_display *display)
{
	(void)display;
	g_init_pointer_constraints_count++;
	wl_signal_init(&g_stub_pointer_constraints.events.new_constraint);
	return &g_stub_pointer_constraints;
}

static struct wlr_relative_pointer_manager_v1 *
wlr_relative_pointer_manager_v1_create(struct wl_display *display)
{
	(void)display;
	g_init_relative_pointer_count++;
	return &g_stub_relative_ptr;
}

static struct wlr_pointer_gestures_v1 *
wlr_pointer_gestures_v1_create(struct wl_display *display)
{
	(void)display;
	g_init_pointer_gestures_count++;
	return &g_stub_pointer_gestures;
}

static struct wlr_cursor_shape_manager_v1 *
wlr_cursor_shape_manager_v1_create(struct wl_display *display,
	uint32_t version)
{
	(void)display; (void)version;
	g_init_cursor_shape_count++;
	wl_signal_init(&g_stub_cursor_shape.events.request_set_shape);
	return &g_stub_cursor_shape;
}

static struct wlr_virtual_keyboard_manager_v1 *
wlr_virtual_keyboard_manager_v1_create(struct wl_display *display)
{
	(void)display;
	g_init_virtual_keyboard_count++;
	wl_signal_init(&g_stub_vkbd_mgr.events.new_virtual_keyboard);
	return &g_stub_vkbd_mgr;
}

static struct wlr_virtual_pointer_manager_v1 *
wlr_virtual_pointer_manager_v1_create(struct wl_display *display)
{
	(void)display;
	g_init_virtual_pointer_count++;
	wl_signal_init(&g_stub_vptr_mgr.events.new_virtual_pointer);
	return &g_stub_vptr_mgr;
}

static struct wlr_data_control_manager_v1 *
wlr_data_control_manager_v1_create(struct wl_display *display)
{
	(void)display;
	g_init_data_control_count++;
	return &g_stub_data_control;
}

static struct wlr_xdg_activation_v1 *
wlr_xdg_activation_v1_create(struct wl_display *display)
{
	(void)display;
	g_init_xdg_activation_count++;
	wl_signal_init(&g_stub_xdg_activation.events.request_activate);
	return &g_stub_xdg_activation;
}

static struct wlr_tearing_control_manager_v1 *
wlr_tearing_control_manager_v1_create(struct wl_display *display,
	uint32_t version)
{
	(void)display; (void)version;
	g_init_tearing_control_count++;
	wl_signal_init(&g_stub_tearing_ctrl.events.new_object);
	return &g_stub_tearing_ctrl;
}

static struct wlr_output_power_manager_v1 *
wlr_output_power_manager_v1_create(struct wl_display *display)
{
	(void)display;
	g_init_output_power_count++;
	wl_signal_init(&g_stub_output_power.events.set_mode);
	return &g_stub_output_power;
}

static struct wlr_keyboard_shortcuts_inhibit_manager_v1 *
wlr_keyboard_shortcuts_inhibit_v1_create(struct wl_display *display)
{
	(void)display;
	g_init_kb_shortcuts_inhibit_count++;
	wl_list_init(&g_stub_kb_inhibit.inhibitors);
	wl_signal_init(&g_stub_kb_inhibit.events.new_inhibitor);
	return &g_stub_kb_inhibit;
}

static struct wlr_content_type_manager_v1 *
wlr_content_type_manager_v1_create(struct wl_display *display,
	uint32_t version)
{
	(void)display; (void)version;
	g_init_content_type_count++;
	return &g_stub_content_type;
}

static struct wlr_alpha_modifier_v1 *
wlr_alpha_modifier_v1_create(struct wl_display *display)
{
	(void)display;
	g_init_alpha_modifier_count++;
	return &g_stub_alpha_mod;
}

static struct wlr_security_context_manager_v1 *
wlr_security_context_manager_v1_create(struct wl_display *display)
{
	(void)display;
	g_init_security_context_count++;
	return &g_stub_sec_ctx;
}

static struct wlr_linux_dmabuf_v1 *
wlr_linux_dmabuf_v1_create_with_renderer(struct wl_display *display,
	uint32_t version, struct wlr_renderer *renderer)
{
	(void)display; (void)version; (void)renderer;
	g_init_linux_dmabuf_count++;
	return &g_stub_dmabuf;
}

static struct wlr_xdg_output_manager_v1 *
wlr_xdg_output_manager_v1_create(struct wl_display *display,
	struct wlr_output_layout *layout)
{
	(void)display; (void)layout;
	g_init_xdg_output_count++;
	return &g_stub_xdg_output;
}

static struct wlr_xdg_foreign_registry *
wlr_xdg_foreign_registry_create(struct wl_display *display)
{
	(void)display;
	g_init_xdg_foreign_registry_count++;
	return &g_stub_xdg_foreign_reg;
}

static struct wlr_xdg_foreign_v1 *
wlr_xdg_foreign_v1_create(struct wl_display *display,
	struct wlr_xdg_foreign_registry *registry)
{
	(void)display; (void)registry;
	g_init_xdg_foreign_v1_count++;
	return &g_stub_xdg_foreign1;
}

static struct wlr_xdg_foreign_v2 *
wlr_xdg_foreign_v2_create(struct wl_display *display,
	struct wlr_xdg_foreign_registry *registry)
{
	(void)display; (void)registry;
	g_init_xdg_foreign_v2_count++;
	return &g_stub_xdg_foreign2;
}

static struct wlr_ext_foreign_toplevel_list_v1 *
wlr_ext_foreign_toplevel_list_v1_create(struct wl_display *display,
	uint32_t version)
{
	(void)display; (void)version;
	g_init_ext_foreign_toplevel_count++;
	return &g_stub_ext_toplevel;
}

/* --- Forward declarations for public functions (suppress -Wmissing-prototypes) --- */
void wm_protocols_init(struct wm_server *server);
void wm_protocols_finish(struct wm_server *server);
void wm_protocols_update_pointer_constraint(struct wm_server *server,
	struct wlr_surface *new_focus);
void wm_protocols_update_kb_shortcuts_inhibitor(struct wm_server *server,
	struct wlr_surface *new_focus);
bool wm_protocols_kb_shortcuts_inhibited(struct wm_server *server);

/* --- Include protocols.c source directly --- */

#include "protocols.c"

/* --- Test helpers --- */

static void
reset_globals(void)
{
	g_set_primary_selection_count = 0;
	g_constraint_activated_count = 0;
	g_constraint_deactivated_count = 0;
	g_cursor_set_xcursor_count = 0;
	g_cursor_shape_name = NULL;
	g_keyboard_setup_count = 0;
	g_cursor_attach_count = 0;
	g_focus_view_count = 0;
	g_focus_view_arg = NULL;
	g_output_state_init_count = 0;
	g_output_state_set_enabled_count = 0;
	g_output_state_enabled_value = false;
	g_output_commit_count = 0;
	g_output_state_finish_count = 0;
	g_kb_inhibitor_activate_count = 0;
	g_kb_inhibitor_deactivate_count = 0;
	g_constraint_for_surface_count = 0;
	g_constraint_for_surface_result = NULL;
	g_try_from_surface_result = NULL;

	g_init_primary_sel_count = 0;
	g_init_pointer_constraints_count = 0;
	g_init_relative_pointer_count = 0;
	g_init_pointer_gestures_count = 0;
	g_init_cursor_shape_count = 0;
	g_init_virtual_keyboard_count = 0;
	g_init_virtual_pointer_count = 0;
	g_init_data_control_count = 0;
	g_init_xdg_activation_count = 0;
	g_init_tearing_control_count = 0;
	g_init_output_power_count = 0;
	g_init_kb_shortcuts_inhibit_count = 0;
	g_init_content_type_count = 0;
	g_init_alpha_modifier_count = 0;
	g_init_security_context_count = 0;
	g_init_linux_dmabuf_count = 0;
	g_init_xdg_output_count = 0;
	g_init_xdg_foreign_registry_count = 0;
	g_init_xdg_foreign_v1_count = 0;
	g_init_xdg_foreign_v2_count = 0;
	g_init_ext_foreign_toplevel_count = 0;
}

/* Create a minimal server with initialized seat */
static struct wlr_seat g_test_seat;
static struct wl_display g_test_display;
static struct wlr_cursor g_test_cursor;
static struct wlr_xcursor_manager g_test_cursor_mgr;
static struct wlr_renderer g_test_renderer;
static struct wlr_output_layout g_test_output_layout;

static void
init_server(struct wm_server *server)
{
	memset(server, 0, sizeof(*server));
	memset(&g_test_seat, 0, sizeof(g_test_seat));
	wl_signal_init(&g_test_seat.events.request_set_primary_selection);

	server->wl_display = &g_test_display;
	server->seat = &g_test_seat;
	server->cursor = &g_test_cursor;
	server->cursor_mgr = &g_test_cursor_mgr;
	server->renderer = &g_test_renderer;
	server->output_layout = &g_test_output_layout;
	wl_list_init(&server->views);
}

/* ===== Tests ===== */

/* Test 1: wm_protocols_init creates all managers */
static void
test_init_creates_all_managers(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	wm_protocols_init(&server);

	assert(g_init_primary_sel_count == 1);
	assert(g_init_pointer_constraints_count == 1);
	assert(g_init_relative_pointer_count == 1);
	assert(g_init_pointer_gestures_count == 1);
	assert(g_init_cursor_shape_count == 1);
	assert(g_init_virtual_keyboard_count == 1);
	assert(g_init_virtual_pointer_count == 1);
	assert(g_init_data_control_count == 1);
	assert(g_init_xdg_activation_count == 1);
	assert(g_init_tearing_control_count == 1);
	assert(g_init_output_power_count == 1);
	assert(g_init_kb_shortcuts_inhibit_count == 1);
	assert(g_init_content_type_count == 1);
	assert(g_init_alpha_modifier_count == 1);
	assert(g_init_security_context_count == 1);
	assert(g_init_linux_dmabuf_count == 1);
	assert(g_init_xdg_output_count == 1);
	assert(g_init_xdg_foreign_registry_count == 1);
	assert(g_init_xdg_foreign_v1_count == 1);
	assert(g_init_xdg_foreign_v2_count == 1);
	assert(g_init_ext_foreign_toplevel_count == 1);

	/* Verify managers stored on server */
	assert(server.primary_selection_mgr == &g_stub_primary_sel);
	assert(server.pointer_constraints == &g_stub_pointer_constraints);
	assert(server.active_constraint == NULL);
	assert(server.active_kb_inhibitor == NULL);

	/* Clean up (finish needs initialized listeners) */
	wm_protocols_finish(&server);

	printf("  PASS: test_init_creates_all_managers\n");
}

/* Test 2: wm_protocols_finish removes listeners */
static void
test_finish_removes_listeners(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	wm_protocols_init(&server);
	wm_protocols_finish(&server);

	/* After finish, listener links should be NULL (removed from lists) */
	assert(server.kb_inhibitor_destroy.link.prev == NULL);
	assert(server.new_kb_shortcuts_inhibitor.link.prev == NULL);
	assert(server.output_power_set_mode.link.prev == NULL);

	printf("  PASS: test_finish_removes_listeners\n");
}

/* Test 3: handle_request_set_primary_selection dispatches correctly */
static void
test_handle_primary_selection(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	wm_protocols_init(&server);

	struct wlr_primary_selection_source source;
	struct wlr_seat_request_set_primary_selection_event event = {
		.source = &source,
		.serial = 42,
	};

	/* Invoke handler via listener */
	server.request_set_primary_selection.notify(
		&server.request_set_primary_selection, &event);

	assert(g_set_primary_selection_count == 1);

	wm_protocols_finish(&server);

	printf("  PASS: test_handle_primary_selection\n");
}

/* Test 4: handle_new_constraint activates when surface has focus */
static void
test_new_constraint_focused(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	wm_protocols_init(&server);

	struct wlr_surface surface = { .id = 1 };
	g_test_seat.keyboard_state.focused_surface = &surface;

	struct wlr_pointer_constraint_v1 constraint = {
		.surface = &surface,
	};
	wl_signal_init(&constraint.events.destroy);

	server.new_pointer_constraint.notify(
		&server.new_pointer_constraint, &constraint);

	assert(g_constraint_activated_count == 1);
	assert(g_constraint_deactivated_count == 0);
	assert(server.active_constraint == &constraint);

	wm_protocols_finish(&server);

	printf("  PASS: test_new_constraint_focused\n");
}

/* Test 5: handle_new_constraint does nothing when surface not focused */
static void
test_new_constraint_not_focused(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	wm_protocols_init(&server);

	struct wlr_surface surface1 = { .id = 1 };
	struct wlr_surface surface2 = { .id = 2 };
	g_test_seat.keyboard_state.focused_surface = &surface1;

	struct wlr_pointer_constraint_v1 constraint = {
		.surface = &surface2,
	};
	wl_signal_init(&constraint.events.destroy);

	server.new_pointer_constraint.notify(
		&server.new_pointer_constraint, &constraint);

	assert(g_constraint_activated_count == 0);
	assert(server.active_constraint == NULL);

	wm_protocols_finish(&server);

	printf("  PASS: test_new_constraint_not_focused\n");
}

/* Test 6: handle_new_constraint deactivates old constraint */
static void
test_new_constraint_replaces_existing(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	wm_protocols_init(&server);

	struct wlr_surface surface = { .id = 1 };
	g_test_seat.keyboard_state.focused_surface = &surface;

	struct wlr_pointer_constraint_v1 old_constraint = {
		.surface = &surface,
	};
	wl_signal_init(&old_constraint.events.destroy);
	server.active_constraint = &old_constraint;

	struct wlr_pointer_constraint_v1 new_constraint = {
		.surface = &surface,
	};
	wl_signal_init(&new_constraint.events.destroy);

	server.new_pointer_constraint.notify(
		&server.new_pointer_constraint, &new_constraint);

	assert(g_constraint_deactivated_count == 1);
	assert(g_constraint_activated_count == 1);
	assert(server.active_constraint == &new_constraint);

	wm_protocols_finish(&server);

	printf("  PASS: test_new_constraint_replaces_existing\n");
}

/* Test 7: handle_constraint_destroy clears active constraint */
static void
test_constraint_destroy(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	wm_protocols_init(&server);

	struct wlr_surface surface = { .id = 1 };
	g_test_seat.keyboard_state.focused_surface = &surface;

	struct wlr_pointer_constraint_v1 constraint = {
		.surface = &surface,
	};
	wl_signal_init(&constraint.events.destroy);

	/* Activate first */
	server.new_pointer_constraint.notify(
		&server.new_pointer_constraint, &constraint);
	assert(server.active_constraint == &constraint);

	/* Now fire destroy */
	server.pointer_constraint_destroy.notify(
		&server.pointer_constraint_destroy, NULL);

	assert(server.active_constraint == NULL);

	wm_protocols_finish(&server);

	printf("  PASS: test_constraint_destroy\n");
}

/* Test 8: handle_cursor_shape_request - non-pointer device type ignored */
static void
test_cursor_shape_non_pointer(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	wm_protocols_init(&server);

	struct wlr_cursor_shape_manager_v1_request_set_shape_event event = {
		.device_type = WLR_CURSOR_SHAPE_MANAGER_V1_DEVICE_TYPE_TABLET_TOOL,
		.seat_client = NULL,
		.shape = 0,
	};

	server.cursor_shape_request.notify(
		&server.cursor_shape_request, &event);

	assert(g_cursor_set_xcursor_count == 0);

	wm_protocols_finish(&server);

	printf("  PASS: test_cursor_shape_non_pointer\n");
}

/* Test 9: handle_cursor_shape_request - focus mismatch ignored */
static void
test_cursor_shape_focus_mismatch(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	wm_protocols_init(&server);

	struct wlr_seat_client client1 = { .id = 1 };
	struct wlr_seat_client client2 = { .id = 2 };
	g_test_seat.pointer_state.focused_client = &client1;

	struct wlr_cursor_shape_manager_v1_request_set_shape_event event = {
		.device_type = WLR_CURSOR_SHAPE_MANAGER_V1_DEVICE_TYPE_POINTER,
		.seat_client = &client2,
		.shape = 0,
	};

	server.cursor_shape_request.notify(
		&server.cursor_shape_request, &event);

	assert(g_cursor_set_xcursor_count == 0);

	wm_protocols_finish(&server);

	printf("  PASS: test_cursor_shape_focus_mismatch\n");
}

/* Test 10: handle_cursor_shape_request - success path */
static void
test_cursor_shape_success(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	wm_protocols_init(&server);

	struct wlr_seat_client client = { .id = 1 };
	g_test_seat.pointer_state.focused_client = &client;

	struct wlr_cursor_shape_manager_v1_request_set_shape_event event = {
		.device_type = WLR_CURSOR_SHAPE_MANAGER_V1_DEVICE_TYPE_POINTER,
		.seat_client = &client,
		.shape = 1,
	};

	server.cursor_shape_request.notify(
		&server.cursor_shape_request, &event);

	assert(g_cursor_set_xcursor_count == 1);
	assert(g_cursor_shape_name != NULL);

	wm_protocols_finish(&server);

	printf("  PASS: test_cursor_shape_success\n");
}

/* Test 11: handle_new_virtual_keyboard calls wm_keyboard_setup */
static void
test_new_virtual_keyboard(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	wm_protocols_init(&server);

	struct wlr_virtual_keyboard_v1 vkbd;
	memset(&vkbd, 0, sizeof(vkbd));

	server.new_virtual_keyboard.notify(
		&server.new_virtual_keyboard, &vkbd);

	assert(g_keyboard_setup_count == 1);

	wm_protocols_finish(&server);

	printf("  PASS: test_new_virtual_keyboard\n");
}

/* Test 12: handle_new_virtual_pointer calls cursor_attach */
static void
test_new_virtual_pointer(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	wm_protocols_init(&server);

	struct wlr_virtual_pointer_v1 vptr;
	memset(&vptr, 0, sizeof(vptr));

	struct wlr_virtual_pointer_v1_new_pointer_event event = {
		.new_pointer = &vptr,
	};

	server.new_virtual_pointer.notify(
		&server.new_virtual_pointer, &event);

	assert(g_cursor_attach_count == 1);

	wm_protocols_finish(&server);

	printf("  PASS: test_new_virtual_pointer\n");
}

/* Test 13: xdg_activation denied without seat */
static void
test_xdg_activation_no_seat(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	wm_protocols_init(&server);

	struct wlr_xdg_activation_token_v1 token = { .seat = NULL };
	struct wlr_surface surface = { .id = 1 };

	struct wlr_xdg_activation_v1_request_activate_event event = {
		.token = &token,
		.surface = &surface,
	};

	server.xdg_activation_request.notify(
		&server.xdg_activation_request, &event);

	assert(g_focus_view_count == 0);

	wm_protocols_finish(&server);

	printf("  PASS: test_xdg_activation_no_seat\n");
}

/* Test 14: xdg_activation denied when surface is not a toplevel */
static void
test_xdg_activation_not_toplevel(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	wm_protocols_init(&server);

	struct wlr_xdg_activation_token_v1 token = { .seat = &g_test_seat };
	struct wlr_surface surface = { .id = 1 };
	g_try_from_surface_result = NULL; /* not a toplevel */

	struct wlr_xdg_activation_v1_request_activate_event event = {
		.token = &token,
		.surface = &surface,
	};

	server.xdg_activation_request.notify(
		&server.xdg_activation_request, &event);

	assert(g_focus_view_count == 0);

	wm_protocols_finish(&server);

	printf("  PASS: test_xdg_activation_not_toplevel\n");
}

/* Test 15: xdg_activation focuses matching view */
static void
test_xdg_activation_focus_view(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	wm_protocols_init(&server);

	struct wlr_surface toplevel_surface = { .id = 99 };
	struct wlr_xdg_surface xdg_surface = { .surface = &toplevel_surface };
	struct wlr_xdg_toplevel toplevel = { .base = &xdg_surface };

	struct wlr_xdg_activation_token_v1 token = { .seat = &g_test_seat };
	struct wlr_surface surface = { .id = 1 };
	g_try_from_surface_result = &toplevel;

	/* Add a matching view to the server's views list */
	struct wm_view view;
	memset(&view, 0, sizeof(view));
	view.xdg_toplevel = &toplevel;
	view.title = "test window";
	wl_list_insert(&server.views, &view.link);

	struct wlr_xdg_activation_v1_request_activate_event event = {
		.token = &token,
		.surface = &surface,
	};

	server.xdg_activation_request.notify(
		&server.xdg_activation_request, &event);

	assert(g_focus_view_count == 1);
	assert(g_focus_view_arg == &view);

	wm_protocols_finish(&server);

	printf("  PASS: test_xdg_activation_focus_view\n");
}

/* Test 16: xdg_activation no matching view */
static void
test_xdg_activation_no_matching_view(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	wm_protocols_init(&server);

	struct wlr_xdg_surface xdg_surface = { .surface = NULL };
	struct wlr_xdg_toplevel toplevel = { .base = &xdg_surface };
	struct wlr_xdg_toplevel other_toplevel = { .base = &xdg_surface };

	struct wlr_xdg_activation_token_v1 token = { .seat = &g_test_seat };
	struct wlr_surface surface = { .id = 1 };
	g_try_from_surface_result = &toplevel;

	/* Add a view that doesn't match */
	struct wm_view view;
	memset(&view, 0, sizeof(view));
	view.xdg_toplevel = &other_toplevel;
	wl_list_insert(&server.views, &view.link);

	struct wlr_xdg_activation_v1_request_activate_event event = {
		.token = &token,
		.surface = &surface,
	};

	server.xdg_activation_request.notify(
		&server.xdg_activation_request, &event);

	assert(g_focus_view_count == 0);

	wm_protocols_finish(&server);

	printf("  PASS: test_xdg_activation_no_matching_view\n");
}

/* Test 17: tearing_new_object allocates listeners */
static void
test_tearing_new_object(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	wm_protocols_init(&server);

	struct wlr_tearing_control_v1 control;
	memset(&control, 0, sizeof(control));
	control.current = 1;
	wl_signal_init(&control.events.set_hint);
	wl_signal_init(&control.events.destroy);

	server.tearing_new_object.notify(
		&server.tearing_new_object, &control);

	/* Verify listeners were added to the control's signals */
	assert(!wl_list_empty(&control.events.set_hint.listener_list));
	assert(!wl_list_empty(&control.events.destroy.listener_list));

	/* Fire destroy to free the allocated listeners */
	struct wl_listener *destroy_listener = wl_container_of(
		control.events.destroy.listener_list.next,
		destroy_listener, link);
	destroy_listener->notify(destroy_listener, NULL);

	wm_protocols_finish(&server);

	printf("  PASS: test_tearing_new_object\n");
}

/* Test 18: output_power_set_mode ON */
static void
test_output_power_on(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	wm_protocols_init(&server);

	struct wlr_output output = { .name = "test-output" };
	struct wlr_output_power_v1_set_mode_event event = {
		.output = &output,
		.mode = ZWLR_OUTPUT_POWER_V1_MODE_ON,
	};

	server.output_power_set_mode.notify(
		&server.output_power_set_mode, &event);

	assert(g_output_state_init_count == 1);
	assert(g_output_state_set_enabled_count == 1);
	assert(g_output_state_enabled_value == true);
	assert(g_output_commit_count == 1);
	assert(g_output_state_finish_count == 1);

	wm_protocols_finish(&server);

	printf("  PASS: test_output_power_on\n");
}

/* Test 19: output_power_set_mode OFF */
static void
test_output_power_off(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	wm_protocols_init(&server);

	struct wlr_output output = { .name = "test-output" };
	struct wlr_output_power_v1_set_mode_event event = {
		.output = &output,
		.mode = ZWLR_OUTPUT_POWER_V1_MODE_OFF,
	};

	server.output_power_set_mode.notify(
		&server.output_power_set_mode, &event);

	assert(g_output_state_set_enabled_count == 1);
	assert(g_output_state_enabled_value == false);
	assert(g_output_commit_count == 1);

	wm_protocols_finish(&server);

	printf("  PASS: test_output_power_off\n");
}

/* Test 20: new_kb_shortcuts_inhibitor activates when surface focused */
static void
test_new_kb_inhibitor_focused(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	wm_protocols_init(&server);

	struct wlr_surface surface = { .id = 1 };
	g_test_seat.keyboard_state.focused_surface = &surface;

	struct wlr_keyboard_shortcuts_inhibitor_v1 inhibitor;
	memset(&inhibitor, 0, sizeof(inhibitor));
	inhibitor.surface = &surface;
	wl_signal_init(&inhibitor.events.destroy);

	server.new_kb_shortcuts_inhibitor.notify(
		&server.new_kb_shortcuts_inhibitor, &inhibitor);

	assert(g_kb_inhibitor_activate_count == 1);
	assert(server.active_kb_inhibitor == &inhibitor);

	wm_protocols_finish(&server);

	printf("  PASS: test_new_kb_inhibitor_focused\n");
}

/* Test 21: new_kb_shortcuts_inhibitor does nothing when not focused */
static void
test_new_kb_inhibitor_not_focused(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	wm_protocols_init(&server);

	struct wlr_surface surface1 = { .id = 1 };
	struct wlr_surface surface2 = { .id = 2 };
	g_test_seat.keyboard_state.focused_surface = &surface1;

	struct wlr_keyboard_shortcuts_inhibitor_v1 inhibitor;
	memset(&inhibitor, 0, sizeof(inhibitor));
	inhibitor.surface = &surface2;
	wl_signal_init(&inhibitor.events.destroy);

	server.new_kb_shortcuts_inhibitor.notify(
		&server.new_kb_shortcuts_inhibitor, &inhibitor);

	assert(g_kb_inhibitor_activate_count == 0);
	assert(server.active_kb_inhibitor == NULL);

	wm_protocols_finish(&server);

	printf("  PASS: test_new_kb_inhibitor_not_focused\n");
}

/* Test 22: new_kb_inhibitor replaces existing */
static void
test_new_kb_inhibitor_replaces_existing(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	wm_protocols_init(&server);

	struct wlr_surface surface = { .id = 1 };
	g_test_seat.keyboard_state.focused_surface = &surface;

	struct wlr_keyboard_shortcuts_inhibitor_v1 old_inhibitor;
	memset(&old_inhibitor, 0, sizeof(old_inhibitor));
	old_inhibitor.surface = &surface;
	server.active_kb_inhibitor = &old_inhibitor;

	struct wlr_keyboard_shortcuts_inhibitor_v1 new_inhibitor;
	memset(&new_inhibitor, 0, sizeof(new_inhibitor));
	new_inhibitor.surface = &surface;
	wl_signal_init(&new_inhibitor.events.destroy);

	server.new_kb_shortcuts_inhibitor.notify(
		&server.new_kb_shortcuts_inhibitor, &new_inhibitor);

	assert(g_kb_inhibitor_deactivate_count == 1);
	assert(g_kb_inhibitor_activate_count == 1);
	assert(server.active_kb_inhibitor == &new_inhibitor);

	wm_protocols_finish(&server);

	printf("  PASS: test_new_kb_inhibitor_replaces_existing\n");
}

/* Test 23: kb_inhibitor_destroy clears active inhibitor */
static void
test_kb_inhibitor_destroy(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	wm_protocols_init(&server);

	struct wlr_surface surface = { .id = 1 };
	g_test_seat.keyboard_state.focused_surface = &surface;

	struct wlr_keyboard_shortcuts_inhibitor_v1 inhibitor;
	memset(&inhibitor, 0, sizeof(inhibitor));
	inhibitor.surface = &surface;
	wl_signal_init(&inhibitor.events.destroy);

	/* Activate it first */
	server.new_kb_shortcuts_inhibitor.notify(
		&server.new_kb_shortcuts_inhibitor, &inhibitor);
	assert(server.active_kb_inhibitor == &inhibitor);

	/* Fire destroy */
	server.kb_inhibitor_destroy.notify(
		&server.kb_inhibitor_destroy, NULL);

	assert(server.active_kb_inhibitor == NULL);

	wm_protocols_finish(&server);

	printf("  PASS: test_kb_inhibitor_destroy\n");
}

/* Test 24: wm_protocols_kb_shortcuts_inhibited - no inhibitor */
static void
test_kb_inhibited_no_inhibitor(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	server.active_kb_inhibitor = NULL;

	assert(wm_protocols_kb_shortcuts_inhibited(&server) == false);

	printf("  PASS: test_kb_inhibited_no_inhibitor\n");
}

/* Test 25: wm_protocols_kb_shortcuts_inhibited - active and matching */
static void
test_kb_inhibited_active(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	struct wlr_surface surface = { .id = 1 };
	g_test_seat.keyboard_state.focused_surface = &surface;

	struct wlr_keyboard_shortcuts_inhibitor_v1 inhibitor;
	memset(&inhibitor, 0, sizeof(inhibitor));
	inhibitor.surface = &surface;
	inhibitor.active = true;

	server.active_kb_inhibitor = &inhibitor;

	assert(wm_protocols_kb_shortcuts_inhibited(&server) == true);

	printf("  PASS: test_kb_inhibited_active\n");
}

/* Test 26: wm_protocols_kb_shortcuts_inhibited - not active */
static void
test_kb_inhibited_not_active(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	struct wlr_surface surface = { .id = 1 };
	g_test_seat.keyboard_state.focused_surface = &surface;

	struct wlr_keyboard_shortcuts_inhibitor_v1 inhibitor;
	memset(&inhibitor, 0, sizeof(inhibitor));
	inhibitor.surface = &surface;
	inhibitor.active = false;

	server.active_kb_inhibitor = &inhibitor;

	assert(wm_protocols_kb_shortcuts_inhibited(&server) == false);

	printf("  PASS: test_kb_inhibited_not_active\n");
}

/* Test 27: wm_protocols_kb_shortcuts_inhibited - different surface */
static void
test_kb_inhibited_different_surface(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	struct wlr_surface surface1 = { .id = 1 };
	struct wlr_surface surface2 = { .id = 2 };
	g_test_seat.keyboard_state.focused_surface = &surface1;

	struct wlr_keyboard_shortcuts_inhibitor_v1 inhibitor;
	memset(&inhibitor, 0, sizeof(inhibitor));
	inhibitor.surface = &surface2;
	inhibitor.active = true;

	server.active_kb_inhibitor = &inhibitor;

	assert(wm_protocols_kb_shortcuts_inhibited(&server) == false);

	printf("  PASS: test_kb_inhibited_different_surface\n");
}

/* Test 28: update_kb_shortcuts_inhibitor - deactivate on focus change */
static void
test_update_kb_inhibitor_deactivate(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	wm_protocols_init(&server);

	struct wlr_surface old_surface = { .id = 1 };
	struct wlr_surface new_surface = { .id = 2 };

	struct wlr_keyboard_shortcuts_inhibitor_v1 inhibitor;
	memset(&inhibitor, 0, sizeof(inhibitor));
	inhibitor.surface = &old_surface;
	inhibitor.active = true;
	wl_signal_init(&inhibitor.events.destroy);

	/* Set up as if inhibitor was previously activated */
	server.active_kb_inhibitor = &inhibitor;
	/* Link the destroy listener so wl_list_remove works */
	wl_list_init(&server.kb_inhibitor_destroy.link);

	wm_protocols_update_kb_shortcuts_inhibitor(&server, &new_surface);

	assert(g_kb_inhibitor_deactivate_count == 1);
	assert(server.active_kb_inhibitor == NULL);

	wm_protocols_finish(&server);

	printf("  PASS: test_update_kb_inhibitor_deactivate\n");
}

/* Test 29: update_kb_shortcuts_inhibitor - activate matching inhibitor */
static void
test_update_kb_inhibitor_activate(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	wm_protocols_init(&server);

	struct wlr_surface surface = { .id = 1 };

	struct wlr_keyboard_shortcuts_inhibitor_v1 inhibitor;
	memset(&inhibitor, 0, sizeof(inhibitor));
	inhibitor.surface = &surface;
	inhibitor.seat = server.seat;
	wl_signal_init(&inhibitor.events.destroy);

	/* Add inhibitor to manager's list */
	wl_list_insert(&server.kb_shortcuts_inhibit_mgr->inhibitors,
		&inhibitor.link);

	wm_protocols_update_kb_shortcuts_inhibitor(&server, &surface);

	assert(g_kb_inhibitor_activate_count == 1);
	assert(server.active_kb_inhibitor == &inhibitor);

	wm_protocols_finish(&server);

	printf("  PASS: test_update_kb_inhibitor_activate\n");
}

/* Test 30: update_kb_shortcuts_inhibitor - NULL focus deactivates */
static void
test_update_kb_inhibitor_null_focus(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	wm_protocols_init(&server);

	struct wlr_surface surface = { .id = 1 };

	struct wlr_keyboard_shortcuts_inhibitor_v1 inhibitor;
	memset(&inhibitor, 0, sizeof(inhibitor));
	inhibitor.surface = &surface;
	wl_signal_init(&inhibitor.events.destroy);

	server.active_kb_inhibitor = &inhibitor;
	wl_list_init(&server.kb_inhibitor_destroy.link);

	wm_protocols_update_kb_shortcuts_inhibitor(&server, NULL);

	assert(g_kb_inhibitor_deactivate_count == 1);
	assert(server.active_kb_inhibitor == NULL);

	wm_protocols_finish(&server);

	printf("  PASS: test_update_kb_inhibitor_null_focus\n");
}

/* Test 31: update_pointer_constraint - deactivate on focus change */
static void
test_update_pointer_constraint_deactivate(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	wm_protocols_init(&server);

	struct wlr_surface old_surface = { .id = 1 };
	struct wlr_surface new_surface = { .id = 2 };

	struct wlr_pointer_constraint_v1 constraint;
	memset(&constraint, 0, sizeof(constraint));
	constraint.surface = &old_surface;
	wl_signal_init(&constraint.events.destroy);

	server.active_constraint = &constraint;
	wl_list_init(&server.pointer_constraint_destroy.link);

	wm_protocols_update_pointer_constraint(&server, &new_surface);

	assert(g_constraint_deactivated_count == 1);
	assert(server.active_constraint == NULL);

	wm_protocols_finish(&server);

	printf("  PASS: test_update_pointer_constraint_deactivate\n");
}

/* Test 32: update_pointer_constraint - activate for new focus */
static void
test_update_pointer_constraint_activate(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	wm_protocols_init(&server);

	struct wlr_surface surface = { .id = 1 };

	struct wlr_pointer_constraint_v1 constraint;
	memset(&constraint, 0, sizeof(constraint));
	constraint.surface = &surface;
	wl_signal_init(&constraint.events.destroy);

	g_constraint_for_surface_result = &constraint;

	wm_protocols_update_pointer_constraint(&server, &surface);

	assert(g_constraint_for_surface_count == 1);
	assert(g_constraint_activated_count == 1);
	assert(server.active_constraint == &constraint);

	wm_protocols_finish(&server);

	printf("  PASS: test_update_pointer_constraint_activate\n");
}

/* Test 33: update_pointer_constraint - NULL focus deactivates */
static void
test_update_pointer_constraint_null_focus(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	wm_protocols_init(&server);

	struct wlr_surface surface = { .id = 1 };

	struct wlr_pointer_constraint_v1 constraint;
	memset(&constraint, 0, sizeof(constraint));
	constraint.surface = &surface;
	wl_signal_init(&constraint.events.destroy);

	server.active_constraint = &constraint;
	wl_list_init(&server.pointer_constraint_destroy.link);

	wm_protocols_update_pointer_constraint(&server, NULL);

	assert(g_constraint_deactivated_count == 1);
	assert(server.active_constraint == NULL);

	wm_protocols_finish(&server);

	printf("  PASS: test_update_pointer_constraint_null_focus\n");
}

/* Test 34: update_pointer_constraint - no constraint for surface */
static void
test_update_pointer_constraint_no_match(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	wm_protocols_init(&server);

	struct wlr_surface surface = { .id = 1 };
	g_constraint_for_surface_result = NULL;

	wm_protocols_update_pointer_constraint(&server, &surface);

	assert(g_constraint_for_surface_count == 1);
	assert(g_constraint_activated_count == 0);
	assert(server.active_constraint == NULL);

	wm_protocols_finish(&server);

	printf("  PASS: test_update_pointer_constraint_no_match\n");
}

/* Test 35: update_pointer_constraint - same surface keeps constraint */
static void
test_update_pointer_constraint_same_surface(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	wm_protocols_init(&server);

	struct wlr_surface surface = { .id = 1 };

	struct wlr_pointer_constraint_v1 constraint;
	memset(&constraint, 0, sizeof(constraint));
	constraint.surface = &surface;
	wl_signal_init(&constraint.events.destroy);

	server.active_constraint = &constraint;
	wl_list_init(&server.pointer_constraint_destroy.link);

	/* Focus moves to same surface - constraint should stay */
	wm_protocols_update_pointer_constraint(&server, &surface);

	assert(g_constraint_deactivated_count == 0);
	assert(server.active_constraint == &constraint);

	wm_protocols_finish(&server);

	printf("  PASS: test_update_pointer_constraint_same_surface\n");
}

/* Test 36: update_kb_shortcuts_inhibitor - same surface keeps inhibitor */
static void
test_update_kb_inhibitor_same_surface(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	wm_protocols_init(&server);

	struct wlr_surface surface = { .id = 1 };

	struct wlr_keyboard_shortcuts_inhibitor_v1 inhibitor;
	memset(&inhibitor, 0, sizeof(inhibitor));
	inhibitor.surface = &surface;
	inhibitor.active = true;
	wl_signal_init(&inhibitor.events.destroy);

	server.active_kb_inhibitor = &inhibitor;
	wl_list_init(&server.kb_inhibitor_destroy.link);

	/* Focus stays on same surface - inhibitor should stay */
	wm_protocols_update_kb_shortcuts_inhibitor(&server, &surface);

	assert(g_kb_inhibitor_deactivate_count == 0);
	assert(server.active_kb_inhibitor == &inhibitor);

	wm_protocols_finish(&server);

	printf("  PASS: test_update_kb_inhibitor_same_surface\n");
}

/* Test 37: wm_protocols_kb_shortcuts_inhibited - no focused surface */
static void
test_kb_inhibited_no_focused_surface(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	g_test_seat.keyboard_state.focused_surface = NULL;

	struct wlr_keyboard_shortcuts_inhibitor_v1 inhibitor;
	memset(&inhibitor, 0, sizeof(inhibitor));
	inhibitor.active = true;
	inhibitor.surface = NULL;

	server.active_kb_inhibitor = &inhibitor;

	assert(wm_protocols_kb_shortcuts_inhibited(&server) == false);

	printf("  PASS: test_kb_inhibited_no_focused_surface\n");
}

int
main(void)
{
	printf("test_protocols:\n");

	test_init_creates_all_managers();
	test_finish_removes_listeners();
	test_handle_primary_selection();
	test_new_constraint_focused();
	test_new_constraint_not_focused();
	test_new_constraint_replaces_existing();
	test_constraint_destroy();
	test_cursor_shape_non_pointer();
	test_cursor_shape_focus_mismatch();
	test_cursor_shape_success();
	test_new_virtual_keyboard();
	test_new_virtual_pointer();
	test_xdg_activation_no_seat();
	test_xdg_activation_not_toplevel();
	test_xdg_activation_focus_view();
	test_xdg_activation_no_matching_view();
	test_tearing_new_object();
	test_output_power_on();
	test_output_power_off();
	test_new_kb_inhibitor_focused();
	test_new_kb_inhibitor_not_focused();
	test_new_kb_inhibitor_replaces_existing();
	test_kb_inhibitor_destroy();
	test_kb_inhibited_no_inhibitor();
	test_kb_inhibited_active();
	test_kb_inhibited_not_active();
	test_kb_inhibited_different_surface();
	test_update_kb_inhibitor_deactivate();
	test_update_kb_inhibitor_activate();
	test_update_kb_inhibitor_null_focus();
	test_update_pointer_constraint_deactivate();
	test_update_pointer_constraint_activate();
	test_update_pointer_constraint_null_focus();
	test_update_pointer_constraint_no_match();
	test_update_pointer_constraint_same_surface();
	test_update_kb_inhibitor_same_surface();
	test_kb_inhibited_no_focused_surface();

	printf("All protocols tests passed (37/37).\n");
	return 0;
}
