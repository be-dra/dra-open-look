#ifndef	lint
#ifdef sccs
static char     sccsid[] = "@(#)nint_n_in.c 20.12 93/06/28 Copyr 1985 Sun Micro  DRA: $Id: nint_n_in.c,v 4.1 2024/03/28 18:09:08 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Nint_n_in.c - Implement the notify_next_input_func interface.
 */

#include <xview_private/ntfy.h>
#include <xview_private/ndet.h>
#include <xview_private/nint.h>

extern          Notify_value
notify_next_input_func(nclient, fd)
    Notify_client   nclient;
    int             fd;
{
    return (nint_next_fd_func(nclient, NTFY_INPUT, fd));
}
