/* #ident	"@(#)winbutton.c	26.31	93/06/28 SMI" */
char winbutton_c_sccsid[] = "@(#) winbutton.c V1.1 94/11/02 21:13:42";

/*
 *      (c) Copyright 1989 Sun Microsystems, Inc.
 */

/*
 *      Sun design patents pending in the U.S. and foreign countries. See
 *      LEGAL_NOTICE file for terms of the license.
 */

#include <errno.h>
#include <stdio.h>
#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include "i18n.h"		/* needed by olgx.h */
#include <olgx/olgx.h>

#include "ollocale.h"
#include "mem.h"
#include "olwm.h"
#include "win.h"
#include "globals.h"
#include "menu.h"
#include "events.h"
#include "atom.h"

extern void FrameAllowEvents();
extern Bool DoDefaultMenuAction();

/***************************************************************************
* private data
***************************************************************************/

#define in_windowmark(win,x,y) \
	( (x) >= 0 && (y) >= 0 && \
	  (x) <= Abbrev_MenuButton_Width(WinGI((win),NORMAL_GINFO)) && \
	  (y) <= Abbrev_MenuButton_Height(WinGI((win),NORMAL_GINFO)) \
	)
static Bool buttonActive = False;
static ClassButton classButton;
static SemanticAction currentAction = ACTION_NONE;

/***************************************************************************
* private functions
***************************************************************************/

static int drawButton();

static void doUnhilite(int act, MenuTrackMode mode, WinButton *winInfo)
{
    Graphics_info	*gisNormal = WinGI(winInfo, NORMAL_GINFO);
    long flags;

    if (act != SYNC_CHANGECLICK) {
		GC windowGC = WinGC(winInfo, WINDOW_GC);

		flags = OLGX_NORMAL | OLGX_ERASE;
		/* see the comment in "drawButton" - let us try to
		 * erase the background first.
		 * This seems to solve the problem of the abbrev button staying
		 * even when the menu has been unmapped - at least in 2d case.
		 */
		XFillRectangle(gisNormal->dpy, winInfo->core.self, windowGC, 0, 0,
				Abbrev_MenuButton_Width(gisNormal),
				Abbrev_MenuButton_Height(gisNormal));
	}
    else if (mode == MODE_CLICK) {
		flags = OLGX_BUSY | OLGX_ERASE | OLGX_NORMAL;
	}
    else {
		/* don't do this; it's unsettling to press it in when you drag again */
		return;
    }
    olgx_draw_abbrev_button(gisNormal, winInfo->core.self, 0, 0,
								OLGX_VERTICAL | flags);
}

/* 
 * eventButtonPress - handle button press events on the close button window.  
 */
static int eventButtonPress(Display *dpy, XEvent *event, WinButton *winInfo)
{
	Client *cli = winInfo->core.client;
	WinPaneFrame *winFrame = cli->framewin;
	Graphics_info *gisNormal = WinGI(winInfo, NORMAL_GINFO);
	SemanticAction a;

	a = MenuMouseAction(dpy, event, ModMaskMap[MOD_CONSTRAIN]);

	if (winInfo->ignore) {
		FrameAllowEvents(cli, event->xbutton.time);
		return 0;
	}

	switch (a) {
		case ACTION_SELECT:
			olgx_draw_abbrev_button(gisNormal, winInfo->core.self,
					0, 0, OLGX_VERTICAL | OLGX_INVOKED);
			/*
			 * REMIND: bad style.  This is grabbing the pointer after
			 * the fact.  We should set up a passive grab instead.
			 */
			XGrabPointer(dpy, winInfo->core.self, False,
					(ButtonReleaseMask | ButtonPressMask | PointerMotionMask),
					GrabModeAsync, GrabModeAsync, None,
					GRV.MovePointer, event->xbutton.time);
			buttonActive = True;
			currentAction = a;
			break;

		case ACTION_MENU:
			olgx_draw_abbrev_button(gisNormal, winInfo->core.self,
					0, 0, OLGX_VERTICAL | OLGX_INVOKED);
			if (winFrame->core.client->wmDecors->menu_type != MENU_NONE)
				ShowStandardMenuSync(winFrame, event, True, doUnhilite,
						winInfo);
			break;

		default:
			FrameAllowEvents(cli, event->xbutton.time);
			return 0;
	}
	return 0;
}

/* 
 * eventButtonRelease - handle button release events on the close button 
 * window.  When we handle an event, start ignoring mouse events on the button 
 * and send a ClientMessage to ourself.  When we receive the ClientMessage, 
 * stop ignore events.  This is so that double-clicking on the button doesn't 
 * close and then reopen the window (or perform the default action twice).
 */
static int eventButtonRelease(Display *dpy, XEvent *event, WinButton *winInfo)
{
	Client *cli = winInfo->core.client;
	int x, y;
	XClientMessageEvent ce;

	FrameAllowEvents(cli, event->xbutton.time);

	if (!AllButtonsUp(event))
		return 0;

	XUngrabPointer(dpy, event->xbutton.time);

	x = event->xbutton.x;
	y = event->xbutton.y;

	if (buttonActive) {
		drawButton(dpy, winInfo);
		buttonActive = False;
	}

	if (!in_windowmark(winInfo, x, y) || currentAction != ACTION_SELECT) {
		return 0;
	}

	if (!winInfo->ignore) {
		if (!DoDefaultMenuAction(cli->framewin)) {
			ClientOpenCloseToggle(cli, event->xbutton.time);
		}
		ce.type = ClientMessage;
		ce.window = winInfo->core.self;
		ce.message_type = AtomChangeState;
		ce.format = 32;
		XSendEvent(dpy, winInfo->core.self, False, NoEventMask,
				(XEvent *) & ce);
		winInfo->ignore = True;
	}

	currentAction = ACTION_NONE;
	return 0;
}

/* 
 * eventMotionNotify - handle motion notify events on the close button window.  
 */
static int eventMotionNotify(Display *dpy, XEvent *event, WinButton	*winInfo)
{
	int x, y;
	Graphics_info *gisNormal = WinGI(winInfo, NORMAL_GINFO);

	if (!event->xmotion.same_screen || currentAction != ACTION_SELECT)
		return 0;

	x = event->xmotion.x;
	y = event->xmotion.y;
	if (buttonActive && !in_windowmark(winInfo, x, y)) {
		drawButton(dpy, winInfo);
		buttonActive = False;
	}
	else if (!buttonActive && in_windowmark(winInfo, x, y)) {
		olgx_draw_abbrev_button(gisNormal, winInfo->core.self,
				0, 0, OLGX_VERTICAL | OLGX_INVOKED);
		buttonActive = True;
	}
	return 0;
}


/*
 * eventClientMessage - handle ClientMessage events sent to the button.  In 
 * eventButtonRelease, we send a ClientMessage to ourself.  When we receive 
 * it, stop ignoring button press events.
 */
static int eventClientMessage(Display *dpy, XClientMessageEvent *ce,
					WinButton *winInfo)
{
    if (ce->message_type == AtomChangeState) winInfo->ignore = False;
    return 0;
}


/*
 * drawButton -- draw the window button
 */
static int drawButton(Display	*dpy, WinButton *winInfo)
{
	Client *cli = winInfo->core.client;
	GC  windowGC = WinGC(winInfo, WINDOW_GC);
	XGCValues gcv;
	Graphics_info *gisNormal = WinGI(winInfo, NORMAL_GINFO);
	int focusLines = (GRV.FocusFollowsMouse ? 1 : 0) ^
			(GRV.InvertFocusHighlighting ? 1 : 0);
	int is3d = (GRV.ui_style == UIS_3D_COLOR);

	/*
	 * Erase the background first.  Unfortunately, we can't depend on
	 * OLGX_ERASE to do the right thing, because it (a) erases only in BG1,
	 * and (b) erases only in 2D mode.  We need to erase a background color
	 * that depends on the state of the frame.  If we're in click-focus and we
	 * have the focus, draw in BG2; otherwise, draw in BG1.
	 */

	/* Temporarily set background to BG2 if click-to-type */
	if (!focusLines && winInfo->core.client->isFocus && is3d) {
		XGetGCValues(dpy, windowGC, GCBackground, &gcv);
		XSetBackground(dpy, windowGC, cli->scrInfo->colorInfo.bg2Color);
	}

	XFillRectangle(dpy, winInfo->core.self, windowGC, 0, 0,
			Abbrev_MenuButton_Width(gisNormal),
			Abbrev_MenuButton_Height(gisNormal));

	/* Restore background back to BG1 */
	if (!focusLines && winInfo->core.client->isFocus && is3d) {
		XSetBackground(dpy, windowGC, gcv.background);
	}

	olgx_draw_abbrev_button(gisNormal, winInfo->core.self,
			0, 0, OLGX_VERTICAL | OLGX_NORMAL | OLGX_ERASE);

	/*
	 * REMIND: hack for working around OLGX deficiency.  OLGX erases the
	 * "ears" at each corner of the window button to the background color.  
	 * They should really be filled in with the foreground color.
	 */
	if (!focusLines && winInfo->core.client->isFocus && !is3d) {
		XDrawRectangle(dpy, winInfo->core.self, WinGC(winInfo, FOREGROUND_GC),
				0, 0,
				Abbrev_MenuButton_Width(gisNormal) - 1,
				Abbrev_MenuButton_Height(gisNormal) - 1);
		XDrawPoint(dpy, winInfo->core.self, WinGC(winInfo, FOREGROUND_GC),
				Abbrev_MenuButton_Width(gisNormal) - 1,
				Abbrev_MenuButton_Height(gisNormal) - 1);
	}
	return 0;
}


/*
 * DestroyButton -- destroy the close button window resources and free any allocated
 *	data.
 */
static int destroyButton(Display	*dpy, WinButton *winInfo)
{
	/* free our data and throw away window */
	ScreenDestroyWindow(winInfo->core.client->scrInfo, winInfo->core.self);
	WIUninstallInfo(winInfo->core.self);
	MemFree(winInfo);
	return 0;
}

/* 
 * focusButton - the focus or selection state has changed
 */
static int focusButton(Display *dpy, WinButton *winInfo, Bool selected)
{
	(WinFunc(winInfo,core.drawfunc))(dpy, winInfo);
	return 0;
}

/*
 * heightfuncButton - recomputes the height of the close button window
 */
static int 
heightfuncButton(win, pxcre)
WinButton *win;
XConfigureRequestEvent *pxcre;
{
	return Abbrev_MenuButton_Height(WinGI(win,NORMAL_GINFO));
}

/*
 * widthfuncButton - recomputes the width of the close button window
 */
static int 
widthfuncButton(win, pxcre)
WinButton *win;
XConfigureRequestEvent *pxcre;
{
	return Abbrev_MenuButton_Width(WinGI(win,NORMAL_GINFO));
}


/***************************************************************************
* global functions
***************************************************************************/

/*
 * MakeButton  -- create the close button window. Return a WinGeneric structure.
 */
WinButton *
MakeButton(dpy, par, x, y)
Display	*dpy;
WinGeneric *par;
int x,y;
{
	WinButton *w;
	Window win;
        unsigned long valuemask;
        XSetWindowAttributes attributes;
	Graphics_info	*gisNormal = WinGI(par,NORMAL_GINFO);

        attributes.event_mask =
	    ButtonReleaseMask | ButtonPressMask | ExposureMask;
	attributes.cursor = GRV.IconPointer;
        valuemask = CWEventMask | CWCursor;

	win = ScreenCreateWindow(par->core.client->scrInfo, par->core.self,
	    x, y,
	    Abbrev_MenuButton_Width(gisNormal),
	    Abbrev_MenuButton_Height(gisNormal),
	    valuemask, &attributes);

	/* create the associated structure */
	w = MemNew(WinButton);
	w->core.self = win;
	w->class = &classButton;
	w->core.kind = WIN_WINBUTTON;
	WinAddChild(par,w);
	w->core.children = NULL;
	w->core.client = par->core.client;
	w->core.x = x;	
	w->core.y = y;
	w->core.width = Abbrev_MenuButton_Width(gisNormal);
	w->core.height = Abbrev_MenuButton_Height(gisNormal);
	w->core.dirtyconfig = 0;
	w->core.exposures = NULL;
	w->core.helpstring = "olwm:CloseButton";
	w->ignore = False;

	/* register the window */
	WIInstallInfo(w);

        XMapWindow(dpy, win);

	return w;
}

void ButtonInit(Display *dpy)
{
	classButton.core.kind = WIN_WINBUTTON;
	classButton.core.xevents[ButtonPress] = eventButtonPress;
	classButton.core.xevents[ButtonRelease] = eventButtonRelease;
	classButton.core.xevents[MotionNotify] = eventMotionNotify;
	classButton.core.xevents[Expose] = WinEventExpose;
	classButton.core.xevents[ClientMessage] = eventClientMessage;
	classButton.core.focusfunc = focusButton;
	classButton.core.drawfunc = drawButton;
	classButton.core.destroyfunc = destroyButton;
	classButton.core.selectfunc = NULL;
	classButton.core.newconfigfunc = WinNewConfigFunc;
	classButton.core.newposfunc = WinNewPosFunc;
	classButton.core.setconfigfunc = WinSetConfigFunc;
	classButton.core.createcallback = NULL;
	classButton.core.heightfunc = heightfuncButton;
	classButton.core.widthfunc = widthfuncButton;
}

