/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * import.h - fluxland --import-fluxbox: a Fluxbox setup made fluxland's
 */

#ifndef WM_IMPORT_H
#define WM_IMPORT_H

#include <stdbool.h>
#include <stdio.h>

/*
 * Copy a Fluxbox configuration directory (~/.fluxbox when src is NULL)
 * to fluxland's (dst, or $XDG_CONFIG_HOME/fluxland, ~/.config/fluxland
 * when NULL): its paths pointed at the new place, the startup script
 * without "exec fluxbox" and with what only X can do commented out,
 * fbsetbg's last wallpaper as fluxland's.  Files that are there already
 * are left unless force.  What it does is written to out.  Returns 0,
 * or 1 when there was nothing to import or something failed.
 */
int wm_import_fluxbox(const char *src, const char *dst, bool force,
	FILE *out);

/* The startup script line made fluxland's: NULL to drop it (exec
 * fluxbox), or a new line (the caller frees it), commented out with
 * the reason when *why is set */
char *wm_import_startup_line(const char *line, const char **why);

/*
 * A Fluxbox startup script as fluxland runs it: "exec fluxbox" and what
 * follows it left out, the programs only X can run commented out with
 * their reasons, a #!/bin/sh first when it has none. Each change is
 * written to report when it isn't NULL. A new string.
 */
char *wm_import_startup_script(const char *script, FILE *report);

#endif /* WM_IMPORT_H */
