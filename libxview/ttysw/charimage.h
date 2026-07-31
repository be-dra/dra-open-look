/*	@(#)charimage.h 20.14 93/06/28 SMI RCS: $Id: charimage.h,v 4.4 2026/07/30 12:06:21 dra Exp $	*/

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */

/*
 * Definitions relating to maintenance of virtual screen image.
 */

/*
 * Screen is maintained as an array of characters.
 * Screen is bottom lines and right columns.
 * Each line has length and array of characters.
 * Characters past length position are undefined.
 * Line is otherwise null terminated.
 */
#ifdef ONLY_ONE_TTY_PER_PROCESS
extern CHAR	**image;
extern char	**screenmode;
extern int	ttysw_top, ttysw_bottom, ttysw_left, ttysw_right;
extern int	cursrow, curscol;
#endif /* ONLY_ONE_TTY_PER_PROCESS */

#define LINE_LENGTH(line)	((unsigned char)((line)[-1]))
#define MODE_CLEAR	0
#define MODE_INVERT	1
#define MODE_UNDERSCORE	2
#define MODE_BOLD	4

#define	setlinelength(ttysw, line, column) \
	{ int _col = ((column)>ttysw->ttysw_right)?ttysw->ttysw_right:(column); \
	  (line)[(_col)] = '\0'; \
	  line[-1] = (unsigned char) (_col);}
