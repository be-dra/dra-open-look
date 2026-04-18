#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)fm_set.c 20.110 93/06/28 DRA: $Id: fm_set.c,v 4.9 2026/04/18 07:00:41 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL_NOTICE 
 *	file for terms of the license.
 */

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <xview_private/i18n_impl.h>
#include <xview_private/fm_impl.h>
#include <xview_private/draw_impl.h>
/* ACC_XVIEW */
#include <xview/defaults.h>
#include <xview_private/svr_impl.h>
#include <xview_private/win_info.h>
/* ACC_XVIEW */
#include <xview/cms.h>
#include <xview/server.h>
#include <pixrect/pixrect.h>
#ifdef __STDC__ 
#ifndef CAT
#define CAT(a,b)        a ## b 
#endif 
#endif
#include <pixrect/memvar.h>

#ifndef NULL
#define NULL 0
#endif

