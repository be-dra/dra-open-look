#ifndef lint
char     ow_get_c_sccsid[] = "@(#)ow_get.c 1.25 90/06/21 DRA: $Id: ow_get.c,v 4.1 2024/03/28 18:20:25 dra Exp $ ";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */

/*
 * Package:     openwin
 *
 * Module:      ow_get.c
 *
 * Description: Return values for openwin attributes
 *
 */

#include <xview_private/ow_impl.h>
#include <xview_private/draw_impl.h>
#include <xview_private/portable.h>
#include <xview/sel_pkg.h>

/*-------------------Function Definitions-------------------*/

/*
 * openwin_get - return value for given attribute(s)
 */
Pkg_private Xv_opaque openwin_get(Openwin owin_public, int *get_status, Attr_attribute attr, va_list valist)
{
	Xv_openwin_info *owin = OPENWIN_PRIVATE(owin_public);
	Openwin_view_info *view;
	Xv_opaque v = 0;

	switch (attr) {
		case OPENWIN_NTH_VIEW:
			view = openwin_nth_view(owin, va_arg(valist, int));

			if (view != NULL) {
				v = (Xv_opaque) VIEW_PUBLIC(view);
			}
			else {
				v = XV_NULL;
			}
			break;
		case OPENWIN_SHOW_BORDERS:
			v = (Xv_opaque) STATUS(owin, show_borders);
			break;
		case WIN_VERTICAL_SCROLLBAR:
			view = openwin_nth_view(owin, 0);
			if (view == NULL)
				v = (Xv_opaque) NULL;
			else
				v = (Xv_opaque) openwin_sb(view, SCROLLBAR_VERTICAL);
			break;
		case OPENWIN_NVIEWS:
			v = (Xv_opaque) openwin_count_views(owin);
			break;
		case OPENWIN_VERTICAL_SCROLLBAR:
			view = VIEW_PRIVATE(va_arg(valist, Xv_Window));
			if ((view == NULL) && ((view = openwin_nth_view(owin, 0)) == NULL))
				v = (Xv_opaque) NULL;
			else
				v = (Xv_opaque) openwin_sb(view, SCROLLBAR_VERTICAL);
			break;
		case OPENWIN_HORIZONTAL_SCROLLBAR:
			view = VIEW_PRIVATE(va_arg(valist, Xv_Window));
			if ((view == NULL) && ((view = openwin_nth_view(owin, 0)) == NULL))
				v = (Xv_opaque) NULL;
			else
				v = (Xv_opaque) openwin_sb(view, SCROLLBAR_HORIZONTAL);
			break;
		case OPENWIN_AUTO_CLEAR:
			v = (Xv_opaque) STATUS(owin, auto_clear);
			break;
		case WIN_HORIZONTAL_SCROLLBAR:
			view = openwin_nth_view(owin, 0);
			if (view == NULL)
				v = (Xv_opaque) NULL;
			else
				v = (Xv_opaque) openwin_sb(view, SCROLLBAR_HORIZONTAL);
			break;
		case OPENWIN_ADJUST_FOR_VERTICAL_SCROLLBAR:
			v = (Xv_opaque) STATUS(owin, adjust_vertical);
			break;
		case OPENWIN_ADJUST_FOR_HORIZONTAL_SCROLLBAR:
			v = (Xv_opaque) STATUS(owin, adjust_horizontal);
			break;
		case OPENWIN_VIEW_CLASS:
			v = (Xv_opaque)WINDOW;
			break;
		case OPENWIN_NTH_PW:
			view = openwin_nth_view(owin, va_arg(valist, int));

			if (view != NULL) v = view->pw;
			else v = XV_NULL;
			break;
		case OPENWIN_SELECTABLE:
			v = (Xv_opaque) STATUS(owin, selectable);
			break;
		case OPENWIN_PW_CLASS:
			v = XV_NULL;  /* subclasses ! */
			break;
		case OPENWIN_SEL_OWNER_CLASS:
			v = (Xv_opaque)SELECTION_OWNER;
			break;
		case OPENWIN_RESIZE_VERIFY_PROC:
			v = (Xv_opaque)owin->resize_verify_proc;
			break;
		case OPENWIN_NO_MARGIN:
			v = (Xv_opaque) STATUS(owin, no_margin);
			break;
		case OPENWIN_SELECTED_VIEW:
			if (owin->selected_view) v = VIEW_PUBLIC(owin->selected_view);
			else v = XV_NULL;

		case OPENWIN_SPLIT_INIT_PROC:
			v = (Xv_opaque) (owin->split_init_proc);
			break;
		case OPENWIN_SPLIT_DESTROY_PROC:
			v = (Xv_opaque) (owin->split_destroy_proc);
			break;
		default:
			xv_check_bad_attr(OPENWIN, attr);
			*get_status = XV_ERROR;
	}
	return (v);
}
