/*
 * wm-wayland - A Fluxbox-inspired Wayland compositor
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
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wlr/types/wlr_keyboard.h>

#define KEYS_LINE_MAX 1024

struct action_map {
	const char *name;
	enum wm_action action;
};

static const struct action_map actions[] = {
	{"ActivateTab",        WM_ACTION_ACTIVATE_TAB},
	{"Close",              WM_ACTION_CLOSE},
	{"DetachClient",       WM_ACTION_DETACH_CLIENT},
	{"Exec",               WM_ACTION_EXEC},
	{"ExecCommand",        WM_ACTION_EXEC},
	{"Execute",            WM_ACTION_EXEC},
	{"Exit",               WM_ACTION_EXIT},
	{"Focus",              WM_ACTION_FOCUS},
	{"Fullscreen",         WM_ACTION_FULLSCREEN},
	{"HideMenus",         WM_ACTION_HIDE_MENUS},
	{"Iconify",            WM_ACTION_MINIMIZE},
	{"KeyMode",            WM_ACTION_KEY_MODE},
	{"Kill",               WM_ACTION_KILL},
	{"KillWindow",         WM_ACTION_KILL},
	{"Lower",              WM_ACTION_LOWER},
	{"LowerLayer",         WM_ACTION_LOWER_LAYER},
	{"MacroCmd",           WM_ACTION_MACRO_CMD},
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
	{"NextTab",            WM_ACTION_NEXT_TAB},
	{"NextWindow",         WM_ACTION_FOCUS_NEXT},
	{"NextWorkspace",      WM_ACTION_NEXT_WORKSPACE},
	{"PrevTab",            WM_ACTION_PREV_TAB},
	{"PrevWindow",         WM_ACTION_FOCUS_PREV},
	{"PrevWorkspace",      WM_ACTION_PREV_WORKSPACE},
	{"Quit",               WM_ACTION_EXIT},
	{"Raise",              WM_ACTION_RAISE},
	{"RaiseLayer",         WM_ACTION_RAISE_LAYER},
	{"RootMenu",           WM_ACTION_ROOT_MENU},
	{"Reconfigure",        WM_ACTION_RECONFIGURE},
	{"Resize",             WM_ACTION_RESIZE},
	{"ResizeTo",           WM_ACTION_RESIZE_TO},
	{"SendToWorkspace",    WM_ACTION_SEND_TO_WORKSPACE},
	{"SetDecor",           WM_ACTION_SET_DECOR},
	{"SetLayer",           WM_ACTION_SET_LAYER},
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
	{"ToggleCmd",          WM_ACTION_TOGGLE_CMD},
	{"ToggleDecor",        WM_ACTION_TOGGLE_DECOR},
	{"WindowMenu",         WM_ACTION_WINDOW_MENU},
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
		/* Parse sub-commands from argument */
		if (argument) {
			bind->subcmds = parse_subcmds(argument,
				&bind->subcmd_count);
		}
		/* Store raw argument too for reference */
		if (argument)
			bind->argument = strdup(argument);
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
	FILE *f = fopen(path, "r");
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
