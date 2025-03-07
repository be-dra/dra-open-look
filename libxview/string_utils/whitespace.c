#ifndef lint
char whitespace_c_sccsid[] = "@(#)whitespace.c 20.15 93/06/28 RCS: $Id: whitespace.c,v 4.2 2024/03/31 17:33:59 dra Exp $";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

#include <xview/str_utils.h>

/*
 * these two procedures are in a separate file because they are also defined
 * in libio_stream.a, and having them be in string_utils.c tends to make the
 * loader complain about things being multiply defined
 */

enum CharClass
xv_white_space(c)
    char            c;
{
    switch (c) {
      case ' ':
      case '\n':
      case '\t':
	return (Sepr);
      default:
	return (Other);
    }
}

/* ARGSUSED */
struct CharAction
xv_everything(c)
    char            c;
{
    static struct CharAction val = {False, True};

    return (val);
}
