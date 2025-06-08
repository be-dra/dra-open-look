#ifndef proplist_INCLUDED
#define proplist_INCLUDED 1

/*
 * "@(#) %M% V%I% %E% %U% $Id: proplist.h,v 4.4 2025/06/06 18:47:52 dra Exp $"
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

extern const Xv_pkg xv_proplist_pkg;
#define PROP_LIST &xv_proplist_pkg
typedef Xv_opaque Property_list;

typedef struct {
	Xv_panel_list parent_data;
    Xv_opaque     private_data;
} Xv_proplist;

/* muss genauso wie in PANEL_LIST (p_list.c) sein */
#define PANEL_LEVEL_DOT_HEIGHT PANEL_CHOICE_OFFSET

#define	LIST_ATTR(_t, _o) ATTR(ATTR_PKG_LISTPROP, _t, _o)

typedef enum {
	PROPLIST_VERIFY_PROC         = LIST_ATTR(ATTR_FUNCTION_PTR, 1),  /* CSG */
	PROPLIST_COPY_PROC           = LIST_ATTR(ATTR_FUNCTION_PTR, 2),  /* CSG */
	PROPLIST_FREE_ITEM_PROC      = LIST_ATTR(ATTR_FUNCTION_PTR, 3),  /* CSG */
	PROPLIST_ITEM_DATA_SIZE      = LIST_ATTR(ATTR_INT, 4),           /* C-- */
	PROPLIST_ENTRY_STRING_OFFSET = LIST_ATTR(ATTR_INT, 5),           /* CSG */
	PROPLIST_GLYPH_OFFSET        = LIST_ATTR(ATTR_INT, 6),           /* CSG */
	PROPLIST_GLYPH_FILE_OFFSET   = LIST_ATTR(ATTR_INT, 7),           /* CSG */
	PROPLIST_SORT_PROC           = LIST_ATTR(ATTR_FUNCTION_PTR, 8),  /* C-G */
	PROPLIST_FONT                = LIST_ATTR(ATTR_OPAQUE, 9),        /* CSG */
	PROPLIST_CREATE_BUTTONS      = LIST_ATTR(ATTR_NO_VALUE, 26)      /* -S- */
} Proplist_attr;

#define PL_GLYPH_INDEX 0
#define PL_MASK_INDEX 1

typedef struct {
	int num_items;
	Xv_opaque item_data;
} Proplist_contents;

typedef enum {
	PROPLIST_APPLY,
	PROPLIST_INSERT,
	PROPLIST_CONVERT,
	PROPLIST_USE_IN_PANEL,
	PROPLIST_CREATE_GLYPH,
	PROPLIST_DELETE_QUERY,
	PROPLIST_DELETE_OLD_CHANGED,
	PROPLIST_DELETE_FROM_RESET,
	PROPLIST_DELETE,
	PROPLIST_LIST_CHANGED,
	PROPLIST_DESELECT
} Proplist_verify_op;

typedef void (*proplist_sort_proc_t) (Property_list);
typedef void (*proplist_copy_proc_t) (Property_list, Xv_opaque, Xv_opaque);
typedef int (*proplist_verify_proc_t) (Property_list, Xv_opaque,
								Proplist_verify_op, Xv_opaque);
typedef void (*proplist_free_proc_t) (Property_list, Xv_opaque);

_XVFUNCPROTOBEGIN
EXTERN_FUNCTION(void xv_proplist_converter, (int, int, Proplist_contents *,
										Property_list, Xv_opaque));
_XVFUNCPROTOEND

#endif
