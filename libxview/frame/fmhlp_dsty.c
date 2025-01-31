#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)fmhlp_dsty.c 1.15 93/06/28 DRA: $Id: fmhlp_dsty.c,v 4.1 2024/03/28 12:57:19 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

#include <xview_private/fm_impl.h>
#include <xview_private/frame_help.h>

/*
 * free the frame struct and all its resources.
 */
static void frame_help_free(Frame_help_info *frame)
{
    /* Free frame struct */
    free((char *) frame);
}

/* Destroy the frame struct */
Pkg_private int frame_help_destroy(Frame frame_public, Destroy_status status)
{
    Frame_help_info *frame = FRAME_HELP_PRIVATE(frame_public);

    if (status == DESTROY_CLEANUP) {	/* waste of time if ...PROCESS_DEATH */
	frame_help_free(frame);
    }
    return XV_OK;
}
