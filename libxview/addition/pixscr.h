#ifndef pixscr_h_INCLUDED
#define pixscr_h_INCLUDED

/*
 * "@(#) %M% V%I% %E% %U% $Id: pixscr.h,v 4.1 2024/04/12 05:58:21 dra Exp $"
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


extern Xv_pkg xv_pixmap_scroller_pkg;
#define PIXMAP_SCROLLER &xv_pixmap_scroller_pkg
typedef Xv_opaque Pixmap_scroller;

typedef struct {
	Xv_scrollwin  parent_data;
    Xv_opaque     private_data;
} Xv_pixmap_scroller;

#define	PIXSCR_ATTR(type,ord) ATTR(ATTR_PKG_PIXMAP_SCROLLER,type,ord)

typedef enum {
	PIXSCR_PIXMAP       = PIXSCR_ATTR(ATTR_LONG, 1),          /* CSG */
	PIXSCR_CENTER_IMAGE = PIXSCR_ATTR(ATTR_BOOLEAN, 2),       /* CSG */
	PIXSCR_LAYOUT_PROC  = PIXSCR_ATTR(ATTR_FUNCTION_PTR, 3),  /* CSG */
	PIXSCR_DROP_PROC    = PIXSCR_ATTR(ATTR_FUNCTION_PTR, 4),  /* CSG */
	PIXSCR_DO_LAYOUT    = PIXSCR_ATTR(ATTR_NO_VALUE, 25),     /* -S- */
	PIXSCR_EXPAND_FRAME = PIXSCR_ATTR(ATTR_BOOLEAN, 50)       /* CSG */
} Pixmap_scroller_attr;


#endif /* pixscr_h_INCLUDED */
