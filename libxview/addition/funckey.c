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
#define FUNCKEY_INTERNAL_USE_ONLY 1
#include <xview/funckey.h>
#include <xview/scrollw.h>
#include <xview/defaults.h>
#include <xview_private/i18n_impl.h>

char funckey_c_sccsid[] = "@(#) %M% V%I% %E% %U% $Id: funckey.c,v 4.19 2025/06/16 18:26:23 dra Exp $";

#define NUM_FUNC 12

#define FDOFF(field) FP_OFF(Funckey_descr_t *,field)

typedef int (*permprop_cb_t)(Perm_prop_frame, int);
typedef void (*free_func_t)(Function_keys, Funckey_descr_t *fd);
typedef void (*copy_func_t)(Function_keys, Funckey_descr_t *src,
									Funckey_descr_t *target);
typedef int (*verify_func_t)(Function_keys, Xv_opaque data,
									Proplist_verify_op op, Xv_opaque orig);
typedef void (*funckey_function_t)(Xv_opaque, Event *, Xv_opaque);
/* can be used with a PANEL_BUTTON's notify proc: */
typedef void (*funckey_pfunction_t)(Xv_opaque, Event *, Xv_opaque);
/* can be used with a MENU_NOTIFY_PROC: */
typedef void (*funckey_mfunction_t)(Xv_opaque, Xv_opaque, Xv_opaque);
typedef void (*fk_split_proc_t)(Xv_window, Xv_Window, int);

/* An application works similar to the following:
	++  create a FUNCTION_KEYS [[ this can be created with an XV_NULL owner,
								see set and get methods for attr XV_OWNER
								in permlist.c ]]
	++  register functions using FUNCKEY_REGISTER
	++  call FUNCKEY_INSTALL (only if you want, not really necessary )
	++  if you want to use it in a property window or (always) want to read
		the saved settings (using PERM_RESET_FROM_DB etc), do
			· set XV_OWNER (unless already done)
			· call FUNCKEY_CREATE_TOP
			· call PERMLIST_CREATE_BOTTOM
	++  use FUNCKEY_SOFT_KEY_WINDOWS to register all subwindows that are
		supposed to react on function keys. Subclasses of OPENWIN and PANEL
		are handled automatically. Others should use their own event handlers
		and call xv_get(fkey, FUNCKEY_HANDLE_EVENT, ev)
*/

typedef enum {
	INITIALIZED,
	LAID_OUT,
	FINISHED
} fk_ui_state_t;

typedef enum {
	GENERIC,
	BUTTON_LIKE,
	MENU_LIKE,
	OLWM,
	FALL_THROUGH
} fk_cb_type_t;

/* linked list that describes one available function */
typedef struct _func {
	struct _func *next;
	char *description;
	char *code;

	int enum_value;
	char was_used;

	fk_cb_type_t cbtype;
	union {
		struct {
			funckey_pfunction_t cb;
			Xv_opaque client_data;
		} g;
		struct {
			Xv_opaque panel_item; /* or similar.... */
		} p;
		struct {
			Xv_opaque menu;
			Xv_opaque menu_item;
		} m;
		Atom o;
	} u;
} *Func_reg_t;


typedef struct _Funckey_private {
	Xv_opaque           public_self;
	free_func_t         free_item_func;
	copy_func_t         copy_item_func;
	verify_func_t       verify_item_func;
	permprop_cb_t       appl_apply;
	funckey_event_cb_t          event_cb;
	Xv_window          *softkeywins;
	int                 num_softkeywins;
	Panel_item          func_choice;
	Func_reg_t          registered;
	Permprop_res_enum_pair *pairs;
	fk_ui_state_t state;
	int include_wm_functions;
	int list_created;
	int installed;  /* meaning that the 'enum values' have been 'established' */
	struct _Funckey_private *next_instance; /* list within ONE propframe  */
} Funckey_private;

#define A0 *attrs
#define A1 attrs[1]
#define A2 attrs[2]

#define ADONE ATTR_CONSUME(*attrs);break

#define FKPRIV(_x_) XV_PRIVATE(Funckey_private, Xv_funckeys, _x_)
#define FKPUB(_x_) XV_PUBLIC(_x_)

static char *modstr[] = {
	"                  ",
	"+Shift         ",
	"+Ctrl          ",
	"+Shift+Ctrl",
	"+Meta           ",
	"+Shift+Meta     ",
	"+Ctrl+Meta      ",
	"+Shift+Ctrl+Meta",
};

static Attr_attribute fkey_key = 0;
#define OWNER_PRIV_LIST XV_KEY_DATA,fkey_key

static void free_func_descr(Function_keys self, Funckey_descr_t *fd)
{
	Funckey_private *priv = FKPRIV(self);

	if (fd->label) xv_free(fd->label);
	if (fd->listlabel) xv_free(fd->listlabel);

	if (priv->free_item_func)
		(*(priv->free_item_func))(self, fd);
}

static void copy_func_descr(Function_keys self, Funckey_descr_t *src, Funckey_descr_t *target)
{
	Funckey_private *priv = FKPRIV(self);

	if (target->label) xv_free(target->label);
	if (target->listlabel) xv_free(target->listlabel);
	if (priv->free_item_func)
		(*(priv->free_item_func))(self, target);

	memcpy((char *)target, (char *)src,
				xv_get(FKPUB(priv), PROPLIST_ITEM_DATA_SIZE));
	target->label = xv_strsave(target->label ? target->label : "");
	target->listlabel =
				xv_strsave(target->listlabel?target->listlabel:"");

	if (priv->copy_item_func)
		(*(priv->copy_item_func))(self, src, target);
}

static void improve_layout(Funckey_private *priv)
{
	int y;

	if (priv->state != LAID_OUT) return;
	if (! priv->func_choice) return;

	priv->state = FINISHED;
	y = (int)xv_get(priv->func_choice, XV_Y);
	xv_set(xv_get(priv->func_choice, XV_KEY_DATA, fkey_key),
				XV_Y, y + 5,
				NULL);
}

static int verify_func_descr(Function_keys self, Xv_opaque data, Proplist_verify_op op, Xv_opaque orig)
{
	Funckey_private *priv = FKPRIV(self);
	char buf[1000];
	Funckey_descr_t *fd = (Funckey_descr_t *)data;
	Funckey_descr_t *origfd = (Funckey_descr_t *)orig;

	switch (op) {
		case PROPLIST_APPLY:
		case PROPLIST_INSERT:
			{
				int i, num;
				Property_list l = xv_get(self, PERMLIST_PROP_MASTER);

				num = (int)xv_get(l, PANEL_LIST_NROWS);
				for (i = 0; i < num; i++) {
					Funckey_descr_t *tmp;

					tmp = (Funckey_descr_t *)xv_get(l, PANEL_LIST_CLIENT_DATA, i);
					if (tmp && tmp != fd && tmp != origfd) {
						if (tmp->fkeynumber == fd->fkeynumber &&
							tmp->mods == fd->mods)
						{
							xv_set(xv_get(FKPUB(priv), XV_OWNER),
								FRAME_LEFT_FOOTER,
									XV_MSG("key/modifier combination already in list"),
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
			sprintf(buf, "F%d%s           %s",
						fd->fkeynumber+1, modstr[fd->mods], fd->label);
			if (fd->listlabel) xv_free(fd->listlabel);
			fd->listlabel = xv_strsave(buf);
			break;

		case PROPLIST_USE_IN_PANEL:
			improve_layout(priv);
			break;

		default:
			break;
	}

	if (priv->verify_item_func)
		return (*(priv->verify_item_func))(self, data, op, orig);

	return XV_OK;
}

static void update_func_key_labels(Funckey_private *priv)
{
	int itemsize = (int)xv_get(FKPUB(priv), PROPLIST_ITEM_DATA_SIZE);
	char *ptr, *add = (char *)xv_get(xv_get(FKPUB(priv), XV_OWNER),
											FRAME_PROPS_DATA_ADDRESS);
	Proplist_contents *lcont = (Proplist_contents *)(add +
					(int)xv_get(FKPUB(priv), FRAME_PROPS_DATA_OFFSET));
	Funckey_descr_t *descr = 0;
	char buf[1000];
	int i, j;

	ptr = (char *)lcont->item_data;
	buf[0] = '\0';
	for (i = 0; i < NUM_FUNC; i++) {
		int found = FALSE;

		for (j = 0; j < lcont->num_items; j++) {
			descr = (Funckey_descr_t *)(ptr + j * itemsize);
			if (descr->fkeynumber == i && descr->mods == 0) {
				found = TRUE;
				break;
			}
		}

		if (found && descr->label && *descr->label) {
			strcat(buf, descr->label);
			strcat(buf, "\n");
		}
		else strcat(buf, " \n");
	}

	for (i = 0; i < priv->num_softkeywins; i++) {
		Xv_window view, pw, win = priv->softkeywins[i];

		xv_set(win, WIN_SOFT_FNKEY_LABELS, buf, NULL);
		if (xv_get(win, XV_IS_SUBTYPE_OF, OPENWIN)) {
			OPENWIN_EACH_VIEW(win, view)
				xv_set(view, WIN_SOFT_FNKEY_LABELS, buf, NULL);
			OPENWIN_END_EACH
		}

		if (xv_get(win, XV_IS_SUBTYPE_OF, OPENWIN)) {
			OPENWIN_EACH_PW(win, pw)
				xv_set(pw, WIN_SOFT_FNKEY_LABELS, buf, NULL);
			OPENWIN_END_EACH
		}
	}
}

static int note_apply(Perm_prop_frame fram, int is_triggered)
{
	Funckey_private *priv =
				(Funckey_private *)xv_get(fram, OWNER_PRIV_LIST);

	while (priv) {
		if (priv->appl_apply) {
			int val = (*(priv->appl_apply))(fram, is_triggered);

			if (val != XV_OK) return val;
		}

		update_func_key_labels(priv);
		priv = priv->next_instance;
	}

	return XV_OK;
}

static char *make_help(Funckey_private *priv, char *str)
{
	char *myhelp, *itemhelp;

	myhelp = (char *)xv_get(FKPUB(priv), XV_HELP_DATA);
	if (! myhelp) return (char *)0;

	itemhelp = xv_malloc(strlen(myhelp) + strlen(str) + 2);
	sprintf(itemhelp, "%s_%s", myhelp, str);
	return itemhelp;
}

static void free_help_data(Xv_opaque obj, int key, char *data)
{
	if (key == XV_HELP && data) xv_free(data);
}

static void note_function(Panel_item item, int value)
{
	xv_set(xv_get(item, XV_KEY_DATA, fkey_key),
			PANEL_VALUE, xv_get(item, PANEL_CHOICE_STRING, value),
			NULL);
}

static void create_top(Funckey_private *priv, Attr_attribute *avlist)
{
	Func_reg_t reg;
	Function_keys fk = FKPUB(priv);
	Property_list list;
	Panel_item label, fkc;
	int max_top, i;
	Panel pan;
	Menu menu;

	if (! priv->list_created) {
		int i = 0;
		Attr_attribute av[30];

		av[i++] = (Attr_attribute)PERMLIST_CREATE_TOP;
		av[i++] = (Attr_attribute)PANEL_LABEL_STRING;
		av[i++] = (Attr_attribute)XV_MSG("Function Keys:");
		av[i++] = (Attr_attribute)PANEL_LIST_DISPLAY_ROWS;
		av[i++] = (Attr_attribute)7;
		av[i++] = (Attr_attribute)PANEL_LIST_WIDTH;
		av[i++] = (Attr_attribute)300;
		av[i++] = (Attr_attribute)0;
		av[i++] = (Attr_attribute)0;
		av[i++] = (Attr_attribute)0;
		xv_super_set_avlist(fk, FUNCTION_KEYS, av);
	}

	list = xv_get(fk, PERMLIST_PROP_MASTER);

	xv_set(xv_get(fk, XV_OWNER),
				FRAME_PROPS_CREATE_ITEM,
					FRAME_PROPS_ITEM_SPEC, &fkc, -1, PANEL_CHOICE,
					PANEL_DISPLAY_LEVEL, PANEL_CURRENT,
					PANEL_LABEL_STRING, XV_MSG("Key:"),
					PANEL_CHOICE_STRINGS, "F10", NULL, /* Layout purpose... */
					PANEL_CHOICE_NCOLS, 2,
					PANEL_DEFAULT_VALUE, 1,
					FRAME_PROPS_DATA_OFFSET, FDOFF(fkeynumber),
					FRAME_PROPS_SLAVE_OF, list,
					XV_HELP_DATA, make_help(priv, "fkey"),
					XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
					NULL,
				FRAME_PROPS_CREATE_ITEM,
					FRAME_PROPS_ITEM_SPEC, NULL, FRAME_PROPS_MOVE, PANEL_CHOICE,
					PANEL_CHOOSE_ONE, FALSE,
					PANEL_LABEL_STRING, XV_MSG("modified by:"),
					PANEL_CHOICE_STRINGS,
						XV_MSG("Shift"),
						XV_MSG("Ctrl"),
						XV_MSG("Meta"),
/* 						XV_MSG("Alt"), */
						NULL,
					FRAME_PROPS_DATA_OFFSET, FDOFF(mods),
					FRAME_PROPS_SLAVE_OF, list,
					XV_HELP_DATA, make_help(priv, "modif"),
					XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
					NULL,
				NULL);

	pan = xv_get(fkc, XV_OWNER);

	{
		char *hlp = (char *)xv_get(pan, XV_HELP_DATA);

		if (hlp && *hlp) {
			if (0 == strcmp(hlp, "xview:panel")) {
				xv_set(pan, 
						XV_HELP_DATA, make_help(priv, "default_panel_help"),
						XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
						NULL);
			}
		}
		else {
			xv_set(pan, 
					XV_HELP_DATA, make_help(priv, "default_panel_help"),
					XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
					NULL);
		}
	}

	max_top = defaults_get_integer("OpenWindows.NumberOfTopFkeys",
									"OpenWindows.NumberOfTopFkeys", 12);
	for (i = 0; i < max_top; i++) {
		char buf[20];

		sprintf(buf, "F%d", i+1);
		xv_set(fkc, PANEL_CHOICE_STRING, i, buf, NULL);
	}

	if (! priv->registered) return;

	xv_set(xv_get(fk, XV_OWNER),
				FRAME_PROPS_CREATE_ITEM,
					FRAME_PROPS_ITEM_SPEC, &priv->func_choice, -1, PANEL_CHOICE,
					ATTR_LIST, avlist,
					PANEL_LABEL_STRING, XV_MSG("Function:"),
					PANEL_DISPLAY_LEVEL, PANEL_CURRENT,
					PANEL_NOTIFY_PROC, note_function,
					XV_HELP_DATA, make_help(priv, "fkey_function"),
					XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
					FRAME_PROPS_DATA_OFFSET, FDOFF(enum_val),
					FRAME_PROPS_SLAVE_OF, list,
					NULL,
				NULL);

	for (reg = priv->registered; reg; reg = reg->next) {
		xv_set(priv->func_choice,
				PANEL_CHOICE_STRING, reg->enum_value, reg->description,
				NULL);
	}

	xv_set(xv_get(fk, XV_OWNER),
				FRAME_PROPS_CREATE_ITEM,
					FRAME_PROPS_ITEM_SPEC, &label, FRAME_PROPS_MOVE, PANEL_TEXT,
					XV_INSTANCE_NAME, "funckeylabel",
					PANEL_LABEL_STRING, XV_MSG("Label:"),
					PANEL_VALUE_DISPLAY_LENGTH, 10,
					PANEL_VALUE_STORED_LENGTH, 80,
					FRAME_PROPS_DATA_OFFSET, FDOFF(label),
					FRAME_PROPS_SLAVE_OF, list,
					XV_HELP_DATA, make_help(priv, "label"),
					XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
					NULL,
				NULL);

	xv_set(priv->func_choice, XV_KEY_DATA, fkey_key, label, NULL);

	menu = xv_get(priv->func_choice, PANEL_ITEM_MENU);
	if (menu) {
		for (reg = priv->registered; reg; reg = reg->next) {
			Menu_item item = xv_get(menu, MENU_NTH_ITEM, reg->enum_value+1);

			if (item) {
				char helpbuf[100];

				sprintf(helpbuf, "fk_%s", reg->code);
				xv_set(item,
						XV_HELP_DATA, make_help(priv, helpbuf),
						XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
						NULL);
			}
		}
	}
}

static void process_wm_event(Frame base, Atom at)
{
	XEvent xcl;

	/* We send ClientMessages of type _OL_MENU_FULL
	 * to the root window. These ClientMessages have
	 * + window = xid of the frame
	 * + data.l[0] : PROPAGATE
	 */

	xcl.type = ClientMessage;
	xcl.xclient.window = (Window)xv_get(base, XV_XID);
	xcl.xclient.message_type = (Atom)xv_get(XV_SERVER_FROM_WINDOW(base),
												SERVER_WM_MENU_FULL);
	xcl.xclient.format = 32;
	xcl.xclient.data.l[0] = (long)at;
	xcl.xclient.data.l[1] = 0L;
	xcl.xclient.data.l[2] = 0L;
	xcl.xclient.data.l[3] = 0L;
	xcl.xclient.data.l[4] = 0L;

	XSendEvent((Display *)xv_get(base, XV_DISPLAY),
		(Window)xv_get(xv_get(base,XV_ROOT),XV_XID),
		FALSE,
		SubstructureRedirectMask | SubstructureNotifyMask,
		&xcl);
}

static int process_function_key(Funckey_private *priv, Event *ev)
{
	int i, mod = 0;
	char *add, *ptr;
	Proplist_contents *clcont;
	Function_keys self = FKPUB(priv);
	int itemsize = (int)xv_get(self, PROPLIST_ITEM_DATA_SIZE);
	int dataoff = (int)xv_get(self, FRAME_PROPS_DATA_OFFSET);
	Perm_prop_frame pf;

	if (event_is_up(ev)) return FALSE;

#ifdef THAT_WAS_SUNS_KEY_TOP
	/* what the SUN-boys consider as 'KEY_TOP': F1 to F10 */
	/* F11 comes here as KEY_LEFT(1), F12 as KEY_LEFT(2) */
	if (! (event_is_key_top(ev)
		|| event_id(ev) == KEY_LEFT(1)
		|| event_id(ev) == KEY_LEFT(2)))
	{
		return FALSE;
	}
#else /* THAT_WAS_SUNS_KEY_TOP */
	/* modified in XView4.0: F11 and F12 are now also KEY_TOP */
	if (! event_is_key_top(ev)) return FALSE;
#endif /* THAT_WAS_SUNS_KEY_TOP */

	pf = xv_get(self, XV_OWNER);
	add = (char *)xv_get(pf, FRAME_PROPS_DATA_ADDRESS);
	clcont = (Proplist_contents *)(add + dataoff);
	ptr = (char *)clcont->item_data;

	if (event_shift_is_down(ev)) mod |= 1;
	if (event_ctrl_is_down(ev)) mod |= 2;
	if (event_meta_is_down(ev)) mod |= 4;

	for (i = 0; i < clcont->num_items; i++) {
		Funckey_descr_t *fkd = (Funckey_descr_t *)(ptr + itemsize * i);

#ifdef THAT_WAS_SUNS_KEY_TOP
		int fkey_ev_id;
		if (fkd->fkeynumber < 10) fkey_ev_id = KEY_TOP(fkd->fkeynumber+1);
		else fkey_ev_id = KEY_LEFT(fkd->fkeynumber-10);

		if (fkey_ev_id == event_id(ev) && mod == fkd->mods)
#else /* THAT_WAS_SUNS_KEY_TOP */
		if (KEY_TOP(fkd->fkeynumber+1)==event_id(ev) && mod==fkd->mods)
#endif /* THAT_WAS_SUNS_KEY_TOP */
		{
			Func_reg_t reg;

			for (reg = priv->registered; reg; reg = reg->next) {
				if (reg->enum_value == fkd->enum_val) {
					switch (reg->cbtype) {
						case GENERIC:
							(*reg->u.g.cb)(reg->u.g.client_data, ev,
												(Xv_opaque)fkd);
							break;
						case BUTTON_LIKE:
							{
								Panel_item b = reg->u.p.panel_item;
								funckey_pfunction_t cb;

								cb = (funckey_pfunction_t)xv_get(b, 
														PANEL_NOTIFY_PROC);

								(*cb)(b, ev, (Xv_opaque)fkd);
							}
							break;
						case MENU_LIKE:
							{
								Menu menu = reg->u.m.menu;
								Menu_item item = reg->u.m.menu_item;
								funckey_mfunction_t cb;

								cb = (funckey_mfunction_t)xv_get(item, 
														MENU_NOTIFY_PROC);
								if (! cb) {
									cb = (funckey_mfunction_t)xv_get(menu, 
														MENU_NOTIFY_PROC);
								}
								(*cb)(menu, item, (Xv_opaque)fkd);
							}
							break;
						case OLWM:
							process_wm_event(xv_get(pf, XV_OWNER), reg->u.o);
							break;
						case FALL_THROUGH:
							return FALSE;
					}
					return TRUE;
				}
			}
			return TRUE;
		}
	}
	return FALSE;
}


static Notify_value note_event(Xv_window w, Notify_event event,
					Notify_arg arg, Notify_event_type type)
{
	Event *ev = (Event *)event;
	Funckey_private *priv = (Funckey_private *)xv_get(w, XV_KEY_DATA, fkey_key);

	if (priv) {
		if (process_function_key(priv, ev)) return NOTIFY_DONE;

		if (priv->event_cb) {
			int done = (*(priv->event_cb))(FKPUB(priv), w, ev);

			if (done) return NOTIFY_DONE;
		}
	}

	return notify_next_event_func(w, event, arg, type);
}

static void note_split(Xv_window o, Xv_window n, int pos)
{
	Openwin owin = xv_get(o, XV_OWNER);
	Xv_window pw;
	Funckey_private *priv = (Funckey_private *)xv_get(o, XV_KEY_DATA, fkey_key);
	fk_split_proc_t origproc = (fk_split_proc_t)xv_get(owin,
											XV_KEY_DATA, fkey_key);

	if (origproc) {
		(*origproc)(o, n, pos);
	}

	xv_set(n, XV_KEY_DATA, fkey_key, priv, NULL);
	OPENWIN_EACH_PW(owin, pw)
		if (n == xv_get(pw, XV_OWNER)) {
			xv_set(pw, XV_KEY_DATA, fkey_key, priv, NULL);
			notify_interpose_event_func(pw, note_event, NOTIFY_SAFE);
			xv_set(n, XV_KEY_DATA, fkey_key, priv, NULL);
		}
	OPENWIN_END_EACH

	update_func_key_labels(priv);
	notify_interpose_event_func(n, note_event, NOTIFY_SAFE);
}

static int handle_openwin(Funckey_private *priv, Xv_window win)
{
	if (xv_get(win, XV_IS_SUBTYPE_OF, OPENWIN)) {
		Xv_window view, pw;
		int have_pw = FALSE;

		xv_set(win,
			XV_KEY_DATA, fkey_key, xv_get(win, OPENWIN_SPLIT_INIT_PROC),
			OPENWIN_SPLIT,
				OPENWIN_SPLIT_INIT_PROC, note_split,
				NULL,
			NULL);

		OPENWIN_EACH_PW(win, pw)
			have_pw = TRUE;
			xv_set(pw, XV_KEY_DATA, fkey_key, priv, NULL);
			notify_interpose_event_func(pw, note_event, NOTIFY_SAFE);
		OPENWIN_END_EACH

		OPENWIN_EACH_VIEW(win, view)
			xv_set(view, XV_KEY_DATA, fkey_key, priv, NULL);
			if (! have_pw) {
				notify_interpose_event_func(view, note_event, NOTIFY_SAFE);
			}
		OPENWIN_END_EACH

		return TRUE;
	}

	return FALSE;
}

static void add_soft_key_window(Funckey_private *priv, Xv_window win,
						int with_event_handling)
{
	if (priv->num_softkeywins > 0) {
		Xv_window *old_skw = priv->softkeywins;
		int i, oldnum = priv->num_softkeywins;

		priv->num_softkeywins += 1;
		priv->softkeywins = xv_alloc_n(Xv_window, (size_t)priv->num_softkeywins);
		for (i = 0; i < oldnum; i++) priv->softkeywins[i] = old_skw[i];
		xv_free(old_skw);
	}
	else {
		priv->num_softkeywins = 1;
		priv->softkeywins = xv_alloc_n(Xv_window, 1L);
	}
	priv->softkeywins[priv->num_softkeywins - 1] = win;
	if (with_event_handling) {
		handle_openwin(priv, win);
	}
}

static void soft_key_windows(Funckey_private *priv, Xv_window *wins)
{
	unsigned i, cnt;

	if (priv->softkeywins) xv_free(priv->softkeywins);

	for (cnt = 0; wins[cnt]; ++cnt);

	priv->num_softkeywins = cnt;
	priv->softkeywins = xv_alloc_n(Xv_window, (size_t)cnt);
	for (i = 0; i < cnt; i++) {
		Xv_window win = wins[i];

		priv->softkeywins[i] = win;

		if (! handle_openwin(priv, win)) {
			if (xv_get(win, XV_IS_SUBTYPE_OF, PANEL)) {
				xv_set(win, XV_KEY_DATA, fkey_key, priv, NULL);
				notify_interpose_event_func(win, note_event, NOTIFY_SAFE);
			}
		}
	}
}

static void register_wm_function(Funckey_private *priv, char *code,
								char *descr, Xv_server srv, const char *func);

static void do_install(Funckey_private *priv)
{
	Function_keys fk = FKPUB(priv);
	Func_reg_t reg;
	size_t cnt;
	int i;
	Perm_prop_frame pf = xv_get(fk, XV_OWNER);
	Xv_server srv = XV_SERVER_FROM_WINDOW(pf);

	if (priv->include_wm_functions) {
		register_wm_function(priv, "olwm_close", "Close", 
									srv, "_OL_FUNC_CLOSE");
		register_wm_function(priv, "olwm_fullsize", "Full Size", 
									srv, "_OL_FUNC_FULLSIZE");
		register_wm_function(priv, "olwm_props", "Properties", 
									srv, "_OL_FUNC_PROPS");
		register_wm_function(priv, "olwm_back", "Back", 
									srv, "_OL_FUNC_BACK");
		register_wm_function(priv, "olwm_refresh", "Refresh", 
									srv, "_OL_FUNC_REFRESH");
		register_wm_function(priv, "olwm_quit", "Quit", 
									srv, "_OL_FUNC_QUIT");
	}

	for (cnt = 2, reg = priv->registered; reg; cnt++, reg = reg->next);

	priv->pairs = xv_alloc_n(Permprop_res_enum_pair, cnt);

	for (i = 0, reg = priv->registered; reg; reg = reg->next, ++i) {
		priv->pairs[i].value = reg->enum_value;
		priv->pairs[i].name = reg->code;
	}

	/* default */
	priv->pairs[i].value = 0;
	priv->pairs[i].name = (char *)0;

	xv_set(fk,
		PERMLIST_ADD_RESOURCE, "function",
								(long)DAP_enum,
								FDOFF(enum_val),
								priv->pairs,
		NULL);

	priv->installed = TRUE;
}

static void cleanup_reg(Func_reg_t r)
{
	if (r->description) xv_free(r->description);
	if (r->code) xv_free(r->code);

	xv_free(r);
}

static void register_new(Funckey_private *priv, Attr_avlist avlist)
{
	Func_reg_t nr, reg, last;
	Attr_attribute *attrs;
	Panel_item b;
	Menu_item it;
	char *lab;

	nr = xv_alloc(struct _func);

	for (attrs=avlist; *attrs; attrs=attr_next(attrs)) switch ((int)*attrs) {
		case FUNCKEY_DESCRIPTION:
			if (A1) {
				if (nr->description) xv_free(nr->description);
				nr->description = xv_strsave((char *)A1);
			}
			ADONE;

		case FUNCKEY_CODE:
			nr->code = xv_strsave((char *)A1);
			if (priv->installed) {
				char buf[200];

				sprintf(buf,
					XV_MSG("new code '%s' after FUNCKEY_INSTALL:\nToo late!"),
						nr->code);
				xv_error(FKPUB(priv),
					ERROR_PKG, FUNCTION_KEYS,
					ERROR_LAYER, ERROR_PROGRAM,
					ERROR_STRING, buf,
					ERROR_SEVERITY, ERROR_RECOVERABLE,
					NULL);
			}
			ADONE;

		case FUNCKEY_PROC:
			nr->cbtype = GENERIC;
			nr->u.g.cb = (funckey_pfunction_t)A1;
			nr->u.g.client_data = (Xv_opaque)A2;
			ADONE;

		case FUNCKEY_FALL_THROUGH:
			/* this can be used for those cases where a function key
			 * is used via the .textswrc by a TEXTSW.
			 * We want to be able to label it via the Function Keys window,
			 * but do NOT want to handle it here.
			 */
			nr->cbtype = FALL_THROUGH;
			ADONE;

		case FUNCKEY_PANEL_BUTTON:
			b = (Panel_item)A1;
			lab = (char *)xv_get(b, PANEL_LABEL_STRING);
			nr->description = xv_strsave(lab);
			nr->cbtype = BUTTON_LIKE;
			nr->u.p.panel_item = b;
			ADONE;

		case FUNCKEY_MENU_ITEM:
			it = (Menu_item)A2;
			if ((lab = (char *)xv_get(it, MENU_STRING))) {
				nr->description = xv_strsave(lab);
			}
			nr->cbtype = MENU_LIKE;
			nr->u.m.menu = (Menu)A1;
			nr->u.m.menu_item = it;
			ADONE;

		default:
			xv_check_bad_attr(FUNCTION_KEYS, A0);
			break;
	}

	if (! nr->code) {
		xv_error(FKPUB(priv),
			ERROR_PKG, FUNCTION_KEYS,
			ERROR_LAYER, ERROR_PROGRAM,
			ERROR_STRING, XV_MSG("incomplete registration, need a code (attr FUNCKEY_CODE)"),
			ERROR_SEVERITY, ERROR_RECOVERABLE,
			NULL);

		cleanup_reg(nr);
		return;
	}

	last = (Func_reg_t)0;
	for (reg = priv->registered; reg; reg = reg->next) {
		if (! strcmp(nr->code, reg->code)) {
			char errstr[200];

			/* same key means same functionality and same accelerator -
			 * but XView does not allow two menu items to have the same
			 * accelerators
			 */
			sprintf(errstr, XV_MSG("FUNCKEY_CODE '%s' seen twice"), nr->code);
			xv_error(FKPUB(priv),
					ERROR_PKG, FUNCTION_KEYS,
					ERROR_LAYER, ERROR_PROGRAM,
					ERROR_STRING, errstr,
					ERROR_SEVERITY, ERROR_RECOVERABLE,
					NULL);
			cleanup_reg(nr);
			return;
		}
		last = reg;
	}

	if (! nr->description) {
		xv_error(FKPUB(priv),
			ERROR_PKG, FUNCTION_KEYS,
			ERROR_LAYER, ERROR_PROGRAM,
			ERROR_STRING, XV_MSG("incomplete registration, need a\ndescription (attr FUNCKEY_DESCRIPTION)"),
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

static void register_wm_function(Funckey_private *priv, char *code,
								char *descr, Xv_server srv, const char *func)
{
	Func_reg_t nr, reg, last;

	nr = xv_alloc(struct _func);
	nr->description = xv_strsave((char *)descr);
	nr->code = xv_strsave((char *)code);
	nr->cbtype = OLWM;
	nr->u.o = (Atom)xv_get(srv, SERVER_ATOM, func);

	last = (Func_reg_t)0;
	for (reg = priv->registered; reg; reg = reg->next) {
		last = reg;
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

static void from_menu(Function_keys self, Menu menu)
{
	int i;
	Xv_opaque menuproc = xv_get(menu, MENU_NOTIFY_PROC); /* only comparisons */

	for (i = xv_get(menu, MENU_NITEMS); i >= 1; i--) {
		Menu_item it = xv_get(menu, MENU_NTH_ITEM, i);
		Xv_opaque itemproc = xv_get(it, MENU_NOTIFY_PROC);

		if (menuproc != XV_NULL || itemproc != XV_NULL) {
			char *code = (char *)xv_get(it, XV_KEY_DATA, FUNCKEY_CODE);

			if (code && *code) {
				char *desc= (char *)xv_get(it,XV_KEY_DATA,FUNCKEY_DESCRIPTION);

				if (! desc || ! *desc) 
					desc = (char *)xv_get(it, MENU_STRING);

				if (desc && *desc) {
					xv_set(self,
						FUNCKEY_REGISTER,
							FUNCKEY_MENU_ITEM, menu, it,
							FUNCKEY_CODE, code,
							FUNCKEY_DESCRIPTION, desc,
							NULL,
						NULL);
				}
			}
		}
	}
}

static void install_owner(Function_keys self, Funckey_private *priv)
{
	Frame owner = xv_get(self, XV_OWNER);

	if (owner) {
		Funckey_private *other =
					(Funckey_private *)xv_get(owner, OWNER_PRIV_LIST);

		if (! other) {
			/* I'm the first (maybe the only) one */
			priv->appl_apply = (permprop_cb_t)xv_get(owner,
											FRAME_PROPS_APPLY_PROC);
			xv_set(owner, FRAME_PROPS_APPLY_PROC, note_apply, NULL);
		}
		priv->next_instance = other;
		xv_set(owner, OWNER_PRIV_LIST, priv, NULL);
	}
}

static int funckeys_init(Perm_prop_frame owner, Function_keys slf,
							Attr_avlist avlist, int *u)
{
	Xv_funckeys *self = (Xv_funckeys *)slf;
	Funckey_private *priv;
	Attr_attribute *attrs;
	Attr_attribute ownattrs[20];
	size_t itemsize = sizeof(Funckey_descr_t);
	int ai = 0;
	Xv_server srv;

	if (! fkey_key) fkey_key = xv_unique_key();

	if (owner) {
		if (! xv_get(owner, XV_IS_SUBTYPE_OF, PERMANENT_PROPS)) {
			xv_error(slf,
					ERROR_PKG, FUNCTION_KEYS,
					ERROR_LAYER, ERROR_PROGRAM,
					ERROR_STRING, XV_MSG("owner must be a PERMANENT_PROPS"),
					ERROR_SEVERITY, ERROR_NON_RECOVERABLE,
					NULL);
			return XV_ERROR;
		}
		srv = XV_SERVER_FROM_WINDOW(owner);
	}
	else {
		srv = xv_default_server;
	}
	xv_set(srv, XV_KEY_DATA, fkey_key, slf, NULL);

	priv = xv_alloc(Funckey_private);
	if (!priv) return XV_ERROR;

	priv->public_self = slf;
	self->private_data = (Xv_opaque)priv;
	priv->include_wm_functions = TRUE;

	for (attrs=avlist; *attrs; attrs=attr_next(attrs)) switch ((int)*attrs) {
		case PROPLIST_ITEM_DATA_SIZE:
			itemsize = (size_t)A1;
			ADONE;
	}

	ownattrs[ai++] = (Attr_attribute)PROPLIST_VERIFY_PROC;
	ownattrs[ai++] = (Attr_attribute)verify_func_descr;
	ownattrs[ai++] = (Attr_attribute)PROPLIST_FREE_ITEM_PROC;
	ownattrs[ai++] = (Attr_attribute)free_func_descr;
	ownattrs[ai++] = (Attr_attribute)PROPLIST_COPY_PROC;
	ownattrs[ai++] = (Attr_attribute)copy_func_descr;
	ownattrs[ai++] = (Attr_attribute)PROPLIST_ITEM_DATA_SIZE;
	ownattrs[ai++] = (Attr_attribute)itemsize;
	ownattrs[ai++] = (Attr_attribute)0;
	xv_super_set_avlist(slf, FUNCTION_KEYS, ownattrs);

	xv_set(slf,
			PROPLIST_ENTRY_STRING_OFFSET, FDOFF(listlabel),
			PERMLIST_ITEM_RESOURCE_NAME, "funckeydesc",
			PERMLIST_RESOURCE_CATEGORY, PRC_D,
			PERMLIST_ADD_RESOURCE, "fkey", (long)DAP_int, FDOFF(fkeynumber), 0L,
			PERMLIST_ADD_RESOURCE, "modifiers", (long)DAP_int, FDOFF(mods), 0L,
			PERMLIST_ADD_RESOURCE, "label", DAP_string, FDOFF(label), "",
			NULL);

	return XV_OK;
}

static Xv_opaque funckeys_set(Function_keys self, Attr_avlist avlist)
{
	Attr_attribute *attrs;
	Funckey_private *priv = FKPRIV(self);

	for (attrs=avlist; *attrs; attrs=attr_next(attrs)) switch ((int)*attrs) {
		case PROPLIST_VERIFY_PROC:
			priv->verify_item_func = (verify_func_t)A1;
			ADONE;

		case PERMLIST_BEFORE_RESET_FACTORY:
		case PERMLIST_BEFORE_RESET:
			if (! priv->installed) {
				/* we don't really need FUNCKEY_INSTALL any longer... */
				do_install(priv);
			}
			improve_layout(priv);
			ADONE;

		case PROPLIST_FREE_ITEM_PROC:
			priv->free_item_func = (free_func_t)A1;
			ADONE;

		case PROPLIST_COPY_PROC:
			priv->copy_item_func = (copy_func_t)A1;
			ADONE;

		case FUNCKEY_UPDATE_LABELS:
			update_func_key_labels(priv);
			ADONE;

		case FUNCKEY_SOFT_KEY_WINDOWS:
			soft_key_windows(priv, (Xv_window *)&A1);
			improve_layout(priv);
			ADONE;

		case FUNCKEY_ADD_SOFT_KEY_WINDOW:
			add_soft_key_window(priv, (Xv_window)A1, (int)A2);
			ADONE;

		case FUNCKEY_INSTALL:  /* should be done before the first 'reset' */
			do_install(priv);
			improve_layout(priv);
			ADONE;

		case FUNCKEY_CREATE_TOP:
			create_top(priv, attrs + 1);
			ADONE;

		case PERMLIST_CREATE_TOP:
			priv->list_created = TRUE;
			/* let the superclass do its work */
			break;

		case PERMLIST_CREATE_BOTTOM:
			priv->state = LAID_OUT;
			/* let the superclass do its work */
			break;

		case PERMLIST_OWNER_SET:
			install_owner(self, priv);
			/* let the superclass do its work */
			break;

		case FUNCKEY_REGISTER:
			register_new(priv, attrs + 1);
			ADONE;

		case FUNCKEY_MENU:
			from_menu(self, (Menu)A1);
			ADONE;

		case FUNCKEY_EVENT_CALLBACK:
			priv->event_cb = (funckey_event_cb_t)A1;
			ADONE;

		case FUNCKEY_INCLUDE_WM_FUNCTIONS:
			priv->include_wm_functions = (int)A1;
			ADONE;

		case XV_END_CREATE:
			install_owner(self, priv);
			break;

		default: xv_check_bad_attr(FUNCTION_KEYS, A0);
			break;
	}

	return XV_OK;
}

/*ARGSUSED*/
static Xv_opaque funckeys_get(Function_keys self, int *status, Attr_attribute attr, va_list vali)
{
	Funckey_private *priv = FKPRIV(self);
	Event *ev;

	*status = XV_OK;
	switch ((int)attr) {
		case PROPLIST_VERIFY_PROC: return (Xv_opaque)priv->verify_item_func;
		case PROPLIST_FREE_ITEM_PROC: return (Xv_opaque)priv->free_item_func;
		case PROPLIST_COPY_PROC: return (Xv_opaque)priv->copy_item_func;

		case FUNCKEY_COMMAND_ITEM: return priv->func_choice;
		case FUNCKEY_EVENT_CALLBACK: return (Xv_opaque)priv->event_cb;

		case FUNCKEY_HANDLE_EVENT:                 /*    NEW    */
			ev = va_arg(vali, Event *);
			return (Xv_opaque)process_function_key(priv, ev);

		case FUNCKEY_INCLUDE_WM_FUNCTIONS:
			return (Xv_opaque)priv->include_wm_functions;

		default:
			*status = xv_check_bad_attr(FUNCTION_KEYS, attr);
			return (Xv_opaque)XV_OK;
	}
}

static int funckeys_destroy(Function_keys self, Destroy_status status)
{
	if (status == DESTROY_CLEANUP) {
		Funckey_private *priv = FKPRIV(self);
		Func_reg_t reg, next = NULL;

		for (reg = priv->registered; reg; reg = next) {
			next = reg->next;
			cleanup_reg(reg);
		}
		if (priv->softkeywins) xv_free(priv->softkeywins);
		if (priv->pairs) xv_free(priv->pairs);
		xv_free(priv);
	}
	return XV_OK;
}

Function_keys xv_function_keys_from_server(Xv_server srv)
{
	if (fkey_key == 0) return XV_NULL;

	return xv_get(srv, XV_KEY_DATA, fkey_key);
}

const Xv_pkg xv_funckeys_pkg = {
	"FunctionKeys",
	ATTR_PKG_FUNCTION_KEYS,
	sizeof(Xv_funckeys),
	PERMANENT_LIST,
	funckeys_init,
	funckeys_set,
	funckeys_get,
	funckeys_destroy,
	NULL
};
