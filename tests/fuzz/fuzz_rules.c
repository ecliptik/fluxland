/*
 * fuzz_rules.c - libFuzzer target for the window rules (apps file) parser
 *
 * SPDX-License-Identifier: MIT
 *
 * Writes fuzzer-provided data to a temporary file, feeds it to
 * wm_rules_load(), and verifies that the parser doesn't crash.
 * The apps file parser processes per-window rules including regex
 * patterns, which makes it a valuable fuzz target.
 *
 * Build with: clang -fsanitize=fuzzer,address -o fuzz_rules \
 *             fuzz_rules.c ../../src/rules.c \
 *             -I../../include -I../../src \
 *             $(pkg-config --cflags --libs wayland-server wlroots-0.18)
 *
 * Or use the meson build system (if compiler supports -fsanitize=fuzzer).
 */

#define _GNU_SOURCE

#include "rules.h"
#include "workspace.h"
#include "view.h"
#include "decoration.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * Stubs for compositor functions referenced in rules.c but not
 * exercised by the parser-only fuzz target.
 */
void wm_view_set_sticky(struct wm_view *v, bool s)
	{ (void)v; (void)s; }
void wm_view_get_geometry(struct wm_view *v, struct wlr_box *b)
	{ (void)v; if (b) { b->x = 0; b->y = 0; b->width = 0; b->height = 0; } }
void wm_view_move_to_workspace(struct wm_view *v, struct wm_workspace *w)
	{ (void)v; (void)w; }
struct wm_workspace *wm_workspace_get(struct wm_server *s, int i)
	{ (void)s; (void)i; return NULL; }
void wm_decoration_set_preset(struct wm_decoration *d,
	enum wm_decor_preset p, struct wm_style *s)
	{ (void)d; (void)p; (void)s; }
void wm_decoration_set_shaded(struct wm_decoration *d,
	bool s, struct wm_style *st)
	{ (void)d; (void)s; (void)st; }

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	/* Write fuzz input to a temporary file */
	char path[] = "/tmp/fuzz-rules-XXXXXX";
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

	/* Parse the apps file */
	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, path);
	wm_rules_finish(&rules);

	/* Parse again to test re-entrancy / reload path */
	wm_rules_init(&rules);
	wm_rules_load(&rules, path);
	wm_rules_finish(&rules);

	unlink(path);
	return 0;
}
