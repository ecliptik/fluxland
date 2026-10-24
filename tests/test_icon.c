/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * test_icon.c - Icon lookup (themes, desktop files) and client icons
 *
 * Builds a throwaway XDG data directory with a hicolor theme, pixmaps
 * and desktop files, and points XDG_DATA_HOME/XDG_DATA_DIRS at it.
 */

#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <cairo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <drm_fourcc.h>
#include <wlr/interfaces/wlr_buffer.h>

#include "icon.h"

/* render.c stand-in: PNGs only, scaled like the real one */
cairo_surface_t *
wm_load_pixmap(const char *path, int width, int height)
{
	(void)height;
	cairo_surface_t *img = cairo_image_surface_create_from_png(path);
	if (cairo_surface_status(img) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(img);
		return NULL;
	}
	cairo_surface_t *scaled = wm_icon_scale(img, width);
	cairo_surface_destroy(img);
	return scaled;
}

static char root[256];

static void
mkdirs(const char *path)
{
	char buf[512];
	snprintf(buf, sizeof(buf), "%s", path);
	for (char *p = buf + 1; *p; p++) {
		if (*p == '/') {
			*p = '\0';
			mkdir(buf, 0755);
			*p = '/';
		}
	}
	mkdir(buf, 0755);
}

/* A size x size PNG of one colour */
static void
write_png(const char *dir, const char *name, int size, double r)
{
	mkdirs(dir);
	char path[512];
	snprintf(path, sizeof(path), "%s/%s", dir, name);
	cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
		size, size);
	cairo_t *cr = cairo_create(s);
	cairo_set_source_rgb(cr, r, 0, 0);
	cairo_paint(cr);
	cairo_destroy(cr);
	assert(cairo_surface_write_to_png(s, path) == CAIRO_STATUS_SUCCESS);
	cairo_surface_destroy(s);
}

static void
write_file(const char *dir, const char *name, const char *text)
{
	mkdirs(dir);
	char path[512];
	snprintf(path, sizeof(path), "%s/%s", dir, name);
	FILE *f = fopen(path, "w");
	assert(f);
	fputs(text, f);
	fclose(f);
}

/* Red channel of the icon's centre pixel */
static int
centre_red(cairo_surface_t *s)
{
	cairo_surface_flush(s);
	int w = cairo_image_surface_get_width(s);
	int stride = cairo_image_surface_get_stride(s);
	uint32_t px = *(uint32_t *)(cairo_image_surface_get_data(s) +
		(size_t)(w / 2) * stride + (size_t)(w / 2) * 4);
	return (int)((px >> 16) & 0xff);
}

static void
setup(void)
{
	snprintf(root, sizeof(root), "/tmp/fluxland-test/icon-%d", getpid());
	char d[512];

	/* $XDG_DATA_HOME: a 48px and a 256px "term", desktop files */
	snprintf(d, sizeof(d), "%s/home/icons/hicolor/48x48/apps", root);
	write_png(d, "term.png", 48, 1.0);
	snprintf(d, sizeof(d), "%s/home/icons/hicolor/256x256/apps", root);
	write_png(d, "term.png", 256, 0.5);
	snprintf(d, sizeof(d), "%s/home/applications", root);
	write_file(d, "org.example.Term.desktop",
		"[Desktop Entry]\nName=Term\nIcon=term\n");
	write_file(d, "editor.desktop",
		"[Desktop Action new]\nIcon=wrong\n"
		"[Desktop Entry]\nName=Ed\nStartupWMClass=XEdit\nIcon=ed\n");

	/* $XDG_DATA_DIRS: only a pixmap "ed" */
	snprintf(d, sizeof(d), "%s/sys/pixmaps", root);
	write_png(d, "ed.png", 16, 0.25);

	char home[300], dirs[300];
	snprintf(home, sizeof(home), "%s/home", root);
	snprintf(dirs, sizeof(dirs), "%s/nothing:%s/sys", root, root);
	setenv("XDG_DATA_HOME", home, 1);
	setenv("XDG_DATA_DIRS", dirs, 1);
	wm_icon_cache_clear();
}

static void
test_theme_lookup(void)
{
	/* The size at or above what's asked for: 48px for 32 */
	cairo_surface_t *s = wm_icon_lookup("term", 32);
	assert(s && cairo_image_surface_get_width(s) == 32);
	assert(centre_red(s) == 255);
	cairo_surface_destroy(s);

	/* Larger than all but 256: the 256px one */
	s = wm_icon_lookup("term", 100);
	assert(s && centre_red(s) >= 126 && centre_red(s) <= 129);
	cairo_surface_destroy(s);

	/* Pixmaps directory, in XDG_DATA_DIRS */
	s = wm_icon_lookup("ed", 32);
	assert(s && cairo_image_surface_get_width(s) == 32);
	cairo_surface_destroy(s);

	assert(wm_icon_lookup("missing", 32) == NULL);
	assert(wm_icon_lookup("../etc/passwd", 32) == NULL);
	assert(wm_icon_lookup(NULL, 32) == NULL);
	printf("  PASS: theme_lookup\n");
}

static void
test_app_id_lookup(void)
{
	/* Exact desktop file name */
	cairo_surface_t *s = wm_icon_for_app_id("org.example.Term", 32);
	assert(s && centre_red(s) == 255);
	cairo_surface_destroy(s);

	/* Last component of a reverse-DNS name, any case */
	s = wm_icon_for_app_id("TERM", 32);
	assert(s);
	cairo_surface_destroy(s);

	/* X11 class via StartupWMClass; Icon from [Desktop Entry] only */
	s = wm_icon_for_app_id("XEdit", 24);
	assert(s && cairo_image_surface_get_width(s) == 24);
	cairo_surface_destroy(s);

	/* No desktop file: the app_id as an icon name */
	s = wm_icon_for_app_id("ed", 32);
	assert(s);
	cairo_surface_destroy(s);

	assert(wm_icon_for_app_id("nonexistent.App", 32) == NULL);
	/* Cached misses stay misses */
	assert(wm_icon_for_app_id("nonexistent.App", 32) == NULL);
	printf("  PASS: app_id_lookup\n");
}

/* A minimal shm-like wlr_buffer over a malloc'd ARGB8888 image */
struct mem_buffer {
	struct wlr_buffer base;
	void *data;
	size_t stride;
};

static void
mem_destroy(struct wlr_buffer *b)
{
	struct mem_buffer *m = wl_container_of(b, m, base);
	free(m->data);
	free(m);
}

static bool
mem_begin(struct wlr_buffer *b, uint32_t flags, void **data,
	uint32_t *format, size_t *stride)
{
	(void)flags;
	struct mem_buffer *m = wl_container_of(b, m, base);
	*data = m->data;
	*format = DRM_FORMAT_ARGB8888;
	*stride = m->stride;
	return true;
}

static void
mem_end(struct wlr_buffer *b)
{
	(void)b;
}

static const struct wlr_buffer_impl mem_impl = {
	.destroy = mem_destroy,
	.begin_data_ptr_access = mem_begin,
	.end_data_ptr_access = mem_end,
};

static void
test_client_buffer(void)
{
	struct mem_buffer *m = calloc(1, sizeof(*m));
	int w = 64, h = 64;
	m->stride = (size_t)w * 4;
	m->data = malloc(m->stride * h);
	uint32_t *px = m->data;
	for (int i = 0; i < w * h; i++)
		px[i] = 0xff00ff00; /* opaque green */
	wlr_buffer_init(&m->base, &mem_impl, w, h);

	cairo_surface_t *s = wm_icon_from_buffer(&m->base, 32);
	assert(s && cairo_image_surface_get_width(s) == 32);
	cairo_surface_flush(s);
	uint32_t c = *(uint32_t *)(cairo_image_surface_get_data(s) +
		16 * cairo_image_surface_get_stride(s) + 16 * 4);
	assert(c == 0xff00ff00);
	cairo_surface_destroy(s);
	wlr_buffer_drop(&m->base);

	assert(wm_icon_from_buffer(NULL, 32) == NULL);
	printf("  PASS: client_buffer\n");
}

int
main(void)
{
	printf("icon tests:\n");
	setup();
	test_theme_lookup();
	test_app_id_lookup();
	test_client_buffer();
	wm_icon_cache_clear();
	char cmd[300];
	snprintf(cmd, sizeof(cmd), "rm -rf %s", root);
	if (system(cmd) != 0)
		return 1;
	printf("All icon tests passed.\n");
	return 0;
}
