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
#include <xview/cms.h>
#include <xview/scrollbar.h>
#include <xview/pixscr.h>
#include <xview/filereq.h>
#include <xview/dragdrop.h>

char pixscr_c_sccsid[] = "@(#) %M% V%I% %E% %U% $Id: pixscr.c,v 4.2 2025/03/08 13:37:48 dra Exp $";

#define A0 *attrs
#define A1 attrs[1]
#define A2 attrs[2]

#define ADONE ATTR_CONSUME(*attrs);break

typedef void (*layout_proc_t)(Pixmap_scroller, int, int);
typedef void (*drop_proc_t)(Pixmap_scroller, char **, int);

typedef struct {
	Xv_opaque               public_self;
	Pixmap                  pixmap;
	layout_proc_t           layout_proc;
	drop_proc_t             drop_proc;
	int                     width, height;
	char                    center_image, is_bitmap, expand_frame;
	GC                      gc;
} Pixscr_private;

#define PIXSCRPRIV(_x_) XV_PRIVATE(Pixscr_private, Xv_pixmap_scroller, _x_)
#define PIXSCRPUB(_x_) XV_PUBLIC(_x_)

static void pixscr_repaint(Pixscr_private *priv, Scrollwin_repaint_struct *rs)
{
	Scrollpw_info *vi = rs->vinfo;

	if (! priv->gc) return;

	if (priv->pixmap) {
		if (priv->is_bitmap) {
			XCopyPlane(vi->dpy, priv->pixmap, vi->xid, priv->gc,
					rs->win_rect.r_left + vi->scr_x,
					rs->win_rect.r_top + vi->scr_y,
					(unsigned)rs->win_rect.r_width,
					(unsigned)rs->win_rect.r_height,
					rs->win_rect.r_left, rs->win_rect.r_top, 1L);
		}
		else {
			XCopyArea(vi->dpy, priv->pixmap, vi->xid, priv->gc,
					rs->win_rect.r_left + vi->scr_x,
					rs->win_rect.r_top + vi->scr_y,
					(unsigned)rs->win_rect.r_width,
					(unsigned)rs->win_rect.r_height,
					rs->win_rect.r_left, rs->win_rect.r_top);
		}
	}
	else {
		XClearWindow(vi->dpy, vi->xid);
	}
}

static void scroll_to_center(Pixscr_private *priv, Scrollpw pw)
{
	Scrollview view = xv_get(pw, XV_OWNER);
	Scrollbar sb;

	xv_set(view,
			SCROLLVIEW_H_START, (priv->width - (int)xv_get(pw, XV_WIDTH)) / 2,
			SCROLLVIEW_V_START, (priv->height - (int)xv_get(pw, XV_HEIGHT)) / 2,
			NULL);

	sb = xv_get(PIXSCRPUB(priv), OPENWIN_VERTICAL_SCROLLBAR, view);
	if (sb) {
		XClearWindow((Display *)xv_get(sb, XV_DISPLAY),
						(Window)xv_get(sb, XV_XID));
		scrollbar_paint(sb);
	}

	sb = xv_get(PIXSCRPUB(priv), OPENWIN_HORIZONTAL_SCROLLBAR, view);
	if (sb) {
		XClearWindow((Display *)xv_get(sb, XV_DISPLAY),
						(Window)xv_get(sb, XV_XID));
		scrollbar_paint(sb);
	}
}

static void handle_event(Pixscr_private *priv, Scrollwin_event_struct *es)
{
	if (es->action == WIN_RESIZE) {
		if (priv->center_image) {
			scroll_to_center(priv, es->pw);
		}
	}
}

static void manage_scrollbars(Pixscr_private *priv, int fw, int fh)
{
	Pixmap_scroller self = PIXSCRPUB(priv);

	/* if the view is split, we don't touch the scrollbars */
	if ((int)xv_get(self, OPENWIN_NVIEWS) == 1) {
		int need_vsb, need_hsb;
		Scrollbar sb;
		Xv_window view = xv_get(self, OPENWIN_NTH_VIEW, 0);

		need_hsb = (priv->width > fw);
		need_vsb = (priv->height > fh);

		sb = xv_get(self, OPENWIN_VERTICAL_SCROLLBAR, view);
		if (need_vsb) {
			if (! sb) xv_create(self, SCROLLBAR,
									SCROLLBAR_DIRECTION, SCROLLBAR_VERTICAL,
									SCROLLBAR_SPLITTABLE, TRUE,
									NULL);
		}
		else {
			if (sb) xv_destroy(sb);
		}

		sb = xv_get(self, OPENWIN_HORIZONTAL_SCROLLBAR, view);
		if (need_hsb) {
			if (! sb) xv_create(self, SCROLLBAR,
									SCROLLBAR_DIRECTION, SCROLLBAR_HORIZONTAL,
									SCROLLBAR_SPLITTABLE, TRUE,
									NULL);
		}
		else {
			if (sb) xv_destroy(sb);
		}
	}
}

static void own_layout_proc(Pixmap_scroller self, int w, int h)
{
	xv_set(xv_get(self, WIN_FRAME),
				XV_WIDTH, w,
				XV_HEIGHT, h,
				NULL);
}

static void new_layout(Pixscr_private *priv)
{
	Pixmap_scroller self = PIXSCRPUB(priv);
	Xv_window root = xv_get(self, XV_ROOT);
	int rw = (int)xv_get(root, XV_WIDTH) - 300;
	int rh = (int)xv_get(root, XV_HEIGHT) - 300;
	int fw, fh;
	int vu, hu;

	if (priv->expand_frame) {
		fw = MIN(rw, priv->width) + 2 * WIN_DEFAULT_BORDER_WIDTH;
		fh = MIN(rh, priv->height) + 2 * WIN_DEFAULT_BORDER_WIDTH;
		(*(priv->layout_proc))(self, fw, fh);
	}
	else {
		fw = (int)xv_get(xv_get(self, WIN_FRAME), XV_WIDTH);
		fh = (int)xv_get(xv_get(self, WIN_FRAME), XV_HEIGHT);
	}

	manage_scrollbars(priv, fw, fh);

	vu = (int)xv_get(self, SCROLLWIN_V_UNIT);
	hu = (int)xv_get(self, SCROLLWIN_H_UNIT);

	xv_set(self,
			SCROLLWIN_V_OBJECT_LENGTH, (int)(priv->height / vu) + 1,
			SCROLLWIN_H_OBJECT_LENGTH, (int)(priv->width / hu) + 1,
			NULL);
	if (priv->center_image) {
		Scrollpw pw;

		OPENWIN_EACH_PW(self, pw)
			scroll_to_center(priv, pw);
		OPENWIN_END_EACH
	}
	xv_set(self, SCROLLWIN_TRIGGER_REPAINT, NULL);
}

static Notify_value pixscr_events(Xv_opaque self, Event *ev, Notify_arg arg, Notify_event_type type)
{
	Pixscr_private *priv = PIXSCRPRIV(self);

/* 	DTRACE_EVENT(DTL_LAYOUT, "pixscr_events", ev); */

	if (event_action(ev) == WIN_RESIZE) {
		Notify_value val;

		val = notify_next_event_func(self, (Notify_event)ev, arg, type);
		manage_scrollbars(priv,
				(int)xv_get(xv_get(self, WIN_FRAME), XV_WIDTH),
				(int)xv_get(xv_get(self, WIN_FRAME), XV_HEIGHT));

		return val;
	}

	return notify_next_event_func(self, (Notify_event)ev, arg, type);
}

static void pixscr_make_gc(Pixscr_private *priv, Xv_window pw)
{
	Pixmap_scroller self = PIXSCRPUB(priv);
	Display *dpy = (Display *)xv_get(self, XV_DISPLAY);
	Window xid;
	XGCValues gcv;
	Cms cms;
	int fore_index, back_index;
	unsigned long fg, bg;

	/* cleanup old gc, if already there */
	if (priv->gc) XFreeGC(dpy, priv->gc);

	xid = (Window)xv_get(pw, XV_XID);
	fore_index = (int)xv_get(pw, WIN_FOREGROUND_COLOR);
	back_index = (int)xv_get(pw, WIN_BACKGROUND_COLOR);
	cms = xv_get(pw, WIN_CMS);
	bg = (unsigned long)xv_get(cms, CMS_PIXEL, back_index);
	fg = (unsigned long)xv_get(cms, CMS_PIXEL, fore_index);
	gcv.foreground = fg;
	gcv.background = bg;
	gcv.graphics_exposures = FALSE;

	priv->gc = XCreateGC(dpy, xid,
				GCGraphicsExposures | GCForeground | GCBackground, &gcv);
}

static void pixscr_end_of_creation(Pixscr_private *priv)
{
	pixscr_make_gc(priv, xv_get(PIXSCRPUB(priv), OPENWIN_NTH_PW, 0));
	xv_set(PIXSCRPUB(priv),
			WIN_X_EVENT_MASK, StructureNotifyMask,
			WIN_NOTIFY_SAFE_EVENT_PROC, pixscr_events,
			NULL);
}

static Xv_opaque pixscr_set(Pixmap_scroller self, Attr_avlist avlist)
{
	Attr_attribute *attrs;
	Pixscr_private *priv = PIXSCRPRIV(self);
	int need_layout = FALSE;
	Scrollwin_drop_struct *drop;

	for (attrs=avlist; *attrs; attrs=attr_next(attrs)) switch ((int)*attrs) {
		case PIXSCR_EXPAND_FRAME:
			priv->expand_frame = (char)A1;
			if (priv->expand_frame && priv->pixmap) new_layout(priv);
			ADONE;

		case PIXSCR_LAYOUT_PROC:
			priv->layout_proc = (layout_proc_t)A1;
			if (!priv->layout_proc) priv->layout_proc = own_layout_proc;
			ADONE;

		case PIXSCR_CENTER_IMAGE:
			priv->center_image = (char)A1;
			need_layout = TRUE;
			ADONE;

		case PIXSCR_PIXMAP:
			priv->pixmap = (Pixmap)A1;
			if (priv->pixmap) {
				Window root;
				int ux, uy;
				unsigned int wid, hig, ubw, depth;

				XGetGeometry((Display *)xv_get(self, XV_DISPLAY),
								priv->pixmap, &root, &ux, &uy,
								&wid, &hig, &ubw, &depth);

				priv->width = (int)wid;
				priv->height = (int)hig;
				priv->is_bitmap = (depth < 2);
			}
			need_layout = TRUE;
			ADONE;

		case PIXSCR_DROP_PROC:
			priv->drop_proc = (drop_proc_t)A1;
			ADONE;

		case PIXSCR_DO_LAYOUT:
			need_layout = TRUE;
			ADONE;

		case SCROLLWIN_HANDLE_DROP:
			drop = (Scrollwin_drop_struct *)A1;
			if (priv->drop_proc)
				(*(priv->drop_proc))(self, drop->files, drop->cnt);
			ADONE;

		case SCROLLWIN_HANDLE_EVENT:
			handle_event(priv, (Scrollwin_event_struct *)A1);
			break;

		case SCROLLWIN_DROP_EVENT:
			{
				Scrollwin_drop_struct *drop = (Scrollwin_drop_struct *)A1;

				xv_set(drop->sel_req,
							FILE_REQ_ALLOCATE, FALSE,
							FILE_REQ_ALREADY_DECODED, TRUE,
							FILE_REQ_FETCH, drop->event,
							NULL);
				drop->files = (char **)xv_get(drop->sel_req, FILE_REQ_FILES,
											&drop->cnt);

				xv_set(self, SCROLLWIN_HANDLE_DROP, drop, NULL);

				dnd_done(drop->sel_req);
			}
			ADONE;

		case SCROLLWIN_REPAINT:
			pixscr_repaint(priv, (Scrollwin_repaint_struct *)A1);
			ADONE;

		case SCROLLWIN_PW_CMS_CHANGED:
			pixscr_make_gc(priv, (Xv_window)A1);
			break;

		case XV_END_CREATE:
			pixscr_end_of_creation(priv);
			break;

		default:
			xv_check_bad_attr(PIXMAP_SCROLLER, A0);
	}

	if (need_layout) new_layout(priv);

	return XV_OK;
}

/* ARGSUSED */
static Xv_opaque pixscr_get(Pixmap_scroller self, int *status, Attr_attribute attr, va_list vali)
{
	Pixscr_private *priv = PIXSCRPRIV(self);

	*status = XV_OK;
	switch ((int)attr) {
		case PIXSCR_EXPAND_FRAME: return (Xv_opaque)priv->expand_frame;
		case PIXSCR_PIXMAP: return (Xv_opaque)priv->pixmap;
		case PIXSCR_CENTER_IMAGE: return (Xv_opaque)priv->center_image;
		case PIXSCR_LAYOUT_PROC: return (Xv_opaque)priv->layout_proc;
		case PIXSCR_DROP_PROC: return (Xv_opaque)priv->drop_proc;
		case SCROLLWIN_CREATE_SEL_REQ:
			return xv_create(self, FILE_REQUESTOR,
						FILE_REQ_CHECK_ACCESS, TRUE,
						FILE_REQ_USE_LOAD, TRUE,
						NULL);
		default:
			*status = xv_check_bad_attr(PIXMAP_SCROLLER, attr);
			return (Xv_opaque)XV_OK;
	}
}

/* ARGSUSED */
static int pixscr_init(Xv_opaque owner, Xv_opaque slf, Attr_avlist avlist, int *u)
{
	Xv_pixmap_scroller *self = (Xv_pixmap_scroller *)slf;
	Pixscr_private *priv;

	priv = (Pixscr_private *)xv_alloc(Pixscr_private);
	if (!priv) return XV_ERROR;

	priv->public_self = (Xv_opaque)self;
	self->private_data = (Xv_opaque)priv;

	priv->layout_proc = own_layout_proc;

	return XV_OK;
}

/*ARGSUSED*/
static int pixscr_destroy(Pixmap_scroller self, Destroy_status status)
{
	if (status == DESTROY_CLEANUP) {
		Pixscr_private *priv = PIXSCRPRIV(self);
		Display *dpy = (Display *)xv_get(self, XV_DISPLAY);

/* 		DTRACE(DTL_INTERN+10, "in pixscr_destroy(%x)\n", self); */
		if (priv->gc) XFreeGC(dpy, priv->gc);
		free((char *)priv);
	}
	return XV_OK;
}

const Xv_pkg xv_pixmap_scroller_pkg = {
	"PixmapScroller",
	ATTR_PKG_PIXMAP_SCROLLER,
	sizeof(Xv_pixmap_scroller),
	SCROLLWIN,
	pixscr_init,
	pixscr_set,
	pixscr_get,
	pixscr_destroy,
	0
};
