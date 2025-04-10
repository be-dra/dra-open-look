#ifndef regexpr_h_INCLUDED
#define regexpr_h_INCLUDED 1

#include <xview/xv_c_types.h>

/*
 * "@(#) $Id: regexpr.h,v 1.4 2025/04/09 19:53:51 dra Exp $"
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

typedef struct _regexp_context *xv_regexp_context;

EXTERN_FUNCTION(const char *xv_compile_regexp, (char *instring, xv_regexp_context *ctxtp));
EXTERN_FUNCTION(char *xv_match_regexp, (char *string, xv_regexp_context context, ...));
EXTERN_FUNCTION(void xv_free_regexp, (xv_regexp_context context));

#endif /* regexpr_h_INCLUDED */
