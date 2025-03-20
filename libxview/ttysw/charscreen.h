/*	@(#)charscreen.h 20.12 93/06/28 SMIa DRA: $Id: charscreen.h,v 4.2 2025/03/19 21:33:50 dra Exp $	*/

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
#define row_to_y(row)	((row)*ttysw->chrheight)
#define col_to_x(col)	(((col)*ttysw->chrwidth) + ttysw->chrleftmargin)
#define y_to_row(y)	((y)/ttysw->chrheight)
#define x_to_col(x)	((((x) >= ttysw->chrleftmargin) ? \
			  ((x) - ttysw->chrleftmargin) : 0)/ttysw->chrwidth)

/*
 * Character dimensions (fixed width fonts only!)
 * and of screen in pixels.
 */

/*
 * If ttysw_delaypainting, delay painting.  Set when clear screen.
 * When input will block then paint characters (! white space) of entire image
 * and turn ttysw_delaypainting off.
 */
Xv_private int	ttysw_delaypainting;

struct ttysubwindow;

Pkg_private void ttysw_pstring(struct ttysubwindow *ttysw, CHAR *s,
				int mode,
				int col, int row, int op);
Pkg_private void ttysw_pclearline(struct ttysubwindow *ttysw, int fromcol, int tocol, int row);
Pkg_private void ttysw_pcopyline(struct ttysubwindow *ttysw, int fromcol, int tocol, int count, int row);
Pkg_private void ttysw_pclearscreen(struct ttysubwindow *ttysw,int fromrow, int torow);
