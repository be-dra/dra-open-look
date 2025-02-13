#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)expandname.c 20.24 93/06/28 DRA: RCS $Id: expandname.c,v 4.3 2024/11/30 12:56:48 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 *
 * xv_expand_name:  Take a file name, possibly with shell meta-characters in it,
 * and expand it by using "sh -c echo filename" Return the resulting file
 * names as a dynamic struct namelist.
 *
 * free_namelist:  Free a dynamic struct namelist allocated by xv_expand_name.
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include <string.h>
#ifndef __linux
#include <sgtty.h>
#endif

#ifdef sparc
#ifdef SVR4
#include <unistd.h>
#else
#include <vfork.h>
#endif  /* SVR4 */
#endif

#ifdef SVR4
#include <sys/signal.h>
#endif  /* SVR4 */
#include <xview_private/i18n_impl.h>
#include <xview_private/portable.h>
#include <xview/expandname.h>
#include <xview/base.h>
#include <xview/xv_error.h>

#define MAXCHAR 255
#define NOSTR   ((char *) 0)

static int xv_anyof(char  *s1, char  *s2);
struct namelist *xv_mk_0list(void);
struct namelist *xv_mk_1list(char *);

struct namelist *makelist(int len, char *str);

static char    *Default_Shell = "/bin/sh";

#undef NCARGS
#define	NCARGS	10240

struct namelist *xv_expand_name(char name[])
{
    char            xnames[NCARGS];
    char            cmdbuf[BUFSIZ];
    int    pid, length;
    char  *Shell;
    int             status, pivec[2];
    char           *echo = "echo ";
    char           *trimchars = "\t \n";

    /* Strip off leading & trailing whitespace and cr's */
    while (XV_INDEX(trimchars, *name))
	name++;
    length = strlen(name);
    while (length && XV_INDEX(trimchars, name[length - 1]))
	length--;
    if (!length || (length + (int)strlen(echo) + 2) > BUFSIZ)
	return (NONAMES);

    strcpy(cmdbuf, echo);/* init cmd */
    strncat(cmdbuf, name, (size_t)length);	/* copy in trimmed file name */
    cmdbuf[strlen(echo) + length] = '\0';
    name = cmdbuf + strlen(echo);	/* get trimmed file name string */

    if (!xv_anyof(name, "~{[*?$`'\"\\"))
	return (xv_mk_1list(name));

    if (pipe(pivec) < 0) {
	perror(XV_MSG("pipe"));
	return (NONAMES);
    }
    /* sprintf(cmdbuf, "echo %s", name);	 */
    if ((pid = vfork()) == 0) {
	/* sigchild();			 */
	Shell = getenv("SHELL");
	if (Shell == NOSTR)
	    Shell = Default_Shell;
	(void) close(pivec[0]);
	(void) close(1);
	(void) dup(pivec[1]);
	(void) close(pivec[1]);
	(void) close(2);
	execl(Shell, Shell, "-c", cmdbuf, 0);
	_exit(1);
    }
    if (pid == -1) {
	perror(XV_MSG("fork"));
	(void) close(pivec[0]);
	(void) close(pivec[1]);
	return (NONAMES);
    }
    (void) close(pivec[1]);
    for (status = 1, length = 0; length < NCARGS;) {
	status = read(pivec[0], xnames + length, (size_t)(NCARGS - length));
	if (status < 0) {
	    perror(XV_MSG("read"));
	    return (NONAMES);
	}
	if (status == 0)
	    break;
	length += status;
    }
    (void) close(pivec[0]);
#ifndef SVR4
    while (wait( & status) != pid);
#else  /* SVR4 */
    while (wait( & status) != pid);
#endif  /* SVR4 */
    ;
    status &= 0377;
    if (status != 0 && status != SIGPIPE) {
	xv_error(XV_NULL,
		 ERROR_STRING, XV_MSG("\"Echo\" failed"),
		 NULL);
	return (NONAMES);
    }
    if (length == 0) {
	return (xv_mk_0list());
    }
    if (length == NCARGS) {
	char            dummy[128];

	(void) sprintf(dummy,
			XV_MSG("Buffer overflow (> %d)  expanding \"%s\""),
		       NCARGS, name);
	xv_error(XV_NULL,
		 ERROR_STRING, dummy,
		 NULL);
	return (NONAMES);
    }
    xnames[length] = '\0';
    while (length > 0 && xnames[length - 1] == '\n') {
	xnames[--length] = '\0';
    }
    return (makelist(length + 1, xnames));
}


static int xv_anyof(char  *s1, char  *s2)
{
	int c;
	char table[MAXCHAR + 1];

	for (c = 0; c <= MAXCHAR; c++)
		table[c] = '\0';
	while ((c = *s2++)) table[c] = 0177;
	while ((c = *s1++)) if (table[c]) return (1);
	return (0);
}


/*
 * Return a pointer to a dynamically allocated namelist
 *
 * First, the 2 commonest special cases:
 */


struct namelist *xv_mk_0list(void)
{
    struct namelist *result;

    result = (struct namelist *)
	xv_malloc(sizeof(int) + sizeof(char *));
    if (result == NONAMES) {
	xv_error(XV_NULL,
		 ERROR_LAYER, ERROR_SYSTEM,
		 ERROR_STRING, XV_MSG("in xv_expand_name"),
		 NULL);
    } else {
	result->count = 0;
	result->names[0] = NOSTR;
    }
    return (result);
}


struct namelist *xv_mk_1list(char *str)
{
    struct namelist *result;

    result = (struct namelist *)
	xv_malloc((unsigned)
	       sizeof(int) + 2 * sizeof(char *) + strlen(str) +1);
    if (result == NONAMES) {
	xv_error(XV_NULL,
		 ERROR_LAYER, ERROR_SYSTEM,
		 ERROR_STRING, XV_MSG("in xv_expand_name"),
		 NULL);
    } else {
	result->count = 1;
	result->names[0] = (char *) (&result->names[2]);
	result->names[1] = NOSTR;
	(void) strcpy(result->names[0], str);
    }
    return (result);
}


struct namelist *makelist(int len, char *str)
{
    register char  *cp;
    register int    count, i;
    struct namelist *result;

    if (str[0] == '\0') {
	return (NONAMES);
    }
    for (count = 1, cp = str; cp && *cp;) {
	cp = XV_INDEX(cp, ' ');
	if (cp) {
	    count += 1;
	    *cp++ = '\0';
	}
    }
    result = (struct namelist *)
	xv_malloc((unsigned)
	       sizeof(int) + (count + 1) *sizeof(char *) + len);
    if (result == NONAMES) {
	xv_error(XV_NULL,
		 ERROR_LAYER, ERROR_SYSTEM,
		 ERROR_STRING, XV_MSG("in xv_expand_name"),
		 NULL);
    } else {
	result->count = count;
	cp = (char *) (&result->names[count + 1]);
	for (i = len; i--;)
	    cp[i] = str[i];
	for (i = 0; i < count; i++) {
	    result->names[i] = cp;
	    while (*cp++);
	}
	result->names[i] = NOSTR;
    }
    return (result);
}


void
free_namelist(ptr)
    struct namelist *ptr;
{
    if (ptr)
	free((char *) ptr);
}
