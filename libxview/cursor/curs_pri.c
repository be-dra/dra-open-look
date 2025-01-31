#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)curs_pri.c 20.39 93/06/28 DRA: RCS  $Id: curs_pri.c,v 2.2 2024/09/15 08:19:07 dra Exp $";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

#include <stdio.h>
#include <X11/Xlib.h>
#include <xview/font.h>
#include <xview/cursor.h>
#include <xview/screen.h>
#include <xview_private/scrn_vis.h>
#include <xview_private/draw_impl.h>
#include <xview_private/i18n_impl.h>
#include <xview_private/curs_impl.h>
#include <xview_private/pw_impl.h>
#include <pixrect/pixrect.h>

Pkg_private void cursor_free_x(Xv_Drawable_info *info, Cursor old_cursor)
{
    XFreeCursor(xv_display(info), old_cursor);
}

Pkg_private long unsigned cursor_make_x(Xv_Drawable_info *root_info, int w,
			int h, int d, int op, int xhot, int yhot,
			XColor *xfg, XColor *xbg, Xv_opaque pr)
{
	Window root = xv_xid(root_info);
	Display *display = xv_display(root_info);
	GC  gc;
	Pixmap src, mask, m;
	Cursor result;
	int oldw = 1, oldh = 1;
	Screen_visual *visual;
	Xv_Drawable_info info;

	if ((w <= 0) || (h <= 0) || (d <= 0)) {
		xv_error(XV_NULL,
				ERROR_STRING, XV_MSG("cannot create cursor with null image"),
				ERROR_PKG, CURSOR,
				NULL);
		return (unsigned long)None;
	}
	/*
	 * handle the case with xhot or yhot bigger than the source pixrect. BUG:
	 * does not handle negative xhot or yhot.
	 */
	if ((xhot < 0) || (yhot < 0))
		xv_error(XV_NULL,
				ERROR_STRING, XV_MSG("cursor_make_x(): bad xhot/yhot parameters"),
				ERROR_PKG, CURSOR,
				NULL);

	if (xhot > w) {
		w = xhot;
	}
	if (yhot > h) {
		h = yhot;
	}
	/* if the cursor op is XOR, create a bigger pixmap for outline cursor */
	if ((op & PIX_NOT(0)) == (PIX_SRC ^ PIX_DST)) {
		oldw = w;
		oldh = h;
		w += 2;
		h += 2;
		xhot++;
		yhot++;
	}
	/*
	 * BUG: both mask and src pixmaps can only be of depth 1
	 */
	src = XCreatePixmap(display, root, (unsigned)w, (unsigned)h, (unsigned)d);
	/* Fake up an info struct to pass to xv_rop_internal */
	info.visual = (Screen_visual *) xv_get(xv_screen(root_info),
			SCREEN_IMAGE_VISUAL, src, 1);
	info.private_gc = 0;
	info.cms = xv_get(xv_screen(root_info), SCREEN_DEFAULT_CMS);

	m = mask =
			XCreatePixmap(display, root, (unsigned)w, (unsigned)h, (unsigned)d);
	visual = (Screen_visual *) xv_get(xv_screen(root_info), SCREEN_IMAGE_VISUAL,
			src, d);
	gc = visual->gc;
	if (!(src && mask && gc)) {
		return (long unsigned)None;
	}
	/* clear the mask since XOR may be used to rop into it */
	XSetFunction(display, gc, GXclear);
	XFillRectangle(display, mask, gc, 0, 0, (unsigned)w, (unsigned)h);
	/* BUG - Clear the src to workaround xnews cursor bug */
	XFillRectangle(display, src, gc, 0, 0, (unsigned)w, (unsigned)h);

	/* PIX_NOT(0) masks out color and PIX_DONTCLIP */
	switch (op & PIX_NOT(0)) {
		case PIX_CLR:
			/* src is already clear, so don't need to touch it. */
			XSetFunction(display, gc, GXclear);
			XFillRectangle(display, src, gc, 0, 0, (unsigned)w, (unsigned)h);
			mask = None;
			break;
		case PIX_SET:
			XSetFunction(display, gc, GXset);
			XFillRectangle(display, src, gc, 0, 0, (unsigned)oldw,
					(unsigned)oldh);
			mask = None;
			break;
		case PIX_DST:
			XSetFunction(display, gc, GXclear);
			XFillRectangle(display, mask, gc, 0, 0, (unsigned)w, (unsigned)h);
			break;
		case PIX_SRC:
			XSetFunction(display, gc, GXcopy);
			xv_rop_internal(display, src, gc, 0, 0, w, h, pr, 0, 0, &info);
			mask = None;
			break;
		case PIX_NOT(PIX_SRC):
			XSetFunction(display, gc, GXcopyInverted);
			xv_rop_internal(display, src, gc, 0, 0, w, h, pr, 0, 0, &info);
			mask = None;
			break;
		case PIX_SRC & PIX_DST:
			XSetFunction(display, gc, GXcopy);
			xv_rop_internal(display, src, gc, 0, 0, w, h, pr, 0, 0, &info);
			XSetFunction(display, gc, GXcopyInverted);
			xv_rop_internal(display, mask, gc, 0, 0, w, h, pr, 0, 0, &info);
			break;
		case PIX_NOT(PIX_SRC) & PIX_DST:
			XSetFunction(display, gc, GXcopyInverted);
			xv_rop_internal(display, src, gc, 0, 0, w, h, pr, 0, 0, &info);
			XSetFunction(display, gc, GXcopy);
			xv_rop_internal(display, mask, gc, 0, 0, w, h, pr, 0, 0, &info);
			break;
		case PIX_NOT(PIX_SRC) | PIX_DST:
			XSetFunction(display, gc, GXcopyInverted);
			xv_rop_internal(display, src, gc, 0, 0, w, h, pr, 0, 0, &info);
			mask = src;
			break;
		case PIX_SRC ^ PIX_DST:{
				short i, j;

				XSetFunction(display, gc, GXcopy);
				xv_rop_internal(display, src, gc, 1, 1, oldw, oldh, pr, 0, 0,
						&info);
				/* Build a mask that is a stencil around the src. */
				XSetFunction(display, gc, GXor);
				for (i = 0; i <= 2; i++) {
					for (j = 0; j <= 2; j++) {
						xv_rop_internal(display, mask, gc, i, j, oldw, oldh, pr,
								0, 0, &info);
					}
				}
				break;
			}
		case PIX_SRC | PIX_DST:
			/* BUG: The following cases can't be done w/o CURSOR_OP in X */
			/* We just pretend that it's the same as PIX_SRC | PIX_DST */
		case PIX_SRC & PIX_NOT(PIX_DST):
		case PIX_NOT(PIX_SRC) & PIX_NOT(PIX_DST):
		case PIX_NOT(PIX_SRC) ^ PIX_DST:
		case PIX_SRC | PIX_NOT(PIX_DST):
		case PIX_NOT(PIX_SRC) | PIX_NOT(PIX_DST):
		case PIX_NOT(PIX_DST):
			XSetFunction(display, gc, GXcopy);
			xv_rop_internal(display, src, gc, 0, 0, w, h, pr, 0, 0, &info);
			mask = src;
			break;
		default:
			xv_error(XV_NULL,
					ERROR_STRING, "cursor_make_x(): unknown rasterop specified",
					ERROR_PKG, CURSOR, NULL);
	}
	/*
	 * WARNING: X server interprets "mask==None" as implying src is mask, but
	 * we want a completely black mask, so we fill it here if appropriate.
	 */
	if (mask == None) {
		/*
		 * PERFORMANCE ALERT!  More complex code could avoid having set the
		 * mask to 0 above when it is going to be unnecessary.
		 */
		mask = m;
		XSetFunction(display, gc, GXset);
		XFillRectangle(display, mask, gc, 0, 0, (unsigned)w, (unsigned)h);
	}
	result = XCreatePixmapCursor(display, src, mask, xfg, xbg, (unsigned)xhot,
			(unsigned)yhot);
	XFreePixmap(display, src);
	XFreePixmap(display, m);
	return ((long unsigned)result);
}


Pkg_private unsigned long cursor_make_x_font(Xv_Drawable_info *root_info, unsigned int src_char, unsigned int mask_char, XColor *xfg, XColor *xbg)
{
    Display        *display = xv_display(root_info);
    Font            x_cursor_font;
    Xv_Font         xview_cursor_font;

    xview_cursor_font = (Xv_Font) xv_find(xv_server(root_info), FONT,
					  FONT_FAMILY, FONT_FAMILY_OLCURSOR,
					  FONT_TYPE, FONT_TYPE_CURSOR,
					  NULL);
    if (!xview_cursor_font)
	xv_error(XV_NULL,
		 ERROR_STRING, 
		 XV_MSG("Unable to find OPEN LOOK cursor font"),
		 ERROR_PKG, CURSOR,
		 NULL);
    x_cursor_font = (Font) xv_get(xview_cursor_font, XV_XID);
    if (mask_char == 0) {
	mask_char = src_char;
    }
    return (XCreateGlyphCursor(display, x_cursor_font, x_cursor_font,
		src_char, mask_char, xfg, xbg));
}


Pkg_private void cursor_set_cursor_internal(Xv_Drawable_info *info, Cursor cursor)
{
    XDefineCursor(xv_display(info), xv_xid(info), cursor);
}
