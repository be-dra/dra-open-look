/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */

/* $Id: openwin.h,v 4.1 2024/03/28 18:20:25 dra Exp $ */

#ifndef	xview_openwin_DEFINED
#define	xview_openwin_DEFINED

/*
 * Module:	openwin.h
 * Product:	SunView 2.0
 *
 * Level:	Public
 *
 * Description:
 *
 *	Defines attributes for the window that implements a LAF
 *  subwindow.
 *
 */


/*
 ***********************************************************************
 *			Include Files
 ***********************************************************************
 */

#include <xview/window.h>
#include <xview/attrol.h>

/*
 ***********************************************************************
 *			Definitions and Macros
 ***********************************************************************
 */

/*
 * PUBLIC #defines
 */

#define OPENWIN   		&xv_openwin_pkg
#define OPENWIN_VIEW	&xv_openwin_view_pkg

#define OPENWIN_SPLIT_NEWVIEW_IN_PLACE 	-1
#define OPENWIN_CANNOT_EXPAND		-1

#define	OPENWIN_EACH_VIEW(openwin, view)	\
   {int _view_cnt = 0; 				\
   while (((view) = (Xv_Window) xv_get((openwin), OPENWIN_NTH_VIEW, _view_cnt++)) != XV_NULL) { 					\

#define	OPENWIN_END_EACH	}}


#define OPENWIN_EACH_PW(ow, pw)\
   {int __pw_c_=0; while(((pw)=xv_get((ow),OPENWIN_NTH_PW,__pw_c_++))) {
 

/*
 * PRIVATE #defines
 */

#ifndef ATTR_PKG_OPENWIN
#define ATTR_PKG_OPENWIN 		(ATTR_PKG_UNUSED_FIRST + 8)
#endif
#define	OPENWIN_ATTR(type, ordinal)	ATTR(ATTR_PKG_OPENWIN, type, ordinal)
#define OPENWIN_ATTR_LIST(ltype, type, ordinal) \
    OPENWIN_ATTR(ATTR_LIST_INLINE((ltype), (type)), (ordinal))

#define OPENWIN_SET_GC		0
#define OPENWIN_CLR_GC		1
#define OPENWIN_TEXT_GC		2
#define OPENWIN_BOLD_GC		3
#define OPENWIN_GLYPH_GC	4
#define OPENWIN_INACTIVE_GC	5
#define OPENWIN_DIM_GC		6
#define OPENWIN_INVERT_GC	7
#define OPENWIN_NONSTD_GC	8	/* Color or non-standard font */
#define OPENWIN_NBR_GCS		9

/*
 ***********************************************************************
 *		Typedefs, Enumerations, and Structures
 ***********************************************************************
 */

typedef Xv_opaque  Openwin;

typedef struct {
	Xv_window_struct	parent_data;
	Xv_opaque		private_data;
}  Xv_openwin;

typedef Xv_opaque  Openwin_view;

typedef struct {
    Xv_window_struct	parent_data;
    Xv_opaque		private_data;
} Xv_openwin_view;

typedef enum {
	OPENWIN_SPLIT_HORIZONTAL,
	OPENWIN_SPLIT_VERTICAL
} Openwin_split_direction;


/* for OPENWIN_RESIZABLE */   
typedef enum {                  
    OPENWIN_TOP = 1,
    OPENWIN_BOTTOM,           
    OPENWIN_LEFT,             
    OPENWIN_RIGHT             
} Openwin_resize_side;        

typedef enum {
	/*
	* Public Attributes
	*/
	OPENWIN_ADJUST_FOR_HORIZONTAL_SCROLLBAR
				= OPENWIN_ATTR(ATTR_BOOLEAN, 	 1),
	OPENWIN_ADJUST_FOR_VERTICAL_SCROLLBAR
				= OPENWIN_ATTR(ATTR_BOOLEAN, 	 5),
	OPENWIN_AUTO_CLEAR		= OPENWIN_ATTR(ATTR_BOOLEAN, 	10),
	OPENWIN_HORIZONTAL_SCROLLBAR	= OPENWIN_ATTR(ATTR_OPAQUE, 	15),
	OPENWIN_NVIEWS		= OPENWIN_ATTR(ATTR_INT, 	20),
	OPENWIN_NO_MARGIN		= OPENWIN_ATTR(ATTR_BOOLEAN, 	25),
	OPENWIN_NTH_VIEW     	= OPENWIN_ATTR(ATTR_OPAQUE, 	30),
	OPENWIN_SELECTED_VIEW	= OPENWIN_ATTR(ATTR_OPAQUE, 	35),
	OPENWIN_SHOW_BORDERS		= OPENWIN_ATTR(ATTR_BOOLEAN, 	40),
	OPENWIN_SPLIT		= OPENWIN_ATTR_LIST(ATTR_RECURSIVE, ATTR_AV, 45),
	OPENWIN_SPLIT_DIRECTION  	= OPENWIN_ATTR(ATTR_ENUM, 	50),
	OPENWIN_SPLIT_POSITION	= OPENWIN_ATTR(ATTR_X, 		55),
	OPENWIN_SPLIT_VIEW		= OPENWIN_ATTR(ATTR_OPAQUE, 	60),
	OPENWIN_SPLIT_VIEW_START	= OPENWIN_ATTR(ATTR_X, 		65),
	OPENWIN_VERTICAL_SCROLLBAR	= OPENWIN_ATTR(ATTR_OPAQUE, 	70),
	OPENWIN_VIEW_ATTRS		= OPENWIN_ATTR_LIST(ATTR_RECURSIVE, ATTR_AV, 75),
	OPENWIN_SPLIT_INIT_PROC	= OPENWIN_ATTR(ATTR_FUNCTION_PTR, 76),
	OPENWIN_SPLIT_DESTROY_PROC	= OPENWIN_ATTR(ATTR_FUNCTION_PTR, 77),

	OPENWIN_NTH_PW     = OPENWIN_ATTR(ATTR_OPAQUE, 8),               /* --G */
	OPENWIN_PW_ATTRS   = OPENWIN_ATTR_LIST(ATTR_RECURSIVE,ATTR_AV,9),/* CS- */
	OPENWIN_SELECTABLE = OPENWIN_ATTR(ATTR_BOOLEAN, 11),             /* C-G */
	OPENWIN_RESIZABLE  = OPENWIN_ATTR_LIST(ATTR_NULL,ATTR_ENUM,12),  /* C-- */
	OPENWIN_RESIZE_VERIFY_PROC= OPENWIN_ATTR(ATTR_FUNCTION_PTR,13),  /* CSG */

	/*
	* Private Attributes
	*/
	OPENWIN_VIEW_CLASS	   = OPENWIN_ATTR(ATTR_OPAQUE,  	80),

	OPENWIN_PW_CLASS        = OPENWIN_ATTR(ATTR_OPAQUE, 33),      /* --G */
	OPENWIN_SEL_OWNER_CLASS = OPENWIN_ATTR(ATTR_OPAQUE, 34),      /* --G */

	/* use on Openwin_views */
    OPENWIN_VIEW_PAINT_WINDOW = OPENWIN_ATTR(ATTR_OPAQUE, 100)    /* --G */
} Openwin_attribute;


/*
 *Public Functions
 */
_XVFUNCPROTOBEGIN
EXTERN_FUNCTION(void openwin_update_layout, (Openwin owin_public));
_XVFUNCPROTOEND

/*
 ***********************************************************************
 *				Globals
 ***********************************************************************
 */

extern Xv_pkg xv_openwin_pkg;
extern Xv_pkg xv_openwin_view_pkg;

#endif	 /* ~xview_openwin_DEFINED */
