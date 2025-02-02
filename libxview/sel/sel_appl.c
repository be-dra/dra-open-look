#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)sel_appl.c 20.29 93/06/28 DRA: $Id: sel_appl.c,v 4.5 2025/02/02 19:10:47 dra Exp $";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

#include <xview_private/seln_impl.h>

Xv_public Seln_request * seln_ask(Seln_holder *holder, ...)
{
	selection_unsupported(__FUNCTION__);
    return NULL;
}

Xv_public Seln_request *selection_ask(Xv_Server server,Seln_holder *holder, ...)
{
	selection_unsupported(__FUNCTION__);
	return &seln_null_request;
}

Xv_public void seln_init_request(Seln_request *buffer, Seln_holder *holder, ...)
{
	selection_unsupported(__FUNCTION__);
}

Xv_public void selection_init_request(Xv_Server server, Seln_request *buffer,
                       Seln_holder *holder, ...)
{
	selection_unsupported(__FUNCTION__);
}

Xv_public Seln_result seln_query(Seln_holder *holder,
					Seln_result (*reader) (Seln_request *), char *context, ...)
{
	selection_unsupported(__FUNCTION__);
    return SELN_FAILED;
}

Xv_public Seln_result selection_query(Xv_Server server, Seln_holder *holder,
           Seln_result (*reader) (Seln_request *), char *context, ...)
{
	selection_unsupported(__FUNCTION__);
    return SELN_FAILED;
}
