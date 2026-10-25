#ifndef lint
char     cim_size_c_sccsid[] = "@(#)cim_size.c 20.32 93/06/28 DRA: $Id: cim_size.c,v 4.7 2026/10/25 08:17:42 dra Exp $";
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

typedef struct ttysw_cim_size_secret {
	/* these were all static variables - I wanted them to become
	 * instance variables but prevent anybody except cim_size.c
	 * to have access to them
	 */
	char    *lines_ptr;
	char     *mode_ptr;
	char   **temp_image;
	char     **temp_mode;
	char    *temp_lines_ptr;
	char     *temp_mode_ptr;
	int      maxright, maxbottom;
} former_static_t;

/*
 * Initialize initial character image.
 */
Pkg_private int xv_tty_imageinit(Ttysw *ttysw, Xv_object window)
{
	int maximagewidth, maximageheight;

	if (! ttysw->secret) {
		ttysw->secret = xv_alloc(former_static_t);
	}

	if (wininit(ttysw, window, &maximagewidth, &maximageheight) == 0)
		return (0);
	ttysw->ttysw_top = ttysw->ttysw_left = 0;
	ttysw->curscol = ttysw->ttysw_left;
	ttysw->cursrow = ttysw->ttysw_top;
	ttysw->secret->maxright = x_to_col(maximagewidth);
	if (ttysw->secret->maxright > 255)
		ttysw->secret->maxright = 255;	/* line length is stored in a byte */
	ttysw->secret->maxbottom = y_to_row(maximageheight);
	xv_tty_imagealloc(ttysw, FALSE);
	ttysw_pclearscreen(ttysw, 0, ttysw->ttysw_bottom + 1);	/* +1 to get remnant at
															 * bottom */
	return (1);
}

/*
 * Allocate character image.
 */
Pkg_private void xv_tty_imagealloc(Ttysw *ttysw, int for_temp)
					/* decide which data structure to go into */
{
	former_static_t *sec = ttysw->secret;
	register char **newimage;
	register char **newmode;
	register int i;
	int nchars;
	int bytes_per_char;
	register char *line;
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
	ttysw->ttysw_right = (ttysw->ttysw_right < sec->maxright) ?
								ttysw->ttysw_right : sec->maxright;
	ttysw->ttysw_bottom = (ttysw->ttysw_bottom < sec->maxbottom) ?
								ttysw->ttysw_bottom : sec->maxbottom;
	/*
	 * Let pty set terminal size
	 */
	(void)xv_tty_new_size(ttysw, ttysw->ttysw_right, ttysw->ttysw_bottom);
	/*
	 * Allocate line array and character storage
	 */
	nchars = ttysw->ttysw_right * ttysw->ttysw_bottom;
	if (_xv_is_multibyte) {
		bytes_per_char = 4;
	}
	else {
		bytes_per_char = 1;
	}
	newimage = (char **) calloc(1L, (size_t)(ttysw->ttysw_bottom * sizeof(char *)));
	newmode = (char **)calloc(1L, ttysw->ttysw_bottom * sizeof(char *));
	bold = (char *)calloc(1L,
				(size_t)(bytes_per_char * nchars + 3 * ttysw->ttysw_bottom));
	line = (char *)calloc(1L,
				(size_t)(bytes_per_char * nchars + 3 * ttysw->ttysw_bottom));

	for (i = 0; i < ttysw->ttysw_bottom; i++) {
		newimage[i] = line + 2;
		newmode[i] = bold + 2;
		setlinelength(ttysw, newimage[i], 0);
		line += bytes_per_char * ttysw->ttysw_right + 3;
		bold += bytes_per_char * ttysw->ttysw_right + 3;
	}
	if (for_temp) {
		sec->temp_image = newimage;
		sec->temp_mode = newmode;
		sec->temp_lines_ptr = newimage[0] - 2;
		sec->temp_mode_ptr = newmode[0] - 2;

	}
	else {
		ttysw->image = newimage;
		ttysw->screenmode = newmode;
		sec->lines_ptr = newimage[0] - 2;
		sec->mode_ptr = newmode[0] - 2;

	}
}


/*
 * Free character image.
 */
/* ARGSUSED */
Pkg_private void xv_tty_free_image_and_mode(Ttysw_private ttysw)
{
	former_static_t *sec = ttysw->secret;

	if (sec->lines_ptr) {
		free((char *) (sec->lines_ptr));
		sec->lines_ptr = NULL;
	}
	if (ttysw->image) {
		free((char **) ttysw->image);
		ttysw->image = NULL;
	}
	if (sec->mode_ptr) {
		free((char *)(sec->mode_ptr));
		sec->mode_ptr = NULL;
	}
	if (ttysw->screenmode) {
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
	former_static_t *sec = ttysw->secret;
	char **oldimage;
	register int oldrow, row;
	int oldbottom = ttysw->ttysw_bottom;
	int topstart;
	int i;

	/*
	 * Get new image and image description
	 */
	(void)xv_tty_imagealloc(ttysw, TRUE);
	/*
	 * Clear max of old/new screen (not image).
	 */
	/*
	 * jcb (void)ttysw_saveCursor(); clrbottom = (oldbottom < ttysw->ttysw_bottom)?
	 * ttysw->ttysw_bottom+2: oldbottom+2; (void)ttysw_pclearscreen(0, clrbottom);
	 * (void)ttysw_restoreCursor();
	 */
	/* Find out where last line of text is (actual oldbottom). */
	for (row = oldbottom; row > ttysw->ttysw_top; row--) {
		if (LINE_LENGTH_BYTES(ttysw->image[row - 1])) {
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
	ttysw->image = sec->temp_image;	/* Hack around globals */

	for (i = ttysw->ttysw_top; i < ttysw->ttysw_bottom; i++)	/* jcb */
		setlinelength(ttysw, ttysw->image[i], 0);

	/*
	 * jcb  (void)ttysw_cim_clear(ttysw->ttysw_top, ttysw->ttysw_bottom); remove extra
	 * repaint #1 jcb
	 */
	/* (This was caused by ~ttysw_delaypainting" to trigger itimer repaint) */

	ttysw->image = oldimage;
	oldimage = (char **)0;

	for (oldrow = topstart, row = 0; oldrow < oldbottom; oldrow++, row++) {
		register int sl = STRLEN(ttysw->image[oldrow]);

#ifdef	DEBUG_LINELENGTH_WHEN_WRAP
		if (sl != LINE_LENGTH(image[oldrow]))
			printf("real %ld saved %ld, l %ld, oldbottom %ld bottom %ld\n", sl,
					LINE_LENGTH(oldimage[l]), l, oldbottom,
					ttysw->ttysw_bottom);
#endif /* DEBUG_LINELENGTH_WHEN_WRAP */

		if (sl > ttysw->ttysw_right)
			sl = ttysw->ttysw_right;
		XV_BCOPY(ttysw->image[oldrow], sec->temp_image[row], (size_t)sl);
		XV_BCOPY(ttysw->screenmode[oldrow], sec->temp_mode[row], (size_t)sl);
		setlinelength(ttysw, sec->temp_image[row], sl);
	}
	xv_tty_free_image_and_mode(ttysw);
	ttysw->image = sec->temp_image;
	ttysw->screenmode = sec->temp_mode;
	sec->lines_ptr = sec->temp_lines_ptr;
	sec->mode_ptr = sec->temp_mode_ptr;

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
	/* (void)ttysw_pdisplayscreen(0);   remove extra repaint #2 jcb */
}
