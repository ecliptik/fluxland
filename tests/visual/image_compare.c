/*
 * image_compare.c - Image comparison library for visual regression testing
 *
 * Uses Cairo's built-in PNG support to load/save reference images
 * and performs pixel-by-pixel comparison with configurable tolerance.
 */

#define _POSIX_C_SOURCE 200809L

#include "image_compare.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* ------------------------------------------------------------------ */
/* Pixel comparison helpers                                            */
/* ------------------------------------------------------------------ */

static inline int
abs_diff(int a, int b)
{
	return a > b ? a - b : b - a;
}

/*
 * Compare two ARGB32 pixel values.
 * Returns true if all channels are within the threshold.
 */
static inline bool
pixels_match(uint32_t a, uint32_t b, int threshold)
{
	int da = abs_diff((a >> 24) & 0xFF, (b >> 24) & 0xFF);
	int dr = abs_diff((a >> 16) & 0xFF, (b >> 16) & 0xFF);
	int dg = abs_diff((a >> 8) & 0xFF, (b >> 8) & 0xFF);
	int db = abs_diff(a & 0xFF, b & 0xFF);

	return da <= threshold && dr <= threshold &&
	       dg <= threshold && db <= threshold;
}

/*
 * Get the maximum per-channel difference between two pixels.
 */
static inline int
pixel_max_diff(uint32_t a, uint32_t b)
{
	int da = abs_diff((a >> 24) & 0xFF, (b >> 24) & 0xFF);
	int dr = abs_diff((a >> 16) & 0xFF, (b >> 16) & 0xFF);
	int dg = abs_diff((a >> 8) & 0xFF, (b >> 8) & 0xFF);
	int db = abs_diff(a & 0xFF, b & 0xFF);

	int max = da;
	if (dr > max) max = dr;
	if (dg > max) max = dg;
	if (db > max) max = db;
	return max;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

struct image_compare_result
image_compare_surfaces(cairo_surface_t *actual,
		       cairo_surface_t *expected,
		       double tolerance_percent,
		       int channel_threshold)
{
	struct image_compare_result result = {
		.match = false,
		.total_pixels = 0,
		.diff_pixels = 0,
		.diff_percent = 100.0,
		.max_channel_diff = 0,
	};

	if (!actual || !expected)
		return result;

	/* Ensure surfaces are flushed */
	cairo_surface_flush(actual);
	cairo_surface_flush(expected);

	int aw = cairo_image_surface_get_width(actual);
	int ah = cairo_image_surface_get_height(actual);
	int ew = cairo_image_surface_get_width(expected);
	int eh = cairo_image_surface_get_height(expected);

	/* Dimension mismatch is an immediate failure */
	if (aw != ew || ah != eh) {
		fprintf(stderr, "image_compare: dimension mismatch "
			"(%dx%d vs %dx%d)\n", aw, ah, ew, eh);
		return result;
	}

	if (aw <= 0 || ah <= 0)
		return result;

	int a_stride = cairo_image_surface_get_stride(actual);
	int e_stride = cairo_image_surface_get_stride(expected);
	unsigned char *a_data = cairo_image_surface_get_data(actual);
	unsigned char *e_data = cairo_image_surface_get_data(expected);

	if (!a_data || !e_data)
		return result;

	result.total_pixels = aw * ah;

	for (int y = 0; y < ah; y++) {
		uint32_t *a_row = (uint32_t *)(a_data + y * a_stride);
		uint32_t *e_row = (uint32_t *)(e_data + y * e_stride);

		for (int x = 0; x < aw; x++) {
			if (!pixels_match(a_row[x], e_row[x],
					  channel_threshold)) {
				result.diff_pixels++;
				int d = pixel_max_diff(a_row[x], e_row[x]);
				if (d > result.max_channel_diff)
					result.max_channel_diff = d;
			}
		}
	}

	result.diff_percent = (result.total_pixels > 0)
		? (100.0 * result.diff_pixels / result.total_pixels)
		: 0.0;
	result.match = (result.diff_percent <= tolerance_percent);

	return result;
}

cairo_surface_t *
image_load_png(const char *path)
{
	if (!path)
		return NULL;

	cairo_surface_t *surface = cairo_image_surface_create_from_png(path);
	if (!surface)
		return NULL;

	if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(surface);
		return NULL;
	}

	return surface;
}

int
image_save_png(cairo_surface_t *surface, const char *path)
{
	if (!surface || !path)
		return -1;

	cairo_surface_flush(surface);
	cairo_status_t status = cairo_surface_write_to_png(surface, path);
	return (status == CAIRO_STATUS_SUCCESS) ? 0 : -1;
}

cairo_surface_t *
image_generate_diff(cairo_surface_t *actual,
		    cairo_surface_t *expected,
		    int channel_threshold)
{
	if (!actual || !expected)
		return NULL;

	cairo_surface_flush(actual);
	cairo_surface_flush(expected);

	int aw = cairo_image_surface_get_width(actual);
	int ah = cairo_image_surface_get_height(actual);
	int ew = cairo_image_surface_get_width(expected);
	int eh = cairo_image_surface_get_height(expected);

	if (aw != ew || ah != eh)
		return NULL;

	if (aw <= 0 || ah <= 0)
		return NULL;

	cairo_surface_t *diff = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, aw, ah);
	if (cairo_surface_status(diff) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(diff);
		return NULL;
	}

	int a_stride = cairo_image_surface_get_stride(actual);
	int e_stride = cairo_image_surface_get_stride(expected);
	int d_stride = cairo_image_surface_get_stride(diff);
	unsigned char *a_data = cairo_image_surface_get_data(actual);
	unsigned char *e_data = cairo_image_surface_get_data(expected);
	unsigned char *d_data = cairo_image_surface_get_data(diff);

	cairo_surface_flush(diff);

	for (int y = 0; y < ah; y++) {
		uint32_t *a_row = (uint32_t *)(a_data + y * a_stride);
		uint32_t *e_row = (uint32_t *)(e_data + y * e_stride);
		uint32_t *d_row = (uint32_t *)(d_data + y * d_stride);

		for (int x = 0; x < aw; x++) {
			if (pixels_match(a_row[x], e_row[x],
					 channel_threshold)) {
				/* Matching pixel: dimmed version of actual */
				uint32_t p = a_row[x];
				uint8_t r = ((p >> 16) & 0xFF) / 3;
				uint8_t g = ((p >> 8) & 0xFF) / 3;
				uint8_t b = (p & 0xFF) / 3;
				d_row[x] = 0xFF000000 | (r << 16) |
					   (g << 8) | b;
			} else {
				/* Differing pixel: bright red */
				d_row[x] = 0xFFFF0000;
			}
		}
	}

	cairo_surface_mark_dirty(diff);
	return diff;
}

struct image_compare_result
image_compare_with_reference(cairo_surface_t *actual,
			     const char *reference_path,
			     const char *diff_output_path,
			     double tolerance_percent,
			     int channel_threshold)
{
	struct image_compare_result result = {
		.match = false,
		.total_pixels = 0,
		.diff_pixels = 0,
		.diff_percent = 100.0,
		.max_channel_diff = 0,
	};

	if (!actual || !reference_path)
		return result;

	/* Check if reference file exists */
	if (access(reference_path, R_OK) != 0) {
		/* Bootstrap mode: save actual as new reference */
		fprintf(stderr, "image_compare: reference not found, "
			"saving new reference: %s\n", reference_path);
		image_save_png(actual, reference_path);
		/*
		 * Always pass in bootstrap mode -- the first run
		 * establishes the baseline.  If the save failed
		 * (e.g. read-only filesystem) the next run will
		 * also bootstrap.
		 */
		result.match = true;
		result.total_pixels =
			cairo_image_surface_get_width(actual) *
			cairo_image_surface_get_height(actual);
		result.diff_pixels = 0;
		result.diff_percent = 0.0;
		return result;
	}

	/* Load reference and compare */
	cairo_surface_t *expected = image_load_png(reference_path);
	if (!expected) {
		fprintf(stderr, "image_compare: failed to load reference: %s\n",
			reference_path);
		return result;
	}

	result = image_compare_surfaces(actual, expected,
					tolerance_percent,
					channel_threshold);

	/* Generate diff image on failure */
	if (!result.match && diff_output_path) {
		cairo_surface_t *diff = image_generate_diff(
			actual, expected, channel_threshold);
		if (diff) {
			image_save_png(diff, diff_output_path);
			cairo_surface_destroy(diff);
			fprintf(stderr, "image_compare: diff saved to %s\n",
				diff_output_path);
		}
	}

	cairo_surface_destroy(expected);
	return result;
}
