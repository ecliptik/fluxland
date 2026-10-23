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


static bool
parse_modifier(const char *token, uint32_t *mods)
{
	if (strcasecmp(token, "Mod1") == 0) {
		*mods |= WLR_MODIFIER_ALT;
		return true;
	}
	if (strcasecmp(token, "Mod4") == 0) {
		*mods |= WLR_MODIFIER_LOGO;
		return true;
	}
	if (strcasecmp(token, "Control") == 0 ||
	    strcasecmp(token, "Ctrl") == 0) {
		*mods |= WLR_MODIFIER_CTRL;
		return true;
	}
	if (strcasecmp(token, "Shift") == 0) {
		*mods |= WLR_MODIFIER_SHIFT;
		return true;
	}
	if (strcasecmp(token, "None") == 0) {
		return true;
	}
	return false;
}

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

/*
 * Parse a context modifier token.
 * Returns true if the token is a context (On*).
 */
static bool
parse_context(const char *token, enum wm_mouse_context *ctx)
{
	if (strcasecmp(token, "OnDesktop") == 0) {
		*ctx = WM_MOUSE_CTX_DESKTOP;
		return true;
	}
	if (strcasecmp(token, "OnToolbar") == 0) {
		*ctx = WM_MOUSE_CTX_TOOLBAR;
		return true;
	}
	if (strcasecmp(token, "OnTitlebar") == 0) {
		*ctx = WM_MOUSE_CTX_TITLEBAR;
		return true;
	}
	if (strcasecmp(token, "OnTab") == 0) {
		*ctx = WM_MOUSE_CTX_TAB;
		return true;
	}
	if (strcasecmp(token, "OnWindow") == 0) {
		*ctx = WM_MOUSE_CTX_WINDOW;
		return true;
	}
	if (strcasecmp(token, "OnWindowBorder") == 0) {
		*ctx = WM_MOUSE_CTX_WINDOW_BORDER;
		return true;
	}
	if (strcasecmp(token, "OnLeftGrip") == 0) {
		*ctx = WM_MOUSE_CTX_LEFT_GRIP;
		return true;
	}
	if (strcasecmp(token, "OnRightGrip") == 0) {
		*ctx = WM_MOUSE_CTX_RIGHT_GRIP;
		return true;
	}
	if (strcasecmp(token, "OnSlit") == 0) {
		*ctx = WM_MOUSE_CTX_SLIT;
		return true;
	}
	return false;
}

/*
 * Map Fluxbox button number to linux input event code.
 * If button_map is non-NULL, use the configurable mapping table;
 * otherwise fall back to defaults.
 */
static uint32_t
button_number_to_code(int num, const uint32_t *button_map)
{
	if (num < 1 || num > 5)
		return 0;
	if (button_map)
		return button_map[num];
	switch (num) {
	case 1: return BTN_LEFT;
	case 2: return BTN_MIDDLE;
	case 3: return BTN_RIGHT;
	case 4: return BTN_SIDE;
	case 5: return BTN_EXTRA;
	default: return 0;
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
	if (!safe_atoi(num_str, &num) || num < 1 || num > 5)
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

	/* Parse tokens: context, modifiers, mouse event */
	enum wm_mouse_context context = WM_MOUSE_CTX_NONE;
	uint32_t modifiers = 0;
	enum wm_mouse_event event;
	uint32_t button;
	bool found_event = false;
	int i = 0;

	/* First token might be a context */
	if (i < token_count && parse_context(tokens[i], &context))
		i++;

	/* Consume modifiers */
	while (i < token_count) {
		if (!parse_modifier(tokens[i], &modifiers))
			break;
		i++;
	}

	/* Next token should be the mouse event */
	if (i < token_count && parse_mouse_event(tokens[i], &event, &button, button_map)) {
		found_event = true;
		i++;
	}

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

struct wm_mousebind *
mousebind_find(struct wl_list *bindings, enum wm_mouse_context context,
	enum wm_mouse_event event, uint32_t button, uint32_t modifiers)
{
	struct wm_mousebind *bind;

	/* First pass: exact context match */
	wl_list_for_each(bind, bindings, link) {
		if (bind->context == context &&
		    bind->event == event &&
		    bind->button == button &&
		    bind->modifiers == modifiers)
			return bind;
	}

	/* Second pass: global bindings (CTX_NONE) match any context */
	if (context != WM_MOUSE_CTX_NONE) {
		wl_list_for_each(bind, bindings, link) {
			if (bind->context == WM_MOUSE_CTX_NONE &&
			    bind->event == event &&
			    bind->button == button &&
			    bind->modifiers == modifiers)
				return bind;
		}
	}

	/* Third pass: OnWindow matches titlebar, border, grips too */
	if (context == WM_MOUSE_CTX_TITLEBAR ||
	    context == WM_MOUSE_CTX_WINDOW_BORDER ||
	    context == WM_MOUSE_CTX_LEFT_GRIP ||
	    context == WM_MOUSE_CTX_RIGHT_GRIP ||
	    context == WM_MOUSE_CTX_TAB) {
		wl_list_for_each(bind, bindings, link) {
			if (bind->context == WM_MOUSE_CTX_WINDOW &&
			    bind->event == event &&
			    bind->button == button &&
			    bind->modifiers == modifiers)
				return bind;
		}
	}

	return NULL;
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
