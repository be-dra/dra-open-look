#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)frame.h 20.77 93/06/28 DRA: $Id: frame.h,v 4.9 2025/07/05 21:26:00 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL_NOTICE 
 *	file for terms of the license.
 */

#ifndef xview_frame_DEFINED
#define xview_frame_DEFINED

/*
 ***********************************************************************
 *			Include Files
 ***********************************************************************
 */

#include <xview/window.h>
#include <xview/attrol.h>
#include <X11/X.h>

/*
 ***********************************************************************
 *			Definitions and Macros
 ***********************************************************************
 */

/*
 * PUBLIC #defines 
 */

#define FRAME			FRAME_BASE
#define FRAME_BASE		&xv_frame_base_pkg
#define FRAME_CMD		&xv_frame_cmd_pkg
#define FRAME_PROPS		&xv_propframe_pkg
#define FRAME_HELP		&xv_frame_help_pkg
#define FRAME_CLASS		&xv_frame_class_pkg

#define	ROOT_FRAME		((Frame)0)

#define FRAME_SHOW_HEADER	FRAME_SHOW_LABEL
#define FRAME_FOCUS_UP_WIDTH	13
#define FRAME_FOCUS_UP_HEIGHT	13
#define FRAME_FOCUS_RIGHT_WIDTH		13
#define FRAME_FOCUS_RIGHT_HEIGHT	13

/*
 * Utility Macros 
 */

#define frame_fit_all(frame) 					\
{ 								\
    Xv_Window win; 						\
    int n = 1; 							\
    while ((win = xv_get(frame, FRAME_NTH_SUBWINDOW, n++, 0)))\
	window_fit(win); 					\
    window_fit(frame); 						\
}

#define frame_done_proc(frame) 					\
   (((void (*)())window_get(frame, FRAME_DONE_PROC))(frame))

#define frame_default_done_proc(frame) 				\
   (((void (*)())window_get(frame, FRAME_DEFAULT_DONE_PROC))(frame))

/*
 * PRIVATE #defines 
 */

#define	FRAME_ATTR(type, ordinal)	ATTR(ATTR_PKG_FRAME, type, ordinal)
#define FRAME_ATTR_LIST(ltype, type, ordinal) \
	FRAME_ATTR(ATTR_LIST_INLINE((ltype), (type)), (ordinal))
#define FRAME_ATTR_OPAQUE_5		ATTR_TYPE(ATTR_BASE_OPAQUE, 5)

/*
 * BUG: maybe these should be attributes 
 */


/*
 * width of border around a frame 
 */
#define FRAME_BORDER_WIDTH      (0)
/*
 * width of border around a subwindow 
 */
#define FRAME_SUBWINDOW_SPACING (FRAME_BORDER_WIDTH)


/* special value for PANEL_MULTILINE_TEXT's notify proc:
 * every character will return PANEL_INSERT
 */
#define FRAME_PROPS_MULTILINE_INSERT_ALL ((long)(-1))

/* do not align it, do not create a changebar for it */
#define FRAME_PROPS_NO_LAYOUT -2
/* move it with the aligned, do not create a changebar for it */
#define FRAME_PROPS_MOVE -3

/*
 * PUBLIC #defines 
 * For SunView 1 Compatibility
 */

#define FRAME_TYPE		ATTR_PKG_FRAME

#define	FRAME_ARGS		XV_INIT_ARGS
#define	FRAME_ARGC_PTR_ARGV	XV_INIT_ARGC_PTR_ARGV
#define	FRAME_CMDLINE_HELP_PROC	XV_USAGE_PROC
#define	FRAME_LABEL		XV_LABEL
#ifdef OW_I18N
#define	FRAME_LABEL_WCS		XV_LABEL_WCS
#endif
#define FRAME_OPEN_RECT		WIN_RECT

/*
 ***********************************************************************
 *		Typedefs, Enumerations, and Structures	
 ***********************************************************************
 */

typedef	Xv_opaque 	Frame;
typedef Xv_opaque	Frame_cmd;
typedef Xv_opaque	Frame_props;
typedef Xv_opaque	Frame_help;

typedef struct {
    Xv_window_struct	parent_data;
    Xv_opaque		private_data;
} Xv_frame_class;

typedef struct {
    Xv_frame_class	parent_data;
    Xv_opaque		private_data;
} Xv_frame_base;

typedef struct {
	Xv_frame_base parent_data;
    Xv_opaque      private_data;
} Xv_propframe;

typedef Xv_frame_base 	Xv_frame_cmd;
/* typedef Xv_frame_base 	Xv_frame_props; */
typedef Xv_frame_base 	Xv_frame_help;

typedef enum {
    FRAME_FOCUS_UP,
    FRAME_FOCUS_RIGHT
} Frame_focus_direction;

typedef enum {    /* do not change this order */
    FRAME_CMD_PIN_OUT,
    FRAME_CMD_PIN_IN
} Frame_cmd_pin_state;

typedef enum {
	FRAME_FOOTER_CREATE,
	FRAME_FOOTER_LEFT,
	FRAME_FOOTER_RIGHT
} Frame_footer_state;
typedef void (*frame_footer_proc_t)(Frame, Xv_window *footer,
								Frame_footer_state, char *);

typedef void (*frame_accel_notify_func)(Xv_opaque, Event *);

typedef struct frame_accelerator {
    short	    code;   /* = event->ie_code */
    KeySym	    keysym; /* X keysym */
    frame_accel_notify_func	notify_proc; /* accelerator notify proc */
    Xv_opaque	    data;   /* opaque data handle */
    struct frame_accelerator *next;
} Frame_accelerator;
 
typedef enum {
	/*
	 * PUBLIC attributes 
	 */
	FRAME_BACKGROUND_COLOR	= FRAME_ATTR(ATTR_SINGLE_COLOR_PTR,	   5),
	FRAME_BUSY		= FRAME_ATTR(ATTR_BOOLEAN,		  10),
	FRAME_CLOSED	= FRAME_ATTR(ATTR_BOOLEAN,		  15),
	FRAME_CLOSED_RECT	= FRAME_ATTR(ATTR_RECT_PTR,		  20),
	FRAME_WM_COMMAND_STRINGS= FRAME_ATTR_LIST(ATTR_NULL, ATTR_STRING, 21),
	FRAME_WM_COMMAND_ARGC_ARGV
				= FRAME_ATTR(ATTR_INT_PAIR, 		  22),
	FRAME_WM_COMMAND_ARGV	= FRAME_ATTR(ATTR_OPAQUE,		  23),
	FRAME_WM_COMMAND_ARGC	= FRAME_ATTR(ATTR_INT,			  24),
	FRAME_CMD_PANEL		= FRAME_ATTR(ATTR_OPAQUE,		  25),
	FRAME_CMD_PANEL_BORDERED = FRAME_ATTR(ATTR_BOOLEAN,		  26), /* by DRA */
	FRAME_CURRENT_RECT	= FRAME_ATTR(ATTR_RECT_PTR,		  35),
	FRAME_OLD_RECT          = FRAME_ATTR(ATTR_RECT_PTR,               36),
	FRAME_DEFAULT_DONE_PROC	= FRAME_ATTR(ATTR_FUNCTION_PTR,		  40),
	FRAME_DONE_PROC		= FRAME_ATTR(ATTR_FUNCTION_PTR,		  45),
	FRAME_FOCUS_WIN		= FRAME_ATTR(ATTR_INT_PAIR,		 165),
	FRAME_FOCUS_DIRECTION	= FRAME_ATTR(ATTR_ENUM,			 170),
	FRAME_FOREGROUND_COLOR	= FRAME_ATTR(ATTR_SINGLE_COLOR_PTR,	  50),
	FRAME_ICON		= FRAME_ATTR(ATTR_OPAQUE,		  55),
	FRAME_INHERIT_COLORS	= FRAME_ATTR(ATTR_BOOLEAN,		  60),
	FRAME_LEFT_FOOTER	= FRAME_ATTR(ATTR_STRING,		  65),
#ifdef OW_I18N
	FRAME_LEFT_FOOTER_WCS	= FRAME_ATTR(ATTR_WSTRING,		  66),
#endif
	FRAME_NEXT_PANE		= FRAME_ATTR(ATTR_NO_VALUE,		  67),
	FRAME_NO_CONFIRM	= FRAME_ATTR(ATTR_BOOLEAN,		  70),
	FRAME_NTH_SUBFRAME	= FRAME_ATTR(ATTR_INT,			  75),
	FRAME_NTH_SUBWINDOW	= FRAME_ATTR(ATTR_INT,			  80),
	FRAME_PREVIOUS_ELEMENT	= FRAME_ATTR(ATTR_NO_VALUE,		  81),
	FRAME_PREVIOUS_PANE	= FRAME_ATTR(ATTR_NO_VALUE,		  82),
	FRAME_PROPERTIES_PROC	= FRAME_ATTR(ATTR_FUNCTION_PTR,		  85),
	FRAME_CMD_PUSHPIN_IN	= FRAME_ATTR(ATTR_BOOLEAN,		 105),
	FRAME_CMD_DEFAULT_PIN_STATE = FRAME_ATTR(ATTR_ENUM,		 106),
	FRAME_CMD_PIN_STATE	= FRAME_ATTR(ATTR_ENUM,		 	 107),
	FRAME_RIGHT_FOOTER	= FRAME_ATTR(ATTR_STRING,		 115),
#ifdef OW_I18N
	FRAME_RIGHT_FOOTER_WCS	= FRAME_ATTR(ATTR_WSTRING,		 116),
#endif
	FRAME_SHOW_FOOTER	= FRAME_ATTR(ATTR_BOOLEAN,		 125),
	FRAME_SHOW_RESIZE_CORNER = FRAME_ATTR(ATTR_BOOLEAN,		 128),
	FRAME_SHOW_LABEL	= FRAME_ATTR(ATTR_BOOLEAN,		 130),
	FRAME_GROUP_LEADER	= FRAME_ATTR(ATTR_BOOLEAN,		 135),
	FRAME_MIN_SIZE		= FRAME_ATTR(ATTR_INT_PAIR,	 	 136),
	FRAME_MAX_SIZE		= FRAME_ATTR(ATTR_INT_PAIR,	 	 137),
        /* ACC_XVIEW */
	FRAME_MENUS		= FRAME_ATTR_LIST(ATTR_NULL,ATTR_OPAQUE,245),
	FRAME_MENU_ADD		= FRAME_ATTR(ATTR_OPAQUE,		 246),
	FRAME_MENU_DELETE	= FRAME_ATTR(ATTR_OPAQUE,		 247),
	FRAME_MENU_COUNT	= FRAME_ATTR(ATTR_INT,			 248),
        /* ACC_XVIEW */
	/*
	 * PRIVATE attributes 
	 */
	FRAME_NEXT_CHILD	= FRAME_ATTR(ATTR_OPAQUE,		 140),
	FRAME_PREVIOUS_CHILD	= FRAME_ATTR(ATTR_OPAQUE,		 142),
	FRAME_SCALE_STATE	= FRAME_ATTR(ATTR_INT,			 145),
	FRAME_SUBWINDOWS_ADJUSTABLE	
				= FRAME_ATTR(ATTR_BOOLEAN,		 150),
        FRAME_COUNT             = FRAME_ATTR(ATTR_INT,                   160),
	FRAME_FOCUS_UP_IMAGE	= FRAME_ATTR(ATTR_OPAQUE,		 175),
	FRAME_FOCUS_RIGHT_IMAGE	= FRAME_ATTR(ATTR_OPAQUE,		 180),
	FRAME_FOCUS_GC		= FRAME_ATTR(ATTR_OPAQUE,		 185),
	FRAME_ORPHAN_WINDOW	= FRAME_ATTR(ATTR_INT,			 190),
	FRAME_FOOTER_WINDOW	= FRAME_ATTR(ATTR_BOOLEAN,               195),
#ifdef OW_I18N
	FRAME_IMSTATUS_WINDOW	= FRAME_ATTR(ATTR_BOOLEAN,               196),
#endif	
	FRAME_ACCELERATOR	= FRAME_ATTR(ATTR_INT_TRIPLE,		 200),
	FRAME_X_ACCELERATOR	= FRAME_ATTR(ATTR_INT_TRIPLE,		 205),
        /* ACC_XVIEW */
	FRAME_MENU_ACCELERATOR	= FRAME_ATTR(ATTR_OPAQUE_TRIPLE,	 207),
	FRAME_MENU_REMOVE_ACCELERATOR	= FRAME_ATTR(ATTR_STRING,	 208),
	FRAME_MENU_X_ACCELERATOR	= FRAME_ATTR(FRAME_ATTR_OPAQUE_5,209),
#ifdef OW_I18N
	FRAME_MENU_ACCELERATOR_WCS	= FRAME_ATTR(ATTR_OPAQUE_TRIPLE, 250),
	FRAME_MENU_REMOVE_ACCELERATOR_WCS= FRAME_ATTR(ATTR_STRING,	 255),
#endif	
        /* ACC_XVIEW */
#ifdef OW_I18N
	FRAME_LEFT_IMSTATUS_WCS	= FRAME_ATTR(ATTR_WSTRING,		 210),
	FRAME_LEFT_IMSTATUS     = FRAME_ATTR(ATTR_STRING, 		 215),
	FRAME_RIGHT_IMSTATUS_WCS= FRAME_ATTR(ATTR_WSTRING,		 220),
        FRAME_RIGHT_IMSTATUS    = FRAME_ATTR(ATTR_STRING, 		 225),
	FRAME_SHOW_IMSTATUS	= FRAME_ATTR(ATTR_BOOLEAN,               230),
	FRAME_INACTIVE_IMSTATUS	= FRAME_ATTR(ATTR_BOOLEAN,               231),
	FRAME_CMD_POINTER_WARP	= FRAME_ATTR(ATTR_BOOLEAN,		 240),
#ifdef FULL_R5
/* This attr is private to XView */
	FRAME_IMSTATUS_RECT	= FRAME_ATTR(ATTR_RECT_PTR,		 242),
#endif /* FULL_R5 */	
#endif
	FRAME_COMPOSE_STATE	= FRAME_ATTR(ATTR_BOOLEAN,               235),
	FRAME_WINTYPE       = FRAME_ATTR(ATTR_LONG, 236),
	FRAME_FOOTER_HELP_KEY	= FRAME_ATTR(ATTR_INT, 232), 
	FRAME_FOOTER_HELP_PROC_KEY	= FRAME_ATTR(ATTR_INT, 233), 

	/* see OL GUI Spec, p 55 */
	FRAME_FOOTER_PROC       = FRAME_ATTR(ATTR_FUNCTION_PTR, 123),/* C-- */

	/* begin property frame: */
	FRAME_PROPS_APPLY_PROC          = FRAME_ATTR(ATTR_FUNCTION_PTR,86),/* CSG */
	FRAME_PROPS_RESET_PROC          = FRAME_ATTR(ATTR_FUNCTION_PTR,87),/* CSG */
	FRAME_PROPS_SET_DEFAULTS_PROC   = FRAME_ATTR(ATTR_FUNCTION_PTR,88),/* CSG */
	FRAME_PROPS_RESET_FACTORY_PROC  = FRAME_ATTR(ATTR_FUNCTION_PTR,89),/* CSG */
	FRAME_PROPS_CREATE_BUTTONS=FRAME_ATTR_LIST(ATTR_NULL,ATTR_OPAQUE,90),/*-S-*/
	FRAME_PROPS_CREATE_ITEM=FRAME_ATTR_LIST(ATTR_RECURSIVE,ATTR_AV,91), /*-S-*/
	FRAME_PROPS_ITEM_SPEC           = FRAME_ATTR(ATTR_OPAQUE_TRIPLE,92),  /* -S- */
	FRAME_PROPS_RESET_CHANGE_BARS   = FRAME_ATTR(ATTR_NO_VALUE,93),    /* -S- */
	FRAME_PROPS_ALIGN_ITEMS         = FRAME_ATTR(ATTR_NO_VALUE,94),    /* -S- */
	FRAME_PROPS_ITEM_CHANGED        = FRAME_ATTR(ATTR_OPAQUE_PAIR, 95),/* -S- */
	FRAME_PROPS_NEW_CATEGORY        = FRAME_ATTR(ATTR_OPAQUE_PAIR, 96),/* -SG */
	FRAME_PROPS_MULTIPLE_CATEGORIES = FRAME_ATTR(ATTR_BOOLEAN, 97),    /* C-G */
	FRAME_PROPS_MAX_ITEMS           = FRAME_ATTR(ATTR_INT, 98),        /* C-G */
	FRAME_PROPS_SWITCH_PROC         = FRAME_ATTR(ATTR_FUNCTION_PTR, 99),/*CSG */
	FRAME_PROPS_RESET_ON_SWITCH     = FRAME_ATTR(ATTR_BOOLEAN, 100),   /* CSG */
	FRAME_PROPS_TRIGGER             = FRAME_ATTR(ATTR_ENUM,101),       /* -S- */
	FRAME_PROPS_DATA_ADDRESS        = FRAME_ATTR(ATTR_OPAQUE,102),     /* CSG */
	FRAME_PROPS_DEFAULT_DATA_ADDRESS= FRAME_ATTR(ATTR_OPAQUE,103),     /* CSG */
	FRAME_PROPS_FACTORY_DATA_ADDRESS= FRAME_ATTR(ATTR_OPAQUE,104),     /* CSG */
	FRAME_PROPS_CREATE_CONTENTS_PROC= FRAME_ATTR(ATTR_FUNCTION_PTR,108),  /* CSG */
	FRAME_PROPS_TRIGGER_SLAVES      = FRAME_ATTR(ATTR_OPAQUE_TRIPLE,109), /* -S- */
	FRAME_PROPS_RESET_SLAVE_CBS     = FRAME_ATTR(ATTR_OPAQUE,110),     /* -S- */
	FRAME_PROPS_CATEGORY_LABEL      = FRAME_ATTR(ATTR_STRING,111),     /* C-G */
	FRAME_PROPS_CATEGORY_NCOLS      = FRAME_ATTR(ATTR_INT,112),        /* C-G */
	FRAME_PROPS_HAS_OPEN_CHANGES    = FRAME_ATTR(ATTR_BOOLEAN,113),    /* --G */
	FRAME_PROPS_APPLY_LABEL         = FRAME_ATTR(ATTR_STRING,114),     /* C-G */
	FRAME_PROPS_FRONT_CATEGORY      = FRAME_ATTR(ATTR_OPAQUE, 27),     /* -S- */
	FRAME_PROPS_READ_ONLY           = FRAME_ATTR(ATTR_BOOLEAN, 28),    /* -S- */

	/* BEGIN two selections: */
	FRAME_PROPS_HOLDER              = FRAME_ATTR(ATTR_OPAQUE, 201),    /* C-G */
	FRAME_PROPS_SECOND_SEL          = FRAME_ATTR(ATTR_BOOLEAN, 202),   /* -SG */
	/* END two selections */

	/* for use with CREATE_ITEM, ITEM_SPEC */
	FRAME_PROPS_DATA_OFFSET         = FRAME_ATTR(ATTR_INT,117),        /* -S- */
	FRAME_PROPS_CONVERTER           = FRAME_ATTR(ATTR_OPAQUE_PAIR, 51),/* -S- */
	FRAME_PROPS_SLAVE_OF            = FRAME_ATTR(ATTR_OPAQUE, 52)      /* -S- */
} Frame_attribute;

/* BEGIN two selections: */
typedef enum {
	FRAME_PROPS_APPLY_ORIG,
	FRAME_PROPS_APPLY_NEW,
	FRAME_PROPS_APPLY_RELEASE
} Propframe_apply_mode;
/* END two selections */

typedef enum {
	FRAME_PROPS_APPLY = 1,
	FRAME_PROPS_RESET,
	FRAME_PROPS_SET_DEFAULTS,
	FRAME_PROPS_RESET_FACTORY
} Propframe_buttons;

#define	FRAME_PROPS_PUSHPIN_IN	FRAME_CMD_PUSHPIN_IN
#define	FRAME_PROPS_PANEL	FRAME_CMD_PANEL

/*
 *  values for scale state
 */
#define Frame_rescale_state	Window_rescale_state
#define FRAME_SCALE_SMALL	WIN_SCALE_SMALL
#define FRAME_SCALE_MEDIUM	WIN_SCALE_MEDIUM
#define FRAME_SCALE_LARGE	WIN_SCALE_LARGE
#define FRAME_SCALE_EXTRALARGE	WIN_SCALE_EXTRALARGE

/*
 ***********************************************************************
 *				Globals
 ***********************************************************************
 */

extern const Xv_pkg	xv_frame_class_pkg;
extern const Xv_pkg	xv_frame_base_pkg;
extern const Xv_pkg	xv_frame_cmd_pkg;
extern const Xv_pkg	xv_frame_props_pkg;
extern const Xv_pkg	xv_frame_help_pkg;
extern const Xv_pkg xv_propframe_pkg;

/*
 * XView public functions
 * xv_window_loop/xv_window_return are defined in libxview/misc/xv_win_lp.c
 * The are declared here (and not pkg_public.h) as they require frame.h
 * to be included.
 */
_XVFUNCPROTOBEGIN
EXTERN_FUNCTION (Xv_opaque xv_window_loop, (Frame frame));
EXTERN_FUNCTION (void xv_window_return, (Xv_opaque ret));
EXTERN_FUNCTION (void frame_get_rect, (Frame frame, Rect *rect));
EXTERN_FUNCTION (void frame_set_rect, (Frame frame, Rect *rect));

/*
 * XView Private functions
 */
EXTERN_FUNCTION (void frame_cmdline_help, (char *name));
EXTERN_FUNCTION (void frame_grant_extend_to_edge, (Frame frame, int to_right));
EXTERN_FUNCTION (void frame_kbd_use, (Frame frame, Xv_Window sw, Xv_Window pw));
EXTERN_FUNCTION (void frame_kbd_done, (Frame frame, Xv_Window sw));

/* Extensions by DRA */
typedef void (*xv_popup_frame_initializer_t)(Frame newpopup, Frame parent);
EXTERN_FUNCTION (xv_popup_frame_initializer_t xv_set_popup_frame_initializer, (xv_popup_frame_initializer_t));
EXTERN_FUNCTION (void xv_ol_default_background, (Xv_opaque panel, Event *ev));
typedef void (*xv_soon_proc_t)(Xv_opaque);
EXTERN_FUNCTION (void xv_perform_soon, (Xv_opaque cldt, xv_soon_proc_t func));
EXTERN_FUNCTION (void xv_perform_later, (Xv_opaque cldt, xv_soon_proc_t func, int usec));

typedef void (*xv_frame_layout_cb_t)(Frame, void *);
typedef void (*xv_frame_help_proc_t)(Frame, Event *);

extern Attr_attribute xv_get_rwid_key(void);
extern void xv_set_frame_resizing(Frame_cmd frame, int resize_width,
										xv_frame_layout_cb_t cb, void *cldt);
extern void xv_activate_resizing(void);

/* set this on panel items that are to be resized horizontally */
#define FRAME_ITEM_RESIZE_WIDTH         XV_KEY_DATA,xv_get_rwid_key(),TRUE

EXTERN_FUNCTION (int frame_props_cbkey, (void));
EXTERN_FUNCTION (int frame_props_cbidkey, (void));
_XVFUNCPROTOEND

#define FRAME_PROPS_CHANGEBAR XV_KEY_DATA,frame_props_cbkey()
#define FRAME_PROPS_IS_CHANGEBAR XV_KEY_DATA,frame_props_cbidkey(),PANEL_MESSAGE

#define FP_OFF(_ptr_type_,_field_) \
	((int) (((char *) (&(((_ptr_type_)NULL)->_field_))) - ((char *) NULL)))


#endif /* xview_frame_DEFINED */
