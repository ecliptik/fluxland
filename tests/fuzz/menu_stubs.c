/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * menu_stubs.c - Compositor symbols for the menu parser fuzz target
 *
 * Same stand-ins as tests/test_menu.c: the parser links the real menu
 * sources, but nothing that renders or acts on windows runs.
 */

#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>

#include "menu.h"
#include "render.h"
#include "view.h"
#include "workspace.h"
#include "foreign_toplevel.h"

PangoDirection find_base_dir(const char *text)
	{ (void)text; return PANGO_DIRECTION_LTR; }
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
#include "keyboard_actions.h"
bool wm_execute_action(struct wm_server *s, enum wm_action a,
	const char *arg)
	{ (void)s; (void)a; (void)arg; return true; }
bool wm_execute_action_on(struct wm_server *s, enum wm_action a,
	const char *arg, struct wm_view *v)
	{ (void)s; (void)a; (void)arg; (void)v; return true; }
#include "server.h"
void wm_server_reconfigure(struct wm_server *s)
	{ (void)s; }
#include "toolbar.h"
void wm_toolbar_relayout(struct wm_toolbar *toolbar)
	{ (void)toolbar; }
#include "decoration.h"
void wm_decoration_update(struct wm_decoration *deco, struct wm_style *style)
	{ (void)deco; (void)style; }
#include "keybind.h"
enum wm_action wm_action_from_name(const char *name)
	{ (void)name; return WM_ACTION_NOP; }
#include "command.h"
static int stub_command_run_count;
struct wm_command *wm_command_parse(const char *str)
{
	if (!str || !*str)
		return NULL;
	return calloc(1, sizeof(struct wm_command));
}
void wm_command_destroy(struct wm_command *cmd) { free(cmd); }
bool wm_command_execute(struct wm_server *s, struct wm_command *cmd,
	struct wm_view *target)
	{ (void)s; (void)cmd; (void)target; stub_command_run_count++; return true; }
