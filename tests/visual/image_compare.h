/*
 * image_compare.h - Image comparison library for visual regression testing
 *
 * Provides pixel-by-pixel comparison of Cairo image surfaces (ARGB32)
 * with configurable tolerance for fuzzy matching.  Can also load and
 * save PNG reference images for comparing against known baselines.
 */

#ifndef FLUXLAND_IMAGE_COMPARE_H
#define FLUXLAND_IMAGE_COMPARE_H

#include <cairo.h>
#include <stdbool.h>

/*
 * Result of an image comparison.
 */
struct image_compare_result {
	bool match;		/* true if images match within tolerance */
	int total_pixels;	/* total pixel count */
	int diff_pixels;	/* number of pixels that differ */
	double diff_percent;	/* percentage of differing pixels */
	int max_channel_diff;	/* maximum per-channel difference found */
};

/*
 * Compare two Cairo image surfaces pixel-by-pixel.
 *
 * Both surfaces must be CAIRO_FORMAT_ARGB32.  If the surfaces have
 * different dimensions, the comparison fails immediately.
 *
 * tolerance_percent: maximum allowed percentage of differing pixels
 *                    (e.g. 1.0 means 1% of pixels may differ).
 * channel_threshold: per-channel (R/G/B/A) difference below which
 *                    two pixels are considered equal (e.g. 2 means
 *                    values differing by 0-2 are treated as matching).
 *
 * Returns the comparison result.
 */
struct image_compare_result image_compare_surfaces(
	cairo_surface_t *actual,
	cairo_surface_t *expected,
	double tolerance_percent,
	int channel_threshold);

/*
 * Load a PNG file into a Cairo image surface.
 * Returns NULL on error.
 * Caller must call cairo_surface_destroy() on the result.
 */
cairo_surface_t *image_load_png(const char *path);

/*
 * Save a Cairo image surface to a PNG file.
 * Returns 0 on success, -1 on error.
 */
int image_save_png(cairo_surface_t *surface, const char *path);

/*
 * Generate a diff image highlighting pixel differences between
 * two surfaces.  Matching pixels are dimmed; differing pixels
 * are shown in bright red.
 *
 * Returns a new Cairo surface, or NULL on error.
 * Caller must call cairo_surface_destroy() on the result.
 */
cairo_surface_t *image_generate_diff(
	cairo_surface_t *actual,
	cairo_surface_t *expected,
	int channel_threshold);

/*
 * Compare an actual surface against a reference PNG file.
 *
 * If the reference file does not exist, the actual surface is saved
 * as the new reference and the comparison returns a match (bootstrap
 * mode for generating initial reference images).
 *
 * If the comparison fails, a diff image is saved to diff_output_path
 * (if non-NULL) for debugging.
 *
 * Returns the comparison result.
 */
struct image_compare_result image_compare_with_reference(
	cairo_surface_t *actual,
	const char *reference_path,
	const char *diff_output_path,
	double tolerance_percent,
	int channel_threshold);

#endif /* FLUXLAND_IMAGE_COMPARE_H */
