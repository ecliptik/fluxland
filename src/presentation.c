/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * presentation.c - wp-presentation-time protocol support
 *
 * Provides frame timing feedback to clients via the wp_presentation
 * interface. With wlroots 0.18 scene graph, presentation feedback is
 * handled automatically by wlr_scene_surface — the compositor only
 * needs to create the wlr_presentation global.
 */

#define _POSIX_C_SOURCE 200809L
#include <wlr/types/wlr_presentation_time.h>
#include <wlr/util/log.h>

#include "presentation.h"
#include "server.h"

void
wm_presentation_init(struct wm_server *server)
{
	server->presentation = wlr_presentation_create(
		server->wl_display, server->backend);
	if (!server->presentation) {
		wlr_log(WLR_ERROR, "%s", "failed to create wp-presentation-time");
		return;
	}
	wlr_log(WLR_INFO, "%s", "wp-presentation-time protocol initialized");
}
