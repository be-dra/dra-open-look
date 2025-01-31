#ifndef lint
char     txt_sel_c_sccsid[] = "@(#)txt_sel.c 20.55 93/06/28 DRA: $Id: txt_sel.c,v 4.24 2025/01/08 21:21:34 dra Exp $";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * User interface to selection within text subwindows.
 */

#include <xview/pkg.h>
#include <xview/attrol.h>
#include <xview_private/primal.h>
#include <xview_private/txt_impl.h>
#include <xview_private/svr_impl.h>
#include <xview_private/ev_impl.h>
#include <xview_private/txt_18impl.h>
#ifdef SVR4 
#include <stdlib.h> 
#endif /* SVR4 */
#include <assert.h> 

#ifdef OW_I18N
Xv_public   void	textsw_set_selection_wcs();
#endif

extern char *xv_app_name;

Xv_public void
#ifdef OW_I18N
textsw_normalize_view_wc(abstract, pos)
    Textsw          abstract;
    Textsw_index        pos;
#else
textsw_normalize_view(Textsw_view v, Textsw_index pos)
#endif
{
    int             upper_context;
    Textsw_view_private view;

	/* textsw_view or termsw_view */
	if (xv_get(v, XV_IS_SUBTYPE_OF, OPENWIN_VIEW)) {
		view = VIEW_PRIVATE(v);
	}
	else {
    	view = VIEW_ABS_TO_REP(v);
	}

    upper_context = (int)ev_get(view->e_view, EV_CHAIN_UPPER_CONTEXT,
										XV_NULL, XV_NULL, XV_NULL);
    textsw_normalize_internal(view, pos, pos,
			      upper_context, 0, TXTSW_NI_DEFAULT);
}

#ifdef OW_I18N
Xv_public void
textsw_normalize_view(abstract, pos)
    Textsw          abstract;
    Es_index        pos;
{
    Textsw_view_private view = VIEW_ABS_TO_REP(abstract);
    int             upper_context;

    upper_context = (int) ev_get(view->e_view, EV_CHAIN_UPPER_CONTEXT, XV_NULL, XV_NULL, XV_NULL);
    pos = textsw_wcpos_from_mbpos(TSWPRIV_FOR_VIEWPRIV(view), pos);
    textsw_normalize_internal(view, pos, pos,
			      upper_context, 0, TXTSW_NI_DEFAULT);
}
#endif /* OW_I18N */

static void textsw_not_visible_normalize(Textsw_view v, Es_index pos)
{
    int             upper_context;
    Textsw_view_private view;

	if (xv_get(v, XV_IS_SUBTYPE_OF, TEXTSW_VIEW)) {
		view = VIEW_PRIVATE(v);
	}
	else {
    	view = VIEW_ABS_TO_REP(v);
	}

    upper_context = (int) ev_get(view->e_view, EV_CHAIN_UPPER_CONTEXT,
										XV_NULL, XV_NULL, XV_NULL);
    textsw_normalize_internal(view, pos, pos, upper_context, 0,
			      TXTSW_NI_NOT_IF_IN_VIEW | TXTSW_NI_MARK);
}

/*
 *	the following two routines are identical names for the same thing
 */
Xv_public	void
#ifdef OW_I18N
textsw_possibly_normalize_wc(abstract, pos)
    Textsw          abstract;
    Es_index        pos;
#else
textsw_possibly_normalize(Textsw_view v, Textsw_index pos)
#endif
{
    textsw_not_visible_normalize(v, pos);
}

#ifdef OW_I18N
Xv_public	void
textsw_possibly_normalize(abstract, pos)
    Textsw          abstract;
    Es_index        pos;
{
    Textsw_view_private view = VIEW_ABS_TO_REP(abstract);

    textsw_not_visible_normalize(abstract,
			textsw_wcpos_from_mbpos(TSWPRIV_FOR_VIEWPRIV(view), pos));
}
#endif /* OW_I18N */

Pkg_private	void textsw_possibly_normalize_and_set_selection(Textsw abstract, Es_index first, Es_index last_plus_one, unsigned type)
{
    int             upper_context;
    Textsw_view_private view;

	if (xv_get(abstract, XV_IS_SUBTYPE_OF, TEXTSW_VIEW)) {
		view = VIEW_PRIVATE(abstract);
	}
	else {
    	view = VIEW_ABS_TO_REP(abstract);
	}

#ifdef OW_I18N
	textsw_implicit_commit(TSWPRIV_FOR_VIEWPRIV(view));
#endif
    upper_context = (int)
	ev_get(view->e_view, EV_CHAIN_UPPER_CONTEXT, XV_NULL, XV_NULL, XV_NULL);

    textsw_normalize_internal(view, first, last_plus_one, upper_context, 0,
	   TXTSW_NI_NOT_IF_IN_VIEW | TXTSW_NI_MARK | TXTSW_NI_SELECT(type));
}

Pkg_private void textsw_normalize_internal(Textsw_view_private view, Es_index first, Es_index last_plus_one, int upper_context, int lower_context, unsigned flags)
{
	Rect rect;
	Es_index line_start, start;
	int lines_above, lt_index, lines_below;
	int normalize = TRUE;
	CHAR newline_str[2];

	newline_str[0] = '\n';
	newline_str[1] = '\0';

	if (flags & TXTSW_NI_NOT_IF_IN_VIEW) {
		switch (ev_xy_in_view(view->e_view, first, &lt_index, &rect)) {
			case EV_XY_VISIBLE:
				normalize = FALSE;
				break;
			case EV_XY_RIGHT:
				normalize = FALSE;
				break;
			default:
				break;
		}
	}
	if (normalize) {
		line_start = ev_line_start(view->e_view, first);
		lines_below = textsw_screen_line_count(VIEW_PUBLIC(view));
		if (flags & TXTSW_NI_AT_BOTTOM) {
			lines_above = (lines_below > lower_context) ?
					(lines_below - (lower_context + 1)) : (lines_below - 1);
		}
		else
			lines_above = (upper_context < lines_below) ? upper_context : 0;
		if (lines_above > 0) {
			ev_find_in_esh(view->textsw_priv->views->esh, newline_str, 1,
					line_start, (unsigned)lines_above + 1, EV_FIND_BACKWARD,
					&start, &line_start);
			if (start == ES_CANNOT_SET)
				line_start = 0;
		}
		/* Ensure no caret turds will leave behind */
		textsw_take_down_caret(TSWPRIV_FOR_VIEWPRIV(view));

		ev_set_start(view->e_view, line_start);

		/* ensure line_start really is visible now */
		lines_below -= (lines_above + 1);
		ev_make_visible(view->e_view, first, lines_below, 0, 0);

		if (!(flags & TXTSW_NI_DONT_UPDATE_SCROLLBAR))
			textsw_update_scrollbars(TSWPRIV_FOR_VIEWPRIV(view), view);
	}
	/*
	 * force textsw find to do pending delete selection
	 */
	if (flags & EV_SEL_PD_PRIMARY)

#ifdef OW_I18N
		textsw_set_selection_wcs(VIEW_PUBLIC(view), first, last_plus_one,
#else
		textsw_set_selection(xv_get(VIEW_PUBLIC(view), XV_OWNER),
						first, last_plus_one,
#endif

				(EV_SEL_BASE_TYPE(flags) | EV_SEL_PD_PRIMARY));
	else if (EV_SEL_BASE_TYPE(flags)) {

#ifdef OW_I18N
		textsw_set_selection_wcs(VIEW_PUBLIC(view), first, last_plus_one,
#else
		textsw_set_selection(xv_get(VIEW_PUBLIC(view), XV_OWNER),
									first, last_plus_one,
#endif

				EV_SEL_BASE_TYPE(flags));
	}
}

Pkg_private     Es_index textsw_set_insert( register Textsw_private priv, register Es_index pos)
{
    register Es_index set_to;
    Es_index        boundary;

    if (TXTSW_IS_READ_ONLY(priv)) {
	return (EV_GET_INSERT(priv->views));
    }
    if (TXTSW_HAS_READ_ONLY_BOUNDARY(priv)) {
	boundary = textsw_find_mark_internal(priv,
					     priv->read_only_boundary);
	if (pos < boundary) {
	    if AN_ERROR
		(boundary == ES_INFINITY) {
	    } else
		return (EV_GET_INSERT(priv->views));
	}
    }
    /* Ensure timer is set to fix caret display */
    textsw_take_down_caret(priv);
    EV_SET_INSERT(priv->views, pos, set_to);
    ASSUME((pos == ES_INFINITY) || (pos == set_to));
    return (set_to);
}

Pkg_private caddr_t textsw_checkpoint_undo(Textsw abstract, caddr_t undo_mark)
{
    Textsw_private priv;
    caddr_t         current_mark;

	/* hier kann TEXTSW oder TERMSW kommen */
	assert(xv_get(abstract, XV_IS_SUBTYPE_OF, OPENWIN));
	priv = TEXTSW_PRIVATE(abstract);

    /* AGAIN/UNDO support */
    if (((long)undo_mark) < TEXTSW_INFINITY - 1) {
		current_mark = undo_mark;
    }
	else {
		current_mark = es_get(priv->views->esh, ES_UNDO_MARK);
    }
    if (TXTSW_DO_UNDO(priv) && (((long)undo_mark) != TEXTSW_INFINITY)) {
		if (current_mark != priv->undo[0]) {
	    	/* Make room for, and then record the current mark. */
	    	XV_BCOPY((char *) (&priv->undo[0]), (char *) (&priv->undo[1]),
		  			(int) (priv->undo_count - 1) * sizeof(priv->undo[0]));
	    	priv->undo[0] = current_mark;
		}
    }
    return (current_mark);
}

Pkg_private	void textsw_checkpoint_again(Textsw abstract)
{
    register Textsw_private priv;

	assert(xv_get(abstract, XV_IS_SUBTYPE_OF, OPENWIN));
	priv = TEXTSW_PRIVATE(abstract);

    /* AGAIN/UNDO support */
    if (!TXTSW_DO_AGAIN(priv))
	return;
    if (priv->func_state & TXTSW_FUNC_AGAIN)
	return;
    priv->again_first = ES_INFINITY;
    priv->again_last_plus_one = ES_INFINITY;
    priv->again_insert_length = 0;
    if (TXTSW_STRING_IS_NULL(&priv->again[0]))
	return;
    if (priv->again_count > 1) {
	/* Make room for this action sequence. */
	textsw_free_again(priv,
			  &priv->again[priv->again_count - 1]);
	XV_BCOPY((char *) (&priv->again[0]), (char *) (&priv->again[1]),
	      (int) (priv->again_count - 1) * sizeof(priv->again[0]));
    }
    priv->again[0] = null_string;
    priv->state &= ~(TXTSW_AGAIN_HAS_FIND | TXTSW_AGAIN_HAS_MATCH);
}

static void extend_selbuffer(Textsw_private priv, int reqsize)
{
	if (reqsize > priv->bufsize) {
		while (reqsize > priv->bufsize) priv->bufsize += 500;
		if (priv->selbuffer) xv_free(priv->selbuffer);
		priv->selbuffer = xv_malloc(priv->bufsize);
	}
}

Pkg_private int textsw_get_local_selected_text(Textsw_private priv,
					unsigned typ, Es_index *first, Es_index *last_plus_one,
					char **selstr, int *str_len)
{
	int to_read, buf_max_len, buf_len;

	ev_get_selection(priv->views, first, last_plus_one, typ);
	if (*first >= *last_plus_one) return FALSE;
	to_read = *last_plus_one - *first;
	buf_max_len = to_read + 1;
	extend_selbuffer(priv, buf_max_len);
	buf_len = textsw_es_read((typ & EV_SEL_CLIPBOARD)
									? priv->trash
									: priv->views->esh,
						priv->selbuffer, *first, *last_plus_one);
	priv->selbuffer[buf_len] = '\0';

	*selstr = priv->selbuffer;
	*str_len = buf_len;
	return TRUE;
}

static void set_local_selected_text(Textsw_private priv, int indx, unsigned typ)
{
	Es_index first, last_plus_one;
	int	buf_len;
	char *mybuf;
	struct timeval tv;

	server_set_timestamp(XV_SERVER_FROM_WINDOW(TEXTSW_PUBLIC(priv)), &tv, 0L);

	SERVERTRACE((688, "%s for %ld\n", __FUNCTION__,
					xv_get(priv->sel_owner[indx], SEL_RANK)));
	xv_set(priv->sel_owner[indx],
				SEL_TIME, &tv,
				SEL_OWN, TRUE,
				NULL);

	if (! textsw_get_local_selected_text(priv, typ, &first, &last_plus_one,
					&mybuf, &buf_len))
	{
		xv_set(priv->sel_item[indx],
				SEL_DATA, XV_NULL,
				SEL_LENGTH, 0,
				NULL);
		return;
	}

	xv_set(priv->sel_item[indx],
			SEL_DATA, mybuf,
			SEL_LENGTH, buf_len,
			NULL);
}

Xv_public	void
#ifdef OW_I18N
textsw_set_selection_wcs(abstract, first, last_plus_one, type)
    Textsw          abstract;
    Es_index        first, last_plus_one;
    unsigned        type;
#else
/* abstract kann ein View sein oder auch das Textsw   !!! */
/* aber den View-Quatsch will ich nicht */
textsw_set_selection(Textsw abstract, Textsw_index first,
					Textsw_index last_plus_one, unsigned type)
#endif
{
	Es_index max_len;
	register Textsw_private priv;

	/* hier kann TEXTSW oder TERMSW kommen */
	assert(xv_get(abstract, XV_IS_SUBTYPE_OF, OPENWIN));
	priv = TEXTSW_PRIVATE(abstract);

	textsw_take_down_caret(priv);
	type &= EV_SEL_MASK;
	if ((first == ES_INFINITY) && (last_plus_one == ES_INFINITY)) {
		ev_clear_selection(priv->views, type);
		return;
	}
	/* make sure selection falls within valid text for WRAPAROUND case
	   by trying to read at the first or last_plus_one position */
	max_len = (Es_index)((long)es_get(priv->views->esh, ES_PS_SCRATCH_MAX_LEN));
	if (max_len != ES_INFINITY) {
		int result;
		CHAR buf[1];

		if (first < last_plus_one) {
			es_set_position(priv->views->esh, first);
		}
		else {
			es_set_position(priv->views->esh, last_plus_one);
		}
		es_read(priv->views->esh, 1, buf, &result);
		if (result == 0) {
			ev_clear_selection(priv->views, type);
			return;
		}
	}
	ev_set_selection(priv->views, first, last_plus_one, type);

	if (type & EV_SEL_CARET) {
		struct timeval tv;
	
		server_set_timestamp(XV_SERVER_FROM_WINDOW(abstract), &tv, 0L);

		/* 08.02.2025: do we ever see this ? */
		fprintf(stderr, "%s: %s`%s-%d: CARET selection is used!!!\n",
			xv_app_name, __FILE__, __FUNCTION__, __LINE__);
		xv_set(priv->sel_owner[TSW_SEL_CARET],
				SEL_TIME, &tv,
				SEL_OWN, TRUE,
				NULL);
		return;
	}
	if (type & EV_SEL_PRIMARY) {
		set_local_selected_text(priv, TSW_SEL_PRIMARY, EV_SEL_PRIMARY);
	}
	else if (type & EV_SEL_SECONDARY) {
		set_local_selected_text(priv, TSW_SEL_SECONDARY, EV_SEL_SECONDARY);
	}
	else if (type & EV_SEL_CLIPBOARD) {
		set_local_selected_text(priv, TSW_SEL_CLIPBOARD, EV_SEL_PRIMARY);
	}
	else return;

	if (type & EV_SEL_PRIMARY) {
		textsw_checkpoint_undo(abstract, (caddr_t) TEXTSW_INFINITY - 1);
	}
}

/* It is okay for the caller to pass EV_NULL for e_view. */
Pkg_private Textsw_view_private textsw_view_for_entity_view(Textsw_private priv, Ev_handle e_view)
{
    register Textsw_view_private textsw_view;

	Xv_opaque textsw = TEXTSW_PUBLIC(priv);
	Textsw_view vp;
	OPENWIN_EACH_VIEW(textsw, vp)
		textsw_view = VIEW_PRIVATE(vp);
		if (textsw_view->e_view == e_view) return (textsw_view);
	OPENWIN_END_EACH
    return ((Textsw_view_private) 0);
}

static unsigned my_determine_selection_type(Textsw_private priv, Event *ev)
{
	if (priv->current_sel == TSW_SEL_SECONDARY) {
		if (event_is_quick_move(ev))
			return (EV_SEL_SECONDARY | EV_SEL_PD_SECONDARY);
		return EV_SEL_SECONDARY;
	}
	return EV_SEL_PRIMARY;
}

/*
 *	jcb	1/2/91
 *
 *	this is new code that determines if the user action is one of the
 *	mouseless keyboard select commands. if this is the case the action
 *	specified is done and the action is consumed upon return.
 *
 *	there are two other routines that handle the mouseless commands
 *	(other than pane navigation). these are in txt_event.c and txt_scroll.c
 */
Pkg_private	int textsw_mouseless_select_event(Textsw_view_private view, Event *ie, Notify_arg arg)
{
	Textsw_private priv	= TSWPRIV_FOR_VIEWPRIV(view);
	char		*msg		= NULL;
	int             action		= event_action(ie);
	int             is_select_event	= TRUE;
	Ev_chain 	chain 		= priv->views;
	short		do_dir		= FALSE;
	Textsw_Caret_Direction	dir	= (Textsw_Caret_Direction)0;
	int		rep_cnt		= 1;
	int		num_lines	= view->e_view->line_table.last_plus_one;
	Es_index	old_position, new_position;
	unsigned 	sel_type;
	Es_index        first, last_plus_one;

	if (event_is_up(ie)) return FALSE;

	switch( action ) {
	case ACTION_SELECT_DATA_END:
		msg	= "ACTION_SELECT_DATA_END";
		dir	= TXTSW_DOCUMENT_END;
		break;
	case ACTION_SELECT_DATA_START:
		msg	= "ACTION_SELECT_DATA_START";
		dir	= TXTSW_DOCUMENT_START;
		break;
	case ACTION_SELECT_DOWN:
		msg	= "ACTION_SELECT_DOWN";
		dir	= TXTSW_LINE_END;
		break;
	case ACTION_SELECT_JUMP_DOWN:
		msg	= "ACTION_SELECT_JUMP_DOWN";
		dir	= TXTSW_LINE_END;
		rep_cnt	= num_lines / 2 - 1;
		break;
	case ACTION_SELECT_JUMP_LEFT:
		msg	= "ACTION_SELECT_JUMP_LEFT";
		dir	= TXTSW_WORD_BACKWARD;
		break;
	case ACTION_SELECT_JUMP_RIGHT:
		msg	= "ACTION_SELECT_JUMP_RIGHT";
		dir	= TXTSW_WORD_FORWARD;
		break;
	case ACTION_SELECT_JUMP_UP:
		msg	= "ACTION_SELECT_JUMP_UP";
		dir	= TXTSW_LINE_START;
		rep_cnt	= num_lines / 2 - 1;
		break;
	case ACTION_SELECT_LEFT:
		msg	= "ACTION_SELECT_LEFT";
		dir	= TXTSW_CHAR_BACKWARD;
		do_dir	= TRUE; /* enum type is 0 */
		break;
	case ACTION_SELECT_LINE_END:
		msg	= "ACTION_SELECT_LINE_END";
		dir	= TXTSW_LINE_END;
		break;
	case ACTION_SELECT_LINE_START:
		msg	= "ACTION_SELECT_LINE_START";
		dir	= TXTSW_LINE_START;
		break;
	case ACTION_SELECT_RIGHT:
		msg	= "ACTION_SELECT_RIGHT";
		dir	= TXTSW_CHAR_FORWARD;
		break;
	case ACTION_SELECT_PANE_DOWN:
		msg	= "ACTION_SELECT_PANE_DOWN";
		dir	= TXTSW_LINE_START;
		rep_cnt	= num_lines - 2;
		break;
	case ACTION_SELECT_PANE_LEFT:
		msg	= "ACTION_SELECT_PANE_LEFT";
		dir	= TXTSW_LINE_START;
		break;
	case ACTION_SELECT_PANE_RIGHT:
		msg	= "ACTION_SELECT_PANE_RIGHT";
		dir	= TXTSW_LINE_END;
		break;
	case ACTION_SELECT_PANE_UP:
		msg	= "ACTION_SELECT_PANE_UP";
		dir	= TXTSW_LINE_START;
		rep_cnt	= num_lines - 2;
		break;
	case ACTION_SELECT_UP:
		msg	= "ACTION_SELECT_UP";
		dir	= TXTSW_LINE_START;
		break;
	default:
		is_select_event	= FALSE;
		break;
	}

	/* anything to do? */
	if( is_select_event ) {

		if (! msg) {
			msg = "will keine Warnung";
		}
/*		printf("mouseless select %-40s %d\n", msg, dir); */

		/* do we know what to do? */
		if( dir != (Textsw_Caret_Direction)0 || do_dir ) {

			old_position = EV_GET_INSERT(chain);

			sel_type = my_determine_selection_type(priv, ie);
			(void) ev_get_selection(priv->views, 
						&first, &last_plus_one,
						sel_type);

			/* do the caret movement */
			do {
				textsw_move_caret(view, dir);
			} while( --rep_cnt > 0 );

			new_position = EV_GET_INSERT(chain);

/*			printf("start: position %d/%d first %d last %d dir %d\n", 
			       old_position, new_position, first, last_plus_one, dir );
*/
			if( new_position == old_position ) return is_select_event;

			/* moving `left' in file */
			if( new_position < old_position ) {

				if( first == old_position )
					first	= new_position;
				else if( last_plus_one == old_position )
					last_plus_one = new_position;
				else {
					first	= new_position;
					last_plus_one = old_position;
				}
			}
			else {

				if( last_plus_one == old_position )
					last_plus_one = new_position;
				else if( first == old_position )
					first	= new_position;
				else {
					first = old_position;
					last_plus_one = new_position;
				}
			}

			if( first > last_plus_one ) {
				old_position = first;
				first = last_plus_one;
				last_plus_one = old_position;
			}

/*			printf("do:    position %d/%d first %d last %d sel_type %d\n", 
			       old_position, new_position, first, last_plus_one, sel_type );
*/
#ifdef OW_I18N
			textsw_set_selection_wcs( VIEW_PUBLIC(view),
#else
			textsw_set_selection(TEXTSW_PUBLIC(priv),
#endif
					     first, last_plus_one, 
					     sel_type | EV_SEL_PD_PRIMARY);

#ifdef OW_I18N
			textsw_possibly_normalize_wc(VIEW_PUBLIC(view),
#else
			textsw_possibly_normalize(VIEW_PUBLIC(view),
#endif
						  new_position );
		}
	}

	return is_select_event;
}

static void update_selection(Textsw_view_private view, Event *ie)
{
	register Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
	register unsigned sel_type;
	register Es_index position;
	Es_index first, last_plus_one, ro_bdry;

	position = ev_resolve_xy(view->e_view, ie->ie_locx, ie->ie_locy);
	if (position == ES_INFINITY)
		return;
	sel_type = my_determine_selection_type(priv, ie);
	if (position == es_get_length(priv->views->esh)) {
		/* Check for and handle case of empty textsw. */
		if (position == 0) {
			last_plus_one = 0;
			goto Do_Update;
		}
		position--;	/* ev_span croaks if passed EOS */
	}
	if (priv->track_state & TXTSW_TRACK_POINT) {
		/* laf */
		if (priv->span_level == EI_SPAN_POINT) {
			last_plus_one = position;
		}
		else if (priv->span_level == EI_SPAN_CHAR) {
			last_plus_one = position;	/* +1; jcb -- no single char sel */
		}
		else {
			ev_span(priv->views, position, &first, &last_plus_one,
					(int)priv->span_level);
			ASSERT(first != ES_CANNOT_SET);
			position = first;
		}
		priv->adjust_pivot = position;	/* laf */
	}
	else if (priv->track_state & TXTSW_TRACK_WIPE) {	/* laf */
		unsigned save_span_level = priv->span_level;

		if (priv->track_state & TXTSW_TRACK_ADJUST_END)
			/* Adjust-from-end is char-adjust */
			priv->span_level = EI_SPAN_CHAR;
		if ((priv->state & TXTSW_ADJUST_IS_PD) && (sel_type & EV_SEL_PRIMARY)) {
			/*
			 * laf if (priv->state & TXTSW_CONTROL_DOWN) sel_type &=
			 * ~EV_SEL_PD_PRIMARY; else
			 */
			sel_type |= EV_SEL_PD_PRIMARY;
		}
		if (position < priv->adjust_pivot) {
			/* There is nothing to the left of position == 0! */
			if (position > 0) {
				/*
				 * Note: span left only finds the left portion of what the
				 * entire span would be. The position between lines is
				 * ambiguous: is it at the end of the previous line or the
				 * beginning of the next? EI_SPAN_WORD treats it as being at
				 * the end of the inter-word space terminated by newline.
				 * EI_SPAN_LINE treats it as being at the start of the
				 * following line.
				 */
				switch (priv->span_level) {
						/* laf */
					case EI_SPAN_POINT:
					case EI_SPAN_CHAR:
						break;
					case EI_SPAN_WORD:
						(void)ev_span(priv->views, position + 1,
								&first, &last_plus_one,
								(int)priv->span_level | EI_SPAN_LEFT_ONLY);
						position = first;
						ASSERT(position != ES_CANNOT_SET);
						break;
					case EI_SPAN_LINE:
						(void)ev_span(priv->views, position,
								&first, &last_plus_one,
								(int)priv->span_level | EI_SPAN_LEFT_ONLY);
						position = first;
						ASSERT(position != ES_CANNOT_SET);
						break;
				}
			}
			last_plus_one = priv->adjust_pivot;
		}
		else {
			/* laf */
			if (priv->span_level == EI_SPAN_POINT) {
				last_plus_one = position + 1;
			}
			else if (priv->span_level == EI_SPAN_CHAR) {
				last_plus_one = position + 1;
			}
			else {
				(void)ev_span(priv->views, position, &first, &last_plus_one,
						(int)priv->span_level | EI_SPAN_RIGHT_ONLY);
				ASSERT(first != ES_CANNOT_SET);
			}
			position = priv->adjust_pivot;
		}
		priv->span_level = save_span_level;
	}
	else {
		unsigned save_span_level = priv->span_level;

		/*
		 * Adjust
		 */
		if (event_action(ie) != LOC_MOVE) {
			(void)ev_get_selection(priv->views, &first, &last_plus_one,
					sel_type);
			ro_bdry = TXTSW_IS_READ_ONLY(priv) ? last_plus_one
					: textsw_read_only_boundary_is_at(priv);
			/* 
			 * If there is no current selection, get the insertion point.
			 * All adjustments for r/w primary selections will be made from 
			 * the insertion point.  All adjustments for ro and secondary
			 * selections will be made from the current point (last
			 * click).  If there is no current point, the insertion 
			 * point will be used.  [7/30/92 sjoe]
			 */
			if (first == last_plus_one) {
				if ((!((sel_type & (EV_SEL_SECONDARY | EV_SEL_PD_SECONDARY))
										|| (last_plus_one <= ro_bdry)))
						|| (first == TEXTSW_INFINITY)) {
					first = EV_GET_INSERT(priv->views);
					last_plus_one = (position < first) ? first : first + 1;
				}
				else {
					if (position < first)
						last_plus_one++;
				}
			}
			if ((position == first) || (position + 1 == last_plus_one)) {
				priv->track_state |= TXTSW_TRACK_ADJUST_END;
			}
			else
				priv->track_state &= ~TXTSW_TRACK_ADJUST_END;
			priv->adjust_pivot =
					(position <
					(last_plus_one + first) / 2 ? last_plus_one : first);
		}
		if (priv->track_state & TXTSW_TRACK_ADJUST_END)
			/* Adjust-from-end is char-adjust */
			priv->span_level = EI_SPAN_CHAR;
		if ((priv->state & TXTSW_ADJUST_IS_PD) && (sel_type & EV_SEL_PRIMARY)) {
			if (priv->state & TXTSW_CONTROL_DOWN)
				sel_type &= ~EV_SEL_PD_PRIMARY;
			else
				sel_type |= EV_SEL_PD_PRIMARY;
		}
		if (position < priv->adjust_pivot) {
			/* There is nothing to the left of position == 0! */
			if (position > 0) {
				/*
				 * Note: span left only finds the left portion of what the
				 * entire span would be. The position between lines is
				 * ambiguous: is it at the end of the previous line or the
				 * beginning of the next? EI_SPAN_WORD treats it as being at
				 * the end of the inter-word space terminated by newline.
				 * EI_SPAN_LINE treats it as being at the start of the
				 * following line.
				 */
				switch (priv->span_level) {
						/* laf */
					case EI_SPAN_POINT:
					case EI_SPAN_CHAR:
						break;
					case EI_SPAN_WORD:
						ev_span(priv->views, position + 1,
								&first, &last_plus_one,
								(int)priv->span_level | EI_SPAN_LEFT_ONLY);
						position = first;
						ASSERT(position != ES_CANNOT_SET);
						break;
					case EI_SPAN_LINE:
						ev_span(priv->views, position,
								&first, &last_plus_one,
								(int)priv->span_level | EI_SPAN_LEFT_ONLY);
						position = first;
						ASSERT(position != ES_CANNOT_SET);
						break;
				}
			}
			last_plus_one = priv->adjust_pivot;
		}
		else {
			if ((priv->span_level == EI_SPAN_CHAR) ||	/* laf */
					(priv->span_level == EI_SPAN_POINT)) {
				last_plus_one = position + 1;
			}
			else {
				ev_span(priv->views, position, &first, &last_plus_one,
						(int)priv->span_level | EI_SPAN_RIGHT_ONLY);
				ASSERT(first != ES_CANNOT_SET);
			}
			position = priv->adjust_pivot;
		}
		priv->span_level = save_span_level;
	}

  Do_Update:
	if (sel_type & (EV_SEL_PD_PRIMARY | EV_SEL_PD_SECONDARY)) {
		ro_bdry = TXTSW_IS_READ_ONLY(priv) ? last_plus_one
				: textsw_read_only_boundary_is_at(priv);
		if (last_plus_one <= ro_bdry) {
			sel_type &= ~(EV_SEL_PD_PRIMARY | EV_SEL_PD_SECONDARY);
		}
	}

#ifdef OW_I18N
	textsw_set_selection_wcs(VIEW_PUBLIC(view),
#else
	textsw_set_selection(TEXTSW_PUBLIC(priv),
#endif
			position, last_plus_one, sel_type);
	if (sel_type & EV_SEL_PRIMARY) {
		textsw_checkpoint_again(TEXTSW_PUBLIC(priv));
	}
	ASSERT(allock());
}

Pkg_private void textsw_start_seln_tracking(Textsw_view_private view, Event *ie)
{
	register Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);

	textsw_flush_caches(view, TFC_STD);
	switch (event_action(ie)) {
		case ACTION_ADJUST:
			priv->track_state |= TXTSW_TRACK_ADJUST;
			priv->last_click_x = ie->ie_locx;
			priv->last_click_y = ie->ie_locy;
			break;
		case ACTION_SELECT:{
				int delta;	/* in millisecs */

				/* laf */
				if (priv->state & TXTSW_SHIFT_DOWN) {
					priv->track_state |= TXTSW_TRACK_ADJUST;
				}
				else
					priv->track_state |= TXTSW_TRACK_POINT;
				delta = (ie->ie_time.tv_sec - priv->last_point.tv_sec) * 1000;
				delta += ie->ie_time.tv_usec / 1000;
				delta -= priv->last_point.tv_usec / 1000;
				if (delta >= priv->multi_click_timeout) {
					/* laf */
					priv->span_level = EI_SPAN_POINT;
				}
				else {
					int delta_x = ie->ie_locx - priv->last_click_x;
					int delta_y = ie->ie_locy - priv->last_click_y;

					if (delta_x < 0) delta_x = -delta_x;
					if (delta_y < 0) delta_y = -delta_y;
					if (delta_x > priv->multi_click_space ||
							delta_y > priv->multi_click_space) {
						/* laf */
						priv->span_level = EI_SPAN_POINT;
					}
					else {
						switch (priv->span_level) {
								/* laf */
							case EI_SPAN_POINT:
								if (!(priv->state & TXTSW_CONTROL_DOWN))
									priv->span_level = EI_SPAN_WORD;
								else
									priv->span_level = EI_SPAN_CHAR;
								break;
							case EI_SPAN_WORD:
								priv->span_level = EI_SPAN_LINE;
								break;
							case EI_SPAN_LINE:
								priv->span_level = EI_SPAN_DOCUMENT;
								break;
							case EI_SPAN_DOCUMENT:
								/* laf */
								priv->span_level = EI_SPAN_POINT;
								break;
								/* laf */
							default:
								priv->span_level = EI_SPAN_POINT;
						}
					}
				}
				priv->last_click_x = ie->ie_locx;
				priv->last_click_y = ie->ie_locy;
				break;
			}
		default:
			LINT_IGNORE(ASSUME(0));
	}
	/*
	 * laf if ((priv->state & TXTSW_CONTROL_DOWN) &&
	 */
	if (!TXTSW_IS_READ_ONLY(priv))
		priv->state |= TXTSW_PENDING_DELETE;
	update_selection(view, ie);
}

Pkg_private Es_index textsw_do_balance_beam(Textsw_view_private view, int x,
								int y, Es_index first, Es_index last_plus_one)
{
	register Ev_handle e_view;
	Es_index new_insert;
	register Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
	register int delta;
	int lt_index;
	struct rect rect;

	if (priv->track_state & TXTSW_TRACK_ADJUST) {
		return ((first == priv->adjust_pivot) ? last_plus_one : first);
	}
	new_insert = last_plus_one;
	e_view = view->e_view;
	/*
	 * When the user has multi-clicked up to select enough text to occupy
	 * multiple physical display lines, linearize the distance to the
	 * endpoints, then do the balance beam.
	 */
	if (ev_xy_in_view(e_view, first, &lt_index, &rect) != EV_XY_VISIBLE)
		return new_insert;

	delta = x - rect.r_left;
	delta += ((y - rect.r_top) / rect.r_height) * e_view->rect.r_width;
	switch (ev_xy_in_view(e_view, last_plus_one, &lt_index, &rect)) {
		case EV_XY_BELOW:
			/*
			 * SPECIAL CASE: if last_plus_one was at the start of the last line
			 * in the line table, it is still EV_XY_BELOW, and we should still
			 * treat it like EV_XY_VISIBLE, with rect.r_left ==
			 * e_view->rect.r_left.
			 */
			if (last_plus_one != ft_position_for_index(e_view->line_table,
							e_view->line_table.last_plus_one - 1)) {
				return first;
			}
			else {
				rect.r_left = e_view->rect.r_left;
			}
			/* FALL THROUGH */
		case EV_XY_VISIBLE:
			if (e_view->rect.r_left == rect.r_left) {
				/* last_plus_one must be a new_line, so back up */
				if (ev_xy_in_view(e_view, last_plus_one - 1, &lt_index, &rect)
						!= EV_XY_VISIBLE) {
					return first;
				}
				if ((view->textsw_priv->span_level == EI_SPAN_POINT) &&
						(view->textsw_priv->track_state != TXTSW_TRACK_WIPE) &&
						(x >= rect.r_left) && (y >= rect.r_top) &&
						(y <= rect_bottom(&rect))) {
					/*
					 * SPECIAL CASE: Selecting in the white-space past the end
					 * of a line puts the insertion point at the end of that
					 * line, rather than the beginning of the next line.
					 * 
					 * SPECIAL SPECIAL CASE: Selecting in the white-space past
					 * the end of a wrapped line (due to wide margins or word
					 * wrapping) puts the insertion point at the beginning
					 * of the next line.
					 */
					if (ev_considered_for_line(e_view, lt_index) >=
							ev_index_for_line(e_view, lt_index + 1))
						new_insert = last_plus_one;
					else
						new_insert = last_plus_one - 1;
					return new_insert;
				}
			}
			break;
		default:
			return first;
	}
	if (y < rect.r_top)
		rect.r_left += (((rect.r_top - y) / rect.r_height) + 1) *
				e_view->rect.r_width;
	if (delta < rect.r_left - x) new_insert = first;

	return new_insert;
}

static void done_tracking(Textsw_view_private view, int x, int y)
{
	register Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);

	if (((priv->track_state & TXTSW_TRACK_SECONDARY) == 0) ||
			(priv->func_state & TXTSW_FUNC_COPY)) {
		Es_index first, last_plus_one, new_insert;

		(void)ev_get_selection(priv->views, &first, &last_plus_one,
				(unsigned)((priv->func_state & TXTSW_FUNC_COPY)
						? EV_SEL_SECONDARY : EV_SEL_PRIMARY));
		/* laf */
		if (priv->track_state & TXTSW_TRACK_POINT &&
				priv->span_level == EI_SPAN_POINT)
		{
			if (last_plus_one != ES_INFINITY) last_plus_one++;
			if (priv->state & TXTSW_CONTROL_DOWN) {
				Es_index position;

				position = ev_resolve_xy(view->e_view, x, y);
				(void)ev_span(priv->views, position,
						&first, &last_plus_one, (int)EI_SPAN_WORD);
			}
		}
		new_insert = textsw_do_balance_beam(view, x, y, first, last_plus_one);
		if (new_insert != ES_INFINITY) {
			first = textsw_set_insert(priv, new_insert);
			ASSUME(first != ES_CANNOT_SET);
		}
	}
	priv->track_state &= ~(TXTSW_TRACK_ADJUST
							| TXTSW_TRACK_ADJUST_END
							| TXTSW_TRACK_POINT
							| TXTSW_TRACK_WIPE);	/* laf */
	/*
	 * Reset pending-delete state (although we may be in the middle of a
	 * multi-click, there is no way to know that and we must reset now in
	 * case user has ADJUST_IS_PENDING_DELETE or had CONTROL_DOWN).
	 */
	if ((priv->func_state & TXTSW_FUNC_CUT) == 0)
		priv->state &= ~TXTSW_PENDING_DELETE;
}

Pkg_private int textsw_track_selection(Textsw_view_private view, Event *ie)
{
	register Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);

	if (event_is_up(ie)) {
		switch (event_action(ie)) {
			case ACTION_SELECT:{
					priv->last_point = ie->ie_time;
					break;
				}
			case ACTION_ADJUST:{
					priv->last_adjust = ie->ie_time;
					break;
				}
			default:
				if ((priv->track_state & TXTSW_TRACK_SECONDARY) ||
						(priv->func_state & TXTSW_FUNC_ALL)) {
					done_tracking(view, ie->ie_locx, ie->ie_locy);
					return (FALSE);	/* Don't ignore: FUNC up */
				}
				else {
					return (TRUE);	/* Ignore: other key up */
				}
		}
		done_tracking(view, ie->ie_locx, ie->ie_locy);
	}
	else {
		switch (event_action(ie)) {
				/*
				 * BUG:  Should interpose on the scrollbar and look for a
				 * WIN_EXIT event
				 */
			case LOC_DRAG:
				if (priv->track_state & TXTSW_TRACK_POINT) {
					/*
					 * Don't get out of point tracking until move more than
					 * a specific amount from the original click.
					 */
					if ((ie->ie_locx > (priv->last_click_x +TXTSW_X_POINT_SLOP))
						|| (ie->ie_locx<(priv->last_click_x-TXTSW_X_POINT_SLOP))
						|| (ie->ie_locy>(priv->last_click_y+TXTSW_Y_POINT_SLOP))
						|| (ie->ie_locy<(priv->last_click_y-TXTSW_Y_POINT_SLOP))
					) {
						priv->track_state &= ~TXTSW_TRACK_POINT;
						priv->track_state |= TXTSW_TRACK_WIPE;
					}
					else
						break;
				}
				if (priv->track_state & TXTSW_TRACK_ADJUST) {
					priv->track_state &= ~TXTSW_TRACK_ADJUST;
					priv->track_state |= TXTSW_TRACK_WIPE;
					priv->track_state |= TXTSW_TRACK_ADJUST_END;
				}
				update_selection(view, ie);
				break;
			case LOC_WINEXIT:
				done_tracking(view, ie->ie_locx, ie->ie_locy);
				textsw_may_win_exit(priv);
				break;
			default:
				/* ignore it */
				;
		}
	}
	return TRUE;
}

/*
 * ===============================================================
 * 
 * Utilities to simplify dealing with selections.
 * 
 * ===============================================================
 */


Pkg_private int textsw_get_selection_as_string(Textsw_private priv,
    unsigned type, CHAR *buf, int buf_max_len)
{
	Es_index first, last_plus_one;
	char *selbuf;
	int buflen;

	if (textsw_get_local_selected_text(priv, type, &first, &last_plus_one,
									&selbuf, &buflen))
	{
		strncpy(buf, selbuf, (size_t)buf_max_len);
		return buflen + 1;
	}

	*buf = '\0';
	return 0;
}

/* Das ist von mir und liefert nur etwas, wenn ICH SELBST
 * der PRIMARY-owner bin
 */
Xv_public int textsw_get_primary_selection(Textsw tsw, char *buf, int maxlen,
					Textsw_index *first, Textsw_index *last_plus_one)
{
	Textsw_private priv = TEXTSW_PRIVATE(tsw);
	Es_index f, lpo;

	if (! xv_get(priv->sel_owner[TSW_SEL_PRIMARY], SEL_OWN)) return 0;
	ev_get_selection(priv->views, &f, &lpo, EV_SEL_PRIMARY);
	if (first) *first = (Textsw_index)f;
	if (last_plus_one) *last_plus_one = (Textsw_index)lpo;

	strncpy(buf, (char *)xv_get(priv->sel_item[TSW_SEL_PRIMARY], SEL_DATA),
							(size_t)(lpo - f));
	return lpo - f;
}
