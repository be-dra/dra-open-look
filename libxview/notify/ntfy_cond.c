#ifndef	lint
#ifdef sccs
static char     sccsid[] = "@(#)ntfy_cond.c 20.19 93/06/28 Copyr 1985 Sun Micro  DRA: $Id: ntfy_cond.c,v 4.2 2025/03/29 21:01:26 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Ntfy_condition.c - NTFY_CONDITION specific operations that both the
 * detector and dispatcher share.
 */

#include <xview_private/ntfy.h>
#include <stdio.h>		/* For NULL */
#include <signal.h>

/*
 * Paranoid enumerator protection macros.
 */
static int ntfy_paranoid_count;
#define	NTFY_BEGIN_PARANOID	ntfy_paranoid_count++
#define	NTFY_IN_PARANOID	(ntfy_paranoid_count > 0)
#define	NTFY_END_PARANOID   	ntfy_paranoid_count--;

/* Variables used in paranoid enumerator */
static NTFY_CONDITION *ntfy_enum_condition;
static NTFY_CONDITION *ntfy_enum_condition_next;

void ntfy_reset_paranoid(void);

pkg_private NTFY_CONDITION *ntfy_find_condition(NTFY_CONDITION *condition_list,
				NTFY_TYPE type, register NTFY_CONDITION **condition_latest,
				NTFY_DATA data, int use_data)
{
	register NTFY_CONDITION *condition;
	NTFY_CONDITION *next;

	ntfy_assert(NTFY_IN_CRITICAL, 23 /* Unprotected list search */ );
	/* See if hint matches */
	if (*condition_latest && (*condition_latest)->type == type) {
		if (!use_data || (NTFY_DATA) (*condition_latest)->data.an_u_long == data)
			return (*condition_latest);
	}
	/* Search entire list */
	for (condition = condition_list; condition; condition = next) {
		next = condition->next;
		if (condition->type == type) {
			if (!use_data || (NTFY_DATA) condition->data.an_u_long == data) {
				/* Set up hint for next time */
				*condition_latest = condition;
				return (condition);
			}
		}
	}

	return (NTFY_CONDITION_NULL);
}

/*
 * Find/create condition defined by type and data (if use_data TRUE)
 */
pkg_private NTFY_CONDITION *ntfy_new_condition(NTFY_CONDITION **condition_list,
							NTFY_TYPE type, NTFY_CONDITION **condition_latest,
							NTFY_DATA data, int use_data)
{
	register NTFY_CONDITION *condition;

	if ((condition = ntfy_find_condition(*condition_list, type,
							condition_latest, data,
							use_data)) == NTFY_CONDITION_NULL)
	{
		/* Allocate condition */
		if ((condition = ntfy_alloc_condition()) == NTFY_CONDITION_NULL)
			return (NTFY_CONDITION_NULL);
		/* Initialize condition */
		condition->next = NTFY_CONDITION_NULL;
		condition->type = type;
		condition->data.an_u_long = (u_long) ((use_data) ? data : 0);
		condition->callout.function = (Notify_func) notify_nop;
		condition->func_count = 0;
		condition->func_next = 0;
		condition->arg = (Notify_arg) 0;
		condition->release = NOTIFY_RELEASE_NULL;
		/* Append to condition list */
		ntfy_append_condition(condition_list, condition);
		/* Set up condition hint */
		*condition_latest = condition;
	}
	return (condition);
}

pkg_private void
ntfy_unset_condition(client_list, client, condition, client_latest, who)
    NTFY_CLIENT   **client_list;
    NTFY_CLIENT    *client;
    NTFY_CONDITION *condition;
    NTFY_CLIENT   **client_latest;
    NTFY_WHO        who;
{
    /* Remove and free condition from client */
    ntfy_remove_condition(client, condition, who);
    /* Free client if no more condition */
    if (client->conditions == NTFY_CONDITION_NULL)
	ntfy_remove_client(client_list, client, client_latest, who);
}

pkg_private void
ntfy_remove_condition(client, condition, who)
    NTFY_CLIENT    *client;
    NTFY_CONDITION *condition;
    NTFY_WHO        who;
{
	/* Fixup enumeration variables if condition matches one of them */
	if (condition == ntfy_enum_condition)
		ntfy_enum_condition = NTFY_CONDITION_NULL;
	if (condition == ntfy_enum_condition_next)
		ntfy_enum_condition_next = ntfy_enum_condition_next->next;
	ntfy_remove_from_table(client, condition);
	/* Free data portion of condition if dynamically allocated */
	/* First, check to see if might be non-null pointer */
	if (condition->data.an_u_long != 0) {
		switch (condition->type) {
			case NTFY_REAL_ITIMER:
			case NTFY_VIRTUAL_ITIMER:
				if (who == NTFY_NDET)
					ntfy_free_node((NTFY_NODE *) condition->data.an_u_long);
				break;
			case NTFY_WAIT3:
				if (who == NTFY_NDIS)
					ntfy_free_malloc((NTFY_DATA) condition->data.an_u_long);
				break;
			default:
				break;
		}
	}
	/* Free any function list */
	if (condition->func_count > 1 && condition->callout.functions)
		ntfy_free_functions(condition->callout.functions);
	/* Remove & free condition from client's condition list */
	ntfy_remove_node((NTFY_NODE **) & client->conditions,
			(NTFY_NODE *) condition);
	/* Invalidate condition hint */
	client->condition_latest = NTFY_CONDITION_NULL;
}

pkg_private NTFY_ENUM ntfy_paranoid_enum_conditions(NTFY_CLIENT *clients,
    				NTFY_ENUM_FUNC enum_func, NTFY_ENUM_DATA  context)
{
    extern NTFY_CLIENT *ntfy_enum_client;
    extern NTFY_CLIENT *ntfy_enum_client_next;
    NTFY_ENUM       return_code = NTFY_ENUM_NEXT;

    extern sigset_t    ndet_sigs_managing;
    sigset_t oldmask, newmask;
    newmask = ndet_sigs_managing;  /* assume interesting < 32 */
    sigprocmask(SIG_BLOCK, &newmask , &oldmask);


    /*
     * Blocking signals because async signal sender uses this paranoid
     * enumerator.
     */

    ntfy_assert(!NTFY_IN_PARANOID, 24
		/* More then 1 paranoid using enumerator */);
    NTFY_BEGIN_PARANOID;
    for (ntfy_enum_client = clients; ntfy_enum_client;
	 ntfy_enum_client = ntfy_enum_client_next) {
	ntfy_enum_client_next = ntfy_enum_client->next;
	for (ntfy_enum_condition = ntfy_enum_client->conditions;
	     ntfy_enum_condition;
	     ntfy_enum_condition = ntfy_enum_condition_next) {
	    ntfy_enum_condition_next = ntfy_enum_condition->next;
	    switch (enum_func(ntfy_enum_client, ntfy_enum_condition,
			      context)) {
	      case NTFY_ENUM_SKIP:
		goto BreakOut;
	      case NTFY_ENUM_TERM:
		return_code = NTFY_ENUM_TERM;
		goto Done;
	      default:
		if (ntfy_enum_client == NTFY_CLIENT_NULL)
		    goto BreakOut;
	    }
	}
BreakOut:
	{
	}
    }
Done:
    /* Reset global state */
    ntfy_enum_client = ntfy_enum_client_next = NTFY_CLIENT_NULL;
    ntfy_enum_condition = ntfy_enum_condition_next = NTFY_CONDITION_NULL;
    NTFY_END_PARANOID;
    sigprocmask(SIG_SETMASK, &oldmask, (sigset_t *) 0);
    return (return_code);
}

/* VARARGS2 */
pkg_private     NTFY_ENUM
ntfy_enum_conditions(NTFY_CLIENT *clients, NTFY_ENUM_FUNC enum_func, NTFY_ENUM_DATA context)
{
	register NTFY_CLIENT *clt, *clt_next;
	register NTFY_CONDITION *cdn, *cdn_next;

	for (clt = clients; clt; clt = clt_next) {
		clt_next = clt->next;
		for (cdn = clt->conditions; cdn; cdn = cdn_next) {
			cdn_next = cdn->next;
			switch (enum_func(clt, cdn, context)) {
				case NTFY_ENUM_SKIP:
					goto BreakOut;
				case NTFY_ENUM_TERM:
					return (NTFY_ENUM_TERM);
				default:{
					}
			}
		}
	  BreakOut:
		{
		}
	}
	return (NTFY_ENUM_NEXT);
}

Notify_value notify_nop()
{
    return (NOTIFY_IGNORED);
}

void ntfy_reset_paranoid(void)
{
    /* Reset static variables used in paranoid enumerator */
    ntfy_paranoid_count = 0;
    ntfy_enum_condition = NTFY_CONDITION_NULL;
    ntfy_enum_condition_next = NTFY_CONDITION_NULL;
}
