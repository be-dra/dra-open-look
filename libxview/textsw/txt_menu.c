#ifndef lint
char     txt_menu_c_sccsid[] = "@(#)txt_menu.c 20.90 93/06/28 DRA: $Id: txt_menu.c,v 4.34 2025/01/31 19:46:27 dra Exp $";
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
#include <xview/textsw.h>
#include <xview_private/txt_impl.h>
#include <xview_private/ev_impl.h>
#include <errno.h>
#include <assert.h>
#include <pixrect/pr_util.h>
#include <xview/file_chsr.h>

#ifdef __STDC__ 
#ifndef CAT
#define CAT(a,b)        a ## b 
#endif 
#endif
#include <pixrect/memvar.h>

#include <pixrect/pixfont.h>
#include <xview/defaults.h>
#include <xview/win_input.h>
#include <xview/win_struct.h>
#include <xview/fullscreen.h>
#include <xview/notice.h>
#include <xview/frame.h>
#include <xview/openmenu.h>
#include <xview/window.h>
#include <xview/cursor.h>
#include <xview/screen.h>
#include <xview/server.h>
#include <xview/svrimage.h>

#define			MAX_SIZE_MENU_STRING	30

typedef enum {
    TEXTSWMENU,
    TERMSWMENU
}               Menu_type;

typedef struct local_menu_object {
    int             refcount;	/* refcount for textsw_menu */
    Menu            menu;	/* Let default to NULL */
    Menu           *sub_menus;	/* Array of the sub menu handles */
    Menu_item      *menu_items /* [TEXTSW_MENU_LAST_CMD] */ ;
}               Local_menu_object;

int TXT_MENU_ITEMS_KEY, TXT_FILE_MENU_KEY, TXT_SET_DEF_KEY;

Xv_private void textsw_file_do_menu_action(Menu, Menu_item);

int             STORE_FILE_POPUP_KEY = 0;
int             SAVE_FILE_POPUP_KEY;
int             LOAD_FILE_POPUP_KEY;
int             FILE_STUFF_POPUP_KEY;
int             SEARCH_POPUP_KEY;
int             MATCH_POPUP_KEY;
int             SEL_LINE_POPUP_KEY;
int             EXTRASMENU_FILENAME_KEY;
int             TEXTSW_MENU_DATA_KEY;
int             TEXTSW_HANDLE_KEY;
int             TEXTSW_CURRENT_POPUP_KEY;
int             FC_PARENT_KEY;
int             FC_EXTEN_ITEM_KEY;

/* Menu strings for File sub menu */
#define	SAVE_FILE	"Save "
#define	STORE_NEW_FILE	"Save as..."
#define	LOAD_FILE	"Open..."
#define	INCLUDE_FILE	"Include..."
#define	EMPTY_DOC	"Empty Document"

/* Menu strings for Edit sub menu */
#define	AGAIN_STR	"Again"
#define	UNDO_STR	"Undo"
#define	COPY_STR	"Copy"
#define	PASTE_STR	"Paste"
#define	CUT_STR		"Cut"

/* Menu strings for View sub menu */
#define	SEL_LINE_AT_NUM		"Select Line at Number..."
#define	WHAT_LINE_NUM		"What Line Number?"
#define	SHOW_CARET_AT_TOP	"Show Caret at Top"
#define	CHANGE_LINE_WRAP	"Change Line Wrap"

/* Menu strings for Find sub menu */
#define	FIND_REPLACE		"Find and Replace..."
#define	FIND_SELECTION		"Find Selection"
#define	FIND_MARKED_TEXT	"Find Marked Text..."
#define	REPLACE_FIELD		"Replace |>field<| "

static Defaults_pairs line_break_pairs[] = {
    { "TEXTSW_CLIP", (int) TEXTSW_CLIP },
    { "TEXTSW_WRAP_AT_CHAR", (int) TEXTSW_WRAP_AT_CHAR },
    { "TEXTSW_WRAP_AT_WORD", (int) TEXTSW_WRAP_AT_WORD },
    { NULL, (int) TEXTSW_WRAP_AT_WORD }
};


Pkg_private Menu textsw_menu_get(Textsw_private priv)
{
	return priv->menu_accessible_only_via_textsw_menu;
}

Pkg_private void textsw_menu_set(Textsw_private priv, Menu menu)
{
	priv->menu_accessible_only_via_textsw_menu = menu;
}

static void textsw_edit_do_menu_action(Menu cmd_menu, Menu_item cmd_item)
{
	Textsw abstract;
	Textsw_view textsw_view = textsw_from_menu(cmd_menu);
	register Textsw_view_private view;
	register Textsw_private textsw;
	Textsw_menu_cmd cmd = (Textsw_menu_cmd)xv_get(cmd_item, MENU_VALUE);
	register Event *ie = (Event *)xv_get(cmd_menu, MENU_FIRST_EVENT);
	register int locx, locy;
	Xv_Notice text_notice;
	int menu_pinned = FALSE;
	Frame frame;
	Frame menu_cmd_frame = (Frame) xv_get(cmd_menu, MENU_PIN_WINDOW);

	if (textsw_view == 0) {
		if (event_action(ie) == ACTION_ACCELERATOR) {
			abstract = xv_get(cmd_menu, XV_KEY_DATA, TEXTSW_HANDLE_KEY);
			textsw = TEXTSW_PRIVATE(abstract);
			textsw_view = (Textsw_view) xv_get(abstract, OPENWIN_NTH_VIEW, 0);
			view = VIEW_ABS_TO_REP(textsw_view);
		}
		else {
			return;
		}
	}
	else {
		assert(xv_get(textsw_view, XV_IS_SUBTYPE_OF, OPENWIN_VIEW));
		view = VIEW_PRIVATE(textsw_view);
		abstract = xv_get(textsw_view, XV_OWNER);
		textsw = TEXTSW_PRIVATE(abstract);
	}

	if (menu_cmd_frame && (xv_get(menu_cmd_frame, XV_SHOW))) {
		menu_pinned = TRUE;
	}

	if (AN_ERROR(ie == 0)) {
		locx = locy = 0;
	}
	else {
		locx = ie->ie_locx;
		locy = ie->ie_locy;
	}

	switch (cmd) {

		case TEXTSW_MENU_AGAIN:
			textsw_again(view, locx, locy);
			break;

		case TEXTSW_MENU_UNDO:
			if (textsw_has_been_modified(abstract))
				textsw_undo(textsw);
			break;

		case TEXTSW_MENU_UNDO_ALL:
			if (textsw_has_been_modified(abstract)) {
				int result;

				frame = (Frame) xv_get(abstract, WIN_FRAME);
				text_notice = xv_get(frame, XV_KEY_DATA, text_notice_key);
				if (!text_notice) {
					text_notice = xv_create(frame, NOTICE, NULL);

					xv_set(frame,
							XV_KEY_DATA, text_notice_key, text_notice, NULL);
				}
				xv_set(text_notice,
					NOTICE_MESSAGE_STRINGS,
						XV_MSG("Undo All Edits will discard unsaved edits.\n\
Please confirm."),
						NULL,
					NOTICE_BUTTON_YES, XV_MSG("Confirm, discard edits"),
					NOTICE_BUTTON_NO, XV_MSG("Cancel"),
					/*
					   NOTICE_NO_BEEPING, 1,
					 */
					NOTICE_STATUS, &result,
					XV_SHOW, TRUE,
					NULL);

				if (result == NOTICE_YES)
					textsw_reset_2(abstract, locx, locy, TRUE, TRUE);
			}
			break;

		case TEXTSW_MENU_CUT:{
				Es_index first, last_plus_one;

				(void)ev_get_selection(textsw->views,
						&first, &last_plus_one, EV_SEL_PRIMARY);
				if (first < last_plus_one)	/* Local primary selection */
					textsw_new_sel_cut(view,
							(Event *)xv_get(cmd_menu,MENU_LAST_EVENT), FALSE);
				else {
					frame = (Frame) xv_get(VIEW_PUBLIC(view), WIN_FRAME);

					text_notice = xv_get(frame, XV_KEY_DATA, text_notice_key);
					if (!text_notice) {
						text_notice = xv_create(frame, NOTICE, NULL);

						xv_set(frame,
								XV_KEY_DATA, text_notice_key, text_notice,
								NULL);
					}
					xv_set(text_notice,
						NOTICE_MESSAGE_STRINGS,
							XV_MSG("Please make a primary selection in this textsw first.\n\
Press \"Continue\" to proceed."),
							NULL,
						NOTICE_BUTTON_YES, XV_MSG("Continue"),
						XV_SHOW, TRUE,
						NOTICE_BUSY_FRAMES,
							menu_pinned ? menu_cmd_frame : XV_NULL,
							NULL,
						NULL);
				}
				break;
			}
		case TEXTSW_MENU_COPY_APPEND:
			{
				Xv_window vpub = VIEW_PUBLIC(view);
				Es_index first, last_plus_one;

				(void)ev_get_selection(textsw->views,
						&first, &last_plus_one, EV_SEL_PRIMARY);
				if (first < last_plus_one) {	/* Local primary selection */
					textsw_new_sel_copy_appending(view,
							(Event *)xv_get(cmd_menu,MENU_LAST_EVENT));
				}
				else {
					frame = (Frame) xv_get(vpub, WIN_FRAME);
					text_notice = xv_get(frame, XV_KEY_DATA, text_notice_key);
					if (!text_notice) {
						text_notice = xv_create(frame, NOTICE, NULL);

						xv_set(frame,
								XV_KEY_DATA, text_notice_key, text_notice,
								NULL);
					}
					xv_set(text_notice,
						NOTICE_MESSAGE_STRINGS,
							XV_MSG("Please make a primary selection first.\n\
Press \"Continue\" to proceed."),
							NULL,
						NOTICE_BUTTON_YES, XV_MSG("Continue"),
						XV_SHOW, TRUE,
						NOTICE_BUSY_FRAMES,
							menu_pinned ? menu_cmd_frame : XV_NULL,
							NULL,
						NULL);
				}
				break;
			}
		case TEXTSW_MENU_COPY:
			{
				Es_index first, last_plus_one;

				(void)ev_get_selection(textsw->views,
						&first, &last_plus_one, EV_SEL_PRIMARY);
				if (first < last_plus_one) {	/* Local primary selection */
					textsw_new_sel_copy(view,
							(Event *)xv_get(cmd_menu,MENU_LAST_EVENT));
				}
				else {
					frame = (Frame) xv_get(VIEW_PUBLIC(view), WIN_FRAME);
					text_notice = xv_get(frame, XV_KEY_DATA, text_notice_key);
					if (!text_notice) {
						text_notice = xv_create(frame, NOTICE, NULL);

						xv_set(frame,
								XV_KEY_DATA, text_notice_key, text_notice,
								NULL);
					}
					xv_set(text_notice,
						NOTICE_MESSAGE_STRINGS,
							XV_MSG("Please make a primary selection first.\n\
Press \"Continue\" to proceed."),
							NULL,
						NOTICE_BUTTON_YES, XV_MSG("Continue"),
						XV_SHOW, TRUE,
						NOTICE_BUSY_FRAMES,
							menu_pinned ? menu_cmd_frame : XV_NULL,
							NULL,
						NULL);
				}
				break;
			}
		case TEXTSW_MENU_PASTE:
			textsw_new_sel_paste(view, 
						(Event *)xv_get(cmd_menu,MENU_LAST_EVENT), FALSE);
			break;
		default:
			break;
	}
}

static void textsw_view_do_menu_action(Menu cmd_menu, Menu_item cmd_item)
{
    Textsw          abstract;
    Textsw_view     textsw_view = textsw_from_menu(cmd_menu);
    register Textsw_view_private view;
    register Textsw_private textsw;
    Textsw_menu_cmd cmd = (Textsw_menu_cmd)xv_get(cmd_item, MENU_VALUE);
    Es_index        first, last_plus_one;
    Xv_Notice	    text_notice;
    Frame	    frame;

     if (AN_ERROR(textsw_view == 0)) return;

    view = VIEW_ABS_TO_REP(textsw_view);
    textsw = TSWPRIV_FOR_VIEWPRIV(view);
    abstract = TEXTSW_PUBLIC(textsw);

    switch (cmd) {

      case TEXTSW_MENU_CLIP_LINES:
	xv_set(TEXTSW_PUBLIC(TSWPRIV_FOR_VIEWPRIV(view)),
	       TEXTSW_LINE_BREAK_ACTION, TEXTSW_CLIP,
	       NULL);
	break;

      case TEXTSW_MENU_WRAP_LINES_AT_CHAR:
	xv_set(TEXTSW_PUBLIC(TSWPRIV_FOR_VIEWPRIV(view)),
	       TEXTSW_LINE_BREAK_ACTION, TEXTSW_WRAP_AT_CHAR,
	       NULL);
	break;

      case TEXTSW_MENU_WRAP_LINES_AT_WORD:
	xv_set(TEXTSW_PUBLIC(TSWPRIV_FOR_VIEWPRIV(view)),
	       TEXTSW_LINE_BREAK_ACTION, TEXTSW_WRAP_AT_WORD,
	       NULL);
	break;

      case TEXTSW_MENU_NORMALIZE_INSERTION:{
	    Es_index        insert;
	    int             upper_context;
	    insert = EV_GET_INSERT(textsw->views);
	    if (insert != ES_INFINITY) {
		upper_context = (int)
		    ev_get(view->e_view, EV_CHAIN_UPPER_CONTEXT, XV_NULL, XV_NULL, XV_NULL);
		textsw_normalize_internal(view, insert, insert, upper_context, 0,
					  TXTSW_NI_DEFAULT);
	    }
	    break;
	}

      case TEXTSW_MENU_NORMALIZE_LINE:{
	    Frame           base_frame = (Frame) xv_get(abstract, WIN_FRAME);
	    Frame           popup = (Frame) xv_get(base_frame, XV_KEY_DATA,
						   SEL_LINE_POPUP_KEY);
	    if (popup) {
		(void) textsw_get_and_set_selection(popup, view,
					  (int) TEXTSW_MENU_NORMALIZE_LINE);
	    } else {
		(void) textsw_create_popup_frame(view,
					  (int) TEXTSW_MENU_NORMALIZE_LINE);
	    }
	    break;
	}

      case TEXTSW_MENU_COUNT_TO_LINE:{
	    char            msg[200];
	    int             count;
	    if (!textsw_is_seln_nonzero(textsw, EV_SEL_PRIMARY)) {
                frame = (Frame)xv_get(VIEW_PUBLIC(view), WIN_FRAME);
                text_notice = xv_get(frame, XV_KEY_DATA, text_notice_key);
                if (!text_notice)  {
                    text_notice = xv_create(frame, NOTICE, NULL);

                    xv_set(frame, 
                        XV_KEY_DATA, text_notice_key, text_notice,
                        NULL);
                }
				xv_set(text_notice, 
					NOTICE_MESSAGE_STRINGS,
						XV_MSG("Please make a primary selection first.\n\
Press \"Continue\" to proceed."),
						NULL,
					NOTICE_BUTTON_YES, XV_MSG("Continue"),
					XV_SHOW, TRUE, 
					NULL);
			break;
	    }
	    ev_get_selection(textsw->views, &first, &last_plus_one, EV_SEL_PRIMARY);
	    if (first >= last_plus_one)
		break;
	    count = ev_newlines_in_esh(textsw->views->esh, 0, first);
	    (void) sprintf(msg, 
		XV_MSG("Selection starts in line %d."), 
		count + 1);

	    frame = (Frame)xv_get(abstract, WIN_FRAME);
            text_notice = xv_get(frame, XV_KEY_DATA, text_notice_key);
            if (!text_notice)  {
                text_notice = xv_create(frame, NOTICE, NULL);

                xv_set(frame, XV_KEY_DATA, text_notice_key, text_notice, NULL);
            }
			xv_set(text_notice, 
				NOTICE_MESSAGE_STRINGS,
					msg,
					XV_MSG("Press \"Continue\" to proceed."),
					NULL,
				NOTICE_BUTTON_YES, XV_MSG("Continue"),
				XV_SHOW, TRUE, 
				NULL);

	    break;
	}

      default:
	break;
    }
}

static void textsw_find_do_menu_action(Menu cmd_menu, Menu_item cmd_item)
{
	Textsw textsw;
	Textsw_view textsw_view = textsw_from_menu(cmd_menu);
	register Textsw_view_private view;
	register Textsw_private priv;
	Textsw_menu_cmd cmd = (Textsw_menu_cmd) xv_get(cmd_item, MENU_VALUE);
	Event *ie = (Event *) xv_get(cmd_menu, MENU_FIRST_EVENT);
	register int locx, locy;
	register long unsigned find_options = 0L;

#ifdef OW_I18N
	static CHAR bar_lt[] = { '<', '|', 0 };
	static CHAR bar_gt[] = { '|', '>', 0 };
#endif

	if (AN_ERROR(textsw_view == 0)) {
		if (event_action(ie) == ACTION_ACCELERATOR) {
			textsw = xv_get(cmd_menu, XV_KEY_DATA, TEXTSW_HANDLE_KEY);
			priv = TEXTSW_PRIVATE(textsw);
			textsw_view = (Textsw_view) xv_get(textsw, OPENWIN_NTH_VIEW, 0);
			view = VIEW_ABS_TO_REP(textsw_view);
		}
		else {
			return;
		}
	}
	else {

		assert(xv_get(textsw_view, XV_IS_SUBTYPE_OF, OPENWIN_VIEW));
		view = VIEW_PRIVATE(textsw_view);
		textsw = xv_get(textsw_view, XV_OWNER);
		priv = TEXTSW_PRIVATE(textsw);
	}

	if (AN_ERROR(ie == 0)) {
		locx = locy = 0;
	}
	else {
		locx = ie->ie_locx;
		locy = ie->ie_locy;
	}

	switch (cmd) {
		case TEXTSW_MENU_FIND_BACKWARD:
			find_options = TFSAN_BACKWARD;
			/* Fall through */
		case TEXTSW_MENU_FIND:
			find_options |= (EV_SEL_PRIMARY | TFSAN_CLIPBOARD_ALSO);
			if (textsw_is_seln_nonzero(priv, (unsigned)find_options))
				textsw_find_selection_and_normalize(view, locx, locy,
						find_options);
			else
				window_bell(XV_PUBLIC(view));
			break;

		case TEXTSW_MENU_SEL_ENCLOSE_FIELD:{
				Es_index first, last_plus_one;

				first = last_plus_one = EV_GET_INSERT(priv->views);
				(void)textsw_match_field_and_normalize(view, &first,
						&last_plus_one,

#ifdef OW_I18N
						bar_lt, 2, TEXTSW_FIELD_ENCLOSE, FALSE);
#else
						"<|", 2, TEXTSW_FIELD_ENCLOSE, FALSE);
#endif

				break;
			}
		case TEXTSW_MENU_SEL_NEXT_FIELD:
			textsw_match_selection_and_normalize(view,

#ifdef OW_I18N
					bar_gt, TEXTSW_FIELD_FORWARD);
#else
					"|>", TEXTSW_FIELD_FORWARD);
#endif

			break;

		case TEXTSW_MENU_SEL_PREV_FIELD:
			textsw_match_selection_and_normalize(view,

#ifdef OW_I18N
					bar_lt, TEXTSW_FIELD_BACKWARD);
#else
					"<|", TEXTSW_FIELD_BACKWARD);
#endif

			break;

		case TEXTSW_MENU_SEL_MARK_TEXT:{
				Frame base_frame = (Frame) xv_get(textsw, WIN_FRAME);
				Frame popup = (Frame) xv_get(base_frame, XV_KEY_DATA,
						MATCH_POPUP_KEY);

				if (popup) {
					xv_set(popup, XV_SHOW, TRUE, WIN_CLIENT_DATA, view, NULL);
				}
				else {
					textsw_create_popup_frame(view,
							(int)TEXTSW_MENU_SEL_MARK_TEXT);
				}
				break;
			}

		case TEXTSW_MENU_FIND_AND_REPLACE:{
				Frame base_frame = (Frame) xv_get(textsw, WIN_FRAME);
				Frame popup = (Frame) xv_get(base_frame, XV_KEY_DATA,
						SEARCH_POPUP_KEY);

				if (popup) {
					textsw_get_and_set_selection(popup, view,
							(int)TEXTSW_MENU_FIND_AND_REPLACE);
				}
				else {
					textsw_create_popup_frame(view,
							(int)TEXTSW_MENU_FIND_AND_REPLACE);
				}
				break;
			}

		default:
			break;
	}
}

static void textsw_done_menu(Menu menu, Xv_opaque result)
{
    Textsw_view textsw_view = xv_get(menu, XV_KEY_DATA, TEXTSW_MENU_DATA_KEY);
	Textsw t = xv_get(textsw_view, XV_OWNER);
	Textsw_private priv = TEXTSW_PRIVATE(t);

    textsw_thaw_caret(priv);
    textsw_stablize(priv, 1);
}

static Menu gen_edit_menu(Menu menu, Menu_generate op)
{
	Textsw tsw;
	Textsw_private priv;
	Menu_item *mi, item;
	int sellen;
	char buf[10];

	if (op != MENU_DISPLAY) return menu;

	tsw = xv_get(menu, XV_KEY_DATA, TEXTSW_HANDLE_KEY);
	if (! tsw) return menu;

	priv = TEXTSW_PRIVATE(tsw);
	mi = priv->menu_table;

	sellen = textsw_get_primary_selection(tsw, buf, (unsigned)sizeof(buf),
											NULL, NULL);

	item = mi[TEXTSW_MENU_COPY];
	if (item) xv_set(item, MENU_INACTIVE, sellen == 0, NULL);
	item = mi[TEXTSW_MENU_CUT];
	if (item) xv_set(item, MENU_INACTIVE, sellen == 0, NULL);
	item = mi[TEXTSW_MENU_COPY_APPEND];
	if (item) xv_set(item, MENU_INACTIVE, sellen == 0, NULL);

	return menu;
}

static Menu textsw_new_restricted_menu(Textsw_private priv, Textsw textsw,
								Menu_item *menu_items)
{
	/* we stick to the submenu structure, but create only **those** 
	 * menu items that are needed in PANEL_MULTILINE_TEXTS
	 */
	char menuinst[100];
	char *txtinst;
	Xv_server server = XV_SERVER_FROM_WINDOW(textsw);
	Menu top_menu;

	txtinst = (char *)xv_get(textsw, XV_INSTANCE_NAME);
	if (! txtinst || ! *txtinst) {
		txtinst = "textsw";
	}

	menu_items[(int)TEXTSW_MENU_UNDO] = xv_create(XV_NULL, MENUITEM,
			MENU_RELEASE,
			MENU_STRING, XV_MSG(UNDO_STR),
			MENU_VALUE, (long)TEXTSW_MENU_UNDO,
			MENU_NOTIFY_PROC, textsw_edit_do_menu_action,
			XV_HELP_DATA, textsw_make_help(textsw, "mundolast"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);

	/*
	 * Set accelerator for menu item
	 */
	xv_set(menu_items[(int)TEXTSW_MENU_UNDO],
			MENU_ACCELERATOR, "coreset Undo",
			NULL);

	menu_items[(int)TEXTSW_MENU_COPY] = xv_create(XV_NULL, MENUITEM,
			MENU_RELEASE,
			MENU_STRING, XV_MSG(COPY_STR),
			MENU_VALUE, (long)TEXTSW_MENU_COPY,
			MENU_NOTIFY_PROC, textsw_edit_do_menu_action,
			XV_HELP_DATA, textsw_make_help(textsw, "meditcopy"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);
	menu_items[(int)TEXTSW_MENU_PASTE] = xv_create(XV_NULL, MENUITEM,
			MENU_RELEASE,
			MENU_STRING, XV_MSG(PASTE_STR),
			MENU_VALUE, (long)TEXTSW_MENU_PASTE,
			MENU_NOTIFY_PROC, textsw_edit_do_menu_action,
			XV_HELP_DATA, textsw_make_help(textsw, "meditpaste"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);
	menu_items[(int)TEXTSW_MENU_CUT] = xv_create(XV_NULL, MENUITEM,
			MENU_RELEASE,
			MENU_STRING, XV_MSG(CUT_STR),
			MENU_VALUE, (long)TEXTSW_MENU_CUT,
			MENU_NOTIFY_PROC, textsw_edit_do_menu_action,
			XV_HELP_DATA, textsw_make_help(textsw, "meditcut"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);

	/*
	 * Set accelerator for menu items
	 */
	xv_set(menu_items[(int)TEXTSW_MENU_COPY],
			MENU_ACCELERATOR, "coreset Copy",
			NULL);
	xv_set(menu_items[(int)TEXTSW_MENU_PASTE],
			MENU_ACCELERATOR, "coreset Paste",
			NULL);
	xv_set(menu_items[(int)TEXTSW_MENU_CUT],
			MENU_ACCELERATOR, "coreset Cut",
			NULL);

	sprintf(menuinst, "%s_topmenu", txtinst);
	top_menu = xv_create(server, MENU,
			XV_INSTANCE_NAME, menuinst,
			MENU_TITLE_ITEM, XV_MSG("Edit"),
			XV_HELP_DATA, textsw_make_help(textsw, "mtopmenu"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);

	xv_set(top_menu,
			MENU_APPEND_ITEM, menu_items[(int)TEXTSW_MENU_UNDO],
			MENU_APPEND_ITEM, menu_items[(int)TEXTSW_MENU_CUT],
			MENU_APPEND_ITEM, menu_items[(int)TEXTSW_MENU_COPY],
			MENU_APPEND_ITEM, menu_items[(int)TEXTSW_MENU_PASTE],
			XV_KEY_DATA, TEXTSW_MENU_DATA_KEY,
						xv_get(textsw, OPENWIN_NTH_VIEW, 0),
			NULL);

	return top_menu;
}

/* if a bad application destroys the file submenu, we don't want to fiddle
 * around with the default item...
 */
static void file_sub_destroyed(Xv_opaque obj, int key, char *data)
{	
	Textsw_private priv = (Textsw_private)data;
	priv->sub_menu_table[TXTSW_FILE_SUB_MENU] = XV_NULL;
}

static Menu textsw_new_full_menu(Textsw_private priv, Textsw textsw,
								Menu_item *menu_items, Menu *sub_menu)
{
	char *txtinst;
	char menuinst[100];
	Xv_server server = XV_SERVER_FROM_WINDOW(textsw);
	Menu undo_cmds, break_mode, find_sel_cmds, select_field_cmds, top_menu;
	Menu_item break_mode_item, undo_cmds_item, find_sel_cmds_item,
			select_field_cmds_item;
	Menu_item copy_cmds_item;
	char *def_str;
	int line_break;
	char *filename;
	int index;
	Frame frame;

	txtinst = (char *)xv_get(textsw, XV_INSTANCE_NAME);
	if (! txtinst || ! *txtinst) {
		txtinst = "textsw";
	}

	sprintf(menuinst, "%s_breakmode", txtinst);
	break_mode = xv_create(server, MENU_CHOICE_MENU,
			XV_INSTANCE_NAME, menuinst,
			XV_HELP_DATA, textsw_make_help(textsw, "mbreakmode"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);
	menu_items[(int)TEXTSW_MENU_WRAP_LINES_AT_WORD] = xv_create(XV_NULL,
			MENUITEM,
			MENU_RELEASE,
			MENU_STRING, XV_MSG("Wrap at Word"),
			MENU_VALUE, (long)TEXTSW_MENU_WRAP_LINES_AT_WORD,
			XV_HELP_DATA, textsw_make_help(textsw, "mwrapwords"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);
	menu_items[(int)TEXTSW_MENU_WRAP_LINES_AT_CHAR] = xv_create(XV_NULL,
			MENUITEM,
			MENU_RELEASE,
			MENU_STRING, XV_MSG("Wrap at Character"),
			MENU_VALUE, (long)TEXTSW_MENU_WRAP_LINES_AT_CHAR,
			XV_HELP_DATA, textsw_make_help(textsw, "mwrapchars"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);
	menu_items[(int)TEXTSW_MENU_CLIP_LINES] = xv_create(XV_NULL,
			MENUITEM,
			MENU_RELEASE,
			MENU_STRING, XV_MSG("Clip Lines"),
			MENU_VALUE, (long)TEXTSW_MENU_CLIP_LINES,
			XV_HELP_DATA, textsw_make_help(textsw, "mcliplines"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);
	def_str = defaults_get_string("text.lineBreak","Text.LineBreak",(char *)0);
	if (def_str == NULL || def_str[0] == '\0'
			|| (line_break =
					(int)defaults_lookup(def_str,
							line_break_pairs)) == TEXTSW_WRAP_AT_WORD)
		xv_set(break_mode,
			MENU_APPEND_ITEM, menu_items[(int)TEXTSW_MENU_WRAP_LINES_AT_WORD],
			MENU_APPEND_ITEM, menu_items[(int)TEXTSW_MENU_WRAP_LINES_AT_CHAR],
			MENU_APPEND_ITEM, menu_items[(int)TEXTSW_MENU_CLIP_LINES],
			NULL);
	else if (TEXTSW_WRAP_AT_CHAR == line_break)
		xv_set(break_mode,
			MENU_APPEND_ITEM, menu_items[(int)TEXTSW_MENU_WRAP_LINES_AT_CHAR],
			MENU_APPEND_ITEM, menu_items[(int)TEXTSW_MENU_WRAP_LINES_AT_WORD],
			MENU_APPEND_ITEM, menu_items[(int)TEXTSW_MENU_CLIP_LINES],
			NULL);
	else
		xv_set(break_mode,
			MENU_APPEND_ITEM, menu_items[(int)TEXTSW_MENU_CLIP_LINES],
			MENU_APPEND_ITEM, menu_items[(int)TEXTSW_MENU_WRAP_LINES_AT_CHAR],
			MENU_APPEND_ITEM, menu_items[(int)TEXTSW_MENU_WRAP_LINES_AT_WORD],
			NULL);

	sprintf(menuinst, "%s_undocmds", txtinst);
	undo_cmds = xv_create(server, MENU,
			XV_INSTANCE_NAME, menuinst,
			XV_HELP_DATA, textsw_make_help(textsw, "mundocmds"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);
	menu_items[(int)TEXTSW_MENU_UNDO] = xv_create(XV_NULL, MENUITEM,
			MENU_RELEASE,
			MENU_STRING, XV_MSG("Last Edit"),
			MENU_VALUE, (long)TEXTSW_MENU_UNDO,
			XV_HELP_DATA, textsw_make_help(textsw, "mundolast"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);

	/*
	 * Set accelerator for menu item
	 */
	xv_set(menu_items[(int)TEXTSW_MENU_UNDO],
			MENU_ACCELERATOR, "coreset Undo",
			NULL);

	menu_items[(int)TEXTSW_MENU_UNDO_ALL] = xv_create(XV_NULL,
			MENUITEM,
			MENU_RELEASE,
			MENU_STRING, XV_MSG("All Edits"),
			MENU_VALUE, (long)TEXTSW_MENU_UNDO_ALL,
			XV_HELP_DATA, textsw_make_help(textsw, "mundoall"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);
	xv_set(undo_cmds,
			MENU_APPEND_ITEM, menu_items[(int)TEXTSW_MENU_UNDO],
			MENU_APPEND_ITEM, menu_items[(int)TEXTSW_MENU_UNDO_ALL],
			NULL);
	xv_set(undo_cmds,
			XV_KEY_DATA, TEXTSW_MENU_DATA_KEY,
						xv_get(textsw, OPENWIN_NTH_VIEW, 0),
			NULL);

	sprintf(menuinst, "%s_selfieldcmds", txtinst);
	select_field_cmds = xv_create(server, MENU,
			XV_INSTANCE_NAME, menuinst,
			XV_HELP_DATA, textsw_make_help(textsw, "mselfieldcmds"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);
	menu_items[(int)TEXTSW_MENU_SEL_ENCLOSE_FIELD] = xv_create(XV_NULL,MENUITEM,
			MENU_RELEASE,
			MENU_STRING, XV_MSG("Expand"),
			MENU_VALUE, (long)TEXTSW_MENU_SEL_ENCLOSE_FIELD,
			XV_HELP_DATA, textsw_make_help(textsw, "mselexpand"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);
	menu_items[(int)TEXTSW_MENU_SEL_NEXT_FIELD] = xv_create(XV_NULL, MENUITEM,
			MENU_RELEASE,
			MENU_STRING, XV_MSG("Next"),
			MENU_VALUE, (long)TEXTSW_MENU_SEL_NEXT_FIELD,
			XV_HELP_DATA, textsw_make_help(textsw, "mselnext"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);
	menu_items[(int)TEXTSW_MENU_SEL_PREV_FIELD] = xv_create(XV_NULL, MENUITEM,
			MENU_RELEASE,
			MENU_STRING, XV_MSG("Previous"),
			MENU_VALUE, (long)TEXTSW_MENU_SEL_PREV_FIELD,
			XV_HELP_DATA, textsw_make_help(textsw, "mselprevious"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);
	xv_set(select_field_cmds,
			MENU_APPEND_ITEM, menu_items[(int)TEXTSW_MENU_SEL_ENCLOSE_FIELD],
			MENU_APPEND_ITEM, menu_items[(int)TEXTSW_MENU_SEL_NEXT_FIELD],
			MENU_APPEND_ITEM, menu_items[(int)TEXTSW_MENU_SEL_PREV_FIELD],
			NULL);

	sprintf(menuinst, "%s_findselcmds", txtinst);
	find_sel_cmds = xv_create(server, MENU,
			XV_INSTANCE_NAME, menuinst,
			XV_HELP_DATA, textsw_make_help(textsw, "mfindselcmds"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);
	menu_items[(int)TEXTSW_MENU_FIND] = xv_create(XV_NULL, MENUITEM,
			MENU_RELEASE,
			MENU_STRING, XV_MSG("Forward"),
			MENU_VALUE, (long)TEXTSW_MENU_FIND,
			XV_HELP_DATA, textsw_make_help(textsw, "mfindforward"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);

	/*
	 * Set accelerator for menu item
	 */
	xv_set(menu_items[(int)TEXTSW_MENU_FIND],
			MENU_ACCELERATOR, "coreset Find",
			NULL);

	menu_items[(int)TEXTSW_MENU_FIND_BACKWARD] = xv_create(XV_NULL, MENUITEM,
			MENU_RELEASE,
			MENU_STRING, XV_MSG("Backward"),
			MENU_VALUE, (long)TEXTSW_MENU_FIND_BACKWARD,
			XV_HELP_DATA, textsw_make_help(textsw, "mfindbackward"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);
	xv_set(find_sel_cmds,
			MENU_APPEND_ITEM, menu_items[(int)TEXTSW_MENU_FIND],
			MENU_APPEND_ITEM, menu_items[(int)TEXTSW_MENU_FIND_BACKWARD], NULL);

	menu_items[(int)TEXTSW_MENU_LOAD] = xv_create(XV_NULL, MENUITEM,
			MENU_RELEASE,
			MENU_STRING, XV_MSG(LOAD_FILE),
			MENU_VALUE, (long)TEXTSW_MENU_LOAD,
			MENU_INACTIVE, ((priv->state & TXTSW_NO_LOAD) != 0),
			XV_HELP_DATA, textsw_make_help(textsw, "mloadfile"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);

	/*
	 * Set accelerator for menu item
	 */
	xv_set(menu_items[(int)TEXTSW_MENU_LOAD],
			MENU_ACCELERATOR, "coreset Open",
			NULL);

	menu_items[(int)TEXTSW_MENU_SAVE] = xv_create(XV_NULL, MENUITEM,
			MENU_RELEASE,
			MENU_STRING, XV_MSG(SAVE_FILE),
			MENU_VALUE, (long)TEXTSW_MENU_SAVE,
			XV_HELP_DATA, textsw_make_help(textsw, "msavefile"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);

	menu_items[(int)TEXTSW_MENU_STORE] = xv_create(XV_NULL, MENUITEM,
			MENU_RELEASE,
			MENU_STRING, XV_MSG(STORE_NEW_FILE),
			MENU_VALUE, (long)TEXTSW_MENU_STORE,
			XV_HELP_DATA, textsw_make_help(textsw, "mstorefile"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);

	/*
	 * Set accelerator for menu item.
	 * Meta+S will accelerate "Save"
	 *
	 * Meta+S is an already existing SunView key binding for 
	 * "Save As".
	 * We override it here with a different action even though
	 * this may cause compatibility problems because this
	 * is required by the spec.
	 */
	xv_set(menu_items[(int)TEXTSW_MENU_SAVE],
			MENU_ACCELERATOR, "coreset Save",
			NULL);


	menu_items[(int)TEXTSW_MENU_FILE_STUFF] = xv_create(XV_NULL, MENUITEM,
			MENU_RELEASE,
			MENU_STRING, XV_MSG(INCLUDE_FILE),
			MENU_VALUE, (long)TEXTSW_MENU_FILE_STUFF,
			XV_HELP_DATA, textsw_make_help(textsw, "mincludefile"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);
	menu_items[(int)TEXTSW_MENU_RESET] = xv_create(XV_NULL, MENUITEM,
			MENU_RELEASE,
			MENU_STRING, XV_MSG(EMPTY_DOC),
			MENU_VALUE, (long)TEXTSW_MENU_RESET,
			XV_HELP_DATA, textsw_make_help(textsw, "memptydoc"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);

	sprintf(menuinst, "%s_filecmds", txtinst);
	sub_menu[(int)TXTSW_FILE_SUB_MENU] = xv_create(server, MENU,
			XV_INSTANCE_NAME, menuinst,
			XV_HELP_DATA, textsw_make_help(textsw, "mfilecmds"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			XV_KEY_DATA, FC_PARENT_KEY, priv,
			XV_KEY_DATA_REMOVE_PROC, FC_PARENT_KEY, file_sub_destroyed,
			NULL);

	xv_set(sub_menu[(int)TXTSW_FILE_SUB_MENU],
			MENU_APPEND_ITEM, menu_items[(int)TEXTSW_MENU_LOAD],
			MENU_APPEND_ITEM, menu_items[(int)TEXTSW_MENU_SAVE],
			MENU_APPEND_ITEM, menu_items[(int)TEXTSW_MENU_STORE],
			MENU_APPEND_ITEM, menu_items[(int)TEXTSW_MENU_FILE_STUFF],
			MENU_APPEND_ITEM, menu_items[(int)TEXTSW_MENU_RESET],
			NULL);

	menu_items[(int)TEXTSW_MENU_AGAIN] = xv_create(XV_NULL, MENUITEM,
			MENU_RELEASE,
			MENU_STRING, XV_MSG(AGAIN_STR),
			MENU_VALUE, (long)TEXTSW_MENU_AGAIN,
			XV_HELP_DATA, textsw_make_help(textsw, "meditagain"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);
	undo_cmds_item = xv_create(XV_NULL, MENUITEM,
			MENU_RELEASE,
			MENU_STRING, XV_MSG(UNDO_STR),
			MENU_PULLRIGHT, undo_cmds,
			XV_HELP_DATA, textsw_make_help(textsw, "meditundo"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);
	if (defaults_get_boolean("text.haveCopyAppend","Text.HaveCopyAppend",FALSE))
	{
		menu_items[TEXTSW_MENU_COPY] = xv_create(XV_NULL, MENUITEM,
				MENU_RELEASE,
				MENU_STRING, XV_MSG("Replace"),
				MENU_VALUE, (long)TEXTSW_MENU_COPY,
				XV_HELP_DATA, textsw_make_help(textsw, "meditcopy"),
				XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
				NULL);
		menu_items[TEXTSW_MENU_COPY_APPEND] = xv_create(XV_NULL, MENUITEM,
				MENU_RELEASE,
				MENU_STRING, XV_MSG("Append"),
				MENU_VALUE, (long)TEXTSW_MENU_COPY_APPEND,
				XV_HELP_DATA, textsw_make_help(textsw, "meditcopyappend"),
				XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
				NULL);
		sprintf(menuinst, "%s_copycmds", txtinst);
		copy_cmds_item = xv_create(XV_NULL, MENUITEM,
				MENU_RELEASE,
				MENU_STRING, XV_MSG(COPY_STR),
				MENU_PULLRIGHT, xv_create(server, MENU,
						XV_INSTANCE_NAME, menuinst,
						XV_HELP_DATA, textsw_make_help(textsw, "meditcmds"),
						XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
						MENU_APPEND_ITEM, menu_items[TEXTSW_MENU_COPY],
						MENU_APPEND_ITEM, menu_items[TEXTSW_MENU_COPY_APPEND],
						NULL),
				XV_HELP_DATA, textsw_make_help(textsw, "meditcopymenu"),
				XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
				NULL);

	}
	else {
		menu_items[TEXTSW_MENU_COPY] = xv_create(XV_NULL, MENUITEM,
				MENU_RELEASE,
				MENU_STRING, XV_MSG(COPY_STR),
				MENU_VALUE, (long)TEXTSW_MENU_COPY,
				XV_HELP_DATA, textsw_make_help(textsw, "meditcopy"),
				XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
				NULL);
		menu_items[TEXTSW_MENU_COPY_APPEND] = XV_NULL;
		copy_cmds_item = menu_items[TEXTSW_MENU_COPY];
	}
	menu_items[(int)TEXTSW_MENU_PASTE] = xv_create(XV_NULL, MENUITEM,
			MENU_RELEASE,
			MENU_STRING, XV_MSG(PASTE_STR),
			MENU_VALUE, (long)TEXTSW_MENU_PASTE,
			XV_HELP_DATA, textsw_make_help(textsw, "meditpaste"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);
	menu_items[(int)TEXTSW_MENU_CUT] = xv_create(XV_NULL, MENUITEM,
			MENU_RELEASE,
			MENU_STRING, XV_MSG(CUT_STR),
			MENU_VALUE, (long)TEXTSW_MENU_CUT,
			XV_HELP_DATA, textsw_make_help(textsw, "meditcut"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);

	/*
	 * Set accelerator for menu items
	 */
	xv_set(menu_items[(int)TEXTSW_MENU_COPY],
			MENU_ACCELERATOR, "coreset Copy",
			NULL);
	xv_set(menu_items[(int)TEXTSW_MENU_PASTE],
			MENU_ACCELERATOR, "coreset Paste",
			NULL);
	xv_set(menu_items[(int)TEXTSW_MENU_CUT],
			MENU_ACCELERATOR, "coreset Cut",
			NULL);

	sprintf(menuinst, "%s_editcmds", txtinst);
	sub_menu[(int)TXTSW_EDIT_SUB_MENU] = xv_create(server, MENU,
			XV_INSTANCE_NAME, menuinst,
			MENU_GEN_PROC, gen_edit_menu,
			XV_HELP_DATA, textsw_make_help(textsw, "meditcmds"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);
	xv_set(sub_menu[(int)TXTSW_EDIT_SUB_MENU],
			MENU_APPEND_ITEM, menu_items[(int)TEXTSW_MENU_AGAIN],
			MENU_APPEND_ITEM, undo_cmds_item,
			MENU_APPEND_ITEM, menu_items[(int)TEXTSW_MENU_CUT],
			MENU_APPEND_ITEM, copy_cmds_item,
			MENU_APPEND_ITEM, menu_items[(int)TEXTSW_MENU_PASTE],
			NULL);


	menu_items[(int)TEXTSW_MENU_NORMALIZE_LINE] = xv_create(XV_NULL, MENUITEM,
			MENU_RELEASE,
			MENU_STRING, XV_MSG(SEL_LINE_AT_NUM),
			MENU_VALUE, (long)TEXTSW_MENU_NORMALIZE_LINE,
			XV_HELP_DATA, textsw_make_help(textsw, "mselectline"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);
	menu_items[(int)TEXTSW_MENU_COUNT_TO_LINE] = xv_create(XV_NULL, MENUITEM,
			MENU_RELEASE,
			MENU_STRING, XV_MSG(WHAT_LINE_NUM),
			MENU_VALUE, (long)TEXTSW_MENU_COUNT_TO_LINE,
			XV_HELP_DATA, textsw_make_help(textsw, "mwhatline"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);
	menu_items[(int)TEXTSW_MENU_NORMALIZE_INSERTION] = xv_create(XV_NULL, MENUITEM,
			MENU_RELEASE,
			MENU_STRING, XV_MSG(SHOW_CARET_AT_TOP),
			MENU_VALUE, (long)TEXTSW_MENU_NORMALIZE_INSERTION,
			XV_HELP_DATA, textsw_make_help(textsw, "mshowcaret"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);
	break_mode_item = xv_create(XV_NULL, MENUITEM,
			MENU_RELEASE,
			MENU_STRING, XV_MSG(CHANGE_LINE_WRAP),
			MENU_PULLRIGHT, break_mode,
			XV_HELP_DATA, textsw_make_help(textsw, "mchangelinewrap"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);

	sprintf(menuinst, "%s_displaycmds", txtinst);
	sub_menu[TXTSW_VIEW_SUB_MENU] = xv_create(server, MENU,
			XV_INSTANCE_NAME, menuinst,
			XV_HELP_DATA, textsw_make_help(textsw, "mdisplaycmds"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);
	xv_set(sub_menu[(int)TXTSW_VIEW_SUB_MENU],
			MENU_CLIENT_DATA, textsw,
			MENU_APPEND_ITEM, menu_items[(int)TEXTSW_MENU_NORMALIZE_LINE],
			MENU_APPEND_ITEM, menu_items[(int)TEXTSW_MENU_COUNT_TO_LINE],
			MENU_APPEND_ITEM, menu_items[(int)TEXTSW_MENU_NORMALIZE_INSERTION],
			MENU_APPEND_ITEM, break_mode_item, NULL);

	menu_items[(int)TEXTSW_MENU_FIND_AND_REPLACE] = xv_create(XV_NULL, MENUITEM,
			MENU_RELEASE,
			MENU_STRING, XV_MSG(FIND_REPLACE),
			MENU_VALUE, (long)TEXTSW_MENU_FIND_AND_REPLACE,
			XV_HELP_DATA, textsw_make_help(textsw, "mfindreplace"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);
	find_sel_cmds_item = xv_create(XV_NULL, MENUITEM,
			MENU_RELEASE,
			MENU_STRING, XV_MSG(FIND_SELECTION),
			MENU_PULLRIGHT, find_sel_cmds,
			XV_HELP_DATA, textsw_make_help(textsw, "mfindselcmds"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);
	menu_items[(int)TEXTSW_MENU_SEL_MARK_TEXT] = xv_create(XV_NULL, MENUITEM,
			MENU_RELEASE,
			MENU_STRING, XV_MSG(FIND_MARKED_TEXT),
			MENU_VALUE, (long)TEXTSW_MENU_SEL_MARK_TEXT,
			XV_HELP_DATA, textsw_make_help(textsw, "mfindtext"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);
	select_field_cmds_item = xv_create(XV_NULL, MENUITEM,
			MENU_RELEASE,
			MENU_STRING, XV_MSG(REPLACE_FIELD),
			MENU_PULLRIGHT, select_field_cmds,
			XV_HELP_DATA, textsw_make_help(textsw, "mselfieldcmds"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);

	sprintf(menuinst, "%s_findcmds", txtinst);
	sub_menu[TXTSW_FIND_SUB_MENU] = xv_create(server, MENU,
			XV_INSTANCE_NAME, menuinst,
			XV_HELP_DATA, textsw_make_help(textsw, "mfindcmds"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);
	xv_set(sub_menu[TXTSW_FIND_SUB_MENU],
			MENU_APPEND_ITEM, menu_items[(int)TEXTSW_MENU_FIND_AND_REPLACE],
			MENU_APPEND_ITEM, find_sel_cmds_item,
			MENU_APPEND_ITEM, menu_items[(int)TEXTSW_MENU_SEL_MARK_TEXT],
			MENU_APPEND_ITEM, select_field_cmds_item,
			NULL);

	sprintf(menuinst, "%s_extrasmenu", txtinst);
	sub_menu[(int)TXTSW_EXTRAS_SUB_MENU] = xv_create(server, MENU,
			XV_INSTANCE_NAME, menuinst,
			XV_HELP_DATA, textsw_make_help(textsw, "extrasmenu"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);


	sprintf(menuinst, "%s_topmenu", txtinst);
	top_menu = xv_create(server, MENU,
			XV_INSTANCE_NAME, menuinst,
			MENU_TITLE_ITEM, XV_MSG("Text Pane"),
			XV_HELP_DATA, textsw_make_help(textsw, "mtopmenu"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);
	menu_items[TEXTSW_MENU_FILE_CMDS] = xv_create(XV_NULL, MENUITEM,
			MENU_RELEASE,
			MENU_STRING, XV_MSG("File"),
			MENU_PULLRIGHT, sub_menu[(int)TXTSW_FILE_SUB_MENU],
			XV_HELP_DATA, textsw_make_help(textsw, "mfilecmds"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);


	menu_items[TEXTSW_MENU_VIEW_CMDS] = xv_create(XV_NULL, MENUITEM,
			MENU_RELEASE,
			MENU_STRING, XV_MSG("View"),
			MENU_PULLRIGHT, sub_menu[(int)TXTSW_VIEW_SUB_MENU],
			XV_HELP_DATA, textsw_make_help(textsw, "mdisplaycmds"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);
	menu_items[TEXTSW_MENU_EDIT_CMDS] = xv_create(XV_NULL, MENUITEM,
			MENU_RELEASE,
			MENU_STRING,
			XV_MSG("Edit"),
			MENU_PULLRIGHT, sub_menu[(int)TXTSW_EDIT_SUB_MENU],
			XV_HELP_DATA, textsw_make_help(textsw, "meditcmds"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);
	menu_items[TEXTSW_MENU_FIND_CMDS] = xv_create(XV_NULL, MENUITEM,
			MENU_RELEASE,
			MENU_STRING, XV_MSG("Find"),
			MENU_PULLRIGHT, sub_menu[TXTSW_FIND_SUB_MENU],
			XV_HELP_DATA, textsw_make_help(textsw, "mfindcmds"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);
	menu_items[TEXTSW_MENU_EXTRAS_CMDS] = xv_create(XV_NULL, MENUITEM,
			MENU_RELEASE,
			MENU_GEN_PROC, textsw_extras_gen_proc,
			MENU_PULLRIGHT, sub_menu[(int)TXTSW_EXTRAS_SUB_MENU],
			MENU_STRING, XV_MSG("Extras"),
			MENU_CLIENT_DATA, textsw,   /* Ref (hklbrefvklhbs) txt_e_menu.c */
			XV_HELP_DATA, textsw_make_help(textsw, "mextras"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
			NULL);

	filename = textsw_get_extras_filename(server,
								menu_items[TEXTSW_MENU_EXTRAS_CMDS]);
	(void)textsw_build_extras_menu_items(textsw, filename,
			sub_menu[(int)TXTSW_EXTRAS_SUB_MENU]);

	xv_set(top_menu,
			MENU_APPEND_ITEM, menu_items[TEXTSW_MENU_FILE_CMDS],
			MENU_APPEND_ITEM, menu_items[TEXTSW_MENU_VIEW_CMDS],
			MENU_APPEND_ITEM, menu_items[TEXTSW_MENU_EDIT_CMDS],
			MENU_APPEND_ITEM, menu_items[TEXTSW_MENU_FIND_CMDS],
			MENU_APPEND_ITEM, menu_items[TEXTSW_MENU_EXTRAS_CMDS],
			NULL);

	for (index = TEXTSW_MENU_LOAD; index <= TEXTSW_MENU_RESET; index++) {
		if (menu_items[index]) {
			xv_set(menu_items[index],
					MENU_NOTIFY_PROC, textsw_file_do_menu_action, NULL);
		}
	}
	for (index = TEXTSW_MENU_AGAIN; index <= TEXTSW_MENU_CUT; index++) {
		if (menu_items[index]) {
			xv_set(menu_items[index],
					MENU_NOTIFY_PROC, textsw_edit_do_menu_action,
					NULL);
		}
	}
	for (index=TEXTSW_MENU_NORMALIZE_LINE;index<=TEXTSW_MENU_CLIP_LINES;index++)
	{
		if (menu_items[index]) {
			xv_set(menu_items[index],
					MENU_NOTIFY_PROC, textsw_view_do_menu_action,
					NULL);
		}
	}
	for (index = TEXTSW_MENU_FIND_AND_REPLACE;
			index <= TEXTSW_MENU_SEL_PREV_FIELD; index++) {
		if (menu_items[index]) {
			xv_set(menu_items[index],
					MENU_NOTIFY_PROC, textsw_find_do_menu_action,
					NULL);
		}
	}

	/*
	 * This is a fix/hack for menu accelerators.
	 * The menu action procs depend on TEXTSW_MENU_DATA_KEY
	 * to get the textsw view. The textsw view is set on the
	 * menu in the event proc for the textsw view.
	 * Since we don't have that info when accelerators are
	 * used, we have to set it here. A new key to store the
	 * textsw is used because at this time in this function,
	 * the views do not exist yet.
	 *
	 * Note:
	 * Sharing of the textsw menus may break menu accelerators.
	 */
	xv_set(sub_menu[(int)TXTSW_FILE_SUB_MENU],
			XV_KEY_DATA, TEXTSW_HANDLE_KEY, textsw,
			NULL);
	xv_set(sub_menu[(int)TXTSW_EDIT_SUB_MENU],
			XV_KEY_DATA, TEXTSW_HANDLE_KEY, textsw,
			NULL);
	xv_set(undo_cmds, XV_KEY_DATA, TEXTSW_HANDLE_KEY, textsw, NULL);
	xv_set(find_sel_cmds, XV_KEY_DATA, TEXTSW_HANDLE_KEY, textsw, NULL);

	frame = xv_get(textsw, WIN_FRAME);
	xv_set(sub_menu[(int)TXTSW_EDIT_SUB_MENU],
			MENU_GEN_PIN_WINDOW, frame, XV_MSG("Edit"),
			NULL);

	return top_menu;
}

static void textsw_new_menu(Textsw_private priv)
{
	Menu top_menu;
	register Menu *sub_menu;
	register Menu_item *menu_items;
	Textsw textsw = TEXTSW_PUBLIC(priv);

	if (!STORE_FILE_POPUP_KEY) {
		STORE_FILE_POPUP_KEY = xv_unique_key();
		SAVE_FILE_POPUP_KEY = xv_unique_key();
		LOAD_FILE_POPUP_KEY = xv_unique_key();
		FILE_STUFF_POPUP_KEY = xv_unique_key();
		SEARCH_POPUP_KEY = xv_unique_key();
		MATCH_POPUP_KEY = xv_unique_key();
		SEL_LINE_POPUP_KEY = xv_unique_key();
		EXTRASMENU_FILENAME_KEY = xv_unique_key();
		TEXTSW_MENU_DATA_KEY = xv_unique_key();
		TEXTSW_HANDLE_KEY = xv_unique_key();
		TXT_MENU_ITEMS_KEY = xv_unique_key();
		TXT_FILE_MENU_KEY = xv_unique_key();
		TXT_SET_DEF_KEY = xv_unique_key();
		TEXTSW_CURRENT_POPUP_KEY = xv_unique_key();
		FC_PARENT_KEY = xv_unique_key();
		FC_EXTEN_ITEM_KEY = xv_unique_key();
	}

	menu_items = (Menu_item *) calloc((size_t)TEXTSW_MENU_LAST_CMD,
			sizeof(Menu_item));

	if (priv->restrict_menu) {
		top_menu = textsw_new_restricted_menu(priv, textsw, menu_items);
	}
	else {
		sub_menu = (Menu *) calloc(((size_t)TXTSW_EXTRAS_SUB_MENU + 1),
									sizeof(Menu));
		top_menu = textsw_new_full_menu(priv, textsw, menu_items, sub_menu);
		priv->sub_menu_table = sub_menu;
	}

	priv->menu_table = menu_items;

	xv_set(top_menu, MENU_DONE_PROC, textsw_done_menu, NULL);
	textsw_menu_set(priv, top_menu);
}


Pkg_private Menu textsw_menu_init(Textsw_private priv)
{
	textsw_new_menu(priv);

	return textsw_menu_get(priv);
}

Pkg_private Menu textsw_get_unique_menu(Textsw_private priv)
{
	return textsw_menu_get(priv);
}

Pkg_private void textsw_do_menu(Textsw_view_private view, Event *ie)
{
    register Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
    Textsw_view     textsw_view = VIEW_PUBLIC(view);

    /* freeze caret so don't invalidate menu's bitmap under data */
    textsw_freeze_caret(priv);

    xv_set(textsw_menu_get(priv), XV_KEY_DATA, TEXTSW_MENU_DATA_KEY, textsw_view, NULL);
	/* before I built in this call, termsw lost it's caret
	 * when the menu was popped up. This happens in textsw_new_menu for
	 * the 'original' textsw menu, but not, if an application (or another
	 * package) puts a new menu here using xv_set(... WIN_MENU ...)
	 */
    xv_set(textsw_menu_get(priv), MENU_DONE_PROC, textsw_done_menu, NULL);

    menu_show(textsw_menu_get(priv), textsw_view, ie, NULL);
}

Pkg_private Textsw_view textsw_from_menu(Menu menu)
{
	Textsw_view textsw_view = XV_NULL;
	Menu temp_menu;
	Menu_item temp_item;

	/* as MENU_PARENT seems to be a little volatile, I first try
	 * the XV_KEY_DATA, TEXTSW_MENU_DATA_KEY at first
	 */
	textsw_view = xv_get(menu, XV_KEY_DATA, TEXTSW_MENU_DATA_KEY);
	if (textsw_view) return textsw_view;

	while (menu) {
		temp_item = xv_get(menu, MENU_PARENT);
		if (temp_item) {
			temp_menu = xv_get(temp_item, MENU_PARENT);

			/* if there is no menu parent, use menu's view */
			if (temp_menu == XV_NULL)
				textsw_view = xv_get(menu, XV_KEY_DATA, TEXTSW_MENU_DATA_KEY);
			menu = temp_menu;
		}
		else {
			textsw_view = xv_get(menu, XV_KEY_DATA, TEXTSW_MENU_DATA_KEY);
			break;
		}
	}
	return textsw_view;
}

Xv_private void textsw_file_do_menu_action(Menu cmd_menu, Menu_item cmd_item)
{
	Textsw tsw;
	Textsw_view textsw_view = textsw_from_menu(cmd_menu);
	register Textsw_view_private view;
	register Textsw_private priv;
	Textsw_menu_cmd cmd = (Textsw_menu_cmd) xv_get(cmd_item, MENU_VALUE);
	register Event *ie = (Event *) xv_get(cmd_menu, MENU_FIRST_EVENT);
	Xv_Notice text_notice;

	if (AN_ERROR(textsw_view == 0)) {
		if (event_action(ie) == ACTION_ACCELERATOR) {
			tsw = xv_get(cmd_menu, XV_KEY_DATA, TEXTSW_HANDLE_KEY);
			priv = TEXTSW_PRIVATE(tsw);
			textsw_view = (Textsw_view) xv_get(tsw, OPENWIN_NTH_VIEW, 0);
			view = VIEW_ABS_TO_REP(textsw_view);
		}
		else {
			return;
		}
	}
	else {
		assert(xv_get(textsw_view, XV_IS_SUBTYPE_OF, OPENWIN_VIEW));
		view = VIEW_PRIVATE(textsw_view);
		tsw = xv_get(textsw_view, XV_OWNER);
		priv = TEXTSW_PRIVATE(tsw);
	}

	switch (cmd) {
		case TEXTSW_MENU_RESET:
			textsw_empty_document(tsw, ie);
			xv_set(cmd_menu, MENU_DEFAULT, 1, NULL);
			break;

		case TEXTSW_MENU_LOAD:{
				Frame base_frame = (Frame) xv_get(tsw, WIN_FRAME);
				Frame popup = (Frame) xv_get(base_frame, XV_KEY_DATA,
						LOAD_FILE_POPUP_KEY);

				if (priv->state & TXTSW_NO_LOAD) {
					Frame frame = xv_get(tsw, WIN_FRAME);
					text_notice = xv_get(frame, XV_KEY_DATA, text_notice_key);
					if (!text_notice) {
						text_notice = xv_create(frame, NOTICE, NULL);

						xv_set(frame,
								XV_KEY_DATA, text_notice_key, text_notice,
								NULL);
					}
					xv_set(text_notice,
							NOTICE_MESSAGE_STRINGS,
								XV_MSG("Illegal Operation.\n\
Load File has been disabled."),
								NULL,
							NOTICE_BUTTON_YES, XV_MSG("Continue"),
							XV_SHOW, TRUE,
							NULL);

					break;
				}
				if (popup) {
					textsw_set_dir_str((int)TEXTSW_MENU_LOAD);
					textsw_get_and_set_selection(popup, view,
							(int)TEXTSW_MENU_LOAD);
				}
				else {
					textsw_create_popup_frame(view, (int)TEXTSW_MENU_LOAD);
				}
				break;
			}

		case TEXTSW_MENU_SAVE:{
				textsw_do_save(tsw, priv, view);
				break;
			}

		case TEXTSW_MENU_STORE:{
				Frame base_frame =  xv_get(tsw, WIN_FRAME);
				Frame popup =  xv_get(base_frame, XV_KEY_DATA,
						STORE_FILE_POPUP_KEY);

				if (popup) {
					textsw_set_dir_str((int)TEXTSW_MENU_STORE);
					textsw_get_and_set_selection(popup, view,
							(int)TEXTSW_MENU_STORE);
				}
				else {
					textsw_create_popup_frame(view, (int)TEXTSW_MENU_STORE);
				}
				break;
			}

		case TEXTSW_MENU_FILE_STUFF:{
				Frame base_frame = xv_get(tsw, WIN_FRAME);
				Frame popup = xv_get(base_frame, XV_KEY_DATA,
						FILE_STUFF_POPUP_KEY);

				if (popup) {
					textsw_set_dir_str((int)TEXTSW_MENU_FILE_STUFF);
					textsw_get_and_set_selection(popup, view,
							(int)TEXTSW_MENU_FILE_STUFF);
				}
				else {
					textsw_create_popup_frame(view,(int)TEXTSW_MENU_FILE_STUFF);
				}
				break;
			}

		default:
			break;
	}
}

/*
 * called after a file is loaded, this sets the menu default to save file
 */

Pkg_private void textsw_do_save(Textsw abstract, Textsw_private textsw, Textsw_view_private view)
{
    Frame           base_frame = (Frame) xv_get(abstract, WIN_FRAME);
    Frame           popup      = (Frame) xv_get(base_frame, XV_KEY_DATA,
						    SAVE_FILE_POPUP_KEY);
    CHAR           *name; 
    Xv_Notice	    text_notice;

    if (textsw_has_been_modified(abstract)) {
        Es_handle       original;

        original = (Es_handle) es_get(textsw->views->esh, ES_PS_ORIGINAL);
        if ((TXTSW_IS_READ_ONLY(textsw)) || (original == ES_NULL) ||
            ((Es_enum) ((long)es_get(original, ES_TYPE)) != ES_TYPE_FILE)) {

            if ((Es_enum) ((long)es_get(original, ES_TYPE)) != ES_TYPE_FILE) {
                goto final;
            }
                        

            text_notice = xv_get(base_frame, XV_KEY_DATA, text_notice_key);

            if (!text_notice)  {
                text_notice = xv_create(base_frame, NOTICE, NULL);
                xv_set(base_frame, XV_KEY_DATA, text_notice_key, text_notice,
                    NULL);
            }
			xv_set(text_notice, 
					NOTICE_MESSAGE_STRINGS,
						XV_MSG("Unable to Save Current File."),
						NULL,
					NOTICE_BUTTON_YES, XV_MSG("Continue"),
					XV_SHOW, TRUE, 
					NULL);
            return;		/* jcb */
        }
    } else {
        text_notice = xv_get(base_frame, XV_KEY_DATA, text_notice_key);

        if (!text_notice)  {
            text_notice = xv_create(base_frame, NOTICE, NULL);

            xv_set(base_frame, XV_KEY_DATA, text_notice_key, text_notice, NULL);
        }
		xv_set(text_notice, 
			NOTICE_MESSAGE_STRINGS,
				XV_MSG("File has not been modified.\n\
Save File operation ignored."),
			NULL,
			NOTICE_BUTTON_YES, XV_MSG("Continue"),
			XV_SHOW, TRUE, 
			NULL);
        return;
    }

    if (textsw_file_name(TSWPRIV_FOR_VIEWPRIV(view), &name) == 0)  {
	int	confirm_state_changed = 0;

	if (textsw->state & TXTSW_CONFIRM_OVERWRITE) {
	    textsw->state &= ~TXTSW_CONFIRM_OVERWRITE;
	    confirm_state_changed = 1;
	}
#ifdef OW_I18N
        textsw_store_file_wcs(VIEW_PUBLIC(view),name,0,0);
#else
        textsw_store_file(abstract, name,0,0);
#endif
	if (confirm_state_changed)
	    textsw->state |= TXTSW_CONFIRM_OVERWRITE;
        return;
    }
                   
final: 

    popup = (Frame) xv_get(base_frame, XV_KEY_DATA, SAVE_FILE_POPUP_KEY);
    if (popup){
        (void) textsw_get_and_set_selection(popup, view, (int) TEXTSW_MENU_SAVE);
    }
    else {
        (void) textsw_create_popup_frame(view, (int) TEXTSW_MENU_SAVE);
    }
}
