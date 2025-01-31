#ifndef	lint
#ifdef sccs
static char     sccsid[] = "@(#)ndet_s_out.c 20.12 93/06/28 Copyr 1985 Sun Micro DRA: $Id: ndet_s_out.c,v 4.1 2024/03/28 18:09:08 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Ndet_s_out.c - Implement notify_set_output_func call.
 */

#include <xview_private/ntfy.h>
#include <xview_private/ndet.h>

Notify_io_func notify_set_output_func(Notify_client nclient, Notify_io_func func, int fd)
{
    return (Notify_io_func)ndet_set_fd_func(nclient, (Notify_func)func, fd, NTFY_OUTPUT);
}
