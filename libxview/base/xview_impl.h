#ifndef lint
#ifdef sccs
static	char sccsid[] = "@(#)xview_impl.h 1.13 93/06/28  DRA: $Id: xview_impl.h,v 4.1 2024/03/28 19:35:11 dra Exp $";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

#ifndef xview_xview_impl_DEFINED
#define xview_xview_impl_DEFINED

#include <X11/Xlib.h>

/*
 * This file should contain declarations that are internal to 
 * Sunview library, but are not internal to any particular package
 */

extern int xv_set_embedding_data(Xv_opaque object, Xv_opaque std_object);
extern int xv_has_been_initialized(void);
extern void xv_connection_error(char *);
extern int xv_x_error_handler(Display *dpy, XErrorEvent *event);
extern void xv_set_default_font(Display *display, int screen, Font font);
Xv_private void xv_get_cmdline_str(char	*str);
Xv_private void xv_get_cmdline_argv(char **argv, int *argc_ptr);

#endif /* xview_xview_impl_DEFINED */
