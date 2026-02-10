/*
 * wm-wayland - A Fluxbox-inspired Wayland compositor
 *
 * autostart.c - Startup script execution
 */

#define _POSIX_C_SOURCE 200809L

#include "autostart.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wlr/util/log.h>

static void
spawn_child(void (*exec_fn)(void *), void *data, const char *wayland_display)
{
	/* Prevent zombie processes */
	struct sigaction sa = { .sa_handler = SIG_IGN };
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_NOCLDWAIT;
	sigaction(SIGCHLD, &sa, NULL);

	pid_t pid = fork();
	if (pid < 0) {
		wlr_log(WLR_ERROR, "autostart: fork failed");
		return;
	}
	if (pid == 0) {
		/* Child process */
		if (wayland_display) {
			setenv("WAYLAND_DISPLAY", wayland_display, 1);
		}
		exec_fn(data);
		_exit(1);
	}
	wlr_log(WLR_INFO, "autostart: launched child pid %d", pid);
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

	struct stat st;
	if (stat(sd.path, &st) != 0) {
		wlr_log(WLR_DEBUG, "autostart: no startup file at %s", sd.path);
		return -1;
	}

	if (!S_ISREG(st.st_mode)) {
		wlr_log(WLR_ERROR, "autostart: %s is not a regular file",
			sd.path);
		return -1;
	}

	sd.executable = (st.st_mode & S_IXUSR) != 0;
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
