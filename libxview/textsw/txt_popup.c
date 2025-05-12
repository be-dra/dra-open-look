#ifndef lint
char     txt_popup_c_sccsid[] = "@(#)txt_popup.c 1.54 93/06/28 DRA: $Id: txt_popup.c,v 4.12 2025/05/11 12:58:23 dra Exp $";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Text subwindow menu creation and support.
 */

#include <xview_private/primal.h>
#include <xview_private/txt_impl.h>
#include <xview_private/svr_impl.h>
#include <xview_private/win_info.h>
#include <xview_private/ev_impl.h>
#include <xview_private/txt_18impl.h>
#include <sys/time.h>
#include <signal.h>
#include <xview/notice.h>
#include <xview/frame.h>
#include <xview/panel.h>
#include <xview/textsw.h>
#include <xview/openmenu.h>
#include <xview/wmgr.h>
#include <xview/pixwin.h>
#include <xview/defaults.h>
#include <xview/win_struct.h>
#include <xview/win_screen.h>
#include <xview/file_chsr.h>

#ifdef SVR4
#include <unistd.h>
#endif /* SVR4 */
 
#define   	MAX_STR_LENGTH		1024
/* This is for select line number */
#define		MAX_SEL_LINE_PANEL_ITEMS  2
/* This is for load, store and include file */
#define		MAX_FILE_PANEL_ITEMS	  3
/* This is for find and replace */
#define		MAX_SEARCH_PANEL_ITEMS	  10
/* This is for find marked text */
#define		MAX_MATCH_PANEL_ITEMS	  6

/* for select line number */
typedef enum {
    SEL_LINE_ITEM = 0,
    SEL_LINE_NUMBER_ITEM = 1
}               Sel_line_panel_item_enum;

/* for load, store and include file */
typedef enum {
    FILE_CMD_ITEM = 0,
    DIR_STRING_ITEM = 1,
    FILE_STRING_ITEM = 2
}               File_panel_item_enum;

/* This is for find marked text */
typedef enum {
    CHOICE_ITEM = 0,
    FIND_PAIR_ITEM = 1,
    FIND_PAIR_CHOICE_ITEM = 2,
    INSERT_ITEM = 3,
    REMOVE_ITEM = 4
} Match_panel_item_enum;

Pkg_private int      STORE_FILE_POPUP_KEY;
Pkg_private int      SAVE_FILE_POPUP_KEY;
Pkg_private int      LOAD_FILE_POPUP_KEY;
Pkg_private int      FILE_STUFF_POPUP_KEY;
Pkg_private int      SEARCH_POPUP_KEY;
Pkg_private int      MATCH_POPUP_KEY;
Pkg_private int      SEL_LINE_POPUP_KEY;
Pkg_private int      TEXTSW_CURRENT_POPUP_KEY;
Pkg_private int      FC_PARENT_KEY;

/* key data holding one of the above key names */
static int      TEXTSW_POPUP_KEY;

static int textfield_key = 0;
static int fc_to_textsw_key = 0;

Panel_item      store_panel_items[MAX_FILE_PANEL_ITEMS];
Panel_item      load_panel_items[MAX_FILE_PANEL_ITEMS];
Panel_item      include_panel_items[MAX_FILE_PANEL_ITEMS];

static Notify_error textsw_popup_destroy_func(Notify_client client,
     Destroy_status status)
{
	Frame popup_frame = client;
	Frame textsw_frame;
	Textsw textsw;
	int popup_key_name;

	popup_key_name = xv_get(popup_frame, XV_KEY_DATA, TEXTSW_POPUP_KEY);
	textsw = xv_get(popup_frame, XV_KEY_DATA, TEXTSW_CURRENT_POPUP_KEY);

	if (textsw) {
		/* CAREFUL: this frame MIGHT already have been destroyed -
		 * especially, if the 'TEXTSW-containing frame' was not a FRAME_BASE.
		 * That is, when you have a textsw in a popup frame and 
		 * create popup windows (like the 'search and replace' frame),
		 * you might run into a problem here when the application is
		 * about to exit.
		 * In this case the 'textsw_frame' is not the PARENT of
		 * the popup window, but the 'brother'.
		 *
		 * However, we have (hopefully) solved the 'brother problem',
		 * see (jkwehrfjkwhef)
		 */
		textsw_frame = xv_get(textsw, WIN_FRAME);
		if (textsw_frame) {
			xv_set(textsw_frame,
				XV_KEY_DATA, popup_key_name, XV_NULL,
				NULL);
		}
	}
	return notify_next_destroy_func(client, status);
}

static void add_exten_item(File_chooser fc);

static char *instname(Textsw tsw, const char *namepart)
{
	static char nambuf[200];
	char *ti = (char *)xv_get(tsw, XV_INSTANCE_NAME);

	sprintf(nambuf, "%s%s", ti ? ti : "textsw", namepart);
	return nambuf;
}

static char *make_title(Xv_server srv, Frame bas, const char *func)
{
	static char *p, buf[200];

	p = (char *)xv_get(srv, XV_APP_NAME);
	if (! p) {
		p = (char *)xv_get(bas, XV_LABEL);
	}

	sprintf(buf, "%s : %s", p, func);
	return buf;
}

Pkg_private void textsw_create_popup_frame(Textsw_view_private view, int popup_type)
{
	Textsw tsw = VIEW_PUBLIC(view);
	Frame frame_parent = xv_get(tsw, WIN_FRAME);
	Frame popup_frame = XV_NULL, base_frame;
	char *label = NULL;
	Xv_server srv = XV_SERVER_FROM_WINDOW(tsw);

#ifdef OW_I18N
	int win_use_im = ((popup_type != TEXTSW_MENU_SEL_MARK_TEXT) &&
			(popup_type != TEXTSW_MENU_NORMALIZE_LINE));
#endif

#ifdef BEFORE_DRA_CHANGED_IT
	/* can lead to problems (jkwehrfjkwhef) */
	base_frame = (xv_get(frame_parent, XV_IS_SUBTYPE_OF, FRAME_BASE) ?
			frame_parent : xv_get(frame_parent, WIN_OWNER));
#else
	base_frame = frame_parent;
#endif

	if (!TEXTSW_POPUP_KEY) {
		TEXTSW_POPUP_KEY = xv_unique_key();
		textfield_key = xv_unique_key();
		fc_to_textsw_key = xv_unique_key();
	}
	switch (popup_type) {
		case TEXTSW_MENU_STORE:
			popup_frame = (Frame) xv_create(base_frame, FILE_CHOOSER,
#ifdef OW_I18N
					WIN_USE_IM, win_use_im,
#endif
					FILE_CHOOSER_TYPE, FILE_CHOOSER_SAVEAS,
#ifdef OW_I18N
					FILE_CHOOSER_NOTIFY_FUNC_WCS, textsw_save_cmd_proc,
#else
					FILE_CHOOSER_NOTIFY_FUNC, textsw_save_cmd_proc,
#endif
					FRAME_SHOW_LABEL, TRUE,
					WIN_CLIENT_DATA, view,
					FILE_CHOOSER_ADD_UI_FUNC, add_exten_item,
					XV_KEY_DATA, fc_to_textsw_key, tsw,
					XV_KEY_DATA, TEXTSW_POPUP_KEY, STORE_FILE_POPUP_KEY,
					NULL);
			xv_set(frame_parent,
					XV_KEY_DATA, STORE_FILE_POPUP_KEY, popup_frame,
					NULL);
			label = XV_MSG("Save As");
			break;

		case TEXTSW_MENU_LOAD:
			popup_frame = (Frame) xv_create(base_frame, FILE_CHOOSER,
#ifdef OW_I18N
					WIN_USE_IM, win_use_im,
#endif
					FILE_CHOOSER_TYPE, FILE_CHOOSER_OPEN,
#ifdef OW_I18N
					FILE_CHOOSER_NOTIFY_FUNC_WCS, textsw_open_cmd_proc,
#else
					FILE_CHOOSER_NOTIFY_FUNC, textsw_open_cmd_proc,
#endif
					FRAME_SHOW_LABEL, TRUE,
					WIN_CLIENT_DATA, view,
					FILE_CHOOSER_ADD_UI_FUNC, add_exten_item,
					XV_KEY_DATA, fc_to_textsw_key, tsw,
					XV_KEY_DATA, TEXTSW_POPUP_KEY, LOAD_FILE_POPUP_KEY,
					NULL);
			xv_set(frame_parent,
					XV_KEY_DATA, LOAD_FILE_POPUP_KEY, popup_frame,
					NULL);
			label = XV_MSG("Open");
			break;

		case TEXTSW_MENU_SAVE:
			popup_frame = (Frame) xv_create(base_frame, FILE_CHOOSER,
#ifdef OW_I18N
					WIN_USE_IM, win_use_im,
#endif
					FILE_CHOOSER_TYPE, FILE_CHOOSER_SAVE,
#ifdef OW_I18N
					FILE_CHOOSER_NOTIFY_FUNC_WCS, textsw_save_cmd_proc,
#else
					FILE_CHOOSER_NOTIFY_FUNC, textsw_save_cmd_proc,
#endif
					FRAME_SHOW_LABEL, TRUE,
					WIN_CLIENT_DATA, view,
					FILE_CHOOSER_ADD_UI_FUNC, add_exten_item,
					XV_KEY_DATA, fc_to_textsw_key, tsw,
					XV_KEY_DATA, TEXTSW_POPUP_KEY, SAVE_FILE_POPUP_KEY,
					NULL);
			xv_set(frame_parent,
					XV_KEY_DATA, SAVE_FILE_POPUP_KEY, popup_frame,
					NULL);

			label = XV_MSG("Save");
			break;

		case TEXTSW_MENU_FILE_STUFF:
			popup_frame =
					(Frame) xv_create(base_frame, FILE_CHOOSER_OPEN_DIALOG,
#ifdef OW_I18N
					WIN_USE_IM, win_use_im,
#endif
					FRAME_SHOW_LABEL, TRUE,
					FILE_CHOOSER_CUSTOMIZE_OPEN,
						XV_MSG("Include"), XV_MSG("Click Select to Include"),
					FILE_CHOOSER_SELECT_FILES,
#ifdef OW_I18N
					FILE_CHOOSER_NOTIFY_FUNC_WCS, textsw_include_cmd_proc,
#else
					FILE_CHOOSER_NOTIFY_FUNC, textsw_include_cmd_proc,
#endif
					WIN_CLIENT_DATA, view,
					FILE_CHOOSER_ADD_UI_FUNC, add_exten_item,
					XV_KEY_DATA, fc_to_textsw_key, tsw,
					XV_KEY_DATA, TEXTSW_POPUP_KEY, FILE_STUFF_POPUP_KEY,
					NULL);

			xv_set(frame_parent,
					XV_KEY_DATA, FILE_STUFF_POPUP_KEY, popup_frame,
					NULL);
			label = XV_MSG("Include");
			xv_set(xv_get(popup_frame, FILE_CHOOSER_CHILD,
						FILE_CHOOSER_CUSTOM_BUTTON_CHILD),
					XV_HELP_DATA, textsw_make_help(tsw, "include_button"),
					XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
					NULL);
			break;

		case TEXTSW_MENU_FIND_AND_REPLACE:
			{
				int pin_in = defaults_get_boolean("text.findAndReplacePinned",
										"Text.FindAndReplacePinned", TRUE);

				popup_frame = xv_create(base_frame, FRAME_CMD,
						XV_INSTANCE_NAME, instname(tsw, "findrepl"),
#ifdef OW_I18N
						WIN_USE_IM, win_use_im,
#endif
						FRAME_SHOW_LABEL, TRUE,
						FRAME_CMD_DEFAULT_PIN_STATE,
							pin_in ? FRAME_CMD_PIN_IN : FRAME_CMD_PIN_OUT,
						WIN_CLIENT_DATA, view,
/* 						WIN_FRONT, */
						XV_KEY_DATA, TEXTSW_POPUP_KEY, SEARCH_POPUP_KEY,
						XV_SET_POPUP, XV_WIDTH, FRAME_CMD_PIN_STATE, NULL,
						NULL);
			}
			xv_set(frame_parent,
					XV_KEY_DATA, SEARCH_POPUP_KEY, popup_frame,
					NULL);
			textsw_create_search_panel(popup_frame, view,textfield_key);
			label = XV_MSG("Find and Replace");
			/* beisst sich das mit XV_SET_POPUP?
				server_register_ui(srv, popup_frame, NULL);
			*/
			break;

		case TEXTSW_MENU_SEL_MARK_TEXT:
			popup_frame = (Frame) xv_create(base_frame, FRAME_CMD,
					XV_INSTANCE_NAME, instname(tsw, "matchpairs"),
#ifdef OW_I18N
					WIN_USE_IM, win_use_im,
#endif
					FRAME_SHOW_LABEL, TRUE,
					WIN_CLIENT_DATA, view,
					XV_KEY_DATA, TEXTSW_POPUP_KEY, MATCH_POPUP_KEY,
					WIN_FRONT,
					NULL);
			xv_set(frame_parent,
					XV_KEY_DATA, MATCH_POPUP_KEY, popup_frame,
					NULL);
			textsw_create_match_panel(popup_frame, view);
			label = XV_MSG("Find Marked Text");
			server_register_ui(srv, popup_frame, NULL);
			break;

		case TEXTSW_MENU_NORMALIZE_LINE:
			popup_frame = (Frame) xv_create(base_frame, FRAME_CMD,
					XV_INSTANCE_NAME, instname(tsw, "lineNum"),
#ifdef OW_I18N
					WIN_USE_IM, win_use_im,
#endif
					FRAME_SHOW_LABEL, TRUE,
					WIN_CLIENT_DATA, view,
					XV_KEY_DATA, TEXTSW_POPUP_KEY, SEL_LINE_POPUP_KEY,
					WIN_FRONT,
					NULL);
			xv_set(frame_parent,
					XV_KEY_DATA, SEL_LINE_POPUP_KEY, popup_frame,
					NULL);
			textsw_create_sel_line_panel(popup_frame, view);
			label = XV_MSG("Line Number");
			server_register_ui(srv, popup_frame, NULL);
			break;
	}



/* 	if (panel) { */
/* 		window_fit(panel); */
/* 		window_fit(popup_frame); */
/* 	} */
	xv_set(popup_frame,
			FRAME_LABEL, make_title(srv, base_frame, label),
			XV_KEY_DATA, TEXTSW_CURRENT_POPUP_KEY,
								TEXTSW_PUBLIC(TSWPRIV_FOR_VIEWPRIV(view)),
			XV_SHOW, TRUE,
			NULL);
	notify_interpose_destroy_func(popup_frame, textsw_popup_destroy_func);
}

Pkg_private void textsw_get_and_set_selection(Frame popup_frame, Textsw_view_private view, int popup_type)
{
	Es_index dummy;
	CHAR show_str[MAX_STR_LENGTH];
	Textsw tsw = TEXTSW_PUBLIC(TSWPRIV_FOR_VIEWPRIV(view));

	show_str[0] = '\0';
	xv_set(popup_frame, XV_KEY_DATA, TEXTSW_CURRENT_POPUP_KEY, tsw, NULL);

	switch (popup_type) {
		case TEXTSW_MENU_STORE:
		case TEXTSW_MENU_LOAD:
		case TEXTSW_MENU_FILE_STUFF:
			xv_set(popup_frame, FILE_CHOOSER_UPDATE, NULL);
			break;
		case TEXTSW_MENU_FIND_AND_REPLACE:
			textsw_get_selection(view, &dummy, &dummy, show_str,MAX_STR_LENGTH);
			xv_set(xv_get(popup_frame, XV_KEY_DATA, textfield_key),
					PANEL_VALUE, show_str, NULL);
			textsw_update_replace(popup_frame,
							(int)xv_get(tsw, TEXTSW_READ_ONLY));
			break;
		case TEXTSW_MENU_NORMALIZE_LINE:
			break;

	}

	(void)xv_set(popup_frame, XV_SHOW, TRUE, WIN_CLIENT_DATA, view, NULL);

#undef PANEL_SET_VALUE
}

Pkg_private void textsw_set_dir_str(int popup_type)
{
/*
    char            curr_dir[MAX_STR_LENGTH];

    (void) getcwd(curr_dir, MAX_STR_LENGTH);
    switch (popup_type) {
      case TEXTSW_MENU_STORE:
	(void) panel_set_value(store_panel_items[(int) DIR_STRING_ITEM], curr_dir);
	break;
      case TEXTSW_MENU_LOAD:
	(void) panel_set_value(load_panel_items[(int) DIR_STRING_ITEM], curr_dir);
	break;
      case TEXTSW_MENU_FILE_STUFF:
	(void) panel_set_value(include_panel_items[(int) DIR_STRING_ITEM],
			       curr_dir);
	break;
    }
*/
}

Pkg_private Textsw_view_private text_view_frm_p_itm(Panel_item panel_item)
{
    Panel   panel = xv_get(panel_item, XV_OWNER, 0);
    Xv_Window search_frame = xv_get(panel, WIN_FRAME);
    Textsw_view_private view = (Textsw_view_private) xv_get(search_frame, WIN_CLIENT_DATA, 0);

    return (view);
}

Pkg_private     Xv_Window frame_from_panel_item(Panel_item panel_item)
{
    Panel           panel = xv_get(panel_item, XV_OWNER, 0);
    Xv_Window       popup_frame = xv_get(panel, WIN_FRAME);

    return (popup_frame);
}

Pkg_private int textsw_get_selection(Textsw_view_private view,
    				Es_index *first, Es_index *last_plus_one,
					CHAR *selected_str, int  max_str_len)
{
    /*
     * Return true iff primary selection is in the textsw of current view. If
     * there is a selection in any of the textsw and selected_str is not
     * null, then it will copy it to selected_str.
     */
    Textsw_private    priv = TSWPRIV_FOR_VIEWPRIV(view);
	Es_index f, lpo;
	char *mybuf;
	int	buf_len;

	if (! textsw_get_local_selected_text(priv, EV_SEL_PRIMARY, &f, &lpo,
					&mybuf, &buf_len))
	{
		return FALSE;
	}

	if (selected_str) {
		strncpy(selected_str, mybuf, (size_t)max_str_len);
		selected_str[max_str_len] = '\0';
	}
    *first = f;
    *last_plus_one = lpo;

    return ((*first != ES_CANNOT_SET) && (*last_plus_one != ES_CANNOT_SET));
}

static void show_dot_files_proc(Panel_choice_item item,int value,Event *event);

static void add_exten_item(File_chooser fc)
{
    Panel panel;
	Textsw tsw = xv_get(fc, XV_KEY_DATA, fc_to_textsw_key);

    panel = xv_get(fc, FRAME_CMD_PANEL);
    
    xv_create(panel, PANEL_CHOICE,
				PANEL_LABEL_STRING, XV_MSG("Hidden Files:"),
				PANEL_NEXT_ROW, -1,
				PANEL_ITEM_LAYOUT_ROLE, PANEL_ROLE_LEADER,
				PANEL_CHOICE_STRINGS, XV_MSG("Hide"), XV_MSG("Show"), NULL,
				PANEL_NOTIFY_PROC, show_dot_files_proc,
				XV_KEY_DATA, FC_PARENT_KEY, fc,
				XV_HELP_DATA, textsw_make_help(tsw, "hiddenfileschoice"),
				XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
				NULL);
}

static void show_dot_files_proc(Panel_choice_item item, int value, Event *event)
{
    File_chooser fc = xv_get(item, XV_KEY_DATA, FC_PARENT_KEY);

    xv_set(fc, FILE_CHOOSER_SHOW_DOT_FILES, value, NULL);
}
