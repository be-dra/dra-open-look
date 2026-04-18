#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)frame_cmd.h 1.39 93/06/28 DRA: $Id: frame_cmd.h,v 4.5 2026/04/17 12:00:13 dra Exp $ ";
#endif
#endif

/***********************************************************************/
/*	                      frame_cmd.h		               */
/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL_NOTICE
 *	file for terms of the license.
 */
/***********************************************************************/

#ifndef _frame_cmd_h_already_included
#define _frame_cmd_h_already_included

/* standard includes */
#ifndef FILE
#if !defined(SVR4) && !defined(__linux)
#undef NULL
#endif  /* SVR4 */
#include <stdio.h>
#endif  /* FILE */
#include <sys/time.h>
#include <xview/notify.h>
#include <xview/rect.h>
#include <xview/rectlist.h>

#include <xview/win_struct.h>	/* for WL_ links */
#include <xview/win_input.h>

/* all this for wmgr.h */
#include <xview/win_screen.h>
#include <xview/wmgr.h>
#include <xview_private/wmgr_decor.h>

#include <xview/pkg.h>
#include <xview/attrol.h>
#include <xview_private/fm_impl.h>
#include <xview/icon.h>
#include <xview/openmenu.h>

/* all this for XWMHints */
#include <X11/Xlib.h>
#include <X11/Xutil.h>

/* frame_cmd.c */
Pkg_private Notify_value frame_cmd_input(Frame frame_public, Notify_event ev,
					Notify_arg arg, Notify_event_type type);

#endif
