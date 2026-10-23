/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * ipc_json.c - Minimal JSON helpers for the IPC protocol
 *
 * Kept separate from ipc_commands.c so the fuzz target exercises the
 * same code the compositor runs on untrusted socket input.
 */

#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ipc_json.h"

char *
wm_ipc_json_quote(const char *s)
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
				p += snprintf(p, cap - (size_t)(p - out), "\\u%04x", c);
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

char *
wm_ipc_json_get_string(const char *json, const char *key)
{
	/* Find "key" : (an empty key would match at every position) */
	size_t klen = strlen(key);
	if (klen == 0)
		return NULL;
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
