/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * regex_guard.c - Refuse regexes that blow up glibc's regcomp()
 *
 * Regexes come from config files (apps, keys) and IPC. glibc expands
 * repetitions by copying subtrees, so a short pattern can take
 * gigabytes to compile. Found by the fuzz_command and fuzz_rules
 * targets.
 */

#include <stdbool.h>
#include <stdlib.h>

#include "regex_guard.h"

/*
 * Reject regexes that make glibc's regcomp() explode in time or memory
 * (it expands repetitions by copying subtrees): stacked quantifiers
 * such as "a+++" (undefined in POSIX EREs anyway), large {n,m} bounds,
 * too many quantifiers, or deep nesting. Window patterns never need
 * any of these.
 */
#define MAX_QUANTIFIERS 32
#define MAX_REPEAT 32
#define MAX_GROUP_DEPTH 16

bool
wm_regex_is_sane(const char *str)
{
	int quantifiers = 0, depth = 0;
	bool prev_quant = false;
	for (const char *p = str; *p; p++) {
		bool quant = false;
		switch (*p) {
		case '\\':
			if (p[1])
				p++;
			break;
		case '[': {
			/* Skip a bracket expression; ']' first is literal */
			const char *q = p + 1;
			if (*q == '^')
				q++;
			if (*q == ']')
				q++;
			while (*q && *q != ']')
				q++;
			if (!*q)
				return false;
			p = q;
			break;
		}
		case '(':
			if (++depth > MAX_GROUP_DEPTH)
				return false;
			break;
		case ')':
			depth--;
			break;
		case '*':
		case '+':
		case '?':
			quant = true;
			break;
		case '{': {
			char *end;
			long lo = strtol(p + 1, &end, 10);
			if (end == p + 1)
				break; /* literal '{' */
			long hi = lo;
			if (*end == ',') {
				const char *h = end + 1;
				hi = strtol(h, &end, 10);
				if (end == h)
					hi = lo; /* {n,} */
			}
			if (*end != '}')
				break;
			if (lo > MAX_REPEAT || hi > MAX_REPEAT)
				return false;
			p = end;
			quant = true;
			break;
		}
		default:
			break;
		}
		if (quant) {
			if (prev_quant || ++quantifiers > MAX_QUANTIFIERS)
				return false;
		}
		prev_quant = quant;
	}
	return true;
}

