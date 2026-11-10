/*
 * test_eis.c - Unit tests for the input emulation of X11 programs (EIS)
 *
 * eis.c is #included with stand-ins for the server, the config and the
 * menus.  What is looked at is the decisions it makes (who may, which
 * process is XWayland) and what arrives as emulated input: real wlroots
 * pointer and keyboard devices are created, and their signals watched.
 */

#define _GNU_SOURCE

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef WM_HAS_EIS
#define WM_HAS_EIS
#endif
#ifndef WM_HAS_XWAYLAND
#define WM_HAS_XWAYLAND
#endif

#include <wayland-server-core.h>
#include <wlr/interfaces/wlr_keyboard.h>
#include <wlr/interfaces/wlr_pointer.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/xwayland.h>

/* --- Stand-ins for what eis.c uses --- */

#define WM_SERVER_H
#define WM_CONFIG_H
#define WM_IDLE_H
#define WM_INPUT_H
#define WM_KEYBOARD_H
#define WM_MENU_H
#define WM_MENU_PARSE_H
#define WM_SESSION_LOCK_H

enum wm_eis_policy {
	WM_EIS_ASK,
	WM_EIS_ALLOW,
	WM_EIS_DENY,
};

struct wm_config {
	int eis_policy;
	char *eis_allow;
	char *eis_deny;
};

enum wm_menu_item_type {
	WM_MENU_CALLBACK,
};

struct wm_server;

struct wm_menu_item {
	enum wm_menu_item_type type;
	char *label;
	void (*callback)(struct wm_server *server, void *data);
	void *callback_data;
	struct wl_list link;
};

struct wm_menu {
	char *title;
	struct wl_list items;
	void (*on_close)(struct wm_menu *menu, void *data);
	void *on_close_data;
};

struct wm_server {
	struct wl_event_loop *wl_event_loop;
	struct wlr_output_layout *output_layout;
	struct wlr_cursor *cursor;
	struct wlr_xwayland *xwayland;
	struct wm_config *config;
	struct wm_menu *custom_menu;
	struct wm_eis *eis;
	bool locked;
};

static int g_resource_sets;
static char g_resource_value[256];
int
config_set_resource(struct wm_config *config, const char *key,
	const char *value)
{
	(void)config;
	g_resource_sets++;
	snprintf(g_resource_value, sizeof(g_resource_value), "%s=%s", key,
		value);
	return 0;
}

static struct wm_menu *
menu_create(struct wm_server *server, const char *title)
{
	(void)server;
	struct wm_menu *menu = calloc(1, sizeof(*menu));
	assert(menu);
	menu->title = strdup(title);
	wl_list_init(&menu->items);
	return menu;
}

static struct wm_menu_item *
menu_item_create(enum wm_menu_item_type type, const char *label)
{
	struct wm_menu_item *item = calloc(1, sizeof(*item));
	assert(item);
	item->type = type;
	item->label = strdup(label);
	return item;
}

static void wm_menu_hide_all(struct wm_server *server) { (void)server; }
static void wm_menu_show(struct wm_menu *menu, int x, int y)
	{ (void)menu; (void)x; (void)y; }
static bool wm_session_is_locked(struct wm_server *s) { return s->locked; }
static void wm_idle_notify_activity(struct wm_server *s) { (void)s; }
static void wm_input_update_capabilities(struct wm_server *s, bool touch)
	{ (void)s; (void)touch; }
static void wm_keyboard_setup(struct wm_server *s,
	struct wlr_input_device *d, bool is_virtual)
	{ (void)s; (void)d; (void)is_virtual; }

#include "eis.c"

/* --- Tests --- */

static void
test_name_in_list(void)
{
	assert(name_in_list("steam", "steam"));
	assert(name_in_list("xdotool steam", "steam"));
	assert(name_in_list("xdotool,steam", "steam"));
	assert(name_in_list("  xdotool \t steam ", "xdotool"));
	assert(!name_in_list("steam", "stea"));
	assert(!name_in_list("steamer", "steam"));
	assert(!name_in_list("", "steam"));
	assert(!name_in_list(NULL, "steam"));
	assert(!name_in_list("steam", ""));
	assert(!name_in_list("Steam", "steam"));
	printf("  PASS: name_in_list\n");
}

static void
test_cmdline_is_xwayland(void)
{
	static const char line[] =
		"Xwayland\0:1\0-rootless\0-core\0-listenfd\0" "59";
	static const char full[] = "/usr/bin/Xwayland\0:1\0-rootless";
	static const char other[] = "/usr/bin/Xwayland\0:2\0-rootless";
	static const char longer[] = "Xwayland\0:11\0";
	static const char other_program[] = "xdotool\0:1\0";
	static const char no_display[] = "Xwayland";

	assert(cmdline_is_xwayland(line, sizeof(line), ":1"));
	assert(cmdline_is_xwayland(full, sizeof(full), ":1"));
	assert(!cmdline_is_xwayland(other, sizeof(other), ":1"));
	assert(!cmdline_is_xwayland(longer, sizeof(longer), ":1"));
	assert(!cmdline_is_xwayland(other_program, sizeof(other_program),
		":1"));
	assert(!cmdline_is_xwayland(no_display, sizeof(no_display), ":1"));
	assert(!cmdline_is_xwayland(line, sizeof(line), ""));
	assert(!cmdline_is_xwayland(line, sizeof(line), NULL));
	assert(!cmdline_is_xwayland(line, 0, ":1"));
	printf("  PASS: cmdline_is_xwayland\n");
}

static void
test_held_update(void)
{
	uint32_t list[EIS_HELD_MAX];
	int count = 0;

	held_update(list, &count, 272, true);
	held_update(list, &count, 273, true);
	held_update(list, &count, 272, true);	/* again: still one */
	assert(count == 2);
	held_update(list, &count, 272, false);
	assert(count == 1 && list[0] == 273);
	held_update(list, &count, 999, false);	/* never down: nothing */
	assert(count == 1);
	held_update(list, &count, 273, false);
	assert(count == 0);

	for (uint32_t i = 0; i < EIS_HELD_MAX + 5; i++)
		held_update(list, &count, 100 + i, true);
	assert(count == EIS_HELD_MAX);	/* full: the rest are not kept */
	printf("  PASS: held_update\n");
}

static void
test_decisions(void)
{
	struct wm_eis eis = { 0 };
	wl_list_init(&eis.decisions);

	assert(!decision_find(&eis, "steam"));
	decision_set(&eis, "steam", true);
	assert(decision_find(&eis, "steam")->allowed);
	decision_set(&eis, "steam", false);	/* the latest answer */
	assert(!decision_find(&eis, "steam")->allowed);
	decision_set(&eis, "xdotool", true);
	assert(decision_find(&eis, "xdotool")->allowed);
	assert(!decision_find(&eis, "steam")->allowed);

	struct wm_eis_decision *d, *tmp;
	wl_list_for_each_safe(d, tmp, &eis.decisions, link) {
		wl_list_remove(&d->link);
		free(d->name);
		free(d);
	}
	printf("  PASS: decisions\n");
}

/* --- What arrives as input --- */

static struct wlr_pointer_motion_event g_motion;
static int g_motion_count;
static struct wlr_pointer_button_event g_button;
static int g_button_count;
static struct wlr_pointer_axis_event g_axis[4];
static int g_axis_count;
static int g_frame_count;
static struct wlr_keyboard_key_event g_key;
static int g_key_count;

static void
on_motion(struct wl_listener *l, void *data)
	{ (void)l; g_motion = *(struct wlr_pointer_motion_event *)data;
	g_motion_count++; }
static void
on_button(struct wl_listener *l, void *data)
	{ (void)l; g_button = *(struct wlr_pointer_button_event *)data;
	g_button_count++; }
static void
on_axis(struct wl_listener *l, void *data)
	{ (void)l; if (g_axis_count < 4)
		g_axis[g_axis_count++] = *(struct wlr_pointer_axis_event *)data; }
static void
on_frame(struct wl_listener *l, void *data)
	{ (void)l; (void)data; g_frame_count++; }
static void
on_key(struct wl_listener *l, void *data)
	{ (void)l; g_key = *(struct wlr_keyboard_key_event *)data;
	g_key_count++; }

static void
test_emulated_input(void)
{
	struct wm_eis eis = { 0 };
	wlr_pointer_init(&eis.pointer, &pointer_impl, "test pointer");
	wlr_keyboard_init(&eis.keyboard, &keyboard_impl, "test keyboard");

	struct wl_listener motion = { .notify = on_motion };
	struct wl_listener button = { .notify = on_button };
	struct wl_listener axis = { .notify = on_axis };
	struct wl_listener frame = { .notify = on_frame };
	struct wl_listener key = { .notify = on_key };
	wl_signal_add(&eis.pointer.events.motion, &motion);
	wl_signal_add(&eis.pointer.events.button, &button);
	wl_signal_add(&eis.pointer.events.axis, &axis);
	wl_signal_add(&eis.pointer.events.frame, &frame);
	wl_signal_add(&eis.keyboard.events.key, &key);

	emit_motion(&eis, 1234, 3.5, -2);
	assert(g_motion_count == 1 && g_motion.pointer == &eis.pointer);
	assert(g_motion.time_msec == 1234);
	assert(g_motion.delta_x == 3.5 && g_motion.delta_y == -2);
	assert(g_motion.unaccel_dx == 3.5 && g_motion.unaccel_dy == -2);

	emit_button(&eis, 50, 272, true);
	assert(g_button_count == 1 && g_button.button == 272);
	assert(g_button.state == WL_POINTER_BUTTON_STATE_PRESSED);
	emit_button(&eis, 60, 272, false);
	assert(g_button_count == 2 &&
		g_button.state == WL_POINTER_BUTTON_STATE_RELEASED);

	/* A wheel step of libei (120) is one of Wayland's (15), and up
	 * (negative) stays up */
	emit_axis(&eis, 70, WL_POINTER_AXIS_VERTICAL_SCROLL,
		WL_POINTER_AXIS_SOURCE_WHEEL, -120 / EIS_SCROLL_STEP *
		WAYLAND_SCROLL_STEP, -120);
	assert(g_axis_count == 1);
	assert(g_axis[0].orientation == WL_POINTER_AXIS_VERTICAL_SCROLL);
	assert(g_axis[0].source == WL_POINTER_AXIS_SOURCE_WHEEL);
	assert(g_axis[0].delta == -15.0 && g_axis[0].delta_discrete == -120);

	emit_frame(&eis);
	assert(g_frame_count == 1);

	emit_key(&eis, 80, 30, true);
	assert(g_key_count == 1 && g_key.keycode == 30);
	assert(g_key.state == WL_KEYBOARD_KEY_STATE_PRESSED);
	assert(g_key.update_state);
	emit_key(&eis, 90, 30, false);
	assert(g_key.state == WL_KEYBOARD_KEY_STATE_RELEASED);

	/* A client that goes with things held down lets them go */
	struct wm_eis_client client = { .eis = &eis };
	held_update(client.buttons, &client.button_count, 273, true);
	held_update(client.keys, &client.key_count, 31, true);
	held_update(client.keys, &client.key_count, 32, true);
	g_button_count = g_key_count = g_frame_count = 0;
	release_held(&client, 100);
	assert(g_button_count == 1 && g_button.button == 273 &&
		g_button.state == WL_POINTER_BUTTON_STATE_RELEASED);
	assert(g_frame_count == 1);
	assert(g_key_count == 2 &&
		g_key.state == WL_KEYBOARD_KEY_STATE_RELEASED);
	assert(client.button_count == 0 && client.key_count == 0);

	wl_list_remove(&motion.link);
	wl_list_remove(&button.link);
	wl_list_remove(&axis.link);
	wl_list_remove(&frame.link);
	wl_list_remove(&key.link);
	wlr_keyboard_finish(&eis.keyboard);
	wlr_pointer_finish(&eis.pointer);
	printf("  PASS: emulated_input\n");
}

int
main(void)
{
	printf("test_eis:\n");
	test_name_in_list();
	test_cmdline_is_xwayland();
	test_held_update();
	test_decisions();
	test_emulated_input();
	printf("All EIS tests passed.\n");
	return 0;
}
