#ifndef	lint
#ifdef sccs
static char     sccsid[] = "@(#)ndisd_wait.c 20.13 93/06/28 Copyr 1985 Sun Micro  DRA: $Id: ndisd_wait.c,v 4.2 2024/11/30 13:02:13 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */

/*
 * Ndis_d_wait.c - Default wait3 function that is a nop.
 */
#include <xview_private/ntfy.h>
#include <xview_private/ndis.h>
#include <signal.h>

/* ARGSUSED */
extern          Notify_value
notify_default_wait3(client, pid, status, rusage)
    Notify_client   client;
    int             pid;
#if !defined(SVR4) && !defined(__linux)
    union wait     *status;
#else  /* SVR4 */
    int *status;
#endif  /* SVR4 */
    struct rusage  *rusage;
{
    return (NOTIFY_IGNORED);
}
