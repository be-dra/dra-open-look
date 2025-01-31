#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)dnd_decode.c 1.15 93/06/28 DRA: $Id: dnd_decode.c,v 4.5 2025/01/07 19:20:22 dra Exp $ ";
#endif
#endif

/*
 *      (c) Copyright 1990 Sun Microsystems, Inc. Sun design patents
 *      pending in the U.S. and foreign countries. See LEGAL NOTICE
 *      file for terms of the license.
 */

#include <sys/time.h>
#include <X11/Xatom.h>
#include <xview/xview.h>
#include <xview/server.h>
#include <xview/window.h>
#include <xview/sel_pkg.h>
#include <xview/dragdrop.h>
#include <xview_private/dndimpl.h>
#include <xview_private/xv_list.h>
#include <xview_private/sel_impl.h>
#ifdef NO_XDND
#else /* NO_XDND */
#  include <xview_private/windowimpl.h>
extern int DndSendEvent(Display *dpy, XEvent *, const char *);
#endif /* NO_XDND */

static int SendACK(Selection_requestor sel_req, Event *ev);

typedef struct dnd_drop_site {
    Xv_sl_link           next;
    Xv_drop_site         drop_item;
} Dnd_drop_site;

static int dnd_is_xdnd_key, dnd_transient_key = 0;

Xv_public Xv_opaque dnd_decode_drop(Selection_requestor sel_req, Event *event)
{
	XClientMessageEvent *cM;
	Dnd_drop_site *site;


	if (!(event_action(event) == ACTION_DRAG_COPY ||
					event_action(event) == ACTION_DRAG_MOVE))
		return (DND_ERROR);

	if (!dnd_transient_key) {
		dnd_transient_key = xv_unique_key();

#ifdef NO_XDND
#else /* NO_XDND */
		dnd_is_xdnd_key = xv_unique_key();
#endif /* NO_XDND */
	}

	cM = (XClientMessageEvent *) event_xevent(event);

	if (cM->message_type != (Atom) xv_get(XV_SERVER_FROM_WINDOW(xv_get(sel_req,
									XV_OWNER)), SERVER_ATOM,
					"_SUN_DRAGDROP_TRIGGER"))
		return (DND_ERROR);

#ifdef NO_XDND
#else /* NO_XDND */
	xv_set(sel_req,
			XV_KEY_DATA, dnd_is_xdnd_key,
			((event_flags(event) & DND_IS_XDND) != 0), NULL);
#endif /* NO_XDND */

	/* Remind ourself to send _SUN_DRAGDROP_DONE in dnd_done() */
	if (DND_IS_TRANSIENT(event))
		xv_set(sel_req, XV_KEY_DATA, dnd_transient_key, True, NULL);

	/* Set the rank of the selection to the rank being used in
	 * the drag and drop transaction.
	 */
	xv_set(sel_req,
				SEL_RANK, cM->data.l[0],
				SEL_TIME, &event_time(event),
				NULL);

	/* If the acknowledgement flag is set, send an ack. */
	if (cM->data.l[4] & DND_ACK_FLAG) {
		if (SendACK(sel_req, event) == DND_ERROR) return DND_ERROR;
	}

	/* Find the drop site that was dropped on. */
	site = (Dnd_drop_site *) xv_get(event_window(event), WIN_ADD_DROP_ITEM);

	/* SUPPRESS 560 */
	while ((site = (Dnd_drop_site *) (XV_SL_SAFE_NEXT(site)))) {
		if ((long)xv_get(site->drop_item, DROP_SITE_ID) == (long)cM->data.l[3])
			return (site->drop_item);
	}

	return (DND_ERROR);
}

static int SendACK(Selection_requestor sel_req, Event *ev)
{
	Xv_Server server = XV_SERVER_FROM_WINDOW(event_window(ev));

	if (dnd_is_local(ev)) {	/* flag set to True in local case */
		Attr_attribute dndKey = xv_get(server, SERVER_DND_ACK_KEY);

		xv_set(server, XV_KEY_DATA, dndKey, True, NULL);
		return DND_SUCCEEDED;
	}
	else {	/* Remote case */
		char *data;
		int format;
		long length;

		xv_set(sel_req, SEL_TYPE_NAME, "_SUN_DRAGDROP_ACK", NULL);
		data = (char *)xv_get(sel_req, SEL_DATA, &length, &format);
    	if (data) XFree(data);
		if (length == SEL_ERROR) {
			return DND_ERROR;
		}
		return DND_SUCCEEDED;
	}
}

Xv_public void dnd_done(Selection_requestor sel_req)
{
	/* this is a little strange: the holy XView bible says that the
	 * application is supposed to call dnd_done when the whole dnd
	 * transaction has been performed, It says
	 * "The function informs the toolkit that the drag and drop operation
	 *    has been completed."
	 * 
	 * However, I can find nowhere (not in SELECTION_OWNER nor in DRAGDROP)
	 * any code that handles (or at least, recognizes) _SUN_DRAGDROP_DONE....
	 * There is something in TEXTSW - but this is, from the DND point of view,
	 * the application.
	 *
	 * Let's find a place to implement that (probably in SELECTION_OWNER)
	 * - it will be Reference (jgvesfvchjerwvj)
	 */

	if (xv_get(sel_req, XV_KEY_DATA, dnd_transient_key)) {
		int format;
		long length;
		reply_proc_t reply_proc;

		/* the intention here is obviously to have 
		 * 
		 * temporarily NO reply proc !!
		 *
		 * however, subclasses of Selection_requestor might intercept
		 * the setting of SEL_REPLY_PROC and do funny things....
		 *
		 * Therefore, we do not use xv_get (and later xv_set) to 
		 * manipulate the reply procedure - instead, we directly access
		 * the selection requestor's private part...
		 */
		Sel_req_info *srpriv = SEL_REQUESTOR_PRIVATE(sel_req);

		/* and, in addition to all that - could it be we produced a little
		 * memory leak here??
		 */
		char *data;

		reply_proc = srpriv->reply_proc;
		srpriv->reply_proc = 0;

		xv_set(sel_req, XV_KEY_DATA, dnd_transient_key, False, NULL);
		xv_set(sel_req, SEL_TYPE_NAME, "_SUN_DRAGDROP_DONE", NULL);

		data = (char *)xv_get(sel_req, SEL_DATA, &length, &format);

    	if (data) XFree(data);
		srpriv->reply_proc = reply_proc;

		{
			Xv_window win;
			Display *dpy;
			win = xv_get(sel_req, XV_OWNER);
			dpy = XV_DISPLAY_FROM_WINDOW(win);
			/* das soll fuer ALLE properties 'avail = TRUE' setzen */
    		xv_sel_free_property(dpy, 0L);
		}
	}

#ifdef NO_XDND
#else /* NO_XDND */
	/* this assumes that
	 * -   'sel_req' is the selection_requestor that already
	 *     did the dnd_decode_drop
	 * -   the owner of sel_req was the window with the hit drop site
	 * -   the XdndDrop event was sent to this window's frame
	 */

	if (xv_get(sel_req, XV_KEY_DATA, dnd_is_xdnd_key)) {
		Xv_window win = xv_get(sel_req, XV_OWNER);

		if (win) {
			Frame frame = xv_get(win, WIN_FRAME);
			Window_info *framepriv = WIN_PRIVATE(frame);

			if (framepriv->xdnd_sender) {
				XClientMessageEvent cM;

				cM.type = ClientMessage;
				cM.display = (Display *)xv_get(frame, XV_DISPLAY);
				cM.format = 32;
				cM.message_type = xv_get(XV_SERVER_FROM_WINDOW(frame),
									SERVER_ATOM, "XdndFinished");
				cM.window = framepriv->xdnd_sender;
				cM.data.l[0] = xv_get(frame, XV_XID);
				cM.data.l[1] = 1;
				cM.data.l[2] = xv_get(XV_SERVER_FROM_WINDOW(frame),
									SERVER_ATOM, "XdndActionCopy");
				cM.data.l[3] = 0;
				cM.data.l[4] = 0;
				DndSendEvent(cM.display, (XEvent *)&cM, "XdndFinished");
			}
		}
	}
	xv_set(sel_req, XV_KEY_DATA, dnd_is_xdnd_key, FALSE, NULL);
#endif /* NO_XDND */
}
