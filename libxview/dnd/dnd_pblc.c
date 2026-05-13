#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)dnd_pblc.c 1.17 93/06/28 DRA: $Id: dnd_pblc.c,v 4.13 2026/05/13 13:51:06 dra Exp $ ";
#endif
#endif

/*
 *      (c) Copyright 1990 Sun Microsystems, Inc. Sun design patents
 *      pending in the U.S. and foreign countries. See LEGAL NOTICE
 *      file for terms of the license.
 */

#include <X11/Xatom.h>
#include <xview/xview.h>
#include <xview/notify.h>
#include <xview/dragdrop.h>
#include <xview_private/dndimpl.h>
#include <xview_private/draw_impl.h>
#include <xview_private/portable.h>
#ifdef SVR4
#include <stdlib.h>
#endif  /* SVR4 */
