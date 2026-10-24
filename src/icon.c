/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * icon.c - Application icons for the iconbar
 *
 * An icon comes from the window (xdg-toplevel-icon: pixels or an icon
 * name) or, for the many apps that don't send one, from the desktop file
 * matching its app_id. Names are looked up in the hicolor theme and
 * pixmaps directories of $XDG_DATA_HOME and $XDG_DATA_DIRS. Lookups are
 * cached; a miss is cached too.
 */

#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <drm_fourcc.h>
#include <wlr/types/wlr_buffer.h>

#include "icon.h"
#include "render.h"

#define ICON_CACHE_MAX 128

struct icon_cache_entry {
	char *key;          /* "name@size" or "app:<app_id>" */
	cairo_surface_t *icon; /* NULL: looked up, not found */
	char *value;        /* app entries: the Icon= name */
};

static struct icon_cache_entry cache[ICON_CACHE_MAX];
static int cache_count;

static struct icon_cache_entry *
cache_find(const char *key)
{
	for (int i = 0; i < cache_count; i++) {
		if (strcmp(cache[i].key, key) == 0)
			return &cache[i];
	}
	return NULL;
}

static struct icon_cache_entry *
cache_add(const char *key)
{
	if (cache_count == ICON_CACHE_MAX) {
		/* Full: drop the oldest */
		free(cache[0].key);
		free(cache[0].value);
		if (cache[0].icon)
			cairo_surface_destroy(cache[0].icon);
		memmove(&cache[0], &cache[1],
			sizeof(cache[0]) * (ICON_CACHE_MAX - 1));
		cache_count--;
	}
	struct icon_cache_entry *e = &cache[cache_count];
	memset(e, 0, sizeof(*e));
	e->key = strdup(key);
	if (!e->key)
		return NULL;
	cache_count++;
	return e;
}

void
wm_icon_cache_clear(void)
{
	for (int i = 0; i < cache_count; i++) {
		free(cache[i].key);
		free(cache[i].value);
		if (cache[i].icon)
			cairo_surface_destroy(cache[i].icon);
	}
	cache_count = 0;
}

/* $XDG_DATA_HOME then $XDG_DATA_DIRS, NULL-terminated; static storage */
static const char **
data_dirs(void)
{
	static char home_dir[1024];
	static char dirs_buf[4096];
	static const char *dirs[33];

	int n = 0;
	const char *xdg_home = getenv("XDG_DATA_HOME");
	const char *home = getenv("HOME");
	if (xdg_home && *xdg_home)
		snprintf(home_dir, sizeof(home_dir), "%s", xdg_home);
	else if (home)
		snprintf(home_dir, sizeof(home_dir), "%s/.local/share", home);
	else
		home_dir[0] = '\0';
	if (home_dir[0])
		dirs[n++] = home_dir;

	const char *xdg_dirs = getenv("XDG_DATA_DIRS");
	snprintf(dirs_buf, sizeof(dirs_buf), "%s",
		xdg_dirs && *xdg_dirs ? xdg_dirs : "/usr/local/share:/usr/share");
	char *save = NULL;
	for (char *d = strtok_r(dirs_buf, ":", &save); d && n < 32;
	     d = strtok_r(NULL, ":", &save)) {
		if (*d == '/')
			dirs[n++] = d;
	}
	dirs[n] = NULL;
	return dirs;
}

static bool
is_file(const char *path)
{
	struct stat st;
	return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

/* Path of an icon file for a name, preferring sizes >= size */
static bool
find_icon_file(const char *name, int size, char *path, size_t len)
{
	if (name[0] == '/') {
		snprintf(path, len, "%s", name);
		return is_file(path);
	}
	/* No path components in a theme icon name */
	if (strchr(name, '/'))
		return false;

	static const int sizes[] = {
		16, 22, 24, 32, 48, 64, 96, 128, 256, 512,
	};
	const int n_sizes = (int)(sizeof(sizes) / sizeof(sizes[0]));
	/* The first size >= wanted, then larger, then smaller */
	int start = 0;
	while (start < n_sizes - 1 && sizes[start] < size)
		start++;

	const char **dirs = data_dirs();
	for (int pass = 0; pass < n_sizes; pass++) {
		int i = pass < n_sizes - start ? start + pass :
			n_sizes - 1 - pass;
		if (i < 0 || i >= n_sizes)
			continue;
		for (const char **d = dirs; *d; d++) {
			snprintf(path, len, "%s/icons/hicolor/%dx%d/apps/%s.png",
				*d, sizes[i], sizes[i], name);
			if (is_file(path))
				return true;
		}
	}
	static const char *const others[] = {
		"%s/icons/hicolor/scalable/apps/%s.svg",
		"%s/pixmaps/%s.png",
		"%s/pixmaps/%s.svg",
		"%s/pixmaps/%s.xpm",
	};
	for (size_t o = 0; o < sizeof(others) / sizeof(others[0]); o++) {
		for (const char **d = dirs; *d; d++) {
			snprintf(path, len, others[o], *d, name);
			if (is_file(path))
				return true;
		}
	}
	return false;
}

cairo_surface_t *
wm_icon_lookup(const char *name, int size)
{
	if (!name || !*name || size <= 0)
		return NULL;
	char key[512];
	snprintf(key, sizeof(key), "%s@%d", name, size);
	struct icon_cache_entry *e = cache_find(key);
	if (e)
		return e->icon ? cairo_surface_reference(e->icon) : NULL;

	char path[1024];
	cairo_surface_t *icon = NULL;
	if (find_icon_file(name, size, path, sizeof(path)))
		icon = wm_load_pixmap(path, size, size);

	e = cache_add(key);
	if (e && icon)
		e->icon = cairo_surface_reference(icon);
	return icon;
}

/* Value of Key= in the [Desktop Entry] group of a desktop file */
static char *
desktop_entry_value(const char *path, const char *key)
{
	FILE *f = fopen(path, "r");
	if (!f)
		return NULL;
	char line[1024];
	bool in_entry = false;
	size_t klen = strlen(key);
	char *value = NULL;
	while (fgets(line, sizeof(line), f)) {
		line[strcspn(line, "\r\n")] = '\0';
		if (line[0] == '[') {
			in_entry = strcmp(line, "[Desktop Entry]") == 0;
			continue;
		}
		if (in_entry && strncmp(line, key, klen) == 0 &&
		    line[klen] == '=') {
			value = strdup(line + klen + 1);
			break;
		}
	}
	fclose(f);
	return value;
}

/* Find the desktop file for an app_id: <app_id>.desktop (any case), or
 * one whose StartupWMClass is the app_id (X11 classes) */
static char *
icon_name_for_app_id(const char *app_id)
{
	const char **dirs = data_dirs();
	char path[1024];
	for (const char **d = dirs; *d; d++) {
		snprintf(path, sizeof(path), "%s/applications/%s.desktop",
			*d, app_id);
		if (is_file(path))
			return desktop_entry_value(path, "Icon");
	}
	/* Case-insensitive name, reverse-DNS suffix, or StartupWMClass */
	for (const char **d = dirs; *d; d++) {
		char dir[1024];
		snprintf(dir, sizeof(dir), "%s/applications", *d);
		DIR *dh = opendir(dir);
		if (!dh)
			continue;
		struct dirent *de;
		char *found = NULL;
		while (!found && (de = readdir(dh))) {
			size_t n = strlen(de->d_name);
			if (n < 9 || strcmp(de->d_name + n - 8, ".desktop") != 0)
				continue;
			snprintf(path, sizeof(path), "%s/%s", dir, de->d_name);
			char stem[256];
			snprintf(stem, sizeof(stem), "%.*s", (int)(n - 8),
				de->d_name);
			const char *last = strrchr(stem, '.');
			bool match = strcasecmp(stem, app_id) == 0 ||
				(last && strcasecmp(last + 1, app_id) == 0);
			if (!match) {
				char *wmclass = desktop_entry_value(path,
					"StartupWMClass");
				match = wmclass && strcasecmp(wmclass, app_id) == 0;
				free(wmclass);
			}
			if (match)
				found = desktop_entry_value(path, "Icon");
		}
		closedir(dh);
		if (found)
			return found;
	}
	return NULL;
}

cairo_surface_t *
wm_icon_for_app_id(const char *app_id, int size)
{
	if (!app_id || !*app_id || size <= 0)
		return NULL;
	char key[512];
	snprintf(key, sizeof(key), "app:%s", app_id);
	struct icon_cache_entry *e = cache_find(key);
	if (!e) {
		e = cache_add(key);
		if (!e)
			return NULL;
		e->value = icon_name_for_app_id(app_id);
		/* No desktop file: the app_id itself may be an icon name */
		if (!e->value)
			e->value = strdup(app_id);
	}
	return e->value ? wm_icon_lookup(e->value, size) : NULL;
}

cairo_surface_t *
wm_icon_from_buffer(struct wlr_buffer *buffer, int size)
{
	void *data;
	uint32_t format;
	size_t stride;
	if (!buffer || !wlr_buffer_begin_data_ptr_access(buffer,
			WLR_BUFFER_DATA_PTR_ACCESS_READ, &data, &format,
			&stride))
		return NULL;

	cairo_surface_t *icon = NULL;
	/* Wayland buffers are premultiplied, like cairo's ARGB32 */
	if (format == DRM_FORMAT_ARGB8888 || format == DRM_FORMAT_XRGB8888) {
		int w = buffer->width, h = buffer->height;
		cairo_surface_t *src = cairo_image_surface_create(
			format == DRM_FORMAT_ARGB8888 ?
				CAIRO_FORMAT_ARGB32 : CAIRO_FORMAT_RGB24, w, h);
		if (cairo_surface_status(src) == CAIRO_STATUS_SUCCESS) {
			cairo_surface_flush(src);
			unsigned char *dst = cairo_image_surface_get_data(src);
			int dst_stride = cairo_image_surface_get_stride(src);
			for (int y = 0; y < h; y++)
				memcpy(dst + (size_t)y * dst_stride,
					(unsigned char *)data + (size_t)y * stride,
					(size_t)w * 4);
			cairo_surface_mark_dirty(src);
			icon = wm_icon_scale(src, size);
		}
		cairo_surface_destroy(src);
	}
	wlr_buffer_end_data_ptr_access(buffer);
	return icon;
}

cairo_surface_t *
wm_icon_scale(cairo_surface_t *src, int size)
{
	int w = cairo_image_surface_get_width(src);
	int h = cairo_image_surface_get_height(src);
	if (w <= 0 || h <= 0 || size <= 0)
		return NULL;
	cairo_surface_t *dst = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
		size, size);
	if (cairo_surface_status(dst) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(dst);
		return NULL;
	}
	cairo_t *cr = cairo_create(dst);
	double scale = (double)size / (w > h ? w : h);
	cairo_translate(cr, (size - w * scale) / 2, (size - h * scale) / 2);
	cairo_scale(cr, scale, scale);
	cairo_set_source_surface(cr, src, 0, 0);
	cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_GOOD);
	cairo_paint(cr);
	cairo_destroy(cr);
	return dst;
}
