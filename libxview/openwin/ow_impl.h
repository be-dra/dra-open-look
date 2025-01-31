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

#ifndef	__openwin_impl_h
#define	__openwin_impl_h

/* $Id: ow_impl.h,v 4.1 2024/03/28 18:20:25 dra Exp $ */
/*
 * Package:     openwin
 *
 * Module:	ow_impl.h
 *
 * Description: defines internal data structures for managing openlook windows
 *
 */

#include <xview/xview.h>
#include <xview/openwin.h>
#include <xview/scrollbar.h>
#include <xview/sel_pkg.h>
#include <xview/rect.h>
#include <xview/rectlist.h>
#include <olgx/olgx.h>

/*
 * Global Defines:
 */

/* macros to convert variable from/to public/private form */
#define OPENWIN_PRIVATE(win)  \
	XV_PRIVATE(Xv_openwin_info, Xv_openwin, win)
#define OPENWIN_PUBLIC(win)   XV_PUBLIC(win)

#define VIEW_PRIVATE(win) XV_PRIVATE(Openwin_view_info, Xv_openwin_view, win)
#define VIEW_PUBLIC(_vp_) XV_PUBLIC(_vp_)

#define OPENWIN_REGULAR_VIEW_MARGIN	4
#define OPENWIN_VIEW_BORDER_WIDTH 2

#define OPENWIN_SPLIT_VERTICAL_MINIMUM  50
#define OPENWIN_SPLIT_HORIZONTAL_MINIMUM  50

#define OPENWIN_SCROLLBAR_LEFT 0
#define OPENWIN_SCROLLBAR_RIGHT 1

#define openwin_sb(view, direction)    \
   ((view)->sb[((direction) == SCROLLBAR_VERTICAL) ? 0 : 1])

#define openwin_set_sb(view, direction, sb) \
   openwin_sb((view), (direction)) = sb


/*
 * Typedefs:
 */

typedef struct	openwin_view_struct		Openwin_view_info;
typedef struct	openwin_info_struct		Xv_openwin_info;

struct openwin_view_struct {
	Openwin_view	public_self;
	Scrollbar	sb[2]; /* 0 -> vertical 1 -> horizontal */
	Rect		enclosing_rect; /* full area the view takes up --
	               includes margins, borders and scrollbars */

	int			right_edge; /* view against openwin's right edge */
	int			bottom_edge; /* view against bottom edge */
	Openwin_view_info	*next_view;
    Xv_openwin_info       *owin;

	Xv_window pw;
};

#define STATUS(ow, field)           ((ow)->status_bits.field)
#define STATUS_SET(ow, field)       STATUS(ow, field) = TRUE
#define STATUS_RESET(ow, field)     STATUS(ow, field) = FALSE
#define BOOLEAN_FIELD(field)        unsigned field : 1

typedef int (*resize_verification_t)(Openwin ow, Openwin_resize_side side,
                                            int frame_pos, int is_up_event);

struct openwin_info_struct {
   	Openwin		public_self;		/* Back pointer */

	Openwin_view_info	*views;
	int		margin;
	Rect		cached_rect;
	Scrollbar	vsb_on_create;	/* cached scrollbar until view is */
	Scrollbar	hsb_on_create;	/* created */
	Attr_avlist	view_attrs; 	/* cached view avlist on create */
	Attr_avlist	view_end_attrs;
	Attr_avlist	pw_attrs;
	Attr_avlist	pw_end_attrs;
	Xv_window last_focus_pw;
	struct {
	    BOOLEAN_FIELD(auto_clear);
	    BOOLEAN_FIELD(adjust_vertical);
	    BOOLEAN_FIELD(adjust_horizontal);
	    BOOLEAN_FIELD(no_margin);
	    BOOLEAN_FIELD(created);
	    BOOLEAN_FIELD(show_borders);
	    BOOLEAN_FIELD(removing_scrollbars);
	    BOOLEAN_FIELD(mapped);
	    BOOLEAN_FIELD(left_scrollbars);
#ifndef NO_OPENWIN_PAINT_BG
	    BOOLEAN_FIELD(paint_bg);
#endif /* NO_OPENWIN_PAINT_BG */
	    BOOLEAN_FIELD(selectable);
	} status_bits;
	int		nbr_cols;		/* WIN_COLUMNS specified by client */
	int		nbr_rows;		/* WIN_ROWS specified by client */
	window_layout_proc_t	layout_proc;
	void		(*split_init_proc)(Xv_window, Xv_window, int);
	void		(*split_destroy_proc)(Xv_window);
	resize_verification_t       resize_verify_proc;
	Openwin_resize_side cur_resize;
	int frame_trans, last_frame_pos;
	Selection_owner sel_owner;
	Openwin_view_info	*selected_view;	/* selected view, if any */
	struct timeval seltime;
	unsigned resize_sides;
	Xv_window resize_handles[(int)OPENWIN_RIGHT];
	Graphics_info *ginfo;
	GC resize_gc;
	GC border_gc;
	Xv_opaque target_ptr;
#ifndef NO_OPENWIN_PAINT_BG
	XColor		background;
#endif /* NO_OPENWIN_PAINT_BG */
};

/*
 * Package private function declarations:
 */

/* ow_get.c */
Pkg_private Xv_opaque openwin_get(Openwin owin_public, int *get_status, Attr_attribute attr, va_list valist);

/* ow_set.c */
Pkg_private Xv_opaque openwin_set(Openwin owin_public, Attr_avlist avlist);

/* ow_evt.c */
Pkg_private Notify_value openwin_event(Openwin owin_public, Notify_event ev,
    Notify_arg arg, Notify_event_type type);

/* ow_resize.c */
Pkg_private void openwin_adjust_views(Xv_openwin_info *owin, Rect *owin_rect);
Pkg_private void openwin_adjust_view(Xv_openwin_info *owin,
    Openwin_view_info *view, Rect *view_rect);
Pkg_private void openwin_place_scrollbar(Xv_object owin_public, Xv_opaque view_public, Scrollbar sb, Scrollbar_setting direction, Rect *r, Rect *sb_r);

/* ow_paint.c */

Pkg_private void openwin_select_view(Openwin self, Openwin_view_info *vp);

Pkg_private void openwin_lose_selection(Selection_owner owner);


/* openwin_view.c */
Pkg_private void openwin_create_initial_view(Xv_openwin_info *owin);
Pkg_private void openwin_destroy_views(Xv_openwin_info *owin);
Pkg_private int openwin_count_views(Xv_openwin_info *owin);
Pkg_private Openwin_view_info *openwin_nth_view(Xv_openwin_info *owin, int place);
Pkg_private int openwin_viewdata_for_view(Xv_Window window, Openwin_view_info **view);
Pkg_private void openwin_split_view(Xv_openwin_info *owin, Openwin_view_info *view, Openwin_split_direction direction, int pos, int view_start);
Pkg_private int openwin_fill_view_gap(Xv_openwin_info *owin, Openwin_view_info *view);
Pkg_private void openwin_copy_scrollbar(Xv_openwin_info *owin, Scrollbar sb, Openwin_view_info *to_view);
Pkg_private void openwin_remove_split(Xv_openwin_info *owin, Openwin_view_info *view);
Pkg_private int openwin_viewdata_for_sb(Xv_openwin_info *owin, Scrollbar sb, Openwin_view_info **view, Scrollbar_setting *sb_direction, int *last_sb);

Pkg_private void openwin_rescale(Openwin owin_public, int scale);

Pkg_private void openwin_set_bg_color(Openwin owin_public);
Pkg_private int openwin_border_width(Openwin owin_public, Xv_opaque view_public);

#endif	 /* __openwin_impl_h */
