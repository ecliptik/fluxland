/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * input_config.h - Apply session.input.* settings to libinput devices
 */

#ifndef WM_INPUT_CONFIG_H
#define WM_INPUT_CONFIG_H

#include <stdbool.h>
#include <stddef.h>

struct libinput_device;
struct wm_input_rule;

/*
 * The name used in session.input.device.<Name>.*: the device name with
 * every non-alphanumeric character replaced by '_'.
 */
void wm_input_device_key(const char *name, char *out, size_t len);

/* Touchpads are devices that support tapping */
bool wm_input_device_is_touchpad(struct libinput_device *d);

/* Apply every rule that matches this device, general to specific */
void wm_input_config_apply(struct libinput_device *d, const char *name,
	const struct wm_input_rule *rules, int count);

#endif /* WM_INPUT_CONFIG_H */
