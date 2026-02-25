/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * drm_syncobj.c - linux-drm-syncobj-v1 explicit synchronization support
 *
 * Advertises explicit sync support to clients. This allows GPU clients
 * to use DRM sync objects for buffer synchronization instead of relying
 * on implicit sync. Requires both the renderer and backend to support
 * explicit synchronization.
 *
 * The wlroots scene graph handles the actual sync timeline management
 * internally — we just need to create the manager to advertise the
 * protocol to clients.
 */

#define _POSIX_C_SOURCE 200809L
#include <wlr/backend.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_linux_drm_syncobj_v1.h>
#include <wlr/util/log.h>

#include "drm_syncobj.h"
#include "server.h"

void
wm_drm_syncobj_init(struct wm_server *server)
{
	/*
	 * Get the DRM FD from the renderer. Explicit sync requires a
	 * DRM-capable renderer (not pixman).
	 */
	int drm_fd = wlr_renderer_get_drm_fd(server->renderer);
	if (drm_fd < 0) {
		wlr_log(WLR_INFO, "%s",
			"explicit sync: not available "
			"(renderer has no DRM FD)");
		server->syncobj_mgr = NULL;
		return;
	}

	/*
	 * Create the syncobj manager. This advertises the
	 * linux-drm-syncobj-v1 protocol to clients. Version 1 is the
	 * initial version of the protocol.
	 *
	 * The wlroots scene graph will use the sync timelines internally
	 * to coordinate buffer access between the compositor and clients.
	 */
	server->syncobj_mgr = wlr_linux_drm_syncobj_manager_v1_create(
		server->wl_display, 1, drm_fd);
	if (!server->syncobj_mgr) {
		wlr_log(WLR_INFO, "%s",
			"explicit sync: failed to create manager "
			"(kernel may lack syncobj support)");
		return;
	}

	wlr_log(WLR_INFO, "%s",
		"initialized linux-drm-syncobj-v1 protocol "
		"(explicit synchronization)");
}

void
wm_drm_syncobj_finish(struct wm_server *server)
{
	/* Manager is cleaned up by wl_display destruction */
	(void)server;
}
