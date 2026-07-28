#ifndef lint
char     es_cp_file_c_sccsid[] = "@(#)es_cp_file.c 20.33 93/06/28 DRA: $Id: es_cp_file.c,v 4.3 2026/07/27 17:05:26 dra Exp $";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Routines to copy a file.  Stolen from cp.c, then modified.
 */
#include <stdio.h>
#include <sys/param.h>
#include <sys/stat.h>
#ifdef SVR4
#  include <sys/types.h>
#  include <unistd.h>
#  include <dirent.h>
#  include <netdb.h>
#else /* SVR4 */
#  include <sys/dir.h>
#  include <sys/file.h>
#endif /* SVR4 */
#include <unistd.h>
#include <fcntl.h>
#include <xview_private/i18n_impl.h>
#include <xview_private/portable.h>
#include <xview_private/es.h>
#include <xview/pkg.h>
#include <xview/attrol.h>

#ifndef BSIZE 
#define	BSIZE	8192
#endif /* BSIZE */

Pkg_private int es_copy_file(char *from, char *to)
{
    int             from_fd, result;

    from_fd = open(from, O_RDONLY);

    if (from_fd < 0) return 1;
    result = es_copy_fd(from, to, from_fd);
    close(from_fd);
    return (result);
}

#define FSTAT_FAILED	-1
#define WILL_OVERWRITE	 1
Pkg_private int es_copy_status(char *to, int fold, int *from_mode)
{
    struct stat     stfrom, stto;

    if (fstat(fold, &stfrom) < 0)
	return (FSTAT_FAILED);
    if (stat(to, &stto) >= 0) {
	if (stfrom.st_dev == stto.st_dev &&
	    stfrom.st_ino == stto.st_ino) {
	    return (WILL_OVERWRITE);
	}
    }
    *from_mode = (int) stfrom.st_mode;
    return (0);
}

#define	es_Perror(s)

Pkg_private int es_copy_fd(char *from, char *to, int fold)
{
	int fnew, fnew_mode, n;
	struct stat stto;
	char *last, destname[BSIZE], buf[BSIZE];

	if (stat(to, &stto) >= 0 && (stto.st_mode & S_IFMT) == S_IFDIR) {
		last = (char *)XV_RINDEX(from, '/');
		if (last)
			last++;
		else
			last = from;
		if ((int)strlen(to) + (int)strlen(last) >= BSIZE - 1) {
			return (1);
		}
		sprintf(destname, "%s/%s", to, last);
		to = destname;
	}
	switch (es_copy_status(to, fold, &fnew_mode)) {
		case FSTAT_FAILED:
			es_Perror(from);
			return (1);
		case WILL_OVERWRITE:
			return (1);
		default:
			break;
	}
	fnew = creat(to, (unsigned)fnew_mode);
	if (fnew < 0) {
		es_Perror(to);
		return (1);
	}
	for (;;) {
		n = read(fold, buf, (size_t)BSIZE);
		if (n == 0)
			break;
		if (n < 0) {
			es_Perror(from);
			(void)close(fnew);
			return (1);
		}
		if (write(fnew, buf, (size_t)n) != n) {
			es_Perror(to);
			(void)close(fnew);
			return (1);
		}
	}
	(void)close(fnew);
	return (0);
}
