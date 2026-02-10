#ifndef lint
char     openwin_c_sccsid[] = "@(#)openwin.c 1.37 93/06/28 DRA: $Id: openwin.c,v 4.8 2026/02/09 19:37:04 dra Exp $ ";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */

/*
 * Package:     openwin
 *
 * Module:      openwin.c
 *
 * Description: Implements general creation and initialization for openwin
 *
 */

#include <stdio.h>
#include <xview/openwin.h>
#include <xview_private/i18n_impl.h>
#include <xview_private/attr_impl.h>
#include <xview_private/draw_impl.h>
#include <xview_private/sb_impl.h>
#include <xview_private/svr_impl.h>
#include <xview_private/windowimpl.h>
#include <xview/win_notify.h>
#include <xview/scrollbar.h>
#include <xview/defaults.h>
#include <xview/cms.h>
#include <xview/help.h>
#include <xview/font.h>
#include <xview/cursor.h>
#include <xview/sel_pkg.h>
#include <olgx/olgx.h>

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

#define ADONE ATTR_CONSUME(avlist[0]);break

typedef Xv_opaque  Resize_handle;
static const Xv_pkg *resize_handle_class(void);

extern char *xv_app_name;

#define RESIZE_HANDLE resize_handle_class()

#define RH_APPL_RESIZES OPENWIN_ATTR(ATTR_BOOLEAN, 244)
#define RH_SIDE         OPENWIN_ATTR(ATTR_BOOLEAN, 245)

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
	openwin_split_init_proc split_init_proc;
	openwin_split_destroy_proc split_destroy_proc;
	openwin_resize_verification_t resize_verify_proc;
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
 * Package private functions
 */

extern char *xv_app_name;

/*
 * Module private functions
 */

/*
 * Global Data
 */
static Defaults_pairs sb_placement_pairs[] =
{
    { "Left",  OPENWIN_SCROLLBAR_LEFT },
    { "left",  OPENWIN_SCROLLBAR_LEFT },
    { "Right", OPENWIN_SCROLLBAR_RIGHT },
    { "right", OPENWIN_SCROLLBAR_RIGHT },
    { NULL,    OPENWIN_SCROLLBAR_RIGHT }
};

static int openwin_layout(Openwin owin_public, Xv_Window child, Window_layout_op op, Xv_opaque d1, Xv_opaque d2, Xv_opaque d3, Xv_opaque d4, Xv_opaque d5);

/*-------------------Function Definitions-------------------*/

#ifndef NO_OPENWIN_PAINT_BG
static void openwin_set_bg_color(Openwin owin_public)
{
	Xv_openwin_info  *owin = OPENWIN_PRIVATE(owin_public);
	Xv_Drawable_info *info;

	DRAWABLE_INFO_MACRO(owin_public, info);
	if (XAllocColor(xv_display(info),
			(Colormap)xv_get(xv_cms(info), XV_XID, 0),
			&(owin->background)) == 1) {
		XSetWindowBackground(xv_display(info), xv_xid(info),
				     owin->background.pixel);
		SERVERTRACE((500, "%s: %s: XClearWindow, bg=%lx\n", xv_app_name, __FUNCTION__, owin->background.pixel));
		XClearWindow(xv_display(info), xv_xid(info));
	}
}
#endif /* NO_OPENWIN_PAINT_BG */

static int openwin_count_views(Xv_openwin_info *owin)
{
	int i = 0;
	Openwin_view_info *view = owin->views;

	while (view != NULL) {
		i++;
		view = view->next_view;
	}
	return (i);
}

/*
 * openwin_adjust_view_scrollbars -
 */
static void openwin_adjust_view_scrollbars (Xv_openwin_info *owin, Openwin_view_info *view, Rect *avail_rect)
{
	int vsb_w, hsb_h;

	/* no computation if not adjusted or already adjusted */
	if (!STATUS(owin, adjust_vertical) && !STATUS(owin, adjust_horizontal)) {
		return;
	}

	vsb_w = hsb_h = scrollbar_width_for_owner(OPENWIN_PUBLIC(owin));

	if (STATUS(owin, adjust_vertical) && vsb_w < avail_rect->r_width) {
		avail_rect->r_width -= vsb_w;
		if (STATUS(owin, left_scrollbars))
			avail_rect->r_left += vsb_w;
	}
	if (STATUS(owin, adjust_horizontal) && hsb_h < avail_rect->r_height) {
		avail_rect->r_height -= hsb_h;
	}
}

/*
 * openwin_border_width - return the border with in pixels of an openwin
 */
static int openwin_border_width(Openwin owin_public, Xv_opaque view_public)
{
	/*
	 * OPENWIN_SHOW_BORDERS & WIN_BORDER are the same now since borders are
	 * always drawn using X window borders for performance reasons. However,
	 * this might have to change when we implement border highlighting for
	 * pane selection.
	 */

	if (((int)xv_get(owin_public, OPENWIN_SHOW_BORDERS) == TRUE) ||
			(view_public && (int)xv_get(view_public, WIN_BORDER) == TRUE)) {
		return (WIN_DEFAULT_BORDER_WIDTH);
	}
	else {
		return (0);
	}
}

/*
 * openwin_adjust_view_by_margins -
 */
static void openwin_adjust_view_by_margins(Xv_openwin_info *owin,
					Openwin_view_info *view, int margin, Rect *view_rect)
{
	int n_vmargins, n_hmargins;
	int border_width = 0;

	/* set up margins */
	if (STATUS(owin, no_margin)) {
		n_vmargins = n_hmargins = 0;
	}
	else {
#ifndef SVR4
		n_vmargins = n_hmargins = 1;
#else /* SVR4 */
		n_vmargins = n_hmargins = 2;
#endif /* SVR4 */
	}

	/* get rid of margin if view is on one of the edges, or if there is
	 * a scrollbar
	 */
	if (view->right_edge ||
		(openwin_sb(view, SCROLLBAR_VERTICAL) != XV_NULL) ||
		STATUS(owin, adjust_vertical))
		n_vmargins = 0;
	if (view->bottom_edge ||
		(openwin_sb(view, SCROLLBAR_HORIZONTAL) != XV_NULL) ||
		STATUS(owin, adjust_horizontal))
		n_hmargins = 0;

	border_width = openwin_border_width(OPENWIN_PUBLIC(owin), VIEW_PUBLIC(view));

	view_rect->r_width -= n_vmargins * margin + 2 * border_width;
	view_rect->r_height -= n_hmargins * margin + 2 * border_width;
}

/*
 * openwin_view_rect_from_avail_rect -
 */
static void openwin_view_rect_from_avail_rect(Xv_openwin_info *owin,
							Openwin_view_info *view, Rect *r)
{
    openwin_adjust_view_scrollbars(owin, view, r);
    openwin_adjust_view_by_margins(owin, view, owin->margin, r);
}

/*
 * openwin_place_scrollbar - position the scrollbar inside the openwin
 */
static void openwin_place_scrollbar(Xv_object owin_public,
			Xv_opaque view_public, Scrollbar sb, Scrollbar_setting direction,
			Rect *r, Rect *sb_r)
{
	Xv_openwin_info *owin = OPENWIN_PRIVATE(owin_public);
	int border_width;

	if (sb == XV_NULL) return;

	border_width = openwin_border_width(owin_public, view_public);

	if (direction == SCROLLBAR_VERTICAL) {
		sb_r->r_width = scrollbar_width(sb);
		sb_r->r_height = r->r_height + (2 * border_width);
		sb_r->r_top = r->r_top;
		if (STATUS(owin, left_scrollbars))
			sb_r->r_left = r->r_left - sb_r->r_width;
		else
			sb_r->r_left = r->r_left + r->r_width + (2 * border_width);
	}
	else {
		sb_r->r_left = r->r_left;
		sb_r->r_top = r->r_top + r->r_height + (2 * border_width);
		sb_r->r_width = r->r_width + (2 * border_width);
		sb_r->r_height = scrollbar_width(sb);
	}
}

/*
 * openwin_adjust_view_rect -
 */
static void openwin_adjust_view_rect(Xv_openwin_info *owin, Openwin_view_info *vpriv, Rect *view_rect)
{
	Scrollbar vsb, hsb;

	vsb = openwin_sb(vpriv, SCROLLBAR_VERTICAL);
	hsb = openwin_sb(vpriv, SCROLLBAR_HORIZONTAL);

	xv_set(VIEW_PUBLIC(vpriv), WIN_RECT, view_rect, NULL);

	if (vsb != XV_NULL) {
		xv_set(vsb,
				SCROLLBAR_VIEW_LENGTH,
				view_rect->r_height / (int)xv_get(vsb,
						SCROLLBAR_PIXELS_PER_UNIT), NULL);
	}
	if (hsb != XV_NULL) {
		xv_set(hsb,
				SCROLLBAR_VIEW_LENGTH,
				view_rect->r_width / (int)xv_get(hsb,
						SCROLLBAR_PIXELS_PER_UNIT), NULL);
	}
}

/*
 * openwin_adjust_view - resize the view to fit the rect
 */
static void openwin_adjust_view(Xv_openwin_info *owin,
    Openwin_view_info *view, Rect *view_rect)
{
	Rect r, sb_r;
	Scrollbar sb;

	r = view->enclosing_rect = *view_rect;

	openwin_view_rect_from_avail_rect(owin, view, &r);

	if (r.r_width <= 0) {
		r.r_width = view_rect->r_width;
	}
	else if (r.r_height <= 0) {
		r.r_height = view_rect->r_height;
	}
	/* place the scrollbars */
	if ((sb = openwin_sb(view, SCROLLBAR_VERTICAL)) != XV_NULL) {
		openwin_place_scrollbar(OPENWIN_PUBLIC(owin), VIEW_PUBLIC(view),
				openwin_sb(view, SCROLLBAR_VERTICAL), SCROLLBAR_VERTICAL,
				&r, &sb_r);
		xv_set(sb, WIN_RECT, &sb_r, NULL);
	}
	if ((sb = openwin_sb(view, SCROLLBAR_HORIZONTAL)) != XV_NULL) {

		openwin_place_scrollbar(OPENWIN_PUBLIC(owin), VIEW_PUBLIC(view),
				openwin_sb(view, SCROLLBAR_HORIZONTAL), SCROLLBAR_HORIZONTAL,
				&r, &sb_r);
		xv_set(sb, WIN_RECT, &sb_r, NULL);
	}
	/*
	 * now place the view.  Must do this after placing the sb's because if
	 * the sb's are moved after the view has been resized, they cause an
	 * exposure to be sent to the view, causing a second repaint.
	 */
	openwin_adjust_view_rect(owin, view, &r);
}

/*
 * openwin_rescale - resize the openwin for the given scale
 */
static void openwin_rescale(Openwin owin_public, int scale)
{
	Xv_openwin_info *owin = OPENWIN_PRIVATE(owin_public);
	Openwin_view_info *view = owin->views;
	Window_rescale_rect_obj *rect_obj_list;
	int num_views = 0, i = 0;
	int parent_width, parent_height;
	Rect new_rect, parent_new_rect;

	SERVERTRACE((790, "%s: scale=%d\n", __FUNCTION__, scale));
	/*
	 * first change scale unless this has been in the event func
	 */

	parent_new_rect = *(Rect *) xv_get(owin_public, WIN_RECT);
	parent_width = parent_new_rect.r_width;
	parent_height = parent_new_rect.r_height;

	/*
	 * Openwin rect has been set. The rescale has changed the font as well
	 */

	num_views = openwin_count_views(owin);
	rect_obj_list = window_create_rect_obj_list(num_views);

	for (view = owin->views; view != NULL; view = view->next_view) {
		Event event;
		Xv_window v = VIEW_PUBLIC(view);

		event_set_id(&event, ACTION_RESCALE);
		event_set_action(&event, ACTION_RESCALE);
		SERVERTRACE((790, "posting ACTION_RESCALE to %ld\n", v));
    	notify_post_event_and_arg(v, (Notify_event)&event, NOTIFY_IMMEDIATE,
			      (unsigned long)scale, NOTIFY_COPY_NULL, NOTIFY_RELEASE_NULL);

		window_set_rescale_state(v, scale);
		window_start_rescaling(v);
		/* third arg has to be address [vmh - 10/16/90] */
		window_add_to_rect_list(rect_obj_list, v, &view->enclosing_rect, i);
		i++;
	}
	window_adjust_rects(rect_obj_list, owin_public, num_views, parent_width,
			parent_height);
	i = 0;
	for (view = owin->views; view != NULL; view = view->next_view) {
		if (!window_rect_equal_ith_obj(rect_obj_list, &new_rect, i))
			openwin_adjust_view(owin, view, &new_rect);
		window_end_rescaling(VIEW_PUBLIC(view));
	}
	window_destroy_rect_obj_list(rect_obj_list);
}

/*
 * openwin_adjust_views - resize all views of a openwin to fit the rect
 */
static void openwin_adjust_views(Xv_openwin_info *owin, Rect *owin_rect)
{
    Openwin_view_info *view = owin->views;
    Rect            r;
    int		    adjust_rect;

    /* find the views that are on the vertical edge */
    for (view = owin->views; view != NULL; view = view->next_view) {
	adjust_rect = FALSE;
	r = view->enclosing_rect;

	/* See if view is visable in the owin */
	if ((owin_rect->r_width > r.r_left) &&
	    (owin_rect->r_height > r.r_top)) {

	    if (view->right_edge) {
		r.r_width = owin_rect->r_width - r.r_left;
		if (r.r_width <= 0) {
		    r.r_width = 1;
		}
		adjust_rect = TRUE;
	    }

	    if (view->bottom_edge) {
		r.r_height = owin_rect->r_height - r.r_top;
		if (r.r_height <= 0)
		  r.r_height = 1;
		adjust_rect = TRUE;
	    }

	    if (adjust_rect)
	      openwin_adjust_view(owin, view, &r);
	}
    }
}

/*
 * openwin_event - event handler for openwin
 */
static Notify_value openwin_event(Openwin owin_public, Notify_event ev,
    Notify_arg arg, Notify_event_type type)
{
    Event *event = (Event *)ev;
	Xv_openwin_info *owin = OPENWIN_PRIVATE(owin_public);
	Rect r;

	switch (event_action(event)) {
		case ACTION_RESCALE:
			openwin_rescale(owin_public, (int)arg);
			break;

		case WIN_RESIZE:
			r = *(Rect *) xv_get(owin_public, WIN_RECT);
			openwin_adjust_views(owin, &r);
			owin->cached_rect = r;
			break;
		case WIN_REPAINT:
			/* Enable painting in openwin_paint_border */
			STATUS_SET(owin, mapped);
			break;

		default:
			break;
	}

	return notify_next_event_func(owin_public, ev, arg, type);
}

/*
 * openwin_init - initialize the openwin data structure
 */
static int openwin_init(Xv_opaque parent, Xv_opaque self, Attr_avlist avlist,
										int *unused)
{
	Xv_openwin *openwin = (Xv_openwin *) self;
	Xv_openwin_info *owin;
	Xv_font cursfont;
	XFontStruct *fontinfo;

#ifndef NO_OPENWIN_PAINT_BG
	Xv_Drawable_info *info;
	screen_ui_style_t ui_style;
#endif /* NO_OPENWIN_PAINT_BG */

	if (!(owin = xv_alloc(Xv_openwin_info))) {
		fprintf(stderr, XV_MSG("can't allocate openwin structure. Abort\n"));
		return XV_ERROR;
	}
	owin->public_self = self;
	openwin->private_data = (Xv_opaque) owin;
	owin->margin = OPENWIN_REGULAR_VIEW_MARGIN;
	owin->cached_rect = *(Rect *) xv_get(self, WIN_RECT);

	STATUS_SET(owin, auto_clear);
	STATUS_SET(owin, show_borders);

	/* no longer:    STATUS_SET(owin, selectable); */

	if (defaults_get_enum("openWindows.scrollbarPlacement",
					"OpenWindows.ScrollbarPlacement",
					sb_placement_pairs) == OPENWIN_SCROLLBAR_LEFT)
		STATUS_SET(owin, left_scrollbars);
	else
		STATUS_RESET(owin, left_scrollbars);

	owin->layout_proc = (window_layout_proc_t) xv_get(self, WIN_LAYOUT_PROC);

#ifndef NO_OPENWIN_PAINT_BG
	DRAWABLE_INFO_MACRO(self, info);
	ui_style = (screen_ui_style_t)xv_get(xv_screen(info), SCREEN_UI_STYLE);

	if (ui_style != SCREEN_UIS_2D_BW) {
		STATUS_SET(owin, paint_bg);
		XParseColor(xv_display(info),
				(Colormap) xv_get(xv_cms(info), XV_XID, 0),
				defaults_get_string("openWindows.windowColor",
						"OpenWindows.WindowColor", "#cccccc"),
				&(owin->background));
		openwin_set_bg_color(self);
	}
	else
		STATUS_RESET(owin, paint_bg);
#endif /* NO_OPENWIN_PAINT_BG */

	/*
	 * For performance reasons, the openwin borders are always being painted
	 * using X borders. This might change when border highlighting is
	 * implemented for pane selection. WIN_CONSUME_PICK_EVENT, MS_LEFT &
	 * WIN_REPAINT will have to turned on here to implement border
	 * highlighting.
	 */
	xv_set(self,
			WIN_NOTIFY_SAFE_EVENT_PROC, openwin_event,
			WIN_NOTIFY_IMMEDIATE_EVENT_PROC, openwin_event,
			/*
			   WIN_INHERIT_COLORS, TRUE,
			 */
			WIN_LAYOUT_PROC, openwin_layout,
			NULL);

	cursfont = xv_find((Xv_opaque)self, FONT,
					FONT_FAMILY, FONT_FAMILY_OLCURSOR,
					NULL);
	fontinfo = (XFontStruct *)xv_get(cursfont, FONT_INFO);

	if (fontinfo->min_char_or_byte2 > OLC_BUSY_PTR) {
		/* I once encountered such a funny X server - I didn't believe it */
		/* broken X server - assume old open look cursor font */
		owin->target_ptr = xv_create(self, CURSOR,
    				CURSOR_SRC_CHAR, OLC_PANNING_PTR,
    				CURSOR_MASK_CHAR, OLC_PANNING_MASK_PTR,
					NULL);
	}
	else if (fontinfo->max_char_or_byte2 >= OLC_NAVIGATION_LEVEL_PTR &&
		fontinfo->per_char[OLC_NAVIGATION_LEVEL_PTR].width > 0)
	{
		/* new open look cursor font */
		owin->target_ptr = xv_create(self, CURSOR,
    				CURSOR_SRC_CHAR, OLC_NAVIGATION_LEVEL_PTR,
    				CURSOR_MASK_CHAR, OLC_NAVIGATION_LEVEL_MASK_PTR,
					NULL);
	}
	else {
		/* old open look cursor font */
		owin->target_ptr = xv_create(self, CURSOR,
    				CURSOR_SRC_CHAR, OLC_PANNING_PTR,
    				CURSOR_MASK_CHAR, OLC_PANNING_MASK_PTR,
					NULL);
	}
	return XV_OK;
}

/*
 * ow_set_width - set the width of the openwin to the ncols columns of text.
 */
static void ow_set_width(Xv_openwin_info *owin, int ncols)	/* number of columns */
{
	Openwin owin_public = OPENWIN_PUBLIC(owin);
	Scrollbar sb = openwin_sb(owin->views, SCROLLBAR_VERTICAL);
	int sb_width;
	int width;

	if (sb)
		sb_width = (int)xv_get(sb, XV_WIDTH, 0);
	else if (STATUS(owin, adjust_vertical)) {
		sb_width = scrollbar_width_for_owner(owin_public);
	}
	else
		sb_width = 0;
	width = (int)xv_cols(VIEW_PUBLIC(owin->views), ncols) +
			(STATUS(owin, no_margin) ? 0 :
			(int)xv_get(owin_public, XV_LEFT_MARGIN) +
			(int)xv_get(owin_public, XV_RIGHT_MARGIN)) +
			sb_width +
			2 * owin->margin +
			2 * openwin_border_width(owin_public, VIEW_PUBLIC(owin->views));
	if ((int)xv_get(owin_public, XV_WIDTH) != width)
		xv_set(owin_public, XV_WIDTH, width, NULL);
}

static void ow_set_height(Xv_openwin_info *owin, int nrows)	/* number of columns */
{
	Openwin owin_public = OPENWIN_PUBLIC(owin);
	Scrollbar sb = openwin_sb(owin->views, SCROLLBAR_HORIZONTAL);
	int sb_height;
	int height;

	if (sb)
		sb_height = (int)xv_get(sb, XV_WIDTH);
	else if (STATUS(owin, adjust_horizontal)) {
		sb_height = scrollbar_width_for_owner(owin_public);
	}
	else
		sb_height = 0;
	height = (int)xv_rows(VIEW_PUBLIC(owin->views), nrows) +
			(STATUS(owin, no_margin) ? 0 :
			(int)xv_get(owin_public, WIN_TOP_MARGIN) +
			(int)xv_get(owin_public, WIN_BOTTOM_MARGIN)) +
			sb_height +
			2 * owin->margin +
			2 * openwin_border_width(owin_public, VIEW_PUBLIC(owin->views));

	if ((int)xv_get(owin_public, XV_HEIGHT) != height)
		xv_set(owin_public, XV_HEIGHT, height, NULL);
}

static void openwin_copy_scrollbar(Xv_openwin_info *owin, Scrollbar sb, Openwin_view_info *to_view)
{
	int view_length;
	Scrollbar_setting direction =
			(Scrollbar_setting) xv_get(sb, SCROLLBAR_DIRECTION);
	Rect sb_r, r;
	Scrollbar copy_sb;
	int pixs_per_unit;
	char instname[200], *owinst;
	const Xv_pkg *pkg;
	Openwin ow = OPENWIN_PUBLIC(owin);

	r = *(Rect *) xv_get(VIEW_PUBLIC(to_view), WIN_RECT);
	openwin_place_scrollbar(ow, VIEW_PUBLIC(to_view), sb, direction, &r, &sb_r);

	pixs_per_unit = (int)xv_get(sb, SCROLLBAR_PIXELS_PER_UNIT);
	view_length = (direction == SCROLLBAR_VERTICAL) ?
			(int)xv_get(VIEW_PUBLIC(to_view), WIN_HEIGHT) : (int)xv_get(VIEW_PUBLIC(to_view),
			WIN_WIDTH);
	view_length = view_length / pixs_per_unit;

	owinst = (char *)xv_get(ow, XV_INSTANCE_NAME);
	pkg = (const Xv_pkg *)xv_get(ow, XV_TYPE);
	sprintf(instname, "%s%sSB", owinst ? owinst : pkg->name,
					((direction == SCROLLBAR_VERTICAL) ? "vert" : "hor"));
	copy_sb = xv_create(ow, SCROLLBAR,
			XV_INSTANCE_NAME, instname,
			SCROLLBAR_DIRECTION, direction,
			SCROLLBAR_PIXELS_PER_UNIT, pixs_per_unit,
			SCROLLBAR_OBJECT_LENGTH, xv_get(sb, SCROLLBAR_OBJECT_LENGTH),
			SCROLLBAR_VIEW_START, xv_get(sb, SCROLLBAR_VIEW_START),
			SCROLLBAR_VIEW_LENGTH, view_length,
			SCROLLBAR_PAGE_LENGTH, xv_get(sb, SCROLLBAR_PAGE_LENGTH),
			SCROLLBAR_SHOW_PAGE, xv_get(sb, SCROLLBAR_SHOW_PAGE),
			SCROLLBAR_PAGE_HEIGHT, xv_get(sb, SCROLLBAR_PAGE_HEIGHT),
			XV_FONT, xv_get(sb, XV_FONT), /* es gab einen Grund... */
			SCROLLBAR_NORMALIZE_PROC, xv_get(sb, SCROLLBAR_NORMALIZE_PROC),
			SCROLLBAR_NOTIFY_CLIENT, VIEW_PUBLIC(to_view),
			SCROLLBAR_SPLITTABLE, xv_get(sb, SCROLLBAR_SPLITTABLE),
			SCROLLBAR_COMPUTE_SCROLL_PROC, xv_get(sb, SCROLLBAR_COMPUTE_SCROLL_PROC),
			WIN_RECT, &sb_r,
			XV_VISUAL, xv_get(sb, XV_VISUAL),
			WIN_CMS, xv_get(sb, WIN_CMS),
			XV_SHOW, TRUE,
			NULL);

	openwin_set_sb(to_view, direction, copy_sb);
}

/*
 * ow_set_scrollbar - give a scrollbar to an owin.  If sb is null, then
 *                    destroy all scrollbars of that direction.
 */
static Xv_opaque ow_set_scrollbar(Xv_openwin_info *owin, Scrollbar sb, Scrollbar_setting direction)
{
	Openwin_view_info *view = owin->views;
	Rect r;
	int view_length;
	Xv_opaque result = (Xv_opaque) XV_OK;
	Xv_opaque sb_notify_client;

	/* give the vertical scrollbar to the first view */
	if (sb != XV_NULL) {
		/* if we already have a scrollbar report an error */
		while (view != NULL) {
			if (openwin_sb(view, direction) != XV_NULL) {
				/* FATAL ERROR */
				return ((Xv_opaque) XV_ERROR);
			}
			view = view->next_view;
		}

		/* Reparent the scrollbar if necessary for
		 * SunView compatibility.  Must be done before
		 * placing the scrollbar.
		 */
		if (xv_get(sb, WIN_PARENT) != OPENWIN_PUBLIC(owin) ||
				xv_get(sb, XV_OWNER) != OPENWIN_PUBLIC(owin)) {
			xv_set(sb, WIN_PARENT, OPENWIN_PUBLIC(owin),
					XV_OWNER, OPENWIN_PUBLIC(owin), NULL);
		}

		/* give this scrollbar to first view */
		openwin_set_sb(owin->views, direction, sb);

		/* Adjust the size of the view and place the scrollbar. */
		r = owin->views->enclosing_rect;
		openwin_adjust_view(owin, owin->views, &r);

		view_length = (direction == SCROLLBAR_VERTICAL) ?
				(int)xv_get(VIEW_PUBLIC(owin->views), WIN_HEIGHT) :
				(int)xv_get(VIEW_PUBLIC(owin->views), WIN_WIDTH);
		view_length = view_length / (int)xv_get(sb, SCROLLBAR_PIXELS_PER_UNIT);

		xv_set(sb,
				SCROLLBAR_DIRECTION, direction,
				SCROLLBAR_VIEW_LENGTH, view_length, XV_SHOW, TRUE, NULL);

		sb_notify_client = xv_get(sb, SCROLLBAR_NOTIFY_CLIENT);
		if (sb_notify_client == XV_NULL ||
				sb_notify_client == OPENWIN_PUBLIC(owin)) {
			xv_set(sb, SCROLLBAR_NOTIFY_CLIENT, VIEW_PUBLIC(owin->views), NULL);
		}
		/* create new scrollbars for other views */
		view = owin->views->next_view;
		while (view != NULL) {
			openwin_copy_scrollbar(owin, sb, view);
			r = view->enclosing_rect;
			openwin_adjust_view(owin, view, &r);
			view = view->next_view;
		}
	}
	else {
		/* remove all scrollbars */
		/* set bit so layout code which removes sb's  */
		/* isn't invoked */
		/* for each view unset as having sb and adjust view */
		for (view = owin->views; view != NULL; view = view->next_view) {
			sb = openwin_sb(view, direction);
			openwin_set_sb(view, direction, XV_NULL);
			if (sb != XV_NULL) {
				xv_destroy(sb);
			}
		}
		r = *(Rect *) xv_get(OPENWIN_PUBLIC(owin), WIN_RECT);
		openwin_adjust_views(owin, &r);
	}
	return (result);
}

static int openwin_viewdata_for_view(Xv_Window window, Openwin_view_info **view)
{
	*view = VIEW_PRIVATE(window);
	if (*view != NULL) {
		return (XV_OK);
	}
	else {
		return (XV_ERROR);
	}
}

static void make_gcs(Xv_openwin_info *priv, Xv_window pw)
{
	Display *dpy = (Display *)xv_get(pw, XV_DISPLAY);
	Window xid = (Window)xv_get(pw, XV_XID);
	Xv_window view;
	XGCValues   gcv;
	Cms cms;
	int fore_index, back_index;
	unsigned long fg, bg;

	if (priv->resize_gc) XFreeGC(dpy, priv->resize_gc);

	cms = xv_get(pw, WIN_CMS);
	fore_index = (int)xv_get(pw, WIN_FOREGROUND_COLOR);
	back_index = (int)xv_get(pw, WIN_BACKGROUND_COLOR);
	bg = (unsigned long)xv_get(cms, CMS_PIXEL, back_index);
	fg = (unsigned long)xv_get(cms, CMS_PIXEL, fore_index);

	gcv.foreground = fg ^ bg;
	gcv.function = GXxor;
	gcv.subwindow_mode = IncludeInferiors;

	priv->resize_gc = XCreateGC(dpy, xid,
				GCForeground | GCFunction | GCSubwindowMode,
				&gcv);

	view = xv_get(pw, XV_OWNER);
	cms = xv_get(view, WIN_CMS);
	fore_index = (int)xv_get(view, WIN_FOREGROUND_COLOR);
	gcv.foreground = (unsigned long)xv_get(cms, CMS_PIXEL, fore_index);
	gcv.subwindow_mode = IncludeInferiors;
	priv->border_gc = XCreateGC(dpy, xid, GCForeground | GCSubwindowMode, &gcv);
}

static void deselect_view(Xv_openwin_info *priv)
{
	Openwin_view_info *vp = priv->selected_view;
	Xv_window view;
	int i;

	if (! vp) return;

	view = VIEW_PUBLIC(vp);
	priv->selected_view = (Openwin_view_info *)0;

    for (i = 0; i < (int)OPENWIN_RIGHT; i++) {
        if (priv->resize_handles[i]) {
            xv_set(priv->resize_handles[i], XV_SHOW, FALSE, NULL);
        }
    }

	SERVERTRACE((500, "%s: %s: XClearArea\n", xv_app_name, __FUNCTION__));
	XClearArea((Display *)xv_get(view, XV_DISPLAY),
				(Window)xv_get(vp->pw, XV_XID),
				0, 0, 0, 0, TRUE);
}

static void paint_border(Xv_openwin_info *priv, Openwin_view_info *vp)
{
	if (vp == priv->selected_view) {
		Xv_window view = VIEW_PUBLIC(vp);

		XDrawRectangle((Display *)xv_get(view, XV_DISPLAY),
					(Window)xv_get(view, XV_XID),
					priv->border_gc, 0, 0,
					(unsigned)xv_get(view, XV_WIDTH) - 1,
					(unsigned)xv_get(view, XV_HEIGHT) - 1);
	}
}

static void select_view(Xv_openwin_info *priv, Openwin_view_info *vp)
{
	if (vp) {
		Openwin self = OPENWIN_PUBLIC(priv);
		Xv_window view = VIEW_PUBLIC(vp);
		Xv_window pw = vp->pw;
		Display *dpy = (Display *)xv_get(pw, XV_DISPLAY);
		Window pww = xv_get(pw, XV_XID);
		Resize_handle rh;
		int ar;
		int sb_off;
		Rect ow_r, view_r;
		Scrollbar sb;
		int num_views = (int)xv_get(self, OPENWIN_NVIEWS);
		int viewdelta = priv->margin + 2;

		if (priv->selected_view) {
			if (vp == priv->selected_view) return;
		}
		else {
			xv_set(priv->sel_owner, SEL_TIME, &priv->seltime, NULL);
			xv_set(priv->sel_owner, SEL_OWN, TRUE, NULL);
		}

		deselect_view(priv);

		/* now select the new view */
		priv->selected_view = vp;

		/* we want the rect BEFORE modifying border.... */
		view_r = *((Rect *)xv_get(view, XV_RECT));
		ow_r = *((Rect *)xv_get(self, XV_RECT));


/*
 	Auch wenn man hier die border_width auf 6 setzt, wird zwar ein
	dicker Rand gezeichnet, aber
	WEDER fallen Maus-Events auf dem dicken Rand in das View-Window
	NOCH ist auf dem dicken Rand der target_ptr
 */

		SERVERTRACE((500, "%s: %s: XClearArea\n", xv_app_name, __FUNCTION__));
		XClearArea(dpy, pww, 0, 0, 0, 0, TRUE);

		rh = XV_NULL;

		/* application says, my top is adjustable - and 'view' is the topmost */
		if ((priv->resize_sides & (1 << ((int)OPENWIN_TOP - 1))) 
			&& view_r.r_top < viewdelta)
		{
			rh = priv->resize_handles[(int)OPENWIN_TOP - 1];
			ar = TRUE;
		}
		else {
			if (num_views > 1 && view_r.r_top > viewdelta) {
				rh = priv->resize_handles[(int)OPENWIN_TOP - 1];
				ar = FALSE;
			}
		}
		if (rh) {
			xv_set(rh,
					RH_APPL_RESIZES, ar,
					XV_X, view_r.r_left + view_r.r_width / 2 -
							Vertsb_Endbox_Width(priv->ginfo) / 2,
					XV_Y, view_r.r_top,
					XV_WIDTH, Vertsb_Endbox_Width(priv->ginfo),
					XV_HEIGHT, Vertsb_Endbox_Height(priv->ginfo),
					XV_SHOW, TRUE,
					WIN_FRONT,
					NULL);
		}

		if ((sb = xv_get(self, OPENWIN_HORIZONTAL_SCROLLBAR, view)))
			sb_off = (int)xv_get(sb, XV_HEIGHT);
		else sb_off = 0;

		/* application says, my bottom is adjustable -
		 * and 'view' is the bottommost view
		 */
		rh = XV_NULL;
		if ((priv->resize_sides & (1 << ((int)OPENWIN_BOTTOM - 1)))
			&& rect_bottom(&view_r) > ow_r.r_height - sb_off - viewdelta)
		{
			rh = priv->resize_handles[(int)OPENWIN_BOTTOM - 1];
			ar = TRUE;
		}
		else {
			if (num_views > 1
				&& rect_bottom(&view_r) < ow_r.r_height - sb_off - viewdelta)
			{
				rh = priv->resize_handles[(int)OPENWIN_BOTTOM - 1];
				ar = FALSE;
			}
		}

		if (rh) {
			xv_set(rh,
					RH_APPL_RESIZES, ar,
					XV_X, view_r.r_left + view_r.r_width / 2 -
							Vertsb_Endbox_Width(priv->ginfo) / 2,
					XV_Y, rect_bottom(&view_r) -
							Vertsb_Endbox_Height(priv->ginfo) +
							2 * WIN_DEFAULT_BORDER_WIDTH + 1,
					XV_WIDTH, Vertsb_Endbox_Width(priv->ginfo),
					XV_HEIGHT, Vertsb_Endbox_Height(priv->ginfo),
					XV_SHOW, TRUE,
					WIN_FRONT,
					NULL);
		}

		rh = XV_NULL;
		if ((priv->resize_sides & (1 << ((int)OPENWIN_LEFT - 1))) 
			&& view_r.r_left < viewdelta)
		{
			rh = priv->resize_handles[(int)OPENWIN_LEFT - 1];
			ar = TRUE;
		}
		else {
			if (num_views > 1 && view_r.r_left > viewdelta) {
				rh = priv->resize_handles[(int)OPENWIN_LEFT - 1];
				ar = FALSE;
			}
		}
		if (rh) {
			xv_set(rh,
					RH_APPL_RESIZES, ar,
					XV_Y, view_r.r_top + view_r.r_height / 2 -
							Vertsb_Endbox_Width(priv->ginfo) / 2,
					XV_X, view_r.r_left,
					XV_HEIGHT, Vertsb_Endbox_Width(priv->ginfo),
					XV_WIDTH, Vertsb_Endbox_Height(priv->ginfo),
					XV_SHOW, TRUE,
					WIN_FRONT,
					NULL);
		}

		if ((sb = xv_get(self, OPENWIN_VERTICAL_SCROLLBAR, view)))
			sb_off = (int)xv_get(sb, XV_WIDTH);
		else sb_off = 0;

		rh = XV_NULL;
		if ((priv->resize_sides & (1 << ((int)OPENWIN_RIGHT - 1)))
			&& rect_right(&view_r) > ow_r.r_width - sb_off - viewdelta)
		{
			rh = priv->resize_handles[(int)OPENWIN_RIGHT - 1];
			ar = TRUE;
		}
		else {
			if (num_views > 1
				&& rect_right(&view_r) < ow_r.r_width - sb_off - viewdelta)
			{
				rh = priv->resize_handles[(int)OPENWIN_RIGHT - 1];
				ar = FALSE;
			}
		}
		if (rh) {
			xv_set(rh,
						RH_APPL_RESIZES, ar,
						XV_Y, view_r.r_top + view_r.r_height / 2 -
								Vertsb_Endbox_Width(priv->ginfo) / 2,
						XV_X, rect_right(&view_r) -
								Vertsb_Endbox_Height(priv->ginfo) +
								2 * WIN_DEFAULT_BORDER_WIDTH + 1,
						XV_HEIGHT, Vertsb_Endbox_Width(priv->ginfo),
						XV_WIDTH, Vertsb_Endbox_Height(priv->ginfo),
						XV_SHOW, TRUE,
						WIN_FRONT,
						NULL);
		}

		paint_border(vp->owin, vp);
	}
	else {
		if (priv->selected_view) {
			xv_set(priv->sel_owner, SEL_OWN, FALSE, NULL);
		}
		deselect_view(priv);
	}
}

static void openwin_lose_selection(Selection_owner owner)
{
    Openwin self = xv_get(owner, XV_OWNER);
	Xv_openwin_info *priv = OPENWIN_PRIVATE(self);

    if (! priv) return;
/*     if (priv->destroying) return; */

    /* the following leads to an endless recursion (from SEL_OWN, FALSE...) */
/*  xv_set(xv_get(owner, XV_OWNER), OPENWIN_SELECTED_VIEW, XV_NULL, NULL); */
    deselect_view(priv);
}

extern Graphics_info *xv_init_olgx(Xv_window, int *, Xv_font);

static void openwin_select_view(Openwin self, Openwin_view_info *vp)
{
	Xv_openwin_info *priv = OPENWIN_PRIVATE(self);

	/* we want to select a view even in the non-selectable case:
	 * if we have several views
	 */
/* 	if (! STATUS(priv, selectable)) { */
/* 		priv->selected_view = NULL; */
/* 		return; */
/* 	} */

	if (! priv->ginfo) {
		int three_d = FALSE;
		Resize_handle rh;

		three_d = (SCREEN_UIS_3D_COLOR ==
				(screen_ui_style_t)xv_get(XV_SCREEN_FROM_WINDOW(self),
												SCREEN_UI_STYLE));

#ifdef NOT_SO_GOOD
		priv->ginfo = (Graphics_info *)xv_init_olgx(self, &three_d,
													xv_get(self, XV_FONT));
		da schaltet z.B. CANVAS seine WIN_CMS um....
#endif

		make_gcs(priv, vp->pw);

		rh = xv_create(self, RESIZE_HANDLE,
						RH_SIDE, OPENWIN_TOP,
						XV_WIDTH, 2,
						XV_HEIGHT, 2,
						NULL);
		priv->resize_handles[OPENWIN_TOP-1] = rh;

		priv->ginfo = (Graphics_info *)xv_init_olgx(rh, &three_d,
													xv_get(self, XV_FONT));

		priv->resize_handles[OPENWIN_BOTTOM-1]=xv_create(self,RESIZE_HANDLE,
						RH_SIDE, OPENWIN_BOTTOM,
						XV_WIDTH, 2,
						XV_HEIGHT, 2,
						NULL);

		priv->resize_handles[OPENWIN_LEFT-1]=xv_create(self, RESIZE_HANDLE,
						RH_SIDE, OPENWIN_LEFT,
						XV_WIDTH, 2,
						XV_HEIGHT, 2,
						NULL);

		priv->resize_handles[OPENWIN_RIGHT-1]=xv_create(self,RESIZE_HANDLE,
						RH_SIDE, OPENWIN_RIGHT,
						XV_WIDTH, 2,
						XV_HEIGHT, 2,
						NULL);
	}

	select_view(OPENWIN_PRIVATE(self), vp);
}

/*
 * ow_append_view_attrs - add view attrs to the cached view avlist.
 */
static void ow_append_view_attrs(Xv_openwin_info *owin, Attr_avlist argv)
{
	if (owin->view_attrs == NULL) {
		/* No current list, so allocate a new one */
		owin->view_attrs = xv_alloc_n(Attr_attribute,
										(size_t)ATTR_STANDARD_SIZE);

		owin->view_end_attrs = owin->view_attrs;
	}
	owin->view_end_attrs = attr_copy_avlist(owin->view_end_attrs,
														argv);
}

static void set_rh_positions(Xv_openwin_info *priv, Openwin_view_info *vp)
{
	Rect view_r;

	if (vp != priv->selected_view) return;

	view_r = *((Rect *)xv_get(VIEW_PUBLIC(vp), XV_RECT));

	xv_set(priv->resize_handles[(int)OPENWIN_TOP - 1],
			XV_X, view_r.r_left + view_r.r_width / 2 -
					Vertsb_Endbox_Width(priv->ginfo) / 2,
			XV_Y, view_r.r_top,
			NULL);

	xv_set(priv->resize_handles[(int)OPENWIN_BOTTOM - 1],
			XV_X, view_r.r_left + view_r.r_width / 2 -
					Vertsb_Endbox_Width(priv->ginfo) / 2,
			XV_Y, rect_bottom(&view_r) -
					Vertsb_Endbox_Height(priv->ginfo) +
					2 * WIN_DEFAULT_BORDER_WIDTH + 1,
			NULL);

	xv_set(priv->resize_handles[(int)OPENWIN_LEFT - 1],
			XV_Y, view_r.r_top + view_r.r_height / 2 -
					Vertsb_Endbox_Width(priv->ginfo) / 2,
			XV_X, view_r.r_left,
			NULL);

	xv_set(priv->resize_handles[(int)OPENWIN_RIGHT - 1],
			XV_Y, view_r.r_top + view_r.r_height / 2 -
					Vertsb_Endbox_Width(priv->ginfo) / 2,
			XV_X, rect_right(&view_r) -
					Vertsb_Endbox_Height(priv->ginfo) +
					2 * WIN_DEFAULT_BORDER_WIDTH + 1,
			NULL);
}

Xv_private void screen_adjust_gc_color(Xv_Window window, int gc_index);

static void openwin_clear_damage(Xv_Window window, Rectlist *rl)
{
	register Xv_Drawable_info *info;
	Xv_Screen screen;
	GC *gc_list;

	if (rl) {
		DRAWABLE_INFO_MACRO(window, info);
		screen = xv_screen(info);
		gc_list = (GC *) xv_get(screen, SCREEN_OLGC_LIST, window);
		screen_adjust_gc_color(window, SCREEN_CLR_GC);
		XFillRectangle(xv_display(info), xv_xid(info), gc_list[SCREEN_CLR_GC],
				rl->rl_bound.r_left, rl->rl_bound.r_top,
				(unsigned)rl->rl_bound.r_width,
				(unsigned)rl->rl_bound.r_height);
	}
}

static void do_split(Openwin_view_info *vp,Openwin_split_direction dir, int pos)
{
	Openwin self = OPENWIN_PUBLIC(vp->owin);

	if (vp == vp->owin->selected_view) {
		xv_set(self, OPENWIN_SELECTED_VIEW, NULL, NULL);
	}
	xv_set(self,
			OPENWIN_SPLIT,
				OPENWIN_SPLIT_VIEW, VIEW_PUBLIC(vp),
				OPENWIN_SPLIT_DIRECTION, dir,
				OPENWIN_SPLIT_POSITION, pos,
				NULL,
			NULL);

	/* now we have at least two views: we act 'selectably': */
	if (! STATUS(vp->owin,selectable)) {
		/* but only if we have a paint window */
		if (vp->pw) {
			Openwin_view_info *view;

			for (view = vp->owin->views; view != NULL; view = view->next_view) {
				xv_set(VIEW_PUBLIC(view),
							WIN_CURSOR, vp->owin->target_ptr,
							NULL);
			}
		}
	}
}

static int openwin_locate_right_viewers( Openwin_view_info *views, Rect *r, Openwin_view_info *bounders[])
{
    Openwin_view_info *view;
    Rect            view_r;
    int             num_bound = 0;
    int             found_min = FALSE, found_max = FALSE;

    for (view = views; view != NULL; view = view->next_view) {
	view_r = view->enclosing_rect;

	if ((r->r_left + r->r_width) == view_r.r_left) {
	    /* if views starting point and ending point is */
	    /* between the gap add to list */
	    if (view_r.r_top >= r->r_top) {
		if (view_r.r_top + view_r.r_height <= r->r_top + r->r_height) {
		    bounders[num_bound++] = view;
		} else {
		    /* view extends beyond end of gap */
		    return (FALSE);
		}
	    }
	    if (view_r.r_top == r->r_top) {
		found_min = TRUE;
	    }
	    if (view_r.r_top + view_r.r_height == r->r_top + r->r_height) {
		found_max = TRUE;
	    }
	}
    }

    if (num_bound > 0) {
	bounders[num_bound] = NULL;
    }
    return (found_min && found_max);
}

static int openwin_locate_left_viewers(Openwin_view_info *views, Rect *r, Openwin_view_info *bounders[])
{
    Openwin_view_info *view;
    Rect            view_r;
    int             num_bound = 0;
    int             found_min = FALSE, found_max = FALSE;

    /* look below/to the right first */
    for (view = views; view != NULL; view = view->next_view) {
	view_r = view->enclosing_rect;

	if (r->r_left == (view_r.r_left + view_r.r_width)) {
	    /* see if view fits in the gap */
	    if (view_r.r_top >= r->r_top) {
		if (view_r.r_top + view_r.r_height <= r->r_top + r->r_height) {
		    bounders[num_bound++] = view;
		} else {
		    /* view extend beyond gap bounds */
		    return (FALSE);
		}
	    }
	    if (view_r.r_top == r->r_top) {
		found_min = TRUE;
	    }
	    if (view_r.r_top + view_r.r_height == r->r_top + r->r_height) {
		found_max = TRUE;
	    }
	}
    }

    if (num_bound > 0) {
	bounders[num_bound] = NULL;
    }
    return (found_min && found_max);
}

static int openwin_locate_bottom_viewers(Openwin_view_info *views, Rect *r, Openwin_view_info *bounders[])
{
    Openwin_view_info *view;
    Rect            view_r;
    int             num_bound = 0;
    int             found_min = FALSE, found_max = FALSE;

    /* look below/to the right first */
    for (view = views; view != NULL; view = view->next_view) {
	view_r = view->enclosing_rect;

	if ((r->r_top + r->r_height) == view_r.r_top) {
	    /* see if view fits in the gap */
	    if (view_r.r_left >= r->r_left) {
		if (view_r.r_left + view_r.r_width <= r->r_left + r->r_width) {
		    bounders[num_bound++] = view;
		} else {
		    /* view extend beyond gap bounds */
		    return (FALSE);
		}
	    }
	    if (view_r.r_left == r->r_left) {
		found_min = TRUE;
	    }
	    if (view_r.r_left + view_r.r_width == r->r_left + r->r_width) {
		found_max = TRUE;
	    }
	}
    }

    if (num_bound > 0) {
	bounders[num_bound] = NULL;
    }
    return (found_min && found_max);
}

static int openwin_locate_top_viewers(Openwin_view_info *views, Rect *r, Openwin_view_info *bounders[])
{
    Openwin_view_info *view;
    Rect            view_r;
    int             num_bound = 0;
    int             found_min = FALSE, found_max = FALSE;

    /* look below/to the right first */
    for (view = views; view != NULL; view = view->next_view) {
	view_r = view->enclosing_rect;

	if (r->r_top == (view_r.r_top + view_r.r_height)) {
	    /* see if view fits in the gap */
	    if (view_r.r_left >= r->r_left) {
		if (view_r.r_left + view_r.r_width <= r->r_left + r->r_width) {
		    bounders[num_bound++] = view;
		} else {
		    /* view extend beyond gap bounds */
		    return (FALSE);
		}
	    }
	    if (view_r.r_left == r->r_left) {
		found_min = TRUE;
	    }
	    if (view_r.r_left + view_r.r_width == r->r_left + r->r_width) {
		found_max = TRUE;
	    }
	}
    }

    if (num_bound > 0) {
	bounders[num_bound] = NULL;
    }
    return (found_min && found_max);
}

static void openwin_expand_viewers(Xv_openwin_info *owin, Openwin_view_info *old_view, Openwin_view_info **viewers, Rect *r, Openwin_split_direction direction)
{
    Rect            view_r;
    Openwin_view_info *view;

    for (view = *viewers; view != NULL; view = *(++viewers)) {
	view_r = view->enclosing_rect;
	if (direction == OPENWIN_SPLIT_VERTICAL) {
	    if (view_r.r_top > r->r_top) {
		view_r.r_top = r->r_top;
	    }
	    view_r.r_height += r->r_height;

	    if (old_view->bottom_edge) {
		view->bottom_edge = TRUE;
	    }
	} else {
	    if (view_r.r_left > r->r_left) {
		view_r.r_left = r->r_left;
	    }
	    view_r.r_width += r->r_width;

	    if (old_view->right_edge) {
		view->right_edge = TRUE;
	    }
	}
	openwin_adjust_view(owin, view, &view_r);
    }
}

static int openwin_fill_view_gap(Xv_openwin_info *owin, Openwin_view_info *view)
{
	Openwin_view_info *bounding_viewers[50];
	Rect r;

	/* four cases -- look right, left, bottom, top */

	r = view->enclosing_rect;
	if (openwin_locate_right_viewers(owin->views, &r, bounding_viewers)) {
		/* these windows grow horizontally */
		openwin_expand_viewers(owin, view, bounding_viewers, &r,
				OPENWIN_SPLIT_HORIZONTAL);
	}
	else if (openwin_locate_left_viewers(owin->views, &r, bounding_viewers)) {
		/* these windows grow horizontally */
		openwin_expand_viewers(owin, view, bounding_viewers, &r,
				OPENWIN_SPLIT_HORIZONTAL);
	}
	else if (openwin_locate_bottom_viewers(owin->views, &r, bounding_viewers)) {
		/* these windows grow vertically */
		openwin_expand_viewers(owin, view, bounding_viewers, &r,
				OPENWIN_SPLIT_VERTICAL);
	}
	else if (openwin_locate_top_viewers(owin->views, &r, bounding_viewers)) {
		/* these windows grow vertically */
		openwin_expand_viewers(owin, view, bounding_viewers, &r,
				OPENWIN_SPLIT_VERTICAL);
	}
	else {
		return (OPENWIN_CANNOT_EXPAND);
	}

	return (XV_OK);
}

static void do_join(Openwin_view_info *vp)
{
	int num_views;
	Xv_openwin_info *owin = vp->owin;
	Openwin self = OPENWIN_PUBLIC(owin);

	if (vp == owin->selected_view) {
		xv_set(self,
				OPENWIN_SELECTED_VIEW, NULL,
				NULL);
	}

	if (vp->pw) {
		if (owin->last_focus_pw == vp->pw) {
			Xv_window pw = XV_NULL, focwin;

			focwin = xv_get(xv_get(self,WIN_FRAME),FRAME_FOCUS_WIN);

			if (focwin && xv_get(focwin, WIN_PARENT) == vp->pw) {
				xv_set(focwin, XV_SHOW, FALSE, NULL);

				OPENWIN_EACH_PW(self, pw)
					if (pw != vp->pw) {
						xv_set(focwin, WIN_PARENT, pw, NULL);
						owin->last_focus_pw = pw;
						xv_set(self, WIN_SET_FOCUS, NULL);
						break;
					}
				OPENWIN_END_EACH
			}
			if (!pw || owin->last_focus_pw == vp->pw) {
				owin->last_focus_pw = XV_NULL;
			}
		}
	}

	num_views = openwin_count_views(owin);
	if (num_views > 1) {
		void (*destroy_proc)(Xv_window);

		/* before OPENVIEW_VIEW existed, we had only xv_destroy_save
		 * here - and the layout things were done in openwin_layout.
		 * See in openwin.c the WIN_DESTROY case .
		 *
		 * Now the method openwin_view_destroy destroys the 
		 * scrollbars (their WIN_DESTROY will be called...).
		 *
		 * But the WIN_DESTROY case is no longer called for the view....
		 */
		openwin_fill_view_gap(owin, vp);
		destroy_proc = owin->split_destroy_proc;

		xv_destroy_safe(VIEW_PUBLIC(vp));
		--num_views;

		if (destroy_proc) {
			destroy_proc(self);
		}

		if (num_views < 2) {
            if (! STATUS(owin,selectable)) {
				Xv_screen screen = XV_SCREEN_FROM_WINDOW(self);

				xv_set(VIEW_PUBLIC(owin->views),
						 WIN_CURSOR, xv_get(screen, SCREEN_BASIC_CURSOR),
						 NULL);
			}
		}
	}
}

static void split_destroy_preview(Xv_window win, Openwin_view_info *view,
										unsigned do_begin)
{
	Display *dpy = (Display *) xv_get(win, XV_DISPLAY);

	if (do_begin) {
		Xv_Drawable_info *info;
		Rect vrect;
		GC *gc_list;

		DRAWABLE_INFO_MACRO(win, info);
		gc_list = (GC *) xv_get(xv_screen(info), SCREEN_OLGC_LIST, win);

		vrect = *((Rect *) xv_get(win, WIN_OUTER_RECT));

		/* muss man immer wieder machen, wegen evtl PANEL_MULTILINE_TEXT... */
		XSetForeground(dpy, gc_list[SCREEN_JOINPREVIEW_GC], xv_bg(info));
		XFillRectangle(dpy, xv_get(OPENWIN_PUBLIC(view->owin), XV_XID),
				gc_list[SCREEN_JOINPREVIEW_GC], vrect.r_left, vrect.r_top,
				(unsigned)vrect.r_width, (unsigned)vrect.r_height);
	}
	else {
		XExposeEvent xexp;
		Event event;
		Window xid = (Window) xv_get(win, XV_XID);

		/* XClearWindow will NOT redraw the border */
		XUnmapWindow(dpy, xid);
		XMapWindow(dpy, xid);

		event_init(&event);
		event_set_id(&event, WIN_REPAINT);
		event_set_action(&event, WIN_REPAINT);
		event_set_window(&event, win);
		event_set_xevent(&event, (XEvent *) & xexp);
		xexp.type = Expose;
		xexp.send_event = 0;
		xexp.display = dpy;
		xexp.window = xid;
		xexp.x = 0;
		xexp.y = 0;
		xexp.width = (int)xv_get(win, XV_WIDTH);
		xexp.height = (int)xv_get(win, XV_HEIGHT);
		xexp.count = 0;

		win_post_event(win, &event, NOTIFY_IMMEDIATE);
	}
}

/*
 * openwin_view_event - event handler for openwin views
 */
static Notify_value openwin_view_event(Xv_window view, Notify_event ev,
								Notify_arg arg, Notify_event_type type)
{
    Event *event = (Event *)ev;
	Openwin_view_info *vp = VIEW_PRIVATE(view);
	Notify_value val;

	switch (event_action(event)) {
		case WIN_RESIZE:
			val =  notify_next_event_func(view, ev, arg, type);
			set_rh_positions(vp->owin, vp);
			return val;

        case ACTION_SELECT:
            if (STATUS(vp->owin,selectable) ||
				(vp->pw && openwin_count_views(vp->owin) > 1))
			{
				/* without that vp->pw query a TEXTSW with several views
				 * would no longer react on ACTION_SELECT
				 */
                if (event_is_up(event)) {
                    vp->owin->seltime = event_time(event);
                    xv_set(OPENWIN_PUBLIC(vp->owin),
								OPENWIN_SELECTED_VIEW, view,
								NULL);
                }
                return NOTIFY_DONE;
			}
            break;

		case WIN_REPAINT:
			/* clear the damaged area */
			if (STATUS(vp->owin, auto_clear)) {
				openwin_clear_damage(view, win_get_damage(view));
			}
			paint_border(vp->owin, vp);
			break;

		case ACTION_WHEEL_FORWARD:
			if (event_is_down(event) && vp->sb[0]) {
				int newpos = (int)xv_get(vp->sb[0], SCROLLBAR_VIEW_START);
				if (newpos > 0) {
					xv_set(vp->sb[0], SCROLLBAR_VIEW_START, newpos - 1, NULL);
				}
			}
			break;

		case ACTION_WHEEL_BACKWARD:
			if (event_is_down(event) && vp->sb[0]) {
				int newpos = (int)xv_get(vp->sb[0], SCROLLBAR_VIEW_START);
				xv_set(vp->sb[0], SCROLLBAR_VIEW_START, newpos + 1, NULL);
			}
			break;

		case ACTION_SPLIT_HORIZONTAL:
			do_split(vp, OPENWIN_SPLIT_HORIZONTAL, (int)arg);
			break;

		case ACTION_SPLIT_VERTICAL:
			do_split(vp, OPENWIN_SPLIT_VERTICAL, (int)arg);
			break;

		case ACTION_SPLIT_DESTROY:
			do_join(vp);
			return NOTIFY_DONE;

		case ACTION_HELP:
			if (event_is_down(event)) {
				char *help = (char *)xv_get(view, XV_HELP_DATA);

				if (help) {
					xv_help_show(view, help, event);
					return NOTIFY_DONE;
				}
				xv_help_show(view, "xview:pane_borders", event);
			}
			return NOTIFY_DONE;

		case ACTION_RESCALE:
			if (vp->sb[0]) {
    			notify_post_event_and_arg(vp->sb[0], ev, NOTIFY_IMMEDIATE,
			      		arg, NOTIFY_COPY_NULL, NOTIFY_RELEASE_NULL);
			}
			if (vp->sb[1]) {
    			notify_post_event_and_arg(vp->sb[1], ev, NOTIFY_IMMEDIATE,
			      		arg, NOTIFY_COPY_NULL, NOTIFY_RELEASE_NULL);
			}
			if (vp->pw) {
    			notify_post_event_and_arg(vp->pw, ev, NOTIFY_IMMEDIATE,
			      		arg, NOTIFY_COPY_NULL, NOTIFY_RELEASE_NULL);
			}
			return NOTIFY_DONE;

#ifdef ACTION_SPLIT_DESTROY_PREVIEW_BEGIN
		case ACTION_SPLIT_DESTROY_PREVIEW_BEGIN:
			if (openwin_count_views(vp->owin) <= 1) return NOTIFY_IGNORED;
			split_destroy_preview(view, vp, TRUE);
			break;

		case ACTION_SPLIT_DESTROY_PREVIEW_CANCEL:
			if (openwin_count_views(vp->owin) <= 1) return NOTIFY_IGNORED;
			split_destroy_preview(view, vp, FALSE);
			break;
#endif /* ACTION_SPLIT_DESTROY_PREVIEW_BEGIN */

		default:
			break;
	}
	return notify_next_event_func(view, ev, arg, type);
}

static Notify_value note_pw_event(Xv_window pw, Notify_event event,
							Notify_arg arg, Notify_event_type type)
{
	Notify_value val;
	Event *ev = (Event *)event;
	Openwin_view_info *vp = VIEW_PRIVATE(xv_get(pw,XV_OWNER));

	val = notify_next_event_func(pw, event, arg, type);

	switch (event_action(ev)) {
		case KBD_USE:
			vp->owin->last_focus_pw = pw;
			frame_kbd_use(xv_get(OPENWIN_PUBLIC(vp->owin), WIN_FRAME),
									OPENWIN_PUBLIC(vp->owin), pw);
			break;

		case KBD_DONE:
			frame_kbd_done(xv_get(OPENWIN_PUBLIC(vp->owin), WIN_FRAME),
									OPENWIN_PUBLIC(vp->owin));
			break;

		case WIN_REPAINT:

			paint_border(vp->owin, vp);
			break;
	}
	return val;
}

/* erstmal will ich das hier nicht in openwin_view_init verlagern, deshalb: */
static Openwin_view_info *very_transient_created = NULL;

static void openwin_create_viewwindow(Xv_openwin_info *owin,
			Openwin_view_info *from_view, Openwin_view_info *view, Rect *r)
{
	int xborders;
	Visual *visual;
	Cms cms;
	Openwin self = OPENWIN_PUBLIC(owin);
	Openwin_view newview;
	const Xv_pkg *vcl = (const Xv_pkg *)xv_get(self, OPENWIN_VIEW_CLASS);
	const Xv_pkg *pwcl = (const Xv_pkg *)xv_get(self, OPENWIN_PW_CLASS);

	if (from_view != NULL) {
		xborders = (int)xv_get(VIEW_PUBLIC(from_view), WIN_BORDER);
		visual = (Visual *) xv_get(VIEW_PUBLIC(from_view), XV_VISUAL);
		cms = (Cms) xv_get(VIEW_PUBLIC(from_view), WIN_CMS);
	}
	else {
		xborders = STATUS(owin, show_borders) ? TRUE : FALSE;
		visual = (Visual *) xv_get(OPENWIN_PUBLIC(owin), XV_VISUAL);
		cms = (Cms) xv_get(OPENWIN_PUBLIC(owin), WIN_CMS);
	}

	/* The openwin_view_init methode doesn't allocate that, but takes
	 * it from here:
	 */
	very_transient_created = view;

	if (owin->view_attrs == NULL) {
		/* parent/child registration process sets the data handle */
		newview = xv_create(OPENWIN_PUBLIC(owin), vcl,
				WIN_NOTIFY_SAFE_EVENT_PROC, openwin_view_event,
				WIN_NOTIFY_IMMEDIATE_EVENT_PROC, openwin_view_event,
				WIN_RECT, r,
				WIN_BORDER, (long)xborders,
				XV_VISUAL, visual,
				WIN_CMS, cms,
				NULL);
	}
	else {
		/* parent/child registration process sets the data handle */
		newview = xv_create(OPENWIN_PUBLIC(owin), vcl,
				ATTR_LIST, owin->view_attrs,
				WIN_NOTIFY_SAFE_EVENT_PROC, openwin_view_event,
				WIN_NOTIFY_IMMEDIATE_EVENT_PROC, openwin_view_event,
				WIN_RECT, r,
				WIN_BORDER, (long)xborders,
				XV_VISUAL, visual,
				WIN_CMS, cms,
				NULL);

		/* if client toggled xborders redo rect placement */
		if ((int)xv_get(newview, WIN_BORDER) != xborders) {
			*r = view->enclosing_rect;
			openwin_view_rect_from_avail_rect(owin, view, r);
			if (!rect_equal(&view->enclosing_rect, r)) {
				xv_set(newview, WIN_RECT, r, NULL);
			}
			/* no xborders is the default so if xborders is TRUE */
			/* and we each here assume client defaulted */
			/* WIN_BORDER to mean inherit from splittee. */
			/* Therefore we do a set to turn the borders on */
			/* we couldn't do this in the xv_create call */
			/* because ATTR_LIST must appear first in the avlist */
			/* This means a split can't override a border true */
			/* on create. No big deal since it is rare. */
			if (xborders) {
				xv_set(newview, WIN_BORDER, xborders, NULL);
			}
		}
		xv_free(owin->view_attrs);
		owin->view_attrs = NULL;
	}
	if (pwcl) {
		if (owin->pw_attrs == NULL) {
			if (STATUS(owin,selectable)) {
				view->pw = xv_create(newview, pwcl,
								WIN_BORDER, FALSE,
								WIN_COLLAPSE_EXPOSURES, FALSE,
								WIN_RETAINED, FALSE, /* wegen SubwindowMode */
								NULL);
			}
			else {
				view->pw = xv_create(newview, pwcl,
								WIN_BORDER, FALSE,
								WIN_COLLAPSE_EXPOSURES, FALSE,
								NULL);
			}
		}
		else {
			if (STATUS(owin,selectable)) {
				view->pw = xv_create(newview, pwcl,
								ATTR_LIST, owin->pw_attrs,
								WIN_BORDER, FALSE,
								WIN_COLLAPSE_EXPOSURES, FALSE,
								WIN_RETAINED, FALSE, /* wegen SubwindowMode */
								NULL);
			}
			else {
				view->pw = xv_create(newview, pwcl,
								ATTR_LIST, owin->pw_attrs,
								WIN_BORDER, FALSE,
								WIN_COLLAPSE_EXPOSURES, FALSE,
								NULL);
			}
			xv_free(owin->pw_attrs);
			owin->pw_attrs = NULL;
		}
		/* wenn von hier 'Unknown client' kommt, sollte man mit
		 * OPENWIN_PW_ATTRS bereits etwas Event-Handlings-maessiges
		 * einbauen
		 */
		notify_interpose_event_func(view->pw, note_pw_event, NOTIFY_SAFE);
		notify_interpose_event_func(view->pw, note_pw_event, NOTIFY_IMMEDIATE);

		if (STATUS(owin, selectable)) {
			xv_set(newview,
					WIN_CURSOR, owin->target_ptr,
					WIN_CONSUME_EVENTS,
						ACTION_HELP, WIN_MOUSE_BUTTONS, WIN_RESIZE, NULL,
					NULL);
		}
		else {
			xv_set(newview,
					WIN_CONSUME_EVENTS,
						ACTION_HELP, WIN_MOUSE_BUTTONS, WIN_RESIZE, NULL,
					NULL);
		}
	}
	else {
		STATUS_RESET(owin, selectable);
	}
}

static void openwin_link_view(Xv_openwin_info *owin, Openwin_view_info *view)
{
	Openwin_view_info *t_view;

	if (owin->views == NULL) {
		owin->views = view;
	}
	else {
		for (t_view = owin->views; t_view->next_view != NULL;
				t_view = t_view->next_view);
		t_view->next_view = view;
	}
}

static int openwin_unlink_view(Xv_openwin_info *owin, Openwin_view_info *view)
{
	Openwin_view_info *t_view;

	if (owin->views == view) {
		owin->views = view->next_view;
		return (XV_OK);
	}
	else {
		for (t_view = owin->views; t_view->next_view != NULL;
				t_view = t_view->next_view) {
			if (t_view->next_view == view) {
				t_view->next_view = view->next_view;
				return (XV_OK);
			}
		}
	}
	return (XV_ERROR);
}

static void openwin_init_view(Xv_openwin_info *owin, Openwin_view_info *twin,
					Openwin_split_direction direction, Rect *r,
					Openwin_view_info **new_view)
{
	Openwin_view_info *view;

	*new_view = NULL;

	/* allocate the view */
	view = xv_alloc(Openwin_view_info);
	view->owin = owin;

	view->enclosing_rect = *r;

	if (twin == NULL) {
		/* default view -- on create -- add scrollbars */
		if (owin->vsb_on_create) {
			openwin_set_sb(view, SCROLLBAR_VERTICAL, owin->vsb_on_create);
		}
		if (owin->hsb_on_create) {
			openwin_set_sb(view, SCROLLBAR_HORIZONTAL, owin->hsb_on_create);
		}
		view->right_edge = view->bottom_edge = TRUE;
	}
	else {
		if (direction == OPENWIN_SPLIT_VERTICAL) {
			view->right_edge = twin->right_edge;
			twin->right_edge = FALSE;
			view->bottom_edge = twin->bottom_edge;
		}
		else {
			view->bottom_edge = twin->bottom_edge;
			twin->bottom_edge = FALSE;
			view->right_edge = twin->right_edge;
		}
	}

	/* use old view so get border and sb info */
	openwin_view_rect_from_avail_rect(owin, view, r);

	/* create the view window */
	openwin_create_viewwindow(owin, twin, view, r);
	openwin_link_view(owin, view);

	*new_view = view;
}

static void openwin_split_view(Xv_openwin_info *owin,
				Openwin_view_info *view, Openwin_split_direction direction,
				int pos, int view_start)
{
	Openwin_view_info *new_view;
	Rect r, new_r;
	Scrollbar sb;

	/* compute the new rects for both the new and old views
	 * placing the new view to the left and above the old
	 */
	r = new_r = view->enclosing_rect;
	if (direction == OPENWIN_SPLIT_VERTICAL) {
		r.r_width = pos;
		new_r.r_left += pos;
		new_r.r_width -= pos;
	}
	else {
		r.r_height = pos;
		new_r.r_top += pos;
		new_r.r_height -= pos;
	}

	/* create new view */
	/* this automatically adjusts the view if needed */
	openwin_init_view(owin, view, direction, &new_r, &new_view);

	/* adjust active views rect */
	openwin_adjust_view(owin, view, &r);

	/* add needed scrollbars */
	if ((sb = openwin_sb(view, SCROLLBAR_VERTICAL)) != XV_NULL) {
		openwin_copy_scrollbar(owin, sb, new_view);
		if (direction == OPENWIN_SPLIT_HORIZONTAL) {
			sb = openwin_sb(new_view, SCROLLBAR_VERTICAL);
			xv_set(sb, SCROLLBAR_VIEW_START,
					view_start / (int)xv_get(sb, SCROLLBAR_PIXELS_PER_UNIT),
					NULL);
		}
	}
	if ((sb = openwin_sb(view, SCROLLBAR_HORIZONTAL)) != XV_NULL) {
		openwin_copy_scrollbar(owin, sb, new_view);
		if (direction == OPENWIN_SPLIT_VERTICAL) {
			sb = openwin_sb(new_view, SCROLLBAR_HORIZONTAL);
			xv_set(sb, SCROLLBAR_VIEW_START,
					view_start / (int)xv_get(sb, SCROLLBAR_PIXELS_PER_UNIT),
					NULL);
		}
	}

#ifdef SELECTABLE_VIEWS
	/* paint borders if needed */
	if (STATUS(owin, show_borders))
		openwin_paint_border(OPENWIN_PUBLIC(owin), new_view, TRUE);
#endif /* SELECTABLE_VIEWS */

	if (owin->split_init_proc) {
		(owin->split_init_proc)(VIEW_PUBLIC(view), VIEW_PUBLIC(new_view), pos);
	}
}

/*
 * ow_parse_split_attrs - parse split args, then split the view
 */
static Xv_opaque ow_parse_split_attrs(Xv_openwin_info *owin, Attr_avlist avlist)
{
	Attr_attribute attr;
	Openwin_split_direction split_direction = OPENWIN_SPLIT_HORIZONTAL;
	Openwin_view_info *view;
	Xv_Window split_view = XV_NULL;
	int split_position = 0;
	int split_viewstart = OPENWIN_SPLIT_NEWVIEW_IN_PLACE;
	Rect r;
	Scrollbar vsb, hsb;
	int min_size;

	for (attr = avlist[0]; attr; avlist = attr_next(avlist), attr = avlist[0]) {
		switch (attr) {
			case OPENWIN_SPLIT_DIRECTION:
				split_direction = (Openwin_split_direction) avlist[1];
				break;
			case OPENWIN_SPLIT_VIEW:
				split_view = (Xv_Window) avlist[1];
				break;
			case OPENWIN_SPLIT_POSITION:
				split_position = (int)avlist[1];
				break;
			case OPENWIN_SPLIT_INIT_PROC:
				owin->split_init_proc = (openwin_split_init_proc)avlist[1];
				break;

			case OPENWIN_SPLIT_DESTROY_PROC:
				owin->split_destroy_proc =(openwin_split_destroy_proc)avlist[1];
				break;

			case OPENWIN_SPLIT_VIEW_START:
				split_viewstart = (int)avlist[1];
				break;
			default:
				xv_check_bad_attr(OPENWIN, attr);
				break;
		}
	}

	/* do data validation */

	/* see if a window was passed to be split and if it is valid */
	if (split_view == XV_NULL
			|| openwin_viewdata_for_view(split_view, &view) != XV_OK) {
		/* error invalid view */
		return (XV_ERROR);
	}

	/* see if position is one in the window */
	vsb = openwin_sb(view, SCROLLBAR_VERTICAL);
	hsb = openwin_sb(view, SCROLLBAR_HORIZONTAL);
	r = *(Rect *) xv_get(split_view, WIN_RECT);
	if (split_direction == OPENWIN_SPLIT_VERTICAL) {
		if (hsb) {
			min_size = scrollbar_minimum_size(hsb);
			if (vsb)
				min_size += (int)xv_get(vsb, XV_WIDTH);
		}
		else
			min_size = OPENWIN_SPLIT_VERTICAL_MINIMUM;
		if (split_position < min_size || split_position > r.r_width - min_size)
			/* error invalid position */
			return (XV_ERROR);
	}
	else {
		if (vsb) {
			min_size = scrollbar_minimum_size(vsb);
			if (hsb)
				min_size += (int)xv_get(hsb, XV_HEIGHT);
		}
		else
			min_size = OPENWIN_SPLIT_HORIZONTAL_MINIMUM;
		if (split_position < min_size || split_position > r.r_height - min_size)
			/* error invalid position */
			return (XV_ERROR);
	}
	/* see if view start is valid */
	if (split_viewstart == OPENWIN_SPLIT_NEWVIEW_IN_PLACE) {
		Scrollbar sb = (split_direction == OPENWIN_SPLIT_VERTICAL) ? hsb : vsb;

		if (sb) {
			split_viewstart =
					(int)xv_get(sb, SCROLLBAR_VIEW_START) + split_position;
		}
		else {
			split_viewstart = split_position;
		}
	}
	openwin_split_view(owin, view, split_direction, split_position,
			split_viewstart);
	return (XV_OK);
}

static void openwin_register_initial_sb(Xv_openwin_info *owin,
		Openwin_view_info *view, Scrollbar sb, Scrollbar_setting direction)
{
	Rect r, sb_r;
	long unsigned view_length;

	r = *(Rect *) xv_get(VIEW_PUBLIC(view), WIN_RECT);

	openwin_place_scrollbar(OPENWIN_PUBLIC(owin), VIEW_PUBLIC(view), sb, direction,
			&r, &sb_r);

	view_length = (direction == SCROLLBAR_VERTICAL) ? r.r_height : r.r_width;
	view_length = view_length / (int)xv_get(sb, SCROLLBAR_PIXELS_PER_UNIT);

	if (xv_get(sb, WIN_PARENT) != OPENWIN_PUBLIC(owin) ||
			xv_get(sb, XV_OWNER) != OPENWIN_PUBLIC(owin))
		xv_set(sb,
				WIN_PARENT, OPENWIN_PUBLIC(owin),
				XV_OWNER, OPENWIN_PUBLIC(owin),
				NULL);
	xv_set(sb,
			WIN_RECT, &sb_r,
			SCROLLBAR_DIRECTION, direction,
			SCROLLBAR_VIEW_LENGTH, view_length,
			SCROLLBAR_NOTIFY_CLIENT, VIEW_PUBLIC(view),
			XV_SHOW, TRUE,
			NULL);
}

static void openwin_create_initial_view(Xv_openwin_info *owin)
{
	Rect r;
	Openwin_view_info *new_view;

	r = *(Rect *) xv_get(OPENWIN_PUBLIC(owin), WIN_RECT);
	r.r_left = r.r_top = 0;

	openwin_init_view(owin, NULL, OPENWIN_SPLIT_VERTICAL, &r, &new_view);

	/* add scrollbars if we have seen them */
	if (owin->vsb_on_create) {
		openwin_register_initial_sb(owin, new_view, owin->vsb_on_create,
				SCROLLBAR_VERTICAL);
		owin->vsb_on_create = XV_NULL;
	}
	if (owin->hsb_on_create) {
		openwin_register_initial_sb(owin, new_view, owin->hsb_on_create,
				SCROLLBAR_HORIZONTAL);
		owin->hsb_on_create = XV_NULL;
	}
}

/*
 * openwin_set - handle xv_set for an openwin
 */
static Xv_opaque openwin_set(Openwin self, Attr_avlist avlist)
{
	Xv_openwin_info *owin = OPENWIN_PRIVATE(self);
	Attr_attribute attr;
	Rect r;
	Xv_Window view, pw;

	Xv_opaque retval = XV_OK;


	for (attr = avlist[0]; attr; avlist = attr_next(avlist), attr = avlist[0]) {
		switch (attr) {
			case WIN_COLUMNS:
				/* If you intend to attach a vertical scrollbar to this openwin,
				 * and you want the view to be the WIN_COLUMNS specified, then
				 * you must specify
				 * OPENWIN_ADJUST_FOR_VERTICAL_SCROLLBAR, TRUE,
				 */
				if (STATUS(owin, created))
					ow_set_width(owin, (int)avlist[1]);
				else
					owin->nbr_cols = (int)avlist[1];
				ADONE;

			case WIN_ROWS:
				/* If you intend to attach a horizontal scrollbar to this openwin,
				 * and you want the view to be the WIN_ROWS specified, then
				 * you must specify
				 * OPENWIN_ADJUST_FOR_HORIZONTAL_SCROLLBAR, TRUE,
				 */
				if (STATUS(owin, created))
					ow_set_height(owin, (int)avlist[1]);
				else
					owin->nbr_rows = (int)avlist[1];
				ADONE;

			case WIN_VERTICAL_SCROLLBAR:
				if ((Scrollbar) avlist[1] != XV_NULL) {
					STATUS_SET(owin, adjust_vertical);
				}
				else {
					STATUS_RESET(owin, adjust_vertical);
				}
				if (STATUS(owin, created)) {
					(void)ow_set_scrollbar(owin, (Scrollbar) avlist[1],
							SCROLLBAR_VERTICAL);
				}
				else {
					owin->vsb_on_create = (Scrollbar) avlist[1];
				}
				break;
			case WIN_HORIZONTAL_SCROLLBAR:
				if ((Scrollbar) avlist[1] != XV_NULL) {
					STATUS_SET(owin, adjust_horizontal);
				}
				else {
					STATUS_RESET(owin, adjust_horizontal);
				}
				if (STATUS(owin, created)) {
					(void)ow_set_scrollbar(owin, (Scrollbar) avlist[1],
							SCROLLBAR_HORIZONTAL);
				}
				else {
					owin->hsb_on_create = (Scrollbar) avlist[1];
				}
				break;
			case OPENWIN_NO_MARGIN:
				if ((int)avlist[1] == 0) {
					STATUS_RESET(owin, no_margin);
				}
				else {
					STATUS_SET(owin, no_margin);
				}
				if (STATUS(owin, created))
					openwin_adjust_views(owin, &owin->cached_rect);
				ADONE;

			case OPENWIN_SHOW_BORDERS:
				if (!STATUS(owin, created)) {
					if ((int)avlist[1] == 0) {
						STATUS_RESET(owin, show_borders);
					}
					else {
						STATUS_SET(owin, show_borders);
					}
				}
				else {
					xv_error(self,
							ERROR_CREATE_ONLY, attr,
							ERROR_PKG, OPENWIN,
							NULL);
				}
				ADONE;

			case OPENWIN_SELECTED_VIEW:
				{
					Openwin_view_info *viewinfo;

					if (avlist[1] == XV_NULL)
						viewinfo = NULL;
					else
						openwin_viewdata_for_view((Xv_Window)avlist[1],
														&viewinfo);
					openwin_select_view(self, viewinfo);
				}
				ADONE;

			case OPENWIN_AUTO_CLEAR:
				if ((int)avlist[1] == 0) {
					STATUS_RESET(owin, auto_clear);
				}
				else {
					STATUS_SET(owin, auto_clear);
				}
				ADONE;
			case OPENWIN_ADJUST_FOR_VERTICAL_SCROLLBAR:
				if ((int)avlist[1] != STATUS(owin, adjust_vertical) &&
						(int)avlist[1] == FALSE) {
					STATUS_RESET(owin, adjust_vertical);
				}
				else if ((int)avlist[1] != STATUS(owin, adjust_vertical)) {
					STATUS_SET(owin, adjust_vertical);
				}
				if (STATUS(owin, created)) {
					r = *(Rect *) xv_get(OPENWIN_PUBLIC(owin), WIN_RECT);
					openwin_adjust_views(owin, &r);
				}
				ADONE;
			case OPENWIN_ADJUST_FOR_HORIZONTAL_SCROLLBAR:
				if ((int)avlist[1] != STATUS(owin, adjust_horizontal) &&
						(int)avlist[1] == FALSE) {
					STATUS_RESET(owin, adjust_horizontal);
				}
				else if ((int)avlist[1] != STATUS(owin, adjust_horizontal)) {
					STATUS_SET(owin, adjust_horizontal);
				}
				if (STATUS(owin, created)) {
					r = *(Rect *) xv_get(OPENWIN_PUBLIC(owin), WIN_RECT);
					openwin_adjust_views(owin, &r);
				}
				ADONE;
			case OPENWIN_VIEW_ATTRS:
				if (STATUS(owin, created)) {
					OPENWIN_EACH_VIEW(OPENWIN_PUBLIC(owin), view)
						xv_set_avlist(view, &(avlist[1]));
					OPENWIN_END_EACH
				}
				else {
					ow_append_view_attrs(owin, &(avlist[1]));
				}
				ADONE;
			case OPENWIN_PW_ATTRS:
				if (STATUS(owin, created)) {
					OPENWIN_EACH_PW(OPENWIN_PUBLIC(owin), pw)
						xv_set_avlist(pw, &(avlist[1]));
					OPENWIN_END_EACH
				}
				else {
					if (owin->pw_attrs == NULL) {
						/* No current list, so allocate a new one */
						owin->pw_attrs = xv_alloc_n(Attr_attribute,
											(size_t)ATTR_STANDARD_SIZE);
						owin->pw_end_attrs = owin->pw_attrs;
					}
					owin->pw_end_attrs = (Attr_avlist)attr_copy_avlist(
									owin->pw_end_attrs, avlist + 1);
				}
				ADONE;
			case OPENWIN_SELECTABLE:
				if (avlist[1]) {
					STATUS_SET(owin, selectable);
				}
				else {
					STATUS_RESET(owin, selectable);
				}
				ADONE;
			case OPENWIN_RESIZABLE:
                {
                    int i;

                    owin->resize_sides = 0;
                    for (i = 1; avlist[i]; i++) {
                        owin->resize_sides |= (1 << ((int)avlist[i] - 1));
                    }
                }
				ADONE;
			case OPENWIN_RESIZE_VERIFY_PROC:
                owin->resize_verify_proc = (openwin_resize_verification_t)avlist[1];
				ADONE;
			case OPENWIN_SPLIT:
				if (ow_parse_split_attrs(owin, &(avlist[1])) != XV_OK) {
					/* handle error */
				}
				ADONE;

#ifndef NO_OPENWIN_PAINT_BG
				/* catch any attempts to change background color */
			case WIN_CMS:
			case WIN_CMS_NAME:
			case WIN_CMS_DATA:
			case WIN_FOREGROUND_COLOR:
			case WIN_BACKGROUND_COLOR:
			case WIN_COLOR_INFO:
				{
					Xv_opaque defaults_array[ATTR_STANDARD_SIZE];

					if (STATUS(owin, paint_bg)) {
						defaults_array[0] = avlist[0];
						defaults_array[1] = avlist[1];
						defaults_array[2] = (Xv_opaque) 0;
						window_set_avlist(OPENWIN_PUBLIC(owin), defaults_array);
						ATTR_CONSUME(avlist[0]);
						openwin_set_bg_color(OPENWIN_PUBLIC(owin));
					}
				}
				break;
#endif /* NO_OPENWIN_PAINT_BG */

			case WIN_SET_FOCUS:
				retval = XV_ERROR;
				if (owin->last_focus_pw) {
					win_set_kbd_focus(owin->last_focus_pw,
								(Window)xv_get(owin->last_focus_pw, XV_XID));
					retval = XV_OK;
				}
				else {
					Xv_window pw;

					OPENWIN_EACH_PW(self, pw)
						if (win_getinputcodebit((Inputmask *)xv_get(pw,
										WIN_INPUT_MASK), KBD_USE)) {
							win_set_kbd_focus(pw, (Window)xv_get(pw, XV_XID));
							retval = XV_OK;
							break;
						}
					OPENWIN_END_EACH
				}
				ADONE;

			case XV_END_CREATE:
				/* openwin size if now correct */
				owin->cached_rect =
						*(Rect *) xv_get(OPENWIN_PUBLIC(owin), WIN_RECT);
				openwin_create_initial_view(owin);
				/* Note: ow_set_width and ow_set_height will each
				 * generate a WIN_RESIZE event on the openwin because they change
				 * the openwin's width and height, respectively.  This will cause
				 * openwin_adjust_views to be called, which calls
				 * openwin_adjust_view, which calls
				 * openwin_view_rect_from_avail_rect, which calls
				 * openwin_adjust_view_scrollbars, which changes the size of the
				 * view to make room for the scrollbar(s).
				 *
				 * N.B.:  The size of the view window will always be determined
				 * by the size of the enclosing openwin.  If adjust_{vertical,
				 * horizontal} is set, then the view is shrunk by the width of
				 * the scrollbar that would go there.  This adjustment is made
				 * in openwin_adjust_view_scrollbars.
				 * openwin_adjust_view_scrollbars is eventually called in the
				 * following situations:
				 *  - from openwin_create_initial_view
				 *  - a WIN_RESIZE event
				 *  - frow ow_set_width (causes a WIN_RESIZE)
				 *  - from ow_set_height (causes a WIN_RESIZE)
				 *  - from ow_set_scrollbar
				 */
				if (owin->nbr_cols > 0)
					ow_set_width(owin, owin->nbr_cols);
				if (owin->nbr_rows > 0)
					ow_set_height(owin, owin->nbr_rows);
				STATUS_SET(owin, created);
				{
					const Xv_pkg *socl = (const Xv_pkg *)xv_get(self, OPENWIN_SEL_OWNER_CLASS);
					owin->sel_owner = xv_create(self, socl,
								SEL_RANK, XA_PRIMARY,
								SEL_LOSE_PROC, openwin_lose_selection,
								NULL);
				}
				break;
			default:
				xv_check_bad_attr(OPENWIN, attr);
				break;
		}
	}
	return retval;
}

static Openwin_view_info *openwin_nth_view(Xv_openwin_info *owin, int place)
{
	int i = 0;
	Openwin_view_info *view = owin->views;

	for (i = 0; i < place; i++) {
		view = view->next_view;
		if (view == NULL) {
			return (NULL);
		}
	}
	return (view);
}

/*
 * openwin_get - return value for given attribute(s)
 */
static Xv_opaque openwin_get(Openwin owin_public, int *get_status,
							Attr_attribute attr, va_list valist)
{
	Xv_openwin_info *owin = OPENWIN_PRIVATE(owin_public);
	Openwin_view_info *view;
	Xv_opaque v = 0;

	switch (attr) {
		case OPENWIN_NTH_VIEW:
			view = openwin_nth_view(owin, va_arg(valist, int));

			if (view != NULL) {
				v = (Xv_opaque) VIEW_PUBLIC(view);
			}
			else {
				v = XV_NULL;
			}
			break;
		case OPENWIN_SHOW_BORDERS:
			v = (Xv_opaque) STATUS(owin, show_borders);
			break;
		case WIN_VERTICAL_SCROLLBAR:
			view = openwin_nth_view(owin, 0);
			if (view == NULL)
				v = (Xv_opaque) NULL;
			else
				v = (Xv_opaque) openwin_sb(view, SCROLLBAR_VERTICAL);
			break;
		case OPENWIN_NVIEWS:
			v = (Xv_opaque) openwin_count_views(owin);
			break;
		case OPENWIN_VERTICAL_SCROLLBAR:
			view = VIEW_PRIVATE(va_arg(valist, Xv_Window));
			if ((view == NULL) && ((view = openwin_nth_view(owin, 0)) == NULL))
				v = (Xv_opaque) NULL;
			else
				v = (Xv_opaque) openwin_sb(view, SCROLLBAR_VERTICAL);
			break;
		case OPENWIN_HORIZONTAL_SCROLLBAR:
			view = VIEW_PRIVATE(va_arg(valist, Xv_Window));
			if ((view == NULL) && ((view = openwin_nth_view(owin, 0)) == NULL))
				v = (Xv_opaque) NULL;
			else
				v = (Xv_opaque) openwin_sb(view, SCROLLBAR_HORIZONTAL);
			break;
		case OPENWIN_AUTO_CLEAR:
			v = (Xv_opaque) STATUS(owin, auto_clear);
			break;
		case WIN_HORIZONTAL_SCROLLBAR:
			view = openwin_nth_view(owin, 0);
			if (view == NULL)
				v = (Xv_opaque) NULL;
			else
				v = (Xv_opaque) openwin_sb(view, SCROLLBAR_HORIZONTAL);
			break;
		case OPENWIN_ADJUST_FOR_VERTICAL_SCROLLBAR:
			v = (Xv_opaque) STATUS(owin, adjust_vertical);
			break;
		case OPENWIN_ADJUST_FOR_HORIZONTAL_SCROLLBAR:
			v = (Xv_opaque) STATUS(owin, adjust_horizontal);
			break;
		case OPENWIN_VIEW_CLASS:
			v = (Xv_opaque)WINDOW;
			break;
		case OPENWIN_NTH_PW:
			view = openwin_nth_view(owin, va_arg(valist, int));

			if (view != NULL) v = view->pw;
			else v = XV_NULL;
			break;
		case OPENWIN_SELECTABLE:
			v = (Xv_opaque) STATUS(owin, selectable);
			break;
		case OPENWIN_PW_CLASS:
			v = XV_NULL;  /* subclasses ! */
			break;
		case OPENWIN_SEL_OWNER_CLASS:
			v = (Xv_opaque)SELECTION_OWNER;
			break;
		case OPENWIN_RESIZE_VERIFY_PROC:
			v = (Xv_opaque)owin->resize_verify_proc;
			break;
		case OPENWIN_NO_MARGIN:
			v = (Xv_opaque) STATUS(owin, no_margin);
			break;
		case OPENWIN_SELECTED_VIEW:
			if (owin->selected_view) v = VIEW_PUBLIC(owin->selected_view);
			else v = XV_NULL;

		case OPENWIN_SPLIT_INIT_PROC:
			v = (Xv_opaque) (owin->split_init_proc);
			break;
		case OPENWIN_SPLIT_DESTROY_PROC:
			v = (Xv_opaque) (owin->split_destroy_proc);
			break;
		default:
			xv_check_bad_attr(OPENWIN, attr);
			*get_status = XV_ERROR;
	}
	return (v);
}
static void openwin_remove_scrollbars(Openwin_view_info *view)
{
	Scrollbar vsb, hsb;

	vsb = openwin_sb(view, SCROLLBAR_VERTICAL);
	hsb = openwin_sb(view, SCROLLBAR_HORIZONTAL);

	if (vsb != XV_NULL) {
		view->sb[0] = XV_NULL;
		xv_destroy_status(vsb, DESTROY_CLEANUP);
	}
	if (hsb != XV_NULL) {
		view->sb[1] = XV_NULL;
		xv_destroy_status(hsb, DESTROY_CLEANUP);
	}
}

static void openwin_free_view(Openwin_view_info *view)
{
    openwin_remove_scrollbars(view);
    xv_destroy_status(VIEW_PUBLIC(view), DESTROY_CLEANUP);
}

static void openwin_destroy_views(Xv_openwin_info *owin)
{
    Openwin_view_info *view, *next_view;

    /* set that sb's being removed so remove code in layout */
    /* won't get called */
    STATUS_SET(owin, removing_scrollbars);

    for (view = owin->views; view != NULL; view = next_view) {
		next_view = view->next_view;
		openwin_free_view(view);
    }
}

/*
 * openwin_destroy - handle the cleanup and destruction of an openwin
 */
static int openwin_destroy(Openwin owin_public, Destroy_status destroy_status)
{
    Xv_openwin_info *owin = OPENWIN_PRIVATE(owin_public);

    if ((destroy_status == DESTROY_CLEANUP) ||
		(destroy_status == DESTROY_PROCESS_DEATH)) {
		/* unlink layout procs */
		xv_set(owin_public, WIN_LAYOUT_PROC, owin->layout_proc, NULL);
		if (owin->sel_owner) xv_destroy(owin->sel_owner);

		openwin_destroy_views(owin);

		if (owin->target_ptr) {
			Xv_cursor c = owin->target_ptr;

			owin->target_ptr = XV_NULL;

			/* I want to destroy this - no matter how many views have set it
			 * as their WIN_CURSOR (thereby incrementing the ref count)
			 */
			xv_set(c, XV_RESET_REF_COUNT, NULL);
			xv_destroy(c);
		}

		if (owin->resize_gc) XFreeGC((Display *)xv_get(owin_public, XV_DISPLAY),
													owin->resize_gc);
		if (owin->border_gc) XFreeGC((Display *)xv_get(owin_public, XV_DISPLAY),
													owin->border_gc);
		if (destroy_status == DESTROY_CLEANUP) free((char *) owin);
    }
    return XV_OK;
}

static int openwin_test_for_sb(Xv_openwin_info *owin, Scrollbar sb, Scrollbar_setting sb_direction, Openwin_view_info **view, int *last_sb)
{
	Scrollbar test_sb;
	Openwin_view_info *test_view;


	for (test_view = owin->views; test_view != NULL;
			test_view = test_view->next_view) {
		test_sb = openwin_sb(test_view, sb_direction);
		if (test_sb == sb) {
			*view = test_view;
		}
		else if (test_sb != XV_NULL) {
			*last_sb = FALSE;
		}
	}

	if (*view != NULL) {
		return (XV_OK);
	}
	else {
		return (XV_ERROR);
	}
}

static int openwin_viewdata_for_sb(Xv_openwin_info *owin, Scrollbar sb,
		Openwin_view_info **view, Scrollbar_setting *sb_direction, int *last_sb)
{

	/* look vertical first */
	*last_sb = TRUE;
	*sb_direction = SCROLLBAR_VERTICAL;
	*view = NULL;
	openwin_test_for_sb(owin, sb, *sb_direction, view, last_sb);

	if (*view != NULL) {
		/* found it */
		return (XV_OK);
	}
	*last_sb = TRUE;
	*sb_direction = SCROLLBAR_HORIZONTAL;
	*view = NULL;

	return (openwin_test_for_sb(owin, sb, *sb_direction, view, last_sb));
}

#ifdef OLD_FUNCTIONALITY
static void openwin_remove_split(Xv_openwin_info *owin, Openwin_view_info *view)
{
    openwin_unlink_view(owin, view);
    openwin_remove_scrollbars(view);
}
#endif /* OLD_FUNCTIONALITY */

/*
 * openwin_layout - position the views of the openwin
 */
static int openwin_layout(Openwin owin_public, Xv_Window child,
						Window_layout_op op, Xv_opaque d1, Xv_opaque d2,
						Xv_opaque d3, Xv_opaque d4, Xv_opaque d5)
{
	Xv_openwin_info *owin = OPENWIN_PRIVATE(owin_public);
	Openwin_view_info *view;
	Scrollbar_setting direction;
	int last;
	Rect r;

	switch (op) {
		case WIN_CREATE:
			/* Determine if child is a scrollbar. */
			if (xv_get(child, XV_IS_SUBTYPE_OF, SCROLLBAR)) {
				direction =
						(Scrollbar_setting) xv_get(child, SCROLLBAR_DIRECTION);
				xv_set(owin_public,
						direction ==
						SCROLLBAR_VERTICAL ? WIN_VERTICAL_SCROLLBAR :
						WIN_HORIZONTAL_SCROLLBAR, child, NULL);
			}
			break;

		case WIN_DESTROY:
			if (xv_get(child, XV_IS_SUBTYPE_OF, OPENWIN_VIEW)) {
#ifdef OLD_FUNCTIONALITY
				void (*destroy_proc)(Xv_window);

				/* since we have the class OPENWIN_VIEW, we never saw this
				 * case.
				 *
				 * The functionality was move from here to
				 * openwin_view_destroy and opewin_view_event (see the
				 * ACTION_SPLIT_DESTROY case.
				 * 
				 */
				view = VIEW_PRIVATE(child);

				destroy_proc = owin->split_destroy_proc;
				openwin_remove_split(owin, view);
				(void)openwin_fill_view_gap(owin, view);
				if (destroy_proc) {
					destroy_proc(owin_public);
				}
#else /* OLD_FUNCTIONALITY */
				xv_error(owin_public,
		     				ERROR_LAYER, ERROR_SYSTEM,
		     				ERROR_STRING, "Unexpected WIN_DESTROY case",
							ERROR_PKG, OPENWIN,
							NULL);
#endif /* OLD_FUNCTIONALITY */
			}
			else if (!STATUS(owin, removing_scrollbars)) {
				/* must look through data structures since can't */
				/* do a get on the sb to get information */
				if (openwin_viewdata_for_sb(owin, child, &view, &direction,
								&last) == XV_OK) {
					openwin_set_sb(view, direction, XV_NULL);
					/* only re-adjust if last view with sb */
					if (last) {
						if (direction == SCROLLBAR_VERTICAL) {
							STATUS_RESET(owin, adjust_vertical);
						}
						else {
							STATUS_RESET(owin, adjust_horizontal);
						}
						r = *(Rect *) xv_get(OPENWIN_PUBLIC(owin), WIN_RECT);
						openwin_adjust_views(owin, &r);
					}
				}
			}
			break;
		default:
			break;
	}

	if (owin->layout_proc != NULL) {
		return (owin->layout_proc(owin_public, child, op, d1, d2, d3, d4, d5));
	}
	else {
		return TRUE;
	}
}

const Xv_pkg xv_openwin_pkg = {
    "Open Window",		/* seal -> package name */
    (Attr_pkg) ATTR_PKG_OPENWIN,/* openwin attr */
    sizeof(Xv_openwin),		/* size of the openwin data struct */
    WINDOW,		/* pointer to parent */
    openwin_init,		/* init routine for openwin */
    openwin_set,		/* set routine */
    openwin_get,		/* get routine */
    openwin_destroy,		/* destroy routine */
    NULL			/* No find proc */
};


static int openwin_view_init(Openwin parent, Openwin_view slf,
									Attr_avlist avlist, int *unused)
{
	Xv_openwin_view *self = (Xv_openwin_view *)slf;
	Openwin_view_info *priv = very_transient_created;
	Xv_openwin_info *owin = OPENWIN_PRIVATE(parent);

	very_transient_created = NULL;

	priv->public_self = (Xv_opaque)self;
	self->private_data = (Xv_opaque)priv;

	priv->owin = owin;

	return XV_OK;
}

static Xv_opaque openwin_view_get(Openwin_view self, int *status,
									Attr_attribute attr, va_list valist)
{
	if (attr == OPENWIN_VIEW_PAINT_WINDOW) {
		Openwin_view_info *priv = VIEW_PRIVATE(self);

		*status = XV_OK;
		return priv->pw;
	}
	*status = XV_ERROR;
	return (Xv_opaque)XV_OK;
}

static int openwin_view_destroy(Openwin_view self, Destroy_status stat)
{
	if (stat == DESTROY_CLEANUP) {
		Openwin_view_info *priv = VIEW_PRIVATE(self);
		Scrollbar sb;

		if (priv->owin) {
			if (priv == priv->owin->selected_view) {
				xv_set(OPENWIN_PUBLIC(priv->owin),
						OPENWIN_SELECTED_VIEW, NULL,
						NULL);
				/* very careful: */
				priv->owin->selected_view = 0;
			}
		}
		openwin_unlink_view(priv->owin, priv);
		if (priv->sb[0]) {
			sb = priv->sb[0];
			priv->sb[0] = XV_NULL;

			xv_destroy_status(sb, DESTROY_CLEANUP);
		}
		if (priv->sb[1]) {
			sb = priv->sb[1];
			priv->sb[1] = XV_NULL;

			xv_destroy_status(sb, DESTROY_CLEANUP);
		}
		xv_free(priv);
	}
	return XV_OK;
}


const Xv_pkg xv_openwin_view_pkg = {
    "Open Window View",				/* seal -> package name */
    (Attr_pkg) ATTR_PKG_OPENWIN,	/* openwin attr */
    sizeof(Xv_openwin_view),		/* size of the openwin data struct */
    WINDOW,							/* pointer to parent */
    openwin_view_init,				/* init routine for openwin */
	NULL,
	openwin_view_get,
    openwin_view_destroy,			/* destroy routine */
    NULL							/* No find proc */
};
/********************** resize handle *************************/

typedef struct {
	Xv_window_struct parent_data;
	Xv_opaque private_data;
}  Xv_resize_handle;

typedef struct {
	Xv_opaque public_self;
	int appl_resizes;
	Openwin_resize_side	side;
} Resize_handle_private;

#define RHPRIV(_x_) XV_PRIVATE(Resize_handle_private, Xv_resize_handle, _x_)

static void paint_resize_handle(Xv_openwin_info *priv, Xv_window rh, int invoked)
{
	if (!priv->ginfo) return;

	olgx_draw_box(priv->ginfo, (Window)xv_get(rh, XV_XID),
			0, 0, (int)xv_get(rh, XV_WIDTH), (int)xv_get(rh, XV_HEIGHT),
			(invoked ? OLGX_INVOKED : OLGX_NORMAL) | OLGX_ERASE, TRUE);
}

static int calculate_frame_pos(Xv_openwin_info *priv, Event *ev)
{
	int pos;

	switch (priv->cur_resize) {
		case OPENWIN_TOP:
		case OPENWIN_BOTTOM:
			pos = (int)event_y(ev);
			break;
		default:
			pos = (int)event_x(ev);
			break;
	}
	return priv->frame_trans + pos;
}

static int own_verification(Xv_openwin_info *priv, int new_pos, Rect *swr)
{
	int min_sb = AbbScrollbar_Height(priv->ginfo);

	switch (priv->cur_resize) {
		case OPENWIN_TOP:
			if (new_pos > rect_bottom(swr) - min_sb) return XV_ERROR;
			break;
		case OPENWIN_BOTTOM:
			if (new_pos < swr->r_top + min_sb) return XV_ERROR;
			break;
		case OPENWIN_LEFT:
			if (new_pos > rect_right(swr) - min_sb) return XV_ERROR;
			break;
		case OPENWIN_RIGHT:
			if (new_pos < swr->r_left + min_sb) return XV_ERROR;
			break;
	}

	return XV_OK;
}

static int own_resize_drag_verifier(Openwin self, Rect *swr, int new_pos)
{
	Xv_openwin_info *priv = OPENWIN_PRIVATE(self);
	Xv_window ov, sv;
	Rect ovr;
	Scrollbar sb;
	int vdist = 0, hdist = 0;
	int min_sb = AbbScrollbar_Height(priv->ginfo);
	int viewdelta = priv->margin + 2;

	sv = VIEW_PUBLIC(priv->selected_view);
	sb = xv_get(self, OPENWIN_HORIZONTAL_SCROLLBAR, sv);
	if (sb) {
		vdist = (int)xv_get(sb, XV_HEIGHT) - 2;
	}
	vdist += viewdelta + 1;

	sb = xv_get(self, OPENWIN_VERTICAL_SCROLLBAR, sv);
	if (sb) {
		hdist = (int)xv_get(sb, XV_WIDTH) - 2;
	}
	hdist += viewdelta + 1;

	OPENWIN_EACH_VIEW(self, ov)
		ovr = *((Rect *)xv_get(ov, XV_RECT));
		ovr.r_top += (int)xv_get(self, XV_Y);
		ovr.r_left += (int)xv_get(self, XV_X);

		switch (priv->cur_resize) {
			case OPENWIN_TOP:
				/* we consider all views with the same top */
				if (ovr.r_top == swr->r_top) {
					if (new_pos > rect_bottom(&ovr) - min_sb)
						return XV_ERROR;
				}
				/* we consider all views whose bottom
				 * is my top
				 */
				if (rect_bottom(&ovr) + vdist == swr->r_top) {
					if (new_pos < ovr.r_top + min_sb + vdist)
						return XV_ERROR;
				}
				break;
			case OPENWIN_BOTTOM:
				/* we consider all views with the same bottom */
				if (rect_bottom(&ovr) == rect_bottom(swr)) {
					if (new_pos < ovr.r_top + min_sb)
						return XV_ERROR;
				}

				/* we consider all views whose top
				 * is my bottom
				 */
				if (rect_bottom(swr) + vdist == ovr.r_top) {
					if (new_pos > rect_bottom(&ovr) - min_sb - vdist)
						return XV_ERROR;
				}
				break;
			case OPENWIN_LEFT:
				/* we consider all views with the same left */
				if (ovr.r_left == swr->r_left) {
					if (new_pos > rect_right(&ovr)-min_sb)
						return XV_ERROR;
				}
				/* we consider all views whose right
				 * is my left
				 */
				if (rect_right(&ovr) + hdist == swr->r_left) {
					if (new_pos < ovr.r_left + min_sb + hdist)
						return XV_ERROR;
				}
				break;
			case OPENWIN_RIGHT:
				/* we consider all views with the same right */
				if (rect_right(&ovr) == rect_right(swr)) {
					if (new_pos < ovr.r_left + min_sb)
						return XV_ERROR;
				}

				/* we consider all views whose left
				 * is my right
				 */
				if (rect_right(swr) + hdist == ovr.r_left) {
					if (new_pos > rect_right(&ovr)-min_sb - hdist)
						return XV_ERROR;
				}
				break;
		}
	OPENWIN_END_EACH

	return XV_OK;
}

static int verify_drag(Xv_window rh, Xv_openwin_info *priv, Event *ev,
						int *new_posp)
{
	Rect swr;
	int verify_result;

	swr = *((Rect *)xv_get(VIEW_PUBLIC(priv->selected_view), XV_RECT));
	swr.r_top += (int)xv_get(OPENWIN_PUBLIC(priv), XV_Y);
	swr.r_left += (int)xv_get(OPENWIN_PUBLIC(priv), XV_X);

	*new_posp = calculate_frame_pos(priv, ev);
	/* perform own checks */
	verify_result = own_verification(priv, *new_posp, &swr);

	if (verify_result == XV_OK) {
		int appl_resizes = (int)xv_get(rh, RH_APPL_RESIZES);

		if (appl_resizes) {
			if (priv->resize_verify_proc) {
				verify_result = (*(priv->resize_verify_proc))(OPENWIN_PUBLIC(priv),
							priv->cur_resize,
							*new_posp,
							FALSE);
			}
		}
		else {
			verify_result = own_resize_drag_verifier(OPENWIN_PUBLIC(priv), &swr,
												*new_posp);
		}
	}

	return verify_result;
}

static void paint_resize_preview(Xv_openwin_info *priv)
{
	Frame fram = xv_get(OPENWIN_PUBLIC(priv), WIN_FRAME);
	Rect r;
	int sx, sy, ex, ey;

	if (! fram) return;

	r = *((Rect *)xv_get(OPENWIN_PUBLIC(priv), XV_RECT));
	switch (priv->cur_resize) {
		case OPENWIN_TOP:
		case OPENWIN_BOTTOM:
			sx = r.r_left;
			ex = rect_right(&r);
			sy = ey = priv->last_frame_pos;
			break;
		default:
			sy = r.r_top;
			ey = rect_bottom(&r);
			sx = ex = priv->last_frame_pos;
			break;
	}

	XDrawLine((Display *)xv_get(fram, XV_DISPLAY), (Window)xv_get(fram, XV_XID),
							priv->resize_gc, sx, sy, ex, ey);
}

Xv_public void openwin_update_layout(Openwin owin_public)
{
    Xv_openwin_info *owin = OPENWIN_PRIVATE(owin_public);
    Openwin_view_info *view;
	int vsb_w, hsb_h;
	int n_vmargins, n_hmargins;
	int border_width = 0;

	vsb_w = hsb_h = scrollbar_width_for_owner(owin_public);

	for (view = owin->views; view != NULL; view = view->next_view) {
		view->enclosing_rect = *((Rect *)xv_get(VIEW_PUBLIC(view), XV_RECT));

		if ((openwin_sb(view, SCROLLBAR_VERTICAL) != XV_NULL) ||
			STATUS(owin, adjust_vertical))
		{
			view->enclosing_rect.r_width += vsb_w;
		}

		if ((openwin_sb(view, SCROLLBAR_HORIZONTAL) != XV_NULL) ||
			STATUS(owin, adjust_horizontal))
		{
			view->enclosing_rect.r_height += hsb_h;
		}

		border_width = 0;

		/* set up margins */
		if (STATUS(owin, no_margin)) {
			n_vmargins = n_hmargins = 0;
		}
		else {
			n_vmargins = n_hmargins = 1;
		}

		/* get rid of margin if view is on one of the edges, or if there is
		 * a scrollbar
		 */
		if (view->right_edge ||
			(openwin_sb(view, SCROLLBAR_VERTICAL) != XV_NULL) ||
			STATUS(owin, adjust_vertical))
			n_vmargins = 0;
		if (view->bottom_edge ||
			(openwin_sb(view, SCROLLBAR_HORIZONTAL) != XV_NULL) ||
			STATUS(owin, adjust_horizontal))
			n_hmargins = 0;

		border_width = openwin_border_width(OPENWIN_PUBLIC(owin), VIEW_PUBLIC(view));

		view->enclosing_rect.r_width += n_vmargins * owin->margin + 2 * border_width;
		view->enclosing_rect.r_height += n_hmargins * owin->margin + 2 * border_width;
	}
}
static void resize_views(Openwin self)
{
	Xv_openwin_info *priv = OPENWIN_PRIVATE(self);
	Xv_window ov, sv;
	Rect svr, ovr;
	Scrollbar sb;
	int offset, vdist = 0, hdist = 0;
	int viewdelta = priv->margin + 2;

	/* ich kann das hier nicht als FRAME-Koordinate brauchen */
	offset = priv->last_frame_pos - priv->frame_trans;

	sv = VIEW_PUBLIC(priv->selected_view);
	sb = xv_get(self, OPENWIN_HORIZONTAL_SCROLLBAR, sv);
	if (sb) {
		vdist = (int)xv_get(sb, XV_HEIGHT) - 2; /* INCOMPLETE - 2 ???? */
	}
	vdist += viewdelta + 1;

	sb = xv_get(self, OPENWIN_VERTICAL_SCROLLBAR, sv);
	if (sb) {
		hdist = (int)xv_get(sb, XV_WIDTH) - 2; /* INCOMPLETE - 2 ???? */
	}
	hdist += viewdelta + 1;

	svr = *((Rect *)xv_get(sv, XV_RECT));

	OPENWIN_EACH_VIEW(self, ov)
		if (ov == sv) continue;

		ovr = *((Rect *)xv_get(ov, XV_RECT));

		switch (priv->cur_resize) {
			case OPENWIN_TOP:
				/* we consider all views with the same top */
				if (ovr.r_top == svr.r_top) {
					xv_set(ov,
							XV_Y, ovr.r_top + offset,
							XV_HEIGHT, ovr.r_height - offset,
							NULL);
					sb = xv_get(self, OPENWIN_VERTICAL_SCROLLBAR, ov);
					if (sb) {
						xv_set(sb,
							XV_Y, (int)xv_get(sb, XV_Y) + offset,
							XV_HEIGHT, (int)xv_get(sb, XV_HEIGHT) - offset,
							NULL);
						xv_set(sb,
							SCROLLBAR_VIEW_LENGTH,
								(ovr.r_height - offset) /
									(int)xv_get(sb, SCROLLBAR_PIXELS_PER_UNIT),
	       				NULL);
					}
				}
				/* we consider all views whose bottom
				 * is my top
				 */
				if (rect_bottom(&ovr) + vdist == svr.r_top) {
					xv_set(ov,
							XV_HEIGHT, ovr.r_height + offset,
							NULL);
					sb = xv_get(self, OPENWIN_VERTICAL_SCROLLBAR, ov);
					if (sb) {
						xv_set(sb,
							XV_HEIGHT, (int)xv_get(sb, XV_HEIGHT) + offset,
							NULL);
						xv_set(sb,
							SCROLLBAR_VIEW_LENGTH,
								(ovr.r_height + offset) /
									(int)xv_get(sb, SCROLLBAR_PIXELS_PER_UNIT),
	       				NULL);
					}
					sb = xv_get(self, OPENWIN_HORIZONTAL_SCROLLBAR, ov);
					if (sb) 
						xv_set(sb, XV_Y, (int)xv_get(sb, XV_Y) + offset, NULL);
				}
				break;
			case OPENWIN_BOTTOM:
				/* we consider all views with the same bottom */
				if (rect_bottom(&ovr) == rect_bottom(&svr)) {
					xv_set(ov,
							XV_HEIGHT, ovr.r_height + offset,
							NULL);

					sb = xv_get(self, OPENWIN_VERTICAL_SCROLLBAR, ov);
					if (sb) {
						xv_set(sb,
							XV_HEIGHT, (int)xv_get(sb, XV_HEIGHT) + offset,
							NULL);
						xv_set(sb,
							SCROLLBAR_VIEW_LENGTH,
								(ovr.r_height + offset) /
									(int)xv_get(sb, SCROLLBAR_PIXELS_PER_UNIT),
	       				NULL);
					}

					sb = xv_get(self, OPENWIN_HORIZONTAL_SCROLLBAR, ov);
					if (sb) 
						xv_set(sb, XV_Y, (int)xv_get(sb, XV_Y) + offset, NULL);
				}

				/* we consider all views whose top
				 * is my bottom
				 */
				if (rect_bottom(&svr) + vdist == ovr.r_top) {
					xv_set(ov,
							XV_Y, ovr.r_top + offset,
							XV_HEIGHT, ovr.r_height - offset,
							NULL);

					sb = xv_get(self, OPENWIN_VERTICAL_SCROLLBAR, ov);
					if (sb) {
						xv_set(sb,
							XV_Y, (int)xv_get(sb, XV_Y) + offset,
							XV_HEIGHT, (int)xv_get(sb, XV_HEIGHT) - offset,
							NULL);
						xv_set(sb,
							SCROLLBAR_VIEW_LENGTH,
								(ovr.r_height - offset) /
									(int)xv_get(sb, SCROLLBAR_PIXELS_PER_UNIT),
	       				NULL);
					}
				}
				break;
			case OPENWIN_LEFT:
				/* we consider all views with the same left */
				if (ovr.r_left == svr.r_left) {
					xv_set(ov,
							XV_X, ovr.r_left + offset,
							XV_WIDTH, ovr.r_width - offset,
							NULL);
					sb = xv_get(self, OPENWIN_HORIZONTAL_SCROLLBAR, ov);
					if (sb) {
						xv_set(sb,
							XV_X, (int)xv_get(sb, XV_X) + offset,
							XV_WIDTH, (int)xv_get(sb, XV_WIDTH) - offset,
							NULL);
						xv_set(sb,
							SCROLLBAR_VIEW_LENGTH,
								(ovr.r_width + offset) /
									(int)xv_get(sb, SCROLLBAR_PIXELS_PER_UNIT),
	       				NULL);
					}
				}
				/* we consider all views whose right
				 * is my left
				 */
				if (rect_right(&ovr) + hdist == svr.r_left) {
					xv_set(ov,
							XV_WIDTH, ovr.r_width + offset,
							NULL);
					sb = xv_get(self, OPENWIN_HORIZONTAL_SCROLLBAR, ov);
					if (sb) {
						xv_set(sb,
							XV_WIDTH, (int)xv_get(sb, XV_WIDTH) + offset,
							NULL);
						xv_set(sb,
							SCROLLBAR_VIEW_LENGTH,
								(ovr.r_width + offset) /
									(int)xv_get(sb, SCROLLBAR_PIXELS_PER_UNIT),
	       				NULL);
					}
					sb = xv_get(self, OPENWIN_VERTICAL_SCROLLBAR, ov);
					if (sb) 
						xv_set(sb, XV_X, (int)xv_get(sb, XV_X) + offset, NULL);
				}
				break;
			case OPENWIN_RIGHT:
				/* we consider all views with the same right */
				if (rect_right(&ovr) == rect_right(&svr)) {
					xv_set(ov,
							XV_WIDTH, ovr.r_width + offset,
							NULL);
					sb = xv_get(self, OPENWIN_HORIZONTAL_SCROLLBAR, ov);
					if (sb) {
						xv_set(sb,
							XV_WIDTH, (int)xv_get(sb, XV_WIDTH) + offset,
							NULL);
						xv_set(sb,
							SCROLLBAR_VIEW_LENGTH,
								(ovr.r_width + offset) /
									(int)xv_get(sb, SCROLLBAR_PIXELS_PER_UNIT),
	       				NULL);
					}
					sb = xv_get(self, OPENWIN_VERTICAL_SCROLLBAR, ov);
					if (sb) 
						xv_set(sb, XV_X, (int)xv_get(sb, XV_X) + offset, NULL);
				}

				/* we consider all views whose left
				 * is my right
				 */
				if (rect_right(&svr) + hdist == ovr.r_left) {
					xv_set(ov,
							XV_X, ovr.r_left + offset,
							XV_WIDTH, ovr.r_width - offset,
							NULL);
					sb = xv_get(self, OPENWIN_HORIZONTAL_SCROLLBAR, ov);
					if (sb) {
						xv_set(sb,
							XV_X, (int)xv_get(sb, XV_X) + offset,
							XV_WIDTH, (int)xv_get(sb, XV_WIDTH) - offset,
							NULL);
						xv_set(sb,
							SCROLLBAR_VIEW_LENGTH,
								(ovr.r_width - offset) /
									(int)xv_get(sb, SCROLLBAR_PIXELS_PER_UNIT),
	       				NULL);
					}
				}
				break;
		}
	OPENWIN_END_EACH

	/* now the selected view */
	switch (priv->cur_resize) {
		case OPENWIN_TOP:
			xv_set(sv,
					XV_Y, svr.r_top + offset,
					XV_HEIGHT, svr.r_height - offset,
					NULL);
			sb = xv_get(self, OPENWIN_VERTICAL_SCROLLBAR, sv);
			if (sb) {
				xv_set(sb,
					XV_Y, (int)xv_get(sb, XV_Y) + offset,
					XV_HEIGHT, (int)xv_get(sb, XV_HEIGHT) - offset,
					NULL);
				xv_set(sb,
					SCROLLBAR_VIEW_LENGTH,
							(svr.r_height - offset) /
								(int)xv_get(sb, SCROLLBAR_PIXELS_PER_UNIT),
       				NULL);
			}
			break;
		case OPENWIN_BOTTOM:
			xv_set(sv,
					XV_HEIGHT, svr.r_height + offset,
					NULL);

			sb = xv_get(self, OPENWIN_VERTICAL_SCROLLBAR, sv);
			if (sb) {
				xv_set(sb,
					XV_HEIGHT, (int)xv_get(sb, XV_HEIGHT) + offset,
					NULL);
				xv_set(sb,
					SCROLLBAR_VIEW_LENGTH,
							(svr.r_height + offset) /
								(int)xv_get(sb, SCROLLBAR_PIXELS_PER_UNIT),
       				NULL);
			}

			sb = xv_get(self, OPENWIN_HORIZONTAL_SCROLLBAR, sv);
			if (sb) 
				xv_set(sb, XV_Y, (int)xv_get(sb, XV_Y) + offset, NULL);
			break;
		case OPENWIN_LEFT:
			xv_set(sv,
					XV_X, svr.r_left + offset,
					XV_WIDTH, svr.r_width - offset,
					NULL);
			sb = xv_get(self, OPENWIN_HORIZONTAL_SCROLLBAR, sv);
			if (sb) {
				xv_set(sb,
					XV_X, (int)xv_get(sb, XV_X) + offset,
					XV_WIDTH, (int)xv_get(sb, XV_WIDTH) - offset,
					NULL);
				xv_set(sb,
					SCROLLBAR_VIEW_LENGTH,
							(svr.r_width - offset) /
								(int)xv_get(sb, SCROLLBAR_PIXELS_PER_UNIT),
       				NULL);
			}
			break;
		case OPENWIN_RIGHT:
			xv_set(sv,
					XV_WIDTH, svr.r_width + offset,
					NULL);
			sb = xv_get(self, OPENWIN_HORIZONTAL_SCROLLBAR, sv);
			if (sb) {
				xv_set(sb,
					XV_WIDTH, (int)xv_get(sb, XV_WIDTH) + offset,
					NULL);
				xv_set(sb,
					SCROLLBAR_VIEW_LENGTH,
							(svr.r_width + offset) /
								(int)xv_get(sb, SCROLLBAR_PIXELS_PER_UNIT),
       				NULL);
			}
			sb = xv_get(self, OPENWIN_VERTICAL_SCROLLBAR, sv);
			if (sb) 
				xv_set(sb, XV_X, (int)xv_get(sb, XV_X) + offset, NULL);
			break;
	}

	openwin_update_layout(self);
}

static Notify_value resizehandle_events(Xv_window rh, Notify_event event,
							Notify_arg arg, Notify_event_type type)
{
	Event *ev = (Event *)event;
	Openwin self = xv_get(rh, XV_OWNER);
	Xv_openwin_info *priv = OPENWIN_PRIVATE(self);
	int new_pos, verify_result;
	Rect swr, rhr;

	switch (event_action(ev)) {
		case WIN_REPAINT:
			paint_resize_handle(priv, rh, FALSE);
			break;

		case ACTION_SELECT:
			if (event_is_down(ev)) {

				paint_resize_handle(priv, rh, TRUE);
				priv->cur_resize = (Openwin_resize_side)xv_get(rh, RH_SIDE);

				swr = *((Rect *)xv_get(self, XV_RECT));
				rhr = *((Rect *)xv_get(rh, XV_RECT));

				switch (priv->cur_resize) {
					case OPENWIN_TOP:
						priv->frame_trans = swr.r_top + rhr.r_top -
											(int)event_y(ev);
						break;
					case OPENWIN_BOTTOM:
						priv->frame_trans = swr.r_top + rect_bottom(&rhr) -
											(int)event_y(ev);
						break;
					case OPENWIN_LEFT:
						priv->frame_trans = swr.r_left+rhr.r_left -
											(int)event_x(ev);
						break;
					case OPENWIN_RIGHT:
						priv->frame_trans = swr.r_left+rect_right(&rhr) -
											(int)event_x(ev);
						break;
				}

				verify_result = verify_drag(rh, priv, ev, &new_pos);

				if (verify_result == XV_OK) {
					priv->last_frame_pos = new_pos;
					paint_resize_preview(priv);
				}
			}
			else {
				Xv_window pw;
				int appl_resizes = (int)xv_get(rh, RH_APPL_RESIZES);

				paint_resize_preview(priv);
				paint_resize_handle(priv, rh, FALSE);
				if (appl_resizes) {
					if (priv->resize_verify_proc) {
						(void)(*(priv->resize_verify_proc))(self,
										priv->cur_resize,
										calculate_frame_pos(priv, ev),
										TRUE);
					}
				}
				else {
					resize_views(self);
				}

				if ((pw = priv->selected_view->pw)) {
					SERVERTRACE((500, "%s: %s: XClearArea\n", xv_app_name, __FUNCTION__));
					XClearArea((Display *)xv_get(pw, XV_DISPLAY),
							xv_get(pw, XV_XID), 0, 0, 0, 0, TRUE);
				}
			}
			break;

		case LOC_DRAG:
			verify_result = verify_drag(rh, priv, ev, &new_pos);

			if (verify_result == XV_OK) {
				paint_resize_preview(priv);
				priv->last_frame_pos = new_pos;
				paint_resize_preview(priv);
			}

			break;
		case ACTION_HELP:
			if (event_is_down(ev)) {
				xv_help_show(rh, "xview:resize_handles", ev);
			}
			break;
	}

	return notify_next_event_func(rh, event, arg, type);
}

static int resize_handle_init(Openwin parent, Resize_handle slf,
									Attr_avlist avlist, int *unused)
{
	Xv_resize_handle *self = (Xv_resize_handle *)slf;
	Resize_handle_private *priv = (Resize_handle_private *)xv_alloc(Resize_handle_private);
	Attr_attribute *attrs;

	priv->public_self = (Xv_opaque)self;
	self->private_data = (Xv_opaque)priv;


	for (attrs=avlist; *attrs; attrs=attr_next(attrs)) switch ((int)*attrs) {
		case RH_SIDE:
			priv->side = (Openwin_resize_side)attrs[1];
			ATTR_CONSUME(*attrs);
			break;
		default: break;
	}

	xv_set(slf,
			WIN_BORDER, FALSE,
			WIN_CONSUME_EVENTS, ACTION_HELP, WIN_MOUSE_BUTTONS, LOC_DRAG, NULL,
			WIN_NOTIFY_IMMEDIATE_EVENT_PROC, resizehandle_events,
			WIN_NOTIFY_SAFE_EVENT_PROC, resizehandle_events,
			XV_SHOW, FALSE,
			NULL);

	return XV_OK;
}

static Xv_opaque resize_handle_get(Resize_handle self, int *status,
									Attr_attribute attr, va_list valist)
{
	Resize_handle_private *priv = RHPRIV(self);

	*status = XV_OK;
	switch ((int)attr) {
		case RH_APPL_RESIZES: return (Xv_opaque)priv->appl_resizes;
		case RH_SIDE: return (Xv_opaque)priv->side;
		default:
			*status = xv_check_bad_attr(RESIZE_HANDLE, attr);
			return (Xv_opaque)XV_OK;
	}
}

static Xv_opaque resize_handle_set(Resize_handle self, Attr_avlist avlist)
{
	Resize_handle_private *priv = RHPRIV(self);
	Attr_attribute *attrs;

	for (attrs=avlist; *attrs; attrs=attr_next(attrs)) switch ((int)*attrs) {
		case RH_APPL_RESIZES:
			priv->appl_resizes = (int)attrs[1];
			ATTR_CONSUME(*attrs);
			break;
		case RH_SIDE:
			priv->side = (Openwin_resize_side)attrs[1];
			ATTR_CONSUME(*attrs);
			break;
		default: xv_check_bad_attr(RESIZE_HANDLE, attrs[0]);
			break;
	}

	return XV_OK;
}

static int resize_handle_destroy(Resize_handle self, Destroy_status stat)
{
	if (stat == DESTROY_CLEANUP) {
		Resize_handle_private *priv = RHPRIV(self);
		xv_free(priv);
	}
	return XV_OK;
}

static const Xv_pkg xv_resize_handle_pkg = {
    "ResizeHandle",
    (Attr_pkg) ATTR_PKG_OPENWIN,
    sizeof(Xv_resize_handle),
    WINDOW,
    resize_handle_init,
    resize_handle_set,
    resize_handle_get,
    resize_handle_destroy,
    NULL
};

static const Xv_pkg *resize_handle_class(void)
{
	return &xv_resize_handle_pkg;
}
