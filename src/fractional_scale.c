/*
 * wm-wayland - A Fluxbox-inspired Wayland compositor
 * fractional_scale.c - wp-fractional-scale-v1 protocol support
 *
 * Provides fractional scale information to Wayland clients so they can
 * render at the correct scale for HiDPI displays (e.g. 1.25x, 1.5x, 2x).
 * The scene graph notifies surfaces of scale changes via output events;
 * we hook those to call wlr_fractional_scale_v1_notify_scale().
 */

#define _POSIX_C_SOURCE 200809L
#include <wlr/types/wlr_fractional_scale_v1.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/log.h>

#include "fractional_scale.h"
#include "server.h"

void
wm_fractional_scale_init(struct wm_server *server)
{
	server->fractional_scale_mgr =
		wlr_fractional_scale_manager_v1_create(server->wl_display, 1);
	if (!server->fractional_scale_mgr) {
		wlr_log(WLR_ERROR, "%s",
			"failed to create fractional scale manager");
		return;
	}

	wlr_log(WLR_INFO, "%s",
		"initialized wp-fractional-scale-v1 protocol");
}
