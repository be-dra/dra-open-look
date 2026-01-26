#ifndef lint
char     ow_view_c_sccsid[] = "@(#)ow_view.c 1.43 91/04/24 DRA: $Id: ow_view.c,v 4.11 2026/01/25 09:32:33 dra Exp $ ";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */

/*
 * Module:	ow_view.c Product:	SunView 2.0
 *
 * Description:
 *
 * manages creating and destroyomg views
 *
 */


/*
 * Include files:
 */

#include <stdio.h>
#include <xview_private/ow_impl.h>
#include <xview_private/windowimpl.h>
#include <xview_private/svr_impl.h>
#include <xview/win_notify.h>
#include <xview/cms.h>
#include <xview/defaults.h>
#include <xview/help.h>

/*
 * Declaration of Functions Defined in This File (in order):
 */

static int openwin_locate_left_viewers(Openwin_view_info *views, Rect *r, Openwin_view_info *bounders[]);
static int openwin_locate_right_viewers(Openwin_view_info *views, Rect *r, Openwin_view_info *bounders[]);
static int openwin_locate_bottom_viewers(Openwin_view_info *views, Rect *r, Openwin_view_info *bounders[]);
static void openwin_remove_scrollbars(Openwin_view_info *view);
static void openwin_create_viewwindow(Xv_openwin_info *owin, Openwin_view_info *from_view, Openwin_view_info *view, Rect *r);
static void openwin_link_view(Xv_openwin_info *owin, Openwin_view_info *view);
static int openwin_locate_top_viewers(Openwin_view_info *views, Rect *r, Openwin_view_info *bounders[]);
static void openwin_expand_viewers(Xv_openwin_info *owin, Openwin_view_info *old_view, Openwin_view_info **viewers, Rect *r, Openwin_split_direction direction);

static void openwin_register_initial_sb(Xv_openwin_info *owin, Openwin_view_info *view, Scrollbar       sb, Scrollbar_setting direction);

Pkg_private void openwin_view_rect_from_avail_rect(Xv_openwin_info *owin, Openwin_view_info *view, Rect *r);
static void openwin_init_view(Xv_openwin_info *owin, Openwin_view_info *twin, Openwin_split_direction direction, Rect *r, Openwin_view_info **new_view);
static int openwin_unlink_view(Xv_openwin_info *owin, Openwin_view_info *view);
static int openwin_test_for_sb(Xv_openwin_info *owin, Scrollbar sb, Scrollbar_setting sb_direction, Openwin_view_info **view, int *last_sb);
static void openwin_free_view(Openwin_view_info *view);


typedef Xv_opaque  Resize_handle;
static const Xv_pkg *resize_handle_class(void);

extern char *xv_app_name;

#define RESIZE_HANDLE resize_handle_class()

#define RH_APPL_RESIZES OPENWIN_ATTR(ATTR_BOOLEAN, 244)
#define RH_SIDE         OPENWIN_ATTR(ATTR_BOOLEAN, 245)

/******************************************************************/

Pkg_private void openwin_create_initial_view(Xv_openwin_info *owin)
{
	Rect r;
	Openwin_view_info *new_view;

	r = *(Rect *) xv_get(OPENWIN_PUBLIC(owin), WIN_RECT);
	r.r_left = r.r_top = 0;

	openwin_init_view(owin, NULL, OPENWIN_SPLIT_VERTICAL, &r, &new_view);

	/* add scrollbars if we have seen them */
	if (owin->vsb_on_create) {
		openwin_register_initial_sb(owin, new_view, owin->vsb_on_create,
				SCROLLBAR_VERTICAL);
		owin->vsb_on_create = XV_NULL;
	}
	if (owin->hsb_on_create) {
		openwin_register_initial_sb(owin, new_view, owin->hsb_on_create,
				SCROLLBAR_HORIZONTAL);
		owin->hsb_on_create = XV_NULL;
	}
}

Pkg_private void openwin_destroy_views(Xv_openwin_info *owin)
{
    Openwin_view_info *view, *next_view;

    /* set that sb's being removed so remove code in layout */
    /* won't get called */
    STATUS_SET(owin, removing_scrollbars);

    for (view = owin->views; view != NULL; view = next_view) {
		next_view = view->next_view;
		openwin_free_view(view);
    }
}

#ifdef NOT_USED
static int openwin_check_view(Openwin_view_info *view)
{
	int ret;
	Scrollbar sb;

	ret = xv_destroy_status(VIEW_PUBLIC(view), DESTROY_CHECKING);
	if (ret != XV_OK)
		return (ret);

	if ((sb = openwin_sb(view, SCROLLBAR_VERTICAL)) != XV_NULL) {
		ret = xv_destroy_status(sb, DESTROY_CHECKING);
		if (ret != XV_OK)
			return (ret);
	}
	if ((sb = openwin_sb(view, SCROLLBAR_HORIZONTAL)) != XV_NULL) {
		ret = xv_destroy_status(sb, DESTROY_CHECKING);
		if (ret != XV_OK)
			return (ret);
	}
	return (ret);
}

static int openwin_check_views(Xv_openwin_info *owin)
{
	int ret;
	Openwin_view_info *view;

	for (view = owin->views; view != NULL; view = view->next_view) {
		if ((ret = openwin_check_view(view)) != XV_OK) {
			return (ret);
		}
	}
	return (XV_OK);
}
#endif /* NOT_USED */

Pkg_private int openwin_count_views(Xv_openwin_info *owin)
{
	int i = 0;
	Openwin_view_info *view = owin->views;

	while (view != NULL) {
		i++;
		view = view->next_view;
	}
	return (i);
}

Pkg_private Openwin_view_info *openwin_nth_view(Xv_openwin_info *owin, int place)
{
	int i = 0;
	Openwin_view_info *view = owin->views;

	for (i = 0; i < place; i++) {
		view = view->next_view;
		if (view == NULL) {
			return (NULL);
		}
	}
	return (view);
}

Pkg_private int openwin_viewdata_for_view(Xv_Window window, Openwin_view_info **view)
{
	*view = VIEW_PRIVATE(window);
	if (*view != NULL) {
		return (XV_OK);
	}
	else {
		return (XV_ERROR);
	}
}

Pkg_private int openwin_viewdata_for_sb(Xv_openwin_info *owin, Scrollbar sb, Openwin_view_info **view, Scrollbar_setting *sb_direction, int *last_sb)
{

	/* look vertical first */
	*last_sb = TRUE;
	*sb_direction = SCROLLBAR_VERTICAL;
	*view = NULL;
	openwin_test_for_sb(owin, sb, *sb_direction, view, last_sb);

	if (*view != NULL) {
		/* found it */
		return (XV_OK);
	}
	*last_sb = TRUE;
	*sb_direction = SCROLLBAR_HORIZONTAL;
	*view = NULL;

	return (openwin_test_for_sb(owin, sb, *sb_direction, view, last_sb));
}

static int openwin_test_for_sb(Xv_openwin_info *owin, Scrollbar sb, Scrollbar_setting sb_direction, Openwin_view_info **view, int *last_sb)
{
	Scrollbar test_sb;
	Openwin_view_info *test_view;


	for (test_view = owin->views; test_view != NULL;
			test_view = test_view->next_view) {
		test_sb = openwin_sb(test_view, sb_direction);
		if (test_sb == sb) {
			*view = test_view;
		}
		else if (test_sb != XV_NULL) {
			*last_sb = FALSE;
		}
	}

	if (*view != NULL) {
		return (XV_OK);
	}
	else {
		return (XV_ERROR);
	}
}

Pkg_private void openwin_split_view(Xv_openwin_info *owin, Openwin_view_info *view, Openwin_split_direction direction, int pos, int view_start)
{
	Openwin_view_info *new_view;
	Rect r, new_r;
	Scrollbar sb;

	/* compute the new rects for both the new and old views
	 * placing the new view to the left and above the old
	 */
	r = new_r = view->enclosing_rect;
	if (direction == OPENWIN_SPLIT_VERTICAL) {
		r.r_width = pos;
		new_r.r_left += pos;
		new_r.r_width -= pos;
	}
	else {
		r.r_height = pos;
		new_r.r_top += pos;
		new_r.r_height -= pos;
	}

	/* create new view */
	/* this automatically adjusts the view if needed */
	openwin_init_view(owin, view, direction, &new_r, &new_view);

	/* adjust active views rect */
	openwin_adjust_view(owin, view, &r);

	/* add needed scrollbars */
	if ((sb = openwin_sb(view, SCROLLBAR_VERTICAL)) != XV_NULL) {
		openwin_copy_scrollbar(owin, sb, new_view);
		if (direction == OPENWIN_SPLIT_HORIZONTAL) {
			sb = openwin_sb(new_view, SCROLLBAR_VERTICAL);
			xv_set(sb, SCROLLBAR_VIEW_START,
					view_start / (int)xv_get(sb, SCROLLBAR_PIXELS_PER_UNIT),
					NULL);
		}
	}
	if ((sb = openwin_sb(view, SCROLLBAR_HORIZONTAL)) != XV_NULL) {
		openwin_copy_scrollbar(owin, sb, new_view);
		if (direction == OPENWIN_SPLIT_VERTICAL) {
			sb = openwin_sb(new_view, SCROLLBAR_HORIZONTAL);
			xv_set(sb, SCROLLBAR_VIEW_START,
					view_start / (int)xv_get(sb, SCROLLBAR_PIXELS_PER_UNIT),
					NULL);
		}
	}

#ifdef SELECTABLE_VIEWS
	/* paint borders if needed */
	if (STATUS(owin, show_borders))
		openwin_paint_border(OPENWIN_PUBLIC(owin), new_view, TRUE);
#endif /* SELECTABLE_VIEWS */

	if (owin->split_init_proc) {
		(owin->split_init_proc)(VIEW_PUBLIC(view), VIEW_PUBLIC(new_view), pos);
	}
}

Pkg_private int openwin_fill_view_gap(Xv_openwin_info *owin, Openwin_view_info *view)
{
	Openwin_view_info *bounding_viewers[50];
	Rect r;

	/* four cases -- look right, left, bottom, top */

	r = view->enclosing_rect;
	if (openwin_locate_right_viewers(owin->views, &r, bounding_viewers)) {
		/* these windows grow horizontally */
		openwin_expand_viewers(owin, view, bounding_viewers, &r,
				OPENWIN_SPLIT_HORIZONTAL);
	}
	else if (openwin_locate_left_viewers(owin->views, &r, bounding_viewers)) {
		/* these windows grow horizontally */
		openwin_expand_viewers(owin, view, bounding_viewers, &r,
				OPENWIN_SPLIT_HORIZONTAL);
	}
	else if (openwin_locate_bottom_viewers(owin->views, &r, bounding_viewers)) {
		/* these windows grow vertically */
		openwin_expand_viewers(owin, view, bounding_viewers, &r,
				OPENWIN_SPLIT_VERTICAL);
	}
	else if (openwin_locate_top_viewers(owin->views, &r, bounding_viewers)) {
		/* these windows grow vertically */
		openwin_expand_viewers(owin, view, bounding_viewers, &r,
				OPENWIN_SPLIT_VERTICAL);
	}
	else {
		return (OPENWIN_CANNOT_EXPAND);
	}

	return (XV_OK);
}

Pkg_private void openwin_copy_scrollbar(Xv_openwin_info *owin, Scrollbar sb, Openwin_view_info *to_view)
{
	int view_length;
	Scrollbar_setting direction =
			(Scrollbar_setting) xv_get(sb, SCROLLBAR_DIRECTION);
	Rect sb_r, r;
	Scrollbar copy_sb;
	int pixs_per_unit;
	char instname[200], *owinst;
	const Xv_pkg *pkg;
	Openwin ow = OPENWIN_PUBLIC(owin);

	r = *(Rect *) xv_get(VIEW_PUBLIC(to_view), WIN_RECT);
	openwin_place_scrollbar(ow, VIEW_PUBLIC(to_view), sb, direction, &r, &sb_r);

	pixs_per_unit = (int)xv_get(sb, SCROLLBAR_PIXELS_PER_UNIT);
	view_length = (direction == SCROLLBAR_VERTICAL) ?
			(int)xv_get(VIEW_PUBLIC(to_view), WIN_HEIGHT) : (int)xv_get(VIEW_PUBLIC(to_view),
			WIN_WIDTH);
	view_length = view_length / pixs_per_unit;

	owinst = (char *)xv_get(ow, XV_INSTANCE_NAME);
	pkg = (const Xv_pkg *)xv_get(ow, XV_TYPE);
	sprintf(instname, "%s%sSB", owinst ? owinst : pkg->name,
					((direction == SCROLLBAR_VERTICAL) ? "vert" : "hor"));
	copy_sb = xv_create(ow, SCROLLBAR,
			XV_INSTANCE_NAME, instname,
			SCROLLBAR_DIRECTION, direction,
			SCROLLBAR_PIXELS_PER_UNIT, pixs_per_unit,
			SCROLLBAR_OBJECT_LENGTH, xv_get(sb, SCROLLBAR_OBJECT_LENGTH),
			SCROLLBAR_VIEW_START, xv_get(sb, SCROLLBAR_VIEW_START),
			SCROLLBAR_VIEW_LENGTH, view_length,
			SCROLLBAR_PAGE_LENGTH, xv_get(sb, SCROLLBAR_PAGE_LENGTH),
			SCROLLBAR_SHOW_PAGE, xv_get(sb, SCROLLBAR_SHOW_PAGE),
			SCROLLBAR_PAGE_HEIGHT, xv_get(sb, SCROLLBAR_PAGE_HEIGHT),
			XV_FONT, xv_get(sb, XV_FONT), /* es gab einen Grund... */
			SCROLLBAR_NORMALIZE_PROC, xv_get(sb, SCROLLBAR_NORMALIZE_PROC),
			SCROLLBAR_NOTIFY_CLIENT, VIEW_PUBLIC(to_view),
			SCROLLBAR_SPLITTABLE, xv_get(sb, SCROLLBAR_SPLITTABLE),
			SCROLLBAR_COMPUTE_SCROLL_PROC, xv_get(sb, SCROLLBAR_COMPUTE_SCROLL_PROC),
			WIN_RECT, &sb_r,
			XV_VISUAL, xv_get(sb, XV_VISUAL),
			WIN_CMS, xv_get(sb, WIN_CMS),
			XV_SHOW, TRUE,
			NULL);

	openwin_set_sb(to_view, direction, copy_sb);
}

Pkg_private void openwin_remove_split(Xv_openwin_info *owin, Openwin_view_info *view)
{
    openwin_unlink_view(owin, view);
    openwin_remove_scrollbars(view);
}

static void openwin_init_view(Xv_openwin_info *owin, Openwin_view_info *twin,
					Openwin_split_direction direction, Rect *r,
					Openwin_view_info **new_view)
{
	Openwin_view_info *view;

	*new_view = NULL;

	/* allocate the view */
	view = xv_alloc(Openwin_view_info);
	view->owin = owin;

	view->enclosing_rect = *r;

	if (twin == NULL) {
		/* default view -- on create -- add scrollbars */
		if (owin->vsb_on_create) {
			openwin_set_sb(view, SCROLLBAR_VERTICAL, owin->vsb_on_create);
		}
		if (owin->hsb_on_create) {
			openwin_set_sb(view, SCROLLBAR_HORIZONTAL, owin->hsb_on_create);
		}
		view->right_edge = view->bottom_edge = TRUE;
	}
	else {
		if (direction == OPENWIN_SPLIT_VERTICAL) {
			view->right_edge = twin->right_edge;
			twin->right_edge = FALSE;
			view->bottom_edge = twin->bottom_edge;
		}
		else {
			view->bottom_edge = twin->bottom_edge;
			twin->bottom_edge = FALSE;
			view->right_edge = twin->right_edge;
		}
	}

	/* use old view so get border and sb info */
	openwin_view_rect_from_avail_rect(owin, view, r);

	/* create the view window */
	openwin_create_viewwindow(owin, twin, view, r);
	openwin_link_view(owin, view);

	*new_view = view;
}


static void openwin_free_view(Openwin_view_info *view)
{
    openwin_remove_scrollbars(view);
    xv_destroy_status(VIEW_PUBLIC(view), DESTROY_CLEANUP);
}

static void openwin_remove_scrollbars(Openwin_view_info *view)
{
	Scrollbar vsb, hsb;

	vsb = openwin_sb(view, SCROLLBAR_VERTICAL);
	hsb = openwin_sb(view, SCROLLBAR_HORIZONTAL);

	if (vsb != XV_NULL) {
		view->sb[0] = XV_NULL;
		xv_destroy_status(vsb, DESTROY_CLEANUP);
	}
	if (hsb != XV_NULL) {
		view->sb[1] = XV_NULL;
		xv_destroy_status(hsb, DESTROY_CLEANUP);
	}
}

static void paint_border(Xv_openwin_info *priv, Openwin_view_info *vp)
{
	if (vp == priv->selected_view) {
		Xv_window view = VIEW_PUBLIC(vp);

		XDrawRectangle((Display *)xv_get(view, XV_DISPLAY),
					(Window)xv_get(view, XV_XID),
					priv->border_gc, 0, 0,
					(unsigned)xv_get(view, XV_WIDTH) - 1,
					(unsigned)xv_get(view, XV_HEIGHT) - 1);
	}
}

static void split_destroy_preview(Xv_window win, Openwin_view_info *view,
										unsigned do_begin)
{
	Display *dpy = (Display *) xv_get(win, XV_DISPLAY);

	if (do_begin) {
		Xv_Drawable_info *info;
		Rect vrect;
		GC *gc_list;

		DRAWABLE_INFO_MACRO(win, info);
		gc_list = (GC *) xv_get(xv_screen(info), SCREEN_OLGC_LIST, win);

		vrect = *((Rect *) xv_get(win, WIN_OUTER_RECT));

		/* muss man immer wieder machen, wegen evtl PANEL_MULTILINE_TEXT... */
		XSetForeground(dpy, gc_list[SCREEN_JOINPREVIEW_GC], xv_bg(info));
		XFillRectangle(dpy, xv_get(OPENWIN_PUBLIC(view->owin), XV_XID),
				gc_list[SCREEN_JOINPREVIEW_GC], vrect.r_left, vrect.r_top,
				(unsigned)vrect.r_width, (unsigned)vrect.r_height);
	}
	else {
		XExposeEvent xexp;
		Event event;
		Window xid = (Window) xv_get(win, XV_XID);

		/* XClearWindow will NOT redraw the border */
		XUnmapWindow(dpy, xid);
		XMapWindow(dpy, xid);

		event_init(&event);
		event_set_id(&event, WIN_REPAINT);
		event_set_action(&event, WIN_REPAINT);
		event_set_window(&event, win);
		event_set_xevent(&event, (XEvent *) & xexp);
		xexp.type = Expose;
		xexp.send_event = 0;
		xexp.display = dpy;
		xexp.window = xid;
		xexp.x = 0;
		xexp.y = 0;
		xexp.width = (int)xv_get(win, XV_WIDTH);
		xexp.height = (int)xv_get(win, XV_HEIGHT);
		xexp.count = 0;

		win_post_event(win, &event, NOTIFY_IMMEDIATE);
	}
}

static void set_rh_positions(Xv_openwin_info *priv, Openwin_view_info *vp)
{
	Rect view_r;

	if (vp != priv->selected_view) return;

	view_r = *((Rect *)xv_get(VIEW_PUBLIC(vp), XV_RECT));

	xv_set(priv->resize_handles[(int)OPENWIN_TOP - 1],
			XV_X, view_r.r_left + view_r.r_width / 2 -
					Vertsb_Endbox_Width(priv->ginfo) / 2,
			XV_Y, view_r.r_top,
			NULL);

	xv_set(priv->resize_handles[(int)OPENWIN_BOTTOM - 1],
			XV_X, view_r.r_left + view_r.r_width / 2 -
					Vertsb_Endbox_Width(priv->ginfo) / 2,
			XV_Y, rect_bottom(&view_r) -
					Vertsb_Endbox_Height(priv->ginfo) +
					2 * WIN_DEFAULT_BORDER_WIDTH + 1,
			NULL);

	xv_set(priv->resize_handles[(int)OPENWIN_LEFT - 1],
			XV_Y, view_r.r_top + view_r.r_height / 2 -
					Vertsb_Endbox_Width(priv->ginfo) / 2,
			XV_X, view_r.r_left,
			NULL);

	xv_set(priv->resize_handles[(int)OPENWIN_RIGHT - 1],
			XV_Y, view_r.r_top + view_r.r_height / 2 -
					Vertsb_Endbox_Width(priv->ginfo) / 2,
			XV_X, rect_right(&view_r) -
					Vertsb_Endbox_Height(priv->ginfo) +
					2 * WIN_DEFAULT_BORDER_WIDTH + 1,
			NULL);
}

static void do_split(Openwin_view_info *vp,Openwin_split_direction dir, int pos)
{
	Openwin self = OPENWIN_PUBLIC(vp->owin);

	if (vp == vp->owin->selected_view) {
		xv_set(self, OPENWIN_SELECTED_VIEW, NULL, NULL);
	}
	xv_set(self,
			OPENWIN_SPLIT,
				OPENWIN_SPLIT_VIEW, VIEW_PUBLIC(vp),
				OPENWIN_SPLIT_DIRECTION, dir,
				OPENWIN_SPLIT_POSITION, pos,
				NULL,
			NULL);

	/* now we have at least two views: we act 'selectably': */
	if (! STATUS(vp->owin,selectable)) {
		/* but only if we have a paint window */
		if (vp->pw) {
			Openwin_view_info *view;

			for (view = vp->owin->views; view != NULL; view = view->next_view) {
				xv_set(VIEW_PUBLIC(view),
							WIN_CURSOR, vp->owin->target_ptr,
							NULL);
			}
		}
	}
}

static void do_join(Openwin_view_info *vp)
{
	int num_views;
	Xv_openwin_info *owin = vp->owin;
	Openwin self = OPENWIN_PUBLIC(owin);

	if (vp == owin->selected_view) {
		xv_set(self,
				OPENWIN_SELECTED_VIEW, NULL,
				NULL);
	}

	if (vp->pw) {
		if (owin->last_focus_pw == vp->pw) {
			Xv_window pw = XV_NULL, focwin;

			focwin = xv_get(xv_get(self,WIN_FRAME),FRAME_FOCUS_WIN);

			if (focwin && xv_get(focwin, WIN_PARENT) == vp->pw) {
				xv_set(focwin, XV_SHOW, FALSE, NULL);

				OPENWIN_EACH_PW(self, pw)
					if (pw != vp->pw) {
						xv_set(focwin, WIN_PARENT, pw, NULL);
						owin->last_focus_pw = pw;
						xv_set(self, WIN_SET_FOCUS, NULL);
						break;
					}
				OPENWIN_END_EACH
			}
			if (!pw || owin->last_focus_pw == vp->pw) {
				owin->last_focus_pw = XV_NULL;
			}
		}
	}

	num_views = openwin_count_views(owin);
	if (num_views > 1) {
		void (*destroy_proc)(Xv_window);

		/* before OPENVIEW_VIEW existed, we had only xv_destroy_save
		 * here - and the layout things were done in openwin_layout.
		 * See in openwin.c the WIN_DESTROY case .
		 *
		 * Now the method openwin_view_destroy destroys the 
		 * scrollbars (their WIN_DESTROY will be called...).
		 *
		 * But the WIN_DESTROY case is no longer called for the view....
		 */
		openwin_fill_view_gap(owin, vp);
		destroy_proc = owin->split_destroy_proc;

		xv_destroy_safe(VIEW_PUBLIC(vp));
		--num_views;

		if (destroy_proc) {
			destroy_proc(self);
		}

		if (num_views < 2) {
            if (! STATUS(owin,selectable)) {
				Xv_screen screen = XV_SCREEN_FROM_WINDOW(self);

				xv_set(VIEW_PUBLIC(owin->views),
						 WIN_CURSOR, xv_get(screen, SCREEN_BASIC_CURSOR),
						 NULL);
			}
		}
	}
}

Xv_private void screen_adjust_gc_color(Xv_Window window, int gc_index);

static void openwin_clear_damage(Xv_Window window, Rectlist *rl)
{
	register Xv_Drawable_info *info;
	Xv_Screen screen;
	GC *gc_list;

	if (rl) {
		DRAWABLE_INFO_MACRO(window, info);
		screen = xv_screen(info);
		gc_list = (GC *) xv_get(screen, SCREEN_OLGC_LIST, window);
		screen_adjust_gc_color(window, SCREEN_CLR_GC);
		XFillRectangle(xv_display(info), xv_xid(info), gc_list[SCREEN_CLR_GC],
				rl->rl_bound.r_left, rl->rl_bound.r_top,
				(unsigned)rl->rl_bound.r_width,
				(unsigned)rl->rl_bound.r_height);
	}
}

/*
 * openwin_view_event - event handler for openwin views
 */
static Notify_value openwin_view_event(Xv_window view, Notify_event ev,
								Notify_arg arg, Notify_event_type type)
{
    Event *event = (Event *)ev;
	Openwin_view_info *vp = VIEW_PRIVATE(view);
	Notify_value val;

	switch (event_action(event)) {
		case WIN_RESIZE:
			val =  notify_next_event_func(view, ev, arg, type);
			set_rh_positions(vp->owin, vp);
			return val;

        case ACTION_SELECT:
            if (STATUS(vp->owin,selectable) ||
				(vp->pw && openwin_count_views(vp->owin) > 1))
			{
				/* without that vp->pw query a TEXTSW with several views
				 * would no longer react on ACTION_SELECT
				 */
                if (event_is_up(event)) {
                    vp->owin->seltime = event_time(event);
                    xv_set(OPENWIN_PUBLIC(vp->owin),
								OPENWIN_SELECTED_VIEW, view,
								NULL);
                }
                return NOTIFY_DONE;
			}
            break;

		case WIN_REPAINT:
			/* clear the damaged area */
			if (STATUS(vp->owin, auto_clear)) {
				openwin_clear_damage(view, win_get_damage(view));
			}
			paint_border(vp->owin, vp);
			break;

		case ACTION_WHEEL_FORWARD:
			if (event_is_down(event) && vp->sb[0]) {
				int newpos = (int)xv_get(vp->sb[0], SCROLLBAR_VIEW_START);
				if (newpos > 0) {
					xv_set(vp->sb[0], SCROLLBAR_VIEW_START, newpos - 1, NULL);
				}
			}
			break;

		case ACTION_WHEEL_BACKWARD:
			if (event_is_down(event) && vp->sb[0]) {
				int newpos = (int)xv_get(vp->sb[0], SCROLLBAR_VIEW_START);
				xv_set(vp->sb[0], SCROLLBAR_VIEW_START, newpos + 1, NULL);
			}
			break;

		case ACTION_SPLIT_HORIZONTAL:
			do_split(vp, OPENWIN_SPLIT_HORIZONTAL, (int)arg);
			break;

		case ACTION_SPLIT_VERTICAL:
			do_split(vp, OPENWIN_SPLIT_VERTICAL, (int)arg);
			break;

		case ACTION_SPLIT_DESTROY:
			do_join(vp);
			return NOTIFY_DONE;

		case ACTION_HELP:
			if (event_is_down(event)) {
				char *help = (char *)xv_get(view, XV_HELP_DATA);

				if (help) {
					xv_help_show(view, help, event);
					return NOTIFY_DONE;
				}
				xv_help_show(view, "xview:pane_borders", event);
			}
			return NOTIFY_DONE;

		case ACTION_RESCALE:
			if (vp->sb[0]) {
    			notify_post_event_and_arg(vp->sb[0], ev, NOTIFY_IMMEDIATE,
			      		arg, NOTIFY_COPY_NULL, NOTIFY_RELEASE_NULL);
			}
			if (vp->sb[1]) {
    			notify_post_event_and_arg(vp->sb[1], ev, NOTIFY_IMMEDIATE,
			      		arg, NOTIFY_COPY_NULL, NOTIFY_RELEASE_NULL);
			}
			if (vp->pw) {
    			notify_post_event_and_arg(vp->pw, ev, NOTIFY_IMMEDIATE,
			      		arg, NOTIFY_COPY_NULL, NOTIFY_RELEASE_NULL);
			}
			return NOTIFY_DONE;

#ifdef ACTION_SPLIT_DESTROY_PREVIEW_BEGIN
		case ACTION_SPLIT_DESTROY_PREVIEW_BEGIN:
			if (openwin_count_views(vp->owin) <= 1) return NOTIFY_IGNORED;
			split_destroy_preview(view, vp, TRUE);
			break;

		case ACTION_SPLIT_DESTROY_PREVIEW_CANCEL:
			if (openwin_count_views(vp->owin) <= 1) return NOTIFY_IGNORED;
			split_destroy_preview(view, vp, FALSE);
			break;
#endif /* ACTION_SPLIT_DESTROY_PREVIEW_BEGIN */

		default:
			break;
	}
	return notify_next_event_func(view, ev, arg, type);
}

static Notify_value note_pw_event(Xv_window pw, Notify_event event,
							Notify_arg arg, Notify_event_type type)
{
	Notify_value val;
	Event *ev = (Event *)event;
	Openwin_view_info *vp = VIEW_PRIVATE(xv_get(pw,XV_OWNER));

	val = notify_next_event_func(pw, event, arg, type);

	switch (event_action(ev)) {
		case KBD_USE:
			vp->owin->last_focus_pw = pw;
			frame_kbd_use(xv_get(OPENWIN_PUBLIC(vp->owin), WIN_FRAME),
									OPENWIN_PUBLIC(vp->owin), pw);
			break;

		case KBD_DONE:
			frame_kbd_done(xv_get(OPENWIN_PUBLIC(vp->owin), WIN_FRAME),
									OPENWIN_PUBLIC(vp->owin));
			break;

		case WIN_REPAINT:

			paint_border(vp->owin, vp);
			break;
	}
	return val;
}

/* erstmal will ich das hier nicht in openwin_view_init verlagern, deshalb: */
static Openwin_view_info *very_transient_created = NULL;

static void openwin_create_viewwindow(Xv_openwin_info *owin, Openwin_view_info *from_view, Openwin_view_info *view, Rect *r)
{
	int xborders;
	Visual *visual;
	Cms cms;
	Openwin self = OPENWIN_PUBLIC(owin);
	Openwin_view newview;
	const Xv_pkg *vcl = (const Xv_pkg *)xv_get(self, OPENWIN_VIEW_CLASS);
	const Xv_pkg *pwcl = (const Xv_pkg *)xv_get(self, OPENWIN_PW_CLASS);

	if (from_view != NULL) {
		xborders = (int)xv_get(VIEW_PUBLIC(from_view), WIN_BORDER);
		visual = (Visual *) xv_get(VIEW_PUBLIC(from_view), XV_VISUAL);
		cms = (Cms) xv_get(VIEW_PUBLIC(from_view), WIN_CMS);
	}
	else {
		xborders = STATUS(owin, show_borders) ? TRUE : FALSE;
		visual = (Visual *) xv_get(OPENWIN_PUBLIC(owin), XV_VISUAL);
		cms = (Cms) xv_get(OPENWIN_PUBLIC(owin), WIN_CMS);
	}

	/* die openwin_view_init-Methode allokiert das nicht, sondern nimmt
	 * es von hier:
	 */
	very_transient_created = view;

	if (owin->view_attrs == NULL) {
		/* parent/child registration process sets the data handle */
		newview = xv_create(OPENWIN_PUBLIC(owin), vcl,
				WIN_NOTIFY_SAFE_EVENT_PROC, openwin_view_event,
				WIN_NOTIFY_IMMEDIATE_EVENT_PROC, openwin_view_event,
				WIN_RECT, r,
				WIN_BORDER, (long)xborders,
				XV_VISUAL, visual,
				WIN_CMS, cms,
				NULL);
	}
	else {
		/* parent/child registration process sets the data handle */
		newview = xv_create(OPENWIN_PUBLIC(owin), vcl,
				ATTR_LIST, owin->view_attrs,
				WIN_NOTIFY_SAFE_EVENT_PROC, openwin_view_event,
				WIN_NOTIFY_IMMEDIATE_EVENT_PROC, openwin_view_event,
				WIN_RECT, r,
				WIN_BORDER, (long)xborders,
				XV_VISUAL, visual,
				WIN_CMS, cms,
				NULL);

		/* if client toggled xborders redo rect placement */
		if ((int)xv_get(newview, WIN_BORDER) != xborders) {
			*r = view->enclosing_rect;
			openwin_view_rect_from_avail_rect(owin, view, r);
			if (!rect_equal(&view->enclosing_rect, r)) {
				xv_set(newview, WIN_RECT, r, NULL);
			}
			/* no xborders is the default so if xborders is TRUE */
			/* and we each here assume client defaulted */
			/* WIN_BORDER to mean inherit from splittee. */
			/* Therefore we do a set to turn the borders on */
			/* we couldn't do this in the xv_create call */
			/* because ATTR_LIST must appear first in the avlist */
			/* This means a split can't override a border true */
			/* on create. No big deal since it is rare. */
			if (xborders) {
				xv_set(newview, WIN_BORDER, xborders, NULL);
			}
		}
		xv_free(owin->view_attrs);
		owin->view_attrs = NULL;
	}
	if (pwcl) {
		if (owin->pw_attrs == NULL) {
			if (STATUS(owin,selectable)) {
				view->pw = xv_create(newview, pwcl,
								WIN_BORDER, FALSE,
								WIN_COLLAPSE_EXPOSURES, FALSE,
								WIN_RETAINED, FALSE, /* wegen SubwindowMode */
								NULL);
			}
			else {
				view->pw = xv_create(newview, pwcl,
								WIN_BORDER, FALSE,
								WIN_COLLAPSE_EXPOSURES, FALSE,
								NULL);
			}
		}
		else {
			if (STATUS(owin,selectable)) {
				view->pw = xv_create(newview, pwcl,
								ATTR_LIST, owin->pw_attrs,
								WIN_BORDER, FALSE,
								WIN_COLLAPSE_EXPOSURES, FALSE,
								WIN_RETAINED, FALSE, /* wegen SubwindowMode */
								NULL);
			}
			else {
				view->pw = xv_create(newview, pwcl,
								ATTR_LIST, owin->pw_attrs,
								WIN_BORDER, FALSE,
								WIN_COLLAPSE_EXPOSURES, FALSE,
								NULL);
			}
			xv_free(owin->pw_attrs);
			owin->pw_attrs = NULL;
		}
		/* wenn von hier 'Unknown client' kommt, sollte man mit
		 * OPENWIN_PW_ATTRS bereits etwas Event-Handlings-maessiges
		 * einbauen
		 */
		notify_interpose_event_func(view->pw, note_pw_event, NOTIFY_SAFE);
		notify_interpose_event_func(view->pw, note_pw_event, NOTIFY_IMMEDIATE);

		if (STATUS(owin, selectable)) {
			xv_set(newview,
					WIN_CURSOR, owin->target_ptr,
					WIN_CONSUME_EVENTS,
						ACTION_HELP, WIN_MOUSE_BUTTONS, WIN_RESIZE, NULL,
					NULL);
		}
		else {
			xv_set(newview,
					WIN_CONSUME_EVENTS,
						ACTION_HELP, WIN_MOUSE_BUTTONS, WIN_RESIZE, NULL,
					NULL);
		}
	}
	else {
		STATUS_RESET(owin, selectable);
	}
}

static void openwin_link_view(Xv_openwin_info *owin, Openwin_view_info *view)
{
	Openwin_view_info *t_view;

	if (owin->views == NULL) {
		owin->views = view;
	}
	else {
		for (t_view = owin->views; t_view->next_view != NULL;
				t_view = t_view->next_view);
		t_view->next_view = view;
	}
}

static int openwin_unlink_view(Xv_openwin_info *owin, Openwin_view_info *view)
{
	Openwin_view_info *t_view;

	if (owin->views == view) {
		owin->views = view->next_view;
		return (XV_OK);
	}
	else {
		for (t_view = owin->views; t_view->next_view != NULL;
				t_view = t_view->next_view) {
			if (t_view->next_view == view) {
				t_view->next_view = view->next_view;
				return (XV_OK);
			}
		}
	}
	return (XV_ERROR);
}


static int
openwin_locate_right_viewers(views, r, bounders)
    Openwin_view_info *views;
    Rect           *r;
    Openwin_view_info *bounders[];
{
    Openwin_view_info *view;
    Rect            view_r;
    int             num_bound = 0;
    int             found_min = FALSE, found_max = FALSE;

    for (view = views; view != NULL; view = view->next_view) {
	view_r = view->enclosing_rect;

	if ((r->r_left + r->r_width) == view_r.r_left) {
	    /* if views starting point and ending point is */
	    /* between the gap add to list */
	    if (view_r.r_top >= r->r_top) {
		if (view_r.r_top + view_r.r_height <= r->r_top + r->r_height) {
		    bounders[num_bound++] = view;
		} else {
		    /* view extends beyond end of gap */
		    return (FALSE);
		}
	    }
	    if (view_r.r_top == r->r_top) {
		found_min = TRUE;
	    }
	    if (view_r.r_top + view_r.r_height == r->r_top + r->r_height) {
		found_max = TRUE;
	    }
	}
    }

    if (num_bound > 0) {
	bounders[num_bound] = NULL;
    }
    return (found_min && found_max);
}

static int openwin_locate_left_viewers(Openwin_view_info *views, Rect *r, Openwin_view_info *bounders[])
{
    Openwin_view_info *view;
    Rect            view_r;
    int             num_bound = 0;
    int             found_min = FALSE, found_max = FALSE;

    /* look below/to the right first */
    for (view = views; view != NULL; view = view->next_view) {
	view_r = view->enclosing_rect;

	if (r->r_left == (view_r.r_left + view_r.r_width)) {
	    /* see if view fits in the gap */
	    if (view_r.r_top >= r->r_top) {
		if (view_r.r_top + view_r.r_height <= r->r_top + r->r_height) {
		    bounders[num_bound++] = view;
		} else {
		    /* view extend beyond gap bounds */
		    return (FALSE);
		}
	    }
	    if (view_r.r_top == r->r_top) {
		found_min = TRUE;
	    }
	    if (view_r.r_top + view_r.r_height == r->r_top + r->r_height) {
		found_max = TRUE;
	    }
	}
    }

    if (num_bound > 0) {
	bounders[num_bound] = NULL;
    }
    return (found_min && found_max);
}

static int openwin_locate_bottom_viewers(Openwin_view_info *views, Rect *r, Openwin_view_info *bounders[])
{
    Openwin_view_info *view;
    Rect            view_r;
    int             num_bound = 0;
    int             found_min = FALSE, found_max = FALSE;

    /* look below/to the right first */
    for (view = views; view != NULL; view = view->next_view) {
	view_r = view->enclosing_rect;

	if ((r->r_top + r->r_height) == view_r.r_top) {
	    /* see if view fits in the gap */
	    if (view_r.r_left >= r->r_left) {
		if (view_r.r_left + view_r.r_width <= r->r_left + r->r_width) {
		    bounders[num_bound++] = view;
		} else {
		    /* view extend beyond gap bounds */
		    return (FALSE);
		}
	    }
	    if (view_r.r_left == r->r_left) {
		found_min = TRUE;
	    }
	    if (view_r.r_left + view_r.r_width == r->r_left + r->r_width) {
		found_max = TRUE;
	    }
	}
    }

    if (num_bound > 0) {
	bounders[num_bound] = NULL;
    }
    return (found_min && found_max);
}

static int openwin_locate_top_viewers(Openwin_view_info *views, Rect *r, Openwin_view_info *bounders[])
{
    Openwin_view_info *view;
    Rect            view_r;
    int             num_bound = 0;
    int             found_min = FALSE, found_max = FALSE;

    /* look below/to the right first */
    for (view = views; view != NULL; view = view->next_view) {
	view_r = view->enclosing_rect;

	if (r->r_top == (view_r.r_top + view_r.r_height)) {
	    /* see if view fits in the gap */
	    if (view_r.r_left >= r->r_left) {
		if (view_r.r_left + view_r.r_width <= r->r_left + r->r_width) {
		    bounders[num_bound++] = view;
		} else {
		    /* view extend beyond gap bounds */
		    return (FALSE);
		}
	    }
	    if (view_r.r_left == r->r_left) {
		found_min = TRUE;
	    }
	    if (view_r.r_left + view_r.r_width == r->r_left + r->r_width) {
		found_max = TRUE;
	    }
	}
    }

    if (num_bound > 0) {
	bounders[num_bound] = NULL;
    }
    return (found_min && found_max);
}

static void openwin_expand_viewers(Xv_openwin_info *owin, Openwin_view_info *old_view, Openwin_view_info **viewers, Rect *r, Openwin_split_direction direction)
{
    Rect            view_r;
    Openwin_view_info *view;

    for (view = *viewers; view != NULL; view = *(++viewers)) {
	view_r = view->enclosing_rect;
	if (direction == OPENWIN_SPLIT_VERTICAL) {
	    if (view_r.r_top > r->r_top) {
		view_r.r_top = r->r_top;
	    }
	    view_r.r_height += r->r_height;

	    if (old_view->bottom_edge) {
		view->bottom_edge = TRUE;
	    }
	} else {
	    if (view_r.r_left > r->r_left) {
		view_r.r_left = r->r_left;
	    }
	    view_r.r_width += r->r_width;

	    if (old_view->right_edge) {
		view->right_edge = TRUE;
	    }
	}
	openwin_adjust_view(owin, view, &view_r);
    }
}

static void openwin_register_initial_sb(Xv_openwin_info *owin, Openwin_view_info *view, Scrollbar       sb, Scrollbar_setting direction)
{
	Rect r, sb_r;
	long unsigned view_length;

	r = *(Rect *) xv_get(VIEW_PUBLIC(view), WIN_RECT);

	openwin_place_scrollbar(OPENWIN_PUBLIC(owin), VIEW_PUBLIC(view), sb, direction,
			&r, &sb_r);

	view_length = (direction == SCROLLBAR_VERTICAL) ? r.r_height : r.r_width;
	view_length = view_length / (int)xv_get(sb, SCROLLBAR_PIXELS_PER_UNIT);

	if (xv_get(sb, WIN_PARENT) != OPENWIN_PUBLIC(owin) ||
			xv_get(sb, XV_OWNER) != OPENWIN_PUBLIC(owin))
		xv_set(sb,
				WIN_PARENT, OPENWIN_PUBLIC(owin),
				XV_OWNER, OPENWIN_PUBLIC(owin),
				NULL);
	xv_set(sb,
			WIN_RECT, &sb_r,
			SCROLLBAR_DIRECTION, direction,
			SCROLLBAR_VIEW_LENGTH, view_length,
			SCROLLBAR_NOTIFY_CLIENT, VIEW_PUBLIC(view),
			XV_SHOW, TRUE,
			NULL);
}

static void deselect_view(Xv_openwin_info *priv)
{
	Openwin_view_info *vp = priv->selected_view;
	Xv_window view;
	int i;

	if (! vp) return;

	view = VIEW_PUBLIC(vp);
	priv->selected_view = (Openwin_view_info *)0;

    for (i = 0; i < (int)OPENWIN_RIGHT; i++) {
        if (priv->resize_handles[i]) {
            xv_set(priv->resize_handles[i], XV_SHOW, FALSE, NULL);
        }
    }

	SERVERTRACE((500, "%s: %s: XClearArea\n", xv_app_name, __FUNCTION__));
	XClearArea((Display *)xv_get(view, XV_DISPLAY),
				(Window)xv_get(vp->pw, XV_XID),
				0, 0, 0, 0, TRUE);
}

static void select_view(Xv_openwin_info *priv, Openwin_view_info *vp)
{
	if (vp) {
		Openwin self = OPENWIN_PUBLIC(priv);
		Xv_window view = VIEW_PUBLIC(vp);
		Xv_window pw = vp->pw;
		Display *dpy = (Display *)xv_get(pw, XV_DISPLAY);
		Window pww = xv_get(pw, XV_XID);
		Resize_handle rh;
		int ar;
		int sb_off;
		Rect ow_r, view_r;
		Scrollbar sb;
		int num_views = (int)xv_get(self, OPENWIN_NVIEWS);
		int viewdelta = priv->margin + 2;

		if (priv->selected_view) {
			if (vp == priv->selected_view) return;
		}
		else {
			xv_set(priv->sel_owner, SEL_TIME, &priv->seltime, NULL);
			xv_set(priv->sel_owner, SEL_OWN, TRUE, NULL);
		}

		deselect_view(priv);

		/* now select the new view */
		priv->selected_view = vp;

		/* we want the rect BEFORE modifying border.... */
		view_r = *((Rect *)xv_get(view, XV_RECT));
		ow_r = *((Rect *)xv_get(self, XV_RECT));


/*
 	Auch wenn man hier die border_width auf 6 setzt, wird zwar ein
	dicker Rand gezeichnet, aber
	WEDER fallen Maus-Events auf dem dicken Rand in das View-Window
	NOCH ist auf dem dicken Rand der target_ptr
 */

		SERVERTRACE((500, "%s: %s: XClearArea\n", xv_app_name, __FUNCTION__));
		XClearArea(dpy, pww, 0, 0, 0, 0, TRUE);

		rh = XV_NULL;

		/* application says, my top is adjustable - and 'view' is the topmost */
		if ((priv->resize_sides & (1 << ((int)OPENWIN_TOP - 1))) 
			&& view_r.r_top < viewdelta)
		{
			rh = priv->resize_handles[(int)OPENWIN_TOP - 1];
			ar = TRUE;
		}
		else {
			if (num_views > 1 && view_r.r_top > viewdelta) {
				rh = priv->resize_handles[(int)OPENWIN_TOP - 1];
				ar = FALSE;
			}
		}
		if (rh) {
			xv_set(rh,
					RH_APPL_RESIZES, ar,
					XV_X, view_r.r_left + view_r.r_width / 2 -
							Vertsb_Endbox_Width(priv->ginfo) / 2,
					XV_Y, view_r.r_top,
					XV_WIDTH, Vertsb_Endbox_Width(priv->ginfo),
					XV_HEIGHT, Vertsb_Endbox_Height(priv->ginfo),
					XV_SHOW, TRUE,
					WIN_FRONT,
					NULL);
		}

		if ((sb = xv_get(self, OPENWIN_HORIZONTAL_SCROLLBAR, view)))
			sb_off = (int)xv_get(sb, XV_HEIGHT);
		else sb_off = 0;

		/* application says, my bottom is adjustable -
		 * and 'view' is the bottommost view
		 */
		rh = XV_NULL;
		if ((priv->resize_sides & (1 << ((int)OPENWIN_BOTTOM - 1)))
			&& rect_bottom(&view_r) > ow_r.r_height - sb_off - viewdelta)
		{
			rh = priv->resize_handles[(int)OPENWIN_BOTTOM - 1];
			ar = TRUE;
		}
		else {
			if (num_views > 1
				&& rect_bottom(&view_r) < ow_r.r_height - sb_off - viewdelta)
			{
				rh = priv->resize_handles[(int)OPENWIN_BOTTOM - 1];
				ar = FALSE;
			}
		}

		if (rh) {
			xv_set(rh,
					RH_APPL_RESIZES, ar,
					XV_X, view_r.r_left + view_r.r_width / 2 -
							Vertsb_Endbox_Width(priv->ginfo) / 2,
					XV_Y, rect_bottom(&view_r) -
							Vertsb_Endbox_Height(priv->ginfo) +
							2 * WIN_DEFAULT_BORDER_WIDTH + 1,
					XV_WIDTH, Vertsb_Endbox_Width(priv->ginfo),
					XV_HEIGHT, Vertsb_Endbox_Height(priv->ginfo),
					XV_SHOW, TRUE,
					WIN_FRONT,
					NULL);
		}

		rh = XV_NULL;
		if ((priv->resize_sides & (1 << ((int)OPENWIN_LEFT - 1))) 
			&& view_r.r_left < viewdelta)
		{
			rh = priv->resize_handles[(int)OPENWIN_LEFT - 1];
			ar = TRUE;
		}
		else {
			if (num_views > 1 && view_r.r_left > viewdelta) {
				rh = priv->resize_handles[(int)OPENWIN_LEFT - 1];
				ar = FALSE;
			}
		}
		if (rh) {
			xv_set(rh,
					RH_APPL_RESIZES, ar,
					XV_Y, view_r.r_top + view_r.r_height / 2 -
							Vertsb_Endbox_Width(priv->ginfo) / 2,
					XV_X, view_r.r_left,
					XV_HEIGHT, Vertsb_Endbox_Width(priv->ginfo),
					XV_WIDTH, Vertsb_Endbox_Height(priv->ginfo),
					XV_SHOW, TRUE,
					WIN_FRONT,
					NULL);
		}

		if ((sb = xv_get(self, OPENWIN_VERTICAL_SCROLLBAR, view)))
			sb_off = (int)xv_get(sb, XV_WIDTH);
		else sb_off = 0;

		rh = XV_NULL;
		if ((priv->resize_sides & (1 << ((int)OPENWIN_RIGHT - 1)))
			&& rect_right(&view_r) > ow_r.r_width - sb_off - viewdelta)
		{
			rh = priv->resize_handles[(int)OPENWIN_RIGHT - 1];
			ar = TRUE;
		}
		else {
			if (num_views > 1
				&& rect_right(&view_r) < ow_r.r_width - sb_off - viewdelta)
			{
				rh = priv->resize_handles[(int)OPENWIN_RIGHT - 1];
				ar = FALSE;
			}
		}
		if (rh) {
			xv_set(rh,
						RH_APPL_RESIZES, ar,
						XV_Y, view_r.r_top + view_r.r_height / 2 -
								Vertsb_Endbox_Width(priv->ginfo) / 2,
						XV_X, rect_right(&view_r) -
								Vertsb_Endbox_Height(priv->ginfo) +
								2 * WIN_DEFAULT_BORDER_WIDTH + 1,
						XV_HEIGHT, Vertsb_Endbox_Width(priv->ginfo),
						XV_WIDTH, Vertsb_Endbox_Height(priv->ginfo),
						XV_SHOW, TRUE,
						WIN_FRONT,
						NULL);
		}

		paint_border(vp->owin, vp);
	}
	else {
		if (priv->selected_view) {
			xv_set(priv->sel_owner, SEL_OWN, FALSE, NULL);
		}
		deselect_view(priv);
	}
}

Pkg_private void openwin_lose_selection(Selection_owner owner)
{
    Openwin self = xv_get(owner, XV_OWNER);
	Xv_openwin_info *priv = OPENWIN_PRIVATE(self);

    if (! priv) return;
/*     if (priv->destroying) return; */

    /* the following leads to an endless recursion (from SEL_OWN, FALSE...) */
/*  xv_set(xv_get(owner, XV_OWNER), OPENWIN_SELECTED_VIEW, XV_NULL, NULL); */
    deselect_view(priv);
}

static void paint_resize_handle(Xv_openwin_info *priv, Xv_window rh, int invoked)
{
	if (!priv->ginfo) return;

	olgx_draw_box(priv->ginfo, (Window)xv_get(rh, XV_XID),
			0, 0, (int)xv_get(rh, XV_WIDTH), (int)xv_get(rh, XV_HEIGHT),
			(invoked ? OLGX_INVOKED : OLGX_NORMAL) | OLGX_ERASE, TRUE);
}

static void paint_resize_preview(Xv_openwin_info *priv)
{
	Frame fram = xv_get(OPENWIN_PUBLIC(priv), WIN_FRAME);
	Rect r;
	int sx, sy, ex, ey;

	if (! fram) return;

	r = *((Rect *)xv_get(OPENWIN_PUBLIC(priv), XV_RECT));
	switch (priv->cur_resize) {
		case OPENWIN_TOP:
		case OPENWIN_BOTTOM:
			sx = r.r_left;
			ex = rect_right(&r);
			sy = ey = priv->last_frame_pos;
			break;
		default:
			sy = r.r_top;
			ey = rect_bottom(&r);
			sx = ex = priv->last_frame_pos;
			break;
	}

	XDrawLine((Display *)xv_get(fram, XV_DISPLAY), (Window)xv_get(fram, XV_XID),
							priv->resize_gc, sx, sy, ex, ey);
}

static int calculate_frame_pos(Xv_openwin_info *priv, Event *ev)
{
	int pos;

	switch (priv->cur_resize) {
		case OPENWIN_TOP:
		case OPENWIN_BOTTOM:
			pos = (int)event_y(ev);
			break;
		default:
			pos = (int)event_x(ev);
			break;
	}
	return priv->frame_trans + pos;
}

static int own_resize_drag_verifier(Openwin self, Rect *swr, int new_pos)
{
	Xv_openwin_info *priv = OPENWIN_PRIVATE(self);
	Xv_window ov, sv;
	Rect ovr;
	Scrollbar sb;
	int vdist = 0, hdist = 0;
	int min_sb = AbbScrollbar_Height(priv->ginfo);
	int viewdelta = priv->margin + 2;

	sv = VIEW_PUBLIC(priv->selected_view);
	sb = xv_get(self, OPENWIN_HORIZONTAL_SCROLLBAR, sv);
	if (sb) {
		vdist = (int)xv_get(sb, XV_HEIGHT) - 2;
	}
	vdist += viewdelta + 1;

	sb = xv_get(self, OPENWIN_VERTICAL_SCROLLBAR, sv);
	if (sb) {
		hdist = (int)xv_get(sb, XV_WIDTH) - 2;
	}
	hdist += viewdelta + 1;

	OPENWIN_EACH_VIEW(self, ov)
		ovr = *((Rect *)xv_get(ov, XV_RECT));
		ovr.r_top += (int)xv_get(self, XV_Y);
		ovr.r_left += (int)xv_get(self, XV_X);

		switch (priv->cur_resize) {
			case OPENWIN_TOP:
				/* we consider all views with the same top */
				if (ovr.r_top == swr->r_top) {
					if (new_pos > rect_bottom(&ovr) - min_sb)
						return XV_ERROR;
				}
				/* we consider all views whose bottom
				 * is my top
				 */
				if (rect_bottom(&ovr) + vdist == swr->r_top) {
					if (new_pos < ovr.r_top + min_sb + vdist)
						return XV_ERROR;
				}
				break;
			case OPENWIN_BOTTOM:
				/* we consider all views with the same bottom */
				if (rect_bottom(&ovr) == rect_bottom(swr)) {
					if (new_pos < ovr.r_top + min_sb)
						return XV_ERROR;
				}

				/* we consider all views whose top
				 * is my bottom
				 */
				if (rect_bottom(swr) + vdist == ovr.r_top) {
					if (new_pos > rect_bottom(&ovr) - min_sb - vdist)
						return XV_ERROR;
				}
				break;
			case OPENWIN_LEFT:
				/* we consider all views with the same left */
				if (ovr.r_left == swr->r_left) {
					if (new_pos > rect_right(&ovr)-min_sb)
						return XV_ERROR;
				}
				/* we consider all views whose right
				 * is my left
				 */
				if (rect_right(&ovr) + hdist == swr->r_left) {
					if (new_pos < ovr.r_left + min_sb + hdist)
						return XV_ERROR;
				}
				break;
			case OPENWIN_RIGHT:
				/* we consider all views with the same right */
				if (rect_right(&ovr) == rect_right(swr)) {
					if (new_pos < ovr.r_left + min_sb)
						return XV_ERROR;
				}

				/* we consider all views whose left
				 * is my right
				 */
				if (rect_right(swr) + hdist == ovr.r_left) {
					if (new_pos > rect_right(&ovr)-min_sb - hdist)
						return XV_ERROR;
				}
				break;
		}
	OPENWIN_END_EACH

	return XV_OK;
}

static void resize_views(Openwin self)
{
	Xv_openwin_info *priv = OPENWIN_PRIVATE(self);
	Xv_window ov, sv;
	Rect svr, ovr;
	Scrollbar sb;
	int offset, vdist = 0, hdist = 0;
	int viewdelta = priv->margin + 2;

	/* ich kann das hier nicht als FRAME-Koordinate brauchen */
	offset = priv->last_frame_pos - priv->frame_trans;

	sv = VIEW_PUBLIC(priv->selected_view);
	sb = xv_get(self, OPENWIN_HORIZONTAL_SCROLLBAR, sv);
	if (sb) {
		vdist = (int)xv_get(sb, XV_HEIGHT) - 2; /* INCOMPLETE - 2 ???? */
	}
	vdist += viewdelta + 1;

	sb = xv_get(self, OPENWIN_VERTICAL_SCROLLBAR, sv);
	if (sb) {
		hdist = (int)xv_get(sb, XV_WIDTH) - 2; /* INCOMPLETE - 2 ???? */
	}
	hdist += viewdelta + 1;

	svr = *((Rect *)xv_get(sv, XV_RECT));

	OPENWIN_EACH_VIEW(self, ov)
		if (ov == sv) continue;

		ovr = *((Rect *)xv_get(ov, XV_RECT));

		switch (priv->cur_resize) {
			case OPENWIN_TOP:
				/* we consider all views with the same top */
				if (ovr.r_top == svr.r_top) {
					xv_set(ov,
							XV_Y, ovr.r_top + offset,
							XV_HEIGHT, ovr.r_height - offset,
							NULL);
					sb = xv_get(self, OPENWIN_VERTICAL_SCROLLBAR, ov);
					if (sb) {
						xv_set(sb,
							XV_Y, (int)xv_get(sb, XV_Y) + offset,
							XV_HEIGHT, (int)xv_get(sb, XV_HEIGHT) - offset,
							NULL);
						xv_set(sb,
							SCROLLBAR_VIEW_LENGTH,
								(ovr.r_height - offset) /
									(int)xv_get(sb, SCROLLBAR_PIXELS_PER_UNIT),
	       				NULL);
					}
				}
				/* we consider all views whose bottom
				 * is my top
				 */
				if (rect_bottom(&ovr) + vdist == svr.r_top) {
					xv_set(ov,
							XV_HEIGHT, ovr.r_height + offset,
							NULL);
					sb = xv_get(self, OPENWIN_VERTICAL_SCROLLBAR, ov);
					if (sb) {
						xv_set(sb,
							XV_HEIGHT, (int)xv_get(sb, XV_HEIGHT) + offset,
							NULL);
						xv_set(sb,
							SCROLLBAR_VIEW_LENGTH,
								(ovr.r_height + offset) /
									(int)xv_get(sb, SCROLLBAR_PIXELS_PER_UNIT),
	       				NULL);
					}
					sb = xv_get(self, OPENWIN_HORIZONTAL_SCROLLBAR, ov);
					if (sb) 
						xv_set(sb, XV_Y, (int)xv_get(sb, XV_Y) + offset, NULL);
				}
				break;
			case OPENWIN_BOTTOM:
				/* we consider all views with the same bottom */
				if (rect_bottom(&ovr) == rect_bottom(&svr)) {
					xv_set(ov,
							XV_HEIGHT, ovr.r_height + offset,
							NULL);

					sb = xv_get(self, OPENWIN_VERTICAL_SCROLLBAR, ov);
					if (sb) {
						xv_set(sb,
							XV_HEIGHT, (int)xv_get(sb, XV_HEIGHT) + offset,
							NULL);
						xv_set(sb,
							SCROLLBAR_VIEW_LENGTH,
								(ovr.r_height + offset) /
									(int)xv_get(sb, SCROLLBAR_PIXELS_PER_UNIT),
	       				NULL);
					}

					sb = xv_get(self, OPENWIN_HORIZONTAL_SCROLLBAR, ov);
					if (sb) 
						xv_set(sb, XV_Y, (int)xv_get(sb, XV_Y) + offset, NULL);
				}

				/* we consider all views whose top
				 * is my bottom
				 */
				if (rect_bottom(&svr) + vdist == ovr.r_top) {
					xv_set(ov,
							XV_Y, ovr.r_top + offset,
							XV_HEIGHT, ovr.r_height - offset,
							NULL);

					sb = xv_get(self, OPENWIN_VERTICAL_SCROLLBAR, ov);
					if (sb) {
						xv_set(sb,
							XV_Y, (int)xv_get(sb, XV_Y) + offset,
							XV_HEIGHT, (int)xv_get(sb, XV_HEIGHT) - offset,
							NULL);
						xv_set(sb,
							SCROLLBAR_VIEW_LENGTH,
								(ovr.r_height - offset) /
									(int)xv_get(sb, SCROLLBAR_PIXELS_PER_UNIT),
	       				NULL);
					}
				}
				break;
			case OPENWIN_LEFT:
				/* we consider all views with the same left */
				if (ovr.r_left == svr.r_left) {
					xv_set(ov,
							XV_X, ovr.r_left + offset,
							XV_WIDTH, ovr.r_width - offset,
							NULL);
					sb = xv_get(self, OPENWIN_HORIZONTAL_SCROLLBAR, ov);
					if (sb) {
						xv_set(sb,
							XV_X, (int)xv_get(sb, XV_X) + offset,
							XV_WIDTH, (int)xv_get(sb, XV_WIDTH) - offset,
							NULL);
						xv_set(sb,
							SCROLLBAR_VIEW_LENGTH,
								(ovr.r_width + offset) /
									(int)xv_get(sb, SCROLLBAR_PIXELS_PER_UNIT),
	       				NULL);
					}
				}
				/* we consider all views whose right
				 * is my left
				 */
				if (rect_right(&ovr) + hdist == svr.r_left) {
					xv_set(ov,
							XV_WIDTH, ovr.r_width + offset,
							NULL);
					sb = xv_get(self, OPENWIN_HORIZONTAL_SCROLLBAR, ov);
					if (sb) {
						xv_set(sb,
							XV_WIDTH, (int)xv_get(sb, XV_WIDTH) + offset,
							NULL);
						xv_set(sb,
							SCROLLBAR_VIEW_LENGTH,
								(ovr.r_width + offset) /
									(int)xv_get(sb, SCROLLBAR_PIXELS_PER_UNIT),
	       				NULL);
					}
					sb = xv_get(self, OPENWIN_VERTICAL_SCROLLBAR, ov);
					if (sb) 
						xv_set(sb, XV_X, (int)xv_get(sb, XV_X) + offset, NULL);
				}
				break;
			case OPENWIN_RIGHT:
				/* we consider all views with the same right */
				if (rect_right(&ovr) == rect_right(&svr)) {
					xv_set(ov,
							XV_WIDTH, ovr.r_width + offset,
							NULL);
					sb = xv_get(self, OPENWIN_HORIZONTAL_SCROLLBAR, ov);
					if (sb) {
						xv_set(sb,
							XV_WIDTH, (int)xv_get(sb, XV_WIDTH) + offset,
							NULL);
						xv_set(sb,
							SCROLLBAR_VIEW_LENGTH,
								(ovr.r_width + offset) /
									(int)xv_get(sb, SCROLLBAR_PIXELS_PER_UNIT),
	       				NULL);
					}
					sb = xv_get(self, OPENWIN_VERTICAL_SCROLLBAR, ov);
					if (sb) 
						xv_set(sb, XV_X, (int)xv_get(sb, XV_X) + offset, NULL);
				}

				/* we consider all views whose left
				 * is my right
				 */
				if (rect_right(&svr) + hdist == ovr.r_left) {
					xv_set(ov,
							XV_X, ovr.r_left + offset,
							XV_WIDTH, ovr.r_width - offset,
							NULL);
					sb = xv_get(self, OPENWIN_HORIZONTAL_SCROLLBAR, ov);
					if (sb) {
						xv_set(sb,
							XV_X, (int)xv_get(sb, XV_X) + offset,
							XV_WIDTH, (int)xv_get(sb, XV_WIDTH) - offset,
							NULL);
						xv_set(sb,
							SCROLLBAR_VIEW_LENGTH,
								(ovr.r_width - offset) /
									(int)xv_get(sb, SCROLLBAR_PIXELS_PER_UNIT),
	       				NULL);
					}
				}
				break;
		}
	OPENWIN_END_EACH

	/* now the selected view */
	switch (priv->cur_resize) {
		case OPENWIN_TOP:
			xv_set(sv,
					XV_Y, svr.r_top + offset,
					XV_HEIGHT, svr.r_height - offset,
					NULL);
			sb = xv_get(self, OPENWIN_VERTICAL_SCROLLBAR, sv);
			if (sb) {
				xv_set(sb,
					XV_Y, (int)xv_get(sb, XV_Y) + offset,
					XV_HEIGHT, (int)xv_get(sb, XV_HEIGHT) - offset,
					NULL);
				xv_set(sb,
					SCROLLBAR_VIEW_LENGTH,
							(svr.r_height - offset) /
								(int)xv_get(sb, SCROLLBAR_PIXELS_PER_UNIT),
       				NULL);
			}
			break;
		case OPENWIN_BOTTOM:
			xv_set(sv,
					XV_HEIGHT, svr.r_height + offset,
					NULL);

			sb = xv_get(self, OPENWIN_VERTICAL_SCROLLBAR, sv);
			if (sb) {
				xv_set(sb,
					XV_HEIGHT, (int)xv_get(sb, XV_HEIGHT) + offset,
					NULL);
				xv_set(sb,
					SCROLLBAR_VIEW_LENGTH,
							(svr.r_height + offset) /
								(int)xv_get(sb, SCROLLBAR_PIXELS_PER_UNIT),
       				NULL);
			}

			sb = xv_get(self, OPENWIN_HORIZONTAL_SCROLLBAR, sv);
			if (sb) 
				xv_set(sb, XV_Y, (int)xv_get(sb, XV_Y) + offset, NULL);
			break;
		case OPENWIN_LEFT:
			xv_set(sv,
					XV_X, svr.r_left + offset,
					XV_WIDTH, svr.r_width - offset,
					NULL);
			sb = xv_get(self, OPENWIN_HORIZONTAL_SCROLLBAR, sv);
			if (sb) {
				xv_set(sb,
					XV_X, (int)xv_get(sb, XV_X) + offset,
					XV_WIDTH, (int)xv_get(sb, XV_WIDTH) - offset,
					NULL);
				xv_set(sb,
					SCROLLBAR_VIEW_LENGTH,
							(svr.r_width - offset) /
								(int)xv_get(sb, SCROLLBAR_PIXELS_PER_UNIT),
       				NULL);
			}
			break;
		case OPENWIN_RIGHT:
			xv_set(sv,
					XV_WIDTH, svr.r_width + offset,
					NULL);
			sb = xv_get(self, OPENWIN_HORIZONTAL_SCROLLBAR, sv);
			if (sb) {
				xv_set(sb,
					XV_WIDTH, (int)xv_get(sb, XV_WIDTH) + offset,
					NULL);
				xv_set(sb,
					SCROLLBAR_VIEW_LENGTH,
							(svr.r_width + offset) /
								(int)xv_get(sb, SCROLLBAR_PIXELS_PER_UNIT),
       				NULL);
			}
			sb = xv_get(self, OPENWIN_VERTICAL_SCROLLBAR, sv);
			if (sb) 
				xv_set(sb, XV_X, (int)xv_get(sb, XV_X) + offset, NULL);
			break;
	}

	openwin_update_layout(self);
}

static int own_verification(Xv_openwin_info *priv, int new_pos, Rect *swr)
{
	int min_sb = AbbScrollbar_Height(priv->ginfo);

	switch (priv->cur_resize) {
		case OPENWIN_TOP:
			if (new_pos > rect_bottom(swr) - min_sb) return XV_ERROR;
			break;
		case OPENWIN_BOTTOM:
			if (new_pos < swr->r_top + min_sb) return XV_ERROR;
			break;
		case OPENWIN_LEFT:
			if (new_pos > rect_right(swr) - min_sb) return XV_ERROR;
			break;
		case OPENWIN_RIGHT:
			if (new_pos < swr->r_left + min_sb) return XV_ERROR;
			break;
	}

	return XV_OK;
}

static int verify_drag(Xv_window rh, Xv_openwin_info *priv, Event *ev,
						int *new_posp)
{
	Rect swr;
	int verify_result;

	swr = *((Rect *)xv_get(VIEW_PUBLIC(priv->selected_view), XV_RECT));
	swr.r_top += (int)xv_get(OPENWIN_PUBLIC(priv), XV_Y);
	swr.r_left += (int)xv_get(OPENWIN_PUBLIC(priv), XV_X);

	*new_posp = calculate_frame_pos(priv, ev);
	/* perform own checks */
	verify_result = own_verification(priv, *new_posp, &swr);

	if (verify_result == XV_OK) {
		int appl_resizes = (int)xv_get(rh, RH_APPL_RESIZES);

		if (appl_resizes) {
			if (priv->resize_verify_proc) {
				verify_result = (*(priv->resize_verify_proc))(OPENWIN_PUBLIC(priv),
							priv->cur_resize,
							*new_posp,
							FALSE);
			}
		}
		else {
			verify_result = own_resize_drag_verifier(OPENWIN_PUBLIC(priv), &swr,
												*new_posp);
		}
	}

	return verify_result;
}

static Notify_value resizehandle_events(Xv_window rh, Notify_event event,
							Notify_arg arg, Notify_event_type type)
{
	Event *ev = (Event *)event;
	Openwin self = xv_get(rh, XV_OWNER);
	Xv_openwin_info *priv = OPENWIN_PRIVATE(self);
	int new_pos, verify_result;
	Rect swr, rhr;

	switch (event_action(ev)) {
		case WIN_REPAINT:
			paint_resize_handle(priv, rh, FALSE);
			break;

		case ACTION_SELECT:
			if (event_is_down(ev)) {

				paint_resize_handle(priv, rh, TRUE);
				priv->cur_resize = (Openwin_resize_side)xv_get(rh, RH_SIDE);

				swr = *((Rect *)xv_get(self, XV_RECT));
				rhr = *((Rect *)xv_get(rh, XV_RECT));

				switch (priv->cur_resize) {
					case OPENWIN_TOP:
						priv->frame_trans = swr.r_top + rhr.r_top -
											(int)event_y(ev);
						break;
					case OPENWIN_BOTTOM:
						priv->frame_trans = swr.r_top + rect_bottom(&rhr) -
											(int)event_y(ev);
						break;
					case OPENWIN_LEFT:
						priv->frame_trans = swr.r_left+rhr.r_left -
											(int)event_x(ev);
						break;
					case OPENWIN_RIGHT:
						priv->frame_trans = swr.r_left+rect_right(&rhr) -
											(int)event_x(ev);
						break;
				}

				verify_result = verify_drag(rh, priv, ev, &new_pos);

				if (verify_result == XV_OK) {
					priv->last_frame_pos = new_pos;
					paint_resize_preview(priv);
				}
			}
			else {
				Xv_window pw;
				int appl_resizes = (int)xv_get(rh, RH_APPL_RESIZES);

				paint_resize_preview(priv);
				paint_resize_handle(priv, rh, FALSE);
				if (appl_resizes) {
					if (priv->resize_verify_proc) {
						(void)(*(priv->resize_verify_proc))(self,
										priv->cur_resize,
										calculate_frame_pos(priv, ev),
										TRUE);
					}
				}
				else {
					resize_views(self);
				}

				if ((pw = priv->selected_view->pw)) {
					SERVERTRACE((500, "%s: %s: XClearArea\n", xv_app_name, __FUNCTION__));
					XClearArea((Display *)xv_get(pw, XV_DISPLAY),
							xv_get(pw, XV_XID), 0, 0, 0, 0, TRUE);
				}
			}
			break;

		case LOC_DRAG:
			verify_result = verify_drag(rh, priv, ev, &new_pos);

			if (verify_result == XV_OK) {
				paint_resize_preview(priv);
				priv->last_frame_pos = new_pos;
				paint_resize_preview(priv);
			}

			break;
		case ACTION_HELP:
			if (event_is_down(ev)) {
				xv_help_show(rh, "xview:resize_handles", ev);
			}
			break;
	}

	return notify_next_event_func(rh, event, arg, type);
}

static void make_gcs(Xv_openwin_info *priv, Xv_window pw)
{
	Display *dpy = (Display *)xv_get(pw, XV_DISPLAY);
	Window xid = (Window)xv_get(pw, XV_XID);
	Xv_window view;
	XGCValues   gcv;
	Cms cms;
	int fore_index, back_index;
	unsigned long fg, bg;

	if (priv->resize_gc) XFreeGC(dpy, priv->resize_gc);

	cms = xv_get(pw, WIN_CMS);
	fore_index = (int)xv_get(pw, WIN_FOREGROUND_COLOR);
	back_index = (int)xv_get(pw, WIN_BACKGROUND_COLOR);
	bg = (unsigned long)xv_get(cms, CMS_PIXEL, back_index);
	fg = (unsigned long)xv_get(cms, CMS_PIXEL, fore_index);

	gcv.foreground = fg ^ bg;
	gcv.function = GXxor;
	gcv.subwindow_mode = IncludeInferiors;

	priv->resize_gc = XCreateGC(dpy, xid,
				GCForeground | GCFunction | GCSubwindowMode,
				&gcv);

	view = xv_get(pw, XV_OWNER);
	cms = xv_get(view, WIN_CMS);
	fore_index = (int)xv_get(view, WIN_FOREGROUND_COLOR);
	gcv.foreground = (unsigned long)xv_get(cms, CMS_PIXEL, fore_index);
	gcv.subwindow_mode = IncludeInferiors;
	priv->border_gc = XCreateGC(dpy, xid, GCForeground | GCSubwindowMode, &gcv);
}

extern Graphics_info *xv_init_olgx(Xv_window, int *, Xv_font);

Pkg_private void openwin_select_view(Openwin self, Openwin_view_info *vp)
{
	Xv_openwin_info *priv = OPENWIN_PRIVATE(self);

	/* we want to select a view even in the non-selectable case:
	 * if we have several views
	 */
/* 	if (! STATUS(priv, selectable)) { */
/* 		priv->selected_view = NULL; */
/* 		return; */
/* 	} */

	if (! priv->ginfo) {
		int three_d = FALSE;
		Resize_handle rh;

		three_d = (SCREEN_UIS_3D_COLOR ==
				(screen_ui_style_t)xv_get(XV_SCREEN_FROM_WINDOW(self),
												SCREEN_UI_STYLE));

#ifdef NOT_SO_GOOD
		priv->ginfo = (Graphics_info *)xv_init_olgx(self, &three_d,
													xv_get(self, XV_FONT));
		da schaltet z.B. CANVAS seine WIN_CMS um....
#endif

		make_gcs(priv, vp->pw);

		rh = xv_create(self, RESIZE_HANDLE,
						RH_SIDE, OPENWIN_TOP,
						XV_WIDTH, 2,
						XV_HEIGHT, 2,
						NULL);
		priv->resize_handles[OPENWIN_TOP-1] = rh;

		priv->ginfo = (Graphics_info *)xv_init_olgx(rh, &three_d,
													xv_get(self, XV_FONT));

		priv->resize_handles[OPENWIN_BOTTOM-1]=xv_create(self,RESIZE_HANDLE,
						RH_SIDE, OPENWIN_BOTTOM,
						XV_WIDTH, 2,
						XV_HEIGHT, 2,
						NULL);

		priv->resize_handles[OPENWIN_LEFT-1]=xv_create(self, RESIZE_HANDLE,
						RH_SIDE, OPENWIN_LEFT,
						XV_WIDTH, 2,
						XV_HEIGHT, 2,
						NULL);

		priv->resize_handles[OPENWIN_RIGHT-1]=xv_create(self,RESIZE_HANDLE,
						RH_SIDE, OPENWIN_RIGHT,
						XV_WIDTH, 2,
						XV_HEIGHT, 2,
						NULL);
	}

	select_view(OPENWIN_PRIVATE(self), vp);
}

static int openwin_view_init(Openwin parent, Openwin_view slf,
									Attr_avlist avlist, int *unused)
{
	Xv_openwin_view *self = (Xv_openwin_view *)slf;
	Openwin_view_info *priv = very_transient_created;
	Xv_openwin_info *owin = OPENWIN_PRIVATE(parent);

	very_transient_created = NULL;

	priv->public_self = (Xv_opaque)self;
	self->private_data = (Xv_opaque)priv;

	priv->owin = owin;

	return XV_OK;
}

static Xv_opaque openwin_view_get(Openwin_view self, int *status,
									Attr_attribute attr, va_list valist)
{
	if (attr == OPENWIN_VIEW_PAINT_WINDOW) {
		Openwin_view_info *priv = VIEW_PRIVATE(self);

		*status = XV_OK;
		return priv->pw;
	}
	*status = XV_ERROR;
	return (Xv_opaque)XV_OK;
}

static int openwin_view_destroy(Openwin_view self, Destroy_status stat)
{
	if (stat == DESTROY_CLEANUP) {
		Openwin_view_info *priv = VIEW_PRIVATE(self);
		Scrollbar sb;

		if (priv->owin) {
			if (priv == priv->owin->selected_view) {
				xv_set(OPENWIN_PUBLIC(priv->owin),
						OPENWIN_SELECTED_VIEW, NULL,
						NULL);
				/* very careful: */
				priv->owin->selected_view = 0;
			}
		}
		openwin_unlink_view(priv->owin, priv);
		if (priv->sb[0]) {
			sb = priv->sb[0];
			priv->sb[0] = XV_NULL;

			xv_destroy_status(sb, DESTROY_CLEANUP);
		}
		if (priv->sb[1]) {
			sb = priv->sb[1];
			priv->sb[1] = XV_NULL;

			xv_destroy_status(sb, DESTROY_CLEANUP);
		}
		xv_free(priv);
	}
	return XV_OK;
}


const Xv_pkg xv_openwin_view_pkg = {
    "Open Window View",				/* seal -> package name */
    (Attr_pkg) ATTR_PKG_OPENWIN,	/* openwin attr */
    sizeof(Xv_openwin_view),		/* size of the openwin data struct */
    WINDOW,							/* pointer to parent */
    openwin_view_init,				/* init routine for openwin */
	NULL,
	openwin_view_get,
    openwin_view_destroy,			/* destroy routine */
    NULL							/* No find proc */
};

/********************** resize handle *************************/

typedef struct {
	Xv_window_struct parent_data;
	Xv_opaque private_data;
}  Xv_resize_handle;

typedef struct {
	Xv_opaque public_self;
	int appl_resizes;
	Openwin_resize_side	side;
} Resize_handle_private;

#define RHPRIV(_x_) XV_PRIVATE(Resize_handle_private, Xv_resize_handle, _x_)

static int resize_handle_init(Openwin parent, Resize_handle slf,
									Attr_avlist avlist, int *unused)
{
	Xv_resize_handle *self = (Xv_resize_handle *)slf;
	Resize_handle_private *priv = (Resize_handle_private *)xv_alloc(Resize_handle_private);
	Attr_attribute *attrs;

	priv->public_self = (Xv_opaque)self;
	self->private_data = (Xv_opaque)priv;


	for (attrs=avlist; *attrs; attrs=attr_next(attrs)) switch ((int)*attrs) {
		case RH_SIDE:
			priv->side = (Openwin_resize_side)attrs[1];
			ATTR_CONSUME(*attrs);
			break;
		default: break;
	}

	xv_set(slf,
			WIN_BORDER, FALSE,
			WIN_CONSUME_EVENTS, ACTION_HELP, WIN_MOUSE_BUTTONS, LOC_DRAG, NULL,
			WIN_NOTIFY_IMMEDIATE_EVENT_PROC, resizehandle_events,
			WIN_NOTIFY_SAFE_EVENT_PROC, resizehandle_events,
			XV_SHOW, FALSE,
			NULL);

	return XV_OK;
}

static Xv_opaque resize_handle_get(Resize_handle self, int *status,
									Attr_attribute attr, va_list valist)
{
	Resize_handle_private *priv = RHPRIV(self);

	*status = XV_OK;
	switch ((int)attr) {
		case RH_APPL_RESIZES: return (Xv_opaque)priv->appl_resizes;
		case RH_SIDE: return (Xv_opaque)priv->side;
		default:
			*status = xv_check_bad_attr(RESIZE_HANDLE, attr);
			return (Xv_opaque)XV_OK;
	}
}

static Xv_opaque resize_handle_set(Resize_handle self, Attr_avlist avlist)
{
	Resize_handle_private *priv = RHPRIV(self);
	Attr_attribute *attrs;

	for (attrs=avlist; *attrs; attrs=attr_next(attrs)) switch ((int)*attrs) {
		case RH_APPL_RESIZES:
			priv->appl_resizes = (int)attrs[1];
			ATTR_CONSUME(*attrs);
			break;
		case RH_SIDE:
			priv->side = (Openwin_resize_side)attrs[1];
			ATTR_CONSUME(*attrs);
			break;
		default: xv_check_bad_attr(RESIZE_HANDLE, attrs[0]);
			break;
	}

	return XV_OK;
}

static int resize_handle_destroy(Resize_handle self, Destroy_status stat)
{
	if (stat == DESTROY_CLEANUP) {
		Resize_handle_private *priv = RHPRIV(self);
		xv_free(priv);
	}
	return XV_OK;
}

static const Xv_pkg xv_resize_handle_pkg = {
    "ResizeHandle",
    (Attr_pkg) ATTR_PKG_OPENWIN,
    sizeof(Xv_resize_handle),
    WINDOW,
    resize_handle_init,
    resize_handle_set,
    resize_handle_get,
    resize_handle_destroy,
    NULL
};

static const Xv_pkg *resize_handle_class(void)
{
	return &xv_resize_handle_pkg;
}
