#ifndef	lint
#ifdef sccs
static char     sccsid[] = "@(#)sys_select.c 20.17 93/06/28 Copyr 1985 Sun Micro  DRA: $Id: sys_select.c,v 4.1 2024/03/28 18:09:08 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */

/*
 * Sys_select.c - Real system call to select.
 */

#if defined(SVR4)
#  define DRA_USE_POLL 1
#else
#  define DRA_USE_POLL 0
#endif

#if DRA_USE_POLL
#  include <values.h>
#  include <sys/time.h>
#  include <sys/types.h>
#  include <sys/syscall.h>
#  include <sys/poll.h>
#  include <sys/select.h>
#else
#  include <syscall.h>
#endif
#include <xview_private/ntfy.h>	/* For ntfy_assert */
#include <xview_private/ndet.h>	/* For ntfy_assert */
#include <errno.h>		/* For debugging */
#include <stdio.h>		/* For debugging */

#ifndef POLLRDBAND
#  define POLLRDBAND POLLPRI
#endif
#ifndef NULL
#define NULL	0
#endif

pkg_private int notify_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *tv)
{
#if ! DRA_USE_POLL
#if OVERLOAD_SYSCALLS
	extern int syscall(int, ...);

    nfds = syscall(SYS_select, nfds, readfds, writefds, exceptfds, tv);
/*     ntfy_assert(!(nfds == 0 && tv == (struct timeval *) 0 && */
/* 		  *readfds == 0 && *writefds == 0 && *exceptfds == 0), 39 */
		/* SYS_select returned when no stimuli */
/* 		); */
    return nfds;
#else /* OVERLOAD_SYSCALLS */
	return select(nfds, readfds, writefds, exceptfds, tv);
#endif /* OVERLOAD_SYSCALLS */
#else /* DRA_USE_POLL */
    /* register declarations ordered by expected frequency of use */
    register long *in, *out, *ex;
    register u_long m;	/* bit mask */
    register int j;		/* loop counter */
    register u_long b;	/* bits to test */
    register int n, rv, ms;
    struct pollfd pfd[FD_SETSIZE];
    register struct pollfd *p = pfd;
    int lastj = -1;
    /* "zero" is read-only, it could go in the text segment */
    static fd_set zero = { 0 };

    /*
     * If any input args are null, point them at the null array.
     */
    if (readfds == NULL)
	    readfds = &zero;
    if (writefds == NULL)
	    writefds = &zero;
    if (exceptfds == NULL)
	    exceptfds = &zero;

    /*
     * For each fd, if any bits are set convert them into
     * the appropriate pollfd struct.
     */
    in = (long *)readfds->fds_bits;
    out = (long *)writefds->fds_bits;
    ex = (long *)exceptfds->fds_bits;
    for (n = 0; n < nfds; n += NFDBITS) {
	    b = (u_long)(*in | *out | *ex);
	    for (j = 0, m = 1; b != 0; j++, b >>= 1, m <<= 1) {
		    if (b & 1) {
			    p->fd = n + j;
			    if (p->fd >= nfds)
				    goto done;
			    p->events = 0;
			    if (*in & m)
				    p->events |= POLLIN;
			    if (*out & m)
				    p->events |= POLLOUT;
			    if (*ex & m)
				    p->events |= POLLRDBAND;
			    p++;
		    }
	    }
	    in++;
	    out++;
	    ex++;
    }
done:
    /*
     * Convert timeval to a number of millseconds.
     * Test for zero cases to avoid doing arithmetic.
     * XXX - this is very complex, is it worth it?
     */
    if (tv == NULL) {
	    ms = -1;
    } else if (tv->tv_sec == 0) {
	    if (tv->tv_usec == 0) {
		    ms = 0;
	    } else if (tv->tv_usec < 0 || tv->tv_usec > 1000000) {
		    errno = EINVAL;
		    return (-1);
	    } else {
		    /*
		     * lint complains about losing accuracy,
		     * but I know otherwise.  Unfortunately,
		     * I can't make lint shut up about this.
		     */
		    ms = (int)(tv->tv_usec / 1000);
	    }
    } else if (tv->tv_sec > (MAXINT) / 1000) {
	    if (tv->tv_sec > 100000000) {
		    errno = EINVAL;
		    return (-1);
	    } else {
		    ms = MAXINT;
	    }
    } else if (tv->tv_sec > 0) {
	    /*
	     * lint complains about losing accuracy,
	     * but I know otherwise.  Unfortunately,
	     * I can't make lint shut up about this.
	     */
	    ms = (int)((tv->tv_sec * 1000) + (tv->tv_usec / 1000));
    } else {	/* tv_sec < 0 */
	    errno = EINVAL;
	    return (-1);
    }

    /*
     * Now do the poll.
     */
    n = p - pfd;		/* number of pollfd's */
    rv = syscall(SYS_poll, pfd, (u_long)n, ms);
	/* da kommt bei linux ENOSYS raus */
    if (rv < 0)		/* let 0 drop through so fs_set's get 0'ed */
	    return (rv);

    /*
     * Convert results of poll back into bits
     * in the argument arrays.
     *
     * We assume POLLIN, POLLOUT, and POLLRDBAND will only be set
     * on return from poll if they were set on input, thus we don't
     * worry about accidentally setting the corresponding bits in the
     * zero array if the input bit masks were null.
     */
    for (p = pfd; n-- > 0; p++) {
	    j = p->fd / NFDBITS;
	    /* have we moved into another word of the bit mask yet? */
	    if (j != lastj) {
		    /* clear all output bits to start with */
		    in = (long *)&readfds->fds_bits[j];
		    out = (long *)&writefds->fds_bits[j];
		    ex = (long *)&exceptfds->fds_bits[j];
		    /*
		     * In case we made "zero" read-only (e.g., with
		     * cc -R), avoid actually storing into it.
		     */
		    if (readfds != &zero)
			    *in = 0;
		    if (writefds != &zero)
			    *out = 0;
		    if (exceptfds != &zero)
			    *ex = 0;
		    lastj = j;
	    }
	    if (p->revents) {
		    /*
		     * select will return EBADF immediately if any fd's
		     * are bad.  poll will complete the poll on the
		     * rest of the fd's and include the error indication
		     * in the returned bits.  This is a rare case so we
		     * accept this difference and return the error after
		     * doing more work than select would've done.
		     */
		    if (p->revents & POLLNVAL) {
			    errno = EBADF;
			    return (-1);
		    }

		    m = 1 << (p->fd % NFDBITS);
		    if (p->revents & POLLIN)
			    *in |= m;
		    if (p->revents & POLLOUT)
			    *out |= m;
		    if (p->revents & POLLRDBAND)
			    *ex |= m;
		    /*
		     * Only set this bit on return if we asked about
		     * input conditions.
		     */
		    if ((p->revents & (POLLHUP|POLLERR)) &&
			(p->events & POLLIN))
			    *in |= m;
		    /*
		     * Only set this bit on return if we asked about
		     * output conditions.
		     */
		    if ((p->revents & (POLLHUP|POLLERR)) &&
			(p->events & POLLOUT))
			    *out |= m;
	    }
    }
    ntfy_assert(!(nfds == 0 && tv == (struct timeval *) 0), 40
	    /* select returned when no stimuli */);
    return (rv);
#endif  /* DRA_USE_POLL */
}
