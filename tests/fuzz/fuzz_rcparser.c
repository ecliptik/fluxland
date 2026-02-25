/*
 * fuzz_rcparser.c - libFuzzer target for the rcparser config parser
 *
 * Writes fuzzer-provided data to a temporary file, feeds it to
 * rc_load(), and verifies that the parser doesn't crash.
 *
 * Build with: clang -fsanitize=fuzzer,address -o fuzz_rcparser \
 *             fuzz_rcparser.c ../../src/rcparser.c -I../../include -I../../src
 *
 * Or use the meson build system (if compiler supports -fsanitize=fuzzer).
 */

#define _POSIX_C_SOURCE 200809L

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
	char path[] = "/tmp/fuzz-rcparser-XXXXXX";
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

	/* Parse the file */
	struct rc_database *db = rc_create();
	if (db) {
		rc_load(db, path);

		/* Exercise lookup functions with various keys */
		rc_get_string(db, "session.screen0.workspaces");
		rc_get_int(db, "session.screen0.workspaces", 4);
		rc_get_bool(db, "session.screen0.autoRaise", false);

		/* Try to look up keys that might exist in fuzz data */
		rc_get_string(db, "");
		rc_get_string(db, "a");
		rc_get_int(db, "nonexistent", 0);
		rc_get_bool(db, "nonexistent", false);

		rc_destroy(db);
	}

	unlink(path);
	return 0;
}
