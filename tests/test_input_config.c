/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * test_input_config.c - session.input.* rules applied to libinput devices
 *
 * libinput is replaced by a fake device that records what was set and
 * advertises a configurable set of capabilities.
 */

#include <assert.h>
#include <libinput.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "input_config.h"

struct libinput_device {
	/* capabilities */
	int tap_fingers;
	bool natural, left_handed_ok, dwt_ok, middle_ok, accel_ok;
	uint32_t scroll_methods, click_methods, send_modes;
	/* what was set (-1 = untouched) */
	int tap, drag, drag_lock, tap_map;
	int natural_set, left_handed, dwt, middle;
	int profile;
	double speed;
	int scroll_method, scroll_button, click_method;
	int send_mode;
};

static void
fake_init(struct libinput_device *d, bool touchpad)
{
	memset(d, 0, sizeof(*d));
	d->tap_fingers = touchpad ? 3 : 0;
	d->natural = d->left_handed_ok = d->accel_ok = true;
	d->dwt_ok = touchpad;
	d->middle_ok = !touchpad;
	d->scroll_methods = touchpad ?
		(LIBINPUT_CONFIG_SCROLL_2FG | LIBINPUT_CONFIG_SCROLL_EDGE) :
		LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN;
	d->click_methods = touchpad ?
		(LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS |
		 LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER) : 0;
	d->send_modes = LIBINPUT_CONFIG_SEND_EVENTS_DISABLED |
		(touchpad ? LIBINPUT_CONFIG_SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE :
		 0);
	d->tap = d->drag = d->drag_lock = d->tap_map = -1;
	d->natural_set = d->left_handed = d->dwt = d->middle = -1;
	d->profile = -1;
	d->speed = NAN;
	d->scroll_method = d->scroll_button = d->click_method = -1;
	d->send_mode = -1;
}

/* --- libinput stand-ins --- */

int
libinput_device_config_tap_get_finger_count(struct libinput_device *d)
{
	return d->tap_fingers;
}

enum libinput_config_status
libinput_device_config_tap_set_enabled(struct libinput_device *d,
	enum libinput_config_tap_state s)
{
	d->tap = s == LIBINPUT_CONFIG_TAP_ENABLED;
	return LIBINPUT_CONFIG_STATUS_SUCCESS;
}

enum libinput_config_status
libinput_device_config_tap_set_drag_enabled(struct libinput_device *d,
	enum libinput_config_drag_state s)
{
	d->drag = s == LIBINPUT_CONFIG_DRAG_ENABLED;
	return LIBINPUT_CONFIG_STATUS_SUCCESS;
}

enum libinput_config_status
libinput_device_config_tap_set_drag_lock_enabled(struct libinput_device *d,
	enum libinput_config_drag_lock_state s)
{
	d->drag_lock = s == LIBINPUT_CONFIG_DRAG_LOCK_ENABLED;
	return LIBINPUT_CONFIG_STATUS_SUCCESS;
}

enum libinput_config_status
libinput_device_config_tap_set_button_map(struct libinput_device *d,
	enum libinput_config_tap_button_map m)
{
	d->tap_map = m == LIBINPUT_CONFIG_TAP_MAP_LMR;
	return LIBINPUT_CONFIG_STATUS_SUCCESS;
}

int
libinput_device_config_scroll_has_natural_scroll(struct libinput_device *d)
{
	return d->natural;
}

enum libinput_config_status
libinput_device_config_scroll_set_natural_scroll_enabled(
	struct libinput_device *d, int on)
{
	d->natural_set = on;
	return LIBINPUT_CONFIG_STATUS_SUCCESS;
}

int
libinput_device_config_left_handed_is_available(struct libinput_device *d)
{
	return d->left_handed_ok;
}

enum libinput_config_status
libinput_device_config_left_handed_set(struct libinput_device *d, int on)
{
	d->left_handed = on;
	return LIBINPUT_CONFIG_STATUS_SUCCESS;
}

int
libinput_device_config_dwt_is_available(struct libinput_device *d)
{
	return d->dwt_ok;
}

enum libinput_config_status
libinput_device_config_dwt_set_enabled(struct libinput_device *d,
	enum libinput_config_dwt_state s)
{
	d->dwt = s == LIBINPUT_CONFIG_DWT_ENABLED;
	return LIBINPUT_CONFIG_STATUS_SUCCESS;
}

int
libinput_device_config_middle_emulation_is_available(
	struct libinput_device *d)
{
	return d->middle_ok;
}

enum libinput_config_status
libinput_device_config_middle_emulation_set_enabled(
	struct libinput_device *d,
	enum libinput_config_middle_emulation_state s)
{
	d->middle = s == LIBINPUT_CONFIG_MIDDLE_EMULATION_ENABLED;
	return LIBINPUT_CONFIG_STATUS_SUCCESS;
}

int
libinput_device_config_accel_is_available(struct libinput_device *d)
{
	return d->accel_ok;
}

enum libinput_config_status
libinput_device_config_accel_set_profile(struct libinput_device *d,
	enum libinput_config_accel_profile p)
{
	d->profile = p == LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE;
	return LIBINPUT_CONFIG_STATUS_SUCCESS;
}

enum libinput_config_status
libinput_device_config_accel_set_speed(struct libinput_device *d,
	double speed)
{
	d->speed = speed;
	return LIBINPUT_CONFIG_STATUS_SUCCESS;
}

uint32_t
libinput_device_config_scroll_get_methods(struct libinput_device *d)
{
	return d->scroll_methods;
}

enum libinput_config_status
libinput_device_config_scroll_set_method(struct libinput_device *d,
	enum libinput_config_scroll_method m)
{
	d->scroll_method = (int)m;
	return LIBINPUT_CONFIG_STATUS_SUCCESS;
}

enum libinput_config_status
libinput_device_config_scroll_set_button(struct libinput_device *d,
	uint32_t button)
{
	d->scroll_button = (int)button;
	return LIBINPUT_CONFIG_STATUS_SUCCESS;
}

uint32_t
libinput_device_config_click_get_methods(struct libinput_device *d)
{
	return d->click_methods;
}

enum libinput_config_status
libinput_device_config_click_set_method(struct libinput_device *d,
	enum libinput_config_click_method m)
{
	d->click_method = (int)m;
	return LIBINPUT_CONFIG_STATUS_SUCCESS;
}

uint32_t
libinput_device_config_send_events_get_modes(struct libinput_device *d)
{
	return d->send_modes;
}

enum libinput_config_status
libinput_device_config_send_events_set_mode(struct libinput_device *d,
	uint32_t mode)
{
	d->send_mode = (int)mode;
	return LIBINPUT_CONFIG_STATUS_SUCCESS;
}

/* --- helpers --- */

static struct wm_input_rule
rule(enum wm_input_scope scope, char *device)
{
	struct wm_input_rule r;
	memset(&r, 0, sizeof(r));
	r.scope = scope;
	r.device = device;
	r.tap = r.tap_drag = r.tap_drag_lock = r.tap_button_map = -1;
	r.natural_scroll = r.left_handed = -1;
	r.disable_while_typing = r.middle_emulation = -1;
	r.accel_profile = -1;
	r.accel_speed = NAN;
	r.scroll_method = r.scroll_button = r.click_method = -1;
	r.send_events = -1;
	return r;
}

/* --- tests --- */

static void
test_device_key(void)
{
	char key[32];
	wm_input_device_key("SynPS/2 Synaptics TouchPad", key, sizeof(key));
	assert(strcmp(key, "SynPS_2_Synaptics_TouchPad") == 0);
	wm_input_device_key("a-very-long-device-name-that-is-cut", key, 8);
	assert(strcmp(key, "a_very_") == 0);
	wm_input_device_key(NULL, key, sizeof(key));
	assert(key[0] == '\0');
	printf("  PASS: device_key\n");
}

static void
test_nothing_set_touches_nothing(void)
{
	struct libinput_device d;
	fake_init(&d, true);
	struct wm_input_rule r = rule(WM_INPUT_SCOPE_ALL, NULL);
	wm_input_config_apply(&d, "pad", &r, 1);
	assert(d.tap == -1 && d.natural_set == -1 && d.profile == -1);
	assert(isnan(d.speed) && d.scroll_method == -1);
	assert(d.click_method == -1 && d.send_mode == -1);
	printf("  PASS: nothing_set_touches_nothing\n");
}

static void
test_scopes_by_device_type(void)
{
	struct wm_input_rule rules[3];
	rules[0] = rule(WM_INPUT_SCOPE_ALL, NULL);
	rules[0].natural_scroll = 1;
	rules[1] = rule(WM_INPUT_SCOPE_TOUCHPAD, NULL);
	rules[1].tap = 1;
	rules[1].tap_button_map = 1;
	rules[1].click_method = 2;
	rules[1].disable_while_typing = 0;
	rules[2] = rule(WM_INPUT_SCOPE_POINTER, NULL);
	rules[2].accel_profile = 0;
	rules[2].accel_speed = 0.25;
	rules[2].middle_emulation = 1;

	struct libinput_device pad, mouse;
	fake_init(&pad, true);
	fake_init(&mouse, false);
	wm_input_config_apply(&pad, "pad", rules, 3);
	wm_input_config_apply(&mouse, "mouse", rules, 3);

	assert(pad.natural_set == 1 && mouse.natural_set == 1);
	assert(pad.tap == 1 && pad.tap_map == 1);
	assert(pad.click_method == LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER);
	assert(pad.dwt == 0);
	assert(pad.profile == -1 && isnan(pad.speed) && pad.middle == -1);
	assert(mouse.tap == -1 && mouse.click_method == -1);
	assert(mouse.profile == 0 && mouse.speed == 0.25 && mouse.middle == 1);
	printf("  PASS: scopes_by_device_type\n");
}

static void
test_specific_wins(void)
{
	char name[] = "Logitech_USB_Trackball";
	struct wm_input_rule rules[3];
	/* Device rule first in the file; it still wins */
	rules[0] = rule(WM_INPUT_SCOPE_DEVICE, name);
	rules[0].natural_scroll = 0;
	rules[0].scroll_method = 3;
	rules[0].scroll_button = 276;
	rules[1] = rule(WM_INPUT_SCOPE_POINTER, NULL);
	rules[1].natural_scroll = 1;
	rules[2] = rule(WM_INPUT_SCOPE_ALL, NULL);
	rules[2].left_handed = 1;

	struct libinput_device ball, other;
	fake_init(&ball, false);
	fake_init(&other, false);
	wm_input_config_apply(&ball, "Logitech USB Trackball", rules, 3);
	wm_input_config_apply(&other, "Generic Mouse", rules, 3);

	assert(ball.natural_set == 0 && other.natural_set == 1);
	assert(ball.scroll_method == LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN);
	assert(ball.scroll_button == 276);
	assert(other.scroll_method == -1 && other.scroll_button == -1);
	assert(ball.left_handed == 1 && other.left_handed == 1);
	printf("  PASS: specific_wins\n");
}

static void
test_unsupported_settings_skipped(void)
{
	struct wm_input_rule r = rule(WM_INPUT_SCOPE_ALL, NULL);
	r.tap = 1;
	r.scroll_method = 2;   /* edge: touchpad only here */
	r.scroll_button = 274; /* needs button scrolling */
	r.click_method = 1;
	r.send_events = 2;     /* disabledOnExternalMouse: touchpad only */
	r.disable_while_typing = 1;

	struct libinput_device mouse;
	fake_init(&mouse, false);
	mouse.accel_ok = false;
	r.accel_speed = 0.5;
	wm_input_config_apply(&mouse, "mouse", &r, 1);
	assert(mouse.tap == -1);
	assert(mouse.scroll_method == -1);
	assert(mouse.scroll_button == 274); /* mouse supports button scroll */
	assert(mouse.click_method == -1);
	assert(mouse.send_mode == -1);
	assert(mouse.dwt == -1);
	assert(isnan(mouse.speed));

	struct libinput_device pad;
	fake_init(&pad, true);
	wm_input_config_apply(&pad, "pad", &r, 1);
	assert(pad.tap == 1);
	assert(pad.scroll_method == LIBINPUT_CONFIG_SCROLL_EDGE);
	assert(pad.scroll_button == -1);
	assert(pad.click_method ==
		LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS);
	assert(pad.send_mode ==
		LIBINPUT_CONFIG_SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE);

	/* "none" and "enabled" are always accepted */
	r = rule(WM_INPUT_SCOPE_ALL, NULL);
	r.scroll_method = 0;
	r.click_method = 0;
	r.send_events = 0;
	fake_init(&mouse, false);
	wm_input_config_apply(&mouse, "mouse", &r, 1);
	assert(mouse.scroll_method == LIBINPUT_CONFIG_SCROLL_NO_SCROLL);
	assert(mouse.click_method == LIBINPUT_CONFIG_CLICK_METHOD_NONE);
	assert(mouse.send_mode == LIBINPUT_CONFIG_SEND_EVENTS_ENABLED);
	printf("  PASS: unsupported_settings_skipped\n");
}

int
main(void)
{
	printf("input_config tests:\n");
	test_device_key();
	test_nothing_set_touches_nothing();
	test_scopes_by_device_type();
	test_specific_wins();
	test_unsupported_settings_skipped();
	printf("All input_config tests passed.\n");
	return 0;
}
