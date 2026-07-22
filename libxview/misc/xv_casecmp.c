#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)xv_casecmp.c 1.4 93/06/28 DRA: RCS $Id: xv_casecmp.c,v 4.3 2026/07/21 10:21:22 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1992 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL_NOTICE
 *	file for terms of the license.
 */

/*
 * The strcasecmp() function is neither defined by POSIX or SVID,
 * so we provide this for portablity.  Note that a real implementation
 * would optimize performance with a lookup table rather than calling
 * tolower(), but hey, this is a GUI toolkit, not libc; so I ain't goin'
 * to that much trouble.
 */

#include <stdio.h>
#include <ctype.h>
#include <xview_private/i18n_impl.h>

int xv_strcasecmp(char *str1, char *str2);

int xv_strcasecmp(char *str1, char *str2)
{
    char low1, low2;

    if ( str1 == str2 )
	return 0;

    while ( (low1 = tolower(*str1)) == (low2 = tolower(*str2)) ) {
	if ( !low1 )
	    return 0;
	str1++; str2++;
    }

    return low1 - low2;
}

int xv_strncasecmp(char *str1, char *str2, int n);

int xv_strncasecmp(char *str1, char *str2, int n)
{
    char low1, low2;

    if ( str1 == str2 )
	return 0;

    n++;

    while ( (--n != 0) && ((low1 = tolower(*str1)) == (low2 = tolower(*str2))) ) {
	if ( !low1 )
	    return 0;
	str1++; str2++;
    }

    return ( (n == 0) ? 0 : (low1 - low2) );
}
