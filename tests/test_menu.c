/*
 * test_menu.c - Unit tests for menu file parser
 *
 * Tests the parsing layer of menu.c (wm_menu_load, wm_menu_destroy).
 * The rendering/display functions require a running compositor, so
 * we only test menu loading and item structure here.
 */

#define _GNU_SOURCE

#include "menu.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/*
 * Stubs for compositor functions referenced in menu.c but not
 * exercised by the parser-only tests below.
 */
#include "render.h"
#include "view.h"
#include "workspace.h"
#include "foreign_toplevel.h"

cairo_surface_t *wm_render_texture(const struct wm_texture *t,
	int w, int h, float s)
	{ (void)t; (void)w; (void)h; (void)s; return NULL; }
cairo_surface_t *wm_render_text(const char *text, const struct wm_font *f,
	const struct wm_color *c, int max_w, int *tw, int *th,
	enum wm_justify j, float s)
	{ (void)text; (void)f; (void)c; (void)max_w; (void)tw; (void)th;
	  (void)j; (void)s; return NULL; }
int wm_measure_text_width(const char *text, const struct wm_font *f,
	float s)
	{ (void)f; (void)s;
	  return text ? (int)(strlen(text) * 7) : 0; }
int style_load(struct wm_style *s, const char *p)
	{ (void)s; (void)p; return 0; }
void wm_view_set_sticky(struct wm_view *v, bool s)
	{ (void)v; (void)s; }
void wm_view_raise(struct wm_view *v)
	{ (void)v; }
void wm_workspace_switch(struct wm_server *s, int i)
	{ (void)s; (void)i; }
void wm_foreign_toplevel_set_minimized(struct wm_view *v, bool m)
	{ (void)v; (void)m; }
void wm_focus_view(struct wm_view *v, struct wlr_surface *s)
	{ (void)v; (void)s; }
int wm_rules_remember_window(struct wm_view *v, const char *p)
	{ (void)v; (void)p; return 0; }

#define TEST_DIR "/tmp/fluxland-test/wm-test-menu"
#define TEST_MENU TEST_DIR "/menu"
#define TEST_INCLUDE TEST_DIR "/include_menu"

static void
setup(void)
{
	mkdir("/tmp/fluxland-test", 0755);
	mkdir(TEST_DIR, 0755);
}

static void
cleanup(void)
{
	unlink(TEST_MENU);
	unlink(TEST_INCLUDE);
	rmdir(TEST_DIR);
}

static void
write_file(const char *path, const char *contents)
{
	FILE *f = fopen(path, "w");
	assert(f);
	fputs(contents, f);
	fclose(f);
}

/* Helper to count items in a menu */
static int
count_items(struct wm_menu *menu)
{
	int n = 0;
	struct wm_menu_item *item;
	wl_list_for_each(item, &menu->items, link)
		n++;
	return n;
}

/* Helper to get nth item from menu */
static struct wm_menu_item *
get_item(struct wm_menu *menu, int index)
{
	int i = 0;
	struct wm_menu_item *item;
	wl_list_for_each(item, &menu->items, link) {
		if (i == index)
			return item;
		i++;
	}
	return NULL;
}

/* Test: loading NULL path returns NULL */
static void
test_load_null(void)
{
	struct wm_menu *menu = wm_menu_load(NULL, NULL);
	assert(menu == NULL);
	printf("  PASS: test_load_null\n");
}

/* Test: loading nonexistent file returns NULL */
static void
test_load_nonexistent(void)
{
	struct wm_menu *menu = wm_menu_load(NULL,
		"/tmp/fluxland-test/nonexistent_menu_file");
	assert(menu == NULL);
	printf("  PASS: test_load_nonexistent\n");
}

/* Test: loading an empty file returns menu with default title */
static void
test_load_empty(void)
{
	write_file(TEST_MENU, "# empty\n");

	struct wm_menu *menu = wm_menu_load(NULL, TEST_MENU);
	assert(menu != NULL);
	assert(menu->title != NULL);
	assert(strcmp(menu->title, "Menu") == 0);
	assert(count_items(menu) == 0);

	wm_menu_destroy(menu);
	printf("  PASS: test_load_empty\n");
}

/* Test: begin tag sets menu title */
static void
test_begin_title(void)
{
	write_file(TEST_MENU,
		"[begin] (My Root Menu)\n"
		"[end]\n"
	);

	struct wm_menu *menu = wm_menu_load(NULL, TEST_MENU);
	assert(menu != NULL);
	assert(menu->title != NULL);
	assert(strcmp(menu->title, "My Root Menu") == 0);

	wm_menu_destroy(menu);
	printf("  PASS: test_begin_title\n");
}

/* Test: exec items */
static void
test_exec_items(void)
{
	write_file(TEST_MENU,
		"[begin] (Menu)\n"
		"[exec] (Terminal) {foot}\n"
		"[exec] (Browser) {firefox} <firefox.png>\n"
		"[end]\n"
	);

	struct wm_menu *menu = wm_menu_load(NULL, TEST_MENU);
	assert(menu != NULL);
	assert(count_items(menu) == 2);

	struct wm_menu_item *item0 = get_item(menu, 0);
	assert(item0 != NULL);
	assert(item0->type == WM_MENU_EXEC);
	assert(item0->label != NULL);
	assert(strcmp(item0->label, "Terminal") == 0);
	assert(item0->command != NULL);
	assert(strcmp(item0->command, "foot") == 0);
	assert(item0->icon_path == NULL);

	struct wm_menu_item *item1 = get_item(menu, 1);
	assert(item1 != NULL);
	assert(item1->type == WM_MENU_EXEC);
	assert(strcmp(item1->label, "Browser") == 0);
	assert(strcmp(item1->command, "firefox") == 0);
	assert(item1->icon_path != NULL);
	assert(strcmp(item1->icon_path, "firefox.png") == 0);

	wm_menu_destroy(menu);
	printf("  PASS: test_exec_items\n");
}

/* Test: separator items */
static void
test_separator(void)
{
	write_file(TEST_MENU,
		"[begin] (Menu)\n"
		"[exec] (A) {a}\n"
		"[separator]\n"
		"[exec] (B) {b}\n"
		"[end]\n"
	);

	struct wm_menu *menu = wm_menu_load(NULL, TEST_MENU);
	assert(menu != NULL);
	assert(count_items(menu) == 3);

	struct wm_menu_item *sep = get_item(menu, 1);
	assert(sep != NULL);
	assert(sep->type == WM_MENU_SEPARATOR);
	assert(sep->label == NULL);

	wm_menu_destroy(menu);
	printf("  PASS: test_separator\n");
}

/* Test: submenu items */
static void
test_submenu(void)
{
	write_file(TEST_MENU,
		"[begin] (Root)\n"
		"[submenu] (Editors) {My Editors}\n"
		"  [exec] (Vim) {vim}\n"
		"  [exec] (Emacs) {emacs}\n"
		"[end]\n"
		"[end]\n"
	);

	struct wm_menu *menu = wm_menu_load(NULL, TEST_MENU);
	assert(menu != NULL);
	assert(count_items(menu) == 1);

	struct wm_menu_item *sub_item = get_item(menu, 0);
	assert(sub_item != NULL);
	assert(sub_item->type == WM_MENU_SUBMENU);
	assert(strcmp(sub_item->label, "Editors") == 0);
	assert(sub_item->submenu != NULL);
	assert(sub_item->submenu->title != NULL);
	assert(strcmp(sub_item->submenu->title, "My Editors") == 0);
	assert(sub_item->submenu->parent == menu);

	/* Check submenu items */
	assert(count_items(sub_item->submenu) == 2);
	struct wm_menu_item *vim = get_item(sub_item->submenu, 0);
	assert(vim != NULL);
	assert(vim->type == WM_MENU_EXEC);
	assert(strcmp(vim->label, "Vim") == 0);
	assert(strcmp(vim->command, "vim") == 0);

	wm_menu_destroy(menu);
	printf("  PASS: test_submenu\n");
}

/* Test: nested submenus */
static void
test_nested_submenus(void)
{
	write_file(TEST_MENU,
		"[begin] (Root)\n"
		"[submenu] (Level 1)\n"
		"  [submenu] (Level 2)\n"
		"    [exec] (Deep) {deep_cmd}\n"
		"  [end]\n"
		"[end]\n"
		"[end]\n"
	);

	struct wm_menu *menu = wm_menu_load(NULL, TEST_MENU);
	assert(menu != NULL);
	assert(count_items(menu) == 1);

	struct wm_menu_item *l1 = get_item(menu, 0);
	assert(l1 != NULL);
	assert(l1->submenu != NULL);
	assert(count_items(l1->submenu) == 1);

	struct wm_menu_item *l2 = get_item(l1->submenu, 0);
	assert(l2 != NULL);
	assert(l2->submenu != NULL);
	assert(count_items(l2->submenu) == 1);

	struct wm_menu_item *deep = get_item(l2->submenu, 0);
	assert(deep != NULL);
	assert(deep->type == WM_MENU_EXEC);
	assert(strcmp(deep->label, "Deep") == 0);
	assert(strcmp(deep->command, "deep_cmd") == 0);

	/* Verify parent chain */
	assert(l2->submenu->parent == l1->submenu);
	assert(l1->submenu->parent == menu);

	wm_menu_destroy(menu);
	printf("  PASS: test_nested_submenus\n");
}

/* Test: nop items */
static void
test_nop(void)
{
	write_file(TEST_MENU,
		"[begin] (Menu)\n"
		"[nop] (Disabled Item)\n"
		"[end]\n"
	);

	struct wm_menu *menu = wm_menu_load(NULL, TEST_MENU);
	assert(menu != NULL);
	assert(count_items(menu) == 1);

	struct wm_menu_item *item = get_item(menu, 0);
	assert(item != NULL);
	assert(item->type == WM_MENU_NOP);
	assert(strcmp(item->label, "Disabled Item") == 0);

	wm_menu_destroy(menu);
	printf("  PASS: test_nop\n");
}

/* Test: simple tag types (exit, reconfig, config, workspaces) */
static void
test_simple_tags(void)
{
	write_file(TEST_MENU,
		"[begin] (Menu)\n"
		"[config] (Configuration)\n"
		"[workspaces] (Workspaces)\n"
		"[reconfig] (Reload)\n"
		"[restart] (Restart) {fluxland}\n"
		"[exit] (Quit)\n"
		"[end]\n"
	);

	struct wm_menu *menu = wm_menu_load(NULL, TEST_MENU);
	assert(menu != NULL);
	assert(count_items(menu) == 5);

	struct wm_menu_item *item;
	item = get_item(menu, 0);
	assert(item->type == WM_MENU_CONFIG);
	assert(strcmp(item->label, "Configuration") == 0);

	item = get_item(menu, 1);
	assert(item->type == WM_MENU_WORKSPACES);

	item = get_item(menu, 2);
	assert(item->type == WM_MENU_RECONFIG);

	item = get_item(menu, 3);
	assert(item->type == WM_MENU_RESTART);
	assert(strcmp(item->label, "Restart") == 0);
	assert(item->command != NULL);
	assert(strcmp(item->command, "fluxland") == 0);

	item = get_item(menu, 4);
	assert(item->type == WM_MENU_EXIT);

	wm_menu_destroy(menu);
	printf("  PASS: test_simple_tags\n");
}

/* Test: style items */
static void
test_style_item(void)
{
	write_file(TEST_MENU,
		"[begin] (Menu)\n"
		"[style] (Dark Theme) {/usr/share/themes/dark}\n"
		"[end]\n"
	);

	struct wm_menu *menu = wm_menu_load(NULL, TEST_MENU);
	assert(menu != NULL);
	assert(count_items(menu) == 1);

	struct wm_menu_item *item = get_item(menu, 0);
	assert(item->type == WM_MENU_STYLE);
	assert(strcmp(item->label, "Dark Theme") == 0);
	assert(strcmp(item->command, "/usr/share/themes/dark") == 0);

	wm_menu_destroy(menu);
	printf("  PASS: test_style_item\n");
}

/* Test: command items */
static void
test_command_item(void)
{
	write_file(TEST_MENU,
		"[begin] (Menu)\n"
		"[command] (Tile Grid) {ArrangeWindowsGrid}\n"
		"[end]\n"
	);

	struct wm_menu *menu = wm_menu_load(NULL, TEST_MENU);
	assert(menu != NULL);
	assert(count_items(menu) == 1);

	struct wm_menu_item *item = get_item(menu, 0);
	assert(item->type == WM_MENU_COMMAND);
	assert(strcmp(item->label, "Tile Grid") == 0);
	assert(strcmp(item->command, "ArrangeWindowsGrid") == 0);

	wm_menu_destroy(menu);
	printf("  PASS: test_command_item\n");
}

/* Test: comments are skipped */
static void
test_comments(void)
{
	write_file(TEST_MENU,
		"# A comment\n"
		"[begin] (Menu)\n"
		"# Another comment\n"
		"[exec] (A) {a}\n"
		"[end]\n"
	);

	struct wm_menu *menu = wm_menu_load(NULL, TEST_MENU);
	assert(menu != NULL);
	assert(count_items(menu) == 1);

	wm_menu_destroy(menu);
	printf("  PASS: test_comments\n");
}

/* Test: complex menu with mixed item types */
static void
test_complex_menu(void)
{
	write_file(TEST_MENU,
		"[begin] (fluxland)\n"
		"[exec] (Terminal) {foot}\n"
		"[exec] (Files) {nautilus}\n"
		"[separator]\n"
		"[submenu] (Editors)\n"
		"  [exec] (Vim) {vim}\n"
		"  [exec] (Nano) {nano}\n"
		"[end]\n"
		"[separator]\n"
		"[config] (Settings)\n"
		"[reconfig] (Reload Config)\n"
		"[separator]\n"
		"[restart] (Restart)\n"
		"[exit] (Exit)\n"
		"[end]\n"
	);

	struct wm_menu *menu = wm_menu_load(NULL, TEST_MENU);
	assert(menu != NULL);
	assert(strcmp(menu->title, "fluxland") == 0);
	assert(count_items(menu) == 10);

	/* Verify structure */
	assert(get_item(menu, 0)->type == WM_MENU_EXEC);
	assert(get_item(menu, 1)->type == WM_MENU_EXEC);
	assert(get_item(menu, 2)->type == WM_MENU_SEPARATOR);
	assert(get_item(menu, 3)->type == WM_MENU_SUBMENU);
	assert(get_item(menu, 4)->type == WM_MENU_SEPARATOR);
	assert(get_item(menu, 5)->type == WM_MENU_CONFIG);
	assert(get_item(menu, 6)->type == WM_MENU_RECONFIG);
	assert(get_item(menu, 7)->type == WM_MENU_SEPARATOR);
	assert(get_item(menu, 8)->type == WM_MENU_RESTART);
	assert(get_item(menu, 9)->type == WM_MENU_EXIT);

	/* Check submenu has 2 items */
	struct wm_menu_item *editors = get_item(menu, 3);
	assert(editors->submenu != NULL);
	assert(count_items(editors->submenu) == 2);

	wm_menu_destroy(menu);
	printf("  PASS: test_complex_menu\n");
}

/* Test: destroy NULL is safe */
static void
test_destroy_null(void)
{
	wm_menu_destroy(NULL);
	printf("  PASS: test_destroy_null\n");
}

/* Test: include directive */
static void
test_include(void)
{
	write_file(TEST_INCLUDE,
		"[exec] (Included App) {included_cmd}\n"
	);

	write_file(TEST_MENU,
		"[begin] (Menu)\n"
		"[exec] (Local App) {local_cmd}\n"
		"[include] (" TEST_INCLUDE ")\n"
		"[end]\n"
	);

	struct wm_menu *menu = wm_menu_load(NULL, TEST_MENU);
	assert(menu != NULL);
	assert(count_items(menu) == 2);

	struct wm_menu_item *local = get_item(menu, 0);
	assert(local->type == WM_MENU_EXEC);
	assert(strcmp(local->label, "Local App") == 0);

	struct wm_menu_item *included = get_item(menu, 1);
	assert(included->type == WM_MENU_EXEC);
	assert(strcmp(included->label, "Included App") == 0);
	assert(strcmp(included->command, "included_cmd") == 0);

	wm_menu_destroy(menu);
	printf("  PASS: test_include\n");
}

/* Test: window menu item types */
static void
test_window_menu_tags(void)
{
	write_file(TEST_MENU,
		"[begin] (Window)\n"
		"[shade] (Shade)\n"
		"[stick] (Stick)\n"
		"[maximize] (Maximize)\n"
		"[iconify] (Iconify)\n"
		"[close] (Close)\n"
		"[kill] (Kill)\n"
		"[raise] (Raise)\n"
		"[lower] (Lower)\n"
		"[end]\n"
	);

	struct wm_menu *menu = wm_menu_load(NULL, TEST_MENU);
	assert(menu != NULL);
	assert(count_items(menu) == 8);

	assert(get_item(menu, 0)->type == WM_MENU_SHADE);
	assert(get_item(menu, 1)->type == WM_MENU_STICK);
	assert(get_item(menu, 2)->type == WM_MENU_MAXIMIZE);
	assert(get_item(menu, 3)->type == WM_MENU_ICONIFY);
	assert(get_item(menu, 4)->type == WM_MENU_CLOSE);
	assert(get_item(menu, 5)->type == WM_MENU_KILL);
	assert(get_item(menu, 6)->type == WM_MENU_RAISE);
	assert(get_item(menu, 7)->type == WM_MENU_LOWER);

	wm_menu_destroy(menu);
	printf("  PASS: test_window_menu_tags\n");
}

int
main(void)
{
	printf("test_menu:\n");
	setup();

	test_load_null();
	test_load_nonexistent();
	test_load_empty();
	test_begin_title();
	test_exec_items();
	test_separator();
	test_submenu();
	test_nested_submenus();
	test_nop();
	test_simple_tags();
	test_style_item();
	test_command_item();
	test_comments();
	test_complex_menu();
	test_destroy_null();
	test_include();
	test_window_menu_tags();

	cleanup();
	printf("All menu tests passed.\n");
	return 0;
}
