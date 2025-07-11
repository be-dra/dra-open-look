/*	@(#)om_impl.h 20.67 93/06/28	DRA: $Id: om_impl.h,v 4.4 2025/07/05 12:42:45 dra Exp $	*/

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL_NOTICE
 *	file for terms of the license.
 */

#ifndef _xview_walkmenu_impl_h_already_included
#define _xview_walkmenu_impl_h_already_included

#include <xview/openmenu.h>
#include <xview/rect.h>
#include <xview/frame.h>
#include <xview/server.h>
#include <xview/svrimage.h>
#include <xview/win_input.h>
#include <xview_private/omi_impl.h>
#include <olgx/olgx.h>
#include <X11/Xutil.h>

#ifdef OW_I18N
#include <X11/Xlib.h>
#include <X11/Xresource.h>
#endif  /* OW_I18N */

/***** Definitions *****/
#ifndef TRUE
#define TRUE	1
#endif

#ifndef FALSE
#define FALSE	0
#endif

#ifndef NULL
#define NULL	0
#endif

#define imax(a, b) ((int)(b) > (int)(a) ? (int)(b) : (int)(a))
#define range(v, min, max) ((v) >= (min) && (v) <= (max))

#define MENU_FILLER 5

#define MENU_PRIVATE(menu)  XV_PRIVATE(Xv_menu_info,Xv_menu,menu)
#define MENU_PUBLIC(menu)   XV_PUBLIC(menu)

#define MENU_ITEM_PRIVATE(menu_item) \
	XV_PRIVATE(Xv_menu_item_info, Xv_menu_item, menu_item)
#define MENU_ITEM_PUBLIC(menu_item)  XV_PUBLIC(menu_item)

/* ACC_XVIEW */
#define MENU_MAX_FRAMES 10
/* ACC_XVIEW */


/***** Procedures *****/
Pkg_private Xv_opaque menu_sets(Menu menu_public, Attr_attribute *attrs);
Pkg_private Xv_opaque menu_item_sets(Menu_item menu_item_public, Attr_attribute *attrs);
Pkg_private Xv_opaque menu_gets(Menu menu_public, int *status, Attr_attribute attr, va_list args);
Pkg_private Xv_opaque menu_item_gets(Menu_item menu_item_public, int *status, Attr_attribute attr, va_list args);


/***** Enumerations *****/
typedef enum {
    MENU_STATUS_PIN = -2,
    MENU_STATUS_ABORT = -1,
    MENU_STATUS_DONE = 0,
    MENU_STATUS_PARENT = 1,
    MENU_STATUS_IGNORE = 2,
    MENU_STATUS_DEFAULT = 3,
    MENU_STATUS_BUSY = 4
} Menu_status;

typedef enum {
    MENU_STATE_INIT,	/* generate and initialize menu */
    MENU_STATE_GET_MODE, /* Press-Drag-Release or Click-Move-Click mode? */
    MENU_STATE_TRACK_MOUSE /* track the mouse: process Button Press and Drag */
} Menu_state;




/***********************************************************************/
/*	        	Structures 				       */
/***********************************************************************/

/* Terminology:
 *
 * Menu Group:  a collection of menus.  The first menu shown is referred
 *		to as the top or base menu.  Subsequent menus are referred
 *		to as submenus or pullrights.
 *
 * Menu:	a pair of windows (menu and shadow).  The menu window displays
 *		a list of menu items.
 *
 * Menu item:	one of the entires in a menu.  It's either a command button,
 *		choice rectangle or toggle rectangle.
 */


typedef Xv_opaque (*Menu_notify_proc_t)(Menu, Menu_item);

/***********	Menu Group	**********
 *
 * Menu_group_info data is maintained across the entire menu group.
 * Each menu in the menu group points to the same Menu_group_info
 * structure, which is a static variable in om_public.c
 */
typedef struct menu_group {
    Xv_Window		client_window;
    int			color_index;	/* default foreground color index
					 * for menu items (-1= xv_fg(info) */
    XVisualInfo		*vinfo;		/* Visual information used to create the window */
    int			depth;		/* Level of current menu */
    struct inputevent	first_event;
    struct inputevent	last_event;
    struct inputevent	menu_down_event;  /* action==0 => no MENU-down event */
    Menu_notify_proc_t notify_proc;
    struct inputevent	previous_event;
    struct menu		*pinned_menu;
    struct menu 	*selected_menu;
    Xv_Server		server;
    int			setting_default;
    int			three_d;	/* TRUE: 3D, FALSE: 2D */
} Xv_menu_group_info;

typedef Menu (*Menu_gen_proc_t) (Menu, Menu_generate);

/***********	Menu	**********/
typedef struct menu {
    Menu	public_self;	/* public handle */

    int		color_index;	/* default foreground color index
				 * for menu items (-1= xv_fg(info) */
    int		curitem;		/* current item */
    int		default_position;	/* Default item */
    int		drag_right_distance;  /* # pixels continuously dragged right */
    int		nitems;			/* Number of items */
    int		notify_status;	/* notify proc status: XV_OK or XV_ERROR */
    int		max_nitems;		/* Size of item list */
    int		ncols;
    int		nrows;
    int		pending_default_position;  /* selected, but not accepted yet */
    int		pullright_delta;  /* # pixels to drag right to show submenu */
    int     	pushpin_left, pushpin_top;  /* relative to menu window */
    int		read_inputevent;
    int		selected_position;	/* Last selected item */
    int		type;		/* MENU_MENU: use to verify menu handle */
    /* ACC_XVIEW */
    int		menu_mark;
    /* ACC_XVIEW */
    struct image	default_image;	/* Font, max size, etc. for label */
    /* ACC_XVIEW */
    struct image	default_qual_image;  /* Font, max size, etc for qualifier */
    struct image	default_key_image;  /* Font, max size, etc for accelerator*/
    /* ACC_XVIEW */
    struct menu_item	**item_list;
    struct menu_item	*parent;	/* Last pullright item */
    struct pr_pos	pin_window_pos;	/* coordinate of top left corner
					 * of pinned window */
    /* An enable rectangle is used to both describe where the menu
     * is positioned, and the rectangle in which the pointer has
     * to remain, when outside the menu, in order for the menu to stay up.
     * A position rect is used only to describe where the menu is
     * positioned.
     * If an enable rect is not defined, the menu stays up no matter
     * where the pointer is positioned.  Such a menu is called a popup menu.
     */
    XVisualInfo	vinfo_template; /* Describes what kind of visual menu wants */
    long	vinfo_mask;	/* tells what fields of the template have been filled in */
    Graphics_info *ginfo;	/* Graphics info struct used by libolgx */
    Menu_gen_proc_t	gen_proc;  /* Dynamically generate menu */
    Menu_class	class;		/* command, choice or toggle OR MIXED !!!!!  */
    Menu_state	state;
    Menu_status	status;
    Rect	enable_rect;	/* defines menu position and stay-up area */
    Rect	fs_menurect;	/* in screen coordinates */
    Rect	menurect;		/* relative to input event window */
    Rect	pin_window_rect; /* ... from frame_get_rect() */
    Rect	position_rect;	/* defines menu position only.
				 * width==0 && pulldown => flush left menu
				 * instead of centered */
    Rect    	pushpin_rect;	/* coordinates relative to input event window */
    void (*busy_proc)(Menu);/* called when stay-up mode is first selected */
    void (*done_proc)(Menu, Xv_opaque);/* called when menu group is dismissed */
    void (*pin_proc)(Menu, int, int);/* Dynamically generate pinned window */
    Xv_menu_group_info	*group_info;
    Menu_notify_proc_t notify_proc;	/* Handler for item select;
					 * calls menu item proc */
    Xv_opaque	glyph_font;
    Xv_opaque	pin_parent_frame;
    Xv_Window	pin_window;		/* window shown when menu is pinned */
#ifdef OW_I18N
    _xv_string_attr_nodup_t
		pin_window_header;
#else
    char	*pin_window_header;
#endif  /* OW_I18N */
    Xv_Window	window;			/* window containing menu items */
    Xv_Window	shadow_window;		/* gray shadow window */
#ifdef OW_I18N
    Xv_Window   client_window;		/* copy of client window (for ic) */
#endif
    /* ACC_XVIEW */
    Frame	*frame_list;		/* for menu accelerators */
    int		nframes;		/* number of frames menu will be on */
    int		max_frames;		/* max number of frames the current frame_list
					 * list can take
					 */
    /* ACC_XVIEW */


    /* Flags */
    unsigned	active:1;	/* Menu is part of the current menu group.
				 * (It is being displayed on the screen.) */
    unsigned	column_major:1;     /* Layout items in col major */
    unsigned	gen_items:1;		/* menu has generated menu items */
    unsigned	stay_up:1;
    unsigned	h_line:1;	/* Draw horizontal line after item */
/*  BR# 1092662 */
    unsigned	ncols_fixed;	/* MENU_NCOLS fixed by user */
    unsigned	nrows_fixed;	/* MENU_NROWS fixed by user */
/*  BR# 1092662 */
    unsigned	pin:1;		/* Menu contains a pushpin */
    unsigned	popup:1;	/* TRUE: popup, FALSE: pulldown or pullright */
    unsigned	pulldown:1;	/* TRUE: pulldown, FALSE: pullright */
    unsigned	rendered:1;	/* Menu has been painted */
    unsigned	select_is_menu:1; /* SELECT button == MENU button */
    unsigned	valid_result:1; /* True if m->value is valid */
    unsigned	v_line:1;	/* Draw vertical line after item */
#ifdef OW_I18N
    unsigned    ic_was_active:1;
#endif

    /*
     * SunView1 compatibility entries
     */
    Xv_opaque	client_data;
    void (*extra_destroy_proc)(struct menu *, Attr_attribute);	/* an extra destroy proc that
						 * is used when client calls
						 * menu_destroy_with_proc() */
	/* BEGIN MENU_MIXED */
	/* bisher brauche ich nichts */
}  Xv_menu_info;

typedef Menu (*Menu_item_gen_proc_t) (Menu_item, Menu_generate);
typedef void (*Menu_item_preview_proc_t) (Menu_item it, unsigned startpreview);

/*********** 	Menu item 	**********/
typedef struct menu_item {
    Menu_item	public_self;	    /* Back pointer */

    int		color_index;  /* foreground color index (-1= m->color_index) */
    Menu_item_gen_proc_t gen_pullright;/* Called before displaying menu */
    Menu_item_gen_proc_t gen_proc;	    /* Called before displaying item */
    struct image image;
    /* ACC_XVIEW */
    struct image qual_image;
    struct image key_image;
    /* ACC_XVIEW */
    struct menu	*parent;	    /* Current enclosing menu */
    void	(*extra_destroy_proc)(Menu_item, Attr_attribute); /* an extra destroy proc that
					  * is used when client calls
    					  * menu_item_destroy_with_proc() */
    Menu_notify_proc_t notify_proc;   /* Called only when selected */
    Xv_opaque	panel_item_handle;  /* ... for pinned window */
    Xv_opaque	value;		    /* union: value, menu_ptr */
    /* ACC_XVIEW */
#ifdef OW_I18N
    _xv_string_attr_dup_t	menu_acc;
#else
    char	*menu_acc;
#endif /* OW_I18N */
    unsigned int	mark_type;
    /* ACC_XVIEW */

    /* SunView1 compatibility */
    Xv_opaque	client_data;

    /* Flags  */
    unsigned	inactive:1;
    unsigned	no_feedback:1;
    unsigned	pullright:1;
    unsigned	selected:1;
    unsigned	title:1;	     /* This item is the menu's title */
    unsigned	free_item:1;
    unsigned	free_value:1;        /* Not used */
    unsigned	free_client_data:1;  /* Not used */
    unsigned	h_line:1;	     /* Draw horizontal line after item */
    unsigned	v_line:1;	     /* Draw vertical line after item */
    unsigned	toggle_on:1;
    unsigned	toggle_feedback_on:1;
    unsigned	is_invoked:1;  /* used for preview_proc */

	Menu_item_preview_proc_t preview_proc;

	/* BEGIN MENU_MIXED */
    Menu_class	class;		/* command, choice or toggle  in a mixed menu */
	/* item_rect: der ganze Platz des Items
	 * paint_rect: was 'bemalt' wird - nur bei MENU_TOGGLE anders als item_rect
	 */
	Rect item_rect, paint_rect; /* die werden belegt in.... */
	int         choice_group; /* relevant only for class MENU_CHOICE:
							   * The index of the first item of this group.
							   * currently only one group per menu...
							   */
}  Xv_menu_item_info;



#define MIXED() fprintf(stderr, "%s`%s-%d: MENU_MIXED unhandled\n", __FILE__,__FUNCTION__,__LINE__); abort()

Pkg_private void menu_render(Xv_menu_info *menu, Xv_menu_group_info *group,
    Xv_menu_item_info *parent);
Pkg_private int menu_image_compute_size(Xv_menu_info *m, struct image *im,
						struct image *std_image);
Xv_private void menu_set_acc_on_frame(Frame frame, Menu menu, Menu_item	item, int set);
Pkg_private Notify_value menu_client_window_event_proc(
    Xv_Window       win,	/* menu client window */
    Notify_event ev, Notify_arg      arg, Notify_event_type type);
Pkg_private void menu_done(Xv_menu_info *m);
Pkg_private void menu_window_event_proc(Xv_Window window, Event *event);
Pkg_private void menu_destroys(Xv_menu_info *m, void (*destroy_proc) (Xv_menu_info *, Attr_attribute));
Pkg_private void menu_item_destroys(Xv_menu_item_info *mi, void (*destroy_proc) (Menu_item, Attr_attribute));
Pkg_private Xv_opaque menu_pullright_return_result(Menu_item menu_item_public);
Pkg_private void menu_return_no_value(Menu menu_public);

Pkg_private void menu_create_pin_panel_items(Xv_opaque panel, Xv_menu_info *menu);
Pkg_private void menu_set_pin_window(Xv_menu_info *m, Xv_opaque pin_window);
Xv_private void menu_save_pin_window_rect( Xv_Window win);
Pkg_private Notify_value menu_pin_window_event_proc(Xv_Window win, Notify_event ev, Notify_arg arg, Notify_event_type type);

struct _frame_a_d;
Xv_private void menu_accelerator_notify_proc(struct _frame_a_d *accelerator_data, Event *event);
Pkg_private int menu_compute_max_item_size(Xv_menu_info *menu, struct image *std_image,
    int	*status,	/* output parameter */
    int	call_gen_proc, int *height_sum);

#endif  /* _xview_walkmenu_impl_h_already_included */
