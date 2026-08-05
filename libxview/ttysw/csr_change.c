#ifndef lint
char     csr_change_c_sccsid[] = "@(#)csr_change.c 20.51 93/06/28 DRA: RCS $Id: csr_change.c,v 4.15 2026/08/04 18:21:38 dra Exp $";
#endif
/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */

/*
 * Character screen operations (except size change and init).
 */

#include <xview_private/tty_impl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <string.h>
#include <X11/Xlib.h>
#include <pixrect/pixrect.h>
#include <pixrect/pixfont.h>

#ifdef __STDC__
#ifndef CAT
#define CAT(a,b)   a ## b
#endif
#endif
#include <pixrect/memvar.h>

#include <xview/rect.h>
#include <xview/rectlist.h>
#include <xview/pixwin.h>
#include <xview/ttysw.h>
#include <xview_private/charimage.h>
#include <xview_private/charscreen.h>
#undef CTRL
#include <xview_private/ttyansi.h>
#include <xview_private/term_impl.h>
#include <xview_private/win_info.h>
#include <xview_private/svr_impl.h>
#include <xview/window.h>
#include <xview/pkg.h>
#include <xview/attrol.h>
#include <xview/server.h>
#include <xview/font.h>
Xv_private char *xv_shell_prompt;

#define TTYSW_HOME_CHAR	'A'

static void ttysw_paintCursor(Ttysw_private ttysw, int op);

static int      caretx, carety, lxhome;
static short    charcursx, charcursy;

static int      boldstyle, inverse_mode, underline_mode;

static struct timeval  ttysw_bell_tv = {0, 100000};	/* 1/10 second */

/* Note: change to void */
Pkg_private int ttysw_setboldstyle(int new_boldstyle)
{
    if (new_boldstyle > TTYSW_BOLD_MAX
	|| new_boldstyle < TTYSW_BOLD_NONE)
	boldstyle = TTYSW_BOLD_NONE;
    else
	boldstyle = new_boldstyle;
    return boldstyle;
}

Pkg_private void ttysw_set_inverse_mode(int new_inverse_mode)
{
    inverse_mode = new_inverse_mode;
}

Pkg_private void ttysw_set_underline_mode( int new_underline_mode)
{
    underline_mode = new_underline_mode;
}

Pkg_private int ttysw_getboldstyle(void)
{
    return boldstyle;
}

/* NOT USED */
/* ttysw_get_inverse_mode() */
/* { */
/*     return inverse_mode; */
/* } */

/* NOT USED */
/* ttysw_get_underline_mode() */
/* { */
/*     return underline_mode; */
/* } */


/* NOT USED */
/* ttysw_getleftmargin() */
/* { */
/*     return ttysw->chrleftmargin; */
/* } */

static void ttysw_fixup_display_mode(char *mode)
{

	if ((*mode & MODE_INVERT) && (inverse_mode != TTYSW_ENABLE)) {
		*mode &= ~MODE_INVERT;
		if (inverse_mode == TTYSW_SAME_AS_BOLD)
			*mode |= MODE_BOLD;
	}
	if ((*mode & MODE_UNDERSCORE) && (underline_mode != TTYSW_ENABLE)) {
		*mode &= ~MODE_UNDERSCORE;
		if (underline_mode == TTYSW_SAME_AS_BOLD)
			*mode |= MODE_BOLD;
	}
	if ((*mode & MODE_BOLD) && (boldstyle & TTYSW_BOLD_INVERT)) {
		*mode &= ~MODE_BOLD;
		*mode |= MODE_INVERT;
	}
}

Pkg_private int ttysw_strlenchar(const char *s)
{
	int num_chars = 0;

	if (! s) return 0;

	while (*s) {
		int l = mblen(s, MB_CUR_MAX);

		if (l <= 0) l = 1;
		++num_chars;
		s += l;
	}
	return num_chars;
}

/* Note: whole string will be diplayed with mode. */
Pkg_private void ttysw_pstring(Ttysw_private ttysw, char *s,
				/* dieser Compiler warnt immer ueber 'different width'
				 * bei formalen Parametern von Typ char....
				 */
				int parmode,
				int col, int row, int op)
/* PIX_SRC | PIX_DST (faster), or PIX_SRC (safer) */
{
	Xv_window csrwin = csr_pixwin_get();

	register int x_home;
	register int y_home;
	XFontStruct *x_font_info =
			(XFontStruct *) xv_get((Xv_opaque) ttysw->pixfont, FONT_INFO);
	char mode = (char)parmode;

	SERVERTRACE((888, "%s: '%s', len %d\n", __FUNCTION__, s, strlen(s)));
	x_home = x_font_info->per_char ?
			x_font_info->per_char[TTYSW_HOME_CHAR -
			x_font_info->min_char_or_byte2].lbearing
			: x_font_info->min_bounds.lbearing;
	y_home = -x_font_info->ascent;

	/* this is needed for correct caret rendering */
	lxhome = x_home;

	/* possibly use escape sequences ? */

	SERVERTRACE((44, "SERVER_JOURNALLING?\n"));
	if (xv_get(XV_SERVER_FROM_WINDOW(csrwin), SERVER_JOURNALLING)) {
		if (INDEX(s, xv_shell_prompt[0])) {
			SERVERTRACE((44, " YES \n"));
			xv_set(XV_SERVER_FROM_WINDOW(csrwin),
					SERVER_JOURNAL_SYNC_EVENT, 1, NULL);
		}
	}

	SERVERTRACE((888, "%s: delay=%d\n", __FUNCTION__, ttysw_delaypainting));
	if (ttysw_delaypainting) {
		if (row == ttysw->ttysw_bottom)
			/*
			 * Reached bottom of screen so end ttysw_delaypainting.
			 */
			ttysw_pdisplayscreen(ttysw, TRUE, FALSE);
		return;
	}
	if (!s) return;

	ttysw_fixup_display_mode(&mode);

	if (mode & MODE_BOLD) {
		/* Clean up first */
		ttysw_pclearline(ttysw, col, col + (int)ttysw_strlenchar(s), row);

		/* render the first one, the potential offset of the others */
		tty_newtext(csrwin,
				col_to_x(col) - x_home,
				row_to_y(row) - y_home,
				(mode & MODE_INVERT) ? PIX_NOT(PIX_SRC) : op,
				(Xv_opaque) ttysw->pixfont, s, (int)strlen(s));

		if (boldstyle & TTYSW_BOLD_OFFSET_X)
			tty_newtext(csrwin,
					col_to_x(col) - x_home + 1,
					row_to_y(row) - y_home,
					(mode & MODE_INVERT) ? PIX_NOT(PIX_SRC) & PIX_DST :
					PIX_SRC | PIX_DST, (Xv_opaque) ttysw->pixfont, s, (int)strlen(s));
		if (boldstyle & TTYSW_BOLD_OFFSET_Y)
			tty_newtext(csrwin,
					col_to_x(col) - x_home,
					row_to_y(row) - y_home + 1,
					(mode & MODE_INVERT) ? PIX_NOT(PIX_SRC) & PIX_DST :
					PIX_SRC | PIX_DST, (Xv_opaque) ttysw->pixfont, s, (int)strlen(s));
		if (boldstyle & TTYSW_BOLD_OFFSET_XY)
			tty_newtext(csrwin,
					col_to_x(col) - x_home + 1,
					row_to_y(row) - y_home + 1,
					(mode & MODE_INVERT) ? PIX_NOT(PIX_SRC) & PIX_DST :
					PIX_SRC | PIX_DST, (Xv_opaque) ttysw->pixfont, s, (int)strlen(s));
	}
	else {
		SERVERTRACE((888, "%s: before tty_newtext(%s), len %d\n", __FUNCTION__,
									s, strlen(s)));
		tty_newtext(csrwin,
				col_to_x(col) - x_home,
				row_to_y(row) - y_home,
				(mode & MODE_INVERT) ? PIX_NOT(PIX_SRC) : op,
				(Xv_opaque) ttysw->pixfont, s, (int)strlen(s));
	}
	if (mode & MODE_UNDERSCORE) {
		tty_background(csrwin,
				col_to_x(col), row_to_y(row) + ttysw->chrheight - 1,
				(int)ttysw_strlenchar(s) * ttysw->chrwidth, 1,
				(mode & MODE_INVERT) ? PIX_NOT(PIX_SRC) : PIX_SRC);
	}
}

Pkg_private void ttysw_pclearline(Ttysw_private ttysw, int fromcol, int tocol,
							int row)
{
    int	klu1284	= (fromcol == 0 ? 1 : 0 );
	Xv_window csrwin = csr_pixwin_get();

    if (ttysw_delaypainting) return;
    tty_background(csrwin,
			  col_to_x(fromcol)-klu1284, row_to_y(row),
			  col_to_x(tocol) - col_to_x(fromcol)+klu1284,
			  ttysw->chrheight, PIX_CLR);
}

Pkg_private void ttysw_pcopyline(Ttysw_private ttysw, int fromcol, int tocol, int count, int row)
{
	Xv_window csrwin = csr_pixwin_get();
    int             pix_width = (count * ttysw->chrwidth);
    if (ttysw_delaypainting) return;
    (void) tty_copyarea(csrwin,
		     col_to_x(fromcol)-1, row_to_y(row), pix_width+1, ttysw->chrheight,
			col_to_x(tocol)-1, row_to_y(row));
    tty_synccopyarea(csrwin);
}

Pkg_private void ttysw_pclearscreen(Ttysw_private ttysw, int fromrow, int torow)
{
	Xv_window csrwin = csr_pixwin_get();
    if (ttysw_delaypainting) return;
    tty_background(csrwin, col_to_x(ttysw->ttysw_left)-1,
			  row_to_y(fromrow),
			  ttysw->winwidthp+1, row_to_y(torow - fromrow), PIX_CLR);
}

Pkg_private void ttysw_pcopyscreen(Ttysw_private ttysw, int fromrow, int torow, int count)
{
	Xv_window csrwin = csr_pixwin_get();
    if (ttysw_delaypainting) return;
    tty_copyarea(csrwin, col_to_x(ttysw->ttysw_left)-1,
			row_to_y(fromrow), ttysw->winwidthp+1, row_to_y(count),
			col_to_x(ttysw->ttysw_left)-1, row_to_y(torow));
    tty_synccopyarea(csrwin);
}

/* return value means "row is empty" */
static int ttysw_displayrow(Ttysw_private ttysw, int row, int leftcol)
{
	register int colstart, blanks, colfirst;
	register char *strstart, *strfirst;
	register char modefirst;
	register char *modestart;
	char csave;

	colfirst = 0;
	colstart = leftcol;

	/* here the "ue" is already broken */
	SERVERTRACE((888, "%s: whole row '%s'\n", __FUNCTION__, ttysw->image[row]));
	if ((unsigned char)leftcol < LINE_LENGTH_CHARS(ttysw->image[row])) {
		strfirst = (caddr_t) 0;
		modefirst = MODE_CLEAR;
		blanks = 1;
		for (strstart = ttysw->image[row] + leftcol,
				modestart = ttysw->screenmode[row] + leftcol; *strstart;
				strstart++, modestart++, colstart++) {
			/*
			 * Find beginning of bold string
			 */
			if (*modestart != modefirst) {
				goto Flush;
				/*
				 * Find first non-blank char
				 */
			}
			else if (blanks && (*strstart != ' '))
				goto Flush;
			else
				continue;
		  Flush:

			if (strfirst != (caddr_t) 0)
			{
				csave = *strstart;
				*strstart = '\0';
				SERVERTRACE((888, "%s: before pstring(%s)\n", __FUNCTION__,
								strfirst));
				ttysw_pstring(ttysw, strfirst, modefirst, colfirst, row,
						PIX_SRC /* | PIX_DST - jcb */ );
				*strstart = csave;
			}
			colfirst = colstart;
			strfirst = strstart;
			modefirst = *modestart;
			blanks = 0;
		}

		if (strfirst != (caddr_t) 0)
		{
			SERVERTRACE((888, "%s: before pstring(%s)\n", __FUNCTION__,
								strfirst));
			ttysw_pstring(ttysw, strfirst, modefirst, colfirst,
					row, PIX_SRC /* | PIX_DST -- jcb */ );
		}

		return FALSE;
	}
	return (LINE_LENGTH_BYTES(ttysw->image[row]) == 0);
}

Pkg_private void ttysw_pdisplayscreen(Ttysw_private ttysw,
						int dontrestorecursor, int allowclearareawhenempty)
{
/* 	struct rect *rect; */
	int row, all_empty = TRUE;;
	Xv_window view = csr_pixwin_get();

	ttysw_delaypainting = 0;
	/*
	 * refresh the entire image.
	 */
	SERVERTRACE((567, "%s: view clearing\n", __FUNCTION__));
#ifdef THIS_CLEARS_THE_WHOLE_WINDOW_WHICH_CAN_BE_DONE_EASIER
	rect = (struct rect *)xv_get(view, WIN_RECT);
	tty_background(view, 0, 0,
			rect->r_width + 1, rect->r_height, PIX_CLR);
#else
	XClearArea((Display *)xv_get(view, XV_DISPLAY),
					xv_get(view, XV_XID), 0, 0, 0, 0, TRUE);
#endif

	SERVERTRACE((567, "%s: top %d to bottom %d\n", __FUNCTION__,
							ttysw->ttysw_top, ttysw->ttysw_bottom));
	for (row = ttysw->ttysw_top; row < ttysw->ttysw_bottom; row++) {
		all_empty = (all_empty && ttysw_displayrow(ttysw, row, 0));
	}

	SERVERTRACE((567, "%s(allowclearareawhenempty=%d, all_empty=%d)\n", __FUNCTION__,allowclearareawhenempty, all_empty));
	if (allowclearareawhenempty && all_empty) {
		SERVERTRACE((567, "%s: calling XClearArea\n", __FUNCTION__));
		XClearArea((Display *)xv_get(view, XV_DISPLAY),
					xv_get(view, XV_XID), 0, 0, 0, 0, TRUE);
	}
	if (!dontrestorecursor) {
		/*
		 * The following has effect of restoring cursor.
		 */
		SERVERTRACE((567, "%s: ttysw_removeCursor\n", __FUNCTION__));
		ttysw_removeCursor(ttysw);
	}
}

/* ARGSUSED */
Pkg_private void ttysw_prepair(XEvent *eventp)
{
	Xv_window csrwin = csr_pixwin_get();
	Tty_exposed_lines *exposed;
	Ttysw_private	ttysw = TTY_PRIVATE_FROM_ANY_VIEW(csrwin);
	register int	row;
	int		leftcol;
	int		display_cursor = FALSE;

	/*
	 * Handles expose and graphics expose events for the ttysw.
	 */

	/* Get the expose events, ignore textsw caret checking with -10000 */
	exposed = tty_calc_exposed_lines(csrwin, eventp, -10000);

	leftcol = x_to_col(exposed->leftmost);

	/*
	 * Check damage on for cursor:
	 * When the cursor is light, it actually appears
	 * in the lines above and below the cursrow
	 * so these lines have to be checked too.
	 */
	if(leftcol <= ttysw->curscol+1) {
	  /*
	   * cheak and use one column to right and left of curscol because
	   * unhilighted cursor actually overlaps into adjoining columns.
	   * Need to repaint these characters so they look right after
	   * erasing with ttysw_paintCursor().
	   */
	  leftcol = MIN(leftcol, ttysw->curscol-1);
	  leftcol = MAX(leftcol, 0);
	  if ((exposed->line_exposed[ttysw->cursrow]) ||
	     (((ttysw->cursor & LIGHTCURSOR) && exposed->line_exposed[ttysw->cursrow+1]) ||
	      (ttysw->cursrow > 0 && exposed->line_exposed[ttysw->cursrow-1]))) {

			ttysw_paintCursor(ttysw, PIX_CLR);
			exposed->line_exposed[ttysw->cursrow] = TRUE;
			display_cursor = TRUE;
	  }
	}

	if(ttysw->sels[TTY_SEL_PRIMARY].sel_made && !ttysw->sels[TTY_SEL_PRIMARY].sel_null) {
		/*
		 * In this case, there is a primary selection when
		 * an expose event happened.  To be most efficient and
		 * visually appealing, we might want to only repaint
		 * damaged areas.  But because the primary selection
		 * is highlighted with exclusive-or, this has to be
		 * done very cleverly.
		 *
		 * The secondary selection is painted over the text
		 * not using xor so it appears correctly regardless.
		 * This selection might be repainted here if it becomes an
		 * issue, but the thinking is that simultaneous selections
		 * and exposures are relatively rare.
		 */
		struct textselpos *begin, *end;
		int	selected_lines_damaged = FALSE;

		ttysortextents(&ttysw->sels[TTY_SEL_PRIMARY], &begin, &end);

		for (row = begin->tsp_row; row <= end->tsp_row; row++)
			if(exposed->line_exposed[row]) {
				/* there was damage to the selected areas. */
				selected_lines_damaged = TRUE;
				break;
			}

		for (row = ttysw->ttysw_top; row < ttysw->ttysw_bottom; row++) {
			if((selected_lines_damaged &&
			   (row >= begin->tsp_row) && (row <= end->tsp_row)) ||
			   (row == ttysw->cursrow)) {
				/* because of xor, the line is cleared */
				ttysw_pclearline(ttysw, 0, (int)STRLEN(ttysw->image[row])+1, row);
				ttysw_displayrow(ttysw, row, 0);
			} else
			if(exposed->line_exposed[row])
				ttysw_displayrow(ttysw, row, leftcol);
		}

		if (selected_lines_damaged)
			ttyhiliteselection(ttysw, &ttysw->sels[TTY_SEL_PRIMARY], TTY_SEL_PRIMARY);
		/* secondary selection is painted in caller */


	} else {

		/* The easy case: no selection, just repaint damaged lines. */

		for (row = ttysw->ttysw_top; row < ttysw->ttysw_bottom; row++) {
		    if(exposed->line_exposed[row])
				ttysw_displayrow(ttysw, row, leftcol);
		}
	}

	if(display_cursor)
		ttysw_removeCursor(ttysw);

	tty_clear_clip_rectangles(csrwin);

}

Pkg_private void ttysw_drawCursor(Ttysw_private ttysw, int yChar, int xChar)
{
	Xv_window csrwin = csr_pixwin_get();

	charcursx = xChar;
	charcursy = yChar;
	caretx = col_to_x(xChar);
	carety = row_to_y(yChar);
	if (ttysw_delaypainting || ttysw->cursor == NOCURSOR)
		return;
	tty_background(csrwin,
			caretx - lxhome, carety, ttysw->chrwidth, ttysw->chrheight,
			PIX_NOT(PIX_DST));
	if (ttysw->cursor & LIGHTCURSOR) {
		(void)tty_background(csrwin,
				caretx - lxhome - 1, carety - 1, ttysw->chrwidth + 2,
				ttysw->chrheight + 2, PIX_NOT(PIX_DST));
		ttysw_pos(ttysw, xChar, yChar);
	}
}

static void ttysw_paintCursor(Ttysw_private ttysw, int op)
{
	Xv_window csrwin = csr_pixwin_get();
	int y;
	int height;
	/*
	 * Erase or xor all bits used in light and normal cursor.
	 */

	y = carety-1;
	height = ttysw->chrheight + 2;
	if(y<0) {
		/* work around xnews server bug. */
		y=0;
		height--;
	}

	tty_background(csrwin, caretx - lxhome - 1, y, ttysw->chrwidth + 2,
							height, op);
}

Pkg_private void ttysw_removeCursor(Ttysw_private ttysw)
{
	Xv_window csrwin = csr_pixwin_get();

    if (ttysw_delaypainting || ttysw->cursor == NOCURSOR) return;
    (void) tty_background(csrwin,
		     caretx-lxhome, carety, ttysw->chrwidth, ttysw->chrheight, PIX_NOT(PIX_DST));
    if (ttysw->cursor & LIGHTCURSOR) ttysw_paintCursor(ttysw, PIX_NOT(PIX_DST));
}

Pkg_private void ttysw_restoreCursor(Ttysw_private ttysw)	/* BUG ALERT: unnecessary routine */
{
    ttysw_removeCursor(ttysw);
}

Pkg_private void ttysw_screencomp(void)	/* BUG ALERT: unnecessary routine */
{
}

Pkg_private void ttysw_blinkscreen(Xv_window window)
{
	struct timeval now;
	static struct timeval lastblink;

	gettimeofday(&now, (struct timezone *)0);
	if (now.tv_sec - lastblink.tv_sec > 1) {
		win_bell(window, ttysw_bell_tv, window);
		lastblink = now;
	}
}
