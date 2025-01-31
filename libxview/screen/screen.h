 /*      @(#)screen.h 20.37 93/06/28 SMI DRA: RCS: $Id: screen.h,v 4.5 2025/01/09 16:54:26 dra Exp $     */

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL_NOTICE 
 *	file for terms of the license.
 */

#ifndef xview_screen_DEFINED
#define xview_screen_DEFINED

/*
 ***********************************************************************
 *			Include Files
 ***********************************************************************
 */

#include <xview/generic.h>

/*
 ***********************************************************************
 *			Definitions and Macros
 ***********************************************************************
 */

#ifndef XV_ATTRIBUTES_ONLY

/*
 * PUBLIC #defines 
 */

#define	SCREEN				&xv_screen_pkg

/*
 * PRIVATE #defines 
 */

#define SCREEN_TYPE			ATTR_PKG_SCREEN

#endif /* ~XV_ATTRIBUTES_ONLY */

#define SCREEN_ATTR(type, ordinal)      ATTR(ATTR_PKG_SCREEN, type, ordinal)

/*
 ***********************************************************************
 *		Typedefs, Enumerations, and Structures
 ***********************************************************************
 */

#ifndef XV_ATTRIBUTES_ONLY

typedef	Xv_opaque 	Xv_Screen;
typedef	Xv_opaque 	Xv_screen;

#endif /* ~XV_ATTRIBUTES_ONLY */

typedef enum {
	/*
	 * Public attributes 
	 */
	SCREEN_NUMBER	  		     = SCREEN_ATTR(ATTR_INT, 10),
	SCREEN_SERVER	  		     = SCREEN_ATTR(ATTR_OPAQUE, 15),
	SCREEN_OLWM_MANAGED          = SCREEN_ATTR(ATTR_BOOLEAN, 25), /* G-- */
	SCREEN_ENHANCED_OLWM         = SCREEN_ATTR(ATTR_BOOLEAN, 26), /* G-- */
	SCREEN_CHECK_SUN_WM_PROTOCOL = SCREEN_ATTR(ATTR_STRING,	27), /* G-- */
	SCREEN_BUSY_CURSOR 		     = SCREEN_ATTR(ATTR_OPAQUE, 17),
	SCREEN_UI_STYLE 		     = SCREEN_ATTR(ATTR_ENUM, 16),
	/*
	 * Private attributes 
	 */
	SCREEN_DEFAULT_VISUAL		= SCREEN_ATTR(ATTR_OPAQUE,      75),  /* G-- */
	SCREEN_VISUAL		       	= SCREEN_ATTR(ATTR_OPAQUE,	80),  /* G-- */
	/* Format: xv_get(screen, SCREEN_VISUAL, vinfo_template, vinfo_mask); */
	SCREEN_IMAGE_VISUAL		= SCREEN_ATTR(ATTR_OPAQUE,      85),  /* G-- */
	/* Format: xv_get(screen, SCREEN_IMAGE_VISUAL, xid, depth); */ 
        SCREEN_DEFAULT_CMS      	= SCREEN_ATTR(ATTR_OPAQUE,     	30),
        SCREEN_RETAIN_WINDOWS   	= SCREEN_ATTR(ATTR_BOOLEAN,    	40),
	SCREEN_BG1_PIXMAP		= SCREEN_ATTR(ATTR_OPAQUE,	50),
	SCREEN_BG2_PIXMAP		= SCREEN_ATTR(ATTR_OPAQUE,	55),
	SCREEN_BG3_PIXMAP		= SCREEN_ATTR(ATTR_OPAQUE,	60),
	SCREEN_GINFO			= SCREEN_ATTR(ATTR_OPAQUE,	65),
	SCREEN_OLGC_LIST		= SCREEN_ATTR(ATTR_OPAQUE,      70),  /* G-- */
	SCREEN_SUN_WINDOW_STATE		= SCREEN_ATTR(ATTR_BOOLEAN,     90),
	SCREEN_SELECTION_STATE		= SCREEN_ATTR(ATTR_LONG,        95),
	SCREEN_INPUT_PIXEL		= SCREEN_ATTR(ATTR_OPAQUE,        96)

} Screen_attr;

typedef enum {
	SCREEN_UIS_2D_BW,
	SCREEN_UIS_2D_COLOR,
	SCREEN_UIS_3D_COLOR
} screen_ui_style_t;

/* Define the different types of GC available in the GC list */
#define SCREEN_SET_GC		0
#define SCREEN_CLR_GC		1
#define SCREEN_TEXT_GC		2
#define SCREEN_BOLD_GC		3
#define SCREEN_GLYPH_GC		4
#define SCREEN_INACTIVE_GC	5
#define SCREEN_DIM_GC		6
#define SCREEN_INVERT_GC	7
#define SCREEN_NONSTD_GC	8	/* Color or non-standard font */
#define SCREEN_RUBBERBAND_GC	9
#define SCREEN_CARET_GC	10
#define SCREEN_HELP_GC	11
#define SCREEN_MENUSHADOW_GC	12
#define SCREEN_JOINPREVIEW_GC	13
#define SCREEN_BITMAP_GC		14

#define SCREEN_OLGC_LIST_SIZE	15

#ifndef XV_ATTRIBUTES_ONLY

typedef struct {
    Xv_generic_struct	parent;
    Xv_opaque		private_data;
} Xv_screen_struct;

#endif /* ~XV_ATTRIBUTES_ONLY */

/*
 ***********************************************************************
 *				Globals
 ***********************************************************************
 */

/*
 * PUBLIC Variables 
 */

extern Xv_Screen	xv_default_screen;

/*
 * PRIVATE Variables 
 */

extern Xv_pkg	xv_screen_pkg;

#endif /* ~xview_screen_DEFINED */
