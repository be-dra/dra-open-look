#ifndef lint
char	txt_once_c_sccsid[] = "@(#)txt_once.c 20.131 93/06/28 DRA: $Id: txt_once.c,v 4.20 2025/03/16 13:37:28 dra Exp $";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */

/*
 * Initialization and finalization of text subwindows.
 */

#include <xview_private/primal.h>
#include <xview/textsw.h>
#include <xview_private/txt_impl.h>
#include <fcntl.h>
#include <signal.h>
#include <pixrect/pr_util.h>

#ifdef __STDC__
#ifndef CAT
#define CAT(a,b)        a ## b
#endif
#endif
#include <pixrect/memvar.h>

#include <pixrect/pixfont.h>
#include <xview/rect.h>
#include <xview/win_struct.h>
#include <xview/win_notify.h>
#include <xview/window.h>
#include <xview/notice.h>
#include <xview/frame.h>
#include <xview/font.h>
#include <xview/openmenu.h>
#include <xview/defaults.h>
#include <xview/cursor.h>
#include <xview/screen.h>
#include <xview_private/attr_impl.h>
#include <xview_private/font_impl.h>
#include <xview_private/ps_impl.h>
#include <xview_private/ev_impl.h>
#ifdef OW_I18N
#include <xview_private/draw_impl.h>
#endif
#ifdef SVR4
#include <dirent.h>
#else
#include <sys/dir.h>
#endif /* SVR4 */

Xv_public Pixfont *xv_pf_open(char *fontname, Xv_server srv);
Xv_public int xv_pf_close(Pixfont *pf);

Textsw_private    textsw_head;	/* = 0; implicit for cc -A-R */
extern int termsw_creation_flag;

Pkg_private int             STORE_FILE_POPUP_KEY;
Pkg_private int             SAVE_FILE_POPUP_KEY;
Pkg_private int             LOAD_FILE_POPUP_KEY;
Pkg_private int             FILE_STUFF_POPUP_KEY;
Pkg_private int             SEARCH_POPUP_KEY;
Pkg_private int             MATCH_POPUP_KEY;
Pkg_private int             SEL_LINE_POPUP_KEY;
Pkg_private int             TEXTSW_CURRENT_POPUP_KEY;
Pkg_private int             TEXTSW_MENU_DATA_KEY;

/* BUG ALERT:  Is a "duplicate" cursor ever used? */
/*
 * short dup_cursor_data[] = { #include <images/dup_cursor.pr> };
 * mpr_static(textedit_dup_cursor_pr, 16, 16, 1, dup_cursor_data);
 */


Pkg_private void textsw_init_again(Textsw_private priv, int count)
{
	register int i;
	register int old_count = priv->again_count;
	register string_t *old_again = priv->again;

	VALIDATE_FOLIO(priv);
	priv->again_first = priv->again_last_plus_one = ES_INFINITY;
	priv->again_insert_length = 0;
	priv->again = (string_t *) ((count)
			? calloc((size_t)count, sizeof(priv->again[0]))
			: 0);
	for (i = 0; i < count; i++) {
		priv->again[i] = (i < old_count) ? old_again[i] : null_string;
	}
	for (i = priv->again_count; i < old_count; i++) {
		textsw_free_again(priv, &old_again[i]);
	}
	if (old_again)
		free((char *)old_again);
	priv->again_count = count;
}

Pkg_private void textsw_init_undo(Textsw_private priv, int count)
{
    register int    i;
    register int    old_count = priv->undo_count;
    register caddr_t *old_undo = priv->undo;

    VALIDATE_FOLIO(priv);
    priv->undo = (caddr_t *) ((count)
			       ? calloc((size_t)count, sizeof(priv->undo[0]))
			       : 0);
    for (i = 0; i < count; i++) {
	priv->undo[i] =
	    (i < old_count) ? old_undo[i] : ES_NULL_UNDO_MARK;
    }
    /*
     * old_undo[ [priv->undo_count..old_count) ] are 32-bit quantities, and
     * thus don't need to be deallocated.
     */

    /*-----------------------------------------------------------
    ... but old_undo itself is a 260 byte quantity that should
    be deallocated to avoid a noticeable memory leak.
    This is a fix for bug 1020222.  -- Mick / inserted jcb 7/20/89
    -----------------------------------------------------------*/
    if(old_undo)
          free((char *)old_undo);

    if (old_count == 0 && priv->undo != NULL )
	priv->undo[0] = es_get(priv->views->esh, ES_UNDO_MARK);
    priv->undo_count = count;
}

static void textsw_view_chain_notify(Textsw_private priv, Attr_avlist attributes)
{
    register Ev_handle e_view;
    register Textsw_view_private view = 0;
    register Attr_avlist attrs;
    Rect           *from_rect, *rect, *to_rect;

    for (attrs = attributes; *attrs; attrs = attr_next(attrs)) {
	switch ((Ev_notify_action) (*attrs)) {
	    /* BUG ALERT: following need to be fleshed out. */
	  case EV_ACTION_VIEW:
	    e_view = (Ev_handle) attrs[1];
	    view = textsw_view_for_entity_view(priv, e_view);
	    break;
	  case EV_ACTION_EDIT:
	    if (view && (priv->notify_level & TEXTSW_NOTIFY_EDIT)) {
		textsw_notify_replaced(view,
				   (Es_index) attrs[1], (Es_index) attrs[2],
				   (Es_index) attrs[3], (Es_index) attrs[4],
				       (Es_index) attrs[5]);
	    }
	    textsw_checkpoint(priv);
	    break;
	  case EV_ACTION_PAINT:
	    if (view && (priv->notify_level & TEXTSW_NOTIFY_PAINT)) {
		rect = (Rect *) attrs[1];
		textsw_notify(view, TEXTSW_ACTION_PAINTED, rect, NULL);
	    }
	    break;
	  case EV_ACTION_SCROLL:
	    if (view && (priv->notify_level & TEXTSW_NOTIFY_SCROLL)) {
		from_rect = (Rect *) attrs[1];
		to_rect = (Rect *) attrs[2];
		textsw_notify(view,
			      TEXTSW_ACTION_SCROLLED, from_rect, to_rect,
			      NULL);
	    }
	    break;
	  default:
	    LINT_IGNORE(ASSERT(0));
	    break;
	}
    }
}

static void textsw_read_defaults(Textsw_private textsw, Attr_avlist defaults)
{
	char *def_str;	/* Strings owned by defaults. */
	register Attr_attribute attr;
	Xv_opaque font = XV_NULL;
	char *name;
	Xv_opaque textsw_public = TEXTSW_PUBLIC(textsw);
	int is_client_pane = xv_get(textsw_public, WIN_IS_CLIENT_PANE);
	Xv_opaque srv = XV_SERVER_FROM_WINDOW(textsw_public);

	def_str = defaults_get_string("keyboard.deleteChar", "Keyboard.DeleteChar",
									"\177");	/* ??? Keymapping strategy? */
	textsw->edit_bk_char = def_str[0];
	def_str = defaults_get_string("keyboard.deleteWord", "Keyboard.DeleteWord", "\027");	/* ??? Keymapping
																							 * strategy? */
	textsw->edit_bk_word = def_str[0];
	def_str = defaults_get_string("keyboard.deleteLine", "Keyboard.DeleteLine", "\025");	/* ??? Keymapping
																							 * strategy? */
	textsw->edit_bk_line = def_str[0];

#ifdef OW_I18N
	textsw->need_im = (xv_get(XV_SERVER_FROM_WINDOW(textsw_public), XV_IM) &&
			xv_get(textsw_public, WIN_USE_IM)) ? TRUE : FALSE;

	/*  Drawing pre-edit text requires doing lots of replace.
	 *  This causes the memory buffer to run out very fast,
	 *  So if the user do not set this value, we will default it
	 *  to TEXTSW_INFINITY.
	 */
	if (textsw->need_im)
		textsw->es_mem_maximum =
				defaults_get_integer_check("text.maxDocumentSize",
				"Text.MaxDocumentSize", TEXTSW_INFINITY, 0,
				TEXTSW_INFINITY + 1);
	else
#endif /* OW_I18N */

		textsw->es_mem_maximum =
				defaults_get_integer_check("text.maxDocumentSize",
				"Text.MaxDocumentSize", 20000, 0, (int)(TEXTSW_INFINITY + 1));
	textsw->drag_threshold = defaults_get_integer("openWindows.dragThreshold",
									"OpenWindows.DragThreshold", 5);

#ifndef lint
	if (textsw_get_from_defaults(TEXTSW_ADJUST_IS_PENDING_DELETE, srv))
		textsw->state |= TXTSW_ADJUST_IS_PD;
	else
		textsw->state &= ~TXTSW_ADJUST_IS_PD;
	if (textsw_get_from_defaults(TEXTSW_AUTO_INDENT, srv))
		textsw->state |= TXTSW_AUTO_INDENT;
	else
		textsw->state &= ~TXTSW_AUTO_INDENT;
	if (textsw_get_from_defaults(TEXTSW_BLINK_CARET, srv))
		textsw->caret_state |= TXTSW_CARET_FLASHING;
	else
		textsw->caret_state &= ~TXTSW_CARET_FLASHING;
	if (textsw_get_from_defaults(TEXTSW_CONFIRM_OVERWRITE, srv))
		textsw->state |= TXTSW_CONFIRM_OVERWRITE;
	else
		textsw->state &= ~TXTSW_CONFIRM_OVERWRITE;
	if (textsw_get_from_defaults(TEXTSW_STORE_CHANGES_FILE, srv))
		textsw->state |= TXTSW_STORE_CHANGES_FILE;
	else
		textsw->state &= ~TXTSW_STORE_CHANGES_FILE;
	if (textsw_get_from_defaults(TEXTSW_AGAIN_RECORDING, srv))
		textsw->state &= ~TXTSW_NO_AGAIN_RECORDING;
	else
		textsw->state |= TXTSW_NO_AGAIN_RECORDING;
#endif

	if (defaults_get_boolean("text.retained", "Text.Retained", False))
		textsw->state |= TXTSW_RETAINED;
	else
		textsw->state &= ~TXTSW_RETAINED;

#ifndef lint
	textsw->multi_click_space =
			textsw_get_from_defaults(TEXTSW_MULTI_CLICK_SPACE, srv);
	textsw->multi_click_timeout =
			textsw_get_from_defaults(TEXTSW_MULTI_CLICK_TIMEOUT, srv);
	textsw->insert_makes_visible =
			(Textsw_enum) textsw_get_from_defaults(TEXTSW_INSERT_MAKES_VISIBLE,
			srv);
#endif

	/*
	 * The following go through the standard textsw_set mechanism
	 * (eventually) because they rely on all of the side-effects that
	 * accompany textsw_set calls.
	 */

#ifndef lint
	*defaults++ = attr = TEXTSW_AGAIN_LIMIT;
	*defaults++ = textsw_get_from_defaults((Attr32_attribute)attr, srv);
	*defaults++ = attr = TEXTSW_HISTORY_LIMIT;
	*defaults++ = textsw_get_from_defaults((Attr32_attribute)attr, srv);
	*defaults++ = attr = TEXTSW_AUTO_SCROLL_BY;
	*defaults++ = textsw_get_from_defaults((Attr32_attribute)attr, srv);
	*defaults++ = attr = TEXTSW_LOWER_CONTEXT;
	*defaults++ = textsw_get_from_defaults((Attr32_attribute)attr, srv);
	*defaults++ = attr = TEXTSW_UPPER_CONTEXT;
	*defaults++ = textsw_get_from_defaults((Attr32_attribute)attr, srv);

#ifdef OW_I18N
	defaults_set_locale(NULL, XV_LC_BASIC_LOCALE);
	name = xv_font_monospace();
	defaults_set_locale(NULL, NULL);

	if (name && ((int)strlen(name) > 0)) {
		font = (Xv_opaque) xv_pf_open(name, srv);
	}
	else
		font = (Xv_opaque) 0;

	if (!font) {
		Xv_opaque parent_font;
		int scale, size;

		parent_font = (Xv_opaque) xv_get(textsw_public, XV_FONT);
		scale = (int)xv_get(parent_font, FONT_SCALE);

		if (scale > 0) {
			font = (Xv_opaque) xv_find(textsw_public, FONT,
					FONT_FAMILY, FONT_FAMILY_DEFAULT_FIXEDWIDTH,
					FONT_SCALE, scale, NULL);
		}
		else {
			size = (int)xv_get(parent_font, FONT_SIZE);
			font = (Xv_opaque) xv_find(textsw_public, FONT,
					FONT_FAMILY, FONT_FAMILY_DEFAULT_FIXEDWIDTH,
					FONT_SIZE, (size > 0) ? size : FONT_SIZE_DEFAULT, NULL);
		}

		if (!font)
			font = (Xv_opaque) xv_find(NULL, FONT, NULL);
	}
	if (font) {
		attr = XV_FONT;
		*defaults++ = attr;
		*defaults++ = font;
	}
#else /* OW_I18N */

	name = xv_font_monospace();
	if (name && ((int)strlen(name) > 0)) {
		font = (Xv_opaque) xv_pf_open(name, srv);
	}
	else
		font = (Xv_opaque) 0;

	if (is_client_pane) {
		Xv_opaque parent_font;
		int scale, size;

		if (!font) {
			parent_font = xv_get(textsw_public, XV_FONT);
			scale = xv_get(parent_font, FONT_SCALE);
			if (scale > 0) {
				font = (Xv_opaque) xv_find(textsw_public, FONT,
						FONT_FAMILY, FONT_FAMILY_DEFAULT_FIXEDWIDTH,
						/* FONT_FAMILY,        FONT_FAMILY_SCREEN, */
						FONT_SCALE, (scale > 0) ? scale : FONT_SCALE_DEFAULT,
						NULL);
			}
			else {
				size = xv_get(parent_font, FONT_SIZE);
				font = (Xv_opaque) xv_find(textsw_public, FONT,
						FONT_FAMILY, FONT_FAMILY_DEFAULT_FIXEDWIDTH,
						/* FONT_FAMILY,        FONT_FAMILY_SCREEN, */
						FONT_SIZE, (size > 0) ? size : FONT_SIZE_DEFAULT, NULL);
			}
			if (font) {
				attr = XV_FONT;
				*defaults++ = attr;
				*defaults++ = font;
			}
		}
	}
	else {
		if (!font) {
			Xv_opaque parent_font = xv_get(textsw_public, XV_FONT);
			int scale = xv_get(parent_font, FONT_SCALE);

			if (scale > 0) {
				font = (Xv_opaque) xv_find(textsw_public, FONT,
						FONT_FAMILY, FONT_FAMILY_DEFAULT_FIXEDWIDTH,
						/* FONT_FAMILY,        FONT_FAMILY_SCREEN, */
						FONT_SCALE, (scale > 0) ? scale : FONT_SCALE_DEFAULT,
						NULL);
			}
			else {
				int size = xv_get(parent_font, FONT_SIZE);

				font = (Xv_opaque) xv_find(textsw_public, FONT,
						FONT_FAMILY, FONT_FAMILY_DEFAULT_FIXEDWIDTH,
						/* FONT_FAMILY,        FONT_FAMILY_SCREEN, */
						FONT_SIZE, (size > 0) ? size : FONT_SIZE_DEFAULT, NULL);
			}
		}
		if (font) {
			attr = XV_FONT;
			*defaults++ = attr;
			*defaults++ = font;
		}
	}
	if ((!font) && is_client_pane) {
		font = textsw_get_from_defaults(XV_FONT, srv);
		if (font) {
			*defaults++ = XV_FONT;
			*defaults++ = font;
		}
	}
#endif /* OW_I18N */

	*defaults++ = attr = TEXTSW_LINE_BREAK_ACTION;
	*defaults++ = textsw_get_from_defaults((Attr32_attribute)attr, srv);
	*defaults++ = attr = TEXTSW_LEFT_MARGIN;
	*defaults++ = textsw_get_from_defaults((Attr32_attribute)attr, srv);
	*defaults++ = attr = TEXTSW_RIGHT_MARGIN;
	*defaults++ = textsw_get_from_defaults((Attr32_attribute)attr, srv);
	*defaults++ = attr = TEXTSW_TAB_WIDTH;
	*defaults++ = textsw_get_from_defaults((Attr32_attribute)attr, srv);
	*defaults++ = attr = TEXTSW_CONTROL_CHARS_USE_FONT;
	*defaults++ = textsw_get_from_defaults((Attr32_attribute)attr, srv);
#endif

	*defaults = 0;
}

#define needctrlmask(c)		((0<=(c) && (c)<=31) || (128<=(c) && (c)<=159))
#define needshiftmask(c)	((64<=(c) && (c)<=95) || (192<=(c) && (c)<=223))
#define needmetamask(c)		(128<=(c) && (c)<=255)

/* 
static char *wops[] = {
	"CREATE",
	"INSERT",
	"REMOVE",
	"DESTROY",
	"GET_RIGHT_OF",
	"GET_BELOW",
	"ADJUST_RECT",
	"GET_X",
	"GET_Y",
	"GET_WIDTH",
	"GET_HEIGHT",
	"GET_RECT",
	"LAYOUT",
	"INSTALL"
};
*/

static int textsw_layout(Textsw textsw, Xv_Window child, Window_layout_op op,
		Xv_opaque d1, Xv_opaque d2, Xv_opaque d3, Xv_opaque d4, Xv_opaque d5)
{
	Textsw_private priv = TEXTSW_PRIVATE(textsw);

	switch (op) {
		case WIN_CREATE:
			if (xv_get(child, XV_IS_SUBTYPE_OF, TEXTSW_VIEW)) {
				textsw_register_view(textsw, child);
			}
		default:
			break;
	}

	if (priv->layout_proc != NULL)
		return (priv->layout_proc(textsw, child, op, d1, d2, d3, d4, d5));
	else
		return TRUE;

}

Pkg_private Textsw_view_private textsw_view_init_internal(Textsw_view_private view, Textsw_status  *status)
{
	Textsw_view view_public = VIEW_PUBLIC(view);
	Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
	PIXFONT *font = (PIXFONT *) ei_get(priv->views->eih, EI_FONT);
	Xv_opaque public_textsw = TEXTSW_PUBLIC(priv);
	Xv_object screen = xv_get(public_textsw, XV_SCREEN);

	*status = TEXTSW_STATUS_OTHER_ERROR;

	xv_set(view_public,
			WIN_RETAINED, xv_get(screen, SCREEN_RETAIN_WINDOWS),
			OPENWIN_AUTO_CLEAR, FALSE,
			WIN_BIT_GRAVITY, (long)ForgetGravity,
			XV_FONT, font,
			WIN_X_PAINT_WINDOW, TRUE,
			NULL);

 	if (! xv_get(public_textsw, OPENWIN_PW_CLASS)) {
		textsw_set_base_mask(view_public);
	}

	view->e_view = ev_create_view(priv->views, view_public, &view->rect);

	if (view->e_view != EV_NULL) {
		ev_set(view->e_view, EV_NO_REPAINT_TIL_EVENT, FALSE, NULL);

		return view;
	}

	free((char *)priv);
	free((char *)view);
	return NULL;
}


#define	TXTSW_NEED_SELN_CLIENT	(Seln_client)1

Pkg_private Textsw_private textsw_init_internal(Textsw_private priv,
			Textsw_status *status,
			int (*default_notify_proc)(Textsw, Attr_attribute *),
			Attr_attribute *attrs)
{
	register Textsw textsw = TEXTSW_PUBLIC(priv);
	Attr_attribute defaults_array[ATTR_STANDARD_SIZE];
	Attr_avlist defaults;
	Es_handle ps_esh;
	Ei_handle plain_text_eih;
	char *name = 0;
	CHAR scratch_name[MAXNAMLEN];
	Es_status es_status;
	Frame frame;
	Xv_Notice text_notice;

#ifdef OW_I18N
	CHAR name_wc[MAXNAMLEN];

#ifdef FULL_R5
	XVaNestedList va_nested_list;
#endif /* FULL_R5 */

	name_wc[0] = NULL;
#endif

	priv->magic = TEXTSW_MAGIC;

	if ((plain_text_eih = ei_plain_text_create()) == 0)
		goto Error_Return;

	defaults = defaults_array;
	/*
	 * The following go through the standard textsw_set mechanism
	 * (eventually) because they rely on all of the side-effects that
	 * accompany textsw_set calls.
	 */
	*defaults++ = TEXTSW_NOTIFY_PROC;
	*defaults++ = (Attr_attribute) default_notify_proc;
	*defaults++ = TEXTSW_INSERTION_POINT;
	*defaults++ = 0;

	*defaults = 0;
	textsw_read_defaults(priv, defaults);
	/*
	 * Special case the initial attributes that must be handled as part of
	 * the initial set up.  Optimizing out creating a memory entity_stream
	 * and then replacing it with a file causes most of the following
	 * complications.
	 */
	defaults = attr_find(defaults_array, (Attr_attribute) XV_FONT);
	if (*defaults) {
		(void)ei_set(plain_text_eih, EI_FONT, defaults[1], NULL);
		ATTR_CONSUME(*defaults);
	}
	else {
		(void)ei_set(plain_text_eih, EI_FONT, xv_get(textsw, XV_FONT), NULL);
	}
	priv->state |= TXTSW_OPENED_FONT;

#ifdef FULL_R5

#ifdef OW_I18N
	if (priv->ic
			&& (priv->
					xim_style & (XIMPreeditArea | XIMPreeditPosition |
							XIMPreeditNothing))) {
		va_nested_list =
				XVaCreateNestedList(NULL, XNLineSpace,
				(int)ei_get(plain_text_eih, EI_LINE_SPACE), NULL);
		XSetICValues(priv->ic, XNPreeditAttributes, va_nested_list, NULL);
		XFree(va_nested_list);
	}
#endif /* OW_I18N */
#endif /* FULL_R5 */

	/*
	 * Look for client provided entity_stream creation proc, and client
	 * provided data, which must be passed to the creation proc.
	 */
	defaults = attr_find(attrs, (Attr_attribute) TEXTSW_ES_CREATE_PROC);
	if (*defaults) {
		ATTR_CONSUME(*defaults);
		priv->es_create =
				(Es_handle(*)(Xv_opaque, Es_handle, Es_handle)) defaults[1];
	}
	else
		priv->es_create = ps_create;
	defaults = attr_find(attrs, (Attr_attribute) TEXTSW_CLIENT_DATA);
	if (*defaults) {
		ATTR_CONSUME(*defaults);
		priv->client_data = defaults[1];
	}

	if (termsw_creation_flag)
		priv->es_mem_maximum = 130;
	else {
		defaults = attr_find(attrs, (Attr_attribute) TEXTSW_MEMORY_MAXIMUM);
		if (*defaults) {
			priv->es_mem_maximum = (unsigned)defaults[1];
		}
		if (priv->es_mem_maximum == 0) {
			priv->es_mem_maximum = TEXTSW_INFINITY;
		}
		else if (priv->es_mem_maximum < 128)
			priv->es_mem_maximum = 128;
	}

	defaults = attr_find(attrs, (Attr_attribute) TEXTSW_FILE);
	if (*defaults) {
		ATTR_CONSUME(*defaults);
		name = (char *)defaults[1];

#ifdef OW_I18N
		if (name)
			(void)mbstowcs(name_wc, name, MAXNAMLEN);
#endif
	}

#ifdef OW_I18N
	defaults = attr_find(attrs, (Attr_attribute) TEXTSW_FILE_WCS);
	if (*defaults) {
		char name_mb[MAXNAMLEN];

		ATTR_CONSUME(*defaults);
		STRCPY(name_wc, (CHAR *) defaults[1]);
		(void)wcstombs(name_mb, name_wc, MAXNAMLEN);
		name = name_mb;
	}
	if (name_wc[0] != NULL) {	/* } for match */
		ps_esh = textsw_create_file_ps(priv, name_wc, scratch_name, &es_status);
#else /* OW_I18N */
	if (name) {
		ps_esh = textsw_create_file_ps(priv, name, scratch_name, &es_status);
#endif /* OW_I18N */

		if (es_status != ES_SUCCESS) {
			frame = (Frame) xv_get((Xv_opaque) textsw, WIN_FRAME);
			text_notice = xv_get(frame, XV_KEY_DATA, text_notice_key);
			if (!text_notice) {
				text_notice = xv_create(frame, NOTICE, NULL);

				xv_set(frame, XV_KEY_DATA, text_notice_key, text_notice, NULL);
			}
			xv_set(text_notice,
					NOTICE_MESSAGE_STRINGS,
						XV_MSG("Can't load specified file:"),
						name,
						XV_MSG("Starting with empty buffer."),
						NULL,
					NOTICE_BUTTON_YES, XV_MSG("Continue"),
					XV_SHOW, TRUE,
					NULL);

			*status = TEXTSW_STATUS_CANNOT_OPEN_INPUT;
		}
	}
	else {
		Attr_avlist attr = (Attr_avlist) attrs;
		int have_file_contents;
		char *initial_greeting;

#ifdef OW_I18N
		CHAR *initial_greeting_ws;
		int free_string = 0;
		extern CHAR _xv_null_string_wc[];
#endif

		attr = attr_find(attrs, (Attr_attribute) TEXTSW_FILE_CONTENTS);
		have_file_contents = (*attr != 0);
		/*
		 * Always look for TEXTSW_CONTENTS in defaults_array so that it is
		 * freed, even if it is not used, to avoid storage leak. Similarly,
		 * always consume TEXTSW_CONTENTS from attrs.
		 */
		defaults = attr_find(defaults_array, (Attr_attribute) TEXTSW_CONTENTS);
		attr = attr_find(attrs, (Attr_attribute) TEXTSW_CONTENTS);
		initial_greeting =
				(have_file_contents) ? "" : ((*attr) ? (char *)attr[1]
				: ((*defaults) ? (char *)defaults[1]
						: ""));

#ifdef OW_I18N
		if ((initial_greeting) && (unsigned)strlen(initial_greeting) > 0) {
			initial_greeting_ws = _xv_mbstowcsdup(initial_greeting);
			free_string = TRUE;
		}
		else {
			attr = attr_find(attrs, (Attr_attribute) TEXTSW_FILE_CONTENTS_WCS);
			if (!have_file_contents)
				have_file_contents = (*attr != 0);
			/*
			 * Always look for TEXTSW_CONTENTS_WCS in defaults_array so that it is
			 * freed, even if it is not used, to avoid storage leak. Similarly,
			 * always consume TEXTSW_CONTENTS_WCS from attrs.
			 */
			defaults =
					attr_find(defaults_array,
					(Attr_attribute) TEXTSW_CONTENTS_WCS);
			attr = attr_find(attrs, (Attr_attribute) TEXTSW_CONTENTS_WCS);
			initial_greeting_ws =
					(have_file_contents) ? _xv_null_string_wc
					: ((*attr) ? (CHAR *) attr[1]
					: ((*defaults) ? (CHAR *) defaults[1]
							: _xv_null_string_wc));
		}
		if (!initial_greeting_ws) {
			initial_greeting_ws = _xv_null_string_wc;
		}

		ps_esh = es_mem_create((unsigned)STRLEN(initial_greeting_ws),
				initial_greeting_ws);
		if (free_string)
			free((char *)initial_greeting_ws);
#else /* OW_I18N */
		ps_esh = es_mem_create((unsigned)strlen(initial_greeting),
				initial_greeting);
#endif /* OW_I18N */

		ps_esh = textsw_create_mem_ps(priv, ps_esh);
		if (*defaults) {
			ATTR_CONSUME(*defaults);
			free((char *)defaults[1]);
		}
		if (*attr) {
			ATTR_CONSUME(*attr);
		}
	}

	if (ps_esh == ES_NULL)
		goto Error_Return;
	/*
	 * Make the view chain and the initial view(s).
	 */
	priv->views = ev_create_chain(ps_esh, plain_text_eih);
	(void)ev_set((Ev_handle) 0, priv->views,
			EV_CHAIN_DATA, priv,
			EV_CHAIN_NOTIFY_PROC, textsw_view_chain_notify,
			EV_CHAIN_NOTIFY_LEVEL, EV_NOTIFY_ALL,
			NULL);

	/*
	 * Set the default, and then the client's, attributes.
	 */
	if (!xv_get(XV_PUBLIC(priv), OPENWIN_NTH_VIEW, 0)) {
		(void)textsw_set_null_view_avlist(priv, defaults_array);
		(void)xv_set_avlist(textsw, defaults_array);

		(void)textsw_set_null_view_avlist(priv, attrs);
	}
	priv->layout_proc =
			(int (*)(Textsw, Xv_Window, Window_layout_op, Xv_opaque, Xv_opaque,
					Xv_opaque, Xv_opaque, Xv_opaque))xv_get(textsw,
			WIN_LAYOUT_PROC);

	(void)xv_set_avlist(textsw, (Attr_avlist) attrs);
	/* This xv_set call should be combined with the xv_set_avlist call.
	 * BUG ALERT:  This code assumes that the default "text.enableScrollbar"
	 * is always TRUE, since that is what OPEN LOOK requires.
	 * This default should be eliminated in "txt_attr.c".
	 */
	(void)xv_set(textsw,
			WIN_LAYOUT_PROC, textsw_layout,
			OPENWIN_ADJUST_FOR_VERTICAL_SCROLLBAR, TRUE,
			XV_FOCUS_RANK, XV_FOCUS_PRIMARY, NULL);

	/*
	 * Make last_point/_adjust/_ie_time close (but not too close) to current
	 * time to avoid overflow in tests for multi-click.
	 */
	(void)gettimeofday(&priv->last_point, (struct timezone *)0);
	priv->last_point.tv_sec -= 1000;
	priv->last_adjust = priv->last_point;
	priv->last_ie_time = priv->last_point;
	/*
	 * Final touchups.
	 */
	priv->trash = ES_NULL;
	priv->to_insert_next_free = priv->to_insert;
	priv->to_insert_counter = 0;
	priv->span_level = EI_SPAN_POINT;
	SET_TEXTSW_TIMER(&priv->timer);
	EV_INIT_MARK(priv->save_insert);
	priv->owed_by_filter = 0;
	/*
	 * Get the user filters in the ~/.textswrc file. Note that their
	 * description is read only once per process, and shared among all of the
	 * folios in each process.
	 */
	if (textsw_head) {
		priv->key_maps = textsw_head->key_maps;
	}
	else
		(void)textsw_parse_rc(priv);
	/*
	 * Initialize selection service data. Note that actual hookup will only
	 * be attempted when necessary.
	 */
	timerclear(&priv->selection_died);
	*status = TEXTSW_STATUS_OKAY;
	priv->state |= TXTSW_INITIALIZED;
	priv->temp_filename = NULL;
	(void)textsw_menu_init(priv);

	/*
	 * Link this priv in.
	 */
	if (textsw_head)
		priv->next = textsw_head;
	textsw_head = priv;

	/* set delete replaces clipboard flag based on resource
	   Text.DeleteReplacesClipboard */

	if (defaults_get_boolean("text.deleteReplacesClipboard",
					"Text.DeleteReplacesClipboard", False)) {
		priv->state |= TXTSW_DELETE_REPLACES_CLIPBOARD;
	}

#ifdef OW_I18N
	priv->ic = NULL;
	EV_INIT_MARK(priv->preedit_start);
	priv->blocking_newline = FALSE;
	priv->locale_is_ale = (int)ei_get(plain_text_eih, EI_LOCALE_IS_ALE);
	(void)getwidth(&priv->euc_width);
	priv->euc_width._eucw2++;	/* increment one for SS2 */
	priv->euc_width._eucw3++;	/* increment one for SS3 */
	textsw_init_convpos(priv);
	/*
	 * Textsw should have IC even if TEXTSW_READ_ONLY is True.
	 * Because read_only mode may be changed to edit mode later on
	 * by setting TEXTSW_READ_ONLY to TRUE, or by loading another file.
	 */
	if (priv->need_im) {
		Xv_private void textsw_pre_edit_start();
		Xv_private void textsw_pre_edit_draw();
		Xv_private void textsw_pre_edit_done();

		/* Set preedit callbacks */
		xv_set(textsw,
				WIN_IC_PREEDIT_START,
				(XIMProc) textsw_pre_edit_start, (XPointer) textsw,
				WIN_IC_PREEDIT_DRAW,
				(XIMProc) textsw_pre_edit_draw, (XPointer) textsw,
				WIN_IC_PREEDIT_DONE,
				(XIMProc) textsw_pre_edit_done, (XPointer) textsw, NULL);

		priv->start_pecb_struct.callback = (XIMProc) textsw_pre_edit_start;
		priv->start_pecb_struct.client_data = (XPointer) textsw;

		priv->draw_pecb_struct.callback = (XIMProc) textsw_pre_edit_draw;
		priv->draw_pecb_struct.client_data = (XPointer) textsw;

		priv->done_pecb_struct.callback = (XIMProc) textsw_pre_edit_done;
		priv->done_pecb_struct.client_data = (XPointer) textsw;
	}
#endif /* OW_I18N */

	return (priv);

  Error_Return:
	free((char *)priv);
	return (0);


}



Pkg_private void textsw_setup_scrollbar( Scrollbar sb)
{

    if (sb) xv_set(sb,
		      SCROLLBAR_PIXELS_PER_UNIT, 1L,
		      SCROLLBAR_OBJECT_LENGTH, 0L,
		      SCROLLBAR_VIEW_START, 0L,
		      SCROLLBAR_VIEW_LENGTH, 0L,
		      SCROLLBAR_COMPUTE_SCROLL_PROC, textsw_compute_scroll,
		      SCROLLBAR_SPLITTABLE, TRUE,
		      SCROLLBAR_DIRECTION, SCROLLBAR_VERTICAL,
		      WIN_COLLAPSE_MOTION_EVENTS, TRUE,
		      NULL);

}

static void textsw_destroy_popup(int key_data_name, Textsw textsw, Frame parent_frame)
{
	Frame popup_frame = xv_get(parent_frame, XV_KEY_DATA, key_data_name);

	if (popup_frame &&
		(textsw == xv_get(popup_frame,XV_KEY_DATA,TEXTSW_CURRENT_POPUP_KEY)))
	{
		xv_set(parent_frame, XV_KEY_DATA, key_data_name, 0, NULL);
		xv_destroy(popup_frame);
	}
}

Pkg_private void textsw_cleanup_termsw_menuitems(Xv_opaque opriv)
{
	Textsw_private priv = (Textsw_private)opriv;
	Menu_item *mis = priv->menu_table;

	xv_destroy(mis[TEXTSW_MENU_EDIT_CMDS]);
	xv_destroy(mis[TEXTSW_MENU_FIND_CMDS]);
	xv_destroy(mis[TEXTSW_MENU_EXTRAS_CMDS]);
}

/* Diese Funktion wird aus textsw_destroy aufgerufen.
   Problem: da XView die destroy-Methode erst für Subklassen aufruft,
   kommt textsw_destroy FRUEHER dra als openwin_destroy - und DORT werden
   die Views vernichtet, d.h. dann kommt man in textsw_view_destroy an.
   Ich habe ja hier die Instanzenvariablen ViewPriv::next und Textsw::first_view
   eliminiert.
   Jetzt muss ich wohl auch dafuer sorgen, dass hier VOR dem Aufruf von
   ev_destroy_chain_and_views(priv->views) bei allen Views der Pointer
   e_view auf NULL gesetzt wird, damit danach in textsw_view_cleanup
   bei ev_destroy(view->e_view) nichts mehr Schlimmes passiert (doppel-free
   etc)
   Dabei muss man bedenken, dass es ja auch (bei Join Views) die 
   Vernichtung nur von Views auch gibt!
*/

static void textsw_folio_cleanup(Textsw_private priv)
{
	Key_map_handle this_key, next_key;
	Textsw textsw = TEXTSW_PUBLIC(priv);
	Textsw_view vp;
	Frame parent_frame = xv_get(textsw, WIN_FRAME);
	Menu top;

	textsw_init_again(priv, 0);	/* Flush AGAIN info */
	textsw_destroy_esh(priv, priv->views->esh);

	/* destroy any popups which are accessing this textsw */

	textsw_destroy_popup(STORE_FILE_POPUP_KEY, textsw, parent_frame);
	textsw_destroy_popup(SAVE_FILE_POPUP_KEY, textsw, parent_frame);
	textsw_destroy_popup(LOAD_FILE_POPUP_KEY, textsw, parent_frame);
	textsw_destroy_popup(FILE_STUFF_POPUP_KEY, textsw, parent_frame);
	textsw_destroy_popup(SEARCH_POPUP_KEY, textsw, parent_frame);
	textsw_destroy_popup(MATCH_POPUP_KEY, textsw, parent_frame);
	textsw_destroy_popup(SEL_LINE_POPUP_KEY, textsw, parent_frame);

	if (priv->state & TXTSW_OPENED_FONT) {
		PIXFONT *font = (PIXFONT *)
				ei_get(priv->views->eih, EI_FONT);

		xv_pf_close(font);
	}
	priv->views->eih = ei_destroy(priv->views->eih);

	OPENWIN_EACH_VIEW(textsw, vp)
		Textsw_view_private view = VIEW_PRIVATE(vp);
		view->e_view = NULL;
		view->textsw_priv = NULL;
	OPENWIN_END_EACH
	ev_destroy_chain_and_views(priv->views);

#ifdef OW_I18N
	textsw_destroy_convpos(priv);
#endif

	priv->caret_state &= ~TXTSW_CARET_ON;
	textsw_remove_timer(priv);
	/*
	 * Unlink the textsw from the chain.
	 */
	if (priv == textsw_head) {
		textsw_head = priv->next;
		if (priv->next == 0) {
			/*
			 * Last textsw in process, so free key_maps.
			 */

			for (this_key = priv->key_maps; this_key; this_key = next_key) {
				next_key = this_key->next;
				free((char *)this_key);
			}
		}
	}
	else {
		Textsw_private temp;

		for (temp = textsw_head; temp; temp = temp->next) {
			if (priv == temp->next) {
				temp->next = priv->next;
				break;
			}
		}
	}

	/*
	 *  The following code should reduce the memory leakage
	 *  in a textsw create/destroy cycle from 8k to 1k.
	 */
	if ((top = textsw_menu_get(priv))) {
		xv_destroy(top);
	}
	if (priv->menu_table) free(priv->menu_table);
	if (priv->sub_menu_table) xv_free(priv->sub_menu_table);
	if (priv->undo) free(priv->undo);
	if (priv->selbuffer) xv_free(priv->selbuffer);

	if (priv->temp_filename) xv_free(priv->temp_filename);

	((Xv_textsw *)textsw)->private_data = XV_NULL;
	free((char *)priv);
}

static void textsw_view_cleanup(Textsw_view_private view)
{
	Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);

	view->view_state |= TXTSW_VIEW_DYING;
	if (priv) {
		/* Warn client that view is dying BEFORE killing it. */
		if (priv->notify_level & TEXTSW_NOTIFY_DESTROY_VIEW)
			textsw_notify(view, TEXTSW_ACTION_DESTROY_VIEW, NULL);

#ifdef BEFORE_DRA_CHANGED_IT
		Textsw tsw = XV_PUBLIC(priv);
		int nviews = (int)xv_get(tsw, OPENWIN_NVIEWS);
		/* This is for the panel menu of textedit */

		/* Neither in the original textedit.c nor in my texted
		 * are those MENU_CLIENT_DATA used...
		 */
		if ((!(priv->state & TXTSW_DESTROY_ALL_VIEWS)) && (nviews >= 1)) {
			int i;
			Textsw_view firstview = xv_get(tsw, OPENWIN_NTH_VIEW, 0);

			/* 	was soll das sein */

			for (i = TXTSW_FILE_SUB_MENU; i <= TXTSW_FIND_SUB_MENU; i++) {
				if (priv->sub_menu_table[i]) {
					xv_set(priv->sub_menu_table[i],
						MENU_CLIENT_DATA, firstview,
						NULL);
				}
			}
			for (i = (int)TEXTSW_MENU_FILE_CMDS; i < (int)TEXTSW_MENU_LAST_CMD; i++) {
				if (priv->menu_table[i])
					xv_set(priv->menu_table[i],
						MENU_CLIENT_DATA, firstview,
						NULL);
			}
		}
#endif /* BEFORE_DRA_CHANGED_IT */
	}
	else {
		/* Warn client that view is dying BEFORE killing it. */
		textsw_notify(view, TEXTSW_ACTION_DESTROY_VIEW, NULL);
	}
	/* Destroy all of the view's auxillary objects and any back links */
	if (view->e_view) ev_destroy(view->e_view);
	free((char *)view);
}


Pkg_private int textsw_destroy(Textsw tsw, Destroy_status status)
{
	register Textsw_private priv = TEXTSW_PRIVATE(tsw);
	Frame frame;
	Xv_Notice text_notice;

	switch (status) {
		case DESTROY_CHECKING:
			if (textsw_has_been_modified(tsw) &&
					(priv->ignore_limit != TEXTSW_INFINITY)) {
				int result;

				frame = (Frame) xv_get(tsw, WIN_FRAME);
				text_notice = xv_get(frame, XV_KEY_DATA, text_notice_key);

				if (!text_notice) {
					text_notice = xv_create(frame, NOTICE, NULL);

					xv_set(frame,
							XV_KEY_DATA, text_notice_key, text_notice, NULL);
				}
				xv_set(text_notice,
					NOTICE_MESSAGE_STRINGS,
						XV_MSG("The text has been edited.\n\
\n\
You may discard edits now and quit, or cancel\n\
the request to Quit and go back and either save the\n\
contents or store the contents as a new file."),
						NULL,
					NOTICE_BUTTON_YES, XV_MSG("Cancel, do NOT Quit"),
					NOTICE_BUTTON, XV_MSG("Discard edits, then Quit"), 123,
					NOTICE_STATUS, &result,
					XV_SHOW, TRUE,
					NULL);

				if ((result == ACTION_STOP) || (result == NOTICE_YES)
						|| (result == NOTICE_FAILED)) {
					return (XV_ERROR);
				}
				else {
					(void)textsw_reset(tsw, 0, 0);
					(void)textsw_reset(tsw, 0, 0);
				}
			}
			break;
		case DESTROY_CLEANUP:
			{
				priv->state |= TXTSW_DESTROY_ALL_VIEWS;

				textsw_new_selection_destroy(tsw);
				xv_set(tsw, WIN_LAYOUT_PROC, priv->layout_proc, NULL);

				frame = (Frame) xv_get(tsw, WIN_FRAME);
				/* Problems ahead, if several TEXTSW live within
				 * the same FRAME
				 */
				text_notice = xv_get(frame, XV_KEY_DATA, text_notice_key);
				if (text_notice) {
					xv_set(frame, XV_KEY_DATA, text_notice_key, XV_NULL, NULL);
#ifdef LEADS_TO_UNKNOWN_CLIENT_ERRORS
					xv_destroy_status(text_notice, DESTROY_CLEANUP);
					xv_destroy(text_notice);
#endif /* LEADS_TO_UNKNOWN_CLIENT_ERRORS */
				}
				textsw_folio_cleanup(priv);
				break;
			}
		case DESTROY_PROCESS_DEATH:
			textsw_destroy_esh(priv, priv->views->esh);
			break;

		default:	/* Conservative in face of new cases. */
			break;
	}
	if (status == DESTROY_PROCESS_DEATH || status == DESTROY_CLEANUP) {
		(void)notify_remove((Notify_client) tsw);
		(void)notify_remove((Notify_client) priv);
	}
	return (XV_OK);
}

Pkg_private int textsw_view_destroy(Textsw_view view_public, Destroy_status status)
{
	register Textsw_view_private view = VIEW_PRIVATE(view_public);
	register Textsw tsw = xv_get(view_public, XV_OWNER);
	Frame frame;
	Xv_Notice text_notice;
	int nviews = (int)xv_get(tsw, OPENWIN_NVIEWS);
	Textsw_private priv;

	switch (status) {
		case DESTROY_CHECKING:
			priv = TEXTSW_PRIVATE(tsw);
			if (nviews <= 1 &&
					textsw_has_been_modified(tsw) &&
					(priv->ignore_limit != TEXTSW_INFINITY))
			{
				int result;

				frame = (Frame) xv_get(tsw, WIN_FRAME);
				text_notice = xv_get(frame, XV_KEY_DATA, text_notice_key);
				if (!text_notice) {
					text_notice = xv_create(frame, NOTICE, NULL);
					xv_set(frame,XV_KEY_DATA,text_notice_key,text_notice,NULL);
				}
				xv_set(text_notice,
					NOTICE_MESSAGE_STRINGS,
						XV_MSG("The text has been edited.\n\
\n\
You may discard edits now and quit, or cancel\n\
the request to Quit and go back and either save the\n\
contents or store the contents as a new file."),
						NULL,
						NOTICE_BUTTON_YES, XV_MSG("Cancel, do NOT Quit"),
						NOTICE_BUTTON, XV_MSG("Discard edits, then Quit"), 123,
						NOTICE_STATUS, &result,
						XV_SHOW, TRUE,
						NULL);

				if ((result == ACTION_STOP) || (result == NOTICE_YES)
						|| (result == NOTICE_FAILED)) {
					return (XV_ERROR);
				}
				else {
					(void)textsw_reset(tsw, 0, 0);
					(void)textsw_reset(tsw, 0, 0);
				}
			}
			break;
		case DESTROY_CLEANUP:
#ifdef OW_I18N
			/*
			 * When focus window set in ic is equal to the destroying view window,
			 * set XNFocusWindow with the other view window. And then unsetting of
			 * ic focus and after care of caret have to be done because KBD_DONE
			 * event will not be sent to textsw in this case.
			 */
			if (priv->ic) {
				Xv_Drawable_info *info;
				XID ic_xid;

				XGetICValues(priv->ic, XNFocusWindow, &ic_xid, NULL);
				DRAWABLE_INFO_MACRO(view_public, info);

				if (xv_xid(info) == ic_xid) {
					Xv_Window view_win;
					int view_nbr;
					Xv_object frame;

					for (view_nbr = 0;; view_nbr++) {
						view_win = xv_get(tsw, OPENWIN_NTH_VIEW, view_nbr);
						if (!view_win)
							break;
						if (view_public != view_win) {
							DRAWABLE_INFO_MACRO(view_win, info);
							window_set_ic_focus_win(tsw, priv->ic,
									xv_xid(info));
							XUnsetICFocus(priv->ic);

							/* after care of caret */
							textsw_hide_caret(priv);
							priv->state &= ~TXTSW_HAS_FOCUS;
							if (frame = xv_get(TEXTSW_PUBLIC(priv), WIN_FRAME))
								frame_kbd_done(frame, TEXTSW_PUBLIC(priv));
							textsw_stop_blinker(priv);

							priv->focus_view = view_win;
							break;
						}
					}
				}
			}
#endif /* OW_I18N */

			/* If menus access the view being destroyed, then set
			   TEXTSW_MENU_DATA_KEY to another view.
			 */
			/* If this is the last view, we shouldn't touch priv any more -
			 * that has been free'd in textsw_destroy
			 */
			if (nviews > 1) {
				priv = TEXTSW_PRIVATE(tsw);

				if (priv != NULL) {
					if (view_public == xv_get(textsw_menu_get(priv),
											XV_KEY_DATA, TEXTSW_MENU_DATA_KEY))
					{
						int v;
						Textsw_view another_view;

						for (v = 0;; v++) {
							another_view = xv_get(tsw, OPENWIN_NTH_VIEW, v);
							if (!another_view)
								break;
							if (view_public != another_view) {
								xv_set(textsw_menu_get(priv), XV_KEY_DATA,
									TEXTSW_MENU_DATA_KEY, another_view, NULL);
								break;
							}
						}
					}
				}
			}

			xv_destroy(view->drop_site);
			if (view->selreq) xv_destroy(view->selreq);
			textsw_view_cleanup(view);
			break;

		default:	/* Conservative in face of new cases. */
			break;
	}
	return (XV_OK);
}


Xv_private void textsw_register_view(Textsw textsw, Xv_Window newview)
{
	Textsw_private priv = TEXTSW_PRIVATE(textsw);
	Textsw_view_private view = VIEW_PRIVATE(newview);
	CHAR *name;
	Textsw_view vp;
	int nviews = (int)xv_get(textsw, OPENWIN_NVIEWS);

	OPENWIN_EACH_VIEW(textsw, vp)
		if (vp == newview) return;	/* This view is already registered */
	OPENWIN_END_EACH

	if (textsw_file_name(priv, &name))
		textsw_notify(view, TEXTSW_ACTION_USING_MEMORY, NULL);

#ifdef OW_I18N
	else {
		char *name_mb = _xv_wcstombsdup(name);

		textsw_notify(view, TEXTSW_ACTION_LOADED_FILE, name_mb,
				TEXTSW_ACTION_LOADED_FILE_WCS, name, NULL);
		if (name_mb)
			free(name_mb);
	}
#else
	else
		textsw_notify(view, TEXTSW_ACTION_LOADED_FILE, name, NULL);
#endif /* OW_I18N */

	if (nviews >= 1) {
		/* is a split */
		Textsw_view_private other;

		OPENWIN_EACH_VIEW(textsw, vp)
			if (vp != newview) {
				other = VIEW_PRIVATE(vp);
				ev_set(view->e_view, EV_SAME_AS, other->e_view, NULL);
				break;
			}
		OPENWIN_END_EACH
	}
}
