#ifndef filereq_h_INCLUDED
#define filereq_h_INCLUDED 1

/*
 * "@(#) %M% V%I% %E% %U% $Id: filereq.h,v 4.3 2025/03/08 13:37:48 dra Exp $"
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
#include <xview/sel_pkg.h>
#include <xview/attrol.h>

extern const Xv_pkg xv_filereq_pkg;
typedef Xv_opaque File_requestor;

#define FILE_REQUESTOR &xv_filereq_pkg

typedef struct {
	Xv_sel_requestor parent_data;
    Xv_opaque        private_data;
} Xv_filereq_public;

#define	FR_ATTR(type, ordinal)	ATTR(ATTR_PKG_FILE_REQ, type, ordinal)

typedef enum {
	FILE_REQ_FETCH            = FR_ATTR(ATTR_OPAQUE, 1),   /* -S- */
	FILE_REQ_STATUS           = FR_ATTR(ATTR_ENUM, 2),          /* --G */
	FILE_REQ_SINGLE           = FR_ATTR(ATTR_BOOLEAN, 3),       /* CSG */
	FILE_REQ_OFFSET           = FR_ATTR(ATTR_INT, 4),           /* CSG */
	FILE_REQ_ALLOCATE         = FR_ATTR(ATTR_BOOLEAN, 5),       /* CSG */
	FILE_REQ_FILES            = FR_ATTR(ATTR_INT, 6),           /* --G */
	FILE_REQ_ALREADY_DECODED  = FR_ATTR(ATTR_BOOLEAN, 7),       /* CSG */
	FILE_REQ_CHECK_ACCESS     = FR_ATTR(ATTR_BOOLEAN, 8),       /* CSG */
	FILE_REQ_REQUEST_STRING   = FR_ATTR(ATTR_OPAQUE_PAIR, 9),   /* -S- */
	FILE_REQ_STRING_TO_FILE   = FR_ATTR(ATTR_OPAQUE_PAIR, 10),  /* -S- */
	FILE_REQ_FETCH_VIA        = FR_ATTR(ATTR_OPAQUE, 11),       /* -S- */
	FILE_REQ_REQUEST_DELETE   = FR_ATTR(ATTR_INT, 12),          /* -S- */
	FILE_REQ_DATA             = FR_ATTR(ATTR_OPAQUE, 17),       /* --G */
	FILE_REQ_USE_LOAD         = FR_ATTR(ATTR_BOOLEAN, 27),      /* CSG */
	FILE_REQ_SAVE             = FR_ATTR(ATTR_OPAQUE_TRIPLE, 28),/* -S- */
	FILE_REQ_REMOTE_NAME_FOR  = FR_ATTR(ATTR_STRING, 29),       /* --G */
	FILE_REQ_REMOTE_APPL_FOR  = FR_ATTR(ATTR_STRING, 30),       /* --G */
	FILE_REQ_TITLE_FOR        = FR_ATTR(ATTR_STRING, 31),       /* --G */
	FILE_REQ_IS_LOADED        = FR_ATTR(ATTR_STRING, 34),       /* --G */
	FILE_REQ_RELEASE          = FR_ATTR(ATTR_STRING, 32)        /* -S- */
} Filereq_attr;

typedef enum {
	FR_OK,
	FR_CANNOT_DECODE,
	FR_WRONG_EVENT,
	FR_CONVERSION_REJECTED,
	FR_OPEN_FAILED,
	FR_WRITE_FAILED,
	FR_BAD_TIME,
	FR_BAD_WIN_ID,
	FR_BAD_PROPERTY,
	FR_TIMEDOUT,
	FR_PROPERTY_DELETED,
	FR_BAD_PROPERTY_EVENT,
	FR_UNEXPECTED_DATA,
	FR_UNKNOWN_ERROR
} Filereq_status;

typedef struct {
	long inode, mtime, size;
} Filereq_remote_stat;

/* used for FILE_REQ_SAVE: */
typedef void (*Filereq_remote_save_proc)(File_requestor fr,
												Filereq_status status,
												char *localname);

/* used for FILE_REQ_REQUEST_STRING */
typedef enum {
	FILE_REQ_STRING_ERROR, /* length is a Filereq_status */
	FILE_REQ_STRING, /* not incr, value is the whole received string */
	FILE_REQ_INCR, /* announcement, length is the announced length */
	FILE_REQ_INCR_PACKET, /* partial string during incr */
	FILE_REQ_INCR_END  /* end of incremental transfer */
} Filereq_string_op;

typedef void (*Filereq_string_proc)(File_requestor fr,
											Filereq_string_op op,
											char *value,
											unsigned long length);
#endif
