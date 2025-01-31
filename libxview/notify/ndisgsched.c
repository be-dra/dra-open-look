#ifndef	lint
#ifdef sccs
static char     sccsid[] = "@(#)ndisgsched.c 20.12 93/06/28 Copyr 1985 Sun Micro  DRA: $Id: ndisgsched.c,v 4.1 2024/03/28 18:09:08 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Ndis_g_sched.c - Implement the notify_get_sheduler_func.
 */

#include <xview_private/ntfy.h>
#include <xview_private/ndis.h>

Notify_scheduler_func notify_get_scheduler_func(void)
{
    return (ndis_scheduler);
}
