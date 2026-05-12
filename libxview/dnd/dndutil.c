#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)dndutil.c 1.16 93/06/28 DRA: $Id: dndutil.c,v 4.6 2026/05/11 19:33:30 dra Exp $ ";
#endif
#endif

/*
 *      (c) Copyright 1990 Sun Microsystems, Inc. Sun design patents 
 *      pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *      file for terms of the license.
 */

#include <X11/Xproto.h>
#include <xview/xview.h>
#include <xview/cursor.h>
#include <xview/server.h>
#include <xview/dragdrop.h>
#include <xview_private/dndimpl.h>

static int sendEventError;
static XErrorHandler old_handler;

static int sendEventErrorHandler(Display *dpy, XErrorEvent *error)
{
	if (error->request_code == X_SendEvent) {
		sendEventError = True;
		return 0;
	}
	return (*old_handler) (dpy, error);
}

int debug_DND = -1;

Pkg_private int DndSendEvent(Display *dpy, XEvent *event, const char *nam)
{
    Status status;

	if (debug_DND < 0) {
		char *envdnddeb = getenv("XVDND_DEBUG");

		if (envdnddeb && *envdnddeb) debug_DND = atoi(envdnddeb);
		else debug_DND = 0;
	}
    sendEventError = False;
    old_handler = XSetErrorHandler(sendEventErrorHandler);

    status = XSendEvent(dpy, event->xbutton.window, False, NoEventMask,
			(XEvent *) event);
	if (debug_DND > 0) fprintf(stderr, "sent %s, before XSync\n", nam);
    XFlush(dpy);
    (void) XSetErrorHandler(old_handler);
	if (debug_DND > 0) fprintf(stderr, "              after XSync\n");

    if (status && ! sendEventError) return DND_SUCCEEDED;
    
    return DND_ERROR;
}
