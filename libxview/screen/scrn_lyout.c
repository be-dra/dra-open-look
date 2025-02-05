#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)scrn_lyout.c 20.26 93/06/28 DRA: RCS $Id: scrn_lyout.c,v 4.1 2024/03/28 18:27:44 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL_NOTICE 
 *	file for terms of the license.
 */

#include <stdio.h>
#include <xview_private/scrn_impl.h>
#include <xview_private/windowimpl.h>
#include <xview_private/win_info.h>
#include <xview/rect.h>
#include <xview/frame.h>


/*ARGSUSED*/
/*VARARGS3*/
Pkg_private int
screen_layout(Xv_Window root, Xv_Window child, Window_layout_op op, Xv_opaque d1, Xv_opaque d2, Xv_opaque d3, Xv_opaque d4, Xv_opaque d5)
{
	int top_level = (int)xv_get(child, WIN_TOP_LEVEL);
	int result;

	/*
	 * use the default if not a top_level win, but don't insert on
	 * WIN_CREATE.
	 */
	if (!top_level)
		return (op == WIN_CREATE) ?
				FALSE : window_layout(root, child, op, d1, d2, d3, d4, d5);

	switch (op) {
		case WIN_CREATE:
			return FALSE;

		case WIN_GET_BELOW:
			/*
			 * window_getrelrect(child, (Xv_Window) d1, &rect); rect1 = *(Rect *)
			 * xv_get(child,WIN_RECT); rect1.r_top = rect.r_top + rect.r_height +
			 * FRAME_BORDER_WIDTH; d1 = (int) &rect1;
			 */
			/* bogus -- portability problems */
			/*
			 * op = WIN_ADJUST_RECT;
			 */
			break;

		case WIN_GET_RIGHT_OF:
			/*
			 * window_getrelrect(child, (Xv_Window) d1, &rect); rect1 = *(Rect *)
			 * xv_get(child,WIN_RECT); rect1.r_left = rect.r_left + rect.r_width
			 * + FRAME_BORDER_WIDTH; d1 = (int) &rect1;
			 */
			/* bogus -- portability problems */
			/*
			 * op = WIN_ADJUST_RECT;
			 */
			break;

		case WIN_ADJUST_RECT:
			if (xv_get(child, XV_IS_SUBTYPE_OF, FRAME_CLASS)) {
				Rect *r = (Rect *) d1;
				Rect real_size;
				int rect_info = (int)xv_get(child, WIN_RECT_INFO);

				if (!(rect_info & WIN_HEIGHT_SET)) {
					win_getsize(child, &real_size);
					r->r_height = real_size.r_height;
				}
			}
			break;

		default:
			break;
	}

	if ((op == WIN_ADJUST_RECT) &&
			(top_level) && !(Bool) xv_get(child, WIN_TOP_LEVEL_NO_DECOR)) {

		typedef int (*layout_proc_t)(Xv_Window,Xv_Window,Window_layout_op,Xv_opaque,Xv_opaque,Xv_opaque,Xv_opaque,Xv_opaque);
		layout_proc_t layout_proc;

		layout_proc = (layout_proc_t)xv_get(child, WIN_LAYOUT_PROC);
		result = layout_proc(root, child, op, d1, d2, d3, d4, d5);
	}
	else
		result = window_layout(root, child, op, d1, d2, d3, d4, d5);
	return result;
}
