#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)fm_rescale.c 20.21 93/06/28 DRA: $Id: fm_rescale.c,v 4.3 2024/12/05 05:58:12 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

#include <xview_private/fm_impl.h>
#include <xview_private/windowimpl.h>
#include <xview_private/svr_impl.h>
#include <xview/font.h>

/*
 * rescale the sub_tree
 */

Pkg_private void frame_rescale_subwindows(Frame frame_public, int scale)
{
	Frame_class_info *fpriv = FRAME_CLASS_PRIVATE(frame_public);
	register Xv_Window sw;
	Rect new_rect;
	Window_rescale_rect_obj *rect_obj_list;
	int i = 0;
	int num_sws = 0;
	int frame_width, frame_height;

	Event ev;

	event_init(&ev);
	event_set_id(&ev, ACTION_RESCALE);
	event_set_action(&ev, ACTION_RESCALE);
	event_set_window(&ev, frame_public);

	SERVERTRACE((790, "%s\n", __FUNCTION__));

	/*
	 * this is do able only for the frame
	 * xv_set(frame_public,WIN_RESCALE_AND_SIZE,0);
	 */


	/*
	 * if this is not called from the frame_input function then call it. This
	 * gets teh correct font and the correct scale.
	 */

	SERVERTRACE((790, "calling window_default_event_func, font = %ld\n",
									xv_get(frame_public, XV_FONT)));
	window_default_event_func(frame_public, &ev, (Notify_arg)scale, (Notify_event_type) 0);
	SERVERTRACE((790, "after   window_default_event_func, font = %ld\n",
									xv_get(frame_public, XV_FONT)));

	SERVERTRACE((790, "frame size [%d, %d]\n",
							xv_get(frame_public, XV_WIDTH),
							xv_get(frame_public, XV_HEIGHT)));
	window_calculate_new_size(frame_public, frame_public,
			&frame_width, &frame_height);

	SERVERTRACE((790, "frame size [%d, %d]\n", frame_width, frame_height));
#ifdef BEFORE_DRA_FIXED_BUG
	xv_set(frame_public, WIN_RECT, 0);	/* This looks like a XView bug to me */
#endif


	/*
	 * must rescale inner rect but should layout according to outer rect
	 */

	FRAME_EACH_SUBWINDOW(fpriv, sw)	/* count number of sw's */
		num_sws++;
	FRAME_END_EACH

	SERVERTRACE((790, "have %d subwindows\n", num_sws));
	rect_obj_list = window_create_rect_obj_list(num_sws);

	FRAME_EACH_SUBWINDOW(fpriv, sw)
		window_set_rescale_state(sw, scale);
		window_start_rescaling(sw);
		window_add_to_rect_list(rect_obj_list, sw,
							(Rect *) xv_get(sw, WIN_RECT), i);
		i++;
	FRAME_END_EACH

	SERVERTRACE((790, "\n"));
	window_adjust_rects(rect_obj_list, frame_public, num_sws, frame_width, frame_height);
	SERVERTRACE((790, "\n"));

	SERVERTRACE((790, "\n"));
	FRAME_EACH_SUBWINDOW(fpriv, sw)
		if (!window_rect_equal_ith_obj(rect_obj_list, &new_rect, i))
			xv_set(sw, WIN_RECT, &new_rect, NULL);
		window_end_rescaling(sw);
	FRAME_END_EACH

	SERVERTRACE((790, "\n"));
	window_destroy_rect_obj_list(rect_obj_list);
}
