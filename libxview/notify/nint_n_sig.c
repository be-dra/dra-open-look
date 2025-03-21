#ifndef	lint
#ifdef sccs
static char     sccsid[] = "@(#)nint_n_sig.c 20.12 93/06/28 Copyr 1985 Sun Micro  DRA: $Id: nint_n_sig.c,v 4.1 2024/03/28 18:09:08 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Nint_n_sig.c - Implement the notify_next_signal_func interface.
 */

#include <xview_private/ntfy.h>
#include <xview_private/ndet.h>
#include <xview_private/nint.h>

extern          Notify_value
notify_next_signal_func(nclient, signal, mode)
    Notify_client   nclient;
    int             signal;
    Notify_signal_mode mode;
{
    Notify_func     func;
    NTFY_TYPE       type;

    /* Check arguments */
    if (ndet_check_mode(mode, &type) || ndet_check_sig(signal))
	return (NOTIFY_UNEXPECTED);
    if ((func = nint_next_callout(nclient, type)) == NOTIFY_FUNC_NULL)
	return (NOTIFY_UNEXPECTED);
    return (func(nclient, signal, mode));
}
