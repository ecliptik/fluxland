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
#include "import.h"
#include "i18n.h"
#include "keybind.h"
#include "server.h"
#include "session_env.h"
#include <wlr/version.h>
#include "validate.h"

static const char usage[] = N_(
	"Usage: fluxland [options...]\n"
	"  -s, --startup <cmd>  Run command on startup\n"
	"  -c, --check-config   Validate config and exit\n"
	"  -d, --debug          Enable debug logging\n"
	"  -v, --version        Show version and exit\n"
	"  -l, --list-commands  List available commands and exit\n"
	"  --ipc-no-exec        Disable Exec/BindKey/SetStyle via IPC\n"
	"  --import-fluxbox[=DIR]  Make ~/.config/fluxland from a Fluxbox\n"
	"                       configuration (~/.fluxbox) and exit\n"
	"  --force              With --import-fluxbox: replace files\n"
	"  -h, --help           Show this help\n");

enum {
	OPT_IPC_NO_EXEC = 256,
	OPT_IMPORT_FLUXBOX,
	OPT_FORCE,
};

static const struct option long_options[] = {
	{"startup",      required_argument, NULL, 's'},
	{"check-config", no_argument,       NULL, 'c'},
	{"debug",        no_argument,       NULL, 'd'},
	{"version",      no_argument,       NULL, 'v'},
	{"list-commands", no_argument,      NULL, 'l'},
	{"ipc-no-exec",  no_argument,       NULL, OPT_IPC_NO_EXEC},
	{"import-fluxbox", optional_argument, NULL, OPT_IMPORT_FLUXBOX},
	{"force",        no_argument,       NULL, OPT_FORCE},
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
	bool import = false, force = false;
	const char *import_from = NULL;
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
		case OPT_IMPORT_FLUXBOX:
			import = true;
			import_from = optarg;
			break;
		case OPT_FORCE:
			force = true;
			break;
		case 'v':
			printf("fluxland %s (wlroots %s)\n", FLUXLAND_VERSION,
				WLR_VERSION_STR);
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

	if (import) {
		int ret = wm_import_fluxbox(import_from, NULL, force, stdout);
		if (ret != 0)
			return ret;
		/* What fluxland makes of it now */
		struct wm_config *cfg = config_create();
		if (cfg && cfg->config_dir) {
			printf("\n");
			fflush(stdout);
			ret = validate_config(cfg->config_dir);
		}
		config_destroy(cfg);
		return ret;
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

	/* Nested in another session, or headless (tests, CI) */
	bool nested = getenv("WAYLAND_DISPLAY") || getenv("DISPLAY");
	const char *backends = getenv("WLR_BACKENDS");
	bool headless = backends && strstr(backends, "headless");
	wm_session_env_defaults();

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

	/* Portals and other user services find the session this way */
	int mode = server.config ? server.config->update_activation_env : -1;
	bool exported = wm_session_env_should_export(
		mode == 1 ? WM_ACTIVATION_ENV_ALWAYS :
		mode == 0 ? WM_ACTIVATION_ENV_NEVER : WM_ACTIVATION_ENV_AUTO,
		nested, headless);
	if (exported)
		wm_session_env_export(false);

	/* Run autostart script from config directory */
	wm_autostart_run(server.config ? server.config->config_dir : NULL,
		server.socket);

	/* The apps file's [startup] commands */
	for (int i = 0; i < server.rules.startup_count; i++)
		wm_autostart_run_cmd(server.rules.startup[i], server.socket);

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
	if (exported)
		wm_session_env_export(true);
	wm_server_destroy(&server);

	return 0;
}
