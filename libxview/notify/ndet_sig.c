#ifndef	lint
#ifdef sccs
static char     sccsid[] = "@(#)ndet_sig.c 20.12 93/06/28 Copyr 1985 Sun Micro DRA: $Id: ndet_sig.c,v 4.1 2024/03/28 18:09:08 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Ndet_sig.c - Implement signal specific calls that are shared among
 * NTFY_SYNC_SIGNAL and NTFY_ASYNC_SIGNAL.
 */

#include <xview_private/ntfy.h>
#include <xview_private/ndet.h>
#include <signal.h>

pkg_private int ndet_check_mode(Notify_signal_mode mode,
					    NTFY_TYPE *type_ptr)
{
	NTFY_TYPE type;

	switch (mode) {
		case NOTIFY_SYNC:
			type = NTFY_SYNC_SIGNAL;
			break;
		case NOTIFY_ASYNC:
			type = NTFY_ASYNC_SIGNAL;
			break;
		default:
			ntfy_set_errno(NOTIFY_INVAL);
			return (-1);
	}
	if (type_ptr)
		*type_ptr = type;
	return (0);
}

pkg_private int ndet_check_sig(int sig)
{
	if (sig < 0 || sig >= NSIG) {
		ntfy_set_errno(NOTIFY_BAD_SIGNAL);
		return (-1);
	}
	return (0);
}
