#ifndef process_h_included
#define process_h_included 1

/*
 * "@(#) %M% V%I% %E% %U% $Id: process.h,v 4.1 2024/04/12 05:58:21 dra Exp $"
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

#include <xview/generic.h>
#include <xview/attrol.h>

extern Xv_pkg xv_process_pkg;
#define PROCESS &xv_process_pkg
typedef Xv_opaque Process;

typedef struct {
    Xv_generic_struct 	parent_data;
    Xv_opaque           private_data;
} Xv_process;

#define	PROC_ATTR(_t, _o) ATTR(ATTR_PKG_PROCESS, _t, _o)

typedef enum {
	PROCESS_PROGRAM           = PROC_ATTR(ATTR_STRING, 1),           /* CSG */
	PROCESS_ARGV              = PROC_ATTR(ATTR_OPAQUE, 2),           /* CSG */
	PROCESS_ARGS =PROC_ATTR(ATTR_LIST_INLINE(ATTR_NULL,ATTR_STRING),3),/* CS- */
	PROCESS_ENVIRONMENT       = PROC_ATTR(ATTR_OPAQUE, 4),           /* CSG */
	PROCESS_OWN_SESSION       = PROC_ATTR(ATTR_BOOLEAN, 5),          /* CSG */
	PROCESS_INPUT_FD          = PROC_ATTR(ATTR_INT, 6),              /* CSG */
	PROCESS_OUTPUT_FD         = PROC_ATTR(ATTR_INT, 7),              /* CSG */
	PROCESS_ERROR_FD          = PROC_ATTR(ATTR_INT, 8),              /* CSG */
	PROCESS_OUTPUT_PROC       = PROC_ATTR(ATTR_FUNCTION_PTR, 9),     /* CSG */
	PROCESS_ERROR_PROC        = PROC_ATTR(ATTR_FUNCTION_PTR, 10),    /* CSG */
	PROCESS_EXIT_PROC         = PROC_ATTR(ATTR_FUNCTION_PTR, 11),    /* CSG */
	PROCESS_OUTPUT_IS_ERROR   = PROC_ATTR(ATTR_BOOLEAN, 12),         /* CSG */
	PROCESS_RUN               = PROC_ATTR(ATTR_NO_VALUE, 13),        /* CS- */
	PROCESS_PID               = PROC_ATTR(ATTR_INT, 14),             /* --G */
	PROCESS_KILL              = PROC_ATTR(ATTR_INT, 15),             /* -S- */
	PROCESS_AUTO_DESTROY      = PROC_ATTR(ATTR_BOOLEAN, 16),         /* CSG */
	PROCESS_STATUS            = PROC_ATTR(ATTR_ENUM, 17),            /* --G */
	PROCESS_ERRNO             = PROC_ATTR(ATTR_INT, 18),             /* --G */
	PROCESS_ERROR_DESCRIPTION = PROC_ATTR(ATTR_STRING, 19),          /* --G */
	PROCESS_SHELL_COMMAND     = PROC_ATTR(ATTR_STRING, 20),          /* CS- */
	PROCESS_DIRECTORY         = PROC_ATTR(ATTR_STRING, 21),          /* CS- */
	PROCESS_NICE              = PROC_ATTR(ATTR_INT, 22),             /* CSG */
	PROCESS_ABORT_CHILD_IO    = PROC_ATTR(ATTR_NO_VALUE, 23),        /* -S- */
	PROCESS_WAIT_FOR_EXEC     = PROC_ATTR(ATTR_BOOLEAN, 24),         /* CSG */
	PROCESS_NOTIFY_IMMEDIATE  = PROC_ATTR(ATTR_BOOLEAN, 25),         /* CSG */
	PROCESS_OUTPUT_FD_PTR     = PROC_ATTR(ATTR_OPAQUE, 26),          /* CSG */
	PROCESS_ERROR_FD_PTR      = PROC_ATTR(ATTR_OPAQUE, 27),          /* CSG */
	PROCESS_OWN_WAIT          = PROC_ATTR(ATTR_BOOLEAN, 28),         /* CSG */
	PROCESS_DIE               = PROC_ATTR(ATTR_NO_VALUE, 29),        /* -S- */
	PROCESS_CHILD_FUNCTION    = PROC_ATTR(ATTR_FUNCTION_PTR, 30),    /* CSG */
	PROCESS_CLIENT_DATA       = PROC_ATTR(ATTR_OPAQUE, 100)          /* CSG */
} Process_attr;

typedef enum {
	PROCESS_OK,
	PROCESS_EXEC_FAILED,
	PROCESS_NO_EXECUTABLE,
	PROCESS_FORK_FAILED,
	PROCESS_PIPE_FAILED,
	PROCESS_FAILED
} Process_status;

#endif /* process_h_included */
