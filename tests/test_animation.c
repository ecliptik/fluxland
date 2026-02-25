/*
 * test_animation.c - Unit tests for animation system
 *
 * Includes animation.c directly with stub implementations to avoid
 * needing wlroots/wayland libraries at link time. Header guards block
 * the real wayland/wlroots headers so our stub types are used instead.
 */

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- Block real headers via their include guards --- */
#define WAYLAND_SERVER_CORE_H
#define WAYLAND_SERVER_H
#define WAYLAND_UTIL_H
#define WLR_UTIL_LOG_H
#define WM_VIEW_H
#define WM_SERVER_H

/* --- Stub wayland types --- */

struct wl_list {
	struct wl_list *prev;
	struct wl_list *next;
};

struct wl_event_source {
	int (*callback)(void *data);
	void *data;
	int next_ms;
	bool removed;
};

struct wl_event_loop {
	int dummy;
};

/* --- Stub wlr types --- */
struct wlr_scene_tree { int dummy; };
struct wlr_xdg_toplevel { int dummy; };
struct wlr_box { int x, y, width, height; };

/* wlr_log is a macro in the real header; define a no-op version */
#define wlr_log(verb, fmt, ...) ((void)0)
#define WLR_ERROR 1

/* --- Stub wayland event functions --- */

static struct wl_event_source g_stub_source;

static struct wl_event_source *
wl_event_loop_add_timer(struct wl_event_loop *loop,
	int (*callback)(void *data), void *data)
{
	if (!loop)
		return NULL;
	g_stub_source.callback = callback;
	g_stub_source.data = data;
	g_stub_source.next_ms = 0;
	g_stub_source.removed = false;
	return &g_stub_source;
}

static int
wl_event_source_timer_update(struct wl_event_source *source, int ms)
{
	if (source)
		source->next_ms = ms;
	return 0;
}

static int
wl_event_source_remove(struct wl_event_source *source)
{
	if (source)
		source->removed = true;
	return 0;
}

/* --- Minimal struct definitions needed by animation.c --- */

struct wm_animation;

struct wm_server {
	struct wl_event_loop *wl_event_loop;
};

struct wm_view {
	struct wl_list link;
	struct wm_server *server;
	struct wlr_xdg_toplevel *xdg_toplevel;
	struct wlr_scene_tree *scene_tree;
	uint32_t id;
	int x, y;
	struct wlr_box saved_geometry;
	bool maximized, fullscreen;
	bool maximized_vert, maximized_horiz;
	bool lhalf, rhalf;
	bool show_decoration;
	char *title;
	char *app_id;
	void *workspace;
	bool sticky;
	int layer;
	void *tab_group;
	struct wl_list tab_link;
	int focus_alpha;
	int unfocus_alpha;
	struct wm_animation *animation;
	int _test_opacity;
};

/* --- Stub wm_view_set_opacity --- */

static int g_opacity_set_count;
static int g_last_opacity;

static void
wm_view_set_opacity(struct wm_view *view, int alpha)
{
	if (!view)
		return;
	if (alpha < 0) alpha = 0;
	if (alpha > 255) alpha = 255;
	view->_test_opacity = alpha;
	g_last_opacity = alpha;
	g_opacity_set_count++;
}

/* --- Include animation source directly (uses our stubs) --- */

#include "animation.h"
#include "animation.c"

/* --- Test callback tracking --- */

static int g_done_called;
static bool g_done_completed;
static void *g_done_data;
static struct wm_view *g_done_view;

static void
test_done_callback(struct wm_view *view, bool completed, void *data)
{
	g_done_called++;
	g_done_completed = completed;
	g_done_data = data;
	g_done_view = view;
}

static void
reset_globals(void)
{
	g_opacity_set_count = 0;
	g_last_opacity = -1;
	g_done_called = 0;
	g_done_completed = false;
	g_done_data = NULL;
	g_done_view = NULL;
	memset(&g_stub_source, 0, sizeof(g_stub_source));
}

/* Helper to create a minimal test view */
static struct wl_event_loop test_loop;
static struct wm_server test_server;
static struct wlr_scene_tree test_scene_tree;

static struct wm_view
make_test_view(void)
{
	test_server.wl_event_loop = &test_loop;
	struct wm_view view;
	memset(&view, 0, sizeof(view));
	view.server = &test_server;
	view.scene_tree = &test_scene_tree;
	view.focus_alpha = 255;
	view.unfocus_alpha = 200;
	view.animation = NULL;
	view._test_opacity = 255;
	return view;
}

/* Simulate firing the timer callback N times */
static void
pump_timer(int n)
{
	for (int i = 0; i < n; i++) {
		if (g_stub_source.removed || !g_stub_source.callback)
			break;
		g_stub_source.callback(g_stub_source.data);
	}
}

/* ===== Tests ===== */

static void
test_start_null_view(void)
{
	reset_globals();
	struct wm_animation *anim = wm_animation_start(
		NULL, WM_ANIM_FADE_IN, 0, 255, 100, test_done_callback, NULL);
	assert(anim == NULL);
	assert(g_done_called == 0);
	printf("  PASS: start_null_view\n");
}

static void
test_start_null_server(void)
{
	reset_globals();
	struct wm_view view;
	memset(&view, 0, sizeof(view));
	view.server = NULL;
	struct wm_animation *anim = wm_animation_start(
		&view, WM_ANIM_FADE_IN, 0, 255, 100, test_done_callback, NULL);
	assert(anim == NULL);
	assert(g_done_called == 0);
	printf("  PASS: start_null_server\n");
}

static void
test_zero_duration(void)
{
	reset_globals();
	struct wm_view view = make_test_view();
	int cookie = 42;

	struct wm_animation *anim = wm_animation_start(
		&view, WM_ANIM_FADE_IN, 0, 200, 0,
		test_done_callback, &cookie);

	assert(anim == NULL);
	assert(g_done_called == 1);
	assert(g_done_completed == true);
	assert(g_done_data == &cookie);
	assert(g_done_view == &view);
	assert(view._test_opacity == 200);
	printf("  PASS: zero_duration\n");
}

static void
test_negative_duration(void)
{
	reset_globals();
	struct wm_view view = make_test_view();

	struct wm_animation *anim = wm_animation_start(
		&view, WM_ANIM_FADE_OUT, 255, 0, -50,
		test_done_callback, NULL);
	assert(anim == NULL);
	assert(g_done_called == 1);
	assert(g_done_completed == true);
	assert(view._test_opacity == 0);
	printf("  PASS: negative_duration\n");
}

static void
test_start_sets_initial_state(void)
{
	reset_globals();
	struct wm_view view = make_test_view();

	struct wm_animation *anim = wm_animation_start(
		&view, WM_ANIM_FADE_IN, 50, 200, 160,
		test_done_callback, NULL);

	assert(anim != NULL);
	assert(anim->view == &view);
	assert(anim->type == WM_ANIM_FADE_IN);
	assert(anim->start_alpha == 50);
	assert(anim->target_alpha == 200);
	assert(anim->duration_ms == 160);
	assert(anim->elapsed_ms == 0);
	assert(anim->done_fn == test_done_callback);
	assert(view._test_opacity == 50);
	assert(!g_stub_source.removed);
	assert(g_stub_source.next_ms == 16);
	assert(view.animation == anim);
	assert(g_done_called == 0);

	wm_animation_cancel(anim);
	printf("  PASS: start_sets_initial_state\n");
}

static void
test_interpolation(void)
{
	reset_globals();
	struct wm_view view = make_test_view();

	/* 0 -> 255 over 160ms (10 frames at 16ms each) */
	struct wm_animation *anim = wm_animation_start(
		&view, WM_ANIM_FADE_IN, 0, 255, 160,
		test_done_callback, NULL);
	assert(anim != NULL);
	assert(view._test_opacity == 0);

	/* After 1 frame (16ms): t = 0.1, alpha = 25 */
	pump_timer(1);
	assert(view._test_opacity == 25);
	assert(g_done_called == 0);

	/* After 2 frames (32ms): t = 0.2, alpha = 51 */
	pump_timer(1);
	assert(view._test_opacity == 51);

	/* After 5 frames (80ms): t = 0.5, alpha = 127 */
	pump_timer(3);
	assert(view._test_opacity == 127);
	assert(g_done_called == 0);

	wm_animation_cancel(anim);
	printf("  PASS: interpolation\n");
}

static void
test_completion(void)
{
	reset_globals();
	struct wm_view view = make_test_view();
	int marker = 99;

	/* 0 -> 200 over 48ms (3 frames) */
	struct wm_animation *anim = wm_animation_start(
		&view, WM_ANIM_FADE_IN, 0, 200, 48,
		test_done_callback, &marker);
	assert(anim != NULL);

	pump_timer(2);
	assert(g_done_called == 0);

	/* 3rd frame completes (elapsed 48 >= duration 48) */
	pump_timer(1);
	assert(g_done_called == 1);
	assert(g_done_completed == true);
	assert(g_done_data == &marker);
	assert(g_done_view == &view);
	assert(view._test_opacity == 200);
	assert(view.animation == NULL);
	assert(g_stub_source.removed);
	printf("  PASS: completion\n");
}

static void
test_cancel(void)
{
	reset_globals();
	struct wm_view view = make_test_view();
	int marker = 77;

	struct wm_animation *anim = wm_animation_start(
		&view, WM_ANIM_FADE_OUT, 255, 0, 320,
		test_done_callback, &marker);
	assert(anim != NULL);

	pump_timer(2);
	assert(g_done_called == 0);

	wm_animation_cancel(anim);
	assert(g_done_called == 1);
	assert(g_done_completed == false);
	assert(g_done_data == &marker);
	assert(view.animation == NULL);
	assert(g_stub_source.removed);
	printf("  PASS: cancel\n");
}

static void
test_cancel_null(void)
{
	reset_globals();
	wm_animation_cancel(NULL);
	assert(g_done_called == 0);
	printf("  PASS: cancel_null\n");
}

static void
test_start_cancels_existing(void)
{
	reset_globals();
	struct wm_view view = make_test_view();
	int first_data = 1;
	int second_data = 2;

	struct wm_animation *anim1 = wm_animation_start(
		&view, WM_ANIM_FADE_IN, 0, 255, 160,
		test_done_callback, &first_data);
	assert(anim1 != NULL);
	assert(view.animation == anim1);

	struct wm_animation *anim2 = wm_animation_start(
		&view, WM_ANIM_FADE_OUT, 255, 0, 160,
		test_done_callback, &second_data);
	assert(anim2 != NULL);

	/* First animation's callback fired with completed=false */
	assert(g_done_called == 1);
	assert(g_done_completed == false);
	assert(g_done_data == &first_data);
	assert(view.animation == anim2);

	wm_animation_cancel(anim2);
	printf("  PASS: start_cancels_existing\n");
}

static void
test_no_callback(void)
{
	reset_globals();
	struct wm_view view = make_test_view();

	/* Zero duration with NULL callback */
	struct wm_animation *anim = wm_animation_start(
		&view, WM_ANIM_FADE_IN, 0, 255, 0, NULL, NULL);
	assert(anim == NULL);
	assert(g_done_called == 0);
	assert(view._test_opacity == 255);

	/* Normal animation with NULL callback, cancel it */
	anim = wm_animation_start(
		&view, WM_ANIM_FADE_OUT, 255, 0, 160, NULL, NULL);
	assert(anim != NULL);
	wm_animation_cancel(anim);
	assert(g_done_called == 0);
	printf("  PASS: no_callback\n");
}

static void
test_completion_no_callback(void)
{
	reset_globals();
	struct wm_view view = make_test_view();

	struct wm_animation *anim = wm_animation_start(
		&view, WM_ANIM_FADE_IN, 0, 100, 16, NULL, NULL);
	assert(anim != NULL);

	pump_timer(1);
	assert(view._test_opacity == 100);
	assert(view.animation == NULL);
	assert(g_done_called == 0);
	printf("  PASS: completion_no_callback\n");
}

static void
test_fade_out(void)
{
	reset_globals();
	struct wm_view view = make_test_view();

	/* 255 -> 0 over 80ms (5 frames) */
	struct wm_animation *anim = wm_animation_start(
		&view, WM_ANIM_FADE_OUT, 255, 0, 80,
		test_done_callback, NULL);
	assert(anim != NULL);
	assert(view._test_opacity == 255);

	/* t=0.2: alpha = 255 + (0-255)*0.2 = 204 */
	pump_timer(1);
	assert(view._test_opacity == 204);

	/* t=0.4: alpha = 153 */
	pump_timer(1);
	assert(view._test_opacity == 153);

	wm_animation_cancel(anim);
	printf("  PASS: fade_out\n");
}

static void
test_timer_null_view(void)
{
	reset_globals();
	struct wm_view view = make_test_view();

	struct wm_animation *anim = wm_animation_start(
		&view, WM_ANIM_FADE_IN, 0, 255, 160,
		test_done_callback, NULL);
	assert(anim != NULL);

	/* Simulate view destruction */
	anim->view = NULL;

	/* Should free anim without crash */
	pump_timer(1);
	assert(g_done_called == 0);
	printf("  PASS: timer_null_view\n");
}

int
main(void)
{
	printf("test_animation:\n");

	test_start_null_view();
	test_start_null_server();
	test_zero_duration();
	test_negative_duration();
	test_start_sets_initial_state();
	test_interpolation();
	test_completion();
	test_cancel();
	test_cancel_null();
	test_start_cancels_existing();
	test_no_callback();
	test_completion_no_callback();
	test_fade_out();
	test_timer_null_view();

	printf("All animation tests passed.\n");
	return 0;
}
