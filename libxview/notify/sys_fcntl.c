#ifndef	lint
#ifdef sccs
static char     sccsid[] = "@(#)sys_fcntl.c 20.13 93/06/28 Copyr 1985 Sun Micro  DRA: $Id: sys_fcntl.c,v 4.1 2024/03/28 18:09:08 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */

/*
 * Sys_fcntl.c - Real system call to fcntl.
 */

#include <errno.h>
#include <sys/syscall.h>
#include <xview_private/ntfy.h>

pkg_private int notify_fcntl(fd, cmd, arg)
    int fd, cmd, arg;
{
#if OVERLOAD_SYSCALLS
#ifdef DRA_IRIX
	int len;

    len = syscall(SYS_fcntl, fd, cmd, arg);
	if (len == -1 && geteuid() == 53) {
		perror("syscall-fcntl");
		fprintf(stderr, "cmd was %d (%x), arg was %d (%x)\n",cmd,cmd,arg,arg);
	}
    return len;
#else
	extern int syscall(int, ...);

    return (syscall(SYS_fcntl, fd, cmd, arg));
#endif
#else /* OVERLOAD_SYSCALLS */
	return fcntl(fd, cmd, arg);
#endif /* OVERLOAD_SYSCALLS */
}
