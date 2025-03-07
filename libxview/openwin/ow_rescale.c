#ifndef lint
char     ow_rescale_c_sccsid[] = "@(#)ow_rescale.c 1.21 93/06/28 DRA: $Id: ow_rescale.c,v 4.2 2025/03/06 17:36:20 dra Exp $ ";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Package:     openwin
 * 
 * Module:      ow_rescale.c
 *
 * Description: Rescale and openwin
 */

#include <xview_private/ow_impl.h>
#include <xview_private/svr_impl.h>
#include <xview_private/windowimpl.h>
#include <xview/font.h>

/*
 * Package private functions
 */

/*-------------------Function Definitions-------------------*/

/*
 * openwin_rescale - resize the openwin for the given scale
 */
Pkg_private void openwin_rescale(Openwin owin_public, int scale)
{
	Xv_openwin_info *owin = OPENWIN_PRIVATE(owin_public);
	Openwin_view_info *view = owin->views;
	Window_rescale_rect_obj *rect_obj_list;
	int num_views = 0, i = 0;
	int parent_width, parent_height;
	Rect new_rect, parent_new_rect;

	SERVERTRACE((790, "%s: scale=%d\n", __FUNCTION__, scale));
	/*
	 * first change scale unless this has been in the event func
	 */

	parent_new_rect = *(Rect *) xv_get(owin_public, WIN_RECT);
	parent_width = parent_new_rect.r_width;
	parent_height = parent_new_rect.r_height;

	/*
	 * Openwin rect has been set. The rescale has changed the font as well
	 */

	num_views = openwin_count_views(owin);
	rect_obj_list = window_create_rect_obj_list(num_views);

	for (view = owin->views; view != NULL; view = view->next_view) {
		Event event;
		Xv_window v = VIEW_PUBLIC(view);

		event_set_id(&event, ACTION_RESCALE);
		event_set_action(&event, ACTION_RESCALE);
		SERVERTRACE((790, "posting ACTION_RESCALE to %ld\n", v));
    	notify_post_event_and_arg(v, (Notify_event)&event, NOTIFY_IMMEDIATE,
			      (unsigned long)scale, NOTIFY_COPY_NULL, NOTIFY_RELEASE_NULL);

		window_set_rescale_state(v, scale);
		window_start_rescaling(v);
		/* third arg has to be address [vmh - 10/16/90] */
		window_add_to_rect_list(rect_obj_list, v, &view->enclosing_rect, i);
		i++;
	}
	window_adjust_rects(rect_obj_list, owin_public, num_views, parent_width,
			parent_height);
	i = 0;
	for (view = owin->views; view != NULL; view = view->next_view) {
		if (!window_rect_equal_ith_obj(rect_obj_list, &new_rect, i))
			openwin_adjust_view(owin, view, &new_rect);
		window_end_rescaling(VIEW_PUBLIC(view));
	}
	window_destroy_rect_obj_list(rect_obj_list);
}
