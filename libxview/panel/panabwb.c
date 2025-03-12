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
#include <xview/panel.h>
#include <olgx/olgx.h>
#include <xview_private/panel_impl.h>
#include <xview_private/draw_impl.h>

char panabwb_c_sccsid[] = "@(#) %M% V%I% %E% %U% $Id: panabwb.c,v 4.3 2025/03/08 13:08:26 dra Exp $";

typedef Frame (*creation_proc_t)(Panel_item);

typedef struct {
	Xv_opaque       public_self;

	Frame window;
	creation_proc_t creproc;
	int abbrev_width;
	int state;
} PanelAbbrevWindow_private;

#define PAWBPRIV(_x_) XV_PRIVATE(PanelAbbrevWindow_private, Xv_panel_abbrev_window, _x_)
#define PAWBPUB(_x_) XV_PUBLIC(_x_)

#define A0 *attrs
#define A1 attrs[1]
#define INHERIT NULL

#define ADONE ATTR_CONSUME(*attrs);break

#define AMB_OFFSET	4	/* gap between label and Abbrev. Window Button */

static void awbtn_paint_value(Panel_item self, int state)
{
	Item_info *ip = ITEM_PRIVATE(self);
	Xv_Window	    pw;

	if (inactive(ip)) state |= OLGX_INACTIVE;

	PANEL_EACH_PAINT_WINDOW(ip->panel, pw)
		Xv_Drawable_info *info;

		DRAWABLE_INFO_MACRO(pw, info);
		olgx_draw_abbrev_button(ip->panel->ginfo, xv_xid(info),
					ip->value_rect.r_left + AMB_OFFSET, ip->value_rect.r_top,
					OLGX_ABBREV | state);
	PANEL_END_EACH_PAINT_WINDOW
}

static void panabwb_begin_preview(Panel_item item, Event *event)
{
	PanelAbbrevWindow_private *priv = PAWBPRIV(item);
	int awb_invoked;

	if (!event_is_button(event))
		awb_invoked = TRUE;
	else {
		Item_info *ip = ITEM_PRIVATE(item);
		Rect awb_rect;

		rect_construct(&awb_rect, ip->value_rect.r_left + AMB_OFFSET,
								ip->value_rect.r_top,
								ip->value_rect.r_width - AMB_OFFSET,
								ip->value_rect.r_height);
		awb_invoked = rect_includespoint(&awb_rect, event_x(event),
											event_y(event));
	}
	if (awb_invoked) {
		awbtn_paint_value(item, OLGX_INVOKED);
		priv->state |= OLGX_INVOKED;
	}
}

static void panabwb_cancel_preview(Panel_item item, Event *event)
{
	PanelAbbrevWindow_private *priv = PAWBPRIV(item);

	/*
	 * The pointer as been dragged out of the item after
	 * beginning a preview.  Remove the active feedback
	 * (i.e., unhighlight) and clean up any private data.
	 */
	if (priv->state & OLGX_INVOKED) {
		Panel pan = xv_get(item, XV_OWNER);
		Panel_status *st = (Panel_status *)xv_get(pan, PANEL_STATUS);
		priv->state &= ~OLGX_INVOKED;
		awbtn_paint_value(item,
					st->three_d ? OLGX_NORMAL : OLGX_ERASE | OLGX_NORMAL);
	}
}

static void panabwb_accept_preview(Panel_item item, Event *event)
{
	PanelAbbrevWindow_private *priv = PAWBPRIV(item);

	if (priv->state & OLGX_INVOKED) {
		Panel pan = xv_get(item, XV_OWNER);
		Panel_status *st = (Panel_status *)xv_get(pan, PANEL_STATUS);
		priv->state &= ~OLGX_INVOKED;
		awbtn_paint_value(item, OLGX_BUSY);

		if (!priv->window && priv->creproc) {
			priv->window = (*priv->creproc)(item);
		}

		if (priv->window) {
			xv_set(priv->window, XV_SHOW, TRUE, NULL);
		}

		awbtn_paint_value(item,
					st->three_d ? OLGX_NORMAL : OLGX_ERASE | OLGX_NORMAL);
		xv_set(item, PANEL_NOTIFY_STATUS, XV_ERROR, NULL);
	}
}

static void panabwb_paint(Panel_item item, Panel_setting unused)
{
	Panel pan = xv_get(item, XV_OWNER);
	Panel_status *st = (Panel_status *)xv_get(pan, PANEL_STATUS);

	/* Paint the label */
	panel_paint_label(item);

	/* Paint the value */
	awbtn_paint_value(item, st->three_d ? OLGX_NORMAL : OLGX_ERASE|OLGX_NORMAL);
}

static void panabwb_accept_kbd_focus(Panel_item item)
{
	Frame frame;
	PanelAbbrevWindow_private *priv = PAWBPRIV(item);
	int x, y;
	Rect *vr;

	frame = xv_get(xv_get(item, XV_OWNER), WIN_FRAME);
	vr = (Rect *)xv_get(item, PANEL_ITEM_VALUE_RECT);

	if ((Panel_setting)xv_get(item, PANEL_LAYOUT) == PANEL_HORIZONTAL) {
		xv_set(frame, FRAME_FOCUS_DIRECTION, FRAME_FOCUS_UP, NULL);
		x = vr->r_left + (priv->abbrev_width - FRAME_FOCUS_UP_WIDTH) / 2;
		y = rect_bottom(vr);
	}
	else {
		xv_set(frame, FRAME_FOCUS_DIRECTION, FRAME_FOCUS_RIGHT, NULL);
		x = vr->r_left - FRAME_FOCUS_RIGHT_WIDTH;
		y = vr->r_top + (priv->abbrev_width - FRAME_FOCUS_RIGHT_HEIGHT) / 2;
	}
	if (x < 0) x = 0;
	if (y < 0) y = 0;

	panel_show_focus_win(item, frame, x, y);
}

static int panabwb_init(Panel owner, Xv_opaque slf, Attr_avlist avlist, int *unused)
{
	static Panel_ops ops = {
		panel_default_handle_event,     /* handle_event() */
		panabwb_begin_preview,          /* begin_preview() */
		NULL,                           /* update_preview() */
		panabwb_cancel_preview,         /* cancel_preview() */
		panabwb_accept_preview,         /* accept_preview() */
		NULL,                           /* accept_menu() */
		INHERIT,                        /* accept_key() */
		panel_default_clear_item,       /* clear() */
		panabwb_paint,                  /* paint() */
		NULL,                           /* resize() */
		NULL,                           /* remove() */
		NULL,                           /* restore() */
		NULL,                           /* layout() */
		panabwb_accept_kbd_focus,       /* accept_kbd_focus() */
		INHERIT,                        /* yield_kbd_focus() */
		NULL                            /* extension: reserved for future use */
	};
	typedef void (*panel_event_proc_t)(Panel_item, Event *);
	Xv_panel_abbrev_window *self = (Xv_panel_abbrev_window *)slf;
	Panel_info *panel = PANEL_PRIVATE(owner);
	Item_info *ip = ITEM_PRIVATE(slf);
	PanelAbbrevWindow_private *priv;

	if (!(priv = (PanelAbbrevWindow_private *)xv_alloc(PanelAbbrevWindow_private)))
		return XV_ERROR;

	priv->public_self = slf;
	self->private_data = (Xv_opaque)priv;

	ip->ops = ops;
	if (panel->event_proc) {
		ip->ops.panel_op_handle_event = (panel_event_proc_t)panel->event_proc;
	}

	return XV_OK;
}

static void end_create(Panel_abbrev_window_item self,
						PanelAbbrevWindow_private *priv)
{
	Item_info *ip = ITEM_PRIVATE(self);
	Graphics_info *ginfo = ip->panel->ginfo;
	Rect vr;

	priv->abbrev_width = AMB_OFFSET + Abbrev_MenuButton_Width(ginfo);

	vr = (ip->value_rect);
	vr.r_width = priv->abbrev_width;
	vr.r_height = Abbrev_MenuButton_Height(ginfo);
	ip->value_rect = vr;
}

static Xv_opaque panabwb_set(Panel_abbrev_window_item self, Attr_avlist avlist)
{
	Attr_attribute *attrs;
	PanelAbbrevWindow_private *priv = PAWBPRIV(self);

	for (attrs=avlist; *attrs; attrs=attr_next(attrs)) switch ((int)*attrs) {
		case PANEL_AWB_WINDOW:
			priv->window = (Xv_opaque)A1;
			ADONE;

		case PANEL_AWB_CREATE_WINDOW_PROC:
			priv->creproc = (creation_proc_t)A1;
			ADONE;

		case XV_END_CREATE:
			end_create(self, priv);
			return XV_OK;

		default: break;
	}

	return XV_OK;
}

static Xv_opaque panabwb_get(Panel_item self, int *status, Attr_attribute attr, va_list vali)
{
	PanelAbbrevWindow_private *priv = PAWBPRIV(self);

	*status = XV_OK;
	switch ((int)attr) {
		case PANEL_AWB_WINDOW:
			return priv->window;
		case PANEL_AWB_CREATE_WINDOW_PROC:
			return (Xv_opaque)priv->creproc;
		default: 
			*status = XV_ERROR;
	}
	return (Xv_opaque)XV_OK;
}

static int panabwb_destroy(Panel_abbrev_window_item self, Destroy_status status)
{
	if (status == DESTROY_CLEANUP) {
		PanelAbbrevWindow_private *priv = PAWBPRIV(self);

		if (priv->window) {
			xv_destroy(priv->window);
			priv->window = XV_NULL;
		}
		xv_free((char *)priv);
	}
	return XV_OK;
}

const Xv_pkg xv_panel_awbtn_pkg = {
	"PanelAbbrevWindowItem",
	ATTR_PKG_PANEL,
	sizeof(Xv_panel_abbrev_window),
	PANEL_ITEM,
	panabwb_init,
	panabwb_set,
	panabwb_get,
	panabwb_destroy,
	0
};
