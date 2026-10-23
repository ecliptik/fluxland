/*
 * fuzz_ipc_command.c - libFuzzer target for IPC JSON command parsing
 *
 * SPDX-License-Identifier: MIT
 *
 * Exercises the JSON parsing functions used to process incoming IPC
 * requests over the Unix socket.  These are the primary network-facing
 * attack surface: an attacker with local socket access can send
 * arbitrary bytes.
 *
 * The wm_ipc_json_get_string() and wm_ipc_json_quote() helpers are extracted from
 * ipc_commands.c to enable focused fuzzing without compositor
 * dependencies.
 *
 * Build with: clang -fsanitize=fuzzer,address -o fuzz_ipc_command \
 *             fuzz_ipc_command.c -I../../include -I../../src
 *
 * Or use the meson build system (if compiler supports -fsanitize=fuzzer).
 */

#define _GNU_SOURCE

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ipc_json.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	/* Limit input to prevent excessive memory allocation */
	if (size > 64 * 1024)
		return 0;

	/* NUL-terminate the input */
	char *input = malloc(size + 1);
	if (!input)
		return 0;
	memcpy(input, data, size);
	input[size] = '\0';

	/*
	 * Exercise wm_ipc_json_get_string() with common IPC keys.
	 * This is the main attack surface — it parses untrusted
	 * JSON from IPC clients looking for key-value pairs.
	 */
	char *val;

	val = wm_ipc_json_get_string(input, "command");
	free(val);

	val = wm_ipc_json_get_string(input, "action");
	free(val);

	val = wm_ipc_json_get_string(input, "events");
	free(val);

	val = wm_ipc_json_get_string(input, "args");
	free(val);

	/* Try with empty and single-char keys */
	val = wm_ipc_json_get_string(input, "");
	free(val);

	val = wm_ipc_json_get_string(input, "x");
	free(val);

	/*
	 * Exercise wm_ipc_json_quote() — used to build JSON responses
	 * from strings that may contain attacker-controlled data
	 * (window titles, app_ids).
	 */
	char *escaped = wm_ipc_json_quote(input);
	free(escaped);

	/* Also test json_escape with truncated/partial input */
	if (size > 0) {
		char *half = malloc(size / 2 + 1);
		if (half) {
			memcpy(half, data, size / 2);
			half[size / 2] = '\0';
			escaped = wm_ipc_json_quote(half);
			free(escaped);
			free(half);
		}
	}

	free(input);
	return 0;
}
