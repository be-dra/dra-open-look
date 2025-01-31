#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)dndutil.c 1.16 93/06/28 DRA: $Id: dndutil.c,v 4.3 2024/11/18 12:22:15 dra Exp $ ";
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

static Atom InternSelection(Xv_server server, int n, XID xid);

/* 
 * Determine what cursor to use, create one if none defined.  Return the XID.
 */
Xv_private XID DndGetCursor(Dnd_info *dnd)
{
	if (!dnd->xCursor && !dnd->cursor) {
		/* Actually, the CURSOR package has **no** find-method...
		 * and it's superclass GENERIC also has **no** find-method
		 * What happens here?
		 *
		 * It is just a simple xv_create...
		 */
		dnd->cursor = xv_find(dnd->parent, CURSOR,
			CURSOR_SRC_CHAR,(dnd->type==DND_MOVE)?OLC_MOVE_PTR
												:OLC_COPY_PTR,
			CURSOR_MASK_CHAR,(dnd->type==DND_MOVE)?OLC_MOVE_MASK_PTR
												:OLC_COPY_MASK_PTR,
			NULL);
		return ((XID) xv_get(dnd->cursor, XV_XID));
	}
	else if (dnd->cursor)
		return ((XID) xv_get(dnd->cursor, XV_XID));
	else
		return ((XID) dnd->xCursor);
}

Xv_private int DndGetSelection(Dnd_info *dnd, Display *dpy)
{
	int i = 0;
	Atom seln;
	Xv_Server server = XV_SERVER_FROM_WINDOW(dnd->parent);

	/* Application defined selection. */
	if (xv_get(DND_PUBLIC(dnd), SEL_OWN))
		return DND_SUCCEEDED;

	/* Create our own transient selection. */
	/* Look for a selection no one else is using. */

	/* XXX: This will become very slow if the app
	 * has > 100 selections in use.  We will go
	 * through > 100 XGetSelectionOwner() requests
	 * looking for a free selection.
	 */
	for (i = 0;; i++) {
		seln = InternSelection(server, i, (XID) xv_get(dnd->parent, XV_XID));
		if (XGetSelectionOwner(dpy, seln) == None) {
			dnd->transientSel = True;
			xv_set(DND_PUBLIC(dnd), SEL_RANK, seln, SEL_OWN, True, NULL);
			break;
		}
	}
	return DND_SUCCEEDED;
}

/* I'm just 'saving atoms' .... */
static Atom InternSelection(Xv_server server, int n, XID xid)
{
    char buf[60]; 

	/* orig was
	 * sprintf(buf, "_SUN_DRAGDROP_TRANSIENT_%d_%ld", n, xid);
	 */
    sprintf(buf, "_SUN_DRAGDROP_TRANSIENT_%d", n);
    return xv_get(server, SERVER_ATOM, buf);
}

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

/*ARGSUSED*/
Pkg_private Bool DndMatchProp(Display *dpy, XEvent *event, XPointer cldt)
{
	DnDWaitEvent *wE = (DnDWaitEvent *)cldt;

    if ((event->type == wE->eventType) &&
                           (((XPropertyEvent*)event)->atom == (Atom)wE->window))
        return(True);
    else
        return(False);
}
