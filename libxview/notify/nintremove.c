#ifndef	lint
#ifdef sccs
static char     sccsid[] = "@(#)nintremove.c 20.13 93/06/28 Copyr 1985 Sun Micro  DRA: $Id: nintremove.c,v 4.2 2025/03/29 20:50:18 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Nint_remove.c - Implement the nint_remove_func private interface.
 */

#include <xview_private/ntfy.h>
#include <xview_private/ndet.h>
#include <xview_private/nint.h>

pkg_private     Notify_error
nint_remove_func(nclient, func, type, data, use_data)
    Notify_client   nclient;
    Notify_func     func;
    NTFY_TYPE       type;
    caddr_t         data;
    int             use_data;
{
    NTFY_CLIENT    *client;
    register NTFY_CONDITION *cond;
    register int    i, j;

    NTFY_BEGIN_CRITICAL;
    /* Find client */
    if ((client = ntfy_find_nclient(ndet_clients, nclient,
				&ndet_client_latest)) == NTFY_CLIENT_NULL) {
	ntfy_set_errno(NOTIFY_UNKNOWN_CLIENT);
	goto Error;
    }
    /* Find condition using data */
    if ((cond = ntfy_find_condition(client->conditions, type,
			    &(client->condition_latest), data, use_data)) ==
	NTFY_CONDITION_NULL) {
	ntfy_set_errno(NOTIFY_NO_CONDITION);
	goto Error;
    }
    /* If going to remove last function then error */
    if (cond->func_count == 1) {
	ntfy_set_errno(NOTIFY_FUNC_LIMIT);
	goto Error;
    }
    for (i = 0; i < cond->func_count; ++i) {
	/* Find matching function */
	if (cond->callout.functions[i] == func) {
	    /* Slide remaining functions over removed one */
	    for (j = i; j < cond->func_count; j++) {
		cond->callout.functions[j] =
		    cond->callout.functions[j + 1];
	    }
	    /* Let's be tidy */
	    cond->callout.functions[(int)(--cond->func_count)] = NOTIFY_FUNC_NULL;
	    /* Exit external loop */
	    break;
	}
    }
    /* Remove function list if no more interpositions */
    if (cond->func_count == 1) {
	Notify_func     func_save = *(cond->callout.functions);

	ntfy_free_functions(cond->callout.functions);
	cond->callout.function = func_save;
    }
    NTFY_END_CRITICAL;
    return (NOTIFY_OK);
Error:
    NTFY_END_CRITICAL;
    return (notify_errno);
}
