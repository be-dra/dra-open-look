#ifndef lint
char     tty_c_sccsid[] = "@(#)tty.c 20.64 93/06/28 DRA: $Id: tty.c,v 4.16 2025/11/01 14:56:47 dra Exp $";
#endif

/*****************************************************************/
/* tty.c                           */
/*	
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */
/*****************************************************************/

#include <xview/ttysw.h>
#include <xview_private/tty_impl.h>
#include <xview/defaults.h>
#include <xview_private/term_impl.h>
#include <xview_private/ttyansi.h>
#include <xview_private/svr_impl.h>
#include <xview_private/draw_impl.h>

#ifdef SVR4
#include <sys/suntty.h>
#include <sys/strredir.h>
#endif

#define HELP_INFO(s) XV_HELP_DATA, s,

static void tty_quit_on_death(Notify_client client, int pid, int *status, struct rusage *rusage);
static void tty_handle_death(Notify_client , int pid, int *status, struct rusage *rusage);

static Pixfont* change_font;

/* deswegen inkludiere ich doch nicht term_impl.h */
extern int tty_notice_key;


/*****************************************************************************/
/* Ttysw init routines for folio and  view	                             */
/*****************************************************************************/

static int tty_folio_init(Xv_Window parent, Tty tty_public,
								Attr_attribute avlist[], int *u)
{
	Ttysw_private ttysw;	/* Private object data */

#ifdef OW_I18N
	Xv_private void tty_text_start();
	Xv_private void tty_text_done();
	Xv_private void tty_text_draw();
#endif

	if (!tty_notice_key) {
		tty_notice_key = xv_unique_key();
	}

	ttysw = (Ttysw_private)ttysw_init_folio_internal(tty_public);
	if (!ttysw) {
		return (XV_ERROR);
	}

#ifdef OW_I18N
	if (xv_get(tty_public, WIN_USE_IM)) {
		/* Set preedit callbacks */
		xv_set(tty_public,
				WIN_IC_PREEDIT_START,
				(XIMProc) tty_text_start, (XPointer) tty_public, NULL);

		xv_set(tty_public,
				WIN_IC_PREEDIT_DRAW,
				(XIMProc) tty_text_draw, (XPointer) tty_public, NULL);

		xv_set(tty_public,
				WIN_IC_PREEDIT_DONE,
				(XIMProc) tty_text_done, (XPointer) tty_public, NULL);

		ttysw->start_pecb_struct.client_data = (XPointer) tty_public;
		ttysw->start_pecb_struct.callback = (XIMProc) tty_text_start;

		ttysw->draw_pecb_struct.client_data = (XPointer) tty_public;
		ttysw->draw_pecb_struct.callback = (XIMProc) tty_text_draw;

		ttysw->done_pecb_struct.client_data = (XPointer) tty_public;
		ttysw->done_pecb_struct.callback = (XIMProc) tty_text_done;
	}
#endif

	ttysw->pass_thru_modifiers = (int)xv_get(XV_SERVER_FROM_WINDOW(parent),
			SERVER_CLEAR_MODIFIERS);
	ttysw->eight_bit_output = (int)defaults_get_boolean("ttysw.eightBitOutput",
			"Ttysw.EightBitOutput", TRUE);

	/*
	 * jcb	-- remove continual cursor repaint in shelltool windows also
	 * known to tty_main.c
	 */
	ttysw->do_cursor_draw = TRUE;
	/* 0 -> NOCURSOR, 1 -> UNDERCURSOR, 2 -> BLOCKCURSOR */
	ttysw->cursor = BLOCKCURSOR | LIGHTCURSOR;
	ttysw->hdrstate = HS_BEGIN;
	ttysw->ttysw_stringop = ttytlsw_string;
	ttysw->ttysw_escapeop = ttytlsw_escape;
	xv_set(tty_public, WIN_MENU, ttysw_walkmenu(tty_public), NULL);
	ttysw_interpose(ttysw);
	return (XV_OK);
}

/*ARGSUSED*/
static int tty_view_init(Xv_Window parent,	Tty_view tty_view_public, Attr_attribute avlist[], int *u)
{
    Ttysw_view_handle ttysw_view;	/* Private object data */

    if (!tty_notice_key)  {
	tty_notice_key = xv_unique_key();
    }

    /*
     * BUG ALERT!  Re-arrange code to pass this pixwin into the appropriate
     * layer instead of just smashing it set from here!
     */
    csr_pixwin_set(tty_view_public);

    ttysw_view = (Ttysw_view_handle) (ttysw_init_view_internal(parent, tty_view_public));

    if (!ttysw_view)
	return (XV_ERROR);


    /* ttysw_walkmenu() can only be called after public self linked to */
    (void) xv_set(tty_view_public,
		  WIN_NOTIFY_SAFE_EVENT_PROC, ttysw_event,
		  WIN_NOTIFY_IMMEDIATE_EVENT_PROC, ttysw_event,
		  NULL);

    /* ttysw_interpose(ttysw_view); */

    /* Draw cursor on the screen and retained portion */
#ifdef OW_I18N
#ifdef FULL_R5
    if (IS_TTY_VIEW(tty_view_public))
#endif
#endif
	{
		Ttysw_private ttysw = TTY_PRIVATE_FROM_ANY_PUBLIC(parent);
        ttysw_drawCursor(ttysw, 0, 0);
	}

    return (XV_OK);
}




/***************************************************************************
ttysw_set_internal
*****************************************************************************/
#ifdef OW_I18N
static          Xv_opaque
ttysw_set_internal(tty_public, avlist, is_folio)
    Tty             tty_public;
    Attr_attribute  avlist[];
    int			is_folio;
#else
static Xv_opaque ttysw_set_internal(Tty tty_public, Attr_attribute avlist[])
#endif
{
	Ttysw_private ttysw = TTY_PRIVATE_FROM_ANY_PUBLIC(tty_public);
	register Attr_avlist attrs;
	static int quit_tool;
	int pid = -1, bold_style = -1, argv_set = 0;
	char **argv = 0;
	int do_fork = FALSE;
	char *buf;
	int *buf_used;
	int buf_len;
	int m;
	Xv_Drawable_info *info;

#ifdef OW_I18N
	Tty ttysw_pub;
#endif

	for (attrs = avlist; *attrs; attrs = attr_next(attrs)) {
		switch (attrs[0]) {

			case TTY_ARGV:
				do_fork = TRUE;
				argv_set = 1;
				argv = (char **)attrs[1];
				ATTR_CONSUME(attrs[0]);
				break;

			case TTY_CONSOLE:
				if (attrs[1]) {

#ifdef sun	/* Vaxen do not support the TIOCCONS ioctl */

#ifdef SVR4
					int consfd;

					if ((consfd = open("/dev/console", O_RDONLY)) == -1)
						xv_error(tty_public,
								ERROR_STRING, "open of /dev/console failed",
								ERROR_LAYER, ERROR_SYSTEM,
								ERROR_PKG, TTY, NULL);

					else if ((ioctl(consfd, SRIOCSREDIR,
											ttysw->ttysw_tty)) == -1)
						xv_error(tty_public, ERROR_STRING,
								"ioctl SRIOCSREDIR returned -1, attempt to make tty the console failed",
								ERROR_LAYER, ERROR_SYSTEM, ERROR_PKG, TTY,
								NULL);

#else
					if ((ioctl(ttysw->ttysw_tty, TIOCCONS, 0)) == -1)
						xv_error(tty_public,
								ERROR_STRING,
								"ioctl TIOCCONS returned -1, attempt to make tty the console failed",
								ERROR_LAYER, ERROR_SYSTEM,
								ERROR_PKG, TTY, NULL);
#endif
#endif
				};
				ATTR_CONSUME(attrs[0]);
				break;

			case TTY_INPUT:
				buf = (char *)attrs[1];
				buf_len = (int)attrs[2];
				buf_used = (int *)attrs[3];
				*buf_used = ttysw_input_it(ttysw, buf, buf_len);
				ATTR_CONSUME(attrs[0]);
				break;

			case TTY_PAGE_MODE:
				/* idiotic interface */
				ttysw_setopt((Ttysw_private)(ttysw->view),
									TTYOPT_PAGEMODE, (int)attrs[1]);
				ATTR_CONSUME(attrs[0]);
				break;

			case TTY_QUIT_ON_CHILD_DEATH:
				quit_tool = (int)attrs[1];
				ATTR_CONSUME(attrs[0]);
				break;

			case TTY_BOLDSTYLE:
				(void)ttysw_setboldstyle((int)attrs[1]);
				ATTR_CONSUME(attrs[0]);
				break;

			case TTY_BOLDSTYLE_NAME:
				bold_style = ttysw_lookup_boldstyle((char *)attrs[1]);
				if (bold_style == -1)
					(void)ttysw_print_bold_options();
				else
					(void)ttysw_setboldstyle(bold_style);
				ATTR_CONSUME(attrs[0]);
				break;

			case TTY_INVERSE_MODE:
				(void)ttysw_set_inverse_mode((int)attrs[1]);
				ATTR_CONSUME(attrs[0]);
				break;

			case TTY_PID:
				do_fork = TRUE;
				/* TEXTSW_INFINITY ==> no child process, 0 ==> we want one */
				/* BUG ALERT: need validity check on (int)attrs[1]. */
				ttysw->ttysw_pidchild = (int)attrs[1];
				ATTR_CONSUME(attrs[0]);
				break;

			case TTY_UNDERLINE_MODE:
				(void)ttysw_set_underline_mode((int)attrs[1]);
				ATTR_CONSUME(attrs[0]);
				break;

			case XV_FONT:
				if (attrs[1] && csr_pixwin_get()) {
					/*
					 * Cursor for the original font has been drawn, so take
					 * down
					 */
					ttysw_removeCursor(ttysw);
					xv_new_tty_chr_font(ttysw, (Pixfont *) attrs[1]);
					/* after changing font size, cursor needs to be re-drawn */
					(void)ttysw_drawCursor(ttysw, 0, 0);
				}
				else if (attrs[1])
					change_font = (Pixfont *) attrs[1];
				break;

			case TTY_LEFT_MARGIN:
				m = (int)attrs[1];
    			ttysw->chrleftmargin = m > 0 ? m : 0;
				ATTR_CONSUME(attrs[0]);
				break;

			case WIN_SET_FOCUS:{
					Tty_view win;

					ATTR_CONSUME(avlist[0]);

					win = TTY_VIEW_PUBLIC(ttysw->view);
					DRAWABLE_INFO_MACRO(win, info);
					if (win_getinputcodebit(
									(Inputmask *) xv_get(win, WIN_INPUT_MASK),
									KBD_USE)) {
						win_set_kbd_focus(win, xv_xid(info));
						return (XV_OK);
					}
					return (XV_ERROR);
				}

#ifdef OW_I18N
			case WIN_IC_ACTIVE:
				ttysw_pub = TTY_PUBLIC(ttysw);
				if (is_folio && (int)xv_get(ttysw_pub, WIN_USE_IM)) {
					Ttysw_view_handle view;
					Tty_view view_public;

					view = TTY_VIEW_HANDLE_FROM_TTY_FOLIO(ttysw);
					view_public = TTY_VIEW_PUBLIC(view);
					xv_set(view_public, WIN_IC_ACTIVE, attrs[1], 0);
				}
				break;
#endif

			case XV_END_CREATE:
				/*
				 * xv_create(0, TTY, 0) should fork a default shell, but
				 * xv_create(0, TTY, TTY_ARGV, TTY_ARGV_DO_NOT_FORK, 0) should
				 * not fork anything (ttysw_pidchild will == TEXTSW_INFINITY >
				 * 0).
				 */
				if (!do_fork && ttysw->ttysw_pidchild <= 0)
					do_fork = TRUE;
				if (ttysw->view)
					ttysw_resize(ttysw->view);

				if (change_font) {
					ttysw_removeCursor(ttysw);
					xv_new_tty_chr_font(ttysw, (Pixfont *) change_font);
					/* after changing font size, cursor needs to be re-drawn */
					ttysw_drawCursor(ttysw, 0, 0);
					change_font = NULL;
				}

#ifdef OW_I18N
				ttysw->ic = NULL;
				ttysw_pub = TTY_PUBLIC(ttysw);

				if (xv_get(ttysw_pub, WIN_USE_IM)) {
					ttysw->ic = (XIC) xv_get(ttysw_pub, WIN_IC);

#ifdef FULL_R5
					if (ttysw->ic)
						XGetICValues(ttysw->ic, XNInputStyle, &ttysw->xim_style,
								NULL);
#endif /* FULL_R5 */

				}

				if (TTY_IS_TERMSW(ttysw))
					break;

				if (ttysw->ic) {
					Ttysw_view_handle view;
					Tty_view view_public;

					view = TTY_VIEW_HANDLE_FROM_TTY_FOLIO(ttysw);
					view_public = TTY_VIEW_PUBLIC(view);

					xv_set(view_public, WIN_IC, ttysw->ic, 0);

					if (xv_get(ttysw_pub, WIN_IC_ACTIVE) == FALSE)
						xv_set(view_public, WIN_IC_ACTIVE, FALSE, 0);
				}
#endif

				break;

			default:
				(void)xv_check_bad_attr(TTY, attrs[0]);
				break;
		}
	}

	/*
	 * WARNING. For certain sequences of calls, the following code loses
	 * track of the process id of the current child, and could be tricked
	 * into having multiple children executing at once.
	 */
	if (argv == (char **)TTY_ARGV_DO_NOT_FORK) {
		ttysw->ttysw_pidchild = TEXTSW_INFINITY;
	}
	else {
		if (argv_set && ttysw->ttysw_pidchild == TEXTSW_INFINITY) {
			ttysw->ttysw_pidchild = 0;
		}
		if (ttysw->ttysw_pidchild <= 0 && do_fork) {
			SERVERTRACE((333, "before ttysw_fork_it, argv=%p, %lx\n", argv,
									TTY_ARGV_DO_NOT_FORK));
			pid = ttysw_fork_it(ttysw, argv ? argv : (char **)&argv, 0);
			if (pid > 0) {
				SERVERTRACE((333, "after ttysw_fork_it, pid=%d\n", pid));
				notify_set_wait3_func((Notify_client) ttysw,
						(Notify_func) (quit_tool ? tty_quit_on_death :
								tty_handle_death), pid);
			}
		}
	}

	return (XV_OK);
}

static Xv_opaque ttysw_folio_set(Tty ttysw_folio_public, Attr_attribute avlist[])
{
#ifdef OW_I18N
    return (ttysw_set_internal(ttysw_folio_public, avlist, 1));
#else
    return (ttysw_set_internal(ttysw_folio_public, avlist));
#endif

}

static Xv_opaque ttysw_view_set(Tty_view ttysw_view_public, Attr_attribute avlist[])
{
#ifdef OW_I18N
    return (ttysw_set_internal(ttysw_view_public, avlist, 0));
#else
    return (ttysw_set_internal(ttysw_view_public, avlist));
#endif

}




/*****************************************************************************/
/* ttysw_get_internal        				                     */
/*****************************************************************************/
static Xv_opaque ttysw_get_internal(Ttysw_private ttysw, int *status,
					Attr_attribute attr, va_list args)
{
	switch (attr) {
		case OPENWIN_VIEW_CLASS:
			return ((Xv_opaque) TTY_VIEW);

		case TTY_PAGE_MODE:
			return (Xv_opaque) ttysw_getopt(ttysw, TTYOPT_PAGEMODE);

		case TTY_QUIT_ON_CHILD_DEATH:
			return (Xv_opaque) 0;

		case TTY_PID:
			return (Xv_opaque) ttysw->ttysw_pidchild;

		case TTY_PTY_FD:
			return (Xv_opaque) ttysw->ttysw_pty;

		case TTY_TTY_FD:
			return (Xv_opaque) ttysw->ttysw_tty;

		case WIN_TYPE:	/* SunView1.X compatibility */
			return (Xv_opaque) TTY_TYPE;

		default:
			if (xv_check_bad_attr(TTY, attr) == XV_ERROR) {
				*status = XV_ERROR;
			}
			return ((Xv_opaque) 0);
	}
}


static Xv_opaque ttysw_folio_get(Tty ttysw_folio_public, int *status,
							Attr_attribute attr, va_list args)
{
    Ttysw_private ttysw = TTY_PRIVATE_FROM_ANY_FOLIO(ttysw_folio_public);

	return ttysw_get_internal(ttysw, status, attr, args);
}

static Xv_opaque ttysw_view_get(Tty_view ttysw_view_public, int *status,
									Attr_attribute attr, va_list args)
{
	/* this is again an example of that funny mixture of Tty and Tty_view:
	 * I'm asking for view attributesa (if any), but get the answer from 
	 * the tty....
	 */
    Ttysw_private ttysw = TTY_PRIVATE_FROM_ANY_VIEW(ttysw_view_public);

    return ttysw_get_internal(ttysw, status, attr, args);
}

static void tty_quit_on_death(Notify_client client, int pid, int *status,
									struct rusage *rusage)
{
	Ttysw_private ttysw = (Ttysw_private) client;
	Tty tty_public = TTY_PUBLIC(ttysw);
	Xv_object frame;

	if (!(WIFSTOPPED(*status))) {
		if (WTERMSIG(*status) || WEXITSTATUS(*status) || WCOREDUMP(*status)) {
			if (TTY_IS_TERMSW(ttysw)) {
				fprintf(stderr, XV_MSG(
					"A command window has exited because its child exited.\n"));
			}
			else {
				fprintf(stderr, XV_MSG(
						"A tty window has exited because its child exited.\n"));
			}

			fprintf(stderr, XV_MSG("Its child's process id was %d and it"),
									pid);
			if (WTERMSIG(*status)) {
				fprintf(stderr, XV_MSG(" died due to signal %d"),
											WTERMSIG(*status));
			}
			else if (WEXITSTATUS(*status)) {
				fprintf(stderr, XV_MSG(" exited with return code %d"),
											WEXITSTATUS(*status));
			}
			if (WCOREDUMP(*status)) {
				fprintf(stderr, XV_MSG(" and left a core dump.\n"));
			}
			else {
				fprintf(stderr, ".\n");
			}
		}
		frame = xv_get(tty_public, WIN_FRAME);
		xv_set(frame, FRAME_NO_CONFIRM, TRUE, NULL);
		xv_destroy(frame);
	}
}

static void tty_handle_death(Notify_client client, int pid,
							int *status, struct rusage *rusage)
{
	if (!(WIFSTOPPED(*status))) {
		Ttysw_private tty_folio_private = (Ttysw_private)client;
		tty_folio_private->ttysw_pidchild = 0;
	}
}


static int ttysw_view_destroy(Tty_view ttysw_view_public, Destroy_status status)
{
    Ttysw_view_handle ttysw_view_private =
    						TTY_VIEW_PRIVATE_FROM_ANY_VIEW(ttysw_view_public);
	Xv_window v;


    if ((status != DESTROY_CHECKING) && (status != DESTROY_SAVE_YOURSELF)) {
		Openwin ow = xv_get(ttysw_view_public, XV_OWNER);

		csr_pixwin_set(XV_NULL);
		OPENWIN_EACH_VIEW(ow, v)
			if (v != ttysw_view_public) {
				csr_pixwin_set(v);
				break;
			}
		OPENWIN_END_EACH
		free((char *) ttysw_view_private);
    }
    return (XV_OK);
}

static int ttysw_folio_destroy(Tty ttysw_folio_public, Destroy_status status)
{
    return (ttysw_destroy(ttysw_folio_public, status));
}

const Xv_pkg          xv_tty_pkg = {
    "Tty",
    (Attr_pkg) ATTR_PKG_TTY,
    sizeof(Xv_tty),
    OPENWIN,
    tty_folio_init,
    ttysw_folio_set,
    ttysw_folio_get,
    ttysw_folio_destroy,
    NULL			/* no find proc */
};

const Xv_pkg          xv_tty_view_pkg = {
    "Tty_view",
    (Attr_pkg) ATTR_PKG_TTY_VIEW,
    sizeof(Xv_tty_view),
    OPENWIN_VIEW,
    tty_view_init,
    ttysw_view_set,
    ttysw_view_get,
    ttysw_view_destroy,
    NULL			/* no find proc */
};
