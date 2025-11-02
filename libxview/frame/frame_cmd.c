#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)frame_cmd.c 1.48 93/06/28 DRA: $Id: frame_cmd.c,v 4.5 2025/11/01 13:00:16 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

#include <X11/Xlib.h>
#include <xview_private/fm_impl.h>
#include <xview_private/frame_cmd.h>
#include <xview_private/draw_impl.h>
#include <xview/cursor.h>
#include <xview/server.h>
#include <xview_private/svr_atom.h>
#include <xview_private/svr_impl.h>
#include <xview_private/wmgr_decor.h>

#include <X11/Xatom.h>

#if defined(WITH_3X_LIBC) || defined(vax)
/* 3.x - 4.0 libc transition code; old (pre-4.0) code must define the symbol */
#define jcsetpgrp(p)  setpgrp((p),(p))
#endif

static xv_popup_frame_initializer_t popup_frame_init = NULL;

xv_popup_frame_initializer_t xv_set_popup_frame_initializer(xv_popup_frame_initializer_t initor)
{
	xv_popup_frame_initializer_t old = popup_frame_init;

	popup_frame_init = initor;
	return old;
}

static int frame_cmd_init(Xv_Window owner, Frame frame_public,
						Attr_attribute avlist[], int *u)
{
	Xv_frame_cmd *frame_object = (Xv_frame_cmd *) frame_public;
	Xv_Drawable_info *info;
	Xv_opaque server_public;
	Frame_cmd_info *frame;
	int set_popup = FALSE;
	Attr_avlist attrs;
	int dont_show_resize_corner = TRUE;

	DRAWABLE_INFO_MACRO(frame_public, info);
	server_public = xv_server(info);
	frame = xv_alloc(Frame_cmd_info);

	/* link to object */
	frame_object->private_data = (Xv_opaque) frame;
	frame->public_self = frame_public;

	status_set(frame, warp_pointer, TRUE);

	/* set initial window decoration flags */
	frame->win_attr.flags = WMWinType | WMMenuType | WMPinState;
	frame->win_attr.win_type = (Atom) xv_get(server_public, SERVER_WM_WT_CMD);
	frame->win_attr.menu_type =
			(Atom) xv_get(server_public, SERVER_WM_MENU_LIMITED);
	frame->win_attr.pin_initial_state = WMPushpinIsOut;

	frame->panel_bordered = TRUE;

	status_set(frame, show_label, TRUE);
	status_set(frame, pushpin_in, FALSE);
	status_set(frame, default_pin_state, FRAME_CMD_PIN_OUT);	/* new attr */

	/* Wmgr default to have resize corner for cmd frame */
	status_set(frame, show_resize_corner, TRUE);

	notify_interpose_event_func(frame_public, frame_cmd_input, NOTIFY_SAFE);
	notify_interpose_event_func(frame_public, frame_cmd_input,
			NOTIFY_IMMEDIATE);

	for (attrs = avlist; *attrs; attrs = attr_next(attrs)) {
		switch (*attrs) {

			case FRAME_SCALE_STATE:
				/*
				 * change scale and find the apprioprate size of the font Don't
				 * call frame_rescale_subwindows because they have not created
				 * yet.
				 */
				/*
				 * WAIT FOR NAYEEM window_set_rescale_state(frame_public,
				 * attrs[1]);
				 */
				wmgr_set_rescale_state(frame_public, (int)attrs[1]);
				break;
			case FRAME_SHOW_RESIZE_CORNER:
				dont_show_resize_corner = !attrs[1];
				break;
			case XV_SET_POPUP:
				set_popup = TRUE;
				server_set_popup(frame_public, (Attr_attribute *)&attrs[1]);
				ATTR_CONSUME(*attrs);
				break;
			default:
				break;
		}
	}
	if (dont_show_resize_corner)
		(void)xv_set(frame_public, FRAME_SHOW_RESIZE_CORNER, FALSE, NULL);

	/* Flush the intial _OL_WIN_ATTR.  We were waiting until END_CREATE,
	 * but that caused problems because the frame could be mapped before
	 * the attr gets flushed.
	 */
	(void)wmgr_set_win_attr(frame_public, &(frame->win_attr));
	if (popup_frame_init) {
		(*popup_frame_init) (frame_public, owner);
	}

	if (! set_popup) {
		Attr_attribute attr = 0;
		server_set_popup(frame_public, &attr);
	}

	return XV_OK;
}

Pkg_private Notify_value frame_cmd_input(Frame frame_public, Notify_event ev,
					Notify_arg arg, Notify_event_type type)
{
	Event *event = (Event *) ev;
	Frame_cmd_info *frame = FRAME_CMD_PRIVATE(frame_public);
	unsigned int action = event_action(event);

	switch (action) {
		case ACTION_PININ:
			status_set(frame, pushpin_in, TRUE);
			break;
		case ACTION_PINOUT:
			status_set(frame, pushpin_in, FALSE);	/* old attr */
			break;
		case WIN_UNMAP_NOTIFY:
			/*
			 * reset the warp_pointer flag so when the user invokes the popup
			 * again, we will warp the pointer
			 */
			status_set(frame, warp_pointer, TRUE);
			break;
	}

	return notify_next_event_func(frame_public, ev, arg, type);
}

const Xv_pkg xv_frame_cmd_pkg = {
    "Frame_cmd", ATTR_PKG_FRAME,
    sizeof(Xv_frame_cmd),
    FRAME_CLASS,
    frame_cmd_init,
    frame_cmd_set_avlist,
    frame_cmd_get_attr,
    frame_cmd_destroy,
    NULL			/* no find proc */
};
