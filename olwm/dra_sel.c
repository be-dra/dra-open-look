#include <stdio.h>
#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include "events.h"
#include "properties.h"
#include "selection.h"
#include "atom.h"

char dra_sel_c_sccsid[] = "@(#) %M% V%I% %E% %U% $Id: dra_sel.c,v 1.20 2025/02/25 12:09:27 dra Exp $";

extern Window NoFocusWin;

static void handlePseudoSelections(XEvent *event)
{
	XSelectionEvent reply;
	XSelectionRequestEvent *request;
	Bool do_request = False;
    unsigned long data[10];	/* long enough for most return values */
	unsigned char str[2];
    unsigned char *propdata;
    int format, nelements;
    Atom type;

	if (event->type == SelectionClear) {
		fprintf(stderr, "UNEXPECTED: losing _DRA_PSEUDO_...\n");
		return;
	}

	request = (XSelectionRequestEvent *) event;

	/*
	 * Set up a reply event for refusal.  If a conversion is successful, the 
	 * property field will be filled in appropriately.
	 */

	reply.type = SelectionNotify;
	reply.requestor = request->requestor;
	reply.selection = request->selection;
	reply.property = None;
	reply.target = request->target;
	reply.time = request->time;

	/* backwards compatibility per ICCCM section 2.2 */
	if (request->property == None) request->property = request->target;

	if (request->target == XA_STRING) {
		str[0] = '\0';
		nelements = 1;
		type = XA_STRING;
		format = 8;
    	propdata = str;
		reply.property = request->property;
		do_request = True;
	}
	else if (request->target == AtomTargets) {
		data[0] = AtomTargets;
		data[1] = XA_STRING;
		nelements = 2;
		type = XA_ATOM;
		format = 32;
    	propdata = (unsigned char *) data;
		reply.property = request->property;
	}

	if (reply.property != None) {
    	/* write the property, free it if necessary, and return success */
    	XChangeProperty(event->xany.display, reply.requestor, reply.property,
								type, format, PropModeReplace,
								(unsigned char *)propdata, nelements);
	}

	XSendEvent(event->xany.display, reply.requestor, False,
			NoEventMask, (XEvent *)&reply);

	if (do_request) {
		if (request->selection == AtomPseudoClipBoard) {
			/* request the PRIMARY STRING */
			dra_olwm_trace(300, "olwm: requesting PRIMARY STRING\n");
			XConvertSelection(event->xany.display, XA_PRIMARY, XA_STRING,
							XA_PRIMARY, NoFocusWin, request->time);
		}
		else if (request->selection == AtomPseudoSecondary) {
			/* request the SECONDARY _SUN_SELECTION_END */
			XConvertSelection(event->xany.display, XA_SECONDARY, AtomSelectEnd,
							XA_SECONDARY, NoFocusWin, request->time);
		}
	}
	XFlush(event->xany.display);
}

static unsigned char *clip_data = 0;

static Bool convertClip(Display *dpy, Window requestor, Atom target,
											Atom property)
{
    unsigned long data[10];	/* long enough for most return values */
	unsigned char str[2];
    unsigned char *propdata;
    int format, nelements;
    Atom type;

	if (target == XA_STRING) {
		if (clip_data) {
			nelements = (int)strlen((char *)clip_data);
			propdata = clip_data;
		}
		else {
			str[0] = '\0';
			nelements = 0;
			propdata = str;
		}
		dra_olwm_trace(710,
				"answering STRING for CLIPBOARD on property '%s' => '%s'\n", 
				XGetAtomName(dpy, property), propdata);
		type = XA_STRING;
		format = 8;
	}
	else if (target == AtomTargets) {
		data[0] = AtomTargets;
		data[1] = AtomMultiple;
		data[2] = XA_STRING;
		nelements = 3;
		type = XA_ATOM;
		format = 32;
    	propdata = (unsigned char *) data;
	}
	else {
		dra_olwm_trace(710,
				"unknown target '%s' requested for CLIPBOARD\n", 
				XGetAtomName(dpy, target));
		return False;
    }

    /* write the property and return success */

    XChangeProperty(dpy, requestor, property, type, format, PropModeReplace,
		    (unsigned char *)propdata, nelements);
    return True;
}


static void handleClipboard(XEvent *event)
{
	XSelectionEvent reply;
	XSelectionRequestEvent *request;
	Atom *pairs;
	int i;
	Bool writeback = False;
	unsigned long nitems, remain;

	if (event->type == SelectionClear) {
		dra_olwm_trace(678, "olwm: losing CLIPBOARD\n");
		return;
	}

	request = (XSelectionRequestEvent *) event;

	/*
	 * Set up a reply event for refusal.  If a conversion is successful, the 
	 * property field will be filled in appropriately.
	 */

	reply.type = SelectionNotify;
	reply.requestor = request->requestor;
	reply.selection = request->selection;
	reply.property = None;
	reply.target = request->target;
	reply.time = request->time;

	if (request->target == AtomMultiple) {
		if (request->property != None) {
			pairs = GetWindowProperty(request->display, request->requestor,
					request->property, 0L, 100000L,
					AtomAtomPair, 32, &nitems, &remain);
			if (pairs != NULL) {
				/*
				 * Process each pair of atoms (target, property).  Watch
				 * out for an odd last atom, and for property atoms of
				 * None.  If the conversion fails, replace it with None in
				 * the original property.
				 */
				for (i = 0; i + 1 < nitems; i += 2) {
					if (pairs[i + 1] == None)
						continue;

					if (!convertClip(request->display,
									request->requestor, pairs[i], pairs[i+1]))
					{
						pairs[i + 1] = None;
						writeback = True;
					}
				}
				if (writeback)
					XChangeProperty(request->display, request->requestor,
							request->property, AtomAtomPair, 32,
							PropModeReplace, (unsigned char *)pairs,
							nitems);

				XFree((char *)pairs);
				reply.property = request->property;
			}
		}
	}
	else {
		/* backwards compatibility per ICCCM section 2.2 */
		if (request->property == None)
			request->property = request->target;

		if (convertClip(request->display, request->requestor,
						request->target, request->property)) {
			reply.property = request->property;
		}
	}

	XSendEvent(event->xany.display, reply.requestor, False,
			NoEventMask, (XEvent *) & reply);
	XFlush(event->xany.display);
}

void dra_sel_reply(XEvent *pEvent)
{
	XSelectionEvent *sel = (XSelectionEvent *)pEvent;
	Atom act_typeatom;
	int status, act_format;
	unsigned long items, rest;
	long maxreq = XMaxRequestSize(sel->display);

	if (sel->property == None) return;

	if (sel->selection == XA_PRIMARY && sel->target == XA_STRING) {
		dra_olwm_trace(710, "PRIMARY answer: prop %x\n", sel->property);

		if (clip_data) XFree(clip_data);
		clip_data = 0;

		status = XGetWindowProperty(sel->display, sel->requestor, sel->property,
						0L, maxreq - 1, True,
						AnyPropertyType, &act_typeatom, &act_format,
						&items, &rest, &clip_data);

		if (status == Success) {
			if (act_format == 8) {
				clip_data[items] = '\0';
				if (rest != 0) {
					fprintf(stderr, "unexpected: PRIMARY answer has more than %ld bytes: rest %ld\n", maxreq, rest);
				}
				dra_olwm_trace(678, "olwm: owning CLIPBOARD\n");
				dra_olwm_trace(710, "owning CLIPBOARD for (len %ld) '%s'\n",
												items, clip_data);
				XSetSelectionOwner(sel->display, AtomClipBoard, sel->requestor,
								LastEventTime);
			}
			else {
				fprintf(stderr, "unexpected selection property format %d\n",
								act_format);
				return;
			}
		}
		else {
			clip_data = 0;
		}
	}
	else if (sel->selection == XA_SECONDARY && sel->target == AtomSelectEnd) {
		status = XDeleteProperty(sel->display, sel->requestor, sel->property);
	}
	else {
		fprintf(stderr, "unexpected selection notification %ld, %ld\n",
							sel->selection, sel->target );
		return;
	}
}

void dra_sel_init(Display *dpy, Window win, Time ts)
{
	/* die Idee: neuerdings (Jan 2009) kann ich in xterm kein COPY
	 * mehr (erfolgreich) machen (warum auch immer - der Code sah mir
	 * so aus, als ob xterm sowieso nur EINE Selection haben koennte).
	 * Und ich will, dass xterm bei PRIMARY bleibt, also weiterhin
	 * *selectToClipboard: False
	 *
	 * Also: olwm wird (beim Start) selection owner fuer
	 * _DRA_PSEUDO_CLIPBOARD. in die xterm-translations schreib ich 
	 * fuer COPY rein:  insert-selection(_DRA_PSEUDO_CLIPBOARD),
	 * d.h. xterm macht beim copy-Event einen Request auf STRING gegen
	 * die Selection _DRA_PSEUDO_CLIPBOARD.
	 * Der olwm antwortet NUR auf XA_STRING mit einem leeren String,
	 * (d.h. xterm kann nicht wirklich was einfuegen)
	 * macht dann aber einen Selection Request fuer PRIMARY, STRING.
	 * Wenn er da was kriegt, speichert er das und wird Selection Owner
	 * fuer CLIPBOARD. Wenn dann da einer nach STRING fragt, bekommt er das...
	 */
	/* The idea: recently (jan 2009) I can't perform a successful COPY
	 * in xterm (no idea why - the code looked as if xterm could handle only
	 * ONE selection anyway).
	 * And I want xterm to stick to PRIMARY, so
	 * *selectToClipboard: False
	 *
	 * So: olwm becomes selection owner for _DRA_PSEUDO_CLIPBOARD when it
	 * starts.
	 * My xterm translation contains 
	 *    <KeyPress>F16: insert-selection(_DRA_PSEUDO_CLIPBOARD)
	 * (F16 is my COPY key)
	 *
	 * so, xterm performs a selection request for STRING against
	 * _DRA_PSEUDO_CLIPBOARD.
	 * Now, olwm answers ONLY on XA_STRING with an empty string (so, xterm
	 * cannot really insert anything) and then performs a selection request
	 * on 'PRIMARY, STRING'. If it gets something, saves this and becomes
	 * selection owner for CLIPBOARD. If anybody asks for (CLIPBOARD, STRING),
	 * they will get the saved string.
	 *
	 * I know: it would have looked much simpler and easier to provide a
	 * translation of the form
	 *    <KeyPress>F16: select-end(CLIPBOARD)
	 * or, for those people who think Ctrl-c is a reasonable shortcut 
	 * for COPY:
	 *    c<KeyPress>c: select-end(CLIPBOARD)
	 *
	 * however, the inventors of xterm want the 'select-end' action to be
	 * invoked with a mouse button event.... 
	 */
    SelectionRegister(AtomClipBoard, handleClipboard);
    SelectionRegister(AtomPseudoClipBoard, handlePseudoSelections);
	XSetSelectionOwner(dpy, AtomPseudoClipBoard, win, ts);

	/* Now, an additional idea came into my mind: I wanted xterm to be able
	 * to perform quick duplicate. This could be easily done by a translation
	 * <KeyRelease>F18: insert-selection(SECONDARY,CLIPBOARD,NIX1,NIX2,NIX3)
	 * This will first attempt to get the SECONDARY selection, and if that
	 * fails, the CLIPBOARD will be queried.
	 * However, in case of a quick duplicate, the SECONDARY selection will
	 * be kept - and the next PASTE event (intended for CLIPBOARD) will still
	 * produce the SECONDARY text....
	 * So, let's make an additional selection _DRA_PSEUDO_SECONDARY which
	 * answers a STRING request with an empty string AND attempts to
	 * request _SUN_SELECTION_END against SECONDARY. So, I need a translation
	 * <KeyRelease>F18: insert-selection(SECONDARY,CLIPBOARD,NIX1,NIX2,NIX3) insert-selection(_DRA_PSEUDO_SECONDARY)
	 */
    SelectionRegister(AtomPseudoSecondary, handlePseudoSelections);
	XSetSelectionOwner(dpy, AtomPseudoSecondary, win, ts);
}
