#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)frame_help.h 1.27 93/06/28 DRA: $Id: frame_help.h,v 4.3 2024/11/30 12:38:45 dra Exp $ ";
#endif
#endif

/***********************************************************************/
/*	                      frame_help.h		               */
/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL_NOTICE
 *	file for terms of the license.
 */
/***********************************************************************/

#ifndef _frame_help_h_already_included
#define _frame_help_h_already_included

/* standard includes */
#ifndef FILE
#if !defined(SVR4) && !defined(__linux)
#undef NULL
#endif  /* SVR4 */
#include <stdio.h>
#endif  /* FILE */
#include <sys/time.h>
#include <xview/notify.h>
#include <xview/rect.h>
#include <xview/rectlist.h>

#include <xview/win_struct.h>	/* for WL_ links */
#include <xview/win_input.h>

/* all this for wmgr.h */
#include <xview/win_screen.h>
#include <xview/wmgr.h>
#include <xview_private/wmgr_decor.h>

#include <xview/pkg.h>
#include <xview/attrol.h>
#include <xview/frame.h>
#include <xview/icon.h>
#include <xview/openmenu.h>

/* all this for XWMHints */
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#define	FRAME_HELP_PRIVATE(f)	XV_PRIVATE(Frame_help_info, Xv_frame_help, f)
#define FRAME_HELP_PUBLIC(f)	XV_PUBLIC(f)
#define FRAME_CLASS_FROM_HELP(f) FRAME_PRIVATE(FRAME_HELP_PUBLIC(f))

#define FRAME_HELP_FLAGS  WMDecorationHeader

typedef	struct	{
    Frame	public_self;	/* back pointer to object */
    WM_Win_Type	win_attr;	/* _OL_WIN_ATTR */

    struct {
	BIT_FIELD(show_label); 		/* show label or not */
	BIT_FIELD(show_resize_corner);	/* show resize corner or not */
    } status_bits;
} Frame_help_info;

/* frame_help_get.c */
Pkg_private Xv_opaque frame_help_get_attr(Frame frame_public, int *status, Attr_attribute attr, va_list valist);

/* frame_help_set.c */
Pkg_private Xv_opaque frame_help_set_avlist(Frame frame_public, Attr_attribute avlist[]);

/* frame_help_destroy.c */
Pkg_private int frame_help_destroy(Frame frame_public, Destroy_status status);

#endif
