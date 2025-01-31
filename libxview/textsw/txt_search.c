#ifndef lint
char     txt_search_c_sccsid[] = "@(#)txt_search.c 20.45 93/06/28 DRA: $Id: txt_search.c,v 4.10 2024/11/09 21:34:13 dra Exp $";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Text search popup frame creation and support.
 */


#include <xview_private/primal.h>
#include <xview_private/txt_impl.h>
#include <xview_private/ev_impl.h>
#include <sys/time.h>
#include <signal.h>
#include <xview/frame.h>
#include <xview/panel.h>
#include <xview/textsw.h>
#include <xview/openmenu.h>
#include <xview/wmgr.h>
#include <xview/pixwin.h>
#include <xview/win_struct.h>
#include <xview/win_screen.h>
#include <xview_private/svr_impl.h>

#define		MAX_DISPLAY_LENGTH	50
#define   	MAX_STR_LENGTH		1024

#define       DONT_RING_BELL               0x00000000
#define       RING_IF_NOT_FOUND            0x00000001
#define       RING_IF_ONLY_ONE             0x00000002

#define DIRECTION_VIEW	1	/* unique key for textsw view handle */

static int key_find =  0, key_replace, key_wrap, key_inact_when_ro;

static Es_index textsw_do_search_proc(Textsw_view_private view, unsigned direction, unsigned ring_bell_status, int is_global, Frame fram)
{
	Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
	Es_index first, last_plus_one;
	CHAR buf[MAX_STR_LENGTH];
	unsigned str_len;
	Es_index start_pos;
    int wrapping_off = (int)xv_get(xv_get(fram, XV_KEY_DATA, key_wrap),
											PANEL_VALUE);

	if (!textsw_get_selection(view, &first, &last_plus_one, NULL, 0))
		first = last_plus_one = EV_GET_INSERT(priv->views);

	if (direction == EV_FIND_DEFAULT)
		first = last_plus_one;

	STRNCPY(buf, (CHAR *)xv_get(xv_get(fram, XV_KEY_DATA, key_find),
					PANEL_VALUE), (size_t)MAX_STR_LENGTH - 1);

	str_len = STRLEN(buf);
	start_pos = (direction & EV_FIND_BACKWARD)
			? first : (first - str_len);

	textsw_find_pattern(priv, &first, &last_plus_one, buf, str_len, direction);

	if (wrapping_off) {
		if (direction == EV_FIND_DEFAULT)
			first = (start_pos > last_plus_one) ? ES_CANNOT_SET : first;
		else
			first = (start_pos < last_plus_one) ? ES_CANNOT_SET : first;
	}
	if ((first == ES_CANNOT_SET) || (last_plus_one == ES_CANNOT_SET)) {
		if (ring_bell_status & RING_IF_NOT_FOUND)
			(void)window_bell(XV_PUBLIC(view));
		return (ES_CANNOT_SET);
	}
	else {
		if ((ring_bell_status & RING_IF_ONLY_ONE) && (first == start_pos))
			(void)window_bell(XV_PUBLIC(view));
		if (!is_global)
			textsw_possibly_normalize_and_set_selection(VIEW_PUBLIC(view),
					first, last_plus_one, EV_SEL_PRIMARY);
		else

#ifdef OW_I18N
			textsw_set_selection_wcs(VIEW_PUBLIC(view), first,
					last_plus_one, EV_SEL_PRIMARY);
#else
			textsw_set_selection(TEXTSW_PUBLIC(priv), first, last_plus_one,
					EV_SEL_PRIMARY);
#endif

		(void)textsw_set_insert(priv, last_plus_one);
		textsw_record_find(priv, buf, (int)str_len, (int)direction);
		return ((direction == EV_FIND_DEFAULT) ? last_plus_one : first);
	}
}

static void note_find_forwards(Menu menu, Menu_item item)
{
    Textsw_view_private view = (Textsw_view_private) xv_get(menu, XV_KEY_DATA, DIRECTION_VIEW);
	Frame fram = xv_get(menu, XV_KEY_DATA, key_find);

    textsw_do_search_proc(view, EV_FIND_DEFAULT,
		      (RING_IF_NOT_FOUND | RING_IF_ONLY_ONE), FALSE, fram);
}

static void note_find_backwards(Menu menu, Menu_item item)
{
    Textsw_view_private view = (Textsw_view_private) xv_get(menu, XV_KEY_DATA, DIRECTION_VIEW);
	Frame fram = xv_get(menu, XV_KEY_DATA, key_find);

    (void) textsw_do_search_proc(view, EV_FIND_BACKWARD,
		      (RING_IF_NOT_FOUND | RING_IF_ONLY_ONE), FALSE, fram);
}

static int do_replace_proc(Textsw_view_private view, Frame fram)
{
	Textsw textsw = VIEW_PUBLIC(view);
	CHAR buf[MAX_STR_LENGTH];
	int selection_found;
	Es_index first, last_plus_one;

	if ((selection_found = textsw_get_selection(view, &first, &last_plus_one,
														NULL, 0)))
	{
		Panel_item rtext = xv_get(fram, XV_KEY_DATA, key_replace);
		STRNCPY(buf, (CHAR *) xv_get(rtext, PANEL_VALUE),
				(size_t)MAX_STR_LENGTH - 1);
		textsw_replace(textsw, first, last_plus_one, buf, (long)STRLEN(buf));
	}
	return selection_found;
}

static void do_replace_all_proc(Textsw_view_private view, int do_replace_first, unsigned direction, Frame fram)
{
    Textsw_private    priv = TSWPRIV_FOR_VIEWPRIV(view);
    int             start_checking = FALSE;	/* See if now is the time to
											 * check for wrap point */
    Es_index        cur_pos, prev_pos, cur_mark_pos;
    Ev_mark_object  mark = 0;
    int             exit_loop = FALSE;
    int             first_time = TRUE, process_aborted;
    int		    string_length_diff;

    if (do_replace_first)
	(void) do_replace_proc(view, fram);

    process_aborted = FALSE;

    cur_mark_pos = prev_pos = cur_pos = textsw_do_search_proc(view, direction, RING_IF_NOT_FOUND, TRUE, fram);

    exit_loop = (cur_pos == ES_CANNOT_SET);

#ifdef OW_I18N
    string_length_diff = STRLEN((CHAR *) xv_get(
	    search_panel_items[(int) REPLACE_STRING_ITEM], PANEL_VALUE_WCS, 
	    NULL)) - STRLEN((CHAR *) xv_get(
	    search_panel_items[(int) FIND_STRING_ITEM],
	    PANEL_VALUE_WCS, NULL));
#else
    string_length_diff = STRLEN((CHAR *) xv_get(
	    xv_get(fram, XV_KEY_DATA, key_replace), PANEL_VALUE))
		- STRLEN((CHAR *) xv_get(xv_get(fram, XV_KEY_DATA, key_find), PANEL_VALUE));
#endif

    while (!process_aborted && !exit_loop) {
	if (start_checking) {
	    cur_mark_pos = textsw_find_mark_internal(priv, mark);

	    exit_loop = (direction == EV_FIND_DEFAULT) ?
		(cur_mark_pos <= cur_pos) : (cur_mark_pos >= cur_pos);
	} else {
	    /* Did we wrap around the file already */
	    if (!first_time && (prev_pos == cur_pos))
		/* Only one instance of the pattern in the file. */
		start_checking = TRUE;
	    else 
		start_checking = (direction == EV_FIND_DEFAULT) ?
		    (prev_pos > cur_pos) : (cur_pos > prev_pos);
	    /*
	     * This is a special case Start search at the first instance of
	     * the pattern in the file.
	     */

	    if (start_checking) {
		cur_mark_pos = textsw_find_mark_internal(priv, mark);
		exit_loop = (direction == EV_FIND_DEFAULT) ?
		    (cur_mark_pos <= cur_pos) : (cur_mark_pos >= cur_pos);
	    }
	}

	if (!exit_loop) {
	    (void) do_replace_proc(view, fram);

	    if (first_time) {
		mark = textsw_add_mark_internal(priv, cur_mark_pos,
						TEXTSW_MARK_MOVE_AT_INSERT);
		first_time = FALSE;
	    }
	    prev_pos = cur_pos + string_length_diff;
	    cur_pos = textsw_do_search_proc(view, direction, DONT_RING_BELL, TRUE, fram);
	    exit_loop = (cur_pos == ES_CANNOT_SET);
	}
    }
 
    if (prev_pos != ES_CANNOT_SET)
#ifdef OW_I18N
    textsw_normalize_view_wc(VIEW_PUBLIC(view), prev_pos);
#else /* OW_I18N */
    textsw_normalize_view(VIEW_PUBLIC(view), prev_pos);

#endif /* OW_I18N */
    
    
    if (process_aborted)
	window_bell(VIEW_PUBLIC(view));
}

static void note_replace(Panel_item item)
{
	Textsw_view_private view = text_view_frm_p_itm(item);
	Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
	Frame fram = frame_from_panel_item(item);

	if (TXTSW_IS_READ_ONLY(priv) || !do_replace_proc(view, fram)) {
		(void)window_bell(VIEW_PUBLIC(view));
	}
}

static void note_find_then_replace(Panel_item item)
{
	Textsw_view_private view = text_view_frm_p_itm(item);
	Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
	Frame fram = frame_from_panel_item(item);

	if (!TXTSW_IS_READ_ONLY(priv)) {
		if (textsw_do_search_proc(view, EV_FIND_DEFAULT, RING_IF_NOT_FOUND,
						FALSE, fram) != ES_CANNOT_SET)
		{
			(void)do_replace_proc(view, fram);
		}
	}
}

static void note_replace_then_find(Panel_item item)
{
	Textsw_view_private view = text_view_frm_p_itm(item);
	Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
	Frame fram = frame_from_panel_item(item);

	if (!TXTSW_IS_READ_ONLY(priv)) {
		(void)do_replace_proc(view, fram);
		(void)textsw_do_search_proc(view,
				EV_FIND_DEFAULT, RING_IF_NOT_FOUND, FALSE, fram);
	}
}

static void note_replace_all(Panel_item item)
{
	Textsw_view_private view = text_view_frm_p_itm(item);
	Frame fram = frame_from_panel_item(item);

	do_replace_all_proc(view, FALSE, EV_FIND_DEFAULT, fram);
}

Pkg_private	void textsw_update_replace(Frame f, int ro)
{
    Panel panel = xv_get(f, FRAME_CMD_PANEL);
	Panel_item item;

	PANEL_EACH_ITEM(panel, item)
		if (xv_get(item, XV_KEY_DATA, key_inact_when_ro)) {
			xv_set(item, PANEL_INACTIVE, ro, NULL);
		}
	PANEL_END_EACH
}

static Panel create_search_items(Frame fram, Textsw_view_private view,int tf_key)
{
	static char *search = "Find";
	static char *replace = "Replace";
	static char *all = "Replace All";
	static char *search_replace = "Find then Replace";
	static char *replace_search = "Replace then Find";
	static char *backward = "Backward";
	static char *forward = "Forward";
	static int init_str = 0;
	CHAR search_string[MAX_STR_LENGTH];
	Es_index dummy;
	Panel panel;
	Menu menu;
	Panel_item ftext, rtext, wrap;
	char nambuf[100];
	Textsw tsw = xv_get(XV_PUBLIC(view), XV_OWNER);
	Xv_server srv = XV_SERVER_FROM_WINDOW(fram);
	int ro = (int)xv_get(tsw, TEXTSW_READ_ONLY);

	if (!init_str) {
		/*
		 * FIX_ME: The current gettext/dgettext return the uniq
		 * pointer for all messages, but future version and/or
		 * different implementation may behave differently.  If it is
		 * the case, you should wrap around following gettext by
		 * strdup call.
		 */
		search = XV_MSG("Find");
		replace = XV_MSG("Replace");
		all = XV_MSG("Replace All");
		search_replace = XV_MSG("Find then Replace");
		replace_search = XV_MSG("Replace then Find");
		backward = XV_MSG("Backward");
		forward = XV_MSG("Forward");
		init_str = 1;
	}

    panel = xv_get(fram, FRAME_CMD_PANEL);
	xv_set(panel,
				XV_HELP_DATA, textsw_make_help(tsw, "searchpanel"),
				XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
				NULL);

	search_string[0] = '\0';
	(void)textsw_get_selection(view, &dummy, &dummy, search_string,
						MAX_STR_LENGTH);

	ftext = xv_create(panel, PANEL_TEXT,
				PANEL_ITEM_LAYOUT_ROLE, PANEL_ROLE_LEADER,
				PANEL_VALUE_DISPLAY_LENGTH, MAX_DISPLAY_LENGTH,
				PANEL_VALUE_STORED_LENGTH, MAX_STR_LENGTH,
				PANEL_LABEL_STRING, XV_MSG("Search:"),
#ifdef OW_I18N
				PANEL_VALUE_WCS, search_string,
#else
				PANEL_VALUE, search_string,
#endif
				XV_HELP_DATA, textsw_make_help(tsw, "findstring"),
				XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
				NULL);

	wrap = xv_create(panel, PANEL_CYCLE,
				PANEL_ITEM_LAYOUT_ROLE, PANEL_ROLE_FOLLOWER,
				PANEL_CHOICE_STRINGS,
					XV_MSG("All Text"),
					XV_MSG("To End"),
					NULL,
				XV_HELP_DATA, textsw_make_help(tsw, "wrap"),
				XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
				NULL);

	rtext = xv_create(panel, PANEL_TEXT,
				PANEL_NEXT_ROW, -1,
				PANEL_ITEM_LAYOUT_ROLE, PANEL_ROLE_LEADER,
				PANEL_VALUE_DISPLAY_LENGTH, MAX_DISPLAY_LENGTH,
				PANEL_VALUE_STORED_LENGTH, MAX_STR_LENGTH,
				PANEL_LABEL_STRING, XV_MSG("Replace:"),
				XV_HELP_DATA, textsw_make_help(tsw, "replacestring"),
				XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
				XV_KEY_DATA, key_inact_when_ro, TRUE,
				PANEL_INACTIVE, ro,
				NULL);

	window_fit_width(panel);

	sprintf(nambuf, "%s.searchDirMenu", (char *)xv_get(fram, XV_INSTANCE_NAME));
	menu = xv_create(srv, MENU,
				XV_INSTANCE_NAME, nambuf,
				MENU_ITEM,
					MENU_STRING, forward,
					MENU_VALUE, 1,
					XV_HELP_DATA, textsw_make_help(tsw, "mdirforward"),
					XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
					MENU_NOTIFY_PROC, note_find_forwards,
					NULL,
				MENU_ITEM,
					MENU_STRING, backward,
					MENU_VALUE, 2,
					MENU_NOTIFY_PROC, note_find_backwards,
					XV_HELP_DATA, textsw_make_help(tsw, "mdirbackward"),
					XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
					NULL,
 				XV_KEY_DATA, DIRECTION_VIEW, view,
				XV_KEY_DATA, key_find, fram,
				XV_HELP_DATA, textsw_make_help(tsw, "mdirection"),
				XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
				XV_SET_MENU, fram,
				NULL);

	xv_set(panel, PANEL_DEFAULT_ITEM, xv_create(panel, PANEL_BUTTON,
					PANEL_NEXT_ROW, -1,
					PANEL_ITEM_LAYOUT_ROLE, PANEL_ROLE_CENTER,
					PANEL_LABEL_STRING, search,
					PANEL_ITEM_MENU, menu,
					XV_HELP_DATA, textsw_make_help(tsw, "find"),
					XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
					NULL),
				NULL);

	xv_create(panel, PANEL_BUTTON,
				PANEL_ITEM_LAYOUT_ROLE, PANEL_ROLE_CENTER,
				PANEL_LABEL_STRING, replace,
				PANEL_NOTIFY_PROC, note_replace,
				XV_HELP_DATA, textsw_make_help(tsw, "replace"),
				XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
				XV_KEY_DATA, key_inact_when_ro, TRUE,
				PANEL_INACTIVE, ro,
				NULL);

	xv_create(panel, PANEL_BUTTON,
				PANEL_ITEM_LAYOUT_ROLE, PANEL_ROLE_CENTER,
				PANEL_LABEL_STRING, search_replace,
				PANEL_NOTIFY_PROC, note_find_then_replace,
				XV_HELP_DATA, textsw_make_help(tsw, "findreplace"),
				XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
				XV_KEY_DATA, key_inact_when_ro, TRUE,
				PANEL_INACTIVE, ro,
				NULL);

	xv_create(panel, PANEL_BUTTON,
				PANEL_ITEM_LAYOUT_ROLE, PANEL_ROLE_CENTER,
				PANEL_LABEL_STRING, replace_search,
				PANEL_NOTIFY_PROC, note_replace_then_find,
				XV_HELP_DATA, textsw_make_help(tsw, "replacefind"),
				XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
				XV_KEY_DATA, key_inact_when_ro, TRUE,
				PANEL_INACTIVE, ro,
				NULL);

	xv_create(panel, PANEL_BUTTON,
				PANEL_ITEM_LAYOUT_ROLE, PANEL_ROLE_CENTER,
				PANEL_LABEL_STRING, all,
				PANEL_NOTIFY_PROC, note_replace_all,
				XV_HELP_DATA, textsw_make_help(tsw, "replaceall"),
				XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
				XV_KEY_DATA, key_inact_when_ro, TRUE,
				PANEL_INACTIVE, ro,
				NULL);

	xv_set(fram,
				XV_KEY_DATA, key_wrap, wrap,
				XV_KEY_DATA, key_find, ftext,
				XV_KEY_DATA, tf_key, ftext, /* for txt_popup.c */
				XV_KEY_DATA, key_replace, rtext,
				NULL);

	if (search_string[0] && ! ro) {
		xv_set(panel, PANEL_CARET_ITEM, rtext, NULL);
	}
	else {
		xv_set(panel, PANEL_CARET_ITEM, ftext, NULL);
	}

	xv_set(panel, PANEL_DO_LAYOUT, NULL);
	xv_set(fram,
			XV_HEIGHT, xv_get(panel, XV_HEIGHT),
			XV_WIDTH, xv_get(panel, XV_WIDTH),
			NULL);
/* 	frame_fit_all(fram); */

	return panel;
}

Pkg_private	Panel textsw_create_search_panel(Frame frame,
						Textsw_view_private view, int tf_key)
{
	if (! key_find) {
		key_find = xv_unique_key();
		key_replace = xv_unique_key();
		key_wrap = xv_unique_key();
		/* inactive when textsw is READ_ONLY */
		key_inact_when_ro = xv_unique_key();
	}

    return create_search_items(frame, view, tf_key);
}
