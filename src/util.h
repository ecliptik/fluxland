/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * util.h - Shared utility helpers
 */

#ifndef WM_UTIL_H
#define WM_UTIL_H

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

/*
 * Open a file with O_NOFOLLOW to reject symlinks.
 * Defense against symlink attacks on config file reload paths
 * (an attacker with IPC access could create symlinks between
 * check and reload). Logs a warning if the path is a symlink.
 */
static inline FILE *fopen_nofollow(const char *path, const char *mode) {
	int flags = O_RDONLY | O_NOFOLLOW;
	if (strchr(mode, 'w'))
		flags = O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW;
	if (strchr(mode, 'a'))
		flags = O_WRONLY | O_CREAT | O_APPEND | O_NOFOLLOW;
	int fd = open(path, flags, 0644);
	if (fd < 0) {
		if (errno == ELOOP)
			fprintf(stderr, "[fluxland] Refusing to open"
				" symlink: %s\n", path);
		return NULL;
	}
	FILE *f = fdopen(fd, mode);
	if (!f)
		close(fd);
	return f;
}

#endif /* WM_UTIL_H */
