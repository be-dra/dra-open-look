#ifndef	lint
#ifdef sccs
static char     sccsid[] = "@(#)nintrexcpt.c 20.12 93/06/28 Copyr 1985 Sun Micro  DRA: $Id: nintrexcpt.c,v 4.1 2024/03/28 18:09:08 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Nint_r_except.c - Implement the notify_remove_exception_func interface.
 */

#include <xview_private/ntfy.h>
#include <xview_private/nint.h>

Notify_error notify_remove_exception_func(Notify_client nclient, Notify_io_func func, int fd)
{
    return nint_remove_fd_func(nclient, (Notify_func)func, NTFY_EXCEPTION, fd);
}
