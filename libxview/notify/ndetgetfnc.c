#ifndef	lint
#ifdef sccs
static char     sccsid[] = "@(#)ndetgetfnc.c 20.12 93/06/28 Copyr 1985 Sun Micro DRA: $Id: ndetgetfnc.c,v 4.1 2024/03/28 18:09:08 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Ndet_get_func.c - Implement the ndet_get_func private interface.
 */

#include <xview_private/ntfy.h>
#include <xview_private/ndet.h>
#include <xview_private/nint.h>

pkg_private Notify_func ndet_get_func(Notify_client nclient,
					    NTFY_TYPE type, NTFY_DATA data, int use_data)
{
	register Notify_func func = NOTIFY_FUNC_NULL;
	register NTFY_CLIENT *client;
	NTFY_CONDITION *condition;

	NTFY_BEGIN_CRITICAL;
	/* Find client */
	if ((client = ntfy_find_nclient(ndet_clients, nclient,
							&ndet_client_latest)) == NTFY_CLIENT_NULL) {
		ntfy_set_warning(NOTIFY_UNKNOWN_CLIENT);
		goto Done;
	}
	/* Find condition */
	if ((condition = ntfy_find_condition(client->conditions, type,
							&(client->condition_latest), data, use_data)) ==
			NTFY_CONDITION_NULL) {
		ntfy_set_warning(NOTIFY_NO_CONDITION);
		goto Done;
	}
	/* Get function */
	func = nint_get_func(condition);
  Done:
	NTFY_END_CRITICAL;
	return (func);
}
