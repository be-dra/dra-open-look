#ifndef	lint
#ifdef sccs
static char     sccsid[] = "@(#)ndet_g_in.c 20.12 93/06/28 Copyr 1985 Sun Micro DRA: $Id: ndet_g_in.c,v 4.1 2024/03/28 18:09:08 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Ndet_g_in.c - Implement notify_get_input_func call.
 */

#include <xview_private/ntfy.h>
#include <xview_private/ndet.h>

Notify_io_func notify_get_input_func(Notify_client nclient, int fd)
{
    return (Notify_io_func)ndet_get_fd_func(nclient, fd, NTFY_INPUT);
}
