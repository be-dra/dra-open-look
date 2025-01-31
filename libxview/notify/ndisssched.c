#ifndef	lint
#ifdef sccs
static char     sccsid[] = "@(#)ndisssched.c 20.12 93/06/28 Copyr 1985 Sun Micro  DRA: $Id: ndisssched.c,v 4.1 2024/03/28 18:09:08 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Ndis_s_sched.c - Implement the notify_set_sheduler_func.
 */

#include <xview_private/ntfy.h>
#include <xview_private/ndis.h>

Notify_scheduler_func
notify_set_scheduler_func(Notify_scheduler_func scheduler_func)
{
	Notify_scheduler_func old_func;

	NTFY_BEGIN_CRITICAL;
	old_func = ndis_scheduler;
	ndis_scheduler = scheduler_func;
	if (ndis_scheduler == NOTIFY_SCHED_FUNC_NULL)
		ndis_scheduler = ndis_default_scheduler;
	NTFY_END_CRITICAL;
	return (old_func);
}
