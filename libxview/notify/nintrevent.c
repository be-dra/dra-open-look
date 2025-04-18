#ifndef	lint
#ifdef sccs
static char     sccsid[] = "@(#)nintrevent.c 20.12 93/06/28 Copyr 1985 Sun Micro  DRA: $Id: nintrevent.c,v 4.2 2025/03/29 20:52:23 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Nint_r_event.c - Implement the notify_remove_event_func interface.
 */

#include <xview_private/ntfy.h>
#include <xview_private/ndet.h>
#include <xview_private/nint.h>

Notify_error notify_remove_event_func(Notify_client nclient,
				Notify_event_interposer_func func, Notify_event_type when)
{
    NTFY_TYPE       type;

    /* Check arguments */
    if (ndet_check_when(when, &type)) return (notify_errno);
    return (nint_remove_func(nclient, (Notify_func)func, type, NTFY_DATA_NULL,
			     NTFY_IGNORE_DATA));
}
