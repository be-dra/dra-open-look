#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)cursor.c 20.55 93/06/28 DRA: RCS  $Id: cursor.c,v 2.7 2025/03/11 17:22:02 dra Exp $";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL_NOTICE 
 *	file for terms of the license.
 */

/*
 * cursor.c: Routines for creating & modifying a cursor.
 * 
 */

#include <X11/Xlib.h>
#include <xview_private/portable.h>
#include <xview_private/curs_impl.h>
#include <xview_private/attr_impl.h>
#include <xview_private/pw_impl.h>
#include <xview/font.h>
#include <xview/notify.h>
#include <xview/svrimage.h>
#include <xview/window.h>
#include <xview/screen.h>

static Xv_opaque create_text_cursor(Cursor_info *cursor,Xv_Drawable_info *info);

#define CURSOR_TEXT_XHOT 9
#define CURSOR_TEXT_YHOT 9

static int cursor_create_internal(Xv_Screen parent, Xv_Cursor object,
							Attr_avlist avlist, int *u)
/*
 * Parent should be either a window or a screen, or any object that will
 * return the root window in response to xv_get(parent, XV_ROOT).
 */
{
	register Cursor_info *cursor;
	register Pixrect *pr;
	Cursor_info *other_cursor;
	Attr_avlist copy_attr;

	((Xv_cursor_struct *) (object))->private_data =
										(Xv_opaque) xv_alloc(Cursor_info);
	if (!(cursor = CURSOR_PRIVATE(object))) {
		return XV_ERROR;
	}
	cursor->public_self = object;
	cursor->cur_src_char = NOFONTCURSOR;
	/* Use default screen if none given (xv_create ensures default ok) */
	cursor->root = xv_get((parent ? parent : xv_default_screen), XV_ROOT);

	copy_attr = attr_find(avlist, (Attr_attribute) XV_COPY_OF);
	if (*copy_attr) {
		other_cursor = CURSOR_PRIVATE(copy_attr[1]);
		*cursor = *other_cursor;

		/* Allocate new shape, copy old, and flag need to free new shape. */
		pr = other_cursor->cur_shape;
		cursor->cur_shape =
				(Pixrect *) xv_create(xv_get(other_cursor->root, XV_SCREEN),
				SERVER_IMAGE,
				XV_WIDTH, pr->pr_width,
				XV_HEIGHT, pr->pr_height,
				SERVER_IMAGE_DEPTH, pr->pr_depth,
				NULL);
		if (!cursor->cur_shape) return XV_ERROR;

		xv_rop((Xv_opaque) cursor->cur_shape, 0, 0, pr->pr_width, pr->pr_height,
				PIX_SRC, pr, 0, 0);
		cursor->flags |= FREE_SHAPE;
		cursor->cur_xhot = other_cursor->cur_xhot;
		cursor->cur_yhot = other_cursor->cur_yhot;
		cursor->cur_src_char = other_cursor->cur_src_char;
		cursor->cur_mask_char = other_cursor->cur_mask_char;
		cursor->cur_function = other_cursor->cur_function;
	}
	else {
		cursor->cur_function = PIX_SRC | PIX_DST;
		cursor->cur_shape =
				(Pixrect *) xv_create(xv_get(cursor->root, XV_SCREEN),
							SERVER_IMAGE,
							XV_WIDTH, CURSOR_MAX_IMAGE_WORDS,
							XV_HEIGHT, CURSOR_MAX_IMAGE_WORDS,
							SERVER_IMAGE_DEPTH, 1,
							NULL);
		cursor->flags = FREE_SHAPE;
	}
	/* the id will be set the first time through cursor_set() */
	cursor->cursor_id = 0;

	/* default foreground/background color */
	cursor->fg.red = 0;
	cursor->fg.green = 0;
	cursor->fg.blue = 0;
	cursor->bg.red = 255;
	cursor->bg.green = 255;
	cursor->bg.blue = 255;

	cursor->type = CURSOR_TYPE_PIXMAP;
	cursor->drag_state = CURSOR_NEUTRAL;
	cursor->drag_type = CURSOR_MOVE;

	xv_set(object, XV_RESET_REF_COUNT, NULL);	/* Mark as ref counted. */

	return XV_OK;
}


static int cursor_destroy_internal(Xv_Cursor cursor_public,
									Destroy_status status)
{
	Cursor_info *cursor = CURSOR_PRIVATE(cursor_public);
	Xv_Drawable_info *info;

	if (status == DESTROY_CLEANUP) {
		if (free_shape(cursor))
			xv_destroy((Xv_opaque) (cursor->cur_shape));
		if (cursor->type == CURSOR_TYPE_TEXT && cursor->cursor_id) {
			DRAWABLE_INFO_MACRO(cursor->root, info);
			XFreeCursor(xv_display(info), cursor->cursor_id);

#ifdef OW_I18N
			_xv_free_ps_string_attr_dup(&cursor->string);
#endif
		}
		free((char *)cursor);
	}
	return XV_OK;
}

static Xv_opaque cursor_get_internal(Xv_Cursor cursor_public, int *status,
							Attr_attribute which_attr, va_list args)
{
	register Cursor_info *cursor = CURSOR_PRIVATE(cursor_public);

	switch (which_attr) {
		case XV_XID:
			return (Xv_opaque) cursor->cursor_id;

		case XV_SHOW:
			return (Xv_opaque) show_cursor(cursor);

		case CURSOR_STRING:

#ifndef OW_I18N
			return (Xv_opaque) cursor->string;
#else
			return (Xv_opaque) _xv_get_mbs_attr_dup(&cursor->string);

		case CURSOR_STRING_WCS:
			return (Xv_opaque) _xv_get_wcs_attr_dup(&cursor->string);
#endif /* OW_I18N */

		case CURSOR_DRAG_STATE:
			return (Xv_opaque) cursor->drag_state;

		case CURSOR_DRAG_TYPE:
			return (Xv_opaque) cursor->drag_type;

		case CURSOR_SRC_CHAR:
			return (Xv_opaque) cursor->cur_src_char;

		case CURSOR_MASK_CHAR:
			return (Xv_opaque) cursor->cur_mask_char;

		case CURSOR_XHOT:
			return (Xv_opaque) cursor->cur_xhot;

		case CURSOR_YHOT:
			return (Xv_opaque) cursor->cur_yhot;

		case CURSOR_OP:
			return (Xv_opaque) cursor->cur_function;

		case CURSOR_IMAGE:
			return (Xv_opaque) cursor->cur_shape;

		case CURSOR_FOREGROUND_COLOR:
			return ((Xv_opaque) & cursor->fg);

		case CURSOR_BACKGROUND_COLOR:
			return ((Xv_opaque) & cursor->bg);

		default:
			if (xv_check_bad_attr(CURSOR, which_attr) == XV_ERROR) {
				*status = XV_ERROR;
			}
			return XV_NULL;
	}

}

/* cursor_set_attr sets the attributes mentioned in avlist. */
static Xv_opaque cursor_set_internal(Xv_Cursor cursor_public,
								Attr_avlist avlist)
{
	register Cursor_info *cursor = CURSOR_PRIVATE(cursor_public);
	register Pixrect *pr;
	register int dirty = FALSE;
	Xv_Drawable_info *root_info;
	register Xv_opaque arg1;
	int end_create = FALSE;
	Xv_singlecolor *fg = NULL, *bg = NULL;
	XColor xfg, xbg;

	for (; *avlist; avlist = attr_next(avlist)) {
		arg1 = avlist[1];
		switch (*avlist) {
			case XV_SHOW:
				/* BUG: is this used anywhere? */
				if ((int)arg1)
					cursor->flags &= ~DONT_SHOW_CURSOR;
				else
					cursor->flags |= DONT_SHOW_CURSOR;
				dirty = TRUE;
				break;

			case CURSOR_STRING:

#ifdef OW_I18N
				_xv_set_mbs_attr_dup(&cursor->string, (char *)arg1);
#else
				cursor->string = (char *)arg1;
#endif

				cursor->type = CURSOR_TYPE_TEXT;
				ATTR_CONSUME(avlist[0]);
				break;

#ifdef OW_I18N
			case CURSOR_STRING_WCS:
				_xv_set_wcs_attr_dup(&cursor->string, (CHAR *) arg1);
				cursor->type = CURSOR_TYPE_TEXT;
				ATTR_CONSUME(avlist[0]);
				break;
#endif

			case CURSOR_DRAG_STATE:
				cursor->drag_state = (Cursor_drag_state) arg1;
				ATTR_CONSUME(avlist[0]);
				break;

			case CURSOR_DRAG_TYPE:
				cursor->drag_type = (Cursor_drag_type) arg1;
				ATTR_CONSUME(avlist[0]);
				break;

			case CURSOR_SRC_CHAR:
				cursor->cur_src_char = (unsigned int)arg1;
				cursor->type = CURSOR_TYPE_GLYPH;
				dirty = TRUE;
				ATTR_CONSUME(avlist[0]);
				break;

			case CURSOR_MASK_CHAR:
				cursor->cur_mask_char = (unsigned int)arg1;
				dirty = TRUE;
				ATTR_CONSUME(avlist[0]);
				break;

			case CURSOR_XHOT:
				cursor->cur_xhot = (int)arg1;
				dirty = TRUE;
				ATTR_CONSUME(avlist[0]);
				break;

			case CURSOR_YHOT:
				cursor->cur_yhot = (int)arg1;
				dirty = TRUE;
				ATTR_CONSUME(avlist[0]);
				break;

			case CURSOR_OP:
				cursor->cur_function = (int)arg1;
				dirty = TRUE;
				ATTR_CONSUME(avlist[0]);
				break;

			case CURSOR_IMAGE:
				if (free_shape(cursor)) {
					/* destroy the remote image */
					xv_destroy((Xv_opaque) (cursor->cur_shape));
					cursor->flags &= ~FREE_SHAPE;
				}
				cursor->cur_shape = (Pixrect *) arg1;
				cursor->type = CURSOR_TYPE_PIXMAP;
				dirty = TRUE;
				ATTR_CONSUME(avlist[0]);
				break;

			case CURSOR_FOREGROUND_COLOR:
				fg = (Xv_singlecolor *) arg1;
				cursor->fg.red = fg->red;
				cursor->fg.green = fg->green;
				cursor->fg.blue = fg->blue;
				ATTR_CONSUME(avlist[0]);
				break;

			case CURSOR_BACKGROUND_COLOR:
				bg = (Xv_singlecolor *) arg1;
				cursor->bg.red = bg->red;
				cursor->bg.green = bg->green;
				cursor->bg.blue = bg->blue;
				ATTR_CONSUME(avlist[0]);
				break;

			case XV_COPY_OF:
				dirty = TRUE;
				break;

			case XV_END_CREATE:
				end_create = TRUE;
				break;

			default:
				(void)xv_check_bad_attr(CURSOR, *avlist);
				break;
		}
	}

	DRAWABLE_INFO_MACRO(cursor->root, root_info);

	if (end_create && cursor->type == CURSOR_TYPE_TEXT) {
		return create_text_cursor(cursor, root_info);
	}

	xfg.red = cursor->fg.red << 8;
	xfg.green = cursor->fg.green << 8;
	xfg.blue = cursor->fg.blue << 8;
	xfg.flags = DoRed | DoGreen | DoBlue;
	xbg.red = cursor->bg.red << 8;
	xbg.green = cursor->bg.green << 8;
	xbg.blue = cursor->bg.blue << 8;
	xbg.flags = DoRed | DoGreen | DoBlue;

	if (!dirty) {
		if (fg || bg) {
			XRecolorCursor(xv_display(root_info), cursor->cursor_id,
					&xfg, &xbg);
		}
		return XV_OK;
	}

	/* make the cursor now */
	if (cursor->cursor_id) {
		cursor_free_x(root_info, cursor->cursor_id);
	}
	if (cursor->cur_src_char != NOFONTCURSOR) {
		cursor->cursor_id = cursor_make_x_font(root_info,
				(unsigned int)cursor->cur_src_char,
				(unsigned int)cursor->cur_mask_char, &xfg, &xbg);
	}
	else {
		pr = cursor->cur_shape;

		cursor->cursor_id = cursor_make_x(root_info,
				pr->pr_size.x, pr->pr_size.y,
				pr->pr_depth, cursor->cur_function,
				cursor->cur_xhot, cursor->cur_yhot, &xfg, &xbg, (Xv_opaque) pr);
	}

	/* BUG: ok to abort? */
	if (!cursor->cursor_id) {
		xv_error((Xv_object) cursor,
				ERROR_STRING, XV_MSG("cursor: can't create cursor"),
				ERROR_PKG, CURSOR,
				NULL);
	}
	return (Xv_opaque) XV_OK;
}

Xv_private void
cursor_set_cursor(window, cursor_public)
    Xv_object       window;
    Xv_Cursor       cursor_public;
{
    Cursor_info    *cursor = CURSOR_PRIVATE(cursor_public);
    Xv_Drawable_info *window_info;

    if (xv_get(window, XV_ROOT) != cursor->root) {
	xv_error((Xv_object)cursor,
		 ERROR_STRING,
		   XV_MSG("Window and cursor have different roots! Can't set cursor"),
		 ERROR_PKG, CURSOR,
		 NULL);
    } else {
	DRAWABLE_INFO_MACRO(window, window_info);
	cursor_set_cursor_internal(window_info, cursor->cursor_id);
    }
}

/* returns XV_OK or XV_ERROR */
static Xv_opaque create_text_cursor(Cursor_info *cursor, Xv_Drawable_info *info)
{
	/* The cursor table indices are [drag_state][drag_type] */
	static int cursor_table[3][2] = {
		{ OLC_TEXT_MOVE_DRAG, OLC_TEXT_COPY_DRAG },
		{ OLC_TEXT_MOVE_INSERT, OLC_TEXT_COPY_INSERT },
		{ OLC_TEXT_MOVE_NODROP,	OLC_TEXT_COPY_NODROP }
	};

	unsigned int best_height;
	unsigned int best_width;
	XColor bg;	/* background color of cursor */
	XColor fg;	/* foreground color of cursor */
	Colormap cmap;
	int src_char;
	Display *display;
	Xv_Font textfont, cursor_font;
	int length;
	int screen_nbr;
	Pixmap mask_pixmap, src_pixmap;
	Screen_visual *visual;
	Status status;
	XID xid;
	XFontStruct *xfs;
    int descent = 0;
    int ascent = 0;
    int direction = 0;
    XCharStruct chstr;
	unsigned pixheight, pixwidth;
	char buf[2];

	display = xv_display(info);
	xid = xv_xid(info);

#ifdef OW_I18N
	length = STRLEN(cursor->string.pswcs.value);
#else
	length = strlen(cursor->string);
#endif

	src_char = cursor_table[cursor->drag_state][cursor->drag_type];

    cursor_font = (Xv_Font) xv_find(xv_server(info), FONT,
					  FONT_FAMILY, FONT_FAMILY_OLCURSOR,
					  FONT_TYPE, FONT_TYPE_CURSOR,
					  NULL);
    if (!cursor_font)
		xv_error(XV_NULL,
					ERROR_STRING,XV_MSG("Unable to find OPEN LOOK cursor font"),
					ERROR_PKG, CURSOR,
					NULL);

	buf[0] = src_char;
	buf[1] = '\0';
	xfs = (XFontStruct *)xv_get(cursor_font, FONT_INFO);
	if (xfs->max_char_or_byte2 < 0x7c) {
		/* old open look cursor font */

		Xv_Drawable_info *root_info;
		XColor xfg, xbg;

		DRAWABLE_INFO_MACRO(cursor->root, root_info);
		xfg.red = cursor->fg.red << 8;
		xfg.green = cursor->fg.green << 8;
		xfg.blue = cursor->fg.blue << 8;
		xfg.flags = DoRed | DoGreen | DoBlue;
		xbg.red = cursor->bg.red << 8;
		xbg.green = cursor->bg.green << 8;
		xbg.blue = cursor->bg.blue << 8;
		xbg.flags = DoRed | DoGreen | DoBlue;

		cursor->cursor_id = cursor_make_x_font(root_info,
				(unsigned int)OLC_COPY_PTR, (unsigned int)OLC_COPY_PTR + 1,
				&xfg, &xbg);
		return XV_OK;
	}

	XTextExtents(xfs, buf, 1, &direction, &ascent, &descent, &chstr);

	pixheight = (unsigned) (ascent+descent+10);
	pixwidth = chstr.width;

	/* See if we can create a cursor of this size */
	status = XQueryBestCursor(display, xid, pixwidth,
			pixheight, &best_width, &best_height);
	if (!status || best_width < pixwidth || best_height < pixheight)
		return XV_ERROR;

	/* Create mask and source pixmaps */
	mask_pixmap = XCreatePixmap(display, xid, pixwidth, pixheight, 1);
	src_pixmap = XCreatePixmap(display, xid, pixwidth, pixheight, 1);

	/* Draw text into source pixmap */
	visual = (Screen_visual *) xv_get(xv_screen(info),
			SCREEN_IMAGE_VISUAL, src_pixmap, 1);

	XSetForeground(display, visual->gc, 0L);
	XSetFont(display, visual->gc, xv_get(cursor_font, XV_XID));
	XSetFillStyle(display, visual->gc, FillSolid);

	XFillRectangle(display, mask_pixmap, visual->gc, 0, 0, pixwidth, pixheight);
	XFillRectangle(display, src_pixmap, visual->gc, 0, 0, pixwidth, pixheight);
	XSetForeground(display, visual->gc, 1L);

	XDrawString(display,src_pixmap,visual->gc, -chstr.lbearing, ascent, buf, 1);
	buf[0] = src_char + 1;
	XDrawString(display,mask_pixmap,visual->gc,-chstr.lbearing, ascent, buf, 1);

	/* the cursor_font reports size -66, scale 0, ascent 12 -
	 * let's use the ascent as the font size of the text font:
	 * better than the 'FONT_SIZE, FONT_SIZE_DEFAULT' as it was originally
	 */
	textfont = xv_find(xv_server(info), FONT,
			FONT_FAMILY, FONT_FAMILY_DEFAULT_FIXEDWIDTH,
			FONT_STYLE, FONT_STYLE_DEFAULT,
			FONT_SIZE, ascent,
			NULL);
	if (!textfont)
		return XV_ERROR;

	XSetFont(display, visual->gc, xv_get(textfont, XV_XID));

	if (length <= 5) {
		XSetForeground(display, visual->gc, 0L);
		/* erase the "more arrow" */
		XFillRectangle(display, src_pixmap, visual->gc, 51, 21, 6, 12);
		XSetForeground(display, visual->gc, 1L);
	}
	else {
		/* need space for the more arrow */
		length = 4;
	}

	/* Draw string into cursor pixmap */

#ifdef OW_I18N
	XwcDrawString(display, src_pixmap, (XFontSet) xv_get(textfont, FONT_SET_ID),
			visual->gc, 20, ascent+descent+3,
			cursor->string.pswcs.value, length);
#else
	XDrawString(display,src_pixmap,visual->gc, 20, ascent+descent+3,
									cursor->string, length);
#endif

	/* Define foreground and background colors */
	screen_nbr = (int)xv_get(xv_screen(info), SCREEN_NUMBER);
	fg.flags = bg.flags = DoRed | DoGreen | DoBlue;
	fg.pixel = BlackPixel(display, screen_nbr);
	cmap = xv_get(xv_cms(info), XV_XID);
	XQueryColor(display, cmap, &fg);
	bg.pixel = WhitePixel(display, screen_nbr);
	XQueryColor(display, cmap, &bg);

	/* Create Pixmap Cursor */
	cursor->cursor_id = XCreatePixmapCursor(display, src_pixmap, mask_pixmap,
			&fg, &bg, (unsigned)(-chstr.lbearing), (unsigned)ascent);

	/* Free the src_pixmap and mask_pixmap */

	XFreePixmap(display, src_pixmap);
	XFreePixmap(display, mask_pixmap);

	if (cursor->cursor_id)
		return XV_OK;
	else
		return XV_ERROR;
}


const Xv_pkg          xv_cursor_pkg = {
    "Cursor",			/* seal -> package name */
    ATTR_PKG_CURSOR,		/* cursor attr */
    sizeof(Xv_cursor_struct),	/* size of the cursor data struct */
    &xv_generic_pkg,		/* pointer to parent */
    cursor_create_internal,	/* init routine for cursor */
    cursor_set_internal,
    cursor_get_internal,
    cursor_destroy_internal,
    NULL			/* no find proc */
};
