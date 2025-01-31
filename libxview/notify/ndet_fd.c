#ifndef	lint
#ifdef sccs
static char     sccsid[] = "@(#)ndet_fd.c 20.14 93/06/28 Copyr 1985 Sun Micro DRA: $Id: ndet_fd.c,v 4.1 2024/03/28 18:09:08 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */

/*
 * Ndet_fd.c - Implement file descriptor specific calls that are shared among
 * NTFY_INPUT, NTFY_OUTPUT and NTFY_EXCEPTION.
 */

#include <xview_private/ntfy.h>
#include <xview_private/ndet.h>
#include <unistd.h>

/* performance: global cache of getdtablesize() */
extern int      dtablesize_cache;
#ifdef SVR4
#define GETDTABLESIZE() \
(dtablesize_cache?dtablesize_cache:(dtablesize_cache=(int)sysconf(_SC_OPEN_MAX)))
#else
#define GETDTABLESIZE() \
    (dtablesize_cache?dtablesize_cache:(dtablesize_cache=getdtablesize()))
#endif  /* SVR4 */

static int      ndet_fd_table_size;	/* Number of descriptor slots
					 * available */

pkg_private int
ndet_check_fd(int fd)
{
	if (ndet_fd_table_size == 0)
		ndet_fd_table_size = GETDTABLESIZE();
	if (fd < 0 || fd >= ndet_fd_table_size) {
		ntfy_set_errno(NOTIFY_BADF);
		return (-1);
	}
	return (0);
}
