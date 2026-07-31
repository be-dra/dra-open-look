#ifndef lint
char tty_mapkey_c_sccsid[] = "@(#)tty_mapkey.c 20.41 93/06/28 DRA: $Id: tty_mapkey.c,v 4.11 2026/07/30 12:06:21 dra Exp $";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */

#include <stdio.h>
#include <ctype.h>
#ifdef SVR4
#include <string.h>
#endif
#include <sys/types.h>
#include <sys/file.h>
#include <sys/time.h>
#include <xview_private/i18n_impl.h>
#include <xview_private/portable.h>
#include <xview/defaults.h>
#include <xview/win_input.h>
#include <xview/ttysw.h>
#include <xview/termsw.h>
#include <xview/frame.h>
#include <xview_private/tty_impl.h>
#include <xview/win_notify.h>
#include <xview/help.h>
#define _NOTIFY_MIN_SYMBOLS
#include <xview/notify.h>
#undef _NOTIFY_MIN_SYMBOLS

/* static routines	 */

static char *savestr(char *s);
static char *tdecode(register char *src, char *dst);

static void ttysw_arrow_keys_to_string(unsigned xv_id, char *str);
static void ttysw_doset(struct ttysubwindow *ttysw, char *var);
static int ttysw_mapkey(Ttysw *ttysw, char *key, char *to, int output);
static int ttysw_strtokey(char *s);


extern char    *xv_getlogindir(void);

/*
 * Read rc file.
 */
Pkg_private void ttysw_readrc(struct ttysubwindow *ttysw)
{
	char *p;
	char rc[1025];
	FILE *fp;
	char line[1025], *av[4];
	int i, lineno = 0;

	if ((p = xv_getlogindir()) == (char *)NULL)
		return;
	(void)strcpy(rc, p);
	(void)strcat(rc, "/.ttyswrc");
	if ((fp = fopen(rc, "r")) == (FILE *) NULL) {
		if ((int)defaults_get_boolean("term.useAlternateTtyswrc",
						"Term.UseAlternateTtyswrc", TRUE)) {
			char *altrc;

			XV_BZERO(rc, sizeof(rc) - 1);
			if ((p = (char *)getenv("OPENWINHOME")) != (char *)NULL) {
				(void)strcpy(rc, p);
				(void)strcat(rc, "/lib/.ttyswrc");
			}
			else
				(void)strcpy(rc, "/lib/.ttyswrc");
			altrc = (char *)defaults_get_string("term.alternateTtyswrc",
					"Term.AlternateTtyswrc", rc);
			if ((fp = fopen(altrc, "r")) == (FILE *) NULL) {
				return;
			}
		}
		else {
			return;
		}
	}

	while (fgets(line, (unsigned)sizeof(line), fp)) {
		register char *t;

		lineno++;
		if (line[strlen(line) - 1] != '\n') {
			register int c;

			(void)printf(XV_MSG("%s: line %d longer than 1024 characters\n"),
					rc, lineno);
			while ((c = fgetc(fp)) != '\n' && c != EOF);
			continue;
		}
		for (t = line; isspace(*t); t++);
		if (*t == '#' || *t == '\0')
			continue;

		for (i = 0; i < 2; i++) {
			av[i] = t;
			while (!isspace(*t) && *t)
				t++;
			if (!*t)
				break;
			else
				*t++ = '\0';
			while (isspace(*t) && *t)
				t++;
			if (!*t)
				break;
		}
		if (*t) {
			i = 2;
			av[2] = t;
			av[2][strlen(av[2]) - 1] = '\0';
		}
		if (i == 2 && strcmp(av[0], "mapi") == 0)
			(void)ttysw_mapkey(ttysw, av[1], av[2], 0);
		else if (i == 2 && strcmp(av[0], "mapo") == 0)
			(void)ttysw_mapkey(ttysw, av[1], av[2], 1);
		else if (i == 1 && strcmp(av[0], "set") == 0)
			(void)ttysw_doset(ttysw, av[1]);
		else
			(void)printf(XV_MSG("%s: unknown command on line %d\n"), rc,
					lineno);
	}
	(void)fclose(fp);
}

static void ttysw_doset(struct ttysubwindow *ttysw, char *var)
{

    /* XXX - for now */
    if (strcmp(var, "pagemode") == 0)
		ttysw_setopt(ttysw, TTYOPT_PAGEMODE, 1);
}

static int ttysw_mapkey(Ttysw *ttysw, char *key, char *to, int output)
{
    int             k;

    if ((k = ttysw_strtokey(key)) == -1)
	return (-1);
    ttysw->ttysw_kmtp->kmt_key = k;
    ttysw->ttysw_kmtp->kmt_output = output;
    ttysw->ttysw_kmtp->kmt_to = savestr(tdecode(to, to));
    ttysw->ttysw_kmtp++;
    return (k);
}

Pkg_private void ttysw_mapsetim(Ttysw_private ttysw)
{
    struct keymaptab *kmt;
    Inputmask       imask;
    Xv_object       window = TTY_PUBLIC(ttysw);

    /* Update input mask */
    (void) win_getinputmask(window, &imask, 0);
    /*
     * Added user defined mappings to kbd mask.  Note: this wouldn't always
     * be the right thing for escape sequence generated events like TOP.
     */
    for (kmt = ttysw->ttysw_kmt; kmt < ttysw->ttysw_kmtp; kmt++)
	win_setinputcodebit(&imask, kmt->kmt_key);
    imask.im_flags |= IM_NEGEVENT;
    win_setinputcodebit(&imask, WIN_KEY_EXPOSE);
    win_setinputcodebit(&imask, WIN_KEY_OPEN);
    win_setinputmask(window, &imask, 0, 0L);
}

Pkg_private int ttysw_domap(Ttysw_private ttysw, Event *ie)
{
	unsigned short key = event_action(ie);
	unsigned short unmapped_key = event_id(ie);
	struct keymaptab *kmt;
	int len;

	/*
	 * The following switch is required to enforce the portion of the default
	 * user interface that MUST coordinate with Selection Service.
	 */
	/*
	 * Note: The switch below gives precedence to keymapped events in the
	 * switch before .ttyswrc mappings.  After the switch .ttyswrc mappings
	 * are checked with the .ttyswrc T1 mapping having precedence over the
	 * CAPS_LOCK keymapped event.  This fixes an alpha4 bug.
	 */
	switch (key) {
		case ACTION_PASTE:
			if (event_is_up(ie)) ttysw_event_paste_up(ttysw, &event_time(ie));
			return TTY_DONE;

		case ACTION_CUT:
			if (event_is_up(ie)) ttysw_event_cut_up(ttysw, ie);
			return TTY_DONE;

		case ACTION_COPY:
			if (event_is_down(ie))
				ttysw_event_copy_down(ttysw, &event_time(ie));
			return TTY_DONE;

		case ACTION_FIND_FORWARD:
		case ACTION_FIND_BACKWARD:
			return TTY_DONE;

		case ACTION_OPEN:
		case ACTION_CLOSE:
		case ACTION_FRONT:
		case ACTION_BACK:
			/* Pass event to container as it is a "container affecting" event. */
			win_post_event(xv_get(TTY_PUBLIC(ttysw), WIN_OWNER),
					ie, NOTIFY_IMMEDIATE);
			return TTY_DONE;

		case ACTION_HELP:
		case ACTION_MORE_HELP:
		case ACTION_TEXT_HELP:
		case ACTION_MORE_TEXT_HELP:
		case ACTION_INPUT_FOCUS_HELP:
			if (event_is_down(ie))
				xv_help_show(TTY_PUBLIC(ttysw),(char *)xv_get(TTY_PUBLIC(ttysw),
								XV_HELP_DATA), ie);
			return TTY_DONE;

		default:
			break;
	}
	if (event_is_down(ie)) {
		for (kmt = ttysw->ttysw_kmt; kmt < ttysw->ttysw_kmtp; kmt++) {
			if (kmt->kmt_key == unmapped_key) {
				len = strlen(kmt->kmt_to);

				if (kmt->kmt_output)
					ttysw_output_it(ttysw->view, kmt->kmt_to, len);
				else
					ttysw_input_it(ttysw, kmt->kmt_to, len);

				return (TTY_DONE);
			}
		}
		/*
		 * BUG:  These 4 keys should be initialized in ttysw->ttysw_kmt as
		 * .ttyswrc
		 */
		if ((unmapped_key == KEY_RIGHT(8)) || (unmapped_key == KEY_RIGHT(14)) ||
				(unmapped_key == KEY_RIGHT(10))
				|| (unmapped_key == KEY_RIGHT(12))) {
			char str[5];

			(void)ttysw_arrow_keys_to_string(unmapped_key, str);
			if ((int)strlen(str) > 0) {
				ttysw_input_it(ttysw, str, (int)strlen(str));
				return (TTY_DONE);
			}
		}
	}
	return TTY_OK;
}

static int ttysw_strtokey(char *s)
{
    int             i;

    if (strcmp(s, "LEFT") == 0)
	return (KEY_BOTTOMLEFT);
    else if (strcmp(s, "RIGHT") == 0)
	return (KEY_BOTTOMRIGHT);
    else if (isdigit(s[1])) {
	i = atoi(&s[1]);
	if (i < 1 || i > 16)
	    return (-1);
	switch (s[0]) {
	  case 'L':
	    if (i == 1 || (i > 4 && i < 11)) {
		char            dummy[128];

		(void) sprintf(dummy,
			       XV_MSG(".ttyswrc error: %s cannot be mapped"),
			       s);
		xv_error(XV_NULL,
			 ERROR_STRING, dummy,
			 ERROR_PKG, TTY,
			 NULL);
		return (-1);
	    } else
		return (KEY_LEFT(i));
	  case 'R':
	    return (KEY_RIGHT(i));
	  case 'T':
	  case 'F':
	    return (KEY_TOP(i));
	}
    }
    return (-1);
}

static char *savestr(char *s)
{
	char *p;

	p = malloc(strlen(s) + 1);
	if (p == (char *)NULL) {
		xv_error(XV_NULL,
				ERROR_LAYER, ERROR_SYSTEM,
				ERROR_STRING, XV_MSG("while saving key strings"),
				ERROR_PKG, TTY,
				NULL);
		return ((char *)NULL);
	}
	(void)strcpy(p, s);
	return (p);
}

/*
 * Interpret escape sequences in src, while copying to dst.  Stolen from
 * termcap.
 */
static char *tdecode(register char *src, char *dst)
{
	register char *cp;
	register int c;
	register char *dp;
	int i;

	cp = dst;
	while ((c = *src++)) {
		switch (c) {

			case '^':
				c = *src++ & 037;
				break;

			case '\\':
				dp = "E\033^^\\\\::n\nr\rt\tb\bf\f";
				c = *src++;
			  nextc:
				if (*dp++ == c) {
					c = *dp++;
					break;
				}
				dp++;
				if (*dp)
					goto nextc;
				if (isdigit(c)) {
					c -= '0', i = 2;
					do
						c <<= 3, c |= *src++ - '0';
					while (--i && isdigit(*src));
				}
				break;
		}
		*cp++ = c;
	}
	*cp++ = 0;
	return (dst);
}

/*
 * BUG:  This is a quick workaround,  we might want to improve this when we
 * have more time.
 */

#if defined(i386) && !defined(__linux)
static void ttysw_arrow_keys_to_string(unsigned xv_id, char *str)
{
    int             i = 0;

    str[i++] = '\033';		/* Escape char */

    switch (xv_id) {
      case KEY_RIGHT(8):
	strcpy(str + i, "[215z");
	i = 6;
	break;
      case KEY_RIGHT(14):
	strcpy(str + i, "[221z");
	i = 6;
	break;
      case KEY_RIGHT(10):
	strcpy(str + i, "[217z");
	i = 6;
	break;
      case KEY_RIGHT(12):
	strcpy(str + i, "[219z");
	i = 6;
	break;
      default:
	i = 0;
	break;
    }
    str[i++] = '\0';
}

#else
static void ttysw_arrow_keys_to_string(unsigned xv_id, char *str)
{
    int             i = 0;

    str[i++] = '\033';		/* Escape char */
    str[i++] = '[';

    switch (xv_id) {
      case KEY_RIGHT(8):
	str[i++] = 'A';
	break;
      case KEY_RIGHT(14):
	str[i++] = 'B';
	break;
      case KEY_RIGHT(10):
	str[i++] = 'D';
	break;
      case KEY_RIGHT(12):
	str[i++] = 'C';
	break;
      default:
	i = 0;
	break;
    }
    str[i++] = '\0';
}

#endif				/* ~i386 */
