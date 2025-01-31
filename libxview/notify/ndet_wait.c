#ifndef	lint
#ifdef sccs
static char     sccsid[] = "@(#)ndet_wait.c 20.12 93/06/28 Copyr 1985 Sun Micro DRA: $Id: ndet_wait.c,v 4.1 2024/03/28 18:09:08 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Ndet_wait.c - Implement wait3 specific calls.
 */

#include <xview_private/ntfy.h>
#include <xview_private/ndet.h>
#include <errno.h>

pkg_private int ndet_check_pid(int pid)
{
	if (kill(pid, 0)) {
		ntfy_set_errno((errno == ESRCH) ? NOTIFY_SRCH : NOTIFY_INVAL);
		return (-1);
	}
	return (0);
}
