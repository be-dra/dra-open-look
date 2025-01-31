#ifndef lint
char     txt_line_c_sccsid[] = "@(#)txt_line.c 1.26 93/06/28 DRA: $Id: txt_line.c,v 4.5 2024/11/01 11:51:35 dra Exp $";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Text line selection popup frame creation and support.
 */

#include <xview_private/primal.h>
#include <xview_private/txt_impl.h>
#include <xview_private/ev_impl.h>
#include <sys/time.h>
#include <signal.h>
#include <xview/notice.h>
#include <xview/frame.h>
#include <xview/panel.h>
#include <xview/textsw.h>
#include <xview/openmenu.h>
#include <xview/wmgr.h>
#include <xview/pixwin.h>
#include <xview/win_struct.h>
#include <xview/win_screen.h>



#define		MAX_DISPLAY_LENGTH      12
#define   	MAX_STR_LENGTH		8

/*ARGSUSED*/
static int do_sel_line_proc(Textsw_private priv, Frame fram, Panel_item tf)
{
	Es_index prev;
	CHAR buf[10];
	int buf_fill_len;
	char *line_number;
	int line_no;
	Es_index first, last_plus_one;
	Textsw_view_private view = VIEW_PRIVATE(xv_get(XV_PUBLIC(priv), OPENWIN_NTH_VIEW, 0));

	line_number = (char *)xv_get(tf, PANEL_VALUE);
	line_no = atoi(line_number);

	if (line_no == 0) {
		window_bell(XV_PUBLIC(view));
		return TRUE;
	}
	else {
		buf[0] = '\n';
		buf_fill_len = 1;
		if (line_no == 1) {
			prev = 0;
		}
		else {
			ev_find_in_esh(priv->views->esh, buf, buf_fill_len,
					(Es_index) 0, (u_int) line_no - 1, 0, &first, &prev);
			if (first == ES_CANNOT_SET) {
				window_bell(XV_PUBLIC(view));
				return TRUE;
			}
		}
		ev_find_in_esh(priv->views->esh, buf, buf_fill_len,
				prev, 1, 0, &first, &last_plus_one);
		if (first == ES_CANNOT_SET) {
			window_bell(XV_PUBLIC(view));
			return TRUE;
		}
		textsw_possibly_normalize_and_set_selection(VIEW_PUBLIC(view), prev,
				last_plus_one, EV_SEL_PRIMARY);
		(void)textsw_set_insert(priv, last_plus_one);
		(void)xv_set(fram, XV_SHOW, FALSE, NULL);
		return FALSE;
	}
}

static void sel_line_cmd_proc(Panel_item item, Event *event)
{
	Textsw_view_private view = text_view_frm_p_itm(item);
	Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
	Frame fram = frame_from_panel_item(item);
	Panel_item tf = xv_get(item, PANEL_CLIENT_DATA);
	int error;

	error = do_sel_line_proc(priv, fram, tf);

	if (error) {
		xv_set(item, PANEL_NOTIFY_STATUS, XV_ERROR, NULL);
	}
}

/* This creates all of the panel_items */

Pkg_private void textsw_create_sel_line_panel(Frame frame, Textsw_view_private view)
{
    Panel panel;
	Panel_item tf;
	Textsw tsw = xv_get(XV_PUBLIC(view), XV_OWNER);

    panel = xv_get(frame, FRAME_CMD_PANEL);
	xv_set(panel, 
			XV_HELP_DATA, textsw_make_help(tsw, "sellinepanel"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);

	xv_set(panel,
			XV_WIDTH, WIN_EXTEND_TO_EDGE,
			XV_HEIGHT, WIN_EXTEND_TO_EDGE,
			NULL);

    tf = xv_create(panel, PANEL_TEXT,
			PANEL_VALUE_DISPLAY_LENGTH, MAX_DISPLAY_LENGTH,
			PANEL_VALUE_STORED_LENGTH, MAX_STR_LENGTH,
			PANEL_LABEL_STRING, XV_MSG("Line Number:"),
			XV_HELP_DATA, textsw_make_help(tsw, "linenumber"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);

    xv_set(panel, PANEL_CARET_ITEM, tf, NULL);
	window_fit_width(panel);

    xv_create(panel, PANEL_BUTTON,
			PANEL_NEXT_ROW, -1,
			PANEL_ITEM_LAYOUT_ROLE, PANEL_ROLE_CENTER,
			PANEL_LABEL_STRING, XV_MSG("Select Line at Number"),
			PANEL_NOTIFY_PROC, sel_line_cmd_proc,
			PANEL_CLIENT_DATA, tf,
			XV_HELP_DATA, textsw_make_help(tsw, "selectline"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);

    xv_set(panel, PANEL_DO_LAYOUT, NULL);

	window_fit_height(panel);
	frame_fit_all(frame);
}
