/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * mousebind.c - Mouse binding parser and management
 *
 * Parses Fluxbox-format mouse binding lines from the keys file:
 *   [OnContext] [Modifier ...] EventN :Action [argument]
 *
 * Context: OnDesktop, OnTitlebar, OnTab, OnWindow, OnWindowBorder,
 *          OnLeftGrip, OnRightGrip, OnToolbar
 * Events: MouseN (press), ClickN (click), DoubleN (double), MoveN (drag)
 * Button numbers: 1=left, 2=middle, 3=right, 4=scroll_up, 5=scroll_down
 */

#define _POSIX_C_SOURCE 200809L

#include "command.h"
#include "mousebind.h"
#include "util.h"
#include <ctype.h>
#include <linux/input-event-codes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wlr/types/wlr_keyboard.h>

#define KEYS_LINE_MAX 1024


static char *
strip(char *s)
{
	while (isspace((unsigned char)*s))
		s++;
	if (*s == '\0')
		return s;
	char *end = s + strlen(s) - 1;
	while (end > s && isspace((unsigned char)*end))
		end--;
	*(end + 1) = '\0';
	return s;
}

/* An On* context name (Keys.cc), as its flag; 0 if it is not one */
static uint32_t
parse_context(const char *token)
{
	static const struct {
		const char *name;
		uint32_t ctx;
	} names[] = {
		{ "OnDesktop",      WM_MOUSE_CTX_DESKTOP },
		{ "OnToolbar",      WM_MOUSE_CTX_TOOLBAR },
		{ "OnSlit",         WM_MOUSE_CTX_SLIT },
		{ "OnWindow",       WM_MOUSE_CTX_WINDOW },
		{ "OnTitlebar",     WM_MOUSE_CTX_TITLEBAR },
		{ "OnWinButton",    WM_MOUSE_CTX_WINBUTTON },
		{ "OnMinButton",    WM_MOUSE_CTX_MINBUTTON },
		{ "OnMaxButton",    WM_MOUSE_CTX_MAXBUTTON },
		{ "OnWindowBorder", WM_MOUSE_CTX_WINDOW_BORDER },
		{ "OnLeftGrip",     WM_MOUSE_CTX_LEFT_GRIP },
		{ "OnRightGrip",    WM_MOUSE_CTX_RIGHT_GRIP },
		{ "OnTab",          WM_MOUSE_CTX_TAB },
	};
	for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
		if (strcasecmp(token, names[i].name) == 0)
			return names[i].ctx;
	}
	return 0;
}

/*
 * Map Fluxbox button number to linux input event code.
 * If button_map is non-NULL, use the configurable mapping table;
 * otherwise fall back to defaults.
 */
static uint32_t
button_number_to_code(int num, const uint32_t *button_map)
{
	if (num >= 1 && num <= 5 && button_map && button_map[num])
		return button_map[num];
	switch (num) {
	case 1: return BTN_LEFT;
	case 2: return BTN_MIDDLE;
	case 3: return BTN_RIGHT;
	case 4: return WM_BUTTON_WHEEL_UP;
	case 5: return WM_BUTTON_WHEEL_DOWN;
	case 6: return WM_BUTTON_WHEEL_LEFT;
	case 7: return WM_BUTTON_WHEEL_RIGHT;
	case 8: return BTN_SIDE;
	case 9: return BTN_EXTRA;
	default:
		/* Further buttons follow BTN_EXTRA (BTN_FORWARD, BTN_BACK...) */
		return num < 16 ? (uint32_t)(BTN_EXTRA + (num - 9)) : 0;
	}
}

/*
 * Parse a mouse event token like "Mouse1", "Click3", "Double1", "Move1".
 * Returns true if parsed, sets event type and button code.
 */
static bool
parse_mouse_event(const char *token, enum wm_mouse_event *event,
		  uint32_t *button, const uint32_t *button_map)
{
	const char *num_str = NULL;

	if (strncasecmp(token, "Mouse", 5) == 0) {
		*event = WM_MOUSE_PRESS;
		num_str = token + 5;
	} else if (strncasecmp(token, "Click", 5) == 0) {
		*event = WM_MOUSE_CLICK;
		num_str = token + 5;
	} else if (strncasecmp(token, "Double", 6) == 0) {
		*event = WM_MOUSE_DOUBLE;
		num_str = token + 6;
	} else if (strncasecmp(token, "Move", 4) == 0) {
		*event = WM_MOUSE_MOVE;
		num_str = token + 4;
	} else {
		return false;
	}

	if (!num_str || !*num_str)
		return false;

	int num;
	if (!safe_atoi(num_str, &num) || num < 1)
		return false;

	*button = button_number_to_code(num, button_map);
	return *button != 0;
}

/*
 * Parse a single mouse binding line.
 * Format: [OnContext] [Modifier ...] EventN :Action [argument]
 */
static struct wm_mousebind *
parse_mousebind_line(const char *line, const uint32_t *button_map)
{
	const char *colon = strchr(line, ':');
	if (!colon)
		return NULL;

	size_t key_part_len = colon - line;
	char *key_part = strndup(line, key_part_len);
	if (!key_part)
		return NULL;

	const char *action_str = colon + 1;
	while (isspace((unsigned char)*action_str))
		action_str++;

	if (*action_str == '\0') {
		free(key_part);
		return NULL;
	}

	/* Tokenize the key part */
	char *saveptr;
	char *tokens[32];
	int token_count = 0;

	char *tok = strtok_r(key_part, " \t", &saveptr);
	while (tok && token_count < 32) {
		tokens[token_count++] = tok;
		tok = strtok_r(NULL, " \t", &saveptr);
	}

	if (token_count == 0) {
		free(key_part);
		return NULL;
	}

	/*
	 * Contexts, modifiers, "Double" and the event, in any order
	 * (Keys.cc): "OnTitlebar Double Mouse1", "Mod1 OnWindow Mouse3".
	 */
	uint32_t context = 0;
	uint32_t modifiers = 0;
	enum wm_mouse_event event = WM_MOUSE_PRESS;
	uint32_t button = 0;
	bool found_event = false, is_double = false;

	for (int i = 0; i < token_count; i++) {
		uint32_t ctx = parse_context(tokens[i]);
		if (ctx) {
			context |= ctx;
		} else if (strcasecmp(tokens[i], "Double") == 0) {
			is_double = true;
		} else if (wm_modifier_from_name(tokens[i], &modifiers)) {
			continue;
		} else if (!found_event &&
		    parse_mouse_event(tokens[i], &event, &button, button_map)) {
			found_event = true;
		} else {
			found_event = false;
			break;
		}
	}
	/* "Double" goes with a press ("Double Mouse1" = "Double1") */
	if (is_double && found_event && event == WM_MOUSE_PRESS)
		event = WM_MOUSE_DOUBLE;
	modifiers &= ~WM_MODIFIERS_IGNORED;

	if (!found_event) {
		free(key_part);
		return NULL;
	}

	/* Parse the command (shared grammar with keys and IPC) */
	struct wm_command *cmd = wm_command_parse(action_str);
	if (!cmd) {
		free(key_part);
		return NULL;
	}

	/* Build the mouse binding */
	struct wm_mousebind *bind = calloc(1, sizeof(*bind));
	if (!bind) {
		wm_command_destroy(cmd);
		free(key_part);
		return NULL;
	}

	bind->context = context;
	bind->event = event;
	bind->button = button;
	bind->modifiers = modifiers;
	bind->command = cmd;
	bind->action = cmd->action;
	if (cmd->argument)
		bind->argument = strdup(cmd->argument);

	free(key_part);
	return bind;
}

/*
 * Check if a line is a mouse binding line.
 * Mouse bindings start with On* context or contain Mouse/Click/Double/Move
 * event tokens.
 */
static bool
is_mouse_line(const char *line)
{
	/* Lines starting with On* are always mouse bindings */
	if (strncasecmp(line, "On", 2) == 0)
		return true;

	/* Check for Mouse/Click/Double/Move tokens anywhere before ':' */
	const char *colon = strchr(line, ':');
	if (!colon)
		return false;

	/* Scan tokens before the colon */
	size_t len = colon - line;
	char *copy = strndup(line, len);
	if (!copy)
		return false;

	bool found = false;
	char *saveptr;
	char *tok = strtok_r(copy, " \t", &saveptr);
	while (tok) {
		if (strncasecmp(tok, "Mouse", 5) == 0 ||
		    strncasecmp(tok, "Click", 5) == 0 ||
		    strncasecmp(tok, "Double", 6) == 0 ||
		    strncasecmp(tok, "Move", 4) == 0) {
			/* Make sure it's followed by a digit */
			const char *p = tok;
			while (*p && !isdigit((unsigned char)*p))
				p++;
			if (*p) {
				found = true;
				break;
			}
		}
		tok = strtok_r(NULL, " \t", &saveptr);
	}

	free(copy);
	return found;
}

int
mousebind_load(struct wl_list *bindings, const char *path,
	const uint32_t *button_map)
{
	FILE *f = fopen_nofollow(path, "r");
	if (!f)
		return -1;

	char line[KEYS_LINE_MAX];
	while (fgets(line, sizeof(line), f)) {
		/* Skip remainder of lines longer than buffer */
		size_t len = strlen(line);
		if (len > 0 && line[len - 1] != '\n' && !feof(f)) {
			int ch;
			while ((ch = fgetc(f)) != EOF && ch != '\n')
				;
			continue;
		}

		char *p = strip(line);

		if (*p == '\0' || *p == '#' || *p == '!')
			continue;

		if (!is_mouse_line(p))
			continue;

		struct wm_mousebind *bind =
			parse_mousebind_line(p, button_map);
		if (bind)
			wl_list_insert(bindings->prev, &bind->link);
	}

	fclose(f);
	return 0;
}

/*
 * The command after the binding's ':' (after "mode:" for a key mode
 * line), or NULL.
 */
static const char *
command_part(const char *line)
{
	const char *colon = strchr(line, ':');
	return colon ? colon + 1 : NULL;
}

bool
wm_keys_check_line(const char *line, char *why, size_t len)
{
	char buf[KEYS_LINE_MAX];
	snprintf(buf, sizeof(buf), "%s", line);
	char *p = strip(buf);
	if (*p == '\0' || *p == '#' || *p == '!')
		return true;

	/* "mode: key :command" (as parse_mode_prefix reads it) */
	const char *rest = p;
	bool in_mode = false;
	const char *q = p;
	while (*q && !isspace((unsigned char)*q) && *q != ':')
		q++;
	if (*q == ':' && q != p && strchr(q + 1, ':')) {
		in_mode = true;
		rest = q + 1;
		while (isspace((unsigned char)*rest))
			rest++;
	}

	const char *cmd_text = command_part(rest);
	if (!cmd_text) {
		snprintf(why, len, "no ':' before the command");
		return false;
	}
	while (isspace((unsigned char)*cmd_text))
		cmd_text++;
	if (!*cmd_text) {
		snprintf(why, len, "no command after ':'");
		return false;
	}
	struct wm_command *cmd = wm_command_parse(cmd_text);
	if (!cmd) {
		char word[64];
		size_t n = strcspn(cmd_text, " \t");
		snprintf(word, sizeof(word), "%.*s",
			(int)(n < sizeof(word) ? n : sizeof(word) - 1),
			cmd_text);
		snprintf(why, len, "unknown or malformed command '%s'", word);
		return false;
	}
	wm_command_destroy(cmd);

	if (is_mouse_line(rest)) {
		if (in_mode) {
			snprintf(why, len, "mouse bindings in a key mode "
				"aren't supported yet");
			return false;
		}
		struct wm_mousebind *bind = parse_mousebind_line(rest, NULL);
		if (!bind) {
			snprintf(why, len, "not a mouse binding fluxland "
				"understands");
			return false;
		}
		wm_command_destroy(bind->command);
		free(bind->argument);
		free(bind);
		return true;
	}

	struct wl_list tmp;
	wl_list_init(&tmp);
	bool ok = keybind_add_from_string(&tmp, rest);
	keybind_destroy_list(&tmp);
	if (!ok)
		snprintf(why, len, "unknown key or modifier");
	return ok;
}

/* A binding without On* is for the desktop (Fluxbox's GLOBAL) */
static uint32_t
bind_context(const struct wm_mousebind *bind)
{
	return bind->context ? bind->context : WM_MOUSE_CTX_GLOBAL;
}

struct wm_mousebind *
mousebind_find(struct wl_list *bindings, uint32_t context,
	enum wm_mouse_event event, uint32_t button, uint32_t modifiers)
{
	struct wm_mousebind *bind;
	wl_list_for_each(bind, bindings, link) {
		if ((bind_context(bind) & context) &&
		    bind->event == event &&
		    bind->button == button &&
		    bind->modifiers == modifiers)
			return bind;
	}
	return NULL;
}

bool
mousebind_has_move(struct wl_list *bindings, uint32_t context,
	uint32_t button, uint32_t modifiers)
{
	return mousebind_find(bindings, context, WM_MOUSE_MOVE, button,
		modifiers) != NULL;
}

void
mousebind_destroy_list(struct wl_list *bindings)
{
	struct wm_mousebind *bind, *tmp;
	wl_list_for_each_safe(bind, tmp, bindings, link) {
		wl_list_remove(&bind->link);
		wm_command_destroy(bind->command);
		free(bind->argument);
		free(bind);
	}
}
