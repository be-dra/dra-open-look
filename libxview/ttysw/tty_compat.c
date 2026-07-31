#ifndef lint
char     tty_compat_c_sccsid[] = "@(#)tty_compat.c 20.21 93/06/28 RCS: $Id: tty_compat.c,v 4.4 2026/07/30 12:06:21 dra Exp $";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */

/*
 * Compatibility bridge for SunView1.X programs.
 */

#include <sys/types.h>
#include <sys/time.h>
#include <xview_private/i18n_impl.h>
#include <xview_private/tty_impl.h>
#include <xview_private/term_impl.h>

extern void ttysw_becomeconsole(Tty ttysw0);

Sv1_public void ttysw_becomeconsole(Tty ttysw0)
{
    xv_set(ttysw0, TTY_CONSOLE, TRUE, NULL);
}

extern int ttysw_cmd(caddr_t ttysw_opaque, char *buf, int buflen);

/* NOT USED */
int ttysw_cmd(caddr_t ttysw_opaque, char *buf, int buflen)
{
    int             result;

    (void) xv_set((Xv_opaque)ttysw_opaque, TTY_INPUT, buf, buflen, &result, NULL);
    return (result);
}

extern  int ttysw_input(caddr_t ttysw0, char *addr, int len);

Xv_public int ttysw_input(caddr_t ttysw0, char *addr, int len)
{
    return (ttysw_input_it(TTY_PRIVATE_FROM_ANY_PUBLIC(ttysw0), addr, len));
}
