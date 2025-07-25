#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)frame_base.h 1.30 93/06/28 DRA: $Id: frame_base.h,v 4.3 2025/07/23 16:48:22 dra Exp $ ";
#endif
#endif

/***********************************************************************/
/*	                      frame_base.h		               */
/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL_NOTICE
 *	file for terms of the license.
 */
/***********************************************************************/

#ifndef _frame_base_h_already_included
#define _frame_base_h_already_included

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
#include <xview_private/fm_impl.h>
#include <xview/icon.h>
#include <xview/openmenu.h>

/* all this for XWMHints */
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#define	FRAME_BASE_PRIVATE(f)	XV_PRIVATE(Frame_base_info, Xv_frame_base, f)
#define FRAME_BASE_PUBLIC(f)	XV_PUBLIC(f)
#define FRAME_CLASS_FROM_BASE(f) FRAME_PRIVATE(FRAME_BASE_PUBLIC(f))

#define FRAME_BASE_FLAGS  WMDecorationHeader | WMDecorationCloseButton | \
                          WMDecorationResizeable

typedef	struct	{
    Frame		 public_self;	/* back pointer to object */
	frame_props_proc_t props_proc;
    WM_Win_Type		 win_attr;	/* _OL_WIN_ATTR */
    char		**cmd_line_strings;
    int			cmd_line_strings_count;
    struct {
	BIT_FIELD(show_label); 		/* show label or not */
	BIT_FIELD(props_active);
	BIT_FIELD(show_resize_corner);  /* show resize corner or not */
    } status_bits;
} Frame_base_info;

/* frame_base.c */
Pkg_private int frame_base_init(Xv_Window owner, Frame frame_public, Attr_attribute avlist[], int *u);

/* fmbs_get.c */
Pkg_private Xv_opaque frame_base_get_attr(Frame frame_public, int *status, Attr_attribute attr, va_list valist);

/* fmbs_set.c */
Pkg_private     Xv_opaque frame_base_set_avlist(Frame frame_public, Attr_attribute avlist[]);

/* fmbs_destroy.c */
Pkg_private int frame_base_destroy(Frame frame_public, Destroy_status status);
Xv_private void frame_handle_props(Frame frame_public);

#endif
