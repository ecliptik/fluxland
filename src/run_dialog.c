/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * run_dialog.c - What fluxland-run (RunDialog, fbrun's place) needs of the
 * system: names to complete, and the commands run before
 */

#define _GNU_SOURCE
#include "run_dialog.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* --- Completion --- */

static int
compare_strings(const void *a, const void *b)
{
	return strcmp(*(char *const *)a, *(char *const *)b);
}

struct list {
	char **items;
	size_t count, capacity;
};

static void
list_add(struct list *list, const char *s)
{
	if (list->count == list->capacity) {
		size_t capacity = list->capacity ? list->capacity * 2 : 32;
		char **items = realloc(list->items, capacity * sizeof(char *));
		if (!items)
			return;
		list->items = items;
		list->capacity = capacity;
	}
	char *copy = strdup(s);
	if (copy)
		list->items[list->count++] = copy;
}

/* Sort, and drop the repeats (a program is on PATH more than once) */
static void
list_finish(struct list *list)
{
	if (list->count < 2)
		return;
	qsort(list->items, list->count, sizeof(char *), compare_strings);
	size_t out = 1;
	for (size_t i = 1; i < list->count; i++) {
		if (strcmp(list->items[i], list->items[out - 1]) == 0)
			free(list->items[i]);
		else
			list->items[out++] = list->items[i];
	}
	list->count = out;
}

static bool
is_executable_file(const char *path)
{
	struct stat st;
	return stat(path, &st) == 0 && S_ISREG(st.st_mode) &&
		access(path, X_OK) == 0;
}

static bool
is_directory(const char *path)
{
	struct stat st;
	return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

/* The programs on PATH that start with prefix */
static void
complete_program(const char *prefix, struct list *out)
{
	const char *path = getenv("PATH");
	if (!path)
		return;
	char *copy = strdup(path);
	if (!copy)
		return;
	size_t plen = strlen(prefix);
	char *save = NULL;
	for (char *dir = strtok_r(copy, ":", &save); dir;
	     dir = strtok_r(NULL, ":", &save)) {
		DIR *d = opendir(*dir ? dir : ".");
		if (!d)
			continue;
		struct dirent *e;
		while ((e = readdir(d))) {
			if (e->d_name[0] == '.' && prefix[0] != '.')
				continue;
			if (strncmp(e->d_name, prefix, plen) != 0)
				continue;
			char full[PATH_MAX];
			snprintf(full, sizeof(full), "%s/%s", dir, e->d_name);
			if (is_executable_file(full))
				list_add(out, e->d_name);
		}
		closedir(d);
	}
	free(copy);
}

/* Files and directories for a word with a path in it (or after the first
 * word): those that start with its last component */
static void
complete_path(const char *word, bool first_word, struct list *out)
{
	/* The directory as typed (kept in what is written), and as read */
	const char *slash = strrchr(word, '/');
	size_t dirlen = slash ? (size_t)(slash - word) + 1 : 0;
	const char *name = word + dirlen;

	char typed_dir[PATH_MAX], read_dir[PATH_MAX];
	snprintf(typed_dir, sizeof(typed_dir), "%.*s", (int)dirlen, word);
	if (typed_dir[0] == '~' && (typed_dir[1] == '/' || typed_dir[1] == 0)) {
		const char *home = getenv("HOME");
		if (!home)
			return;
		snprintf(read_dir, sizeof(read_dir), "%s%s", home,
			typed_dir + 1);
	} else {
		snprintf(read_dir, sizeof(read_dir), "%s",
			typed_dir[0] ? typed_dir : ".");
	}

	DIR *d = opendir(read_dir);
	if (!d)
		return;
	size_t nlen = strlen(name);
	struct dirent *e;
	while ((e = readdir(d))) {
		if (e->d_name[0] == '.' && name[0] != '.')
			continue;
		if (strncmp(e->d_name, name, nlen) != 0)
			continue;
		char full[PATH_MAX];
		snprintf(full, sizeof(full), "%s/%s", read_dir, e->d_name);
		bool dir = is_directory(full);
		/* A program wants a program or a directory to look in */
		if (first_word && !dir && !is_executable_file(full))
			continue;
		char cand[PATH_MAX];
		snprintf(cand, sizeof(cand), "%s%s%s", typed_dir, e->d_name,
			dir ? "/" : "");
		list_add(out, cand);
	}
	closedir(d);
}

size_t
wm_run_candidates(const char *text, size_t cursor, char ***out,
	size_t *start)
{
	*out = NULL;
	size_t s = cursor;
	while (s > 0 && text[s - 1] != ' ' && text[s - 1] != '\t')
		s--;
	if (start)
		*start = s;
	char word[PATH_MAX];
	snprintf(word, sizeof(word), "%.*s", (int)(cursor - s), text + s);

	bool first = true;
	for (size_t i = 0; i < s; i++)
		if (text[i] != ' ' && text[i] != '\t')
			first = false;

	struct list list = { 0 };
	if (first && !strchr(word, '/') && word[0] != '~' && word[0])
		complete_program(word, &list);
	else if (word[0] || !first)
		complete_path(word, first, &list);
	list_finish(&list);
	*out = list.items;
	return list.count;
}

char *
wm_run_common_prefix(char **words, size_t n)
{
	if (n == 0)
		return strdup("");
	size_t len = strlen(words[0]);
	for (size_t i = 1; i < n; i++) {
		size_t k = 0;
		while (k < len && words[i][k] && words[i][k] == words[0][k])
			k++;
		len = k;
	}
	return strndup(words[0], len);
}

void
wm_run_free_list(char **list, size_t n)
{
	for (size_t i = 0; i < n; i++)
		free(list[i]);
	free(list);
}

/* --- History --- */

char *
wm_run_history_path(void)
{
	const char *state = getenv("XDG_STATE_HOME");
	const char *home = getenv("HOME");
	char *path = NULL;
	if (state && *state) {
		if (asprintf(&path, "%s/fluxland/run_history", state) < 0)
			path = NULL;
	} else if (home && *home) {
		if (asprintf(&path, "%s/.local/state/fluxland/run_history",
			     home) < 0)
			path = NULL;
	}
	return path;
}

void
wm_run_history_free(struct wm_run_history *history)
{
	wm_run_free_list(history->items, history->count);
	history->items = NULL;
	history->count = 0;
}

void
wm_run_history_add(struct wm_run_history *history, const char *command)
{
	if (!command || !*command)
		return;
	/* Where it was before, if it was, is no more */
	for (size_t i = 0; i < history->count; i++) {
		if (strcmp(history->items[i], command) == 0) {
			free(history->items[i]);
			memmove(&history->items[i], &history->items[i + 1],
				(history->count - i - 1) * sizeof(char *));
			history->count--;
			break;
		}
	}
	/* Room for one more at the end (and the oldest go) */
	if (history->count >= WM_RUN_HISTORY_MAX) {
		size_t drop = history->count - WM_RUN_HISTORY_MAX + 1;
		for (size_t i = 0; i < drop; i++)
			free(history->items[i]);
		memmove(history->items, history->items + drop,
			(history->count - drop) * sizeof(char *));
		history->count -= drop;
	}
	char **items = realloc(history->items,
		(history->count + 1) * sizeof(char *));
	if (!items)
		return;
	history->items = items;
	char *copy = strdup(command);
	if (copy)
		history->items[history->count++] = copy;
}

void
wm_run_history_load(struct wm_run_history *history, const char *path)
{
	if (!path)
		return;
	int fd = open(path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
	if (fd < 0)
		return;
	FILE *f = fdopen(fd, "r");
	if (!f) {
		close(fd);
		return;
	}
	char *line = NULL;
	size_t cap = 0;
	ssize_t n;
	while ((n = getline(&line, &cap, f)) >= 0) {
		while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
			line[--n] = '\0';
		wm_run_history_add(history, line);
	}
	free(line);
	fclose(f);
}

/* mkdir -p of the directory of path */
static void
make_parent_dirs(const char *path)
{
	char dir[PATH_MAX];
	snprintf(dir, sizeof(dir), "%s", path);
	char *slash = strrchr(dir, '/');
	if (!slash)
		return;
	*slash = '\0';
	for (char *p = dir + 1; *p; p++) {
		if (*p == '/') {
			*p = '\0';
			mkdir(dir, 0700);
			*p = '/';
		}
	}
	mkdir(dir, 0700);
}

bool
wm_run_history_save(const struct wm_run_history *history, const char *path)
{
	if (!path)
		return false;
	make_parent_dirs(path);
	char tmp[PATH_MAX];
	snprintf(tmp, sizeof(tmp), "%s.%d.tmp", path, (int)getpid());
	int fd = open(tmp, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW |
		O_CLOEXEC, 0600);
	if (fd < 0)
		return false;
	FILE *f = fdopen(fd, "w");
	if (!f) {
		close(fd);
		unlink(tmp);
		return false;
	}
	for (size_t i = 0; i < history->count; i++)
		fprintf(f, "%s\n", history->items[i]);
	bool ok = fflush(f) == 0 && !ferror(f);
	fclose(f);
	if (!ok || rename(tmp, path) != 0) {
		unlink(tmp);
		return false;
	}
	return true;
}
