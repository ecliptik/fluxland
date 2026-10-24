/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * test_command.c - Unit tests for command.c and client_pattern.c
 *
 * Links the real command language and client pattern code. The action
 * dispatcher (keyboard_actions.c) is replaced by a recorder so tests can
 * check which actions ran, with which argument, on which window.
 */

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>

#include "client_pattern.h"
#include "command.h"
#include "decoration.h"
#include "server.h"
#include "view.h"
#include "workspace.h"

/* --- Recorded dispatcher (stands in for keyboard_actions.c) --- */

#define MAX_CALLS 32

static struct {
	enum wm_action action;
	char argument[128];
	struct wm_view *target;
} calls[MAX_CALLS];
static int call_count;

/* Optional side effect, e.g. closing a window during ForEach */
static void (*on_action)(struct wm_view *target);

bool
wm_execute_action_on(struct wm_server *server, enum wm_action action,
	const char *argument, struct wm_view *target)
{
	(void)server;
	if (call_count < MAX_CALLS) {
		calls[call_count].action = action;
		snprintf(calls[call_count].argument,
			sizeof(calls[call_count].argument), "%s",
			argument ? argument : "");
		calls[call_count].target = target;
	}
	call_count++;
	if (on_action)
		on_action(target);
	return true;
}

/* --- Other symbols client_pattern.c needs --- */

void
wm_view_get_geometry(struct wm_view *view, struct wlr_box *box)
{
	(void)view;
	box->x = 0;
	box->y = 0;
	box->width = 100;
	box->height = 100;
}

struct wm_workspace *
wm_workspace_get_active(struct wm_server *server)
{
	return server->current_workspace;
}

struct wm_view *
wm_view_parent(struct wm_view *view)
{
	(void)view;
	return NULL;
}

/* --- Fixture --- */

#define NVIEWS 4

static struct wl_display *display;
static struct wm_server server;
static struct wm_workspace ws0, ws1;
static struct wm_view views[NVIEWS];
static struct wlr_scene_tree trees[NVIEWS];
static struct wm_decoration decos[NVIEWS];

static void
reset(void)
{
	call_count = 0;
	on_action = NULL;
	memset(calls, 0, sizeof(calls));

	if (!display) {
		display = wl_display_create();
		assert(display);
	}
	struct wl_event_loop *loop = wl_display_get_event_loop(display);
	struct wlr_output_layout *layout = server.output_layout;

	memset(&server, 0, sizeof(server));
	server.wl_display = display;
	server.wl_event_loop = loop;
	server.output_layout = layout ? layout :
		wlr_output_layout_create(display);
	wl_list_init(&server.views);
	wl_list_init(&server.outputs);
	wl_list_init(&server.workspaces);
	wl_list_init(&server.pending_commands);

	memset(&ws0, 0, sizeof(ws0));
	memset(&ws1, 0, sizeof(ws1));
	ws0.index = 0;
	ws0.name = "one";
	ws1.index = 1;
	ws1.name = "two";
	wl_list_insert(server.workspaces.prev, &ws0.link);
	wl_list_insert(server.workspaces.prev, &ws1.link);
	server.current_workspace = &ws0;
}

/* Add view i (appended, so earlier views are higher in the stack) */
static struct wm_view *
add_view(int i, const char *app_id, const char *title,
	struct wm_workspace *ws)
{
	struct wm_view *v = &views[i];
	memset(v, 0, sizeof(*v));
	memset(&trees[i], 0, sizeof(trees[i]));
	memset(&decos[i], 0, sizeof(decos[i]));
	trees[i].node.enabled = true;
	v->server = &server;
	v->scene_tree = &trees[i];
	v->decoration = &decos[i];
	v->id = (uint32_t)i + 1;
	v->app_id = (char *)app_id;
	v->title = (char *)title;
	v->workspace = ws;
	v->layer = WM_LAYER_NORMAL;
	v->mapped = true;
	wl_list_insert(server.views.prev, &v->link);
	return v;
}

static bool
matches(const char *pattern, struct wm_view *v)
{
	struct wm_client_pattern *p = wm_client_pattern_parse(pattern, NULL);
	assert(p);
	bool ok = wm_client_pattern_match(&server, p, v);
	wm_client_pattern_destroy(p);
	return ok;
}

static bool
cond(const char *text, struct wm_view *v)
{
	struct wm_condition *c = wm_condition_parse(text);
	assert(c);
	bool ok = wm_condition_evaluate(&server, c, v);
	wm_condition_destroy(c);
	return ok;
}

/* --- Client patterns --- */

static void
test_pattern_bare_word_is_anchored_name(void)
{
	reset();
	struct wm_view *x = add_view(0, "xterm", "shell", &ws0);
	struct wm_view *u = add_view(1, "uxterm", "shell", &ws0);
	struct wm_client_pattern *p = wm_client_pattern_parse("(xterm)", NULL);
	assert(p && p->term_count == 1);
	assert(p->terms[0].prop == WM_PROP_NAME);
	assert(wm_client_pattern_match(&server, p, x));
	/* Fluxbox anchors regexes: no substring matches */
	assert(!wm_client_pattern_match(&server, p, u));
	wm_client_pattern_destroy(p);
	printf("  PASS: pattern_bare_word_is_anchored_name\n");
}

static void
test_pattern_terms_and_negation(void)
{
	reset();
	struct wm_view *v = add_view(0, "foot", "nvim main.c", &ws0);
	assert(matches("(title=.*vim.*) (class!=firefox)", v));
	assert(!matches("(title=.*vim.*) (class!=foot)", v));
	assert(matches("(name=foo|foot)", v));
	/* Alternation is grouped before anchoring */
	assert(!matches("(name=foo|bar)", v));
	printf("  PASS: pattern_terms_and_negation\n");
}

static void
test_pattern_current_workspace(void)
{
	reset();
	struct wm_view *a = add_view(0, "a", "A", &ws0);
	struct wm_view *b = add_view(1, "b", "B", &ws1);
	struct wm_view *c = add_view(2, "c", "C", &ws1);
	c->sticky = true;
	assert(matches("(workspace)", a));
	assert(matches("(workspace=[current])", a));
	assert(!matches("(workspace=[current])", b));
	assert(matches("(workspace=[current])", c)); /* sticky */
	assert(matches("(workspace!=[current])", b));
	/* Workspace numbers count from 0, as in Fluxbox */
	assert(matches("(workspace=1)", b));
	assert(matches("(workspacename=two)", b));
	printf("  PASS: pattern_current_workspace\n");
}

static void
test_pattern_current_focused(void)
{
	reset();
	struct wm_view *a = add_view(0, "term", "A", &ws0);
	struct wm_view *b = add_view(1, "term", "B", &ws0);
	struct wm_view *c = add_view(2, "web", "C", &ws0);
	/* No focused window: [current] never matches, even negated */
	assert(!matches("(class=[current])", a));
	assert(!matches("(class!=[current])", c));
	server.focused_view = a;
	assert(matches("(class=[current])", b));
	assert(!matches("(class=[current])", c));
	assert(matches("(class!=[current])", c));
	printf("  PASS: pattern_current_focused\n");
}

/* fluxland's (tag=...): the xdg-toplevel-tag the app gave the window */
static void
test_pattern_tag(void)
{
	reset();
	struct wm_view *v = add_view(0, "org.gnome.Nautilus", "Home", &ws0);
	assert(!matches("(tag=main)", v));
	assert(matches("(tag!=main)", v));
	v->tag = "main";
	assert(matches("(tag=main)", v));
	assert(matches("(tag=ma.*)", v));
	assert(!matches("(tag=mai)", v));  /* anchored */
	v->tag = NULL;
	printf("  PASS: pattern_tag\n");
}

static void
test_pattern_states_and_layer(void)
{
	reset();
	struct wm_view *v = add_view(0, "a", "A", &ws0);
	v->maximized = true;
	v->sticky = true;
	v->layer = WM_LAYER_ABOVE;
	v->urgent = true;
	decos[0].shaded = true;
	assert(matches("(maximized=yes) (stuck=yes) (shaded=yes)", v));
	assert(matches("(maximizedvertical=yes) (maximizedhorizontal=yes)",
		v));
	assert(matches("(minimized=no) (fullscreen=no) (urgent=yes)", v));
	/* Fluxbox name and the fluxland 1.x alias both work */
	assert(matches("(layer=AboveDock)", v));
	assert(matches("(layer=Above)", v));
	assert(!matches("(layer=Normal)", v));
	trees[0].node.enabled = false;
	assert(matches("(minimized=yes)", v));
	printf("  PASS: pattern_states_and_layer\n");
}

static void
test_pattern_glob_fallback_and_errors(void)
{
	reset();
	struct wm_view *v = add_view(0, "a", "my vim session", &ws0);
	/* "*vim*" is not a valid ERE; fluxland 1.x used globs */
	assert(matches("(title=*vim*)", v));
	assert(!wm_client_pattern_parse("", NULL));
	assert(!wm_client_pattern_parse("(", NULL));
	assert(!wm_client_pattern_parse("(a) junk", NULL));
	/*
	 * Regexes that make regcomp() blow up are never compiled as such;
	 * they fall back to a literal glob, which is cheap and safe.
	 */
	static const char *const bombs[] = {
		"(}+++++++++)", "(title=(a{1000}){1000})", "(title=a**)",
	};
	for (size_t i = 0; i < sizeof(bombs) / sizeof(bombs[0]); i++) {
		struct wm_client_pattern *b =
			wm_client_pattern_parse(bombs[i], NULL);
		assert(b);
		wm_client_pattern_destroy(b);
	}
	struct wm_view *lit = add_view(1, "}+++++++++", "t", &ws0);
	assert(matches("(}+++++++++)", lit));
	/* ...but ordinary ones still work, including literal braces */
	struct wm_client_pattern *ok =
		wm_client_pattern_parse("(title=.*vim.*) (name=a{2}|x[+*]+)", NULL);
	assert(ok);
	wm_client_pattern_destroy(ok);
	/* Invalid as a regex and as a glob: fails once, no recursion */
	assert(!wm_client_pattern_parse("([sF)", NULL));
	struct wm_window_list_spec spec;
	assert(!wm_window_list_parse("[sF", &spec));
	const char *rest = NULL;
	struct wm_client_pattern *p =
		wm_client_pattern_parse("(a) {2} junk", &rest);
	assert(p && p->match_limit == 2);
	assert(rest && strcmp(rest, "junk") == 0);
	wm_client_pattern_destroy(p);
	printf("  PASS: pattern_glob_fallback_and_errors\n");
}

static void
test_pattern_unknown_property_is_name_regex(void)
{
	reset();
	struct wm_view *v = add_view(0, "foo=bar", "t", &ws0);
	assert(matches("(foo=bar)", v));
	/* Nested parentheses inside a regex */
	struct wm_view *w = add_view(1, "foobar", "t", &ws0);
	assert(matches("(name=foo(bar))", w));
	printf("  PASS: pattern_unknown_property_is_name_regex\n");
}

static void
test_window_list_parse(void)
{
	struct wm_window_list_spec spec;
	assert(wm_window_list_parse(NULL, &spec));
	assert(!spec.pattern && !spec.groups && !spec.static_order);
	assert(wm_window_list_parse("{static groups} (name=x)", &spec));
	assert(spec.static_order && spec.groups && spec.pattern);
	wm_window_list_finish(&spec);
	assert(wm_window_list_parse("{groups} (workspace=[current])", &spec));
	assert(spec.groups && !spec.static_order);
	assert(spec.pattern->terms[0].kind == WM_PATTERN_CURRENT);
	wm_window_list_finish(&spec);
	/* fluxland 1.x: a bare glob */
	assert(wm_window_list_parse("fire*", &spec));
	assert(spec.pattern && spec.pattern->terms[0].prop == WM_PROP_NAME);
	wm_window_list_finish(&spec);
	assert(!wm_window_list_parse("(", &spec));
	printf("  PASS: window_list_parse\n");
}

/* --- Conditions --- */

static void
test_conditions(void)
{
	reset();
	struct wm_view *a = add_view(0, "term", "A", &ws0);
	struct wm_view *b = add_view(1, "web", "B", &ws0);
	assert(cond("Matches (term)", a));
	assert(!cond("Matches (term)", b));
	assert(!cond("Matches (term)", NULL));
	assert(cond("Not Matches (term)", b));
	assert(cond("Not {Matches (term)}", b));
	assert(cond("And {Matches (term)} {Matches (title=A)}", a));
	assert(!cond("And {Matches (term)} {Matches (title=B)}", a));
	assert(cond("Or {Matches (web)} {Matches (title=A)} {Matches (x)}",
		a));
	/* Xor of three trues is true */
	assert(cond("Xor {Matches (term)} {Matches (title=A)} "
		"{Matches (workspace)}", a));
	assert(cond("Some Matches (web)", NULL));
	assert(!cond("Every Matches (web)", NULL));
	assert(cond("Every Matches (workspace=0)", NULL));
	/* fluxland 1.x shorthand */
	assert(cond("Some (web)", NULL));
	assert(!wm_condition_parse("Bogus (x)"));
	assert(!wm_condition_parse("Matches"));
	assert(!wm_condition_parse("And"));
	printf("  PASS: conditions\n");
}

/* --- Command parsing --- */

static void
test_parse_leaf(void)
{
	struct wm_command *c = wm_command_parse("  SendToWorkspace 3  ");
	assert(c && c->action == WM_ACTION_SEND_TO_WORKSPACE);
	assert(strcmp(c->argument, "3") == 0);
	wm_command_destroy(c);
	c = wm_command_parse("iconify");
	assert(c && c->action == WM_ACTION_MINIMIZE && !c->argument);
	wm_command_destroy(c);
	assert(!wm_command_parse("NoSuchCommand"));
	assert(!wm_command_parse(""));
	assert(!wm_command_create(WM_ACTION_MACRO_CMD, NULL));
	printf("  PASS: parse_leaf\n");
}

static void
test_parse_depth_limit(void)
{
	char buf[4096] = "";
	for (int i = 0; i < 40; i++)
		strcat(buf, "MacroCmd {");
	strcat(buf, "Raise");
	for (int i = 0; i < 40; i++)
		strcat(buf, "}");
	assert(!wm_command_parse(buf));
	printf("  PASS: parse_depth_limit\n");
}

static void
test_contains(void)
{
	struct wm_command *c = wm_command_parse(
		"MacroCmd {Raise} {If {Matches (x)} {Lower} "
		"{ToggleCmd {Focus} {Exec foot}}}");
	assert(c);
	assert(wm_command_contains(c, WM_ACTION_EXEC));
	assert(wm_command_contains(c, WM_ACTION_LOWER));
	assert(!wm_command_contains(c, WM_ACTION_KILL));
	wm_command_destroy(c);
	printf("  PASS: contains\n");
}

/* --- Execution --- */

static void
test_macro_nested_with_target(void)
{
	reset();
	struct wm_view *v = add_view(0, "a", "A", &ws0);
	struct wm_command *c = wm_command_parse(
		"MacroCmd {Raise} {MacroCmd {Focus} {SetAlpha 200}} {Lower}");
	assert(c);
	assert(wm_command_execute(&server, c, v));
	assert(call_count == 4);
	assert(calls[0].action == WM_ACTION_RAISE);
	assert(calls[1].action == WM_ACTION_FOCUS);
	assert(calls[2].action == WM_ACTION_SET_ALPHA);
	assert(strcmp(calls[2].argument, "200") == 0);
	assert(calls[3].action == WM_ACTION_LOWER);
	for (int i = 0; i < 4; i++)
		assert(calls[i].target == v);
	wm_command_destroy(c);
	printf("  PASS: macro_nested_with_target\n");
}

static void
test_toggle(void)
{
	reset();
	struct wm_command *c = wm_command_parse(
		"ToggleCmd {ShadeOn} {MacroCmd {ShadeOff} {Raise}}");
	assert(c);
	wm_command_execute(&server, c, NULL);
	wm_command_execute(&server, c, NULL);
	wm_command_execute(&server, c, NULL);
	assert(call_count == 4);
	assert(calls[0].action == WM_ACTION_SHADE_ON);
	assert(calls[1].action == WM_ACTION_SHADE_OFF);
	assert(calls[2].action == WM_ACTION_RAISE);
	assert(calls[3].action == WM_ACTION_SHADE_ON);
	wm_command_destroy(c);
	printf("  PASS: toggle\n");
}

static void
test_if_uses_target_then_focused(void)
{
	reset();
	struct wm_view *a = add_view(0, "term", "A", &ws0);
	struct wm_view *b = add_view(1, "web", "B", &ws0);
	server.focused_view = b;
	struct wm_command *c = wm_command_parse(
		"Cond {Matches (term)} {Maximize} {Minimize}");
	assert(c);
	wm_command_execute(&server, c, a);    /* clicked window: a */
	wm_command_execute(&server, c, NULL); /* focused window: b */
	assert(call_count == 2);
	assert(calls[0].action == WM_ACTION_MAXIMIZE && calls[0].target == a);
	assert(calls[1].action == WM_ACTION_MINIMIZE && !calls[1].target);
	wm_command_destroy(c);
	printf("  PASS: if_uses_target_then_focused\n");
}

static void
test_foreach_fluxbox_and_legacy_order(void)
{
	reset();
	struct wm_view *a = add_view(0, "term", "A", &ws0);
	add_view(1, "web", "B", &ws0);
	struct wm_view *c = add_view(2, "term", "C", &ws1);

	struct wm_command *cmd = wm_command_parse(
		"ForEach {Minimize} {Matches (term)}");
	assert(cmd);
	wm_command_execute(&server, cmd, NULL);
	assert(call_count == 2);
	assert(calls[0].target == a && calls[1].target == c);
	wm_command_destroy(cmd);

	call_count = 0;
	cmd = wm_command_parse("ForEach {Matches (term)} {Close}");
	assert(cmd && cmd->then_cmd->action == WM_ACTION_CLOSE);
	wm_command_execute(&server, cmd, NULL);
	assert(call_count == 2);
	wm_command_destroy(cmd);

	call_count = 0;
	cmd = wm_command_parse("Map {Raise}");
	assert(cmd);
	wm_command_execute(&server, cmd, NULL);
	assert(call_count == 3);
	wm_command_destroy(cmd);
	printf("  PASS: foreach_fluxbox_and_legacy_order\n");
}

static void
close_window(struct wm_view *target)
{
	/* Close the next window in the list as a side effect */
	if (target == &views[0])
		wl_list_remove(&views[1].link);
}

static void
test_foreach_survives_closed_windows(void)
{
	reset();
	add_view(0, "a", "A", &ws0);
	add_view(1, "b", "B", &ws0);
	add_view(2, "c", "C", &ws0);
	on_action = close_window;
	struct wm_command *cmd = wm_command_parse("ForEach {Close}");
	assert(cmd);
	wm_command_execute(&server, cmd, NULL);
	/* views[1] vanished mid-loop and is skipped */
	assert(call_count == 2);
	assert(calls[0].target == &views[0]);
	assert(calls[1].target == &views[2]);
	wm_command_destroy(cmd);
	on_action = NULL;
	printf("  PASS: foreach_survives_closed_windows\n");
}

static void
dispatch_for(int ms)
{
	struct timespec start, now;
	clock_gettime(CLOCK_MONOTONIC, &start);
	for (;;) {
		wl_event_loop_dispatch(server.wl_event_loop, 5);
		clock_gettime(CLOCK_MONOTONIC, &now);
		long elapsed = (now.tv_sec - start.tv_sec) * 1000 +
			(now.tv_nsec - start.tv_nsec) / 1000000;
		if (elapsed >= ms)
			break;
	}
}

static void
test_delay_debounces(void)
{
	reset();
	struct wm_command *c = wm_command_parse("Delay {Raise} 20000");
	assert(c && c->delay_us == 20000);
	wm_command_execute(&server, c, NULL);
	dispatch_for(5);
	wm_command_execute(&server, c, NULL); /* restarts the timer */
	assert(call_count == 0);
	assert(wl_list_length(&server.pending_commands) == 1);
	dispatch_for(60);
	assert(call_count == 1);
	assert(calls[0].action == WM_ACTION_RAISE);
	assert(wl_list_empty(&server.pending_commands));

	/* Pending delays are cancelled at shutdown */
	wm_command_execute(&server, c, NULL);
	wm_command_finish(&server);
	dispatch_for(40);
	assert(call_count == 1);
	wm_command_destroy(c);
	printf("  PASS: delay_debounces\n");
}

static void
test_run_string(void)
{
	reset();
	assert(wm_command_run(&server, "MacroCmd {Raise} {Lower}", NULL));
	assert(call_count == 2);
	assert(!wm_command_run(&server, "MacroCmd", NULL));
	assert(!wm_command_run(&server, "Bogus", NULL));
	printf("  PASS: run_string\n");
}

int
main(void)
{
	printf("test_command:\n");

	test_pattern_bare_word_is_anchored_name();
	test_pattern_terms_and_negation();
	test_pattern_current_workspace();
	test_pattern_current_focused();
	test_pattern_states_and_layer();
	test_pattern_tag();
	test_pattern_glob_fallback_and_errors();
	test_pattern_unknown_property_is_name_regex();
	test_window_list_parse();

	test_conditions();

	test_parse_leaf();
	test_parse_depth_limit();
	test_contains();

	test_macro_nested_with_target();
	test_toggle();
	test_if_uses_target_then_focused();
	test_foreach_fluxbox_and_legacy_order();
	test_foreach_survives_closed_windows();
	test_delay_debounces();
	test_run_string();

	wl_display_destroy(display);
	printf("All command tests passed.\n");
	return 0;
}
