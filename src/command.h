/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * command.h - Fluxbox command language (parse and execute)
 *
 * One parser and executor shared by key bindings, mouse bindings, menu
 * [command] items and IPC. Commands nest like Fluxbox's:
 *
 *   MacroCmd {cmd} {cmd} ...
 *   ToggleCmd {cmd} {cmd} ...
 *   If {condition} {then} [{else}]       (alias: Cond)
 *   ForEach {cmd} [{condition}]          (alias: Map)
 *   Delay {cmd} [microseconds]
 *
 * Conditions:
 *   Matches <client pattern>
 *   Some <condition> | Every <condition> | Not <condition>
 *   And|Or|Xor {condition} {condition} ...
 *
 * Every command runs against a target window: the window under the
 * cursor for mouse bindings, each window in turn for ForEach, and the
 * focused window otherwise (target == NULL).
 */

#ifndef WM_COMMAND_H
#define WM_COMMAND_H

#include <stdbool.h>
#include <wayland-server-core.h>

#include "keybind.h"

struct wm_server;
struct wm_view;
struct wm_client_pattern;

enum wm_condition_type {
	WM_COND_MATCHES,   /* target window matches a client pattern */
	WM_COND_SOME,      /* true if ANY window satisfies child */
	WM_COND_EVERY,     /* true if ALL windows satisfy child */
	WM_COND_NOT,       /* negate child */
	WM_COND_AND,       /* all operands */
	WM_COND_OR,        /* any operand */
	WM_COND_XOR,       /* odd number of operands */
};

struct wm_condition {
	enum wm_condition_type type;
	struct wm_client_pattern *pattern;  /* Matches */
	struct wm_condition *child;         /* Not, Some, Every */
	struct wm_condition **operands;     /* And, Or, Xor */
	int operand_count;
};

struct wm_command {
	enum wm_action action;
	char *argument;                 /* raw argument text */

	/* MacroCmd / ToggleCmd */
	struct wm_command **children;
	int child_count;
	int toggle_index;

	/* If / ForEach / Map */
	struct wm_condition *condition;
	struct wm_command *then_cmd;    /* If, ForEach/Map body, Delay body */
	struct wm_command *else_cmd;    /* If */
	int delay_us;                   /* Delay */
};

/* Parse "Action [args]". Returns NULL for unknown or malformed commands. */
struct wm_command *wm_command_parse(const char *str);

/* Build a command from an action and its argument text. */
struct wm_command *wm_command_create(enum wm_action action,
	const char *argument);

void wm_command_destroy(struct wm_command *cmd);

/* Parse a condition expression, e.g. "Matches (title=foo)". */
struct wm_condition *wm_condition_parse(const char *str);

void wm_condition_destroy(struct wm_condition *cond);

/* Evaluate a condition against a window (NULL = no window). */
bool wm_condition_evaluate(struct wm_server *server,
	struct wm_condition *cond, struct wm_view *view);

/* True if the command or any nested command is the given action. */
bool wm_command_contains(const struct wm_command *cmd,
	enum wm_action action);

/*
 * Execute a parsed command against `target` (NULL = focused window).
 * Returns false if a leaf action failed or is unsupported.
 */
bool wm_command_execute(struct wm_server *server, struct wm_command *cmd,
	struct wm_view *target);

/* Parse, execute once and free. Returns false on parse failure too. */
bool wm_command_run(struct wm_server *server, const char *str,
	struct wm_view *target);

/* Cancel pending Delay timers (server shutdown). */
void wm_command_finish(struct wm_server *server);

#endif /* WM_COMMAND_H */
