/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * presentation.h - wp-presentation-time protocol support
 */

#ifndef WM_PRESENTATION_H
#define WM_PRESENTATION_H

struct wm_server;

/* Initialize wp-presentation-time protocol */
void wm_presentation_init(struct wm_server *server);

#endif /* WM_PRESENTATION_H */
