#ifndef lint
char     txt_event_c_sccsid[] = "@(#)txt_event.c 20.63 93/06/28 DRA: $Id: txt_event.c,v 4.6 2025/03/08 13:15:23 dra Exp $";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * New style, notifier-based, event and timer support by text subwindows.
 */

#include <xview_private/primal.h>
#include <xview_private/draw_impl.h>
#include <xview_private/txt_impl.h>
#include <xview_private/ev_impl.h>
#include <errno.h>
#include <xview/win_notify.h>


extern char *xv_app_name;

static void textsw_start_blinker(Textsw_private priv);
static Notify_value textsw_blink(Notify_client cldt, int which);

#define CARET_WIDTH 7

Pkg_private Textsw_private textsw_folio_for_view(Textsw_view_private view)
{
    ASSUME(view->magic == TEXTSW_VIEW_MAGIC);
    ASSUME(view->textsw_priv->magic == TEXTSW_MAGIC);
    return (view->textsw_priv);
}

/*
 *	there are a couple of crashes that appear to
 *	end up in this routine. therefore a blanket is 
 *	being placed around it to protect minimally 
 *	from bad pointer problems. If anything goes
 *	wrong a NULL pointer is passed back, which
 *	will bubble logic errors back out to the 
 *	place where (we hope) they originate. 6/20/89
 */
Pkg_private Textsw_view_private textsw_view_abs_to_rep(Textsw_view abstract
		, const char *func, int line)
{
	Textsw_view_private view;

	if (abstract == XV_NULL)
		return NULL;

{
/* ich will hier dazu kommen, dass ich nur mit Textsw_view aufgerufen werde */
const Xv_pkg *pkg = (const Xv_pkg *)xv_get(abstract, XV_TYPE);

if (0 != strcmp(pkg->name, "Textsw_view") && 0 != strcmp(pkg->name, "Termsw_view")) {
	fprintf(stderr, "%s: %s-%d: called with %s\n", xv_app_name, func, line, pkg->name);
}
}
	view = VIEW_PRIVATE(abstract);

	if (view == NULL)
		return NULL;

	if (view->magic != TEXTSW_VIEW_MAGIC) {
		Textsw_private priv = TEXTSW_PRIVATE(abstract);
		Textsw tsw;
		Xv_window vp;

		if (priv == NULL)
			return NULL;

		/* that can also be a Termsw !!! */
		tsw = XV_PUBLIC(priv);
		/* then this might be a Termsw_view */
		vp = xv_get(tsw, OPENWIN_NTH_VIEW, 0);

		/* this can be called too early */
		if (! vp) return NULL;

#ifdef NO_CODE
				ist jetzt Termsw_view eine Subklasse von Textsw_view ????
typedef struct {
	/*
	 * This isn't really a textsw view, only shares few attrs 
	 */
        Xv_textsw_view    	parent_data; 
        Xv_opaque  		private_data;
        Xv_opaque		private_text;
	Xv_opaque		private_tty;
} Xv_termsw_view;

Nein! :

const Xv_pkg          xv_termsw_view_pkg = {
    "Termsw_view",
    (Attr_pkg) ATTR_PKG_TERMSW_VIEW,
    sizeof(Xv_termsw_view),
    &xv_window_pkg,               <<====  Das ist die Superklasse
    termsw_view_init,
	...
};
#endif

		view = VIEW_PRIVATE(vp);
	}

	return (view);
}

Pkg_private     Textsw textsw_view_rep_to_abs(Textsw_view_private rep)
{
    ASSUME((rep == 0) || (rep->magic == TEXTSW_VIEW_MAGIC));
    return (VIEW_PUBLIC(rep));
}

Pkg_private Notify_value textsw_view_event_internal(Textsw_view view_public,
	Notify_event ev, Notify_arg arg, Notify_event_type type)
{
	Event *event = (Event *)ev;
	Textsw_view_private view = VIEW_PRIVATE(view_public);
	register Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
	register int process_status;
	Xv_Drawable_info *info;
	Xv_object frame;
	Xv_Window next_view;
	Xv_Window nth_view;
	Xv_Window previous_view = XV_NULL;
	Scrollbar sb;
	Textsw textsw = TEXTSW_PUBLIC(priv);
	int view_nbr;
	static short read_status;

	priv->state |= TXTSW_DOING_EVENT;
	switch (event_action(event)) {
		case ACTION_WHEEL_FORWARD:
			if (event_is_down(event)) {
				event_set_action(event, ACTION_SCROLL_UP);
				textsw_process_event(view_public, event, arg);
			}
			return NOTIFY_DONE;

		case ACTION_WHEEL_BACKWARD:
			if (event_is_down(event)) {
				event_set_action(event, ACTION_SCROLL_DOWN);
				textsw_process_event(view_public, event, arg);
			}
			return NOTIFY_DONE;

		case ACTION_NEXT_PANE:
		case ACTION_NEXT_ELEMENT:
			if (event_is_down(event)) {
				for (view_nbr = 0;
						(nth_view = xv_get(textsw, OPENWIN_NTH_VIEW, view_nbr));
						view_nbr++) {
					if (nth_view == view_public)
						break;
				}
				next_view = xv_get(textsw, OPENWIN_NTH_VIEW, view_nbr + 1);
				if (next_view) {
					/* Set focus to first element in next view window */
					xv_set(next_view, WIN_SET_FOCUS, NULL);
					xv_set(textsw, XV_FOCUS_ELEMENT, 0, NULL);
				}
				else
					xv_set(xv_get(textsw, WIN_FRAME), FRAME_NEXT_PANE, NULL);
			}
			break;

		case ACTION_PREVIOUS_PANE:
		case ACTION_PREVIOUS_ELEMENT:
			if (event_is_down(event)) {
				for (view_nbr = 0;
						(nth_view = xv_get(textsw, OPENWIN_NTH_VIEW, view_nbr));
						view_nbr++) {
					if (nth_view == view_public)
						break;
					previous_view = nth_view;
				}
				if (view_nbr > 0) {
					/* Set focus to last element in previous paint window */
					xv_set(previous_view, WIN_SET_FOCUS, NULL);
					xv_set(textsw, XV_FOCUS_ELEMENT, -1, NULL);
				}
				else {
					xv_set(xv_get(textsw, WIN_FRAME), FRAME_PREVIOUS_PANE, NULL);
				}
			}
			break;

		case ACTION_VERTICAL_SCROLLBAR_MENU:
		case ACTION_HORIZONTAL_SCROLLBAR_MENU:
			if (event_action(event) == ACTION_VERTICAL_SCROLLBAR_MENU)
				sb = xv_get(textsw, OPENWIN_VERTICAL_SCROLLBAR, view_public);
			else
				sb = xv_get(textsw, OPENWIN_HORIZONTAL_SCROLLBAR, view_public);
			if (sb) {
				Event sb_event;

				event_init(&sb_event);
				event_set_action(&sb_event, ACTION_MENU);
				event_set_window(&sb_event, sb);
				sb_event.ie_flags = event->ie_flags;	/* set up/down flag */
				win_post_event(sb, &sb_event, NOTIFY_SAFE);
			}
			break;

		case ACTION_JUMP_MOUSE_TO_INPUT_FOCUS:
			xv_set(view_public, WIN_MOUSE_XY, 0, 0, NULL);
			/* BUG ALERT:  Clicking MENU at this point does not send ACTION_MENU
			 *         to the textsw view window.  Instead, a Window Manager
			 *         Window menu is brought up.
			 */
			break;

		case ACTION_RESCALE:
			/* don't need to do anything since frame will refont us */
			break;
		case LOC_WINENTER:	/* only enabled in Follow-Mouse mode */
			DRAWABLE_INFO_MACRO(view_public, info);
			win_set_kbd_focus(view_public, xv_xid(info));
			break;
		case KBD_USE:
			textsw_hide_caret(priv);	/* To get rid of ghost */

#ifdef OW_I18N
			if (priv->ic && priv->focus_view != view_public) {
				DRAWABLE_INFO_MACRO(view_public, info);
				window_set_ic_focus_win(view_public, priv->ic, xv_xid(info));
			}
#endif

			priv->focus_view = view_public;
			priv->state |= TXTSW_HAS_FOCUS;
			if (priv->caret_state & TXTSW_CARET_FLASHING)
				textsw_start_blinker(priv);
			(void)ev_set(view->e_view, EV_CHAIN_CARET_IS_GHOST, FALSE, NULL);
			if ((frame = xv_get(TEXTSW_PUBLIC(priv), WIN_FRAME)))
				frame_kbd_use(frame, TEXTSW_PUBLIC(priv),
						TEXTSW_PUBLIC(priv));
			goto Default;
		case KBD_DONE:
			textsw_hide_caret(priv);	/* To get rid of solid */
			priv->state &= ~TXTSW_HAS_FOCUS;
			(void)ev_set(view->e_view, EV_CHAIN_CARET_IS_GHOST, TRUE, NULL);
			if ((frame = xv_get(TEXTSW_PUBLIC(priv), WIN_FRAME)))
				frame_kbd_done(frame, TEXTSW_PUBLIC(priv));
			textsw_stop_blinker(priv);
			goto Default;
		case WIN_MAP_NOTIFY:
			view->view_state |= TXTSW_VIEW_IS_MAPPED;
			goto Default;
		case WIN_UNMAP_NOTIFY:
			view->view_state &= ~TXTSW_VIEW_IS_MAPPED;
			goto Default;

		case WIN_REPAINT:
		case WIN_GRAPHICS_EXPOSE:
			ev_paint_view(view->e_view, view_public, event_xevent(event));
			goto Return2;

		case WIN_NO_EXPOSE:
			goto Return2;

		case WIN_VISIBILITY_NOTIFY:
			view->obscured = event_xevent(event)->xvisibility.state;
			goto Return2;

		case ACTION_PROPS:
			process_status = textsw_process_event(view_public, event, arg);
			if (event_is_up(event)) {
				Window owner;
				/* do we have a secondary selection owner? */
				owner = XGetSelectionOwner((Display *)xv_get(view_public,
								XV_DISPLAY), XA_SECONDARY);
				if (owner != None) {
					/* yes, we have a SECONDARY owner - do not handle
					 * this as the Props event
					 */
					/* just a convention to tell the caller that
					 * notify_next_event_func is NOT required.
					 */
					return NOTIFY_IGNORED;
				}
			}
			break;

		default:{
			  Default:
				process_status = textsw_process_event(view_public, event, arg);
				if (process_status & TEXTSW_PE_READ_ONLY) {
					if (!read_status) {
						textsw_read_only_msg(priv, event_x(event),
								event_y(event));
						read_status = 1;
					}
				}
				else if (process_status == 0) {
					if (read_status)
						read_status = 0;
					goto Return;
				}
				break;
			}
	}
  Return:
	/* Stablize window if no more typing expected */
	if (!textsw_is_typing_pending(priv, event))
		textsw_stablize(priv, 0);
  Return2:
	priv->state &= ~TXTSW_DOING_EVENT;

	/* just a convention to tell the caller that notify_next_event_func
	 * is required.
	 */
	return NOTIFY_UNEXPECTED;
}

	/* textsw_view_event_internal was invented to enable the 
	 * 'event forwarding' from the pw to the view - as soon as we REALLY
	 * have a paint window...
	 */
Pkg_private Notify_value textsw_view_event(Textsw_view view_public,
					Notify_event ev, Notify_arg arg, Notify_event_type type)
{
	Notify_value result = textsw_view_event_internal(view_public,ev,arg,type);
	if (result == NOTIFY_UNEXPECTED) {
		return notify_next_event_func(view_public, ev, arg, type);
	}

	return result;
}

/* ARGSUSED */
Pkg_private void textsw_stablize(Textsw_private priv, int blink)
{
	/* Flush if pending */
	if ((priv->to_insert_next_free != priv->to_insert) &&
			((priv->func_state & TXTSW_FUNC_FILTER) == 0))
		textsw_flush_caches(VIEW_PRIVATE(xv_get(XV_PUBLIC(priv), OPENWIN_NTH_VIEW, 0)), TFC_STD);
	/* Display caret */
	if (blink)
		textsw_invert_caret(priv);
	else
		textsw_show_caret(priv);
	/* update the scrollbars if needed */
	textsw_real_update_scrollbars(priv);
}

/*
 *	This is new code that determines if the user action is one of the
 *	mouseless keyboard misc commands. if this is the case the action
 *	specified is done and the action is consumed upon return.
 *
 *	there are two other routines that handle the mouseless commands
 *	(other than pane navigation). these are in txt_sel.c and txt_scroll.c
 *
 * Returns: TRUE= event was a mouseless event; event was processed.
 *	    FALSE= event was not a mouseless event; nothing done.
 */
Pkg_private	int textsw_mouseless_misc_event(Textsw_view_private view, Event *event)
{
	int action;
	Ev_chain chain;
	Textsw_Caret_Direction dir;
	Textsw_private priv;
	Es_index new_position;
	int num_lines;
	Es_index old_position;
	int rep_cnt = 0;

	if (event_is_up(event))
		return FALSE;	/* not a mouseless event */

	action = event_action(event);
	dir = (Textsw_Caret_Direction) 0;
	num_lines = view->e_view->line_table.last_plus_one;

	priv = TSWPRIV_FOR_VIEWPRIV(view);
	chain = priv->views;

	switch (action) {
		case ACTION_DELETE_SELECTION:
			break;
		case ACTION_ERASE_LINE:
			break;
		case ACTION_PANE_DOWN:
			dir = TXTSW_NEXT_LINE;
			rep_cnt = num_lines - 2;
			break;
		case ACTION_PANE_UP:
			dir = TXTSW_PREVIOUS_LINE;
			rep_cnt = num_lines - 2;
			break;
		case ACTION_JUMP_DOWN:
			dir = TXTSW_NEXT_LINE;
			rep_cnt = num_lines / 2 - 1;
			break;
		case ACTION_JUMP_UP:
			dir = TXTSW_PREVIOUS_LINE;
			rep_cnt = num_lines / 2 - 1;
			break;
		default:
			return FALSE;	/* not a mouseless event */
	}

	if (dir != (Textsw_Caret_Direction) 0) {

		if (TXTSW_IS_READ_ONLY(priv) || TXTSW_HAS_READ_ONLY_BOUNDARY(priv)) {
			/* Cannot move the caret: just scroll the text */
			Es_index first, last_plus_one;
			Textsw_view vp = VIEW_PUBLIC(view);

			if (dir == TXTSW_PREVIOUS_LINE) rep_cnt = -rep_cnt;

			ev_scroll_lines(view->e_view, rep_cnt, FALSE);
			ev_view_range(view->e_view, &first, &last_plus_one);
			xv_set(xv_get(xv_get(vp, XV_OWNER), OPENWIN_VERTICAL_SCROLLBAR, vp),
					SCROLLBAR_VIEW_START, first,
					SCROLLBAR_VIEW_LENGTH, last_plus_one - first,
					NULL);
		}
		else {

#ifdef OW_I18N
			textsw_implicit_commit(priv);
#endif

			/* Move the caret */
			do {
				old_position = EV_GET_INSERT(chain);
				textsw_move_caret(view, dir);
				new_position = EV_GET_INSERT(chain);
			} while (--rep_cnt > 0 && new_position != old_position);

#ifdef OW_I18N
			textsw_possibly_normalize_wc(VIEW_PUBLIC(view), new_position);
#else
			textsw_possibly_normalize(VIEW_PUBLIC(view), new_position);
#endif
		}
	}

	return TRUE;	/* was a mouseless event */
}


/*
 * When called from outside this module, it is telling the system that it
 * wants the cursor put back up, because the caller had removed it.
 */
/* ARGSUSED */
Pkg_private Notify_value textsw_timer_expired(Textsw_private priv, int which)
{
    textsw_show_caret(priv);
    return NOTIFY_DONE;
}

/* Means really pull the caret down and keep it down */
Pkg_private void textsw_remove_timer(Textsw_private priv)

{
    textsw_stop_blinker(priv);
    textsw_hide_caret(priv);
}

/* Means really pull the caret down, but make sure that it gets up later */
Pkg_private void textsw_take_down_caret(Textsw_private textsw)
{
	textsw_hide_caret(textsw);
	if (!(textsw->state & TXTSW_DOING_EVENT)) {
		/* So that caret can be put back later */
		textsw_start_blinker(textsw);
	}
	/* else exiting textsw_view_event will put caret back */
}

/* ARGSUSED */
static Notify_value textsw_blink(Notify_client cldt, int which)
{
	Textsw_private priv = (Textsw_private)cldt;

	/* If views are zero then we are coming in after destruction of the priv */
	if (!(priv->views))
		return (NOTIFY_DONE);
	textsw_stablize(priv, (priv->caret_state & TXTSW_CARET_FLASHING) ? 1 : 0);
	if (notify_get_itimer_func((Notify_client) priv, ITIMER_REAL) ==
			NOTIFY_TIMER_FUNC_NULL)
		priv->caret_state &= ~TXTSW_CARET_TIMER_ON;
	else
		priv->caret_state |= TXTSW_CARET_TIMER_ON;

	return NOTIFY_DONE;
}

static void textsw_start_blinker(Textsw_private priv)
{
	struct itimerval itimer;

	if ((priv->caret_state & TXTSW_CARET_TIMER_ON) ||
			(TXTSW_IS_READ_ONLY(priv)))
		return;
	if ((priv->caret_state & TXTSW_CARET_FLASHING) &&
			(priv->state & TXTSW_HAS_FOCUS)) {
		/* Set interval timer to be repeating */
		itimer.it_value = priv->timer;
		itimer.it_interval = priv->timer;
	}
	else {
		/* Set interval timer come back ASAP, and not repeat */
		itimer.it_value = NOTIFY_POLLING_ITIMER.it_value;
		itimer.it_interval = NOTIFY_NO_ITIMER.it_interval;
	}
	if (NOTIFY_TIMER_FUNC_NULL == notify_set_itimer_func((Notify_client) priv,
					textsw_blink, ITIMER_REAL, &itimer,
					(struct itimerval *)0)) {
		notify_perror(XV_MSG("textsw adding timer"));
		priv->caret_state &= ~TXTSW_CARET_TIMER_ON;
	}
	else
		priv->caret_state |= TXTSW_CARET_TIMER_ON;
}

Pkg_private void textsw_stop_blinker(Textsw_private priv)
{
	if (!(priv->caret_state & TXTSW_CARET_TIMER_ON))
		return;
	/* Stop interval timer */
	if (NOTIFY_TIMER_FUNC_NULL == notify_set_itimer_func((Notify_client) priv,
					textsw_blink, ITIMER_REAL, &NOTIFY_NO_ITIMER,
					(struct itimerval *)0))
		notify_perror(XV_MSG("textsw removing timer"));
	priv->caret_state &= ~TXTSW_CARET_TIMER_ON;
}

Pkg_private void textsw_show_caret(Textsw_private textsw)
{
#ifdef FULL_R5
#ifdef OW_I18N
    XPoint		loc;
    int			x, y;
    XVaNestedList	va_nested_list;
    XIMStyle		xim_style = 0;
#endif /* OW_I18N */	
#endif /* FULL_R5 */	


    if ((textsw->caret_state & (TXTSW_CARET_ON | TXTSW_CARET_FROZEN)) ||
	TXTSW_IS_READ_ONLY(textsw) ||
	TXTSW_IS_BUSY(textsw))
	return;
    ev_blink_caret(textsw->focus_view, textsw->views, 1);
    textsw->caret_state |= TXTSW_CARET_ON;
#ifdef OW_I18N
#ifdef FULL_R5

    if (textsw->ic && (textsw->xim_style & XIMPreeditPosition) && textsw->focus_view) {
        Textsw_view_private	view = VIEW_PRIVATE(textsw->focus_view);
        if (ev_caret_to_xy(view->e_view, &x, &y)) {
	    loc.x = (short)(x + (CARET_WIDTH/2) + 1);
	    loc.y = (short)y;
	    va_nested_list = XVaCreateNestedList(NULL, 
					     XNSpotLocation, &loc, 
					     NULL);
	    XSetICValues(textsw->ic, XNPreeditAttributes, va_nested_list,
        	     NULL);
	    XFree(va_nested_list);
	}

    }
#endif /* FULL_R5 */	    
#endif /* OW_I18N */	
    
}

Pkg_private void textsw_hide_caret(Textsw_private textsw)
{

    if (!(textsw->caret_state & TXTSW_CARET_ON) ||
	(textsw->caret_state & TXTSW_CARET_FROZEN))
	return;
    ev_blink_caret(textsw->focus_view, textsw->views, 0);
    textsw->caret_state &= ~TXTSW_CARET_ON;
}

Pkg_private void textsw_freeze_caret(Textsw_private textsw)
{
    textsw->caret_state |= TXTSW_CARET_FROZEN;
}

Pkg_private void textsw_thaw_caret(Textsw_private textsw)
{
    textsw->caret_state &= ~TXTSW_CARET_FROZEN;
}

Pkg_private void textsw_invert_caret(Textsw_private textsw)
{
    if (textsw->caret_state & TXTSW_CARET_ON)
	textsw_hide_caret(textsw);
    else
	textsw_show_caret(textsw);
}

Pkg_private int textsw_is_typing_pending(Textsw_private priv, Event *event)
{
	Textsw tsw = XV_PUBLIC(priv);
	Xv_window vp = xv_get(tsw, OPENWIN_NTH_VIEW, 0);
	Scrollbar sb = xv_get(tsw, OPENWIN_VERTICAL_SCROLLBAR, vp);

	/* Probably should be something else, but I know this works */
	Xv_Drawable_info *view_info;
	Display *display;
	XEvent xevent_next, *xevent_cur = event->ie_xevent;
	char c;

	/*
	 * !sb can happen if there is no scrollbar and !xevent_cur
	 * can happen when initially going from ttysw to textsw.  Not worth
	 * looking ahead if nothing to flush.
	 */
	if (!sb || !xevent_cur ||
			(priv->to_insert_next_free == priv->to_insert))
		return 0;
	DRAWABLE_INFO_MACRO(sb, view_info);
	display = xv_display(view_info);
	if (!QLength(display))
		return 0;
	/*
	 * See if next event is a matching KeyRelease to the last event queued
	 * on to_insert.
	 */
	XPeekEvent(display, &xevent_next);
	if ((xevent_next.type == KeyRelease) &&
			(xevent_cur->xkey.x == xevent_next.xkey.x) &&
			(xevent_cur->xkey.y == xevent_next.xkey.y) &&
			(xevent_cur->xkey.window == xevent_next.xkey.window) &&
			(XLookupString((XKeyEvent *) & xevent_next, &c, 1, (KeySym *) 0,
							0) == 1)
			&& (c == (priv->to_insert_next_free - (CHAR *) 1))) {
		/* Take the event off the queue and discard it */
		XNextEvent(display, &xevent_next);
		/* Get new top of queue */
		if (!QLength(display))
			return 0;
		XPeekEvent(display, &xevent_next);
	}
	/* See if next event is typing on main key array */
	if ((xevent_next.type == KeyPress) &&
			(xevent_cur->xkey.x == xevent_next.xkey.x) &&
			(xevent_cur->xkey.y == xevent_next.xkey.y) &&
			(xevent_cur->xkey.window == xevent_next.xkey.window) &&
			(XLookupString((XKeyEvent *) & xevent_next, &c, 1, (KeySym *) 0,
							0) == 1) && (c >= 32 /*" " */ 
					&& c <= 126 /*"~" */ ))
		return 1;
	return 0;
}
