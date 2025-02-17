/*      @(#)xv_i18n.h 1.11 93/06/28; SMI DRA: RCS: $Id: xv_i18n.h,v 4.5 2025/02/16 20:31:41 dra Exp $ */
/*
 *      (c) Copyright 1991 Sun Microsystems, Inc. Sun design patents 
 *      pending in the U.S. and foreign countries. See LEGAL_NOTICE 
 *      file for terms of the license.
 */

#ifndef xv_i18n_h_DEFINED
#define xv_i18n_h_DEFINED

#include <xview/xv_c_types.h>

#ifdef OW_I18N

/*
 * This public header file provided to improve the portability across
 * the different platforms (different vendor/OSs).  It's not only
 * necessary by the XView itself, but all of the i18n XView based
 * programs and/or applications should include this file to avoid
 * unnecessary portability problems.  This file should provide the
 * following information in the all platforms.
 */
/*
 *	1. definition for the "wchar_t".
 *	2. definition for the MB_CUR_MAX, MB_LEN_MAX.
 *	3. definition for the setlocale() function and related
 *	    #define.
 *	4. definition for the wide character and multibyte
 *	   functions (ie, wscpy, mblen....).
 *	5. definition for the wide character classification
 *	   functions.
 *	6. include the all i18n specific Xlib include files.
 *	7. Currently XView require to have a Sun specific extension
 *	   to the XIM spec.  Include those definitions.
 */
/*
 * Also, because of different platform may have a different naming
 * scheme for the wide character functions, you may want to provide
 * the macros to adapt to the specific platform.  The current code is
 * using AT&T MNLS and/or J/ALE naming scheme (wsXXX, ie. wscpy).
 */

#include <stdlib.h>		/* #2 (MB_CUR_MAX) */
#include <limits.h>		/* #2 (MB_LEN_MAX) */
#ifdef sun
#  include <widec.h>		/* #1, #4 */
#else
#  include <wchar.h>
#endif
#include <locale.h>		/* #3 */
#include <wctype.h>		/* #5 */

#include <X11/Xlib.h>
#if ! defined(XlibSpecificationRelease) || XlibSpecificationRelease < 5
/*
 * i18n version of the XView require the X11R5 or later version of the
 * Xlib, if platform provides equivalent functionality in pre-R5
 * environment, specify necessary definition in follows.
 */
#include <X11/XlibR5.h>		/* #6 */
#endif /* XlibSpecificationRelease <= 5 */
#ifdef sun
#  include <X11/XSunExt.h>	/* #7 */
#endif

#endif /* OW_I18N */

/* the original dgettext worked only with the 'official' msgfmt, but
 * this annoyed me with error messages:
 * input file doesn't contain a header entry with a charset specification
 */
EXTERN_FUNCTION (char *xv_dgettext, (const char *, const char *));
EXTERN_FUNCTION (char *xv_bindtextdomain, (char *, unsigned char *));

#endif /* xv_i18n_h_DEFINED */
