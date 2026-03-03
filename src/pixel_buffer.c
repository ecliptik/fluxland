/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * pixel_buffer.c - Cairo-to-wlr_buffer bridge
 *
 * A wlr_buffer implementation backed by raw pixel data copied from a
 * Cairo surface. Used by decorations, toolbar, menus, snap previews,
 * and systray to feed Cairo-rendered content into the wlroots scene graph.
 */

#include <drm_fourcc.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/types/wlr_buffer.h>
#include <cairo.h>

#include "pixel_buffer.h"

struct wm_pixel_buffer {
	struct wlr_buffer base;
	void *data;
	uint32_t format;
	size_t stride;
};

static void pixel_buffer_destroy(struct wlr_buffer *wlr_buffer)
{
	struct wm_pixel_buffer *buffer = wl_container_of(wlr_buffer, buffer, base);
	free(buffer->data);
	free(buffer);
}

static bool pixel_buffer_begin_data_ptr_access(struct wlr_buffer *wlr_buffer,
	uint32_t flags, void **data, uint32_t *format, size_t *stride)
{
	struct wm_pixel_buffer *buffer = wl_container_of(wlr_buffer, buffer, base);
	if (flags & WLR_BUFFER_DATA_PTR_ACCESS_WRITE) {
		return false; /* read-only */
	}
	*data = buffer->data;
	*format = buffer->format;
	*stride = buffer->stride;
	return true;
}

static void pixel_buffer_end_data_ptr_access(struct wlr_buffer *wlr_buffer)
{
	/* nothing to do */
}

static const struct wlr_buffer_impl pixel_buffer_impl = {
	.destroy = pixel_buffer_destroy,
	.begin_data_ptr_access = pixel_buffer_begin_data_ptr_access,
	.end_data_ptr_access = pixel_buffer_end_data_ptr_access,
};

struct wlr_buffer *
wlr_buffer_from_cairo(cairo_surface_t *surface)
{
	if (!surface) {
		return NULL;
	}

	cairo_surface_flush(surface);

	int width = cairo_image_surface_get_width(surface);
	int height = cairo_image_surface_get_height(surface);
	int stride = cairo_image_surface_get_stride(surface);
	unsigned char *src = cairo_image_surface_get_data(surface);

	if (width <= 0 || height <= 0 || stride <= 0 || !src) {
		cairo_surface_destroy(surface);
		return NULL;
	}

	if ((size_t)stride > SIZE_MAX / (size_t)height) {
		cairo_surface_destroy(surface);
		return NULL;
	}
	size_t size = (size_t)stride * (size_t)height;
	void *data = malloc(size);
	if (!data) {
		cairo_surface_destroy(surface);
		return NULL;
	}
	memcpy(data, src, size);

	struct wm_pixel_buffer *buffer = calloc(1, sizeof(*buffer));
	if (!buffer) {
		free(data);
		cairo_surface_destroy(surface);
		return NULL;
	}

	wlr_buffer_init(&buffer->base, &pixel_buffer_impl, width, height);
	buffer->data = data;
	/* Cairo ARGB32 is native-endian ARGB which is DRM_FORMAT_ARGB8888 */
	buffer->format = DRM_FORMAT_ARGB8888;
	buffer->stride = stride;

	cairo_surface_destroy(surface);
	return &buffer->base;
}
