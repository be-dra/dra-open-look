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
#include <xview/xview.h>
#include <xview/panel.h>
#include <xview/fontprop.h>
#include <xview/permprop.h>
#include <xview_private/i18n_impl.h>

char fontprop_c_sccsid[] = "@(#) %M% V%I% %E% %U% $Id: fontprop.c,v 4.3 2024/12/05 06:29:35 dra Exp $";

#define SUBOFF(_field_) FP_OFF(fontprop_t *,_field_)

typedef void (*notify_proc_t)(Font_props, Fontprop_setting, fontprop_t *);

typedef struct {
	Xv_opaque           public_self;
	Frame_props          frame;
	Panel_item          preview, list, size, slant, style, name;
	int                 dataoff;
	notify_proc_t       proc;
	char                *fam_label, *name_label;
	char                name_read_only, scales_only, fixedwidth_only,
						need_both, scalable_only;
	Fontprop_setting    create[(int)FONTPROP_last];
	fontprop_t          cbfp;
} Fontprops_private;

#define A0 *attrs
#define A1 attrs[1]
#define A2 attrs[2]

#define ADONE ATTR_CONSUME(*attrs);break

#define FONTPROPSPRIV(_x_) XV_PRIVATE(Fontprops_private, Xv_fontprops, _x_)
#define FONTPROPSPUB(_x_) XV_PUBLIC(_x_)

static int fontpkey = 0;

static char *string_in_string(char *search, char *in_me)
{
	register size_t kl_len = strlen(search);
	register char *p = strchr(in_me, *search);

	while (p) {
		if (! strncmp(p, search, kl_len)) return p;
		p = strchr(p+1, *search);
	}
	return (char *)0;
}

static void convert_style(int val, int panel_to_data, char **datptr,
										Panel_item it)
{
	if (panel_to_data) {
		if (*datptr) xv_free(*datptr);

		*datptr = xv_strsave(val ? "bold" : "medium");
	}
	else {
		if (*datptr) {
			if (string_in_string("bold", *datptr) ||
				string_in_string("BOLD", *datptr) ||
				string_in_string("Bold", *datptr))
			{
				xv_set(it, PANEL_VALUE, TRUE, NULL);
				return;
			}
		}
		xv_set(it, PANEL_VALUE, FALSE, NULL);
	}
}

static void convert_slant(int val, int panel_to_data, char *datptr,
										Panel_item it)
{
	static char slants[] = { 'r', 'o', 'i' };

	if (panel_to_data) {
		*datptr = slants[val];
	}
	else {
		int i;

		val = 0;
		for (i = 0; i < sizeof(slants); i++) {
			if (*datptr == slants[i]) val = i;
		}
		xv_set(it, PANEL_VALUE, val, NULL);
	}
}

static int scales[] = { 100, 120, 140, 190 };

static void convert_scale_size(int val, int panel_to_data, fontprop_t *datptr,
									Panel_item it, Xv_opaque is_scale)
{
	if (is_scale) {
		if (panel_to_data) {
			datptr->scale = val;
			datptr->size = scales[val];
		}
		else {
			xv_set(it, PANEL_VALUE, datptr->scale, NULL);
		}
	}
	else {
		if (panel_to_data) datptr->size = val;
		else xv_set(it, PANEL_VALUE, datptr->size, NULL);
	}
}

static void make_font_name(Fontprops_private *priv, Fontprop_setting which)
{
	char buf[300];
	int sizeval;

	xv_set(priv->frame, FRAME_LEFT_FOOTER, "", NULL);

	if (priv->size) {
		sizeval = (int)xv_get(priv->size, PANEL_VALUE);
		if (priv->scales_only) {
			priv->cbfp.scale = sizeval;
			priv->cbfp.size = scales[sizeval];
		}
		else {
			priv->cbfp.size = sizeval;
		}
	}

	if (priv->list) {
		if (priv->cbfp.family) xv_free(priv->cbfp.family);
		priv->cbfp.family = xv_strsave((char *)xv_get(priv->list,
			PANEL_LIST_STRING, xv_get(priv->list, PANEL_LIST_FIRST_SELECTED)));
	}
	else priv->cbfp.family = xv_strsave("*-*");

	if (priv->style) {
		convert_style((int)xv_get(priv->style, PANEL_VALUE), TRUE,
						&priv->cbfp.style, priv->style);
	}
	else priv->cbfp.style = xv_strsave("medium");

	if (priv->slant) {
		convert_slant((int)xv_get(priv->slant, PANEL_VALUE), TRUE,
						&priv->cbfp.slant, priv->slant);
	}
	else priv->cbfp.slant = 'r';

	sprintf(buf, "-%s-%s-%c-*-*-*-%d-*-*-*-*-iso10646-1",
					priv->cbfp.family,
					priv->cbfp.style,
					priv->cbfp.slant,
					priv->cbfp.size);

	xv_set(priv->name, PANEL_VALUE, buf, NULL);
	xv_set(priv->frame, FRAME_PROPS_ITEM_CHANGED, priv->name, TRUE, NULL);

	if (priv->proc) {
		priv->cbfp.fontname = buf;
		(*(priv->proc))(FONTPROPSPUB(priv), which, &priv->cbfp);
	}
}

static void note_make_font_name(Panel_item item)
{
	Fontprops_private *priv = (Fontprops_private *)xv_get(item,
											XV_KEY_DATA, fontpkey);

	make_font_name(priv, (Fontprop_setting)xv_get(item, PANEL_CLIENT_DATA));
}

/* ARGSUSED */
static int note_list(Panel_item item, char *string, Xv_opaque cldt,
							Panel_list_op op, Event *ev, int rownum)
{
	xv_set(xv_get(xv_get(item, XV_OWNER), XV_OWNER), FRAME_LEFT_FOOTER, "", NULL);
	switch (op) {
		case PANEL_LIST_OP_DESELECT:
			break;
		case PANEL_LIST_OP_SELECT:
			xv_set(xv_get(xv_get(item, XV_OWNER), XV_OWNER),
					FRAME_PROPS_ITEM_CHANGED, item, TRUE,
					NULL);
			note_make_font_name(item);
			break;
		case PANEL_LIST_OP_DELETE:
		case PANEL_LIST_OP_VALIDATE:
			return XV_ERROR;
		default: break;
	}

	return XV_OK;
}

static void note_preview(Panel_item item)
{
	Fontprops_private *priv = (Fontprops_private *)xv_get(item,
											XV_KEY_DATA, fontpkey);
	Xv_font myfont;
	char *name;

	xv_set(priv->frame, FRAME_LEFT_FOOTER, "", NULL);
	xv_set(item, PANEL_NOTIFY_STATUS, XV_ERROR, NULL);
	name = (char *)xv_get(priv->name, PANEL_VALUE);
	if (!name || ! *name) {
		xv_set(priv->frame,
				FRAME_LEFT_FOOTER, XV_MSG("please enter a font name"),
				WIN_ALARM,
				NULL);

		return;
	}

	xv_set(priv->preview, PANEL_LABEL_STRING, "", NULL);
	server_appl_busy(priv->frame, TRUE, XV_NULL);
/* 	dra_set_xv_error_behaviour(TRUE, FALSE); */
	myfont = xv_find(priv->frame, FONT, FONT_NAME, name, NULL);
/* 	dra_set_xv_error_behaviour(FALSE, FALSE); */
	server_appl_busy(priv->frame, FALSE, XV_NULL);
	if (! myfont) {
		xv_set(priv->frame,
				FRAME_LEFT_FOOTER, XV_MSG("cannot load that font"),
				WIN_ALARM,
				NULL);

		return;
	}

	xv_set(priv->preview,
			PANEL_LABEL_FONT, myfont,
			PANEL_LABEL_STRING, XV_MSG("The Font will look like this"),
			NULL);
}

static void try_family(Display *dpy, Fontprops_private *priv, Panel_item list,	
										char *fmt)
{
	char foundryfam[200], name[1000];
	char *q, *p, **namelist;
	int i, cnt;

	sprintf(name, fmt, "*-*", "bold");
	namelist = XListFonts(dpy, name, 1000, &cnt);
	if (!namelist) return;

	for (i = 0; i < cnt; i++) {
		if (!(q = strtok(namelist[i], "-"))) continue;
		if (!(p = strtok((char *)0, "-"))) continue;
		sprintf(foundryfam, "%s-%s", q, p);

		if (priv->need_both) {
			char **medlist;
			int cc;

			sprintf(name, fmt, foundryfam, "medium");
			medlist = XListFonts(dpy, name, 1000, &cc);
			if (!medlist) continue;
			XFreeFontNames(medlist);
		}

		xv_set(list,
				PANEL_LIST_INSERT, i,
				PANEL_LIST_STRING, i, foundryfam,
/* 				PANEL_LIST_FONT, i, xv_get(xv_get(list,XV_OWNER),XV_FONT), */
				NULL);
	}

	XFreeFontNames(namelist);
}

static void find_families(Fontprops_private *priv, Panel_item list)
{
	Display *dpy = (Display *)xv_get(priv->frame, XV_DISPLAY);
	int i, cnt;
	char *p;

	if (priv->scalable_only) {
		if (priv->fixedwidth_only) {
			try_family(dpy, priv, list, "-%s-%s-r-*-*-0-0-*-*-m-*-iso10646-1");
			try_family(dpy, priv, list, "-%s-%s-r-*-*-0-0-*-*-c-*-iso10646-1");
		}
		else {
			try_family(dpy, priv, list, "-%s-%s-r-*-*-0-0-*-*-*-*-iso10646-1");
		}
	}
	else {
		if (priv->fixedwidth_only) {
			try_family(dpy, priv, list, "-%s-%s-r-*-*-*-120-*-*-m-*-iso10646-1");
			try_family(dpy, priv, list, "-%s-%s-r-*-*-*-120-*-*-c-*-iso10646-1");
			try_family(dpy, priv, list, "-%s-%s-r-*-*-*-0-*-*-m-*-iso10646-1");
			try_family(dpy, priv, list, "-%s-%s-r-*-*-*-0-*-*-c-*-iso10646-1");
		}
		else {
			try_family(dpy, priv, list, "-%s-%s-r-*-*-*-120-*-*-*-*-iso10646-1");
			try_family(dpy, priv, list, "-%s-%s-r-*-*-*-0-*-*-*-*-iso10646-1");
		}
	}

	cnt = (int)xv_get(list, PANEL_LIST_NROWS);

	for (i = cnt - 1; i >= 0; i--) {
		p = (char *)xv_get(list, PANEL_LIST_STRING, i);
		if (p && *p) {
			xv_set(list, PANEL_LIST_SELECT, i, FALSE, NULL);
		}
		else xv_set(list, PANEL_LIST_DELETE, i, NULL);
	}
	xv_set(list,
			PANEL_LIST_SORT, PANEL_FORWARD,
			PANEL_CHOOSE_NONE, FALSE,
			NULL);
	xv_set(list, PANEL_LIST_SELECT, 0, TRUE, NULL);
}

static char *make_help(Fontprops_private *priv, char *str)
{
	char *myhelp, *itemhelp;

	myhelp = (char *)xv_get(FONTPROPSPUB(priv), XV_HELP_DATA);
	if (! myhelp) return (char *)0;

	itemhelp = xv_malloc(strlen(myhelp) + strlen(str) + 2);
	sprintf(itemhelp, "%s_%s", myhelp, str);
	return itemhelp;
}

static void free_help_data(Xv_opaque obj, int key, char *data)
{
	if (key == XV_HELP && data) xv_free(data);
}

static void note_list_dying(Xv_opaque obj, int key, Fontprops_private *priv)
{
	if (key == fontpkey) xv_destroy(FONTPROPSPUB(priv));
}

static void create_fam(Fontprops_private *priv)
{
	xv_set(priv->frame,
			FRAME_PROPS_CREATE_ITEM,
				FRAME_PROPS_ITEM_SPEC, &priv->list, -1, PANEL_LIST,
				XV_INSTANCE_NAME, "fontproplist",
				PANEL_LABEL_STRING,
						priv->fam_label ?
						priv->fam_label :
						XV_MSG("Foundry-Family:"),
				PANEL_CHOOSE_ONE, TRUE,
				PANEL_CHOOSE_NONE, TRUE,
				PANEL_NOTIFY_PROC, note_list,
				PANEL_CLIENT_DATA, FONTPROP_FAMILY,
				XV_KEY_DATA, fontpkey, priv,
				XV_KEY_DATA_REMOVE_PROC, fontpkey, note_list_dying,
				PANEL_READ_ONLY, TRUE,
				PANEL_LIST_INSERT_DUPLICATE, FALSE,
				PANEL_LIST_WIDTH, 300,
				PANEL_LIST_DISPLAY_ROWS, 6,
				FRAME_PROPS_DATA_OFFSET, priv->dataoff + SUBOFF(family),
				FRAME_SHOW_RESIZE_CORNER, TRUE,
				XV_HELP_DATA,
					make_help(priv, priv->fixedwidth_only ? "fixed_fam":"fam"),
				XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
				NULL,
			NULL);
}

static void create_size(Fontprops_private *priv)
{
	if (priv->scales_only) {
		xv_set(priv->frame,
				FRAME_PROPS_CREATE_ITEM,
					FRAME_PROPS_ITEM_SPEC, &priv->size,-1, PANEL_CHOICE,
					PANEL_LABEL_STRING, XV_MSG("Scale:"),
					PANEL_CHOICE_STRINGS,
						XV_MSG("Small"),
						XV_MSG("Medium"),
						XV_MSG("Large"),
						XV_MSG("Extra Large"),
						NULL,
					PANEL_NOTIFY_PROC, note_make_font_name,
					PANEL_CLIENT_DATA, FONTPROP_SIZE,
					XV_KEY_DATA, fontpkey, priv,
					XV_HELP_DATA, make_help(priv, "scale"),
					XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
					FRAME_PROPS_DATA_OFFSET, priv->dataoff,
					FRAME_PROPS_CONVERTER, convert_scale_size, (Xv_opaque)TRUE,
					NULL,
				NULL);
	}
	else {
		xv_set(priv->frame,
				FRAME_PROPS_CREATE_ITEM,
					FRAME_PROPS_ITEM_SPEC, &priv->size, -1, PANEL_NUMERIC_TEXT,
					PANEL_LABEL_STRING, XV_MSG("Size:"),
					PANEL_MIN_VALUE, 40,
					PANEL_MAX_VALUE, 400,
					PANEL_NOTIFY_PROC, note_make_font_name,
					PANEL_CLIENT_DATA, FONTPROP_SIZE,
					XV_KEY_DATA, fontpkey, priv,
					XV_HELP_DATA, make_help(priv, "size"),
					XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
					FRAME_PROPS_DATA_OFFSET, priv->dataoff,
					FRAME_PROPS_CONVERTER, convert_scale_size, (Xv_opaque)FALSE,
					NULL,
				NULL);
	}
}

static void create_preview(Fontprops_private *priv)
{
	xv_set(priv->frame,
		FRAME_PROPS_CREATE_ITEM,
			FRAME_PROPS_ITEM_SPEC, NULL, 30, PANEL_MESSAGE,
			PANEL_LABEL_STRING, " ",
			NULL,
		FRAME_PROPS_CREATE_ITEM,
			FRAME_PROPS_ITEM_SPEC, NULL, FRAME_PROPS_MOVE, PANEL_BUTTON,
			PANEL_LABEL_STRING, XV_MSG("Preview"),
			PANEL_NOTIFY_PROC, note_preview,
			XV_KEY_DATA, fontpkey, priv,
			XV_HELP_DATA, make_help(priv, "prebutt"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
			NULL,
		FRAME_PROPS_CREATE_ITEM,
			FRAME_PROPS_ITEM_SPEC,
				&priv->preview, FRAME_PROPS_MOVE, PANEL_MESSAGE,
			PANEL_LABEL_STRING, XV_MSG("The Font will look like this"),
			XV_HELP_DATA, make_help(priv, "preview"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
			NULL,
		NULL);
}

static void create_name(Fontprops_private *priv)
{
	xv_set(priv->frame,
			FRAME_PROPS_CREATE_ITEM,
				FRAME_PROPS_ITEM_SPEC, &priv->name, -1, PANEL_TEXT,
				XV_INSTANCE_NAME, "fontpropname",
				PANEL_LABEL_STRING,
					priv->name_label ? priv->name_label : XV_MSG("Name:"),
				PANEL_VALUE_DISPLAY_LENGTH, 56,
				PANEL_VALUE_STORED_LENGTH, 300,
				PANEL_READ_ONLY, priv->name_read_only,
				PANEL_VALUE_UNDERLINED, ! priv->name_read_only,
				XV_HELP_DATA,
					make_help(priv, priv->name_read_only ? "ro_name" : "name"),
				XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
				FRAME_PROPS_DATA_OFFSET, priv->dataoff + SUBOFF(fontname),
				NULL,
			NULL);
}

static void create_slant(Fontprops_private *priv)
{
	xv_set(priv->frame,
			FRAME_PROPS_CREATE_ITEM,
				FRAME_PROPS_ITEM_SPEC, &priv->slant, -1, PANEL_CHOICE,
				PANEL_LABEL_STRING, XV_MSG("Slant:"),
				PANEL_NOTIFY_PROC, note_make_font_name,
				PANEL_CLIENT_DATA, FONTPROP_SLANT,
				XV_KEY_DATA, fontpkey, priv,
				PANEL_CHOICE_STRINGS,
					XV_MSG("Regular"),
					XV_MSG("Oblique"),
					XV_MSG("Italics"),
					NULL,
				XV_HELP_DATA, make_help(priv, "slant"),
				XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
				FRAME_PROPS_DATA_OFFSET, priv->dataoff+SUBOFF(slant),
				FRAME_PROPS_CONVERTER, convert_slant, XV_NULL,
				NULL,
			NULL);
}

static void create_style(Fontprops_private *priv)
{
	xv_set(priv->frame,
			FRAME_PROPS_CREATE_ITEM,
				FRAME_PROPS_ITEM_SPEC, &priv->style, -1, PANEL_CHOICE,
				PANEL_CHOOSE_ONE, FALSE,
				PANEL_FEEDBACK, PANEL_MARKED,
				PANEL_LABEL_STRING, XV_MSG("Style:"),
				PANEL_NOTIFY_PROC, note_make_font_name,
				PANEL_CLIENT_DATA, FONTPROP_STYLE,
				XV_KEY_DATA, fontpkey, priv,
				PANEL_CHOICE_STRINGS, XV_MSG("Bold"), NULL,
				XV_HELP_DATA, make_help(priv, "style"),
				XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
				FRAME_PROPS_DATA_OFFSET, priv->dataoff+SUBOFF(style),
				FRAME_PROPS_CONVERTER, convert_style, XV_NULL,
				NULL,
			NULL);
}

typedef void (*create_t)(Fontprops_private *);

static struct { Fontprop_setting which; create_t func; } create[] = {
	{ FONTPROP_FAMILY, create_fam },
	{ FONTPROP_SIZE, create_size },
	{ FONTPROP_STYLE, create_style },
	{ FONTPROP_SLANT, create_slant },
	{ FONTPROP_NAME, create_name },
	{ FONTPROP_PREVIEW, create_preview }
};

static void fill_panel(Fontprops_private *priv)
{
	int i;

	for (i = 0; i < PERM_NUMBER(priv->create); i++) {
		int j;

		if (! priv->create[i]) break;

		for (j = 0; j < PERM_NUMBER(create); j++) {
			if (priv->create[i] == create[j].which) {
				(*(create[j].func))(priv);
				break;
			}
		}
	}

	if (priv->list) find_families(priv, priv->list);
}

static char *make_foundry_family(Font_props self, Xv_font font)
{
	Xv_window owner = xv_get(self, XV_OWNER);
	Xv_server srv = XV_SERVER_FROM_WINDOW(owner);
	XFontStruct *fs = (XFontStruct *)xv_get(font, FONT_INFO);
	Atom foundry = (Atom)xv_get(srv, SERVER_ATOM, "FOUNDRY");
	char *fam, *foun, foundry_family[301];
	unsigned long val = 0;

	if (! XGetFontProperty(fs, foundry, &val)) {
		val = 0;
	}

	fam = (char *)xv_get(font, FONT_FAMILY);
	if (val) {
		foun = (char *)xv_get(srv, SERVER_ATOM_NAME, val);
		if (foun) {
			char *pp, myfoun[200];

			strcpy(myfoun, foun);
			for (pp = myfoun; *pp; pp++) {
				if (isupper(*pp)) *pp = tolower(*pp);
			}
			sprintf(foundry_family, "%s-%s", myfoun, fam ? fam : "*");
			return xv_strsave(foundry_family);
		}
	}

	sprintf(foundry_family, "*-%s", fam ? fam : "*");
	return xv_strsave(foundry_family);
}

static int fontprops_init(Panel owner, Font_props slf, Attr_avlist avlist,
										int *u)
{
	Xv_fontprops *self = (Xv_fontprops *)slf;
	Fontprops_private *priv;
	Frame_props fram;

	if (! fontpkey) fontpkey = xv_unique_key();

	if (xv_get(owner, XV_IS_SUBTYPE_OF, PANEL)) {
		fram = xv_get(owner, XV_OWNER);
	}
	else {
		fram = owner;
	}

	if (! xv_get(fram, XV_IS_SUBTYPE_OF, FRAME_PROPS)) {
		xv_error((Xv_opaque)self,
				ERROR_PKG, FONT_PROPS,
				ERROR_LAYER, ERROR_PROGRAM,
				ERROR_STRING,
					"owner must be a FRAME_PROPS or a PANEL in a FRAME_PROPS",
				ERROR_SEVERITY, ERROR_NON_RECOVERABLE,
				NULL);
		return XV_ERROR;
	}

	priv = xv_alloc(Fontprops_private);
	if (!priv) return XV_ERROR;

	priv->frame = fram;
	priv->public_self = (Xv_opaque)self;
	self->private_data = (Xv_opaque)priv;

	priv->scales_only = TRUE;

	priv->create[0] = FONTPROP_FAMILY;
	priv->create[1] = FONTPROP_SIZE;
	priv->create[2] = FONTPROP_STYLE;
	priv->create[3] = FONTPROP_SLANT;
	priv->create[4] = FONTPROP_NAME;
	priv->create[5] = FONTPROP_PREVIEW;
	priv->create[6] = 0;

	return XV_OK;
}

static Xv_opaque fontprops_set(Font_props self, Attr_avlist avlist)
{
	Attr_attribute *attrs;
	Fontprops_private *priv = FONTPROPSPRIV(self);
	fontprop_t *fontprop_addr;
	Xv_font font;
	int i;
	char *p;

	for (attrs=avlist; *attrs; attrs=attr_next(attrs)) switch ((int)*attrs) {
		case FONTPROPS_DATA_OFFSET:
			priv->dataoff = (int)A1;
			ADONE;

		case FONTPROPS_BOLD_AND_MEDIUM:
			priv->need_both = (char)A1;
			ADONE;

		case FONTPROPS_INIT_FROM_FONT:
			font = (Xv_font)A1;
			fontprop_addr = (fontprop_t *)A2;
			fontprop_addr->family = make_foundry_family(self, font);
			fontprop_addr->scale = (int)xv_get(font, FONT_SCALE);
			p = (char *)xv_get(font, FONT_STYLE);
			fontprop_addr->style = xv_strsave(p ? p : "");
			fontprop_addr->size = 10 * (int)xv_get(font, FONT_SIZE);
			ADONE;

		case FONTPROPS_FILL_FAMILY_LIST:
			find_families(priv, (Panel_item)A1);
			ADONE;

		case FONTPROPS_FAMILY_LABEL:
			priv->fam_label = (char *)A1;
			ADONE;

		case FONTPROPS_NAME_LABEL:
			priv->name_label = (char *)A1;
			ADONE;

		case FONTPROPS_INIT_FROM_NAME:
			{
				char foundryfam[100];
				char buf[500];
				fontprop_t *fp  = (fontprop_t *)A1;
				char *parts[30];

				memset(parts, 0, sizeof(parts));
				strcpy(buf, fp->fontname ? fp->fontname : "");
				for (parts[i = 0] = strtok(buf, "-");
					parts[i];
					parts[++i] = strtok((char *)0, "-"));

				sprintf(foundryfam, "%s-%s",
								parts[0] ? parts[0] : "b&h",
								parts[1] ? parts[1] : "lucida");
				fp->family = xv_strsave(foundryfam);
				fp->style = xv_strsave(parts[2] ? parts[2] : "medium");
				fp->slant = (parts[3] ? *(parts[3]) : 'r');
				fp->size = atoi(parts[8] ? parts[8] : "120");
			}
			ADONE;

		case FONTPROPS_USE_FOR_SIZE:
			if (priv->scales_only) {
				priv->cbfp.size = scales[(int)A1];
			}
			else {
				priv->cbfp.size = (int)A1;
			}
			if (priv->name) make_font_name(priv, FONTPROP_TRIGGER);
			ADONE;

		case FONTPROPS_ITEMS:
			for (i = 1; i < PERM_NUMBER(priv->create) && attrs[i]; i++) {
				priv->create[i - 1] = (Fontprop_setting)attrs[i];
			}
			priv->create[i - 1] = 0;
			ADONE;

		case FONTPROPS_NOTIFY_PROC:
			priv->proc = (notify_proc_t)A1;
			ADONE;

		case FONTPROPS_FIXEDWIDTH_ONLY:
			priv->fixedwidth_only = (char)A1;
			ADONE;

		case FONTPROPS_SCALABLE_ONLY:
			priv->scalable_only = (char)A1;
			ADONE;

		case FONTPROPS_SCALES_ONLY:
			priv->scales_only = (char)A1;
			ADONE;

		case FONTPROPS_NAME_READ_ONLY:
			priv->name_read_only = (char)A1;
			ADONE;

		case XV_END_CREATE:
			fill_panel(priv);
			break;

		default: xv_check_bad_attr(FONT_PROPS, A0);
			break;
	}

	return XV_OK;
}

/*ARGSUSED*/
static Xv_opaque fontprops_get(Font_props self, int *status,
								Attr_attribute attr, va_list vali)
{
	Fontprops_private *priv = FONTPROPSPRIV(self);

	*status = XV_OK;
	switch ((int)attr) {
		case FONTPROPS_DATA_OFFSET: return (Xv_opaque)priv->dataoff;
		case FONTPROPS_FIXEDWIDTH_ONLY: return (Xv_opaque)priv->fixedwidth_only;
		case FONTPROPS_SCALABLE_ONLY: return (Xv_opaque)priv->scalable_only;
		case FONTPROPS_SCALES_ONLY: return (Xv_opaque)priv->scales_only;
		case FONTPROPS_NAME_READ_ONLY: return (Xv_opaque)priv->name_read_only;
		case FONTPROPS_BOLD_AND_MEDIUM: return (Xv_opaque)priv->need_both;
		default:
			*status = xv_check_bad_attr(FONT_PROPS, attr);
			return (Xv_opaque)XV_OK;
	}
}

static int fontprops_destroy(Font_props self, Destroy_status status)
{
	if (status == DESTROY_CLEANUP) {
		Fontprops_private *priv = FONTPROPSPRIV(self);

		if (priv->cbfp.family) xv_free(priv->cbfp.family);
		if (priv->cbfp.style) xv_free(priv->cbfp.style);
		xv_free(priv);
	}
	return XV_OK;
}

Xv_pkg xv_fontprops_pkg = {
	"FontProperty",
	ATTR_PKG_FONT_PROPS,
	sizeof(Xv_fontprops),
	XV_GENERIC_OBJECT,
	fontprops_init,
	fontprops_set,
	fontprops_get,
	fontprops_destroy,
	0
};
