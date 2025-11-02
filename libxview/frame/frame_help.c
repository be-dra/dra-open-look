#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)frame_help.c 1.27 93/06/28 DRA: $Id: frame_help.c,v 4.4 2025/11/01 13:00:16 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

#include <X11/Xlib.h>
#include <xview_private/fm_impl.h>
#include <xview_private/frame_help.h>
#include <xview_private/draw_impl.h>
#include <xview/cursor.h>
#include <xview/server.h>
#include <xview_private/svr_atom.h>
#include <xview_private/svr_impl.h>

#include <X11/Xatom.h>

#if defined(WITH_3X_LIBC) || defined(vax)
/* 3.x - 4.0 libc transition code; old (pre-4.0) code must define the symbol */
#define jcsetpgrp(p)  setpgrp((p),(p))
#endif

/* ARGSUSED */
static int frame_help_init(Xv_Window owner, Frame frame_public, Attr_attribute avlist[], int *u)
{
	Xv_frame_help *frame_object = (Xv_frame_help *) frame_public;
	Xv_Drawable_info *info;
	Xv_opaque server_public;
	Frame_help_info *frame;
	Attr_avlist attrs;
	int set_popup = FALSE;

	DRAWABLE_INFO_MACRO(frame_public, info);
	server_public = xv_server(info);
	frame = xv_alloc(Frame_help_info);

	/* link to object */
	frame_object->private_data = (Xv_opaque) frame;
	frame->public_self = frame_public;

	/* set initial window decoration flags */
	frame->win_attr.flags = WMWinType | WMMenuType | WMPinState;
	frame->win_attr.win_type = (Atom) xv_get(server_public, SERVER_WM_WT_HELP);
	frame->win_attr.menu_type = (Atom) xv_get(server_public,
			SERVER_WM_MENU_LIMITED);
	frame->win_attr.pin_initial_state = WMPushpinIsIn;

	status_set(frame, show_label, TRUE);

	status_set(frame, show_resize_corner, FALSE);

	for (attrs = avlist; *attrs; attrs = attr_next(attrs)) {
		switch (*attrs) {
			case XV_SET_POPUP:
				set_popup = TRUE;
				server_set_popup(frame_public, (Attr_attribute *)&attrs[1]);
				ATTR_CONSUME(*attrs);
				break;
			default:
				break;
		}
	}

	if (! set_popup) {
		Attr_attribute attr = 0;
		server_set_popup(frame_public, &attr);
	}

	return XV_OK;
}

const Xv_pkg          xv_frame_help_pkg = {
    "Frame_help", (Attr_pkg) ATTR_PKG_FRAME,
    sizeof(Xv_frame_help),
    FRAME_CLASS,
    frame_help_init,
    frame_help_set_avlist,
    frame_help_get_attr,
    frame_help_destroy,
    NULL			/* no find proc */
};
