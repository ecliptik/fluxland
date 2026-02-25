/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * drm_syncobj.h - linux-drm-syncobj-v1 explicit synchronization support
 */

#ifndef WM_DRM_SYNCOBJ_H
#define WM_DRM_SYNCOBJ_H

struct wm_server;

/* Initialize explicit sync protocol (may be no-op if unsupported) */
void wm_drm_syncobj_init(struct wm_server *server);

/* Clean up explicit sync resources */
void wm_drm_syncobj_finish(struct wm_server *server);

#endif /* WM_DRM_SYNCOBJ_H */
