/*
 * test_frame_pacing.c - wp_fifo_v1 and wp_commit_timing_v1 integration tests
 *
 * Runs the headless compositor and checks, as a client, that content updates
 * with a fifo barrier or a timestamp are held back and applied in order, and
 * that the protocol errors are raised. A content update that has been
 * applied is seen as the release of the buffer it replaced.
 *
 * Exits 77 (meson skip) if the headless backend is unavailable.
 */

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include "integration/harness.h"

#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include "commit-timing-v1-client-protocol.h"
#include "fifo-v1-client-protocol.h"

#define BUFFER_SIZE 64
#define MAX_BUFFERS 64

static int tests_run;
static int tests_passed;
static int tests_failed;

#define TEST_START(name) \
	do { \
		tests_run++; \
		printf("  [%d] %s ... ", tests_run, (name)); \
		fflush(stdout); \
	} while (0)

#define TEST_PASS() \
	do { \
		tests_passed++; \
		printf("PASS\n"); \
	} while (0)

#define TEST_FAIL(reason) \
	do { \
		tests_failed++; \
		printf("FAIL: %s\n", (reason)); \
	} while (0)

struct pacing_client {
	struct test_client client;
	struct test_toplevel toplevel;
	struct wp_fifo_manager_v1 *fifo_manager;
	struct wp_commit_timing_manager_v1 *timing_manager;
	struct wp_fifo_v1 *fifo;
	struct wp_commit_timer_v1 *timer;

	struct wl_buffer *buffers[MAX_BUFFERS];
	int released[MAX_BUFFERS]; /* times each buffer was released */
	int release_count;
};

static void
registry_global(void *data, struct wl_registry *registry, uint32_t name,
	const char *interface, uint32_t version)
{
	(void)version;
	struct pacing_client *pc = data;
	if (strcmp(interface, wp_fifo_manager_v1_interface.name) == 0)
		pc->fifo_manager = wl_registry_bind(registry, name,
			&wp_fifo_manager_v1_interface, 1);
	else if (strcmp(interface, wp_commit_timing_manager_v1_interface.name) == 0)
		pc->timing_manager = wl_registry_bind(registry, name,
			&wp_commit_timing_manager_v1_interface, 1);
}

static void
registry_global_remove(void *data, struct wl_registry *registry, uint32_t name)
{
	(void)data;
	(void)registry;
	(void)name;
}

static const struct wl_registry_listener registry_listener = {
	.global = registry_global,
	.global_remove = registry_global_remove,
};

struct buffer_slot {
	struct pacing_client *pc;
	int index;
};

static void
buffer_release(void *data, struct wl_buffer *buffer)
{
	(void)buffer;
	struct buffer_slot *slot = data;
	slot->pc->released[slot->index]++;
	slot->pc->release_count++;
}

static const struct wl_buffer_listener buffer_listener = {
	.release = buffer_release,
};

static struct buffer_slot slots[MAX_BUFFERS];

static struct wl_buffer *
make_buffer(struct pacing_client *pc, int index)
{
	int stride = BUFFER_SIZE * 4;
	size_t size = (size_t)stride * BUFFER_SIZE;
	int fd = memfd_create("frame-pacing-test", 0);
	if (fd < 0 || ftruncate(fd, (off_t)size) < 0)
		return NULL;
	void *map = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (map == MAP_FAILED) {
		close(fd);
		return NULL;
	}
	memset(map, 0x40 + index, size);
	munmap(map, size);
	struct wl_shm_pool *pool = wl_shm_create_pool(pc->client.shm, fd, (int)size);
	close(fd);
	struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0, BUFFER_SIZE,
		BUFFER_SIZE, stride, WL_SHM_FORMAT_XRGB8888);
	wl_shm_pool_destroy(pool);
	slots[index] = (struct buffer_slot){ .pc = pc, .index = index };
	wl_buffer_add_listener(buffer, &buffer_listener, &slots[index]);
	return buffer;
}

static int64_t
now_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* Dispatch events for the given time */
static void
run_for_ms(struct pacing_client *pc, int ms)
{
	struct wl_display *display = pc->client.display;
	int64_t end = now_ms() + ms;
	while (now_ms() < end) {
		while (wl_display_prepare_read(display) != 0)
			wl_display_dispatch_pending(display);
		wl_display_flush(display);
		struct pollfd pfd = { .fd = wl_display_get_fd(display),
			.events = POLLIN };
		if (poll(&pfd, 1, 5) > 0 && (pfd.revents & POLLIN))
			wl_display_read_events(display);
		else
			wl_display_cancel_read(display);
		wl_display_dispatch_pending(display);
	}
}

static int
setup(pid_t *pid, struct pacing_client *pc, const char **socket)
{
	memset(pc, 0, sizeof(*pc));
	harness_setup();
	*pid = harness_start_compositor(socket);
	if (*pid <= 0) {
		harness_cleanup();
		return (*pid == 0) ? 77 : 1;
	}
	if (harness_connect(&pc->client, *socket) < 0) {
		harness_stop_compositor(*pid);
		harness_cleanup();
		return 1;
	}
	/* The harness's own registry already has a listener: use another */
	struct wl_registry *registry = wl_display_get_registry(pc->client.display);
	wl_registry_add_listener(registry, &registry_listener, pc);
	harness_roundtrip(&pc->client);
	wl_registry_destroy(registry);
	if (!pc->fifo_manager || !pc->timing_manager) {
		fprintf(stderr, "fifo-v1 or commit-timing-v1 is not advertised\n");
		harness_disconnect(&pc->client);
		harness_stop_compositor(*pid);
		harness_cleanup();
		return 1;
	}
	return 0;
}

static void
teardown(pid_t pid, struct pacing_client *pc)
{
	harness_disconnect(&pc->client);
	harness_stop_compositor(pid);
	harness_cleanup();
}

/* A mapped toplevel, with fifo and timer objects */
static bool
open_window(struct pacing_client *pc)
{
	if (harness_create_toplevel(&pc->client, &pc->toplevel, BUFFER_SIZE,
			BUFFER_SIZE) < 0) {
		fprintf(stderr, "could not create the toplevel\n");
		return false;
	}
	pc->fifo = wp_fifo_manager_v1_get_fifo(pc->fifo_manager,
		pc->toplevel.surface);
	pc->timer = wp_commit_timing_manager_v1_get_timer(pc->timing_manager,
		pc->toplevel.surface);
	for (int i = 0; i < MAX_BUFFERS; i++) {
		pc->buffers[i] = make_buffer(pc, i);
		if (!pc->buffers[i])
			return false;
	}
	harness_roundtrip(&pc->client);
	/* The window is on an output once it has been placed and rendered */
	wl_surface_attach(pc->toplevel.surface, pc->buffers[0], 0, 0);
	wl_surface_commit(pc->toplevel.surface);
	/* Let it be placed and shown on the output */
	run_for_ms(pc, 300);
	return true;
}

static void
commit_buffer(struct pacing_client *pc, int index)
{
	wl_surface_attach(pc->toplevel.surface, pc->buffers[index], 0, 0);
	wl_surface_damage_buffer(pc->toplevel.surface, 0, 0, BUFFER_SIZE,
		BUFFER_SIZE);
	wl_surface_commit(pc->toplevel.surface);
}

static void
set_timestamp_in_ms(struct pacing_client *pc, int ms)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	int64_t nsec = (int64_t)ts.tv_nsec + (int64_t)ms * 1000000;
	uint64_t sec = (uint64_t)ts.tv_sec + (uint64_t)(nsec / 1000000000);
	wp_commit_timer_v1_set_timestamp(pc->timer, (uint32_t)(sec >> 32),
		(uint32_t)sec, (uint32_t)(nsec % 1000000000));
}

/* --- Tests --- */

static void
test_fifo_paces_updates(void)
{
	TEST_START("fifo barriers show each update for a refresh");
	pid_t pid;
	const char *socket;
	struct pacing_client pc;
	int rc = setup(&pid, &pc, &socket);
	if (rc == 77)
		exit(77);
	if (rc != 0 || !open_window(&pc)) {
		TEST_FAIL("setup");
		return;
	}
	int base = pc.release_count;
	/* 30 updates at once, each waiting for the previous one's barrier */
	for (int i = 1; i <= 30; i++) {
		wp_fifo_v1_wait_barrier(pc.fifo);
		wp_fifo_v1_set_barrier(pc.fifo);
		commit_buffer(&pc, i);
	}
	harness_roundtrip(&pc.client);
	int immediately = pc.release_count - base;
	run_for_ms(&pc, 150);
	int soon = pc.release_count - base;
	run_for_ms(&pc, 1500);
	int later = pc.release_count - base;
	teardown(pid, &pc);
	/*
	 * Without pacing all 30 apply at once (30 releases). Paced, at most
	 * one applies per refresh, and they all do eventually.
	 */
	if (immediately > 2)
		TEST_FAIL("updates were applied without waiting for a refresh");
	else if (soon >= 20)
		TEST_FAIL("updates were applied faster than the output refreshes");
	else if (later < 30)
		TEST_FAIL("held updates were never applied");
	else
		TEST_PASS();
}

static void
test_timestamp_holds_update(void)
{
	TEST_START("a timestamp holds an update until its time");
	pid_t pid;
	const char *socket;
	struct pacing_client pc;
	int rc = setup(&pid, &pc, &socket);
	if (rc == 77)
		exit(77);
	if (rc != 0 || !open_window(&pc)) {
		TEST_FAIL("setup");
		return;
	}
	int base = pc.release_count;
	set_timestamp_in_ms(&pc, 400);
	commit_buffer(&pc, 1);
	run_for_ms(&pc, 200);
	int early = pc.release_count - base;
	run_for_ms(&pc, 600);
	int late = pc.release_count - base;
	teardown(pid, &pc);
	if (early != 0)
		TEST_FAIL("the update was applied before its timestamp");
	else if (late != 1)
		TEST_FAIL("the update was not applied after its timestamp");
	else
		TEST_PASS();
}

static void
test_updates_apply_in_order(void)
{
	TEST_START("an update after a held one is not applied before it");
	pid_t pid;
	const char *socket;
	struct pacing_client pc;
	int rc = setup(&pid, &pc, &socket);
	if (rc == 77)
		exit(77);
	if (rc != 0 || !open_window(&pc)) {
		TEST_FAIL("setup");
		return;
	}
	int base = pc.release_count;
	set_timestamp_in_ms(&pc, 400);
	commit_buffer(&pc, 1);
	commit_buffer(&pc, 2); /* no constraint of its own */
	run_for_ms(&pc, 200);
	int early = pc.release_count - base;
	run_for_ms(&pc, 600);
	int late = pc.release_count - base;
	/* Buffer 0 was on show; each update releases the one before it */
	bool in_order = pc.released[0] == 1 && pc.released[1] == 1;
	teardown(pid, &pc);
	if (early != 0)
		TEST_FAIL("a later update jumped ahead of a held one");
	else if (late != 2 || !in_order)
		TEST_FAIL("both updates should apply once the timestamp passes");
	else
		TEST_PASS();
}

static void
test_past_timestamp_applies_at_once(void)
{
	TEST_START("a timestamp in the past does not hold an update");
	pid_t pid;
	const char *socket;
	struct pacing_client pc;
	int rc = setup(&pid, &pc, &socket);
	if (rc == 77)
		exit(77);
	if (rc != 0 || !open_window(&pc)) {
		TEST_FAIL("setup");
		return;
	}
	int base = pc.release_count;
	wp_commit_timer_v1_set_timestamp(pc.timer, 0, 1, 0);
	commit_buffer(&pc, 1);
	run_for_ms(&pc, 100);
	int applied = pc.release_count - base;
	teardown(pid, &pc);
	if (applied != 1)
		TEST_FAIL("the update was held for a time that has passed");
	else
		TEST_PASS();
}

static void
test_second_objects_are_errors(void)
{
	TEST_START("a second fifo or commit timer for a surface is an error");
	pid_t pid;
	const char *socket;
	struct pacing_client pc;
	int rc = setup(&pid, &pc, &socket);
	if (rc == 77)
		exit(77);
	if (rc != 0 || !open_window(&pc)) {
		TEST_FAIL("setup");
		return;
	}
	wp_fifo_manager_v1_get_fifo(pc.fifo_manager, pc.toplevel.surface);
	harness_roundtrip(&pc.client);
	bool fifo_error = wl_display_get_error(pc.client.display) == EPROTO;
	teardown(pid, &pc);

	rc = setup(&pid, &pc, &socket);
	if (rc != 0 || !open_window(&pc)) {
		TEST_FAIL("setup");
		return;
	}
	wp_commit_timing_manager_v1_get_timer(pc.timing_manager,
		pc.toplevel.surface);
	harness_roundtrip(&pc.client);
	bool timer_error = wl_display_get_error(pc.client.display) == EPROTO;
	teardown(pid, &pc);
	if (!fifo_error || !timer_error)
		TEST_FAIL("no protocol error for a second object");
	else
		TEST_PASS();
}

static void
test_timestamp_errors(void)
{
	TEST_START("a bad or repeated timestamp is an error");
	pid_t pid;
	const char *socket;
	struct pacing_client pc;
	int rc = setup(&pid, &pc, &socket);
	if (rc == 77)
		exit(77);
	if (rc != 0 || !open_window(&pc)) {
		TEST_FAIL("setup");
		return;
	}
	wp_commit_timer_v1_set_timestamp(pc.timer, 0, 1, 1000000000);
	harness_roundtrip(&pc.client);
	bool invalid = wl_display_get_error(pc.client.display) == EPROTO;
	teardown(pid, &pc);

	rc = setup(&pid, &pc, &socket);
	if (rc != 0 || !open_window(&pc)) {
		TEST_FAIL("setup");
		return;
	}
	wp_commit_timer_v1_set_timestamp(pc.timer, 0, 1, 0);
	wp_commit_timer_v1_set_timestamp(pc.timer, 0, 2, 0);
	harness_roundtrip(&pc.client);
	bool repeated = wl_display_get_error(pc.client.display) == EPROTO;
	teardown(pid, &pc);

	/* One timestamp for each commit is fine */
	rc = setup(&pid, &pc, &socket);
	if (rc != 0 || !open_window(&pc)) {
		TEST_FAIL("setup");
		return;
	}
	wp_commit_timer_v1_set_timestamp(pc.timer, 0, 1, 0);
	commit_buffer(&pc, 1);
	wp_commit_timer_v1_set_timestamp(pc.timer, 0, 2, 0);
	commit_buffer(&pc, 2);
	harness_roundtrip(&pc.client);
	bool per_commit_ok = wl_display_get_error(pc.client.display) == 0;
	teardown(pid, &pc);
	if (!invalid)
		TEST_FAIL("tv_nsec of a second or more was accepted");
	else if (!repeated)
		TEST_FAIL("a second timestamp before the commit was accepted");
	else if (!per_commit_ok)
		TEST_FAIL("a timestamp for each commit was refused");
	else
		TEST_PASS();
}

int
main(void)
{
	printf("test_frame_pacing:\n");
	test_fifo_paces_updates();
	test_timestamp_holds_update();
	test_updates_apply_in_order();
	test_past_timestamp_applies_at_once();
	test_second_objects_are_errors();
	test_timestamp_errors();
	printf("\n%d run, %d passed, %d failed\n", tests_run, tests_passed,
		tests_failed);
	return tests_failed ? 1 : 0;
}
