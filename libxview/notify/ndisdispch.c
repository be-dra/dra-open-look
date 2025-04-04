#ifndef	lint
#ifdef sccs
static char     sccsid[] = "@(#)ndisdispch.c 20.22 93/06/28 Copyr 1985 Sun Micro  DRA: $Id: ndisdispch.c,v 4.3 2025/03/29 21:01:10 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */

/*
 * Ndis_dispatch.c - Central control mechanism for the dispatcher.
 */

#include <xview_private/i18n_impl.h>
#include <xview_private/ntfy.h>
#include <xview_private/ndis.h>
#include <xview_private/nint.h>
#include <xview_private/ndet.h>	/* For ndet_set_event_processing/ndet_flags */
#include <signal.h>
#if defined(SVR4) || defined(__linux)
#include <unistd.h>
#endif  /* SVR4 */

/* performance: global cache of getdtablesize() */
int             dtablesize_cache = 0;
#if defined(SVR4) || defined(__linux)
#define GETDTABLESIZE() \
(dtablesize_cache?dtablesize_cache:(dtablesize_cache=(int)sysconf(_SC_OPEN_MAX)))
#else
#define GETDTABLESIZE() \
 (dtablesize_cache?dtablesize_cache:(dtablesize_cache=getdtablesize()))
#endif  /* SVR4 */

pkg_private_data u_int ndis_flags = 0;
pkg_private_data NTFY_CLIENT *ndis_clients = 0;
pkg_private_data NTFY_CLIENT *ndis_client_latest = 0;

pkg_private_data Notify_scheduler_func ndis_scheduler = ndis_default_scheduler;

static Notify_client *ndis_sched_nclients;	/* Parallel data structure to
						 * ndis_clients for use with
						 * scheduler */
static u_int    ndis_sched_count;	/* Last valid position (plus one) in
					 * ndis_sched_nclients */
static u_int    ndis_sched_length;	/* Current length of
					 * ndis_sched_nclients */

static Notify_event *ndis_events;	/* List of events currently being
					 * dispatched for a given client */
static Notify_arg *ndis_args;		/* List of event args currently being
					 * dispatched for a given client
					 * (parallels ndis_events) */
static int ndis_event_count;	/* Last valid position (plus one) in
					 * ndis_events & ndis_args */
static u_int    ndis_event_length;	/* Current length of ndis_events &
					 * ndis_args */

static Notify_error ndis_send_func(Notify_client nclient, NTFY_TYPE type,
								NTFY_DATA data, int use_data,
								Notify_func *func_ptr, NTFY_DATA *data_ptr,
								Notify_release *release_func_ptr);	/* Used to get func for sending notify */

#define	NDIS_RELEASE_NULL	((Notify_release *)0)

static NTFY_ENUM ndis_setup_sched_clients(NTFY_CLIENT *client,
    NTFY_CONDITION *condition, NTFY_ENUM_DATA  context);

/* Enqueue condition from dectector on to the dispatchers client list */
pkg_private Notify_error ndis_enqueue(NTFY_CLIENT *ndet_client,
									NTFY_CONDITION *ndet_condition)
{
	register NTFY_CLIENT *client;
	register NTFY_CONDITION *condition;

	ntfy_assert(NTFY_IN_CRITICAL, 18
			/* Not protected when enqueue condition */ );
	/* Find/create client that corresponds to ndet_client */
	if ((client = ntfy_new_nclient(&ndis_clients, ndet_client->nclient,
							&ndis_client_latest)) == NTFY_CLIENT_NULL)
		goto Error;
	client->prioritizer = ndet_client->prioritizer;
	/* Allocate condition */
	if ((condition = ntfy_alloc_condition()) == NTFY_CONDITION_NULL)
		goto Error;
	/* Initialize condition */
	condition->next = NTFY_CONDITION_NULL;
	condition->type = ndet_condition->type;
	condition->release = ndet_condition->release;
	condition->arg = ndet_condition->arg;
	switch (condition->type) {
		case NTFY_REAL_ITIMER:
		case NTFY_VIRTUAL_ITIMER:
			condition->data.an_u_long = 0;
			break;
		case NTFY_WAIT3:
			if ((condition->data.wait3 =
							(NTFY_WAIT3_DATA *)
							ntfy_malloc((u_int)sizeof(NTFY_WAIT3_DATA))) ==
					NTFY_WAIT3_DATA_NULL)
				goto Error;
			*condition->data.wait3 = *ndet_condition->data.wait3;
			break;
		default:
			condition->data.an_u_long = ndet_condition->data.an_u_long;
			break;
	}
	condition->func_count = ndet_condition->func_count;
	condition->func_next = 0;
	if (nint_copy_callout(condition, ndet_condition) != NOTIFY_OK)
		goto Error;
	/* Append to condition list */
	ntfy_append_condition(&(client->conditions), condition);
	/* Set up condition hint */
	client->condition_latest = condition;
	/* Set dispatcher flag */
	ndis_flags |= NDIS_EVENT_QUEUED;
	return (NOTIFY_OK);
  Error:
	return (notify_errno);
}

/* Dispatch to all clients according to the scheduler */
pkg_private     Notify_error ndis_dispatch(void)
{
    Notify_scheduler_func sched_func;

    ntfy_assert(!NTFY_IN_CRITICAL, 19
		/* In critical when dispatch */);
    ntfy_assert((!NTFY_IN_INTERRUPT || NTFY_DEAF_INTERRUPT), 20
		/* In interrupt when dispatch */);
    NTFY_BEGIN_CRITICAL;
    /* Build nclient list for scheduler */
    for (;;) {
	ndis_sched_count = 0;
	/* If enumerator returns NTFY_ENUM_TERM then list too short */
	if (ntfy_enum_conditions(ndis_clients, ndis_setup_sched_clients,
				 NTFY_ENUM_DATA_NULL) == NTFY_ENUM_TERM) {
	    /* Free previous list */
	    if (ndis_sched_nclients)
		ntfy_free_malloc(
				 (NTFY_DATA) ndis_sched_nclients);
	    /* Allocate new list */
	    ndis_sched_length += 20;
	    if ((ndis_sched_nclients = (Notify_client *)
		 ntfy_malloc((u_int)(ndis_sched_length * sizeof(Notify_client))))
		== (Notify_client *) 0) {
		NTFY_END_CRITICAL;
		return (notify_errno);
	    }
	} else
	    break;
    }
    /* Call scheduler */
    sched_func = ndis_scheduler;
    NTFY_END_CRITICAL;
    if (sched_func((int)ndis_sched_count, ndis_sched_nclients) ==
	NOTIFY_UNEXPECTED)
	/*
	 * Scheduler sets notify_errno and returns error if there is some
	 * non-callout related problem.
	 */
	return (notify_errno);
    return (NOTIFY_OK);
}

/* ARGSUSED */
static NTFY_ENUM ndis_setup_sched_clients(NTFY_CLIENT *client,
    NTFY_CONDITION *condition, NTFY_ENUM_DATA  context)
{
    /* Terminate enumeration if next slot overflows size of client table */
    if (ndis_sched_count == ndis_sched_length)
	return (NTFY_ENUM_TERM);
    *(ndis_sched_nclients + ndis_sched_count) = client->nclient;
    ndis_sched_count++;
    return (NTFY_ENUM_SKIP);
}

/* Flush given nclient & its conditions */
extern void
notify_flush_pending(nclient)
    Notify_client   nclient;
{
    NTFY_CLIENT    *client;
    register int    i;

    NTFY_BEGIN_CRITICAL;
    if ((client = ntfy_find_nclient(ndis_clients, nclient,
				  &ndis_client_latest)) == NTFY_CLIENT_NULL)
	goto Done;
    else {
	/* Flush any pending notification from dispatcher */
	ntfy_remove_client(&ndis_clients, client, &ndis_client_latest,
			   NTFY_NDIS);
	/* Remove nclient from list of clients to be scheduled */
	for (i = 0; i < ndis_sched_length; i++)
	    if (ndis_sched_nclients[i] == nclient)
		ndis_sched_nclients[i] = NOTIFY_CLIENT_NULL;
	/*
	 * The dispatcher may be calling out to an nclient at this time. When
	 * an nclient calls back in, we always check the validity of the
	 * handle.  Thus, the dispatcher ignores any further references to
	 * client.
	 */
    }
Done:
    NTFY_END_CRITICAL;
    return;
}

Notify_error notify_client(Notify_client   nclient)
{
	NTFY_CLIENT *client;
	register NTFY_CONDITION *cdn;
	sigset_t sigbits, auto_sigbits;
	fd_set ibits, obits, ebits;

	/* Don't make register because take address of them */
	Notify_prioritizer_func pri_func;
	int maxfds = GETDTABLESIZE();

	/* Check if heap access protected */
	ntfy_assert((!NTFY_IN_INTERRUPT || NTFY_DEAF_INTERRUPT), 21
			/* In interrupt when notify */ );
	NTFY_BEGIN_CRITICAL;
  Retry_Client:
	/* Find client */
	if ((client = ntfy_find_nclient(ndis_clients, nclient,
							&ndis_client_latest)) == NTFY_CLIENT_NULL)
		/* Can get here on first time if client removed pending cond */
		goto Done;
	/* Reset flags that detects when another condition enqueued */
	ndis_flags &= ~NDIS_EVENT_QUEUED;
  Retry_Conditions:
	/* Initialize prioritizer arguments */
	sigemptyset(&sigbits);
	sigemptyset(&auto_sigbits);
	FD_ZERO(&ibits);
	FD_ZERO(&obits);
	FD_ZERO(&ebits);
	ndis_event_count = 0;
	/* Fill out prioritizer arguments based on conditions */
	for (cdn = client->conditions; cdn; cdn = cdn->next) {
		switch (cdn->type) {
			case NTFY_INPUT:
				FD_SET(cdn->data.fd, &ibits);
				break;
			case NTFY_OUTPUT:
				FD_SET(cdn->data.fd, &obits);
				break;
			case NTFY_EXCEPTION:
				FD_SET(cdn->data.fd, &ebits);
				break;
			case NTFY_SYNC_SIGNAL:
				sigaddset(&sigbits, cdn->data.signal);
				break;
			case NTFY_REAL_ITIMER:
				sigaddset(&auto_sigbits, SIGALRM);
				break;
			case NTFY_VIRTUAL_ITIMER:
				sigaddset(&auto_sigbits, SIGVTALRM);
				break;
			case NTFY_WAIT3:
				sigaddset(&auto_sigbits, SIGCHLD);
				break;
			case NTFY_DESTROY:
				switch (cdn->data.status) {
					case DESTROY_CLEANUP:
						sigaddset(&auto_sigbits, SIGTERM);
						break;
					case DESTROY_PROCESS_DEATH:
						sigaddset(&auto_sigbits, SIGKILL);
						break;
					case DESTROY_CHECKING:
						sigaddset(&auto_sigbits, SIGTSTP);
						break;
					case DESTROY_SAVE_YOURSELF:
						sigaddset(&auto_sigbits, SIGUSR1);
						break;
				}
				break;
			case NTFY_SAFE_EVENT:
				/*
				 * Build event list for prioritizer. Terminate condition
				 * traversal if next slot overflows size of event table.
				 */
				if (ndis_event_count == ndis_event_length) {
					/* Free previous list */
					if (ndis_events) {
						ntfy_free_malloc((NTFY_DATA) ndis_events);
						ntfy_free_malloc((NTFY_DATA) ndis_args);
					}
					/* Allocate new list */
					ndis_event_length += 20;
					if ((ndis_events = (Notify_event *)
									ntfy_malloc((u_int)(ndis_event_length *
											sizeof(Notify_event)))) ==
							(Notify_event *) 0)
						goto Error;
					if ((ndis_args = (Notify_arg *)
									ntfy_malloc((u_int)(ndis_event_length *
											sizeof(Notify_arg)))) ==
							(Notify_arg *) 0)
						goto Error;
					/* Restart condition traversal */
					goto Retry_Conditions;
				}
				*(ndis_events + ndis_event_count) = cdn->data.event;
				*(ndis_args + ndis_event_count) = cdn->arg;
				ndis_event_count++;
				break;
			default:
				ntfy_fatal_error(XV_MSG("Unexpected dispatcher cond"));
		}
	}
	/* Call prioritizer */
	pri_func = client->prioritizer;
	NTFY_END_CRITICAL;
	/* the second arg to pri_func should be max fd # for select call ? ? ? */
	(void)pri_func(nclient, maxfds, &ibits, &obits, &ebits,
			NSIG, &sigbits, &auto_sigbits, &ndis_event_count, ndis_events,
			ndis_args);
	NTFY_BEGIN_CRITICAL;
	/* Check for NDIS_EVENT_QUEUED and redo loop */
	if (ndis_flags & NDIS_EVENT_QUEUED)
		goto Retry_Client;
  Done:
	NTFY_END_CRITICAL;
	return (NOTIFY_OK);
  Error:
	NTFY_END_CRITICAL;
	return (notify_errno);
}

static Notify_error notify_fd(Notify_client nclient, int fd, NTFY_TYPE type)
{
    Notify_func     func;

    /* Check arguments and get function to call */
    if (ndet_check_fd(fd) ||
	ndis_send_func(nclient, type, (NTFY_DATA)((long)fd), NTFY_USE_DATA,
		 &func, NTFY_DATA_PTR_NULL, NDIS_RELEASE_NULL) != NOTIFY_OK)
	return (notify_errno);
    (void) func(nclient, fd);
    /* Pop condition off interposition stack */
    nint_pop_callout();
    return (NOTIFY_OK);
}

extern          Notify_error
notify_input(nclient, fd)
    Notify_client   nclient;
    int             fd;
{
    return (notify_fd(nclient, fd, NTFY_INPUT));
}

extern          Notify_error
notify_output(nclient, fd)
    Notify_client   nclient;
    int             fd;
{
    return (notify_fd(nclient, fd, NTFY_OUTPUT));
}

extern          Notify_error
notify_exception(nclient, fd)
    Notify_client   nclient;
    int             fd;
{
    return (notify_fd(nclient, fd, NTFY_EXCEPTION));
}

extern          Notify_error
notify_itimer(nclient, which)
    Notify_client   nclient;
    int             which;
{
    NTFY_TYPE       type;
    Notify_func     func;

    /* Check arguments and get function to call */
    if (ndet_check_which(which, &type) ||
	ndis_send_func(nclient, type, NTFY_DATA_NULL, NTFY_IGNORE_DATA,
		 &func, NTFY_DATA_PTR_NULL, NDIS_RELEASE_NULL) != NOTIFY_OK)
	return (notify_errno);
    (void) func(nclient, which);
    /* Pop condition off interposition stack */
    nint_pop_callout();
    return (NOTIFY_OK);
}

extern          Notify_error
notify_event(nclient, event, arg)
    Notify_client   nclient;
    Notify_event    event;
    Notify_arg      arg;
{
    Notify_func     func;
    Notify_release  release_func;

    /* Get function to call */
    if (ndis_send_func(nclient, NTFY_SAFE_EVENT, (NTFY_DATA) event,
		 NTFY_USE_DATA, &func, NTFY_DATA_PTR_NULL, &release_func) !=
	NOTIFY_OK)
	return (notify_errno);
    (void) ndet_set_event_processing(nclient, 1);
    (void) func(nclient, event, arg, NOTIFY_SAFE);
    (void) ndet_set_event_processing(nclient, 0);
    /* Pop condition off interposition stack */
    nint_pop_callout();
    /* Release arg and event */
    if (release_func != NOTIFY_RELEASE_NULL)
	(void) release_func(nclient, arg, event);
    return (NOTIFY_OK);
}

extern          Notify_error
notify_signal(nclient, sig)
    Notify_client   nclient;
    int             sig;
{
    Notify_func     func;

    /* Check arguments and get function to call */
    if (ndet_check_sig(sig) ||
	ndis_send_func(nclient, NTFY_SYNC_SIGNAL, (NTFY_DATA)((long)sig),
	     NTFY_USE_DATA, &func, NTFY_DATA_PTR_NULL, NDIS_RELEASE_NULL) !=
	NOTIFY_OK)
	return (notify_errno);
    (void) func(nclient, sig, NOTIFY_SYNC);
    /* Pop condition off interposition stack */
    nint_pop_callout();
    return (NOTIFY_OK);
}

extern          Notify_error
notify_destroy(nclient, status)
    Notify_client   nclient;
    Destroy_status  status;
{
    Notify_func     func;

    /* Check arguments and get function to call */
    if (ndet_check_status(status) ||
	ndis_send_func(nclient, NTFY_DESTROY, NTFY_DATA_NULL,
	  NTFY_IGNORE_DATA, &func, NTFY_DATA_PTR_NULL, NDIS_RELEASE_NULL) !=
	NOTIFY_OK)
	return (notify_errno);
    ndet_flags &= ~NDET_VETOED;	/* Note: Unprotected */
    (void) func(nclient, status);
    /* Pop condition off interposition stack */
    nint_pop_callout();
    if ((status == DESTROY_CHECKING) || (status == DESTROY_SAVE_YOURSELF)) {
	if (ndet_flags & NDET_VETOED)
	    return (NOTIFY_DESTROY_VETOED);
    } else if (status != DESTROY_SAVE_YOURSELF) {
	NTFY_BEGIN_CRITICAL;
	/* remove client (if client hasn't done so himself) */
	if (ntfy_find_nclient(ndet_clients, nclient,
			      &ndet_client_latest) != NTFY_CLIENT_NULL) {
	    NTFY_END_CRITICAL;
	    return (notify_remove(nclient));
	}
	NTFY_END_CRITICAL;
    }
    return (NOTIFY_OK);
}

extern          Notify_error
notify_wait3(nclient)
    Notify_client   nclient;
{
    Notify_func     func;
    NTFY_WAIT3_DATA *wd;
    NTFY_CLIENT    *client;

    /* Loop until no more wait conditions to notify */
    for (;;) {
	/*
	 * See if WAIT3 condition is pending.  This WAIT3 notify proc differs
	 * from the others in this module because a client might have
	 * multiple wait3 events pending but the prioritizer only can tell if
	 * there is any waiting at all.  Thus we must search out ALL the
	 * WAIT3 conditions. Protect as critical section because list
	 * searches check to make sure that in critical.
	 */
	NTFY_BEGIN_CRITICAL;
	if ((client = ntfy_find_nclient(ndis_clients, nclient,
				&ndis_client_latest)) == NTFY_CLIENT_NULL ||
	    ntfy_find_condition(client->conditions, NTFY_WAIT3,
				&(client->condition_latest), NTFY_DATA_NULL,
				NTFY_IGNORE_DATA) == NTFY_CONDITION_NULL) {
	    NTFY_END_CRITICAL;
	    break;
	}
	NTFY_END_CRITICAL;
	/* Get function to call */
	if (ndis_send_func(nclient, NTFY_WAIT3, NTFY_DATA_NULL,
			   NTFY_IGNORE_DATA, &func, (NTFY_DATA *) & wd,
			   NDIS_RELEASE_NULL) != NOTIFY_OK)
	    return (notify_errno);
	(void) func(nclient, wd->pid, &wd->status, &wd->rusage);
	/* Free wait record */
	NTFY_BEGIN_CRITICAL;
	/* Pop condition off interposition stack */
	nint_unprotected_pop_callout();
	ntfy_free_malloc((NTFY_DATA) wd);
	NTFY_END_CRITICAL;
    }
    return (NOTIFY_OK);
}

static Notify_error ndis_send_func(Notify_client   nclient,
    NTFY_TYPE       type,
    NTFY_DATA       data,
    int             use_data,
    Notify_func    *func_ptr,
    NTFY_DATA      *data_ptr,
    Notify_release *release_func_ptr)
{
    register NTFY_CLIENT *client;
    register NTFY_CONDITION *condition;

    NTFY_BEGIN_CRITICAL;
    /* Find client */
    if ((client = ntfy_find_nclient(ndis_clients, nclient,
				&ndis_client_latest)) == NTFY_CLIENT_NULL) {
	ntfy_set_warning(NOTIFY_UNKNOWN_CLIENT);
	goto Error;
    }
    /* Find condition */
    if ((condition = ntfy_find_condition(client->conditions, type,
			    &(client->condition_latest), data, use_data)) ==
	NTFY_CONDITION_NULL) {
	ntfy_set_warning(NOTIFY_NO_CONDITION);
	goto Error;
    }
    /* Remember function */
    *func_ptr = nint_push_callout(client, condition);
    /* Snatch condition data and null it */
    if (data_ptr) {
	*data_ptr = (NTFY_DATA)((long) condition->data.an_u_long);
	condition->data.an_u_long = 0;
    }
    /* Remember release function so can use later and null it */
    if (release_func_ptr) {
	*release_func_ptr = condition->release;
	condition->release = NOTIFY_RELEASE_NULL;
    }
    /* Remove condition */
    ntfy_unset_condition(&ndis_clients, client, condition,
			 &ndis_client_latest, NTFY_NDIS);
    NTFY_END_CRITICAL;
    return (NOTIFY_OK);
Error:
    NTFY_END_CRITICAL;
    return (notify_errno);
}

pkg_private void ndis_flush_condition(Notify_client nclient, NTFY_TYPE type,
										NTFY_DATA data, int use_data)
{
    register NTFY_CLIENT *client;
    register NTFY_CONDITION *condition;

    NTFY_BEGIN_CRITICAL;
    for (;;) {
	/* Find client */
	if ((client = ntfy_find_nclient(ndis_clients, nclient,
				  &ndis_client_latest)) == NTFY_CLIENT_NULL)
	    break;
	/* Find condition */
	if ((condition = ntfy_find_condition(client->conditions, type,
			    &(client->condition_latest), data, use_data)) ==
	    NTFY_CONDITION_NULL)
	    break;
	/* Remove condition */
	ntfy_unset_condition(&ndis_clients, client, condition,
			     &ndis_client_latest, NTFY_NDIS);
    }
    NTFY_END_CRITICAL;
}

pkg_private void ndis_flush_wait3(Notify_client  nclient, int pid)
{
    register NTFY_CLIENT *client;
    register NTFY_CONDITION *condition;
    register NTFY_CONDITION *next;

    NTFY_BEGIN_CRITICAL;
    /* Find client */
    if ((client = ntfy_find_nclient(ndis_clients, nclient,
				  &ndis_client_latest)) == NTFY_CLIENT_NULL)
	goto Done;
    /* Find condition */
    for (condition = client->conditions; condition; condition = next) {
	next = condition->next;
	if (condition->type == NTFY_WAIT3 &&
	    condition->data.wait3->pid == pid) {
	    /* Remove condition */
	    ntfy_unset_condition(&ndis_clients, client, condition,
				 &ndis_client_latest, NTFY_NDIS);
	    break;
	}
    }
Done:
    NTFY_END_CRITICAL;
}
