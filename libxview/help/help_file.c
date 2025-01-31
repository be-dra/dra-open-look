#ifndef lint
char help_file_c_sccsid[] = "@(#)help_file.c 1.17 90/12/04 $Id: help_file.c,v 4.7 2024/06/26 09:03:08 dra Exp $";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <xview/sun.h>
#include <xview/pkg.h>

#include <xview_private/i18n_impl.h>
#include <xview_private/svr_impl.h>

#define DEFAULT_HELP_PATH "/usr/openwin/lib/locale:/usr/openwin/lib/help"
#define MAX_MORE_HELP_CMD 128

Xv_private char	*xv_strtok(char *, char *);

static FILE    *help_file;
static char     help_buffer[1280];

/* returns XV_OK or XV_ERROR */
    /* key;	Spot Help key */
    /* more_help; OUTPUT parameter: More Help system cmd */
static int help_search_file(char *key, char **more_help)
{
	char *entry;
	char *more_help_cmd;
	static char more_help_cmd_buffer[MAX_MORE_HELP_CMD];

	fseek(help_file, 0L, 0);

	while ((entry = fgets(help_buffer, (unsigned)sizeof(help_buffer),
							help_file)))
	{
		if (*entry++ == ':') {
			entry = xv_strtok(entry, ":\n");	/* parse Spot Help key */
			if (entry && !strcmp(entry, key)) {
				/* Found requested Spot Help key */
				more_help_cmd = xv_strtok(NULL, "\n"); /* parse More Help system
														* command */
				if (more_help_cmd) {
					strncpy(more_help_cmd_buffer, more_help_cmd,
							(size_t)MAX_MORE_HELP_CMD - 1);
					*more_help = &more_help_cmd_buffer[0];
				}
				else
					*more_help = NULL;
				return XV_OK;
			}
		}
	}

	if (0 == strncmp(key, "wsm_", 4L)) {
		SERVERTRACE((800,
				"help key '%s' missing, trying unknownWorkspaceMenu\n", key));
		return help_search_file("unknownWorkspaceMenu", more_help);
	}
	return XV_ERROR;
}

/*
 * FIX ME help_find_file is called frlom attr.c (attr_names) so we
 * can't add an extra parameter to help_find_file for the XV_LC_DISPLAY_LANG
 * so we'll use LC_MESSAGES for now
 */

Xv_private FILE * xv_help_find_file(Xv_server srv, char *filename);

Xv_private FILE * xv_help_find_file(Xv_server srv, char *filename)
{
	FILE *file_ptr;
	char *helpdir = NULL;
	char *helppath;
	char *helppath_copy;
	char *xv_lc_display_lang = "";
	extern int _xv_use_locale;

	helppath = (char *)getenv("HELPPATH");
	if (!helppath)
		helppath = DEFAULT_HELP_PATH;
	helppath_copy = (char *)xv_malloc(strlen(helppath) + 1);
	strcpy(helppath_copy, helppath);

	if (_xv_use_locale) {
		xv_lc_display_lang = (char *)xv_get(srv, XV_LC_DISPLAY_LANG);
		if (! xv_lc_display_lang)
			xv_lc_display_lang = setlocale(LC_MESSAGES, NULL);
	}

	helpdir = xv_strtok(helppath_copy, ":");
	do {
		/*  
		 * If XV_USE_LOCALE set to TRUE, look for locale specific
		 * help file first.
		 */
		if (_xv_use_locale) {
			sprintf(help_buffer, "%s/%s/help/%s", helpdir, xv_lc_display_lang,
												filename);
			if ((file_ptr = fopen(help_buffer, "r")) != NULL)
				break;
		}
		/*
		 * If locale specific help file not found or required, fallback
		 * on helpdir/filename.
		 */
		sprintf(help_buffer, "%s/%s", helpdir, filename);
		if ((file_ptr = fopen(help_buffer, "r")) != NULL) {
			break;
		}
	} while ((helpdir = xv_strtok(NULL, ":")));
	free(helppath_copy);
	return file_ptr;
}

Pkg_private int xv_help_get_arg(Xv_server srv, char *data, char **more_help);

/* returns XV_OK or XV_ERROR */
/*    data;	"file:key" */
Pkg_private int xv_help_get_arg(Xv_server srv, char *data, char **more_help)
{
	char *client;
	char data_copy[64];
	char filename[64];
	char *key;
	static char last_client[64];

	if (data == NULL)
		return XV_ERROR;	/* No key supplied */
	strncpy(data_copy, data, sizeof(data_copy));
	data_copy[sizeof(data_copy) - 1] = '\0';
	if (!(client = xv_strtok(data_copy, ":")) || !(key = xv_strtok(NULL, "")))
		return XV_ERROR;	/* No file specified in key */
	if (strcmp(last_client, client)) {
		/* Last .info filename != new .info filename */
		if (help_file) {
			fclose(help_file);
			last_client[0] = '\0';
		}
		sprintf(filename, "%s.info", client);
		help_file = xv_help_find_file(srv, filename);
		if (help_file) {
			strcpy(last_client, client);
			return help_search_file(key, more_help);
		}
		else
			return XV_ERROR;	/* Specified .info file not found */
	}
	return help_search_file(key, more_help);
}

Pkg_private char *xv_help_get_text(int use_textsw);

Pkg_private char *xv_help_get_text(int use_textsw)
{
#ifdef OW_I18N
    char           *ptr;

    while ((ptr = fgets(help_buffer, sizeof(help_buffer), help_file)) &&
		(*ptr == '#'))
			;

    return (ptr && *ptr != ':' ? ptr : NULL);
#else
	char *s, *t;
    char *ptr = fgets(help_buffer, (unsigned)sizeof(help_buffer), help_file);

	if (!ptr) return NULL;
	if (*ptr == ':' || *ptr == '#' || *ptr == ';') return NULL;

	if (ptr[strlen(ptr)-1] != '\n') {
		fprintf(stderr, "\n\n%s-%d: line not NEWLINE terminated: '%s'\n\n",
					__FILE__, __LINE__, ptr);
	}

	if (use_textsw) {
		for (s = help_buffer, t = help_buffer; *s; ) {
			if (*s == '\\') { /* we eliminate \bo, \it, \bi, \no */
				if (s[1] == 'b' && s[2] == 'o') {
					s += 3;
				}
				else if (s[1] == 'i' && s[2] == 't') {
					s += 3;
				}
				else if (s[1] == 'b' && s[2] == 'i') {
					s += 3;
				}
				else if (s[1] == 'n' && s[2] == 'o') {
					s += 3;
				}
				else {
					*t++ = *s++;
				}
			}
			else {
				*t++ = *s++;
			}
		}
		*t = '\0';
	}

    return ptr;
#endif
}
