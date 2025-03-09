#ifndef scrollw_h_included
#define scrollw_h_included

/*
 * "@(#) %M% V%I% %E% %U% $Id: scrollw.h,v 4.3 2025/03/08 13:04:27 dra Exp $"
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
#include <xview/openwin.h>

#include <xview/attrol.h>


extern const Xv_pkg xv_scrollwin_pkg;
#define SCROLLWIN &xv_scrollwin_pkg
typedef Xv_opaque Scrollwin;

typedef struct {
	Xv_openwin  parent_data;
    Xv_opaque   private_data;
} Xv_scrollwin;

extern const Xv_pkg xv_scrollview_pkg;
#define SCROLLVIEW &xv_scrollview_pkg
typedef Xv_opaque Scrollview;

typedef struct {
	Xv_openwin_view  parent_data;
	Xv_opaque         private_data;
} Xv_scrollview;

extern const Xv_pkg xv_scrollpw_pkg;
#define SCROLLPW &xv_scrollpw_pkg
typedef Xv_opaque Scrollpw;

typedef struct {
	Xv_window_struct  parent_data;
	Xv_opaque         private_data;
} Xv_scrollpw;

#define	SCROLL_ATTR(type, ordinal)	ATTR(ATTR_PKG_SCROLL, type, ordinal)
#define SW_ATTR_LIST(ltype, type, ordinal) \
				SCROLL_ATTR(ATTR_LIST_INLINE((ltype), (type)), (ordinal))

typedef enum {
	/* public attributes */
	SCROLLWIN_SCALE_PERCENT   = SCROLL_ATTR(ATTR_INT, 1),           /* CSG */
	SCROLLWIN_RESTRICT_PAN_PTR= SCROLL_ATTR(ATTR_BOOLEAN, 2),       /* CSG */
	SCROLLWIN_TRIGGER_REPAINT = SCROLL_ATTR(ATTR_NO_VALUE, 3),      /* -S- */
	SCROLLWIN_V_OBJECT_LENGTH = SCROLL_ATTR(ATTR_INT, 4),           /* CSG */
	SCROLLWIN_H_OBJECT_LENGTH = SCROLL_ATTR(ATTR_INT, 5),           /* CSG */
	SCROLLWIN_V_UNIT          = SCROLL_ATTR(ATTR_INT, 6),           /* CSG */
	SCROLLWIN_H_UNIT          = SCROLL_ATTR(ATTR_INT, 7),           /* CSG */
	SCROLLWIN_ENABLE_AUTO_SCROLL= SCROLL_ATTR(ATTR_BOOLEAN, 10),    /* CSG */
	SCROLLWIN_UPDATE_PROC     = SCROLL_ATTR(ATTR_FUNCTION_PTR, 9),  /* CSG */

	/* private attributes */
    SCROLLWIN_SCALE_CHANGED   = SCROLL_ATTR(ATTR_NO_VALUE, 32),     /* -S- */
	SCROLLWIN_HANDLE_DROP     = SCROLL_ATTR(ATTR_OPAQUE, 34),       /* -S- */
	SCROLLWIN_DROPPABLE       = SCROLL_ATTR(ATTR_ENUM, 35),         /* C-G */
	SCROLLWIN_AUTO_SCROLL     = SCROLL_ATTR(ATTR_OPAQUE, 36),       /* -S- */
	SCROLLWIN_CREATE_SEL_REQ  = SCROLL_ATTR(ATTR_NO_VALUE, 37),     /* --G */
	SCROLLWIN_DROP_EVENT      = SCROLL_ATTR(ATTR_OPAQUE, 38),       /* -S- */
	SCROLLWIN_HANDLE_EVENT    = SCROLL_ATTR(ATTR_OPAQUE, 39),       /* -S- */
	SCROLLWIN_PW_CMS_CHANGED  = SCROLL_ATTR(ATTR_OPAQUE, 40),       /* -S- */
	SCROLLWIN_REPAINT         = SCROLL_ATTR(ATTR_OPAQUE, 41)        /* -S- */
} Scrollwin_attr;

typedef enum {
	SCROLLVIEW_V_START   = SCROLL_ATTR(ATTR_INT, 51),         /* -SG */
	SCROLLVIEW_H_START   = SCROLL_ATTR(ATTR_INT, 52)          /* -SG */
} Scrollview_attr;

typedef enum {
	SCROLLPW_INFO      = SCROLL_ATTR(ATTR_OPAQUE, 61)       /* --G */
} Scrollpw_attr;

/* for SCROLLWIN_DROPPABLE */
typedef enum {
	SCROLLWIN_NONE,
	SCROLLWIN_DROP,
	SCROLLWIN_DEFAULT,
	SCROLLWIN_DROP_WITH_PREVIEW,
	SCROLLWIN_DEFAULT_WITH_PREVIEW
} Scrollwin_drop_setting;

typedef struct {
	Display *dpy;
	Window xid;
	int width, height, scr_x, scr_y;
	int scale_percent;
} Scrollpw_info;

#define SCR_WIN_X(_vi_, _vx_) (((_vi_->scale_percent*(_vx_))/100)-_vi_->scr_x)
#define SCR_WIN_Y(_vi_, _vy_) (((_vi_->scale_percent*(_vy_))/100)-_vi_->scr_y)
#define SCR_WIN_SIZE(_vi_, _vsiz_) ((_vi_->scale_percent*(_vsiz_))/100)

typedef struct {
	Scrollpw_info *vinfo;
	Event *event;
	Scrollpw pw;
	int virt_x, virt_y;
	int action;
	int consumed;
} Scrollwin_event_struct;

typedef enum {
	SCROLLWIN_REASON_EXPOSE, SCROLLWIN_REASON_PAN, SCROLLWIN_REASON_SCROLL
} Scrollwin_repaint_reason;

typedef struct {
	Scrollpw_info *vinfo;
	Scrollpw pw;
	Rect win_rect, virt_rect;
	Scrollwin_repaint_reason reason;
} Scrollwin_repaint_struct;

typedef struct {
	Scrollpw_info *vinfo;
	Event *event;
	int virt_x, virt_y;
	Xv_opaque sel_req;
	char **files;
	int cnt;
} Scrollwin_drop_struct;

typedef struct {
	Scrollpw_info *vinfo;
	Scrollpw paint_window;
	int mouse_x, mouse_y; /* valid only if is_start is FALSE */
	int virt_mouse_x, virt_mouse_y; /* valid only if is_start is FALSE */
	int is_start;
} Scrollwin_auto_scroll_struct;

#endif
