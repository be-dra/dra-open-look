/*	@(#)attrol.h 20.11 88/09/05	DRA: RCS: $Id: attrol.h,v 4.6 2025/05/31 10:44:58 dra Exp $ */

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL_NOTICE 
 *	file for terms of the license.
 */

#ifndef xview_attrol_DEFINED
#define	xview_attrol_DEFINED

/*
 ***********************************************************************
 *			Include Files
 ***********************************************************************
 */

#include <xview/base.h>
#include <xview/attr.h>

/*
 ***********************************************************************
 *			Definitions and Macros
 ***********************************************************************
 */

/*
 * These are the packages that are part of the XView OL library.
 *
 * IMPORTANT NOTE:  The attr id numbers start where the "intrinsics"
 *		    attr ids leave off.  The range of valid values
 *		    for these objects is [64..128) where 
 *		    ATTR_PKG_LAST_VALUE must be less than 128 and all
 *		    ATTR_PKG_ ids defined here must fit in the range 
 *		    (ATTR_PKG_LAST_VALUE..128).
 *
 *		    Be sure to check the value of ATTR_PKG_LAST_VALUE
 *		    when adding any new packages.
 */

#define ATTR_PKG_CANVAS		     (ATTR_PKG_LAST_VALUE +  1)
#define ATTR_PKG_ENTITY   	     (ATTR_PKG_LAST_VALUE +  2)
#define ATTR_PKG_TERMSW		     (ATTR_PKG_LAST_VALUE +  3)
#define ATTR_PKG_FRAME		     (ATTR_PKG_LAST_VALUE +  4)	
#define ATTR_PKG_ICON		     (ATTR_PKG_LAST_VALUE +  5)
#define ATTR_PKG_MENU		     (ATTR_PKG_LAST_VALUE +  6)
#define ATTR_PKG_PANEL		     (ATTR_PKG_LAST_VALUE +  7)
#define ATTR_PKG_OPENWIN         (ATTR_PKG_LAST_VALUE +  8)
#define ATTR_PKG_TEXTSW		     (ATTR_PKG_LAST_VALUE +  9)
#define ATTR_PKG_TTY		     (ATTR_PKG_LAST_VALUE + 10)
#define ATTR_PKG_NOTICE		     (ATTR_PKG_LAST_VALUE + 11)
#define ATTR_PKG_HELP		     (ATTR_PKG_LAST_VALUE + 12)
#define ATTR_PKG_TEXTSW_VIEW	 (ATTR_PKG_LAST_VALUE + 13)
#define ATTR_PKG_PANEL_VIEW	     (ATTR_PKG_LAST_VALUE + 14)
#define ATTR_PKG_CANVAS_VIEW     (ATTR_PKG_LAST_VALUE + 15)
#define ATTR_PKG_CANVAS_PAINT_WINDOW (ATTR_PKG_LAST_VALUE + 16)
#define ATTR_PKG_TTY_VIEW 	     (ATTR_PKG_LAST_VALUE + 17)
#define ATTR_PKG_TERMSW_VIEW 	 (ATTR_PKG_LAST_VALUE + 18)
#define ATTR_PKG_SCROLLBAR 	     (ATTR_PKG_LAST_VALUE + 19)

/* See REMIND in attr.h before adding any new pkgs. */
/* Selection package is using (ATTR_PKG_LAST_VALUE + 20) */

#define ATTR_PKG_FILE_CHOOSER 	 (ATTR_PKG_LAST_VALUE + 21)
#define ATTR_PKG_FILE_LIST 	     (ATTR_PKG_LAST_VALUE + 22)
#define ATTR_PKG_HIST 	     	 (ATTR_PKG_LAST_VALUE + 23)
#define ATTR_PKG_PATH	 	     (ATTR_PKG_LAST_VALUE + 24)

#define ATTR_PKG_PANEL_COLOR_TEXT   (ATTR_PKG_LAST_VALUE+25)
#define ATTR_PKG_COLOR_CHOOSER      (ATTR_PKG_LAST_VALUE+26)
#define ATTR_PKG_GROUP              (ATTR_PKG_LAST_VALUE+27)
#define ATTR_PKG_PROCESS            (ATTR_PKG_LAST_VALUE+28)
#define ATTR_PKG_SCROLL             (ATTR_PKG_LAST_VALUE+29)
#define ATTR_PKG_PERMPROP           (ATTR_PKG_LAST_VALUE+30)
#define ATTR_PKG_LISTPROP           (ATTR_PKG_LAST_VALUE+31)
#define ATTR_PKG_PERM_LIST          (ATTR_PKG_LAST_VALUE+32)
#define ATTR_PKG_ACCEL              (ATTR_PKG_LAST_VALUE+33)
#define ATTR_PKG_FUNCTION_KEYS      (ATTR_PKG_LAST_VALUE+34)
#define ATTR_PKG_BITMAP             (ATTR_PKG_LAST_VALUE+35)
#define ATTR_PKG_FONT_PROPS         (ATTR_PKG_LAST_VALUE+36)
#define ATTR_PKG_PIXMAP_SCROLLER    (ATTR_PKG_LAST_VALUE+37)
#define ATTR_PKG_FILE_REQ           (ATTR_PKG_LAST_VALUE+38)
#define ATTR_PKG_FILEDRAG           (ATTR_PKG_LAST_VALUE+39)
#define ATTR_PKG_ICCC               (ATTR_PKG_LAST_VALUE+40)
#define ATTR_PKG_SHORTCUTS          (ATTR_PKG_LAST_VALUE+41)
#define ATTR_PKG_RICHTEXT           (ATTR_PKG_LAST_VALUE+42)
#define ATTR_PKG_MLLIST             (ATTR_PKG_LAST_VALUE+43)
#define ATTR_PKG_QUICK              (ATTR_PKG_LAST_VALUE+44)
#define ATTR_PKG_TALK               (ATTR_PKG_LAST_VALUE+45)
#define ATTR_PKG_DIR                (ATTR_PKG_LAST_VALUE+46)
#define ATTR_PKG_FILEMGR            (ATTR_PKG_LAST_VALUE+47)
#define ATTR_PKG_GRAPHWIN           (ATTR_PKG_LAST_VALUE+48)


#endif /* ~xview_attrol_DEFINED */
