#ifndef	lint
#ifdef sccs
static char     sccsid[] = "@(#)ndetpdeath.c 20.15 93/06/28 Copyr 1985 Sun Micro DRA: $Id: ndetpdeath.c,v 4.1 2024/03/28 18:09:08 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Ndet_p_death.c - Notify_post_destroy implementation.
 */

#include <xview_private/ntfy.h>
#include <xview_private/ndet.h>
#include <xview_private/ndis.h>
#include <xview/xview.h>

Notify_error notify_post_destroy(Notify_client nclient, Destroy_status status,
								Notify_event_type when)
{
	NTFY_ENUM enum_code;
	register NTFY_CLIENT *client;
	register NTFY_CONDITION *condition;
	Notify_error return_code = NOTIFY_OK;

	/* Check arguments */
	if (ndet_check_status(status))
		return (notify_errno);
	NTFY_BEGIN_CRITICAL;
	/* Find client */
	if ((client = ntfy_find_nclient(ndet_clients, nclient,
							&ndet_client_latest)) == NTFY_CLIENT_NULL) {
		ntfy_set_errno(NOTIFY_UNKNOWN_CLIENT);
		goto Error;
	}
	/* Find condition that matches type */
	if ((condition = ntfy_find_condition(client->conditions, NTFY_DESTROY,
							&(client->condition_latest), NTFY_DATA_NULL,
							NTFY_IGNORE_DATA)) == NTFY_CONDITION_NULL) {
		ntfy_set_errno(NOTIFY_NO_CONDITION);
		goto Error;
	}
	if (when == NOTIFY_IMMEDIATE) {
		/* Prepare to dispatch event */
		ntfy_set_errno(NOTIFY_OK);
		enum_code = ndet_immediate_destroy(client, condition,
				(NTFY_ENUM_DATA) status);
		/* If checking then set notify_errno */
		if (status == DESTROY_CHECKING) {
			if (enum_code == NTFY_ENUM_TERM)
				return_code = NOTIFY_DESTROY_VETOED;
		}
		else if (status != DESTROY_SAVE_YOURSELF) {
			/* remove client (if client hasn't done so himself) */
			if (ntfy_find_nclient(ndet_clients, nclient,
							&ndet_client_latest) != NTFY_CLIENT_NULL)
				return_code = notify_remove(nclient);
		}
		NTFY_END_CRITICAL;
	}
	else {
		condition->data.status = status;
		if (ndis_enqueue(client, condition) != NOTIFY_OK) {
			ntfy_set_errno(NOTIFY_INTERNAL_ERROR);
			goto Error;
		}
		NTFY_END_CRITICAL;
		/* Dispatch the notification if not in notifier loop */
		if (!(ndet_flags & NDET_STARTED) && (ndis_dispatch() != NOTIFY_OK))
			return (notify_errno);
	}
	return (return_code);
  Error:
	NTFY_END_CRITICAL;
	return (notify_errno);
}
