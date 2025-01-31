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
#include <xview/scrollbar.h>
#include <xview_private/panel_impl.h>

#ifndef lint
#ifdef sccs
static char pansw_c_sccsid[] = "@(#) %M% V%I% %E% %U% $Id: pansw.c,v 4.2 2024/04/12 06:05:12 dra Exp $";
#endif
#endif

typedef void (*notify_t)(Panel_item, int, int*);
typedef struct {
	Xv_opaque public_self;
	Xv_window subwin;
	int width, height;
	char size_dirty, pos_dirty;
	Xv_pkg *subwin_class;
	Attr_avlist create_attrs;
	notify_t notify_proc;
} Panel_subwin_private;

#define PSWPRIV(_x_) XV_PRIVATE(Panel_subwin_private, Xv_panel_subwindow, _x_)
#define PSWPUB(_x_) XV_PUBLIC(_x_)
#define A0 *attrs
#define A1 attrs[1]
#define A2 attrs[2]
#define A3 attrs[3]

#define ADONE ATTR_CONSUME(*attrs);break

static void set_subwin_pos(Panel_subwin_private *priv)
{
	if (priv->subwin) {
    	Item_info *ip = ITEM_PRIVATE(PSWPUB(priv));
		int x, y;

		win_translate_xy(PANEL_PUBLIC(ip->panel),
						xv_get(PANEL_PUBLIC(ip->panel), XV_OWNER),
						ip->value_rect.r_left, ip->value_rect.r_top, &x, &y);

		xv_set(priv->subwin, XV_X, x, XV_Y, y, NULL);
		priv->pos_dirty = FALSE;
	}
}

static void pansw_paint(Panel_subwin_item item, Panel_setting u)
{
	Panel_subwin_private *priv = PSWPRIV(item);

	panel_paint_label(item);

	/* let's assume we wouldn't be asked to repaint if we weren't visible */
	if (priv->subwin) {
		xv_set(priv->subwin, XV_SHOW, TRUE, NULL);
		XClearArea((Display *)xv_get(priv->subwin, XV_DISPLAY),
					(Window)xv_get(priv->subwin, XV_XID),
					0, 0, 0, 0, TRUE);
	}
}

static void pansw_resize(Panel_subwin_item item)
{
	Panel_subwin_private *priv = PSWPRIV(item);
    Item_info *ip = ITEM_PRIVATE(PSWPUB(priv));

	priv->size_dirty = FALSE;
	if (!priv->subwin) return;
	set_subwin_pos(priv);
	xv_set(priv->subwin,
			XV_WIDTH, ip->value_rect.r_width,
			XV_HEIGHT, ip->value_rect.r_height,
			NULL);
}

static void pansw_remove(Panel_subwin_item item)
{
	Panel_subwin_private *priv = PSWPRIV(item);

	if (!priv->subwin) return;

	xv_set(priv->subwin, XV_SHOW, FALSE, NULL);
}

static void pansw_restore(Panel_subwin_item item, Panel_setting u)
{
	Panel_subwin_private *priv = PSWPRIV(item);

	if (!priv->subwin) return;

	xv_set(priv->subwin, XV_SHOW, TRUE, NULL);
}

static void pansw_layout(Panel_subwin_item item, Rect *delta)
{
	Panel_subwin_private *priv = PSWPRIV(item);


	if (!priv->subwin) return;

	xv_set(priv->subwin,
				XV_X, delta->r_left + (int)xv_get(priv->subwin, XV_X),
				XV_Y, delta->r_top + (int)xv_get(priv->subwin, XV_Y),
				NULL);
	priv->pos_dirty = FALSE;
}

static void pansw_accept_kbd_focus(Panel_subwin_item item)
{
	Panel_subwin_private *priv = PSWPRIV(item);

	if (!priv->subwin) return;
	xv_set(priv->subwin, WIN_SET_FOCUS, NULL);
}

/*ARGSUSED*/
static int pansw_init(Panel owner, Xv_opaque slf, Attr_avlist avlist, int *u)
{
	static Panel_ops ops = {
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		panel_default_clear_item,
		pansw_paint,
		pansw_resize,
		pansw_remove,
		pansw_restore,
		pansw_layout,
		pansw_accept_kbd_focus,
		NULL,
		NULL
	};

	Xv_panel_subwindow *self = (Xv_panel_subwindow *)slf;
	Attr_attribute *attrs;
	Panel_subwin_private *priv = xv_alloc(Panel_subwin_private);
    Item_info *ip = ITEM_PRIVATE(slf);

	if (!priv) return XV_ERROR;

	priv->public_self = slf;
	self->private_data = (Xv_opaque)priv;

	priv->width = priv->height = 150;
	priv->subwin_class = WINDOW;
    ip->ops = ops;

	for (attrs=avlist; *attrs; attrs=attr_next(attrs)) switch ((int)*attrs) {
		case PANEL_WINDOW_WIDTH:
			priv->width = (int)A1;
			ADONE;

		case PANEL_WINDOW_HEIGHT:
			priv->height = (int)A1;
			ADONE;
	}

	ip->value_rect.r_width = priv->width;
	ip->value_rect.r_height = priv->height;
	ip->flags |= DEAF;

	xv_set(slf,
			PANEL_LABEL_FONT, xv_get(owner, PANEL_BOLD_FONT),
			PANEL_ACCEPT_KEYSTROKE, TRUE,
			NULL);

	return XV_OK;
}

static Xv_opaque pansw_set(Panel_subwin_item self, Attr_avlist avlist)
{
	Attr_attribute *attrs;
	Panel_subwin_private *priv = PSWPRIV(self);
	Rect *rp, rect;
	int notify = FALSE, geom = FALSE;

	/* check for position attributes */
	for (attrs=avlist; *attrs; attrs=attr_next(attrs)) switch ((int)*attrs) {
		case PANEL_NOTIFY_PROC:
			priv->notify_proc = (notify_t)A1;
			ADONE;

		case XV_X:
		case XV_Y:
		case XV_RECT:
		case PANEL_ITEM_LABEL_RECT:
		case PANEL_VALUE_X:
		case PANEL_VALUE_Y:
		case PANEL_LABEL_X:
		case PANEL_LABEL_Y:
		case PANEL_ITEM_X:
		case PANEL_ITEM_Y:
			priv->pos_dirty = TRUE;
			break;
		default: break;
	}

	if (*avlist != XV_END_CREATE) {
		xv_set(xv_get(self, XV_OWNER), PANEL_NO_REDISPLAY_ITEM, TRUE, NULL);
		xv_super_set_avlist(self, PANEL_SUBWINDOW, avlist);
		xv_set(xv_get(self, XV_OWNER), PANEL_NO_REDISPLAY_ITEM, FALSE, NULL);
	}

	for (attrs=avlist; *attrs; attrs=attr_next(attrs)) switch ((int)*attrs) {
		case PANEL_WINDOW_CLASS:
			if (!priv->subwin) priv->subwin_class = (Xv_pkg *)A1;
			ADONE;

		case PANEL_WINDOW_ATTRS:
			if (priv->subwin) {
				xv_set_avlist(priv->subwin, &A1);
			}
			else {
				priv->create_attrs = &A1;
			}
			ADONE;

		case PANEL_WINDOW_SIZE_CHANGED:
			if (priv->subwin) {
				xv_error(self,
						ERROR_PKG, PANEL_SUBWINDOW,
						ERROR_SEVERITY, ERROR_RECOVERABLE,
						ERROR_STRING, "PANEL_WINDOW_SIZE_CHANGED not yet implemented",
						NULL);
			}
			ADONE;

		case PANEL_WINDOW_WIDTH:
			priv->width = (int)A1;
			geom = TRUE;
			ADONE;

		case PANEL_WINDOW_HEIGHT:
			priv->height = (int)A1;
			if (priv->subwin) notify = TRUE;
			geom = TRUE;
			ADONE;

		case XV_SHOW:
			if (priv->subwin)
				xv_set(priv->subwin, XV_SHOW, (int)A1, NULL);
			break;

		case XV_END_CREATE:
			rp = (Rect *)xv_get(self, PANEL_ITEM_VALUE_RECT);
			rect = *rp;
			rect.r_width = priv->width;
			rect.r_height = priv->height;

			xv_set(self, PANEL_ITEM_VALUE_RECT, &rect, NULL);

			if (priv->create_attrs) {
				priv->subwin = xv_create(xv_get(self,XV_OWNER),
						priv->subwin_class,
						ATTR_LIST, priv->create_attrs,
						XV_INSTANCE_NAME, "p_subwin",
						XV_WIDTH, priv->width,
						XV_HEIGHT, priv->height,
						XV_HELP_DATA, xv_get(self, XV_HELP_DATA),
						XV_KEY_DATA, FRAME_ORPHAN_WINDOW, TRUE,
						XV_SHOW, xv_get(self, XV_SHOW),
						NULL);
			}
			else {
				priv->subwin = xv_create(xv_get(self,XV_OWNER),
						priv->subwin_class,
						XV_INSTANCE_NAME, "p_subwin",
						XV_WIDTH, priv->width,
						XV_HEIGHT, priv->height,
						XV_HELP_DATA, xv_get(self, XV_HELP_DATA),
						XV_KEY_DATA, FRAME_ORPHAN_WINDOW, TRUE,
						XV_SHOW, xv_get(self, XV_SHOW),
						NULL);
			}

			set_subwin_pos(priv);

			return XV_OK;

		default:
			break;
	}

	if (priv->pos_dirty) set_subwin_pos(priv);

	if (notify && priv->notify_proc) {
		(*(priv->notify_proc))(self, priv->height, &priv->width);
		geom = TRUE;
	}

	if (geom) {
		rp = (Rect *)xv_get(self, PANEL_ITEM_VALUE_RECT);
		rect = *rp;
		rect.r_width = priv->width;
		rect.r_height = priv->height;

		priv->size_dirty = TRUE;
		xv_set(self, PANEL_ITEM_VALUE_RECT, &rect, NULL);
		if (priv->size_dirty) {
			/* resize method has not been called */
			if (priv->subwin) xv_set(priv->subwin,
										XV_WIDTH, priv->width,
										XV_HEIGHT, priv->height,
										NULL);
			priv->size_dirty = FALSE;
		}
	}

	return XV_SET_DONE;
}

static Xv_opaque pansw_get(Panel_subwin_item self, int *status, Attr_attribute attr, va_list vali)
{
	Panel_subwin_private *priv = PSWPRIV(self);

	*status = XV_OK;
	switch ((int)attr) {
		case PANEL_WINDOW_WIDTH: return (Xv_opaque)priv->width;
		case PANEL_WINDOW_HEIGHT: return (Xv_opaque)priv->height;
		case PANEL_NOTIFY_PROC: return (Xv_opaque)priv->notify_proc;

		case PANEL_ITEM_NTH_WINDOW:
			if (! va_arg(vali, int)) return priv->subwin;
			else return (Xv_opaque)0;

		case PANEL_ITEM_NWINDOWS:
			return (Xv_opaque)1;

		default:
			*status = XV_ERROR;
	}
	return (Xv_opaque)XV_OK;
}

/*ARGSUSED*/
static int pansw_destroy(Panel_subwin_item self, Destroy_status status)
{
	if (status == DESTROY_CLEANUP) {
		Panel_subwin_private *priv = PSWPRIV(self);

		pansw_remove((Xv_opaque)self);
		if (priv->subwin) xv_destroy(priv->subwin);

		free((char *)priv);
	}
	return XV_OK;
}

Xv_pkg xv_panel_subwin_pkg = {
	"PanelSubWindowItem",
	ATTR_PKG_PANEL,
	sizeof(Xv_panel_subwindow),
	PANEL_ITEM,
	pansw_init,
	pansw_set,
	pansw_get,
	pansw_destroy,
	0
};
