#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)sb.c 1.53 93/06/28 DRA: $Id: scrollbar.c,v 1.3 2025/03/06 17:36:51 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Module:	sb.c
 * 
 * 
 * Include files:
 */

#include <xview_private/sb_impl.h>
#include <xview_private/draw_impl.h>
#include <xview_private/win_info.h>
#include <xview_private/svr_impl.h>
#include <olgx/olgx.h>
#include <xview/canvas.h>
#include <xview/font.h>
#include <xview/openmenu.h>
#include <xview/win_notify.h>
#include <xview/openwin.h>
#include <xview/xv_error.h>
#include <xview/defaults.h>
#include <xview/svrimage.h>
#include <xview/termsw.h>
#include <xview/fullscreen.h>
#include <xview/help.h>

/* macros to convert variable from/to public/private form */
#define SCROLLBAR_PRIVATE(scrollbar)  \
	XV_PRIVATE(Xv_scrollbar_info, Xv_scrollbar, scrollbar)
#define SCROLLBAR_PUBLIC(scrollbar)   XV_PUBLIC(scrollbar)

#define SB_VERTICAL(sb) (sb->direction == SCROLLBAR_VERTICAL)

#define Max_offset(sb)	\
	((sb)->object_length - (sb)->view_length)

/* macro used when just the normalized elevator height is needed, and
 * sb_normalize_rect would be doing too much work.
 */
#define SB_NORMALIZED_ELEVATOR_HEIGHT(sb) \
  ((sb->direction == SCROLLBAR_VERTICAL) ? \
   sb->elevator_rect.r_height : sb->elevator_rect.r_width)

/* uninitialized lengths */
#define	SCROLLBAR_DEFAULT_LENGTH 	0
#define SCROLLBAR_CABLE_GAP		2

/* time in msec between repeats of single line scroll */
#define SCROLLBAR_REPEAT_DELAY 			100
#define SCROLLBAR_REPEAT_PAGE_INTERVAL	        100
#define SCROLLBAR_REPEAT_LINE_INTERVAL	        50

#define SCROLLBAR_POSITION_UNCHANGED 		-1

#define SB_ELEVATOR_INNER_TOP 			 1
#define SB_ELEVATOR_INNER_LEFT 			 1

/* dimensions for small size scrollbar */
#define SCROLLBAR_SMALL_THICKNESS		17
#define	SB_SMALL_MARGIN				 2
#define	SB_SMALL_MARKER_HEIGHT			 5
#define SB_SMALL_ELEVATOR_BOX_HEIGHT	 	13
#define SB_SMALL_FLINE_IMAGE_WIDTH		 6
#define SB_SMALL_FLINE_IMAGE_HEIGHT		 4
#define SB_SMALL_LINE_IMAGE_LEFT_MARGIN		 1
#define SB_SMALL_LINE_IMAGE_TOP_MARGIN		 3
#define SB_REDUCED_SMALL_LINE_IMAGE_TOP_MARGIN	 2

/* dimensions for regular size scrollbar */
#define SCROLLBAR_MEDIUM_THICKNESS		19
#define	SB_MEDIUM_MARGIN			 2
#define	SB_MEDIUM_MARKER_HEIGHT			 6
#define SB_MEDIUM_ELEVATOR_BOX_HEIGHT	 	15
#define SB_MEDIUM_FLINE_IMAGE_WIDTH		 8
#define SB_MEDIUM_FLINE_IMAGE_HEIGHT		 5
#define SB_MEDIUM_LINE_IMAGE_LEFT_MARGIN	 2
#define SB_MEDIUM_LINE_IMAGE_TOP_MARGIN		 4
#define SB_REDUCED_MEDIUM_LINE_IMAGE_TOP_MARGIN	 2

/* dimensions for large size scrollbar */
#define SCROLLBAR_LARGE_THICKNESS		23
#define	SB_LARGE_MARGIN				 3
#define	SB_LARGE_MARKER_HEIGHT			 7
#define SB_LARGE_ELEVATOR_BOX_HEIGHT	 	17
#define SB_LARGE_FLINE_IMAGE_WIDTH		 8
#define SB_LARGE_FLINE_IMAGE_HEIGHT		 5
#define SB_LARGE_LINE_IMAGE_LEFT_MARGIN		 2
#define SB_LARGE_LINE_IMAGE_TOP_MARGIN		 5
#define SB_REDUCED_LARGE_LINE_IMAGE_TOP_MARGIN	 3

/* dimensions for extralarge size scrollbar */
#ifndef SVR4
#define SCROLLBAR_XLARGE_THICKNESS		29
#else  /* SVR4 */
#define SCROLLBAR_XLARGE_THICKNESS		27
#endif  /* SVR4 */
#define	SB_XLARGE_MARGIN			 4
#define	SB_XLARGE_MARKER_HEIGHT			 9
#define SB_XLARGE_ELEVATOR_BOX_HEIGHT	 	21
#define SB_XLARGE_FLINE_IMAGE_WIDTH		12
#define SB_XLARGE_FLINE_IMAGE_HEIGHT		 7
#define SB_XLARGE_LINE_IMAGE_LEFT_MARGIN	 3
#define SB_XLARGE_LINE_IMAGE_TOP_MARGIN		 7
#define SB_REDUCED_XLARGE_LINE_IMAGE_TOP_MARGIN	 5

typedef enum {
    SCROLLBAR_FULL_SIZE,
    SCROLLBAR_ABBREVIATED,
    SCROLLBAR_MINIMUM
} Scrollbar_size;

/*
 * Typedefs:
 */

typedef struct scrollbar			Xv_scrollbar_info;
typedef void (*normalize_proc_t)(Scrollbar, long, Scroll_motion, long *);
typedef void (*compute_scroll_proc_t)(Scrollbar, int, int, Scroll_motion,
								long *, long *);

struct scrollbar {
    Scrollbar		public_self;		  /* Back pointer */
    int			creating;

    Scrollbar_setting	direction;		/* Scrollbar's orientation */
    int			can_split;
    int			inactive;
    unsigned long	last_view_start;
    Menu		menu;
	Menu_item  split_view_item, join_views_item;
	char have_join, have_split;
    Xv_opaque		managee;
    unsigned int	page_length; /* in units */
    unsigned long	object_length; /* in units */
    unsigned int	pixels_per_unit;
    unsigned int	view_length; /* in units */
    unsigned long	view_start; /* in units ??? */
    compute_scroll_proc_t compute_scroll_proc; /* Application supplied compute proc */
    normalize_proc_t normalize_proc;  /* Application supplied normalize proc */

    int		        jump_pointer;		/* boolean, adjust pointer? */
    int			multiclick_timeout;
    int                 drag_repaint_percent;   /* percent of dragging events*/
						/* to be passed on to */
						/* application as repaint */
						/* events */

    Event		transit_event;
    Scroll_motion	transit_motion;
    int			transit_occurred;
    struct timeval	last_select_time;  	/* timestamp of last ACTION_SELECT */

    Scrollbar_size	size;			/* full, abbreviated, or minimum */
    Frame_rescale_state	scale;
    Graphics_info       *ginfo;			/* OLGX graphics information */
    XID			window;                 /* the XID to paint into */
    Rect		elevator_rect;          /* Rectangle describing elevator location */
    int			elevator_state;		/* OLGX state */
    Rect		top_anchor_rect;	/* Geometry of the top anchor */
    int			top_anchor_inverted;    /* boolean */
    Rect		bottom_anchor_rect;	/* Geometry of the bottom anchor */
    int			bottom_anchor_inverted; /* boolean */
    int			cable_start;
    int			cable_height;
    int			painted;
    int			length;

    int			last_pos;		/* Used for cutting down on excess */
    int			last_prop_pos;		/* painting of the scrollbar */
    int			last_prop_length;
    int			last_state;
    Scroll_motion	last_motion;		/* keep track of scrollbar motion */
	int	show_page;
	int current_page;
	int page_height;
	Xv_window	page_window;    /* stored temporarily during dragging */
};

static	Attr_attribute	sb_context_key;

extern Graphics_info *xv_init_olgx(Xv_Window, int *, Xv_Font);
Xv_private Defaults_pairs xv_kbd_cmds_value_pairs[4];

static int	ignore_drag_count;
static int	ignore_drag_max;

#define BORDER_WIDTH			1

static Notify_value scrollbar_timed_out(Notify_client client, int which);
static int scrollbar_preview_split(Xv_scrollbar_info *sb,
    Event *event, Rect  *r, Event *completion_event);
/* static void scrollbar_handle_timed_page_event(Xv_scrollbar_info *sb, Scroll_motion motion); */
/* static void scrollbar_handle_timed_line_event(Xv_scrollbar_info *sb, Scroll_motion      motion); */

/*
 * additions for the delay time between repeat action events
 */
#define	SB_SET_FOR_LINE		1
#define	SB_SET_FOR_PAGE		2

/******************************************************************/

static unsigned long scrollbar_absolute_offset(Xv_scrollbar_info *sb, int pos, int length)
{
    if (length <= 0)
	return (0);
    else
	return (Max_offset(sb) * pos / length * sb->pixels_per_unit);
}

static int scrollbar_offset_to_client_units(Xv_scrollbar_info *sb, long unsigned pixel_offset, Scroll_motion motion, long *view_start)
{
    *view_start = sb->view_start;

    switch (motion) {
      case SCROLLBAR_ABSOLUTE:
      case SCROLLBAR_MIN_TO_POINT:
      case SCROLLBAR_TO_END:
      case SCROLLBAR_TO_START:
      case SCROLLBAR_LINE_FORWARD:
      case SCROLLBAR_LINE_BACKWARD:
	*view_start  = pixel_offset / sb->pixels_per_unit;
	break;

      case SCROLLBAR_POINT_TO_MIN:
	*view_start = ((pixel_offset % sb->pixels_per_unit) == 0) ? 
	                pixel_offset / sb->pixels_per_unit : 
		        pixel_offset / sb->pixels_per_unit + 1;
	break;

      case SCROLLBAR_PAGE_BACKWARD:
	if (sb->page_length != SCROLLBAR_DEFAULT_LENGTH && sb->page_length != 0) {

	    long unsigned pixels_per_page = sb->pixels_per_unit * sb->page_length;
	    long unsigned page = pixel_offset / pixels_per_page;
	    
	    /* If not on a page boundary round up */
	    if ( (page * pixels_per_page) != pixel_offset )
	      page++;
	    
	    *view_start = page * sb->page_length;
	} else {
	    *view_start = pixel_offset / sb->pixels_per_unit;
	}
	break;

      case SCROLLBAR_PAGE_FORWARD:
	if (sb->page_length != SCROLLBAR_DEFAULT_LENGTH && sb->page_length != 0) {

	    long unsigned pixels_per_page = sb->pixels_per_unit * sb->page_length;
	    long unsigned page = pixel_offset / pixels_per_page;
	    
	    *view_start = page * sb->page_length;
	} else {
	    *view_start = pixel_offset / sb->pixels_per_unit;
	}
	break;

      default:
	break;
    }

    if ( *view_start > Max_offset(sb) )
      *view_start = Max_offset(sb);

    return (XV_OK);
}

static int scrollbar_available_cable(Xv_scrollbar_info *sb)
{
	int available;

	available = sb->cable_height - sb->elevator_rect.r_height;
	if (available < 0)
		available = 0;
	return available;
}

static int scrollbar_scroll_to_offset(Xv_scrollbar_info *sb, long view_start)
{
	/* do bounds checking */
	if (sb->view_length > sb->object_length)
		view_start = 0;
	else if (view_start > sb->object_length) {
		view_start = sb->object_length;
	}
	else if (view_start < 0) { /* nonsense when this was still UNSIGNED long */
		view_start = 0;
	}
	if (view_start != sb->view_start) {

		sb->last_view_start = sb->view_start;
		sb->view_start = view_start;

		(void)win_post_id_and_arg(sb->managee, SCROLLBAR_REQUEST, NOTIFY_SAFE,
				SCROLLBAR_PUBLIC(sb),
				(Notify_copy) win_copy_event, (Notify_release) win_free_event);
		return (XV_OK);
	}
	else {
		return (SCROLLBAR_POSITION_UNCHANGED);
	}
}

/*
 * scrollbar_proportional_indicator - return the starting position, and length 
 *   of the scrollbar's cable proportional indicator.
 */
static void scrollbar_proportional_indicator(Xv_scrollbar_info *sb,
							int elev_pos,int *position,	int *length)
{
	int cable_size = scrollbar_available_cable(sb) - sb->cable_start;

	if (sb->size != SCROLLBAR_FULL_SIZE) {
		*position = 0;
		*length = 0;
	}
	else if ((sb->object_length == 0) || (sb->object_length <= sb->view_length)) {
		*position = sb->cable_start;
		*length = sb->cable_height - 2;
	}
	else {
		*length = sb->cable_height * sb->view_length / sb->object_length;
		if (*length > sb->cable_height - 2)
			*length = sb->cable_height - 2;

		if (*length > sb->elevator_rect.r_height &&
				elev_pos > sb->cable_start && cable_size > 0)
			*position = elev_pos - (elev_pos - sb->cable_start) *
					(*length - sb->elevator_rect.r_height) / cable_size;
		else
			*position = elev_pos;

		/* 
		 * If the prop indicator would be covered by the elevator,
		 * make the indicator a 3 point mark above (or below) the
		 * elevator
		 */
		if (*length < sb->elevator_rect.r_height) {
			if ((elev_pos - 4) >= sb->cable_start) {
				*position = elev_pos - 4;
				*length = 2 + sb->elevator_rect.r_height + 1;
			}
			else if ((elev_pos + sb->elevator_rect.r_height + 1) <=
					(sb->cable_start + sb->cable_height - 1)) {
				*position = elev_pos;
				*length = 2 + sb->elevator_rect.r_height;
			}
			else
				*length = 0;
		}
	}
}

static void scrollbar_paint_elevator_move(Xv_scrollbar_info *sb, int new_pos)
{
    int x, y, old_pos;
    int prop_pos, prop_length, state;
    int scroll_backward, scroll_forward;
    
    state = (sb->size == SCROLLBAR_FULL_SIZE) ? 
      (OLGX_NORMAL | OLGX_ERASE | OLGX_UPDATE) : OLGX_ABBREV;
    state |= sb->elevator_state | OLGX_ERASE;
    old_pos = sb->elevator_rect.r_top;
    
    if (sb->direction == SCROLLBAR_VERTICAL) {
	state |= OLGX_VERTICAL;
	x = sb->elevator_rect.r_left;
	if (sb->size != SCROLLBAR_FULL_SIZE)
	  y = sb->elevator_rect.r_top;
	else
	  y = 0;

    } else {
	state |= OLGX_HORIZONTAL;
	if (sb->size != SCROLLBAR_FULL_SIZE)
	  x = sb->elevator_rect.r_top;
	else
	  x = 0;
	y = sb->elevator_rect.r_left;
    }
    if (!(state & OLGX_INACTIVE) &&
	(!(state & (OLGX_SCROLL_BACKWARD | 
		    OLGX_SCROLL_FORWARD | 
		    OLGX_SCROLL_ABSOLUTE)))) {
	scroll_forward = (sb->view_start < Max_offset(sb));
	scroll_backward = (sb->view_start > 0);
	if (!scroll_forward && !scroll_backward)
	  state |= OLGX_INACTIVE;
	else if (!scroll_backward)
	  state |= OLGX_SCROLL_NO_BACKWARD;
	else if (!scroll_forward)
	  state |= OLGX_SCROLL_NO_FORWARD;
    }
    scrollbar_proportional_indicator(sb, new_pos, &prop_pos, &prop_length);
    
    if ((sb->last_pos != new_pos) || 
	(sb->last_prop_pos != prop_pos) ||
	(sb->last_prop_length != prop_length) ||
	(sb->last_state != state)) {
	olgx_draw_scrollbar(sb->ginfo, sb->window, x, y, sb->length,
			    new_pos, old_pos, prop_pos, prop_length, state);
	sb->last_pos = new_pos;
	sb->last_prop_pos = prop_pos;
	sb->last_prop_length = prop_length;
	sb->last_state = state;
	sb->elevator_rect.r_top = new_pos;
    }
}	    

static void scrollbar_position_elevator(Xv_scrollbar_info *sb, int paint,
							Scroll_motion motion)
{
	int available_cable;
	int new_top;

	available_cable = scrollbar_available_cable(sb);
	/* Bounds checking */
	if (sb->view_start > Max_offset(sb)) {
		sb->view_start = Max_offset(sb);
	}
	if ((sb->size == SCROLLBAR_FULL_SIZE) && (motion != SCROLLBAR_ABSOLUTE)) {
		if (sb->view_start == 0 || sb->object_length <= sb->view_length) {
			/* At beginning */
			new_top = sb->cable_start;
		}
		else {
			new_top = (double)sb->view_start * (double)available_cable /
										(double)Max_offset(sb);
			if (new_top < 3) {
				/* Not at beginning, leave anchor a little */
				new_top = (available_cable < 3) ? available_cable : 3;
			}
			else if (sb->view_start < Max_offset(sb)
					&& new_top > available_cable - 3 && available_cable > 3) {
				/* Not at end, leave anchor a little */
				new_top = available_cable - 3;
			}
			new_top += sb->cable_start;
		}
	}
	else
		/*
		 * User has positioned (e.g., dragged) the elevator to a specific
		 * point.  We don't want to jump the elevator to the "true" position
		 * (to indicate the view start) because this is poor user interface.
		 */
		new_top = sb->elevator_rect.r_top;

	if (paint)
		scrollbar_paint_elevator_move(sb, new_top);
	else
		sb->elevator_rect.r_top = new_top;
}

static int scrollbar_scroll(Xv_scrollbar_info *sb,int pos, Scroll_motion motion)
{
	long voffset = 0;
	long objlen, vstart = 0;
	int result = SCROLLBAR_POSITION_UNCHANGED;
	int available_cable;

	if (motion == SCROLLBAR_NONE)
		return (result);

	/* translate position into client space */
	available_cable = scrollbar_available_cable(sb);

	if (sb->compute_scroll_proc != NULL) {
		/* give the application the chance to set SCROLLBAR_PAGE,
		 * otherwise we calculate the page number, see (klbwefhlkhwregf)
		 */
		sb->current_page = 0;
		sb->compute_scroll_proc(SCROLLBAR_PUBLIC(sb), pos, available_cable,
				motion, &voffset, &objlen);
		sb->object_length = objlen;
	}
	if (sb->normalize_proc != NULL) {
		sb->normalize_proc(SCROLLBAR_PUBLIC(sb), voffset, motion, &vstart);
	}
	else {
		vstart = voffset;
	}

	if (vstart != sb->view_start) {
		result = scrollbar_scroll_to_offset(sb, vstart);
	}
	scrollbar_position_elevator(sb, sb->painted, motion);

	return (result);
}

static int sb_elevator_height(Xv_scrollbar_info *sb, Scrollbar_size size)
{
	if (size != SCROLLBAR_FULL_SIZE) {
		return AbbScrollbar_Height(sb->ginfo);
	}

	return ScrollbarElevator_Height(sb->ginfo);
}

static int sb_margin(Xv_scrollbar_info *sb)
{
	switch (sb->scale) {
		case WIN_SCALE_SMALL:
			return SB_SMALL_MARGIN;
		case WIN_SCALE_MEDIUM:
		default:
			return SB_MEDIUM_MARGIN;
		case WIN_SCALE_LARGE:
			return SB_LARGE_MARGIN;
		case WIN_SCALE_EXTRALARGE:
			return SB_XLARGE_MARGIN;
	}
}

/*
 * scrollbar_top_anchor_rect - return the normalized rect of the top anchor
 */
static void scrollbar_top_anchor_rect(Xv_scrollbar_info *sb, Rect *r)	/* RETURN VALUE */
{
	r->r_left = sb_margin(sb);
	r->r_width = Vertsb_Endbox_Width(sb->ginfo);
	r->r_height = Vertsb_Endbox_Height(sb->ginfo);
	if (sb->size != SCROLLBAR_FULL_SIZE)
		r->r_top = sb->elevator_rect.r_top - SCROLLBAR_CABLE_GAP - r->r_height;
	else
		r->r_top = 0;
}

/*
 * scrollbar_bottom_anchor_rect - return the normalized rect of the bottom anchor
 */
static void scrollbar_bottom_anchor_rect(Xv_scrollbar_info *sb, Rect *r)	/* RETURN VALUE */
{
	r->r_left = sb_margin(sb);
	if (sb->size == SCROLLBAR_FULL_SIZE)
		r->r_top = sb->length - Vertsb_Endbox_Height(sb->ginfo);
	else
		r->r_top = rect_bottom(&sb->elevator_rect) + 1 + SCROLLBAR_CABLE_GAP;
	r->r_width = Vertsb_Endbox_Width(sb->ginfo);
	r->r_height = Vertsb_Endbox_Height(sb->ginfo);
}

static void sb_abbreviated(Xv_scrollbar_info *sb)
{
	sb->size = SCROLLBAR_ABBREVIATED;
	sb->elevator_rect.r_height = sb_elevator_height(sb, sb->size);
	if ((sb->elevator_rect.r_top = (sb->length / 2) -
					(sb->elevator_rect.r_height / 2)) < 0)
		sb->elevator_rect.r_top = 0;
	sb->length = sb->elevator_rect.r_height;
	scrollbar_top_anchor_rect(sb, &sb->top_anchor_rect);
	scrollbar_bottom_anchor_rect(sb, &sb->bottom_anchor_rect);
}

static void sb_minimum(Xv_scrollbar_info *sb)
{
    sb_abbreviated(sb);
    sb->size = SCROLLBAR_MINIMUM;
}

static void sb_full_size(Xv_scrollbar_info *sb)
{
    sb->size = SCROLLBAR_FULL_SIZE;
    sb->elevator_rect.r_height = sb_elevator_height(sb, sb->size);
    scrollbar_top_anchor_rect(sb, &sb->top_anchor_rect);
    scrollbar_bottom_anchor_rect(sb, &sb->bottom_anchor_rect);
}

static void sb_normalize_rect(Xv_scrollbar_info *sb, Rect *r)
{
	int temp;

	if (!SB_VERTICAL(sb)) {
		temp = r->r_top;
		r->r_top = r->r_left;
		r->r_left = temp;
		temp = r->r_width;
		r->r_width = r->r_height;
		r->r_height = temp;
	}
}

static int sb_marker_height(Xv_scrollbar_info *sb)
{
	int height;

	if (sb->ginfo != (Graphics_info *) NULL)
		height = Vertsb_Endbox_Height(sb->ginfo);
	else
		switch (sb->scale) {
			case WIN_SCALE_SMALL:
				height = SB_SMALL_MARKER_HEIGHT;
			case WIN_SCALE_MEDIUM:
			default:
				height = SB_MEDIUM_MARKER_HEIGHT;
			case WIN_SCALE_LARGE:
				height = SB_LARGE_MARKER_HEIGHT;
			case WIN_SCALE_EXTRALARGE:
				height = SB_XLARGE_MARKER_HEIGHT;
		}
	return (height);
}

static void sb_resize(Xv_scrollbar_info *sb)
{
	Rect r;
	int anchors_height;

	r = *(Rect *) xv_get(SCROLLBAR_PUBLIC(sb), WIN_RECT);
	sb_normalize_rect(sb, &r);
	sb->length = r.r_height;

	anchors_height = 2 * (sb_marker_height(sb) + SCROLLBAR_CABLE_GAP);
	sb->cable_height = r.r_height - anchors_height;

	if ((anchors_height + sb_elevator_height(sb, SCROLLBAR_ABBREVIATED))
				> sb->length)
		sb_minimum(sb);
	else if (sb->cable_height <= sb_elevator_height(sb, SCROLLBAR_FULL_SIZE))
		sb_abbreviated(sb);
	else
		sb_full_size(sb);
}

static void scrollbar_init_positions(Xv_scrollbar_info *sb)
{
	sb->scale = (Frame_rescale_state)xv_get(xv_get(SCROLLBAR_PUBLIC(sb),
											XV_FONT), FONT_SCALE);

	sb_resize(sb);

	if (sb->object_length == SCROLLBAR_DEFAULT_LENGTH)
	  sb->object_length = sb->length / sb->pixels_per_unit;
	if (sb->view_length == SCROLLBAR_DEFAULT_LENGTH)
	  sb->view_length = sb->length / sb->pixels_per_unit;
	
	sb->cable_start = sb_marker_height(sb) + SCROLLBAR_CABLE_GAP;
	sb->cable_height = sb->length - 2 * (sb_marker_height(sb) + SCROLLBAR_CABLE_GAP);

	/* initialize elevator rect */
	if (sb->size == SCROLLBAR_FULL_SIZE)
	  sb->elevator_rect.r_top = sb->cable_start;
	sb->elevator_rect.r_left = sb_margin(sb);
	sb->elevator_rect.r_width = ScrollbarElevator_Width(sb->ginfo);

	xv_set(SCROLLBAR_PUBLIC(sb),
	       (SB_VERTICAL(sb) ? WIN_WIDTH : WIN_HEIGHT),
	           scrollbar_width_for_scale(sb->scale),
	       NULL);
}

static void scrollbar_absolute_position_elevator(Xv_scrollbar_info *sb,
												int pos)
{
	int available_cable = scrollbar_available_cable(sb);

	if (pos < 0 || available_cable <= 0) {
		pos = 0;
	}
	else if (pos > available_cable) {
		pos = available_cable;
	}
	/* add back in cable_start offset */
	pos += sb->cable_start;

	scrollbar_paint_elevator_move(sb, pos);
}


static void sb_rescale(Xv_scrollbar_info *sb, Frame_rescale_state scale)
{
	SERVERTRACE((790, "%s: scale=%d\n", __FUNCTION__, scale));
	if (sb->scale != scale) {
		Scrollbar scrollbar = SCROLLBAR_PUBLIC(sb);
		Xv_font old_font, new_font;
		Xv_screen scr = XV_SCREEN_FROM_WINDOW(scrollbar);
		int three_d;

		old_font = xv_get(scrollbar, XV_FONT);
		new_font = old_font ? xv_find(scrollbar, FONT,
								FONT_RESCALE_OF, old_font, (int)scale,
								NULL)
							: (Xv_Font) 0;
		if (new_font) {
			(void)xv_set(old_font, XV_INCREMENT_REF_COUNT, NULL);
			(void)xv_set(scrollbar, XV_FONT, new_font, NULL);
		}

		three_d = (SCREEN_UIS_3D_COLOR ==
							(screen_ui_style_t)xv_get(scr, SCREEN_UI_STYLE));
		sb->scale = scale;
		sb->ginfo = xv_init_olgx(scrollbar, &three_d, new_font);
		if (sb->size == SCROLLBAR_ABBREVIATED) {
			sb_abbreviated(sb);
		}
		else {
			sb_full_size(sb);
		}
	}
}

/* 
 * scrollbar_line_backward_rect - compute the normalized backward scroll rect
 */
static void scrollbar_line_backward_rect(Xv_scrollbar_info *sb, Rect *r)	/* RETURN VALUE */
{
	r->r_left = sb->elevator_rect.r_left;
	r->r_width = sb->elevator_rect.r_width;
	r->r_top = sb->elevator_rect.r_top;
	if (sb->size != SCROLLBAR_ABBREVIATED)
		r->r_height = sb->elevator_rect.r_height / 3;
	else
		r->r_height = sb->elevator_rect.r_height / 2;
}

#ifdef SEEMS_UNUSED
/* 
 * scrollbar_absolute_rect - compute the normalized absolute scroll rect
 */
static void scrollbar_absolute_rect(Xv_scrollbar_info *sb, Rect *r)	/* RETURN VALUE */
{
	r->r_left =   sb->elevator_rect.r_left;
	r->r_width =  sb->elevator_rect.r_width;
	r->r_top =    sb->elevator_rect.r_top + (sb->elevator_rect.r_height / 3);
	r->r_height = sb->elevator_rect.r_height / 3;
}
#endif /* SEEMS_UNUSED */

/* 
 * scrollbar_line_forward_rect - compute the normalized forward scroll rect
 */
static void scrollbar_line_forward_rect(Xv_scrollbar_info *sb, Rect *r)
/* RETURN VALUE */
{
	r->r_left = sb->elevator_rect.r_left;
	r->r_width = sb->elevator_rect.r_width;
	if (sb->size != SCROLLBAR_ABBREVIATED) {
		r->r_top = sb->elevator_rect.r_top
						+ (sb->elevator_rect.r_height / 3) * 2;
		r->r_height = sb->elevator_rect.r_height / 3;
	}
	else {
		r->r_top = sb->elevator_rect.r_top + (sb->elevator_rect.r_height / 2);
		r->r_height = sb->elevator_rect.r_height / 2;
	}
}

Xv_private int scrollbar_width_for_scale(Frame_rescale_state scale)
{
	switch (scale) {
		case WIN_SCALE_SMALL:
			return SCROLLBAR_SMALL_THICKNESS;
		case WIN_SCALE_MEDIUM:
			return SCROLLBAR_MEDIUM_THICKNESS;
		case WIN_SCALE_LARGE:
			return SCROLLBAR_LARGE_THICKNESS;
		case WIN_SCALE_EXTRALARGE:
			return SCROLLBAR_XLARGE_THICKNESS;
		default:
			return SCROLLBAR_MEDIUM_THICKNESS;
	}
}

Xv_private int scrollbar_width_for_owner(Xv_window owner)
{
	Xv_font font;

	if (xv_get(owner, WIN_IS_CLIENT_PANE)) {
		font = xv_get(xv_get(owner, WIN_FRAME), XV_FONT);
	}
	else {
		font = xv_get(owner, XV_FONT);
	}
	return scrollbar_width_for_scale((int)xv_get(font, FONT_SCALE));
}

Xv_private int scrollbar_width(Scrollbar sb)
{
#ifdef DAS_WAR_ZU_SCHMAL
    Xv_scrollbar_info *priv = SCROLLBAR_PRIVATE(sb);
	return  ScrollbarElevator_Width(priv->ginfo);
#else
	return scrollbar_width_for_owner(xv_get(sb, XV_OWNER));
#endif
}

Xv_private int scrollbar_minimum_size(Scrollbar sb_public)
{
    Xv_scrollbar_info *sb = SCROLLBAR_PRIVATE(sb_public);

    return sb_elevator_height(sb, SCROLLBAR_MINIMUM);
}
static void scrollbar_paint_anchor(Xv_scrollbar_info *sb, Rect *r, int invoked)
{
    sb_normalize_rect(sb, r);
    olgx_draw_box(sb->ginfo, sb->window,
		  r->r_left, r->r_top, r->r_width, r->r_height,
		  invoked | OLGX_ERASE, TRUE);
    sb_normalize_rect(sb, r);
}


static void scrollbar_paint_elevator(Xv_scrollbar_info *sb)
{
    scrollbar_paint_elevator_move(sb, sb->elevator_rect.r_top);
}

static void scrollbar_invert_region(Xv_scrollbar_info *sb, Scroll_motion motion)
{
	int state;

	switch (motion) {
		case SCROLLBAR_TO_START:
			state = sb->top_anchor_inverted = !(sb->top_anchor_inverted);
			scrollbar_paint_anchor(sb, &sb->top_anchor_rect, state);
			break;
		case SCROLLBAR_TO_END:
			state = sb->bottom_anchor_inverted = !(sb->bottom_anchor_inverted);
			scrollbar_paint_anchor(sb, &sb->bottom_anchor_rect, state);
			break;
		case SCROLLBAR_LINE_FORWARD:
			sb->elevator_state =
					(sb->elevator_state ==
					OLGX_SCROLL_FORWARD) ? 0 : OLGX_SCROLL_FORWARD;
			scrollbar_paint_elevator(sb);
			break;
		case SCROLLBAR_LINE_BACKWARD:
			sb->elevator_state =
					(sb->elevator_state ==
					OLGX_SCROLL_BACKWARD) ? 0 : OLGX_SCROLL_BACKWARD;
			scrollbar_paint_elevator(sb);
			break;
		case SCROLLBAR_ABSOLUTE:
			sb->elevator_state =
					(sb->elevator_state ==
					OLGX_SCROLL_ABSOLUTE) ? 0 : OLGX_SCROLL_ABSOLUTE;
			scrollbar_paint_elevator(sb);
			break;
		default:
			break;
	}
}
typedef struct {
    Graphics_info *ginfo;
	int root_x, root_y;
	int digitwidth;
	int pagenumber, pn_digits;
	char c_pn[20];
	int height, width;
	int x_offset, ascent, hmargin, vmargin, visible, yoff;
} page_data_t;

static void pagewin_paint(Window xid, page_data_t *pn)
{
	if (Dimension(pn->ginfo) == OLGX_3D_COLOR) {
		olgx_draw_box(pn->ginfo, xid, 0, 0,
					pn->width, pn->height, OLGX_NORMAL, FALSE);
	}
	olgx_draw_text(pn->ginfo, xid, pn->c_pn, pn->hmargin,
					pn->vmargin + Ascent_of_TextFont(pn->ginfo),
					0, OLGX_NORMAL);
}

static void pw_events(Xv_window win, Event *ev)
{
	if (event_action(ev) == WIN_REPAINT) {
		pagewin_paint((Window)xv_get(win, XV_XID),
						(page_data_t *)xv_get(win, WIN_CLIENT_DATA));
	}
}

static void get_page_window(Xv_scrollbar_info *sb)
{
	Xv_screen screen;
	page_data_t *pn;

	if (sb->page_window) return;

	screen = XV_SCREEN_FROM_WINDOW(SCROLLBAR_PUBLIC(sb));

	sb->page_window = xv_get(screen, XV_KEY_DATA, SCROLLBAR_SHOW_PAGE);
	if (sb->page_window) return;

	pn = (page_data_t *)xv_calloc(1, (unsigned)sizeof(page_data_t));
	/* wenn man das so macht:
	 * pn->x_offset = 3 - ((Dimension(sb->ginfo) == OLGX_2D) ? 2 : 0);
	 * ergeben sich (im 2D-Fall) Verschiebungen zwischen dem Scrollbar und
	 * dem page_window und zwar in Y-Richtung !!!!! Voellig obskur...
	 */
	pn->x_offset = 3;
	pn->hmargin = 6;
	pn->vmargin = 1;
    pn->ginfo = sb->ginfo;

	pn->yoff = AbbScrollbar_Height(pn->ginfo) / 2;
	pn->digitwidth = XTextWidth(TextFont_Struct(pn->ginfo), "0", 1);
	pn->height = Ascent_of_TextFont(pn->ginfo) +
								Descent_of_TextFont(pn->ginfo) +
								2 * pn->vmargin;

	sb->page_window = xv_create(xv_get(SCROLLBAR_PUBLIC(sb), XV_ROOT), WINDOW,
					XV_HEIGHT, pn->height,
					XV_WIDTH, 2,
					WIN_BORDER, (Dimension(pn->ginfo) != OLGX_3D_COLOR),
					WIN_CMS, xv_get(SCROLLBAR_PUBLIC(sb), WIN_CMS),
					WIN_CONSUME_EVENTS, WIN_REPAINT, NULL,
					WIN_EVENT_PROC, pw_events,
					WIN_SAVE_UNDER, TRUE,
					WIN_TOP_LEVEL_NO_DECOR, TRUE,
					WIN_CLIENT_DATA, pn,
					XV_SHOW, FALSE,
					NULL);

	xv_set(screen, XV_KEY_DATA, SCROLLBAR_SHOW_PAGE, sb->page_window, NULL);
}

static void update_pagewin(Xv_scrollbar_info *sb)
{
	Display *dpy;
	page_data_t *pn;
	int new_pn_digits, need_paint, i = 0;
	unsigned int new_pagenumber;
	Attr_attribute a[20];

	if (! sb->page_window) return;

	dpy = (Display *)xv_get(sb->page_window, XV_DISPLAY);
	pn = (page_data_t *)xv_get(sb->page_window, WIN_CLIENT_DATA);

	if (! pn->visible) {
		pn->visible = TRUE;
		a[i++] = XV_SHOW;
		a[i++] = TRUE;
	}

	/* Ref (klbwefhlkhwregf) */
	if (sb->current_page == 0) { /* appl compute_scroll_proc set nothing */
		unsigned int line = sb->view_start;

		/* view_start ist  nicht immer eine Zeilenzahl -
		 * zumindest bei TEXTSW und TERMSW ist es eine 'Pixelzahl', weil
		 * dort SCROLLBAR_PIXELS_PER_UNIT = 1 setzen.
		 * Das ist ein konzeptioneller Mist, denn jetzt habe ich keine
		 * Möglichkeit, die Zeilenzahl zu bestimmen.
		 * Dagegen wird page_height = SCROLLBAR_PAGE_HEIGHT immer
		 * als Zahl von Zeilen verstanden. D.h. man braucht bei
		 * TEXTSW und TERMSW immer eine compute_scroll_proc, die
		 * current_page = SCROLLBAR_PAGE setzt.
		 */

		new_pagenumber = 1 + line / sb->page_height;
	}
	else if (sb->current_page < 0) {
		/* interprete negative 'current_page' as 'line number' */
		new_pagenumber = 1 + (-sb->current_page / sb->page_height);
	}
	else {
		new_pagenumber = sb->current_page;
	}

	sprintf(pn->c_pn, "%u", new_pagenumber);
	new_pn_digits = strlen(pn->c_pn);
	if (new_pn_digits != pn->pn_digits) {
		pn->width = pn->digitwidth * new_pn_digits + 2 * pn->hmargin;
		a[i++] = XV_WIDTH;
		a[i++] = pn->width;
		a[i++] = XV_X;
		a[i++] = pn->root_x - pn->x_offset - pn->width;
	}
	need_paint = (pn->pagenumber != new_pagenumber);
	pn->pagenumber = new_pagenumber;
	pn->pn_digits = new_pn_digits;

	a[i++] = XV_Y;
	a[i++] = pn->root_y + pn->yoff + sb->elevator_rect.r_top;
	a[i++] = 0L;
	xv_set_avlist(sb->page_window, a);
	if (need_paint) {
		XClearWindow(dpy, (Window)xv_get(sb->page_window, XV_XID));
		pagewin_paint((Window)xv_get(sb->page_window, XV_XID), pn);
	}
	XFlush(dpy);
}

static void scrollbar_position_mouse(Xv_scrollbar_info *sb, int x, int y)
{
	Xv_Window win = SCROLLBAR_PUBLIC(sb);
	Rect *mouse_rect;
	Rect *sb_rect;

	mouse_rect = (Rect *) xv_get(win, WIN_MOUSE_XY, NULL);
	sb_rect = (Rect *) xv_get(win, XV_RECT, NULL);

	/* move mouse if it is still in the scrollbar window */
	if (mouse_rect->r_left >= 0 &&
			mouse_rect->r_left < sb_rect->r_width &&
			mouse_rect->r_top >= 0 && mouse_rect->r_top < sb_rect->r_height)
	{
		if (SB_VERTICAL(sb))
			xv_set(win, WIN_MOUSE_XY, x, y, NULL);
		else
			xv_set(win, WIN_MOUSE_XY, y, x, NULL);
	}
}

static void scrollbar_handle_timed_line_event(Xv_scrollbar_info *sb, Scroll_motion      motion)
{
	int x, y;
	Rect active_region;

	if ((scrollbar_scroll(sb, 0, motion) != SCROLLBAR_POSITION_UNCHANGED) &&
			(sb->jump_pointer)) {
		/* reposition mouse if necessary */
		if (motion == SCROLLBAR_LINE_FORWARD)
			scrollbar_line_forward_rect(sb, &active_region);
		else
			scrollbar_line_backward_rect(sb, &active_region);

		x = active_region.r_left + (active_region.r_width / 2);
		y = active_region.r_top + (active_region.r_height / 2);
		scrollbar_position_mouse(sb, x, y);
	}
}

static void scrollbar_timer_start(Scrollbar scrollbar, int actiontype)
{
	struct itimerval timer;
	int interval;

	/* Call timeout routine using pre-set times according to actiontype */
	if (actiontype == SB_SET_FOR_LINE) {
		interval = defaults_get_integer_check("scrollbar.lineInterval",
			"Scrollbar.LineInterval", SCROLLBAR_REPEAT_LINE_INTERVAL, 0, 999);
	}
	else {
		interval = defaults_get_integer_check("scrollbar.pageInterval",
			"Scrollbar.PageInterval", SCROLLBAR_REPEAT_PAGE_INTERVAL, 0, 999);
	}

	timer.it_value.tv_usec = 1000
					* defaults_get_integer_check("scrollbar.repeatDelay",
							"Scrollbar.RepeatDelay", SCROLLBAR_REPEAT_DELAY,
							0, 999);
	timer.it_value.tv_sec = 0;

	/* next time call in `interval' msec         */
	timer.it_interval.tv_usec = interval * 1000;
	timer.it_interval.tv_sec = 0;

	(void)notify_set_itimer_func((Notify_client) scrollbar,
			scrollbar_timed_out, ITIMER_REAL, &timer, (struct itimerval *)NULL);
}

static void scrollbar_timer_stop(Scrollbar scrollbar)
{
	(void)notify_set_itimer_func((Notify_client) scrollbar,
			scrollbar_timed_out,
			ITIMER_REAL, (struct itimerval *)NULL, (struct itimerval *)NULL);
}


static int scrollbar_handle_elevator_event(Xv_scrollbar_info *sb,
									Event *event, Scroll_motion motion)
{
	int y, x, transit_y;

	switch (event_action(event)) {
		case ACTION_SELECT:
			if (event_is_down(event)) {
				y = SB_VERTICAL(sb) ? event_y(event) : event_x(event);
				scrollbar_invert_region(sb, motion);
				switch (motion) {
					case SCROLLBAR_LINE_FORWARD:
					case SCROLLBAR_LINE_BACKWARD:
						scrollbar_timer_start(SCROLLBAR_PUBLIC(sb),
								SB_SET_FOR_LINE);
						break;
					case SCROLLBAR_ABSOLUTE:
						if (sb->drag_repaint_percent)
							ignore_drag_max = 100 / sb->drag_repaint_percent;
						ignore_drag_count = 1;

						if (sb->show_page) {
							page_data_t *pn;

							get_page_window(sb);
							pn = (page_data_t *) xv_get(sb->page_window,
									WIN_CLIENT_DATA);
							win_translate_xy(SCROLLBAR_PUBLIC(sb),
									xv_get(SCROLLBAR_PUBLIC(sb), XV_ROOT),
									0, 0, &pn->root_x, &pn->root_y);
							pn->pn_digits = 0;
							pn->pagenumber = 0;
							pn->visible = FALSE;

							/* hier war bis zum 22.1.2024 Schluss - aber die
							 * OL Spec sagt, dass das page_window schon beim
							 * SELECT Press kommt:
							 * hierher kommt man schon, wenn man auf die
							 * Drag Area drueckt.
							 */

							/* Aber wenn man vorher z.B. seitenweise geblaettert
							 * hat, kommen hier (zumindest bei TEXTSW und
							 * TERMSW) lustige Seitenzahlen. Das kommt wohl
							 * daher, dass kein scrollbar_scroll aufgerufen
							 * wurde und daher auch die compute_scroll_proc
							 * nicht aufgerufen wurde. Also:
							 */
							x = scrollbar_available_cable(sb);
							y = sb->elevator_rect.r_top - sb->cable_start;
							if (y < 0) {
								y = 0;
							}
							else if (y > x) {
								y = x;
							}
							scrollbar_scroll(sb, y, motion);

							update_pagewin(sb);
						}
						break;

					default:
						break;
				}
			}
			else {
				scrollbar_invert_region(sb, sb->transit_motion);
				switch (sb->transit_motion) {

					case SCROLLBAR_LINE_FORWARD:
					case SCROLLBAR_LINE_BACKWARD:
						scrollbar_timer_stop(SCROLLBAR_PUBLIC(sb));
						if (!sb->transit_occurred) {
							scrollbar_handle_timed_line_event(sb,
									sb->transit_motion);
						}
						break;

					case SCROLLBAR_ABSOLUTE:
						/* 
						 * compute absolute position by top of elevator
						 * in the coordinate space of the cable
						 */
						x = scrollbar_available_cable(sb);
						y = sb->elevator_rect.r_top - sb->cable_start;
						if (y < 0) {
							y = 0;
						}
						else if (y > x) {
							y = x;
						}
						scrollbar_scroll(sb, y, sb->transit_motion);

						if (sb->page_window) {
							xv_set(sb->page_window, XV_SHOW, FALSE, NULL);
							sb->page_window = XV_NULL;
						}
						break;

					default:
						break;
				}
				scrollbar_timer_stop(SCROLLBAR_PUBLIC(sb));
			}
			break;
		case LOC_DRAG:
			if (sb->transit_motion == SCROLLBAR_ABSOLUTE) {
				if (SB_VERTICAL(sb)) {
					y = event_y(event);
					transit_y = event_y(&sb->transit_event);
				}
				else {
					y = event_x(event);
					transit_y = event_x(&sb->transit_event);
				}
				if (transit_y != y) {
					/* 
					 * compute absolute position by top of elevator
					 * in the coordinate space of the cable
					 */
					x = scrollbar_available_cable(sb);
					y = sb->elevator_rect.r_top + (y - transit_y) -
							sb->cable_start;
					if (y < 0) {
						y = 0;
					}
					else if (y > x) {
						y = x;
					}
					scrollbar_absolute_position_elevator(sb, y);
					if ((ignore_drag_count > ignore_drag_max)
							&& sb->drag_repaint_percent) {
						scrollbar_scroll(sb, y, sb->transit_motion);
						if (sb->show_page) {
							SERVERTRACE((400, "LOC_DRAG: y=%d, transit_y=%d\n",
											y, transit_y));
							update_pagewin(sb);
						}
						ignore_drag_count = 1;
					}
					ignore_drag_count++;
				}
			}
			break;
		default:
			break;
	}

	return XV_OK;
}

static void scrollbar_handle_timed_page_event(Xv_scrollbar_info *sb, Scroll_motion   motion)
{
	int x, y, new_y;

	if ((scrollbar_scroll(sb, 0, motion) != SCROLLBAR_POSITION_UNCHANGED) &&
			(sb->jump_pointer)) {
		/* adjust the mouse if necessary */
		if (SB_VERTICAL(sb)) {
			x = event_x(&sb->transit_event);
			y = event_y(&sb->transit_event);
		}
		else {
			x = event_y(&sb->transit_event);
			y = event_x(&sb->transit_event);
		}

		new_y = y;
		if (sb->transit_motion == SCROLLBAR_PAGE_FORWARD &&
				(y <= rect_bottom(&sb->elevator_rect)))
					new_y = rect_bottom(&sb->elevator_rect) + 1;
		else if (sb->transit_motion == SCROLLBAR_PAGE_BACKWARD &&
				(y >= sb->elevator_rect.r_top))
					new_y = sb->elevator_rect.r_top - 1;

		scrollbar_position_mouse(sb, x, new_y);

		if (new_y != y) {
			if (SB_VERTICAL(sb))
				event_set_y(&sb->transit_event, y);
			else
				event_set_x(&sb->transit_event, y);
		}
	}
}

/*
 * Poll the state of the mouse to see if the user still has it physically
 * depressed. This is used with the current paradigm to insure speed of user
 * scrolling while also preventing overflow processing after the mouse is
 * released. The other solution is to slow the scroll action so it works
 * correctly on the _slowest_ case, rather than taking advantage of the
 * faster hardware that is available. jcb 5/3/89
 */
static int scrollbar_select_is_up(Xv_scrollbar_info *sb)
{
	Scrollbar sb_public = SCROLLBAR_PUBLIC(sb);
	Xv_Drawable_info *info;
	Window root, child;
	int root_X, root_Y;
	int win_X, win_Y;
	unsigned int key_buttons;

	/* get the root of X information */
	DRAWABLE_INFO_MACRO(sb_public, info);

	XQueryPointer(xv_display(info), xv_xid(info), &root, &child,
						&root_X, &root_Y, &win_X, &win_Y, &key_buttons);

	/* This looks as if the SELECT button is hardcoded Button1.
	 * However, if a user uses olws_props to manage the workspace properties,
	 * the mouse button will be (re)mapped via XSetPointerMapping, so,
	 * SELECT will always be Button1, ADJUST=Button2, MENU=Button3 -
	 * at least for a 3 button mouse.
	 */
	return ((key_buttons & Button1Mask) == 0);
}

static Notify_value scrollbar_timed_out(Notify_client client, int which)
{
	Xv_scrollbar_info *sb = SCROLLBAR_PRIVATE(client);

	/* if the event has happened, but the user is no longer physically */
	/* pressing the Left Mouse Button, we skip the action. This is a   */
	/* method to use the current programmatic paradigm w/o processing  */
	/* past events/timeouts that have triggered but are no longer valid */
	/* jcb -- 5/3/89 */
	if (scrollbar_select_is_up(sb))
		return NOTIFY_DONE;


	/* tell myself to repeat a scroll */
	switch (sb->transit_motion) {

		case SCROLLBAR_LINE_BACKWARD:
			if (sb->view_start) {
				scrollbar_handle_timed_line_event(sb, sb->transit_motion);
				sb->transit_occurred = TRUE;
			}
			break;

		case SCROLLBAR_LINE_FORWARD:
			scrollbar_handle_timed_line_event(sb, sb->transit_motion);
			sb->transit_occurred = TRUE;
			break;

		case SCROLLBAR_PAGE_FORWARD:
			scrollbar_handle_timed_page_event(sb, sb->transit_motion);
			sb->transit_occurred = TRUE;
			break;

		case SCROLLBAR_PAGE_BACKWARD:
			if (sb->view_start) {
				scrollbar_handle_timed_page_event(sb, sb->transit_motion);
				sb->transit_occurred = TRUE;
			}
			break;

		default:
			break;
	}

	return NOTIFY_DONE;
}


static void scrollbar_line_to_top(Menu menu, Menu_item item)
{
	Xv_scrollbar_info *sb;
	int pos;

	sb = (Xv_scrollbar_info *) xv_get(menu, XV_KEY_DATA, sb_context_key, NULL);

	/* look at transit event to see where the mouse button went down */
	pos =
			SB_VERTICAL(sb) ? event_y(&sb->transit_event) : event_x(&sb->
			transit_event);
	scrollbar_scroll(sb, pos, SCROLLBAR_POINT_TO_MIN);
}

static void scrollbar_top_to_line(Menu menu, Menu_item item)
{
	Xv_scrollbar_info *sb;
	int pos;

	sb = (Xv_scrollbar_info *) xv_get(menu, XV_KEY_DATA, sb_context_key);

	/* look at transit event to see where the mouse button went down */
	pos =
			SB_VERTICAL(sb) ? event_y(&sb->transit_event) : event_x(&sb->
			transit_event);
	scrollbar_scroll(sb, pos, SCROLLBAR_MIN_TO_POINT);
}


static Scroll_motion scrollbar_translate_scrollbar_event_to_motion(Xv_scrollbar_info *sb, Event *event)
{
	int anchor_height = sb_marker_height(sb);
	int elevator_start;
	int elevator_end;
	int region_height;
	int pos;

	pos = SB_VERTICAL(sb) ? event_y(event) : event_x(event);

	elevator_start = sb->elevator_rect.r_top;
	elevator_end = elevator_start + sb->elevator_rect.r_height - 1;
	region_height = sb->elevator_rect.r_height;
	region_height /= (sb->size != SCROLLBAR_FULL_SIZE) ? 2 : 3;

	if (sb->size == SCROLLBAR_MINIMUM)
		if ((pos < elevator_start) || (pos > elevator_end))
			return SCROLLBAR_NONE;
		else if (pos < (elevator_start + region_height))
			return SCROLLBAR_LINE_BACKWARD;
		else
			return SCROLLBAR_LINE_FORWARD;
	else {
		/* Check for top anchor */

		if (pos <= rect_bottom(&sb->top_anchor_rect))
			return SCROLLBAR_TO_START;

		/* Check in between top anchor and elevator */
		else if ((sb->size == SCROLLBAR_FULL_SIZE) && (pos <= elevator_start))
			if (elevator_start <= anchor_height + SCROLLBAR_CABLE_GAP)
				/* already at the top */
				return SCROLLBAR_NONE;
			else
				return SCROLLBAR_PAGE_BACKWARD;

		/* Check for line backward region of elevator */
		else if (pos <= (elevator_start + region_height))
			return SCROLLBAR_LINE_BACKWARD;

		/* Check for absolute scroll region of elevator */
		else if ((sb->size == SCROLLBAR_FULL_SIZE) &&
				(pos <= (elevator_start + region_height * 2)))
			return SCROLLBAR_ABSOLUTE;

		/* Check for line forward region of elevator */
		else if (pos <= elevator_end)
			return SCROLLBAR_LINE_FORWARD;

		/* Check in between elevator and bottom anchor */
		else if ((sb->size == SCROLLBAR_FULL_SIZE)
				&& (pos <=
(sb->length - anchor_height))) if (elevator_end + SCROLLBAR_CABLE_GAP + 1 >=
					sb->length - anchor_height)
				/* already at the bottom */
				return SCROLLBAR_NONE;
			else
				return SCROLLBAR_PAGE_FORWARD;

		/* Check the bottom anchor */
		else if ((pos > sb->bottom_anchor_rect.r_top) &&
				(pos <= rect_bottom(&sb->bottom_anchor_rect)))
					return SCROLLBAR_TO_END;
		else
			return SCROLLBAR_NONE;
	}
}

static void scrollbar_destroy_split(Xv_scrollbar_info *sb)
{
	win_post_id_and_arg(sb->managee, ACTION_SPLIT_DESTROY, NOTIFY_SAFE, XV_NULL,
			(Notify_copy)win_copy_event, (Notify_release)win_free_event);
}

static void scrollbar_invoke_split(Xv_scrollbar_info *sb, Event *event)
{
	int split_loc;
	Scroll_motion motion;

	/* make sure the event is in the scrollbar */
	split_loc = (sb->direction == SCROLLBAR_VERTICAL) ? event_y(event)
			: event_x(event);

	motion = scrollbar_translate_scrollbar_event_to_motion(sb, event);
	if (motion != sb->transit_motion) {
		if ((sb->transit_motion == SCROLLBAR_TO_START
						&& motion == SCROLLBAR_TO_END)
				|| (sb->transit_motion == SCROLLBAR_TO_END
						&& motion == SCROLLBAR_TO_START)) {
			/* destroy the split */
			scrollbar_destroy_split(sb);
		}
		else {
			/* create the split */
			(void)win_post_id_and_arg(sb->managee,
					(sb->direction == SCROLLBAR_VERTICAL) ?
							ACTION_SPLIT_HORIZONTAL : ACTION_SPLIT_VERTICAL,
					NOTIFY_SAFE,
					(unsigned long)split_loc, (Notify_copy)win_copy_event,
					(Notify_release)win_free_event);
		}
	}
}


/*ARGSUSED*/
static void scrollbar_split_view_from_menu(Menu menu, Menu_item item)
{
	Event last_menu_event;
	int do_split;
	Rect managee_rect;
	Xv_scrollbar_info *sb;
	Event split_event;

	sb = (Xv_scrollbar_info *) xv_get(menu, XV_KEY_DATA, sb_context_key);
	last_menu_event = *(Event *) xv_get(menu, MENU_LAST_EVENT);
	if (event_is_button(&last_menu_event))
		scrollbar_invoke_split(sb, (Event *) xv_get(menu, MENU_FIRST_EVENT));
	else {
		event_set_x(&last_menu_event, BORDER_WIDTH);
		event_set_y(&last_menu_event, BORDER_WIDTH);
		managee_rect = *(Rect *) xv_get(sb->managee, WIN_RECT);
		do_split = scrollbar_preview_split(sb, &last_menu_event, &managee_rect,
				&split_event);
		if (do_split == XV_OK)
			scrollbar_invoke_split(sb, &split_event);
	}
}


/*ARGSUSED*/
static void scrollbar_join_view_from_menu(Menu menu, Menu_item item)
{
	scrollbar_destroy_split(
			(Xv_scrollbar_info *) xv_get(menu, XV_KEY_DATA, sb_context_key));
}

/*ARGSUSED*/
static void scrollbar_last_position(Menu menu, Menu_item item)
{
	Xv_scrollbar_info *sb;

	sb = (Xv_scrollbar_info *) xv_get(menu, XV_KEY_DATA, sb_context_key);

	scrollbar_scroll_to_offset(sb, (long)sb->last_view_start);
}


static void scrollbar_set_intransit(Xv_scrollbar_info *sb, Scroll_motion motion, Event *event)
{
    sb->transit_motion = motion;
    sb->transit_event = *event;
    sb->transit_occurred = FALSE;
}

static void scrollbar_translate_to_elevator_event(Xv_scrollbar_info *sb, Event *event)
{
	if (SB_VERTICAL(sb)) {
		event_set_x(event, event_x(event) - sb_margin(sb));
		event_set_y(event, event_y(event) - sb->elevator_rect.r_top);
	}
	else {
		event_set_x(event, event_x(event) - sb->elevator_rect.r_top);
		event_set_y(event, event_y(event) - sb_margin(sb));
	}
}

static void handle_split_destroy_preview(Xv_scrollbar_info *sb, int low, int high, int pos, int *preview_sent)
{
	if (pos >= low && pos <= high) {
		if (!*preview_sent) {
			*preview_sent = TRUE;
			win_post_id_and_arg(sb->managee,
					ACTION_SPLIT_DESTROY_PREVIEW_BEGIN, NOTIFY_IMMEDIATE,
					XV_NULL, (Notify_copy)win_copy_event,
					(Notify_release)win_free_event);
		}
	}
	else {
		if (*preview_sent) {
			*preview_sent = FALSE;
			win_post_id_and_arg(sb->managee,
					ACTION_SPLIT_DESTROY_PREVIEW_CANCEL, NOTIFY_IMMEDIATE,
					XV_NULL, (Notify_copy)win_copy_event,
					(Notify_release)win_free_event);
		}
	}
}

static void note_join_preview(Menu_item item, unsigned do_begin)
{
	Xv_scrollbar_info *sb = (Xv_scrollbar_info *)xv_get(item, XV_KEY_DATA,
														sb_context_key);

	win_post_id_and_arg(sb->managee,
					do_begin
							? ACTION_SPLIT_DESTROY_PREVIEW_BEGIN
							: ACTION_SPLIT_DESTROY_PREVIEW_CANCEL,
					NOTIFY_IMMEDIATE, XV_NULL, (Notify_copy)win_copy_event,
					(Notify_release)win_free_event);
}

static void scrollbar_create_special_menu_items(Xv_scrollbar_info *sb)
{
	sb->split_view_item = xv_create(XV_NULL, MENUITEM,
			MENU_ACTION_ITEM,
				XV_MSG("Split View"), scrollbar_split_view_from_menu,
			XV_HELP_DATA, "xview:scrollbarSplitView",
			NULL);

	sb->join_views_item = xv_create(XV_NULL, MENUITEM,
			MENU_ACTION_ITEM,
				XV_MSG("Join Views"), scrollbar_join_view_from_menu,
			MENU_PREVIEW_PROC, note_join_preview,
			XV_KEY_DATA, sb_context_key, sb,
			XV_HELP_DATA, "xview:scrollbarJoinViews",
			NULL);
}

static int scrollbar_preview_split(Xv_scrollbar_info *sb,
    Event *event, Rect  *r, Event *completion_event)
{
	Fullscreen fs;
	Xv_Window paint_window;
	Xv_Drawable_info *info;
	Display *display;
	XID pw_xid;
	GC *gc_list, gc;
	Rect sb_r;
	int x, y;
	int sb_from_x, sb_from_y, sb_to_x, sb_to_y;
	int from_x, from_y, to_x, to_y;
	Scrollbar scrollbar = SCROLLBAR_PUBLIC(sb);
	Inputmask im;
	int i;
	int jump_delta;
	int update_feedback;
	int status;
	int done;
	int preview_sent = FALSE;
	Rect *use_rect = (sb->top_anchor_inverted ?

			&sb->bottom_anchor_rect : &sb->top_anchor_rect);

	/* Create the fullscreen paint window needed to draw the preview */
	fs = xv_create(xv_get(scrollbar, XV_ROOT), FULLSCREEN,
			FULLSCREEN_INPUT_WINDOW, scrollbar,
			FULLSCREEN_CURSOR_WINDOW, scrollbar,
			WIN_CONSUME_EVENTS,
			WIN_MOUSE_BUTTONS, LOC_DRAG, WIN_UP_EVENTS, NULL, NULL);
	paint_window = (Xv_Window) xv_get(fs, FULLSCREEN_PAINT_WINDOW);
	DRAWABLE_INFO_MACRO(paint_window, info);
	display = xv_display(info);
	pw_xid = xv_xid(info);

	/* Get rubberband GC */
	gc_list = (GC *) xv_get(xv_screen(info), SCREEN_OLGC_LIST, paint_window);
	gc = gc_list[SCREEN_RUBBERBAND_GC];

	/*
	 * Calculate the preview line coordinates in the scrollbar's
	 * coordinate space
	 */
	sb_r = *(Rect *) xv_get(scrollbar, WIN_RECT);
	if (sb->direction == SCROLLBAR_VERTICAL) {
		sb_from_y = sb_to_y = event_y(event);
		if (sb_r.r_left < r->r_left) {
			/* left handed scrollbar */
			sb_from_x = 0;
			sb_to_x = sb_r.r_width + r->r_width;
		}
		else {
			/* right handed scrollbar */
			sb_from_x = r->r_left - sb_r.r_left;
			sb_to_x = sb_r.r_width;
		}
		jump_delta = sb_r.r_height / 10;
	}
	else {
		/* horizontal scrollbar */
		sb_from_x = sb_to_x = event_x(event);
		sb_from_y = r->r_top - sb_r.r_top;
		sb_to_y = sb_r.r_height;
		jump_delta = sb_r.r_width / 10;
	}

	/* 
	 * Translate the preview line coordinates from the scrollbar's
	 * space to the paint window's coordinate space
	 */
	win_translate_xy(scrollbar, paint_window, sb_from_x, sb_from_y, &from_x,
			&from_y);
	win_translate_xy(scrollbar, paint_window, sb_to_x, sb_to_y, &to_x, &to_y);

	x = event_x(event);
	y = event_y(event);

	/* Must synchronize server before drawing the initial preview */
	xv_set(XV_SERVER_FROM_WINDOW(scrollbar), SERVER_SYNC, 0, NULL);

	/* Draw initial preview line */
	XDrawLine(display, pw_xid, gc, from_x, from_y, to_x, to_y);

	/* Set up the input mask */
	for (i = 0; i < IM_MASKSIZE; i++)
		im.im_keycode[i] = 0;
	win_setinputcodebit(&im, MS_LEFT);
	win_setinputcodebit(&im, MS_MIDDLE);
	win_setinputcodebit(&im, MS_RIGHT);
	win_setinputcodebit(&im, LOC_DRAG);
	im.im_flags = IM_ISO | IM_NEGEVENT;

	done = FALSE;
	status = XV_OK;
	while (!done && (status != XV_ERROR)) {
		if (xv_input_readevent((Xv_Window) scrollbar, completion_event, TRUE,
						TRUE, &im) == -1)
			status = XV_ERROR;
		else {
			update_feedback = TRUE;
			xv_translate_iso(event);

			switch (event_action(completion_event)) {
				case ACTION_CANCEL:
					if (event_is_up(completion_event)) {
						XDrawLine(display, pw_xid, gc, from_x, from_y, to_x,
								to_y);
						status = XV_ERROR;
					}
					else
						update_feedback = FALSE;
					break;
				case ACTION_DEFAULT_ACTION:
				case ACTION_SELECT:
					if (event_is_up(completion_event)) {
						XDrawLine(display, pw_xid, gc, from_x, from_y, to_x,
								to_y);
						done = TRUE;
					}
					else
						update_feedback = FALSE;
					break;
				case ACTION_DOWN:
					if (event_is_down(completion_event) &&
							sb->direction == SCROLLBAR_VERTICAL &&
							y < sb_r.r_height - 2 * BORDER_WIDTH)
						y++;
					else
						update_feedback = FALSE;
					break;
				case ACTION_UP:
					if (event_is_down(completion_event) &&
							sb->direction == SCROLLBAR_VERTICAL &&
							y > BORDER_WIDTH) y--;
					else
						update_feedback = FALSE;
					break;
				case ACTION_LEFT:
					if (event_is_down(completion_event) &&
							sb->direction == SCROLLBAR_HORIZONTAL &&
							x > BORDER_WIDTH) x--;
					else
						update_feedback = FALSE;
					break;
				case ACTION_RIGHT:
					if (event_is_down(completion_event) &&
							sb->direction == SCROLLBAR_HORIZONTAL &&
							x < sb_r.r_width - 2 * BORDER_WIDTH)
						x++;
					else
						update_feedback = FALSE;
					break;
				case ACTION_JUMP_DOWN:
					if (event_is_down(completion_event) &&
							sb->direction == SCROLLBAR_VERTICAL) {
						y =
								MIN(y + jump_delta,
								sb_r.r_height - 2 * BORDER_WIDTH);
					}
					else
						update_feedback = FALSE;
					break;
				case ACTION_JUMP_UP:
					if (event_is_down(completion_event) &&
							sb->direction == SCROLLBAR_VERTICAL) {
						y = MAX(y - jump_delta, BORDER_WIDTH);
					}
					else
						update_feedback = FALSE;
					break;
				case ACTION_JUMP_RIGHT:
					if (event_is_down(completion_event) &&
							sb->direction == SCROLLBAR_HORIZONTAL) {
						x =
								MIN(x + jump_delta,
								sb_r.r_width - 2 * BORDER_WIDTH);
					}
					else
						update_feedback = FALSE;
					break;
				case ACTION_JUMP_LEFT:
					if (event_is_down(completion_event) &&
							sb->direction == SCROLLBAR_HORIZONTAL) {
						x = MAX(x - jump_delta, BORDER_WIDTH);
					}
					else
						update_feedback = FALSE;
					break;
				case ACTION_LINE_START:
				case ACTION_DATA_START:
					if (sb->direction == SCROLLBAR_VERTICAL)
						y = BORDER_WIDTH;
					else
						x = BORDER_WIDTH;
					break;
				case ACTION_LINE_END:
				case ACTION_DATA_END:
					if (sb->direction == SCROLLBAR_VERTICAL)
						y = sb_r.r_height - 2 * BORDER_WIDTH;
					else
						x = sb_r.r_width - 2 * BORDER_WIDTH;
					break;
				case LOC_DRAG:
					x = event_x(completion_event);
					y = event_y(completion_event);
					break;
				default:
					update_feedback = FALSE;
					break;
			}
			if (update_feedback) {
				if (sb->direction == SCROLLBAR_VERTICAL) {
					if (y >= 0 && y <= sb_r.r_height) {
						XDrawLine(display, pw_xid, gc, from_x, from_y, to_x,
								to_y);
						from_y += y - sb_from_y;
						to_y = from_y;
						sb_from_y = y;

						handle_split_destroy_preview(sb, use_rect->r_top,
								use_rect->r_top + use_rect->r_height,
								y, &preview_sent);

						XDrawLine(display, pw_xid, gc, from_x, from_y, to_x,
								to_y);
					}
				}
				else {
					if (x >= 0 && x <= sb_r.r_width) {
						XDrawLine(display, pw_xid, gc, from_x, from_y, to_x,
								to_y);
						from_x += x - sb_from_x;
						to_x = from_x;
						sb_from_x = x;

						handle_split_destroy_preview(sb, use_rect->r_top,
								use_rect->r_top + use_rect->r_height,
								x, &preview_sent);

						XDrawLine(display, pw_xid, gc, from_x, from_y, to_x,
								to_y);
					}
				}
			}
		}
	}  /* while (!done && (status != XV_ERROR)) */
	xv_destroy(fs);

	if (preview_sent && status == XV_ERROR) {
		win_post_id_and_arg(sb->managee,
				ACTION_SPLIT_DESTROY_PREVIEW_CANCEL, NOTIFY_IMMEDIATE,
				XV_NULL, (Notify_copy)win_copy_event, (Notify_release)win_free_event);
	}

	event_set_x(completion_event, x);
	event_set_y(completion_event, y);
	return (status);
}



/*
 * scrollbar_multiclick returns TRUE if this (ACTION_SELECT) event has
 * occurred within sb->multiclick_timeout msec of the last ACTION_SELECT
 * event; otherwise, it returns FALSE.
 */
static int scrollbar_multiclick(Xv_scrollbar_info *sb, Event *event)
{
	unsigned int sec_delta, usec_delta;

	sec_delta = event->ie_time.tv_sec - sb->last_select_time.tv_sec;
	usec_delta = event->ie_time.tv_usec - sb->last_select_time.tv_usec;
	if (sec_delta > 0) {	/* normalize the time difference */
		sec_delta--;
		usec_delta += 1000000;
	}

	/* 
	 * Compare the delta time with the multiclick timeout (in msec).
	 * We can't just convert the delta time to msec because we may
	 * very well overflow the machine's integer format with unpredictable
	 * results.
	 */
	if (sec_delta != (sb->multiclick_timeout / 1000)) {
		return (sec_delta < (sb->multiclick_timeout / 1000));
	}
	else {
		return (usec_delta <= (sb->multiclick_timeout * 1000));
	}
}


static int scrollbar_find_view_nbr(Xv_scrollbar_info *sb, Openwin openwin)
{
	Xv_Window view;
	int view_nbr;

	for (view_nbr = 0;
			(view = xv_get(openwin, OPENWIN_NTH_VIEW, view_nbr)) != XV_NULL;
			view_nbr++)
		if (view == sb->managee)
			break;
	return view_nbr;
}

#define SPLIT_VIEW_MENU_ITEM_NBR	5
#define JOIN_VIEWS_MENU_ITEM_NBR	6

static void scrollbar_update_menu(Xv_scrollbar_info *sb)
{
	int nitems;

	if (sb->can_split) {
		Openwin openwin = xv_get(SCROLLBAR_PUBLIC(sb), XV_OWNER);
		int num_views = (int)xv_get(openwin, OPENWIN_NVIEWS);

		if (! sb->have_split) {
			/* this code assumes that the application adds items
			 * (if any) to the END of the menu
			 */
			nitems = (int)xv_get(sb->menu, MENU_NITEMS);
			if (nitems <= SPLIT_VIEW_MENU_ITEM_NBR)
				xv_set(sb->menu,
					MENU_APPEND_ITEM, sb->split_view_item,
					NULL);
			else
				xv_set(sb->menu,
					MENU_INSERT, SPLIT_VIEW_MENU_ITEM_NBR, sb->split_view_item,
					NULL);

			sb->have_split = TRUE;
		}

		if (num_views > 1) {
			if (! sb->have_join) {
				/* this code assumes that the application adds items
				 * (if any) to the END of the menu
				 */
				nitems = (int)xv_get(sb->menu, MENU_NITEMS);
				if (nitems <= JOIN_VIEWS_MENU_ITEM_NBR)
					xv_set(sb->menu,
						MENU_APPEND_ITEM, sb->join_views_item,
						NULL);
				else
					xv_set(sb->menu,
						MENU_INSERT, JOIN_VIEWS_MENU_ITEM_NBR, sb->join_views_item,
						NULL);
				sb->have_join = TRUE;
			}
		}
		else {
			if (sb->have_join)
				xv_set(sb->menu,
						MENU_REMOVE_ITEM, sb->join_views_item,
						NULL);
			sb->have_join = FALSE;
		}
	}
	else {
		if (sb->have_join)
			xv_set(sb->menu,
					MENU_REMOVE_ITEM, sb->join_views_item,
					NULL);
		sb->have_join = FALSE;
		if (sb->have_split)
			xv_set(sb->menu,
					MENU_REMOVE_ITEM, sb->split_view_item,
					NULL);
		sb->have_split = FALSE;
	}
}

static Menu scrollbar_gen_menu(Menu menu, Menu_generate   op)
{
	if (op == MENU_DISPLAY) {
		scrollbar_update_menu((Xv_scrollbar_info *)xv_get(menu,
											XV_KEY_DATA, sb_context_key));
	}
	return (menu);
}

static void scrollbar_create_standard_menu(Xv_scrollbar_info *sb)
{
	Scrollbar scroll = SCROLLBAR_PUBLIC(sb);
	Xv_server srv = XV_SERVER_FROM_WINDOW(scroll);
	char instname[200], *sbinst;

	scrollbar_create_special_menu_items(sb);

	sbinst = (char *)xv_get(scroll, XV_INSTANCE_NAME);
	sprintf(instname, "%sMenu", sbinst ? sbinst : "scrollbar");

	sb->menu = (Menu) xv_create(srv, MENU,
			XV_INSTANCE_NAME, instname,
			MENU_GEN_PROC, scrollbar_gen_menu,
			XV_HELP_DATA, "xview:scrollbarMenu",
			MENU_TITLE_ITEM, XV_MSG("Scrollbar"),
			MENU_ITEM,
				MENU_STRING, (sb->direction == SCROLLBAR_VERTICAL) ?
					XV_MSG("Here to top") : XV_MSG("Here to left"),
				MENU_NOTIFY_PROC, scrollbar_line_to_top,
				XV_HELP_DATA, (sb->direction == SCROLLBAR_VERTICAL) ?
					"xview:scrollbarHereToTop" : "xview:scrollbarHereToLeft",
				0,
			MENU_ITEM,
				MENU_STRING, (sb->direction == SCROLLBAR_VERTICAL) ?
						XV_MSG("Top to here") : XV_MSG("Left to here"),
				MENU_NOTIFY_PROC, scrollbar_top_to_line,
				XV_HELP_DATA, (sb->direction == SCROLLBAR_VERTICAL) ?
					"xview:scrollbarTopToHere" : "xview:scrollbarLeftToHere",
				0,
			MENU_ITEM,
				MENU_STRING, XV_MSG("Previous"),
				MENU_NOTIFY_PROC, scrollbar_last_position,
				XV_HELP_DATA, "xview:scrollbarPrevious",
				0,
			XV_KEY_DATA, sb_context_key, sb,
			NULL);

	scrollbar_update_menu(sb);
	xv_set( sb->menu, XV_SET_MENU, scroll, NULL);
}

static Notify_value scrollbar_handle_event(Xv_Window sb_public,
				Notify_event ev, Notify_arg arg, Notify_event_type type)
{
	Event *event = (Event *)ev;
	int do_split;
	Xv_Window focus_win;
	Frame frame;
	char *help_key;
	Scroll_motion motion;
	Xv_window openwin;
	Scrollbar other_sb;	/* other Scrollbar attached to this openwin */
	Xv_Window paint_window;
	Rect r;
	Xv_scrollbar_info *sb = SCROLLBAR_PRIVATE(sb_public);
	Event split_event;
	int view_nbr;
	Xv_Window view_window;

	xv_translate_iso(event);

	switch (event_action(event)) {
		case ACTION_RESCALE:
			sb_rescale(sb, (Frame_rescale_state) arg);
			notify_next_event_func(sb_public, ev, arg, type);
			break;

		case WIN_RESIZE:
			sb_resize(sb);
			break;

		case WIN_REPAINT:
			scrollbar_paint(sb_public);
			sb->painted = TRUE;
			break;

		case ACTION_HELP:
		case ACTION_MORE_HELP:
		case ACTION_TEXT_HELP:
		case ACTION_MORE_TEXT_HELP:
		case ACTION_INPUT_FOCUS_HELP:
			if (event_is_down(event)) {
				motion = scrollbar_translate_scrollbar_event_to_motion(sb,
															event);
				switch (motion) {
					case SCROLLBAR_TO_END:
    					if (sb->can_split) {
							openwin = xv_get(sb_public, XV_OWNER);

							if (xv_get(openwin, OPENWIN_NVIEWS) >= 2)
								help_key = "xview:scrollbarBottomSplit2";
							else help_key = "xview:scrollbarBottomSplit";
						}
						else help_key = "xview:scrollbarBottom";
						break;
					case SCROLLBAR_TO_START:
    					if (sb->can_split) {
							openwin = xv_get(sb_public, XV_OWNER);

							if (xv_get(openwin, OPENWIN_NVIEWS) >= 2)
								help_key = "xview:scrollbarTopSplit2";
							else help_key = "xview:scrollbarTopSplit";
						}
						else help_key = "xview:scrollbarTop";
						break;
					case SCROLLBAR_PAGE_FORWARD:
						help_key = "xview:scrollbarPageForward";
						break;
					case SCROLLBAR_PAGE_BACKWARD:
						help_key = "xview:scrollbarPageBackward";
						break;
					case SCROLLBAR_LINE_FORWARD:
						help_key = "xview:scrollbarLineForward";
						break;
					case SCROLLBAR_LINE_BACKWARD:
						help_key = "xview:scrollbarLineBackward";
						break;
					case SCROLLBAR_ABSOLUTE:
						help_key = "xview:scrollbarDrag";
						break;
					case SCROLLBAR_NONE:
						help_key = "xview:scrollbarPageBackward";
						break;
					default:
						help_key = "xview:scrollbarOther";
						break;
				}
				xv_help_show(sb_public, help_key, event);
			}
			break;

		case KBD_USE:
			frame = xv_get(sb_public, WIN_FRAME);
			focus_win = xv_get(frame, FRAME_FOCUS_WIN);
			xv_set(frame,
					FRAME_FOCUS_DIRECTION,
						SB_VERTICAL(sb) ? FRAME_FOCUS_RIGHT : FRAME_FOCUS_UP,
					NULL);
			xv_set(focus_win, WIN_PARENT, sb_public, XV_X, 0, XV_Y, 0, NULL);
			frame_kbd_use(frame, xv_get(sb_public, XV_OWNER), sb_public);
			xv_set(focus_win, XV_SHOW, TRUE, NULL);
			break;

		case KBD_DONE:
			frame = xv_get(sb_public, WIN_FRAME);
			focus_win = xv_get(frame, FRAME_FOCUS_WIN);
			xv_set(focus_win, XV_SHOW, FALSE, NULL);
			frame_kbd_done(frame, xv_get(sb_public, XV_OWNER));
			break;

		case ACTION_NEXT_PANE:
			/* Order of precedence:
			 *  Scrollbar -> next paint or view window's first element
			 *  Scrollbar -> next pane
			 */
			if (event_is_down(event)) {
				openwin = xv_get(sb_public, XV_OWNER);
				if (openwin) {
					view_nbr = scrollbar_find_view_nbr(sb, openwin);
					/* Scrollbar -> next paint or view window's first element */
					if (xv_get(openwin, XV_IS_SUBTYPE_OF, CANVAS)) {
						/* Next paint window receives input focus.  Find the paint
						 * window that corresponds to the view window.
						 */
						paint_window = xv_get(openwin,
								CANVAS_NTH_PAINT_WINDOW, view_nbr + 1);
						if (paint_window) {
							xv_set(paint_window, WIN_SET_FOCUS, NULL);
							xv_set(openwin, XV_FOCUS_ELEMENT, 0, NULL);
						}
						else
							openwin = XV_NULL;
					}
					else {
						/* Next view window receives input focus */
						view_window =
								xv_get(openwin, OPENWIN_NTH_VIEW, view_nbr + 1);
						if (view_window)
							xv_set(view_window, WIN_SET_FOCUS, NULL);
						else
							openwin = XV_NULL;
					}
				}
				if (!openwin) {
					frame = xv_get(sb_public, WIN_FRAME);
					xv_set(frame, FRAME_NEXT_PANE, NULL);
				}
			}
			break;

		case ACTION_PREVIOUS_PANE:
			/* Order of precedence:
			 *  Scrollbar -> scrollbar's paint or view window's last element
			 *  Scrollbar -> previous pane
			 */
			if (event_is_down(event)) {
				openwin = xv_get(sb_public, XV_OWNER);
				if (openwin) {
					view_nbr = scrollbar_find_view_nbr(sb, openwin);
					/* Scrollbar -> scrollbar's paint or view window */
					if (xv_get(openwin, XV_IS_SUBTYPE_OF, CANVAS)) {
						/* Paint window receives input focus.  Find the paint
						 * window that corresponds to the view window.  Set the
						 * focus to the last element in the paint window.
						 */
						xv_set(xv_get(openwin,CANVAS_NTH_PAINT_WINDOW,view_nbr),
									WIN_SET_FOCUS,
									NULL);
						xv_set(openwin, XV_FOCUS_ELEMENT, -1, NULL);
					}
					else
						/* View window receives input focus */
						xv_set(sb->managee, WIN_SET_FOCUS, NULL);
				}
				else {
					frame = xv_get(sb_public, WIN_FRAME);
					xv_set(frame, FRAME_PREVIOUS_PANE, NULL);
				}
			}
			break;

		case ACTION_NEXT_ELEMENT:
			/* Order of precedence:
			 *  Vertical scrollbar -> Horizontal scrollbar
			 *  Any scrollbar -> next paint or view window's first element
			 *  Any scrollbar -> next pane
			 */
			if (event_is_down(event)) {
				openwin = xv_get(sb_public, XV_OWNER);
				if (openwin) {
					view_nbr = scrollbar_find_view_nbr(sb, openwin);
					if (SB_VERTICAL(sb)) {
						view_window =
								xv_get(openwin, OPENWIN_NTH_VIEW, view_nbr);
						other_sb =
								xv_get(openwin, OPENWIN_HORIZONTAL_SCROLLBAR,
								view_window);
						if (other_sb) {
							xv_set(other_sb, WIN_SET_FOCUS, NULL);
							break;
						}
					}
					/* Any scrollbar -> next paint or view window's first element */
					if (xv_get(openwin, XV_IS_SUBTYPE_OF, CANVAS)) {
						/* Next paint window receives input focus.  Find the paint
						 * window that corresponds to the view window.
						 */
						paint_window = xv_get(openwin,
								CANVAS_NTH_PAINT_WINDOW, view_nbr + 1);
						if (paint_window) {
							xv_set(paint_window, WIN_SET_FOCUS, NULL);
							xv_set(openwin, XV_FOCUS_ELEMENT, 0, NULL);
						}
						else
							openwin = XV_NULL;
					}
					else {
						/* Next view window receives input focus */
						view_window =
								xv_get(openwin, OPENWIN_NTH_VIEW, view_nbr + 1);
						if (view_window)
							xv_set(view_window, WIN_SET_FOCUS, NULL);
						else
							openwin = XV_NULL;
					}
				}
				if (!openwin) {
					frame = xv_get(sb_public, WIN_FRAME);
					xv_set(frame, FRAME_NEXT_PANE, NULL);
				}
			}
			break;

		case ACTION_PREVIOUS_ELEMENT:
			/* Order of precedence:
			 *  Horizontal scrollbar -> Vertical scrollbar
			 *  Any scrollbar -> scrollbar's paint or view window
			 *  Any scrollbar -> last element in previous pane
			 */
			if (event_is_down(event)) {
				openwin = xv_get(sb_public, XV_OWNER);
				if (openwin) {
					view_nbr = scrollbar_find_view_nbr(sb, openwin);
					if (!SB_VERTICAL(sb)) {
						view_window =
								xv_get(openwin, OPENWIN_NTH_VIEW, view_nbr);
						other_sb =
								xv_get(openwin, OPENWIN_VERTICAL_SCROLLBAR,
								view_window);
						if (other_sb) {
							/* Horizontal scrollbar -> Vertical scrollbar */
							xv_set(other_sb, WIN_SET_FOCUS, NULL);
							break;
						}
					}
					/* Any scrollbar -> scrollbar's paint or view window */
					if (xv_get(openwin, XV_IS_SUBTYPE_OF, CANVAS)) {
						/* Paint window receives input focus.  Find the paint
						 * window that corresponds to the view window.  Set the
						 * focus to the last element in the paint window.
						 */
						xv_set(xv_get(openwin, CANVAS_NTH_PAINT_WINDOW,
										view_nbr), WIN_SET_FOCUS, NULL);
						xv_set(openwin, XV_FOCUS_ELEMENT, -1, NULL);
					}
					else
						/* View window receives input focus */
						xv_set(sb->managee, WIN_SET_FOCUS, NULL);
				}
				else {
					/* Any scrollbar -> last element in previous pane */
					frame = xv_get(sb_public, WIN_FRAME);
					xv_set(frame, FRAME_PREVIOUS_ELEMENT, NULL);
				}
			}
			break;

		case ACTION_UP:
			if (event_is_up(event) || !SB_VERTICAL(sb))
				break;
			scrollbar_scroll(sb, 0, SCROLLBAR_LINE_BACKWARD);
			break;

		case ACTION_DOWN:
			if (event_is_up(event) || !SB_VERTICAL(sb))
				break;
			scrollbar_scroll(sb, 0, SCROLLBAR_LINE_FORWARD);
			break;

		case ACTION_LEFT:
			if (event_is_up(event) || SB_VERTICAL(sb))
				break;
			scrollbar_scroll(sb, 0, SCROLLBAR_LINE_BACKWARD);
			break;

		case ACTION_RIGHT:
			if (event_is_up(event) || SB_VERTICAL(sb))
				break;
			scrollbar_scroll(sb, 0, SCROLLBAR_LINE_FORWARD);
			break;

		case ACTION_JUMP_UP:
			if (event_is_up(event) || !SB_VERTICAL(sb))
				break;
			scrollbar_scroll(sb, 0, SCROLLBAR_PAGE_BACKWARD);
			break;

		case ACTION_JUMP_DOWN:
			if (event_is_up(event) || !SB_VERTICAL(sb))
				break;
			scrollbar_scroll(sb, 0, SCROLLBAR_PAGE_FORWARD);
			break;

		case ACTION_JUMP_LEFT:
			if (event_is_up(event) || SB_VERTICAL(sb))
				break;
			scrollbar_scroll(sb, 0, SCROLLBAR_PAGE_BACKWARD);
			break;

		case ACTION_JUMP_RIGHT:
			if (event_is_up(event) || SB_VERTICAL(sb))
				break;
			scrollbar_scroll(sb, 0, SCROLLBAR_PAGE_FORWARD);
			break;

		case ACTION_DATA_START:
			if (event_is_down(event))
				scrollbar_scroll(sb, 0, SCROLLBAR_TO_START);
			break;

		case ACTION_DATA_END:
			if (event_is_down(event))
				scrollbar_scroll(sb, 0, SCROLLBAR_TO_END);
			break;

		case ACTION_JUMP_MOUSE_TO_INPUT_FOCUS:
			xv_set(sb_public, WIN_MOUSE_XY, 0, 0, NULL);
			break;

		case ACTION_SELECT:
			if (sb->inactive)
				break;
			if (event_is_down(event)) {
				motion = scrollbar_multiclick(sb, event) ? sb->transit_motion :
						scrollbar_translate_scrollbar_event_to_motion(sb,
						event);
				sb->last_motion = motion;
				switch (motion) {
					case SCROLLBAR_TO_END:
					case SCROLLBAR_TO_START:
						scrollbar_invert_region(sb, motion);
						break;

					case SCROLLBAR_PAGE_FORWARD:
					case SCROLLBAR_PAGE_BACKWARD:
						scrollbar_timer_start(SCROLLBAR_PUBLIC(sb),
								SB_SET_FOR_PAGE);
						break;

					case SCROLLBAR_LINE_FORWARD:
					case SCROLLBAR_LINE_BACKWARD:
					case SCROLLBAR_ABSOLUTE:
						/* translate event to elevator space */
						scrollbar_translate_to_elevator_event(sb, event);
						scrollbar_handle_elevator_event(sb, event, motion);
						break;

					default:
						break;
				}
				scrollbar_set_intransit(sb, motion, event);
			}
			else {
				switch (sb->transit_motion) {
					case SCROLLBAR_TO_END:
					case SCROLLBAR_TO_START:
						scrollbar_invert_region(sb, sb->transit_motion);
						(void)scrollbar_scroll(sb, 0, sb->transit_motion);
						break;

					case SCROLLBAR_PAGE_FORWARD:
					case SCROLLBAR_PAGE_BACKWARD:
						if (!sb->transit_occurred) {
							scrollbar_handle_timed_page_event(sb,
									sb->transit_motion);
						}
						break;

					case SCROLLBAR_LINE_FORWARD:
					case SCROLLBAR_LINE_BACKWARD:
					case SCROLLBAR_ABSOLUTE:
						/* translate event to elevator space */
						scrollbar_translate_to_elevator_event(sb, event);
						scrollbar_handle_elevator_event(sb, event,
								sb->transit_motion);
						break;

					default:
						break;
				}
				scrollbar_timer_stop(SCROLLBAR_PUBLIC(sb));
			}
			sb->last_select_time = event->ie_time;
			break;

		case ACTION_MENU:
			if (sb->inactive)
				break;
			if (event_is_down(event)) {
				scrollbar_set_intransit(sb, SCROLLBAR_NONE, event);
				if (!sb->menu) {
					scrollbar_create_standard_menu(sb);
				}
				menu_show(sb->menu, SCROLLBAR_PUBLIC(sb), event, NULL);
			}
			break;

		case LOC_DRAG:
			if (sb->inactive)
				break;
			if ((sb->transit_motion == SCROLLBAR_TO_END
							|| sb->transit_motion == SCROLLBAR_TO_START)
					&& sb->can_split) {
				/* when user pulls out of split bar start split */
				motion =
						scrollbar_translate_scrollbar_event_to_motion(sb,
						event);
				if (motion != sb->transit_motion) {
					r = *(Rect *) xv_get(sb->managee, WIN_RECT);
					do_split =
							scrollbar_preview_split(sb, event, &r,
							&split_event);
					/* split event may cause this sb to be destroy */
					/* therefore all painting must be done first */
					scrollbar_invert_region(sb, sb->transit_motion);
					if (do_split == XV_OK) {
						scrollbar_invoke_split(sb, &split_event);
					}
					scrollbar_set_intransit(sb, SCROLLBAR_NONE, event);
				}
			}
			else if (sb->transit_motion == SCROLLBAR_ABSOLUTE) {
				/* translate event to elevator space */
				scrollbar_translate_to_elevator_event(sb, event);
				scrollbar_handle_elevator_event(sb, event, sb->transit_motion);
			}
			break;

		default:
			break;
	}

	return NOTIFY_DONE;
}

/******************************************************************/

static int scrollbar_create_internal(Xv_opaque parent, Xv_opaque sb_public,
								Xv_opaque *avlist, int *unused)
{
	Xv_scrollbar *scrollbar = (Xv_scrollbar *) sb_public;
	Xv_scrollbar_info *sb;
	Xv_Drawable_info *info;
	int three_d = FALSE;

	sb = xv_alloc(Xv_scrollbar_info);
	sb->public_self = sb_public;
	scrollbar->private_data = (Xv_opaque) sb;
	sb->managee = parent;

	DRAWABLE_INFO_MACRO(sb_public, info);
	sb->direction = SCROLLBAR_VERTICAL;
	sb->elevator_state = 0;
	sb->size = SCROLLBAR_FULL_SIZE;
	sb->can_split = FALSE;
	sb->compute_scroll_proc = scrollbar_default_compute_scroll_proc;
	sb->creating = TRUE;
	three_d = (SCREEN_UIS_3D_COLOR ==
				(screen_ui_style_t)xv_get(xv_screen(info), SCREEN_UI_STYLE));

	{
		Xv_font font;

		if (xv_get(parent, WIN_IS_CLIENT_PANE)) {
			/* woher bekomme ich jetzt einen 'vernuenftigen Font ? */
			/* vielleicht ist der OK ? */
			Xv_window fram = xv_get(parent, XV_OWNER);
			font = xv_get(fram, XV_FONT);
			xv_set(sb_public,
					WIN_GLYPH_FONT, xv_get(fram, WIN_GLYPH_FONT),
					NULL);
		}
		else {
			font = xv_get(sb_public, XV_FONT);
		}
		sb->ginfo = xv_init_olgx(sb_public, &three_d, font);
	}

	sb->last_view_start = 0;
	sb->menu = (Menu) 0;	/* Delay menu creation until first show */
	if (defaults_exists("openWindows.scrollbarjumpCursor",
					"OpenWindows.ScrollbarJumpCursor")) {
		sb->jump_pointer =
				defaults_get_boolean("OpenWindows.scrollbarjumpCursor",
				"OpenWindows.ScrollbarJumpCursor", (Bool) TRUE);
	}
	else {
		sb->jump_pointer = defaults_get_boolean("scrollbar.jumpCursor",
				"Scrollbar.JumpCursor", (Bool) TRUE);
	}
	sb->length = 1;
	sb->multiclick_timeout = 100 *
			defaults_get_integer_check("openWindows.scrollbarRepeatTimeout",
			"OpenWindows.ScrollbarRepeatTimeout", 3, 1, 9);
	sb->object_length = 1 /*SCROLLBAR_DEFAULT_LENGTH */ ;
	sb->page_length = SCROLLBAR_DEFAULT_LENGTH;
	sb->page_height = 60;
	sb->pixels_per_unit = 1;
	sb->view_length = SCROLLBAR_DEFAULT_LENGTH;
	sb->view_start = 0;
	sb->length = 1;
	sb->window = xv_xid(info);
	sb->top_anchor_inverted = FALSE;
	sb->bottom_anchor_inverted = FALSE;
	sb->size = SCROLLBAR_FULL_SIZE;
	sb->transit_motion = SCROLLBAR_NONE;
	sb->last_motion = SCROLLBAR_NONE;
	sb->drag_repaint_percent = 100;

	/* create keys so client can hang data off objects */
	if (sb_context_key == (Attr_attribute) 0) {
		sb_context_key = xv_unique_key();
	}

	if (defaults_get_enum("openWindows.keyboardCommands",
					"OpenWindows.KeyboardCommands",
					xv_kbd_cmds_value_pairs) < KBD_CMDS_FULL ||
			(parent && xv_get(parent, XV_IS_SUBTYPE_OF, TERMSW)) ||
			(parent && xv_get(parent, XV_IS_SUBTYPE_OF, TEXTSW))) {
		/* Mouseless operations disabled, or we're attached to a textsw
		 * (which does it's own keyboard-based scrolling):
		 * Don't allow the Scrollbar window to accept the keyboard focus.
		 * Otherwise, scrolling via clicking SELECT will remove the
		 * keyboard focus from it's client (e.g., a textsw).
		 * Note: Consuming ACTION_HELP sets KBD_USE in the input mask,
		 * which allows keyboard focus to be set on the Scrollbar Window.
		 * (See call to win_set_kbd_focus() in xevent_to_event() for more
		 * information.)
		 */
		win_set_no_focus(sb_public, TRUE);
	}

	xv_set(sb_public,
			XV_SHOW, FALSE,
			WIN_NOTIFY_SAFE_EVENT_PROC, scrollbar_handle_event,
			WIN_NOTIFY_IMMEDIATE_EVENT_PROC, scrollbar_handle_event,
			WIN_BIT_GRAVITY, ForgetGravity,
			WIN_CONSUME_EVENTS,
				ACTION_HELP, WIN_UP_EVENTS, LOC_DRAG, WIN_MOUSE_BUTTONS, NULL,
			WIN_RETAINED,
				xv_get(xv_get(sb_public, XV_SCREEN, 0), SCREEN_RETAIN_WINDOWS),
			NULL);

	return XV_OK;
}

#define ADONE ATTR_CONSUME(*attrs);break

static void scrollbar_parse_attr(Xv_scrollbar_info *sb, Attr_avlist avlist)
{
	register Attr_avlist attrs;
	int position_elevator = FALSE;
	long view_start = 0;
	int view_start_set = FALSE;

	for (attrs = avlist; *attrs; attrs = attr_next(attrs)) {
		switch (*attrs) {
			case SCROLLBAR_INACTIVE:
				sb->inactive = (int)attrs[1];
				sb->elevator_state = (sb->inactive) ? OLGX_INACTIVE : 0;
				if (!sb->creating) {
					position_elevator = TRUE;	/* repaint elevator */
				}
				ADONE;

			case SCROLLBAR_PIXELS_PER_UNIT:
				{
					int pixels = (int)attrs[1];

					if (pixels > 0) {
						if (!sb->creating) {
							sb->view_length = sb->view_length *
									sb->pixels_per_unit / pixels;
							sb->object_length = sb->object_length *
									sb->pixels_per_unit / pixels;
							sb->pixels_per_unit = pixels;
							scrollbar_paint(SCROLLBAR_PUBLIC(sb));
						}
						else
							sb->pixels_per_unit = pixels;
					}
				}
				ADONE;

			case SCROLLBAR_OBJECT_LENGTH:
				if ((int)attrs[1] >= 0 && (int)attrs[1] != sb->object_length) {
					sb->object_length = (unsigned)attrs[1];
					if (!sb->creating) {
						position_elevator = TRUE;
					}
				}
				ADONE;

			case SCROLLBAR_OVERSCROLL:
				/* This is only here for binary compat */
				ADONE;

			case SCROLLBAR_VIEW_START:
				view_start = (long)attrs[1];
				view_start_set = TRUE;
				ADONE;

			case SCROLLBAR_VIEW_LENGTH:
				if ((int)attrs[1] >= 0 && (int)attrs[1] != sb->view_length) {
					sb->view_length = (int)attrs[1];
				}
				ADONE;

			case SCROLLBAR_PAGE_LENGTH:
				if ((int)attrs[1] >= 0) {
					sb->page_length = (int)attrs[1];
				}
				ADONE;

			case SCROLLBAR_SPLITTABLE:
				sb->can_split = (int)attrs[1];
				ADONE;

			case SCROLLBAR_PAGE:
				sb->current_page = (int)attrs[1];
				ADONE;
			case SCROLLBAR_SHOW_PAGE:
				sb->show_page = (int)attrs[1];
				ADONE;
			case SCROLLBAR_PAGE_HEIGHT:
				sb->page_height = (int)attrs[1];
				ADONE;

			case SCROLLBAR_NORMALIZE_PROC:
				sb->normalize_proc = (normalize_proc_t)attrs[1];
				ADONE;

			case SCROLLBAR_COMPUTE_SCROLL_PROC:
				sb->compute_scroll_proc = (compute_scroll_proc_t)attrs[1];
				ADONE;

			case SCROLLBAR_NOTIFY_CLIENT:
				sb->managee = (Xv_opaque)attrs[1];
				ADONE;

			case SCROLLBAR_DIRECTION:
				sb->direction = (Scrollbar_setting) attrs[1];
				if (!sb->creating) {
					scrollbar_init_positions(sb);
				}
				ADONE;

			case SCROLLBAR_PERCENT_OF_DRAG_REPAINTS:
				sb->drag_repaint_percent = (int)attrs[1];
				if (sb->drag_repaint_percent > 100)
					sb->drag_repaint_percent = 100;
				if (sb->drag_repaint_percent < 0)
					sb->drag_repaint_percent = 0;
				ADONE;

			case WIN_CMS_CHANGE:
				if (sb->ginfo)
					/* should also clear window */
					scrollbar_paint(SCROLLBAR_PUBLIC(sb));
				break;

			case XV_END_CREATE:
				sb->creating = FALSE;
				if (sb->direction == SCROLLBAR_VERTICAL) {
					sb->length = (int)xv_get(SCROLLBAR_PUBLIC(sb), XV_HEIGHT);
				}
				else {
					sb->length = (int)xv_get(SCROLLBAR_PUBLIC(sb), XV_WIDTH);
				}
				scrollbar_init_positions(sb);
				break;

			default:
				/* both vertical and horizontal share the same attrs */
				xv_check_bad_attr(SCROLLBAR, *attrs);
				break;
		}
	}

	/* Process a change in view_start */
	if (view_start_set) {
		if (!sb->creating) {
			/* normalize first */
			if (sb->normalize_proc != NULL)
				sb->normalize_proc(SCROLLBAR_PUBLIC(sb),
						(long)view_start, SCROLLBAR_ABSOLUTE, &view_start);
			if (view_start > Max_offset(sb))
				view_start = Max_offset(sb);
			if (scrollbar_scroll_to_offset(sb, (long)view_start) !=
					SCROLLBAR_POSITION_UNCHANGED)
				position_elevator = TRUE;
		}
		else {
			if (view_start > Max_offset(sb))
				view_start = Max_offset(sb);
			sb->view_start = view_start;
		}
	}
	/*
	 * Calculate new elevator position, but don't paint unless already
	 * painted.
	 */
	if (position_elevator && sb->ginfo) {
		scrollbar_position_elevator(sb, sb->painted, SCROLLBAR_NONE);
	}

	if (!sb->managee && sb->can_split) {
		sb->can_split = FALSE;
		xv_error(XV_NULL,
				ERROR_STRING,
				XV_MSG
				("Cannot split a scrollbar created with scrollbar_create()"),
				ERROR_PKG, SCROLLBAR, NULL);
	}
}

static Xv_opaque scrollbar_set_internal(Scrollbar scrollbar, Attr_avlist avlist)
{
    Xv_scrollbar_info *sb = SCROLLBAR_PRIVATE(scrollbar);

    scrollbar_parse_attr(sb, avlist);

    return (XV_OK);
}

static Xv_opaque scrollbar_get_internal(Scrollbar scroll_public, int *status,
						Attr_attribute attr, va_list valist)
{
	Xv_scrollbar_info *sb = SCROLLBAR_PRIVATE(scroll_public);

	switch (attr) {
		case SCROLLBAR_VIEW_START:
			return ((Xv_opaque) sb->view_start);

		case SCROLLBAR_PIXELS_PER_UNIT:
			return ((Xv_opaque) sb->pixels_per_unit);

		case SCROLLBAR_NOTIFY_CLIENT:
			return ((Xv_opaque) sb->managee);

		case SCROLLBAR_OBJECT_LENGTH:
			return ((Xv_opaque) sb->object_length);

		case SCROLLBAR_VIEW_LENGTH:
			return ((Xv_opaque) sb->view_length);

		case SCROLLBAR_DIRECTION:
			return ((Xv_opaque) sb->direction);

		case SCROLLBAR_PERCENT_OF_DRAG_REPAINTS:
			return ((Xv_opaque) sb->drag_repaint_percent);

		case SCROLLBAR_MOTION:
			return ((Xv_opaque) sb->last_motion);

		case SCROLLBAR_LAST_VIEW_START:
			return ((Xv_opaque) sb->last_view_start);

		case SCROLLBAR_NORMALIZE_PROC:
			return ((Xv_opaque) sb->normalize_proc);

		case SCROLLBAR_COMPUTE_SCROLL_PROC:
			return ((Xv_opaque) sb->compute_scroll_proc);

		case SCROLLBAR_SPLITTABLE:
			return ((Xv_opaque) sb->can_split);

		case SCROLLBAR_SHOW_PAGE:
			return ((Xv_opaque) sb->show_page);
		case SCROLLBAR_PAGE:
			return ((Xv_opaque) sb->current_page);
		case SCROLLBAR_PAGE_HEIGHT:
			return ((Xv_opaque) sb->page_height);

		case SCROLLBAR_MENU:
			/* If the menu hasn't been created yet, do so */
			if (!sb->menu)
				scrollbar_create_standard_menu(sb);
			return ((Xv_opaque) sb->menu);

		case SCROLLBAR_INACTIVE:
			return ((Xv_opaque) sb->inactive);

		case SCROLLBAR_PAGE_LENGTH:
			return ((Xv_opaque) sb->page_length);

			/* defunct attribute */
		case SCROLLBAR_OVERSCROLL:
			return ((Xv_opaque) NULL);

		default:
			xv_check_bad_attr(SCROLLBAR, attr);
			*status = XV_ERROR;
			return (Xv_opaque) 0;
	}
}

#define SPLIT_VIEW_MENU_ITEM_NBR	5
#define JOIN_VIEWS_MENU_ITEM_NBR	6

static int scrollbar_destroy_internal(Scrollbar sb_public, Destroy_status status)
{
	Xv_scrollbar_info *sb = SCROLLBAR_PRIVATE(sb_public);
	Xv_Window focus_win;
	Frame frame;

	if ((status == DESTROY_CLEANUP) || (status == DESTROY_PROCESS_DEATH)) {
		Menu_item mi;

		/* If the scrollbar owns the Frame focus window, then ???
		 */
		frame = xv_get(sb_public, WIN_FRAME);
		focus_win = xv_get(frame, FRAME_FOCUS_WIN);
		if (focus_win && xv_get(focus_win, WIN_PARENT) == sb_public) {
			xv_set(focus_win, WIN_PARENT, frame,	/* the only window
													 * guaranteed still
													 * to exist.
													 */
					XV_SHOW, FALSE, NULL);
			/* BUG ALERT:  If the canvas is the only frame subwindow,
			 *         we will be left without a current focus subwindow.
			 */
			xv_set(frame, FRAME_NEXT_PANE, NULL);
		}

		/* Clean up menu */
		if (sb->menu) xv_destroy_status(sb->menu, DESTROY_CLEANUP);
		sb->menu = XV_NULL;

		mi = sb->join_views_item;
		sb->join_views_item = XV_NULL;
		if (mi) {
			/* this has been created without MENU_RELEASE - so, if we want it
			 * to be cleanup up, we have to set MENU_RELEASE
			 */
			xv_set(mi, MENU_RELEASE, NULL);
			xv_destroy_status(mi, DESTROY_CLEANUP);
		}
		mi = sb->split_view_item;
		sb->split_view_item = XV_NULL;
	 	if (mi) {
			xv_set(mi, MENU_RELEASE, NULL);
			xv_destroy_status(mi, DESTROY_CLEANUP);
		}

		if (status == DESTROY_CLEANUP)
			free((char *)sb);
	}
	return XV_OK;
}

Xv_public void scrollbar_default_compute_scroll_proc(Scrollbar scroll_public,
			int pos, int length, Scroll_motion motion, long *offset,
			long *object_length)
{
	Xv_scrollbar_info *sb = SCROLLBAR_PRIVATE(scroll_public);
	int minus_movement;
	unsigned long pixel_offset;

	pixel_offset = sb->view_start * sb->pixels_per_unit;

	switch (motion) {
		case SCROLLBAR_ABSOLUTE:
			/* pos is position in the cable */
			pixel_offset = scrollbar_absolute_offset(sb, pos, length);
			break;

		case SCROLLBAR_POINT_TO_MIN:
			pixel_offset += (pos - sb->pixels_per_unit);
			break;

		case SCROLLBAR_PAGE_FORWARD:
			if (sb->page_length != SCROLLBAR_DEFAULT_LENGTH) {
				/* page scrolling */
				pixel_offset += (sb->page_length * sb->pixels_per_unit);
			}
			else {
				/* display scrolling */
				pixel_offset += (sb->view_length * sb->pixels_per_unit);
			}
			break;

		case SCROLLBAR_LINE_FORWARD:
			pixel_offset += sb->pixels_per_unit;
			break;

		case SCROLLBAR_MIN_TO_POINT:
			if (pos > pixel_offset) {
				*offset = 0;
			}
			else {
				pixel_offset -= (pos - sb->pixels_per_unit);
			}
			break;

		case SCROLLBAR_PAGE_BACKWARD:
			if (sb->page_length != SCROLLBAR_DEFAULT_LENGTH) {
				/* page scrolling */
				minus_movement = sb->page_length * sb->pixels_per_unit;
			}
			else {
				/* display scrolling */
				minus_movement = sb->view_length * sb->pixels_per_unit;
			}
			if (minus_movement > pixel_offset) {
				pixel_offset = 0;
			}
			else {
				pixel_offset -= minus_movement;
			}
			break;

		case SCROLLBAR_LINE_BACKWARD:
			if (sb->pixels_per_unit > pixel_offset) {
				pixel_offset = 0;
			}
			else {
				pixel_offset -= sb->pixels_per_unit;
			}
			break;

		case SCROLLBAR_TO_END:
			pixel_offset = Max_offset(sb) * sb->pixels_per_unit;
			break;

		case SCROLLBAR_TO_START:
			pixel_offset = 0;
			break;

		default:
			break;
	}

	scrollbar_offset_to_client_units(sb, pixel_offset, motion, offset);
	*object_length = sb->object_length;
}

Xv_public void scrollbar_paint(Scrollbar sb_public)
{
    Xv_scrollbar_info *sb = SCROLLBAR_PRIVATE(sb_public);

    /* Calculate correct position of elevator.  Paint cable and elevator. */
    sb->last_state = 0;
    scrollbar_position_elevator(sb, TRUE, SCROLLBAR_NONE);

    if (sb->size != SCROLLBAR_MINIMUM) {
	/* Paint top cable anchor */
	scrollbar_paint_anchor(sb, &sb->top_anchor_rect, sb->top_anchor_inverted);
    
	/* Paint bottom cable anchor */
	scrollbar_paint_anchor(sb, &sb->bottom_anchor_rect, sb->bottom_anchor_inverted);
    }
}


Xv_pkg          xv_scrollbar_pkg = {
    "Scrollbar",			/* seal -> package name */
    (Attr_pkg) ATTR_PKG_SCROLLBAR,	/* scrollbar attr */
    sizeof(Xv_scrollbar),		/* size of the scrollbar data struct */
    &xv_window_pkg,			/* pointer to parent */
    scrollbar_create_internal,		/* init routine for scrollbar */
    scrollbar_set_internal,		/* set routine */
    scrollbar_get_internal,		/* get routine */
    scrollbar_destroy_internal,		/* destroy routine */
    NULL				/* No find proc */
};
