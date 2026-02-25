/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 *
 * util.h - Shared utility helpers
 */

#ifndef WM_UTIL_H
#define WM_UTIL_H

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>

/* Safe integer parsing — returns false on overflow, non-numeric input, or empty string */
static inline bool safe_atoi(const char *s, int *out) {
	if (!s || !*s) return false;
	char *end;
	errno = 0;
	long val = strtol(s, &end, 10);
	if (errno || end == s || *end != '\0' || val > INT_MAX || val < INT_MIN)
		return false;
	*out = (int)val;
	return true;
}

#endif /* WM_UTIL_H */
