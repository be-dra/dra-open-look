#include <stdio.h>
#include <xview/graphwin.h>
#include <xview/cms.h>
#include <xview/defaults.h>
#include <xview/help.h>
#include <xview/font.h>

char graphwin_c_sccsid[] = "@(#) %M% V%I% %E% %U% $Id: graphwin.c,v 1.35 2025/06/03 16:47:47 dra Exp $";

#define A0 *attrs
#define A1 attrs[1]
#define A2 attrs[2]
#define A3 attrs[3]
#define A4 attrs[4]
#define A5 attrs[5]
#define A6 attrs[6]

#define ADONE ATTR_CONSUME(*attrs);break

typedef enum {
  SNULL, SNULL_TIMER, SEL_BACK, SEL_OBJ, SEL_OBJ_TIMER, ADJ_BACK,
  ADJ_OBJ, ADJ_OBJ_TIMER, FRAME_SEL, FRAME_ADJ,
  CONSTRAIN_PRESSED, CONSTRAIN_X, CONSTRAIN_Y, DRAG_PENDING, MOVE
} graphwin_states;

typedef enum {
  SELECT_DOWN, SELECT_DOWN_S, SELECT_DOWN_CONSTRAIN, SELECT_DOWN_CS,
  SELECT_UP, ADJUST_DOWN, ADJUST_UP, DRAG, TIMER, STOP,
  UNKNOWN_INPUT, SYNT_IMPOSSIBLE, SYNT_START_FRAME_CATCH, SYNT_START_DRAG
} graphwin_events;

typedef struct _graphobj {
	Xv_opaque               public_self;
	struct _graphobj        *next;
	Rect                    rect, label_rect, image_rect;
	char                    *label;
	int                     label_gravity;
	int                     space;
	Graphobj_state          state;
	struct _graphwin        *owner;
	Xv_opaque               client_data;
	char                    no_redisplay;
} Graphobj_private, *graphlist;

typedef void (*gw_autoscroll_t)(struct _graphwin *, Scrollpw_info *);

typedef struct _graphwin {
	Xv_opaque               public_self;
	graphlist               current, first;
	struct _graphpaint     *painter;
	int                     down_x, down_y, drag_threshold,
							nitems, margin, multiclick_time,
							vdown_x, vdown_y, vlast_x, vlast_y;
	GC                      frame_gc, sel_draw_gc, draw_gc;
	Xv_opaque               client_data;
	Graphwin_notify_proc_t  notify_proc;
	gw_autoscroll_t         auto_scroll_func;
	unsigned long           features;
	graphwin_states         state;
	int                     ascent;
	char                    being_destroyed, no_redisplay,
							auto_scroll_do_x, auto_scroll_do_y;
} Graphwin_private;

typedef struct _graphpaint {
	Xv_opaque               public_self;
	Scrollpw_info          *vi;
	Graphwin_private       *ownerpriv;
	int                     state;
	GC                      current_gc;
} Graphpaint_private;

#define GRAPHPRIV(_x_) XV_PRIVATE(Graphwin_private, Xv_graphwin, _x_)
#define GRAPHPUB(_x_) XV_PUBLIC(_x_)

#define GROBJPRIV(_x_) XV_PRIVATE(Graphobj_private, Xv_graphobj, _x_)
#define GROBJPUB(_x_) XV_PUBLIC(_x_)

#define GRPAINTPRIV(_x_) XV_PRIVATE(Graphpaint_private, Xv_graphpainter, _x_)
#define GRPAINTPUB(_x_) XV_PUBLIC(_x_)

#define MAXi(a,b) (((a) > (b)) ? (a) : (b))
#define MINi(a,b) (((a) < (b)) ? (a) : (b))
#define ABSo(a) (((a) < 0) ? -(a) : (a))

static void start_timer(Graphwin_private *priv);

static void repaint_obj(Graphwin_private *priv, graphlist it)
{
	Xv_opaque pw;
	Scrollpw_info vi;
	Graphobj_repaint_struct grs;

	if (priv->no_redisplay) return;
	if (it->no_redisplay) return;

	grs.rect = it->rect;
	grs.image_rect = it->image_rect;
	grs.state = it->state;
	grs.gc = it->state.selected ? priv->sel_draw_gc : priv->draw_gc;
	grs.xor_gc = priv->frame_gc;
	grs.normal_gc = priv->draw_gc;
	grs.invers_gc = priv->sel_draw_gc;
	grs.vinfo = &vi;
	grs.painter = GRPAINTPUB(priv->painter);
	priv->painter->vi = &vi;

	OPENWIN_EACH_PW(GRAPHPUB(priv), pw)
		xv_get(pw, SCROLLPW_INFO, &vi);
		XClearArea(vi.dpy, vi.xid, 
					grs.rect.r_left - vi.scr_x, grs.rect.r_top - vi.scr_y,
					(unsigned)grs.rect.r_width, (unsigned)grs.rect.r_height,
					FALSE);
		xv_set(GROBJPUB(it), GRAPH_DRAW, &grs, NULL);
	OPENWIN_END_EACH
}

static void graphwin_repaint_from_expose(Graphwin_private *priv,
							Scrollwin_repaint_struct *rs)
{
	graphlist it;
	Graphobj_repaint_struct grs;

	if (priv->no_redisplay) return;

	grs.vinfo = rs->vinfo;
	if (priv->painter) priv->painter->vi = grs.vinfo;
	grs.painter = GRPAINTPUB(priv->painter);
	grs.xor_gc = priv->frame_gc;
	grs.normal_gc = priv->draw_gc;
	grs.invers_gc = priv->sel_draw_gc;

	for (it = priv->first; it; it = it->next) {
		if (! it->no_redisplay && it->state.visible) {
			if (rect_intersectsrect(&rs->virt_rect, &it->rect)) {
				grs.rect = it->rect;
				grs.image_rect = it->image_rect;
				grs.state = it->state;
				grs.gc = it->state.selected ? priv->sel_draw_gc :priv->draw_gc;
				xv_set(GROBJPUB(it), GRAPH_DRAW, &grs, NULL);
			}
		}
	}
}

static void update_virtual_size(Graphwin_private *priv)
{
	graphlist it;
	struct {
		int r_left, r_top, r_width, r_height;
	} whole, tmpwhole;
	int vunit, hunit;

	whole.r_left = whole.r_top = priv->margin;
	whole.r_width = whole.r_height = 1 + 2 * priv->margin;

	priv->nitems = 0;
	for (it = priv->first; it; it = it->next) {
		++priv->nitems;
		if (it->rect.r_width > 0 && it->rect.r_height > 0) {
			tmpwhole.r_left = MIN(whole.r_left, it->rect.r_left);
			tmpwhole.r_top = MIN(whole.r_top, it->rect.r_top);
			tmpwhole.r_width = MAX(whole.r_left + whole.r_width,
								it->rect.r_left + (int)it->rect.r_width)
							- tmpwhole.r_left;
			tmpwhole.r_height = MAX(whole.r_top + whole.r_height,
								it->rect.r_top + (int)it->rect.r_height)
							- tmpwhole.r_top;
			whole = tmpwhole;
		}
	}

	whole.r_width += priv->margin;
	whole.r_height += priv->margin;

	if (whole.r_left < priv->margin || whole.r_top < priv->margin) {
		int deltax = 0, deltay = 0;

		if (whole.r_left < priv->margin) deltax = priv->margin - whole.r_left;
		if (whole.r_top < priv->margin) deltay = priv->margin - whole.r_top;

		priv->no_redisplay = TRUE;
		for (it = priv->first; it; it = it->next) {
			xv_set(GROBJPUB(it),
					XV_X, it->rect.r_left + deltax,
					XV_Y, it->rect.r_top + deltay,
					NULL);
		}
		priv->no_redisplay = FALSE;
		whole.r_width += deltax;
		whole.r_height += deltay;
	}

	vunit = (int)xv_get(GRAPHPUB(priv), SCROLLWIN_V_UNIT);
	hunit = (int)xv_get(GRAPHPUB(priv), SCROLLWIN_H_UNIT);

	if (vunit < 1) vunit = 1;
	if (hunit < 1) hunit = 1;

	xv_set(GRAPHPUB(priv),
			SCROLLWIN_V_OBJECT_LENGTH, 1 + (whole.r_height-1) / vunit,
			SCROLLWIN_H_OBJECT_LENGTH, 1 + (whole.r_width-1) / hunit,
			NULL);
	if (! priv->no_redisplay)
		xv_set(GRAPHPUB(priv), SCROLLWIN_TRIGGER_REPAINT, NULL);
}

static void internal_drag_objects(Graphwin_private *priv, Scrollpw_info *vinfo)
{
	graphlist it;
	int deltax = priv->vlast_x - priv->vdown_x;
	int deltay = priv->vlast_y - priv->vdown_y;
	Graphobj_repaint_struct grs;

	grs.gc = priv->frame_gc;
	grs.xor_gc = priv->frame_gc;
	grs.normal_gc = priv->draw_gc;
	grs.invers_gc = priv->sel_draw_gc;
	grs.vinfo = vinfo;
	if (priv->painter) priv->painter->vi = vinfo;
	grs.painter = GRPAINTPUB(priv->painter);


	for (it = priv->first; it; it = it->next) {
		if (it->state.selected) {
			grs.rect = it->rect;
			grs.image_rect = it->image_rect;
			grs.state = it->state;
			grs.rect.r_left += deltax;
			grs.rect.r_top += deltay;

			xv_set(GROBJPUB(it), GRAPH_DRAW, &grs, NULL);
		}
	}
}

static short drag_objects(Graphwin_private *priv, Scrollwin_event_struct * es)
{
	internal_drag_objects(priv, es->vinfo);
	return 0;
}

static void perform_frame_catch(Graphwin_private *priv,
					Scrollwin_event_struct *es, int toggle)
{
	Rect frame;
	graphlist it;

	frame.r_left = MINi(es->virt_x, priv->vdown_x);
	frame.r_top = MINi(es->virt_y, priv->vdown_y);
	frame.r_width = ABSo(es->virt_x - priv->vdown_x);
	frame.r_height = ABSo(es->virt_y - priv->vdown_y);

	for (it = priv->first; it; it = it->next) {
		if (rect_intersectsrect(&frame, &it->rect)) {
			if (toggle) it->state.selected = ! it->state.selected;
			else it->state.selected = TRUE;
		}
		else if (! toggle) it->state.selected = FALSE;
	}

/* 	xv_set(GRAPHPUB(priv), SCROLLWIN_TRIGGER_REPAINT, NULL); */
}

static void move_selected(Graphwin_private *priv)
{
	graphlist it;
	int deltax = priv->vlast_x - priv->vdown_x;
	int deltay = priv->vlast_y - priv->vdown_y;

	priv->no_redisplay = TRUE;
	for (it = priv->first; it; it = it->next) {
		if (it->state.selected) {
			xv_set(GROBJPUB(it),
					XV_X, it->rect.r_left + deltax,
					XV_Y, it->rect.r_top + deltay,
					NULL);
		}
	}

	xv_set(GRAPHPUB(priv), GRAPH_DO_LAYOUT, NULL);
	priv->no_redisplay = FALSE;
	xv_set(GRAPHPUB(priv), SCROLLWIN_TRIGGER_REPAINT, NULL);
}

static graphlist find_caught(graphlist it, int x, int y)
{
	graphlist hit;

	if (! it) return it;
	if ((hit = find_caught(it->next, x, y))) return hit;
	if (!it->state.visible) return (graphlist)0;

	if (rect_includespoint(&it->rect, x, y)) {
		if (xv_get(GROBJPUB(it), GRAPH_CATCH, x, y)) return it;
	}

	return (graphlist)0;
}

/*********************************************************************/
/*****                        actions                            *****/
/*********************************************************************/

static short remember_down(Graphwin_private *priv, Scrollwin_event_struct * es)
{
	priv->vdown_x = es->virt_x;
	priv->vdown_y = es->virt_y;
	priv->down_x = (int)event_x(es->event);
	priv->down_y = (int)event_y(es->event);
	return 0;
}

static short check_catch(Graphwin_private *priv, Scrollwin_event_struct * es)
{
	priv->current = find_caught(priv->first, es->virt_x, es->virt_y);
	if (priv->current) return 1;
	else return 0;
}

static short disable_auto_scroll(Graphwin_private *priv)
{
	xv_set(GRAPHPUB(priv), SCROLLWIN_ENABLE_AUTO_SCROLL, FALSE, NULL);
	xv_set(GRPAINTPUB(priv->painter), GRAPH_END_XOR_DRAWING, NULL);
	xv_set(GRAPHPUB(priv), SCROLLWIN_TRIGGER_REPAINT, NULL);
	return 0;
}

static short check_feature_multiselect(Graphwin_private *priv)
{
	return ((priv->features & (1 << (int)GE_MULTISELECT)) != 0);
}

static short check_feature_interactive_position(Graphwin_private *priv)
{
	return ((priv->features & (1 << (int)GE_INTERACTIVE_POSITION)) != 0);
}

static short check_feature_drag_and_drop(Graphwin_private *priv)
{
	return ((priv->features & (1 << (int)GE_DRAG_AND_DROP)) != 0);
}

static short check_feature_frame_catch(Graphwin_private *priv)
{
	return ((priv->features & (1 << (int)GE_FRAME_CATCH)) != 0);
}

static short check_something_movable(Graphwin_private *priv)
{
	graphlist it;
	short one_movable = FALSE;

	for (it = priv->first; it; it = it->next) {
		if (it->state.selected) {
			if (it->state.movable) {
				one_movable = TRUE;
			}
			else {
				it->state.selected = FALSE;
				repaint_obj(priv, it);
			}
		}
	}

	return one_movable;
}

static short check_drag_threshold(Graphwin_private *priv,
								Scrollwin_event_struct * es)
{
	int dx, dy;

	if ((dx = priv->down_x - (int)event_x(es->event)) < 0) dx = -dx;
	if ((dy = priv->down_y - (int)event_y(es->event)) < 0) dy = -dy;

	if (dx > priv->drag_threshold || dy > priv->drag_threshold) return 1;
	else return 0;
}

static short beep(Graphwin_private *priv)
{
	xv_set(GRAPHPUB(priv), WIN_ALARM, NULL);
	return 0;
}

static void drag_and_drop(Graphwin_private *priv, Scrollwin_event_struct * es)
{
	xv_set(GRAPHPUB(priv), GRAPH_START_DND, es, NULL);
}

static short handle_sel_down_object(Graphwin_private *priv)
{
	Graphobj_private *sel = priv->current;
	graphlist it;

	if (sel) {
		if (! sel->state.selected) {
			for (it = priv->first; it; it = it->next) {
				if (it->state.selected) {
					it->state.selected = FALSE;
					repaint_obj(priv, it);
				}
			}
			sel->state.selected = TRUE;
			repaint_obj(priv, sel);
		}
	}
	return 0;
}

static short is_x_direction(Graphwin_private *priv, Scrollwin_event_struct * es)
{
	int dx, dy;
	short retval;

	xv_set(GRPAINTPUB(priv->painter), GRAPH_START_XOR_DRAWING, NULL);
	if ((dx = priv->down_x - (int)event_x(es->event)) < 0) dx = -dx;
	if ((dy = priv->down_y - (int)event_y(es->event)) < 0) dy = -dy;

	if (dx < dy) {
		retval = FALSE;
		priv->vlast_x = priv->vdown_x;
		priv->vlast_y = es->virt_y;
	}
	else {
		retval = TRUE;
		priv->vlast_x = es->virt_x;
		priv->vlast_y = priv->vdown_y;
	}
	internal_drag_objects(priv, es->vinfo);

	priv->auto_scroll_do_x = retval;
	priv->auto_scroll_do_y = ! retval;
	priv->auto_scroll_func = internal_drag_objects;
	xv_set(GRAPHPUB(priv), SCROLLWIN_ENABLE_AUTO_SCROLL, TRUE, NULL);

	return retval;
}

static short start_move(Graphwin_private *priv, Scrollwin_event_struct * es)
{
	xv_set(GRPAINTPUB(priv->painter), GRAPH_START_XOR_DRAWING, NULL);
	priv->vlast_x = es->virt_x;
	priv->vlast_y = es->virt_y;
	internal_drag_objects(priv, es->vinfo);

	priv->auto_scroll_do_x = TRUE;
	priv->auto_scroll_do_y = TRUE;
	priv->auto_scroll_func = internal_drag_objects;
	xv_set(GRAPHPUB(priv), SCROLLWIN_ENABLE_AUTO_SCROLL, TRUE, NULL);

	return 0;
}

static short end_constrain_x(Graphwin_private *priv, Scrollwin_event_struct * es)
{
	internal_drag_objects(priv, es->vinfo);
	priv->vlast_x = es->virt_x;
	move_selected(priv);
	return 0;
}

static short end_constrain_y(Graphwin_private *priv, Scrollwin_event_struct * es)
{
	internal_drag_objects(priv, es->vinfo);
	priv->vlast_y = es->virt_y;
	move_selected(priv);
	return 0;
}

static short end_move(Graphwin_private *priv, Scrollwin_event_struct * es)
{
	internal_drag_objects(priv, es->vinfo);
	priv->vlast_y = es->virt_y;
	priv->vlast_x = es->virt_x;

	move_selected(priv);
	return 0;
}

static short move_update(Graphwin_private *priv, Scrollwin_event_struct * es)
{
	internal_drag_objects(priv, es->vinfo);
	priv->vlast_y = es->virt_y;
	priv->vlast_x = es->virt_x;
	internal_drag_objects(priv, es->vinfo);
	return 0;
}

static short constrain_x_update(Graphwin_private *priv, Scrollwin_event_struct * es)
{
	internal_drag_objects(priv, es->vinfo);
	priv->vlast_x = es->virt_x;
	internal_drag_objects(priv, es->vinfo);
	return 0;
}

static short constrain_y_update(Graphwin_private *priv, Scrollwin_event_struct * es)
{
	internal_drag_objects(priv, es->vinfo);
	priv->vlast_y = es->virt_y;
	internal_drag_objects(priv, es->vinfo);
	return 0;
}

static void internal_draw_frame(Graphwin_private *priv, Scrollpw_info *vinfo)
{
	XDrawRectangle(vinfo->dpy, vinfo->xid, priv->frame_gc,
			SCR_WIN_X(vinfo, MINi(priv->vdown_x, priv->vlast_x)),
			SCR_WIN_Y(vinfo, MINi(priv->vdown_y, priv->vlast_y)),
			(unsigned)SCR_WIN_SIZE(vinfo, ABSo(priv->vdown_x - priv->vlast_x)),
			(unsigned)SCR_WIN_SIZE(vinfo, ABSo(priv->vdown_y - priv->vlast_y)));
}

static short draw_frame(Graphwin_private *priv, Scrollwin_event_struct * es)
{
	internal_draw_frame(priv, es->vinfo);
	return 0;
}

static short start_frame(Graphwin_private *priv, Scrollwin_event_struct * es)
{
	xv_set(GRPAINTPUB(priv->painter), GRAPH_START_XOR_DRAWING, NULL);
	priv->vlast_x = es->virt_x;
	priv->vlast_y = es->virt_y;
	internal_draw_frame(priv, es->vinfo);

	priv->auto_scroll_do_x = TRUE;
	priv->auto_scroll_do_y = TRUE;
	priv->auto_scroll_func = internal_draw_frame;
	xv_set(GRAPHPUB(priv), SCROLLWIN_ENABLE_AUTO_SCROLL, TRUE, NULL);

	return 0;
}

static short update_frame(Graphwin_private *priv, Scrollwin_event_struct * es)
{
	internal_draw_frame(priv, es->vinfo);
	priv->vlast_x = es->virt_x;
	priv->vlast_y = es->virt_y;
	internal_draw_frame(priv, es->vinfo);
	return 0;
}

static short select_frame(Graphwin_private *priv, Scrollwin_event_struct * es)
{
	internal_draw_frame(priv, es->vinfo);
	priv->vlast_x = es->virt_x;
	priv->vlast_y = es->virt_y;
	perform_frame_catch(priv, es, FALSE);
	return 0;
}

static short adjust_frame(Graphwin_private *priv, Scrollwin_event_struct * es)
{
	internal_draw_frame(priv, es->vinfo);
	priv->vlast_x = es->virt_x;
	priv->vlast_y = es->virt_y;
	perform_frame_catch(priv, es, TRUE);
	return 0;
}

static short deselect_all(Graphwin_private *priv)
{
	graphlist it;

	for (it = priv->first; it; it = it->next) {
		if (it->state.selected) {
			it->state.selected = FALSE;
			repaint_obj(priv, it);
		}
	}
	return 0;
}

static short double_select(Graphwin_private *priv, Scrollwin_event_struct * es)
{
	if (priv->notify_proc)
		(*(priv->notify_proc))(GRAPHPUB(priv),
								GRAPH_NOTE_DOUBLE_SELECT,
								es->event);
	return 0;
}

static short double_adjust(Graphwin_private *priv, Scrollwin_event_struct * es)
{
	if (priv->notify_proc)
		(*(priv->notify_proc))(GRAPHPUB(priv),
								GRAPH_NOTE_DOUBLE_ADJUST,
								es->event);
	return 0;
}

static short deselect_all_except(Graphwin_private *priv)
{
	graphlist it;

	for (it = priv->first; it; it = it->next) {
		if (it != priv->current && it->state.selected) {
			it->state.selected = FALSE;
			repaint_obj(priv, it);
		}
	}
	return 0;
}

static void graphwin_handle_event(graphwin_events ev, graphwin_states *staptr,
					Graphwin_private *priv, Scrollwin_event_struct *es)
{
/* 		graphwin_machine(ev, staptr, priv, gev); */
	switch (*staptr) {
		case SNULL:
			switch (ev) {
				case SELECT_DOWN:
					remember_down(priv, es);
					if (! check_catch(priv, es)) {
						*staptr = SEL_BACK;
						return;
					}
					handle_sel_down_object(priv);
					*staptr = SEL_OBJ;
					return;

				case SELECT_DOWN_S:
					if (! check_feature_drag_and_drop(priv)) {
						graphwin_handle_event(SELECT_DOWN, staptr, priv, es);
						return;
					}
					remember_down(priv, es);
					if (! check_catch(priv, es)) {
						*staptr = SNULL;
						return;
					}
					handle_sel_down_object(priv);
					*staptr = DRAG_PENDING;
					return;

				case SELECT_DOWN_CONSTRAIN:
					remember_down(priv, es);
					if (! check_catch(priv, es)) {
						*staptr = SEL_BACK;
						return;
					}
					handle_sel_down_object(priv);
					*staptr = CONSTRAIN_PRESSED;
					return;

				case ADJUST_DOWN:
					if (! check_feature_multiselect(priv)) return;
					remember_down(priv, es);
					if (! check_catch(priv, es)) {
						*staptr = ADJ_BACK;
						return;
					}
					*staptr = ADJ_OBJ;
					return;

				default: break;
			}
			break;

		case SNULL_TIMER:
			switch (ev) {
				case SELECT_DOWN:
					remember_down(priv, es);
					if (! check_catch(priv, es)) {
						*staptr = SEL_BACK;
						return;
					}
					handle_sel_down_object(priv);
					*staptr = SEL_OBJ_TIMER;
					return;

				case SELECT_DOWN_S: /* drag */
					if (! check_feature_drag_and_drop(priv)) {
						graphwin_handle_event(SELECT_DOWN, staptr, priv, es);
						return;
					}
					remember_down(priv, es);
					if (! check_catch(priv, es)) {
						*staptr = SNULL;
						return;
					}
					handle_sel_down_object(priv);
					*staptr = DRAG_PENDING;
					return;

				case ADJUST_DOWN:
					if (!check_feature_multiselect(priv)) return;
					remember_down(priv, es);
					if (! check_catch(priv, es)) {
						*staptr = ADJ_BACK;
						return;
					}
					*staptr = ADJ_OBJ_TIMER;
					return;

				case TIMER:
					*staptr = SNULL;
					return;

				default: break;
			}
			break;

		case SEL_BACK:
			switch (ev) {
				case DRAG:
					if (!check_drag_threshold(priv, es)) return;
					if (! check_feature_frame_catch(priv)) return;
					start_frame(priv, es);
					*staptr = FRAME_SEL;
					return;

				case SELECT_UP:
					deselect_all(priv);
					*staptr = SNULL;
					return;

				case ADJUST_DOWN:
					beep(priv);
					*staptr = SNULL;
					return;

				default: break;
			}
			break;

		case SEL_OBJ:
			switch (ev) {
				case DRAG:
					if (!check_drag_threshold(priv, es)) return;
					if (! check_feature_interactive_position(priv)) {
						graphwin_handle_event(SYNT_START_DRAG,staptr, priv, es);
						return;
					}
					if (! check_something_movable(priv)) {
						graphwin_handle_event(SYNT_IMPOSSIBLE,
											staptr, priv, es);
						return;
					}
					start_move(priv, es);
					*staptr = MOVE;
					return;

				case SYNT_START_DRAG:
					if (! check_feature_drag_and_drop(priv)) {
						graphwin_handle_event(SYNT_START_FRAME_CATCH,
											staptr, priv, es);
						return;
					}
					drag_and_drop(priv, es);
					*staptr = SNULL;
					return;

				case SYNT_START_FRAME_CATCH:
					if (! check_feature_frame_catch(priv)) {
						graphwin_handle_event(SYNT_IMPOSSIBLE,staptr, priv, es);
						return;
					}
					start_frame(priv, es);
					*staptr = FRAME_SEL;
					return;

				case SYNT_IMPOSSIBLE:
					beep(priv);
					*staptr = SNULL;
					return;

				case SELECT_UP:
					deselect_all_except(priv);
					start_timer(priv);
					*staptr = SNULL_TIMER;
					return;

				case ADJUST_DOWN:
					beep(priv);
					*staptr = SNULL;
					return;

				default: break;
			}

		case SEL_OBJ_TIMER:
			switch (ev) {
				case DRAG:
					if (!check_drag_threshold(priv, es)) return;
					if (! check_feature_interactive_position(priv)) {
						graphwin_handle_event(SYNT_START_DRAG,staptr, priv, es);
						return;
					}
					if (! check_something_movable(priv)) {
						graphwin_handle_event(SYNT_IMPOSSIBLE,
											staptr, priv, es);
						return;
					}
					start_move(priv, es);
					*staptr = MOVE;
					return;

				case SYNT_START_DRAG:
					if (! check_feature_drag_and_drop(priv)) {
						graphwin_handle_event(SYNT_START_FRAME_CATCH,
											staptr, priv, es);
						return;
					}
					drag_and_drop(priv, es);
					*staptr = SNULL;
					return;

				case SYNT_START_FRAME_CATCH:
					if (! check_feature_frame_catch(priv)) {
						graphwin_handle_event(SYNT_IMPOSSIBLE,staptr, priv, es);
						return;
					}
					start_frame(priv, es);
					*staptr = FRAME_SEL;
					return;

				case SYNT_IMPOSSIBLE:
					beep(priv);
					*staptr = SNULL;
					return;

				case SELECT_UP:
					double_select(priv, es);
					*staptr = SNULL;
					return;

				case ADJUST_DOWN:
					beep(priv);
					*staptr = SNULL;
					return;

				case TIMER:
					*staptr = SEL_OBJ;
					return;
		
				default:
					break;
			}
			break;

		case ADJ_BACK:
			switch (ev) {
				case SELECT_DOWN:
					beep(priv);
					*staptr = SNULL;
					return;

				case DRAG:
					if (!check_drag_threshold(priv, es)) return;
					if (! check_feature_frame_catch(priv)) return;
					start_frame(priv, es);
					*staptr = FRAME_ADJ;
					return;

				case ADJUST_UP:
					*staptr = SNULL;
					return;

				default: break;
			}
			break;

		case ADJ_OBJ:
			switch (ev) {
				case SELECT_DOWN:
					beep(priv);
					*staptr = SNULL;
					return;

				case DRAG:
					if (!check_drag_threshold(priv, es)) return;
					if (! check_feature_frame_catch(priv)) return;
					start_frame(priv, es);
					*staptr = FRAME_ADJ;
					return;

				case ADJUST_UP:
					if (priv->current) {
						priv->current->state.selected =
											! priv->current->state.selected;
						repaint_obj(priv, priv->current);
					}
					start_timer(priv);
					*staptr = SNULL_TIMER;
					return;
				default: break;
			}
			break;

		case ADJ_OBJ_TIMER:
			switch (ev) {
				case SELECT_DOWN:
					beep(priv);
					*staptr = SNULL;
					return;

				case DRAG:
					if (!check_drag_threshold(priv, es)) return;
					if (! check_feature_frame_catch(priv)) return;
					start_frame(priv, es);
					*staptr = FRAME_ADJ;
					return;

				case ADJUST_UP:
					double_adjust(priv, es);
					*staptr = SNULL;
					return;

				case TIMER:
					*staptr = ADJ_OBJ;
					return;

				default: break;
			}
			break;

		case FRAME_SEL:
			switch (ev) {
				case DRAG:
					update_frame(priv, es);
					return;

				case SELECT_UP:
					select_frame(priv, es);
					disable_auto_scroll(priv);
					*staptr = SNULL;
					return;

				case STOP:
					draw_frame(priv, es);
					disable_auto_scroll(priv);
					*staptr = SNULL;
					return;

				case ADJUST_DOWN:
					beep(priv);
					draw_frame(priv, es);
					disable_auto_scroll(priv);
					*staptr = SNULL;
					return;

				default: break;
			}
			break;

		case FRAME_ADJ:
			switch (ev) {
				case SELECT_DOWN:
					beep(priv);
					draw_frame(priv, es);
					disable_auto_scroll(priv);
					*staptr = SNULL;
					return;

				case DRAG:
					update_frame(priv, es);
					return;

				case STOP:
					draw_frame(priv, es);
					disable_auto_scroll(priv);
					*staptr = SNULL;
					return;

				case ADJUST_UP:
					adjust_frame(priv, es);
					disable_auto_scroll(priv);
					*staptr = SNULL;
					return;

				default: break;
			}
			break;

		case CONSTRAIN_PRESSED:
			switch (ev) {
				case SELECT_UP:
					deselect_all_except(priv);
					*staptr = SNULL;
					return;

				case DRAG:
					if (!check_drag_threshold(priv, es)) return;
					if (! check_feature_interactive_position(priv)) {
						*staptr = SNULL;
						return;
					}
					if (! check_something_movable(priv)) {
						graphwin_handle_event(SYNT_IMPOSSIBLE,
											staptr, priv, es);
						return;
					}
					if (is_x_direction(priv, es)) *staptr = CONSTRAIN_X;
					else *staptr = CONSTRAIN_Y;
					return;

				case SYNT_IMPOSSIBLE:
					beep(priv);
					*staptr = SNULL;
					return;

				default: break;
			}
			break;

		case CONSTRAIN_X:
			switch (ev) {
				case DRAG:
					constrain_x_update(priv, es);
					return;

				case STOP:
					drag_objects(priv, es);
					disable_auto_scroll(priv);
					*staptr = SNULL;
					return;

				case SELECT_UP:
					end_constrain_x(priv, es);
					disable_auto_scroll(priv);
					*staptr = SNULL;
					return;

				case ADJUST_UP:
					end_constrain_x(priv, es);
					disable_auto_scroll(priv);
					*staptr = SNULL;
					return;

				default: break;
			}
			break;

		case CONSTRAIN_Y:
			switch (ev) {
				case DRAG:
					constrain_y_update(priv, es);
					return;

				case STOP:
					drag_objects(priv, es);
					disable_auto_scroll(priv);
					*staptr = SNULL;
					return;

				case SELECT_UP:
					end_constrain_y(priv, es);
					disable_auto_scroll(priv);
					*staptr = SNULL;
					return;

				case ADJUST_UP:
					end_constrain_y(priv, es);
					disable_auto_scroll(priv);
					*staptr = SNULL;
					return;

				default: break;
			}
			break;

		case DRAG_PENDING:
			switch (ev) {
				case SELECT_UP:
					*staptr = SNULL;
					return;

				case STOP:
					*staptr = SNULL;
					return;

				case DRAG:
					if (!check_drag_threshold(priv, es)) return;
					drag_and_drop(priv, es);
					*staptr = SNULL;
					return;

				default: break;
			}
			break;

		case MOVE:
			switch (ev) {
				case DRAG:
					move_update(priv, es);
					return;

				case STOP:
					drag_objects(priv, es);
					disable_auto_scroll(priv);
					*staptr = SNULL;
					return;

				case SELECT_UP:
					end_move(priv, es);
					disable_auto_scroll(priv);
					*staptr = SNULL;
					return;

				default: break;
			}
			break;
	}
}

static Notify_value timer_event(Notify_client cl, int unused)
{
	Graphwin_private *priv = (Graphwin_private *)cl;
	Scrollpw_info vi;
	Scrollwin_event_struct es;
	graphwin_states st;

	es.pw = xv_get(GRAPHPUB(priv), OPENWIN_NTH_PW, 0);
	xv_get(es.pw, SCROLLPW_INFO, &vi);
	es.vinfo = &vi;
	es.event = (Event *)0;
	es.virt_x = 0;
	es.virt_y = 0;
	st = priv->state;
	priv->painter->vi = es.vinfo;
	graphwin_handle_event(TIMER, &st, priv, &es);
	priv->state = st;
	return NOTIFY_DONE;
}

static void start_timer(Graphwin_private *priv)
{
	struct itimerval timer;

	timer.it_value.tv_sec = 0;
	timer.it_value.tv_usec = 100000 * priv->multiclick_time;
	timer.it_interval.tv_sec = 99999999; /* linux-bug */
	timer.it_interval.tv_usec = 0;
	notify_set_itimer_func((Xv_opaque)priv, timer_event, ITIMER_REAL, &timer,
							(struct itimerval *)0);
}

static int is_contrain(Graphwin_private *priv, Event *ev)
{
	return (int)xv_get(XV_SERVER_FROM_WINDOW(GRAPHPUB(priv)),
				SERVER_EVENT_HAS_CONSTRAIN_MODIFIERS, ev);
}

static int graphwin_event_proc(Graphwin_private *priv,
								Scrollwin_event_struct *es)
{
	graphwin_states st;
	graphwin_events sem;

	switch (es->action) {
		case ACTION_SELECT:
			if (event_is_down(es->event)) {
				if (is_contrain(priv, es->event)) {
					sem = SELECT_DOWN_CONSTRAIN;
				}
				else if (event_shift_is_down(es->event)) {
					if (event_ctrl_is_down(es->event)) sem = SELECT_DOWN_CS;
					else sem = SELECT_DOWN_S;
				}
				else {
					sem = SELECT_DOWN;
				}
			}
			else sem = SELECT_UP;
			break;

		case ACTION_ADJUST:
			if (event_is_down(es->event)) sem = ADJUST_DOWN;
			else sem = ADJUST_UP;
			break;

		case LOC_DRAG:
			sem = DRAG;
			break;

		case ACTION_STOP:
			sem = STOP;
			break;

		case ACTION_HELP:
		case ACTION_MORE_HELP:
		case ACTION_TEXT_HELP:
		case ACTION_MORE_TEXT_HELP:
		case ACTION_INPUT_FOCUS_HELP:
			if (event_is_down(es->event)) {
				char *help = (char *)xv_get(GRAPHPUB(priv), XV_HELP_DATA);

				if (help) xv_help_show(es->pw, help, es->event);
				else xv_help_show(es->pw, "GraphWindow:graph_window",es->event);
			}
			return TRUE;

		default:
			return FALSE;
	}

	priv->painter->vi = es->vinfo;
	st = priv->state;
	graphwin_handle_event(sem, &st, priv, es);
	priv->state = st;
	return TRUE;
}

static graphlist unlink_object(graphlist l, graphlist obj)
{
	if (l) {
		if (l == obj) return l->next;
		l->next = unlink_object(l->next, obj);
	}
	return l;
}

static void set_new_event_features(Graphwin_private *priv, Attr_attribute *feat)
{
	unsigned long mask = 0;
	Attr_attribute *p;

	for (p = feat; *p; p++) {
		mask |= (1 << (int)*p);
	}

	priv->features = mask;
}

static void graphwin_make_gcs(Graphwin_private *priv, Scrollpw pw)
{
	XGCValues gcv;
	Display *dpy = (Display *)xv_get(pw, XV_DISPLAY);
	Window xid = (Window)xv_get(pw, XV_XID);
	Xv_opaque font = xv_get(GRAPHPUB(priv), XV_FONT);
	Cms cms;
	int fore_index, back_index;
	unsigned long back, fore;
	XFontStruct *finfo;

	if (priv->frame_gc) XFreeGC(dpy, priv->frame_gc);
	if (priv->sel_draw_gc) XFreeGC(dpy, priv->sel_draw_gc);
	if (priv->draw_gc) XFreeGC(dpy, priv->draw_gc);

	cms = xv_get(pw, WIN_CMS);
	fore_index = (int)xv_get(pw, WIN_FOREGROUND_COLOR);
	back_index = (int)xv_get(pw, WIN_BACKGROUND_COLOR);

	back = (unsigned long)xv_get(cms, CMS_PIXEL, back_index);
	fore = (unsigned long)xv_get(cms, CMS_PIXEL, fore_index);

	finfo = (XFontStruct *)xv_get(font, FONT_INFO);
	priv->ascent = finfo->ascent;

	gcv.line_width = 0;
	gcv.font = (Font)xv_get(font, XV_XID);
	gcv.foreground = back ^ fore;
	gcv.function = GXxor;
	priv->frame_gc = XCreateGC(dpy, xid,
					GCFont | GCFunction | GCForeground | GCLineWidth,
					&gcv);

	gcv.foreground = fore;
	gcv.background = back;
	gcv.function = GXcopy;
	priv->draw_gc = XCreateGC(dpy, xid,
					GCFunction | GCForeground | GCBackground | GCFont,
					&gcv);

	gcv.foreground = back;
	gcv.background = fore;
	priv->sel_draw_gc = XCreateGC(dpy, xid,
					GCFunction | GCForeground | GCBackground | GCFont,
					&gcv);
}

static void do_auto_scroll(Graphwin_private *priv,
								Scrollwin_auto_scroll_struct *ass)
{
	if (ass->is_start) {
		(*(priv->auto_scroll_func))(priv, ass->vinfo);
	}
	else {
		if (priv->auto_scroll_do_x) priv->vlast_x = ass->virt_mouse_x;
		if (priv->auto_scroll_do_y) priv->vlast_y = ass->virt_mouse_y;
		(*(priv->auto_scroll_func))(priv, ass->vinfo);
	}
}

static void insert_sorted(Graphwin_private *priv, graphlist it)
{
	graphlist prev = 0, p = priv->first;

	while (p) {
		if (xv_get(GROBJPUB(it), GRAPH_ENTER_BEFORE, GROBJPUB(p))) {
			if (prev) {
				prev->next = it;
				it->next = p;
				return;
			}
			else {
				priv->first = it;
				it->next = p;
				return;
			}
		}
		prev = p;
		p = p->next;
	}
	prev->next = it;
}

static Xv_opaque graphwin_set(Xv_opaque self, Attr_avlist avlist)
{
	int need_layout = FALSE;
	Attr_attribute *attrs;
	Graphwin_private *priv = GRAPHPRIV(self);
	int consumed;
	Scrollwin_event_struct *es;
	graphlist it;

	for (attrs=avlist; *attrs; attrs=attr_next(attrs)) switch ((int)*attrs) {
		case SCROLLWIN_HANDLE_EVENT:
			es = (Scrollwin_event_struct *)A1;
			consumed = graphwin_event_proc(priv, es);
			if (consumed) {
				es->consumed = TRUE;
				ADONE;
			}
			break;

		case SCROLLWIN_AUTO_SCROLL:
			do_auto_scroll(priv, (Scrollwin_auto_scroll_struct *)A1);
			ADONE;

		case GRAPH_START_DND:
			xv_error(self,
				ERROR_STRING, "GRAPH_START_DND: subclass responsibility",
				NULL);
			ADONE;

		case SCROLLWIN_REPAINT:
			graphwin_repaint_from_expose(priv, (Scrollwin_repaint_struct *)A1);
			ADONE;

		case SCROLLWIN_HANDLE_DROP:
			ADONE;

		case GRAPH_INSERT_OBJ:
			it = GROBJPRIV((Xv_opaque)A1);
			it->next = (graphlist)0;
			++priv->nitems;
			if (priv->first) {
				insert_sorted(priv, it);
			}
			else {
				priv->first = it;
			}
			need_layout = TRUE;
			ADONE;

		case GRAPH_REMOVE_OBJ:
			if (! priv->being_destroyed) {
				--priv->nitems;
				priv->first = unlink_object(priv->first,
									GROBJPRIV((Xv_opaque)A1));
				xv_set(self, GRAPH_DO_LAYOUT, NULL);
			}
			ADONE;

		case GRAPH_DO_LAYOUT:
			update_virtual_size(priv);
			ADONE;

		case SCROLLWIN_SCALE_CHANGED:
			need_layout = TRUE;
			break;

		case GRAPH_EVENT_FEATURES:
			set_new_event_features(priv, (Attr_attribute *)&A1);
			ADONE;

		case GRAPH_CLIENT_DATA:
			priv->client_data = (Xv_opaque)A1;
			ADONE;

		case GRAPH_NO_REDISPLAY_ITEM:
			priv->no_redisplay = (char)A1;
			ADONE;

		case GRAPH_MARGIN:
			priv->margin = (int)A1;
			if (priv->margin <= 0) priv->margin = 1;
			need_layout = TRUE;
			ADONE;

		case GRAPH_NOTIFY_PROC:
			priv->notify_proc = (Graphwin_notify_proc_t)A1;
			ADONE;

		case SCROLLWIN_PW_CMS_CHANGED:
			graphwin_make_gcs(priv, (Scrollpw)A1);
			break;

		case XV_END_CREATE:
			graphwin_make_gcs(priv, xv_get(self, OPENWIN_NTH_PW, 0));
			xv_create(self, GRAPHPAINTER, NULL);
			break;

		default:
			xv_check_bad_attr(GRAPHWIN, A0);
	}

	if (need_layout) {
		xv_set(self, GRAPH_DO_LAYOUT, NULL);
	}

	return XV_OK;
}

static Xv_opaque graphwin_get(Xv_opaque self, int *status, Attr_attribute attr,
								va_list vali)
{
	Graphwin_private *priv = GRAPHPRIV(self);
	graphlist it;
	int v1, v2;

	*status = XV_OK;
	switch ((int)attr) {
/* 		case SCROLLWIN_DROPPABLE: return (Xv_opaque)SCROLLWIN_NONE; */
		case GRAPH_NOTIFY_PROC: return (Xv_opaque)priv->notify_proc;
		case GRAPH_CLIENT_DATA: return priv->client_data;
		case GRAPH_MARGIN: return (Xv_opaque)priv->margin;
		case GRAPH_NITEMS: return (Xv_opaque)priv->nitems;
		case GRAPH_CATCH:
			v1 = va_arg(vali, int);
			v2 = va_arg(vali, int);
			it = find_caught(priv->first, v1, v2);
			if (it) return GROBJPUB(it);
			else return XV_NULL;
		case GRAPH_FIRST_ITEM:
			if (priv->first) return GROBJPUB(priv->first);
			else return XV_NULL;
		default:
			*status = xv_check_bad_attr(GRAPHWIN, attr);
			return (Xv_opaque)XV_OK;
	}
}

static int graphwin_init(Xv_opaque owner, Xv_opaque xself, Attr_avlist avlist,
						int *unused)
{
	Xv_graphwin *self = (Xv_graphwin *)xself;
	Graphwin_private *priv;

	priv = (Graphwin_private *)xv_alloc(Graphwin_private);
	if (!priv) return XV_ERROR;

	priv->public_self = (Xv_opaque)self;
	self->private_data = (Xv_opaque)priv;

	priv->drag_threshold = defaults_get_integer("openWindows.dragThreshold",
											"OpenWindows.DragThreshold", 5);
	priv->multiclick_time = 
				defaults_get_integer_check("openWindows.multiClickTimeout",
								"OpenWindows.MultiClickTimeout", 4, 2, 10);

	priv->margin = 2;
	priv->features = ((1 << (int)GE_MULTISELECT) | 
					(1 << (int)GE_INTERACTIVE_POSITION) |
					(1 << (int)GE_FRAME_CATCH));

	return XV_OK;
}

static void destroy_all_objects(graphlist obj)
{
	if (obj) {
		destroy_all_objects(obj->next);
		xv_destroy(GROBJPUB(obj));
	}
}

static int graphwin_destroy(Xv_opaque self, Destroy_status status)
{
	if (status == DESTROY_CLEANUP) {
		Graphwin_private *priv = GRAPHPRIV(self);
		Display *dpy = (Display *)xv_get(self, XV_DISPLAY);

		priv->being_destroyed = TRUE;

		/* destroy all objects */
		destroy_all_objects(priv->first);

		if (priv->painter) xv_destroy(GRPAINTPUB(priv->painter));
		if (priv->frame_gc) XFreeGC(dpy, priv->frame_gc);
		if (priv->sel_draw_gc) XFreeGC(dpy, priv->sel_draw_gc);
		if (priv->draw_gc) XFreeGC(dpy, priv->draw_gc);

		free((char *)priv);
	}
	return XV_OK;
}

/********************************************************************/
/********************************************************************/
/******                                                        ******/
/******                     Graph Object                       ******/
/******                                                        ******/
/********************************************************************/
/********************************************************************/

static void graphobj_draw(Graphobj_private *priv, Graphobj_repaint_struct *grs,
								int ascent)
{
	Scrollpw_info *vi = grs->vinfo;

	if (! priv->label || ! *priv->label) return;

	XDrawImageString(vi->dpy, vi->xid, grs->gc,
			SCR_WIN_X(vi, grs->rect.r_left + priv->label_rect.r_left),
			SCR_WIN_Y(vi, grs->rect.r_top + priv->label_rect.r_top +ascent),
			priv->label,
			(int)strlen(priv->label));
}

static void make_label_rect(Graphobj_private *priv)
{
	Font_string_dims dims;
	Xv_font font = xv_get(GRAPHPUB(priv->owner), XV_FONT);
	XFontStruct *finfo;
	int scale = (int)xv_get(GRAPHPUB(priv->owner), SCROLLWIN_SCALE_PERCENT);

	finfo = (XFontStruct *)xv_get(font, FONT_INFO);

	if (priv->label) {
		xv_get(font, FONT_STRING_DIMS, priv->label, &dims);
		priv->label_rect.r_width = dims.width;
	}
	else {
		xv_get(font, FONT_STRING_DIMS, "A", &dims);
		priv->label_rect.r_width = 0;
	}
	priv->label_rect.r_height = finfo->ascent + finfo->descent;

	/* INCOMPLETE, this is only correct as long as
	 * the font is not rescaled
	 */
	priv->label_rect.r_height = (priv->label_rect.r_height * 100) / scale;
	priv->label_rect.r_width = (priv->label_rect.r_width * 100) / scale;
}

static void make_rect(Graphobj_private *priv)
{
	Rect *r = &priv->rect,
		*lr = &priv->label_rect,
		*ir = &priv->image_rect;

	switch (priv->label_gravity) {
		case NorthWestGravity:
			r->r_width = MAXi(lr->r_width, ir->r_width);
			r->r_height = lr->r_height + ir->r_height + priv->space;
			lr->r_left = 0;
			lr->r_top = 0;
			ir->r_left = 0;
			ir->r_top = lr->r_height + priv->space;
			break;

		case NorthGravity:
			r->r_width = MAXi(lr->r_width, ir->r_width);
			r->r_height = lr->r_height + ir->r_height + priv->space;
			lr->r_left = (r->r_width - lr->r_width) / 2;
			ir->r_left = (r->r_width - ir->r_width) / 2;
			lr->r_top = 0;
			ir->r_top = lr->r_height + priv->space;
			break;

		case NorthEastGravity:
			r->r_width = MAXi(lr->r_width, ir->r_width);
			r->r_height = lr->r_height + ir->r_height + priv->space;
			lr->r_left = r->r_width - lr->r_width;
			ir->r_left = r->r_width - ir->r_width;
			lr->r_top = 0;
			ir->r_top = lr->r_height + priv->space;
			break;

		case WestGravity:
			r->r_width = lr->r_width + ir->r_width + priv->space;
			r->r_height = MAXi(lr->r_height, ir->r_height);
			lr->r_left = 0;
			lr->r_top = (r->r_height - lr->r_height) / 2;
			ir->r_left = lr->r_width + priv->space;
			ir->r_top = (r->r_height - ir->r_height) / 2;
			break;

		case CenterGravity:
			r->r_width = MAXi(lr->r_width, ir->r_width);
			r->r_height = MAXi(lr->r_height, ir->r_height);
			lr->r_left = (r->r_width - lr->r_width) / 2;
			lr->r_top = (r->r_height - lr->r_height) / 2;
			ir->r_left = (r->r_width - ir->r_width) / 2;
			ir->r_top = (r->r_height - ir->r_height) / 2;
			break;

		case EastGravity:
			r->r_width = lr->r_width + ir->r_width + priv->space;
			r->r_height = MAXi(lr->r_height, ir->r_height);
			lr->r_left = ir->r_width + priv->space;
			lr->r_top = (r->r_height - lr->r_height) / 2;
			ir->r_left = 0;
			ir->r_top = (r->r_height - ir->r_height) / 2;
			break;

		case SouthWestGravity:
			r->r_width = MAXi(lr->r_width, ir->r_width);
			r->r_height = lr->r_height + ir->r_height + priv->space;
			lr->r_left = 0;
			lr->r_top = ir->r_height + priv->space;
			ir->r_left = 0;
			ir->r_top = 0;
			break;

		case SouthGravity:
			r->r_width = MAXi(lr->r_width, ir->r_width);
			r->r_height = lr->r_height + ir->r_height + priv->space;
			lr->r_left = (r->r_width - lr->r_width) / 2;
			ir->r_left = (r->r_width - ir->r_width) / 2;
			lr->r_top = ir->r_height + priv->space;
			ir->r_top = 0;
			break;

		case SouthEastGravity:
			r->r_width = MAXi(lr->r_width, ir->r_width);
			r->r_height = lr->r_height + ir->r_height + priv->space;
			lr->r_left = r->r_width - lr->r_width;
			ir->r_left = r->r_width - ir->r_width;
			lr->r_top = ir->r_height + priv->space;
			ir->r_top = 0;
			break;

		default:
			break;
	}
}

static Xv_opaque graphobj_set(Xv_opaque self, Attr_avlist avlist)
{
	Attr_attribute *attrs;
	Graphobj_private *priv = GROBJPRIV(self);
	int need_rect = FALSE;
	int need_label_rect = FALSE;

	for (attrs=avlist; *attrs; attrs=attr_next(attrs)) switch ((int)*attrs) {
		case XV_X:
			priv->rect.r_left = (int)A1;
			ADONE;

		case XV_Y:
			priv->rect.r_top = (int)A1;
			ADONE;

		case XV_LABEL:
			if (priv->label) xv_free(priv->label);
			priv->label = (char *)0;
			if (A1) priv->label = xv_strsave((char *)A1);
			need_label_rect = TRUE;
			ADONE;

		case XV_SHOW:
			priv->state.visible = (unsigned)A1;
			ADONE;

		case XV_END_CREATE:
			xv_set(GRAPHPUB(priv->owner), GRAPH_INSERT_OBJ, self, NULL);
			break;

		case GRAPH_CLIENT_DATA:
			priv->client_data = (Xv_opaque)A1;
			ADONE;

		case GRAPH_PAINT:
			repaint_obj(priv->owner, priv);
			ADONE;

		case GRAPH_NO_REDISPLAY_ITEM:
			priv->no_redisplay = (char)A1;
			ADONE;

		case GRAPH_IMAGE_RECT:
			priv->image_rect = *(Rect *)A1;
			need_rect = TRUE;
			ADONE;

		case GRAPH_LABEL_GRAVITY:
			priv->label_gravity = (int)A1;
			need_rect = TRUE;
			ADONE;

		case GRAPH_DRAW:
			graphobj_draw(priv,
						(Graphobj_repaint_struct *)A1,
						priv->owner->ascent);
			ADONE;

		case GRAPH_SELECTED:
			priv->state.selected = (unsigned)A1;
			ADONE;

		case GRAPH_MOVABLE:
			priv->state.movable = (unsigned)A1;
			ADONE;

		case GRAPH_DRAGGABLE:
			priv->state.draggable = (unsigned)A1;
			ADONE;

		default:
			xv_check_bad_attr(GRAPHWIN, A0);
	}

	if (need_label_rect) {
		make_label_rect(priv);
		need_rect = TRUE;
	}

	if (need_rect) make_rect(priv);

	return XV_OK;
}

static Xv_opaque graphobj_get(Xv_opaque self, int *status, Attr_attribute attr,
								va_list vali)
{
	Graphobj_private *priv = GROBJPRIV(self);

	*status = XV_OK;
	switch ((int)attr) {
		case GRAPH_CLIENT_DATA: return priv->client_data;
		case GRAPH_NO_REDISPLAY_ITEM: return (Xv_opaque)priv->no_redisplay;
		case GRAPH_SELECTED: return (Xv_opaque)priv->state.selected;
		case GRAPH_MOVABLE: return (Xv_opaque)priv->state.movable;
		case GRAPH_DRAGGABLE: return (Xv_opaque)priv->state.draggable;
		case GRAPH_ENTER_BEFORE: return (Xv_opaque)FALSE;
		case XV_SHOW: return (Xv_opaque)priv->state.visible;
		case XV_LABEL: return (Xv_opaque)priv->label;
		case XV_X: return (Xv_opaque)priv->rect.r_left;
		case XV_Y: return (Xv_opaque)priv->rect.r_top;
		case XV_WIDTH: return (Xv_opaque)priv->rect.r_width;
		case XV_HEIGHT: return (Xv_opaque)priv->rect.r_height;
		case XV_RECT: return (Xv_opaque)&priv->rect;
		case GRAPH_CATCH: return (Xv_opaque)TRUE;
		case GRAPH_NEXT_ITEM:
			if (priv->next) return GROBJPUB(priv->next);
			else return XV_NULL;
		default:
			*status = xv_check_bad_attr(GRAPHWIN, attr);
			return (Xv_opaque)XV_OK;
	}
}

static int graphobj_init(Xv_opaque owner, Xv_opaque xself, Attr_avlist avlist,
					int *unused)
{
	Xv_graphobj *self = (Xv_graphobj *)xself;
	Graphobj_private *priv;

	priv = (Graphobj_private *)xv_alloc(Graphobj_private);
	if (!priv) return XV_ERROR;

	priv->public_self = xself;
	self->private_data = (Xv_opaque)priv;

	priv->owner = GRAPHPRIV(owner);
	priv->label_gravity = SouthGravity;
	priv->space = 4;
	priv->state.visible = TRUE;
	priv->state.movable = TRUE;

	return XV_OK;
}

static int graphobj_destroy(Xv_opaque self, Destroy_status status)
{
	if (status == DESTROY_CLEANUP) {
		Graphobj_private *priv = GROBJPRIV(self);

		xv_set(xv_get(self, XV_OWNER), GRAPH_REMOVE_OBJ, self, NULL);
		if (priv->label) xv_free(priv->label);
		free((char *)priv);
	}
	return XV_OK;
}

/****************************************************************/
/**********           painter                     ***************/
/****************************************************************/

#define ST_INVERS	0x00000001
#define ST_XOR		0x00000002

typedef enum {
	st_nothing = 0,
	st_invers = ST_INVERS,
	st_xor = ST_XOR,
	st_xor_invers = ST_XOR | ST_INVERS
} painter_state_t;


static void draw_point(Graphpaint_private *priv, int x, int y)
{
	int wx, wy;

	if (! priv->vi) return;

	switch ((painter_state_t)priv->state) {
		case st_nothing:			break;
		case st_invers:				break;
		case st_xor:				break;
		case st_xor_invers:			return;
	}

	wx = SCR_WIN_X(priv->vi, x);
	wy = SCR_WIN_Y(priv->vi, y);
	XDrawPoint(priv->vi->dpy, priv->vi->xid, priv->current_gc, wx, wy);
}

static void draw_line(Graphpaint_private *priv, int x1, int y1, int x2, int y2)
{
	int xa,ya,xe,ye;

	if (! priv->vi) return;

	switch ((painter_state_t)priv->state) {
		case st_nothing:			break;
		case st_invers:				break;
		case st_xor:				break;
		case st_xor_invers:			return;
	}

	xa = SCR_WIN_X(priv->vi, x1);
	ya = SCR_WIN_Y(priv->vi, y1);
	xe = SCR_WIN_X(priv->vi, x2);
	ye = SCR_WIN_Y(priv->vi, y2);
	XDrawLine(priv->vi->dpy, priv->vi->xid, priv->current_gc, xa,ya, xe,ye);
}

static void draw_segments(Graphpaint_private *priv, XSegment *seg, int nseg)
{
	XSegment *s;
	int i;

	if (! priv->vi) return;

	switch ((painter_state_t)priv->state) {
		case st_nothing:			break;
		case st_invers:				break;
		case st_xor:				break;
		case st_xor_invers:			return;
	}

	for (s = seg, i = 0; i < nseg; i++, s++) {
		s->x1 = SCR_WIN_X(priv->vi, s->x1);
		s->x2 = SCR_WIN_X(priv->vi, s->x2);
		s->y1 = SCR_WIN_Y(priv->vi, s->y1);
		s->y2 = SCR_WIN_Y(priv->vi, s->y2);
	}

	XDrawSegments(priv->vi->dpy, priv->vi->xid, priv->current_gc, seg, nseg);
}

static void draw_string(Graphpaint_private *priv, int x, int y, char *string,
								int len)
{
	int wx,wy,string_len;

	if (! priv->vi) return;

	if (!string || !*string) {
		return;
	}

	switch ((painter_state_t)priv->state) {
		case st_nothing:			break;
		case st_invers:				break;
		case st_xor:				break;
		case st_xor_invers:			return;
	}

	if (len == -1) string_len = strlen(string);
	else string_len = len;

	wx = SCR_WIN_X(priv->vi, x);
	wy = SCR_WIN_Y(priv->vi, y) + priv->ownerpriv->ascent;

	XDrawString(priv->vi->dpy, priv->vi->xid, priv->current_gc, wx, wy,
											string, string_len);
}

static void draw_opaque_string(Graphpaint_private *priv, int x, int y,
								char *string, int len)
{
	int wx,wy;
	int string_len;

	if (! priv->vi) return;

	if (!string || !*string) {
		return;
	}

	switch ((painter_state_t)priv->state) {
		case st_nothing:			break;
		case st_invers:				break;
		case st_xor:
				draw_string(priv, x, y, string, len);
				return;
		case st_xor_invers:			return;
	}

	if (len == -1) string_len = strlen(string);
	else string_len = len;

	wx = SCR_WIN_X(priv->vi, x);
	wy = SCR_WIN_Y(priv->vi, y) + priv->ownerpriv->ascent;

	XDrawImageString(priv->vi->dpy, priv->vi->xid, priv->current_gc, wx, wy,
											string, string_len);
}

static void draw_rectangle(Graphpaint_private *priv, int left, int top,
								int width, int height)
{
	int dx,dy;
	unsigned dh,dw;

	if (! priv->vi) return;

	switch ((painter_state_t)priv->state) {
		case st_nothing:          break;
		case st_invers:           break;
		case st_xor:              break;
		case st_xor_invers:       return;
	}

	dx = SCR_WIN_X(priv->vi, left);
	dy = SCR_WIN_Y(priv->vi, top);
	dw = SCR_WIN_SIZE(priv->vi, width);
	dh = SCR_WIN_SIZE(priv->vi, height);
	
    if (dw<1) dw=1;
    if (dh<1) dh=1;
    XDrawRectangle(priv->vi->dpy, priv->vi->xid, priv->current_gc, dx,dy, dw,dh);
}

static void fill_rectangle(Graphpaint_private *priv, int left, int top,
								int width, int height)
{
	int dx,dy;
	unsigned dh,dw;

	if (! priv->vi) return;

	switch ((painter_state_t)priv->state) {
		case st_nothing:			break;
		case st_invers:				break;
		case st_xor:				break;
		case st_xor_invers:			return;
	}

	dx = SCR_WIN_X(priv->vi, left);
	dy = SCR_WIN_Y(priv->vi, top);
	dw = SCR_WIN_SIZE(priv->vi, width);
	dh = SCR_WIN_SIZE(priv->vi, height);
    
    if (dw<1) dw=1;
    if (dh<1) dh=1;
	XFillRectangle(priv->vi->dpy, priv->vi->xid, priv->current_gc, dx,dy, dw,dh);
}

static void draw_opaque_rectangle(Graphpaint_private *priv, int left, int top,
								int width, int height)
{
	int dx,dy;
	unsigned dh,dw;

	if (! priv->vi) return;

	switch ((painter_state_t)priv->state) {
		case st_nothing: break;
		case st_invers: /* ? ? */ return;
		case st_xor:
			draw_rectangle(priv, left, top, width, height);
			return;
		case st_xor_invers: return;
	}

	dx = SCR_WIN_X(priv->vi, left);
	dy = SCR_WIN_Y(priv->vi, top);
	dw = SCR_WIN_SIZE(priv->vi, width);
	dh = SCR_WIN_SIZE(priv->vi, height);
    if (dw<1) dw=1;
    if (dh<1) dh=1;

	XFillRectangle(priv->vi->dpy, priv->vi->xid, priv->ownerpriv->sel_draw_gc,
						dx+1,dy+1, dw,dh);
	XDrawRectangle(priv->vi->dpy, priv->vi->xid, priv->current_gc, dx,dy, dw,dh);
}

static void convert_points(Graphpaint_private *priv, XPoint *points,
								int npoints, int reltoprev)
{
	XPoint *p;
	int i;

	if (! priv->vi) return;

	points->x = SCR_WIN_X(priv->vi, points->x);
	points->y = SCR_WIN_Y(priv->vi, points->y);

	if (reltoprev) {
		for (p = points+1, i = 1; i < npoints; i++, p++) {
			p->x = SCR_WIN_SIZE(priv->vi, p->x);
			p->y = SCR_WIN_SIZE(priv->vi, p->y);
		}
	}
	else {
		for (p = points+1, i = 1; i < npoints; i++, p++) {
			p->x = SCR_WIN_X(priv->vi, p->x);
			p->y = SCR_WIN_Y(priv->vi, p->y);
		}
	}
}

static void draw_polygon(Graphpaint_private *priv, XPoint *points, int npoints,
								int reltoprev)
{
	if (! priv->vi) return;

	switch ((painter_state_t)priv->state) {
		case st_nothing:          break;
		case st_invers:           break;
		case st_xor:              break;
		case st_xor_invers:       return;
	}

	convert_points(priv, points, npoints, reltoprev);

	XDrawLines(priv->vi->dpy, priv->vi->xid, priv->current_gc,
			points, npoints,
			reltoprev ? CoordModePrevious:CoordModeOrigin);
}

static void fill_polygon(Graphpaint_private *priv, XPoint *points, int npoints,
								int reltoprev)
{
	if (! priv->vi) return;

	switch ((painter_state_t)priv->state) {
		case st_nothing:			break;
		case st_invers:				break;
		case st_xor:				break;
		case st_xor_invers:			return;
	}

	convert_points(priv, points, npoints, reltoprev);

	XFillPolygon(priv->vi->dpy, priv->vi->xid, priv->current_gc,
			points, npoints, Complex,
			reltoprev ? CoordModePrevious:CoordModeOrigin);
}

static void draw_opaque_polygon(Graphpaint_private *priv, XPoint *points,
								int npoints, int reltoprev)
{
	if (! priv->vi) return;

	switch ((painter_state_t)priv->state) {
		case st_nothing:			break;
		case st_invers: /* ? ? */	return;
		case st_xor:
			draw_polygon(priv, points, npoints, reltoprev);
			return;
		case st_xor_invers:			return;
	}

	convert_points(priv, points, npoints, reltoprev);

	XFillPolygon(priv->vi->dpy, priv->vi->xid, priv->ownerpriv->sel_draw_gc,
				points, npoints, Complex,
				reltoprev ? CoordModePrevious: CoordModeOrigin);

	XDrawLines(priv->vi->dpy, priv->vi->xid, priv->current_gc,
				points, npoints,
				reltoprev ? CoordModePrevious: CoordModeOrigin);
}

static void draw_arc(Graphpaint_private *priv, int x, int y,
								int width, int height, int angle1, int angle2)
{
	int dx,dy;
	unsigned dh,dw;
	int wa,we;

	if (! priv->vi) return;

	switch ((painter_state_t)priv->state) {
		case st_nothing:          break;
		case st_invers:           break;
		case st_xor:              break;
		case st_xor_invers:       return;
	}

	dx = SCR_WIN_X(priv->vi, x);
	dy = SCR_WIN_Y(priv->vi, y);
	dw = SCR_WIN_SIZE(priv->vi, width);
	dh = SCR_WIN_SIZE(priv->vi, height);

	wa = (int)(64 * angle1);
	we = (int)(64 * angle2);

	XDrawArc(priv->vi->dpy, priv->vi->xid, priv->current_gc, dx, dy, dw, dh, wa, we );
}

static void fill_arc(Graphpaint_private *priv, int x, int y,
								int width, int height, int angle1, int angle2)
{
	int dx,dy;
	unsigned dh,dw;
	int wa,we;

	if (! priv->vi) return;

	switch ((painter_state_t)priv->state) {
		case st_nothing:			break;
		case st_invers:				break;
		case st_xor:				break;
		case st_xor_invers:			return;
	}

	dx = SCR_WIN_X(priv->vi, x);
	dy = SCR_WIN_Y(priv->vi, y);
	dw = SCR_WIN_SIZE(priv->vi, width);
	dh = SCR_WIN_SIZE(priv->vi, height);

	wa = (int)(64 * angle1);
	we = (int)(64 * angle2);

	XFillArc(priv->vi->dpy, priv->vi->xid, priv->current_gc, dx, dy, dw, dh, wa, we );
}

static void draw_opaque_arc(Graphpaint_private *priv, int x, int y,
								int width, int height, int angle1, int angle2)
{
	int dx,dy;
	unsigned dh,dw;
	int wa,we;

	if (! priv->vi) return;

	switch ((painter_state_t)priv->state) {
		case st_nothing:			break;
		case st_invers: /* ? ? */	return;
		case st_xor:
			draw_arc(priv, x,y,width,height,angle1,angle2);
			return;
		case st_xor_invers:			return;
	}

	dx = SCR_WIN_X(priv->vi, x);
	dy = SCR_WIN_Y(priv->vi, y);
	dw = SCR_WIN_SIZE(priv->vi, width);
	dh = SCR_WIN_SIZE(priv->vi, height);

	wa = (int)(64 * angle1);
	we = (int)(64 * angle2);


	XFillArc(priv->vi->dpy, priv->vi->xid, priv->ownerpriv->sel_draw_gc,
								dx, dy, dw, dh, wa, we );
	XDrawArc(priv->vi->dpy, priv->vi->xid, priv->current_gc,
								dx, dy, dw, dh, wa, we );
}

static Xv_opaque graphpaint_set(Xv_opaque self, Attr_avlist avlist)
{
	Attr_attribute *attrs;
	Graphpaint_private *priv = GRPAINTPRIV(self);

	for (attrs=avlist; *attrs; attrs=attr_next(attrs)) switch ((int)*attrs) {
		case GRAPH_USE_NORMAL_CONTEXT:
			priv->state &= (~ST_INVERS);

			if (! (priv->state & ST_XOR))
				priv->current_gc = priv->ownerpriv->draw_gc;
			ADONE;

		case GRAPH_USE_INVERS_CONTEXT:
			priv->state |= ST_INVERS;
			if (! (priv->state & ST_XOR))
				priv->current_gc = priv->ownerpriv->sel_draw_gc;
			ADONE;

		case GRAPH_USE_NON_STANDARD_CONTEXT:
			if (! (priv->state & ST_XOR)) priv->current_gc = (GC)A1;
			ADONE;
		case GRAPH_DRAW_POINT:
			draw_point(priv, (int)A1, (int)A2);
			ADONE;
		case GRAPH_DRAW_LINE:
			draw_line(priv, (int)A1, (int)A2, (int)A3, (int)A4);
			ADONE;
		case GRAPH_DRAW_LINES:
			draw_segments(priv, (XSegment *)A1, (int)A2);
			ADONE;
		case GRAPH_DRAW_RECTANGLE:
			draw_rectangle(priv, (int)A1, (int)A2, (int)A3, (int)A4);
			ADONE;
		case GRAPH_FILL_RECTANGLE:
			fill_rectangle(priv, (int)A1, (int)A2, (int)A3, (int)A4);
			ADONE;
		case GRAPH_DRAW_OPAQUE_RECTANGLE:
			draw_opaque_rectangle(priv, (int)A1, (int)A2, (int)A3, (int)A4);
			ADONE;
		case GRAPH_DRAW_POLYGON:
			draw_polygon(priv, (XPoint *)A1, (int)A2, (int)A3);
			ADONE;
		case GRAPH_FILL_POLYGON:
			fill_polygon(priv, (XPoint *)A1, (int)A2, (int)A3);
			ADONE;
		case GRAPH_DRAW_OPAQUE_POLYGON:
			draw_opaque_polygon(priv, (XPoint *)A1, (int)A2, (int)A3);
			ADONE;
		case GRAPH_DRAW_ARC:
			draw_arc(priv, (int)A1, (int)A2, (int)A3, (int)A4, (int)A5,(int)A6);
			ADONE;
		case GRAPH_FILL_ARC:
			fill_arc(priv, (int)A1, (int)A2, (int)A3, (int)A4, (int)A5,(int)A6);
			ADONE;
		case GRAPH_DRAW_OPAQUE_ARC:
			draw_opaque_arc(priv,(int)A1,(int)A2,(int)A3,(int)A4,
											(int)A5,(int)A6);
			ADONE;
		case GRAPH_DRAW_STRING:
			draw_string(priv, (int)A1, (int)A2, (char *)A3, (int)A4);
			ADONE;
		case GRAPH_DRAW_OPAQUE_STRING:
			draw_opaque_string(priv, (int)A1, (int)A2, (char *)A3, (int)A4);
			ADONE;
		case GRAPH_START_XOR_DRAWING:
			priv->state |= ST_XOR;
			priv->current_gc = priv->ownerpriv->frame_gc;
			ADONE;
		case GRAPH_END_XOR_DRAWING:
			priv->state &= (~ST_XOR);
			priv->current_gc = priv->ownerpriv->draw_gc;
			ADONE;

		case XV_END_CREATE:
			priv->current_gc = priv->ownerpriv->draw_gc;
			break;

		default:
			xv_check_bad_attr(GRAPHWIN, A0);
	}

	return XV_OK;
}

static Xv_opaque graphpaint_get(Xv_opaque self, int *status,
								Attr_attribute attr, va_list vali)
{
/* 	Graphpaint_private *priv = GRPAINTPRIV(self); */

	*status = XV_OK;
	switch ((int)attr) {
		default:
			*status = xv_check_bad_attr(GRAPHWIN, attr);
			return (Xv_opaque)XV_OK;
	}
}

static int graphpaint_init(Xv_opaque owner, Xv_opaque xself, Attr_avlist avlist,
								int *unused)
{
	Xv_graphobj *self = (Xv_graphobj *)xself;
	Graphpaint_private *priv;

	priv = (Graphpaint_private *)xv_alloc(Graphpaint_private);
	if (!priv) return XV_ERROR;

	priv->public_self = (Xv_opaque)self;
	self->private_data = (Xv_opaque)priv;
	priv->ownerpriv = GRAPHPRIV(owner);
	priv->ownerpriv->painter = priv;

	return XV_OK;
}

static int graphpaint_destroy(Xv_opaque self, Destroy_status status)
{
	if (status == DESTROY_CLEANUP) {
		Graphpaint_private *priv = GRPAINTPRIV(self);

		free((char *)priv);
	}
	return XV_OK;
}

const Xv_pkg xv_graphwin_pkg = {
	"GraphicsWindow",
	ATTR_PKG_GRAPHWIN,
	sizeof(Xv_graphwin),
	SCROLLWIN,
	graphwin_init,
	graphwin_set,
	graphwin_get,
	graphwin_destroy,
	0
};

const Xv_pkg xv_graphobj_pkg = {
	"GraphicsObject",
	ATTR_PKG_GRAPHWIN,
	sizeof(Xv_graphobj),
	XV_GENERIC_OBJECT,
	graphobj_init,
	graphobj_set,
	graphobj_get,
	graphobj_destroy,
	0
};

const Xv_pkg xv_graphpainter_pkg = {
	"GraphicsPainter",
	ATTR_PKG_GRAPHWIN,
	sizeof(Xv_graphpainter),
	XV_GENERIC_OBJECT,
	graphpaint_init,
	graphpaint_set,
	graphpaint_get,
	graphpaint_destroy,
	0
};
