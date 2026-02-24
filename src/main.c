/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * main.c - Entry point
 */

#define _POSIX_C_SOURCE 200809L
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <wlr/util/log.h>

#include "autostart.h"
#include "config.h"
#include "server.h"

static const char usage[] =
	"Usage: fluxland [options...]\n"
	"  -s, --startup <cmd>  Run command on startup\n"
	"  -d, --debug          Enable debug logging\n"
	"  -v, --version        Show version and exit\n"
	"  -h, --help           Show this help\n";

static const struct option long_options[] = {
	{"startup", required_argument, NULL, 's'},
	{"debug",   no_argument,       NULL, 'd'},
	{"version", no_argument,       NULL, 'v'},
	{"help",    no_argument,       NULL, 'h'},
	{0, 0, 0, 0},
};

int
main(int argc, char *argv[])
{
	char *startup_cmd = NULL;
	enum wlr_log_importance verbosity = WLR_ERROR;

	int c;
	while ((c = getopt_long(argc, argv, "s:dvh", long_options,
			NULL)) != -1) {
		switch (c) {
		case 's':
			startup_cmd = optarg;
			break;
		case 'd':
			verbosity = WLR_DEBUG;
			break;
		case 'v':
			printf("fluxland %s\n", FLUXLAND_VERSION);
			return 0;
		case 'h':
		default:
			printf("%s", usage);
			return 0;
		}
	}

	wlr_log_init(verbosity, NULL);

	if (!getenv("XDG_RUNTIME_DIR")) {
		wlr_log(WLR_ERROR, "%s", "XDG_RUNTIME_DIR is not set");
		return 1;
	}

	struct wm_server server = {0};
	if (!wm_server_init(&server)) {
		wlr_log(WLR_ERROR, "%s", "failed to initialize server");
		return 1;
	}

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
