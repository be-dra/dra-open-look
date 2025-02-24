/*      @(#)portable.h 1.14 93/06/28 SMI DRA: RCS: $Id: portable.h,v 4.2 2024/11/30 12:40:36 dra Exp $      */

/*
 *      (c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *      pending in the U.S. and foreign countries. See LEGAL NOTICE
 *      file for terms of the license.
 */


#ifndef xview_portable_h_DEFINED
#define xview_portable_h_DEFINED

#include <xview/attr.h>

#if defined(__STDC__) || defined(__cplusplus) || defined(c_plusplus)
#include <stdarg.h>
#define ANSI_FUNC_PROTO
#define VA_START( ptr, param )  va_start( ptr, param )
#else
#include <varargs.h>
#define VA_START( ptr, param )  va_start( ptr )
#endif

#define NO_CAST_VATOAV 1


EXTERN_FUNCTION (Attr_avlist copy_va_to_av, (va_list valist, Attr_avlist avlist, Attr_attribute attr1));

#ifdef NO_CAST_VATOAV
#define AVLIST_DECL  Attr_attribute avarray[ATTR_STANDARD_SIZE];  \
                     Attr_avlist    avlist = avarray

#define MAKE_AVLIST( valist, avlist ) copy_va_to_av( valist, avlist, XV_NULL )

#else
#define AVLIST_DECL  Attr_avlist  avlist

#define MAKE_AVLIST( valist, avlist )   \
        if( *((Attr_avlist)(valist)) == (Attr_attribute) ATTR_LIST )  \
        {  \
           Attr_attribute avarray[ATTR_STANDARD_SIZE];  \
           avlist = avarray;  \
           copy_va_to_av( valist, avlist, 0 );  \
        }  \
        else  \
           (avlist) = (Attr_avlist)(valist);
#endif


#if !(defined(__STDC__) || defined(__cplusplus) || defined(c_plusplus))
#define const
#endif

#if defined(SVR4) || defined(__linux)
#define XV_BCOPY(a,b,c) memmove(b,a,c)
#define XV_BZERO(a,b) memset(a,0,b)
#define XV_INDEX(a,b) strchr(a,b)
#define XV_RINDEX(a,b) strrchr(a,b)
#else
#include <strings.h>
#define XV_BCOPY(a,b,c) bcopy(a,b,c)
#define XV_BZERO(a,b) bzero(a,b)
#define XV_INDEX(a,b) index(a,b)
#define XV_RINDEX(a,b) rindex(a,b)
#endif

/*
 * Defines governing tty mode and pty behavior.  (These are relevant to the
 * ttysw code.)
 */
#ifdef	SVR4
#  define	XV_USE_TERMIOS
#  define	XV_USE_SVR4_PTYS
#else	/* SVR4 */

#  ifdef __linux
#    define	XV_USE_TERMIOS
#    undef	XV_USE_SVR4_PTYS
#  else
#    undef	XV_USE_TERMIOS
#    undef	XV_USE_SVR4_PTYS
#  endif
#endif	/* SVR4 */

#endif /* xview_portable_h_DEFINED */
