#ifndef lint
char     ow_evt_c_sccsid[] = "@(#)ow_evt.c 1.30 93/06/28 DRA: $Id: ow_evt.c,v 4.1 2024/03/28 18:20:25 dra Exp $ ";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */

/*
 * Module:	ow_evt.c
 *
 * Package:     openwin
 *
 * Description: Handles events to both the openwin window, and the view window.
 *
 */

#include <xview_private/ow_impl.h>

/*-------------------Function Definitions-------------------*/

/*
 * openwin_event - event handler for openwin
 */
Pkg_private Notify_value openwin_event(Openwin owin_public, Notify_event ev,
    Notify_arg arg, Notify_event_type type)
{
    Event *event = (Event *)ev;
	Xv_openwin_info *owin = OPENWIN_PRIVATE(owin_public);
	Rect r;

	switch (event_action(event)) {
		case ACTION_RESCALE:
			openwin_rescale(owin_public, (int)arg);
			break;

		case WIN_RESIZE:
			r = *(Rect *) xv_get(owin_public, WIN_RECT);
			openwin_adjust_views(owin, &r);
			owin->cached_rect = r;
			break;
		case WIN_REPAINT:
			/* Enable painting in openwin_paint_border */
			STATUS_SET(owin, mapped);
			break;

		default:
			break;
	}

	return notify_next_event_func(owin_public, ev, arg, type);
}
