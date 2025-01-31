#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)win_compat.c% 20.23 93/06/28 DRA: $Id: win_compat.c,v 4.2 2024/09/15 17:09:50 dra Exp $";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Win_compat.c: SunView 1.X compatibility routines.
 */

#include <xview_private/i18n_impl.h>
#include <xview/pkg.h>
#include <xview/window.h>
#include <xview/win_input.h>
#include <xview/fullscreen.h>

/* ARGSUSED */
void win_getinputmask(Xv_object window, Inputmask *im, Xv_opaque *nextwindownumber)
{
    *im = *((Inputmask *) xv_get(window, WIN_INPUT_MASK));
}

/* ARGSUSED */
void win_setinputmask(Xv_object window, Inputmask *im, Inputmask *im_flush, Xv_opaque nextwindownumber)
{

    if (xv_get(window, WIN_IS_IN_FULLSCREEN_MODE)) {
	fprintf(stderr,
		XV_MSG(" Attempting to set the input mask of a window in fullscreen mode!\n"));
	abort();
    }
    xv_set(window, WIN_INPUT_MASK, im, NULL);
}


xv_coord_t win_getheight(Xv_object window)
{

    return (xv_coord_t) window_get(window, WIN_GET_HEIGHT, NULL);
}

xv_coord_t win_getwidth(Xv_object window)
{

    return ((int) window_get(window, WIN_GET_WIDTH, NULL));
}
