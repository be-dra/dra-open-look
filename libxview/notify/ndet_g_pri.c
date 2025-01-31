#ifndef	lint
#ifdef sccs
static char     sccsid[] = "@(#)ndet_g_pri.c 20.12 93/06/28 Copyr 1985 Sun Micro DRA: $Id: ndet_g_pri.c,v 4.1 2024/03/28 18:09:08 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Ndet_g_pri.c - Implement the notify_get_prioritizer_func interface.
 */

#include <xview_private/ntfy.h>
#include <xview_private/ndet.h>

Notify_prioritizer_func notify_get_prioritizer_func(Notify_client nclient)
{
	register NTFY_CLIENT *client;
	Notify_prioritizer_func func = (Notify_prioritizer_func) 0;

	NTFY_BEGIN_CRITICAL;
	/* Find client that corresponds to nclient */
	if ((client = ntfy_find_nclient(ndet_clients, nclient,
							&ndet_client_latest)) == NTFY_CLIENT_NULL) {
		ntfy_set_errno(NOTIFY_UNKNOWN_CLIENT);
		goto Done;
	}
	/* Get function */
	func = client->prioritizer;
  Done:
	NTFY_END_CRITICAL;
	return (func);
}
