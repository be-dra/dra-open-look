#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)gen_impl.h 1.4 93/06/28  DRA: $Id: gen_impl.h,v 4.3 2026/09/07 10:07:04 dra Exp $";
#endif
#endif

obsolete - all in generic.c


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


Pkg_private Xv_opaque generic_get(Xv_object object, int *status,
								Attr_attribute attr, va_list args);

#endif  /* _gen_impl_h_already_included */
