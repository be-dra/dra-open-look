/*	@(#)charimage.h 20.14 93/06/28 SMI RCS: $Id: charimage.h,v 4.5 2026/08/04 18:21:38 dra Exp $	*/

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */

/*
 * Definitions relating to maintenance of virtual screen image.
 */

/* also in the multibyte case: this is the line length IN BYTES ! */
#define LINE_LENGTH(line)	syntax error
#define LINE_LENGTH_BYTES(line)	((unsigned char)((line)[-1]))
#define LINE_LENGTH_CHARS(line)	((unsigned char)((line)[-2]))

#define MODE_CLEAR	0
#define MODE_INVERT	1
#define MODE_UNDERSCORE	2
#define MODE_BOLD	4

#ifdef BEFORE_MULTIBYTE
#define	setlinelength(ttysw, line, column) \
	{ int _col = ((column)>ttysw->ttysw_right)?ttysw->ttysw_right:(column); \
	  (line)[(_col)] = '\0'; \
	  line[-1] = (unsigned char) (_col);}
#else
Pkg_private int column_to_byteoffset(const char *s, int col);
Pkg_private void set_linelength(Ttysw_private ttysw, char *line, int column);
#define	setlinelength(ttysw, line, column) set_linelength(ttysw, line, column)
#endif
