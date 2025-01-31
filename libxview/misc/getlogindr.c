#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)getlogindr.c 20.18 93/06/28 Copyr 1984 Sun Micro DRA: RCS $Id: getlogindr.c,v 4.2 2024/05/03 11:03:31 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * xv_getlogindir - Get login directory.  First try HOME environment variable.
 * Next try password file.  Print message if can't get login directory.
 */

#include <stdio.h>
#include <pwd.h>
#include <unistd.h>
#include <xview_private/i18n_impl.h>
#include <xview/xv_error.h>

char *xv_getlogindir(void);


char *xv_getlogindir(void)
{
	struct passwd *passwdent;
	char *home, *loginname;

	home = getenv("HOME");
	if (home != NULL) return home;
	loginname = getlogin();
	if (loginname == NULL) passwdent = getpwuid(getuid());
	else passwdent = getpwnam(loginname);
	if (passwdent == NULL) {
		xv_error(XV_NULL,
				ERROR_STRING,
				XV_MSG("xv_getlogindir: couldn't find user in password file"),
				NULL);
		return NULL;
	}
	if (passwdent->pw_dir == NULL) {
		xv_error(XV_NULL,
				ERROR_STRING,
				XV_MSG("xv_getlogindir: no home directory in password file"),
				NULL);
		return NULL;
	}
	return passwdent->pw_dir;
}
