/* #ident	"@(#)atom.c	26.24	93/06/28 SMI" */
char atom_c_sccsid[] = "@(#) %M% V%I% %E% %U% $Id: atom.c,v 2.15 2025/03/01 12:25:28 dra Exp $";

/*
 *      (c) Copyright 1989 Sun Microsystems, Inc.
 */

/*
 *      Sun design patents pending in the U.S. and foreign countries. See
 *      LEGAL_NOTICE file for terms of the license.
 */

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xresource.h>
#include "olwm.h"
#include "atom.h"

Atom atoms[_OL_LAST_ATOM];

/***************************************************************************
* Global functions
***************************************************************************/

/*
 * InitAtoms -- initialize the atoms needed to communicate with Open
 *	Look clients
 */
void InitAtoms(Display *dpy)
{
	static char *ans[] = {
		/* ICCCM specific flags */
		"WM_COLORMAP_WINDOWS",
		"WM_STATE",
		"WM_CHANGE_STATE" ,
		"WM_PROTOCOLS" ,
		"WM_TAKE_FOCUS" ,
		"WM_SAVE_YOURSELF" ,
		"WM_DELETE_WINDOW" ,

		/* OpenLook specific flags */
		/*** PROPERTY_DEFINITION ********************************************
		 * _OL_WIN_ATTR     Type _OL_WIN_ATTR        Format 32
		 * Owner: client, Reader: wm
		 */
		"_OL_WIN_ATTR" ,
		/*** PROPERTY_DEFINITION ********************************************
		 * _OL_PIN_STATE     Type INTEGER        Format 32
		 * Owner: wm, Reader: client
		 */
		"_OL_PIN_STATE" ,
		/*** PROPERTY_DEFINITION ********************************************
		 * _OL_WIN_BUSY     Type INTEGER             Format 32
		 * Owner: client, Reader: wm
		 */
		"_OL_WIN_BUSY" ,
		"_OL_WINMSG_ERROR" ,
		"_OL_WINMSG_STATE" ,
		"_OL_PIN_OUT" ,
		"_OL_DECOR_RESIZE" ,
		"_OL_WT_BASE" ,
		"_OL_DECOR_FOOTER" ,
		/*** PROPERTY_DEFINITION ********************************************
		 * _OL_DECOR_ADD     Type ATOM        Format 32
		 * Owner: client, Reader: wm
		 */
		"_OL_DECOR_ADD" ,
		/*** PROPERTY_DEFINITION ********************************************
		 * _OL_DECOR_DEL     Type ATOM        Format 32
		 * Owner: client, Reader: wm
		 */
		"_OL_DECOR_DEL" ,
		"_OL_DECOR_PIN" ,
		"_OL_WT_CMD" ,
		"_OL_WT_PROP" ,
		"_OL_PIN_IN" ,
		"_OL_NONE" ,
		"_OL_WT_NOTICE" ,
		"_OL_MENU_FULL" ,
		"_OL_DECOR_HEADER" ,
		"_OL_WT_HELP" ,
		"_OL_MENU_LIMITED" ,
		"_OL_DECOR_CLOSE" ,
		"_OL_WT_OTHER" ,
		"_SUN_OLWM_NOFOCUS_WINDOW",
		"_OL_DFLT_BTN",
		"_OL_DECOR_ICON_NAME",
#ifdef OW_I18N_L4
		"_OL_DECOR_IMSTATUS",
		"_OL_WINMSG_IMSTATUS",
		"_OL_WINMSG_IMPREEDIT",
#endif

		/* ICCCM selection atoms */
		"ATOM_PAIR",
		"CLIENT_WINDOW",
		"CLASS",
		"DELETE",
		"MULTIPLE",
		"LIST_LENGTH",
		"NAME",
		"TARGETS",
		"TIMESTAMP",
		"USER",
#ifdef OW_I18N_L4
		"COMPOUND_TEXT" ,
#endif

		/* SunView environment */
		"_SUN_SUNVIEW_ENV",

		/* Sun window manager atoms */
		"_SUN_LED_MAP",
		"_SUN_WM_PROTOCOLS",
		"_SUN_WINDOW_STATE",
		"_SUN_OL_WIN_ATTR_5",
		"_SUN_WM_REREAD_MENU_FILE",

		"_DRA_ENHANCED_OLWM",
		"_OL_SHOW_PROPS" ,
		"_OL_PROPAGATE_EVENT" ,
		"_OL_WARP_BACK",
		"_OL_NO_WARPING",
		"_OL_WARP_TO_PIN",
		"_OL_OWN_HELP",
		"_OL_GROUP_MANAGED",
		"_OL_IS_WS_PROPS",
		"_OL_ALLOW_ICON_SIZE",
		"_OL_WM_MENU_FILE_NAME",
		"_SUN_WM_REREAD_MENU_FILE",
		"_DRA_TRACE",
		/*** PROPERTY_DEFINITION ********************************************
		 * _OL_WIN_MENU_DEFAULT        Type INTEGER        Format 32
		 * Owner: wm, Reader: client
		 */
		"_OL_WIN_MENU_DEFAULT",
		/*** PROPERTY_DEFINITION ********************************************
		 * _OL_SET_WIN_MENU_DEFAULT    Type INTEGER        Format 32
		 * Owner: client, Reader: wm
		 */
		"_OL_SET_WIN_MENU_DEFAULT",
		/*** PROPERTY_DEFINITION ********************************************
		 * _OL_WIN_COLORS              Type _OL_WIN_COLORS     Format 32
		 * Owner: client, Reader: wm 
		 */ 
		"_OL_WIN_COLORS",
		/*** PROPERTY_DEFINITION *********************************************
		 * _OL_COLORS_FOLLOW        Type WINDOW                Format 32
		 * Owner: client, Reader: wm
		 */
		"_OL_COLORS_FOLLOW",

		"CLIPBOARD",
		"_DRA_PSEUDO_CLIPBOARD",
		"_DRA_PSEUDO_SECONDARY",
		"_SUN_SELECTION_END",

		"_OL_FUNC_CLOSE",
		"_OL_FUNC_FULLSIZE",
		"_OL_FUNC_PROPS",
		"_OL_FUNC_BACK",
		"_OL_FUNC_REFRESH",
		"_OL_FUNC_QUIT",
		"_OL_NOTICE_EMANATION",

		"_SUN_QUICK_SELECTION_KEY_STATE",
		"DUPLICATE",
		"_SUN_SELECTION_END",
		"NULL",
		"_OL_SOFT_KEYS_PROCESS",
		"_NET_WM_STATE_MAXIMIZED_VERT",
		"_NET_WM_STATE_MAXIMIZED_HORZ",

		/* Sun drag-and-drop atoms */

		"_SUN_DRAGDROP_INTEREST",
		"_SUN_DRAGDROP_DSDM",
		"_SUN_DRAGDROP_SITE_RECTS",
		"_DRA_TOP_LEVEL_WINDOWS",
		"_NET_WM_WINDOW_TYPE",
		"_NET_WM_WINDOW_TYPE_UTILITY",
		"_NET_WM_STATE",
		"VERSION",
		"SHUTDOWN",
		"REBOOT"
		, "_OL_SELECTION_IS_WORD"
#ifdef ALWAYS_IN_JOURNALLING_MODE_QUESTION
		, "JOURNAL_SYNC"    /* just to keep this silent */
#endif
		, "_NET_WM_ICON"
	};

	XInternAtoms(dpy, ans, sizeof(ans)/sizeof(ans[0]), False, atoms);
}
