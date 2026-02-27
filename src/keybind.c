/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * keybind.c - Keybinding parser and management
 *
 * Parses Fluxbox-format keys files. Supports:
 *   [Modifier ...] key :Action [argument]
 *   [Modifier ...] key [Modifier ...] key :Action   (chains)
 *   modename: [Modifier ...] key :Action             (keymodes)
 *   MacroCmd {cmd1} {cmd2} ...                       (meta-commands)
 *   ToggleCmd {cmd1} {cmd2} ...
 *
 * Modifiers: None, Mod1 (Alt), Mod4 (Super), Control, Shift
 * Key names use XKB naming convention.
 */

#define _POSIX_C_SOURCE 200809L

#include "keybind.h"
#include "util.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/util/log.h>

#define KEYS_LINE_MAX 1024

struct action_map {
	const char *name;
	enum wm_action action;
};

static const struct action_map actions[] = {
	{"ActivateTab",        WM_ACTION_ACTIVATE_TAB},
	{"AddWorkspace",       WM_ACTION_ADD_WORKSPACE},
	{"ArrangeWindows",     WM_ACTION_ARRANGE_WINDOWS},
	{"BindKey",            WM_ACTION_BIND_KEY},
	{"ArrangeWindowsHorizontal", WM_ACTION_ARRANGE_HORIZ},
	{"ArrangeWindowsStackBottom", WM_ACTION_ARRANGE_STACK_BOTTOM},
	{"ArrangeWindowsStackLeft", WM_ACTION_ARRANGE_STACK_LEFT},
	{"ArrangeWindowsStackRight", WM_ACTION_ARRANGE_STACK_RIGHT},
	{"ArrangeWindowsStackTop", WM_ACTION_ARRANGE_STACK_TOP},
	{"ArrangeWindowsVertical", WM_ACTION_ARRANGE_VERT},
	{"CascadeWindows",    WM_ACTION_CASCADE_WINDOWS},
	{"Close",              WM_ACTION_CLOSE},
	{"ClientMenu",         WM_ACTION_CLIENT_MENU},
	{"CloseAllWindows",    WM_ACTION_CLOSE_ALL_WINDOWS},
	{"CustomMenu",         WM_ACTION_CUSTOM_MENU},
	{"Deiconify",          WM_ACTION_DEICONIFY},
	{"Delay",              WM_ACTION_DELAY},
	{"DetachClient",       WM_ACTION_DETACH_CLIENT},
	{"FocusActivate",      WM_ACTION_FOCUS_ACTIVATE},
	{"Exec",               WM_ACTION_EXEC},
	{"ExecCommand",        WM_ACTION_EXEC},
	{"Execute",            WM_ACTION_EXEC},
	{"Exit",               WM_ACTION_EXIT},
	{"Export",             WM_ACTION_SET_ENV},
	{"Focus",              WM_ACTION_FOCUS},
	{"FocusDown",          WM_ACTION_FOCUS_DOWN},
	{"FocusLeft",          WM_ACTION_FOCUS_LEFT},
	{"ForEach",            WM_ACTION_FOREACH},
	{"FocusNext",          WM_ACTION_FOCUS_NEXT},
	{"FocusNextElement",   WM_ACTION_FOCUS_NEXT_ELEMENT},
	{"FocusPrev",          WM_ACTION_FOCUS_PREV},
	{"FocusPrevElement",   WM_ACTION_FOCUS_PREV_ELEMENT},
	{"FocusRight",         WM_ACTION_FOCUS_RIGHT},
	{"FocusToolbar",       WM_ACTION_FOCUS_TOOLBAR},
	{"FocusUp",            WM_ACTION_FOCUS_UP},
	{"Fullscreen",         WM_ACTION_FULLSCREEN},
	{"GotoWindow",         WM_ACTION_GOTO_WINDOW},
	{"HideMenus",         WM_ACTION_HIDE_MENUS},
	{"Iconify",            WM_ACTION_MINIMIZE},
	{"If",                 WM_ACTION_IF},
	{"KeyMode",            WM_ACTION_KEY_MODE},
	{"Kill",               WM_ACTION_KILL},
	{"KillWindow",         WM_ACTION_KILL},
	{"LeftWorkspace",      WM_ACTION_LEFT_WORKSPACE},
	{"LHalf",              WM_ACTION_LHALF},
	{"Lower",              WM_ACTION_LOWER},
	{"LowerLayer",         WM_ACTION_LOWER_LAYER},
	{"MacroCmd",           WM_ACTION_MACRO_CMD},
	{"Map",                WM_ACTION_MAP},
	{"Maximize",           WM_ACTION_MAXIMIZE},
	{"MaximizeHorizontal", WM_ACTION_MAXIMIZE_HORIZ},
	{"MaximizeVertical",   WM_ACTION_MAXIMIZE_VERT},
	{"Minimize",           WM_ACTION_MINIMIZE},
	{"Move",               WM_ACTION_MOVE},
	{"MoveDown",           WM_ACTION_MOVE_DOWN},
	{"MoveLeft",           WM_ACTION_MOVE_LEFT},
	{"MoveRight",          WM_ACTION_MOVE_RIGHT},
	{"MoveTabLeft",        WM_ACTION_MOVE_TAB_LEFT},
	{"MoveTabRight",       WM_ACTION_MOVE_TAB_RIGHT},
	{"MoveTo",             WM_ACTION_MOVE_TO},
	{"MoveUp",             WM_ACTION_MOVE_UP},
	{"NextGroup",          WM_ACTION_NEXT_GROUP},
	{"NextLayout",         WM_ACTION_NEXT_LAYOUT},
	{"NextTab",            WM_ACTION_NEXT_TAB},
	{"NextWindow",         WM_ACTION_NEXT_WINDOW},
	{"NextWorkspace",      WM_ACTION_NEXT_WORKSPACE},
	{"PrevGroup",          WM_ACTION_PREV_GROUP},
	{"PrevLayout",         WM_ACTION_PREV_LAYOUT},
	{"PrevTab",            WM_ACTION_PREV_TAB},
	{"PrevWindow",         WM_ACTION_PREV_WINDOW},
	{"PrevWorkspace",      WM_ACTION_PREV_WORKSPACE},
	{"Quit",               WM_ACTION_EXIT},
	{"RHalf",              WM_ACTION_RHALF},
	{"Raise",              WM_ACTION_RAISE},
	{"RaiseLayer",         WM_ACTION_RAISE_LAYER},
	{"Remember",           WM_ACTION_REMEMBER},
	{"RootMenu",           WM_ACTION_ROOT_MENU},
	{"Reconfigure",        WM_ACTION_RECONFIGURE},
	{"ReloadStyle",        WM_ACTION_RELOAD_STYLE},
	{"RemoveLastWorkspace", WM_ACTION_REMOVE_LAST_WORKSPACE},
	{"Resize",             WM_ACTION_RESIZE},
	{"ResizeHorizontal",   WM_ACTION_RESIZE_HORIZ},
	{"ResizeTo",           WM_ACTION_RESIZE_TO},
	{"ResizeVertical",     WM_ACTION_RESIZE_VERT},
	{"RightWorkspace",     WM_ACTION_RIGHT_WORKSPACE},
	{"SetAlpha",           WM_ACTION_SET_ALPHA},
	{"SendToNextHead",     WM_ACTION_SEND_TO_NEXT_HEAD},
	{"SendToNextWorkspace", WM_ACTION_SEND_TO_NEXT_WORKSPACE},
	{"SendToPrevHead",     WM_ACTION_SEND_TO_PREV_HEAD},
	{"SendToPrevWorkspace", WM_ACTION_SEND_TO_PREV_WORKSPACE},
	{"SendToWorkspace",    WM_ACTION_SEND_TO_WORKSPACE},
	{"SetHead",            WM_ACTION_SET_HEAD},
	{"SetDecor",           WM_ACTION_SET_DECOR},
	{"SetEnv",             WM_ACTION_SET_ENV},
	{"SetLayer",           WM_ACTION_SET_LAYER},
	{"SetWorkspaceName",   WM_ACTION_SET_WORKSPACE_NAME},
	{"SetStyle",           WM_ACTION_SET_STYLE},
	{"Shade",              WM_ACTION_SHADE},
	{"ShadeOff",           WM_ACTION_SHADE_OFF},
	{"ShadeOn",            WM_ACTION_SHADE_ON},
	{"ShadeWindow",        WM_ACTION_SHADE},
	{"ShowDesktop",        WM_ACTION_SHOW_DESKTOP},
	{"StartMoving",        WM_ACTION_START_MOVING},
	{"StartResizing",      WM_ACTION_START_RESIZING},
	{"StartTabbing",       WM_ACTION_START_TABBING},
	{"Stick",              WM_ACTION_STICK},
	{"StickWindow",        WM_ACTION_STICK},
	{"TakeToNextWorkspace", WM_ACTION_TAKE_TO_NEXT_WORKSPACE},
	{"TakeToPrevWorkspace", WM_ACTION_TAKE_TO_PREV_WORKSPACE},
	{"TakeToWorkspace",    WM_ACTION_TAKE_TO_WORKSPACE},
	{"ToggleCmd",          WM_ACTION_TOGGLE_CMD},
	{"Unclutter",          WM_ACTION_UNCLUTTER},
	{"ToggleDecor",        WM_ACTION_TOGGLE_DECOR},
	{"ToggleSlitAbove",    WM_ACTION_TOGGLE_SLIT_ABOVE},
	{"ToggleSlitHidden",   WM_ACTION_TOGGLE_SLIT_HIDDEN},
	{"ToggleToolbarAbove", WM_ACTION_TOGGLE_TOOLBAR_ABOVE},
	{"ToggleToolbarVisible", WM_ACTION_TOGGLE_TOOLBAR_VISIBLE},
	{"ToggleShowPosition", WM_ACTION_TOGGLE_SHOW_POSITION},
	{"WindowList",         WM_ACTION_WINDOW_LIST},
	{"WindowMenu",         WM_ACTION_WINDOW_MENU},
	{"WorkspaceMenu",      WM_ACTION_WORKSPACE_MENU},
	{"Workspace",          WM_ACTION_WORKSPACE},
};

static enum wm_action
parse_action_name(const char *name)
{
	for (size_t i = 0; i < sizeof(actions) / sizeof(actions[0]); i++) {
		if (strcasecmp(actions[i].name, name) == 0)
			return actions[i].action;
	}
	return WM_ACTION_NOP;
}

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
 * Parse a key specification segment (modifiers + key name) from a token
 * array. Consumes tokens starting at *pos, sets modifiers and keysym.
 * Returns true if a valid key was found, advances *pos past consumed tokens.
 */
static bool
parse_key_segment(char **tokens, int token_count, int *pos,
		  uint32_t *out_mods, xkb_keysym_t *out_keysym)
{
	uint32_t mods = 0;
	int i = *pos;

	/* Consume modifiers until we find a non-modifier (the key) */
	while (i < token_count) {
		if (!parse_modifier(tokens[i], &mods))
			break;
		i++;
	}

	if (i >= token_count)
		return false;

	xkb_keysym_t sym = xkb_keysym_from_name(tokens[i],
		XKB_KEYSYM_CASE_INSENSITIVE);
	if (sym == XKB_KEY_NoSymbol)
		return false;

	*out_mods = mods;
	*out_keysym = sym;
	*pos = i + 1;
	return true;
}

/*
 * Free a sub-command list.
 */
static void
free_subcmds(struct wm_subcmd *head)
{
	while (head) {
		struct wm_subcmd *next = head->next;
		free(head->argument);
		free(head);
		head = next;
	}
}

/*
 * Free a keybind node and all its children recursively.
 */
static void
keybind_free(struct wm_keybind *bind)
{
	/* Free children recursively */
	struct wm_keybind *child, *tmp;
	wl_list_for_each_safe(child, tmp, &bind->children, link) {
		wl_list_remove(&child->link);
		keybind_free(child);
	}
	free_subcmds(bind->subcmds);
	free_subcmds(bind->else_cmd);
	wm_condition_destroy(bind->condition);
	free(bind->argument);
	free(bind);
}

/*
 * Create a new keybind node.
 */
static struct wm_keybind *
keybind_create(uint32_t modifiers, xkb_keysym_t keysym)
{
	struct wm_keybind *bind = calloc(1, sizeof(*bind));
	if (!bind)
		return NULL;
	bind->modifiers = modifiers;
	bind->keysym = keysym;
	bind->action = WM_ACTION_NOP;
	wl_list_init(&bind->children);
	return bind;
}

/*
 * Find or create an intermediate chain node in the given list.
 * If a node with the same mod+key exists, return it. Otherwise create one.
 */
static struct wm_keybind *
find_or_create_chain_node(struct wl_list *list, uint32_t mods,
			  xkb_keysym_t keysym)
{
	struct wm_keybind *bind;
	wl_list_for_each(bind, list, link) {
		if (bind->modifiers == mods && bind->keysym == keysym)
			return bind;
	}

	/* Not found — create new intermediate node */
	bind = keybind_create(mods, keysym);
	if (!bind)
		return NULL;
	wl_list_insert(list->prev, &bind->link);
	return bind;
}

/*
 * Parse sub-commands from a MacroCmd or ToggleCmd argument string.
 * Format: {cmd1} {cmd2} ...
 * Each {block} contains an action and optional argument.
 */
static struct wm_subcmd *
parse_subcmds(const char *str, int *count)
{
	struct wm_subcmd *head = NULL;
	struct wm_subcmd **tail = &head;
	int n = 0;

	while (str && *str) {
		/* Find next '{' */
		const char *open = strchr(str, '{');
		if (!open)
			break;

		/* Find matching '}' */
		const char *close = strchr(open + 1, '}');
		if (!close)
			break;

		/* Extract the content between braces */
		size_t len = close - (open + 1);
		char *content = strndup(open + 1, len);
		if (!content)
			break;

		/* Trim whitespace */
		char *p = strip(content);

		/* Parse as action [argument] */
		char action_name[256];
		const char *arg = NULL;

		const char *sp = p;
		while (*sp && !isspace((unsigned char)*sp))
			sp++;

		size_t name_len = sp - p;
		if (name_len >= sizeof(action_name)) {
			free(content);
			str = close + 1;
			continue;
		}
		memcpy(action_name, p, name_len);
		action_name[name_len] = '\0';

		if (*sp) {
			arg = sp;
			while (isspace((unsigned char)*arg))
				arg++;
			if (*arg == '\0')
				arg = NULL;
		}

		enum wm_action action = parse_action_name(action_name);
		if (action != WM_ACTION_NOP) {
			struct wm_subcmd *cmd = calloc(1, sizeof(*cmd));
			if (cmd) {
				cmd->action = action;
				if (arg)
					cmd->argument = strdup(arg);
				*tail = cmd;
				tail = &cmd->next;
				n++;
			}
		}

		free(content);
		str = close + 1;
	}

	if (count)
		*count = n;
	return head;
}

/*
 * Recursively free a condition tree.
 */
void
wm_condition_destroy(struct wm_condition *cond)
{
	if (!cond)
		return;
	free(cond->property);
	free(cond->pattern);
	wm_condition_destroy(cond->child);
	wm_condition_destroy(cond->left);
	wm_condition_destroy(cond->right);
	free(cond);
}

/*
 * Skip whitespace in a string pointer.
 */
static void
skip_ws(const char **str)
{
	while (**str && isspace((unsigned char)**str))
		(*str)++;
}

/*
 * Extract content between matching '{' and '}' braces, handling nesting.
 * Advances *str past the closing '}'.
 * Returns malloc'd string of inner content, or NULL on error.
 */
static char *
extract_brace_block(const char **str)
{
	skip_ws(str);
	if (**str != '{')
		return NULL;
	(*str)++; /* skip opening '{' */

	int depth = 1;
	const char *start = *str;
	while (**str && depth > 0) {
		if (**str == '{')
			depth++;
		else if (**str == '}')
			depth--;
		if (depth > 0)
			(*str)++;
	}
	if (depth != 0)
		return NULL;

	size_t len = *str - start;
	(*str)++; /* skip closing '}' */

	char *content = strndup(start, len);
	return content;
}

/*
 * Recursive descent parser for condition expressions.
 * Syntax:
 *   Matches (property=pattern)
 *   Some (property=pattern)
 *   Every (property=pattern)
 *   Not {condition}
 *   And {condition} {condition}
 *   Or {condition} {condition}
 *   Xor {condition} {condition}
 */
static struct wm_condition *
parse_condition(const char **str)
{
	skip_ws(str);
	if (**str == '\0')
		return NULL;

	/* Read keyword */
	const char *start = *str;
	while (**str && !isspace((unsigned char)**str) &&
	       **str != '(' && **str != '{')
		(*str)++;

	size_t kw_len = *str - start;
	if (kw_len == 0)
		return NULL;

	char keyword[64];
	if (kw_len >= sizeof(keyword))
		return NULL;
	memcpy(keyword, start, kw_len);
	keyword[kw_len] = '\0';

	struct wm_condition *cond = calloc(1, sizeof(*cond));
	if (!cond)
		return NULL;

	if (strcasecmp(keyword, "Matches") == 0 ||
	    strcasecmp(keyword, "Some") == 0 ||
	    strcasecmp(keyword, "Every") == 0) {
		if (strcasecmp(keyword, "Matches") == 0)
			cond->type = WM_COND_MATCHES;
		else if (strcasecmp(keyword, "Some") == 0)
			cond->type = WM_COND_SOME;
		else
			cond->type = WM_COND_EVERY;

		/* Parse (property=pattern) */
		skip_ws(str);
		if (**str != '(') {
			wlr_log(WLR_ERROR,
				"condition '%s': expected '('", keyword);
			free(cond);
			return NULL;
		}
		(*str)++; /* skip '(' */

		const char *pstart = *str;
		while (**str && **str != ')')
			(*str)++;
		if (**str != ')') {
			wlr_log(WLR_ERROR,
				"condition '%s': missing ')'", keyword);
			free(cond);
			return NULL;
		}

		size_t plen = *str - pstart;
		(*str)++; /* skip ')' */

		char *inner = strndup(pstart, plen);
		if (!inner) {
			free(cond);
			return NULL;
		}

		/* Split on first '=' */
		char *eq = strchr(inner, '=');
		if (!eq) {
			wlr_log(WLR_ERROR,
				"condition '%s': missing '=' in property spec",
				keyword);
			free(inner);
			free(cond);
			return NULL;
		}
		*eq = '\0';

		/* Trim whitespace from property and pattern */
		char *prop = inner;
		while (isspace((unsigned char)*prop))
			prop++;
		char *pend = eq - 1;
		while (pend > prop && isspace((unsigned char)*pend))
			*pend-- = '\0';

		char *pat = eq + 1;
		while (isspace((unsigned char)*pat))
			pat++;
		char *patend = pat + strlen(pat) - 1;
		while (patend > pat && isspace((unsigned char)*patend))
			*patend-- = '\0';

		cond->property = strdup(prop);
		cond->pattern = strdup(pat);
		free(inner);

		if (!cond->property || !cond->pattern) {
			wm_condition_destroy(cond);
			return NULL;
		}
	} else if (strcasecmp(keyword, "Not") == 0) {
		cond->type = WM_COND_NOT;
		char *block = extract_brace_block(str);
		if (!block) {
			wlr_log(WLR_ERROR,
				"%s", "Not condition: missing {block}");
			free(cond);
			return NULL;
		}
		const char *bp = block;
		cond->child = parse_condition(&bp);
		free(block);
		if (!cond->child) {
			free(cond);
			return NULL;
		}
	} else if (strcasecmp(keyword, "And") == 0 ||
		   strcasecmp(keyword, "Or") == 0 ||
		   strcasecmp(keyword, "Xor") == 0) {
		if (strcasecmp(keyword, "And") == 0)
			cond->type = WM_COND_AND;
		else if (strcasecmp(keyword, "Or") == 0)
			cond->type = WM_COND_OR;
		else
			cond->type = WM_COND_XOR;

		char *left_block = extract_brace_block(str);
		if (!left_block) {
			wlr_log(WLR_ERROR,
				"%s condition: missing first {block}",
				keyword);
			free(cond);
			return NULL;
		}
		char *right_block = extract_brace_block(str);
		if (!right_block) {
			wlr_log(WLR_ERROR,
				"%s condition: missing second {block}",
				keyword);
			free(left_block);
			free(cond);
			return NULL;
		}

		const char *lp = left_block;
		cond->left = parse_condition(&lp);
		free(left_block);

		const char *rp = right_block;
		cond->right = parse_condition(&rp);
		free(right_block);

		if (!cond->left || !cond->right) {
			wm_condition_destroy(cond);
			return NULL;
		}
	} else {
		wlr_log(WLR_ERROR, "unknown condition keyword: %s", keyword);
		free(cond);
		return NULL;
	}

	return cond;
}

/*
 * Parse a single subcmd from a brace block content string.
 * Format: "ActionName argument..."
 * Returns a single wm_subcmd node or NULL.
 */
static struct wm_subcmd *
parse_single_subcmd(const char *content)
{
	while (isspace((unsigned char)*content))
		content++;
	if (*content == '\0')
		return NULL;

	/* Extract action name */
	const char *sp = content;
	while (*sp && !isspace((unsigned char)*sp))
		sp++;

	char action_name[256];
	size_t name_len = sp - content;
	if (name_len >= sizeof(action_name))
		return NULL;
	memcpy(action_name, content, name_len);
	action_name[name_len] = '\0';

	const char *arg = sp;
	while (isspace((unsigned char)*arg))
		arg++;
	if (*arg == '\0')
		arg = NULL;

	enum wm_action action = parse_action_name(action_name);
	if (action == WM_ACTION_NOP)
		return NULL;

	struct wm_subcmd *cmd = calloc(1, sizeof(*cmd));
	if (!cmd)
		return NULL;
	cmd->action = action;
	if (arg)
		cmd->argument = strdup(arg);
	return cmd;
}

/*
 * Parse If/ForEach/Map/Delay arguments and populate the keybind.
 * Returns true on success.
 */
static bool
parse_conditional_args(enum wm_action action, const char *argument,
		       struct wm_keybind *bind)
{
	if (!argument || !*argument)
		return false;

	const char *p = argument;

	if (action == WM_ACTION_IF) {
		/* Format: {condition} {then_cmd} {else_cmd} */
		char *cond_block = extract_brace_block(&p);
		if (!cond_block) {
			wlr_log(WLR_ERROR,
				"%s", "If: missing condition block");
			return false;
		}
		const char *cp = cond_block;
		bind->condition = parse_condition(&cp);
		free(cond_block);
		if (!bind->condition)
			return false;

		/* Then command */
		char *then_block = extract_brace_block(&p);
		if (!then_block) {
			wlr_log(WLR_ERROR,
				"%s", "If: missing then block");
			return false;
		}
		bind->subcmds = parse_single_subcmd(then_block);
		free(then_block);
		if (bind->subcmds)
			bind->subcmd_count = 1;

		/* Optional else command */
		skip_ws(&p);
		if (*p == '{') {
			char *else_block = extract_brace_block(&p);
			if (else_block) {
				bind->else_cmd =
					parse_single_subcmd(else_block);
				free(else_block);
			}
		}
		return true;
	}

	if (action == WM_ACTION_FOREACH) {
		/* Format: {condition} {action arg} */
		char *cond_block = extract_brace_block(&p);
		if (!cond_block) {
			wlr_log(WLR_ERROR,
				"%s", "ForEach: missing condition block");
			return false;
		}
		const char *cp = cond_block;
		bind->condition = parse_condition(&cp);
		free(cond_block);
		if (!bind->condition)
			return false;

		char *cmd_block = extract_brace_block(&p);
		if (!cmd_block) {
			wlr_log(WLR_ERROR,
				"%s", "ForEach: missing command block");
			return false;
		}
		bind->subcmds = parse_single_subcmd(cmd_block);
		free(cmd_block);
		if (bind->subcmds)
			bind->subcmd_count = 1;
		return true;
	}

	if (action == WM_ACTION_MAP) {
		/* Format: {action arg} */
		char *cmd_block = extract_brace_block(&p);
		if (!cmd_block) {
			wlr_log(WLR_ERROR,
				"%s", "Map: missing command block");
			return false;
		}
		bind->subcmds = parse_single_subcmd(cmd_block);
		free(cmd_block);
		if (bind->subcmds)
			bind->subcmd_count = 1;
		return true;
	}

	if (action == WM_ACTION_DELAY) {
		/* Format: {action arg} microseconds */
		char *cmd_block = extract_brace_block(&p);
		if (!cmd_block) {
			wlr_log(WLR_ERROR,
				"%s", "Delay: missing command block");
			return false;
		}
		bind->subcmds = parse_single_subcmd(cmd_block);
		free(cmd_block);
		if (bind->subcmds)
			bind->subcmd_count = 1;

		/* Parse delay value */
		skip_ws(&p);
		if (*p) {
			char *end;
			errno = 0;
			long val = strtol(p, &end, 10);
			if (!errno && end != p && val > 0 && val <= INT_MAX)
				bind->delay_us = (int)val;
		}
		if (bind->delay_us <= 0)
			bind->delay_us = 1000000; /* default 1 second */
		return true;
	}

	return false;
}

/*
 * Parse the action part of a keybinding line (everything after the last ':').
 * Sets action, argument, and subcmds on the binding.
 * Returns true on success.
 */
static bool
parse_action_part(const char *action_str, struct wm_keybind *bind)
{
	while (isspace((unsigned char)*action_str))
		action_str++;

	if (*action_str == '\0')
		return false;

	/* Extract action name */
	char action_name[256];
	const char *space = action_str;
	while (*space && !isspace((unsigned char)*space))
		space++;

	size_t name_len = space - action_str;
	if (name_len >= sizeof(action_name))
		return false;
	memcpy(action_name, action_str, name_len);
	action_name[name_len] = '\0';

	enum wm_action action = parse_action_name(action_name);
	if (action == WM_ACTION_NOP)
		return false;

	bind->action = action;

	/* Everything after the action name is the argument */
	const char *argument = space;
	while (isspace((unsigned char)*argument))
		argument++;
	if (*argument == '\0')
		argument = NULL;

	if (action == WM_ACTION_MACRO_CMD || action == WM_ACTION_TOGGLE_CMD) {
		if (argument) {
			/* Parse sub-commands from argument */
			bind->subcmds = parse_subcmds(argument,
				&bind->subcmd_count);
			/* Store raw argument too for reference */
			bind->argument = strdup(argument);
		}
	} else if (action == WM_ACTION_IF || action == WM_ACTION_FOREACH ||
		   action == WM_ACTION_MAP || action == WM_ACTION_DELAY) {
		if (argument) {
			if (!parse_conditional_args(action, argument, bind))
				return false;
			bind->argument = strdup(argument);
		}
	} else {
		if (argument)
			bind->argument = strdup(argument);
	}

	return true;
}

/*
 * Parse a single keybinding line, potentially with chains.
 * Format: [Modifier ...] key [Modifier ...] key ... :Action [argument]
 *
 * Inserts the binding into the appropriate position in the binding tree.
 * For chains, creates intermediate nodes as needed.
 */
static bool
parse_keybind_line(const char *line, struct wl_list *bindings)
{
	/* Find the last ':' that separates key spec from action.
	 * For MacroCmd lines, we need the first ':' before the action name,
	 * not colons inside the argument. The action colon is the first one
	 * preceded by a space or the line's key spec. */
	const char *colon = strchr(line, ':');
	if (!colon)
		return false;

	/* Extract the key specification part (before ':') */
	size_t key_part_len = colon - line;
	char *key_part = strndup(line, key_part_len);
	if (!key_part)
		return false;

	/* The action part is everything after the ':' */
	const char *action_str = colon + 1;

	/* Tokenize the key part into modifier+key segments */
	char *saveptr;
	char *tokens[64];
	int token_count = 0;

	char *tok = strtok_r(key_part, " \t", &saveptr);
	while (tok && token_count < 64) {
		tokens[token_count++] = tok;
		tok = strtok_r(NULL, " \t", &saveptr);
	}

	if (token_count == 0) {
		free(key_part);
		return false;
	}

	/* Parse key segments. Each segment is [modifiers...] keyname.
	 * Multiple segments = chain. */
	struct {
		uint32_t mods;
		xkb_keysym_t keysym;
	} segments[32];
	int seg_count = 0;
	int pos = 0;

	while (pos < token_count && seg_count < 32) {
		uint32_t mods;
		xkb_keysym_t keysym;
		if (!parse_key_segment(tokens, token_count, &pos,
				       &mods, &keysym)) {
			free(key_part);
			return false;
		}
		segments[seg_count].mods = mods;
		segments[seg_count].keysym = keysym;
		seg_count++;
	}

	if (seg_count == 0) {
		free(key_part);
		return false;
	}

	/* Build chain: navigate/create intermediate nodes for all but last */
	struct wl_list *current_list = bindings;
	for (int i = 0; i < seg_count - 1; i++) {
		struct wm_keybind *node = find_or_create_chain_node(
			current_list, segments[i].mods, segments[i].keysym);
		if (!node) {
			free(key_part);
			return false;
		}
		current_list = &node->children;
	}

	/* Create the leaf binding */
	struct wm_keybind *bind = keybind_create(
		segments[seg_count - 1].mods,
		segments[seg_count - 1].keysym);
	if (!bind) {
		free(key_part);
		return false;
	}

	if (!parse_action_part(action_str, bind)) {
		keybind_free(bind);
		free(key_part);
		return false;
	}

	wl_list_insert(current_list->prev, &bind->link);
	free(key_part);
	return true;
}

/*
 * Check if a line starts with a keymode prefix like "modename: ..."
 * Returns the mode name (caller must free) and sets *rest to the
 * remainder of the line after the prefix. Returns NULL if not a mode line.
 */
static char *
parse_mode_prefix(const char *line, const char **rest)
{
	const char *p = line;

	/* Mode name: sequence of non-whitespace chars ending with ':' */
	while (*p && !isspace((unsigned char)*p) && *p != ':')
		p++;

	if (*p != ':' || p == line)
		return NULL;

	/* Check that there's a second ':' (for the action).
	 * Otherwise this could be just a regular "key :Action" line. */
	const char *second_colon = strchr(p + 1, ':');
	if (!second_colon)
		return NULL;

	/* It's a mode prefix */
	char *mode_name = strndup(line, p - line);
	if (!mode_name)
		return NULL;

	/* Skip past the prefix colon and whitespace */
	p++;
	while (isspace((unsigned char)*p))
		p++;

	*rest = p;
	return mode_name;
}

struct wm_keymode *
keybind_get_mode(struct wl_list *keymodes, const char *name)
{
	struct wm_keymode *mode;
	wl_list_for_each(mode, keymodes, link) {
		if (strcasecmp(mode->name, name) == 0)
			return mode;
	}

	/* Create new mode */
	mode = calloc(1, sizeof(*mode));
	if (!mode)
		return NULL;
	mode->name = strdup(name);
	if (!mode->name) {
		free(mode);
		return NULL;
	}
	wl_list_init(&mode->bindings);
	wl_list_insert(keymodes->prev, &mode->link);
	return mode;
}

int
keybind_load(struct wl_list *keymodes, const char *path)
{
	FILE *f = fopen_nofollow(path, "r");
	if (!f)
		return -1;

	/* Ensure "default" mode exists */
	struct wm_keymode *default_mode = keybind_get_mode(keymodes,
		"default");
	if (!default_mode) {
		fclose(f);
		return -1;
	}

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

		/* Skip empty lines and comments */
		if (*p == '\0' || *p == '#' || *p == '!')
			continue;

		/* Skip mouse bindings (OnDesktop, OnTitlebar, etc.) */
		if (strncasecmp(p, "On", 2) == 0)
			continue;

		/* Check for keymode prefix */
		const char *rest = NULL;
		char *mode_name = parse_mode_prefix(p, &rest);
		if (mode_name) {
			struct wm_keymode *mode = keybind_get_mode(keymodes,
				mode_name);
			free(mode_name);
			if (mode) {
				parse_keybind_line(rest, &mode->bindings);
			}
			continue;
		}

		/* Regular binding or chain — goes into default mode */
		parse_keybind_line(p, &default_mode->bindings);
	}

	fclose(f);
	return 0;
}

struct wm_keybind *
keybind_find(struct wl_list *bindings, uint32_t modifiers,
	     xkb_keysym_t keysym)
{
	struct wm_keybind *bind;
	wl_list_for_each(bind, bindings, link) {
		if (bind->modifiers == modifiers && bind->keysym == keysym)
			return bind;
	}
	return NULL;
}

/*
 * Recursively destroy all bindings in a list.
 */
void
keybind_destroy_list(struct wl_list *bindings)
{
	struct wm_keybind *bind, *tmp;
	wl_list_for_each_safe(bind, tmp, bindings, link) {
		wl_list_remove(&bind->link);
		keybind_free(bind);
	}
}

void
keybind_destroy_all(struct wl_list *keymodes)
{
	struct wm_keymode *mode, *tmp;
	wl_list_for_each_safe(mode, tmp, keymodes, link) {
		keybind_destroy_list(&mode->bindings);
		wl_list_remove(&mode->link);
		free(mode->name);
		free(mode);
	}
}

bool
keybind_add_from_string(struct wl_list *bindings, const char *line)
{
	return parse_keybind_line(line, bindings);
}

static int
action_name_cmp(const void *a, const void *b)
{
	return strcmp(*(const char *const *)a, *(const char *const *)b);
}

void
wm_keybind_list_actions(void)
{
	size_t n = sizeof(actions) / sizeof(actions[0]);
	const char **names = malloc(n * sizeof(*names));
	if (!names)
		return;

	for (size_t i = 0; i < n; i++)
		names[i] = actions[i].name;

	qsort(names, n, sizeof(*names), action_name_cmp);

	for (size_t i = 0; i < n; i++)
		printf("%s\n", names[i]);

	free(names);
}
