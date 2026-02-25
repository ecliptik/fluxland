/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 *
 * ipc.h - Unix socket IPC server for external control and scripting
 *
 * Protocol: JSON-based, newline-delimited
 *   Request:  { "command": "name", "args": {...} }\n
 *   Response: { "success": true, "data": {...} }\n
 */

#ifndef WM_IPC_H
#define WM_IPC_H

#include <stdint.h>
#include <time.h>
#include <wayland-server-core.h>

struct wm_server;
struct wm_ipc_server;

/* Event types for subscription */
enum wm_ipc_event {
	WM_IPC_EVENT_WINDOW_OPEN    = (1 << 0),
	WM_IPC_EVENT_WINDOW_CLOSE   = (1 << 1),
	WM_IPC_EVENT_WINDOW_FOCUS   = (1 << 2),
	WM_IPC_EVENT_WINDOW_TITLE   = (1 << 3),
	WM_IPC_EVENT_WORKSPACE      = (1 << 4),
	WM_IPC_EVENT_OUTPUT_ADD     = (1 << 5),
	WM_IPC_EVENT_OUTPUT_REMOVE  = (1 << 6),
	WM_IPC_EVENT_STYLE_CHANGED  = (1 << 7),
	WM_IPC_EVENT_CONFIG_RELOADED = (1 << 8),
	WM_IPC_EVENT_FOCUS_CHANGED  = (1 << 9),
	WM_IPC_EVENT_MENU           = (1 << 10),
};

struct wm_ipc_client {
	struct wm_ipc_server *ipc;
	int fd;
	struct wl_event_source *event_source;
	struct wl_list link; /* wm_ipc_server.clients */
	uint32_t subscribed_events;
	/* Read buffer for partial lines */
	char *read_buf;
	size_t read_len;
	size_t read_cap;
};

/* Rate limiting for high-frequency IPC event broadcasts */
#define IPC_EVENT_THROTTLE_MS 16 /* ~60fps for high-frequency events */

struct ipc_event_throttle {
	struct timespec last_sent;
};

struct wm_ipc_server {
	struct wm_server *server;
	int socket_fd;
	char *socket_path;
	struct wl_event_source *event_source;
	struct wl_list clients; /* wm_ipc_client.link */
	int client_count;
	struct ipc_event_throttle event_throttle[32]; /* one per event bit */
};

/* Initialize the IPC server. Creates the Unix socket and registers
 * with the wayland event loop. Returns 0 on success, -1 on error. */
int wm_ipc_init(struct wm_ipc_server *ipc, struct wm_server *server);

/* Tear down the IPC server: close all clients, unlink socket. */
void wm_ipc_destroy(struct wm_ipc_server *ipc);

/* Broadcast an event to all subscribed IPC clients.
 * json_event is a complete JSON string (no trailing newline needed). */
void wm_ipc_broadcast_event(struct wm_ipc_server *ipc,
	enum wm_ipc_event event, const char *json_event);

/* --- Command handling (implemented in ipc_commands.c) --- */

/* Process a single IPC command and return a malloc'd JSON response string.
 * Caller must free the returned string. */
char *wm_ipc_handle_command(struct wm_ipc_server *ipc,
	struct wm_ipc_client *client, const char *json_request);

#endif /* WM_IPC_H */
