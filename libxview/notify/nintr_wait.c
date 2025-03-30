#ifndef	lint
#ifdef sccs
static char     sccsid[] = "@(#)nintr_wait.c 20.12 93/06/28 Copyr 1985 Sun Micro  DRA: $Id: nintr_wait.c,v 4.2 2025/03/29 20:48:51 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Nint_r_wait.c - Implement the notify_remove_wait3_func interface.
 */

#include <xview_private/ntfy.h>
#include <xview_private/ndet.h>
#include <xview_private/nint.h>

Notify_error notify_remove_wait3_func(Notify_client nclient, Notify_func func,
										int pid)
{
    /* Don't check pid because may be gone by now */
    return (nint_remove_func(nclient, func, NTFY_WAIT3,
			     (NTFY_DATA)((long)pid), NTFY_USE_DATA));
}
