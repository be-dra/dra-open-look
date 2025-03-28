#ifndef	lint
#ifdef sccs
static char     sccsid[] = "@(#)ndetsitimr.c 20.13 93/06/28 Copyr 1985 Sun Micro  DRA: $Id: ndetsitimr.c,v 4.2 2025/03/25 14:40:13 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Ndet_s_itimer.c - Implement the notify_set_itimer_func interface.
 */

#include <xview_private/ntfy.h>
#include <xview_private/ndet.h>
#include <xview_private/ndis.h>
#include <xview_private/nint.h>

Notify_timer_func notify_set_itimer_func(Notify_client nclient,
    Notify_timer_func func, int which,
    struct itimerval *value, struct itimerval *ovalue)
{
	Notify_timer_func old_func = NOTIFY_TIMER_FUNC_NULL;
	register NTFY_CLIENT *client;
	NTFY_CONDITION *condition;
	NTFY_TYPE type;

	NTFY_BEGIN_CRITICAL;
	/* Check arguments */
	if (ndet_check_which(which, &type)) goto Done;
	/* Allow null value */
	if (value == NTFY_ITIMER_NULL)
		value = &NOTIFY_NO_ITIMER;
	else
		/* Pre-qualify so doesn't blow up in setitimer later */
	if (ndet_check_tv(&(value->it_value)) ||
			ndet_check_tv(&(value->it_interval)))
		goto Done;
	/* Find/create client that corresponds to nclient */
	if ((client = ntfy_new_nclient(&ndet_clients, nclient,
							&ndet_client_latest)) == NTFY_CLIENT_NULL)
		goto Done;
	/* Find/create condition */
	if ((condition = ntfy_new_condition(&(client->conditions), type,
							&(client->condition_latest), NTFY_DATA_NULL,
							NTFY_IGNORE_DATA)) == NTFY_CONDITION_NULL)
		goto Done;
	ntfy_add_to_table(client, condition, (int)type);
	/* Set ovalue if not null */
	if (ovalue != NTFY_ITIMER_NULL) {
		/*
		 * Data will have zero value if first time client has specified
		 * itimer of this type
		 */
		if (condition->data.ntfy_itimer == (struct ntfy_itimer *)0)
			*ovalue = NOTIFY_NO_ITIMER;
		else
			(void)notify_itimer_value(nclient, which, ovalue);
	}
	/* Set condition data */
	if (condition->data.ntfy_itimer == (struct ntfy_itimer *)0)
		condition->data.ntfy_itimer = ntfy_alloc_ntfy_itimer();
	condition->data.ntfy_itimer->itimer = *value;
	ndet_reset_itimer_set_tv(condition);
	/* Exchange functions */
	old_func = (Notify_timer_func) nint_set_func(condition, (Notify_func) func);
	/* Remove condition if func is null or value indicates no timer */
	if (func == NOTIFY_TIMER_FUNC_NULL || !timerisset(&(value->it_value))) {
		ndis_flush_condition(nclient, type, NTFY_DATA_NULL, NTFY_IGNORE_DATA);
		ntfy_unset_condition(&ndet_clients, client, condition,
				&ndet_client_latest, NTFY_NDET);
	}
	/*
	 * Have notifier check for itimer changes next time around loop. Will
	 * notice and adjust signal handling at this time.
	 */
	ndet_flags |= (type == NTFY_REAL_ITIMER) ? NDET_REAL_CHANGE :
			NDET_VIRTUAL_CHANGE;
  Done:
	NTFY_END_CRITICAL;
	return (old_func);
}
