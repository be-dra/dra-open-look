#ifndef lint
char     cim_size_c_sccsid[] = "@(#)cim_size.c 20.32 93/06/28 DRA: $Id: cim_size.c,v 4.3 2025/03/19 21:33:50 dra Exp $";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */

/*
 * Character image initialization, destruction and size changing routines
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/signal.h>
#include <stdio.h>
#include <string.h>
#include <pixrect/pixrect.h>
#include <pixrect/pixfont.h>
#include <xview/notify.h>
#include <xview/rect.h>
#include <xview/rectlist.h>
#include <xview/pixwin.h>
#include <xview/win_input.h>
#include <xview/win_notify.h>
#include <xview/pkg.h>
#include <xview/attrol.h>
#include <xview_private/i18n_impl.h>
#include <xview_private/tty_impl.h>
#include <xview_private/ttyansi.h>
#include <xview_private/charimage.h>
#include <xview_private/charscreen.h>
#include <xview_private/portable.h>

#ifdef ONLY_ONE_TTY_PER_PROCESS
CHAR          **image;		/* BUG ALERT! Get rid of this global. */
char          **screenmode;     /* BUG ALERT! Get rid of this global. */
#endif /* ONLY_ONE_TTY_PER_PROCESS */
static CHAR    *lines_ptr;
static char     *mode_ptr;      /* BUG ALERT! Get rid of this global. */
static CHAR   **temp_image;
static char     **temp_mode;    /* BUG ALERT! Get rid of this
                                                 * global. */
static CHAR    *temp_lines_ptr;
static char     *temp_mode_ptr; /* BUG ALERT! Get rid of this
                                                 * global. */

static int      maxright, maxbottom;

/*
 * Initialize initial character image.
 */
Pkg_private int
xv_tty_imageinit(ttysw, window)
    Ttysw          *ttysw;
    Xv_object       window;
{
    int             maximagewidth, maximageheight;

    if (wininit(ttysw, window, &maximagewidth, &maximageheight) == 0)
	return (0);
    ttysw->ttysw_top = ttysw->ttysw_left = 0;
    ttysw->curscol = ttysw->ttysw_left;
    ttysw->cursrow = ttysw->ttysw_top;
    maxright = x_to_col(maximagewidth);
    if (maxright > 255)
	maxright = 255;		/* line length is stored in a byte */
    maxbottom = y_to_row(maximageheight);
    xv_tty_imagealloc(ttysw, FALSE);
    ttysw_pclearscreen(ttysw, 0, ttysw->ttysw_bottom + 1);	/* +1 to get remnant at
						 * bottom */
    return (1);
}

/*
 * Allocate character image.
 */
Pkg_private void
xv_tty_imagealloc(ttysw, for_temp)
    Ttysw          *ttysw;
    int             for_temp;	/* decide which data structure to go into */
{
	register CHAR **newimage;
	register char **newmode;
	register int i;
	int nchars;
	register CHAR *line;
	register char *bold;

	/*
	 * Determine new screen dimensions
	 */
	ttysw->ttysw_right = x_to_col(ttysw->winwidthp);
	ttysw->ttysw_bottom = y_to_row(ttysw->winheightp);
	/*
	 * Ensure has some non-zero dimension
	 */
	if (ttysw->ttysw_right < 1)
		ttysw->ttysw_right = 1;
	if (ttysw->ttysw_bottom < 1)
		ttysw->ttysw_bottom = 1;
	/*
	 * Bound new screen dimensions
	 */
	ttysw->ttysw_right = (ttysw->ttysw_right < maxright) ? ttysw->ttysw_right : maxright;
	ttysw->ttysw_bottom = (ttysw->ttysw_bottom < maxbottom) ? ttysw->ttysw_bottom : maxbottom;
	/*
	 * Let pty set terminal size
	 */
	(void)xv_tty_new_size(ttysw, ttysw->ttysw_right, ttysw->ttysw_bottom);
	/*
	 * Allocate line array and character storage
	 */
	nchars = ttysw->ttysw_right * ttysw->ttysw_bottom;
	newimage = (CHAR **) calloc(1L, (size_t)(ttysw->ttysw_bottom * sizeof(CHAR *)));
	newmode = (char **)calloc(1L, ttysw->ttysw_bottom * sizeof(char *));
	bold = (char *)calloc(1L, (size_t)(nchars + 2 * ttysw->ttysw_bottom));

#ifdef OW_I18N
	line = (CHAR *) calloc(1, (unsigned)((nchars + 2 * ttysw->ttysw_bottom) *
					sizeof(CHAR)));
#else
	line = (char *)calloc(1L, (size_t)(nchars + 2 * ttysw->ttysw_bottom));
#endif

	for (i = 0; i < ttysw->ttysw_bottom; i++) {
		newimage[i] = line + 1;
		newmode[i] = bold + 1;
		setlinelength(ttysw, newimage[i], 0);
		line += ttysw->ttysw_right + 2;
		bold += ttysw->ttysw_right + 2;
	}
	if (for_temp) {
		temp_image = newimage;
		temp_mode = newmode;
		temp_lines_ptr = newimage[0] - 1;
		temp_mode_ptr = newmode[0] - 1;

	}
	else {
		ttysw->image = newimage;
		ttysw->screenmode = newmode;
		lines_ptr = newimage[0] - 1;
		mode_ptr = newmode[0] - 1;

	}
}


/*
 * Free character image.
 */
/* ARGSUSED */
Pkg_private void xv_tty_free_image_and_mode(Ttysw_private ttysw)
{

	if (lines_ptr) {
/* 		cfree((CHAR *) (lines_ptr)); */
		free((CHAR *) (lines_ptr));
		lines_ptr = NULL;
	}
	if (ttysw->image) {
/* 		cfree((CHAR **) ttysw->image); */
		free((CHAR **) ttysw->image);
		ttysw->image = NULL;
	}
	if (mode_ptr) {
/* 		cfree((char *)(mode_ptr)); */
		free((char *)(mode_ptr));
		mode_ptr = NULL;
	}
	if (ttysw->screenmode) {
/* 		cfree((char **)ttysw->screenmode); */
		free((char **)ttysw->screenmode);
		ttysw->screenmode = NULL;
	}
}



/*
 * Called when screen changes size.  This will let lines get longer (or
 * shorter perhaps) but won't re-fold older lines.
 */
Pkg_private void ttysw_imagerepair(Ttysw_view_handle ttysw_view)
{
    Ttysw_private ttysw = TTY_FOLIO_FROM_TTY_VIEW_HANDLE(ttysw_view);
    CHAR          **oldimage;
    register int    oldrow, row;
    int             oldbottom = ttysw->ttysw_bottom;
    int             topstart;
    int             i;

    /*
     * Get new image and image description
     */
    (void) xv_tty_imagealloc(ttysw, TRUE);
    /*
     * Clear max of old/new screen (not image).
     */
    /*
     * jcb (void)ttysw_saveCursor(); clrbottom = (oldbottom < ttysw->ttysw_bottom)?
     * ttysw->ttysw_bottom+2: oldbottom+2; (void)ttysw_pclearscreen(0, clrbottom);
     * (void)ttysw_restoreCursor();
     *//* Find out where last line of text is (actual oldbottom). */
    for (row = oldbottom; row > ttysw->ttysw_top; row--) {
#ifdef OW_I18N
        if (LINE_LENGTH(ttysw->image[row - 1])) {
#else
	if (LINE_LENGTH(ttysw->image[row - 1])) {
#endif
	    oldbottom = row;
	    break;
	}
    }
    /*
     * Try to perserve bottom (south west gravity) text. This wouldn't work
     * well for vi and other programs that know about the size of the
     * terminal but aren't notified of changes. However, it should work in
     * many cases  for straight tty programs like the shell.
     */
    if (oldbottom > ttysw->ttysw_bottom)
	topstart = oldbottom - ttysw->ttysw_bottom;
    else
	topstart = 0;
    /*
     * Fill in new screen from old
     */
    ttysw->ttysw_lpp = 0;

    oldimage = ttysw->image;
    ttysw->image = temp_image;		/* Hack around globals */

    for (i = ttysw->ttysw_top; i < ttysw->ttysw_bottom; i++)	/* jcb */
	setlinelength(ttysw, ttysw->image[i], 0);

    /*
     * jcb	(void)ttysw_cim_clear(ttysw->ttysw_top, ttysw->ttysw_bottom); remove extra
     * repaint #1 jcb
     */
    /* (This was caused by ~ttysw_delaypainting" to trigger itimer repaint) */

    ttysw->image = oldimage;
    oldimage = (CHAR **) 0;

    for (oldrow = topstart, row = 0; oldrow < oldbottom; oldrow++, row++) {
	register int    sl = STRLEN(ttysw->image[oldrow]);
#ifdef	DEBUG_LINELENGTH_WHEN_WRAP
#ifdef OW_I18N
        if (sl != LINE_LENGTH(ttysw->image[oldrow]))
            printf("real %ld saved %ld, l %ld, oldbottom %ld bottom %ld\n", sl,
LINE_LENGTH(oldimage[l]), l, oldbottom, ttysw->ttysw_bottom);
#else
	if (sl != LINE_LENGTH(image[oldrow]))
	    printf("real %ld saved %ld, l %ld, oldbottom %ld bottom %ld\n", sl, LINE_LENGTH(oldimage[l]), l, oldbottom, ttysw->ttysw_bottom);
#endif /* OW_I18N */
#endif /* DEBUG_LINELENGTH_WHEN_WRAP */
	if (sl > ttysw->ttysw_right)
	    sl = ttysw->ttysw_right;
#ifdef OW_I18N
	XV_BCOPY(ttysw->image[oldrow], temp_image[row], sl * sizeof(CHAR));
#else
	XV_BCOPY(ttysw->image[oldrow], temp_image[row], (size_t)sl);
#endif
	XV_BCOPY(ttysw->screenmode[oldrow], temp_mode[row], (size_t)sl);
	setlinelength(ttysw, temp_image[row], sl);
    }
    xv_tty_free_image_and_mode(ttysw);
    ttysw->image = temp_image;
    ttysw->screenmode = temp_mode;
    lines_ptr = temp_lines_ptr;
    mode_ptr = temp_mode_ptr;

    /*
     * Move the cursor to its new position in the new coordinate system. If
     * the window is shrinking, and thus "topstart" is the number of rows by
     * which it is shrinking, the row number is decreased by that number of
     * rows; if the window is growing, and thus "topstart" is zero, the row
     * number is unchanged. The column number is unchanged, unless the old
     * column no longer exists (in which case the cursor is placed at the
     * rightmost column).
     */
    ttysw_pos(ttysw, ttysw->curscol, ttysw->cursrow - topstart);
    /* (void)ttysw_pdisplayscreen(0); 	remove extra repaint #2 jcb */
}
