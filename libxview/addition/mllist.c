/*
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
#include <xview/font.h>
#include <xview/cms.h>
#include <olgx/olgx.h>
#include <xview/mllist.h>
#include <xview/defaults.h>
#include <xview_private/i18n_impl.h>
#include <xview_private/attr_impl.h>
#include <xview_private/panel_impl.h>
#include <xview_private/svr_impl.h>

#ifndef lint
char mllist_c_sccsid[] = "@(#) %M% V%I% %E% %U% $Id: mllist.c,v 1.36 2025/04/01 19:55:08 dra Exp $";
#endif

/*******************************************************************
 Achtung: NICHT nochmal versuchen, den TITLE von der Liste selber
 machen zu lassen: man kriegt dann KEIN Level-Menue zu sehen,
 da diese Events dann im Rect der Liste waeren, ausserdem würde die
 Hoehe fuer die Level-Marker nicht ausreichen
********************************************************************/

extern Graphics_info *xv_init_olgx(Xv_Window, int *, Xv_Font);

#define DTL_MENU 700
#define DOT_PMWID 12

/* bits for sbstate */
#define NO_BWD OLGX_SCROLL_NO_BACKWARD
#define NO_FWD OLGX_SCROLL_NO_FORWARD
#define BWD OLGX_SCROLL_BACKWARD
#define FWD OLGX_SCROLL_FORWARD

typedef int (*verify_proc_t)(Multi_level_list, mllist_ptr,
									Proplist_verify_op op, mllist_ptr);
typedef void (*free_proc_t)(Multi_level_list, mllist_ptr);

typedef struct {
	mllist_ptr parent_of_level;
	Menu_item levelitem;
	int levelitem_visible;
	int selected;
} level_descr;

typedef struct {
	Xv_opaque               public_self;

	verify_proc_t           appl_verify;
	free_proc_t             appl_free;
	Panel_item              list; /* a PANEL_LIST or a PROP_LIST */
	Menu                    levelmenu;
	level_descr            *levelstack;
	int                     stackptr;
	mllist_ptr              root;
	int                     double_time, sbstate;
	Graphics_info           *ginfo;
	Display                 *dpy;
	GC                      gc;
	int                     color_offset, three_d;
	Server_image            dot, nodot;
	Window_rescale_state    scale;
	Xv_font                 listfont;
	time_t                  lasttime;
	/* the window for the horizontal title line that hides the 
	 * top border of the PANEL_LIST and the bottom border of the
	 * self-drawn title box. Yes, this seems a little complicated,
	 * but it was dificult to find out when priv->list draws the border...
	 */
	Xv_window               bar_window;
	Window                  panxid;
	Rect                    sbr;
	char                    previewing, created, over_list;
} MultiLevelList_private;

#define MLLPRIV(_x_) XV_PRIVATE(MultiLevelList_private, Xv_multi_level_list, _x_)
#define MLLPUB(_x_) XV_PUBLIC(_x_)

#define A0 *attrs
#define A1 attrs[1]
#define A2 attrs[2]
#define OVER_UNKNOWN -1
#define STACKDEPTH 100

#define ADONE ATTR_CONSUME(*attrs);break

static struct {
	unsigned dot_diam;
	int dot_dist;
	int dot_vert_offset;
	int dot_hor_offset;
	int title_vert_offset;
	int sb_dist;
	int sb_shift;
} ol_mll_sizes[] = {
	{ 4, 8, 8, 17, 23, 4, 2 },
	{ 5, 10, 9, 19, 27, 4, 2 },
	{ 6, 12, 11, 22, 31, 6, 3 },
	{ 8, 16, 13, 28, 39, 8, 4 }
};

static int mllist_key = 0;

static void draw_sb(MultiLevelList_private *priv, int flags)
{
	olgx_draw_scrollbar(priv->ginfo, priv->panxid,
							priv->sbr.r_left,
							priv->sbr.r_top,
							priv->sbr.r_height,
							priv->sbr.r_top, /* elev_pos */
							priv->sbr.r_top, /* old_elev_pos */
							priv->sbr.r_top, /* prop_pos */
							0, /* prop_len */
							OLGX_VERTICAL | OLGX_ABBREV | OLGX_UPDATE |
							OLGX_ERASE | priv->sbstate | flags);
}

static void draw_title(MultiLevelList_private *priv)
{
	Rect *r, rect;
	int i, x;

	r = (Rect *)xv_get(MLLPUB(priv), PANEL_ITEM_VALUE_RECT);
	rect = *r;
	x = rect.r_left + ol_mll_sizes[(int)priv->scale].dot_hor_offset;

	XClearArea(priv->dpy, priv->panxid, rect.r_left + 2, rect.r_top + 2,
					(unsigned)(rect.r_width - priv->sbr.r_width - 4 -
							priv->color_offset -
							ol_mll_sizes[(int)priv->scale].sb_dist),
					(unsigned)(priv->sbr.r_height - 2), FALSE);

	olgx_draw_text(priv->ginfo, priv->panxid,
			priv->levelstack[priv->stackptr].parent_of_level->label,
			x, rect.r_top + ol_mll_sizes[(int)priv->scale].title_vert_offset,
			rect.r_width - priv->sbr.r_width - 1 -
					priv->color_offset -
					ol_mll_sizes[(int)priv->scale].sb_dist -
					ol_mll_sizes[(int)priv->scale].dot_hor_offset,
			OLGX_NORMAL | OLGX_MORE_ARROW);

	for (i = 0; i < priv->stackptr; i++) {
		XFillArc(priv->dpy, priv->panxid, priv->gc,
				x, rect.r_top + ol_mll_sizes[(int)priv->scale].dot_vert_offset,
				ol_mll_sizes[(int)priv->scale].dot_diam,
				ol_mll_sizes[(int)priv->scale].dot_diam,
				0, 64 * 360);
		XDrawArc(priv->dpy, priv->panxid, priv->gc,
				x, rect.r_top + ol_mll_sizes[(int)priv->scale].dot_vert_offset,
				ol_mll_sizes[(int)priv->scale].dot_diam,
				ol_mll_sizes[(int)priv->scale].dot_diam,
				0, 64 * 360);

		x += ol_mll_sizes[(int)priv->scale].dot_dist;
	}

	if (priv->three_d) {
		olgx_draw_box(priv->ginfo, priv->panxid, rect.r_left, rect.r_top,
					rect.r_width - priv->sbr.r_width -
							priv->color_offset -
							ol_mll_sizes[(int)priv->scale].sb_dist,
					priv->sbr.r_height + 2, OLGX_INVOKED, FALSE);
		olgx_draw_box(priv->ginfo, priv->panxid, rect.r_left+1, rect.r_top+1,
					rect.r_width - priv->sbr.r_width -
							priv->color_offset -
							ol_mll_sizes[(int)priv->scale].sb_dist - 2,
					priv->sbr.r_height, OLGX_NORMAL, FALSE);
		XDrawLine(priv->dpy, priv->panxid,
					priv->ginfo->gc_rec[OLGX_WHITE]->gc,
					rect.r_left+1, rect.r_top + priv->sbr.r_height,
					rect.r_left+1, rect.r_top + priv->sbr.r_height + 4);
		XDrawLine(priv->dpy, priv->panxid,
					priv->ginfo->gc_rec[OLGX_BG3]->gc,
					rect.r_left + rect.r_width - priv->sbr.r_width -
							priv->color_offset -
							ol_mll_sizes[(int)priv->scale].sb_dist - 2,
					rect.r_top + priv->sbr.r_height,
					rect.r_left + rect.r_width - priv->sbr.r_width -
							priv->color_offset -
							ol_mll_sizes[(int)priv->scale].sb_dist - 2,
					rect.r_top + priv->sbr.r_height + 4);
	}
	else {
		olgx_draw_box(priv->ginfo, priv->panxid, rect.r_left, rect.r_top,
					rect.r_width - priv->sbr.r_width -
							priv->color_offset -
							ol_mll_sizes[(int)priv->scale].sb_dist,
					priv->sbr.r_height + 2, OLGX_NORMAL, FALSE);
	}
}

static void barwin_event(Xv_window win, Event *ev)
{
	if (event_action(ev) == WIN_REPAINT) {
		MultiLevelList_private *priv =
					(MultiLevelList_private *)xv_get(win, WIN_CLIENT_DATA);
		int x;

		x = ol_mll_sizes[(int)priv->scale].dot_hor_offset / 2;

		XDrawLine(priv->dpy, (Window)xv_get(win, XV_XID), priv->gc,
				x, 0, (int)xv_get(win, XV_WIDTH) - x, 0);
	}
/* 	else if (event_action(ev) == ACTION_MENU && event_is_down(ev)) { */
/* 		MultiLevelList_private *priv = */
/* 					(MultiLevelList_private *)xv_get(win, WIN_CLIENT_DATA); */
/* 		panel_accept_menu(priv->list, ev); */
/* 	} */
}

static void mllist_paint(Panel_item item, Panel_setting u)
{
	MultiLevelList_private *priv = MLLPRIV(item);

	panel_paint_label(item);
	panel_paint(priv->list, PANEL_NO_CLEAR);

	draw_title(priv);
	draw_sb(priv, 0);
}

static void own_free_sublevel(MultiLevelList_private *, mllist_ptr);

static void own_free(Property_list l, mllist_ptr item)
{
	MultiLevelList_private *priv = (MultiLevelList_private *)xv_get(l,
													XV_KEY_DATA, mllist_key);

	if (item->label) {
		xv_free(item->label);
		item->label = (char *)0;
	}
	if (priv->appl_free) (*(priv->appl_free))(MLLPUB(priv), item);
	own_free_sublevel(priv, item->firstchild);
	item->firstchild = (mllist_ptr)0;
}

static void own_free_sublevel(MultiLevelList_private *priv, mllist_ptr item)
{
	if (! item) return;
	own_free_sublevel(priv, item->next);
	own_free(priv->list, item);
	xv_free((char *)item);
}

static void save_current_level(MultiLevelList_private *priv)
{
	int i, num;
	mllist_ptr last = 0, p, curlev;
	level_descr *stack = priv->levelstack;
	int sp = priv->stackptr;

	num = (int)xv_get(priv->list, PANEL_LIST_NROWS);
	curlev = stack[sp].parent_of_level;
	curlev->firstchild = (mllist_ptr)0;
	for (i = 0; i < num; i++) {
		p = (mllist_ptr)xv_get(priv->list, PANEL_LIST_CLIENT_DATA, i);
		if (curlev->firstchild) {
			last->next = p;
			p->next = (mllist_ptr)0;
			last = p;
		}
		else curlev->firstchild = last = p;

		p->next = (mllist_ptr)0;
		p->parent = curlev;
	}

	i = (int)xv_get(priv->list, PANEL_LIST_FIRST_SELECTED);
	stack[sp].selected = (i >= 0 ? i : 0);

	for (i = 0; i < STACKDEPTH; i++) {
		if (stack[sp].levelitem) {
			xv_set(stack[sp].levelitem, MENU_INACTIVE, FALSE, NULL);
		}
	}
}

static void set_list_title(MultiLevelList_private *priv)
{
	Menu_item nmi;
	level_descr *stack = priv->levelstack;
	int sp = priv->stackptr;
	mllist_ptr lev = stack[sp].parent_of_level;

	nmi = stack[sp].levelitem;
	if (! nmi) {
		nmi = xv_create(XV_NULL, MENUITEM,
					MENU_RELEASE,
					MENU_RELEASE_IMAGE,
					MENU_STRING, xv_strsave(" "),
					MENU_CLIENT_DATA, sp,
					NULL);
		stack[sp].levelitem = nmi;
		stack[sp].levelitem_visible = FALSE;
	}

	xv_set(nmi,
			MENU_STRING, xv_strsave(lev->label),
			MENU_INACTIVE, TRUE,
			NULL);

	if (! stack[sp].levelitem_visible) {
		xv_set(priv->levelmenu, MENU_APPEND_ITEM, nmi, NULL);
		stack[sp].levelitem_visible = TRUE;
	}

	draw_title(priv);
}

static void level_down(MultiLevelList_private *priv, mllist_ptr item)
{
	++priv->stackptr;
	priv->levelstack[priv->stackptr].parent_of_level = item;
	set_list_title(priv);
}

static void level_up(MultiLevelList_private *priv)
{
	--priv->stackptr;
	set_list_title(priv);
}

#define PL_LEN 4

static void update_list(MultiLevelList_private *priv, int paint_panel)
{
	level_descr *stack = priv->levelstack;
	int sp = priv->stackptr;
	mllist_ptr curlev, p;
	int num, i, j, sel;
	int has_sublevel = FALSE;
	Attr_avlist attrs;

	sel = stack[sp].selected;
	curlev = stack[sp].parent_of_level;
	for (num = 0, p = curlev->firstchild; p; num++, p = p->next) {
		if (p->has_sublevel) has_sublevel = TRUE;
	}

	attrs = xv_alloc_n(Attr_attribute, (unsigned long)(PL_LEN * num + 20));
	i = 0;

	if (xv_get(priv->list, PANEL_LIST_NROWS) != 0) {
		attrs[i++] = (Attr_attribute)PANEL_LIST_DELETE_ROWS;
		attrs[i++] = 0L;
		attrs[i++] = xv_get(priv->list, PANEL_LIST_NROWS);
	}

	if (num > 0) {
		attrs[i++] = (Attr_attribute)PANEL_LIST_STRINGS;
		for (p = curlev->firstchild; p; p = p->next)
			attrs[i++] = (Attr_attribute)p->label;
		attrs[i++] = XV_NULL;

		attrs[i++] = (Attr_attribute)PANEL_LIST_GLYPHS;
		for (j = 0, p = curlev->firstchild; p; j++, p = p->next) {
			if (p->has_sublevel) {
				p->mllist_private[PL_GLYPH_INDEX] = priv->dot;
				attrs[i++] = (Attr_attribute)priv->dot;
			}
			else {
				p->mllist_private[PL_GLYPH_INDEX] = priv->nodot;
				attrs[i++] = (Attr_attribute)priv->nodot;
			}
		}
		attrs[i++] = XV_NULL;

		attrs[i++] = (Attr_attribute)PANEL_LIST_MASK_GLYPHS;
		for (j = 0, p = curlev->firstchild; p; j++, p = p->next) {
			if (p->has_sublevel) {
				p->mllist_private[PL_MASK_INDEX] = priv->dot;
				attrs[i++] = (Attr_attribute)priv->dot;
			}
			else {
				p->mllist_private[PL_MASK_INDEX] = priv->nodot;
				attrs[i++] = (Attr_attribute)priv->nodot;
			}
		}
		attrs[i++] = XV_NULL;

		attrs[i++] = (Attr_attribute)PANEL_LIST_CLIENT_DATAS;
		for (p = curlev->firstchild; p; p=p->next) {
			attrs[i++] = (Attr_attribute)p;
		}
		attrs[i++] = XV_NULL;

		attrs[i++] = (Attr_attribute)PANEL_LIST_SELECT;
		attrs[i++] = sel;
		attrs[i++] = TRUE;
	}
	attrs[i++] = XV_NULL;

	xv_set_avlist(priv->list, attrs);
	xv_free((char *)attrs);

	priv->sbstate = (sp > 0 ? 0 : NO_BWD);
	if (!has_sublevel) priv->sbstate |= NO_FWD;
	draw_title(priv);
	draw_sb(priv, 0);
	if (paint_panel) {
		panel_paint(xv_get(priv->list, XV_OWNER), PANEL_CLEAR);
	}
}

static void process_scroll_button(MultiLevelList_private *priv, int is_up)
{
	if (is_up && priv->stackptr <= 0) return;
	if (!is_up && (priv->sbstate & NO_FWD)) return;

	save_current_level(priv);

	if (is_up) level_up(priv);
	else {
		if (priv->levelstack[priv->stackptr+1].parent_of_level) {
			++priv->stackptr;
			set_list_title(priv);
		}
		else {
			int i, num;
			mllist_ptr item;

			num = (int)xv_get(priv->list, PANEL_LIST_NROWS);
			for (i = 0; i < num; i++) {
				item = (mllist_ptr)xv_get(priv->list, PANEL_LIST_CLIENT_DATA,i);
				if (item->has_sublevel) {
					level_down(priv, item);
					break;
				}
			}
		}
	}
	update_list(priv, TRUE);
}

static int is_over_list(MultiLevelList_private *priv, Event *ev)
{
	if (priv->over_list == OVER_UNKNOWN) {
		Rect *r = (Rect *)xv_get(priv->list, XV_RECT);

		priv->over_list = rect_includespoint(r, event_x(ev), event_y(ev));
	}

	return priv->over_list;
}

static void mllist_begin_preview(Panel_item	item, Event *ev)
{
	MultiLevelList_private *priv = MLLPRIV(item);

	priv->over_list = OVER_UNKNOWN;
	if (rect_includespoint(&priv->sbr, event_x(ev), event_y(ev))) {
		if (event_y(ev) < priv->sbr.r_top + (priv->sbr.r_height / 2))
			draw_sb(priv, BWD);
		else draw_sb(priv, FWD);
		priv->previewing = TRUE;
	}
	else if (is_over_list(priv, ev)) {
		panel_begin_preview(priv->list, ev);
	}
}

static void mllist_update_preview(Panel_item item, Event *ev)
{
	MultiLevelList_private *priv = MLLPRIV(item);

	if (priv->previewing) {
		if (rect_includespoint(&priv->sbr,event_x(ev),event_y(ev))) {
			if (event_y(ev) < priv->sbr.r_top + (priv->sbr.r_height / 2))
				draw_sb(priv, BWD);
			else draw_sb(priv, FWD);
		}
		else {
			priv->previewing = FALSE;
			draw_sb(priv, 0);
		}
	}
	else if (is_over_list(priv, ev)) {
		panel_update_preview(priv->list, ev);
	}
}

/*ARGSUSED*/
static void mllist_cancel_preview(Panel_item item, Event *ev)
{
	MultiLevelList_private *priv = MLLPRIV(item);

	/*
	 * The pointer as been dragged out of the item after
	 * beginning a preview.  Remove the active feedback
	 * (i.e., unhighlight) and clean up any private data.
	 */

	priv->previewing = FALSE;
	draw_sb(priv, 0);
	if (is_over_list(priv, ev)) {
		panel_cancel_preview(priv->list, ev);
	}
}

static void mllist_accept_preview(Panel_item item, Event *ev)
{
	MultiLevelList_private *priv = MLLPRIV(item);

	if (priv->previewing) {
		if (rect_includespoint(&priv->sbr,event_x(ev),event_y(ev))) {
			process_scroll_button(priv,
					event_y(ev) < priv->sbr.r_top + (priv->sbr.r_height / 2));
		}

		priv->previewing = FALSE;
		draw_sb(priv, 0);
	}
	else if (is_over_list(priv, ev)) {
		panel_accept_preview(priv->list, ev);
	}
}

static void mllist_accept_menu(Panel_item item, Event *ev)
{
	MultiLevelList_private *priv = MLLPRIV(item);

	if (rect_includespoint(&priv->sbr,event_x(ev),event_y(ev))) {
		menu_show(priv->levelmenu, xv_get(item, XV_OWNER), ev, NULL);
	}
	else if (is_over_list(priv, ev)) {
		panel_accept_menu(priv->list, ev);
	}
	else {
		Panel_ops *listops = (Panel_ops *)xv_get(priv->list, PANEL_OPS_VECTOR);
		/* we are in the title area - no menu with panel_accept_menu */
		/* PANEL_LIST has a NULL accept_menu, so we try: */
		(listops->panel_op_handle_event)(priv->list, ev);
	}
}


/*ARGSUSED*/
static void mllist_resize(Panel_item	    item)
{
	/*
	 * The panel has been resized.  Recalculate any extend-to-edge dimensions.
	 */
}


/*ARGSUSED*/
static void mllist_remove(Panel_item	    item)
{
	MultiLevelList_private *priv = MLLPRIV(item);

	/*
	 * The item has been made hidden via xv_set(item, XV_SHOW, FALSE, avlist).
	 */
	xv_set(priv->list, XV_SHOW, FALSE, NULL);
}


static void mllist_restore(Panel_item item, Panel_setting u)
{
	MultiLevelList_private *priv = MLLPRIV(item);

	/*
	 * The item has been made visible via xv_set(item, XV_SHOW, TRUE, avlist).
	 */
	xv_set(priv->list, XV_SHOW, TRUE, NULL);
}

/*ARGSUSED*/
static void mllist_layout(Panel_item	    item, Rect	   *deltas)
{
	MultiLevelList_private *priv = MLLPRIV(item);

	/*
	 * The item has been moved.  Adjust the item coordinates.
	 */
	if (priv->list) {
		xv_set(priv->list,
				XV_X, xv_get(priv->list, XV_X) + deltas->r_left,
				XV_Y, xv_get(priv->list, XV_Y) + deltas->r_top,
				NULL);
	}
	priv->sbr.r_left += deltas->r_left;
	priv->sbr.r_top += deltas->r_top;

	xv_set(priv->bar_window,
				XV_X, xv_get(priv->bar_window, XV_X) + deltas->r_left,
				XV_Y, xv_get(priv->bar_window, XV_Y) + deltas->r_top,
				NULL);
}

static void mllist_accept_kbd_focus(Panel_item item)
{
	MultiLevelList_private *priv = MLLPRIV(item);
	Panel_ops *ops = (Panel_ops *)xv_get(priv->list, PANEL_OPS_VECTOR);

    (*(ops->panel_op_accept_kbd_focus))(priv->list);
}

static void mllist_yield_kbd_focus(Panel_item item)
{
	MultiLevelList_private *priv = MLLPRIV(item);
	Panel_ops *ops = (Panel_ops *)xv_get(priv->list, PANEL_OPS_VECTOR);

    (*(ops->panel_op_yield_kbd_focus))(priv->list);
}

static void mllist_accept_key(Panel_item item, Event *ev)
{
	MultiLevelList_private *priv = MLLPRIV(item);
    panel_accept_key(priv->list, ev);
}

static void note_levelmenu(Menu menu, Menu_item item)
{
	int sp = (int)xv_get(item, MENU_CLIENT_DATA);
	MultiLevelList_private *priv =
			(MultiLevelList_private *)xv_get(menu, XV_KEY_DATA, mllist_key);

	save_current_level(priv);
	priv->stackptr = sp;
	set_list_title(priv);
	update_list(priv, TRUE);
}

static void update_levels_menu_from_delete(MultiLevelList_private *priv, mllist_ptr item)
{
	int i;
	level_descr *stack = priv->levelstack;

	if (! item->has_sublevel) return;

	for (i = 0; stack[i].parent_of_level; i++) {
		if (stack[i].parent_of_level == item) {
			int j;

			for (j = i; j < STACKDEPTH; j++) {
				if (stack[j].levelitem && stack[j].levelitem_visible) {
					xv_set(priv->levelmenu,
							MENU_REMOVE_ITEM, stack[j].levelitem,
							NULL);
					stack[j].levelitem_visible = FALSE;
				}
			}
		}
	}
}

static int call_appl_verify(MultiLevelList_private *priv, mllist_ptr cur, Proplist_verify_op op, mllist_ptr orig)
{
	if (! priv->appl_verify) return XV_OK;

	return (*(priv->appl_verify))(MLLPUB(priv), cur, op, orig);
}

static void one_level_down(MultiLevelList_private *priv, mllist_ptr p)
{
	level_descr *stack = priv->levelstack;
	int sp = priv->stackptr;

	save_current_level(priv);

	if (stack[sp+1].parent_of_level == p) {
		++priv->stackptr;
		set_list_title(priv);
	}
	else {
		int i;

		for (i = sp + 1; i < STACKDEPTH; i++) {
			stack[i].parent_of_level = (mllist_ptr)0;
			if (stack[i].levelitem &&stack[i].levelitem_visible)
			{
				xv_set(priv->levelmenu,
					MENU_REMOVE_ITEM, stack[i].levelitem,
					NULL);
				stack[i].levelitem_visible = FALSE;
			}
			stack[i].selected = 0;
		}

		level_down(priv, p);
	}
	update_list(priv, FALSE);
}

static int note_verify(Property_list list, mllist_ptr cur, Proplist_verify_op op, mllist_ptr orig)
{
	MultiLevelList_private *priv =
			(MultiLevelList_private *)xv_get(list, XV_KEY_DATA, mllist_key);
	mllist_ptr p;
	int num, i;

	switch (op) {
		case PROPLIST_APPLY:
			priv->lasttime = 0L;
			if (call_appl_verify(priv, cur, op, orig) != XV_OK) return XV_ERROR;
			if (cur->has_sublevel) {
				if (orig->has_sublevel) {
					cur->firstchild = orig->firstchild;
					orig->firstchild = (mllist_ptr)0;
					for (p = cur->firstchild; p; p = p->next) p->parent = cur;
				}
			}
			else {
				if (orig->has_sublevel) {
					mllist_ptr ent, next;

					for (ent = orig->firstchild; ent; ent = next) {
						next = ent->next;
						own_free(priv->list, ent);
					}
					orig->firstchild = (mllist_ptr)0;
					cur->firstchild = (mllist_ptr)0;
				}
			}
			break;

		case PROPLIST_INSERT:
			priv->lasttime = 0L;
			return call_appl_verify(priv, cur, op, orig);

		case PROPLIST_CONVERT:
			priv->lasttime = 0L;
			return call_appl_verify(priv, cur, op, orig);

		case PROPLIST_USE_IN_PANEL:
			if (cur->has_sublevel) {
				Panel pan = xv_get(priv->list, XV_OWNER);
				Xv_server srv = XV_SERVER_FROM_WINDOW(pan);
				time_t now = server_get_timestamp(srv);
				time_t double_ms = priv->double_time * 100;

				if (now - priv->lasttime <= double_ms) {
					priv->lasttime = 0L;
					one_level_down(priv, cur);
					return XV_OK;
				}
				priv->lasttime = now;
			}
			return call_appl_verify(priv, cur, op, orig);

		case PROPLIST_CREATE_GLYPH:
			priv->lasttime = 0L;
			if (cur->has_sublevel)
				cur->mllist_private[PL_GLYPH_INDEX] = priv->dot;
			else cur->mllist_private[PL_GLYPH_INDEX] = priv->nodot;
			cur->mllist_private[PL_MASK_INDEX] =
								cur->mllist_private[PL_GLYPH_INDEX];
			break;

		case PROPLIST_DELETE_OLD_CHANGED:
		case PROPLIST_DELETE_FROM_RESET:
		case PROPLIST_DELETE:
			priv->lasttime = 0L;
			update_levels_menu_from_delete(priv, cur);
			return call_appl_verify(priv, cur, op, orig);

		case PROPLIST_LIST_CHANGED:
			priv->lasttime = 0L;
			(void)call_appl_verify(priv, cur, op, orig);
			num = (int)xv_get(priv->list, PANEL_LIST_NROWS);
			for (i = 0; i < num; i++) {
				p = (mllist_ptr)xv_get(priv->list, PANEL_LIST_CLIENT_DATA, i);
				if (p->has_sublevel) break;
			}
			priv->sbstate = (priv->stackptr == 0) ? NO_BWD : 0;
			if (i >= num) priv->sbstate |= NO_FWD;
			draw_sb(priv, 0);
			break;
		default: break;
	}

	return XV_OK;
}

static int mllist_init(Panel owner, Xv_opaque slf, Attr_avlist avlist, int *u)
{
	Xv_multi_level_list *self = (Xv_multi_level_list *)slf;
	MultiLevelList_private *priv;
	Cms cms;
	Attr_avlist attrs;
	Xv_font font;
	XFontStruct *finfo;
	XGCValues val;
	int datasize = -1, i, depth;
	int bwhig = 1;
	unsigned fontheight;
	Attr_attribute sviattr[20];
	Xv_screen scr = XV_SCREEN_FROM_WINDOW(owner);
	screen_ui_style_t ui_style;

	if (!(priv = (MultiLevelList_private *)xv_alloc(MultiLevelList_private)))
		return XV_ERROR;

	priv->public_self = (Xv_opaque)self;
	self->private_data = (Xv_opaque)priv;

	if (! mllist_key) mllist_key = xv_unique_key();

	priv->listfont = xv_get(owner, XV_FONT);

	for (attrs=avlist; *attrs; attrs=attr_next(attrs)) switch ((int)*attrs) {
		case PROPLIST_ITEM_DATA_SIZE:
			datasize = (int)A1;
			ADONE;
		case PROPLIST_FONT:
			priv->listfont = (Xv_font)A1;
			ADONE;
	}

	priv->levelstack = (level_descr *)xv_alloc_n(level_descr,
									(size_t)STACKDEPTH);
	priv->root = (mllist_ptr)xv_alloc(mllist_node);
	priv->root->label = xv_strsave("");
	priv->root->has_sublevel = TRUE;
	priv->levelstack[0].parent_of_level = priv->root;
	priv->levelstack[0].levelitem_visible = TRUE;
	priv->levelstack[0].levelitem = xv_create(XV_NULL, MENUITEM,
								MENU_RELEASE,
								MENU_RELEASE_IMAGE,
								MENU_STRING, xv_strsave(priv->root->label),
								MENU_INACTIVE, TRUE,
								MENU_CLIENT_DATA, 0,
								NULL);

	priv->double_time =
				defaults_get_integer_check("openWindows.multiClickTimeout",
								"OpenWindows.MultiClickTimeout", 4, 2, 10);

	priv->over_list = OVER_UNKNOWN;

	priv->dpy = (Display *)xv_get(owner, XV_DISPLAY);
	priv->panxid = (Window)xv_get(owner, XV_XID);
	depth = (int)xv_get(owner, XV_DEPTH);

	font = xv_get(owner, PANEL_BOLD_FONT);
	priv->scale = (Window_rescale_state)xv_get(font, FONT_SCALE);
	finfo = (XFontStruct *)xv_get(font, FONT_INFO);
	cms = xv_get(owner, WIN_CMS);

	priv->color_offset = 0;

	priv->three_d = FALSE;
	ui_style = (screen_ui_style_t)xv_get(scr, SCREEN_UI_STYLE);
	if (ui_style == SCREEN_UIS_3D_COLOR) {
		priv->three_d = TRUE;
		priv->color_offset = 1;
	}
	{
        Xv_font savefont = xv_get(owner, XV_FONT);

        xv_set(owner, XV_FONT, font, NULL);
		priv->ginfo = (Graphics_info *)xv_init_olgx(owner, &priv->three_d,font);
        xv_set(owner, XV_FONT, savefont, NULL);
    }

	priv->sbr.r_height = AbbScrollbar_Height(priv->ginfo);
	priv->sbr.r_width = ScrollbarElevator_Width(priv->ginfo);

	/* this menu is so dynamic - I don't see a reason to have its state
	 * (default item) recorded - so, we do **not** set XV_SET_MENU
	 */
	priv->levelmenu = xv_create(XV_SERVER_FROM_WINDOW(owner), MENU,
				MENU_TITLE_ITEM, XV_MSG("Levels"),
				MENU_NOTIFY_PROC, note_levelmenu,
				XV_KEY_DATA, mllist_key, priv,
				MENU_APPEND_ITEM, priv->levelstack[0].levelitem,
/* not useful:	XV_SET_MENU, owner, */
				NULL);

	if (datasize > 0) {
		priv->list = xv_create(owner, PROP_LIST,
				PANEL_ITEM_OWNER, slf,
				PANEL_VALUE_DISPLAY_LENGTH, 12,
				PANEL_CHOOSE_NONE, FALSE,
				PROPLIST_ITEM_DATA_SIZE, datasize,
				PROPLIST_FREE_ITEM_PROC, own_free,
				PROPLIST_VERIFY_PROC, note_verify,
				PROPLIST_ENTRY_STRING_OFFSET, FP_OFF(mllist_ptr,label),
				PROPLIST_GLYPH_OFFSET, FP_OFF(mllist_ptr, mllist_private[0]),
				PROPLIST_FONT, priv->listfont,
				XV_KEY_DATA, mllist_key, priv,
				NULL);
	}
	else {
		priv->list = xv_create(owner, PANEL_LIST,
				PANEL_ITEM_OWNER, slf,
				PANEL_VALUE_DISPLAY_LENGTH, 12,
				PANEL_CHOOSE_NONE, FALSE,
				XV_KEY_DATA, mllist_key, priv,
				NULL);
	}

	val.font = finfo->fid;
	val.foreground = (unsigned long)xv_get(cms, CMS_FOREGROUND_PIXEL);
	val.background = (unsigned long)xv_get(cms, CMS_BACKGROUND_PIXEL);

	priv->gc = XCreateGC(priv->dpy, priv->panxid,
						GCFont | GCForeground | GCBackground, &val);

	xv_set(slf,
				PANEL_LABEL_FONT, font,
				PANEL_CHILD_CARET_ITEM, priv->list,
				NULL);

	fontheight = finfo->ascent + finfo->descent;

	i = 0;
	sviattr[i++] = (Attr_attribute) XV_WIDTH;
	sviattr[i++] = (Attr_attribute) DOT_PMWID;
	sviattr[i++] = (Attr_attribute) XV_HEIGHT;
	sviattr[i++] = (Attr_attribute) fontheight;
	sviattr[i++] = (Attr_attribute) SERVER_IMAGE_DEPTH;

	sviattr[i++] = (Attr_attribute) 1;
	sviattr[i++] = (Attr_attribute) NULL;

	priv->dot = xv_create_avlist(scr, SERVER_IMAGE, sviattr);
	priv->nodot = xv_create_avlist(scr, SERVER_IMAGE, sviattr);

	{
		XGCValues gcv;
		GC gc1;
		Pixmap d = (Pixmap)xv_get(priv->dot, XV_XID);
		Pixmap nd = (Pixmap)xv_get(priv->nodot, XV_XID);

		gcv.foreground = 0;
		gcv.background = 0;

		gc1 = XCreateGC(priv->dpy, d, GCForeground | GCBackground, &gcv);

		XFillRectangle(priv->dpy, d, gc1, 0, 0, DOT_PMWID+1, fontheight+1);
		XFillRectangle(priv->dpy, nd, gc1, 0, 0, DOT_PMWID+1, fontheight+1);
		XSetForeground(priv->dpy, gc1, 1L);
		XFillArc(priv->dpy, d, gc1,
				DOT_PMWID - (int)ol_mll_sizes[(int)priv->scale].dot_diam - 2,
				(int)(fontheight - 3 - ol_mll_sizes[(int)priv->scale].dot_diam),
				ol_mll_sizes[(int)priv->scale].dot_diam,
				ol_mll_sizes[(int)priv->scale].dot_diam,
				0, 64 * 360);
		XDrawArc(priv->dpy, d, gc1,
				DOT_PMWID - (int)ol_mll_sizes[(int)priv->scale].dot_diam - 2,
				(int)(fontheight - 3 - ol_mll_sizes[(int)priv->scale].dot_diam),
				ol_mll_sizes[(int)priv->scale].dot_diam,
				ol_mll_sizes[(int)priv->scale].dot_diam,
				0, 64 * 360);
		if (priv->gc) XFreeGC(priv->dpy, gc1);
	}

	if (depth >= 2) bwhig = 3;

	/* to cover the top of the scrolling list border */
	priv->bar_window = xv_create(owner, WINDOW,
						WIN_CONSUME_EVENTS, WIN_REPAINT, NULL,
						WIN_EVENT_PROC, barwin_event,
						WIN_CLIENT_DATA, priv,
						WIN_CMS, cms,
						XV_HEIGHT, bwhig,
						NULL);

	return XV_OK;
}

static void update_own_geom(MultiLevelList_private *priv)
{
	Rect *r, rect;

	r = (Rect *)xv_get(MLLPUB(priv), PANEL_ITEM_VALUE_RECT);
	rect = *r;

	r = (Rect *)xv_get(priv->list, XV_RECT);
	rect.r_width = r->r_width;
	rect.r_height = r->r_height + priv->sbr.r_height + 2;
	priv->sbr.r_left = rect.r_left + rect.r_width - priv->sbr.r_width -
								priv->color_offset -
								ol_mll_sizes[(int)priv->scale].sb_shift;
	priv->sbr.r_top = rect.r_top;
	xv_set(MLLPUB(priv), PANEL_ITEM_VALUE_RECT, &rect, NULL);
}

static void mllist_end_create(MultiLevelList_private *priv,
								Multi_level_list self)
{
	static Panel_ops mllist_ops = {
		panel_default_handle_event,     /* handle_event() */
		mllist_begin_preview,          /* begin_preview() */
		mllist_update_preview,         /* update_preview() */
		mllist_cancel_preview,         /* cancel_preview() */
		mllist_accept_preview,         /* accept_preview() */
		mllist_accept_menu,            /* accept_menu() */
		mllist_accept_key,             /* accept_key() */
		panel_default_clear_item,      /* clear() */
		mllist_paint,                  /* paint() */
		mllist_resize,                 /* resize() */
		mllist_remove,                 /* remove() */
		mllist_restore,                /* restore() */
		mllist_layout,                 /* layout() */
		mllist_accept_kbd_focus,       /* accept_kbd_focus() */
		mllist_yield_kbd_focus,        /* yield_kbd_focus() */
		NULL                           /* extension: reserved for future use */
	};
	Rect *lr, *r, lrect, rect;
	int bwwid, bwx, bwy;

	r = (Rect *)xv_get(self, PANEL_ITEM_VALUE_RECT);
	rect = *r;
	lr = (Rect *)xv_get(priv->list, PANEL_ITEM_VALUE_RECT);
	lrect = *lr;

	rect.r_width = lr->r_width;
	rect.r_height = lr->r_height + priv->sbr.r_height + 2;
	lrect.r_left = rect.r_left;
	lrect.r_top = rect.r_top + priv->sbr.r_height + 2;
	priv->sbr.r_left = rect.r_left + rect.r_width - priv->sbr.r_width -
								priv->color_offset -
								ol_mll_sizes[(int)priv->scale].sb_shift;
	priv->sbr.r_top = rect.r_top;

	xv_set(priv->list,
			PANEL_VALUE_X, lrect.r_left,
			PANEL_VALUE_Y, lrect.r_top - 1,
			NULL);

	xv_set(self,
			PANEL_ITEM_VALUE_RECT, &rect,
			PANEL_OPS_VECTOR, &mllist_ops,
			NULL);

	bwx = rect.r_left + 1,
	bwy = lrect.r_top - 1;
	bwwid = rect.r_width - priv->sbr.r_width - priv->color_offset -
					ol_mll_sizes[(int)priv->scale].sb_dist - 2;

	if (priv->three_d) {
		bwy -= 1;
		bwx += 1;
		bwwid -= 2;
	}

	xv_set(priv->bar_window,
			XV_X, bwx,
			XV_Y, bwy,
			XV_WIDTH, bwwid,
			NULL);

	priv->created = TRUE;
}

/*ARGSUSED*/
static Xv_opaque mllist_set(Multi_level_list self, Attr_avlist avlist)
{
	Attr_attribute *attrs, copy[ATTR_STANDARD_SIZE];
	MultiLevelList_private *priv = MLLPRIV(self);
	int geom_changed = FALSE;
	mllist_ptr ent;
	int j;
	level_descr *stack = priv->levelstack;

	for (attrs=avlist; *attrs; attrs=attr_next(attrs)) {
		switch ((int)*attrs) {
			case PANEL_LIST_TITLE:
				if (priv->root->label) xv_free(priv->root->label);
				if (A1) priv->root->label = xv_strsave((char *)A1);
				else priv->root->label = xv_strsave("");
				xv_set(priv->levelstack[0].levelitem,
									MENU_STRING, xv_strsave(priv->root->label),
									NULL);
				if (priv->stackptr == 0) draw_title(priv);
				ADONE;

			case PROPLIST_VERIFY_PROC:
				priv->appl_verify = (verify_proc_t)A1;
				ADONE;

			case PROPLIST_FREE_ITEM_PROC:
				priv->appl_free = (free_proc_t)A1;
				ADONE;

	/* 		case PANEL_INACTIVE: */
	/* 		case XV_SHOW: */
	/*         	xv_set(priv->list, A0, A1, NULL); */
	/*         	break; */

			case MLLIST_ROOT_CHILDREN:
				if (priv->root->firstchild) {
					mllist_ptr next;

					/* the 'next' link may be corrupted in the current level;
					 * we restore it
					 */
					save_current_level(priv);

					for (ent = priv->root->firstchild; ent; ent = next) {
						next = ent->next;
						own_free(priv->list, ent);
					}
				}

				for (j = 1; j < STACKDEPTH; j++) {
					if (stack[j].levelitem && stack[j].levelitem_visible) {
						xv_set(priv->levelmenu,
								MENU_REMOVE_ITEM, stack[j].levelitem,
								NULL);
						stack[j].levelitem_visible = FALSE;
					}
					stack[j].parent_of_level = (mllist_ptr)0;
				}
				priv->stackptr = 0;
				xv_set(stack[0].levelitem, MENU_INACTIVE, TRUE, NULL);
				priv->root->firstchild = (mllist_ptr)A1;
				set_list_title(priv);
				update_list(priv, FALSE);
				ADONE;

			case MLLIST_INSERT:
				ent = (mllist_ptr)A1;
				j = (int)xv_get(priv->list, PANEL_LIST_FIRST_SELECTED);
				if (j < 0) j = (int)xv_get(priv->list, PANEL_LIST_NROWS);

				if (ent->has_sublevel)
					ent->mllist_private[PL_GLYPH_INDEX] = priv->dot;
				else ent->mllist_private[PL_GLYPH_INDEX] = priv->nodot;
				ent->mllist_private[PL_MASK_INDEX] = priv->dot;

				/* fprintf(stderr, "mllist: insert %d\n", j); */
				xv_set(priv->list,
						PANEL_LIST_INSERT, j,
						PANEL_LIST_STRING, j, ent->label,
						PANEL_LIST_CLIENT_DATA, j, ent,
						PANEL_LIST_GLYPH, j, ent->mllist_private[PL_GLYPH_INDEX],
						PANEL_LIST_MASK_GLYPH, j, priv->dot,
						PANEL_LIST_FONT, j, priv->listfont,
						PANEL_LIST_SELECT, j, TRUE,
						NULL);

				ADONE;

			case MLLIST_IS_LEVEL:
				ent = (mllist_ptr)xv_get(priv->list, PANEL_LIST_CLIENT_DATA, A1);
				if (ent) {
					ent->has_sublevel = (char)A2;
					if (ent->has_sublevel) {
						ent->mllist_private[PL_GLYPH_INDEX] = priv->dot;
					}
					else {
						ent->mllist_private[PL_GLYPH_INDEX] = priv->nodot;
					}
					ent->mllist_private[PL_MASK_INDEX] = priv->dot;
					xv_set(priv->list,
							PANEL_LIST_GLYPH,A1,ent->mllist_private[PL_GLYPH_INDEX],
							PANEL_LIST_MASK_GLYPH, A1, priv->dot,
							NULL);
				}
				ADONE;

			case MLLIST_OPEN_CHILD:
				one_level_down(priv, (mllist_ptr)A1);
				ADONE;

			case XV_END_CREATE:
				mllist_end_create(priv, self);
				return XV_OK;

			default: xv_check_bad_attr(MULTI_LEVEL_LIST, A0);
				break;
		}
	}

	attr_copy_avlist(copy, avlist);

	for (attrs=copy; *attrs; attrs=attr_next(attrs)) switch ((int)*attrs) {
		case XV_X:
		case XV_Y:
		case XV_RECT:
		case PANEL_ITEM_VALUE_RECT:
		case PANEL_ITEM_LABEL_RECT:
			ADONE;

		case PANEL_CHOOSE_ONE:
		case PANEL_LABEL_STRING:
		case PANEL_OPS_VECTOR:
			ADONE;

		case PANEL_LIST_WIDTH:
		case PANEL_LIST_ROW_HEIGHT:
		case PANEL_LIST_DISPLAY_ROWS:
			geom_changed = TRUE;
			break;
	}
	xv_set_avlist(priv->list, copy);

	if (priv->created && geom_changed) update_own_geom(priv);

	return XV_OK;
}

/*ARGSUSED*/
static Xv_opaque mllist_get(Multi_level_list self, int *status, Attr_attribute attr, va_list vali)
{
	MultiLevelList_private *priv = MLLPRIV(self);

	*status = XV_OK;
	switch ((int)attr) {
		case MLLIST_ROOT_CHILDREN:
			save_current_level(priv);
			return (Xv_opaque)priv->root->firstchild;
		case PANEL_LIST_TITLE:
			return (Xv_opaque)priv->root->label;
		case PROPLIST_VERIFY_PROC:
			return (Xv_opaque)priv->appl_verify;
		case PROPLIST_FREE_ITEM_PROC:
			return (Xv_opaque)priv->appl_free;
		case PANEL_LIST_FIRST_SELECTED:
		case PANEL_LIST_CLIENT_DATA:
		case PANEL_LIST_DISPLAY_ROWS:
		case PANEL_LIST_GLYPH:
		case PANEL_LIST_MODE:
		case PANEL_LIST_NROWS:
		case PANEL_LIST_ROW_HEIGHT:
		case PANEL_LIST_SCROLLBAR:
		case PANEL_LIST_SELECTED:
		case PANEL_LIST_STRING:
		case PANEL_LIST_WIDTH:
		case PANEL_ITEM_MENU:
			return xv_get(priv->list, (unsigned)attr, va_arg(vali, int));

		default:
			*status = xv_check_bad_attr(MULTI_LEVEL_LIST, attr);
	}
	return (Xv_opaque)XV_OK;
}

static int mllist_destroy(Multi_level_list self, Destroy_status status)
{
	if (status == DESTROY_CLEANUP) {
		MultiLevelList_private *priv = MLLPRIV(self);
		int i;
		Menu_item it;

		if (priv->levelmenu) {
			for (i = (int)xv_get(priv->levelmenu, MENU_NITEMS);
				(it = xv_get(priv->levelmenu, MENU_NTH_ITEM, i));
				i--)
			{
				xv_set(priv->levelmenu, MENU_REMOVE_ITEM, it, NULL);
				xv_destroy(it);
			}
			xv_destroy(priv->levelmenu);
			xv_destroy(priv->list);
		}
		if (priv->dot) xv_destroy(priv->dot);
		if (priv->nodot) xv_destroy(priv->nodot);

		xv_free(priv->levelstack);

		if (priv->gc) XFreeGC(priv->dpy, priv->gc);
		xv_destroy(priv->bar_window);
		xv_free((char *)priv);
	}
	return XV_OK;
}

const Xv_pkg xv_multi_level_list_pkg = {
	"MultiLevelList",
	ATTR_PKG_MLLIST,
	sizeof(Xv_multi_level_list),
	PANEL_ITEM,
	mllist_init,
	mllist_set,
	mllist_get,
	mllist_destroy,
	0
};
