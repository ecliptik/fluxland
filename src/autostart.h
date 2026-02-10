/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 *
 * autostart.h - Startup script execution
 *
 * Runs a startup script from the config directory on compositor launch,
 * similar to Fluxbox's ~/.fluxbox/startup mechanism.
 */

#ifndef WM_AUTOSTART_H
#define WM_AUTOSTART_H

/*
 * Run the autostart script from the config directory.
 *
 * Looks for config_dir/startup. If the file exists and is executable,
 * it is executed directly. Otherwise it is run via /bin/sh.
 * WAYLAND_DISPLAY is set in the child environment.
 *
 * Returns 0 on success, -1 if no startup file was found.
 */
int wm_autostart_run(const char *config_dir, const char *wayland_display);

/*
 * Run an arbitrary command string via /bin/sh.
 *
 * Used for the -s/--startup command-line flag.
 * WAYLAND_DISPLAY is set in the child environment.
 */
void wm_autostart_run_cmd(const char *cmd, const char *wayland_display);

#endif /* WM_AUTOSTART_H */
