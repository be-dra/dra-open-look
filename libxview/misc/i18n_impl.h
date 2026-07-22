/*      @(#)i18n_impl.h 1.23 93/06/28 SMI	DRA: RCS: $Id: i18n_impl.h,v 4.5 2026/07/21 10:21:22 dra Exp $ */
/*
 *      (c) Copyright 1990 Sun Microsystems, Inc. Sun design patents 
 *      pending in the U.S. and foreign countries. See LEGAL_NOTICE 
 *      file for terms of the license.
 */

#ifndef i18n_impl_h_DEFINED
#define i18n_impl_h_DEFINED

#include <xview/xv_c_types.h>
#include <xview/pkg.h>
#include <xview/xv_i18n.h>
#include <libintl.h>
#include <stdlib.h>

#ifndef OS_HAS_LOCALE
#define OS_HAS_LOCALE
#endif


#ifdef OS_HAS_LOCALE

#include  <locale.h>

/* Linux: gcc 2.4.x does not have LC_MESSAGES, but it has LC_RESPONSE instead */
#if defined(__linux) && !defined(LC_MESSAGES) && defined(LC_RESPONSE)
#define LC_MESSAGES LC_RESPONSE
#endif


#define XV_I18N_MSG(d,s)	(dgettext(d,s))

#ifndef XV_14_CHARS_FILE_NAME
/*
 * System with long file name.
 */
#define XV_TEXT_DOMAIN          "SUNW_WST_LIBXVIEW"
#else /* XV_14_CHARS_FILE_NAME */
/*
 * System with short (max 14 chars) file name.
 */
#define XV_TEXT_DOMAIN          "XVIEW"
#endif /* XV_14_CHARS_FILE_NAME */

#ifdef XGETTEXT
#define	xv_domain		XV_TEXT_DOMAIN
#else  /* XGETTEXT */
/* Initial value assigned at xv_init.c */
Xv_private CONST char	*xv_domain;
#endif /* XGETTEXT */

#define XV_MSG(s)		(xv_dgettext(xv_domain, s))

#else  /* OS_HAS_LOCALE */

#define XV_I18N_MSG(d,s)	((s))

#define XV_MSG(s)		((s))

#endif /* OS_HAS_LOCALE */

#define	ATOI		atoi
#define	CHAR		char
#define	INDEX		STRCHR
#define	RINDEX		STRRCHR
#define	SPRINTF		sprintf
#define	STRCAT		strcat
#define	STRCHR		strchr
#define	STRCMP		strcmp
#define	STRCPY		strcpy
#ifdef notdef
/* Conflict with sun.h's define */
#define	STRDUP		strdup
#endif
#define	STRLEN		strlen
#define	STRNCAT		strncat
#define	STRNCMP		strncmp
#define	STRNCPY		strncpy
#define	STRRCHR		strrchr

#define XV_STRSAVE(s)	 \
	STRCPY((CHAR *)xv_malloc((STRLEN(s)+1) * sizeof(CHAR)), (s))

#endif /* i18n_impl_h_DEFINED */
