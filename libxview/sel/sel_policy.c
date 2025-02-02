#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)sel_policy.c 20.20 93/06/28 DRA: $Id: sel_policy.c,v 4.5 2025/02/02 19:10:47 dra Exp $";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

#include <xview_private/seln_impl.h>

Xv_public Seln_response selection_figure_response(Xv_Server server, Seln_function_buffer *buffer, Seln_holder **holder)
{
	selection_unsupported(__FUNCTION__);
	return SELN_IGNORE;
}

Xv_public void selection_report_event(Xv_Server server, Seln_client seln_client, struct inputevent *event)
{
	selection_unsupported(__FUNCTION__);
}
