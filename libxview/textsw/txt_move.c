#ifndef lint
char     txt_move_c_sccsid[] = "@(#)txt_move.c 20.91 93/06/28 DRA: $Id: txt_move.c,v 4.50 2025/02/25 17:18:24 dra Exp $";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Text subwindow move & duplicate
 */

#include <xview_private/primal.h>
#include <xview_private/ev_impl.h>
#include <xview_private/svr_impl.h>
#include <xview_private/txt_18impl.h>
#include <xview/cursor.h>
#include <pixrect/memvar.h>
#include <xview/notice.h>
#ifdef SVR4 
#include <stdlib.h> 
#endif /* SVR4 */
#include <ctype.h> 
#include <assert.h> 
#include <time.h> 

#ifdef OW_I18N		/* for reducing #ifdef OW_I18N */
#define TEXTSW_CONTENTS_I18N		TEXTSW_CONTENTS_WCS
#define textsw_insert_i18n		textsw_insert_wcs
#define textsw_set_selection_i18n	textsw_set_selection_wcs
#define textsw_delete_i18n		textsw_delete_wcs
#define TEXTSW_INSERTION_POINT_I18N	TEXTSW_INSERTION_POINT_WC
#else
#define TEXTSW_CONTENTS_I18N		TEXTSW_CONTENTS
#define textsw_insert_i18n		textsw_insert
#define textsw_set_selection_i18n	textsw_set_selection
#define textsw_delete_i18n		textsw_delete
#define TEXTSW_INSERTION_POINT_I18N	TEXTSW_INSERTION_POINT
#endif /* OW_I18N */

static int dnd_data_key = 0;
static int dnd_view_key = 0; 
static unsigned short    drag_move_arrow_data[] = {
#include <images/text_move_cursor.pr>
};

mpr_static(drag_move_arrow_pr, 16, 16, 1, drag_move_arrow_data);

struct  textsw_context {
    int    size;
    char   *sel_buffer;
};

static void display_notice (Xv_opaque public_view, int dnd_status)
{
    char 	*error_msg = "?";
    Xv_Notice	 notice;

    switch (dnd_status) {
      case XV_OK:
        return;
      case DND_TIMEOUT:
        error_msg = XV_MSG("Operation timed out");
        break;
      case DND_ILLEGAL_TARGET:
        error_msg = XV_MSG("Illegal drop target");
        break;
      case DND_SELECTION:
        error_msg = XV_MSG("Unable to acquire selection");
        break;
      case DND_ROOT:
        error_msg = XV_MSG("Root window is not a valid drop target");
        break;
      case XV_ERROR:
        error_msg = XV_MSG("Unexpected internal error");
        break;
    }
    notice = xv_create(xv_get(public_view, WIN_FRAME), NOTICE,
                    NOTICE_MESSAGE_STRINGS,
                        XV_MSG("Drag and Drop failed:"),
                        error_msg,
                        NULL,
                    XV_SHOW, TRUE,
                    NULL);
    xv_destroy(notice);
}

static void free_dnd_keys(Xv_opaque obj, int key, char *data)
{
	xv_free(data);
}

static int DndConvertProc(Dnd dnd, Atom *type, Xv_opaque *data,
									unsigned long *length, int *format)
{
	Xv_Server server = XV_SERVER_FROM_WINDOW(xv_get(dnd, XV_OWNER));
	Textsw_view_private view = (Textsw_view_private)xv_get(dnd, XV_KEY_DATA,
														dnd_view_key);
	Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);

	SERVERTRACE((765, "%s request for %s\n", 
			(char *)xv_get(dnd, SEL_RANK_NAME),
			(char *)xv_get(server, SERVER_ATOM_NAME, *type)));

	if (priv->convert_proc) {
		Textsw tsw = TEXTSW_PUBLIC(priv);
		Atom rank = xv_get(dnd, SEL_RANK);

		if ((*(priv->convert_proc))(tsw, rank, type, data, length, format))
			return TRUE;
	}

	if (*type == priv->atoms.dragdrop_done) {
		xv_set(dnd, SEL_OWN, False, NULL);
		xv_destroy_safe(dnd);
		*format = 32;
		*length = 0;
		*data = XV_NULL;
		*type = priv->atoms.null;
		return TRUE;
	}

	if (*type == XA_STRING || *type == priv->atoms.text) {
		char *buf = (char *)xv_get(dnd, XV_KEY_DATA, dnd_data_key);

		*format = 8;
		*length = strlen(buf);
		*data = (Xv_opaque) buf;
		*type = XA_STRING;
		return TRUE;
	}
#ifdef NO_XDND
#else /* NO_XDND */
	if (*type == priv->atoms.plain) {
		char *buf = (char *)xv_get(dnd, XV_KEY_DATA, dnd_data_key);

		*format = 8;
		*length = strlen(buf);
		*data = (Xv_opaque) buf;
		return TRUE;
	}
#endif /* NO_XDND */
	if (*type == priv->atoms.targets) {
		static Atom atom_list[7];

		atom_list[0] = priv->atoms.dragdrop_done;
		atom_list[1] = priv->atoms.delete;
		atom_list[2] = priv->atoms.seln_is_readonly;
		atom_list[3] = XA_STRING;
		atom_list[4] = priv->atoms.text;
		atom_list[5] = priv->atoms.targets;
		atom_list[6] = priv->atoms.timestamp;
		*format = 32;
		*length = 7;
		*data = (Xv_opaque) atom_list;
		*type = XA_ATOM;
		return TRUE;
	}
	if (*type == priv->atoms.delete) {
		if (! textsw_convert_delete(priv, view, dnd, TRUE)) return FALSE;

		*format = 32;
		*length = 0;
		*data = XV_NULL;
		*type = priv->atoms.null;
		return TRUE;
	}

	return textsw_internal_convert(priv, view, dnd, type, data,
												length, format);
}

#define MAX_CHARS_SHOWN	 5	/* most chars shown in the drag move cursor */
#define SELECTION_BUF_SIZE	MAX_CHARS_SHOWN + 3

Pkg_private void textsw_do_drag_copy_move(Textsw_view_private view, Event *ie, int is_copy)
{
    Xv_opaque public_view = VIEW_PUBLIC(view);
	Textsw tsw = xv_get(public_view, XV_OWNER);
    Textsw_private 	 priv = TSWPRIV_FOR_VIEWPRIV(view);
    Xv_Cursor dnd_cursor, dnd_accept_cursor, dnd_reject_cursor;
    CHAR tmpbuf[SELECTION_BUF_SIZE];
    CHAR buf[SELECTION_BUF_SIZE];
    CHAR *tgt;
    int	dnd_status, i;
    CHAR *buffer;
#ifdef OW_I18N
    char *buffer_mb;
#endif
    Es_index first, last_plus_one;
	Es_index to_read;
	int len;
	Drag_drop dnd;

	ev_get_selection(priv->views, &first, &last_plus_one, EV_SEL_PRIMARY);
	if (first + SELECTION_BUF_SIZE > last_plus_one)
		to_read = last_plus_one - first;
	else to_read = SELECTION_BUF_SIZE-1;
	len = textsw_es_read(priv->views->esh, tmpbuf, first, first + to_read);
	tmpbuf[len] = '\0';

	buffer = tmpbuf;
	tgt = buf;
	for (i = 0; *buffer && i < SELECTION_BUF_SIZE; i++) {
		if (! iscntrl(*buffer)) {
			*tgt++ = *buffer;
		}
		++buffer;
	}
	*tgt = '\0';

    dnd_cursor = xv_create(tsw, CURSOR,
#ifdef OW_I18N
			CURSOR_STRING_WCS, 	buf,
#else
			CURSOR_STRING, 		buf,
#endif
			CURSOR_DRAG_TYPE, (is_copy ? CURSOR_DUPLICATE : CURSOR_MOVE),
			NULL);

    dnd_accept_cursor = xv_create(tsw, CURSOR,
#ifdef OW_I18N
			CURSOR_STRING_WCS, 	buf,
#else
			CURSOR_STRING, 		buf,
#endif
			CURSOR_DRAG_TYPE, (is_copy ? CURSOR_DUPLICATE : CURSOR_MOVE),
			CURSOR_DRAG_STATE,	CURSOR_ACCEPT,
			NULL);

    dnd_reject_cursor = xv_create(tsw, CURSOR,
#ifdef OW_I18N
			CURSOR_STRING_WCS, 	buf,
#else
			CURSOR_STRING, 		buf,
#endif
			CURSOR_DRAG_TYPE, (is_copy ? CURSOR_DUPLICATE : CURSOR_MOVE),
			CURSOR_DRAG_STATE,	CURSOR_REJECT,
			NULL);

	if (! dnd_data_key) {
        dnd_data_key = xv_unique_key();
        dnd_view_key = xv_unique_key();
	}

	/* we also had a version where the owner of dnd was tsw...
	 * but then you get LOC_DRAG events for the view, but in
	 * dnd_send_drop, a mouse grab on the dnd's owner is set -
	 * but after dnd_send_drop, you may have still a lot of LOC_DRAG
	 * (or rather, MotionNotify) events for the view in the queue
	 */
    dnd = xv_create(public_view, DRAGDROP,
			SEL_CONVERT_PROC,	DndConvertProc,
			DND_TYPE, 			(is_copy ? DND_COPY : DND_MOVE),
			DND_CURSOR, 		dnd_cursor,
			DND_ACCEPT_CURSOR,	dnd_accept_cursor,
			DND_REJECT_CURSOR,	dnd_reject_cursor,
			NULL);

    ev_get_selection(priv->views, &first, &last_plus_one,EV_SEL_PRIMARY);
#ifdef OW_I18N
    buffer = (CHAR *)xv_malloc((last_plus_one - first + 1) * sizeof(CHAR));
#else
    buffer = (char *)xv_malloc((size_t)(last_plus_one - first + 1));
#endif
	len = textsw_es_read(priv->views->esh, buffer, first, last_plus_one);
	buffer[len] = '\0';

#ifdef OW_I18N
    buffer_mb = _xv_wcstombsdup(buffer);
    xv_set(dnd, XV_KEY_DATA, dnd_data_key, buffer_mb, NULL);
    if (buffer) free((char *)buffer);
#else
    xv_set(dnd,
				XV_KEY_DATA, dnd_data_key, buffer,
				XV_KEY_DATA_REMOVE_PROC, dnd_data_key, free_dnd_keys,
				NULL);
#endif
    xv_set(dnd, XV_KEY_DATA, dnd_view_key, view, NULL);

#ifdef NO_XDND
#else /* NO_XDND */
	if (! xv_get(dnd, XV_KEY_DATA, SEL_NEXT_ITEM)) {
		Atom *tl = xv_calloc(4, (unsigned)sizeof(Atom));

		tl[0] = XA_STRING;
		tl[1] = priv->atoms.text;
		tl[2] = priv->atoms.plain;

		xv_set(dnd,
				XV_KEY_DATA, SEL_NEXT_ITEM, tl,
				XV_KEY_DATA_REMOVE_PROC, SEL_NEXT_ITEM, free_dnd_keys,
				NULL);
	}
#endif /* NO_XDND */
	SERVERTRACE((333, "before dnd_send_drop(%ld)\n", dnd));
    if ((dnd_status = dnd_send_drop(dnd)) != XV_OK) {
       if (dnd_status != DND_ABORTED)
           display_notice(public_view, dnd_status);
    }
	SERVERTRACE((333, "after  dnd_send_drop(%ld)\n", dnd));

    xv_destroy(dnd_cursor);
    xv_destroy(dnd_accept_cursor);
    xv_destroy(dnd_reject_cursor);
}

static int dropdone(Selection_requestor sel)
{
	dnd_done(sel);
	return XV_OK;
}

/*
 * When a textsw gets a ACTION_DRAG_MOVE event, this routines gets called to
 * get the primary selection from another process and do move/copy
 * ACTION_DRAG_MOVE is a result of XSendEvent called by the subwindow that
 * originally sees the button-down that starts the drag move
 */
Pkg_private int textsw_do_remote_drag_copy_move(Textsw_view_private view, Event *ie, int is_copy)
{
	Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
	Textsw tsw = TEXTSW_PUBLIC(priv);
	char *string, wbuf[3];
	int word_drop;
	Textsw_mark mark = TEXTSW_NULL_MARK;

#ifdef OW_I18N
	CHAR *string_wc = NULL;
#endif
	long length;
	int format;
	Es_index ro_bdry, pos, index, temp, selpos;

	/*
	 * First, process insertion point .
	 */
	ev_set(view->e_view, EV_CHAIN_DELAY_UPDATE, TRUE, NULL);
	ro_bdry = textsw_read_only_boundary_is_at(priv);

	if (ro_bdry + 1 == ES_INFINITY)
		ro_bdry = 0;

	pos = ev_resolve_xy(view->e_view, event_x(ie), event_y(ie));

	if (pos < ro_bdry) {
		Es_index insert;

		insert = EV_GET_INSERT(priv->views);
		if (insert >= ro_bdry)
			pos = insert;
		else
			return XV_OK;
	}

	if (!dnd_data_key) dnd_data_key = xv_unique_key();

	if (! view->selreq) {
		view->selreq = xv_create(VIEW_PUBLIC(view), SELECTION_REQUESTOR, NULL);
	}

	if (dnd_decode_drop(view->selreq, ie) == XV_ERROR) {
		return XV_OK;
	}
	/* If the source and the dest is the same process, see if the
	 * drop happened within the primary selection, in which case we
	 * don't do anything.
	 */

	/* MULTI_DISPLAY: the notion "same process" is a little dangerous in
	 * multi-display-applications
	 */

	/* make sure drop doesn't happen in read-only text */
	if (TXTSW_IS_READ_ONLY(priv)) {
		dropdone(view->selreq);
		textsw_read_only_msg(priv, event_x(ie), event_y(ie));
		return XV_OK;
	}

	if (dnd_is_local(ie)) {
		/* only checking whether the drop was *within* the own
		 * primary selection: do nothing
		 */
		Es_index first, last_plus_one;

		(void)ev_get_selection(priv->views, &first, &last_plus_one,
				EV_SEL_PRIMARY);

		if (pos >= first && pos < last_plus_one) {
			dropdone(view->selreq);
			ev_set(view->e_view, EV_CHAIN_DELAY_UPDATE, FALSE, NULL);
			return XV_OK;
		}
	}

	/* attempt to fetch the string */
	length = SEL_ERROR;
	string = NULL;

#ifdef NO_XDND
#else /* NO_XDND */
	if (event_flags(ie) & DND_IS_XDND) {
		/* firefox only offers this for CLIPBOARD, but not for XdndSelection
		xv_set(view->selreq, SEL_TYPE_NAME, "text/plain;charset=ISO-8859-1", NULL);
		string = (char *)xv_get(view->selreq, SEL_DATA, &length, &format);
	*/

		if (length == SEL_ERROR) {
			if (string)
				xv_free(string);
			xv_set(view->selreq, SEL_TYPE_NAME, "text/plain", NULL);
			string = (char *)xv_get(view->selreq, SEL_DATA, &length, &format);
		}
	}
#endif /* NO_XDND */

	if (length == SEL_ERROR) {
		/* Ask for the selection to be converted into a string. */
		xv_set(view->selreq,
					SEL_TYPE, XA_STRING,
					SEL_AUTO_COLLECT_INCR, TRUE,
					NULL);

		SERVERTRACE((333, "requesting STRING\n"));
		string = (char *)xv_get(view->selreq, SEL_DATA, &length, &format);
		SERVERTRACE((333, "requested STRING, len=%ld, '%-.30s'\n",
						length, string));
	}

	if (length == SEL_ERROR) {
		if (string) xv_free(string);
		return dropdone(view->selreq);
	}

	word_drop = xv_get(view->selreq, SEL_IS_WORD_SELECTION);

	ev_set(view->e_view, EV_CHAIN_DELAY_UPDATE, FALSE, NULL);
	EV_SET_INSERT(priv->views, pos, temp);

#ifdef OW_I18N
	string_wc = _xv_mbstowcsdup(string);
	index = textsw_do_input(view, string_wc, (long int)STRLEN(string_wc),
			TXTSW_UPDATE_SCROLLBAR);
	if (string_wc)
		free((char *)string_wc);
#else
	/* preparation of the "word drop" case */
	wbuf[0] = '\0';
	if (pos == 0) xv_get(tsw, TEXTSW_CONTENTS, pos, wbuf+1, 1);
	else xv_get(tsw, TEXTSW_CONTENTS, pos-1, wbuf, 2);
	wbuf[2] = '\0';

	SERVERTRACE((666, "drop place = '%s'\n", wbuf));

	length = (Es_index) strlen(string);
	index = textsw_wrap_do_input(tsw, view, (char *)string, &length);

	selpos = textsw_smart_word_handling(priv, word_drop, wbuf, pos, length);
#endif

	/* If this is a move operation and we were able to insert the text,
	 * ask the source to delete the selection.
	 */
	SERVERTRACE((777, "is_copy=%d, index=%d\n", is_copy, index));
	/* if we are a TERMSW, then index seems to be always 0 ! */
	if (!is_copy && index) {
		long dummylen;
		char *str;

		/* if this a move operation with the *same* textsw, we have to be
		 * careful with the highlighting at 'selpos'.
		 * Not sure, but dnd_is_local only says "the drag source lives in the
		 * same application - does not have to be the same textsw.
		 * But on the other hand: if dnd_is_local returns FALSE, we don't 
		 * have to be careful.
		 */
		if (dnd_is_local(ie)) {
			mark = textsw_add_mark(tsw, selpos, TEXTSW_MARK_DEFAULTS);
		}
		SERVERTRACE((333, "requesting DELETE\n"));
		xv_set(view->selreq, SEL_TYPE_NAME, "DELETE", NULL);
		str = (char *)xv_get(view->selreq, SEL_DATA, &dummylen, &format);
		SERVERTRACE((333, "requested DELETE, len=%ld, ptr=%p\n",
						dummylen, str));
		if (str) xv_free(str);
	}

	free((char *)string);
	dropdone(view->selreq);
	TEXTSW_DO_INSERT_MAKES_VISIBLE(view);

	/* highlight the dropped text */
	if (mark != TEXTSW_NULL_MARK) {
		Textsw_index ix = textsw_find_mark(tsw, mark);

		if (ix != TEXTSW_INFINITY) {
			selpos = ix;
		}
		textsw_remove_mark(tsw, mark);
	}

	if (! xv_get(tsw, TEXTSW_IS_TERMSW)) {
		textsw_set_selection(tsw, selpos, (Es_index)(selpos + length),
						EV_SEL_PRIMARY);
	}

	/* and finally move the caret to the end of the highlighted text */
	EV_SET_INSERT(priv->views, (int)(selpos+length), temp);

	xv_set(VIEW_PUBLIC(view), WIN_SET_FOCUS, NULL);

	return XV_OK;
}
