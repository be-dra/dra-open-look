#ifndef	lint
#ifdef sccs
static char     sccsid[] = "@(#)nintiexcpt.c 20.12 93/06/28 Copyr 1985 Sun Micro  DRA: $Id: nintiexcpt.c,v 4.1 2024/03/28 18:09:08 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Nint_i_except.c - Implement the notify_interpose_exception_func interface.
 */

#include <xview_private/ntfy.h>
#include <xview_private/nint.h>

Notify_error notify_interpose_exception_func(Notify_client nclient, Notify_io_func func, int fd)
{
    return nint_interpose_fd_func(nclient, (Notify_func)func, NTFY_EXCEPTION, fd);
}
