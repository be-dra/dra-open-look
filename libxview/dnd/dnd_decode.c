#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)dnd_decode.c 1.15 93/06/28 DRA: $Id: dnd_decode.c,v 4.10 2026/05/13 13:51:10 dra Exp $ ";
#endif
#endif

/*
 *      (c) Copyright 1990 Sun Microsystems, Inc. Sun design patents
 *      pending in the U.S. and foreign countries. See LEGAL NOTICE
 *      file for terms of the license.
 */

#include <sys/time.h>
#include <X11/Xatom.h>
#include <xview/xview.h>
#include <xview/server.h>
#include <xview/window.h>
#include <xview/sel_pkg.h>
#include <xview/dragdrop.h>
#include <xview_private/dndimpl.h>
#include <xview_private/xv_list.h>
#include <xview_private/svr_impl.h>
