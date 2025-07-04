#ifndef talk_h_INCLUDED
#define talk_h_INCLUDED 1

#include <xview/xview.h>
#include <xview/sel_pkg.h>

/* "@(#) %M% V%I% %E% %U% $Id: talk.h,v 1.18 2025/06/23 16:31:57 dra Exp $" */

/* This class has been motivated by ToolTalk.
 * We can distinguish two kinds of TALK objects:
 *
 * +++ TALK objects that are only used to trigger an action (= send a
 *     message).
 *     xv_create(window, TALK,
 *                      TALK_MESSAGE, "DoThis",
 *                      TALK_MSG_PARAMS
 *                         "param1",
 *                         "32456"
 *                         NULL,
 *                      NULL);
 *
 * +++ TALK objects that register one or more "patterns" that they are
 *     interested in. That sort of object needs a TALK_NOTIFY_PROC. This
 *     sort of TALK objects can also send messages (see above).
 *     xv_create(window, TALK,
 *                      TALK_NOTIFY_PROC, note_talk,
 *                      TALK_PATTERN, "DoThis",
 *                      TALK_PATTERN, "DoThat",
 *                      NULL);
 *
 */

extern const Xv_pkg xv_talk_pkg;
#define TALK &xv_talk_pkg
typedef Xv_opaque Talk;

typedef struct {
	Xv_sel_requestor parent_data;
	Xv_opaque        private_data;
} Xv_talk;

#define	TALK_ATTR(type, ordinal)	ATTR(ATTR_PKG_TALK, type, ordinal)
#define TALK_ATTR_LIST(ltype, type, ordinal) \
				TALK_ATTR(ATTR_LIST_INLINE((ltype), (type)), (ordinal))

typedef enum {
	TALK_NOTIFY_PROC   = TALK_ATTR(ATTR_FUNCTION_PTR, 2),           /* C-G */
	TALK_PATTERN       = TALK_ATTR(ATTR_STRING, 3),                 /* CS- */
	TALK_MESSAGE       = TALK_ATTR(ATTR_STRING, 4),                 /* -S- */
	TALK_MSG_PARAMS    = TALK_ATTR_LIST(ATTR_NULL, ATTR_STRING, 5), /* -S- */
	TALK_SILENT        = TALK_ATTR(ATTR_BOOLEAN, 6),                /* CSG */
	TALK_LAST_ERROR    = TALK_ATTR(ATTR_INT, 7),                    /* --G */
	TALK_NOTIFY_COUNT  = TALK_ATTR(ATTR_OPAQUE, 8),                 /* -S- */
	TALK_CLIENT_DATA   = TALK_ATTR(ATTR_OPAQUE, 9),                 /* CSG */
	TALK_START         = TALK_ATTR_LIST(ATTR_NULL, ATTR_STRING,10), /* -S- */
	TALK_DEREGISTER    = TALK_ATTR(ATTR_NO_VALUE, 98)               /* -S- */
} Talk_attr;

typedef void (*talk_notify_proc_t)(Talk t, char *msg, char *sender_host,
								int sender_pid, char **params);

#endif /* talk_h_INCLUDED */
