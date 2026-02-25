/*
 * fuzz_keybind.c - libFuzzer target for the keybinding parser
 *
 * Writes fuzzer-provided data to a temporary file, feeds it to
 * keybind_load(), and verifies that the parser doesn't crash.
 *
 * Build with: clang -fsanitize=fuzzer,address -o fuzz_keybind \
 *             fuzz_keybind.c ../../src/keybind.c \
 *             -I../../include -I../../src $(pkg-config --cflags --libs \
 *             wayland-server wlroots-0.18 xkbcommon)
 *
 * Or use the meson build system (if compiler supports -fsanitize=fuzzer).
 */

#define _POSIX_C_SOURCE 200809L

#include "keybind.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	/* Write fuzz input to a temporary file */
	char path[] = "/tmp/fuzz-keybind-XXXXXX";
	int fd = mkstemp(path);
	if (fd < 0)
		return 0;

	size_t written = 0;
	while (written < size) {
		ssize_t n = write(fd, data + written, size - written);
		if (n <= 0)
			break;
		written += (size_t)n;
	}
	close(fd);

	/* Parse the file */
	struct wl_list keymodes;
	wl_list_init(&keymodes);

	keybind_load(&keymodes, path);

	/* Exercise lookup on the parsed data */
	struct wm_keymode *mode;
	wl_list_for_each(mode, &keymodes, link) {
		/* Try to find some common keysyms */
		keybind_find(&mode->bindings, 0, XKB_KEY_Return);
		keybind_find(&mode->bindings, WLR_MODIFIER_LOGO, XKB_KEY_q);
	}

	keybind_destroy_all(&keymodes);

	unlink(path);
	return 0;
}
