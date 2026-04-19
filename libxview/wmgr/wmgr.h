/*	@(#)wmgr.h 20.24 93/06/28 SMI RCS $Id: wmgr.h,v 2.3 2026/04/18 17:28:54 dra Exp $ */

#ifndef xview_wmgr_DEFINED
#define xview_wmgr_DEFINED	1

#include <xview/xv_c_types.h>
#include <xview/frame.h>

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL_NOTICE 
 *	file for terms of the license.
 */

/*
 * This header file describes the interface to a window management mechanism.
 * A menu interface to these functions is also provided.
 * Typically, a tool window is responsible for window management.
 */
 
#define	WMGR_ICONIC	WUF_WMGR1	/* Indicates window is iconic
					   in user flags of window */
#define WMGR_SUBFRAME   WUF_WMGR3   /* Indicates window is a sub-frame
					   in user flags of window */

#define	WMGR_SETPOS	-1		/* Indicates "use default" in
					   wmgr_figure*rect calls	*/
/* for XGetProperty call, indicate no deleting the existing property after
 * a get
 */
#define WMGR_NO_DELETE  FALSE

typedef enum  {
	WM_None, WM_N, WM_NE, WM_E, WM_SE, WM_S, WM_SW, WM_W, WM_NW
}   WM_Direction;

/*
 * Basic window management operations.
 * Move and stretch require user interaction.
 */
_XVFUNCPROTOBEGIN
EXTERN_FUNCTION (void wmgr_top, (Frame frame));
EXTERN_FUNCTION (void wmgr_bottom, (Frame frame));

/*
 * Exported by wmgr_rect.c:
 */
EXTERN_FUNCTION (void wmgr_completechangerect, (Xv_opaque window, Rect *newrect, Rect *origrect, int parentprleft, int parentprtop));

EXTERN_FUNCTION (void wmgr_refreshwindow, (Xv_opaque window));


/*
 * Exported by wmgr_state.c:
 */
EXTERN_FUNCTION (void wmgr_changelevel, (Xv_object window, int parent, int top));

_XVFUNCPROTOEND

#endif
