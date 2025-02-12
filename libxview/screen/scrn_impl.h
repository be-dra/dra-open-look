/*	@(#)scrn_impl.h 20.36 93/06/28 SMI    DRA: RCS: $Id: scrn_impl.h,v 4.8 2025/02/12 20:42:35 dra Exp $	*/

/****************************************************************************/
/*	
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL_NOTICE 
 *	file for terms of the license. 
 */
/****************************************************************************/

#ifndef _screen_impl_h_already_included
#define _screen_impl_h_already_included

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <xview/pkg.h>
#include <xview/screen.h>
#include <xview/xv_xrect.h>
#include <xview/window.h>
#include <xview/cms.h>
#include <xview/cursor.h>


Xv_private void screen_adjust_gc_color(Xv_Window window, int gc_index);
Xv_private Xv_Window screen_get_cached_window(Xv_Screen screen_public,
    Notify_func	event_proc, int borders, int transp, Visual *visual,
    int	*new_window);

Xv_private int		screen_get_sun_wm_protocols(Xv_screen);
Xv_private void screen_set_clip_rects(Xv_Screen screen_public, XRectangle *xrect_array, int rect_count);

Xv_private void screen_set_cached_window_busy(Xv_Screen screen_public, Xv_window window, int busy);

Xv_private Xv_xrectlist *screen_get_clip_rects(Xv_Screen screen_public);

#endif
