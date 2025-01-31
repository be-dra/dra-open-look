#ifndef lint
char     txt_caret_c_sccsid[] = "@(#)txt_caret.c 20.28 93/06/28 DRA: $Id: txt_caret.c,v 4.2 2024/12/18 11:26:56 dra Exp $";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Routines for moving textsw caret.
 */


#include <xview_private/primal.h>
#include <xview_private/txt_impl.h>
#include <xview_private/ev_impl.h>

Pkg_private	int ev_get_selection(Ev_chain chain, Es_index *first, Es_index *last_plus_one, unsigned type);

extern char *xv_app_name;
#define XX(_a_) fprintf(stderr,"%s: %s`%s-%d: %s\n", xv_app_name, __FILE__,__FUNCTION__, __LINE__, _a_)
/* #define XX(_a_) */

static void textsw_make_insert_visible(Textsw_view_private view,
		unsigned visible_status, Es_index old_insert, Es_index new_insert)
{
	Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
	register Ev_handle ev_handle = view->e_view;
	register unsigned normalize_flag = (TXTSW_NI_NOT_IF_IN_VIEW |TXTSW_NI_MARK);
	int lower_context, upper_context, scroll_lines;
	int loopcount = 0;
	/*
	 * This procedure will only do display if the new_insert position is not
	 * in view. It will scroll, if the old_insert position is in view, but
	 * not new_insert. It will repaint the entire view, if the old_insert and
	 * new_insert position are not in view.
	 */
	lower_context = (int)ev_get(ev_handle, EV_CHAIN_LOWER_CONTEXT
								, XV_NULL, XV_NULL, XV_NULL);
	upper_context = (int)ev_get(ev_handle, EV_CHAIN_UPPER_CONTEXT
								, XV_NULL, XV_NULL, XV_NULL);
	if (visible_status == EV_XY_VISIBLE) {
		int lines_in_view = textsw_screen_line_count(VIEW_PUBLIC(view));

		if (old_insert > new_insert)	/* scroll backward */
			scroll_lines = ((upper_context>0) && (lines_in_view>=upper_context))
					? -upper_context
					: -1;
		else
			scroll_lines = ((lower_context>0) && (lines_in_view>=lower_context))
					? lower_context
					: 1;

#ifndef FORSCHUNG
		/* das scheint der kritische Fall zu sein: */
		if (old_insert == 1 && new_insert == 0) {
if (getenv("XVIEW_DEBUG")) XX("");
			/* loop-counter an */
			loopcount = 20;
		}
#endif /* FORSCHUNG */
		while (!EV_INSERT_VISIBLE_IN_VIEW(ev_handle)) {
			ev_scroll_lines(ev_handle, scroll_lines, FALSE);
#ifndef FORSCHUNG
			--loopcount;
			if (loopcount == 0) {
				/* das waren jetzt 20 schleifendurchlaeufe.... */
if (getenv("XVIEW_DEBUG")) fprintf(stderr, "%s-%d: scroll_lines = %d, breaking out of loop\n",
					__FILE__, __LINE__, scroll_lines);
				break;
			}
#endif /* FORSCHUNG */
		}
		textsw_update_scrollbars(priv, view);
	}
	else {
		if (old_insert <= new_insert) {
			normalize_flag |= TXTSW_NI_AT_BOTTOM;
			upper_context = 0;
		}
		lower_context = 0;
		textsw_normalize_internal(view, new_insert, new_insert,
				upper_context, lower_context, normalize_flag);
	}
}

static Es_index textsw_move_backward_a_word(Textsw_view_private view, Es_index pos)
{
	register Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
	register Ev_chain chain = priv->views;
	unsigned span_flag = (EI_SPAN_WORD | EI_SPAN_LEFT_ONLY);
	Es_index first, last_plus_one;
	unsigned span_result = EI_SPAN_NOT_IN_CLASS;


	if (pos == 0)	/* This is already at the beginning of the file */
		return (ES_CANNOT_SET);

	while ((pos != 0) && (pos != ES_CANNOT_SET) &&
			(span_result & EI_SPAN_NOT_IN_CLASS)) {
		span_result = ev_span(chain, pos, &first, &last_plus_one, (int)span_flag);
		pos = ((pos == first) ? pos - 1 : first);
	}

	return (pos);
}


static Es_index textsw_move_down_a_line(Textsw_view_private view, Es_index pos, Es_index file_length, int lt_index, struct rect rect)
{
    register Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
    register Ev_handle ev_handle = view->e_view;
    int             line_height = ei_line_height
    (ev_handle->view_chain->eih);
    int             new_x, new_y;
    Es_index        new_pos;
    int             lower_context, scroll_lines;
    Ev_impl_line_seq line_seq = (Ev_impl_line_seq)
    ev_handle->line_table.seq;


    if ((pos >= file_length) ||
	(line_seq[lt_index + 1].pos == ES_INFINITY) ||
	(line_seq[lt_index + 1].pos == file_length))
	return (ES_CANNOT_SET);


    if (pos >= line_seq[ev_handle->line_table.last_plus_one - 2].pos) {
	int             lines_in_view = textsw_screen_line_count(VIEW_PUBLIC(view));

	lower_context = (int) ev_get(ev_handle, EV_CHAIN_LOWER_CONTEXT, XV_NULL, XV_NULL, XV_NULL);
	scroll_lines = ((lower_context > 0) && (lines_in_view > lower_context)) ? lower_context + 1 : 1;
	ev_scroll_lines(ev_handle, scroll_lines, FALSE);
	new_y = rect.r_top - (line_height * (scroll_lines - 1));
	textsw_update_scrollbars(priv, view);
    } else
	new_y = rect.r_top + line_height;

    new_x = textsw_get_recorded_x(view);

    if (new_x < rect.r_left)
	new_x = rect.r_left;

    (void) textsw_record_caret_motion(priv, TXTSW_NEXT_LINE, new_x);

    new_pos = ev_resolve_xy(ev_handle, new_x, new_y);

    return ((new_pos >= 0 && new_pos <= file_length) ?
	    new_pos : ES_CANNOT_SET);
}

static Es_index textsw_move_forward_a_word(Textsw_view_private view, Es_index pos, Es_index file_length)
{
	Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
	Ev_chain chain = priv->views;
	int span_flag = (EI_SPAN_WORD | EI_SPAN_RIGHT_ONLY);
	Es_index first, last_plus_one;
	unsigned span_result = EI_SPAN_NOT_IN_CLASS;

	if (pos >= file_length)
		return (ES_CANNOT_SET);

	/* Get to the end of current word, and use that as start pos */
	(void)ev_span(chain, pos, &first, &last_plus_one, span_flag);

	pos = ((last_plus_one == file_length) ? ES_CANNOT_SET : last_plus_one);

	while ((pos != ES_CANNOT_SET) && (span_result & EI_SPAN_NOT_IN_CLASS)) {
		span_result = ev_span(chain, pos, &first, &last_plus_one, span_flag);

		if (pos == last_plus_one)
			pos = ((pos == file_length) ? ES_CANNOT_SET : pos + 1);
		else
			pos = last_plus_one;
	}


	return ((pos == ES_CANNOT_SET) ? ES_CANNOT_SET : first);
}

static Es_index textsw_move_to_word_end(Textsw_view_private view, Es_index pos, Es_index file_length)
{
    Textsw_private    priv = TSWPRIV_FOR_VIEWPRIV(view);
    register Ev_chain chain = priv->views;
    int span_flag = (EI_SPAN_WORD | EI_SPAN_RIGHT_ONLY);
    Es_index        first, last_plus_one;
    unsigned        span_result = EI_SPAN_NOT_IN_CLASS;

    if (pos >= file_length)
	return (ES_CANNOT_SET);

    while ((pos != ES_CANNOT_SET) && (span_result & EI_SPAN_NOT_IN_CLASS)) {
	span_result = ev_span(chain, pos, &first, &last_plus_one,
			      span_flag);

	if (pos == last_plus_one)
	    pos = ((pos == file_length) ? ES_CANNOT_SET : pos+1);
	else
	    pos = last_plus_one;
    }


    return (pos);
}



static Es_index textsw_move_to_line_start(Textsw_view_private view, Es_index pos)
{
    Textsw_private    priv = TSWPRIV_FOR_VIEWPRIV(view);
    int        span_flag = (EI_SPAN_LINE | EI_SPAN_LEFT_ONLY);
    Es_index        first, last_plus_one;

    if (pos == 0)
	return (ES_CANNOT_SET);

    (void) ev_span(priv->views, pos, &first, &last_plus_one, span_flag);

    return (first);
}



static Es_index textsw_move_to_line_end(Textsw_view_private view, Es_index pos, Es_index file_length)
{
    Textsw_private    priv = TSWPRIV_FOR_VIEWPRIV(view);
    register Ev_chain chain = priv->views;
    int        span_flag = (EI_SPAN_LINE | EI_SPAN_RIGHT_ONLY);
    Es_index        first, last_plus_one;

    if (pos >= file_length)
	return (ES_CANNOT_SET);

    (void) ev_span(chain, pos, &first, &last_plus_one, span_flag);

    if (last_plus_one < file_length)
	return (--last_plus_one);
    else {
	CHAR            buf[1];

#ifdef OW_I18N
	(void) textsw_get_contents_wcs(priv, --last_plus_one, buf, 1);
#else
	(void) textsw_get_contents(priv, --last_plus_one, buf, 1);
#endif
	return ((buf[0] == '\n') ? last_plus_one : file_length);
    }


}


static Es_index textsw_move_next_line_start(Textsw_view_private view, Es_index pos, Es_index file_length)
{
    Textsw_private    priv = TSWPRIV_FOR_VIEWPRIV(view);
    int        span_flag = (EI_SPAN_LINE | EI_SPAN_RIGHT_ONLY);
    Es_index        first, last_plus_one;

    if (pos >= file_length)
	return (ES_CANNOT_SET);

    (void) ev_span(priv->views, pos, &first, &last_plus_one, span_flag);

    return ((last_plus_one == file_length) ? ES_CANNOT_SET : last_plus_one);
}


static Es_index textsw_move_up_a_line(Textsw_view_private view, Es_index pos, Es_index file_length, int lt_index, struct rect rect)
{
	register Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
	register Ev_handle ev_handle = view->e_view;
	int line_height = ei_line_height(ev_handle->view_chain->eih);
	int new_x, new_y;
	Es_index new_pos;
	int upper_context, scroll_lines;
	Ev_impl_line_seq line_seq = (Ev_impl_line_seq)
			ev_handle->line_table.seq;

	if ((pos == 0) || (line_seq[lt_index].pos == 0))
		return (ES_CANNOT_SET);

	if (pos < line_seq[1].pos) {
		int lines_in_view = textsw_screen_line_count(VIEW_PUBLIC(view));

		upper_context =
				(int)ev_get(ev_handle, EV_CHAIN_UPPER_CONTEXT, XV_NULL, XV_NULL,
				XV_NULL);

		scroll_lines = ((upper_context > 0)
				&& (lines_in_view > upper_context)) ? upper_context + 1 : 1;

		ev_scroll_lines(ev_handle, -scroll_lines, FALSE);
		textsw_update_scrollbars(priv, view);
		new_y = rect.r_top + (line_height * (scroll_lines - 1));
	}
	else
		new_y = rect.r_top - line_height;

	new_x = textsw_get_recorded_x(view);

	if (new_x < rect.r_left)
		new_x = rect.r_left;

	(void)textsw_record_caret_motion(priv, TXTSW_PREVIOUS_LINE, new_x);

	new_pos = ev_resolve_xy(ev_handle, new_x, new_y);

	return ((new_pos >= 0 && new_pos <= file_length) ? new_pos : ES_CANNOT_SET);
}

Pkg_private void textsw_move_caret(Textsw_view_private view, Textsw_Caret_Direction direction)
{
	int ok = TRUE;
	Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
	register Ev_chain chain = priv->views;
	Es_index old_pos, pos;
	unsigned visible_status;
	Es_index file_length = es_get_length(chain->esh);
	int lt_index;
	struct rect rect;
	Es_index first, last_plus_one;
	Ev_handle ev_handle = view->e_view;

	if (file_length == 0) {
		(void)window_bell(XV_PUBLIC(view));
		return;
	}
	textsw_flush_caches(view, TFC_STD);
	textsw_checkpoint_undo(TEXTSW_PUBLIC(priv),
			(caddr_t) TEXTSW_INFINITY - 1);
	pos = ES_CANNOT_SET;
	old_pos = EV_GET_INSERT(chain);
	visible_status = ev_xy_in_view(ev_handle, old_pos, &lt_index, &rect);

	switch (direction) {
		case TXTSW_CHAR_BACKWARD:
			pos = ((old_pos == 0) ? ES_CANNOT_SET : (old_pos - 1));
			break;
		case TXTSW_CHAR_FORWARD:
			pos = ((old_pos >= file_length) ? ES_CANNOT_SET : (old_pos + 1));
			break;
		case TXTSW_DOCUMENT_END:

			if ((old_pos < file_length) || (visible_status != EV_XY_VISIBLE)) {
				pos = file_length;
				visible_status = EV_XY_BELOW;
			}
			else
				pos = ES_CANNOT_SET;

			break;
		case TXTSW_DOCUMENT_START:
			if ((old_pos > 0) || (visible_status != EV_XY_VISIBLE)) {
				pos = 0;
				visible_status = EV_XY_ABOVE;
			}
			else
				pos = ES_CANNOT_SET;

			break;
		case TXTSW_LINE_END:
			pos = textsw_move_to_line_end(view, old_pos, file_length);
			break;
		case TXTSW_LINE_START:
			pos = textsw_move_to_line_start(view, old_pos);
			break;
		case TXTSW_NEXT_LINE_START:
			pos = textsw_move_next_line_start(view, old_pos, file_length);
			break;
		case TXTSW_NEXT_LINE:{
				int lower_context =
						(int)ev_get(ev_handle, EV_CHAIN_LOWER_CONTEXT, XV_NULL,
						XV_NULL, XV_NULL);

				if (visible_status != EV_XY_VISIBLE) {
					textsw_normalize_internal(view, old_pos, old_pos,
							0, lower_context + 1,
							(TXTSW_NI_NOT_IF_IN_VIEW | TXTSW_NI_MARK |
									TXTSW_NI_AT_BOTTOM));
					visible_status = ev_xy_in_view(ev_handle, old_pos,
							&lt_index, &rect);
				}
				if (visible_status == EV_XY_VISIBLE)
					pos = textsw_move_down_a_line(view, old_pos, file_length,
							lt_index, rect);
				break;
			}
		case TXTSW_PREVIOUS_LINE:{
				int upper_context =
						(int)ev_get(ev_handle, EV_CHAIN_UPPER_CONTEXT, XV_NULL,
						XV_NULL, XV_NULL);

				if (visible_status != EV_XY_VISIBLE) {
					textsw_normalize_internal(view, old_pos, old_pos,
							upper_context + 1, 0,
							(TXTSW_NI_NOT_IF_IN_VIEW | TXTSW_NI_MARK));
					visible_status = ev_xy_in_view(ev_handle, old_pos,
							&lt_index, &rect);
				}
				if (visible_status == EV_XY_VISIBLE)
					pos = textsw_move_up_a_line(view, old_pos, file_length,
							lt_index, rect);
				break;
			}
		case TXTSW_WORD_BACKWARD:
			pos = textsw_move_backward_a_word(view, old_pos);
			break;
		case TXTSW_WORD_FORWARD:
			pos = textsw_move_forward_a_word(view, old_pos, file_length);
			break;
		case TXTSW_WORD_END:
			pos = textsw_move_to_word_end(view, old_pos, file_length);
			break;
		default:
			ok = FALSE;
			break;
	}

	if (ok) {
		if ((pos == ES_CANNOT_SET) && (visible_status != EV_XY_VISIBLE))
			pos = old_pos;	/* Put insertion point in view anyway */

		if (pos == ES_CANNOT_SET)
			(void)window_bell(XV_PUBLIC(view));
		else {
			textsw_set_insert(priv, pos);
			textsw_make_insert_visible(view, visible_status, old_pos, pos);
			/* Replace pending delete with primary selection, but only
			   if not read only */
			if ((EV_SEL_PD_PRIMARY & ev_get_selection(chain, &first,
									&last_plus_one, EV_SEL_PRIMARY))
					&& !(TXTSW_IS_READ_ONLY(priv)))

#ifdef OW_I18N
				(void)textsw_set_selection_wcs(
#else
				textsw_set_selection(
#endif
						TEXTSW_PUBLIC(priv), first, last_plus_one,
						EV_SEL_PRIMARY);

		}
		textsw_checkpoint_undo(TEXTSW_PUBLIC(priv),
				(caddr_t) TEXTSW_INFINITY - 1);

		if ((direction != TXTSW_NEXT_LINE) &&
				(direction != TXTSW_PREVIOUS_LINE))
			(void)textsw_record_caret_motion(priv, direction, -1);

	}
}
