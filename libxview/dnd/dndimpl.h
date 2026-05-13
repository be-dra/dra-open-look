#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)dndimpl.h 1.15 93/06/28 DRA: $Id: dndimpl.h,v 4.7 2026/05/13 14:07:07 dra Exp $ ";
#endif
#endif

/*
 *      (c) Copyright 1990 Sun Microsystems, Inc. Sun design patents
 *      pending in the U.S. and foreign countries. See LEGAL NOTICE
 *      file for terms of the license.
 */

#ifndef xview_dndimpl_DEFINED
#define xview_dndimpl_DEFINED

#include <sys/time.h>
#include <X11/Xlib.h>

#define XDND_MY_VERSION 5
#define DND_EXPECT_NEW_PREVIEW_EVENT (1<<8)

Pkg_private int DndSendEvent(Display *dpy, XEvent *event, const char *nam);

#endif  /* ~xview_dndimpl_DEFINED */
