#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)wmgr_menu.c 20.42 93/06/28 RCSId $Id: wmgr_menu.c,v 2.3 2026/04/18 17:28:54 dra Exp $";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL_NOTICE
 *	file for terms of the license.
 */

#include <xview/frame.h>
#include <xview/wmgr.h>
#include <xview_private/win_info.h>
#include <xview_private/fm_impl.h>

static void wmgr_top_bottom(Frame frame, int link)
{
	Xv_Window window;

	/*
	 * we always get passed the frame.  If frame is currently open, then
	 * bring frame to top; otherwise, it is the frame's icon that is getting
	 * the top request
	 */
	if (!frame_is_iconic(FRAME_CLASS_PRIVATE(frame))) {
		window = frame;
	}
	else {
		Icon icon = xv_get(frame, FRAME_ICON);

		if (icon)
			window = icon;
		else
			return;
	}
	win_setlink(window, link, None);
}

Xv_public void wmgr_top(Frame frame)
{
    wmgr_top_bottom(frame, WL_COVERED);
}

Xv_public void wmgr_bottom(Frame frame)
{
    wmgr_top_bottom(frame, WL_COVERING);
}
