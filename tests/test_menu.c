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
struct wm_workspace *wm_workspace_get_active(struct wm_server *s)
	{ (void)s; return NULL; }
void wm_foreign_toplevel_set_minimized(struct wm_view *v, bool m)
	{ (void)v; (void)m; }
void wm_spawn_command(const char *cmd) { (void)cmd; }
void wm_focus_view(struct wm_view *v, struct wlr_surface *s)
	{ (void)v; (void)s; }
int wm_rules_remember_window(struct wm_view *v, const char *p)
	{ (void)v; (void)p; return 0; }
#include "ipc.h"
void wm_ipc_broadcast_event(struct wm_ipc_server *ipc,
	enum wm_ipc_event event, const char *payload)
	{ (void)ipc; (void)event; (void)payload; }

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

/* Test: encoding block converts labels */
static void
test_encoding_block(void)
{
	write_file(TEST_MENU,
		"[begin] (Menu)\n"
		"[encoding] {ISO-8859-1}\n"
		"[exec] (Caf\xe9) {cafe}\n"
		"[endencoding]\n"
		"[exec] (Plain) {plain}\n"
		"[end]\n"
	);

	struct wm_menu *menu = wm_menu_load(NULL, TEST_MENU);
	assert(menu != NULL);
	assert(count_items(menu) == 2);

	/* First item had encoding conversion from ISO-8859-1 */
	struct wm_menu_item *item0 = get_item(menu, 0);
	assert(item0 != NULL);
	assert(item0->type == WM_MENU_EXEC);
	assert(item0->label != NULL);
	/* é in UTF-8 is 0xC3 0xA9 */
	assert(strstr(item0->label, "Caf") != NULL);
	assert(strcmp(item0->command, "cafe") == 0);

	/* Second item was outside encoding block, no conversion */
	struct wm_menu_item *item1 = get_item(menu, 1);
	assert(item1 != NULL);
	assert(strcmp(item1->label, "Plain") == 0);

	wm_menu_destroy(menu);
	printf("  PASS: test_encoding_block\n");
}

/* Test: sendto and layer simple tags */
static void
test_sendto_layer_tags(void)
{
	write_file(TEST_MENU,
		"[begin] (Win)\n"
		"[sendto] (Send To)\n"
		"[layer] (Layer)\n"
		"[end]\n"
	);

	struct wm_menu *menu = wm_menu_load(NULL, TEST_MENU);
	assert(menu != NULL);
	assert(count_items(menu) == 2);

	assert(get_item(menu, 0)->type == WM_MENU_SENDTO);
	assert(strcmp(get_item(menu, 0)->label, "Send To") == 0);
	assert(get_item(menu, 1)->type == WM_MENU_LAYER);
	assert(strcmp(get_item(menu, 1)->label, "Layer") == 0);

	wm_menu_destroy(menu);
	printf("  PASS: test_sendto_layer_tags\n");
}

/* Test: tilde expansion in include path */
static void
test_include_tilde(void)
{
	/* Write include file in /tmp so it's allowed */
	write_file(TEST_INCLUDE,
		"[exec] (TildeProg) {tilde_cmd}\n"
	);

	/* Include with relative path under /tmp */
	write_file(TEST_MENU,
		"[begin] (Menu)\n"
		"[include] (" TEST_INCLUDE ")\n"
		"[end]\n"
	);

	struct wm_menu *menu = wm_menu_load(NULL, TEST_MENU);
	assert(menu != NULL);
	assert(count_items(menu) >= 1);

	struct wm_menu_item *item = get_item(menu, 0);
	assert(item != NULL);
	assert(item->type == WM_MENU_EXEC);
	assert(strcmp(item->label, "TildeProg") == 0);

	wm_menu_destroy(menu);
	printf("  PASS: test_include_tilde\n");
}

/* Test: include path with .. is rejected */
static void
test_include_traversal_rejected(void)
{
	write_file(TEST_MENU,
		"[begin] (Menu)\n"
		"[include] (/tmp/../etc/passwd)\n"
		"[end]\n"
	);

	struct wm_menu *menu = wm_menu_load(NULL, TEST_MENU);
	assert(menu != NULL);
	/* The include with .. should be rejected, no items added */
	assert(count_items(menu) == 0);

	wm_menu_destroy(menu);
	printf("  PASS: test_include_traversal_rejected\n");
}

/* Test: stylesdir creates style entries from a directory */
static void
test_stylesdir(void)
{
	/* Create a temporary styles directory with files */
	char stylesdir[] = TEST_DIR "/styles";
	mkdir(stylesdir, 0755);

	FILE *f1 = fopen(TEST_DIR "/styles/dark.style", "w");
	assert(f1);
	fputs("# dark\n", f1);
	fclose(f1);

	FILE *f2 = fopen(TEST_DIR "/styles/light.style", "w");
	assert(f2);
	fputs("# light\n", f2);
	fclose(f2);

	write_file(TEST_MENU,
		"[begin] (Menu)\n"
		"[stylesdir] (" TEST_DIR "/styles)\n"
		"[end]\n"
	);

	struct wm_menu *menu = wm_menu_load(NULL, TEST_MENU);
	assert(menu != NULL);
	/* Should have 2 style items (dark.style and light.style) */
	assert(count_items(menu) == 2);

	struct wm_menu_item *item;
	wl_list_for_each(item, &menu->items, link) {
		assert(item->type == WM_MENU_STYLE);
		assert(item->label != NULL);
		assert(item->command != NULL);
	}

	wm_menu_destroy(menu);

	/* Clean up */
	unlink(TEST_DIR "/styles/dark.style");
	unlink(TEST_DIR "/styles/light.style");
	rmdir(stylesdir);
	printf("  PASS: test_stylesdir\n");
}

/* Test: wallpapers tag creates a submenu with image files */
static void
test_wallpapers(void)
{
	/* Create wallpaper directory */
	char wpdir[] = TEST_DIR "/walls";
	mkdir(wpdir, 0755);

	FILE *f1 = fopen(TEST_DIR "/walls/beach.jpg", "w");
	assert(f1);
	fputs("fake jpg\n", f1);
	fclose(f1);

	FILE *f2 = fopen(TEST_DIR "/walls/forest.png", "w");
	assert(f2);
	fputs("fake png\n", f2);
	fclose(f2);

	/* non-image file should be skipped */
	FILE *f3 = fopen(TEST_DIR "/walls/readme.txt", "w");
	assert(f3);
	fputs("not an image\n", f3);
	fclose(f3);

	write_file(TEST_MENU,
		"[begin] (Menu)\n"
		"[wallpapers] (Backgrounds) {" TEST_DIR "/walls}\n"
		"[end]\n"
	);

	struct wm_menu *menu = wm_menu_load(NULL, TEST_MENU);
	assert(menu != NULL);
	assert(count_items(menu) == 1);

	struct wm_menu_item *wp_item = get_item(menu, 0);
	assert(wp_item != NULL);
	assert(wp_item->type == WM_MENU_SUBMENU);
	assert(strcmp(wp_item->label, "Backgrounds") == 0);
	assert(wp_item->submenu != NULL);

	/* Submenu should have 2 image entries (beach.jpg, forest.png) */
	assert(count_items(wp_item->submenu) == 2);

	/* Verify items are EXEC with swaybg commands */
	struct wm_menu_item *sub_item;
	wl_list_for_each(sub_item, &wp_item->submenu->items, link) {
		assert(sub_item->type == WM_MENU_EXEC);
		assert(sub_item->command != NULL);
		assert(strstr(sub_item->command, "swaybg") != NULL);
	}

	wm_menu_destroy(menu);

	unlink(TEST_DIR "/walls/beach.jpg");
	unlink(TEST_DIR "/walls/forest.png");
	unlink(TEST_DIR "/walls/readme.txt");
	rmdir(wpdir);
	printf("  PASS: test_wallpapers\n");
}

/* Test: wallpapers with empty directory shows (empty) nop */
static void
test_wallpapers_empty_dir(void)
{
	char wpdir[] = TEST_DIR "/emptywalls";
	mkdir(wpdir, 0755);

	write_file(TEST_MENU,
		"[begin] (Menu)\n"
		"[wallpapers] (BG) {" TEST_DIR "/emptywalls}\n"
		"[end]\n"
	);

	struct wm_menu *menu = wm_menu_load(NULL, TEST_MENU);
	assert(menu != NULL);
	assert(count_items(menu) == 1);

	struct wm_menu_item *wp_item = get_item(menu, 0);
	assert(wp_item->submenu != NULL);
	assert(count_items(wp_item->submenu) == 1);

	struct wm_menu_item *empty = get_item(wp_item->submenu, 0);
	assert(empty->type == WM_MENU_NOP);
	assert(strcmp(empty->label, "(empty)") == 0);

	wm_menu_destroy(menu);
	rmdir(wpdir);
	printf("  PASS: test_wallpapers_empty_dir\n");
}

/* Test: stylesmenu creates a submenu with style entries */
static void
test_stylesmenu(void)
{
	char stylesdir[] = TEST_DIR "/sm_styles";
	mkdir(stylesdir, 0755);

	FILE *f1 = fopen(TEST_DIR "/sm_styles/theme_a", "w");
	assert(f1);
	fputs("# theme a\n", f1);
	fclose(f1);

	write_file(TEST_MENU,
		"[begin] (Menu)\n"
		"[stylesmenu] (Themes) {" TEST_DIR "/sm_styles}\n"
		"[end]\n"
	);

	struct wm_menu *menu = wm_menu_load(NULL, TEST_MENU);
	assert(menu != NULL);
	assert(count_items(menu) == 1);

	struct wm_menu_item *sm_item = get_item(menu, 0);
	assert(sm_item->type == WM_MENU_SUBMENU);
	assert(strcmp(sm_item->label, "Themes") == 0);
	assert(sm_item->submenu != NULL);
	assert(count_items(sm_item->submenu) >= 1);

	wm_menu_destroy(menu);

	unlink(TEST_DIR "/sm_styles/theme_a");
	rmdir(stylesdir);
	printf("  PASS: test_stylesmenu\n");
}

/* Test: workspace menu creation */
static void
test_workspace_menu(void)
{
	struct wm_server server;
	memset(&server, 0, sizeof(server));
	wl_list_init(&server.workspaces);
	wl_list_init(&server.views);

	struct wm_workspace ws0 = {0};
	ws0.name = "Desktop 1";
	ws0.index = 0;
	wl_list_insert(server.workspaces.prev, &ws0.link);

	struct wm_workspace ws1 = {0};
	ws1.name = "Desktop 2";
	ws1.index = 1;
	wl_list_insert(server.workspaces.prev, &ws1.link);

	struct wm_menu *menu = wm_menu_create_workspace_menu(&server);
	assert(menu != NULL);
	assert(count_items(menu) == 2);

	struct wm_menu_item *item0 = get_item(menu, 0);
	assert(item0->type == WM_MENU_COMMAND);
	assert(strstr(item0->label, "Desktop 1") != NULL);
	assert(item0->command != NULL);
	assert(strstr(item0->command, "SendToWorkspace") != NULL);

	struct wm_menu_item *item1 = get_item(menu, 1);
	assert(item1->type == WM_MENU_COMMAND);
	assert(strstr(item1->label, "Desktop 2") != NULL);

	wm_menu_destroy(menu);

	wl_list_remove(&ws0.link);
	wl_list_remove(&ws1.link);
	printf("  PASS: test_workspace_menu\n");
}

/* Test: window menu creation */
static void
test_window_menu(void)
{
	struct wm_server server;
	memset(&server, 0, sizeof(server));
	wl_list_init(&server.workspaces);
	wl_list_init(&server.views);

	struct wm_menu *menu = wm_menu_create_window_menu(&server);
	assert(menu != NULL);
	/* Window menu has a standard set of items */
	assert(count_items(menu) > 5);

	/* Check it has the standard types */
	bool has_shade = false;
	bool has_close = false;
	bool has_maximize = false;
	struct wm_menu_item *item;
	wl_list_for_each(item, &menu->items, link) {
		if (item->type == WM_MENU_SHADE) has_shade = true;
		if (item->type == WM_MENU_CLOSE) has_close = true;
		if (item->type == WM_MENU_MAXIMIZE) has_maximize = true;
	}
	assert(has_shade);
	assert(has_close);
	assert(has_maximize);

	wm_menu_destroy(menu);
	printf("  PASS: test_window_menu\n");
}

/* Test: window list creation */
static void
test_window_list(void)
{
	struct wm_server server;
	memset(&server, 0, sizeof(server));
	wl_list_init(&server.workspaces);
	wl_list_init(&server.views);

	struct wm_workspace ws = {0};
	ws.name = "Main";
	ws.index = 0;
	wl_list_insert(server.workspaces.prev, &ws.link);

	/* Create a mock view */
	struct wlr_scene_tree scene_tree;
	memset(&scene_tree, 0, sizeof(scene_tree));
	scene_tree.node.enabled = true;

	struct wm_view view;
	memset(&view, 0, sizeof(view));
	view.title = "Test Window";
	view.app_id = "test";
	view.workspace = &ws;
	view.scene_tree = &scene_tree;
	wl_list_insert(server.views.prev, &view.link);

	server.focused_view = &view;

	struct wm_menu *menu = wm_menu_create_window_list(&server);
	assert(menu != NULL);
	assert(count_items(menu) >= 2); /* header + window entry */

	/* Check for WM_MENU_WINDOW_ENTRY */
	bool has_entry = false;
	struct wm_menu_item *item;
	wl_list_for_each(item, &menu->items, link) {
		if (item->type == WM_MENU_WINDOW_ENTRY) {
			has_entry = true;
			/* Focused view should have * prefix */
			assert(strstr(item->label, "Test Window") != NULL);
			assert(item->data == &view);
		}
	}
	assert(has_entry);

	wm_menu_destroy(menu);

	wl_list_remove(&ws.link);
	wl_list_remove(&view.link);
	printf("  PASS: test_window_list\n");
}

/* Test: window list with iconified window shows brackets */
static void
test_window_list_iconified(void)
{
	struct wm_server server;
	memset(&server, 0, sizeof(server));
	wl_list_init(&server.workspaces);
	wl_list_init(&server.views);

	struct wm_workspace ws = {0};
	ws.name = "Main";
	ws.index = 0;
	wl_list_insert(server.workspaces.prev, &ws.link);

	struct wlr_scene_tree scene_tree;
	memset(&scene_tree, 0, sizeof(scene_tree));
	scene_tree.node.enabled = false; /* iconified */

	struct wm_view view;
	memset(&view, 0, sizeof(view));
	view.title = "Hidden";
	view.workspace = &ws;
	view.scene_tree = &scene_tree;
	wl_list_insert(server.views.prev, &view.link);

	server.focused_view = NULL;

	struct wm_menu *menu = wm_menu_create_window_list(&server);
	assert(menu != NULL);

	/* Find window entry with brackets */
	bool found_bracketed = false;
	struct wm_menu_item *item;
	wl_list_for_each(item, &menu->items, link) {
		if (item->type == WM_MENU_WINDOW_ENTRY) {
			assert(strstr(item->label, "[Hidden]") != NULL);
			found_bracketed = true;
		}
	}
	assert(found_bracketed);

	wm_menu_destroy(menu);

	wl_list_remove(&ws.link);
	wl_list_remove(&view.link);
	printf("  PASS: test_window_list_iconified\n");
}

/* Test: window list with untitled window uses app_id */
static void
test_window_list_untitled(void)
{
	struct wm_server server;
	memset(&server, 0, sizeof(server));
	wl_list_init(&server.workspaces);
	wl_list_init(&server.views);

	struct wm_workspace ws = {0};
	ws.name = "Main";
	ws.index = 0;
	wl_list_insert(server.workspaces.prev, &ws.link);

	struct wlr_scene_tree scene_tree;
	memset(&scene_tree, 0, sizeof(scene_tree));
	scene_tree.node.enabled = true;

	struct wm_view view;
	memset(&view, 0, sizeof(view));
	view.title = NULL;
	view.app_id = "my-app";
	view.workspace = &ws;
	view.scene_tree = &scene_tree;
	wl_list_insert(server.views.prev, &view.link);

	server.focused_view = NULL;

	struct wm_menu *menu = wm_menu_create_window_list(&server);
	assert(menu != NULL);

	/* Entry should use app_id as fallback */
	struct wm_menu_item *item;
	wl_list_for_each(item, &menu->items, link) {
		if (item->type == WM_MENU_WINDOW_ENTRY) {
			assert(strstr(item->label, "my-app") != NULL);
		}
	}

	wm_menu_destroy(menu);

	wl_list_remove(&ws.link);
	wl_list_remove(&view.link);
	printf("  PASS: test_window_list_untitled\n");
}

/* Test: window list empty workspace is skipped */
static void
test_window_list_empty_ws(void)
{
	struct wm_server server;
	memset(&server, 0, sizeof(server));
	wl_list_init(&server.workspaces);
	wl_list_init(&server.views);

	struct wm_workspace ws0 = {0};
	ws0.name = "Empty";
	ws0.index = 0;
	wl_list_insert(server.workspaces.prev, &ws0.link);

	struct wm_workspace ws1 = {0};
	ws1.name = "Has Views";
	ws1.index = 1;
	wl_list_insert(server.workspaces.prev, &ws1.link);

	struct wlr_scene_tree scene_tree;
	memset(&scene_tree, 0, sizeof(scene_tree));
	scene_tree.node.enabled = true;

	struct wm_view view;
	memset(&view, 0, sizeof(view));
	view.title = "View1";
	view.workspace = &ws1;
	view.scene_tree = &scene_tree;
	wl_list_insert(server.views.prev, &view.link);

	server.focused_view = NULL;

	struct wm_menu *menu = wm_menu_create_window_list(&server);
	assert(menu != NULL);

	/* Only ws1 should appear (ws0 has no views) */
	/* Should have header + entry = 2 items */
	assert(count_items(menu) == 2);

	wm_menu_destroy(menu);

	wl_list_remove(&ws0.link);
	wl_list_remove(&ws1.link);
	wl_list_remove(&view.link);
	printf("  PASS: test_window_list_empty_ws\n");
}

/* Test: exec item with nested parens in label */
static void
test_nested_paren_label(void)
{
	write_file(TEST_MENU,
		"[begin] (Menu)\n"
		"[exec] (App (v2.0)) {myapp}\n"
		"[end]\n"
	);

	struct wm_menu *menu = wm_menu_load(NULL, TEST_MENU);
	assert(menu != NULL);
	assert(count_items(menu) == 1);

	struct wm_menu_item *item = get_item(menu, 0);
	assert(item->type == WM_MENU_EXEC);
	assert(strcmp(item->label, "App (v2.0)") == 0);

	wm_menu_destroy(menu);
	printf("  PASS: test_nested_paren_label\n");
}

/* Test: exec item with nested braces in command */
static void
test_nested_brace_command(void)
{
	write_file(TEST_MENU,
		"[begin] (Menu)\n"
		"[exec] (Test) {sh -c \"echo ${HOME}\"}\n"
		"[end]\n"
	);

	struct wm_menu *menu = wm_menu_load(NULL, TEST_MENU);
	assert(menu != NULL);
	assert(count_items(menu) == 1);

	struct wm_menu_item *item = get_item(menu, 0);
	assert(item->type == WM_MENU_EXEC);
	/* Nested braces should be correctly parsed */
	assert(strstr(item->command, "echo") != NULL);

	wm_menu_destroy(menu);
	printf("  PASS: test_nested_brace_command\n");
}

/* Test: submenu with no title uses label */
static void
test_submenu_label_as_title(void)
{
	write_file(TEST_MENU,
		"[begin] (Root)\n"
		"[submenu] (MyApps)\n"
		"  [exec] (App) {app}\n"
		"[end]\n"
		"[end]\n"
	);

	struct wm_menu *menu = wm_menu_load(NULL, TEST_MENU);
	assert(menu != NULL);
	assert(count_items(menu) == 1);

	struct wm_menu_item *sub = get_item(menu, 0);
	assert(sub->submenu != NULL);
	/* When no brace title, submenu title should be the label */
	assert(strcmp(sub->submenu->title, "MyApps") == 0);

	wm_menu_destroy(menu);
	printf("  PASS: test_submenu_label_as_title\n");
}

/* Test: unknown tag is silently ignored */
static void
test_unknown_tag_ignored(void)
{
	write_file(TEST_MENU,
		"[begin] (Menu)\n"
		"[unknowntag] (Foo) {bar}\n"
		"[exec] (A) {a}\n"
		"[end]\n"
	);

	struct wm_menu *menu = wm_menu_load(NULL, TEST_MENU);
	assert(menu != NULL);
	/* Unknown tag is skipped, only exec item remains */
	assert(count_items(menu) == 1);
	assert(get_item(menu, 0)->type == WM_MENU_EXEC);

	wm_menu_destroy(menu);
	printf("  PASS: test_unknown_tag_ignored\n");
}

/* Test: line without tag bracket is skipped */
static void
test_no_bracket_skipped(void)
{
	write_file(TEST_MENU,
		"[begin] (Menu)\n"
		"not a valid line\n"
		"  also not valid\n"
		"[exec] (Valid) {cmd}\n"
		"[end]\n"
	);

	struct wm_menu *menu = wm_menu_load(NULL, TEST_MENU);
	assert(menu != NULL);
	assert(count_items(menu) == 1);

	wm_menu_destroy(menu);
	printf("  PASS: test_no_bracket_skipped\n");
}

/* Test: submenu with icon */
static void
test_submenu_with_icon(void)
{
	write_file(TEST_MENU,
		"[begin] (Root)\n"
		"[submenu] (Apps) {My Apps} <folder.png>\n"
		"  [exec] (A) {a}\n"
		"[end]\n"
		"[end]\n"
	);

	struct wm_menu *menu = wm_menu_load(NULL, TEST_MENU);
	assert(menu != NULL);
	assert(count_items(menu) == 1);

	struct wm_menu_item *sub = get_item(menu, 0);
	assert(sub->type == WM_MENU_SUBMENU);
	assert(sub->icon_path != NULL);
	assert(strcmp(sub->icon_path, "folder.png") == 0);
	assert(sub->submenu != NULL);
	assert(strcmp(sub->submenu->title, "My Apps") == 0);

	wm_menu_destroy(menu);
	printf("  PASS: test_submenu_with_icon\n");
}

/* Test: style with icon */
static void
test_style_with_icon(void)
{
	write_file(TEST_MENU,
		"[begin] (Menu)\n"
		"[style] (Dracula) {/themes/dracula} <dracula.png>\n"
		"[end]\n"
	);

	struct wm_menu *menu = wm_menu_load(NULL, TEST_MENU);
	assert(menu != NULL);
	assert(count_items(menu) == 1);

	struct wm_menu_item *item = get_item(menu, 0);
	assert(item->type == WM_MENU_STYLE);
	assert(item->icon_path != NULL);
	assert(strcmp(item->icon_path, "dracula.png") == 0);

	wm_menu_destroy(menu);
	printf("  PASS: test_style_with_icon\n");
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

	/* Encoding conversion */
	test_encoding_block();

	/* Additional simple tags */
	test_sendto_layer_tags();

	/* Include edge cases */
	test_include_tilde();
	test_include_traversal_rejected();

	/* Directory scanning tags */
	test_stylesdir();
	test_wallpapers();
	test_wallpapers_empty_dir();
	test_stylesmenu();

	/* Dynamic menu creation */
	test_workspace_menu();
	test_window_menu();
	test_window_list();
	test_window_list_iconified();
	test_window_list_untitled();
	test_window_list_empty_ws();

	/* Parser edge cases */
	test_nested_paren_label();
	test_nested_brace_command();
	test_submenu_label_as_title();
	test_unknown_tag_ignored();
	test_no_bracket_skipped();
	test_submenu_with_icon();
	test_style_with_icon();

	cleanup();
	printf("All menu tests passed.\n");
	return 0;
}
