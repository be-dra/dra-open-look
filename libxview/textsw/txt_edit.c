#ifndef lint
char     txt_edit_c_sccsid[] = "@(#)txt_edit.c 20.58 93/06/28 DRA: $Id: txt_edit.c,v 4.14 2026/07/30 07:04:27 dra Exp $";
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

Pkg_private int textsw_adjust_delete_span(Textsw_private priv, Es_index *first,
									Es_index *last_plus_one)
/*
 * Returns: TXTSW_PE_EMPTY_INTERVAL iff *first < *last_plus_one, else
 * TEXTSW_PE_READ_ONLY iff NOTHING should be deleted, else TXTSW_PE_ADJUSTED
 * iff *first adjusted to reflect the constraint imposed by
 * priv->read_only_boundary, else 0.
 */
{
	if (*first >= *last_plus_one) return (TXTSW_PE_EMPTY_INTERVAL);
	if (TXTSW_IS_READ_ONLY(priv)) return (TEXTSW_PE_READ_ONLY);
	if (!EV_MARK_IS_NULL(&priv->read_only_boundary)) {
		register Es_index mark_at;

		mark_at = textsw_find_mark_internal(priv, priv->read_only_boundary);
		if (AN_ERROR(mark_at == ES_INFINITY)) return (0);
		if (*last_plus_one <= mark_at) return (TEXTSW_PE_READ_ONLY);
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

Pkg_private Es_index textsw_delete_span(Textsw_view_private view,
				Es_index first, Es_index last_plus_one, unsigned flags)
/*
 * Returns the change in indices resulting from the operation. Result is:
 * a) usually < 0,
 * b) 0 if span is empty or in a read_only area,
 * c) * ES_CANNOT_SET if ev_delete_span fails
 */
{
	register Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
	Es_index result;

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
			switch (ev_delete_span(priv->views, first, last_plus_one, &result))
			{
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

Xv_public Textsw_index textsw_delete(Textsw abstract, Textsw_index first,
								Textsw_index last_plus_one)
{
    Textsw_view_private view = VIEW_ABS_TO_REP(abstract);
    Textsw_private    priv = TSWPRIV_FOR_VIEWPRIV(view);
    int             result;

    textsw_take_down_caret(priv);
    result = textsw_delete_span(view, first, last_plus_one,
				TXTSW_DS_ADJUST | TXTSW_DS_SHELVE);
    if (result == ES_CANNOT_SET)
	return 0;
    return -result;
}

Xv_public Textsw_index textsw_erase(Textsw abstract, Textsw_index first,
							Textsw_index last_plus_one)
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

	textsw_take_down_caret(priv);
	result = textsw_delete_span(view, first, last_plus_one, TXTSW_DS_ADJUST);
	if (result == ES_CANNOT_SET)
		return 0;
	return -result;
}

Pkg_private int textsw_do_edit(Textsw_view_private view, unsigned unit,
								unsigned direction)
{
	register Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
	struct ei_span_result span;
	int delta;

	span = ev_span_for_edit(priv->views, (int)(unit | direction));
	if ((span.flags >> 16) == 0) {

		/* Don't join with next line for ERASE_LINE_END */
		if ((unit == EV_EDIT_LINE) && (direction == 0)) {
			Es_index file_length = es_get_length(priv->views->esh);

			if (span.last_plus_one < file_length)
				span.last_plus_one--;
		}
		delta = textsw_delete_span(view, span.first, span.last_plus_one,
											TXTSW_DS_ADJUST);

		if (delta == ES_CANNOT_SET) {
			delta = 0;
		}
		else {
			TEXTSW_DO_INSERT_MAKES_VISIBLE(view);
			textsw_record_edit(priv, unit, direction);
			delta = -delta;
		}
	}
	else
		delta = 0;
	return (delta);
}

Xv_public Textsw_index textsw_edit(Textsw abstract, unsigned unit,
						unsigned count, unsigned direction)
{
	Textsw_view_private view = VIEW_ABS_TO_REP(abstract);
	Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
	int result = 0;

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
		result += textsw_do_edit(view, unit, direction);
	}
	return (result);
}

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
		if (memchr(buf, xv_shell_prompt[0], (size_t)buf_len)) {
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

Xv_public Textsw_index textsw_insert(Textsw abstract, CHAR *buf, int buf_len)
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

    textsw_take_down_caret(priv);
    result = textsw_do_input(view, buf, (long)buf_len,
			     TXTSW_UPDATE_SCROLLBAR_IF_NEEDED);    
    return result;
}

Xv_public Textsw_index textsw_replace_bytes(Textsw abstract, Textsw_index first,
					Textsw_index last_plus_one, CHAR *buf, long int buf_len)
/*
 * This routine is a placeholder that can be documented without casting the
 * calling sequence to textsw_replace (the preferred name) in concrete.
 */
{
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
