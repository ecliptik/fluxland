/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * util.c - Process spawning and environment helpers
 */

#define _GNU_SOURCE

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wlr/util/log.h>

#include "util.h"

void
wm_spawn(void (*exec_fn)(void *), void *data)
{
	/*
	 * Double-fork: the first child exits immediately and is reaped
	 * inline.  The grandchild is orphaned and adopted by init, which
	 * auto-reaps it on exit.  This avoids needing a blanket
	 * waitpid(-1) SIGCHLD handler that would race with wlroots'
	 * Xwayland child management.
	 */
	pid_t pid = fork();
	if (pid < 0) {
		wlr_log(WLR_ERROR, "fork failed: %s", strerror(errno));
		return;
	}
	if (pid == 0) {
		sigset_t set;
		sigemptyset(&set);
		sigprocmask(SIG_SETMASK, &set, NULL);

		pid_t grandchild = fork();
		if (grandchild < 0) {
			_exit(1);
		}
		if (grandchild > 0) {
			_exit(0);
		}
		/* Grandchild */
		setsid();
		closefrom(STDERR_FILENO + 1);

		/* Sanitize environment: strip dangerous LD_* vars */
		unsetenv("LD_PRELOAD");
		unsetenv("LD_LIBRARY_PATH");
		unsetenv("LD_AUDIT");
		unsetenv("LD_DEBUG");
		unsetenv("LD_PROFILE");

		exec_fn(data);
		_exit(127);
	}
	waitpid(pid, NULL, 0);
}

static void
exec_shell(void *data)
{
	/* The user's shell, as Fluxbox's ExecuteCmd runs it */
	const char *shell = getenv("SHELL");
	if (!shell || shell[0] != '/')
		shell = "/bin/sh";
	execl(shell, shell, "-c", (const char *)data, (char *)NULL);
	/* Not there: the plain one */
	execl("/bin/sh", "/bin/sh", "-c", (const char *)data, (char *)NULL);
}

void
wm_spawn_command(const char *cmd)
{
	if (!cmd || !*cmd) {
		return;
	}
	wm_spawn(exec_shell, (void *)cmd);
}

void
wm_json_escape(char *dst, size_t dst_size, const char *src)
{
	if (!src) {
		if (dst_size > 0)
			dst[0] = '\0';
		return;
	}
	size_t j = 0;
	for (size_t i = 0; src[i] && j + 6 < dst_size; i++) {
		unsigned char c = (unsigned char)src[i];
		if (c == '"' || c == '\\') {
			dst[j++] = '\\';
			dst[j++] = c;
		} else if (c < 0x20) {
			/* Skip control characters */
			continue;
		} else {
			dst[j++] = c;
		}
	}
	dst[j] = '\0';
}
