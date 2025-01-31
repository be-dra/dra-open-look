#ifndef	lint
#ifdef sccs
static char     sccsid[] = "@(#)nint_i_sig.c 20.12 93/06/28 Copyr 1985 Sun Micro  DRA: $Id: nint_i_sig.c,v 4.1 2024/03/28 18:09:08 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Nint_i_sig.c - Implement the notify_interpose_signal_func interface.
 */

#include <xview_private/ntfy.h>
#include <xview_private/ndet.h>
#include <xview_private/nint.h>

Notify_error notify_interpose_signal_func(Notify_client nclient, Notify_signal_func func, int signal, Notify_signal_mode mode)
{
	NTFY_TYPE type;

	/*
	 * Check arguments & pre-allocate stack incase going to get asynchronous
	 * event before synchronous one.
	 */
	if (ndet_check_mode(mode, &type) || ndet_check_sig(signal) ||
			(nint_alloc_stack() != NOTIFY_OK))
		return (notify_errno);
	return nint_interpose_func(nclient, (Notify_func)func, type,
							(NTFY_DATA)((long)signal), NTFY_USE_DATA);
}
