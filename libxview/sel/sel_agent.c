#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)sel_agent.c 1.81 93/06/29 DRA: $Id: sel_agent.c,v 4.12 2025/01/26 22:08:57 dra Exp $";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL_NOTICE 
 *	file for terms of the license.
 */


#include <xview/xview.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <xview/server.h>
#include <xview/defaults.h>
#include <xview/sel_svc.h>
#include <xview/sel_attrs.h>
#include <xview_private/i18n_impl.h>
#include <xview_private/portable.h>
#include <xview_private/seln_impl.h>
#include <xview_private/sel_impl.h>
#include <xview_private/svr_impl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/file.h>
#include <xview/sel_compat.h>
#include <X11/Xatom.h>
#include <stdio.h>

extern char *xv_app_name;
#define XX(_a_,_n_) SERVERTRACE((333, "%s: %s %ld\n", xv_app_name, _a_, (long)_n_))

#define XXATOM(_a_) SERVERTRACE((333, "%s: %s\n", xv_app_name, (char *)xv_get(xv_default_server, SERVER_ATOM_NAME, _a_)))

/*
 * The following header file provides fd_set compatibility with SunOS for
 * Ultrix
 */
#include <xview_private/ultrix_cpt.h>
#if defined(SVR4) || defined(__linux)
#include <stdlib.h>
#include <unistd.h>
#endif /* SVR4 */

#define DRA_stderr stderr

extern int seln_debug;

/* in windowutil.c */
Xv_private void selection_agent_clear(Xv_Server server, XSelectionClearEvent *clear_event)
{
	selection_unsupported(__FUNCTION__);
}

Xv_private void selection_agent_selectionrequest(Xv_Server srv,
									XSelectionRequestEvent *req_event)
{
	selection_unsupported(__FUNCTION__);
}
