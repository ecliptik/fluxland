// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The Fluxland Contributors
/*
 * fluxland-ctl - Command-line IPC client for fluxland
 *
 * Standalone binary with no wlroots dependency.
 * Connects to the compositor's Unix socket and sends JSON commands.
 *
 * Usage:
 *   fluxland-ctl action Close
 *   fluxland-ctl action Workspace 3
 *   fluxland-ctl get_windows
 *   fluxland-ctl get_workspaces
 *   fluxland-ctl get_outputs
 *   fluxland-ctl get_config
 *   fluxland-ctl subscribe window workspace
 *   fluxland-ctl list-actions
 */

#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define MAX_RESPONSE_SIZE (10 * 1024 * 1024) /* 10 MiB */

static const char usage[] =
	"Usage: fluxland-ctl [options] <command> [args...]\n"
	"\n"
	"Commands:\n"
	"  action <name> [arg]    Execute a compositor action\n"
	"  get_windows            List all managed windows (JSON)\n"
	"  get_workspaces         List workspaces and current workspace (JSON)\n"
	"  get_outputs            List outputs/monitors (JSON)\n"
	"  get_config             Dump current configuration (JSON)\n"
	"  subscribe <type>       Subscribe to events (window, workspace, output)\n"
	"  list-actions           Print all available action names\n"
	"  ping                   Test connectivity\n"
	"\n"
	"Options:\n"
	"  -s <path>  Socket path (default: auto-detect via $FLUXLAND_SOCK\n"
	"             or $XDG_RUNTIME_DIR/fluxland.$WAYLAND_DISPLAY.sock)\n"
	"  -r         Raw output (no pretty printing)\n"
	"  -h         Show this help\n"
	"\n"
	"Examples:\n"
	"  fluxland-ctl action Close\n"
	"  fluxland-ctl action Workspace 3\n"
	"  fluxland-ctl action Exec foot\n"
	"  fluxland-ctl action ArrangeWindows\n"
	"  fluxland-ctl get_windows\n"
	"  fluxland-ctl subscribe window\n"
	"  fluxland-ctl list-actions\n";

/* Available actions table — must match ipc_commands.c action_table */
struct action_info {
	const char *name;
	const char *description;
};

static const struct action_info actions[] = {
	{"Exec <cmd>",          "Execute a shell command"},
	{"Close",               "Close the focused window"},
	{"Kill",                "Force-kill the focused window's process"},
	{"Maximize",            "Toggle maximize on the focused window"},
	{"MaximizeVertical",    "Toggle vertical maximize"},
	{"MaximizeHorizontal",  "Toggle horizontal maximize"},
	{"Fullscreen",          "Toggle fullscreen on the focused window"},
	{"Minimize",            "Minimize (iconify) the focused window"},
	{"Iconify",             "Alias for Minimize"},
	{"Shade",               "Toggle window shading (collapse to titlebar)"},
	{"ShadeOn",             "Shade the focused window"},
	{"ShadeOff",            "Unshade the focused window"},
	{"Stick",               "Toggle sticky (visible on all workspaces)"},
	{"Move",                "Begin interactive move"},
	{"Resize",              "Begin interactive resize"},
	{"Raise",               "Raise the focused window"},
	{"Lower",               "Lower the focused window"},
	{"RaiseLayer",          "Move window up one layer"},
	{"LowerLayer",          "Move window down one layer"},
	{"SetLayer <n>",        "Set window layer explicitly"},
	{"FocusNext",           "Focus the next window"},
	{"NextWindow",          "Alias for FocusNext"},
	{"FocusPrev",           "Focus the previous window"},
	{"PrevWindow",          "Alias for FocusPrev"},
	{"Workspace <n>",       "Switch to workspace N (1-based)"},
	{"SendToWorkspace <n>", "Send focused window to workspace N"},
	{"NextWorkspace",       "Switch to the next workspace"},
	{"PrevWorkspace",       "Switch to the previous workspace"},
	{"ShowDesktop",         "Toggle show desktop (minimize all)"},
	{"MoveLeft <px>",       "Move focused window left by px pixels"},
	{"MoveRight <px>",      "Move focused window right by px pixels"},
	{"MoveUp <px>",         "Move focused window up by px pixels"},
	{"MoveDown <px>",       "Move focused window down by px pixels"},
	{"MoveTo <x> <y>",      "Move focused window to absolute position"},
	{"ResizeTo <w> <h>",    "Resize focused window to width x height"},
	{"ToggleDecor",         "Toggle window decorations"},
	{"SetDecor <style>",    "Set decoration style"},
	{"KeyMode <mode>",      "Switch to a named key binding mode"},
	{"NextTab",             "Switch to the next tab in tab group"},
	{"PrevTab",             "Switch to the previous tab in tab group"},
	{"DetachClient",        "Detach window from its tab group"},
	{"MoveTabLeft",         "Move tab position left"},
	{"MoveTabRight",        "Move tab position right"},
	{"ActivateTab <n>",     "Activate tab N in tab group"},
	{"RootMenu",            "Show the root (desktop) menu"},
	{"WindowMenu",          "Show the window menu"},
	{"HideMenus",           "Hide all open menus"},
	{"ArrangeWindows",      "Arrange windows in a grid layout"},
	{"ArrangeVertical",     "Arrange windows in vertical columns"},
	{"ArrangeHorizontal",   "Arrange windows in horizontal rows"},
	{"CascadeWindows",      "Cascade windows diagonally"},
	{"Reconfigure",         "Reload configuration from disk"},
	{"Exit",                "Exit the compositor"},
	{NULL, NULL},
};

/* List of known IPC commands for validation */
static const char *known_commands[] = {
	"action", "command", "get_windows", "get_workspaces", "get_outputs",
	"get_config", "subscribe", "list-actions", "ping", NULL,
};

static int
is_known_command(const char *cmd)
{
	for (const char **p = known_commands; *p; p++) {
		if (strcmp(cmd, *p) == 0)
			return 1;
	}
	return 0;
}

static void
print_actions(void)
{
	printf("Available actions:\n");
	for (const struct action_info *a = actions; a->name; a++) {
		printf("  %-24s %s\n", a->name, a->description);
	}
}

/* Escape a string for JSON. Returns malloc'd string. */
static char *
json_escape_str(const char *s)
{
	size_t len = strlen(s);
	size_t cap = len * 6 + 3;
	char *out = malloc(cap);
	if (!out) return NULL;

	char *p = out;
	*p++ = '"';
	for (size_t i = 0; i < len; i++) {
		unsigned char c = (unsigned char)s[i];
		switch (c) {
		case '"':  *p++ = '\\'; *p++ = '"';  break;
		case '\\': *p++ = '\\'; *p++ = '\\'; break;
		case '\n': *p++ = '\\'; *p++ = 'n';  break;
		case '\r': *p++ = '\\'; *p++ = 'r';  break;
		case '\t': *p++ = '\\'; *p++ = 't';  break;
		default:
			if (c < 0x20) {
				p += sprintf(p, "\\u%04x", c);
			} else {
				*p++ = (char)c;
			}
			break;
		}
	}
	*p++ = '"';
	*p = '\0';
	return out;
}

/* Auto-detect socket path from environment */
static char *
find_socket_path(void)
{
	/* First try FLUXLAND_SOCK */
	const char *sock = getenv("FLUXLAND_SOCK");
	if (sock) return strdup(sock);

	/* Build from XDG_RUNTIME_DIR and WAYLAND_DISPLAY */
	const char *runtime = getenv("XDG_RUNTIME_DIR");
	const char *display = getenv("WAYLAND_DISPLAY");

	if (!runtime) {
		fprintf(stderr, "error: XDG_RUNTIME_DIR not set\n");
		return NULL;
	}
	if (!display) {
		display = "wayland-0";
	}

	size_t len = strlen(runtime) + strlen("/fluxland.") +
		strlen(display) + strlen(".sock") + 1;
	char *path = malloc(len);
	if (path) {
		snprintf(path, len, "%s/fluxland.%s.sock",
			runtime, display);
	}
	return path;
}

static int
connect_socket(const char *path)
{
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) {
		fprintf(stderr,
			"Cannot connect to fluxland. Is the compositor running?\n");
		return -1;
	}

	struct sockaddr_un addr = {0};
	addr.sun_family = AF_UNIX;
	if (strlen(path) >= sizeof(addr.sun_path)) {
		fprintf(stderr, "error: socket path too long\n");
		close(fd);
		return -1;
	}
	strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		fprintf(stderr,
			"Cannot connect to fluxland. Is the compositor running?\n"
			"  (socket: %s)\n", path);
		close(fd);
		return -1;
	}

	return fd;
}

static int
send_all(int fd, const char *data, size_t len)
{
	size_t sent = 0;
	while (sent < len) {
		ssize_t n = write(fd, data + sent, len - sent);
		if (n < 0) {
			if (errno == EINTR) continue;
			return -1;
		}
		sent += (size_t)n;
	}
	return 0;
}

/* Read a single newline-terminated JSON response */
static char *
read_response(int fd)
{
	size_t cap = 4096;
	size_t len = 0;
	char *buf = malloc(cap);
	if (!buf) return NULL;

	while (1) {
		if (len >= cap - 1) {
			cap *= 2;
			if (cap > MAX_RESPONSE_SIZE) {
				free(buf);
				return NULL;
			}
			char *new_buf = realloc(buf, cap);
			if (!new_buf) { free(buf); return NULL; }
			buf = new_buf;
		}

		ssize_t n = read(fd, buf + len, cap - len - 1);
		if (n < 0) {
			if (errno == EINTR) continue;
			free(buf);
			return NULL;
		}
		if (n == 0) break;
		len += (size_t)n;
		buf[len] = '\0';

		/* Check for newline (end of message) */
		if (memchr(buf, '\n', len)) break;
	}

	buf[len] = '\0';
	return buf;
}

/* Build a JSON request from command and arguments.
 * The wire command for "action" is "command" (compositor expects "command"). */
static char *
build_request(const char *command, int argc, char **argv)
{
	size_t cap = 256;
	char *buf = malloc(cap);
	if (!buf) return NULL;

	/* Both "action" and "command" map to the IPC "command" verb */
	if (strcmp(command, "command") == 0 ||
	    strcmp(command, "action") == 0) {
		/* Build action string from remaining args */
		size_t action_len = 0;
		for (int i = 0; i < argc; i++) {
			action_len += strlen(argv[i]) + 1;
		}
		char *action = malloc(action_len + 1);
		if (!action) { free(buf); return NULL; }
		action[0] = '\0';
		for (int i = 0; i < argc; i++) {
			if (i > 0) strcat(action, " ");
			strcat(action, argv[i]);
		}

		char *escaped = json_escape_str(action);
		free(action);
		if (!escaped) { free(buf); return NULL; }

		size_t needed = strlen(escaped) + 64;
		if (needed > cap) {
			cap = needed;
			char *new_buf = realloc(buf, cap);
			if (!new_buf) { free(buf); free(escaped); return NULL; }
			buf = new_buf;
		}
		snprintf(buf, cap,
			"{\"command\":\"command\",\"action\":%s}\n",
			escaped);
		free(escaped);
	} else if (strcmp(command, "subscribe") == 0) {
		/* Build events string from remaining args */
		size_t events_len = 0;
		for (int i = 0; i < argc; i++) {
			events_len += strlen(argv[i]) + 1;
		}
		char *events = malloc(events_len + 1);
		if (!events) { free(buf); return NULL; }
		events[0] = '\0';
		for (int i = 0; i < argc; i++) {
			if (i > 0) strcat(events, ",");
			strcat(events, argv[i]);
		}

		char *escaped = json_escape_str(events);
		free(events);
		if (!escaped) { free(buf); return NULL; }

		size_t needed = strlen(escaped) + 64;
		if (needed > cap) {
			cap = needed;
			char *new_buf = realloc(buf, cap);
			if (!new_buf) { free(buf); free(escaped); return NULL; }
			buf = new_buf;
		}
		snprintf(buf, cap,
			"{\"command\":\"subscribe\",\"events\":%s}\n",
			escaped);
		free(escaped);
	} else {
		/* Simple command with no args */
		snprintf(buf, cap, "{\"command\":\"%s\"}\n", command);
	}

	return buf;
}

static void
subscribe_loop(int fd)
{
	/* After subscribe command, keep reading events */
	char buf[4096];
	struct pollfd pfd = { .fd = fd, .events = POLLIN };

	while (1) {
		int ret = poll(&pfd, 1, -1);
		if (ret < 0) {
			if (errno == EINTR) continue;
			break;
		}
		if (pfd.revents & (POLLERR | POLLHUP)) break;
		if (pfd.revents & POLLIN) {
			ssize_t n = read(fd, buf, sizeof(buf) - 1);
			if (n <= 0) break;
			buf[n] = '\0';
			/* Print each line (may contain multiple events) */
			printf("%s", buf);
			fflush(stdout);
		}
	}
}

int
main(int argc, char *argv[])
{
	char *socket_path = NULL;
	int opt;

	while ((opt = getopt(argc, argv, "s:rh")) != -1) {
		switch (opt) {
		case 's':
			socket_path = strdup(optarg);
			break;
		case 'r':
			/* raw mode is default currently */
			break;
		case 'h':
			printf("%s", usage);
			return 0;
		default:
			fprintf(stderr, "%s", usage);
			return 1;
		}
	}

	if (optind >= argc) {
		fprintf(stderr, "error: no command specified\n\n%s", usage);
		free(socket_path);
		return 1;
	}

	const char *command = argv[optind];
	int cmd_argc = argc - optind - 1;
	char **cmd_argv = argv + optind + 1;

	/* Handle list-actions locally (no socket needed) */
	if (strcmp(command, "list-actions") == 0) {
		print_actions();
		free(socket_path);
		return 0;
	}

	/* Validate command name */
	if (!is_known_command(command)) {
		fprintf(stderr,
			"Unknown command '%s'. "
			"Run 'fluxland-ctl --help' for usage.\n",
			command);
		free(socket_path);
		return 1;
	}

	/* Find socket path */
	if (!socket_path) {
		socket_path = find_socket_path();
		if (!socket_path) return 1;
	}

	/* Connect */
	int fd = connect_socket(socket_path);
	if (fd < 0) {
		free(socket_path);
		return 1;
	}

	/* Build and send request */
	char *request = build_request(command, cmd_argc, cmd_argv);
	if (!request) {
		fprintf(stderr, "error: failed to build request\n");
		close(fd);
		free(socket_path);
		return 1;
	}

	if (send_all(fd, request, strlen(request)) < 0) {
		fprintf(stderr, "error: send failed: %s\n", strerror(errno));
		free(request);
		close(fd);
		free(socket_path);
		return 1;
	}
	free(request);

	/* Read response */
	char *response = read_response(fd);
	if (response) {
		/* Print response, trimming trailing whitespace */
		size_t len = strlen(response);
		while (len > 0 && (response[len-1] == '\n' ||
				   response[len-1] == '\r'))
			len--;
		response[len] = '\0';
		printf("%s\n", response);
		free(response);
	}

	/* For subscribe, stay connected and print events */
	if (strcmp(command, "subscribe") == 0) {
		subscribe_loop(fd);
	}

	close(fd);
	free(socket_path);
	return 0;
}
