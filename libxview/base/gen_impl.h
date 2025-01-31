#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)gen_impl.h 1.4 93/06/28  DRA: $Id: gen_impl.h,v 4.2 2024/05/25 19:13:47 dra Exp $";
#endif
#endif

/***********************************************************************/
/*	                 gen_impl.h	       		       		*/
/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */
/***********************************************************************/

#ifndef _gen_impl_h_already_included
#define _gen_impl_h_already_included

#include <xview/generic.h>
#include <stdint.h>

#define GEN_PUBLIC(obj)		XV_PUBLIC(obj)
#define GEN_PRIVATE(obj)	XV_PRIVATE(Generic_info, Xv_generic_struct, obj)
#define HEAD(obj)		(GEN_PRIVATE(obj))->key_data

typedef uint32_t		generic_key_type_t;

typedef struct _generic_node {
    struct _generic_node *next;
    generic_key_type_t key;
    Xv_opaque       data;
    void            (*copy_proc) (int copy_proc_is_unused);
    void            (*remove_proc) (Xv_opaque, generic_key_type_t, Xv_opaque);
}Generic_node;

typedef	struct	{
    Xv_object		public_self;	/* back pointer to object */
    Xv_object		owner;		/* owner of object */

    Generic_node	*key_data;
    Xv_opaque		instance_qlist;
    char		*instance_name;
} Generic_info;

Pkg_private Xv_opaque generic_get(Xv_object object, int *status,
								Attr_attribute attr, va_list args);

#endif  /* _gen_impl_h_already_included */
