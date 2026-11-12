#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)frame_help.c 1.27 93/06/28 DRA: $Id: frame_help.c,v 4.9 2026/10/03 18:24:12 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

#include <X11/Xlib.h>
#include <xview_private/fm_impl.h>
#include <xview_private/draw_impl.h>
#include <xview/cursor.h>
#include <xview/server.h>
#include <xview_private/svr_atom.h>
#include <xview_private/svr_impl.h>
#include <xview_private/win_info.h>

#include <X11/Xatom.h>

#if defined(WITH_3X_LIBC) || defined(vax)
/* 3.x - 4.0 libc transition code; old (pre-4.0) code must define the symbol */
#define jcsetpgrp(p)  setpgrp((p),(p))
#endif

#define	FRAME_HELP_PRIVATE(f)	XV_PRIVATE(Frame_help_info, Xv_frame_help, f)
#define FRAME_HELP_PUBLIC(f)	XV_PUBLIC(f)
#define FRAME_CLASS_FROM_HELP(f) FRAME_PRIVATE(FRAME_HELP_PUBLIC(f))

typedef	struct	{
    Frame	public_self;	/* back pointer to object */
    WM_Win_Type	win_attr;	/* _OL_WIN_ATTR */
} Frame_help_info;

static int frame_help_init(Xv_Window owner, Frame frame_public,
							Attr_attribute avlist[], int *u)
{
	Xv_frame_help *frame_object = (Xv_frame_help *) frame_public;
	Xv_Drawable_info *info;
	Xv_opaque server_public;
	Frame_help_info *frame;
	Attr_avlist attrs;
	int set_popup = FALSE;
	Frame_class_info *frcl;

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

	frcl = FRAME_PRIVATE(frame_public);
	status_set(frcl, show_resize_corner, FALSE);

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

static Xv_opaque frame_help_set_avlist(Frame frame_public,
							Attr_attribute avlist[])
{
	Attr_avlist attrs;
	Frame_help_info *frame = FRAME_HELP_PRIVATE(frame_public);
	int result = XV_OK;
	Atom pinstate;
	Xv_opaque srv = XV_SERVER_FROM_WINDOW(frame_public);

	for (attrs = avlist; *attrs; attrs = attr_next(attrs)) {
		switch (attrs[0]) {

			case XV_LABEL:
				{
					extern char *xv_app_name;
					Frame_class_info *frame_class =
							FRAME_CLASS_FROM_HELP(frame);

					*attrs = (Frame_attribute) ATTR_NOP(*attrs);

					if (frame_class->label) {
						free(frame_class->label);
					}
					if ((char *)attrs[1]) {
						frame_class->label = (char *)calloc(1L,
								strlen((char *)attrs[1]) + 1);
						strcpy(frame_class->label, (char *)attrs[1]);
					}
					else {
						if (xv_app_name) {
							frame_class->label = (char *)calloc(1L,
									strlen(xv_app_name) + 6);
							sprintf(frame_class->label, "%s Help", xv_app_name);
						}
						else {
							frame_class->label = NULL;
						}
					}
					frame_display_label(frame_class);
					break;
				}

			case XV_END_CREATE:
				wmgr_set_win_attr(frame_public, &(frame->win_attr));
				pinstate = xv_get(srv, SERVER_WM_PIN_STATE);
				win_change_property(frame_public,
						(Attr_attribute)SERVER_WM_PROTOCOLS, XA_ATOM, 32,
						(unsigned char *)&pinstate, 1, TRUE);
				break;

			default:
				break;

		}
	}

	return (Xv_opaque) result;
}

/* Destroy the frame struct */
static int frame_help_destroy(Frame frame_public, Destroy_status status)
{
    Frame_help_info *frame = FRAME_HELP_PRIVATE(frame_public);

    if (status == DESTROY_CLEANUP) {	/* waste of time if ...PROCESS_DEATH */
    	/* Free frame struct */
    	free((char *) frame);
    }
    return XV_OK;
}

const Xv_pkg          xv_frame_help_pkg = {
    "Frame_help",
	(Attr_pkg) ATTR_PKG_FRAME,
    sizeof(Xv_frame_help),
    FRAME_CLASS,
    frame_help_init,
    frame_help_set_avlist,
    NULL,
    frame_help_destroy,
    NULL			/* no find proc */
};
