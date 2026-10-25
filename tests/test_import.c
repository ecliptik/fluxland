/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * test_import.c - fluxland --import-fluxbox
 */

#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "import.h"

static char root[] = "/tmp/fluxland-test/import-XXXXXX";

static void
put(const char *rel, const char *content)
{
	char path[512];
	snprintf(path, sizeof(path), "%s/%s", root, rel);
	FILE *f = fopen(path, "w");
	assert(f);
	fputs(content, f);
	fclose(f);
}

static char *
get(const char *rel)
{
	char path[512];
	snprintf(path, sizeof(path), "%s/%s", root, rel);
	FILE *f = fopen(path, "r");
	if (!f)
		return NULL;
	static char buf[8192];
	size_t n = fread(buf, 1, sizeof(buf) - 1, f);
	buf[n] = '\0';
	fclose(f);
	return buf;
}

static void
test_startup_lines(void)
{
	const char *why;
	char *l = wm_import_startup_line("gkrellm -w &", &why);
	assert(l && !why && strcmp(l, "gkrellm -w &") == 0);
	free(l);
	assert(!wm_import_startup_line("exec fluxbox", &why));
	assert(!wm_import_startup_line("  exec /usr/bin/startfluxbox", &why));
	l = wm_import_startup_line("exec fluxbox-remote foo", &why);
	assert(l && !why);	/* not fluxbox itself */
	free(l);
	l = wm_import_startup_line("/usr/bin/xcompmgr -c &", &why);
	assert(why && strncmp(l, "# fluxland: ", 12) == 0);
	assert(strstr(l, "xcompmgr -c &"));
	free(l);
	l = wm_import_startup_line("feh --bg-scale ~/x.jpg", &why);
	assert(why);
	free(l);
	l = wm_import_startup_line("feh ~/x.jpg", &why);
	assert(!why);	/* a picture, not the root window */
	free(l);
	l = wm_import_startup_line("# xsetroot -solid black", &why);
	assert(!why);	/* comments stay */
	free(l);
	printf("  PASS: startup_lines\n");
}

static void
test_import(void)
{
	assert(mkdtemp(root));
	char dir[512];
	snprintf(dir, sizeof(dir), "%s/fb", root);
	mkdir(dir, 0755);
	snprintf(dir, sizeof(dir), "%s/fb/styles", root);
	mkdir(dir, 0755);
	put("fb/init", "session.keyFile:\t~/.fluxbox/keys\n"
		"session.screen0.rootCommand:\tfbsetbg -l\n\n"
		"session.screen0.workspaces:\t4\n");
	put("fb/keys", "Mod1 Tab :NextWindow\n"
		"Mod4 m :RootMenu\n");
	put("fb/menu", "[begin] (fb)\n"
		"[stylesdir] (~/.fluxbox/styles)\n"
		"[stylesdir] (~/.fluxboxrc/x)\n[end]\n");
	put("fb/startup", "xsetroot -solid grey &\nconky &\nexec fluxbox\n");
	put("fb/lastwallpaper", "wpsetters feh\n-t|/pics/a.png|s|:0.0\n");
	put("fb/styles/s1", "window.title.focus: flat\n");

	char src[512], dst[512];
	snprintf(src, sizeof(src), "%s/fb", root);
	snprintf(dst, sizeof(dst), "%s/fl", root);
	FILE *log = tmpfile();
	assert(wm_import_fluxbox(src, dst, false, log) == 0);

	/* Paths pointed here, as a whole name only */
	char *init = get("fl/init");
	assert(strstr(init, dst) || strstr(init, "fl/keys"));
	assert(!strstr(init, ".fluxbox"));
	assert(strstr(init, "! fluxland draws the desktop itself"));
	assert(strstr(init, "\n\nsession.screen0.workspaces"));
	char *menu = get("fl/menu");
	assert(!strstr(menu, "~/.fluxbox/styles"));
	assert(strstr(menu, "~/.fluxboxrc/x"));
	assert(strcmp(get("fl/keys"), "Mod1 Tab :NextWindow\n"
		"Mod4 m :RootMenu\n") == 0);

	char *startup = get("fl/startup");
	assert(strncmp(startup, "#!/bin/sh\n", 10) == 0);
	assert(strstr(startup, "# fluxland: fluxland draws the desktop"));
	assert(strstr(startup, "\nconky &\n"));
	assert(strstr(startup, "# fluxland runs this once it has started: "
		"exec fluxbox"));
	struct stat st;
	snprintf(dir, sizeof(dir), "%s/fl/startup", root);
	assert(stat(dir, &st) == 0 && (st.st_mode & S_IXUSR));

	assert(strcmp(get("fl/lastwallpaper"), "/pics/a.png\nTile\n") == 0);
	assert(strcmp(get("fl/styles/s1"), "window.title.focus: flat\n") == 0);

	/* A second time keeps what is there, --force replaces it */
	put("fl/keys", "mine\n");
	assert(wm_import_fluxbox(src, dst, false, log) == 0);
	assert(strcmp(get("fl/keys"), "mine\n") == 0);
	assert(wm_import_fluxbox(src, dst, true, log) == 0);
	assert(strcmp(get("fl/keys"), "mine\n") != 0);

	/* Nothing to import */
	snprintf(src, sizeof(src), "%s/none", root);
	assert(wm_import_fluxbox(src, dst, false, log) == 1);
	fclose(log);
	printf("  PASS: import\n");
}

int
main(void)
{
	mkdir("/tmp/fluxland-test", 0755);
	printf("test_import:\n");
	test_startup_lines();
	test_import();
	char cmd[600];
	snprintf(cmd, sizeof(cmd), "rm -rf '%s'", root);
	if (system(cmd) != 0)
		return 1;
	printf("All import tests passed.\n");
	return 0;
}
