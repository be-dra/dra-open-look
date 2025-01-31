#ifndef lint
#ifdef sccs
char     scrn_get_c_sccsid[] = "@(#)scrn_get.c 20.54 93/06/28 DRA: RCS $Id: scrn_get.c,v 4.8 2025/01/10 10:13:05 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL_NOTICE 
 *	file for terms of the license.
 */

#include <xview_private/i18n_impl.h>
#include <xview_private/portable.h>
#include <xview_private/scrn_impl.h>
#include <xview_private/draw_impl.h>
#include <xview/notify.h>
#include <xview/win_input.h>
#include <xview/win_screen.h>
#include <xview/base.h>
#include <xview/defaults.h>
#include <xview/font.h>
#include <xview/server.h>
#include <X11/Xatom.h>

/* Bitmap used for the inactive GC */
static unsigned short screen_gray50_bitmap[16] = {   /* 50% gray pattern */
    0xAAAA, 0x5555, 0xAAAA, 0x5555, 0xAAAA, 0x5555, 0xAAAA, 0x5555,
    0xAAAA, 0x5555, 0xAAAA, 0x5555, 0xAAAA, 0x5555, 0xAAAA, 0x5555
};

static GC *screen_get_cached_gc_list(Screen_info *screen, Xv_Window window);

/* Caller turns varargs into va_list that has already been va_start'd */
/*ARGSUSED*/
Pkg_private Xv_opaque screen_get_attr(Xv_Screen screen_public, int *status,
									Attr_attribute attr, va_list valist)
{
	Screen_info *screen = SCREEN_PRIVATE(screen_public);
	Xv_opaque value = (Xv_opaque) NULL;

	switch (attr) {
		/* Public Attributes */

		case SCREEN_SERVER:
			value = (Xv_opaque) screen->server;
			break;

		case SCREEN_OLGC_LIST:
			{
				Xv_opaque window;

				window = va_arg(valist, Xv_opaque);
				if (xv_get(window, XV_IS_SUBTYPE_OF, WINDOW))
					value = (Xv_opaque) screen_get_cached_gc_list(screen,
							window);
				else
					*status = XV_ERROR;
			}
			break;

		case SCREEN_CHECK_SUN_WM_PROTOCOL:
			{
				int	i;
				char *name = va_arg(valist, char *);
				Atom at = (Atom)xv_get(screen->server, SERVER_ATOM, name);

				for (i = 0; i < screen->num_sun_wm_protocols; i++) {
					if (screen->sun_wm_protocols[i] == at) {
						return (Xv_opaque)TRUE;
					}
				}

				value = (Xv_opaque)FALSE;
			}
			break;

		case SCREEN_OLWM_MANAGED:
			if (screen->olwm_managed < 0) {
				screen_update_sun_wm_protocols(screen->root_window, FALSE);
    			screen->olwm_managed = (screen->num_sun_wm_protocols > 0);
			}
			value = (Xv_opaque) screen->olwm_managed;
			break;

		case SCREEN_ENHANCED_OLWM:
			value = xv_get(screen_public, SCREEN_CHECK_SUN_WM_PROTOCOL,
												"_DRA_ENHANCED_OLWM");
			break;

		case SCREEN_NUMBER:
			value = (Xv_opaque) screen->number;
			break;

		case SCREEN_UI_STYLE:
			value = (Xv_opaque) screen->ui_style;
			break;

		case SCREEN_INPUT_PIXEL:
			value = (Xv_opaque) screen->input_pixel;
			break;

		case XV_ROOT:
			value = (Xv_opaque) screen->root_window;
			break;

		/* Private Attributes */

		case SCREEN_DEFAULT_CMS:
			value = (Xv_opaque) (screen->default_cms);
			break;

		case SCREEN_RETAIN_WINDOWS:
			value = (Xv_opaque) screen->retain_windows;
			break;


		case SCREEN_DEFAULT_VISUAL:
			value = (Xv_opaque) & (screen->screen_visuals[0]);
			break;

		case SCREEN_VISUAL:
			{
				XVisualInfo *vinfo;
				XVisualInfo *template;
				long vinfo_mask;

				vinfo_mask = va_arg(valist, long);

				template = va_arg(valist, XVisualInfo *);
				vinfo = screen_match_visual_info(screen, vinfo_mask, template);
				value = (Xv_opaque) screen_get_visual(
						(Display *) xv_get(screen->server, XV_DISPLAY),
						screen, vinfo);
			}
			break;

		case SCREEN_IMAGE_VISUAL:
			{
				XID xid;
				unsigned int depth;
				Display *display =
						(Display *) xv_get(screen->server, XV_DISPLAY);

				xid = va_arg(valist, XID);
				depth = va_arg(valist, unsigned int);

				value = (Xv_opaque) screen_get_image_visual(display, screen,
						xid, depth);
			}
			break;

		case SCREEN_SUN_WINDOW_STATE:
			value = xv_get(screen_public, SCREEN_CHECK_SUN_WM_PROTOCOL,
												"_SUN_WINDOW_STATE");
			break;

		case SCREEN_SELECTION_STATE:
			/* always 0, de facto unsupported */
			value = XV_NULL;
			break;

		case SCREEN_BUSY_CURSOR:
			if (! screen->busy_pointer) {
				screen->busy_pointer = xv_create(screen_public, CURSOR,
										CURSOR_SRC_CHAR, OLC_BUSY_PTR,
										CURSOR_MASK_CHAR, OLC_BUSY_MASK_PTR,
										NULL);
			}
			value = screen->busy_pointer;
			break;

		default:
			if (xv_check_bad_attr(&xv_screen_pkg, attr) == XV_ERROR) {
				*status = XV_ERROR;
			}
	}
	return value;
}

/* 
 * screen_match_visual_info - screen wrapper around XGetVisualInfo, but only
 *     using the visualid, depth and class of the template/mask.
 */
Pkg_private XVisualInfo *screen_match_visual_info(Screen_info *screen, long mask, XVisualInfo *template)
{
    XVisualInfo *best_match = (XVisualInfo *)NULL;
    XVisualInfo *match;
    int visual;
    int default_depth;

    if (screen->screen_visuals)
      default_depth = screen->screen_visuals->depth;
    else
      default_depth = DefaultDepth((Display *)xv_get(screen->server, XV_DISPLAY), 
				   screen->number);
    
    for (visual = 0; visual < screen->num_visuals; visual++) {
	/* Check visualid */
	if ((mask & VisualIDMask) && 
	    (template->visualid != screen->visual_infos[visual].visualid))
	  continue;

	/* Check class */
	if ((mask & VisualClassMask) &&
	    (template->class != screen->visual_infos[visual].class))
	  continue;

	/* Check depth */
	if ((mask & VisualDepthMask) &&
	    (template->depth != screen->visual_infos[visual].depth))
	  continue;
	
	/* Found one */
	match = &(screen->visual_infos[visual]);
	if (!best_match)
	  best_match = match;

	/* 
	 * If they asked for a specific Visual ID, or they specified both the
	 * depth and class, just return the first one found.  Otherwise, keep 
	 * searching.
	 */
	if ((mask & VisualIDMask) || 
	    ((mask & VisualDepthMask) && (mask & VisualClassMask)))
	  return(best_match);
	else if (match != best_match) {
	    if (mask & VisualClassMask) {
		/* They only specified the class, so favor the default depth,
		 * or the highest depth visual with that class.
		 */
		if (match->depth == default_depth)
		  best_match = match;
		else if ((best_match->depth != default_depth) &&
			 (best_match->depth < match->depth))
		  best_match = match;
	    } else {
		/* They only specified the depth, so favor the highest class
		 * visual with that depth. However, we special case TrueColor
		 * to be favored over DirectColor because if someone was just
		 * trying to get at a 24 bit visual, they most likely want the
		 * TrueColor visual, and not the DirectColor.
		 */
		if ((best_match->class == DirectColor) &&
		    (match->class == TrueColor))
		  best_match = match;
		else if ((best_match->class < match->class) &&
			 ((best_match->class != TrueColor) ||
			  (match->class != DirectColor)))
		  best_match = &(screen->visual_infos[visual]);
	    }
	    
	}
    }
    return(best_match);
}


/*
 * Windows are cached in the screen and shared between menus.
 */
Xv_private Xv_Window screen_get_cached_window(Xv_Screen screen_public,
    Notify_func	event_proc, int borders, int transp, Visual *visual,
    int	*new_window)
{
    Screen_info    *screen = SCREEN_PRIVATE(screen_public);
    Xv_cached_window *cached_window;

    for (cached_window = screen->cached_windows; cached_window != NULL;
	 cached_window = cached_window->next) {
	if (cached_window->busy == FALSE &&
	    cached_window->borders == (short) borders &&
	    cached_window->transparent == (short) transp &&
	    XVisualIDFromVisual(cached_window->visual) == 
	        XVisualIDFromVisual(visual)) {
	    cached_window->busy = TRUE;
	    *new_window = FALSE;
	    return ((Xv_Window) cached_window->window);
	}
    }

    *new_window = TRUE;
    cached_window = (Xv_cached_window *) xv_alloc(Xv_cached_window);

	if (transp) {
    	cached_window->window = (Xv_Window) xv_create(
					xv_get(screen_public, XV_ROOT), WINDOW,
					WIN_BIT_GRAVITY, ForgetGravity,
					WIN_TRANSPARENT,
					WIN_BORDER, borders,
        			XV_VISUAL, visual,						  
					WIN_NOTIFY_SAFE_EVENT_PROC, event_proc,
					WIN_TOP_LEVEL_NO_DECOR, TRUE,
					WIN_SAVE_UNDER, TRUE,
					XV_SHOW, FALSE,
					NULL);
	}
	else {
    	cached_window->window = (Xv_Window) xv_create(
					xv_get(screen_public, XV_ROOT), WINDOW,
					WIN_BIT_GRAVITY, ForgetGravity,
					WIN_BORDER, borders,
        			XV_VISUAL, visual,						  
					WIN_NOTIFY_SAFE_EVENT_PROC, event_proc,
					WIN_TOP_LEVEL_NO_DECOR, TRUE,
					WIN_SAVE_UNDER, TRUE,
					XV_SHOW, FALSE,
					NULL);
	}

	{
		Xv_server srv = xv_get(screen_public, SCREEN_SERVER);
		Display *dpy = (Display *)xv_get(srv, XV_DISPLAY);
		Atom nwt[2];
		Window win;

		win = xv_get(cached_window->window, XV_XID);
		nwt[0] = xv_get(srv, SERVER_ATOM, "_NET_WM_WINDOW_TYPE_POPUP_MENU");
		XChangeProperty(dpy, win,
				xv_get(srv, SERVER_ATOM, "_NET_WM_WINDOW_TYPE"),
				XA_ATOM, 32, PropModeReplace, (unsigned char *)nwt, 1);
	}

    if (screen->cached_windows == NULL) {
		screen->cached_windows = cached_window;
    }
	else {
		cached_window->next = screen->cached_windows;
		screen->cached_windows = cached_window;
    }
    cached_window->busy = TRUE;
    cached_window->borders = (short) borders;
    cached_window->transparent = (short) transp;
    cached_window->visual = visual;
    return ((Xv_Window) cached_window->window);
}

#define lowbit(x) ((x) & (~(x) + 1))

static GC *screen_get_cached_gc_list(Screen_info *screen, Xv_Window window)
{
	Display *dpy;
	Xv_Drawable_info *info;
	Screen_OLGC_List *gc_list = screen->gc_list;
	Screen_OLGC_List *new_gc_list;
	XGCValues gc_value;
	unsigned long gc_value_mask;
	int gc;
	Xv_Font std_font, bold_font;
	Pixmap gray50, tmpbit = None;
	Drawable gcxid;

	/* Search the screen's cached gc_list list and see if
	 * there is a suitable list for this window.
	 */
	DRAWABLE_INFO_MACRO(window, info);
	while (gc_list && (gc_list->depth != xv_depth(info)))
		gc_list = gc_list->next;
	if (gc_list)
		return (gc_list->gcs);

	dpy = xv_display(info);
	gcxid = xv_xid(info);

	/* None was found, so create a new one */
	new_gc_list = (Screen_OLGC_List *) xv_alloc(Screen_OLGC_List);
	new_gc_list->depth = xv_depth(info);
	new_gc_list->next = screen->gc_list;
	screen->gc_list = new_gc_list;

	gray50 = XCreateBitmapFromData(dpy, xv_xid(info),
						(char *)screen_gray50_bitmap, 16, 16);


	/* Create each of the GCs for the list */
	std_font = (Xv_Font) xv_get(window, XV_FONT);
	for (gc = 0; gc < SCREEN_OLGC_LIST_SIZE; gc++) {
		gc_value.foreground = xv_fg(info);
		gc_value.background = xv_bg(info);
		gc_value.function = GXcopy;
		gc_value.plane_mask = xv_plane_mask(info);
		gc_value.graphics_exposures = FALSE;
		gc_value_mask = GCForeground | GCBackground | GCFunction |
				GCPlaneMask | GCGraphicsExposures;
		switch (gc) {
			case SCREEN_SET_GC:
			case SCREEN_NONSTD_GC:
				break;
			case SCREEN_CLR_GC:
				gc_value.foreground = xv_bg(info);
				break;
			case SCREEN_TEXT_GC:

#ifdef OW_I18N
				/* do nothing since using font sets always */
#else /* OW_I18N */
				gc_value.font = (Font) xv_get(std_font, XV_XID);
				gc_value_mask |= GCFont;
#endif /* OW_I18N */

				break;
			case SCREEN_BOLD_GC:

#ifdef OW_I18N
				/* do nothing since using font sets always */
#else /* OW_I18N */
				bold_font = (Xv_Font) xv_find(window, FONT,
						FONT_FAMILY, xv_get(std_font, FONT_FAMILY),
						FONT_STYLE, FONT_STYLE_BOLD,
						FONT_SIZE, xv_get(std_font, FONT_SIZE), NULL);
				if (bold_font == XV_NULL) {
					xv_error(XV_NULL,
							ERROR_STRING,
							XV_MSG
							("Unable to find bold font; using standard font"),
							ERROR_PKG, SCREEN, NULL);
					bold_font = std_font;
				}
				gc_value.font = (Font) xv_get(bold_font, XV_XID);
				gc_value_mask |= GCFont;
#endif /* OW_I18N */

				break;
			case SCREEN_GLYPH_GC:
				gc_value.font = (Font) xv_get(xv_get(window, WIN_GLYPH_FONT),
						XV_XID);
				gc_value_mask |= GCFont;
				break;
			case SCREEN_INACTIVE_GC:
				gc_value.foreground = xv_bg(info);
				gc_value.background = xv_fg(info);
				gc_value.stipple = gray50;
				gc_value.fill_style = FillStippled;
				gc_value_mask |= GCStipple | GCFillStyle;
				break;
			case SCREEN_DIM_GC:
				gc_value.line_style = LineDoubleDash;
				gc_value.dashes = 1;
				gc_value_mask |= GCLineStyle | GCDashList;
				break;
			case SCREEN_INVERT_GC:
				gc_value.function = GXinvert;
				gc_value.plane_mask = gc_value.foreground ^ gc_value.background;
				break;
			case SCREEN_RUBBERBAND_GC:
				gc_value.subwindow_mode = IncludeInferiors;
				gc_value.function = GXinvert;
				gc_value_mask |= GCSubwindowMode | GCFunction;
				break;

			case SCREEN_CARET_GC:
				if (screen->ui_style != SCREEN_UIS_2D_BW
					&& (xv_visual(info)->vinfo->class == TrueColor ||
						xv_visual(info)->vinfo->class == DirectColor))
				{
					XColor color;
					char *col = defaults_get_string("openWindows.inputColor",
                        	"OpenWindows.InputColor", "#000000");

					if (!XParseColor(dpy,
							DefaultColormap(dpy, screen->number),
							col, &color))
					{
						xv_error(XV_NULL,
							ERROR_STRING,
							XV_MSG("Unable to find RGB values for a named color"),
							ERROR_PKG, SCREEN,
							NULL);
						gc_value.foreground = xv_fg(info);
					}
					else {
						XAllocColor(dpy,
							DefaultColormap(dpy, screen->number),
							&color);
						gc_value.foreground = color.pixel;
						screen->input_pixel = color.pixel;
					}
				}

				gc_value.font = (Font) xv_get(xv_get(window, WIN_GLYPH_FONT),
						XV_XID);
				gc_value_mask |= GCFont;
				break;

			case SCREEN_HELP_GC:
				gc_value.subwindow_mode = IncludeInferiors;
				gc_value_mask |= GCSubwindowMode;
				break;
			case SCREEN_MENUSHADOW_GC:
				if (screen->ui_style != SCREEN_UIS_2D_BW
					&& (xv_visual(info)->vinfo->class == TrueColor ||
						xv_visual(info)->vinfo->class == DirectColor))
				{
					XVisualInfo *vis = xv_visual(info)->vinfo;
					unsigned int rm, gm, bm, rf, gf, bf;

					rm = vis->red_mask / lowbit(vis->red_mask) + 1;
					gm = vis->green_mask / lowbit(vis->green_mask) + 1;
					bm = vis->blue_mask / lowbit(vis->blue_mask) + 1;

					/* die rm etc sind alle = 0x100 */

					rf = rm / (gm | bm);
					if (rf < 1) rf = 1;
					gf = gm / (rm | bm);
					if (gf < 1) gf = 1;
					bf = bm / (gm | rm);
					if (bf < 1) bf = 1;

					/* die rf etc sind alle = 1 */

					/* that '0x95' is not at all a secret - it is just the
					 * result of many tryouts
					 */
					gc_value.foreground =
							(vis->red_mask
								& (rf * 0x95 * lowbit(vis->red_mask))) +
							(vis->green_mask
								& (gf * 0x95 * lowbit(vis->green_mask))) +
							(vis->blue_mask
								& (bf * 0x95 * lowbit(vis->blue_mask)));
					/* damit wird das 0x959595 - ein simples Ergebnis
					 * fuer soviel Rechnerei
					 */

					gc_value.function = GXand;
					gc_value_mask = GCForeground | GCFunction;
				}
				else {
					gc_value.stipple = gray50;
					gc_value.fill_style = FillStippled;
					gc_value_mask |= GCFillStyle | GCStipple;
				}
				break;
			case SCREEN_JOINPREVIEW_GC:
				gc_value.foreground = WhitePixel(dpy,
													screen->number);
				gc_value.stipple = gray50;
				gc_value.fill_style = FillStippled;
				gc_value.subwindow_mode = IncludeInferiors;
				gc_value_mask |= GCFillStyle | GCStipple | GCSubwindowMode;
				break;
			case SCREEN_BITMAP_GC:
				tmpbit = XCreatePixmap(dpy, RootWindow(dpy,screen->number),
											1, 1, 1);
				gc_value.background = 0;
				gc_value.foreground = 1;
				gc_value_mask = GCBackground | GCForeground;
				gcxid = tmpbit;
				break;
		}
		new_gc_list->gcs[gc] = XCreateGC(dpy, gcxid, gc_value_mask, &gc_value);
	}
	XFreePixmap(dpy, gray50);
	if (tmpbit != None) XFreePixmap(dpy, tmpbit);
	return (new_gc_list->gcs);
}

Xv_private void screen_adjust_gc_color(Xv_Window window, int gc_index)
{
	Xv_Drawable_info *info;
	unsigned long new_fg;
	unsigned long new_bg;
	unsigned long new_plane_mask;
	GC *gc_list;

#if XlibSpecificationRelease >= 5
	XGCValues gc_tmp;
#endif

	DRAWABLE_INFO_MACRO(window, info);
	new_plane_mask = xv_plane_mask(info);

	new_bg = xv_bg(info);
	new_fg = xv_fg(info);

	gc_list = (GC *) xv_get(xv_screen(info), SCREEN_OLGC_LIST, window);
	switch (gc_index) {
		case SCREEN_SET_GC:
		case SCREEN_NONSTD_GC:
		case SCREEN_TEXT_GC:
		case SCREEN_BOLD_GC:
		case SCREEN_GLYPH_GC:
		case SCREEN_DIM_GC:
		case SCREEN_CARET_GC:
			break;
		case SCREEN_INVERT_GC:
			new_fg = xv_fg(info);
			new_plane_mask = new_fg ^ new_bg;
			break;
		case SCREEN_CLR_GC:
			new_fg = new_bg = xv_bg(info);
			break;
		case SCREEN_INACTIVE_GC:
			new_fg = xv_bg(info);
			new_bg = xv_fg(info);
			break;
	}

#if XlibSpecificationRelease >= 5
	XGetGCValues(xv_display(info), gc_list[gc_index],
			GCPlaneMask | GCForeground | GCBackground, &gc_tmp);

	if ((new_fg != gc_tmp.foreground) ||
			(new_bg != gc_tmp.background) ||
			(new_plane_mask != gc_tmp.plane_mask)) {
		gc_tmp.foreground = new_fg;
		gc_tmp.background = new_bg;
		gc_tmp.plane_mask = new_plane_mask;
		XChangeGC(xv_display(info), gc_list[gc_index],
				GCForeground | GCBackground | GCPlaneMask, &gc_tmp);
	}
#else
	XGCValues gc_values;

	if (new_fg != gc_list[gc_index]->values.foreground ||
			new_bg != gc_list[gc_index]->values.background ||
			new_plane_mask != gc_list[gc_index]->values.plane_mask) {
		gc_values.foreground = new_fg;
		gc_values.background = new_bg;
		gc_values.plane_mask = new_plane_mask;
		XChangeGC(xv_display(info), gc_list[gc_index],
				GCForeground | GCBackground | GCPlaneMask, &gc_values);
	}
#endif
}
