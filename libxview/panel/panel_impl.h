/*	@(#)panel_impl.h 20.90 93/06/28 SMI  DRA: $Id: panel_impl.h,v 4.14 2025/03/08 13:08:26 dra Exp $	*/

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL_NOTICE
 *	file for terms of the license.
 */

#ifndef panel_impl_defined
#define panel_impl_defined

#ifndef FILE
#if !defined(SVR4) && !defined(__linux)
#undef NULL
#endif  /* SVR4 */
#include <stdio.h>
#endif  /* FILE */
#include <sys/types.h>
#include <X11/Xlib.h>
#include <xview_private/i18n_impl.h>
#include <xview_private/portable.h>
#include <olgx/olgx.h>
#ifdef OW_I18N
#include <xview/xv_i18n.h>
#endif /* OW_I18N */
#include <xview/pkg.h>
#include <xview/attrol.h>
#include <xview/font.h>
#include <xview/frame.h>
#include <xview/openwin.h>
#include <xview/panel.h>
#include <xview/sel_pkg.h>
#include <xview/svrimage.h>
#include <xview/dragdrop.h>
#include <xview/rectlist.h>
#include <xview/scrollbar.h>
#include <xview_private/item_impl.h>

/* panels and panel_items are both of type Xv_panel_or_item so that we
 * can pass them to common routines.
 */
#define PANEL_PRIVATE(p)	XV_PRIVATE(Panel_info, Xv_panel, p)
#define PANEL_PUBLIC(panel)     XV_PUBLIC(panel)

#define rect_copy(to, from)		to.r_left = from.r_left; \
                                to.r_top = from.r_top;   \
                                to.r_width = from.r_width; \
                                to.r_height = from.r_height;

#define	set(value)	((value) != -1)


#define PANEL_MORE_TEXT_WIDTH 16
#define PANEL_MORE_TEXT_HEIGHT 14
#define PANEL_PULLDOWN_MENU_WIDTH 16
#define PANEL_PULLDOWN_MENU_HEIGHT 8

#define PANEL_SEL_PRIMARY	0
#define PANEL_SEL_SECONDARY	1
#define PANEL_SEL_CLIPBOARD	2
#define PANEL_SEL_DND	3
#define NBR_PANEL_SELECTIONS	4


/* 			structures                                      */


/***************************** panel **************************************/
/* *** NOTE: The first three fields of the panel_info struct must match those
 * of the item_info struct, since these are used interchangably in some
 * routines.
 */
typedef struct panel_info {
    /****  DO NOT CHANGE THE ORDER OR PLACEMENT OF THESE THREE FIELDS ****/
    Panel_ops		ops;		/* panel specific operations */
    unsigned int	flags;		/* default item flags */
	/*  N.B.:  panel->flags uses the "Item status flags" definitions
	 * found in "item_impl.h". */
    Panel		public_self;	/* back pointer to object */
    /**** ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ ****/

    int			active_caret_ascent;
    int			active_caret_height;
    int			active_caret_width;
    struct {
		Atom clipboard;
		Atom		delete;
		Atom		length;
		Atom		null;
		Atom		selection_end;
		Atom		seln_yield;
#ifdef OW_I18N
		Atom		compound_text;
		Atom		length_chars;
#endif /*OW_I18N*/
    } atom;
    Xv_Font		bold_font;
#ifdef OW_I18N
    XFontSet		bold_fontset_id;
#else
    Font		bold_font_xid;
#endif /* OW_I18N */
    int			caret;		/* current caret character index */
    int			caret_ascent;	/* # of pixels above baseline */
    Pixmap		caret_bg_pixmap; /* used to restore the pixels
					  * underneath the caret */
    int			caret_height;
    int			caret_on;	/* caret is painted */
    int			caret_width;
    Xv_opaque		client_data;	/* for client use */
    Item_info		*current;
    int			current_col_x;	/* left position of current column */
    Cursor		cursor;		/* panel's default (basic) cursor */
    Panel_item		default_item;
    Item_info		*default_drop_site_item;
    int			drag_threshold;
	/* # of pixels the mouse may move with a button down and still be
	 * considered a click.  If it moves more than this, it is considered
	 * to be a press-drag-release gesture instead of a click.  Drag-and-drop
	 * is initiated after the mouse is dragged 'drag_threshold' pixels.
	 * Default is 5.
	 */
    int			(*event_proc)(Xv_opaque, Event *);
    int			extra_height;
    int			extra_width;
    Xv_Window		focus_pw;
	/* Current or last input focus paint window.  (The
	 * panel->status.has_input_focus flag indicates whether
	 * the panel currently has the input focus.) Initially,
	 * focus_pw is set to the first paint window created.
	 */
    Graphics_info	*ginfo;		/* olgx graphics information */
    int			h_margin;	/* horizontal margin */
    int			inactive_caret_ascent;
    int			inactive_caret_height;
    int			inactive_caret_width;
    int			item_x;
    int			item_x_offset;
    int			item_y;
    int			item_y_offset;
    Item_info		*items;
    Item_info		*last_item;
    Item_info		*kbd_focus_item;/* panel item with the keyboard focus */
    Panel_setting	layout;		/* HORIZONTAL, VERTICAL */
    window_layout_proc_t layout_proc; /* interposed window layout proc */
    int			lowest_bottom;	/* lowest bottom of any item */
    int			max_item_y;	/* lowest item on row ??? */
    int			multiclick_timeout;  /* in msec */
    int			no_redisplay_item; /* Don't call panel_redisplay_item
					    * from item_set_generic */
    Panel_paint_window	*paint_window;
    Item_info		*primary_focus_item; /* current or last panel item
					      * that is/was a primary
					      * (First-Class) focus item */
    Panel_setting	repaint;	/* default repaint behavior  */
    int	(*repaint_proc)(Panel , Xv_Window, Rectlist *);
    int			rightmost_right; /* rightmost right of any item */
    Item_info		*sel_holder[NBR_PANEL_SELECTIONS];
#ifdef OW_I18N
    _xv_pswcs_t         clipboard;	  /* none sel_item version of clipb */
#else
    Selection_item	sel_item[NBR_PANEL_SELECTIONS];
#endif
    Selection_owner	sel_owner[NBR_PANEL_SELECTIONS];
    Atom		sel_rank[NBR_PANEL_SELECTIONS];
    Selection_requestor	sel_req;
    Panel_status	status;
    Xv_Font		std_font;	/* standard font */
#ifdef OW_I18N
    XFontSet		std_fontset_id;
#else
    Font		std_font_xid;
#endif /* OW_I18N */
    struct itimerval	timer_full;	/* initial secs & usecs */
    int			v_margin;	/* vertical margin */
#ifdef OW_I18N
    XIC                  ic;
    Item_info		 *preedit_item; /* panel item with the keyboard focus */
    XIMPreeditDrawCallbackStruct  *preedit; /*Save full preedit information*/
    Bool		 preedit_own_by_others;
		/*
		 * When panel text being used by canvas for preedit
		 * rendering, preedit data structure is owned by the
		 * canvas, not panel. So that, panel should not free
		 * the data structure upon destory (note, however,
		 * panel still create the preedit data structure once
		 * created, first canvas in the given frame will use
		 * that data structure, see cnvs_view.c,
		 * canvas_paint_set, XV_END_CREATE).
		 */
#ifdef FULL_R5
    XIMStyle		xim_style;
#endif /* FULL_R5 */
#endif /* OW_I18N */
    int			show_border;
    unsigned short	old_width;
    unsigned short	old_height;
    Graphics_info *revpin_ginfo; /* for 2d preview of default pushpins */
} Panel_info;


#define	PANEL_EACH_PAINT_WINDOW(panel, pw)	\
   {Panel_paint_window *_next; \
    for (_next = (panel)->paint_window; _next != NULL; _next = _next->next) { \
    	(pw) = _next->pw;

#define	PANEL_END_EACH_PAINT_WINDOW	}}

/***********************************************************************/
/* external declarations of private variables                          */
/***********************************************************************/

Pkg_private Attr_attribute	panel_context_key;
Pkg_private Attr_attribute	panel_pw_context_key;


/***********************************************************************/
/* Pkg_private declarations of private functions                          */
/***********************************************************************/

Pkg_private void panel_accept_kbd_focus(Panel_info *panel);
/* Pkg_private void              	panel_adjust_label_size(); unused */
Pkg_private void		panel_append(Item_info *ip);
Pkg_private void		panel_btn_accepted(Item_info *ip, Event *event);
Pkg_private void		panel_check_item_layout(Item_info *ip);
Pkg_private void          	panel_clear_item(Item_info *ip);
Pkg_private void		panel_clear_pw_rect(Xv_window pw, Rect rect);
Pkg_private void		panel_clear_rect(Panel_info *panel, Rect rect);
Pkg_private int		 	panel_col_to_x(Xv_Font font, int col);
Pkg_private void		panel_display(Panel_info *panel, Panel_setting flag);
Pkg_private Notify_value	panel_default_event(Panel p_public, Event *event,
                                            Notify_arg arg);
Pkg_private int			panel_duplicate_key_is_down(Panel_info *panel, Event *event);
Pkg_private Rect		panel_enclosing_rect(Rect *r1, Rect *r2);
Pkg_private int			panel_erase_action(Event *event);
Pkg_private void		panel_find_default_xy(Panel_info *panel, Item_info *item);
Pkg_private int 		panel_fonthome(Xv_Font font);
Pkg_private void		panel_free_image(Panel_image *);
Pkg_private Xv_opaque	 	panel_get_attr(Panel panel_public, int *status,
                            Attr_attribute attr, va_list valist);
Pkg_private int			panel_height(Panel_info *);
Pkg_private void panel_image_set_font(Panel_image *image, Xv_Font font);
Pkg_private void panel_invert( Panel_info *panel, Rect *r, int color_index);
Pkg_private void		panel_invert_polygon(Panel); /* unused */
Pkg_private int panel_is_multiclick(Panel_info *panel, struct timeval *last_click_time, struct timeval *new_click_time);
Pkg_private void panel_item_layout( Item_info *ip, Rect *deltas);
Pkg_private void panel_itimer_set( Panel panel_public, struct itimerval timer);
Pkg_private struct pr_size panel_make_image( Xv_Font font, Panel_image *dest, int , Xv_opaque , int , int );
Pkg_private int panel_navigation_action( Event	 *event);
Pkg_private Item_info * panel_next_kbd_focus( Panel_info *panel, int wrap);
Pkg_private void panel_normalize_scroll( Scrollbar sb, long offset, Scroll_motion motion, long *vs);
Pkg_private Notify_value panel_notify_event( Xv_Window paint_window, Event *event, Notify_arg arg, Notify_event_type type);
Pkg_private     Notify_value panel_notify_panel_event(Xv_Window window, Notify_event ev, Notify_arg arg, Notify_event_type type);
/* Pkg_private Notify_value	panel_notify_view_event(); */
Pkg_private int panel_nullproc(Panel_item it, Event *ev);
Pkg_private void panel_paint_image(Panel_info *panel, Panel_image *image,
    Rect *rect, int  inactive_item, int  color_index);
Pkg_private void panel_paint_svrim(Xv_Window pw, Pixrect *pr, int x, int y,
    int color_index, Pixrect *mask_pr);
Pkg_private void panel_paint_text(Xv_opaque	pw,
#ifdef OW_I18N
    XFontSet	font_xid,
#else
    Font	font_xid,
#endif /* OW_I18N */
    int		color_index, int x, int y, CHAR *str);
Pkg_private Item_info *panel_previous_kbd_focus(Panel_info	*panel, int	wrap);
Pkg_private int panel_printable_char(int code);
Pkg_private void panel_pw_invert(Xv_Window pw, Rect *rect, int color_index);
Pkg_private void panel_redisplay(Panel panel_public, Xv_Window pw, Rectlist *repaint_area);
Pkg_private void panel_redisplay_item(Item_info *ip, Panel_setting   flag);
Pkg_private void panel_refont(Panel_info *panel, int arg);
Pkg_private void panel_register_view(Panel_info *panel, Xv_Window view);
/* Pkg_private int			panel_resize(); */
Pkg_private int panel_round(int x, int y);
Pkg_private int		 	panel_row_to_y(Xv_Font font, int line);
/* Pkg_private void		panel_sel_init(); */
Pkg_private Xv_opaque panel_set_avlist(Panel panel_public, Attr_avlist avlist);
Pkg_private void panel_set_bold_label_font(Item_info *ip);
Pkg_private Panel_item panel_set_kbd_focus(Panel_info *panel, Item_info *ip);
Pkg_private char * panel_strsave(char *source);
Pkg_private Item_info *panel_successor(Item_info *ip);
Pkg_private void panel_text_caret_on(Panel_info     *panel, int on);
Pkg_private void panel_text_paint_label(Item_info *ip);
Pkg_private void panel_unlink(Item_info *ip, int destroy);
Pkg_private void panel_update_extent(Panel_info *panel, Rect rect);
/* Pkg_private void		panel_update_scrollbars(); */
Pkg_private void panel_user_error(Item_info	*object, Event	*event);
Pkg_private Rect *panel_viewable_rect(Panel_info *panel, Xv_Window pw);
Pkg_private int panel_viewable_width(Panel_info *panel, Xv_Window pw);
Pkg_private int panel_viewable_height(Panel_info *panel, Xv_Window pw);
Pkg_private int			panel_wants_focus(Panel_info *);
Pkg_private int			panel_width(Panel_info *);
Pkg_private void panel_yield_kbd_focus(Panel_info *panel);
Pkg_private void panel_autoscroll_start_itimer(Panel_item item, Notify_timer_func autoscroll_itimer_func);
Pkg_private void panel_autoscroll_stop_itimer(Panel_item item);
Pkg_private void panel_paint_border(Panel panel_public, Panel_info *panel, Xv_Window pw);
#ifdef OW_I18N
Xv_private  void		ml_panel_display_interm();
Pkg_private void 		ml_panel_saved_caret();
Pkg_private void 		panel_implicit_commit();
Pkg_private wchar_t	       *panel_strsave_wc();
Xv_private void	        	panel_preedit_display();
Xv_private void	        	panel_text_start();
Xv_private void	        	panel_text_draw();
Xv_private void	        	panel_text_done();
#endif /* OW_I18N */

Pkg_private int panel_item_x_start(Panel_info *panel);
Pkg_private int panel_item_y_start(Panel_info *panel);
Pkg_private int panel_label_x_gap(Panel_info *panel);
Pkg_private int panel_label_y_gap(Panel_info *panel);
Pkg_private void panel_layout_items(Panel pan, int do_fit_width);
Pkg_private void panel_buttons_to_menu(Panel pan);
Pkg_private void panel_align_labels(Panel pan, Panel_item *items);
Pkg_private void panel_update_scrolling_size(Panel client_panel);
Xv_private Notify_value panel_base_event_handler(Notify_client client,
     Notify_event event, Notify_arg arg, Notify_event_type type);

Pkg_private int panel_view_init(Panel parent, Panel_view view_public, Attr_attribute  avlist[], int *u);
Pkg_private int panel_x_to_col(Xv_Font font, int x);
Pkg_private int panel_event_is_xview_semantic(Event *event);

Pkg_private const Xv_pkg xv_panel_pw_pkg;

#endif
