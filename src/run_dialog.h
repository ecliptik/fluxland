/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * run_dialog.h - What fluxland-run (RunDialog, fbrun's place) needs of the
 * system: names to complete, and the commands run before
 */

#ifndef WM_RUN_DIALOG_H
#define WM_RUN_DIALOG_H

#include <stdbool.h>
#include <stddef.h>

/*
 * The words that could finish the one at the end of text[0..cursor): a
 * program on PATH for the first word, else a file or directory (a directory
 * ends in '/').  The word is what follows the last blank before the cursor;
 * it may begin with ~/.  Returns how many, sorted and without repeats, in
 * *out (a list to free with wm_run_free_list()); *start is where the word
 * begins in text.  Each is the whole word as it is to be written.
 */
size_t wm_run_candidates(const char *text, size_t cursor, char ***out,
	size_t *start);

/* The longest prefix every one of the n words has, in a new string */
char *wm_run_common_prefix(char **words, size_t n);

void wm_run_free_list(char **list, size_t n);

/* The commands run, oldest first */
struct wm_run_history {
	char **items;
	size_t count;
};

#define WM_RUN_HISTORY_MAX 200

/* Where it is kept: $XDG_STATE_HOME/fluxland/run_history, else under
 * ~/.local/state; a new string, NULL with neither set */
char *wm_run_history_path(void);

/* Read it (nothing if there is none) and write it (false if it could not
 * be); neither follows a symlink at the file */
void wm_run_history_load(struct wm_run_history *history, const char *path);
bool wm_run_history_save(const struct wm_run_history *history,
	const char *path);

/* A command run: it goes last, and is not twice in it; the oldest go when
 * there are more than WM_RUN_HISTORY_MAX */
void wm_run_history_add(struct wm_run_history *history, const char *command);

void wm_run_history_free(struct wm_run_history *history);

#endif /* WM_RUN_DIALOG_H */
