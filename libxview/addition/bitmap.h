#ifndef bitmap_h_included
#define bitmap_h_included 1

/*
 * "@(#) %M% V%I% %E% %U% $Id: bitmap.h,v 4.2 2025/03/08 13:37:48 dra Exp $"
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
#include <xview/svrimage.h>
#include <xview/attrol.h>

extern const Xv_pkg xv_bitmap_pkg;
#define BITMAP &xv_bitmap_pkg
typedef Xv_opaque Bitmap;

typedef struct {
	Xv_server_image parent_data;
    Xv_opaque       private_data;
} Xv_bitmap;

#define	BITMAP_ATTR(type, ordinal)	ATTR(ATTR_PKG_BITMAP, type, ordinal)

typedef enum {
	/* public attributes */
	BITMAP_FILE               = BITMAP_ATTR(ATTR_STRING, 1),        /* CSG */
	BITMAP_FILE_STATUS        = BITMAP_ATTR(ATTR_OPAQUE, 2),        /* -S- */
	BITMAP_FILE_TYPE          = BITMAP_ATTR(ATTR_ENUM, 3),          /* CSG */
	BITMAP_LOAD_FILE          = BITMAP_ATTR(ATTR_NO_VALUE, 4),      /* -S- */
	BITMAP_STORE_FILE         = BITMAP_ATTR(ATTR_NO_VALUE, 5),      /* -S- */
	BITMAP_THICKEN            = BITMAP_ATTR(ATTR_NO_VALUE, 6),      /* -S- */
	BITMAP_FILL_FOR_MASK      = BITMAP_ATTR(ATTR_NO_VALUE, 7),      /* -S- */
	BITMAP_COPY_FROM          = BITMAP_ATTR(ATTR_OPAQUE, 8),        /* -S- */
	BITMAP_COPY_TO            = BITMAP_ATTR(ATTR_OPAQUE, 9),        /* -S- */
	BITMAP_INVERT             = BITMAP_ATTR(ATTR_NO_VALUE, 10),     /* -S- */
	BITMAP_FLOOD_FILL         = BITMAP_ATTR(ATTR_INT_PAIR, 100)     /* -S- */
} Bitmap_attr;

typedef enum {
	BITMAP_UNKNOWN,
	BITMAP_XVIEW,
	BITMAP_X11,
	BITMAP_RASTER
} Bitmap_file_type;

typedef enum {
	BITMAP_OK,
	BITMAP_NO_FILE_NAME,
	BITMAP_UNKNOWN_TYPE,
	BITMAP_ACCESS,
	BITMAP_NO_RASTER,
	BITMAP_BAD_MAGIC,
	BITMAP_INVALID_DEPTH,
	BITMAP_INVALID_TYPE,
	BITMAP_INCONSISTENT,
	BITMAP_NO_SERVERIMAGE,
	BITMAP_NO_MEM
} Bitmap_file_status;

#endif
