#ifndef accel_h_INCLUDED
#define accel_h_INCLUDED

/*
 * "@(#) $Id: accel.h,v 4.4 2025/03/08 13:04:27 dra Exp $"
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


extern const Xv_pkg xv_accelerator_pkg;
#define ACCELERATOR &xv_accelerator_pkg
typedef Xv_opaque Accelerator;

typedef struct {
    Xv_permlist    parent_data;
    Xv_opaque      private_data;
} Xv_accelerator;

#define	ACCEL_ATTR(_t, _o) ATTR(ATTR_PKG_ACCEL, _t, _o)
#define ACCEL_REC_ATTR(_t, _o) ACCEL_ATTR(ATTR_LIST_INLINE(ATTR_RECURSIVE, (_t)), (_o))

typedef enum {
	ACCEL_CREATE_ITEMS   = ACCEL_ATTR(ATTR_NO_VALUE, 1),      /* CS- */
	ACCEL_FRAME          = ACCEL_ATTR(ATTR_OPAQUE, 2),        /* CSG */
	ACCEL_REGISTER       = ACCEL_REC_ATTR(ATTR_AV, 39),       /* CS- */
	ACCEL_DONE           = ACCEL_ATTR(ATTR_NO_VALUE, 99),     /* -S- */

	/* subattributes for ACCEL_REGISTER */
	ACCEL_DESCRIPTION    = ACCEL_ATTR(ATTR_STRING, 50),       /* -S- */
	ACCEL_KEY            = ACCEL_ATTR(ATTR_STRING, 51),       /* -S- */
	ACCEL_MENU_AND_ITEM  = ACCEL_ATTR(ATTR_OPAQUE_PAIR, 52),  /* -S- */
	ACCEL_MENU_AND_LABEL = ACCEL_ATTR(ATTR_OPAQUE_PAIR, 53),  /* -S- */
	ACCEL_PROC           = ACCEL_ATTR(ATTR_OPAQUE_PAIR, 54)   /* -S- */
} Accelerator_attr;

/* public functions */
_XVFUNCPROTOBEGIN
EXTERN_FUNCTION (Accelerator xv_accel_get_accelerator, (Xv_window));
_XVFUNCPROTOEND

#endif
