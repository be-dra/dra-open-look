#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)dnd_dsdm.c 1.8 93/06/28 DRA: $Id: dnd_dsdm.c,v 4.5 2024/12/24 08:07:14 dra Exp $ ";
#endif
#endif

/*
 *      (c) Copyright 1990 Sun Microsystems, Inc. Sun design patents
 *      pending in the U.S. and foreign countries. See LEGAL NOTICE
 *      file for terms of the license.
 */

#include <stdio.h>
#include <sys/time.h>
#include <xview/xview.h>
#include <xview/dragdrop.h>
#include <xview_private/dndimpl.h>
#include <xview_private/portable.h>

static int dnd_site_key	= 0;

#define ATOM(name)	(Atom)xv_get(server, SERVER_ATOM, name)
#define POINT_IN_SITE(sr, px, py) \
			((px) >= (sr).x && (py) >= (sr).y && \
			 (px) < (sr).x+(sr).w && (py) < (sr).y+(sr).h)
#define SCREENS_MATCH(dnd, i) \
			(dnd->siteRects[i].screen_number == dnd->screenNumber)

static void ReplyProc(Selection_requestor sel, Atom target, Atom type,
							Xv_opaque buffer, unsigned long length, int format)
{
	Xv_server server = XV_SERVER_FROM_WINDOW(xv_get(sel, XV_OWNER));

	/* ORIG: if (target == ATOM("_SUN_DRAGDROP_DSDM")), but _SUN_DRAGDROP_DSDM
	 * was the SEL_RANK, while the SEL_TYPE was "_SUN_DRAGDROP_SITE_RECTS"
	 *
	 * I don't believe that this has ever worked - but on the other it was
	 * probably never used because dsdm = olwm does not answer with INCR...
	 */
	if (target == ATOM("_SUN_DRAGDROP_SITE_RECTS")) {
		Dnd_info *dnd = (Dnd_info *) xv_get(sel, XV_KEY_DATA, dnd_site_key);

		/* Only handle INCR responses in ReplyProc(). */
		if (type == ATOM("INCR")) {
			/* We are in incr mode. */
			dnd->incr_mode = True;
			dnd->incr_size = 0;
		}
		else if (length && dnd->incr_mode) {
			if (!dnd->incr_size)
				dnd->siteRects = (DndSiteRects *) xv_malloc(4 * length);
			else
				dnd->siteRects = (DndSiteRects *) xv_realloc(dnd->siteRects,
						dnd->incr_size + (4 * length));
			XV_BCOPY((char *)buffer, (char *)(dnd->siteRects + dnd->incr_size),
					(4 * length));
			dnd->incr_size += (4 * length);
		}
		else if (dnd->incr_mode) {
			dnd->incr_size = 0;
			dnd->incr_mode = False;
		}
	}
}

Xv_private int DndContactDSDM(Dnd_info	*dnd)
{
	unsigned long length;
	int format;
	struct timeval *time;
	DndSiteRects *siteRects;

	if (!dnd->dsdm_selreq) {
		Xv_object owner, server;

		owner = (Xv_object) xv_get(DND_PUBLIC(dnd), XV_OWNER);

		server = XV_SERVER_FROM_WINDOW(owner);

		/* XXX: For multiple dnd objects, could use the same window. */
		dnd->window = xv_create(owner, WINDOW,
				WIN_INPUT_ONLY,
				XV_X, 0,
				XV_Y, 0,
				XV_WIDTH, 1,
				XV_HEIGHT, 1,
				XV_SHOW, FALSE,
				NULL);

		dnd->dsdm_selreq = xv_create(dnd->window, SELECTION_REQUESTOR,
				SEL_RANK, dnd->atom[DSDM],
				SEL_REPLY_PROC, ReplyProc,
				SEL_TYPE, ATOM("_SUN_DRAGDROP_SITE_RECTS"),
				NULL);
	}

	/* Set the time if we know what it is. */
	if ((time = (struct timeval *)xv_get(DND_PUBLIC(dnd), SEL_TIME)) != NULL)
		xv_set(dnd->dsdm_selreq, SEL_TIME, time, NULL);

	if (dnd->siteRects) {
		xv_free(dnd->siteRects);
		dnd->siteRects = NULL;
	}

	/* Hang the private dnd info off the selection object so we can
	 * access it in the ReplyProc.
	 */
	if (dnd_site_key == 0)
		dnd_site_key = xv_unique_key();

	xv_set(dnd->dsdm_selreq, XV_KEY_DATA, dnd_site_key, (char *)dnd, NULL);

	siteRects = (DndSiteRects *) xv_get(dnd->dsdm_selreq, SEL_DATA, &length, &format);
	/* If the dsdm responded with INCR then siteRects should be NULL and
	 * dnd->siteRects will contain the data.
	 */
	if (siteRects)
		dnd->siteRects = siteRects;

	dnd->numSites = length / 8;
	dnd->lastSiteIndex = 0;
	dnd->eventSiteIndex = DND_NO_SITE;

	if (!dnd->siteRects)
		return (False);
	return (True);
}

Xv_private int DndFindSite(Dnd_info *dnd, XButtonEvent *e)
{
	int i;

	if (POINT_IN_SITE(dnd->siteRects[dnd->lastSiteIndex], e->x_root, e->y_root))
	{
		return DndSendPreviewEvent(dnd, dnd->lastSiteIndex, (XEvent *) e);
	}

	/* Determine the number of the screen that the mouse is currently in. */
	if (dnd->lastRootWindow != e->root) {	/* Same root window? */
		dnd->lastRootWindow = e->root;	/* Cache root window */
		for (i = 0; i < ScreenCount(e->display); i++) {
			if (e->root == RootWindowOfScreen(ScreenOfDisplay(e->display, i)))
				dnd->screenNumber = i;
		}
	}

	for (i = 0; i < dnd->numSites; i++) {
		if (SCREENS_MATCH(dnd, i) &&
				POINT_IN_SITE(dnd->siteRects[i], e->x_root, e->y_root)) {
			dnd->lastSiteIndex = i;
			return DndSendPreviewEvent(dnd, dnd->lastSiteIndex, (XEvent *) e);
		}
	}
	return DndSendPreviewEvent(dnd, DND_NO_SITE, (XEvent *) e);
}
