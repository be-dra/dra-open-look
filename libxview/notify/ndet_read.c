#ifndef	lint
#ifdef sccs
static char     sccsid[] = "@(#)ndet_read.c 20.12 93/06/28 Copyr 1985 Sun Micro DRA: $Id: ndet_read.c,v 4.1 2024/03/28 18:09:08 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Ndet_read.c - Notifier's version of read.  Will do notification cycle if
 * not already in the middle of it.
 */

#include <xview_private/ntfy.h>
#include <xview_private/ndet.h>
#include <xview_private/ndis.h>	/* For ndis_client == NTFY_CLIENT_NULL check */
#include <errno.h>

#if OVERLOAD_SYSCALLS
extern int      notify_exclude_fd;


static int      ndet_read_done;

static Notify_value ndet_read_in_func(Notify_client nclient, int fd);
static Notify_client ndet_read_nclient = (Notify_client) & ndet_read_done;

int read(fd, buf, nbytes)
    register int    fd;
    char           *buf;
    int             nbytes;
{
    int             ndelay;
    Notify_error    return_code;

    /*
     * Do real read if in middle of notification loop or no other clients and
     * no notifications pending. Also, don't dispatch events if haven't
     * started notifier and "background" dispatching hasn't been turned on.
     */
/*printf("fd==%d, nfd==%d.\n", fd, notify_exclude_fd);*/
    if ((ndet_flags & NDET_STARTED) ||
	((!(ndet_flags & NDET_STARTED)) &&
	 (!(ndet_flags & NDET_DISPATCH))) ||
	((ndet_clients == NTFY_CLIENT_NULL) &&
         (ndis_clients == NTFY_CLIENT_NULL)) ||
        (fd == notify_exclude_fd))
	return (notify_read(fd, buf, nbytes));
    /* Set up read condition */
    if (notify_set_input_func(ndet_read_nclient, ndet_read_in_func, fd) ==
	NOTIFY_FUNC_NULL && notify_errno == NOTIFY_BADF) {
	errno = EBADF;
	return (-1);
    }
    /* Set up variable that will notice the notification */
    ndet_read_done = 0;
    /* Setup flag to break out of notify_start if fd is non-blocking */
    /* if (ndelay = (FD_BIT(fd) & ndet_fndelay_mask)) */
    if (ndelay = FD_ISSET(fd, &ndet_fndelay_mask))
	ndet_flags |= NDET_NO_DELAY;
    /*
     * Start notifier.  Wouldn't return until (1) input available on fd, (2)
     * some other notifier client calls notify_stop(), (3) there is an error
     * during the real select call (e.g., EWOULDBLOCK, EBADF, EINTR on slow
     * fd).  Note: Using errno from the real select as return value for read.
     * Will this work in all cases?
     */
    return_code = notify_start();
    ndet_flags &= ~NDET_NO_DELAY;
    /* Clear read condition from notifier */
    (void) notify_set_input_func(ndet_read_nclient, NOTIFY_FUNC_NULL, fd);
    /* If no read notification then assume error return from real read */
    if (ndet_read_done) {
	/* Do real read */
	return (notify_read(fd, buf, nbytes));
    } else {
	if (return_code == NOTIFY_OK && ndelay)
	    errno = EWOULDBLOCK;
	return (-1);
    }
}
/* ARGSUSED */
static Notify_value ndet_read_in_func(Notify_client nclient, int fd)
{
    ndet_read_done = 1;
    (void) notify_stop();
    return (NOTIFY_DONE);
}

#else /* OVERLOAD_SYSCALLS */

extern void das_brauch_ich_nie(void);

void das_brauch_ich_nie(void)
{
}
#endif /* OVERLOAD_SYSCALLS */
