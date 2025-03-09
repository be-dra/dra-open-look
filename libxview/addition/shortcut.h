#ifndef shortcut_h_INCLUDED
#define shortcut_h_INCLUDED

/*
 * "@(#) %M% V%I% %E% %U% $Id: shortcut.h,v 4.2 2025/03/08 13:37:48 dra Exp $"
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
#include <xview/permlist.h>
#include <xview/attrol.h>


extern const Xv_pkg xv_shortcuts_pkg;
#define SHORTCUTS &xv_shortcuts_pkg
typedef Xv_opaque Shortcuts;

typedef struct {
    Xv_permlist    parent_data;
    Xv_opaque      private_data;
} Xv_shortcuts;

#define	SHCUT_ATTR(_t, _o) ATTR(ATTR_PKG_SHORTCUTS, _t, _o)
#define SHCUT_REC_ATTR(_t, _o) SHCUT_ATTR(ATTR_LIST_INLINE(ATTR_RECURSIVE, (_t)), (_o))

typedef enum {
	SHORTCUT_REGISTER             = SHCUT_ATTR(ATTR_OPAQUE, 15),  /* CS- */
	SHORTCUT_HANDLE_EVENT         = SHCUT_ATTR(ATTR_OPAQUE, 5),   /* --G */
	SHORTCUT_COMMAND_ITEM         = SHCUT_ATTR(ATTR_OPAQUE, 6),   /* --G */
	SHORTCUT_CREATE_TOP           = SHCUT_REC_ATTR(ATTR_AV, 23),  /* -S- */

	/* use with XV_KEY_DATA on menu items with submenu: */
	SHORTCUT_AVOID_SUBMENU        = SHCUT_ATTR(ATTR_BOOLEAN, 1)
} Shortcuts_attr;

#endif
