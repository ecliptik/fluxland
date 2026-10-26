/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * autostart.c - Startup script execution
 */

#define _GNU_SOURCE

#include "autostart.h"
#include "import.h"
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wlr/util/log.h>

static void
spawn_child(void (*exec_fn)(void *), void *data, const char *wayland_display)
{
	/*
	 * Double-fork: the first child exits immediately and is reaped
	 * inline. The grandchild is orphaned and adopted by init, which
	 * auto-reaps it on exit. This avoids needing a blanket
	 * waitpid(-1) SIGCHLD handler that would race with wlroots'
	 * Xwayland child management.
	 */
	pid_t pid = fork();
	if (pid < 0) {
		wlr_log(WLR_ERROR, "%s", "autostart: fork failed");
		return;
	}
	if (pid == 0) {
		/* First child: unblock signals, fork again, and exit */
		sigset_t set;
		sigemptyset(&set);
		sigprocmask(SIG_SETMASK, &set, NULL);

		pid_t grandchild = fork();
		if (grandchild < 0) {
			_exit(1);
		}
		if (grandchild > 0) {
			/* First child exits; grandchild is adopted by init */
			_exit(0);
		}

		/* Grandchild: start a new session and exec */
		setsid();
		closefrom(STDERR_FILENO + 1);
		if (wayland_display) {
			setenv("WAYLAND_DISPLAY", wayland_display, 1);
		}
		exec_fn(data);
		_exit(1);
	}
	/* Parent: reap the first child immediately (it exits right away) */
	waitpid(pid, NULL, 0);
	wlr_log(WLR_INFO, "%s", "autostart: launched child");
}

struct script_data {
	char path[4096];
	int executable;
};

static void
exec_script(void *data)
{
	struct script_data *sd = data;
	if (sd->executable) {
		execl(sd->path, sd->path, (char *)NULL);
	} else {
		execl("/bin/sh", "sh", sd->path, (char *)NULL);
	}
}

struct cmd_data {
	const char *cmd;
};

static void
exec_cmd(void *data)
{
	struct cmd_data *cd = data;
	execl("/bin/sh", "/bin/sh", "-c", cd->cmd, (char *)NULL);
}

/* Read from a Fluxbox directory itself (~/.fluxbox, fluxland having
 * none of its own)? */
static bool
is_fluxbox_dir(const char *dir)
{
	size_t n = strlen(dir);
	while (n > 1 && dir[n - 1] == '/')
		n--;
	return n >= 8 && strncmp(dir + n - 8, ".fluxbox", 8) == 0 &&
		(n == 8 || dir[n - 9] == '/');
}

/*
 * Fluxbox's startup script as fluxland runs it (--import-fluxbox's
 * conversion: no "exec fluxbox" -- that would start Fluxbox on Xwayland --
 * nor what only X can run), written to a file of ours in the runtime
 * directory; path gets its name. What it leaves out is logged.
 */
static bool
convert_fluxbox_startup(int fd, char *path, size_t size)
{
	struct stat st;
	if (fstat(fd, &st) != 0 || st.st_size > 1024 * 1024)
		return false;
	char *script = calloc(1, (size_t)st.st_size + 1);
	if (!script)
		return false;
	ssize_t got = read(fd, script, (size_t)st.st_size);
	if (got < 0) {
		free(script);
		return false;
	}
	script[got] = '\0';

	char *report = NULL;
	size_t report_len = 0;
	FILE *rf = open_memstream(&report, &report_len);
	char *fixed = wm_import_startup_script(script, rf);
	if (rf)
		fclose(rf);
	free(script);
	if (report && *report)
		wlr_log(WLR_INFO, "autostart: Fluxbox's startup, as fluxland "
			"runs it:\n%s", report);
	free(report);
	if (!fixed)
		return false;

	const char *runtime = getenv("XDG_RUNTIME_DIR");
	if (!runtime || !*runtime) {
		free(fixed);
		return false;
	}
	snprintf(path, size, "%s/fluxland-fluxbox-startup", runtime);
	unlink(path);
	int out = open(path, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW |
		O_CLOEXEC, 0700);
	bool ok = out >= 0;
	if (ok) {
		size_t len = strlen(fixed);
		ok = write(out, fixed, len) == (ssize_t)len;
		close(out);
	}
	free(fixed);
	return ok;
}

int
wm_autostart_run(const char *config_dir, const char *wayland_display)
{
	if (!config_dir) {
		return -1;
	}

	struct script_data sd;
	snprintf(sd.path, sizeof(sd.path), "%s/startup", config_dir);

	/* Open the file and use fstat() to avoid TOCTOU race between
	 * stat check and later exec.  O_NOFOLLOW is omitted because users
	 * may symlink their startup scripts. */
	int fd = open(sd.path, O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		wlr_log(WLR_DEBUG, "autostart: no startup file at %s", sd.path);
		return -1;
	}

	struct stat st;
	if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode)) {
		wlr_log(WLR_ERROR, "autostart: %s is not a regular file",
			sd.path);
		close(fd);
		return -1;
	}

	sd.executable = (st.st_mode & S_IXUSR) != 0;
	if (is_fluxbox_dir(config_dir)) {
		if (!convert_fluxbox_startup(fd, sd.path, sizeof(sd.path))) {
			wlr_log(WLR_ERROR, "autostart: can't make %s/startup "
				"one fluxland runs: not run", config_dir);
			close(fd);
			return -1;
		}
		sd.executable = 1;
	}
	close(fd);
	wlr_log(WLR_INFO, "autostart: running %s (%s)", sd.path,
		sd.executable ? "executable" : "via /bin/sh");

	spawn_child(exec_script, &sd, wayland_display);
	return 0;
}

void
wm_autostart_run_cmd(const char *cmd, const char *wayland_display)
{
	if (!cmd) {
		return;
	}

	wlr_log(WLR_INFO, "autostart: running startup command: %s", cmd);
	struct cmd_data cd = { .cmd = cmd };
	spawn_child(exec_cmd, &cd, wayland_display);
}
