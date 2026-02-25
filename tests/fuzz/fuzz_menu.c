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
#include "render.h"
#include "view.h"
#include "workspace.h"
#include "foreign_toplevel.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * Stubs for compositor functions referenced in menu.c but not
 * exercised by the parser-only fuzz target.
 */
cairo_surface_t *wm_render_texture(const struct wm_texture *t,
	int w, int h, float s)
	{ (void)t; (void)w; (void)h; (void)s; return NULL; }
cairo_surface_t *wm_render_text(const char *text, const struct wm_font *f,
	const struct wm_color *c, int max_w, int *tw, int *th,
	enum wm_justify j, float s)
	{ (void)text; (void)f; (void)c; (void)max_w; (void)tw; (void)th;
	  (void)j; (void)s; return NULL; }
int wm_measure_text_width(const char *text, const struct wm_font *f,
	float s)
	{ (void)f; (void)s;
	  return text ? (int)(strlen(text) * 7) : 0; }
int style_load(struct wm_style *s, const char *p)
	{ (void)s; (void)p; return 0; }
void wm_view_set_sticky(struct wm_view *v, bool s)
	{ (void)v; (void)s; }
void wm_view_raise(struct wm_view *v)
	{ (void)v; }
void wm_workspace_switch(struct wm_server *s, int i)
	{ (void)s; (void)i; }
struct wm_workspace *wm_workspace_get_active(struct wm_server *s)
	{ (void)s; return NULL; }
void wm_foreign_toplevel_set_minimized(struct wm_view *v, bool m)
	{ (void)v; (void)m; }
void wm_focus_view(struct wm_view *v, struct wlr_surface *s)
	{ (void)v; (void)s; }
int wm_rules_remember_window(struct wm_view *v, const char *p)
	{ (void)v; (void)p; return 0; }

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
