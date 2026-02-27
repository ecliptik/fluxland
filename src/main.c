/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * main.c - Entry point
 */

#define _POSIX_C_SOURCE 200809L
#include <getopt.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <wlr/util/log.h>

#include "autostart.h"
#include "config.h"
#include "i18n.h"
#include "keybind.h"
#include "server.h"
#include "validate.h"

static const char usage[] = N_(
	"Usage: fluxland [options...]\n"
	"  -s, --startup <cmd>  Run command on startup\n"
	"  -c, --check-config   Validate config and exit\n"
	"  -d, --debug          Enable debug logging\n"
	"  -v, --version        Show version and exit\n"
	"  -l, --list-commands  List available commands and exit\n"
	"  --ipc-no-exec        Disable Exec/BindKey/SetStyle via IPC\n"
	"  -h, --help           Show this help\n");

enum {
	OPT_IPC_NO_EXEC = 256,
};

static const struct option long_options[] = {
	{"startup",      required_argument, NULL, 's'},
	{"check-config", no_argument,       NULL, 'c'},
	{"debug",        no_argument,       NULL, 'd'},
	{"version",      no_argument,       NULL, 'v'},
	{"list-commands", no_argument,      NULL, 'l'},
	{"ipc-no-exec",  no_argument,       NULL, OPT_IPC_NO_EXEC},
	{"help",         no_argument,       NULL, 'h'},
	{0, 0, 0, 0},
};

int
main(int argc, char *argv[])
{
	setlocale(LC_ALL, "");
#ifdef HAVE_GETTEXT
	bindtextdomain("fluxland", LOCALEDIR);
	textdomain("fluxland");
#endif

	char *startup_cmd = NULL;
	bool check_config = false;
	bool ipc_no_exec = false;
	enum wlr_log_importance verbosity = WLR_ERROR;

	int c;
	while ((c = getopt_long(argc, argv, "s:cdvlh", long_options,
			NULL)) != -1) {
		switch (c) {
		case 's':
			startup_cmd = optarg;
			break;
		case 'c': {
			check_config = true;
			break;
		}
		case 'd':
			verbosity = WLR_DEBUG;
			break;
		case OPT_IPC_NO_EXEC:
			ipc_no_exec = true;
			break;
		case 'v':
			printf("fluxland %s\n", FLUXLAND_VERSION);
			return 0;
		case 'l':
			wm_keybind_list_actions();
			return 0;
		case 'h':
		default:
			printf("%s", _(usage));
			return 0;
		}
	}

	if (check_config) {
		struct wm_config *cfg = config_create();
		if (!cfg) {
			fprintf(stderr, "%s\n", _("failed to create config"));
			return 1;
		}
		const char *dir = cfg->config_dir;
		if (!dir) {
			fprintf(stderr, "%s\n",
				_("cannot determine config directory"));
			config_destroy(cfg);
			return 1;
		}
		int ret = validate_config(dir);
		config_destroy(cfg);
		return ret;
	}

	wlr_log_init(verbosity, NULL);

	if (getuid() == 0 || geteuid() == 0) {
		wlr_log(WLR_ERROR, "%s",
			"Running as root is not supported. "
			"Use a non-root user account.");
		return 1;
	}

	if (!getenv("XDG_RUNTIME_DIR")) {
		wlr_log(WLR_ERROR, "%s", "XDG_RUNTIME_DIR is not set");
		return 1;
	}

	struct wm_server server = {0};
	if (!wm_server_init(&server)) {
		wlr_log(WLR_ERROR, "%s", "failed to initialize server");
		return 1;
	}
	server.ipc_no_exec = ipc_no_exec;

	if (!wm_server_start(&server)) {
		wlr_log(WLR_ERROR, "%s", "failed to start server");
		wm_server_destroy(&server);
		return 1;
	}

	setenv("WAYLAND_DISPLAY", server.socket, true);
	wlr_log(WLR_INFO, "running on WAYLAND_DISPLAY=%s", server.socket);

	/* Run autostart script from config directory */
	wm_autostart_run(server.config ? server.config->config_dir : NULL,
		server.socket);

	/* Run root command from config if set */
	if (server.config && server.config->root_command) {
		wm_autostart_run_cmd(server.config->root_command,
			server.socket);
	}

	/* Run -s/--startup command if provided */
	if (startup_cmd) {
		wm_autostart_run_cmd(startup_cmd, server.socket);
	}

	wm_server_run(&server);
	wm_server_destroy(&server);

	return 0;
}
