/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * import.c - fluxland --import-fluxbox: a Fluxbox setup made fluxland's
 *
 * fluxland reads ~/.fluxbox as it is when it has no directory of its
 * own; this makes that directory from it, so the two can part ways:
 * the files copied with their paths pointed at the new place, the
 * startup script made one fluxland can run, and fbsetbg's last
 * wallpaper turned into fluxland's.
 */

#define _GNU_SOURCE
#include "import.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef FLUXLAND_DATADIR
#define FLUXLAND_DATADIR "/usr/share/fluxland"
#endif

struct import {
	char src[PATH_MAX];	/* the Fluxbox directory */
	char dst[PATH_MAX];	/* fluxland's */
	char dst_name[PATH_MAX];	/* dst as paths in files write it */
	bool force;
	FILE *out;
	int copied, skipped, failed;
};

static bool
is_dir(const char *path)
{
	struct stat st;
	return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static bool
is_file(const char *path)
{
	struct stat st;
	return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static char *
read_file(const char *path)
{
	FILE *f = fopen(path, "r");
	if (!f)
		return NULL;
	size_t cap = 4096, len = 0;
	char *buf = malloc(cap);
	size_t n;
	while (buf && (n = fread(buf + len, 1, cap - len - 1, f)) > 0) {
		len += n;
		if (cap - len <= 1) {
			char *nb = realloc(buf, cap * 2);
			if (!nb) {
				free(buf);
				buf = NULL;
				break;
			}
			buf = nb;
			cap *= 2;
		}
	}
	fclose(f);
	if (buf)
		buf[len] = '\0';
	return buf;
}

/* Written whole, or not at all (a temporary file renamed) */
static bool
write_file(const char *path, const char *content, mode_t mode)
{
	char tmp[PATH_MAX + 8];
	snprintf(tmp, sizeof(tmp), "%s.tmp", path);
	int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW |
		O_CLOEXEC, mode);
	if (fd < 0)
		return false;
	size_t len = strlen(content);
	bool ok = write(fd, content, len) == (ssize_t)len;
	ok &= fchmod(fd, mode) == 0;
	ok &= close(fd) == 0;
	if (ok)
		ok = rename(tmp, path) == 0;
	if (!ok)
		unlink(tmp);
	return ok;
}

/* A growing string */
struct text {
	char *s;
	size_t len, cap;
};

static void
text_add(struct text *t, const char *s, size_t n)
{
	if (t->len + n + 1 > t->cap) {
		size_t cap = t->cap ? t->cap : 1024;
		while (cap < t->len + n + 1)
			cap *= 2;
		char *ns = realloc(t->s, cap);
		if (!ns)
			return;
		t->s = ns;
		t->cap = cap;
	}
	memcpy(t->s + t->len, s, n);
	t->len += n;
	t->s[t->len] = '\0';
}

static void
text_str(struct text *t, const char *s)
{
	text_add(t, s, strlen(s));
}

/* The next line of *p (empty ones too), NULL after the last; a final
 * newline doesn't make an empty line after it */
static char *
next_line(char **p)
{
	if (!*p || !**p)
		return NULL;
	return strsep(p, "\n");
}

/*
 * Paths into the Fluxbox directory pointed at fluxland's: the directory
 * itself, ~/.fluxbox, $HOME/.fluxbox and ${HOME}/.fluxbox, each only as
 * a whole name (~/.fluxboxrc is something else).
 */
static char *
rewrite_paths_to(struct import *im, const char *line, const char *to)
{
	const char *forms[] = { im->src, "~/.fluxbox", "$HOME/.fluxbox",
		"${HOME}/.fluxbox" };
	struct text t = { 0 };
	const char *p = line;
	while (*p) {
		size_t matched = 0;
		for (size_t i = 0; i < sizeof(forms) / sizeof(forms[0]); i++) {
			size_t n = strlen(forms[i]);
			if (n && strncmp(p, forms[i], n) == 0 &&
			    !isalnum((unsigned char)p[n]) && p[n] != '_' &&
			    p[n] != '-' && p[n] != '.') {
				matched = n;
				break;
			}
		}
		if (matched) {
			text_str(&t, to);
			p += matched;
		} else {
			text_add(&t, p, 1);
			p++;
		}
	}
	if (!t.s)
		return strdup("");
	return t.s;
}

static char *
rewrite_paths(struct import *im, const char *line)
{
	return rewrite_paths_to(im, line, im->dst_name);
}

/* The destination path of a file, and whether to write it */
static bool
target(struct import *im, const char *name, char *path, size_t size)
{
	snprintf(path, size, "%s/%s", im->dst, name);
	if (!im->force && (is_file(path) || is_dir(path))) {
		fprintf(im->out, "  kept      %s (it is there already)\n", name);
		im->skipped++;
		return false;
	}
	return true;
}

static void
put(struct import *im, const char *name, const char *path,
	const char *content, mode_t mode, const char *what)
{
	if (write_file(path, content, mode)) {
		fprintf(im->out, "  %-9s %s\n", what, name);
		im->copied++;
	} else {
		fprintf(im->out, "  FAILED    %s: %s\n", name, strerror(errno));
		im->failed++;
	}
}

/* A style Fluxbox installed, as fluxland installed it, when it isn't */
static char *
installed_style(const char *value)
{
	static const char *const fluxbox_dirs[] = {
		"/usr/share/fluxbox/styles/",
		"/usr/local/share/fluxbox/styles/",
	};
	if (is_file(value) || is_dir(value))
		return NULL;
	for (size_t i = 0; i < 2; i++) {
		size_t n = strlen(fluxbox_dirs[i]);
		if (strncmp(value, fluxbox_dirs[i], n) != 0)
			continue;
		const char *name = value + n;
		static const char *const subdirs[] = { "upstream/",
			"community/", "" };
		for (size_t j = 0; j < 3; j++) {
			char path[PATH_MAX];
			snprintf(path, sizeof(path), "%s/styles/%s%s",
				FLUXLAND_DATADIR, subdirs[j], name);
			if (is_file(path) || is_dir(path))
				return strdup(path);
		}
	}
	return NULL;
}

/* The init: paths pointed here, a Fluxbox style fluxland ships as its
 * copy, fbsetbg as a root command left out (fluxland draws the desktop) */
static void
import_init(struct import *im)
{
	char from[PATH_MAX + 8], path[PATH_MAX + 8];
	snprintf(from, sizeof(from), "%s/init", im->src);
	char *content = read_file(from);
	if (!content || !target(im, "init", path, sizeof(path))) {
		free(content);
		return;
	}
	struct text out = { 0 };
	char *cursor = content;
	for (char *line; (line = next_line(&cursor));) {
		char *fixed = rewrite_paths(im, line);
		char *colon = strchr(fixed, ':');
		if (colon && strncasecmp(fixed, "session.styleFile",
		    (size_t)(colon - fixed)) == 0 &&
		    (size_t)(colon - fixed) == strlen("session.styleFile")) {
			char *v = colon + 1;
			while (isspace((unsigned char)*v))
				v++;
			char *ours = installed_style(v);
			if (ours) {
				fprintf(im->out, "  init:     styleFile %s is "
					"fluxland's copy %s\n", v, ours);
				text_str(&out, "session.styleFile:\t");
				text_str(&out, ours);
				text_str(&out, "\n");
				free(ours);
				free(fixed);
				continue;
			}
		}
		if (colon && strcasestr(fixed, "rootCommand") &&
		    strcasestr(colon, "fbsetbg")) {
			fprintf(im->out, "  init:     rootCommand fbsetbg left "
				"out: fluxland keeps the last wallpaper\n");
			text_str(&out, "! fluxland draws the desktop itself: ");
		}
		text_str(&out, fixed);
		text_str(&out, "\n");
		free(fixed);
	}
	put(im, "init", path, out.s ? out.s : "", 0644, "converted");
	free(out.s);
	free(content);
}

/* keys, apps, menu...: their paths pointed here */
static void
import_text(struct import *im, const char *name)
{
	char from[PATH_MAX + 8], path[PATH_MAX + 8];
	snprintf(from, sizeof(from), "%s/%s", im->src, name);
	if (!is_file(from))
		return;
	char *content = read_file(from);
	if (!content || !target(im, name, path, sizeof(path))) {
		free(content);
		return;
	}
	char *fixed = rewrite_paths(im, content);
	put(im, name, path, fixed, 0644,
		strcmp(fixed, content) ? "converted" : "copied");
	free(fixed);
	free(content);
}

/*
 * A startup line: "exec fluxbox" dropped, programs that only work on an
 * X display commented out with the reason, the rest as it was.
 */
char *
wm_import_startup_line(const char *line, const char **why)
{
	static const struct {
		const char *program;
		const char *why;
	} x_only[] = {
		{ "xmodmap", "the keyboard map is session.xkb.* in the init" },
		{ "setxkbmap", "the keyboard layout is session.xkb.* in the init" },
		{ "xsetroot", "fluxland draws the desktop (the style's "
			"background, SetWallpaper)" },
		{ "fbsetroot", "fluxland draws the desktop (the style's "
			"background, SetWallpaper)" },
		{ "fbsetbg", "fluxland keeps the last wallpaper itself "
			"(SetWallpaper)" },
		{ "hsetroot", "fluxland draws the desktop (SetWallpaper)" },
		{ "nitrogen", "fluxland draws the desktop (SetWallpaper)" },
		{ "xcompmgr", "a Wayland compositor composites itself" },
		{ "compton", "a Wayland compositor composites itself" },
		{ "picom", "a Wayland compositor composites itself" },
		{ "xset", "X server settings: see fluxland-init(5)" },
		{ "xrandr", "outputs are session.output.* in the init" },
		{ "xscreensaver", "use swayidle and swaylock" },
		{ "xautolock", "use swayidle and swaylock" },
		{ "xbindkeys", "bind keys in the keys file" },
		{ "unclutter", "it works on an X display only" },
		{ "numlockx", "it works on an X display only" },
		{ "idesk", "it draws on the X root window" },
		{ "fbpager", "it pages Fluxbox's own X windows" },
	};
	*why = NULL;
	const char *p = line;
	while (isspace((unsigned char)*p))
		p++;
	if (*p == '#' || *p == '\0')
		return strdup(line);

	/* The first word, or the one after exec */
	bool exec = strncmp(p, "exec", 4) == 0 && isspace((unsigned char)p[4]);
	if (exec) {
		p += 4;
		while (isspace((unsigned char)*p))
			p++;
	}
	size_t n = strcspn(p, " \t&;|");
	const char *word = p;
	for (size_t i = 0; i < n; i++) {
		if (p[i] == '/')
			word = p + i + 1;
	}
	size_t wn = (size_t)(p + n - word);
	if (exec && ((wn == 7 && strncmp(word, "fluxbox", 7) == 0) ||
	    (wn == 12 && strncmp(word, "startfluxbox", 12) == 0)))
		return NULL;
	for (size_t i = 0; i < sizeof(x_only) / sizeof(x_only[0]); i++) {
		if (wn == strlen(x_only[i].program) &&
		    strncmp(word, x_only[i].program, wn) == 0) {
			*why = x_only[i].why;
			break;
		}
	}
	/* feh sets the root window only with --bg-* */
	if (!*why && wn == 3 && strncmp(word, "feh", 3) == 0 &&
	    strstr(p, "--bg"))
		*why = "fluxland draws the desktop (SetWallpaper)";
	if (!*why)
		return strdup(line);
	size_t len = strlen(line) + strlen(*why) + 32;
	char *out = malloc(len);
	if (out)
		snprintf(out, len, "# fluxland: %s: %s", *why, line);
	return out;
}

char *
wm_import_startup_script(const char *script, FILE *report)
{
	char *copy = strdup(script ? script : "");
	if (!copy)
		return NULL;
	struct text out = { 0 };
	if (strncmp(copy, "#!", 2) != 0)
		text_str(&out, "#!/bin/sh\n");
	char *cursor = copy;
	bool ended = false;
	for (char *line; (line = next_line(&cursor));) {
		const char *t = line + strspn(line, " \t");
		if (ended && (*t == '#' || *t == '\0')) {
			text_str(&out, line);
			text_str(&out, "\n");
			continue;
		}
		if (ended) {
			/* After exec fluxbox nothing ran */
			text_str(&out, "# fluxland: after exec fluxbox: ");
			text_str(&out, line);
			text_str(&out, "\n");
			continue;
		}
		const char *why;
		char *l = wm_import_startup_line(line, &why);
		if (!l) {
			if (report)
				fprintf(report, "  startup:  \"%s\" left out "
					"(fluxland runs this once it has "
					"started)\n", line);
			text_str(&out, "# fluxland runs this once it has "
				"started: ");
			text_str(&out, line);
			text_str(&out, "\n");
			ended = true;
			continue;
		}
		if (why && report)
			fprintf(report, "  startup:  \"%s\" commented out: %s\n",
				line, why);
		text_str(&out, l);
		text_str(&out, "\n");
		free(l);
	}
	free(copy);
	return out.s ? out.s : strdup("");
}

/* The startup script: something fluxland runs once it has started */
static void
import_startup(struct import *im)
{
	char from[PATH_MAX + 8], path[PATH_MAX + 8];
	snprintf(from, sizeof(from), "%s/startup", im->src);
	char *content = read_file(from);
	if (!content || !target(im, "startup", path, sizeof(path))) {
		free(content);
		return;
	}
	char *fixed = rewrite_paths(im, content);
	char *out = wm_import_startup_script(fixed, im->out);
	put(im, "startup", path, out ? out : "", 0755, "converted");
	free(out);
	free(fixed);
	free(content);
}

/* fbsetbg's lastwallpaper ("-f|/path|style|:0.0" lines) as fluxland's
 * (the path, then the mode) */
static void
import_lastwallpaper(struct import *im)
{
	char from[PATH_MAX + 16], path[PATH_MAX + 16];
	snprintf(from, sizeof(from), "%s/lastwallpaper", im->src);
	char *content = read_file(from);
	if (!content)
		return;
	char option[64] = "", file[PATH_MAX] = "";
	char *cursor = content;
	for (char *line; (line = next_line(&cursor));) {
		char *bar = strchr(line, '|');
		if (!bar || strncmp(line, "wpsetters", 9) == 0)
			continue;
		char *end = strchr(bar + 1, '|');
		snprintf(option, sizeof(option), "%.*s", (int)(bar - line),
			line);
		snprintf(file, sizeof(file), "%.*s", end ?
			(int)(end - bar - 1) : (int)strlen(bar + 1), bar + 1);
	}
	free(content);
	if (!*file || !target(im, "lastwallpaper", path, sizeof(path)))
		return;
	const char *mode = strstr(option, "-t") ? "Tile" :
		strstr(option, "-c") ? "Center" : strstr(option, "-a") ?
		"Fit" : "Stretch";
	/* fluxland reads it as it is: no ~ */
	char *fixed = rewrite_paths_to(im, file, im->dst);
	char buf[PATH_MAX + 32];
	snprintf(buf, sizeof(buf), "%s\n%s\n", fixed, mode);
	free(fixed);
	put(im, "lastwallpaper", path, buf, 0644, "converted");
}

/* A directory (styles, backgrounds, pixmaps) copied as it is */
static void
copy_tree(struct import *im, const char *from, const char *to,
	const char *name)
{
	DIR *d = opendir(from);
	if (!d)
		return;
	mkdir(to, 0755);
	struct dirent *e;
	while ((e = readdir(d))) {
		if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
			continue;
		char a[PATH_MAX], b[PATH_MAX], rel[PATH_MAX];
		snprintf(a, sizeof(a), "%s/%s", from, e->d_name);
		snprintf(b, sizeof(b), "%s/%s", to, e->d_name);
		snprintf(rel, sizeof(rel), "%s/%s", name, e->d_name);
		if (is_dir(a)) {
			copy_tree(im, a, b, rel);
			continue;
		}
		if (!is_file(a))
			continue;
		if (!im->force && is_file(b)) {
			im->skipped++;
			continue;
		}
		int in = open(a, O_RDONLY | O_CLOEXEC);
		int out = open(b, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW |
			O_CLOEXEC, 0644);
		bool ok = in >= 0 && out >= 0;
		char buf[65536];
		ssize_t n;
		while (ok && (n = read(in, buf, sizeof(buf))) > 0)
			ok = write(out, buf, (size_t)n) == n;
		if (in >= 0)
			close(in);
		if (out >= 0)
			ok &= close(out) == 0;
		if (ok) {
			im->copied++;
		} else {
			fprintf(im->out, "  FAILED    %s\n", rel);
			im->failed++;
		}
	}
	closedir(d);
}

int
wm_import_fluxbox(const char *src, const char *dst, bool force, FILE *out)
{
	struct import im = { .force = force, .out = out };
	const char *home = getenv("HOME");
	if (src) {
		snprintf(im.src, sizeof(im.src), "%s", src);
	} else if (home) {
		snprintf(im.src, sizeof(im.src), "%s/.fluxbox", home);
	} else {
		fprintf(out, "fluxland: HOME is not set\n");
		return 1;
	}
	size_t sl = strlen(im.src);
	while (sl > 1 && im.src[sl - 1] == '/')
		im.src[--sl] = '\0';
	if (dst) {
		snprintf(im.dst, sizeof(im.dst), "%s", dst);
	} else if (getenv("XDG_CONFIG_HOME") && *getenv("XDG_CONFIG_HOME")) {
		/* (an empty one is unset, as the XDG spec says) */
		snprintf(im.dst, sizeof(im.dst), "%s/fluxland",
			getenv("XDG_CONFIG_HOME"));
	} else if (home) {
		snprintf(im.dst, sizeof(im.dst), "%s/.config/fluxland", home);
	} else {
		fprintf(out, "fluxland: HOME is not set\n");
		return 1;
	}
	if (!is_file(im.src) && !is_dir(im.src)) {
		fprintf(out, "fluxland: no Fluxbox configuration at %s\n",
			im.src);
		return 1;
	}
	if (strcmp(im.src, im.dst) == 0) {
		fprintf(out, "fluxland: %s is already where fluxland "
			"reads it\n", im.src);
		return 1;
	}

	/* Paths in the files write the new place as ~/... when it is in
	 * the home directory, as Fluxbox's did */
	size_t hl = home ? strlen(home) : 0;
	if (home && hl > 1 && strncmp(im.dst, home, hl) == 0 &&
	    im.dst[hl] == '/')
		snprintf(im.dst_name, sizeof(im.dst_name), "~%s", im.dst + hl);
	else
		snprintf(im.dst_name, sizeof(im.dst_name), "%s", im.dst);

	if (!is_dir(im.dst)) {
		char parent[PATH_MAX];
		snprintf(parent, sizeof(parent), "%s", im.dst);
		char *slash = strrchr(parent, '/');
		if (slash && slash != parent) {
			*slash = '\0';
			mkdir(parent, 0755);
		}
		if (mkdir(im.dst, 0700) != 0 && errno != EEXIST) {
			fprintf(out, "fluxland: cannot make %s: %s\n", im.dst,
				strerror(errno));
			return 1;
		}
	}
	fprintf(out, "Importing %s into %s:\n", im.src, im.dst);

	import_init(&im);
	static const char *const files[] = { "keys", "apps", "menu",
		"windowmenu", "overlay", "slitlist", "usermenu" };
	for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); i++)
		import_text(&im, files[i]);
	import_startup(&im);
	import_lastwallpaper(&im);
	static const char *const dirs[] = { "styles", "backgrounds",
		"pixmaps" };
	for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
		char a[PATH_MAX + 16], b[PATH_MAX + 16];
		snprintf(a, sizeof(a), "%s/%s", im.src, dirs[i]);
		snprintf(b, sizeof(b), "%s/%s", im.dst, dirs[i]);
		if (!is_dir(a))
			continue;
		int before = im.copied;
		copy_tree(&im, a, b, dirs[i]);
		int n = im.copied - before;
		if (n == 0)
			fprintf(out, "  kept      %s/ (it is there already)\n",
				dirs[i]);
		else
			fprintf(out, "  copied    %s/ (%d file%s)\n", dirs[i],
				n, n == 1 ? "" : "s");
	}

	fprintf(out, "%d written, %d kept%s, %d failed\n", im.copied,
		im.skipped, im.skipped && !force ? " (--force replaces them)" :
		"", im.failed);
	return im.failed ? 1 : 0;
}
