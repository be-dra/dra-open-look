#ifndef _colorchsr_h_INCLUDED
#define _colorchsr_h_INCLUDED 1

/*
 * "@(#) %M% V%I% %E% %U% $Id: colorchsr.h,v 4.3 2025/03/08 13:37:48 dra Exp $"
 *
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

#include <xview/frame.h>
#include <xview/attrol.h>

extern const Xv_pkg xv_colorchsr_pkg;
#define COLOR_CHOOSER &xv_colorchsr_pkg
typedef Xv_opaque Color_chooser;

typedef struct {
	Xv_propframe   parent_data;
    Xv_opaque      private_data;
} Xv_colorchsr;

#define	COLCH_ATTR(_t, _o) ATTR(ATTR_PKG_COLOR_CHOOSER, _t, _o)

typedef enum {
	COLORCHOOSER_COLOR        = COLCH_ATTR(ATTR_OPAQUE_TRIPLE, 1),   /* CS- */
	COLORCHOOSER_CHANGED_PROC = COLCH_ATTR(ATTR_FUNCTION_PTR, 2),    /* CSG */
	COLORCHOOSER_IS_WINDOW_COLOR = COLCH_ATTR(ATTR_BOOLEAN, 3),      /* C-G */
	COLORCHOOSER_TEXTFIELD  = COLCH_ATTR(ATTR_OPAQUE, 4),            /* CSG */
	COLORCHOOSER_CLIENT_DATA  = COLCH_ATTR(ATTR_OPAQUE, 99)          /* CSG */
} Colorchooser_attr;

typedef struct {
	int r, g, b;
} Color_chooser_RGB;

typedef struct {
	int h, s, v;
} Color_chooser_HSV;

typedef struct {
	int red_shift, green_shift, blue_shift;
	int red_width, green_width, blue_width;
} Color_chooser_color_converter;

_XVFUNCPROTOBEGIN
EXTERN_FUNCTION( void xv_HSV_to_RGB, (Color_chooser_HSV *hsv, Color_chooser_RGB *rgb));
EXTERN_FUNCTION( void xv_RGB_to_HSV, (Color_chooser_RGB *rgb, Color_chooser_HSV *hsv));

EXTERN_FUNCTION( void xv_RGB_to_XColor, (Color_chooser_RGB *r, XColor *x));

EXTERN_FUNCTION( void xv_HSV_to_XColor, (Color_chooser_HSV *h, XColor *x));
EXTERN_FUNCTION( void xv_XColor_to_HSV, (XColor *x, Color_chooser_HSV *h));

EXTERN_FUNCTION( void xv_color_chooser_olgx_hsv_to_3D, (Color_chooser_HSV *bg1,
    			XColor *bg2, XColor *bg3, XColor *white));

EXTERN_FUNCTION( unsigned long xv_XColor_to_Pixel, (Color_chooser_color_converter *cc,
									XColor *col));
EXTERN_FUNCTION( void xv_color_choser_initialize_color_conversion, (Frame fram,
									Color_chooser_color_converter *cc));

EXTERN_FUNCTION( void xv_color_chooser_convert_color, (char *val, int panel_to_data,
				Color_chooser_RGB *datptr, Xv_opaque it, Xv_opaque unused));
_XVFUNCPROTOEND

#endif
