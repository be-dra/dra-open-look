#ifndef	lint
#ifdef sccs
static char     sccsid[] = "@(#)nintndeath.c 20.12 93/06/28 Copyr 1985 Sun Micro  DRA: $Id: nintndeath.c,v 4.1 2024/03/28 18:09:08 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Nint_n_death.c - Implement the notify_next_destroy_func interface.
 */

#include <xview_private/ntfy.h>
#include <xview_private/ndet.h>
#include <xview_private/nint.h>

extern          Notify_value
notify_next_destroy_func(nclient, status)
    Notify_client   nclient;
    Destroy_status  status;
{
    Notify_func     func;

    /* Check arguments */
    if (ndet_check_status(status))
	return (NOTIFY_UNEXPECTED);
    if ((func = nint_next_callout(nclient, NTFY_DESTROY)) ==
	NOTIFY_FUNC_NULL)
	return (NOTIFY_UNEXPECTED);
    return (func(nclient, status));
}
