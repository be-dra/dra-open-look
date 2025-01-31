#ifndef lint
char     txt_edit_c_sccsid[] = "@(#)txt_edit.c 20.58 93/06/28 DRA: $Id: txt_edit.c,v 4.13 2025/01/30 09:11:33 dra Exp $";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Programming interface to editing facilities of text subwindows.
 */

#include <xview_private/primal.h>
#include <xview_private/txt_impl.h>
#include <xview_private/ev_impl.h>
#include <xview_private/txt_18impl.h>
#include <xview_private/svr_impl.h>
#include <xview/pkg.h>
#include <xview/attrol.h>
#include <xview/notice.h>
#include <xview/frame.h>
#include <xview/server.h>

#define THRESHOLD  20
#define UPDATE_SCROLLBAR(_delta, _old_length)\
	((THRESHOLD * _delta) >= _old_length)

Xv_private char *xv_shell_prompt;

Pkg_private     Es_handle textsw_esh_for_span(Textsw_view_private view, Es_index first, Es_index last_plus_one, Es_handle to_recycle)
{
    Es_handle       esh = TSWPRIV_FOR_VIEWPRIV(view)->views->esh;

    return ((Es_handle)
	    es_get5(esh, ES_HANDLE_FOR_SPAN, first, last_plus_one,
		    to_recycle, 0, 0));
}

Pkg_private int textsw_adjust_delete_span(Textsw_private priv, Es_index *first, Es_index *last_plus_one)
/*
 * Returns: TXTSW_PE_EMPTY_INTERVAL iff *first < *last_plus_one, else
 * TEXTSW_PE_READ_ONLY iff NOTHING should be deleted, else TXTSW_PE_ADJUSTED
 * iff *first adjusted to reflect the constraint imposed by
 * priv->read_only_boundary, else 0.
 */
{
    if (*first >= *last_plus_one)
	return (TXTSW_PE_EMPTY_INTERVAL);
    if TXTSW_IS_READ_ONLY
	(priv)
	    return (TEXTSW_PE_READ_ONLY);
    if (!EV_MARK_IS_NULL(&priv->read_only_boundary)) {
	register Es_index mark_at;
	mark_at = textsw_find_mark_internal(priv,
					    priv->read_only_boundary);
	if AN_ERROR
	    (mark_at == ES_INFINITY)
		return (0);
	if (*last_plus_one <= mark_at)
	    return (TEXTSW_PE_READ_ONLY);
	if (*first < mark_at) {
	    *first = mark_at;
	    return (TXTSW_PE_ADJUSTED);
	}
    }
    return (0);
}

Pkg_private void textsw_esh_failed_msg(Textsw_private priv, char *preamble)
{
	Es_status status;
	Xv_Notice text_notice;
	Frame frame;

	status = (Es_status)es_get(priv->views->esh, ES_STATUS);
	switch (status) {
		case ES_SHORT_WRITE:
			if (TEXTSW_OUT_OF_MEMORY(priv, status)) {
				frame = xv_get(TEXTSW_PUBLIC(priv), WIN_FRAME);
				text_notice = xv_get(frame, XV_KEY_DATA, text_notice_key, NULL);

				if (!text_notice) {
					text_notice = xv_create(frame, NOTICE, NULL);

					xv_set(frame,
							XV_KEY_DATA, text_notice_key, text_notice,
							NULL);

				}
				xv_set(text_notice,
							NOTICE_BUTTON_YES, XV_MSG("Continue"),
							NOTICE_MESSAGE_STRINGS,
								(strlen(preamble)) ? preamble :
								XV_MSG("Action failed -"),
								XV_MSG("The memory buffer is full.\n\
If this is an isolated case, you can circumvent\n\
this condition by undoing the operation you just\n\
performed, storing the contents of the subwindow\n\
to a file using the text menu, and then redoing\n\
the operation.  Or, you can enlarge the size of\n\
this buffer by changing the appropriate value in\n\
the .Xdefaults file (Text.MaxDocumentSize)."),
								NULL,
							NOTICE_BUSY_FRAMES, frame, NULL,
							XV_SHOW, TRUE,
							NULL);
				break;
			}
			/* else fall through */
		case ES_CHECK_ERRNO:
		case ES_CHECK_FERROR:
		case ES_FLUSH_FAILED:
		case ES_FSYNC_FAILED:
		case ES_SEEK_FAILED:{
				frame = xv_get(TEXTSW_PUBLIC(priv), WIN_FRAME);
				text_notice = xv_get(frame, XV_KEY_DATA, text_notice_key, NULL);

				if (!text_notice) {
					text_notice = xv_create(frame, NOTICE, NULL);

					xv_set(frame,
							XV_KEY_DATA, text_notice_key, text_notice, NULL);
				}
				xv_set(text_notice,
						NOTICE_BUTTON_YES, XV_MSG("Continue"),
						NOTICE_MESSAGE_STRINGS,
							(strlen(preamble)) ? preamble :
							XV_MSG("Action failed -"),
							XV_MSG("A problem with the file system has been detected.\n\
File system is probably full."),
							NULL,
						NOTICE_BUSY_FRAMES, frame, NULL,
						XV_SHOW, TRUE,
						NULL);
				break;
			}
		case ES_REPLACE_DIVERTED:
			break;
		default:
			break;
	}
}

Pkg_private Es_index textsw_delete_span(Textsw_view_private view, Es_index first, Es_index last_plus_one, unsigned flags)
/*
 * Returns the change in indices resulting from the operation.  Result is: a)
 * usually < 0, b) 0 if span is empty or in a read_only area, c)
 * ES_CANNOT_SET if ev_delete_span fails
 */
{
	register Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
	Es_index result;

#ifdef OW_I18N
	int ret;
#endif

	result = (flags & TXTSW_DS_ADJUST)
			? textsw_adjust_delete_span(priv, &first, &last_plus_one)
			: (first >= last_plus_one) ? TXTSW_PE_EMPTY_INTERVAL : 0;
	switch (result) {
		case TEXTSW_PE_READ_ONLY:
		case TXTSW_PE_EMPTY_INTERVAL:
			result = 0;
			break;
		case TXTSW_PE_ADJUSTED:
			if (flags & TXTSW_DS_CLEAR_IF_ADJUST(0)) {
				textsw_set_selection(xv_get(VIEW_PUBLIC(view), XV_OWNER),
						ES_INFINITY, ES_INFINITY, EV_SEL_BASE_TYPE(flags));
			}
			/* Fall through to do delete on remaining span. */
		default:
			if (flags & TXTSW_DS_SHELVE) {
				extern char *xv_app_name;

				priv->trash = textsw_esh_for_span(view, first, last_plus_one,
						priv->trash);
				fprintf(stderr,
					"\n\n%s-%d: %s: RESEARCH - DO WE COME HERE?\n\n\n",
					__FILE__,__LINE__, xv_app_name);
				xv_set(TEXTSW_PUBLIC(priv), WIN_ALARM, NULL);
				/* why should we "own the clipboard selection" ???
				 * and what would be the "clipboard contents" ?
				 */
				/* this is the last remainder of the sel svc -
				 * but in the end this function did nothing.
				 * Just in case we come here: own the clipboard...
				 */
/* 				textsw_acquire_seln(priv, SELN_CLIPBOARD); */
			}

#ifdef OW_I18N
			if (flags & TXTSW_DS_RETURN_BYTES)
				ret = ev_delete_span_bytes(priv, first, last_plus_one, &result);
			else
				ret = ev_delete_span(priv->views, first, last_plus_one,
						&result);
			switch (ret) {	/* } for match */
#else
			switch (ev_delete_span(priv->views, first, last_plus_one, &result)) {
#endif /* OW_I18N */

				case 0:
					if (flags & TXTSW_DS_RECORD) {
						textsw_record_delete(priv);
					}
					break;
				case 3:
					textsw_esh_failed_msg(priv, XV_MSG("Deletion failed - "));
					/* Fall through */
				default:
					result = ES_CANNOT_SET;
					break;
			}
			break;
	}
	return (result);
}

Pkg_private Es_index textsw_do_pending_delete(Textsw_view_private view, unsigned type, int flags)
{
    register Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
    int             is_pending_delete;
    Es_index        first, last_plus_one, delta, insert;
    int             result = ev_get_selection(priv->views, &first, &last_plus_one, type);

    is_pending_delete = ((type == EV_SEL_PRIMARY) ?
			 (EV_SEL_PD_PRIMARY & result) :
			 (EV_SEL_PD_SECONDARY & result));

    if (first >= last_plus_one)
	return (0);
    textsw_take_down_caret(priv);
    insert = (flags & TFC_INSERT) ? EV_GET_INSERT(priv->views) : first;
    if (is_pending_delete &&
	(first <= insert) && (insert <= last_plus_one)) {
	if (priv->state & TXTSW_DELETE_REPLACES_CLIPBOARD) {
	    delta = textsw_delete_span(view, first, last_plus_one,
				       TXTSW_DS_ADJUST | TXTSW_DS_SHELVE |
				       TXTSW_DS_CLEAR_IF_ADJUST(type));
	} else {
	    delta = textsw_delete_span(view, first, last_plus_one,
				       TXTSW_DS_ADJUST | 
				       TXTSW_DS_CLEAR_IF_ADJUST(type));
	}
    } else {
	if (flags & TFC_SEL) {
	    textsw_set_selection(TEXTSW_PUBLIC(priv), ES_INFINITY, ES_INFINITY,
									type);
	}
	delta = 0;
    }
    return (delta);
}

Xv_public          Textsw_index
#ifdef OW_I18N
textsw_delete_wcs(abstract, first, last_plus_one)
    Textsw          abstract;
    Textsw_index        first, last_plus_one;
#else
textsw_delete(Textsw abstract, Textsw_index first, Textsw_index last_plus_one)
#endif
{
    Textsw_view_private view = VIEW_ABS_TO_REP(abstract);
    Textsw_private    priv = TSWPRIV_FOR_VIEWPRIV(view);
    int             result;

#ifdef OW_I18N
    textsw_implicit_commit(priv);
#endif
    textsw_take_down_caret(priv);
    result = textsw_delete_span(view, first, last_plus_one,
				TXTSW_DS_ADJUST | TXTSW_DS_SHELVE);
    if (result == ES_CANNOT_SET)
	return 0;
    return -result;
}

Xv_public          Textsw_index
#ifdef OW_I18N
textsw_erase_wcs(abstract, first, last_plus_one)
    Textsw          abstract;
    Es_index        first, last_plus_one;
#else
textsw_erase(Textsw abstract, Textsw_index first, Textsw_index last_plus_one)
#endif
/*
 * This routine is identical to textsw_delete EXCEPT it does not affect the
 * contents of the shelf (useful for client implementing ^W/^U or mailtool).
 */
{
	int result;
    Textsw_private priv;
    Textsw_view_private view;

	if (xv_get(abstract, XV_IS_SUBTYPE_OF, OPENWIN)) {
		priv = TEXTSW_PRIVATE(abstract);
		view = VIEW_PRIVATE(xv_get(abstract, OPENWIN_NTH_VIEW, 0));
	}
	else {
		view = VIEW_ABS_TO_REP(abstract);
		priv = TSWPRIV_FOR_VIEWPRIV(view);
		abort();
	}

#ifdef OW_I18N
	textsw_implicit_commit(priv);
#endif

	textsw_take_down_caret(priv);
	result = textsw_delete_span(view, first, last_plus_one, TXTSW_DS_ADJUST);
	if (result == ES_CANNOT_SET)
		return 0;
	return -result;
}

#ifdef OW_I18N

Xv_public          Textsw_index
textsw_delete(abstract, first, last_plus_one)
    Textsw          abstract;
    Es_index        first, last_plus_one;
{
    Textsw_view_private view = VIEW_ABS_TO_REP(abstract);
    Textsw_private    priv = TSWPRIV_FOR_VIEWPRIV(view);
    int             result;

    textsw_implicit_commit(priv);
    /*
     * This check is for the performance, because textsw_wcpos_from_mbpos()
     * is expensive process.
     */
    if (first < 0 || last_plus_one < 0 || first >= last_plus_one)
	return 0;

    textsw_take_down_caret(priv);
    first = textsw_wcpos_from_mbpos(priv, first);
    last_plus_one = textsw_wcpos_from_mbpos(priv, last_plus_one);
    result = textsw_delete_span(view, first, last_plus_one,
		TXTSW_DS_ADJUST | TXTSW_DS_SHELVE | TXTSW_DS_RETURN_BYTES);
    if (result == ES_CANNOT_SET)
	return 0;
    return -result;
}

Xv_public          Textsw_index
textsw_erase(abstract, first, last_plus_one)
    Textsw          abstract;
    Es_index        first, last_plus_one;
/*
 * This routine is identical to textsw_delete EXCEPT it does not affect the
 * contents of the shelf (useful for client implementing ^W/^U or mailtool).
 */
{
    Textsw_view_private view = VIEW_ABS_TO_REP(abstract);
    Textsw_private    priv = TSWPRIV_FOR_VIEWPRIV(view);
    int             result;

    textsw_implicit_commit(priv);
    /*
     * This check is for the performance, because textsw_wcpos_from_mbpos()
     * is expensive process.
     */
    if (first < 0 || last_plus_one < 0 || first >= last_plus_one)
	return 0;

    textsw_take_down_caret(priv);
    first = textsw_wcpos_from_mbpos(priv, first);
    last_plus_one = textsw_wcpos_from_mbpos(priv, last_plus_one);
    result = textsw_delete_span(view, first, last_plus_one,
				TXTSW_DS_ADJUST | TXTSW_DS_RETURN_BYTES);
    if (result == ES_CANNOT_SET)
	return 0;
    return -result;
}

#endif /* OW_I18N */

#ifdef OW_I18N
/*
 * If byte_return is 1, return number of deleted bytes. And if the value
 * is 0, return number of deleted characters.
 */
Pkg_private int textsw_do_edit(Textsw_view_private view, unsigned unit, unsigned direction, unsigned byte_return)
#else /* OW_I18N */
Pkg_private int textsw_do_edit(Textsw_view_private view, unsigned unit, unsigned direction)
#endif /* OW_I18N */
{
    register Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
    struct ei_span_result span;
    int             delta;

    span = ev_span_for_edit(priv->views, (int) (unit | direction));
    if ((span.flags >> 16) == 0) {

	/* Don't join with next line for ERASE_LINE_END */

	if ((unit == EV_EDIT_LINE) && (direction == 0)) {
	    Es_index        file_length = es_get_length(priv->views->esh);

	    if (span.last_plus_one < file_length)
		span.last_plus_one--;
	}
	delta = textsw_delete_span(view, span.first, span.last_plus_one,
#ifdef OW_I18N
		 ((!byte_return) ? TXTSW_DS_ADJUST :
				   TXTSW_DS_ADJUST | TXTSW_DS_RETURN_BYTES));
#else
				   TXTSW_DS_ADJUST);
#endif
	if (delta == ES_CANNOT_SET) {
	    delta = 0;
	} else {
	    TEXTSW_DO_INSERT_MAKES_VISIBLE(view);
	    textsw_record_edit(priv, unit, direction);
	    delta = -delta;
	}
    } else
	delta = 0;
    return (delta);
}

Xv_public          Textsw_index
#ifdef OW_I18N
textsw_edit_wcs(Textsw abstract, unsigned unit, unsigned count,
							unsigned direction)
#else
textsw_edit(Textsw abstract, unsigned unit, unsigned count, unsigned direction)
#endif
{
	Textsw_view_private view = VIEW_ABS_TO_REP(abstract);
	Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
	int result = 0;

#ifdef OW_I18N
	textsw_implicit_commit(priv);
#endif

	if (direction)
		direction = EV_EDIT_BACK;
	switch (unit) {
		case TEXTSW_UNIT_IS_CHAR:
			unit = EV_EDIT_CHAR;
			break;
		case TEXTSW_UNIT_IS_WORD:
			unit = EV_EDIT_WORD;
			break;
		case TEXTSW_UNIT_IS_LINE:
			unit = EV_EDIT_LINE;
			break;
		default:
			return 0;
	}
	textsw_take_down_caret(priv);

	for (; count; count--) {
#ifdef OW_I18N
		result += textsw_do_edit(view, unit, direction, 0);
#else
		result += textsw_do_edit(view, unit, direction);
#endif
	}
	return (result);
}

#ifdef OW_I18N
/*
 * Difference between this function and textsw_edit_wcs() is only
 * the last argument of textsw_do_edit() is 1, not 0.
 */
Xv_public          Textsw_index
textsw_edit(abstract, unit, count, direction)
    Textsw          abstract;
    register unsigned unit, count;
    unsigned        direction;
{
    Textsw_view_private view = VIEW_ABS_TO_REP(abstract);
    Textsw_private    priv = TSWPRIV_FOR_VIEWPRIV(view);
    int             result = 0;

    textsw_implicit_commit(priv);
    if (direction)
	direction = EV_EDIT_BACK;
    switch (unit) {
      case TEXTSW_UNIT_IS_CHAR:
	unit = EV_EDIT_CHAR;
	break;
      case TEXTSW_UNIT_IS_WORD:
	unit = EV_EDIT_WORD;
	break;
      case TEXTSW_UNIT_IS_LINE:
	unit = EV_EDIT_LINE;
	break;
      default:
	return 0;
    }
    textsw_take_down_caret(priv);
    for (; count; count--) {
	result += textsw_do_edit(view, unit, direction, 1);
    }
    return (result);
}
#endif /* OW_I18N */

Pkg_private void textsw_input_before(Textsw_private priv, Es_index *old_insert_pos, Es_index *old_length)
{
	register Ev_chain chain = priv->views;
	Ev_chain_pd_handle private = EV_CHAIN_PRIVATE(chain);

	*old_length = es_get_length(chain->esh);
	*old_insert_pos = EV_GET_INSERT(chain);
	if (private->lower_context != EV_NO_CONTEXT) {
		ev_check_insert_visibility(chain);
	}
}

Pkg_private int textsw_input_partial(Textsw_private priv, CHAR *buf, long int buf_len)
{
	int status;

	status = ev_input_partial(priv->views, buf, buf_len);
	if (status) {
		textsw_esh_failed_msg(priv, XV_MSG("Insertion failed - "));
	}
	return status;
}

Pkg_private Es_index textsw_input_after(Textsw_view_private view, Es_index old_insert_pos, Es_index old_length, int record)
{
	register Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
	Es_index delta;

	delta = ev_input_after(priv->views, old_insert_pos, old_length);
	if (delta != ES_CANNOT_SET) {
		TEXTSW_DO_INSERT_MAKES_VISIBLE(view);
		if (record) {
			Es_handle pieces;

			pieces = textsw_esh_for_span(VIEW_PRIVATE(xv_get(XV_PUBLIC(priv),
												OPENWIN_NTH_VIEW, 0)),
					old_insert_pos, old_insert_pos + delta, ES_NULL);
			textsw_record_piece_insert(priv, pieces);
		}
		if ((priv->state & TXTSW_EDITED) == 0)
			textsw_possibly_edited_now_notify(priv);
		if (priv->notify_level & TEXTSW_NOTIFY_EDIT) {
			textsw_notify_replaced(VIEW_PRIVATE(xv_get(XV_PUBLIC(priv), OPENWIN_NTH_VIEW, 0)),
					old_insert_pos, old_length, old_insert_pos,
					old_insert_pos, delta);
		}
		(void)textsw_checkpoint(priv);
	}
	return (delta);
}

#ifdef OW_I18N
static CHAR * 
memchr_wcs(buf, c, buf_len)
    CHAR		*buf;
    char		c;
    long int            buf_len;		
{
    CHAR		wc;
    int			i;
    
    mbtowc(&wc, &c, 1);
    for (i = 0; i < buf_len; i++) {
        if (*buf == wc)
            return(buf);
        else
            buf++;    
    }
    return(NULL);	
}
#endif /* OW_I18N */

Pkg_private Es_index textsw_do_input(Textsw_view_private view, CHAR *buf,
										long int buf_len, unsigned flag)
{
	register Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
	Textsw tsw = TEXTSW_PUBLIC(priv);
	register Ev_chain chain = priv->views;
	int record;
	Es_index delta, old_insert_pos, old_length;

	/* possibly use escape sequences ? */
	SERVERTRACE((44, "SERVER_JOURNALLING?\n"));
	if (xv_get(XV_SERVER_FROM_WINDOW(tsw), SERVER_JOURNALLING)) {
#ifdef OW_I18N
		if (memchr_wcs(buf, xv_shell_prompt[0], buf_len))
#else /* OW_I18N */
		if (memchr(buf, xv_shell_prompt[0], (size_t)buf_len))
#endif /* OW_I18N */
		{
			SERVERTRACE((44, " YES \n"));
			xv_set(XV_SERVER_FROM_WINDOW(tsw),
					SERVER_JOURNAL_SYNC_EVENT, 1,
					NULL);
		}
	}
	textsw_input_before(priv, &old_insert_pos, &old_length);
	if (textsw_input_partial(priv, buf, buf_len) == ES_CANNOT_SET)
		return (0);
	record = (TXTSW_DO_AGAIN(priv) &&
			((priv->func_state & TXTSW_FUNC_AGAIN) == 0));
	delta = textsw_input_after(view, old_insert_pos, old_length,
			record && (buf_len > 100));
	if (delta == ES_CANNOT_SET)
		return (0);

	if ((int)ev_get(view->e_view, EV_CHAIN_DELAY_UPDATE, XV_NULL, XV_NULL,
					XV_NULL) == 0) {
		ev_update_chain_display(chain);

		if (flag & TXTSW_UPDATE_SCROLLBAR)
			textsw_update_scrollbars(priv, TEXTSW_VIEW_NULL);
		else if ((flag & TXTSW_UPDATE_SCROLLBAR_IF_NEEDED) &&
				UPDATE_SCROLLBAR(delta, old_length))
			textsw_update_scrollbars(priv, TEXTSW_VIEW_NULL);
	}
	if (record && (buf_len <= 100))
		textsw_record_input(priv, buf, buf_len);
	return delta;
}

#ifdef OW_I18N
Xv_public Textsw_index textsw_insert_wcs(abstract, buf, buf_len)
    Textsw          abstract;
    CHAR           *buf;
    int             buf_len;
#else
Xv_public Textsw_index textsw_insert(Textsw abstract, CHAR *buf, int buf_len)
#endif
{
    Es_index        result;
    Textsw_private priv;
    Textsw_view_private view = NULL;

	if (xv_get(abstract, XV_IS_SUBTYPE_OF, OPENWIN)) {
		priv = TEXTSW_PRIVATE(abstract);
		view = VIEW_PRIVATE(xv_get(abstract, OPENWIN_NTH_VIEW, 0));
	}
	else if (xv_get(abstract, XV_IS_SUBTYPE_OF, TEXTSW_VIEW)) {
		view = VIEW_PRIVATE(abstract);
		priv = TEXTSW_PRIVATE(xv_get(abstract, XV_OWNER));
	}
	else {
    	Textsw_view_private view = VIEW_ABS_TO_REP(abstract);
    	priv = TSWPRIV_FOR_VIEWPRIV(view);
	}

#ifdef OW_I18N
    textsw_implicit_commit(priv);
#endif
    textsw_take_down_caret(priv);
    result = textsw_do_input(view, buf, (long)buf_len,
			     TXTSW_UPDATE_SCROLLBAR_IF_NEEDED);    
    return result;
}

#ifdef OW_I18N
#define INSERT_BUFSIZE 8193
Xv_public          Textsw_index
textsw_insert(abstract, buf, buf_len)
    Textsw          abstract;
    char           *buf;
    long int        buf_len;
{
    Textsw_view_private view = VIEW_ABS_TO_REP(abstract);
    register Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
    Es_index        result;
    CHAR            wbuf[INSERT_BUFSIZE];
    CHAR	   *wbuf_ptr;
    int             wbuf_len, unused, unconverted_bytes = buf_len;

    textsw_implicit_commit(priv);

    if (buf_len >= INSERT_BUFSIZE)
	wbuf_ptr=MALLOC(buf_len + 1);
    else
	wbuf_ptr=(CHAR *)wbuf;

    wbuf_len = textsw_mbstowcs_by_mblen(wbuf_ptr, buf, &unconverted_bytes, &unused);
    wbuf_ptr[wbuf_len] = NULL;

    textsw_take_down_caret(priv);
    result = textsw_do_input(view, wbuf_ptr, wbuf_len,
			     TXTSW_UPDATE_SCROLLBAR_IF_NEEDED);
    if (buf_len >= INSERT_BUFSIZE)
    	free((char *)wbuf_ptr);
    if (result) /* not error, or wbuf_len is not 0 */
	result = buf_len - unconverted_bytes;
    return (result);
}
#undef INSERT_BUFSIZE
#endif /* OW_I18N */

Xv_public          Textsw_index
#ifdef OW_I18N
textsw_replace_wcs(abstract, first, last_plus_one, buf, buf_len)
    Textsw          abstract;
    Es_index        first, last_plus_one;
    CHAR           *buf;
    long int        buf_len;
#else
textsw_replace_bytes(Textsw abstract, Textsw_index first, Textsw_index last_plus_one, CHAR *buf, long int buf_len)
#endif
/*
 * This routine is a placeholder that can be documented without casting the
 * calling sequence to textsw_replace (the preferred name) in concrete.
 */
{
#ifdef OW_I18N
    Textsw_view_private	view = VIEW_ABS_TO_REP(abstract);

    textsw_implicit_commit(TSWPRIV_FOR_VIEWPRIV(view));
#endif
    return (textsw_replace(abstract, first, last_plus_one, buf, buf_len));
}

Xv_public Textsw_index textsw_replace(Textsw abstract, Textsw_index first, Textsw_index last_plus_one, CHAR *buf, long int buf_len)
{
	Ev_mark_object saved_insert_mark;
	Es_index saved_insert, temp;
	register Ev_chain chain;
	Es_index result, insert_result;
	int lower_context;
    Textsw_private priv;
    Textsw_view_private view = NULL;

	/* hier kann TEXTSW oder TERMSW kommen */
	if (xv_get(abstract, XV_IS_SUBTYPE_OF, OPENWIN)) {
		priv = TEXTSW_PRIVATE(abstract);
		view = VIEW_PRIVATE(xv_get(abstract, OPENWIN_NTH_VIEW, 0));
	}
	else if (xv_get(abstract, XV_IS_SUBTYPE_OF, TEXTSW_VIEW)) {
		view = VIEW_PRIVATE(abstract);
		priv = TEXTSW_PRIVATE(xv_get(abstract, XV_OWNER));
	}
	else {
    	Textsw_view_private view = VIEW_ABS_TO_REP(abstract);
    	priv = TSWPRIV_FOR_VIEWPRIV(view);
	}

	chain = priv->views;
	insert_result = 0;
	textsw_take_down_caret(priv);
	/* BUG ALERT: change this to avoid the double paint. */
	if (first < last_plus_one) {
		ev_set(view->e_view, EV_CHAIN_DELAY_UPDATE, TRUE, NULL);
		result = textsw_delete_span(view, first, last_plus_one,
				TXTSW_DS_ADJUST);
		ev_set(view->e_view, EV_CHAIN_DELAY_UPDATE, FALSE, NULL);
		if (result == ES_CANNOT_SET) {
			if (ES_REPLACE_DIVERTED == (Es_status)
					es_get(priv->views->esh, ES_STATUS)) {
				result = 0;
			}
		}
	}
	else {
		result = 0;
	}

	/* changing none to all should perform correctly */
	if (result == ES_CANNOT_SET &&
			first == 0 && last_plus_one == TEXTSW_INFINITY)
		result = 1;

	if (result == ES_CANNOT_SET) {
		result = 0;
	}
	else {
		ev_check_insert_visibility(chain);
		lower_context =
				(int)ev_get(view->e_view, EV_CHAIN_LOWER_CONTEXT, XV_NULL,
				XV_NULL, XV_NULL);
		ev_set(view->e_view, EV_CHAIN_LOWER_CONTEXT, EV_NO_CONTEXT, NULL);

		saved_insert = EV_GET_INSERT(chain);
		saved_insert_mark =
				textsw_add_mark_internal(priv, saved_insert,
				TEXTSW_MARK_MOVE_AT_INSERT);
		EV_SET_INSERT(chain, first, temp);
		insert_result += textsw_do_input(view, buf, buf_len,
				TXTSW_DONT_UPDATE_SCROLLBAR);
		result += insert_result;
		saved_insert = textsw_find_mark_internal(priv, saved_insert_mark);
		if AN_ERROR
			(saved_insert == ES_INFINITY) {
			}
		else
			EV_SET_INSERT(chain, saved_insert, temp);
		textsw_remove_mark_internal(priv, saved_insert_mark);

		ev_set(view->e_view, EV_CHAIN_LOWER_CONTEXT, lower_context, NULL);
		ev_scroll_if_old_insert_visible(chain, saved_insert, (int)insert_result);
		textsw_update_scrollbars(priv, TEXTSW_VIEW_NULL);
	}
	return (result);
}

#ifdef OW_I18N
Xv_public          Textsw_index
textsw_replace_bytes(abstract, first, last_plus_one, buf, buf_len)
    Textsw          abstract;
    Es_index        first, last_plus_one;
    char           *buf;
    long int        buf_len;
{
    Ev_mark_object  saved_insert_mark;
    Es_index        saved_insert, temp;
    Textsw_view_private view = VIEW_ABS_TO_REP(abstract);
    register Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
    register Ev_chain chain = priv->views;
    Es_index        result, insert_result;
    int             lower_context;

    textsw_implicit_commit(priv);
    if (first < 0) /* wrong specfication */
	return ((Es_index) 0);
    insert_result = 0;
    textsw_take_down_caret(priv);
    /* BUG ALERT: change this to avoid the double paint. */
    if (first < last_plus_one) {
	ev_set(view->e_view, EV_CHAIN_DELAY_UPDATE, TRUE, NULL);
	first = textsw_wcpos_from_mbpos(priv, first);
	last_plus_one = textsw_wcpos_from_mbpos(priv, last_plus_one);
	result = textsw_delete_span(view, first, last_plus_one,
				    TXTSW_DS_ADJUST | TXTSW_DS_RETURN_BYTES);
	ev_set(view->e_view, EV_CHAIN_DELAY_UPDATE, FALSE, NULL);
	if (result == ES_CANNOT_SET) {
	    if (ES_REPLACE_DIVERTED == (Es_status)
		es_get(priv->views->esh, ES_STATUS)) {
		result = 0;
	    }
	}
    } else {
	/* need first for insertion pos. */
	first = textsw_wcpos_from_mbpos(priv, first);
	result = 0;
    }
  
    /* changing none to all should perform correctly */
    if(result == ES_CANNOT_SET && 
       first == 0 && 
       last_plus_one == TEXTSW_INFINITY)
      result = 1;

    if (result == ES_CANNOT_SET) {
	result = 0;
    } else {
#define REPLACE_BUFSIZE	8193
	CHAR    wbuf[REPLACE_BUFSIZE];
	CHAR   *wbuf_ptr;
	int	wbuf_len, unused, unconverted_bytes = buf_len;

	if (buf_len >= REPLACE_BUFSIZE)
		wbuf_ptr=MALLOC(buf_len + 1);
	else
		wbuf_ptr=(CHAR*)wbuf;

	wbuf_len = textsw_mbstowcs_by_mblen(wbuf_ptr, buf,
					    &unconverted_bytes, &unused);
	wbuf_ptr[wbuf_len] = NULL;

	ev_check_insert_visibility(chain);
	lower_context =
	    (int) ev_get(view->e_view, EV_CHAIN_LOWER_CONTEXT, XV_NULL, XV_NULL, XV_NULL);
	ev_set(view->e_view,
	       EV_CHAIN_LOWER_CONTEXT, EV_NO_CONTEXT, NULL);

	saved_insert = EV_GET_INSERT(chain);
	saved_insert_mark =
	    textsw_add_mark_internal(priv, saved_insert,
				     TEXTSW_MARK_MOVE_AT_INSERT);
	EV_SET_INSERT(chain, first, temp);
	insert_result += textsw_do_input(view, wbuf_ptr, wbuf_len,
					 TXTSW_DONT_UPDATE_SCROLLBAR);
	if (buf_len >= REPLACE_BUFSIZE)  
		free ((char *)wbuf_ptr);

	if (insert_result) /* not error, or wbuf_len is not 0 */
		result += buf_len - unconverted_bytes;
	saved_insert = textsw_find_mark_internal(priv, saved_insert_mark);
	if AN_ERROR
	    (saved_insert == ES_INFINITY) {
	} else
	    EV_SET_INSERT(chain, saved_insert, temp);
	textsw_remove_mark_internal(priv, saved_insert_mark);

	ev_set(view->e_view,
	       EV_CHAIN_LOWER_CONTEXT, lower_context, NULL);
	ev_scroll_if_old_insert_visible(chain,
					saved_insert, insert_result);
	textsw_update_scrollbars(priv, TEXTSW_VIEW_NULL);
#undef REPLACE_BUFSIZE
    }
    return (result);
}

/*
 *   *byte_len returns the number of bytes was unconverted.
 *   If all bytes in str is converted, and *byte_len > 0 (means that
 *   *byte_len is bigger than the number of bytes in buf),
 *   *big_len_flag returns 1, else returns 0. 
 */
pkg_private int
textsw_mbstowcs_by_mblen(wstr, str, byte_len, big_len_flag)
    register CHAR   *wstr;
    register char   *str;
    int		    *byte_len;
    int		    *big_len_flag;
{
    register int    rest_byte = *byte_len; /* for a bit performance up */
    register int    bytes;
    CHAR	   *wstr_org = wstr;

    *big_len_flag = 0;
    if (str == NULL || wstr == NULL)
	return (0);

    while (rest_byte > 0 && *str) {
	/*
 	 * Following ifdef is for performance up. Direct cast multibye to wide
	 * char is available against ascii characters with sun compiler.
 	 */
#ifdef sun
	if (isascii(*str)) {
	    *wstr++ = (CHAR)*str++;
	    rest_byte--;
	    continue;
	}
#endif
	if ((bytes = mbtowc(wstr++, str, rest_byte)) == -1) {
	    *(--wstr) = NULL;
	    goto Done;
	}
	str += bytes;
	rest_byte -= bytes;
    }
    if (rest_byte > 0)
	*big_len_flag = 1;
Done:
    *byte_len = rest_byte;
    return (wstr - wstr_org);
}


/* Skip unconverted byte untill the last limit_bytes of str. */
pkg_private int
textsw_error_skip_mbstowcs(wstr, str, byte_len, limit_bytes)
    register CHAR	*wstr;
    register char	*str;
    int			*byte_len;
    int			limit_bytes;
{
    register	rest_byte = *byte_len; /* for a bit performance up */
    register	bytes;
    CHAR       *wstr_org = wstr;

    if (str == NULL || wstr == NULL)
	return (0);

    while (rest_byte > 0 && *str) {
	/*
 	 * Following ifdef is for performance up. Direct cast multibye to wide
	 * char is available against ascii characters with sun compiler.
 	 */
#ifdef sun
	if (isascii(*str)) {
	    *wstr++ = (CHAR)*str++;
	    rest_byte--;
	    continue;
	}
#endif
	if ((bytes = mbtowc(wstr, str, rest_byte)) == -1) { /* unconverted */
	    if (rest_byte >= limit_bytes) {	/* then skip one byte */
		str++;
		rest_byte--;
		continue;
	    }
	    break;
	}
	str += bytes;
	rest_byte -= bytes;
	wstr++;
    }
    *byte_len = rest_byte;
    return (wstr - wstr_org);
}
#endif /* OW_I18N */
