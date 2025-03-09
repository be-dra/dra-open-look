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
#include <ctype.h>
#include <xview_private/i18n_impl.h>
#include <xview/shortcut.h>

char shortcut_c_sccsid[] = "@(#) %M% V%I% %E% %U% $Id: shortcut.c,v 4.12 2025/03/08 13:37:48 dra Exp $";

typedef struct {
	char *code;
	int indx;
} funclabel_t;

typedef struct {
	char *listlabel;
	char *character;
	int mods;
	funclabel_t func;
} Shortcut_descr_t;
#define FDOFF(field) FP_OFF(Shortcut_descr_t *,field)

typedef void (*free_func_t)(Shortcuts, Shortcut_descr_t *fd);
typedef void (*copy_func_t)(Shortcuts, Shortcut_descr_t *src,
									Shortcut_descr_t *target);
typedef int (*verify_func_t)(Shortcuts, Xv_opaque data,
									Proplist_verify_op op, Xv_opaque orig);
typedef void (*shortcut_pfunction_t)(Xv_opaque, Event *, Xv_opaque);
/* can be used with a MENU_NOTIFY_PROC: */
typedef void (*shortcut_mfunction_t)(Xv_opaque, Xv_opaque, Xv_opaque);

typedef enum {
	SHIFT_BIT,
	CTRL_BIT,
	META_BIT,
	ALT_BIT
} mod_bits_t;

/* An application works similar to the following:
	++  create a SHORTCUTS [[ this can be created with an XV_NULL owner,
								see set and get methods for attr XV_OWNER
								in permlist.c ]]
	++  register menus and buttons using SHORTCUT_REGISTER
	++  if you want to use it in a property window or (always) want to read
		the saved settings (using PERM_RESET_FROM_DB etc), do
			· set XV_OWNER (unless already done)
			· call SHORTCUT_CREATE_TOP
			· call PERMLIST_CREATE_BOTTOM
	++  in your own event handlers call
		event_has_been_handled = (int)xv_get(sc, SHORTCUT_HANDLE_EVENT, ev)
*/

typedef enum {
	BUTTON_LIKE,
	MENUITEM_LIKE
} sc_cb_type_t;

/* linked list that describes one available function */
typedef struct _shortc {
	struct _shortc *next;
	char *label;
	char *code;

	sc_cb_type_t cbtype;
	union {
		struct {
			Xv_opaque panel_item; /* or similar.... */
		} p;
		struct {
			Xv_opaque menu;
			Xv_opaque menu_item;
		} m;
	} u;
} *Short_reg_t;


typedef struct _Shortcut_private {
	Xv_opaque           public_self;
	free_func_t         free_item_func;
	copy_func_t         copy_item_func;
	verify_func_t       verify_item_func;
	Panel_item          func_choice;
	Short_reg_t          registered;
	int list_created;
	char *resource_manager;
} Shortcut_private;

#define A0 *attrs
#define A1 attrs[1]
#define A2 attrs[2]

#define ADONE ATTR_CONSUME(*attrs);break

#define SCPRIV(_x_) XV_PRIVATE(Shortcut_private, Xv_shortcuts, _x_)
#define SCPUB(_x_) XV_PUBLIC(_x_)

static char *modstr[] = {
	"                  ",
	"+Shift         ",
	"+Ctrl          ",
	"+Shift+Ctrl",
	"+Meta           ",
	"+Shift+Meta     ",
	"+Ctrl+Meta      ",
	"+Shift+Ctrl+Meta",
	"+Alt              ",
	"+Shift+Alt     ",
	"+Ctrl+Alt      ",
	"+Shift+Ctrl+Alt ",
	"+Meta+Alt       ",
	"+Shift+Meta+Alt ",
	"+Ctrl+Meta+Alt  ",
	"+Shift+Ctrl+Meta+Alt",
};

static Attr_attribute sc_key = 0;

static void free_sc_descr(Shortcuts self, Shortcut_descr_t *fd)
{
	Shortcut_private *priv = SCPRIV(self);

	if (fd->listlabel) xv_free(fd->listlabel);
	if (fd->func.code) xv_free(fd->func.code);
	if (fd->character) xv_free(fd->character);

	if (priv->free_item_func)
		(*(priv->free_item_func))(self, fd);
}

static char *codify(char *lab)
{
	char *p, *t;
	static char buffer[200];

	t = buffer;
	for (p = lab; *p; p++) {
		switch (*p) {
			case ' ': *t++ = 'B'; break;
			case '@': *t++ = 'K'; break;
			case '[': *t++ = 'E'; break;
			case ']': *t++ = 'F'; break;
			case '{': *t++ = 'W'; break;
			case '}': *t++ = 'X'; break;
			case '(': *t++ = 'R'; break;
			case ')': *t++ = 'S'; break;
			case '\'': *t++ = 'N'; break;
			case '`': *t++ = 'O'; break;
			case '-': *t++ = 'M'; break;
			default:
				if (isalnum(*p)) *t++ = *p;
				break;
		}
	}
	*t = '\0';
	return buffer;
}

static int code_to_index(Panel_item it, const char *code)
{
	int i, n = (int)xv_get(it, PANEL_NCHOICES);

	for (i = 0; i < n; i++) {
		char *lab = (char *)xv_get(it, PANEL_CHOICE_STRING, i);
		char *cod = codify(lab);

		if (0 == strcmp(cod, code)) {
			return i;
		}
	}
	return 0;
}

static void copy_sc_descr(Shortcuts self, Shortcut_descr_t *src,
										Shortcut_descr_t *target)
{
	Shortcut_private *priv = SCPRIV(self);

	if (target->listlabel) xv_free(target->listlabel);
	if (target->func.code) xv_free(target->func.code);
	if (target->character) xv_free(target->character);
	if (priv->free_item_func)
		(*(priv->free_item_func))(self, target);

	memcpy((char *)target, (char *)src,
				xv_get(SCPUB(priv), PROPLIST_ITEM_DATA_SIZE));

	target->func.indx = code_to_index(priv->func_choice, target->func.code);
	target->listlabel = xv_strsave(target->listlabel ? target->listlabel : "");
	target->func.code = xv_strsave(target->func.code ? target->func.code : "");
	target->character =
				xv_strsave(target->character?target->character:"");

	if (priv->copy_item_func)
		(*(priv->copy_item_func))(self, src, target);
}

/* check whether the shortcut (= key/modifier combination) is already
 * a 'global resource' (= contained in the RESOURCE_MANAGER string)
 */
static int is_a_resource(Shortcut_private *priv, Frame fram,
								Shortcut_descr_t *fd)
{
	Display *dpy = (Display *)xv_get(fram, XV_DISPLAY);
	char *p, *line;

	if (! priv->resource_manager) {
		priv->resource_manager = XResourceManagerString(dpy);
	}

	line = priv->resource_manager;
	do {
		p = strchr(line, '\n');
		if (p) {
			char *colon, curline[1000];

			*p = '\0';
			strcpy(curline, line);
			colon = strchr(curline, ':');
			if (colon) {
				char *no_comma[10];
				int i, k = 0;
				char *q;
				int mods = 0;

				++colon;
				for (q = strtok(colon, " \t,"); q; q = strtok(NULL, " \t,")) {
					no_comma[k++] = q;
				}

				for (i = 0; i < k; i++) {
					char ch[10];

					ch[0] = '\0';
					for (q = strtok(no_comma[i],"+"); q; q = strtok(NULL,"+")) {
						if (0 == strcmp(q, "Shift")) mods |= (1<< SHIFT_BIT);
						else if (0 == strcmp(q, "Ctrl")) mods |= (1<< CTRL_BIT);
						else if (0 == strcmp(q, "Meta")) mods |= (1<< META_BIT);
						else if (0 == strcmp(q, "Alt")) mods |= (1<< ALT_BIT);
						else strncpy(ch, q, 9L);
					}

					if (mods!=0 && ch[1]=='\0' && ch[0]==fd->character[0]) {
						if (mods == fd->mods) {
							char *z, errbuf[400];

							sprintf(errbuf, "already in use: %s", line);
							if ((z = strchr(errbuf, '\t'))) *z = ' ';
							if ((z = strchr(errbuf, '\t'))) *z = ' ';
							if ((z = strchr(errbuf, '\t'))) *z = ' ';
							xv_set(fram, 
									FRAME_LEFT_FOOTER, XV_MSG(errbuf),
									WIN_ALARM,
									NULL);
							return TRUE;
						}
						else {
							fprintf(stderr, "%s-%d: similar: %s\n",
												__FILE__,__LINE__, line);
						}
					}
				}
			}
			*p = '\n';
			line = p + 1;
		}
		else {
			line = NULL;
		}
	} while (line);

	return FALSE;
}

static int verify_sc_descr(Shortcuts self, Xv_opaque data,
						Proplist_verify_op op, Xv_opaque orig)
{
	Shortcut_private *priv = SCPRIV(self);
	int idx;
	char *lab, buf[1000];
	Shortcut_descr_t *fd = (Shortcut_descr_t *)data;
	Shortcut_descr_t *origfd = (Shortcut_descr_t *)orig;

	switch (op) {
		case PROPLIST_APPLY:
		case PROPLIST_INSERT:
			{
				int i, num;
				Property_list l = xv_get(self, PERMLIST_PROP_MASTER);

				num = (int)xv_get(l, PANEL_LIST_NROWS);
				for (i = 0; i < num; i++) {
					Shortcut_descr_t *tmp;

					tmp = (Shortcut_descr_t *)xv_get(l,
												PANEL_LIST_CLIENT_DATA, i);
					if (tmp && tmp != fd && tmp != origfd) {
						if (0 == strcmp(tmp->character, fd->character) &&
							tmp->mods == fd->mods)
						{
							xv_set(xv_get(SCPUB(priv), XV_OWNER),
								FRAME_LEFT_FOOTER,
									XV_MSG("key/modifier combination already in list"),
								WIN_ALARM,
								NULL);

							return XV_ERROR;
						}
					}
				}
			}
			if ((fd->mods & 14) == 0) {
				xv_set(xv_get(SCPUB(priv), XV_OWNER),
					FRAME_LEFT_FOOTER,
						XV_MSG("Unmodified characters not accepted as shortcuts"),
					WIN_ALARM,
					NULL);
				return XV_ERROR;
			}
			if (is_a_resource(priv, xv_get(SCPUB(priv), XV_OWNER), fd)) {
				return XV_ERROR;
			}
			/* everything seems ok, now fall through to the CONVERT case
			 * to supply the list label
			 */

		case PROPLIST_CONVERT:
			idx = fd->func.indx;
			lab = (char *)xv_get(priv->func_choice, PANEL_CHOICE_STRING, idx);
			sprintf(buf, "%s%s           %s",
						fd->character, modstr[fd->mods], lab);
			if (fd->listlabel) xv_free(fd->listlabel);
			fd->listlabel = xv_strsave(buf);
			break;

		case PROPLIST_USE_IN_PANEL:
			xv_set(priv->func_choice, PANEL_VALUE, fd->func.indx, NULL);
			break;

		default:
			break;
	}

	if (priv->verify_item_func)
		return (*(priv->verify_item_func))(self, data, op, orig);

	return XV_OK;
}

static char *make_help(Shortcut_private *priv, char *str)
{
	char *myhelp, *itemhelp;

	myhelp = (char *)xv_get(SCPUB(priv), XV_HELP_DATA);
	if (! myhelp) return (char *)0;

	itemhelp = xv_malloc(strlen(myhelp) + strlen(str) + 2);
	sprintf(itemhelp, "%s_%s", myhelp, str);
	return itemhelp;
}

static void free_help_data(Xv_opaque obj, int key, char *data)
{
	if (key == XV_HELP && data) xv_free(data);
}

static void convert_func(int val, int panel_to_data, funclabel_t *datptr,
								Panel_item it, Xv_opaque unused)
{
	if (panel_to_data) {
		char *lab;

		datptr->indx = val;
		lab = (char *)xv_get(it, PANEL_CHOICE_STRING, val);
		datptr->code = strdup(codify(lab));
	}
	else {
		datptr->indx = code_to_index(it, datptr->code);
	}
}

static void create_top(Shortcut_private *priv, Attr_attribute *avlist)
{
	Short_reg_t reg;
	Shortcuts fk = SCPUB(priv);
	Property_list list;
	Panel_item fkc;
	int i;
	Panel pan;
	Menu menu;

	if (! priv->list_created) {
		int i = 0;
		Attr_attribute av[30];

		av[i++] = (Attr_attribute)PERMLIST_CREATE_TOP;
		av[i++] = (Attr_attribute)PANEL_LABEL_STRING;
		av[i++] = (Attr_attribute)XV_MSG("Shortcuts:");
		av[i++] = (Attr_attribute)PANEL_LIST_DISPLAY_ROWS;
		av[i++] = (Attr_attribute)7;
		av[i++] = (Attr_attribute)PANEL_LIST_WIDTH;
		av[i++] = (Attr_attribute)300;
		av[i++] = (Attr_attribute)0;
		av[i++] = (Attr_attribute)0;
		av[i++] = (Attr_attribute)0;
		xv_super_set_avlist(fk, SHORTCUTS, av);
	}

	list = xv_get(fk, PERMLIST_PROP_MASTER);

	xv_set(xv_get(fk, XV_OWNER),
				FRAME_PROPS_CREATE_ITEM,
					FRAME_PROPS_ITEM_SPEC, &fkc, -1, PANEL_TEXT,
					PANEL_LABEL_STRING, XV_MSG("Key:"),
					PANEL_VALUE_DISPLAY_LENGTH, 3,
					PANEL_VALUE_STORED_LENGTH, 1,
					FRAME_PROPS_DATA_OFFSET, FDOFF(character),
					FRAME_PROPS_SLAVE_OF, list,
					XV_HELP_DATA, make_help(priv, "key"),
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
						XV_MSG("Alt"),
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

	if (! priv->registered) return;

	xv_set(xv_get(fk, XV_OWNER),
				FRAME_PROPS_CREATE_ITEM,
					FRAME_PROPS_ITEM_SPEC, &priv->func_choice, -1, PANEL_CHOICE,
					ATTR_LIST, avlist,
					PANEL_LABEL_STRING, XV_MSG("Function:"),
					PANEL_DISPLAY_LEVEL, PANEL_CURRENT,
					XV_HELP_DATA, make_help(priv, "sc_function"),
					XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
					FRAME_PROPS_DATA_OFFSET, FDOFF(func),
					FRAME_PROPS_CONVERTER, convert_func, XV_NULL,
					FRAME_PROPS_SLAVE_OF, list,
					NULL,
				NULL);

	for (i = 0, reg = priv->registered; reg; reg = reg->next, i++) {
		xv_set(priv->func_choice,
				PANEL_CHOICE_STRING, i, reg->label,
				NULL);
	}

	menu = xv_get(priv->func_choice, PANEL_ITEM_MENU);
	if (menu) {
		for (i = 0, reg = priv->registered; reg; reg = reg->next, i++) {
			Menu_item item = xv_get(menu, MENU_NTH_ITEM, i + 1);

			if (item) {
				char helpbuf[100];

				sprintf(helpbuf, "sc_%s", reg->code);
				xv_set(item,
						XV_HELP_DATA, make_help(priv, helpbuf),
						XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
						NULL);
			}
		}
	}
}

static int process_key(Shortcuts self, Shortcut_private *priv, Event *ev)
{
	int i, mod = 0;
	char *addr, *ptr;
	Proplist_contents *clcont;
	int itemsize = (int)xv_get(self, PROPLIST_ITEM_DATA_SIZE);
	int dataoff = (int)xv_get(self, FRAME_PROPS_DATA_OFFSET);
	Perm_prop_frame pf;
	XEvent *xev = event_xevent(ev);
	char charac[2];

	/* we want to handle nothing but KeyRelease events: */
	if (event_is_up(ev)) return FALSE;
	if (! xev) return FALSE;
	if (xev->type != KeyPress) return FALSE;

	charac[0] = event_id(ev);
	charac[1] = '\0';

	pf = xv_get(self, XV_OWNER);
	addr = (char *)xv_get(pf, FRAME_PROPS_DATA_ADDRESS);
	clcont = (Proplist_contents *)(addr + dataoff);
	ptr = (char *)clcont->item_data;

	if (event_shift_is_down(ev)) mod |= (1 << SHIFT_BIT);
	if (event_ctrl_is_down(ev)) mod |= (1 << CTRL_BIT);
	if (event_meta_is_down(ev)) mod |= (1 << META_BIT);
	if (event_alt_is_down(ev)) mod |= (1 << ALT_BIT);

	for (i = 0; i < clcont->num_items; i++) {
		Shortcut_descr_t *fkd = (Shortcut_descr_t *)(ptr + itemsize * i);

		if (0 == strcmp(charac, fkd->character) && mod==fkd->mods) {
			Short_reg_t reg;

			for (reg = priv->registered; reg; reg = reg->next) {
				if (0 == strcmp(reg->code, fkd->func.code)) {
					switch (reg->cbtype) {
						case BUTTON_LIKE:
							{
								Panel_item b = reg->u.p.panel_item;
								shortcut_pfunction_t cb;

								cb = (shortcut_pfunction_t)xv_get(b, 
														PANEL_NOTIFY_PROC);

								(*cb)(b, ev, (Xv_opaque)fkd);
							}
							break;
						case MENUITEM_LIKE:
							{
								Menu menu = reg->u.m.menu;
								Menu_item item = reg->u.m.menu_item;
								shortcut_mfunction_t cb;

								cb = (shortcut_mfunction_t)xv_get(item, 
														MENU_NOTIFY_PROC);
								if (! cb) {
									cb = (shortcut_mfunction_t)xv_get(menu, 
														MENU_NOTIFY_PROC);
								}
								if (cb) {
									(*cb)(menu, item, (Xv_opaque)fkd);
								}
							}
							break;
					}
					return TRUE;
				}
			}
			return TRUE;
		}
	}
	return FALSE;
}

static void cleanup_reg(Short_reg_t r)
{
	if (r->label) xv_free(r->label);
	if (r->code) xv_free(r->code);

	xv_free(r);
}

static void link_to_end(Short_reg_t list, Short_reg_t nr)
{
	Short_reg_t r, last = NULL;

	for (r = list; r; r = r->next) {
		last = r;
	}

	last->next = nr;
}

static void register_new(Shortcut_private *priv, Xv_opaque obj,
							 const char *pref)
{
	Short_reg_t nr = NULL, r;

	if (xv_get(obj, XV_IS_SUBTYPE_OF, MENU_COMMAND_MENU)) {
		int i, num = (int)xv_get(obj, MENU_NITEMS);

		for (i = 1; i <= num; i++) {
			Menu_item it = xv_get(obj, MENU_NTH_ITEM, i);
			Menu submenu = xv_get(it, MENU_PULLRIGHT);
			char menubuf[100];

			if (submenu) {
				int avoid = (int)xv_get(it, XV_KEY_DATA,SHORTCUT_AVOID_SUBMENU);
				/* recursive */

				if (avoid) continue;
				sprintf(menubuf, "%s ", (char *)xv_get(it, MENU_STRING));
				register_new(priv, submenu, menubuf);
			}
			else {
				char *lab = (char *)xv_get(it, MENU_STRING);

				if (lab && *lab) {
					nr = xv_alloc(struct _shortc);

					nr->next = NULL;
					sprintf(menubuf, "%s%s", pref, lab);
					nr->label = strdup(menubuf);
					nr->code = strdup(codify(nr->label));

					nr->cbtype = MENUITEM_LIKE;
					nr->u.m.menu = obj;
					nr->u.m.menu_item = it;

					if (priv->registered) link_to_end(priv->registered, nr);
					else priv->registered = nr;
				}
			}
		}
	}
	else if (xv_get(obj, XV_IS_SUBTYPE_OF, PANEL_BUTTON)) {
		nr = xv_alloc(struct _shortc);

		nr->next = NULL;
		nr->label = strdup((char *)xv_get(obj, PANEL_LABEL_STRING));
		nr->code = strdup(codify(nr->label));

		nr->cbtype = BUTTON_LIKE;
		nr->u.p.panel_item = obj;

		if (priv->registered) link_to_end(priv->registered, nr);
		else priv->registered = nr;
	}

	if (!nr) return;
	for (r = priv->registered; r; r = r->next) {
		if (r != nr) {
			if (! strcmp(nr->code, r->code)) {
				char errstr[200];

				/* same key means same functionality and same accelerator -
				 * but XView does not allow two menu items to have the same
				 * accelerators
				 */
				sprintf(errstr, "code '%s' seen twice", nr->code);
				fprintf(stderr, "shortcut.c: code '%s' seen twice", nr->code);
				xv_error(SCPUB(priv),
						ERROR_PKG, SHORTCUTS,
						ERROR_LAYER, ERROR_PROGRAM,
						ERROR_STRING, errstr,
						ERROR_SEVERITY, ERROR_RECOVERABLE,
						NULL);
				cleanup_reg(nr);
				return;
			}
		}
	}
}

static int shortcuts_init(Perm_prop_frame owner, Shortcuts slf,
							Attr_avlist avlist, int *u)
{
	Xv_shortcuts *self = (Xv_shortcuts *)slf;
	Shortcut_private *priv;
	Attr_attribute *attrs;
	Attr_attribute ownattrs[20];
	size_t itemsize = sizeof(Shortcut_descr_t);
	int ai = 0;
	Xv_server srv;

	if (! sc_key) sc_key = xv_unique_key();

	if (owner) {
		if (! xv_get(owner, XV_IS_SUBTYPE_OF, PERMANENT_PROPS)) {
			xv_error(slf,
					ERROR_PKG, SHORTCUTS,
					ERROR_LAYER, ERROR_PROGRAM,
					ERROR_STRING, "owner must be a PERMANENT_PROPS",
					ERROR_SEVERITY, ERROR_NON_RECOVERABLE,
					NULL);
			return XV_ERROR;
		}
		srv = XV_SERVER_FROM_WINDOW(owner);
	}
	else {
		srv = xv_default_server;
	}
	xv_set(srv, XV_KEY_DATA, sc_key, slf, NULL);

	priv = xv_alloc(Shortcut_private);
	if (!priv) return XV_ERROR;

	priv->public_self = slf;
	self->private_data = (Xv_opaque)priv;

	for (attrs=avlist; *attrs; attrs=attr_next(attrs)) switch ((int)*attrs) {
		case PROPLIST_ITEM_DATA_SIZE:
			itemsize = (size_t)A1;
			ADONE;
	}

	ownattrs[ai++] = (Attr_attribute)PROPLIST_VERIFY_PROC;
	ownattrs[ai++] = (Attr_attribute)verify_sc_descr;
	ownattrs[ai++] = (Attr_attribute)PROPLIST_FREE_ITEM_PROC;
	ownattrs[ai++] = (Attr_attribute)free_sc_descr;
	ownattrs[ai++] = (Attr_attribute)PROPLIST_COPY_PROC;
	ownattrs[ai++] = (Attr_attribute)copy_sc_descr;
	ownattrs[ai++] = (Attr_attribute)PROPLIST_ITEM_DATA_SIZE;
	ownattrs[ai++] = (Attr_attribute)itemsize;
	ownattrs[ai++] = (Attr_attribute)0;
	xv_super_set_avlist(slf, SHORTCUTS, ownattrs);

	xv_set(slf,
			PROPLIST_ENTRY_STRING_OFFSET, (long)FDOFF(listlabel),
			PERMLIST_ITEM_RESOURCE_NAME, "shortcutdesc",
			PERMLIST_RESOURCE_CATEGORY, (long)PRC_D,
			PERMLIST_ADD_RESOURCE, "character", DAP_string, FDOFF(character), 0,
			PERMLIST_ADD_RESOURCE, "modifiers", DAP_int, FDOFF(mods), 0,
			PERMLIST_ADD_RESOURCE, "code", DAP_string, FDOFF(func.code), "",
			NULL);

	return XV_OK;
}

static Xv_opaque shortcuts_set(Shortcuts self, Attr_avlist avlist)
{
	Attr_attribute *attrs;
	Shortcut_private *priv = SCPRIV(self);

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

		case SHORTCUT_CREATE_TOP:
			create_top(priv, attrs + 1);
			ADONE;

		case PERMLIST_CREATE_TOP:
			priv->list_created = TRUE;
			/* let the superclass do its work */
			break;

		case SHORTCUT_REGISTER:
			register_new(priv, (Xv_opaque)A1, "");
			ADONE;

		default: xv_check_bad_attr(SHORTCUTS, A0);
			break;
	}

	return XV_OK;
}

static Xv_opaque shortcuts_get(Shortcuts self, int *status, Attr_attribute attr, va_list vali)
{
	Shortcut_private *priv = SCPRIV(self);
	Event *ev;

	*status = XV_OK;
	switch ((int)attr) {
		case PROPLIST_VERIFY_PROC: return (Xv_opaque)priv->verify_item_func;
		case PROPLIST_FREE_ITEM_PROC: return (Xv_opaque)priv->free_item_func;
		case PROPLIST_COPY_PROC: return (Xv_opaque)priv->copy_item_func;

		case SHORTCUT_COMMAND_ITEM: return priv->func_choice;

		case SHORTCUT_HANDLE_EVENT:                 /*    NEW    */
			ev = va_arg(vali, Event *);
			return (Xv_opaque)process_key(self, priv, ev);

		default:
			*status = xv_check_bad_attr(SHORTCUTS, attr);
			return (Xv_opaque)XV_OK;
	}
}

static void free_reg(Short_reg_t r)
{
	if (! r) return;
	free_reg(r->next);
	cleanup_reg(r);
}

static int shortcuts_destroy(Shortcuts self, Destroy_status status)
{
	if (status == DESTROY_CLEANUP) {
		Shortcut_private *priv = SCPRIV(self);

		free_reg(priv->registered);
		xv_free(priv);
	}
	return XV_OK;
}

const Xv_pkg xv_shortcuts_pkg = {
	"Shortcuts",
	ATTR_PKG_FUNCTION_KEYS,
	sizeof(Xv_shortcuts),
	PERMANENT_LIST,
	shortcuts_init,
	shortcuts_set,
	shortcuts_get,
	shortcuts_destroy,
	NULL
};
