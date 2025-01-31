#ifndef	lint
#ifdef sccs
static char     sccsid[] = "@(#)ndet_g_sig.c 20.12 93/06/28 Copyr 1985 Sun Micro DRA: $Id: ndet_g_sig.c,v 4.1 2024/03/28 18:09:08 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Ndet_g_sig.c - Implement the notify_get_signal_func interface.
 */

#include <xview_private/ntfy.h>
#include <xview_private/ndet.h>

Notify_signal_func notify_get_signal_func(Notify_client   nclient,
    int             signal,
    Notify_signal_mode mode)
{
	NTFY_TYPE type;

	/* Check arguments */
	if (ndet_check_mode(mode, &type))
		return (NOTIFY_SIGNAL_FUNC_NULL);
	if (ndet_check_sig(signal))
		return (NOTIFY_SIGNAL_FUNC_NULL);
	return (Notify_signal_func)ndet_get_func(nclient, type,
							(NTFY_DATA) ((long)signal), NTFY_USE_DATA);
}
