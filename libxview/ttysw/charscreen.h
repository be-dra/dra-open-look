/*	@(#)charscreen.h 20.12 93/06/28 SMIa DRA: $Id: charscreen.h,v 4.1 2024/03/28 19:24:33 dra Exp $	*/

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */

/*
 * Definitions relating to physical screen image.
 */

/*
 * Macros to convert character coordinates to pixel coordinates.
 */
#define row_to_y(row)	((row)*chrheight)
#define col_to_x(col)	(((col)*chrwidth) + chrleftmargin)
#define y_to_row(y)	((y)/chrheight)
#define x_to_col(x)	((((x) >= chrleftmargin) ? \
			  ((x) - chrleftmargin) : 0)/chrwidth)

/*
 * Character dimensions (fixed width fonts only!)
 * and of screen in pixels.
 */
Xv_private int	chrheight, chrwidth, chrbase;
Xv_private int	winheightp, winwidthp;
Xv_private int	chrleftmargin;

Xv_private struct	pixfont *pixfont;

/*
 * If delaypainting, delay painting.  Set when clear screen.
 * When input will block then paint characters (! white space) of entire image
 * and turn delaypainting off.
 */
Xv_private int	delaypainting;

Pkg_private void ttysw_pstring(CHAR *s,
				/* dieser Compiler warnt immer ueber 'different width'
				 * bei formalen Parametern von Typ char....
				 */
				int mode,
				int col, int row, int op);
void	ttysw_pclearline(int fromcol, int tocol, int row);
void	ttysw_pcopyline(int fromcol, int tocol, int count, int row);
Pkg_private void ttysw_pclearscreen( int fromrow, int torow);
