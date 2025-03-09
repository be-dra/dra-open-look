#ifndef lint
char     openwin_c_sccsid[] = "@(#)openwin.c 1.37 93/06/28 DRA: $Id: openwin.c,v 4.6 2025/03/08 12:47:02 dra Exp $ ";
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
#include <xview_private/i18n_impl.h>
#include <xview_private/ow_impl.h>
#include <xview_private/draw_impl.h>
#include <xview_private/svr_impl.h>
#include <xview/defaults.h>
#include <xview/cms.h>
#include <xview/font.h>
#include <xview/cursor.h>

/*
 * Package private functions
 */

Pkg_private void openwin_remove_split(Xv_openwin_info *owin, Openwin_view_info *view);
Pkg_private int openwin_viewdata_for_sb(Xv_openwin_info *owin, Scrollbar sb, Openwin_view_info **view, Scrollbar_setting *sb_direction, int *last_sb);

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

#ifndef NO_OPENWIN_PAINT_BG
Pkg_private void openwin_set_bg_color(Openwin owin_public)
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

const Xv_pkg xv_openwin_pkg = {
    "Open Window",		/* seal -> package name */
    (Attr_pkg) ATTR_PKG_OPENWIN,/* openwin attr */
    sizeof(Xv_openwin),		/* size of the openwin data struct */
    &xv_window_pkg,		/* pointer to parent */
    openwin_init,		/* init routine for openwin */
    openwin_set,		/* set routine */
    openwin_get,		/* get routine */
    openwin_destroy,		/* destroy routine */
    NULL			/* No find proc */
};
