#ifndef	lint
#ifdef sccs
static char     sccsid[] = "@(#)nint_r_sig.c 20.12 93/06/28 Copyr 1985 Sun Micro  DRA: $Id: nint_r_sig.c,v 4.1 2024/03/28 18:09:08 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Nint_r_sig.c - Implement the notify_remove_signal_func interface.
 */

#include <xview_private/ntfy.h>
#include <xview_private/ndet.h>
#include <xview_private/nint.h>

Notify_error notify_remove_signal_func(Notify_client nclient,
		Notify_signal_func func, int signal, Notify_signal_mode mode)
{
	NTFY_TYPE type;

	/* Check arguments */
	if (ndet_check_mode(mode, &type))
		return (notify_errno);
	if (ndet_check_sig(signal))
		return (notify_errno);
	return nint_remove_func(nclient, (Notify_func)func, type,
						(NTFY_DATA)((long)signal), NTFY_USE_DATA);
}
