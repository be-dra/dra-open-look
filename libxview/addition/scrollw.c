/*
 * This file is a product of Bernhard Drahota and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify this file without charge, but are not authorized to
 * license or distribute it to anyone else except as part of a product
 * or program developed by the user.
 *
 * THIS FILE IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 *
 * This file is provided with no support and without any obligation on the
 * part of Bernhard Drahota to assist in its use, correction,
 * modification or enhancement.
 *
 * BERNHARD DRAHOTA SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY THIS FILE
 * OR ANY PART THEREOF.
 *
 * In no event will Bernhard Drahota be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even
 * if B. Drahota has been advised of the possibility of such damages.
 */
#include <stdio.h>
#include <xview/xview.h>
#include <xview/scrollbar.h>
#include <xview/cursor.h>
#include <xview/scrollw.h>
#include <xview/help.h>
#include <xview/dragdrop.h>
#include <xview/cms.h>
#include <xview/font.h>
#include <xview/defaults.h>
#include <olgx/olgx.h>
#include <xview/win_notify.h>

char scrollw_c_sccsid[] = "@(#) %M% V%I% %E% %U% $Id: scrollw.c,v 4.6 2025/01/28 19:49:43 dra Exp $";

extern Graphics_info *xv_init_olgx(Xv_window, int *, Xv_font);

#define A0 *attrs
#define A1 attrs[1]
#define A2 attrs[2]
#define A3 attrs[3]
#define A4 attrs[4]

#define ADONE ATTR_CONSUME(*attrs);break

typedef void (*update_proc_t)(Scrollwin, Scrollview);

typedef struct {
	/* internals */
	Xv_opaque                   public_self;
	GC                          copy_gc;
	struct itimerval        	auto_scroll_time;
	Scrollbar                   panhsb, panvsb;
	int                         vdownx, vdowny, vminx, vminy, vmaxx, vmaxy,
								vlastx, vlasty,
								auto_scroll_xdir, auto_scroll_ydir;
	int                         win_v_unit, win_h_unit;

	Xv_opaque                   auto_scroll_pw, sel_req,
								pan_cursor;

	/* attributes */

	int                         scale_percent;
	int                         v_objlen, h_objlen, v_unit, h_unit;
	update_proc_t               update_proc;
	Scrollwin_drop_setting      drop_kind;
	char                        created, panning, restrict_panning,
								auto_scroll_enabled, destroying;
} Scrollwin_private;

typedef struct _scrollview_struct {
	Xv_opaque                   public_self;
	Scrollwin_private           *priv;
	struct _scrollpw_private    *pwp;
} Scrollview_private;

typedef struct _scrollpw_private {
	Xv_opaque                   public_self;
	Xv_drop_site                dropsite;
	int                         sel_offset;
	Scrollpw_info               vi;
	Scrollwin_private           *priv;
	Scrollview_private          *vp;
} Scrollpw_private;

#define SCRPRIV(_x_) XV_PRIVATE(Scrollwin_private, Xv_scrollwin, _x_)
#define SCRPUB(_x_) XV_PUBLIC(_x_)

#define VIEWPRIV(_x_) XV_PRIVATE(Scrollview_private, Xv_scrollview, _x_)
#define VIEWPUB(_x_) XV_PUBLIC(_x_)

#define PWPRIV(_x_) XV_PRIVATE(Scrollpw_private, Xv_scrollpw, _x_)
#define PWPUB(_x_) XV_PUBLIC(_x_)

static void trigger_repaint(Scrollwin_private *priv, Scrollpw_private *pwp, Rect *r, Scrollwin_repaint_reason reason)
{
	Scrollwin_repaint_struct rs;

	rs.vinfo = &pwp->vi;
	rs.reason = reason;
	rs.pw = PWPUB(pwp);
	rs.win_rect = *r;
	rs.virt_rect.r_left = ((rs.win_rect.r_left + rs.vinfo->scr_x) * 100) /
										priv->scale_percent;
	rs.virt_rect.r_top = ((rs.win_rect.r_top + rs.vinfo->scr_y) * 100) /
										priv->scale_percent;
	rs.virt_rect.r_width = (rs.win_rect.r_width * 100) / priv->scale_percent;
	rs.virt_rect.r_height = (rs.win_rect.r_height * 100) / priv->scale_percent;

	xv_set(SCRPUB(priv), SCROLLWIN_REPAINT, &rs, NULL);
}

static void pw_paint(Scrollpw_private *pwp, int clear)
{
	Rect rect;

	rect.r_left = rect.r_top = 0;
	rect.r_width = pwp->vi.width;
	rect.r_height = pwp->vi.height;

	if (clear) XClearWindow(pwp->vi.dpy, pwp->vi.xid);
	trigger_repaint(pwp->priv, pwp, &rect, SCROLLWIN_REASON_EXPOSE);
}

static void update_view_scrollbars(Scrollwin_private *priv, Xv_opaque view, int repaint)
{
	Scrollview_private *vp = VIEWPRIV(view);
	Scrollpw_private *pwp = vp->pwp;
	Scrollbar sb;
	int pl;

	if (priv->update_proc) {
		(priv->update_proc)(SCRPUB(priv), view);
	}
	if ((sb = xv_get(SCRPUB(priv), OPENWIN_VERTICAL_SCROLLBAR, view))) {
		pl = (pwp->vi.height - 1) / priv->win_v_unit;
		if (pl <= 0) pl = 1;

		xv_set(sb,
				SCROLLBAR_PIXELS_PER_UNIT, priv->win_v_unit,
				SCROLLBAR_OBJECT_LENGTH, priv->v_objlen,
				SCROLLBAR_PAGE_LENGTH, pl,
				/* this will disable everything - even the menu */
/* 				SCROLLBAR_INACTIVE, pl > priv->v_objlen, */
				NULL);

		if (pl >= priv->v_objlen) {
			xv_set(sb, SCROLLBAR_VIEW_START, 0, NULL);
		}
		pwp->vi.scr_y = priv->win_v_unit*(int)xv_get(sb, SCROLLBAR_VIEW_START) +
						pwp->sel_offset;
	}

	if ((sb = xv_get(SCRPUB(priv), OPENWIN_HORIZONTAL_SCROLLBAR, view))) {
		pl = (pwp->vi.width - 1) / priv->win_h_unit;
		if (pl <= 0) pl = 1;

/* 		DTRACE(DTL_INTERN, "update_view_scrollbars:objlen=%d,pagelen=%d\n", */
/* 												priv->h_objlen, pl); */

		xv_set(sb,
				SCROLLBAR_PIXELS_PER_UNIT, priv->win_h_unit,
				SCROLLBAR_OBJECT_LENGTH, priv->h_objlen,
				SCROLLBAR_PAGE_LENGTH, pl,
				/* this will disable everything - even the menu */
/* 				SCROLLBAR_INACTIVE, pl > priv->h_objlen, */
				/* strange, this is not necessary for the vertical scrollbar */
				SCROLLBAR_VIEW_LENGTH, pl,
				NULL);

		pwp->vi.scr_x = priv->win_h_unit*(int)xv_get(sb, SCROLLBAR_VIEW_START) +
						pwp->sel_offset;

/* 		DTRACE(DTL_INTERN, "\tvs = %d\n\tvl = %d\n\tol = %d\n\tpl = %d\n", */
/* 					xv_get(sb, SCROLLBAR_VIEW_START), */
/* 					xv_get(sb, SCROLLBAR_VIEW_LENGTH), */
/* 					xv_get(sb, SCROLLBAR_OBJECT_LENGTH), */
/* 					xv_get(sb, SCROLLBAR_PAGE_LENGTH)); */
	}

	if (repaint) pw_paint(pwp, TRUE);
}

static void update_scrollbars(Scrollwin_private *priv, int repaint)
{
	Xv_opaque view;

	OPENWIN_EACH_VIEW(SCRPUB(priv), view)
		update_view_scrollbars(priv, view, repaint);
	OPENWIN_END_EACH
}

static int subclass_handle_event(Scrollwin_private *priv, Scrollpw_private *pwp, Event *ev)
{
	Scrollwin_event_struct es;
	Scrollpw_info vi;

	es.consumed = FALSE;
	/* make a local copy rather than using the original */
	vi = pwp->vi;
	es.vinfo = &vi;
	es.pw = PWPUB(pwp);
	es.event = ev;
	es.virt_x = (((int)event_x(ev) + vi.scr_x) * 100) / priv->scale_percent;
	es.virt_y = (((int)event_y(ev) + vi.scr_y) * 100) / priv->scale_percent;
	es.action = event_action(ev);

	xv_set(SCRPUB(priv), SCROLLWIN_HANDLE_EVENT, &es, NULL);

	return es.consumed;
}

static Notify_value auto_scroll_timer_event(Notify_client xpwp, int unused)
{
	Scrollpw_private *pwp = (Scrollpw_private *)xpwp;
	Scrollwin_private *priv = pwp->priv;
	Scrollpw_info vinfo;
	Scrollwin_auto_scroll_struct ass;
	Scrollview view = VIEWPUB(pwp->vp);
	Attr_attribute attrs[10];
	int cnt = 0;
	Rect *rect;

	vinfo = pwp->vi;
	ass.vinfo = &vinfo;
	ass.mouse_x = ass.mouse_y = 0;
	ass.virt_mouse_x = ass.virt_mouse_y = 0;
	ass.is_start = TRUE;
	ass.paint_window = PWPUB(pwp);
	xv_set(SCRPUB(priv), SCROLLWIN_AUTO_SCROLL, &ass, NULL);

	if (priv->auto_scroll_xdir != 0) {
		attrs[cnt++] = (Attr_attribute)SCROLLVIEW_H_START;
		attrs[cnt++] = (Attr_attribute)(
							(int)xv_get(view, SCROLLVIEW_H_START) +
							priv->auto_scroll_xdir);
	}

	if (priv->auto_scroll_ydir != 0) {
		attrs[cnt++] = (Attr_attribute)SCROLLVIEW_V_START;
		attrs[cnt++] = (Attr_attribute)(
							(int)xv_get(view, SCROLLVIEW_V_START) +
							priv->auto_scroll_ydir);
	}

	if (cnt) {
		attrs[cnt] = (Attr_attribute)0;
		xv_set_avlist(view, attrs);
	}

	rect = (Rect *)xv_get(PWPUB(pwp), WIN_MOUSE_XY);
	vinfo = pwp->vi;
	ass.mouse_x = rect->r_left;
	ass.mouse_y = rect->r_top;
	ass.virt_mouse_x = ((ass.mouse_x + vinfo.scr_x) * 100) /priv->scale_percent;
	ass.virt_mouse_y = ((ass.mouse_y + vinfo.scr_y) * 100) /priv->scale_percent;
	ass.is_start = FALSE;
	xv_set(SCRPUB(priv), SCROLLWIN_AUTO_SCROLL, &ass, NULL);

	return NOTIFY_DONE;
}

static void cancel_auto_scroller(Scrollwin_private *priv)
{
	Scrollpw_private *pwp;

	if (! priv->auto_scroll_pw) return;

	pwp = PWPRIV(priv->auto_scroll_pw);
	notify_set_itimer_func((Notify_client)pwp, NOTIFY_TIMER_FUNC_NULL, ITIMER_REAL,
							(struct itimerval *)0, (struct itimerval *)0);
	priv->auto_scroll_pw = XV_NULL;
}

static void start_auto_scroller(Scrollpw_private *pwp)
{
	notify_set_itimer_func((Notify_client)pwp, auto_scroll_timer_event, ITIMER_REAL,
						&pwp->priv->auto_scroll_time, (struct itimerval *)0);
	pwp->priv->auto_scroll_pw = PWPUB(pwp);
}

static void determine_auto_scroll_direction(Scrollwin_private *priv, Scrollpw_private *pwp, Event *ev)
{
	int x = (int)event_x(ev), y = (int)event_y(ev);

	if (x < 0) priv->auto_scroll_xdir = -1;
	else if (x > pwp->vi.width) priv->auto_scroll_xdir = 1;
	else priv->auto_scroll_xdir = 0;

	if (y < 0) priv->auto_scroll_ydir = -1;
	else if (y > pwp->vi.height) priv->auto_scroll_ydir = 1;
	else priv->auto_scroll_ydir = 0;
}

static void start_panning(Scrollwin_private *priv, Scrollpw_private *pwp, Event *ev)
{
	int pwid, phig, vwid, vhig;
	Xv_window view;

	view = VIEWPUB(pwp->vp);

	pwid = priv->h_objlen * priv->win_h_unit;
	phig = priv->v_objlen * priv->win_v_unit;
	vwid = pwp->vi.width;
	vhig = pwp->vi.height;
	if (pwid < pwp->vi.scr_x+vwid) pwid = pwp->vi.scr_x+vwid;
	if (phig < pwp->vi.scr_y+vhig) phig = pwp->vi.scr_y+vhig;

	priv->vmaxx = priv->vdownx = pwp->vi.scr_x + (int)event_x(ev);
	priv->vmaxy = priv->vdowny = pwp->vi.scr_y + (int)event_y(ev);
	priv->vlastx = (int)event_x(ev);
	priv->vlasty = (int)event_y(ev);

#ifdef OLD_STUFF
	if ((priv->panhsb = xv_get(SCRPUB(priv),OPENWIN_HORIZONTAL_SCROLLBAR,view)))
		priv->vminx = priv->vmaxx - pwid + vwid;
	else priv->vminx = priv->vmaxx;

	if ((priv->panvsb = xv_get(SCRPUB(priv),OPENWIN_VERTICAL_SCROLLBAR,view)))
	{
		priv->vminy = priv->vmaxy - phig + vhig;
		DTRACE(DTL_EV, "start pan: VS=%d\n",
				xv_get(priv->panvsb, SCROLLBAR_VIEW_START));
	}
	else priv->vminy = priv->vmaxy;
#else /* OLD_STUFF */
	priv->panhsb = xv_get(SCRPUB(priv),OPENWIN_HORIZONTAL_SCROLLBAR,view);
	priv->vminx = priv->vmaxx - pwid + vwid;

	priv->panvsb = xv_get(SCRPUB(priv),OPENWIN_VERTICAL_SCROLLBAR,view);
	priv->vminy = priv->vmaxy - phig + vhig;
#endif /* OLD_STUFF */

	/* grab the pointer for this paint window */
	XGrabPointer(pwp->vi.dpy, pwp->vi.xid, False,
			(unsigned)(ButtonMotionMask | ButtonReleaseMask),
			GrabModeAsync, GrabModeAsync,
			priv->restrict_panning ? pwp->vi.xid : None,
			(Cursor)xv_get(priv->pan_cursor, XV_XID),
			event_xevent(ev)->xbutton.time);
	XFlush(pwp->vi.dpy);
	priv->panning = TRUE;
}

static void update_panning(Scrollwin_private *priv, Scrollpw_private *pwp, Event *ev)
{
	XEvent event;
	int x, y, cnt;

	/* get rid of OLD MotionNotify events, only take the last */
	cnt = 0;
	while (XCheckTypedWindowEvent(pwp->vi.dpy,pwp->vi.xid,MotionNotify,&event))
		++cnt;

	if (cnt) {
/* 		DTRACE(DTL_EV + 50, "superfluous MotionNotify\n"); */
		x = event.xmotion.x;
		y = event.xmotion.y;
	}
	else {
		x = (int)event_x(ev);
		y = (int)event_y(ev);
	}

	if (x < priv->vminx) x = priv->vminx;
	if (x > priv->vmaxx) x = priv->vmaxx;
	if (y < priv->vminy) y = priv->vminy;
	if (y > priv->vmaxy) y = priv->vmaxy;

	if (x != priv->vlastx || y != priv->vlasty) {
		int dx, dy, sx, sy, tx, ty;
		Rect r1, r2;

		if ((dx = x - priv->vlastx) < 0) dx = -dx;
		if ((dy = y - priv->vlasty) < 0) dy = -dy;

		r1.r_left   = 0;
		r1.r_width  = pwp->vi.width;
		r1.r_height = dy;

		r2.r_width  = dx;
		r2.r_height = pwp->vi.height - dy;

		if (x > priv->vlastx) {
			sx = r2.r_left = 0;
			tx = dx;
		}
		else if (x < priv->vlastx) {
			sx = dx;
			tx = 0;
			r2.r_left = pwp->vi.width - dx;
		}
		else {
			sx = tx = 0;
		}

		if (y > priv->vlasty) {
			sy = r1.r_top = 0;
			ty = r2.r_top = dy;
		}
		else if (y < priv->vlasty) {
			sy = dy;
			ty = r2.r_top = 0;
			r1.r_top = r2.r_height;
		}
		else {
			r2.r_top = sy = ty = 0;
		}

		pwp->vi.scr_x = priv->vdownx - (priv->vlastx = x);
		pwp->vi.scr_y = priv->vdowny - (priv->vlasty = y);

		XCopyArea(pwp->vi.dpy, pwp->vi.xid, pwp->vi.xid, priv->copy_gc, sx, sy,
					(unsigned)(r1.r_width - dx), (unsigned)r2.r_height, tx, ty);

		if (dy) {
			if (priv->panvsb) {
				xv_set(priv->panvsb,
					SCROLLBAR_VIEW_START, pwp->vi.scr_y / priv->win_v_unit,
					NULL);
			}
			XClearArea(pwp->vi.dpy, pwp->vi.xid, r1.r_left, r1.r_top,
						(unsigned)r1.r_width, (unsigned)r1.r_height, FALSE);

			trigger_repaint(priv, pwp, &r1, SCROLLWIN_REASON_PAN);
		}

		if (dx) {
			if (priv->panhsb) {
				xv_set(priv->panhsb,
					SCROLLBAR_VIEW_START, pwp->vi.scr_x / priv->win_h_unit,
					NULL);
			}
			XClearArea(pwp->vi.dpy, pwp->vi.xid, r2.r_left, r2.r_top,
						(unsigned)r2.r_width, (unsigned)r2.r_height, FALSE);

			trigger_repaint(priv, pwp, &r2, SCROLLWIN_REASON_PAN);
		}
	}
}

static void end_panning(Scrollwin_private *priv, Scrollpw_private *pwp, Event *ev)
{
	int x, y, vs, unit;

	priv->panning = FALSE;
	XUngrabPointer(pwp->vi.dpy, event_xevent(ev)->xbutton.time);

	x = (int)event_x(ev);
	y = (int)event_y(ev);
	if (x < priv->vminx) x = priv->vminx;
	if (x > priv->vmaxx) x = priv->vmaxx;
	if (y < priv->vminy) y = priv->vminy;
	if (y > priv->vmaxy) y = priv->vmaxy;

	if (priv->panhsb) {
		unit = priv->win_h_unit;
		vs = (unit ? ((priv->vmaxx - x) / unit) : 0);

		if (x == priv->vminx) {
			vs = priv->h_objlen -
				(int)xv_get(priv->panhsb, SCROLLBAR_VIEW_LENGTH);
		}

		xv_set(priv->panhsb, SCROLLBAR_VIEW_START, vs, NULL);

		/* we never know whether the scrollbar accepts it */
		pwp->vi.scr_x = unit * (int)xv_get(priv->panhsb, SCROLLBAR_VIEW_START) +
						pwp->sel_offset;
	}

	if (priv->panvsb) {
		unit = priv->win_v_unit;
		vs = (unit ? ((priv->vmaxy - y) / unit) : 0);

		if (y == priv->vminy) {
			vs = priv->v_objlen -
				(int)xv_get(priv->panvsb, SCROLLBAR_VIEW_LENGTH);
		}

		xv_set(priv->panvsb, SCROLLBAR_VIEW_START, vs, NULL);

		/* we never know whether the scrollbar accepts it */
		pwp->vi.scr_y = unit * (int)xv_get(priv->panvsb, SCROLLBAR_VIEW_START) +
						pwp->sel_offset;

/* 		DTRACE(DTL_EV, "end pan: VS=%d\n", */
/* 				xv_get(priv->panvsb, SCROLLBAR_VIEW_START)); */
	}
	pw_paint(pwp, TRUE);
}

static Notify_value scrollpw_events(Xv_opaque pw, Event *ev, Notify_arg arg, Notify_event_type type)
{
	Scrollpw_private *pwp = PWPRIV(pw);
	Scrollwin_private *priv = pwp->priv;
	Notify_value val;
	Scrollbar sb;
	Xv_window view;
	Rect rect;
	Xv_drop_site ds;

/* 	DTRACE_EVENT(DTL_EV, "scrollpw_events", ev); */

	switch (event_action(ev)) {
		case ACTION_DRAG_MOVE:
		case ACTION_DRAG_COPY:
			if (priv->sel_req) {
				if ((ds = dnd_decode_drop(priv->sel_req, ev)) == XV_ERROR) {
					return NOTIFY_DONE;
				}

				if (xv_get(ds, XV_OWNER) == pw) {
					Scrollwin_drop_struct drop;

					drop.vinfo = &pwp->vi;
					drop.event = ev;
					drop.sel_req = priv->sel_req;
					drop.files = (char **)0;
					drop.cnt = 0;
					drop.virt_x = (((int)event_x(ev) + drop.vinfo->scr_x)*100) /
														priv->scale_percent;
					drop.virt_y = (((int)event_y(ev) + drop.vinfo->scr_y)*100) /
														priv->scale_percent;
					xv_set(SCRPUB(priv), SCROLLWIN_DROP_EVENT, &drop, NULL);

					return NOTIFY_DONE;
				}
			}
			break;

		case WIN_RESIZE:
			val =  notify_next_event_func(pw, (Notify_event)ev, arg, type);

			rect.r_width = pwp->vi.width = (int)xv_get(pw, XV_WIDTH);
			rect.r_height = pwp->vi.height = (int)xv_get(pw, XV_HEIGHT);

			if (pwp->dropsite) {
				rect.r_left = rect.r_top = 0;
				xv_set(pwp->dropsite,
							DROP_SITE_DELETE_REGION_PTR, NULL,
							DROP_SITE_REGION, &rect,
							NULL);
			}

			update_view_scrollbars(priv, pwp->vp->public_self, FALSE);
			subclass_handle_event(priv, pwp, ev);
			return val;

		case WIN_REPAINT:
			if (event_xevent(ev)) {
				XExposeEvent *xev = (XExposeEvent *)event_xevent(ev);

				rect.r_left = xev->x;
				rect.r_top = xev->y;
				rect.r_width = xev->width;
				rect.r_height = xev->height;
			}
			else {
				rect.r_left = rect.r_top = 0;
				rect.r_width = pwp->vi.width;
				rect.r_height = pwp->vi.height;
			}

			trigger_repaint(priv, pwp, &rect, SCROLLWIN_REASON_EXPOSE);
			return NOTIFY_DONE;

		case WIN_NO_EXPOSE: return NOTIFY_DONE;

		case WIN_GRAPHICS_EXPOSE:
			if (event_xevent(ev)) {
				XGraphicsExposeEvent *xev =
							(XGraphicsExposeEvent *)event_xevent(ev);

				rect.r_left = xev->x;
				rect.r_top = xev->y;
				rect.r_width = xev->width;
				rect.r_height = xev->height;
/* 				DTRACE(DTL_EV+20, "GE: rect [%d,%d,%d,%d], cnt=%d, mc=%d\n", */
/* 						xev->x, xev->y, xev->width, xev->height, */
/* 						xev->count, xev->major_code); */
			}
			else {
/* 				DTRACE(DTL_EV, "GE: NO X\n"); */
				rect.r_left = rect.r_top = 0;
				rect.r_width = pwp->vi.width;
				rect.r_height = pwp->vi.height;
			}
			trigger_repaint(priv, pwp, &rect, SCROLLWIN_REASON_EXPOSE);
			return NOTIFY_DONE;

		case ACTION_SELECT:
			if (event_is_down(ev)) {
				if (xv_get(XV_SERVER_FROM_WINDOW(pw),SERVER_EVENT_HAS_PAN_MODIFIERS,ev)) {
					start_panning(priv, pwp, ev);
					return NOTIFY_DONE;
				}
			}
			else {
				if (priv->panning) {
					end_panning(priv, pwp, ev);
					return NOTIFY_DONE;
				}
			}
			break;

		case LOC_WINENTER:
			if (priv->auto_scroll_pw) cancel_auto_scroller(priv);
			break;

		case LOC_WINEXIT:
			if (priv->auto_scroll_enabled) {
				determine_auto_scroll_direction(priv, pwp, ev);
				start_auto_scroller(pwp);
			}
			break;

		case LOC_DRAG:
			if (priv->auto_scroll_pw) {
				determine_auto_scroll_direction(priv, pwp, ev);
			}

			if (action_select_is_down(ev) && priv->panning) {
				update_panning(priv, pwp, ev);
				return NOTIFY_DONE;
			}

			break;

		case ACTION_NEXT_PANE:
			if (event_is_down(ev)) {
				int pwnr;
				Xv_window next_pw, nth_pw;

				/* find paint window number */
				for (pwnr = 0;
						(nth_pw = xv_get(SCRPUB(priv), OPENWIN_NTH_PW, pwnr));
						pwnr++)
				{
					if (nth_pw == pw) break;
				}

/* 				DTRACE(DTL_EV, "own pr-number is %d\n", pwnr); */

				next_pw = xv_get(SCRPUB(priv), OPENWIN_NTH_PW, pwnr + 1);

/* 				DTRACE(DTL_EV, "next pw is %x\n", next_pw); */

				if (next_pw) {
/* 					DTRACE(DTL_EV, "set focus to next pw\n"); */

					/* Set focus to first element in next paint window */
					xv_set(next_pw, WIN_SET_FOCUS, NULL);
					xv_set(SCRPUB(priv), XV_FOCUS_ELEMENT, 0, NULL);
				}
				else {
/* 					DTRACE(DTL_EV, "set focus to next PANE\n"); */
					xv_set(xv_get(SCRPUB(priv), WIN_FRAME),
							FRAME_NEXT_PANE,
							NULL);
				}
			}
			break;

		case ACTION_PREVIOUS_PANE:
			if (event_is_down(ev)) {
				int pwnr;
				Xv_window previous_pw = XV_NULL, nth_pw;

				for (pwnr = 0;
						(nth_pw = xv_get(SCRPUB(priv), OPENWIN_NTH_PW, pwnr));
						pwnr++)
				{
					if (nth_pw == pw) break;
					previous_pw = nth_pw;
				}

				if (pwnr > 0) {
					/* Set focus to last element in previous paint window */
					xv_set(previous_pw, WIN_SET_FOCUS, NULL);
					xv_set(SCRPUB(priv), XV_FOCUS_ELEMENT, -1, NULL);
				}
				else {
					xv_set(xv_get(SCRPUB(priv),WIN_FRAME),
							FRAME_PREVIOUS_PANE,
							NULL);
				}
			}
			break;

		case ACTION_PREVIOUS_ELEMENT:
			/*
			 * Order of precedence:
			 *       previous paint window's horizontal scrollbar
			 *       previous paint window's vertical scrollbar
			 *       last element in previous frame subwindow
			 */
			if (event_is_down(ev)) {
				int vnr;
				Xv_window previous_view = XV_NULL, nth_view;

				view = VIEWPUB(pwp->vp);

				for (vnr = 0;
						(nth_view = xv_get(SCRPUB(priv),OPENWIN_NTH_VIEW,vnr));
						vnr++)
				{
					if (nth_view == view) break;
					previous_view = nth_view;
				}

				if (vnr > 0) {
					sb = xv_get(SCRPUB(priv),
							OPENWIN_HORIZONTAL_SCROLLBAR, previous_view);
					if (!sb)
						sb = xv_get(SCRPUB(priv),
							OPENWIN_VERTICAL_SCROLLBAR, previous_view);
					xv_set(sb, WIN_SET_FOCUS, NULL);
				}
				else {
					/* Go to last element in previous frame subwindow */
					xv_set(xv_get(SCRPUB(priv), WIN_FRAME),
								FRAME_PREVIOUS_ELEMENT,
								NULL);
				}
			}
			break;

		case ACTION_NEXT_ELEMENT:
			/*
			 * Order of precedence:
			 *     paint window's vertical scrollbar
			 *     paint window's horizontal scrollbar
			 *     next frame subwindow
			 */
			if (event_is_down(ev)) {
				view = VIEWPUB(pwp->vp);

				sb = xv_get(SCRPUB(priv), OPENWIN_VERTICAL_SCROLLBAR, view);
				if (!sb)
					sb = xv_get(SCRPUB(priv),OPENWIN_HORIZONTAL_SCROLLBAR,view);

				if (sb) xv_set(sb, WIN_SET_FOCUS, NULL);
				else {
					/* There is no scrollbar attached: go to next pane */
					xv_set(xv_get(SCRPUB(priv), WIN_FRAME), FRAME_NEXT_PANE, NULL);
				}
			}
			break;

		case ACTION_VERTICAL_SCROLLBAR_MENU:
		case ACTION_HORIZONTAL_SCROLLBAR_MENU:
			view = VIEWPUB(pwp->vp);

			if (event_action(ev) == ACTION_VERTICAL_SCROLLBAR_MENU)
				sb = xv_get(SCRPUB(priv), OPENWIN_VERTICAL_SCROLLBAR, view);
			else
				sb = xv_get(SCRPUB(priv),OPENWIN_HORIZONTAL_SCROLLBAR,view);

			if (sb) {
				Event sb_event;

				event_init(&sb_event);
				event_set_action(&sb_event, ACTION_MENU);
				event_set_window(&sb_event, sb);
				sb_event.ie_flags = ev->ie_flags; /* set up/down flag */
				win_post_event(sb, &sb_event, NOTIFY_SAFE);
			}
			break;

		case ACTION_JUMP_MOUSE_TO_INPUT_FOCUS:
			xv_set(pw, WIN_MOUSE_XY, pwp->vi.width/2, pwp->vi.height/2, NULL);
			break;

		default: break;
	}

	if (subclass_handle_event(priv, pwp, ev)) return NOTIFY_DONE;

	return notify_next_event_func(pw, (Notify_event)ev, arg, type);
}

/*  here we handle those event that were offered to subclasses
 *  when the subclasses did not consume them
 */
static void scrollwin_events_left_from_subclass(Scrollwin_private *priv, Scrollwin_event_struct *es)
{
	Xv_opaque menu;
	Xv_window view;
	Scrollbar sb;
	int new;

	if (es->consumed) return;

	if (es->action == ACTION_MENU && priv->auto_scroll_enabled) {
		es->consumed = TRUE;
		return;
	}

	/* at the moment, we handle only down - events here */
	if (! event_is_down(es->event)) return;

	view = xv_get(es->pw, XV_OWNER);

	switch (es->action) {
		case ACTION_HELP:
		case ACTION_MORE_HELP:
		case ACTION_TEXT_HELP:
		case ACTION_MORE_TEXT_HELP:
		case ACTION_INPUT_FOCUS_HELP:
			if (event_is_down(es->event)) {
				char *help = (char *)xv_get(SCRPUB(priv), XV_HELP_DATA);

				if (help) xv_help_show(es->pw, help, es->event);
			}
			break;

		case ACTION_SCROLL_JUMP_UP:
		case ACTION_SCROLL_UP:
		case ACTION_WHEEL_FORWARD:
			if ((sb = xv_get(SCRPUB(priv), OPENWIN_VERTICAL_SCROLLBAR, view))) {
				new = (int)xv_get(sb, SCROLLBAR_VIEW_START);
				if (new > 0) {
					xv_set(sb, SCROLLBAR_VIEW_START, new - 1, NULL);
				}
			}
			es->consumed = TRUE;
			break;

		case ACTION_SCROLL_JUMP_DOWN:
		case ACTION_SCROLL_DOWN:
		case ACTION_WHEEL_BACKWARD:
			if ((sb = xv_get(SCRPUB(priv), OPENWIN_VERTICAL_SCROLLBAR, view))) {
				new = (int)xv_get(sb, SCROLLBAR_VIEW_START);
				xv_set(sb, SCROLLBAR_VIEW_START, new + 1, NULL);
			}
			es->consumed = TRUE;
			break;

		case ACTION_SCROLL_JUMP_LEFT:
		case ACTION_SCROLL_LEFT:
			if ((sb = xv_get(SCRPUB(priv),OPENWIN_HORIZONTAL_SCROLLBAR,view))) {
				new = (int)xv_get(sb, SCROLLBAR_VIEW_START);
				if (new > 0) {
					xv_set(sb, SCROLLBAR_VIEW_START, new - 1, NULL);
				}
			}
			es->consumed = TRUE;
			break;

		case ACTION_SCROLL_JUMP_RIGHT:
		case ACTION_SCROLL_RIGHT:
			if ((sb = xv_get(SCRPUB(priv),OPENWIN_HORIZONTAL_SCROLLBAR,view))) {
				new = (int)xv_get(sb, SCROLLBAR_VIEW_START);
				xv_set(sb, SCROLLBAR_VIEW_START, new + 1, NULL);
			}
			es->consumed = TRUE;
			break;

		case ACTION_SCROLL_LINE_END:
			es->consumed = TRUE;
			if ((sb = xv_get(SCRPUB(priv),OPENWIN_HORIZONTAL_SCROLLBAR,view))) {
				xv_set(sb, SCROLLBAR_VIEW_START, priv->h_objlen, NULL);
			}
			break;

		case ACTION_SCROLL_LINE_START:
			if ((sb = xv_get(SCRPUB(priv),OPENWIN_HORIZONTAL_SCROLLBAR,view))) {
				xv_set(sb, SCROLLBAR_VIEW_START, 0, NULL);
			}
			es->consumed = TRUE;
			break;

		case ACTION_SCROLL_PANE_LEFT:
			if ((sb = xv_get(SCRPUB(priv),OPENWIN_HORIZONTAL_SCROLLBAR,view))) {
				new = (int)xv_get(sb, SCROLLBAR_VIEW_START);
				new -= (int)xv_get(sb, SCROLLBAR_PAGE_LENGTH);
				if (new < 0) new = 0;
				xv_set(sb, SCROLLBAR_VIEW_START, new, NULL);
			}
			es->consumed = TRUE;
			break;

		case ACTION_SCROLL_PANE_RIGHT:
			if ((sb = xv_get(SCRPUB(priv),OPENWIN_HORIZONTAL_SCROLLBAR,view))) {
				new = (int)xv_get(sb, SCROLLBAR_VIEW_START);
				new += (int)xv_get(sb, SCROLLBAR_PAGE_LENGTH);
				xv_set(sb, SCROLLBAR_VIEW_START, new, NULL);
			}
			es->consumed = TRUE;
			break;

		case ACTION_SCROLL_PANE_DOWN:
		case ACTION_PANE_DOWN:
			if ((sb = xv_get(SCRPUB(priv), OPENWIN_VERTICAL_SCROLLBAR, view))) {
				new = (int)xv_get(sb, SCROLLBAR_VIEW_START);
				new += (int)xv_get(sb, SCROLLBAR_PAGE_LENGTH);
				xv_set(sb, SCROLLBAR_VIEW_START, new, NULL);
			}
			es->consumed = TRUE;
			break;

		case ACTION_SCROLL_PANE_UP:
		case ACTION_PANE_UP:
			if ((sb = xv_get(SCRPUB(priv), OPENWIN_VERTICAL_SCROLLBAR, view))) {
				new = (int)xv_get(sb, SCROLLBAR_VIEW_START);
				new -= (int)xv_get(sb, SCROLLBAR_PAGE_LENGTH);
				if (new < 0) new = 0;
				xv_set(sb, SCROLLBAR_VIEW_START, new, NULL);
			}
			es->consumed = TRUE;
			break;

		case ACTION_SCROLL_DATA_END:
			if ((sb = xv_get(SCRPUB(priv), OPENWIN_VERTICAL_SCROLLBAR, view))) {
				xv_set(sb, SCROLLBAR_VIEW_START, priv->v_objlen, NULL);
			}
			es->consumed = TRUE;
			break;

		case ACTION_SCROLL_DATA_START:
			if ((sb = xv_get(SCRPUB(priv), OPENWIN_VERTICAL_SCROLLBAR, view))) {
				xv_set(sb, SCROLLBAR_VIEW_START, 0, NULL);
			}
			es->consumed = TRUE;
			break;

		case ACTION_MENU:
			if ((menu = xv_get(SCRPUB(priv), WIN_MENU))) {
				menu_show(menu, es->pw, es->event, NULL);
				es->consumed = TRUE;
			}
			break;
	}
}

static void set_pw_size(Scrollwin_private *priv, Scrollview_private *vp)
{
	Scrollpw pw = PWPUB(vp->pwp);
	Scrollview view = VIEWPUB(vp);

	xv_set(pw,
			XV_X, 0,
			XV_Y, 0,
			XV_WIDTH, (int)xv_get(view, XV_WIDTH),
			XV_HEIGHT, (int)xv_get(view, XV_HEIGHT),
			NULL);
}

static Notify_value scrollview_events(Xv_opaque view, Event *ev, Scrollbar sbar, Notify_event_type type)
{
	Scrollview_private *vp = VIEWPRIV(view);
	Scrollpw_private *pwp = vp->pwp;
	Scrollwin_private *priv = vp->priv;
	Scrollbar_setting orient;
	int old, new, delta;
	Notify_value val;
	Rect rect;

/* 	DTRACE_EVENT(DTL_EV+50, "scrollview_events", ev); */

	switch (event_action(ev)) {
		case WIN_RESIZE:
			val =  notify_next_event_func(view, (Notify_event)ev, sbar, type);
			set_pw_size(priv, vp);
			return val;

		case ACTION_SPLIT_DESTROY:
			val = notify_next_event_func(view, (Notify_event)ev, sbar, type);

			update_scrollbars(priv, TRUE);
			return val;

		case ACTION_SPLIT_HORIZONTAL:
		case ACTION_SPLIT_VERTICAL:

			val = notify_next_event_func(view, (Notify_event)ev, sbar, type);

			update_scrollbars(priv, TRUE);
			return val;

#ifdef ACTION_SPLIT_DESTROY_PREVIEW_CANCEL
		case ACTION_SPLIT_DESTROY_PREVIEW_CANCEL:
			val = notify_next_event_func(view, (Notify_event)ev, sbar, type);
			if (val != NOTIFY_IGNORED) pw_paint(pwp, FALSE);
			return val;
#endif

		case SCROLLBAR_REQUEST:
			if (priv->panning) return NOTIFY_DONE;

			orient = (Scrollbar_setting)xv_get(sbar, SCROLLBAR_DIRECTION);
			new = (int)xv_get(sbar, SCROLLBAR_VIEW_START);

			if (orient == SCROLLBAR_VERTICAL) {
				old = (pwp->vi.scr_y - pwp->sel_offset) / priv->win_v_unit;
				pwp->vi.scr_y = new * priv->win_v_unit + pwp->sel_offset;

				if ((delta = old - new) < 0) delta = -delta;

				if (! delta) {
					pw_paint(pwp, TRUE);
					return NOTIFY_DONE;
				}

				delta *= priv->win_v_unit;
				if (delta > pwp->vi.height) delta = pwp->vi.height;

				rect.r_left = 0;
				rect.r_width = pwp->vi.width;
				rect.r_height = delta;

/* 				DTRACE(DTL_EV, "sb_req: VS=%d, old=%d\n", new, old); */

				if (old < new) {
					rect.r_top = pwp->vi.height - delta;
					XCopyArea(pwp->vi.dpy, pwp->vi.xid, pwp->vi.xid,
								priv->copy_gc, 0, delta,
								(unsigned)rect.r_width, (unsigned)rect.r_top,
								0, 0);
				}
				else {
					rect.r_top = 0;
					XCopyArea(pwp->vi.dpy, pwp->vi.xid, pwp->vi.xid,
								priv->copy_gc, 0, 0,
								(unsigned)rect.r_width,
								(unsigned)pwp->vi.height - delta,
								0, delta);
				}
			}
			else {
				old = (pwp->vi.scr_x - pwp->sel_offset) / priv->win_h_unit;
				pwp->vi.scr_x = new * priv->win_h_unit + pwp->sel_offset;

				if ((delta = old - new) < 0) delta = -delta;

				if (! delta) {
					pw_paint(pwp, TRUE);
					return NOTIFY_DONE;
				}

				delta *= priv->win_h_unit;
				if (delta > pwp->vi.width) delta = pwp->vi.width;

				rect.r_top = 0;
				rect.r_width = delta;
				rect.r_height = pwp->vi.height;

				if (old < new) {
					rect.r_left = pwp->vi.width - delta;
					XCopyArea(pwp->vi.dpy, pwp->vi.xid, pwp->vi.xid,
								priv->copy_gc, delta, 0,
								(unsigned)rect.r_left, (unsigned)rect.r_height,
								0, 0);
				}
				else {
					rect.r_left = 0;
					XCopyArea(pwp->vi.dpy, pwp->vi.xid, pwp->vi.xid,
								priv->copy_gc, 0, 0,
								(unsigned)pwp->vi.width - delta, (unsigned)rect.r_height,
								delta, 0);
				}
			}

			XClearArea(pwp->vi.dpy, pwp->vi.xid,
								rect.r_left, rect.r_top,
								(unsigned)rect.r_width, (unsigned)rect.r_height,
								FALSE);

			trigger_repaint(priv, pwp, &rect, SCROLLWIN_REASON_SCROLL);
			return NOTIFY_DONE;
	}

	return notify_next_event_func(view, (Notify_event)ev, sbar, type);
}

/*************  scrolled window's paint windows ****************/

/* ARGSUSED */
static int scrollpw_init(Scrollview view, Scrollpw xself, Attr_avlist avlist,
				int *unused)
{
	Xv_scrollpw *self = (Xv_scrollpw *)xself;
	Scrollpw_private *pwp = (Scrollpw_private *)xv_alloc(Scrollpw_private);
	Scrollview_private *vp = VIEWPRIV(view);

	if (!pwp) return XV_ERROR;

	self->private_data = (Xv_opaque)pwp;
	pwp->public_self = (Xv_opaque)self;

	pwp->priv = vp->priv;
	pwp->vp = vp;
	vp->pwp = pwp;

/* 	DTRACE(DTL_INTERN, "scrollpw_init(0x%x)\n", self); */

	pwp->vi.dpy = (Display *)xv_get((Xv_opaque)self, XV_DISPLAY);
	pwp->vi.xid = (Window)xv_get((Xv_opaque)self, XV_XID);
	pwp->vi.scale_percent = pwp->priv->scale_percent;
	pwp->sel_offset = 0;

	xv_set(xself,
			XV_WIDTH, xv_get(view, XV_WIDTH),
			XV_HEIGHT, xv_get(view, XV_HEIGHT),
			NULL);

	return XV_OK;
}

static int scrollpw_destroy(Scrollpw self, Destroy_status status)
{
	if (status == DESTROY_CLEANUP) {
		Scrollpw_private *pwp = PWPRIV(self);

/* 		DTRACE(DTL_INTERN+10, "in scrollpw_destroy(%x)\n", self); */

		if (pwp->dropsite) xv_destroy(pwp->dropsite);
		memset((char *)pwp, 0, sizeof(*pwp));
		xv_free((char *)pwp);
	}
	return XV_OK;
}

static Xv_opaque scrollpw_set(Scrollpw self, Attr_avlist avlist)
{
	register Attr_attribute *attrs;
	Scrollpw_private *pwp = PWPRIV(self);
	Rect rect;
	Scrollwin_private *priv = pwp->priv;

	for (attrs=avlist; *attrs; attrs=attr_next(attrs)) switch ((int)*attrs) {
		case XV_END_CREATE:

			rect.r_left = rect.r_top = 0;
			rect.r_width = pwp->vi.width = (int)xv_get(self, XV_WIDTH);
			rect.r_height = pwp->vi.height = (int)xv_get(self, XV_HEIGHT);

			if (priv->created) {    /* is a split */
				Scrollpw other_pw;

				OPENWIN_EACH_PW(SCRPUB(priv), other_pw)
					if (other_pw != self) {
						xv_set(self,
							WIN_COLOR_INFO, xv_get(other_pw, WIN_COLOR_INFO),
							WIN_CURSOR, xv_get(other_pw, WIN_CURSOR),
							NULL);

						break;
					}
				OPENWIN_END_EACH
			}
			xv_set(self,
						WIN_NOTIFY_IMMEDIATE_EVENT_PROC, scrollpw_events,
						WIN_NOTIFY_SAFE_EVENT_PROC, scrollpw_events,
						WIN_CONSUME_EVENTS,
							WIN_NO_EVENTS,
							WIN_REPAINT,
							WIN_GRAPHICS_EXPOSE,
							WIN_RESIZE,
							WIN_MOUSE_BUTTONS,
							WIN_ASCII_EVENTS, /* help !! */
							LOC_DRAG,
							LOC_WINENTER,
							LOC_WINEXIT,
							KBD_USE,
							KBD_DONE,
							NULL,
						NULL);

			if (priv->drop_kind == (Scrollwin_drop_setting)(-1)) {
				priv->drop_kind = xv_get(SCRPUB(pwp->priv),SCROLLWIN_DROPPABLE);
			}

			/* the drop site implementation is rather funny !! */
			switch (priv->drop_kind) {
				case SCROLLWIN_DROP:
					pwp->dropsite = xv_create(self, DROP_SITE_ITEM,
								DROP_SITE_REGION, &rect,
								NULL);
					break;
				case SCROLLWIN_DEFAULT:
					pwp->dropsite = xv_create(self, DROP_SITE_ITEM,
								DROP_SITE_REGION, &rect,
								DROP_SITE_DEFAULT, TRUE,
								NULL);
					break;
				case SCROLLWIN_DROP_WITH_PREVIEW:
					pwp->dropsite = xv_create(self, DROP_SITE_ITEM,
								DROP_SITE_REGION, &rect,
								DROP_SITE_EVENT_MASK, DND_ENTERLEAVE|DND_MOTION,
								NULL);
					break;
				case SCROLLWIN_DEFAULT_WITH_PREVIEW:
					pwp->dropsite = xv_create(self, DROP_SITE_ITEM,
								DROP_SITE_REGION, &rect,
								DROP_SITE_DEFAULT, TRUE,
								DROP_SITE_EVENT_MASK, DND_ENTERLEAVE|DND_MOTION,
								NULL);
					break;
				default: break;
			}
			break;

		case WIN_CMS_CHANGE:
			xv_set(SCRPUB(priv), SCROLLWIN_PW_CMS_CHANGED, self, NULL);
			break;

		default:
			xv_check_bad_attr(SCROLLPW, *attrs);
			break;
	}

	return XV_OK;
}

static Xv_opaque scrollpw_get(Xv_opaque xself, int *status, Attr_attribute attr, va_list vali)
{
	Xv_scrollpw *self = (Xv_scrollpw *)xself;
	Scrollpw_private *pwp = (Scrollpw_private *)self->private_data;
	Scrollpw_info *vi;

	*status = XV_OK;
	switch ((int)attr) {
		case SCROLLPW_INFO:
			vi = va_arg(vali, Scrollpw_info *);
			*vi = pwp->vi;
			return (Xv_opaque)XV_OK;

		default:
			*status = xv_check_bad_attr(SCROLLPW, attr);
	}
	return (Xv_opaque)XV_OK;
}

/*************  scrolled window's views ****************/

static int scrollview_init(Xv_opaque own, Xv_opaque xself, Attr_avlist avlist,
				int *unused)
{
	Xv_scrollwin *owner = (Xv_scrollwin *)own;
	Xv_scrollview *self = (Xv_scrollview *)xself;
	Scrollview_private *vp = (Scrollview_private *)xv_alloc(Scrollview_private);

	if (!vp) return XV_ERROR;

	self->private_data = (Xv_opaque)vp;
	vp->public_self = (Xv_opaque)self;
	vp->priv = (Scrollwin_private *)owner->private_data;

/* 	DTRACE(DTL_INTERN, "scrollview_init(0x%x)\n", self); */

	return XV_OK;
}

static int scrollview_destroy(Scrollview self, Destroy_status status)
{
	if (status == DESTROY_CLEANUP || status == DESTROY_PROCESS_DEATH) {
		Scrollview_private *vp = VIEWPRIV(self);

/* 		DTRACE(DTL_INTERN+10, "in scrollview_destroy(%x)\n", self); */

		if (xv_destroy_status(PWPUB(vp->pwp), status) != XV_OK) return XV_ERROR;

		memset((char *)vp, 0, sizeof(*vp));
		xv_free((char *)vp);
	}
	return XV_OK;
}

static Xv_opaque scrollview_set(Scrollview self, Attr_avlist avlist)
{
	register Attr_attribute *attrs;
	Scrollview_private *vp = VIEWPRIV(self);
	Scrollwin_private *priv = vp->priv;
	int vs, vl;
	Scrollbar sb;

	for (attrs=avlist; *attrs; attrs=attr_next(attrs)) switch ((int)*attrs) {
		case SCROLLVIEW_V_START:
			if ((sb = xv_get(SCRPUB(priv), OPENWIN_VERTICAL_SCROLLBAR, self))) {
				vs = (int)A1;
				vl = (int)xv_get(sb, SCROLLBAR_VIEW_LENGTH);
				if (vs > priv->v_objlen - vl) vs = priv->v_objlen - vl;
				if (vs < 0) vs = 0;

				vp->pwp->vi.scr_y = priv->win_v_unit * vs + vp->pwp->sel_offset;
				xv_set(sb, SCROLLBAR_VIEW_START, vs, NULL);

				vp->pwp->vi.scr_y = priv->win_v_unit *
									(int)xv_get(sb, SCROLLBAR_VIEW_START) +
									vp->pwp->sel_offset;
			}
			ADONE;

		case SCROLLVIEW_H_START:
			if ((sb = xv_get(SCRPUB(priv),OPENWIN_HORIZONTAL_SCROLLBAR,self))) {
				vs = (int)A1;
				vl = (int)xv_get(sb, SCROLLBAR_VIEW_LENGTH);
				if (vs > priv->h_objlen - vl) vs = priv->h_objlen - vl;
				if (vs < 0) vs = 0;

				vp->pwp->vi.scr_x = priv->win_h_unit * vs + vp->pwp->sel_offset;
				xv_set(sb, SCROLLBAR_VIEW_START, vs, NULL);

				vp->pwp->vi.scr_x = priv->win_h_unit *
									(int)xv_get(sb, SCROLLBAR_VIEW_START) +
									vp->pwp->sel_offset;
			}
			ADONE;

		case WIN_SET_FOCUS:
			xv_set(PWPUB(vp->pwp), WIN_SET_FOCUS, NULL);
			ADONE;

		case XV_END_CREATE:

			xv_set(self,
				WIN_NOTIFY_IMMEDIATE_EVENT_PROC, scrollview_events,
				WIN_NOTIFY_SAFE_EVENT_PROC, scrollview_events,
				WIN_CONSUME_EVENTS,
					WIN_RESIZE,
					ACTION_HELP,
					NULL,
				NULL);

			if (priv->created) {    /* is a split */
				Scrollview other_view;

				OPENWIN_EACH_VIEW(SCRPUB(priv), other_view)
					if (other_view != self) {
						xv_set(self,
							XV_HELP_DATA, xv_get(other_view, XV_HELP_DATA),
							NULL);

						break;
					}
				OPENWIN_END_EACH
			}

			update_view_scrollbars(priv, self, FALSE);

			break;

		default:
			xv_check_bad_attr(SCROLLVIEW, *attrs);
			break;
	}
	return XV_OK;
}

static Xv_opaque scrollview_get(Scrollview self, int *status, Attr_attribute attr, va_list vali)
{
	Scrollview_private *vp = VIEWPRIV(self);
	Scrollwin_private *priv = vp->priv;
	Scrollbar sb;

	*status = XV_OK;
	switch ((int)attr) {
		case SCROLLVIEW_H_START:
			if ((sb = xv_get(SCRPUB(priv),OPENWIN_HORIZONTAL_SCROLLBAR,self))) {
				return xv_get(sb, SCROLLBAR_VIEW_START);
			}
			else return XV_NULL;

		case SCROLLVIEW_V_START:
			if ((sb = xv_get(SCRPUB(priv), OPENWIN_VERTICAL_SCROLLBAR, self))) {
				return xv_get(sb, SCROLLBAR_VIEW_START);
			}
			else return XV_NULL;

		default:
			*status = xv_check_bad_attr(SCROLLVIEW, attr);
	}
	return (Xv_opaque)XV_OK;
}

static void scrollwin_end_of_creation(Scrollwin_private *priv)
{
	Scrollwin self = SCRPUB(priv);
	Display *dpy = (Display *)xv_get(self, XV_DISPLAY);
	Window xid = (Window)xv_get(self, XV_XID);
	XGCValues   gcv;

	/* if we set graphics_exposures to FALSE, panning produces
	 * dirty windows when the panned window is partially obscured!
	 */
	gcv.graphics_exposures = TRUE;
	priv->copy_gc = XCreateGC(dpy, xid, GCGraphicsExposures, &gcv);

	priv->sel_req = xv_get(self, SCROLLWIN_CREATE_SEL_REQ);
	xv_set(self, OPENWIN_AUTO_CLEAR, FALSE, NULL);
	priv->created = TRUE;
}

static Xv_opaque scrollwin_set(Scrollwin self, Attr_avlist avlist)
{
	register Attr_attribute *attrs;
	Scrollwin_private *priv = SCRPRIV(self);
	Xv_opaque view;
	int retval = XV_OK;
	int old_scale;
	short sb_upd = FALSE;
	int scaling_changed = FALSE;

	for (attrs=avlist; *attrs; attrs=attr_next(attrs)) {
/* 		DTRACE_ATTR(DTL_SET, *attrs); */
		switch ((int)*attrs) {
			case SCROLLWIN_HANDLE_EVENT:
				scrollwin_events_left_from_subclass(priv,
									(Scrollwin_event_struct *)A1);

				ADONE;

			case SCROLLWIN_REPAINT:
				xv_error(self,
						ERROR_PKG, SCROLLWIN,
						ERROR_SEVERITY, ERROR_RECOVERABLE,
						ERROR_LAYER, ERROR_PROGRAM,
						ERROR_STRING,"SCROLLWIN_REPAINT is subclass responsibility",
						NULL);
				ADONE;

			case SCROLLWIN_HANDLE_DROP:
				ADONE;

			case SCROLLWIN_UPDATE_PROC:
				priv->update_proc = (update_proc_t)A1;
				ADONE;

			case SCROLLWIN_DROP_EVENT:
#ifdef OLD_STUFF
				{
					Scrollwin_drop_struct *drop = (Scrollwin_drop_struct *)A1;

					xv_set(drop->sel_req,
								FILE_REQ_ALLOCATE, FALSE,
								FILE_REQ_ALREADY_DECODED, TRUE,
								FILE_REQ_FETCH, drop->event,
								NULL);
					drop->files = (char **)xv_get(drop->sel_req, FILE_REQ_FILES,
												&drop->cnt);

					xv_set(SCRPUB(priv), SCROLLWIN_HANDLE_DROP, drop, NULL);

					dnd_done(drop->sel_req);
				}
#else /* OLD_STUFF */
				{
					char buf[200];

					/* jede Klasse, bei der das kommt, sollte das
					 * OLD_STUFF-Zeug bekommen
					 */
					sprintf(buf, "Class %s handles SCROLLWIN_CREATE_SEL_REQ,\nbut not SCROLLWIN_DROP_EVENT",
							((Xv_pkg *)xv_get(self, XV_TYPE))->name);
					xv_error(self,
						ERROR_PKG, SCROLLWIN,
						ERROR_SEVERITY, ERROR_RECOVERABLE,
						ERROR_LAYER, ERROR_PROGRAM,
						ERROR_STRING, buf,
						NULL);
				}
#endif /* OLD_STUFF */
				ADONE;

			case SCROLLWIN_DROPPABLE:
				priv->drop_kind = (Scrollwin_drop_setting)A1;
				ADONE;

			case SCROLLWIN_TRIGGER_REPAINT:
				OPENWIN_EACH_VIEW(self, view)
					Scrollview_private *vp= VIEWPRIV(view);

					pw_paint(vp->pwp, TRUE);
				OPENWIN_END_EACH
				ADONE;

			case SCROLLWIN_SCALE_PERCENT:
				old_scale = priv->scale_percent;
				priv->scale_percent = (int)A1;
				if (priv->scale_percent <= 0) priv->scale_percent = 100;
				OPENWIN_EACH_VIEW(self, view)
					Scrollview_private *vp= VIEWPRIV(view);

					vp->pwp->vi.scale_percent = priv->scale_percent;
				OPENWIN_END_EACH
				if (old_scale != priv->scale_percent) scaling_changed = TRUE;
				ADONE;

			case SCROLLWIN_SCALE_CHANGED:
				ADONE;

			case SCROLLWIN_V_OBJECT_LENGTH:
				priv->v_objlen = (int)A1;
				if (priv->v_objlen <= 0) priv->v_objlen = 1;
				sb_upd = TRUE;
				ADONE;

			case SCROLLWIN_H_OBJECT_LENGTH:
				priv->h_objlen = (int)A1;
				if (priv->h_objlen <= 0) priv->h_objlen = 1;
				sb_upd = TRUE;
				ADONE;

			case SCROLLWIN_V_UNIT:
				priv->v_unit = (int)A1;
				if (priv->v_unit <= 0) priv->v_unit = 1;
				scaling_changed = TRUE;
				ADONE;

			case SCROLLWIN_H_UNIT:
				priv->h_unit = (int)A1;
				if (priv->h_unit <= 0) priv->h_unit = 1;
				scaling_changed = TRUE;
				ADONE;

			case SCROLLWIN_RESTRICT_PAN_PTR:
				priv->restrict_panning = (char)A1;
				ADONE;

			case SCROLLWIN_AUTO_SCROLL:
				ADONE;

			case SCROLLWIN_ENABLE_AUTO_SCROLL:
				priv->auto_scroll_enabled = (char)A1;
				if (priv->auto_scroll_pw && ! priv->auto_scroll_enabled) {
					cancel_auto_scroller(priv);
				}
				ADONE;

			case SCROLLWIN_PW_CMS_CHANGED:
				ADONE;

			case XV_END_CREATE:
				scrollwin_end_of_creation(priv);
				break;

			default:
				xv_check_bad_attr(SCROLLWIN, A0);
		}
	}

	if (scaling_changed) {
    	priv->win_v_unit = (priv->v_unit * priv->scale_percent) / 100;
    	if (priv->win_v_unit < 1) priv->win_v_unit = 1;
    	priv->win_h_unit = (priv->h_unit * priv->scale_percent) / 100;
    	if (priv->win_h_unit < 1) priv->win_h_unit = 1;

		if (priv->created) xv_set(self, SCROLLWIN_SCALE_CHANGED, NULL);
		sb_upd = TRUE;
	}

	if (sb_upd) {
		update_scrollbars(priv, FALSE);
		update_scrollbars(priv, TRUE);
	}

	return retval;
}

static Xv_opaque scrollwin_get(Scrollwin self, int *status, Attr_attribute attr, va_list vali)
{
	Scrollwin_private *priv = SCRPRIV(self);

	*status = XV_OK;
	switch ((int)attr) {
		case SCROLLWIN_TRIGGER_REPAINT:
		case SCROLLWIN_REPAINT:
		case SCROLLWIN_HANDLE_EVENT:
			xv_error(self,
				ERROR_SEVERITY, ERROR_RECOVERABLE,
				ERROR_PKG, SCROLLWIN,
				ERROR_LAYER, ERROR_PROGRAM,
				ERROR_CANNOT_GET, attr,
				NULL);
			return (Xv_opaque)XV_OK;

		case SCROLLWIN_UPDATE_PROC: return (Xv_opaque)priv->update_proc;
		case SCROLLWIN_SCALE_PERCENT: return (Xv_opaque)priv->scale_percent;
		case SCROLLWIN_V_OBJECT_LENGTH: return (Xv_opaque)priv->v_objlen;
		case SCROLLWIN_H_OBJECT_LENGTH: return (Xv_opaque)priv->h_objlen;
		case SCROLLWIN_V_UNIT: return (Xv_opaque)priv->v_unit;
		case SCROLLWIN_H_UNIT: return (Xv_opaque)priv->h_unit;

		case SCROLLWIN_ENABLE_AUTO_SCROLL:
			return (Xv_opaque)priv->auto_scroll_enabled;

		case SCROLLWIN_RESTRICT_PAN_PTR:
			return (Xv_opaque)priv->restrict_panning;

		case OPENWIN_VIEW_CLASS: return (Xv_opaque)SCROLLVIEW;
		case OPENWIN_PW_CLASS: return (Xv_opaque)SCROLLPW;
		case SCROLLWIN_DROPPABLE: return (Xv_opaque)SCROLLWIN_NONE;
		case SCROLLWIN_CREATE_SEL_REQ:
			/* for subclasses... */
			return XV_NULL;

		default:
			*status = xv_check_bad_attr(SCROLLWIN, attr);
			return (Xv_opaque)XV_OK;
	}
}

static int scrollwin_init(Xv_opaque uuowner, Xv_opaque xself,
				Attr_avlist uuavlist, int *unused)
{
	Xv_scrollwin *self = (Xv_scrollwin *)xself;
	Scrollwin_private *priv;
	int delay_time, line_interval;

	priv = (Scrollwin_private *)xv_alloc(Scrollwin_private);
	if (!priv) return XV_ERROR;

	priv->public_self = (Xv_opaque)self;
	self->private_data = (Xv_opaque)priv;

	priv->drop_kind = (Scrollwin_drop_setting)(-1);
	priv->restrict_panning = TRUE;

	priv->v_objlen = priv->h_objlen = priv->v_unit = priv->h_unit = 1;
    priv->win_v_unit = priv->win_h_unit = 1;
	priv->scale_percent = 100;

	priv->destroying = FALSE;

	priv->pan_cursor = xv_create((Xv_opaque)self, CURSOR,
						CURSOR_SRC_CHAR, OLC_PANNING_PTR,
						CURSOR_MASK_CHAR, OLC_PANNING_PTR+1,
						NULL);

	delay_time = defaults_get_integer_check("scrollbar.repeatDelay",
							"Scrollbar.RepeatDelay", 300, 0, 999);

	line_interval = defaults_get_integer_check("scrollbar.lineInterval",
							"Scrollbar.LineInterval", 10, 0, 999);

	priv->auto_scroll_time.it_value.tv_usec = delay_time * 1000;
	priv->auto_scroll_time.it_value.tv_sec = 0;
	priv->auto_scroll_time.it_interval.tv_usec = line_interval * 1000;
	priv->auto_scroll_time.it_interval.tv_sec = 0;

	/*
	 * 1. Make all the paint windows inherit the WIN_DYNAMIC_VISUAL attribute.
	 * 2. The Scrollwin is, by default, a First-Class (primary) focus client.
	 */
	xv_set((Xv_opaque)self,
						WIN_INHERIT_COLORS, TRUE,
						XV_FOCUS_RANK, XV_FOCUS_PRIMARY,
						NULL);

	return XV_OK;
}

static int scrollwin_destroy(Scrollwin self, Destroy_status status)
{
	if (status == DESTROY_CLEANUP) {
		Xv_scrollwin *sws;
		Scrollwin_private *priv = SCRPRIV(self);
		Display *dpy = (Display *)xv_get(self, XV_DISPLAY);
		Scrollview v;

/* 		DTRACE(DTL_INTERN+10, "in scrollwin_destroy(%x)\n", self); */
		priv->destroying = TRUE;
		if (priv->sel_req) xv_destroy(priv->sel_req);
		if (priv->pan_cursor) xv_destroy(priv->pan_cursor);

		OPENWIN_EACH_VIEW(self, v)
			Scrollview_private *vp = VIEWPRIV(v);

			vp->priv = 0;
		OPENWIN_END_EACH

		if (priv->copy_gc) XFreeGC(dpy, priv->copy_gc);
		memset((char *)priv, 0, sizeof(*priv));
		xv_free((char *)priv);
		sws = (Xv_scrollwin *)self;
		sws->private_data = 0;
	}
	return XV_OK;
}

Xv_pkg xv_scrollwin_pkg = {
	"ScrollWin",
	ATTR_PKG_SCROLL,
	sizeof(Xv_scrollwin),
	OPENWIN,
	scrollwin_init,
	scrollwin_set,
	scrollwin_get,
	scrollwin_destroy,
	0
};

Xv_pkg xv_scrollview_pkg = {
	"ScrollView",
	ATTR_PKG_SCROLL,
	sizeof(Xv_scrollview),
	OPENWIN_VIEW,
	scrollview_init,
	scrollview_set,
	scrollview_get,
	scrollview_destroy,
	0
};

Xv_pkg xv_scrollpw_pkg = {
	"ScrollPaintWindow",
	ATTR_PKG_SCROLL,
	sizeof(Xv_scrollpw),
	WINDOW,
	scrollpw_init,
	scrollpw_set,
	scrollpw_get,
	scrollpw_destroy,
	0
};
