#ifndef lint
char     ow_set_c_sccsid[] = "@(#)ow_set.c 1.49 91/05/23 DRA: $Id: ow_set.c,v 4.5 2025/07/22 16:22:55 dra Exp $ ";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */

/*
 * Package:     openwin
 *
 * Module:      ow_set.c
 *
 * Description: Set values for openwin's attributes
 *
 */

#include <xview_private/ow_impl.h>
#include <xview_private/draw_impl.h>
#include <xview_private/attr_impl.h>
#include <xview_private/sb_impl.h>
#include <xview_private/windowimpl.h>
#include <xview/font.h>

/*
 * Package private functions
 */

/*
 * Module private functions
 */
static Xv_opaque ow_parse_split_attrs(Xv_openwin_info *owin, Attr_avlist avlist);
static Xv_opaque ow_set_scrollbar(Xv_openwin_info *owin, Scrollbar sb, Scrollbar_setting direction);
static void ow_append_view_attrs(Xv_openwin_info *owin, Attr_avlist argv);
static void ow_set_width(Xv_openwin_info *owin, int ncols);
static void ow_set_height(Xv_openwin_info *owin, int nrows);

#define ADONE ATTR_CONSUME(avlist[0]);break

/*-------------------Function Definitions-------------------*/

/*
 * openwin_set - handle xv_set for an openwin
 */
Pkg_private Xv_opaque openwin_set(Openwin self, Attr_avlist avlist)
{
	Xv_openwin_info *owin = OPENWIN_PRIVATE(self);
	Attr_attribute attr;
	Rect r;
	Xv_Window view, pw;

	Xv_opaque retval = XV_OK;


	for (attr = avlist[0]; attr; avlist = attr_next(avlist), attr = avlist[0]) {
		switch (attr) {
			case WIN_COLUMNS:
				/* If you intend to attach a vertical scrollbar to this openwin,
				 * and you want the view to be the WIN_COLUMNS specified, then
				 * you must specify
				 * OPENWIN_ADJUST_FOR_VERTICAL_SCROLLBAR, TRUE,
				 */
				if (STATUS(owin, created))
					ow_set_width(owin, (int)avlist[1]);
				else
					owin->nbr_cols = (int)avlist[1];
				ADONE;

			case WIN_ROWS:
				/* If you intend to attach a horizontal scrollbar to this openwin,
				 * and you want the view to be the WIN_ROWS specified, then
				 * you must specify
				 * OPENWIN_ADJUST_FOR_HORIZONTAL_SCROLLBAR, TRUE,
				 */
				if (STATUS(owin, created))
					ow_set_height(owin, (int)avlist[1]);
				else
					owin->nbr_rows = (int)avlist[1];
				ADONE;

			case WIN_VERTICAL_SCROLLBAR:
				if ((Scrollbar) avlist[1] != XV_NULL) {
					STATUS_SET(owin, adjust_vertical);
				}
				else {
					STATUS_RESET(owin, adjust_vertical);
				}
				if (STATUS(owin, created)) {
					(void)ow_set_scrollbar(owin, (Scrollbar) avlist[1],
							SCROLLBAR_VERTICAL);
				}
				else {
					owin->vsb_on_create = (Scrollbar) avlist[1];
				}
				break;
			case WIN_HORIZONTAL_SCROLLBAR:
				if ((Scrollbar) avlist[1] != XV_NULL) {
					STATUS_SET(owin, adjust_horizontal);
				}
				else {
					STATUS_RESET(owin, adjust_horizontal);
				}
				if (STATUS(owin, created)) {
					(void)ow_set_scrollbar(owin, (Scrollbar) avlist[1],
							SCROLLBAR_HORIZONTAL);
				}
				else {
					owin->hsb_on_create = (Scrollbar) avlist[1];
				}
				break;
			case OPENWIN_NO_MARGIN:
				if ((int)avlist[1] == 0) {
					STATUS_RESET(owin, no_margin);
				}
				else {
					STATUS_SET(owin, no_margin);
				}
				if (STATUS(owin, created))
					openwin_adjust_views(owin, &owin->cached_rect);
				ADONE;

			case OPENWIN_SHOW_BORDERS:
				if (!STATUS(owin, created)) {
					if ((int)avlist[1] == 0) {
						STATUS_RESET(owin, show_borders);
					}
					else {
						STATUS_SET(owin, show_borders);
					}
				}
				else {
					xv_error(self,
							ERROR_CREATE_ONLY, attr,
							ERROR_PKG, OPENWIN,
							NULL);
				}
				ADONE;

			case OPENWIN_SELECTED_VIEW:
				{
					Openwin_view_info *viewinfo;

					if (avlist[1] == XV_NULL)
						viewinfo = NULL;
					else
						openwin_viewdata_for_view((Xv_Window)avlist[1],
														&viewinfo);
					openwin_select_view(self, viewinfo);
				}
				ADONE;

			case OPENWIN_AUTO_CLEAR:
				if ((int)avlist[1] == 0) {
					STATUS_RESET(owin, auto_clear);
				}
				else {
					STATUS_SET(owin, auto_clear);
				}
				ADONE;
			case OPENWIN_ADJUST_FOR_VERTICAL_SCROLLBAR:
				if ((int)avlist[1] != STATUS(owin, adjust_vertical) &&
						(int)avlist[1] == FALSE) {
					STATUS_RESET(owin, adjust_vertical);
				}
				else if ((int)avlist[1] != STATUS(owin, adjust_vertical)) {
					STATUS_SET(owin, adjust_vertical);
				}
				if (STATUS(owin, created)) {
					r = *(Rect *) xv_get(OPENWIN_PUBLIC(owin), WIN_RECT);
					openwin_adjust_views(owin, &r);
				}
				ADONE;
			case OPENWIN_ADJUST_FOR_HORIZONTAL_SCROLLBAR:
				if ((int)avlist[1] != STATUS(owin, adjust_horizontal) &&
						(int)avlist[1] == FALSE) {
					STATUS_RESET(owin, adjust_horizontal);
				}
				else if ((int)avlist[1] != STATUS(owin, adjust_horizontal)) {
					STATUS_SET(owin, adjust_horizontal);
				}
				if (STATUS(owin, created)) {
					r = *(Rect *) xv_get(OPENWIN_PUBLIC(owin), WIN_RECT);
					openwin_adjust_views(owin, &r);
				}
				ADONE;
			case OPENWIN_VIEW_ATTRS:
				if (STATUS(owin, created)) {
					OPENWIN_EACH_VIEW(OPENWIN_PUBLIC(owin), view)
						xv_set_avlist(view, &(avlist[1]));
					OPENWIN_END_EACH
				}
				else {
					ow_append_view_attrs(owin, &(avlist[1]));
				}
				ADONE;
			case OPENWIN_PW_ATTRS:
				if (STATUS(owin, created)) {
					OPENWIN_EACH_PW(OPENWIN_PUBLIC(owin), pw)
						xv_set_avlist(pw, &(avlist[1]));
					OPENWIN_END_EACH
				}
				else {
					if (owin->pw_attrs == NULL) {
						/* No current list, so allocate a new one */
						owin->pw_attrs = xv_alloc_n(Attr_attribute,
											(size_t)ATTR_STANDARD_SIZE);
						owin->pw_end_attrs = owin->pw_attrs;
					}
					owin->pw_end_attrs = (Attr_avlist)attr_copy_avlist(
									owin->pw_end_attrs, avlist + 1);
				}
				ADONE;
			case OPENWIN_SELECTABLE:
				if (avlist[1]) {
					STATUS_SET(owin, selectable);
				}
				else {
					STATUS_RESET(owin, selectable);
				}
				ADONE;
			case OPENWIN_RESIZABLE:
                {
                    int i;

                    owin->resize_sides = 0;
                    for (i = 1; avlist[i]; i++) {
                        owin->resize_sides |= (1 << ((int)avlist[i] - 1));
                    }
                }
				ADONE;
			case OPENWIN_RESIZE_VERIFY_PROC:
                owin->resize_verify_proc = (openwin_resize_verification_t)avlist[1];
				ADONE;
			case OPENWIN_SPLIT:
				if (ow_parse_split_attrs(owin, &(avlist[1])) != XV_OK) {
					/* handle error */
				}
				ADONE;

#ifndef NO_OPENWIN_PAINT_BG
				/* catch any attempts to change background color */
			case WIN_CMS:
			case WIN_CMS_NAME:
			case WIN_CMS_DATA:
			case WIN_FOREGROUND_COLOR:
			case WIN_BACKGROUND_COLOR:
			case WIN_COLOR_INFO:
				{
					Xv_opaque defaults_array[ATTR_STANDARD_SIZE];

					if (STATUS(owin, paint_bg)) {
						defaults_array[0] = avlist[0];
						defaults_array[1] = avlist[1];
						defaults_array[2] = (Xv_opaque) 0;
						window_set_avlist(OPENWIN_PUBLIC(owin), defaults_array);
						ATTR_CONSUME(avlist[0]);
						openwin_set_bg_color(OPENWIN_PUBLIC(owin));
					}
				}
				break;
#endif /* NO_OPENWIN_PAINT_BG */

			case WIN_SET_FOCUS:
				retval = XV_ERROR;
				if (owin->last_focus_pw) {
					win_set_kbd_focus(owin->last_focus_pw,
								(Window)xv_get(owin->last_focus_pw, XV_XID));
					retval = XV_OK;
				}
				else {
					Xv_window pw;

					OPENWIN_EACH_PW(self, pw)
						if (win_getinputcodebit((Inputmask *)xv_get(pw,
										WIN_INPUT_MASK), KBD_USE)) {
							win_set_kbd_focus(pw, (Window)xv_get(pw, XV_XID));
							retval = XV_OK;
							break;
						}
					OPENWIN_END_EACH
				}
				ADONE;

			case XV_END_CREATE:
				/* openwin size if now correct */
				owin->cached_rect =
						*(Rect *) xv_get(OPENWIN_PUBLIC(owin), WIN_RECT);
				openwin_create_initial_view(owin);
				/* Note: ow_set_width and ow_set_height will each
				 * generate a WIN_RESIZE event on the openwin because they change
				 * the openwin's width and height, respectively.  This will cause
				 * openwin_adjust_views to be called, which calls
				 * openwin_adjust_view, which calls
				 * openwin_view_rect_from_avail_rect, which calls
				 * openwin_adjust_view_scrollbars, which changes the size of the
				 * view to make room for the scrollbar(s).
				 *
				 * N.B.:  The size of the view window will always be determined
				 * by the size of the enclosing openwin.  If adjust_{vertical,
				 * horizontal} is set, then the view is shrunk by the width of
				 * the scrollbar that would go there.  This adjustment is made
				 * in openwin_adjust_view_scrollbars.
				 * openwin_adjust_view_scrollbars is eventually called in the
				 * following situations:
				 *  - from openwin_create_initial_view
				 *  - a WIN_RESIZE event
				 *  - frow ow_set_width (causes a WIN_RESIZE)
				 *  - from ow_set_height (causes a WIN_RESIZE)
				 *  - from ow_set_scrollbar
				 */
				if (owin->nbr_cols > 0)
					ow_set_width(owin, owin->nbr_cols);
				if (owin->nbr_rows > 0)
					ow_set_height(owin, owin->nbr_rows);
				STATUS_SET(owin, created);
				{
					const Xv_pkg *socl = (const Xv_pkg *)xv_get(self, OPENWIN_SEL_OWNER_CLASS);
					owin->sel_owner = xv_create(self, socl,
								SEL_RANK, XA_PRIMARY,
								SEL_LOSE_PROC, openwin_lose_selection,
								NULL);
				}
				break;
			default:
				xv_check_bad_attr(OPENWIN, attr);
				break;
		}
	}
	return retval;
}

/*
 * ow_parse_split_attrs - parse split args, then split the view
 */
static Xv_opaque ow_parse_split_attrs(Xv_openwin_info *owin, Attr_avlist avlist)
{
	Attr_attribute attr;
	Openwin_split_direction split_direction = OPENWIN_SPLIT_HORIZONTAL;
	Openwin_view_info *view;
	Xv_Window split_view = XV_NULL;
	int split_position = 0;
	int split_viewstart = OPENWIN_SPLIT_NEWVIEW_IN_PLACE;
	Rect r;
	Scrollbar vsb, hsb;
	int min_size;

	for (attr = avlist[0]; attr; avlist = attr_next(avlist), attr = avlist[0]) {
		switch (attr) {
			case OPENWIN_SPLIT_DIRECTION:
				split_direction = (Openwin_split_direction) avlist[1];
				break;
			case OPENWIN_SPLIT_VIEW:
				split_view = (Xv_Window) avlist[1];
				break;
			case OPENWIN_SPLIT_POSITION:
				split_position = (int)avlist[1];
				break;
			case OPENWIN_SPLIT_INIT_PROC:
				owin->split_init_proc = (openwin_split_init_proc)avlist[1];
				break;

			case OPENWIN_SPLIT_DESTROY_PROC:
				owin->split_destroy_proc =(openwin_split_destroy_proc)avlist[1];
				break;

			case OPENWIN_SPLIT_VIEW_START:
				split_viewstart = (int)avlist[1];
				break;
			default:
				xv_check_bad_attr(OPENWIN, attr);
				break;
		}
	}

	/* do data validation */

	/* see if a window was passed to be split and if it is valid */
	if (split_view == XV_NULL
			|| openwin_viewdata_for_view(split_view, &view) != XV_OK) {
		/* error invalid view */
		return (XV_ERROR);
	}

	/* see if position is one in the window */
	vsb = openwin_sb(view, SCROLLBAR_VERTICAL);
	hsb = openwin_sb(view, SCROLLBAR_HORIZONTAL);
	r = *(Rect *) xv_get(split_view, WIN_RECT);
	if (split_direction == OPENWIN_SPLIT_VERTICAL) {
		if (hsb) {
			min_size = scrollbar_minimum_size(hsb);
			if (vsb)
				min_size += (int)xv_get(vsb, XV_WIDTH);
		}
		else
			min_size = OPENWIN_SPLIT_VERTICAL_MINIMUM;
		if (split_position < min_size || split_position > r.r_width - min_size)
			/* error invalid position */
			return (XV_ERROR);
	}
	else {
		if (vsb) {
			min_size = scrollbar_minimum_size(vsb);
			if (hsb)
				min_size += (int)xv_get(hsb, XV_HEIGHT);
		}
		else
			min_size = OPENWIN_SPLIT_HORIZONTAL_MINIMUM;
		if (split_position < min_size || split_position > r.r_height - min_size)
			/* error invalid position */
			return (XV_ERROR);
	}
	/* see if view start is valid */
	if (split_viewstart == OPENWIN_SPLIT_NEWVIEW_IN_PLACE) {
		Scrollbar sb = (split_direction == OPENWIN_SPLIT_VERTICAL) ? hsb : vsb;

		if (sb) {
			split_viewstart =
					(int)xv_get(sb, SCROLLBAR_VIEW_START) + split_position;
		}
		else {
			split_viewstart = split_position;
		}
	}
	openwin_split_view(owin, view, split_direction, split_position,
			split_viewstart);
	return (XV_OK);
}

/*
 * ow_set_scrollbar - give a scrollbar to an owin.  If sb is null, then
 *                    destroy all scrollbars of that direction.
 */
static Xv_opaque ow_set_scrollbar(Xv_openwin_info *owin, Scrollbar sb, Scrollbar_setting direction)
{
	Openwin_view_info *view = owin->views;
	Rect r;
	int view_length;
	Xv_opaque result = (Xv_opaque) XV_OK;
	Xv_opaque sb_notify_client;

	/* give the vertical scrollbar to the first view */
	if (sb != XV_NULL) {
		/* if we already have a scrollbar report an error */
		while (view != NULL) {
			if (openwin_sb(view, direction) != XV_NULL) {
				/* FATAL ERROR */
				return ((Xv_opaque) XV_ERROR);
			}
			view = view->next_view;
		}

		/* Reparent the scrollbar if necessary for
		 * SunView compatibility.  Must be done before
		 * placing the scrollbar.
		 */
		if (xv_get(sb, WIN_PARENT) != OPENWIN_PUBLIC(owin) ||
				xv_get(sb, XV_OWNER) != OPENWIN_PUBLIC(owin)) {
			xv_set(sb, WIN_PARENT, OPENWIN_PUBLIC(owin),
					XV_OWNER, OPENWIN_PUBLIC(owin), NULL);
		}

		/* give this scrollbar to first view */
		openwin_set_sb(owin->views, direction, sb);

		/* Adjust the size of the view and place the scrollbar. */
		r = owin->views->enclosing_rect;
		openwin_adjust_view(owin, owin->views, &r);

		view_length = (direction == SCROLLBAR_VERTICAL) ?
				(int)xv_get(VIEW_PUBLIC(owin->views), WIN_HEIGHT) :
				(int)xv_get(VIEW_PUBLIC(owin->views), WIN_WIDTH);
		view_length = view_length / (int)xv_get(sb, SCROLLBAR_PIXELS_PER_UNIT);

		xv_set(sb,
				SCROLLBAR_DIRECTION, direction,
				SCROLLBAR_VIEW_LENGTH, view_length, XV_SHOW, TRUE, NULL);

		sb_notify_client = xv_get(sb, SCROLLBAR_NOTIFY_CLIENT);
		if (sb_notify_client == XV_NULL ||
				sb_notify_client == OPENWIN_PUBLIC(owin)) {
			xv_set(sb, SCROLLBAR_NOTIFY_CLIENT, VIEW_PUBLIC(owin->views), NULL);
		}
		/* create new scrollbars for other views */
		view = owin->views->next_view;
		while (view != NULL) {
			openwin_copy_scrollbar(owin, sb, view);
			r = view->enclosing_rect;
			openwin_adjust_view(owin, view, &r);
			view = view->next_view;
		}
	}
	else {
		/* remove all scrollbars */
		/* set bit so layout code which removes sb's  */
		/* isn't invoked */
		/* for each view unset as having sb and adjust view */
		for (view = owin->views; view != NULL; view = view->next_view) {
			sb = openwin_sb(view, direction);
			openwin_set_sb(view, direction, XV_NULL);
			if (sb != XV_NULL) {
				xv_destroy(sb);
			}
		}
		r = *(Rect *) xv_get(OPENWIN_PUBLIC(owin), WIN_RECT);
		openwin_adjust_views(owin, &r);
	}
	return (result);
}

/*
 * ow_append_view_attrs - add view attrs to the cached view avlist.
 */
static void ow_append_view_attrs(Xv_openwin_info *owin, Attr_avlist argv)
{
	if (owin->view_attrs == NULL) {
		/* No current list, so allocate a new one */
		owin->view_attrs = xv_alloc_n(Attr_attribute,
										(size_t)ATTR_STANDARD_SIZE);

		owin->view_end_attrs = owin->view_attrs;
	}
	owin->view_end_attrs = attr_copy_avlist(owin->view_end_attrs,
														argv);
}

/*
 * ow_set_width - set the width of the openwin to the ncols columns of text.
 */
static void ow_set_width(Xv_openwin_info *owin, int ncols)	/* number of columns */
{
	Openwin owin_public = OPENWIN_PUBLIC(owin);
	Scrollbar sb = openwin_sb(owin->views, SCROLLBAR_VERTICAL);
	int sb_width;
	int width;

	if (sb)
		sb_width = (int)xv_get(sb, XV_WIDTH, 0);
	else if (STATUS(owin, adjust_vertical)) {
		sb_width = scrollbar_width_for_owner(owin_public);
	}
	else
		sb_width = 0;
	width = (int)xv_cols(VIEW_PUBLIC(owin->views), ncols) +
			(STATUS(owin, no_margin) ? 0 :
			(int)xv_get(owin_public, XV_LEFT_MARGIN) +
			(int)xv_get(owin_public, XV_RIGHT_MARGIN)) +
			sb_width +
			2 * owin->margin +
			2 * openwin_border_width(owin_public, VIEW_PUBLIC(owin->views));
	if ((int)xv_get(owin_public, XV_WIDTH) != width)
		xv_set(owin_public, XV_WIDTH, width, NULL);
}

static void ow_set_height(Xv_openwin_info *owin, int nrows)	/* number of columns */
{
	Openwin owin_public = OPENWIN_PUBLIC(owin);
	Scrollbar sb = openwin_sb(owin->views, SCROLLBAR_HORIZONTAL);
	int sb_height;
	int height;

	if (sb)
		sb_height = (int)xv_get(sb, XV_WIDTH);
	else if (STATUS(owin, adjust_horizontal)) {
		sb_height = scrollbar_width_for_owner(owin_public);
	}
	else
		sb_height = 0;
	height = (int)xv_rows(VIEW_PUBLIC(owin->views), nrows) +
			(STATUS(owin, no_margin) ? 0 :
			(int)xv_get(owin_public, WIN_TOP_MARGIN) +
			(int)xv_get(owin_public, WIN_BOTTOM_MARGIN)) +
			sb_height +
			2 * owin->margin +
			2 * openwin_border_width(owin_public, VIEW_PUBLIC(owin->views));

	if ((int)xv_get(owin_public, XV_HEIGHT) != height)
		xv_set(owin_public, XV_HEIGHT, height, NULL);
}
