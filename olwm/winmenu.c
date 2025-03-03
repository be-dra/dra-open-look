char winmenu_c_sccsid[] = "@(#) %M% V%I% %E% %U% $Id: winmenu.c,v 2.4 2025/03/02 17:58:52 dra Exp $";

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
#ifdef OW_I18N_L4
#include "i18n.h"
#endif
#include <olgx/olgx.h>

#include "i18n.h"
#include "ollocale.h"
#include "mem.h"
#include "olwm.h"
#include "win.h"
#include "menu.h"
#include "globals.h"

/***************************************************************************
* private data
***************************************************************************/

/* function vector for menu windows */
static ClassMenu classMenu;

/***************************************************************************
*  private event functions
***************************************************************************/

/* 
 * eventButtonPress  - a button has gone down.
 */
static int
eventButtonPress(dpy, event, winInfo)
	Display		*dpy;
	XEvent		*event;
	WinMenu		*winInfo;
{
	/* REMIND - placeholder for future */
	return 0;
}

/* 
 * eventButtonRelease  - a button has gone up
 */
static int
eventButtonRelease(dpy, event, winInfo)
	Display		*dpy;
	XEvent		*event;
	WinMenu		*winInfo;
{
	/* REMIND - placeholder for future */
	return 0;
}

/* 
 * eventKeyPress  - a key has gone down
 */
static int
eventKeyPress(dpy, event, winInfo)
	Display		*dpy;
	XEvent		*event;
	WinMenu		*winInfo;
{
	/* REMIND - mouseless operation */
	return 0;
}

/* 
 * eventKeyRelease  - a key has gone up
 */
static int
eventKeyRelease(dpy, event, winInfo)
	Display		*dpy;
	XEvent		*event;
	WinMenu		*winInfo;
{
	/* REMIND - mouseless operation */
	return 0;
}

/* 
 * eventMotionNotify - mouse moved
 */
static int
eventMotionNotify(dpy, event, winInfo)
	Display		*dpy;
	XEvent		*event;
	WinMenu		*winInfo;
{
	/* REMIND - placeholder for future */
	return 0;
}

/*
 * destroyMenu -- destroy the menu window resources and free any allocated
 *	data.
 */
static int
destroyMenu(dpy, winInfo)
	Display		*dpy;
	WinMenu 	*winInfo;
{
	XUndefineCursor(dpy, winInfo->core.self);
	XDestroyWindow(dpy, winInfo->core.self);
	MemFree(winInfo);
	return 0;
}


/***************************************************************************
* global functions
***************************************************************************/

/*
 * MakeMenu  -- create the WinMenu structure and windows but does not
 *		map them.
 */
WinMenu * MakeMenu(Display *dpy, WinGeneric *winInfo)
{
	WinMenu 	*w;
	Window 		win;
    unsigned long 	valuemask;
    XSetWindowAttributes attributes;
	Client		*cli = winInfo->core.client;

	/* create the associated structure */
	w = MemNew(WinMenu);
	w->class = &classMenu;
	w->core.kind = WIN_MENU;
	w->core.children = NULL;
	w->core.client = cli;
	w->core.x = 0;
	w->core.y = 0;
	w->core.width = 1;
	w->core.height = 1;
	/* REMIND - is dirtyconfig necessary??? */
	w->core.dirtyconfig = CWX|CWY|CWWidth|CWHeight;
	w->core.exposures = NULL;
	w->core.helpstring = (char *)0;

	/* Menu window. */
	attributes.event_mask = ButtonPressMask | ExposureMask;
	attributes.save_under = True;
	attributes.border_pixel = 0;
	attributes.colormap = cli->scrInfo->colormap;
	valuemask = CWEventMask | CWSaveUnder | CWBorderPixel | CWColormap;
	attributes.border_pixel = cli->scrInfo->colorInfo.fgColor;
	attributes.background_pixel = cli->scrInfo->colorInfo.bgColor;
	attributes.bit_gravity = ForgetGravity;
	valuemask |= CWBackPixel | CWBitGravity;

	win = XCreateWindow(dpy, WinRootID(winInfo),
			w->core.x, w->core.y,
			w->core.width, w->core.height,
			0,
			WinDepth(winInfo),
			InputOutput,
			WinVisual(winInfo),
			valuemask,
			&attributes);
	w->core.self = win;

	WIInstallInfo(w);

	{
		WinMenu 	*sh;

		/* create the associated structure */
		sh = MemNew(WinMenu);
		sh->class = &classMenu;
		sh->core.kind = WIN_MENU;
		sh->core.children = NULL;
		sh->core.client = cli;
		sh->core.x = 0;
		sh->core.y = 0;
		sh->core.width = 8;
		sh->core.height = 8;
		/* REMIND - is dirtyconfig necessary??? */
		sh->core.dirtyconfig = CWX|CWY|CWWidth|CWHeight;
		sh->core.exposures = NULL;
		sh->core.helpstring = (char *)0;

		attributes.background_pixmap = None; /* transparent */
		attributes.save_under = True;
		attributes.event_mask = ExposureMask;
		valuemask = CWEventMask | CWBackPixmap |  CWSaveUnder;
		w->shadow = XCreateWindow(dpy, WinRootID(winInfo),
				0, 0, 8, 8,
				0,
				WinDepth(winInfo),
				InputOutput,
				WinVisual(winInfo),
				valuemask,
				&attributes);
		sh->core.self = w->shadow;

		WIInstallInfo(sh);
	}

	XDefineCursor( dpy, win, GRV.MenuPointer );

	return w;
}

static void draw_menu_shadow(Display *dpy, WinMenu *winInfo)
{
	if (winInfo->shadow) {
		XFillRectangle(dpy, winInfo->shadow,
				winInfo->core.client->scrInfo->gc[MENU_SHADOW_GC],
				0, 0, winInfo->core.width + 1, winInfo->core.height + 1);
	}
}

/*
 * MapMenuWindow - Configures (sizes) and maps the WinMenu windows
 */
void MapMenuWindow(Display *dpy, WinMenu *winInfo, MenuInfo	*menuInfo)
{
	XWindowChanges	changes;

	/* position, size and map menu window */
	winInfo->core.x = menuInfo->menuX;
	winInfo->core.y = menuInfo->menuY;
	winInfo->core.width = menuInfo->menuWidth;
	winInfo->core.height = menuInfo->menuHeight;
	changes.x = winInfo->core.x;
	changes.y = winInfo->core.y;
	changes.width = winInfo->core.width;
	changes.height = winInfo->core.height;
	XConfigureWindow(dpy,winInfo->core.self, CWX|CWY|CWWidth|CWHeight,&changes);

	if (winInfo->shadow) {
		/* map shadow below menu window */
		changes.x = menuInfo->menuX + MENU_SHADOW_OFFSET;
		changes.y = menuInfo->menuY + MENU_SHADOW_OFFSET;
		XConfigureWindow(dpy,winInfo->shadow, CWX|CWY|CWWidth|CWHeight,
											&changes);
		XMapRaised(dpy, winInfo->shadow);
		draw_menu_shadow(dpy, winInfo);
	}

	XMapRaised(dpy,winInfo->core.self);

	/* save the menuinfo */
	winInfo->menuInfo = menuInfo;

	DrawMenu(dpy,menuInfo);
}

/*
 * UnmapMenuWindow - take down WinMenu windows
 */
void UnmapMenuWindow(Display *dpy, WinMenu *winInfo)
{
	XUnmapWindow(dpy,winInfo->core.self);
	XFlush(dpy);
/* 	usleep(999999); */
	if (winInfo->shadow) XUnmapWindow(dpy,winInfo->shadow);
	winInfo->menuInfo = (MenuInfo *)NULL;
}


int MenuEventExpose(Display *dpy, XEvent *event, WinGeneric *winInfo)
{
	MenuInfo *mInfo = NULL;

	dra_olwm_trace(740, "MenuEventExpose(%lx)\n", winInfo);

	if (winInfo->core.kind == WIN_MENU)
		mInfo = ((WinMenu *) winInfo)->menuInfo;
	else
		mInfo = ((WinPinMenu *) winInfo)->menuInfo;

	if (mInfo == NULL)	/*not yet reparented */
		WinEventExpose(dpy, event, winInfo);
	else {
		SetMenuRedrawHints(dpy, event, mInfo);

		if (event->xexpose.count == 0) {
			if (winInfo->core.kind == WIN_MENU) {
				draw_menu_shadow(dpy, (WinMenu *) winInfo);
			}
			DrawMenuWithHints(dpy, mInfo);
		}
	}
	return 0;
}

/*
 * drawMenu -- draw the menu window
 */
int MenuEventDrawMenu(Display *dpy, WinGeneric *winInfo)
{
    MenuInfo *mInfo = NULL;

	dra_olwm_trace(745, "MenuEventDrawMenu(%lx)\n", winInfo);
    if (winInfo->core.kind == WIN_MENU)
	mInfo = ((WinMenu *) winInfo)->menuInfo;
    else
	mInfo = ((WinPinMenu *) winInfo)->menuInfo;

    if (mInfo) {
    	if (winInfo->core.kind == WIN_MENU) {
			draw_menu_shadow(dpy, (WinMenu *)winInfo);
		}
		DrawMenu(dpy, mInfo);
	}
	return 0;
}


/*
 * MenuInit - initialize WinMenu class functions
 */
/*ARGSUSED*/
void MenuInit(Display *dpy)
{
	classMenu.core.kind = WIN_MENU;
	classMenu.core.xevents[ButtonPress] = eventButtonPress;
	classMenu.core.xevents[ButtonRelease] = eventButtonRelease;
	classMenu.core.xevents[MotionNotify] = eventMotionNotify;
	classMenu.core.xevents[KeyPress] = eventKeyPress;
	classMenu.core.xevents[KeyRelease] = eventKeyRelease;
	classMenu.core.xevents[Expose] = MenuEventExpose;
	classMenu.core.drawfunc = MenuEventDrawMenu;
	classMenu.core.destroyfunc = destroyMenu;
	classMenu.core.heightfunc = NULL;
	classMenu.core.widthfunc = NULL;
}
