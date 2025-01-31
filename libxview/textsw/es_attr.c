#ifndef lint
char     es_attr_c_sccsid[] = "@(#)es_attr.c 20.22 93/06/28 DRA: $Id: es_attr.c,v 4.1 2024/03/28 19:06:00 dra Exp $";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Attribute support for entity streams.
 */

#include <sys/types.h>
#include <xview/attrol.h>
#include <xview/pkg.h>
#include <xview_private/primal.h>
#include <xview_private/es.h>

Pkg_private int
#ifdef ANSI_FUNC_PROTO
es_set(Es_handle esh, ...)
#else
es_set(esh, va_alist)
    Es_handle esh;
va_dcl
#endif
{
    va_list  valist;
    AVLIST_DECL;

    VA_START( valist, esh );
    MAKE_AVLIST( valist, avlist );
    va_end( valist );
    return (esh->ops->set(esh, avlist));
}
