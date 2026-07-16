/*      @(#)xv_i18n.h 1.11 93/06/28; SMI DRA: RCS: $Id: xv_i18n.h,v 4.6 2026/07/15 18:30:28 dra Exp $ */
/*
 *      (c) Copyright 1991 Sun Microsystems, Inc. Sun design patents 
 *      pending in the U.S. and foreign countries. See LEGAL_NOTICE 
 *      file for terms of the license.
 */

#ifndef xv_i18n_h_DEFINED
#define xv_i18n_h_DEFINED

#include <xview/xv_c_types.h>

#include <stdlib.h>		/* #2 (MB_CUR_MAX) */
#include <limits.h>		/* #2 (MB_LEN_MAX) */
#include <locale.h>		/* #3 */
#include <wctype.h>		/* #5 */

#include <X11/Xlib.h>

extern int _xv_is_multibyte;

/* the original dgettext worked only with the 'official' msgfmt, but
 * this annoyed me with error messages:
 * input file doesn't contain a header entry with a charset specification
 */
EXTERN_FUNCTION (char *xv_dgettext, (const char *, const char *));
EXTERN_FUNCTION (char *xv_bindtextdomain, (char *, unsigned char *));

#endif /* xv_i18n_h_DEFINED */
