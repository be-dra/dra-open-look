#ifndef	lint
#ifdef sccs
static char     sccsid[] = "@(#)ndetgdeath.c 20.12 93/06/28 Copyr 1985 Sun Micro DRA: $Id: ndetgdeath.c,v 4.1 2024/03/28 18:09:08 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Ndet_g_destroy.c - Implement the notify_get_destroy_func interface.
 */

#include <xview_private/ntfy.h>
#include <xview_private/ndet.h>

Notify_destroy_func notify_get_destroy_func(Notify_client nclient)
{
    return (Notify_destroy_func)ndet_get_func(nclient, NTFY_DESTROY,
										NTFY_DATA_NULL, NTFY_IGNORE_DATA);
}
