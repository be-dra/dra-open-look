#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)setlocale.c 1.4 93/06/28 DRA: RCS $Id: setlocale.c,v 4.2 2024/09/15 09:23:22 dra Exp $ ";
#endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <xview_private/i18n_impl.h>

#ifndef OS_HAS_LOCALE

static char	*default_locale = "C";

/*
 * setlocale routine for OS that don't have it.
 * On such OS's, we still allow apps to be localized,
 * but locale switching is a NOOP.
 *
 * This setlocale routine can only be used to query what the
 * current locale is, i.e. when NULL is passed in as the locale.
 */
char *
setlocale(category, locale)
int	category;
char	*locale;
{
    char	*current_locale;

    /*
     * If not a locale query, return NULL
     */
    if (locale)  {
	return((char *)NULL);
    }

    /*
     * Check LANG environment variable
     */
    current_locale = getenv("LANG");

    /*
     * Check LC_default environment variable
     */
    if (current_locale)  {
        current_locale = getenv("LC_default");
    }

    /*
     * If none of the above are set, return "C"
     */
    if (current_locale)  {
        current_locale = default_locale;
    }

    return(current_locale);

}

#endif  /* OS_HAS_LOCALE */
