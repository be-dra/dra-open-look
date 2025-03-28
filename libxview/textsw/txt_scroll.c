#ifndef lint
char     txt_scroll_c_sccsid[] = "@(#)txt_scroll.c 20.41 93/06/28 DRA: $Id: txt_scroll.c,v 4.5 2025/01/28 21:34:38 dra Exp $";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Liason routines between textsw and scrollbar
 */

#include <xview_private/primal.h>
#include <xview_private/txt_impl.h>
#include <xview_private/ev_impl.h>
#include <xview_private/svr_impl.h>
#include <xview_private/draw_impl.h>

Pkg_private void textsw_scroll(Scrollbar sb)
{
    Es_index        first, last_plus_one;
	Textsw_view vp = xv_get(sb, SCROLLBAR_NOTIFY_CLIENT);
    register Textsw_view_private view = VIEW_PRIVATE(vp);
    Es_index        new_start;


    ev_view_range(view->e_view, &first, &last_plus_one);
    new_start = (Es_index) xv_get(sb, SCROLLBAR_VIEW_START);

    if (new_start != first)
#ifdef OW_I18N
	textsw_normalize_view_wc(VIEW_PUBLIC(view),
			      new_start);
#else
	textsw_normalize_view(vp, new_start);
#endif
}


Pkg_private void textsw_compute_scroll(Scrollbar sb, int pos, int length,
			Scroll_motion motion, long *offset, long *obj_length)
{
	Textsw_view vp = xv_get(sb, SCROLLBAR_NOTIFY_CLIENT);
    register Textsw_view_private view = VIEW_PRIVATE(vp);
	Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
	Es_index new_start = TEXTSW_CANNOT_SET;
	int lines = 0;
	Es_index first, last_plus_one;

	*obj_length = es_get_length(priv->views->esh);

	switch (motion) {
		case SCROLLBAR_ABSOLUTE:
			if (length == 0)
				new_start = (Es_index) pos;
			else
				new_start =
						((double)*obj_length * (double)pos) / (double)length;
			break;
		case SCROLLBAR_POINT_TO_MIN:
		case SCROLLBAR_MIN_TO_POINT:{
				/*
				 * Check for expose events (from unmapping scrollbar menu).
				 * Dont worry about sync'ing with server, because menu already did.
				 */
				XEvent xevent;
				Xv_window win;
				Xv_Drawable_info *info;

				win = PIXWIN_FOR_VIEW(view);
				DRAWABLE_INFO_MACRO(win, info);

				if (XCheckWindowEvent(xv_display(info), xv_xid(info),
								ExposureMask, &xevent))
					if (xevent.type == Expose)
						ev_paint_view(view->e_view, win, &xevent);

				lines = ev_line_for_y(view->e_view, view->rect.r_top + pos);
				if (lines == 0)
					lines++;	/* Always make some progress */
				if (motion == SCROLLBAR_MIN_TO_POINT)
					lines = -lines;
			}
			break;
		case SCROLLBAR_PAGE_FORWARD:
			lines = view->e_view->line_table.last_plus_one - 2;
			break;
		case SCROLLBAR_PAGE_BACKWARD:
			lines = -view->e_view->line_table.last_plus_one + 2;
			break;
		case SCROLLBAR_LINE_FORWARD:
			lines = 1;
			break;
		case SCROLLBAR_LINE_BACKWARD:
			lines = -1;
			break;
		case SCROLLBAR_TO_START:
			new_start = 0;
			break;
		case SCROLLBAR_TO_END:
			new_start = *obj_length;
			break;
		default:
			break;
	}

	if (new_start != TEXTSW_CANNOT_SET) {
		/*
		 * Motion must be SCROLLBAR_ABSOLUTE, SCROLLBAR_TO_START,
		 * SCROLLBAR_TO_END
		 */
		int upper_context = (int)ev_get(view->e_view,
				EV_CHAIN_UPPER_CONTEXT, XV_NULL, XV_NULL, XV_NULL);

		textsw_normalize_internal(view, new_start, new_start,
				upper_context, 0, TXTSW_NI_DONT_UPDATE_SCROLLBAR);
	}
	else {
		textsw_take_down_caret(priv);	/* Prevent caret turds */
		ev_scroll_lines(view->e_view, lines, FALSE);
	}

	/* if this is TO_END, change so there is half a screen visible if possible */
	if (motion == SCROLLBAR_TO_END) {
		lines = -view->e_view->line_table.last_plus_one / 2 + 1;
		textsw_take_down_caret(priv);	/* Prevent caret turds */
		ev_scroll_lines(view->e_view, lines, FALSE);
	}

	ev_view_range(view->e_view, &first, &last_plus_one);

	xv_set(sb, SCROLLBAR_VIEW_LENGTH, last_plus_one - first, NULL);
	*offset = first;
}

/*
 *	jcb	12/18/90
 *
 *	this is new code that determines if the user action is one of the
 *	mouseless keyboard scroll commands. if this is the case the action
 *	specified is done and the action is consumed upon return.
 *
 *	there are two other routines that handle the mouseless commands
 *	(other than pane navigation). these are in txt_sel.c and txt_event.c
 */
Pkg_private	int textsw_mouseless_scroll_event(Textsw_view_private view, Event *ie, Notify_arg arg)
{
	Textsw_private    priv		= TSWPRIV_FOR_VIEWPRIV(view);
	char		*msg		= NULL;
	int             action		= event_action(ie);
	int             is_scroll_event	= TRUE;
	Es_index        first, last_plus_one;
	Textsw_Caret_Direction	dir	= (Textsw_Caret_Direction)0;
	short		do_dir		= FALSE;
	int             lines 		= 0;
	int             vlen, olen;

	if (event_is_up(ie)) return FALSE;

	/* get current last plus one */
	last_plus_one	= view->e_view->line_table.last_plus_one;

	switch( action ) {
	case ACTION_SCROLL_DATA_END:
		msg	= "ACTION_SCROLL_DATA_END";
		dir	= TXTSW_DOCUMENT_END;
		break;
	case ACTION_SCROLL_DATA_START:
		msg	= "ACTION_SCROLL_DATA_START";
		dir	= TXTSW_DOCUMENT_START;
		break;
	case ACTION_SCROLL_DOWN:
		msg	= "ACTION_SCROLL_DOWN";
		lines 	= 1;
		break;
	case ACTION_SCROLL_JUMP_DOWN:
		msg	= "ACTION_SCROLL_JUMP_DOWN";
		lines 	= (last_plus_one / 2 + 1);
		break;
	case ACTION_SCROLL_JUMP_LEFT:
		msg	= "ACTION_SCROLL_JUMP_LEFT";
		dir	= TXTSW_WORD_BACKWARD;
		break;
	case ACTION_SCROLL_JUMP_RIGHT:
		msg	= "ACTION_SCROLL_JUMP_RIGHT";
		dir	= TXTSW_WORD_FORWARD;
		break;
	case ACTION_SCROLL_JUMP_UP:
		msg	= "ACTION_SCROLL_JUMP_UP";
		lines 	= -(last_plus_one / 2 + 1);
		break;
	case ACTION_SCROLL_LEFT:
		msg	= "ACTION_SCROLL_LEFT";
		dir	= TXTSW_CHAR_BACKWARD;
		do_dir	= TRUE; /* enum type is 0 */
		break;
	case ACTION_SCROLL_LINE_END:
		msg	= "ACTION_SCROLL_LINE_END";
		dir	= TXTSW_LINE_END;
		break;
	case ACTION_SCROLL_LINE_START:
		msg	= "ACTION_SCROLL_LINE_START";
		dir	= TXTSW_LINE_START;
		break;
	case ACTION_SCROLL_RIGHT:
		msg	= "ACTION_SCROLL_RIGHT";
		dir	= TXTSW_CHAR_FORWARD;
		break;
	case ACTION_SCROLL_PANE_DOWN:
		msg	= "ACTION_SCROLL_PANE_DOWN";
		lines 	= (last_plus_one - 2);
		break;
	case ACTION_SCROLL_PANE_LEFT:
		msg	= "ACTION_SCROLL_PANE_LEFT";
		dir	= TXTSW_LINE_START;
		break;
	case ACTION_SCROLL_PANE_RIGHT:
		msg	= "ACTION_SCROLL_PANE_RIGHT";
		dir	= TXTSW_LINE_END;
		break;
	case ACTION_SCROLL_PANE_UP:
		msg	= "ACTION_SCROLL_PANE_UP";
		lines 	= -(last_plus_one - 2);
		break;
	case ACTION_SCROLL_UP:
		msg	= "ACTION_SCROLL_UP";
		lines 	= -1;
		break;
	default:
		is_scroll_event	= FALSE;
		break;
	}

	/* anything to do? */
	if( is_scroll_event ) {

		/* this left in for debuggin', just in case */
		SERVERTRACE((788, "mouseless scroll %-32s  lines %d dir %d\n", msg, lines, (int)dir ));

		if( lines != 0 ) {
			(void) textsw_take_down_caret(priv);	/* Prevent caret turds */
			(void) ev_scroll_lines(view->e_view, lines, FALSE);
		}
		else if( dir != (Textsw_Caret_Direction)0 || do_dir ) {

			textsw_move_caret(view, dir);
		}
			
		/* if this is TO_END, change so there is half a screen visible if possible */
/* unneeded?	if( action == ACTION_SCROLL_DATA_END ) {
			lines 	= last_plus_one - 2;
			(void) textsw_take_down_caret(priv);	
			(void) ev_scroll_lines(view->e_view, lines, FALSE);
		}
*/
		/* set the scrollbar correctly with the new offsets */
		ev_view_range(view->e_view, &first, &last_plus_one);

		vlen = last_plus_one - first;
		olen = es_get_length(priv->views->esh);

		xv_set(textsw_get_scrollbar(view), 
		       SCROLLBAR_VIEW_START, first,
		       SCROLLBAR_VIEW_LENGTH, vlen,
		       SCROLLBAR_OBJECT_LENGTH, olen,
		       NULL);
	}

	return is_scroll_event;
}

/*
 * following routines are called from the interval timer to determine if the
 * caller wanted to update the scrollbars. This is done to minimize the
 * repaint and recalculation of the scrollbar info. In the asynchronous mode,
 * the action is requested as many times as the code wants to do it. When the
 * interval timer triggers and decides to repaint the caret it also calls
 * this routine to determine if it should also update the scrollbars at the
 * same time. There are also times that this is still done using the older
 * synchronous method, but this additional logic greatly reduces the overhead
 * of the scrollbar repaint.
 */
#define	BADVIEW			((Textsw_view_private)-1)

static short    txt_update_bars = FALSE;
static Textsw_view_private txt_only_view = BADVIEW;

Pkg_private void textsw_real_update_scrollbars(Textsw_private priv)
{
	Textsw_view vp;
	Textsw tsw = TEXTSW_PUBLIC(priv);

	if (txt_update_bars == FALSE)
		return;

	OPENWIN_EACH_VIEW(tsw, vp)
		register Textsw_view_private view = VIEW_PRIVATE(vp);
		register Scrollbar sb;
		Es_index first, last_plus_one;
		int vlen, olen;

		if (txt_only_view != BADVIEW &&
				txt_only_view != 0 && (view != txt_only_view))
			continue;

		if ((sb = xv_get(tsw, OPENWIN_VERTICAL_SCROLLBAR, vp)) == XV_NULL) {
			continue;
		}
		ev_view_range(view->e_view, &first, &last_plus_one);
		vlen = last_plus_one - first;
		olen = es_get_length(priv->views->esh);

		/*
		 * printf("real scrollbar update:: first %5d  viewl %5d  objl %5d\n",
		 * first, vlen, olen );
		 */
		xv_set(sb,
				SCROLLBAR_VIEW_START, first,
				SCROLLBAR_VIEW_LENGTH, vlen,
				SCROLLBAR_OBJECT_LENGTH, olen,
				NULL);
	OPENWIN_END_EACH

	txt_update_bars = FALSE;
	txt_only_view = BADVIEW;
}


/*
 * this old routine now just sets flags that indicate that update is needed
 */
Pkg_private void textsw_update_scrollbars(Textsw_private priv, Textsw_view_private only_view)
{
	/* determine the correct global setting for view update */
	if (txt_update_bars) {
		if (only_view && txt_only_view == BADVIEW)
			txt_only_view = only_view;
		else if (only_view != NULL)
			txt_only_view = only_view;
		else
			txt_only_view = 0;
	}
	else {
		txt_only_view = only_view;
		txt_update_bars = TRUE;
	}

	/* if there is no current timer update do actual update now */
	if (priv != NULL) {	/* (can be passed NULL pointer) */
		if (!(priv->caret_state & TXTSW_CARET_TIMER_ON))
			textsw_real_update_scrollbars(priv);
	}
}

Pkg_private Scrollbar textsw_get_scrollbar(Textsw_view_private view)
{
	if (!(view->view_state & TXTSW_SCROLLBAR_DISABLED)) {
		return xv_get(TEXTSW_PUBLIC(TSWPRIV_FOR_VIEWPRIV(view)),
						OPENWIN_VERTICAL_SCROLLBAR, VIEW_PUBLIC(view));
	}

	return XV_NULL;
}
