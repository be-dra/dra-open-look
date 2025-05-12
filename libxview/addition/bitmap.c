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
#include <xview/bitmap.h>
#include <xview/icon_load.h>
#include <X11/Xutil.h>
#if defined(sun) && defined(BSD)
#  include <rasterfile.h>
#else
#  include <pixrect/rasterfile.h>
#endif
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

char bitmap_c_sccsid[] = "@(#) %M% V%I% %E% %U% $Id: bitmap.c,v 4.5 2025/05/11 10:38:31 dra Exp $";

typedef struct _bm_priv {
	Xv_opaque           public_self;
	char               *file;
	Bitmap_file_type    type;

	Xv_screen           screen;
	Display            *dpy;
	GC                  gc;
	struct _bm_priv    *next;
} Bitmap_private;

#define BITPRIV(_x_) XV_PRIVATE(Bitmap_private, Xv_bitmap, _x_)
#define BITPUB(_x_) XV_PUBLIC(_x_)

#define A0 *attrs
#define A1 attrs[1]
#define A2 attrs[2]

#define IMIN(a, b) ((a) > (b) ? (b) : (a))
#define ADONE ATTR_CONSUME(*attrs);break

static int bm_key = 0;

static void flood_fill(XImage *xima, int x, int y)
{
	if (x < 0 || y < 0 || x >= xima->width || y >= xima->height) return;
	if (XGetPixel(xima, x, y) != 0) return;

	XPutPixel(xima, x, y, 1L);
	flood_fill(xima, x+1, y);
	flood_fill(xima, x-1, y);
	flood_fill(xima, x, y+1);
	flood_fill(xima, x, y-1);
}

static void fill_for_mask(Bitmap_private *priv)
{
	XImage *xima;
	Pixmap pix = (Pixmap)xv_get(BITPUB(priv), XV_XID);
	unsigned wid = (unsigned)xv_get(BITPUB(priv), XV_WIDTH);
	unsigned hig = (unsigned)xv_get(BITPUB(priv), XV_HEIGHT);
	int i;

	xima = XGetImage(priv->dpy, pix, 0, 0, wid, hig, 1L, XYPixmap);

	for (i = 0; i < wid; i++) {
		flood_fill(xima, i, 0);
		flood_fill(xima, i, (int)hig-1);
	}
	for (i = 0; i < hig; i++) {
		flood_fill(xima, 0, i);
		flood_fill(xima, (int)wid-1, i);
	}

#ifndef THIS_DIVES_NOT
	/* sorry, not every X server can orInverted (hp !!) */
	XSetFunction(priv->dpy, priv->gc, GXorInverted);
#else
	for (i = 0; i < wid; i++) {
		int j;

		for (j = 0; j < hig; j++) {
			XPutPixel(xima, i, j, ! XGetPixel(xima, i, j));
		}
	}
	XSetFunction(priv->dpy, priv->gc, GXor);
#endif
	XPutImage(priv->dpy, pix, priv->gc, xima,
					0, 0, 0, 0, wid, hig);
	XSetFunction(priv->dpy, priv->gc, GXcopy);

	XDestroyImage(xima);
}

static int is_white(XImage *xima, int x, int y)
{
	if (x<0 || x>=(int)xima->width || y<0 || y>=(int)xima->height) return 0;
	return (XGetPixel(xima, x, y) == 0);
}

static int has_white_neighbour(XImage *xima, int x, int y)
{
	return (is_white(xima, x-1, y-1) ||
			is_white(xima, x-1, y) ||
			is_white(xima, x-1, y+1) ||
			is_white(xima, x, y-1) ||
			is_white(xima, x, y+1) ||
			is_white(xima, x+1, y-1) ||
			is_white(xima, x+1, y) ||
			is_white(xima, x+1, y+1));
}

static void put_black(XImage *xima, int x, int y)
{
	if (x<0 || x>=(int)xima->width || y<0 || y>=(int)xima->height) return;
	XPutPixel(xima, x, y, 1L);
}

static void put_box_around(XImage *xima, int x, int y)
{
	put_black(xima, x-1, y-1);
	put_black(xima, x-1, y);
	put_black(xima, x-1, y+1);
	put_black(xima, x, y-1);
	put_black(xima, x, y+1);
	put_black(xima, x+1, y-1);
	put_black(xima, x+1, y);
	put_black(xima, x+1, y+1);
}

static void bitmap_thicken(Bitmap_private *priv)
{
	char **matrix;
	XImage *xima;
	Pixmap pix = (Pixmap)xv_get(BITPUB(priv), XV_XID);
	unsigned wid = (unsigned)xv_get(BITPUB(priv), XV_WIDTH);
	unsigned hig = (unsigned)xv_get(BITPUB(priv), XV_HEIGHT);
	int x, y;

	xima = XGetImage(priv->dpy, pix, 0, 0, wid, hig, 1L, XYPixmap);

	matrix = (char **)xv_alloc_n(char *, (size_t)wid);
	for (x = 0; x < wid; x++) matrix[x] = xv_alloc_n(char,(size_t)hig);

	for (x = 0; x < wid; x++) for (y = 0; y < hig; y++) {
		if (XGetPixel(xima, x, y) && has_white_neighbour(xima, x, y)) {
			matrix[x][y] = 1;
		}
	}
	
	for (x = 0; x < wid; x++) for (y = 0; y < hig; y++) {
		if (matrix[x][y]) put_box_around(xima, x, y);
	}
	
	for (x = 0; x < wid; x++) xv_free(matrix[x]);
	xv_free((char *)matrix);

	XSetFunction(priv->dpy, priv->gc, GXor);
	XPutImage(priv->dpy, pix, priv->gc, xima,
					0, 0, 0, 0, wid, hig);
	XSetFunction(priv->dpy, priv->gc, GXcopy);

	XDestroyImage(xima);
}

static void bitmap_flood_fill(Bitmap_private *priv, int x, int y)
{
	XImage *xima;
	Pixmap pix = (Pixmap)xv_get(BITPUB(priv), XV_XID);
	unsigned wid = (unsigned)xv_get(BITPUB(priv), XV_WIDTH);
	unsigned hig = (unsigned)xv_get(BITPUB(priv), XV_HEIGHT);

	xima = XGetImage(priv->dpy, pix, 0, 0, wid, hig, 1L, XYPixmap);
	flood_fill(xima, x, y);
	XSetFunction(priv->dpy, priv->gc, GXor);
	XPutImage(priv->dpy, pix, priv->gc, xima, 0, 0, 0, 0, wid, hig);
	XSetFunction(priv->dpy, priv->gc, GXcopy);
	XDestroyImage(xima);
}

static void bitmap_invert(Bitmap_private *priv)
{
	Pixmap pix = (Pixmap)xv_get(BITPUB(priv), XV_XID);
	unsigned wid = (unsigned)xv_get(BITPUB(priv), XV_WIDTH);
	unsigned hig = (unsigned)xv_get(BITPUB(priv), XV_HEIGHT);

	XSetFunction(priv->dpy, priv->gc, GXinvert);
	XFillRectangle(priv->dpy, pix, priv->gc, 0, 0, wid+1, hig+1);
	XSetFunction(priv->dpy, priv->gc, GXcopy);
}

static void copy_from(Bitmap_private *priv, Server_image src)
{
	Pixmap mypix, spix;
	unsigned wid, hig, swid, shig;

	mypix = (Pixmap)xv_get(BITPUB(priv), XV_XID);
	if (! mypix) {
		xv_error(BITPUB(priv), 
					ERROR_PKG, BITMAP,
					ERROR_SEVERITY, ERROR_RECOVERABLE,
					ERROR_STRING, "have no pixmap",
					NULL);
		return;
	}
	wid = (unsigned)xv_get(BITPUB(priv), XV_WIDTH);
	hig = (unsigned)xv_get(BITPUB(priv), XV_HEIGHT);

	spix = (Pixmap)xv_get(src, XV_XID);
	swid = (unsigned)xv_get(src, XV_WIDTH);
	shig = (unsigned)xv_get(src, XV_HEIGHT);

	XCopyPlane(priv->dpy, spix, mypix, priv->gc,
						0, 0, IMIN(swid, wid), IMIN(shig, hig), 0, 0, 1L);
}

static void copy_to(Bitmap_private *priv, Server_image tgt)
{
	Pixmap mypix, tpix;
	unsigned wid, hig, twid, thig;

	mypix = (Pixmap)xv_get(BITPUB(priv), XV_XID);
	if (! mypix) {
		xv_error(BITPUB(priv), 
					ERROR_PKG, BITMAP,
					ERROR_SEVERITY, ERROR_RECOVERABLE,
					ERROR_STRING, "have no pixmap",
					NULL);
		return;
	}
	wid = (unsigned)xv_get(BITPUB(priv), XV_WIDTH);
	hig = (unsigned)xv_get(BITPUB(priv), XV_HEIGHT);

	tpix = (Pixmap)xv_get(tgt, XV_XID);
	twid = (unsigned)xv_get(tgt, XV_WIDTH);
	thig = (unsigned)xv_get(tgt, XV_HEIGHT);

	XCopyPlane(priv->dpy, mypix, tpix, priv->gc,
						0, 0, IMIN(twid, wid), IMIN(thig, hig), 0, 0, 1L);
}

static Bitmap_file_status read_raster_file(Bitmap_private *priv, int fd)
{
	int width;
	struct rasterfile rsf;
	unsigned short *buf;

	if (read(fd, (char *)&rsf, sizeof(rsf)) < sizeof(rsf))
		return BITMAP_NO_RASTER;

	if (rsf.ras_magic != RAS_MAGIC) return BITMAP_BAD_MAGIC;

	if (rsf.ras_depth != 1 || rsf.ras_maptype != 0)
		return BITMAP_INVALID_DEPTH;

	if (rsf.ras_type != RT_STANDARD) return BITMAP_INVALID_TYPE;

	width = ((rsf.ras_width - 1) / 16 + 1);
	if (rsf.ras_length != 0) {
		if (rsf.ras_height * 2 * width != rsf.ras_length)
			return BITMAP_INCONSISTENT;
	}
	else rsf.ras_length = width * 2 * rsf.ras_height;

	buf = (unsigned short *)xv_malloc((size_t)rsf.ras_length);
	if (! buf) return BITMAP_NO_MEM;

	if (read(fd, (char *)buf, (size_t)rsf.ras_length) < rsf.ras_length)
		return BITMAP_INCONSISTENT;

	xv_set(BITPUB(priv),
				XV_WIDTH, rsf.ras_width,
				XV_HEIGHT, rsf.ras_height,
				SERVER_IMAGE_DEPTH, 1,
				SERVER_IMAGE_BITS, buf,
				NULL);

	xv_free(buf);
	return BITMAP_OK;
}

static Bitmap_file_status load_file(Bitmap_private *priv)
{
	char msg[BUFSIZ];
	Bitmap_file_status status;
	FILE *file;
	Server_image new;
	int st;
	unsigned int wid, hig;
	Pixmap pix;
	int hx, hy;

	if (! priv->file) return BITMAP_NO_FILE_NAME;

	switch (priv->type) {
		case BITMAP_RASTER:
			if ((file = fopen(priv->file, "r"))) {
				status = read_raster_file(priv, fileno(file));
				fclose(file);
				return status;
			}
			return BITMAP_ACCESS;

		case BITMAP_X11:
			{
				int st;
				unsigned int wid, hig;
				Pixmap pix;
				int hx, hy;

				st = XReadBitmapFile(priv->dpy,
						(Window)xv_get(xv_get(priv->screen,XV_ROOT),XV_XID),
						priv->file, &wid, &hig, &pix, &hx, &hy);

				if (st == BitmapSuccess) {
					xv_set(BITPUB(priv),
						SERVER_IMAGE_DEPTH, 1,
						XV_WIDTH, wid,
						XV_HEIGHT, hig,
						SERVER_IMAGE_PIXMAP, pix,
						NULL);
					priv->type = BITMAP_X11;
					return BITMAP_OK;
				}
				else return BITMAP_UNKNOWN_TYPE;
			}
		case BITMAP_XVIEW:
			if (access(priv->file, R_OK)) return BITMAP_ACCESS;
			new = icon_load_svrim(priv->file, msg);
			if (new) {
				xv_set(BITPUB(priv),
						XV_WIDTH, xv_get(new, XV_WIDTH),
						XV_HEIGHT, xv_get(new, XV_HEIGHT),
						SERVER_IMAGE_DEPTH, 1,
						NULL);
				copy_from(priv, new);
				xv_destroy(new);
				return BITMAP_OK;
			}
			return BITMAP_UNKNOWN_TYPE;

		default: /* including case BITMAP_UNKNOWN: */
			errno = 0;
			new = icon_load_svrim(priv->file, msg);
			st = errno; /* save errno */
			if (new) {
				priv->type = BITMAP_XVIEW;
				xv_set(BITPUB(priv),
						XV_WIDTH, xv_get(new, XV_WIDTH),
						XV_HEIGHT, xv_get(new, XV_HEIGHT),
						SERVER_IMAGE_DEPTH, 1,
						NULL);
				copy_from(priv, new);
				xv_destroy(new);
				return BITMAP_OK;
			}
			if (st == EACCES || st == ENOENT) return BITMAP_ACCESS;

			if (!(file = fopen(priv->file, "r"))) return BITMAP_ACCESS;

			status = read_raster_file(priv, fileno(file));
			fclose(file);

			if (status == BITMAP_OK) {
				priv->type = BITMAP_RASTER;
				return BITMAP_OK;
			}

			st = XReadBitmapFile(priv->dpy,
					(Window)xv_get(xv_get(priv->screen,XV_ROOT),XV_XID),
					priv->file, &wid, &hig, &pix, &hx, &hy);

			if (st == BitmapSuccess) {
				xv_set(BITPUB(priv),
					SERVER_IMAGE_DEPTH, 1,
					XV_WIDTH, wid,
					XV_HEIGHT, hig,
					SERVER_IMAGE_PIXMAP, pix,
					NULL);
				priv->type = BITMAP_X11;
				return BITMAP_OK;
			}
			return BITMAP_UNKNOWN_TYPE;
	}
}

#define RS_BLOCK_LEN 256

static void write_val(int fd, unsigned val, unsigned short *buf, int *indx)
{
	buf[*indx] = (unsigned short)val;
	if (++*indx >= RS_BLOCK_LEN) {
		write(fd, buf, (size_t)RS_BLOCK_LEN * 2);
		*indx = 0;
	}
}

static void write_raster_file(Bitmap_private *priv, int fd)
{
	XImage *xima;
	Pixmap pix = (Pixmap)xv_get(BITPUB(priv), XV_XID);
	unsigned wid = (unsigned)xv_get(BITPUB(priv), XV_WIDTH);
	unsigned hig = (unsigned)xv_get(BITPUB(priv), XV_HEIGHT);
	int bi, x, y, file_width;
	unsigned short mask, val, block[RS_BLOCK_LEN];
	struct rasterfile rsf;

	rsf.ras_magic = RAS_MAGIC;
	rsf.ras_width = wid;
	rsf.ras_height = hig;
	rsf.ras_depth = 1;
	rsf.ras_type = RT_STANDARD;
	rsf.ras_maptype = 0;
	rsf.ras_maplength = 0;

	xima = XGetImage(priv->dpy, pix, 0, 0, wid, hig, 1L, XYPixmap);

	file_width = ((wid - 1) / 16 + 1) * 16;
	rsf.ras_length = (file_width / 8) * hig;  /* length (bytes) */

	write(fd, (char *)&rsf, sizeof(rsf));

	bi = 0;
	for (y = 0; y < hig; y++) {
		val = 0;
		mask = 0x8000;
		for (x = 0; x < wid; x++) {
			if (XGetPixel(xima, x, y)) {
				val |= mask;
			}
			mask >>= 1;
			if (mask == 0) {
				write_val(fd, val, block, &bi);
				mask = 0x8000;
				val = 0;
			}
		}
		if (mask != 0x8000) {
			write_val(fd, val, block, &bi);
			mask = 0x8000;
			val = 0;
		}
	}
	if (bi != 0) {
		write(fd, block, (size_t)bi * 2);
	}
	XDestroyImage(xima);
}

static void write_xview_file(Bitmap_private *priv, FILE *file)
{
	XImage *xima;
	Pixmap pix = (Pixmap)xv_get(BITPUB(priv), XV_XID);
	unsigned wid = (unsigned)xv_get(BITPUB(priv), XV_WIDTH);
	unsigned hig = (unsigned)xv_get(BITPUB(priv), XV_HEIGHT);
	int col_cnt, x, y, file_width;
	unsigned short mask, val;

	xima = XGetImage(priv->dpy, pix, 0, 0, wid, hig, 1L, XYPixmap);

	file_width = ((wid - 1) / 16 + 1) * 16;
	fprintf(file, "/* Format_version=1, Width=%d, Height=%d, ",
				file_width, hig);
	fprintf(file, "Depth=1, Valid_bits_per_item=16\n */\n");

	col_cnt = 0;
	for (y = 0; y < hig; y++) {
		val = 0;
		mask = 0x8000;
		for (x = 0; x < wid; x++) {
			if (XGetPixel(xima, x, y)) {
				val |= mask;
			}
			mask >>= 1;
			if (mask == 0) {
				fprintf(file, " 0x%04x,", val);
				mask = 0x8000;
				val = 0;
				if (++col_cnt >= 8) {
					col_cnt = 0;
					fprintf(file, "\n");
				}
			}
		}
		if (mask != 0x8000) {
			fprintf(file, " 0x%04x,", val);
			mask = 0x8000;
			val = 0;
			if (++col_cnt >= 8) {
				col_cnt = 0;
				fprintf(file, "\n");
			}
		}
	}
	XDestroyImage(xima);
}

static Bitmap_file_status store_file(Bitmap_private *priv)
{
	FILE *fil;
	int status;

	if (! priv->file) return BITMAP_NO_FILE_NAME;

	switch (priv->type) {
		case BITMAP_X11:
			status = XWriteBitmapFile(priv->dpy, priv->file,
							(Pixmap)xv_get(BITPUB(priv), XV_XID),
							(unsigned)xv_get(BITPUB(priv), XV_WIDTH),
							(unsigned)xv_get(BITPUB(priv), XV_HEIGHT),
							-1, -1);

			if (status != BitmapSuccess) return BITMAP_ACCESS;
			return BITMAP_OK;

		case BITMAP_XVIEW:
			fil = fopen(priv->file, "w");
			if (! fil) return BITMAP_ACCESS;

			write_xview_file(priv, fil);
			fclose(fil);
			return BITMAP_OK;

		case BITMAP_RASTER:
			fil = fopen(priv->file, "w");
			if (! fil) return BITMAP_ACCESS;
			write_raster_file(priv, fileno(fil));
			fclose(fil);
			return BITMAP_OK;

		default: return BITMAP_UNKNOWN_TYPE;
	}
}

static int bitmap_init(Xv_screen owner, Bitmap slf, Attr_avlist avlist, int *u)
{
	Xv_bitmap *self = (Xv_bitmap *)slf;
	Bitmap_private *priv, *list;
	Xv_window xvroot;
	GC *gc_list;

	if (! bm_key) bm_key = xv_unique_key();

	if (!(priv = (Bitmap_private *)xv_alloc(Bitmap_private)))
		return XV_ERROR;

	priv->public_self = (Xv_opaque)self;
	self->private_data = (Xv_opaque)priv;

	priv->screen = owner ? owner : xv_default_screen;
	priv->dpy =(Display *)xv_get(xv_get(priv->screen,SCREEN_SERVER),XV_DISPLAY);

	xvroot = xv_get(priv->screen, XV_ROOT);
    gc_list = (GC *)xv_get(priv->screen, SCREEN_OLGC_LIST, xvroot);
	priv->gc = gc_list[SCREEN_BITMAP_GC];

	if ((list = (Bitmap_private *)xv_get(priv->screen, XV_KEY_DATA, bm_key))) {
		while (list->next) list = list->next;

		/* assign new image object to end of list */
		list->next = priv;
	}
	else {
		xv_set(priv->screen, XV_KEY_DATA, bm_key, priv, NULL);
	}
	return XV_OK;
}

static Xv_opaque bitmap_set(Bitmap self, Attr_avlist avlist)
{
	Attr_attribute *attrs;
	Bitmap_private *priv = BITPRIV(self);
	Bitmap_file_status st, *stp = (Bitmap_file_status *)0;

	for (attrs=avlist; *attrs; attrs=attr_next(attrs)) switch ((int)*attrs) {
		case XV_DEPTH:
		case SERVER_IMAGE_DEPTH: A1 = (Attr_attribute)1;
			break;

		case BITMAP_FILE:
			if (priv->file) xv_free(priv->file);
			priv->file = (char *)0;
			if (A1) priv->file = xv_strsave((char *)A1);
			ADONE;

		case BITMAP_FILE_STATUS:
			stp = (Bitmap_file_status *)A1;
			ADONE;

		case BITMAP_FILE_TYPE:
			priv->type = (Bitmap_file_type)A1;
			ADONE;

		case BITMAP_LOAD_FILE:
			st = load_file(priv);
			if (stp) *stp = st;
			ADONE;

		case BITMAP_STORE_FILE:
			st = store_file(priv);
			if (stp) *stp = st;
			ADONE;

		case BITMAP_THICKEN:
			bitmap_thicken(priv);
			ADONE;

		case BITMAP_INVERT:
			bitmap_invert(priv);
			ADONE;

		case BITMAP_FILL_FOR_MASK:
			fill_for_mask(priv);
			ADONE;

		case BITMAP_COPY_FROM:
			copy_from(priv, (Server_image)A1);
			ADONE;

		case BITMAP_COPY_TO:
			copy_to(priv, (Server_image)A1);
			ADONE;

		case BITMAP_FLOOD_FILL:
			bitmap_flood_fill(priv, (int)A1, (int)A2);
			ADONE;

		default: xv_check_bad_attr(BITMAP, A0);
			break;
	}

	return XV_OK;
}

static Xv_opaque bitmap_get(Bitmap self, int *status, Attr_attribute attr, va_list vali)
{
	Bitmap_private *priv = BITPRIV(self);

	*status = XV_OK;
	switch ((int)attr) {
		case BITMAP_FILE: return (Xv_opaque)priv->file;
		case BITMAP_FILE_TYPE: return (Xv_opaque)priv->type;
		default: *status = xv_check_bad_attr(BITMAP, attr);
	}
	return (Xv_opaque)XV_OK;
}

static int bitmap_destroy(Bitmap self, Destroy_status status)
{
	if (status == DESTROY_CLEANUP) {
		Bitmap_private *priv = BITPRIV(self), *list;

        list = (Bitmap_private *)xv_get(priv->screen, XV_KEY_DATA, bm_key);
        if (BITPUB(list) == self) {
            xv_set(priv->screen, XV_KEY_DATA, bm_key, list->next, NULL);
		}
		else {
        	for ( ; list->next; list = list->next) {
            	if (BITPUB(list->next) == self) {
                	list->next = list->next->next;
                	break;
            	}
			}
		}

		if (priv->file) xv_free(priv->file);
		xv_free((char *)priv);
	}
	return XV_OK;
}

static Xv_opaque bitmap_find(Xv_screen owner, const Xv_pkg *pkg,
							Attr_avlist avlist)
{
	Xv_Screen screen = owner? owner : xv_default_screen;
	Bitmap_private *list;
	Attr_attribute *attrs;
	/* consider all the attrs we allow "find" to match on */
	int     width = -1, height = -1;
	char   *filename = NULL;

	if (! bm_key) bm_key = xv_unique_key();

	/* get the list of existing images from the screen */
	list = (Bitmap_private *)xv_get(screen, XV_KEY_DATA, bm_key);

	if (!list) return XV_NULL;

    for (attrs = avlist; *attrs; attrs = attr_next(attrs)) switch (attrs[0]) {
		case XV_WIDTH : width = (int)attrs[1]; break;
		case XV_HEIGHT : height = (int)attrs[1]; break;
		case BITMAP_FILE: filename = (char *)attrs[1]; break;
		default: break;
	}
	/* Now loop thru each object looking for those whose
	 * value that match those specified above.
	 */
	for ( ; list; list = list->next) {
		/* If it doesn't match, continue to the next object in
		 * the list.  Repeat for each requested attribute.
		 */
		 if (width > -1 && (width != (int)xv_get(XV_PUBLIC(list), XV_WIDTH)))
			continue;
		if (height > -1 && (height != (int)xv_get(XV_PUBLIC(list), XV_HEIGHT)))
			continue;
		if (filename && (!list->file || strcmp(filename, list->file)))
			continue;
		/* all matches seemed to be successful, return this object */
		return BITPUB(list);
	}
	/* nothing found */
	return XV_NULL;
}

const Xv_pkg xv_bitmap_pkg = {
	"Bitmap",
	ATTR_PKG_BITMAP,
	sizeof(Xv_bitmap),
	SERVER_IMAGE,
	bitmap_init,
	bitmap_set,
	bitmap_get,
	bitmap_destroy,
	bitmap_find
};
