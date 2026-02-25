/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * focus_nav.c - Keyboard-only focus navigation between UI zones
 *
 * Manages a state machine for navigating between windows, toolbar
 * elements, and menu items using only the keyboard. Emits IPC
 * events for screen reader integration.
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <wlr/util/log.h>

#include "focus_nav.h"
#include "ipc.h"
#include "server.h"
#include "toolbar.h"
#include "workspace.h"
#include "view.h"

/* Escape a string for safe inclusion in JSON (writes into dst). */
static void
json_escape_buf(char *dst, size_t dst_size, const char *src)
{
	if (!src) {
		if (dst_size > 0) dst[0] = '\0';
		return;
	}
	size_t j = 0;
	for (size_t i = 0; src[i] && j + 6 < dst_size; i++) {
		unsigned char c = (unsigned char)src[i];
		if (c == '"' || c == '\\') {
			dst[j++] = '\\';
			dst[j++] = c;
		} else if (c < 0x20) {
			continue;
		} else {
			dst[j++] = c;
		}
	}
	dst[j] = '\0';
}

void
wm_focus_nav_init(struct wm_focus_nav *nav)
{
	nav->zone = WM_FOCUS_ZONE_WINDOWS;
	nav->toolbar_index = -1;
}

void
wm_focus_nav_enter_toolbar(struct wm_server *server)
{
	struct wm_toolbar *toolbar = server->toolbar;
	if (!toolbar || !toolbar->visible || toolbar->tool_count == 0) {
		return;
	}

	server->focus_nav.zone = WM_FOCUS_ZONE_TOOLBAR;
	server->focus_nav.toolbar_index = 0;

	/* Redraw toolbar with focus indicator */
	wm_toolbar_update_iconbar(toolbar);

	/* Broadcast focus_changed event for screen readers */
	struct wm_toolbar_tool *tool = &toolbar->tools[0];
	const char *name = "toolbar";
	switch (tool->type) {
	case WM_TOOL_PREV_WORKSPACE: name = "Previous Workspace"; break;
	case WM_TOOL_NEXT_WORKSPACE: name = "Next Workspace"; break;
	case WM_TOOL_WORKSPACE_NAME: name = "Workspace Name"; break;
	case WM_TOOL_ICONBAR: name = "Icon Bar"; break;
	case WM_TOOL_CLOCK: name = "Clock"; break;
	case WM_TOOL_PREV_WINDOW: name = "Previous Window"; break;
	case WM_TOOL_NEXT_WINDOW: name = "Next Window"; break;
	}

	char buf[512];
	snprintf(buf, sizeof(buf),
		"{\"event\":\"focus_changed\","
		"\"target\":\"toolbar\","
		"\"element\":\"%s\","
		"\"index\":%d}",
		name, 0);
	wm_ipc_broadcast_event(&server->ipc,
		WM_IPC_EVENT_FOCUS_CHANGED, buf);

	wlr_log(WLR_DEBUG, "focus_nav: entered toolbar, element=%s", name);
}

void
wm_focus_nav_return_to_windows(struct wm_server *server)
{
	if (server->focus_nav.zone == WM_FOCUS_ZONE_WINDOWS) {
		return;
	}

	server->focus_nav.zone = WM_FOCUS_ZONE_WINDOWS;
	server->focus_nav.toolbar_index = -1;

	/* Redraw toolbar without focus indicator */
	if (server->toolbar) {
		wm_toolbar_update_iconbar(server->toolbar);
	}

	/* Re-announce the focused window for screen readers */
	if (server->focused_view) {
		char esc_app[256], esc_title[256], buf[1024];
		json_escape_buf(esc_app, sizeof(esc_app),
			server->focused_view->app_id);
		json_escape_buf(esc_title, sizeof(esc_title),
			server->focused_view->title);

		snprintf(buf, sizeof(buf),
			"{\"event\":\"focus_changed\","
			"\"target\":\"window\","
			"\"id\":%u,"
			"\"app_id\":\"%s\","
			"\"title\":\"%s\"}",
			server->focused_view->id,
			esc_app, esc_title);
		wm_ipc_broadcast_event(&server->ipc,
			WM_IPC_EVENT_FOCUS_CHANGED, buf);
	}

	wlr_log(WLR_DEBUG, "focus_nav: returned to windows");
}

static const char *
tool_type_name(enum wm_toolbar_tool_type type)
{
	switch (type) {
	case WM_TOOL_PREV_WORKSPACE: return "Previous Workspace";
	case WM_TOOL_NEXT_WORKSPACE: return "Next Workspace";
	case WM_TOOL_WORKSPACE_NAME: return "Workspace Name";
	case WM_TOOL_ICONBAR: return "Icon Bar";
	case WM_TOOL_CLOCK: return "Clock";
	case WM_TOOL_PREV_WINDOW: return "Previous Window";
	case WM_TOOL_NEXT_WINDOW: return "Next Window";
	}
	return "toolbar";
}

void
wm_focus_nav_next_element(struct wm_server *server)
{
	if (server->focus_nav.zone != WM_FOCUS_ZONE_TOOLBAR) {
		return;
	}

	struct wm_toolbar *toolbar = server->toolbar;
	if (!toolbar || toolbar->tool_count == 0) {
		return;
	}

	int next = server->focus_nav.toolbar_index + 1;
	if (next >= toolbar->tool_count) {
		next = 0; /* wrap around */
	}
	server->focus_nav.toolbar_index = next;

	/* Redraw toolbar with updated focus indicator */
	wm_toolbar_update_iconbar(toolbar);

	/* Broadcast for screen readers */
	struct wm_toolbar_tool *tool = &toolbar->tools[next];
	const char *name = tool_type_name(tool->type);

	char buf[512];
	snprintf(buf, sizeof(buf),
		"{\"event\":\"focus_changed\","
		"\"target\":\"toolbar\","
		"\"element\":\"%s\","
		"\"index\":%d}",
		name, next);
	wm_ipc_broadcast_event(&server->ipc,
		WM_IPC_EVENT_FOCUS_CHANGED, buf);

	wlr_log(WLR_DEBUG, "focus_nav: next element=%s idx=%d", name, next);
}

void
wm_focus_nav_prev_element(struct wm_server *server)
{
	if (server->focus_nav.zone != WM_FOCUS_ZONE_TOOLBAR) {
		return;
	}

	struct wm_toolbar *toolbar = server->toolbar;
	if (!toolbar || toolbar->tool_count == 0) {
		return;
	}

	int prev = server->focus_nav.toolbar_index - 1;
	if (prev < 0) {
		prev = toolbar->tool_count - 1; /* wrap around */
	}
	server->focus_nav.toolbar_index = prev;

	/* Redraw toolbar with updated focus indicator */
	wm_toolbar_update_iconbar(toolbar);

	/* Broadcast for screen readers */
	struct wm_toolbar_tool *tool = &toolbar->tools[prev];
	const char *name = tool_type_name(tool->type);

	char buf[512];
	snprintf(buf, sizeof(buf),
		"{\"event\":\"focus_changed\","
		"\"target\":\"toolbar\","
		"\"element\":\"%s\","
		"\"index\":%d}",
		name, prev);
	wm_ipc_broadcast_event(&server->ipc,
		WM_IPC_EVENT_FOCUS_CHANGED, buf);

	wlr_log(WLR_DEBUG, "focus_nav: prev element=%s idx=%d", name, prev);
}

void
wm_focus_nav_activate(struct wm_server *server)
{
	if (server->focus_nav.zone != WM_FOCUS_ZONE_TOOLBAR) {
		return;
	}

	struct wm_toolbar *toolbar = server->toolbar;
	if (!toolbar || toolbar->tool_count == 0) {
		return;
	}

	int idx = server->focus_nav.toolbar_index;
	if (idx < 0 || idx >= toolbar->tool_count) {
		return;
	}

	struct wm_toolbar_tool *tool = &toolbar->tools[idx];
	wlr_log(WLR_DEBUG, "focus_nav: activate tool type=%d", tool->type);

	switch (tool->type) {
	case WM_TOOL_PREV_WORKSPACE:
		wm_workspace_switch_prev(server);
		break;
	case WM_TOOL_NEXT_WORKSPACE:
		wm_workspace_switch_next(server);
		break;
	case WM_TOOL_WORKSPACE_NAME:
		/* Activate workspace name → show workspace menu */
		wm_workspace_switch_next(server);
		break;
	case WM_TOOL_ICONBAR:
		/* Activate iconbar → focus the first window or cycle */
		if (toolbar->ib_count > 0 && toolbar->ib_entries) {
			struct wm_view *view = toolbar->ib_entries[0].view;
			if (view) {
				wm_focus_view(view,
					view->xdg_toplevel->base->surface);
				/* Return to window zone */
				wm_focus_nav_return_to_windows(server);
			}
		}
		break;
	case WM_TOOL_PREV_WINDOW:
		wm_focus_prev_view(server);
		wm_focus_nav_return_to_windows(server);
		break;
	case WM_TOOL_NEXT_WINDOW:
		wm_focus_next_view(server);
		wm_focus_nav_return_to_windows(server);
		break;
	case WM_TOOL_CLOCK:
		/* No action for clock */
		break;
	}
}

int
wm_focus_nav_get_toolbar_index(struct wm_server *server)
{
	if (server->focus_nav.zone != WM_FOCUS_ZONE_TOOLBAR) {
		return -1;
	}
	return server->focus_nav.toolbar_index;
}
