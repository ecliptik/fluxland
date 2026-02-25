/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 *
 * autostart.c - Startup script execution
 */

#define _GNU_SOURCE

#include "autostart.h"
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
