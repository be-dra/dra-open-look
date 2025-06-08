#ifndef fontprop_h_INCLUDED
#define fontprop_h_INCLUDED

/*
 * "@(#) %M% V%I% %E% %U% $Id: fontprop.h,v 4.4 2025/06/06 18:40:17 dra Exp $"
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
#include <xview/xview.h>
#include <xview/font.h>
#include <xview/attrol.h>


extern const Xv_pkg xv_fontprops_pkg;
#define FONT_PROPS &xv_fontprops_pkg
typedef Xv_opaque Font_props;

typedef struct {
    Xv_generic_struct parent_data;
    Xv_opaque         private_data;
} Xv_fontprops;

#define	FONTP_ATTR(_t, _o) ATTR(ATTR_PKG_FONT_PROPS, _t, _o)
#define FONTP_LIST_ATTR(_t_, _o_) FONTP_ATTR(ATTR_LIST_INLINE(ATTR_NULL, _t_), _o_)

typedef enum {
	FONTPROPS_DATA_OFFSET     = FONTP_ATTR(ATTR_INT, 1),           /* CSG */
	FONTPROPS_INIT_FROM_FONT  = FONTP_ATTR(ATTR_OPAQUE_PAIR, 2),   /* CS- */
	FONTPROPS_FIXEDWIDTH_ONLY = FONTP_ATTR(ATTR_BOOLEAN, 3),       /* CSG */
	FONTPROPS_SCALES_ONLY     = FONTP_ATTR(ATTR_BOOLEAN, 4),       /* CSG */
	FONTPROPS_FILL_FAMILY_LIST= FONTP_ATTR(ATTR_OPAQUE, 5),        /* CS- */
	FONTPROPS_FAMILY_LABEL    = FONTP_ATTR(ATTR_STRING, 6),        /* C-- */
	FONTPROPS_NAME_LABEL      = FONTP_ATTR(ATTR_STRING, 7),        /* C-- */
	FONTPROPS_ITEMS           = FONTP_LIST_ATTR(ATTR_ENUM, 8),     /* C-- */
	FONTPROPS_NOTIFY_PROC     = FONTP_ATTR(ATTR_FUNCTION_PTR, 9),  /* CSG */
	FONTPROPS_INIT_FROM_NAME  = FONTP_ATTR(ATTR_OPAQUE, 10),       /* CS- */
	FONTPROPS_USE_FOR_SIZE    = FONTP_ATTR(ATTR_INT, 11),          /* CS- */
	FONTPROPS_NAME_READ_ONLY  = FONTP_ATTR(ATTR_BOOLEAN, 12),      /* CSG */
	FONTPROPS_SCALABLE_ONLY   = FONTP_ATTR(ATTR_BOOLEAN, 13),      /* CSG */
	FONTPROPS_BOLD_AND_MEDIUM = FONTP_ATTR(ATTR_BOOLEAN, 80)       /* CSG */
} Fontprops_attr;

typedef struct {
	char *fontname;
	char *family;
	char *style;
	int scale;
	int size;
	char slant;
} fontprop_t;

typedef enum {
	FONTPROP_TRIGGER,
	FONTPROP_STYLE,
	FONTPROP_SIZE,
	FONTPROP_SLANT,
	FONTPROP_NAME,
	FONTPROP_FAMILY,
	FONTPROP_PREVIEW,
	FONTPROP_last
} Fontprop_setting;

typedef void (*fontprops_notify_proc_t)(Font_props, Fontprop_setting, fontprop_t *);

#endif
