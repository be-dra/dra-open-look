#ifndef permlist_h_INCLUDED
#define permlist_h_INCLUDED

/*
 * "@(#) %M% V%I% %E% %U% $Id: permlist.h,v 4.7 2025/07/13 07:26:01 dra Exp $"
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
#include <xview/permprop.h>
#include <xview/proplist.h>


extern const Xv_pkg xv_permlist_pkg;
#define PERMANENT_LIST &xv_permlist_pkg
typedef Xv_opaque Permanent_list;

typedef struct {
    Xv_generic_struct parent_data;
    Xv_opaque         private_data;
} Xv_permlist;

#define	PERML_ATTR(_t, _o) ATTR(ATTR_PKG_PERM_LIST, _t, _o)
#define ATTR_PERML_QUAD ATTR_TYPE(ATTR_BASE_OPAQUE, 4)
#define PERML_ATTR_LIST(ordinal) \
    

typedef enum {
	PERMLIST_CREATE_TOP=PERML_ATTR(ATTR_LIST_INLINE(ATTR_RECURSIVE,ATTR_AV),1),/* -S- */
	PERMLIST_PROP_MASTER          = PERML_ATTR(ATTR_OPAQUE, 2),      /* --G */
	PERMLIST_ADD_RESOURCE         = PERML_ATTR(ATTR_PERML_QUAD, 3),  /* C-- */
	PERMLIST_ITEM_RESOURCE_NAME   = PERML_ATTR(ATTR_STRING, 4),      /* CSG */
	PERMLIST_RESOURCE_CATEGORY    = PERML_ATTR(ATTR_INT, 5),         /* C-- */
	PERMLIST_BEFORE_RESET         = PERML_ATTR(ATTR_OPAQUE, 6),      /* -S- */
	PERMLIST_BEFORE_RESET_FACTORY = PERML_ATTR(ATTR_OPAQUE, 7),      /* -S- */
	PERMLIST_TRIGGER_FREE         = PERML_ATTR(ATTR_OPAQUE, 8),      /* -S- */
	PERMLIST_TRIGGER_COPY         = PERML_ATTR(ATTR_OPAQUE_PAIR, 9), /* -S- */
	PERMLIST_CREATE_BOTTOM        = PERML_ATTR(ATTR_NO_VALUE, 99),   /* -S- */
	/* private attributes */
	PERMLIST_OWNER_SET            = PERML_ATTR(ATTR_NO_VALUE, 111)   /* -S- */
	                                        /* keep away from 112 !!! */
} Permlist_attr;

#endif
