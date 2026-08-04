#ifndef lint
char     cim_change_c_sccsid[] = "@(#)cim_change.c 20.19 93/06/28 DRA: $Id: cim_change.c,v 4.8 2026/08/03 19:26:14 dra Exp $";
#endif

/*@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
  @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
 *
 * Lass die Finger hier weg, ich bin jetzt schon ein paarmal
 * versionsmaessig zurueckgegangen...  wegen Repaint-Dreck.
 *
 * cim_change.c ist mittlerweile der Hauptverdaechtige......
 *
  @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
  @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ */
/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */

/*
 * Character image manipulation (except size change) routines.
 */

#include <xview_private/i18n_impl.h>
#include <sys/types.h>
#include <pixrect/pixrect.h>
#include <xview_private/ttyansi.h>
#include <xview_private/charimage.h>
#include <xview_private/charscreen.h>
#include <xview_private/tty_impl.h>

Xv_private_data int	ttysw_delaypainting;

static void ttysw_roll(Ttysw_private ttysw, int first, int mid, int last);

#define JF

Pkg_private void ttysw_vpos(Ttysw_private ttysw, int row, int col)
{
	register char *line = ttysw->image[row];
	register char *bold = ttysw->screenmode[row];
	register int i;

	while ((int)LINE_LENGTH(line) <= col) {
		bold[LINE_LENGTH(line)] = MODE_CLEAR;
		i = LINE_LENGTH(line);
		line[-1]++;
		line[i] = (char)' ';
	}
	setlinelength(ttysw, line, ((int)LINE_LENGTH(line)));
}

Pkg_private void ttysw_bold_mode(Ttysw_private ttysw)
{
    ttysw->boldify |= MODE_BOLD;
}

Pkg_private void ttysw_underscore_mode(Ttysw_private ttysw)
{
    ttysw->boldify |= MODE_UNDERSCORE;
}

Pkg_private void ttysw_inverse_mode(Ttysw_private ttysw)
{
    ttysw->boldify |= MODE_INVERT;
}

#ifdef SEEMS_UNUSED
/* NOT USED */
ttysw_noinverse_mode()
{
    ttysw->boldify &= ~MODE_INVERT;
}
#endif /* SEEMS_UNUSED */

Pkg_private void ttysw_clear_mode(Ttysw_private ttysw)
{
    ttysw->boldify = MODE_CLEAR;
}

Pkg_private void ttysw_writePartialLine(Ttysw_private ttysw, char *s,
								int curscolStart)
{
	register char *sTmp;
	register char *line = ttysw->image[ttysw->cursrow];
	register char *bold = ttysw->screenmode[ttysw->cursrow];
	register int curscolTmp = curscolStart;

	/*
	 * Fix line length if start is past end of line length. This shouldn't
	 * happen but does.
	 */
	if ((int)LINE_LENGTH(line) < curscolStart)
		ttysw_vpos(ttysw, ttysw->cursrow, curscolStart);
	/*
	 * Stick characters in line.
	 */
	for (sTmp = s; *sTmp != '\0'; sTmp++) {
		line[curscolTmp] = *sTmp;
		bold[curscolTmp] = ttysw->boldify;
		curscolTmp++;
	}
	/*
	 * Set new line length.
	 */
	if ((int)LINE_LENGTH(line) < curscolTmp)
		setlinelength(ttysw, line, curscolTmp);
	/*
	 * if (sTmp>(s+3)) printf("%d\n",sTmp-s);
	 */
	/* Note: curscolTmp should equal curscol here */
	/*
	 * if (curscolTmp!=ttysw->curscol) printf("csurscolTmp=%d, curscol=%d\n",
	 * curscolTmp,ttysw->curscol);
	 */
	ttysw_pstring(ttysw, s, ttysw->boldify, curscolStart, ttysw->cursrow,
			PIX_SRC);
}

static void ttysw_swap(Ttysw_private ttysw, int a, int b)
{
    char           *tmpline = ttysw->image[a];
    char           *tmpbold = ttysw->screenmode[a];

    ttysw->image[a] = ttysw->image[b];
    ttysw->image[b] = tmpline;
    ttysw->screenmode[a] = ttysw->screenmode[b];
    ttysw->screenmode[b] = tmpbold;
}

#ifdef JF
Pkg_private void ttysw_cim_scroll(Ttysw_private ttysw, int n)
{
    register int    new;

#ifdef DEBUG_LINES
    printf(" ttysw_cim_scroll(%d)	\n", n);
#endif
    if (n > 0) {		/* text moves UP screen	 */
	(void) ttysw_delete_lines(ttysw, ttysw->ttysw_top, n);
    } else {			/* (n<0)	text moves DOWN	screen	 */
	new = ttysw->ttysw_bottom + n;
	ttysw_roll(ttysw, ttysw->ttysw_top, new, ttysw->ttysw_bottom);
	ttysw_pcopyscreen(ttysw, ttysw->ttysw_top, ttysw->ttysw_top - n, new);
	ttysw_cim_clear(ttysw, ttysw->ttysw_top, ttysw->ttysw_top - n);
    }
}

#else

static void ttysw_swapregions(int a, int b, int n)
{
    while (n--) ttysw_swap(a++, b++);
}

Pkg_private void ttysw_cim_scroll(int toy, int fromy)
{

	if (toy < fromy)	/* scrolling up */
		(void)ttysw_roll(toy, ttysw->ttysw_bottom, fromy);
	else
		ttysw_swapregions(fromy, toy, ttysw->ttysw_bottom - toy);
	if (fromy > toy) {
		ttysw_pcopyscreen(fromy, toy, ttysw->ttysw_bottom - fromy);
		ttysw_cim_clear(ttysw->ttysw_bottom - (fromy-toy), ttysw->ttysw_bottom);
		/* move text up */
	}
	else {
		ttysw_pcopyscreen(fromy, toy, ttysw->ttysw_bottom - toy);
		ttysw_cim_clear(fromy, ttysw->ttysw_bottom - (toy-fromy));	/* down */
	}
}

#endif

Pkg_private void ttysw_insert_lines(Ttysw_private ttysw, int where, int n)
{
	register int new = where + n;

#ifdef DEBUG_LINES
	printf(" ttysw_insert_lines(%d,%d) ttysw_bottom=%d	\n", where, n,
			ttysw->ttysw_bottom);
#endif

	if (new > ttysw->ttysw_bottom)
		new = ttysw->ttysw_bottom;
	ttysw_roll(ttysw, where, new, ttysw->ttysw_bottom);
	(void)ttysw_pcopyscreen(ttysw, where, new, ttysw->ttysw_bottom - new);
	ttysw_cim_clear(ttysw, where, new);
}

/* BUG ALERT:  Externally visible procedure without a valid XView prefix. */
Pkg_private void ttysw_delete_lines(Ttysw_private ttysw, int where, int n)
{
	register int new = where + n;

#ifdef DEBUG_LINES
	printf(" ttysw_delete_lines(%d,%d)	\n", where, n);
#endif

	if (new > ttysw->ttysw_bottom) {
		n -= new - ttysw->ttysw_bottom;
		new = ttysw->ttysw_bottom;
	}
	ttysw_roll(ttysw, where, ttysw->ttysw_bottom - n, ttysw->ttysw_bottom);
	ttysw_pcopyscreen(ttysw, new, where, ttysw->ttysw_bottom - new);
	ttysw_cim_clear(ttysw, ttysw->ttysw_bottom - n, ttysw->ttysw_bottom);
}

static void reverse(Ttysw_private ttysw, int a, int b)
{
    b--;
    while (a < b) ttysw_swap(ttysw, a++, b--);
}

static void ttysw_roll(Ttysw_private ttysw, int first, int mid, int last)
{

    /* printf("first=%d, mid=%d, last=%d\n", first, mid, last); */
    reverse(ttysw, first, last);
    reverse(ttysw, first, mid);
    reverse(ttysw, mid, last);
}

Pkg_private void ttysw_cim_clear(Ttysw_private ttysw, int a, int b)
{
	register int i;

	for (i = a; i < b; i++)
		setlinelength(ttysw, ttysw->image[i], 0);
	ttysw_pclearscreen(ttysw, a, b);
	if (a == ttysw->ttysw_top && b == ttysw->ttysw_bottom) {
		if (ttysw_delaypainting)
			ttysw_pdisplayscreen(ttysw, TRUE, FALSE);
		else
			ttysw_delaypainting = 1;
	}
}

Pkg_private void ttysw_deleteChar(Ttysw_private ttysw, int fromcol, int tocol,
								int row)
{
    char           *line = ttysw->image[row];
    char           *bold = ttysw->screenmode[row];
#ifndef SVR4
    int             len = LINE_LENGTH(line);
#else
    int             len = (int)LINE_LENGTH(line);
#endif /* ~SVR4 */

    if (fromcol >= tocol)
	return;

    if (tocol < len) {
	/*
	 * There's a fragment left at the end
	 */
	int             gap = tocol - fromcol;
	{
		register char  *a = line + fromcol;
		register char  *b = line + tocol;
	    register char  *am = bold + fromcol;
	    register char  *bm = bold + tocol;
	    while ((*a++ = *b++)) *am++ = *bm++;
	}
	setlinelength(ttysw, line, len - gap);
	ttysw_pcopyline(ttysw, fromcol, tocol, len - tocol, row);
	ttysw_pclearline(ttysw, len - gap, len, row);
    } else if (fromcol < len) {
	setlinelength(ttysw, line, fromcol);
	ttysw_pclearline(ttysw, fromcol, len, row);
    }
}

Pkg_private void ttysw_insertChar(Ttysw_private ttysw, int fromcol, int tocol, int row)
{
    register char  *line = ttysw->image[row];
    register char  *bold = ttysw->screenmode[row];
    int             len = LINE_LENGTH(line);
    register int    i;
    int             delta, newlen, slug, rightextent;

    if (fromcol >= tocol || fromcol >= len)
	return;
    delta = tocol - fromcol;
    newlen = len + delta;
    if (newlen > ttysw->ttysw_right)
	newlen = ttysw->ttysw_right;
    if (tocol > ttysw->ttysw_right)
	tocol = ttysw->ttysw_right;
    for (i = newlen; i >= tocol; i--) {
	line[i] = line[i - delta];
	bold[i] = bold[i - delta];
    }
    for (i = fromcol; i < tocol; i++) {
	line[i] = ' ';
	bold[i] = MODE_CLEAR;
    }
    setlinelength(ttysw, line, newlen);
    rightextent = len + (tocol - fromcol);
    slug = len - fromcol;
    if (rightextent > ttysw->ttysw_right)
	slug -= rightextent - ttysw->ttysw_right;
    ttysw_pcopyline(ttysw, tocol, fromcol, slug, row);
    ttysw_pclearline(ttysw, fromcol, tocol, row);
}
