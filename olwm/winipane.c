/* #ident	"@(#)winipane.c	26.33	93/06/28 SMI" */
char winipane_c_sccsid[] = "@(#) winipane.c V1.1 94/11/02 21:13:46";

/*
 *      (c) Copyright 1989 Sun Microsystems, Inc.
 */

/*
 *      Sun design patents pending in the U.S. and foreign countries. See
 *      LEGAL_NOTICE file for terms of the license.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
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
#include "menu.h"
#include "globals.h"
#include "events.h"
#include "error.h"
#include "atom.h"


/***************************************************************************
* global data
***************************************************************************/

extern Window NoFocusWin;

extern Time TimeFresh();

/***************************************************************************
* private data
***************************************************************************/

/* border width for reparented windows */
#define NORMAL_BORDERWIDTH      0

static ClassPane classIconPane;

#define IPANE_DEFAULT_PIXMAP(w) (w)->core.client->scrInfo->pixmap[ICON_BITMAP]
#define IPANE_DEFAULT_MASK(w) 	(w)->core.client->scrInfo->pixmap[ICON_MASK]

/***************************************************************************
* private functions
***************************************************************************/

/*
 * drawIPane -- draw the pane window
 */
/*ARGSUSED*/	/* dpy arg will be used when multiple Displays supported */
static int drawIPane(dpy, winInfo)
Display	*dpy;
WinIconPane *winInfo;
{
	Window pane = winInfo->core.self;
	GC  gc;

#ifdef BUSY_ICONS_DID_NOT_WORK
	My attempt to implement busy icons did not work, because all the
	XView clients create their own icon windows - and we have no control
	here about **when** the clients repaint their icons.
#endif /* BUSY_ICONS_DID_NOT_WORK */

	if (winInfo->iconClientWindow)
		return 0;

	XFillRectangle(dpy, pane, WinGC(winInfo, WORKSPACE_GC),
			0, 0, winInfo->core.width, winInfo->core.height);

	/*
	 * REMIND: (1) Need to error-check icon pixmap and mask for being the 
	 * proper depth.  (2) Need to handle color changes better.  Should we use 
	 * a different GC?
	 */
	gc = WinGC(winInfo, ICON_NORMAL_GC);

	if (winInfo->iconMask != None) {
		gc = WinGC(winInfo, ICON_MASK_GC);
		XSetClipMask(dpy, gc, winInfo->iconMask);
	}

	if (winInfo->iconPixmapDepth <= 1) {
		XCopyPlane(dpy, winInfo->iconPixmap, pane, gc,
				0, 0, winInfo->core.width, winInfo->core.height,
				0, 0, (unsigned long)1L);
	}
	else {
		XCopyArea(dpy, winInfo->iconPixmap, pane, gc,
				0, 0, winInfo->core.width, winInfo->core.height, 0, 0);
	}

	if (winInfo->iconMask != None) {
		XSetClipMask(dpy, gc, None);
	}
	return 0;
}


/*
 * focusIPane -- handle focus change
 */
static int
focusIPane(dpy, winInfo, focus)
Display	*dpy;
WinGeneric *winInfo;
Bool focus;
{
	/* REMIND: change background pixel of pane window */
	return 0;
}

/*
 * destroyIPane -- destroy the pane window resources and free any allocated
 *	data.
 */
static int
destroyIPane(dpy, winInfo)
Display	*dpy;
WinIconPane *winInfo;
{
	/* free our data and throw away window */
	WIUninstallInfo(winInfo->core.self);
        if (!winInfo->iconClientWindow)
	{
	      /* REMIND there may be other resources to be freed */
	      ScreenDestroyWindow(winInfo->core.client->scrInfo,
				  winInfo->core.self);
	}
	MemFree(winInfo);
	return 0;
}

/*
 * setconfigIPane -- change configuration of pane window
 */
/*ARGSUSED*/	/* dpy arg will be used when multiple Displays supported */
static int
setconfigIPane(dpy, winInfo)
Display	*dpy;
WinIconPane *winInfo;
{
        XWindowChanges xwc;

        if (winInfo->core.dirtyconfig)
        {
                xwc.x = winInfo->core.x;
                xwc.y = winInfo->core.y;
                xwc.width = winInfo->core.width;
                xwc.height = winInfo->core.height;
                XConfigureWindow(dpy, winInfo->core.self,
                        winInfo->core.dirtyconfig&(CWX|CWY|CWWidth|CWHeight), &xwc);
                winInfo->core.dirtyconfig &= ~(CWX|CWY|CWWidth|CWHeight);
        }
	return 0;
}


/* 
 * newconfigIPane - compute a new configuration given an event
 * Note:  this function must *always* be called with a configure request
 * event.
 */
static int
newconfigIPane(win, pxcre)
WinIconPane *win;
XConfigureRequestEvent *pxcre;
{
    int 	oldWidth, oldHeight;
    int 	oldX, oldY;
    WinIconFrame *winFrame = (WinIconFrame *)(win->core.parent);

    if (pxcre == NULL)
	return win->core.dirtyconfig;

    oldX = win->core.x;
    oldY = win->core.y;
    oldWidth = win->core.width;
    oldHeight = win->core.height;

    if ((pxcre->value_mask & CWHeight) && (pxcre->height != oldHeight))
    {
	win->core.height = pxcre->height;
	win->core.dirtyconfig |= CWHeight;
    }

    if ((pxcre->value_mask & CWWidth) && (pxcre->width != oldWidth))
    {
	win->core.width = pxcre->width;
	win->core.dirtyconfig |= CWWidth;
    }

    if (pxcre->value_mask & CWBorderWidth)
    {
	win->pcore.oldBorderWidth = pxcre->border_width;
    }

    if (pxcre->value_mask & (CWX | CWY)) 
    {
	FrameSetPosFromPane(winFrame, (pxcre->value_mask & CWX)?(pxcre->x):oldX,
		(pxcre->value_mask & CWY)?(pxcre->y):oldY);
    }

    if (pxcre->value_mask & (CWStackMode | CWSibling))
    {
	GFrameSetStack(winFrame, pxcre->value_mask, pxcre->detail, pxcre->above);
    }

    return win->core.dirtyconfig;
}

/* 
 * newposIPane - move to a given position (relative to parent)
 */
static int
newposIPane(win,x,y)
WinIconPane *win;
int x, y;
{
	if (win->core.x != x)
	{
		win->core.x = x;
		win->core.dirtyconfig |= CWX;
	}

	if (win->core.y != y)
	{
		win->core.y = y;
		win->core.dirtyconfig |= CWY;
	}

	return win->core.dirtyconfig;
}

/* 
 * setsizeIPane - set the pane to a particular size, and initiate a reconfigure
 */
static int
setsizeIPane(win,w,h)
WinIconPane *win;
int w, h;
{
	if (win->core.width != w)
	{
		win->core.width = w;
		win->core.dirtyconfig |= CWWidth;
	}

	if (win->core.height != h)
	{
		win->core.height = h;
		win->core.dirtyconfig |= CWHeight;
	}
	return 0;
}

static int eventEnterNotify(Display *dpy, XEvent *event, WinIconPane *winInfo)
{
    if (event->xany.type == EnterNotify) {
        ColorWindowCrossing(dpy, event, winInfo);
	}

	return 0;
}

static Pixmap createNetPixmapInternal(Display *dpy, Window clwin,
						ScreenInfo *scr, unsigned long offset,
						unsigned long length)
{
	int xsiz, ysiz;
	XImage *xima;
	Pixmap pix;
	int r, c;
	int red_is_small = True;
	char *pixdata;
	unsigned long len, rest, *h;
	unsigned long *data = GetWindowProperty(dpy, clwin, Atom_NET_WM_ICON, 
							offset, length, XA_CARDINAL, 32, &len, &rest);
	xsiz = (int)data[0];
	ysiz = (int)data[1];

	pix = XCreatePixmap(dpy, scr->rootid, xsiz, ysiz,
							DefaultDepth(dpy, scr->screen));      

	pixdata = calloc(sizeof(unsigned long), xsiz * ysiz);
	xima = XCreateImage(dpy, scr->visual, DefaultDepth(dpy, scr->screen),
    				ZPixmap, 0, pixdata, xsiz, ysiz, BitmapPad(dpy), 0);

	data += 2;
	h = data;
	for (c = 0; c < xsiz; c++) {
		for (r = 0; r < ysiz; r++) {
			if (c < 9) {
				if (r == c)  {
					/* for firefox, the red component was less than 5 */
					int redcomp = (0xff0000 & *h) >> 16;
					dra_olwm_trace(222, "pixel value %d: %06lx\n", c,*h);

					red_is_small = red_is_small && (redcomp <= 4);
				}
			}
			else {
				c = xsiz;
				r = ysiz;
			}
			++h;
		}
	}
	dra_olwm_trace(222, "red_is_small %d\n", red_is_small);

	for (c = 0; c < xsiz; c++) {
		for (r = 0; r < ysiz; r++) {
			/* ignore the alpha channel */
			if (red_is_small) {
				XPutPixel(xima, r, c, 0xffffff & *data++);

			}
			else {
				XPutPixel(xima, r, c, 0xffffff - (0xffffff & *data++));
			}
		}
	}
	XPutImage(dpy, pix, scr->gc[ROOT_GC], xima, 0, 0, 0, 0, xsiz, ysiz);
	XDestroyImage(xima);
	/* we die here:                   free(pixdata); */

/* 	fprintf(stderr, "net icon pixmap = %lx\n", pix); */
	return pix;
}

static Pixmap createNetPixmap(Display *dpy, Window clwin, ScreenInfo *scr,
							unsigned long *netIcon, int netIconLength,
							int preferredsize)
{
	int xsiz, ysiz;
	unsigned long offset;
#ifdef SOMETIMES_DIRTY
	XImage *xima;
	Pixmap pix;
	int r, c;
	char *pixdata;
#endif

	offset = 0;
	xsiz = (int)netIcon[0];
	ysiz = (int)netIcon[1];
	while (xsiz != preferredsize) {
		netIcon += (xsiz * ysiz + 2);
		offset += (xsiz * ysiz + 2);
		netIconLength -= (xsiz * ysiz + 2);
		if (netIconLength <= 0) {
			/* preferredsize not found */
			return None;
		}
		xsiz = (int)netIcon[0];
		ysiz = (int)netIcon[1];
	}

	/* now xsiz == preferredsize. In many cases (firefox, gimp)
	 * the resulting pixmap was *sometimes* 'dirty', with wrong colors
	 * and text fragments. I wonder whether there is a bug in either
	 * XChangeProperty or XGetWindowProperty or the X server (I never
	 * encountered dirty firefox icons in the context of Xvnc...).
	 * Let's try the following: we use the 'big data' to find out whether
	 * a suitable icon size is available and then perform a SMALLER
	 * XGetWindowProperty for the pixmap data.
	 *
	 * First impression: no more dirt....
	 * Second impression: still dirt....
	 */
#ifdef SOMETIMES_DIRTY
	pix = XCreatePixmap(dpy, scr->rootid, xsiz, ysiz,
							DefaultDepth(dpy, scr->screen));      

	pixdata = calloc(sizeof(unsigned long), xsiz * ysiz);
	xima = XCreateImage(dpy, scr->visual, DefaultDepth(dpy, scr->screen),
    				ZPixmap, 0, pixdata, xsiz, ysiz, BitmapPad(dpy), 0);

	netIcon += 2;
	for (c = 0; c < xsiz; c++) {
		for (r = 0; r < ysiz; r++) {
			/* ignore the alpha channel */
			XPutPixel(xima, r, c, 0xffffff & *netIcon++);
		}
	}
	XPutImage(dpy, pix, scr->gc[ROOT_GC], xima, 0, 0, 0, 0, xsiz, ysiz);
	XDestroyImage(xima);
	/* we die here:                   free(pixdata); */

/* 	fprintf(stderr, "net icon pixmap = %lx\n", pix); */
	return pix;
#else /* SOMETIMES_DIRTY */
	return createNetPixmapInternal(dpy, clwin, scr, offset, xsiz*ysiz + 2);
#endif /* SOMETIMES_DIRTY */
}

/***************************************************************************
* global functions
***************************************************************************/

/*
 * MakeIconPane  -- create the pane window. Return a WinGeneric structure.
 */
WinIconPane * MakeIconPane(Client *cli, WinGeneric *par, XWMHints *wmHints,
							Bool fexisting, Window clwin)
{
	WinIconPane *w;
	WinIconFrame *frame = (WinIconFrame *) par;
	XSetWindowAttributes xswa;
	XWindowAttributes attr;
	long valuemask;
	Window iconPane;
	Window winRoot;
	unsigned int borderWidth, depthReturn;
	Display *dpy = cli->dpy;
	Status status;
	WinGeneric *info;
	unsigned long *netIcon, netIconLength;

	/* this event mask is used for wm-created icon panes */
#define ICON_PANE_EVENT_MASK						\
		(ButtonPressMask | ButtonReleaseMask |			\
		ButtonMotionMask | ExposureMask | EnterWindowMask)

	/* create the associated structure */
	w = MemNew(WinIconPane);
	w->class = &classIconPane;
	w->core.kind = WIN_ICONPANE;
	WinAddChild(par, w);
	w->core.children = NULL;
	w->core.client = cli;
	w->core.x = 0;
	w->core.y = 0;
	w->core.colormap = cli->scrInfo->colormap;
	w->core.dirtyconfig = CWX | CWY | CWWidth | CWHeight;
	w->core.exposures = NULL;
	w->core.helpstring = "Icon";
	w->iconClientWindow = False;
	w->iconPixmap = None;
	w->iconPixmapDepth = 1;
	w->iconMask = None;

	frame->fcore.panewin = (WinGenericPane *) w;

	/* first try the client's icon window hint */

	if (wmHints && (wmHints->flags & IconWindowHint)) {
		iconPane = wmHints->icon_window;
		info = WIGetInfo(iconPane);
		if (info != NULL && info->core.kind != WIN_PANE) {
			ErrorWarning(GetString
					("An existing window was named as an icon window."));
		}
		else {
			if (info != NULL)
				StateWithdrawn(info->core.client, TimeFresh());

			status = XGetWindowAttributes(dpy, iconPane, &attr);

			if (status) {
				w->core.x = attr.x;
				w->core.y = attr.y;
				/* constrain to max icon size */
				if (cli->protocols & ALLOW_ICON_SIZE) {
					w->core.width = MIN(attr.width, ICON_MAX_WIDTH);
					w->core.height = MIN(attr.height, ICON_MAX_HEIGHT);
				}
				else {
					w->core.width = MIN(attr.width, GRV.MaximumIconSize);
					w->core.height = MIN(attr.height, GRV.MaximumIconSize);
				}
				w->core.colormap = attr.colormap;

				w->iconClientWindow = True;
				XSelectInput(dpy, iconPane,
						ButtonPressMask | ButtonReleaseMask |
						ButtonMotionMask | EnterWindowMask);

				if (attr.border_width != NORMAL_BORDERWIDTH) {
					XSetWindowBorderWidth(dpy, iconPane, NORMAL_BORDERWIDTH);
				}
				goto goodicon;
			}
			ErrorWarning(GetString
					("An invalid window was named as an icon window."));
		}
	}

	/* do we have that monster _NET_WM_ICON ? */
	if (PropGetNetWMIcon(dpy, clwin, &netIcon, &netIconLength)) {
		int size = 64;
		Pixmap netpix;

		netpix = createNetPixmap(dpy, clwin, cli->scrInfo,
								netIcon, netIconLength, size);

		if (netpix == None) {
			size = 48;
			netpix = createNetPixmap(dpy, clwin, cli->scrInfo,
								netIcon, netIconLength, size);
		}

		/* anyway: this is a huge waste of server memory ... */
		{
			Atom at = XA_PIXMAP;

			XChangeProperty(dpy, clwin, Atom_NET_WM_ICON, XA_CARDINAL,
						32, PropModeReplace, (unsigned char *)&at, 1);
		}

		if (netpix != None) {
			w->core.x = 0;
			w->core.y = 0;
			w->core.width = w->core.height = size;
			borderWidth = 0;
			depthReturn = DefaultDepth(dpy, cli->scrInfo->screen);      

			/* build icon pixmap window */
			xswa.border_pixel = 0;
			xswa.colormap = cli->scrInfo->colormap;
			xswa.event_mask = ICON_PANE_EVENT_MASK;
			valuemask = CWBorderPixel | CWColormap | CWEventMask;

			iconPane = ScreenCreateWindow(cli->scrInfo, WinRootID(par),
					0, 0, w->core.width, w->core.height, valuemask, &xswa);

			w->iconPixmap = netpix;
			w->iconPixmapDepth = depthReturn;
			w->ignoreWMHintsIcon = True;

			goto goodicon;
		}
	}

	/* try the client's icon pixmap hint */
	if (wmHints && (wmHints->flags & IconPixmapHint)) {
		status = XGetGeometry(dpy, wmHints->icon_pixmap, &winRoot,
				&(w->core.x), &(w->core.y),
				&(w->core.width), &(w->core.height),
				&borderWidth, &depthReturn);

		if (status) {
			/* build icon pixmap window */
			xswa.border_pixel = 0;
			xswa.colormap = cli->scrInfo->colormap;
			xswa.event_mask = ICON_PANE_EVENT_MASK;
			valuemask = CWBorderPixel | CWColormap | CWEventMask;

			/* constrain to max icon size */
			if (cli->protocols & ALLOW_ICON_SIZE) {
				w->core.width = MIN(w->core.width, ICON_MAX_WIDTH);
				w->core.height = MIN(w->core.height, ICON_MAX_HEIGHT);
			}
			else {
				w->core.width = MIN(w->core.width, GRV.MaximumIconSize);
				w->core.height = MIN(w->core.height, GRV.MaximumIconSize);
			}

			iconPane = ScreenCreateWindow(cli->scrInfo, WinRootID(par),
					0, 0, w->core.width, w->core.height, valuemask, &xswa);

			w->iconPixmap = wmHints->icon_pixmap;
			w->iconPixmapDepth = depthReturn;

			/* check for the icon mask */

			if (wmHints->flags & IconMaskHint) {
				int junkx, junky;
				unsigned int junkw, junkh;

				status = XGetGeometry(dpy, wmHints->icon_mask, &winRoot,
						&junkx, &junky, &junkw, &junkh,
						&borderWidth, &depthReturn);

				if (status && depthReturn == 1)
					w->iconMask = wmHints->icon_mask;
				else
					ErrorWarning(GetString
							("An invalid pixmap was named as an icon mask"));
			}
			goto goodicon;

		}
		else {
			ErrorWarning(GetString
					("An invalid pixmap was named as an icon pixmap"));
		}
	}

	/* use the default icon */

	w->iconClientWindow = False;
	w->iconPixmap = IPANE_DEFAULT_PIXMAP(w);
	w->iconMask = IPANE_DEFAULT_MASK(w);

	w->core.x = w->core.y = 0;
	w->core.width = cli->scrInfo->dfltIconWidth;
	w->core.height = cli->scrInfo->dfltIconHeight;

	xswa.border_pixel = 0;
	xswa.colormap = cli->scrInfo->colormap;
	xswa.event_mask = ICON_PANE_EVENT_MASK;
	valuemask = CWBorderPixel | CWColormap | CWEventMask;

	iconPane = ScreenCreateWindow(cli->scrInfo, WinRootID(par),
			0, 0, w->core.width, w->core.height, valuemask, &xswa);

  goodicon:

	w->core.self = iconPane;

	/* set up icon cursor */
	XDefineCursor(dpy, w->core.self, GRV.IconPointer);

	/* register the window */
	WIInstallInfo(w);

	return w;
}

/*
 * IconPaneInit -- initialise the IconPane class function vector
 */
void
IconPaneInit(dpy)
Display *dpy;
{
	classIconPane.core.kind = WIN_ICONPANE;
	classIconPane.core.xevents[Expose] = WinEventExpose;
	classIconPane.core.xevents[ButtonRelease] = PropagateEventToParent;
	classIconPane.core.xevents[MotionNotify] = PropagateEventToParent;
	classIconPane.core.xevents[ButtonPress] = PropagateEventToParent;
	classIconPane.core.xevents[EnterNotify] = eventEnterNotify;
	classIconPane.core.focusfunc = focusIPane;
	classIconPane.core.drawfunc = drawIPane;	/* NULL */
	classIconPane.core.destroyfunc = destroyIPane;
	classIconPane.core.selectfunc = drawIPane;	/* NULL */
	classIconPane.core.newconfigfunc = newconfigIPane;
	classIconPane.core.newposfunc = newposIPane;
	classIconPane.core.setconfigfunc = setconfigIPane;
	classIconPane.core.createcallback = NULL;
	classIconPane.core.heightfunc = NULL;
	classIconPane.core.widthfunc = NULL;
	classIconPane.pcore.setsizefunc = setsizeIPane;
}

/*
 * Set the icon pane's pixmap.
 */
void IconPaneSetPixmap(Display *dpy, WinIconPane	*winInfo, Pixmap pixmap)
{
	if (winInfo->iconClientWindow)
		return;

	if (pixmap == None || pixmap == winInfo->iconPixmap)
		return;

	if (winInfo->ignoreWMHintsIcon) return;

	if (winInfo->iconMask == IPANE_DEFAULT_MASK(winInfo))
		winInfo->iconMask = None;
 
	winInfo->iconPixmap = pixmap;
	winInfo->iconPixmapDepth = 1;
}

/*
 * Set the icon pane's mask.
 */
void
IconPaneSetMask(dpy,winInfo,mask)
	Display		*dpy;
	WinIconPane	*winInfo;
	Pixmap		mask;
{
	if (winInfo->iconClientWindow)
		return;

	if (mask == None || mask == winInfo->iconMask)
		return;

	if (winInfo->ignoreWMHintsIcon) return;

	if (winInfo->iconPixmap == IPANE_DEFAULT_PIXMAP(winInfo))
		winInfo->iconPixmap = None;

	winInfo->iconMask = mask;
}


