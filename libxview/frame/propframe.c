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
#include <xview/frame.h>
#include <xview/panel.h>
#include <xview/defaults.h>
#include <xview/openmenu.h>
#include <xview/scrollbar.h>
#include <xview/win_notify.h>
#include <xview_private/i18n_impl.h>
#include <xview_private/wmgr_decor.h>
#include <xview_private/svr_impl.h>
#define _OTHER_TEXTSW_FUNCTIONS 1
#include <xview/textsw.h>

char propframe_c_sccsid[] = "@(#) %M% V%I% %E% %U% $Id: propframe.c,v 4.16 2025/06/08 16:18:08 dra Exp $";

#define A0 *attrs
#define A1 attrs[1]
#define A2 attrs[2]
#define A3 attrs[3]
#define A4 attrs[4]

#define YOFF 1

#define ADONE ATTR_CONSUME(*attrs);break
#define NUMBER(array) (sizeof(array)/sizeof(array[0]))

typedef struct _propframe_priv_t *propframe_priv_ptr_t;
typedef struct _pp_item_t *pp_item_ptr_t;

typedef void (*convert_t)(Xv_opaque val, int panel_to_data, char **datptr, Panel_item it, Xv_opaque unused);
typedef void (*propagate_action)(propframe_priv_ptr_t,
									Panel_item, pp_item_ptr_t, Xv_opaque);
typedef int (*propcbproc) (Frame_props pf, int is_triggered);
typedef int (*propswitchproc)(Frame_props pf,Panel newp,int isfilled);
typedef void (*create_proc_t)(Frame_props);
typedef int (*prop2apply_t)(Frame_props, Xv_opaque, Propframe_apply_mode);

typedef struct _pp_item_t {
	int data_offset;
	convert_t converter;
	Xv_opaque client_data;
	Xv_opaque master;
} Propframe_item;

typedef struct _one_panel {   /* = one category */
	struct _one_panel *next;
	Panel             panel;
	int               cat_initial_width, cat_initial_height;
	int               cat_width, cat_height;
	propswitchproc    switch_proc;
	Panel_item        apply, pseudo_default;
	Panel_item        resize_item;
	int               resize_y; /* the y coordinate of the resize_item */
	int               resize_height; /* the initial height of the resize_item */
	char              is_filled;
} *one_panel_t;

typedef struct _propframe_priv_t {
	Xv_opaque       public_self;
	propcbproc      apply, reset, set_defaults, reset_factory;
	create_proc_t   create_contents_proc;
	char            *cur_data, *def_data, *fac_data;
	char            *apply_label, *cat_label;
	int             cat_cols;
	Panel_item      last_changebar, cat_choice, *aligns, *moves, last_leader;
	Menu            menu;
	Menu_item       apply_item;
	one_panel_t     cats, current;
	char            buttonreq[4];
	char            *buttonhelps[4];
	short           apply_width_diff, max_items, align_count, move_count,
					choice_cnt;
	char            multi_cat, reset_on_switch, is_apply_all,
					read_only, is_cmd_win, is_cancel, buttons_created;
	/* BEGIN two selections: */
	Xv_opaque       holder; /* the object that holds the selection(s) */
	char            second_sel;
	prop2apply_t    apply_2; /* with a holder, we use a different apply cb */
	Menu_item       new_sel_1, new_sel_2;
	Menu            apply_menu;
	/* END two selections */
} Propframe_private;

#define WINATTR_LEN (sizeof(WM_Win_Type)/sizeof(unsigned long))

#define PROPFRAMEPRIV(_x_) XV_PRIVATE(Propframe_private, Xv_propframe, _x_)
#define PROPFRAMEPUB(_x_) XV_PUBLIC(_x_)
#define MAX_ITEMS 30
#define BORDER_WIDTH 1

#define ITEMS_CHANGEBAR XV_KEY_DATA,to_change_bar_key
#define OBJECT_TO_PRIV XV_KEY_DATA,props_key
#define CB_TO_TYPE XV_KEY_DATA,props_key
#define PANEL_TO_ONE_PANEL XV_KEY_DATA,props_key

static int props_key = 0,
			attach_key = 0,
			to_change_bar_key = 0,
			to_orig_note_key = 0,
			resize_key = 0;

static void init_keys(void)
{
	if (props_key > 0) return;

	attach_key = xv_unique_key();
	props_key = xv_unique_key();
	resize_key = xv_unique_key();
	to_change_bar_key = xv_unique_key();
	to_orig_note_key = xv_unique_key();
}

int frame_props_cbkey(void)
{
	init_keys();
	return to_change_bar_key;
}

int frame_props_cbidkey(void)
{
	init_keys();
	return props_key;
}

/*******************************************************************/
/****           BEGIN   layout                                  ****/
/*******************************************************************/

static void align_labels(Panel_item *aligns, int acnt,
						Panel_item *moves, int mcnt)
{
	int mi, ai, offset, max = 0;

	for (ai = 0; ai < acnt; ai++) {
		offset = (int)xv_get(aligns[ai], PANEL_VALUE_X);
		if (offset > max) max = offset;
	}

	mi = 0;

	for (ai = 0; ai < acnt; ai++) {
		int x = (int)xv_get(aligns[ai], XV_X);
		int y = (int)xv_get(aligns[ai], XV_Y);

		offset = (int)xv_get(aligns[ai], PANEL_VALUE_X);
		xv_set(aligns[ai], XV_X, x + max - offset, NULL);

		while (mi < mcnt && (int)xv_get(moves[mi], XV_Y) == y) {
			int newx =  (int)xv_get(moves[mi], XV_X) + max - offset;

			xv_set(moves[mi], XV_X, newx, NULL);
			mi++;
		}
	}
}

/*******************************************************************/
/****           BEGIN   converters                              ****/
/*******************************************************************/

static void convert_int(Xv_opaque cval, int panel_to_data, char **dp,
								Panel_item it, Xv_opaque unused)
{
	int val = (int)cval;
	int *datptr = (int *)dp;

	if (panel_to_data) *datptr = val;
	else xv_set(it, PANEL_VALUE, *datptr, NULL);
}

static void convert_string(Xv_opaque cval, int panel_to_data, char **datptr,
								Panel_item it, Xv_opaque unused)
{
	if (panel_to_data) {
		char *val = (char *)cval;

		if (*datptr) xv_free(*datptr);
		*datptr = xv_strsave(val ? val : "");
	}
	else xv_set(it, PANEL_VALUE, (*datptr ? *datptr : ""), NULL);
}

static void convert_string_multiline(Xv_opaque cval, int panel_to_data,
							char **datptr, Panel_item it, Xv_opaque unused)
{
	char *val = (char *)cval;

	if (panel_to_data) {
		if (*datptr) xv_free(*datptr);
		*datptr = xv_strsave(val ? val : "");
	}
	else {
		Textsw text;

		xv_set(it, PANEL_VALUE, (*datptr ? *datptr : ""), NULL);
		text = xv_get(xv_get(it, PANEL_ITEM_NTH_WINDOW, 0), XV_OWNER);
		if (xv_get(text, XV_IS_SUBTYPE_OF, TEXTSW)) {
			xv_set(text,
					TEXTSW_FIRST, 0,
					TEXTSW_INSERTION_POINT, 0,
					NULL);
			textsw_normalize_view(xv_get(text,OPENWIN_NTH_VIEW,0), 0);
		}
		else {
			xv_error(it,
				ERROR_PKG, FRAME_PROPS,
				ERROR_SEVERITY, ERROR_RECOVERABLE,
				ERROR_LAYER, ERROR_TOOLKIT,
				ERROR_STRING, XV_MSG("failed to derive TEXTSW from PANEL_MULTILINE_TEXT"),
				NULL);
		}
	}
}

static void scroll_to_selected(Panel_item list, int sel_ind)
{
	Scrollbar sb = xv_get(list, PANEL_LIST_SCROLLBAR);
	int vis_count = (int)xv_get(list, PANEL_LIST_DISPLAY_ROWS),
		first_visible = (int)xv_get(sb, SCROLLBAR_VIEW_START);

	if (sel_ind < first_visible)
		xv_set(sb, SCROLLBAR_VIEW_START, sel_ind, NULL);
	else if (sel_ind >= first_visible + vis_count)
		xv_set(sb, SCROLLBAR_VIEW_START, sel_ind + 1 - vis_count, NULL);
}

static void list_string_converter(Xv_opaque cfirst, int panel_to_data,
						char **datptr, Panel_item it, Xv_opaque unused)
{
	int first = (int)cfirst;
	char *lstr;

	if (panel_to_data) {
		if (*datptr) xv_free(*datptr);

		lstr = (char *)xv_get(it, PANEL_LIST_STRING, first);
		*datptr = xv_strsave(lstr ? lstr : "");
	}
	else {
		int i, cnt = (int)xv_get(it, PANEL_LIST_NROWS);

		if (! *datptr) return;

		for (i = 0; i < cnt; i++) {
			lstr = (char *)xv_get(it, PANEL_LIST_STRING, i);
			if (lstr && ! strcmp(lstr, *datptr)) {
				xv_set(it, PANEL_LIST_SELECT, i, TRUE, NULL);
				/* position of scrollbar */
				scroll_to_selected(it, i);
				return;
			}
		}
		xv_set(it,
				PANEL_LIST_INSERT, cnt,
				PANEL_LIST_STRING, cnt, *datptr,
				NULL);

		xv_set(it, PANEL_LIST_SELECT, cnt, TRUE, NULL);
		/* position of scrollbar */
		scroll_to_selected(it, cnt);
	}
}

static void convert_nil(Xv_opaque first, int panel_to_data, char **datptr, Panel_item it, Xv_opaque unused)
{
}

static struct { const Xv_pkg *pkg; convert_t proc; } converters[] = {
	{ PANEL_MESSAGE,            convert_nil },
	{ PANEL_ABBREV_MENU_BUTTON, convert_nil },
	{ PANEL_BUTTON,             convert_nil },
	{ PANEL_CHOICE,             convert_int },
	{ PANEL_DROP_TARGET,        convert_nil },
	{ PANEL_GAUGE,              convert_nil },
	{ PANEL_LIST,               list_string_converter },
	{ PANEL_MULTILINE_TEXT,     convert_string_multiline },
	{ PANEL_NUMERIC_TEXT,       convert_int },
	{ PANEL_SLIDER,             convert_int },
	{ PANEL_TEXT,               convert_string }
};

/*******************************************************************/
/****           END     converters                              ****/
/*******************************************************************/

static void reset_change_bars_in_panel(Propframe_private *priv, Panel pan)
{
	Panel_item cb;

	PANEL_EACH_ITEM(pan, cb)
		if (PANEL_MESSAGE == (const Xv_pkg *)xv_get(cb, CB_TO_TYPE)) {
			xv_set(cb, PANEL_LABEL_STRING, " ", NULL);
		}
	PANEL_END_EACH
}

static void update_win_attrs(Propframe_private *priv, int is_cancel, int update_pin)
{
	Frame self = PROPFRAMEPUB(priv);
	Xv_opaque server = XV_SERVER_FROM_WINDOW(self);
	Display *dpy = (Display *)xv_get(self, XV_DISPLAY);
	Window xid = (Window)xv_get(self, XV_XID);
	Atom atom;
	WM_Win_Type *win_attr;
	Atom typeatom;
	int act_format;
	unsigned long nelem, left_to_be_read = 0;

	if (priv->is_cmd_win) return;

	atom = (Atom)xv_get(server, SERVER_WM_WIN_ATTR);
	if (XGetWindowProperty(dpy, xid, atom, 0L, 30L, False, atom, &typeatom,
					&act_format, &nelem, &left_to_be_read,
					(unsigned char **)&win_attr) != Success) return;
	if (nelem != WINATTR_LEN || typeatom != atom) return;
 
	priv->is_cancel = is_cancel;


	win_attr->flags = WMWinType | WMMenuType | WMCancel;
	win_attr->cancel = is_cancel;

	/* we never change the CONTENTS of the pinstate field */
	if (update_pin) {
		win_attr->flags |= WMPinState;
	}

	XChangeProperty(dpy, xid, atom, atom, 32, PropModeReplace,
					(unsigned char *)win_attr, (unsigned)WINATTR_LEN);

	XFree((char *)win_attr);
}

static void set_changed(Propframe_private *priv, Panel_item item, int is_set)
{
	Panel_item cb = xv_get(item, ITEMS_CHANGEBAR);

	if (!cb) return;

	xv_set(cb, PANEL_LABEL_STRING, is_set ? "|" : " ", NULL);

	if (is_set) {
		if (!priv) priv = (Propframe_private*)xv_get(cb,XV_KEY_DATA,attach_key);

		if (priv && xv_get(PROPFRAMEPUB(priv), XV_SHOW))
			update_win_attrs(priv, TRUE, FALSE);
	}
}

static void set_cat_changed(Propframe_private *priv, int is_all)
{
	one_panel_t p;
	int diff;

	if (priv->is_cmd_win) return;
	if (priv->is_apply_all == is_all) return;

	priv->is_apply_all = is_all;
	p = priv->cats;
	diff = (is_all? -priv->apply_width_diff:priv->apply_width_diff);
	set_changed(priv, priv->cat_choice, is_all);

	if (priv->apply_item) {
		xv_set(priv->apply_item,
				MENU_STRING, is_all ? XV_MSG("Apply All") :
										XV_MSG("Apply"),
				NULL);
	}

	while (p) {
		if (p->apply) {
			xv_set(p->apply,
					XV_X, (int)xv_get(p->apply, XV_X) + diff,
					PANEL_LABEL_STRING,
							is_all ? XV_MSG("Apply All") :
									XV_MSG("Apply"),
					NULL);
		}
		p = p->next;
	}
}

static void reset_change_bars(Propframe_private *priv)
{
	one_panel_t op = priv->cats;

	if (priv->is_cmd_win) return;

	while (op) {
		reset_change_bars_in_panel(priv, op->panel);
		op = op->next;
	}

	if (priv->multi_cat) set_cat_changed(priv, FALSE);

	if (xv_get(PROPFRAMEPUB(priv), XV_SHOW))
		update_win_attrs(priv, FALSE, FALSE);
}

static int note_numeric(Panel_item item, int val, Event *ev)
{
	typedef int (*notefunc_t)(Panel_item item, int val, Event *ev);
	notefunc_t orig = (notefunc_t)xv_get(item, XV_KEY_DATA, to_orig_note_key);

	xv_set(xv_get(xv_get(item, XV_OWNER), XV_OWNER),
			FRAME_LEFT_FOOTER, "",
			NULL);
	set_changed(NULL, item, TRUE);

	if (orig) return (*orig)(item, val, ev);

	return XV_OK;
}

static Panel_setting note_multi_text(Panel_item item, Event *ev)
{
	typedef Panel_setting (*notefunc_t)(Panel_item item, Event *ev);
	notefunc_t orig = (notefunc_t)xv_get(item, XV_KEY_DATA, to_orig_note_key);

	set_changed(NULL, item, TRUE);

	xv_set(xv_get(xv_get(item,XV_OWNER),XV_OWNER), FRAME_LEFT_FOOTER, "", NULL);
	if (orig) return (*orig)(item, ev);

	return (int)PANEL_INSERT;
}

static Panel_setting note_text(Panel_item item, Event *ev)
{
	typedef Panel_setting (*notefunc_t)(Panel_item item, Event *ev);
	notefunc_t orig = (notefunc_t)xv_get(item, XV_KEY_DATA, to_orig_note_key);
	Panel_setting val;

	xv_set(xv_get(xv_get(item,XV_OWNER),XV_OWNER), FRAME_LEFT_FOOTER, "", NULL);
	if (orig) val = (*orig)(item, ev);
	else val = panel_text_notify(item, ev);

	if ((event_is_iso(ev) && val == PANEL_INSERT) ||
		event_action(ev) == ACTION_ERASE_CHAR_BACKWARD)
		set_changed(NULL, item, TRUE);

	return val;
}

static Panel_setting note_numeric_text(Panel_item item, Event *ev)
{
	typedef Panel_setting (*notefunc_t)(Panel_item item, Event *ev);
	notefunc_t orig = (notefunc_t)xv_get(item, XV_KEY_DATA, to_orig_note_key);
	Panel_setting val;

	xv_set(xv_get(xv_get(item, XV_OWNER), XV_OWNER),
					FRAME_LEFT_FOOTER, "",
					NULL);
	if (orig) val = (*orig)(item, ev);
	else val = panel_text_notify(item, ev);

	set_changed(NULL, item, TRUE);

	return val;
}

static void propagate_through_panel_data_items(Propframe_private *priv, Panel panel, Xv_opaque master, propagate_action action, Xv_opaque context)
{
	Panel_item it;

	PANEL_EACH_ITEM(panel, it)
		Propframe_item *attach;

		/* eliminate change bars */
		if (PANEL_MESSAGE == (const Xv_pkg *)xv_get(it, CB_TO_TYPE))
			continue;

		/* don't handle items with no attached data */
		if (!(attach = (Propframe_item *)xv_get(it,XV_KEY_DATA,attach_key)))
			continue;

		/* don't handle items with no appropriate data set */
		if (attach->data_offset < 0) continue;

		if (attach->master != master) continue;

		(*action)(priv, it, attach, context);
	PANEL_END_EACH
}

static void propagate_through_data_items(Propframe_private *priv, Xv_opaque master, propagate_action action, Xv_opaque context)
{
	one_panel_t p = priv->cats;

	while (p) {
		propagate_through_panel_data_items(priv,p->panel,master,action,context);
		p = p->next;
	}
}

static void reset_item_chbar(Propframe_private *priv, Panel_item item, Propframe_item *attach, Xv_opaque context)
{
	Panel_item cb = xv_get(item, ITEMS_CHANGEBAR);

	if (cb) xv_set(cb, PANEL_LABEL_STRING, " ", NULL);
}

static void reset_slave_changebars(Propframe_private *priv, Xv_opaque master)
{
	propagate_through_data_items(priv, master, reset_item_chbar, XV_NULL);
}

typedef struct {
	int panel_to_data;
	char *base;
} data_context;

static void process_item(Propframe_private *priv, Panel_item item, Propframe_item *attach, Xv_opaque context)
{
	Attr32_attribute attribute;
	data_context *dc = (data_context *)context;

	if (xv_get(item, XV_IS_SUBTYPE_OF, PANEL_LIST))
		attribute = PANEL_LIST_FIRST_SELECTED;
	else if (xv_get(item, XV_IS_SUBTYPE_OF, PANEL_MESSAGE))
		attribute = XV_Y; /* nothing useful */
	else if (xv_get(item, XV_IS_SUBTYPE_OF, PANEL_ABBREV_MENU_BUTTON))
		attribute = XV_Y; /* nothing useful */
	else if (xv_get(item, XV_IS_SUBTYPE_OF, PANEL_BUTTON))
		attribute = XV_Y; /* nothing useful */
	else attribute = PANEL_VALUE;

	(*(attach->converter))(xv_get(item, attribute),
					dc->panel_to_data,
					(char **)(dc->base + attach->data_offset),
					item,
					attach->client_data);
}

static void process_panel_items_and_data(Propframe_private *priv, char *base, int panel_to_data, Xv_opaque for_master)
{
	data_context context;

	if (! base) return;

	context.panel_to_data = panel_to_data;
	context.base = base;
	propagate_through_data_items(priv, for_master, process_item,
												(Xv_opaque)&context);
}

static int notify_apply(Propframe_private *priv, int triggered)
{
	int val;

	if (priv->buttons_created) {
		process_panel_items_and_data(priv, priv->cur_data, TRUE, XV_NULL);
	}

	if (priv->apply)
		val = (*(priv->apply))(PROPFRAMEPUB(priv), triggered);
	else val = XV_OK;

	if (priv->buttons_created && val == XV_OK) reset_change_bars(priv);

	return val;
}

static void note_internal_apply(Panel_item item)
{
	int val;
	Propframe_private *priv = (Propframe_private *)xv_get(item,
								OBJECT_TO_PRIV);

	xv_set(PROPFRAMEPUB(priv), FRAME_LEFT_FOOTER, "", NULL);
	val = notify_apply(priv, FALSE);

	xv_set(item, PANEL_NOTIFY_STATUS, val, NULL);
}

static int notify_menu_apply(Propframe_private *priv, Propframe_apply_mode mode)
{
	if (priv->apply_2) {
		if (priv->buttons_created) {
			process_panel_items_and_data(priv, priv->cur_data, TRUE, XV_NULL);
		}

		return (priv->apply_2)(PROPFRAMEPUB(priv), priv->holder, mode);
	}

	return XV_OK;
}

static void note_internal_menu_apply(Menu menu, Menu_item item)
{
	int val;
	Propframe_private *priv = (Propframe_private *)xv_get(menu,
								OBJECT_TO_PRIV);

	xv_set(PROPFRAMEPUB(priv), FRAME_LEFT_FOOTER, "", NULL);

	if (priv->holder) {
		int val = notify_menu_apply(priv, FRAME_PROPS_APPLY_ORIG);
		if (val != XV_OK) xv_set(menu, MENU_NOTIFY_STATUS, XV_ERROR, NULL);
	}
	else {
		val = notify_apply(priv, FALSE);

		if (val == XV_OK) {
			xv_set(PROPFRAMEPUB(priv), XV_SHOW, FALSE, NULL);
		}
	}
}

static void notify_non_apply(Xv_opaque item, Propframe_private *priv, propcbproc cb, int reset_change, int triggered)
{
	int val;

	if (cb) val = (*cb)(PROPFRAMEPUB(priv), triggered);
	else val = XV_OK;

	if (val == XV_OK && reset_change) reset_change_bars(priv);

	if (item && xv_get(item, XV_IS_SUBTYPE_OF, PANEL_BUTTON))
		xv_set(item, PANEL_NOTIFY_STATUS, XV_ERROR, NULL);
}

static void note_internal_reset(Xv_opaque item)
{
	Propframe_private *priv = (Propframe_private *)xv_get(item,
								OBJECT_TO_PRIV);

	xv_set(PROPFRAMEPUB(priv), FRAME_LEFT_FOOTER, "", NULL);
	notify_non_apply(item, priv, priv->reset, TRUE, FALSE);
	process_panel_items_and_data(priv, priv->cur_data, FALSE, XV_NULL);
}

static void note_internal_set_defaults(Xv_opaque item)
{
	Propframe_private *priv = (Propframe_private *)xv_get(item,
								OBJECT_TO_PRIV);

	xv_set(PROPFRAMEPUB(priv), FRAME_LEFT_FOOTER, "", NULL);
	process_panel_items_and_data(priv, priv->def_data, TRUE, XV_NULL);
	notify_non_apply(item, priv, priv->set_defaults, FALSE, FALSE);
}

static void note_internal_reset_factory(Xv_opaque item)
{
	Propframe_private *priv = (Propframe_private *)xv_get(item,
								OBJECT_TO_PRIV);

	xv_set(PROPFRAMEPUB(priv), FRAME_LEFT_FOOTER, "", NULL);
	notify_non_apply(item, priv, priv->reset_factory, TRUE, FALSE);
	process_panel_items_and_data(priv, priv->fac_data, FALSE, XV_NULL);
}

static void note_cancel(Xv_opaque item)
{
	Propframe_private *priv = (Propframe_private *)xv_get(item,
								OBJECT_TO_PRIV);

	xv_set(PROPFRAMEPUB(priv), XV_SHOW, FALSE, NULL);
}

static void note_prop_panel_background(Panel pan, Event *ev)
{
	if (event_action(ev) == ACTION_MENU) {
		Frame self = xv_get(pan, XV_OWNER);
		Propframe_private *priv = PROPFRAMEPRIV(self);

		if (! priv->is_cmd_win) {
			if (event_is_down(ev)) {
				if (priv->menu) menu_show(priv->menu, pan, ev, NULL);
			}
			return;
		}
	}

	if (event_action(ev) == ACTION_SELECT && event_is_down(ev)) {
		xv_set(xv_get(pan, XV_OWNER), FRAME_LEFT_FOOTER, "", NULL);
	}

	panel_default_handle_event(pan, ev);
}

static void note_new_selection(Menu menu, Menu_item item)
{
	Propframe_private *priv = (Propframe_private *)xv_get(menu,
										OBJECT_TO_PRIV);
	int val = notify_menu_apply(priv, FRAME_PROPS_APPLY_NEW);
	if (val != XV_OK) xv_set(menu, MENU_NOTIFY_STATUS, XV_ERROR, NULL);
}

static void note_orig_selection(Menu menu, Menu_item item)
{
	Propframe_private *priv = (Propframe_private *)xv_get(menu,
										OBJECT_TO_PRIV);
	int val = notify_menu_apply(priv, FRAME_PROPS_APPLY_ORIG);
	if (val != XV_OK) xv_set(menu, MENU_NOTIFY_STATUS, XV_ERROR, NULL);
}

static void create_menu(Propframe_private *priv)
{
	if (priv->is_cmd_win || priv->menu) return;

	priv->menu = xv_create(XV_SERVER_FROM_WINDOW(PROPFRAMEPUB(priv)), MENU,
						MENU_TITLE_ITEM, XV_MSG("Settings"),
						OBJECT_TO_PRIV, priv,
						NULL);
}

static Notify_value resize_interposer(Panel pan, Notify_event event,
								Notify_arg arg, Notify_event_type type)
{
	Event *ev = (Event *)event;
	Notify_value val = notify_next_event_func(pan, event, arg,type);

	if (event_action(ev) == WIN_RESIZE) {
		one_panel_t p = (one_panel_t)xv_get(pan, XV_KEY_DATA, resize_key);
		int panhig = (int)xv_get(pan, XV_HEIGHT);

		if (p->resize_item) {
			typedef void (*prp_t)(Panel_item,int,Attr_attribute *,int *,int *);
			int stay_in_loop;
			Panel_item it;
			int rowhig, rows, minrows, rest_height;
			Attr_attribute row_attr;
			prp_t resize_proc;

			resize_proc = (prp_t )xv_get(p->resize_item,
												PANEL_ITEM_LAYOUT_PROC);

			if (resize_proc) {
				rest_height = p->cat_initial_height - p->resize_height;

				(*resize_proc)(p->resize_item, panhig - rest_height,
								&row_attr, &rows, &minrows);
			}
			else {
				/* the following attempts didn't work:
				 * +  setting XV_HEIGHT
				 * +  modifying PANEL_ITEM_VALUE_RECT
				 * ... ok, let's go the complicated way....
				 */
				if (xv_get(p->resize_item, XV_IS_SUBTYPE_OF, PANEL_LIST)) {
					rowhig = (int)xv_get(p->resize_item, PANEL_LIST_ROW_HEIGHT);
					row_attr = (Attr_attribute)PANEL_LIST_DISPLAY_ROWS;
					minrows = 3;
				}
				else if (xv_get(p->resize_item,XV_IS_SUBTYPE_OF,PANEL_MULTILINE_TEXT)) {
					Xv_font font;
					Font_string_dims dims;

					font = xv_get(pan, XV_FONT);
					xv_get(font, FONT_STRING_DIMS, "W", &dims);
					rowhig = dims.height;
					row_attr = (Attr_attribute)PANEL_DISPLAY_ROWS;
					minrows = 2;
				}
				else if (xv_get(p->resize_item,XV_IS_SUBTYPE_OF, PANEL_SUBWINDOW)) {
					rowhig = 1;
					row_attr = (Attr_attribute)PANEL_WINDOW_HEIGHT;
					minrows = 10;
				}
				else {
					rowhig = 1;
					row_attr = (Attr_attribute)XV_HEIGHT;
					minrows = 10;
				}

				rest_height = p->cat_initial_height - p->resize_height;
				rows = (panhig - rest_height) / rowhig;
			}

			stay_in_loop = TRUE;

			while (stay_in_loop) {
				Rect r;

				stay_in_loop = FALSE;
				xv_set(p->resize_item, row_attr, rows, NULL);
				r = *((Rect *)xv_get(p->resize_item, XV_RECT));

				PANEL_EACH_ITEM(pan, it)
					int bottom_offset = (int)xv_get(it,
											XV_KEY_DATA, resize_key);

					if (bottom_offset > 0) {
						int new_y = panhig - bottom_offset;

						xv_set(it, XV_Y, new_y, NULL);
						if (new_y <= rect_bottom(&r)) {
							stay_in_loop = TRUE;
						}
					}
				PANEL_END_EACH

				if (rows <= minrows) break;

				--rows;
			}
		}

/* 		panel_paint(pan, PANEL_CLEAR); */

		/* the border was not always repainted completely
		XClearArea((Display *)xv_get(pan, XV_DISPLAY), xv_get(pan, XV_XID),
					0, 0, 0, 0, TRUE);
		*/

		/* we try the same thing as olwm from the window menu's Refresh */
		{
			Display *dpy = (Display *)xv_get(pan, XV_DISPLAY);
			Window w;
			XSetWindowAttributes xswa;

			w = XCreateWindow(dpy, xv_get(pan, XV_XID), 0, 0,
						(unsigned)xv_get(pan, XV_WIDTH), (unsigned)panhig,
						0, (int)CopyFromParent, InputOutput, CopyFromParent,
    					0L, &xswa);
			XMapRaised(dpy, w);
			XDestroyWindow(dpy, w);
		}
	}

	return val;
}

static void finish_panel(Propframe_private *priv, one_panel_t p)
{
	int i, pwid, buttwid, buttons_y = 0, x_gap, next_x = 0;
	Rect *r = (Rect *)0;
	Panel_item apply = (Panel_item)0, items[5];
	Panel curpan = p->panel;
	Frame_props self = PROPFRAMEPUB(priv);
	int need_menu = ((priv->menu == XV_NULL) && !priv->is_cmd_win);

	/* categories NOT YET filled will be not processed now ! */
	if (! p->is_filled) return;

	x_gap = (int)xv_get(curpan, PANEL_ITEM_X_GAP);
	i = 0;

	if (priv->buttonreq[0]) {
		int ygap = (int)xv_get(curpan, PANEL_ITEM_Y_GAP);

		if (!priv->apply_label)
			priv->apply_label = xv_strsave(XV_MSG("Apply"));

		if (priv->holder) {
			if (! priv->apply_menu) {
				priv->new_sel_1 = xv_create(XV_NULL, MENUITEM,
								MENU_STRING, XV_MSG("New Selection"),
								MENU_NOTIFY_PROC, note_new_selection,
								MENU_INACTIVE, TRUE,
								XV_HELP_DATA, "xview:apply_new_selection",
								MENU_RELEASE,
								NULL);
				priv->apply_menu
					= xv_create(XV_SERVER_FROM_WINDOW(PROPFRAMEPUB(priv)), MENU,
								OBJECT_TO_PRIV, priv,
								MENU_ITEM,
									MENU_STRING, XV_MSG("Original Selection"),
									MENU_NOTIFY_PROC, note_orig_selection,
									XV_HELP_DATA, "xview:apply_orig_selection",
									NULL,
								MENU_APPEND_ITEM, priv->new_sel_1,
								NULL);
			}
			p->apply = apply = items[i] = xv_create(curpan, PANEL_BUTTON,
					PANEL_ITEM_LAYOUT_ROLE, PANEL_ROLE_CENTER,
					PANEL_LABEL_STRING, priv->apply_label,
					PANEL_ITEM_MENU, priv->apply_menu,
					PANEL_INACTIVE, priv->read_only,
					OBJECT_TO_PRIV, priv,
					PANEL_NEXT_ROW, (int)((3 * ygap) / 2),
					XV_HELP_DATA, priv->buttonhelps[0],
					NULL);
		}
		else {
			p->apply = apply = items[i] = xv_create(curpan, PANEL_BUTTON,
					PANEL_ITEM_LAYOUT_ROLE, PANEL_ROLE_CENTER,
					PANEL_LABEL_STRING, priv->apply_label,
					PANEL_NOTIFY_PROC, note_internal_apply,
					PANEL_INACTIVE, priv->read_only,
					OBJECT_TO_PRIV, priv,
					PANEL_NEXT_ROW, ygap * 2,
					XV_HELP_DATA, priv->buttonhelps[0],
					NULL);
		}

		xv_set(curpan, PANEL_DEFAULT_ITEM, items[i], NULL);
		i++;
		buttons_y = (int)xv_get(apply, XV_Y);
		r = (Rect *)xv_get(items[i - 1], XV_RECT);
		next_x = rect_right(r) + x_gap;

		if (p == priv->cats) {
			create_menu(priv);

			if (need_menu) {
				priv->apply_item = xv_create(XV_NULL, MENUITEM,
						MENU_STRING, XV_MSG("Apply"),
						MENU_NOTIFY_PROC, note_internal_menu_apply,
						MENU_INACTIVE, priv->read_only,
						XV_HELP_DATA, priv->buttonhelps[0],
						MENU_RELEASE,
						NULL);
				xv_set(priv->menu, MENU_APPEND_ITEM, priv->apply_item, NULL);
			}
		}
	}
	else {
		xv_error(self,
				ERROR_PKG, FRAME_PROPS,
				ERROR_SEVERITY, ERROR_NON_RECOVERABLE,
				ERROR_LAYER, ERROR_PROGRAM,
				ERROR_STRING, "Any property window needs an apply button",
				NULL);
	}

	if (priv->buttonreq[2]) {
		items[i++] = xv_create(curpan, PANEL_BUTTON,
					PANEL_ITEM_LAYOUT_ROLE, PANEL_ROLE_CENTER,
					PANEL_LABEL_STRING, XV_MSG("Set Default"),
					XV_Y, buttons_y,
					XV_X, next_x,
					PANEL_NOTIFY_PROC, note_internal_set_defaults,
					OBJECT_TO_PRIV, priv,
					XV_HELP_DATA, priv->buttonhelps[2],
					NULL);

		r = (Rect *)xv_get(items[i - 1], XV_RECT);
		next_x = rect_right(r) + x_gap;

		if (p == priv->cats) {
			create_menu(priv);

			if (need_menu) 
				xv_set(priv->menu,
					MENU_APPEND_ITEM,
						xv_create(XV_SERVER_FROM_WINDOW(curpan), MENUITEM,
							MENU_STRING, XV_MSG("Set Default"),
							MENU_NOTIFY_PROC, note_internal_set_defaults,
							XV_HELP_DATA, priv->buttonhelps[2],
							MENU_RELEASE,
							NULL),
					NULL);
		}
	}

	if (priv->buttonreq[1]) {
		items[i++] = xv_create(curpan, PANEL_BUTTON,
					PANEL_ITEM_LAYOUT_ROLE, PANEL_ROLE_CENTER,
					PANEL_LABEL_STRING, XV_MSG("Reset"),
					XV_Y, buttons_y,
					XV_X, next_x,
					PANEL_NOTIFY_PROC, note_internal_reset,
					OBJECT_TO_PRIV, priv,
					XV_HELP_DATA, priv->buttonhelps[1],
					NULL);

		r = (Rect *)xv_get(items[i - 1], XV_RECT);
		next_x = rect_right(r) + x_gap;

		if (p == priv->cats) {
			create_menu(priv);

			if (need_menu) {
				xv_set(priv->menu,
					MENU_APPEND_ITEM,
						xv_create(XV_SERVER_FROM_WINDOW(curpan), MENUITEM,
							MENU_STRING, XV_MSG("Reset"),
							MENU_NOTIFY_PROC, note_internal_reset,
							XV_HELP_DATA, priv->buttonhelps[1],
							MENU_RELEASE,
							NULL),
					NULL);

				if (priv->holder) {
					priv->new_sel_2 = xv_create(XV_NULL, MENUITEM,
								MENU_STRING, XV_MSG("Apply to New Selection"),
								MENU_NOTIFY_PROC, note_new_selection,
								MENU_INACTIVE, TRUE,
								XV_HELP_DATA, "xview:apply_new_selection",
								MENU_RELEASE,
								NULL);
					xv_set(priv->menu, MENU_APPEND_ITEM, priv->new_sel_2, NULL);
				}
			}
		}
	}

	if (priv->buttonreq[3]) {
		items[i++] = xv_create(curpan, PANEL_BUTTON,
					PANEL_ITEM_LAYOUT_ROLE, PANEL_ROLE_CENTER,
					PANEL_LABEL_STRING, XV_MSG("Reset to Factory"),
					XV_Y, buttons_y,
					XV_X, next_x,
					PANEL_NOTIFY_PROC, note_internal_reset_factory,
					OBJECT_TO_PRIV, priv,
					XV_HELP_DATA, priv->buttonhelps[3],
					NULL);

		r = (Rect *)xv_get(items[i - 1], XV_RECT);
		next_x = rect_right(r) + x_gap;

		if (p == priv->cats) {
			create_menu(priv);

			if (need_menu) 
				xv_set(priv->menu,
					MENU_APPEND_ITEM,
						xv_create(XV_SERVER_FROM_WINDOW(curpan), MENUITEM,
							MENU_STRING, XV_MSG("Reset to Factory"),
							MENU_NOTIFY_PROC, note_internal_reset_factory,
							XV_HELP_DATA, priv->buttonhelps[3],
							MENU_RELEASE,
							NULL),
					NULL);
		}
	}

	if (defaults_get_boolean("openWindows.cancelButtonInPropFrames",
								"OpenWindows.CancelButtonInPropFrames", FALSE))
	{
		items[i++] = xv_create(curpan, PANEL_BUTTON,
					PANEL_ITEM_LAYOUT_ROLE, PANEL_ROLE_CENTER,
					PANEL_LABEL_STRING, XV_MSG("Cancel"),
					XV_Y, buttons_y,
					XV_X, next_x,
					PANEL_NOTIFY_PROC, note_cancel,
					OBJECT_TO_PRIV, priv,
					NULL);

		if (p == priv->cats) {
			create_menu(priv);

			if (need_menu) 
				xv_set(priv->menu,
					MENU_APPEND_ITEM,
						xv_create(XV_SERVER_FROM_WINDOW(curpan), MENUITEM,
							MENU_STRING, XV_MSG("Cancel"),
							MENU_NOTIFY_PROC, note_cancel,
							MENU_RELEASE,
							NULL),
					NULL);
		}
	}

	window_fit_width(curpan);
	pwid = (int)xv_get(curpan, XV_WIDTH);

	if (priv->multi_cat && apply) {
		r = (Rect *)xv_get(items[i - 1], XV_RECT);
		buttwid = rect_right(r) + (int)xv_get(items[0], XV_X) + 30;
		if (pwid < buttwid + priv->apply_width_diff)
			xv_set(curpan,
						XV_WIDTH, pwid = buttwid + priv->apply_width_diff,
						NULL);
	}
	p->cat_width = p->cat_initial_width = pwid;

	/* for the moment, we only use PANEL_ROLE_CENTER. The rest might be
	 * too complicated because of the changebars....   5.4.2020
	 */
	xv_set(curpan, PANEL_DO_LAYOUT, NULL);

	if (priv->multi_cat && apply) {
		Rect *rect;
		int button_x, button_y;
		Panel pp = xv_get(self, FRAME_PROPS_PANEL);

		rect = (Rect *)xv_get(apply, PANEL_ITEM_RECT);

		win_translate_xy(p->panel, pp,
							rect->r_left, rect->r_top,
							&button_x, &button_y);

		/* we are in a multi category property window and already have
		 * an Apply button in the current panel and we have also set this
		 * button as PANEL_DEFAULT_ITEM in the current panel (so that it
		 * will be displayed with the default ring).
		 * However, pointer warping (via the _OL_DFLT_BTN property) works
		 * only for the default button in the FRAME_PROPS_PANEL.
		 * Therefore, for each category, we create an invisible inactive
		 * Apply button with the 'correct' position.
		 * And we mark it with "XV_KEY_DATA, PANEL_DEFAULT_ITEM, TRUE"
		 */
		p->pseudo_default = xv_create(pp, PANEL_BUTTON,
							PANEL_LABEL_STRING, XV_MSG("Apply"),
							PANEL_INACTIVE, TRUE,
							XV_SHOW, TRUE,
							XV_X, button_x,
							XV_Y, button_y,
							XV_KEY_DATA, PANEL_DEFAULT_ITEM, TRUE,
							NULL);

	}

	window_fit_height(curpan);
	p->cat_initial_height = p->cat_height = (int)xv_get(curpan, XV_HEIGHT);

	/* look whether the apply buttom must be labelled 'Apply All' */
	if (priv->is_apply_all) {
		xv_set(apply,
				XV_X, (int)xv_get(apply, XV_X) - priv->apply_width_diff,
				PANEL_LABEL_STRING, XV_MSG("Apply All"),
				NULL);
	}

	/* be prepared for resizing */
	xv_set(curpan,
			XV_WIDTH, WIN_EXTEND_TO_EDGE,
			XV_HEIGHT, WIN_EXTEND_TO_EDGE,
			XV_KEY_DATA, resize_key, p,
			NULL);

	if (xv_get(self, FRAME_SHOW_RESIZE_CORNER) && p->resize_item != XV_NULL) {
		Panel_item it;

		p->resize_y = (int)xv_get(p->resize_item, XV_Y);

		/* record the distance of items to the bottom of the panel */
		PANEL_EACH_ITEM(curpan, it)
			int y = (int)xv_get(it, XV_Y);

			if (y > p->resize_y + 5) { /* that '5' is just carefulness */
				/* this item is below the resize_item */
				xv_set(it,
						XV_KEY_DATA, resize_key, p->cat_initial_height - y,
						NULL);
			}
		PANEL_END_EACH

		notify_interpose_event_func(curpan, resize_interposer, NOTIFY_SAFE);
		notify_interpose_event_func(curpan, resize_interposer,NOTIFY_IMMEDIATE);
	}
}

static void call_switch(Propframe_private *priv)
{
	char was_filled = priv->current->is_filled;

	if (priv->current->switch_proc) {
		(*(priv->current->switch_proc))(PROPFRAMEPUB(priv),
									priv->current->panel, 
									priv->current->is_filled);
	}

	if (! was_filled) {
		finish_panel(priv, priv->current);
		if (priv->current->is_filled && priv->cur_data) {
			data_context context;

			context.panel_to_data = FALSE;
			context.base = priv->cur_data;
			propagate_through_panel_data_items(priv, priv->current->panel,
								XV_NULL, process_item, (Xv_opaque)&context);
		}
	}
}

static int set_new_current_panel(Propframe_private *priv, one_panel_t p)
{
	Frame self = PROPFRAMEPUB(priv);
	Panel proppan = xv_get(self, FRAME_PROPS_PANEL);
	int delta, wid, height;
	Panel_item it;
	one_panel_t oldp;
	static XEvent xev;
	Event event;

	oldp = priv->current;
	oldp->cat_height = (int)xv_get(oldp->panel, XV_HEIGHT);
	oldp->cat_width = (int)xv_get(oldp->panel, XV_WIDTH);
	priv->current = p;

	call_switch(priv);
	if (p == oldp) return FALSE;

	xv_set(oldp->panel, XV_SHOW, FALSE, NULL);

	delta = (xv_get(proppan, WIN_BORDER) ? (2 * BORDER_WIDTH) : 0);
	wid = p->cat_width + delta;

	/* when this was still uncommented, the panel was not wide enough
	 * if the buttons needed more width than the rest of the panel.
	 */
/* 	xv_set(proppan, XV_WIDTH, wid, NULL); */

	height = (int) xv_get(proppan, XV_HEIGHT) + p->cat_height + delta;
	xv_set(self,
				XV_HEIGHT, height,
				XV_WIDTH, wid,
				NULL);

	if (p->pseudo_default)
		xv_set(proppan, PANEL_DEFAULT_ITEM, p->pseudo_default, NULL);

	xv_set(p->panel, XV_SHOW, TRUE, NULL);
	xv_set(XV_SERVER_FROM_WINDOW(self), SERVER_SYNC_AND_PROCESS_EVENTS, NULL);

	/* many PANEL_MULTILINE_TEXTs do not repaint poperly when
	 * switching categories
	 */
	PANEL_EACH_ITEM(p->panel, it)
		if (xv_get(it, XV_IS_SUBTYPE_OF, PANEL_MULTILINE_TEXT)) {
			Xv_window view = xv_get(it, PANEL_ITEM_NTH_WINDOW, 0);

			/* unreliable: XClearArea */
			/* this is what the panel_op_paint method would also do
			 * and therefore, it doesn't help...
    		 * win_post_id(view, WIN_REPAINT, NOTIFY_SAFE);
			 */
			/* by the way: win_post_id ends up in 
			 * ev_paint_view with a NULL XEvent - this calls
			 * tty_calc_exposed_lines - and there XClearArea is called 
			 * when the XEvent is NULL......
			 *
			 * Let's simulate what win_post_id does - but with a 'real'
			 * Expose event - and let's try NOTIFY_IMMEDIATE
			 */

			xev.type = Expose;
			xev.xexpose.window = xv_get(view, XV_XID);
			xev.xexpose.x = 0;
			xev.xexpose.y = 0;
			xev.xexpose.width = 1000;
			xev.xexpose.height = 1000;
			xev.xexpose.count = 0;

			event_init(&event);
			event_set_id(&event, WIN_REPAINT);
			event_set_window(&event, view);
			event_set_xevent(&event, &xev);
			win_post_event(view, &event, NOTIFY_IMMEDIATE);
		}
	PANEL_END_EACH

	/* that seems to be the case when the panel border is not repainted
	 * on the right side: when the next category is not so wide as the old
	 */
	return (oldp->cat_width > p->cat_width);
}

static void choose_category(Propframe_private *priv, Panel_item item, int value)
{
	one_panel_t p, newcur;
	int i, narrower;


	newcur = priv->cats;
	i = -1;

	while (++i < value) newcur = newcur->next;

	narrower = set_new_current_panel(priv, newcur);
	xv_set(priv->cat_choice,
			PANEL_VALUE, value,
			XV_KEY_DATA, PANEL_VALUE, value,
			NULL);

	p = priv->cats;
	while (p) {
		Panel_item it;

		PANEL_EACH_ITEM(p->panel, it)
			char *bar;
			Panel_item cb = xv_get(it, ITEMS_CHANGEBAR);
			Propframe_item *attach = (Propframe_item *)xv_get(it,
											XV_KEY_DATA,attach_key);

			/* find change bars */
			/* consider only non-slaves with a change bar */
			if (cb && attach && ! attach->master) {
				/* is there any change bar set ? */
				if ((bar = (char *)xv_get(cb, PANEL_LABEL_STRING)) &&
					(*bar == '|')) {

					/*  Yes, (at least) this one is set.
					 *  We set the category's change bar to show that
					 *  in an other category there are changed to be applied.
					 */

					set_cat_changed(priv, TRUE);
					return;
				}
			}
		PANEL_END_EACH

		p = p->next;
	}

	if (narrower) {
		/* didn't help:
		xv_set(newcur->panel, PANEL_BORDER, TRUE, NULL);
		*/

		/* didn't help:
		xev.type = Expose;
		xev.xexpose.window = xv_get(newcur->panel, XV_XID);
		xev.xexpose.x = 0;
		xev.xexpose.y = 0;
		xev.xexpose.width = 1000;
		xev.xexpose.height = 1000;
		xev.xexpose.count = 0;

		event_init(&event);
		event_set_id(&event, WIN_REPAINT);
		event_set_window(&event, newcur->panel);
		event_set_xevent(&event, &xev);
		win_post_event(newcur->panel, &event, NOTIFY_IMMEDIATE);
		*/

		/* didn't help:
		panel_paint(newcur->panel, PANEL_CLEAR);
		*/

		/* didn't help:
		XClearArea((Display *)xv_get(newcur->panel, XV_DISPLAY),
						xv_get(newcur->panel, XV_XID), 0, 0, 0, 0, TRUE);
		*/

		/* NOTHING HELPS !!!!!!   */
	}
}

static void note_cat_choice(Panel_item item, int value)
{
	Propframe_private *priv =
				(Propframe_private *)xv_get(item, OBJECT_TO_PRIV);
 	int ccnt = (int)xv_get(item, XV_KEY_DATA, PANEL_DEFAULT_VALUE);

	xv_set(PROPFRAMEPUB(priv), FRAME_LEFT_FOOTER, "", NULL);

	/* do we have a selfmade Next item ? */
	if (ccnt > 0 && value == ccnt) {
		int oldval = (int)xv_get(item, XV_KEY_DATA, PANEL_VALUE);

		value = (1 + oldval) % ccnt;
	}
	choose_category(priv, item, value);
}

static void create_buttons(Propframe_private *priv, Attr_attribute *which)
{
	int i;
	Panel pan;
	one_panel_t p = priv->cats;
	Frame_props self = PROPFRAMEPUB(priv);

	if (priv->buttons_created) {
		xv_error(self,
					ERROR_PKG, FRAME_PROPS,
					ERROR_SEVERITY, ERROR_RECOVERABLE,
					ERROR_LAYER, ERROR_PROGRAM,
					ERROR_STRING, "second FRAME_PROPS_CREATE_BUTTONS ignored",
					NULL);

		return;
	}

	priv->buttons_created = TRUE;

	for (i = 0; which[i]; i += 2) {
		priv->buttonreq[(int)which[i] - 1] = TRUE;
		if ((int)which[i+1] == -1)
			priv->buttonhelps[(int)which[i] - 1] = (char *)0;
		else priv->buttonhelps[(int)which[i] - 1] =
												xv_strsave((char *)which[i+1]);
	}

	if (priv->multi_cat) {
		Font_string_dims dims1, dims2;
		Xv_font font;

		font = xv_get(xv_get(self, FRAME_PROPS_PANEL), XV_FONT);
		xv_get(font, FONT_STRING_DIMS, XV_MSG("Apply"), &dims1);
		xv_get(font, FONT_STRING_DIMS, XV_MSG("Apply All"), &dims2);
		priv->apply_width_diff = dims2.width - dims1.width;
		priv->is_apply_all = FALSE;

		if (priv->cat_choice) {
			if (! xv_get(priv->cat_choice, PANEL_ITEM_CYCLIC)) {
				/* maybe because of several columns */
				xv_set(priv->cat_choice,
					PANEL_CHOICE_STRING, priv->choice_cnt,
										XV_MSG("\253 Next \273"),
					NULL);
				xv_set(priv->cat_choice,
 					XV_KEY_DATA, PANEL_DEFAULT_VALUE, priv->choice_cnt,
 					PANEL_DEFAULT_VALUE, priv->choice_cnt,
					NULL);
			}
		}
	}

	while (p) {
		finish_panel(priv, p);
		p = p->next;
	}

	if (! priv->multi_cat) {
		int ow, oh;

		pan = xv_get(self, FRAME_PROPS_PANEL);
		window_fit(self);
		ow = (int)xv_get(self, XV_WIDTH);
		oh = (int)xv_get(self, XV_HEIGHT);

		/* die folgenden beiden Aufrufe hamm's anscheinend geschafft */
		xv_set(self,
				XV_WIDTH, ow + 1,
				XV_HEIGHT, oh + YOFF + 1,
				NULL);
		xv_set(XV_SERVER_FROM_WINDOW(self), SERVER_SYNC_AND_PROCESS_EVENTS, NULL);

		xv_set(self,
				XV_WIDTH, ow,
				XV_HEIGHT, oh + YOFF,
				NULL);


		if (xv_get(self, FRAME_SHOW_RESIZE_CORNER)) {
			Panel_item item;
			int rw_key, rw = FALSE;

			/* find out whether something is resizable horizontally */
			rw_key = xv_get_rwid_key();
			PANEL_EACH_ITEM(pan, item)
				if (xv_get(item, XV_KEY_DATA, rw_key)) rw = TRUE;
			PANEL_END_EACH

			xv_set_frame_resizing(self, rw, NULL, NULL);
			xv_set(pan,
					XV_Y, YOFF, /* to prevent the drawing of the line */
					NULL);
		}
		else {
			xv_set(pan,
					XV_Y, YOFF, /* to prevent the drawing of the line */
#ifdef hat_auch_nicht_immer_die_richtige_groesse_gehabt
					XV_WIDTH, WIN_EXTEND_TO_EDGE,
					XV_HEIGHT, WIN_EXTEND_TO_EDGE,
#endif
					XV_WIDTH, ow,
					XV_HEIGHT, oh,
					NULL);

			/* ich werde noch verrueckt - es gibt immer wieder
			 * Situationen, wo ich PropertyFrames (besonders gern
			 * fileprops oder colorchooser) sehe, die diese verdammte
			 * Defaultgroesse haben... bei denen hat zwar dann das 
			 * Panel dies richtige Groesse, aber die NormalHints 
			 * und damit dann wohl auch der Frame sind falsch:
			 */
			xv_set(self,
					FRAME_MIN_SIZE, ow, oh,
					FRAME_MAX_SIZE, ow, oh,
					NULL);
		}
		xv_set(XV_SERVER_FROM_WINDOW(self), SERVER_SYNC_AND_PROCESS_EVENTS, NULL);
	}
}

static void first_popup(Frame_props self)
{
	xv_set(self, XV_SHOW, TRUE, NULL);
}

static void free_key_data(Xv_opaque obj, int key, char *data)
{
	if (data) xv_free(data);
}

static Panel_item create_panel_item(Propframe_private *priv, int layout, const Xv_pkg *pkg, Attr_avlist avlist)
{
	Panel_item newitem, cb = XV_NULL;
	Attr_attribute *attrs;
	convert_t converter = (convert_t)0;
	Xv_opaque client_data = (Xv_opaque)0;
	Xv_opaque master = (Xv_opaque)0;
	int data_offset = -1;
	int appl_assigned_changebar = FALSE;
	Propframe_item *attach = (Propframe_item *)0;
	Panel_item_role role;
	int is_resize_item = FALSE;

	SERVERTRACE((333, "create_panel_item: %s\n", pkg->name));
	/* parse for our attributes */
	for (attrs=avlist; *attrs; attrs=attr_next(attrs)) switch ((int)*attrs) {
		case FRAME_PROPS_DATA_OFFSET:
			data_offset = (int)A1;
			ADONE;
		case FRAME_PROPS_SLAVE_OF:
			master = (Xv_opaque)A1;
			ADONE;
		case FRAME_SHOW_RESIZE_CORNER:
			is_resize_item = (int)A1;
			ADONE;
		case FRAME_PROPS_CONVERTER:
			converter = (convert_t)A1;
			client_data = (Xv_opaque)A2;
			ADONE;
		case XV_KEY_DATA:
			if ((int)A1 == to_change_bar_key) appl_assigned_changebar = TRUE;
			break;
	}

	if (layout != FRAME_PROPS_NO_LAYOUT && layout != FRAME_PROPS_MOVE) {
		if (! priv->is_cmd_win) {
			cb = xv_create(priv->current->panel, PANEL_MESSAGE,
						PANEL_LABEL_STRING, " ",
						CB_TO_TYPE, PANEL_MESSAGE,
						XV_KEY_DATA, attach_key, priv,
						PANEL_NEXT_ROW, layout,
						XV_HELP_DATA, "xview:propframe_changebar",
						NULL);

			priv->last_changebar = cb;
		}
	}

	switch (layout) {
		case FRAME_PROPS_NO_LAYOUT:
			role = PANEL_ROLE_NONE;
			break;
		case FRAME_PROPS_MOVE:
			role = PANEL_ROLE_FOLLOWER;
			break;
		default:
			role = PANEL_ROLE_LEADER;
			break;
	}

	priv->current->is_filled = TRUE;
	newitem = xv_create(priv->current->panel, pkg,
							ATTR_LIST, avlist,
							PANEL_ITEM_LAYOUT_ROLE, role,
							NULL);

	if (role == PANEL_ROLE_LEADER) {
		priv->last_leader = newitem;
	}
	else if (role == PANEL_ROLE_FOLLOWER) {
		if (priv->last_leader) {
			int lead_y = (int)xv_get(priv->last_leader, XV_Y);
			int new_y = (int)xv_get(newitem, XV_Y);

			if (new_y > lead_y) {
				/* the panel might have been too narrow */
				Rect *r = (Rect *)xv_get(priv->last_leader, XV_RECT);

				xv_set(newitem,
							XV_X, rect_right(r) + 6,
							XV_Y, lead_y,
							NULL);
			}
		}
	}

	if (is_resize_item) {
		priv->current->resize_item = newitem;
		priv->current->resize_height = (int)xv_get(newitem, XV_HEIGHT);
	}

	if (layout == FRAME_PROPS_MOVE && ! appl_assigned_changebar) {
		cb = priv->last_changebar;
	}

	if (converter || client_data || data_offset >= 0) {
		attach = (Propframe_item *)xv_alloc(Propframe_item);

		if (!converter) {
			int i;

			for (i = 0; i < NUMBER(converters); i++) {
				if (xv_get(newitem, XV_IS_SUBTYPE_OF, converters[i].pkg)) {
					converter = converters[i].proc;
					break;
				}
			}

			if (!converter) converter = convert_nil;
		}

		attach->data_offset = data_offset;
		attach->converter = converter;
		attach->client_data = client_data;
		attach->master = master;
	}

	if ((layout != FRAME_PROPS_NO_LAYOUT && layout != FRAME_PROPS_MOVE) ||
		(layout == FRAME_PROPS_MOVE && ! appl_assigned_changebar))
	{
		long items_notification = (long)xv_get(newitem, PANEL_NOTIFY_PROC);

		if ((int)xv_get(newitem, XV_IS_SUBTYPE_OF, PANEL_CHOICE) ||
			(int)xv_get(newitem, XV_IS_SUBTYPE_OF, PANEL_SLIDER))
		{
			xv_set(newitem, PANEL_NOTIFY_PROC, note_numeric, NULL);
		}
		else if (xv_get(newitem, XV_IS_SUBTYPE_OF, PANEL_NUMERIC_TEXT)) {
			xv_set(newitem, PANEL_NOTIFY_PROC, note_numeric_text, NULL);
		}
		else if (xv_get(newitem, XV_IS_SUBTYPE_OF, PANEL_TEXT)) {
			xv_set(newitem,
					PANEL_NOTIFY_PROC, note_text,
					PANEL_NOTIFY_LEVEL, PANEL_ALL,
					NULL);
		}
		else if (xv_get(newitem, XV_IS_SUBTYPE_OF, PANEL_MULTILINE_TEXT)) {
			xv_set(newitem,
					PANEL_NOTIFY_PROC, note_multi_text,
					PANEL_NOTIFY_LEVEL, PANEL_ALL,
					NULL);

			if (items_notification == (long)FRAME_PROPS_MULTILINE_INSERT_ALL) {
				items_notification = 0L;
			}
		}
		else {
			items_notification = 0L;
		}

		xv_set(newitem,
					ITEMS_CHANGEBAR, cb,
					XV_KEY_DATA, to_orig_note_key, items_notification,
					NULL);
	}

	xv_set(newitem, XV_KEY_DATA, attach_key, attach,
					XV_KEY_DATA_REMOVE_PROC, attach_key, free_key_data,
					NULL);

	if (layout == FRAME_PROPS_MOVE) {
		priv->moves[priv->move_count++] = newitem;
		if (priv->move_count >= priv->max_items) {
			xv_error(PROPFRAMEPUB(priv),
					ERROR_PKG, FRAME_PROPS,
					ERROR_SEVERITY, ERROR_RECOVERABLE,
					ERROR_LAYER, ERROR_PROGRAM,
					ERROR_STRING, "FRAME_PROPS_MAX_ITEMS value to small",
					NULL);
		}
	}
	else if (layout != FRAME_PROPS_NO_LAYOUT) {
		priv->aligns[priv->align_count++] = newitem;
		if (priv->align_count >= priv->max_items) {
			xv_error(PROPFRAMEPUB(priv),
					ERROR_PKG, FRAME_PROPS,
					ERROR_SEVERITY, ERROR_RECOVERABLE,
					ERROR_LAYER, ERROR_PROGRAM,
					ERROR_STRING, "FRAME_PROPS_MAX_ITEMS value to small",
					NULL);
		}
	}

	return newitem;
}

static int propframe_init(Frame owner, Xv_opaque slf, Attr_avlist avlist,
							int *quatsch)
{
	Xv_propframe *self = (Xv_propframe *)slf;
	Propframe_private *priv = (Propframe_private *)xv_alloc(Propframe_private);
	Attr_attribute *attrs;

	if (!priv) return XV_ERROR;

	priv->public_self = (Xv_opaque)self;
	self->private_data = (Xv_opaque)priv;

	init_keys();

	priv->max_items = MAX_ITEMS;
	priv->cat_cols = 1;
	priv->cat_label = xv_strsave(XV_MSG("Category:"));

	for (attrs=avlist; *attrs; attrs=attr_next(attrs)) switch ((int)*attrs) {
		case FRAME_PROPS_MULTIPLE_CATEGORIES:
			priv->multi_cat = (char)A1;
			*attrs = FRAME_CMD_PANEL_BORDERED;
			A1 = FALSE;
			break;

		case FRAME_PROPS_MAX_ITEMS:
			priv->max_items = (int)A1;
			if (priv->max_items <= 0) priv->max_items = 1;
			ADONE;

		case FRAME_PROPS_HOLDER:
			priv->holder = (Xv_opaque)A1;
			ADONE;
	}

	priv->aligns = (Xv_opaque *)xv_alloc_n(Xv_opaque, (size_t)priv->max_items);
	priv->moves = (Xv_opaque *)xv_alloc_n(Xv_opaque, (size_t)priv->max_items);

	if (priv->multi_cat) {
		/* here the FRAME_PROPS_PANEL is not yet created */
	}
	else {
		priv->cats = priv->current = (one_panel_t)xv_alloc(struct _one_panel);
	}

	return XV_OK;
}

static Xv_opaque propframe_set(Frame_props self, Attr_avlist avlist)
{
	Attr_attribute *attrs;
	Propframe_private *priv = PROPFRAMEPRIV(self);
	Panel ppan, cpan;
	char *help;
	Panel_status *statp;

	for (attrs=avlist; *attrs; attrs=attr_next(attrs)) switch ((int)*attrs) {
		case FRAME_PROPS_MULTIPLE_CATEGORIES:
		case FRAME_PROPS_MAX_ITEMS:
			xv_error(self,
					ERROR_PKG, FRAME_PROPS,
					ERROR_SEVERITY, ERROR_RECOVERABLE,
					ERROR_LAYER, ERROR_PROGRAM,
					ERROR_CREATE_ONLY, A0,
					NULL);
			ADONE;

		case FRAME_PROPS_APPLY_LABEL:
			priv->apply_label = xv_strsave((char *)A1);
			priv->is_cmd_win = TRUE;
			ADONE;

		case FRAME_PROPS_FRONT_CATEGORY:
			if (priv->multi_cat) {
				Panel reqpan = (Panel)A1;
				one_panel_t p;
				int i;

				for (i = 0, p = priv->cats; p; p = p->next, i++) {
					if (reqpan == p->panel) {
						/* do the same as if the user had chosen
						 * that category from the categories choice
						 */
						choose_category(priv, priv->cat_choice, i);
						break;
					}
				}
			}
			ADONE;
		case FRAME_PROPS_READ_ONLY:
			{
				one_panel_t p;

				priv->read_only = (char)A1;
				if (priv->apply_item)
					xv_set(priv->apply_item, MENU_INACTIVE, (int)A1, NULL);

				for (p = priv->cats; p; p = p->next) {
					if (p->apply) xv_set(p->apply, PANEL_INACTIVE, (int)A1, NULL);
				}
			}
			ADONE;
		case FRAME_PROPS_RESET_ON_SWITCH:
			priv->reset_on_switch = (char)A1; /* unused at the moment */
			ADONE;
		case FRAME_PROPS_CREATE_CONTENTS_PROC:
			priv->create_contents_proc = (create_proc_t)A1;
			ADONE;
		case FRAME_PROPS_SWITCH_PROC:
			if (priv->current) priv->current->switch_proc = (propswitchproc)A1;
			else {
				xv_error(self,
						ERROR_PKG, FRAME_PROPS,
						ERROR_SEVERITY, ERROR_RECOVERABLE,
						ERROR_LAYER, ERROR_PROGRAM,
						ERROR_STRING, "no current panel yet",
						NULL);
			}
			ADONE;
		case FRAME_PROPS_DATA_ADDRESS:
			priv->cur_data = (char *)A1;
			ADONE;
		case FRAME_PROPS_DEFAULT_DATA_ADDRESS:
			priv->def_data = (char *)A1;
			ADONE;
		case FRAME_PROPS_FACTORY_DATA_ADDRESS:
			priv->fac_data = (char *)A1;
			ADONE;
		case FRAME_PROPS_APPLY_PROC:
			if (priv->holder) {
				priv->apply_2 = (prop2apply_t)A1;
			}
			else {
				priv->apply = (propcbproc)A1;
			}
			ADONE;
		case FRAME_PROPS_RESET_PROC:
			priv->reset = (propcbproc)A1;
			ADONE;
		case FRAME_PROPS_SET_DEFAULTS_PROC:
			priv->set_defaults = (propcbproc)A1;
			ADONE;
		case FRAME_PROPS_RESET_FACTORY_PROC:
			priv->reset_factory = (propcbproc)A1;
			ADONE;
		case FRAME_PROPS_ITEM_CHANGED:
			set_changed(priv, (Panel_item)A1, (int)A2);
			ADONE;
		case FRAME_PROPS_CREATE_BUTTONS:
			create_buttons(priv, &A1);
			ADONE;
		case FRAME_PROPS_TRIGGER_SLAVES:
			process_panel_items_and_data(priv, (char *)A3,
					(Propframe_buttons)A1 == FRAME_PROPS_APPLY,
					(Xv_opaque)A2);
			ADONE;
		case FRAME_PROPS_RESET_SLAVE_CBS:
			reset_slave_changebars(priv, (Xv_opaque)A1);
			ADONE;
		case FRAME_PROPS_CREATE_ITEM:
			if (A1 == (Attr_attribute)FRAME_PROPS_ITEM_SPEC) {
				Panel_item newitem;
				Panel_item *ret = (Panel_item *)A2;

				if (! priv->current) {
					xv_error(self,
							ERROR_PKG, FRAME_PROPS,
							ERROR_SEVERITY, ERROR_RECOVERABLE,
							ERROR_LAYER, ERROR_PROGRAM,
							ERROR_STRING, "no current panel yet",
							NULL);

					ADONE;
				}

				if (! priv->current->panel) {
					xv_error(self,
							ERROR_PKG, FRAME_PROPS,
							ERROR_SEVERITY, ERROR_RECOVERABLE,
							ERROR_LAYER, ERROR_PROGRAM,
							ERROR_STRING, "no current panel yet",
							NULL);

					ADONE;
				}

				newitem = create_panel_item(priv,(int)A3,(const Xv_pkg *)A4,attrs+5);

				if (ret) *ret = newitem;
			}
			else {
				xv_error(self,
					ERROR_PKG, FRAME_PROPS,
					ERROR_SEVERITY, ERROR_RECOVERABLE,
					ERROR_LAYER, ERROR_PROGRAM,
					ERROR_STRING,
						"FRAME_PROPS_ITEM_SPEC expected after FRAME_PROPS_CREATE_ITEM",
					NULL);
			}
			ADONE;
		case FRAME_PROPS_ALIGN_ITEMS:
			align_labels(priv->aligns, priv->align_count,
						priv->moves, priv->move_count);
			priv->align_count = 0;
			priv->move_count = 0;
			ADONE;
		case FRAME_PROPS_ITEM_SPEC:
			/* this is only for ..._CREATE_ITEM to work */
			ADONE;
		case FRAME_PROPS_RESET_CHANGE_BARS:
			reset_change_bars(priv);
			ADONE;
		case FRAME_PROPS_CATEGORY_LABEL:
			if (priv->cat_label) xv_free(priv->cat_label);
			priv->cat_label = xv_strsave((char *)A1);
			ADONE;
		case FRAME_PROPS_CATEGORY_NCOLS:
			priv->cat_cols = (int)A1;
			ADONE;
		case FRAME_PROPS_NEW_CATEGORY:
			if (priv->multi_cat) {
				Panel *appan = (Panel *)A2;
				one_panel_t q, p = (one_panel_t)xv_alloc(struct _one_panel);
				char catinstname[30];

				priv->align_count = 0;
				priv->move_count = 0;

				/* I don't use the category label, as this might
				 * be locale dependent
				 */
				sprintf(catinstname, "category_panel_%d", priv->choice_cnt + 1);

				*appan = p->panel = xv_create(self, PANEL,
						XV_INSTANCE_NAME, catinstname,
						XV_X, 0,
						WIN_BELOW, xv_get(self, FRAME_PROPS_PANEL),
						PANEL_BACKGROUND_PROC, note_prop_panel_background,
						PANEL_BORDER, TRUE,
						PANEL_TO_ONE_PANEL, p,
						XV_WIDTH, 1000,
						XV_USE_DB,
							/* did not work: XV_WIDTH, 1000, */
							PANEL_ITEM_Y_GAP, 13,
							NULL,
						NULL);

				xv_set(priv->cat_choice,
						PANEL_CHOICE_STRING, priv->choice_cnt, A1,
						PANEL_VALUE, 0,
						XV_KEY_DATA, PANEL_VALUE, 0,
						NULL);
				priv->choice_cnt++;

				if (priv->cats) {
					for (q = priv->cats; q->next; q = q->next);
					q->next = p;
				}
				else priv->cats = p;

				priv->current = p;
			}
			else {
				xv_error(self,
						ERROR_PKG, FRAME_PROPS,
						ERROR_SEVERITY, ERROR_RECOVERABLE,
						ERROR_LAYER, ERROR_PROGRAM,
						ERROR_STRING, "Frame is single category",
						NULL);
			}
			ADONE;

		case FRAME_PROPS_TRIGGER:
			switch ((Propframe_buttons)A1) {
				case FRAME_PROPS_APPLY:
					(void)notify_apply(priv, TRUE);
					break;
				case FRAME_PROPS_RESET:
					notify_non_apply(self, priv, priv->reset, TRUE, TRUE);
					process_panel_items_and_data(priv, priv->cur_data,
														FALSE, XV_NULL);
					break;
				case FRAME_PROPS_SET_DEFAULTS:
					process_panel_items_and_data(priv, priv->def_data,
														TRUE, XV_NULL);
					notify_non_apply(self, priv, priv->set_defaults,
														FALSE, TRUE);
					break;
				case FRAME_PROPS_RESET_FACTORY:
					notify_non_apply(self, priv, priv->reset_factory,
														TRUE, TRUE);
					process_panel_items_and_data(priv, priv->fac_data,
														FALSE, XV_NULL);
					break;
				default: break;
			}
			ADONE;

		case FRAME_NEXT_PANE:
			if (! priv->multi_cat) break;

			ppan = xv_get(self, FRAME_PROPS_PANEL);
			cpan = priv->current->panel;
			statp = (Panel_status *)xv_get(ppan, PANEL_STATUS);
			if (statp->has_input_focus) {
				if (xv_set(cpan, WIN_SET_FOCUS, NULL) == XV_OK) {
					xv_set(cpan, XV_FOCUS_ELEMENT, 0, NULL);
				}
			}
			else {
				if (xv_set(ppan, WIN_SET_FOCUS, NULL) == XV_OK) {
					xv_set(ppan, XV_FOCUS_ELEMENT, 0, NULL);
				}
			}
			ADONE;

		case FRAME_PREVIOUS_ELEMENT:
		case FRAME_PREVIOUS_PANE:
			if (! priv->multi_cat) break;

			ppan = xv_get(self, FRAME_PROPS_PANEL);
			cpan = priv->current->panel;
			statp = (Panel_status *)xv_get(ppan, PANEL_STATUS);
			if (statp->has_input_focus) {
				if (xv_set(cpan, WIN_SET_FOCUS, NULL) == XV_OK) {
					xv_set(cpan, XV_FOCUS_ELEMENT, -1, NULL);
				}
			}
			else {
				if (xv_set(ppan, WIN_SET_FOCUS, NULL) == XV_OK) {
					xv_set(ppan, XV_FOCUS_ELEMENT, -1, NULL);
				}
			}
			ADONE;

		case FRAME_PROPS_SECOND_SEL:
			priv->second_sel = (char)A1;
			if (priv->new_sel_1) {
				xv_set(priv->new_sel_1, MENU_INACTIVE, !((int)A1), NULL);
				xv_set(priv->new_sel_2, MENU_INACTIVE, !((int)A1), NULL);
			}
			ADONE;

		case XV_SHOW:
			if ((int)A1) {
				if (! priv->buttons_created && priv->create_contents_proc) {
					Frame base = xv_get(self, XV_OWNER);

					/* do not make me busy: in XV3.2 all multi line texts
					 * will inherit the busy cursor permanently !
					 */
					if (base) xv_set(base, FRAME_BUSY, TRUE, NULL);

					(*(priv->create_contents_proc))(self);

					if (base) xv_set(base, FRAME_BUSY, FALSE, NULL);

					if (A1 == (Attr_attribute)XV_AUTO_CREATE) {
						ADONE;
					}
				}
				A1 = (Attr_attribute)TRUE;
			}

			if ((int)A1 && ! xv_get(self, XV_SHOW)) {
				/* a real transition from unmapped tp mapped */

				update_win_attrs(priv, FALSE, TRUE);
			}

			if ((int)A1 && priv->multi_cat) {
				/* I am popping up */
				one_panel_t p = priv->cats;
				int i, val;

				if (priv->cat_choice) {
					val = (int)xv_get(priv->cat_choice, PANEL_VALUE);

					i = 0;
					while (p) {
						if (i == val) {
							set_new_current_panel(priv, p);
							xv_set(priv->cat_choice,
									XV_KEY_DATA, PANEL_VALUE, val,
									NULL);
						}
						else {
							xv_set(p->panel, XV_SHOW, FALSE, NULL);
						}
						++i;
						p = p->next;
					}
				}
				else {
					/* no cat_choice yet, we are probably in the
					 * initialize phase !
					 */

					/* set up a timer for popping up as soon as
					 * the dispatching starts
					 */
					xv_perform_soon(self, first_popup);

					/* hide that XV_SHOW */
					ADONE;
				}
			}

			if ((int)A1) {
				/* this is the same as in TRIGGER_RESET */
				notify_non_apply(self, priv, priv->reset, TRUE, TRUE);
				process_panel_items_and_data(priv,priv->cur_data,FALSE,XV_NULL);
			}

			if (! (int)A1) {
				if (priv->holder && priv->apply_2) {
					(priv->apply_2)(self, priv->holder,
										FRAME_PROPS_APPLY_RELEASE);
				}
			}
			break;

		case XV_END_CREATE:
			ppan = xv_get(self, FRAME_PROPS_PANEL);

			if ((help = (char *)xv_get(self, XV_HELP_DATA)))
				xv_set(ppan, XV_HELP_DATA, help, NULL);

			if (priv->multi_cat) {
				char helpbuf[100];
				Panel_item cat_cb;

				sprintf(helpbuf, "%s_categ_cb", help ? help :"xview:propframe");
				cat_cb = xv_create(ppan, PANEL_MESSAGE,
						PANEL_LABEL_STRING, " ",
						CB_TO_TYPE, PANEL_MESSAGE,
						XV_KEY_DATA, attach_key, priv,
						XV_HELP_DATA, xv_strsave(helpbuf),
						XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_key_data,
						NULL);

				sprintf(helpbuf, "%s_category", help ? help :"xview:propframe");
				priv->cat_choice = xv_create(ppan, PANEL_CHOICE,
						PANEL_DISPLAY_LEVEL, PANEL_CURRENT,
						PANEL_ITEM_CYCLIC, TRUE,
						PANEL_LABEL_STRING, priv->cat_label,
						PANEL_CHOICE_NCOLS, priv->cat_cols,
						PANEL_NOTIFY_PROC, note_cat_choice,
						ITEMS_CHANGEBAR, cat_cb,
						OBJECT_TO_PRIV, priv,
						XV_HELP_DATA, xv_strsave(helpbuf),
						XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_key_data,
						NULL);
				xv_set(cat_cb, XV_Y, (int)xv_get(cat_cb, XV_Y) + 4, NULL);
				window_fit_height(ppan);
				xv_set(ppan, XV_WIDTH, WIN_EXTEND_TO_EDGE, NULL);
			}
			else {
				/* single category, we don't want to see that line */
				priv->current->panel = ppan;
				xv_set(self,
						WIN_CMS, xv_get(ppan, WIN_CMS),
						WIN_BACKGROUND_COLOR, xv_get(ppan,WIN_BACKGROUND_COLOR),
						NULL);

				xv_set(ppan,
						PANEL_BACKGROUND_PROC, note_prop_panel_background,
						PANEL_BORDER, TRUE,
						PANEL_TO_ONE_PANEL, priv->current,
						NULL);
			}

#ifdef NO_LONGER
			if (priv->create_contents_proc)
				xvwp_set_popup(self, XV_AUTO_CREATE, NULL);
			else xvwp_set_popup(self, NULL);
#endif

			break;

		default: 
			break;
	}

	return XV_OK;
}

/*ARGSUSED*/
static Xv_opaque propframe_get(Frame_props self, int *status, Attr_attribute attr, va_list vali)
{
	Propframe_private *priv = PROPFRAMEPRIV(self);

	*status = XV_OK;
	switch ((int)attr) {
		case FRAME_PROPS_CREATE_BUTTONS:
		case FRAME_PROPS_CREATE_ITEM:
		case FRAME_PROPS_ITEM_SPEC:
		case FRAME_PROPS_RESET_CHANGE_BARS:
			return (Xv_opaque)0;

/* 		case FRAME_PROPS_NEW_CATEGORY: */

		case FRAME_PROPS_RESET_ON_SWITCH:
			return (Xv_opaque)priv->reset_on_switch;
		case FRAME_PROPS_READ_ONLY:
			return (Xv_opaque)priv->read_only;
		case FRAME_PROPS_ITEM_CHANGED:
			{
				Panel_item item = va_arg(vali, Panel_item);
				Panel_item cb = xv_get(item, ITEMS_CHANGEBAR);
				char *lab = (char *)xv_get(cb, PANEL_LABEL_STRING);

				if (lab) return (Xv_opaque)(*lab == '|');
				else return (Xv_opaque)FALSE;
			}
		case FRAME_PROPS_SWITCH_PROC:
			return (Xv_opaque)priv->current->switch_proc;
		case FRAME_PROPS_DATA_ADDRESS:
			return (Xv_opaque)priv->cur_data;
		case FRAME_PROPS_DEFAULT_DATA_ADDRESS:
			return (Xv_opaque)priv->def_data;
		case FRAME_PROPS_FACTORY_DATA_ADDRESS:
			return (Xv_opaque)priv->fac_data;
		case FRAME_PROPS_MULTIPLE_CATEGORIES:
			return (Xv_opaque)priv->multi_cat;
		case FRAME_PROPS_MAX_ITEMS:
			return (Xv_opaque)priv->max_items;
		case FRAME_PROPS_CATEGORY_LABEL:
			return (Xv_opaque)priv->cat_label;
		case FRAME_PROPS_CATEGORY_NCOLS:
			return (Xv_opaque)priv->cat_cols;
		case FRAME_PROPS_APPLY_PROC:
			return (Xv_opaque)priv->apply;
		case FRAME_PROPS_RESET_PROC:
			return (Xv_opaque)priv->reset;
		case FRAME_PROPS_SET_DEFAULTS_PROC:
			return (Xv_opaque)priv->set_defaults;
		case FRAME_PROPS_RESET_FACTORY_PROC:
			return (Xv_opaque)priv->reset_factory;
		case FRAME_PROPS_CREATE_CONTENTS_PROC:
			return (Xv_opaque)priv->create_contents_proc;
		case FRAME_PROPS_HAS_OPEN_CHANGES:
			return (Xv_opaque)priv->is_cancel;
		case FRAME_PROPS_HOLDER:
			return priv->holder;
		case FRAME_PROPS_SECOND_SEL:
			return (Xv_opaque)priv->second_sel;

		case FRAME_WINTYPE:
			return xv_get(XV_SERVER_FROM_WINDOW(self), SERVER_WM_WT_PROP);

		default:
			*status = XV_ERROR;
			return (Xv_opaque)XV_OK;

	}
}

static void destroy_cats(one_panel_t cat)
{
	if (cat) {
		destroy_cats(cat->next);
		xv_destroy(cat->panel);
		xv_free(cat);
	}
}

static int propframe_destroy(Frame_props self, Destroy_status status)
{
	Propframe_private *priv;
	int n;

	if (status != DESTROY_CLEANUP) return XV_OK;

	priv = PROPFRAMEPRIV(self);
	if (priv->apply_label) xv_free(priv->apply_label);
	if (priv->menu) {
		Menu_item it;

		while ((it = xv_get(priv->menu, MENU_NTH_ITEM, 1))) {
			xv_set(priv->menu, MENU_REMOVE_ITEM, it, NULL);
			xv_destroy(it);
		}
		xv_destroy(priv->menu);
	}

	for (n = 0; n < NUMBER(priv->buttonhelps); n++) {
		if (priv->buttonhelps[n]) xv_free(priv->buttonhelps[n]);
	}

	destroy_cats(priv->cats);

	xv_free(priv->aligns);
	xv_free(priv->moves);
	if (priv->cat_label) xv_free(priv->cat_label);
	memset((char *)priv, 0, sizeof(Propframe_private));
	xv_free(priv);
	return XV_OK;
}

const Xv_pkg xv_propframe_pkg = {
	"PropertyFrame",
    (Attr_pkg) ATTR_PKG_FRAME,
	sizeof(Xv_propframe),
	FRAME_CMD,
	propframe_init,
	propframe_set,
	propframe_get,
	propframe_destroy,
	NULL
};
