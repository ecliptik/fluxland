/*
 * wm-wayland - A Fluxbox-inspired Wayland compositor
 * screencopy.h - Screen capture protocol support
 */

#ifndef WM_SCREENCOPY_H
#define WM_SCREENCOPY_H

struct wm_server;

/* Initialize screencopy and DMA-BUF export protocols */
void wm_screencopy_init(struct wm_server *server);

#endif /* WM_SCREENCOPY_H */
