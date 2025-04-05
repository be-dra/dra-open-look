#ifndef lint
char     tty_menu_c_sccsid[] = "@(#)tty_menu.c 20.68 93/06/28 DRA: $Id: tty_menu.c,v 4.12 2025/04/04 20:13:02 dra Exp $";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */

/*
 * Ttysw menu initialization and call-back procedures
 */

#include <xview/frame.h>
#include <xview/ttysw.h>
#include <xview/notice.h>
#include <xview/openmenu.h>
#include <xview_private/win_info.h>
#include <xview_private/tty_impl.h>
#include <xview_private/term_impl.h>
#include <xview_private/txt_impl.h>

#define HELP_INFO(s) XV_HELP_DATA,s

Xv_private void textsw_file_do_menu_action(Menu , Menu_item);

#define EDITABLE		0
#define READ_ONLY		1
#define ENABLE_SCROLL		2
#define DISABLE_SCROLL		3



/* shorthand */
#define	iwbp	ttysw->ttysw_ibuf.cb_wbp
#define	irbp	ttysw->ttysw_ibuf.cb_rbp

/* ttysw walking menu definitions */

static Menu_item ttysw_menu_page_state(Menu_item mi, Menu_generate op);
Pkg_private void ttysw_show_walkmenu(Tty_view anysw_view_public, Event *event);

static void ttysw_enable_scrolling(Menu menu, Menu_item mi);
static void ttysw_disable_scrolling(Menu cmd_menu, Menu_item cmd_item);
static void ttysw_menu_page(Menu menu, Menu_item mi);
static void ttysw_menu_copy(Menu menu, Menu_item mi);
static void ttysw_menu_paste(Menu menu, Menu_item mi);
static void ttysw_mode_action(Menu cmd_menu, Menu_item cmd_item);


/* termsw walking menu definitions */

static int ITEM_DATA_KEY;

/* ttysw walking menu utilities */


Pkg_private Menu ttysw_walkmenu(Tty ttysw_folio_public)
{	   /* This create a ttysw menu */
	Menu ttysw_menu;
	Frame fram = xv_get(ttysw_folio_public, WIN_FRAME);

	ttysw_menu = xv_create(XV_SERVER_FROM_WINDOW(ttysw_folio_public), MENU,
			XV_INSTANCE_NAME, "ttysw_menu",
			XV_SET_MENU, fram,
			XV_HELP_DATA, "ttysw:menu",
			MENU_TITLE_ITEM, XV_MSG("Term Pane"),
			MENU_ITEM,
				MENU_STRING, XV_MSG("Disable Page Mode"),
				MENU_ACTION, ttysw_menu_page,
				MENU_GEN_PROC, ttysw_menu_page_state,
				MENU_CLIENT_DATA, ttysw_folio_public,
				XV_HELP_DATA, "ttysw:mdsbpage",
				NULL,
			MENU_ITEM,
				MENU_STRING, XV_MSG("Copy"),
				MENU_ACTION, ttysw_menu_copy,
				MENU_CLIENT_DATA, ttysw_folio_public,
				XV_HELP_DATA, "ttysw:mcopy",
				NULL,
			MENU_ITEM,
				MENU_STRING, XV_MSG("Paste"),
				MENU_ACTION, ttysw_menu_paste,
				MENU_CLIENT_DATA, ttysw_folio_public,
				XV_HELP_DATA, "ttysw:mpaste",
				NULL,
			NULL);

	if (IS_TERMSW(ttysw_folio_public)) {
		xv_set(ttysw_menu,
				MENU_ITEM,
					MENU_STRING, XV_MSG("Enable Scrolling"),
					MENU_ACTION, ttysw_enable_scrolling,
					MENU_CLIENT_DATA, ttysw_folio_public,
					XV_HELP_DATA, "ttysw:menscroll",
					NULL,
				NULL);
	}
	return ttysw_menu;
}


Pkg_private void ttysw_show_walkmenu(Tty_view anysw_view_public, Event *event)
{
	register Menu menu;

	if (IS_TTY_VIEW(anysw_view_public)) {
		menu = (Menu) xv_get(TTY_FROM_TTY_VIEW(anysw_view_public), WIN_MENU);
	}
	else {
		Ttysw_private ttysw = TTY_PRIVATE_FROM_TERMSW_VIEW(anysw_view_public);
		Termsw_folio termsw = TERMSW_FOLIO_FROM_TERMSW_VIEW(anysw_view_public);

		if (ttysw_getopt(ttysw, TTYOPT_TEXT)) {
			ttysw->current_view_public = anysw_view_public;
			menu = termsw->text_menu;
			xv_set(menu,
					XV_KEY_DATA, TEXTSW_MENU_DATA_KEY, anysw_view_public,
					NULL);
		}
		else if (ttysw->current_view_public == anysw_view_public)
			menu = termsw->tty_menu;
		else {
			menu = termsw->text_menu;
			xv_set(menu,
					XV_KEY_DATA, TEXTSW_MENU_DATA_KEY, anysw_view_public,
					NULL);
		}
	}

	if (!menu)
		return;

	/* insure that there are no caret render race conditions */
	termsw_menu_set();
	xv_set(menu, MENU_DONE_PROC, termsw_menu_clr, NULL);

	menu_show(menu, anysw_view_public, event, NULL);
}


/*
 * Menu item gen procs
 */
static Menu_item ttysw_menu_page_state(Menu_item mi, Menu_generate op)
{
    Tty             ttysw_public;
    Ttysw_private     ttysw;

    if (op == MENU_DISPLAY_DONE) return mi;

    ttysw_public = xv_get(mi, MENU_CLIENT_DATA);
    ttysw = TTY_PRIVATE_FROM_ANY_PUBLIC(ttysw_public);


    if (ttysw->ttysw_flags & TTYSW_FL_FROZEN)
		xv_set(mi, MENU_STRING, XV_MSG("Continue"),
				XV_HELP_DATA, "ttysw:mcont",
				NULL);
    else if (ttysw_getopt(ttysw, TTYOPT_PAGEMODE))
		xv_set(mi, MENU_STRING, XV_MSG("Disable Page Mode"),
				XV_HELP_DATA, "ttysw:mdsbpage",
				NULL);
    else
		xv_set(mi, MENU_STRING, XV_MSG("Enable Page Mode "),
				XV_HELP_DATA, "ttysw:menbpage",
				NULL);
    return mi;
}


#define __TTY_VIEW_HANDLE_FROM_TTY_FOLIO(_tty_folio_private) \
	 ((Ttysw_view_handle)(((Ttysw_private)_tty_folio_private)->view))

/*
 * Callout functions
 */
static void ttysw_menu_page(Menu menu, Menu_item mi)
{
	/* Looks like we are trying to get the value of MENU_CLIENT_DATA; for
	 * this we have to use xv_get, not menu_get. [vmh - 7/19/90]
	 */
	Tty ttysw_public = xv_get(mi, MENU_CLIENT_DATA);
	Ttysw_private ttysw = TTY_PRIVATE_FROM_ANY_PUBLIC(ttysw_public);

	if (ttysw->ttysw_flags & TTYSW_FL_FROZEN)
		ttysw_freeze(ttysw->view, 0);
	else
		ttysw_setopt(ttysw, TTYOPT_PAGEMODE,
				!ttysw_getopt(ttysw, TTYOPT_PAGEMODE));
}

/* ARGSUSED */


static void ttysw_menu_copy(Menu menu, Menu_item mi)
{
	/* Looks like we are trying to gte the value of MENU_CLIENT_DATA; for
	 * this we have to use xv_get, not menu_get. [vmh - 7/19/90]
	 */
	Tty ttysw_public = xv_get(mi, MENU_CLIENT_DATA);
	Ttysw_private ttysw = TTY_PRIVATE_FROM_ANY_PUBLIC(ttysw_public);
	Xv_Notice tty_notice;

	if (!ttysw_do_copy(ttysw)) {
		Frame frame = xv_get(ttysw_public, WIN_FRAME);

		tty_notice = xv_get(frame, XV_KEY_DATA, tty_notice_key, NULL);

		if (!tty_notice) {
			tty_notice = xv_create(frame, NOTICE,
					NOTICE_BUTTON_YES, XV_MSG("Continue"),
					NOTICE_MESSAGE_STRINGS,
						XV_MSG("Please make a primary selection first."),
						NULL,
					NOTICE_BUSY_FRAMES,
						frame,
						NULL,
					XV_SHOW, TRUE,
					NULL);

			xv_set(frame, XV_KEY_DATA, tty_notice_key, tty_notice, NULL);

		}
		else {
			xv_set(tty_notice,
					NOTICE_BUTTON_YES, XV_MSG("Continue"),
					NOTICE_MESSAGE_STRINGS,
						XV_MSG("Please make a primary selection first."),
						NULL,
					NOTICE_BUSY_FRAMES,
						frame,
						NULL,
					XV_SHOW, TRUE,
					NULL);
		}
	}
}

/*ARGSUSED*/
static void ttysw_menu_paste(Menu menu, Menu_item mi)
{
	Tty ttysw_public = xv_get(mi, MENU_CLIENT_DATA);
	Ttysw_private ttysw = TTY_PRIVATE_FROM_ANY_PUBLIC(ttysw_public);
	Xv_Notice tty_notice;

#ifdef OW_I18N
	ttysw_implicit_commit(ttysw, 1);
#endif

	if (!ttysw_do_paste(ttysw)) {
		Frame frame = xv_get(ttysw_public, WIN_FRAME);

		tty_notice = xv_get(frame, XV_KEY_DATA, tty_notice_key, NULL);
		if (!tty_notice) {
			tty_notice = xv_create(frame, NOTICE,
					NOTICE_BUTTON_YES, XV_MSG("Continue"),
					NOTICE_MESSAGE_STRINGS,
						XV_MSG("Please Copy text onto clipboard first."),
						NULL,
					NOTICE_BUSY_FRAMES,
						frame,
						NULL,
					XV_SHOW, TRUE,
					NULL);

			xv_set(frame, XV_KEY_DATA, tty_notice_key, tty_notice, NULL);

		}
		else {
			xv_set(tty_notice,
					NOTICE_BUTTON_YES, XV_MSG("Continue"),
					NOTICE_MESSAGE_STRINGS,
						XV_MSG("Please Copy text onto clipboard first."),
						NULL,
					NOTICE_BUSY_FRAMES,
						frame,
						NULL,
					XV_SHOW, TRUE,
					NULL);
		}
	}
}

static void fit_termsw_panel_and_textsw(Frame frame, Termsw_folio termsw_folio)
{
    Rect            rect, panel_rect, textsw_rect;
    int             termsw_height;

    rect = *((Rect *) xv_get(TERMSW_PUBLIC(termsw_folio), WIN_RECT));

    termsw_height = (rect.r_height / 3);
    xv_set(TERMSW_PUBLIC(termsw_folio), XV_HEIGHT, termsw_height, NULL);

    panel_rect = *((Rect *) xv_get(termsw_folio->textedit_panel, WIN_RECT));
    panel_rect.r_left = rect.r_left;
    panel_rect.r_top = rect.r_top + termsw_height;
    panel_rect.r_width = rect.r_width;

    xv_set(termsw_folio->textedit_panel,
				WIN_RECT, &panel_rect,
				XV_SHOW, TRUE,
				NULL);

    textsw_rect.r_left = panel_rect.r_left;
    textsw_rect.r_top = panel_rect.r_top + panel_rect.r_height;
    textsw_rect.r_width = panel_rect.r_width;
    if ((textsw_rect.r_height =
                rect.r_height - (panel_rect.r_top + panel_rect.r_height)) <= 0)
        textsw_rect.r_height = 1;


    xv_set(termsw_folio->textedit,
				WIN_RECT, &textsw_rect,
				XV_SHOW, TRUE,
				NULL);

    window_fit(frame);
    xv_set(termsw_folio->textedit_panel, XV_WIDTH, WIN_EXTEND_TO_EDGE, NULL);
    xv_set(termsw_folio->textedit,
				XV_WIDTH, WIN_EXTEND_TO_EDGE,
				XV_HEIGHT, WIN_EXTEND_TO_EDGE,
				NULL);
}

static void panel_button_proc(Panel_item *item, Event *event)
{
	Textsw textsw =
			(Textsw) xv_get((Xv_opaque) item, XV_KEY_DATA, ITEM_DATA_KEY);
	Menu menu = (Menu) xv_get((Xv_opaque) item, PANEL_ITEM_MENU);
	Panel p_menu = (Panel) xv_get(menu, MENU_PIN_WINDOW);
	Menu_item menu_item;
	Menu pullr_menu;
	int num_items, i;

	xv_set(menu,
		XV_KEY_DATA, TEXTSW_MENU_DATA_KEY, xv_get(textsw, OPENWIN_NTH_VIEW, 0),
		NULL);
	if (p_menu) {
		num_items = (int)xv_get(menu, MENU_NITEMS);
		for (i = 1; i <= num_items; i++) {
			menu_item = (Menu_item) xv_get(menu, MENU_NTH_ITEM, i);
			if (menu_item) {
				pullr_menu = (Menu) xv_get(menu_item, MENU_PULLRIGHT);
				if (pullr_menu)
					xv_set(pullr_menu,
						XV_KEY_DATA, TEXTSW_MENU_DATA_KEY,
								xv_get(textsw, OPENWIN_NTH_VIEW, 0),
						NULL);
			}
		}
	}
}

static void create_textedit_panel_item(Panel panel, Textsw textsw)
{
    Panel_item file_panel_item, edit_panel_item, display_panel_item, find_panel_item;

    if (!ITEM_DATA_KEY)
		ITEM_DATA_KEY = xv_unique_key();

    file_panel_item = xv_create(panel, PANEL_BUTTON,
				PANEL_LABEL_STRING, XV_MSG("File"),
				PANEL_NOTIFY_PROC, panel_button_proc,
				PANEL_ITEM_MENU, xv_get(textsw, TEXTSW_SUBMENU_FILE),
				NULL);
    display_panel_item = xv_create(panel, PANEL_BUTTON,
				PANEL_LABEL_STRING, XV_MSG("View"),
				PANEL_NOTIFY_PROC, panel_button_proc,
				PANEL_ITEM_MENU, (Menu) xv_get(textsw, TEXTSW_SUBMENU_VIEW),
				NULL);

    edit_panel_item = xv_create(panel, PANEL_BUTTON,
				PANEL_LABEL_STRING, XV_MSG("Edit"),
				PANEL_NOTIFY_PROC, panel_button_proc,
				PANEL_ITEM_MENU, (Menu) xv_get(textsw, TEXTSW_SUBMENU_EDIT),
				NULL);

    find_panel_item = xv_create(panel, PANEL_BUTTON,
				PANEL_LABEL_STRING, XV_MSG("Find"),
				PANEL_NOTIFY_PROC, panel_button_proc,
				PANEL_ITEM_MENU, (Menu) xv_get(textsw, TEXTSW_SUBMENU_FIND),
				NULL);

    xv_set(file_panel_item, XV_KEY_DATA, ITEM_DATA_KEY, textsw, NULL);
    xv_set(display_panel_item, XV_KEY_DATA, ITEM_DATA_KEY, textsw, NULL);
    xv_set(edit_panel_item, XV_KEY_DATA, ITEM_DATA_KEY, textsw, NULL);
    xv_set(find_panel_item, XV_KEY_DATA, ITEM_DATA_KEY, textsw, NULL);

    window_fit_height(panel);
}


static void ttysw_enable_editor(Menu do_not_use_cmd_menu, Menu_item item)
{
	Textsw textsw = xv_get(item, MENU_CLIENT_DATA); /* == termsw */
	Frame frame = xv_get(textsw, WIN_FRAME);
	register Termsw_folio termsw_folio =
			TERMSW_FOLIO_FOR_VIEW(TERMSW_VIEW_PRIVATE_FROM_TEXTSW(textsw));
	Xv_opaque my_font = xv_get(textsw, XV_FONT);
	Xv_Notice tty_notice;

	if ((int)xv_get(textsw, OPENWIN_NVIEWS) >= 2) {
		tty_notice = xv_get(frame, XV_KEY_DATA, tty_notice_key, NULL);
		if (!tty_notice) {
			tty_notice = xv_create(frame, NOTICE,
					NOTICE_BUTTON_YES, XV_MSG("Continue"),
					NOTICE_MESSAGE_STRINGS,
						XV_MSG("Please destroy all split views before enabling File Editor.\n\
Press \"Continue\" to proceed."),
						NULL,
					NOTICE_BUSY_FRAMES, frame, NULL,
					XV_SHOW, TRUE,
					NULL);

			xv_set(frame, XV_KEY_DATA, tty_notice_key, tty_notice, NULL);
		}
		else {
			xv_set(tty_notice,
					NOTICE_BUTTON_YES, XV_MSG("Continue"),
					NOTICE_MESSAGE_STRINGS,
						XV_MSG("Please destroy all split views before enabling File Editor.\n\
Press \"Continue\" to proceed."),
						NULL,
					NOTICE_BUSY_FRAMES, frame, NULL,
					XV_SHOW, TRUE,
					NULL);
		}
		xv_set(item, MENU_SELECTED, FALSE, NULL);
		return;
	}
	if (!termsw_folio->textedit) {
		termsw_folio->textedit_panel = xv_create(frame, PANEL,
				WIN_BELOW, TERMSW_PUBLIC(termsw_folio),
				PANEL_LAYOUT, PANEL_HORIZONTAL,
				XV_SHOW, FALSE,
				XV_WIDTH, (int)xv_get(frame, XV_WIDTH),
				NULL);

		termsw_folio->textedit = xv_create(frame, TEXTSW,
				XV_FONT, my_font,
				WIN_BELOW, termsw_folio->textedit_panel,
				XV_SHOW, FALSE,
				NULL);

		create_textedit_panel_item(termsw_folio->textedit_panel,
				termsw_folio->textedit);
	}
	if ((int)xv_get(termsw_folio->textedit, XV_SHOW)) {
		tty_notice = xv_get(frame, XV_KEY_DATA, tty_notice_key, NULL);
		if (!tty_notice) {
			tty_notice = xv_create(frame, NOTICE,
					NOTICE_BUTTON_YES, XV_MSG("Continue"),
					NOTICE_MESSAGE_STRINGS,
						XV_MSG("Textedit is already created.\n\
Press \"Continue\" to proceed."),
						NULL,
					NOTICE_BUSY_FRAMES, frame, NULL,
					XV_SHOW, TRUE,
					NULL);

			xv_set(frame, XV_KEY_DATA, tty_notice_key, tty_notice, NULL);
		}
		else {
			xv_set(tty_notice,
					NOTICE_BUTTON_YES, XV_MSG("Continue"),
					NOTICE_MESSAGE_STRINGS,
						XV_MSG("Textedit is already created.\n\
Press \"Continue\" to proceed."),
						NULL,
					NOTICE_BUSY_FRAMES, frame, NULL,
					XV_SHOW, TRUE,
					NULL);
		}
		return;
	}
	fit_termsw_panel_and_textsw(frame, termsw_folio);
}

static void ttysw_disable_editor(Menu do_not_use_cmd_menu, Menu_item item)
{
	Textsw textsw = xv_get(item, MENU_CLIENT_DATA);
	Frame frame = (Frame) xv_get(textsw, WIN_FRAME);
	register Termsw_folio termsw_folio =
			TERMSW_FOLIO_FOR_VIEW(TERMSW_VIEW_PRIVATE_FROM_TEXTSW(textsw));
	Event ie;
	int x, y;
	Rect rect;
	Xv_Notice tty_notice;

	if ((!termsw_folio->textedit) ||
			(!(int)xv_get(termsw_folio->textedit, XV_SHOW))) {
		tty_notice = xv_get(frame, XV_KEY_DATA, tty_notice_key, NULL);
		if (!tty_notice) {
			tty_notice = xv_create(frame, NOTICE,
					NOTICE_BUTTON_YES, XV_MSG("Continue"),
					NOTICE_MESSAGE_STRINGS,
						XV_MSG("No textedit is enabled yet.\n\
Press \"Continue\" to proceed."),
						NULL,
					NOTICE_BUSY_FRAMES, frame, NULL,
					XV_SHOW, TRUE,
					NULL);

			xv_set(frame, XV_KEY_DATA, tty_notice_key, tty_notice, NULL);
		}
		else {
			xv_set(tty_notice,
					NOTICE_BUTTON_YES, XV_MSG("Continue"),
					NOTICE_MESSAGE_STRINGS,
						XV_MSG("No textedit is enabled yet.\n\
Press \"Continue\" to proceed."),
						NULL,
					NOTICE_BUSY_FRAMES, frame, NULL,
					XV_SHOW, TRUE,
					NULL);
		}
		return;
	}
	win_getmouseposition(termsw_folio->textedit, &x, &y);
	ie.ie_locx = x;
	ie.ie_locy = y;
	if (textsw_empty_document(termsw_folio->textedit, &ie) == XV_ERROR)
		return;

	/* Change default to "Enable editor" */

	rect = *((Rect *) xv_get(termsw_folio->textedit, WIN_RECT));


	xv_set(termsw_folio->textedit, XV_SHOW, FALSE, NULL);
	xv_set(termsw_folio->textedit_panel, XV_SHOW, FALSE, NULL);

	xv_set(TERMSW_PUBLIC(termsw_folio),
			XV_HEIGHT, rect.r_top + rect.r_height - 1,
			XV_WIDTH, rect.r_width,
			NULL);
	window_fit(frame);
}

static void note_file_editor(Menu m, Menu_item item)
{
	if (xv_get(item, MENU_SELECTED)) {
		ttysw_enable_editor(m, item);
	}
	else {
		ttysw_disable_editor(m, item);
	}
}

static void wrap_textsw_file_do_menu_action(Menu menu, Menu_item item)
{
	int save_default;

	/* in textsw_file_do_menu_action with value TEXTSW_MENU_RESET
	 * somebody thought it would be a good idea to set
	 *         xv_set(cmd_menu, MENU_DEFAULT, 1, NULL);
	 * Dear people, OpenLook specifies that the user can modify
	 * a menu's default item....
	 */
	save_default = (int)xv_get(menu, MENU_DEFAULT);
	textsw_file_do_menu_action(menu, item);
	xv_set(menu, MENU_DEFAULT, save_default, NULL);
}

/* termsw walking menu definitions */
Pkg_private void ttysw_set_menu(Termsw termsw_public)
{
	Menu history_menu, mode_menu;
	Menu_item edit_item, find_item, extras_item, history_item, mode_item,
			scroll_item, editor_item;
	Menu_item editable_item, readonly_item, store_item, clear_item;
	Xv_Server server;
	Termsw_folio termsw_folio = TERMSW_PRIVATE(termsw_public);
	Textsw textsw = termsw_public;


	server = XV_SERVER_FROM_WINDOW(termsw_public);
	termsw_folio->text_menu = xv_create(server, MENU_MIXED_MENU,
			XV_HELP_DATA, "ttysw:mterms",
			NULL);

	/* History sub menu */
	history_menu = xv_create(server, MENU,
			MENU_CLIENT_DATA, textsw,
			XV_HELP_DATA, "ttysw:mhistory",
			NULL);
	mode_menu = xv_create(server, MENU_CHOICE_MENU,
			XV_HELP_DATA, "ttysw:mmode",
			NULL);

	editable_item = xv_create(XV_NULL, MENUITEM,
			MENU_RELEASE,
			MENU_STRING, XV_MSG("Editable"),
			MENU_VALUE, EDITABLE,
			MENU_ACTION, ttysw_mode_action,
			MENU_CLIENT_DATA, textsw,
			XV_HELP_DATA, "ttysw:mmode",
			NULL);
	readonly_item = xv_create(XV_NULL, MENUITEM,
			MENU_RELEASE,
			MENU_STRING, XV_MSG("Read Only"),
			MENU_VALUE, READ_ONLY,
			MENU_ACTION, ttysw_mode_action,
			MENU_CLIENT_DATA, textsw,
			XV_HELP_DATA, "ttysw:mmode",
			NULL);

	xv_set(mode_menu,
			MENU_APPEND_ITEM, editable_item,
			MENU_APPEND_ITEM, readonly_item,
			MENU_DEFAULT_ITEM, readonly_item,
			NULL);

	mode_item = (Menu_item) xv_create(XV_NULL, MENUITEM,
			MENU_RELEASE,
			MENU_STRING, XV_MSG("Mode"),
			MENU_PULLRIGHT, mode_menu,
			XV_HELP_DATA, "ttysw:mmode",
			NULL);

	store_item = xv_create(XV_NULL, MENUITEM,
			MENU_RELEASE,
			MENU_STRING, XV_MSG("Store log as new file..."),
			MENU_ACTION, textsw_file_do_menu_action,
			MENU_VALUE, TEXTSW_MENU_STORE,
			MENU_CLIENT_DATA, textsw,
			XV_HELP_DATA, "textsw:mstorelog",
			NULL);
	clear_item = xv_create(XV_NULL, MENUITEM,
			MENU_RELEASE,
			MENU_STRING, XV_MSG("Clear log"),
			MENU_ACTION, wrap_textsw_file_do_menu_action,
			MENU_VALUE, TEXTSW_MENU_RESET,
			MENU_CLIENT_DATA, textsw,
			XV_HELP_DATA, "textsw:mclearlog",
			NULL);

	xv_set(history_menu,
			MENU_APPEND_ITEM, mode_item,
			MENU_APPEND_ITEM, store_item, MENU_APPEND_ITEM, clear_item, NULL);

	history_item = (Menu_item) xv_create(XV_NULL, MENUITEM,
			MENU_RELEASE,
			MENU_STRING, XV_MSG("History"),
			MENU_PULLRIGHT, history_menu,
			XV_HELP_DATA, "ttysw:mhistory",
			NULL);

	edit_item = (Menu) xv_create(XV_NULL, MENUITEM,
			MENU_RELEASE,
			MENU_STRING, XV_MSG("Edit"),
			MENU_PULLRIGHT, xv_get(termsw_public, TEXTSW_SUBMENU_EDIT),
			XV_HELP_DATA, "ttysw:medit",
			NULL);

	find_item = (Menu) xv_create(XV_NULL, MENUITEM,
			MENU_RELEASE,
			MENU_STRING, XV_MSG("Find"),
			MENU_PULLRIGHT, xv_get(termsw_public, TEXTSW_SUBMENU_FIND),
			XV_HELP_DATA, "ttysw:mfind",
			NULL);

	extras_item = (Menu) xv_create(XV_NULL, MENUITEM,
			MENU_RELEASE,
			MENU_STRING, XV_MSG("Extras"),
			MENU_PULLRIGHT, xv_get(termsw_public, TEXTSW_EXTRAS_CMD_MENU),
			XV_HELP_DATA, "ttysw:mcommands",
			NULL);

	editor_item = xv_create(XV_NULL, MENUITEM,
			MENU_RELEASE,
			MENU_CLASS, MENU_TOGGLE,
			MENU_STRING, XV_MSG("File Editor"),
			MENU_NOTIFY_PROC, note_file_editor,
			MENU_CLIENT_DATA, textsw,
			XV_HELP_DATA, "ttysw:meditor",
			NULL);

	scroll_item = xv_create(XV_NULL, MENUITEM,
			MENU_RELEASE,
			MENU_STRING, XV_MSG("Disable Scrolling"),
			MENU_VALUE, DISABLE_SCROLL,
			MENU_ACTION, ttysw_disable_scrolling,
			MENU_CLIENT_DATA, textsw,
			XV_HELP_DATA, "textsw:mdisscroll",
			NULL);

	xv_set(termsw_folio->text_menu,
			MENU_TITLE_ITEM, XV_MSG("Term Pane"),
			MENU_APPEND_ITEM, history_item,
			MENU_APPEND_ITEM, edit_item,
			MENU_APPEND_ITEM, find_item,
			MENU_APPEND_ITEM, extras_item,
			MENU_APPEND_ITEM, editor_item,
			MENU_APPEND_ITEM, scroll_item,
			NULL);
}

static void ttysw_mode_action(Menu cmd_menu, Menu_item cmd_item)
{
	Textsw textsw = (Textsw) (xv_get(cmd_item, MENU_CLIENT_DATA));

	register Termsw_folio termsw =
			TERMSW_FOLIO_FOR_VIEW(TERMSW_VIEW_PRIVATE_FROM_TEXTSW(textsw));
	int value = (int)(xv_get(cmd_item, MENU_VALUE, 0));
	Textsw_index tmp_index, insert;

	if ((value == READ_ONLY) && !termsw->append_only_log) {
		tmp_index = (int)textsw_find_mark_i18n(textsw, termsw->pty_mark);
		insert = (Textsw_index) xv_get(textsw, TEXTSW_INSERTION_POINT_I18N);
		if (insert != tmp_index) {
			(void)xv_set(textsw, TEXTSW_INSERTION_POINT_I18N, tmp_index, NULL);
		}
		termsw->read_only_mark =
				textsw_add_mark_i18n(textsw,
				termsw->cooked_echo ? tmp_index : TEXTSW_INFINITY - 1,
				TEXTSW_MARK_READ_ONLY);
		termsw->append_only_log = TRUE;
	}
	else if ((value == EDITABLE) && termsw->append_only_log) {
		textsw_remove_mark(textsw, termsw->read_only_mark);
		termsw->append_only_log = FALSE;
	}
}

static void ttysw_enable_scrolling(Menu menu, Menu_item mi)
/*
 * This routine should only be invoked from the item added to the ttysw menu
 * when sw is created as a termsw.  It relies on the menu argument being the
 * handle for the ttysw menu.
 */
{
	/* The textsw handle is really a termsw handle */
	Textsw textsw = (Textsw) (xv_get(mi, MENU_CLIENT_DATA));

	/*Textsw        textsw = (Textsw) (menu_get(mi, MENU_CLIENT_DATA)); */
	Termsw_folio termsw_folio = TERMSW_PRIVATE((Termsw) textsw);
	Xv_Notice tty_notice;
	Ttysw_private ttysw_folio = TTY_PRIVATE_FROM_ANY_PUBLIC(textsw);

	if (termsw_folio->ok_to_enable_scroll) {

#ifdef OW_I18N
		ttysw_implicit_commit(ttysw_folio, 0);
#endif

		ttysw_setopt(ttysw_folio, TTYOPT_TEXT, 1);
	}
	else {
		Frame frame = xv_get(textsw, WIN_FRAME);

		tty_notice = xv_get(frame, XV_KEY_DATA, tty_notice_key, NULL);

		if (!tty_notice) {
			tty_notice = xv_create(frame, NOTICE,
					NOTICE_BUTTON_YES, XV_MSG("Continue"),
					NOTICE_MESSAGE_STRINGS,
						XV_MSG("Cannot enable scrolling while this application is running."),
						NULL,
					NOTICE_BUSY_FRAMES, frame, NULL,
					XV_SHOW, TRUE,
					NULL);

			xv_set(frame, XV_KEY_DATA, tty_notice_key, tty_notice, NULL);

		}
		else {
			xv_set(tty_notice,
					NOTICE_BUTTON_YES, XV_MSG("Continue"),
					NOTICE_MESSAGE_STRINGS,
						XV_MSG("Cannot enable scrolling while this application is running."),
						NULL,
					NOTICE_BUSY_FRAMES, frame, NULL,
					XV_SHOW, TRUE,
					NULL);
		}
	}
}

static void ttysw_disable_scrolling(Menu cmd_menu, Menu_item cmd_item)
{
	/* The textsw handle is really a termsw handle */
	Textsw textsw = xv_get(cmd_item, MENU_CLIENT_DATA);
	/* this is REALLY a Termsw_view! */
	Termsw_view vpub = xv_get(cmd_menu, XV_KEY_DATA, TEXTSW_MENU_DATA_KEY);
	/* so, this should really be a Termsw: */
	Termsw term = xv_get(vpub, XV_OWNER);
	Ttysw_private ttysw_folio = TTY_PRIVATE_TERMSW(term);
	Xv_Notice tty_notice;

	if (ttysw_getopt(ttysw_folio, TTYOPT_TEXT)) {
#ifdef OW_I18N
		textsw_implicit_commit(TEXTSW_PRIVATE(textsw));
#endif
		ttysw_setopt(ttysw_folio, TTYOPT_TEXT, 0);
	}
	else {
		Frame frame = xv_get(textsw, WIN_FRAME);

		tty_notice = xv_get(frame, XV_KEY_DATA, tty_notice_key, NULL);

		if (!tty_notice) {
			tty_notice = xv_create(frame, NOTICE,
					NOTICE_BUTTON_YES, XV_MSG("Continue"),
					NOTICE_MESSAGE_STRINGS,
						XV_MSG("Only one termsw view can turn into a ttysw at a time."),
						NULL,
					NOTICE_BUSY_FRAMES, frame, NULL,
					XV_SHOW, TRUE,
					NULL);

			xv_set(frame, XV_KEY_DATA, tty_notice_key, tty_notice, NULL);
		}
		else {
			xv_set(tty_notice,
					NOTICE_BUTTON_YES, XV_MSG("Continue"),
					NOTICE_MESSAGE_STRINGS,
						XV_MSG("Only one termsw view can turn into a ttysw at a time."),
						NULL,
					NOTICE_BUSY_FRAMES, frame, NULL,
					XV_SHOW, TRUE,
					NULL);
		}
	}
	xv_set(cmd_menu, MENU_DEFAULT, 1, NULL);
}
