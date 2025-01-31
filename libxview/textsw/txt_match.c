#ifndef lint
char     txt_match_c_sccsid[] = "@(#)txt_match.c 1.33 93/06/28 DRA: $Id: txt_match.c,v 4.6 2025/01/01 20:35:21 dra Exp $";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Text match delimiter popup frame creation and support.
 */

#include <xview_private/primal.h>
#include <xview_private/txt_impl.h>
#include <xview_private/ev_impl.h>
#include <xview_private/txt_18impl.h>
#include <xview_private/svr_impl.h>
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

#define		MAX_DISPLAY_LENGTH	50
#define   	MAX_STR_LENGTH		1024

static int key_choice = 0;

Pkg_private int textsw_match_selection_and_normalize(Textsw_view_private view, CHAR *start_marker, unsigned field_flag);

#ifdef OW_I18N

static CHAR l_curly_brace[] = { '{', 0 };
static CHAR l_paren[] = { '(', 0 };
static CHAR dbl_quote[] = { '"', 0 };
static CHAR sgl_quote[] = { '\'', 0 };
static CHAR accent_grave[] = { '`', 0 };
static CHAR l_square_brace[] = { '[', 0 };
static CHAR bar_gt[] = { '|', '>', 0 };
static CHAR open_comment[] = { '/', '*', 0 };

static CHAR r_curly_brace[] = { '}', 0 };
static CHAR r_paren[] = { ')', 0 };
static CHAR r_square_brace[] = { ']', 0 };
static CHAR lt_bar[] = { '<', '|', 0 };
static CHAR close_comment[] = { '*', '/', 0 };

static CHAR    *delimiter_pairs[2][8] = {
    { l_paren, dbl_quote, sgl_quote, accent_grave, bar_gt, l_square_brace,
      l_curly_brace, open_comment,},
    { r_paren, dbl_quote, sgl_quote, accent_grave, lt_bar, r_square_brace,
      r_curly_brace, close_comment,}
};

#else /* OW_I18N */

static char    *delimiter_pairs[2][8] = {
    {"(", "\"", "'", "`", "|>", "[", "{", "/*"},
    {")", "\"", "'", "`", "<|", "]", "}", "*/"}
};
#endif /* OW_I18N */

static void do_insert_or_remove_delimiter(Textsw_view_private view, int value, int do_insert)
{
	Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
	Es_index first, last_plus_one, ro_bdry, num_of_byte, temp;
	CHAR *buf, *sel_str, *temp_ptr;
	Xv_Notice text_notice;
	Frame frame;


	frame = xv_get(TEXTSW_PUBLIC(priv), WIN_FRAME);

	if (TXTSW_IS_READ_ONLY(priv)) {
		text_notice = xv_get(frame, XV_KEY_DATA, text_notice_key);
		if (!text_notice) {
			text_notice = xv_create(frame, NOTICE, NULL);

			xv_set(frame, XV_KEY_DATA, text_notice_key, text_notice, NULL);
		}
		xv_set(text_notice,
				NOTICE_MESSAGE_STRINGS, XV_MSG("Operation is aborted.\n\
This text window is read only."), NULL,
				NOTICE_BUTTON_YES, XV_MSG("Continue"),
				XV_SHOW, TRUE,
				NULL);
		return;
	}
	ro_bdry = textsw_read_only_boundary_is_at(priv);
	(void)ev_get_selection(priv->views, &first, &last_plus_one,
			EV_SEL_PRIMARY);
	if ((ro_bdry != 0) && (last_plus_one <= ro_bdry)) {
		text_notice = xv_get(frame, XV_KEY_DATA, text_notice_key);

		if (!text_notice) {
			text_notice = xv_create(frame, NOTICE, NULL);

			xv_set(frame, XV_KEY_DATA, text_notice_key, text_notice, NULL);
		}
		xv_set(text_notice,
				NOTICE_MESSAGE_STRINGS,
					XV_MSG("Operation is aborted.\n\
Selected text is in read only area."),
					NULL,
				NOTICE_BUTTON_YES, XV_MSG("Continue"),
				XV_SHOW, TRUE,
				NULL);
		return;

	}
	buf = sel_str = temp_ptr = NULL;
	if (do_insert) {
		temp_ptr = buf = MALLOC((size_t)((last_plus_one - first) + 5));
		buf[0] = '\0';
		STRCPY(buf, delimiter_pairs[0][value]);
		temp_ptr = buf + STRLEN(buf);

		if (first < last_plus_one) {
			sel_str = MALLOC((size_t)(last_plus_one - first + 1));
			if (textsw_get_selection_as_string(priv, EV_SEL_PRIMARY, sel_str,
							(int)((last_plus_one - first) + 1))) {
				STRCPY(temp_ptr, sel_str);
				temp_ptr = buf + STRLEN(buf);
			}
		}
		else {
			first = last_plus_one = EV_GET_INSERT(priv->views);
		}
		STRCPY(temp_ptr, delimiter_pairs[1][value]);
	}
	else {
		int sel_str_len = last_plus_one - first;
		int del_len1 = STRLEN(delimiter_pairs[0][value]);
		int del_len2 = STRLEN(delimiter_pairs[1][value]);

		buf = MALLOC((size_t)sel_str_len);
		if (first < last_plus_one) {
			sel_str = MALLOC((size_t)sel_str_len + 1);
			if (textsw_get_selection_as_string(priv, EV_SEL_PRIMARY, sel_str,
							(sel_str_len + 1))) {
				temp_ptr = sel_str + (sel_str_len - del_len2);

				if ((STRNCMP(delimiter_pairs[0][value], sel_str,
										(size_t)del_len1) == 0)
						&& (STRNCMP(delimiter_pairs[1][value], temp_ptr,
										(size_t)del_len2) == 0)) {
					STRNCPY(buf, sel_str + del_len1,
							(size_t)(sel_str_len - (del_len1 + del_len2)));
					buf[(sel_str_len - (del_len1 + del_len2))] = '\0';
				}
				else {
					text_notice = xv_get(frame, XV_KEY_DATA, text_notice_key);

					if (!text_notice) {
						text_notice = xv_create(frame, NOTICE, NULL);

						xv_set(frame,
								XV_KEY_DATA, text_notice_key, text_notice,
								NULL);
					}
					xv_set(text_notice,
							NOTICE_MESSAGE_STRINGS,
								XV_MSG("Operation is aborted.\n\
Selection does not include the indicated pair."),
								NULL,
							NOTICE_BUTTON_YES, XV_MSG("Continue"),
							XV_SHOW, TRUE,
							NULL);
					return;
				}
			}
		}
		else {
			text_notice = xv_get(frame, XV_KEY_DATA, text_notice_key);

			if (!text_notice) {
				text_notice = xv_create(frame, NOTICE, NULL);

				xv_set(frame, XV_KEY_DATA, text_notice_key, text_notice, NULL);
			}
			xv_set(text_notice,
					NOTICE_MESSAGE_STRINGS,
						XV_MSG("Operation is aborted, because no text is selected"),
						NULL,
					NOTICE_BUTTON_YES, XV_MSG("Continue"),
					XV_SHOW, TRUE,
					NULL);
			return;
		}
	}

#ifdef OW_I18N
	textsw_implicit_commit(priv);
#endif

	num_of_byte =
			textsw_replace(TEXTSW_PUBLIC(priv), first, last_plus_one, buf,
			(long)STRLEN(buf));

	if (num_of_byte != 0)
		EV_SET_INSERT(priv->views, last_plus_one + num_of_byte, temp);
	if (buf != NULL)
		free(buf);
	if (sel_str != NULL)
		free(sel_str);

}

static void note_find_pair_forward(Menu menu, Menu_item it)
{
	Textsw_view_private view= (Textsw_view_private)xv_get(menu, MENU_CLIENT_DATA);
	Frame fram = xv_get(menu, XV_KEY_DATA, key_choice);
	int delimiter_value =
			(int)xv_get(xv_get(fram, XV_KEY_DATA, key_choice), PANEL_VALUE);

	textsw_match_selection_and_normalize(view,
					delimiter_pairs[0][delimiter_value],
					TEXTSW_DELIMITER_FORWARD);
}

static void note_find_pair_backward(Menu menu, Menu_item it)
{
	Textsw_view_private view= (Textsw_view_private)xv_get(menu, MENU_CLIENT_DATA);
	Frame fram = xv_get(menu, XV_KEY_DATA, key_choice);
	int delimiter_value =
			(int)xv_get(xv_get(fram, XV_KEY_DATA, key_choice), PANEL_VALUE);

	textsw_match_selection_and_normalize(view,
					delimiter_pairs[1][delimiter_value],
					TEXTSW_DELIMITER_BACKWARD);
}

static void note_find_pair_expand(Menu menu, Menu_item it)
{
	Textsw_view_private view= (Textsw_view_private)xv_get(menu, MENU_CLIENT_DATA);
	Frame fram = xv_get(menu, XV_KEY_DATA, key_choice);
	int delimiter_value =
			(int)xv_get(xv_get(fram, XV_KEY_DATA, key_choice), PANEL_VALUE);

	Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
	Es_index first, last_plus_one;

	first = last_plus_one = EV_GET_INSERT(priv->views);
	(void)textsw_match_field_and_normalize(view, &first,
					&last_plus_one, delimiter_pairs[1][delimiter_value],
					(unsigned)STRLEN(delimiter_pairs[1][delimiter_value]),
					TEXTSW_DELIMITER_ENCLOSE, FALSE);
}

static void note_insert_pair(Panel_item item)
{
	Textsw_view_private view = text_view_frm_p_itm(item);
	Frame fram = frame_from_panel_item(item);
	int delimiter_value =
			(int)xv_get(xv_get(fram, XV_KEY_DATA, key_choice), PANEL_VALUE);

	(void)do_insert_or_remove_delimiter(view, delimiter_value, TRUE);
}

static void note_remove_pair(Panel_item item)
{
	Textsw_view_private view = text_view_frm_p_itm(item);
	Frame fram = frame_from_panel_item(item);
	int delimiter_value =
			(int)xv_get(xv_get(fram, XV_KEY_DATA, key_choice), PANEL_VALUE);

	(void)do_insert_or_remove_delimiter(view, delimiter_value, FALSE);
}

static Panel create_match_items(Frame fram, Textsw_view_private view)
{
	char *curly_bracket = " { }  ";
	char *parenthesis = " ( )  ";
	char *double_quote = " \" \"  ";
	char *single_quote = " ' '  ";
	char *back_qoute = " ` `  ";
	char *square_bracket = " [ ]  ";
	char *field_marker = " |> <|  ";
	char *comment = " /* */  ";

	char *Insert_Pair = XV_MSG("Insert Pair");
	char *Backward = XV_MSG("Backward");
	char *Expand = XV_MSG("Expand");
	char *Forward = XV_MSG("Forward");
	char *Remove_Pair = XV_MSG("Remove Pair");
	char *Find_Pair = XV_MSG("Find Pair");
    Panel           panel;
	Panel_item ch, it;
	Menu menu;
	char nambuf[100];
	Xv_server srv = XV_SERVER_FROM_WINDOW(fram);
	Textsw tsw = xv_get(XV_PUBLIC(view), XV_OWNER);

    panel = xv_get(fram, FRAME_CMD_PANEL);
	xv_set(panel,
			XV_HELP_DATA, textsw_make_help(tsw, "fieldpanel"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);

	ch = xv_create(panel, PANEL_CHOICE,
			XV_X, 10,
			XV_Y, 8,
			PANEL_CHOICE_STRINGS,
				parenthesis,
				double_quote,
				single_quote,
				back_qoute,
				field_marker,
				square_bracket,
				curly_bracket,
				comment,
				NULL,
			XV_HELP_DATA, textsw_make_help(tsw, "fieldchoice"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);

	window_fit_width(panel);
	sprintf(nambuf, "%s.pairDirMenu", (char *)xv_get(fram, XV_INSTANCE_NAME));

	it = xv_create(panel, PANEL_BUTTON,
			PANEL_NEXT_ROW, -1,
			PANEL_ITEM_LAYOUT_ROLE, PANEL_ROLE_CENTER,
			PANEL_LABEL_STRING, Find_Pair,
			PANEL_ITEM_MENU, menu = xv_create(srv, MENU,
						XV_INSTANCE_NAME, nambuf,
						MENU_CLIENT_DATA, view,
						XV_KEY_DATA, key_choice, fram,
						XV_HELP_DATA, textsw_make_help(tsw, "findpairchoice"),
						XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
						MENU_ITEM,
							MENU_STRING, Forward,
							MENU_NOTIFY_PROC, note_find_pair_forward,
							NULL,
						MENU_ITEM,
							MENU_STRING, Backward,
							MENU_NOTIFY_PROC, note_find_pair_backward,
							NULL,
						MENU_ITEM,
							MENU_STRING, Expand,
							MENU_NOTIFY_PROC, note_find_pair_expand,
							NULL,
						XV_SET_MENU, fram,
						NULL),
			XV_HELP_DATA, textsw_make_help(tsw, "findpair"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);

	xv_set(panel, PANEL_DEFAULT_ITEM, it, NULL);

	xv_create(panel, PANEL_BUTTON,
			PANEL_ITEM_LAYOUT_ROLE, PANEL_ROLE_CENTER,
			PANEL_LABEL_STRING, Insert_Pair,
			PANEL_NOTIFY_PROC, note_insert_pair,
			XV_HELP_DATA, textsw_make_help(tsw, "insertpair"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);

	xv_create(panel, PANEL_BUTTON,
			PANEL_ITEM_LAYOUT_ROLE, PANEL_ROLE_CENTER,
			PANEL_LABEL_STRING, Remove_Pair,
			PANEL_NOTIFY_PROC, note_remove_pair,
			XV_HELP_DATA, textsw_make_help(tsw, "removepair"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);

	xv_set(fram, XV_KEY_DATA, key_choice, ch, NULL);

	xv_set(panel, PANEL_DO_LAYOUT, NULL);
	xv_set(panel,
			XV_WIDTH, WIN_EXTEND_TO_EDGE,
			XV_HEIGHT, WIN_EXTEND_TO_EDGE,
			NULL);
	frame_fit_all(fram);

	return panel;
}

Pkg_private Panel textsw_create_match_panel(Frame frame, Textsw_view_private view)
{
	if (! key_choice) {
		key_choice = xv_unique_key();
	}

    return create_match_items(frame, view);
}
