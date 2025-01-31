#ifndef	lint
#ifdef sccs
static char     sccsid[] = "@(#)sys_read.c 20.13 93/06/28 Copyr 1985 Sun Micro  DRA: $Id: sys_read.c,v 4.1 2024/03/28 18:09:08 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */

/*
 * Sys_read.c - Real system call to read.
 */

#include <sys/syscall.h>
#include <xview_private/ntfy.h>

pkg_private int
notify_read(fd, buf, nbytes)
    int             fd;
    char           *buf;
    int             nbytes;
{
#if OVERLOAD_SYSCALLS
#ifdef DRA_IRIX
	int len = syscall(SYS_read, fd, buf, nbytes);
	if (len == -1 && geteuid() == 53) perror("syscall-read");
    return len;
#else
	extern int syscall(int, ...);

    return (syscall(SYS_read, fd, buf, nbytes));
#endif
#else /* OVERLOAD_SYSCALLS */
	return read(fd, buf, nbytes);
#endif /* OVERLOAD_SYSCALLS */
}
