/*
 * wm-wayland-ctl - Command-line IPC client for wm-wayland
 *
 * Standalone binary with no wlroots dependency.
 * Connects to the compositor's Unix socket and sends JSON commands.
 *
 * Usage:
 *   wm-wayland-ctl get_workspaces
 *   wm-wayland-ctl get_windows
 *   wm-wayland-ctl get_outputs
 *   wm-wayland-ctl get_config
 *   wm-wayland-ctl command Close
 *   wm-wayland-ctl command 'Exec foot'
 *   wm-wayland-ctl subscribe window workspace
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

static const char usage[] =
	"Usage: wm-wayland-ctl [options] <command> [args...]\n"
	"\n"
	"Commands:\n"
	"  get_workspaces       List all workspaces\n"
	"  get_windows          List all windows\n"
	"  get_outputs          List all outputs\n"
	"  get_config           Show current configuration\n"
	"  command <action>     Execute a window manager action\n"
	"  subscribe <events>   Subscribe to events (window, workspace, output)\n"
	"  ping                 Test connectivity\n"
	"\n"
	"Options:\n"
	"  -s <path>  Socket path (default: auto-detect)\n"
	"  -r         Raw output (no pretty printing)\n"
	"  -h         Show this help\n"
	"\n"
	"Examples:\n"
	"  wm-wayland-ctl command Close\n"
	"  wm-wayland-ctl command 'Exec foot'\n"
	"  wm-wayland-ctl command 'Workspace 2'\n"
	"  wm-wayland-ctl subscribe window workspace\n";

/* Escape a string for JSON. Returns malloc'd string. */
static char *
json_escape_str(const char *s)
{
	size_t len = strlen(s);
	size_t cap = len * 2 + 3;
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
		default:   *p++ = (char)c;            break;
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
	/* First try WM_WAYLAND_SOCK */
	const char *sock = getenv("WM_WAYLAND_SOCK");
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

	size_t len = strlen(runtime) + strlen("/wm-wayland.") +
		strlen(display) + strlen(".sock") + 1;
	char *path = malloc(len);
	if (path) {
		snprintf(path, len, "%s/wm-wayland.%s.sock",
			runtime, display);
	}
	return path;
}

static int
connect_socket(const char *path)
{
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) {
		fprintf(stderr, "error: socket(): %s\n", strerror(errno));
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
		fprintf(stderr, "error: connect(%s): %s\n",
			path, strerror(errno));
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

/* Build a JSON request from command and arguments */
static char *
build_request(const char *command, int argc, char **argv)
{
	size_t cap = 256;
	char *buf = malloc(cap);
	if (!buf) return NULL;

	if (strcmp(command, "command") == 0) {
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
