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
/* NOTE: this class cannot be a subclass of PROPLIST
 * because of the late creation (FRAME_PROPS_CREATE_CONTENTS_PROC)
 * of Property Lists (but the resources must be available at appl startup).
 */
#include <string.h>
#include <xview/permlist.h>

char permlist_c_sccsid[] = "@(#) %M% V%I% %E% %U% $Id: permlist.c,v 4.12 2025/07/13 07:18:19 dra Exp $";

/* this attribute is so private that I don't even want to have it in
 * permlist.h
 */
#define PERMLIST_PREPARE_DESTROY PERML_ATTR(ATTR_NO_VALUE, 112)

typedef int (*permprop_cb_t)(Perm_prop_frame, int);
typedef void (*free_func_t)(Permanent_list, Xv_opaque);
typedef void (*copy_func_t)(Permanent_list, Xv_opaque, Xv_opaque);
typedef int (*verify_func_t)(Permanent_list, Xv_opaque data,
									Proplist_verify_op op, Xv_opaque orig);

typedef struct _pl_private {
	Xv_opaque           public_self;
	Perm_prop_frame     frame;
	Property_list       list;
	int                 label_off, itemsize, dataoff;
	Permprop_res_category_t res_cat;
	free_func_t         free_item_func;
	copy_func_t         copy_item_func;
	verify_func_t       verify_item_func;
	permprop_cb_t       appl_setdef, appl_resetfac;
	char                *item_resource_name;
	Permprop_res_res_t  *fdres;
	int                 num_fdres;
	char                factory_reset_performed;
	struct _pl_private  *next_in_same_frame;
} Permlist_private;

#define A0 *attrs
#define A1 attrs[1]
#define A2 attrs[2]

#define ADONE ATTR_CONSUME(*attrs);break

#define PLPRIV(_x_) XV_PRIVATE(Permlist_private, Xv_permlist, _x_)
#define PLPUB(_x_) XV_PUBLIC(_x_)

static int permlist_key = 0;

static void internal_free_func(Permanent_list self, Xv_opaque fd)
{
	int i;
	Permlist_private *priv = PLPRIV(self);
	char **straddr, *addr = (char *)fd;

	for (i = 0; i < priv->num_fdres; i++) {
		Permprop_res_res_t  *p = priv->fdres + i;

		switch (p->type) {
			case DAP_int: break;
			case DAP_bool: break;
			case DAP_enum: break;
			case DAP_string:
				straddr = (char **)(addr + p->offset);
				if (*straddr) xv_free(*straddr);
				*straddr = NULL;
				break;
			case DAP_stringlist:
				fprintf(stderr, "DAP_stringlist not yet\n");
				break;
		}
	}
	memset(addr, 0, (size_t)priv->itemsize);
}

static void internal_copy_func(Permanent_list self, Xv_opaque src, Xv_opaque tgt)
{
	int i;
	Permlist_private *priv = PLPRIV(self);
	char **straddr, *taddr = (char *)tgt, *saddr = (char *)src;

	for (i = 0; i < priv->num_fdres; i++) {
		Permprop_res_res_t  *p = priv->fdres + i;

		switch (p->type) {
			case DAP_int: break;
			case DAP_bool: break;
			case DAP_enum: break;
			case DAP_string:
				straddr = (char **)(taddr + p->offset);
				if (*straddr) xv_free(*straddr);
				*straddr = NULL;
				break;
			case DAP_stringlist:
				fprintf(stderr, "DAP_stringlist not yet\n");
				break;
		}
	}

	memcpy(taddr, saddr, (size_t)priv->itemsize);

	for (i = 0; i < priv->num_fdres; i++) {
		Permprop_res_res_t  *p = priv->fdres + i;

		switch (p->type) {
			case DAP_int: break;
			case DAP_bool: break;
			case DAP_enum: break;
			case DAP_string:
				straddr = (char **)(taddr + p->offset);
				if (*straddr) *straddr = xv_strsave(*straddr);
				break;
			case DAP_stringlist:
				fprintf(stderr, "DAP_stringlist not yet\n");
				break;
		}
	}
}

static void free_item(Property_list l, Xv_opaque fd)
{
	Permlist_private *priv = (Permlist_private *)xv_get(l, XV_KEY_DATA,
															permlist_key);

	if (priv->free_item_func)
		(*(priv->free_item_func))(PLPUB(priv), fd);
}

static void copy_item(Property_list l, Xv_opaque src, Xv_opaque target)
{
	Permlist_private *priv = (Permlist_private *)xv_get(l, XV_KEY_DATA,
															permlist_key);

	if (priv->copy_item_func)
		(*(priv->copy_item_func))(PLPUB(priv), src, target);
}

static int verify_item(Property_list l, Xv_opaque data, Proplist_verify_op op, Xv_opaque orig)
{
	Permlist_private *priv = (Permlist_private *)xv_get(l, XV_KEY_DATA,
															permlist_key);

	if (priv->verify_item_func)
		return (*(priv->verify_item_func))(PLPUB(priv), data, op, orig);

	return XV_OK;
}

static int set_default(Permlist_private *priv, Perm_prop_frame fram, int is_triggered)
{
	Proplist_contents *lcont;
	int i;
	char resnam[200];
	Xv_server srv = XV_SERVER_FROM_WINDOW(fram);
	Xv_opaque *db = (Xv_opaque *)xv_get(srv, SERVER_RESOURCE_DATABASES);
	char *add, *descr, *ppp, prefix[100];

	if (priv->appl_setdef) {
		int val = (*(priv->appl_setdef))(fram, is_triggered);

		if (val != XV_OK) return val;
	}

	add = (char *)xv_get(fram, FRAME_PROPS_DEFAULT_DATA_ADDRESS);
	if (! add) {
		xv_error(PLPUB(priv),
					ERROR_PKG, xv_get(PLPUB(priv), XV_TYPE),
					ERROR_SEVERITY, ERROR_RECOVERABLE,
					ERROR_LAYER, ERROR_PROGRAM,
					ERROR_STRING,"owner frame has no FRAME_PROPS_DEFAULT_DATA_ADDRESS",
					NULL);
		return XV_ERROR;
	}

	lcont = (Proplist_contents *)(add + priv->dataoff);
	descr = (char *)lcont->item_data;

	ppp = (char *)xv_get(fram, PERM_RESOURCE_PREFIX);
	if (ppp) strcpy(prefix, ppp);
	else sprintf(prefix, "%s.", server_get_instance_appname());

	for (i = 0; i < lcont->num_items; i++) {
		sprintf(resnam, "%s%s%03d.", prefix, priv->item_resource_name, i);
		Permprop_res_update_dbs(db, resnam, descr + i * priv->itemsize,
									priv->fdres, priv->num_fdres);
	}

	return XV_OK;
}

static int note_set_default(Perm_prop_frame fram, int is_triggered)
{
	Permlist_private *priv =
				(Permlist_private *)xv_get(fram, XV_KEY_DATA, permlist_key);

	while (priv) {
		int val = set_default(priv, fram, is_triggered);

		if (val != XV_OK) return val;
		priv = priv->next_in_same_frame;
	}
	return XV_OK;
}

static int reset_factory(Permlist_private *priv, Perm_prop_frame fram, int is_triggered)
{
	char *add;
	Proplist_contents *clcont, *dlcont;
	int i;
	char resnam[200];
	char *cdescr, *ddescr;
	Xv_server srv = XV_SERVER_FROM_WINDOW(fram);
	Xv_opaque *db = (Xv_opaque *)xv_get(srv, SERVER_RESOURCE_DATABASES);
	char *ppp, prefix[100];

	ppp = (char *)xv_get(fram, PERM_RESOURCE_PREFIX);
	if (ppp) strcpy(prefix, ppp);
	else sprintf(prefix, "%s.", server_get_instance_appname());

	add = (char *)xv_get(fram, FRAME_PROPS_DATA_ADDRESS);
	clcont = (Proplist_contents *)(add + priv->dataoff);

	add = (char *)xv_get(fram, FRAME_PROPS_FACTORY_DATA_ADDRESS);
	if (! add) {
		xv_error(PLPUB(priv),
					ERROR_PKG, xv_get(PLPUB(priv), XV_TYPE),
					ERROR_SEVERITY, ERROR_RECOVERABLE,
					ERROR_LAYER, ERROR_PROGRAM,
					ERROR_STRING,"owner frame has no FRAME_PROPS_FACTORY_DATA_ADDRESS",
					NULL);
		return XV_ERROR;
	}
	dlcont = (Proplist_contents *)(add + priv->dataoff);

	xv_set(PLPUB(priv), PERMLIST_BEFORE_RESET_FACTORY, dlcont, NULL);
	priv->factory_reset_performed = TRUE;

	clcont->item_data = (Xv_opaque)xv_calloc((unsigned)priv->itemsize,
								1 + (unsigned)dlcont->num_items);
	dlcont->item_data = (Xv_opaque)xv_calloc((unsigned)priv->itemsize,
								1 + (unsigned)dlcont->num_items);
	cdescr = (char *)clcont->item_data;
	ddescr = (char *)dlcont->item_data;

	for (i = 0; i < dlcont->num_items; i++) {
		/* old resources, supported for backwards compatibility */
		sprintf(resnam, "%s%s%d.", prefix, priv->item_resource_name, i);
		Permprop_res_read_dbs(db, resnam, ddescr + i * priv->itemsize,
									priv->fdres, priv->num_fdres);
		Permprop_res_read_dbs(db, resnam, cdescr + i * priv->itemsize,
									priv->fdres, priv->num_fdres);

		/* new resources */
		sprintf(resnam, "%s%s%03d.", prefix, priv->item_resource_name, i);
		Permprop_res_read_dbs(db, resnam, ddescr + i * priv->itemsize,
									priv->fdres, priv->num_fdres);
		Permprop_res_read_dbs(db, resnam, cdescr + i * priv->itemsize,
									priv->fdres, priv->num_fdres);
	}

	if (priv->appl_resetfac) 
		return (*(priv->appl_resetfac))(fram, is_triggered);

	return XV_OK;
}

static int note_reset_factory(Perm_prop_frame fram, int is_triggered)
{
	Permlist_private *priv =
				(Permlist_private *)xv_get(fram, XV_KEY_DATA, permlist_key);

	while (priv) {
		int val = reset_factory(priv, fram, is_triggered);

		if (val != XV_OK) return val;
		priv = priv->next_in_same_frame;
	}
	return XV_OK;
}

static void my_converter(int u1, int panel_to_data, Proplist_contents *data,
					Property_list plist, Permanent_list self)
{
	if (! panel_to_data) {
		xv_set(self, PERMLIST_BEFORE_RESET, data, NULL);
	}
	xv_proplist_converter(u1, panel_to_data, data, plist, self);
}

static char *make_help(Permlist_private *priv, char *str)
{
	char *myhelp, *itemhelp;

	myhelp = (char *)xv_get(PLPUB(priv), XV_HELP_DATA);
	if (! myhelp) return (char *)0;

	itemhelp = xv_malloc(strlen(myhelp) + strlen(str) + 2);
	sprintf(itemhelp, "%s_%s", myhelp, str);
	return itemhelp;
}

static void free_help_data(Xv_opaque obj, int key, char *data)
{
	if (key == XV_HELP && data) xv_free(data);
}

static void create_top(Permlist_private *priv, Attr_avlist avlist)
{
	Attr_attribute *attrs, av[300];
	int vert_off_index;
	int ac = 0;
#define ADD_ATTR(_val_) av[ac++] = (Attr_attribute)(_val_)

	ADD_ATTR(FRAME_PROPS_CREATE_ITEM);
	ADD_ATTR(FRAME_PROPS_ITEM_SPEC);
	ADD_ATTR(&priv->list);
	vert_off_index = ac;
	ADD_ATTR(-1);
	ADD_ATTR(PROP_LIST);

	for (attrs=avlist; *attrs; attrs=attr_next(attrs)) switch ((int)*attrs) {
		case PANEL_NEXT_ROW:
			av[vert_off_index] = A1;
			break;
		case PANEL_LIST_ROW_HEIGHT:
		case PANEL_LABEL_STRING:
		case PANEL_LIST_DISPLAY_ROWS:
		case PANEL_LIST_WIDTH:
		case PROPLIST_GLYPH_OFFSET:
		case PROPLIST_SORT_PROC:
		case FRAME_SHOW_RESIZE_CORNER:
			ADD_ATTR(A0);
			ADD_ATTR(A1);
			break;
	}

	ADD_ATTR(PANEL_CHOOSE_NONE);
	ADD_ATTR(TRUE);
	ADD_ATTR(PROPLIST_VERIFY_PROC);
	ADD_ATTR(verify_item);
	ADD_ATTR(PROPLIST_FREE_ITEM_PROC);
	ADD_ATTR(free_item);
	ADD_ATTR(PROPLIST_COPY_PROC);
	ADD_ATTR(copy_item);
	ADD_ATTR(PROPLIST_ITEM_DATA_SIZE);
	ADD_ATTR(priv->itemsize);
	ADD_ATTR(PROPLIST_ENTRY_STRING_OFFSET);
	ADD_ATTR(priv->label_off);
	ADD_ATTR(XV_KEY_DATA);
	ADD_ATTR(permlist_key);
	ADD_ATTR(priv);
	ADD_ATTR(FRAME_SHOW_RESIZE_CORNER);
	ADD_ATTR(TRUE);
	ADD_ATTR(FRAME_PROPS_DATA_OFFSET);
	ADD_ATTR(priv->dataoff);
	ADD_ATTR(FRAME_PROPS_CONVERTER);
	ADD_ATTR(my_converter);
	ADD_ATTR(PLPUB(priv));
	ADD_ATTR(XV_KEY_DATA);
	ADD_ATTR(XV_HELP);
	ADD_ATTR(make_help(priv, "list"));
	ADD_ATTR(XV_KEY_DATA_REMOVE_PROC);
	ADD_ATTR(XV_HELP);
	ADD_ATTR(free_help_data);
	ADD_ATTR(0);
	ADD_ATTR(0);
	ADD_ATTR(0);

	xv_set_avlist(priv->frame, av);
}

static void create_bottom(Permlist_private *priv)
{
	xv_set(priv->frame, FRAME_PROPS_ALIGN_ITEMS, NULL);
	xv_set(priv->list, PROPLIST_CREATE_BUTTONS, NULL);
}

static void new_resource(Permlist_private *priv, char *nam, Permprop_res_type_t typ, int off, Ppmt def)
{
	Permprop_res_res_t *newres;

	if (priv->factory_reset_performed) {
		/* reason for this: in an application's (calman) property window,
		 * PERM_RESET_FROM_DB,
		 * FRAME_PROPS_TRIGGER, FRAME_PROPS_RESET_FACTORY,
		 * was performed quite soon after the creation of a subclass
		 * (FUNCTION_KEYS), but later, an additional resource was added.
		 * This did not work.....
		 */
		xv_error(PLPUB(priv),
				ERROR_PKG, PERMANENT_LIST,
				ERROR_LAYER, ERROR_PROGRAM,
				ERROR_STRING, "PERMLIST_ADD_RESOURCE after FRAME_PROPS_RESET_FACTORY: too late",
				ERROR_SEVERITY, ERROR_RECOVERABLE,
				NULL);
	}

	++priv->num_fdres;
	newres = xv_alloc_n(Permprop_res_res_t, (size_t)priv->num_fdres);
	if (priv->num_fdres > 1) {
		memcpy(newres, priv->fdres,
					(priv->num_fdres-1)*sizeof(Permprop_res_res_t));
		xv_free(priv->fdres);
	}
	priv->fdres = newres;
	newres[priv->num_fdres - 1].res_name = xv_strsave(nam);
	newres[priv->num_fdres - 1].category = priv->res_cat;
	newres[priv->num_fdres - 1].type = typ;
	newres[priv->num_fdres - 1].offset = off;
	newres[priv->num_fdres - 1].misc = def;
}

static Notify_error frame_destruction(Perm_prop_frame fram, Destroy_status st)
{
	if (st == DESTROY_CLEANUP) {
		Permlist_private *next, *priv =
				(Permlist_private *)xv_get(fram, XV_KEY_DATA, permlist_key);

		while (priv) {
			Permanent_list self = PLPUB(priv);

			next = priv->next_in_same_frame;
			xv_set(self, PERMLIST_PREPARE_DESTROY, NULL);
			xv_destroy(self);
			priv = next;
		}
	}
	return notify_next_destroy_func(fram, st);
}

static void destroy_data(Permlist_private *priv, char *addr)
{
	Proplist_contents *lcont;
	char *descr;

	if (! addr) return;

	lcont = (Proplist_contents *)(addr + priv->dataoff);
	descr = (char *)lcont->item_data;
	if (descr) {
		int i;

		for (i = 0; i < lcont->num_items; i++) {
			char *singleblock = descr + i * priv->itemsize;

			priv->free_item_func(PLPUB(priv), (Xv_opaque)singleblock);
			memset(singleblock, 0, (size_t)priv->itemsize);
		}
	}
}

static void prepare_destroy(Permlist_private *priv)
{
	if (priv->list) {
		Proplist_contents contents;

		contents.num_items = 0;
		contents.item_data = XV_NULL;

		/* this is a sort of hack to get the list emptied */
		xv_proplist_converter(0, FALSE, &contents, priv->list, XV_NULL);

		/* we don't want any connection with property list callbacks */
		xv_set(priv->list,
				PROPLIST_VERIFY_PROC, NULL,
				PROPLIST_FREE_ITEM_PROC, NULL,
				PROPLIST_COPY_PROC, NULL,
				NULL);
	}

	if (priv->frame && priv->free_item_func) {
		destroy_data(priv, (char *)xv_get(priv->frame,
									FRAME_PROPS_DATA_ADDRESS));
		destroy_data(priv, (char *)xv_get(priv->frame,
									FRAME_PROPS_FACTORY_DATA_ADDRESS));
	}
}

static void set_frame_callbacks(Permlist_private *priv)
{
	if (! priv->next_in_same_frame) {
		/* these are the application's callbacks
		 * ONLY if I'm alone
		 */
		/* AND IF I HAVE AN OWNER !!!!! */
		if (priv->frame) {
			priv->appl_setdef =
					(permprop_cb_t)xv_get(priv->frame,
										FRAME_PROPS_SET_DEFAULTS_PROC);
			priv->appl_resetfac =
					(permprop_cb_t)xv_get(priv->frame,
										FRAME_PROPS_RESET_FACTORY_PROC);
			xv_set(priv->frame,
				FRAME_PROPS_SET_DEFAULTS_PROC, note_set_default,
				FRAME_PROPS_RESET_FACTORY_PROC, note_reset_factory,
				NULL);
		}
	}
}

static int permlist_init(Perm_prop_frame owner, Permanent_list slf, Attr_avlist avlist, int *u)
{
	Xv_permlist *self = (Xv_permlist *)slf;
	Permlist_private *priv, *other;

	if (! permlist_key) permlist_key = xv_unique_key();

	if (owner) {
		if (! xv_get(owner, XV_IS_SUBTYPE_OF, PERMANENT_PROPS)) {
			xv_error((Xv_opaque)self,
					ERROR_PKG, PERMANENT_LIST,
					ERROR_LAYER, ERROR_PROGRAM,
					ERROR_STRING, "owner must be a PERMANENT_PROPS",
					ERROR_SEVERITY, ERROR_NON_RECOVERABLE,
					NULL);
			return XV_ERROR;
		}
	}

	priv = xv_alloc(Permlist_private);
	if (!priv) return XV_ERROR;

	priv->public_self = (Xv_opaque)self;
	self->private_data = (Xv_opaque)priv;
	priv->frame = owner; /* might be XV_NULL here */

	if (owner) {
		other = (Permlist_private *)xv_get(owner, XV_KEY_DATA, permlist_key);
		if (other) {
			priv->next_in_same_frame = other;
		}
		else {
			/* I'm the first (or maybe ONLY) permlist in this frame.
			 * I need to be notified about the frame's death, otherwise
			 * nobody would clean me (and my brothers) up.
			 */
			notify_interpose_destroy_func(owner, frame_destruction);
		}
		xv_set(owner, XV_KEY_DATA, permlist_key, priv, NULL);
	}
	priv->free_item_func = internal_free_func;
	priv->copy_item_func = internal_copy_func;
	priv->dataoff = -1;
	priv->itemsize = -1;
	priv->label_off = -1;
	priv->item_resource_name = xv_strsave("item_");
	priv->res_cat = PRC_U;

	return XV_OK;
}

static Xv_opaque permlist_set(Permanent_list self, Attr_avlist avlist)
{
	Attr_attribute *attrs;
	Permlist_private *priv = PLPRIV(self);
	Permlist_private *other;

	for (attrs=avlist; *attrs; attrs=attr_next(attrs)) switch ((int)*attrs) {
		case XV_OWNER:
			priv->frame = (Perm_prop_frame)A1;
			if (! xv_get(priv->frame, XV_IS_SUBTYPE_OF, PERMANENT_PROPS)) {
				xv_error(self,
						ERROR_PKG, PERMANENT_LIST,
						ERROR_LAYER, ERROR_PROGRAM,
						ERROR_STRING, "owner must be a PERMANENT_PROPS",
						ERROR_SEVERITY, ERROR_NON_RECOVERABLE,
						NULL);
				return XV_ERROR;
			}
			xv_set(self, PERMLIST_OWNER_SET, NULL);
			ADONE;

		case PERMLIST_OWNER_SET:
			other = (Permlist_private *)xv_get(priv->frame, XV_KEY_DATA,
															permlist_key);
			if (other) {
				priv->next_in_same_frame = other;
			}
			else {
				/* I'm the first (or maybe ONLY) permlist in this frame.
				 * I need to be notified about the frame's death, otherwise
				 * nobody would clean me (and my brothers) up.
				 */
				notify_interpose_destroy_func(priv->frame, frame_destruction);
			}
			xv_set(priv->frame, XV_KEY_DATA, permlist_key, priv, NULL);
			set_frame_callbacks(priv);
			ADONE;

		case FRAME_PROPS_DATA_OFFSET:
			priv->dataoff = (int)A1;
			ADONE;

		case PERMLIST_BEFORE_RESET_FACTORY:
		case PERMLIST_BEFORE_RESET:
			/* subclass responsibility */
			ADONE;

		case PROPLIST_VERIFY_PROC:
			priv->verify_item_func = (verify_func_t)A1;
			ADONE;

		case PROPLIST_FREE_ITEM_PROC:
			priv->free_item_func = (free_func_t)A1;
			ADONE;

		case PROPLIST_COPY_PROC:
			priv->copy_item_func = (copy_func_t)A1;
			ADONE;

		case PROPLIST_ITEM_DATA_SIZE:
			priv->itemsize = (int)A1;
			ADONE;

		case PROPLIST_ENTRY_STRING_OFFSET:
			priv->label_off = (int)A1;
			ADONE;

		case PERMLIST_RESOURCE_CATEGORY:
			priv->res_cat = (Permprop_res_category_t)A1;
			ADONE;

		case PERMLIST_ITEM_RESOURCE_NAME:
			if (priv->item_resource_name) xv_free(priv->item_resource_name);
			priv->item_resource_name = xv_strsave((char *)A1);
			ADONE;

		case PERMLIST_CREATE_TOP:
			create_top(priv, attrs + 1);
			ADONE;

		case PERMLIST_CREATE_BOTTOM:
			create_bottom(priv);
			ADONE;

		case PERMLIST_ADD_RESOURCE:
			new_resource(priv, (char *)A1, (Permprop_res_type_t)A2,
										(int)attrs[3], (Ppmt)attrs[4]);
			ADONE;

		case PERMLIST_PREPARE_DESTROY:
			prepare_destroy(priv);
			ADONE;

		case XV_END_CREATE:
			set_frame_callbacks(priv);
			break;

		default: xv_check_bad_attr(PERMANENT_LIST, A0);
			break;
	}

	return XV_OK;
}

/*ARGSUSED*/
static Xv_opaque permlist_get(Permanent_list self, int *status, Attr_attribute attr, va_list vali)
{
	Permlist_private *priv = PLPRIV(self);

	*status = XV_OK;
	switch ((int)attr) {
		case XV_OWNER: return priv->frame;
		case FRAME_PROPS_DATA_OFFSET: return (Xv_opaque)priv->dataoff;
		case PROPLIST_VERIFY_PROC: return (Xv_opaque)priv->verify_item_func;
		case PROPLIST_FREE_ITEM_PROC: return (Xv_opaque)priv->free_item_func;
		case PROPLIST_COPY_PROC: return (Xv_opaque)priv->copy_item_func;
		case PROPLIST_ITEM_DATA_SIZE: return (Xv_opaque)priv->itemsize;
		case PERMLIST_ITEM_RESOURCE_NAME:
			return (Xv_opaque)priv->item_resource_name;
		case PERMLIST_PROP_MASTER: return (Xv_opaque)priv->list;
		default:
			*status = xv_check_bad_attr(PERMANENT_LIST, attr);
			return (Xv_opaque)XV_OK;
	}
}

static int permlist_destroy(Permanent_list self, Destroy_status status)
{
	if (status == DESTROY_CLEANUP) {
		Permlist_private *priv = PLPRIV(self);
		char *addr;
		Proplist_contents *lcont;

		if (priv->item_resource_name) xv_free(priv->item_resource_name);
		if (priv->fdres) {
			int i;

			for (i = 0; i < priv->num_fdres; i++) {
				xv_free(priv->fdres[i].res_name);
			}
			xv_free(priv->fdres);
		}
		addr = (char *)xv_get(priv->frame, FRAME_PROPS_DATA_ADDRESS);
		if (addr) {
			lcont = (Proplist_contents *)(addr + priv->dataoff);
			if (lcont->item_data) xv_free(lcont->item_data);
		}

		addr = (char *)xv_get(priv->frame, FRAME_PROPS_FACTORY_DATA_ADDRESS);
		if (addr) {
			lcont = (Proplist_contents *)(addr + priv->dataoff);
			if (lcont->item_data) xv_free(lcont->item_data);
		}

		xv_free(priv);
	}
	return XV_OK;
}

static Xv_object permlist_find(Perm_prop_frame owner, const Xv_pkg *pkg,
							Attr_avlist avlist)
{
	Attr_attribute *attrs;
	char *item_resource_name = NULL;
	Permlist_private *priv;

	for (attrs=avlist; *attrs; attrs=attr_next(attrs)) switch ((int)*attrs) {
		case PERMLIST_ITEM_RESOURCE_NAME:
			item_resource_name = (char *)A1;
			break;
	}

	if (! permlist_key) permlist_key = xv_unique_key();
	priv = (Permlist_private *)xv_get(owner, XV_KEY_DATA, permlist_key);
	for (; priv; priv = priv->next_in_same_frame) {
		Permanent_list self = PLPUB(priv);

		if (! xv_get(self, XV_IS_SUBTYPE_OF, pkg)) continue;
		if (item_resource_name &&
			0 != strcmp(item_resource_name,
							(char *)xv_get(self, PERMLIST_ITEM_RESOURCE_NAME)))
		{
			continue;
		}
		return self;
	}
	return XV_NULL;
}

const Xv_pkg xv_permlist_pkg = {
	"PermanentList",
	ATTR_PKG_PERM_LIST,
	sizeof(Xv_permlist),
	XV_GENERIC_OBJECT,
	permlist_init,
	permlist_set,
	permlist_get,
	permlist_destroy,
	permlist_find
};
