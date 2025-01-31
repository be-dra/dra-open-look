/*	@(#)scrn_impl.h 20.36 93/06/28 SMI    DRA: RCS: $Id: scrn_impl.h,v 4.7 2025/01/09 16:54:26 dra Exp $	*/

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

typedef struct scrn_cache_window {
    Xv_window			window;
    short			busy;
    short			borders;
    Visual			*visual;
	short           transparent;
    struct scrn_cache_window	*next;
} Xv_cached_window;

typedef struct scrn_gc_list {
    int			 depth;
    GC			 gcs[SCREEN_OLGC_LIST_SIZE];
    struct scrn_gc_list *next;
} Screen_OLGC_List;

struct screen_visual;

typedef struct _screen_info {
    Xv_Screen		 public_self;		/* back pointer */
    int			 number;		/* screen number on server */
    Xv_opaque		 server;		/* always a Server */
    Xv_opaque		 root_window;		/* always a Window */
    XVisualInfo	        *visual_infos;	 	/* List of available visuals
						 * for this screen */
    int			 num_visuals;	  	/* number of available visuals
						 * for this screen */
    struct screen_visual *screen_visuals;	/* list of screen visuals
						 * (first is always default) */
    Cms			 default_cms;		
    Xv_xrectlist         clip_xrects;    	/* clipping rectangle list */
    Xv_cached_window    *cached_windows;
    short                retain_windows;  	/* retain leaf windows for
						 * perf */
    Screen_OLGC_List	*gc_list;		/* List of gc lists */
    Atom *sun_wm_protocols;	/* Sun specific WM_PROTOCOLS */
    int	num_sun_wm_protocols;	/* No. of protocols in above */
	unsigned long input_pixel;
	int olwm_managed;
	screen_ui_style_t ui_style;
	/* atoms I want to know in order to handle them */
	Atom sun_wm_p;
	Xv_Cursor busy_pointer;
} Screen_info;

#define	SCREEN_PRIVATE(screen)	\
	XV_PRIVATE(Screen_info, Xv_screen_struct, screen)
#define	SCREEN_PUBLIC(screen)	XV_PUBLIC(screen)


/* screen_get.c */
Pkg_private Xv_opaque screen_get_attr(Xv_Screen screen_public, int *status, Attr_attribute attr, va_list args);
Pkg_private XVisualInfo *screen_match_visual_info(Screen_info *screen, long mask, XVisualInfo *template);
Xv_private void screen_adjust_gc_color(Xv_Window window, int gc_index);
Xv_private Xv_Window screen_get_cached_window(Xv_Screen screen_public,
    Notify_func	event_proc, int borders, int transp, Visual *visual,
    int	*new_window);

/* screen.c */
Xv_private int		screen_get_sun_wm_protocols(Xv_screen);
Xv_private void screen_set_clip_rects(Xv_Screen screen_public, XRectangle *xrect_array, int rect_count);
Xv_private void screen_update_sun_wm_protocols(Xv_object window, int is_delete);

/* screen_layout.c */
Pkg_private int screen_layout(Xv_Window root, Xv_Window child, Window_layout_op op, Xv_opaque d1, Xv_opaque d2, Xv_opaque d3, Xv_opaque d4, Xv_opaque d5);
Xv_private void screen_set_cached_window_busy(Xv_Screen screen_public, Xv_window window, int busy);

Xv_private Xv_xrectlist *screen_get_clip_rects(Xv_Screen screen_public);

#endif
