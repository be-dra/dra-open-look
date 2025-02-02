#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)sel_clnt.c 20.41 93/06/29 DRA: $Id: sel_clnt.c,v 4.5 2025/02/02 19:10:47 dra Exp $";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

#include <xview_private/seln_impl.h>

int seln_debug;

Pkg_private void selection_unsupported(const char *func)
{
	char buf[100];
	sprintf(buf,
		"Invalid call to function %s, old selection service no longer supported.",
		func);
	xv_error(XV_NULL,
			ERROR_LAYER, ERROR_PROGRAM,
			ERROR_STRING, buf,
			ERROR_SEVERITY, ERROR_RECOVERABLE,
			NULL);
}

/*
 * External procedures (called by our client)
 * 
 * 
 * Create:  generate & store client identification; initialize per-process
 * information (socket, service transport handle, ...) if this is the first
 * client.
 */
Xv_public Seln_client selection_create(Xv_Server       server,
    void (*function_proc)(Xv_opaque, Seln_function_buffer *),
	Seln_result (*request_proc)(Seln_attribute, Seln_replier_data *, int),
    char *client_data)
{
	selection_unsupported(__FUNCTION__);
    return (char *)NULL;
}

Xv_public void selection_destroy(Xv_Server server, char *client)
{
	selection_unsupported(__FUNCTION__);
}

Xv_public Seln_rank selection_acquire(Xv_Server       server,
    Seln_client     seln_client,
    Seln_rank       asked)
{
	selection_unsupported(__FUNCTION__);
	return SELN_SHELF;
}

Xv_public Seln_result selection_done(Xv_Server server, Seln_client seln_client,
    Seln_rank rank)
{
	selection_unsupported(__FUNCTION__);
	return SELN_FAILED;
}

Xv_public void selection_yield_all(Xv_Server server)
{
	selection_unsupported(__FUNCTION__);
}

Xv_public Seln_function_buffer selection_inform(Xv_Server server,
    Seln_client seln_client, Seln_function which, int down)
{
	selection_unsupported(__FUNCTION__);
	return seln_null_function;
}

Xv_public Seln_holder selection_inquire(Xv_Server server, Seln_rank which)
{
	selection_unsupported(__FUNCTION__);
	return seln_null_holder;
}

Xv_public Seln_holders_all selection_inquire_all(Xv_Server server)
{
    Seln_holders_all result = { SELN_NULL_HOLDER,
								SELN_NULL_HOLDER,
								SELN_NULL_HOLDER,
								SELN_NULL_HOLDER };
	selection_unsupported(__FUNCTION__);
    return (result);
}

Xv_public void selection_clear_functions(Xv_Server server)
{
	selection_unsupported(__FUNCTION__);
}

Xv_public Seln_result selection_request(Xv_Server server,
    Seln_holder *holder, Seln_request *buffer)
{
	selection_unsupported(__FUNCTION__);
    return SELN_FAILED;
}

Xv_public Seln_result selection_hold_file(Xv_Server server,
										Seln_rank rank, char *path)
{
	selection_unsupported(__FUNCTION__);
    return SELN_FAILED;
}

Xv_public void selection_use_timeout(Xv_Server server, int seconds)
{
	selection_unsupported(__FUNCTION__);
}
