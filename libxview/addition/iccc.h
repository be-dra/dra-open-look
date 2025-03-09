#ifndef iccc_included
#define iccc_included

/*
 * "@(#) %M% V%I% %E% %U% $Id: iccc.h,v 4.4 2025/03/08 13:37:48 dra Exp $"
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
#include <xview/window.h>
#include <xview/dragdrop.h>
#include <xview/attrol.h>


extern const Xv_pkg xv_iccc_pkg;
typedef Xv_opaque Iccc;

#define ICCC &xv_iccc_pkg

typedef struct {
	Xv_dnd_struct parent_data;
    Xv_opaque private_data;
} Xv_iccc_public;

#define	ICCC_ATTR(type, ordinal)	ATTR(ATTR_PKG_ICCC, type, ordinal)

typedef enum {
	ICCC_CONVERT            = ICCC_ATTR(ATTR_OPAQUE_PAIR, 1),    /* --G */
	ICCC_ADD_TARGET         = ICCC_ATTR(ATTR_LONG, 2),           /* CS- */
	ICCC_ADD_TARGETS=ICCC_ATTR(ATTR_LIST_INLINE(ATTR_NULL,ATTR_LONG),3),/*CS- */
	ICCC_ADD_TARGET_NAME    = ICCC_ATTR(ATTR_STRING, 4),         /* CS- */
	ICCC_ADD_TARGET_NAMES   = ICCC_ATTR(ATTR_LIST_INLINE(ATTR_NULL, ATTR_STRING), 5), /* CS- */
	ICCC_REMOVE_TARGET      = ICCC_ATTR(ATTR_LONG, 6),           /* CS- */
	ICCC_REMOVE_TARGETS = ICCC_ATTR(ATTR_LIST_INLINE(ATTR_NULL, ATTR_LONG), 7), /* CS- */
	ICCC_REMOVE_TARGET_NAME = ICCC_ATTR(ATTR_STRING, 8),         /* CS- */
	ICCC_REMOVE_TARGET_NAMES= ICCC_ATTR(ATTR_LIST_INLINE(ATTR_NULL, ATTR_STRING), 9), /* CS- */
	ICCC_APPL_CONVERT_PROC  = ICCC_ATTR(ATTR_FUNCTION_PTR, 41)   /* CS- */
} Iccc_attr;

typedef struct {
	Atom *type;
	Xv_opaque *value;
	unsigned long *length;
	int *format;
} iccc_convert_t;

_XVFUNCPROTOBEGIN
EXTERN_FUNCTION (int xv_iccc_convert, (Xv_opaque owner, Atom *target, Xv_opaque *value, unsigned long *length, int *format));
_XVFUNCPROTOEND

#endif
