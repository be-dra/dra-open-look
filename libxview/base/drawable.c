#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)drawable.c 20.24 93/06/28  DRA: $Id: drawable.c,v 4.3 2025/03/08 13:01:51 dra Exp $";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

#include <xview_private/draw_impl.h>
#include <xview_private/win_info.h>
#define _NOTIFY_MIN_SYMBOLS
#include <xview/notify.h>
#undef _NOTIFY_MIN_SYMBOLS

const char *xv_draw_info_str = "drawable_info";
const char *xv_notptr_str = "not a pointer";

/*ARGSUSED*/
static int drawable_init(Xv_opaque parent_public, Xv_Drawable self, Attr_avlist avlist, int *offset_ptr)
{
    Xv_drawable_struct *drawable_public = (Xv_drawable_struct *)self;

	drawable_public->private_data = (Xv_opaque) xv_alloc(Xv_Drawable_info);

	return XV_OK;
}

static int drawable_destroy(Xv_Drawable drawable_public, Destroy_status status)
{
    Xv_Drawable_info *drawable = DRAWABLE_PRIVATE(drawable_public);

    if (status == DESTROY_CLEANUP) {
		(void) free((char *) drawable);
		return XV_OK;
    }
    return XV_OK;
}

static Xv_opaque drawable_get_attr(Xv_Drawable drawable_public, int *status,
									Attr_attribute attr, va_list valist)
{

    Xv_Drawable_info *info;

    switch (attr) {
      case DRAWABLE_INFO:
	return ((Xv_opaque) DRAWABLE_PRIVATE(drawable_public));

      case XV_XID:
	info = DRAWABLE_PRIVATE(drawable_public);
	return ((Xv_opaque) (info->xid));

      case XV_DISPLAY:
	info = DRAWABLE_PRIVATE(drawable_public);
	return ((Xv_opaque) (info->visual->display));

      default:
	if (xv_check_bad_attr(&xv_drawable_pkg,
				     (Attr_attribute) attr) == XV_ERROR) {
	    *status = XV_ERROR;
	}
	return (XV_NULL);
    }
}

Xv_private GC xv_private_gc(Xv_opaque d)
{
    return ((GC) window_private_gc(d));
}

const Xv_pkg xv_drawable_pkg = {
    "Drawable",
    ATTR_PKG_DRAWABLE,
    sizeof(Xv_drawable_struct),
    &xv_generic_pkg,
    drawable_init,
    NULL,			/* No set allowed */
    drawable_get_attr,
    drawable_destroy,
    NULL,			/* No find procedure */
};
