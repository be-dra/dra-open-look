#ifndef lint
char     tty_newtxt_c_sccsid[] = "@(#)tty_newtxt.c 1.45 93/06/28 DRA: $Id: tty_newtxt.c,v 4.6 2025/04/07 19:24:44 dra Exp $";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 *
 * tty_newtxt.c	-- faster routines for text output
 *
 * Experimental routines for faster text thruput. Jim Becker 4/3/89
 *
 * These routines are being added as needed to speed up the text output. Note
 * that, since I am not changing the main body of code, any initialization of
 * the routines is done the `firsttime' they are called.
 *
 * This logic is being augmented to perform text clipping on the line level for
 * the rendering of text to the screen. The current tty and text logic always
 * repaints all of the screen. This prevents lines that are out of the
 * clipping regions from being displayed.
 *
 * There are three local GCs that are allocated in this file for all the text
 * needs.
 *
 * brianw [Tue Aug 25, 1992]
 * Above comments about repaint strategy are obsolete.  The logic for
 * handling expose and graphics expose events now exports information (via
 * the Tty_exposed_lines type) about what needs repainting so that only
 * damaged areas are repainted.  Code in the textsw and ttysw have been
 * changed so that they use this information when repainting.
 */


#  include <X11/Xlibint.h>
#include <xview/window.h>
#include <xview_private/pw_impl.h>
#include <pixrect/pixrect.h>
#include <pixrect/pixfont.h>
#include <xview/xv_xrect.h>
#include <xview_private/i18n_impl.h>
#include <xview_private/tty_impl.h>
#include <xview/font.h>
#include <xview_private/svr_impl.h>

#define	BADVAL		-1
#define	ALL		AllPlanes

#ifdef OW_I18N
extern void      XwcDrawString(), XwcDrawImageString();
#endif

typedef struct tty_gc_list {
    int depth;
    unsigned long fore, back;
    GC  gcs[3];
    struct tty_gc_list *next;
} Tty_GC_List;

#define INVERTED_GC	0
#define DEFAULT_GC	1
#define BACK_GC		2

static int TTY_CURRENT_FONT_KEY;
static int TTY_GC_LIST_KEY;
extern int xv_to_xop[16];

/*
 * set font_height to 13 (default).  It really doesn't matter what it is,
 * just as long as it isn't zero.
 */
static int      font_height = 13;

/*
 * create a local GC for the user. This is used the first time that the text
 * routines are used to make the three GC used herein.
 */

static GC create_GC(Display *display, Drawable drawable,
				unsigned int foreground, unsigned int background, int function)
{
    unsigned long   gcvaluemask;
    XGCValues       gcvalues;
    GC              gc;

    gcvalues.function = function;
    gcvalues.plane_mask = AllPlanes;
    gcvalues.foreground = foreground;
    gcvalues.background = background;

    gcvaluemask = GCFunction | GCPlaneMask |
	GCForeground | GCBackground;

    gc = XCreateGC(display, drawable, gcvaluemask, &gcvalues);

    return gc;
}

/*
 * get_gc_list - return a list of GC's that can be used with the given
 * 		 window's drawable info.
 */
static GC * get_gc_list(Xv_Drawable_info *info)
{
    Display *display;
    Xv_Screen	screen;
    Drawable xid;
    Tty_GC_List *front, *gc_list;
    int depth;
    unsigned long fore, back;

    screen = xv_screen(info);

    depth = xv_depth(info);
    fore = xv_fg(info);
    back = xv_bg(info);
    front = (Tty_GC_List *)xv_get(screen, XV_KEY_DATA, TTY_GC_LIST_KEY);
    gc_list = front;
    while (gc_list != (Tty_GC_List *)NULL) {
	if (gc_list->depth == depth &&
	    gc_list->fore == fore &&
	    gc_list->back == back)
	  return(gc_list->gcs);
	else
	  gc_list = gc_list->next;
    }

    /* Couldn't find one that was cached, so create a new list */
    display = xv_display(info);
    xid = xv_xid(info);
    gc_list = (Tty_GC_List *)malloc(sizeof(Tty_GC_List));
    gc_list->depth = depth;
    gc_list->fore = fore;
    gc_list->back = back;
    gc_list->next = front;
    gc_list->gcs[INVERTED_GC] = create_GC(display, xid, (unsigned)(fore ^ back),
								(unsigned)back, GXxor);
    gc_list->gcs[DEFAULT_GC] = create_GC(display, xid, (unsigned)fore,
								(unsigned)back, GXcopy);
    gc_list->gcs[BACK_GC] = create_GC(display, xid, (unsigned)fore,
								(unsigned)back, GXcopy);
    xv_set(screen, XV_KEY_DATA, TTY_GC_LIST_KEY, gc_list, NULL);
    return(gc_list->gcs);
}

/*
 * set the fonts in the GCs to the Pixfont specified. called once only!
 */
static void setup_font(Xv_opaque window, Xv_opaque pixfnt)
{
#ifdef OW_I18N
    XFontSet            font_set;
    XFontSetExtents     *font_set_extents;

    font_set = (XFontSet)xv_get(pixfnt, FONT_SET_ID);
    font_set_extents = (XFontSetExtents *)XExtentsOfFontSet(font_set);
    font_height = font_set_extents->max_logical_extent.height;
#else
    Xv_Drawable_info *info;
    Display        *display;
    Font            font;
    XFontStruct    *fontinfo;
    GC		    *gc_list;

    DRAWABLE_INFO_MACRO(window, info);
    display = xv_display(info);
    font = (Font) xv_get(pixfnt, XV_XID);

    /* it should always be valid, but be careful */
    if (font != XV_NULL) {
	gc_list = get_gc_list(info);
	
	XSetFont(display, gc_list[DEFAULT_GC], font);
	XSetFont(display, gc_list[INVERTED_GC], font);
	
	/* determine font height -- don't trust globals!! */
	fontinfo = (XFontStruct *)xv_get(pixfnt, FONT_INFO);
	
	font_height = fontinfo->ascent + fontinfo->descent;
    }
#endif
}

/*
 * setup the correct pens and such in the graphics context
 */
static void setup_GC(Display *display, Xv_Drawable_info *info,GC gc, int pix_op)
{
	unsigned long lfore, lback;
	unsigned long lplanes = AllPlanes;
	int lfunc = xv_to_xop[pix_op >> PIX_OP_SHIFT];

	if (!info)
		return;

	lfore = xv_fg(info);
	lback = xv_bg(info);

	/* convert functions from SunView expectation to X understanding */
	switch (lfunc) {
		case GXclear:
			lfore = lback;
			lfunc = GXcopy;
			break;
		case GXset:
			lfunc = GXcopy;
			break;
		case GXxor:
			lfore = lfore ^ lback;
			break;
		case GXinvert:
			lplanes = lfore ^ lback;
			break;
		case GXcopyInverted:	/* jcb 11/15/89:1/23/90  to fix lack of highlight */
			lfore = lback;
			lback = xv_fg(info);
			lfunc = GXcopy;
	}

	XSetState(display, gc, lfore, lback, lfunc, lplanes);
}


/*
 * called the firsttime we have some access to this module to perform
 * self-initialization.. This keeps this code self-contained to some degree,
 * as it can be called from anywhere.
 */
static void firsttime_init(void)
{
    if (!TTY_GC_LIST_KEY) TTY_GC_LIST_KEY = xv_unique_key();
}

/*
 * basic mechanism is to cache the information between use of this routine.
 * this assumes that certain of the attributes are not going to change.
 *
 * Note that this also assumes that all will be the same dependent on the
 * "window" pointer. [There would still be too much work to look at the even
 * the window XID for me to be happy...]
 */
Xv_private void tty_newtext(Xv_opaque window, int xbasew, int ybasew, int op,
								Xv_opaque pixfnt, CHAR *string, int len)
{
	static int old_op = BADVAL;
	Xv_Drawable_info *info;
	Display *display;
	Drawable drawable;
	static GC *gc;
	Xv_window paint_window;

#ifdef OW_I18N
	static void (*routine)();
#else /* OW_I18N */
	typedef int (*draw_str_t)(Display *, Drawable, GC, int, int, const char *,
			int);
	static draw_str_t routine;
#endif /* OW_I18N */

	Xv_Screen screen;
	static Xv_Screen old_screen;
	static int old_depth = 0;
	int new_fg, new_bg;
	GC *gc_list;
	static GC *old_gc_list = NULL;	/* fix for 1053036 */
	XGCValues *gv;

#if XlibSpecificationRelease >= 5
	/* lumpi@dobag.in-berlin.de */
	XGCValues lumpi_tmp;
#endif

	if (len == 0)
		return;

	if (xv_get(window, XV_IS_SUBTYPE_OF, OPENWIN_VIEW)) {
		paint_window = xv_get(window, OPENWIN_VIEW_PAINT_WINDOW);
		if (paint_window) window = paint_window;
	}
	else {
		extern char *xv_app_name;

		fprintf(stderr, "%s: tty_newtext called with a %s\n", xv_app_name,
					((Xv_pkg *)xv_get(window, XV_TYPE))->name);
	}
	DRAWABLE_INFO_MACRO(window, info);
	display = xv_display(info);
	drawable = xv_xid(info);
	screen = xv_screen(info);

	if (!TTY_GC_LIST_KEY)
		firsttime_init();
	gc_list = get_gc_list(info);

	if (!TTY_CURRENT_FONT_KEY)
		TTY_CURRENT_FONT_KEY = xv_unique_key();

	if (pixfnt != (Xv_opaque) xv_get(screen, XV_KEY_DATA, TTY_CURRENT_FONT_KEY)
			|| old_gc_list != gc_list) {
		setup_font(window, pixfnt);
		xv_set(screen, XV_KEY_DATA, TTY_CURRENT_FONT_KEY, pixfnt, NULL);
		old_gc_list = gc_list;
	}

	if (((op = PIX_OP(op)) != old_op) ||
			(screen != old_screen) || (xv_depth(info) != old_depth))
	{
		old_screen = screen;
		old_depth = xv_depth(info);
		if (op == PIX_NOT(PIX_DST)) {
			gc = &gc_list[INVERTED_GC];
		}
		else {
			gc = &gc_list[DEFAULT_GC];
			setup_GC(display, info, *gc, op);
		}

#ifdef OW_I18N
		if (op == PIX_SRC || op == PIX_NOT(PIX_SRC))
			routine = XwcDrawImageString;
		else
			routine = XwcDrawString;
#else
		if (op == PIX_SRC || op == PIX_NOT(PIX_SRC))
			routine = XDrawImageString;
		else
			routine = XDrawString;
#endif

		old_op = op;
	}
	else {
		if (op == PIX_NOT(PIX_DST))
			gc = &gc_list[INVERTED_GC];
		else
			gc = &gc_list[DEFAULT_GC];

	}

#if XlibSpecificationRelease >= 5
	XGetGCValues(display, *gc, GCForeground | GCBackground, &lumpi_tmp);
	gv = &lumpi_tmp;
#else
	gv = &(*gc)->values;
#endif

	new_fg = xv_fg(info);
	new_bg = xv_bg(info);
	if (((new_fg != gv->foreground) || (new_bg != gv->background)) &&
			(op != PIX_NOT(PIX_SRC) /* jcb -- in this case pens switched */ ))
	{
		XGCValues gc_values;

		if (gc == &gc_list[INVERTED_GC]) {
			gc_values.foreground = new_fg ^ new_bg;
		}
		else {
			gc_values.foreground = new_fg;
		}
		gc_values.background = new_bg;

		XChangeGC(display, *gc, GCForeground | GCBackground, &gc_values);
	}

#ifdef OW_I18N
	(void)(*routine) (display, drawable, xv_get(pixfnt, FONT_SET_ID),
			*gc, xbasew, ybasew, string, len);
#else
	(*routine) (display, drawable, *gc, xbasew, ybasew, string, len);
#endif
}

/*
 * routine to set, clear or invert the background.
 *
 * takes the same parameters as pw_writebackground()
 */


Xv_private void tty_background(Xv_opaque window, int x, int y, int w, int h, int op)
{
    Xv_Drawable_info *info;
    Display *display;
    Drawable drawable;
    GC	*gc_list;

	{
		Xv_window paint_window;
		paint_window = xv_get(window, OPENWIN_VIEW_PAINT_WINDOW);
		if (paint_window) window = paint_window;
	}

    DRAWABLE_INFO_MACRO(window, info);
    display = xv_display(info);
    drawable = xv_xid(info);

    if (!TTY_GC_LIST_KEY)
      firsttime_init();
    gc_list = get_gc_list(info);
 
    setup_GC(display, info, gc_list[BACK_GC] , op);
	SERVERTRACE((567, "tty_background(op=%d, [%d, %d, %d, %d]\n",
						op, x, y, w, h));
    XFillRectangle(display,drawable, gc_list[BACK_GC], x, y, (unsigned)w, (unsigned)h);
}

Pkg_private void ttysw_underline(Xv_window win, int x1, int x2, int y)
{
    Xv_Drawable_info *info;
    GC *gc_list;

    DRAWABLE_INFO_MACRO(win, info);
    gc_list = get_gc_list(info);

	XDrawLine(xv_display(info), xv_xid(info), gc_list[DEFAULT_GC],x1, y, x2, y);
}

/*
 * copy bits from one place to another on same window.
 */
Xv_private void tty_copyarea(Xv_opaque window, int sX, int sY, int W, int H, int dX, int dY)
{
    Xv_Drawable_info *info;
    Display        *display;
    Drawable        drawable;
    GC	*gc_list;

	{
		Xv_window paint_window;
		paint_window = xv_get(window, OPENWIN_VIEW_PAINT_WINDOW);
		if (paint_window) window = paint_window;
	}
    DRAWABLE_INFO_MACRO(window, info);
    display = xv_display(info);
    drawable = xv_xid(info);

    /* reset to normal mode for copying */
    if (!TTY_GC_LIST_KEY)
      firsttime_init();
    gc_list = get_gc_list(info);

    XSetState(display, gc_list[BACK_GC], xv_fg(info), xv_bg(info), GXcopy, AllPlanes);
    XCopyArea(display, drawable, drawable, gc_list[BACK_GC],
		sX, sY, (unsigned)W, (unsigned)H, dX, dY);
}

Xv_private_data int ttysw_view_obscured;

Xv_private void tty_synccopyarea(Xv_opaque window)
{
	Xv_Drawable_info *info;
	Display        *dpy;
	Drawable        win;
	XEvent		xevent;

	if(ttysw_view_obscured == VisibilityPartiallyObscured) {
		{
			Xv_window paint_window;
			paint_window = xv_get(window, OPENWIN_VIEW_PAINT_WINDOW);
			if (paint_window) window = paint_window;
		}
		DRAWABLE_INFO_MACRO(window, info);
		dpy = xv_display(info);
		win = xv_xid(info);
 
		XSync(dpy, FALSE);
		if(XCheckWindowEvent(dpy, win, ExposureMask, &xevent))
			if(xevent.type != NoExpose)
				ttysw_prepair(&xevent);
	}
}


static int clip_rects_set;

/*
 * Read in the X expose & graphics expose events, flag the lines needing 
 * repaint. Data structure is returned so that other modules can employ it
 * to repaint only those lines that have been exposed.
 */
Xv_private Tty_exposed_lines *tty_calc_exposed_lines(Xv_opaque window,
						XEvent *first_event,	/* must be an "expose" event */
						int caret_y)
{
	static Tty_exposed_lines exposed;
	Display *dpy;
	Window win;
	Xv_Drawable_info *info;
	int i;
	int nlines, fline;
	XEvent xevent;

	/* assert(font_height > 0); */

	{
		Xv_window paint_window;

		paint_window = xv_get(window, OPENWIN_VIEW_PAINT_WINDOW);
		if (paint_window) window = paint_window;
	}
	DRAWABLE_INFO_MACRO(window, info);
	dpy = xv_display(info);
	win = xv_xid(info);

	/* initialize to unexposed, could use bzero here. */
	for (i = 0; i < MAX_LINES; i++) exposed.line_exposed[i] = FALSE;
	exposed.caret_line_exposed = FALSE;

	if (first_event) {
		xevent = *first_event;
	}
	else {
		/* No event was passed in, handle it gracefully.
		 * This happens when win_post_id() is called.
		 * The XClearArea causes a real expose event to happen.
		 * since this is rare, the effective round trip
		 * is not considered a performance problem.
		 */
		XClearArea(dpy, win, 0, 0, 0, 0, True);
		return &exposed;
	}
	exposed.leftmost = xevent.xexpose.x;
	/*
	 * Iterate through Expose events in queue, first event is passed in,
	 * stop when reached some other event type or no more events.
	 */
	while (1) {
		if (xevent.type == Expose || xevent.type == GraphicsExpose) {

			/* mark lines in region as exposed by expose event */
			fline = xevent.xexpose.y / font_height;
			nlines = (xevent.xexpose.height + font_height) / font_height;

			for (i = 0; (i <= nlines) && (i < MAX_LINES); i++)
				exposed.line_exposed[i + fline] = TRUE;

			if (xevent.xexpose.x < exposed.leftmost)
				exposed.leftmost = xevent.xexpose.x;

			if (xevent.xexpose.count == 0)
				/* 
				 * Don't attempt to get events past those where 
				 * count == 0.  This avoids ioctls and reads inside 
				 * XCheckWindowEvent if no more are in the queue.
				 */
				break;

			if (XCheckWindowEvent(dpy, win, ExposureMask, &xevent) == FALSE)
				break;

		}
		else {
			/* must be a NoExpose event */
			break;
		}
	}


	/*
	 * Check to see if the line the caret needs repainting by checking
	 * the line that it is on.
	 * Assume that caret crosses into next line.  (Textsw)
	 */
	fline = caret_y / font_height;	/* reuse: fline */
	exposed.caret_line = fline;
	if ((fline >= 0 && fline < MAX_LINES - 1) &&
			(exposed.line_exposed[fline] || exposed.line_exposed[fline + 1])) {
		exposed.caret_line_exposed = TRUE;
		exposed.line_exposed[fline] = TRUE;
		exposed.line_exposed[fline + 1] = TRUE;
	}

	if ((exposed.caret_line_exposed == FALSE) &&
			(first_event->xexpose.count == 0)) {
		/*
		 * Setup the gcs so that they clip.  There is a slight
		 * performance hit from doing this, and in fact this
		 * code is not necessary.  However, it reduces flickering.
		 * We're only setting it when it's a singular expose
		 * event because its simpiler to avoid overlapping clip
		 * rects that way.
		 * Also, clip rects are not set when the caret line is
		 * involved because of the way the caret is cleared then
		 * xor'd: using another gc.
		 */
		GC *gc_list;
		XRectangle rect;

		if (!TTY_GC_LIST_KEY)
			firsttime_init();
		gc_list = get_gc_list(info);

		clip_rects_set = TRUE;
		rect.x = first_event->xexpose.x;
		rect.y = first_event->xexpose.y;
		rect.width = first_event->xexpose.width;
		rect.height = first_event->xexpose.height;

		XSetClipRectangles(dpy, gc_list[DEFAULT_GC], 0, 0, &rect, 1, Unsorted);

		XSetClipRectangles(dpy, gc_list[INVERTED_GC], 0, 0, &rect, 1, Unsorted);

		XSetClipRectangles(dpy, gc_list[BACK_GC], 0, 0, &rect, 1, Unsorted);
	}


	return &exposed;
}

void tty_clear_clip_rectangles(Xv_opaque window)
{
	Display *display;
	Xv_Drawable_info *info;
	GC *gc_list;

	if (clip_rects_set) {
		{
			Xv_window paint_window;
			paint_window = xv_get(window, OPENWIN_VIEW_PAINT_WINDOW);
			if (paint_window) window = paint_window;
		}
		DRAWABLE_INFO_MACRO(window, info);
		display = xv_display(info);

		gc_list = get_gc_list(info);

		XSetClipMask(display, gc_list[INVERTED_GC], None);
		XSetClipMask(display, gc_list[DEFAULT_GC], None);
		XSetClipMask(display, gc_list[BACK_GC], None);
		clip_rects_set = FALSE;
	}
}

