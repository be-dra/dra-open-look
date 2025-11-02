#ifndef lint
char     termsw_c_sccsid[] = "@(#)termsw.c 1.59 93/06/28 DRA: $Id: termsw.c,v 4.21 2025/11/01 14:56:47 dra Exp $";
#endif

/*****************************************************************/
/* termsw.c                        */
/*	
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */
/*****************************************************************/

#include <xview_private/i18n_impl.h>
#include <xview/ttysw.h>
#include <xview/termsw.h>
#include <xview/defaults.h>
#include <xview_private/term_impl.h>
#include <xview_private/txt_impl.h>
#include <xview_private/font_impl.h>

#define HELP_INFO(s) XV_HELP_DATA, s,

/*
 * Key data for notice hung off frame AND OTHER PURPOSES
 */
int	tty_notice_key;

/*
 * Warning: a termsw is a specialization of window, not ttysw or textsw, so
 * that it has a chance to "fixup" the public object before it gets into
 * ttysw/textsw set/get routines.
 */


typedef enum {
    IF_AUTO_SCROLL = 0,
    ALWAYS = 1,
    INSERT_SAME_AS_TEXT = 2
} insert_makes_visible_flags;

static Defaults_pairs insert_makes_visible_pairs[] = {
    { "If_auto_scroll", (int) IF_AUTO_SCROLL },
    { "Always", (int) ALWAYS },
    { "Same_as_for_text", (int) INSERT_SAME_AS_TEXT },
    { NULL, (int) INSERT_SAME_AS_TEXT }
};

typedef enum {
    DO_NOT_USE_FONT = 0,
    DO_USE_FONT = 1,
    USE_FONT_SAME_AS_TEXT = 2
} control_chars_use_font_flags;

static Defaults_pairs control_chars_use_font_pairs[] = {
    { "False", (int) DO_NOT_USE_FONT },
    { "True", (int) DO_USE_FONT },
    { "Same_as_for_text", (int) USE_FONT_SAME_AS_TEXT },
    { NULL, (int) USE_FONT_SAME_AS_TEXT }
};

typedef enum {
    DO_NOT_AUTO_INDENT = 0,
    DO_AUTO_INDENT = 1,
    AUTO_INDENT_SAME_AS_TEXT = 2

} auto_indent_flags;

static Defaults_pairs auto_indent_pairs[] = {
    { "False", (int) DO_NOT_AUTO_INDENT },
    { "True", (int) DO_AUTO_INDENT },
    { "Same_as_for_text", (int) AUTO_INDENT_SAME_AS_TEXT },
    { NULL, (int) AUTO_INDENT_SAME_AS_TEXT }
};

static void termsw_register_view(Termsw termsw_public, Xv_Window termsw_view_public)
{
	Termsw_folio termsw_folio = TERMSW_PRIVATE(termsw_public);
	Termsw_view vp;

	OPENWIN_EACH_VIEW(termsw_public, vp)
		if (vp == termsw_view_public) return;/* already registered */
	OPENWIN_END_EACH

	if (0 == (int)xv_get(termsw_public, OPENWIN_NVIEWS)) {
		int length;
		int ttymargin = 0;

		ttymargin += (int)xv_get(termsw_public, TEXTSW_LEFT_MARGIN);
		ttymargin += (int)xv_get(termsw_public, TEXTSW_RIGHT_MARGIN);

		/* Misc other setup */
		xv_set(termsw_public, TTY_LEFT_MARGIN, ttymargin, NULL);

		termsw_folio->next_undo_point = textsw_checkpoint_undo(termsw_public,
				(caddr_t) TEXTSW_INFINITY);



		/*
		 * Finish set up of fields that track state of textsw i/o for ttysw.
		 * Only AFTER they are correct, interpose on textsw i/o.
		 */
		length = (int)xv_get(termsw_view_public, TEXTSW_LENGTH_I18N);
		termsw_folio->user_mark =
				textsw_add_mark_i18n(termsw_public,
				length, TEXTSW_MARK_DEFAULTS);
		termsw_folio->pty_mark =
				textsw_add_mark_i18n(termsw_public,
				length, TEXTSW_MARK_DEFAULTS);
		if (termsw_folio->append_only_log) {
			/*
			 * Note that read_only_mark is not TEXTSW_MOVE_AT_INSERT. Thus,
			 * as soon as it quits being moved by pty inserts, it will equal
			 * the user_mark.
			 */
			termsw_folio->read_only_mark =
					textsw_add_mark_i18n(termsw_public,
					termsw_folio->cooked_echo ? length : TEXTSW_INFINITY - 1,
					TEXTSW_MARK_READ_ONLY);
		}
	}

	termsw_folio->view_count++;
}



static int termsw_layout(Termsw termsw_public, Xv_Window termsw_view_public, Window_layout_op op, Xv_opaque d1, Xv_opaque d2, Xv_opaque d3, Xv_opaque d4, Xv_opaque d5)
{
	Termsw_folio termsw_folio = TERMSW_PRIVATE(termsw_public);

	switch (op) {
		case WIN_CREATE:
			if (xv_get(termsw_view_public, XV_IS_SUBTYPE_OF, TERMSW_VIEW)) {
				/*
				 * termsw_folio->layout_proc(termsw_public, termsw_view_public,
				 * op, d1, d2, d3, d4, d5);
				 */
				termsw_register_view(termsw_public, termsw_view_public);
			}
		default:
			break;
	}

	if (termsw_folio->layout_proc != NULL)
		return termsw_folio->layout_proc(termsw_public, termsw_view_public, op,
						d1, d2, d3, d4, d5);
	else
		return TRUE;
}

static void ttysw_interpose_on_textsw(Termsw_view t)
{
    notify_interpose_event_func(t, termsw_text_event, NOTIFY_SAFE);
    notify_interpose_event_func(t, termsw_text_event, NOTIFY_IMMEDIATE);
}

/*
 * Termsw: view related procedures
 */

static int termsw_view_init_internal(Xv_Window parent,
					Termsw_view termsw_view_public, Attr_attribute avlist[])
{
	Xv_termsw_view *termsw_view_object = (Xv_termsw_view *) termsw_view_public;
	Termsw termsw_public = TERMSW_FROM_TERMSW_VIEW(termsw_view_public);
	Xv_termsw *termsw_folio_object = (Xv_termsw *) termsw_public;

	/* Make the folio of termsw into a ttysw folio */
	termsw_folio_object->parent_data.private_data =
			termsw_folio_object->private_tty;

	/* Initialized to ttysw */
	if ((*(xv_tty_view_pkg.init))(parent, termsw_view_public, avlist, NULL) == XV_ERROR) {
		goto Error_Return;
	}
	/*
	 * Turning this off to prevent the application from waking each time the
	 * cursor passes over the window. Might have to be turned on when
	 * follow_cursor is implemented.
	 * (void)win_getinputmask(termsw_view_public, &im, 0);
	 * win_setinputcodebit(&im, LOC_WINENTER); win_setinputcodebit(&im,
	 * LOC_WINEXIT); (void)win_setinputmask(termsw_view_public, &im, 0, 0);
	 */

	termsw_view_object->private_tty =
			(Xv_opaque) TTY_VIEW_PRIVATE(termsw_view_public);


	/* BUG:  This is a work around until WIN_REMOVE_EVENT_PROC is ready */
	notify_remove_event_func(termsw_view_public,
			(Notify_event_interposer_func) ttysw_event, NOTIFY_SAFE);
	notify_remove_event_func(termsw_view_public,
			(Notify_event_interposer_func) ttysw_event, NOTIFY_IMMEDIATE);
	/*
	 * Restore the object as a textsw view
	 */
	termsw_folio_object->parent_data.private_data =
											termsw_folio_object->private_text;
	termsw_view_object->parent_data.private_data =
											termsw_view_object->private_text;

	ttysw_interpose_on_textsw(termsw_view_public);

	return XV_OK;

Error_Return:
	return XV_ERROR;
}

static void cleanup_mlk(Xv_object obj, int key, char *data)
{
	xv_free(data);
}

static int termsw_init_internal(Xv_Window parent, Termsw_folio termsw_folio,
									Attr_attribute avlist[])
{
	Termsw termsw_public = TERMSW_PUBLIC(termsw_folio);
	Xv_termsw *termsw_object = (Xv_termsw *) termsw_public;
	int fd;
	char *tmpfile_name = (char *)malloc(30L);
	Textsw_status status;
	int is_client_pane;

#ifdef OW_I18N
	Xv_opaque font = NULL;
#else
	Xv_opaque font;
#endif

	char *def_str;

#ifndef __linux
	char *termcap;
	static char *cmd_termcap = "TERMCAP=sun-cmd:te=\\E[>4h:ti=\\E[>4l:tc=sun:";
#endif /* __linux */

	static char *cmd_term = "TERM=sun-cmd";
	int on = 1;
	Xv_opaque defaults_array[10];
	Attr_avlist defaults;
	int temp;
	Ttysw_private ttysw_folio;
	char *font_name = NULL;

	Xv_opaque parent_font;
	int scale, size;

#ifdef OW_I18N
	Textsw_folio txt_folio;
#endif

	Xv_opaque srv = XV_SERVER_FROM_WINDOW(termsw_public);

	/* Generate a new temporary file name and open the file up. */
	(void)strcpy(tmpfile_name, "/tmp/tty.txt.XXXXXX");

	if ((fd = mkstemp(tmpfile_name)) < 0)
		return XV_ERROR;

	close(fd);

	is_client_pane = (int)xv_get(termsw_public, WIN_IS_CLIENT_PANE);

#ifdef OW_I18N
	defaults_set_locale(NULL, XV_LC_BASIC_LOCALE);
	font_name = xv_font_monospace();
	defaults_set_locale(NULL, NULL);

	if (font_name)
		font = xv_pf_open(font_name, srv);
	else
		font = (Xv_opaque) 0;

	/*
	 * if name is present, it has already been handled during the
	 * creation of the "Window" superclass in window_init.
	 */
	if (font == NULL) {
		parent_font = (Xv_opaque) xv_get(termsw_public, XV_FONT);
		scale = (int)xv_get(parent_font, FONT_SCALE);
		if (scale > 0)
			font = (Xv_opaque) xv_find(termsw_public, FONT,
					FONT_FAMILY, FONT_FAMILY_DEFAULT_FIXEDWIDTH,
					FONT_SCALE, scale, NULL);
		else {
			size = (int)xv_get(parent_font, FONT_SIZE);
			font = (Xv_opaque) xv_find(termsw_public, FONT,
					FONT_FAMILY, FONT_FAMILY_DEFAULT_FIXEDWIDTH,
					FONT_SIZE, size, NULL);
		}
	}

	if (font == NULL)
		font = (Xv_opaque) xv_get(termsw_public, XV_FONT);

#else
	font_name = xv_font_monospace();

	if (font_name && (strlen(font_name) != 0)) {
		font = (Xv_font) xv_pf_open(font_name, srv);
	}
	else
		font = (Xv_opaque) 0;

	if (is_client_pane) {
		if (!font) {
			parent_font = (Xv_opaque) xv_get(termsw_public, XV_FONT);
			scale = (int)xv_get(parent_font, FONT_SCALE);
			if (scale > 0) {
				font = (Xv_opaque) xv_find(termsw_public, FONT,
						FONT_FAMILY, FONT_FAMILY_DEFAULT_FIXEDWIDTH,
						/* FONT_FAMILY,        FONT_FAMILY_SCREEN, */
						FONT_SCALE, (scale > 0) ? scale : FONT_SCALE_DEFAULT,
						NULL);
			}
			else {
				size = (int)xv_get(parent_font, FONT_SIZE);
				font = (Xv_opaque) xv_find(termsw_public, FONT,
						FONT_FAMILY, FONT_FAMILY_DEFAULT_FIXEDWIDTH,
						/* FONT_FAMILY,        FONT_FAMILY_SCREEN,  */
						FONT_SIZE, (size > 0) ? size : FONT_SIZE_DEFAULT, NULL);
			}
		}
	}
	else {
		if (!font) {
			parent_font = (Xv_opaque) xv_get(termsw_public, XV_FONT);
			scale = (int)xv_get(parent_font, FONT_SCALE);

			if (scale > 0) {
				font = (Xv_opaque) xv_find(termsw_public, FONT,
						FONT_FAMILY, FONT_FAMILY_DEFAULT_FIXEDWIDTH,
						/* FONT_FAMILY,        FONT_FAMILY_SCREEN, */
						FONT_SCALE, (scale > 0) ? scale : FONT_SCALE_DEFAULT,
						NULL);
			}
			else {
				size = (int)xv_get(parent_font, FONT_SIZE);

				font = (Xv_opaque) xv_find(termsw_public, FONT,
						FONT_FAMILY, FONT_FAMILY_DEFAULT_FIXEDWIDTH,
						/* FONT_FAMILY,        FONT_FAMILY_SCREEN, */
						FONT_SIZE, (size > 0) ? size : FONT_SIZE_DEFAULT, NULL);
			}
		}
	}
	if (!font)
		font = (Xv_opaque) xv_get(termsw_public, XV_FONT);
#endif

	xv_set(termsw_public,
			XV_FONT, font,
			TEXTSW_STATUS, &status,
			TEXTSW_DISABLE_LOAD, TRUE,
			TEXTSW_DISABLE_CD, TRUE,
			TEXTSW_ES_CREATE_PROC, ts_create,
			TEXTSW_NO_RESET_TO_SCRATCH, TRUE,
			TEXTSW_IGNORE_LIMIT, TEXTSW_INFINITY,
			TEXTSW_NOTIFY_LEVEL,
			TEXTSW_NOTIFY_STANDARD | TEXTSW_NOTIFY_EDIT |
			TEXTSW_NOTIFY_DESTROY_VIEW | TEXTSW_NOTIFY_SPLIT_VIEW,
			XV_KEY_DATA, tty_notice_key, tmpfile_name,
			XV_KEY_DATA_REMOVE_PROC, tty_notice_key, cleanup_mlk,
			HELP_INFO("ttysw:termsw")
			NULL);
	if (status != TEXTSW_STATUS_OKAY) {
		goto Error_Return;
	}
	/* BUG ALERT textsw attr */
	termsw_folio->erase_line =
			(char)xv_get(termsw_public, TEXTSW_EDIT_BACK_LINE);
	termsw_folio->erase_word =
			(char)xv_get(termsw_public, TEXTSW_EDIT_BACK_WORD);
	termsw_folio->erase_char =
			(char)xv_get(termsw_public, TEXTSW_EDIT_BACK_CHAR);
	termsw_folio->pty_eot = -1;
	termsw_folio->ttysw_resized = FALSE;

	/* Initialized to ttysw */
	if ((*(xv_tty_pkg.init))(parent, termsw_public, avlist, NULL) == XV_ERROR) {
		goto Error_Return;
	}
	termsw_folio->tty_menu = (Menu) xv_get(termsw_public, WIN_MENU);


	ttysw_folio = TTY_PRIVATE(termsw_public);
	ttysw_folio->ttysw_opt |= 1 << TTYOPT_TEXT;
	ttysw_folio->ttysw_flags |= TTYSW_FL_IS_TERMSW;
	termsw_object->private_tty = (Xv_opaque) ttysw_folio;

	/*
	 * Set TERM and TERMCAP environment variables.
	 *
	 * XXX: Use terminfo here?
	 */
	putenv(cmd_term);

#ifndef __linux
	termcap = getenv("TERMCAP");
	if (!termcap || *termcap != '/')
		putenv(cmd_termcap);
#endif /* __linux */

#ifdef XV_USE_SVR4_PTYS
	/*
	 * We'll discover the tty modes as soon as we get the first input through
	 * the master side of the pty.  In the meantime, initialize cooked_echo to
	 * an arbitrary value.
	 */
	termsw_folio->cooked_echo = TRUE;
#else /* XV_USE_SVR4_PTYS */
	/* Find out what the intra-line editing, etc. chars are. */
	fd = (int)xv_get(termsw_public, TTY_TTY_FD);

#ifdef XV_USE_TERMIOS
	(void)tcgetattr(fd, &ttysw_folio->termios);
#else /* XV_USE_TERMIOS */
	(void)ioctl(fd, TIOCGETP, &ttysw_folio->sgttyb);
	(void)ioctl(fd, TIOCGETC, &ttysw_folio->tchars);
	(void)ioctl(fd, TIOCGLTC, &ttysw_folio->ltchars);
#endif /* XV_USE_TERMIOS */

	termsw_folio->cooked_echo =
			tty_iscanon(ttysw_folio) && tty_isecho(ttysw_folio);
#endif /* XV_USE_SVR4_PTYS */

	/* Set the PTY to operate as a "remote terminal". */
	fd = (int)xv_get(termsw_public, TTY_PTY_FD);

#if !defined(__linux) || defined(TIOCREMOTE)
	(void)ioctl(fd, TIOCREMOTE, &on);
#else

#    ifdef __linux
#      ifdef NO_LONGER_TRY_OUT
	/* I'm just trying out: */
	/* INCOMPLETE - was ist mit dem Zeug hier - tried out enough ??? */
/* 		ttysw_folio->termios.c_iflag |= INLCR; */
	ttysw_folio->termios.c_lflag &= (~(ICANON | ECHO | ECHOE));
	tcsetattr(ttysw_folio->ttysw_tty, TCSANOW, &ttysw_folio->termios);
	(void)tcgetattr(ttysw_folio->ttysw_tty, &ttysw_folio->termios);
	on = termsw_folio->cooked_echo =
			(tty_isecho(ttysw_folio) && tty_iscanon(ttysw_folio));
#      endif /* NO_LONGER_TRY_OUT */
#    endif /* __linux */
#endif

	ttysw_folio->remote = ttysw_folio->pending_remote = on;

	/*
	 * Restore the object as a textsw
	 */

	termsw_object->parent_data.private_data = termsw_object->private_text;

#ifdef OW_I18N
	/*
	 * Restore the preedit callbacks of textsw
	 */

	txt_folio = (Textsw_private) TEXTSW_PRIVATE_FROM_TERMSW(termsw_public);

	xv_set(termsw_public,
			WIN_IC_PREEDIT_START,
			(XIMProc) txt_folio->start_pecb_struct.callback,
			(XPointer) txt_folio->start_pecb_struct.client_data, NULL);

	xv_set(termsw_public,
			WIN_IC_PREEDIT_DRAW,
			(XIMProc) txt_folio->draw_pecb_struct.callback,
			(XPointer) txt_folio->draw_pecb_struct.client_data, NULL);

	xv_set(termsw_public,
			WIN_IC_PREEDIT_DONE,
			(XIMProc) txt_folio->done_pecb_struct.callback,
			(XPointer) txt_folio->done_pecb_struct.client_data, NULL);
#endif

	/*
	 * Build attribute list for textsw from /Tty defaults.
	 */
	defaults = defaults_array;
	def_str = defaults_get_string("text.autoIndent", "Text.AutoIndent",
			"False");
	switch (temp = defaults_lookup(def_str, auto_indent_pairs)) {
		case DO_AUTO_INDENT:
		case DO_NOT_AUTO_INDENT:
			*defaults++ = (Xv_opaque) TEXTSW_AUTO_INDENT;
			*defaults++ = (Xv_opaque) (temp == (int)DO_AUTO_INDENT);
			break;
			/* default: do nothing */
	}
	def_str = defaults_get_string("text.displayControlChars",
			"Text.DisplayControlChars", "Same_as_for_text");
	switch (temp = defaults_lookup(def_str, control_chars_use_font_pairs)) {
		case DO_USE_FONT:
		case DO_NOT_USE_FONT:
			*defaults++ = (Xv_opaque) TEXTSW_CONTROL_CHARS_USE_FONT;
			*defaults++ = (Xv_opaque) (temp == (int)DO_USE_FONT);
			break;
			/* default: do nothing */
	}
	def_str = defaults_get_string("text.insertMakesCaretVisible",
			"Text.InsertMakesCaretVisible", (char *)NULL);
	switch (temp = defaults_lookup(def_str, insert_makes_visible_pairs)) {
		case IF_AUTO_SCROLL:
		case ALWAYS:
			*defaults++ = (Xv_opaque) TEXTSW_INSERT_MAKES_VISIBLE;
			*defaults++ = (Xv_opaque) ((temp == (int)IF_AUTO_SCROLL)
					? TEXTSW_IF_AUTO_SCROLL : TEXTSW_ALWAYS);
			break;
			/* default: do nothing */
	}
	*defaults++ = 0;
	/*
	 * Point the textsw at the temporary file.  The TEXTSW_CLIENT_DATA must
	 * be the tty private data during the load so that the tty entity stream
	 * will be provided the appropriate client data during the textsw's
	 * outward call to ts_create. Note: reset TEXTSW_CLIENT_DATA in separate
	 * call because the file load happens AFTER all the attributes are
	 * processed.
	 */
	termsw_folio->layout_proc = (window_layout_proc_t) xv_get(termsw_public,
			WIN_LAYOUT_PROC);
	xv_set(termsw_public,
			ATTR_LIST, defaults_array,
			TEXTSW_CLIENT_DATA, termsw_object->private_tty,
			TEXTSW_STATUS, &status,
			OPENWIN_VIEW_ATTRS,
				TEXTSW_FILE, tmpfile_name,
				NULL,
			TEXTSW_TEMP_FILENAME, tmpfile_name,
			TEXTSW_NOTIFY_PROC, ttysw_textsw_changed,
			WIN_LAYOUT_PROC, termsw_layout,
			NULL);
	/*
	 * (void)xv_set(termsw_public, TEXTSW_CLIENT_DATA, 0, 0);
	 *
	 */
	xv_set(termsw_public,
			OPENWIN_AUTO_CLEAR, FALSE,
			WIN_BIT_GRAVITY, ForgetGravity,
			NULL);

	if (status != TEXTSW_STATUS_OKAY) {
		goto Error_Return;
	}
	/*
	 * Finish set up of fields that track state of textsw i/o for ttysw. Only
	 * AFTER they are correct, interpose on textsw i/o.
	 */
	termsw_folio->cmd_started = termsw_folio->pty_owes_newline = 0;
	termsw_folio->append_only_log =
			(int)defaults_get_boolean("term.enableEdit", "Term.EnableEdit",
			(Bool) TRUE);

	/*
	 * Must come *after* setting append_only_log to get string correct in the
	 * append_only_log toggle item, and after textsw_menu has been restored.
	 */
	ttysw_set_menu(termsw_public);
	xv_set(termsw_public, WIN_MENU, termsw_folio->text_menu, NULL);
	return (XV_OK);

  Error_Return:
	return (XV_ERROR);
}

int termsw_creation_flag;

static int termsw_init(Xv_Window parent, Termsw termsw_public,
							Attr_attribute avlist[], int *u)
{
	Xv_termsw *termsw_object = (Xv_termsw *) termsw_public;
	Termsw_folio termsw_folio;
	int dummy;


	if (!tty_notice_key) {
		tty_notice_key = xv_unique_key();
	}

	termsw_folio = xv_alloc(Termsw_folio_object);
	if (!termsw_folio)
		return (XV_ERROR);

	/* link to object; termsw is a textsw at this moment */
	termsw_object->private_data = (Xv_opaque) termsw_folio;
	termsw_folio->public_self = termsw_public;
	termsw_folio->magic = 0xF011EFFA;

	termsw_object->private_tty = (Xv_opaque) 0;

	/* Initialized to textsw */
	termsw_creation_flag = TRUE;
	if (xv_textsw_pkg.init(parent, termsw_public, avlist, &dummy) == XV_ERROR) {
		termsw_creation_flag = FALSE;
		return (XV_ERROR);
	}

	/* If we wouldn't save the TEXTSW-topmenu, we would have a few MLKs... */
	termsw_folio->saved_topmenu = xv_get(termsw_public, WIN_MENU);

	termsw_creation_flag = FALSE;
	termsw_object->private_text = termsw_object->parent_data.private_data;

	if (termsw_init_internal(parent, termsw_folio, avlist) != XV_OK) {
		free((char *)termsw_folio);
		return (XV_ERROR);
	}
	return (XV_OK);
}




/*
 * termsw_g/set_internal and termsw_destroy These routines must be careful to
 * guarantee that the value of termsw_object->parent_data.private_data is
 * preserved over a call to them. [In particular, violating this rule makes
 * the value of the field wrong after the initial call to tty_init.]  Since
 * they need to switch between the textsw's and ttysw's private data in this
 * field during the outward calls on the respective g/set procedures, this
 * requires saving and restoring the value.
 */
typedef Xv_opaque (*get_method_t)(Xv_opaque, int *, Attr_attribute, va_list);
typedef Xv_opaque (*set_method_t)(Xv_opaque, Attr_avlist);


static Xv_opaque termsw_set(Termsw termsw_folio_public, Attr_avlist avlist)
{
	Xv_termsw *termsw_object = (Xv_termsw *) termsw_folio_public;
	set_method_t fnp;
	Xv_opaque error_code;
	Xv_opaque save = termsw_object->parent_data.private_data;
	Attr_avlist attrs;
	char *buf;
	int *buf_used;
	int buf_len;

	/* First do termsw set */
	for (attrs=avlist; *attrs; attrs=attr_next(attrs)) switch ((int)attrs[0]) {
		case TTY_INPUT:
			{
				Ttysw_private ttysw = TTY_PRIVATE_TERMSW(termsw_folio_public);
				Ttysw_view_handle ttysw_view = ttysw->view;
				Termsw_folio termsw_folio =
						TERMSW_PRIVATE(termsw_folio_public);

				if (ttysw_getopt(ttysw, TTYOPT_TEXT) &&
						termsw_folio->cooked_echo) {
					buf = (char *)attrs[1];
					buf_len = (int)attrs[2];
					buf_used = (int *)attrs[3];

					*buf_used =
							ttysw_cooked_echo_cmd(ttysw_view, buf, buf_len);
					ATTR_CONSUME(*attrs);
				}
			}	/* else let tty's set do the work */
			break;
		case TERMSW_MODE:{
				Ttysw_private ttysw = TTY_PRIVATE_TERMSW(termsw_folio_public);

				ttysw_setopt(ttysw, TTYOPT_TEXT,
						((Termsw_mode) attrs[1] == TERMSW_MODE_TYPE));
				ATTR_CONSUME(*attrs);
			}
			break;

		default:
			(void)xv_check_bad_attr(TERMSW, attrs[0]);
			break;
	}

	/* Next do textsw set */
	fnp = (set_method_t) xv_textsw_pkg.set;
	if (termsw_object->private_text) {
		termsw_object->parent_data.private_data = termsw_object->private_text;
	}  /* else must be in text init and field is
	    * already correct */
	error_code = (fnp) (termsw_folio_public, avlist);
	if (error_code != (Xv_opaque) XV_OK)
		goto Return;

	/* Next do ttysw set */
	if (termsw_object->private_tty) {
		Termsw_folio termsw_folio = TERMSW_PRIVATE(termsw_folio_public);

		if (termsw_folio->destroying) {
			/* what did we see here?
			 *   +++ WIN_MENU coming from tty_ntfy.c`ttysw_destroy
			 *   +++ WIN_LAYOUT_PROC coming from txt_once.c`textsw_destroy
			 */
			if (avlist[0] != WIN_MENU && avlist[0] != WIN_LAYOUT_PROC) {
				fprintf(stderr, "%s`%s-%d: destroying: attempt to set %s\n", 
						__FILE__,__FUNCTION__,__LINE__, attr_name(avlist[0]));
			}
		}
		else {
			fnp = (set_method_t) xv_tty_pkg.set;
			termsw_object->parent_data.private_data = termsw_object->private_tty;
			error_code = (fnp) (termsw_folio_public, avlist);
		}
	}

  Return:
	termsw_object->parent_data.private_data = save;
	return (error_code);
}



static Xv_opaque termsw_get(Termsw termsw_folio_public, int *status,
							Attr_attribute attr, va_list args)
{
	Xv_termsw *termsw_object = (Xv_termsw *) termsw_folio_public;
	get_method_t fnp;
	Xv_opaque result;
	Xv_opaque save = termsw_object->parent_data.private_data;


	switch (attr) {
		case OPENWIN_VIEW_CLASS: return ((Xv_opaque) TERMSW_VIEW);
		case TEXTSW_IS_TERMSW: return (Xv_opaque)TRUE;
		case XV_IS_SUBTYPE_OF:
			{
				const Xv_pkg *pkg = va_arg(args, const Xv_pkg *);

				/* Termsw is a textsw or textsw view */
				if (pkg == TEXTSW) return ((Xv_opaque) TRUE);
			}
			break;
		default:
			break;
	}

	/* First do textsw get */
	fnp = (get_method_t) xv_textsw_pkg.get;
	if (termsw_object->private_text) {
		termsw_object->parent_data.private_data = termsw_object->private_text;
	}  /* else must be in text init and field is already correct */
	result = (fnp) (termsw_folio_public, status, attr, args);
	if (*status == XV_OK)
		goto Return;

	/* Next do ttysw get */
	if (termsw_object->private_tty) {
		*status = XV_OK;
		fnp = (get_method_t) xv_tty_pkg.get;
		termsw_object->parent_data.private_data = termsw_object->private_tty;
		/* BUG ALERT: should do equivalent of va_start/end(args) around call. */
		result = (fnp) (termsw_folio_public, status, attr, args);
		if (*status == XV_OK)
			goto Return;
	}
	/* Finally, do termsw get */
	*status = XV_ERROR;
	result = 0;

  Return:
	termsw_object->parent_data.private_data = save;
	return (result);
}



static int termsw_destroy(Termsw termsw_folio_public, Destroy_status status)
{
	Xv_termsw * termsw_object = (Xv_termsw *) termsw_folio_public;
	Xv_opaque save = termsw_object->parent_data.private_data;
	int result = XV_OK;

	switch (status) {
		case DESTROY_CHECKING:
			termsw_object->parent_data.private_data =termsw_object->private_tty;
			result = xv_tty_pkg.destroy(termsw_folio_public, status);
			if (result != XV_OK)
				break;
			termsw_object->parent_data.private_data =
					termsw_object->private_text;
			result = xv_textsw_pkg.destroy(termsw_folio_public, status);
			break;
		case DESTROY_CLEANUP:
			{
				Termsw_folio priv = TERMSW_PRIVATE(termsw_folio_public);

				priv->destroying = TRUE;

				/* remove those items that have been used in the Term Pane
				 * menu: Edit = 4, Find = 5, Extras = 6
				 */
				xv_set(priv->saved_topmenu, MENU_REMOVE, 6, NULL);
				xv_set(priv->saved_topmenu, MENU_REMOVE, 5, NULL);
				xv_set(priv->saved_topmenu, MENU_REMOVE, 4, NULL);
				xv_destroy(priv->saved_topmenu);

				if (priv->textedit) {
					xv_destroy(priv->textedit);
					priv->textedit = XV_NULL;
				}

				/* Explanation: TERMS is neither a subclass of TEXTSW nor
				 * of TTYSW - it is a subclass of OPENWIN.
				 * Here was a call to xv_textsw_pkg.destroy, but the people
				 * at SUN forgot (???) calling the xv_tty_pkg.destroy.
				 *
				 * What pointer modifications do we need here? 
				 * See above in the DESTROY_CHECKING branch...
				 */
				termsw_object->parent_data.private_data =termsw_object->private_tty;
				result = xv_tty_pkg.destroy(termsw_folio_public, status);

				termsw_object->parent_data.private_data =
						termsw_object->private_text;

				/* before this, we had a lot of MENUITEM memory leaks */
				textsw_cleanup_termsw_menuitems(termsw_object->private_text);

				result = xv_textsw_pkg.destroy(termsw_folio_public, status);
				if (result != XV_OK) break;

				/* dangerous - who might have destroyed the menu(s) ? */
				/* Has already been destroyed in ttysw_destroy, therefore
				 * we avoid "Unknown client": xv_destroy(priv->text_menu);
				 */
				xv_destroy(priv->tty_menu);

				/* BUG ALERT!  May have storage leak here. */
				xv_free(priv);
				termsw_object->private_text = XV_NULL;
			}
			break;
		case DESTROY_PROCESS_DEATH:
		default:
			break;
	}
	termsw_object->parent_data.private_data = save;
	return (result);
}


/*
 * Termsw: view related procedures
 */
static int termsw_view_init(Xv_Window parent, Termsw_view termsw_view_public, Attr_attribute avlist[], int *u)
{
	Termsw_view_handle view;
	Xv_termsw_view *view_object = (Xv_termsw_view *) termsw_view_public;
	int dummy;

	if (!tty_notice_key) {
		tty_notice_key = xv_unique_key();
	}

	view = xv_alloc(Termsw_view_object);
	if (!view) return XV_ERROR;

	/* link to object */
	view_object->private_data = (Xv_opaque) view;
	view->public_self = termsw_view_public;
	view->folio = TERMSW_PRIVATE(parent);
	view->magic = 0xF011AFFE; /* compare textsw_view: magic 0xF0110A0A */


	/* Initialized to textsw_view */
	if (xv_textsw_view_pkg.init(parent, termsw_view_public, avlist,
					&dummy) == XV_ERROR) {
		return (XV_ERROR);
	}
	view_object->private_text = view_object->parent_data.private_data;
	/* Might not want to call textsw_register_view() here */
	textsw_register_view(parent, termsw_view_public);

	if (termsw_view_init_internal(parent,termsw_view_public,avlist) != XV_OK) {
		free((char *)view);
		return XV_ERROR;
	}
	return XV_OK;
}

static Xv_opaque termsw_view_set(Termsw_view termsw_view_public, Attr_avlist avlist)
{
	Xv_termsw_view *termsw_object =
									(Xv_termsw_view *) termsw_view_public;
	set_method_t fnp;
	Xv_opaque error_code;
	Xv_opaque save = termsw_object->parent_data.private_data;
	Attr_avlist attrs;
	char *buf;
	int *buf_used;
	int buf_len;

	/* First do termsw set */
	for (attrs = avlist; *attrs; attrs = attr_next(attrs)) {
		switch ((int)attrs[0]) {
			case TTY_INPUT:{
					Ttysw_view_handle ttysw_view =
						TTY_VIEW_PRIVATE_FROM_TERMSW_VIEW(termsw_view_public);
					Termsw_folio termsw_folio =
						TERMSW_FOLIO_FROM_TERMSW_VIEW(termsw_view_public);
					Ttysw_private ttysw =
						TTY_FOLIO_FROM_TTY_VIEW_HANDLE(ttysw_view);

					if (ttysw_getopt(ttysw, TTYOPT_TEXT) &&
							termsw_folio->cooked_echo) {
						buf = (char *)attrs[1];
						buf_len = (int)attrs[2];
						buf_used = (int *)attrs[3];

						*buf_used =
								ttysw_cooked_echo_cmd(ttysw_view, buf, buf_len);
						ATTR_CONSUME(*attrs);
					}
				}	/* else let tty's set do the work */
				break;

			default:
				(void)xv_check_bad_attr(TERMSW_VIEW, attrs[0]);
				break;
		}
	}

	/* Next do textsw set */
	fnp = (set_method_t) xv_textsw_view_pkg.set;

	if (termsw_object->private_text) {
		termsw_object->parent_data.private_data = termsw_object->private_text;
	}  /* else must be in text init and field is
	    * already correct */
	error_code = (fnp) (termsw_view_public, avlist);
	if (error_code != (Xv_opaque) XV_OK)
		goto Return;

	/* Next do ttysw set */
	if (termsw_object->private_tty) {
		fnp = (set_method_t) xv_tty_view_pkg.set;
		termsw_object->parent_data.private_data = termsw_object->private_tty;
		error_code = (fnp) (termsw_view_public, avlist);
		if (error_code != (Xv_opaque) XV_OK)
			goto Return;
	}
  Return:
	termsw_object->parent_data.private_data = save;
	return (error_code);
}



static Xv_opaque termsw_view_get(Termsw_view termsw_view_public, int *status, Attr_attribute attr, va_list args)
{
	Xv_termsw_view *termsw_object =
			(Xv_termsw_view *) termsw_view_public;
	get_method_t fnp;
	Xv_opaque result;
	Xv_opaque save = termsw_object->parent_data.private_data;


	if ((attr == XV_IS_SUBTYPE_OF) && (va_arg(args, const Xv_pkg *) == TEXTSW_VIEW))
		return ((Xv_opaque) TRUE);

	switch (attr) {
		case OPENWIN_VIEW_CLASS:
			return ((Xv_opaque) TERMSW_VIEW);
		case XV_IS_SUBTYPE_OF:{
				const Xv_pkg *pkg = va_arg(args, const Xv_pkg *);

				/* Termsw is a textsw or textsw view */
				if (pkg == TEXTSW)
					return ((Xv_opaque) TRUE);
			}
			break;
		default:
			break;
	}

	/* First do textsw get */
	fnp = (get_method_t) xv_textsw_view_pkg.get;
	if (termsw_object->private_text) {
		termsw_object->parent_data.private_data = termsw_object->private_text;
	}  /* else must be in text init and field is
	    * already correct */
	result = (fnp) (termsw_view_public, status, attr, args);
	if (*status == XV_OK)
		goto Return;

	/* Next do ttysw get */
	if (termsw_object->private_tty) {
		*status = XV_OK;
		fnp = (get_method_t) xv_tty_view_pkg.get;
		termsw_object->parent_data.private_data = termsw_object->private_tty;
		/* BUG ALERT: should do equivalent of va_start/end(args) around call. */
		result = (fnp) (termsw_view_public, status, attr, args);
		if (*status == XV_OK)
			goto Return;
	}
	/* Finally, do termsw get */
	*status = XV_ERROR;
	result = 0;

  Return:
	termsw_object->parent_data.private_data = save;
	return (result);
}



static int termsw_view_destroy(Termsw_view termsw_view_public, Destroy_status status)
{
	Xv_termsw_view *termsw_view_object =
							(Xv_termsw_view *) termsw_view_public;
	Xv_opaque save = termsw_view_object->parent_data.private_data;
	Termsw_view_handle view = TERMSW_VIEW_PRIVATE(termsw_view_public);
	int result = XV_OK;

	switch (status) {
		case DESTROY_SAVE_YOURSELF:
		case DESTROY_PROCESS_DEATH:
			break;
		case DESTROY_CHECKING:
			termsw_view_object->parent_data.private_data =
					termsw_view_object->private_tty;
			result = xv_tty_view_pkg.destroy(termsw_view_public, status);
			if (result != XV_OK)
				break;
			termsw_view_object->parent_data.private_data =
					termsw_view_object->private_text;
			result = xv_textsw_view_pkg.destroy(termsw_view_public, status);
			break;
		case DESTROY_CLEANUP:
		default:
			termsw_view_object->parent_data.private_data =
					termsw_view_object->private_tty;
			result = xv_tty_view_pkg.destroy(termsw_view_public, status);
			if (result != XV_OK)
				break;	/* BUG ALERT!  May have storage leak here. */
			termsw_view_object->private_tty = XV_NULL;
			termsw_view_object->parent_data.private_data =
					termsw_view_object->private_text;

			result = xv_textsw_view_pkg.destroy(termsw_view_public, status);
			if (result != XV_OK)
				break;	/* BUG ALERT!  May have storage leak here. */
			termsw_view_object->private_text = XV_NULL;
			xv_free(view);
			break;
	}
	termsw_view_object->parent_data.private_data = save;
	return (result);
}


/*
 * The following four routines have been created to insure that there is no
 * asynchronous update of the caret during periods of popup menu operation.
 * There is a global flag that is set/cleared during menu rendering and
 * handling logic. This flag is checked in the itimer interrupt routine
 * before the caret is to be rendered.
 *
 * Note that this code finds it's way here because it is shared between ttysw
 * and txt logic. This method is used because it is simple enough to work.
 */

static short    menu_currently_active = FALSE;


Pkg_private void termsw_menu_set(void)
{
    menu_currently_active = TRUE;

    /* printf("setting menu state active\n"); */
}

Pkg_private void termsw_menu_clr(void)
{
    menu_currently_active = FALSE;

    /* printf("setting menu state INactive\n"); */
}

/*
 * more poor hacks to prevent the cursor from leaving turds
 */

static short    caret_cleared = FALSE;

Pkg_private void termsw_caret_cleared(void)
{
    caret_cleared = TRUE;
}

#ifdef SEEMS_UNUSED
/* NOT USED */
static void termsw_caret_rendered(void)
{
    caret_cleared = FALSE;
}

/*
 * this only returns correct value the next time
 */
/* NOT USED */
static int termsw_caret_invalid(void)
{
    short           ret = caret_cleared;

    caret_cleared = FALSE;

    return ret;
}
#endif /* SEEMS_UNUSED */

Pkg_private Termsw_view_handle termsw_first_view_private(Termsw_folio priv)
{
	Termsw self = TERMSW_PUBLIC(priv);
    Termsw_view	view;

	if (xv_get(self, XV_IS_SUBTYPE_OF, OPENWIN)) {
    	view = xv_get(self, OPENWIN_NTH_VIEW, 0);

		if (!view) return NULL;
	}
	else {
		fprintf(stderr, "called with a %s\n", ((Xv_pkg *)xv_get(self, XV_TYPE))->name);
		abort();
	}

	return TERMSW_VIEW_PRIVATE(view);
}

const Xv_pkg          xv_termsw_pkg = {
    "Termsw",
    (Attr_pkg) ATTR_PKG_TERMSW,
    sizeof(Xv_termsw),
    OPENWIN,
    termsw_init,
    termsw_set,
    termsw_get,
    termsw_destroy,
    NULL			/* no find proc */
};

const Xv_pkg          xv_termsw_view_pkg = {
    "Termsw_view",
    (Attr_pkg) ATTR_PKG_TERMSW_VIEW,
    sizeof(Xv_termsw_view),
    OPENWIN_VIEW,
    termsw_view_init,
    termsw_view_set,
    termsw_view_get,
    termsw_view_destroy,
    NULL			/* no find proc */
};
