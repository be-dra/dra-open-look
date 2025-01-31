/*	@(#)seln_impl.h 20.38 93/06/28		DRA: $Id: seln_impl.h,v 4.5 2025/01/26 22:08:57 dra Exp $ */

#ifndef	suntool_selection_impl_DEFINED
#define	suntool_selection_impl_DEFINED

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */

#include <errno.h>
#ifndef FILE
#if !defined(SVR4) && !defined(__linux)
#undef NULL
#endif  /* SVR4 */
#include <stdio.h>
#endif  /* FILE */
#include <sys/time.h>
#include <sys/types.h>
#include <netdb.h>
#include <xview_private/i18n_impl.h>
#include <xview/notify.h>
#include <xview/pkg.h>
#include <xview/sel_pkg.h>
#include <xview/sel_svc.h>
#include <xview/sel_attrs.h>
#include <X11/Xlib.h>

/*
 * Procedure IDs for client-module procedures
 */

#define SELN_CLNT_REQUEST	17
#define SELN_CLNT_DO_FUNCTION	18


/*	initializers		*/

#define SELN_NULL_ACCESS { 0, 0, {0}, {0}, 0}
#define SELN_NULL_HOLDER { SELN_UNKNOWN, SELN_NONE, SELN_NULL_ACCESS}
#define SELN_STD_TIMEOUT_SEC 	4
#define SELN_STD_TIMEOUT_USEC 	0	/* 4 sec timeout on connections  */


#define complain(str)	\
	(void)fprintf(stderr, XV_MSG("Selection library internal error:\n%s\n"), XV_MSG(str))

typedef struct {
    void	    (*do_function)(Xv_opaque, Seln_function_buffer *);
    Seln_result	    (*do_request)( Seln_attribute, Seln_replier_data *, int);
}	Seln_client_ops;


typedef struct client_node    {
    Seln_client_ops	 ops;  /* How we pass requests to client  */
    char		*client_data;
    Seln_access		 access;
    struct client_node	*next;
    unsigned		client_num; /* this client is the (client_num)th
    				     * client for this selection library
				     */
}	Seln_client_node;


#define HIST_SIZE	50

#ifdef OW_I18N
typedef struct {
    unsigned char       first_time;
    unsigned char       event_sent;
    XID                 requestor;
    Atom                property;
    Atom                selection;
    Atom                target;
    Display             *display;
    int                 chars_remaining;
    Time                timestamp;
    unsigned char       format;
    CHAR                *buffer;
    int                 offset;
} Seln_agent_context;
#else
typedef struct {
    unsigned char     	first_time;
    unsigned char	event_sent;
    XID         	requestor;
    Atom		property;
    Atom		selection;
    Atom		target;
    Display		*display;
    int			bytes_remaining;
    Time		timestamp;
    unsigned char	format;
} Seln_agent_context;
#endif  /* OW_I18N */

typedef struct {
    long		offset;
    Atom		property;/* Property returned after XConvertSelection*/
} Seln_agent_getprop;
#define	SELN_RANKS	((u_int)SELN_UNSPECIFIED)

typedef union {
	struct {
    Atom	length;
    Atom	contents_pieces;
    Atom	first;
    Atom	first_unit;
    Atom	last;
    Atom	last_unit;
    Atom	level;
    Atom	file_name;
    Atom	commit_pending_delete;
    Atom	delete;
    Atom	restore;
    Atom	yield;
    Atom	fake_level;
    Atom	set_level;
    Atom	end_request;
    Atom	targets;
    Atom	do_function;
    Atom	multiple;
    Atom	timestamp;
    Atom	string;
    Atom	is_readonly;
    Atom	func_key_state;
    Atom	selected_windows;
    Atom	object_content;
    Atom	object_size;
    Atom	sel_end;
#ifdef OW_I18N
    Atom        length_chars;
    Atom	first_wc;
    Atom	last_wc;
    Atom        compound_text;
#endif
	} s;
#ifdef OW_I18N
	Atom a[30];
#else
	Atom a[26];
#endif
} Seln_target_atoms;
#define SELN_PROPERTY	31

typedef struct {
    Seln_agent_context	req_context;
    Seln_holder		client_holder[SELN_RANKS];
    int			held_file[SELN_RANKS];
    Seln_holder		agent_holder;
    Time		seln_acquired_time[SELN_RANKS];
    XID			xid;
    Seln_agent_getprop	get_prop;
    Seln_target_atoms	targets;
    Atom		property[SELN_PROPERTY];
    Atom		clipboard;
    Atom		caret;
    int			timeout;	/* Timeout in secs */
} Seln_agent_info;

Xv_private void selection_agent_clear(Xv_Server server, XSelectionClearEvent *clear_event);
Pkg_private void selection_unsupported(const char *func);
Pkg_private Seln_result selection_send_yield(Xv_Server server,
    Seln_rank rank, Seln_holder *holder);
Xv_private void selection_agent_clear(Xv_Server server,
									XSelectionClearEvent *clear_event);
Xv_private void selection_agent_selectionrequest(Xv_Server srv,
									XSelectionRequestEvent *req_event);

#endif
