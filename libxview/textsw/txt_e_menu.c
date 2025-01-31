#ifndef lint
char     txt_e_menu_c_sccsid[] = "@(#)txt_e_menu.c 20.50 93/06/28 DRA: $Id: txt_e_menu.c,v 4.10 2024/11/14 15:49:27 dra Exp $";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Allow a user definable "Extras" menu in the textsw.  The search path to
 * find the name of the file to initially look in is:
 *
 *  1. text.extrasMenuFilename{.<locale>} (Xdefaults)
 *  2. $(EXTRASMENU){.<locale>} (environment variable),
 *  3. $(HOME)/.text_extras_menu{.<locale>} (home dir),
 *  4. locale sensitive system default
 *		("$OPENWINHOME/lib/locale/<locale>/XView/.text_extras_menu")
 *  5. fall back to SunView1 ("/usr/lib/.text_extras_menu")
 *
 * Always try locale specific name first, if not there, try without
 * locale name.  In the #4, we will fall back to the "C" locale.
 * Much of this code was borrowed from the suntools dynamic rootmenu code.
 */

#include <stdlib.h>
#include <sys/types.h>
#include <sys/file.h>
#include <xview_private/primal.h>
#include <xview_private/txt_impl.h>
#include <errno.h>
#include <ctype.h>
#include <xview/openmenu.h>
#include <xview/defaults.h>
#include <xview/wmgr.h>
#include <xview/icon.h>
#include <xview/icon_load.h>
#include <sys/stat.h>
#include <string.h>
#include <xview/str_utils.h>
#include <unistd.h>

#define	ERROR	-1

#define	MAX_FILES	40
#ifndef MAXPATHLEN
#  define MAXPATHLEN	1024
#endif
#define	EXTRASMENU	"text_extras_menu"
#define	MAXSTRLEN	256
#define	MAXARGS		20

struct stat_rec {
    char           *name;	/* Dynamically allocated menu file name */
    time_t          mftime;	/* Modified file time */
};

static struct stat_rec Extras_stat_array[MAX_FILES];
static int      Textsw_nextfile;

extern void expand_path(char *nm, char *buf);

static char *textsw_savestr(char *s);
static char *textsw_save2str(char *s, char *t);
static void free_argv(char **argv);
static char * check_filename_locale(char *locale, char *filename, int copy);
static Menu_item textsw_handle_extras_menuitem(Menu menu, Menu_item item);
static int walk_getmenu(Textsw_view textsw_view, Menu m, char *file,
    									FILE *mfd, int *lineno);
static	int      Nargs;

extern int      EXTRASMENU_FILENAME_KEY;

Pkg_private char *textsw_get_extras_filename(Xv_server srv, Menu_item mi)
{
	char *filename;
	char full_file[MAXPATHLEN];
	char *locale;
	char *result;
	char tmp[MAXPATHLEN + 1];


	filename = (char *)xv_get(mi, XV_KEY_DATA, EXTRASMENU_FILENAME_KEY);
	if (filename && *filename) return filename;

	locale = (char *)xv_get(srv, XV_LC_DISPLAY_LANG);
	if (! locale) locale = setlocale(LC_MESSAGES, NULL);

	filename = defaults_get_string("text.extrasMenuFilename",
									"Text.ExtrasMenuFilename", NULL);
	if (filename && (int)strlen(filename) > 0) {
		expand_path(filename, full_file);
		if ((result = check_filename_locale(locale, full_file, TRUE)) != NULL)
			goto found;
	}

	if ((filename = getenv("EXTRASMENU")) != NULL
			&& (result = check_filename_locale(locale, filename, FALSE))!=NULL)
		goto found;

	/*
	 * FIX_ME: Using $HOME might not be a very portable way to to find
	 * out the home directory when port to the various UNIX flavors.
	 */
	if ((filename = getenv("HOME")) != NULL) {
		(void)sprintf(tmp, "%s/.%s", filename, EXTRASMENU);
		if ((result = check_filename_locale(locale, tmp, TRUE)) != NULL)
			goto found;
	}

	if ((filename = getenv("OPENWINHOME")) != NULL) {
		(void)sprintf(tmp, "%s/lib/locale/%s/xview/.%s",
				filename, locale, EXTRASMENU);
		if ((result = check_filename_locale(NULL, tmp, TRUE)) != NULL)
			goto found;

		if (strcmp(locale, "C") != 0) {
			(void)sprintf(tmp, "%s/lib/locale/C/xview/.%s",
					filename, EXTRASMENU);
			if ((result = check_filename_locale(NULL, tmp, TRUE)) != NULL)
				goto found;
		}
	}

	/* Giving up, try with ancient way (SunView1) */
	(void)sprintf(tmp, "/usr/lib/.%s", EXTRASMENU);
	result = xv_strsave(tmp);

  found:
	xv_set(mi,
			XV_KEY_DATA, EXTRASMENU_FILENAME_KEY, result,
			XV_KEY_DATA_REMOVE_PROC, EXTRASMENU_FILENAME_KEY, textsw_free_help,
			NULL);

	return result;
}


static char *check_filename_locale(char *locale, char *filename, int copy)
{
    char	tmp[MAXPATHLEN + 1];
	int fd_to_be_closed;

    if ((int)strlen(filename) <= 0) return NULL;

	if (locale != NULL) {
		snprintf(tmp, sizeof(tmp)-1, "%s.%s", filename, locale);
		if ((fd_to_be_closed = open(tmp, O_RDONLY)) != -1) {
			close(fd_to_be_closed);
    		filename = xv_strsave(tmp);
			return filename;
		}
	}

    if ((fd_to_be_closed = open(filename, O_RDONLY)) == -1) return NULL;

	close(fd_to_be_closed);
    if (copy) filename = xv_strsave(filename);

    return filename;
}


static int extras_menufile_changed(void)
{
    int             i;
    struct stat     statb;

    /* Whenever an existing menu goes up, stat menu files */
    for (i = 0; i < Textsw_nextfile; i++) {
	if (stat(Extras_stat_array[i].name, &statb) < 0) {
	    if (errno == ENOENT)
		return (TRUE);
	    xv_error(XV_NULL,
		     ERROR_LAYER, ERROR_SYSTEM,
		     ERROR_STRING, Extras_stat_array[i].name,
		     ERROR_PKG, TEXTSW,
		     NULL);
	    return (ERROR);
	}
	if (statb.st_mtime > Extras_stat_array[i].mftime)
	    return (TRUE);
    }

    return (FALSE);
}

static void textsw_remove_all_menu_items(Menu menu)
{
    int             n = (int) xv_get(menu, MENU_NITEMS);
    Menu_item       mi;
    int             i;

    if (!menu)
	return;

    for (i = n; i >= 1; i--) {
	mi = xv_get(menu, MENU_NTH_ITEM, i);
	xv_set(menu, MENU_REMOVE_ITEM, mi, NULL);
	xv_destroy(mi);
    }
}

/*
 * Check to see if there is a valid extrasmenu file.  If the file
 * exists then turn on the Extras item.  If the file does not exist and
 * there isn't an Extras menu to display then gray out the Extras
 * item.  Otherwise leave it alone.
 */
Pkg_private Menu_item textsw_extras_gen_proc(Menu_item mi, Menu_generate operation)
{
	char full_file[MAXPATHLEN];
	struct stat statb;
	char *filename;
	int file_exists;
	Xv_opaque tsw;
	Xv_server srv;

	if (operation != MENU_DISPLAY)
		return mi;

	tsw = xv_get(mi, MENU_CLIENT_DATA);   /* Ref (hklbrefvklhbs) txt_menu.c */
	srv = XV_SERVER_FROM_WINDOW(tsw);

	filename = textsw_get_extras_filename(srv, mi);

	expand_path(filename, full_file);

	file_exists = (stat(full_file, &statb) >= 0);

	xv_set(mi, MENU_INACTIVE, !file_exists, NULL);

	if (file_exists && extras_menufile_changed()) {
		Menu menu_handle = xv_get(mi, MENU_PULLRIGHT);
		Textsw_view textsw_view = textsw_from_menu(menu_handle);

		textsw_remove_all_menu_items(menu_handle);
		textsw_build_extras_menu_items(textsw_view, full_file, menu_handle);
	}
	return (mi);
}


Pkg_private int textsw_build_extras_menu_items(Textsw_view textsw_view,
										char *file, Menu menu)
{
	FILE *mfd;
	int lineno = 1;	/* Needed for recursion */
	char full_file[MAXPATHLEN];
	struct stat statb;

	expand_path(file, full_file);
	if ((mfd = fopen(full_file, "r")) == NULL) {
		char *error_string;

		error_string = malloc(strlen(full_file) +
				strlen(XV_MSG("extras menu file ")) + 2);
		strcpy(error_string, XV_MSG("extras menu file "));
		strcat(error_string, full_file);
		xv_error(XV_NULL,
				ERROR_LAYER, ERROR_SYSTEM,
				ERROR_STRING, error_string,
				ERROR_PKG, TEXTSW,
				NULL);
		free(error_string);
		return (ERROR);
	}

	if (Textsw_nextfile >= MAX_FILES - 1) {
		char dummy[128];

		(void)sprintf(dummy,
				XV_MSG("textsw: max number of menu files is %d"), MAX_FILES);
		xv_error(XV_NULL, ERROR_STRING, dummy, ERROR_PKG, TEXTSW, NULL);

		(void)fclose(mfd);
		return (ERROR);
	}
	if (stat(full_file, &statb) < 0) {
		xv_error(XV_NULL,
				ERROR_LAYER, ERROR_SYSTEM,
				ERROR_STRING, full_file,
				ERROR_PKG, TEXTSW,
				NULL);
		fclose(mfd);
		return (ERROR);
	}
	Extras_stat_array[Textsw_nextfile].mftime = statb.st_mtime;
	Extras_stat_array[Textsw_nextfile].name = textsw_savestr(full_file);
	Textsw_nextfile++;

	if (walk_getmenu(textsw_view, menu, full_file, mfd, &lineno) < 0) {
		free(Extras_stat_array[--Textsw_nextfile].name);
		(void)fclose(mfd);
		return (ERROR);
	}
	else {
		(void)fclose(mfd);
		return (TRUE);
	}
}

#ifndef IL_ERRORMSG_SIZE
#define IL_ERRORMSG_SIZE	256
#endif

Pkg_private char *textsw_make_help(Textsw self, const char *str)
{
	char *hf, *myhelp, *itemhelp, helpbuf[100];
	Xv_server srv = XV_SERVER_FROM_WINDOW(self);

	myhelp = (char *)xv_get(self, XV_HELP_DATA);
	if (myhelp) {
		itemhelp = xv_malloc(strlen(myhelp) + strlen(str) + 2);
		sprintf(itemhelp, "%s_%s", myhelp, str);
		return itemhelp;
	}

	hf = (char *)xv_get(srv,XV_APP_HELP_FILE);
	if (! hf) hf = "textsw";

	sprintf(helpbuf, "%s:%s", hf, str);

	return xv_strsave(helpbuf);
}

Pkg_private void textsw_free_help(Xv_opaque obj, int key, char *data)
{	
	/* key might be XV_HELP or EXTRASMENU_FILENAME_KEY .... */
	if (data) xv_free(data);
}

static int cleanup_key = 0;

static int walk_getmenu(Textsw_view textsw_view, Menu m, char *file,
    									FILE *mfd, int *lineno)
{
	char line[256], tag[32], prog[256], args[256];
	register char *p;
	Menu nm;
	Menu_item mi = (Menu_item) 0;
	char *nqformat, *qformat, *iformat, *format;
	char err[IL_ERRORMSG_SIZE], icon_file[MAXPATHLEN];
	struct pixrect *mpr;

	if (cleanup_key == 0) cleanup_key = xv_unique_key();

	nqformat = "%[^ \t\n]%*[ \t]%[^ \t\n]%*[ \t]%[^\n]\n";
	qformat = "\"%[^\"]\"%*[ \t]%[^ \t\n]%*[ \t]%[^\n]\n";
	iformat = "<%[^>]>%*[ \t]%[^ \t\n]%*[ \t]%[^\n]\n";

	xv_set(m, MENU_CLIENT_DATA, textsw_view, NULL);

	for (; fgets(line, (int)sizeof(line), mfd); (*lineno)++) {

		if (line[0] == '#') {
			if (line[1] == '?') {
				char help[256];

				for (p = line + 2; isspace(*p); p++);

				if (*p != '\0' && sscanf(p, "%[^\n]\n", help) > 0)
					xv_set((mi != XV_NULL ? mi : m), XV_HELP_DATA, help, NULL);
			}
			continue;
		}
		for (p = line; isspace(*p); p++);

		if (*p == '\0')
			continue;

		args[0] = '\0';
		format = *p == '"' ? qformat : *p == '<' ? iformat : nqformat;

		if (sscanf(p, format, tag, prog, args) < 2) {
			char dummy[128];

			(void)sprintf(dummy,
					XV_MSG("textsw: format error in %s: line %d"),
					file, *lineno);
			xv_error(XV_NULL,
					ERROR_STRING, dummy,
					ERROR_PKG, TEXTSW,
					NULL);
			return (ERROR);
		}
		if (strcmp(prog, "END") == 0)
			return (TRUE);

		if (format == iformat) {
			expand_path(tag, icon_file);
			if ((mpr = icon_load_mpr(icon_file, err)) == NULL) {
				char *error_string;

				error_string = malloc(strlen(err) +
						strlen(XV_MSG("textsw: icon file format error: ")) + 2);
				strcpy(error_string,
						XV_MSG("textsw: icon file format error: "));
				strcat(error_string, err);
				xv_error(XV_NULL,
						ERROR_STRING, error_string, ERROR_PKG, TEXTSW, NULL);
				free(error_string);
				return (ERROR);
			}
		}
		else
			mpr = NULL;

		if (strcmp(prog, "MENU") == 0) {
			nm = xv_create(XV_NULL, MENU,
					MENU_NOTIFY_PROC, menu_return_item,
					XV_HELP_DATA, textsw_make_help(xv_get(textsw_view,XV_OWNER),
													"extrasmenu"),
					XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
					NULL);

			if (args[0] == '\0') {
				if (walk_getmenu(textsw_view, nm, file, mfd, lineno) < 0) {
					xv_destroy(nm);
					return (ERROR);
				}
			}
			else {
				if (textsw_build_extras_menu_items(textsw_view, args, nm) < 0) {
					xv_destroy(nm);
					return (ERROR);
				}
			}
			if (mpr)
				mi = xv_create(XV_NULL, MENUITEM,
						MENU_IMAGE, mpr,
						MENU_PULLRIGHT, nm,
						MENU_RELEASE,
						MENU_RELEASE_IMAGE,
						NULL);
			else
				mi = xv_create(XV_NULL, MENUITEM,
						MENU_STRING, textsw_savestr(tag),
						MENU_PULLRIGHT, nm,
						MENU_RELEASE,
						MENU_RELEASE_IMAGE,
						NULL);
		}
		else {
			if (mpr) {
				mi = xv_create(XV_NULL, MENUITEM,
						MENU_IMAGE, mpr,
						MENU_CLIENT_DATA, textsw_save2str(prog, args),
						MENU_RELEASE,
						MENU_RELEASE_IMAGE,
						MENU_NOTIFY_PROC, textsw_handle_extras_menuitem,
						NULL);
			}
			else {
				char *cldt = textsw_save2str(prog, args);

				/* originally, this was only 'attached' as MENU_CLIENT_DATA,
				 * which led to a memory leak, because the MenuItem 
				 * destruction does not do anything with MENU_CLIENT_DATA.
				 * Now, I left it as MENU_CLIENT_DATA, but **also**
				 * entered it as XV_KEY_DATA with a XV_KEY_DATA_REMOVE_PROC
				 */
				mi = xv_create(XV_NULL, MENUITEM,
						MENU_STRING, textsw_savestr(tag),
						MENU_CLIENT_DATA, cldt,
						XV_KEY_DATA, cleanup_key, cldt,
						XV_KEY_DATA_REMOVE_PROC, cleanup_key, textsw_free_help,
						MENU_RELEASE,
						MENU_RELEASE_IMAGE,
						MENU_NOTIFY_PROC, textsw_handle_extras_menuitem,
						NULL);
			}
		}
		xv_set(m, MENU_APPEND_ITEM, mi, NULL);
	}

	return (TRUE);
}

static Menu_item textsw_handle_extras_menuitem(Menu menu, Menu_item item)
{
	char *prog, *args;
	char command_line[MAXPATHLEN];
	char **filter_argv;
	Textsw_view textsw_view = textsw_from_menu(menu);
	register Textsw_view_private view;
	register Textsw_private priv;
	int again_state;

#ifdef OW_I18N
	CHAR cmd_line_wcs[MAXPATHLEN];
#endif

	if AN_ERROR
		(textsw_view == 0)
				return XV_NULL;

	view = VIEW_ABS_TO_REP(textsw_view);
	priv = TSWPRIV_FOR_VIEWPRIV(view);

	prog = (char *)xv_get(item, MENU_CLIENT_DATA);
	args = XV_INDEX(prog, '\0') + 1;

	sprintf(command_line, "%s %s", prog, args);
	filter_argv = textsw_string_to_argv(command_line);

	textsw_flush_caches(view, TFC_STD);
	priv->func_state |= TXTSW_FUNC_FILTER;
	again_state = priv->func_state & TXTSW_FUNC_AGAIN;

#ifdef OW_I18N
	(void)mbstowcs(cmd_line_wcs, command_line, MAXPATHLEN);
	textsw_record_extras(priv, cmd_line_wcs);
#else
	textsw_record_extras(priv, command_line);
#endif

	priv->func_state |= TXTSW_FUNC_AGAIN;

	textsw_checkpoint_undo(TEXTSW_PUBLIC(priv),
			(caddr_t) TEXTSW_INFINITY - 1);

	(void)textsw_call_filter(view, filter_argv);

	textsw_checkpoint_undo(TEXTSW_PUBLIC(priv),
			(caddr_t) TEXTSW_INFINITY - 1);

	priv->func_state &= ~TXTSW_FUNC_FILTER;
	if (again_state == 0)
		priv->func_state &= ~TXTSW_FUNC_AGAIN;
	free_argv(filter_argv);
	return (item);
}

/*
 * *	textsw_string_to_argv - This function takes a char * that contains * program
 * and it's arguments and returns *			a char ** argument
 * vector for use with execvp *
 * 
 * For example "fmt -65" is turned into * rgv[0] = "fmt" *
 * ] = "-65" * rgv[2] = NULL
 */
static int any_shell_meta(char *s);

Pkg_private char **textsw_string_to_argv(char *command_line)
{
    int             i, pos = 0;
    char          **new_argv;
    char           *arg_array[MAXARGS];
    char            scratch[MAXSTRLEN];
    int             use_shell = any_shell_meta(command_line);

    Nargs = 0;

    if (use_shell) {
	/* put in their favorite shell and pass cmd as single string */
	char           *shell;

	if ((shell = getenv("SHELL")) == NULL)
	    shell = "/bin/sh";
	new_argv = (char **) malloc((unsigned) 4 * sizeof(char *));
	new_argv[0] = shell;
	new_argv[1] = "-c";
	new_argv[2] = strdup(command_line);
	new_argv[3] = '\0';
    } else {
	/* Split command_line into it's individual arguments */
	while (string_get_token(command_line, &pos, scratch, xv_white_space) != NULL)
	    arg_array[Nargs++] = strdup(scratch);

	/*
	 * Allocate a new array of appropriate size (Nargs+1 for NULL string)
	 * This is so the caller will know where the array ends
	 */
	new_argv = (char **) malloc(((unsigned) Nargs + 1) *
				    (sizeof(char *)));

	/* Copy the strings from arg_array into it */
	for (i = 0; i < Nargs; i++)
	    new_argv[i] = arg_array[i];
	new_argv[Nargs] = '\0';
    }
    return (new_argv);
}

static void free_argv(char **argv)
{
    while (Nargs > 0)
	free(argv[--Nargs]);
    free(argv);
}

static char *textsw_savestr(char *s)
{
    register char  *p;

    if ((p = malloc(strlen(s) + 1)) == NULL) {
	xv_error(XV_NULL,
		 ERROR_SEVERITY, ERROR_NON_RECOVERABLE,
		 ERROR_LAYER, ERROR_SYSTEM,
		 ERROR_STRING, XV_MSG("textsw: menu strings"),
		 ERROR_PKG, TEXTSW,
		 NULL);
    }
    (void) strcpy(p, s);
    return (p);
}

static char *textsw_save2str(char *s, char *t)
{
    register char  *p;

    if ((p = malloc(strlen(s) + strlen(t) + 1 + 1)) == NULL) {
	xv_error(XV_NULL,
		 ERROR_SEVERITY, ERROR_NON_RECOVERABLE,
		 ERROR_LAYER, ERROR_SYSTEM,
		 ERROR_STRING, XV_MSG("textsw: menu strings"),
		 ERROR_PKG, TEXTSW,
		 NULL);
    }
    (void) strcpy(p, s);
    (void) strcpy(XV_INDEX(p, '\0') + 1, t);
    return (p);
}

/*
 * Are there any shell meta-characters in string s?
 */
static int any_shell_meta(char *s)
{

    while (*s) {
	if (XV_INDEX("~{[*?$`'\"\\", *s))
	    return (1);
	s++;
    }
    return (0);
}
