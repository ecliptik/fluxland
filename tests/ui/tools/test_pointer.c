/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * test_pointer.c - fluxland-test-pointer: a virtual pointer for the UI
 * tests that holds buttons across moves (wlrctl only clicks)
 *
 * Usage: fluxland-test-pointer ACTION...
 *   move X Y     to (X, Y) in the layout: the top left corner, then there
 *   to X Y       to (X, Y) straight from where the last move left it
 *   press N      press button N (1 left, 2 middle, 3 right)
 *   release N
 *   sleep MS
 */

#define _POSIX_C_SOURCE 200809L

#include <linux/input-event-codes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wayland-client.h>
#include "wlr-virtual-pointer-unstable-v1-client-protocol.h"

static struct zwlr_virtual_pointer_manager_v1 *manager;
static struct wl_seat *seat;

static void
global_add(void *data, struct wl_registry *registry, uint32_t name,
	const char *interface, uint32_t version)
{
	(void)data; (void)version;
	if (strcmp(interface, zwlr_virtual_pointer_manager_v1_interface.name)
	    == 0)
		manager = wl_registry_bind(registry, name,
			&zwlr_virtual_pointer_manager_v1_interface, 1);
	else if (strcmp(interface, wl_seat_interface.name) == 0 && !seat)
		seat = wl_registry_bind(registry, name, &wl_seat_interface, 1);
}

static void
global_remove(void *data, struct wl_registry *registry, uint32_t name)
{
	(void)data; (void)registry; (void)name;
}

static const struct wl_registry_listener registry_listener = {
	.global = global_add,
	.global_remove = global_remove,
};

static uint32_t
now_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static void
motion(struct zwlr_virtual_pointer_v1 *ptr, double dx, double dy)
{
	zwlr_virtual_pointer_v1_motion(ptr, now_ms(),
		wl_fixed_from_double(dx), wl_fixed_from_double(dy));
	zwlr_virtual_pointer_v1_frame(ptr);
}

static int
button_code(const char *arg)
{
	int n = atoi(arg);
	return n == 1 ? BTN_LEFT : n == 2 ? BTN_MIDDLE : n == 3 ? BTN_RIGHT :
		n == 8 ? BTN_SIDE : n == 9 ? BTN_EXTRA : -1;
}

int
main(int argc, char *argv[])
{
	struct wl_display *display = wl_display_connect(NULL);
	if (!display) {
		fprintf(stderr, "cannot connect to the compositor\n");
		return 1;
	}
	struct wl_registry *registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	wl_display_roundtrip(display);
	if (!manager) {
		fprintf(stderr, "no zwlr_virtual_pointer_manager_v1\n");
		return 1;
	}
	struct zwlr_virtual_pointer_v1 *ptr =
		zwlr_virtual_pointer_manager_v1_create_virtual_pointer(manager,
			seat);

	/* Where the pointer is, from the top left corner of the layout */
	double x = 0, y = 0;
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "move") == 0 && i + 2 < argc) {
			/* The cursor stops at the layout's edge */
			motion(ptr, -100000, -100000);
			wl_display_roundtrip(display);
			x = atof(argv[i + 1]);
			y = atof(argv[i + 2]);
			motion(ptr, x, y);
			i += 2;
		} else if (strcmp(argv[i], "to") == 0 && i + 2 < argc) {
			/* A button held: not by the corner */
			double nx = atof(argv[i + 1]), ny = atof(argv[i + 2]);
			motion(ptr, nx - x, ny - y);
			x = nx;
			y = ny;
			i += 2;
		} else if ((strcmp(argv[i], "press") == 0 ||
			    strcmp(argv[i], "release") == 0) && i + 1 < argc) {
			int code = button_code(argv[i + 1]);
			if (code < 0) {
				fprintf(stderr, "unknown button %s\n",
					argv[i + 1]);
				return 1;
			}
			zwlr_virtual_pointer_v1_button(ptr, now_ms(), code,
				argv[i][0] == 'p' ?
				WL_POINTER_BUTTON_STATE_PRESSED :
				WL_POINTER_BUTTON_STATE_RELEASED);
			zwlr_virtual_pointer_v1_frame(ptr);
			i += 1;
		} else if (strcmp(argv[i], "sleep") == 0 && i + 1 < argc) {
			wl_display_roundtrip(display);
			struct timespec ts = { 0, atol(argv[i + 1]) * 1000000L };
			while (ts.tv_nsec >= 1000000000L) {
				ts.tv_sec++;
				ts.tv_nsec -= 1000000000L;
			}
			nanosleep(&ts, NULL);
			i += 1;
		} else {
			fprintf(stderr, "usage: %s [move X Y | to X Y | "
				"press N | release N | sleep MS]...\n", argv[0]);
			return 1;
		}
		wl_display_roundtrip(display);
	}

	zwlr_virtual_pointer_v1_destroy(ptr);
	wl_display_roundtrip(display);
	wl_display_disconnect(display);
	return 0;
}
