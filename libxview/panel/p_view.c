#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)p_view.c 20.31 93/06/28 DRA: $Id: p_view.c,v 4.5 2025/03/08 13:08:26 dra Exp $";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL_NOTICE 
 *	file for terms of the license.
 */

#include <xview_private/panel_impl.h>
#include <xview/defaults.h>
#include <xview/notify.h>


/*ARGSUSED*/
Pkg_private int panel_view_init(Panel parent, Panel_view view_public,
						Attr_attribute  avlist[], int *u)
{
	Xv_Window pw;

	if (view_public) {
		/* the paintwindow is not yet created.... */
		return XV_OK;
	}
	else pw = parent;

	if (pw) {
		Xv_Screen screen = (Xv_Screen) xv_get(parent, XV_SCREEN);

		xv_set(pw,
				WIN_RETAINED, ((int)xv_get(screen, SCREEN_RETAIN_WINDOWS)),
				WIN_NOTIFY_SAFE_EVENT_PROC, panel_notify_event,
				WIN_NOTIFY_IMMEDIATE_EVENT_PROC, panel_notify_event,
				WIN_CONSUME_EVENTS,
					WIN_UP_EVENTS, WIN_ASCII_EVENTS, KBD_USE, LOC_DRAG,
					LOC_WINEXIT,
					WIN_MOUSE_BUTTONS, ACTION_RESCALE,
					ACTION_CUT, ACTION_COPY, ACTION_PASTE,
					ACTION_SELECT_FIELD_FORWARD, ACTION_FIND_FORWARD,
					/* BUG: enable IM_ISO if ecd_input enabled */
					ACTION_HELP, WIN_EDIT_KEYS, KEY_RIGHT(8), KEY_RIGHT(10),
					KEY_RIGHT(12), KEY_RIGHT(14),
					NULL,
				NULL);
		return XV_OK;
	}
	else {
		return XV_ERROR;
	}
}

const Xv_pkg xv_panel_view_pkg = {
    "PanelView", ATTR_PKG_PANEL,
    sizeof(Xv_panel),
    &xv_canvas_view_pkg,
    panel_view_init,
    NULL,
    NULL,
    NULL,
    NULL			/* no find proc */
};

static int panel_pw_init(Panel_view view, Xv_window pw,
						Attr_attribute  avlist[], int *u)
{
	Xv_Screen screen = (Xv_Screen) xv_get(view, XV_SCREEN);
	Panel pp = xv_get(view, XV_OWNER);
	Panel_info *panel = PANEL_PRIVATE(pp);

	xv_set(pw,
			WIN_RETAINED, ((int)xv_get(screen, SCREEN_RETAIN_WINDOWS)),
			WIN_NOTIFY_SAFE_EVENT_PROC, panel_notify_event,
			WIN_NOTIFY_IMMEDIATE_EVENT_PROC, panel_notify_event,
			WIN_CONSUME_EVENTS,
				WIN_UP_EVENTS, WIN_ASCII_EVENTS, KBD_USE, LOC_DRAG,
				LOC_WINEXIT,
				WIN_MOUSE_BUTTONS, ACTION_RESCALE, ACTION_OPEN,
				ACTION_FRONT, ACTION_CUT, ACTION_COPY, ACTION_PASTE,
				ACTION_SELECT_FIELD_FORWARD, ACTION_FIND_FORWARD,
				/* BUG: enable IM_ISO if ecd_input enabled */
				ACTION_HELP, WIN_EDIT_KEYS, KEY_RIGHT(8), KEY_RIGHT(10),
				KEY_RIGHT(12), KEY_RIGHT(14),
				NULL,
			NULL);

	panel_register_view(panel, view);

	return XV_OK;
}

const Xv_pkg xv_panel_pw_pkg = {
    "PanelPW", ATTR_PKG_PANEL,
    sizeof(Xv_canvas_pw),
    &xv_canvas_pw_pkg,
    panel_pw_init,
    NULL,
    NULL,
    NULL,
    NULL			/* no find proc */
};
