/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * ipc.c - Unix socket IPC server
 *
 * Creates a Unix domain socket for external tools (fluxland-ctl)
 * to query compositor state and execute commands.
 */

#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <wlr/util/log.h>

#include "ipc.h"
#include "server.h"

#define IPC_READ_BUF_INITIAL 1024
#define IPC_READ_BUF_MAX     (64 * 1024)

static void
ipc_client_destroy(struct wm_ipc_client *client)
{
	wl_event_source_remove(client->event_source);
	close(client->fd);
	wl_list_remove(&client->link);
	free(client->read_buf);
	free(client);
}

static bool
ipc_send(int fd, const char *data, size_t len)
{
	size_t sent = 0;
	while (sent < len) {
		ssize_t n = write(fd, data + sent, len - sent);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				continue;
			return false;
		}
		sent += (size_t)n;
	}
	return true;
}

static void
ipc_send_response(struct wm_ipc_client *client, const char *json)
{
	size_t len = strlen(json);
	ipc_send(client->fd, json, len);
	if (len == 0 || json[len - 1] != '\n') {
		ipc_send(client->fd, "\n", 1);
	}
}

static void
process_line(struct wm_ipc_client *client, const char *line)
{
	char *response = wm_ipc_handle_command(client->ipc, client, line);
	if (response) {
		ipc_send_response(client, response);
		free(response);
	}
}

static int
handle_client_readable(int fd, uint32_t mask, void *data)
{
	struct wm_ipc_client *client = data;

	if (mask & (WL_EVENT_HANGUP | WL_EVENT_ERROR)) {
		wlr_log(WLR_DEBUG, "IPC client disconnected (fd %d)", fd);
		ipc_client_destroy(client);
		return 0;
	}

	/* Ensure buffer space */
	if (client->read_len >= client->read_cap) {
		if (client->read_cap >= IPC_READ_BUF_MAX) {
			wlr_log(WLR_ERROR, "IPC client buffer overflow, fd %d", fd);
			ipc_client_destroy(client);
			return 0;
		}
		size_t new_cap = client->read_cap * 2;
		if (new_cap > IPC_READ_BUF_MAX)
			new_cap = IPC_READ_BUF_MAX;
		char *new_buf = realloc(client->read_buf, new_cap);
		if (!new_buf) {
			ipc_client_destroy(client);
			return 0;
		}
		client->read_buf = new_buf;
		client->read_cap = new_cap;
	}

	ssize_t n = read(fd, client->read_buf + client->read_len,
		client->read_cap - client->read_len);
	if (n <= 0) {
		if (n < 0 && (errno == EINTR || errno == EAGAIN))
			return 0;
		wlr_log(WLR_DEBUG, "IPC client closed (fd %d)", fd);
		ipc_client_destroy(client);
		return 0;
	}
	client->read_len += (size_t)n;

	/* Process complete newline-delimited lines */
	char *start = client->read_buf;
	char *end = client->read_buf + client->read_len;
	char *nl;
	while ((nl = memchr(start, '\n', (size_t)(end - start))) != NULL) {
		*nl = '\0';
		process_line(client, start);
		start = nl + 1;
	}

	/* Move remaining partial data to front of buffer */
	size_t remaining = (size_t)(end - start);
	if (remaining > 0 && start != client->read_buf) {
		memmove(client->read_buf, start, remaining);
	}
	client->read_len = remaining;

	return 0;
}

static int
handle_new_connection(int fd, uint32_t mask, void *data)
{
	struct wm_ipc_server *ipc = data;

	(void)fd;
	(void)mask;

	int client_fd = accept(ipc->socket_fd, NULL, NULL);
	if (client_fd < 0) {
		wlr_log(WLR_ERROR, "IPC accept failed: %s", strerror(errno));
		return 0;
	}

	/* Set non-blocking */
	int flags = fcntl(client_fd, F_GETFL);
	if (flags >= 0) {
		fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
	}

	struct wm_ipc_client *client = calloc(1, sizeof(*client));
	if (!client) {
		close(client_fd);
		return 0;
	}

	client->ipc = ipc;
	client->fd = client_fd;
	client->subscribed_events = 0;
	client->read_buf = malloc(IPC_READ_BUF_INITIAL);
	client->read_len = 0;
	client->read_cap = IPC_READ_BUF_INITIAL;
	if (!client->read_buf) {
		free(client);
		close(client_fd);
		return 0;
	}

	struct wl_event_loop *loop =
		wl_display_get_event_loop(ipc->server->wl_display);
	client->event_source = wl_event_loop_add_fd(loop, client_fd,
		WL_EVENT_READABLE | WL_EVENT_HANGUP,
		handle_client_readable, client);

	wl_list_insert(&ipc->clients, &client->link);
	wlr_log(WLR_DEBUG, "IPC client connected (fd %d)", client_fd);

	return 0;
}

int
wm_ipc_init(struct wm_ipc_server *ipc, struct wm_server *server)
{
	memset(ipc, 0, sizeof(*ipc));
	ipc->server = server;
	ipc->socket_fd = -1;
	wl_list_init(&ipc->clients);

	/* Build socket path: $XDG_RUNTIME_DIR/fluxland.$WAYLAND_DISPLAY.sock */
	const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
	if (!runtime_dir) {
		wlr_log(WLR_ERROR, "IPC: %s", "XDG_RUNTIME_DIR not set");
		return -1;
	}

	const char *wl_display = server->socket;
	if (!wl_display) {
		wl_display = getenv("WAYLAND_DISPLAY");
	}
	if (!wl_display) {
		wl_display = "wayland-0";
	}

	size_t path_len = strlen(runtime_dir) + strlen("/fluxland.") +
		strlen(wl_display) + strlen(".sock") + 1;
	ipc->socket_path = malloc(path_len);
	if (!ipc->socket_path) {
		return -1;
	}
	snprintf(ipc->socket_path, path_len, "%s/fluxland.%s.sock",
		runtime_dir, wl_display);

	/* Remove stale socket */
	unlink(ipc->socket_path);

	/* Create Unix domain socket */
	ipc->socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (ipc->socket_fd < 0) {
		wlr_log(WLR_ERROR, "IPC: socket() failed: %s",
			strerror(errno));
		goto err;
	}

	/* Set non-blocking */
	int flags = fcntl(ipc->socket_fd, F_GETFL);
	if (flags >= 0) {
		fcntl(ipc->socket_fd, F_SETFL, flags | O_NONBLOCK);
	}

	struct sockaddr_un addr = {0};
	addr.sun_family = AF_UNIX;
	if (strlen(ipc->socket_path) >= sizeof(addr.sun_path)) {
		wlr_log(WLR_ERROR, "IPC: socket path too long: %s",
			ipc->socket_path);
		goto err;
	}
	strncpy(addr.sun_path, ipc->socket_path, sizeof(addr.sun_path) - 1);

	if (bind(ipc->socket_fd, (struct sockaddr *)&addr,
			sizeof(addr)) < 0) {
		wlr_log(WLR_ERROR, "IPC: bind() failed: %s",
			strerror(errno));
		goto err;
	}

	if (listen(ipc->socket_fd, 8) < 0) {
		wlr_log(WLR_ERROR, "IPC: listen() failed: %s",
			strerror(errno));
		goto err;
	}

	/* Register with wayland event loop */
	struct wl_event_loop *loop =
		wl_display_get_event_loop(server->wl_display);
	ipc->event_source = wl_event_loop_add_fd(loop, ipc->socket_fd,
		WL_EVENT_READABLE, handle_new_connection, ipc);

	/* Set environment variable for child processes */
	setenv("FLUXLAND_SOCK", ipc->socket_path, 1);

	wlr_log(WLR_INFO, "IPC server listening on %s", ipc->socket_path);
	return 0;

err:
	if (ipc->socket_fd >= 0) {
		close(ipc->socket_fd);
		ipc->socket_fd = -1;
	}
	free(ipc->socket_path);
	ipc->socket_path = NULL;
	return -1;
}

void
wm_ipc_destroy(struct wm_ipc_server *ipc)
{
	/* Close all connected clients */
	struct wm_ipc_client *client, *tmp;
	wl_list_for_each_safe(client, tmp, &ipc->clients, link) {
		ipc_client_destroy(client);
	}

	if (ipc->event_source) {
		wl_event_source_remove(ipc->event_source);
	}
	if (ipc->socket_fd >= 0) {
		close(ipc->socket_fd);
	}

	if (ipc->socket_path) {
		unlink(ipc->socket_path);
		free(ipc->socket_path);
	}

	unsetenv("FLUXLAND_SOCK");
}

void
wm_ipc_broadcast_event(struct wm_ipc_server *ipc,
	enum wm_ipc_event event, const char *json_event)
{
	struct wm_ipc_client *client, *tmp;
	wl_list_for_each_safe(client, tmp, &ipc->clients, link) {
		if (client->subscribed_events & event) {
			size_t len = strlen(json_event);
			if (!ipc_send(client->fd, json_event, len)) {
				ipc_client_destroy(client);
				continue;
			}
			if (len == 0 || json_event[len - 1] != '\n') {
				if (!ipc_send(client->fd, "\n", 1)) {
					ipc_client_destroy(client);
				}
			}
		}
	}
}
