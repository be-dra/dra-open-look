#ifndef	lint
#ifdef sccs
static char     sccsid[] = "@(#)ndet_death.c 20.13 93/06/28 Copyr 1985 Sun Micro DRA: $Id: ndet_death.c,v 4.1 2024/03/28 18:09:08 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Ndet_death.c - Common routines between notify_post_destroy and notify_die.
 */

#include <xview_private/ntfy.h>
#include <xview_private/ndet.h>
#include <xview_private/ndis.h>
#include <xview_private/nint.h>

pkg_private int ndet_check_status(Destroy_status  status)
{
	if (!(status == DESTROY_PROCESS_DEATH || status == DESTROY_CHECKING ||
					status == DESTROY_CLEANUP
					|| status == DESTROY_SAVE_YOURSELF)) {
		ntfy_set_errno(NOTIFY_INVAL);
		return (-1);
	}
	return (0);
}

/*
 * Immediate destroy notification.
 */
pkg_private NTFY_ENUM ndet_immediate_destroy(NTFY_CLIENT *client,
    NTFY_CONDITION *condition,
    NTFY_ENUM_DATA  context)
{
    Destroy_status  status = (Destroy_status)((long) context);

    if (condition->type == NTFY_DESTROY) {
	Notify_func     func = nint_push_callout(client, condition);

	/* Send destroy notification */
	ndet_flags &= ~NDET_VETOED;
	NTFY_END_CRITICAL;
	(void) func(client->nclient, status);
	NTFY_BEGIN_CRITICAL;
	nint_unprotected_pop_callout();
	/* Return NTFY_ENUM_TERM if checking and was told to veto */
	if (status == DESTROY_CHECKING && (ndet_flags & NDET_VETOED))
	    return (NTFY_ENUM_TERM);
	else
	    /*
	     * Since only one destroy per client can skip other conditions.
	     */
	    return (NTFY_ENUM_SKIP);
    }
    return (NTFY_ENUM_NEXT);
}
