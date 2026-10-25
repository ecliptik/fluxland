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
 * Built by meson when the compiler supports -fsanitize=fuzzer (Clang).
 */

#define _GNU_SOURCE

#include "rules.h"
#include "workspace.h"
#include "view.h"
#include "decoration.h"
#include "foreign_toplevel.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* What client_pattern.c's matching (linked, not exercised) refers to */
struct wm_view *wm_view_parent(struct wm_view *v) { (void)v; return NULL; }
void wm_view_get_geometry(struct wm_view *v, struct wlr_box *b)
	{ (void)v; if (b) { b->x = 0; b->y = 0; b->width = 0; b->height = 0; } }
struct wm_workspace *wm_workspace_get_active(struct wm_server *s)
	{ (void)s; return NULL; }

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
