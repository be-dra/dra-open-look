#ifndef	lint
#ifdef sccs
static char     sccsid[] = "@(#)nint_set.c 20.12 93/06/28 Copyr 1985 Sun Micro  DRA: $Id: nint_set.c,v 4.1 2024/03/28 18:09:08 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Nint_set.c - Implement the nint_set_func private interface.
 */

#include <xview_private/ntfy.h>
#include <xview_private/ndet.h>
#include <xview_private/nint.h>

pkg_private     Notify_func nint_set_func(NTFY_CONDITION *cond,
					    Notify_func new_func)
{
	Notify_func old_func;

	if (cond->func_count > 1) {
		old_func = cond->callout.functions[cond->func_count - 1];
		cond->callout.functions[cond->func_count - 1] = new_func;
	}
	else {
		old_func = cond->callout.function;
		cond->callout.function = new_func;
		cond->func_count = 1;
	}
	return (old_func);
}
