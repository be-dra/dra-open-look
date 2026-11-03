#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)dndimpl.h 1.15 93/06/28 DRA: $Id: dndimpl.h,v 4.9 2026/11/02 10:14:57 dra Exp $ ";
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
 */
#define DND_EXPECT_NEW_PREVIEW_EVENT (1<<24)

/* this denotes a "pseudo drop site": it has been constructed by 
 * the drop sita data manager (= olwm) from the XdndAware property
 */
#define DND_XDND_AWARE        (1<<25)

Pkg_private int DndSendEvent(Display *dpy, XEvent *event, const char *nam);

#endif  /* ~xview_dndimpl_DEFINED */
