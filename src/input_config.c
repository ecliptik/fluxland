/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * input_config.c - Apply session.input.* settings to libinput devices
 *
 * Rules apply from general to specific: every device, then the device
 * type (touchpad or other pointer), then the named device. Settings a
 * device doesn't support are skipped.
 */

#include <ctype.h>
#include <libinput.h>
#include <math.h>
#include <string.h>

#include "config.h"
#include "input_config.h"

void
wm_input_device_key(const char *name, char *out, size_t len)
{
	size_t i = 0;
	for (; name && name[i] && i + 1 < len; i++) {
		unsigned char c = (unsigned char)name[i];
		out[i] = isalnum(c) ? (char)c : '_';
	}
	out[i] = '\0';
}

static enum libinput_config_tap_state
tap_state(int on)
{
	return on ? LIBINPUT_CONFIG_TAP_ENABLED : LIBINPUT_CONFIG_TAP_DISABLED;
}

static void
apply_rule(struct libinput_device *d, const struct wm_input_rule *r)
{
	if (libinput_device_config_tap_get_finger_count(d) > 0) {
		if (r->tap >= 0)
			libinput_device_config_tap_set_enabled(d,
				tap_state(r->tap));
		if (r->tap_drag >= 0)
			libinput_device_config_tap_set_drag_enabled(d,
				r->tap_drag ? LIBINPUT_CONFIG_DRAG_ENABLED :
				LIBINPUT_CONFIG_DRAG_DISABLED);
		if (r->tap_drag_lock >= 0)
			libinput_device_config_tap_set_drag_lock_enabled(d,
				r->tap_drag_lock ?
				LIBINPUT_CONFIG_DRAG_LOCK_ENABLED :
				LIBINPUT_CONFIG_DRAG_LOCK_DISABLED);
		if (r->tap_button_map >= 0)
			libinput_device_config_tap_set_button_map(d,
				r->tap_button_map ? LIBINPUT_CONFIG_TAP_MAP_LMR :
				LIBINPUT_CONFIG_TAP_MAP_LRM);
	}

	if (r->natural_scroll >= 0 &&
	    libinput_device_config_scroll_has_natural_scroll(d))
		libinput_device_config_scroll_set_natural_scroll_enabled(d,
			r->natural_scroll);

	if (r->left_handed >= 0 &&
	    libinput_device_config_left_handed_is_available(d))
		libinput_device_config_left_handed_set(d, r->left_handed);

	if (r->disable_while_typing >= 0 &&
	    libinput_device_config_dwt_is_available(d))
		libinput_device_config_dwt_set_enabled(d,
			r->disable_while_typing ? LIBINPUT_CONFIG_DWT_ENABLED :
			LIBINPUT_CONFIG_DWT_DISABLED);

	if (r->middle_emulation >= 0 &&
	    libinput_device_config_middle_emulation_is_available(d))
		libinput_device_config_middle_emulation_set_enabled(d,
			r->middle_emulation ?
			LIBINPUT_CONFIG_MIDDLE_EMULATION_ENABLED :
			LIBINPUT_CONFIG_MIDDLE_EMULATION_DISABLED);

	if (libinput_device_config_accel_is_available(d)) {
		if (r->accel_profile >= 0)
			libinput_device_config_accel_set_profile(d,
				r->accel_profile ?
				LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE :
				LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT);
		if (!isnan(r->accel_speed))
			libinput_device_config_accel_set_speed(d,
				r->accel_speed);
	}

	if (r->scroll_method >= 0) {
		static const enum libinput_config_scroll_method methods[] = {
			LIBINPUT_CONFIG_SCROLL_NO_SCROLL,
			LIBINPUT_CONFIG_SCROLL_2FG,
			LIBINPUT_CONFIG_SCROLL_EDGE,
			LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN,
		};
		enum libinput_config_scroll_method m = methods[r->scroll_method];
		uint32_t supported =
			libinput_device_config_scroll_get_methods(d);
		if (m == LIBINPUT_CONFIG_SCROLL_NO_SCROLL || (supported & m))
			libinput_device_config_scroll_set_method(d, m);
	}
	if (r->scroll_button > 0 &&
	    (libinput_device_config_scroll_get_methods(d) &
	     LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN))
		libinput_device_config_scroll_set_button(d,
			(uint32_t)r->scroll_button);

	if (r->click_method >= 0) {
		static const enum libinput_config_click_method methods[] = {
			LIBINPUT_CONFIG_CLICK_METHOD_NONE,
			LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS,
			LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER,
		};
		enum libinput_config_click_method m = methods[r->click_method];
		uint32_t supported =
			libinput_device_config_click_get_methods(d);
		if (m == LIBINPUT_CONFIG_CLICK_METHOD_NONE || (supported & m))
			libinput_device_config_click_set_method(d, m);
	}

	if (r->send_events >= 0) {
		static const uint32_t modes[] = {
			LIBINPUT_CONFIG_SEND_EVENTS_ENABLED,
			LIBINPUT_CONFIG_SEND_EVENTS_DISABLED,
			LIBINPUT_CONFIG_SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE,
		};
		uint32_t mode = modes[r->send_events];
		if (mode == LIBINPUT_CONFIG_SEND_EVENTS_ENABLED ||
		    (libinput_device_config_send_events_get_modes(d) & mode))
			libinput_device_config_send_events_set_mode(d, mode);
	}
}

bool
wm_input_device_is_touchpad(struct libinput_device *d)
{
	return libinput_device_config_tap_get_finger_count(d) > 0;
}

void
wm_input_config_apply(struct libinput_device *d, const char *name,
	const struct wm_input_rule *rules, int count)
{
	char key[256];
	wm_input_device_key(name, key, sizeof(key));
	bool touchpad = wm_input_device_is_touchpad(d);

	/* General to specific, so the most specific setting wins */
	for (int i = 0; i < count; i++) {
		if (rules[i].scope == WM_INPUT_SCOPE_ALL)
			apply_rule(d, &rules[i]);
	}
	for (int i = 0; i < count; i++) {
		if ((rules[i].scope == WM_INPUT_SCOPE_TOUCHPAD && touchpad) ||
		    (rules[i].scope == WM_INPUT_SCOPE_POINTER && !touchpad))
			apply_rule(d, &rules[i]);
	}
	for (int i = 0; i < count; i++) {
		if (rules[i].scope == WM_INPUT_SCOPE_DEVICE &&
		    rules[i].device && strcmp(rules[i].device, key) == 0)
			apply_rule(d, &rules[i]);
	}
}
