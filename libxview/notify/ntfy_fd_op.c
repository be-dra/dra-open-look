#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)ntfy_fd_op.c 20.13 93/06/28  DRA: $Id: ntfy_fd_op.c,v 4.2 2024/11/30 13:02:13 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

#ifdef __linux
#include <sys/time.h>
#endif

#include <sys/types.h>
#include <xview_private/ultrix_cpt.h>
#ifdef __linux
#  include <sys/types.h>
#  ifndef howmany
#    define howmany(x, y)   (((x)+((y)-1))/(y))
#  endif
#endif

/* AND op on two fds */
int
ntfy_fd_cmp_and(a, b)
    fd_set         *a, *b;
{
    register int    i;

    for (i = 0; i < howmany(FD_SETSIZE, NFDBITS); i++)
	if (a->fds_bits[i] & b->fds_bits[i])
	    return (1);
    return (0);
}


/* OR op on two fds */
int
ntfy_fd_cmp_or(a, b)
    fd_set         *a, *b;
{
    register int    i;

    for (i = 0; i < howmany(FD_SETSIZE, NFDBITS); i++)
	if (a->fds_bits[i] | b->fds_bits[i])
	    return (1);
    return (0);
}


/* Are any of the bits set */
int
ntfy_fd_anyset(a)
    fd_set         *a;
{
    register int    i;

    for (i = 0; i < howmany(FD_SETSIZE, NFDBITS); i++)
	if (a->fds_bits[i])
	    return (1);
    return (0);
}


/* Return OR of two fd's */
fd_set
* ntfy_fd_cpy_or(a, b)
    fd_set         *a, *b;
{
    register int    i;

    for (i = 0; i < howmany(FD_SETSIZE, NFDBITS); i++)
	a->fds_bits[i] = a->fds_bits[i] | b->fds_bits[i];
    return (a);
}


/* Return AND of two fd's */
fd_set
* ntfy_fd_cpy_and(a, b)
    fd_set         *a, *b;
{
    register int    i;

    for (i = 0; i < howmany(FD_SETSIZE, NFDBITS); i++)
	a->fds_bits[i] = a->fds_bits[i] & b->fds_bits[i];
    return (a);
}




/* Return XOR of two fd's */
fd_set
* ntfy_fd_cpy_xor(a, b)
    fd_set         *a, *b;
{
    register int    i;

    for (i = 0; i < howmany(FD_SETSIZE, NFDBITS); i++)
	a->fds_bits[i] = a->fds_bits[i] ^ b->fds_bits[i];
    return (a);
}
