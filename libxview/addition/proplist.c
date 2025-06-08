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
#include <string.h>
#include <xview/proplist.h>
#include <xview/scrollbar.h>
#include <xview/openmenu.h>
#include <xview_private/i18n_impl.h>

#ifndef lint
char proplist_c_sccsid[] = "@(#) %M% V%I% %E% %U% $Id: proplist.c,v 4.7 2025/06/06 18:47:49 dra Exp $";
#endif

#define A0 *attrs
#define A1 attrs[1]
#define A2 attrs[2]
#define A3 attrs[3]

#define ADONE ATTR_CONSUME(*attrs);break

typedef struct {
	Xv_opaque          public_self;
	proplist_verify_proc_t      verify_proc;
	proplist_copy_proc_t        copy_proc;
	proplist_free_proc_t        free_proc;
	proplist_sort_proc_t        sort_proc;
	Xv_font	           font;
	Panel_item         delete, apply_edits;
	Menu_item          up_item, down_item;
	int                entry_string_offset, item_data_size;
	int                glyph_offset;
	Frame_props         frame;
	Xv_opaque          slave_master;
	int                level_height;
} Proplist_private;

static int pl_key = 0;
#define XVPL_KEY XV_KEY_DATA,pl_key

#define PROPLISTPRIV(_x_) XV_PRIVATE(Proplist_private, Xv_proplist, _x_)
#define PROPLISTPUB(_x_) XV_PUBLIC(_x_)

static int proplist_verify_item(Property_list self, Xv_opaque item, Proplist_verify_op op, Xv_opaque unused)
{
	return XV_OK;
}

static void proplist_copy_item(Property_list self, Xv_opaque src, Xv_opaque target)
{
	Proplist_private *priv = PROPLISTPRIV(self);

	memcpy((char *)target, (char *)src, (size_t)priv->item_data_size);
}

static void proplist_free_item(Property_list self, Xv_opaque item)
{
	/* do nothing */
}

static void enter_list(Proplist_private *priv, Xv_opaque data, int pos, int is_insert)
{
	Attr_attribute attrs[100];
	int cnt = 0;
	char **label;
	Server_image *imaptr;

	if (is_insert) {
		attrs[cnt++] = (Attr_attribute)PANEL_LIST_INSERT;
		attrs[cnt++] = (Attr_attribute)pos;

/* 		fprintf(stderr, "proplist: insert %d\n", pos); */
		if (priv->font) {
			attrs[cnt++] = (Attr_attribute)PANEL_LIST_FONT;
			attrs[cnt++] = (Attr_attribute)pos;
			attrs[cnt++] = (Attr_attribute)priv->font;
		}
	}

	label = (char **)((char *)data + priv->entry_string_offset);
	attrs[cnt++] = (Attr_attribute)PANEL_LIST_STRING;
	attrs[cnt++] = (Attr_attribute)pos;
	attrs[cnt++] = (Attr_attribute)*label;

	attrs[cnt++] = (Attr_attribute)PANEL_LIST_CLIENT_DATA;
	attrs[cnt++] = (Attr_attribute)pos;
	attrs[cnt++] = (Attr_attribute)data;

	if (priv->glyph_offset >= 0) {
		(void)(*(priv->verify_proc))(PROPLISTPUB(priv), data,
									PROPLIST_CREATE_GLYPH, XV_NULL);
		imaptr = (Server_image *)((char *)data + priv->glyph_offset);

		attrs[cnt++] = (Attr_attribute)PANEL_LIST_GLYPH;
		attrs[cnt++] = (Attr_attribute)pos;
		attrs[cnt++] = (Attr_attribute)imaptr[PL_GLYPH_INDEX];

		attrs[cnt++] = (Attr_attribute)PANEL_LIST_MASK_GLYPH;
		attrs[cnt++] = (Attr_attribute)pos;
		attrs[cnt++] = (Attr_attribute)imaptr[PL_MASK_INDEX];
	}

	attrs[cnt++] = (Attr_attribute)PANEL_LIST_SELECT;
	attrs[cnt++] = (Attr_attribute)pos;
	attrs[cnt++] = (Attr_attribute)TRUE;

	attrs[cnt++] = (Attr_attribute)0;
	xv_set_avlist(PROPLISTPUB(priv), attrs);
	(void)(*(priv->verify_proc))(PROPLISTPUB(priv), 
							XV_NULL, PROPLIST_LIST_CHANGED, XV_NULL);
}

static void list_to_data(Proplist_private *priv, Proplist_contents *datptr)
{
	int num, i;
	char *ptr, *new;
	Xv_opaque cd;

	ptr = (char *)datptr->item_data;

	for (i = 0; i < datptr->num_items; i++) {
		(*(priv->free_proc))(PROPLISTPUB(priv), (Xv_opaque)ptr);
		ptr += priv->item_data_size;
	}
	if (datptr->item_data) xv_free((char *)datptr->item_data);

	num = (int)xv_get(PROPLISTPUB(priv), PANEL_LIST_NROWS);
	datptr->num_items = num;

	ptr = new = (char *)xv_calloc((unsigned)num + 1,
									(unsigned)priv->item_data_size);

	for (i = 0; i < num; i++) {
		cd = xv_get(PROPLISTPUB(priv), PANEL_LIST_CLIENT_DATA, i);
		(*(priv->copy_proc))(PROPLISTPUB(priv), cd, (Xv_opaque)ptr);
		ptr += priv->item_data_size;
	}
	datptr->item_data = (Xv_opaque)new;
}

static void sort_me(Proplist_private *priv)
{
	int visible_rows, curstart, curend, sel;
	Scrollbar sb;

	if (! priv->sort_proc) return;
	(*(priv->sort_proc))(PROPLISTPUB(priv));

	sel = (int)xv_get(PROPLISTPUB(priv), PANEL_LIST_FIRST_SELECTED);
	if (sel < 0) return;

	sb = xv_get(PROPLISTPUB(priv), PANEL_LIST_SCROLLBAR);

	curstart = (int)xv_get(sb, SCROLLBAR_VIEW_START);
	visible_rows = (int)xv_get(PROPLISTPUB(priv), PANEL_LIST_DISPLAY_ROWS);
	curend = curstart - 1 + visible_rows;

	if (sel >= curstart && sel <= curend) return; /* no need to scroll */

	if (sel > curend) {
		xv_set(sb, SCROLLBAR_VIEW_START, sel - visible_rows + 1, NULL);
	}
	else {
		xv_set(sb, SCROLLBAR_VIEW_START, sel, NULL);
	}
}

static void make_list_empty(Proplist_private *priv, int destroying)
{
	int num, i;

	num = (int)xv_get(PROPLISTPUB(priv), PANEL_LIST_NROWS);

	for (i = 0; i < num; i++) {
		Xv_opaque cd;

		cd = xv_get(PROPLISTPUB(priv), PANEL_LIST_CLIENT_DATA, i);

		if (! destroying) {
			(*(priv->verify_proc))(PROPLISTPUB(priv), cd,
							PROPLIST_DELETE_FROM_RESET, XV_NULL);
		}
		(*(priv->free_proc))(PROPLISTPUB(priv), cd);
		xv_free((char *)cd);
	}

	xv_set(PROPLISTPUB(priv), PANEL_LIST_DELETE_ROWS, 0, num, NULL);
}

static void data_to_list(Proplist_private *priv, Proplist_contents *datptr)
{
	int i;
	Xv_opaque new;
	char *ptr;

	make_list_empty(priv, FALSE);

	ptr = (char *)datptr->item_data;
	for (i = 0; i < datptr->num_items; i++) {
		new = (Xv_opaque)xv_calloc(1, (unsigned)priv->item_data_size);
		(*(priv->copy_proc))(PROPLISTPUB(priv), (Xv_opaque)ptr, new);
		if (XV_OK == (*(priv->verify_proc))(PROPLISTPUB(priv),new,
							PROPLIST_CONVERT, XV_NULL))
		{
			enter_list(priv, new, i, TRUE);
		}
		ptr += priv->item_data_size;
	}
	sort_me(priv);
}

static int note_list(Property_list self, char *string, Xv_opaque cldt, Panel_list_op op, Event *ev, int rownum)
{
	Proplist_private *priv = PROPLISTPRIV(self);

	switch (op) {
		case PANEL_LIST_OP_DESELECT:
			xv_set(priv->apply_edits, PANEL_INACTIVE, TRUE, NULL);
			xv_set(priv->delete, PANEL_INACTIVE, TRUE, NULL);
			if (priv->up_item) {
				xv_set(priv->up_item, MENU_INACTIVE, TRUE, NULL);
				xv_set(priv->down_item, MENU_INACTIVE, TRUE, NULL);
			}
			(*(priv->verify_proc))(PROPLISTPUB(priv), cldt,
								PROPLIST_DESELECT, XV_NULL);
			break;

		case PANEL_LIST_OP_SELECT:
			xv_set(priv->apply_edits, PANEL_INACTIVE, FALSE, NULL);
			xv_set(priv->delete, PANEL_INACTIVE, FALSE, NULL);
			if (priv->up_item) {
				int nn = (int)xv_get(self, PANEL_LIST_NROWS);
				xv_set(priv->up_item, MENU_INACTIVE, rownum <= 0, NULL);
				xv_set(priv->down_item, MENU_INACTIVE, rownum>=nn-1, NULL);
			}

			xv_set(priv->frame,
					FRAME_PROPS_TRIGGER_SLAVES,
						FRAME_PROPS_RESET, priv->slave_master, cldt,
					FRAME_PROPS_RESET_SLAVE_CBS, priv->slave_master,
					NULL);
			(*(priv->verify_proc))(PROPLISTPUB(priv), cldt,
								PROPLIST_USE_IN_PANEL, XV_NULL);
			break;

		default: return XV_ERROR;
	}

	return XV_OK;
}

static int insert_into_list(Proplist_private *priv, Menu menu, int sel)
{
	Xv_opaque new;

	new = (Xv_opaque)xv_calloc(1, (unsigned)priv->item_data_size);

	xv_set(priv->frame,
			FRAME_PROPS_TRIGGER_SLAVES,FRAME_PROPS_APPLY,priv->slave_master,new,
			NULL);
	if (menu) xv_set(menu, MENU_NOTIFY_STATUS, XV_ERROR, NULL);

	if (XV_OK != (*(priv->verify_proc))(PROPLISTPUB(priv),new,PROPLIST_INSERT,
										XV_NULL))
	{
		(*(priv->free_proc))(PROPLISTPUB(priv), new);
		xv_free(new);
		return FALSE;
	}

	enter_list(priv, new, sel, TRUE);
	xv_set(priv->frame,
			FRAME_PROPS_RESET_SLAVE_CBS, PROPLISTPUB(priv),
			FRAME_PROPS_ITEM_CHANGED, PROPLISTPUB(priv), TRUE,
			NULL);

	return TRUE;
}

static void note_insert_button(Panel_item item)
{
	Proplist_private *priv = (Proplist_private *)xv_get(item, XVPL_KEY);
	int last = (int)xv_get(PROPLISTPUB(priv), PANEL_LIST_NROWS);

	xv_set(item, PANEL_NOTIFY_STATUS, XV_ERROR, NULL);
	if (insert_into_list(priv, XV_NULL, last)) {
		sort_me(priv);
	}
}

static void note_insert_before(Menu menu)
{
	Proplist_private *priv = (Proplist_private *)xv_get(menu, XVPL_KEY);
	int sel = (int)xv_get(PROPLISTPUB(priv), PANEL_LIST_FIRST_SELECTED);

	if (sel < 0) sel = 0;

	insert_into_list(priv, menu, sel);
}

static void note_insert_after(Menu menu)
{
	Proplist_private *priv = (Proplist_private *)xv_get(menu, XVPL_KEY);
	int sel = (int)xv_get(PROPLISTPUB(priv), PANEL_LIST_FIRST_SELECTED);

	if (sel < 0) sel = (int)xv_get(PROPLISTPUB(priv), PANEL_LIST_NROWS);
	else ++sel;

	insert_into_list(priv, menu, sel);
}

static void note_insert_first(Menu menu)
{
	Proplist_private *priv = (Proplist_private *)xv_get(menu, XVPL_KEY);
	insert_into_list(priv, menu, 0);
}

static void note_insert_last(Menu menu)
{
	Proplist_private *priv = (Proplist_private *)xv_get(menu, XVPL_KEY);

	insert_into_list(priv, menu,
				(int)xv_get(PROPLISTPUB(priv), PANEL_LIST_NROWS));
}

typedef struct {
	Xv_opaque cldt;
	char *str;
	Server_image glyph, mask;
} item_attrs_t;

static void fill_attrs(Property_list self, int sel, item_attrs_t *a)
{
	a->cldt = xv_get(self, PANEL_LIST_CLIENT_DATA, sel);
	a->str = (char *)xv_get(self, PANEL_LIST_STRING, sel);
	a->glyph = xv_get(self, PANEL_LIST_GLYPH, sel);
	a->mask = xv_get(self, PANEL_LIST_MASK_GLYPH, sel);

	a->str = xv_strsave(a->str);
}

static void interchange(Property_list self, int i0, int i1)
{
	item_attrs_t attrs[2];

	fill_attrs(self, i0, attrs);
	fill_attrs(self, i1, attrs + 1);

	xv_set(self,
			PANEL_LIST_CLIENT_DATA, i1, attrs[0].cldt,
			PANEL_LIST_STRING, i1, attrs[0].str,
			PANEL_LIST_GLYPH, i1, attrs[0].glyph,
			PANEL_LIST_MASK_GLYPH, i1, attrs[0].mask,
			PANEL_LIST_CLIENT_DATA, i0, attrs[1].cldt,
			PANEL_LIST_STRING, i0, attrs[1].str,
			PANEL_LIST_GLYPH, i0, attrs[1].glyph,
			PANEL_LIST_MASK_GLYPH, i0, attrs[1].mask,
			NULL);

	xv_free(attrs[0].str);
	xv_free(attrs[1].str);
}

static void note_item_up(Menu menu, Menu_item item)
{
	int sel;
	Proplist_private *priv = (Proplist_private *)xv_get(item, XVPL_KEY);
	Property_list self = PROPLISTPUB(priv);

	sel = (int)xv_get(self, PANEL_LIST_FIRST_SELECTED);
	if (sel < 0) {
		xv_set(priv->frame, 
				FRAME_LEFT_FOOTER, XV_MSG("nothing selected"),
				WIN_ALARM,
				NULL);
		return;
	}
	if (sel == 0) return;

	interchange(self, sel, sel - 1);

	--sel;
	xv_set(self, PANEL_LIST_SELECT, sel, TRUE, NULL);
	xv_set(item, MENU_INACTIVE, sel == 0, NULL);
}

static void note_item_down(Menu menu, Menu_item item)
{
	int num, sel;
	Proplist_private *priv = (Proplist_private *)xv_get(item, XVPL_KEY);
	Property_list self = PROPLISTPUB(priv);

	sel = (int)xv_get(self, PANEL_LIST_FIRST_SELECTED);
	if (sel < 0) {
		xv_set(priv->frame, 
				FRAME_LEFT_FOOTER, XV_MSG("nothing selected"),
				WIN_ALARM,
				NULL);
		return;
	}

	num = (int)xv_get(self, PANEL_LIST_NROWS);

	if (sel >= num - 1) return;

	interchange(self, sel, sel + 1);

	++sel;
	xv_set(self, PANEL_LIST_SELECT, sel, TRUE, NULL);
	xv_set(item, MENU_INACTIVE, sel >= num-1, NULL);
}

static void note_delete(Panel_item item)
{
	Proplist_private *priv = (Proplist_private *)xv_get(item, XVPL_KEY);
	Property_list self = PROPLISTPUB(priv);
	int sel = (int)xv_get(self, PANEL_LIST_FIRST_SELECTED);

	xv_set(item, PANEL_NOTIFY_STATUS, XV_ERROR, NULL);

	if (sel < 0) {
		xv_set(priv->frame, 
				FRAME_LEFT_FOOTER, XV_MSG("nothing selected"),
				WIN_ALARM,
				NULL);
	}
	else {
		Xv_opaque cd;

		cd = (Xv_opaque)xv_get(self, PANEL_LIST_CLIENT_DATA, sel);
		if (cd) {
			if (XV_OK != (*(priv->verify_proc))(PROPLISTPUB(priv), cd,
									PROPLIST_DELETE_QUERY, XV_NULL))
			{
				return;
			}

			xv_set(self, PANEL_LIST_DELETE, sel, NULL);
			xv_set(priv->frame, FRAME_PROPS_ITEM_CHANGED, self, TRUE, NULL);
			sel = (int)xv_get(self, PANEL_LIST_FIRST_SELECTED);

			(void)(*(priv->verify_proc))(self, cd, PROPLIST_DELETE,
														XV_NULL);
			(*(priv->free_proc))(self, cd);
			xv_free((char *)cd);
			(void)(*(priv->verify_proc))(self, 
							XV_NULL, PROPLIST_LIST_CHANGED, XV_NULL);

			sel = (int)xv_get(self, PANEL_LIST_FIRST_SELECTED);
		}
	}

	xv_set(priv->apply_edits, PANEL_INACTIVE, sel < 0, NULL);
	xv_set(priv->delete, PANEL_INACTIVE, sel < 0, NULL);
	if (priv->up_item) {
		int nn = (int)xv_get(self, PANEL_LIST_NROWS);
		xv_set(priv->up_item, MENU_INACTIVE, sel <= 0, NULL);
		xv_set(priv->down_item, MENU_INACTIVE, sel>=nn-1, NULL);
	}
}

static void note_change(Panel_item item)
{
	Proplist_private *priv = (Proplist_private *)xv_get(item, XVPL_KEY);
	Property_list self = PROPLISTPUB(priv);
	int sel = (int)xv_get(self, PANEL_LIST_FIRST_SELECTED);
	Xv_opaque cldt, new;

	xv_set(item, PANEL_NOTIFY_STATUS, XV_ERROR, NULL);

	if (sel < 0) {
		xv_set(self,
					FRAME_LEFT_FOOTER, XV_MSG("nothing selected!"),
					WIN_ALARM,
					NULL);
		xv_set(item, PANEL_INACTIVE, TRUE, NULL);
		return;
	}

	new = (Xv_opaque)xv_calloc(1, (unsigned)priv->item_data_size);

	xv_set(priv->frame,
			FRAME_PROPS_TRIGGER_SLAVES,FRAME_PROPS_APPLY,priv->slave_master,new,
			NULL);

	cldt = xv_get(self, PANEL_LIST_CLIENT_DATA, sel);

	if (XV_OK != (*(priv->verify_proc))(self, new, PROPLIST_APPLY,
											cldt))
	{
		(*(priv->free_proc))(self, new);
		xv_free(new);
		return;
	}

	enter_list(priv, new, sel, FALSE);

	(void)(*(priv->verify_proc))(self, cldt, PROPLIST_DELETE_OLD_CHANGED,
														XV_NULL);
	(*(priv->free_proc))(self, cldt);
	xv_free((char *)cldt);

	sort_me(priv);

	xv_set(priv->frame,
			FRAME_PROPS_RESET_SLAVE_CBS, self,
			FRAME_PROPS_ITEM_CHANGED, self, TRUE,
			NULL);
}

static char *make_help(Property_list self, char *str)
{
	char *myhelp, *itemhelp;

	myhelp = (char *)xv_get(self, XV_HELP_DATA);
	if (! myhelp) return (char *)0;

	itemhelp = xv_malloc(strlen(myhelp) + strlen(str) + 5);
	strcpy(itemhelp, myhelp);
	strcat(itemhelp, "_");
	strcat(itemhelp, str);
	return itemhelp;
}

static void free_help_data(Xv_opaque obj, int key, char *data)
{
	if (key == XV_HELP) xv_free(data);
}

static void create_buttons(Proplist_private *priv)
{
	static int instcnt = 0;
	Property_list self = PROPLISTPUB(priv);
	Panel pan;
	char tmpinst[30], menuinst[200];
	char *inst;

	pan = xv_get(self, XV_OWNER);

	inst = (char *)xv_get(self, XV_INSTANCE_NAME);
	if (! inst) inst = (char *)xv_get(priv->frame, XV_INSTANCE_NAME);
	if (! inst) {
		sprintf(tmpinst, "proplist%d", ++instcnt);
		inst = tmpinst;
	}

	if (priv->sort_proc) {
		xv_create(pan, PANEL_BUTTON,
				PANEL_NEXT_ROW, -1,
				PANEL_ITEM_LAYOUT_ROLE, PANEL_ROLE_CENTER,
				PANEL_LABEL_STRING, XV_MSG("Insert"),
				PANEL_NOTIFY_PROC, note_insert_button,
				XVPL_KEY, priv,
				XV_HELP_DATA, make_help(self, "insertbasebutt"),
				XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
				NULL);
		priv->up_item = XV_NULL;
		priv->down_item = XV_NULL;
	}
	else {
		Menu insmenu, lmenu;

		sprintf(menuinst, "%s_insert_menu", inst);

		insmenu = xv_create(XV_SERVER_FROM_WINDOW(pan), MENU,
				XV_INSTANCE_NAME, menuinst,
				XVPL_KEY, priv,
				MENU_ITEM,
					MENU_STRING, XV_MSG("Before"),
					MENU_NOTIFY_PROC, note_insert_before,
					XV_HELP_DATA, make_help(self, "insertbefore"),
					XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
					NULL,
				MENU_ITEM,
					MENU_STRING, XV_MSG("After"),
					MENU_NOTIFY_PROC, note_insert_after,
					XV_HELP_DATA, make_help(self, "insertafter"),
					XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
					NULL,
				MENU_ITEM,
					MENU_STRING, XV_MSG("First"),
					MENU_NOTIFY_PROC, note_insert_first,
					XV_HELP_DATA, make_help(self, "insertfirst"),
					XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
					NULL,
				MENU_ITEM,
					MENU_STRING, XV_MSG("Last"),
					MENU_NOTIFY_PROC, note_insert_last,
					XV_HELP_DATA, make_help(self, "insertlast"),
					XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
					NULL,
				XV_SET_MENU, priv->frame,
				NULL);

		xv_create(pan, PANEL_BUTTON,
				PANEL_NEXT_ROW, -1,
				PANEL_ITEM_LAYOUT_ROLE, PANEL_ROLE_CENTER,
				PANEL_LABEL_STRING, XV_MSG("Insert"),
				PANEL_ITEM_MENU, insmenu,
				XV_HELP_DATA, make_help(self, "insertbutt"),
				XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
				NULL);

		lmenu = xv_get(self, PANEL_ITEM_MENU);
		priv->up_item = xv_create(XV_NULL, MENUITEM,
				MENU_RELEASE,
				MENU_STRING, XV_MSG("Move up"),
				MENU_NOTIFY_PROC, note_item_up,
				XVPL_KEY, priv,
				XV_HELP_DATA, make_help(self, "item_up"),
				XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
				NULL);
		priv->down_item = xv_create(XV_NULL, MENUITEM,
				MENU_RELEASE,
				MENU_STRING, XV_MSG("Move down"),
				MENU_NOTIFY_PROC, note_item_down,
				XVPL_KEY, priv,
				XV_HELP_DATA, make_help(self, "item_down"),
				XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
				NULL);
		xv_set(lmenu, 
				MENU_APPEND_ITEM, priv->up_item,
				MENU_APPEND_ITEM, priv->down_item,
				NULL);
	}

	priv->delete = xv_create(pan, PANEL_BUTTON,
			PANEL_ITEM_LAYOUT_ROLE, PANEL_ROLE_CENTER,
			PANEL_LABEL_STRING, XV_MSG("Delete"),
			PANEL_NOTIFY_PROC, note_delete,
			PANEL_INACTIVE, TRUE,
			XV_HELP_DATA, make_help(self, "deletebutt"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
			XVPL_KEY, priv,
			NULL);

	priv->apply_edits = xv_create(pan, PANEL_BUTTON,
			PANEL_ITEM_LAYOUT_ROLE, PANEL_ROLE_CENTER,
			PANEL_LABEL_STRING, XV_MSG("Change"),
			PANEL_NOTIFY_PROC, note_change,
			PANEL_INACTIVE, TRUE,
			XVPL_KEY, priv,
			XV_HELP_DATA, make_help(self, "change_butt"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
			NULL);
}

static int proplist_init(Panel owner, Xv_opaque slf, Attr_avlist avlist, int *u)
{
	Xv_proplist *self = (Xv_proplist *)slf;
	Proplist_private *priv = (Proplist_private *)xv_alloc(Proplist_private);
	Attr_attribute *attrs;

	if (!priv) return XV_ERROR;

	priv->public_self = (Xv_opaque)self;
	self->private_data = (Xv_opaque)priv;

	if (! pl_key) pl_key = xv_unique_key();

	priv->frame = xv_get(owner, WIN_FRAME);
	priv->level_height = 0;

	for (attrs=avlist; *attrs; attrs=attr_next(attrs)) switch ((int)*attrs) {
		case PANEL_LEVEL_DOT_HEIGHT:
			priv->level_height = (int)A1;
			ADONE;
		case PROPLIST_ITEM_DATA_SIZE:
			priv->item_data_size = (unsigned)A1;
			ADONE;
	}

	if (! priv->item_data_size) {
		xv_error((Xv_opaque)self,
				ERROR_PKG, PROP_LIST,
				ERROR_LAYER, ERROR_PROGRAM,
				ERROR_STRING, "Have no PROPLIST_ITEM_DATA_SIZE",
				ERROR_SEVERITY, ERROR_NON_RECOVERABLE,
				NULL);
	}

	priv->glyph_offset = -1;
	priv->verify_proc = proplist_verify_item;
	priv->copy_proc = proplist_copy_item;
	priv->free_proc = proplist_free_item;

	xv_set((Xv_opaque)self,
				PANEL_READ_ONLY, TRUE,
				PANEL_CHOOSE_ONE, TRUE,
				PANEL_NOTIFY_PROC, note_list,
				NULL);

	return XV_OK;
}

static Xv_opaque proplist_set(Property_list self, Attr_avlist avlist)
{
	Attr_attribute *attrs;
	Proplist_private *priv = PROPLISTPRIV(self);
	int sel;

	for (attrs=avlist; *attrs; attrs=attr_next(attrs)) switch ((int)*attrs) {
		case PROPLIST_VERIFY_PROC:
			priv->verify_proc = (proplist_verify_proc_t)A1;
			if (!priv->verify_proc) priv->verify_proc = proplist_verify_item;
			ADONE;
		case PROPLIST_COPY_PROC:
			priv->copy_proc = (proplist_copy_proc_t)A1;
			if (!priv->copy_proc) priv->copy_proc = proplist_copy_item;
			ADONE;
		case PROPLIST_FREE_ITEM_PROC:
			priv->free_proc = (proplist_free_proc_t)A1;
			if (!priv->free_proc) priv->free_proc = proplist_free_item;
			ADONE;
		case PROPLIST_FONT:
			priv->font = (Xv_font)A1;
/* 			fprintf(stderr, "proplist_set PROPLIST_FONT %lx\n", priv->font); */
			ADONE;
		case PROPLIST_SORT_PROC:
			priv->sort_proc = (proplist_sort_proc_t)A1;
			ADONE;
		case PROPLIST_GLYPH_OFFSET:
			priv->glyph_offset = (int)A1;
			ADONE;
		case PROPLIST_ENTRY_STRING_OFFSET:
			priv->entry_string_offset = (int)A1;
			ADONE;
		case PROPLIST_CREATE_BUTTONS:
			create_buttons(priv);
			ADONE;

		case PANEL_LIST_SELECT:
			xv_super_set_avlist(self, PROP_LIST, avlist);

			if (A2) {
				/* a select operation */
				sel = (int)xv_get(self, PANEL_LIST_FIRST_SELECTED);
				if (sel >= 0) {
					Xv_opaque cldt = xv_get(self, PANEL_LIST_CLIENT_DATA, sel);

					xv_set(priv->apply_edits, PANEL_INACTIVE, FALSE, NULL);
					xv_set(priv->delete, PANEL_INACTIVE, FALSE, NULL);
					if (priv->up_item) {
						int nn = (int)xv_get(self, PANEL_LIST_NROWS);
						xv_set(priv->up_item, MENU_INACTIVE, sel <= 0, NULL);
						xv_set(priv->down_item, MENU_INACTIVE, sel>=nn-1, NULL);
					}
					xv_set(priv->frame,
							FRAME_PROPS_TRIGGER_SLAVES,
								FRAME_PROPS_RESET, priv->slave_master, cldt,
							FRAME_PROPS_RESET_SLAVE_CBS, priv->slave_master,
							NULL);

					(void)(*(priv->verify_proc))(self, cldt, PROPLIST_USE_IN_PANEL,
								XV_NULL);
				}
			}
			else {
				/* a deselect operation */
				xv_set(priv->apply_edits, PANEL_INACTIVE, TRUE, NULL);
				xv_set(priv->delete, PANEL_INACTIVE, TRUE, NULL);
				if (priv->up_item) {
					xv_set(priv->up_item, MENU_INACTIVE, TRUE, NULL);
					xv_set(priv->down_item, MENU_INACTIVE, TRUE, NULL);
				}
			}
			return XV_SET_DONE;

		case XV_END_CREATE:
			priv->slave_master = xv_get(self, PANEL_ITEM_OWNER);
			if (! priv->slave_master) priv->slave_master = self;
			break;

		default: xv_check_bad_attr(PROP_LIST, A0);
			break;
	}

	return XV_OK;
}

/*ARGSUSED*/
static Xv_opaque proplist_get(Property_list self, int *status, Attr_attribute attr, va_list vali)
{
	Proplist_private *priv = PROPLISTPRIV(self);

	*status = XV_OK;
	switch ((int)attr) {
		case PROPLIST_VERIFY_PROC: return (Xv_opaque)priv->verify_proc;
		case PROPLIST_COPY_PROC: return (Xv_opaque)priv->copy_proc;
		case PROPLIST_FREE_ITEM_PROC: return (Xv_opaque)priv->free_proc;
		case PROPLIST_FONT: return (Xv_opaque)priv->font;
		case PROPLIST_SORT_PROC: return (Xv_opaque)priv->sort_proc;
		case PROPLIST_GLYPH_OFFSET:
			return (Xv_opaque)priv->glyph_offset;
		case PROPLIST_ENTRY_STRING_OFFSET:
			return (Xv_opaque)priv->entry_string_offset;
		case PANEL_LEVEL_DOT_HEIGHT:
			if (priv->level_height) {
				return (Xv_opaque)priv->level_height;
			}
			/* fall through */
		default:
			*status = xv_check_bad_attr(PROP_LIST, attr);
			return (Xv_opaque)XV_OK;
	}
}

static int proplist_destroy(Property_list self, Destroy_status status)
{
	if (status == DESTROY_CLEANUP) {
		Proplist_private *priv = PROPLISTPRIV(self);

		make_list_empty(priv, TRUE);
		memset((char *)priv, 0, sizeof(Proplist_private));
		xv_free(priv);
	}
	return XV_OK;
}

void xv_proplist_converter(int unused1, int panel_to_data,
			Proplist_contents *data, Property_list self, Xv_opaque unused2)
{
	if (panel_to_data) list_to_data(PROPLISTPRIV(self), data);
	else data_to_list(PROPLISTPRIV(self), data);
}

const Xv_pkg xv_proplist_pkg = {
	"PropertyList",
	ATTR_PKG_LISTPROP,
	sizeof(Xv_proplist),
	PANEL_LIST,
	proplist_init,
	proplist_set,
	proplist_get,
	proplist_destroy,
	0
};
