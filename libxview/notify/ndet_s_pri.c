#ifndef	lint
#ifdef sccs
static char     sccsid[] = "@(#)ndet_s_pri.c 20.12 93/06/28 Copyr 1985 Sun Micro DRA: $Id: ndet_s_pri.c,v 4.1 2024/03/28 18:09:08 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Ndet_s_pri.c - Implement the notify_set_prioritizer_func interface.
 */

#include <xview_private/ntfy.h>
#include <xview_private/ndet.h>
#include <xview_private/ndis.h>	/* For ndis_default_prioritizer */

Notify_prioritizer_func notify_set_prioritizer_func(Notify_client nclient,
    Notify_prioritizer_func func)
{
    Notify_prioritizer_func old_func = NOTIFY_PRIO_FUNC_NULL;
    register NTFY_CLIENT *client;

    NTFY_BEGIN_CRITICAL;
    /* Find/create client that corresponds to nclient */
    if ((client = ntfy_new_nclient(&ndet_clients, nclient,
				   &ndet_client_latest)) == NTFY_CLIENT_NULL)
	goto Done;
    /* Exchange functions */
    old_func = client->prioritizer;
    client->prioritizer = func;
    /* Use default if null */
    if (func == NOTIFY_PRIO_FUNC_NULL)
	client->prioritizer = ndis_default_prioritizer;
Done:
    NTFY_END_CRITICAL;
    return (old_func);
}
