#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)fm_impl.h 20.64 93/06/28 DRA: $Id: fm_impl.h,v 4.9 2025/07/23 16:48:22 dra Exp $ ";
#endif
#endif

/***********************************************************************/
/*	                 frame_impl.h/fm_impl.h	       		       */
/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL_NOTICE
 *	file for terms of the license.
 */
/***********************************************************************/

#ifndef _frame_impl_h_already_included
#define _frame_impl_h_already_included

/* standard includes */
#ifndef FILE
#if !defined(SVR4) && !defined(__linux)
#undef NULL
#endif  /* SVR4 */
#include <stdio.h>
#endif  /* FILE */
#include <sys/time.h>
#ifdef OW_I18N
#endif /* OW_I18N */
#include <xview/xv_i18n.h>
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
#include <xview/frame.h>
#include <xview/icon.h>
#include <xview/openmenu.h>
#include <xview/cms.h>
#include <xview_private/i18n_impl.h>
#include <xview_private/portable.h>
#include <olgx/olgx.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#ifdef OW_I18N
#include <X11/Xresource.h>
#endif
#define	FRAME_CLASS_PRIVATE(f)	XV_PRIVATE(Frame_class_info, Xv_frame_class, f)
#define	FRAME_CLASS_PUBLIC(frame)	XV_PUBLIC(frame)
#define FRAME_PUBLIC(frame)	FRAME_CLASS_PUBLIC(frame)
#define FRAME_PRIVATE(f)	FRAME_CLASS_PRIVATE(f)

#define	BIT_FIELD(field)	unsigned field : 1

/* ACC_XVIEW */
typedef struct frame_menu_accelerator {
#ifdef OW_I18N
    _xv_string_attr_dup_t	keystr;
#else
    char	   		*keystr; /* e.g. "Shift+Ctrl+m" */
#endif /* OW_I18N */
    short	    code;   /* = event->ie_code */
    KeySym	    keysym; /* X keysym */
    frame_accel_notify_func	notify_proc; /* accelerator notify proc */
    Xv_opaque	    data;   /* opaque data handle */
    unsigned int    modifiers;  /* state of modifiers */
    struct frame_menu_accelerator *next;
} Frame_menu_accelerator;

typedef struct _frame_a_d{
        Xv_opaque	menu;
        Xv_opaque	item;
}Frame_accel_data;
/* ACC_XVIEW */


typedef	struct	{
    Frame	 public_self;	  /* back pointer to object */
#ifndef OW_I18N
    char	*label;		  /* string in name stripe & default icon */
#endif
    Icon 	 icon;
    Icon 	 default_icon;    /* Default Icon */
    Frame	 first_subframe;  /* first subframe in list of subframes */
    Xv_Window	 first_subwindow; /* first subwindow in list of subwindows */
    Rect	 rectcache; 	  /* rect of frame (tool relative) */
    Rect 	 oldrect;  	  /* quick check if size has changed */
    Rect         user_rect;       /* user position and size */
    frame_done_proc_t done_proc;
    frame_done_proc_t default_done_proc;
    Xv_Window	 focus_subwindow; /* last subwindow that had the input focus */
    Xv_Window	 primary_focus_sw;/* primary focus subwindow */
    XWMHints	 wmhints;	  /* Window manager hints structure */
    XSizeHints   normal_hints;	  /* WM_SIZE_HINTS */
    int		 geometry_flags;  /* Geometry information */
    XColor	 bg;		  /* background color */
    XColor	 fg;		  /* foreground color */
    Xv_Window	 focus_window;	  /* Location Cursor window */
    Xv_Window	 footer;          /* Win that is used to implement the footer */
#ifndef OW_I18N
    char	*left_footer;	  /* string in left footer */
    char	*right_footer;	  /* string in right footer */
#endif
    Graphics_info	*ginfo;	  /* OLGX structure used to paint footers */
    Frame_accelerator	*accelerators;	/* Window Level Accelerator list */
    /* ACC_XVIEW */
    Frame_menu_accelerator	*menu_accelerators;	/* Menu Accelerator list */
    Menu	*menu_list;	/* Menu list for acceleration*/
    int		menu_count;	/* Count of menus in list */
    int		max_menu_count;	/* Max count of menus in list */
    /* ACC_XVIEW */
#ifdef OW_I18N
    Xv_Window	 imstatus;	  /* Win that is used to implement IMstatus */
    _xv_string_attr_dup_t
		 label;           /* string in name stripe & default icon */
    _xv_string_attr_dup_t
		 left_footer;     /* left footer */
    _xv_string_attr_dup_t
		 right_footer;    /* right footer */
    _xv_string_attr_dup_t
		 left_IMstatus;   /* IM Status region */
    _xv_string_attr_dup_t
		 right_IMstatus;  /* IM Status region */
#endif

	/* footer text coordinates, set in frame_display_footer */
	int left_x, right_x;
	frame_footer_proc_t	footer_proc;

    struct {
	BIT_FIELD(bndrymgr);		/* movable borders between subwindows */
	BIT_FIELD(full);		/* current state is full */
	BIT_FIELD(no_confirm);		/* don't confirm on destroy */
	BIT_FIELD(initial_state);	/* icon initial state (set = iconic) */
	/* Note that iconic is used only to detect a change in state.
	 * frame_is_iconic() is the function to call to determine
	 * if the frame is iconic or not.
	 */
	BIT_FIELD(busy);		/* whether frame is busy or not     */
	BIT_FIELD(iconic);		/* whether frame is iconic or not   */
	BIT_FIELD(dismiss);		/* whether frame is being dismissed */
					/* or not.			    */
	BIT_FIELD(map_state_change);	/* whether we (un)mapped the frame
					 * and should expect an (Un)mapNotify.*/
	BIT_FIELD(created);		/* whether frame has been created */
	BIT_FIELD(frame_color);		/* whether frame has been created */
	BIT_FIELD(show_footer);		/* whether the footer is visable */
	BIT_FIELD(compose_led);		/* whether the compose LED is on/off */
	BIT_FIELD(accept_default_focus);/* whether frame will take focus if none
					   of it's subwindows wants it */
#ifdef OW_I18N
	BIT_FIELD(show_imstatus);	/* whether the IMstatus is visable */
	BIT_FIELD(inactive_imstatus);	/* whether the IMstatus is active */
#endif
    } status_bits;
} Frame_class_info;

#define	status_get(frame, field)	((frame)->status_bits.field)
#define	status_set(frame, field, value)	\
	(frame)->status_bits.field = ((value) != FALSE)



#define	FRAME_EACH_CHILD(first, child)	\
    { Xv_Window	_next; \
    for ((child) = (first), \
	 (child) && \
	 (_next = xv_get((child), XV_KEY_DATA, FRAME_NEXT_CHILD)); \
	 (child); (child) = _next, \
	 (child) && \
	 (_next = xv_get((child), XV_KEY_DATA, FRAME_NEXT_CHILD))) {

#define	FRAME_EACH_SUBWINDOW(frame, child)	\
    FRAME_EACH_CHILD((frame)->first_subwindow, child)

#define	FRAME_EACH_SUBFRAME(frame, child)	\
    FRAME_EACH_CHILD((frame)->first_subframe, child)

#define	FRAME_EACH_SHOWN_SUBWINDOW(frame, child)	\
    FRAME_EACH_SUBWINDOW(frame, child)	\
	if (!xv_get(child, XV_SHOW)) \
	    continue;

#define	FRAME_END_EACH	}}

#define	EXTEND_HEIGHT(child)	\
    ((int) xv_get((child), WIN_DESIRED_HEIGHT) == WIN_EXTEND_TO_EDGE)

#define	EXTEND_WIDTH(child)	\
    ((int) xv_get((child), WIN_DESIRED_WIDTH) == WIN_EXTEND_TO_EDGE)


#define	frame_getnormalrect(frame, rectp) \
	    win_getrect(FRAME_PUBLIC(frame), (rectp))
#define	frame_setnormalrect(frame, rectp) \
	    win_setrect(FRAME_PUBLIC(frame), (rectp))

#define frame_is_iconic(frame)  (status_get(frame, iconic))


#define	FRAME_ICON_WIDTH	64
#define	FRAME_ICON_HEIGHT	64

#ifdef OW_I18N
#define LEFT_IMSTATUS           0
#define RIGHT_IMSTATUS          1
#endif

/* frame.c */
Pkg_private int		frame_notify_count;
Pkg_private void	frame_default_done_func(Frame);

/* frame_get.c */
Pkg_private Xv_opaque frame_get_attr(Frame, int *, Attr_attribute, va_list);

/* frame_set.c */
Pkg_private Xv_opaque frame_set_avlist(Frame, Attr_attribute *);

/* frame_layout.c */
Pkg_private int	frame_layout(Xv_Window frame_public, Xv_Window child, Window_layout_op op, Xv_opaque d1, Xv_opaque d2, Xv_opaque d3, Xv_opaque d4, Xv_opaque d5);
Pkg_private Xv_Window frame_last_child(Xv_Window first);

/* frame_destroy.c */
Pkg_private int frame_destroy(Frame frame_public, Destroy_status status);

/* frame_win.c */
Pkg_private int frame_is_exposed(Frame frame);

/* frame_input.c */
Pkg_private Notify_value frame_input(Frame, Notify_event, Notify_arg, Notify_event_type);
Pkg_private Notify_value frame_footer_input(Xv_Window footer, Event *event, Notify_arg arg, Notify_event_type type);
#ifdef OW_I18N
Pkg_private Notify_value frame_IMstatus_input(void);
#endif
Pkg_private void frame_focus_win_event_proc(Xv_Window window, Event *event, Notify_arg arg);

/* frame_display.c */
Pkg_private void frame_display_label(Frame_class_info *frame);
Pkg_private void frame_display_footer(Frame frame_public, int clear_first,
			int *left_start, int *left_end, int *right_start, int *right_end);
#ifdef OW_I18N
Pkg_private void	frame_display_IMstatus(Frame, int);
#endif
Pkg_private void frame_set_color(Frame_class_info *frame, XColor *fg, XColor *bg);

/* fm_geom.c */
Pkg_private int frame_height_from_lines(int n, int show_label);
Pkg_private int frame_width_from_columns(int x);
Pkg_private void frame_position_sw(Frame_class_info *frame, Xv_Window nsw, Xv_Window swprevious, int width, int height, Rect *rect);
Pkg_private int frame_footer_height(Frame_rescale_state scale);
#ifdef OW_I18N
Pkg_private int frame_IMstatus_height(Frame_rescale_state scale);
#endif
Pkg_private int frame_footer_baseline(Frame_rescale_state scale);
Pkg_private int frame_footer_margin(Frame_rescale_state scale);
Pkg_private int frame_inter_footer_gap(Frame_rescale_state scale);

/* frame_cmdline.c */
Pkg_private int		frame_set_cmdline_options(Frame, int);
Pkg_private int frame_set_icon_cmdline_options(Frame frame_public);

/* frame_rescale.c */
Pkg_private void frame_rescale_subwindows(Frame frame_public, int scale);

/* frame_sw.c */
Pkg_private void frame_layout_subwindows(Frame frame_public);

Pkg_private void frame_update_compose_led(Frame_class_info *frame, int state);
Pkg_private int frame_gravity_from_flags(int flags);
Pkg_private Xv_window frame_create_footer(Frame_class_info *frame);
Pkg_private Frame_menu_accelerator *frame_find_menu_acc(Frame frame_public, int keycode, unsigned int	state, KeySym keysym, int remove);
Pkg_private void frame_display_busy(Frame_class_info *frame, int status);
Pkg_private void frame_update_status_win_color(Frame frame_public, Xv_Window	status_window, Cms new_frame_cms, unsigned long	new_frame_fg, int new_frame_fg_set, int *repaint_needed);

Pkg_private void frame_set_position(unsigned long parent, Frame_class_info *frame);
Pkg_private int frame_all_set_cmdline_options(Frame frame_public);
Pkg_private void frame_compute_constraint(Frame_class_info *frame, Xv_Window target_sw, register Rect *rconstrain);
Xv_private	void frame_set_accept_default_focus(Frame frame_public, int flag);

#endif
