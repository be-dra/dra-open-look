/*      @(#)xview_xvin.h 1.14 93/06/28 SMI   DRA: $Id: xview_xvin.h,v 4.1 2024/03/28 19:35:11 dra Exp $      */

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

#ifndef xview_xview_xvin_DEFINED
#define xview_xview_xvin_DEFINED

/*
 ***********************************************************************
 *			Include Files
 ***********************************************************************
 */

#include <signal.h>

#include <sys/types.h>
#ifdef OLD_UNUSED
#include <pixrect/pixrect.h>
#include <pixrect/pr_planegroups.h>
#include <pixrect/pr_util.h>
#endif /* OLD_UNUSED */

#ifdef __STDC__
#ifndef CAT
#define CAT(a,b)        a ## b
#endif
#endif
#ifdef OLD_UNUSED
#include <pixrect/memvar.h>

#include <pixrect/pixfont.h>
#include <pixrect/traprop.h>
#include <pixrect/pr_line.h>
#endif /* OLD_UNUSED */

#if defined(__cplusplus) || defined(__STDC__)
#include <stdlib.h>
#endif /* __cplusplus || __STDC__ */

#include <xview/xv_c_types.h>   
#include <xview/generic.h>
#include <xview/server.h>
#include <xview/screen.h>

#include <xview/notify.h>
#ifdef OLD_UNUSED
#include <xview/pixwin.h>
#endif /* OLD_UNUSE */
#include <xview/win_input.h>

#endif /* ~xview_xview_xvin_DEFINED */
