#ifndef	lint
#ifdef sccs
static char     sccsid[] = "@(#)ndet_die.c 20.14 93/06/28 Copyr 1985 Sun Micro DRA: $Id: ndet_die.c,v 4.1 2024/03/28 18:09:08 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Ndet_die.c - Notify_die implementation.
 */

#include <xview_private/ntfy.h>
#include <xview_private/ndet.h>
#include <xview_private/ndis.h>

extern int ntfy_errno_no_print;


/*
 * Remove each client.
 */
/* ARGSUSED */
static NTFY_ENUM ndet_remove_all(NTFY_CLIENT    *client,
    NTFY_CONDITION *condition, NTFY_ENUM_DATA  context)
{
    (void) notify_remove(client->nclient);
    return (NTFY_ENUM_SKIP);
}

Notify_error notify_die(status)
    Destroy_status  status;
{
    NTFY_ENUM       enum_code;
    Notify_error    return_code;

    if (ndet_check_status(status))
	return (NOTIFY_INVAL);
    NTFY_BEGIN_CRITICAL;

    /* 1067273 */
    ntfy_errno_no_print=1;
    /* Call all destroy procs (go around entire dispatch mechanism) */
    enum_code = ntfy_paranoid_enum_conditions(ndet_clients,
			   ndet_immediate_destroy, (NTFY_ENUM_DATA) status);
    ntfy_errno_no_print=0;
    /* If checking then return result */
    return_code = NOTIFY_OK;
    if (status == DESTROY_CHECKING) {
	if (enum_code == NTFY_ENUM_TERM)
	    return_code = NOTIFY_DESTROY_VETOED;
    } else if (status != DESTROY_SAVE_YOURSELF)
	/* else remove all clients */
	(void) ntfy_paranoid_enum_conditions(ndet_clients,
				      ndet_remove_all, NTFY_ENUM_DATA_NULL);
    NTFY_END_CRITICAL;
    return (return_code);
}
