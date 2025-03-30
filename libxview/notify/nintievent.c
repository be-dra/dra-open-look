#ifndef	lint
#ifdef sccs
static char     sccsid[] = "@(#)nintievent.c 20.12 93/06/28 Copyr 1985 Sun Micro  DRA: $Id: nintievent.c,v 4.2 2025/03/29 21:02:24 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Nint_i_event.c - Implement the notify_interpose_event_func interface.
 */

#include <xview_private/ntfy.h>
#include <xview_private/ndet.h>
#include <xview_private/nint.h>

Notify_error notify_interpose_event_func(Notify_client nclient,
				Notify_event_interposer_func func, Notify_event_type when)
{
	NTFY_TYPE type;

	/* Check arguments */
	if (ndet_check_when(when, &type))
		return notify_errno;

	return nint_interpose_func(nclient, (Notify_func)func, type, NTFY_DATA_NULL,
					NTFY_IGNORE_DATA);
}
