#ifndef lint
#ifdef SCCS
static char     sccsid[] = "@(#)sel_util.c 1.29 93/06/28 DRA: $Id: sel_util.c,v 4.23 2025/02/05 15:24:50 dra Exp $";
#endif
#endif

/*
 *	(c) Copyright 1990 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */

#include <xview_private/sel_impl.h>
#include <xview_private/svr_impl.h>
#include <xview_private/win_info.h>
#include <xview/notify.h>
#include <xview/window.h>
#include <xview/server.h>
#ifdef SVR4
#include <stdlib.h>
#endif  /* SVR4 */

typedef struct sel_req_list {
    int            done;
    Sel_reply_info   *reply;
    struct  sel_req_list   *next;
} Sel_req_list;


static void tvdiff(struct timeval *t1, struct timeval *t2, struct timeval *diff);
static void FreeMultiProp(Sel_reply_info  *reply);
static int SelFindReply(Sel_reply_info *r1, Sel_reply_info *r2);
static Sel_req_list *SelMatchReqTbl( Sel_reply_info  *reply);

XContext  selCtx;

/* instead of including <xview_private/seln_impl.h> */
Xv_private void selection_agent_selectionrequest(Xv_Server server,
									XSelectionRequestEvent *req_event);

Pkg_private void xv_sel_cvt_xtime_to_timeval(Time  XTime, struct timeval *tv)
{
    tv->tv_sec = ((unsigned long) XTime )/1000;
    tv->tv_usec = (((unsigned long) XTime ) % 1000) * 1000;
}


Pkg_private Time xv_sel_cvt_timeval_to_xtime(struct  timeval  *timeV)
{
    return ( ( timeV->tv_sec * 1000 ) + ( timeV->tv_usec / 1000 ) );
}


Pkg_private Sel_owner_info *xv_sel_find_selection_data(Display *dpy,
								Atom selection, Window xid)
{
	Sel_owner_info *sel;

	if (selCtx == 0)
		selCtx = XUniqueContext();

	/* every SELECTION_OWNER stores "itself" using
	 * xv_sel_set_selection_data and uses xv_sel_find_selection_data
	 * to identify itself in case of SelectionRequest or SelectionClear events.
	 * 
	 * When a SELECTION_REQUESTOR calls this function, it *may* get a
	 * selection owner (in the same process), but if for this selCtx
	 * is nothing found, we **now** return NULL.
	 */
	if (XFindContext(dpy, (Window) selection, selCtx, (caddr_t *) & sel)) {
		return NULL;
	}
	return sel;
}



Pkg_private Sel_owner_info * xv_sel_set_selection_data(Display *dpy, Atom selection, Sel_owner_info *sel_owner)
{
    if ( selCtx == 0 )
        selCtx = XUniqueContext();

    /* Attach the atom type informations to the display */
    sel_owner->atomList = xv_sel_find_atom_list( dpy, sel_owner->xid );
    sel_owner->dpy = dpy;
    sel_owner->selection = selection;
    sel_owner->status = 0;
    (void)XSaveContext( dpy,(Window)selection, selCtx,(caddr_t)sel_owner);
    return  sel_owner;
}

/*
 * REMINDER: This needs to be changed to a more efficient way of getting
 *  last event time.
*/
Xv_private Time xv_sel_get_last_event_time(Display  *dpy, Window   win)
{
    XEvent         event;
    XPropertyEvent *ev;
    Atom  prop = xv_sel_get_property( dpy );
    XWindowAttributes  winAttr;
    int    arg;
    int  status = xv_sel_add_prop_notify_mask( dpy, win, &winAttr );

    XChangeProperty( dpy, win, prop, XA_STRING, 8, PropModeReplace,
		    (unsigned char *) NULL, 0 );

    /* Wait for the PropertyNotify */
    arg = PropertyNotify;
    if ( !xv_sel_block_for_event( dpy, &event, 3 , xv_sel_predicate, (char *) &arg ) ) {
	xv_error(XV_NULL,
                 ERROR_STRING,XV_MSG("xv_sel_get_last_event_time: Unable to get the last event time"),
	         ERROR_PKG,SELECTION,
                 NULL);
	return ( (Time) NULL );
    }

    xv_sel_free_property( dpy, prop );

    /*
     * If we have added PropertyChangeMask to the win, reset the mask to
     * it's original state.
     */
    if ( status )
        XSelectInput( dpy, win, winAttr.your_event_mask  );

    ev = (XPropertyEvent *) &event;
    return( ev->time );
}


int xv_sel_add_prop_notify_mask(Display *dpy, Window win,XWindowAttributes *winAttr)
{
    XGetWindowAttributes( dpy, win, winAttr );

    /*
     * If PropertyChangeMask has already been set on the window, don't set
     * it again.
     */
    if (  winAttr->your_event_mask & PropertyChangeMask )
        return FALSE;

    XSelectInput( dpy, win, winAttr->your_event_mask | PropertyChangeMask );
    return TRUE;
}


XContext  targetCtx;

/* REMEMBER TO FREE THE ALLOCATED MEM */
Pkg_private Sel_atom_list *xv_sel_find_atom_list(Display *dpy, Window xid)
{
	Sel_atom_list *list;
	Xv_window xvWin;
	Xv_Server server;

	if (targetCtx == 0)
		targetCtx = XUniqueContext();
	if (XFindContext(dpy, DefaultRootWindow(dpy), targetCtx, (caddr_t *)&list))
	{
		list = xv_alloc(Sel_atom_list);
		if (list == NULL) {
			return ((Sel_atom_list *) NULL);
		}
		/* Get XView object */
		xvWin = win_data(dpy, xid);
		server = XV_SERVER_FROM_WINDOW(xvWin);

		list->multiple = (Atom) xv_get(server, SERVER_ATOM, "MULTIPLE");
		list->targets = (Atom) xv_get(server, SERVER_ATOM, "TARGETS");
		list->timestamp = (Atom) xv_get(server, SERVER_ATOM, "TIMESTAMP");
		list->file_name = (Atom) xv_get(server, SERVER_ATOM, "FILE_NAME");
		list->string = XA_STRING;
		list->incr = (Atom) xv_get(server, SERVER_ATOM, "INCR");
		list->integer = XA_INTEGER;
		list->atom_pair = (Atom) xv_get(server, SERVER_ATOM, "ATOM_PAIR");

#ifdef OW_I18N
		list->ctext = (Atom) xv_get(server, SERVER_ATOM, "COMPOUND_TEXT");
#endif /* OW_I18N */

		XSaveContext(dpy, DefaultRootWindow(dpy), targetCtx, (caddr_t)list);
	}
	return list;
}

XContext  propCtx;

/* Pkg_private */
static Sel_prop_list * xv_sel_get_prop_list(Display *dpy)
{
	Sel_prop_list *list;

	if (propCtx == 0)
		propCtx = XUniqueContext();
	if (XFindContext(dpy, DefaultRootWindow(dpy), propCtx, (caddr_t *)&list)) {

		list = xv_alloc(Sel_prop_list);
		if (list == NULL) {
			return ((Sel_prop_list *) NULL);
		}

		list->prop = XInternAtom(dpy, "XV_SELECTION_0", FALSE);
		list->avail = TRUE;
		list->next = NULL;

		(void)XSaveContext(dpy, DefaultRootWindow(dpy), propCtx,
				(caddr_t) list);
	}
	return (list);
}


/* NOTE: the avail field should be reset to TRUE after a complete transaction. */
Pkg_private Atom xv_sel_get_property(Display  *dpy)
{
	Sel_prop_list *cPtr;
	char str[100];
	int i = 0;

	cPtr = xv_sel_get_prop_list(dpy);

	do {
		if (cPtr->avail) {
			cPtr->avail = FALSE;
			return (cPtr->prop);
		}
		i++;
		if (cPtr->next == NULL)
			break;
		cPtr = cPtr->next;
	} while (1);


	/* Create a new property and add it to the list */
	cPtr->next = xv_alloc(Sel_prop_list);
	if (cPtr->next == NULL) {
		return ((Atom) NULL);
	}
	cPtr = cPtr->next;
	sprintf(str, "XV_SELECTION_%d", i);
	cPtr->prop = XInternAtom(dpy, str, FALSE);
	cPtr->avail = FALSE;
	cPtr->next = NULL;
	return (cPtr->prop);
}


Pkg_private void xv_sel_free_property(Display *dpy, Atom prop)
{
	Sel_prop_list *plPtr;

	plPtr = xv_sel_get_prop_list(dpy);

	do {
		if (prop == None) {
			/* ALLE sind available */
			plPtr->avail = TRUE;
		}
		else {
			if (plPtr->prop == prop) {
				plPtr->avail = TRUE;
				break;
			}
		}
		if (plPtr->next == NULL) {
			break;
		}
		plPtr = plPtr->next;
	} while (1);
}



/*
 * Predicate function for XCheckIfEvent
 *
 */
Pkg_private int xv_sel_predicate(Display *display, XEvent *xevent, char *args)
{
	int eventType;

	XV_BCOPY((char *)args, (char *)&eventType, sizeof(int));

	if ((xevent->type & 0177) == eventType)
		return (TRUE);

	/*
	 * If we receive a SelectionRequest while waiting, handle selection request
	 * to avoid a dead lock.
	 */
	if ((xevent->type & 0177) == SelectionRequest) {
		XSelectionRequestEvent *reqEvent = (XSelectionRequestEvent *) xevent;

		if (!xv_sel_handle_selection_request(reqEvent)) {
			Xv_window xvWin;
			Xv_Server server;

			xvWin = win_data(display, reqEvent->requestor);

			if (xvWin == 0)
				server = xv_default_server;
			else
				server = XV_SERVER_FROM_WINDOW(xvWin);

			/* I want to know exactly what is happening here: */
			fprintf(stderr, "Received a sel req %s:%s ...\n",
					(char *)xv_get(server,SERVER_ATOM_NAME,reqEvent->selection),
					(char *)xv_get(server, SERVER_ATOM_NAME, reqEvent->target));
			/*
			 * Send this request to the old selection package.
			 */
			selection_agent_selectionrequest(server, reqEvent);
		}

	}

	return (FALSE);
}

/*
 * Predicate function for XCheckIfEvent
 *
 */
/*ARGSUSED*/
Pkg_private int xv_sel_check_selnotify(Display *display, XEvent *xevent, char *args)
{
	Sel_reply_info reply;

	XV_BCOPY((char *)args, (char *)&reply, sizeof(Sel_reply_info));

	if ((xevent->type & 0177) == SelectionNotify) {
		XSelectionEvent *ev = (XSelectionEvent *) xevent;

		if ((ev->target == *reply.sri_target))
			return (TRUE);
		/*
		 * else
		 * fprintf(stderr, "Possible BUG case - xv_sel_check_selnotify \n");
		 */
	}

	/*
	 * If we receive a SelectionRequest while waiting, handle selection request
	 * to avoid a dead lock.
	 */
	if ((xevent->type & 0177) == SelectionRequest) {
		XSelectionRequestEvent *reqEvent = (XSelectionRequestEvent *) xevent;

		/* I want to know exactly what is happening here: */
		fprintf(stderr, "Received a sel req %s:%s ...\n",
				(char *)xv_get(xv_default_server, SERVER_ATOM_NAME,
											reqEvent->selection),
				(char *)xv_get(xv_default_server, SERVER_ATOM_NAME,
											reqEvent->target));
		fprintf(stderr, "... while waiting for %s:%s\n",
				(char *)xv_get(xv_default_server, SERVER_ATOM_NAME, 
						xv_get(SEL_REQUESTOR_PUBLIC(reply.sri_req_info),SEL_RANK)),
				(char *)xv_get(xv_default_server, SERVER_ATOM_NAME,
												*reply.sri_target));

		if (!xv_sel_handle_selection_request(reqEvent)) {
			Xv_window xvWin;
			Xv_Server server;

			xvWin = win_data(display, reqEvent->requestor);

			if (xvWin == 0)
				server = xv_default_server;
			else
				server = XV_SERVER_FROM_WINDOW(xvWin);

			/*
			 * Send this request to the old selection package.
			 */
			selection_agent_selectionrequest(server, reqEvent);
		}

	}

	return (FALSE);
}




Pkg_private char * xv_sel_atom_to_str(Display *dpy, Atom atom, XID xid)
{
    Xv_window xvWin;
    Xv_opaque server;

    if ( xid == 0 )
        server = xv_default_server;
    else {
	xvWin = win_data( dpy, xid );
	server = XV_SERVER_FROM_WINDOW( xvWin );
    }

    return ( (char *) xv_get( server, SERVER_ATOM_NAME, atom ));
}

Pkg_private Atom xv_sel_str_to_atom(Display *dpy, char *str, XID xid)
{
    Xv_window xvWin;
    Xv_opaque server;

    if ( xid == 0 )
        server = xv_default_server;
    else {
	xvWin = win_data( dpy, xid );
	server = XV_SERVER_FROM_WINDOW( xvWin );
    }

    return ( (Atom) xv_get( server, SERVER_ATOM, str ));
}

Pkg_private void xv_sel_handle_error(int errCode, Sel_req_info *sel,
						Sel_reply_info *replyInfo, Atom target)
{
    Selection_requestor sel_req;

    if ( sel != NULL )
        sel_req = SEL_REQUESTOR_PUBLIC( sel );

    if ( (sel != NULL) && (sel->reply_proc != NULL) )
        (*sel->reply_proc)( sel_req, target, (Atom)0, (Xv_opaque)&errCode,
								(unsigned long)(SEL_ERROR), 0 );
}


/*
 * xv_sel_block_for_event
 *
 * Scan the input queue for the specified event.
 * If there aren't any events in the queue, select() for them until a
 * certain timeout period has elapsed.  Return value indicates whether the
 * specified event  was seen.
 */
Pkg_private int xv_sel_block_for_event(Display *display, XEvent *xevent, int seconds, int (*predicate)(Display *, XEvent *, char *), char *arg)
{
	fd_set rfds;
	int result;
	struct timeval timeout;
	struct timeval starttime, curtime, diff1, diff2;

	timeout.tv_sec = seconds;
	timeout.tv_usec = 0;

	(void)gettimeofday(&starttime, NULL);
	XSync(display, False);
	while (1) {
		/*
		 * Check for data on the connection.  Read it and scan it.
		 */
		if (XCheckIfEvent(display, xevent, predicate, (char *)arg))
			return (TRUE);

		/*
		 * We've drained the queue, so we must select for more.
		 */
		FD_ZERO(&rfds);
		FD_SET(ConnectionNumber(display), &rfds);

		result = select(ConnectionNumber(display) + 1, &rfds, NULL,
				NULL, &timeout);

		if (result == 0) {
			((XSelectionEvent *) xevent)->property = None;
			/* REMINDER: Do we need this ^^^ here? */

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
		 * Either we got interrupted or the descriptor became ready.
		 * Compute the remaining time on the timeout.
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

static Sel_req_list *xv_sel_add_new_req(Sel_req_list *reqTbl, Sel_reply_info *reply)
{
	Sel_req_list *rPtr;

	rPtr = reqTbl;

	/*
	 * If there is an slot open use it.
	 */
	do {
		if (rPtr->done) {
			if (rPtr->reply != NULL)
				XFree((char *)rPtr->reply);
			rPtr->reply = reply;
			rPtr->done = FALSE;
			return reqTbl;
		}
		if (rPtr->next == NULL)
			break;
		rPtr = rPtr->next;
	} while (1);

	/*
	 * Create a new reply and add it to the list.
	 */
	rPtr->next = xv_alloc(Sel_req_list);
	if (rPtr->next == NULL) {
		return ((Sel_req_list *) NULL);
	}
	rPtr = rPtr->next;
	rPtr->reply = reply;
	rPtr->done = FALSE;
	rPtr->next = NULL;

	return reqTbl;
}


static XContext replyCtx;

/* for every nonblocking request (sel_post_req) this function is called
 * and enters this request (the Sel_reply_info) into a linked list
 * of Sel_req_list.
 * This list hangs on the root window with replyCtx.
 */
Pkg_private void xv_sel_set_reply(Sel_reply_info  *reply)
{
	Sel_req_list *reqTbl;
	Display *dpy;

	if (replyCtx == 0) replyCtx = XUniqueContext();

	dpy = reply->sri_dpy;

	if (XFindContext(dpy, DefaultRootWindow(dpy), replyCtx, (caddr_t *)&reqTbl))
	{
		reqTbl = xv_alloc(Sel_req_list);
		reqTbl->done = FALSE;
		reqTbl->reply = reply;
		reqTbl->next = NULL;
		(void)XSaveContext(dpy, DefaultRootWindow(dpy), replyCtx,
				(caddr_t) reqTbl);
	}
	else {
		xv_sel_add_new_req(reqTbl, reply);
	}
}

static int SelMatchReply( XEvent *event, Sel_reply_info *reply)
{
	if (event->type == SelectionNotify) {
		XSelectionEvent *ev = (XSelectionEvent *) event;

		if (ev->requestor != reply->sri_requestor)
			return FALSE;

		if (ev->selection != reply->sri_selection)
			return FALSE;

		if ((ev->target != *reply->sri_target) &&
				(ev->target != reply->sri_incr))
			return FALSE;
	}
	else {
		XPropertyEvent *ev = (XPropertyEvent *) event;

		if (ev->window != reply->sri_requestor)
			return FALSE;

		if (ev->state != PropertyNewValue)
			return FALSE;
	}

	return TRUE;
}

Pkg_private Sel_reply_info *xv_sel_get_reply(XEvent *event)
{
	Sel_req_list *reqTbl, *rPtr;
	Display *dpy;

	if (replyCtx == 0) {
		replyCtx = XUniqueContext();
		return (Sel_reply_info *) NULL;
	}

	if (event->type == SelectionNotify)
		dpy = event->xselection.display;
	else
		dpy = event->xproperty.display;

	if (XFindContext(dpy, DefaultRootWindow(dpy), replyCtx, (caddr_t *)&reqTbl))
		return (Sel_reply_info *) NULL;

	rPtr = reqTbl;

	/*
	 * Find this window's reply struct.
	 */
	do {
		if (!rPtr->done && SelMatchReply(event, rPtr->reply))
			return rPtr->reply;

		if (rPtr->next == NULL)
			break;
		rPtr = rPtr->next;
	} while (1);

	return (Sel_reply_info *) NULL;
}




/*ARGSUSED*/
Pkg_private Notify_value xv_sel_handle_sel_timeout(Notify_client client, int which)
{
    Sel_reply_info  *reply = (Sel_reply_info *) client;
    Sel_req_list  *reqTbl;


    notify_set_itimer_func(client, NOTIFY_TIMER_FUNC_NULL,
               ITIMER_REAL, (struct itimerval *)0, (struct itimerval *)0);
    reqTbl = (Sel_req_list *) SelMatchReqTbl( reply);

    if ( reqTbl != NULL )  {

        if ( !reqTbl->done  )  {
	    xv_sel_handle_error( SEL_TIMEDOUT, reply->sri_req_info, reply,
			*reply->sri_target );
	    xv_sel_end_request( reply);
	}
    }

    return NOTIFY_DONE;
}


static Sel_req_list *SelMatchReqTbl(Sel_reply_info  *reply)
{
    Sel_req_list  *reqTbl, *rPtr;

    if ( replyCtx == 0 ) {
        replyCtx = XUniqueContext();
		return FALSE;
	}

    if (XFindContext(reply->sri_dpy, DefaultRootWindow(reply->sri_dpy), replyCtx, (caddr_t *)&reqTbl))
		return FALSE;
	/*
	 * #0  SelMatchReqTbl (reply=0x253779e0) at sel_util.c:743
	 * #1  xv_sel_end_request (reply=0x253779e0) at sel_util.c:770
	 * #2  xv_sel_handle_selection_notify(ev=0x7fffc20898a0) at sel_req.c:1440
	 * #3  window_default_event_func ()
	 */

    rPtr = reqTbl;

    /*
     * Find this window's request table.
     */
    do {
		if ( !rPtr->done && SelFindReply( reply, rPtr->reply ) )
	    	return rPtr;

		if ( rPtr->next == NULL )
	    	break;
		rPtr = rPtr->next;
    } while ( 1 );

    return (Sel_req_list *) NULL;
}


int xv_sel_end_request(Sel_reply_info *reply)
{
	XWindowAttributes winAttr;
	Sel_req_list *reqTbl;

	reqTbl = SelMatchReqTbl(reply);

	if (reqTbl != NULL) {
		Sel_owner_info *contextdata;
		Window sele = (Window)reply->sri_selection;
		Atom prop = reqTbl->reply->sri_property;

		notify_set_itimer_func((Notify_client) reply, NOTIFY_TIMER_FUNC_NULL,
				ITIMER_REAL, NULL, NULL);

		/*
		 * Free all the properties that were created for the multiple
		 * request.
		 */
		FreeMultiProp(reply);

		reqTbl->done = TRUE;
		/*
		 * If we have added PropertyChangeMask to the win, reset the mask to
		 * it's original state.
		 */
		if (reply->sri_status == TRUE) {
			XGetWindowAttributes(reply->sri_dpy, reply->sri_requestor, &winAttr);

			XSelectInput(reply->sri_dpy, reply->sri_requestor,
					(winAttr.your_event_mask & ~(PropertyChangeMask)));
		}

		if (0 == XFindContext(reply->sri_dpy, sele, selCtx,
								(caddr_t *)&contextdata))
		{
			XDeleteContext(reply->sri_dpy, sele, selCtx);
		}

		xv_sel_free_property(reply->sri_dpy, prop);

		/* here, I always saw reply == reqTbl->reply */
		if (reply->sri_target) xv_free(reply->sri_target);
		XFree((char *)reqTbl->reply);
		reqTbl->reply = NULL;
		return TRUE;
	}
	return FALSE;
}




static int SelFindReply(Sel_reply_info *r1, Sel_reply_info *r2)
{
    /*
     * Make sure it is really it!!
     */
    if ( (r1->sri_requestor == r2->sri_requestor) && (*r1->sri_target == *r2->sri_target) &&
	 (r1->sri_property == r2->sri_property) && (r1->sri_format == r2->sri_format) &&
	 (r1->sri_time == r2->sri_time) && (r2->sri_length == r1->sri_length) &&
	 (r1->sri_selection == r2->sri_selection) )
        return TRUE;
    return FALSE;
}


/*
 * Free all the properties that were created for the multiple
 * request.
 */
static void
FreeMultiProp(Sel_reply_info  *reply)
{
    int            i;

    if ( reply->sri_multiple_count ) {
        for ( i=0; i < reply->sri_multiple_count; i++ )
	    xv_sel_free_property( reply->sri_dpy, reply->sri_atomPair[i].property );
    }
}





/*ARGSUSED*/
Pkg_private int xv_sel_check_property_event(Display *display, XEvent *xevent, char *args)
{
    Sel_reply_info   reply;

    XV_BCOPY( (char *) args, (char *) &reply, sizeof(Sel_reply_info) );

    /*
     * If some other process wants to become the selection owner, let
     * it do so but continue handling the current transaction.
     */
    if ( ( xevent->type & 0177) == SelectionClear )  {
	xv_sel_handle_selection_clear( (XSelectionClearEvent * ) xevent );
	return FALSE;
    }

    if ( ( xevent->type & 0177) == PropertyNotify )  {
	XPropertyEvent *ev = (XPropertyEvent *) xevent;
	if ( ev->state == PropertyNewValue && ev->atom == reply.sri_property &&
		ev->time > reply.sri_time )
	    return ( TRUE );
    }
    return ( FALSE );
}



static XContext cmpatCtx;


Xv_private void xv_sel_set_compat_data(Display *dpy, Atom selection, Window xid, int clientType)
{
}



Pkg_private void xv_sel_free_compat_data(Display *dpy, Atom selection)
{
    Sel_cmpat_info      *cmptInfo, *infoPtr;

    if ( cmpatCtx == 0 )
        cmpatCtx = XUniqueContext();
    if ( XFindContext(dpy, DefaultRootWindow(dpy), cmpatCtx,(caddr_t *)&cmptInfo))
	return;

    infoPtr = cmptInfo;

    do {
	if ( infoPtr->selection == selection ) {
	    infoPtr->selection = (Atom)0;
	    infoPtr->owner = None;
	    infoPtr->clientType = (Atom)0;
	    return;
	}
	if ( infoPtr->next == NULL )
	    break;
	infoPtr = infoPtr->next;
    } while ( 1 );
}


Xv_private int xv_seln_handle_req(Sel_cmpat_info *cmpatInfo, Display *dpy, Atom sel, Atom target, Atom prop, Window req, Time time)
{
    XSelectionRequestEvent  reqEvent;
    Sel_cmpat_info   *infoPtr;

    if ( cmpatInfo == (Sel_cmpat_info *) NULL )
	return FALSE;

    infoPtr = cmpatInfo;
    do {
	if ( infoPtr->selection != sel ) {
	    if ( infoPtr->next == NULL )
		return FALSE;
	    infoPtr = infoPtr->next;
	} else {
	    /*
	     * We found the local selection owner data!!
	     */
    	    reqEvent.display = dpy;
    	    reqEvent.owner = infoPtr->owner;
    	    reqEvent.requestor = req;
    	    reqEvent.selection = infoPtr->selection;
    	    reqEvent.target = target;
    	    reqEvent.property = prop;
    	    reqEvent.time = time;
	    break;
	}
    } while( 1 );

    /*
     * Note that selection data transfer will fail if the request is:
     * MULTIPLE or INCR.
     * This routine is compatibility proc to solve the communication
     * problem between the old selection package and the new one.
     * This routine and all the refrences to it can be deleted after
     * textsw starts using the new selection package.
     */
     xv_sel_handle_selection_request( &reqEvent );
     return TRUE;
}



Pkg_private Sel_cmpat_info  * xv_sel_get_compat_data(Display *dpy)
{
    Sel_cmpat_info  *cmpatInfo;

    if ( XFindContext( dpy, DefaultRootWindow(dpy), cmpatCtx,
			(caddr_t *)&cmpatInfo ))
	return( (Sel_cmpat_info *) NULL );
    return( (Sel_cmpat_info *) cmpatInfo );

}



Xv_private void
xv_sel_send_old_owner_sel_clear(Display *dpy, Atom selection, Window xid, Time time)
{
    Sel_cmpat_info      *cmpatInfo, *infoPtr;

    if ( cmpatCtx == 0 )
        cmpatCtx = XUniqueContext();
    if ( XFindContext(dpy, DefaultRootWindow(dpy), cmpatCtx,(caddr_t *)&cmpatInfo))
	return;

    infoPtr = cmpatInfo;
    do {
	if ( (infoPtr->selection == selection) && (infoPtr->owner != xid) ) {
	    XSelectionClearEvent  clrEvent;

	    clrEvent.display = dpy;
	    clrEvent.window = infoPtr->owner;
	    clrEvent.selection = selection;
	    clrEvent.time = time;

	    xv_sel_handle_selection_clear( &clrEvent );
	}
	if ( infoPtr->next == NULL )
	    break;
	infoPtr = infoPtr->next;
    }while (1);
}


