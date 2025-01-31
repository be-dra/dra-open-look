#ifndef funckey_h_INCLUDED
#define funckey_h_INCLUDED

/*
 * "@(#) %M% V%I% %E% %U% $Id: funckey.h,v 4.4 2024/11/04 22:25:14 dra Exp $"
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


extern Xv_pkg xv_funckeys_pkg;
#define FUNCTION_KEYS &xv_funckeys_pkg
typedef Xv_opaque Function_keys;

typedef struct {
    Xv_permlist    parent_data;
    Xv_opaque      private_data;
} Xv_funckeys;

#define	FUNCK_ATTR(_t, _o) ATTR(ATTR_PKG_FUNCTION_KEYS, _t, _o)
#define FUNCK_LIST_ATTR(_t_, _o_) FUNCK_ATTR(ATTR_LIST_INLINE(ATTR_NULL, _t_), _o_)
#define FUNCK_REC_ATTR(_t, _o) FUNCK_ATTR(ATTR_LIST_INLINE(ATTR_RECURSIVE, (_t)), (_o))

typedef enum {
	FUNCKEY_UPDATE_LABELS        = FUNCK_ATTR(ATTR_NO_VALUE, 2),      /* -S- */
	FUNCKEY_SOFT_KEY_WINDOWS     = FUNCK_LIST_ATTR(ATTR_OPAQUE, 10),      /* CS- */
	FUNCKEY_ADD_SOFT_KEY_WINDOW  = FUNCK_ATTR(ATTR_OPAQUE_PAIR, 21),  /* CS- */
	FUNCKEY_EVENT_CALLBACK       = FUNCK_ATTR(ATTR_FUNCTION_PTR, 11), /* CSG */
	FUNCKEY_REGISTER             = FUNCK_REC_ATTR(ATTR_AV, 15),       /* CS- */
	FUNCKEY_HANDLE_EVENT         = FUNCK_ATTR(ATTR_OPAQUE, 5),        /* --G */
	FUNCKEY_CREATE_TOP           = FUNCK_REC_ATTR(ATTR_AV, 23),       /* -S- */
	FUNCKEY_INCLUDE_WM_FUNCTIONS = FUNCK_ATTR(ATTR_BOOLEAN, 16),      /* CSG */

	FUNCKEY_INSTALL              = FUNCK_ATTR(ATTR_NO_VALUE, 12),     /* -S- */
	FUNCKEY_COMMAND_ITEM         = FUNCK_ATTR(ATTR_OPAQUE, 18),       /* --G */

	/* use FUNCKEY_DESCRIPTION and FUNCKEY_CODE with XV_KEY_DATA
	 * on Menu_items to mark them as 'function-key-able'.
	 * Then set the menu using:
	 */
	FUNCKEY_MENU                 = FUNCK_ATTR(ATTR_OPAQUE, 20),       /* -S- */

	/* subattributes for FUNCKEY_REGISTER */
	FUNCKEY_DESCRIPTION          = FUNCK_ATTR(ATTR_STRING, 50),      /* -S- */
	FUNCKEY_CODE                 = FUNCK_ATTR(ATTR_STRING, 51),      /* -S- */
	FUNCKEY_PANEL_BUTTON         = FUNCK_ATTR(ATTR_OPAQUE, 52),      /* -S- */
	FUNCKEY_MENU_ITEM            = FUNCK_ATTR(ATTR_OPAQUE_PAIR, 53), /* -S- */
	FUNCKEY_PROC                 = FUNCK_ATTR(ATTR_OPAQUE_PAIR, 54), /* -S- */
	FUNCKEY_FALL_THROUGH         = FUNCK_ATTR(ATTR_NO_VALUE, 55)     /* -S- */
} Funckeys_attr;

/* Conventions for FUNCKEY_MENU: */
#define MENU_FUNCKEY_CODE XV_KEY_DATA,FUNCKEY_CODE
#define MENU_FUNCKEY_DESCRIPTION XV_KEY_DATA,FUNCKEY_DESCRIPTION

typedef struct {
	char *listlabel;
	char *label;
	int fkeynumber, mods;
	int enum_val;
} Funckey_descr_t;

_XVFUNCPROTOBEGIN
EXTERN_FUNCTION(Function_keys xv_function_keys_from_server, (Xv_server srv));
_XVFUNCPROTOEND

#endif
