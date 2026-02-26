/*
 * test_session_lock.c - ext-session-lock-v1 protocol integration tests
 *
 * Each test forks a headless compositor, connects as a Wayland client,
 * exercises session lock functionality, and verifies expected behavior.
 * Exits 77 (meson skip) if the headless backend is unavailable.
 */

#define _GNU_SOURCE

#include "integration/harness.h"
#include "ext-session-lock-v1-client-protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

/* ------------------------------------------------------------------ */
/* Test helpers                                                        */
/* ------------------------------------------------------------------ */

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

/* ------------------------------------------------------------------ */
/* Client state                                                        */
/* ------------------------------------------------------------------ */

struct lock_client {
	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct wl_shm *shm;
	struct wl_output *output;
	struct ext_session_lock_manager_v1 *lock_mgr;
};

struct lock_state {
	bool got_locked;
	bool got_finished;
	bool got_configure;
	uint32_t configure_serial;
	uint32_t configure_width;
	uint32_t configure_height;
};

/* ------------------------------------------------------------------ */
/* Protocol listeners                                                  */
/* ------------------------------------------------------------------ */

static void
lock_locked(void *data, struct ext_session_lock_v1 *lock)
{
	(void)lock;
	struct lock_state *s = data;
	s->got_locked = true;
}

static void
lock_finished(void *data, struct ext_session_lock_v1 *lock)
{
	(void)lock;
	struct lock_state *s = data;
	s->got_finished = true;
}

static const struct ext_session_lock_v1_listener lock_listener = {
	.locked = lock_locked,
	.finished = lock_finished,
};

static void
lock_surface_configure(void *data,
		       struct ext_session_lock_surface_v1 *surface,
		       uint32_t serial, uint32_t width, uint32_t height)
{
	(void)surface;
	struct lock_state *s = data;
	s->got_configure = true;
	s->configure_serial = serial;
	s->configure_width = width;
	s->configure_height = height;
}

static const struct ext_session_lock_surface_v1_listener
lock_surface_listener = {
	.configure = lock_surface_configure,
};

static void
registry_global(void *data, struct wl_registry *registry, uint32_t name,
		const char *interface, uint32_t version)
{
	struct lock_client *c = data;

	if (strcmp(interface, "wl_compositor") == 0) {
		c->compositor = wl_registry_bind(registry, name,
			&wl_compositor_interface, version < 6 ? version : 6);
	} else if (strcmp(interface, "wl_shm") == 0) {
		c->shm = wl_registry_bind(registry, name,
			&wl_shm_interface, version < 1 ? version : 1);
	} else if (strcmp(interface, "wl_output") == 0) {
		if (!c->output)
			c->output = wl_registry_bind(registry, name,
				&wl_output_interface, version < 4 ? version : 4);
	} else if (strcmp(interface, "ext_session_lock_manager_v1") == 0) {
		c->lock_mgr = wl_registry_bind(registry, name,
			&ext_session_lock_manager_v1_interface, 1);
	}
}

static void
registry_global_remove(void *data, struct wl_registry *registry,
		       uint32_t name)
{
	(void)data;
	(void)registry;
	(void)name;
}

static const struct wl_registry_listener registry_listener = {
	.global = registry_global,
	.global_remove = registry_global_remove,
};

/* ------------------------------------------------------------------ */
/* SHM buffer creation                                                 */
/* ------------------------------------------------------------------ */

static struct wl_buffer *
create_shm_buffer(struct lock_client *c, int width, int height)
{
	int stride = width * 4;
	int size = stride * height;

	int fd = memfd_create("lock-test-buf", MFD_CLOEXEC);
	if (fd < 0)
		return NULL;

	if (ftruncate(fd, size) < 0) {
		close(fd);
		return NULL;
	}

	struct wl_shm_pool *pool = wl_shm_create_pool(c->shm, fd, size);
	close(fd);
	if (!pool)
		return NULL;

	struct wl_buffer *buf = wl_shm_pool_create_buffer(
		pool, 0, width, height, stride, WL_SHM_FORMAT_XRGB8888);
	wl_shm_pool_destroy(pool);

	return buf;
}

/* ------------------------------------------------------------------ */
/* Setup / teardown                                                    */
/* ------------------------------------------------------------------ */

static int
setup(pid_t *pid, struct lock_client *c, const char **socket)
{
	harness_setup();

	*pid = harness_start_compositor(socket);
	if (*pid <= 0) {
		harness_cleanup();
		return (*pid == 0) ? 77 : 1;
	}

	memset(c, 0, sizeof(*c));
	c->display = wl_display_connect(*socket);
	if (!c->display) {
		harness_stop_compositor(*pid);
		harness_cleanup();
		return 1;
	}

	c->registry = wl_display_get_registry(c->display);
	wl_registry_add_listener(c->registry, &registry_listener, c);
	wl_display_roundtrip(c->display);

	if (!c->compositor || !c->lock_mgr) {
		fprintf(stderr, "session_lock: missing globals "
			"(compositor=%p lock_mgr=%p) - skip\n",
			(void *)c->compositor, (void *)c->lock_mgr);
		wl_display_disconnect(c->display);
		harness_stop_compositor(*pid);
		harness_cleanup();
		return 77;
	}

	return 0;
}

static void
teardown(pid_t pid, struct lock_client *c)
{
	if (c->output)
		wl_output_destroy(c->output);
	if (c->shm)
		wl_shm_destroy(c->shm);
	if (c->lock_mgr)
		ext_session_lock_manager_v1_destroy(c->lock_mgr);
	if (c->compositor)
		wl_compositor_destroy(c->compositor);
	if (c->registry)
		wl_registry_destroy(c->registry);
	if (c->display)
		wl_display_disconnect(c->display);
	harness_stop_compositor(pid);
	harness_cleanup();
}

/* ------------------------------------------------------------------ */
/* Helper: lock + roundtrip                                            */
/* ------------------------------------------------------------------ */

static struct ext_session_lock_v1 *
do_lock(struct lock_client *c, struct lock_state *state)
{
	memset(state, 0, sizeof(*state));

	struct ext_session_lock_v1 *lock =
		ext_session_lock_manager_v1_lock(c->lock_mgr);
	if (!lock)
		return NULL;

	ext_session_lock_v1_add_listener(lock, &lock_listener, state);
	wl_display_roundtrip(c->display);

	return lock;
}

/* ------------------------------------------------------------------ */
/* Tests                                                               */
/* ------------------------------------------------------------------ */

/*
 * 1. Lock session, verify locked event received.
 */
static void
test_lock_session(struct lock_client *c)
{
	TEST_START("lock_session");

	struct lock_state state;
	struct ext_session_lock_v1 *lock = do_lock(c, &state);
	if (!lock) {
		TEST_FAIL("failed to create lock");
		return;
	}

	if (state.got_locked) {
		TEST_PASS();
	} else if (state.got_finished) {
		TEST_FAIL("got finished instead of locked");
	} else {
		TEST_FAIL("no locked or finished event");
	}

	if (state.got_locked)
		ext_session_lock_v1_unlock_and_destroy(lock);
	else
		ext_session_lock_v1_destroy(lock);
	wl_display_roundtrip(c->display);
}

/*
 * 2. Create lock surface on output, verify configure with dimensions.
 */
static void
test_lock_surface_configure(struct lock_client *c)
{
	TEST_START("lock_surface_configure");

	if (!c->output) {
		TEST_FAIL("no output available");
		return;
	}

	struct lock_state state;
	struct ext_session_lock_v1 *lock = do_lock(c, &state);
	if (!lock) {
		TEST_FAIL("create lock failed");
		return;
	}
	if (!state.got_locked) {
		TEST_FAIL("lock not confirmed");
		ext_session_lock_v1_destroy(lock);
		wl_display_roundtrip(c->display);
		return;
	}

	struct wl_surface *surf =
		wl_compositor_create_surface(c->compositor);
	struct ext_session_lock_surface_v1 *ls =
		ext_session_lock_v1_get_lock_surface(lock, surf, c->output);

	ext_session_lock_surface_v1_add_listener(
		ls, &lock_surface_listener, &state);
	wl_display_roundtrip(c->display);

	if (state.got_configure && state.configure_width > 0 &&
	    state.configure_height > 0) {
		TEST_PASS();
	} else {
		TEST_FAIL("no configure or zero dimensions");
	}

	ext_session_lock_surface_v1_destroy(ls);
	wl_surface_destroy(surf);
	ext_session_lock_v1_unlock_and_destroy(lock);
	wl_display_roundtrip(c->display);
}

/*
 * 3. Unlock (destroy lock), verify clean roundtrip.
 */
static void
test_unlock(struct lock_client *c)
{
	TEST_START("unlock_session");

	struct lock_state state;
	struct ext_session_lock_v1 *lock = do_lock(c, &state);
	if (!lock) {
		TEST_FAIL("create lock failed");
		return;
	}
	if (!state.got_locked) {
		TEST_FAIL("lock not confirmed");
		ext_session_lock_v1_destroy(lock);
		wl_display_roundtrip(c->display);
		return;
	}

	ext_session_lock_v1_unlock_and_destroy(lock);

	if (wl_display_roundtrip(c->display) >= 0) {
		TEST_PASS();
	} else {
		TEST_FAIL("roundtrip failed after unlock");
	}
}

/*
 * 4. Full lifecycle: lock, surface, ack configure, buffer, commit.
 */
static void
test_lock_surface_full_lifecycle(struct lock_client *c)
{
	TEST_START("lock_surface_full_lifecycle");

	if (!c->output || !c->shm) {
		TEST_FAIL("no output or shm");
		return;
	}

	struct lock_state state;
	struct ext_session_lock_v1 *lock = do_lock(c, &state);
	if (!lock) {
		TEST_FAIL("create lock failed");
		return;
	}
	if (!state.got_locked) {
		TEST_FAIL("lock not confirmed");
		ext_session_lock_v1_destroy(lock);
		wl_display_roundtrip(c->display);
		return;
	}

	struct wl_surface *surf =
		wl_compositor_create_surface(c->compositor);
	struct ext_session_lock_surface_v1 *ls =
		ext_session_lock_v1_get_lock_surface(lock, surf, c->output);

	ext_session_lock_surface_v1_add_listener(
		ls, &lock_surface_listener, &state);
	wl_display_roundtrip(c->display);

	if (!state.got_configure) {
		TEST_FAIL("no configure event");
		ext_session_lock_surface_v1_destroy(ls);
		wl_surface_destroy(surf);
		ext_session_lock_v1_unlock_and_destroy(lock);
		wl_display_roundtrip(c->display);
		return;
	}

	ext_session_lock_surface_v1_ack_configure(
		ls, state.configure_serial);

	struct wl_buffer *buf = create_shm_buffer(
		c, (int)state.configure_width,
		(int)state.configure_height);
	if (!buf) {
		TEST_FAIL("create shm buffer failed");
		ext_session_lock_surface_v1_destroy(ls);
		wl_surface_destroy(surf);
		ext_session_lock_v1_unlock_and_destroy(lock);
		wl_display_roundtrip(c->display);
		return;
	}

	wl_surface_attach(surf, buf, 0, 0);
	wl_surface_commit(surf);

	if (wl_display_roundtrip(c->display) >= 0) {
		TEST_PASS();
	} else {
		TEST_FAIL("roundtrip failed after commit");
	}

	ext_session_lock_surface_v1_destroy(ls);
	wl_surface_destroy(surf);
	wl_buffer_destroy(buf);
	ext_session_lock_v1_unlock_and_destroy(lock);
	wl_display_roundtrip(c->display);
}

/*
 * 5. Second lock attempt while already locked (verify rejection).
 */
static void
test_second_lock_rejected(struct lock_client *c)
{
	TEST_START("second_lock_rejected");

	struct lock_state state1;
	struct ext_session_lock_v1 *lock1 = do_lock(c, &state1);
	if (!lock1) {
		TEST_FAIL("create first lock failed");
		return;
	}
	if (!state1.got_locked) {
		TEST_FAIL("first lock not confirmed");
		ext_session_lock_v1_destroy(lock1);
		wl_display_roundtrip(c->display);
		return;
	}

	/* Second lock attempt - should be rejected */
	struct lock_state state2;
	struct ext_session_lock_v1 *lock2 = do_lock(c, &state2);
	if (!lock2) {
		TEST_PASS();
		ext_session_lock_v1_unlock_and_destroy(lock1);
		wl_display_roundtrip(c->display);
		return;
	}

	if (!state2.got_locked) {
		TEST_PASS();
	} else {
		TEST_FAIL("second lock was accepted");
	}

	/*
	 * The server rejected lock2 (sent finished, destroyed resource).
	 * Don't send protocol requests on it — just free the client proxy.
	 * Unlock lock1 normally.
	 */
	wl_proxy_destroy((struct wl_proxy *)lock2);
	ext_session_lock_v1_unlock_and_destroy(lock1);
	wl_display_roundtrip(c->display);
}

/*
 * 6. Destroy lock surface, verify compositor handles cleanup.
 */
static void
test_destroy_lock_surface(struct lock_client *c)
{
	TEST_START("destroy_lock_surface");

	if (!c->output) {
		TEST_FAIL("no output available");
		return;
	}

	struct lock_state state;
	struct ext_session_lock_v1 *lock = do_lock(c, &state);
	if (!lock) {
		TEST_FAIL("create lock failed");
		return;
	}
	if (!state.got_locked) {
		TEST_FAIL("lock not confirmed");
		ext_session_lock_v1_destroy(lock);
		wl_display_roundtrip(c->display);
		return;
	}

	struct wl_surface *surf =
		wl_compositor_create_surface(c->compositor);
	struct ext_session_lock_surface_v1 *ls =
		ext_session_lock_v1_get_lock_surface(lock, surf, c->output);

	ext_session_lock_surface_v1_add_listener(
		ls, &lock_surface_listener, &state);
	wl_display_roundtrip(c->display);

	ext_session_lock_surface_v1_destroy(ls);
	wl_surface_destroy(surf);

	if (wl_display_roundtrip(c->display) >= 0) {
		TEST_PASS();
	} else {
		TEST_FAIL("roundtrip failed after surface destroy");
	}

	ext_session_lock_v1_unlock_and_destroy(lock);
	wl_display_roundtrip(c->display);
}

/*
 * 7. Lock then unlock quickly (no surface creation).
 */
static void
test_lock_unlock_quickly(struct lock_client *c)
{
	TEST_START("lock_unlock_quickly");

	struct lock_state state;
	struct ext_session_lock_v1 *lock = do_lock(c, &state);
	if (!lock) {
		TEST_FAIL("create lock failed");
		return;
	}

	if (state.got_locked)
		ext_session_lock_v1_unlock_and_destroy(lock);
	else
		ext_session_lock_v1_destroy(lock);

	if (wl_display_roundtrip(c->display) >= 0) {
		TEST_PASS();
	} else {
		TEST_FAIL("roundtrip failed after quick unlock");
	}
}

/*
 * 8. Unlock and lock again (re-lock after unlock).
 */
static void
test_relock(struct lock_client *c)
{
	TEST_START("relock_after_unlock");

	struct lock_state state1;
	struct ext_session_lock_v1 *lock1 = do_lock(c, &state1);
	if (!lock1) {
		TEST_FAIL("create first lock failed");
		return;
	}
	if (!state1.got_locked) {
		TEST_FAIL("first lock not confirmed");
		ext_session_lock_v1_destroy(lock1);
		wl_display_roundtrip(c->display);
		return;
	}

	ext_session_lock_v1_unlock_and_destroy(lock1);
	wl_display_roundtrip(c->display);

	struct lock_state state2;
	struct ext_session_lock_v1 *lock2 = do_lock(c, &state2);
	if (!lock2) {
		TEST_FAIL("create second lock failed");
		return;
	}

	if (state2.got_locked) {
		TEST_PASS();
		ext_session_lock_v1_unlock_and_destroy(lock2);
	} else {
		TEST_FAIL("re-lock not confirmed");
		ext_session_lock_v1_destroy(lock2);
	}
	wl_display_roundtrip(c->display);
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */

int
main(void)
{
	printf("test_session_lock:\n");

	pid_t pid;
	struct lock_client client;
	const char *socket;

	int rc = setup(&pid, &client, &socket);
	if (rc == 77) {
		printf("  SKIP: headless backend or protocol unavailable\n");
		return 77;
	}
	if (rc != 0)
		return 1;

	printf("  Connected to compositor on %s\n", socket);

	test_lock_session(&client);
	test_lock_surface_configure(&client);
	test_unlock(&client);
	test_lock_surface_full_lifecycle(&client);
	test_destroy_lock_surface(&client);
	test_lock_unlock_quickly(&client);
	test_relock(&client);
	test_second_lock_rejected(&client);

	teardown(pid, &client);

	printf("  Results: %d/%d passed", tests_passed, tests_run);
	if (tests_failed > 0)
		printf(", %d FAILED", tests_failed);
	printf("\n");

	return tests_failed > 0 ? 1 : 0;
}
