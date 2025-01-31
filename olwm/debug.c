/* #ident	"@(#)debug.c	26.11	93/06/28 SMI" */
char debug_c_sccsid[] = "@(#) %M% V%I% %E% %U% $Id: debug.c,v 1.3 2002/05/03 15:30:53 dra Exp $";

/*
 *      (c) Copyright 1989 Sun Microsystems, Inc.
 */

/*
 *	Sun design patents pending in the U.S. and foreign countries. See
 *	LEGAL_NOTICE file for terms of the license.
 */


#include <errno.h>
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "i18n.h"
#include "olwm.h"
#include "win.h"
#include "debug.h"


static char *eventNames[] = {
	"Extension event",
	"<EventOne>",
	"KeyPress",
	"KeyRelease",
	"ButtonPress",
	"ButtonRelease",
	"MotionNotify",
	"EnterNotify",
	"LeaveNotify",
	"FocusIn",
	"FocusOut",
	"KeymapNotify",
	"Expose",
	"GraphicsExpose",
	"NoExpose",
	"VisibilityNotify",
	"CreateNotify",
	"DestroyNotify",
	"UnmapNotify",
	"MapNotify",
	"MapRequest",
	"ReparentNotify",
	"ConfigureNotify",
	"ConfigureRequest",
	"GravityNotify",
	"ResizeRequest",
	"CirculateNotify",
	"CirculateRequest",
	"PropertyNotify",
	"SelectionClear",
	"SelectionRequest",
	"SelectionNotify",
	"ColormapNotify",
	"ClientMessage",
	"MappingNotify"
};

void
DebugEvent(ep, str)
	XEvent *ep;
	char *str;
{
	int indx = (ep->type >= sizeof(eventNames)/sizeof(eventNames[0]) ?
					0 : ep->type);

	(void)fprintf(stderr, "%s: %s\n", str, eventNames[indx]);
}


static char *typeNames[] = {
	"Frame",
	"Icon",
	"Resize",
	"Pushpin",
	"Button",
	"Pane",
	"IconPane",
	"Colormap",
	"Menu",
	"PinMenu",
	"NoFocus",
	"Root",
	"Busy"
};

void
DebugWindow(win)
	WinGeneric *win;
{
	if (win == NULL) {
		(void)fprintf(stderr, "other window - ");
	} else {
		(void)fprintf(stderr, "win %x (self %lu) %s - ",
		        win, win->core.self, typeNames[win->core.kind]);
	}
}
