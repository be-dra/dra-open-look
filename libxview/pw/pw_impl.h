/*	@(#)pw_impl.h 20.17 89/08/18 SMI  DRA: RCS $Id: pw_impl.h,v 2.3 2025/01/09 09:18:40 dra Exp $	*/

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL_NOTICE 
 *	file for terms of the license.
 */

#ifndef xv_pw_impl_h_already_defined
#define	xv_pw_impl_h_already_defined

#include <sys/types.h>
#include <pixrect/pixrect.h>
#include <pixrect/memvar.h>
#include <xview/pkg.h>
#include <xview/window.h>
#include <xview/pixwin.h>
#include <X11/Xlib.h>
#include <xview_private/draw_impl.h>

#ifdef i386
extern 	   struct pixrectops mem_ops;
#endif /* ~i386 */
Xv_private struct pixrectops	server_image_ops;
extern int			xv_to_xop[];

#define PIX_OP_SHIFT    1
#ifndef PIX_OP
#define	PIX_OP(_op)	((_op) & PIX_NOT(0))
#endif /* PIX_OP */
#define XV_TO_XOP(_op)	(xv_to_xop[PIX_OP(_op) >> PIX_OP_SHIFT])

#define SERVER_IMAGE_PR  	1
#define MEMORY_PR 		2
#define OTHER_PR  		3
#define PR_IS_MPR(pr)		(((Pixrect *)pr)->pr_ops == &mem_ops)
#define PR_NOT_MPR(pr)		(((Pixrect *)pr)->pr_ops != &mem_ops)
#define PR_IS_SERVER_IMAGE(pr)	(((Pixrect *)pr)->pr_ops == &server_image_ops)
#define PR_NOT_SERVER_IMAGE(pr)	(((Pixrect *)pr)->pr_ops != &server_image_ops)
#define PR_TYPE(pr)		PR_IS_MPR(pr) ? MEMORY_PR : \
				(PR_IS_SERVER_IMAGE(pr) ? SERVER_IMAGE_PR :  OTHER_PR)

Xv_private void	xv_set_gc_op(Display *, Xv_Drawable_info *, GC, int,short,int);
Xv_private GC xv_find_proper_gc(Display *, Xv_Drawable_info *, int);
Xv_private int xv_mem_destroy( Pixrect *pr);

struct gc_chain {
        struct gc_chain *next;
        GC               gc;
        int              depth;
        Drawable         xid;
	short		 clipping_set;
};

Xv_private Pixrect * xv_mem_create(int w, int h, int depth);
Xv_private int xv_rop_mpr_internal(Display *display, Drawable d, GC gc, int x, int y, int width, int height, Xv_opaque src, int xr, int yr, Xv_Drawable_info *dest_info, int mpr_bits);
Xv_private int xv_rop_internal(Display *display, Drawable d, GC gc, int x, int y, int width, int height, Xv_opaque src, int xr, int yr, Xv_Drawable_info *dest_info);

struct graphics_info;

Xv_private struct graphics_info *xv_init_olgx(Xv_Window win, int *three_d,
									Xv_opaque text_font);
Xv_private Cms xv_set_control_cms(Xv_Window window_public, Xv_Drawable_info *info, int cms_status);

Xv_public int xv_read_internal(Pixrect *pr, int op, int x, int y, int width, int height, Display *display, Drawable d, int sx, int sy);
Xv_private int xv_stencil_internal(Display *display, Xv_Drawable_info *info, Drawable d, GC gc,
		int dx, int dy, int width, int height,
		Xv_opaque stpr, int stx, int sty,
		Xv_opaque spr, int sx, int sy, Xv_Drawable_info *dest_info);
Xv_private int xv_replrop_internal(Display *display, Xv_Drawable_info *info,
		Drawable d, GC gc, int xw, int yw, int width, int height,
		Pixrect *src, int xr, int yr, Xv_Drawable_info *dest_info);
Xv_private Pixrect *xv_mem_point(int w, int h, int depth, short *image);

#endif /* xv_pw_impl_h_already_defined */
