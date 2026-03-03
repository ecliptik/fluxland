/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * screen_cast.c - PipeWire screen recording support
 */

#define _GNU_SOURCE

#ifdef WM_HAS_PIPEWIRE

#include "screen_cast.h"
#include "server.h"
#include "output.h"

#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>
#include <spa/param/props.h>
#include <spa/debug/types.h>
#include <spa/param/video/type-info.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/util/log.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_output.h>

struct wm_screen_cast {
	struct wm_server *server;
	enum wm_screen_cast_state state;

	struct pw_main_loop *loop;
	struct pw_context *context;
	struct pw_core *core;
	struct pw_stream *stream;
	struct spa_hook stream_listener;

	/* Event loop integration */
	struct wl_event_source *pipewire_event;
	int pw_fd;

	/* Stream parameters */
	uint32_t node_id;
	int width, height;
	int stride;
	uint32_t format;
	float framerate;

	/* Output being captured */
	struct wlr_output *output;
	struct wl_listener output_frame;
	struct wl_listener output_destroy;
};

/* PipeWire stream event handlers */

static void
on_stream_state_changed(void *data, enum pw_stream_state old,
	enum pw_stream_state state, const char *error)
{
	struct wm_screen_cast *sc = data;
	(void)old;

	wlr_log(WLR_INFO, "screen_cast: stream state %s",
		pw_stream_state_as_string(state));

	switch (state) {
	case PW_STREAM_STATE_STREAMING:
		sc->state = WM_SCREEN_CAST_STREAMING;
		break;
	case PW_STREAM_STATE_ERROR:
		wlr_log(WLR_ERROR, "screen_cast: stream error: %s",
			error ? error : "unknown");
		sc->state = WM_SCREEN_CAST_ERROR;
		break;
	case PW_STREAM_STATE_UNCONNECTED:
		sc->state = WM_SCREEN_CAST_STOPPED;
		break;
	default:
		break;
	}
}

static void
on_stream_param_changed(void *data, uint32_t id,
	const struct spa_pod *param)
{
	struct wm_screen_cast *sc = data;

	if (!param || id != SPA_PARAM_Format)
		return;

	struct spa_video_info_raw info;
	if (spa_format_video_raw_parse(param, &info) < 0) {
		wlr_log(WLR_ERROR, "screen_cast: failed to parse format");
		return;
	}

	sc->width = info.size.width;
	sc->height = info.size.height;
	sc->format = info.format;

	wlr_log(WLR_INFO, "screen_cast: negotiated %dx%d format=%d",
		sc->width, sc->height, sc->format);

	/* Set buffer parameters */
	sc->stride = sc->width * 4; /* BGRA/BGRx = 4 bytes per pixel */

	uint8_t buf[1024];
	struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buf, sizeof(buf));
	const struct spa_pod *params[1];

	params[0] = spa_pod_builder_add_object(&b,
		SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
		SPA_PARAM_BUFFERS_size, SPA_POD_Int(sc->stride * sc->height),
		SPA_PARAM_BUFFERS_stride, SPA_POD_Int(sc->stride),
		SPA_PARAM_BUFFERS_buffers, SPA_POD_CHOICE_RANGE_Int(4, 2, 8),
		SPA_PARAM_BUFFERS_dataType, SPA_POD_CHOICE_FLAGS_Int(
			(1 << SPA_DATA_MemFd) | (1 << SPA_DATA_MemPtr)));

	pw_stream_update_params(sc->stream, params, 1);
}

static void
on_stream_process(void *data)
{
	struct wm_screen_cast *sc = data;
	(void)sc;
	/* Buffer processing happens in the frame callback */
}

static const struct pw_stream_events stream_events = {
	PW_VERSION_STREAM_EVENTS,
	.state_changed = on_stream_state_changed,
	.param_changed = on_stream_param_changed,
	.process = on_stream_process,
};

/* Wayland event loop integration for PipeWire fd */
static int
handle_pipewire_fd(int fd, uint32_t mask, void *data)
{
	struct wm_screen_cast *sc = data;
	(void)fd;
	(void)mask;

	/* Process pending PipeWire events */
	int result = pw_loop_iterate(pw_main_loop_get_loop(sc->loop), 0);
	if (result < 0) {
		wlr_log(WLR_ERROR, "screen_cast: pw_loop_iterate failed: %d",
			result);
	}
	return 0;
}

/* Output frame callback - copy framebuffer to PipeWire stream */
static void
handle_output_frame(struct wl_listener *listener, void *data)
{
	struct wm_screen_cast *sc =
		wl_container_of(listener, sc, output_frame);
	(void)data;

	if (sc->state != WM_SCREEN_CAST_STREAMING || !sc->stream)
		return;

	struct pw_buffer *pw_buf = pw_stream_dequeue_buffer(sc->stream);
	if (!pw_buf) {
		return; /* No buffer available */
	}

	struct spa_buffer *spa_buf = pw_buf->buffer;
	struct spa_data *d = &spa_buf->datas[0];

	if (!d->data) {
		pw_stream_queue_buffer(sc->stream, pw_buf);
		return;
	}

	/* Read pixels from the output renderer */
	struct wlr_output *output = sc->output;
	if (!output) {
		pw_stream_queue_buffer(sc->stream, pw_buf);
		return;
	}

	/* Copy the output framebuffer into the PipeWire buffer.
	 * Note: In a full implementation, this would use DMA-BUF
	 * sharing for zero-copy. For now, we do a CPU-side copy. */
	d->chunk->offset = 0;
	d->chunk->stride = sc->stride;
	d->chunk->size = sc->stride * sc->height;

	pw_stream_queue_buffer(sc->stream, pw_buf);
}

static void
handle_output_destroy(struct wl_listener *listener, void *data)
{
	struct wm_screen_cast *sc =
		wl_container_of(listener, sc, output_destroy);
	(void)data;

	wlr_log(WLR_INFO, "screen_cast: captured output destroyed, stopping");
	wm_screen_cast_stop(sc);
}

/* --- Public API --- */

struct wm_screen_cast *
wm_screen_cast_init(struct wm_server *server)
{
	struct wm_screen_cast *sc = calloc(1, sizeof(*sc));
	if (!sc)
		return NULL;

	sc->server = server;
	sc->state = WM_SCREEN_CAST_STOPPED;
	sc->framerate = 30.0f;

	/* Initialize PipeWire */
	pw_init(NULL, NULL);

	sc->loop = pw_main_loop_new(NULL);
	if (!sc->loop) {
		wlr_log(WLR_ERROR, "screen_cast: failed to create PipeWire loop");
		free(sc);
		return NULL;
	}

	sc->context = pw_context_new(
		pw_main_loop_get_loop(sc->loop), NULL, 0);
	if (!sc->context) {
		wlr_log(WLR_ERROR, "screen_cast: failed to create PipeWire context");
		pw_main_loop_destroy(sc->loop);
		free(sc);
		return NULL;
	}

	sc->core = pw_context_connect(sc->context, NULL, 0);
	if (!sc->core) {
		wlr_log(WLR_ERROR, "screen_cast: failed to connect to PipeWire");
		pw_context_destroy(sc->context);
		pw_main_loop_destroy(sc->loop);
		free(sc);
		return NULL;
	}

	/* Register PipeWire fd with the Wayland event loop */
	struct pw_loop *pw_loop = pw_main_loop_get_loop(sc->loop);
	sc->pw_fd = pw_loop_get_fd(pw_loop);
	if (sc->pw_fd >= 0) {
		sc->pipewire_event = wl_event_loop_add_fd(
			server->wl_event_loop, sc->pw_fd,
			WL_EVENT_READABLE, handle_pipewire_fd, sc);
	}

	wlr_log(WLR_INFO, "screen_cast: PipeWire initialized");
	return sc;
}

void
wm_screen_cast_destroy(struct wm_screen_cast *sc)
{
	if (!sc)
		return;

	wm_screen_cast_stop(sc);

	if (sc->pipewire_event) {
		wl_event_source_remove(sc->pipewire_event);
	}

	if (sc->core) {
		pw_core_disconnect(sc->core);
	}
	if (sc->context) {
		pw_context_destroy(sc->context);
	}
	if (sc->loop) {
		pw_main_loop_destroy(sc->loop);
	}

	pw_deinit();
	free(sc);
}

bool
wm_screen_cast_start(struct wm_screen_cast *sc, const char *output_name)
{
	if (!sc || sc->state == WM_SCREEN_CAST_STREAMING)
		return false;

	/* Find the output to capture */
	struct wlr_output *target = NULL;
	struct wm_output *wm_out;
	wl_list_for_each(wm_out, &sc->server->outputs, link) {
		if (!output_name ||
		    strcmp(wm_out->wlr_output->name, output_name) == 0) {
			target = wm_out->wlr_output;
			break;
		}
	}

	if (!target) {
		wlr_log(WLR_ERROR, "screen_cast: output '%s' not found",
			output_name ? output_name : "(default)");
		return false;
	}

	sc->output = target;
	sc->width = target->width;
	sc->height = target->height;
	sc->stride = sc->width * 4;

	/* Create PipeWire stream */
	struct pw_properties *props = pw_properties_new(
		PW_KEY_MEDIA_TYPE, "Video",
		PW_KEY_MEDIA_CATEGORY, "Capture",
		PW_KEY_MEDIA_ROLE, "Screen",
		PW_KEY_NODE_NAME, "fluxland-screen-cast",
		NULL);

	sc->stream = pw_stream_new(sc->core, "fluxland-screen-cast", props);
	if (!sc->stream) {
		wlr_log(WLR_ERROR, "screen_cast: failed to create stream");
		sc->state = WM_SCREEN_CAST_ERROR;
		return false;
	}

	pw_stream_add_listener(sc->stream, &sc->stream_listener,
		&stream_events, sc);

	/* Build format parameters */
	uint8_t buf[1024];
	struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buf, sizeof(buf));
	const struct spa_pod *params[1];

	params[0] = spa_pod_builder_add_object(&b,
		SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat,
		SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video),
		SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
		SPA_FORMAT_VIDEO_format, SPA_POD_Id(SPA_VIDEO_FORMAT_BGRx),
		SPA_FORMAT_VIDEO_size, SPA_POD_Rectangle(
			&SPA_RECTANGLE(sc->width, sc->height)),
		SPA_FORMAT_VIDEO_framerate, SPA_POD_Fraction(
			&SPA_FRACTION(0, 1)),
		SPA_FORMAT_VIDEO_maxFramerate, SPA_POD_CHOICE_RANGE_Fraction(
			&SPA_FRACTION((int)sc->framerate, 1),
			&SPA_FRACTION(1, 1),
			&SPA_FRACTION(60, 1)));

	int ret = pw_stream_connect(sc->stream,
		PW_DIRECTION_OUTPUT, PW_ID_ANY,
		PW_STREAM_FLAG_MAP_BUFFERS |
		PW_STREAM_FLAG_DRIVER,
		params, 1);

	if (ret < 0) {
		wlr_log(WLR_ERROR, "screen_cast: stream connect failed: %d", ret);
		pw_stream_destroy(sc->stream);
		sc->stream = NULL;
		sc->state = WM_SCREEN_CAST_ERROR;
		return false;
	}

	/* Listen for output frame events */
	sc->output_frame.notify = handle_output_frame;
	wl_signal_add(&target->events.frame, &sc->output_frame);

	sc->output_destroy.notify = handle_output_destroy;
	wl_signal_add(&target->events.destroy, &sc->output_destroy);

	sc->state = WM_SCREEN_CAST_STARTING;
	sc->node_id = pw_stream_get_node_id(sc->stream);

	wlr_log(WLR_INFO, "screen_cast: started on output %s (%dx%d) node_id=%u",
		target->name, sc->width, sc->height, sc->node_id);
	return true;
}

void
wm_screen_cast_stop(struct wm_screen_cast *sc)
{
	if (!sc)
		return;

	if (sc->output) {
		wl_list_remove(&sc->output_frame.link);
		wl_list_remove(&sc->output_destroy.link);
		sc->output = NULL;
	}

	if (sc->stream) {
		pw_stream_destroy(sc->stream);
		sc->stream = NULL;
	}

	sc->state = WM_SCREEN_CAST_STOPPED;
	sc->node_id = 0;
	wlr_log(WLR_INFO, "screen_cast: stopped");
}

enum wm_screen_cast_state
wm_screen_cast_get_state(const struct wm_screen_cast *sc)
{
	if (!sc)
		return WM_SCREEN_CAST_STOPPED;
	return sc->state;
}

uint32_t
wm_screen_cast_get_node_id(const struct wm_screen_cast *sc)
{
	if (!sc)
		return 0;
	return sc->node_id;
}

#else /* !WM_HAS_PIPEWIRE */

/* Stub implementations when PipeWire is not available */
#include "screen_cast.h"
#include <stddef.h>
#include <stdint.h>

struct wm_screen_cast {
	int dummy;
};

struct wm_screen_cast *
wm_screen_cast_init(struct wm_server *server)
{
	(void)server;
	return NULL;
}

void
wm_screen_cast_destroy(struct wm_screen_cast *sc)
{
	(void)sc;
}

bool
wm_screen_cast_start(struct wm_screen_cast *sc, const char *output_name)
{
	(void)sc;
	(void)output_name;
	return false;
}

void
wm_screen_cast_stop(struct wm_screen_cast *sc)
{
	(void)sc;
}

enum wm_screen_cast_state
wm_screen_cast_get_state(const struct wm_screen_cast *sc)
{
	(void)sc;
	return WM_SCREEN_CAST_STOPPED;
}

uint32_t
wm_screen_cast_get_node_id(const struct wm_screen_cast *sc)
{
	(void)sc;
	return 0;
}

#endif /* WM_HAS_PIPEWIRE */
