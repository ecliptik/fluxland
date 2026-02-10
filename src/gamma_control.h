/*
 * wm-wayland - A Fluxbox-inspired Wayland compositor
 * gamma_control.h - Gamma control protocol for color temperature tools
 */

#ifndef WM_GAMMA_CONTROL_H
#define WM_GAMMA_CONTROL_H

struct wm_server;

/* Initialize wlr-gamma-control-v1 protocol */
void wm_gamma_control_init(struct wm_server *server);

#endif /* WM_GAMMA_CONTROL_H */
