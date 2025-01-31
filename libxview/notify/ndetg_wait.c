#ifndef	lint
#ifdef sccs
static char     sccsid[] = "@(#)ndetg_wait.c 20.12 93/06/28 Copyr 1985 Sun Micro DRA: $Id: ndetg_wait.c,v 4.1 2024/03/28 18:09:08 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Ndet_g_wait.c - Implement the notify_get_wait3_func interface.
 */

#include <xview_private/ntfy.h>
#include <xview_private/ndet.h>

Notify_func notify_get_wait3_func(Notify_client nclient, int pid)
{
	/* Check arguments */
	if (ndet_check_pid(pid))
		return (NOTIFY_FUNC_NULL);
	return (ndet_get_func(nclient, NTFY_WAIT3, (NTFY_DATA) ((long)pid),
					NTFY_USE_DATA));
}
