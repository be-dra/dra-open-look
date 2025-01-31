#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)p_scroll.c 20.14 89/07/31 Copyr 1984 Sun Micro DRA: $Id: p_scroll.c,v 4.3 2025/01/31 19:43:31 dra Exp $";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

#include <xview_private/panel_impl.h>
#include <xview/scrollbar.h>

static void normalize_top(Panel_info  *panel, int *offset);
static void normalize_left(Panel_info  *panel, int *offset);
static void normalize_bottom(Panel_info *panel, Xv_Window pw, int scrolling_up, int *offset);
static void normalize_right(Panel_info *panel, Xv_Window pw, int scrolling_up, int *offset);
static int top_pair(Panel_info *panel, int target, Item_info **low_ip, Item_info **high_ip);
static int left_pair(Panel_info *panel, int target, Item_info **low_ip, Item_info **high_ip);

/****************************************************************************/
/* panel_scroll                                                             */
/****************************************************************************/


Pkg_private void panel_normalize_scroll( Scrollbar sb, long offset,
								Scroll_motion motion, long *vs)
{
	Panel panel_public = (Panel) xv_get(sb, XV_OWNER);
	Panel_info *panel = PANEL_PRIVATE(panel_public);
	Xv_Window view;
	Xv_Window pw;
	int line_ht;
	int align_to_max, scrolling_up, vertical;
	Item_info *low_ip, *high_ip;
	int my_offset;

	vertical = (Scrollbar_setting) xv_get(sb, SCROLLBAR_DIRECTION)
			== SCROLLBAR_VERTICAL;
	line_ht = (int)xv_get(sb, SCROLLBAR_PIXELS_PER_UNIT);

	/* If everything in the panel is in view, then don't scroll. */
	if ((int)xv_get(sb, SCROLLBAR_OBJECT_LENGTH) <=
			(int)xv_get(sb, SCROLLBAR_VIEW_LENGTH))
	{
		*vs = offset;
		return;
	}

	if (line_ht != 1) {
		*vs = offset;
		return;
	}
	view = (Xv_Window) xv_get(sb, SCROLLBAR_NOTIFY_CLIENT);
	pw = (Xv_Window) xv_get(view, CANVAS_VIEW_PAINT_WINDOW);
	switch (motion) {
		case SCROLLBAR_ABSOLUTE:
		case SCROLLBAR_LINE_FORWARD:
		case SCROLLBAR_TO_START:
			align_to_max = FALSE;
			scrolling_up = TRUE;
			break;

		case SCROLLBAR_PAGE_FORWARD:
		case SCROLLBAR_TO_END:
			align_to_max = TRUE;
			scrolling_up = TRUE;
			break;

		case SCROLLBAR_POINT_TO_MIN:
			align_to_max = TRUE;
			scrolling_up = TRUE;
			break;

		case SCROLLBAR_MIN_TO_POINT:
			align_to_max = TRUE;
			scrolling_up = FALSE;
			break;

		case SCROLLBAR_PAGE_BACKWARD:
		case SCROLLBAR_LINE_BACKWARD:
		default: /* ???? */
			align_to_max = FALSE;
			scrolling_up = FALSE;
			break;
	}

	if (motion == SCROLLBAR_LINE_FORWARD || motion == SCROLLBAR_LINE_BACKWARD) {
		if (vertical) {
			(void)top_pair(panel, (int)offset, &low_ip, &high_ip);
			if (scrolling_up && high_ip)
				offset = high_ip->rect.r_top + high_ip->rect.r_height + 1;
			else if (!scrolling_up && low_ip)
				offset = low_ip->rect.r_top - 1;
		}
		else {
			(void)left_pair(panel, (int)offset, &low_ip, &high_ip);
			if (scrolling_up && high_ip)
				offset = high_ip->rect.r_left + high_ip->rect.r_width + 1;
			else if (!scrolling_up && low_ip)
				offset = low_ip->rect.r_left - 1;
		}
		if (offset < 0)	/* be sure we didn't go negative */
			offset = 0;
	}
	my_offset = (int)offset;
	if (vertical) {
		if (align_to_max)
			normalize_bottom(panel, pw, scrolling_up, &my_offset);
		else
			normalize_top(panel, &my_offset);
	}
	else {
		if (align_to_max)
			normalize_right(panel, pw, scrolling_up, &my_offset);
		else
			normalize_left(panel, &my_offset);
	}
	*vs = my_offset;
}

/*
 * Object normalization methodology: A line is drawn through all the objects
 * at the current view_offset. The two rectangles (ip's) straddling this
 * line, just above & below, are obtained.  The offset is then modified to be
 * just above or below this rectangle [by the margin length] depending on the
 * direction of scrolling and on whether or not the topmost ip intersects
 * offset.
 * 
 * low_ip +-------+ |	| <----intersects=TRUE
 * ---------------------------------------------------- offset |	|
 * +-------+ +-------+	|	| +-------+ high_ip
 * 
 * 
 */
static int top_pair(Panel_info     *panel, int             target, Item_info     **low_ip, Item_info     **high_ip)
{
    register Item_info *ip;
    register int    low_top = -1;
    register int    high_top = panel_height(panel);
    register int    top;
    int             intersects = FALSE;

    /* fix for sb neg truncation: pin at 0:target = -1 => low_ip = null */
    if (target == 0)
	target = -1;

    *high_ip = NULL;
    *low_ip = NULL;
    for (ip = panel->items; ip; ip = ip->next) {
	if (!hidden(ip)) {
	    top = ip->rect.r_top;
	    if (top <= target) {
		if (top > low_top) {
		    low_top = top;
		    *low_ip = ip;
		    intersects = (top + ip->rect.r_height > target);
		}
	    } else {
		if (top < high_top) {
		    high_top = top;
		    *high_ip = ip;
		}
	    }
	}
    }
    /*
     * if (!*high_ip) high_ip = *low_ip; else if (!*low_ip) low_ip =
     * *high_ip;
     */
    return intersects;
}

static void normalize_top(Panel_info  *panel, int *offset)
{
    Item_info      *low_ip, *high_ip;
    register int    top;
    int             intersects;

    intersects = top_pair(panel, *offset, &low_ip, &high_ip);

    if (high_ip && low_ip) {
	top = high_ip->rect.r_top;
	if (intersects)
	    top = low_ip->rect.r_top;
    } else if (low_ip)
	top = low_ip->rect.r_top;
    else
	top = 0;

    top -= panel->v_margin;
    if (top <= panel->v_margin)
	top = 0;

    *offset = top;
}

/*
 * Bottom aligned normalization methodology: Like the above but with the
 * bottoms of the rectangles used in the calculations of position and the
 * high ip determining the intersection. low_ip +-------+ |	| high_ip
 * -------+ +-------+ |	| <----intersects=TRUE
 * ---------------------------------------------------- offset |	|
 * +-------+
 * 
 * The "rounding rule" is the same: round up when scrolling down.
 */
static void normalize_bottom(Panel_info *panel, Xv_Window pw, int scrolling_up, int *offset)
{
    register Item_info *ip;
    register int    low_bottom = 0;
    register int    high_bottom = panel_height(panel);
    register int    top, bottom;
    int             target = *offset + panel_viewable_height(panel, pw);
    int             intersects = FALSE;

    for (ip = panel->items; ip; ip = ip->next) {
	if (!hidden(ip)) {
	    top = ip->rect.r_top;
	    bottom = top + ip->rect.r_height;
	    if (bottom >= target) {
		if (bottom < high_bottom) {
		    high_bottom = bottom;
		    intersects = (top < target);
		}
	    } else {
		if (bottom > low_bottom)
		    low_bottom = bottom;
	    }
	}
    }

    bottom = low_bottom;
    if (!scrolling_up && intersects)
	bottom = high_bottom;

    top = bottom + panel->v_margin - panel_viewable_height(panel, pw);
    if (top <= panel->v_margin)
	top = 0;

    *offset = top;
}

static int left_pair(Panel_info *panel, int target, Item_info **low_ip, Item_info **high_ip)
{
    register Item_info *ip;
    register int    low_left = -1;
    register int    high_left = panel_width(panel);
    register int    left;
    int             intersects = FALSE;

    /* fix for sb neg truncation: pin at 0:target = -1 => low_ip = null */
    if (target == 0)
	target = -1;

    *high_ip = NULL;
    *low_ip = NULL;
    for (ip = panel->items; ip; ip = ip->next) {
	if (!hidden(ip)) {
	    left = ip->rect.r_left;
	    if (left <= target) {
		if (left > low_left) {
		    low_left = left;
		    *low_ip = ip;
		    intersects = (left + ip->rect.r_width > target);
		}
	    } else {
		if (left < high_left) {
		    high_left = left;
		    *high_ip = ip;
		}
	    }
	}
    }
    /*
     * if (!*high_ip) high_ip = *low_ip; else if (!*low_ip) low_ip =
     * *high_ip;
     */
    return intersects;
}

static void normalize_left(Panel_info  *panel, int *offset)
{
    Item_info      *low_ip, *high_ip;
    register int    left;
    int             intersects;

    intersects = left_pair(panel, *offset, &low_ip, &high_ip);

    if (high_ip && low_ip) {
	left = high_ip->rect.r_left;
	if ( /* scrolling_up && */ intersects)
	    left = low_ip->rect.r_left;
    } else if (low_ip)
	left = low_ip->rect.r_left;
    else
	left = 0;

    left -= panel->h_margin;
    if (left <= panel->h_margin)
	left = 0;

    *offset = left;
}

static void normalize_right(Panel_info *panel, Xv_Window pw, int scrolling_up, int *offset)
{
    register Item_info *ip;
    register int    low_right = 0;
    register int    high_right = panel_width(panel);
    register int    left, right;
    int             target = *offset + panel_viewable_width(panel, pw);
    int intersects = FALSE;

    for (ip = panel->items; ip; ip = ip->next) {
	if (!hidden(ip)) {
	    left = ip->rect.r_left;
	    right = left + ip->rect.r_width;
	    if (right >= target) {
		if (right < high_right) {
		    high_right = right;
		    intersects = (left < target);
		}
	    } else {
		if (right > low_right)
		    low_right = right;
	    }
	}
    }

    right = low_right;
    if (!scrolling_up && intersects)
	right = high_right;

    left = right + panel->h_margin - panel_viewable_width(panel, pw);
    if (left <= panel->h_margin)
	left = 0;

    *offset = left;
}


/*****************************************************************************/
/* panel_update_scrolling_size                                               */
/*****************************************************************************/

Pkg_private void panel_update_scrolling_size(Panel client_panel)
{
    register Panel_info *panel = PANEL_PRIVATE(client_panel);
    register Item_info *item;
    int             v_end, h_end;

    v_end = h_end = 0;
    for (item = panel->items; item; item = item->next) {
	if (!hidden(item)) {
	    if (item->rect.r_top + item->rect.r_height > v_end)
		v_end = item->rect.r_top + item->rect.r_height;
	    if (item->rect.r_left + item->rect.r_width > h_end)
		h_end = item->rect.r_left + item->rect.r_width;
	}
    }

    if (panel_height(panel) != v_end + panel->extra_height) {
	(void) xv_set(client_panel, CANVAS_MIN_PAINT_HEIGHT, v_end + panel->extra_height, NULL);
    }
    if (panel_width(panel) != h_end + panel->extra_width) {
	(void) xv_set(client_panel, CANVAS_MIN_PAINT_WIDTH, h_end + panel->extra_width, NULL);
    }
}
