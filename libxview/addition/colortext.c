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
#include <xview/colortext.h>
#include <xview/colorchsr.h>
#include <xview_private/i18n_impl.h>
#include <xview_private/attr_impl.h>

char colortext_c_sccsid[] = "@(#) %M% V%I% %E% %U% $Id: colortext.c,v 4.3 2024/11/19 12:58:02 dra Exp $";

typedef void (*layout_proc_t)(Panel_item, Rect *);
typedef struct {
	Xv_opaque       public_self;

	layout_proc_t   layout;
	Panel_item      winbutton;
	char *header;
	Attr_avlist saved_attrs;
} PanelColortext_private;

#define PCTPRIV(_x_) XV_PRIVATE(PanelColortext_private, Xv_panel_colortext, _x_)
#define PCTPUB(_x_) XV_PUBLIC(_x_)

#define A0 *attrs
#define A1 attrs[1]
#define A2 attrs[2]

#define ADONE ATTR_CONSUME(*attrs);break

static void colortext_layout(Panel_item item, Rect *deltas)
{
	PanelColortext_private *priv = PCTPRIV(item);

	/*
	 * The item has been moved.  Adjust the item coordinates.
	 */
	(*(priv->layout))(item, deltas);
	if (priv->winbutton) {
		xv_set(priv->winbutton,
				XV_X, xv_get(priv->winbutton, XV_X) + deltas->r_left,
				XV_Y, xv_get(priv->winbutton, XV_Y) + deltas->r_top,
				NULL);
	}
}


/* only for setting a changebar */
static void note_color_changed(Color_chooser c, int r, int g, int b)
{
	Panel_item self = xv_get(c, COLORCHOOSER_TEXTFIELD);
	Frame fram = xv_get(xv_get(self, XV_OWNER), XV_OWNER);

	if (xv_get(fram, XV_IS_SUBTYPE_OF, FRAME_PROPS)) {
		xv_set(fram, FRAME_PROPS_ITEM_CHANGED, self, TRUE, NULL);
	}
}

static char *make_help(PanelColortext_private *priv, char *str)
{
	char *hf, *myhelp, *itemhelp, helpbuf[100];
	Panel_item self = PCTPUB(priv);
	Xv_server srv = XV_SERVER_FROM_WINDOW(xv_get(self, XV_OWNER));

	myhelp = (char *)xv_get(self, XV_HELP_DATA);
	if (myhelp) {
		itemhelp = xv_malloc(strlen(myhelp) + strlen(str) + 2);
		sprintf(itemhelp, "%s_%s", myhelp, str);
		return itemhelp;
	}

	hf = (char *)xv_get(srv,XV_APP_HELP_FILE);
	if (! hf) hf = "xview";

	sprintf(helpbuf, "%s:colortext_%s", hf, str);

	return xv_strsave(helpbuf);
}

static void free_help_data(Xv_opaque obj, int key, char *data)
{
	if (key == XV_HELP && data) xv_free(data);
}

static int ds_force_popup_on_screen(int *popup_x_p, int *popup_y_p, Frame popup)
{
    Rect popup_rect;
    Rect *scr_size;
    int popup_x, popup_y, screen_width, screen_height;
    int popup_width, popup_height, n, rcode;

    popup_x = *popup_x_p;
    popup_y = *popup_y_p;

    /* Get the screen size */
    scr_size = (Rect *)xv_get(popup, WIN_SCREEN_RECT);
    screen_width = scr_size->r_width;
    screen_height = scr_size->r_height;

    frame_get_rect(popup, &popup_rect);
    popup_width = popup_rect.r_width;
    popup_height = popup_rect.r_height;

    /* Make sure frame does not go off side of screen */
    n = popup_x + popup_width;
    if (n > screen_width) {
        popup_x -= (n - screen_width);
    } else if (popup_x < 0) {
        popup_x = 0;
    }

    /* Make sure frame doen't go off top or bottom */
    n = popup_y + popup_height;
    if (n > screen_height) {
        popup_y -= n - screen_height;
    } else if (popup_y < 0) {
        popup_y = 0;
    }

    /* Set location and return */
    popup_rect.r_left = popup_x;
    popup_rect.r_top = popup_y;
    frame_set_rect(popup, &popup_rect);

    if (popup_x != *popup_x_p || popup_y != *popup_y_p) {
        rcode = TRUE;
    } else {
        rcode = FALSE;
    }
    *popup_x_p = popup_x;
    *popup_y_p = popup_y;

    return(rcode);
}

static Frame create_color_frame(Panel_item abwb)
{
	Panel pan = xv_get(abwb, XV_OWNER);
	Frame par, f = xv_get(pan, XV_OWNER);
	Panel_item self = xv_get(abwb, PANEL_CLIENT_DATA);
	PanelColortext_private *priv = PCTPRIV(self);
	Rect r = *((Rect *)xv_get(abwb, XV_RECT));
	int x, y;
	Color_chooser cch;

	/* Versuche, einen FRAME_BASE als owner zu finden */
	par = f;
	while (par != XV_NULL) {
		f = par;
		if (xv_get(f, XV_IS_SUBTYPE_OF, FRAME_BASE)) break;

		par = xv_get(f, XV_OWNER);
	}
	/* hilft aber nix */

	win_translate_xy(pan, xv_get(pan, XV_ROOT), r.r_left, r.r_top+r.r_height,
												&x, &y);

	if (priv->saved_attrs) {
		cch = xv_create(f, COLOR_CHOOSER,
					ATTR_LIST, priv->saved_attrs,
					XV_LABEL, priv->header,
					XV_X, x + 4,
					XV_Y, y + 2,
					COLORCHOOSER_TEXTFIELD, xv_get(abwb, PANEL_CLIENT_DATA),
					COLORCHOOSER_CHANGED_PROC, note_color_changed,
					XV_HELP_DATA, make_help(priv, "chooser"),
					XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
					NULL);

		xv_free(priv->saved_attrs);
		priv->saved_attrs = NULL;
	}
	else {
		cch = xv_create(f, COLOR_CHOOSER,
					XV_LABEL, priv->header,
					XV_X, x + 4,
					XV_Y, y + 2,
					COLORCHOOSER_TEXTFIELD, xv_get(abwb, PANEL_CLIENT_DATA),
					COLORCHOOSER_CHANGED_PROC, note_color_changed,
					XV_HELP_DATA, make_help(priv, "chooser"),
					XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
					NULL);
	}
	ds_force_popup_on_screen(&x, &y, cch);
	return cch;
}

static int colortext_init(Panel owner, Xv_opaque slf, Attr_avlist avlist, int *unused)
{
	Xv_panel_colortext *self = (Xv_panel_colortext *)slf;
	PanelColortext_private *priv;
	Rect *r;

	if (!(priv = (PanelColortext_private *)xv_alloc(PanelColortext_private)))
		return XV_ERROR;

	priv->public_self = (Xv_opaque)self;
	self->private_data = (Xv_opaque)priv;

	r = (Rect *)xv_get(slf, PANEL_ITEM_VALUE_RECT);

	/* den Button schon jetzt erzeugen, vergleiche p_slider.c:616 */
	priv->winbutton = xv_create(owner, PANEL_ABBREV_WINDOW_BUTTON,
						PANEL_AWB_CREATE_WINDOW_PROC, create_color_frame,
						PANEL_ITEM_OWNER, slf,
						PANEL_CLIENT_DATA, slf,
						XV_X, rect_right(r) + 4,
						XV_Y, r->r_top,
						NULL);

	xv_set(slf, 
			PANEL_VALUE_DISPLAY_LENGTH, 12,
			PANEL_VALUE_STORED_LENGTH, 12,
			NULL);

	return XV_OK;
}

static void colortext_end_create(PanelColortext_private *priv, Panel_color_text_item self)
{
	static Panel_ops myops;
	Panel_ops *superops = (Panel_ops *)xv_get(self, PANEL_OPS_VECTOR);
	Rect *r;

	myops = *superops;
	priv->layout = myops.panel_op_layout;
	myops.panel_op_layout = colortext_layout;
	xv_set(self, PANEL_OPS_VECTOR, &myops, NULL);

	if (! priv->header) {
		priv->header = strdup(XV_MSG("Color"));
	}
	r = (Rect *)xv_get(self, PANEL_ITEM_VALUE_RECT);
	xv_set(priv->winbutton,
			XV_X, rect_right(r) + 4,
			XV_Y, r->r_top,
			XV_HELP_DATA, make_help(priv, "abwbutton"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
			NULL);
}

static void save_window_attrs(PanelColortext_private *priv, Attr_avlist avlist)
{
	priv->saved_attrs = xv_alloc_n(Attr_attribute, (size_t)ATTR_STANDARD_SIZE);

	attr_copy_avlist(priv->saved_attrs, avlist);
}

static Xv_opaque colortext_set(Panel_color_text_item self, Attr_avlist avlist)
{
	Attr_attribute *attrs;
	PanelColortext_private *priv = PCTPRIV(self);

	for (attrs=avlist; *attrs; attrs=attr_next(attrs)) switch ((int)*attrs) {
		case PANEL_COLORTEXT_HEADER:
			if (priv->header) xv_free(priv->header);
			priv->header = strdup((char *)A1);
			ADONE;
		case PANEL_INACTIVE:
			xv_set(priv->winbutton, PANEL_INACTIVE, (int)attrs[1], NULL);
			break;
	
		case PANEL_COLORTEXT_WINDOW_ATTRS:
			save_window_attrs(priv, attrs+1);
			ADONE;

		case XV_END_CREATE:
			colortext_end_create(priv, self);
			return XV_OK;

		default: xv_check_bad_attr(PANEL_COLORTEXT, A0);
			break;
	}

	return XV_OK;
}

static Xv_opaque colortext_get(Panel_color_text_item self, int *status, Attr_attribute attr, va_list vali)
{
	PanelColortext_private *priv = PCTPRIV(self);

	*status = XV_OK;
	switch ((int)attr) {
		case PANEL_COLORTEXT_WINDOW_BUTTON: return (Xv_opaque)priv->winbutton;
		case PANEL_COLORTEXT_HEADER: return (Xv_opaque)priv->header;
		default:
			*status = xv_check_bad_attr(PANEL_COLORTEXT, attr);

	}
	return (Xv_opaque)XV_OK;
}

static int colortext_destroy(Panel_color_text_item self, Destroy_status status)
{
	if (status == DESTROY_CLEANUP) {
		PanelColortext_private *priv = PCTPRIV(self);

		if (priv->winbutton) xv_destroy(priv->winbutton);
		priv->winbutton = XV_NULL;
		if (priv->header) xv_free(priv->header);

		xv_free((char *)priv);
	}
	return XV_OK;
}

Xv_pkg xv_panel_color_text_pkg = {
	"PanelColortextItem",
	ATTR_PKG_PANEL_COLOR_TEXT,
	sizeof(Xv_panel_colortext),
	PANEL_TEXT,
	colortext_init,
	colortext_set,
	colortext_get,
	colortext_destroy,
	0
};
