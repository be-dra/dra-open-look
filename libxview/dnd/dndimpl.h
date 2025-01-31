#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)dndimpl.h 1.15 93/06/28 DRA: $Id: dndimpl.h,v 4.5 2025/01/07 18:39:29 dra Exp $ ";
#endif
#endif

/*
 *      (c) Copyright 1990 Sun Microsystems, Inc. Sun design patents
 *      pending in the U.S. and foreign countries. See LEGAL NOTICE
 *      file for terms of the license.
 */

#ifndef xview_dndimpl_DEFINED
#define xview_dndimpl_DEFINED

#include <sys/time.h>
#include <X11/Xlib.h>
#include <xview/pkg.h>
#include <xview/attr.h>
#include <xview/window.h>
#include <xview/dragdrop.h>

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

#  define XDND_MY_VERSION 5
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

#define DND_EXPECT_NEW_PREVIEW_EVENT (1<<8)

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

Pkg_private int DndGetSelection(Dnd_info *, Display *);
Pkg_private int DndContactDSDM(Dnd_info *dnd);
Pkg_private int DndFindSite(Dnd_info *, XButtonEvent *);
Pkg_private XID  DndGetCursor(Dnd_info *);
Pkg_private int DndSendPreviewEvent(Dnd_info *dnd, int site, XEvent *e);
Pkg_private Bool DndMatchProp(Display *dpy, XEvent *event, XPointer);
Pkg_private Bool DndMatchEvent(Display *dpy, XEvent *event, XPointer);
typedef Bool (*DndEventWaiterFunc)(Display *, XEvent *, XPointer);
Pkg_private int DndWaitForEvent(Display *dpy, Window window, int eventType,
	Atom target, struct timeval *timeout, XEvent *event,
	DndEventWaiterFunc MatchFunc);
Pkg_private int DndSendEvent(Display *dpy, XEvent *event, const char *nam);

#endif  /* ~xview_dndimpl_DEFINED */
