#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)screen.c 20.51 93/06/28 DRA: RCS $Id: screen.c,v 4.14 2025/03/03 19:08:33 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL_NOTICE 
 *	file for terms of the license.
 */

#include <xview_private/i18n_impl.h>
#include <xview_private/scrn_impl.h>
#include <xview_private/scrn_vis.h>
#include <xview_private/xview_impl.h>
#include <xview_private/svr_impl.h>
#include <xview_private/cms_impl.h>
#include <xview_private/windowimpl.h>
#include <xview_private/win_info.h>
#include <xview/notify.h>
#include <xview/server.h>
#include <xview/cms.h>
#include <xview/window.h>
#include <xview/defaults.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#define VisualClassError -1
static Defaults_pairs visual_class_pairs[] = {
    { "StaticGray",  StaticGray },
    { "GrayScale",   GrayScale },
    { "StaticColor", StaticColor },
    { "PseudoColor", PseudoColor },
    { "TrueColor",   TrueColor },
    { "DirectColor", DirectColor },
    { NULL,          VisualClassError }
};

static Defaults_pairs ui_styles[] = {
	{ "two_d_bw", SCREEN_UIS_2D_BW },
	{ "two_d_color", SCREEN_UIS_2D_COLOR },
	{ "three_d_color", SCREEN_UIS_3D_COLOR },
	/* in former times this was boolean: */
	{ "false", SCREEN_UIS_2D_BW },
	{ "true", SCREEN_UIS_3D_COLOR },
	{ (char *)0, SCREEN_UIS_3D_COLOR }
};

static unsigned short screen_gray50_bitmap[16] = {   /* 50% gray pattern */
    0xAAAA, 0x5555, 0xAAAA, 0x5555, 0xAAAA, 0x5555, 0xAAAA, 0x5555,
    0xAAAA, 0x5555, 0xAAAA, 0x5555, 0xAAAA, 0x5555, 0xAAAA, 0x5555
};

typedef struct scrn_cache_window {
    Xv_window			window;
    short			busy;
    short			borders;
    Visual			*visual;
	short           transparent;
    struct scrn_cache_window	*next;
} Xv_cached_window;

typedef struct scrn_gc_list {
    int			 depth;
    GC			 gcs[SCREEN_OLGC_LIST_SIZE];
    struct scrn_gc_list *next;
} Screen_OLGC_List;

struct screen_visual;

typedef struct _screen_info {
    Xv_Screen		 public_self;		/* back pointer */
    int			 number;		/* screen number on server */
    Xv_opaque		 server;		/* always a Server */
    Xv_opaque		 root_window;		/* always a Window */
    XVisualInfo	        *visual_infos;	 	/* List of available visuals
						 * for this screen */
    int			 num_visuals;	  	/* number of available visuals
						 * for this screen */
    struct screen_visual *screen_visuals;	/* list of screen visuals
						 * (first is always default) */
    Cms			 default_cms;		
    Xv_xrectlist         clip_xrects;    	/* clipping rectangle list */
    Xv_cached_window    *cached_windows;
    short                retain_windows;  	/* retain leaf windows for
						 * perf */
    Screen_OLGC_List	*gc_list;		/* List of gc lists */
    Atom *sun_wm_protocols;	/* Sun specific WM_PROTOCOLS */
    int	num_sun_wm_protocols;	/* No. of protocols in above */
	unsigned long input_pixel;
	int olwm_managed;
	screen_ui_style_t ui_style;
	/* atoms I want to know in order to handle them */
	Atom sun_wm_p;
	Xv_Cursor busy_pointer;
} Screen_info;

#define	SCREEN_PRIVATE(screen) XV_PRIVATE(Screen_info, Xv_screen_struct, screen)
#define	SCREEN_PUBLIC(screen)	XV_PUBLIC(screen)

static Screen_visual * screen_new_visual(Display *display, Screen_info *screen,
					XID xid, unsigned int depth, XVisualInfo *visual_info)
{
	Screen_visual *visual;
	GC  gc;
	XGCValues gc_values;

	/* Make sure colors are correct in gc. */
	/* BUG ALERT!  This is not handling color properly!!! */
	gc_values.foreground = BlackPixel(display, screen->number);
	gc_values.background = WhitePixel(display, screen->number);
	gc = XCreateGC(display, xid, GCForeground | GCBackground, &gc_values);
	if (!gc)
		return (Screen_visual *) NULL;

	visual = xv_alloc(Screen_visual);
	visual->screen = SCREEN_PUBLIC(screen);
	visual->server = screen->server;
	visual->display = display;
	visual->root_window = screen->root_window;
	visual->vinfo = visual_info;
	visual->depth = depth;
	if (visual_info == (XVisualInfo *) NULL)
		visual->colormaps = (Xv_opaque) NULL;
	else
		visual->colormaps = cms_default_colormap(screen->server, display,
									screen->number, visual_info);
	visual->gc = gc;
	visual->image_bitmap = (XImage *) NULL;
	visual->image_pixmap = (XImage *) NULL;

	visual->next = (Screen_visual *) NULL;
	return (visual);
}

static Screen_visual * screen_get_visual(Display *display, Screen_info *screen,
							XVisualInfo *visual_info)
{
    Screen_visual *visual = screen->screen_visuals;
    
    /* Sanity check */
    if (!visual_info) {
	visual = NULL;
    } else {

	/* See if visual has already been created */
	while (visual && (visual->vinfo != visual_info))
	  visual = visual->next;
	
	/* If we didn't find one, create it and add it to the screen's list */
	if (!visual) {
	    visual = screen_new_visual(display, screen, 
				       RootWindow(display, screen->number),
				       (unsigned)visual_info->depth, visual_info);
	    if (visual) {
		/* The first screen visual is the default, which is always there */
		visual->next = screen->screen_visuals->next;
		screen->screen_visuals->next = visual;
	    }
	}
    }
    return(visual);
}


static Screen_visual * screen_get_image_visual(Display *display,
						Screen_info *screen, XID xid, unsigned int depth)
{
    Screen_visual *visual = screen->screen_visuals;
    
    /* See if visual has already been created */
    while (visual && (visual->vinfo || (visual->depth != depth)))
      visual = visual->next;
    
    /* If we didn't find one, create it and add it to the screen's list */
    if (!visual) {
	visual = screen_new_visual(display, screen, xid, depth, NULL);
	if (visual) {
	    /* The first screen visual is the default, which is always there */
	    visual->next = screen->screen_visuals->next;
	    screen->screen_visuals->next = visual;
	}
    }
    return(visual);
}

static int screen_layout(Xv_Window root, Xv_Window child, Window_layout_op op,
		Xv_opaque d1, Xv_opaque d2, Xv_opaque d3, Xv_opaque d4, Xv_opaque d5)
{
	int top_level = (int)xv_get(child, WIN_TOP_LEVEL);
	int result;

	/*
	 * use the default if not a top_level win, but don't insert on
	 * WIN_CREATE.
	 */
	if (!top_level)
		return (op == WIN_CREATE) ?
				FALSE : window_layout(root, child, op, d1, d2, d3, d4, d5);

	switch (op) {
		case WIN_CREATE:
			return FALSE;

		case WIN_GET_BELOW:
			/*
			 * window_getrelrect(child, (Xv_Window) d1, &rect); rect1 = *(Rect *)
			 * xv_get(child,WIN_RECT); rect1.r_top = rect.r_top + rect.r_height +
			 * FRAME_BORDER_WIDTH; d1 = (int) &rect1;
			 */
			/* bogus -- portability problems */
			/*
			 * op = WIN_ADJUST_RECT;
			 */
			break;

		case WIN_GET_RIGHT_OF:
			/*
			 * window_getrelrect(child, (Xv_Window) d1, &rect); rect1 = *(Rect *)
			 * xv_get(child,WIN_RECT); rect1.r_left = rect.r_left + rect.r_width
			 * + FRAME_BORDER_WIDTH; d1 = (int) &rect1;
			 */
			/* bogus -- portability problems */
			/*
			 * op = WIN_ADJUST_RECT;
			 */
			break;

		case WIN_ADJUST_RECT:
			if (xv_get(child, XV_IS_SUBTYPE_OF, FRAME_CLASS)) {
				Rect *r = (Rect *) d1;
				Rect real_size;
				int rect_info = (int)xv_get(child, WIN_RECT_INFO);

				if (!(rect_info & WIN_HEIGHT_SET)) {
					win_getsize(child, &real_size);
					r->r_height = real_size.r_height;
				}
			}
			break;

		default:
			break;
	}

	if ((op == WIN_ADJUST_RECT) &&
			(top_level) && !(Bool) xv_get(child, WIN_TOP_LEVEL_NO_DECOR)) {

		typedef int (*layout_proc_t)(Xv_Window,Xv_Window,Window_layout_op,Xv_opaque,Xv_opaque,Xv_opaque,Xv_opaque,Xv_opaque);
		layout_proc_t layout_proc;

		layout_proc = (layout_proc_t)xv_get(child, WIN_LAYOUT_PROC);
		result = layout_proc(root, child, op, d1, d2, d3, d4, d5);
	}
	else
		result = window_layout(root, child, op, d1, d2, d3, d4, d5);
	return result;
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

/* 
 * screen_match_visual_info - screen wrapper around XGetVisualInfo, but only
 *     using the visualid, depth and class of the template/mask.
 */
static XVisualInfo *screen_match_visual_info(Screen_info *screen, long mask, XVisualInfo *template)
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

static void screen_update_sun_wm_protocols(Xv_object window, int is_delete)
{
	Xv_Screen screen_public = XV_SCREEN_FROM_WINDOW(window);
	Screen_info *screen = SCREEN_PRIVATE(screen_public);
	int format;
	Atom type;
	unsigned long nitems, bytes_after;
	Display *dpy = (Display *) xv_get(screen->server, XV_DISPLAY);

	if (screen->sun_wm_protocols) {
		XFree((char *)screen->sun_wm_protocols);
		screen->num_sun_wm_protocols = 0;
	}

	if (is_delete) {
		screen->sun_wm_protocols = NULL;
		return;
	}

	/* Store away any sun specific WM_PROTOCOLS. */
	if (XGetWindowProperty(dpy, RootWindow(dpy, screen->number),
					(Atom) xv_get(screen->server, SERVER_ATOM,
												"_SUN_WM_PROTOCOLS"),
					0L, 100L, False, XA_ATOM, &type, &format,
					&nitems, &bytes_after,
					(unsigned char **)(&screen->sun_wm_protocols))
			!= Success) {

		screen->sun_wm_protocols = NULL;
	}
	else if (type == None) {
		screen->sun_wm_protocols = NULL;
	}
	else {
#ifdef WHAT_ABOUT_OTHER_PROPERTIES
		/* idiotic: we might be interested in OTHER property changes */
		XWindowAttributes xwa;

		XGetWindowAttributes(dpy, RootWindow(dpy, screen->number), &xwa);
		xwa.your_event_mask ^= PropertyChangeMask;
		XSelectInput(dpy, RootWindow(dpy, screen->number), xwa.your_event_mask);
#endif /* WHAT_ABOUT_OTHER_PROPERTIES */
		screen->num_sun_wm_protocols = nitems;
	}
}

/* Caller turns varargs into va_list that has already been va_start'd */
static Xv_opaque screen_get_attr(Xv_Screen screen_public, int *status,
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
 * Windows are cached in the screen and shared between menus.
 */
Xv_private Xv_Window screen_get_cached_window(Xv_Screen screen_public,
    Notify_func	event_proc, int borders, int transp, Visual *visual,
    int	*is_new_window)
{
	Screen_info *screen = SCREEN_PRIVATE(screen_public);
	Xv_cached_window *cached_window;

	for (cached_window = screen->cached_windows; cached_window != NULL;
			cached_window = cached_window->next) {
		if (cached_window->busy == FALSE &&
				cached_window->borders == (short)borders &&
				cached_window->transparent == (short)transp &&
				XVisualIDFromVisual(cached_window->visual) ==
				XVisualIDFromVisual(visual)) {
			cached_window->busy = TRUE;
			*is_new_window = FALSE;
			return ((Xv_Window) cached_window->window);
		}
	}

	*is_new_window = TRUE;
	cached_window = (Xv_cached_window *) xv_alloc(Xv_cached_window);

	if (transp) {
		cached_window->window = xv_create(xv_get(screen_public,XV_ROOT), WINDOW,
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
		cached_window->window = xv_create(xv_get(screen_public,XV_ROOT), WINDOW,
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
		Display *dpy = (Display *) xv_get(srv, XV_DISPLAY);
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
	cached_window->borders = (short)borders;
	cached_window->transparent = (short)transp;
	cached_window->visual = visual;
	return cached_window->window;
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
static Screen_visual * screen_default_visual(Display *display, Screen_info *screen);
static XVisualInfo *screen_default_visual_info(Display	*display, Screen_info *screen);
static void screen_input(Xv_server server, Display *dpy, XPropertyEvent	*xev,	Xv_opaque obj);

typedef enum {
	XvNewValue,
	XvDeleted
} Xv_sel_state;

static  int screen_init(Xv_opaque parent, Xv_Screen screen_public,
								Attr_avlist avlist, int *u)
{
	Screen_info *screen;
	Attr_avlist attrs;
	Xv_screen_struct *screen_object;
	Xv_object font;
	XVisualInfo visual_template;
	Display *display;
	char cms_name[100];
	Font font_id;

	/* Allocate private data and set up forward/backward links. */
	screen = (Screen_info *) xv_alloc(Screen_info);
	screen->public_self = screen_public;
	screen_object = (Xv_screen_struct *) screen_public;
	screen_object->private_data = (Xv_opaque) screen;

	screen->server = parent ? parent : xv_default_server;
	display = (Display *) xv_get(screen->server, XV_DISPLAY);
	screen->number = DefaultScreen(display);
	screen->gc_list = (Screen_OLGC_List *) NULL;
	screen->sun_wm_protocols = NULL;
	screen->num_sun_wm_protocols = 0;

	screen->sun_wm_p = xv_get(screen->server, SERVER_ATOM, "_SUN_WM_PROTOCOLS");

	for (attrs = avlist; *attrs; attrs = attr_next(attrs)) {
		switch ((int)attrs[0]) {
			case SCREEN_NUMBER:
				if ((int)attrs[1] >= ScreenCount(display)) {
					xv_error(XV_NULL,
							ERROR_BAD_VALUE, attrs[1], attrs[0],
							ERROR_PKG, SCREEN,
							NULL);
					xv_free(screen);
					return XV_ERROR;
				}
				screen->number = (int)attrs[1];
				ATTR_CONSUME(attrs[0]);
				break;

			default:
				break;
		}
	}

	/* get information about all visuals supported by this window */
	visual_template.screen = screen->number;
	screen->visual_infos = XGetVisualInfo(display, (long)VisualScreenMask,
			&visual_template, &(screen->num_visuals));

	/* get the default visual from the list */
	screen->screen_visuals = screen_default_visual(display, screen);

	/* This should be changed to make the default colors controlled by 
	 * -fg, and -bg.
	 */
	sprintf(cms_name, "xv_default_cms_for_0x%lx",
			screen->screen_visuals->vinfo->visualid);
	screen->default_cms = (Cms) xv_create(screen_public, CMS,
			CMS_NAME, cms_name,
			XV_VISUAL, screen->screen_visuals->vinfo->visual,
			CMS_TYPE, XV_STATIC_CMS,
			CMS_SIZE, 2,
			CMS_NAMED_COLORS, "White", "Black", NULL,
			CMS_DEFAULT_CMS, TRUE,
			NULL);

	/*
	 * By default, monochrome leaf windows (ie. windows into which bits are
	 * actually written) are set to non-retained. This can be turned to retained
	 *  by a cmdline arg for debugging.
	 */
	if (DisplayPlanes(display, screen->number) > 1) {
		screen->retain_windows = FALSE;
	}
	else {
		screen->retain_windows =
				!(defaults_get_boolean("window.mono.disableRetained",
						"Window.Mono.DisableRetained", TRUE));
	}

	/* set the default font in the GC for this screen */
	font = (Xv_object) xv_get(screen->server, SERVER_FONT_WITH_NAME, NULL,
			NULL);
	if (!font) {
		XFree((char *)(screen->visual_infos));
		xv_free(screen);
		return XV_ERROR;
	}
	font_id = xv_get(font, XV_XID);
	xv_set_default_font((Display *) xv_get(screen->server, XV_DISPLAY),
			screen->number, (Font) font_id);

	/* NOTE: Do we really need the screen_layout proc? */
	screen->root_window = xv_create(screen_public, WINDOW,
			WIN_IS_ROOT,
			WIN_LAYOUT_PROC, screen_layout,
			NULL);

	if (!screen->root_window) {
		XFree((char *)(screen->visual_infos));
		xv_free(screen);
		return XV_ERROR;
	}

	xv_set(screen->server,
			SERVER_PRIVATE_XEVENT_PROC, screen_input, screen->root_window,
			SERVER_PRIVATE_XEVENT_MASK, (XID)xv_get(screen->root_window,XV_XID),
								PropertyChangeMask, screen->root_window,
			NULL);

	/* now tell the server it has a new screen */
	xv_set(screen->server,
			SERVER_NTH_SCREEN, screen->number, screen_public,
			NULL);

	/* Store away any sun specific WM_PROTOCOLS. */
	screen_update_sun_wm_protocols(screen->root_window, FALSE);

	/* don't know yet - we might be in an application that runs
	 * very early...
	 */
	screen->olwm_managed = -1;

	screen->ui_style = defaults_get_enum("openWindows.uiStyle",
					   "OpenWindows.UiStyle", ui_styles);
	if (DefaultDepth(display, screen->number) == 1)
		screen->ui_style = SCREEN_UIS_2D_BW;

	return XV_OK;
}


static Screen_visual * screen_default_visual(Display *display, Screen_info *screen)
{
    Screen_visual *new_visual;
    XVisualInfo   *default_vinfo;
    
    default_vinfo = screen_default_visual_info(display, screen);
    new_visual = screen_new_visual(display, screen, 
				   RootWindow(display, screen->number), 
				   (unsigned)default_vinfo->depth, default_vinfo);
    return(new_visual);
}


static XVisualInfo *screen_default_visual_info(Display	*display, Screen_info *screen)
{
    XVisualInfo *visual_info = (XVisualInfo *)NULL;
    XVisualInfo template;
    long	mask = 0;
    
    if (defaults_exists("window.visual", "Window.Visual")) {
	template.class = defaults_get_enum("window.visual",
					   "Window.Visual",
					   visual_class_pairs);
	if (template.class != VisualClassError)
	  mask |= VisualClassMask;
	else {
	    char message[1000];
	    
	    sprintf(message, XV_MSG("Unknown visual class \"%s\", using default visual\n"),
	    defaults_get_string("window.visual", "Window.Visual",
				(char *)NULL));
	    xv_error(XV_NULL,
		     ERROR_STRING, message,
		     ERROR_PKG, SCREEN,
		     NULL);
	}
    }
        
    if (defaults_exists("window.depth", "Window.Depth")) {
	template.depth = 
	  (unsigned int)defaults_get_integer("window.depth", 
					     "Window.Depth",
					     DefaultDepth(display, screen->number));
	mask |= VisualDepthMask;
    }
    
    if (mask)
      visual_info = screen_match_visual_info(screen, mask, &template);
    
    /* If user didn't specify visual, or there was an error getting 
     * the visual requested, then use the screen's default visual.
     */
    if (visual_info == (XVisualInfo *)NULL) {
	template.visualid = XVisualIDFromVisual(DefaultVisual(display, screen->number));
	visual_info = screen_match_visual_info(screen, (long)VisualIDMask, &template);
    }
    return(visual_info);
}


static int screen_destroy(Xv_Screen screen_public, Destroy_status status)
{
	Screen_info *screen = SCREEN_PRIVATE(screen_public);

	if (notify_post_destroy(screen->root_window, status, NOTIFY_IMMEDIATE) ==
			NOTIFY_DESTROY_VETOED)
		return XV_ERROR;

	if ((status == DESTROY_CHECKING)
		|| (status == DESTROY_SAVE_YOURSELF)
		|| (status == DESTROY_PROCESS_DEATH))
		return XV_OK;

	/* now tell the server it has lost a screen */
	xv_set(screen->server, SERVER_NTH_SCREEN, screen->number, XV_NULL, NULL);

	XFree((char *)(screen->visual_infos));
	if (screen->sun_wm_protocols) XFree((char *)(screen->sun_wm_protocols));
	if (screen->busy_pointer) xv_destroy(screen->busy_pointer);
	free(screen);

	return XV_OK;
}

/* ARGSUSED */
static Xv_opaque screen_set_avlist(Xv_Screen screen_public, Attr_attribute avlist[])
{
    register Attr_avlist    attrs;

    for (attrs = avlist; *attrs; attrs = attr_next(attrs)) {
	switch (attrs[0]) {
	    default:
		xv_check_bad_attr(&xv_screen_pkg, attrs[0]);
		break;
	}
    }

    return (XV_OK);
}


Xv_private void screen_set_clip_rects(Xv_Screen screen_public, XRectangle *xrect_array, int rect_count)
{
    Screen_info    *screen = SCREEN_PRIVATE(screen_public);
    int             i;

    for (i = 0; i < rect_count; i++)
	screen->clip_xrects.rect_array[i] = xrect_array[i];
    screen->clip_xrects.count = rect_count;
}


Xv_private Xv_xrectlist *screen_get_clip_rects(Xv_Screen screen_public)
{
    Screen_info    *screen = SCREEN_PRIVATE(screen_public);

    return (&screen->clip_xrects);
}


Xv_private void screen_set_cached_window_busy(Xv_Screen screen_public, Xv_window window, int busy)
{
    Screen_info    *screen = SCREEN_PRIVATE(screen_public);
    Xv_cached_window *cached_window;

    for (cached_window = screen->cached_windows; cached_window != NULL;
	 cached_window = cached_window->next) {
	if (cached_window->window == window) {
	    cached_window->busy = busy;
	    break;
	}
    }
    if (cached_window == NULL) {
	xv_error(XV_NULL,
		 ERROR_STRING, 
		 XV_MSG("Unable to return window to screen cache"),
		 ERROR_PKG, SCREEN,
		 NULL);
    }
}

Xv_private int screen_get_sun_wm_protocols(Xv_Screen screen_public)
{
    Screen_info    *screen = SCREEN_PRIVATE(screen_public);

    return (screen->num_sun_wm_protocols);
}

/* Receives events from RootWindow.  Specifically looking for PropertyNotify
 * events for the _SUN_WM_PROTOCOLS property.
 */
static void screen_input(Xv_server server, Display *dpy, XPropertyEvent	*xev,
							Xv_opaque obj)
{
	Xv_Screen screen_public;
	Screen_info *screen;

	if (xev->type != PropertyNotify) return;

	screen_public = XV_SCREEN_FROM_WINDOW(obj);
	screen = SCREEN_PRIVATE(screen_public);

	if (xev->atom != screen->sun_wm_p && xev->atom != XA_RESOURCE_MANAGER) {
		return;
	}

	/* test only: quite a few core files at XGetWindowProperty */
	SERVERTRACE((100, "%s: atom = %s\n", __FUNCTION__,
				(char *)xv_get(server, SERVER_ATOM_NAME, xev->atom)));

	if (xev->atom == screen->sun_wm_p) {
		int isdel = FALSE;

		if (xev->state == PropertyDelete) isdel = TRUE;

		screen_update_sun_wm_protocols(obj, isdel);
	}
	else if (xev->atom == XA_RESOURCE_MANAGER) {
		Atom act_type;
		int act_format;
		unsigned long nelem = 0L, rest;
		unsigned char *data = NULL;
		Xv_Screen screen_public = XV_SCREEN_FROM_WINDOW(obj);
		Screen_info *screen = SCREEN_PRIVATE(screen_public);

		/* quite a few core files here */

		/* could it be we get a PropertyDelete on XA_RESOURCE_MANAGER ??? */
		if (xev->state == PropertyDelete) return;

		if (XGetWindowProperty(dpy, RootWindow(dpy, screen->number),
					XA_RESOURCE_MANAGER, 0L, 60000L, False, XA_STRING,
					&act_type, &act_format, &nelem, &rest,
					&data) != Success)
		{
			return;
		}
		if (! data) return;
		if (nelem == 0) return;

		data[nelem] = '\0';
		xv_set(server, SERVER_UPDATE_RESOURCE_DATABASES, data, NULL);
		XFree((char *)data);
	}
}

Xv_pkg xv_screen_pkg = {
    "Screen", ATTR_PKG_SCREEN,
    sizeof(Xv_screen_struct),
    &xv_generic_pkg,
    screen_init,
    screen_set_avlist,
    screen_get_attr,
    screen_destroy,
    NULL			/* no find proc */
};
