/*
 * fuzz_mousebind.c - libFuzzer target for the mouse binding parser
 *
 * SPDX-License-Identifier: MIT
 *
 * Writes fuzzer-provided data to a temporary file, feeds it to
 * mousebind_load(), and verifies that the parser doesn't crash.
 * The parser handles Fluxbox-format mouse bindings including context,
 * modifier, and action parsing with MacroCmd/ToggleCmd sub-commands.
 *
 * Build with: clang -fsanitize=fuzzer,address -o fuzz_mousebind \
 *             fuzz_mousebind.c ../../src/mousebind.c \
 *             -I../../include -I../../src \
 *             $(pkg-config --cflags --libs wayland-server wlroots-0.18 \
 *               xkbcommon)
 *
 * Or use the meson build system (if compiler supports -fsanitize=fuzzer).
 */

#define _POSIX_C_SOURCE 200809L

#include "mousebind.h"

#include <linux/input-event-codes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	/* Write fuzz input to a temporary file */
	char path[] = "/tmp/fuzz-mousebind-XXXXXX";
	int fd = mkstemp(path);
	if (fd < 0)
		return 0;

	/* Write data in chunks to handle large inputs */
	size_t written = 0;
	while (written < size) {
		ssize_t n = write(fd, data + written, size - written);
		if (n <= 0)
			break;
		written += (size_t)n;
	}
	close(fd);

	/* Parse with default button mapping */
	struct wl_list bindings;
	wl_list_init(&bindings);

	mousebind_load(&bindings, path, NULL);

	/* Exercise lookup on the parsed data */
	mousebind_find(&bindings, WM_MOUSE_CTX_DESKTOP,
		WM_MOUSE_PRESS, BTN_LEFT, 0);
	mousebind_find(&bindings, WM_MOUSE_CTX_TITLEBAR,
		WM_MOUSE_CLICK, BTN_RIGHT, WLR_MODIFIER_LOGO);
	mousebind_find(&bindings, WM_MOUSE_CTX_NONE,
		WM_MOUSE_MOVE, BTN_LEFT, WLR_MODIFIER_ALT);

	mousebind_destroy_list(&bindings);

	/* Also test with a custom button mapping */
	uint32_t button_map[6] = {
		0,
		BTN_LEFT,
		BTN_MIDDLE,
		BTN_RIGHT,
		BTN_SIDE,
		BTN_EXTRA,
	};

	wl_list_init(&bindings);
	mousebind_load(&bindings, path, button_map);
	mousebind_destroy_list(&bindings);

	unlink(path);
	return 0;
}
