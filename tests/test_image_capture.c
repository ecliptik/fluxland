/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * test_image_capture.c - ext-image-copy-capture-v1 integration test
 *
 * Captures the headless output the way xdg-desktop-portal-wlr and OBS
 * do: an output capture source, a capture session, a shm buffer, one
 * frame. Checks that the frame arrives with the output's size and that
 * it holds the desktop (not an empty buffer).
 * Exits 77 (meson skip) if the headless backend is unavailable.
 */

#define _GNU_SOURCE

#include "integration/harness.h"
#include "ext-image-capture-source-v1-client-protocol.h"
#include "ext-image-copy-capture-v1-client-protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

static int tests_run, tests_passed, tests_failed;

#define TEST_START(name) \
	do { \
		tests_run++; \
		printf("  [%d] %s ... ", tests_run, (name)); \
		fflush(stdout); \
	} while (0)
#define TEST_PASS() do { tests_passed++; printf("PASS\n"); } while (0)
#define TEST_FAIL(reason) \
	do { tests_failed++; printf("FAIL: %s\n", (reason)); } while (0)

struct capture_client {
	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_shm *shm;
	struct wl_output *output;
	struct ext_output_image_capture_source_manager_v1 *source_mgr;
	struct ext_image_copy_capture_manager_v1 *capture_mgr;

	/* Session constraints */
	uint32_t width, height;
	bool have_xrgb, have_argb;
	bool session_done, session_stopped;

	/* Frame result */
	bool frame_ready, frame_failed;
	uint32_t fail_reason;
};

static void
registry_global(void *data, struct wl_registry *registry, uint32_t name,
	const char *interface, uint32_t version)
{
	struct capture_client *c = data;
	(void)version;
	if (strcmp(interface, wl_shm_interface.name) == 0)
		c->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
	else if (strcmp(interface, wl_output_interface.name) == 0 &&
	    !c->output)
		c->output = wl_registry_bind(registry, name,
			&wl_output_interface, 1);
	else if (strcmp(interface,
	    ext_output_image_capture_source_manager_v1_interface.name) == 0)
		c->source_mgr = wl_registry_bind(registry, name,
			&ext_output_image_capture_source_manager_v1_interface, 1);
	else if (strcmp(interface,
	    ext_image_copy_capture_manager_v1_interface.name) == 0)
		c->capture_mgr = wl_registry_bind(registry, name,
			&ext_image_copy_capture_manager_v1_interface, 1);
}

static void
registry_global_remove(void *data, struct wl_registry *r, uint32_t name)
{
	(void)data; (void)r; (void)name;
}

static const struct wl_registry_listener registry_listener = {
	.global = registry_global,
	.global_remove = registry_global_remove,
};

/* --- Session events --- */

static void
session_buffer_size(void *data,
	struct ext_image_copy_capture_session_v1 *s, uint32_t w, uint32_t h)
{
	struct capture_client *c = data;
	(void)s;
	c->width = w;
	c->height = h;
}

static void
session_shm_format(void *data,
	struct ext_image_copy_capture_session_v1 *s, uint32_t format)
{
	struct capture_client *c = data;
	(void)s;
	if (format == WL_SHM_FORMAT_XRGB8888)
		c->have_xrgb = true;
	if (format == WL_SHM_FORMAT_ARGB8888)
		c->have_argb = true;
}

static void
session_dmabuf_device(void *data,
	struct ext_image_copy_capture_session_v1 *s, struct wl_array *dev)
{
	(void)data; (void)s; (void)dev;
}

static void
session_dmabuf_format(void *data,
	struct ext_image_copy_capture_session_v1 *s, uint32_t format,
	struct wl_array *modifiers)
{
	(void)data; (void)s; (void)format; (void)modifiers;
}

static void
session_done(void *data, struct ext_image_copy_capture_session_v1 *s)
{
	struct capture_client *c = data;
	(void)s;
	c->session_done = true;
}

static void
session_stopped(void *data, struct ext_image_copy_capture_session_v1 *s)
{
	struct capture_client *c = data;
	(void)s;
	c->session_stopped = true;
}

static const struct ext_image_copy_capture_session_v1_listener
session_listener = {
	.buffer_size = session_buffer_size,
	.shm_format = session_shm_format,
	.dmabuf_device = session_dmabuf_device,
	.dmabuf_format = session_dmabuf_format,
	.done = session_done,
	.stopped = session_stopped,
};

/* --- Frame events --- */

static void
frame_transform(void *data, struct ext_image_copy_capture_frame_v1 *f,
	uint32_t transform)
{
	(void)data; (void)f; (void)transform;
}

static void
frame_damage(void *data, struct ext_image_copy_capture_frame_v1 *f,
	int32_t x, int32_t y, int32_t w, int32_t h)
{
	(void)data; (void)f; (void)x; (void)y; (void)w; (void)h;
}

static void
frame_presentation_time(void *data, struct ext_image_copy_capture_frame_v1 *f,
	uint32_t hi, uint32_t lo, uint32_t nsec)
{
	(void)data; (void)f; (void)hi; (void)lo; (void)nsec;
}

static void
frame_ready(void *data, struct ext_image_copy_capture_frame_v1 *f)
{
	struct capture_client *c = data;
	(void)f;
	c->frame_ready = true;
}

static void
frame_failed(void *data, struct ext_image_copy_capture_frame_v1 *f,
	uint32_t reason)
{
	struct capture_client *c = data;
	(void)f;
	c->frame_failed = true;
	c->fail_reason = reason;
}

static const struct ext_image_copy_capture_frame_v1_listener
frame_listener = {
	.transform = frame_transform,
	.damage = frame_damage,
	.presentation_time = frame_presentation_time,
	.ready = frame_ready,
	.failed = frame_failed,
};

/* Dispatch until *flag is set (or a second flag), with a timeout */
static bool
wait_for(struct capture_client *c, bool *flag, bool *other, int ms)
{
	struct timespec start, now;
	clock_gettime(CLOCK_MONOTONIC, &start);
	while (!*flag && !(other && *other)) {
		if (wl_display_roundtrip(c->display) < 0)
			return false;
		clock_gettime(CLOCK_MONOTONIC, &now);
		long elapsed = (now.tv_sec - start.tv_sec) * 1000 +
			(now.tv_nsec - start.tv_nsec) / 1000000;
		if (elapsed > ms)
			return false;
		usleep(10000);
	}
	return *flag;
}

static struct wl_buffer *
create_shm_buffer(struct capture_client *c, uint32_t format, void **data,
	size_t *size)
{
	int stride = (int)c->width * 4;
	*size = (size_t)stride * c->height;
	int fd = memfd_create("capture", MFD_CLOEXEC);
	if (fd < 0 || ftruncate(fd, (off_t)*size) < 0)
		return NULL;
	*data = mmap(NULL, *size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (*data == MAP_FAILED) {
		close(fd);
		return NULL;
	}
	memset(*data, 0, *size);
	struct wl_shm_pool *pool = wl_shm_create_pool(c->shm, fd, (int)*size);
	struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0,
		(int)c->width, (int)c->height, stride, format);
	wl_shm_pool_destroy(pool);
	close(fd);
	return buffer;
}

int
main(void)
{
	printf("image capture integration tests:\n");
	harness_setup();
	const char *socket = NULL;
	pid_t pid = harness_start_compositor(&socket);
	if (pid <= 0) {
		harness_cleanup();
		return pid == 0 ? 77 : 1;
	}

	struct capture_client c;
	memset(&c, 0, sizeof(c));
	c.display = wl_display_connect(socket);
	if (!c.display) {
		harness_stop_compositor(pid);
		harness_cleanup();
		return 1;
	}
	c.registry = wl_display_get_registry(c.display);
	wl_registry_add_listener(c.registry, &registry_listener, &c);
	wl_display_roundtrip(c.display);

	TEST_START("capture_globals_advertised");
	if (c.shm && c.output && c.source_mgr && c.capture_mgr)
		TEST_PASS();
	else
		TEST_FAIL("missing ext-image-copy-capture globals");

	struct ext_image_copy_capture_session_v1 *session = NULL;
	TEST_START("session_constraints");
	if (c.source_mgr && c.capture_mgr) {
		struct ext_image_capture_source_v1 *source =
			ext_output_image_capture_source_manager_v1_create_source(
				c.source_mgr, c.output);
		session = ext_image_copy_capture_manager_v1_create_session(
			c.capture_mgr, source, 0);
		ext_image_copy_capture_session_v1_add_listener(session,
			&session_listener, &c);
		ext_image_capture_source_v1_destroy(source);
		if (wait_for(&c, &c.session_done, &c.session_stopped, 3000) &&
		    c.width > 0 && c.height > 0 &&
		    (c.have_xrgb || c.have_argb))
			TEST_PASS();
		else
			TEST_FAIL("no buffer size / shm format");
	} else {
		TEST_FAIL("skipped: no globals");
	}

	TEST_START("capture_frame");
	if (session && c.session_done) {
		void *pixels = NULL;
		size_t size = 0;
		uint32_t format = c.have_xrgb ? WL_SHM_FORMAT_XRGB8888 :
			WL_SHM_FORMAT_ARGB8888;
		struct wl_buffer *buffer =
			create_shm_buffer(&c, format, &pixels, &size);
		struct ext_image_copy_capture_frame_v1 *frame =
			ext_image_copy_capture_session_v1_create_frame(session);
		ext_image_copy_capture_frame_v1_add_listener(frame,
			&frame_listener, &c);
		ext_image_copy_capture_frame_v1_attach_buffer(frame, buffer);
		ext_image_copy_capture_frame_v1_damage_buffer(frame, 0, 0,
			(int32_t)c.width, (int32_t)c.height);
		ext_image_copy_capture_frame_v1_capture(frame);

		if (!wait_for(&c, &c.frame_ready, &c.frame_failed, 5000)) {
			char msg[64];
			snprintf(msg, sizeof(msg), "no frame (failed=%d reason=%u)",
				c.frame_failed, c.fail_reason);
			TEST_FAIL(msg);
		} else {
			/* The desktop: background, toolbar... not all zero */
			size_t nonzero = 0;
			const uint32_t *px = pixels;
			for (size_t i = 0; i < size / 4; i++) {
				if (px[i] & 0x00ffffff)
					nonzero++;
			}
			if (nonzero > 0)
				TEST_PASS();
			else
				TEST_FAIL("captured frame is empty");
		}
		ext_image_copy_capture_frame_v1_destroy(frame);
		wl_buffer_destroy(buffer);
		munmap(pixels, size);
	} else {
		TEST_FAIL("skipped: no session");
	}

	if (session)
		ext_image_copy_capture_session_v1_destroy(session);
	wl_display_roundtrip(c.display);
	wl_display_disconnect(c.display);
	harness_stop_compositor(pid);
	harness_cleanup();

	printf("%d/%d passed\n", tests_passed, tests_run);
	return tests_failed ? 1 : 0;
}
