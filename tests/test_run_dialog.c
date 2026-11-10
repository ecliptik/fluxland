/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * test_run_dialog.c - Run dialog completion and history
 */

#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "run_dialog.h"

static char root[256];

static void
touch(const char *dir, const char *name, mode_t mode)
{
	char path[512];
	snprintf(path, sizeof(path), "%s/%s", dir, name);
	int fd = open(path, O_CREAT | O_WRONLY, mode);
	assert(fd >= 0);
	close(fd);
	chmod(path, mode);
}

static bool
has(char **list, size_t n, const char *word)
{
	for (size_t i = 0; i < n; i++)
		if (strcmp(list[i], word) == 0)
			return true;
	return false;
}

static void
test_programs_from_path(void)
{
	char bin[300];
	snprintf(bin, sizeof(bin), "%s/bin", root);
	mkdir(bin, 0700);
	touch(bin, "fooprog", 0700);
	touch(bin, "foodata", 0600);	/* not executable */
	touch(bin, "foobar", 0700);
	setenv("PATH", bin, 1);

	char **c;
	size_t start;
	size_t n = wm_run_candidates("foo", 3, &c, &start);
	assert(start == 0);
	assert(n == 2);
	assert(has(c, n, "fooprog") && has(c, n, "foobar"));
	char *common = wm_run_common_prefix(c, n);
	assert(strcmp(common, "foo") == 0);
	free(common);
	wm_run_free_list(c, n);

	n = wm_run_candidates("foop", 4, &c, &start);
	assert(n == 1 && strcmp(c[0], "fooprog") == 0);
	wm_run_free_list(c, n);
}

static void
test_files_after_first_word(void)
{
	char dir[300];
	snprintf(dir, sizeof(dir), "%s/files", root);
	mkdir(dir, 0700);
	touch(dir, "alpha.txt", 0600);
	char sub[400];
	snprintf(sub, sizeof(sub), "%s/alphadir", dir);
	mkdir(sub, 0700);

	char text[600];
	snprintf(text, sizeof(text), "cat %s/alp", dir);
	char **c;
	size_t start;
	size_t n = wm_run_candidates(text, strlen(text), &c, &start);
	assert(n == 2);
	assert(start == 4);
	char want[400];
	snprintf(want, sizeof(want), "%s/alphadir/", dir);
	assert(has(c, n, want));
	snprintf(want, sizeof(want), "%s/alpha.txt", dir);
	assert(has(c, n, want));
	wm_run_free_list(c, n);

	/* Nothing matches */
	snprintf(text, sizeof(text), "cat %s/zzz", dir);
	n = wm_run_candidates(text, strlen(text), &c, &start);
	assert(n == 0);
	wm_run_free_list(c, n);
}

static void
test_history(void)
{
	char path[300];
	snprintf(path, sizeof(path), "%s/state/sub/history", root);

	struct wm_run_history h = { 0 };
	wm_run_history_add(&h, "one");
	wm_run_history_add(&h, "two");
	wm_run_history_add(&h, "one");	/* moves to the end */
	assert(h.count == 2);
	assert(strcmp(h.items[0], "two") == 0);
	assert(strcmp(h.items[1], "one") == 0);
	assert(wm_run_history_save(&h, path));

	struct stat st;
	assert(stat(path, &st) == 0);
	assert((st.st_mode & 0777) == 0600);

	struct wm_run_history g = { 0 };
	wm_run_history_load(&g, path);
	assert(g.count == 2);
	assert(strcmp(g.items[1], "one") == 0);
	wm_run_history_free(&g);
	wm_run_history_free(&h);

	/* Only the newest are kept */
	struct wm_run_history m = { 0 };
	for (int i = 0; i < WM_RUN_HISTORY_MAX + 10; i++) {
		char cmd[32];
		snprintf(cmd, sizeof(cmd), "cmd%d", i);
		wm_run_history_add(&m, cmd);
	}
	assert(m.count == WM_RUN_HISTORY_MAX);
	assert(strcmp(m.items[0], "cmd10") == 0);
	wm_run_history_free(&m);

	/* A symlink in the way is replaced, not written through */
	char target[300], link[300];
	snprintf(target, sizeof(target), "%s/target", root);
	snprintf(link, sizeof(link), "%s/link", root);
	touch(root, "target", 0600);
	assert(symlink(target, link) == 0);
	struct wm_run_history s = { 0 };
	wm_run_history_add(&s, "evil");
	wm_run_history_save(&s, link);
	assert(stat(target, &st) == 0 && st.st_size == 0);
	wm_run_history_free(&s);
}

int
main(void)
{
	snprintf(root, sizeof(root), "/tmp/fluxland-run-dialog-XXXXXX");
	if (!mkdtemp(root))
		return 1;
	char *old_path = strdup(getenv("PATH"));
	test_programs_from_path();
	setenv("PATH", old_path, 1);
	free(old_path);
	test_files_after_first_word();
	test_history();
	char cmd[400];
	snprintf(cmd, sizeof(cmd), "rm -rf '%s'", root);
	if (system(cmd) != 0)
		return 1;
	printf("test_run_dialog: all passed\n");
	return 0;
}
