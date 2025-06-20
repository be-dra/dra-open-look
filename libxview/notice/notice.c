#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)notice.c 20.110 93/06/28  DRA: RCS $Id: notice.c,v 4.14 2025/06/19 11:48:39 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL_NOTICE 
 *	file for terms of the license.
 */

#include <xview/notice.h>
#include <olgx/olgx.h>
#include <xview_private/i18n_impl.h>
#include <xview_private/fm_impl.h>
#include <xview_private/windowimpl.h>
#include <xview_private/draw_impl.h>
#include <xview_private/win_info.h>
#include <xview_private/svr_impl.h>
#include <xview_private/sel_impl.h>
#include <xview_private/pw_impl.h>
#include <xview_private/wmgr_decor.h>
#include <xview/frame.h>
#include <xview/panel.h>
#include <xview/fullscreen.h>
#include <xview/defaults.h>
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>
#include <images/bg1.xbm>
#include <images/bg2.xbm>
#include <images/bg3.xbm>

#define NOTICE_PRIVATE(notice_public)	XV_PRIVATE(Notice_info, Xv_notice_struct, notice_public)
#define	NOTICE_PUBLIC(notice)	XV_PUBLIC(notice)
#define NOTICE_HELP		(NOTICE_TRIGGERED-1)
#define NOTICE_ACTION_DO_IT	'\015'

#define VERT_MSG_MARGIN(scale)		Notice_dimensions[scale].vert_msg_margin
#define HORIZ_MSG_MARGIN(scale)		Notice_dimensions[scale].horiz_msg_margin
#define APEX_DIST(scale)		Notice_dimensions[scale].apex_dist
#define BUT_PORTION_HEIGHT(scale)	Notice_dimensions[scale].but_portion_height
#define FONT_POINTSIZE(scale)		Notice_dimensions[scale].font_pointsize
#define FONT_POINTSIZE(scale)		Notice_dimensions[scale].font_pointsize
#define NOTICE_BORDER_WIDTH(scale)	Notice_dimensions[scale].border_width
#define PANE_BORDER_WIDTH(scale)	Notice_dimensions[scale].pane_border_width
#define PANE_NOTICE_BORDER_DIST(scale)	Notice_dimensions[scale].pane_notice_border_dist
#define MSG_VERT_GAP(scale)		Notice_dimensions[scale].msg_vert_gap
#define BUT_HORIZ_GAP(scale)		Notice_dimensions[scale].but_horiz_gap

#define PANE_XY(is_toplevel_window, scale)		\
		( is_toplevel_window ?			\
		    (NOTICE_BORDER_WIDTH(scale)+	\
		    PANE_NOTICE_BORDER_DIST(scale)+	\
		    PANE_BORDER_WIDTH(scale)) :		\
		    PANE_BORDER_WIDTH(scale)		\
		)

#define	PANE_NOTICE_DIFF(is_toplevel_window, scale) \
		(2 * (PANE_XY(is_toplevel_window, scale)+1))

#define NOTICE_NOT_TOPLEVEL		0
#define NOTICE_IS_TOPLEVEL		1


typedef struct notice {
    Xv_Notice		public_self;

    Frame		client_window;
    Frame		owner_window;

    /*
     * XView objects that make up the non-screen locking
     * notice
     */
    Frame		sub_frame;
    Panel		panel;
    Frame		*busy_frames;
    void		(*event_proc)(Xv_notice, int, Event *);

    Xv_object		fullscreen_window;

    int			result;
    int			*result_ptr;

    int			default_input_code;
    Event		*event;
    Event		help_event;

    Xv_Font		notice_font;

    int			beeps;

    int			focus_x;
    int			focus_y;

    int			old_mousex;
    int			old_mousey;

    CHAR		**message_items;

    int			number_of_buttons;
    int			number_of_strs;
    struct notice_buttons *button_info;
    struct notice_msgs 	*msg_info;
    char 		*help_data;

    Graphics_info	*ginfo;
    int			three_d;

    /*
     * Notice scale
     */
    int			scale;

    /* flags */
    unsigned		lock_screen:1;
    unsigned		yes_button_exists:1;
    unsigned		no_button_exists:1;
    unsigned		focus_specified:1;
    unsigned		dont_beep:1;
    unsigned		need_layout:1;
    unsigned		show:1;
    unsigned		new:1;
    unsigned		block_thread:1;
    unsigned		lock_screen_looking:1;

} Notice_info;

typedef struct notice	*notice_handle;

struct notice_msgs {
    Panel			panel_item;
    CHAR			*string;
    struct rect			 msg_rect;
    struct notice_msgs		*next;
};

struct notice_buttons {
    Panel			panel_item;
    CHAR			*string;
    int				 value;
    int				 is_yes;
    int				 is_no;
    struct rect			 button_rect;
    struct notice_buttons	*next;
};

typedef struct notice_buttons	*notice_buttons_handle;
typedef struct notice_msgs	*notice_msgs_handle;

typedef struct {
    unsigned int	width;			/* (a) */
    unsigned int	vert_msg_margin;	/* (b) */
    unsigned int	horiz_msg_margin;	/* (c) */
    unsigned int	apex_dist;		/* (d) */
    unsigned int	but_portion_height;	/* (e) */
    unsigned int	font_pointsize;		/* (f) */
    unsigned int	border_width;		/* extra */
    unsigned int	pane_border_width;	/* extra */
    unsigned int	pane_notice_border_dist;/* extra */
    unsigned int	msg_vert_gap;		/* extra */
    unsigned int	but_horiz_gap;		/* extra */
}Notice_config;

typedef struct {
	Xv_opaque root_window;
	Xv_Window client_window;
	Fullscreen fs;
	Rect notice_screen_rect;
	Rect rect;
	Xv_Drawable_info *info;
	int x;
	int y;
	int old_mousex;
	int old_mousey;
	int buttons_width;
	int leftoff, topoff;
	int quadrant;
	int left_placement;
	int top_placement;
	int event_state;
	int have_shape;
} notice_prep_t;

/*
 * OPEN LOOK geometry
 * Numbers represent pixels
 * Should use points and convert to pixels.
 */
static Notice_config Notice_dimensions[] = {
    /* NOTICE_SMALL */
    { 332, 30, 16, 36, 32, 10, 2, 2, 3, 3, 8 },

    /* NOTICE_MEDIUM */
    { 390, 36, 20, 42, 36, 12, 2, 2, 3, 3, 10 },

    /* NOTICE_LARGE */
    { 448, 42, 24, 50, 40, 14, 2, 2, 3, 3, 12 },

    /* NOTICE_EXTRALARGE */
    { 596, 54, 32, 64, 48, 19, 2, 2, 3, 3, 16 }
};


static Defaults_pairs bell_types[] = {
	{ "never",   0 },
	{ "notices", 1 },
	{ "always",  2 },
	{ NULL,      2 }
};

static int		default_beeps;
/* static int		notice_use_audible_bell; */
static int		notice_jump_cursor;
static int		notice_context_key;

/*
 * Key data for handle to notice private data
 */
/* int		notice_context_key; */
/* int		default_beeps; */
/* int		notice_jump_cursor; */

/*
 * Table containing valid values for OpenWindows.KeyboardCommands resource
 */
Xv_private Defaults_pairs xv_kbd_cmds_value_pairs[4];

/*
 * --------------------------- Cursor Stuff -------------------------
 */


#ifdef OW_I18N
extern struct pr_size xv_pf_textwidth_wc();
#else
extern struct pr_size xv_pf_textwidth(int len, Xv_font pf, char  *str);
#endif

#define NOTICE_INVERT_BUTTON	1
#define NOTICE_NORMAL_BUTTON	0

/*
 * --------------------------- Externals ----------------------------
 */

extern Graphics_info *xv_init_olgx(Xv_Window, int *, Xv_Font);


static void notice_draw_polygons(Display *dpy, notice_handle notice,
			int isbm, Drawable d, GC gc, 
			unsigned long bg1, unsigned long bg2, unsigned long bg3,
			Rect *r, int quadrant, int leftoff, int topoff)
{
	Rect rect = *r;
	Pixmap bitmap;
	XPoint points[4];
	int diff = PANE_NOTICE_DIFF(NOTICE_IS_TOPLEVEL, notice->scale);
	int finetune; /* das habe ich manuell rausgefieselt... */
    Xv_screen screen = xv_get(notice->owner_window, XV_SCREEN);
	int num = (int)xv_get(screen, SCREEN_NUMBER);
	unsigned long fg = BlackPixel(dpy, num);
	unsigned long gwyn = WhitePixel(dpy, num);
	/* original: int three_d = notice->three_d;
	 * but I wanted in the SCREEN_UIS_2D_COLOR case the same
	 * "emanation shadow" as in the 3D case... so, the distinction
	 * is made by a variable: not three_d, but no_stipple.
	 */
	screen_ui_style_t ui_style = xv_get(screen, SCREEN_UI_STYLE);
	int no_stipple = (ui_style != SCREEN_UIS_2D_BW);
	int transp = defaults_get_boolean("openWindows.noticeShadowTransparent",
								"OpenWindows.NoticeShadowTransparent", FALSE);
	int func;

	if (transp) func = GXand;
	else func = GXcopy;

	switch (quadrant) {
		case 0:	/* break down and to right */
			points[0].x = 0;
			points[0].y = 0;
			points[1].x = leftoff + (isbm ? 1 : 0);
			points[1].y = topoff + (isbm ? 1 : 0);

			/* 1st (top) triangle of the shadow */
			points[2].x = rect.r_width + diff + leftoff + (isbm ? 1 : 0);
			points[2].y = topoff + (isbm ? 1 : 0);
			XSetForeground(dpy, gc, bg2);
			if (!isbm) {
				if (no_stipple) {
					XSetFunction(dpy, gc, func);
				}
				else {
					bitmap = (Pixmap) xv_get(screen, XV_KEY_DATA,
													SCREEN_BG3_PIXMAP);
					if (bitmap == (Pixmap) NULL) {
						bitmap = XCreatePixmapFromBitmapData(dpy,
							d, bg3_bits, bg3_width, bg3_height, fg, gwyn, 1);
						xv_set(screen,
								XV_KEY_DATA,SCREEN_BG3_PIXMAP, bitmap,
								NULL);
					}
					XSetStipple(dpy, gc, bitmap);
					XSetFillStyle(dpy, gc, FillOpaqueStippled);
					XSetBackground(dpy, gc, gwyn);
				}
			}
			XFillPolygon(dpy, d, gc, points, 3, Complex, CoordModeOrigin);
			XSetFillStyle(dpy, gc, FillSolid);
			XSetFunction(dpy, gc, GXcopy);

			/* 2nd (left) triangle of the shadow */
			points[2].x = leftoff + (isbm ? 1 : 0);
			points[2].y = rect.r_height + diff + topoff + (isbm ? 1 : 0);
			XSetForeground(dpy, gc, bg3);
			if (!isbm) {
				if (no_stipple) {
					XSetFunction(dpy, gc, func);
				}
				else {
					bitmap = (Pixmap) xv_get(screen, XV_KEY_DATA,
													SCREEN_BG2_PIXMAP);
					if (bitmap == (Pixmap) NULL) {
						bitmap = XCreatePixmapFromBitmapData(dpy,
							d, bg2_bits, bg2_width, bg2_height, fg, gwyn, 1);
						xv_set(screen,
								XV_KEY_DATA,SCREEN_BG2_PIXMAP, bitmap,
								NULL);
					}
					XSetStipple(dpy, gc, bitmap);
					XSetFillStyle(dpy, gc, FillOpaqueStippled);
					XSetBackground(dpy, gc, gwyn);
				}
			}
			XFillPolygon(dpy, d, gc, points, 3, Complex, CoordModeOrigin);
			XSetFillStyle(dpy, gc, FillSolid);
			XSetFunction(dpy, gc, GXcopy);

			finetune = 2;
			XSetForeground(dpy, gc, bg1);
			XFillRectangle(dpy, d, gc, points[1].x,
								points[1].y,
								(unsigned)rect.r_width + diff - finetune,
								(unsigned)rect.r_height + diff - finetune);

			break;
		case 1:	/* break down and to left */
			/* (right) triangle of the shadow */
			finetune = 2;
			points[0].x = rect.r_width + diff + leftoff;
			points[0].y = 0;
			points[1].x = rect.r_width + diff - finetune;
			points[1].y = rect.r_height + diff + topoff - (isbm ? 1 : 0);
			points[2].x = rect.r_width + diff - finetune;
			points[2].y = topoff;

			XSetForeground(dpy, gc, bg3);
			if (!isbm) {
				if (no_stipple) {
					XSetFunction(dpy, gc, func);
				}
				else {
					bitmap = (Pixmap) xv_get(screen, XV_KEY_DATA,
													SCREEN_BG2_PIXMAP);
					if (bitmap == (Pixmap) NULL) {
						bitmap = XCreatePixmapFromBitmapData(dpy,
							d, bg2_bits, bg2_width, bg2_height, fg, gwyn, 1);
						xv_set(screen,
							XV_KEY_DATA,SCREEN_BG2_PIXMAP, bitmap,
							NULL);
					}
					XSetStipple(dpy, gc, bitmap);
					XSetFillStyle(dpy, gc, FillOpaqueStippled);
					XSetBackground(dpy, gc, gwyn);
				}
			}
			XFillPolygon(dpy, d, gc, points, 3, Complex, CoordModeOrigin);
			XSetFillStyle(dpy, gc, FillSolid);
			XSetFunction(dpy, gc, GXcopy);

			/* (top) triangle of the shadow */
			points[1].x = 0;
			points[1].y = topoff;

			XSetForeground(dpy, gc, bg2);
			if (!isbm) {
				if (no_stipple) {
					XSetFunction(dpy, gc, func);
				}
				else {
					bitmap = (Pixmap) xv_get(screen, XV_KEY_DATA,
													SCREEN_BG3_PIXMAP);
					if (bitmap == (Pixmap) NULL) {
						bitmap = XCreatePixmapFromBitmapData(dpy,
							d, bg3_bits, bg3_width, bg3_height, fg, gwyn, 1);
						xv_set(screen,
								XV_KEY_DATA,SCREEN_BG3_PIXMAP, bitmap,
								NULL);
					}
					XSetStipple(dpy, gc, bitmap);
					XSetFillStyle(dpy, gc, FillOpaqueStippled);
					XSetBackground(dpy, gc, gwyn);
				}
			}
			XFillPolygon(dpy, d, gc, points, 3, Complex, CoordModeOrigin);
			XSetFillStyle(dpy, gc, FillSolid);
			XSetFunction(dpy, gc, GXcopy);

			XSetForeground(dpy, gc, bg1);
			XFillRectangle(dpy, d, gc,
								0,
								topoff,
								(unsigned)rect.r_width + diff - finetune,
								(unsigned)rect.r_height + diff - finetune);
			break;
		case 2:	/* break up and to left */
			finetune = 2;
			/* rechtes  Dreieck */
			points[0].x = rect.r_width + diff + leftoff;
			points[0].y = rect.r_height + diff + topoff;
			points[1].x = rect.r_width + diff - finetune;
			points[1].y = - (isbm ? 2 : 0);
			points[2].x = rect.r_width + diff - finetune;
			points[2].y = rect.r_height + diff - finetune;

			XSetForeground(dpy, gc, bg2);
			if (!isbm) {
				if (no_stipple) {
					XSetFunction(dpy, gc, func);
				}
				else {
					bitmap = (Pixmap) xv_get(screen, XV_KEY_DATA,
												SCREEN_BG2_PIXMAP);
					if (bitmap == (Pixmap) NULL) {
						bitmap = XCreatePixmapFromBitmapData(dpy,
							d, bg2_bits, bg2_width, bg2_height, fg, gwyn, 1);
						xv_set(screen,
								XV_KEY_DATA,SCREEN_BG2_PIXMAP, bitmap,
								NULL);
					}
					XSetStipple(dpy, gc, bitmap);
					XSetFillStyle(dpy, gc, FillOpaqueStippled);
					XSetBackground(dpy, gc, gwyn);
				}
			}
			XFillPolygon(dpy, d, gc, points, 3, Complex, CoordModeOrigin);
			XSetFillStyle(dpy, gc, FillSolid);
			XSetFunction(dpy, gc, GXcopy);

			/* unteres  Dreieck */
			points[1].x = 0;
			points[1].y = rect.r_height + diff - finetune;

			XSetForeground(dpy, gc, bg3);
			if (!isbm) {
				if (no_stipple) {
					XSetFunction(dpy, gc, func);
				}
				else {
					bitmap = (Pixmap) xv_get(screen, XV_KEY_DATA,
												SCREEN_BG1_PIXMAP);
					if (bitmap == (Pixmap) NULL) {
						bitmap = XCreatePixmapFromBitmapData(dpy,
							d, bg1_bits, bg1_width, bg1_height, fg, gwyn, 1);
						xv_set(screen,
								XV_KEY_DATA,SCREEN_BG1_PIXMAP, bitmap,
								NULL);
					}
					XSetStipple(dpy, gc, bitmap);
					XSetFillStyle(dpy, gc, FillOpaqueStippled);
					XSetBackground(dpy, gc, gwyn);
				}
			}
			XFillPolygon(dpy, d, gc, points, 3, Complex, CoordModeOrigin);
			XSetFillStyle(dpy, gc, FillSolid);
			XSetFunction(dpy, gc, GXcopy);

			XSetForeground(dpy, gc, bg1);
			XFillRectangle(dpy, d, gc,
								0,
								0,
								(unsigned)rect.r_width + diff - finetune,
								(unsigned)rect.r_height + diff - finetune);
			break;
		case 3:	/* break up and to right */
			finetune = 2;

			/* unteres Dreieck */
			points[0].x = 0;
			points[0].y = rect.r_height + diff + topoff;
			points[1].x = rect.r_width + diff + leftoff - (isbm ? 2 : 0);
			points[1].y = rect.r_height + diff - (isbm ? 1 : 0) - finetune;
			points[2].x = leftoff;
			points[2].y = rect.r_height + diff - (isbm ? 1 : 0) - finetune;

			XSetForeground(dpy, gc, bg3);
			if (!isbm) {
				if (no_stipple) {
					XSetFunction(dpy, gc, func);
				}
				else {
					bitmap = (Pixmap) xv_get(screen, XV_KEY_DATA,
													SCREEN_BG1_PIXMAP);
					if (bitmap == (Pixmap) NULL) {
						bitmap = XCreatePixmapFromBitmapData(dpy,
							d, bg1_bits, bg1_width, bg1_height, fg, gwyn, 1);
						xv_set(screen,
								XV_KEY_DATA,SCREEN_BG1_PIXMAP, bitmap,
								NULL);
					}
					XSetStipple(dpy, gc, bitmap);
					XSetFillStyle(dpy, gc, FillOpaqueStippled);
					XSetBackground(dpy, gc, gwyn);
				}
			}
			XFillPolygon(dpy, d, gc, points, 3, Complex, CoordModeOrigin);
			XSetFillStyle(dpy, gc, FillSolid);
			XSetFunction(dpy, gc, GXcopy);

			/* linkes Dreieck */
			points[1].x = leftoff;
			points[1].y = 0;

			XSetForeground(dpy, gc, bg2);
			if (!isbm) {
				if (no_stipple) {
					XSetFunction(dpy, gc, func);
				}
				else {
					bitmap = (Pixmap) xv_get(screen, XV_KEY_DATA,
												SCREEN_BG2_PIXMAP);
					if (bitmap == (Pixmap) NULL) {
						bitmap = XCreatePixmapFromBitmapData(dpy,
							d, bg2_bits, bg2_width, bg2_height, fg, gwyn, 1);
						xv_set(screen,
								XV_KEY_DATA,SCREEN_BG2_PIXMAP, bitmap,
								NULL);
					}
					XSetStipple(dpy, gc, bitmap);
					XSetFillStyle(dpy, gc, FillOpaqueStippled);
					XSetBackground(dpy, gc, gwyn);
				}
			}
			XFillPolygon(dpy, d, gc, points, 3, Complex, CoordModeOrigin);
			XSetFillStyle(dpy, gc, FillSolid);
			XSetFunction(dpy, gc, GXcopy);

			XSetForeground(dpy, gc, bg1);
			XFillRectangle(dpy, d, gc,
								leftoff + (isbm ? 1 : 0),
								0,
								(unsigned)rect.r_width + diff - finetune,
								(unsigned)rect.r_height + diff - finetune);
			break;
	}
}

/*
 * notice_draw_borders
 * Draws the notice window border as well as the notice pane border.
 * The x, y, width, and height parameters passed in correspond to the
 * position and dimensions of the notice window.
 */
static void notice_draw_borders(Xv_window	window, int x, int y, int width, int height, int is_toplevel_window)
{
    Display		*display;
    XSegment		seg[5];
    unsigned long   	value_mask;
    XGCValues       	val;
    GC			gc;
    unsigned long	white;
    unsigned long	bg3;
    unsigned long	fg;
    Window		w;
    Cms			cms;
    Xv_Drawable_info	*info;
    notice_handle	notice;
    int			paneX;
    int			paneY;
    int			paneWidth;
    int			paneHeight;
    int			pane_notice_edge_dist;
    int			i;
	screen_ui_style_t ui_style;

    DRAWABLE_INFO_MACRO(window, info);
    display = xv_display(info);
    w = xv_xid(info);
/* 	fprintf(stderr, "%s-%d: %s, xid=%lx [%d,%d,%d,%d]\n", */
/* 			__FILE__,__LINE__,__FUNCTION__, */
/* 			w, x, y, width, height); */
    notice = (notice_handle)xv_get(window, XV_KEY_DATA, notice_context_key);

    /*
     * Get Cms, pixel values
     */
    cms = xv_get(window, WIN_CMS, NULL);
    bg3 = xv_get(cms, CMS_PIXEL, 2, NULL);
    white = xv_get(cms, CMS_PIXEL, 3, NULL);
    fg = xv_get(cms, CMS_FOREGROUND_PIXEL);
	ui_style = xv_get(xv_screen(info), SCREEN_UI_STYLE);

    /*
     ****************************
     * Draw notice window borders
     ****************************
     */

    /*
     * Find a GC
     */
    gc = (GC)xv_find_proper_gc(display, info, PW_VECTOR);

    /*
     * Notice border color is foreground color
     */
    val.foreground = fg;
    val.line_style = LineSolid;
    val.line_width = 1;
    value_mask = GCLineStyle | GCLineWidth | GCForeground;
    XChangeGC(display, gc, value_mask, &val);

    if (is_toplevel_window)  {
        /*
         * Draw notice border 'notice_border_width' pixels wide
         */
        for (i=0; i < (NOTICE_BORDER_WIDTH(notice->scale)); ++i)  {
            /*
	     * Code to draw border to show 'raised' effect
            olgx_draw_box(ginfo, w,
                        i,
                        i,
                        width - (2*i),
                        height - (2*i),
                        OLGX_NORMAL, 0);
            */

/* fprintf(stderr, "%s-%d: %lx w=%d\n", __FILE__, __LINE__, w, width); */
            XDrawRectangle(display, w, gc, x+i, y+i,
							(unsigned )((width - 2*i) - 1),
							(unsigned )((height - 2*i) - 1));
        }
    }

    /*
     **************************
     * Draw notice pane borders
     **************************
     * REMINDER:
     * The notice pkg does its own rendering here. When olgx
     * supports chiseled boxes, then the notice pkg should use
     * that.
     */
    if (is_toplevel_window)  {
        pane_notice_edge_dist = NOTICE_BORDER_WIDTH(notice->scale) +
				PANE_NOTICE_BORDER_DIST(notice->scale);
    }
    else  {
        pane_notice_edge_dist = 0;
    }
    paneX = x + pane_notice_edge_dist;
    paneY = y + pane_notice_edge_dist;
    paneWidth = width - (2 * pane_notice_edge_dist);
    paneHeight = height - (2 * pane_notice_edge_dist);

    /*
     * Code to draw border line to show 'raised' effect
    olgx_draw_box(ginfo, w,
			paneX,
			paneY,
			paneWidth,
			paneHeight,
			OLGX_NORMAL, 0);
    */

    /*
     * Pane border color
     *	light colored lines - white
     *	dark colored lines - bg3
     */
	if (ui_style == SCREEN_UIS_3D_COLOR) {
		val.foreground = bg3;
		XChangeGC(display, gc, GCForeground, &val);

		/*
		 * Draw lines to give 'chiseled' look
		 * Draw dark lines first
		 */
		seg[0].x1 = paneX;
		seg[0].y1 = paneY + (paneHeight-1);
		seg[0].x2 = paneX;
		seg[0].y2 = paneY;

		seg[1].x1 = paneX;
		seg[1].y1 = paneY;
		seg[1].x2 = paneX + (paneWidth-1) - 1;
		seg[1].y2 = paneY;

		seg[2].x1 = paneX + (paneWidth-1) - 1;
		seg[2].y1 = paneY + 1;
		seg[2].x2 = paneX + (paneWidth-1) - 1;
		seg[2].y2 = paneY + (paneHeight-1) - 1;

		seg[3].x1 = paneX + (paneWidth-1) - 1;
		seg[3].y1 = paneY + (paneHeight-1) - 1;
		seg[3].x2 = paneX + 2;
		seg[3].y2 = paneY + (paneHeight-1) - 1;

		XDrawSegments(display, w, gc, seg, 4);

		/*
		 * Set gc to draw light lines
		 */
		if (ui_style != SCREEN_UIS_2D_COLOR) {
			val.foreground = white;
			XChangeGC(display, gc, GCForeground, &val);
		}

		/*
		 * Draw light lines next
		 */
		seg[0].x1 = paneX + (paneWidth-1);
		seg[0].y1 = paneY;
		seg[0].x2 = paneX + (paneWidth-1);
		seg[0].y2 = paneY + (paneHeight-1);

		seg[1].x1 = paneX + (paneWidth-1);
		seg[1].y1 = paneY + (paneHeight-1);
		seg[1].x2 = paneX + 1;
		seg[1].y2 = paneY + (paneHeight-1);

		seg[2].x1 = paneX + 1;
		seg[2].y1 = paneY + (paneHeight-1);
		seg[2].x2 = paneX + 1;
		seg[2].y2 = paneY + 1;

		seg[3].x1 = paneX + 1;
		seg[3].y1 = paneY + 1;
		seg[3].x2 = paneX + (paneWidth-1) - 2;
		seg[3].y2 = paneY + 1;

		XDrawSegments(display, w, gc, seg, 4);
	}
	else {
		unsigned pw = paneWidth;
		unsigned ph = paneHeight;

		/* the DrawSeqments fiddling is ugly */
		val.foreground = fg;
		val.line_width = 3;
		XChangeGC(display, gc, GCForeground | GCLineWidth, &val);
		XDrawRectangle(display, w, gc, paneX, paneY, pw-1, ph-1);
		val.line_width = 1;
		XChangeGC(display, gc, GCLineWidth, &val);
/* 		XDrawLine(display, w, gc, 0, -50, 100, 100); */
	}
}

static void notice_drawbox(Xv_Window pw, struct rect *rectp, int quadrant,
									int leftoff, int topoff)
{
	notice_handle notice;
	Display *display;
	Drawable d;
	GC  fill_gc;
	XGCValues gc_val;
	Xv_Drawable_info *info;
	Cms cms;
	unsigned long bg;
	unsigned long bg2;
	unsigned long bg3;
	Xv_screen screen = XV_SCREEN_FROM_WINDOW(pw);
	screen_ui_style_t ui_style = xv_get(screen, SCREEN_UI_STYLE);

	/*
	 * Set information needed by Xlib calls
	 */
	DRAWABLE_INFO_MACRO(pw, info);
	display = xv_display(info);
	d = xv_xid(info);

	notice = (notice_handle) xv_get(pw, XV_KEY_DATA, notice_context_key);

	/*
	 * Determine which colors to use
	 */
	cms = xv_get(pw, WIN_CMS);
	bg = xv_get(cms, CMS_BACKGROUND_PIXEL);

	/* this works in the 3D case and plays no role in the 2D_BW case -
	 * but in the 2D_COLOR case the three bg, bg2, bg3 are equal....
	 */
	if (ui_style != SCREEN_UIS_2D_COLOR) {
		bg2 = xv_get(cms, CMS_PIXEL, 1);
		bg3 = xv_get(cms, CMS_PIXEL, 2);
	}
	else {
		XColor xbg, xbg2, xbg3, xhigh;
		Colormap cmap;

		xbg.pixel = bg;
		cmap = DefaultColormap(display, (int)xv_get(screen, SCREEN_NUMBER));
		XQueryColor(display, cmap, &xbg);
		olgx_calculate_3Dcolors((XColor *) NULL, &xbg, &xbg2, &xbg3, &xhigh);
		XAllocColor(display, cmap, &xbg2);
		XAllocColor(display, cmap, &xbg3);
		bg2 = xbg2.pixel;
		bg3 = xbg3.pixel;
	}

	/*
	 * Most of the code here is straight out from libxvin/pw/pw_plygon2.c
	 * Using the server images created, get/set the gc to be used by XFillPolygon
	 * The reason, the GCs are NOT get/set separately, as are the server images,
	 * is because xv_find_proper_gc may return the same GC when called the 2nd
	 * time.
	 */
	fill_gc = (GC) xv_find_proper_gc(display, info, PW_POLYGON2);
	gc_val.fill_style = FillSolid;
	XChangeGC(display, fill_gc, GCFillStyle, &gc_val);

	notice_draw_polygons(display, notice, FALSE, d, fill_gc, bg, bg2, bg3,
				rectp, quadrant, leftoff, topoff);

	/*
	 * draw box
	 */

	/* screen locking notices had no border - I'll try this: */
	if (notice->lock_screen) {
		struct rect rect;

		rect = *rectp;

		notice_draw_borders(pw,
			(int)(rect.r_left - PANE_XY(NOTICE_IS_TOPLEVEL, notice->scale)),
			(int)(rect.r_top - PANE_XY(NOTICE_IS_TOPLEVEL, notice->scale)),
			(int)(rect.r_width + PANE_NOTICE_DIFF(NOTICE_IS_TOPLEVEL,
								notice->scale)),
			(int)(rect.r_height + PANE_NOTICE_DIFF(NOTICE_IS_TOPLEVEL,
								notice->scale)), NOTICE_IS_TOPLEVEL);
	}
}


static void fullscreen_win_do_nothing(Xv_Window window, Event *event)
{
}

static void fullscreen_win_event_proc(Xv_Window window, Event *event)
{
	notice_prep_t *p = (notice_prep_t *)xv_get(window, WIN_CLIENT_DATA);

	switch (event_id(event)) {
		case WIN_MAP_NOTIFY:
			/* when the next WIN_REPAINT comes, simply repaint */
			p->event_state = 1;
			break;
		case WIN_REPAINT:
			if (p->have_shape) {
				notice_drawbox(window, &p->rect, p->quadrant, p->leftoff,
														p->topoff);
			}
			else {
				if (p->event_state == 1) {
					p->event_state = 0;
					notice_drawbox(window, &p->rect, p->quadrant, p->leftoff,
														p->topoff);
				}
				else {
					Display *dpy = (Display *)xv_get(window, XV_DISPLAY);
					Window xid = xv_get(window, XV_XID);

					XUnmapWindow(dpy, xid);
					XMapWindow(dpy, xid);
				}
			}
			break;
		case WIN_CLIENT_MESSAGE:
			fprintf(stderr, "%s-%d: client message\n", __FUNCTION__, __LINE__);
			if (event_xevent(event)->xclient.message_type == 
				xv_get(XV_SERVER_FROM_WINDOW(window), SERVER_ATOM,"_DRA_TRACE"))
			{
				Notice_info *notice = (Notice_info *)xv_get(window,
											XV_KEY_DATA, notice_context_key);

				fprintf(stderr, "window: [%ld, %ld, %ld, %ld]\n", 
								xv_get(window, XV_X),
								xv_get(window, XV_Y),
								xv_get(window, XV_WIDTH),
								xv_get(window, XV_HEIGHT));
				/* root window:
					fprintf(stderr, "notice_screen_rect: ");
					rect_print(&p->notice_screen_rect);
				*/
				fprintf(stderr, "rect              : ");
				rect_print(&p->rect);
				fprintf(stderr, "quadrant %d: leftoff %d, topoff %d\n", 
									p->quadrant, p->leftoff, p->topoff);
				fprintf(stderr, "subframe: [%ld, %ld, %ld, %ld]\n", 
								xv_get(notice->sub_frame, XV_X),
								xv_get(notice->sub_frame, XV_Y),
								xv_get(notice->sub_frame, XV_WIDTH),
								xv_get(notice->sub_frame, XV_HEIGHT));
			}
			break;
	}
}

static int notice_quadrant(Rect	notice_screen_rect, int	x, int y)
{
    int             quadrant;

    if ((x <= notice_screen_rect.r_width / 2) && (y <= notice_screen_rect.r_height / 2))
	quadrant = 0;
    else if ((x > notice_screen_rect.r_width / 2) && (y <= notice_screen_rect.r_height / 2))
	quadrant = 1;
    else if ((x > notice_screen_rect.r_width / 2) && (y > notice_screen_rect.r_height / 2))
	quadrant = 2;
    else
	quadrant = 3;

    return (quadrant);
}

/*
 * font char/pixel conversion routines
 */

static int notice_text_width(Xv_Font font, CHAR *str)
{
    struct pr_size  size;

#ifdef OW_I18N
    size = xv_pf_textwidth_wc(STRLEN(str), font, str);
#else
    size = xv_pf_textwidth((int)STRLEN(str), font, str);
#endif

    return (size.x);
}

static int notice_button_width(Xv_Font font, Graphics_info *ginfo, notice_buttons_handle	button)
{
    button->button_rect.r_width = notice_text_width(font, button->string) +
	2*ButtonEndcap_Width(ginfo);
    button->button_rect.r_height = Button_Height(ginfo);
    return (button->button_rect.r_width);
}


/*
 ************************************
 * Routines for screen-locking notice
 ************************************
 */
static void notice_get_notice_size(notice_handle notice, struct rect *rect, int *buttons_width)
{
    Graphics_info	*ginfo = notice->ginfo;
    notice_msgs_handle	curMsg = notice->msg_info;
    notice_buttons_handle curButton = notice->button_info;
    int		notice_width = 0, notice_height = 0;
    int		maxButHeight = 0;
    int		totalButWidth = 0;
    int         chrht;
    Xv_Font	this_font = (Xv_Font)(notice->notice_font);
    int		i;

    /*
     * get character width and height
     */
    chrht = xv_get(this_font, FONT_DEFAULT_CHAR_HEIGHT);

    /*
     * Scan thru messages
     */
    while (curMsg)  {
        int         str_width = 0;

	/*
	 * Get string width
	 */
        str_width = notice_text_width(this_font, curMsg->string);

	/*
	 * Maintain MAX width of all strings
	 */
	notice_width = MAX(notice_width, str_width);

	/*
	 * For each message string, add height
	 */
	notice_height += chrht;

	/*
	 * Go on to next message string
	 */
        curMsg = curMsg->next;

	/*
	 * Don't add message gaps for last message item
	 */
	if (curMsg)  {
            notice_height += MSG_VERT_GAP(notice->scale);
	}
    }

    /*
     * Add margins to current notice width
     */
    notice_width += (2 * HORIZ_MSG_MARGIN(notice->scale));

    i = 0;

    /*
     * Scan thru buttons
     */
    while (curButton) {
	int             this_buttons_width = 0;

	/*
	 * get button width
	 */
	this_buttons_width = notice_button_width(this_font, ginfo,
					curButton);
	/*
	 * Increment width of total buttons
	 */
	totalButWidth += this_buttons_width;

	/*
	 * Increment button ct.
	 */
	i++;

	/*
	 * Go on to next button
	 */
	curButton = curButton->next;
    }

    /*
     * All buttons have same height, so take the first one
     */
    maxButHeight = notice->button_info->button_rect.r_height;

    /*
     * Add horizontal gap and margins to total button width
     */
    totalButWidth += (i-1) * BUT_HORIZ_GAP(notice->scale);

    /*
     * The button portion height is max of the OPENLOOK value
     * and the actual button height
     */
    BUT_PORTION_HEIGHT(notice->scale) = MAX(BUT_PORTION_HEIGHT(notice->scale),
						maxButHeight);

    /*
     * Add to panel height the top/bottom margins and the height of
     * buttons.
     */
    notice_height += (2 * VERT_MSG_MARGIN(notice->scale)) + BUT_PORTION_HEIGHT(notice->scale);

    /*
     * notice width is max of strings width(current notice width) and
     * total width of buttons
     */
    notice_width = MAX(notice_width,
		(totalButWidth + (2 * HORIZ_MSG_MARGIN(notice->scale))) );

    *buttons_width = totalButWidth;

    rect->r_top = 0;
    rect->r_left = 0;
    rect->r_width = notice_width;
    rect->r_height = notice_height;
}

static void notice_do_bell(notice_handle notice)
{
    Xv_Drawable_info	*info;
    struct timeval  wait;
	int notice_use_audible_bell = defaults_get_enum("openWindows.beep",
							"OpenWindows.Beep", bell_types);

    if (!notice_use_audible_bell)  {
		return;
    }

    DRAWABLE_INFO_MACRO(notice->client_window, info);
    wait.tv_sec = 0;
    wait.tv_usec = 100000;
    if (!notice->dont_beep && (notice->beeps > 0)) {
	int             i = notice->beeps;
	while (i--)
	    win_beep(xv_display(info), wait);
    }
}

static void notice_get_button_pin_points(notice_handle	notice)
{
    Graphics_info	*ginfo = notice->ginfo;
    notice_buttons_handle curr;
    Xv_Font	this_font = (Xv_Font)(notice->notice_font);

    for (curr = notice->button_info; curr != NULL; curr = curr->next) {
        (void)notice_button_width(this_font, ginfo, curr);
    }
}


static void notice_prepare_for_shadow(Notice_info *notice, notice_prep_t *p)
{
	Display *dpy;
	int three_d, shape_avail;
	Cms cms = XV_NULL;
	unsigned w, h;

	p->root_window = xv_get(p->client_window, XV_ROOT);

	win_getrect(p->root_window, &p->notice_screen_rect);

	DRAWABLE_INFO_MACRO(p->client_window, p->info);

	three_d = (SCREEN_UIS_3D_COLOR ==
				(screen_ui_style_t)xv_get(xv_screen(p->info), SCREEN_UI_STYLE));

	/*
	 * Use client window CMS
	 */
	cms = xv_cms(p->info);

	if (!notice->fullscreen_window) {
		notice->fullscreen_window = xv_create(p->root_window, WINDOW,
				WIN_TRANSPARENT,
				WIN_TOP_LEVEL_NO_DECOR, TRUE,	/* no wmgr decoration */
				WIN_SAVE_UNDER, TRUE,	/* no damage caused */
				WIN_BIT_GRAVITY, ForgetGravity,
				WIN_CMS, cms,
				WIN_CONSUME_EVENTS,
					WIN_VISIBILITY_NOTIFY,
					WIN_MAP_NOTIFY,
					WIN_REPAINT,
					WIN_MOUSE_BUTTONS,
					WIN_ASCII_EVENTS,
					WIN_UP_ASCII_EVENTS,
					LOC_WINENTER,
					LOC_WINEXIT,
					LOC_DRAG,
					LOC_MOVE,
					NULL,
				XV_VISUAL, xv_get(cms, XV_VISUAL),
				XV_FONT, notice->notice_font,
				XV_KEY_DATA, notice_context_key, notice,
				/* safe is unneeded */
				WIN_NOTIFY_IMMEDIATE_EVENT_PROC, (notice->lock_screen
												? fullscreen_win_do_nothing
												: fullscreen_win_event_proc),
				XV_SHOW, FALSE,
				NULL);

		notice->ginfo = xv_init_olgx(notice->fullscreen_window, &three_d,
				xv_get(notice->fullscreen_window, XV_FONT));

	}

	/*
	 * Get mouse positioning info
	 */
	if (notice->focus_specified) {
		int new_x, new_y;

		win_translate_xy(p->client_window, p->root_window,
				notice->focus_x, notice->focus_y, &new_x, &new_y);
		p->x = p->old_mousex = new_x;
		p->y = p->old_mousey = new_y;
	}
	else {
		Rect *old_mouse_position;

		old_mouse_position = (Rect *) xv_get(p->root_window, WIN_MOUSE_XY);
		p->x = p->old_mousex = notice->focus_x = old_mouse_position->r_left;
		p->y = p->old_mousey = notice->focus_y = old_mouse_position->r_top;
	}

	/* Get size of rectangle */
	notice_get_notice_size(notice, &p->rect, &p->buttons_width);

	notice_get_button_pin_points(notice);

	/*
	 * Now offset for shadow
	 */
	p->leftoff = p->topoff = APEX_DIST(notice->scale);

	/*
	 * If x and y position is somehow offscreen, default to center of
	 * screen
	 */
	if (p->x < 0) {
		p->x = p->old_mousex =
				(p->notice_screen_rect.r_left + p->notice_screen_rect.r_width) / 2;
	}

	if (p->y < 0) {
		p->y = p->old_mousey =
				(p->notice_screen_rect.r_top + p->notice_screen_rect.r_height) / 2;
	}

	p->quadrant = notice_quadrant(p->notice_screen_rect, p->x, p->y);

	switch (p->quadrant) {
		case 0:
			p->left_placement = p->old_mousex;
			p->top_placement = p->old_mousey;
			p->rect.r_left = p->leftoff + PANE_XY(NOTICE_IS_TOPLEVEL, notice->scale);
			p->rect.r_top = p->topoff + PANE_XY(NOTICE_IS_TOPLEVEL, notice->scale);
			break;
		case 1:
			p->left_placement = p->old_mousex - (p->rect.r_width +
					PANE_NOTICE_DIFF(NOTICE_IS_TOPLEVEL,
							notice->scale) + p->leftoff);
			p->top_placement = p->old_mousey;
			p->rect.r_left = PANE_XY(NOTICE_IS_TOPLEVEL, notice->scale);
			p->rect.r_top = p->topoff + PANE_XY(NOTICE_IS_TOPLEVEL, notice->scale);
			break;
		case 2:
			p->left_placement = p->old_mousex - (p->rect.r_width +
					PANE_NOTICE_DIFF(NOTICE_IS_TOPLEVEL, notice->scale)
					+ p->leftoff);
			p->top_placement =
					p->old_mousey - (p->rect.r_height +
					PANE_NOTICE_DIFF(NOTICE_IS_TOPLEVEL, notice->scale)
					+ p->topoff);
			p->rect.r_left = PANE_XY(NOTICE_IS_TOPLEVEL, notice->scale);
			p->rect.r_top = PANE_XY(NOTICE_IS_TOPLEVEL, notice->scale);
			break;
		case 3:
			p->left_placement = p->old_mousex;
			p->top_placement = p->old_mousey - (p->rect.r_height +
					PANE_NOTICE_DIFF(NOTICE_IS_TOPLEVEL,
							notice->scale) + p->topoff);
			p->rect.r_left = p->leftoff + PANE_XY(NOTICE_IS_TOPLEVEL, notice->scale);
			p->rect.r_top = PANE_XY(NOTICE_IS_TOPLEVEL, notice->scale);
			break;
	}

	w = p->rect.r_width + PANE_NOTICE_DIFF(NOTICE_IS_TOPLEVEL, notice->scale)
					+ p->leftoff;
	h = p->rect.r_height + PANE_NOTICE_DIFF(NOTICE_IS_TOPLEVEL, notice->scale)
					+ p->topoff;

	p->have_shape = FALSE;
	shape_avail = (int)xv_get(XV_SERVER_FROM_WINDOW(p->root_window),
										SERVER_SHAPE_AVAILABLE);

	dpy = xv_display(p->info);
	if (shape_avail) {
		Window xid = xv_xid(p->info);
		Pixmap bm = XCreatePixmap(dpy, xid, w, h, 1);
		XGCValues gc_val;
		GC gc;
		Xv_screen scrn = XV_SCREEN_FROM_WINDOW(p->root_window);

		gc_val.foreground = 0;
		gc = XCreateGC(dpy, bm, GCForeground, &gc_val);
		XFillRectangle(dpy, bm, gc, 0, 0, w+1, h+1);

		/* here we only fill a BITmap  which we use for XShapeCombineMask */
		notice_draw_polygons(dpy, notice, TRUE, bm, gc, 1L, 1L, 1L,
						&p->rect, p->quadrant, p->leftoff, p->topoff);

		if (xv_get(scrn, SCREEN_OLWM_MANAGED)) {
			XFillRectangle(dpy, bm, gc, p->rect.r_left, p->rect.r_top,
					(unsigned)p->rect.r_width+1, (unsigned)p->rect.r_height+1);
		}
		XShapeCombineMask(dpy, xv_get(notice->fullscreen_window, XV_XID),
					ShapeBounding, 0, 0, bm, ShapeSet);
		XFreeGC(dpy, gc);
		XFreePixmap(dpy, bm);
		p->have_shape = TRUE;
	}

	if (notice->sub_frame) {
		char *wintyp;
		Xv_server srv = XV_SERVER_FROM_WINDOW(p->root_window);
		Window fsw = xv_get(notice->fullscreen_window, XV_XID);
		Window fxid = xv_get(notice->sub_frame, XV_XID);
		Atom nwt[5], wt = xv_get(srv, SERVER_ATOM, "_NET_WM_WINDOW_TYPE");
		int numwts;
		Window wins[2];
		XChangeProperty(xv_display(p->info), fxid,
				xv_get(srv, SERVER_ATOM, "_OL_NOTICE_EMANATION"),
				XA_WINDOW, 32, PropModeReplace, (unsigned char *)&fsw, 1);

		/* oh, dear desktop freaks..... */

		/* there seems to be no reliable way to tell the fucking
		 * 'desktop system window managers' that this toplevel window
		 * should be 'minimally decorated' ....
		 */
		if ((wintyp = getenv("WIN_TYPE"))) {
			char tmpbuf[200];

			sprintf(tmpbuf, "_NET_WM_WINDOW_TYPE_%s", wintyp);
			nwt[0] = xv_get(srv, SERVER_ATOM, tmpbuf);
			numwts = 1;
		}
		else {
			int idx = 0;
			/* when I say _NET_WM_WINDOW_TYPE_SPLASH, the notice **is** 
			 * minimally decorated - but it vanishes on the first 
			 * ButtonPress event anywhere...
			 */

			/* in fact DESKTOP and DOCK are not at all decorated,
			 * at least under XFCE, KDE, Openbox...
			 *
			 * A _NET_WM_WINDOW_TYPE_MENU has a header and functions
			 * fullscreen (!!!) and close - this would also be
			 * acceptable
			 */
			nwt[idx++] = xv_get(srv,SERVER_ATOM,"_NET_WM_WINDOW_TYPE_DOCK");
			nwt[idx++] = xv_get(srv,SERVER_ATOM,"_NET_WM_WINDOW_TYPE_DESKTOP");
			nwt[idx++] = xv_get(srv,SERVER_ATOM,"_NET_WM_WINDOW_TYPE_MENU");
			nwt[idx++] = xv_get(srv,SERVER_ATOM,"_NET_WM_WINDOW_TYPE_DIALOG");
			numwts = idx;
		}

		XChangeProperty(dpy, fxid, wt,
				XA_ATOM, 32, PropModeReplace, (unsigned char *)nwt, numwts);

		XChangeProperty(dpy, fsw, wt,
				XA_ATOM, 32, PropModeReplace, (unsigned char *)nwt, numwts);

		wins[0] = (Window) xv_get(notice->client_window, XV_XID);
		XChangeProperty(dpy, fsw, xv_get(srv, SERVER_WM_TRANSIENT_FOR),
				XA_WINDOW, 32, PropModeReplace, (unsigned char *)wins, 1);
	}

	if (xv_get(XV_SCREEN_FROM_WINDOW(p->root_window), SCREEN_ENHANCED_OLWM)) {
		xv_set(notice->fullscreen_window,
				WIN_CLIENT_DATA, p,
				XV_X, p->left_placement,
				XV_Y, p->top_placement,
				XV_WIDTH, w,
				XV_HEIGHT, h,
				XV_SHOW, TRUE,
				NULL);
	}
	else {
		/* when I tried this under KDE (kwin_x11), the nonrectangular
		 * fullscreen_window appeared, but covered the notice, so that
		 * I could push no buttons...
		 */
	}

	/*
	 * BUG: Should wait for notice window to map
	 */

	/* we do that ONLY if lock_screen, BUT NOT if lock_screen_looking  */
	if (notice->lock_screen) {
		p->fs = xv_create(p->root_window, FULLSCREEN,
				FULLSCREEN_INPUT_WINDOW, notice->fullscreen_window,
				WIN_CONSUME_EVENTS,
					WIN_VISIBILITY_NOTIFY,
					WIN_REPAINT,
					WIN_MOUSE_BUTTONS,
					WIN_ASCII_EVENTS,
					WIN_UP_ASCII_EVENTS,
					LOC_WINENTER,
					LOC_WINEXIT,
					LOC_DRAG,
					LOC_MOVE,
					NULL,
				NULL);
	}

	SERVERTRACE((44, "SERVER_JOURNALLING?\n"));
	if (xv_get(xv_server(p->info), SERVER_JOURNALLING)) {
		SERVERTRACE((44, " YES \n"));
		xv_set(xv_server(p->info), SERVER_JOURNAL_SYNC_EVENT, 1, NULL);
	}

	notice_do_bell(notice);

	/*
	 * then draw empty box and shadow
	 */
	notice_drawbox(notice->fullscreen_window, &p->rect, p->quadrant,
									p->leftoff, p->topoff);
}

static int notice_offset_from_baseline(Xv_Font	font)
{
#ifdef  OW_I18N
    XFontSet            font_set;
    XFontSetExtents     *font_set_extents;
#else
    XFontStruct		*x_font_info;
#endif /* OW_I18N */

    if (font == XV_NULL)
	return (0);

#ifdef OW_I18N
    font_set = (XFontSet)xv_get(font, FONT_SET_ID);
    font_set_extents = XExtentsOfFontSet(font_set);
    return(font_set_extents->max_ink_extent.y);
#else
    x_font_info = (XFontStruct *)xv_get(font, FONT_INFO);
    return (x_font_info->ascent);
#endif /* OW_I18N */
}

static void notice_paint_button(Xv_Window pw, notice_buttons_handle button,
						int invert, Graphics_info *ginfo, int three_d)
{
	Xv_Drawable_info *info;
	int state;

	DRAWABLE_INFO_MACRO(pw, info);
	if (invert) state = OLGX_INVOKED;
	else if (three_d) state = OLGX_NORMAL;
	else state = OLGX_NORMAL | OLGX_ERASE;
	if (button->is_yes)
		state |= OLGX_DEFAULT;
	olgx_draw_button(ginfo, xv_xid(info), button->button_rect.r_left,
			button->button_rect.r_top, button->button_rect.r_width, 0,

#ifdef OW_I18N
			button->string, state | OLGX_LABEL_IS_WCS);
#else
			button->string, state);
#endif
}

static void notice_build_button(Xv_Window pw, int x,int y,
			notice_buttons_handle button, Graphics_info *ginfo, int three_d)
{
    button->button_rect.r_top = y;
    button->button_rect.r_left = x;
    notice_paint_button(pw, button, NOTICE_NORMAL_BUTTON, ginfo, three_d);
}

static void notice_do_buttons(notice_handle notice, struct rect *rect,
		int starty, notice_buttons_handle this_button_only, int totalButWidth)
{
	Graphics_info *ginfo = notice->ginfo;
	int three_d = notice->three_d;
	int x, y;
	int paneWidth = rect->r_width;
	notice_buttons_handle curButton;
	notice_msgs_handle curMsg;
	Xv_Window fswin = notice->fullscreen_window;
	Xv_Font this_font = (Xv_Font) (notice->notice_font);

	/* Additional vars for replacing pw_ calls */
	int chrht;

	if (starty < 0) {
		/*
		 * later, consider centering here
		 */
		chrht = xv_get(this_font, FONT_DEFAULT_CHAR_HEIGHT);

		y = rect->r_top + VERT_MSG_MARGIN(notice->scale);

		curMsg = notice->msg_info;
		while (curMsg) {
			/*
			 * Add character height to vertical position
			 */
			y += chrht;

			curMsg = curMsg->next;

			/*
			 * Don't add message gaps for last message item
			 */
			if (curMsg) {
				y += MSG_VERT_GAP(notice->scale);
			}
		}
	}
	else {
		y = starty;
	}

	curButton = notice->button_info;

	x = rect->r_left + (paneWidth - totalButWidth) / 2;
	y += VERT_MSG_MARGIN(notice->scale) +
			((BUT_PORTION_HEIGHT(notice->scale) -
					curButton->button_rect.r_height) / 2);

	while (curButton) {
		if (this_button_only) {
			if (this_button_only == curButton) {
				notice_build_button(fswin, x, y, curButton, ginfo, three_d);
				curButton = NULL;
			}
			else {
				x += curButton->button_rect.r_width +
						BUT_HORIZ_GAP(notice->scale);

				curButton = curButton->next;
			}
		}
		else {
			notice_build_button(fswin, x, y, curButton, ginfo, three_d);
			x += curButton->button_rect.r_width + BUT_HORIZ_GAP(notice->scale);

			curButton = curButton->next;
		}
	}
}

static void notice_layout(notice_handle notice, struct rect *rect, int totalButWidth)
{
    int				x, y;
    int				paneWidth = rect->r_width;
    notice_msgs_handle		curMsg;
    Xv_Font			this_font = (Xv_Font)(notice->notice_font);
    /* Additional vars for replacing pw_ calls */
    Xv_Drawable_info		*info;
    Display			*display;
    Drawable			d;
    unsigned long   		value_mask;
    XGCValues       		val;
    GC				gc;
    int				chrht;
    int				ascent = notice_offset_from_baseline(this_font);
#ifdef  OW_I18N
    XFontSet        font_set;
#endif /* OW_I18N */
    /*
     * Set information needed by Xlib calls
     */
    DRAWABLE_INFO_MACRO(notice->fullscreen_window, info);
    display = xv_display(info);
    d = xv_xid(info);

    /*
     * later, consider centering here
     */
    chrht = xv_get(this_font, FONT_DEFAULT_CHAR_HEIGHT);

    y = rect->r_top + VERT_MSG_MARGIN(notice->scale);

    if (notice->msg_info) {
        XID             font;

		/*
		 * Set GC needed by XDrawImageString()
		 */
		/* this gc will be overwritten: */
/*         gc = (GC)xv_find_proper_gc(display, info, PW_TEXT); */

#ifdef OW_I18N
        font_set = (XFontSet)xv_get( this_font , FONT_SET_ID );
#endif /* OW_I18N */
		font = xv_get(this_font, XV_XID);

        gc = (GC)xv_find_proper_gc(display, info, PW_VECTOR);
        val.background = xv_bg(info);
        val.foreground = xv_fg(info);
        val.font = font;
        value_mask = GCForeground | GCBackground | GCFont;
        XChangeGC(display, gc, value_mask, &val);
    }

    curMsg = notice->msg_info;
    while(curMsg) {
        int	str_width, len;
	CHAR	*str;

        str = curMsg->string;

        if ((len = STRLEN(str))) {
            str_width = notice_text_width(this_font, (CHAR *)str);

            x = (rect->r_left + (paneWidth - str_width)/2);

#ifdef OW_I18N
            XwcDrawImageString(display, d, font_set, gc,
                        x,
                        y + ascent,
                        str, len);
#else
            XDrawImageString(display, d, gc,
                        x,
                        y + ascent,
                        str, len);
#endif /* OW_I18N */
        }

	/*
	 * Add character height to vertical position
	 */
        y += chrht;

	curMsg = curMsg->next;

	/*
	 * Don't add message gaps for last message item
	 */
	if (curMsg)  {
            y += MSG_VERT_GAP(notice->scale);
	}
    }

    notice_do_buttons(notice, rect, y, NULL, totalButWidth);
}

static int notice_get_owner_frame(Notice_info	*notice)
{
    Xv_window	client_window, owner_window;

    if (!notice)  {
	return(XV_ERROR);
    }

    owner_window = client_window = notice->client_window;

    if (!client_window)  {
	return(XV_ERROR);
    }

    /*
     * Check if client window is a frame.
     * If not, try to get it
     */
    if (!xv_get(client_window, XV_IS_SUBTYPE_OF, FRAME_CLASS))  {
	/*
	 * client_window not Frame
	 * Get it's WIN_FRAME
	 */
        owner_window = xv_get(client_window, WIN_FRAME);

	if (owner_window)  {
            if (!xv_get(owner_window, XV_IS_SUBTYPE_OF, FRAME_CLASS))  {
		/*
		 * If WIN_FRAME does not return a Frame
		 * reset to NULL so can continue search
		 */
		owner_window = (Frame)NULL;
	    }
	}

	if (!owner_window)  {
	    Frame	frame_obj;

	    /*
	     * Get WIN_FRAME as key data
	     * This case applies to menus
	     */
            owner_window = xv_get(client_window, XV_KEY_DATA, WIN_FRAME);

	    if (owner_window)  {
                if (!xv_get(owner_window, XV_IS_SUBTYPE_OF, FRAME_CLASS))  {
		    /*
		     * If KEY_DATA using WIN_FRAME does not return a Frame
		     * reset to NULL so can continue search
		     */
		    owner_window = (Frame)NULL;
	        }
	    }

	    if (!owner_window)  {
		/*
		 * Traverse XV_OWNER links until find Frame
		 */
	        for (frame_obj = xv_get(client_window, XV_OWNER);
		     frame_obj; 
		     frame_obj = xv_get(frame_obj, XV_OWNER) )  {
		    if (xv_get(frame_obj, XV_IS_SUBTYPE_OF, FRAME_CLASS))  {
			/*
			 * break out of loop when find a Frame
			 */
		        owner_window = frame_obj;
		        break;
		    }
	        }
	    }
	}

    }

    /*
     * BUG:
     * If cannot find a Frame as owner, use client_window
     */
    if (!owner_window)  {
	owner_window = client_window;
    }

    notice->owner_window = owner_window;

    return(XV_OK);
}

static Xv_window notice_get_focus_win(Notice_info *notice)
{
    if (!notice->owner_window)  {
        notice_get_owner_frame(notice);
    }

    return((Xv_window)xv_get(notice->owner_window, FRAME_FOCUS_WIN));
}

static int notice_show_focus_win(Notice_info *notice,
			notice_buttons_handle	button, Xv_window focus_window, int erase)
{
    Xv_window	fs_win;
    Xv_Drawable_info *image_info;
    Xv_Drawable_info *info;
    Server_image	image;
    GC		gc;
    XGCValues	gc_values;
    unsigned long	valuemask = 0;
    int		x, y, width, height;

    if (!button)  {
	return(XV_ERROR);
    }

    fs_win = notice->fullscreen_window;

    if (!fs_win)  {
	return(XV_ERROR);
    }

    if (!focus_window)  {
	return(XV_ERROR);
    }

    x = button->button_rect.r_left + 
		(button->button_rect.r_width - FRAME_FOCUS_UP_WIDTH)/2;
    y = button->button_rect.r_top + button->button_rect.r_height - FRAME_FOCUS_UP_HEIGHT/2;
    width = FRAME_FOCUS_UP_WIDTH;
    height = FRAME_FOCUS_UP_HEIGHT;


    DRAWABLE_INFO_MACRO(focus_window, info);
    gc = (GC) xv_get(focus_window, XV_KEY_DATA, FRAME_FOCUS_GC);
    if (!gc) {
        /* Create the Graphics Context for the Focus Window */
	/* THIS IS ALSO DONE IN frame_focus_win_event_proc() in fm_input.c*/
        gc_values.fill_style = FillOpaqueStippled;
        gc = XCreateGC(xv_display(info), xv_xid(info), GCFillStyle,
                            &gc_values);
        xv_set(focus_window, XV_KEY_DATA, FRAME_FOCUS_GC, gc, NULL);
    }

    DRAWABLE_INFO_MACRO(fs_win, info);

    if (erase)  {
        gc_values.fill_style = FillSolid;
        gc_values.foreground = xv_bg(info);
    }
    else  {
        image = xv_get(focus_window, XV_KEY_DATA, FRAME_FOCUS_UP_IMAGE);
        DRAWABLE_INFO_MACRO(image, image_info);
        gc_values.fill_style = FillOpaqueStippled;
        gc_values.stipple = xv_xid(image_info);
        gc_values.ts_x_origin = x;
        gc_values.ts_y_origin = y;
        gc_values.background = xv_bg(info);
        gc_values.foreground = xv_fg(info);
	valuemask |= GCStipple | GCTileStipXOrigin | GCTileStipYOrigin
			| GCBackground;
    }

    valuemask |= GCFillStyle | GCForeground;

    XChangeGC(xv_display(info), gc, valuemask, &gc_values);

    XFillRectangle(xv_display(info), xv_xid(info), gc, x, y,
                        (unsigned)width, (unsigned)height);

    if (!erase)  {
        gc_values.ts_x_origin = 0;
        gc_values.ts_y_origin = 0;
        gc_values.fill_style = FillOpaqueStippled;
        XChangeGC(xv_display(info), gc, GCTileStipXOrigin | GCTileStipYOrigin 
		    | GCFillStyle, &gc_values);
    }

    return(XV_OK);
}

static notice_buttons_handle notice_button_for_event(notice_handle notice,
								int x, int y)
{
	register notice_buttons_handle curr;

	if (notice->button_info == NULL)
		return (NULL);
	for (curr = notice->button_info; curr; curr = curr->next) {
		if ((x >= curr->button_rect.r_left)
				&& (x <= (curr->button_rect.r_left + curr->button_rect.r_width))
				&& (y >= curr->button_rect.r_top)
				&& (y <= (curr->button_rect.r_top
								+ curr->button_rect.r_height))) {
			return (curr);
		}
	}
	return ((notice_buttons_handle) 0);
}

static notice_buttons_handle notice_get_prev_button(notice_handle notice,
									notice_buttons_handle	button)
{
	register notice_buttons_handle cur, prev = NULL;
	int last = FALSE;

	if (notice->button_info == NULL) {
		return (NULL);
	}

	if (notice->number_of_buttons == 1) {
		return (notice->button_info);
	}

	if (!button) {
		return (notice->button_info);
	}

	for (cur = notice->button_info; cur; prev = cur, cur = cur->next) {
		if (cur == button) {
			if (prev) {
				return (prev);
			}
			else {
				last = TRUE;
			}
		}
	}

	if (last) {
		return ((notice_buttons_handle) prev);
	}
	else {
		return ((notice_buttons_handle) 0);
	}
}

static void notice_copy_event(notice_handle notice, Event *event)
{
	if (notice->event == (Event *) 0) return;

	*notice->event = *event;
}

static int notice_block_popup(Notice_info *notice)
{
	notice_prep_t p;
	notice_buttons_handle button;
	notice_buttons_handle current_button = NULL;
	notice_buttons_handle prev_button = NULL;
	notice_buttons_handle default_button = NULL;
	Graphics_info *ginfo;
	Event ie;
	Inputmask im;
	int is_highlighted = FALSE;
	int ok_to_toggle_buttons = FALSE;
	int result;
	unsigned short this_event;
	unsigned short this_id;
	unsigned short trigger;
	int x;
	int y;
	Xv_Window focus_window = XV_NULL;
	int mouseless = FALSE, first_repaint_set = FALSE;

	p.event_state = 0;
	p.client_window = notice->client_window;
	p.fs = XV_NULL;
	/*
	 * Check if mouseless is on
	 */
	if (defaults_get_enum("openWindows.keyboardCommands",
					"OpenWindows.KeyboardCommands",
					xv_kbd_cmds_value_pairs) == KBD_CMDS_FULL) {
		mouseless = TRUE;
		focus_window = notice_get_focus_win(notice);
	}

	DRAWABLE_INFO_MACRO(p.client_window, p.info);

	input_imnull(&im);
	/*
	 * Set im to be used in xv_input_readevent
	 */
	win_setinputcodebit(&im, MS_LEFT);
	win_setinputcodebit(&im, MS_MIDDLE);
	win_setinputcodebit(&im, MS_RIGHT);
	win_setinputcodebit(&im, LOC_WINENTER);
	win_setinputcodebit(&im, LOC_WINEXIT);
	win_setinputcodebit(&im, LOC_DRAG);
	win_setinputcodebit(&im, LOC_MOVE);
	win_setinputcodebit(&im, WIN_VISIBILITY_NOTIFY);
	win_setinputcodebit(&im, WIN_REPAINT);
	im.im_flags = IM_ASCII | IM_NEGEVENT;

	notice_prepare_for_shadow(notice, &p);
	ginfo = notice->ginfo;

	/*
	 * now fill in the box with the text AND buttons
	 */
	notice_layout(notice, &p.rect, p.buttons_width);

	/*
	 * Mouseless
	 * Draw location cursor
	 */
	if (mouseless) {
		notice_show_focus_win(notice, notice->button_info, focus_window, FALSE);
	}

	/*
	 * If notice.jumpCursor is set, and default button exists (always
	 * true), calculate (x,y) of center of default button, and warp
	 * ptr there. Also save ptr to default button for later use.
	 */
	if (notice_jump_cursor && notice->yes_button_exists) {
		notice_buttons_handle curr;

		for (curr = (notice_buttons_handle) notice->button_info;
				curr != (notice_buttons_handle) NULL;
				curr = (notice_buttons_handle) curr->next)
			if (curr->is_yes) {
				default_button = curr;
				x = p.left_placement + curr->button_rect.r_left +
						(curr->button_rect.r_width / 2);
				y = p.top_placement + curr->button_rect.r_top +
						(curr->button_rect.r_height / 2);

				(void)xv_set(p.root_window, WIN_MOUSE_XY, x, y, NULL);
				break;
			}
	}

	if (!default_button) {
		notice_buttons_handle curr;

		/*
		 * Search for default button on notice
		 */
		for (curr = (notice_buttons_handle) notice->button_info;
				curr != (notice_buttons_handle) NULL;
				curr = (notice_buttons_handle) curr->next) {
			if (curr->is_yes) {
				default_button = curr;
				break;
			}
		}
	}

	/*
	 * Stay in fullscreen until a button is pressed, or trigger used
	 */
	trigger = notice->default_input_code;

	SERVERTRACE((44, "SERVER_JOURNALLING?\n"));
	if (xv_get(xv_server(p.info), SERVER_JOURNALLING)) {
		SERVERTRACE((44, " YES \n"));
		(void)xv_set(xv_server(p.info), SERVER_JOURNAL_SYNC_EVENT, 1, NULL);
	}

	prev_button = notice->button_info;

	for (;;) {
		int type, is_select_action, is_stop_key;
		Time server_time, response_time, repaint_time;
		short button_or_key_event;

		if (xv_input_readevent(notice->fullscreen_window, &ie, TRUE, TRUE, &im)
				== (Xv_object)(-1)) {
			break;
		}

		type = event_xevent_type(&ie);

		/*
		 * For Button and Key events:
		 * We need to make sure that these types of events are
		 * processed only if they occurred sometime *after* the
		 * notice window is mapped.
		 */

		/*
		 * Init flag to FALSE
		 */
		button_or_key_event = FALSE;

		/*
		 * Get event time for Button/Key events
		 * set flag to TRUE
		 */
		if ((type == ButtonPress) || (type == ButtonRelease)) {
			XButtonEvent *eb = (XButtonEvent *) event_xevent(&ie);

			response_time = eb->time;
			button_or_key_event = TRUE;
		}

		if ((type == KeyPress) || (type == KeyRelease)) {
			XKeyEvent *ek = (XKeyEvent *) event_xevent(&ie);

			response_time = ek->time;
			button_or_key_event = TRUE;
		}

		/*
		 * Check if the Button/Key event occurred before the first repaint
		 * event.
		 */
		if (button_or_key_event) {
			/*
			 * If the repaint time is not even set, this event should not be
			 * processed
			 */
			if (!first_repaint_set) {
				continue;
			}

			if (response_time < repaint_time) {
				Xv_Drawable_info *notice_window_info;

				/*
				 * The Button/Key event time is before the repaint time.
				 * 2 possibilities here:
				 *  1. the Button/Key event actually occurred before the
				 *     repaint time
				 *  2. the server time went over 49.7 days and got rolled
				 *     over. So, in this case an event occurring after
				 *     the first repaint might have a time value that is
				 *     smaller than the repaint time.
				 * To check for (2), we get the current server time, and see if 
				 * it is smaller than the repaint time. If yes, the clock had
				 * rolled over, and the event *did* actually occur after the 
				 * repaint. If no, the clock had not rolled over, and the
				 * event did occcur before the repaint event - such events are 
				 * ignored.
				 */
				DRAWABLE_INFO_MACRO(notice->fullscreen_window, notice_window_info);

				server_time = xv_sel_get_last_event_time(
									xv_display(notice_window_info),
									xv_xid(notice_window_info));

				if (server_time > repaint_time) {
					continue;
				}
				else {
					repaint_time = server_time;
				}
			}
		}

		x = event_x(&ie);
		y = event_y(&ie);

		/* Translate unmodified ISO (ASCII) Mouseless Keyboard Commands
		 * used inside a notice.
		 */
		if (mouseless) {
			xv_translate_iso(&ie);
		}

		this_event = event_action(&ie);	/* get encoded event */
		this_id = event_id(&ie);	/* get unencoded event */

		if (this_event == ACTION_HELP) {
			continue;
		}

		is_select_action = ((this_event == (int)ACTION_SELECT) ||
				(this_id == (int)MS_LEFT))
				? 1 : 0;
		is_stop_key = ((this_event == (int)ACTION_STOP) ||
				(this_id == (int)WIN_STOP))
				? 1 : 0;

		/*
		 * Get notice button for this event, given (x,y) position
		 * on notice window
		 */
		button = notice_button_for_event(notice, x, y);

		if (event_action(&ie) == ACTION_NEXT_ELEMENT) {
			if (event_is_down(&ie)) {
				if (prev_button) {
					button = prev_button->next;
					if (!button) {
						button = notice->button_info;
					}
				}
				else {
					prev_button = notice->button_info;

					if (prev_button->next) {
						button = prev_button->next;
					}
				}
				notice_show_focus_win(notice, prev_button, focus_window, TRUE);
				notice_do_buttons(notice, &p.rect, -1, prev_button,
						p.buttons_width);
				prev_button = button;
				notice_show_focus_win(notice, button, focus_window, FALSE);
				continue;
			}
		}

		if (event_action(&ie) == ACTION_PREVIOUS_ELEMENT) {
			if (event_is_down(&ie)) {
				button = notice_get_prev_button(notice, prev_button);
				notice_show_focus_win(notice, prev_button, focus_window, TRUE);
				notice_do_buttons(notice, &p.rect, -1, prev_button,
						p.buttons_width);
				prev_button = button;
				notice_show_focus_win(notice, button, focus_window, FALSE);
				continue;
			}
		}

		/*
		 * Must use the button selected using mouseless interface
		 * if mouseless on, and event is not mouse-related
		 */
		if (mouseless && !event_is_button(&ie) && (this_event != LOC_DRAG)) {
			button = prev_button;
		}


		/*
		 * Check if notice is obscured
		 */
		if (this_event == WIN_VISIBILITY_NOTIFY) {
			XVisibilityEvent *xVisEv;

			xVisEv = (XVisibilityEvent *) event_xevent(&ie);

			if ((xVisEv->state == VisibilityPartiallyObscured) ||
					(xVisEv->state == VisibilityFullyObscured)) {
				Xv_Drawable_info *notice_window_info;

				DRAWABLE_INFO_MACRO(notice->fullscreen_window, notice_window_info);
				/*
				 * If notice is obscured, raise it
				 */
				XRaiseWindow(xv_display(notice_window_info),
						xv_xid(notice_window_info));
			}

			continue;
		}

		/*
		 * Check if notice needs to be repainted
		 */
		if (this_event == WIN_REPAINT) {
			Xv_Drawable_info *notice_window_info;

			DRAWABLE_INFO_MACRO(notice->fullscreen_window, notice_window_info);

			if (!first_repaint_set) {
				repaint_time = xv_sel_get_last_event_time(
									xv_display(notice_window_info),
									xv_xid(notice_window_info));
				first_repaint_set = TRUE;
			}

			/*
			 * draw empty box and shadow
			 */
			notice_drawbox(notice->fullscreen_window, &p.rect,
					p.quadrant, p.leftoff, p.topoff);
			/*
			 * now fill in the box with the text AND buttons
			 */
			notice_layout(notice, &p.rect, p.buttons_width);

			/*
			 * Mouseless
			 * Draw location cursor
			 */
			if (mouseless) {
				notice_show_focus_win(notice, button, focus_window, FALSE);
			}

			continue;
		}


		if (((this_event == trigger) || (this_id == trigger))
				&& (((trigger == (int)ACTION_SELECT) ||
								(trigger == (int)MS_LEFT)) ?
						(event_is_up(&ie) && (current_button == NULL))
						: 0)) {
			/*
			 * catch UP mouse left if missed down below for trigger
			 */
			notice->result = NOTICE_TRIGGERED;
			notice_copy_event(notice, &ie);
			goto Done;
		}
		else if (((this_event == trigger) || (this_id == trigger))
				&& (((trigger == (int)ACTION_SELECT) ||
								(trigger == (int)MS_LEFT)) ?
						(event_is_down(&ie) && (button == NULL))
						: 0)) {
			/*
			 * catch down mouse left for trigger, check above against
			 * button rather than current_button since current_button
			 * is NULL on SELECT down, but button may be a real button
			 */
			notice->result = NOTICE_TRIGGERED;
			notice_copy_event(notice, &ie);
			goto Done;
		}
		else if (is_stop_key && notice->no_button_exists) {
			notice->result = NOTICE_NO;
			notice_copy_event(notice, &ie);
			goto Done;
		}
		else if ((this_event == ACTION_DO_IT
						|| this_event == NOTICE_ACTION_DO_IT)
				&& notice->yes_button_exists) {
			if (!event_is_down(&ie)) {
				continue;
			}

			notice->result = default_button->value;
			notice_copy_event(notice, &ie);
			goto Done;
			/*
			 * NOTE: handle button event processing starting here
			 */
		}
		else if (is_select_action && notice->button_info) {
			if (event_is_down(&ie)) {
				if (current_button &&
						(current_button != button) && is_highlighted) {
					notice_paint_button(notice->fullscreen_window,
							current_button, NOTICE_NORMAL_BUTTON, ginfo,
							notice->three_d);
					current_button = NULL;
					is_highlighted = FALSE;
					ok_to_toggle_buttons = FALSE;
				}
				if (button && !is_highlighted && current_button != button) {

					/* Mouseless */
					if (mouseless) {
						/*
						 * Erase focus window over previous button
						 * Redraw previous button
						 */
						if (prev_button) {
							notice_show_focus_win(notice, prev_button,
									focus_window, TRUE);
							notice_do_buttons(notice, &p.rect, -1, prev_button,
									p.buttons_width);
						}

						/*
						 * Draw focus window over current button
						 */
						notice_show_focus_win(notice, current_button,
								focus_window, TRUE);
					}

					current_button = button;
					notice_paint_button(notice->fullscreen_window,
							current_button, NOTICE_INVERT_BUTTON, ginfo,
							notice->three_d);
					prev_button = current_button = button;

					/* Mouseless */
					if (mouseless) {
						notice_show_focus_win(notice, button, focus_window,
								FALSE);
					}

					is_highlighted = TRUE;
					ok_to_toggle_buttons = TRUE;
				}
			}
			else {	/* event_is_up */
				if (button) {
					if (current_button &&
							(current_button != button) && is_highlighted) {

						/* Mouseless */
						if (mouseless) {
							notice_show_focus_win(notice, current_button,
									focus_window, TRUE);
						}

						notice_paint_button(notice->fullscreen_window,
								current_button, NOTICE_NORMAL_BUTTON, ginfo,
								notice->three_d);
						current_button = NULL;
						is_highlighted = FALSE;
						ok_to_toggle_buttons = FALSE;
					}
					notice->result = button->value;
					notice_copy_event(notice, &ie);
					goto Done;
				}
				else {
					ok_to_toggle_buttons = FALSE;
				}
			}
		}
		else if (this_event == LOC_DRAG) {
			if (current_button && (current_button != button)) {
				notice_paint_button(notice->fullscreen_window,
						current_button, NOTICE_NORMAL_BUTTON, ginfo, notice->three_d);

				/* Mouseless */
				if (mouseless) {
					notice_show_focus_win(notice, current_button, focus_window,
							FALSE);
				}

				is_highlighted = FALSE;
				current_button = NULL;
				continue;
			}
			if (button) {
				if (current_button == button) {
					continue;	/* already there */
				}
				else if ((current_button == NULL) && ok_to_toggle_buttons) {
					/* Mouseless */
					if (mouseless && prev_button) {
						notice_show_focus_win(notice, prev_button, focus_window,
								TRUE);
						notice_do_buttons(notice, &p.rect, -1, prev_button,
								p.buttons_width);
					}

					notice_paint_button(notice->fullscreen_window,
							button, NOTICE_INVERT_BUTTON, ginfo, notice->three_d);
					prev_button = current_button = button;

					/* Mouseless */
					if (mouseless) {
						notice_show_focus_win(notice, button, focus_window,
								FALSE);
					}

					is_highlighted = TRUE;
					continue;
				}
			}
			else if (!button && current_button) {
				/* Mouseless */
				if (mouseless) {
					notice_show_focus_win(notice, current_button, focus_window,
							TRUE);
				}

				notice_paint_button(notice->fullscreen_window,
						current_button, NOTICE_NORMAL_BUTTON, ginfo, notice->three_d);
				current_button = NULL;
				is_highlighted = FALSE;
				continue;
			}
		}
		else if (((this_event == trigger) || (this_id == trigger))
				&& (!is_select_action)) {
			/*
			 * catch trigger as a last case, trigger can't be select button
			 * here as that case is dealt with above
			 */
			notice->result = NOTICE_TRIGGERED;
			notice_copy_event(notice, &ie);
			goto Done;
		}
	}

  Done:
	if (xv_get(xv_server(p.info), SERVER_JOURNALLING))
		(void)xv_set(xv_server(p.info), SERVER_JOURNAL_SYNC_EVENT, 1, NULL);

	if (p.fs) {
		xv_destroy(p.fs);
	}

	result = notice->result;

	/*
	 * Copy the result to notice->result_ptr if NOTICE_STATUS was specified 
	 * i.e. an additional place to put the result
	 */
	if (notice->result_ptr) {
		*(notice->result_ptr) = notice->result;
	}

	if (p.client_window && (notice->event != (Event *) 0)) {
		int new_x, new_y;

		win_translate_xy(notice->fullscreen_window, p.client_window,
				event_x(notice->event), event_y(notice->event), &new_x, &new_y);
		event_set_x(notice->event, new_x);
		event_set_y(notice->event, new_y);
		event_set_window(notice->event, p.client_window);
	}
	/* warp mouse back */
	if (notice_jump_cursor && notice->yes_button_exists) {
		if (notice->focus_specified) {
			(void)xv_set(p.root_window, WIN_MOUSE_XY,
					p.old_mousex, p.old_mousey, NULL);
		}
		else {
			(void)xv_set(p.root_window, WIN_MOUSE_XY, p.old_mousex, p.old_mousey,
					NULL);
		}
	}

	xv_set(notice->fullscreen_window, XV_SHOW, FALSE, NULL);

	/*
	 * BUG: Should wait for notice window to unmap before returning
	 */

	return (result);
}

/*
 * Determines which font to use for notice window.
 * OPEN LOOK specifies it to be one scale larger than
 * the client window font.
 *
 * The NOTICE pkg however tries to use the font of the client
 * window. If that is not available, a default font is used.
 * Rescaling of fonts is not done because we dont want to depend
 * on the ability of the server to rescale fonts, or on the
 * presence of fonts in the sizes/scales we want.
 */
#ifdef  OW_I18N

static int notice_determine_font(Xv_Window client_window, notice_handle notice)
{
    Xv_Font     font = NULL;

    if (client_window)
        font = xv_get(client_window, XV_FONT);

    if (font == NULL)
        font = (Xv_Font) xv_find(NULL, FONT,
                                FONT_FAMILY, FONT_FAMILY_DEFAULT,
                                FONT_STYLE, FONT_STYLE_DEFAULT,
                                FONT_SCALE, FONT_SCALE_DEFAULT,
                                NULL);

    if (font == NULL) {
        xv_error(NULL,
            ERROR_STRING,
                XV_MSG("Unable to find \"fixed\" font. (Notice package)"),
        NULL);
        return(XV_ERROR);
    }

    notice->notice_font = font;
    return(XV_OK);
}

#else /*OW_I18N*/

static int notice_determine_font(Xv_Window client_window, notice_handle notice)
{
	Xv_Font client_font = (Xv_Font) NULL;
	Xv_Font default_font = (Xv_Font) NULL;

	/*
	 * Get client window font
	 */
	if (client_window) {
		int rescale;

		client_font = xv_get(client_window, XV_FONT);
		/*
		 * Should try to rescale here
		 */
		/* this default 'TRUE' is demanded by Open Look */
		rescale = defaults_get_boolean("openWindows.rescaleNotices",
										"OpenWindows.RescaleNotices", TRUE);
		if (client_font && rescale) {
    		Window_rescale_state scale =
						(Window_rescale_state)xv_get(client_font, FONT_SCALE);

			if (scale < WIN_SCALE_EXTRALARGE) {
				client_font = xv_find(client_window, FONT,
								FONT_RESCALE_OF, client_font, (int)scale + 1,
								NULL);
				notice->scale = (int)scale + 1;
			}
		}
	}

	if (!client_font) {
		/*
		 * If cannot find client window font, try to find
		 * default font
		 */
		default_font = (Xv_Font) xv_find(client_window, FONT,
							FONT_FAMILY, FONT_FAMILY_DEFAULT,
							FONT_STYLE, FONT_STYLE_DEFAULT,
							FONT_SCALE, FONT_SCALE_DEFAULT,
							NULL);

		if (!default_font) {
			/*
			 * If cannot find default font, find fixed font
			 */
			default_font = (Xv_Font) xv_find(client_window, FONT,
					FONT_NAME, "fixed", NULL);

			/*
			 * If all the above fails, return error code
			 */
			if (!default_font) {
				xv_error(XV_NULL,
						ERROR_STRING,
						XV_MSG("Unable to find \"fixed\" font."),
						ERROR_PKG, NOTICE, NULL);
				return XV_ERROR;
			}
		}

	}

	notice->notice_font = client_font ? client_font : default_font;

	return XV_OK;
}

#endif  /* OW_I18N */

/*
 * Function to return value of the default button on notice
 */
static int notice_get_default_value(Notice_info	*notice)
{
    int		numButtons = notice->number_of_buttons;
    struct notice_buttons	*curButton = notice->button_info;
    int		i;

    /*
     * Search thru all buttons
     */
    for (i=0; i < numButtons; ++i, curButton = curButton->next)  {
	/*
	 * Return default/'yes' button value 
	 */
	if (curButton->is_yes)  {
	    return(curButton->value);
	}
    }

    /*
     * If none found, return value of the first button
     */
    return(notice->button_info->value);

}

/*
 * Event proc for notice sub frame
 * Note:
 * Since the Window pkg does not support variable width
 * window borders, we fake the extra thickness by drawing a rectangle 
 * inside the window.
 * Also, if detect ACTION_DISMISS, simulate pressing of default button.
 */
static void subframe_event_proc(Xv_window window, Event *event)
{
	XEvent *xEvent;
	Notice_info *notice;
	int width, height;
	Xv_Notice notice_public;
	int notice_value;

	xEvent = event_xevent(event);
	notice = (Notice_info *) xv_get(window,
			XV_KEY_DATA, notice_context_key, NULL);

	/*
	 * Check if notice exists as key data on window
	 */
	if (notice) {
		/*
		 * Check for X events
		 */
		switch (xEvent->type) {
			case Expose:
				/*
				 * Expose event detected - draw fake border
				 */
				width = xv_get(window, XV_WIDTH);
				height = xv_get(window, XV_HEIGHT);

				notice_draw_borders(window, 0, 0, width, height,
						NOTICE_NOT_TOPLEVEL);

				break;
		}

		notice_public = NOTICE_PUBLIC(notice);

		/*
		 * Check for XView Semantice events
		 */
		switch (event_action(event)) {
			case ACTION_DISMISS:
				/*
				 ********************************************
				 * When see ACTION_DISMISS, 
				 * Simulate the default button being pressed.
				 ********************************************
				 */
				notice_value = notice_get_default_value(notice);

				/*
				 * Store the value in the result field of notice for
				 * later retrieval
				 * notice->result_ptr is the address specified by user via
				 * NOTICE_STATUS
				 */
				notice->result = notice_value;
				if (notice->result_ptr) {
					*(notice->result_ptr) = notice_value;
				}

				/*
				 * Call notice event proc if any
				 */
				if (notice->event_proc) {
					(notice->event_proc) (notice_public, notice_value, event);
				}

				/*
				 * Pop down notice
				 */
				if (notice->block_thread) {
					xv_window_return((Xv_opaque) XV_OK);
				}
				else {
					xv_set(notice_public, XV_SHOW, FALSE, NULL);
				}
				break;
		}

		/*
		 * If detect default action, post it to panel
		 */
		if (notice->panel) {
			if (event_action(event) == ACTION_DEFAULT_ACTION
				|| xv_translate_iso(event))
			{
				/* orig only xv_iso_default_action */
				notify_post_event(notice->panel, (Notify_event) event, NOTIFY_IMMEDIATE);
			}
		}
	}
}

/*
 * Destroy interpose proc for notice subframe
 * This is needed if the notice subframe is killed
 * via QUIT by the user.
 * olwm will not put any wmgr menus on the notice subframe
 * if _OL_WT_NOTICE is set on the notice subframe, but
 * other window wmgrs might.
 */
static Notify_error subframe_destroy_proc(Notify_client	sub_frame, Destroy_status status)
{
    Notice_info		*notice; 
    Xv_Notice		notice_public;
    Event		event;
    int			notice_value;

    /*
     * Get notice private data hanging off subframe
     */
    notice = (Notice_info *)xv_get(sub_frame, 
			XV_KEY_DATA, notice_context_key, 
		        NULL);

    if (!notice)  {
	/*
	 * Call next destroy proc if notice object missing
	 */
	return notify_next_destroy_func(sub_frame, status);
    }

    if (!notice->show)  {
	/*
	 * Call next destroy proc if notice is not currenty visible
	 */
	return notify_next_destroy_func(sub_frame, status);
    }

    notice_public = NOTICE_PUBLIC(notice);

    switch (status)  {
    case DESTROY_PROCESS_DEATH:
    break;

    case DESTROY_CHECKING:
    break;

    case DESTROY_CLEANUP:
	/*
	 ********************************************
	 * Simulate the default button being pressed.
	 ********************************************
	 */


	notice_value = notice_get_default_value(notice);

        /*
         * Store the value in the result field of notice for
         * later retrieval
         * notice->result_ptr is the address specified by user via
         * NOTICE_STATUS
         */
	notice->result = notice_value;
	if (notice->result_ptr)  {
    	    *(notice->result_ptr) = notice_value;
	}

	/*
	 * Call notice event proc if any
	 */
	if (notice->event_proc)  {
	    event_init((&event));
	    (notice->event_proc)(notice_public, notice_value, &event);
	}

	/*
	 * Pop down notice
	 */
	if (notice->block_thread)  {
	    xv_window_return((Xv_opaque)XV_OK);
	}
	else  {
	    xv_set(notice_public, XV_SHOW, FALSE, NULL);
	}

	/*
	 * Set sub_frame to NULL so that it will be re-created
	 * next time this notice is mapped
	 */
	notice->sub_frame = (Frame)NULL;

	/*
	 * Call next destroy proc
	 */
	return notify_next_destroy_func(sub_frame, status);
    break;

    case DESTROY_SAVE_YOURSELF:
    break;
    }

    return(NOTIFY_DONE);
}

/*
 * notice_create_base(notice)
 * Create base_frame and panel for notice 
 */
static int notice_create_base(Notice_info	*notice)
{
	Xv_Drawable_info *info;
	Xv_Drawable_info *client_info;

	/*
	 * If no font specified, try to get client_window font
	 */
	if (!notice->notice_font) {
		int e;

		if ((e = notice_determine_font(notice->client_window, notice)) != XV_OK)
		{
			/*
			 * If error occurred during font determination, 
			 * return error code
			 */
			return (e);
		}
	}

	if (!notice->sub_frame) {
		Xv_server server;
		WM_Win_Type win_attr;

		/*
		 * Create sub frame for notice if havent yet
		 */
		notice->sub_frame = xv_create(notice->owner_window, FRAME,
				XV_LABEL, "Notice",
				XV_FONT, notice->notice_font,
				WIN_BORDER, FALSE,
				WIN_CONSUME_X_EVENT_MASK,
					ExposureMask | KeyPressMask | FocusChangeMask,
				WIN_EVENT_PROC, subframe_event_proc,
				WIN_FRONT,
#ifdef OW_I18N
				WIN_USE_IM, FALSE,
#endif
				XV_KEY_DATA, notice_context_key, notice,
				XV_HELP_DATA, "xview:notice",
				NULL);

		/*
		 * Do not grab SELECT button on notice frame
		 */
		xv_set(notice->sub_frame, WIN_UNGRAB_SELECT, NULL);

		/*
		 * Tell frame to accept focus if the panel does not take it.
		 * When not in Full mouseless mode, this is necessary
		 */
		frame_set_accept_default_focus(notice->sub_frame, TRUE);

		/*
		 * Set subframe's destroy proc
		 */
		notify_interpose_destroy_func(notice->sub_frame, subframe_destroy_proc);

		DRAWABLE_INFO_MACRO(notice->sub_frame, info);
		DRAWABLE_INFO_MACRO(notice->owner_window, client_info);
		XSetTransientForHint(xv_display(info), xv_xid(info),
				xv_xid(client_info));

		/*
		 * Get server object
		 */
		server = XV_SERVER_FROM_WINDOW(notice->sub_frame);

		/*
		 * Set window attributes
		 *  window type = notice
		 *  pin initial state = none
		 *  menu type = none
		 */
		memset(&win_attr, 0, sizeof(win_attr));
		win_attr.flags = WMWinType;
		win_attr.win_type = (Atom) xv_get(server, SERVER_ATOM, "_OL_WT_NOTICE");
		(void)wmgr_set_win_attr(notice->sub_frame, &win_attr);

		notice->three_d = (SCREEN_UIS_3D_COLOR ==
				(screen_ui_style_t)xv_get(xv_screen(info), SCREEN_UI_STYLE));
		notice->ginfo = xv_init_olgx(notice->sub_frame, &notice->three_d,
				xv_get(notice->sub_frame, XV_FONT));
	}

	/*
	 * Create panel which will contain message and buttons
	 */
	if (!notice->panel) {
		notice->panel = xv_create(notice->sub_frame, PANEL,
				XV_FONT, notice->notice_font,

#ifdef OW_I18N
				WIN_USE_IM, FALSE,
#endif

				XV_HELP_DATA, "xview:notice",
				NULL);
	}

	/*
	 * Get default CMS from panel and make frame use it
	 * This is to make frame background be same color as 
	 * panel.
	 */
	xv_set(notice->sub_frame, WIN_CMS, xv_get(notice->panel, WIN_CMS), NULL);

	return (XV_OK);
}

/*
 * Center notice within parent, if any
 */
static int notice_center(Notice_info *notice)
{
	Display *dpy;
	Xv_Screen screen;
	int screen_num;
	Frame parent = notice->owner_window;
	Frame sub_frame = notice->sub_frame;
	Panel panel = notice->panel;
	int xDiff, yDiff, pWidth, pHeight, cWidth, cHeight, cX, cY, pX, pY;

	if (!parent) return XV_ERROR;
	if (!sub_frame) return XV_ERROR;

	dpy = (Display *) xv_get(sub_frame, XV_DISPLAY);
	screen = (Xv_Screen) xv_get(sub_frame, XV_SCREEN);
	screen_num = xv_get(screen, SCREEN_NUMBER);

	/*
	 * Get subframe width, height
	 */
	cWidth = xv_get(panel, XV_WIDTH) +
			PANE_NOTICE_DIFF(NOTICE_NOT_TOPLEVEL, notice->scale);
	cHeight = xv_get(panel, XV_HEIGHT) +
			PANE_NOTICE_DIFF(NOTICE_NOT_TOPLEVEL, notice->scale);

	if (xv_get(parent, FRAME_CLOSED)) {
		Xv_window root_window;
		Rect *mouse_position;

		/*
		 * Calculations to center the sub_frame where the pointer is
		 * currently located
		 */

		/*
		 * Get root window, and pointer position relative to root
		 * window
		 */
		root_window = (Xv_window) xv_get(sub_frame, XV_ROOT);
		mouse_position = (Rect *) xv_get(root_window, WIN_MOUSE_XY);

		cX = mouse_position->r_left - (cWidth / 2);
		cY = mouse_position->r_top - (cHeight / 2);
	}
	else {
		Xv_Drawable_info *p_info;
		XID dummy;

		/*
		 * Calculations to center the sub_frame within the parent
		 */

		pWidth = xv_get(parent, XV_WIDTH);
		pHeight = xv_get(parent, XV_HEIGHT);
		DRAWABLE_INFO_MACRO(parent, p_info);
		XTranslateCoordinates(dpy, xv_xid(p_info),
				(XID) xv_get(xv_root(p_info), XV_XID), 0, 0, &pX, &pY, &dummy);

		xDiff = (pWidth - cWidth) / 2;
		yDiff = (pHeight - cHeight) / 2;

		/*
		 * Centered positions of sub frame
		 */
		cX = pX + xDiff;
		cY = pY + yDiff;
	}

	/*
	 * Check if sub frame is off screen
	 */

	/*
	 * Check x coordinates first
	 * The checks for wider than DisplayWidth and less than zero
	 * are not exclusive because by correcting for wider than
	 * DisplayWidth, the x position can be set to < 0
	 */
	if ((cX + cWidth) > DisplayWidth(dpy, screen_num)) {
		cX = DisplayWidth(dpy, screen_num) - cWidth;
	}

	if (cX < 0) {
		cX = 0;
	}

	/*
	 * Check y coordinate
	 */
	if ((cY + cHeight) > DisplayHeight(dpy, screen_num)) {
		cY = DisplayHeight(dpy, screen_num) - cHeight;
	}

	if (cY < 0) {
		cY = 0;
	}

	xv_set(sub_frame,
			XV_X, cX,
			XV_Y, cY,
			XV_WIDTH, cWidth,
			XV_HEIGHT, cHeight,
			NULL);

	/*
	 * Location of the panel itself is:
	 * notice border width + pane-border distance + pane border width
	 */
	xv_set(panel,
			XV_X, PANE_XY(NOTICE_NOT_TOPLEVEL, notice->scale),
			XV_Y, PANE_XY(NOTICE_NOT_TOPLEVEL, notice->scale),
			NULL);

	return XV_OK;
}

static void notice_button_panel_proc(Panel_item item, Event *event)
{
	Notice_info *notice_info =
				(Notice_info *) xv_get(item, XV_KEY_DATA, notice_context_key);
	Xv_Notice notice = NOTICE_PUBLIC(notice_info);
	struct notice_buttons *buttons = notice_info->button_info;
	struct notice_buttons *cur;


	/*
	 * Determine value of button 
	 */
	cur = buttons;
	while (cur) {
		if (cur->panel_item == item) {
			break;
		}
		else {
			cur = cur->next;
		}
	}

	/*
	 * Store the value in the result field of notice for
	 * later retrieval
	 * notice->result_ptr is the address specified by user via
	 * NOTICE_STATUS
	 */
	if (cur) {
		notice_info->result = cur->value;
		if (notice_info->result_ptr) {
			*(notice_info->result_ptr) = cur->value;
		}
	}

	/*
	 * Call notice event proc if one was specified
	 */
	if (notice_info->event_proc) {
		if (cur) {
			/*
			 * Call notice event proc
			 */
			(notice_info->event_proc) (notice, cur->value, event);
		}
	}

	/*
	 * Pop down notice 
	 */
	if (notice_info->block_thread) {
		xv_window_return((Xv_opaque)XV_OK);
	}
	else {
		xv_set(notice, XV_SHOW, FALSE, NULL);
	}
}

/*
 * Routine to create and arrange panel items
 */
static void notice_position_items(Notice_info *notice, Bool do_msg, Bool do_butt)
{
	Panel panel = notice->panel;
	Rect **msg_rect_list;
	Rect **button_rect_list;
	int i;
	int panelWidth = 0, panelHeight = 0;
	int maxButHeight = 0;
	int totalButWidth = 0;
	int panelItemX, panelItemY;
	int numMsgStr = notice->number_of_strs;
	int numButtons = notice->number_of_buttons;
	struct notice_buttons *curButton = notice->button_info;
	struct notice_msgs *curMsg = notice->msg_info;

	msg_rect_list = (Rect **) malloc(numMsgStr * (sizeof(Rect *)));
	button_rect_list = (Rect **) malloc(numButtons * (sizeof(Rect *)));

/* fprintf(stderr, "%s-%d: %s\n", __FILE__,__LINE__,__FUNCTION__); */
	/*
	 * Set the message string Panel Items
	 */
	/*
	 * Check if any first
	 */
	if (!curMsg) {
		if (numMsgStr) {
			numMsgStr = 0;
		}
	}

	for (i = 0; i < numMsgStr; curMsg = curMsg->next, ++i) {
		if (do_msg) {
			if (curMsg->panel_item) {
				xv_set(curMsg->panel_item,
#ifdef OW_I18N
						PANEL_LABEL_STRING_WCS, curMsg->string,
#else
						PANEL_LABEL_STRING, curMsg->string,
#endif /* OW_I18N */
						NULL);
			}
			else {
				curMsg->panel_item = xv_create(notice->panel, PANEL_MESSAGE,
#ifdef OW_I18N
						PANEL_LABEL_STRING_WCS, curMsg->string,
#else
						PANEL_LABEL_STRING, curMsg->string,
#endif /* OW_I18N */
						XV_HELP_DATA, "xview:notice",
						NULL);
			}
		}

		/*
		 * Get the created panel item's rect info
		 */
		msg_rect_list[i] = (Rect *) xv_get(curMsg->panel_item, PANEL_ITEM_RECT);

		/*
		 * Update panel width/height
		 */
		panelWidth = MAX(panelWidth, msg_rect_list[i]->r_width);
		panelHeight += msg_rect_list[i]->r_height;

		/*
		 * vertical distance is vertical gap
		 * - add only if not last message
		 */
		if (i < (numMsgStr - 1)) {
			panelHeight += MSG_VERT_GAP(notice->scale);
		}
	}

	/*
	 * Add margins to panel width
	 */
	panelWidth += 2 * HORIZ_MSG_MARGIN(notice->scale);

	/*
	 * Set button Panel Items
	 */
	for (i = 0; i < numButtons; ++i, curButton = curButton->next) {
		/*
		 * Create buttons
		 */
		if (do_butt) {
			if (curButton->panel_item) {
				xv_set(curButton->panel_item,
#ifdef OW_I18N
						PANEL_LABEL_STRING_WCS, curButton->string,
#else
						PANEL_LABEL_STRING, curButton->string,
#endif /* OW_I18N */
						PANEL_NOTIFY_PROC, notice_button_panel_proc,
						XV_KEY_DATA, notice_context_key, notice,
						NULL);
			}
			else {
				curButton->panel_item = xv_create(notice->panel, PANEL_BUTTON,
#ifdef OW_I18N
						PANEL_LABEL_STRING_WCS, curButton->string,
#else
						PANEL_LABEL_STRING, curButton->string,
#endif /* OW_I18N */
						PANEL_NOTIFY_PROC, notice_button_panel_proc,
						XV_KEY_DATA, notice_context_key, notice,
						XV_HELP_DATA, "xview:notice",
						NULL);
			}

			if (curButton->is_yes) {
				xv_set(notice->panel,
						PANEL_DEFAULT_ITEM, curButton->panel_item,
						NULL);
			}
		}

		/*
		 * Get rect info
		 */
		button_rect_list[i] = (Rect *) xv_get(curButton->panel_item,
				PANEL_ITEM_RECT);

		/*
		 * Update total button width and max button height
		 */
		totalButWidth += button_rect_list[i]->r_width;
		maxButHeight = MAX(maxButHeight, button_rect_list[i]->r_height);
	}

	/*
	 * Add button horizontal gap(s) to total width
	 */
	totalButWidth += ((numButtons - 1) * BUT_HORIZ_GAP(notice->scale));

	/*
	 * The button portion height is max of the OPENLOOK value
	 * and the actual button height
	 */
	BUT_PORTION_HEIGHT(notice->scale) = MAX(BUT_PORTION_HEIGHT(notice->scale),
			maxButHeight);

	/*
	 * Add to panel height the top/bottom margins and the height of
	 * buttons.
	 */
	panelHeight += (2 * VERT_MSG_MARGIN(notice->scale)) +
			BUT_PORTION_HEIGHT(notice->scale);

	/*
	 * Panel width is max of current panel width and the width of the buttons
	 */
	if (panelWidth < (totalButWidth + (2 * HORIZ_MSG_MARGIN(notice->scale)))) {
		panelWidth = totalButWidth + (2 * HORIZ_MSG_MARGIN(notice->scale));
	}

	/*
	 * Set panel width/height
	 */
	xv_set(panel, XV_WIDTH, panelWidth, XV_HEIGHT, panelHeight, NULL);

	/*
	 * Reset button/msg pointers
	 */
	curButton = notice->button_info;
	curMsg = notice->msg_info;

	/*
	 * Position messages
	 * Start at top and work downwards
	 */
	panelItemY = VERT_MSG_MARGIN(notice->scale);
	for (i = 0; i < numMsgStr; ++i, curMsg = curMsg->next) {
		/*
		 * Center messages
		 */
		panelItemX = (panelWidth - msg_rect_list[i]->r_width) / 2;
		/*
		 * Set panel item coordinates
		 */
		xv_set(curMsg->panel_item, XV_X, panelItemX, XV_Y, panelItemY, NULL);

		/*
		 * Add message height to vertical position
		 */
		panelItemY += msg_rect_list[i]->r_height;

		/*
		 * vertical distance is vertical gap
		 * - add only if not last message
		 */
		if (i < (numMsgStr - 1)) {
			panelItemY += MSG_VERT_GAP(notice->scale);
		}

	}

	/*
	 * Position buttons
	 * Center buttons in button area vertically and horizontally
	 */
	panelItemX = (panelWidth - totalButWidth) / 2;
	panelItemY += VERT_MSG_MARGIN(notice->scale) +
			((BUT_PORTION_HEIGHT(notice->scale) - maxButHeight) / 2);

	for (i = 0; i < numButtons; ++i, curButton = curButton->next) {
		/*
		 * Set button coordinates
		 */
		xv_set(curButton->panel_item, XV_X, panelItemX, XV_Y, panelItemY, NULL);

		/*
		 * horizontal distance is button width + button gap
		 */
		panelItemX +=
				button_rect_list[i]->r_width + BUT_HORIZ_GAP(notice->scale);

	}

	/*
	 * Free rect list
	 */
	free((char *)msg_rect_list);
	free((char *)button_rect_list);
}

/*
 * Routine to create sub_frame and panels
 */
static void notice_subframe_layout(Notice_info	*notice, Bool do_msg, Bool do_butt)
{
	/*
	 * Create base frame and panel for notice
	 */
	notice_create_base(notice);

	/*
	 **********************************************************
	 **********************************************************
	 * Position panel items within panel according to OPEN LOOK
	 **********************************************************
	 **********************************************************
	 */
	notice_position_items(notice, do_msg, do_butt);

	/*
	 * Calculations to center the sub_frame within the parent
	 */
	notice_center(notice);

	/*
	 * Set _OL_DFLT_BTN property on window if notice.jumpCursor resource set
	 */
	if (notice_jump_cursor) {
		Panel_item panel_default_item;
		Rect *rect;
		int button_x, button_y;
		unsigned long int data[6];

		panel_default_item =
				(Panel_item) xv_get(notice->panel, PANEL_DEFAULT_ITEM);

		/*
		 * Do only if default item exists
		 */
		if (panel_default_item) {
			rect = (Rect *) xv_get(panel_default_item, PANEL_ITEM_RECT);

			/*
			 * Check for Rect returned to avoid seg fault
			 */
			if (rect) {
				win_translate_xy(notice->panel, notice->sub_frame,
						rect->r_left, rect->r_top, &button_x, &button_y);

				data[0] = button_x + rect->r_width / 2;
				data[1] = button_y + rect->r_height / 2;
				data[2] = button_x;
				data[3] = button_y;
				data[4] = rect->r_width;
				data[5] = rect->r_height;

				/*
				 * Set property on notice frame
				 */
				win_change_property(notice->sub_frame,
						(Attr_attribute)SERVER_WM_DEFAULT_BUTTON, XA_INTEGER,
						32, (unsigned char *)data, 6);
			}
		}
	}
	else {
		/*
		 * notice.jumpCursor NOT set.
		 * Set zero length property on notice frame so that the ptr is not warped.
		 */
		win_change_property(notice->sub_frame,
				(Attr_attribute)SERVER_WM_DEFAULT_BUTTON, XA_INTEGER,
				32, NULL, 0);
	}

	notice->need_layout = 0;
}

#ifdef OW_I18N
extern struct pr_size xv_pf_textwidth_wc();
#else
extern struct pr_size xv_pf_textwidth(int len, Xv_font pf, char  *str);
#endif

#ifdef  OW_I18N
static wchar_t notice_default_button_str[8] = {
                        (wchar_t)'C' ,
                        (wchar_t)'o' ,
                        (wchar_t)'n' ,
                        (wchar_t)'f' ,
                        (wchar_t)'i' ,
                        (wchar_t)'r' ,
                        (wchar_t)'m' ,
                        (wchar_t) NULL };

#else
static char *notice_default_button_str = "Confirm";
#endif


/*
 * Fill in fields of private_data with default values
 */
static void notice_defaults(notice_handle notice)
{
    notice->client_window = notice->owner_window = 
    				notice->fullscreen_window = XV_NULL;
    notice->sub_frame = (Frame)NULL;
    notice->panel = (Panel)NULL;
    notice->busy_frames = (Frame *)NULL;
    notice->event_proc = (void(*)())NULL;

    notice->result_ptr = NULL;

    notice->default_input_code = '\0';	/* ASCII NULL */
    notice->event = (Event *)NULL;

    notice->notice_font = (Xv_Font) NULL;

    notice->beeps = default_beeps;

    notice->number_of_buttons = 0;
    notice->number_of_strs = 0;
    notice->button_info = (notice_buttons_handle) NULL;
    notice->msg_info = (notice_msgs_handle) NULL;
    notice->help_data = "xview:notice";

    notice->lock_screen = 0;
    notice->yes_button_exists = 0;
    notice->no_button_exists = 0;
    notice->focus_specified = 0;
    notice->dont_beep = 0;
    notice->need_layout = 1;
    notice->show = 0;
    notice->new = 1;

    notice->block_thread = 1;

    notice->scale = 1;

}


static int notice_init_internal(Xv_Window client_window, Xv_notice self,
					Attr_avlist avlist, int *u)
{
	Xv_notice_struct *notice_public = (Xv_notice_struct *) self;
	Notice_info *notice;
	Xv_screen screen;

	if (!client_window) {
		xv_error(XV_NULL,
				ERROR_STRING,
					XV_MSG("NULL parent window passed to NOTICE. Not allowed."),
				ERROR_PKG, NOTICE,
				NULL);
		return (XV_ERROR);
	}

	if (!notice_context_key) {
		notice_context_key = xv_unique_key();
	}

	/*
	 * Allocate space for private data
	 */
	notice = (Notice_info *) xv_calloc(1, (unsigned)sizeof(Notice_info));
	if (!notice) {
		xv_error(XV_NULL,
				ERROR_STRING, XV_MSG("Malloc failed."),
				ERROR_PKG, NOTICE,
				NULL);
		return (XV_ERROR);
	}

	/*
	 * Set forward/backward pointers
	 */
	notice_public->private_data = (Xv_opaque) notice;
	notice->public_self = (Xv_opaque) notice_public;

	/*
	 * Make notice pkg look for new resource OpenWindows.PopupJumpCursor
	 * first before Notice.JumpCursor
	 */
	if (defaults_exists("openWindows.popupJumpCursor",
					"OpenWindows.PopupJumpCursor")) {
		notice_jump_cursor =
				(int)defaults_get_boolean("openWindows.popupJumpCursor",
				"OpenWindows.PopupJumpCursor", (Bool) TRUE);
	}
	else {
		notice_jump_cursor = (int)defaults_get_boolean("notice.jumpCursor",
				"Notice.JumpCursor", (Bool) TRUE);
	}
	default_beeps = defaults_get_integer("notice.beepCount",
			"Notice.BeepCount", 1);

	/*
	 * Set fields of notice to default values in preparation
	 * for notice_set_avlist
	 */
	notice_defaults(notice);

	notice->client_window = client_window;
	screen = xv_get(client_window, XV_SCREEN);
	if (xv_get(screen, SCREEN_ENHANCED_OLWM)) {
		/* notice according to OL GUI Spec: with an emanation shadow */
    	notice->lock_screen_looking = 1;
	}
	else {
		/* other window managers will not handle the
		 * emanation shadow correctly
		 */
    	notice->lock_screen_looking = 0;
	}

	notice_get_owner_frame(notice);


	return (XV_OK);
}

/*
 * Create a button struct
 */
static notice_buttons_handle notice_create_button_struct(void)
{
	notice_buttons_handle pi = NULL;

	pi = (notice_buttons_handle) xv_calloc(1, (unsigned)sizeof(struct notice_buttons));
	if (!pi) {
		xv_error(XV_NULL,
				ERROR_STRING,
				XV_MSG("calloc failed in notice_create_button_struct()."),
				ERROR_PKG, NOTICE, NULL);
	}
	else {
		pi->is_yes = (int)FALSE;
	}
	return pi;
}

/*
 * Add button to list of buttons on notice
 */
static void notice_add_button_to_list(register notice_handle notice, notice_buttons_handle button)
{
    notice_buttons_handle curr;

    if (notice->button_info) {
	for (curr = notice->button_info; curr; curr = curr->next)
	    if (curr->next == NULL) {
		curr->next = button;
		break;
	    }
    } else
	notice->button_info = button;
}

/*
 * Add a button struct to the list of buttons in the notice private data
 * and give it default values.
 */
static void notice_add_default_button(register notice_handle notice)
{
    notice_buttons_handle button;

    button = (notice_buttons_handle) notice_create_button_struct();
    button->string = (CHAR *) XV_STRSAVE(XV_MSG(notice_default_button_str));
    button->is_yes = TRUE;
    button->value = NOTICE_YES;
    button->panel_item = (Panel_item)NULL;
    notice->yes_button_exists = TRUE;
    notice_add_button_to_list(notice, button);
    notice->number_of_buttons++;

}

/*
 * Add string to list of messages on notice
 */
static void notice_add_msg_to_list(register notice_handle notice, notice_msgs_handle msg)
{
    notice_msgs_handle curr;

    if (notice->msg_info) {
	for (curr = notice->msg_info; curr; curr = curr->next)
	    if (curr->next == NULL) {
		curr->next = msg;
		break;
	    }
    } else
	notice->msg_info = msg;
}


/*
 * Create a button struct
 */
static notice_msgs_handle notice_create_msg_struct(void)
{
	notice_msgs_handle pi = NULL;

	pi = (notice_msgs_handle) xv_calloc(1, (unsigned)sizeof(struct notice_msgs));
	if (!pi) {
		xv_error(XV_NULL,
				ERROR_STRING,
				XV_MSG("calloc failed in notice_create_msg_struct()."),
				ERROR_PKG, NOTICE, NULL);
	}
	return pi;
}

/*
 * Free a message struct
 */
static void notice_free_msg_structs(notice_msgs_handle first)
{
    notice_msgs_handle current;
    notice_msgs_handle next;

    if (!first)
	return;
    for (current = first; current != NULL; current = next) {
	next = current->next;
	free(current->string);

	if (current->panel_item)  {
	    xv_destroy(current->panel_item);
	}

	free(current);
    }
}

/*
 * Free a button struct
 */
static void notice_free_button_structs(notice_buttons_handle first)
{
	notice_buttons_handle current;
	notice_buttons_handle next;

	if (!first) return;

	for (current = first; current != NULL; current = next) {
		next = current->next;
		free(current->string);

		if (current->panel_item) {
			xv_destroy(current->panel_item);
		}

		free(current);
	}
}

/*
 * Update notice xy position
 */
/* static void notice_update_xy(Notice_info *notice) */
/* { */
/*     Frame	sub_frame = notice->sub_frame; */
/*     Rect	rect; */
/*  */
/*     frame_get_rect(sub_frame, &rect); */
/*     frame_set_rect(sub_frame, &rect); */
/* } */

static void busy_frame_and_subframes(Frame fram, int busy)
{
	Frame fr;
	int i;

	if (xv_get(fram, XV_SHOW)) xv_set(fram, FRAME_BUSY, busy, NULL);

	for (i = 1; (fr = xv_get(fram, FRAME_NTH_SUBFRAME, i)); i++) {
		busy_frame_and_subframes(fr, busy);
	}
}

static void make_busy(Notice_info *notice, Frame *frames, int busy)
{
	if (frames) {
		while (*frames) {
			xv_set(*frames, FRAME_BUSY, busy, NULL);
			frames++;
		}
	}
	/* OL specifies 'all visible windows of the application are busy' */
	else {
		Frame baseframe = notice->owner_window;

		if (baseframe) {
			while (! xv_get(baseframe, XV_IS_SUBTYPE_OF, FRAME_BASE)) {
				baseframe = xv_get(baseframe, XV_OWNER);
				if (! baseframe) {
					/* very sorry */
					return;
				}
			}

			busy_frame_and_subframes(baseframe, busy);
		}
	}
}


#ifdef  OW_I18N
static CHAR     **notice_string_set();
#endif

/*
 * notice_do_show(notice)
 * Pops up the notice. Handles both the screen locking and non 
 * screen locking case
 */
static int notice_do_show(Notice_info	*notice)
{
	Frame *busy_frames;

	/*
	 * Check if this the screen locking type of notice
	 */
	if (notice->lock_screen) {
		/*
		 * Screen-locking notice
		 */
		if (notice->show) {
			notice->show = 1;

			notice->result = notice_block_popup(notice);
			notice->show = 0;
		}
	}
	else if (notice->lock_screen_looking) {
		/*
		 * Non-screen-locking notice
		 */
		busy_frames = notice->busy_frames;

		if (notice->show) {
			Xv_window root = xv_get(notice->sub_frame, XV_ROOT);
			Rect *rect;

			/*
			 * Get current ptr position on root window
			 */
			rect = (Rect *) xv_get(root, WIN_MOUSE_XY);

			/*
			 * Save this for later - when notice is popped down we want to
			 * warp the pointer back to the saved (x,y) position
			 */
			notice->old_mousex = rect->r_left;
			notice->old_mousey = rect->r_top;

			/*
			 * Make subframe busy
			 */
			if (!notice->block_thread) {
				xv_set(notice->client_window, FRAME_BUSY, TRUE, NULL);
			}

			/*
			 * Make frames busy
			 */
			make_busy(notice, busy_frames, TRUE);

			/*
			 * Make notice visible
			 */
			if (notice->block_thread) {
				notice_prep_t p;
				Rect r;

				p.event_state = 0;
				p.client_window = notice->client_window;
				p.fs = XV_NULL;
				DRAWABLE_INFO_MACRO(p.client_window, p.info);

				notice_prepare_for_shadow(notice, &p);

				r.r_left = p.left_placement + p.rect.r_left - 6 - 1;
				r.r_top = p.top_placement + p.rect.r_top - 6 - 1;
				r.r_width = (int)xv_get(notice->sub_frame, XV_WIDTH) + 8;
				r.r_height = p.rect.r_height + 35;

				xv_set(notice->sub_frame,
						XV_X, r.r_left,
						XV_Y, r.r_top,
						XV_HEIGHT, r.r_height,
						NULL);
				frame_set_rect(notice->sub_frame, &r);

				window_set_tree_flag(notice->fullscreen_window, XV_NULL,
											FALSE, TRUE);
				/*
				 * For thread blocking case, use xv_window_loop()
				 */
				xv_window_loop(notice->sub_frame);

				window_set_tree_flag(notice->fullscreen_window, XV_NULL,
											FALSE, FALSE);
    			if (notice->fullscreen_window) {
					xv_set(notice->fullscreen_window, XV_SHOW, FALSE, NULL);
				}
				/*
				 * Make busy frames NOT busy again
				 */
				busy_frames = notice->busy_frames;
				make_busy(notice, busy_frames, FALSE);

				notice->show = 0;

			}
			else {
				/*
				 * Non thread blocking case
				 */
				xv_set(notice->sub_frame, XV_SHOW, TRUE, NULL);
			}
		}
		else {
			/*
			 * Make client frame not busy
			 */
			if (!notice->block_thread) {
				xv_set(notice->client_window, FRAME_BUSY, FALSE, NULL);
			}

			make_busy(notice, busy_frames, FALSE);

			/*
			 * Pop down notice sub frame
			 *
			 * Do only if non blocking.
			 * For thread blocking case, this was done above already.
			 */
			if (!notice->block_thread) {
				xv_set(notice->sub_frame, XV_SHOW, FALSE, NULL);
			}
		}
	}
	else {
		/*
		 * Non-screen-locking and non-screen-locking-looking notice
		 */
		busy_frames = notice->busy_frames;

		if (notice->show) {
			Xv_window root =
					(Xv_window) xv_get(notice->sub_frame, XV_ROOT, NULL);
			Rect *rect;

			/*
			 * Get current ptr position on root window
			 */
			rect = (Rect *) xv_get(root, WIN_MOUSE_XY);

			/*
			 * Save this for later - when notice is popped down we want to
			 * warp the pointer back to the saved (x,y) position
			 */
			notice->old_mousex = rect->r_left;
			notice->old_mousey = rect->r_top;

			/*
			 * Make subframe busy
			 */
			if (!notice->block_thread) {
				xv_set(notice->client_window, FRAME_BUSY, TRUE, NULL);
			}

			/*
			 * Make frames busy
			 */
			make_busy(notice, busy_frames, TRUE);

			notice_center(notice);

			/*
			 * Ring bell
			 */
			(void)notice_do_bell(notice);

			/*
			 * Make notice visible
			 */
			if (notice->block_thread) {
				/*
				 * For thread blocking case, use xv_window_loop()
				 */
				xv_window_loop(notice->sub_frame);

				/*
				 * Make busy frames NOT busy again
				 */
				busy_frames = notice->busy_frames;
				make_busy(notice, busy_frames, FALSE);

				notice->show = 0;

			}
			else {
				/*
				 * Non thread blocking case
				 */
				xv_set(notice->sub_frame, XV_SHOW, TRUE, NULL);
			}
		}
		else {
			/*
			 * Make client frame not busy
			 */
			if (!notice->block_thread) {
				xv_set(notice->client_window, FRAME_BUSY, FALSE, NULL);
			}

			make_busy(notice, busy_frames, FALSE);

			/*
			 * Pop down notice sub frame
			 *
			 * Do only if non blocking.
			 * For thread blocking case, this was done above already.
			 */
			if (!notice->block_thread) {
				xv_set(notice->sub_frame, XV_SHOW, FALSE, NULL);
			}
		}
	}

	return (XV_OK);

}

#define ADONE ATTR_CONSUME(avlist[0]);break

static Xv_opaque notice_set_avlist(Xv_Notice notice_public, Attr_attribute *avlist)
{
    Notice_info	*notice = NOTICE_PRIVATE(notice_public);
	notice_buttons_handle last_button = NULL;
	notice_buttons_handle reuse_buttons = notice->button_info;
	int yes_button_seen = FALSE;
	int no_button_seen = FALSE;
	int num_butt = 0;
	int num_strs = 0;
	int trigger_set = 0;
	caddr_t value;
	CHAR *str;
	CHAR **new_msg = NULL;
	CHAR *one_msg[2];

#ifdef OW_I18N
	CHAR **wc_msg;
#endif

	Bool butt_changed = FALSE;
	Bool show_seen = FALSE;
	Bool bad_attr;

	for (; *avlist; avlist = attr_next(avlist)) {
		value = (caddr_t) avlist[1];
		bad_attr = FALSE;
		switch (avlist[0]) {

				/*
				 * GENERIC NOTICE ATTRIBUTES
				 * - Attributes used by ALL NOTICES
				 */
			case NOTICE_LOCK_SCREEN:
				notice->lock_screen = (avlist[1] != 0L);
				ADONE;

			case NOTICE_LOCK_SCREEN_LOOKING:
				notice->lock_screen_looking = (avlist[1] != 0L);
				ADONE;

			case NOTICE_BLOCK_THREAD:
				notice->block_thread = (avlist[1] != 0L);
				if (! notice->block_thread) {
					notice->lock_screen_looking = FALSE;
				}
				ADONE;

#ifdef OW_I18N
			case NOTICE_MESSAGE_STRINGS_ARRAY_PTR:
				/* Convert mbs to wchar before passing to new_msg */
				{
					int str_count, i;
					char **pptr = NULL;
					char *str;

					pptr = (char **)value;
					for (str_count = 0, i = 0, str = pptr[i]; str;
							str = pptr[++i]) {
						str_count++;
					}
					wc_msg = xv_calloc(str_count + 1, sizeof(CHAR *));
					pptr = (char **)value;
					for (i = 0; i < str_count; i++) {
						wc_msg[i] = _xv_mbstowcsdup(pptr[i]);
					}
					wc_msg[str_count] = (CHAR *) NULL;
					new_msg = (CHAR **) wc_msg;
				}
				ADONE;

			case NOTICE_MESSAGE_STRINGS_ARRAY_PTR_WCS:
				new_msg = (CHAR **) value;
				ADONE;

			case NOTICE_MESSAGE_STRINGS:
				/* Convert mbs to wchar before passing to new_msg */
				{
					int str_count, i;
					char **pptr = NULL;
					char *str;

					pptr = (char **)&avlist[1];
					for (str_count = 0, i = 0, str = pptr[i]; str;
							str = pptr[++i]) {
						str_count++;
					}
					wc_msg = xv_calloc(str_count + 1, sizeof(CHAR *));
					pptr = (char **)&avlist[1];
					for (i = 0; i < str_count; i++) {
						wc_msg[i] = _xv_mbstowcsdup(pptr[i]);
					}
					wc_msg[str_count] = (CHAR *) NULL;
					new_msg = (CHAR **) wc_msg;
				}
				ADONE;

			case NOTICE_MESSAGE_STRINGS_WCS:
				new_msg = (CHAR **) & avlist[1];
				ADONE;

			case NOTICE_MESSAGE_STRING:
				one_msg[0] = (wchar_t *)_xv_mbstowcsdup((char *)avlist[1]);
				one_msg[1] = (CHAR *) NULL;
				new_msg = (CHAR **) one_msg;
				ADONE;

			case NOTICE_MESSAGE_STRING_WCS:
				one_msg[0] = (CHAR *) avlist[1];
				one_msg[1] = (CHAR *) NULL;
				new_msg = (CHAR **) one_msg;
				ADONE;

			case NOTICE_BUTTON_YES:
			case NOTICE_BUTTON_YES_WCS:{
					notice_buttons_handle button;

					if (!yes_button_seen) {
						yes_button_seen = TRUE;
					}
					else {
						(void)xv_error(NULL,
								ERROR_STRING,
								XV_MSG
								("Only one NOTICE_BUTTON_YES attr allowed. Attr ignored."),
								ERROR_PKG, NOTICE, NULL);
						break;
					}

					/*   
					 * Button structs are reused for notices
					 * If there were no buttons to start off with, they
					 * are allocated
					 */
					if (reuse_buttons) {
						last_button = button = reuse_buttons;
						reuse_buttons = reuse_buttons->next;
						if (button->string) {
							free(button->string);
							button->string = (CHAR *) NULL;
						}
					}
					else {
						button = (notice_buttons_handle)
								notice_create_button_struct();
						button->panel_item = (Panel_item) NULL;
						button->next = (notice_buttons_handle) NULL;
						(void)notice_add_button_to_list(notice, button);
					}

					if (avlist[0] == NOTICE_BUTTON_YES)
						button->string =
								(wchar_t *)_xv_mbstowcsdup((char *)avlist[1]);
					else
						button->string = XV_STRSAVE((CHAR *) avlist[1]);
					button->is_yes = TRUE;
					button->value = NOTICE_YES;
					notice->yes_button_exists = TRUE;
					num_butt++;
					butt_changed = TRUE;
					ADONE;
				}

			case NOTICE_BUTTON_NO:
			case NOTICE_BUTTON_NO_WCS:{
					notice_buttons_handle button;

					if (!no_button_seen) {
						no_button_seen = TRUE;
					}
					else {
						xv_error(NULL,
								ERROR_STRING,
								XV_MSG
								("Only one NOTICE_BUTTON_NO attr allowed. Attr ignored."),
								ERROR_PKG, NOTICE, NULL);
						break;
					}

					if (reuse_buttons) {
						last_button = button = reuse_buttons;
						reuse_buttons = reuse_buttons->next;
						if (button->string) {
							free(button->string);
							button->string = (CHAR *) NULL;
						}
					}
					else {
						button = (notice_buttons_handle)
								notice_create_button_struct();
						button->panel_item = (Panel_item) NULL;
						button->next = (notice_buttons_handle) NULL;
						(void)notice_add_button_to_list(notice, button);
					}

					if (avlist[0] == NOTICE_BUTTON_NO)
						button->string =
								(wchar_t *)_xv_mbstowcsdup((char *)avlist[1]);
					else
						button->string = XV_STRSAVE((CHAR *) avlist[1]);
					button->is_no = TRUE;
					button->value = NOTICE_NO;
					notice->no_button_exists = TRUE;
					num_butt++;
					butt_changed = TRUE;

					ADONE;
				}

			case NOTICE_BUTTON:
			case NOTICE_BUTTON_WCS:{
					notice_buttons_handle button;

					if (reuse_buttons) {
						last_button = button = reuse_buttons;
						reuse_buttons = reuse_buttons->next;
						if (button->string) {
							free(button->string);
							button->string = (CHAR *) NULL;
						}
					}
					else {
						button = (notice_buttons_handle)
								notice_create_button_struct();
						button->panel_item = (Panel_item) NULL;
						button->next = (notice_buttons_handle) NULL;
						(void)notice_add_button_to_list(notice, button);
					}

					if (avlist[0] == NOTICE_BUTTON)
						button->string =
								(wchar_t *)_xv_mbstowcsdup((char *)avlist[1]);
					else
						button->string = XV_STRSAVE((CHAR *) avlist[1]);
					button->value = (int)avlist[2];
					num_butt++;
					butt_changed = TRUE;

					ADONE;
				}
#else
			case NOTICE_MESSAGE_STRINGS_ARRAY_PTR:
				new_msg = (char **)value;
				ADONE;

			case NOTICE_MESSAGE_STRINGS:
				new_msg = (char **)&avlist[1];
				ADONE;

			case NOTICE_MESSAGE_STRING:
				one_msg[0] = (char *)avlist[1];
				one_msg[1] = (char *)NULL;
				new_msg = (char **)one_msg;
				ADONE;

			case NOTICE_BUTTON_YES:{
					notice_buttons_handle button;

					if (!yes_button_seen) {
						yes_button_seen = TRUE;
					}
					else {
						(void)xv_error(XV_NULL,
								ERROR_STRING,
								XV_MSG
								("Only one NOTICE_BUTTON_YES attr allowed. Attr ignored."),
								ERROR_PKG, NOTICE, NULL);
						break;
					}

					/*
					 * Button structs are reused for notices
					 * If there were no buttons to start off with, they
					 * are allocated
					 */
					if (reuse_buttons) {
						last_button = button = reuse_buttons;
						reuse_buttons = reuse_buttons->next;
						if (button->string) {
							free(button->string);
							button->string = (char *)NULL;
						}
					}
					else {
						button = (notice_buttons_handle)
								notice_create_button_struct();
						button->panel_item = (Panel_item) NULL;
						button->next = (notice_buttons_handle) NULL;
						(void)notice_add_button_to_list(notice, button);
					}

					/*
					 * Space has to be malloc for string.
					 * For non-locking notices that use panel items, this
					 * Is not necessary, since the string is cached by the
					 * panel pkg. But for screen locking notices, we have to cache
					 * the strings. So, we do it for all cases.
					 * Doing it for one case only will make things more
					 * complicated than it already is, since we can switch
					 * back and forth from non-screen-locking to screen-locking
					 * notices.
					 */
					button->string = xv_strsave((char *)avlist[1]);
					button->is_yes = TRUE;
					button->value = NOTICE_YES;
					notice->yes_button_exists = TRUE;
					num_butt++;
					butt_changed = TRUE;

					ADONE;
				}

			case NOTICE_BUTTON_NO:{
					notice_buttons_handle button;

					if (!no_button_seen) {
						no_button_seen = TRUE;
					}
					else {
						xv_error(XV_NULL,
								ERROR_STRING,
								XV_MSG
								("Only one NOTICE_BUTTON_NO attr allowed. Attr ignored."),
								ERROR_PKG, NOTICE, NULL);
						break;
					}

					if (reuse_buttons) {
						last_button = button = reuse_buttons;
						reuse_buttons = reuse_buttons->next;
						if (button->string) {
							free(button->string);
							button->string = (char *)NULL;
						}
					}
					else {
						button = (notice_buttons_handle)
								notice_create_button_struct();
						button->panel_item = (Panel_item) NULL;
						button->next = (notice_buttons_handle) NULL;
						(void)notice_add_button_to_list(notice, button);
					}

					button->string = xv_strsave((char *)avlist[1]);
					button->is_no = TRUE;
					button->value = NOTICE_NO;
					notice->no_button_exists = TRUE;
					num_butt++;
					butt_changed = TRUE;

					ADONE;
				}

			case NOTICE_BUTTON:{
					notice_buttons_handle button;

					if (reuse_buttons) {
						last_button = button = reuse_buttons;
						reuse_buttons = reuse_buttons->next;
						if (button->string) {
							free(button->string);
							button->string = (char *)NULL;
						}
					}
					else {
						button = (notice_buttons_handle)
								notice_create_button_struct();
						button->panel_item = (Panel_item) NULL;
						button->next = (notice_buttons_handle) NULL;
						(void)notice_add_button_to_list(notice, button);
					}

					button->string = xv_strsave((char *)avlist[1]);
					button->value = (int)avlist[2];
					num_butt++;
					butt_changed = TRUE;

					ADONE;
				}
#endif

			case NOTICE_FONT:
				/*
				 * NOTICE_FONT is a create only attribute
				 */
				if (notice->new) {
					notice->notice_font = (Xv_Font) avlist[1];
				}
				ADONE;

			case NOTICE_NO_BEEPING:
				if (avlist[1]) {
					notice->dont_beep = 1;
				}
				else {
					notice->dont_beep = 0;
				}
				ADONE;

				/*
				 * END of GENERIC NOTICE ATTRIBUTES
				 */

				/*
				 * ATTRIBUTES FOR SCREEN LOCKING NOTICES
				 */
			case NOTICE_FOCUS_XY:
				/*
				 * needs to be implemented
				 */
				notice->focus_x = (int)avlist[1];
				notice->focus_y = (int)avlist[2];
				notice->focus_specified = TRUE;
				ADONE;

			case NOTICE_TRIGGER:
				notice->default_input_code = (int)avlist[1];
				trigger_set = 1;
				ADONE;

			case NOTICE_TRIGGER_EVENT:
				if ((Event *) value) {
					notice->event = (Event *) value;
				}
				ADONE;

			case NOTICE_STATUS:
				if ((int *)value) {
					notice->result_ptr = (int *)value;
				}
				ADONE;

				/*
				 * END OF SCREEN LOCKING ATTRIBUTES
				 */

				/*
				 * ATTRIBUTES FOR NON SCREEN LOCKING ATTRIBUTES
				 */
			case NOTICE_EVENT_PROC:
				if (avlist[1]) {
					notice->event_proc = (void (*)(Xv_notice, int, Event *))avlist[1];
				}
				ADONE;

			case NOTICE_BUSY_FRAMES:
				if (notice->lock_screen) {
					ADONE;
				}

				if ((Frame) value) {
					Frame *busy_frames;
					int i;
					int count = 0;

					if (notice->busy_frames) {
						free(notice->busy_frames);
					}

					/*
					 * Count frames and alloc space for list
					 */
					for (i = 1; avlist[i]; ++i, ++count);
					busy_frames = (Frame *) xv_calloc((unsigned)count + 1, (unsigned)sizeof(Frame));

					/*
					 * Copy frames into list
					 */
					for (i = 1; avlist[i]; ++i) {
						busy_frames[i - 1] = avlist[i];
					}
					/*
					 * End list with NULL
					 */
					busy_frames[count] = XV_NULL;

					notice->busy_frames = busy_frames;
				}
				ADONE;


			case XV_SHOW:
				/*
				 * If the notice is already in the state we want to set it to,
				 * skip
				 */
				if ((avlist[1] && notice->show) ||
						(!avlist[1] && !(notice->show))) {
					break;
				}

				/*
				 * Set flag apprpriately
				 */
				notice->show = (value != NULL);
				show_seen = TRUE;

				break;

			case XV_END_CREATE:
				/*
				 * If no font specified, try to get client_window font
				 */
				if (!notice->notice_font) {
					int e;

					if ((e = notice_determine_font(notice->client_window,
											notice)) != XV_OK) {
						/*
						 * If error occurred during font determination, 
						 * return error code
						 */
						return (e);
					}
				}

				/*
				 * Pop up notice below if show == TRUE
				 */
				if (notice->show) {
					show_seen = TRUE;
				}

				/*
				 * Set the new flag to false so that the notice can be pop'd up
				 * if needed below
				 */
				notice->new = FALSE;

				break;

			default:
				bad_attr = TRUE;
				xv_check_bad_attr(&xv_notice_pkg, avlist[0]);
				break;

		}

		if (!bad_attr) {
			ATTR_CONSUME(avlist[0]);
		}
	}

	if (notice->new && (num_butt == 0) && (trigger_set == 0)) {
		notice->default_input_code = (int)ACTION_STOP;
	}

	if (notice->lock_screen) {
		/* a notice that locks the screen should also *look* like this */
		notice->lock_screen_looking = TRUE;
	}

	/*
	 * New notice message strings specified
	 */
	if (new_msg) {
		notice_msgs_handle msg, cur_msg;
		int i;

#ifdef OW_I18N
		wchar_t ret = (wchar_t)'\n';
#else
		char ret = '\n';
#endif

		CHAR *curStr;
		CHAR **cur_str_ptr;

		/*
		 * Count new strings
		 */
		for (i = 0, num_strs = 0, str = new_msg[i];
				str; num_strs++, str = new_msg[++i]) {

			/*
			 * for every return character increment number of strings
			 */
			while (1) {
				if ((curStr = STRCHR(str, ret)) == NULL) {
					/*
					 * If return character found, break out of loop
					 */
					break;
				}
				str = curStr + 1;
				num_strs++;
			}
		}

		/*
		 * If only one string specified, set count to 1
		 */
		if (!num_strs && new_msg[0]) {
			num_strs = 1;
		}

		/*
		 * If new string count is more than previous, create new
		 * message structs
		 */
		if (num_strs > notice->number_of_strs) {
			for (i = notice->number_of_strs; i < num_strs; ++i) {
				msg = notice_create_msg_struct();

				msg->panel_item = (Panel_item) NULL;

				msg->string = (CHAR *) NULL;
				msg->next = (notice_msgs_handle) NULL;
				notice_add_msg_to_list(notice, msg);
			}
		}
		else {
			/*
			 * If new string count is less than previous, free excess
			 * message structs
			 */
			if (num_strs < notice->number_of_strs) {
				for (i = num_strs; i < notice->number_of_strs; ++i) {
					msg = notice->msg_info;

					if (!msg) {
						break;
					}

					if (msg->string) {
						free(msg->string);
						msg->string = (CHAR *) NULL;
					}

					if (msg->panel_item) {
						xv_destroy(msg->panel_item);
					}

					notice->msg_info = msg->next;

					free((char *)msg);
				}
			}
		}

		/*
		 * At this point the number of message structs == the number of
		 * message strings that we need to store
		 */

		/*
		 * cur_str_ptr is for traversing ptr passed on avlist
		 */
		cur_str_ptr = new_msg;
		cur_msg = notice->msg_info;

		while (*cur_str_ptr) {
			/*
			 * retPtr is used for hunting down return's
			 */
			CHAR *retPtr = *cur_str_ptr;
			int len;

			/*
			 * curStr is used for traversing the current string
			 */
			curStr = *cur_str_ptr;

			/*
			 * Save string:
			 * Each string on the avlist can contain more than one
			 * return terminated string.
			 *
			 * Do until no more return chars found:
			 */
			while (retPtr) {
				/*
				 * Search for return character
				 */
				retPtr = STRCHR(curStr, ret);

				if (retPtr) {
					CHAR *tmp;

					/*
					 * If return character found, calculate length of 
					 * needed to be alloc'd.
					 */
					len = retPtr - curStr + 1;
					tmp = xv_calloc((unsigned)len, (unsigned)sizeof(CHAR));
					/*
					 * copy string
					 */

#ifdef OW_I18N
					STRNCPY(tmp, curStr, len - 1);

#else

#ifdef SVR4
					memmove(tmp, curStr, len - 1);
#else
					bcopy(curStr, tmp, (unsigned long)len - 1);
#endif /* SVR4 */
#endif /* OW_I18N */

					/*
					 * Pad with terminating null character
					 */

#ifdef OW_I18N
					tmp[len - 1] = (wchar_t)'\0';
#else
					tmp[len - 1] = '\0';
#endif

					cur_msg->string = tmp;

					/*
					 * Set current str to one character AFTER return
					 * character
					 */
					curStr = retPtr + 1;
				}
				else {
					/*
					 * The current string is already null terminated
					 */
					cur_msg->string = XV_STRSAVE(curStr);
				}

				/*
				 * Advance to next message item in notice
				 */
				cur_msg = cur_msg->next;
			}


			/*
			 * Advance to next string on avlist
			 */
			cur_str_ptr++;
		}

		/*
		 * Update number of notice message strings
		 */
		notice->number_of_strs = num_strs;

		/*
		 * If we are in lock screen mode, make sure we set the layout
		 * flag so that when we switch to non screen locking mode,
		 * we do a layout
		 */
		if (notice->lock_screen) notice->need_layout = 1;
		if (notice->lock_screen_looking) notice->need_layout = 1;
	}

	/*
	 * If there were new buttons specified and not all the old 
	 * button structs were used, free old button structs
	 * and string space
	 */
	if (butt_changed) {
		notice_buttons_handle cur, prev;

		/*
		 * If there were button strcuts that were NOT reused, free them
		 */
		if (reuse_buttons) {
			cur = prev = reuse_buttons;
			while (cur) {
				prev = cur;
				cur = cur->next;
				if (prev->string) {
					free(prev->string);
					prev->string = (CHAR *) NULL;
				}

				if (prev->panel_item) {
					xv_destroy(prev->panel_item);
				}

				free((CHAR *) prev);
			}

			/*
			 * End the new list with NULL
			 */
			if (last_button) {
				last_button->next = NULL;
			}

		}
		notice->number_of_buttons = num_butt;

		if (notice->lock_screen) notice->need_layout = 1;
		if (notice->lock_screen_looking) notice->need_layout = 1;

		/*
		 * Also, if did not see NOTICE_BUTTON_YES (default button),
		 * make the first button the default
		 */
		if (!yes_button_seen) {
			notice->button_info->is_yes = TRUE;
			notice->yes_button_exists = TRUE;
		}
	}

	/*
	 * If no buttons specified, give default
	 */
	if (notice->number_of_buttons == 0) {
		notice_add_default_button(notice);
	}

	/*
	 * We don't need to do layout if the notice is screen locking
	 */
	if (!notice->lock_screen) {
		/*
		 * Do layout only if the layout flag is set(this is done
		 * only when in screen locking mode), or new message
		 * or buttons were specified.
		 */
		if (notice->need_layout || new_msg || butt_changed) {
			notice_subframe_layout(notice, TRUE, TRUE);
		}
	}

	/*
	 * If this is not within xv_create
	 */
	if (!(notice->new) && show_seen) {
		Xv_screen screen;

		if (notice->client_window)
			screen = XV_SCREEN_FROM_WINDOW(notice->client_window);
    	else if (notice->owner_window) 
			screen = XV_SCREEN_FROM_WINDOW(notice->owner_window);
		else screen = xv_default_screen;

		if (! xv_get(screen, SCREEN_ENHANCED_OLWM)) {
    		notice->lock_screen_looking = FALSE;
		}
		notice_do_show(notice);
	}

	return XV_OK;
}

static Xv_opaque notice_get_attr(Xv_notice notice_public, int *status, Attr_attribute attr, va_list valist)
{
	Notice_info *notice = NOTICE_PRIVATE(notice_public);
	Xv_opaque v = (Xv_opaque) NULL;

	switch (attr) {
		case NOTICE_LOCK_SCREEN:
			v = (Xv_opaque) notice->lock_screen;
			break;

		case NOTICE_LOCK_SCREEN_LOOKING:
			v = (Xv_opaque) notice->lock_screen_looking;
			break;

		case NOTICE_BLOCK_THREAD:
			v = (Xv_opaque) notice->block_thread;
			break;

		case NOTICE_NO_BEEPING:
			v = (Xv_opaque) notice->dont_beep;
			break;

		case NOTICE_FONT:
			v = (Xv_opaque) notice->notice_font;
			break;

		case NOTICE_STATUS:
			v = (Xv_opaque) notice->result;
			break;

		case XV_SHOW:
			v = (Xv_opaque) notice->show;
			break;

		default:
			if (xv_check_bad_attr(&xv_notice_pkg, attr) == XV_ERROR) {
				*status = XV_ERROR;
			}
			break;
	}

	return ((Xv_opaque) v);

}

static int notice_destroy_internal(Xv_notice notice_public, Destroy_status	status)
{
	Notice_info *notice = NOTICE_PRIVATE(notice_public);


	if (status == DESTROY_CLEANUP) {
		/*
		 * Free button structures
		 */
		if (notice->button_info) {
			notice_free_button_structs(notice->button_info);
			notice->button_info = (notice_buttons_handle) NULL;
		}

		if (notice->msg_info) {
			notice_free_msg_structs(notice->msg_info);
			notice->msg_info = (notice_msgs_handle) NULL;
		}

		/*
		 * Destroy subframe
		 */
		if (notice->sub_frame) {
			xv_set(notice->sub_frame,
					XV_KEY_DATA, notice_context_key, NULL, NULL);
			xv_destroy_safe(notice->sub_frame);
			notice->sub_frame = (Frame) NULL;
		}

		/*
		 * Destroy window used for screen locking notice
		 */
		if (notice->fullscreen_window) {
			xv_destroy(notice->fullscreen_window);
			notice->fullscreen_window = (Xv_window) NULL;
		}

		/*
		 * Free list of busy frames
		 */
		if (notice->busy_frames) {
			free(notice->busy_frames);
			notice->busy_frames = (Frame *) NULL;
		}

		/*
		 * Free notice struct
		 */
		free((char *)notice);
	}

	return (XV_OK);
}

/*
 * ----------------------- Public Interface -------------------------
 */

Xv_public int notice_prompt(Xv_Window client_window, Event *event, ...)
{
    va_list			valist;
    int				result;
    AVLIST_DECL;

	fprintf(stderr, "This program uses deprecated function \"notice_prompt\".\n");
    if (!client_window) {
		xv_error(XV_NULL,
				ERROR_STRING, XV_MSG("NULL parent window passed to notice_prompt(). Not allowed."),
	         	ERROR_PKG, NOTICE,
		 		NULL);
		return NOTICE_FAILED;
	}

    VA_START(valist, event);
    MAKE_AVLIST(valist, avlist);
    va_end(valist);

	xv_destroy(xv_create(client_window, NOTICE,
					ATTR_LIST, avlist,
					NOTICE_STATUS, &result,
					XV_SHOW, TRUE,
					NULL));

	return result;
}

const Xv_pkg xv_notice_pkg = {
    "Notice",
	ATTR_PKG_NOTICE,
    sizeof(Xv_notice_struct),
    &xv_generic_pkg,		/* subclass of generic */
    notice_init_internal,
    notice_set_avlist,
    notice_get_attr,
    notice_destroy_internal,
    NULL			/* no find proc */
};
