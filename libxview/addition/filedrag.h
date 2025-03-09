#ifndef filedrag_included
#define filedrag_included

/*
 * "@(#) %M% V%I% %E% %U% $Id: filedrag.h,v 4.3 2025/03/08 13:37:48 dra Exp $"
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
#include <xview/iccc.h>


extern const Xv_pkg xv_filedrag_pkg;
typedef Xv_opaque FileDrag;

#define FILEDRAG &xv_filedrag_pkg

typedef struct {
	Xv_iccc_public parent_data;
    Xv_opaque private_data;
} Xv_filedrag_public;

#define	FILEDRAG_ATTR(type, ordinal)	ATTR(ATTR_PKG_FILEDRAG, type, ordinal)

typedef enum {
	FILEDRAG_FILES = FILEDRAG_ATTR(ATTR_LIST_INLINE(ATTR_NULL,ATTR_STRING),1), /* CS- */
	FILEDRAG_FILE_ARRAY    = FILEDRAG_ATTR(ATTR_OPAQUE, 2),           /* CS- */
	FILEDRAG_ENABLE_STRING = FILEDRAG_ATTR(ATTR_ENUM, 3),             /* CSG */
	FILEDRAG_DO_DELETE     = FILEDRAG_ATTR(ATTR_BOOLEAN, 4),          /* CSG */
	FILEDRAG_ALLOW_MOVE    = FILEDRAG_ATTR(ATTR_BOOLEAN, 5),          /* CSG */
	FILEDRAG_DND_DONE_PROC = FILEDRAG_ATTR(ATTR_FUNCTION_PTR, 6),     /* CSG */
	FILEDRAG_CURRENT_FILE  = FILEDRAG_ATTR(ATTR_BOOLEAN, 20)          /* --G */
} Filedrag_attr;

typedef enum {
	FILEDRAG_NO_STRING, FILEDRAG_NAME_STRING, FILEDRAG_CONTENTS_STRING
} Filedrag_string_setting;

#endif
