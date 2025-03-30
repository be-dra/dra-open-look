#ifndef	lint
#ifdef sccs
static char     sccsid[] = "@(#)nintideath.c 20.14 93/06/28  DRA: $Id: nintideath.c,v 4.2 2025/03/29 20:43:29 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Nint_i_death.c - Implement the notify_interpose_destroy_func interface.
 */

#include <xview_private/ntfy.h>
#include <xview_private/ndet.h>
#include <xview_private/nint.h>


/* nowhere a prototype ??? - but also nowhere called.... */
Notify_func notify_set_destroy_interposer(Notify_func func);

/*
 * Following indirection added to allow XView to redefine
 * notify_interpose_destroy_func() when Notifier used as just a part of
 * XView, while still allowing applications to use the Notifier
 * independent of the rest of XView.
 * 
 * To make live easy, we add the following typedef for a pointer to a function
 * returning a Notify_value.
 */
typedef Notify_error (*dstr_intrps)(Notify_client nclient, Notify_destroy_func func);
static Notify_error default_interpose_destroy_func(Notify_client nclient, Notify_destroy_func func);
static dstr_intrps nint_destroy_interposer = default_interpose_destroy_func;

Notify_error notify_interpose_destroy_func(Notify_client nclient, Notify_destroy_func func)
{
    return nint_destroy_interposer(nclient, func);
}

Notify_func notify_set_destroy_interposer(Notify_func func)
{
	dstr_intrps f = (dstr_intrps)func;
	dstr_intrps result = nint_destroy_interposer;

    nint_destroy_interposer = f ? f : default_interpose_destroy_func;
    return (Notify_func)result;
}

static Notify_error default_interpose_destroy_func(Notify_client nclient, Notify_destroy_func func)
{
    return nint_interpose_func(nclient, (Notify_func)func, NTFY_DESTROY,
							NTFY_DATA_NULL, NTFY_IGNORE_DATA);
}
