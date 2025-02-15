#ifndef	lint
#ifdef sccs
static char     sccsid[] = "@(#)nint_r_fd.c 20.12 93/06/28 Copyr 1985 Sun Micro  DRA: $Id: nint_r_fd.c,v 4.1 2024/03/28 18:09:08 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Nint_r_fd.c - Implement the nint_remove_fd_func private interface.
 */

#include <xview_private/ntfy.h>
#include <xview_private/ndet.h>
#include <xview_private/nint.h>

pkg_private Notify_error nint_remove_fd_func(Notify_client nclient,
					Notify_func func, NTFY_TYPE type, int fd)
{
	/* Check arguments */
	if (ndet_check_fd(fd))
		return (notify_errno);
	return (nint_remove_func(nclient, func, type, (NTFY_DATA)((long)fd),
					NTFY_USE_DATA));
}
