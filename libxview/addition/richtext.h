#ifndef richtext_included
#define richtext_included

/*
 * "@(#) %M% V%I% %E% %U% $Id: richtext.h,v 1.5 2025/03/08 13:37:48 dra Exp $"
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

#include <xview/scrollw.h>
#include <xview/attrol.h>

extern const Xv_pkg xv_richtext_pkg;
#define RICHTEXT &xv_richtext_pkg
typedef Xv_opaque Richtext;

typedef struct {
	Xv_scrollwin parent_data;
    Xv_opaque private_data;
} Xv_richtext;

#define	RICHTXT_ATTR(type, ordinal) ATTR(ATTR_PKG_RICHTEXT, type, ordinal)

typedef enum {
	RICHTEXT_START             = RICHTXT_ATTR(ATTR_STRING, 1),       /* CS- */
	RICHTEXT_APPEND            = RICHTXT_ATTR(ATTR_STRING, 2),       /* -S- */
	RICHTEXT_FILLED            = RICHTXT_ATTR(ATTR_NO_VALUE, 3),     /* -S- */
	RICHTEXT_REPORT_LONG_LINES = RICHTXT_ATTR(ATTR_BOOLEAN, 5),      /* CSG */
	RICHTEXT_BASE_FONT         = RICHTXT_ATTR(ATTR_OPAQUE, 99)       /* CS- */
} Spellcheck_attr;

#endif /* richtext_included */
