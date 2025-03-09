#ifndef mllist_h_INCLUDED
#define mllist_h_INCLUDED 1

/*
 * "@(#) %M% V%I% %E% %U% $Id: mllist.h,v 1.11 2025/03/08 13:37:48 dra Exp $"
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
#include <xview/proplist.h>
#include <xview/attrol.h>

extern const Xv_pkg xv_multi_level_list_pkg;
typedef Panel_item Multi_level_list;

#define MULTI_LEVEL_LIST &xv_multi_level_list_pkg

typedef struct {
	Xv_item     parent_data;
	Xv_opaque   private_data;
} Xv_multi_level_list;

#define	MLL_ATTR(type, ordinal)	ATTR(ATTR_PKG_MLLIST, type, ordinal)

typedef enum {
	MLLIST_ROOT_CHILDREN    = MLL_ATTR(ATTR_OPAQUE, 1),       /* CSG */
	MLLIST_INSERT           = MLL_ATTR(ATTR_OPAQUE, 2),       /* -S- */
	MLLIST_IS_LEVEL         = MLL_ATTR(ATTR_OPAQUE_PAIR, 3),  /* -S- */
	MLLIST_OPEN_CHILD       = MLL_ATTR(ATTR_OPAQUE, 4)        /* -S- */
} Multi_level_list_attrs;

/* structure to be used as the FIRST part of the application's
 * list item client data: every 'real' client data structure should
 * look like struct { mllist_node mllpart; ... other fields ... }
 */

typedef struct _mll_entry {
	struct _mll_entry *next, *parent;
	char              *label;
	int               has_sublevel;
	struct _mll_entry *firstchild; /* if has_sublevel == TRUE */
	Xv_opaque         mllist_private[2];
} mllist_node, *mllist_ptr;

#endif
