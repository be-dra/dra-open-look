#ifndef graphwin_included
#define graphwin_included

#include <xview/xview.h>
#include <xview/scrollw.h>

/* "@(#) %M% V%I% %E% %U% $Id: graphwin.h,v 1.10 2025/06/03 16:47:39 dra Exp $" */

extern const Xv_pkg xv_graphwin_pkg;
#define GRAPHWIN &xv_graphwin_pkg
typedef Xv_opaque Graphwin;

extern const Xv_pkg xv_graphobj_pkg;
#define GRAPHOBJ &xv_graphobj_pkg
typedef Xv_opaque Graphobj;

extern const Xv_pkg xv_graphpainter_pkg;
#define GRAPHPAINTER &xv_graphpainter_pkg
typedef Xv_opaque Graphpainter;

typedef struct {
	Xv_scrollwin  parent_data;
    Xv_opaque     private_data;
} Xv_graphwin;

typedef struct {
	Xv_generic_struct parent_data;
    Xv_opaque         private_data;
} Xv_graphobj;

typedef struct {
	Xv_generic_struct parent_data;
    Xv_opaque         private_data;
} Xv_graphpainter;

#define ATTR_OPAQUE_QUAD ATTR_TYPE(ATTR_BASE_OPAQUE, 4)
#define ATTR_OPAQUE_SEXT ATTR_TYPE(ATTR_BASE_OPAQUE, 6)

#define	GRAPH_ATTR(type, ordinal) ATTR(ATTR_PKG_GRAPHWIN, type, ordinal)
#define GRAPH_ATTR_LIST(_lt,_t,_o) GRAPH_ATTR(ATTR_LIST_INLINE((_lt),(_t)),(_o))

typedef enum {
	/* public attributes */
	/* for graphics window and graphics object */
	GRAPH_CLIENT_DATA   = GRAPH_ATTR(ATTR_OPAQUE, 1),              /* CSG */
	GRAPH_PAINT         = GRAPH_ATTR(ATTR_NO_VALUE, 2),            /* -S- */
	GRAPH_NO_REDISPLAY_ITEM = GRAPH_ATTR(ATTR_BOOLEAN, 3),         /* CSG */

	/* for graphics window */
	GRAPH_CURRENT_ITEM  = GRAPH_ATTR(ATTR_OPAQUE, 24),             /* CSG */
	GRAPH_FIRST_ITEM    = GRAPH_ATTR(ATTR_OPAQUE, 25),             /* CSG */
	GRAPH_DO_LAYOUT     = GRAPH_ATTR(ATTR_NO_VALUE, 26),           /* -S- */
	GRAPH_EVENT_FEATURES= GRAPH_ATTR_LIST(ATTR_NULL,ATTR_ENUM,27), /* -S- */
	GRAPH_NOTIFY_PROC   = GRAPH_ATTR(ATTR_FUNCTION_PTR, 28),       /* CSG */
	GRAPH_MARGIN        = GRAPH_ATTR(ATTR_INT, 29),                /* CSG */
	GRAPH_NITEMS        = GRAPH_ATTR(ATTR_INT, 30),                /* --G */

	/* for graphics object  */
	GRAPH_SELECTED      = GRAPH_ATTR(ATTR_BOOLEAN, 41),            /* CSG */
	GRAPH_NEXT_ITEM     = GRAPH_ATTR(ATTR_OPAQUE, 42),             /* --G */
	GRAPH_IMAGE_RECT    = GRAPH_ATTR(ATTR_RECT_PTR, 43),           /* CSG */
	GRAPH_LABEL_GRAVITY = GRAPH_ATTR(ATTR_INT, 44),                /* CSG */
	GRAPH_DRAGGABLE     = GRAPH_ATTR(ATTR_BOOLEAN, 45),            /* CSG */
	GRAPH_MOVABLE       = GRAPH_ATTR(ATTR_BOOLEAN, 46),            /* CSG */

	/* for graphics painter */
	GRAPH_USE_NORMAL_CONTEXT = GRAPH_ATTR(ATTR_NO_VALUE, 50),
	GRAPH_USE_INVERS_CONTEXT = GRAPH_ATTR(ATTR_NO_VALUE, 51),
	GRAPH_USE_NON_STANDARD_CONTEXT = GRAPH_ATTR(ATTR_OPAQUE, 52),
	GRAPH_DRAW_POINT = GRAPH_ATTR(ATTR_OPAQUE_PAIR, 53),
	GRAPH_DRAW_LINE = GRAPH_ATTR(ATTR_OPAQUE_QUAD, 54),
	GRAPH_DRAW_LINES = GRAPH_ATTR(ATTR_OPAQUE_PAIR, 55),
	GRAPH_DRAW_RECTANGLE = GRAPH_ATTR(ATTR_OPAQUE_QUAD, 56),
	GRAPH_FILL_RECTANGLE = GRAPH_ATTR(ATTR_OPAQUE_QUAD, 57),
	GRAPH_DRAW_OPAQUE_RECTANGLE = GRAPH_ATTR(ATTR_OPAQUE_QUAD, 58),
	GRAPH_DRAW_POLYGON = GRAPH_ATTR(ATTR_OPAQUE_TRIPLE, 59),
	GRAPH_FILL_POLYGON = GRAPH_ATTR(ATTR_OPAQUE_TRIPLE, 60),
	GRAPH_DRAW_OPAQUE_POLYGON = GRAPH_ATTR(ATTR_OPAQUE_TRIPLE, 61),
	GRAPH_DRAW_ARC = GRAPH_ATTR(ATTR_OPAQUE_SEXT, 62),
	GRAPH_FILL_ARC = GRAPH_ATTR(ATTR_OPAQUE_SEXT, 63),
	GRAPH_DRAW_OPAQUE_ARC = GRAPH_ATTR(ATTR_OPAQUE_SEXT, 64),
	GRAPH_DRAW_STRING = GRAPH_ATTR(ATTR_OPAQUE_QUAD, 65),
	GRAPH_DRAW_OPAQUE_STRING = GRAPH_ATTR(ATTR_OPAQUE_QUAD, 66),
	GRAPH_START_XOR_DRAWING = GRAPH_ATTR(ATTR_NO_VALUE, 67),
	GRAPH_END_XOR_DRAWING = GRAPH_ATTR(ATTR_NO_VALUE, 68),

	/* private attributes */
	GRAPH_CATCH         = GRAPH_ATTR(ATTR_INT_PAIR, 91),           /* --G */
	GRAPH_INSERT_OBJ    = GRAPH_ATTR(ATTR_OPAQUE, 92),             /* -S- */
	GRAPH_REMOVE_OBJ    = GRAPH_ATTR(ATTR_OPAQUE, 93),             /* -S- */
	GRAPH_DRAW          = GRAPH_ATTR(ATTR_OPAQUE, 94),             /* -S- */
	GRAPH_START_DND     = GRAPH_ATTR(ATTR_OPAQUE, 95),             /* -S- */
	GRAPH_ENTER_BEFORE  = GRAPH_ATTR(ATTR_OPAQUE, 96)              /* --G */
} Graph_attr;

typedef enum {
	GRAPH_NOTE_DOUBLE_SELECT,
	GRAPH_NOTE_DOUBLE_ADJUST
} Graphwin_notification;

typedef enum {
	GE_MULTISELECT = 1,
	GE_INTERACTIVE_POSITION,
	GE_DRAG_AND_DROP,
	GE_FRAME_CATCH         /* must be the last ! */
} Graphwin_event_features;

typedef struct {
	unsigned visible : 1;
	unsigned selected : 1;
	unsigned draggable : 1;
	unsigned movable : 1;
} Graphobj_state;

typedef struct {
	Rect rect, image_rect;
	Graphobj_state state;
	GC gc;
	GC normal_gc, xor_gc, invers_gc;
	Scrollpw_info *vinfo;
	Graphpainter painter;
} Graphobj_repaint_struct;

typedef void (*Graphwin_notify_proc_t)(Graphwin, Graphwin_notification, Event *);
;
#endif
