#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)dndimpl.h 1.15 93/06/28 DRA: $Id: dndimpl.h,v 4.8 2026/09/17 21:25:35 dra Exp $ ";
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

/* this extends the drop site flags
 * (DND_ENTERLEAVE, DND_MOTION, DND_DEFAULT_SITE)
 * and denotes a "pseudo drop site": this has been constructed by 
 * the drop sita data manager (= olwm) from the XdndAware property
 */
#define DND_XDND_AWARE        (1<<4)
#define DND_EXPECT_NEW_PREVIEW_EVENT (1<<5)

Pkg_private int DndSendEvent(Display *dpy, XEvent *event, const char *nam);

#endif  /* ~xview_dndimpl_DEFINED */
