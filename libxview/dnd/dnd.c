#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)dnd.c 1.30 93/06/28 DRA: $Id: dnd.c,v 4.27 2026/05/13 14:07:02 dra Exp $ ";
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
#include <xview/pkg.h>
#include <xview/attr.h>
#include <xview/window.h>
#include <xview_private/dndimpl.h>
#include <xview_private/windowimpl.h>
#include <xview_private/win_info.h>
#include <xview_private/sel_impl.h>
#include <xview_private/svr_impl.h>
#include <xview/win_notify.h>
#include <sys/utsname.h>
#include <X11/Xproto.h>

#define DND_PRIVATE(dnd_public) XV_PRIVATE(Dnd_info, Xv_dnd_struct, dnd_public)
#define DND_PUBLIC(dnd)         XV_PUBLIC(dnd)

#define DND_POINT_IN_RECT(r, xx, yy) \
			((xx) >= (r)->x && (yy) >= (r)->y && \
	  		(xx) < (r)->x+(r)->w && (yy) < (r)->y+(r)->h)

#define DND_IS_TRANSIENT(event) (event->ie_xevent->xclient.data.l[4] & \
				 DND_TRANSIENT_FLAG)

#define DND_NO_SITE	-1

		/* Index into atom array */
#define TRIGGER			0
#define PREVIEW			1
#define ACK			2
#define WMSTATE			3
#define INTEREST		4
#define DSDM			5

#ifdef NO_XDND

#  define NUM_ATOMS		DSDM +1

#else /* NO_XDND */

#  define XdndAware 6
#  define XdndSelection 7
#  define XdndEnter 8
#  define XdndLeave 9
#  define XdndPosition 10
#  define XdndDrop 11
#  define XdndFinished 12
#  define XdndStatus 13
#  define XdndActionCopy 14
#  define XdndActionMove 15
#  define XdndActionLink 16
#  define XdndActionAsk 17
#  define XdndActionPrivate 18
#  define XdndTypeList 19
#  define XdndActionList 20
#  define XdndActionDescription 21

#  define NUM_ATOMS		XdndActionDescription+1
#  define MAXTARGETS 20

#endif /* NO_XDND */

typedef enum {
    Dnd_Trigger_Remote,
    Dnd_Trigger_Local,
    Dnd_Preview
} DndMsgType;

typedef struct dndrect {
    int		x, y;
    unsigned    w, h;
} DndRect;

typedef struct dnd_site_desc {
    Window	 window;
    long	 site_id;
    unsigned int nrects;
    DndRect	*rect;
    unsigned long flags;
} Dnd_site_desc;

typedef struct dndWaitEvent {
    Window 	window;
    int 	eventType;
    Atom	target;
} DnDWaitEvent;

typedef struct dnd_site_rects {
    long	screen_number;
    long	site_id;
    long	window;
    long	x, y;
    long	w, h;
    long	flags;
} DndSiteRects;

typedef struct dnd_info {
    Dnd			 public_self;
    Xv_window		 parent;
    DndDragType		 type;
    Atom		 atom[NUM_ATOMS];
    Xv_opaque		 cursor;
    Cursor		 xCursor;
    Xv_opaque		 affCursor;
    Cursor		 affXCursor;
    Xv_opaque		 rejectCursor;
    Cursor		 rejectXCursor;
    short		 transientSel;
    int			 drop_target_x;
    int			 drop_target_y;
    Dnd_site_desc   	 dropSite;
    struct timeval	 timeout;
    Xv_opaque		 window;
    Selection_requestor	 dsdm_selreq;
    DndSiteRects	*siteRects;
    int			 lastSiteIndex;
    int			 eventSiteIndex;
    unsigned int	 numSites;
    /* DND_HACK begin */
    short		 is_old;
    /* DND_HACK end */
    int			 incr_size;
    int			 incr_mode;  	/* Response from dsdm in INCR. */
    Window               lastRootWindow;
    int                  screenNumber;

	Atom targetlist[MAXTARGETS];
	int numtargets;
#ifdef NO_XDND
#else /* NO_XDND */

	Selection_owner xdnd_owner;
	Window last_top_win;
	int xdnd_used_version;
	char last_top_win_is_aware;
	char xdnd_status_from_last_top_win_seen;
	char xdnd_last_status_was_accept;
	Window *tl_cache;
#endif /* NO_XDND */
} Dnd_info;

/* trace level: */
#define TLXDND 411

static int dnd_key = 0;
static int dnd_site_key	= 0;

#define MYATOM(name)	(Atom)xv_get(server, SERVER_ATOM, name)
#define POINT_IN_SITE(sr, px, py) \
			((px) >= (sr).x && (py) >= (sr).y && \
			 (px) < (sr).x+(sr).w && (py) < (sr).y+(sr).h)
#define SCREENS_MATCH(dnd, i) \
			(dnd->siteRects[i].screen_number == dnd->screenNumber)

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
static void ReplyProc(Selection_requestor sel, Atom target, Atom type,
							Xv_opaque buffer, unsigned long length, int format)
{
	Xv_server server = XV_SERVER_FROM_WINDOW(xv_get(sel, XV_OWNER));

	/* ORIG: if (target == MYATOM("_SUN_DRAGDROP_DSDM")), but _SUN_DRAGDROP_DSDM
	 * was the SEL_RANK, while the SEL_TYPE was "_SUN_DRAGDROP_SITE_RECTS"
	 *
	 * I don't believe that this has ever worked - but on the other it was
	 * probably never used because dsdm = olwm does not answer with INCR...
	 */
	if (target == MYATOM("_SUN_DRAGDROP_SITE_RECTS")) {
		Dnd_info *dnd = (Dnd_info *) xv_get(sel, XV_KEY_DATA, dnd_site_key);

		/* Only handle INCR responses in ReplyProc(). */
		if (type == MYATOM("INCR")) {
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

static int contact_dsdm(Dnd_info	*dnd)
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
				SEL_TYPE, MYATOM("_SUN_DRAGDROP_SITE_RECTS"),
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

/* 
 * Determine what cursor to use, create one if none defined.  Return the XID.
 */
static XID get_cursor(Dnd_info *dnd)
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
				get_cursor(dnd), t);
}

#ifdef NO_XDND
#else /* NO_XDND */
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
					cM.xclient.data.l[2] = (ev->x_root << 16) | ev->y_root;
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


static int send_preview_event(Dnd_info *dnd, int site, XEvent *e)
{
	int i = dnd->eventSiteIndex;

	if (e->type != ButtonPress 
		&& e->type != ButtonRelease
		&& e->type != MotionNotify
		&& e->type != KeyRelease  /* wegen ACTION_STOP */
		)
	{
		fprintf(stderr, "%s-%d: unexpected event type %d in send_preview_event\n",
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

static int find_site(Dnd_info *dnd, XButtonEvent *e)
{
	int i;

	if (POINT_IN_SITE(dnd->siteRects[dnd->lastSiteIndex], e->x_root, e->y_root))
	{
		return send_preview_event(dnd, dnd->lastSiteIndex, (XEvent *) e);
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
			return send_preview_event(dnd, dnd->lastSiteIndex, (XEvent *) e);
		}
	}
	return send_preview_event(dnd, DND_NO_SITE, (XEvent *) e);
}
Xv_private Xv_opaque server_get_timestamp(Xv_Server server_public);

extern int debug_DND;

/* DND_HACK begin */

/* The code highlighted by the words DND_HACK is here to support dropping
 * on V2 clients.  The V3 drop protocol is not compatibile with the V2.
 * If we detect a V2 application, by a property on the its frame, we try
 * to send an V2 style drop event.   This code can be removed once we decide
 * not to support running V2 apps with the latest release.
 */

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

static Bool match_prop(Display *dpy, XEvent *event, XPointer cldt)
{
	DnDWaitEvent *wE = (DnDWaitEvent *)cldt;

    if ((event->type == wE->eventType) &&
                           (((XPropertyEvent*)event)->atom == (Atom)wE->window))
        return(True);
    else
        return(False);
}

typedef Bool (*DndEventWaiterFunc)(Display *, XEvent *, XPointer);

static int wait_for_dnd_event(Display *dpy, Window window, int eventType,
 					Atom target, struct timeval *timeout, XEvent *event,
					DndEventWaiterFunc MatchFunc)
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
static Bool match_dnd_event(Display *dpy, XEvent *event, XPointer cldt)
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
	if ((status = wait_for_dnd_event(dpy, xv_xid(info), SelectionRequest,
						dnd->atom[ACK], &dnd->timeout, &event, match_dnd_event))
			!= DND_SUCCEEDED)
		goto BailOut;

	if (debug_DND > 0)
		fprintf(stderr, "%ld in WairForAck, after wait_for_dnd_event(ACK)\n",
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
		fprintf(stderr, "%ld in WairForAck, before wait_for_dnd_event(prop)\n",
				time(0));
	/* the second param is declared as Window, but match_prop
	 * compares it with the atom of a PropertyNotify event....
	 */
	status = wait_for_dnd_event(dpy, property, PropertyNotify, None, &dnd->timeout,
			&event, match_prop);

	if (debug_DND > 0)
		fprintf(stderr, "%ld in WairForAck, after wait_for_dnd_event(prop)\n",
				time(0));
	/* XXX: This will kill any events someone else has selected for. */
	XSelectInput(dpy, event.xproperty.window, NoEventMask);
	XFlush(dpy);

  BailOut:
	return (status);
}

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

#ifdef NO_XDND
#else /* NO_XDND */


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

static void fill_dnd_toplevel_cache_slow(Dnd_info *dnd, Xv_window dragsource, Atom wmstate)
{
	Xv_screen screen = XV_SCREEN_FROM_WINDOW(dragsource);
	Window *toplevels;
	Xv_window xvroot = xv_get(screen, XV_ROOT);
	Window root = xv_get(xvroot, XV_XID);
	Display *dpy = (Display *)xv_get(dragsource, XV_DISPLAY);
	int idx = 0;

	toplevels = xv_alloc_n(Window, 100L); /* 100 should be enough */
	init_toplevels(dpy, root, wmstate, toplevels, &idx);
	toplevels[idx] = None;
	dnd->tl_cache = toplevels;
}

static void fill_dnd_toplevel_cache(Dnd_info *dnd, Xv_window dragsource, Atom wmstate)
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
		fill_dnd_toplevel_cache_slow(dnd, dragsource, wmstate);
	}
	else {
		dnd->tl_cache = result;
	}
	xv_destroy(sr);
}

#endif /* NO_XDND */

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

static int get_selection(Dnd_info *dnd, Display *dpy)
{
	int i = 0;
	Atom seln;
	Xv_Server server = XV_SERVER_FROM_WINDOW(dnd->parent);
	XID xid;

	/* Application defined selection. */
	if (xv_get(DND_PUBLIC(dnd), SEL_OWN))
		return DND_SUCCEEDED;

	/* Create our own transient selection. */
	/* Look for a selection no one else is using. */

 	xid = (XID) xv_get(dnd->parent, XV_XID);

	/* XXX: This will become very slow if the app
	 * has > 100 selections in use.  We will go
	 * through > 100 XGetSelectionOwner() requests
	 * looking for a free selection.
	 */
	for (i = 0;; i++) {
		seln = InternSelection(server, i, xid);
		if (XGetSelectionOwner(dpy, seln) == None) {
			dnd->transientSel = True;
			xv_set(DND_PUBLIC(dnd), SEL_RANK, seln, SEL_OWN, True, NULL);
			break;
		}
	}
	return DND_SUCCEEDED;
}

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
	if (get_selection(dnd, dpy) == DND_SELECTION)
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

	if (dnd->numtargets > 0) {    /* Reference (khvertgkhbwerfv)  */
		/* this property is supposed to be used only during the
		 * preview phase
		 */
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
					GrabModeAsync, GrabModeAsync, None, get_cursor(dnd),
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
		if (contact_dsdm(dnd))
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
						button_released = True;
					}
				}
				break;
			case LOC_DRAG:
				lasttime = ev->xmotion.time;
				server_set_timestamp(srv, NULL, lasttime);
				if (dsdm_present) {
					find_site(dnd, (XButtonEvent *) ev);
				}
				break;
			case ACTION_STOP:
				lasttime = ev->xkey.time;
				server_set_timestamp(srv, NULL, lasttime);
				/* Send LeaveNotify if necessary */
				if (dsdm_present)
					(void)send_preview_event(dnd, DND_NO_SITE, ev);
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
				if (ev->xclient.message_type == dnd->atom[ACK]) {
					/* this is a 'Preview-Answer' !
					 * At the moment we consider data.l[0] as 'rejected',
					 * see also reference (jklvtrdgkhbegtr)
					 */
					UpdateGrabCursor(dnd, EnterNotify,
										(int)ev->xclient.data.l[0], lasttime);
				}
#ifdef NO_XDND
#else /* NO_XDND */
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
							UpdateGrabCursor(dnd, EnterNotify,
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

	if (dnd->numtargets > 0) {
		/* this property is supposed to be used only during the
		 * preview phase - the preview phase is now over!
		 */
		XDeleteProperty(dpy, xv_xid(info), dnd->atom[PREVIEW]);
	}

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
	/* Reference (jklvtrdgkhbegtr) */
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

	/* new preview events have the drag source window in data.l[4] ! */
	sender = xcl->data.l[4];
	requested[cnt++] = first;

	va_start(ap, first);
	while ((requested[cnt++] = va_arg(ap, Atom)));
	va_end(ap);

	/* Some drag sources set a _SUN_DRAGDROP_PREVIEW property on their
	 * window. The intention is for the "preview receiver" to have a
	 * (preliminary) list of "data format atoms" (usually a subset of
	 * the TARGETS).  Reference (khvertgkhbwerfv)
	 */
	if (XGetWindowProperty(xcl->display, sender,
				xcl->message_type, 0L, 1000L, FALSE, XA_ATOM, &act_type,
				&act_format, &items, &left_to_be_read,
				&bp) != Success)
	{
		return;
	}

	if (act_format != 32) return;
	if (act_type != XA_ATOM) return;

	/* this is the list of "data format atoms" that the drag source offers */
	offered = (Atom *)bp;

	for (j = 0; j < items; j++) {
		for (i = 0; i < cnt; i++) {
			if (offered[j] == requested[i]) {
				/* a matching atom found: we do NOT reject */
				reject = FALSE;
				break;
			}
		}
	}

	XFree(bp);

	if (reject) {
		/* send a rejection client message */
		dnd_preview_reply(ev, TRUE);
	}
}

#define ADONE ATTR_CONSUME(*attrs);break

static int dnd_init(Xv_Window parent, Xv_drag_drop dnd_public,
									Attr_avlist avlist, int *u)
{
    Dnd_info			*dnd = NULL;
    Xv_dnd_struct		*dnd_object;
    Xv_opaque server;

    dnd = (Dnd_info *)xv_alloc(Dnd_info);
    dnd->public_self = dnd_public;
    dnd_object = (Xv_dnd_struct *)dnd_public;
    dnd_object->private_data = (Xv_opaque)dnd;

    dnd->parent = parent ? parent : xv_get(xv_default_screen, XV_ROOT);
    server = XV_SERVER_FROM_WINDOW(dnd->parent);

    dnd->atom[TRIGGER] = (Atom)xv_get(server,
				        			SERVER_ATOM, "_SUN_DRAGDROP_TRIGGER");
    dnd->atom[PREVIEW] = (Atom)xv_get(server,
									SERVER_ATOM, "_SUN_DRAGDROP_PREVIEW");
    dnd->atom[ACK] = (Atom)xv_get(server, SERVER_ATOM, "_SUN_DRAGDROP_ACK");
    dnd->atom[WMSTATE] = (Atom)xv_get(server, SERVER_ATOM, "WM_STATE");
    dnd->atom[INTEREST] =(Atom)xv_get(server,
									SERVER_ATOM, "_SUN_DRAGDROP_INTEREST");
    dnd->atom[DSDM] = (Atom)xv_get(server, SERVER_ATOM, "_SUN_DRAGDROP_DSDM");
#ifdef NO_XDND
#else /* NO_XDND */
	dnd->atom[XdndAware] = (Atom)xv_get(server, SERVER_ATOM, "XdndAware");
	dnd->atom[XdndSelection] = (Atom)xv_get(server, SERVER_ATOM, "XdndSelection");
	dnd->atom[XdndEnter] = (Atom)xv_get(server, SERVER_ATOM, "XdndEnter");
	dnd->atom[XdndLeave] = (Atom)xv_get(server, SERVER_ATOM, "XdndLeave");
	dnd->atom[XdndPosition] = (Atom)xv_get(server, SERVER_ATOM, "XdndPosition");
	dnd->atom[XdndDrop] = (Atom)xv_get(server, SERVER_ATOM, "XdndDrop");
	dnd->atom[XdndFinished] = (Atom)xv_get(server, SERVER_ATOM, "XdndFinished");
	dnd->atom[XdndStatus] = (Atom)xv_get(server, SERVER_ATOM, "XdndStatus");
	dnd->atom[XdndActionCopy] = (Atom)xv_get(server, SERVER_ATOM, "XdndActionCopy");
	dnd->atom[XdndActionMove] = (Atom)xv_get(server, SERVER_ATOM, "XdndActionMove");
	dnd->atom[XdndActionLink] = (Atom)xv_get(server, SERVER_ATOM, "XdndActionLink");
	dnd->atom[XdndActionAsk] = (Atom)xv_get(server, SERVER_ATOM, "XdndActionAsk");
	dnd->atom[XdndActionPrivate] = (Atom)xv_get(server, SERVER_ATOM, "XdndActionPrivate");
	dnd->atom[XdndTypeList] = (Atom)xv_get(server, SERVER_ATOM, "XdndTypeList");
	dnd->atom[XdndActionList] = (Atom)xv_get(server, SERVER_ATOM, "XdndActionList");
	dnd->atom[XdndActionDescription] = (Atom)xv_get(server, SERVER_ATOM, "XdndActionDescription");
#endif /* NO_XDND */

    dnd->type = DND_MOVE;

    dnd->timeout.tv_sec = xv_get(DND_PUBLIC(dnd), SEL_TIMEOUT_VALUE);
    dnd->timeout.tv_usec = 0;

    return XV_OK;
}

static Xv_opaque dnd_set_avlist(Dnd dnd_public, Attr_attribute *avlist)
{
	Dnd_info *dnd = DND_PRIVATE(dnd_public);
	Attr_avlist attrs;

	for (attrs = avlist; *attrs; attrs = attr_next(attrs)) {
		switch (attrs[0]) {
			case DND_TYPE:
				dnd->type = (DndDragType) attrs[1];
				ADONE;
			case DND_CURSOR:
				dnd->cursor = (Xv_opaque) attrs[1];
				ADONE;
			case DND_X_CURSOR:
				dnd->xCursor = (Cursor) attrs[1];
				ADONE;
			case DND_ACCEPT_CURSOR:
				dnd->affCursor = (Xv_opaque) attrs[1];
				ADONE;
			case DND_ACCEPT_X_CURSOR:
				dnd->affXCursor = (Cursor) attrs[1];
				ADONE;
			case DND_REJECT_CURSOR:
				dnd->rejectCursor = (Xv_opaque) attrs[1];
				ADONE;
			case DND_REJECT_X_CURSOR:
				dnd->rejectXCursor = (Cursor) attrs[1];
				ADONE;
			case DND_TIMEOUT_VALUE:
				XV_BCOPY((struct timeval *)attrs[1], &(dnd->timeout),
						sizeof(struct timeval));
				ADONE;
			case SEL_DRAGDROP_DONE:
				{
					Xv_Drawable_info *info;

					DRAWABLE_INFO_MACRO(dnd->parent, info);

					/* there **might** be a property from dnd_send_drop -
					 * actually, it should have been deleted in dnd_send_drop
					 * immediately BEFORE the trigger message has been sent....
					 */
					XDeleteProperty(xv_display(info), xv_xid(info),
							dnd->atom[PREVIEW]);
				}
				ADONE;
#ifdef NO_XDND
#else /* NO_XDND */
			case SEL_OWN:
				{
					int val;
					val = (int)attrs[1];
					if (! val) {
						/* about to give up ownership, maybe as a consequence
						 * of _SUN_DRAGDROP_DONE.
						 */
						if (dnd->xdnd_owner)
							xv_set(dnd->xdnd_owner, SEL_OWN, FALSE, NULL);
					}
				}
				/* do not consume it */
				break;
#endif /* NO_XDND */
			case XV_END_CREATE:
				break;
			default:
				(void)xv_check_bad_attr(DRAGDROP, attrs[0]);
				break;
		}
	}

	return ((Xv_opaque) XV_OK);
}

static Xv_opaque dnd_get_attr(Dnd dnd_public, int *status,
									Attr_attribute attr, va_list args)
{
	Dnd_info *dnd = DND_PRIVATE(dnd_public);
	Xv_opaque value = 0;

	switch (attr) {
		case DND_TYPE:
			value = (Xv_opaque) dnd->type;
			break;
		case DND_CURSOR:
			value = (Xv_opaque) dnd->cursor;
			break;
		case DND_X_CURSOR:
			value = (Xv_opaque) dnd->xCursor;
			break;
		case DND_ACCEPT_CURSOR:
			value = (Xv_opaque) dnd->affCursor;
			break;
		case DND_ACCEPT_X_CURSOR:
			value = (Xv_opaque) dnd->affXCursor;
			break;
		case DND_REJECT_CURSOR:
			value = (Xv_opaque) dnd->rejectCursor;
			break;
		case DND_REJECT_X_CURSOR:
			value = (Xv_opaque) dnd->rejectXCursor;
			break;
		case DND_TIMEOUT_VALUE:
			value = (Xv_opaque) & dnd->timeout;
			break;
		default:
			if (xv_check_bad_attr(DRAGDROP, attr) == XV_ERROR)
				*status = XV_ERROR;
			break;
	}

	return (value);
}

static int dnd_destroy(Dnd dnd_public, Destroy_status status)
{
	Dnd_info *dnd = DND_PRIVATE(dnd_public);

	if (status == DESTROY_CLEANUP) {
		if (dnd->dsdm_selreq)
			xv_destroy(dnd->dsdm_selreq);
		if (dnd->window)
			xv_destroy(dnd->window);

#ifdef NO_XDND
#else /* NO_XDND */
		if (dnd->xdnd_owner)
			xv_destroy(dnd->xdnd_owner);
		if (dnd->tl_cache) xv_free(dnd->tl_cache);
#endif /* NO_XDND */

		if (dnd->siteRects) {
			xv_free(dnd->siteRects);
			/* It is possible that the dnd object will be destroyed before
			 * dnd_send_drop() returns.
			 */
			dnd->siteRects = NULL;
		}

		xv_free(dnd);
	}

	return (XV_OK);
}

/* trace level: */
#define TLXDND 411

typedef struct dnd_drop_site {
    Xv_sl_link           next;
    Xv_drop_site         drop_item;
} Dnd_drop_site;

static int dnd_is_xdnd_key, dnd_transient_key = 0;

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
		selection_reply_proc_t reply_proc;

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
		srpriv->reply_proc = NULL;

		xv_set(sel_req, XV_KEY_DATA, dnd_transient_key, False, NULL);
		xv_set(sel_req, SEL_TYPE_NAME, "_SUN_DRAGDROP_DONE", NULL);

		data = (char *)xv_get(sel_req, SEL_DATA, &length, &format);

    	if (data) XFree(data);
		srpriv->reply_proc = reply_proc;

		{
			Xv_window win = xv_get(sel_req, XV_OWNER);
			Display *dpy = XV_DISPLAY_FROM_WINDOW(win);
			Xv_server srv = XV_SERVER_FROM_WINDOW(win);

			/* das soll fuer ALLE properties 'avail = TRUE' setzen */
    		xv_sel_free_property(srv, dpy, 0L);
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
				SERVERTRACE((TLXDND, "sending XdndFinished\n"));
				DndSendEvent(cM.display, (XEvent *)&cM, "XdndFinished");
			}
		}
	}
	xv_set(sel_req, XV_KEY_DATA, dnd_is_xdnd_key, FALSE, NULL);
#endif /* NO_XDND */
}
const Xv_pkg xv_dnd_pkg = {
    "Drag & Drop", ATTR_PKG_DND,
    sizeof(Xv_dnd_struct),
    SELECTION_OWNER,
    dnd_init,
    dnd_set_avlist,
    dnd_get_attr,
    dnd_destroy,
    NULL		/* BUG: Need find */
};
