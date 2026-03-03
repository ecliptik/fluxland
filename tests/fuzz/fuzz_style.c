/*
 * fuzz_style.c - libFuzzer target for the style parser
 *
 * Writes fuzzer-provided data to a temporary file, feeds it to
 * style_load(), and verifies that the parser doesn't crash.
 * Also exercises the parsing helpers (color, font, texture) directly.
 *
 * Build with: clang -fsanitize=fuzzer,address -o fuzz_style \
 *             fuzz_style.c ../../src/style.c ../../src/rcparser.c \
 *             -I../../include -I../../src
 *
 * Or use the meson build system (if compiler supports -fsanitize=fuzzer).
 */

#define _POSIX_C_SOURCE 200809L

#include "style.h"
#include "rcparser.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	/* Write fuzz input to a temporary file */
	char path[] = "/tmp/fuzz-style-XXXXXX";
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

	/* Load as a full style file */
	struct wm_style *style = style_create();
	if (style) {
		style_load(style, path);

		/* Exercise reload path */
		style_reload(style);

		style_destroy(style);
	}

	/*
	 * Also exercise the individual parsing helpers with a
	 * NUL-terminated portion of the fuzz input (treating
	 * the raw bytes as a string).
	 */
	if (size > 0 && size < 4096) {
		char *str = malloc(size + 1);
		if (str) {
			memcpy(str, data, size);
			str[size] = '\0';

			/* Color parsing */
			style_parse_color(str);
			style_parse_color("");
			style_parse_color("#");
			style_parse_color("rgb:");

			/* Font parsing */
			struct wm_font font = style_parse_font(str);
			free(font.family);

			/* Texture parsing */
			struct wm_texture tex = style_parse_texture(str);
			free(tex.pixmap_path);

			free(str);
		}
	}

	unlink(path);
	return 0;
}
