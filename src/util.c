/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * util.c - Process spawning and environment helpers
 */

#define _GNU_SOURCE

#include <signal.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wlr/util/log.h>

#include "util.h"

void
wm_spawn_command(const char *cmd)
{
	if (!cmd || !*cmd) {
		return;
	}
	/*
	 * Double-fork: the first child exits immediately and is reaped
	 * inline.  The grandchild is orphaned and adopted by init, which
	 * auto-reaps it on exit.  This avoids needing a blanket
	 * waitpid(-1) SIGCHLD handler that would race with wlroots'
	 * Xwayland child management.
	 */
	pid_t pid = fork();
	if (pid < 0) {
		wlr_log(WLR_ERROR, "fork failed for: %s", cmd);
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

		execl("/bin/sh", "/bin/sh", "-c", cmd, (char *)NULL);
		_exit(1);
	}
	waitpid(pid, NULL, 0);
}
