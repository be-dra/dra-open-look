#ifndef xv_quick_h_INCLUDED
#define xv_quick_h_INCLUDED

/*
 * "@(#) %M% V%I% %E% %U% $Id: xv_quick.h,v 1.4 2025/03/06 16:06:24 dra Exp $"
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

#include <xview/sel_pkg.h>
#include <xview/attrol.h>

#define QUICK_OWNER &xv_quick_owner_pkg

#define	QUICK_ATTR(type, ordinal)	ATTR(ATTR_PKG_QUICK, type, ordinal)
#define ATTR_QUICK_QUAD ATTR_TYPE(ATTR_BASE_OPAQUE, 4)

typedef	Xv_opaque 		Quick_owner;

typedef enum {
	QUICK_START_X				= QUICK_ATTR(ATTR_INT,			  1),
	QUICK_END_X					= QUICK_ATTR(ATTR_INT,			  2),
	QUICK_BASELINE				= QUICK_ATTR(ATTR_INT,			  3),
	QUICK_GET_STRING_PROC		= QUICK_ATTR(ATTR_FUNCTION_PTR,	  4),
	QUICK_REMOVE_UNDERLINE_PROC	= QUICK_ATTR(ATTR_FUNCTION_PTR,	  5),
	QUICK_SELECT_DOWN			= QUICK_ATTR(ATTR_QUICK_QUAD,     6),
	QUICK_LOC_DRAG				= QUICK_ATTR(ATTR_OPAQUE,         7),
	QUICK_ADJUST_UP				= QUICK_ATTR(ATTR_OPAQUE,         8),
	QUICK_FONTINFO				= QUICK_ATTR(ATTR_OPAQUE,         9),
	QUICK_CLIENT_DATA			= QUICK_ATTR(ATTR_OPAQUE,			  99)
} Quick_attr;

typedef struct {
    Xv_sel_owner	parent_data;
    Xv_opaque		private_data;
} Xv_quick_owner;

extern Xv_pkg 		xv_quick_owner_pkg;

#endif /* xv_quick_h_INCLUDED */
