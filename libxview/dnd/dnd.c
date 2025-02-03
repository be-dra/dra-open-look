#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)dnd.c 1.30 93/06/28 DRA: $Id: dnd.c,v 4.17 2025/02/02 19:54:36 dra Exp $ ";
#endif
#endif

/*
 *      (c) Copyright 1990 Sun Microsystems, Inc. Sun design patents
 *      pending in the U.S. and foreign countries. See LEGAL NOTICE
 *      file for terms of the license.
 */

#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <xview/xview.h>
#include <xview/cursor.h>
#include <xview/dragdrop.h>
#include <xview_private/dndimpl.h>
#include <xview_private/windowimpl.h>
#include <xview_private/win_info.h>
#include <xview_private/svr_impl.h>
#include <xview/win_notify.h>
#include <sys/utsname.h>

static int Verification(XButtonEvent *ev, Dnd_info *dnd);
static int ConstructSiteList(Display *dpy, Window dest_window, long *prop,
 Dnd_site_desc **dsite, unsigned int *nsites);
static int FindDropSite(Dnd_info *dnd, Dnd_site_desc *dsl, /* drop site list */
 unsigned int nsites, Dnd_site_desc *site);
static int SendDndEvent(Dnd_info *dnd, DndMsgType type, long subtype,
							XButtonEvent *ev);

Xv_private Xv_opaque server_get_timestamp(Xv_Server server_public);

/* DND_HACK begin */
/* The code highlighted by the words DND_HACK is here to support dropping
 * on V2 clients.  The V3 drop protocol is not compatibile with the V2.
 * If we detect a V2 application, by a property on the its frame, we try
 * to send an V2 style drop event.   This code can be removed once we decide
 * not to support running V2 apps with the latest release.
 */
static Window FindLeafWindow(XButtonEvent *ev);
/* DND_HACK end */

static void UpdateGrabCursor( Dnd_info *dnd, int type, int rejected, Time t);

static int SendTrigger(Dnd_info *dnd, Xv_Drawable_info *info,
						XButtonEvent *buttonEvent, int local);
static int SendOldDndEvent(Dnd_info *dnd, XButtonEvent *buttonEvent);
static int IsV2App(Display *dpy, Window window, Dnd_info *dnd, XButtonEvent *ev);
static int WaitForAck(Dnd_info *dnd, Xv_Drawable_info *info);

#ifdef NO_XDND
#else /* NO_XDND */

/* trace level: */
#define TLXDND 411

static int dnd_key = 0;

typedef int (*convert_func)(Selection_owner,Atom *,Xv_opaque *, unsigned long *,int *);

/* this is the selection owner that handles Xdnd drop requests */
static int delegate_convert_selection(Selection_owner selown, Atom *type,
				Xv_opaque *value, unsigned long *length, int *format)
{
	Drag_drop dnd_public = xv_get(selown, XV_KEY_DATA, dnd_key);
	convert_func cf = (convert_func)xv_get(dnd_public, SEL_CONVERT_PROC);
	Dnd_info *dnd = DND_PRIVATE(dnd_public);
	Xv_opaque server = XV_SERVER_FROM_WINDOW(dnd->parent);

	SERVERTRACE((TLXDND, "received sel req '%s'\n", 
						(char *)xv_get(server, SERVER_ATOM_NAME, *type)));

	if (*type == xv_get(server, SERVER_ATOM, "XdndFinished")) {
		/* in win_input.c, we 'convert' a XdndFinished client message
		 * into a Selection request. It will (hopefully) do no harm
		 * if we refuse..  the only purpose of the whole thing was
		 * to disown the XdndSelection
		 */
		xv_set(selown, SEL_OWN, FALSE, NULL);
		xv_set(dnd_public, SEL_OWN, FALSE, NULL);
		return FALSE;
	}

	/* we do not want to be too clever here on this generic level
	 * where we know nothing....
	 */ 
	return (*cf)(dnd_public, type, value, length, format);
}

#ifdef XQueryTree_LEIDER_UEBERS_NETZ_SEHR_LANGSAM
static void init_toplevels(Display *dpy, Window parent, Atom wmstate,
						Window *tls, int *cntp)
{
	int act_format = 0;
	Atom act_typeatom;
	unsigned long items, rest;
	unsigned char *bp;
	unsigned i, nch;
	Window u, *ch;

	if (XGetWindowProperty(dpy, parent, wmstate,
				0L, 1000L, FALSE, wmstate, &act_typeatom,
				&act_format, &items, &rest, &bp) == Success &&
			act_format == 32)
	{
		XFree(bp);
		tls[*cntp] = parent;
		++(*cntp);
		return;
	}
	XQueryTree(dpy, parent, &u, &u, &ch, &nch);
	for (i = 0; i < nch; i++) {
		init_toplevels(dpy, ch[i], wmstate, tls, cntp);
	}

	if (ch) XFree(ch);
}

static void fill_dnd_toplevel_cache(Dnd_info *dnd, Xv_window dragsource, Atom wmstate)
{
	Xv_screen screen = XV_SCREEN_FROM_WINDOW(dragsource);
	Window *toplevels = (Window *)xv_get(screen, XV_KEY_DATA, dnd_key);
	Xv_window xvroot = xv_get(screen, XV_ROOT);
	Window root = xv_get(xvroot, XV_XID);
	Display *dpy = (Display *)xv_get(dragsource, XV_DISPLAY);
	int idx = 0;

	if (! toplevels) {
		toplevels = xv_alloc_n(Window, 100L); /* 100 should be enough */
		xv_set(screen, XV_KEY_DATA, dnd_key, toplevels, NULL);
	}
	init_toplevels(dpy, root, wmstate, toplevels, &idx);
	toplevels[idx] = None;
}
#else /* XQueryTree_LEIDER_UEBERS_NETZ_SEHR_LANGSAM */
static void fill_dnd_toplevel_cache(Dnd_info *dnd, Xv_window dragsource, Atom unused_wmstate)
{
	Selection_requestor sr;
	Window *result;
	unsigned long length;
	int format;

	if (dnd->tl_cache) xv_free(dnd->tl_cache) ;

	sr = xv_create(dragsource, SELECTION_REQUESTOR,
						SEL_RANK_NAME, "_SUN_DRAGDROP_DSDM",
						SEL_TYPE_NAME, "_DRA_TOP_LEVEL_WINDOWS",
						NULL);

	result = (Window *)xv_get(sr, SEL_DATA, &length, &format);
	if (length == SEL_ERROR) {
		dnd->tl_cache = NULL;
	}
	else {
		dnd->tl_cache = result;
	}
	xv_destroy(sr);
}
#endif /* XQueryTree_LEIDER_UEBERS_NETZ_SEHR_LANGSAM */

/* we return a window only if 
 * +++ it is NOT the root window
 * +++ it does NOT have a INTEREST property
 * +++ it does have a WMSTATE property
 */
static Window find_xdnd_top_level(Dnd_info *dnd, XEvent *xbm)
{
	Display *dpy;
	Window dest, frm, ch;
	int nx, ny, sx, sy;
	int count_xtc = 0;

	dest = xbm->xany.window;
	dpy = xbm->xany.display;
	/* xbm kann ein Button oder Motion sein - das ist fuer die
	 * folgenden Komponenten egal, drum 'nennen' wir es 'button'
	 */
	frm = ch = xbm->xbutton.root;
	sx = xbm->xbutton.x;
	sy = xbm->xbutton.y;

	/* kann man da nicht mit _DRA_TOP_LEVEL_WINDOWS arbeiten?
	 * Kann man schon, aber dann muss man fuer jedes TopLevelWindow
	 * pruefen, ob xbm auch wirklich "da drin" ist : womoeglich werden
	 * das mehr XTranslateCoordinates-Aufrufe....
	 * Gewoehnlich habe wir hier 3 XTranslateCoordinates-Aufrufe,
	 * aber mein UI hat mindestens 17 Toplevel-Windows - und fuer jedes
	 * muss ich XTranslateCoordinates aufrufen, um herauszufinden, wo
	 * die Maus ist - also durchschnittlich 8 Aufrufe.
	 */
	while (ch && XTranslateCoordinates(dpy,dest,frm,sx,sy,&nx,&ny,&ch)) {
		int act_format = 0;
		Atom act_typeatom;
		unsigned long items, rest;
		unsigned char *bp;

		++count_xtc;
		if (dnd->tl_cache) {
			int i;
			for (i = 0; dnd->tl_cache[i]; i++) {
				if (frm == dnd->tl_cache[i]) {
					if (XGetWindowProperty(dpy, frm, dnd->atom[INTEREST], 0L,1L,
							False, AnyPropertyType, &act_typeatom, &act_format,
							&items, &rest, &bp) == Success &&
						act_format == 32)
					{
						/* this is one of us - we don't need any Xdnd things */
						XFree(bp);
						frm = None;
					}

					SERVERTRACE((950, "%d XTranslateCoord: 0x%lx\n",
										count_xtc, frm));
					return frm;
				}
			}
		}
		else {
			if (XGetWindowProperty(dpy, frm, dnd->atom[WMSTATE],
					0L, 1000L, FALSE, dnd->atom[WMSTATE], &act_typeatom,
					&act_format, &items, &rest, &bp) == Success &&
				act_format == 32)
			{
				dest = frm;
				XFree(bp);
				if (XGetWindowProperty(dpy, frm, dnd->atom[INTEREST], 0L, 1L,
							False, AnyPropertyType, &act_typeatom, &act_format,
							&items, &rest, &bp) == Success &&
					act_format == 32)
				{
					/* this is one of us - we don't need any Xdnd things */
					XFree(bp);
					SERVERTRACE((950, "%d XTranslateCoord 0x0\n", count_xtc));
					return None;
				}
				break;
			}
		}
		sx = nx;
		sy = ny;
		dest = frm;
		frm = ch;
	}

	if (dest == xbm->xbutton.root) {
		/* we do NOT return the root window here */
		dest = None;
	}
	SERVERTRACE((950, "%d XTranslateCoord: 0x%lx\n", count_xtc, dest));
	return dest;
}

static void send_preview_leave(Display *dpy, Dnd_info *dnd, Time t)
{
	XEvent cM;

	cM.xclient.type = ClientMessage;
	cM.xclient.display = dpy;
	cM.xclient.format = 32;
	cM.xclient.message_type = dnd->atom[XdndLeave];
	cM.xclient.window = dnd->last_top_win;
	cM.xclient.data.l[0] = (Window)xv_get(dnd->parent, XV_XID);
	cM.xclient.data.l[1] = 0;
	cM.xclient.data.l[2] = 0;
	cM.xclient.data.l[3] = 0;
	cM.xclient.data.l[4] = 0;

	SERVERTRACE((TLXDND, "sending XdndLeave to %lx\n", dnd->last_top_win));
	DndSendEvent(dpy, (XEvent *)&cM, "XdndLeave");
	UpdateGrabCursor(dnd, LeaveNotify, FALSE, t);
}

#endif /* NO_XDND */

#ifdef INVESTIGATION_OF_STRANGE_LOG_DRAG
typedef struct {
	Window window;
} dndctxt_t;

static Bool check_dnd_drag(Display *dpy, XEvent *ev, char *arg)
{
	dndctxt_t *ctxt = (dndctxt_t *)arg;

	if (ev->type != MotionNotify) return False;
	if (ev->xmotion.window != ctxt->window) return False;
	if ((ev->xmotion.state & Button1Mask) == 0) return False;

	return True;
}
#endif /* INVESTIGATION_OF_STRANGE_LOG_DRAG */

extern Xv_object xview_x_input_readevent(Display *display, Event *event,
								Xv_object req_window, int block, int type,
								unsigned int xevent_mask, XEvent *rep);

Xv_public int dnd_send_drop(Drag_drop dnd_public)
{
	Dnd_info *dnd = DND_PRIVATE(dnd_public);
	int status = DND_SUCCEEDED,
			dsdm_present = False,
			button_released = False, win_was_deaf = False,
			stop = False;	/* Stop key pressed */
	Display *dpy;
	XEvent *ev, xevent;
	Event event;
	Xv_Drawable_info *info;
	Window_info *win_info;
	unsigned long lasttime;
	int i;
	Atom *tl;
	Xv_server srv;

	event_init(&event);
	DRAWABLE_INFO_MACRO(dnd->parent, info);
	dpy = xv_display(info);
	srv = XV_SERVER_FROM_WINDOW(dnd->parent);
	lasttime = server_get_timestamp(srv);

	/* Assure that we have a selection to use. */
	if (DndGetSelection(dnd, dpy) == DND_SELECTION)
		return (DND_SELECTION);

	/* in our first version, we entered (hardcoded) 'text/plain'
	 * and 'text/uri-list' - not knowing, what is correct.
	 * Thinking about a way to ask (especially unknown subclasses)
	 * about a sort of TARGETS list, we came to the idea of using
	 * the attribute SEL_NEXT_ITEM, which has some advantages:
	 * - in the SELECTION package, it is a GET ONLY attribute
	 * - it belongs to the correct package, so no problems with
	 *   xv_check_bad_attr
	 * - it is an SEL_ATTR(ATTR_OPAQUE,	60), so, we can use it
	 *   to supply an Atom array to be filled
	 *
	 * However, this is only useful for subclasses of DRAGDROP.
	 * But a way to attach TARGETS to a dnd would also be a 
	 * convention to use SEL_NEXT_ITEM as a XV_KEY_DATA key
	 * to attach an Atom array
	 *
	 * Decision (10.03.2020): we use XV_KEY_DATA, SEL_NEXT_ITEM
	 * for a list of atoms. HOWEVER: this is not really TARGETS
	 * which contains a lot of ATOMS that are somehow 'secondary' like
	 * TARGETS, _SUN_SELECTION_END, _SUN_DRAGDROP_DONE, DELETE.
	 * 
	 * Convention: this atom list should contain ONLY atoms that
	 * describe the 'main data type' of the dragged data.
	 */

	memset(dnd->targetlist, '\0', sizeof(dnd->targetlist));

	tl = (Atom *)xv_get(DND_PUBLIC(dnd), XV_KEY_DATA, SEL_NEXT_ITEM);

	if (tl) {
		dnd->numtargets = MAXTARGETS;
		for (i = 0; i < MAXTARGETS; i++) {
			if (tl[i]) {
				dnd->targetlist[i] = tl[i];
			}
			else {
				dnd->targetlist[i] = (Atom)0;
				dnd->numtargets = i;
				i = MAXTARGETS;
				break;
			}
		}
	}
	else {
		dnd->numtargets = 0;
	}

	if (dnd->numtargets > 0) {
		XChangeProperty(dpy, xv_xid(info), dnd->atom[PREVIEW], XA_ATOM, 32,
						PropModeReplace, (unsigned char *)dnd->targetlist,
						dnd->numtargets);
	}

#ifdef NO_XDND
#else /* NO_XDND */
	if (! dnd_key) dnd_key = xv_unique_key();

	dnd->last_top_win = None;
	dnd->last_top_win_is_aware = FALSE;
	dnd->xdnd_status_from_last_top_win_seen = FALSE;
	fill_dnd_toplevel_cache(dnd, dnd->parent, dnd->atom[WMSTATE]);

	/* our research about KDE tools (konqueror, kwrite, amarok)
	 * have shown that when you drag files from konqueror,
	 * if the applications ask the dragger to convert 'text/plain'
	 * konqueror will send a newline separated list of all
	 * dragged filenames.
	 * (This example might show you that they are blind idiots...)
	 * So, our idea here is to record a notion of a 'file drag'
	 * (probably from fileman or similar) and evaluate this
	 * later (in delegate_convert_selection) in case the 
	 * requestor asks for 'text/plain'
	 */
	if (! dnd->xdnd_owner) {
		dnd->xdnd_owner = xv_create(dnd->parent, SELECTION_OWNER,
				SEL_RANK, dnd->atom[XdndSelection],
				SEL_CONVERT_PROC, delegate_convert_selection,
				XV_KEY_DATA, dnd_key, dnd_public,
				NULL);
	}
	xv_set(dnd->xdnd_owner, SEL_OWN, TRUE, NULL);
#endif /* NO_XDND */

	/* Need to grab the keyboard to get STOP key events. */

#ifdef OW_I18N
	if (window_set_xgrabkeyboard(dnd->parent, dpy, xv_xid(info), FALSE,
					GrabModeAsync, GrabModeAsync, lasttime) != GrabSuccess) {
		status = DND_ERROR;
		goto BreakOut;
	}
#else
	if (XGrabKeyboard(dpy, xv_xid(info), FALSE, GrabModeAsync,
					GrabModeAsync, lasttime) != GrabSuccess) {
		status = DND_ERROR;
		goto BreakOut;
	}
#endif

	if (XGrabPointer(dpy, xv_xid(info), FALSE,
					(int)(ButtonMotionMask | ButtonReleaseMask),
					GrabModeAsync, GrabModeAsync, None, DndGetCursor(dnd),
					lasttime) != GrabSuccess) {
		status = DND_ERROR;

#ifdef OW_I18N
		window_set_xungrabkeyboard(dnd->parent, dpy, lasttime);
#else
		XUngrabKeyboard(dpy, lasttime);
#endif

		goto BreakOut;
	}

	/* Contact DSDM. */
	if (XGetSelectionOwner(dpy, dnd->atom[DSDM]) != None) {
		if (DndContactDSDM(dnd))
			dsdm_present = True;	/* XXX: sort list */
	}

	ev = &xevent;

	/* If the app is deaf (BUSY), set this window as un-deaf so that
	 * xevent_to_event() will pass us the keyboard and mouse events.
	 */
	win_info = WIN_PRIVATE(dnd->parent);
	if (WIN_IS_DEAF(win_info)) {
		WIN_SET_DEAF(win_info, False);
		win_was_deaf = True;
	}
	do {
		xview_x_input_readevent(dpy, &event, dnd->parent, True, True,
				(int)(ButtonMotionMask|ButtonReleaseMask|KeyReleaseMask), ev);
		switch (event_action(&event)) {
			case ACTION_SELECT:
			case ACTION_ADJUST:
			case ACTION_MENU:
				lasttime = ev->xbutton.time;
				server_set_timestamp(srv, NULL, lasttime);
				if (event_is_up(&event)) {
					unsigned int state = ev->xbutton.state;

					/* Remove the button that was just released from the
					 * state field, then check and see if any other buttons
					 * are press.  If none are, then the drop happened.
					 */
					state &= ~(1 << (ev->xbutton.button + 7));
					if (!(state & (Button1Mask | Button2Mask | Button3Mask |
											Button4Mask | Button5Mask)))
					{
#ifdef INVESTIGATION_OF_STRANGE_LOG_DRAG
	 					/* Compare in txt_xsel.c Reference (jg45tiyfgmbdrgfh) */
						int cnt = 0;
						XEvent olddrag;
						dndctxt_t dndcontext;

						button_released = True;
						/* are there still drag events with SELECT pressed
						 * in the queue?
						 */
						dndcontext.window = xv_xid(info);

						/* when this was active, DnD from PANEL_TEXTs
						 * made problems...
						 */
						while (XCheckIfEvent(dpy, &olddrag, check_dnd_drag,
													(char *)&dndcontext))
							++cnt;

						SERVERTRACE((363,
								"===== removed %d SELECT-DRAG\n", cnt));
#else /* INVESTIGATION_OF_STRANGE_LOG_DRAG */

						button_released = True;

#endif /* INVESTIGATION_OF_STRANGE_LOG_DRAG */
					}
				}
				break;
			case LOC_DRAG:
				lasttime = ev->xmotion.time;
				server_set_timestamp(srv, NULL, lasttime);
				if (dsdm_present) {
					DndFindSite(dnd, (XButtonEvent *) ev);
				}
				break;
			case ACTION_STOP:
				lasttime = ev->xkey.time;
				server_set_timestamp(srv, NULL, lasttime);
				/* Send LeaveNotify if necessary */
				if (dsdm_present)
					(void)DndSendPreviewEvent(dnd, DND_NO_SITE, ev);
				stop = True;

#ifdef NO_XDND
#else /* NO_XDND */
				/* Send XdndLeave if necessary */
				if (dnd->last_top_win != None && dnd->last_top_win_is_aware) {
					send_preview_leave(ev->xany.display, dnd, lasttime);
				}
#endif /* NO_XDND */

				break;

			case WIN_CLIENT_MESSAGE:
#ifdef NO_XDND
#else /* NO_XDND */
				if (ev->xclient.message_type == dnd->atom[ACK]) {
					/* das ist eine 'Preview-Answer' ! */
					/* im Augenblick sehen wir mal in data.l[0] als 'rejected'
					 * siehe auch 
					 */
					UpdateGrabCursor(dnd, EnterNotify,
										(int)ev->xclient.data.l[0], lasttime);
				}
				else 
				/* derzeit fangen wir damit nicht viel an */
				if (ev->xclient.message_type == dnd->atom[XdndStatus]) {

					SERVERTRACE((TLXDND, "\nXdndStatus:\n"));
					SERVERTRACE((TLXDND, "data[0] = %lx       window\n",
								ev->xclient.data.l[0]));
					SERVERTRACE((TLXDND+10, "data[1] = %lx       %saccepted, want %smore\n",
								ev->xclient.data.l[1],
								(ev->xclient.data.l[1] & 1) ? "" : "not ",
								(ev->xclient.data.l[1] & 2) ? "" : "no "));
					SERVERTRACE((TLXDND+10, "data[2,3] = rect[%ld,%ld,%ld,%ld]\n",
								(ev->xclient.data.l[2] >> 16) & 0xffff,
								ev->xclient.data.l[2] & 0xffff,
								(ev->xclient.data.l[3] >> 16) & 0xffff,
								ev->xclient.data.l[3] & 0xffff));
					SERVERTRACE((TLXDND+10, "data[4] = %ld       action\n",
								ev->xclient.data.l[4]));

					/* is that still interesting ?
					 * The mouse might have been moved to a different
					 * target window
					 */
					if (ev->xclient.data.l[0] == dnd->last_top_win) {
						int accepted = (ev->xclient.data.l[1] & 1);

						dnd->xdnd_status_from_last_top_win_seen = TRUE;
						if (dnd->xdnd_last_status_was_accept != accepted) {
							dnd->xdnd_last_status_was_accept = accepted;
							UpdateGrabCursor(dnd,
										accepted ? EnterNotify : LeaveNotify,
										! accepted, lasttime);
						}
					}
				}
				break;
#endif /* NO_XDND */

			default:
				break;
		}
	} while (!button_released && !stop);

	if (win_was_deaf)
		WIN_SET_DEAF(win_info, True);

	server_set_timestamp(srv, NULL, lasttime);
	XUngrabPointer(dpy, lasttime);

#ifdef OW_I18N
	window_set_xungrabkeyboard(dnd->parent, dpy, lasttime);
#else
	XUngrabKeyboard(dpy, lasttime);
#endif

	if (stop) {
		status = DND_ABORTED;
		goto BreakOut;
	}

	if ((status = Verification(&ev->xbutton, dnd)) == DND_SUCCEEDED) {
		/* If drop site is within same process, optimize! */

		/* MULTI_DISPLAY: the notion "same process" is a little dangerous in
		 * multi-display-applications
		 */
		status = SendTrigger(dnd, info, &ev->xbutton,
								(int)win_data(dpy, dnd->dropSite.window));
	}
#ifdef NO_XDND
#else /* NO_XDND */
	else if (status != DND_ROOT) {
		Window toplev = find_xdnd_top_level(dnd, ev);
		Window drop_window = 0;
		int is_xterm = FALSE;

		status = DND_ILLEGAL_TARGET;

		if (toplev) {
			if (toplev == dnd->last_top_win) {
				/* no 'last minute change', we know everything */
				/* however, the whole 'status seen' and 'accepted' thing
				 * seems to be a bit funny: when I drag a file to 
				 * amarok and drag around a little, amarok send 'not accept'
				 * messages. BUT if I drop very quicky (before) any status
				 * messages came in, the drop message is sent and then
				 * amarok requests the files....
				 *
				 * Conclusion: send the drop in any case (having seen
				 * the aware property)
				 */
				if (dnd->last_top_win_is_aware) {
					drop_window = toplev;
				}
			}
			else {
				int act_format = 0;
				Atom act_typeatom;
				unsigned long items, rest;
				unsigned char *bp;

				/* oh - a last minute change - we know nothing */

				if (XGetWindowProperty(ev->xany.display, toplev,
					dnd->atom[XdndAware], 0L, 1000L, FALSE, XA_ATOM, &act_typeatom,
					&act_format, &items, &rest, &bp) == Success &&
					act_format == 32)
				{
					XFree(bp);

					/* no XdndStatus seen, ??? we send the drop anyway */
					drop_window = toplev;
				}
			}

			if (! drop_window) {
				/* now, we want to be able to drop on xterms:
				 * all you need to do is giving xterm a translation of the form
				 *
				 * XTerm*vt100.translations: #override\n\
				 *     ...
				 *     ...
				 *    <ClientMessage>XdndDrop : insert-selection(XdndSelection)
				 *
				 *  ???????????????????????????
				 *
				 * Actually, this was a nice idea, however, the insert-selection
				 * action of xterm assumes (and checks...) that the triggering
				 * event is a button event...
				 *
				 * So, the next idea was to send it a Btn2Up event - but then
				 * we would have to become PRIMARY owner....
				 *
				 * Now, we send it a Btn5Up event: to make this work on the
				 * xterm side, we have to add a translation
				 *  ....
				 *    <Btn5Up>: insert-selection(XdndSelection)
				 *
				 * and set
				 *
				 *    XTerm*vt100.allowSendEvents: True
				 *
				 *
				 * I know, I know, Button 5 is used by mouse wheels......
				 * BUT - we can use a lot of modifiers, for example
				 * shift ctrl button1 button2 button3
				 * Then, the required Translation reads
				 *
				 *  Button1 Button2 Button3 Shift Ctrl<Btn5Up>: insert-selection(XdndSelection)
				 */
				Display *dpy = ev->xany.display;
				XClassHint classhint;

				if (XGetClassHint(dpy, toplev, &classhint)) {
					if (0 == strcmp(classhint.res_class, "XTerm")) {
						/* now, in a classical xterm, the VT100 widget is the
						 * (only) child of the toplevel window
						 */
						Window rt, par, *ch = 0;
						unsigned int numch;

						if (XQueryTree(dpy, toplev, &rt, &par, &ch, &numch)) {
							if (numch == 1) {
								drop_window = ch[0];
								is_xterm = TRUE;
							}
							if (ch) XFree(ch);
						}
					}
					XFree(classhint.res_class);
					XFree(classhint.res_name);
				}
			}

			if (drop_window) {
				if (is_xterm) {
					XEvent xb;

					xb.xbutton.type = ButtonRelease;
					xb.xbutton.display = ev->xany.display;

					xb.xbutton.window = drop_window;
					xb.xbutton.root = ev->xbutton.root;
					xb.xbutton.subwindow = ev->xbutton.subwindow;
					xb.xbutton.time = ev->xbutton.time + 1;
					xb.xbutton.x = ev->xbutton.x;
					xb.xbutton.y = ev->xbutton.y;
					xb.xbutton.x_root = ev->xbutton.x_root;
					xb.xbutton.y_root = ev->xbutton.y_root;
					xb.xbutton.state = (ShiftMask | ControlMask |
								Button1Mask | Button2Mask | Button3Mask);
					xb.xbutton.button = 5;
					xb.xbutton.same_screen = TRUE;

					SERVERTRACE((TLXDND, "sending ButtonRelease to XTerm\n"));
					DndSendEvent(ev->xany.display, &xb, "ButtonRelease");
				}
				else {
					XEvent cM;

					/* send_preview enter */
					cM.xclient.type = ClientMessage;
					cM.xclient.display = ev->xany.display;
					cM.xclient.format = 32;
					cM.xclient.message_type = dnd->atom[XdndDrop];
					cM.xclient.window = drop_window;
					cM.xclient.data.l[0] = (Window)xv_get(dnd->parent, XV_XID);
					cM.xclient.data.l[1] = 0;
					cM.xclient.data.l[2] = ev->xbutton.time;
					cM.xclient.data.l[3] = 0;
					cM.xclient.data.l[4] = 0;

					SERVERTRACE((TLXDND, "sending XdndDrop to %lx\n", drop_window));
					DndSendEvent(ev->xany.display, &cM, "XdndDrop");
				}
				status = DND_SUCCEEDED;
			}
		}
	}
#endif /* NO_XDND */

  BreakOut:
	/* If the dnd pkg created a transient selection and the dnd operation
	 * failed or was aborted, free up the transient selection.
	 */
	if (status != DND_SUCCEEDED && dnd->transientSel) {
		xv_set(dnd_public, SEL_OWN, False, NULL);
		if (dnd->xdnd_owner) xv_set(dnd->xdnd_owner, SEL_OWN, FALSE, NULL);
	}

	if (dnd->siteRects) {
		xv_free(dnd->siteRects);
		dnd->siteRects = NULL;
	}

	return (status);
}

extern int debug_DND;

static int SendTrigger(Dnd_info *dnd, Xv_Drawable_info *info,
						XButtonEvent *buttonEvent, int local)
{
	if (local) {
		int value;
		Xv_Server server = XV_SERVER_FROM_WINDOW(dnd->parent);
		Attr_attribute dndKey = xv_get(server, SERVER_DND_ACK_KEY);

		xv_set(server, XV_KEY_DATA, dndKey, False, NULL);

		if ((value=SendDndEvent(dnd,Dnd_Trigger_Local,0L,buttonEvent))
				== DND_SUCCEEDED) {
			if ((int)xv_get(server, XV_KEY_DATA, dndKey))
				value = DND_SUCCEEDED;
			else
				value = DND_TIMEOUT;
		}
		return (value);
	}
	else {
		if (SendDndEvent(dnd, Dnd_Trigger_Remote, 0L, buttonEvent)
				== DND_SUCCEEDED) {
			int value;

			/* DND_HACK begin */
			/*
			   return(WaitForAck(dnd, info));
			 */
			if (debug_DND > 0) fprintf(stderr, "%ld before WairForAck\n", time(0));
			if ((value = WaitForAck(dnd, info)) == DND_TIMEOUT)
				if (dnd->is_old)
					return (SendOldDndEvent(dnd, buttonEvent));
			return (value);
			/* DND_HACK end */
		}
		else
			return (DND_ERROR);
	}
}

/* DND_HACK begin */
static int SendOldDndEvent(Dnd_info *dnd, XButtonEvent *buttonEvent)
{
    Selection_requestor	 req;
    unsigned long	 length;
    int			 format;
    char		*data;
    int			 i = 0;
    long		 msg[5];

    req = xv_create(dnd->parent, SELECTION_REQUESTOR,
			      SEL_RANK, (Atom)xv_get(DND_PUBLIC(dnd), SEL_RANK),
			      SEL_OWN,	True,
			      SEL_TYPE_NAME,	"FILE_NAME",
			      NULL);

    do {
        data = (char *)xv_get(req, SEL_DATA, &length, &format);
        if (length != SEL_ERROR)
	    break;
        else {
	    i++;
	    if (i == 1)
	        xv_set(req, SEL_TYPE, XA_STRING, NULL);
	    else if (i == 2)
	        xv_set(req, SEL_TYPE_NAME, "TEXT", NULL);
	    else
		return(DND_ERROR);
	}
	/* SUPPRESS 558 */
    } while(True);

    msg[0] = XV_POINTER_WINDOW;
    msg[1] = buttonEvent->x;
    msg[2] = buttonEvent->y;
    msg[3] = (long)xv_get(dnd->parent, XV_XID);
    msg[4] = (long)xv_get(XV_SERVER_FROM_WINDOW(dnd->parent),
				SERVER_ATOM, "DRAG_DROP");

    XChangeProperty(XV_DISPLAY_FROM_WINDOW(dnd->parent), (Window)msg[3],
                    (Atom)msg[4], XA_STRING, 8, PropModeReplace,
                    (unsigned char *)data, (int)strlen(data)+1);

    if (i == 0)
        xv_send_message(dnd->parent, dnd->dropSite.window, "XV_DO_DRAG_LOAD",
			32, (Xv_opaque *)msg, (int)sizeof(msg));
    else if (dnd->type == DND_COPY)
        xv_send_message(dnd->parent, dnd->dropSite.window, "XV_DO_DRAG_COPY",
			32, (Xv_opaque *)msg, (int)sizeof(msg));
    else
        xv_send_message(dnd->parent, dnd->dropSite.window, "XV_DO_DRAG_MOVE",
			32, (Xv_opaque *)msg, (int)sizeof(msg));
    return(DND_SUCCEEDED);
}
/* DND_HACK end */

Xv_private int DndSendPreviewEvent(Dnd_info *dnd, int site, XEvent *e)
{
	int i = dnd->eventSiteIndex;

	if (e->type != ButtonPress 
		&& e->type != ButtonRelease
		&& e->type != MotionNotify
		&& e->type != KeyRelease  /* wegen ACTION_STOP */
		)
	{
		fprintf(stderr, "%s-%d: unexpected event type %d in DndSendPreviewEvent\n",
				__FILE__, __LINE__, e->type);
	}

#ifdef NO_XDND
#else /* NO_XDND */
	/* wenn hier (site == DND_NO_SITE && e->type == MotionNotify),
	 * dann kommen wir aus DndFindSite und haben nichts gefunden -
	 * jetzt kuemmern wir uns mal um XDND:
	 */
	if (site == DND_NO_SITE && e->type == MotionNotify) {
		XEvent cM;
		Window toplev = find_xdnd_top_level(dnd, e);
		int act_format = 0;
		Atom act_typeatom;
		unsigned long items, rest;
		unsigned char *bp;

		if (toplev) {
			if (toplev == dnd->last_top_win) {
				/* brauche das Prop nicht neu zu lesen... */
				if (dnd->last_top_win_is_aware) {
					/* send_preview position */
					cM.xclient.type = ClientMessage;
					cM.xclient.display = e->xmotion.display;
					cM.xclient.format = 32;
					cM.xclient.message_type = dnd->atom[XdndPosition];
					cM.xclient.window = toplev;
					cM.xclient.data.l[0] = (Window)xv_get(dnd->parent, XV_XID);
					cM.xclient.data.l[1] = 0;
					cM.xclient.data.l[2] = (e->xmotion.x_root << 16) | e->xmotion.y_root;
					cM.xclient.data.l[3] = e->xmotion.time;
					cM.xclient.data.l[4] = ((dnd->type == DND_MOVE) ?
												dnd->atom[XdndActionMove] :
												dnd->atom[XdndActionCopy]);

					SERVERTRACE((TLXDND, "sending XdndPosition(time %ld) to %lx\n",
												e->xmotion.time, toplev));
					DndSendEvent(e->xmotion.display, &cM, "XdndPosition");
				}
			}
			else {
				/* a different top level window */
				/* so, this is enter and/or leave */
				if (dnd->last_top_win_is_aware) {
					send_preview_leave(e->xmotion.display, dnd,e->xmotion.time);
				}

				dnd->last_top_win = toplev;
				dnd->last_top_win_is_aware = FALSE;
				dnd->xdnd_status_from_last_top_win_seen = FALSE;
				if (XGetWindowProperty(e->xmotion.display, toplev,
						dnd->atom[INTEREST],
						0L, 10L, FALSE, dnd->atom[INTEREST], &act_typeatom,
						&act_format, &items, &rest, &bp) == Success &&
					act_format == 32)
				{
					/* this is one of us - we don't need any Xdnd things */
					XFree(bp);
				}
				else if (XGetWindowProperty(e->xmotion.display, toplev, dnd->atom[XdndAware],
						0L, 10L, FALSE, XA_ATOM, &act_typeatom,
						&act_format, &items, &rest, &bp) == Success &&
					act_format == 32)
				{
					Atom *ip = (Atom *)bp;
					int targets_version = (int)(*ip);
					int i;
					Xv_server srv = XV_SERVER_FROM_WINDOW(dnd->parent);
					Atom uri = (Atom)xv_get(srv, SERVER_ATOM, "text/uri-list");
					Atom fn = (Atom)xv_get(srv, SERVER_ATOM, "FILE_NAME");
					Atom txt = (Atom)xv_get(srv, SERVER_ATOM, "text/plain");

					XFree(bp);
					dnd->last_top_win_is_aware = TRUE;
					dnd->xdnd_used_version = MIN(targets_version, XDND_MY_VERSION);

					/* send_preview enter */
					cM.xclient.type = ClientMessage;
					cM.xclient.display = e->xmotion.display;
					cM.xclient.format = 32;
					cM.xclient.message_type = dnd->atom[XdndEnter];
					cM.xclient.window = toplev;
					cM.xclient.data.l[0] = (Window)xv_get(dnd->parent, XV_XID);
					cM.xclient.data.l[1] = (dnd->xdnd_used_version << 24);

					/* at this level, we know of a few 'main drag types':
					 *  + a file drag
					 *  + a text drag
					 */
					for (i = 0; i < dnd->numtargets; i++) {
						if (dnd->targetlist[i] == uri || dnd->targetlist[i] == fn) {
							cM.xclient.data.l[2] = uri;
							/* let's simulate dolphin: */
							cM.xclient.data.l[3] = xv_get(srv, SERVER_ATOM,
														"text/x-moz-url");
							cM.xclient.data.l[4] = txt;
							/* thunar has cM.xclient.data.l[3] and ..[4] = None */
							break;
						}
					}
					if (i >= dnd->numtargets) {
						/* no file drag */
						for (i = 0; i < dnd->numtargets; i++) {
							if (dnd->targetlist[i] == txt
								|| dnd->targetlist[i] == XA_STRING)
							{
								cM.xclient.data.l[2] = txt;
								cM.xclient.data.l[3] = XA_STRING;
								cM.xclient.data.l[4] = None;
								break;
							}
						}
					}
					if (i >= dnd->numtargets) {
						/* no file drag and no string drag */
						cM.xclient.data.l[2] = dnd->targetlist[0];
						cM.xclient.data.l[3] = dnd->targetlist[1];
						cM.xclient.data.l[4] = dnd->targetlist[2];
					}

					/* usually we have more than 3 TARGETS */
					if (dnd->numtargets >= 3) {
						/* we should provide a XdndTypeList */
						cM.xclient.data.l[1] |= 1;
						XChangeProperty(e->xmotion.display,
								(Window)cM.xclient.data.l[0],
								dnd->atom[XdndTypeList], XA_ATOM, 32,
								PropModeReplace, (unsigned char *)dnd->targetlist,
								dnd->numtargets);
					}

					SERVERTRACE((TLXDND, "sending XdndEnter to %lx (%ld, %ld, %ld)\n",
							toplev, cM.xclient.data.l[2],
							cM.xclient.data.l[3], cM.xclient.data.l[4]));
					/* we assume 'accept' first */
					dnd->xdnd_last_status_was_accept = 1;
					DndSendEvent(e->xmotion.display, &cM, "XdndEnter");
					UpdateGrabCursor(dnd, EnterNotify, FALSE, e->xmotion.time);

					/* send_preview position */

					cM.xclient.type = ClientMessage;
					cM.xclient.display = e->xmotion.display;
					cM.xclient.format = 32;
					cM.xclient.message_type = dnd->atom[XdndPosition];
					cM.xclient.window = toplev;
					cM.xclient.data.l[0] = (Window)xv_get(dnd->parent, XV_XID);
					cM.xclient.data.l[1] = 0;
					cM.xclient.data.l[2] = (e->xmotion.x_root << 16) | e->xmotion.y_root;
					cM.xclient.data.l[3] = e->xmotion.time;
					cM.xclient.data.l[4] = ((dnd->type == DND_MOVE) ?
												dnd->atom[XdndActionMove] :
												dnd->atom[XdndActionCopy]);

					SERVERTRACE((TLXDND, "sending XdndPosition(time %ld) to %lx\n",
													e->xmotion.time, toplev));
					DndSendEvent(e->xmotion.display, &cM, "XdndPosition");
				}
			}
		}
	}

#endif /* NO_XDND */

	/* No Site yet */
	if (i == DND_NO_SITE) {
		dnd->eventSiteIndex = site;
		/* Moved into a new site */
		if (site != DND_NO_SITE) {
			if (dnd->siteRects[site].flags & DND_ENTERLEAVE) {
				if (SendDndEvent(dnd, Dnd_Preview, (long)EnterNotify, &e->xbutton)
						!= DND_SUCCEEDED)
					return (DND_ERROR);
			}
			UpdateGrabCursor(dnd, EnterNotify, FALSE, e->xmotion.time);
		}
		/* Moved out of the event site */
	}
	else if (i != site) {
		/* Tell the old site goodbye */
		if (dnd->siteRects[i].flags & DND_ENTERLEAVE) {
			if (SendDndEvent(dnd, Dnd_Preview, (long)LeaveNotify, &e->xbutton)
					!= DND_SUCCEEDED)
				return (DND_ERROR);
		}
		UpdateGrabCursor(dnd, LeaveNotify, FALSE, e->xmotion.time);
		dnd->eventSiteIndex = site;
		/* Say hi to the new site */
		if (site != DND_NO_SITE) {
			if (dnd->siteRects[site].flags & DND_ENTERLEAVE) {
				if (SendDndEvent(dnd, Dnd_Preview, (long)EnterNotify, &e->xbutton)
						!= DND_SUCCEEDED)
					return (DND_ERROR);
			}
			UpdateGrabCursor(dnd, EnterNotify, FALSE, e->xmotion.time);
		}
		/* Moving through the current event site */
	}
	else if (i == site) {
		if (dnd->siteRects[i].flags & DND_MOTION) {
			if (SendDndEvent(dnd, Dnd_Preview, (long)MotionNotify, &e->xbutton)
					!= DND_SUCCEEDED)
				return (DND_ERROR);
		}
	}
	return (DND_SUCCEEDED);
}

static void UpdateGrabCursor( Dnd_info *dnd, int type, int rejected, Time t)
{
	Xv_Drawable_info *info;
	Cursor cursor;

	DRAWABLE_INFO_MACRO(dnd->parent, info);

	if (rejected) {
		if (dnd->rejectCursor)
			cursor = xv_get(dnd->rejectCursor, XV_XID);
		else if (dnd->rejectXCursor)
			cursor = dnd->rejectXCursor;
		else {
			/* we have no reject cursor - but are still over a drop site */
			if (dnd->affCursor)
				cursor = xv_get(dnd->affCursor, XV_XID);
			else if (dnd->affXCursor)
				cursor = dnd->affXCursor;
			else
				return;
		}
	}
	else {
		/* If no cursor, then jump out */
		if (dnd->affCursor)
			cursor = xv_get(dnd->affCursor, XV_XID);
		else if (dnd->affXCursor)
			cursor = dnd->affXCursor;
		else
			return;
	}

	if (type == EnterNotify)
		XChangeActivePointerGrab(xv_display(info),
				(int)(ButtonMotionMask | ButtonReleaseMask),
				cursor, t);
	else
		XChangeActivePointerGrab(xv_display(info),
				(int)(ButtonMotionMask | ButtonReleaseMask),
				DndGetCursor(dnd), t);
}

static int WaitForAck(Dnd_info *dnd, Xv_Drawable_info *info)
{
	Display *dpy = xv_display(info);
	XEvent event;
	XEvent selNotifyEvent;
	Atom property;
	int status = DND_SUCCEEDED;

	/* Wait for the dest to respond with an
	 * _SUN_DRAGDROP_ACK.
	 * XXX: Should check timestamp.  Sec 2.2 ICCCM
	 */
	if ((status = DndWaitForEvent(dpy, xv_xid(info), SelectionRequest,
						dnd->atom[ACK], &dnd->timeout, &event, DndMatchEvent))
			!= DND_SUCCEEDED)
		goto BailOut;

	if (debug_DND > 0)
		fprintf(stderr, "%ld in WairForAck, after DndWaitForEvent(ACK)\n",
				time(0));
	/* Select for PropertyNotify events on requestor's window */
	XSelectInput(dpy, event.xselectionrequest.requestor, PropertyChangeMask);

	/* If the property field is None, the requestor is an obsolete
	 * client.   Sec. 2.2 ICCCM
	 */
	if (event.xselectionrequest.property == None)
		property = event.xselectionrequest.target;
	else
		property = event.xselectionrequest.property;

	/* If the destination ACK'ed us, send it back a NULL reply. */
	/* XXX: Should be prepared to handle bad alloc errors from the
	 * server.  Sec 2.2 ICCCM
	 */
	XChangeProperty(dpy, event.xselectionrequest.requestor,
			property, event.xselectionrequest.target, 32,
			PropModeReplace, (unsigned char *)NULL, 0);

	selNotifyEvent.xselection.type = SelectionNotify;
	selNotifyEvent.xselection.display = dpy;
	selNotifyEvent.xselection.requestor = event.xselectionrequest.requestor;
	selNotifyEvent.xselection.selection = event.xselectionrequest.selection;
	selNotifyEvent.xselection.target = event.xselectionrequest.target;
	selNotifyEvent.xselection.property = property;
	selNotifyEvent.xselection.time = event.xselectionrequest.time;

	/* SUPPRESS 68 */
	if (DndSendEvent(dpy, &selNotifyEvent, "SelNtfy") != DND_SUCCEEDED) {
		status = DND_ERROR;
		goto BailOut;
	}

	if (debug_DND > 0)
		fprintf(stderr, "%ld in WairForAck, before DndWaitForEvent(prop)\n",
				time(0));
	/* the second param is declared as Window, but DndMatchProp
	 * compares it with the atom of a PropertyNotify event....
	 */
	status = DndWaitForEvent(dpy, property, PropertyNotify, None, &dnd->timeout,
			&event, DndMatchProp);

	if (debug_DND > 0)
		fprintf(stderr, "%ld in WairForAck, after DndWaitForEvent(prop)\n",
				time(0));
	/* XXX: This will kill any events someone else has selected for. */
	XSelectInput(dpy, event.xproperty.window, NoEventMask);
	XFlush(dpy);

  BailOut:
	return (status);
}

Pkg_private int DndWaitForEvent(Display *dpy, Window window, int eventType,
 Atom target, struct timeval *timeout, XEvent *event, DndEventWaiterFunc MatchFunc)
{
	fd_set xFd;
	int nFd;
	DnDWaitEvent wE;
	struct timeval myTimeout, *sto;

	if (timeout) {
		myTimeout = *timeout;
		sto = &myTimeout;
	}
	else {
		sto = 0;
	}
	wE.window = window;
	wE.eventType = eventType;
	wE.target = target;

	/* Couldm't it be that the event we are waiting for
	 * is already in the event queue ?
	 */
	if (XCheckIfEvent(dpy, event, MatchFunc, (XPointer)&wE))
		return DND_SUCCEEDED;

	FD_ZERO(&xFd);

	XFlush(dpy);
	do {
		FD_SET(XConnectionNumber(dpy), &xFd);
		if (!(nFd = select(XConnectionNumber(dpy) + 1, &xFd, NULL, NULL, sto)))
			return DND_TIMEOUT;
		else if (nFd == -1) {
			if (errno != EINTR)
				return DND_ERROR;
		}
		else {
			if (XCheckIfEvent(dpy, event, MatchFunc, (XPointer)&wE))
				return DND_SUCCEEDED;
		}
		/* SUPPRESS 558 */
	} while (True);

 /*NOTREACHED*/
}

/*ARGSUSED*/
Pkg_private Bool DndMatchEvent(Display *dpy, XEvent *event, XPointer cldt)
{
	DnDWaitEvent *wE = (DnDWaitEvent *) cldt;
	Atom target = 0;

	if (event->type == SelectionNotify)
		target = event->xselection.target;
	else if (event->type == SelectionRequest)
		target = event->xselectionrequest.target;

	if ((event->type == wE->eventType) &&
			(event->xany.window == wE->window) && (target == wE->target))
		return (True);
	else
		return (False);
}

static int SendDndEvent(Dnd_info *dnd, DndMsgType type, long subtype,
							XButtonEvent *ev)
{
	XEvent cM;
	Event event;
	const char *debugname = 0;

	cM.xclient.type = ClientMessage;
	cM.xclient.display = ev->display;
	cM.xclient.format = 32;
	event_init(&event);

	switch (type) {
		case Dnd_Trigger_Remote:
			{
				long flags = DND_ACK_FLAG;

				if (dnd->type == DND_MOVE)
					flags |= DND_MOVE_FLAG;
				if (dnd->transientSel)
					flags |= DND_TRANSIENT_FLAG;
				if (dnd->dropSite.flags & DND_FORWARDED_FLAG)
					flags |= DND_FORWARDED_FLAG;

				debugname = "_SUN_DRAGDROP_TRIGGER";
				cM.xclient.message_type = dnd->atom[TRIGGER];
				cM.xclient.window = dnd->dropSite.window;
				cM.xclient.data.l[0] = (Atom) xv_get(DND_PUBLIC(dnd), SEL_RANK);
				cM.xclient.data.l[1] = ev->time;
				cM.xclient.data.l[2] = (ev->x_root << 16) | ev->y_root;
				cM.xclient.data.l[3] = dnd->dropSite.site_id;	/* Site ID */
				cM.xclient.data.l[4] = flags;
			}
			break;
		case Dnd_Trigger_Local:
			{
				int x, y;
				Window child;
				Xv_Window dropObject = win_data(ev->display,
						dnd->dropSite.window);
				long flags = DND_ACK_FLAG;

				/* Indicate that this is a local event */
				long local_flags = DND_LOCAL;

				if (dnd->type == DND_MOVE)
					flags |= DND_MOVE_FLAG;
				if (dnd->transientSel)
					flags |= DND_TRANSIENT_FLAG;
				if (dnd->dropSite.flags & DND_FORWARDED_FLAG) {
					flags |= DND_FORWARDED_FLAG;
					local_flags |= DND_FORWARDED;
				}

				debugname = "_SUN_DRAGDROP_TRIGGER";
				cM.xclient.message_type = dnd->atom[TRIGGER];
				cM.xclient.serial = 0L;	/* XXX: This is incorrect. */
				cM.xclient.send_event = True;
				cM.xclient.window = dnd->dropSite.window;
				cM.xclient.data.l[0] = (Atom) xv_get(DND_PUBLIC(dnd), SEL_RANK);
				cM.xclient.data.l[1] = ev->time;
				cM.xclient.data.l[2] = (ev->x_root << 16) | ev->y_root;
				cM.xclient.data.l[3] = dnd->dropSite.site_id;	/* Site ID */
				cM.xclient.data.l[4] = flags;

				event_init(&event);
				event_set_window(&event, dropObject);
				event_set_action(&event,
						(dnd->type == DND_MOVE ? ACTION_DRAG_MOVE :
								ACTION_DRAG_COPY));

				if (!XTranslateCoordinates(ev->display, ev->window,
								dnd->dropSite.window, ev->x,
								ev->y, &x, &y, &child)) {
					/* Different Screens */
					return (DND_ERROR);
				}

				/* Set the time of the trigger event */
				event.ie_time.tv_sec = ((unsigned long)ev->time) / 1000;
				event.ie_time.tv_usec =
						(((unsigned long)ev->time) % 1000) * 1000;

				event_set_x(&event, x);
				event_set_y(&event, y);
				/* Indicate that this is a local event */
				event_set_flags(&event, local_flags);
				event_set_xevent(&event, &cM);

				if (win_post_event(dropObject, &event,
								NOTIFY_IMMEDIATE) !=
						NOTIFY_OK) return (DND_ERROR);

				return DND_SUCCEEDED;
			}
			/*NOTREACHED*/
			break;
		case Dnd_Preview:
			{
				Xv_Window eventObject = win_data(ev->display,
						(Window)dnd->siteRects[dnd->eventSiteIndex].window);
				/* The original XView Dnd protocol had no information about
				 * the drag source in the preview client messages.
				 * Therefore, there was no way to establish a communication
				 * between the drag source and the preview receiver, 
				 * the main purpose of such a communication would be a
				 * 'rejection'. The field data.l[4] was taken by the
				 * DND_FORWARDED info, which takes only 1 bit.
				 *
				 * Now, I want to include the 'forwarded' situation into
				 * data.l[2] which contains the mouse position in the
				 * following way:
				 * 1111111111111111 1 111111111111111
				 * x_root (16 bits) ^ y_root (15 bits)
				 *                  |
				 *                  Forwarded
				 *
				 * When this has been thoroughly tested, the data.l[4] field
				 * can be used for the window ID of the drag source - 
				 * and we can (in analogy to Xdnd) provide a property with
				 * that TARGETS or similar....
				 * Reference (erfihlwebfrygjbv)
				 *
				 * However, if I send such a preview event to a client that
				 * is running against the 'standard' XView library, it will
				 * not understand.
				 * Our new XView clients set the bit
				 * DND_EXPECT_NEW_PREVIEW_EVENT in their drop sites - so we
				 * can recognize them.
				 */

				debugname = "_SUN_DRAGDROP_PREVIEW";
				cM.xclient.message_type = dnd->atom[PREVIEW];
				cM.xclient.window = dnd->siteRects[dnd->eventSiteIndex].window;
				cM.xclient.data.l[0] = subtype;
				cM.xclient.data.l[1] = ev->time;
				cM.xclient.data.l[3] = dnd->siteRects[dnd->eventSiteIndex].site_id;

				if (dnd->siteRects[dnd->eventSiteIndex].flags
											& DND_EXPECT_NEW_PREVIEW_EVENT)
				{
					unsigned event_forwarded = 0;
					/* there is a new application that know about
					 * the new preview event
					 */
					if (dnd->siteRects[dnd->eventSiteIndex].flags
											& DND_FORWARDED_FLAG)
					{
						event_forwarded = 0x8000;
					}

					cM.xclient.data.l[2] = (ev->x_root << 16)
											| event_forwarded
											| ev->y_root;
					/* Reference (erfihlwebfrygjbv) */
					cM.xclient.data.l[4] = xv_get(dnd->parent, XV_XID);
				}
				else {
					/* there is an old application that expects the old
					 * preview events.
					 */
					if (dnd->siteRects[dnd->eventSiteIndex].flags
												& DND_FORWARDED_FLAG)
						cM.xclient.data.l[4] = DND_FORWARDED_FLAG;
					else
						cM.xclient.data.l[4] = 0;
				}

				if (eventObject) {
					int x, y;
					Window child;
					long local_flags = DND_LOCAL;

					event_init(&event);
					event_set_window(&event, eventObject);
					switch (subtype) {
						case EnterNotify:
							event_set_id(&event, LOC_WINENTER);
							break;
						case LeaveNotify:
							event_set_id(&event, LOC_WINEXIT);
							break;
						case MotionNotify:
							event_set_id(&event, LOC_DRAG);
							break;
					}
					event_set_action(&event, ACTION_DRAG_PREVIEW);

					/* XXX: This can be improved.  Roundtrip for local preview
					 * events is not neccessary.
					 */
					if (!XTranslateCoordinates(ev->display, ev->root,
							(Window)dnd->siteRects[dnd->eventSiteIndex].window,
							ev->x_root, ev->y_root, &x, &y, &child)) {
						/* XXX: Different Screens */
						return (DND_ERROR);
					}

					event_set_x(&event, x);
					event_set_y(&event, y);

					event.ie_time.tv_sec = ((unsigned long)ev->time) / 1000;
					event.ie_time.tv_usec =
							(((unsigned long)ev->time) % 1000) * 1000;

					if (dnd->siteRects[dnd->eventSiteIndex].flags &
							DND_FORWARDED_FLAG) local_flags |= DND_FORWARDED;

					event_set_flags(&event, local_flags);
					event_set_xevent(&event, &cM);

					if (win_post_event(eventObject, &event, NOTIFY_IMMEDIATE)
							!= NOTIFY_OK)
						return (DND_ERROR);

					return DND_SUCCEEDED;
				}
			}
			break;
		default:
			return (DND_ERROR);
	}
	/* SUPPRESS 68 */
	return DndSendEvent(ev->display, &cM, debugname);
}

/*
 * See if the window has set the  _SUN_DRAGDROP_INTEREST atom.  If so, see if
 * the point at which the drop occured happed within a registered drop site.
 */
static int Verification(XButtonEvent *ev, Dnd_info *dnd)
{
	Window child;
	Display *dpy = ev->display;
	Window srcXid = ev->root, dstXid = ev->root;
	int srcX = ev->x_root,
			srcY = ev->y_root,

			dstX, dstY, root_window = False, window_count = 0;
	long *interest_prop = NULL;

	/* DND_HACK begin */
	dnd->is_old = False;
	/* DND_HACK end */

	do {
		if (!XTranslateCoordinates(dpy, srcXid, dstXid, srcX, srcY, &dstX,
						&dstY, &child))
			return (DND_ERROR);	/* XXX: Different Screens !! */

		srcXid = dstXid;
		dstXid = child;
		srcX = dstX;
		srcY = dstY;

		/* If this is the first time through the loop and child == NULL
		 * meaning there are no mapped childern of the root, assume it
		 * is a root window drop.  This of course won't work if someone
		 * is using a virtual window manager.
		 */
		if (!window_count && !child)
			root_window = True;

		window_count++;

		if (child) {
			Atom type;
			int format;
			unsigned long nitems, remain;
			unsigned char *data;

			/* Look for the interest property set on
			 * the icon, wm frame, or an application's
			 * top level window.
			 */
			if (XGetWindowProperty(dpy, child, dnd->atom[INTEREST], 0L, 65535L,
							False, AnyPropertyType, &type, &format,
							&nitems, &remain, &data)
					!= Success) {
				return (DND_ERROR);
			}
			else if (type != None) {
				Window ignore;

				/* Remember the last interest property we
				 * see on our way out to the WM_STATE prop.
				 */
				if (interest_prop)
					XFree((char *)interest_prop);
				interest_prop = (long *)data;

				(void)XTranslateCoordinates(dpy, srcXid, dstXid, srcX, srcY,
						&dstX, &dstY, &ignore);
				/* Save x,y coords of top level window
				 * of drop site.  Will use later.
				 */
				dnd->drop_target_x = dstX;
				dnd->drop_target_y = dstY;
			}
			/* Look for the WM_STATE property set on a
			 * window as we traverse out to the leaf
			 * windows.  If we see WM_STATE, then the user
			 * dropped over an application, not the root.
			 */
			if (XGetWindowProperty(dpy, child, dnd->atom[WMSTATE], 0L, 2L,
							False, AnyPropertyType, &type, &format,
							&nitems, &remain, &data) != Success)
				return (DND_ERROR);
			else if (type != None) {
				XFree((char *)data);
				/* If we saw the WM_STATE but didn't see an
				 * interest property, then this could be a
				 * V2 application.
				 */
				if (!interest_prop) {
					return (IsV2App(dpy, child, dnd, ev));
				}
				else {
					/* If we saw an interest property and saw
					 * the WM_STATE property, then it's safe
					 * to jump out of this loop.
					 */
					break;
				}
			}
		}
	} while (child);

	/* XXX: Revisit once we work out root window drops */
	/* The idea here is that if we only do one xtranslate before we hit the
	 * root window, then the user dropped on the root.
	 */
	if (root_window) {
		return DND_ROOT;
	}
	else if (interest_prop) {
		Dnd_site_desc *drop_site = NULL;
		unsigned int i, nsites;

		/* This might be a legal drop site. We will
		 * need to dig out the _SUN_DRAGDROP_INTEREST
		 * property, construct the rects and determine
		 * if the drop happened within a drop region.
		 */
		ConstructSiteList(dpy, None, interest_prop, &drop_site, &nsites);
		XFree((char *)interest_prop);
		if (!FindDropSite(dnd, drop_site, nsites, &dnd->dropSite)) {
			if (drop_site) {
				for (i = 0; i < nsites; i++) {
					if (drop_site[i].rect) XFree((char *)drop_site[i].rect);
				}
				XFree((char *)drop_site);
			}
			return IsV2App(dpy, child, dnd, ev);
		}
		else {
			for (i = 0; i < nsites; i++) {
				if (drop_site[i].rect) XFree((char *)drop_site[i].rect);
			}
			XFree((char *)drop_site);
			return DND_SUCCEEDED;
		}
	}

	return DND_ILLEGAL_TARGET;
}

/* DND_HACK begin */
static int IsV2App(Display *dpy, Window window, Dnd_info *dnd, XButtonEvent *ev)
{
	Atom type;
	int format;
	unsigned long nitems, remain;
	unsigned char *data;
	Atom v2_app = xv_get(XV_SERVER_FROM_WINDOW(dnd->parent),
									SERVER_ATOM, "_XVIEW_V2_APP");

	if (!window)
		return DND_ILLEGAL_TARGET;

	if (XGetWindowProperty(dpy, window, v2_app, 0L, 1L, False, AnyPropertyType,
						&type, &format, &nitems, &remain, &data) != Success)
	{
		return DND_ERROR;
	}
	else if (type == None) {
		return DND_ILLEGAL_TARGET;
	}
	else {
		dnd->dropSite.site_id = 0;
		dnd->dropSite.window = FindLeafWindow(ev);
		dnd->is_old = True;
		XFree((char *)data);
		return DND_SUCCEEDED;
	}
}
/* DND_HACK end */

/* DND_HACK begin */
static Window FindLeafWindow(XButtonEvent *ev)
{
	Display *dpy = ev->display;
	Window srcXid = ev->window, dstXid = ev->root;
	int srcX = ev->x, srcY = ev->y, dstX, dstY;
	Window child;

	do {
		if (!XTranslateCoordinates(dpy, srcXid, dstXid, srcX, srcY, &dstX,
						&dstY, &child))
			return (DND_ERROR);	/* XXX: Different Screens !! */

		if (!child)
			break;

		srcXid = dstXid;
		dstXid = child;
		srcX = dstX;
		srcY = dstY;
	} while (child);

	return (dstXid);
}
/* DND_HACK end */

static int ConstructSiteList(Display *dpy, Window dest_window, long *prop,
		Dnd_site_desc **dsite, unsigned int *nsites)
{
	long *data;
	unsigned int i, j;
	Dnd_site_desc *drop_site;

	data = prop;
	(void)*data++;	/* Version Number */

	*nsites = (int)*data++;

	drop_site = (Dnd_site_desc *) xv_calloc(*nsites,
								(unsigned)sizeof(Dnd_site_desc));

	for (i = 0; i < *nsites; i++) {
		drop_site[i].window = (Window) * data++;
		drop_site[i].site_id = (long)*data++;
		drop_site[i].flags = (unsigned long)*data++;	/* flags */
		if (*data++ == DND_RECT_SITE) {
			drop_site[i].nrects = (int)*data++;
			drop_site[i].rect = (DndRect *) xv_calloc(drop_site[i].nrects,
					(unsigned)sizeof(DndRect));
			for (j = 0; j < drop_site[i].nrects; j++) {
				drop_site[i].rect[j].x = (int)*data++;
				drop_site[i].rect[j].y = (int)*data++;
				drop_site[i].rect[j].w = (unsigned int)*data++;
				drop_site[i].rect[j].h = (unsigned int)*data++;
			}
		}
		else {	/* DND_WINDOW_SITE */
			drop_site[i].nrects = (int)*data++;
			drop_site[i].rect = (DndRect *) xv_calloc(drop_site[i].nrects,
					(unsigned)sizeof(DndRect));
			for (j = 0; j < drop_site[i].nrects; j++) {
				Window root;
				int x, y;
				unsigned int w, h;
				unsigned int bw, depth;

				if (!XGetGeometry(dpy, (Window) * data,
								&root, &x, &y, &w, &h, &bw, &depth)) {
					/* XXX: Handle Bad Window */
					(void)*data++;
					/* XXX: Skip it for now */
					drop_site[i].rect[j].x = 0;
					drop_site[i].rect[j].y = 0;
					drop_site[i].rect[j].w = 0;
					drop_site[i].rect[j].h = 0;
				}
				else {
					int dstX, dstY;
					Window child;

					/* Window coords must be in coord space of top level
					 * window.
					 */
					if (!XTranslateCoordinates(dpy, (Window) * data++,
									dest_window, x, y, &dstX, &dstY, &child)) {
						/* Different Screens */
						break;
					}
					else {
						drop_site[i].rect[j].x = dstX;
						drop_site[i].rect[j].y = dstY;
						drop_site[i].rect[j].w = w + 2 * bw;	/*Includes border */
						drop_site[i].rect[j].h = h + 2 * bw;
					}
				}
			}
		}
	}
	*dsite = drop_site;
	return (DND_SUCCEEDED);
}

static int FindDropSite(Dnd_info *dnd, Dnd_site_desc *dsl, /* drop site list */
 unsigned int nsites, Dnd_site_desc *site)
{
	int i, j;

	for (i = 0; i < nsites; i++) {
		for (j = 0; j < dsl[i].nrects; j++) {
			if (DND_POINT_IN_RECT(&dsl[i].rect[j], dnd->drop_target_x,
							dnd->drop_target_y)) {
				site->window = dsl[i].window;
				site->site_id = dsl[i].site_id;
				site->flags = dsl[i].flags;
				return (True);
			}
		}
	}
	return (False);
}

/* This ***new*** function - YES, Xv_public - can be used by a
 * 'preview receiver' to reject a drop....
 * Reference (erfihlwebfrygjbv)
 */
Xv_public void dnd_preview_reply(Event *ev, int reject)
{
	XClientMessageEvent *cm;
	Xv_opaque data[5];
	Window dragger;

	/* only allowed with a ACTION_DRAG_PREVIEW event */
	if (event_action(ev) != ACTION_DRAG_PREVIEW) return;
	cm = (XClientMessageEvent *)event_xevent(ev);
	if (! cm) return;

	/* check whether this is a **new** preview event:
	 * old events had data.l[4] == 0 or == DND_FORWARDED_FLAG;
	 */
	if (cm->data.l[4] == 0 || cm->data.l[4] == DND_FORWARDED_FLAG) {
		/* is old */
		return;
	}

	dragger = (Window)cm->data.l[4];

	data[0] = (Xv_opaque)reject;
	data[1] = XV_NULL;
	data[2] = XV_NULL;
	data[3] = XV_NULL;
	data[4] = XV_NULL;
	/* in the beginning we used _SUN_DRAGDROP_PREVIEW here, but then
	 * win_input.c complains about 'unexpected event type'
	 */
	xv_send_message(event_window(ev), dragger, "_SUN_DRAGDROP_ACK", 32,
								data, (int)sizeof(data));
}

/* This ***new*** function - YES, Xv_public - can be used by a
 * 'preview receiver' to reject a drop unless the drag source supports
 * at least one of the named target atoms.
 * 
 * Reference (erfihlwebfrygjbv)
 */
Xv_public void dnd_reject_unless(Event *ev, Atom first, ...)
{
	va_list ap;
	Atom requested[20];
	int i, cnt = 0;
	XClientMessageEvent *xcl;
	Window sender;
	Atom act_type;
	int act_format;
	unsigned long items, left_to_be_read;
	unsigned char *bp;
	unsigned long j;
	Atom *offered;
	int reject = TRUE;

	/* only allowed with a ***new*** ACTION_DRAG_PREVIEW event */
	if (event_action(ev) != ACTION_DRAG_PREVIEW) return;
	xcl = (XClientMessageEvent *)event_xevent(ev);
	if (! xcl) return;

	/* check whether this is a **new** preview event:
	 * old events had data.l[4] == 0 or == DND_FORWARDED_FLAG;
	 */
	if (xcl->data.l[4] == 0 || xcl->data.l[4] == DND_FORWARDED_FLAG) {
		/* is old */
		return;
	}

	sender = xcl->data.l[4];
	requested[cnt++] = first;

	va_start(ap, first);
	while ((requested[cnt++] = va_arg(ap, Atom)));
	va_end(ap);

	if (XGetWindowProperty(xcl->display, sender,
				xcl->message_type, 0L, 1000L, FALSE, XA_ATOM, &act_type,
				&act_format, &items, &left_to_be_read,
				&bp) != Success)
	{
		return;
	}

	if (act_format != 32) return;
	if (act_type != XA_ATOM) return;

	offered = (Atom *)bp;

	for (j = 0; j < items; j++) {
		for (i = 0; i < cnt; i++) {
			if (offered[j] == requested[i]) {
				reject = FALSE;
				break;
			}
		}
	}

	XFree(bp);

	if (reject) {
		dnd_preview_reply(ev, TRUE);
	}
}
