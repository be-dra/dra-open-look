#ifndef	lint
#ifdef sccs
static char     sccsid[] = "@(#)ndet_g_fd.c 20.12 93/06/28 Copyr 1985 Sun Micro DRA: $Id: ndet_g_fd.c,v 4.1 2024/03/28 18:09:08 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Ndet_g_fd.c - Implement notify_get_*_func file descriptor specific calls
 * that are shared among NTFY_INPUT, NTFY_OUTPUT and NTFY_EXCEPTION.
 */

#include <xview_private/ntfy.h>
#include <xview_private/ndet.h>

Notify_func ndet_get_fd_func(Notify_client nclient, int fd, NTFY_TYPE type)
{
	/* Check arguments */
	if (ndet_check_fd(fd))
		return (NOTIFY_FUNC_NULL);
	return (ndet_get_func(nclient, type, (NTFY_DATA)((long)fd), NTFY_USE_DATA));
}
