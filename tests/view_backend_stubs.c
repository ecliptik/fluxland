/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * view_backend_stubs.c - view.c backend accessors for unit tests
 *
 * Tests that exercise one module without view.c link (or #include)
 * this instead. Every view is treated as an xdg-shell view, and each
 * accessor forwards to the wlroots call the real xdg backend makes, so
 * a test's own wlroots stubs keep seeing the same calls as before the
 * accessors existed.
 *
 * Test stand-ins differ, so every accessor is opt-in: define
 * VIEW_STUB_<NAME> before including for each one the module under test
 * uses (e.g. VIEW_STUB_SURFACE for wm_view_surface()).
 */

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef VIEW_STUB_SURFACE
struct wlr_surface *
wm_view_surface(struct wm_view *view)
{
	return view && view->xdg_toplevel && view->xdg_toplevel->base ?
		view->xdg_toplevel->base->surface : NULL;
}
#endif

#ifdef VIEW_STUB_CONTENT_GEOMETRY
void
wm_view_content_geometry(struct wm_view *view, struct wlr_box *geo)
{
	wlr_xdg_surface_get_geometry(view->xdg_toplevel->base, geo);
}
#endif

#ifdef VIEW_STUB_SET_SIZE
void
wm_view_set_size(struct wm_view *view, int width, int height)
{
	wlr_xdg_toplevel_set_size(view->xdg_toplevel, width, height);
}
#endif

#ifdef VIEW_STUB_SET_ACTIVATED
void
wm_view_set_activated(struct wm_view *view, bool activated)
{
	wlr_xdg_toplevel_set_activated(view->xdg_toplevel, activated);
}
#endif

#ifdef VIEW_STUB_SET_MAXIMIZED
void
wm_view_set_maximized_state(struct wm_view *view, bool maximized)
{
	wlr_xdg_toplevel_set_maximized(view->xdg_toplevel, maximized);
}
#endif

#ifdef VIEW_STUB_SET_FULLSCREEN
void
wm_view_set_fullscreen_state(struct wm_view *view, bool fullscreen)
{
	wlr_xdg_toplevel_set_fullscreen(view->xdg_toplevel, fullscreen);
}
#endif

#ifdef VIEW_STUB_SET_TILED
void
wm_view_set_tiled_state(struct wm_view *view, uint32_t edges)
{
	wlr_xdg_toplevel_set_tiled(view->xdg_toplevel, edges);
}
#endif

#ifdef VIEW_STUB_SET_MINIMIZED
void
wm_view_set_minimized_state(struct wm_view *view, bool minimized)
{
	(void)view;
	(void)minimized;
}
#endif

#ifdef VIEW_STUB_CLOSE
void
wm_view_close(struct wm_view *view)
{
	wlr_xdg_toplevel_send_close(view->xdg_toplevel);
}
#endif

#ifdef VIEW_STUB_KILL
void
wm_view_kill(struct wm_view *view)
{
	(void)view;
}
#endif

#ifdef VIEW_STUB_PID
pid_t
wm_view_pid(struct wm_view *view)
{
	(void)view;
	return 0;
}
#endif

#ifdef VIEW_STUB_PARENT
struct wm_view *
wm_view_parent(struct wm_view *view)
{
	(void)view;
	return NULL;
}
#endif

#ifdef VIEW_STUB_FROM_SURFACE
struct wm_view *
wm_view_from_surface(struct wm_server *server, struct wlr_surface *surface)
{
	if (!surface)
		return NULL;
	struct wm_view *v;
	wl_list_for_each(v, &server->views, link) {
		if (wm_view_surface(v) == surface)
			return v;
	}
	return NULL;
}
#endif

#ifdef VIEW_STUB_MOVE
void
wm_view_move(struct wm_view *view, int x, int y)
{
	view->x = x;
	view->y = y;
	wlr_scene_node_set_position(&view->scene_tree->node, x, y);
}
#endif

#ifdef VIEW_STUB_SET_FRAME_BOX
/* No decorations in these tests: the frame is the client */
void
wm_view_set_frame_box(struct wm_view *view, const struct wlr_box *box)
{
	view->x = box->x;
	view->y = box->y;
	wlr_scene_node_set_position(&view->scene_tree->node, box->x, box->y);
	wm_view_set_size(view, box->width, box->height);
}
#endif

#ifdef VIEW_STUB_TOGGLE_STATES
/* Record the state changes only; their geometry is view_geometry.c's */
void
wm_view_toggle_maximize(struct wm_view *view)
{
	view->maximized = !view->maximized;
}

void
wm_view_toggle_fullscreen(struct wm_view *view)
{
	view->fullscreen = !view->fullscreen;
}

static int g_view_stub_minimize_count;
void
wm_view_minimize(struct wm_view *view)
{
	(void)view;
	g_view_stub_minimize_count++;
}
#endif

#ifdef VIEW_STUB_SET_HEAD
static int g_view_stub_head;
void
wm_view_set_head(struct wm_view *view, int head)
{
	(void)view;
	g_view_stub_head = head;
}
#endif
