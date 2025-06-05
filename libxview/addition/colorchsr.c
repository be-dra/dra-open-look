/*
 * This file is a product of Bernhard Drahota and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify this file without charge, but are not authorized to
 * license or distribute it to anyone else except as part of a product
 * or program developed by the user.
 *
 * THIS FILE IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 *
 * This file is provided with no support and without any obligation on the
 * part of Bernhard Drahota to assist in its use, correction,
 * modification or enhancement.
 *
 * BERNHARD DRAHOTA SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY THIS FILE
 * OR ANY PART THEREOF.
 *
 * In no event will Bernhard Drahota be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even
 * if B. Drahota has been advised of the possibility of such damages.
 */
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include <xview/xview.h>
#include <xview/panel.h>
#include <xview/cms.h>
#include <xview/group.h>
#include <xview/colorchsr.h>
#include <xview/svrimage.h>
#include <xview_private/svr_impl.h>

#ifndef lint
char colorchsr_c_sccsid[] = "@(#) %M% V%I% %E% %U% $Id: colorchsr.c,v 4.7 2025/06/04 20:01:50 dra Exp $";
#endif

#define IMAGE_WIDTH 64
#define IMAGE_HEIGHT 64

typedef struct {
	Color_chooser_HSV hsv;
	Color_chooser_RGB rgb;
} props_t;

#define OFF(_f_) FP_OFF(props_t *,_f_)

/*
 * 0 = WorkspaceColor		Desktop	Selected by user
 * 1 = WindowColor		BG1	Selected by user
 * 2 = IndentColor		BG2	90% of BG1
 * 3 = ShadowColor		BG3	50% of BG1
 * 4 = HighLightColor		White	120% of BG1
 * 5 = BackgroundColor		This is used in the preview window ttysw.
 * 6 = Black			Black
 * 7 = shadow in cms control segment
 * 8 = hilight in cms control segment
 */

#define WorkspaceIndex	0
#define BG1Index	1
#define BG2Index	2
#define BG3Index	3
#define WhiteIndex	4
#define BackIndex	5
#define BlackIndex	6

#define LastIndexPlusOne 7

/* Black has to be last since XView CMS_CONTROL_COLORS does not include
 * black, it expects the last color in a cms to be black... yuck!
 */

static char *image_string_2dframes = "\
7777777777777777777777777777777777777777777777777777777777777777\
7777777777777777777777777777777777777777777777777777777777777778\
7700000000000000000000000000000000000000000000000000000000000088\
7700000000000000000000000000000000000000000000000000000000000088\
7700000000000000000000000000000000000000000000000000000000000088\
7700000000000000000000000000000000000000000000000000000000000088\
7700000000000000000000000000000000000000000000000000000000000088\
7700000000000000000000000000000000000000000000000000000000000088\
7700000000000000000000000000000000000000000000000000000000000088\
7700000000000000000000000000000000000000000000000000000000000088\
7700000000000000000000000000000000000000000000000000000000000088\
7700000000000000000000000000000000000000000000000000000000000088\
7700000000000000000000000000000000000000000000000000000000000088\
7700000000000000000000000000000000000000000000000000000000000088\
7700000000000000000000000000000000000000000000000000000000000088\
7700000000000000000000000000000000000000000000000000000000000088\
7700000000000000000000000000000000000000000000000000000000000088\
7700000000444444444446666666666666666666666666666666666666666688\
7700000000411111111136666666666666666666666666666666666666666688\
7700000000411111111131111111111111111111111111111111111111111188\
7700000000411111111131111111111111111111111111111111111111111188\
7700000000411133333331111111111111111111111111111111111111111188\
7700000000411133333333332444444444444442333333333333333333333388\
7700000000411133222222224111111111111113222222222222222222222288\
7700000000411133222222224111111111111113222222222222222222222288\
7700000000411133222222224111111111111113222222222222222222222288\
7700000000411133222222224111666666611113222222222222222222222288\
7700000000433333222222224111622222411113222222222222222222222288\
7700000000661113222222224111162224111113222222222222222222222288\
7700000000661113222222224111162224111113222222222222222222222288\
7700000000661113222222224111116241111113222222222222222222222288\
7700000000661113222222224111116241111113222222222222222222222288\
7700000000661113222222224111111411111113222222222222222222222288\
7700000000661113222222224111111111111113222222222222222222222288\
7700000000661113222222224111111111111113222222222222222222222288\
7700000000661113222222224111111111111113222222222222222222222288\
7700000000661113222222222333333333333332222222222222222222222288\
7700000000661113444444444444444444444444444444444444444444444488\
7700000000661111111111111111111111111111111111111111111111111188\
7700000000661111111111111111111111111111111111111111111111111188\
7700000000661111111111111111111111111111111111111111111111111188\
7700000000661111111111111111111111111111111111111111111111111188\
7700000000661111111111111111111111111111111111111111111111111188\
7700000000661111144444444444443111666666666666666666666666666688\
7700000000661111141111111111113111655555555555555555555555555588\
7700000000661111141111111111113111655555555555555555555555555588\
7700000000661111141111111111113111655555555665555555555555555588\
7700000000661111141111111111113111655555556556556555555555555588\
7700000000661111143333333333333111655555556556565555555555555588\
7700000000661111111111111111111111655555555665655555555555555588\
7700000000661111111111111111111111655555555556555555555555555588\
7700000000661111111111113111111111655555555565665555555555555588\
7700000000661111111111131311111111655555555656556555555555555588\
7700000000661111111111113111111111655555556556556555555555555588\
7700000000661111111111131311111111655555555555665555555565555588\
7700000000661111111111113111111111655555555555555555555666555588\
7700000000661111111111131311111111655555555555555555555666555588\
7700000000661111111111113111111111655555555555555555556666655588\
7700000000661111111111131311111111655555555555555555556666655588\
7700000000661111111111113111111111655555555555555555566666665588\
7700000000661111111111131311111111655555555555555555566666665588\
7700000000661111111111113111111111655555555555555555555555555588\
7888888888888888888888888888888888888888888888888888888888888888\
8888888888888888888888888888888888888888888888888888888888888888\
";

#define	MAXRGB	0xff
#define	MAXH	360
#define	MAXSV	MAXRGB

#define VMUL		12		/* brighten by 20% (12 = 1.2*10) */
#define SDIV		2		/* unsaturate by 50% (divide by 2) */
#define VMIN		((4*MAXSV)/10)	/* highlight brightness 40% minimum */

#define NUM_RWCOLORS LastIndexPlusOne

/* red, green, blue, black */
#define PURE_COLORS 4

#define COLORMAP_SIZE (NUM_RWCOLORS + PURE_COLORS)

/* the colormap segment looks like:

	0
	..											the default control colors
	CMS_CONTROL_COLORS-1
	CMS_CONTROL_COLORS
	...                                         the read-write colors
	CMS_CONTROL_COLORS+NUM_RWCOLORS
	..  red   \
	..  green  \ the pure colors
	..  blue   /
	..  black /
	CMS_CONTROL_COLORS+COLORMAP_SIZE-1

*/

typedef struct {
	Xv_opaque       public_self;
	Color_chooser_changed_proc_t  changed_proc;
	Panel_item      textfield;
	Xv_opaque       client_data;
	props_t curprops;
	Cms cms;
	Server_image image;
	int image_width, image_height;
	Pixmap bitmap[LastIndexPlusOne];
	Panel_item p_image;
	Panel_item r_slider, g_slider, b_slider;
	Panel_item r_msg, g_msg, b_msg;
	Panel_item h_slider, s_slider, v_slider;
	Color_chooser_HSV xact_slider;
	int is_window_color;
	GC colorGC;
	int is_true_color;
	Color_chooser_color_converter cc;
} Colorchooser_private;

#define A0 *attrs
#define A1 attrs[1]
#define A2 attrs[2]
#define A3 attrs[3]
#define ADONE ATTR_CONSUME(*attrs);break

#define COLCHPRIV(_x_) XV_PRIVATE(Colorchooser_private, Xv_colorchsr, _x_)
#define COLCHPUB(_x_) XV_PUBLIC(_x_)

static int colch_key = 0;

static int max3(int x, int y, int z)
{
    if (y > x) x = y;
    if (z > x) x = z;
    return x;
}

static int min3(int x, int y, int z)
{
    if (y < x) x = y;
    if (z < x) x = z;
    return x;
}

void xv_HSV_to_RGB(Color_chooser_HSV *hsv, Color_chooser_RGB *rgb)
{
    int h = hsv->h;
    int s = hsv->s;
    int v = hsv->v;
    int r = 0, g = 0, b = 0;
    int i, f;
    int p, q, t;

    s = (s * MAXRGB) / MAXSV;
    v = (v * MAXRGB) / MAXSV;
    if (h == 360) h = 0;

	if (s == 0) {
		h = 0;
		r = g = b = v;
	}
    i = h / 60;
    f = h % 60;
    p = v * (MAXRGB - s) / MAXRGB;
    q = v * (MAXRGB - s * f / 60) / MAXRGB;
    t = v * (MAXRGB - s * (60 - f) / 60) / MAXRGB;

	switch (i) {
		case 0: r = v; g = t; b = p; break;
		case 1: r = q; g = v; b = p; break;
		case 2: r = p; g = v; b = t; break;
		case 3: r = p; g = q; b = v; break;
		case 4: r = t; g = p; b = v; break;
		case 5: r = v; g = p; b = q; break;
	}
    rgb->r = r;
    rgb->g = g;
    rgb->b = b;
}

void xv_RGB_to_HSV(Color_chooser_RGB *rgb, Color_chooser_HSV *hsv)
{
    int r = rgb->r;
    int g = rgb->g;
    int b = rgb->b;
    register int maxv = max3(r, g, b);
    register int minv = min3(r, g, b);
    int h = 0;
    int s;
    int v;

    v = maxv;

	if (maxv) {
		s = (maxv - minv) * MAXRGB / maxv;
	}
	else {
		s = 0;
	}

	if (s == 0) {
		h = 0;
	}
	else {
		int rc;
		int gc;
		int bc;
		int hex = 0;

		rc = (maxv - r) * MAXRGB / (maxv - minv);
		gc = (maxv - g) * MAXRGB / (maxv - minv);
		bc = (maxv - b) * MAXRGB / (maxv - minv);

		if (r == maxv) {
			h = bc - gc, hex = 0;
		}
		else if (g == maxv) {
			h = rc - bc, hex = 2;
		}
		else if (b == maxv) {
			h = gc - rc, hex = 4;
		}
		h = hex * 60 + (h * 60 / MAXRGB);
		if (h < 0)
			h += 360;
	}
    hsv->h = h;
    hsv->s = (s * MAXSV) / MAXRGB;
    hsv->v = (v * MAXSV) / MAXRGB;
}

/*
* Load an XColor with an Color_chooser_RGB.
*/
void xv_RGB_to_XColor(Color_chooser_RGB *r, XColor *x)
{
    x->red = (unsigned short) r->r << 8;
    x->green = (unsigned short) r->g << 8;
    x->blue = (unsigned short) r->b << 8;
    x->flags = DoRed | DoGreen | DoBlue;
}

/*
* Load an XColor with an Color_chooser_HSV.
*/
void xv_HSV_to_XColor(Color_chooser_HSV *h, XColor *x)
{
    Color_chooser_RGB r;

    xv_HSV_to_RGB(h, &r);
    xv_RGB_to_XColor(&r, x);
}

/*
* Load an Color_chooser_HSV with an XColor.
*/
void xv_XColor_to_HSV(XColor *x, Color_chooser_HSV *h)
{
    Color_chooser_RGB r;

    r.r = (int) x->red >> 8;
    r.g = (int) x->green >> 8;
    r.b = (int) x->blue >> 8;
    xv_RGB_to_HSV(&r, h);
}

/*
* Take an Color_chooser_HSV and generate the 3 OpenLook 3D colors
* into XColor structures.
*/
void xv_color_chooser_olgx_hsv_to_3D(Color_chooser_HSV *bg1,
    			XColor *bg2, XColor *bg3, XColor *white)
{
    Color_chooser_HSV hsv;
    int h = bg1->h;
    int s = bg1->s;
    int v = bg1->v;

    v = (v * VMUL) / 10;
	if (v > MAXSV) {
		s /= SDIV;
		v = MAXSV;
	}
    if (v < VMIN)
	v = VMIN;

    hsv.h = h;
    hsv.s = s;
    hsv.v = v;
    xv_HSV_to_XColor(&hsv, white);

    hsv.h = bg1->h;
    hsv.s = bg1->s;
    hsv.v = (bg1->v * 9) / 10;	/* 90% */
    xv_HSV_to_XColor(&hsv, bg2);

    hsv.h = bg1->h;
    hsv.s = bg1->s;
    hsv.v = bg1->v >> 1;	/* 50% */
    xv_HSV_to_XColor(&hsv, bg3);
}

;
unsigned long xv_XColor_to_Pixel(Color_chooser_color_converter *cc, XColor *col)
{
	return (((col->red >> cc->red_width) << cc->red_shift) |
			((col->green >> cc->green_width) << cc->green_shift) |
			((col->blue >> cc->blue_width) << cc->blue_shift));
}

static void draw_preview_image(Colorchooser_private *priv, XColor *colors)
{
	int i;
	Xv_opaque self = COLCHPUB(priv);
	Pixmap image = (Pixmap)xv_get(priv->image, XV_XID);
	Display *dsp = (Display *)xv_get(self, XV_DISPLAY);

	if (priv->is_window_color) {
		for (i = 0; i < NUM_RWCOLORS; i++) {
			/***********************************************
			sehr langsam, mal schaun, obs schneller wird
			XAllocColor(dsp, cm, colors + i);
			fprintf(stderr, "%02x %02x %02x : %06x\n",
					colors[i].red, colors[i].green, colors[i].blue,
					colors[i].pixel);
			da kommt : 5151 5252 6f6f : 51526f
			*************************************************/
			XSetForeground(dsp, priv->colorGC,
							xv_XColor_to_Pixel(&priv->cc, colors + i));

			XSetClipMask(dsp, priv->colorGC, priv->bitmap[i]);
			XFillRectangle(dsp, image, priv->colorGC, 0, 0,
							(unsigned)priv->image_width,
							(unsigned)priv->image_height);
		}
	}
	else {
		XSetForeground(dsp, priv->colorGC,
						xv_XColor_to_Pixel(&priv->cc, colors + BG1Index));
		XFillRectangle(dsp, image, priv->colorGC, 0, 0,
							(unsigned)priv->image_width,
							(unsigned)priv->image_height);
	}

	xv_set(priv->p_image, PANEL_PAINT, PANEL_NO_CLEAR, NULL);
}

static void update_message(Panel_item mess, int val)
{
	char buf[10];

	sprintf(buf, "%3d", val);
	xv_set(mess, PANEL_LABEL_STRING, buf, NULL);
	sprintf(buf, "#%x", val);
	xv_set(xv_get(mess, PANEL_CLIENT_DATA), PANEL_LABEL_STRING, buf, NULL);
}

static void update_colors(Colorchooser_private *priv)
{
	XColor xxcolors[NUM_RWCOLORS];

	xv_HSV_to_XColor(&priv->xact_slider, &xxcolors[BG1Index]);
	xv_color_chooser_olgx_hsv_to_3D(&priv->xact_slider, &xxcolors[BG2Index],
								&xxcolors[BG3Index], &xxcolors[WhiteIndex]);

	xxcolors[WorkspaceIndex].red = 0x8080;
	xxcolors[WorkspaceIndex].green = 0x8080;
	xxcolors[WorkspaceIndex].blue = 0x8080;
	xxcolors[WorkspaceIndex].flags = DoRed | DoGreen | DoBlue;

	xxcolors[BackIndex].red = 0xffff;
	xxcolors[BackIndex].green = 0xffff;
	xxcolors[BackIndex].blue = 0xffff;
	xxcolors[BackIndex].flags = DoRed | DoGreen | DoBlue;

	xxcolors[BlackIndex].red = 0;
	xxcolors[BlackIndex].green = 0;
	xxcolors[BlackIndex].blue = 0;
	xxcolors[BlackIndex].flags = DoRed | DoGreen | DoBlue;

	if (priv->is_true_color) {
		draw_preview_image(priv, xxcolors);
	}
	else {
		xv_set(priv->cms,
				CMS_COLOR_COUNT, NUM_RWCOLORS,
				CMS_INDEX, CMS_CONTROL_COLORS,
				CMS_X_COLORS, xxcolors,
				NULL);
	}

	update_message(priv->r_msg, xxcolors[BG1Index].red >> 8);
	update_message(priv->g_msg, xxcolors[BG1Index].green >> 8);
	update_message(priv->b_msg, xxcolors[BG1Index].blue >> 8);
}

static int note_apply(Color_chooser self, int is_triggered)
{
	Colorchooser_private *priv = COLCHPRIV(self);

	update_colors(priv);

	if (priv->changed_proc) {
		(*(priv->changed_proc))(self, priv->curprops.rgb.r,
							priv->curprops.rgb.g, priv->curprops.rgb.b);
	}
	if (priv->textfield) {
		char tt[30];

		sprintf(tt, "%d %d %d", priv->curprops.rgb.r,
							priv->curprops.rgb.g, priv->curprops.rgb.b);
		xv_set(priv->textfield, PANEL_VALUE, tt, NULL);
	}
	return XV_OK;
}

static int note_reset(Color_chooser self, int is_triggered)
{
	Colorchooser_private *priv = COLCHPRIV(self);

	if (priv->textfield) {
		int r, g, b, matches;
		char *tfval = (char *)xv_get(priv->textfield, PANEL_VALUE);

		matches = sscanf(tfval, "%d%d%d", &r, &g, &b);
		if (matches < 3) {
			r = g = b = 0;
		}
		else {
			if (r < 0) r = 0;
			if (r > 255) r = 255;
			if (g < 0) g = 0;
			if (g > 255) g = 255;
			if (b < 0) b = 0;
			if (b > 255) b = 255;
		}

		priv->curprops.rgb.r = r;
		priv->curprops.rgb.g = g;
		priv->curprops.rgb.b = b;
		xv_RGB_to_HSV(&priv->curprops.rgb, &priv->curprops.hsv);

		xv_set(XV_SERVER_FROM_WINDOW(self),
				SERVER_SYNC_AND_PROCESS_EVENTS,
				NULL);
	}
	priv->xact_slider = priv->curprops.hsv;

	update_colors(priv);
	update_message(priv->r_msg, priv->curprops.rgb.r);
	update_message(priv->g_msg, priv->curprops.rgb.g);
	update_message(priv->b_msg, priv->curprops.rgb.b);

	return XV_OK;
}

static void note_done(Color_chooser self)
{
	xv_set(self, XV_SHOW, FALSE, NULL);
}

static void hsv_slider_notify(Panel_item item, int value, Event *event)
{
	Colorchooser_private *priv = (Colorchooser_private *)xv_get(item,
											XV_KEY_DATA, colch_key);
	XColor xcolor;

	switch (xv_get(item, PANEL_CLIENT_DATA)) {
		case 1:
			priv->xact_slider.h = value;
			break;
		case 2:
			priv->xact_slider.s = value;
			break;
		case 3:
			priv->xact_slider.v = value;
			break;
	}

	update_colors(priv);
	xv_HSV_to_XColor(&priv->xact_slider, &xcolor);

	xv_set(priv->r_slider, PANEL_VALUE, xcolor.red >> 8, NULL);
	xv_set(priv->g_slider, PANEL_VALUE, xcolor.green >> 8, NULL);
	xv_set(priv->b_slider, PANEL_VALUE, xcolor.blue >> 8, NULL);

	update_message(priv->r_msg, xcolor.red >> 8);
	update_message(priv->g_msg, xcolor.green >> 8);
	update_message(priv->b_msg, xcolor.blue >> 8);
}

static void rgb_slider_notify(Panel_item slider, int value, Event *ev)
{
	Colorchooser_private *priv = (Colorchooser_private *)xv_get(slider,
											XV_KEY_DATA, colch_key);
	XColor curcol;

	switch (xv_get(slider, PANEL_CLIENT_DATA)) {
		case 1:
			curcol.red = value << 8;
			curcol.green = (int)xv_get(priv->g_slider, PANEL_VALUE) << 8;
			curcol.blue = (int)xv_get(priv->b_slider, PANEL_VALUE) << 8;
			update_message(priv->r_msg, value);
			break;
		case 2:
			curcol.green = value << 8;
			curcol.red = (int)xv_get(priv->r_slider, PANEL_VALUE) << 8;
			curcol.blue = (int)xv_get(priv->b_slider, PANEL_VALUE) << 8;
			update_message(priv->g_msg, value);
			break;
		case 3:
			curcol.blue = value << 8;
			curcol.red = (int)xv_get(priv->r_slider, PANEL_VALUE) << 8;
			curcol.green = (int)xv_get(priv->g_slider, PANEL_VALUE) << 8;
			update_message(priv->b_msg, value);
			break;
	}

	curcol.flags = DoRed | DoGreen | DoBlue;
	xv_XColor_to_HSV(&curcol, &priv->xact_slider);

	xv_set(priv->h_slider, PANEL_VALUE, priv->xact_slider.h, NULL);
	xv_set(priv->s_slider, PANEL_VALUE, priv->xact_slider.s, NULL);
	xv_set(priv->v_slider, PANEL_VALUE, priv->xact_slider.v, NULL);
	update_colors(priv);
}

static Cms create_palette(Colorchooser_private *priv)
{
	XColor mycolors[COLORMAP_SIZE];

	mycolors[NUM_RWCOLORS].red = 0xff00;
	mycolors[NUM_RWCOLORS].green = 0;
	mycolors[NUM_RWCOLORS].blue = 0;
	mycolors[NUM_RWCOLORS].flags = DoRed | DoGreen | DoBlue;

	mycolors[NUM_RWCOLORS+1].red = 0;
	mycolors[NUM_RWCOLORS+1].green = 0xff00;
	mycolors[NUM_RWCOLORS+1].blue = 0;
	mycolors[NUM_RWCOLORS+1].flags = DoRed | DoGreen | DoBlue;

	mycolors[NUM_RWCOLORS+2].red = 0;
	mycolors[NUM_RWCOLORS+2].green = 0;
	mycolors[NUM_RWCOLORS+2].blue = 0xff00;
	mycolors[NUM_RWCOLORS+2].flags = DoRed | DoGreen | DoBlue;

	/* this will be the FOREGROUND_COLOR : black */
	mycolors[NUM_RWCOLORS+3].red = 0;
	mycolors[NUM_RWCOLORS+3].green = 0;
	mycolors[NUM_RWCOLORS+3].blue = 0;
	mycolors[NUM_RWCOLORS+3].flags = DoRed | DoGreen | DoBlue;

	priv->colorGC = XCreateGC((Display *)xv_get(COLCHPUB(priv), XV_DISPLAY),
						(Window)xv_get(xv_get(COLCHPUB(priv), XV_ROOT), XV_XID),
						0L, 0);

	if (priv->is_true_color) {
		return xv_create(XV_SCREEN_FROM_WINDOW(COLCHPUB(priv)), CMS,
				XV_VISUAL, xv_get(COLCHPUB(priv), XV_VISUAL),
				CMS_CONTROL_CMS, TRUE,
				CMS_SIZE, CMS_CONTROL_COLORS + COLORMAP_SIZE,
				CMS_X_COLORS, mycolors,
				NULL);
	}
	else {
		return xv_create(XV_SCREEN_FROM_WINDOW(COLCHPUB(priv)), CMS,
				XV_VISUAL, xv_get(COLCHPUB(priv), XV_VISUAL),
				CMS_TYPE, XV_DYNAMIC_CMS,
				CMS_CONTROL_CMS, TRUE,
				CMS_SIZE, CMS_CONTROL_COLORS + COLORMAP_SIZE,
				CMS_X_COLORS, mycolors,
				NULL);
	}
}

static char *make_help(Colorchooser_private *priv, char *str)
{
	char *hf, *myhelp, *itemhelp, helpbuf[100];

	myhelp = (char *)xv_get(COLCHPUB(priv), XV_HELP_DATA);
	if (myhelp) {
		itemhelp = xv_malloc(strlen(myhelp) + strlen(str) + 2);
		sprintf(itemhelp, "%s_%s", myhelp, str);
		return itemhelp;
	}

	hf = (char *)xv_get(XV_SERVER_FROM_WINDOW(COLCHPUB(priv)),XV_APP_HELP_FILE);
	if (! hf) hf = "xview";

	sprintf(helpbuf, "%s:colorchooser_%s", hf, str);

	return xv_strsave(helpbuf);
}

static void free_help_data(Xv_opaque obj, int key, char *data)
{
	if (key == XV_HELP && data) xv_free(data);
}

static void fill_me(Color_chooser self)
{
	Colorchooser_private *priv = COLCHPRIV(self);
	Panel pan;
	Panel_item r_msg, g_msg, b_msg;
	Rect *r;
	char image_data[IMAGE_WIDTH * IMAGE_HEIGHT];
/* 	int item_top; */
	char *h1, *h2;
	Panel_item preview;
	Group g;

	if (! priv->cms) priv->cms = create_palette(priv);
	pan = xv_get(self, FRAME_PROPS_PANEL);
	xv_set(pan,
			XV_HELP_DATA, make_help(priv,"panel"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
			NULL);

	memset(image_data, CMS_CONTROL_COLORS, sizeof(image_data));

	if (priv->is_window_color) {
		if (priv->is_true_color) {
			int i, j, k;
			unsigned char imadata[LastIndexPlusOne][IMAGE_WIDTH*IMAGE_HEIGHT/8];
			unsigned long shadow, light;
			Pixmap pix;
			Display *dsp = (Display *)xv_get(self, XV_DISPLAY);

			shadow = xv_get(priv->cms, CMS_PIXEL, BG2Index);
			light = xv_get(priv->cms, CMS_PIXEL, BG3Index);

			priv->image = xv_create(XV_SCREEN_FROM_WINDOW(self), SERVER_IMAGE,
								SERVER_IMAGE_CMS, priv->cms,
								XV_WIDTH, priv->image_width,
								XV_HEIGHT, priv->image_height,
								SERVER_IMAGE_DEPTH, (int)xv_get(self, XV_DEPTH),
								NULL);

			pix = (Pixmap)xv_get(priv->image, SERVER_IMAGE_PIXMAP);

			for (i = 0; i < priv->image_width * priv->image_height; i++) {
				j = image_string_2dframes[i] - '0';

				for (k = 0; k < LastIndexPlusOne; k++) {
					imadata[k][i / 8] >>= 1;
				}

				/* less than 7 is one of my colors, 7 & 8 are CMS_CONTROL colors */
				if (j >= LastIndexPlusOne) {
					switch (j) {
						case 7: 
							XSetForeground(dsp, priv->colorGC, shadow);
							break;
						case 8:
							XSetForeground(dsp, priv->colorGC, light);
							break;
					}
					XDrawPoint(dsp, pix, priv->colorGC,
								i % priv->image_width, i / priv->image_width);
				}
				else {
					for (k = 0; k < LastIndexPlusOne; k++) {
						if (k == j) imadata[k][i / 8] |= 0x80;
					}
				}
			}

			for (k = 0; k < LastIndexPlusOne; k++) {
				priv->bitmap[k] = XCreateBitmapFromData(dsp,
								(Window)xv_get(xv_get(self, XV_ROOT), XV_XID),
								(char *)imadata[k],
								(unsigned)priv->image_width,
								(unsigned)priv->image_height);
			}
		}
		else {
			int i, j;
			char image_data[IMAGE_WIDTH * IMAGE_HEIGHT];

			for (i = 0; i < priv->image_width * priv->image_height; i++) {
				j = image_string_2dframes[i] - '0';
				/* less than 7 is one of my colors, 7 & 8 are CMS_CONTROL colors */
				switch (j) {
					case 7:
						j = 2;	/* shadow color */
						break;
					case 8:
						j = 3;	/* hilight color */
						break;
					default:
						j += CMS_CONTROL_COLORS /* + COLOR_CHOICES */ ;
						break;
				}
				image_data[i] = j;
			}
			priv->image = xv_create(XV_SCREEN_FROM_WINDOW(self), SERVER_IMAGE,
					SERVER_IMAGE_CMS, priv->cms,
					XV_WIDTH, priv->image_width,
					XV_HEIGHT, priv->image_height,
					SERVER_IMAGE_DEPTH, (int)xv_get(self, XV_DEPTH),
					SERVER_IMAGE_X_BITS, image_data,
					NULL);
		}
	}
	else {
		int k;

		priv->image = xv_create(XV_SCREEN_FROM_WINDOW(self), SERVER_IMAGE,
				SERVER_IMAGE_CMS, priv->cms,
				XV_WIDTH, priv->image_width,
				XV_HEIGHT, priv->image_height,
				SERVER_IMAGE_DEPTH, (int)xv_get(self, XV_DEPTH),
				SERVER_IMAGE_X_BITS, image_data,
				NULL);
		for (k = 0; k < LastIndexPlusOne; k++) {
			priv->bitmap[k] = 0;
		}
	}

	xv_set(self,
			FRAME_PROPS_CREATE_ITEM,
				FRAME_PROPS_ITEM_SPEC, &preview, -1, PANEL_MESSAGE,
				PANEL_LABEL_BOLD, TRUE,
				PANEL_LABEL_STRING, XV_MSG("Preview:"),
				XV_HELP_DATA, make_help(priv,"preview"),
				XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
				NULL,
			FRAME_PROPS_CREATE_ITEM,
				FRAME_PROPS_ITEM_SPEC,
					&priv->p_image, FRAME_PROPS_MOVE, PANEL_MESSAGE,
				PANEL_LABEL_IMAGE, priv->image,
				PANEL_ITEM_COLOR, CMS_CONTROL_COLORS,
				XV_HELP_DATA, make_help(priv,"preview"),
				XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
				NULL,
			FRAME_PROPS_CREATE_ITEM,
				FRAME_PROPS_ITEM_SPEC, &priv->h_slider, -1, PANEL_SLIDER,
				PANEL_LABEL_STRING, XV_MSG("Hue:"),
				PANEL_CLIENT_DATA, 1,
				XV_KEY_DATA, colch_key, priv,
				PANEL_SHOW_RANGE, FALSE,
				PANEL_SHOW_VALUE, FALSE,
				PANEL_VALUE, 0,
				PANEL_MIN_VALUE, 0,
				PANEL_MAX_VALUE, MAXH,
				PANEL_NOTIFY_LEVEL, PANEL_ALL,
				PANEL_NOTIFY_PROC, hsv_slider_notify,
				FRAME_PROPS_DATA_OFFSET, OFF(hsv.h),
				XV_HELP_DATA, make_help(priv,"HueSlider"),
				XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
				NULL,
			FRAME_PROPS_CREATE_ITEM,
				FRAME_PROPS_ITEM_SPEC, &priv->s_slider, -1, PANEL_SLIDER,
				PANEL_LABEL_STRING, XV_MSG("Saturation:"),
				PANEL_CLIENT_DATA, 2,
				XV_KEY_DATA, colch_key, priv,
				PANEL_SHOW_RANGE, FALSE,
				PANEL_SHOW_VALUE, FALSE,
				PANEL_VALUE, 0,
				PANEL_MIN_VALUE, 0,
				PANEL_MAX_VALUE, MAXSV,
				PANEL_NOTIFY_LEVEL, PANEL_ALL,
				PANEL_NOTIFY_PROC, hsv_slider_notify,
				FRAME_PROPS_DATA_OFFSET, OFF(hsv.s),
				XV_HELP_DATA, make_help(priv,"SaturationSlider"),
				XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
				NULL,
			FRAME_PROPS_CREATE_ITEM,
				FRAME_PROPS_ITEM_SPEC, &priv->v_slider, -1, PANEL_SLIDER,
				PANEL_LABEL_STRING, XV_MSG("Brightness:"),
				PANEL_CLIENT_DATA, 3,
				XV_KEY_DATA, colch_key, priv,
				PANEL_SHOW_RANGE, FALSE,
				PANEL_SHOW_VALUE, FALSE,
				PANEL_VALUE, 0,
				PANEL_MIN_VALUE, 0,
				PANEL_MAX_VALUE, MAXSV,
				PANEL_NOTIFY_LEVEL, PANEL_ALL,
				PANEL_NOTIFY_PROC, hsv_slider_notify,
				FRAME_PROPS_DATA_OFFSET, OFF(hsv.v),
				XV_HELP_DATA, make_help(priv,"BrightnessSlider"),
				XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
				NULL,
			FRAME_PROPS_ALIGN_ITEMS,
			NULL);

	xv_set(self,
			FRAME_PROPS_CREATE_ITEM,
				FRAME_PROPS_ITEM_SPEC,
					&priv->r_slider, FRAME_PROPS_NO_LAYOUT, PANEL_SLIDER,
				PANEL_DIRECTION, PANEL_VERTICAL,
				PANEL_MIN_VALUE, 0,
				PANEL_MAX_VALUE, 255,
				PANEL_ITEM_COLOR, CMS_CONTROL_COLORS+NUM_RWCOLORS,
				PANEL_SHOW_RANGE, FALSE,
				PANEL_SHOW_VALUE, FALSE,
				PANEL_NOTIFY_LEVEL, PANEL_ALL,
				PANEL_NOTIFY_PROC, rgb_slider_notify,
				PANEL_CLIENT_DATA, 1,
				XV_KEY_DATA, colch_key, priv,
				XV_HELP_DATA, make_help(priv,"RSlider"),
				XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
				FRAME_PROPS_DATA_OFFSET, OFF(rgb.r),
				NULL,
			NULL);

	xv_set(self,
			FRAME_PROPS_CREATE_ITEM,
				FRAME_PROPS_ITEM_SPEC,
					&priv->r_msg, FRAME_PROPS_NO_LAYOUT, PANEL_MESSAGE,
				PANEL_LABEL_STRING, "255",
				XV_HELP_DATA, make_help(priv,"RSlider"),
				XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
				NULL,
			FRAME_PROPS_CREATE_ITEM,
				FRAME_PROPS_ITEM_SPEC,
					&r_msg, FRAME_PROPS_NO_LAYOUT, PANEL_MESSAGE,
				PANEL_LABEL_STRING, "255",
				XV_HELP_DATA, make_help(priv,"RSlider"),
				XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
				NULL,
			FRAME_PROPS_CREATE_ITEM,
				FRAME_PROPS_ITEM_SPEC,
					&priv->g_slider, FRAME_PROPS_NO_LAYOUT, PANEL_SLIDER,
				PANEL_DIRECTION, PANEL_VERTICAL,
				PANEL_MIN_VALUE, 0,
				PANEL_MAX_VALUE, 255,
				PANEL_ITEM_COLOR, CMS_CONTROL_COLORS+NUM_RWCOLORS+1,
				PANEL_SHOW_RANGE, FALSE,
				PANEL_SHOW_VALUE, FALSE,
				PANEL_NOTIFY_LEVEL, PANEL_ALL,
				PANEL_NOTIFY_PROC, rgb_slider_notify,
				FRAME_PROPS_DATA_OFFSET, OFF(rgb.g),
				PANEL_CLIENT_DATA, 2,
				XV_KEY_DATA, colch_key, priv,
				XV_HELP_DATA, make_help(priv,"GSlider"),
				XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
				NULL,
			NULL);

	xv_set(self,
			FRAME_PROPS_CREATE_ITEM,
				FRAME_PROPS_ITEM_SPEC,
					&priv->g_msg, FRAME_PROPS_NO_LAYOUT, PANEL_MESSAGE,
				PANEL_LABEL_STRING, "255",
				XV_HELP_DATA, make_help(priv,"GSlider"),
				XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
				NULL,
			FRAME_PROPS_CREATE_ITEM,
				FRAME_PROPS_ITEM_SPEC,
					&g_msg, FRAME_PROPS_NO_LAYOUT, PANEL_MESSAGE,
				PANEL_LABEL_STRING, "255",
				XV_HELP_DATA, make_help(priv,"GSlider"),
				XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
				NULL,
			FRAME_PROPS_CREATE_ITEM,
				FRAME_PROPS_ITEM_SPEC,
					&priv->b_slider, FRAME_PROPS_NO_LAYOUT, PANEL_SLIDER,
				PANEL_DIRECTION, PANEL_VERTICAL,
				PANEL_MIN_VALUE, 0,
				PANEL_MAX_VALUE, 255,
				PANEL_ITEM_COLOR, CMS_CONTROL_COLORS+NUM_RWCOLORS+2,
				PANEL_SHOW_RANGE, FALSE,
				PANEL_SHOW_VALUE, FALSE,
				PANEL_NOTIFY_LEVEL, PANEL_ALL,
				PANEL_NOTIFY_PROC, rgb_slider_notify,
				FRAME_PROPS_DATA_OFFSET, OFF(rgb.b),
				PANEL_CLIENT_DATA, 3,
				XV_KEY_DATA, colch_key, priv,
				XV_HELP_DATA, make_help(priv,"BSlider"),
				XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
				NULL,
			NULL);

	xv_set(self,
			FRAME_PROPS_CREATE_ITEM,
				FRAME_PROPS_ITEM_SPEC,
					&priv->b_msg, FRAME_PROPS_NO_LAYOUT, PANEL_MESSAGE,
				PANEL_LABEL_STRING, "255",
				XV_HELP_DATA, make_help(priv,"BSlider"),
				XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
				NULL,
			FRAME_PROPS_CREATE_ITEM,
				FRAME_PROPS_ITEM_SPEC,
					&b_msg, FRAME_PROPS_NO_LAYOUT, PANEL_MESSAGE,
				PANEL_LABEL_STRING, "255",
				XV_HELP_DATA, make_help(priv,"BSlider"),
				XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
				NULL,
			NULL);

	xv_set(priv->r_msg, PANEL_CLIENT_DATA, r_msg, NULL);
	xv_set(priv->g_msg, PANEL_CLIENT_DATA, g_msg, NULL);
	xv_set(priv->b_msg, PANEL_CLIENT_DATA, b_msg, NULL);

	r = (Rect *)xv_get(priv->h_slider, PANEL_ITEM_VALUE_RECT);
	g = xv_create(pan, GROUP,
			GROUP_TYPE, GROUP_ROW,
			GROUP_HORIZONTAL_SPACING, 10,
			GROUP_ANCHOR_OBJ, preview,
			GROUP_VERTICAL_OFFSET, 0,
			GROUP_HORIZONTAL_OFFSET, 30 + r->r_width,
			GROUP_ANCHOR_POINT, GROUP_NORTHEAST,
			GROUP_MEMBERS,
				xv_create(pan, GROUP,
						GROUP_TYPE, GROUP_COLUMN,
						GROUP_COLUMN_ALIGNMENT, GROUP_VERTICAL_CENTERS,
						GROUP_VERTICAL_SPACING, 4,
						GROUP_MEMBERS,
							priv->r_slider,
							priv->r_msg,
							r_msg,
							NULL,
						NULL),
				xv_create(pan, GROUP,
						GROUP_TYPE, GROUP_COLUMN,
						GROUP_COLUMN_ALIGNMENT, GROUP_VERTICAL_CENTERS,
						GROUP_VERTICAL_SPACING, 4,
						GROUP_MEMBERS,
							priv->g_slider,
							priv->g_msg,
							g_msg,
							NULL,
						NULL),
				xv_create(pan, GROUP,
						GROUP_TYPE, GROUP_COLUMN,
						GROUP_COLUMN_ALIGNMENT, GROUP_VERTICAL_CENTERS,
						GROUP_VERTICAL_SPACING, 4,
						GROUP_MEMBERS,
							priv->b_slider,
							priv->b_msg,
							b_msg,
							NULL,
						NULL),
				NULL,
			NULL);
	group_anchor(g);
	update_colors(priv);
	window_fit_width(pan);
	xv_set(pan, XV_WIDTH, 8 + (int)xv_get(pan, XV_WIDTH), NULL);

	xv_set(self,
				FRAME_PROPS_CREATE_BUTTONS,
					FRAME_PROPS_APPLY, h1 = make_help(priv,"props_apply"),
					FRAME_PROPS_RESET, h2 = make_help(priv,"props_reset"),
					NULL,
				NULL);
	xv_set(XV_SERVER_FROM_WINDOW(self), SERVER_SYNC_AND_PROCESS_EVENTS, NULL);
	xv_free(h1);
	xv_free(h2);
}

static int colorchsr_init(Xv_window owner, Xv_opaque slf,
			Attr_avlist avlist, int *unused)
{
	Xv_colorchsr *self = (Xv_colorchsr *)slf;
	Colorchooser_private *priv = (Colorchooser_private *)xv_alloc(Colorchooser_private);

	if (! colch_key) colch_key = xv_unique_key();
	if (!priv) return XV_ERROR;

	priv->public_self = (Xv_opaque)self;
	self->private_data = (Xv_opaque)priv;
	priv->image_width = 32;
	priv->image_height = 32;
	priv->textfield = XV_NULL;

	xv_set((Xv_opaque)self, 
			FRAME_PROPS_APPLY_PROC, note_apply,
			FRAME_PROPS_RESET_PROC, note_reset,
			FRAME_PROPS_DATA_ADDRESS, &priv->curprops,
			FRAME_SHOW_FOOTER, FALSE,
			/* wichtig wegen frame_init.c:58 */
	  		FRAME_DONE_PROC, note_done,
			NULL);

	priv->is_true_color =
			((int)xv_get((Xv_opaque)self, XV_VISUAL_CLASS) == TrueColor);

	return XV_OK;
}

void xv_color_choser_initialize_color_conversion(Frame fram,
									Color_chooser_color_converter *cc)
{
	XColor col;
	int depth = (int)xv_get(fram, XV_DEPTH);
	Visual *vi = (Visual *)xv_get(fram, XV_VISUAL);
	unsigned long pix;

	pix = vi->red_mask;
	cc->red_shift = 0;
	while ((pix & 1) == 0) {
		++cc->red_shift;
		pix >>= 1;
	}

	pix = vi->green_mask;
	cc->green_shift = 0;
	while ((pix & 1) == 0) {
		++cc->green_shift;
		pix >>= 1;
	}

	pix = vi->blue_mask;
	cc->blue_shift = 0;
	while ((pix & 1) == 0) {
		++cc->blue_shift;
		pix >>= 1;
	}

	/* determine blue_width: */
	cc->blue_width = depth;
	if (cc->red_shift > cc->blue_shift && cc->red_shift < cc->blue_width)
		cc->blue_width = cc->red_shift;
	if (cc->green_shift > cc->blue_shift && cc->green_shift < cc->blue_width)
		cc->blue_width = cc->green_shift;

	cc->blue_width -= cc->blue_shift;
	cc->blue_width = 8 * sizeof(col.blue) - cc->blue_width;

	/* determine green_width: */
	cc->green_width = depth;
	if (cc->red_shift > cc->green_shift && cc->red_shift < cc->green_width)
		cc->green_width = cc->red_shift;
	if (cc->blue_shift > cc->green_shift && cc->blue_shift < cc->green_width)
		cc->green_width = cc->blue_shift;

	cc->green_width -= cc->green_shift;
	cc->green_width = 8 * sizeof(col.green) - cc->green_width;

	/* determine red_width: */
	cc->red_width = depth;
	if (cc->green_shift > cc->red_shift && cc->green_shift < cc->red_width)
		cc->red_width = cc->green_shift;
	if (cc->blue_shift > cc->red_shift && cc->blue_shift < cc->red_width)
		cc->red_width = cc->blue_shift;

	cc->red_width -= cc->red_shift;
	cc->red_width = 8 * sizeof(col.red) - cc->red_width;
}

static Xv_opaque colorchsr_set(Color_chooser self, Attr_avlist avlist)
{
	Attr_attribute *attrs;
	Colorchooser_private *priv = COLCHPRIV(self);

	for (attrs=avlist; *attrs; attrs=attr_next(attrs)) switch ((int)*attrs) {
		case COLORCHOOSER_COLOR:
			priv->curprops.rgb.r = (int)A1;
			priv->curprops.rgb.g = (int)A2;
			priv->curprops.rgb.b = (int)A3;
			xv_RGB_to_HSV(&priv->curprops.rgb, &priv->curprops.hsv);
			ADONE;

		case COLORCHOOSER_CHANGED_PROC:
			priv->changed_proc = (Color_chooser_changed_proc_t)A1;
			ADONE;

		case COLORCHOOSER_TEXTFIELD:
			priv->textfield = A1;
			ADONE;

		case COLORCHOOSER_CLIENT_DATA:
			priv->client_data = A1;
			ADONE;

		case COLORCHOOSER_IS_WINDOW_COLOR:
			priv->is_window_color = (int)A1;
			if (priv->is_window_color) {
				priv->image_width = IMAGE_WIDTH;
				priv->image_height = IMAGE_HEIGHT;
			}
			ADONE;

		case XV_END_CREATE:
			if (! priv->cms) priv->cms = create_palette(priv);
			xv_set(self, WIN_CMS, priv->cms, NULL);
			xv_set(xv_get(self, FRAME_PROPS_PANEL), WIN_CMS, priv->cms, NULL);
			if (priv->is_true_color) {
				xv_color_choser_initialize_color_conversion(self, &priv->cc);
			}
			/* sofort fuellen, damit die Groesse richtig ist -
			 * siehe colortext.c`ds_force_popup_on_screen
			 */
			fill_me(self);
			break;

		default: xv_check_bad_attr(COLOR_CHOOSER, A0);
			break;
	}

	return XV_OK;
}

/*ARGSUSED*/
static Xv_opaque colorchsr_get(Color_chooser self, int *status, Attr_attribute attr, va_list vali)
{
	Colorchooser_private *priv = COLCHPRIV(self);

	*status = XV_OK;
	switch ((int)attr) {
		case COLORCHOOSER_CHANGED_PROC: return (Xv_opaque)priv->changed_proc;
		case COLORCHOOSER_TEXTFIELD: return (Xv_opaque)priv->textfield;
		case COLORCHOOSER_CLIENT_DATA: return (Xv_opaque)priv->client_data;
		case COLORCHOOSER_IS_WINDOW_COLOR: return (Xv_opaque)priv->is_window_color;
		default:
			*status = xv_check_bad_attr(COLOR_CHOOSER, attr);
			return (Xv_opaque)XV_OK;
	}
}

static int colorchsr_destroy(Color_chooser self, Destroy_status status)
{
	if (status == DESTROY_CLEANUP) {
		Colorchooser_private *priv = COLCHPRIV(self);

		XFreeGC((Display *)xv_get(self, XV_DISPLAY), priv->colorGC);
		if (priv->image) xv_destroy(priv->image);
		xv_free(priv);
	}
	return XV_OK;
}

void xv_color_chooser_convert_color(char *val, int panel_to_data,
				Color_chooser_RGB*datptr, Panel_item it, Xv_opaque unused)
{
	if (panel_to_data) {
		int numvals;

		numvals = sscanf(val, "%d%d%d", &datptr->r,
										&datptr->g,
										&datptr->b);
		if (numvals != 3) {
			datptr->r = datptr->g = datptr->b = 0;
		}
	}
	else {
		char buf[20];

		sprintf(buf, "%d %d %d", datptr->r, datptr->g, datptr->b);
		xv_set(it, PANEL_VALUE, buf, NULL);
	}
}

const Xv_pkg xv_colorchsr_pkg = {
	"ColorChooser",
	ATTR_PKG_COLOR_CHOOSER,
	sizeof(Xv_colorchsr),
	FRAME_PROPS,
	colorchsr_init,
	colorchsr_set,
	colorchsr_get,
	colorchsr_destroy,
	0
};
