/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * validate.c - Configuration validation
 *
 * Validates init, keys, style and apps files offline, reporting
 * errors and warnings without requiring compositor startup.
 */

#define _POSIX_C_SOURCE 200809L

#include "config.h"
#include "client_pattern.h"
#include "mousebind.h"
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
/* Fluxbox's, but fluxland can't put the toolbar there yet */
static const char *unsupported_toolbar_placements[] = {
	"LeftTop", "LeftCenter", "LeftBottom",
	"RightTop", "RightCenter", "RightBottom", NULL
};
static const char *unsupported_placements[] = {
	"AutotabPlacement", NULL
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

/* Valid workspace transition values */
static const char *valid_workspace_transitions[] = {
	"None", "Fade", "Slide", NULL
};

/* Valid workspace mode values */
static const char *valid_workspace_modes[] = {
	"global", "per-output", NULL
};

static const char *tabs_attach_areas[] = {
	"Window", "Titlebar", NULL
};

/* Valid menu search values */
static const char *valid_activation_env[] = {
	"auto", "true", "false", "yes", "no", NULL};
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
	const char *key;    /* '*' stands for a part such as a number */
	enum {
		VT_INT, VT_BOOL, VT_ENUM, VT_STRING, VT_PATH,
		VT_LAYER,        /* a Fluxbox layer name or number */
		VT_WINDOW_LIST,  /* {static groups} (pattern), or none */
		VT_IGNORED,      /* Fluxbox's, meaningless on Wayland */
		VT_UNSUPPORTED,  /* Fluxbox's, not in fluxland yet */
	} type;
	int int_min;
	int int_max;
	const char **valid_values; /* for VT_ENUM */
};

static const struct init_key_spec init_keys[] = {
	/* Workspaces */
	{"session.screen0.workspaces", VT_INT, 1, 32, NULL},
	{"session.screen0.workspacewarping", VT_BOOL, 0, 0, NULL},
	{"session.screen0.workspaceNames", VT_STRING, 0, 0, NULL},
	{"session.screen0.workspaceMode", VT_ENUM, 0, 0, valid_workspace_modes},
	{"session.screen0.workspaceTransition", VT_ENUM, 0, 0, valid_workspace_transitions},
	{"session.screen0.workspaceTransitionDuration", VT_INT, 0, 5000, NULL},

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
	{"session.screen0.maxDisableMove", VT_BOOL, 0, 0, NULL},
	{"session.screen0.maxDisableResize", VT_BOOL, 0, 0, NULL},
	{"session.screen0.showWindowPosition", VT_BOOL, 0, 0, NULL},

	/* Placement */
	{"session.screen0.windowPlacement", VT_ENUM, 0, 0, valid_placements},
	{"session.screen0.rowPlacementDirection", VT_STRING, 0, 0, NULL},
	{"session.screen0.colPlacementDirection", VT_STRING, 0, 0, NULL},

	/* Toolbar */
	{"session.screen0.toolbar.visible", VT_BOOL, 0, 0, NULL},
	{"session.screen0.toolbar.placement", VT_ENUM, 0, 0,
		valid_toolbar_placements},
	{"session.screen0.toolbar.autoHide", VT_BOOL, 0, 0, NULL},
	{"session.screen0.toolbar.autoHideDelay", VT_INT, 0, 10000, NULL},
	{"session.screen0.toolbar.widthPercent", VT_INT, 10, 100, NULL},
	{"session.screen0.toolbar.height", VT_INT, 0, 200, NULL},
	{"session.screen0.toolbar.layer", VT_LAYER, 0, 0, NULL},
	{"session.screen0.toolbar.alpha", VT_INT, 0, 255, NULL},
	{"session.screen0.toolbar.onhead", VT_INT, 0, 32, NULL},
	{"session.screen0.toolbar.maxOver", VT_BOOL, 0, 0, NULL},
	{"session.screen0.toolbar.tools", VT_STRING, 0, 0, NULL},

	/* Titlebar */
	{"session.titlebar.left", VT_STRING, 0, 0, NULL},
	{"session.titlebar.right", VT_STRING, 0, 0, NULL},
	{"session.screen0.titlebar.left", VT_STRING, 0, 0, NULL},
	{"session.screen0.titlebar.right", VT_STRING, 0, 0, NULL},

	/* Clock */
	{"session.screen0.strftimeFormat", VT_STRING, 0, 0, NULL},

	/* Iconbar */
	{"session.screen0.iconbar.mode", VT_WINDOW_LIST, 0, 0, NULL},
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
	{"session.tabPadding", VT_INT, 0, 1000, NULL},

	/* Window animations */
	{"session.screen0.animateWindowMap", VT_BOOL, 0, 0, NULL},
	{"session.screen0.animateWindowUnmap", VT_BOOL, 0, 0, NULL},
	{"session.screen0.animateMinimize", VT_BOOL, 0, 0, NULL},
	{"session.screen0.animationDuration", VT_INT, 0, 5000, NULL},

	/* Window snap zones */
	{"session.screen0.enableWindowSnapping", VT_BOOL, 0, 0, NULL},
	{"session.screen0.snapZoneThreshold", VT_INT, 1, 100, NULL},

	/* Window opacity */
	{"session.screen0.window.focus.alpha", VT_INT, 0, 255, NULL},
	{"session.screen0.window.unfocus.alpha", VT_INT, 0, 255, NULL},

	/* Slit */
	{"session.screen0.slit.autoHide", VT_BOOL, 0, 0, NULL},
	{"session.screen0.slit.placement", VT_ENUM, 0, 0, valid_slit_placements},
	{"session.screen0.slit.direction", VT_ENUM, 0, 0, valid_slit_directions},
	{"session.screen0.slit.layer", VT_LAYER, 0, 0, NULL},
	{"session.screen0.slit.onhead", VT_INT, 0, 32, NULL},
	{"session.screen0.slit.alpha", VT_INT, 0, 255, NULL},
	{"session.screen0.slit.maxOver", VT_BOOL, 0, 0, NULL},

	/* Double-click */
	{"session.doubleClickInterval", VT_INT, 50, 2000, NULL},

	/* Menu */
	{"session.screen0.menu.alpha", VT_INT, 0, 255, NULL},
	{"session.menuSearch", VT_ENUM, 0, 0, valid_menu_search},
	{"session.updateActivationEnvironment", VT_ENUM, 0, 0,
		valid_activation_env},
	{"session.switch.lidClose", VT_STRING, 0, 0, NULL},
	{"session.switch.lidOpen", VT_STRING, 0, 0, NULL},
	{"session.switch.tabletModeOn", VT_STRING, 0, 0, NULL},
	{"session.switch.tabletModeOff", VT_STRING, 0, 0, NULL},
	{"session.screen0.menuDelay", VT_INT, 0, 5000, NULL},
	{"session.screen0.windowMenu", VT_STRING, 0, 0, NULL},

	/* Root command */
	{"session.screen0.rootCommand", VT_STRING, 0, 0, NULL},

	/* Edge resize */
	{"session.screen0.edgeResizeSnapThreshold", VT_INT, 0, 1000, NULL},

	/* Decoration */
	{"session.screen0.defaultDeco", VT_STRING, 0, 0, NULL},
	{"session.screen0.struts", VT_STRING, 0, 0, NULL},
	{"session.screen0.struts.*", VT_STRING, 0, 0, NULL},

	/* Wallpapers, mouse buttons, gestures (fluxland) */
	{"session.screen0.wallpaper", VT_PATH, 0, 0, NULL},
	{"session.screen0.wallpaperMode", VT_STRING, 0, 0, NULL},
	{"session.screen0.wallpaperColor", VT_STRING, 0, 0, NULL},
	{"session.screen0.workspace*.wallpaper", VT_PATH, 0, 0, NULL},
	{"session.mouse.button*.map", VT_STRING, 0, 0, NULL},
	{"session.screen0.gestureWorkspaceSwitch", VT_BOOL, 0, 0, NULL},
	{"session.screen0.gestureOverview", VT_BOOL, 0, 0, NULL},
	{"session.screen0.gestureSwipeThreshold", VT_INT, 0, 10000, NULL},
	{"session.screen0.gestureWorkspaceFingers", VT_INT, 2, 5, NULL},

	/* Fluxbox's, with no meaning on Wayland or for fluxland */
	{"session.configVersion", VT_IGNORED, 0, 0, NULL},
	{"session.colorsPerChannel", VT_IGNORED, 0, 0, NULL},
	{"session.cacheLife", VT_IGNORED, 0, 0, NULL},
	{"session.cacheMax", VT_IGNORED, 0, 0, NULL},
	{"session.forcePseudoTransparency", VT_IGNORED, 0, 0, NULL},
	{"session.imageDither", VT_IGNORED, 0, 0, NULL},
	{"session.groupFile", VT_IGNORED, 0, 0, NULL},
	{"session.screen0.allowRemoteActions", VT_IGNORED, 0, 0, NULL},
	{"session.screen0.slit.acceptKdeDockapps", VT_IGNORED, 0, 0, NULL},
	{"session.screen0.imageDither", VT_IGNORED, 0, 0, NULL},
	{"session.screen0.antialias", VT_IGNORED, 0, 0, NULL},
	{"session.screen0.opaqueResizeDelay", VT_IGNORED, 0, 0, NULL},
	{"session.screen0.maxIgnoreIncrement", VT_IGNORED, 0, 0, NULL},

	/* Fluxbox's, not in fluxland yet */
	{"session.screen0.focusSameHead", VT_BOOL, 0, 0, NULL},
	{"session.screen0.noFocusWhileTypingDelay", VT_INT, 0, 100000, NULL},
	{"session.screen0.demandsAttentionTimeout", VT_UNSUPPORTED, 0, 0, NULL},
	{"session.screen0.tooltipDelay", VT_UNSUPPORTED, 0, 0, NULL},
	{"session.screen0.clientMenu.usePixmap", VT_UNSUPPORTED, 0, 0, NULL},
	{"session.screen0.toolbar.autoRaise", VT_BOOL, 0, 0, NULL},
	{"session.screen0.toolbar.button.*", VT_STRING, 0, 0, NULL},
	{"session.screen0.systray.pinLeft", VT_UNSUPPORTED, 0, 0, NULL},
	{"session.screen0.systray.pinRight", VT_UNSUPPORTED, 0, 0, NULL},
	{"session.screen0.slit.autoRaise", VT_BOOL, 0, 0, NULL},
	{"session.screen0.workspacewarpinghorizontal", VT_BOOL, 0, 0, NULL},
	{"session.screen0.workspacewarpingvertical", VT_BOOL, 0, 0, NULL},
	{"session.screen0.workspacewarpinghorizontaloffset", VT_INT, 0, 1000, NULL},
	{"session.screen0.workspacewarpingverticaloffset", VT_INT, 0, 1000, NULL},
	{"session.screen0.iconbar.iconTextPadding", VT_UNSUPPORTED, 0, 0, NULL},
	{"session.screen0.tabs.maxOver", VT_UNSUPPORTED, 0, 0, NULL},
	{"session.screen0.tabs.usePixmap", VT_UNSUPPORTED, 0, 0, NULL},
	{"session.tabsAttachArea", VT_ENUM, 0, 0, tabs_attach_areas},
	{"session.ignoreBorder", VT_BOOL, 0, 0, NULL},

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

/* A key against a spec key, without case; '*' matches any run */
static bool
key_matches(const char *spec, const char *key)
{
	for (; *spec; spec++, key++) {
		if (*spec == '*') {
			for (const char *k = key; ; k++) {
				if (key_matches(spec + 1, k))
					return true;
				if (!*k)
					return false;
			}
		}
		if (tolower((unsigned char)*spec) != tolower((unsigned char)*key))
			return false;
	}
	return *key == '\0';
}

/* VT_ENUM values that are Fluxbox's but not fluxland's yet */
static const struct {
	const char *key;
	const char **values;
} unsupported_enum_values[] = {
	{ "session.screen0.windowPlacement", unsupported_placements },
	{ "session.screen0.toolbar.placement", unsupported_toolbar_placements },
};

static bool
is_unsupported_value(const char *key, const char *value)
{
	size_t n = sizeof(unsupported_enum_values) /
		sizeof(unsupported_enum_values[0]);
	for (size_t i = 0; i < n; i++) {
		if (strcmp(unsupported_enum_values[i].key, key) == 0)
			return is_valid_enum(value,
				unsupported_enum_values[i].values);
	}
	return false;
}

static const struct init_key_spec *
find_init_key(const char *key)
{
	for (int i = 0; init_keys[i].key; i++) {
		if (key_matches(init_keys[i].key, key))
			return &init_keys[i];
	}
	return NULL;
}

/* config.c's parse_dock_layer and fbcompat's names */
static bool
is_valid_layer(const char *value)
{
	static const char *names[] = { "Menu", "AboveDock", "Above", "Dock",
		"Top", "Normal", "Bottom", "Below", "Desktop", NULL };
	return is_valid_int(value) || is_valid_enum(value, names);
}

/* --- session.input.* (libinput) settings --- */

static const char *valid_tap_maps[] = {"lrm", "lmr", NULL};
static const char *valid_accel_profiles[] = {"flat", "adaptive", NULL};
static const char *valid_scroll_methods[] = {
	"none", "twoFinger", "edge", "button", NULL};
static const char *valid_click_methods[] = {
	"none", "buttonAreas", "clickFinger", NULL};
static const char *valid_send_events[] = {
	"enabled", "disabled", "disabledOnExternalMouse", NULL};

/* Settings after session.input[.touchpad|.pointer|.device.<Name>]. */
static const struct init_key_spec input_settings[] = {
	{"tap", VT_BOOL, 0, 0, NULL},
	{"tapDrag", VT_BOOL, 0, 0, NULL},
	{"tapDragLock", VT_BOOL, 0, 0, NULL},
	{"tapButtonMap", VT_ENUM, 0, 0, valid_tap_maps},
	{"naturalScroll", VT_BOOL, 0, 0, NULL},
	{"leftHanded", VT_BOOL, 0, 0, NULL},
	{"disableWhileTyping", VT_BOOL, 0, 0, NULL},
	{"middleEmulation", VT_BOOL, 0, 0, NULL},
	{"accelProfile", VT_ENUM, 0, 0, valid_accel_profiles},
	{"accelSpeed", VT_STRING, 0, 0, NULL}, /* -1.0 .. 1.0, below */
	{"scrollMethod", VT_ENUM, 0, 0, valid_scroll_methods},
	{"scrollButton", VT_INT, 1, 0x2ff, NULL},
	{"clickMethod", VT_ENUM, 0, 0, valid_click_methods},
	{"sendEvents", VT_ENUM, 0, 0, valid_send_events},
};

/*
 * Map session.input.* to its setting spec; NULL if the scope or the
 * setting name is unknown.
 */
static const struct init_key_spec *
find_input_setting(const char *key)
{
	const char *rest = key + strlen("session.input.");
	if (strncmp(rest, "touchpad.", 9) == 0)
		rest += 9;
	else if (strncmp(rest, "pointer.", 8) == 0)
		rest += 8;
	else if (strncmp(rest, "device.", 7) == 0) {
		const char *dot = strrchr(rest + 7, '.');
		if (!dot || dot == rest + 7)
			return NULL;
		rest = dot + 1;
	}
	for (size_t i = 0; i < sizeof(input_settings) /
	     sizeof(input_settings[0]); i++) {
		if (strcasecmp(rest, input_settings[i].key) == 0)
			return &input_settings[i];
	}
	return NULL;
}

/* session.output.<Connector>.<setting> */
static void
validate_output_setting(struct config_result *result, const char *path,
	int lineno, const char *key, const char *value)
{
	const char *name = key + strlen("session.output.");
	const char *setting = strrchr(name, '.');
	if (!setting || setting == name) {
		config_result_add_warning(result, path, lineno,
			"'%s': expected session.output.<Connector>.<setting>",
			key);
		return;
	}
	setting++;

	bool ok = true;
	if (strcasecmp(setting, "enabled") == 0 ||
	    strcasecmp(setting, "adaptiveSync") == 0) {
		ok = is_valid_bool(value);
	} else if (strcasecmp(setting, "mode") == 0) {
		int w, h, mhz;
		ok = config_parse_output_mode(value, &w, &h, &mhz);
	} else if (strcasecmp(setting, "scale") == 0) {
		char *end;
		double v = strtod(value, &end);
		ok = end != value && *end == '\0' && v >= 0.25 && v <= 10.0;
	} else if (strcasecmp(setting, "position") == 0) {
		char *end;
		strtol(value, &end, 10);
		ok = end != value && *end == ',';
		if (ok) {
			const char *p = end + 1;
			strtol(p, &end, 10);
			ok = end != p && *end == '\0';
		}
	} else if (strcasecmp(setting, "transform") == 0) {
		ok = config_parse_output_transform(value) >= 0;
	} else {
		config_result_add_warning(result, path, lineno,
			"unknown key '%s'", key);
		return;
	}
	if (!ok)
		config_result_add_error(result, path, lineno,
			"'%s' invalid value '%s'", key, value);
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

		if (strncmp(key, "session.output.", 15) == 0) {
			validate_output_setting(result, path, lineno, key,
				value);
			continue;
		}

		bool input_key = strncmp(key, "session.input.", 14) == 0;
		const struct init_key_spec *spec = input_key ?
			find_input_setting(key) : find_init_key(key);
		if (input_key && spec && strcasecmp(spec->key, "accelSpeed") == 0) {
			char *end;
			double v = strtod(value, &end);
			if (*value == '\0' || *end != '\0' || v < -1.0 || v > 1.0)
				config_result_add_error(result, path, lineno,
					"'%s' expects a number from -1 to 1, "
					"got '%s'", key, value);
			continue;
		}
		if (!spec && strncasecmp(key, "session.screen", 14) == 0 &&
		    strncasecmp(key, "session.screen0.", 16) != 0) {
			config_result_add_warning(result, path, lineno,
				"'%s': fluxland has one screen (its outputs are "
				"heads): only session.screen0 is read", key);
			continue;
		}
		if (!spec) {
			config_result_add_warning(result, path, lineno,
				"unknown key '%s'", key);
			continue;
		}
		if (spec->type == VT_IGNORED)
			continue;
		if (spec->type == VT_UNSUPPORTED) {
			config_result_add_warning(result, path, lineno,
				"'%s' isn't supported by fluxland yet", key);
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
			if (is_unsupported_value(spec->key, value))
				config_result_add_warning(result, path, lineno,
					"'%s': '%s' isn't supported by fluxland "
					"yet, the default is used",
					spec->key, value);
			else if (!is_valid_enum(value, spec->valid_values))
				config_result_add_error(result, path, lineno,
					"'%s' invalid value '%s'",
					spec->key, value);
			break;

		case VT_LAYER:
			if (!is_valid_layer(value))
				config_result_add_error(result, path, lineno,
					"'%s' expects a layer (Top, Dock, Normal, "
					"Bottom... or a number), got '%s'",
					spec->key, value);
			break;

		case VT_WINDOW_LIST: {
			/* A Fluxbox 1.0 name (Workspace...), or a list */
			static const char *names[] = { "Workspace",
				"AllWindows", "Icons", "NoIcons",
				"WorkspaceIcons", "WorkspaceNoIcons", "none",
				NULL };
			struct wm_window_list_spec list;
			if (is_valid_enum(value, names))
				break;
			if (!wm_window_list_parse(value, &list))
				config_result_add_error(result, path, lineno,
					"'%s' is not a window list such as "
					"{static groups} (workspace): '%s'",
					spec->key, value);
			wm_window_list_finish(&list);
			break;
		}

		case VT_IGNORED:
		case VT_UNSUPPORTED:
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

/*
 * Each line is checked by the parsers that load it (keybind.c and
 * mousebind.c), so what passes here is what fluxland binds.
 */
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
			config_result_add_error(result, path, lineno,
				"line longer than %d characters is ignored",
				LINE_MAX_LEN - 1);
			continue;
		}

		char why[256];
		if (!wm_keys_check_line(line, why, sizeof(why)))
			config_result_add_error(result, path, lineno,
				"%s", why);
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

/* A parser's problem, as a result */
static void
report_apps(void *data, const char *path, int line, bool fatal,
	const char *msg)
{
	if (fatal)
		config_result_add_error(data, path, line, "%s", msg);
	else
		config_result_add_warning(data, path, line, "%s", msg);
}

void
validate_apps_file(const char *path, struct config_result *result)
{
	if (!wm_rules_check_file(path, report_apps, result))
		config_result_add_warning(result, path, 0,
			"cannot open apps file");
}

void
validate_menu_file(const char *path, struct config_result *result)
{
	if (!wm_menu_check_file(path, report_apps, result))
		config_result_add_warning(result, path, 0,
			"cannot open menu file");
}

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

	/* Validate apps file */
	snprintf(path, sizeof(path), "%s/apps", config_dir);
	if (file_exists(path)) {
		fprintf(stderr, "Checking %s ...\n", path);
		validate_apps_file(path, &result);
	}

	/* Validate menu files */
	const char *menus[] = { "menu", "windowmenu" };
	for (int i = 0; i < 2; i++) {
		snprintf(path, sizeof(path), "%s/%s", config_dir, menus[i]);
		if (file_exists(path)) {
			fprintf(stderr, "Checking %s ...\n", path);
			validate_menu_file(path, &result);
		}
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
