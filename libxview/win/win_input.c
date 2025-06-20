#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)win_input.c 20.208 93/06/28 DRA: $Id: win_input.c,v 4.38 2025/06/12 17:05:34 dra Exp $";
#endif
#endif

/*
 * (c) Copyright 1989 Sun Microsystems, Inc. Sun design patents pending in
 * the U.S. and foreign countries. See LEGAL NOTICE file for terms of the
 * license.
 */

/*
 * Win_input.c: Implement the input functions of the win_struct.h interface.
 */

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <iconv.h>
#include <sys/time.h>
#include <X11/Xlib.h>		/* required by Xutil.h */
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <xview_private/svr_impl.h>

/*
 * Note: ntfy.h must be before notify.h. (draw_impl.h includes pkg.h which
 * includes notify.h)
 */
#include <xview_private/i18n_impl.h>
#include <xview_private/portable.h>
#include <xview_private/ntfy.h>
#include <xview_private/draw_impl.h>
#include <xview_private/ndis.h>
#include <xview_private/svr_atom.h>
#include <xview_private/svr_impl.h>
#include <xview_private/win_info.h>
#include <xview_private/windowimpl.h>
#include <xview_private/fm_impl.h>
#include <xview_private/sel_impl.h>
#include <xview_private/win_info.h>
#include <xview_private/site_impl.h>
#include <xview_private/xview_impl.h>
#include <xview/defaults.h>
#include <xview/frame.h>
#include <xview/fullscreen.h>
#include <xview/icon.h>
#include <xview/server.h>
#include <xview/win_notify.h>
#include <xview/win_screen.h>
#include <xview/dragdrop.h>
#include <xview/screen.h>
#include <xview/panel.h>
#include <xview/textsw.h>
#include <xview/termsw.h>

static void tvdiff(struct timeval *t1,struct timeval *t2,struct timeval *diff);
static void win_handle_quick_selection(Xv_Drawable_info *info, Event *event);
static int GetButtonEvent(Display *display, XEvent *xevent, char *args);
static int xevent_to_event(Display *, XEvent *, Event *, Xv_object *);
static int win_handle_compose(Event *event, XComposeStatus *c_status, int last);
static int chording(Display *display, XButtonEvent *bEvent, int timeout);
static int win_handle_menu_accel(Event *event);
static int win_handle_window_accel(Event *event);

extern struct rectlist *win_get_damage(Xv_object);
extern char *xv_app_name;
extern int _xv_is_multibyte;

#ifdef NO_XDND
#else /* NO_XDND */
#  include <xview_private/dndimpl.h>
#define TLXDND 411
#endif /* NO_XDND */

Xv_object xview_x_input_readevent(Display *display, Event *event,
								Xv_object req_window, int block, int type,
								unsigned int xevent_mask, XEvent *rep);

static int process_clientmessage_events(Xv_object window,
						XClientMessageEvent *clientmessage, Event *event);
static int process_property_events(Xv_object window, XPropertyEvent *property,
									Event *event);
static int process_wm_pushpin_state(Xv_object window, Atom atom, Event *event);

struct _XKeytrans {
        struct _XKeytrans *next;/* next on list */
        char *string;           /* string to return when the time comes */
        int len;                /* length of string (since NULL is legit)*/
        KeySym key;             /* keysym rebound */
        unsigned int state;     /* modifier state */
        KeySym *modifiers;      /* modifier keysyms you want */
        int mlen;               /* length of modifier list */
};

#define AllMods (ShiftMask|LockMask|ControlMask| \
		 Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask)

#define 	BUFFERSIZE	256
#define		DND_ERROR_CODE  XV_MSG("Unexpected event type in ACTION_DROP_PREVIEW event")

static KeySym xkctks(Display *dpy, unsigned kc, int idx)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#pragma GCC diagnostic ignored "-Wtraditional-conversion"
	return XKeycodeToKeysym(dpy, kc, idx);
#pragma GCC diagnostic pop
}

/*
 * Are we in xv_window_loop ?
 */
int             xv_in_loop;

Xv_private void win_xmask_to_im(unsigned int xevent_mask, Inputmask *im)
{
    int i;

    input_imnull(im);

    if (xevent_mask & ExposureMask)
	win_setinputcodebit(im, WIN_REPAINT);

    if (xevent_mask & PointerMotionMask)
	win_setinputcodebit(im, LOC_MOVE);

    if (xevent_mask & EnterWindowMask)
	win_setinputcodebit(im, LOC_WINENTER);

    if (xevent_mask & LeaveWindowMask)
	win_setinputcodebit(im, LOC_WINEXIT);

    if (xevent_mask & ButtonMotionMask)
	win_setinputcodebit(im, LOC_DRAG);

    if (xevent_mask & KeymapStateMask)
	win_setinputcodebit(im, KBD_MAP);

    if (xevent_mask & VisibilityChangeMask)
	win_setinputcodebit(im, WIN_VISIBILITY_NOTIFY);

    if (xevent_mask & StructureNotifyMask)
	win_setinputcodebit(im, WIN_STRUCTURE_NOTIFY);

    if (xevent_mask & SubstructureNotifyMask)
	win_setinputcodebit(im, WIN_SUBSTRUCTURE_NOTIFY);

    if (xevent_mask & ResizeRedirectMask)
	win_setinputcodebit(im, WIN_RESIZE_REQUEST);

    if (xevent_mask & PropertyChangeMask)
	win_setinputcodebit(im, WIN_PROPERTY_NOTIFY);

    if (xevent_mask & ColormapChangeMask)
	win_setinputcodebit(im, WIN_COLORMAP_NOTIFY);

    if (xevent_mask & SubstructureRedirectMask)
	win_setinputcodebit(im, WIN_SUBSTRUCTURE_REDIRECT);

    if (xevent_mask & KeyPressMask) {
	im->im_flags |= IM_ASCII;
	for (i = 1; i <= (KEY_LEFTLAST - KEY_LEFTFIRST); i++)
	    win_setinputcodebit(im, KEY_LEFT(i));

	for (i = 1; i <= (KEY_RIGHTLAST - KEY_RIGHTFIRST); i++)
	    win_setinputcodebit(im, KEY_RIGHT(i));

	for (i = 1; i <= (KEY_TOPLAST - KEY_TOPFIRST); i++)
	    win_setinputcodebit(im, KEY_TOP(i));
    }
    if (xevent_mask & KeyReleaseMask)
	im->im_flags |= (IM_NEGASCII | IM_NEGEVENT | IM_NEGMETA);

    if (xevent_mask & FocusChangeMask) {
	win_setinputcodebit(im, KBD_USE);
	win_setinputcodebit(im, KBD_DONE);
    }
    if ((xevent_mask & ButtonPressMask) || (xevent_mask & ButtonReleaseMask)) {
	for (i = 1; i <= (BUT_LAST - BUT_FIRST); i++)
	    win_setinputcodebit(im, BUT(i));
	if (xevent_mask & ButtonReleaseMask)
	    im->im_flags |= IM_NEGEVENT;
    }
}


/*
 * Convert Sunview events to xevents.
 */
Xv_private unsigned int win_im_to_xmask(Xv_object window, Inputmask *im)
{
    register unsigned int xevent_mask = 0;
    int    i;

    /*
     * BUG The events that cannot be generated in X are: LOC_STILL,
     * LOC_RGN{EXIT,ENTER}, WIN_STOP, LOC_TRAJECTORY, KBD_REQUEST
     */
    if (win_getinputcodebit(im, LOC_MOVE))
	xevent_mask |= PointerMotionMask;
    if (win_getinputcodebit(im, LOC_WINENTER))
	xevent_mask |= EnterWindowMask;
    if (win_getinputcodebit(im, LOC_WINEXIT))
	xevent_mask |= LeaveWindowMask;
    if (win_getinputcodebit(im, KBD_MAP))
	xevent_mask |= KeymapStateMask;
    if (win_getinputcodebit(im, WIN_VISIBILITY_NOTIFY))
	xevent_mask |= VisibilityChangeMask;
    if ((win_getinputcodebit(im, WIN_CIRCULATE_NOTIFY)) ||
	(win_getinputcodebit(im, WIN_DESTROY_NOTIFY)) ||
	(win_getinputcodebit(im, WIN_GRAVITY_NOTIFY)) ||
	(win_getinputcodebit(im, WIN_MAP_NOTIFY)) ||
	(win_getinputcodebit(im, WIN_REPARENT_NOTIFY)) ||
    /*
     * (win_getinputcodebit(im, WIN_RESIZE)) ||
     */
	(win_getinputcodebit(im, WIN_UNMAP_NOTIFY)))
	xevent_mask |= StructureNotifyMask;

    if (win_getinputcodebit(im, WIN_SUBSTRUCTURE_NOTIFY))
	xevent_mask |= SubstructureNotifyMask;

    if (win_getinputcodebit(im, WIN_RESIZE_REQUEST))
	xevent_mask |= ResizeRedirectMask;

    if (win_getinputcodebit(im, WIN_PROPERTY_NOTIFY))
	xevent_mask |= PropertyChangeMask;

    if (win_getinputcodebit(im, WIN_COLORMAP_NOTIFY))
	xevent_mask |= ColormapChangeMask;

    if ((win_getinputcodebit(im, WIN_CIRCULATE_REQUEST)) ||
	(win_getinputcodebit(im, WIN_CONFIGURE_REQUEST)) ||
	(win_getinputcodebit(im, WIN_MAP_REQUEST)))
	xevent_mask |= SubstructureRedirectMask;

    if (win_getinputcodebit(im, LOC_DRAG)) {
	xevent_mask |= ButtonMotionMask;
    }
    /* BUT(1-3) are MS_{LEFT, MIDDLE, RIGHT} */
    for (i = 1; i <= (BUT_LAST - BUT_FIRST); i++) {
	if (win_getinputcodebit(im, BUT(i))) {
	    xevent_mask |= ButtonPressMask;
	    break;
	}
    }

    /* Set ButtonReleaseMask if some button consumed and IM_NEGEVENT */
    if (xevent_mask & ButtonPressMask && im->im_flags & IM_NEGEVENT)
	xevent_mask |= ButtonReleaseMask;

    if (win_getinputcodebit(im, WIN_REPAINT)) {
	xevent_mask |= ExposureMask;
    }
    /* Enable focus change events if consuming KBD_USE/KBD_DONE */
    if (win_getinputcodebit(im, KBD_USE) ||
	win_getinputcodebit(im, KBD_DONE)) {
	xevent_mask |= FocusChangeMask;
    }
    /*
     * if top level window AND it does not have no decor flag set then turn
     * on StructureNotify  and PropertyChangeMask
     */
    if ((window) && (Bool) xv_get(window, WIN_TOP_LEVEL) &&
	!(Bool) xv_get(window, WIN_TOP_LEVEL_NO_DECOR)) {
	xevent_mask |= StructureNotifyMask | PropertyChangeMask;
    }
    /*
     * NOTE:  If interested in any keyboard events, must set ButtonPressMask
     * and FocusChangeMask for Click to Type.
     * 
     * BUG ALERT: If you are interested in any keyboard events, you will see
     * button press, KBD_USE, and KBD_DONE events whether you want them or
     * not.
     */
    if ((im->im_flags & IM_NEGASCII) ||
	(im->im_flags & IM_NEGMETA))
	xevent_mask |= KeyReleaseMask | ButtonPressMask | FocusChangeMask;

    /*
     * NOTE:  Anything below this point only deal with KeyPressMask.
     */
    if (im->im_flags & IM_ASCII) {
	xevent_mask |= KeyPressMask | FocusChangeMask;
	goto Return;
    }
    for (i = 1; i <= KEY_LEFTLAST - KEY_LEFTFIRST; i++)
	if (win_getinputcodebit(im, KEY_LEFT(i))) {
	    xevent_mask |= KeyPressMask | FocusChangeMask;
	    goto Return;
	}
    for (i = 1; i <= KEY_RIGHTLAST - KEY_RIGHTFIRST; i++)
	if (win_getinputcodebit(im, KEY_RIGHT(i))) {
	    xevent_mask |= KeyPressMask | FocusChangeMask;
	    goto Return;
	}
    for (i = 1; i <= KEY_TOPLAST - KEY_TOPFIRST; i++)
	if (win_getinputcodebit(im, KEY_TOP(i))) {
	    xevent_mask |= KeyPressMask | FocusChangeMask;
	    goto Return;
	}
Return:
    /* Set KeyReleaseMask if consuming keyboard events and IM_NEGEVENT */
    if (im->im_flags & IM_NEGEVENT && xevent_mask & KeyPressMask)
	xevent_mask |= KeyReleaseMask;
    return (xevent_mask);
}

/*
 * Utilities
 */
void input_imnull(struct inputmask *im)
{
    int             i;
    /* BUG:  Use XV_BZERO here */
    im->im_flags = 0;
    for (i = 0; i < IM_MASKSIZE; i++)
	im->im_keycode[i] = 0;
}

void input_imall(struct inputmask *im)
{
    int             i;

    input_imnull(im);
    im->im_flags = IM_ASCII | IM_META;
    for (i = 0; i < IM_MASKSIZE; i++)
	im->im_keycode[i] = 1;
}

/*
 * Find an event for req_window. Block till the event for this window is
 * found.
 */
Xv_object input_readevent(Xv_object window, Event *event)
{
    register Xv_Drawable_info *info;
    XEvent          xevent;
    Xv_object       retval;

    DRAWABLE_INFO_MACRO(window, info);
    retval = xview_x_input_readevent(xv_display(info), event, window, TRUE, FALSE, 0, &xevent);
    
    /* don't depend on the appl to do an XAllowEvent */
    if (retval && event_id(event) == MS_LEFT)
	window_release_selectbutton(window, event);

    return (retval);
}

/* BUG: implement or throw out all this focus stuff */

/* ARGSUSED */
void win_refuse_kbd_focus(Xv_object window)
{
}

/* ARGSUSED */
void win_release_event_lock(Xv_object window)
{
}

int win_set_kbd_focus(Xv_object window, XID xid)
{
	int rtn = 0;
	register Xv_Drawable_info *info;
	Xv_opaque server_public;
	register Window_info *win;

	DRAWABLE_INFO_MACRO(window, info);
	/* Get server info */
	server_public = xv_server(info);

	if (xid == (XID) WIN_NULLLINK)
		xid = None;
	if (!xv_has_focus(info)) {
		Display *display = xv_display(info);
		Time ts;

		rtn = XSetInputFocus(display, xid, RevertToParent,
								ts = server_get_timestamp(server_public));
		win = WIN_PRIVATE(window);
		if (win->softkey_flag) {
			xv_set(server_public, SERVER_FOCUS_TIMESTAMP, ts, NULL);
		}
	}
	return rtn;
}

XID win_get_kbd_focus(Xv_object window)
{
    register Xv_Drawable_info *info;
    XID             xid;
    int             state;

    DRAWABLE_INFO_MACRO(window, info);
    /* PERFORMANCE: Round trip to the server!!! */
    XGetInputFocus(xv_display(info), &xid, &state);
    return (xid == None ? (XID) WIN_NULLLINK : xid);
}

/*
 * Set no-focus state for window.  If true, don't set input focus on click.
 */
void win_set_no_focus(Xv_object window, int state)
{
    register Xv_Drawable_info *info;

    DRAWABLE_INFO_MACRO(window, info);
    xv_set_no_focus(info, state);
}

#define	SET_SHIFTS(e, state, metamask, altmask)	\
	event_set_shiftmask(e, \
	    (((state) & ShiftMask) ? SHIFTMASK : 0) | \
	    (((state) & ControlMask) ? CTRLMASK : 0) | \
	    (((state) & metamask) ? META_SHIFT_MASK : 0) | \
	    (((state) & altmask) ? ALTMASK : 0) | \
	    (((state) & Button1Mask) ? MS_LEFT_MASK : 0) | \
	    (((state) & Button2Mask) ? MS_MIDDLE_MASK : 0) | \
	    (((state) & Button3Mask) ? MS_RIGHT_MASK : 0))

typedef struct {
    Xv_object       window_requested;
    Event           event;
}               Event_info;


/*
 * Predicate for XCheckIfEvent or XIfEvent Checks to see if the xevent
 * belongs to req_window or not.
 */
static Bool is_reqwindow(Display *display, XEvent *xevent, char *info)
{
    Xv_object       ie_window;
    XAnyEvent      *any = (XAnyEvent *) xevent;
    int             event_type = (xevent->type & 0177);
    Event_info     *event_info = (Event_info *)info;

    /*
     * Check for proper window before calling xevent_to_event, so translation
     * only takes place if event is wanted.
     */
    if (event_type > 1 &&
	event_info->window_requested == win_data(display, any->window) &&
	!xevent_to_event(display, xevent, &event_info->event, &ie_window))
	return (TRUE);
    else
	return (FALSE);
}

/*
 * Find an event for window.  Block until the event for the window is found.
 */
Xv_object xv_input_readevent(Xv_object window, Event *event, int block,
											int type, Inputmask *im)
{
    register Xv_Drawable_info *info;
    unsigned int    xevent_mask = 0; /* na ja, ich weiss nicht wirklich... */
    XEvent          xevent;
    extern Xv_object xv_default_display;
    Xv_object	    retval;

    if (im) {
	xevent_mask = win_im_to_xmask(window, im);
	if (((Bool) xv_get(window, WIN_TOP_LEVEL) == TRUE) &&
	    !((Bool) xv_get(window, WIN_TOP_LEVEL_NO_DECOR) == TRUE))
	    xevent_mask &= ~StructureNotifyMask & ~PropertyChangeMask;
    }
   
    if (window) 
      DRAWABLE_INFO_MACRO(window, info);


    retval = xview_x_input_readevent((window ? xv_display(info) : 
				     (Display *) xv_default_display),
				    event, window, block, type, xevent_mask, 
				    &xevent);
    
    /* don't depend on the app. to do an XAllowEvent */
    if (retval && event_id(event) == MS_LEFT)
	window_release_selectbutton(window, event);

    return (retval);
}

#ifdef NO_XDND
#else /* NO_XDND */

static void prepare_drag_preview_from_xdnd(Window_info *framepriv,
			Xv_Drawable_info *info, Xv_object window, Xv_opaque dropsite, 
			XClientMessageEvent *clientmessage, Event *event, int evid)
{
	int actual_x, actual_y;
	Xv_window realdropwin;
	Window dxid;

	realdropwin = xv_get(dropsite, XV_OWNER);
	dxid = (Window)xv_get(realdropwin, XV_XID);

	/* now we know what the 'real' target window is */
	event_set_window(event, realdropwin);
	event_set_action(event, ACTION_DRAG_PREVIEW);
	event_set_id(event, evid);

	(void)win_translate_xy_internal(xv_display(info),
						xv_xid(info), dxid,
						framepriv->drop_x, framepriv->drop_y,
						&actual_x, &actual_y);

	event_set_x(event, actual_x);
	event_set_y(event, actual_y);

	/* save off the clientmessage info into the window struct */
	window_set_client_message(window, clientmessage);

	/* Set the time of the preview event */
	event->ie_time.tv_sec =
				((unsigned long)clientmessage->data.l[3]) /1000;
	event->ie_time.tv_usec =
				(((unsigned long)clientmessage->data.l[3]) %
						 1000) * 1000;

	/* we still want to be able to recognize it... */
	event_set_flags(event, DND_IS_XDND);
}

struct xdnd_predicate_args_t {
	Window window;
	unsigned int xevent_mask;
};

static Bool xdnd_predicate(Display *dpy, XEvent *ev, char *arg)
{
	struct xdnd_predicate_args_t *cldt = (struct xdnd_predicate_args_t *)arg;

	if (ev->xany.window != cldt->window) return FALSE;
	if (ev->type == ClientMessage) return TRUE;
	if (ev->type == PropertyNotify) return TRUE;
	if (ev->type == SelectionClear) return TRUE;
	if (ev->type == SelectionRequest) return TRUE;
	if (ev->type == SelectionNotify) return TRUE;
	if ((ButtonMotionMask & cldt->xevent_mask) != 0) {
		if (ev->type == MotionNotify) return TRUE;
	}
	if ((ButtonReleaseMask & cldt->xevent_mask) != 0) {
		if (ev->type == ButtonRelease) return TRUE;
	}
	if ((KeyReleaseMask & cldt->xevent_mask) != 0) {
		if (ev->type == KeyRelease) return TRUE;
	}
	return FALSE;
}

static void xdnd_drop_received(Xv_window window, Xv_server srv,
			Xv_Drawable_info *info, XClientMessageEvent *clm, Event *event)
{
	Window_info *framepriv = WIN_PRIVATE(window);
	Xv_window realdropwin;
	int actual_x, actual_y;
	Window dxid;
	XClientMessageEvent origxcl;

	SERVERTRACE((TLXDND, "XdndDrop from %lx to %lx\n", clm->data.l[0],
												clm->window));

	/* we need that later ...
	 * This is one of the misdesigns of the Xdnd protocol:
	 * ++ first phase: client messages (preview and drop)
	 * ++ second phase: selection requests, no need to remember the drag source
	 * ++ third phase: this is the misfit: need to send a XdndFinished
	 *    message to the drag source... a selection request
	 *    for the target, say, XdndFinished would have served the same
	 *    purpose...
	 *    In XView, we use the target _SUN_DRAGDROP_DONE or
	 *    _SUN_SELECTION_END for that purpose.
	 */
	framepriv->xdnd_sender = clm->data.l[0];

	/* I want to dress this as if it were a real XView-Drop */

	/* first, we save the original client message */
	origxcl = *clm;

	/* here, we must provide (at least) everything that is
	 * inspected in dnd_decode_drop()
	 */
	clm->message_type = xv_get(srv, SERVER_ATOM, "_SUN_DRAGDROP_TRIGGER");
	clm->data.l[0] = xv_get(srv,
							SERVER_ATOM, "XdndSelection");
	clm->data.l[1] = origxcl.data.l[2];
	clm->data.l[2] = framepriv->droppos;
	clm->data.l[3] = (long)xv_get(framepriv->dropped_site,
													DROP_SITE_ID);
	clm->data.l[4] = DND_TRANSIENT_FLAG; /* no DND_ACK_FLAG !! */

	realdropwin = xv_get(framepriv->dropped_site, XV_OWNER);
	dxid = (Window)xv_get(realdropwin, XV_XID);

	/* now we know what the 'real' target window is */
	event_set_window(event, realdropwin);

	win_translate_xy_internal(xv_display(info),
							xv_xid(info), dxid,
							framepriv->drop_x, framepriv->drop_y,
							&actual_x, &actual_y);

	event_set_x(event, actual_x);
	event_set_y(event, actual_y);

	/* we still want to be able to recognize it... */
	event_set_flags(event, DND_IS_XDND);

	/* save off the clientmessage info into the window struct */
	window_set_client_message(realdropwin, clm);

	/* Set the time of the drop event */
	event->ie_time.tv_sec =
						((unsigned long)clm->data.l[1]) /
						1000;
	event->ie_time.tv_usec =
						(((unsigned long)clm->data.l[1]) %
						 1000) * 1000;

	if (framepriv->dropaction == XDND_ACTION_MOVE) {
		event_set_action(event, ACTION_DRAG_MOVE);
	}
	else {
		event_set_action(event, ACTION_DRAG_COPY);
	}
}
#endif /* NO_XDND */

static char *evtypes[] = {
	"evt 0",
	"evt 1",
	"KeyPress",
	"KeyRelease",
	"ButtonPress",
	"ButtonRelease",
	"MotionNotify",
	"EnterNotify",
	"LeaveNotify",
	"FocusIn",
	"FocusOut",
	"KeymapNotify",
	"Expose",
	"GraphicsExpose",
	"NoExpose",
	"VisibilityNotify",
	"CreateNotify",
	"DestroyNotify",
	"UnmapNotify",
	"MapNotify",
	"MapRequest",
	"ReparentNotify",
	"ConfigureNotify",
	"ConfigureRequest",
	"GravityNotify",
	"ResizeRequest",
	"CirculateNotify",
	"CirculateRequest",
	"PropertyNotify",
	"SelectionClear",
	"SelectionRequest",
	"SelectionNotify",
	"ColormapNotify",
	"ClientMessage",
	"MappingNotify"
};

/*
 * Read an event from an This needs to be rewritten
 */
Xv_object xview_x_input_readevent(Display *display, Event *event,
								Xv_object req_window, int block, int type,
								unsigned int xevent_mask, XEvent *rep)
{
	Xv_object window = 0;
	Xv_Drawable_info *info;
	Server_info *server;

	/*
	 * Read an event for the req_window.
	 */
	if (req_window) {
		Event_info event_info;

		DRAWABLE_INFO_MACRO(req_window, info);
		if (type) {
			if (block) {
#ifdef NO_XDND
				XWindowEvent(display, xv_xid(info), xevent_mask, rep);
#else /* NO_XDND */
				/* here we want to receive ClientMessages (XdndStatus) -
				 * this doesn't work with XWindowEvent
				 */

				/* In the DnD-case we come here with
				 * ButtonMotionMask | ButtonReleaseMask | KeyReleaseMask
				 * - however, there might be other calls that want MORE
				 * events. We are careful (because of xdnd_predicate !)
				 * and perform a check here.
				 */
				unsigned int testmask =
						~(ButtonMotionMask | ButtonReleaseMask |
						KeyReleaseMask);

				if ((testmask & xevent_mask) != 0) {
					/* this is not the call from dnd.c, we do in the old
					 * way, using XWindowEvent
					 */
					XWindowEvent(display, xv_xid(info), (long)xevent_mask, rep);
				}
				else {
					struct xdnd_predicate_args_t cldt;

					cldt.window = xv_xid(info);
					cldt.xevent_mask = xevent_mask;
					XIfEvent(display, rep, xdnd_predicate, (char *)&cldt);
				}
#endif /* NO_XDND */

				xevent_to_event(display, rep, event, &window);
			}
			else {
				if (!XCheckWindowEvent(display, xv_xid(info), (long)xevent_mask, rep))
					return (0);
				xevent_to_event(display, rep, event, &window);
			}
			event_set_window(event, req_window);
		}
		else {
			event_info.window_requested = req_window;
			if (block)
				XIfEvent(display, rep, is_reqwindow, (char *)&event_info);
			else if (!XCheckIfEvent(display, rep, is_reqwindow,
							(char *)&event_info))
				return (0);	/* window. pending and last event not set */
			window = event_info.window_requested;
			*event = event_info.event;
			/* set the window in the event */
			event_set_window(event, window);
		}
	}
	else {
		static int debug_xview_ev = -1;
		if (debug_xview_ev < 0) {
			char *envxvdeb = getenv("XVEV_DEBUG");

			if (envxvdeb && *envxvdeb) debug_xview_ev = atoi(envxvdeb);
			else debug_xview_ev = 0;
		}
		XNextEvent(display, rep);

		if (debug_xview_ev) {
			fprintf(stderr, "%ld: ev %s %s\n", time(0), 
					rep->type < sizeof(evtypes)/sizeof(evtypes[0]) ? evtypes[rep->type] : "????",
					rep->type==PropertyNotify
						? XGetAtomName(rep->xany.display,
										rep->xproperty.atom)
						: (
					rep->type==ClientMessage
						? XGetAtomName(rep->xany.display,
										rep->xclient.message_type)
						: (
					rep->type==SelectionRequest
						? XGetAtomName(rep->xany.display,
										rep->xselectionrequest.target)
						: (
					rep->type==SelectionNotify
						? XGetAtomName(rep->xany.display,
										rep->xselection.target)
						: "")))
					);
		}

		/* BEFORE_DRA_CHANGED_IT
		 * so habe ich rausgefunden, dass der X-Server bei
		 * Mode_switch-modifizierten Keys IMMER das state-bit 0x2000
		 * anknipst, EGAL auf welchen Modifier man Mode_switch abbildet:

		 if (rep->xany.type == KeyPress) {
		 	XKeyEvent *xk = &rep->xkey;

		 	if (xk->state != 0) fprintf(stderr, "state %x\n", xk->state);
		 }
		 */
		xevent_to_event(display, rep, event, &window);
	}

	if (win_data(display, ((XAnyEvent *) rep)->window)) {
		/* DRA comment: what nonsense is this? if I use the
		 * SERVER_EXTERNAL_XEVENT_* attributes, I do this for EXTERNAL
		 * windows - therefore you cannot expect win_data to return
		 * an XView object.....
		 */
		XV_SL_TYPED_FOR_ALL(SERVER_PRIVATE(xv_default_server), server,
				Server_info *)
				if (server->xidlist && server->xdisplay == display)
			server_do_xevent_callback(server, display, rep);
	}
	else {
		/* this is for EXTERNAL windows - dear people... */
		XV_SL_TYPED_FOR_ALL(SERVER_PRIVATE(xv_default_server), server,
				Server_info *)
				if (server->xidlist && server->xdisplay == display)
			server_do_xevent_callback(server, display, rep);
	}

	/*
	 * pending = QLength(display);
	 */
	return (window);
}


/*
 * This converts an xevent to an XView event. If the event was invalid i.e.
 * no window wanted it ; *pwindow = 0; return 1 else *pwindow = window to
 * which event is to be posted to ; return 0
 * 
 * NOTE: Code has been placed here for handling click to type.  This isn't a
 * terribly appropriate place for it, but it is convenient, since all X
 * events pass through here before becoming  SunView events.  A modification
 * has been made to is_reqwindow, so each X event should be seen here exactly
 * once.
 */

static int xevent_to_event(Display *display, XEvent *xevent, Event *event,
    								Xv_object *pwindow)
{
	register int event_type = (xevent->type & 0177);
	register unsigned temp;
	XAnyEvent *any = (XAnyEvent *) xevent;
	Xv_object window = XV_NULL;
	Xv_Drawable_info *info;
	Xv_opaque srv = XV_NULL;
	static XID pointer_window_xid;
	static short nbuttons = 0;
	static Xv_opaque last_server_public = XV_NULL;
	static unsigned int but2_mod, but3_mod, chord, *key_map;
	static KeySym help_keysym;
	static int alt_modmask = 0,
			meta_modmask = 0,
			num_lock_modmask = 0,
			quick_modmask = 0,
			menu_flag,
			chording_timeout, quote_next_key = FALSE, suspend_mouseless = FALSE;
	static u_char *ascii_sem_map, *key_sem_maps[__SEM_INDEX_LAST],
			/* ACC_XVIEW */
	   *acc_map;

	/* ACC_XVIEW */
	static XComposeStatus *compose_status;

#ifdef OW_I18N
	static KeySym paste_keysym, cut_keysym;
#endif

	/* XXX: assuming 0 => error, 1 => reply */
	/* XXX: This is bogus!  When will X return an event type < 1??? */
	if (event_type > 1) {
		Window_info *win;

		window = win_data(display, any->window);
		if (!window) {
			if (event_type == ClientMessage) {
				XClientMessageEvent *cme = (XClientMessageEvent *) xevent;

				if (cme->data.l[0] == XV_POINTER_WINDOW) {
					window = win_data(display, pointer_window_xid);
				}
				else if (cme->data.l[0] == XV_FOCUS_WINDOW) {
					XID focus_window_id;
					int dummy;

					(void)XGetInputFocus(display, &focus_window_id, &dummy);
					window = win_data(display, focus_window_id);
				}
				/* MappingNotify's are handled here because there is no window
				 * associated with the event.
				 */
			}
			else if (event_type == MappingNotify) {
				XMappingEvent *e = (XMappingEvent *) xevent;

				if (e->request == MappingKeyboard)
					XRefreshKeyboardMapping(e);
				else if (e->request == MappingModifier) {
					/* Get the server object from the display structure. */
					if (XFindContext(display, (Window) display,
									(XContext) xv_get(xv_default_server,
											SERVER_DISPLAY_CONTEXT),
									(caddr_t *) & srv)) {

						*pwindow = XV_NULL;
						return 1;
					}
					/* Update our notion of the modifiers. */
					server_refresh_modifiers(srv, FALSE);
					/* Cache the new modifier masks. */
					alt_modmask = (int)xv_get(srv, SERVER_ALT_MOD_MASK);
					meta_modmask = (int)xv_get(srv, SERVER_META_MOD_MASK);
					quick_modmask = (int)xv_get(srv, SERVER_SEL_MOD_MASK);
					num_lock_modmask =(int)xv_get(srv,SERVER_NUM_LOCK_MOD_MASK);
				}
			}
			if (!window) {

#ifdef OW_I18N
				if (XFilterEvent(xevent, NULL) == True) {
					*pwindow = XV_NULL;
					return (NULL);
				}
#endif

				if (xevent->type >= LASTEvent) {
					void (*extensionProc)(Display *,XEvent *, Xv_window);

					if (! srv) {
						XFindContext(display, (Window) display,
									(XContext) xv_get(xv_default_server,
											SERVER_DISPLAY_CONTEXT),
									(caddr_t *) &srv);
					}

					if (! srv) {
						*pwindow = XV_NULL;
						return 1;
					}

					extensionProc = (void (*)(Display *,XEvent *, Xv_window))xv_get(srv,
													SERVER_EXTENSION_PROC);

					if (extensionProc) {
						(*extensionProc)(display, xevent, window);
					}
				}
				*pwindow = XV_NULL;
				return 1;
			}
		}
		win = WIN_PRIVATE(window);

		/* Get server info */
		DRAWABLE_INFO_MACRO(window, info);
		srv = xv_server(info);

		/*
		   * The following code caches server specific information such as the
		   * keymap, position of the modifier key in the modifier mask and the
		   * number of mouse buttons associated with the mouse.
		 */
		if (last_server_public != srv) {
    		int i;

			last_server_public = srv;
			key_map = (unsigned int *)xv_get(srv, SERVER_XV_MAP);
			for (i = 0; i < __SEM_INDEX_LAST; i++) {
				key_sem_maps[i] = (u_char *) xv_get(srv,SERVER_SEMANTIC_MAP, i);
			}
			ascii_sem_map = (u_char *) xv_get(srv, SERVER_ASCII_MAP);

			/* Cache the modifier masks. */
			alt_modmask = (int)xv_get(srv, SERVER_ALT_MOD_MASK);
			meta_modmask = (int)xv_get(srv, SERVER_META_MOD_MASK);
			quick_modmask = (int)xv_get(srv, SERVER_SEL_MOD_MASK);
			help_keysym = (KeySym)xv_get(srv, SERVER_HELP_KEYSYM);
			num_lock_modmask = (int)xv_get(srv,
					SERVER_NUM_LOCK_MOD_MASK);

			/*
			 * Get the number of phyical mouse buttons and the button modifier
			 * masks.  This info is cached.
			 */
			nbuttons = (short)xv_get(srv, SERVER_MOUSE_BUTTONS);
			but2_mod = (unsigned int)xv_get(srv, SERVER_BUTTON2_MOD);
			but3_mod = (unsigned int)xv_get(srv, SERVER_BUTTON3_MOD);
			chording_timeout =
					(int)xv_get(srv, SERVER_CHORDING_TIMEOUT);
			chord = (unsigned int)xv_get(srv, SERVER_CHORD_MENU);
			compose_status = (XComposeStatus *) xv_get(srv,
					SERVER_COMPOSE_STATUS);

#ifdef OW_I18N
			paste_keysym = (KeySym) xv_get(srv, SERVER_PASTE_KEYSYM);
			cut_keysym = (KeySym) xv_get(srv, SERVER_CUT_KEYSYM);
#endif
		}

#ifdef OW_I18N
		/* 
		 * Workaround for XGrabKey*() problem using frontend method
		 * Skip XFilterEvent() if the client is grabbing the keyboard
		 */
		if (WIN_IS_GRAB(win) || WIN_IS_PASSIVE_GRAB(win)) {
			if (event_type == KeyPress || event_type == KeyRelease) {
				if (win->active_grab) {
					goto ContProcess;
				}
				if (win->passive_grab) {
					KeySym ksym = xkctks(display,
							((XKeyEvent *) xevent)->keycode, 0);

					if (ksym == paste_keysym || ksym == cut_keysym)
						goto ContProcess;
				}
			}
		}
		if (XFilterEvent(xevent, NULL) == True) {
			*pwindow = XV_NULL;
			return (NULL);
		}
	  ContProcess:
#endif

		/*
		 * Check if window is deaf or is NOT involved in xv_window_loop (when
		 * it is active) and the event type is one that we would want it to
		 * ignore. Also check if the select button is pressed so that
		 * XAllowEvents can be called.
		 */
		if ((WIN_IS_DEAF(win) || (WIN_IS_IN_LOOP && !WIN_IS_LOOP(win))) &&
				((event_type == KeyPress) ||
						(event_type == KeyRelease) ||
						(event_type == ButtonPress) ||
						(event_type == ButtonRelease) ||
						(event_type == MotionNotify))) {

			XButtonEvent *buttonEvent = (XButtonEvent *) & xevent->xbutton;

			if ((event_type == ButtonPress) && (buttonEvent->button == Button1)) {
				window_x_allow_events(display, buttonEvent->time);
			}
			*pwindow = XV_NULL;
			return 1;
		}

	}
	else {
		fprintf(stderr, "UNEXPECTED event with type %d\n", event_type);
		*pwindow = XV_NULL;
		return 1;
	}


	/* clear out the event */
	event_init(event);

	/* set the window */
	event_set_window(event, window);

	/* make a reference to the XEvent */
	event_set_xevent(event, xevent);

	switch (event_type) {
		case KeyPress:
		case KeyRelease:
			{
				XKeyEvent *ek = (XKeyEvent *) xevent;
				static char buffer[BUFFERSIZE];
				static int buf_length = 0;
				KeySym ksym, sem_ksym, acc_ksym;
				unsigned int key_value;
				int modifiers = 0, acc_modifiers = 0, sem_action, keyboard_key;
				int status = True, old_chars_matched =

						compose_status->chars_matched;

#ifdef OW_I18N
				Status ret_status;
				XIC ic = NULL;
				static char *buffer_backup = buffer;

				if (event_type == KeyPress) {
					Window_info *win = WIN_PRIVATE(window);

					/*
					 * Dosen't use xv_get() for the performance reason.
					 * If false, ic value is NULL.
					 */
					if (win->win_use_im && win->ic_active)
						ic = win->xic;
				}
#endif

				/*
				 * Clear buffer before we fill it again.  Only NULL out the
				 * number of chars that where actually used in the last pass.
				 */

#ifdef OW_I18N
				if (buffer_backup != buffer) {	/*last time was overflowed to backup */
					xv_free(buffer_backup);
					XV_BZERO(buffer, BUFFERSIZE);	/* may have filled the whole
													   array last time */
					buffer_backup = buffer;
				}
				else
#endif
					XV_BZERO(buffer, (size_t)buf_length);

				if (ek->state & num_lock_modmask) {
					/* Num Lock is on.  For the keycode, if it has a key pad
					 * keysym in its row, then send event as keypad key. 
					 */
#ifdef OW_I18N
						goto DoLookup;
#else
					buf_length = XLookupString(ek, buffer, BUFFERSIZE, &ksym,
								compose_status);
#endif
				}
				else {
					/*
					 * Num Lock is off: Don't use the default policy in
					 * Xlib for the right keypad navigation keys.
					 */
					ksym = xkctks(display, ek->keycode, 0);
					switch (ksym) {
						case XK_Left:
						case XK_Right:
						case XK_Up:
						case XK_Down:
						case XK_Home:
						case XK_R7:	/* Home key on Sun Type-4 keyboard */
						case XK_End:
						case XK_R13:	/* End key on Sun Type-4 keyboard */
						case XK_R9:	/* PgUp key */
						case XK_R15:	/* PgDn key */
							buf_length = 0;
							break;
						default:

#ifdef OW_I18N
						  DoLookup:
							ksym = NoSymbol;
							if (ic) {	/* then keyPress case */
								buf_length =
										XmbLookupString(ic, ek, buffer_backup,
										BUFFERSIZE, &ksym, &ret_status);
								if (ret_status == XBufferOverflow) {
									buffer_backup =
											(char *)xv_malloc(buf_length + 1);
									buf_length =
											XmbLookupString(ic, ek,
											buffer_backup, buf_length, &ksym,
											&ret_status);
								}
							}
							else {
								buf_length =
										XLookupString(ek, buffer, BUFFERSIZE,
										&ksym, compose_status);
								ret_status =
										(ksym ==
										NoSymbol) ? XLookupNone : ((buf_length >
												1) ? XLookupBoth :
										XLookupKeySym);
							}
							if (ret_status == XLookupNone) {
								*pwindow = XV_NULL;
								return 1;
							}
#else /* OW_I18N */
							buf_length =
									XLookupString(ek, buffer, BUFFERSIZE, &ksym,
									compose_status);
#endif /* OW_I18N */

							break;
					}
				}
				if (ksym == XK_ISO_Level3_Shift) {
					ksym = XK_Mode_switch;
				}

				if (event_type == KeyPress)
					event_set_down(event);
				else
					event_set_up(event);

				if (compose_status->chars_matched || old_chars_matched)
					status = win_handle_compose(event, compose_status,
							old_chars_matched);

				/*
				 * If the event is a synthetic event (the event came from
				 * SendEvent request), and the key-pressed is not the help key
				 * (for wm); we should ignore it for security reasons.
				 *
				 * DRA: but the Help key can have been reassigned 
				 * (Workspace Properties - Keyboard Core Functions)
				 * to some other key than XK_Help !
				 */
				if (((xevent->xany.send_event) &&

#ifdef OW_I18N
							(((XKeyEvent *) xevent)->keycode) &&
#endif
							(ksym != help_keysym)) && !defeat_event_security) {
					*pwindow = XV_NULL;
					return 1;
				}

				/*
				 * Determine if this ksym is in the Keyboard Keysym set.  These
				 * are basically your Function, Shift, Ctrl, Meta, Keypad, etc
				 * keys.
				 */
				keyboard_key = ((ksym & KEYBOARD_KYSM_MASK) == KEYBOARD_KYSM);

				/* Set the modifier states */
				SET_SHIFTS(event, ek->state, meta_modmask, alt_modmask);

				/*
				 * Determine offsets into the semantic mapping tables.
				 */
				/* Ref (jklvwerfmbhgrf) */
				if (event_ctrl_is_down(event))
					modifiers += 0x100;
				if (event_meta_is_down(event))
					modifiers += 0x200;
				if (event_alt_is_down(event))
					modifiers += 0x400;

				/*
				 * Keep separate keysym and modifier offsets for menu 
				 * accelerators
				 */
				acc_modifiers = modifiers;
				acc_ksym = ksym;

				/*
				 * Shift handling
				 */
				if (event_shift_is_down(event) && keyboard_key) {
					/*
					 * Add shift offset for menu accelerator lookup
					 */
					acc_modifiers += 0x800;

					/*
					 * Add Shift offset only if a keyboard key is pressed (i.e.
					 * function keys, etc..). 
					 */
					modifiers += 0x800;
				}

				/*
				 * If the keysym is in the keyboard keysym set, check to see if
				 * it maps into an XView ie_code.  (eg. KEY_LEFT(5)...)
				 */

#ifdef OW_I18N
				key_value = (unsigned char)buffer_backup[0];
#else
				key_value = (unsigned char)buffer[0];
#endif

				if (keyboard_key) {
					key_value = ((key_map[(int)ksym & 0xFF] == ksym) ||
							(!key_map[(int)ksym & 0xFF])) ? key_value :
							key_map[(int)ksym & 0xFF];
				}
				event_set_id(event, key_value);

				/*
				 * The semantic table only wants to see a shifted key when the
				 * shift key is down, not when it is caused by the lock key.  So
				 * if the lock key is down and not the shift key, we unshift the
				 * ksym and then do our semantic lookup.
				 */
				sem_ksym = ksym;
				if ((ek->state & LockMask) && !(ek->state & ShiftMask) &&
						(ksym >= 'A' && ksym <= 'Z'))
					sem_ksym = ksym | 0x20;

				/*
				 * Look up in a semantic table to see if the event has an XView
				 * semantic event mapping (eg. ACTION_OPEN).  There is one table
				 * for keyboard keysysm and one for ascii keysyms.
				 */
				if (keyboard_key) {
					int idx = server_sem_map_index(ksym);

					if (key_sem_maps[idx]) {
						sem_action = key_sem_maps[idx][(sem_ksym & 0xFF) +
														modifiers] |
														XVIEW_SEMANTIC;
					}
					else {
						/* We encounter cases where all the semantic events
						 * (OpenWindows.KeyboardCommand.*) fit in 
						 * key_sem_maps[0] (KeySyms <= 0xffff).
						 * But individual programs might want to react on
						 * events that map to KeySyms like XF86Search
						 * (= 0x1008ff1b) - this would attempt to access
						 * key_sem_maps[3] which might be NULL
						 */
						sem_action = ACTION_NULL_EVENT;
					}
				}
				else
					sem_action = ascii_sem_map[(sem_ksym & 0xFF) + modifiers] |
							XVIEW_SEMANTIC;

				/*
				 * If the keypress is modified by Meta alone, and there is no
				 * semantic action defined, then consider the event a Window
				 * Level Accelerator.
				 */
				if (event_is_down(event) &&
						(modifiers & 0xF00) == 0x200
						&& !event_shift_is_down(event)
						&& sem_action == ACTION_NULL_EVENT) sem_action =
							ACTION_ACCELERATOR;

				/* ACC_XVIEW */
				/*
				 * Check if this event is a global/menu accelerator
				 * This overrides window accelerators as well as semantic actions
				 */
				acc_map =
						(u_char *) xv_get(srv,
						SERVER_ACCELERATOR_MAP);
				if (acc_map && acc_map[(acc_ksym & 0xFF) + acc_modifiers]) {
					XEvent *xevent = event_xevent(event);
					XKeyEvent *ek = (XKeyEvent *) xevent;
					Frame frame;
					Frame_menu_accelerator *menu_accelerator;

					/*
					 * Check if this menu accelerator exists for the frame
					 * containing this window
					 */
					frame = xv_get(event_window(event), WIN_FRAME);
					menu_accelerator = (Frame_menu_accelerator *) xv_get(frame,
							FRAME_MENU_X_ACCELERATOR,
							ek->keycode, ek->state, NoSymbol);
					/*
					 * Accelerator exists - set semantic action
					 */
					if (menu_accelerator) {
						sem_action = ACTION_ACCELERATOR;
					}
				}
				/* ACC_XVIEW */

				if (event_is_down(event) &&
						(key_value < VKEY_FIRSTSHIFT
			|| key_value > VKEY_LASTSHIFT)) {
					/* Non-shift keypress event */
					/* -- Priority 1: next key is quoted */
					if (quote_next_key) {
						sem_action = ACTION_NULL_EVENT;
						quote_next_key = FALSE;
					}
					/* -- Priority 2: ResumeMouseless */
					if (sem_action == ACTION_RESUME_MOUSELESS) {
						suspend_mouseless = FALSE;
						*pwindow = 0;
						return (TRUE);
					}
					/* -- Priority 3: mouseless is suspended */
					if (suspend_mouseless)
						sem_action = ACTION_NULL_EVENT;
					/* -- Priority 4: QuoteNextKey or SuspendMouseless */
					if (sem_action == ACTION_QUOTE_NEXT_KEY) {
						quote_next_key = TRUE;
						*pwindow = 0;
						return (TRUE);
					}
					else if (sem_action == ACTION_SUSPEND_MOUSELESS) {
						suspend_mouseless = TRUE;
						*pwindow = 0;
						return (TRUE);
					}
				}
				if (!status ||

#ifdef OW_I18N
						((ret_status == XLookupNone) && (sem_action == ACTION_NULL_EVENT)))
#else
						((ksym == NoSymbol) && (sem_action == ACTION_NULL_EVENT)))
#endif

				{
					*pwindow = 0;
					return (TRUE);
				}
				/*
				 * Make sure the keystroke is sent to the appropriate window.  In
				 * the X input model, keystrokes are sent to the outmost (leaf)
				 * window even it is not the focus window.  To correct this
				 * behavior, we search up the tree for a parent that has the
				 * focus and redirect the keystroke to it.
				 */
				if (!xv_has_focus(info)) {
					int found_focuswindow = FALSE;
					Xv_object dummy_window = window;

					while ((window = xv_get(window, WIN_PARENT))) {
						Xv_Drawable_info *draw_info;

#ifdef BEFORE_DRA_CHANGED_IT
#else /* BEFORE_DRA_CHANGED_IT */
						/* window might be a SCREEN !!!!? */
						if (!xv_get(window, XV_IS_SUBTYPE_OF, WINDOW)) {
							break;
						}
#endif /* BEFORE_DRA_CHANGED_IT */

						DRAWABLE_INFO_MACRO(window, draw_info);
						if (xv_has_focus(draw_info)) {
							found_focuswindow = TRUE;
							event_set_window(event, window);
							break;
						}
					}
					if ((!found_focuswindow) && (sem_action == ACTION_HELP)) {
						Inputmask *im;

						window = dummy_window;

						for (;;) {
							if (!window) {
								*pwindow = 0;
								return (TRUE);
							}
							im = (Inputmask *) xv_get(window, WIN_INPUT_MASK);
							if ((im)
									&& (im->
										   im_flags & (IM_ASCII | IM_NEGASCII)))
							{
								event_set_window(event, window);
								break;
							}
							window = xv_get(window, WIN_PARENT);
						}
						/* If we didn't find the focus window, send it to its original
						   * destination.  The focus might have been lost, but due
						   * to a grab, we still get the key events. 
						 */
					}
					else if (!found_focuswindow)
						window = dummy_window;
				}
				server_set_timestamp(srv, &event->ie_time, ek->time);
				server_set_seln_function_pending(srv,
						(int)(ek->state & quick_modmask));
				event_set_x(event, ek->x);
				event_set_y(event, ek->y);

				/*
				 * If more than one character was returned tell the client that
				 * there is a string in event_string().  Can use
				 * event_is_string() to check.
				 */

#ifdef OW_I18N
				/*
				 * only when buf_length>1 then put the string in ie_string,
				 * else ie_code suffices to store it
				 */
				if ((buf_length > 1) &&
						((ret_status == XLookupBoth)
				|| (ret_status == XLookupChars))) event->ie_string =
							buffer_backup;
				else
					event->ie_string = NULL;
#else
				event_set_string(event, (buf_length > 1) ? buffer : NULL);
#endif

				event_set_action(event, sem_action);

				if ((sem_action == ACTION_PASTE
					|| sem_action == ACTION_CUT
					|| sem_action == ACTION_PROPS) &&
						(event_is_down(event)))
					win_handle_quick_selection(info, event);

				if (win_check_lang_mode(srv, display, event)) {
					*pwindow = XV_NULL;
					return 1;
				}
				break;
			}

		case ButtonPress:
		case ButtonRelease:
			{
				int button, action;
				XButtonEvent *e = (XButtonEvent *) xevent;
				Window_info *win;

				/*
				 * If the event is mouse-press and it is a synthetic event ( the
				 * event came from SendEvent request ), we should ignore it for
				 * security reasons.
				 */
				if (xevent->xany.send_event && !defeat_event_security) {
					*pwindow = XV_NULL;
					return 1;
				}

				server_set_timestamp(srv, &event->ie_time, e->time);
				temp = e->state;
				server_set_seln_function_pending(srv,
						(int)(e->state & quick_modmask));
				event_set_x(event, e->x);
				event_set_y(event, e->y);

				/* The configuration of mouse buttons is done (in the
				 * Workspace Properties) using XSetPointerMapping().
				 * So, here we may assume that MENU is always Button3 etc.
				 */
				switch (e->button) {
					case Button3:
						button = MS_RIGHT;
						action = ACTION_MENU;
						break;
					case Button2:
						/*
						 * if mouse chording is on simulate MENU by chording SELECT
						 * and ADJUST.
						 */
						if (chord) {
							if (chording(display, e, chording_timeout)) {
								button = MS_RIGHT;
								action = ACTION_MENU;
								temp = (temp | Button3Mask) & ~Button1Mask;
								menu_flag = 1;
							}
							else {
								button = MS_MIDDLE;
								action = ACTION_ADJUST;
								menu_flag = 0;
							}
						}
						else {
							/*
							 * OL says on two button mice, the right button is the
							 * menu button.
							 */
							if (nbuttons == 2) {
								button = MS_RIGHT;
								action = ACTION_MENU;
								temp = (temp | Button3Mask) & ~Button2Mask;
							}
							else {
								button = MS_MIDDLE;
								action = ACTION_ADJUST;
							}
						}

						break;
					case Button1:
						if (chord) {
							if (chording(display, e, chording_timeout)) {
								button = MS_RIGHT;
								action = ACTION_MENU;
								temp = (temp | Button2Mask) & ~Button1Mask;
								menu_flag = 1;
							}
							else {
								button = MS_LEFT;
								action = ACTION_SELECT;
								menu_flag = 0;
							}
						}
						else {
							if ((but2_mod & e->state) &&
									((nbuttons == 2) || (nbuttons == 1))) {
								button = MS_MIDDLE;
								action = ACTION_ADJUST;
								temp = (temp | Button2Mask) & ~Button1Mask;
							}
							else if ((but3_mod & e->state) && (nbuttons == 1)) {
								button = MS_RIGHT;
								action = ACTION_MENU;
								temp = (temp | Button3Mask) & ~Button1Mask;
							}
							else {
								button = MS_LEFT;
								action = ACTION_SELECT;
							}
						}
						break;

					/* support for mouse wheels */
					case Button4:
						button = BUT(4);
/* 						action = ACTION_SCROLL_UP; */
						action = ACTION_WHEEL_FORWARD;
						break;

					case Button5:
						button = BUT(5);
/* 						action = ACTION_SCROLL_DOWN; */
						action = ACTION_WHEEL_BACKWARD;
						break;

					default:
						*pwindow = XV_NULL;
						return 1;
				}
				SET_SHIFTS(event, temp, meta_modmask, alt_modmask);
				event_set_id(event, button);
				event_set_action(event, action);
				if (action == ACTION_ADJUST
					&& xv_get(srv, SERVER_EVENT_HAS_PRIMARY_PASTE_MODIFIERS, event))
				{
					action = ACTION_PASTE_PRIMARY;
					event_set_action(event, action);
				}

				if (event_type == ButtonPress) {
					event_set_down(event);
				}
				else {
					event_set_up(event);
				}

				win = WIN_PRIVATE(event_window(event));
				/*
				 * For click to type, if pressed SELECT, window does not appear
				 * to have input focus, window appears to be capable of accepting
				 * focus (i.e. accepts KBD_USE), and no selection function is
				 * pending, set input focus to this window.
				 */
				if (action == ACTION_SELECT &&
						event_type == ButtonPress &&
						!xv_has_focus(info) &&
						!xv_no_focus(info) &&
						(win->xmask & FocusChangeMask) &&
						!server_get_seln_function_pending(srv)) {
					(void)win_set_kbd_focus(window, xv_xid(info));
				}
				if (server_get_seln_function_pending(srv))
					win_handle_quick_selection(info, event);
				break;
			}

		case MotionNotify:
			{
				XMotionEvent *e = (XMotionEvent *) xevent;

				if ((int)xv_get(window, WIN_COLLAPSE_MOTION_EVENTS)) {
					/* collapse motion events already in queue */

					XEvent new_event;

					while (QLength(display)) {
						XNextEvent(display, &new_event);
						if (new_event.type == MotionNotify) {
							*xevent = new_event;
							e = (XMotionEvent *) xevent;
						}
						else {
							XPutBackEvent(display, &new_event);
							break;
						}
					}
				}

				server_set_timestamp(srv, &event->ie_time, e->time);

				event_set_x(event, e->x);
				event_set_y(event, e->y);

				temp = e->state;
				if (menu_flag)
					temp = (temp | Button3Mask);
				SET_SHIFTS(event, temp, meta_modmask, alt_modmask);

				if (event_button_is_down(event)) {
					if (nbuttons == 2) {
						if ((but3_mod & temp) && (temp & Button1Mask))
							temp = (temp | Button2Mask) & ~Button1Mask;
						else if (temp & Button2Mask)
							temp = (temp | Button3Mask) & ~Button2Mask;
					}
					else if (nbuttons == 1) {	/* assume it is button 1 */
						if (but3_mod & temp)
							temp = (temp | Button3Mask) & ~Button1Mask;
						else if (but2_mod & temp)
							temp = (temp | Button2Mask) & ~Button1Mask;
					}
					event_set_id(event, LOC_DRAG);
					event_set_down(event);
					if (server_get_seln_function_pending(srv))
						win_handle_quick_selection(info, event);
				}
				else {
					event_set_id(event, LOC_MOVE);
					event_set_up(event);
				}
				SET_SHIFTS(event, temp, meta_modmask, alt_modmask);
				break;
			}

		case EnterNotify:
		case LeaveNotify:{
				XCrossingEvent *e = (XCrossingEvent *) xevent;
				Xv_Drawable_info *info;

				server_set_timestamp(srv, &event->ie_time, e->time);

				event_set_down(event);
				temp = e->state;
				SET_SHIFTS(event, temp, meta_modmask, alt_modmask);
				server_set_seln_function_pending(srv,
						(int)(e->state & quick_modmask));
				event_set_x(event, e->x);
				event_set_y(event, e->y);
				DRAWABLE_INFO_MACRO(event_window(event), info);
				if (event_type == EnterNotify) {
					event_set_id(event, LOC_WINENTER);
					pointer_window_xid = xv_xid(info);
				}
				else {
					event_set_id(event, LOC_WINEXIT);
				}
				if (server_get_seln_function_pending(srv))
					win_handle_quick_selection(info, event);
				break;
			}

		case ConfigureNotify:{
				Rect temp_rect1;

				temp_rect1.r_width = xevent->xconfigure.width;
				temp_rect1.r_height = xevent->xconfigure.height;
				temp_rect1.r_left = xevent->xconfigure.x;
				temp_rect1.r_top = xevent->xconfigure.y;

				if ((Bool) xv_get(window, WIN_TOP_LEVEL) == TRUE)
					window_update_cache_rect(window, &temp_rect1);

				event_set_id(event, WIN_RESIZE);
				break;
			}
			/*
			 * BUG ALERT:to this correctly need tos store XID of parent instead
			 * of Window handle (or both)
			 */
		case ReparentNotify:{
				XReparentEvent *e = (XReparentEvent *) xevent;
				Xv_Window parent;

				if ((parent = win_data(display, e->parent)))
					window_set_parent(window, parent);

				event_set_id(event, WIN_REPARENT_NOTIFY);
				event_set_x(event, e->x);
				event_set_y(event, e->y);
			}
			break;

		case MapNotify:
			if ((Bool) xv_get(window, WIN_TOP_LEVEL) &&
					!(Bool) xv_get(window, WIN_TOP_LEVEL_NO_DECOR)) {
				event_set_id(event, WIN_MAP_NOTIFY);	/* id must be set before action */
				event_set_action(event, ACTION_OPEN);
			}
			else
				event_set_id(event, WIN_MAP_NOTIFY);
			break;

		case UnmapNotify:
			if ((Bool) xv_get(window, WIN_TOP_LEVEL) &&
					!(Bool) xv_get(window, WIN_TOP_LEVEL_NO_DECOR)) {
				event_set_id(event, WIN_UNMAP_NOTIFY);	/* id must be set before action */
				event_set_action(event, ACTION_CLOSE);
			}
			else
				event_set_id(event, WIN_UNMAP_NOTIFY);
			break;

			/*
			 * BUG ALERT: this is not exactly correct should really translate
			 * GraphicsExpose to Expose events to be exactly correct
			 */
		case GraphicsExpose:
		case Expose:
			{
				XExposeEvent *e = (XExposeEvent *) xevent;
				Window_info *win = WIN_PRIVATE(window);

				if (win_do_expose_event(display, event, e, &window,
								win->collapse_exposures)) {
					*pwindow = 0;
					return (1);
				}
				if (event_type == Expose)
					event_set_id(event, WIN_REPAINT);
				else
					event_set_id(event, WIN_GRAPHICS_EXPOSE);
			}
			break;

		case NoExpose:
			event_set_id(event, WIN_NO_EXPOSE);
			break;

		case SelectionClear:
			event_set_id(event, SEL_CLEAR);
			break;
		case SelectionRequest:
			event_set_id(event, SEL_REQUEST);
			break;
		case SelectionNotify:
			event_set_id(event, SEL_NOTIFY);
			break;
		case ClientMessage:{
				XClientMessageEvent *cme = (XClientMessageEvent *) xevent;

				if (process_clientmessage_events(window, cme, event)) {
					*pwindow = 0;
					return (1);
				}
#ifdef NO_XDND
#else /* NO_XDND */
				/* XdndDrop will be received on the frame, but we want
				 * it to be dispatched to something else, and this all
				 * has happened in process_clientmessage_events
				 */
				window = event_window(event);
#endif /* NO_XDND */
				break;
			}

		case PropertyNotify:{
				XPropertyEvent *pne = (XPropertyEvent *) xevent;

				if (srv) server_set_timestamp(srv, NULL, pne->time);
				if (!xv_sel_handle_property_notify(pne)) {
					if (process_property_events(window, pne, event)) {
						*pwindow = 0;
						return (1);
					}
				}
				break;
			}

		case FocusIn:{
				XFocusChangeEvent *fce = (XFocusChangeEvent *) xevent;
				static char *focdet[] = {
					"Ancestor",
					"Virtual",
					"Inferior",
					"Nonlinear",
					"NonlinearVirtual",
					"Pointer",
					"PointerRoot",
					"DetailNone"
				};

				SERVERTRACE((444, "FocusIn for %lx, detail=%d = %s\n",
							fce->window, fce->detail, focdet[fce->detail]));

				if (fce->detail == NotifyAncestor ||
						fce->detail == NotifyInferior ||
						fce->detail == NotifyNonlinear) {
					if (xv_get(xv_server(info), SERVER_JOURNALLING))
						xv_set(xv_server(info), SERVER_JOURNAL_SYNC_EVENT, 1,
								NULL);
					xv_set_has_focus(info, TRUE);
					event_set_id(event, KBD_USE);
				}
				else {
					/*
					 * BUG: We are dropping Notify{Virtual, NonlinearVirt, pntr,
					 * etc} on the floor.
					 */
					*pwindow = 0;
					return (1);
				}
				break;
			}

		case FocusOut:{
				XFocusChangeEvent *fce = (XFocusChangeEvent *) xevent;
				Frame frame = xv_get(event_window(event), WIN_FRAME);

				/* On FocusOut events turn off the compose LED */
				if (xv_get(frame, FRAME_COMPOSE_STATE)) {
					xv_set((Frame) xv_get(event_window(event), WIN_FRAME),
							FRAME_COMPOSE_STATE, False, NULL);
					compose_status->chars_matched = 0;
				}

				if (fce->detail == NotifyAncestor ||
						fce->detail == NotifyInferior ||
						fce->detail == NotifyNonlinear) {
					xv_set_has_focus(info, FALSE);
					event_set_id(event, KBD_DONE);
				}
				else {
					*pwindow = 0;
					return (1);
				}
				break;
			}
		case KeymapNotify:
			event_set_id(event, KBD_MAP);
			break;
		case VisibilityNotify:{
				XVisibilityEvent *e = (XVisibilityEvent *) xevent;

				event_set_id(event, WIN_VISIBILITY_NOTIFY);
				event_set_flags(event, e->state);	/* VisibilityUnobscured,
													   * VisibilityPartiallyObscured
													   * VisibilityObscured
													 */
				break;
			}
		case GravityNotify:{
				XGravityEvent *e = (XGravityEvent *) xevent;

				event_set_id(event, WIN_GRAVITY_NOTIFY);
				event_set_x(event, e->x);
				event_set_y(event, e->y);
				break;
			}
		case CirculateNotify:{
				XCirculateEvent *e = (XCirculateEvent *) xevent;

				event_set_id(event, WIN_CIRCULATE_NOTIFY);
				event_set_flags(event, e->place);	/* PlaceOnTop or
													   * PlaceOnButton */
				break;
			}
		case ColormapNotify:
			event_set_id(event, WIN_COLORMAP_NOTIFY);
			/*
			 * BUG: This needs support macros to allow user to get at the
			 * colormap, new and state fields in the xevent.
			 */
			break;
			/* Events a window manger (not toolkit) would be interested in.   */
		case CreateNotify:
			event_set_id(event, WIN_CREATE_NOTIFY);
			break;
		case DestroyNotify:
			event_set_id(event, WIN_DESTROY_NOTIFY);
			break;
		case MapRequest:
			event_set_id(event, WIN_MAP_REQUEST);
			break;
		case ResizeRequest:
			event_set_id(event, WIN_RESIZE_REQUEST);
			break;
		case ConfigureRequest:
			event_set_id(event, WIN_CONFIGURE_REQUEST);
			break;
		case CirculateRequest:
			event_set_id(event, WIN_CIRCULATE_REQUEST);
			break;

			/*
			 * If we get an event that is not defined in the X protocol, assume
			 * it is an event generated from a server extension.  If an extension
			 * proc has been set on this server, we will call it back with the
			 * display, extension event, and a window object that is possibly
			 * NULL.
			 */
		default:
			{
				typedef void (*extproc_t)(Display *,XEvent *, Xv_window);
				/*
				 * I would like to cache the extensionProc, but then we run into
				 * the problem where the extensionProc is sporatically changed
				 * by the programmer.
				 */
				extproc_t extensionProc = (extproc_t)xv_get(srv,
												SERVER_EXTENSION_PROC);

				if (extensionProc)
					(*extensionProc)(display, xevent, window);
			}
			*pwindow = XV_NULL;
			return 1;
	}

	*pwindow = window;
	return (0);
}

/* read the input and post to the proper window */
Xv_private Notify_value xv_input_pending(Display *dpy, int unused_fd)
{
	Event event;
	int events_handled = QLength(dpy);
	Xv_object window;
	XEvent xevent;

	event_init(&event);

	/* If there are no events on Xlib's internal queue, then
	 * flush and go to the wire for them.
	 */
	if (!events_handled) {
		events_handled += XPending(dpy);
		/* If we still don't have any events on the queue, then the connection
		 * might have been dropped.  To test for this, we ping the server.
		 */
		if (!events_handled)
			/* XSync will not return if the connection has been lost. */
			XSync(dpy, False);
	}

	while (QLength(dpy)) {
		window = xview_x_input_readevent(dpy, &event, XV_NULL, FALSE, FALSE,
				0, &xevent);

		if (window)
			switch (event_id(&event)) {

				case WIN_GRAPHICS_EXPOSE:
				case WIN_REPAINT:{
						/* Ref (lkhregfvhkwberf) */
						if ((!xv_get(window, WIN_X_PAINT_WINDOW)) &&
								(!xv_get(window, WIN_NO_CLIPPING))) {
							/*
							 * Force the clipping list for the window to be the
							 * damage list while the client is processing the
							 * WIN_REPAINT, then clear the clipping list back to
							 * its normal state.
							 */
							Rectlist *rl;

							rl = win_get_damage(window);
							win_set_clip(window, rl);
							(void)win_post_event(window, &event,
									WIN_IS_IN_LOOP ? NOTIFY_IMMEDIATE :
									NOTIFY_SAFE);
							win_set_clip(window, RECTLIST_NULL);
						}
						else {
							(void)win_post_event(window, &event,
									WIN_IS_IN_LOOP ? NOTIFY_IMMEDIATE :
									NOTIFY_SAFE);
						}
						win_clear_damage(window);
						break;
					}
				case MS_LEFT:{
						Window_info *win = WIN_PRIVATE(window);

						/*
						 * Since we have a synchronous grab on the select button,
						 * after we set the focus, we need to release the events
						 * with XAllowEvents().
						 */
						window_release_selectbutton(window, &event);

						/*
						 * Do not post MS_LEFT events if the user hasn't asked
						 * for them.  XView needs them for click-to-type and we
						 * get them reguardless of whether they are in the event
						 * mask beacuse of the passive button grab.
						 */
						if ((win->xmask & ButtonPressMask
										&& event_is_down(&event))
								|| (win->xmask & ButtonReleaseMask
										&& event_is_up(&event)))
							(void)win_post_event(window, &event,
									WIN_IS_IN_LOOP ? NOTIFY_IMMEDIATE :
									NOTIFY_SAFE);

						break;
					}

				default:
					if (event_action(&event) == ACTION_ACCELERATOR) {
						/* ACC_XVIEW */
						/*
						 * First check if the event is a menu accelerator.
						 * If it is, call the proper notify proc.
						 * If not, check if it is a window accelerator.
						 * If it is, call the proper notify proc.
						 * If not, nullify the semantic action and post the event
						 */
						if (!win_handle_menu_accel(&event)) {
							if (!win_handle_window_accel(&event)) {
								event_set_action(&event, ACTION_NULL_EVENT);
								win_post_event(window, &event,
										WIN_IS_IN_LOOP ? NOTIFY_IMMEDIATE :
										NOTIFY_SAFE);
							}
						}
						/* ACC_XVIEW */
					}
					else {
						win_post_event(window, &event,
								WIN_IS_IN_LOOP ? NOTIFY_IMMEDIATE :
								NOTIFY_SAFE);
					}
					break;
			}

		/* If we've handled all the events on the wire and we've not handled */
		/* more than 25 events on this go-round (as we don't want to deprive */
		/* other input src's [eg. another server]), see if any more events   */
		/* have arrived.                                 */
		if (!QLength(dpy) && (events_handled < 25))
			events_handled += XEventsQueued(dpy, QueuedAfterFlush);
	};
	return NOTIFY_DONE;
}

/* ACC_XVIEW */
/*
 * Returns TRUE if the window accelerator was found/handled
 */
static int win_handle_window_accel(Event *event)
{
    XEvent	*xevent = event_xevent(event);
    XKeyEvent	*ek = (XKeyEvent *) xevent;
    Frame	frame;
    Frame_accelerator *accelerator;
    KeySym          keysym;

    keysym = xkctks(ek->display, ek->keycode, 0);
    frame = xv_get(event_window(event), WIN_FRAME);
    accelerator = (Frame_accelerator *) xv_get(frame,
                            FRAME_ACCELERATOR, event_id(event), keysym);
    if (accelerator) {
        accelerator->notify_proc(accelerator->data, event);
        return(TRUE);
    }

    return(FALSE);
}

/*
 * Returns TRUE if the menu accelerator was found/handled
 * The keysym passed in is unmodified i.e. entry 0 in the 
 * keysym list
 */
static int win_handle_menu_accel(Event *event)
{
    XEvent	*xevent = event_xevent(event);
    XKeyEvent	*ek = (XKeyEvent *) xevent;
    Frame	frame;
    Frame_menu_accelerator *menu_accelerator;

    frame = xv_get(event_window(event), WIN_FRAME);
    menu_accelerator = (Frame_menu_accelerator *) xv_get(frame,
                    FRAME_MENU_X_ACCELERATOR, 
			ek->keycode, ek->state, NoSymbol);

    if (menu_accelerator) {
        /*
         * We consume both the up/down event but the notify
         * proc is called only for the down
         */
        if (event_is_down(event))  {
            menu_accelerator->notify_proc(menu_accelerator->data, event);
        }
        return(TRUE);
    }
    return (FALSE);
}
/* ACC_XVIEW */

static int want_size(Frame win)
{
	int force_size = defaults_get_boolean("window.wmCommand.forceSize",
							"Window.WMCommand.ForceSize", False);
	if (force_size) return TRUE;

	if (xv_get(win, FRAME_SHOW_RESIZE_CORNER)) return TRUE;

	/* has no resize corners - let the program determine its size
	 * and do **not** use -Ws to force the size
	 */
	return FALSE;
}

Xv_private void win_get_cmdline_option(Xv_object window, char *str,
											char *appl_cmdline)
{
	Window root = 0, parent, *children;
	unsigned int nchildren;
	int temp, icon_x, icon_y;
	Rect *rect;
	char iconic[6];
	Icon icon;
	XWindowAttributes xwin_attr;
	Xv_Drawable_info *icon_info, *info;

	DRAWABLE_INFO_MACRO(window, info);
	rect = (Rect *) xv_get(window, WIN_RECT);
	if (xv_get(window, XV_SHOW)) {
		temp = XQueryTree(xv_display(info), xv_xid(info), &root, &parent,
				&children, &nchildren);
		if (temp) {
			XGetWindowAttributes(xv_display(info), parent, &xwin_attr);
			if (nchildren)
				XFree((char *)children);
		}
	}
	else
		XGetWindowAttributes(xv_display(info), xv_xid(info), &xwin_attr);

	rect->r_left = xwin_attr.x;
	rect->r_top = xwin_attr.y;

	icon = (Icon) xv_get(window, FRAME_ICON);
	DRAWABLE_INFO_MACRO(icon, icon_info);
	if (!root)
		root = (int)xv_get(xv_root(icon_info), XV_XID);

	win_translate_xy_internal(xv_display(info), xv_xid(icon_info), root, 0, 0,
			&icon_x, &icon_y);

	iconic[0] = '\0';
	if (xv_get(window, FRAME_CLOSED))
		sprintf(iconic, " -Wi");
	else
		sprintf(iconic, " +Wi");

	/*
	 * Create the string with:
	 *  APP_NAME -Wp WINDOW_X WINDOW_Y -Ws WINDOW_WIDTH WINDOW_HEIGHT 
	 *  -WP ICON_X ICON_Y
	 */
	if (want_size(window)) {
		sprintf(str,
				"%s -Wp %d %d -Ws %d %d -WP %d %d %s", xv_app_name,
				rect->r_left, rect->r_top,
				rect->r_width, rect->r_height, icon_x, icon_y, iconic);
	}
	else {
		sprintf(str,
				"%s -Wp %d %d -WP %d %d %s", xv_app_name,
				rect->r_left, rect->r_top, icon_x, icon_y, iconic);
	}
	/*
	 * Append to str all other XView cmd line options
	 */
	xv_get_cmdline_str(str);

	/*
	 * add any application specific cmdline args
	 */
	if (appl_cmdline) {
		/* put in a space to separate from rest of string */
		strcat(str, " ");
		strcat(str, appl_cmdline);
	}
}


Xv_private void win_set_wm_command_prop(Xv_object window, char **argv,
							char **appl_cmdline_argv, int appl_cmdline_argc)
{
	Xv_Drawable_info *info;
	Window root = 0;
	Window parent, *children;
	unsigned int nchildren;
	int temp;
	XWindowAttributes xwin_attr;
	Rect *rect;
	char icon_x_str[50], icon_y_str[50];
	char window_width[50], window_height[50];
	char window_x[50], window_y[50];
	Icon icon;
	Xv_Drawable_info *icon_info;
	int icon_x, icon_y;
	int argc = 0;

	argv[argc++] = xv_app_name;

	DRAWABLE_INFO_MACRO(window, info);
	rect = (Rect *) xv_get(window, WIN_RECT);

	if (xv_get(window, XV_SHOW)) {
		temp = XQueryTree(xv_display(info), xv_xid(info), &root, &parent,
				&children, &nchildren);
		if (temp) {
			XGetWindowAttributes(xv_display(info), parent, &xwin_attr);
			if (nchildren)
				XFree((char *)children);
		}
	}
	else
		XGetWindowAttributes(xv_display(info), xv_xid(info), &xwin_attr);

	/*
	 * Window position
	 */
	window_x[0] = window_y[0] = '\0';
	sprintf(window_x, "%d", xwin_attr.x);
	sprintf(window_y, "%d", xwin_attr.y);
	argv[argc++] = "-Wp";
	argv[argc++] = window_x;
	argv[argc++] = window_y;

	/*
	 * Put size in size string, if valid rect returned
	 */
	if (rect && want_size(window)) {
		window_width[0] = window_height[0] = '\0';
		sprintf(window_width, "%d", rect->r_width);
		sprintf(window_height, "%d", rect->r_height);
		argv[argc++] = "-Ws";
		argv[argc++] = window_width;
		argv[argc++] = window_height;
	}

	icon = (Icon) xv_get(window, FRAME_ICON);

	/*
	 * Put icon position in icon position string if frame icon present
	 */
	if (icon) {
		DRAWABLE_INFO_MACRO(icon, icon_info);
		if (!root)
			root = (int)xv_get(xv_root(icon_info), XV_XID);

		win_translate_xy_internal(xv_display(info), xv_xid(icon_info), root, 0,
				0, &icon_x, &icon_y);

		icon_x_str[0] = icon_y_str[0] = '\0';
		sprintf(icon_x_str, "%d", icon_x);
		sprintf(icon_y_str, "%d", icon_y);

		argv[argc++] = "-WP";
		argv[argc++] = icon_x_str;
		argv[argc++] = icon_y_str;
	}

	if (xv_get(window, FRAME_CLOSED)) {
		argv[argc++] = "-Wi";
	}
	else {
		argv[argc++] = "+Wi";
	}

	/*
	 * Append to str all other XView cmd line options
	 */
	xv_get_cmdline_argv(argv, &argc);

	if (appl_cmdline_argv) {
		int i = 0;

		for (i = 0; i < appl_cmdline_argc; ++i) {
			argv[argc++] = appl_cmdline_argv[i];
		}
	}

	XSetCommand(xv_display(info), xv_xid(info), argv, argc);
}

static void initialize_ol_trans_utf8(char **utf, char *utfbuf)
{
	iconv_t ic;
	int i;
	char *p, latbuf[2], *lat;
	size_t insiz, outsiz;

	/* actually, this is only needed in a UTF-8 locale.... */
	 if (! _xv_is_multibyte) return;
	if (utf[0] != NULL) return;

	ic = iconv_open("UTF8", "LATIN1");

	p = utfbuf;
	latbuf[1] = '\0';
	for (i = 0; i < 128; i++) {
		utf[i] = p;
		latbuf[1] = i + 128;
		insiz = 1;
		lat = latbuf;
		outsiz = 4;
		iconv(ic, &lat, &insiz, &p, &outsiz);
		*p++ = '\0';
	}

	iconv_close(ic);
}

static int process_clientmessage_events(Xv_object window,
						XClientMessageEvent *clientmessage, Event *event)
{
	static char *ol_trans_utf[128] = { NULL };
	static char ol_trans_utfbuf[1024];
	Xv_Drawable_info *info;
	Xv_opaque server_public;
	Server_atom_type atom_type;
	Bool keyboard_key;
	unsigned int key_value;
	unsigned int *key_map;
	static u_char *ascii_sem_map, *key_sem_maps[__SEM_INDEX_LAST];
	KeySym ksym, sem_ksym;
	int modifiers = 0;
	int sem_action;

	DRAWABLE_INFO_MACRO(window, info);
	server_public = xv_server(info);
	atom_type = server_get_atom_type(server_public,
											 clientmessage->message_type);

	switch (atom_type) {
		case SERVER_WM_DISMISS_TYPE:
			event_set_action(event, ACTION_DISMISS);
			break;
		case SERVER_DO_DRAG_MOVE_TYPE:
		case SERVER_DO_DRAG_COPY_TYPE:
		case SERVER_DO_DRAG_LOAD_TYPE:
			{
				int final_x, final_y;

				/*
				 * the xy is in the sender's coordinate, need to translate it
				 */
				(void)win_translate_xy_internal(xv_display(info),
												(XID) clientmessage->data.l[3],
												xv_xid(info),
												(int)clientmessage->data.l[1],
												(int)clientmessage->data.l[2],
												&final_x, &final_y);
				event_set_x(event, final_x);
				event_set_y(event, final_y);

				/*
				 * save off the clientmessage info into the window struct
				 */
				window_set_client_message(window, clientmessage);

				switch (atom_type) {
					case SERVER_DO_DRAG_MOVE_TYPE:
						event_set_action(event, ACTION_DRAG_MOVE);
						break;
					case SERVER_DO_DRAG_COPY_TYPE:
						event_set_action(event, ACTION_DRAG_COPY);
						break;
					case SERVER_DO_DRAG_LOAD_TYPE:
						event_set_action(event, ACTION_DRAG_LOAD);
						break;
					default:
						break;
				}
			}
			break;
		case SERVER_WM_DRAGDROP_TRIGGER_TYPE:
			{
				int actual_x, actual_y, msg_x, msg_y;

				/* Decode the x, y position in top level coord space */
				msg_x = (clientmessage->data.l[2] >> 16) & 0xffff;
				msg_y = clientmessage->data.l[2] & 0xffff;

				(void)win_translate_xy_internal(xv_display(info),
												xv_get(xv_root(info), XV_XID),
												xv_xid(info), msg_x, msg_y,
												&actual_x, &actual_y);
				event_set_x(event, actual_x);
				event_set_y(event, actual_y);

				/* save off the clientmessage info into the window struct */
				window_set_client_message(window, clientmessage);

				/* Set the time of the trigger event */
				server_set_timestamp(server_public, &event->ie_time,
								 (unsigned long)clientmessage->data.l[1]);

				if (clientmessage->data.l[4] & DND_MOVE_FLAG)
					event_set_action(event, ACTION_DRAG_MOVE);
				else
					event_set_action(event, ACTION_DRAG_COPY);

				/* If the event has been forwarded from some other site,
				 * inform the client.
				 */
				if (clientmessage->data.l[4] & DND_FORWARDED_FLAG)
					event_set_flags(event, DND_FORWARDED);
			}
			break;
		case SERVER_WM_DRAGDROP_PREVIEW_TYPE:
			{
				/* hier will ich den DND_FORWARDED-Sachverhalt - der nur ein
				 * Bit braucht - in data.l[2] integrieren:
				 * 1111111111111111 1 111111111111111
				 * x_root           ^ y_root
				 *                  |
				 *                  Forwarded
				 *
				 * Wenn das ausgiebig getestet ist, wird data.l[4] frei
				 * und ich kann das 'Sender-Window' reinschreiben und darauf
				 * analog XDnd ein Property mit den TARGETS schreiben
				 * oder mit dnd_preview_reject reagieren.
				 *
				 * Reference (erfihlwebfrygjbv)
				 */
				int actual_x, actual_y, msg_x, msg_y;
				unsigned event_forwarded = 0;

				/* check whether this is a **new** preview event:
				 * old events had data.l[4] == 0 or == DND_FORWARDED_FLAG;
				 */
				if (clientmessage->data.l[4] == 0
					|| clientmessage->data.l[4] == DND_FORWARDED_FLAG)
				{	
					/* old preview */
					/* Decode the x, y position in top level coord space */
					msg_x = (clientmessage->data.l[2] >> 16) & 0xffff;
					msg_y = clientmessage->data.l[2] & 0xffff;
					if (clientmessage->data.l[4] & DND_FORWARDED_FLAG)
						event_forwarded = DND_FORWARDED;
					}
				else {
					/* new preview */
					/* Decode the x, y position in top level coord space */
					msg_x = (clientmessage->data.l[2] >> 16) & 0xffff;
					msg_y = clientmessage->data.l[2] & 0x7fff;
					if ((clientmessage->data.l[2] & 0x8000) != 0) {
						event_forwarded = DND_FORWARDED;
					}
				}
				event_set_flags(event, event_forwarded);

				(void)win_translate_xy_internal(xv_display(info),
												xv_get(xv_root(info), XV_XID),
												xv_xid(info), msg_x, msg_y,
												&actual_x, &actual_y);
				event_set_x(event, actual_x);
				event_set_y(event, actual_y);

				/* save off the clientmessage info into the window struct */
				window_set_client_message(window, clientmessage);

				/* Set the time of the preview event */
				server_set_timestamp(server_public, &event->ie_time,
								 (unsigned long)clientmessage->data.l[1]);

				switch (clientmessage->data.l[0]) {
					case EnterNotify:
						event_set_id(event, LOC_WINENTER);
						break;
					case LeaveNotify:
						event_set_id(event, LOC_WINEXIT);
						break;
					case MotionNotify:
						event_set_id(event, LOC_DRAG);
						break;
					default:
						xv_error(event_window(event),
								ERROR_STRING, DND_ERROR_CODE,
								NULL);
				}
				event_set_action(event, ACTION_DRAG_PREVIEW);
			}
			break;

		case SERVER_WM_PROTOCOLS_TYPE:
			switch (server_get_atom_type(server_public,
										 (Atom)clientmessage->data.l[0])) {
				case SERVER_WM_SAVE_YOURSELF_TYPE:

					/* xv_destroy_save_yourself posts NOTIFY_SAFE -
					 * this might be the reason why setting FRAME_WM_COMMAND_*
					 * works on the second 'Save Workspace', but does NOT work on
					 * the first one.
					 */
					notify_post_destroy(window, DESTROY_SAVE_YOURSELF,
										NOTIFY_IMMEDIATE);

					win_set_wm_command(window);
					XFlush(xv_display(info));
					break;
				case SERVER_WM_DELETE_WINDOW_TYPE:
					if (xv_get(window, XV_OWNER) == xv_get(xv_screen(info),
														   XV_ROOT) &&
							((Xv_pkg *) xv_get(window, XV_TYPE) == FRAME))
						xv_destroy_safe(window);
					else
						event_set_action(event, ACTION_DISMISS);
					break;
				case SERVER_WM_TAKE_FOCUS_TYPE:
					server_set_timestamp(server_public, &event->ie_time,
								 (unsigned long)clientmessage->data.l[1]);
					event_set_action(event, ACTION_TAKE_FOCUS);
					break;
				default:
					event_set_id(event, WIN_CLIENT_MESSAGE);
					window_set_client_message(window, clientmessage);
					break;
			}
			break;
		default:
#ifdef NO_XDND
#else /* NO_XDND */
			/* The general idea of integration of Xdnd into our XView
			 * Drag&Drop word is to disguise Xdnd client messages as
			 * XView events (ACTION_DRAG_PREVIEW and ACTION_DRAG_COPY or
			 * ACTION_DRAG_MOVE) according to the registered drop sites
			 * and dispatch those events. So, for our applications 
			 * everything looks like a normal XView dnd operation.
			 * We have to remember that Xdnd events are always sent to
			 * top level windows = frames in XView - so, the parameter
			 * "window" we have here is always a frame.
			 */
			if (clientmessage->message_type ==
					xv_get(server_public, SERVER_ATOM, "XdndPosition")) {
				int site_hit = FALSE;
				int msg_x, msg_y;
		    	Window_info *framepriv = WIN_PRIVATE(window);
				Win_drop_site_list *winDropInterest;
				XClientMessageEvent cM;
				Xv_opaque last_dropped_site;
				Atom act;

    			/*
				 * data.l[0] source window
				 * data.l[1] is reserved for future use (flags).
				 * data.l[2] contains the coordinates of the mouse position
				 *           relative to the root window.
				 *           data.l[2] = (x << 16) | y;
				 * data.l[3] contains the time stamp 
				 * data.l[4] contains the action requested by the user.
				 */


				/* the position is in data.l[2] : (x << 16) | y in
				 * root window coordinates
				 */

				/* Decode the x, y position in top level coord space */
				framepriv->droppos = clientmessage->data.l[2];
				msg_x = (clientmessage->data.l[2] >> 16) & 0xffff;
				msg_y = clientmessage->data.l[2] & 0xffff;

				SERVERTRACE((TLXDND, "XdndPosition from %lx at (root) %d, %d\n",
									clientmessage->data.l[0], msg_x, msg_y));

				(void)win_translate_xy_internal(xv_display(info),
									xv_get(xv_root(info), XV_XID),
									xv_xid(info), msg_x, msg_y,
									&framepriv->drop_x, &framepriv->drop_y);

				/* Now drop_x, drop_y are in frame-coords */
				act = (Atom)clientmessage->data.l[4];

				if (act == (Atom)xv_get(server_public, SERVER_ATOM,
														"XdndActionMove"))
				{
					framepriv->dropaction = XDND_ACTION_MOVE;
				}
				else if (act == (Atom)xv_get(server_public, SERVER_ATOM,
														"XdndActionLink"))
				{
					framepriv->dropaction = XDND_ACTION_LINK;
				}
				else if (act == (Atom)xv_get(server_public, SERVER_ATOM,
														"XdndActionAsk"))
				{
					framepriv->dropaction = XDND_ACTION_ASK;
				}
				else if (act == (Atom)xv_get(server_public, SERVER_ATOM,
														"XdndActionPrivate"))
				{
					framepriv->dropaction = XDND_ACTION_PRIVATE;
				}
				else {
					framepriv->dropaction = XDND_ACTION_COPY;
				}

				last_dropped_site = framepriv->dropped_site;
				framepriv->dropped_site = XV_NULL;
				winDropInterest = framepriv->dropInterest;
				while ((winDropInterest = (Win_drop_site_list *)(XV_SL_SAFE_NEXT(winDropInterest))))
				{
					int found;

					found = DndSiteContains(winDropInterest->drop_item,
									framepriv->drop_x, framepriv->drop_y);

					if (found) {
						framepriv->dropped_site = winDropInterest->drop_item;
						/* we have found a matching drop site */
						site_hit = TRUE;
						break;
					}
				}

				/* now let's do what the Xdnd expects us: send an XdndStatus */
				cM.type = ClientMessage;
				cM.display = clientmessage->display;
				cM.format = 32;
				cM.message_type = xv_get(server_public, SERVER_ATOM,
															"XdndStatus");
				cM.window = clientmessage->data.l[0];
				cM.data.l[0] = clientmessage->window;
				cM.data.l[1] = (site_hit ? 1 : 0);
				cM.data.l[2] = 0;     /* unused rect */
				cM.data.l[3] = 0;     /* unused rect */
				/* we do not (and can not) tell what the application will do */
    			cM.data.l[4] = xv_get(server_public,
										SERVER_ATOM, "XdndActionPrivate");

				SERVERTRACE((TLXDND, "XdndStatus back to %lx\n", cM.window));
				DndSendEvent(cM.display, (XEvent *)&cM, "XDnd");

				/* now back to us: */
				if (site_hit) {

					/* now compare last_dropped_site and
					 * framepriv->dropped_site:
					 */
					if (last_dropped_site == framepriv->dropped_site) {
						/* if they are equal, we can dispatch this as a
						 * LOC_DRAG - preview event.
						 */
						prepare_drag_preview_from_xdnd(framepriv, info, window,
								framepriv->dropped_site, clientmessage, event,
								LOC_DRAG);
					}
					else {
						if (last_dropped_site) {
							Event art_ev;
							event_set_xevent(&art_ev, (XEvent *)clientmessage);
							prepare_drag_preview_from_xdnd(framepriv, info,
									window, last_dropped_site, clientmessage,
									&art_ev, LOC_WINEXIT);
							/* we don't want to remember for the next time */
		    				win_post_event(event_window(&art_ev), &art_ev,
												NOTIFY_IMMEDIATE);
						}
						prepare_drag_preview_from_xdnd(framepriv, info, window,
							framepriv->dropped_site, clientmessage, event,
							LOC_WINENTER);
					}
				}
				else {
					/* now look at last_dropped_site - if there was one,
					 * we obviously just left it, therefore we send it
					 * a LOC_WINEXIT preview event
					 */
					if (last_dropped_site != XV_NULL) {
						prepare_drag_preview_from_xdnd(framepriv, info, window,
								last_dropped_site, clientmessage, event,
								LOC_WINEXIT);
					}
					else {
						/* no hit, we want no further actions */
						/* as far as I understand it, this means
						 * 'no further dispatching of this event'
						 */
						return TRUE;
					}
				}
			}
			else if (clientmessage->message_type ==
					xv_get(server_public, SERVER_ATOM, "XdndEnter"))
			{
		    	Window_info *framepriv = WIN_PRIVATE(window);
				/* XdndEnter is pretty useless (for us) because it doesn't
				 * contain a position, so, we can't assign it to a drop site
				 * (window). It only means 'the frame has been entered'.
				 * [[[ For future use:
				 *    data.l[0]         source window
				 *    data.l[1]         bit0 = 'more than 3' high byte: version
				 *    data.l[2]         first type
				 *    data.l[3]         second type
				 *    data.l[4]         third type    ]]]
				 */

				SERVERTRACE((TLXDND, "dragger sends XdndEnter from %lx with %s, %s and %s\n",
						clientmessage->data.l[0],
						clientmessage->data.l[2] ? (char *)xv_get(server_public, SERVER_ATOM_NAME, clientmessage->data.l[2]) : "-nil-",
						clientmessage->data.l[3] ? (char *)xv_get(server_public, SERVER_ATOM_NAME, clientmessage->data.l[3]) : "-nil-",
						clientmessage->data.l[4] ? (char *)xv_get(server_public, SERVER_ATOM_NAME, clientmessage->data.l[4]) : "-nil-"));

				/* but we know the game must in any case start
				 * from the beginning
				 */
				framepriv->dropped_site = XV_NULL;

				if ((clientmessage->data.l[1] & 1) != 0) {
					Atom type;
					int format;
					unsigned long i, nitems;
					unsigned long bytes;
					unsigned char *prop;
					Atom *ato;

					/* der Arsch verspricht 'mehr als 3', gibt aber
					 * schon mal kein einziges Atom mit....
					 */
					if (XGetWindowProperty(xv_display(info),
						(Window)clientmessage->data.l[0],
						xv_get(server_public, SERVER_ATOM, "XdndTypeList"),
						0L, 9999L, False, AnyPropertyType,
						&type, &format, &nitems, &bytes, &prop) == Success)
					{
						ato = (Atom *)prop;
						for (i = 0; i < nitems; i++) {
							SERVERTRACE((TLXDND, "list[%ld] = '%s'\n", i,
								(char *)xv_get(server_public, SERVER_ATOM_NAME,
													ato[i])));
						}
					}
				}
			}
			else if (clientmessage->message_type ==
					xv_get(server_public, SERVER_ATOM, "XdndLeave"))
			{
		    	Window_info *framepriv = WIN_PRIVATE(window);
				/* XdndLeave is pretty useless because it doesn't contain
				 * a position, so, we can't assign it to a drop site (window).
				 * It only means 'the frame has been left'.
				 * [[[ For future use:
				 *    data.l[0]         source window  ]]]
				 */

				SERVERTRACE((TLXDND, "XdndLeave from %lx to %lx\n",
							clientmessage->data.l[0], clientmessage->window));
				if (framepriv->dropped_site) {
					prepare_drag_preview_from_xdnd(framepriv, info, window, 
							framepriv->dropped_site, clientmessage, event,
							LOC_WINEXIT);
				}
				/* but we can at least forget anything */
				framepriv->dropped_site = XV_NULL;
			}
			else if (clientmessage->message_type ==
					xv_get(server_public, SERVER_ATOM, "XdndDrop"))
			{
		    	Window_info *framepriv = WIN_PRIVATE(window);

				SERVERTRACE((TLXDND, "XdndDrop from %lx to %lx\n",
							clientmessage->data.l[0], clientmessage->window));
				if (! framepriv->dropped_site) return TRUE;
				xdnd_drop_received(window, server_public, info,
										clientmessage, event);
			}
			else if (clientmessage->message_type ==
					xv_get(server_public, SERVER_ATOM, "XdndFinished"))
			{
    			XClientMessageEvent origxcl;
				XSelectionRequestEvent *sreq =
									(XSelectionRequestEvent *)clientmessage;

				/* should we convert this to a SelectionRequest
				 * like _SUN_DRAGDROP_DONE ???
				 * I will rather issue a selection request for XdndFinished.
				 *
				 * I know here nothing about the sending window, but
				 * nobody "there" expects a SelectionNotify...
				 * All I want is a call to the xdnd's conversion proc
				 * so that it disowns the XdndSelection.
				 */
				origxcl = *clientmessage;
				SERVERTRACE((TLXDND, "XdndFinished to %lx\n",
							clientmessage->window));
				sreq->type = SelectionRequest;
				sreq->owner = origxcl.window;
				sreq->requestor = origxcl.window;
				sreq->selection = xv_get(server_public, SERVER_ATOM,
												"XdndSelection");
				sreq->target = origxcl.message_type;
				sreq->property = origxcl.message_type;
				sreq->time = server_get_timestamp(server_public);
				event_set_id(event, SEL_REQUEST);
			}
			else
#endif /* NO_XDND */
			/*
			 * Set the id to WIN_CLIENT_MESSAGE and the content of the gets
			 * stuffed into window struct
			 */

			if (clientmessage->message_type ==
					xv_get(server_public, SERVER_ATOM, "_OL_TRANSLATED_KEY"))
			{
				int meta_modmask = 0;
				int alt_modmask = 0;
				int i;

				initialize_ol_trans_utf8(ol_trans_utf, ol_trans_utfbuf);

				/* the format is 32 **always** even if vkbd is runnung on an
				 * old sun machine
				 */

				/* Initialise an xevent  */

				ksym = (KeySym) clientmessage->data.l[0];
				keyboard_key = ((ksym & KEYBOARD_KYSM_MASK) == KEYBOARD_KYSM);
				key_value = ksym;

				key_map = (unsigned int *)xv_get(server_public, SERVER_XV_MAP);
				for (i = 0; i < __SEM_INDEX_LAST; i++) {
					key_sem_maps[i] = (u_char *) xv_get(server_public,
										SERVER_SEMANTIC_MAP, i);
				}
				ascii_sem_map = (u_char *)xv_get(server_public, SERVER_ASCII_MAP);

				if (keyboard_key)
					key_value = ((key_map[(int)ksym & 0xFF] == ksym) ||
								 (!key_map[(int)ksym & 0xFF])) ? key_value :
										key_map[(int)ksym & 0xFF];

				/* basically, this is the keysym that was sent in data.l[0].
				 * In a single byte locale (let's say, a LATIN1 or similar
				 * locale) this keysym is in fact the "character itself"
				 * (e.g. XK_adiaeresis == 0x00e4).
				 * However, in a multibyte (utf8...) locale, all non-ASCII
				 * keysyms will be **two** bytes ....
				 * At the moment, I see iconv as the reasonable way.
				 * Please note that this is not depending on the DISPLAY,
				 */
				event_set_id(event, key_value);

				if ((int)(clientmessage->data.l[1]) == KeyPress)
					event_set_down(event);
				else if ((int)(clientmessage->data.l[1]) == KeyRelease)
					event_set_up(event);

				event->ie_win = window;
				/* dear people, this
				 * event->ie_string = XKeysymToString(ksym);
				 * creates strings like eacute or
				 * numbersign - and a PANEL_TEXT will actually use
				 * such strings if event_string(ev) is not null.
				 * A TEXTSW will not do that...
				 * Real Key events attempt to supply ie_string using
				 * XLookupString....
				 */
	 			if (_xv_is_multibyte) {
					if (key_value >= 128 && key_value < 256) {
						event->ie_string = ol_trans_utf[key_value - 128];
					}
					else event->ie_string = NULL;
				}
				else event->ie_string = NULL;

				alt_modmask = (int)xv_get(server_public, SERVER_ALT_MOD_MASK);
				meta_modmask = (int)xv_get(server_public, SERVER_META_MOD_MASK);

				/* Get the Semantic Action part of the XView Event initialised */

				/* for the modifier values cf. Ref (jklvwerfmbhgrf) */
				if (clientmessage->data.l[2] & ControlMask) modifiers += 0x100;
				if (clientmessage->data.l[2] & meta_modmask) modifiers += 0x200;

#ifdef BEFORE_DRA_CHANGED_IT
				/* why data.l[3] ??? data.l[2] would have been enough
				 * for all the modifiers...
				 * ... and what I saw on my last available sun, in the
				 * meantime (18.8.2024) the sun-vkbd always sends
				 * data.l[3] = 0;
				 */

				if (clientmessage->data.l[3] & alt_modmask)
					modifiers += 0x400;
				if ((clientmessage->data.l[3] & ShiftMask) && (keyboard_key))
					modifiers += 0x800;
#else /* BEFORE_DRA_CHANGED_IT */
				if (clientmessage->data.l[2] & alt_modmask) modifiers += 0x400;
				if ((clientmessage->data.l[2] & ShiftMask) && (keyboard_key))
					modifiers += 0x800;
#endif /* BEFORE_DRA_CHANGED_IT */

				sem_ksym = ksym;
				if ((clientmessage->data.l[2] & LockMask) &&
						!(clientmessage->data.l[2] & ShiftMask) &&
						(ksym >= 'A' && ksym <= 'Z'))
					sem_ksym = ksym | 0x20;

				if (keyboard_key) {
					int idx = server_sem_map_index(ksym);

					if (key_sem_maps[idx]) {
						sem_action = key_sem_maps[idx][(sem_ksym & 0xFF) +
												modifiers] | XVIEW_SEMANTIC;
					}
					else {
						sem_action = ACTION_NULL_EVENT;
					}
				}
				else
					sem_action = ascii_sem_map[(sem_ksym & 0xFF) +
												modifiers] | XVIEW_SEMANTIC;
				event_set_action(event, sem_action);

				if ((int)(clientmessage->data.l[1]) == KeyPress) {
					SERVERTRACE((100, "%16s: %lx %lx %lx: %d = '%c'\n",
			 				XKeysymToString((KeySym)clientmessage->data.l[0]),
			 				clientmessage->data.l[2],
			 				clientmessage->data.l[3],
			 				clientmessage->data.l[4],
							event_id(event), event_id(event)));
				}
				/* Initialise the xevent to NULL for now */

				event->ie_xevent = NULL;
			}
			/* damit ich von aussen das ACTION_RESCALE initiieren kann
			 * das kann wieder weg, wenn das Rescaling geht
			 */
			else if (clientmessage->message_type ==
					xv_get(server_public, SERVER_ATOM, "_DRA_RESCALE"))
			{
				Xv_window win = window;
				Event ev;
				unsigned long scale = clientmessage->data.l[0];

				SERVERTRACE((790, "scaling to %ld\n", scale));

				if (! xv_get(win, XV_IS_SUBTYPE_OF, FRAME)) {
					win = xv_get(window, WIN_FRAME);
				}

				event_init(&ev);
				event_set_id(&ev, ACTION_RESCALE);
				event_set_action(&ev, ACTION_RESCALE);
				event_set_xevent(&ev, (XEvent *)clientmessage);
				event_set_window(&ev, win);

				SERVERTRACE((790, "posting ACTION_RESCALE %ld to %s\n",
								scale, (char *)xv_get(win, XV_LABEL)));
				win_post_event_arg(win, &ev, NOTIFY_IMMEDIATE, scale,
								NOTIFY_COPY_NULL, NOTIFY_RELEASE_NULL);
				event_set_id(event, WIN_CLIENT_MESSAGE);
				window_set_client_message(window, clientmessage);
				return TRUE;
			}
			else {
				event_set_id(event, WIN_CLIENT_MESSAGE);
				window_set_client_message(window, clientmessage);
			}
	}
	return FALSE;
}

static int process_property_events(Xv_object window, XPropertyEvent *property,
									Event *event)
{
	Xv_Drawable_info *info;
	Xv_opaque server_public;
	Server_atom_type atom_type;

	DRAWABLE_INFO_MACRO(window, info);
	server_public = xv_server(info);
	server_set_timestamp(server_public, &event->ie_time, property->time);
	atom_type = server_get_atom_type(server_public, property->atom);

	switch (atom_type) {
		case SERVER_WM_PIN_STATE_TYPE:
			return (process_wm_pushpin_state(window, property->atom, event));
		default:
			event_set_id(event, WIN_PROPERTY_NOTIFY);
	}
	return FALSE;
}

static int process_wm_pushpin_state(Xv_object window, Atom atom, Event *event)
{
	Xv_Drawable_info *info;
	int status;
	Atom type;
	int format;
	unsigned long nitems;
	unsigned long bytes;
	unsigned char *prop;
	long *pinstate;

	DRAWABLE_INFO_MACRO(window, info);
	status = XGetWindowProperty(xv_display(info), xv_xid(info), atom,
						0L, 1L, False, XA_INTEGER,
						&type, &format, &nitems, &bytes, &prop);
	if (status != Success)
		return 1;

	if (!prop)
		return 1;

	if (format != 32) {
		XFree((char *)prop);
		return 1;
	}
	pinstate = (long *)prop;
	switch (*pinstate) {
		case WMPushpinIsIn:
			event_set_action(event, ACTION_PININ);
			break;
		case WMPushpinIsOut:
			event_set_action(event, ACTION_PINOUT);
			break;
	}
	XFree((char *)prop);
	return 0;
}

#ifdef SEEMS_UNUSED
Xv_private void win_event_to_proc_with_ptr(Xv_opaque window_public,
				Atom event_type, XID sender_id, int x, int y)
{
	Xv_Drawable_info *info;
	XClientMessageEvent event_struct;

	DRAWABLE_INFO_MACRO(window_public, info);
	event_struct.type = ClientMessage;
	event_struct.message_type = event_type;

	event_struct.window = XV_DUMMY_WINDOW;	/* Put anything in here, the
											   * server will not use this */
	event_struct.format = 32;
	event_struct.data.l[0] = x;
	event_struct.data.l[1] = y;
	event_struct.data.l[2] = sender_id;
	XSendEvent(xv_display(info), PointerWindow, False, NoEventMask,
						(XEvent *) & event_struct);
	XFlush(xv_display(info));
}
#endif /* SEEMS_UNUSED */

/*
 * BlockForEvent
 * 
 * Scan the input queue for the specified event. If there aren't any events in
 * the queue, select() for them until a certain timeout period has elapsed.
 * Return value indicates whether the specified event  was seen.
 */
static int BlockForEvent(Display *display, XEvent *xevent, long usec,
				int (*predicate)(Display *, XEvent *, char *), char *eventType)
{
	fd_set rfds;
	int result;
	struct timeval timeout;
	struct timeval starttime, curtime, diff1, diff2;

	timeout.tv_sec = 0;
	timeout.tv_usec = usec;

	(void)gettimeofday(&starttime, NULL);
	XFlush(display);
	XSync(display, False);
	while (1) {
		/*
		 * Check for data on the connection.  Read it and scan it.
		 */
		if (XCheckIfEvent(display, xevent, predicate, eventType))
			return (TRUE);

		/*
		 * We've drained the queue, so we must select for more.
		 */
		FD_ZERO(&rfds);
		FD_SET(ConnectionNumber(display), &rfds);

		result = select(ConnectionNumber(display) + 1, &rfds, NULL, NULL, &timeout);

		if (result == 0) {
			/* we timed out without getting anything */
			return FALSE;
		}
		/*
		 * Report errors. If we were interrupted (errno == EINTR), we simply
		 * continue around the loop. We scan the input queue again.
		 */
		if (result == -1 && errno != EINTR)
			perror("Select");

		/*
		 * Either we got interrupted or the descriptor became ready. Compute
		 * the remaining time on the timeout.
		 */
		(void)gettimeofday(&curtime, NULL);
		tvdiff(&starttime, &curtime, &diff1);
		tvdiff(&diff1, &timeout, &diff2);
		timeout = diff2;
		starttime = curtime;
		if (timeout.tv_sec < 0)
			return False;
	}
}

static int chording(Display *display, XButtonEvent *bEvent, int timeout)
{
    XEvent          xevent;

    /* XView does a passive grab on the SELECT button! */
    window_x_allow_events(display, bEvent->time);

    return BlockForEvent(display, &xevent, (long)timeout *1000L, GetButtonEvent,
			 (char *) bEvent);
}




/*
 * Predicate function for XCheckIfEvent
 * 
 */
/* ARGSUSED */
static int GetButtonEvent(Display *display, XEvent *xevent, char *args)
{
	XButtonEvent prevEvent, *newEvent;
	static int mFlg;

	if (((xevent->type & 0177) != ButtonPress) &&
						((xevent->type & 0177) != ButtonRelease)) {
		mFlg = 0;
		return FALSE;
	}
	newEvent = (XButtonEvent *) xevent;

	XV_BCOPY((char *)args, (char *)&prevEvent, sizeof(XButtonEvent));

	switch (xevent->type) {
		case ButtonPress:
			if ((newEvent->button == prevEvent.button) || newEvent->button == 3) {
				mFlg = 0;
				return FALSE;
			}
			mFlg = 1;
			break;
		case ButtonRelease:
			if (mFlg) {
				mFlg = 0;
				return TRUE;
			}
			return FALSE;
	}
	return TRUE;
}




/* compute t2 - t1 and return the time value in diff */
static void tvdiff(struct timeval *t1, struct timeval *t2, struct timeval *diff)
{
	diff->tv_sec = t2->tv_sec - t1->tv_sec;
	diff->tv_usec = t2->tv_usec - t1->tv_usec;
	if (diff->tv_usec < 0) {
		diff->tv_sec -= 1;
		diff->tv_usec += 1000000;
	}
}


Bool win_check_lang_mode(Xv_server server, Display *display, Event *event)
{
	static short lang_mode = 0;
	static Window sft_key_win;
	Atom enter_lang_atom, exit_lang_atom, translate_key_atom;
	XClientMessageEvent xcl;
	XKeyEvent *keyevent;

	if (!event) {
		/* KBD_DONE event. So get out of the languages mode */
		lang_mode = 0;
		return (1);
	}
	keyevent = (XKeyEvent *) event->ie_xevent;

	if (event_action(event) == ACTION_TRANSLATE) {

		sft_key_win = xv_get_softkey_xid(server, display);

		if (sft_key_win == None)	/* There is no soft keys process running */
			return (0);


		enter_lang_atom = xv_get(server, SERVER_ATOM, "_OL_ENTER_LANG_MODE");
		exit_lang_atom = xv_get(server, SERVER_ATOM, "_OL_EXIT_LANG_MODE");

		if (event_is_down(event)) {

			lang_mode = 1;

			/* Turn off Auto repeat for the LANG key */

			/*
			 * keycontrol_values.key              =
			 * event->ie_xevent->xkey.keycode;
			 * keycontrol_values.auto_repeat_mode = AutoRepeatModeOff;
			 * XChangeKeyboardControl(display,KBKey | KBAutoRepeatMode,
			 * &keycontrol_values); XFlush(display);
			 */


			/* Construct an Event */
			xcl.type = ClientMessage;
			xcl.window = sft_key_win;
			xcl.message_type = enter_lang_atom;
			xcl.format = 32;
			XSendEvent(display, sft_key_win, False, 0L, (XEvent *)&xcl);
			return (1);

		}
		else {

			lang_mode = 0;

			/* Restore AutoRepeat Default mode back to the key */
			/*
			 * keycontrol_values.key              =
			 * event->ie_xevent->xkey.keycode;
			 * keycontrol_values.auto_repeat_mode = AutoRepeatModeDefault;
			 * 
			 * XChangeKeyboardControl(display,KBKey | KBAutoRepeatMode,
			 * &keycontrol_values); XFlush(display);
			 */

			xcl.type = ClientMessage;
			xcl.window = sft_key_win;
			xcl.message_type = exit_lang_atom;
			xcl.format = 32;
			XSendEvent(display, sft_key_win, False, 0L, (XEvent *)&xcl);
			return (1);
		}
	}
	if (lang_mode) {

		/* start sending keys to the virtual keyboard */

		if ((event->ie_code < 33) || (event->ie_code == 127))
			return (0);	/* Do not send unwanted events */

		translate_key_atom = xv_get(server, SERVER_ATOM, "_OL_TRANSLATE_KEY");

		/* Construct an Event */

		xcl.type = ClientMessage;
		xcl.window = sft_key_win;
		xcl.message_type = translate_key_atom;

		/* format 16 WILL NOT WORK when the application is running on a machine
		 * with different byte order than the machine running vkbd !!!!
		 */
		xcl.format = 32;

		xcl.data.l[0] = keyevent->keycode;
		xcl.data.l[1] = keyevent->type;
		xcl.data.l[2] = keyevent->state;

		/* this pattern in data.l[3] indicates that data.l[4] contains
		 * the current focus window, so that the vkbd does not have to
		 * call XGetInputFocus before sending the reply (_OL_TRANSLATED_KEY).
		 * 
		 * Of course, this will only be understood by my own reimplementation
		 * of vkbd. The real (SUN) vkbd will hopefully ignore data.l[3]
		 * and data.l[4].
		 */
		xcl.data.l[3] = 0xaffe123; /* pattern to recognize */
		xcl.data.l[4] = keyevent->window;

		if (keyevent->type == KeyPress) {
			SERVERTRACE((100, "c %d, s %x\n", keyevent->keycode, keyevent->state));
		}

		XSendEvent(display, sft_key_win, False, 0L, (XEvent *)&xcl);
		return (1);
	}
	return (0);
}

/*
 * Get the Soft Key Labels win through the selection mechanism: the Soft key
 * window owns the selection "_OL_SOFT_KEYS_PROCESS". By querying the owner of
 * this selection, we get hold of the soft key window
 */


Xv_private XID xv_get_softkey_xid(Xv_server server, Display *display)
{
	Atom sftk_process_atom;
	Window seln_owner;

	sftk_process_atom = xv_get(server, SERVER_ATOM, "_OL_SOFT_KEYS_PROCESS");
	seln_owner = XGetSelectionOwner(display, sftk_process_atom);
	return (seln_owner);

}

static void win_handle_quick_selection(Xv_Drawable_info *info, Event *event)
{
	Atom key_type = xv_get(xv_server(info), SERVER_ATOM,
					(event_action(event) == ACTION_CUT ? "MOVE" : "DUPLICATE"));
	Atom property = xv_get(xv_server(info), SERVER_ATOM,
											"_SUN_QUICK_SELECTION_KEY_STATE");

	switch (event_action(event)) {
		case ACTION_PROPS:
		case ACTION_PASTE:
		case ACTION_CUT:

			/* Always store the data on "property" on screen 0 of the display */

			XChangeProperty(xv_display(info), RootWindow(xv_display(info), 0),
								property, XA_ATOM, 32, PropModeReplace,
								(unsigned char *)&key_type, 1);
	    	SERVERTRACE((765, "%s: is cut: %d\n", xv_app_name,
								event_action(event) == ACTION_CUT));
			break;
		case ACTION_SELECT:
		case ACTION_ADJUST:
		case ACTION_MENU:
		case LOC_DRAG: /* dear people: again and again.... */
		case LOC_WINENTER:
		case LOC_WINEXIT:
			{
				Atom notUsed;
				int format;
				unsigned long nitems, bytes_after;
				unsigned char *data;

				/* Always get the data from "property" on screen 0 of the display */

				if (XGetWindowProperty(xv_display(info),
								RootWindow(xv_display(info), 0), property, 0L,
								1L, False, XA_ATOM, &notUsed, &format, &nitems,
								&bytes_after, &data) != Success) {
					return;
				}
				else {
					if (data) {
						/* key_type == DUPLICATE in this case */
						if (key_type == *(Atom *) data)
							event_set_quick_duplicate(event);
						else {
	    					SERVERTRACE((765, "%s: set quick_move\n",
										xv_app_name));
							event_set_quick_move(event);
						}
						XFree((char *)data);
					}
				}
				break;
			}
	}
}

static int win_handle_compose(Event *event, XComposeStatus *c_status, int last)
{
	Frame frame = xv_get(event_window(event), WIN_FRAME);
	int current = c_status->chars_matched;

	/* State table for the compose key. */
	/* current == 0:  Not in Compose
	 * current == 1:  1 char matched 
	 * current == 2:  2 char matched 
	 * current == 3:  Accepted
	 */
	/* Return True if we want to pass event on to application. 
	 * return False if we want to eat this event.
	 */

	if (last == 0 || last == 3) {
		if (current == 0 || current == 3)
			return (True);
		else if (current == 1 || current == 2) {
			xv_set(frame, FRAME_COMPOSE_STATE, True, NULL);
			return (False);
		}
	}
	else if (last == 1) {
		if (current == 0) {
			xv_set(frame, FRAME_COMPOSE_STATE, False, NULL);
			return (False);
		}
		else if (current == 1 || current == 3)
			return (False);
		else if (current == 2)
			return (False);
	}
	else if (last == 2) {
		if (current == 0) {
			xv_set(frame, FRAME_COMPOSE_STATE, False, NULL);
			return (False);
		}
		else if (current == 1 || current == 2) {
			return (False);
		}
		else if (current == 3) {
			xv_set(frame, FRAME_COMPOSE_STATE, False, NULL);
			return (True);
		}
	}
	return (False);	/* Eat this event */
}

Xv_private void win_repaint_application(Display *dpy)
{
	XEvent ev;

	while (XCheckTypedEvent(dpy, Expose, &ev)) {
		Event event;
		Xv_object window = XV_NULL;

		xevent_to_event(dpy, &ev, &event, &window);
		/* copied from (lkhregfvhkwberf) */
		if (window && event_id(&event) == WIN_REPAINT) {
			if ((!xv_get(window, WIN_X_PAINT_WINDOW)) &&
				(!xv_get(window, WIN_NO_CLIPPING)))
			{
				/*
				 * Force the clipping list for the window to be the
				 * damage list while the client is processing the
				 * WIN_REPAINT, then clear the clipping list back to
				 * its normal state.
				 */
				Rectlist *rl;

				rl = win_get_damage(window);
				win_set_clip(window, rl);
				win_post_event(window, &event,
						WIN_IS_IN_LOOP ? NOTIFY_IMMEDIATE : NOTIFY_SAFE);
				win_set_clip(window, RECTLIST_NULL);
			}
			else {
				win_post_event(window, &event,
						WIN_IS_IN_LOOP ? NOTIFY_IMMEDIATE : NOTIFY_SAFE);
			}
			win_clear_damage(window);
		}
	}
}
