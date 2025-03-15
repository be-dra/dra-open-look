#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)win_cntral.c 20.20 93/06/28 DRA: $Id: win_cntral.c,v 4.5 2025/03/13 09:40:57 dra Exp $";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Implements library routines for centralized window event management.
 */

#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>
#include <xview_private/windowimpl.h>
#include <xview/notify.h>
#include <xview/rect.h>
#include <xview/win_input.h>
#include <xview/win_notify.h>

static Notify_error win_send(Notify_client client, Event *event, Notify_event_type when, Notify_arg arg, Notify_copy copy_func, Notify_release release_func);

/*
 * Public interface:
 */

Notify_error win_post_id(Notify_client client, int id, Notify_event_type when)
{
    Event           event;

    event_init(&event);
    event_set_id(&event, id);
    event_set_window(&event, client);
    return win_send(client, &event, when, (Notify_arg)0, 
				(Notify_copy)win_copy_event, (Notify_release)win_free_event);
}


Notify_error
win_post_id_and_arg(client, id, when, arg, copy_func, release_func)
    Notify_client   client;
    int           id;
    Notify_event_type when;
    Notify_arg      arg;
    Notify_copy     copy_func;
    Notify_release  release_func;
{
    Event           event;

    event_init(&event);
    event_set_id(&event, id);
    event_set_window(&event, client);
    return (win_send(client, &event, when, arg, copy_func,
		     release_func));
}

Notify_error
win_post_event(client, event, when)
    Notify_client   client;
    Event          *event;
    Notify_event_type when;
{
    /* Send event */
    return (win_send(client, event, when, (Notify_arg) 0,
		     (Notify_copy)win_copy_event, (Notify_release)win_free_event));
}

Notify_error
win_post_event_arg(client, event, when, arg, copy_func, release_func)
    Notify_client   client;
    Event          *event;
    Notify_event_type when;
    Notify_arg      arg;
    Notify_copy     copy_func;
    Notify_release  release_func;
{
    /* Send event */
    return (win_send(client, event, when, arg, copy_func, release_func));
}

Notify_arg win_copy_event(Notify_client client, Notify_arg arg, Event **event_ptr)
{
	Event *event_new;

	if (*event_ptr != EVENT_NULL) {
		XEvent *xev = event_xevent(*event_ptr);
		event_new = (Event *) (xv_malloc(sizeof(Event)));
		*event_new = **event_ptr;
		/* dear people: what about a potential event_xevent ????
		 * how can this ever have worked??? Obviously a lot of event
		 * handlers never access event_xevent(ev).
		 */
		if (xev) {
			XEvent *newxev = (XEvent *) (xv_malloc(sizeof(XEvent)));
			*newxev = *xev;
			event_set_xevent(event_new, newxev);
		}
		*event_ptr = event_new;
	}
	return (arg);
}

void win_free_event(Notify_client client, Notify_arg arg, Event *event)
{
    if (event != EVENT_NULL) {
		XEvent *xev = event_xevent(event);

		if (xev) xv_free(xev);
		free((caddr_t) event);
	}
}

/*
 * Private to this module:
 */

static Notify_error win_send(Notify_client client, Event *event, Notify_event_type when, Notify_arg arg, Notify_copy copy_func, Notify_release release_func)
{
    Notify_error    error;

    /*
     * keymap the event.  Note that we assume the client is a window.
     */
    /*
     * (void)win_keymap_map(client, event);
     */

    /* Post event */
    /* Post immediately if in xv_window_loop */
    error = notify_post_event_and_arg(client, (Notify_event) event,
				      WIN_IS_IN_LOOP ? NOTIFY_IMMEDIATE : when, 
				      arg, copy_func, release_func);
    if (error != NOTIFY_OK) {
		char errbuf[200];

		sprintf(errbuf, "win_send: client = %lx", client);
		notify_perror(errbuf);
	}
    return (error);
}
