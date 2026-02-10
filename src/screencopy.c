/*
 * wm-wayland - A Fluxbox-inspired Wayland compositor
 * screencopy.c - Screen capture protocol support
 *
 * Enables screenshot tools (grim) and screen recorders by providing:
 *   - wlr-screencopy-unstable-v1: SHM-based screen capture
 *   - wlr-export-dmabuf-unstable-v1: DMA-BUF based screen capture
 *
 * wlroots handles all protocol details internally; we just create the
 * manager objects.
 */

#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_export_dmabuf_v1.h>
#include <wlr/util/log.h>

#include "server.h"
#include "screencopy.h"

void
wm_screencopy_init(struct wm_server *server)
{
	struct wlr_screencopy_manager_v1 *screencopy =
		wlr_screencopy_manager_v1_create(server->wl_display);
	if (!screencopy) {
		wlr_log(WLR_ERROR, "failed to create screencopy manager");
		return;
	}
	wlr_log(WLR_INFO, "screencopy protocol enabled");

	struct wlr_export_dmabuf_manager_v1 *dmabuf_export =
		wlr_export_dmabuf_manager_v1_create(server->wl_display);
	if (!dmabuf_export) {
		wlr_log(WLR_ERROR, "failed to create DMA-BUF export manager");
		return;
	}
	wlr_log(WLR_INFO, "DMA-BUF export protocol enabled");
}
