#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include "i18n.h"
#include "ollocale.h"
#include <olgx/olgx.h>
#include "olwm.h"
#include "atom.h"
#include "globals.h"
#include "resources.h"

#include "events.h"
#include "mem.h"
#include "win.h"
#include "group.h"
#include <stdarg.h>

char dra_color_c_sccsid[] = "@(#) %M% V%I% %E% %U% $Id: dra_color.c,v 2.4 2025/01/11 20:12:45 dra Exp $";

static int minlev = 1000;
static int maxlev = 0;

void dra_olwm_set_traces(long *data)
{
	char buf[100];

	minlev = data[3];
	maxlev = data[4];
	sprintf(buf, "new trace interval [%d, %d]\n", minlev, maxlev);
	write(2, buf, strlen(buf));
}

void dra_olwm_trace(int level, char *format, ...)
{
	va_list pvar;

	if (level < minlev || level > maxlev) return;

	va_start(pvar, format);
	vfprintf(stderr, format, pvar);
	va_end(pvar);
}

typedef struct {
	int refcount;
	unsigned long clients_flags;
	unsigned long back_pixel, fore_pixel;
	GC gc[NUM_GCS];
	Graphics_info *gi[NUM_GINFOS];
	Bool gc_is_own[NUM_GCS];
} ClientColorInfo;

#define _OL_WC_FOREGROUND (1<<0)
#define _OL_WC_BACKGROUND (1<<1)
#define _OL_WC_BORDER (1<<2)

typedef struct {
	unsigned long	flags;
	unsigned long	fg_red;
	unsigned long	fg_green;
	unsigned long	fg_blue;
	unsigned long	bg_red;
	unsigned long	bg_green;
	unsigned long	bg_blue;
	unsigned long	bd_red;
	unsigned long	bd_green;
	unsigned long	bd_blue;
} OLWinColors;

#define OLCOL_NUM_LONGS (sizeof(OLWinColors)/sizeof(unsigned long))

void dra_free_client_info(Display *dpy, ClientColorInfo *clcol)
{
	if (clcol) {
		dra_olwm_trace(100, "in dra_free_client_info, refcount = %d\n",
										clcol->refcount);
		if (-- clcol->refcount <= 0) {
			int i;

			olgx_destroy(clcol->gi[NORMAL_GINFO]);
			olgx_destroy(clcol->gi[BUTTON_GINFO]);
			olgx_destroy(clcol->gi[TEXT_GINFO]);
			olgx_destroy(clcol->gi[REVPIN_GINFO]);
			for (i = 0; i < NUM_GCS; i++) {
				if (clcol->gc_is_own[i]) XFreeGC(dpy, clcol->gc[i]);
			}
			MemFree(clcol);
		}
	}
}

void colors_new_state(Client *cli, Display *dpy, Window window,
						Client *wanted_a_subgroup)
{
	XColor fg, bg0, bg1, bg2, bg3;
	Colormap cmap;
	unsigned long flags;
	ScreenInfo	*scrInfo;
	OLWinColors *win_colors;
	unsigned long nitems, rest;
	ClientColorInfo *new;
	XGCValues values;
	unsigned long pixvals[5];
	int dflag;
	Client *cliLead;
	Window *colfol, groupie;

	dra_olwm_trace(110, "\nin dra_colors_new_state(win=%x, cli=%x, ws=%x\n",
					window, cli, wanted_a_subgroup);

	cli->color_master = (Window)0;
	if (! cli->scrInfo->iscolor) return;

	if (cli->groupmask != GROUP_LEADER) {
		dra_olwm_trace(120, "is not GROUP_LEADER\n");

		groupie = cli->groupid;

		/* look whether there is a _OL_COLORS_FOLLOW property */
		colfol = (Window *)GetWindowProperty(dpy, window,
					AtomColorsFollow, 0, 1, 
					XA_WINDOW, 32, &nitems, &rest);
		if (colfol) {
			groupie = *colfol;
			XFree((char *)colfol);
			dra_olwm_trace(120, "is a color follower for %x\n", groupie);
			cli->color_master = groupie;
		}
		else if (wanted_a_subgroup) {
			/* this  client requested as its window group a window
			 * that was not a group leader.
			 * Check whether this had a color_master...
			 */
			dra_olwm_trace(120, "but wanted a different leader\n");
			if (wanted_a_subgroup->color_master) {
				groupie = wanted_a_subgroup->color_master;
				dra_olwm_trace(120, "... that is a color follower for %x\n",
											groupie);
				cli->color_master = groupie;
			}
		}

    
		if ((cliLead = (Client *)GroupLeader(groupie))) {
			dra_olwm_trace(120, "has group leader (or color group leader)\n");

			if (cliLead->client_colors) {
				ClientColorInfo *clcol =
						(ClientColorInfo *)cliLead->client_colors;

				
				dra_olwm_trace(120, "with a color info\n");
				clcol->refcount++;
				cli->client_colors = cliLead->client_colors;
			}
		}

		return;
	}
	else {
		dra_olwm_trace(120, "is a GROUP_LEADER\n");

	/*** PROPERTY_DEFINITION ***********************************************
	 * _OL_DECOR_DEL     Type ATOM        Format 32
	 * Owner: client, Reader: wm
	 */
		/* look whether there is a _OL_COLORS_FOLLOW property */
		colfol = (Window *)GetWindowProperty(dpy, window,
					AtomColorsFollow, 0, 1, 
					XA_WINDOW, 32, &nitems, &rest);

		if (colfol) {
			groupie = *colfol;
			XFree((char *)colfol);
			dra_olwm_trace(120, "is a color follower for %x\n", groupie);
			cli->color_master = groupie;
    
			if ((cliLead = (Client *)GroupLeader(groupie))) {
				dra_olwm_trace(120, "has group leader (or color group leader)\n");

				if (cliLead->client_colors) {
					ClientColorInfo *clcol =
							(ClientColorInfo *)cliLead->client_colors;

				
					dra_olwm_trace(120, "with a color info\n");
					clcol->refcount++;
					cli->client_colors = cliLead->client_colors;
				}
			}

			return;
		}
	}

	dra_olwm_trace(120, "checking prop\n");
	win_colors = (OLWinColors *)GetWindowProperty(dpy, window,
					AtomWinColors, 0, OLCOL_NUM_LONGS,
					AtomWinColors, 32, &nitems, &rest);

	if (! win_colors) return;

	if (nitems != OLCOL_NUM_LONGS || rest != 0) {
		dra_olwm_trace(120, "have _OL_WIN_COLORS of wrong length on window 0x%x\n",
				window);
		XFree((char *)win_colors);
		return;
	}
	dra_olwm_trace(120, "have _OL_WIN_COLORS\n");

	flags = win_colors->flags;
	if (!(flags & (_OL_WC_BACKGROUND | _OL_WC_FOREGROUND))) {
		XFree((char *)win_colors);
		return;
	}

	scrInfo = cli->scrInfo;

	cmap = scrInfo->colormap;
	fg.pixel = scrInfo->colorInfo.fgColor;
	bg0.pixel = scrInfo->colorInfo.bg0Color;
	bg1.pixel = scrInfo->colorInfo.bg1Color;
	bg2.pixel = scrInfo->colorInfo.bg2Color;
	bg3.pixel = scrInfo->colorInfo.bg3Color;

	XQueryColor(dpy,cmap,&fg);
	XQueryColor(dpy,cmap,&bg0);
	XQueryColor(dpy,cmap,&bg1);
	XQueryColor(dpy,cmap,&bg2);
	XQueryColor(dpy,cmap,&bg3);

	if (flags & _OL_WC_FOREGROUND) {
		fg.red = win_colors->fg_red;
		fg.green = win_colors->fg_green;
		fg.blue = win_colors->fg_blue;
		fg.flags = DoRed | DoGreen | DoBlue;
	}

	if (flags & _OL_WC_BACKGROUND) {
		bg1.red = win_colors->bg_red;
		bg1.green = win_colors->bg_green;
		bg1.blue = win_colors->bg_blue;
		bg1.flags = DoRed | DoGreen | DoBlue;
	}

	XFree((char *)win_colors);

	if (flags & _OL_WC_FOREGROUND) {
		XColor newfg;

		newfg = fg;
		if (XAllocColor(dpy, cmap, &newfg) == 0) {
			fprintf(stderr, "XAllocColor fg (%d, %d, %d) failed\n",
					newfg.red, newfg.green, newfg.blue);
			flags &= (~_OL_WC_FOREGROUND);
		}
		else {
			fg = newfg;
		}
	}

	if (flags & _OL_WC_BACKGROUND) {
		olgx_calculate_3Dcolors(&fg,&bg1,&bg2,&bg3,&bg0);

		if (! XAllocColor(dpy, cmap, &bg1))
			fprintf(stderr, "XAllocColor bg1 (%d, %d, %d) failed\n",
					bg1.red, bg1.green, bg1.blue);
		if (! XAllocColor(dpy, cmap, &bg2))
			fprintf(stderr, "XAllocColor bg2 (%d, %d, %d) failed\n",
					bg2.red, bg2.green, bg2.blue);
		if (! XAllocColor(dpy, cmap, &bg3))
			fprintf(stderr, "XAllocColor bg3 (%d, %d, %d) failed\n",
					bg3.red, bg3.green, bg3.blue);
		if (! XAllocColor(dpy, cmap, &bg0))
			fprintf(stderr, "XAllocColor bg0 (%d, %d, %d) failed\n",
					bg0.red, bg0.green, bg0.blue);
	}

	pixvals[OLGX_WHITE] = bg0.pixel;
	pixvals[OLGX_BG1] = bg1.pixel;
	pixvals[OLGX_BG2] = bg2.pixel;
	pixvals[OLGX_BG3] = bg3.pixel;

	new = (ClientColorInfo *)calloc(1, sizeof(ClientColorInfo));
	if (! new) return;

	new->refcount = 1;
	new->fore_pixel = scrInfo->colorInfo.fgColor;
	new->back_pixel = scrInfo->colorInfo.bgColor;

	new->clients_flags = flags;

	new->gc[ROOT_GC] = scrInfo->gc[ROOT_GC];
	new->gc[BORDER_GC] = scrInfo->gc[BORDER_GC];
	new->gc[WORKSPACE_GC] = scrInfo->gc[WORKSPACE_GC];
	new->gc[ICON_NORMAL_GC] = scrInfo->gc[ICON_NORMAL_GC];
	new->gc[ICON_MASK_GC] = scrInfo->gc[ICON_MASK_GC];
	new->gc[ICON_BORDER_GC] = scrInfo->gc[ICON_BORDER_GC];

	/* 
	 * Create a GC for Foregound w/ TitleFont
	 */
	if (flags & _OL_WC_FOREGROUND) {
		new->fore_pixel = fg.pixel;

		values.function = GXcopy;
		values.foreground = new->fore_pixel;
		values.font = GRV.TitleFontInfo->fid;
		values.graphics_exposures = False;
		new->gc[FOREGROUND_GC] = XCreateGC(dpy,
					scrInfo->pixmap[PROTO_DRAWABLE],
					(GCFont | GCFunction | GCForeground | GCGraphicsExposures),
					&values);
		new->gc_is_own[FOREGROUND_GC] = True;
	}
	else new->gc[FOREGROUND_GC] = scrInfo->gc[FOREGROUND_GC];

	/* 
	 * Create a GC for busy stipple in foreground
	 */
	if (flags & _OL_WC_FOREGROUND) {
		values.function = GXcopy;
		values.foreground = new->fore_pixel;
		values.fill_style = FillStippled;
		values.stipple = scrInfo->pixmap[BUSY_STIPPLE];
		values.graphics_exposures = False;
		new->gc_is_own[BUSY_GC] = True;
		new->gc[BUSY_GC] = XCreateGC( dpy,
							scrInfo->pixmap[PROTO_DRAWABLE],
							( GCFunction | GCForeground | GCGraphicsExposures | 
								GCStipple | GCFillStyle),
							&values);
	}
	else new->gc[BUSY_GC] = scrInfo->gc[BUSY_GC];

	if (flags & _OL_WC_BACKGROUND) {
		new->back_pixel = bg1.pixel;

		values.function = GXcopy;
		values.foreground = bg1.pixel;
		values.font = GRV.TitleFontInfo->fid;
		values.graphics_exposures = False;
		new->gc_is_own[WINDOW_GC] = True;
		new->gc[WINDOW_GC] = XCreateGC(dpy,
					scrInfo->pixmap[PROTO_DRAWABLE],
					(GCFunction | GCForeground | GCFont | GCGraphicsExposures),
					&values);
	}
	else new->gc[WINDOW_GC] = scrInfo->gc[WINDOW_GC];

	dflag = (GRV.ui_style == UIS_3D_COLOR) ? OLGX_3D_COLOR : OLGX_2D;

	pixvals[OLGX_BLACK] = fg.pixel;

	/*
	 *	If 3D mode then get all 4 bg colors
	 */
	if (GRV.ui_style == UIS_3D_COLOR) {
		pixvals[OLGX_WHITE] = bg0.pixel;
		pixvals[OLGX_BG1] = bg1.pixel;
		pixvals[OLGX_BG2] = bg2.pixel;
		pixvals[OLGX_BG3] = bg3.pixel;

	}
	else {
		/*
		 *	Else if 2D mode then just use bg1
		 */
		pixvals[OLGX_WHITE] = pixvals[OLGX_BG1] =
		pixvals[OLGX_BG2] = pixvals[OLGX_BG3] = bg1.pixel;
	}

	/* 
	 * Gis for drawing in window color with title font
	 *	most window objects and frame title
 	 */
	new->gi[NORMAL_GINFO] = olgx_main_initialize(dpy,
		scrInfo->screen, scrInfo->depth, dflag,
		GRV.GlyphFontInfo,
		GRV.TitleFontInfo,
		pixvals,NULL);

	/* 
	 * Gis for drawing in window color with button font
	 *	notice buttons & menu buttons
	 */
	new->gi[BUTTON_GINFO] = olgx_main_initialize(dpy,
		scrInfo->screen, scrInfo->depth, dflag,
	 	GRV.GlyphFontInfo,
		GRV.ButtonFontInfo,
		pixvals,NULL);

	/* 
	 * Gis for drawing in window color with text font
	 *	notice descriptive text and 2D resize corners
	 *
	 * NOTE: this is always in 2D, because the resize corners may be
	 * painted in 2D even if everything else is in 3D.  This relies
	 * on the fact that notice text is never truncated, so it will
	 * never require the 3D "more arrow".
	 */
	pixvals[OLGX_WHITE] = bg1.pixel;
	new->gi[TEXT_GINFO] = olgx_main_initialize(dpy,
		scrInfo->screen, scrInfo->depth, OLGX_2D,
	       	GRV.GlyphFontInfo,
		GRV.TextFontInfo,
		pixvals,NULL);

	/* 
	 * Gis for drawing pushpin in reverse - useful only in 2D
	 *	swap fb/bg0 entries
         */
	pixvals[OLGX_WHITE] = fg.pixel;
	if (GRV.ui_style == UIS_2D_BW) pixvals[OLGX_BLACK] = bg0.pixel;
	else pixvals[OLGX_BLACK] = bg1.pixel;

	new->gi[REVPIN_GINFO] = olgx_main_initialize(dpy,
		scrInfo->screen, scrInfo->depth, dflag,
		GRV.GlyphFontInfo,
		GRV.TitleFontInfo,
		pixvals,NULL);

	cli->client_colors = (void *)new;
}

void dra_colors_new_state(Client *cli, Display *dpy, Window window,
						Client *wanted_a_subgroup, WinPaneFrame *frame)
{
	ClientColorInfo *c;

	colors_new_state(cli, dpy, window, wanted_a_subgroup);
	if (! cli->client_colors) return;

	c = (ClientColorInfo *)cli->client_colors;
	XSetWindowBackground(dpy, frame->core.self, c->back_pixel);
}

static void update_title_font(Client *cli, ScreenInfo	*scrInfo)
{
	XFontStruct	*font = GRV.TitleFontInfo;
	ClientColorInfo *c = (ClientColorInfo *)cli->client_colors;

	if (cli->scrInfo != scrInfo) return;
	if (c->gc_is_own[WINDOW_GC]) {
		XSetFont(scrInfo->dpy, c->gc[WINDOW_GC], font->fid);
	}
	if (c->gc_is_own[FOREGROUND_GC]) {
		XSetFont(scrInfo->dpy, c->gc[FOREGROUND_GC], font->fid);
	}
	olgx_set_text_font(c->gi[NORMAL_GINFO], font, OLGX_NORMAL);
	olgx_set_text_font(c->gi[REVPIN_GINFO], font, OLGX_NORMAL);
}

void dra_update_title_font(ScreenInfo	*scrInfo)
{
	dra_all_recolored_clients(update_title_font, scrInfo);
}

static void update_button_font(Client *cli, ScreenInfo	*scrInfo)
{
	XFontStruct	*font = GRV.ButtonFontInfo;
	ClientColorInfo *c = (ClientColorInfo *)cli->client_colors;

	if (cli->scrInfo != scrInfo) return;
	olgx_set_text_font(c->gi[BUTTON_GINFO], font, OLGX_NORMAL);
}

void dra_update_button_font(ScreenInfo	*scrInfo)
{
	dra_all_recolored_clients(update_button_font, scrInfo);
}

static void update_glyph_font(Client *cli, ScreenInfo	*scrInfo)
{
	XFontStruct	*font = GRV.GlyphFontInfo;
	ClientColorInfo *c = (ClientColorInfo *)cli->client_colors;

	if (cli->scrInfo != scrInfo) return;
	olgx_set_glyph_font(c->gi[NORMAL_GINFO], font, OLGX_NORMAL);
	olgx_set_glyph_font(c->gi[REVPIN_GINFO], font, OLGX_NORMAL);
	olgx_set_glyph_font(c->gi[TEXT_GINFO], font, OLGX_NORMAL);
	olgx_set_glyph_font(c->gi[BUTTON_GINFO], font, OLGX_NORMAL);
}

void dra_update_glyph_font(ScreenInfo	*scrInfo)
{
	dra_all_recolored_clients(update_glyph_font, scrInfo);
}

static void update_window_color(Client *cli, ScreenInfo	*scrInfo)
{
	ClientColorInfo *c = (ClientColorInfo *)cli->client_colors;

	if (cli->scrInfo != scrInfo) return;
	if (c->clients_flags & _OL_WC_BACKGROUND) return; /* has own color */

	/* now we know that this client has the default window color */

	if (c->gc_is_own[WINDOW_GC]) {
		XSetForeground(scrInfo->dpy, c->gc[WINDOW_GC],
							scrInfo->colorInfo.bg1Color);
	}

	olgx_set_single_color(c->gi[NORMAL_GINFO], OLGX_WHITE,
							scrInfo->colorInfo.bg0Color, OLGX_NORMAL);
	olgx_set_single_color(c->gi[NORMAL_GINFO], OLGX_BG1,
							scrInfo->colorInfo.bg1Color, OLGX_NORMAL);
	olgx_set_single_color(c->gi[NORMAL_GINFO], OLGX_BG2,
							scrInfo->colorInfo.bg2Color, OLGX_NORMAL);
	olgx_set_single_color(c->gi[NORMAL_GINFO], OLGX_BG3,
							scrInfo->colorInfo.bg3Color, OLGX_NORMAL);

	olgx_set_single_color(c->gi[BUTTON_GINFO], OLGX_WHITE,
							scrInfo->colorInfo.bg0Color, OLGX_NORMAL);
	olgx_set_single_color(c->gi[BUTTON_GINFO], OLGX_BG1,
							scrInfo->colorInfo.bg1Color, OLGX_NORMAL);
	olgx_set_single_color(c->gi[BUTTON_GINFO], OLGX_BG2,
							scrInfo->colorInfo.bg2Color, OLGX_NORMAL);
	olgx_set_single_color(c->gi[BUTTON_GINFO], OLGX_BG3,
							scrInfo->colorInfo.bg3Color, OLGX_NORMAL);

	olgx_set_single_color(c->gi[REVPIN_GINFO], OLGX_BLACK,
							scrInfo->colorInfo.bg0Color, OLGX_NORMAL);
	olgx_set_single_color(c->gi[REVPIN_GINFO], OLGX_BG1,
							scrInfo->colorInfo.bg1Color, OLGX_NORMAL);
	olgx_set_single_color(c->gi[REVPIN_GINFO], OLGX_BG2,
							scrInfo->colorInfo.bg2Color, OLGX_NORMAL);
	olgx_set_single_color(c->gi[REVPIN_GINFO], OLGX_BG3,
							scrInfo->colorInfo.bg3Color, OLGX_NORMAL);

	olgx_set_single_color(c->gi[TEXT_GINFO], OLGX_WHITE,
							scrInfo->colorInfo.bg0Color, OLGX_NORMAL);
	olgx_set_single_color(c->gi[TEXT_GINFO], OLGX_BG1,
							scrInfo->colorInfo.bg1Color, OLGX_NORMAL);
	olgx_set_single_color(c->gi[TEXT_GINFO], OLGX_BG2,
							scrInfo->colorInfo.bg2Color, OLGX_NORMAL);
	olgx_set_single_color(c->gi[TEXT_GINFO], OLGX_BG3,
							scrInfo->colorInfo.bg3Color, OLGX_NORMAL);
}

void dra_update_window_color(ScreenInfo	*scrInfo)
{
	dra_olwm_trace(150, "in dra_update_window_color\n");
	dra_all_recolored_clients(update_window_color, scrInfo);
}

static void update_foreground_color(Client *cli, ScreenInfo	*scrInfo)
{
	ClientColorInfo *c = (ClientColorInfo *)cli->client_colors;

	if (cli->scrInfo != scrInfo) return;
	if (c->clients_flags & _OL_WC_FOREGROUND) return; /* has own color */

	/* now we know that this client has the default foreground color */

	olgx_set_single_color(c->gi[NORMAL_GINFO], OLGX_BLACK,
							scrInfo->colorInfo.fgColor, OLGX_NORMAL);
	olgx_set_single_color(c->gi[BUTTON_GINFO], OLGX_BLACK,
							scrInfo->colorInfo.fgColor, OLGX_NORMAL);
	olgx_set_single_color(c->gi[TEXT_GINFO], OLGX_BLACK,
							scrInfo->colorInfo.fgColor, OLGX_NORMAL);
	olgx_set_single_color(c->gi[REVPIN_GINFO], OLGX_WHITE,
							scrInfo->colorInfo.fgColor, OLGX_NORMAL);
}

void dra_update_foreground_color(ScreenInfo	*scrInfo)
{
	dra_olwm_trace(150, "in dra_update_foreground\n");
	dra_all_recolored_clients(update_foreground_color, scrInfo);
}

Graphics_info *dra_WinGI(WinGeneric *w, int idx)
{
	if (w->core.client->client_colors) {
		ClientColorInfo *clcol =
				(ClientColorInfo *)w->core.client->client_colors;

		return clcol->gi[idx];
	}
	else {
		return w->core.client->scrInfo->gi[idx];
	}
}

GC dra_WinGC(WinGeneric *w, int idx)
{
	if (w->core.client->client_colors) {
		ClientColorInfo *clcol =
				(ClientColorInfo *)w->core.client->client_colors;

		return clcol->gc[idx];
	}
	else {
		return w->core.client->scrInfo->gc[idx];
	}
}

void dra_setup_menu_color(WinGeneric *menuWin, Client *cli)
{
	XSetWindowAttributes attr;

	attr.border_pixel = cli->scrInfo->colorInfo.fgColor;

	/* andererseits waere das weiss: */
	attr.background_pixel = cli->scrInfo->colorInfo.bg1Color;

	if (cli->client_colors) {
		ClientColorInfo *clcol =
				(ClientColorInfo *)cli->client_colors;

		if (clcol->clients_flags & _OL_WC_FOREGROUND) {
			attr.border_pixel = clcol->fore_pixel;
		}

		if (clcol->clients_flags & _OL_WC_BACKGROUND) {
			attr.background_pixel = clcol->back_pixel;
		}

		menuWin->core.client = cli;
	}
	else {
		menuWin->core.client = cli->scrInfo->rootwin->core.client;
	}

	XChangeWindowAttributes(cli->dpy, menuWin->core.self,
					CWBackPixel | CWBorderPixel, &attr);
}
