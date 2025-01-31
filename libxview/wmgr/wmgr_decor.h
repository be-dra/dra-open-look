/*	@(#)wmgr_decor.h 1.17 93/06/28 SMI RCS $Id: wmgr_decor.h,v 2.1 2020/07/26 07:31:24 dra Exp $ */

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */

#ifndef _wmgr_decor_h_already_included
#define _wmgr_decor_h_already_included

#include <X11/Xlib.h>

#define		WM_MAX_DECOR		6

typedef struct {
	long flags;
	enum {
		MENU_FULL = 0,		/* Close, Zoom, Props, Scale, Back,
					 * Refresh, Quit
					 */
		MENU_LIMITED = 1,	/* Dismiss, Scale, Refresh */
		MENU_DISMISS_ONLY = 2	/* Dismiss */
	} menu_type;
	int pushpin_initial_state;	/* WMDontStayUp or WMStayUp */
} WMDecorations;


typedef struct {			/* old _OL_WIN_ATTR format */
    	Atom	win_type;
    	Atom	menu_type;
    	Atom	pin_initial_state;
} WM_Win_Type_Old;

typedef struct {			/* new _OL_WIN_ATTR format */
	unsigned long	flags;
    Atom	win_type;
    Atom	menu_type;
    unsigned long pin_initial_state;
	unsigned long cancel;
} WM_Win_Type;

/*
 * Values for flags in new OL_WIN_ATTR property
 */
#define WMWinType      (1<<0)
#define WMMenuType     (1<<1)
#define WMPinState     (1<<2)
#define WMCancel       (1<<3)

/* value for flags */
#define WMDecorationHeader	(1L<<0)
#define WMDecorationFooter	(1L<<1)
#define WMDecorationPushPin	(1L<<2)
#define WMDecorationCloseButton	(1L<<3)
#define WMDecorationOKButton	(1L<<4)
#define WMDecorationResizeable	(1L<<5)

/* value for pushpin_initial_state */
#ifndef WMPushpinIsOut
#define WMPushpinIsOut	0
#endif  /* WMPushpinIsOut */
#ifndef WMPushpinIsIn
#define WMPushpinIsIn	1
#endif  /* WMPushpinIsIn */

/* value for WM_WINDOW_BUSY property */
#define WMWindowNotBusy	0
#define WMWindowIsBusy	1

extern void wmgr_set_rescale_state(unsigned long frame_public, int state);
extern int wmgr_set_win_attr(unsigned long frame_public, WM_Win_Type *win_attr);
extern int wmgr_add_decor(unsigned long frame_public, Atom *decor_list, int num_of_decor);
extern int wmgr_delete_decor(unsigned long frame_public, Atom *decor_list, int num_of_decor);

#endif  /* _wmgr_decor_h_already_included */
