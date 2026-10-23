/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * command.c - Fluxbox command language (parse and execute)
 *
 * Grammar and semantics follow Fluxbox's FbTk/MacroCommand.cc,
 * FbTk/LogicCommands.cc and WorkspaceCmd.cc (ForEach, Some, Every).
 */

#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wayland-server-core.h>
#include <wlr/util/log.h>

#include "client_pattern.h"
#include "command.h"
#include "keyboard_actions.h"
#include "server.h"
#include "view.h"

/* Nesting limit, so a hostile IPC string can't exhaust the stack */
#define MAX_DEPTH 32

/* --- Lexing helpers --- */

static void
skip_ws(const char **str)
{
	while (**str && isspace((unsigned char)**str))
		(*str)++;
}

/*
 * Extract the content of a {...} block at *str (after whitespace),
 * honouring nesting. Advances *str past the closing brace.
 */
static char *
extract_block(const char **str)
{
	skip_ws(str);
	if (**str != '{')
		return NULL;
	const char *start = ++(*str);
	int depth = 1;
	while (**str) {
		if (**str == '{')
			depth++;
		else if (**str == '}' && --depth == 0)
			break;
		(*str)++;
	}
	if (depth != 0)
		return NULL;
	char *out = strndup(start, (size_t)(*str - start));
	(*str)++;
	return out;
}

/* Read a bare word (up to whitespace, '(' or '{') into buf. */
static bool
read_word(const char **str, char *buf, size_t len)
{
	skip_ws(str);
	const char *start = *str;
	while (**str && !isspace((unsigned char)**str) &&
	       **str != '(' && **str != '{')
		(*str)++;
	size_t n = (size_t)(*str - start);
	if (n == 0 || n >= len)
		return false;
	memcpy(buf, start, n);
	buf[n] = '\0';
	return true;
}

/* --- Conditions --- */

void
wm_condition_destroy(struct wm_condition *cond)
{
	if (!cond)
		return;
	wm_client_pattern_destroy(cond->pattern);
	wm_condition_destroy(cond->child);
	for (int i = 0; i < cond->operand_count; i++)
		wm_condition_destroy(cond->operands[i]);
	free(cond->operands);
	free(cond);
}

static struct wm_condition *parse_condition(const char **str, int depth);

/* A condition operand: either {condition} or a bare condition. */
static struct wm_condition *
parse_operand(const char **str, int depth)
{
	skip_ws(str);
	if (**str == '{') {
		char *block = extract_block(str);
		if (!block)
			return NULL;
		const char *bp = block;
		struct wm_condition *c = parse_condition(&bp, depth + 1);
		skip_ws(&bp);
		if (c && *bp != '\0') {
			wm_condition_destroy(c);
			c = NULL;
		}
		free(block);
		return c;
	}
	/* fluxland 1.x: "Some (prop=pattern)" meant "Some Matches (...)" */
	if (**str == '(') {
		struct wm_condition *c = calloc(1, sizeof(*c));
		if (!c)
			return NULL;
		c->type = WM_COND_MATCHES;
		c->pattern = wm_client_pattern_parse(*str, str);
		if (!c->pattern) {
			free(c);
			return NULL;
		}
		return c;
	}
	return parse_condition(str, depth + 1);
}

static struct wm_condition *
parse_condition(const char **str, int depth)
{
	if (depth > MAX_DEPTH)
		return NULL;

	skip_ws(str);
	if (**str == '{' || **str == '(')
		return parse_operand(str, depth);

	char word[32];
	if (!read_word(str, word, sizeof(word))) {
		wlr_log(WLR_ERROR, "%s", "condition: expected a keyword");
		return NULL;
	}

	struct wm_condition *cond = calloc(1, sizeof(*cond));
	if (!cond)
		return NULL;

	if (strcasecmp(word, "Matches") == 0) {
		cond->type = WM_COND_MATCHES;
		skip_ws(str);
		cond->pattern = wm_client_pattern_parse(*str, str);
		if (!cond->pattern) {
			wlr_log(WLR_ERROR, "Matches: bad client pattern '%s'",
				*str);
			goto error;
		}
	} else if (strcasecmp(word, "Some") == 0 ||
		   strcasecmp(word, "Every") == 0 ||
		   strcasecmp(word, "Not") == 0) {
		cond->type = strcasecmp(word, "Some") == 0 ? WM_COND_SOME :
			strcasecmp(word, "Every") == 0 ? WM_COND_EVERY :
			WM_COND_NOT;
		cond->child = parse_operand(str, depth);
		if (!cond->child) {
			wlr_log(WLR_ERROR, "%s: missing condition", word);
			goto error;
		}
	} else if (strcasecmp(word, "And") == 0 ||
		   strcasecmp(word, "Or") == 0 ||
		   strcasecmp(word, "Xor") == 0) {
		cond->type = strcasecmp(word, "And") == 0 ? WM_COND_AND :
			strcasecmp(word, "Or") == 0 ? WM_COND_OR :
			WM_COND_XOR;
		skip_ws(str);
		while (**str == '{') {
			struct wm_condition *op = parse_operand(str, depth);
			if (!op)
				goto error;
			struct wm_condition **ops = realloc(cond->operands,
				sizeof(*ops) * (size_t)(cond->operand_count + 1));
			if (!ops) {
				wm_condition_destroy(op);
				goto error;
			}
			cond->operands = ops;
			cond->operands[cond->operand_count++] = op;
			skip_ws(str);
		}
		if (cond->operand_count < 1) {
			wlr_log(WLR_ERROR, "%s: needs {condition} blocks",
				word);
			goto error;
		}
	} else {
		wlr_log(WLR_ERROR, "unknown condition keyword: %s", word);
		goto error;
	}
	return cond;

error:
	wm_condition_destroy(cond);
	return NULL;
}

struct wm_condition *
wm_condition_parse(const char *str)
{
	if (!str)
		return NULL;
	const char *p = str;
	struct wm_condition *cond = parse_condition(&p, 0);
	skip_ws(&p);
	if (cond && *p != '\0') {
		wlr_log(WLR_ERROR, "condition: trailing text '%s'", p);
		wm_condition_destroy(cond);
		return NULL;
	}
	return cond;
}

bool
wm_condition_evaluate(struct wm_server *server, struct wm_condition *cond,
	struct wm_view *view)
{
	if (!cond)
		return false;

	switch (cond->type) {
	case WM_COND_MATCHES:
		return view && wm_client_pattern_match(server,
			cond->pattern, view);
	case WM_COND_SOME: {
		struct wm_view *v;
		wl_list_for_each(v, &server->views, link) {
			if (wm_condition_evaluate(server, cond->child, v))
				return true;
		}
		return false;
	}
	case WM_COND_EVERY: {
		struct wm_view *v;
		wl_list_for_each(v, &server->views, link) {
			if (!wm_condition_evaluate(server, cond->child, v))
				return false;
		}
		return true;
	}
	case WM_COND_NOT:
		return !wm_condition_evaluate(server, cond->child, view);
	case WM_COND_AND:
		for (int i = 0; i < cond->operand_count; i++) {
			if (!wm_condition_evaluate(server,
					cond->operands[i], view))
				return false;
		}
		return true;
	case WM_COND_OR:
		for (int i = 0; i < cond->operand_count; i++) {
			if (wm_condition_evaluate(server,
					cond->operands[i], view))
				return true;
		}
		return false;
	case WM_COND_XOR: {
		bool result = false;
		for (int i = 0; i < cond->operand_count; i++)
			result ^= wm_condition_evaluate(server,
				cond->operands[i], view);
		return result;
	}
	}
	return false;
}

/* --- Commands --- */

void
wm_command_destroy(struct wm_command *cmd)
{
	if (!cmd)
		return;
	free(cmd->argument);
	for (int i = 0; i < cmd->child_count; i++)
		wm_command_destroy(cmd->children[i]);
	free(cmd->children);
	wm_condition_destroy(cmd->condition);
	wm_command_destroy(cmd->then_cmd);
	wm_command_destroy(cmd->else_cmd);
	free(cmd);
}

static struct wm_command *parse_command(const char *str, int depth);

static struct wm_command *
parse_block_command(const char **str, int depth)
{
	char *block = extract_block(str);
	if (!block)
		return NULL;
	struct wm_command *cmd = parse_command(block, depth + 1);
	free(block);
	return cmd;
}

/* {cmd} {cmd} ... for MacroCmd / ToggleCmd */
static bool
parse_children(struct wm_command *cmd, const char *args, int depth)
{
	const char *p = args;
	skip_ws(&p);
	while (*p == '{') {
		struct wm_command *child = parse_block_command(&p, depth);
		if (child) {
			struct wm_command **kids = realloc(cmd->children,
				sizeof(*kids) * (size_t)(cmd->child_count + 1));
			if (!kids) {
				wm_command_destroy(child);
				return false;
			}
			cmd->children = kids;
			cmd->children[cmd->child_count++] = child;
		}
		/* Fluxbox skips unparsable sub-commands */
		skip_ws(&p);
	}
	return cmd->child_count > 0;
}

static bool
parse_if(struct wm_command *cmd, const char *args, int depth)
{
	const char *p = args;
	char *cond_block = extract_block(&p);
	if (!cond_block)
		return false;
	const char *cp = cond_block;
	cmd->condition = parse_condition(&cp, depth + 1);
	free(cond_block);
	if (!cmd->condition)
		return false;

	skip_ws(&p);
	if (*p == '{')
		cmd->then_cmd = parse_block_command(&p, depth);
	skip_ws(&p);
	if (*p == '{')
		cmd->else_cmd = parse_block_command(&p, depth);
	return cmd->then_cmd || cmd->else_cmd;
}

/* fluxland 1.x wrote ForEach {condition} {command}: detect that order */
static bool
starts_with_condition(const char *text)
{
	static const char *const keywords[] = {
		"Matches", "Some", "Every", "Not", "And", "Or", "Xor",
	};
	const char *p = text;
	char word[16];
	if (!read_word(&p, word, sizeof(word)))
		return false;
	for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++) {
		if (strcasecmp(word, keywords[i]) == 0)
			return true;
	}
	return false;
}

static bool
parse_foreach(struct wm_command *cmd, const char *args, int depth)
{
	const char *p = args;
	char *first = extract_block(&p);
	if (!first)
		return false;
	skip_ws(&p);
	char *second = *p == '{' ? extract_block(&p) : NULL;

	/* Fluxbox order: ForEach {command} [{condition}] */
	if (!starts_with_condition(first))
		cmd->then_cmd = parse_command(first, depth + 1);
	if (cmd->then_cmd) {
		if (second) {
			const char *sp = second;
			cmd->condition = parse_condition(&sp, depth + 1);
		}
		bool ok = !second || cmd->condition;
		free(first);
		free(second);
		return ok;
	}

	/* fluxland 1.x order: ForEach {condition} {command} */
	if (second) {
		const char *fp = first;
		cmd->condition = parse_condition(&fp, depth + 1);
		cmd->then_cmd = parse_command(second, depth + 1);
	}
	free(first);
	free(second);
	return cmd->condition && cmd->then_cmd;
}

static bool
parse_delay(struct wm_command *cmd, const char *args, int depth)
{
	const char *p = args;
	cmd->then_cmd = parse_block_command(&p, depth);
	if (!cmd->then_cmd)
		return false;
	skip_ws(&p);
	cmd->delay_us = 200; /* Fluxbox's default (FbTk/Timer.cc) */
	if (*p) {
		char *end;
		errno = 0;
		long val = strtol(p, &end, 10);
		if (!errno && end != p && val > 0 && val <= INT_MAX)
			cmd->delay_us = (int)val;
	}
	return true;
}

static struct wm_command *
create_command(enum wm_action action, const char *arg, int depth)
{
	/* Trim trailing whitespace from the argument */
	size_t arg_len = arg ? strlen(arg) : 0;
	while (arg_len > 0 && isspace((unsigned char)arg[arg_len - 1]))
		arg_len--;

	struct wm_command *cmd = calloc(1, sizeof(*cmd));
	if (!cmd)
		return NULL;
	cmd->action = action;
	if (arg_len > 0) {
		cmd->argument = strndup(arg, arg_len);
		if (!cmd->argument) {
			free(cmd);
			return NULL;
		}
	}

	bool ok = true;
	switch (action) {
	case WM_ACTION_MACRO_CMD:
	case WM_ACTION_TOGGLE_CMD:
		ok = cmd->argument && parse_children(cmd, cmd->argument,
			depth);
		break;
	case WM_ACTION_IF:
		ok = cmd->argument && parse_if(cmd, cmd->argument, depth);
		break;
	case WM_ACTION_FOREACH:
	case WM_ACTION_MAP:
		ok = cmd->argument && parse_foreach(cmd, cmd->argument,
			depth);
		break;
	case WM_ACTION_DELAY:
		ok = cmd->argument && parse_delay(cmd, cmd->argument, depth);
		break;
	default:
		break;
	}
	if (!ok) {
		wlr_log(WLR_ERROR, "malformed compound command: %s",
			cmd->argument ? cmd->argument : "");
		wm_command_destroy(cmd);
		return NULL;
	}
	return cmd;
}

static struct wm_command *
parse_command(const char *str, int depth)
{
	if (!str || depth > MAX_DEPTH)
		return NULL;

	const char *p = str;
	skip_ws(&p);
	const char *name_start = p;
	while (*p && !isspace((unsigned char)*p) && *p != '{')
		p++;
	size_t name_len = (size_t)(p - name_start);
	char name[64];
	if (name_len == 0 || name_len >= sizeof(name))
		return NULL;
	memcpy(name, name_start, name_len);
	name[name_len] = '\0';

	enum wm_action action = wm_action_from_name(name);
	if (action == WM_ACTION_NOP && strcasecmp(name, "Nop") != 0) {
		wlr_log(WLR_ERROR, "unknown command: %s", name);
		return NULL;
	}

	skip_ws(&p);
	return create_command(action, p, depth);
}

struct wm_command *
wm_command_parse(const char *str)
{
	return parse_command(str, 0);
}

struct wm_command *
wm_command_create(enum wm_action action, const char *argument)
{
	return create_command(action, argument, 0);
}

bool
wm_command_contains(const struct wm_command *cmd, enum wm_action action)
{
	if (!cmd)
		return false;
	if (cmd->action == action)
		return true;
	for (int i = 0; i < cmd->child_count; i++) {
		if (wm_command_contains(cmd->children[i], action))
			return true;
	}
	return wm_command_contains(cmd->then_cmd, action) ||
		wm_command_contains(cmd->else_cmd, action);
}

/* --- Delay (debounced, like Fluxbox's DelayedCmd) --- */

struct pending_delay {
	struct wl_list link;          /* wm_server.pending_commands */
	struct wm_server *server;
	const struct wm_command *key; /* the Delay node; compared only */
	char *body;                   /* command text to run */
	struct wl_event_source *timer;
};

static void
ensure_pending_list(struct wm_server *server)
{
	if (!server->pending_commands.next)
		wl_list_init(&server->pending_commands);
}

static void
pending_free(struct pending_delay *pd)
{
	wl_list_remove(&pd->link);
	if (pd->timer)
		wl_event_source_remove(pd->timer);
	free(pd->body);
	free(pd);
}

static int
delay_fire(void *data)
{
	struct pending_delay *pd = data;
	struct wm_server *server = pd->server;
	char *body = pd->body;
	pd->body = NULL;
	pending_free(pd);
	wm_command_run(server, body, NULL);
	free(body);
	return 0;
}

static bool
execute_delay(struct wm_server *server, struct wm_command *cmd)
{
	ensure_pending_list(server);
	int ms = cmd->delay_us / 1000;
	if (ms < 1)
		ms = 1;

	struct pending_delay *pd;
	wl_list_for_each(pd, &server->pending_commands, link) {
		if (pd->key == cmd) {
			/* Re-triggered: restart the timer */
			wl_event_source_timer_update(pd->timer, ms);
			return true;
		}
	}

	const char *p = cmd->argument;
	char *body = p ? extract_block(&p) : NULL;
	if (!body)
		return false;
	pd = calloc(1, sizeof(*pd));
	if (!pd) {
		free(body);
		return false;
	}
	pd->server = server;
	pd->key = cmd;
	pd->body = body;
	wl_list_insert(&server->pending_commands, &pd->link);
	pd->timer = wl_event_loop_add_timer(server->wl_event_loop,
		delay_fire, pd);
	if (!pd->timer) {
		pending_free(pd);
		return false;
	}
	wl_event_source_timer_update(pd->timer, ms);
	return true;
}

void
wm_command_finish(struct wm_server *server)
{
	ensure_pending_list(server);
	struct pending_delay *pd, *tmp;
	wl_list_for_each_safe(pd, tmp, &server->pending_commands, link)
		pending_free(pd);
}

/* --- Execution --- */

static bool
view_still_exists(struct wm_server *server, uint32_t id,
	struct wm_view **out)
{
	struct wm_view *v;
	wl_list_for_each(v, &server->views, link) {
		if (v->id == id) {
			*out = v;
			return true;
		}
	}
	return false;
}

static bool
execute_foreach(struct wm_server *server, struct wm_command *cmd)
{
	/* Snapshot ids: the body may close windows or reorder the list */
	int n = wl_list_length(&server->views);
	if (n == 0)
		return true;
	uint32_t *ids = calloc((size_t)n, sizeof(*ids));
	if (!ids)
		return false;
	int count = 0;
	struct wm_view *v;
	wl_list_for_each(v, &server->views, link) {
		if (count < n)
			ids[count++] = v->id;
	}
	for (int i = 0; i < count; i++) {
		if (!view_still_exists(server, ids[i], &v))
			continue;
		if (cmd->condition &&
		    !wm_condition_evaluate(server, cmd->condition, v))
			continue;
		wm_command_execute(server, cmd->then_cmd, v);
	}
	free(ids);
	return true;
}

bool
wm_command_execute(struct wm_server *server, struct wm_command *cmd,
	struct wm_view *target)
{
	if (!cmd)
		return false;

	switch (cmd->action) {
	case WM_ACTION_MACRO_CMD: {
		bool ok = true;
		for (int i = 0; i < cmd->child_count; i++)
			ok &= wm_command_execute(server, cmd->children[i],
				target);
		return ok;
	}
	case WM_ACTION_TOGGLE_CMD: {
		if (cmd->child_count == 0)
			return false;
		int idx = cmd->toggle_index % cmd->child_count;
		cmd->toggle_index = (idx + 1) % cmd->child_count;
		return wm_command_execute(server, cmd->children[idx], target);
	}
	case WM_ACTION_IF: {
		struct wm_view *view = target ? target : server->focused_view;
		if (wm_condition_evaluate(server, cmd->condition, view))
			return !cmd->then_cmd ||
				wm_command_execute(server, cmd->then_cmd,
					target);
		return !cmd->else_cmd ||
			wm_command_execute(server, cmd->else_cmd, target);
	}
	case WM_ACTION_FOREACH:
	case WM_ACTION_MAP:
		return execute_foreach(server, cmd);
	case WM_ACTION_DELAY:
		return execute_delay(server, cmd);
	default:
		return wm_execute_action_on(server, cmd->action,
			cmd->argument, target);
	}
}

bool
wm_command_run(struct wm_server *server, const char *str,
	struct wm_view *target)
{
	struct wm_command *cmd = wm_command_parse(str);
	if (!cmd)
		return false;
	bool ok = wm_command_execute(server, cmd, target);
	wm_command_destroy(cmd);
	return ok;
}
