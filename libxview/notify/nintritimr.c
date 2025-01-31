#ifndef	lint
#ifdef sccs
static char     sccsid[] = "@(#)nintritimr.c 20.12 93/06/28 Copyr 1985 Sun Micro  DRA: $Id: nintritimr.c,v 4.1 2024/03/28 18:09:08 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Nint_r_itimer.c - Implement the notify_remove_itimer_func interface.
 */

#include <xview_private/ntfy.h>
#include <xview_private/ndet.h>
#include <xview_private/nint.h>

Notify_error notify_remove_itimer_func(Notify_client nclient,
    Notify_timer_func func, int which)
{
	NTFY_TYPE type;

	/* Check arguments */
	if (ndet_check_which(which, &type))
		return (notify_errno);
	return (nint_remove_func(nclient, (Notify_func)func, type, NTFY_DATA_NULL,
					NTFY_IGNORE_DATA));
}
