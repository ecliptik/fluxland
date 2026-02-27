/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * validate.c - Configuration validation
 *
 * Validates init, keys, and style config files offline, reporting
 * errors and warnings without requiring compositor startup.
 */

#define _POSIX_C_SOURCE 200809L

#include "validate.h"
#include "i18n.h"
#include "rcparser.h"
#include "util.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#define INITIAL_CAPACITY 16
#define LINE_MAX_LEN 1024

/* ---- Result helpers ---- */

void
config_result_init(struct config_result *result)
{
	result->errors = NULL;
	result->count = 0;
	result->capacity = 0;
}

static void
result_add(struct config_result *result, const char *file, int line,
	   bool fatal, const char *fmt, va_list ap)
{
	/* Cap maximum number of collected errors to prevent unbounded growth */
	if (result->count >= 1000)
		return;

	if (result->count >= result->capacity) {
		int new_cap = result->capacity ? result->capacity * 2
			: INITIAL_CAPACITY;
		if (new_cap > 1000)
			new_cap = 1000;
		struct config_error *new_errs = realloc(result->errors,
			(size_t)new_cap * sizeof(struct config_error));
		if (!new_errs)
			return;
		result->errors = new_errs;
		result->capacity = new_cap;
	}

	char *msg = NULL;
	va_list ap2;
	va_copy(ap2, ap);
	int len = vsnprintf(NULL, 0, fmt, ap2);
	va_end(ap2);
	if (len >= 0) {
		msg = malloc((size_t)len + 1);
		if (msg)
			vsnprintf(msg, (size_t)len + 1, fmt, ap);
	}

	struct config_error *e = &result->errors[result->count++];
	e->line = line;
	e->file = file ? strdup(file) : NULL;
	e->message = msg;
	e->fatal = fatal;
}

void
config_result_add_error(struct config_result *result,
	const char *file, int line, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	result_add(result, file, line, true, fmt, ap);
	va_end(ap);
}

void
config_result_add_warning(struct config_result *result,
	const char *file, int line, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	result_add(result, file, line, false, fmt, ap);
	va_end(ap);
}

void
config_result_destroy(struct config_result *result)
{
	for (int i = 0; i < result->count; i++) {
		free(result->errors[i].file);
		free(result->errors[i].message);
	}
	free(result->errors);
	result->errors = NULL;
	result->count = 0;
	result->capacity = 0;
}

bool
config_result_has_errors(const struct config_result *result)
{
	for (int i = 0; i < result->count; i++) {
		if (result->errors[i].fatal)
			return true;
	}
	return false;
}

void
config_result_print(const struct config_result *result)
{
	for (int i = 0; i < result->count; i++) {
		const struct config_error *e = &result->errors[i];
		const char *prefix = e->fatal ? "error" : "warning";
		if (e->line > 0)
			fprintf(stderr, "%s:%d: %s: %s\n",
				e->file, e->line, prefix,
				e->message ? e->message : "(unknown)");
		else
			fprintf(stderr, "%s: %s: %s\n",
				e->file, prefix,
				e->message ? e->message : "(unknown)");
	}
}

/* ---- Utility ---- */

static bool
file_exists(const char *path)
{
	struct stat st;
	return stat(path, &st) == 0;
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

/* ---- Init file validation ---- */

/* Valid focus model values */
static const char *valid_focus_models[] = {
	"MouseFocus", "ClickToFocus", "ClickFocus",
	"StrictMouseFocus", NULL
};

/* Valid placement policy values */
static const char *valid_placements[] = {
	"RowSmartPlacement", "ColSmartPlacement", "CascadePlacement",
	"UnderMousePlacement", "RowMinOverlapPlacement",
	"ColMinOverlapPlacement", NULL
};

/* Valid toolbar placement values */
static const char *valid_toolbar_placements[] = {
	"TopLeft", "TopCenter", "TopRight",
	"BottomLeft", "BottomCenter", "BottomRight", NULL
};

/* Valid iconbar mode values */
static const char *valid_iconbar_modes[] = {
	"Workspace", "AllWindows", "Icons", "NoIcons",
	"WorkspaceIcons", NULL
};

/* Valid slit placement values */
static const char *valid_slit_placements[] = {
	"TopLeft", "TopCenter", "TopRight",
	"RightTop", "RightCenter", "RightBottom",
	"BottomLeft", "BottomCenter", "BottomRight",
	"LeftTop", "LeftCenter", "LeftBottom", NULL
};

/* Valid slit direction values */
static const char *valid_slit_directions[] = {
	"Vertical", "Horizontal", NULL
};

/* Valid slit layer values */
static const char *valid_slit_layers[] = {
	"AboveDock", "Above", "Dock", "Normal", "Bottom", NULL
};

/* Valid menu search values */
static const char *valid_menu_search[] = {
	"nowhere", "itemstart", "somewhere", NULL
};

static bool
is_valid_enum(const char *value, const char **valid_values)
{
	for (int i = 0; valid_values[i]; i++) {
		if (strcasecmp(value, valid_values[i]) == 0)
			return true;
	}
	return false;
}

/* Check if a string is a valid integer */
static bool
is_valid_int(const char *s)
{
	if (!s || !*s)
		return false;
	if (*s == '-' || *s == '+')
		s++;
	if (!*s)
		return false;
	while (*s) {
		if (!isdigit((unsigned char)*s))
			return false;
		s++;
	}
	return true;
}

/* Check if a string is a valid boolean */
static bool
is_valid_bool(const char *s)
{
	return strcasecmp(s, "true") == 0 || strcasecmp(s, "false") == 0 ||
	       strcmp(s, "1") == 0 || strcmp(s, "0") == 0 ||
	       strcasecmp(s, "yes") == 0 || strcasecmp(s, "no") == 0;
}

struct init_key_spec {
	const char *key;
	enum { VT_INT, VT_BOOL, VT_ENUM, VT_STRING, VT_PATH } type;
	int int_min;
	int int_max;
	const char **valid_values; /* for VT_ENUM */
};

static const struct init_key_spec init_keys[] = {
	/* Workspaces */
	{"session.screen0.workspaces", VT_INT, 1, 32, NULL},
	{"session.screen0.workspacewarping", VT_BOOL, 0, 0, NULL},
	{"session.screen0.workspaceNames", VT_STRING, 0, 0, NULL},

	/* Focus */
	{"session.screen0.focusModel", VT_ENUM, 0, 0, valid_focus_models},
	{"session.screen0.autoRaise", VT_BOOL, 0, 0, NULL},
	{"session.autoRaiseDelay", VT_INT, 0, 10000, NULL},
	{"session.screen0.clickRaises", VT_BOOL, 0, 0, NULL},
	{"session.screen0.focusNewWindows", VT_BOOL, 0, 0, NULL},

	/* Window behavior */
	{"session.screen0.opaqueMove", VT_BOOL, 0, 0, NULL},
	{"session.screen0.opaqueResize", VT_BOOL, 0, 0, NULL},
	{"session.screen0.edgeSnapThreshold", VT_INT, 0, 1000, NULL},
	{"session.screen0.fullMaximization", VT_BOOL, 0, 0, NULL},
	{"session.screen0.showWindowPosition", VT_BOOL, 0, 0, NULL},

	/* Placement */
	{"session.screen0.windowPlacement", VT_ENUM, 0, 0, valid_placements},
	{"session.screen0.rowPlacementDirection", VT_STRING, 0, 0, NULL},
	{"session.screen0.colPlacementDirection", VT_STRING, 0, 0, NULL},

	/* Toolbar */
	{"session.screen0.toolbar.visible", VT_BOOL, 0, 0, NULL},
	{"session.screen0.toolbar.placement", VT_ENUM, 0, 0, valid_toolbar_placements},
	{"session.screen0.toolbar.autoHide", VT_BOOL, 0, 0, NULL},
	{"session.screen0.toolbar.autoHideDelay", VT_INT, 0, 10000, NULL},
	{"session.screen0.toolbar.widthPercent", VT_INT, 10, 100, NULL},
	{"session.screen0.toolbar.height", VT_INT, 0, 200, NULL},
	{"session.screen0.toolbar.layer", VT_INT, 0, 10, NULL},
	{"session.screen0.toolbar.alpha", VT_INT, 0, 255, NULL},
	{"session.screen0.toolbar.onhead", VT_INT, 0, 32, NULL},
	{"session.screen0.toolbar.maxOver", VT_BOOL, 0, 0, NULL},
	{"session.screen0.toolbar.tools", VT_STRING, 0, 0, NULL},

	/* Titlebar */
	{"session.titlebar.left", VT_STRING, 0, 0, NULL},
	{"session.titlebar.right", VT_STRING, 0, 0, NULL},

	/* Clock */
	{"session.screen0.strftimeFormat", VT_STRING, 0, 0, NULL},

	/* Iconbar */
	{"session.screen0.iconbar.mode", VT_ENUM, 0, 0, valid_iconbar_modes},
	{"session.screen0.iconbar.alignment", VT_STRING, 0, 0, NULL},
	{"session.screen0.iconbar.iconWidth", VT_INT, 0, 10000, NULL},
	{"session.screen0.iconbar.usePixmap", VT_BOOL, 0, 0, NULL},
	{"session.screen0.iconbar.wheelMode", VT_STRING, 0, 0, NULL},
	{"session.screen0.iconbar.iconifiedPattern", VT_STRING, 0, 0, NULL},

	/* Tabs */
	{"session.screen0.autoTabPlacement", VT_BOOL, 0, 0, NULL},
	{"session.screen0.tabFocusModel", VT_STRING, 0, 0, NULL},
	{"session.screen0.tabs.intitlebar", VT_BOOL, 0, 0, NULL},
	{"session.screen0.tab.placement", VT_STRING, 0, 0, NULL},
	{"session.screen0.tab.width", VT_INT, 0, 10000, NULL},
	{"session.screen0.tabPadding", VT_INT, 0, 1000, NULL},

	/* Window opacity */
	{"session.screen0.window.focus.alpha", VT_INT, 0, 255, NULL},
	{"session.screen0.window.unfocus.alpha", VT_INT, 0, 255, NULL},

	/* Slit */
	{"session.screen0.slit.autoHide", VT_BOOL, 0, 0, NULL},
	{"session.screen0.slit.placement", VT_ENUM, 0, 0, valid_slit_placements},
	{"session.screen0.slit.direction", VT_ENUM, 0, 0, valid_slit_directions},
	{"session.screen0.slit.layer", VT_ENUM, 0, 0, valid_slit_layers},
	{"session.screen0.slit.onhead", VT_INT, 0, 32, NULL},
	{"session.screen0.slit.alpha", VT_INT, 0, 255, NULL},
	{"session.screen0.slit.maxOver", VT_BOOL, 0, 0, NULL},

	/* Double-click */
	{"session.doubleClickInterval", VT_INT, 50, 2000, NULL},

	/* Menu */
	{"session.screen0.menu.alpha", VT_INT, 0, 255, NULL},
	{"session.menuSearch", VT_ENUM, 0, 0, valid_menu_search},
	{"session.screen0.menuDelay", VT_INT, 0, 5000, NULL},
	{"session.screen0.windowMenu", VT_STRING, 0, 0, NULL},

	/* Root command */
	{"session.screen0.rootCommand", VT_STRING, 0, 0, NULL},

	/* Edge resize */
	{"session.screen0.edgeResizeSnapThreshold", VT_INT, 0, 1000, NULL},

	/* Decoration */
	{"session.screen0.defaultDeco", VT_STRING, 0, 0, NULL},
	{"session.screen0.struts", VT_STRING, 0, 0, NULL},

	/* File paths */
	{"session.keyFile", VT_PATH, 0, 0, NULL},
	{"session.appsFile", VT_PATH, 0, 0, NULL},
	{"session.menuFile", VT_PATH, 0, 0, NULL},
	{"session.styleFile", VT_PATH, 0, 0, NULL},
	{"session.styleOverlay", VT_PATH, 0, 0, NULL},
	{"session.slitlistFile", VT_PATH, 0, 0, NULL},

	/* XKB */
	{"session.xkb.rules", VT_STRING, 0, 0, NULL},
	{"session.xkb.model", VT_STRING, 0, 0, NULL},
	{"session.xkb.layout", VT_STRING, 0, 0, NULL},
	{"session.xkb.variant", VT_STRING, 0, 0, NULL},
	{"session.xkb.options", VT_STRING, 0, 0, NULL},

	{NULL, VT_STRING, 0, 0, NULL},
};

static const struct init_key_spec *
find_init_key(const char *key)
{
	for (int i = 0; init_keys[i].key; i++) {
		if (strcmp(init_keys[i].key, key) == 0)
			return &init_keys[i];
	}
	return NULL;
}

/* Resolve a config path value (handles ~ expansion) for existence check */
static char *
resolve_check_path(const char *config_dir, const char *value)
{
	if (!value)
		return NULL;
	if (value[0] == '/')
		return strdup(value);
	if (value[0] == '~') {
		const char *home = getenv("HOME");
		if (home) {
			size_t len = strlen(home) + strlen(value);
			char *buf = malloc(len + 1);
			if (buf)
				snprintf(buf, len + 1, "%s%s",
					home, value + 1);
			return buf;
		}
	}
	if (config_dir) {
		size_t len = strlen(config_dir) + 1 + strlen(value) + 1;
		char *buf = malloc(len);
		if (buf)
			snprintf(buf, len, "%s/%s", config_dir, value);
		return buf;
	}
	return strdup(value);
}

void
validate_init_file(const char *path, struct config_result *result)
{
	FILE *f = fopen(path, "r");
	if (!f) {
		config_result_add_error(result, path, 0,
			"cannot open file");
		return;
	}

	/* Derive config_dir from path (parent directory) */
	char *path_copy = strdup(path);
	char *config_dir = NULL;
	if (path_copy) {
		char *slash = strrchr(path_copy, '/');
		if (slash) {
			*slash = '\0';
			config_dir = path_copy;
		}
	}

	char line[LINE_MAX_LEN];
	int lineno = 0;
	while (fgets(line, sizeof(line), f)) {
		lineno++;

		/* Skip long lines */
		size_t len = strlen(line);
		if (len > 0 && line[len - 1] != '\n' && !feof(f)) {
			int ch;
			while ((ch = fgetc(f)) != EOF && ch != '\n')
				;
			config_result_add_warning(result, path, lineno,
				"line exceeds maximum length");
			continue;
		}

		char *p = strip(line);
		if (*p == '\0' || *p == '#' || *p == '!')
			continue;

		char *colon = strchr(p, ':');
		if (!colon) {
			config_result_add_warning(result, path, lineno,
				"missing ':' separator");
			continue;
		}

		*colon = '\0';
		char *key = strip(p);
		char *value = strip(colon + 1);

		if (*key == '\0')
			continue;

		const struct init_key_spec *spec = find_init_key(key);
		if (!spec) {
			config_result_add_warning(result, path, lineno,
				"unknown key '%s'", key);
			continue;
		}

		if (*value == '\0') {
			config_result_add_warning(result, path, lineno,
				"empty value for '%s'", key);
			continue;
		}

		switch (spec->type) {
		case VT_INT:
			if (!is_valid_int(value)) {
				config_result_add_error(result, path, lineno,
					"'%s' expects integer, got '%s'",
					spec->key, value);
			} else {
				long v = strtol(value, NULL, 10);
				if (v < spec->int_min || v > spec->int_max)
					config_result_add_warning(result,
						path, lineno,
						"'%s' value %ld out of range "
						"[%d, %d]",
						spec->key, v,
						spec->int_min, spec->int_max);
			}
			break;

		case VT_BOOL:
			if (!is_valid_bool(value))
				config_result_add_error(result, path, lineno,
					"'%s' expects boolean, got '%s'",
					spec->key, value);
			break;

		case VT_ENUM:
			if (!is_valid_enum(value, spec->valid_values))
				config_result_add_error(result, path, lineno,
					"'%s' invalid value '%s'",
					spec->key, value);
			break;

		case VT_PATH: {
			char *resolved = resolve_check_path(config_dir,
				value);
			if (resolved) {
				if (!file_exists(resolved))
					config_result_add_warning(result,
						path, lineno,
						"'%s' references "
						"non-existent path '%s'",
						spec->key, resolved);
				free(resolved);
			}
			break;
		}

		case VT_STRING:
			/* No specific validation for strings */
			break;
		}
	}

	free(path_copy);
	fclose(f);
}

/* ---- Keys file validation ---- */

/* Known action names (same as keybind.c actions table) */
static const char *valid_actions[] = {
	"ActivateTab", "AddWorkspace", "ArrangeWindows", "BindKey",
	"ArrangeWindowsHorizontal", "ArrangeWindowsStackBottom",
	"ArrangeWindowsStackLeft", "ArrangeWindowsStackRight",
	"ArrangeWindowsStackTop", "ArrangeWindowsVertical",
	"CascadeWindows", "Close", "ClientMenu", "CloseAllWindows",
	"CustomMenu", "Deiconify", "Delay", "DetachClient",
	"Exec", "ExecCommand", "Execute", "Exit", "Export",
	"Focus", "FocusActivate", "FocusDown", "FocusLeft", "ForEach",
	"FocusNext", "FocusNextElement", "FocusPrev", "FocusPrevElement",
	"FocusRight", "FocusToolbar", "FocusUp",
	"Fullscreen", "GotoWindow", "HideMenus",
	"Iconify", "If", "KeyMode", "Kill", "KillWindow",
	"LeftWorkspace", "LHalf", "Lower", "LowerLayer",
	"MacroCmd", "Map", "Maximize", "MaximizeHorizontal",
	"MaximizeVertical", "Minimize", "Move", "MoveDown",
	"MoveLeft", "MoveRight", "MoveTabLeft", "MoveTabRight",
	"MoveTo", "MoveUp", "NextGroup", "NextLayout", "NextTab",
	"NextWindow", "NextWorkspace",
	"PrevGroup", "PrevLayout", "PrevTab", "PrevWindow",
	"PrevWorkspace", "Quit", "RHalf", "Raise", "RaiseLayer",
	"Remember", "RootMenu", "Reconfigure", "ReloadStyle",
	"RemoveLastWorkspace", "Resize", "ResizeHorizontal",
	"ResizeTo", "ResizeVertical", "RightWorkspace",
	"SetAlpha", "SendToNextHead", "SendToNextWorkspace",
	"SendToPrevHead", "SendToPrevWorkspace", "SendToWorkspace",
	"SetHead", "SetDecor", "SetEnv", "SetLayer",
	"SetWorkspaceName", "SetStyle", "Shade", "ShadeOff",
	"ShadeOn", "ShadeWindow", "ShowDesktop",
	"StartMoving", "StartResizing", "StartTabbing",
	"Stick", "StickWindow",
	"TakeToNextWorkspace", "TakeToPrevWorkspace", "TakeToWorkspace",
	"ToggleCmd", "Unclutter", "ToggleDecor",
	"ToggleSlitAbove", "ToggleSlitHidden",
	"ToggleToolbarAbove", "ToggleToolbarVisible",
	"ToggleShowPosition", "WindowList", "WindowMenu",
	"WorkspaceMenu", "Workspace",
	NULL
};

static bool
is_valid_action(const char *name)
{
	for (int i = 0; valid_actions[i]; i++) {
		if (strcasecmp(valid_actions[i], name) == 0)
			return true;
	}
	return false;
}

/* Valid modifier names */
static const char *valid_modifiers[] = {
	"Mod1", "Mod4", "Control", "Ctrl", "Shift", "None", NULL
};

static bool
is_valid_modifier(const char *name)
{
	for (int i = 0; valid_modifiers[i]; i++) {
		if (strcasecmp(valid_modifiers[i], name) == 0)
			return true;
	}
	return false;
}

/* Valid mouse context names */
static const char *valid_contexts[] = {
	"OnDesktop", "OnToolbar", "OnTitlebar", "OnTab",
	"OnWindow", "OnWindowBorder", "OnLeftGrip", "OnRightGrip",
	"OnSlit", NULL
};

static bool
is_valid_context(const char *name)
{
	for (int i = 0; valid_contexts[i]; i++) {
		if (strcasecmp(valid_contexts[i], name) == 0)
			return true;
	}
	return false;
}

/* Check if token is a mouse event like Mouse1, Click3, etc. */
static bool
is_mouse_event(const char *token)
{
	const char *prefixes[] = {"Mouse", "Click", "Double", "Move", NULL};
	for (int i = 0; prefixes[i]; i++) {
		size_t plen = strlen(prefixes[i]);
		if (strncasecmp(token, prefixes[i], plen) == 0) {
			const char *num = token + plen;
			int n;
			if (*num && safe_atoi(num, &n))
				return n >= 1 && n <= 5;
		}
	}
	return false;
}

/* Check if token is a mouse event with bad button number */
static bool
is_mouse_event_bad_button(const char *token)
{
	const char *prefixes[] = {"Mouse", "Click", "Double", "Move", NULL};
	for (int i = 0; prefixes[i]; i++) {
		size_t plen = strlen(prefixes[i]);
		if (strncasecmp(token, prefixes[i], plen) == 0) {
			const char *num = token + plen;
			int n;
			if (*num && safe_atoi(num, &n)) {
				if (n < 1 || n > 5)
					return true;
			}
		}
	}
	return false;
}

void
validate_keys_file(const char *path, struct config_result *result)
{
	FILE *f = fopen(path, "r");
	if (!f) {
		config_result_add_warning(result, path, 0,
			"cannot open keys file");
		return;
	}

	char line[LINE_MAX_LEN];
	int lineno = 0;
	while (fgets(line, sizeof(line), f)) {
		lineno++;

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

		/* Find the action separator ':'.
		 * Handle keymode prefix lines like "mode: Mod4 x :Action"
		 * where the first ':' is the mode separator and the
		 * second ':' is the action separator. */
		char *colon = strchr(p, ':');
		if (!colon) {
			config_result_add_error(result, path, lineno,
				"missing ':' before action");
			continue;
		}

		/* Check for keymode prefix: if there's a second ':',
		 * and the first token before the first ':' has no
		 * spaces (mode name), use the second colon. */
		char *second_colon = strchr(colon + 1, ':');
		if (second_colon) {
			/* Check if text before first colon is a single
			 * word (mode name) */
			const char *pre = p;
			while (pre < colon &&
			       !isspace((unsigned char)*pre))
				pre++;
			if (pre == colon) {
				/* Single word before first colon = mode
				 * prefix; skip to second colon */
				colon = second_colon;
			}
		}

		/* Extract action part */
		char *action_str = colon + 1;
		while (isspace((unsigned char)*action_str))
			action_str++;

		if (*action_str == '\0') {
			config_result_add_error(result, path, lineno,
				"empty action after ':'");
			continue;
		}

		/* Extract action name (first word) */
		char action_name[256];
		const char *sp = action_str;
		while (*sp && !isspace((unsigned char)*sp))
			sp++;
		size_t name_len = (size_t)(sp - action_str);
		if (name_len >= sizeof(action_name))
			name_len = sizeof(action_name) - 1;
		memcpy(action_name, action_str, name_len);
		action_name[name_len] = '\0';

		if (!is_valid_action(action_name)) {
			config_result_add_error(result, path, lineno,
				"unknown action '%s'", action_name);
		}

		/* Validate key part tokens */
		size_t key_part_len = (size_t)(colon - p);
		char *key_part = strndup(p, key_part_len);
		if (!key_part)
			continue;

		char *saveptr;
		char *tok = strtok_r(key_part, " \t", &saveptr);
		while (tok) {
			/* Skip known contexts */
			if (is_valid_context(tok) || is_valid_modifier(tok) ||
			    is_mouse_event(tok)) {
				tok = strtok_r(NULL, " \t", &saveptr);
				continue;
			}

			/* Check for bad button numbers */
			if (is_mouse_event_bad_button(tok)) {
				config_result_add_error(result, path, lineno,
					"invalid button number in '%s' "
					"(must be 1-5)", tok);
				tok = strtok_r(NULL, " \t", &saveptr);
				continue;
			}

			/* Otherwise it's presumably a key name;
			 * we can't fully validate without XKB, so skip */
			tok = strtok_r(NULL, " \t", &saveptr);
		}
		free(key_part);
	}

	fclose(f);
}

/* ---- Style file validation ---- */

static bool
is_valid_color(const char *value)
{
	if (!value || *value == '\0')
		return false;

	/* Skip leading whitespace */
	while (isspace((unsigned char)*value))
		value++;

	/* #RRGGBB or #RGB */
	if (*value == '#') {
		value++;
		size_t len = 0;
		while (isxdigit((unsigned char)value[len]))
			len++;
		/* Rest should be whitespace or null */
		const char *rest = value + len;
		while (isspace((unsigned char)*rest))
			rest++;
		if (*rest != '\0')
			return false;
		return len == 6 || len == 3;
	}

	/* rgb:R/G/B format */
	if (strncasecmp(value, "rgb:", 4) == 0)
		return true; /* basic check */

	/* Named colors */
	const char *named[] = {
		"white", "black", "red", "green", "blue",
		"grey", "gray", NULL
	};
	for (int i = 0; named[i]; i++) {
		if (strcasecmp(value, named[i]) == 0)
			return true;
	}

	return false;
}

static bool
is_valid_font_string(const char *value)
{
	/* Minimal check: non-empty, has family name */
	if (!value || *value == '\0')
		return false;
	/* A font string should have at least an alphabetic character
	 * (the family name) */
	while (*value) {
		if (isalpha((unsigned char)*value))
			return true;
		value++;
	}
	return false;
}

void
validate_style_file(const char *path, struct config_result *result)
{
	FILE *f = fopen(path, "r");
	if (!f) {
		config_result_add_warning(result, path, 0,
			"cannot open style file");
		return;
	}

	char line[LINE_MAX_LEN];
	int lineno = 0;
	while (fgets(line, sizeof(line), f)) {
		lineno++;

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

		char *colon = strchr(p, ':');
		if (!colon)
			continue;

		*colon = '\0';
		char *key = strip(p);
		char *value = strip(colon + 1);

		if (*key == '\0' || *value == '\0')
			continue;

		/* Check color values */
		size_t klen = strlen(key);
		bool is_color_key = false;
		if (klen > 5 &&
		    (strcasecmp(key + klen - 5, "Color") == 0 ||
		     strcasecmp(key + klen - 5, "color") == 0))
			is_color_key = true;
		if (klen > 7 &&
		    (strcasecmp(key + klen - 7, "colorTo") == 0 ||
		     strcasecmp(key + klen - 7, "ColorTo") == 0))
			is_color_key = true;

		if (is_color_key && !is_valid_color(value))
			config_result_add_error(result, path, lineno,
				"invalid color format '%s' for '%s' "
				"(expected #RRGGBB, #RGB, rgb:R/G/B, "
				"or named color)", value, key);

		/* Check font values */
		if (klen > 4 &&
		    strcasecmp(key + klen - 4, "font") == 0 &&
		    !is_valid_font_string(value))
			config_result_add_error(result, path, lineno,
				"invalid font format '%s' for '%s' "
				"(expected 'family-size[:style]')",
				value, key);

		/* Check integer properties */
		if (strcasecmp(key, "window.borderWidth") == 0 ||
		    strcasecmp(key, "window.bevelWidth") == 0 ||
		    strcasecmp(key, "window.handleWidth") == 0 ||
		    strcasecmp(key, "window.title.height") == 0 ||
		    strcasecmp(key, "menu.borderWidth") == 0 ||
		    strcasecmp(key, "menu.itemHeight") == 0 ||
		    strcasecmp(key, "menu.titleHeight") == 0 ||
		    strcasecmp(key, "slit.borderWidth") == 0) {
			if (!is_valid_int(value))
				config_result_add_error(result, path, lineno,
					"'%s' expects integer, got '%s'",
					key, value);
			else {
				long v = strtol(value, NULL, 10);
				if (v < 0)
					config_result_add_warning(result,
						path, lineno,
						"'%s' negative value %ld",
						key, v);
			}
		}
	}

	fclose(f);
}

/* ---- Top-level validation ---- */

int
validate_config(const char *config_dir)
{
	struct config_result result;
	config_result_init(&result);

	char path[4096];

	/* Validate init file */
	snprintf(path, sizeof(path), "%s/init", config_dir);
	if (file_exists(path)) {
		fprintf(stderr, "Checking %s ...\n", path);
		validate_init_file(path, &result);
	} else {
		fprintf(stderr, "No init file found at %s "
			"(defaults will be used)\n", path);
	}

	/* Validate keys file */
	snprintf(path, sizeof(path), "%s/keys", config_dir);
	if (file_exists(path)) {
		fprintf(stderr, "Checking %s ...\n", path);
		validate_keys_file(path, &result);
	}

	/* Validate style file */
	snprintf(path, sizeof(path), "%s/style", config_dir);
	if (file_exists(path)) {
		fprintf(stderr, "Checking %s ...\n", path);
		validate_style_file(path, &result);
	}
	/* Also check styles/default */
	snprintf(path, sizeof(path), "%s/styles/default", config_dir);
	if (file_exists(path)) {
		fprintf(stderr, "Checking %s ...\n", path);
		validate_style_file(path, &result);
	}

	/* Print results */
	if (result.count == 0) {
		fprintf(stderr, "Configuration OK: no issues found.\n");
		config_result_destroy(&result);
		return 0;
	}

	config_result_print(&result);

	int errors = 0, warnings = 0;
	for (int i = 0; i < result.count; i++) {
		if (result.errors[i].fatal)
			errors++;
		else
			warnings++;
	}

	fprintf(stderr, "\n%d error(s), %d warning(s)\n", errors, warnings);

	bool has_errors = config_result_has_errors(&result);
	config_result_destroy(&result);
	return has_errors ? 1 : 0;
}
