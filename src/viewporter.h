/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * viewporter.h - Viewporter and single-pixel-buffer protocol support
 */

#ifndef WM_VIEWPORTER_H
#define WM_VIEWPORTER_H

struct wm_server;

/* Initialize viewporter and single-pixel-buffer protocols */
void wm_viewporter_init(struct wm_server *server);

#endif /* WM_VIEWPORTER_H */
