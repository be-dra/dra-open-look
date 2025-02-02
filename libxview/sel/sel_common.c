#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)sel_common.c 20.35 93/06/28 DRA: $Id: sel_common.c,v 4.3 2025/02/02 19:10:47 dra Exp $";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */

#include <xview_private/seln_impl.h>

Pkg_private void seln_give_up_selection(Xv_Server server, Seln_rank rank);

Xv_public int seln_holder_same_client(Seln_holder *holder, char *client_data)
{
	selection_unsupported(__FUNCTION__);
	return 0;
}

Xv_public int seln_holder_same_process(Seln_holder *holder)
{
	selection_unsupported(__FUNCTION__);
    return FALSE;
}

Xv_public int seln_secondary_made(Seln_function_buffer *buffer)
{
	selection_unsupported(__FUNCTION__);
    return FALSE;
}

Xv_public int seln_secondary_exists(Seln_function_buffer *buffer)
{
	selection_unsupported(__FUNCTION__);
    return FALSE;
}

Pkg_private Seln_result selection_send_yield(Xv_Server server,
    Seln_rank rank, Seln_holder *holder)
{
	selection_unsupported(__FUNCTION__);
	return SELN_FAILED;
}
