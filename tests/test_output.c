/*
 * test_output.c - Unit tests for output management (output.c)
 *
 * Includes output.c directly. wlroots is replaced by a small fake
 * output layout (outputs at positions, auto placement, output_at,
 * closest point, change events) so the arrange/place/hotplug logic can
 * be tested with real geometry.
 */

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* --- Block real headers via their include guards --- */
#define WAYLAND_SERVER_CORE_H
#define WAYLAND_SERVER_H
#define WAYLAND_UTIL_H
#define WLR_UTIL_LOG_H
#define WLR_UTIL_BOX_H
#define WLR_BACKEND_WAYLAND_H
#define WLR_TYPES_WLR_OUTPUT_H
#define WLR_TYPES_WLR_OUTPUT_LAYOUT_H
#define WLR_TYPES_WLR_SCENE_H
#define WLR_TYPES_WLR_XCURSOR_MANAGER_H
#define WM_SERVER_H
#define WM_CONFIG_H
#define WM_OUTPUT_H
#define WM_IPC_H
#define WM_STYLE_H
#define WM_DRM_LEASE_H
#define WM_LAYER_SHELL_H
#define WM_TOOLBAR_H
#define WM_VIEW_H
#define WM_WORKSPACE_H
#define WM_OUTPUT_CONFIG_H
#define WM_SLIT_H
#define WM_VIEW_GEOMETRY_H
#define WM_FOREIGN_TOPLEVEL_H
#define WM_EXT_WORKSPACE_H

/* --- Stub wayland types --- */

struct wl_list {
	struct wl_list *prev;
	struct wl_list *next;
};

static inline void
wl_list_init(struct wl_list *list)
{
	list->prev = list;
	list->next = list;
}

static inline void
wl_list_insert(struct wl_list *list, struct wl_list *elm)
{
	elm->prev = list;
	elm->next = list->next;
	list->next = elm;
	elm->next->prev = elm;
}

static inline void
wl_list_remove(struct wl_list *elm)
{
	elm->prev->next = elm->next;
	elm->next->prev = elm->prev;
	elm->prev = NULL;
	elm->next = NULL;
}

static inline int
wl_list_empty(const struct wl_list *list)
{
	return list->next == list;
}

#include <stddef.h>

#define wl_container_of(ptr, sample, member) \
	(__typeof__(sample))((char *)(ptr) - \
	offsetof(__typeof__(*sample), member))

#define wl_list_for_each(pos, head, member) \
	for (pos = wl_container_of((head)->next, pos, member); \
	     &pos->member != (head); \
	     pos = wl_container_of(pos->member.next, pos, member))

#define wl_list_for_each_safe(pos, tmp, head, member) \
	for (pos = wl_container_of((head)->next, pos, member), \
	     tmp = wl_container_of(pos->member.next, tmp, member); \
	     &pos->member != (head); \
	     pos = tmp, \
	     tmp = wl_container_of(pos->member.next, tmp, member))

struct wl_signal {
	struct wl_list listener_list;
};

struct wl_listener {
	struct wl_list link;
	void (*notify)(struct wl_listener *, void *);
};

static inline void
wl_signal_init(struct wl_signal *signal)
{
	wl_list_init(&signal->listener_list);
}

static inline void
wl_signal_add(struct wl_signal *signal, struct wl_listener *listener)
{
	wl_list_insert(signal->listener_list.prev, &listener->link);
}

static inline void
wl_signal_emit(struct wl_signal *signal, void *data)
{
	struct wl_listener *l, *tmp;
	wl_list_for_each_safe(l, tmp, &signal->listener_list, link)
		l->notify(l, data);
}

struct wl_display { int dummy; };
struct wl_event_source { int dummy; };
struct wl_event_loop { int dummy; };

/* --- wlr_log no-op --- */
#define wlr_log(verb, fmt, ...) ((void)0)
#define WLR_ERROR 1
#define WLR_INFO  3
#define WLR_DEBUG 7

/* --- Stub wlr types --- */

struct wlr_box {
	int x, y, width, height;
};

static inline bool
wlr_box_empty(const struct wlr_box *b)
{
	return !b || b->width <= 0 || b->height <= 0;
}

static inline bool
wlr_box_equal(const struct wlr_box *a, const struct wlr_box *b)
{
	return a->x == b->x && a->y == b->y && a->width == b->width &&
		a->height == b->height;
}

struct wlr_output {
	int width, height;
	char *name;
	bool enabled;
	struct {
		struct wl_signal frame;
		struct wl_signal request_state;
		struct wl_signal destroy;
	} events;
};

struct wlr_output_mode { int dummy; };

struct wlr_output_state {
	bool has_enabled, enabled;
	int committed;
};

struct wlr_output_event_request_state {
	struct wlr_output_state *state;
};

static void wlr_output_state_init(struct wlr_output_state *s)
{ memset(s, 0, sizeof(*s)); }

static void wlr_output_state_set_enabled(struct wlr_output_state *s,
	bool enabled)
{ s->has_enabled = true; s->enabled = enabled; s->committed |= 1; }

static void wlr_output_state_set_mode(struct wlr_output_state *s,
	struct wlr_output_mode *m)
{ (void)m; s->committed |= 2; }

static void wlr_output_state_finish(struct wlr_output_state *s)
{ (void)s; }

/* Commits fail while g_commit_failures > 0 */
static int g_commit_failures;
static int g_commit_count;
static bool wlr_output_commit_state(struct wlr_output *o,
	const struct wlr_output_state *s)
{
	g_commit_count++;
	if (g_commit_failures > 0) {
		g_commit_failures--;
		return false;
	}
	if (s->has_enabled)
		o->enabled = s->enabled;
	return true;
}

/* A fake output layout: up to 4 outputs at positions */
struct wlr_output_layout_output {
	struct wlr_output *output;
	int x, y;
	bool used;
};

struct wlr_output_layout {
	struct wlr_output_layout_output outs[4];
	struct {
		struct wl_signal change;
	} events;
};

static struct wlr_output_layout_output *
wlr_output_layout_get(struct wlr_output_layout *l, struct wlr_output *o)
{
	for (int i = 0; i < 4; i++) {
		if (l->outs[i].used && l->outs[i].output == o)
			return &l->outs[i];
	}
	return NULL;
}

static struct wlr_output_layout_output *
layout_add(struct wlr_output_layout *l, struct wlr_output *o, int x, int y)
{
	struct wlr_output_layout_output *lo = wlr_output_layout_get(l, o);
	for (int i = 0; !lo && i < 4; i++) {
		if (!l->outs[i].used)
			lo = &l->outs[i];
	}
	assert(lo);
	lo->used = true;
	lo->output = o;
	lo->x = x;
	lo->y = y;
	wl_signal_emit(&l->events.change, l);
	return lo;
}

static int g_layout_add_count;
static struct wlr_output_layout_output *
wlr_output_layout_add(struct wlr_output_layout *l, struct wlr_output *o,
	int x, int y)
{
	g_layout_add_count++;
	return layout_add(l, o, x, y);
}

/* Auto placement: right of everything else, like wlroots */
static struct wlr_output_layout_output *
wlr_output_layout_add_auto(struct wlr_output_layout *l, struct wlr_output *o)
{
	int x = 0;
	for (int i = 0; i < 4; i++) {
		if (l->outs[i].used && l->outs[i].output != o &&
		    l->outs[i].x + l->outs[i].output->width > x)
			x = l->outs[i].x + l->outs[i].output->width;
	}
	return layout_add(l, o, x, 0);
}

static void
wlr_output_layout_remove(struct wlr_output_layout *l, struct wlr_output *o)
{
	struct wlr_output_layout_output *lo = wlr_output_layout_get(l, o);
	if (!lo)
		return;
	lo->used = false;
	wl_signal_emit(&l->events.change, l);
}

static void
wlr_output_layout_get_box(struct wlr_output_layout *l, struct wlr_output *o,
	struct wlr_box *box)
{
	struct wlr_output_layout_output *lo = wlr_output_layout_get(l, o);
	if (!lo) {
		*box = (struct wlr_box){0};
		return;
	}
	*box = (struct wlr_box){lo->x, lo->y, o->width, o->height};
}

static struct wlr_output *
wlr_output_layout_output_at(struct wlr_output_layout *l, double lx, double ly)
{
	for (int i = 0; i < 4; i++) {
		struct wlr_output_layout_output *lo = &l->outs[i];
		if (lo->used && lx >= lo->x && lx < lo->x + lo->output->width &&
		    ly >= lo->y && ly < lo->y + lo->output->height)
			return lo->output;
	}
	return NULL;
}

static void
wlr_output_layout_closest_point(struct wlr_output_layout *l,
	struct wlr_output *reference, double lx, double ly,
	double *dest_lx, double *dest_ly)
{
	(void)reference;
	double best = -1;
	*dest_lx = lx;
	*dest_ly = ly;
	for (int i = 0; i < 4; i++) {
		struct wlr_output_layout_output *lo = &l->outs[i];
		if (!lo->used)
			continue;
		double x = lx < lo->x ? lo->x : lx >= lo->x + lo->output->width ?
			lo->x + lo->output->width - 1 : lx;
		double y = ly < lo->y ? lo->y : ly >= lo->y + lo->output->height ?
			lo->y + lo->output->height - 1 : ly;
		double d = (x - lx) * (x - lx) + (y - ly) * (y - ly);
		if (best < 0 || d < best) {
			best = d;
			*dest_lx = x;
			*dest_ly = y;
		}
	}
}

struct wlr_scene_node {
	int enabled;
	int x, y;
};

struct wlr_scene_tree {
	struct wlr_scene_node node;
};

struct wlr_scene { int dummy; };
struct wlr_scene_output { int dummy; };
struct wlr_scene_output_layout { int dummy; };

struct wlr_backend {
	struct {
		struct wl_signal new_output;
	} events;
};

struct wlr_renderer { int dummy; };
struct wlr_allocator { int dummy; };

struct wlr_cursor {
	double x, y;
};

struct wlr_xcursor_manager { int dummy; };

/* --- Stub wm types --- */

struct wm_workspace {
	struct wl_list link;
};

struct wm_output {
	struct wl_list link;
	struct wm_server *server;
	struct wlr_output *wlr_output;
	struct wlr_scene_output *scene_output;
	struct wlr_box full_area;
	struct wlr_box usable_area;
	struct wm_workspace *active_workspace;
	bool lock_blanked;
	struct wl_listener frame;
	struct wl_listener request_state;
	struct wl_listener destroy;
};

struct wm_view {
	struct wl_list link;
	struct wm_server *server;
	int x, y;
	int width, height;
	bool mapped, docked;
	int refits, keep_on_screen;
};

struct wm_output_rule {
	char *name;
	int enabled;
	bool has_position;
	int x, y;
};

#define WM_STRUT_HEADS 8
struct wm_config {
	int struts[4];
	int head_struts[WM_STRUT_HEADS][4];
};
int wm_output_head_number(struct wm_output *output);
struct wm_toolbar { int dummy; };
struct wm_slit { int dummy; };

/* IPC stub types */
enum wm_ipc_event {
	WM_IPC_EVENT_OUTPUT_ADD    = (1 << 5),
	WM_IPC_EVENT_OUTPUT_REMOVE = (1 << 6),
};

struct ipc_event_throttle {
	struct timespec last_sent;
};

struct wm_ipc_client {
	struct wl_list link;
};

struct wm_ipc_server {
	struct wl_list clients;
	int client_count;
	struct ipc_event_throttle event_throttle[32];
};

/* Minimal wm_server definition with fields used by output.c */
struct wm_server {
	struct wl_display *wl_display;
	struct wl_event_loop *wl_event_loop;
	struct wlr_backend *backend;
	struct wlr_renderer *renderer;
	struct wlr_allocator *allocator;
	struct wlr_scene *scene;
	struct wlr_scene_output_layout *scene_layout;
	struct wl_list outputs;
	struct wl_listener new_output;
	struct wl_listener output_layout_change;
	struct wlr_output_layout *output_layout;
	struct wlr_cursor *cursor;
	struct wm_view *focused_view;
	struct wm_workspace *current_workspace;
	struct wm_ipc_server ipc;
	struct wm_toolbar *toolbar;
	struct wm_slit *slit;
	struct wm_config *config;
	struct wl_list views;
};

/* --- Stub wlroots functions used by output.c --- */

static struct wlr_output_layout g_stub_output_layout;

static struct wlr_output_layout *
wlr_output_layout_create(struct wl_display *display)
{
	(void)display;
	memset(&g_stub_output_layout, 0, sizeof(g_stub_output_layout));
	wl_signal_init(&g_stub_output_layout.events.change);
	return &g_stub_output_layout;
}

static struct wlr_scene_output_layout g_stub_scene_layout;

static struct wlr_scene_output_layout *
wlr_scene_attach_output_layout(struct wlr_scene *scene,
	struct wlr_output_layout *layout)
{
	(void)scene; (void)layout;
	return &g_stub_scene_layout;
}

static void
wlr_output_init_render(struct wlr_output *o, struct wlr_allocator *a,
	struct wlr_renderer *r)
{ (void)o; (void)a; (void)r; }

static struct wlr_output_mode g_preferred_mode;
static struct wlr_output_mode *
wlr_output_preferred_mode(struct wlr_output *o)
{ (void)o; return &g_preferred_mode; }

static void
wlr_output_effective_resolution(struct wlr_output *o, int *w, int *h)
{ *w = o->width; *h = o->height; }

static struct wlr_scene_output g_stub_scene_output;

static struct wlr_scene_output *
wlr_scene_output_create(struct wlr_scene *s, struct wlr_output *o)
{ (void)s; (void)o; return &g_stub_scene_output; }

static int g_scene_layout_add_count;
static void
wlr_scene_output_layout_add_output(struct wlr_scene_output_layout *l,
	struct wlr_output_layout_output *lo,
	struct wlr_scene_output *so)
{ (void)l; (void)lo; (void)so; g_scene_layout_add_count++; }

static void wlr_scene_output_commit(struct wlr_scene_output *so,
	void *opt)
{ (void)so; (void)opt; }

static void wlr_scene_output_send_frame_done(struct wlr_scene_output *so,
	struct timespec *ts)
{ (void)so; (void)ts; }

/* --- Stub functions from other wm modules --- */

static void
wm_ipc_broadcast_event(struct wm_ipc_server *ipc,
	enum wm_ipc_event event, const char *json_event)
{ (void)ipc; (void)event; (void)json_event; }

static void
wm_drm_lease_offer_output(struct wm_server *s, struct wlr_output *o)
{ (void)s; (void)o; }

void
wm_xwayland_sync_positions(struct wm_server *s)
{ (void)s; }

/* Space taken by a layer-shell panel, the toolbar and the slit */
static struct wm_output *g_panel_output, *g_toolbar_output, *g_slit_output;

static void
wm_layer_shell_configure(struct wm_output *o, const struct wlr_box *full,
	struct wlr_box *usable)
{
	(void)full;
	if (o == g_panel_output) {
		usable->y += 30;
		usable->height -= 30;
	}
}

void wm_foreign_toplevel_sync_outputs(struct wm_server *s) { (void)s; }
static int g_ext_ws_enter, g_ext_ws_leave;
static void wm_ext_workspace_output_enter(struct wm_server *s, struct wlr_output *o) { (void)s; (void)o; g_ext_ws_enter++; }
static void wm_ext_workspace_output_leave(struct wm_server *s, struct wlr_output *o) { (void)s; (void)o; g_ext_ws_leave++; }
static struct wlr_output *g_foreign_forgot;
void
wm_foreign_toplevel_output_destroyed(struct wm_server *s, struct wlr_output *o)
{ (void)s; g_foreign_forgot = o; }

static struct wlr_output *g_layer_closed_for;
static void
wm_layer_shell_output_destroyed(struct wm_server *s, struct wlr_output *o)
{ (void)s; g_layer_closed_for = o; }

static void
wm_toolbar_reserve(struct wm_toolbar *t, struct wm_output *o,
	struct wlr_box *usable)
{
	if (t && o == g_toolbar_output)
		usable->height -= 24;
}

static void
wm_slit_reserve(struct wm_slit *sl, struct wm_output *o,
	struct wlr_box *usable)
{
	if (sl && o == g_slit_output)
		usable->width -= 64;
}

static int g_toolbar_relayout_count, g_slit_reconfigure_count;
static void
wm_toolbar_relayout(struct wm_toolbar *t)
{ (void)t; g_toolbar_relayout_count++; }
static void
wm_slit_reconfigure(struct wm_slit *sl)
{ (void)sl; g_slit_reconfigure_count++; }

/* config.c / output_config.c */
static struct wm_output_rule g_rules[2];
static int g_rule_count;
static const struct wm_output_rule *
config_find_output_rule(const struct wm_config *c, const char *name)
{
	(void)c;
	for (int i = 0; i < g_rule_count; i++) {
		if (name && strcmp(g_rules[i].name, name) == 0)
			return &g_rules[i];
	}
	return NULL;
}

static int g_config_build_count;
static bool
wm_output_config_build(struct wlr_output *o, const struct wm_output_rule *r,
	struct wlr_output_state *state)
{
	(void)o;
	g_config_build_count++;
	if (r && r->enabled == 0)
		wlr_output_state_set_enabled(state, false);
	return r != NULL || state->committed;
}

/* view.c / view_geometry.c */
static void
wm_view_get_geometry(struct wm_view *view, struct wlr_box *box)
{
	*box = (struct wlr_box){view->x, view->y, view->width, view->height};
}
static void
wm_view_refit(struct wm_view *view)
{ view->refits++; }
static void
wm_view_keep_on_screen(struct wm_view *view)
{ view->keep_on_screen++; }

/* --- Stubs for util.h functions --- */

void wm_spawn_command(const char *cmd) { (void)cmd; }

void wm_json_escape(char *dst, size_t dst_size, const char *src)
{
	if (!src) {
		if (dst_size > 0) dst[0] = '\0';
		return;
	}
	size_t j = 0;
	for (size_t i = 0; src[i] && j + 6 < dst_size; i++) {
		unsigned char c = (unsigned char)src[i];
		if (c == '"' || c == '\\') {
			dst[j++] = '\\';
			dst[j++] = c;
		} else if (c < 0x20) {
			continue;
		} else {
			dst[j++] = c;
		}
	}
	dst[j] = '\0';
}

static int g_lock_frame_count;
static int g_lock_changed_count;
void
wm_session_lock_output_frame(struct wm_server *s, struct wm_output *o)
{
	(void)s; (void)o;
	g_lock_frame_count++;
}
void
wm_session_lock_output_changed(struct wm_server *s)
{
	(void)s;
	g_lock_changed_count++;
}

/* output.h API (the header is blocked) */
struct wm_output *wm_output_from_wlr(struct wm_server *server,
	struct wlr_output *wlr_output);
struct wm_output *wm_server_get_focused_output(struct wm_server *server);
void wm_output_layout_box(struct wm_output *output, struct wlr_box *box);
void wm_output_arrange(struct wm_output *output);
void wm_output_arrange_all(struct wm_server *server);
void wm_output_place(struct wm_output *output, bool has_position, int x, int y);
void wm_output_apply_config(struct wm_server *server);
struct wm_output *wm_output_by_head(struct wm_server *server, int head);
struct wm_output *wm_output_for_view(struct wm_view *view);
void wm_output_init(struct wm_server *server);
void wm_output_finish(struct wm_server *server);

/* --- Include output.c source directly --- */

#include "output.c"

/* --- Fixture: a server with fake backend and layout --- */

static struct wm_server srv;
static struct wlr_backend backend;
static struct wlr_cursor cursor;
static struct wm_config config;
static struct wm_toolbar toolbar;
static struct wm_slit slit;

static void
reset_globals(void)
{
	g_commit_failures = 0;
	g_commit_count = 0;
	g_layout_add_count = 0;
	g_scene_layout_add_count = 0;
	g_panel_output = g_toolbar_output = g_slit_output = NULL;
	g_layer_closed_for = NULL;
	g_toolbar_relayout_count = g_slit_reconfigure_count = 0;
	g_rule_count = 0;
	g_config_build_count = 0;
	g_lock_frame_count = g_lock_changed_count = 0;
}

static void
setup(void)
{
	reset_globals();
	memset(&srv, 0, sizeof(srv));
	memset(&backend, 0, sizeof(backend));
	memset(&cursor, 0, sizeof(cursor));
	wl_signal_init(&backend.events.new_output);
	srv.backend = &backend;
	srv.cursor = &cursor;
	srv.config = &config;
	srv.toolbar = &toolbar;
	srv.slit = &slit;
	wl_list_init(&srv.views);
	wm_output_init(&srv);
}

static void
init_wlr_output(struct wlr_output *o, char *name, int w, int h)
{
	memset(o, 0, sizeof(*o));
	o->name = name;
	o->width = w;
	o->height = h;
	wl_signal_init(&o->events.frame);
	wl_signal_init(&o->events.request_state);
	wl_signal_init(&o->events.destroy);
}

/* Plug in a monitor */
static struct wm_output *
plug(struct wlr_output *o)
{
	wl_signal_emit(&backend.events.new_output, o);
	return wm_output_from_wlr(&srv, o);
}

static void
unplug(struct wlr_output *o)
{
	wl_signal_emit(&o->events.destroy, o);
}

static void
add_view(struct wm_view *v, int x, int y, int w, int h)
{
	memset(v, 0, sizeof(*v));
	v->server = &srv;
	v->x = x;
	v->y = y;
	v->width = w;
	v->height = h;
	v->mapped = true;
	wl_list_insert(srv.views.prev, &v->link);
}

/* --- Tests --- */

static void
test_hotplug_order_and_auto_layout(void)
{
	setup();
	struct wlr_output a, b;
	init_wlr_output(&a, "eDP-1", 1920, 1080);
	init_wlr_output(&b, "HDMI-A-1", 1280, 1024);

	struct wm_output *oa = plug(&a);
	struct wm_output *ob = plug(&b);
	assert(oa && ob && a.enabled && b.enabled);

	/* Connection order: head 1 is the first output plugged in */
	assert(wm_output_by_head(&srv, 1) == oa);
	assert(wm_output_by_head(&srv, 2) == ob);
	assert(wm_output_by_head(&srv, 0) == oa);
	assert(wm_output_by_head(&srv, 9) == oa);

	/* Auto layout, and the scene follows each new layout entry */
	assert(oa->full_area.x == 0 && oa->full_area.width == 1920);
	assert(ob->full_area.x == 1920 && ob->full_area.height == 1024);
	assert(g_scene_layout_add_count == 2);

	/* Toolbar and slit move whenever the layout changes */
	assert(g_toolbar_relayout_count >= 2 && g_slit_reconfigure_count >= 2);

	unplug(&b);
	unplug(&a);
	assert(wl_list_empty(&srv.outputs));
	assert(wm_output_by_head(&srv, 1) == NULL);
	printf("  PASS: hotplug_order_and_auto_layout\n");
}

static void
test_arrange_usable_area(void)
{
	setup();
	struct wlr_output a, b;
	init_wlr_output(&a, "eDP-1", 1920, 1080);
	init_wlr_output(&b, "DP-1", 2560, 1440);
	struct wm_output *oa = plug(&a);
	struct wm_output *ob = plug(&b);

	struct wm_view on_a, on_b;
	add_view(&on_a, 100, 100, 800, 600);
	add_view(&on_b, 2500, 100, 800, 600);

	/* Panel and toolbar on the second head, slit on the first */
	g_panel_output = ob;
	g_toolbar_output = ob;
	g_slit_output = oa;
	on_a.refits = on_b.refits = 0;
	wm_output_arrange_all(&srv);

	/* Layout coordinates, minus each reservation */
	assert(ob->usable_area.x == 1920 && ob->usable_area.y == 30);
	assert(ob->usable_area.width == 2560);
	assert(ob->usable_area.height == 1440 - 30 - 24);
	assert(oa->usable_area.x == 0 && oa->usable_area.width == 1920 - 64);
	assert(oa->usable_area.height == 1080);

	/* Windows on a changed output are re-fitted, once */
	assert(on_a.refits == 1 && on_b.refits == 1);

	/* Arranging again changes nothing and re-fits nothing */
	wm_output_arrange_all(&srv);
	assert(ob->usable_area.height == 1440 - 54);
	assert(on_a.refits == 1 && on_b.refits == 1);

	/* Only the output whose area changed */
	g_slit_output = NULL;
	wm_output_arrange_all(&srv);
	assert(oa->usable_area.width == 1920);
	assert(on_a.refits == 2 && on_b.refits == 1);

	unplug(&b);
	unplug(&a);
	printf("  PASS: arrange_usable_area\n");
}

/* struts for every head, struts.N for one; after the reservations */
static void
test_struts(void)
{
	setup();
	memset(&config, 0, sizeof(config));
	struct wlr_output a, b;
	init_wlr_output(&a, "eDP-1", 1920, 1080);
	init_wlr_output(&b, "DP-1", 2560, 1440);
	struct wm_output *oa = plug(&a);
	struct wm_output *ob = plug(&b);
	g_toolbar_output = ob;

	int all[4] = { 10, 20, 30, 40 };
	for (int i = 0; i < WM_STRUT_HEADS; i++)
		memcpy(config.head_struts[i], all, sizeof(all));
	memcpy(config.struts, all, sizeof(all));
	config.head_struts[1][2] = 0;	/* struts.2: no top */
	wm_output_arrange_all(&srv);

	assert(oa->usable_area.x == 10 && oa->usable_area.y == 30);
	assert(oa->usable_area.width == 1920 - 30);
	assert(oa->usable_area.height == 1080 - 70);
	/* Head 2: the toolbar's 24 at the bottom, then the struts */
	assert(ob->usable_area.x == 1920 + 10 && ob->usable_area.y == 0);
	assert(ob->usable_area.height == 1440 - 24 - 40);

	/* Struts larger than the head are ignored */
	config.head_struts[0][0] = 5000;
	wm_output_arrange_all(&srv);
	assert(oa->usable_area.x == 0 && oa->usable_area.width == 1920);

	memset(&config, 0, sizeof(config));
	g_toolbar_output = NULL;
	unplug(&b);
	unplug(&a);
	printf("  PASS: struts\n");
}

static void
test_place_and_disable(void)
{
	setup();
	struct wlr_output a, b;
	init_wlr_output(&a, "eDP-1", 1920, 1080);
	init_wlr_output(&b, "HDMI-A-1", 1280, 1024);
	struct wm_output *oa = plug(&a);
	struct wm_output *ob = plug(&b);

	/* Explicit position: moves, scene association kept */
	int scene_adds = g_scene_layout_add_count;
	wm_output_place(ob, true, -1280, 56);
	assert(ob->full_area.x == -1280 && ob->full_area.y == 56);
	assert(g_scene_layout_add_count == scene_adds);

	/* No position, already placed: stays put */
	wm_output_place(ob, false, 0, 0);
	assert(ob->full_area.x == -1280);

	/* Disabled: out of the layout, its windows come back */
	struct wm_view stranded;
	add_view(&stranded, -1000, 200, 640, 480);
	b.enabled = false;
	wm_output_place(ob, false, 0, 0);
	assert(wlr_output_layout_get(srv.output_layout, &b) == NULL);
	assert(wlr_box_empty(&ob->usable_area));
	assert(stranded.keep_on_screen >= 1);
	assert(wm_output_for_view(&stranded) == oa);

	/* Re-enabled: back in, with a new scene association */
	b.enabled = true;
	wm_output_place(ob, false, 0, 0);
	assert(ob->full_area.x == 1920);
	assert(g_scene_layout_add_count == scene_adds + 1);

	unplug(&b);
	unplug(&a);
	printf("  PASS: place_and_disable\n");
}

static void
test_output_for_view(void)
{
	setup();
	struct wlr_output a, b;
	init_wlr_output(&a, "eDP-1", 1920, 1080);
	init_wlr_output(&b, "DP-1", 1920, 1080);
	struct wm_output *oa = plug(&a);
	struct wm_output *ob = plug(&b);

	struct wm_view v;
	add_view(&v, 1500, 100, 800, 600);   /* center x = 1900: head 1 */
	assert(wm_output_for_view(&v) == oa);
	v.x = 1600;                          /* center x = 2000: head 2 */
	assert(wm_output_for_view(&v) == ob);
	v.x = 5000;                          /* off to the right */
	assert(wm_output_for_view(&v) == ob);
	v.x = -3000;                         /* off to the left */
	assert(wm_output_for_view(&v) == oa);

	/* Focused output: the focused window's own output */
	v.x = 2500;
	srv.focused_view = &v;
	assert(wm_server_get_focused_output(&srv) == ob);
	srv.focused_view = NULL;
	cursor.x = 10;
	assert(wm_server_get_focused_output(&srv) == oa);

	unplug(&b);
	unplug(&a);
	printf("  PASS: output_for_view\n");
}

static void
test_unplug_moves_everything(void)
{
	setup();
	struct wlr_output a, b;
	init_wlr_output(&a, "eDP-1", 1920, 1080);
	init_wlr_output(&b, "HDMI-A-1", 1920, 1080);
	plug(&a);
	plug(&b);

	struct wm_view v, docked;
	add_view(&v, 2500, 300, 800, 600);
	add_view(&docked, 2600, 0, 64, 64);
	docked.docked = true;
	int relayouts = g_toolbar_relayout_count;

	unplug(&b);
	/* Its panels are closed, it leaves the layout, windows come back */
	assert(g_layer_closed_for == &b);
	assert(g_foreign_forgot == &b);
	assert(wlr_output_layout_get(srv.output_layout, &b) == NULL);
	assert(v.keep_on_screen >= 1 && v.refits >= 1);
	assert(docked.keep_on_screen == 0);
	assert(g_toolbar_relayout_count > relayouts);
	assert(g_lock_changed_count == 1);
	assert(srv.outputs.next->next == &srv.outputs);

	unplug(&a);
	printf("  PASS: unplug_moves_everything\n");
}

static void
test_rules_at_hotplug_and_reconfigure(void)
{
	setup();
	g_rules[0] = (struct wm_output_rule){
		.name = "HDMI-A-1", .enabled = -1, .has_position = true,
		.x = -1280, .y = 0,
	};
	g_rules[1] = (struct wm_output_rule){
		.name = "DP-9", .enabled = 0,
	};
	g_rule_count = 2;

	struct wlr_output a, b, c;
	init_wlr_output(&a, "eDP-1", 1920, 1080);
	init_wlr_output(&b, "HDMI-A-1", 1280, 1024);
	init_wlr_output(&c, "DP-9", 1920, 1080);
	plug(&a);
	struct wm_output *ob = plug(&b);
	struct wm_output *oc = plug(&c);

	/* Positioned from init; DP-9 disabled and not in the layout */
	assert(ob->full_area.x == -1280);
	assert(!c.enabled && oc != NULL);
	assert(wlr_output_layout_get(srv.output_layout, &c) == NULL);

	/* A rule that fails to commit falls back to the preferred mode */
	g_rules[1].enabled = -1;
	struct wlr_output d;
	init_wlr_output(&d, "DP-9", 800, 600);
	unplug(&c);
	g_commit_failures = 1;
	g_commit_count = 0;
	struct wm_output *od = plug(&d);
	assert(g_commit_count == 2 && d.enabled);
	assert(od->full_area.width == 800);

	/* Reconfigure re-applies the rules: move HDMI-A-1 */
	g_rules[0].x = 1920 + 800;
	wm_output_apply_config(&srv);
	assert(ob->full_area.x == 2720);

	unplug(&d);
	unplug(&b);
	unplug(&a);
	printf("  PASS: rules_at_hotplug_and_reconfigure\n");
}

static void
test_output_from_wlr(void)
{
	setup();
	struct wlr_output a, unknown;
	init_wlr_output(&a, "eDP-1", 1920, 1080);
	init_wlr_output(&unknown, "X", 1, 1);
	assert(wm_output_from_wlr(&srv, &a) == NULL);
	struct wm_output *oa = plug(&a);
	assert(wm_output_from_wlr(&srv, &a) == oa);
	assert(wm_output_from_wlr(&srv, &unknown) == NULL);
	unplug(&a);
	assert(wm_server_get_focused_output(&srv) == NULL);
	printf("  PASS: output_from_wlr\n");
}

int
main(void)
{
	printf("test_output:\n");
	test_output_from_wlr();
	test_hotplug_order_and_auto_layout();
	test_arrange_usable_area();
	test_struts();
	test_place_and_disable();
	test_output_for_view();
	test_unplug_moves_everything();
	test_rules_at_hotplug_and_reconfigure();
	printf("All output tests passed.\n");
	return 0;
}
