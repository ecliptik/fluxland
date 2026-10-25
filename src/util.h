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
#include <strings.h>
#include <sys/stat.h>
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

/* Check whether an environment variable name is on the security denylist.
 * Used by SetEnv action handlers to block modification of dangerous vars. */
static inline bool wm_is_blocked_env_var(const char *name) {
	static const char *blocked[] = {
		"LD_PRELOAD", "LD_LIBRARY_PATH", "LD_AUDIT",
		"LD_DEBUG", "LD_PROFILE",
		"PATH", "IFS", "SHELL", "HOME",
		"XDG_RUNTIME_DIR", "WAYLAND_DISPLAY",
	};
	for (size_t i = 0; i < sizeof(blocked) / sizeof(blocked[0]); i++) {
		if (strcasecmp(name, blocked[i]) == 0)
			return true;
	}
	return false;
}

/* Spawn a command using double-fork to avoid SIGCHLD racing with wlroots.
 * Sanitizes LD_* environment variables before exec. */
void wm_spawn_command(const char *cmd);

/*
 * Run exec_fn(data) in a double-forked child (orphaned, so it needs no
 * reaping) with a sanitized environment. exec_fn should exec.
 */
void wm_spawn(void (*exec_fn)(void *), void *data);

/* Escape a string for safe inclusion in JSON.
 * Writes into dst (up to dst_size bytes including NUL).
 * Escapes backslash, double-quote, and control characters. */
void wm_json_escape(char *dst, size_t dst_size, const char *src);

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

/*
 * Open a configuration file, following a symlink only to a file that is
 * the user's (or root's) and that no one else can write: dotfile
 * managers (stow, chezmoi, home-manager) link ~/.fluxbox/menu and the
 * rest, while a link planted to someone else's file is still refused.
 * Only regular files are opened (a FIFO or device would hang).
 */
static inline bool config_file_trusted(const struct stat *st) {
	return S_ISREG(st->st_mode) &&
		(st->st_uid == getuid() || st->st_uid == 0) &&
		!(st->st_mode & (S_IWGRP | S_IWOTH));
}

static inline FILE *fopen_config(const char *path, const char *mode) {
	struct stat st;
	if (lstat(path, &st) != 0 || !S_ISLNK(st.st_mode)) {
		if (lstat(path, &st) == 0 && !S_ISREG(st.st_mode))
			return NULL;
		return fopen_nofollow(path, mode);
	}
	bool writing = strchr(mode, 'w') || strchr(mode, 'a');
	/* Check a write's target before opening it (it truncates) */
	if (writing && (stat(path, &st) != 0 || !config_file_trusted(&st))) {
		fprintf(stderr, "[fluxland] Refusing to follow symlink: %s\n",
			path);
		return NULL;
	}
	int flags = O_RDONLY;
	if (strchr(mode, 'w'))
		flags = O_WRONLY | O_TRUNC;
	if (strchr(mode, 'a'))
		flags = O_WRONLY | O_APPEND;
	int fd = open(path, flags);
	if (fd < 0)
		return NULL;
	/* What was opened, not what the name points to now */
	if (fstat(fd, &st) != 0 || !config_file_trusted(&st)) {
		fprintf(stderr, "[fluxland] Refusing to follow symlink: %s\n",
			path);
		close(fd);
		return NULL;
	}
	FILE *f = fdopen(fd, mode);
	if (!f)
		close(fd);
	return f;
}

#endif /* WM_UTIL_H */
