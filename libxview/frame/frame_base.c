#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)frame_base.c 1.31 93/06/28 DRA: $Id: frame_base.c,v 4.4 2025/03/08 13:21:40 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

#include <X11/Xlib.h>
#include <xview_private/fm_impl.h>
#include <xview_private/frame_base.h>
#include <xview_private/draw_impl.h>
#include <xview/cursor.h>
#include <xview/server.h>
#include <xview_private/svr_atom.h>
#include <xview_private/wmgr_decor.h>

#include <X11/Xatom.h>

#if defined(WITH_3X_LIBC) || defined(vax)
/* 3.x - 4.0 libc transition code; old (pre-4.0) code must define the symbol */
#define jcsetpgrp(p)  setpgrp((p),(p))
#endif

/* ARGSUSED */
Pkg_private int frame_base_init(Xv_Window owner, Frame frame_public, Attr_attribute avlist[], int *u)
{
    Xv_frame_base  *frame_object = (Xv_frame_base *) frame_public;
    Xv_Drawable_info *info;
    Xv_opaque       server_public;
    Frame_base_info *frame;
    Attr_avlist      attrs;

    DRAWABLE_INFO_MACRO(frame_public, info);
    server_public = xv_server(info);
    frame = xv_alloc(Frame_base_info);

    /* link to object */
    frame_object->private_data = (Xv_opaque) frame;
    frame->public_self = frame_public;

    /* set saved command line strings to NULL */
    frame->cmd_line_strings = (char **)NULL;
    frame->cmd_line_strings_count = 0;

    /* set initial window decoration flags */

    frame->win_attr.flags = WMWinType | WMMenuType;
    frame->win_attr.win_type = (Atom)xv_get(server_public,SERVER_WM_WT_BASE);
    frame->win_attr.menu_type = (Atom)xv_get(server_public,SERVER_WM_MENU_FULL);

    status_set(frame, show_label, TRUE);
    status_set(frame, props_active, FALSE);

    /* Wmgr default to have resize corner for cmd frame */
    status_set(frame, show_resize_corner, TRUE);

    for (attrs = avlist; *attrs; attrs = attr_next(attrs)) {
	switch (*attrs) {

	  case FRAME_SCALE_STATE:
	    /*
	     * change scale and find the apprioprate size of the font Don't
	     * call frame_rescale_subwindows because they have not been
	     * created yet.
	     */
	    wmgr_set_rescale_state(frame_public, (int)attrs[1]);
	    break;
	  default:
	    break;
	}
    }

    /*
     * _SUN_OL_WIN_ATTR_5 is an atom hung off the _SUN_WM_PROTOCOLS 
     * property on the root window. It's presence indicates that the wmgr
	 * is using the 5-length _OL_WIN_ATTR property. XView now uses the
	 * 5-length property by default. The wmgr will detect this, and will draw
	 * the labels in XView icons.
     * The following code is to prevent this from happening.
     */
	if (xv_get(xv_screen(info), SCREEN_CHECK_SUN_WM_PROTOCOL,
						"_SUN_OL_WIN_ATTR_5"))
	{
        int             delete_decor = 0;
        Atom            delete_decor_list[WM_MAX_DECOR];

        /*
         * Tell wmgr not to draw icon labels - for now this will be done
         * by XView
         */
        delete_decor_list[delete_decor++] =
		(Atom) xv_get(server_public, SERVER_ATOM, "_OL_DECOR_ICON_NAME");
        wmgr_delete_decor(frame_public, delete_decor_list, delete_decor);
    }


    return XV_OK;
}

Xv_private void frame_handle_props(Frame frame_public)
{
	Frame_base_info *frame = FRAME_BASE_PRIVATE(frame_public);

	if (frame->props_proc && status_get(frame, props_active)) {
		(frame->props_proc) (frame_public);
	}
}

const Xv_pkg          xv_frame_base_pkg = {
    "Frame_base", (Attr_pkg) ATTR_PKG_FRAME,
    sizeof(Xv_frame_base),
    &xv_frame_class_pkg,
    frame_base_init,
    frame_base_set_avlist,
    frame_base_get_attr,
    frame_base_destroy,
    NULL			/* no find proc */
};
