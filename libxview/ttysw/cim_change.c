#ifndef lint
char     cim_change_c_sccsid[] = "@(#)cim_change.c 20.19 93/06/28 DRA: $Id: cim_change.c,v 4.11 2026/08/04 20:07:41 dra Exp $";
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

#include <assert.h>
#include <xview_private/i18n_impl.h>
#include <sys/types.h>
#include <pixrect/pixrect.h>
#include <xview_private/ttyansi.h>
#include <xview_private/charscreen.h>
#include <xview_private/tty_impl.h>
#include <xview_private/charimage.h>
#include <xview_private/svr_impl.h>

Xv_private_data int	ttysw_delaypainting;

static void ttysw_roll(Ttysw_private ttysw, int first, int mid, int last);

#define JF

/* INCOMPLETE */
Pkg_private void ttysw_vpos(Ttysw_private ttysw, int row, int col)
{
	register char *line = ttysw->image[row];
	register char *bold = ttysw->screenmode[row];
	register int i;

	while ((int)LINE_LENGTH_CHARS(line) <= col) {
		bold[LINE_LENGTH_CHARS(line)] = MODE_CLEAR;
		i = LINE_LENGTH_BYTES(line);
		line[i] = ' ';
		line[-1]++;
		line[-2]++;
	}
	setlinelength(ttysw, line, ((int)LINE_LENGTH_CHARS(line)));
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

Pkg_private void ttysw_clear_mode(Ttysw_private ttysw)
{
    ttysw->boldify = MODE_CLEAR;
}

Pkg_private int column_to_byteoffset(const char *s, int col)
{
	int byteoff = 0;
	int tmpcol = 0;

	if (col == 0) return 0;
	while (tmpcol < col && *s) {
		int clen = mblen(s, MB_CUR_MAX);
		if (clen <= 0) clen = 1;
		++tmpcol;
		byteoff += clen;
		s += clen;
	}
	return byteoff;
}

Pkg_private void set_linelength(Ttysw_private ttysw, char *line, int column)
{
	int col = ((column)>ttysw->ttysw_right)?ttysw->ttysw_right:(column);
	int byteoff = column_to_byteoffset(line, col);

	line[byteoff] = '\0';
	line[-1] = (unsigned char)byteoff;
	line[-2] = (unsigned char)col; /* new */
}

Pkg_private void ttysw_writePartialLine(Ttysw_private ttysw, char *s,
								int curscolStart)
{
	register char *sTmp;
	register char *line = ttysw->image[ttysw->cursrow];
	register char *bold = ttysw->screenmode[ttysw->cursrow];
	register int curscolTmp = curscolStart;
	int byteoff = column_to_byteoffset(line, curscolStart);

	SERVERTRACE((888, "%s: ccs=%d, '%s', len %d\n", __FUNCTION__,
					curscolStart, s, strlen(s)));

	/* here came a lonely 0303 */
	if (_xv_is_multibyte) {
		assert(mblen(s, MB_CUR_MAX) > 0) ;
	}
	/* otherwise in the C locale german characters will return -1 */

	/*
	 * Fix line length if start is past end of line length. This shouldn't
	 * happen but does.
	 */
	/* we need to compare characters = columns */
	if (LINE_LENGTH_CHARS(line) < curscolStart) {
		/* hopefully never seen ... */
		fprintf(stderr, "UNWANTED: line length =%d, curscolStart=%d\n",
						LINE_LENGTH_CHARS(line), curscolStart);
		ttysw_vpos(ttysw, ttysw->cursrow, curscolStart);
	}
	/*
	 * Stick characters in line.
	 */
	for (sTmp = s; *sTmp != '\0'; ) {
		int j, clen = mblen(sTmp, MB_CUR_MAX);
		if (clen <= 0) clen = 1;

		for (j = 0; j < clen; j++) {
			line[byteoff++] = *sTmp++;
		}
		bold[curscolTmp] = ttysw->boldify;
		curscolTmp++;
	}
	/*
	 * Set new line length.
	 */
	if (LINE_LENGTH_CHARS(line) < curscolTmp) {
		setlinelength(ttysw, line, curscolTmp);
	}
	/*
	 * if (sTmp>(s+3)) printf("%d\n",sTmp-s);
	 */
	/* Note: curscolTmp should equal curscol here */
	/*
	 * if (curscolTmp!=ttysw->curscol) printf("csurscolTmp=%d, curscol=%d\n",
	 * curscolTmp,ttysw->curscol);
	 */
	SERVERTRACE((888, "%s: before pstring(%s), len %d\n", __FUNCTION__, s, strlen(s)));
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
	register int new;

#ifdef DEBUG_LINES
	printf(" ttysw_cim_scroll(%d)	\n", n);
#endif

	if (n > 0) {	/* text moves UP screen  */
		(void)ttysw_delete_lines(ttysw, ttysw->ttysw_top, n);
	}
	else {	/* (n<0)    text moves DOWN screen   */
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
	char *line = ttysw->image[row];
	char *bold = ttysw->screenmode[row];
	int len = (int)LINE_LENGTH_CHARS(line);

	if (fromcol >= tocol)
		return;

	if (tocol < len) {
		/*
		 * There's a fragment left at the end
		 */
		int gap = tocol - fromcol;

		{
			int fb, tb;
			register char *a = line + (fb = column_to_byteoffset(line,fromcol));
			register char *b = line + (tb = column_to_byteoffset(line, tocol));
			register char *am = bold + fb;
			register char *bm = bold + tb;

			while ((*a++ = *b++))
				*am++ = *bm++;
		}
		setlinelength(ttysw, line, len - gap);
		ttysw_pcopyline(ttysw, fromcol, tocol, len - tocol, row);
		ttysw_pclearline(ttysw, len - gap, len, row);
	}
	else if (fromcol < len) {
		setlinelength(ttysw, line, fromcol);
		ttysw_pclearline(ttysw, fromcol, len, row);
	}
}

Pkg_private void ttysw_insertChar(Ttysw_private ttysw, int fromcol, int tocol, int row)
{
	register char *line = ttysw->image[row];
	register char *bold = ttysw->screenmode[row];
	int len = LINE_LENGTH_CHARS(line);
	register int i;
	int delta, newlen, slug, rightextent;

	if (fromcol >= tocol || fromcol >= len)
		return;
	delta = tocol - fromcol; /* chars */
	newlen = len + delta; /* colums */
	if (newlen > ttysw->ttysw_right)
		newlen = ttysw->ttysw_right;
	if (tocol > ttysw->ttysw_right)
		tocol = ttysw->ttysw_right;

	/* INCOMPLETE - do we need any column_to_byteoffset here ?? */
	{
		for (i = newlen; i >= tocol; i--) {
			line[i] = line[i - delta];
			bold[i] = bold[i - delta];
		}
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
