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
#include <xview/accel.h>
#include <xview_private/i18n_impl.h>

char accel_c_sccsid[] = "@(#) %M% V%I% %E% %U% $Id: accel.c,v 4.7 2025/03/08 13:04:27 dra Exp $";

#define ADOFF(f) FP_OFF(Accel_descr_t *,f)

/* structure that describes a scrolling list entry */
typedef struct {
	char *keysymname;
	char *listlabel;
	int func_no;
} Accel_descr_t;

typedef void (*frame_accel_func_t)(Xv_opaque, Event *);
typedef int (*permprop_cb_t)(Perm_prop_frame, int);
typedef void (*free_func_t)(Accelerator, Accel_descr_t *fd);
typedef void (*copy_func_t)(Accelerator, Accel_descr_t *src,Accel_descr_t *tgt);
typedef int (*verify_func_t)(Accelerator, Xv_opaque data,
									Proplist_verify_op op, Xv_opaque orig);

/* linked list that describes one accelerator key */
typedef struct _accel {
	struct _accel *next;
	char *description;
	char *keysymname;
	frame_accel_func_t frame_cb;
	Xv_opaque client_data;
	Menu menu;
	Menu_item item;
	char *item_label;

	int enum_value;
	KeySym keysym;
	char was_used;
} *Accel_reg_t;

typedef struct {
	Xv_opaque              public_self;
	free_func_t            free_item_func;
	copy_func_t            copy_item_func;
	verify_func_t          verify_item_func;
	permprop_cb_t          appl_apply;
	Frame                  install_on;
	Panel_item             func_choice;
	Accel_reg_t            registered;
	Permprop_res_enum_pair *pairs;
    struct {
		unsigned installed:1;/*meaning 'enum values' have been 'established' */
		unsigned filled : 1;   /* at least ONE ACCEL_REGISTER... */
    } status_bits;
} Accel_private;

#define A0 *attrs
#define A1 attrs[1]
#define A2 attrs[2]

#define ADD_ATTR(_a_) attrs[an++]=(Attr_attribute)_a_
#define ADONE ATTR_CONSUME(*attrs);break

#define ACPRIV(_x_) XV_PRIVATE(Accel_private, Xv_accelerator, _x_)
#define ACPUB(_x_) XV_PUBLIC(_x_)

static int accel_key = 0;

static void free_accel_descr(Accelerator self, Accel_descr_t *fd)
{
	Accel_private *priv = ACPRIV(self);

	if (fd->keysymname) xv_free(fd->keysymname);
	if (fd->listlabel) xv_free(fd->listlabel);

	if (priv->free_item_func)
		(*(priv->free_item_func))(self, fd);
}

static void copy_accel_descr(Accelerator self, Accel_descr_t *src, Accel_descr_t *target)
{
	Accel_private *priv = ACPRIV(self);

	if (target->keysymname) xv_free(target->keysymname);
	if (target->listlabel) xv_free(target->listlabel);
	if (priv->free_item_func)
		(*(priv->free_item_func))(self, target);

	memcpy((char *)target, (char *)src,
				xv_get(ACPUB(priv), PROPLIST_ITEM_DATA_SIZE));
	target->keysymname = xv_strsave(target->keysymname?target->keysymname:"");
	target->listlabel = xv_strsave(target->listlabel ? target->listlabel : "");

	if (priv->copy_item_func)
		(*(priv->copy_item_func))(self, src, target);
}

static void note_cycle(Panel_item item, int val)
{
	xv_set(item,
			PANEL_DEFAULT_VALUE, (val + 1) % (int)xv_get(item, PANEL_NCHOICES),
			NULL);
}

static int verify_accel_descr(Accelerator self, Xv_opaque data, Proplist_verify_op op, Xv_opaque orig)
{
	Accel_private *priv = ACPRIV(self);
	char buf[1000];
	Accel_reg_t reg;
	Accel_descr_t *fd = (Accel_descr_t *)data;
	Accel_descr_t *origfd = (Accel_descr_t *)orig;

	switch (op) {
		case PROPLIST_APPLY:
		case PROPLIST_INSERT:
			{
				int i, num;
				Property_list l = xv_get(self, PERMLIST_PROP_MASTER);
				KeySym keysym;

				if (! fd->keysymname || ! *fd->keysymname) {
					xv_set(xv_get(ACPUB(priv), XV_OWNER),
							FRAME_LEFT_FOOTER,
								XV_MSG("Please enter a key symbol name"),
							WIN_ALARM,
							NULL);

					return XV_ERROR;
				}

				keysym = XStringToKeysym(fd->keysymname);
				if (keysym == NoSymbol) {
					xv_set(xv_get(ACPUB(priv), XV_OWNER),
							FRAME_LEFT_FOOTER,
								XV_MSG("Invalid key symbol - use help"),
							WIN_ALARM,
							NULL);

					return XV_ERROR;
				}

				num = (int)xv_get(l, PANEL_LIST_NROWS);
				for (i = 0; i < num; i++) {
					Accel_descr_t *tmp;

					tmp = (Accel_descr_t *)xv_get(l, PANEL_LIST_CLIENT_DATA, i);
					if (tmp && tmp != fd && tmp != origfd) {
						if (! strcmp(tmp->keysymname, fd->keysymname)) {
							xv_set(xv_get(ACPUB(priv), XV_OWNER),
								FRAME_LEFT_FOOTER,
									XV_MSG("Key symbol already in list"),
								WIN_ALARM,
								NULL);

							return XV_ERROR;
						}
					}
				}
			}
			/* everything seems ok, now fall through to the CONVERT case
			 * to supply the list label
			 */

		case PROPLIST_CONVERT:
			for (reg = priv->registered; reg; reg = reg->next) {
				if (reg->enum_value == fd->func_no) break;
			}
			sprintf(buf, "%-8s   %s", fd->keysymname, reg?reg->description:"");
			if (fd->listlabel) xv_free(fd->listlabel);
			fd->listlabel = xv_strsave(buf);
			break;

		case PROPLIST_USE_IN_PANEL:
			note_cycle(priv->func_choice, fd->func_no);
			break;

		default:
			break;
	}

	if (priv->verify_item_func)
		return (*(priv->verify_item_func))(self, data, op, orig);

	return XV_OK;
}

static Menu_item determine_item(Accel_reg_t reg)
{
	if (reg->item) return reg->item;
	else if (reg->item_label) {
		return xv_find(reg->menu, MENUITEM,
				XV_AUTO_CREATE, FALSE,
				MENU_STRING, reg->item_label,
				NULL);
	}
	return XV_NULL;
}

static void do_nothing(Xv_opaque unused, Event *ev)
{
}

static void deregister_keysym(Accel_private *priv, Accel_reg_t reg)
{
	if (reg->keysym == NoSymbol) return;

	if (reg->frame_cb) {
		xv_set(priv->install_on,
				FRAME_X_ACCELERATOR, reg->keysym, do_nothing, reg->client_data,
				NULL);
	}
	if (reg->menu) {
		reg->item = determine_item(reg);

		if (reg->item) xv_set(reg->item, MENU_ACCELERATOR, 0, NULL);
	}

	reg->keysym = NoSymbol;
}

static int note_apply(Perm_prop_frame fram, int is_triggered)
{
	int i;
	Accel_reg_t reg;
	Accel_private *priv = (Accel_private *)xv_get(fram, XV_KEY_DATA, accel_key);
	char *add = (char *)xv_get(fram, FRAME_PROPS_DATA_ADDRESS);
	Proplist_contents *lcont = (Proplist_contents *)(add +
					(int)xv_get(ACPUB(priv), FRAME_PROPS_DATA_OFFSET));
	Accel_descr_t *descr = (Accel_descr_t *)lcont->item_data;

	if (priv->appl_apply) {
		int val = (*(priv->appl_apply))(fram, is_triggered);

		if (val != XV_OK) return val;
	}

	for (reg = priv->registered; reg; reg = reg->next) reg->was_used = FALSE;

	for (i = 0; i < lcont->num_items; i++) {
		for (reg = priv->registered; reg; reg = reg->next) {
			if (reg->enum_value == descr[i].func_no) break;
		}

		if (reg) {
			KeySym keysym = XStringToKeysym(descr[i].keysymname);

			reg->was_used = TRUE;

			if (keysym != NoSymbol) {
				if (reg->frame_cb) {
					xv_set(priv->install_on,
							FRAME_X_ACCELERATOR,
								keysym, reg->frame_cb, reg->client_data,
							NULL);
				}

				if (reg->menu) {
					reg->item = determine_item(reg);

					if (reg->item) {
						char resval[100];

						sprintf(resval, "%s+Meta", descr[i].keysymname);
						xv_set(reg->item, MENU_ACCELERATOR, resval, NULL);
					}
				}

				reg->keysym = keysym;
			}
			else deregister_keysym(priv, reg);
		}
	}

	for (reg = priv->registered; reg; reg = reg->next) {
		if (! reg->was_used) {
			deregister_keysym(priv, reg);
		}
		else if (reg->keysym != NoSymbol) {
			Accel_reg_t s;

			if (reg->menu) {
				for (s = priv->registered; s && s != reg; s = s->next) {
					if (reg->menu == s->menu) break;
				}
				if (! s || s == reg) {
					xv_set(priv->install_on, FRAME_MENU_ADD, reg->menu, NULL);
				}
			}
		}
	}

	return XV_OK;
}

static void do_install(Accel_private *priv)
{
	Accel_reg_t reg;
	size_t cnt;
	int i;

	for (cnt = 2, reg = priv->registered; reg; cnt++, reg = reg->next);

	priv->pairs = xv_alloc_n(Permprop_res_enum_pair, cnt);

	for (i = 0, reg = priv->registered; reg; reg = reg->next, ++i) {
		priv->pairs[i].value = reg->enum_value;
		priv->pairs[i].name = reg->keysymname;
	}

	/* default */
	priv->pairs[i].value = 0;
	priv->pairs[i].name = (char *)0;

	xv_set(ACPUB(priv),
		PERMLIST_ADD_RESOURCE, "function", DAP_enum, ADOFF(func_no),priv->pairs,
		NULL);

	priv->status_bits.installed = TRUE;
}

static void cleanup_reg(Accel_reg_t r)
{
	if (r->item_label) xv_free(r->item_label);
	if (r->keysymname) xv_free(r->keysymname);
	if (r->description) xv_free(r->description);

	xv_free(r);
}

static void register_new(Accel_private *priv, Attr_avlist avlist)
{
	Accel_reg_t nr, reg, last;
	Attr_attribute *attrs;

	nr = xv_alloc(struct _accel);
	nr->keysym = NoSymbol;

	for (attrs=avlist; *attrs; attrs=attr_next(attrs)) switch ((int)*attrs) {
		case ACCEL_DESCRIPTION:
			nr->description = xv_strsave((char *)A1);
			ADONE;
		case ACCEL_KEY:
			nr->keysymname = xv_strsave((char *)A1);
			ADONE;
		case ACCEL_MENU_AND_ITEM:
			nr->menu = (Menu)A1;
			nr->item = (Menu_item)A2;
			ADONE;
		case ACCEL_MENU_AND_LABEL:
			nr->menu = (Menu)A1;
			nr->item_label = xv_strsave((char *)A2);
			ADONE;
		case ACCEL_PROC:
			nr->frame_cb = (frame_accel_func_t)A1;
			nr->client_data = (Xv_opaque)A2;
			ADONE;
		default:
			xv_check_bad_attr(ACCELERATOR, A0);
			break;
	}

	if (! nr->keysymname) {
		xv_error(ACPUB(priv),
			ERROR_PKG, ACCELERATOR,
			ERROR_LAYER, ERROR_PROGRAM,
			ERROR_STRING, "incomplete registration, need a key",
			ERROR_SEVERITY, ERROR_RECOVERABLE,
			NULL);

		cleanup_reg(nr);
		return;
	}

	if (! nr->menu && ! nr->frame_cb) {
		xv_error(ACPUB(priv),
			ERROR_PKG, ACCELERATOR,
			ERROR_LAYER, ERROR_PROGRAM,
			ERROR_STRING, "useless registration",
			ERROR_SEVERITY, ERROR_RECOVERABLE,
			NULL);

		cleanup_reg(nr);
		return;
	}

	if (priv->status_bits.installed) {
		char buf[200];

		sprintf(buf, "new keysym '%s' after ACCEL_DONE:\nToo late!",
				nr->keysymname);
		xv_error(ACPUB(priv),
			ERROR_PKG, ACCELERATOR,
			ERROR_LAYER, ERROR_PROGRAM,
			ERROR_STRING, buf,
			ERROR_SEVERITY, ERROR_RECOVERABLE,
			NULL);
		return;
	}
	last = (Accel_reg_t)0;
	for (reg = priv->registered; reg; reg = reg->next) {
		if (! strcmp(nr->keysymname, reg->keysymname)) {
			char errstr[200];

			/* same key means same functionality and same accelerator -
			 * but XView does not allow two menu items to have the same
			 * accelerators
			 */
			if ((reg->menu && nr->menu) ||
				(reg->frame_cb && nr->frame_cb))
			{
				sprintf(errstr,
							"key '%s' seen twice (on two menus or two frames)",
							nr->keysymname);
				xv_error(ACPUB(priv),
						ERROR_PKG, ACCELERATOR,
						ERROR_LAYER, ERROR_PROGRAM,
						ERROR_STRING, errstr,
						ERROR_SEVERITY, ERROR_RECOVERABLE,
						NULL);
				cleanup_reg(nr);
				return;
			}

			/* if we come here, one has a frame_cb and the other a menu ... */
			if (reg->menu) {
				reg->frame_cb = nr->frame_cb;
				reg->client_data = nr->client_data;
			}
			else {
				reg->menu = nr->menu;
				reg->item = nr->item;
				reg->item_label = nr->item_label;
				nr->item_label = (char *)0;
			}
			cleanup_reg(nr);
			return;
		}
		last = reg;
	}

	if (! nr->description) {
		xv_error(ACPUB(priv),
			ERROR_PKG, ACCELERATOR,
			ERROR_LAYER, ERROR_PROGRAM,
			ERROR_STRING, "incomplete registration, need a description",
			ERROR_SEVERITY, ERROR_RECOVERABLE,
			NULL);

		cleanup_reg(nr);
		return;
	}

	if (last) {
		nr->enum_value = last->enum_value + 1;
		last->next = nr;
	}
	else {
		nr->enum_value = 0;
		priv->registered = nr;
	}
}

static char *make_help(Accel_private *priv, char *str)
{
	char *myhelp, *itemhelp;

	myhelp = (char *)xv_get(ACPUB(priv), XV_HELP_DATA);
	if (! myhelp) return (char *)0;

	itemhelp = xv_malloc(strlen(myhelp) + strlen(str) + 2);
	sprintf(itemhelp, "%s_%s", myhelp, str);
	return itemhelp;
}

static void free_help_data(Xv_opaque obj, int key, char *data)
{
	if (key == XV_HELP && data) xv_free(data);
}

static void create_items(Accel_private *priv)
{
	int chcnt;
	Attr_attribute attrs[30];
	int an = 0;
	Property_list list;
	Accel_reg_t reg;

	for (chcnt = 0, reg = priv->registered; reg; chcnt++, reg = reg->next);

	ADD_ATTR(PERMLIST_CREATE_TOP);
	ADD_ATTR(PANEL_LABEL_STRING);
	ADD_ATTR(XV_MSG("Accelerators:"));
	ADD_ATTR(PANEL_LIST_DISPLAY_ROWS);
	ADD_ATTR(7);
	ADD_ATTR(PANEL_LIST_WIDTH);
	ADD_ATTR(200);
	ADD_ATTR(FRAME_SHOW_RESIZE_CORNER);
	ADD_ATTR(TRUE);
	ADD_ATTR(0);
	ADD_ATTR(0);
	ADD_ATTR(0);
	xv_set_avlist(ACPUB(priv), attrs);
	list = xv_get(ACPUB(priv), PERMLIST_PROP_MASTER);

	chcnt = ((chcnt - 1) / 16) + 1;
	xv_set(xv_get(ACPUB(priv), XV_OWNER),
				FRAME_PROPS_CREATE_ITEM,
					FRAME_PROPS_ITEM_SPEC, &priv->func_choice, -1, PANEL_CHOICE,
					PANEL_DISPLAY_LEVEL, PANEL_CURRENT,
					PANEL_DIRECTION, PANEL_HORIZONTAL,
					PANEL_LABEL_STRING, XV_MSG("Function:"),
					PANEL_NOTIFY_PROC, note_cycle,
					PANEL_CHOICE_NCOLS, chcnt,
					FRAME_PROPS_DATA_OFFSET, ADOFF(func_no),
					FRAME_PROPS_SLAVE_OF, list,
					XV_HELP_DATA, make_help(priv, "func_no"),
					XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
					NULL,
				FRAME_PROPS_CREATE_ITEM,
					FRAME_PROPS_ITEM_SPEC, NULL, -1, PANEL_TEXT,
					XV_INSTANCE_NAME, "keysymname",
					PANEL_LABEL_STRING, XV_MSG("Triggered by:"),
					PANEL_VALUE_DISPLAY_LENGTH, 10,
					PANEL_VALUE_STORED_LENGTH, 100,
					FRAME_PROPS_DATA_OFFSET, ADOFF(keysymname),
					FRAME_PROPS_SLAVE_OF, list,
					XV_HELP_DATA, make_help(priv, "keysymname"),
					XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
					NULL,
				FRAME_PROPS_CREATE_ITEM,
					FRAME_PROPS_ITEM_SPEC, NULL,FRAME_PROPS_MOVE, PANEL_MESSAGE,
					PANEL_LABEL_STRING, XV_MSG("+ Meta"),
					PANEL_LABEL_BOLD, FALSE,
					XV_HELP_DATA, make_help(priv, "keysymname"),
					XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
					NULL,
				NULL);

	for (reg = priv->registered; reg; reg = reg->next) {
		xv_set(priv->func_choice,
				PANEL_CHOICE_STRING, reg->enum_value, reg->description,
				NULL);
	}

	xv_set(ACPUB(priv), PERMLIST_CREATE_BOTTOM, NULL);
}

static int accel_init(Perm_prop_frame owner, Accelerator slf,
						Attr_avlist avlist, int *u)
{
	Xv_accelerator *self = (Xv_accelerator *)slf;
	Accel_private *priv;
	Attr_attribute attrs[30];
	int an = 0;

	if (! accel_key) accel_key = xv_unique_key();

	priv = xv_alloc(Accel_private);
	if (!priv) return XV_ERROR;

	priv->public_self = (Xv_opaque)self;
	self->private_data = (Xv_opaque)priv;

	priv->status_bits.installed = FALSE;
	priv->status_bits.filled = FALSE;
	xv_set(owner, XV_KEY_DATA, accel_key, priv, NULL);

	ADD_ATTR(PROPLIST_VERIFY_PROC);
	ADD_ATTR(verify_accel_descr);
	ADD_ATTR(PROPLIST_FREE_ITEM_PROC);
	ADD_ATTR(free_accel_descr);
	ADD_ATTR(PROPLIST_COPY_PROC);
	ADD_ATTR(copy_accel_descr);
	ADD_ATTR(PROPLIST_ITEM_DATA_SIZE);
	ADD_ATTR(sizeof(Accel_descr_t));
	ADD_ATTR(PERMLIST_RESOURCE_CATEGORY);
	ADD_ATTR(PRC_D);
	ADD_ATTR(0);
	xv_super_set_avlist((Xv_opaque)self, ACCELERATOR, attrs);

	xv_set((Xv_opaque)self,
			PROPLIST_ENTRY_STRING_OFFSET, ADOFF(listlabel),
			PERMLIST_ITEM_RESOURCE_NAME, "accel",
			PERMLIST_ADD_RESOURCE, "keysymname", DAP_string, ADOFF(keysymname), "",
			NULL);

	return XV_OK;
}

static Xv_opaque accel_set(Accelerator self, Attr_avlist avlist)
{
	Attr_attribute *attrs;
	Accel_private *priv = ACPRIV(self);

	for (attrs=avlist; *attrs; attrs=attr_next(attrs)) switch ((int)*attrs) {
		case PROPLIST_VERIFY_PROC:
			priv->verify_item_func = (verify_func_t)A1;
			ADONE;

		case PROPLIST_FREE_ITEM_PROC:
			priv->free_item_func = (free_func_t)A1;
			ADONE;

		case PROPLIST_COPY_PROC:
			priv->copy_item_func = (copy_func_t)A1;
			ADONE;

		case ACCEL_CREATE_ITEMS:
			create_items(priv);
			ADONE;

		case ACCEL_REGISTER:
			priv->status_bits.filled = TRUE;
			register_new(priv, attrs + 1);
			ADONE;

		case ACCEL_FRAME:
			priv->install_on = (Frame)A1;
			xv_set(priv->install_on, XV_KEY_DATA, accel_key, self, NULL);
			/* der letzte gewinnt */
			xv_set(XV_SERVER_FROM_WINDOW(priv->install_on),
						XV_KEY_DATA, accel_key, self,
						NULL);
			ADONE;

		case ACCEL_DONE:
			do_install(priv);
			ADONE;

		case PERMLIST_BEFORE_RESET_FACTORY:
		case PERMLIST_BEFORE_RESET:
			if (! priv->status_bits.installed && priv->status_bits.filled) {
				/* we don't really need ACCEL_DONE any longer... */
				do_install(priv);
			}
			ADONE;

		case XV_END_CREATE:
			priv->appl_apply = (permprop_cb_t)xv_get(xv_get(self, XV_OWNER),
												FRAME_PROPS_APPLY_PROC);
			xv_set(xv_get(self, XV_OWNER),
					FRAME_PROPS_APPLY_PROC, note_apply,
					NULL);
			break;

		default: xv_check_bad_attr(ACCELERATOR, A0);
			break;
	}

	return XV_OK;
}

/*ARGSUSED*/
static Xv_opaque accel_get(Accelerator self, int *status, Attr_attribute attr, va_list vali)
{
	Accel_private *priv = ACPRIV(self);

	*status = XV_OK;
	switch ((int)attr) {
		case ACCEL_FRAME: return priv->install_on;
		case PROPLIST_VERIFY_PROC: return (Xv_opaque)priv->verify_item_func;
		case PROPLIST_FREE_ITEM_PROC: return (Xv_opaque)priv->free_item_func;
		case PROPLIST_COPY_PROC: return (Xv_opaque)priv->copy_item_func;
		default:
			*status = xv_check_bad_attr(ACCELERATOR, attr);
			return (Xv_opaque)XV_OK;
	}
}

static int accel_destroy(Accelerator self, Destroy_status status)
{
	if (status == DESTROY_CLEANUP) {
		Accel_private *priv = ACPRIV(self);
		Accel_reg_t reg, next;

		for (reg = priv->registered; reg; reg = next) {
			next = reg->next;
			cleanup_reg(reg);
		}

		if (priv->pairs) xv_free(priv->pairs);

		xv_free(priv);
	}
	return XV_OK;
}

/* public function */
Accelerator xv_accel_get_accelerator(Xv_window win_maybe_NULL)
{
	if (win_maybe_NULL) {
		return xv_get(XV_SERVER_FROM_WINDOW(win_maybe_NULL),
								XV_KEY_DATA, accel_key);
	}

	return xv_get(xv_default_server, XV_KEY_DATA, accel_key);
}

const Xv_pkg xv_accelerator_pkg = {
	"Accelerators",
	ATTR_PKG_ACCEL,
	sizeof(Xv_accelerator),
	PERMANENT_LIST,
	accel_init,
	accel_set,
	accel_get,
	accel_destroy,
	NULL
};
