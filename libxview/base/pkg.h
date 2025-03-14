/*      @(#)pkg.h 20.22 91/03/19 SMI   DRA: $Id: pkg.h,v 4.3 2025/03/08 12:52:02 dra Exp $      */

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/* 
 * 	Of interest to package implementors only.
 */

#ifndef xview_pkg_DEFINED
#define xview_pkg_DEFINED

/*
 ***********************************************************************
 *			Include Files
 ***********************************************************************
 */

/* Don't include notify.h - it is not actually used by very many modules, and
 * generates too many symbols in the libraries.
 */

#include <xview/attr.h>
#include <xview/notify.h>

#if defined(__STDC__) || defined(__cplusplus) || defined(c_plusplus)
#include <stdarg.h>
#else
#include <varargs.h>
#endif

/*
 ***********************************************************************
 *			Definitions and Macros
 ***********************************************************************
 */

/*
 * PRIVATE #defines (for package implementors only)
 */

#ifndef MAX_NESTED_PKGS
#define MAX_NESTED_PKGS	20
#endif

/*
 * Conversion macros for package implementors (public <=> private)	
 */

#define XV_PRIVATE(private_type, public_type, obj) \
	((private_type *)((public_type *) (obj))->private_data)
#define XV_PUBLIC(obj) \
	((obj)->public_self)

/* set procs can return XV_SET_DONE if xv_super_set_avlist()
 * has been called.  This will end the set.  Note that
 * other possible set proc return values are XV_OK or an attribute.
 */
#define	XV_SET_DONE	((Xv_opaque) 2)

#define Xv_public_data		/* Part of the client interface */
#define Xv_private_data		/* Should only be used by the XView toolkit */
#define Sv1_public	extern /* SunView1 compatibility only; part of the 
				* client interface 
				*/
#define Xv_public	extern	/* Part of the client interface */
#define Xv_private	extern	/* Should only be used by the XView toolkit */
#define Pkg_private	extern	/* Should only be used by same pkg */

typedef struct _xview_pkg {
    char                *name;
    Attr_attribute       attr_id;
    unsigned             size_of_object;
    const struct _xview_pkg   *parent_pkg;
    int                  (*init)(Xv_opaque , Xv_opaque, Attr_avlist, int *);
    Xv_opaque            (*set)(Xv_opaque, Attr_avlist);
    Xv_opaque            (*get)(Xv_opaque, int *, Attr_attribute, va_list vali);
    int                  (*destroy)(Xv_opaque, Destroy_status );
    Xv_object            (*find)(Xv_opaque, const struct _xview_pkg *, Attr_avlist);
} Xv_pkg;

/*
 ***********************************************************************
 *				Globals
 ***********************************************************************
 */

/*
 * PRIVATE functions for package implementors only
 */

_XVFUNCPROTOBEGIN
EXTERN_FUNCTION (Xv_object xv_create_avlist, (Xv_opaque parent, const Xv_pkg *pkg, Attr_attribute *avlist));
EXTERN_FUNCTION (Xv_opaque xv_set_avlist, (Xv_opaque passed_object, Attr_avlist avlist));
EXTERN_FUNCTION (Xv_opaque xv_super_set_avlist, (Xv_opaque object, const Xv_pkg *pkg, Attr_avlist avlist));
EXTERN_FUNCTION (int xv_destroy_status, (Xv_object passed_object, Destroy_status status));
EXTERN_FUNCTION (int xv_check_bad_attr, (const Xv_pkg *pkg, Attr_attribute attr));
EXTERN_FUNCTION (Xv_opaque xv_get_varargs, (Xv_opaque passed_object, Attr_attribute attr, va_list valist));

EXTERN_FUNCTION (void xv_add_custom_attrs, (const Xv_pkg *, DOTDOTDOT)) _X_SENTINEL(0);
_XVFUNCPROTOEND

#endif /* ~xview_pkg_DEFINED */
