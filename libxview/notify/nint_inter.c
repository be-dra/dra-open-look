#ifndef	lint
#ifdef sccs
static char     sccsid[] = "@(#)nint_inter.c 20.13 93/06/28 Copyr 1985 Sun Micro  DRA: $Id: nint_inter.c,v 4.1 2024/03/28 18:09:08 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Nint_inter.c - Implement the nint_interpose_func private interface.
 */

#include <xview_private/ntfy.h>
#include <xview_private/ndet.h>
#include <xview_private/nint.h>

pkg_private     Notify_error
nint_interpose_func(nclient, func, type, data, use_data)
    Notify_client   nclient;
    Notify_func     func;
    NTFY_TYPE       type;
    caddr_t         data;
    int             use_data;
{
	NTFY_CLIENT    *client;
	register NTFY_CONDITION *cond;
	register int    i;

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
/* if (type == 9) { */
/* fprintf(stderr, "client %lx, cond %lx func_count %d : func %lx\n", */
/* 				nclient, cond, cond->func_count, func); */
/* 	if (cond->func_count >= 7) { */
/* 		abort(); */
/* 	} */
/* } */

	if (func == cond->callout.functions[0]) {
		extern char *xv_app_name;
		fprintf(stderr,
			"%s: same client %lx, same condition, same function %lx ???? - aborting...\n",
			xv_app_name, nclient, (unsigned long)func);
		/* das will ich wissen */
		abort();
/* 		ntfy_set_errno(NOTIFY_NO_CONDITION); */
/* 		goto Error; */
	}

	/* See if going to exceded max number of functions supported */
	/* NTFY_FUNCS_MAX ist 6 */
	if (cond->func_count + 1 > NTFY_FUNCS_MAX) {
		ntfy_set_errno(NOTIFY_FUNC_LIMIT);
		goto Error;
	}
	/* Allocate function list if this is first interposition */
	if (cond->func_count == 1) {
		Notify_func	 func_save = cond->callout.function;
		Notify_func	*functions;

		if ((functions = ntfy_alloc_functions()) == NTFY_FUNC_PTR_NULL)
			goto Error;
		functions[0] = func_save;
		cond->callout.functions = functions;
	}
	/* Slide other functions over */
	for (i = cond->func_count; i > 0; i--)
		cond->callout.functions[i] = cond->callout.functions[i - 1];
	/* Add new function to beginning of function list */
	cond->callout.functions[0] = func;
	cond->func_count++;
	NTFY_END_CRITICAL;
	return (NOTIFY_OK);
Error:
	NTFY_END_CRITICAL;
	return (notify_errno);
}
