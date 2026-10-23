/*
 * fuzz_menu.c - libFuzzer target for the menu file parser
 *
 * Writes fuzzer-provided data to a temporary file, feeds it to
 * wm_menu_load(), and verifies that the parser doesn't crash.
 *
 * Since wm_menu_load() requires a wm_server pointer (used for
 * dynamically-created menus like workspaces), we pass NULL which
 * the parser handles gracefully for basic menu file parsing.
 *
 * Build with: clang -fsanitize=fuzzer,address -o fuzz_menu \
 *             fuzz_menu.c ../../src/menu.c \
 *             -I../../include -I../../src \
 *             $(pkg-config --cflags --libs wayland-server wlroots-0.18 \
 *               xkbcommon pangocairo libdrm)
 *
 * Or use the meson build system (if compiler supports -fsanitize=fuzzer).
 */

#define _GNU_SOURCE

#include "menu.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	/* Write fuzz input to a temporary file */
	char path[] = "/tmp/fuzz-menu-XXXXXX";
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

	/* Parse the menu file (NULL server is safe for basic parsing) */
	struct wm_menu *menu = wm_menu_load(NULL, path);
	if (menu) {
		wm_menu_destroy(menu);
	}

	unlink(path);
	return 0;
}
