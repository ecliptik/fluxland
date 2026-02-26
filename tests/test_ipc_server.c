/*
 * test_ipc_server.c - Unit tests for IPC server transport layer (ipc.c)
 *
 * Includes ipc.c directly with stub implementations to avoid
 * needing wlroots/wayland libraries at link time. Tests the socket
 * path generation, buffer management, event throttling, broadcast
 * logic, and client lifecycle.
 */

#define _GNU_SOURCE

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

/* --- Block real headers via their include guards --- */
#define WAYLAND_SERVER_CORE_H
#define WAYLAND_SERVER_H
#define WAYLAND_UTIL_H
#define WLR_UTIL_LOG_H
#define WM_SERVER_H
#define WM_IPC_H

/* --- Stub wayland types --- */

struct wl_list {
	struct wl_list *prev;
	struct wl_list *next;
};

static inline void
wl_list_init(struct wl_list *list)
{
	list->prev = list;
	list->next = list;
}

static inline void
wl_list_insert(struct wl_list *list, struct wl_list *elm)
{
	elm->prev = list;
	elm->next = list->next;
	list->next = elm;
	elm->next->prev = elm;
}

static inline void
wl_list_remove(struct wl_list *elm)
{
	elm->prev->next = elm->next;
	elm->next->prev = elm->prev;
	elm->prev = NULL;
	elm->next = NULL;
}

static inline int
wl_list_empty(const struct wl_list *list)
{
	return list->next == list;
}

#include <stddef.h>

#define wl_container_of(ptr, sample, member) \
	((void *)((char *)(ptr) - offsetof(struct wm_ipc_client, member)))

#define wl_list_for_each_safe(pos, tmp, head, member)			\
	for (pos = wl_container_of((head)->next, pos, member),		\
	     tmp = wl_container_of((pos)->member.next, tmp, member);	\
	     &(pos)->member != (head);					\
	     pos = tmp,							\
	     tmp = wl_container_of((pos)->member.next, tmp, member))

/* --- Stub wl_event_source and related --- */

struct wl_display {
	int dummy;
};

struct wl_event_source {
	int dummy;
	bool removed;
};

struct wl_event_loop {
	int dummy;
};

/* --- Wayland event constants --- */
#define WL_EVENT_READABLE 0x01
#define WL_EVENT_WRITABLE 0x02
#define WL_EVENT_HANGUP   0x04
#define WL_EVENT_ERROR    0x08

/* --- Stub wayland functions --- */

static struct wl_event_loop g_stub_event_loop;
static struct wl_event_source g_stub_event_sources[16];
static int g_stub_event_source_idx;
static int g_stub_event_source_remove_count;

static struct wl_event_loop *
wl_display_get_event_loop(struct wl_display *display)
{
	(void)display;
	return &g_stub_event_loop;
}

static struct wl_event_source *
wl_event_loop_add_fd(struct wl_event_loop *loop, int fd, uint32_t mask,
	int (*callback)(int fd, uint32_t mask, void *data), void *data)
{
	(void)loop;
	(void)fd;
	(void)mask;
	(void)callback;
	(void)data;
	if (g_stub_event_source_idx < 16) {
		struct wl_event_source *src =
			&g_stub_event_sources[g_stub_event_source_idx++];
		src->removed = false;
		return src;
	}
	return NULL;
}

static int
wl_event_source_remove(struct wl_event_source *source)
{
	if (source)
		source->removed = true;
	g_stub_event_source_remove_count++;
	return 0;
}

/* --- wlr_log no-op --- */
#define wlr_log(verb, fmt, ...) ((void)0)
#define WLR_ERROR 1
#define WLR_DEBUG 7
#define WLR_INFO  3

/* --- IPC event types (from real ipc.h) --- */

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

/* Rate limiting constant */
#define IPC_EVENT_THROTTLE_MS 16

struct ipc_event_throttle {
	struct timespec last_sent;
};

/* Forward declarations */
struct wm_ipc_server;

struct wm_ipc_client {
	struct wm_ipc_server *ipc;
	int fd;
	struct wl_event_source *event_source;
	struct wl_list link;
	uint32_t subscribed_events;
	char *read_buf;
	size_t read_len;
	size_t read_cap;
};

struct wm_ipc_server {
	struct wm_server *server;
	int socket_fd;
	char *socket_path;
	struct wl_event_source *event_source;
	struct wl_list clients;
	int client_count;
	struct ipc_event_throttle event_throttle[32];
};

/* --- Minimal wm_server type --- */

struct wm_server {
	struct wl_display *wl_display;
	const char *socket;
};

/* --- Stub for wm_ipc_handle_command (from ipc_commands.c) --- */

static int g_handle_command_count;
static char g_last_command[1024];

char *
wm_ipc_handle_command(struct wm_ipc_server *ipc,
	struct wm_ipc_client *client, const char *json_request)
{
	(void)ipc;
	(void)client;
	g_handle_command_count++;
	if (json_request) {
		snprintf(g_last_command, sizeof(g_last_command),
			"%s", json_request);
	}
	return strdup("{\"success\":true}");
}

/* --- Capture pipe for ipc_send testing --- */

static int g_pipe_fds[2]; /* [0]=read, [1]=write */

static void
create_test_pipe(void)
{
	int rc = pipe(g_pipe_fds);
	assert(rc == 0);
}

static char *
read_pipe_contents(size_t *out_len)
{
	/* Close write end so read sees EOF */
	close(g_pipe_fds[1]);
	g_pipe_fds[1] = -1;

	char buf[4096];
	size_t total = 0;
	ssize_t n;
	while ((n = read(g_pipe_fds[0], buf + total,
		sizeof(buf) - total)) > 0) {
		total += (size_t)n;
	}
	close(g_pipe_fds[0]);
	g_pipe_fds[0] = -1;

	char *result = malloc(total + 1);
	assert(result != NULL);
	memcpy(result, buf, total);
	result[total] = '\0';
	if (out_len)
		*out_len = total;
	return result;
}

/* --- Include ipc.c source directly --- */

#include "../../src/ipc.c"

/* --- Test helpers --- */

static struct wm_server test_server;
static struct wl_display test_display;

static void
reset_globals(void)
{
	g_handle_command_count = 0;
	memset(g_last_command, 0, sizeof(g_last_command));
	g_stub_event_source_idx = 0;
	g_stub_event_source_remove_count = 0;
	memset(g_stub_event_sources, 0, sizeof(g_stub_event_sources));
	memset(&test_server, 0, sizeof(test_server));
	memset(&test_display, 0, sizeof(test_display));
	test_server.wl_display = &test_display;
	test_server.socket = "wayland-test";
}

/*
 * Create a test client attached to an IPC server with a pipe-based fd
 * for capturing output.
 */
static struct wm_ipc_client *
make_test_client(struct wm_ipc_server *ipc, int fd, uint32_t events)
{
	struct wm_ipc_client *client = calloc(1, sizeof(*client));
	assert(client != NULL);
	client->ipc = ipc;
	client->fd = fd;
	client->subscribed_events = events;
	client->read_buf = malloc(1024);
	assert(client->read_buf != NULL);
	client->read_len = 0;
	client->read_cap = 1024;
	client->event_source = &g_stub_event_sources[g_stub_event_source_idx++];
	client->event_source->removed = false;
	wl_list_insert(&ipc->clients, &client->link);
	ipc->client_count++;
	return client;
}

/* ===== Tests ===== */

/* Test 1: ipc_send writes complete data to fd */
static void
test_ipc_send_basic(void)
{
	create_test_pipe();

	const char *msg = "hello world";
	bool ok = ipc_send(g_pipe_fds[1], msg, strlen(msg));
	assert(ok);

	size_t len;
	char *got = read_pipe_contents(&len);
	assert(len == strlen(msg));
	assert(strcmp(got, msg) == 0);
	free(got);

	printf("  PASS: test_ipc_send_basic\n");
}

/* Test 2: ipc_send handles empty data */
static void
test_ipc_send_empty(void)
{
	create_test_pipe();

	bool ok = ipc_send(g_pipe_fds[1], "", 0);
	assert(ok);

	size_t len;
	char *got = read_pipe_contents(&len);
	assert(len == 0);
	free(got);

	printf("  PASS: test_ipc_send_empty\n");
}

/* Test 3: ipc_send returns false on bad fd */
static void
test_ipc_send_bad_fd(void)
{
	bool ok = ipc_send(-1, "test", 4);
	assert(!ok);

	printf("  PASS: test_ipc_send_bad_fd\n");
}

/* Test 4: ipc_send_response appends newline if missing */
static void
test_ipc_send_response_adds_newline(void)
{
	create_test_pipe();

	struct wm_ipc_server ipc;
	memset(&ipc, 0, sizeof(ipc));
	wl_list_init(&ipc.clients);

	struct wm_ipc_client client;
	memset(&client, 0, sizeof(client));
	client.fd = g_pipe_fds[1];
	client.ipc = &ipc;

	ipc_send_response(&client, "{\"success\":true}");

	size_t len;
	char *got = read_pipe_contents(&len);
	assert(len > 0);
	assert(got[len - 1] == '\n');
	assert(strstr(got, "{\"success\":true}") != NULL);
	free(got);

	printf("  PASS: test_ipc_send_response_adds_newline\n");
}

/* Test 5: ipc_send_response does not double-add newline */
static void
test_ipc_send_response_preserves_newline(void)
{
	create_test_pipe();

	struct wm_ipc_server ipc;
	memset(&ipc, 0, sizeof(ipc));
	wl_list_init(&ipc.clients);

	struct wm_ipc_client client;
	memset(&client, 0, sizeof(client));
	client.fd = g_pipe_fds[1];
	client.ipc = &ipc;

	ipc_send_response(&client, "{\"success\":true}\n");

	size_t len;
	char *got = read_pipe_contents(&len);
	/* Should be exactly the original string (one newline, not two) */
	assert(strcmp(got, "{\"success\":true}\n") == 0);
	free(got);

	printf("  PASS: test_ipc_send_response_preserves_newline\n");
}

/* Test 6: should_throttle_event returns false for non-throttled events */
static void
test_throttle_non_throttled_event(void)
{
	struct wm_ipc_server ipc;
	memset(&ipc, 0, sizeof(ipc));

	/* WINDOW_OPEN is not in the throttled set */
	bool throttled = should_throttle_event(&ipc, WM_IPC_EVENT_WINDOW_OPEN);
	assert(!throttled);

	/* WINDOW_CLOSE is not in the throttled set */
	throttled = should_throttle_event(&ipc, WM_IPC_EVENT_WINDOW_CLOSE);
	assert(!throttled);

	/* STYLE_CHANGED is not throttled */
	throttled = should_throttle_event(&ipc, WM_IPC_EVENT_STYLE_CHANGED);
	assert(!throttled);

	printf("  PASS: test_throttle_non_throttled_event\n");
}

/* Test 7: should_throttle_event allows first call for throttled events */
static void
test_throttle_first_call_allowed(void)
{
	struct wm_ipc_server ipc;
	memset(&ipc, 0, sizeof(ipc));

	/* First call for WINDOW_FOCUS should not be throttled */
	bool throttled = should_throttle_event(&ipc, WM_IPC_EVENT_WINDOW_FOCUS);
	assert(!throttled);

	printf("  PASS: test_throttle_first_call_allowed\n");
}

/* Test 8: should_throttle_event throttles rapid successive calls */
static void
test_throttle_rapid_calls(void)
{
	struct wm_ipc_server ipc;
	memset(&ipc, 0, sizeof(ipc));

	/* First call succeeds */
	bool throttled = should_throttle_event(&ipc, WM_IPC_EVENT_WINDOW_FOCUS);
	assert(!throttled);

	/* Immediate second call should be throttled (within 16ms) */
	throttled = should_throttle_event(&ipc, WM_IPC_EVENT_WINDOW_FOCUS);
	assert(throttled);

	printf("  PASS: test_throttle_rapid_calls\n");
}

/* Test 9: broadcast sends to subscribed clients only */
static void
test_broadcast_subscribed_only(void)
{
	reset_globals();
	create_test_pipe();

	struct wm_ipc_server ipc;
	memset(&ipc, 0, sizeof(ipc));
	ipc.server = &test_server;
	wl_list_init(&ipc.clients);

	/*
	 * Create a client subscribed to WORKSPACE events, writing to
	 * the pipe so we can verify the output.
	 */
	struct wm_ipc_client *client = make_test_client(&ipc,
		g_pipe_fds[1], WM_IPC_EVENT_WORKSPACE);

	/* Broadcast a WORKSPACE event */
	wm_ipc_broadcast_event(&ipc, WM_IPC_EVENT_WORKSPACE,
		"{\"event\":\"workspace\"}");

	/* Read back what was written to the pipe */
	size_t len;
	char *got = read_pipe_contents(&len);
	assert(len > 0);
	assert(strstr(got, "\"event\":\"workspace\"") != NULL);
	free(got);

	/* Clean up client manually (event_source already stubbed) */
	wl_list_remove(&client->link);
	ipc.client_count--;
	free(client->read_buf);
	free(client);

	printf("  PASS: test_broadcast_subscribed_only\n");
}

/* Test 10: broadcast skips unsubscribed clients */
static void
test_broadcast_skips_unsubscribed(void)
{
	reset_globals();
	create_test_pipe();

	struct wm_ipc_server ipc;
	memset(&ipc, 0, sizeof(ipc));
	ipc.server = &test_server;
	wl_list_init(&ipc.clients);

	/* Client is subscribed to OUTPUT events, not WORKSPACE */
	struct wm_ipc_client *client = make_test_client(&ipc,
		g_pipe_fds[1],
		WM_IPC_EVENT_OUTPUT_ADD | WM_IPC_EVENT_OUTPUT_REMOVE);

	/* Broadcast a WORKSPACE event -- should not reach this client */
	wm_ipc_broadcast_event(&ipc, WM_IPC_EVENT_WORKSPACE,
		"{\"event\":\"workspace\"}");

	/* Read back -- nothing should have been written */
	size_t len;
	char *got = read_pipe_contents(&len);
	assert(len == 0);
	free(got);

	wl_list_remove(&client->link);
	ipc.client_count--;
	free(client->read_buf);
	free(client);

	printf("  PASS: test_broadcast_skips_unsubscribed\n");
}

/* Test 11: broadcast appends newline if missing */
static void
test_broadcast_appends_newline(void)
{
	reset_globals();
	create_test_pipe();

	struct wm_ipc_server ipc;
	memset(&ipc, 0, sizeof(ipc));
	ipc.server = &test_server;
	wl_list_init(&ipc.clients);

	struct wm_ipc_client *client = make_test_client(&ipc,
		g_pipe_fds[1], WM_IPC_EVENT_WORKSPACE);

	/* JSON without trailing newline */
	wm_ipc_broadcast_event(&ipc, WM_IPC_EVENT_WORKSPACE,
		"{\"event\":\"workspace\"}");

	size_t len;
	char *got = read_pipe_contents(&len);
	assert(len > 0);
	assert(got[len - 1] == '\n');
	free(got);

	wl_list_remove(&client->link);
	ipc.client_count--;
	free(client->read_buf);
	free(client);

	printf("  PASS: test_broadcast_appends_newline\n");
}

/* Test 12: ipc_client_destroy cleans up resources */
static void
test_client_destroy(void)
{
	reset_globals();
	create_test_pipe();

	struct wm_ipc_server ipc;
	memset(&ipc, 0, sizeof(ipc));
	ipc.server = &test_server;
	wl_list_init(&ipc.clients);

	struct wm_ipc_client *client = make_test_client(&ipc,
		g_pipe_fds[1], 0);
	assert(ipc.client_count == 1);
	assert(!wl_list_empty(&ipc.clients));

	ipc_client_destroy(client);

	assert(ipc.client_count == 0);
	assert(wl_list_empty(&ipc.clients));
	assert(g_stub_event_source_remove_count > 0);

	/* Close remaining pipe fd */
	close(g_pipe_fds[0]);
	g_pipe_fds[0] = -1;

	printf("  PASS: test_client_destroy\n");
}

/* Test 13: process_line dispatches to handle_command and sends response */
static void
test_process_line(void)
{
	reset_globals();
	create_test_pipe();

	struct wm_ipc_server ipc;
	memset(&ipc, 0, sizeof(ipc));
	ipc.server = &test_server;
	wl_list_init(&ipc.clients);

	struct wm_ipc_client client;
	memset(&client, 0, sizeof(client));
	client.ipc = &ipc;
	client.fd = g_pipe_fds[1];

	process_line(&client, "{\"command\":\"ping\"}");

	assert(g_handle_command_count == 1);
	assert(strstr(g_last_command, "ping") != NULL);

	/* The response should have been sent to the fd */
	size_t len;
	char *got = read_pipe_contents(&len);
	assert(len > 0);
	assert(strstr(got, "success") != NULL);
	free(got);

	printf("  PASS: test_process_line\n");
}

/* Test 14: handle_client_readable processes complete lines from buffer */
static void
test_handle_client_readable_lines(void)
{
	reset_globals();

	/*
	 * We need a pipe pair: one for reading (the client fd) and
	 * one for writing (to simulate data coming in from a client).
	 * Plus another pair for the response output.
	 */
	int input_pipe[2];
	int rc = pipe(input_pipe);
	assert(rc == 0);

	/* Also create a separate pipe for the response output.
	 * We'll use /dev/null for responses since we just want to
	 * verify command dispatch. */
	int output_fd = open("/dev/null", O_WRONLY);
	assert(output_fd >= 0);

	struct wm_ipc_server ipc;
	memset(&ipc, 0, sizeof(ipc));
	ipc.server = &test_server;
	wl_list_init(&ipc.clients);

	struct wm_ipc_client *client = calloc(1, sizeof(*client));
	assert(client != NULL);
	client->ipc = &ipc;
	client->fd = input_pipe[0]; /* we read from input_pipe[0] */
	client->subscribed_events = 0;
	client->read_buf = malloc(1024);
	assert(client->read_buf != NULL);
	client->read_len = 0;
	client->read_cap = 1024;
	client->event_source =
		&g_stub_event_sources[g_stub_event_source_idx++];
	client->event_source->removed = false;
	wl_list_insert(&ipc.clients, &client->link);
	ipc.client_count++;

	/* Write two complete lines into the pipe */
	const char *data = "{\"command\":\"ping\"}\n{\"command\":\"ping\"}\n";
	ssize_t w = write(input_pipe[1], data, strlen(data));
	assert(w == (ssize_t)strlen(data));
	close(input_pipe[1]);

	/*
	 * We need to redirect the client's response fd.
	 * The handle_client_readable reads from client->fd, but
	 * process_line writes responses to client->fd too.
	 * To avoid blocking, swap the fd to /dev/null for writing.
	 * Actually, handle_client_readable uses the fd parameter for reading
	 * and client->fd for responses. They're the same fd in production
	 * but here we need to be careful.
	 *
	 * The simplest approach: since our stub handle_command returns
	 * a response that goes to ipc_send_response -> ipc_send ->
	 * write(client->fd, ...), and client->fd is the read end of the
	 * pipe that has data... let's just remap. We'll dup the fd.
	 */

	/* Actually, handle_client_readable calls read(fd, ...) where fd is
	 * the first parameter, and process_line calls ipc_send_response
	 * which writes to client->fd. In our setup client->fd IS the read
	 * end. That won't work for writing. Let's fix by giving the client
	 * a write fd and passing the read fd as the parameter. */
	client->fd = output_fd;

	/* Call the handler with the readable fd */
	handle_client_readable(input_pipe[0], WL_EVENT_READABLE, client);

	/* Both lines should have been processed */
	assert(g_handle_command_count == 2);
	assert(client->read_len == 0); /* no partial data remaining */

	/* Cleanup */
	close(input_pipe[0]);
	close(output_fd);
	wl_list_remove(&client->link);
	ipc.client_count--;
	free(client->read_buf);
	free(client);

	printf("  PASS: test_handle_client_readable_lines\n");
}

/* Test 15: handle_client_readable handles partial lines */
static void
test_handle_client_readable_partial(void)
{
	reset_globals();

	int input_pipe[2];
	int rc = pipe(input_pipe);
	assert(rc == 0);

	int output_fd = open("/dev/null", O_WRONLY);
	assert(output_fd >= 0);

	struct wm_ipc_server ipc;
	memset(&ipc, 0, sizeof(ipc));
	ipc.server = &test_server;
	wl_list_init(&ipc.clients);

	struct wm_ipc_client *client = calloc(1, sizeof(*client));
	assert(client != NULL);
	client->ipc = &ipc;
	client->fd = output_fd;
	client->subscribed_events = 0;
	client->read_buf = malloc(1024);
	assert(client->read_buf != NULL);
	client->read_len = 0;
	client->read_cap = 1024;
	client->event_source =
		&g_stub_event_sources[g_stub_event_source_idx++];
	client->event_source->removed = false;
	wl_list_insert(&ipc.clients, &client->link);
	ipc.client_count++;

	/* Write a partial line (no newline) */
	const char *partial = "{\"command\":\"pi";
	ssize_t w = write(input_pipe[1], partial, strlen(partial));
	assert(w == (ssize_t)strlen(partial));

	handle_client_readable(input_pipe[0], WL_EVENT_READABLE, client);

	/* No complete line => no command processed */
	assert(g_handle_command_count == 0);
	/* The partial data should remain in the buffer */
	assert(client->read_len == strlen(partial));

	/* Now write the rest with newline */
	const char *rest = "ng\"}\n";
	w = write(input_pipe[1], rest, strlen(rest));
	assert(w == (ssize_t)strlen(rest));

	handle_client_readable(input_pipe[0], WL_EVENT_READABLE, client);

	/* Now the complete line should have been processed */
	assert(g_handle_command_count == 1);
	assert(client->read_len == 0);

	close(input_pipe[0]);
	close(input_pipe[1]);
	close(output_fd);
	wl_list_remove(&client->link);
	ipc.client_count--;
	free(client->read_buf);
	free(client);

	printf("  PASS: test_handle_client_readable_partial\n");
}

/* Test 16: handle_client_readable on HANGUP destroys client */
static void
test_handle_client_readable_hangup(void)
{
	reset_globals();
	create_test_pipe();

	struct wm_ipc_server ipc;
	memset(&ipc, 0, sizeof(ipc));
	ipc.server = &test_server;
	wl_list_init(&ipc.clients);

	struct wm_ipc_client *client = make_test_client(&ipc,
		g_pipe_fds[1], 0);
	assert(ipc.client_count == 1);

	handle_client_readable(g_pipe_fds[1], WL_EVENT_HANGUP, client);

	assert(ipc.client_count == 0);
	assert(wl_list_empty(&ipc.clients));

	close(g_pipe_fds[0]);

	printf("  PASS: test_handle_client_readable_hangup\n");
}

/* Test 17: handle_client_readable buffer growth */
static void
test_handle_client_readable_buffer_growth(void)
{
	reset_globals();

	int input_pipe[2];
	int rc = pipe(input_pipe);
	assert(rc == 0);

	int output_fd = open("/dev/null", O_WRONLY);
	assert(output_fd >= 0);

	struct wm_ipc_server ipc;
	memset(&ipc, 0, sizeof(ipc));
	ipc.server = &test_server;
	wl_list_init(&ipc.clients);

	/* Start with a very small buffer */
	struct wm_ipc_client *client = calloc(1, sizeof(*client));
	assert(client != NULL);
	client->ipc = &ipc;
	client->fd = output_fd;
	client->subscribed_events = 0;
	client->read_buf = malloc(16);
	assert(client->read_buf != NULL);
	client->read_len = 0;
	client->read_cap = 16;
	client->event_source =
		&g_stub_event_sources[g_stub_event_source_idx++];
	client->event_source->removed = false;
	wl_list_insert(&ipc.clients, &client->link);
	ipc.client_count++;

	/* Fill buffer to capacity to trigger growth */
	memset(client->read_buf, 'A', 16);
	client->read_len = 16;

	/* Write a newline-terminated line */
	const char *data = "B\n";
	ssize_t w = write(input_pipe[1], data, strlen(data));
	assert(w == (ssize_t)strlen(data));

	handle_client_readable(input_pipe[0], WL_EVENT_READABLE, client);

	/* Buffer should have grown */
	assert(client->read_cap > 16);
	/* The line (16 A's + B) should have been processed */
	assert(g_handle_command_count == 1);

	close(input_pipe[0]);
	close(input_pipe[1]);
	close(output_fd);
	wl_list_remove(&client->link);
	ipc.client_count--;
	free(client->read_buf);
	free(client);

	printf("  PASS: test_handle_client_readable_buffer_growth\n");
}

/* Test 18: throttle uses separate slots for different event types */
static void
test_throttle_independent_events(void)
{
	struct wm_ipc_server ipc;
	memset(&ipc, 0, sizeof(ipc));

	/* WINDOW_FOCUS and WORKSPACE are both throttled but independent */
	bool t1 = should_throttle_event(&ipc, WM_IPC_EVENT_WINDOW_FOCUS);
	assert(!t1); /* first call allowed */

	bool t2 = should_throttle_event(&ipc, WM_IPC_EVENT_WORKSPACE);
	assert(!t2); /* first call for this event also allowed */

	/* Second call to WINDOW_FOCUS should be throttled */
	bool t3 = should_throttle_event(&ipc, WM_IPC_EVENT_WINDOW_FOCUS);
	assert(t3);

	/* Second call to WORKSPACE should be throttled */
	bool t4 = should_throttle_event(&ipc, WM_IPC_EVENT_WORKSPACE);
	assert(t4);

	printf("  PASS: test_throttle_independent_events\n");
}

/* Test 19: broadcast is suppressed by throttling */
static void
test_broadcast_throttled(void)
{
	reset_globals();
	create_test_pipe();

	struct wm_ipc_server ipc;
	memset(&ipc, 0, sizeof(ipc));
	ipc.server = &test_server;
	wl_list_init(&ipc.clients);

	struct wm_ipc_client *client = make_test_client(&ipc,
		g_pipe_fds[1], WM_IPC_EVENT_WINDOW_FOCUS);

	/* First broadcast goes through */
	wm_ipc_broadcast_event(&ipc, WM_IPC_EVENT_WINDOW_FOCUS,
		"{\"event\":\"focus1\"}");

	/* Immediate second broadcast should be throttled */
	wm_ipc_broadcast_event(&ipc, WM_IPC_EVENT_WINDOW_FOCUS,
		"{\"event\":\"focus2\"}");

	size_t len;
	char *got = read_pipe_contents(&len);
	/* Only the first event should have been sent */
	assert(strstr(got, "focus1") != NULL);
	assert(strstr(got, "focus2") == NULL);
	free(got);

	wl_list_remove(&client->link);
	ipc.client_count--;
	free(client->read_buf);
	free(client);

	printf("  PASS: test_broadcast_throttled\n");
}

/* Test 20: client count tracking */
static void
test_client_count_tracking(void)
{
	reset_globals();

	struct wm_ipc_server ipc;
	memset(&ipc, 0, sizeof(ipc));
	ipc.server = &test_server;
	wl_list_init(&ipc.clients);

	assert(ipc.client_count == 0);

	int pipes1[2], pipes2[2];
	int rc1 = pipe(pipes1);
	int rc2 = pipe(pipes2);
	assert(rc1 == 0 && rc2 == 0);

	struct wm_ipc_client *c1 = make_test_client(&ipc, pipes1[1], 0);
	assert(ipc.client_count == 1);

	struct wm_ipc_client *c2 = make_test_client(&ipc, pipes2[1], 0);
	assert(ipc.client_count == 2);

	ipc_client_destroy(c1);
	assert(ipc.client_count == 1);

	ipc_client_destroy(c2);
	assert(ipc.client_count == 0);
	assert(wl_list_empty(&ipc.clients));

	close(pipes1[0]);
	close(pipes2[0]);

	printf("  PASS: test_client_count_tracking\n");
}

/* Test 21: broadcast to multiple clients */
static void
test_broadcast_multiple_clients(void)
{
	reset_globals();

	/* Two separate pipes for two clients */
	int pipe1[2], pipe2[2];
	int rc1 = pipe(pipe1);
	int rc2 = pipe(pipe2);
	assert(rc1 == 0 && rc2 == 0);

	struct wm_ipc_server ipc;
	memset(&ipc, 0, sizeof(ipc));
	ipc.server = &test_server;
	wl_list_init(&ipc.clients);

	/* Client 1 subscribed to WORKSPACE */
	struct wm_ipc_client *c1 = make_test_client(&ipc,
		pipe1[1], WM_IPC_EVENT_WORKSPACE);

	/* Client 2 subscribed to WORKSPACE */
	struct wm_ipc_client *c2 = make_test_client(&ipc,
		pipe2[1], WM_IPC_EVENT_WORKSPACE);

	wm_ipc_broadcast_event(&ipc, WM_IPC_EVENT_WORKSPACE,
		"{\"event\":\"workspace\"}");

	/* Both clients should receive the event */
	close(pipe1[1]);
	char buf1[256] = {0};
	ssize_t n1 = read(pipe1[0], buf1, sizeof(buf1) - 1);
	assert(n1 > 0);
	assert(strstr(buf1, "workspace") != NULL);

	close(pipe2[1]);
	char buf2[256] = {0};
	ssize_t n2 = read(pipe2[0], buf2, sizeof(buf2) - 1);
	assert(n2 > 0);
	assert(strstr(buf2, "workspace") != NULL);

	close(pipe1[0]);
	close(pipe2[0]);

	/* Manual cleanup */
	wl_list_remove(&c1->link);
	ipc.client_count--;
	free(c1->read_buf);
	free(c1);

	wl_list_remove(&c2->link);
	ipc.client_count--;
	free(c2->read_buf);
	free(c2);

	printf("  PASS: test_broadcast_multiple_clients\n");
}

/* Test 22: broadcast does not send newline when already present */
static void
test_broadcast_no_double_newline(void)
{
	reset_globals();
	create_test_pipe();

	struct wm_ipc_server ipc;
	memset(&ipc, 0, sizeof(ipc));
	ipc.server = &test_server;
	wl_list_init(&ipc.clients);

	struct wm_ipc_client *client = make_test_client(&ipc,
		g_pipe_fds[1], WM_IPC_EVENT_STYLE_CHANGED);

	/* JSON already has trailing newline */
	wm_ipc_broadcast_event(&ipc, WM_IPC_EVENT_STYLE_CHANGED,
		"{\"event\":\"style\"}\n");

	size_t len;
	char *got = read_pipe_contents(&len);
	assert(strcmp(got, "{\"event\":\"style\"}\n") == 0);
	free(got);

	wl_list_remove(&client->link);
	ipc.client_count--;
	free(client->read_buf);
	free(client);

	printf("  PASS: test_broadcast_no_double_newline\n");
}

int
main(void)
{
	printf("test_ipc_server:\n");

	test_ipc_send_basic();
	test_ipc_send_empty();
	test_ipc_send_bad_fd();
	test_ipc_send_response_adds_newline();
	test_ipc_send_response_preserves_newline();
	test_throttle_non_throttled_event();
	test_throttle_first_call_allowed();
	test_throttle_rapid_calls();
	test_broadcast_subscribed_only();
	test_broadcast_skips_unsubscribed();
	test_broadcast_appends_newline();
	test_client_destroy();
	test_process_line();
	test_handle_client_readable_lines();
	test_handle_client_readable_partial();
	test_handle_client_readable_hangup();
	test_handle_client_readable_buffer_growth();
	test_throttle_independent_events();
	test_broadcast_throttled();
	test_client_count_tracking();
	test_broadcast_multiple_clients();
	test_broadcast_no_double_newline();

	printf("All ipc_server tests passed.\n");
	return 0;
}
