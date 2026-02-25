/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * drm_lease.h - wp-drm-lease-v1 protocol support
 */

#ifndef WM_DRM_LEASE_H
#define WM_DRM_LEASE_H

struct wm_server;
struct wlr_output;

/* Initialize the DRM lease protocol */
void wm_drm_lease_init(struct wm_server *server);

/* Clean up DRM lease resources */
void wm_drm_lease_finish(struct wm_server *server);

/* Offer an output for DRM leasing (call when new outputs appear) */
void wm_drm_lease_offer_output(struct wm_server *server,
	struct wlr_output *output);

#endif /* WM_DRM_LEASE_H */
