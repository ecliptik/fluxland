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
 * The json_get_string() and json_escape() helpers are extracted from
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

/*
 * ---------- JSON helpers extracted from src/ipc_commands.c ----------
 *
 * These are the exact functions that parse untrusted JSON input from
 * IPC clients.  Copied here to avoid linking the full compositor.
 */

/* Escape a string for JSON output. Returns malloc'd string. */
static char *
json_escape(const char *s)
{
	if (!s) {
		return strdup("null");
	}
	/* Worst case: every char needs escaping (\uXXXX = 6 chars) */
	size_t len = strlen(s);
	if (len > SIZE_MAX / 6 - 1)
		return NULL;
	size_t cap = len * 6 + 3; /* quotes + null */
	char *out = malloc(cap);
	if (!out) return NULL;

	char *p = out;
	*p++ = '"';
	for (size_t i = 0; i < len; i++) {
		unsigned char c = (unsigned char)s[i];
		switch (c) {
		case '"':  *p++ = '\\'; *p++ = '"';  break;
		case '\\': *p++ = '\\'; *p++ = '\\'; break;
		case '\n': *p++ = '\\'; *p++ = 'n';  break;
		case '\r': *p++ = '\\'; *p++ = 'r';  break;
		case '\t': *p++ = '\\'; *p++ = 't';  break;
		default:
			if (c < 0x20) {
				p += snprintf(p, cap - (size_t)(p - out),
					"\\u%04x", c);
			} else {
				*p++ = (char)c;
			}
			break;
		}
	}
	*p++ = '"';
	*p = '\0';
	return out;
}

/* Extract a string value for a given key from a JSON object string.
 * Very minimal: only handles top-level string and bare-word values.
 * Returns malloc'd string or NULL. */
static char *
json_get_string(const char *json, const char *key)
{
	/* Find "key" : */
	size_t klen = strlen(key);
	const char *p = json;
	while ((p = strstr(p, key)) != NULL) {
		/* Check that it's preceded by a quote */
		if (p > json && *(p - 1) == '"') {
			const char *after = p + klen;
			if (*after == '"') {
				after++;
				/* Skip whitespace and colon */
				while (*after && (*after == ' ' || *after == '\t'))
					after++;
				if (*after == ':') {
					after++;
					while (*after && (*after == ' ' || *after == '\t'))
						after++;
					if (*after == '"') {
						/* Quoted string value — unescape */
						after++;
						const char *end = after;
						while (*end && *end != '"') {
							if (*end == '\\') {
								if (*(end + 1) == '\0')
									break;
								end++;
							}
							end++;
						}
						/* Unterminated string — no closing quote */
						if (*end != '"') {
							return NULL;
						}
						size_t vlen = (size_t)(end - after);
						char *val = malloc(vlen + 1);
						if (!val) return NULL;
						size_t j = 0;
						for (const char *s = after; s < end; s++) {
							if (*s == '\\' && s + 1 < end) {
								s++;
								switch (*s) {
								case '"':  val[j++] = '"';  break;
								case '\\': val[j++] = '\\'; break;
								case '/':  val[j++] = '/';  break;
								case 'n':  val[j++] = '\n'; break;
								case 't':  val[j++] = '\t'; break;
								case 'r':  val[j++] = '\r'; break;
								default:   val[j++] = *s;   break;
								}
							} else {
								val[j++] = *s;
							}
						}
						val[j] = '\0';
						return val;
					}
					/* Unquoted value (number, bool, null) */
					const char *end = after;
					while (*end && *end != ',' && *end != '}'
						&& *end != ' ' && *end != '\n')
						end++;
					size_t vlen = (size_t)(end - after);
					char *val = malloc(vlen + 1);
					if (val) {
						memcpy(val, after, vlen);
						val[vlen] = '\0';
					}
					return val;
				}
			}
		}
		p++;
	}
	return NULL;
}

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
	 * Exercise json_get_string() with common IPC keys.
	 * This is the main attack surface — it parses untrusted
	 * JSON from IPC clients looking for key-value pairs.
	 */
	char *val;

	val = json_get_string(input, "command");
	free(val);

	val = json_get_string(input, "action");
	free(val);

	val = json_get_string(input, "events");
	free(val);

	val = json_get_string(input, "args");
	free(val);

	/* Try with empty and single-char keys */
	val = json_get_string(input, "");
	free(val);

	val = json_get_string(input, "x");
	free(val);

	/*
	 * Exercise json_escape() — used to build JSON responses
	 * from strings that may contain attacker-controlled data
	 * (window titles, app_ids).
	 */
	char *escaped = json_escape(input);
	free(escaped);

	/* Also test json_escape with truncated/partial input */
	if (size > 0) {
		char *half = malloc(size / 2 + 1);
		if (half) {
			memcpy(half, data, size / 2);
			half[size / 2] = '\0';
			escaped = json_escape(half);
			free(escaped);
			free(half);
		}
	}

	free(input);
	return 0;
}
