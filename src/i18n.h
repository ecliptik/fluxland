/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 *
 * i18n.h - Internationalization macros
 *
 * Provides the _() macro for marking translatable strings via gettext.
 * Include this header in any source file that contains user-visible strings.
 */

#ifndef WM_I18N_H
#define WM_I18N_H

#ifdef HAVE_GETTEXT
#include <libintl.h>
#define _(str) gettext(str)
#else
#define _(str) (str)
#endif

/* Mark string for extraction without translating at runtime */
#define N_(str) (str)

#endif /* WM_I18N_H */
