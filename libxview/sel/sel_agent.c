#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)sel_agent.c 1.81 93/06/29 DRA: $Id: sel_agent.c,v 4.13 2025/02/02 18:36:06 dra Exp $";
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
#include <xview_private/seln_impl.h>
#include <sys/types.h>
#include <xview/sel_compat.h>
#include <stdio.h>

Xv_private void selection_agent_clear(Xv_Server server, XSelectionClearEvent *clear_event)
{
	selection_unsupported(__FUNCTION__);
}

Xv_private void selection_agent_selectionrequest(Xv_Server srv,
									XSelectionRequestEvent *req_event)
{
	selection_unsupported(__FUNCTION__);
}
