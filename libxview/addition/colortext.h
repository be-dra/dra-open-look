#ifndef colortext_included
#define colortext_included

/*
 * "@(#) colortext.h V1.7 95/06/19 11:40:03 $Id: colortext.h,v 4.2 2025/03/08 13:37:48 dra Exp $"
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
#include <xview/panel.h>
#include <xview/attrol.h>
#include <xview/colorchsr.h>


extern const Xv_pkg xv_panel_color_text_pkg;
typedef Panel_item Panel_color_text_item;

#define PANEL_COLORTEXT &xv_panel_color_text_pkg

typedef struct {
	Xv_panel_text parent_data;
	Xv_opaque     private_data;
} Xv_panel_colortext;

#define	PCT_ATTR(type, ordinal)	ATTR(ATTR_PKG_PANEL_COLOR_TEXT, type, ordinal)
#define PCT_ATTR_LIST(ltype, type, ordinal) \
				PCT_ATTR(ATTR_LIST_INLINE((ltype), (type)), (ordinal))

typedef enum {
	PANEL_COLORTEXT_WINDOW_BUTTON = PCT_ATTR(ATTR_OPAQUE, 1),   /* --G */
	PANEL_COLORTEXT_HEADER        = PCT_ATTR(ATTR_STRING, 2),   /* C-G */
	PANEL_COLORTEXT_WINDOW_ATTRS  = PCT_ATTR_LIST(ATTR_RECURSIVE, ATTR_AV,19),   /* CS- */
} Panel_colortext_attrs;

#endif
