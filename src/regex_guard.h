/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * regex_guard.h - Refuse regexes that blow up glibc's regcomp()
 */

#ifndef WM_REGEX_GUARD_H
#define WM_REGEX_GUARD_H

#include <stdbool.h>

/*
 * False for extended regexes that are too costly to compile: stacked
 * quantifiers ("a++"), {n,m} bounds over 32, more than 32 quantifiers,
 * groups nested deeper than 16, or an unterminated bracket expression.
 */
bool wm_regex_is_sane(const char *regex);

#endif /* WM_REGEX_GUARD_H */
