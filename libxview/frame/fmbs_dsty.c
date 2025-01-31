#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)fmbs_dsty.c 1.16 93/06/28 DRA: $Id: fmbs_dsty.c,v 4.3 2025/01/05 20:48:41 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

#include <xview_private/fm_impl.h>
#include <xview_private/frame_base.h>


/*
 * free the frame struct and all its resources.
 */
static void frame_base_free(Frame_base_info *frame)
{
    /* Free frame struct */
    free((char *) frame);
}

/* Destroy the frame struct */
Pkg_private int frame_base_destroy(Frame frame_public, Destroy_status status)
{
	Frame_base_info *frame = FRAME_BASE_PRIVATE(frame_public);

	if (status == DESTROY_CLEANUP) {	/* waste of time if ...PROCESS_DEATH */
		/*
		 * If have saved command line strings, free them
		 */
		if (frame->cmd_line_strings_count > 0) {
			char **old_strings = frame->cmd_line_strings;
			int i;

			for (i = 0; i < frame->cmd_line_strings_count; ++i) {
				if (old_strings[i]) {
					free(old_strings[i]);
				}
			}

			/*
			 * Free array holding strings
			 */
			free(old_strings);
		}

		frame_base_free(frame);
	}
	return XV_OK;
}
