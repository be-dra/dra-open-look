#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)canvas.c 20.44 93/06/28  DRA: $Id: canvas.c,v 4.5 2025/03/11 16:57:42 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL_NOTICE 
 *	file for terms of the license.
 */

#define xview_other_rl_funcs 1
/* #include <xview_private/cnvs_impl.h> */
#include <xview_private/draw_impl.h> 
#include <xview_private/scrn_impl.h>
#include <xview_private/svr_impl.h>
#include <xview_private/attr_impl.h>
#include <xview_private/win_info.h>
#include <xview_private/windowimpl.h>
#include <xview/canvas.h>
#include <xview/scrollbar.h>
#include <xview/cursor.h>
#include <xview/help.h>
#include <xview/win_notify.h>

#ifdef	OW_I18N
#include <xview/font.h>
#include <xview/frame.h>
#include <xview/panel.h>
#include <xview_private/i18n_impl.h>  


Xv_private_data Attr_attribute  canvas_pew_key;
#endif /*OW_I18N*/

#define	CANVAS_PRIVATE(c)	XV_PRIVATE(Canvas_info, Xv_canvas, c)
#define	CANVAS_PUBLIC(canvas)	XV_PUBLIC(canvas)

#define	CANVAS_VIEW_PRIVATE(c)	XV_PRIVATE(Canvas_view_info, Xv_canvas_view, c)
#define	CANVAS_VIEW_PUBLIC(canvas_view)	XV_PUBLIC(canvas_view)


#define	BIT_FIELD(field)	unsigned field : 1


typedef void (*CanvasResizeFunction_t)(Canvas, int, int);

typedef void (*CanvasRepaintFunction_t)(Canvas, Xv_window, Display *, Window,
										Xv_xrectlist *);
typedef void (*CanvasOldRepaintFunction_t)(Canvas, Xv_window, Rectlist *);

#ifdef OW_I18N
/*
 * pew (PreEdit Window) data structure, this will be hanging off on
 * the parent frame using XV_KEY_DATA.
 */
typedef struct {
    Frame	frame;
    Panel	panel;
    Panel_item	ptxt;
    int		reference_count;
    int		active_count;
} Canvas_pew;
#endif /* OW_I18N */

typedef struct {
    Canvas	public_self;	/* back pointer to public self */
    int		margin;		/* view pixwin margin */
    int		width, height;
    int		min_paint_width;
    int		min_paint_height;
    CanvasRepaintFunction_t	repaint_proc;
    CanvasOldRepaintFunction_t	old_repaint_proc;
    CanvasResizeFunction_t 	resize_proc;
    Attr_avlist	paint_avlist; 	/* cached pw avlist on create */
    Attr_avlist	paint_end_avlist;

    struct {
	BIT_FIELD(auto_expand);		/* auto expand canvas with window */
	BIT_FIELD(auto_shrink);		/* auto shrink canvas with window */
	BIT_FIELD(fixed_image);		/* canvas is a fixed size image */
	BIT_FIELD(retained);		/* canvas is a retained window */
	BIT_FIELD(created);		/* first paint window is created */
	BIT_FIELD(x_canvas);		/* treat canvas as an X drawing surface */
	BIT_FIELD(no_clipping);		/* ignore clip rects on repaint */
	BIT_FIELD(cms_repaint);         /* generate repaint on cms changes */
#ifdef OW_I18N
	BIT_FIELD(preedit_exist);       /* keep track of preedit status */
#endif
	BIT_FIELD(panning);
    } status_bits;

	Xv_opaque pan_cursor;
	int pan_x, pan_y;
	Scrollbar panhsb, panvsb;

#ifdef OW_I18N
    /*
     * pe_cache is used to cache the current preedit string for each canvas.
     * Since all canvasses in a frame share the preedit window, but
     * we show the string for the canvas that has the focus, we need to
     * cache for each canvas.
     */
    XIC		ic;			/* cache the ic */
    XIMPreeditDrawCallbackStruct *pe_cache; /* cache current preedit string */
    Canvas_pew	*pew;			/* handle to preedit window (Frame) */
    Canvas_paint_window	focus_pwin;	/* Last paint win which had a focus */
#ifdef FULL_R5
    XIMStyle	     xim_style;
#endif /*FULL_R5*/
#endif /*OW_I18N*/
} Canvas_info;

typedef struct {
    Canvas_view	public_self;	/* back pointer to public self */
    Canvas_info		*private_canvas;
    Xv_Window		paint_window;
} Canvas_view_info;


#define	status(canvas, field)		((canvas)->status_bits.field)
#define	status_set(canvas, field)	status(canvas, field) = TRUE
#define	status_reset(canvas, field)	status(canvas, field) = FALSE

static Attr_attribute  canvas_context_key;
static Attr_attribute  canvas_view_context_key;

static void canvas_set_scrollbar_object_length(Canvas_info *canvas, Scrollbar_setting direction, Scrollbar sb)
{
    int             is_vertical = direction == SCROLLBAR_VERTICAL;
    int             pixels_per;
    long unsigned   current_length, new_length;

    if (sb) {
	pixels_per = (int) xv_get(sb, SCROLLBAR_PIXELS_PER_UNIT);

	if (pixels_per == 0) {
	    pixels_per = 1;
	}
	new_length = ((is_vertical) ?
		      canvas->height : canvas->width) / pixels_per;
	current_length = (long unsigned) xv_get(sb, SCROLLBAR_OBJECT_LENGTH);

	if (new_length != current_length) {
	    /* let the scrollbar know how big the scrolling area is */
	    (void) xv_set(sb,
			  SCROLLBAR_OBJECT_LENGTH, new_length,
			  NULL);
	}
    }
}


#ifdef SEEMS_UNUSED
static void canvas_update_scrollbars(Canvas_info *canvas)
{
    Canvas          canvas_public = CANVAS_PUBLIC(canvas);
    Xv_Window       view;
    Scrollbar       sb;

    OPENWIN_EACH_VIEW(canvas_public, view)
	sb = (Scrollbar) xv_get(canvas_public, OPENWIN_VERTICAL_SCROLLBAR, view);
        if (sb) {
		canvas_set_scrollbar_object_length(canvas, SCROLLBAR_VERTICAL, sb);
		canvas_scroll(xv_get(view, CANVAS_VIEW_PAINT_WINDOW, 0), sb);
	}
        sb = (Scrollbar) xv_get(canvas_public, OPENWIN_HORIZONTAL_SCROLLBAR, view);
        if (sb) {
		canvas_set_scrollbar_object_length(canvas, SCROLLBAR_HORIZONTAL, sb);
		canvas_scroll(xv_get(view, CANVAS_VIEW_PAINT_WINDOW, 0), sb);
	}		
    OPENWIN_END_EACH
}
#endif /* SEEMS_UNUSED */

/*
 * scroll the canvas according to LAST_VIEW_START and VIEW_START.
 */
static void canvas_scroll(Xv_Window paint_window, Scrollbar sb)
{
    int             offset = (int) xv_get(sb, SCROLLBAR_VIEW_START);
    int             old_offset = (int) xv_get(sb, SCROLLBAR_LAST_VIEW_START);
    int             is_vertical;
    int             pixels_per;

    if (offset == old_offset)
	return;
    is_vertical = (Scrollbar_setting) xv_get(sb, SCROLLBAR_DIRECTION) == SCROLLBAR_VERTICAL;
    pixels_per = (int) xv_get(sb, SCROLLBAR_PIXELS_PER_UNIT);
    xv_set(paint_window, is_vertical ? XV_Y : XV_X, -(offset * pixels_per), NULL);

}

static void canvas_view_maxsize(Canvas_info *canvas, int *view_width,
									int *view_height)
{
	Xv_Window view;
	Rect view_rect;

	*view_width = *view_height = 0;

	OPENWIN_EACH_VIEW(CANVAS_PUBLIC(canvas), view)
		view_rect = *(Rect *) xv_get(view, WIN_RECT);
		*view_width = MAX(*view_width, view_rect.r_width);
		*view_height = MAX(*view_height, view_rect.r_height);
	OPENWIN_END_EACH
}

static void canvas_set_paint_window_size(Canvas_info *canvas, int width, int height)
{
    Canvas	    canvas_public = CANVAS_PUBLIC(canvas);
    Xv_Window       paint_window;
    Rect            paint_rect;
    Xv_Window       view_window;
    Rect	    view_rect;
    unsigned int    visable;
    Scrollbar	    sb;

    canvas->width = MAX(width, 1);
    canvas->height = MAX(height, 1);
    CANVAS_EACH_PAINT_WINDOW(canvas_public, paint_window)
		paint_rect = *(Rect *) xv_get(paint_window, WIN_RECT, 0);
		paint_rect.r_width = canvas->width;
		paint_rect.r_height = canvas->height;
		view_window = (Xv_Window) xv_get(paint_window, CANVAS_PAINT_VIEW_WINDOW, 0);
		view_rect = *(Rect *) xv_get(view_window, WIN_RECT, 0);

		/*
		 * check to see if paint window needs to be moved to accomodate 
		 * new size
		 */
		if (paint_rect.r_width <= view_rect.r_width)
			paint_rect.r_left = 0;
		else {
			visable = paint_rect.r_width + paint_rect.r_left;
			if (visable < view_rect.r_width)
				paint_rect.r_left += view_rect.r_width - visable;
		}

		if (paint_rect.r_height <= view_rect.r_height)
			paint_rect.r_top = 0;
		else {
			visable = paint_rect.r_height + paint_rect.r_top;
			if (visable < view_rect.r_height)
				paint_rect.r_top += view_rect.r_height - visable;
		}

		/* update any scrollbars */
		sb = (Scrollbar) xv_get(canvas_public, OPENWIN_VERTICAL_SCROLLBAR,
				view_window);
		if (sb)
			canvas_set_scrollbar_object_length(canvas, SCROLLBAR_VERTICAL, sb);
		sb = (Scrollbar) xv_get(canvas_public, OPENWIN_HORIZONTAL_SCROLLBAR,
				view_window);
		if (sb)
			canvas_set_scrollbar_object_length(canvas, SCROLLBAR_HORIZONTAL, sb);

		xv_set(paint_window, XV_RECT, &paint_rect, NULL);
    CANVAS_END_EACH
}

/*
 * resize the paint window to account for the new view window size.
 */

static void canvas_resize_paint_window(Canvas_info *canvas, int width, int height)
{
	int view_width = 0, view_height = 0;

	/* paint window doesn't ever change size */

	/* use the old value if none specified */
	if (width == 0) {
		width = canvas->width;
	}
	if (height == 0) {
		height = canvas->height;
	}
	/* determine maximim view size of all viewers */
	if (status(canvas, auto_expand) || status(canvas, auto_shrink)) {
		canvas_view_maxsize(canvas, &view_width, &view_height);
	}
	/*
	 * if auto expand is on, always expand the canvas to at the edges of the
	 * viewing pixwin or the minimum width/height.
	 */
	if (status(canvas, auto_expand)) {
		width = MAX(width, view_width);
		height = MAX(height, view_height);
	}
	/*
	 * if auto shrink is on, always shrink the canvas to the edges of the
	 * viewing pixwin or the minimum width/height.
	 */
	if (status(canvas, auto_shrink)) {
		width = MIN(width, view_width);
		height = MIN(height, view_height);
	}
	/* width and height must equal some minimum */
	width = MAX(width, canvas->min_paint_width);
	height = MAX(height, canvas->min_paint_height);

	canvas_set_paint_window_size(canvas, width, height);
}

static void start_panning(Canvas_info *priv, Xv_window pw, Event *ev)
{
	Xv_window view;
	Display *dpy = (Display *)xv_get(pw, XV_DISPLAY);
	Window grabwindow;

	view = CANVAS_VIEW_PUBLIC((Canvas_view_info *) xv_get(pw,
										XV_KEY_DATA, canvas_view_context_key));

	priv->pan_x = event_x(ev); /* this is considered as the vector from */
	priv->pan_y = event_y(ev); /* the left top corner to the 'pan point' */

	priv->panhsb = xv_get(CANVAS_PUBLIC(priv),OPENWIN_HORIZONTAL_SCROLLBAR,view);
	priv->panvsb = xv_get(CANVAS_PUBLIC(priv),OPENWIN_VERTICAL_SCROLLBAR,view);

	grabwindow = xv_get(pw, XV_XID);
	/* mal versuchen */
	grabwindow = xv_get(view, XV_XID);

	/* grab the pointer for this paint window */
	XGrabPointer(dpy, grabwindow, False,
			(unsigned)(ButtonMotionMask | ButtonReleaseMask),
			GrabModeAsync, GrabModeAsync,
			grabwindow, (Cursor)xv_get(priv->pan_cursor, XV_XID),
			event_xevent(ev)->xbutton.time);
	XFlush(dpy);
	status_set(priv, panning);
}

static void update_view_panning(Canvas_info *priv, Canvas_view_info *view,
											Event *ev)
{
	Display *dpy = (Display *)xv_get(view->paint_window, XV_DISPLAY);
	Window win = xv_get(view->paint_window, XV_XID);
	XEvent event;
	int new_win_x, new_win_y;
	int x, y, cnt;

	/* get rid of OLD MotionNotify events, only take the last */
	cnt = 0;
	while (XCheckTypedWindowEvent(dpy, win, MotionNotify,&event))
		++cnt;

	if (cnt) {
		x = event.xmotion.x;
		y = event.xmotion.y;
	}
	else {
		x = (int)event_x(ev);
		y = (int)event_y(ev);
	}

	new_win_x = x - priv->pan_x;
	new_win_y = y - priv->pan_y;

	if (new_win_y > 0) new_win_y = 0;
	if (new_win_x > 0) new_win_x = 0;

	XMoveWindow(dpy, win, new_win_x, new_win_y);
	XSync(dpy, False);
}

static void update_panning(Canvas_info *priv, Xv_window pw, Event *ev)
{
	Display *dpy = (Display *)xv_get(pw, XV_DISPLAY);
	Window win = xv_get(pw, XV_XID);
	XEvent event;
	int new_win_x, new_win_y;
	int x, y, cnt;

	/* get rid of OLD MotionNotify events, only take the last */
	cnt = 0;
	while (XCheckTypedWindowEvent(dpy, win, MotionNotify,&event))
		++cnt;

	if (cnt) {
		x = event.xmotion.x;
		y = event.xmotion.y;
	}
	else {
		x = (int)event_x(ev);
		y = (int)event_y(ev);
	}

	new_win_x = x - priv->pan_x;
	new_win_y = y - priv->pan_y;

	if (new_win_x > 0) new_win_x = 0;
	if (new_win_y > 0) new_win_y = 0;

	XMoveWindow(dpy, win, new_win_x, new_win_y);
/* 	XSync(dpy, True); */
#ifdef NOT_YET
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
					r1.r_width - dx, r2.r_height, tx, ty);

		if (dy) {
			if (priv->panvsb) {
				xv_set(priv->panvsb,
					SCROLLBAR_VIEW_START, pwp->vi.scr_y / priv->win_v_unit,
					NULL);
			}
			XClearArea(pwp->vi.dpy, pwp->vi.xid, r1.r_left, r1.r_top,
						r1.r_width, r1.r_height, FALSE);

			trigger_repaint(priv, pwp, &r1, SCROLLWIN_REASON_PAN);
		}

		if (dx) {
			if (priv->panhsb) {
				xv_set(priv->panhsb,
					SCROLLBAR_VIEW_START, pwp->vi.scr_x / priv->win_h_unit,
					NULL);
			}
			XClearArea(pwp->vi.dpy, pwp->vi.xid, r2.r_left, r2.r_top,
						r2.r_width, r2.r_height, FALSE);

			trigger_repaint(priv, pwp, &r2, SCROLLWIN_REASON_PAN);
		}
	}
#endif /* NOT_YET */
}

static void end_view_panning(Canvas_info *priv, Canvas_view_info *view,
											Event *ev)
{
	Display *dpy = (Display *)xv_get(view->paint_window, XV_DISPLAY);

	status_reset(priv, panning);
	XUngrabPointer(dpy, event_xevent(ev)->xbutton.time);
}


/* Clear the damaged area */
static void canvas_clear_damage(Xv_Window window, Rectlist *rl)
{
	register Xv_Drawable_info *info;
	Xv_xrectlist *clip_xrects;
	Display *display;
	XGCValues gc_values;
	unsigned long gc_value_mask;
	Xv_Screen screen;
	GC *gc_list;

	if (!rl)
		return;
	DRAWABLE_INFO_MACRO(window, info);
	clip_xrects = screen_get_clip_rects(xv_screen(info));
	gc_value_mask = GCForeground | GCBackground | GCFunction | GCPlaneMask |
			GCSubwindowMode | GCFillStyle;
	gc_values.background = xv_bg(info);
	gc_values.function = GXcopy;
	gc_values.plane_mask = xv_plane_mask(info);
	if ((gc_values.stipple = xv_get(window, WIN_BACKGROUND_PIXMAP))) {
		gc_value_mask |= GCStipple;
		gc_values.foreground = xv_fg(info);
		gc_values.fill_style = FillOpaqueStippled;
	}
	else {
		gc_values.foreground = xv_bg(info);
		gc_values.fill_style = FillSolid;
	}
	if (server_get_fullscreen(xv_server(info)))
		gc_values.subwindow_mode = IncludeInferiors;
	else
		gc_values.subwindow_mode = ClipByChildren;
	display = xv_display(info);
	screen = xv_screen(info);
	gc_list = (GC *) xv_get(screen, SCREEN_OLGC_LIST, window);
	XChangeGC(display, gc_list[SCREEN_NONSTD_GC], gc_value_mask, &gc_values);
	XSetClipRectangles(display, gc_list[SCREEN_NONSTD_GC],
			0, 0, clip_xrects->rect_array, clip_xrects->count, Unsorted);
	XFillRectangle(display, xv_xid(info),
			gc_list[SCREEN_NONSTD_GC],
			rl->rl_bound.r_left, rl->rl_bound.r_top,
			(unsigned)rl->rl_bound.r_width, (unsigned)rl->rl_bound.r_height);
	XSetClipMask(display, gc_list[SCREEN_NONSTD_GC], None);
}

/*
 * tell the client to repaint the paint window.
 */
static void canvas_inform_repaint(Canvas_info *canvas, Xv_Window paint_window)
{
	Rectlist *win_damage, damage;

	if (!(win_damage = win_get_damage(paint_window))) {
		win_damage = &rl_null;
	}
	damage = rl_null;
	rl_copy(win_damage, &damage);

	if (xv_get(CANVAS_PUBLIC(canvas), OPENWIN_AUTO_CLEAR)) {
		canvas_clear_damage(paint_window, &damage);
	}
	if (canvas->repaint_proc) {
		if (status(canvas, x_canvas)) {
			Xv_xrectlist xrects;

			/*
			 * If there is no damage on the paint window, pass NULL
			 * xrectangle array and a count of zero to let the application
			 * know that there is no clipping.
			 */
			if (win_damage == &rl_null) {
				(*canvas->repaint_proc) (CANVAS_PUBLIC(canvas), paint_window,
						XV_DISPLAY_FROM_WINDOW(paint_window),
						xv_get(paint_window, XV_XID), NULL);
			}
			else {
				xrects.count = win_convert_to_x_rectlist(&damage,
						xrects.rect_array, XV_MAX_XRECTS);
				(*canvas->repaint_proc) (CANVAS_PUBLIC(canvas), paint_window,
						XV_DISPLAY_FROM_WINDOW(paint_window),
						xv_get(paint_window, XV_XID), &xrects);
			}
		}
		else {
			(*canvas->old_repaint_proc) (CANVAS_PUBLIC(canvas), paint_window,
					&damage);
		}
	}
	rl_free(&damage);
}

static void end_panning(Canvas_info *priv, Xv_window pw, Event *ev)
{
	Display *dpy = (Display *)xv_get(pw, XV_DISPLAY);
/* 	int x, y, vs, unit; */

	status_reset(priv, panning);
	XUngrabPointer(dpy, event_xevent(ev)->xbutton.time);

#ifdef NOT_YET
	x = (int)event_x(ev);
	y = (int)event_y(ev);

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

		DTRACE(DTL_EV, "end pan: VS=%d\n",
				xv_get(priv->panvsb, SCROLLBAR_VIEW_START));
	}
#endif /* NOT_YET */
	canvas_inform_repaint(priv, pw);
}

/*
 * handle events posted to the view window.
 */

/* ARGSUSED */
static Notify_value canvas_view_event(Canvas_view view_public,
				Notify_event event, Notify_arg arg, Notify_event_type type)
{
	Event *ev = (Event *)event;
	Canvas_view_info *view = CANVAS_VIEW_PRIVATE(view_public);
	Canvas_info *canvas = view->private_canvas;
	Xv_Window paint_window = view->paint_window;
	Notify_value result;
	Rect paint_rect;

	if (status(canvas, panning)) {
		switch (event_action(ev)) {
			case ACTION_SELECT:
				if (event_is_up(ev)) {
					end_view_panning(canvas, view, ev);
					return NOTIFY_DONE;
				}
				break;
			case LOC_DRAG:
				if (action_select_is_down(ev)) {
					update_view_panning(canvas, view, ev);
					return NOTIFY_DONE;
				}
			default:
				break;
		}
	}

	result = notify_next_event_func(view_public, event, arg, type);

	switch (event_id(ev)) {
		case WIN_RESIZE:
			paint_rect = *(Rect *) xv_get(paint_window, WIN_RECT);
			canvas_resize_paint_window(canvas, paint_rect.r_width,
					paint_rect.r_height);
			break;
		case SCROLLBAR_REQUEST:
			canvas_scroll(paint_window, (Scrollbar) arg);
			break;

#ifdef ACTION_SPLIT_DESTROY_PREVIEW_CANCEL
		case ACTION_SPLIT_DESTROY_PREVIEW_CANCEL:
			canvas_inform_repaint(canvas, paint_window);
			break;
#endif /* ACTION_SPLIT_DESTROY_PREVIEW_CANCEL */

		default:
			break;
	}

	return (result);
}

static void canvas_inform_resize(Canvas_info *canvas)
{
    Canvas          canvas_public = CANVAS_PUBLIC(canvas);

    if (!canvas->resize_proc) {
	return;
    }
    (*canvas->resize_proc) (canvas_public, canvas->width, canvas->height);
}

/*
 * Handle events for the paint window.  These events are passed on to the
 * canvas client CANVAS_EVENT_PROC.
 */

/* Save some memory space here, since these variables are never
 * used simulataneously.
 */
#define next_pw	    next_view
#define nth_pw	    nth_view
#define previous_pw previous_view
#define pw_nbr	    view_nbr


/* ARGSUSED */
static Notify_value canvas_paint_event(Xv_Window pw, Notify_event event,
									Notify_arg arg, Notify_event_type type)
{
    Event *ev = (Event *)event;
	Canvas_info *canvas;
	Canvas canv_p;
	char *help_data;
	Xv_Window next_view;
	Xv_Window nth_view;
	Xv_Window previous_view;
	Notify_value result;
	Scrollbar sb;
	Xv_Window view;
	int view_nbr;

#ifdef OW_I18N
	XIC ic;
	XID xid;
	XPointer client_data;
	Xv_object paint_public;
#endif /*OW_I18N */

	canvas = (Canvas_info *) xv_get(pw, XV_KEY_DATA, canvas_context_key);
	canv_p = CANVAS_PUBLIC(canvas);

	switch (event_action(ev)) {
		case ACTION_SELECT:
			if (event_is_down(ev)) {
				if (xv_get(XV_SERVER_FROM_WINDOW(pw),SERVER_EVENT_HAS_PAN_MODIFIERS,ev)) {
					start_panning(canvas, pw, ev);
					return NOTIFY_DONE;
				}
			}
			else if (status(canvas, panning)) {
				end_panning(canvas, pw, ev);
				return NOTIFY_DONE;
			}
			break;
		case LOC_DRAG:
			if (action_select_is_down(ev) && status(canvas, panning)) {
				update_panning(canvas, pw, ev);
				return NOTIFY_DONE;
			}
		default:
			break;
	}

	result = notify_next_event_func(pw, event, arg, type);

#ifdef OW_I18N
	ic = (XIC) xv_get(pw, WIN_IC);
	if (ic && (XGetICValues(ic, XNFocusWindow, &xid, NULL) == NULL)
			&& xid) {
		paint_public = (Canvas) win_data(XDisplayOfIM(XIMOfIC(ic)), xid);
		canvas = (Canvas_info *) xv_get(paint_public, XV_KEY_DATA,
				canvas_context_key);
		canv_p = CANVAS_PUBLIC(canvas);
	}
	else {
		canvas = (Canvas_info *) xv_get(pw, XV_KEY_DATA, canvas_context_key);
/* ISSUE: client_data is not initialized */
		canv_p = (Canvas) client_data;
	}
#endif /*OW_I18N */

	switch (event_action(ev)) {
		case WIN_REPAINT:
		case WIN_GRAPHICS_EXPOSE:
			canvas_inform_repaint(canvas, pw);
			break;

		case WIN_RESIZE:
			/* scrollbars have already been updated */
			/* tell the client the paint window changed size */
			canvas_inform_resize(canvas);
			break;

		case ACTION_HELP:
		case ACTION_MORE_HELP:
		case ACTION_TEXT_HELP:
		case ACTION_MORE_TEXT_HELP:
		case ACTION_INPUT_FOCUS_HELP:

#ifdef OW_I18N1
			if (event_is_down(ev)) {
				if ((Attr_pkg) xv_get(pw, WIN_TYPE) == CANVAS_TYPE) {
					help_data = (char *)xv_get(pw, XV_HELP_DATA);
					if (help_data)
						xv_help_show(pw, help_data, ev);
				}
			}
			break;

#else
			if (event_is_down(ev)) {
				if ((Attr_pkg) xv_get(canv_p, WIN_TYPE) == CANVAS_TYPE) {
					help_data = (char *)xv_get(canv_p, XV_HELP_DATA);
					if (help_data)
						xv_help_show(pw, help_data, ev);
				}
			}
			break;
#endif /* OW_I18N */

		case ACTION_NEXT_PANE:
			if (event_is_down(ev)) {
				for (pw_nbr = 0;
						(nth_pw = xv_get(canv_p, CANVAS_NTH_PAINT_WINDOW,
										pw_nbr)); pw_nbr++) {
					if (nth_pw == pw)
						break;
				}
				next_pw = xv_get(canv_p, CANVAS_NTH_PAINT_WINDOW, pw_nbr + 1);
				if (next_pw) {
					/* Set focus to first element in next paint window */
					xv_set(next_pw, WIN_SET_FOCUS, NULL);
					xv_set(canv_p, XV_FOCUS_ELEMENT, 0, NULL);
				}
				else
					xv_set(xv_get(canv_p, WIN_FRAME), FRAME_NEXT_PANE, NULL);
			}
			break;

		case ACTION_PREVIOUS_PANE:
			if (event_is_down(ev)) {
				for (pw_nbr = 0;
						(nth_pw = xv_get(canv_p, CANVAS_NTH_PAINT_WINDOW,
										pw_nbr)); pw_nbr++) {
					if (nth_pw == pw)
						break;
					previous_pw = nth_pw;
				}
				if (pw_nbr > 0) {
					/* Set focus to last element in previous paint window */
					xv_set(previous_pw, WIN_SET_FOCUS, NULL);
					xv_set(canv_p, XV_FOCUS_ELEMENT, -1, NULL);
				}
				else {
					xv_set(xv_get(canv_p, WIN_FRAME), FRAME_PREVIOUS_PANE, NULL);
				}
			}
			break;

		case ACTION_PREVIOUS_ELEMENT:
			/* Order of precedence:
			 *  previous paint window's horizontal scrollbar
			 *  previous paint window's vertical scrollbar
			 *  last element in previous frame subwindow
			 */
			if (event_is_down(ev)) {
				view = CANVAS_VIEW_PUBLIC((Canvas_view_info *)
						xv_get(pw, XV_KEY_DATA, canvas_view_context_key));
				for (view_nbr = 0;
						(nth_view =
								xv_get(canv_p, OPENWIN_NTH_VIEW,
										view_nbr)); view_nbr++) {
					if (nth_view == view)
						break;
					previous_view = nth_view;
				}
				if (view_nbr > 0) {
					sb = xv_get(canv_p,
							OPENWIN_HORIZONTAL_SCROLLBAR, previous_view);
					if (!sb)
						sb = xv_get(canv_p,
								OPENWIN_VERTICAL_SCROLLBAR, previous_view);
					xv_set(sb, WIN_SET_FOCUS, NULL);
				}
				else {
					/* Go to last element in previous frame subwindow */
					xv_set(xv_get(canv_p, WIN_FRAME),
							FRAME_PREVIOUS_ELEMENT, NULL);
				}
			}
			break;

		case ACTION_NEXT_ELEMENT:
			/* Order of precedence:
			 *  paint window's vertical scrollbar
			 *  paint window's horizontal scrollbar
			 *  next frame subwindow
			 */
			if (event_is_down(ev)) {
				view = CANVAS_VIEW_PUBLIC((Canvas_view_info *)
						xv_get(pw, XV_KEY_DATA, canvas_view_context_key));
				sb = xv_get(canv_p, OPENWIN_VERTICAL_SCROLLBAR, view);
				if (!sb)
					sb = xv_get(canv_p, OPENWIN_HORIZONTAL_SCROLLBAR, view);
				if (sb) {
					xv_set(sb, WIN_SET_FOCUS, NULL);
					break;
				}
				/* There is no scrollbar attached: go to next pane */
				xv_set(xv_get(canv_p, WIN_FRAME), FRAME_NEXT_PANE, NULL);
			}
			break;

		case ACTION_VERTICAL_SCROLLBAR_MENU:
		case ACTION_HORIZONTAL_SCROLLBAR_MENU:
			view = CANVAS_VIEW_PUBLIC((Canvas_view_info *) xv_get(pw,
							XV_KEY_DATA, canvas_view_context_key));
			if (event_action(ev) == ACTION_VERTICAL_SCROLLBAR_MENU)
				sb = xv_get(canv_p, OPENWIN_VERTICAL_SCROLLBAR, view);
			else
				sb = xv_get(canv_p, OPENWIN_HORIZONTAL_SCROLLBAR, view);
			if (sb) {
				Event sb_event;

				event_init(&sb_event);
				event_set_action(&sb_event, ACTION_MENU);
				event_set_window(&sb_event, sb);
				sb_event.ie_flags = ev->ie_flags;	/* set up/down flag */
				win_post_event(sb, &sb_event, NOTIFY_SAFE);
			}
			break;

		case ACTION_JUMP_MOUSE_TO_INPUT_FOCUS:
			view = CANVAS_VIEW_PUBLIC((Canvas_view_info *) xv_get(pw,
							XV_KEY_DATA, canvas_view_context_key));
			xv_set(view, WIN_MOUSE_XY, 0, 0, NULL);
			/* BUG ALERT:  Clicking MENU at this point does not send ACTION_MENU
			 *         to the canvas paint window.  Instead, an Window Manager
			 *         Window menu is brought up.
			 */
			break;

		case ACTION_WHEEL_FORWARD:
			if (event_is_down(ev)) {
				view = xv_get(pw, XV_OWNER);
				if ((sb = xv_get(canv_p, OPENWIN_VERTICAL_SCROLLBAR, view))) {
					int newpos = (int)xv_get(sb, SCROLLBAR_VIEW_START);

					if (newpos > 0) {
						xv_set(sb, SCROLLBAR_VIEW_START, newpos - 1, NULL);
					}
				}
			}
			break;

		case ACTION_WHEEL_BACKWARD:
			if (event_is_down(ev)) {
				view = xv_get(pw, XV_OWNER);
				if ((sb = xv_get(canv_p, OPENWIN_VERTICAL_SCROLLBAR, view))) {
					int newpos = (int)xv_get(sb, SCROLLBAR_VIEW_START);

					xv_set(sb, SCROLLBAR_VIEW_START, newpos + 1, NULL);
				}
			}
			break;

#ifdef OW_I18N
		case KBD_USE:{
				if (canvas->ic) {
					if (canvas->focus_pwin != pw) {
						/* 
						 * Set XNFocusWindow and cache the value.
						 */
						window_set_ic_focus_win(pw, canvas->ic,
								xv_get(pw, XV_XID));
						canvas->focus_pwin = pw;
					}

					/*
					 * Update the preedit display.
					 */
					panel_preedit_display(
							((Xv_panel_or_item *) canvas->pew->ptxt)->
							private_data, canvas->pe_cache, TRUE);
				}
				(void)frame_kbd_use(xv_get(canv_p, WIN_FRAME), canv_p, canv_p);
				break;
			}

		case KBD_DONE:{
				Xv_panel_or_item *pi;

				if (canvas->ic) {
					pi = (Xv_panel_or_item *) canvas->pew->ptxt;
					canvas->pe_cache = panel_get_preedit(pi->private_data);
				}
				(void)frame_kbd_done(xv_get(canv_p, WIN_FRAME), canv_p);
				break;
			}
#else
		case KBD_USE:
			(void)frame_kbd_use(xv_get(canv_p, WIN_FRAME), canv_p, canv_p);
			break;

		case KBD_DONE:
			(void)frame_kbd_done(xv_get(canv_p, WIN_FRAME), canv_p);
			break;
#endif /*OW_I18N */

		default:
			break;
	}

	return (result);
}

#ifdef OW_I18N
/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL_NOTICE 
 *	file for terms of the license.
 */

/* #include <X11/Xatom.h> */
/* #include <xview_private/cnvs_impl.h> */
/* #include <xview_private/draw_impl.h> */
/* #include <xview/frame.h> */
/* #include <xview/panel.h> */
/* #include <xview/font.h> */
#include <xview/notice.h>

#define MAX_PREEDIT_CHAR         512	/* characters */
#define PANEL_TEXT_RIM             6	/* pixel */
#define INPUT_WINDOW_WIDTH        50	/* columns */


Xv_private void			cache_text_state();

static Notify_value	canvas_pew_pi_event_proc();


static void canvas_preedit_start(XIC ic, XPointer	client_data, XPointer	callback_data)
{
    Canvas_info	*canvas;
    Canvas_pew	*pew;
    

    canvas = CANVAS_PRIVATE((Canvas) client_data);
    pew = canvas->pew;

    /*
     * Display the pew (PreEdit display Window).  And update the
     * status and counters.
     */
    xv_set(pew->frame, XV_SHOW, TRUE, NULL);
    pew->active_count++;
    status_set(canvas, preedit_exist);
    
    /*
     * start preediting with the input panel
     */
    panel_text_start(ic, pew->panel, NULL);
}


static void canvas_preedit_draw(ic, client_data, callback_data)
    XIC		ic;
    XPointer	client_data;
    XPointer	callback_data;
{
    Canvas_info 			*canvas;
    XIMPreeditDrawCallbackStruct 	*preedit;

    canvas = CANVAS_PRIVATE((Canvas) client_data);
    preedit = (XIMPreeditDrawCallbackStruct *) callback_data;

    /*
     * Can't use PANEL_PRIVATE macro here, can't include panel_impl.h
     * due to name clash between status element of panel_info and
     * canvas's status macro.
     */
    panel_preedit_display(
	((Xv_panel_or_item *) canvas->pew->ptxt)->private_data,
	preedit, FALSE);
}


Xv_private void 
cache_text_state(pre_edit, pe_cache)
XIMPreeditDrawCallbackStruct *pre_edit, *pe_cache;
{

    /*
     * length = length of updated text
     * chg_first = position in the cache where changes begin 
     * chg_length = number of chars in the cache effected
     * chg_last = last position in cache effected
     * tlength = length of new text
     *
     * we want to take all text from pe_cache up to chg_first, 
     * followed by the changed text, followed by any text
     * after chg_last in pe_cache and assemble them inorder in a buffer,
     * then replace the cache with the buffer.
     *
     * Whether the preedit text changes come in multibyte or
     * wide character form, this function will always return
     * the result in wide characters.  Since XView process
     * only wide characters internally so we are assuming
     * the cached preedit text is ALWAYS in wide characters.
     */

    XIMFeedback *feedback;	/* new structure for preedit screen feedback,
					will replace existing cache */
    int 	 i, idx;	/* Indices */
    wchar_t 	*text;		/* new structure for preedit data, will
					replace existing cache */
    unsigned short length = 0;
    unsigned short length_after = 0;
    int 	 tlength = 0;
    int 	 chg_first = pre_edit->chg_first;
    int 	 chg_length = pre_edit->chg_length;
    int 	 chg_last = chg_first + chg_length;

/*  For insertion or change case */

    if (pre_edit->text && (pre_edit->text->length > 0 )) {
	tlength = (int) pre_edit->text->length;
	length_after = pe_cache->text->length -  chg_last;
	length = chg_first + tlength + length_after;

	feedback = (XIMFeedback *) xv_alloc_n(XIMFeedback, length);
	text = (wchar_t *) xv_alloc_n(wchar_t, length+1);

	/* First copy the cached string up to changed area */
	for (i = 0; i < chg_first; i++) {
	    feedback[i] = pe_cache->text->feedback[i];
	    text[i] = pe_cache->text->string.wide_char[i];
	}
		    
	if (pre_edit->text->encoding_is_wchar) {
	    /* Now copy the new text */
	    for (idx = 0; idx < tlength; idx++) {
		feedback[i] = pre_edit->text->feedback[idx];
		text[i++] = pre_edit->text->string.wide_char[idx];
	    }
	} else {
	    wchar_t *tmp_str_wc;
	    /*  Convert the incoming preedit text from
	     *  multibyte to wide character.
	     */
		   
	    tmp_str_wc = _xv_mbstowcsdup (pre_edit->text->string.multi_byte);
	    for (idx = 0; idx < tlength; idx++) {
		feedback[i] = pre_edit->text->feedback[idx];
		text[i++] = tmp_str_wc[idx];
	    }
	    xv_free(tmp_str_wc);
	}
	/* finally, copy any cached text after the changed region */
	for (idx = chg_last; idx < (int) pe_cache->text->length; idx++) {
	    feedback[i] = pe_cache->text->feedback[idx];
	    text[i++] = pe_cache->text->string.wide_char[idx];
	}

	/* null-terminate the text array */
	text[length] = NULL;

	pe_cache->caret = pre_edit->caret;
	pe_cache->chg_first = 0;
	pe_cache->text->length = (unsigned short) length;
	pe_cache->chg_length = pe_cache->text->length;
	pe_cache->text->encoding_is_wchar = 1;
	if (pe_cache->text->string.wide_char)
	    xv_free(pe_cache->text->string.wide_char);
	pe_cache->text->string.wide_char = text;
	if (pe_cache->text->feedback)
	    xv_free(pe_cache->text->feedback);
	pe_cache->text->feedback = feedback;
    } else {
    /*  For the case of deletion or no change */
	if (chg_length > 0) {
	/* First copy the cached string up to changed area */
	    length = (pe_cache->chg_length >= chg_length) ? 
			pe_cache->chg_length - chg_length : 0;

	    feedback = (XIMFeedback *) xv_alloc_n(XIMFeedback, 
		length ? length : 1);
	    text = (wchar_t *) xv_alloc_n(wchar_t, length+1);
	    for (i = 0; i < chg_first; i++) {
		feedback[i] = pe_cache->text->feedback[i];
		text[i] = pe_cache->text->string.wide_char[i];
	    }
	    /* finally, copy any cached text after the changed region */
	    for (idx = chg_last; idx < (int) pe_cache->text->length; idx++) {
		feedback[i] = pe_cache->text->feedback[idx];
		text[i++] = pe_cache->text->string.wide_char[idx];
	    }

	    /* null-terminate the text array */

	    text[length] = NULL,

	    pe_cache->caret = pre_edit->caret;
	    pe_cache->chg_first = 0;
	    pe_cache->text->length = length;
	    pe_cache->chg_length = pe_cache->text->length;
	    pe_cache->text->encoding_is_wchar = 1;
	    if (pe_cache->text->string.wide_char)
		xv_free(pe_cache->text->string.wide_char);
	    pe_cache->text->string.wide_char = text;
	    if (pe_cache->text->feedback)
		xv_free(pe_cache->text->feedback);
	    pe_cache->text->feedback = feedback;
	}
    }
}


static void canvas_preedit_done(XIC ic, XPointer	client_data, XPointer	callback_data)

{
    Canvas_info		*canvas;
    Canvas_pew		*pew;

	
    canvas = CANVAS_PRIVATE((Canvas) client_data);
    pew = canvas->pew;
    if ((--canvas->pew->active_count) <= 0) {
	/*
	 * No other canvas's preedit is active, let's take down the
	 * preedit window if pin is out.
	 */
	if (xv_get(pew->frame, FRAME_CMD_PIN_STATE) == FRAME_CMD_PIN_OUT) {
	    xv_set(pew->frame, XV_SHOW, FALSE, NULL);
	}
    }
    status_reset(canvas, preedit_exist);

    panel_text_done(ic, pew->panel, NULL);
}


/* The following function is a stub. This will be implemented later */
static void canvas_preedit_caret(XIC ic, XPointer	client_data, XPointer	callback_data)
{
    /*
    interprete direction and calculate row and col;
    set cursor position
    */
}


/*
 * canvas_pew_done: Make sure, we will not unmap the pew while there
 * are still active preedit session.
 */
static void canvas_pew_done(Frame	frame) /* frame for preedit window */
{
    Canvas_pew		*pew;

    pew = (Canvas_pew *) xv_get(xv_get(frame, XV_OWNER),
				XV_KEY_DATA, canvas_pew_key);
    if (pew->active_count > 0) {

        notice_prompt(pew->frame, NULL,
		NOTICE_MESSAGE_STRING,	XV_MSG("\
The Preedit Display popup window cannot be\n\
dismissed while input method conversion is\n\
still on in one of the canvas windows."),
		NOTICE_BUTTON_YES,	XV_MSG("Continue"),
		NULL);
    } else
	xv_set(pew->frame, XV_SHOW, FALSE, NULL);
}

/*
 * canvas_pew_destory: Catch the destory of the preedit window, make
 * sure canvas will no longer reference to it.
 */
static Notify_value canvas_pew_destory(Notify_client client,
							Destroy_status	status)
{
    if (status == DESTROY_CLEANUP) {
	Frame		 frame_public;
	Canvas_pew	*pew;

	frame_public =  (Frame) xv_get(client, XV_OWNER);
	if ((pew = (Canvas_pew *) xv_get(frame_public, XV_KEY_DATA,
						canvas_pew_key)) != NULL) {
	    xv_set(frame_public,
		   XV_KEY_DATA,	canvas_pew_key, NULL,
		   NULL);
	    xv_free(pew);
	}
    }
    return notify_next_destroy_func(client, status);
}

/*
 * canvas_create_pew: Create the PreEdit Window and register itself to
 * the parent frame as XV_KEY_DATA.
 */
static Canvas_pew *canvas_create_pew(Frame frame)
{
    Canvas_pew	     *pew;
    Xv_font	      font;
#ifndef PEW_DOES_NOTICE_ON_KBD_USE
    Xv_Drawable_info *info;
    Atom	      prop_array[1];
#endif


    pew = xv_alloc(Canvas_pew);
    font = (Xv_Font) xv_get(frame, XV_FONT);

    pew->frame = (Frame) xv_create(frame, FRAME_CMD,
            XV_LABEL,                   XV_MSG("Preedit Display"),
            WIN_USE_IM,                 FALSE,
            FRAME_CMD_POINTER_WARP,     FALSE,
            FRAME_SHOW_RESIZE_CORNER,   TRUE,
            FRAME_DONE_PROC,            canvas_pew_done,
	    WIN_WIDTH,          	INPUT_WINDOW_WIDTH,
            NULL);

#ifndef PEW_DOES_NOTICE_ON_KBD_USE
    /*
     * Following code can eliminate to have focus event completely.
     */
    DRAWABLE_INFO_MACRO(pew->frame, info);
    prop_array[0] = (Atom) xv_get(xv_server(info), SERVER_WM_DELETE_WINDOW);
    win_change_property(pew->frame, SERVER_WM_PROTOCOLS, XA_ATOM, 32,
							prop_array, 1);
#endif /* PEW_DOES_NOTICE_ON_KBD_USE */

    pew->panel = xv_get(pew->frame, FRAME_CMD_PANEL);

    xv_set(pew->panel,
        WIN_ROWS,           	1,
        WIN_COLUMNS,          	INPUT_WINDOW_WIDTH,
        WIN_IGNORE_X_EVENT_MASK,(KeyPress|KeyRelease
				 |ButtonPress|ButtonRelease),
        XV_FONT,    		font,
        NULL);
 
    notify_interpose_event_func(pew->panel, canvas_pew_pi_event_proc,
					   NOTIFY_SAFE);
    notify_interpose_destroy_func(pew->frame, canvas_pew_destory);
 
    pew->ptxt = (Panel_item) xv_create(pew->panel, PANEL_TEXT,
            PANEL_VALUE_DISPLAY_WIDTH,    INPUT_WINDOW_WIDTH - PANEL_TEXT_RIM,
            PANEL_VALUE_STORED_LENGTH_WCS,MAX_PREEDIT_CHAR,
            				  PANEL_VALUE_FONT, font,
            NULL);
 
    xv_set(frame,
        XV_KEY_DATA,		canvas_pew_key,	pew,
	NULL);

    xv_set(pew->frame,
	WIN_FIT_HEIGHT,		0,
	WIN_FIT_WIDTH,		0,
	NULL);

    return pew;
}



/*
 * canvas_pew_event_proc((Window)pew_win, (Event)event, arg) handles
 * the resizing of the canvas pew.  It changes the display length of
 * the text item.
 */
static Notify_value canvas_pew_pi_event_proc(Notify_client	panel, Event *event,
						Notify_arg arg, Notify_event_type	type)
{
    Notify_value	ret;
    Panel_item		ip;
    int			width;
	

    switch ((Notify_event) event_action(event)) {
      case WIN_RESIZE:
      case WIN_REPAINT:
	ret = notify_next_event_func(panel, (Notify_event) event, arg, type);

	ip = (Panel_item) xv_get(panel, PANEL_FIRST_ITEM);

	width = (int) xv_get(panel, WIN_WIDTH) - PANEL_TEXT_RIM;
	xv_set(ip, PANEL_VALUE_DISPLAY_WIDTH, width, NULL);

	return ret;

#ifdef PEW_DOES_NOTICE_ON_KBD_USE
      /*
       * Bringup notice is desirable behavior, however this can not
       * done with current notice_prompt or notice pkg.  Because once
       * user hit "OK" button, notice will give input focus back to
       * the pew, and pew will see the KBD_USE event again, again, and
       * again....
       */
      case KBD_USE:

	notice_prompt(xv_get(panel, XV_OWNER), NULL,
			NOTICE_MESSAGE_STRING,	XV_MSG("\
You can not type in to the Preedit Display popup window,\n\
this is display only window, you should type in to the\n\
canvas window itself instead."),
                        NOTICE_BUTTON_YES, XV_MSG("OK"),
                        0);
	/*FALLTHROUGH*/

      case KBD_DONE:
	return NOTIFY_DONE;
#endif /* PEW_DOES_NOTICE_ON_KBD_USE */
    }

    return notify_next_event_func(panel, (Notify_event) event, arg, type);
}


/*
 * canvas_pew_event_proc: event interpose proc for the pew (in case of
 * the parent is FRAME_CMD).  Sync up pew with frame_cmd in case for
 * the open/close event.
 */
static Notify_value canvas_pew_event_proc(Window parent_win, Event *event,
						Notify_arg arg, Notify_event_type	type)
{
    Canvas_pew		*pew;

    switch ((Notify_event) event_action(event)) {
      case ACTION_OPEN:
      case ACTION_CLOSE:
	pew = (Canvas_pew *) xv_get(parent_win, XV_KEY_DATA, canvas_pew_key);

	if ((Notify_event) event_action(event) == ACTION_CLOSE) {
	    xv_set(pew->frame, XV_SHOW, FALSE, NULL);

	} else {
	    if (pew->active_count > 0
	     || xv_get(pew->frame, FRAME_CMD_PIN_STATE) == FRAME_CMD_PIN_IN)
		xv_set(pew->frame, XV_SHOW, TRUE, NULL);
	}
	break;
    }
    return notify_next_event_func((Notify_client) parent_win,
				  (Notify_event) event, arg, type);
}

#endif /*OW_I18N*/
/*
 * translate a canvas paint window-space event to a canvas subwindow-space
 * event.
 */
Xv_private Event * canvas_window_event(Canvas canvas_public, Event *event)
{
	Xv_Window paint_window;
	static Event tmp_event;
	int x, y;

	paint_window = xv_get(canvas_public, CANVAS_NTH_PAINT_WINDOW, 0);
	if (paint_window == XV_NULL) {
		/* call xv_error */
		return (event);
	}
	tmp_event = *event;
	win_translate_xy(paint_window, canvas_public,
			event_x(event), event_y(event), &x, &y);
	event_set_x(&tmp_event, x);
	event_set_y(&tmp_event, y);
	return (&tmp_event);
}

/*
 * translate a window-space event to a canvas-space event.
 */
Xv_private Event *canvas_event(Canvas canvas_public, Event *event)
{
	Xv_Window pw;
	static Event tmp_event;
	int x, y;

	pw = xv_get(canvas_public, CANVAS_NTH_PAINT_WINDOW, 0);
	if (pw == XV_NULL) {
		/* call xv_error */
		return (event);
	}
	tmp_event = *event;
	win_translate_xy(pw, canvas_public, event_x(event), event_y(event), &x, &y);
	event_set_x(&tmp_event, x);
	event_set_y(&tmp_event, y);
	return (&tmp_event);
}

/* ARGSUSED */
static int canvas_init(Xv_Window parent, Canvas self, Attr_attribute avlist[],
							int *u)
{
    Xv_canvas          *canvas_object = (Xv_canvas *) self;
    Canvas_info        *canvas;

#ifdef OW_I18N
    Frame	        frame_public;
    const Xv_pkg              *frame_type;
#endif /*OW_I18N*/

    if (canvas_context_key == (Attr_attribute) 0) {
		canvas_context_key = xv_unique_key();
#ifdef OW_I18N
		canvas_pew_key = xv_unique_key();
#endif /*OW_I18N*/
    }
    canvas = xv_alloc(Canvas_info);

    /* link to object */
    canvas_object->private_data = (Xv_opaque) canvas;
    canvas->public_self = self;

    status_set(canvas, fixed_image);
    status_set(canvas, auto_expand);
    status_set(canvas, auto_shrink);
    status_set(canvas, retained);

	canvas->pan_cursor = xv_create(self, CURSOR,
						CURSOR_SRC_CHAR, OLC_PANNING_PTR,
						CURSOR_MASK_CHAR, OLC_PANNING_PTR+1,
						NULL);

    /*
     * 1. Make all the paint windows inherit the WIN_DYNAMIC_VISUAL attribute.
     * 2. The Canvas is, by default, a First-Class (primary) focus client.
     */
    xv_set(self,
        WIN_INHERIT_COLORS, TRUE,
        XV_FOCUS_RANK, XV_FOCUS_PRIMARY,
#ifdef OW_I18N
	WIN_IM_PREEDIT_START,	canvas_preedit_start, self,
	WIN_IM_PREEDIT_DRAW,	canvas_preedit_draw, self,
	WIN_IM_PREEDIT_DONE,	canvas_preedit_done, self,
#endif
        NULL);

    return XV_OK;
}

static void canvas_set_bit_gravity(Canvas_info *canvas)
{
    Xv_Window       paint_window;
    int             bit_value;

    if (status(canvas, fixed_image)) {
	bit_value = NorthWestGravity;
    } else {
	bit_value = ForgetGravity;
    }

    CANVAS_EACH_PAINT_WINDOW(CANVAS_PUBLIC(canvas), paint_window)
	window_set_bit_gravity(paint_window, bit_value);
    CANVAS_END_EACH
}

static void canvas_append_paint_attrs(Canvas_info *canvas, Attr_avlist argv)
{
	if (canvas->paint_avlist == NULL) {
		canvas->paint_avlist = xv_alloc_n(Attr_attribute,
												(size_t)ATTR_STANDARD_SIZE);
		canvas->paint_end_avlist = canvas->paint_avlist;
	}
	canvas->paint_end_avlist =
			(Attr_avlist) attr_copy_avlist(canvas->paint_end_avlist, argv);
}

static Xv_opaque canvas_set_avlist(Canvas canvas_public, Attr_avlist avlist)
{
	Canvas_info *canvas = CANVAS_PRIVATE(canvas_public);
	Attr_attribute attr;
	int width = 0;
	int height = 0;
	int vsb_set = 0, hsb_set = 0;
	Scrollbar vsb = XV_NULL, hsb = XV_NULL;
	short new_paint_size = FALSE;
	short recheck_paint_size = FALSE;
	int ok = TRUE;
	Xv_Window paint_window;
	Rect pw_rect;


	for (attr = avlist[0]; attr; avlist = attr_next(avlist), attr = avlist[0]) {
		switch (attr) {
			case CANVAS_WIDTH:
				if (canvas->width != (int)avlist[1]) {
					width = (int)avlist[1];
					new_paint_size = TRUE;
				}
				ATTR_CONSUME(avlist[0]);
				break;

			case CANVAS_HEIGHT:
				if (canvas->height != (int)avlist[1]) {
					height = (int)avlist[1];
					new_paint_size = TRUE;
				}
				ATTR_CONSUME(avlist[0]);
				break;

			case CANVAS_MIN_PAINT_WIDTH:
				if (canvas->min_paint_width != (int)avlist[1]) {
					canvas->min_paint_width = (int)avlist[1];
					new_paint_size = TRUE;
				}
				ATTR_CONSUME(avlist[0]);
				break;

			case CANVAS_MIN_PAINT_HEIGHT:
				if (canvas->min_paint_height != (int)avlist[1]) {
					canvas->min_paint_height = (int)avlist[1];
					new_paint_size = TRUE;
				}
				ATTR_CONSUME(avlist[0]);
				break;

			case CANVAS_VIEW_MARGIN:
				/* This is a hold over from SunView, it is just a no-op for
				 * the time being
				 */
				break;

			case CANVAS_X_PAINT_WINDOW:
				if ((int)avlist[1] == status(canvas, x_canvas)) {
					break;
				}
				if (avlist[1]) {
					status_set(canvas, x_canvas);
				}
				else {
					status_reset(canvas, x_canvas);
				}
				CANVAS_EACH_PAINT_WINDOW(canvas_public, paint_window)
						xv_set(paint_window, WIN_X_PAINT_WINDOW, avlist[1],
						NULL);
				CANVAS_END_EACH
				ATTR_CONSUME(avlist[0]);
				break;

			case CANVAS_NO_CLIPPING:
				if ((int)avlist[1] == status(canvas, no_clipping)) {
					break;
				}
				if (avlist[1]) {
					status_set(canvas, no_clipping);
				}
				else {
					status_reset(canvas, no_clipping);
				}
				CANVAS_EACH_PAINT_WINDOW(canvas_public, paint_window)
						xv_set(paint_window, WIN_NO_CLIPPING, avlist[1], NULL);
				CANVAS_END_EACH
				ATTR_CONSUME(avlist[0]);
				break;

			case CANVAS_REPAINT_PROC:
				canvas->repaint_proc = (CanvasRepaintFunction_t) avlist[1];
				canvas->old_repaint_proc =
						(CanvasOldRepaintFunction_t) avlist[1];
				ATTR_CONSUME(avlist[0]);
				break;

			case CANVAS_RESIZE_PROC:
				canvas->resize_proc = (CanvasResizeFunction_t) avlist[1];
				ATTR_CONSUME(avlist[0]);
				break;

			case CANVAS_AUTO_EXPAND:
				if ((int)avlist[1] == status(canvas, auto_expand))
					break;
				if (avlist[1])
					status_set(canvas, auto_expand);
				else
					status_reset(canvas, auto_expand);
				recheck_paint_size = TRUE;
				ATTR_CONSUME(avlist[0]);
				break;

			case CANVAS_AUTO_SHRINK:
				if ((int)avlist[1] == status(canvas, auto_shrink))
					break;
				if (avlist[1])
					status_set(canvas, auto_shrink);
				else
					status_reset(canvas, auto_shrink);
				recheck_paint_size = TRUE;
				ATTR_CONSUME(avlist[0]);
				break;

			case CANVAS_RETAINED:
				if ((int)avlist[1] == status(canvas, retained)) {
					break;
				}
				if (avlist[1]) {
					status_set(canvas, retained);
				}
				else {
					status_reset(canvas, retained);
				}
				CANVAS_EACH_PAINT_WINDOW(canvas_public, paint_window)
					xv_set(paint_window, WIN_RETAINED, avlist[1], NULL);
				CANVAS_END_EACH
				ATTR_CONSUME(avlist[0]);
				break;

			case OPENWIN_SELECTABLE:
				if (avlist[1]) {
					status_reset(canvas, retained);
					CANVAS_EACH_PAINT_WINDOW(canvas_public, paint_window)
						xv_set(paint_window, WIN_RETAINED, avlist[1], NULL);
					CANVAS_END_EACH
				}
				break;

			case CANVAS_CMS_REPAINT:
				if (avlist[1]) {
					status_set(canvas, cms_repaint);
				}
				else {
					status_reset(canvas, cms_repaint);
				}
				ATTR_CONSUME(avlist[0]);
				break;

			case CANVAS_FIXED_IMAGE:
				/* don't do anything if no change */
				if (status(canvas, fixed_image) != (int)avlist[1]) {
					if ((int)avlist[1]) {
						status_set(canvas, fixed_image);
					}
					else {
						status_reset(canvas, fixed_image);
					}
					canvas_set_bit_gravity(canvas);
				}
				ATTR_CONSUME(avlist[0]);
				break;

			case WIN_VERTICAL_SCROLLBAR:
			case OPENWIN_VERTICAL_SCROLLBAR:
				vsb = (Scrollbar) avlist[1];
				vsb_set = TRUE;
				break;

			case WIN_HORIZONTAL_SCROLLBAR:
			case OPENWIN_HORIZONTAL_SCROLLBAR:
				hsb = (Scrollbar) avlist[1];
				hsb_set = TRUE;
				break;

			case CANVAS_PAINTWINDOW_ATTRS:
				if (status(canvas, created)) {
					CANVAS_EACH_PAINT_WINDOW(canvas_public, paint_window)
						xv_set_avlist(paint_window, &(avlist[1]));
					CANVAS_END_EACH
				}
				else {
					canvas_append_paint_attrs(canvas, &(avlist[1]));
				}
				ATTR_CONSUME(avlist[0]);
				break;

			case WIN_CMS_CHANGE:
				if (status(canvas, created)) {
					Xv_Drawable_info *info;
					Xv_Window view_public;
					Canvas_view_info *view;
					Cms cms;
					int cms_fg, cms_bg;

					DRAWABLE_INFO_MACRO(canvas_public, info);
					cms = xv_cms(info);
					cms_fg = xv_cms_fg(info);
					cms_bg = xv_cms_bg(info);
					OPENWIN_EACH_VIEW(canvas_public, view_public)
						view = CANVAS_VIEW_PRIVATE(view_public);
						window_set_cms(view_public, cms, cms_bg, cms_fg);
						window_set_cms(view->paint_window, cms, cms_bg,cms_fg);
					OPENWIN_END_EACH
				}
				break;

#ifdef OW_I18N
			case WIN_IC:
				canvas->ic = (XIC) avlist[1];
				break;
#endif /*OW_I18N */

			case XV_END_CREATE:

				/* adjust paint window here rather then view end */
				/* create because canvas_resize_paint_window */
				/* assumes view window is known to canvas */
				paint_window = xv_get(canvas_public,CANVAS_NTH_PAINT_WINDOW, 0);
				pw_rect = *(Rect *) xv_get(paint_window, WIN_RECT);
				canvas_resize_paint_window(canvas, pw_rect.r_width,
						pw_rect.r_height);

				if (status(canvas, no_clipping)) {
					CANVAS_EACH_PAINT_WINDOW(canvas_public, paint_window)
						xv_set(paint_window, WIN_NO_CLIPPING, TRUE, NULL);
					CANVAS_END_EACH
				}
				break;

			default:
				xv_check_bad_attr(&xv_canvas_pkg, attr);
				break;
		}
	}

	if (!status(canvas, created)) {
		/* copy width and height if set */
		if (width != 0) {
			canvas->width = width;
		}
		if (height != 0) {
			canvas->height = height;
		}

	}
	else {

		if (new_paint_size) {
			canvas_resize_paint_window(canvas, width, height);
		}
		else if (recheck_paint_size) {
			canvas_resize_paint_window(canvas, canvas->width, canvas->height);
		}
	}

	if (vsb_set) {
		canvas_set_scrollbar_object_length(canvas, SCROLLBAR_VERTICAL, vsb);
	}
	if (hsb_set) {
		canvas_set_scrollbar_object_length(canvas, SCROLLBAR_HORIZONTAL, hsb);
	}
	return (Xv_opaque) (ok ? XV_OK : XV_ERROR);
}

static Xv_opaque canvas_get_attr(Canvas canvas_public, int *stat,
					Attr_attribute attr, va_list valist)
{
	Canvas_info *canvas = CANVAS_PRIVATE(canvas_public);
	Canvas_view_info *view;
	Xv_Window view_public, paint_window;
	Rect view_rect, *canvas_rect;

	switch (attr) {
		case CANVAS_NTH_PAINT_WINDOW:
			view_public =
					(Xv_Window) xv_get(canvas_public, OPENWIN_NTH_VIEW,
					va_arg(valist, int));
			if (view_public != XV_NULL) {
				return ((Xv_opaque) CANVAS_VIEW_PRIVATE(view_public)->
						paint_window);
			}
			else {
				return XV_NULL;
			}
		case CANVAS_HEIGHT:
			return (Xv_opaque) canvas->height;

		case CANVAS_WIDTH:
			return (Xv_opaque) canvas->width;

		case CANVAS_REPAINT_PROC:
			return (Xv_opaque) canvas->repaint_proc;

		case CANVAS_RESIZE_PROC:
			return (Xv_opaque) canvas->resize_proc;

		case CANVAS_AUTO_EXPAND:
			return (Xv_opaque) status(canvas, auto_expand);

		case CANVAS_AUTO_SHRINK:
			return (Xv_opaque) status(canvas, auto_shrink);

		case CANVAS_RETAINED:
			return (Xv_opaque) status(canvas, retained);

		case CANVAS_CMS_REPAINT:
			return (Xv_opaque) status(canvas, cms_repaint);

		case CANVAS_FIXED_IMAGE:
			return (Xv_opaque) status(canvas, fixed_image);

		case CANVAS_NO_CLIPPING:
			return (Xv_opaque) status(canvas, no_clipping);

		case CANVAS_VIEWABLE_RECT:
			paint_window = va_arg(valist, Xv_Window);
			if (paint_window != XV_NULL) {
				view = CANVAS_VIEW_PRIVATE((Canvas_view) xv_get(paint_window,
								XV_OWNER));
				if (view == NULL) {
					return (Xv_opaque) NULL;
				}
				view_rect =
						*(Rect *) xv_get(CANVAS_VIEW_PUBLIC(view), WIN_RECT);
				canvas_rect = (Rect *) xv_get(paint_window, WIN_RECT);
				canvas_rect->r_left = -canvas_rect->r_left;
				canvas_rect->r_top = -canvas_rect->r_top;
				canvas_rect->r_width = view_rect.r_width;
				canvas_rect->r_height = view_rect.r_height;
				return (Xv_opaque) canvas_rect;
			}
			else {
				return (Xv_opaque) NULL;
			}

		case CANVAS_MIN_PAINT_WIDTH:
			return (Xv_opaque) canvas->min_paint_width;

		case CANVAS_MIN_PAINT_HEIGHT:
			return (Xv_opaque) canvas->min_paint_height;

#ifdef OW_I18N
		case CANVAS_IM_PREEDIT_FRAME:
			if (canvas->pew)
				return (Xv_opaque) canvas->pew->frame;
			else
				return (Xv_opaque) NULL;
#endif

		case WIN_TYPE:	/* SunView1.X compatibility */
			return (Xv_opaque) CANVAS_TYPE;

		case OPENWIN_VIEW_CLASS:
			return (Xv_opaque) CANVAS_VIEW;

		case OPENWIN_PW_CLASS:
			return (Xv_opaque) CANVAS_PAINT_WINDOW;

		default:
			xv_check_bad_attr(&xv_canvas_pkg, attr);
			*stat = XV_ERROR;
			return (Xv_opaque) 0;
	}
}

static int canvas_destroy(Canvas canvas_public, Destroy_status stat)
{
    Canvas_info    *canvas = CANVAS_PRIVATE(canvas_public);

    if (stat == DESTROY_CLEANUP) {
#ifdef OW_I18N
        /*
	 * All the canvases under one frame share the preedit window.
	 * Only when all the canvases have been destroyed, can we
	 * destroy the preedit window. So, we need to keep count of
	 * the canvases.
	 */
	Canvas_pew	   *pew;
	Frame		    frame_public;

	if (canvas->ic) {
	    /*
	     * Get the pew from frame to make sure, pew is still exist
	     * or not (when entire frame get destroy, pew may get
	     * destory first) instead of accessing the private data.
	     */
	    frame_public = (Frame) xv_get(canvas_public, WIN_FRAME);
	    pew = (Canvas_pew *) xv_get(frame_public,
					XV_KEY_DATA, canvas_pew_key);

	    if (pew != NULL) {
	        /*
		 * If the preedit window is still up and not pinned,
		 * make sure it is unmapped.
		 */
		if (status(canvas, preedit_exist)
	         && (--pew->active_count) <= 0) {
		    if (xv_get(pew->frame, FRAME_CMD_PIN_STATE)
						== FRAME_CMD_PIN_OUT) {
		        xv_set(pew->frame, XV_SHOW, FALSE, NULL);
		    }
		}

		/*
		 * If this is last canvas uses pew, and pew is not
		 * destoryed yet, let's destory it.
		 */
		if ((--pew->reference_count) <= 0) {
		    xv_destroy(pew->frame);
		    /*
		     * freeing pew itself and setting null to the pew
		     * will be done in the destroy interpose routine.
		     */
		}
	    }

	    /*
	     * Free the all preedit text cache information.
	     */
	    if (canvas->pe_cache) {
	        if (canvas->pe_cache->text->feedback) {
		    xv_free(canvas->pe_cache->text->feedback);
	        }
	        if (canvas->pe_cache->text->string.wide_char) {
		    xv_free(canvas->pe_cache->text->string.wide_char);
		}
		xv_free(canvas->pe_cache);
	    }
	}
#endif /*OW_I18N*/
	if (canvas->pan_cursor) xv_destroy(canvas->pan_cursor);
	xv_free((char *) canvas);
    }
    return XV_OK;
}

#ifdef  OW_I18N
#include <xview_private/draw_impl.h>
#include <xview/font.h>

extern wchar_t	_xv_null_string_wc[];

#endif /*OW_I18N*/

static int canvas_view_init(Canvas parent, Canvas_view view_public,
						Attr_attribute avlist[], int *u)
{
	Xv_canvas_view *view_object = (Xv_canvas_view *) view_public;
	Canvas_view_info *view;
	Canvas_info *priv = CANVAS_PRIVATE(parent);

	view = xv_alloc(Canvas_view_info);

	/* link to object */
	view_object->private_data = (Xv_opaque) view;
	view->public_self = view_public;
	view->private_canvas = CANVAS_PRIVATE(parent);

	if (priv->width == 0) {
		priv->width = (int)xv_get(view_public, WIN_WIDTH);
	}
	if (priv->height == 0) {
		priv->height = (int)xv_get(view_public, WIN_HEIGHT);
	}

	return XV_OK;
}

static Xv_opaque canvas_view_set(Canvas_view viewpub, Attr_avlist avlist)
{
	Attr_attribute attr;

	for (attr = avlist[0]; attr; avlist = attr_next(avlist), attr = avlist[0]) {
		switch (attr) {
			case XV_END_CREATE:
				xv_set(viewpub,
						WIN_NOTIFY_SAFE_EVENT_PROC, canvas_view_event,
						WIN_NOTIFY_IMMEDIATE_EVENT_PROC, canvas_view_event,
						WIN_CONSUME_PICK_EVENTS, WIN_RESIZE, NULL,
						NULL);
				break;
			default:
				xv_check_bad_attr(&xv_canvas_view_pkg, attr);
				break;
		}
	}
	return (Xv_opaque)XV_OK;
}

/*ARGSUSED*/ /*VARARGS*/
static Xv_opaque canvas_view_get(Canvas_view view_public, int *stat, Attr_attribute attr, va_list valist)
{
	Canvas_view_info *view = CANVAS_VIEW_PRIVATE(view_public);

	*stat = XV_OK;

	switch (attr) {
		case CANVAS_VIEW_PAINT_WINDOW:
			return ((Xv_opaque) view->paint_window);

		case CANVAS_VIEW_CANVAS_WINDOW:
			return ((Xv_opaque) CANVAS_PUBLIC(view->private_canvas));

#ifdef OW_I18N
		case WIN_IC:
			ATTR_CONSUME(attr);
			return (Xv_opaque) view->private_canvas->ic;
#endif /*OW_I18N */

		default:
			xv_check_bad_attr(&xv_canvas_view_pkg, attr);
			*stat = XV_ERROR;
			return (Xv_opaque) 0;
	}
}


/*ARGSUSED*/ /*VARARGS*/
static Xv_opaque canvas_paint_get(Canvas_paint_window paint_public, int *stat,
						Attr_attribute attr, va_list valist)
{
	Canvas_view_info *view;
	Canvas_info *canvas;

	switch (attr) {
		case CANVAS_PAINT_CANVAS_WINDOW:
			canvas = (Canvas_info *) xv_get(paint_public,
					XV_KEY_DATA, canvas_context_key);
			return (Xv_opaque) CANVAS_PUBLIC(canvas);

		case CANVAS_PAINT_VIEW_WINDOW:
			view = (Canvas_view_info *) xv_get(paint_public,
					XV_KEY_DATA, canvas_view_context_key);
			return (Xv_opaque) CANVAS_VIEW_PUBLIC(view);

#ifdef OW_I18N
		case WIN_IC:
			ATTR_CONSUME(attr);
			canvas = (Canvas_info *) xv_get(paint_public,
					XV_KEY_DATA, canvas_context_key);
			return (Xv_opaque) canvas->ic;
#endif /*OW_I18N */

		default:
			xv_check_bad_attr(&xv_canvas_pw_pkg, attr);
			*stat = XV_ERROR;
			return (Xv_opaque) 0;
	}
}

static int canvas_view_destroy(Canvas_view view_public, Destroy_status stat)
{
    Canvas_view_info *view = CANVAS_VIEW_PRIVATE(view_public);

    if ((stat == DESTROY_CLEANUP) || (stat == DESTROY_PROCESS_DEATH)) {
#ifdef OW_I18N
	Canvas_info		*canvas = view->private_canvas;
	Xv_Drawable_info	*info;


	/*
	 * Make sure that XNFocusWindow does not pointing to the
	 * window which being destroyed.
	 */
	if (canvas->ic) {
	    if (view->paint_window == canvas->focus_pwin) {
		/*
		 * Current XNFocusWindow is the one which we are
		 * destroying right now.  Try to find other paint
		 * window (if we are destroying the last view/paint,
		 * then we do not need worry about this).
		 */
	        Xv_Window	pw;

		CANVAS_EACH_PAINT_WINDOW(CANVAS_PUBLIC(canvas), pw)
		    if (pw != view->paint_window) {
			DRAWABLE_INFO_MACRO(pw, info);
			/* 
			 * Cache the XNFocusWindow whenever it is set 
			 */
			window_set_ic_focus_win(view_public, canvas->ic, 
						xv_xid(info));
			break;
		    }
		CANVAS_END_EACH
	    }
	}
#endif /* OW_I18N */
	if (xv_destroy_status(view->paint_window, stat) != XV_OK) {
	    return XV_ERROR;
	}
	if (stat == DESTROY_CLEANUP)
	    free((char *) view);
    }
    return XV_OK;
}


static int canvas_paint_init(Canvas_view view, Canvas_paint_window self,
						Attr_attribute avlist[], int *u)
{
	Canvas canvas = xv_get(view, XV_OWNER);
	Canvas_view_info *vp = CANVAS_VIEW_PRIVATE(view);
	Canvas_info *priv = CANVAS_PRIVATE(canvas);

	xv_set(self,
			XV_KEY_DATA, canvas_view_context_key, vp,
			XV_KEY_DATA, canvas_context_key, priv,
			NULL);

	vp->paint_window = self;

	if (priv->paint_avlist == NULL) {
		xv_set(self,
				WIN_WIDTH, priv->width,
				WIN_HEIGHT, priv->height,
				WIN_NOTIFY_SAFE_EVENT_PROC, canvas_paint_event,
				WIN_NOTIFY_IMMEDIATE_EVENT_PROC, canvas_paint_event,
				WIN_RETAINED, status(priv, retained),
				WIN_X_PAINT_WINDOW, status(priv, x_canvas),
				NULL);
	}
	else {
		xv_set(self,
				ATTR_LIST, priv->paint_avlist,
				WIN_WIDTH, priv->width,
				WIN_HEIGHT, priv->height,
				WIN_NOTIFY_SAFE_EVENT_PROC, canvas_paint_event,
				WIN_NOTIFY_IMMEDIATE_EVENT_PROC, canvas_paint_event,
				WIN_RETAINED, status(priv, retained),
				WIN_X_PAINT_WINDOW, status(priv, x_canvas),
				NULL);
		xv_free(priv->paint_avlist);
		priv->paint_avlist = priv->paint_end_avlist = NULL;
	}

	if (status(priv, created)) {
		Xv_Window split_paint;

		split_paint = xv_get(canvas, CANVAS_NTH_PAINT_WINDOW, 0);
		if (split_paint != XV_NULL) {
			Xv_opaque defaults_array[ATTR_STANDARD_SIZE];
			Attr_avlist defaults = defaults_array;
			Xv_opaque value;
			Scrollbar sb;

			/* inherit certain attributes from the split window */
			value = xv_get(split_paint, WIN_BACKGROUND_PIXMAP, 0);
			if (value) {
				*defaults++ = (Xv_opaque) WIN_BACKGROUND_PIXMAP;
				*defaults++ = xv_get(split_paint, WIN_BACKGROUND_PIXMAP, 0);
			}

			*defaults++ = (Xv_opaque) WIN_BIT_GRAVITY;
			*defaults++ = xv_get(split_paint, WIN_BIT_GRAVITY, 0);

			*defaults++ = (Xv_opaque) WIN_COLOR_INFO;
			*defaults++ = xv_get(split_paint, WIN_COLOR_INFO, 0);

			*defaults++ = (Xv_opaque) WIN_COLUMN_GAP;
			*defaults++ = xv_get(split_paint, WIN_COLUMN_GAP, 0);

			*defaults++ = (Xv_opaque) WIN_COLUMN_WIDTH;
			*defaults++ = xv_get(split_paint, WIN_COLUMN_WIDTH, 0);

			*defaults++ = (Xv_opaque) WIN_CURSOR;
			*defaults++ = xv_get(split_paint, WIN_CURSOR, 0);

			*defaults++ = (Xv_opaque) WIN_EVENT_PROC;
			*defaults++ = xv_get(split_paint, WIN_EVENT_PROC, 0);

			*defaults++ = (Xv_opaque) WIN_ROW_GAP;
			*defaults++ = xv_get(split_paint, WIN_ROW_GAP, 0);

			*defaults++ = (Xv_opaque) WIN_ROW_HEIGHT;
			*defaults++ = xv_get(split_paint, WIN_ROW_HEIGHT, 0);

			*defaults++ = (Xv_opaque) WIN_WINDOW_GRAVITY;
			*defaults++ = xv_get(split_paint, WIN_WINDOW_GRAVITY, 0);

			*defaults++ = (Xv_opaque) WIN_X_EVENT_MASK;
			*defaults++ = xv_get(split_paint, WIN_X_EVENT_MASK, 0);

			/* null terminate the list */
			*defaults = (Xv_opaque) 0;

			/* propagate the attrs to the new paint window */
			xv_set_avlist(self, defaults_array);

			/* Deal with possible scrollbars */
			sb = xv_get(canvas, OPENWIN_VERTICAL_SCROLLBAR, view);
			if (sb != XV_NULL) {
				canvas_scroll(self, sb);
			}
			sb = xv_get(canvas, OPENWIN_HORIZONTAL_SCROLLBAR, view);
			if (sb != XV_NULL) {
				canvas_scroll(self, sb);
			}
		}
	}
	else {
		xv_set(self,
				WIN_BIT_GRAVITY, status(priv, fixed_image)
										? NorthWestGravity :
										ForgetGravity,
				WIN_CONSUME_EVENTS,
					KBD_USE,
					KBD_DONE,
					WIN_ASCII_EVENTS,
					ACTION_HELP,
					WIN_MOUSE_BUTTONS,
					NULL,
				NULL);
		status_set(priv, created);
	}
	return XV_OK;
}


/*ARGSUSED*/ /*VARARGS*/
static Xv_opaque canvas_paint_set(Canvas_paint_window paint_public, Attr_avlist avlist)
{
	Attr_attribute attr;
	Xv_opaque result = XV_OK;

#ifdef OW_I18N
	Canvas_info *canvas = (Canvas_info *) xv_get(paint_public,
			XV_KEY_DATA, canvas_context_key);
#else
	Canvas_info *canvas;
#endif /*OW_I18N */

	for (attr = avlist[0]; attr; avlist = attr_next(avlist), attr = avlist[0]) {
		switch (attr) {
			case WIN_CMS_CHANGE:

#ifndef OW_I18N
				canvas = (Canvas_info *) xv_get(paint_public,
						XV_KEY_DATA, canvas_context_key);
#endif /*OW_I18N */

				if (status(canvas, cms_repaint)) {
					Rect rect;
					Rectlist rl;

					rect.r_left = 0;
					rect.r_top = 0;
					rect.r_width = (short)xv_get(paint_public, WIN_WIDTH);
					rect.r_height = (short)xv_get(paint_public, WIN_HEIGHT);
					rl = rl_null;
					rl_rectunion(&rect, &rl, &rl);

					win_set_damage(paint_public, &rl);
					canvas_inform_repaint(canvas, paint_public);
					win_clear_damage(paint_public);
				}
				break;

#ifdef OW_I18N
			case WIN_IC:
				canvas->ic = (XIC) avlist[1];
				break;

			case XV_END_CREATE:
				{
					Frame frame_public;
					const Xv_pkg *object_type;
					const Xv_pkg *frame_type;
					Canvas canvas_public;
					Xv_object serverobj;

					if (!xv_get(paint_public, WIN_USE_IM))
						break;
					if (canvas->ic)
						break;

					canvas_public = CANVAS_PUBLIC(canvas);

					object_type = (const Xv_pkg *) xv_get(canvas_public, XV_TYPE);
					if (object_type->attr_id == (Attr_pkg) ATTR_PKG_PANEL)
						break;

					/*
					 * Do we really have an IM ?
					 */
					serverobj = XV_SERVER_FROM_WINDOW(canvas_public);
					if ((XIM) xv_get(serverobj, XV_IM) == NULL)
						break;

					frame_public = (Frame) xv_get(canvas_public, WIN_FRAME);
					frame_type = (const Xv_pkg *) xv_get(frame_public, XV_TYPE);

#ifdef notdef
					/*
					 * Here is code to create the pew only on the base
					 * frame.  This is currently #ifdef out to allow popup
					 * frame to have a own pew.
					 */
					if (!strcmp(frame_type->name, "Frame_cmd")) {
						frame_public = (Frame) xv_get(frame_public, XV_OWNER);
						frame_type = (const Xv_pkg *) xv_get(frame_public, XV_TYPE);
					}

					if (strcmp(frame_type->name, "Frame_base"))
						break;
#endif

					canvas->pew = (Canvas_pew *) xv_get(frame_public,
							XV_KEY_DATA, canvas_pew_key);
					if (canvas->pew == NULL) {
						/*
						 * This is the first time creating canvas with IM
						 * on this paricular frame.
						 */
						canvas->pew = canvas_create_pew(frame_public);

						/*
						 * Get the pe_cache from panel which panel
						 * created.  We do not have to duplicates.
						 */
						canvas->pe_cache = panel_get_preedit(
								((Xv_panel_or_item *) canvas->pew->ptxt)->
								private_data);

						/*
						 * If the frame is popup frame (FRAME_CMD), we
						 * need to catch the WIN_CLOSE and WIN_OPEN event
						 * to sync up with base frame operation (ie, if
						 * this popup frame closes, pew should be close as
						 * well).  However this does not happen
						 * automatically, since pew will be child of the
						 * base frame.
						 */
						if (strcmp(frame_type->name, "Frame_cmd") == 0) {
							notify_interpose_event_func(frame_public,
									canvas_pew_event_proc, NOTIFY_SAFE);
						}
					}
					else {
						canvas->pe_cache = (XIMPreeditDrawCallbackStruct *)
								xv_alloc(XIMPreeditDrawCallbackStruct);
						canvas->pe_cache->text = (XIMText *) xv_alloc(XIMText);
						canvas->pe_cache->text->encoding_is_wchar = True;
						/*
						 * We have to set some value in string, otherwise
						 * panel code dumps.
						 */
						canvas->pe_cache->text->string.wide_char
								= wsdup(_xv_null_string_wc);
					}

					canvas->pew->reference_count++;

					canvas->ic = (XIC) xv_get(canvas_public, WIN_IC);
					if (canvas->ic == NULL)
						break;

#ifdef FULL_R5
					XGetICValues(canvas->ic, XNInputStyle, &canvas->xim_style,
							NULL);
#endif /* FULL_R5 */

					/*
					 * DEPEND_ON_BUG_1102972: This "#ifdef notdef" (else
					 * part) is a workaround for the bug 1102972.  We have
					 * to delay the setting of the XNFocusWindow to much
					 * later (after all event mask was set to paint window.
					 * Actually in this case will do in the event_proc with
					 * KBD_USE).
					 */

#ifdef notdef
					canvas->focus_pwin = paint_public;
					/* 
					 * Cache the XNFocusWindow whenever it is set 
					 */
					window_set_ic_focus_win(view_public, canvas->ic,
							xv_get(paint_public, XV_XID));
#else
					canvas->focus_pwin = 0;
#endif

					break;
				}
#endif /*OW_I18N */


			default:
				xv_check_bad_attr(&xv_canvas_pw_pkg, attr);
				break;
		}
	}


	return (result);
}
		
const Xv_pkg          xv_canvas_pw_pkg = {
    "Canvas paint window",
    (Attr_pkg) ATTR_PKG_CANVAS_PAINT_WINDOW,
    sizeof(Xv_canvas_pw),
    &xv_window_pkg,
    canvas_paint_init,
    canvas_paint_set,
    canvas_paint_get,
    NULL,
    NULL
};
const Xv_pkg          xv_canvas_pkg = {
    "Canvas",
    (Attr_pkg) ATTR_PKG_CANVAS,
    sizeof(Xv_canvas),
    &xv_openwin_pkg,
    canvas_init,
    canvas_set_avlist,
    canvas_get_attr,
    canvas_destroy,
    NULL
};
const Xv_pkg          xv_canvas_view_pkg = {
    "Canvas view",
    (Attr_pkg) ATTR_PKG_CANVAS_VIEW,
    sizeof(Xv_canvas_view),
    &xv_openwin_view_pkg,
    canvas_view_init,
    canvas_view_set,
    canvas_view_get,
    canvas_view_destroy,
    NULL
};
