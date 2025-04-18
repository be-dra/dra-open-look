#ifndef	lint
#ifdef sccs
static char     sccsid[] = "@(#)ndis_d_pri.c 20.16 93/06/28 Copyr 1985 Sun Micro  DRA: $Id: ndis_d_pri.c,v 4.2 2024/11/30 13:02:13 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Ndis_d_pri.c - Default prioritizer for dispatcher.
 */
#include <xview_private/ntfy.h>
#include <xview_private/ndis.h>
#include <signal.h>
#if defined(__linux)
#  include <sys/types.h>
#  ifndef howmany
#    define howmany(x, y)   (((x)+((y)-1))/(y))
#  endif
#  ifndef NBBY
#    define NBBY 8
#  endif
#endif

typedef enum notify_error (*Notify_error_func) (Notify_client, int);

static void ndis_send_ascending_sig(Notify_client nclient,
			int nbits, sigset_t *bits_ptr, Notify_error_func func);
static void ndis_send_ascending_fd(Notify_client nclient, int nbits,
 			fd_set *bits_ptr, Notify_error_func func);

pkg_private     Notify_value
ndis_default_prioritizer(Notify_client nclient, int nfd, fd_set *ibits_ptr,
						fd_set *obits_ptr, fd_set *ebits_ptr, int nsig,
						sigset_t *sigbits_ptr, sigset_t *auto_sigbits_ptr,
						int *event_count_ptr, Notify_event *events,
						Notify_arg *args)
{
    register int    i;

    if (!sigisempty( auto_sigbits_ptr )) {
	/* Send itimers */
	if (sigismember( auto_sigbits_ptr, SIGALRM )) {
	    (void) notify_itimer(nclient, ITIMER_REAL);
	    sigdelset( auto_sigbits_ptr, SIGALRM );
	}
	if (sigismember( auto_sigbits_ptr, SIGVTALRM )) {
	    (void) notify_itimer(nclient, ITIMER_VIRTUAL);
	    sigdelset( auto_sigbits_ptr, SIGVTALRM );
	}
	/* Send child process change */
	if (sigismember( auto_sigbits_ptr, SIGCHLD )) {
	    (void) notify_wait3(nclient);
	    sigdelset( auto_sigbits_ptr, SIGCHLD );
	}
    }
    if (!sigisempty(sigbits_ptr))
	/* Send signals (by ascending signal numbers) */
	ndis_send_ascending_sig(nclient, nsig, sigbits_ptr,
				notify_signal);
    if (ntfy_fd_anyset(ebits_ptr))
	/* Send exception fd activity (by ascending fd numbers) */
	ndis_send_ascending_fd(nclient, nfd, ebits_ptr,
			       notify_exception);
    /* Send client event (in order received) */
    for (i = 0; i < *event_count_ptr; i++)
	(void) notify_event(nclient, *(events + i), *(args + i));
    *event_count_ptr = 0;
    if (ntfy_fd_anyset(obits_ptr))
	/* Send output fd activity (by ascending fd numbers) */
	ndis_send_ascending_fd(nclient, nfd, obits_ptr, notify_output);
    if (ntfy_fd_anyset(ibits_ptr))
	/* Send input fd activity (by ascending fd numbers) */
	ndis_send_ascending_fd(nclient, nfd, ibits_ptr, notify_input);

    if (!sigisempty( auto_sigbits_ptr )) {
	/* Send destroy checking */
	if (sigismember( auto_sigbits_ptr, SIGTSTP )) {
	    if ((notify_destroy(nclient, DESTROY_CHECKING) ==
		 NOTIFY_DESTROY_VETOED) &&
		((sigismember( auto_sigbits_ptr, SIGTERM )) ||
		 (sigismember( auto_sigbits_ptr, SIGKILL )))) {
		/* Remove DESTROY_CLEANUP from dispatch list. */
		notify_flush_pending(nclient);
		/* Prevent DESTROY_CLEANUP in this call */
	        sigdelset( auto_sigbits_ptr, SIGTERM );
	        sigdelset( auto_sigbits_ptr, SIGKILL );
	    }
	    sigdelset( auto_sigbits_ptr, SIGTSTP );
	}
	/* Send destroy (only one of them) */
	if (sigismember( auto_sigbits_ptr, SIGTERM )) {
	    (void) notify_destroy(nclient, DESTROY_CLEANUP);
	    sigdelset( auto_sigbits_ptr, SIGTERM );
	} else if (sigismember( auto_sigbits_ptr, SIGKILL )) {
	    (void) notify_destroy(nclient, DESTROY_PROCESS_DEATH);
	    sigdelset( auto_sigbits_ptr, SIGKILL );
	} else if (sigismember( auto_sigbits_ptr, SIGUSR1 )) {
	    (void) notify_destroy(nclient, DESTROY_SAVE_YOURSELF);
	    sigdelset( auto_sigbits_ptr, SIGUSR1 );
	}
    }
    return (NOTIFY_DONE);
}

static void ndis_send_ascending_fd(Notify_client nclient,
    int nbits, fd_set *bits_ptr, Notify_error_func func)
{
	int fd, i, byteNum;
	unsigned long byte;

	/* Send fd (by ascending numbers) */
	for (i = 0; i < howmany(nbits, NFDBITS); i++)
		if (bits_ptr->fds_bits[i])
			/* For each fd_mask set in bits_ptr, mask off all but  */
			/* one byte and see if anything is set.                */
			for (byte = 0xff, byteNum = 0; byte != 0L; byte <<= NBBY, byteNum++)
				if (bits_ptr->fds_bits[i] & byte)
					/* If a byte is set, find out which bit is set. */
					for (fd = byteNum * NBBY + i * NFDBITS;
							fd < byteNum * NBBY + i * NFDBITS + NBBY; fd++)
						if (FD_ISSET(fd, bits_ptr)) {
							(void)func(nclient, fd);
							FD_CLR(fd, bits_ptr);
						}
}

static void ndis_send_ascending_sig(Notify_client   nclient,
    int    nbits, sigset_t    *bits_ptr, Notify_error_func func)
{
	int sig;

	/* Send func (by ascending numbers) */
	for (sig = 1; sig < nbits; sig++) {
		if (sigismember(bits_ptr, sig)) {
			(void)func(nclient, sig);
			sigdelset(bits_ptr, sig);
		}
	}
}
