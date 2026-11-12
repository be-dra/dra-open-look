#ifndef lint
#ifdef sccs
char p_txt_c_sccsid[] = "@(#)p_txt.c 20.217 93/06/28 DRA: $Id: p_txt.c,v 4.70 2026/11/11 21:55:40 dra Exp $";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL_NOTICE
 *	file for terms of the license.
 */

#include <malloc.h>
#include <string.h>
#include <ctype.h>
#include <X11/X.h>
#include <xview_private/draw_impl.h>
#include <xview_private/panel_impl.h>
#include <xview_private/svr_impl.h>
#include <xview_private/win_info.h>
#include <xview_private/font_impl.h>
#include <xview/cursor.h>
#include <xview/defaults.h>
#include <xview/notice.h>
#include <xview/screen.h>
#include <xview/pixwin.h>
#include <xview/font.h>
#include <xview/openmenu.h>
#include <stdlib.h>

#if defined(SEL_DEBUG) && defined(DEBUG)
#define sdfprintf(a)	fprintf a
#else
#define sdfprintf(a)
#endif

/* External procedures */
Xv_private void screen_adjust_gc_color(Xv_Window window, int gc_index);
Xv_private void	    win_grab_quick_sel_keys(Xv_window);

typedef enum {
    INVALID,    /* UNDO => do nothing on */
    INSERT,     /* UNDO => insert contents of undo_buffer into dp->value */
    DELETE      /* UNDO => delete contents of undo_buffer from dp->value */
} Undo_direction;

typedef struct {
    Panel_item      public_self;/* back pointer to object */
    int             caret_offset;	/* caret's x offset from right margin
					 * of left arrow (which may be blank).
					 * -1 = invalid. */
    int             caret_position;	/* caret's character position */
    u_char   	    delete_pending;	/* primary selection is
					 * pending-delete */
    int             display_length;	/* in characters */
    int		    display_width;	/* in pixels */
    Selection_item  dnd_item;	/* Drag and Drop Selection Item */
    int		    dnd_sel_first; /* index of first char in dnd selection */
    int		    dnd_sel_last; /* index of last char in dnd selection */
    Drag_drop	    dnd;	/* Drag and Drop object */
    Drop_site_item  drop_site;	/* Drag and Drop Site item */
    int             ext_first;	/* first char of extended word */
    int             ext_last;	/* last char of extended word */
    int             first_char;	/* first displayed character */
    int             flags;
    int             font_home;
    int             last_char;	/* last displayed character */
    struct timeval  last_click_time;
    char            mask;
    Panel_setting   notify_level;	/* NONE, SPECIFIED, NON_PRINTABLE,
					 * ALL */
    int		    scroll_btn_height;	/* Abbrev_MenuButton_Height() */
    int		    scroll_btn_width;	/* Abbrev_MenuButton_Width() + space */
    unsigned long   sel_length_data;	/* length of selection */
    int		    select_click_cnt[2]; /* nbr of select mouse clicks
					  * pending (primary, secondary) */
    int		    select_down_x;
	    /* x coordinate of SELECT-down event. Used in determining when to
	     * initiate a drag and drop operation. */
    int		    select_down_y;
	    /* y coordinate of SELECT-down event. Used in determining when to
	     * initiate a drag and drop operation. */
    int        sel_first[NBR_PANEL_SELECTIONS];	/* index of first char selected
												 * (primary, secondary) */
    int        sel_last[NBR_PANEL_SELECTIONS];	/* index of last char selected
												 * (primary, secondary) */
    int             stored_length;
    char           *terminators;
    Rect            text_rect;	/* rect containing text (i.e., not arrows) */
    char	   *undo_buffer;
    Undo_direction  undo_direction;	/* Insert or delete the contents
					 * of the undo_buffer to or from
					 * dp->value, or undo_buffer is
					 * invalid */
    char           *value;
    int             value_offset;	/* right margin of last displayed
					 * char (x offset from right margin
					 * of left arrow) */
    int             saved_caret_offset; /* caret's x offset, saved when
                                         * conv mode on and commit. */
    int             saved_caret_position; /* caret's character position */
                                        /* saved when conv mode on and commit */
	int next_field;
	Menu_item cutitem, copyitem, deleteitem;
    unsigned long sel_reply; /* transient for selection replies */
	int select_is_word[NBR_PANEL_SELECTIONS];
} Text_info;

typedef enum {
    HL_NONE,
    HL_UNDERLINE,
    HL_STRIKE_THRU,
    HL_INVERT
} Highlight;

/************************************************************************
 * Panel text item private macros, constants, and data structures	*
 ************************************************************************/

/* Macros */
#define TEXT_PRIVATE(item)      \
	XV_PRIVATE(Text_info, Xv_panel_text, item)
#define TEXT_PUBLIC(item)       XV_PUBLIC(item)
#define TEXT_FROM_ITEM(ip)      TEXT_PRIVATE(ITEM_PUBLIC(ip))

/* Constants */
#define PV_HIGHLIGHT TRUE
#define PV_NO_HIGHLIGHT FALSE
#define BOX_Y   2
#define LINE_Y   1
#define SCROLL_BTN_GAP 3        /* space between Scrolling Button and text */

/* dp->flags masks */
#define SELECTING_ITEM          0x00000001
#define TEXT_HIGHLIGHTED        0x00000002
#define UNDERLINED              0x00000004
#define PTXT_READ_ONLY          0x00000008
#define TEXT_SELECTED           0x00000010
#define PTXT_ALLOW_FILE_DROP    0x00001000

		/* text item has a nonzero length primary seleciton */
#define LEFT_SCROLL_BTN_SELECTED  0x00000020
#define RIGHT_SCROLL_BTN_SELECTED 0x00000040
#define SELECTING_SCROLL_BTN      0x00000080
#define SELECTION_REQUEST_FAILED  0x00000100

#define DRAG_MOVE_FILENAME      0x00000001
#define DRAG_MOVE_TEXT          0x00000002
#define DROP_OR_PASTE_FAILED    0x00000004

/* Panel Item Operations */
static void text_handle_event(Panel_item item_public, Event *event);
static void text_begin_preview (Panel_item item_public, Event *event);
static void text_cancel_preview(Panel_item item_public, Event *event);
static void text_accept_preview(Panel_item item_public, Event *event);
static void create_menu(Panel_item item);
static void text_accept_menu(Panel_item item, Event *ev);
static void text_accept_key(Panel_item item_public, register Event *event);
static void text_clear( Panel_item item_public);
static void text_paint(Panel_item item_public, Panel_setting u);
static void text_remove(Panel_item item_public);
static void text_restore(Panel_item item_public, Panel_setting u);
static void text_layout(Panel_item item_public, Rect *deltas);
static void text_accept_kbd_focus(Panel_item item_public);
static void text_yield_kbd_focus(Panel_item item_public);

/* Local functions */
static int char_position(int , Xv_Font , char *, int );
static void draw_scroll_btn(Item_info *ip, int state);
static void horizontal_scroll(Item_info *ip, int shift);
#ifdef PAINT_BOX
static void	paint_box();
#endif /* PAINT_BOX */
static void paint_caret(Item_info *ip, int on);
static void paint_text(Item_info *ip);
static void paint_value(Item_info *ip, int highlight);
static void panel_find_word(Text_info *dp, int *first, int *last);
static void panel_multiclick_handler(Item_info *ip, Event *event, int rank);
static void panel_select_line(Item_info *ip, Event *event, int rank);
static void text_add_selection(Panel_info *panel, Item_info *ip, int);
static void text_seln_dehighlight(Item_info *ip, int rank);
static int text_seln_delete(Item_info *ip, int rank);
static void text_seln_highlight(Panel_info *panel, Item_info *ip, int rank);
static void text_seln_init(Panel_info *panel);
static void text_set_clipboard(Panel_info *panel, Item_info *ip, Text_info *dp);
static void text_set_sel_data(Selection_item selit, Text_info *dp, int rank);
static void update_caret_offset(Item_info *ip, int , int );
static void update_text_rect(Item_info *ip);
static void update_value(Item_info *ip, int action,
    int ok_to_insert, int synthetic_event, int *retstatus, char *ev_str,
	int ev_flags);
static void update_value_offset(Item_info *ip, int val_change, int , int );

static Panel_ops ops = {
    text_handle_event,			/* handle_event() */
    text_begin_preview,			/* begin_preview() */
    text_begin_preview,			/* update_preview() */
    text_cancel_preview,		/* cancel_preview() */
    text_accept_preview,		/* accept_preview() */
    text_accept_menu,
    text_accept_key,			/* accept_key() */
    text_clear,				/* clear() */
    text_paint,				/* paint() */
    NULL,				/* resize() */
    text_remove,			/* remove() */
    text_restore,			/* restore() */
    text_layout,			/* layout() */
    text_accept_kbd_focus,		/* accept_kbd_focus() */
    text_yield_kbd_focus,		/* yield_kbd_focus() */
    NULL				/* extension: reserved for future use */
};

static int item_key = 0; 

/* Local data */
static Panel_info *primary_seln_panel, *secondary_seln_panel;
static Rect     primary_seln_rect, secondary_seln_rect;
static int      primary_seln_first, secondary_seln_first;
static int      primary_seln_last, secondary_seln_last;
static short	delim_init = FALSE; /* delim_table initialized */
static char	delim_table[256];   /* TRUE= character is a word delimiter */
static Highlight sel_highlight = HL_NONE;
static char	no_buttons; /* Flag indicating that no space needs to be
			    * reserved for the scroll buttons.
			    */


/* ========================================================================= */

static int text_convert_proc(Selection_owner sel_own, Atom *type,
					Xv_opaque *data, unsigned long *length, int *format)
{
	Text_info *dp;
	Panel panel_public;
	Panel_info *panel;
	Atom rank_atom;
	int rank_index;
	Xv_Server srv;

	panel_public = xv_get(sel_own, XV_KEY_DATA, item_key);
	panel = PANEL_PRIVATE(panel_public);
	srv = XV_SERVER_FROM_WINDOW(panel_public);

	rank_atom = (Atom) xv_get(sel_own, SEL_RANK);
	SERVERTRACE((765, "%s request for %s\n",
				(char *)xv_get(srv, SERVER_ATOM_NAME, rank_atom),
				(char *)xv_get(srv, SERVER_ATOM_NAME, *type)));

	if (*type == panel->atom.delete) {
		if (rank_atom == XA_SECONDARY)
			rank_index = PANEL_SEL_SECONDARY;	/* Quick Move */
		else if (rank_atom == panel->atom.clipboard) {
			/* we do not want to delete "from the clipboard" */
			return FALSE;
		}
		else if (rank_atom == XA_PRIMARY)
			rank_index = PANEL_SEL_PRIMARY;
		else rank_index = PANEL_SEL_DND;

		return text_seln_delete(panel->sel_holder[rank_index], rank_index);
	}
	if (*type == panel->atom.selection_end && rank_atom==XA_SECONDARY) {
		/* Lose the Selection - we support this only for SECONDARY */
		xv_set(sel_own, SEL_OWN, FALSE, NULL);
		*type = panel->atom.null;
		*data = XV_NULL;
		*length = 0;
		*format = 32;
		return TRUE;
	}
	if (*type == panel->atom.seln_yield && rank_atom==XA_SECONDARY) {
		static long answer;
		/* Lose the Selection - we support this only for SECONDARY */
		xv_set(sel_own, SEL_OWN, FALSE, NULL);
		answer = 1L;
		*data = (Xv_opaque)&answer;
		*length = 1;
		*format = 32;
		return TRUE;
	}
	if (*type == panel->atom.length) {
		/* This is only used by SunView1 selection clients for
		 * clipboard and secondary selections.
		 */
		if (rank_atom == XA_SECONDARY) rank_index = PANEL_SEL_SECONDARY;
		else if (rank_atom == panel->atom.clipboard)
			rank_index = PANEL_SEL_CLIPBOARD;
		else if (rank_atom == XA_PRIMARY) rank_index = PANEL_SEL_PRIMARY;
		else rank_index = PANEL_SEL_DND;

		if (!panel->sel_holder[rank_index]) return FALSE;
		dp = TEXT_FROM_ITEM(panel->sel_holder[rank_index]);
		dp->sel_length_data =
				(unsigned long)xv_get(panel->sel_item[rank_index], SEL_LENGTH);
		*data = (Xv_opaque) & dp->sel_length_data;
		*length = 1;
		*format = 32;
		return TRUE;
	}
	if (*type==(Atom)xv_get(srv,SERVER_ATOM,"_SUN_SELN_IS_READONLY")) {
		if (rank_atom == XA_SECONDARY) rank_index = PANEL_SEL_SECONDARY;
		else if (rank_atom == panel->atom.clipboard)
			rank_index = PANEL_SEL_CLIPBOARD;
		else if (rank_atom == XA_PRIMARY) rank_index = PANEL_SEL_PRIMARY;
		else rank_index = PANEL_SEL_DND;

		if (!panel->sel_holder[rank_index]) return FALSE;
		dp = TEXT_FROM_ITEM(panel->sel_holder[rank_index]);
		if ((dp->flags & PTXT_READ_ONLY) != 0) {
			dp->sel_reply = 1;
		}
		else {
			dp->sel_reply = 0;
		}
		*data = (Xv_opaque)&dp->sel_reply;
		*length = 1;
		*format = 32;
		return TRUE;
	}
	if (*type == (Atom)xv_get(srv, SERVER_ATOM, "_SUN_DRAGDROP_ACK")) {
		/* Test: I have a Textsw that is also able to accept 'non-text data'
		 * and therefore calls dnd_decode_drop and TRIES to convert to, 
		 * say, PIXMAP.
		 * If this fails, the XView Textsw calls dnd_decode_drop AGAIN... and
		 * this second request failed because _SUN_DRAGDROP_ACK was rejected
		 */
		*format = 32;
		*length = 0;
		*data = XV_NULL;
		*type = xv_get(srv, SERVER_ATOM, "NULL");
		return TRUE;
	}
	if (*type==(Atom)xv_get(srv,SERVER_ATOM,"_OL_SELECTION_IS_WORD")) {
		if (rank_atom == XA_SECONDARY) rank_index = PANEL_SEL_SECONDARY;
		else if (rank_atom == panel->atom.clipboard)
			rank_index = PANEL_SEL_CLIPBOARD;
		else if (rank_atom == XA_PRIMARY) rank_index = PANEL_SEL_PRIMARY;
		else rank_index = PANEL_SEL_DND;

		dp = TEXT_FROM_ITEM(panel->sel_holder[rank_index]);
		dp->sel_reply = dp->select_is_word[rank_index];
		SERVERTRACE((377, "%s: answering %d\n", __FUNCTION__, dp->sel_reply));
		*data = (Xv_opaque)&dp->sel_reply;
		*length = 1;
		*format = 32;
		*type = XA_INTEGER;
		return TRUE;
	}
	if (*type == (Atom)xv_get(srv,SERVER_ATOM,"TARGETS")) {
		static Atom tgts[10];
		int i = 0;

		if (rank_atom != panel->atom.clipboard) tgts[i++] = panel->atom.delete;
		if (rank_atom == XA_SECONDARY) {
			tgts[i++] = panel->atom.selection_end;
			tgts[i++] = panel->atom.seln_yield;
		}
		tgts[i++] = panel->atom.length;
		tgts[i++] = xv_get(srv,SERVER_ATOM,"_SUN_SELN_IS_READONLY");
		tgts[i++] = xv_get(srv,SERVER_ATOM,"_OL_SELECTION_IS_WORD");
		tgts[i++] = XA_STRING;
		tgts[i++] = *type;
		tgts[i++] = xv_get(srv,SERVER_ATOM,"TIMESTAMP");
		tgts[i++] = panel->atom.compound_text;
		*data = (Xv_opaque)tgts;
		*length = i;
		*format = 32;
		*type = XA_ATOM;
		return TRUE;
	}
	if (*type == panel->atom.compound_text) {
		Atom fake_type = XA_STRING;

		/* for now, we handle this as if it were STRING */
		return sel_convert_proc(sel_own, &fake_type, data, length, format);
	}
	else if (*type == (Atom)xv_get(srv,SERVER_ATOM,"TEXT")) {
		*type = XA_STRING;

		/* for now, we handle this as if it were STRING */
		return sel_convert_proc(sel_own, type, data, length, format);
	}
	else {
		int retval;
		/* Use default Selection Package convert procedure */

		retval = sel_convert_proc(sel_own, type, data, (unsigned long *)length,
				format);
#ifdef NO_XDND
#else /* NO_XDND */
		if (! retval) {
			Xv_Server srv = XV_SERVER_FROM_WINDOW(panel_public);
			if (*type == (Atom)xv_get(srv, SERVER_ATOM, "text/plain")) {
				Atom savat = *type;

				*type = XA_STRING;
				retval = sel_convert_proc(sel_own, type, data,
								(unsigned long *)length, format);

				*type = savat;
			}
		}
#endif /* NO_XDND */

		return retval;
	}

#ifdef BEFORE_DRA_CHANGED_IT
  Done:
	*type = panel->atom.null;
	*data = XV_NULL;
	*length = 0;
	*format = 32;
	return TRUE;
#else
	return FALSE; /* we reject it.... */
#endif
}

static void text_lose_rank(Panel_info *panel, int rank)
{
	Text_info *dp;

	if (!panel->sel_holder[rank])
		return;
	text_seln_dehighlight(panel->sel_holder[rank], rank);
	if (rank <= PANEL_SEL_SECONDARY) {
		dp = TEXT_FROM_ITEM(panel->sel_holder[rank]);
		if (rank == PANEL_SEL_PRIMARY)
			dp->flags &= ~TEXT_SELECTED;
		dp->select_click_cnt[rank] = 0;
		SERVERTRACE((444, "scc[%d] = %d\n", rank, dp->select_click_cnt[rank]));
		dp->sel_first[rank] = 0;
		dp->sel_last[rank] = 0;
	}
	panel->sel_holder[rank] = NULL;
}


/* Panel Text Item has lost ownership of the selection. */
static void text_lose_proc(Selection_owner sel_owner)
{
	Panel_info *panel;
	Atom rank_atom;
	int rank_index;

	panel = PANEL_PRIVATE(xv_get(sel_owner, XV_KEY_DATA, item_key));
	rank_atom = (Atom) xv_get(sel_owner, SEL_RANK);
	for (rank_index = 0; rank_index < NBR_PANEL_SELECTIONS; rank_index++) {
		if (rank_atom == panel->sel_rank[rank_index])
			break;
	}
	if (panel->sel_holder[rank_index])
		text_lose_rank(panel, rank_index);
}


static void dnd_lose_proc(Drag_drop dnd)
{
	Panel_info *panel = PANEL_PRIVATE(xv_get(dnd, XV_KEY_DATA, item_key));

	if (panel->sel_owner[PANEL_SEL_DND] == dnd) {
		/* these are set immediately before dnd_send_drop -
		 * now we remove them
		 */
		panel->sel_owner[PANEL_SEL_DND] = XV_NULL;
		panel->sel_holder[PANEL_SEL_DND] = NULL;
	}
}

/* -------------------- XView Functions  -------------------- */
static int text_init(Panel panel_public, Panel_item item_public,
							Attr_avlist avlist, int *u)
{
	Text_info *dp;
	int chrht;
	char *delims;	/* printf formatted text.delimChars default */
	char delim_chars[256];	/* delimiter characters */
	int i;
	Item_info *ip = ITEM_PRIVATE(item_public);
	Xv_Drawable_info *info;
	Xv_panel_text *item_object = (Xv_panel_text *) item_public;
	Panel_info *panel = PANEL_PRIVATE(panel_public);
	int pc_home_y;
	Xv_Window pw;	/* paint window */
	XFontStruct *x_font_info;

	if (item_key == 0) {
		item_key = xv_unique_key();
	}

	dp = xv_alloc(Text_info);

	/* link to object */
	item_object->private_data = (Xv_opaque) dp;
	dp->public_self = item_public;

	if (!panel->sel_owner[PANEL_SEL_PRIMARY]) {
		text_seln_init(panel);
		PANEL_EACH_PAINT_WINDOW(panel, pw)
			win_grab_quick_sel_keys(pw);
		PANEL_END_EACH_PAINT_WINDOW
	}

	ip->ops = ops;
	if (panel->event_proc)
		ip->ops.panel_op_handle_event = (void (*)(Xv_opaque, Event *))panel->event_proc;
	ip->item_type = PANEL_TEXT_ITEM;
	if (ip->notify == panel_nullproc)
		ip->notify = (int (*)(Xv_opaque, Event *))panel_text_notify;
	panel_set_bold_label_font(ip);

	x_font_info = (XFontStruct *) xv_get(ip->value_font, FONT_INFO);
	pc_home_y = -x_font_info->ascent;

	if (pc_home_y < dp->font_home)
		dp->font_home = pc_home_y;

	dp->font_home = -dp->font_home;

	dp->display_length = 80;
	dp->display_width = panel_col_to_x(ip->value_font, dp->display_length);
	dp->flags |= UNDERLINED;
	dp->flags |= PTXT_ALLOW_FILE_DROP;

	dp->mask = '\0';

	dp->notify_level = PANEL_SPECIFIED;
	dp->scroll_btn_height = TextScrollButton_Height(panel->ginfo);
	dp->scroll_btn_width = TextScrollButton_Width(panel->ginfo) +
			SCROLL_BTN_GAP;
	dp->stored_length = 80;

	dp->terminators = (char *)panel_strsave("\n\r\t");
	dp->undo_buffer = (char *)xv_calloc(1, (u_int) (dp->stored_length + 1));
	dp->undo_direction = INVALID;

	dp->value = (char *)xv_calloc(1, (u_int) (dp->stored_length + 1));
	if (!dp->undo_buffer || !dp->value) return XV_ERROR;

	ip->value_rect.r_width = dp->display_width;
	chrht = xv_get(ip->value_font, FONT_DEFAULT_CHAR_HEIGHT);
	ip->value_rect.r_height = MAX(chrht + BOX_Y, dp->scroll_btn_height);

	dp->dnd = xv_create(panel_public, DRAGDROP,
			SEL_CONVERT_PROC, text_convert_proc,
			SEL_LOSE_PROC, dnd_lose_proc,
			DND_BASIC_DATA_NAMES,
				"STRING",
				"text/plain",
				NULL,
			XV_KEY_DATA, item_key, panel_public,
			NULL);

	dp->dnd_item = xv_create(dp->dnd, SELECTION_ITEM, NULL);

	dp->drop_site = xv_create(PANEL_PUBLIC(panel), DROP_SITE_ITEM,
					DROP_SITE_EVENT_MASK, DND_ENTERLEAVE | DND_ACCEPT_STRING,
					DROP_SITE_REGION, &ip->value_rect,
					NULL);

	ip->flags |= WANTS_KEY | WANTS_ISO | WANTS_ADJUST;

	/* If the pixmap used to save and restore the pixels underneath the
	 * caret hasn't been created yet, then do so now.
	 */
	if (!panel->caret_bg_pixmap) {
		DRAWABLE_INFO_MACRO(panel_public, info);
		panel->caret_bg_pixmap = XCreatePixmap(xv_display(info),
				xv_get(xv_get(xv_screen(info), XV_ROOT), XV_XID),
				(unsigned)MAX(panel->active_caret_width, panel->inactive_caret_width),
				(unsigned)MAX(panel->active_caret_height, panel->inactive_caret_height),
				xv_depth(info));
	}

	dp->next_field = defaults_get_integer("keyboard.nextElement",
								"Keyboard.NextElement", '\t');	/* Tab */
	/*
	 * Initialize the word delimiter table
	 */
	if (!delim_init) {
		delims = (char *)defaults_get_string("text.delimiterChars",
				"Text.DelimiterChars", " \t,.:;?!\'\"`*/-+=(){}[]<>\\|~@#$%^&");
		/* Print the string into an array to parse the potential
		 * octal/special characters.
		 */
		sprintf(delim_chars, delims);
		/* Mark off the delimiters specified */
		for (i = 0; i < 256; i++)
			delim_table[i] = FALSE;
		for (delims = delim_chars; *delims; delims++)
			delim_table[(int)*delims] = TRUE;
		delim_init = TRUE;
	}

	/* A Text Item is, by default, a First-Class (primary) focus client */
	xv_set(item_public,
			PANEL_PAINT, PANEL_NONE, XV_FOCUS_RANK, XV_FOCUS_PRIMARY, NULL);

	/* The panel now contains (at least one) First-Class (primary)
	 * focus client
	 */
	xv_set(panel_public, XV_FOCUS_RANK, XV_FOCUS_PRIMARY, NULL);

	return XV_OK;
}

static Xv_opaque text_set_avlist(Panel_item item_public, Attr_avlist avlist)
{
	int display_width_set = FALSE;
	Text_info *dp = TEXT_PRIVATE(item_public);
	Item_info *ip = ITEM_PRIVATE(item_public);
	char *new_value = NULL;

	short select_line = FALSE;
	short value_rect_changed = FALSE;
	Panel_info *panel = ip->panel;
	int pc_home_y;
	Xv_opaque result;
	int retstatus;
	int no_redisplay_item_state;
	XFontStruct *x_font_info;

	/* if a client has called panel_item_parent this item may not */
	/* have a parent -- do nothing in this case */
	if (panel == NULL) {
		return ((Xv_opaque) XV_ERROR);
	}

	if (*avlist == XV_END_CREATE) {
		create_menu(item_public);
		if ((dp->flags & PTXT_ALLOW_FILE_DROP) != 0) {
			xv_set(dp->drop_site,
					DROP_SITE_EVENT_MASK,
						DND_ENTERLEAVE | DND_ACCEPT_STRING | DND_ACCEPT_FILE,
					NULL);
		}

		return XV_OK;
	}

	/* Parse Panel Item Generic attributes before Text Field attributes.
	 * Prevent panel_redisplay_item from being called in item_set_avlist.
	 */
	no_redisplay_item_state = ip->panel->no_redisplay_item;
	ip->panel->no_redisplay_item = TRUE;
	result = xv_super_set_avlist(item_public, PANEL_TEXT, avlist);
	if (!no_redisplay_item_state)
		ip->panel->no_redisplay_item = FALSE;
	if (result != XV_OK)
		return result;

	for (; *avlist; avlist = attr_next(avlist)) {
		switch (avlist[0]) {

			case PANEL_VALUE:
				new_value = (char *)avlist[1];
				break;

			case PANEL_VALUE_FONT:
				x_font_info = (XFontStruct *) xv_get(ip->value_font, FONT_INFO);
				pc_home_y = -x_font_info->ascent;

				if (pc_home_y < dp->font_home)
					dp->font_home = pc_home_y;
				dp->font_home = -dp->font_home;
				if (!display_width_set)
					dp->display_width = panel_col_to_x(ip->value_font,
							dp->display_length);
				value_rect_changed = TRUE;
				break;

			case PANEL_VALUE_UNDERLINED:
				if ((int)avlist[1]) {
					dp->flags |= UNDERLINED;
				}
				else {
					dp->flags &= ~UNDERLINED;
				}
				break;
				/* laf end */

			case PANEL_NOTIFY_LEVEL:
				dp->notify_level = (Panel_setting) avlist[1];
				break;

			case PANEL_NOTIFY_STRING:
				if (dp->terminators)
					free(dp->terminators);
				dp->terminators = (char *)panel_strsave((char *)avlist[1]);
				break;

			case PANEL_VALUE_STORED_LENGTH:
				dp->stored_length = (int)avlist[1];
				dp->undo_buffer = (char *)realloc(dp->undo_buffer,
						(size_t) (dp->stored_length + 1));
				dp->value = (char *)realloc(dp->value,
						(size_t) (dp->stored_length + 1));
				if (dp->display_length >= dp->stored_length) {
					no_buttons = TRUE;
					value_rect_changed = TRUE;
				}
				else
					no_buttons = FALSE;
				break;

			case PANEL_VALUE_DISPLAY_LENGTH:
				dp->display_length = (int)avlist[1];
				dp->display_width = panel_col_to_x(ip->value_font,
						dp->display_length);

				if (dp->display_length >= dp->stored_length)
					no_buttons = TRUE;
				else
					no_buttons = FALSE;
				display_width_set = TRUE;
				value_rect_changed = TRUE;
				break;

			case PANEL_VALUE_DISPLAY_WIDTH:
				dp->display_width = (int)avlist[1];
				dp->display_length =
						panel_x_to_col(ip->value_font, dp->display_width);
				dp->display_width =
						panel_col_to_x(ip->value_font, dp->display_length);

				if (dp->display_length >= dp->stored_length)
					no_buttons = TRUE;
				else
					no_buttons = FALSE;
				display_width_set = TRUE;
				value_rect_changed = TRUE;
				break;

			case PANEL_MASK_CHAR:
				dp->mask = (char)avlist[1];
				break;

			case PANEL_INACTIVE:
				if ((int)avlist[1]
						&& panel->sel_holder[PANEL_SEL_PRIMARY] == ip) {
					/* Inactivating a text item:
					 * Lose the primary selection if the item owns it.
					 */
					xv_set(panel->sel_owner[PANEL_SEL_PRIMARY],
							SEL_OWN, FALSE,
							NULL);
				}
				break;

			case PANEL_ALLOW_FILE_DROP:
				if ((int)avlist[1]) {
					dp->flags |= PTXT_ALLOW_FILE_DROP;
				}
				else {
					dp->flags &= ~PTXT_ALLOW_FILE_DROP;
				}
				ATTR_CONSUME(*avlist);
				break;
				
			case PANEL_READ_ONLY:
				if ((int)avlist[1]) {
					dp->flags |= PTXT_READ_ONLY;
					ip->flags &= ~WANTS_KEY;
					if (panel->kbd_focus_item == ip) {
						/*
						 * Text item had caret: move caret to next text item, if
						 * any
						 */
						paint_caret(panel->kbd_focus_item, FALSE);
						panel->kbd_focus_item =
								panel_next_kbd_focus(panel, TRUE);
						if (panel->kbd_focus_item) {
							if (panel->kbd_focus_item->item_type ==
									PANEL_TEXT_ITEM) {
								paint_caret(panel->kbd_focus_item, TRUE);
							}
							else {
								panel_accept_kbd_focus(panel);
							}
						}
						/* Remove primary selection from item, if any */
						if (panel->sel_holder[PANEL_SEL_PRIMARY] == ip)
							xv_set(panel->sel_owner[PANEL_SEL_PRIMARY],
									SEL_OWN, FALSE,
									NULL);
					}

					/* Don't let item take drops while read-only! */
					xv_set(dp->drop_site,
								DROP_SITE_DELETE_REGION, &ip->value_rect,
								NULL);
				}
				else {
					dp->flags &= ~PTXT_READ_ONLY;
					ip->flags |= WANTS_KEY;

					/* want to take drops again */
					xv_set(dp->drop_site,
							DROP_SITE_REGION, &ip->value_rect,
							NULL);
				}
				break;

			case PANEL_TEXT_SELECT_LINE:
				select_line = TRUE;
				break;

			default:
				break;
		}
	}

	if (new_value) {
		strncpy(dp->value, new_value, (size_t)dp->stored_length);

		if (created(ip) && !hidden(ip) && panel->kbd_focus_item == ip)
			paint_caret(ip, FALSE);
		update_value_offset(ip, 0, 0, 1);
		SERVERTRACE((555, "=============== calling update_value\n"));
		update_value(ip, ACTION_LINE_END,	/* action */
				FALSE,	/* ok_to_insert */
				TRUE,	/* synthetic event */
				&retstatus, NULL, 0);
		if (created(ip) && !hidden(ip) && panel->kbd_focus_item == ip)
			paint_caret(ip, TRUE);
	}

	/*
	 * update the value & items rect if the width or height of the value has
	 * changed.
	 */
	if (value_rect_changed) {
		int chrht;

		ip->value_rect.r_width = dp->display_width;
		if ((ip->value_rect.r_width < 2 * dp->scroll_btn_width) && !no_buttons)
			ip->value_rect.r_width = 2 * dp->scroll_btn_width;
		chrht = xv_get(ip->value_font, FONT_DEFAULT_CHAR_HEIGHT);
		ip->value_rect.r_height = MAX(chrht + BOX_Y, dp->scroll_btn_height);
		ip->rect = panel_enclosing_rect(&ip->label_rect, &ip->value_rect);
		if ((dp->flags & PTXT_READ_ONLY) == 0) {
			xv_set(dp->drop_site,
					DROP_SITE_DELETE_REGION, NULL,
					DROP_SITE_REGION, &ip->value_rect,
					NULL);
		}
	}

	if (select_line && strlen(dp->value)) {
		struct timeval tv;
		Panel pan = PANEL_PUBLIC(panel);
		Xv_server srv = XV_SERVER_FROM_WINDOW(pan);

		/* Select line and position caret at the end of the line */
		dp->select_click_cnt[PANEL_SEL_PRIMARY] = 3;  /* fake a triple-click */
		dp->select_is_word[PANEL_SEL_PRIMARY] = FALSE;
		SERVERTRACE((444, "scc[%d] = %d\n", PANEL_SEL_PRIMARY,
								dp->select_click_cnt[PANEL_SEL_PRIMARY]));
		update_text_rect(ip);
		panel_select_line(ip, NULL, PANEL_SEL_PRIMARY);
		dp->delete_pending = TRUE;
		server_set_timestamp(srv, &tv, 0L);
		xv_set(panel->sel_owner[PANEL_SEL_PRIMARY], SEL_TIME, &tv, NULL);
		if (xv_set(panel->sel_owner[PANEL_SEL_PRIMARY], SEL_OWN, TRUE, NULL)
				== XV_OK) {
			if (panel->sel_holder[PANEL_SEL_PRIMARY])
				text_seln_dehighlight(panel->sel_holder[PANEL_SEL_PRIMARY],
						PANEL_SEL_PRIMARY);
			panel->sel_holder[PANEL_SEL_PRIMARY] = ip;
			text_seln_highlight(panel, ip, PANEL_SEL_PRIMARY);
			text_set_sel_data(panel->sel_item[PANEL_SEL_PRIMARY], dp, PANEL_SEL_PRIMARY);
		}
		update_caret_offset(ip, 0, 0);
		panel_set_kbd_focus(panel, ip);
	}

	return XV_OK;
}

static Xv_opaque text_get_attr(Panel_item item_public, int *status,
								Attr_attribute which_attr, va_list avlist)
{
	Text_info *dp = TEXT_PRIVATE(item_public);

	switch (which_attr) {
		case PANEL_VALUE:
			return (Xv_opaque) dp->value;

		case PANEL_VALUE_UNDERLINED:
			return (Xv_opaque) (dp->flags & UNDERLINED) ? TRUE : FALSE;

		case PANEL_VALUE_STORED_LENGTH:
			return (Xv_opaque) dp->stored_length;

		case PANEL_VALUE_DISPLAY_LENGTH:
			return (Xv_opaque) dp->display_length;

		case PANEL_VALUE_DISPLAY_WIDTH:
			return (Xv_opaque) dp->display_width;

		case PANEL_MASK_CHAR:
			return (Xv_opaque) dp->mask;

		case PANEL_NOTIFY_LEVEL:
			return (Xv_opaque) dp->notify_level;

		case PANEL_NOTIFY_STRING:
			return (Xv_opaque) dp->terminators;

		case PANEL_ALLOW_FILE_DROP:
			if (dp->flags & PTXT_ALLOW_FILE_DROP)
				return (Xv_opaque) TRUE;
			else
				return (Xv_opaque) FALSE;

		case PANEL_READ_ONLY:
			if (dp->flags & PTXT_READ_ONLY)
				return (Xv_opaque) TRUE;
			else
				return (Xv_opaque) FALSE;

		default:
			*status = XV_ERROR;
			return (Xv_opaque) 0;
	}
}

static int text_destroy(Panel_item item_public, Destroy_status  status)
{
    Text_info	   *dp = TEXT_PRIVATE(item_public);

    if ((status == DESTROY_CHECKING) || (status == DESTROY_SAVE_YOURSELF))
	return XV_OK;

    text_remove(item_public);

    xv_destroy(dp->dnd_item);
    xv_destroy(dp->dnd);
    xv_destroy(dp->drop_site);
    free(dp->undo_buffer);
    free(dp->value);
    free(dp->terminators);
    free((char *) dp);

    return XV_OK;
}

static void selection_insert(Text_info *dp, char *sel_string, long sel_length,
					Item_info *ip, Panel_info *panel, CHAR *sp,
					int insert_pos,
					int orig_len, int action, int *retstatus, int rank,
					int *caret_shift, int *val_shift)
{
	CHAR *sel_copy;	/* copy of selected string for pre-parsing */
	long i;
	int sel_length_copy = 0;
	int undo_index;


	/*
	 * HIT says any successive tab characters should be collapsed
	 * into a single space character (1074212).  To do so, use a copy
	 * buffer to save the trouble of shifting left.  otherwise, truncate
	 * before first non-printable character (e.g. NEWLINE).
	 */
	sel_copy = (CHAR *) calloc((size_t)sel_length, sizeof(CHAR));
	for (i = 0; i < sel_length; i++) {
		if (sel_string[i] == '\t') {
			sel_copy[sel_length_copy] = ' ';
			sel_length_copy++;
			while ((sel_string[i + 1] == '\t') && (i + 1 < sel_length))
				i++;	/* ignore successive tabs */
		}

		else if (!panel_printable_char((u_char) sel_string[i])) {
			break;
		}
		else {
			sel_copy[sel_length_copy] = sel_string[i];
			sel_length_copy++;
		}
	}	/* for() */

	xv_free(sel_string);

	sel_string = sel_copy;
	sel_length = sel_length_copy;

	SERVERTRACE((666, "in selection_insert: '%*.*s'\n",
							sel_length, sel_length, sel_string));

	/* Insure there is room in dp->value for the clipboard contents.
	 * Note: sel_length does not include the NULL terminator.
	 */

	sel_length = MIN(sel_length, dp->stored_length - orig_len);
	SERVERTRACE((666, "in selection_insert: sel_length = %d\n", sel_length));

	if ((sel_length <= 0) &&	/*1091198 */
			((action == ACTION_DRAG_COPY) ||
					(action == ACTION_DRAG_MOVE)
					|| (action == ACTION_PASTE))) {
		*retstatus |= DROP_OR_PASTE_FAILED;
		dp->dnd_sel_last = dp->dnd_sel_first = 0;
		xv_free(sel_string);
		SERVERTRACE((666, "nix wars\n"));
		return;
	}

	if (rank == PANEL_SEL_PRIMARY)
		dp->dnd_sel_last = insert_pos + sel_length - 1;
	/* Shift everything to the right (including the NULL terminator) */
	for (i = orig_len; i >= insert_pos; i--)
		sp[i + sel_length] = sp[i];
	/* Copy the characters from the selection to the text field and
	 * the Undo Buffer.
	 */
	for (undo_index = 0, i = insert_pos; undo_index < sel_length;
			undo_index++, i++) {
		sp[i] = sel_string[undo_index];

		dp->undo_buffer[undo_index] = sel_string[undo_index];
	}

	/* Null terminate the Undo Buffer */

	dp->undo_buffer[undo_index] = 0;

	SERVERTRACE((667, "sp = '%s'\n", sp));
	xv_free(sel_string);

	/* Set caret shift, value shift and undo direction */
	*caret_shift = sel_length;
	if (dp->first_char)
		*val_shift = sel_length;
	dp->undo_direction = DELETE;
	/* If this was a drag move, drag copy, or quick move, and we own the
	 * selection (i.e., we moved/copied text from and to the same text
	 * field), and the insert point was before the selection, then adjust
	 * sel_first and sel_last to account for the inserted characters.
	 */
	if ((action == ACTION_DRAG_MOVE || action == ACTION_DRAG_COPY ||
					action == ACTION_CUT) &&
			panel->sel_holder[rank] == ip
			&& insert_pos < dp->sel_first[rank]) {
		dp->sel_first[rank] += sel_length;
		dp->sel_last[rank] += sel_length;
	}
	SERVERTRACE((667, "END selection_insert\n"));
/* END selection_insert */
}

/* --------------------  Panel Item Operations  -------------------- */
static void text_handle_event(Panel_item item_public, Event *event)
{
	Text_info *dp = TEXT_PRIVATE(item_public);
	int format;
	Frame frame;
	Item_info *ip = ITEM_PRIVATE(item_public);
	long length;
	Xv_Notice notice;
	int own_primary_seln;
	Panel_info *panel = ip->panel;
	struct pr_size size;
	int take_down_caret;
	int retstatus;

	xv_translate_iso(event);
	SERVERTRACE((555, "text_handle_event\n"));
	take_down_caret =
			(event_is_down(event) || event_action(event) == ACTION_CANCEL ||
			!event_is_iso(event)) && event_action(event) != ACTION_MENU;

	if (take_down_caret && panel->kbd_focus_item) {
		/* turn caret off */
		paint_caret(panel->kbd_focus_item, FALSE);
	}

	update_text_rect(ip);

	if (event_action(event) == ACTION_DRAG_PREVIEW) {
		if (dp->flags & PTXT_READ_ONLY) {	/* no drops for read-only! */
			dnd_preview_reply(event, TRUE);
		}
		if (event_id(event) == LOC_WINENTER) {
			if (dp->flags & PTXT_ALLOW_FILE_DROP) {
				dnd_reject_unless(event,
							panel->atom.file_name,
							XA_STRING,
							NULL);
			}
			else {
				dnd_reject_unless(event, XA_STRING, NULL);
			}
		}
		return;
	}
	if ((event_action(event) == ACTION_DRAG_COPY
		|| event_action(event) == ACTION_DRAG_MOVE)
		&& !(dp->flags & PTXT_READ_ONLY)) {	/* no drops for read-only! */

		if (dnd_decode_drop(panel->sel_req, event) != XV_ERROR) {
			/* Set caret to position pointed to by cursor */
			dp->caret_offset = event_x(event) - dp->text_rect.r_left;
			if (dp->caret_offset < 0)
				/* Set caret offset to leftmost pixel */
				dp->caret_offset = 0;
			if (dp->caret_offset > dp->text_rect.r_width)
				/* Set caret_offset to rightmost pixel + 1 */
				dp->caret_offset = dp->text_rect.r_width;
			update_caret_offset(ip, 0, 0);
			if (panel->sel_holder[PANEL_SEL_PRIMARY] != ip
				|| dp->caret_position < dp->sel_first[PANEL_SEL_PRIMARY]
				|| dp->caret_position > dp->sel_last[PANEL_SEL_PRIMARY])
			{
				/* We haven't dropped on the selection itself:
				 * Insert the dragged selection contents into the text field.
				 * Otherwise, the drop is ignored.
				 */
				SERVERTRACE((555, "=============== calling update_value\n"));
				update_value(ip, event_action(event), TRUE, FALSE,
						&retstatus, NULL, 0);

				if (!(dp->flags & SELECTION_REQUEST_FAILED)
					|| (retstatus & DROP_OR_PASTE_FAILED))
				{
					if (event_action(event) == ACTION_DRAG_MOVE &&
							!(retstatus & DRAG_MOVE_FILENAME))
					{
						/* Post delete request back to owner */
						xv_set(panel->sel_req,
								SEL_TYPE, panel->atom.delete,
								NULL);
						xv_get(panel->sel_req, SEL_DATA, &length, &format);
					}
					else if (dp->flags & TEXT_SELECTED) {
						/* ACTION_DRAG_COPY to the same text field:
						 * Set the caret at the beginning of the
						 * primary selection.
						 */
						dp->caret_position = dp->sel_first[PANEL_SEL_PRIMARY];
						size = xv_pf_textwidth(dp->caret_position -
								dp->first_char, ip->value_font,
								&dp->value[dp->first_char]);
						dp->caret_offset = size.x;
					}
					/* dp->dnd_sel_first and dp->dnd_sel_last point to text
					 * that was dropped into text field.  Make this text the
					 * primary selection.
					 */
					own_primary_seln =
							xv_get(panel->sel_owner[PANEL_SEL_PRIMARY],SEL_OWN);
					if (own_primary_seln) {
						if (panel->sel_holder[PANEL_SEL_PRIMARY]) {
							if (panel->sel_holder[PANEL_SEL_PRIMARY] == ip)
								text_seln_dehighlight(ip, PANEL_SEL_PRIMARY);
							else
								text_lose_rank(panel, PANEL_SEL_PRIMARY);
						}
					}
					else
						own_primary_seln =
								xv_set(panel->sel_owner[PANEL_SEL_PRIMARY],
											SEL_TIME, &event_time(event),
											SEL_OWN, TRUE,
											NULL) == XV_OK;
					if (own_primary_seln) {
						panel->sel_holder[PANEL_SEL_PRIMARY] = ip;
						if (dp->dnd_sel_last > dp->last_char) {
							/* Scroll right so that the last character inserted
							 * is visible.
							 */
							do {
								update_value_offset(ip, 0, 1, 1);
								update_caret_offset(ip, -1, 0);
							} while (dp->dnd_sel_last > dp->last_char);
							paint_value(ip, PV_HIGHLIGHT);
						}
						dp->sel_first[PANEL_SEL_PRIMARY] =
								MAX(dp->first_char, dp->dnd_sel_first);
						dp->sel_last[PANEL_SEL_PRIMARY] = dp->dnd_sel_last;
						dp->flags |= TEXT_SELECTED;
						dp->delete_pending = TRUE;
						dp->select_click_cnt[PANEL_SEL_PRIMARY] = 1;
						dp->select_is_word[PANEL_SEL_PRIMARY] = FALSE;
						SERVERTRACE((444, "scc[%d] = %d\n", PANEL_SEL_PRIMARY,
									dp->select_click_cnt[PANEL_SEL_PRIMARY]));
						text_seln_highlight(panel, ip, PANEL_SEL_PRIMARY);
						text_set_sel_data(panel->sel_item[PANEL_SEL_PRIMARY], dp, PANEL_SEL_PRIMARY);
					}
				}
			}
			dnd_done(panel->sel_req);
			if (panel->kbd_focus_item != ip)
				panel_set_kbd_focus(panel, ip);
			if (!panel->status.has_input_focus)
				win_set_kbd_focus(PANEL_PUBLIC(panel),
						(XID) xv_get(event_window(event), XV_XID));
		}
		else {
			frame = xv_get(PANEL_PUBLIC(panel), WIN_FRAME);
			notice = xv_create(frame, NOTICE,
						NOTICE_MESSAGE_STRINGS,
							XV_MSG("Drag and Drop failed:"),
							XV_MSG("Unable to decode Drag and Drop message"),
							NULL,
						NOTICE_BUSY_FRAMES, frame, NULL,
						XV_SHOW, TRUE,
						NULL);
			xv_destroy(notice);
		}
		panel->current = NULL;
	}
	else {
		int do_default_handle_event = TRUE;

		/* handle "PRIMARY PASTE" */
		if (event_action(event) == ACTION_PASTE_PRIMARY) {
			do_default_handle_event = FALSE;

			if (event_is_down(event)) {
				unsigned long sel_length;
				int sel_format;
				char *sel_string;

				SERVERTRACE((666, "primary paste DOWN\n"));
				/* no idea how to do that properly - let's try.... */
				xv_set(panel->sel_req,
						SEL_RANK, XA_PRIMARY,
						SEL_TYPE, XA_STRING,
						NULL);
				sel_string = (char *)xv_get(panel->sel_req, SEL_DATA,
												&sel_length, &sel_format);
				if (sel_length != SEL_ERROR) {
					int orig_len, insert_pos;
					int caret_shift = 0;
					int val_shift = 0;
					Event fake_ev;

					SERVERTRACE((666, "got primary '%*.*s'\n",
							sel_length, sel_length, sel_string));

					/* try to set the caret: */
					fake_ev = *event;
					event_set_action(&fake_ev, ACTION_SELECT);
					/* set the caret position */
					text_begin_preview(item_public, &fake_ev);

					/* Get the insert position for character add/delete */
					if (dp->caret_offset == 0)
						insert_pos = dp->first_char;
					else
						insert_pos = dp->caret_position;

					orig_len = STRLEN(dp->value);

					selection_insert(dp, sel_string, (long)sel_length,
						ip, panel, dp->value, insert_pos, orig_len,
						ACTION_PASTE_PRIMARY, &retstatus, PANEL_SEL_PRIMARY,
						&caret_shift, &val_shift);

					/* up to here everything has been done (internally) -
					 * but you don't see it...
					 */
					panel_paint(item_public, PANEL_CLEAR);

					/* give up the selection */
					xv_set(panel->sel_req,
							SEL_TYPE, panel->atom.selection_end,
							NULL);
					sel_post_req_no_answer(panel->sel_req);
				}
			}
		}

		if (do_default_handle_event) {
			SERVERTRACE((555, "before panel_default_handle_event\n"));
			panel_default_handle_event(item_public, event);
			SERVERTRACE((555, "after  panel_default_handle_event\n"));
		}
	}

	/* Note: The panel item with the keyboard focus may have changed. */
	if (take_down_caret
		&& panel->kbd_focus_item
		&& panel->kbd_focus_item->item_type == PANEL_TEXT_ITEM)
	{
		/* turn text caret back on */
		paint_caret(panel->kbd_focus_item, TRUE);
	}
}


static Notify_value textitem_scroll_itimer_func(Panel_item item, int which)
{
    Text_info      *dp = TEXT_PRIVATE(item);
    Item_info      *ip = ITEM_PRIVATE(item);
    Panel_info     *panel = ip->panel;

    if (dp->flags & LEFT_SCROLL_BTN_SELECTED)
    {
        if (!dp->first_char)
        {
            dp->flags &= ~SELECTING_SCROLL_BTN;
            dp->flags &= ~LEFT_SCROLL_BTN_SELECTED;
            panel_autoscroll_stop_itimer( item );
            return( NOTIFY_DONE );
        }
        if (panel->kbd_focus_item)
            paint_caret(panel->kbd_focus_item, FALSE);
        horizontal_scroll(ip, -1);
        if (panel->kbd_focus_item)
            paint_caret(panel->kbd_focus_item, TRUE);
    }
    else if (dp->flags & RIGHT_SCROLL_BTN_SELECTED)
    {
        if (dp->last_char >= (int)strlen(dp->value) - 1) {
            dp->flags &= ~SELECTING_SCROLL_BTN;
            dp->flags &= ~RIGHT_SCROLL_BTN_SELECTED;
            panel_autoscroll_stop_itimer( item );
            return( NOTIFY_DONE );
        }
        if (panel->kbd_focus_item)
            paint_caret(panel->kbd_focus_item, FALSE);
        horizontal_scroll(ip, 1);
        if (panel->kbd_focus_item)
            paint_caret(panel->kbd_focus_item, TRUE);
    }

    return( NOTIFY_DONE );
} /* textitem_scroll_itimer_func */


/* is also used for update_preview */
static void text_begin_preview (Panel_item item_public, Event *event)
{
	Xv_Cursor accept_cursor, neutral_cursor, reject_cursor;
	u_char adjust_right;
	int caret_position;	/* index of character underneath
						 * the cursor */
	Cursor_drag_type cursor_drag_type;	/* CURSOR_MOVE or CURSOR_DUPLICATE */
	Text_info *dp = TEXT_PRIVATE(item_public);
	int dragging;
	char *error_msg = NULL;
	int event_offset;
	int ext_caret_offset;	/* new caret offset when adjusting to the right */
	Frame frame;
	Item_info *ip = ITEM_PRIVATE(item_public);
	int is_multiclick;
	Xv_Notice notice;
	Panel_info *panel = ip->panel;
	int save_caret_offset;	/* caret doesn't move in secondary
							 * selections */
	CHAR save_char;
	u_char save_delete_pending;
	int sel_rank;	/* selection rank: PANEL_SEL_PRIMARY or
					 * PANEL_SEL_SECONDARY */
	struct pr_size size;
	int status;
	int idx;
	int len;

	if ((event_action(event) == ACTION_SELECT)
			|| (event_action(event) == ACTION_ADJUST))
		paint_caret(ip, FALSE);

	dp->undo_direction = INVALID;

	if (event_is_quick_move(event) || event_is_quick_duplicate(event))
		sel_rank = PANEL_SEL_SECONDARY;
	else
		sel_rank = PANEL_SEL_PRIMARY;

	/*
	 * If primary selection and wiping thru text item, call
	 * text_add_selection() to invert the difference in the
	 * growing or shrinking selection.
	 */

	dragging = (strlen(dp->value) &&
			(dp->select_click_cnt[sel_rank] > 0) &&
			(sel_rank == PANEL_SEL_PRIMARY) &&
			(event_action(event) == LOC_DRAG) &&
			(action_select_is_down(event)) && (dp->flags & TEXT_SELECTED));

	/* Ask for kbd focus if this is a primary selection */
	if (sel_rank == PANEL_SEL_PRIMARY)
		win_set_kbd_focus(PANEL_PUBLIC(panel),
				(XID) xv_get(event_window(event), XV_XID));

	/* Check if one of the horizontal scrolling buttons was selected */
	event_offset = event_x(event) - ip->value_rect.r_left;
	if (dp->first_char &&
			event_offset >= 0 && event_offset < dp->scroll_btn_width) {
		if ((event_action(event) == ACTION_SELECT
			|| (event_action(event)==LOC_DRAG && action_select_is_down(event)))
			&& !(dp->flags & LEFT_SCROLL_BTN_SELECTED)) {
			panel_autoscroll_start_itimer(item_public,
					textitem_scroll_itimer_func);

			/* SELECT-down on left scrolling button: invoke left button */
			text_cancel_preview(item_public, event);
			draw_scroll_btn(ip,
					OLGX_SCROLL_BACKWARD | OLGX_INVOKED | OLGX_ERASE);
			dp->flags |= SELECTING_SCROLL_BTN | LEFT_SCROLL_BTN_SELECTED;
		}	/* else ignore event */
		return;
	}
	else if (event_offset > ip->value_rect.r_width -
			dp->scroll_btn_width && event_offset < ip->value_rect.r_width &&
			dp->last_char < (int)strlen(dp->value) - 1)
	{
		if ((event_action(event) == ACTION_SELECT
			|| (event_action(event)==LOC_DRAG && action_select_is_down(event)))
			&& !(dp->flags & RIGHT_SCROLL_BTN_SELECTED))
		{
			panel_autoscroll_start_itimer(item_public, textitem_scroll_itimer_func);
			/* SELECT-down on right scrolling button: invoke right button */
			text_cancel_preview(item_public, event);
			draw_scroll_btn(ip, OLGX_SCROLL_FORWARD | OLGX_INVOKED | OLGX_ERASE);
			dp->flags |= SELECTING_SCROLL_BTN | RIGHT_SCROLL_BTN_SELECTED;
		}  /* else ignore event */
		return;
	}

	/*
	 * If we started out selecting a scrolling button, then don't
	 * start selecting text.  Unhiglight any highlighted scrolling
	 * button and return.
	 */
	if (dp->flags & SELECTING_SCROLL_BTN) {
		if (dp->flags & LEFT_SCROLL_BTN_SELECTED) {
			dp->flags &= ~LEFT_SCROLL_BTN_SELECTED;
			panel_autoscroll_stop_itimer(item_public);
			draw_scroll_btn(ip, OLGX_SCROLL_BACKWARD | OLGX_ERASE);
		}
		else if (dp->flags & RIGHT_SCROLL_BTN_SELECTED) {
			dp->flags &= ~RIGHT_SCROLL_BTN_SELECTED;
			panel_autoscroll_stop_itimer(item_public);
			draw_scroll_btn(ip, OLGX_SCROLL_FORWARD | OLGX_ERASE);
		}
		return;
	}

	/*
	 * If nothing is selected on the line being pointed to, then
	 * translate an ADJUST or DRAG action to SELECT.
	 */
	save_delete_pending = dp->delete_pending;
	if ((dp->select_click_cnt[sel_rank] == 0 ||
					panel->sel_holder[sel_rank] != ip) &&
			(event_action(event) == ACTION_ADJUST ||
					event_action(event) == LOC_DRAG))
		event_set_action(event, ACTION_SELECT);

	/*
	 * Ignore the middle mouse button when the mouse is dragged to a line not
	 * containing the caret.
	 */
	if ((panel->kbd_focus_item != ip) &&
		((event_action(event) == LOC_DRAG)
		&& action_adjust_is_down(event))) {
		return;
	}

	/* The caret offset (position) does not change during secondary
	 * selections or drag and drop operations.
	 */
	save_caret_offset = dp->caret_offset;

	/*
	 * Define rectangle containing the text (i.e., value rect less arrows),
	 * and caret offset within that rectangle.
	 */
	update_text_rect(ip);

	/* Define caret_offset as offset within text rectangle */
	dp->caret_offset = event_x(event) - dp->text_rect.r_left;
	if (dp->caret_offset < 0)
		dp->caret_offset = 0;	/* => leftmost pixel */
	if (dp->caret_offset > dp->text_rect.r_width)
		dp->caret_offset = dp->text_rect.r_width;	/* => rightmost pixel + 1 */

	if (event_action(event) == ACTION_SELECT) {
		is_multiclick = panel_is_multiclick(panel, &dp->last_click_time,
				&event_time(event));
		if (!is_multiclick && dp->flags & TEXT_SELECTED &&
				sel_rank == PANEL_SEL_PRIMARY)
		{
			/* If SELECT-down occurred over highlighted text, then this is
			 * a possible Drag and Drop operation.
			 */
			caret_position = dp->first_char +
					char_position(dp->caret_offset, ip->value_font,
								&dp->value[dp->first_char], FALSE);

			if (caret_position >= dp->sel_first[PANEL_SEL_PRIMARY] &&
					caret_position <= dp->sel_last[PANEL_SEL_PRIMARY]) {
				dp->select_down_x = event_x(event);
				dp->select_down_y = event_y(event);
				panel->status.current_item_active = TRUE;
				dp->caret_offset = save_caret_offset;
				return;
			}
		}

		/*
		 * Set sel_first and sel_last to the character pointed to by the
		 * cursor.
		 */

		if (strlen(dp->value) == 0) {
			dp->sel_first[sel_rank] = 0;
			dp->sel_last[sel_rank] = -1;
		}
		else {
			save_char = dp->value[dp->last_char + 1];
			dp->value[dp->last_char + 1] = 0;
			dp->sel_first[sel_rank] = dp->first_char;
			dp->sel_first[sel_rank] += char_position(dp->caret_offset,
					ip->value_font, &dp->value[dp->first_char], TRUE);
			dp->value[dp->last_char + 1] = save_char;
			if (dp->sel_first[sel_rank] >= (int)strlen(dp->value)) {
				dp->sel_first[sel_rank] = xv_utf8_prev_char_offset(dp->value,
													(int)strlen(dp->value));
			}

			dp->sel_last[sel_rank] = dp->sel_first[sel_rank];
		}
		dp->select_click_cnt[sel_rank]++;
		dp->select_is_word[sel_rank] = (dp->select_click_cnt[sel_rank] == 2);
		SERVERTRACE((377, "%s: is_word[%d]=%d\n", __FUNCTION__,
				sel_rank, dp->select_is_word[sel_rank]));
		SERVERTRACE((444, "scc[%d] = %d\n", sel_rank,
									dp->select_click_cnt[sel_rank]));
		/*
		 * If this is not a double click, or we've moved to another panel
		 * item, then reset mouse left click count to 1.
		 */
		if (!is_multiclick ||
				(panel->sel_holder[sel_rank]
						&& panel->sel_holder[sel_rank] != ip))
		{
			dp->select_click_cnt[sel_rank] = 1;
			dp->select_is_word[sel_rank] = FALSE;
			SERVERTRACE((444, "scc[%d] = %d\n", sel_rank,
									dp->select_click_cnt[sel_rank]));
		}
		dp->last_click_time = event_time(event);
		dp->flags &= ~TEXT_SELECTED;	/* assume we're not selecting text */
		if (dp->select_click_cnt[sel_rank] > 1) {
			/* Double or triple click */
			if (!event_ctrl_is_down(event)) {
				if ((int)strlen(dp->value) > 0) dp->flags |= TEXT_SELECTED;
			}
			(void)panel_multiclick_handler(ip, event, sel_rank);
		}
		else if (event_ctrl_is_down(event))
			/* Single click with ctrl */
			dp->flags |= TEXT_SELECTED;
	}
	else if (strlen(dp->value) && (dp->select_click_cnt[sel_rank] > 0) &&
		(event_action(event) == ACTION_ADJUST || (event_action(event) == LOC_DRAG &&
		(action_select_is_down(event) || action_adjust_is_down(event)))))
	{

		if (event_action(event) == LOC_DRAG &&
				action_select_is_down(event) &&
				panel->status.current_item_active) {
			dp->caret_offset = save_caret_offset;
			if (abs(event_x(event) - dp->select_down_x) >=
					panel->drag_threshold ||
					abs(event_y(event) - dp->select_down_y) >=
					panel->drag_threshold)
			{
				int end_byte_offset, last_char_len = 1;

				/* We've dragged the cursor past the drag threshold:
				 * Initiate a Drag and Drop operation.
				 * Note that we set current_item_active FALSE before doing
				 * the Drag and Drop.  This lets the ACTION_DRAG_{COPY,MOVE}
				 * event to be sent to the panel item under the pointer in
				 * panel_default_event.  Also in panel_default_event, we don't
				 * call panel_cancel if the event is ACTION_DRAG_COPY,
				 * ACTION_DRAG_MOVE or ACTION_DRAG_PREVIEW.  This prevents the
				 * primary selection from being cancelled.
				 */
				panel->status.current_item_active = FALSE;
				if (panel_duplicate_key_is_down(panel, event)) {
					cursor_drag_type = CURSOR_DUPLICATE;
					xv_set(dp->dnd, DND_TYPE, DND_COPY, NULL);
				}
				else {
					cursor_drag_type = CURSOR_MOVE;
					xv_set(dp->dnd, DND_TYPE, DND_MOVE, NULL);
				}

				if (_xv_is_multibyte) {
				    int mbl = mblen(dp->value +dp->sel_last[PANEL_SEL_PRIMARY],
											MB_CUR_MAX);
					if (mbl > 0) {
						last_char_len = mbl;
					}
				}
				end_byte_offset = dp->sel_last[PANEL_SEL_PRIMARY]
									+ last_char_len;

				save_char = dp->value[end_byte_offset];
				dp->value[end_byte_offset] = '\0';

				neutral_cursor = xv_create(PANEL_PUBLIC(panel), CURSOR,
						CURSOR_STRING,
							dp->value+dp->sel_first[PANEL_SEL_PRIMARY],
						CURSOR_DRAG_STATE, CURSOR_NEUTRAL,
						CURSOR_DRAG_TYPE, cursor_drag_type,
						NULL);
				if (neutral_cursor)
					xv_set(dp->dnd, DND_CURSOR, neutral_cursor, NULL);

				accept_cursor = xv_create(PANEL_PUBLIC(panel), CURSOR,
						CURSOR_STRING,
							dp->value+dp->sel_first[PANEL_SEL_PRIMARY],
						CURSOR_DRAG_STATE, CURSOR_ACCEPT,
						CURSOR_DRAG_TYPE, cursor_drag_type,
						NULL);

				reject_cursor = xv_create(PANEL_PUBLIC(panel), CURSOR,
						CURSOR_STRING,
							dp->value+dp->sel_first[PANEL_SEL_PRIMARY],
						CURSOR_DRAG_STATE, CURSOR_REJECT,
						CURSOR_DRAG_TYPE, cursor_drag_type,
						NULL);

				if (accept_cursor)
					xv_set(dp->dnd, DND_ACCEPT_CURSOR, accept_cursor, NULL);

				if (reject_cursor)
					xv_set(dp->dnd, DND_REJECT_CURSOR, reject_cursor, NULL);

				dp->value[end_byte_offset] = save_char;

				text_set_sel_data(dp->dnd_item, dp, PANEL_SEL_PRIMARY);

				/* supply sel_owner and sel_holder for DND -
				 * they will be removed in dnd_lose_proc
				 */
    			panel->sel_owner[PANEL_SEL_DND] = dp->dnd;
    			panel->sel_holder[PANEL_SEL_DND] = ip;
    			dp->sel_first[PANEL_SEL_DND] = dp->sel_first[PANEL_SEL_PRIMARY];
    			dp->sel_last[PANEL_SEL_DND] = dp->sel_last[PANEL_SEL_PRIMARY];

				xv_set(dp->dnd, SEL_TIME, &event_time(event), NULL);
				status = dnd_send_drop(dp->dnd);

				if (neutral_cursor) xv_destroy(neutral_cursor);
				if (accept_cursor) xv_destroy(accept_cursor);
				if (reject_cursor) xv_destroy(reject_cursor);

				switch (status) {
					case XV_OK:
						return;

					case DND_ABORTED:
						error_msg = NULL;
						break;

					case DND_TIMEOUT:
						error_msg = XV_MSG("Selection timed out");
						break;

					case DND_ILLEGAL_TARGET:
						error_msg = XV_MSG("Illegal drop target");
						break;

					case DND_SELECTION:
						error_msg = XV_MSG("Unable to acquire selection");
						break;
					case DND_ROOT:
						error_msg =
								XV_MSG
								("Root window is not a valid drop target");
						break;

					case XV_ERROR:
						error_msg = XV_MSG("unexpected internal error");
						break;
				}
				if (error_msg) {
					frame = xv_get(PANEL_PUBLIC(panel), WIN_FRAME);
					notice = xv_create(frame, NOTICE,
							NOTICE_MESSAGE_STRINGS,
								XV_MSG("Drag and Drop failed:"),
								error_msg,
								NULL,
							NOTICE_BUSY_FRAMES, frame, NULL,
							XV_SHOW, TRUE,
							NULL);
					xv_destroy(notice);
				}
				panel->current = NULL;
			}
			return;
		}

		idx = char_position(dp->caret_offset, ip->value_font,
				&dp->value[dp->first_char], FALSE);
		len = strlen(dp->value);

		if ((len == dp->caret_position) && (idx >= len) &&
				(event_action(event) == ACTION_ADJUST));
		else if ((len == dp->caret_position) &&
				(event_action(event) == LOC_DRAG) &&
				(idx >= len) &&
				(action_select_is_down(event) || action_adjust_is_down(event)));
		else {	/* define extended selection point */
			/* Define extended selection point.
			 * Get the index of the character that the caret_offset is
			 * within.  Do not use the "balance beam" method.
			 */

			dp->ext_first = char_position(dp->caret_offset,
					ip->value_font, &dp->value[dp->first_char], FALSE);
			if (dp->first_char)
				dp->ext_first += dp->first_char;
			if (dp->ext_first >= (int)strlen(dp->value))
				if ((dp->ext_first = strlen(dp->value) - 1) < 0)
					dp->ext_first = 0;

			dp->ext_last = dp->ext_first;

			if (dp->select_click_cnt[sel_rank] >= 3) {
				dp->ext_first = dp->sel_first[sel_rank];
				dp->ext_last = dp->sel_last[sel_rank];
				panel_select_line(ip, event, sel_rank);	/* update caret offset */
			}
			else {
				int ext_last_len;

				if (dp->select_click_cnt[sel_rank] == 2) {
					panel_find_word(dp, &dp->ext_first, &dp->ext_last);
				}

				ext_last_len = xv_utf8_next_char_len(dp->value, dp->ext_last);
				size = xv_pf_textwidth(dp->ext_last - dp->first_char
													+ ext_last_len,
								ip->value_font, &dp->value[dp->first_char]);

				ext_caret_offset = size.x;

				/* Adjust first or last character selected */
				if (dp->ext_first == dp->sel_first[sel_rank]
					&& dp->ext_last == dp->sel_last[sel_rank])
				{
				    /* do nothing, avoiding a modification of caret_offset */
				}
				else if (dp->ext_last > dp->sel_last[sel_rank])
					adjust_right = TRUE;
				else if (dp->ext_first < dp->sel_first[sel_rank])
					adjust_right = FALSE;	/* adjust left */
				else if ((dp->ext_first - dp->sel_first[sel_rank]) <
						(dp->sel_last[sel_rank] - dp->ext_first))
					adjust_right = FALSE;	/* adjust left */
				else
					adjust_right = TRUE;
				
				if (dp->ext_first != dp->sel_first[sel_rank]
					|| dp->ext_last != dp->sel_last[sel_rank])
				{
					if (adjust_right) {	/* Note: caret must be to the right of
										 * the last selected character, due to
										 * a check made on this assumption in
										 * text_seln_delete().
										 */
						int aligned_ext_last = xv_utf8_align_left(dp->value, 
														dp->ext_last);
						dp->sel_last[sel_rank] = aligned_ext_last;
						dp->caret_offset = ext_caret_offset;
					}
					else {	/* adjust left */
						if (dp->sel_last[sel_rank] == dp->sel_first[sel_rank] &&
								(dp->flags & TEXT_SELECTED) == 0) {
							/* First drag after SELECT-down: don't include
							 * character to the right of the caret.
							 */
							dp->sel_last[sel_rank] =xv_utf8_prev_char_offset(dp->value,
															dp->caret_position);
						}
						dp->sel_first[sel_rank] = dp->ext_first;
	
						size = xv_pf_textwidth(dp->ext_first - dp->first_char,
								ip->value_font, &dp->value[dp->first_char]);
						dp->caret_offset = size.x;
					}
				}
			}
			/*
			 * ADJUST or dragging SELECT: select text
			 */
			dp->flags |= TEXT_SELECTED;
		}	/* define extended selection point */
	}

	dp->delete_pending = (sel_rank == PANEL_SEL_PRIMARY) &&
			(dp->flags & TEXT_SELECTED);
	update_caret_offset(ip, 0, 0);

	/* If we're selecting text, and there is text to be selected,
	 * then highlight the selected text and set the selection data.
	 */
	if (dragging) {
		text_add_selection(panel, ip, save_caret_offset);

		text_set_sel_data(panel->sel_item[sel_rank], dp, sel_rank);
	}
	else {
		if (panel->sel_holder[sel_rank]) {
			if (panel->sel_holder[sel_rank] == ip)
				text_seln_dehighlight(ip, sel_rank);
			else
				text_lose_rank(panel, sel_rank);
		}

		if ((dp->flags & TEXT_SELECTED) && (int)strlen(dp->value) > 0) {
			text_seln_highlight(panel, ip, sel_rank);

			text_set_sel_data(panel->sel_item[sel_rank], dp, sel_rank);
		}
		else {
			xv_set(panel->sel_item[sel_rank],
					SEL_DATA, NULL,
					SEL_LENGTH, 0,
					NULL);
		}
	}

	/* Make this item the selection.  Acquire the {PRIMARY,SECONDARY}
	 * selection for this ip.
	 */
	if (xv_set(panel->sel_owner[sel_rank],
							SEL_TIME, &event_time(event),
							SEL_OWN, TRUE,
							NULL) == XV_OK) {
		panel->sel_holder[sel_rank] = ip;
	}

	/*
	 * If we are getting the SECONDARY selection, restore dp->caret_offset
	 * since the insertion point (represented by the caret) doesn't move during
	 * secondary selections, and restore dp->delete_pending, since it only
	 * applies to primary selections.
	 */
	if (sel_rank == PANEL_SEL_SECONDARY) {
		dp->caret_offset = save_caret_offset;
		dp->delete_pending = save_delete_pending;
	}
	dp->flags |= SELECTING_ITEM;
	if (!(dp->flags & PTXT_READ_ONLY) && sel_rank == PANEL_SEL_PRIMARY)
		panel_set_kbd_focus(panel, ip);
}


static void text_cancel_preview(Panel_item item_public, Event *event)
{
	Text_info *dp = TEXT_PRIVATE(item_public);
	Item_info *ip = ITEM_PRIVATE(item_public);
	Panel_info *panel = ip->panel;
	int sel_rank;

	if (dp->flags & SELECTING_SCROLL_BTN) {
		dp->flags &= ~SELECTING_SCROLL_BTN;
		if (dp->flags & LEFT_SCROLL_BTN_SELECTED) {
			dp->flags &= ~LEFT_SCROLL_BTN_SELECTED;
			panel_autoscroll_stop_itimer(item_public);
			draw_scroll_btn(ip, OLGX_SCROLL_BACKWARD | OLGX_ERASE);
		}
		else if (dp->flags & RIGHT_SCROLL_BTN_SELECTED) {
			dp->flags &= ~RIGHT_SCROLL_BTN_SELECTED;
			panel_autoscroll_stop_itimer(item_public);
			draw_scroll_btn(ip, OLGX_SCROLL_FORWARD | OLGX_ERASE);
		}
	}
	if (dp->flags & SELECTING_ITEM) {
		panel_yield_kbd_focus(panel);
		dp->flags &= ~SELECTING_ITEM;
	}
	if (panel->status.current_item_active) {
		panel->status.current_item_active = FALSE;
	}
	else {
		for (sel_rank = PANEL_SEL_PRIMARY;
				sel_rank <= PANEL_SEL_SECONDARY; sel_rank++) {
			if (panel->sel_holder[sel_rank] == ip) {
				xv_set(panel->sel_owner[sel_rank], SEL_OWN, FALSE, NULL);
			}
		}
	}
}


static void text_accept_preview(Panel_item item_public, Event *event)
{
	Text_info *dp = TEXT_PRIVATE(item_public);
	int event_offset;
	Item_info *ip = ITEM_PRIVATE(item_public);

	/* If SELECT-down occurred over a primary selection, but the SELECT-up
	 * occurred before the mouse was dragged drag_threshold pixels, then
	 * interpret the SELECT-up as a single click which cancels the selection
	 * and sets the caret position.
	 */
	if (ip->panel->status.current_item_active) {
		ip->panel->status.current_item_active = FALSE;
		text_lose_rank(ip->panel, PANEL_SEL_PRIMARY); /* cancel the selection */
		event_set_down(event);	/* event = SELECT-down */
		text_begin_preview(item_public, event);	/* set the caret position */
		return;
	}

	/* Check if one of the horizontal scrolling buttons was selected */
	dp->flags &= ~SELECTING_SCROLL_BTN;
	event_offset = event_x(event) - ip->value_rect.r_left;
	if (event_action(event) == ACTION_SELECT) {
		if (dp->first_char &&
				event_offset >= 0 && event_offset < dp->scroll_btn_width) {
			/* User clicked on left scrolling button */
			dp->flags &= ~LEFT_SCROLL_BTN_SELECTED;
			panel_autoscroll_stop_itimer(item_public);
			horizontal_scroll(ip, -1);	/* scroll left */
		}
		else if (event_offset > ip->value_rect.r_width -
				dp->scroll_btn_width &&
				event_offset < ip->value_rect.r_width &&
				dp->last_char < (int)strlen(dp->value) - 1)
		{
			/* User clicked on right scrolling button */
			dp->flags &= ~RIGHT_SCROLL_BTN_SELECTED;
			panel_autoscroll_stop_itimer(item_public);
			horizontal_scroll(ip, 1);	/* scroll right */
		}
	}

	if (!(dp->flags & SELECTING_ITEM))
		return;

	dp->flags &= ~SELECTING_ITEM;
	update_caret_offset(ip, 0, 0);

}


static void note_menu_undo(Menu menu, Menu_item mitem)
{
	Panel_item self = (Panel_item)xv_get(menu, XV_KEY_DATA, item_key);
	Item_info *ip = ITEM_PRIVATE(self);
	int retstatus;

	update_value(ip, ACTION_UNDO, TRUE, FALSE, &retstatus, NULL, 0);
}

static void note_menu_cut(Menu menu, Menu_item mitem)
{
	Panel_item self = (Panel_item)xv_get(menu, XV_KEY_DATA, item_key);
	Item_info *ip = ITEM_PRIVATE(self);
	Text_info *dp = TEXT_PRIVATE(self);
	Panel_info *panel = ip->panel;

	text_set_clipboard(panel, ip, dp);
	/* Delete the selected text */
	text_seln_delete(ip, PANEL_SEL_PRIMARY);
}

static void note_menu_copy(Menu menu, Menu_item mitem)
{
	Panel_item self = (Panel_item)xv_get(menu, XV_KEY_DATA, item_key);
	Item_info *ip = ITEM_PRIVATE(self);
	Text_info *dp = TEXT_PRIVATE(self);
	Panel_info *panel = ip->panel;

	text_set_clipboard(panel, ip, dp);
}

static void note_menu_paste(Menu menu, Menu_item mitem)
{
	Panel_item self = (Panel_item)xv_get(menu, XV_KEY_DATA, item_key);
	Item_info *ip = ITEM_PRIVATE(self);
	int retstatus;

	update_value(ip, ACTION_PASTE, TRUE, FALSE, &retstatus, NULL, 0);
}

static void note_menu_delete(Menu menu, Menu_item mitem)
{
	Panel_item self = (Panel_item)xv_get(menu, XV_KEY_DATA, item_key);
	Item_info *ip = ITEM_PRIVATE(self);

	text_seln_delete(ip, PANEL_SEL_PRIMARY);
}

static void create_menu(Panel_item item)
{
    Item_info *ip = ITEM_PRIVATE(item);
	Text_info *dp = TEXT_PRIVATE(item);
	char *itemname, menuname[200];
	Panel pan = PANEL_PUBLIC(ip->panel);
	Xv_server srv = XV_SERVER_FROM_WINDOW(pan);

	itemname = (char *)xv_get(item, XV_INSTANCE_NAME);
	if (! itemname) {
		char *hlp = (char *)xv_get(item, XV_HELP_DATA);

		if (hlp) {
			itemname = strchr(hlp, ':');
			if (itemname) ++itemname;
		}
	}
	sprintf(menuname, "%sEditMenu", itemname ? itemname:"PanelText");

	dp->cutitem = xv_create(XV_NULL, MENUITEM,
					MENU_RELEASE,
					MENU_STRING, XV_MSG("Cut"),
					MENU_NOTIFY_PROC, note_menu_cut,
					XV_HELP_DATA, "xview:ptxtmenucut",
					NULL);
	dp->copyitem = xv_create(XV_NULL, MENUITEM,
					MENU_RELEASE,
					MENU_STRING, XV_MSG("Copy"),
					MENU_NOTIFY_PROC, note_menu_copy,
					XV_HELP_DATA, "xview:ptxtmenucopy",
					NULL);
	dp->deleteitem = xv_create(XV_NULL, MENUITEM,
					MENU_RELEASE,
					MENU_STRING, XV_MSG("Delete"),
					MENU_NOTIFY_PROC, note_menu_delete,
					XV_HELP_DATA, "xview:ptxtmenudelete",
					NULL);

	xv_set(item, PANEL_ITEM_MENU, xv_create(srv, MENU,
						XV_INSTANCE_NAME, menuname,
						MENU_TITLE_ITEM, XV_MSG("Edit"),
						XV_KEY_DATA, item_key, item,
						XV_HELP_DATA, "xview:ptxteditmenu",
						MENU_ITEM,
							MENU_STRING, XV_MSG("Undo"),
							MENU_NOTIFY_PROC, note_menu_undo,
							XV_HELP_DATA, "xview:ptxtmenuundo",
							0,
						MENU_APPEND_ITEM, dp->cutitem,
						MENU_APPEND_ITEM, dp->copyitem,
						MENU_ITEM,
							MENU_STRING, XV_MSG("Paste"),
							MENU_NOTIFY_PROC, note_menu_paste,
							XV_HELP_DATA, "xview:ptxtmenupaste",
							0,
						MENU_APPEND_ITEM, dp->deleteitem,
						XV_SET_MENU, pan,
						NULL),
					NULL);
}

static void text_accept_menu(Panel_item item, Event *ev)
{
    Item_info *ip = ITEM_PRIVATE(item);
	Text_info *dp = TEXT_PRIVATE(item);

	if (dp->flags & TEXT_SELECTED) {
		xv_set(dp->cutitem, MENU_INACTIVE, FALSE, NULL);
		xv_set(dp->copyitem, MENU_INACTIVE, FALSE, NULL);
		xv_set(dp->deleteitem, MENU_INACTIVE, FALSE, NULL);
	}
	else {
		xv_set(dp->cutitem, MENU_INACTIVE, TRUE, NULL);
		xv_set(dp->copyitem, MENU_INACTIVE, TRUE, NULL);
		xv_set(dp->deleteitem, MENU_INACTIVE, TRUE, NULL);
	}

	menu_show(ip->menu, event_window(ev), ev, NULL);
}

static void text_accept_key(Panel_item item_public, register Event *event)
{
	Text_info *dp = TEXT_PRIVATE(item_public);
	int has_caret;
	Item_info *ip = ITEM_PRIVATE(item_public);
	int notify_desired = FALSE;
	Panel_setting notify_rtn_code;
	int ok_to_insert;
	Panel_info *panel = ip->panel;
	int retstatus;

	SERVERTRACE((555, "text_accept_key\n"));

	xv_translate_iso(event);
	if (panel_printable_char(event_action(event))
		|| panel_erase_action(event)
		|| panel_navigation_action(event)
		|| (event_is_up(event) &&
				(event_action(event) == ACTION_CUT
				|| event_action(event) == ACTION_PASTE
				|| event_action(event) == ACTION_CANCEL)))
	{
		if (dp->delete_pending) {
			if (event_action(event) == ACTION_ERASE_CHAR_BACKWARD ||
				event_action(event) == ACTION_ERASE_CHAR_FORWARD)
			{
				if ((int)strlen(dp->value) > 0 &&
						dp->select_click_cnt[PANEL_SEL_PRIMARY] != 0)
				{
					text_seln_delete(ip, PANEL_SEL_PRIMARY);
					if (dp->notify_level == PANEL_NON_PRINTABLE ||
							dp->notify_level == PANEL_ALL) {
						event_set_action(event, ACTION_DELETE_SELECTION);
						(void)(*ip->notify) (ITEM_PUBLIC(ip), event);
					}
					return;
				}
			}
			else if (panel_erase_action(event) ||
					panel_navigation_action(event) ||
					event_action(event) == ACTION_CANCEL) {
				text_seln_dehighlight(ip, PANEL_SEL_PRIMARY);
				dp->delete_pending = FALSE;
			}
			else if ((int)strlen(dp->value) > 0 &&
					dp->select_click_cnt[PANEL_SEL_PRIMARY] > 0) {
				if (event_action(event) == ACTION_CUT) {
					/* Copy the primary selection data to the clipboard */
					text_set_clipboard(panel, ip, dp);
				}
				/* Delete the selected text */
				text_seln_delete(ip, PANEL_SEL_PRIMARY);
			}
		}
		dp->select_click_cnt[PANEL_SEL_PRIMARY] = 0;
		dp->select_is_word[PANEL_SEL_PRIMARY] = FALSE;
		SERVERTRACE((444, "scc[%d] = %d\n", PANEL_SEL_PRIMARY,
						dp->select_click_cnt[PANEL_SEL_PRIMARY]));
	}
	if (dp->flags & PTXT_READ_ONLY)
		return;

	dp->flags &= ~TEXT_SELECTED;

	if (panel_event_is_xview_semantic(event)) {
		switch (event_action(event)) {
			case ACTION_COPY:
				if (event_is_up(event))
					/* Copy the primary selection data to the clipboard */
					text_set_clipboard(panel, ip, dp);
				return;
			case ACTION_CUT:
			case ACTION_PASTE:

				if (event_is_down(event)) {
					/* now the whole system is in "quick mode" */
					/* try to remove the caret: */
					/* but this seems to be a big secret... */
/* no effect			paint_caret(ip, FALSE); */
/* no effect			panel_text_caret_on(panel, FALSE); */
/* no effect   			if (ip->panel->status.painted) paint_caret(ip, FALSE); */
					return;
				}
				break;
			case ACTION_UNDO:
			case ACTION_PROPS:	/* DRA_CHANGED */
			case ACTION_FIND_BACKWARD:	/* DRA_CHANGED */
			case ACTION_FIND_FORWARD:	/* DRA_CHANGED */
				break;
			default:
				return;
		}
	}

	switch (dp->notify_level) {
		case PANEL_ALL:
			notify_desired = TRUE;
			break;
		case PANEL_SPECIFIED:
			notify_desired = (event_action(event) <= ISO_LAST &&
					(strchr(dp->terminators, event_action(event)) != 0) &&
					event_is_down(event));
			break;
		case PANEL_NON_PRINTABLE:
			notify_desired = !panel_printable_char(event_action(event));
			break;
		case PANEL_NONE:
			notify_desired = FALSE;
			break;
		default:
			break;
	}
	if (notify_desired) {
		notify_rtn_code = (Panel_setting)
				(*ip->notify) (ITEM_PUBLIC(ip), event);
	}
	else {
		notify_rtn_code = panel_text_notify(ITEM_PUBLIC(ip), event);
	}

	if (event_is_up(event) &&
			notify_rtn_code != PANEL_NEXT &&
			notify_rtn_code != PANEL_PREVIOUS &&
			event_action(event) != ACTION_PASTE &&
			event_action(event) != ACTION_CUT)
		/* Primary CUT is handled above under "if (dp->delete_pending)".
		 * Primary PASTE, Secondary PASTE (i.e., Quick Copy) and Secondary CUT
		 * (i.e., Quick Move) are handled in update_value().  update_value()
		 * will ignore Primary CUT.  So, both PASTE-up and CUT-up events are
		 * passed on here.
		 */
		return;

	ok_to_insert = notify_rtn_code == PANEL_INSERT;

	/* Process movement actions that could change lines */
	switch (event_action(event)) {
			/* Note: ACTION_GO_LINE_FORWARD is a SunView1 compatibility action */
		case ACTION_GO_LINE_FORWARD:	/* Go to the start of the next line */
			notify_rtn_code = PANEL_NEXT;
			ok_to_insert = FALSE;
			break;
		case ACTION_GO_CHAR_FORWARD:	/* right arrow */
		case ACTION_GO_CHAR_BACKWARD:	/* left arrow */
			ok_to_insert = FALSE;
			break;
		default:
			break;
	}

	/* If this item has the caret, then turn off the caret.
	 * Note: has_caret is calculated after the text item's notify proc is
	 *       called since the notify proc may change who has the caret.
	 */
	has_caret = panel->status.has_input_focus && panel->kbd_focus_item == ip;

	/*
	 * Do something with the character.  We must turn off the caret
	 * before moving the caret_offset.
	 */
	if (has_caret)
		paint_caret(ip, FALSE);

	SERVERTRACE((555, "=============== calling update_value\n"));
	update_value(ip, event_action(event), ok_to_insert, FALSE,
			&retstatus, event_string(event), event_flags(event));

	if (has_caret)
		paint_caret(ip, TRUE);

	if (has_caret) {
		switch (notify_rtn_code) {
			case PANEL_NEXT:
				(void)panel_advance_caret(PANEL_PUBLIC(panel));
				break;

			case PANEL_PREVIOUS:
				(void)panel_backup_caret(PANEL_PUBLIC(panel));
				break;

			default:
				break;
		}
	}
}


static void text_clear( Panel_item item_public)
{
    Text_info	   *dp = TEXT_PRIVATE(item_public);
    Item_info      *ip = ITEM_PRIVATE(item_public);

    if (ip->panel->kbd_focus_item == ip)
    {
        paint_caret(ip, FALSE);
	ip->panel->caret_on = FALSE;
    }
    panel_default_clear_item(item_public);
    if (dp->flags & TEXT_HIGHLIGHTED) {
	sel_highlight = HL_NONE;
	dp->flags &= ~TEXT_HIGHLIGHTED;
    }
}


static void text_paint(Panel_item item_public, Panel_setting u)
{
    Item_info      *ip = ITEM_PRIVATE(item_public);

    panel_text_paint_label(ip);
	paint_text(ip);
}


static void text_remove(Panel_item item_public)
{
	Text_info *dp = TEXT_PRIVATE(item_public);
	int i;
	Item_info *ip = ITEM_PRIVATE(item_public);
	Panel_info *panel = ip->panel;

	if (dp->flags & PTXT_READ_ONLY)
		return;

	/* Cancel any 'static' selections this item owns (not dnd). */
	for (i = 0; i < PANEL_SEL_DND; i++) {
		if (panel->sel_holder[i] == ip)
			xv_set(panel->sel_owner[i], SEL_OWN, FALSE, NULL);
	}

	/*
	 * Only reassign the keyboard focus to another item if the panel isn't
	 * being destroyed.
	 */
	if (!panel->status.destroying) {
		if (panel->kbd_focus_item == ip) {
			/*
			 * The caret is cleared, so don't paint the caret off.
			 * However, mark that no caret is on.
			 */
			panel->caret_on = FALSE;
			if (panel->primary_focus_item == ip)
				panel->primary_focus_item = NULL;
			panel->kbd_focus_item = panel_next_kbd_focus(panel, TRUE);
			if (panel->kbd_focus_item) {
				if (panel->kbd_focus_item->item_type == PANEL_TEXT_ITEM) {
					paint_caret(panel->kbd_focus_item, TRUE);
				}
				else {
					panel_accept_kbd_focus(panel);
				}
			}
		}
		/*
		 * The primary_focus_item can be different than the
		 * kbd_focus_item. Therefore, ifwe delete the
		 * primary_focus_item, then we've got to set this field
		 * to NULL - fixes bug 1077902
		 */
		if (panel->primary_focus_item == ip)
			panel->primary_focus_item = NULL;
	}
	return;
}


/*
 * text_restore
 *
 * Note: This code assumes that the caller has already set ip to be
 *	 not hidden.
 */
static void text_restore(Panel_item item_public, Panel_setting u)
{
    Text_info	   *dp = TEXT_PRIVATE(item_public);
    Item_info      *ip = ITEM_PRIVATE(item_public);

    if (dp->flags & PTXT_READ_ONLY || hidden(ip))
	return;

    /* see whether selection client is initialized */
    /* may not be if restore is called because of reparent */
    if (!ip->panel->sel_owner[PANEL_SEL_PRIMARY])
	text_seln_init(ip->panel);

    /* If this is the only item, give the caret to this item. */
    if (ip->panel->items == ip && !ip->next) {
	ip->panel->kbd_focus_item = ip;
    }
    return;
}


static void text_layout(Panel_item item_public, Rect *deltas)
{
    Text_info	   *dp = TEXT_PRIVATE(item_public);
    Item_info      *ip = ITEM_PRIVATE(item_public);

	if ((dp->flags & PTXT_READ_ONLY) == 0) {
    	xv_set(dp->drop_site,
						DROP_SITE_DELETE_REGION, NULL,
						DROP_SITE_REGION, &ip->value_rect,
						NULL);
	}
}


static void text_accept_kbd_focus(Panel_item item_public)
{
    Text_info	   *dp = TEXT_PRIVATE(item_public);
    Item_info      *ip = ITEM_PRIVATE(item_public);

    if (ip->panel->status.painted)
	paint_caret(ip,
	    (dp->flags & SELECTING_ITEM && dp->delete_pending) ? FALSE : TRUE);
}


static void text_yield_kbd_focus(Panel_item item_public)
{
    Item_info      *ip = ITEM_PRIVATE(item_public);

    if (ip->panel->status.painted) paint_caret(ip, FALSE);
}



/* --------------------  Local Routines  -------------------- */

/* Find the position of the character to the left of caret_offset */
static int char_position(int caret_offset, Xv_Font font, char *str,
									int balance_beam)
	/* TRUE = return index of char to the
				    * right if caret_offset is in the
				    * right half of the character.
				    * FALSE = return index of char the
				    * caret offset is within.
				    */
{
	XFontSet font_set;
	XFontStruct *x_font_info;
	int i;	/* character string index */
	int i_prev = 0; /* index of the start byte of the current character */
	int x = 0;	/* desired horizontal position */
	int x_new = 0;	/* next horizontal position */
	int clen = 1;

	if (_xv_is_multibyte) {
		font_set = (XFontSet) xv_get(font, FONT_SET_ID);
		for (i = 0; x_new <= caret_offset && str[i];) {
			i_prev = i;
			clen = mblen(str + i, MB_CUR_MAX);
			if (clen < 1)
				clen = 1;

			x = x_new;
			x_new += XmbTextEscapement(font_set, str + i, clen);
			i += clen;
		}
	}
	else {
		x_font_info = (XFontStruct *) xv_get(font, FONT_INFO);
		for (i = 0; x_new <= caret_offset && str[i]; i++) {
			i_prev = i;
			x = x_new;
			if (x_font_info->per_char) {
				x_new += x_font_info->per_char[(u_char) str[i] -
						x_font_info->min_char_or_byte2].width;
			}
			else
				x_new += x_font_info->min_bounds.width;
		}
	}

	/* Return character string index */
	if (x_new <= caret_offset) {
		return (i);	/* cursor points to the right of the last character */
	}

	if (balance_beam && (caret_offset - x) > (x_new - x) / 2) {
		return (i_prev + clen);	/* cursor is in right half of char; point to
						 * next char */
	}
	else {
		return i_prev;	/* cursor is in left half of char, or not
						 * using "balance beam" method; point to this char
						 */
	}
}


static void draw_scroll_btn(Item_info *ip, int state)
{
    register Text_info *dp = TEXT_FROM_ITEM(ip);
    Xv_Drawable_info *info;
    Xv_Window	    pw;

    PANEL_EACH_PAINT_WINDOW(ip->panel, pw)
		DRAWABLE_INFO_MACRO(pw, info);
		if (state & OLGX_SCROLL_BACKWARD) {
	    	/* Draw left scrolling button */
	    	olgx_draw_textscroll_button(ip->panel->ginfo, xv_xid(info),
			ip->value_rect.r_left,
			ip->value_rect.r_top +
		    	(ip->value_rect.r_height - dp->scroll_btn_height) / 2,
			state);
		} else {
	    	/* Draw right scrolling button */
	    	olgx_draw_textscroll_button(ip->panel->ginfo, xv_xid(info),
			ip->value_rect.r_left + ip->value_rect.r_width -
		    	(dp->scroll_btn_width - SCROLL_BTN_GAP),
			ip->value_rect.r_top +
		    	(ip->value_rect.r_height - dp->scroll_btn_height) / 2,
			state);
		}
    PANEL_END_EACH_PAINT_WINDOW
}

/*
  'horizontal_scroll' is only called when the user has
   pressed the select button over the scrollbutton (with
   autoscroll functionality, 'horizontal scroll' is also
   called when the select button is held down over the
   button).

   'update_value_offset' determines the first and last
   character visible (dp->first_char && dp->last_char)
   and also determines the delta (dp->value_offset)
   between the first and last visible character.

   'update_caret_offset' adjusts the caret offset
   (dp->caret_offset) from the left edge of the first
   visible character and the caret position
   (dp->caret_position), which is the index from the
   first character (*not* first visible character) in
   the text item string.
*/

static void horizontal_scroll(Item_info *ip, int shift)
	/* number of characters to shift value and
				 * caret: <0 for shift left, >0 for shift
				 * right. */
{
    update_value_offset(ip, 0, shift, 0);
    update_caret_offset(ip, 0, 1);
    paint_value(ip, PV_HIGHLIGHT );
}


#ifdef PAINT_BOX
static void
paint_box(ip, pw)
    Item_info	   *ip;
    Xv_Window	    pw;
{
    GC             *gc_list;
    Xv_Drawable_info *info;

    DRAWABLE_INFO_MACRO(pw, info);
    gc_list = (GC *)xv_get(xv_screen(info), SCREEN_OLGC_LIST, pw);
    screen_adjust_gc_color(pw, SCREEN_SET_GC);
    XDrawRectangle(xv_display(info), xv_xid(info), gc_list[SCREEN_SET_GC],
		   ip->value_rect.r_left, ip->value_rect.r_top,
		   ip->value_rect.r_width-1, ip->value_rect.r_height-1);
}
#endif	/* PAINT_BOX */


static void paint_caret(Item_info *ip, int on)
{
	register Panel_info *panel = ip->panel;
	register Text_info *dp = TEXT_FROM_ITEM(ip);
	Display *display;
	Xv_Drawable_info *info;
	int max_x;
	Rect *r;
	int painted_caret_offset;
	Xv_Window pw;
	XID pw_xid;
	char str[2];
	int x;
	int y;
	Xv_Screen screen;
	GC *gc_list;

	if ((on && (panel->caret_on || (dp->flags & TEXT_SELECTED))) ||
			(!on && !panel->caret_on) || !panel->caret)
		return;
	panel->caret_on = on;

	/* paint the caret after the offset & above descender */

	painted_caret_offset = (dp->mask == ' ') ? 0 : dp->caret_offset;

	x = ip->value_rect.r_left + painted_caret_offset - panel->caret_width / 2;
	if (dp->first_char)
		x += dp->scroll_btn_width;
	y = ip->value_rect.r_top + dp->font_home;

	PANEL_EACH_PAINT_WINDOW(panel, pw)
		r = panel_viewable_rect(panel, pw);
		max_x = r->r_left + r->r_width;
		if (x <= max_x - panel->caret_width / 2) {
			DRAWABLE_INFO_MACRO(pw, info);
			screen = xv_screen(info);
			gc_list = (GC *) xv_get(screen, SCREEN_OLGC_LIST, pw);
			display = xv_display(info);
			pw_xid = xv_xid(info);
			if (panel->caret_on) {
				/* Save pixels that will be overwritten by caret */
				XCopyArea(display, pw_xid, panel->caret_bg_pixmap,
						gc_list[SCREEN_SET_GC],
						x, y - panel->caret_ascent,
						(unsigned)panel->caret_width,
						(unsigned)panel->caret_height, 0, 0);
				str[0] = panel->caret;
				str[1] = '\0';
				XDrawString(display, pw_xid, gc_list[SCREEN_CARET_GC],
						x, y, str, 1);
			}
			else {
				/* Restore pixels that were overwritten by caret */
				XCopyArea(display, panel->caret_bg_pixmap, pw_xid,
						gc_list[SCREEN_SET_GC],
						0, 0, (unsigned)panel->caret_width, (unsigned)panel->caret_height,
						x, y - panel->caret_ascent);
			}
		}
	PANEL_END_EACH_PAINT_WINDOW
}


static void paint_text(Item_info *ip)
{
    /* compute the caret position */
    update_value_offset(ip, 0, 0, 1);
    update_caret_offset(ip, 0, 0);

    paint_value(ip, PV_HIGHLIGHT );
}


/*
 * paint_value clears the value rect for ip and paints the string value
 * clipped to the left of the rect.
 */
static void paint_value(Item_info *ip, int highlight)
{
	register Text_info *dp;
	register Panel_info *panel = ip->panel;
	int i, j, len;
	Xv_Drawable_info *info;
	Xv_Window pw = XV_NULL;
	CHAR *str;
	int x;
	int y;
	int btnop;

	if (!panel->status.painted)
		return;
	dp = TEXT_FROM_ITEM(ip);
	x = ip->value_rect.r_left;
	y = ip->value_rect.r_top;

	/* Clear the caret and value rect.
	 * Set the colors for the Clear and Set GC's.
	 */
	PANEL_EACH_PAINT_WINDOW(panel, pw)
		DRAWABLE_INFO_MACRO(pw, info);
		screen_adjust_gc_color(pw, SCREEN_SET_GC);
		panel_clear_pw_rect(pw, ip->value_rect);
	PANEL_END_EACH_PAINT_WINDOW
	
	if (panel->kbd_focus_item == ip)
		/* Since part of the caret may lie outside the value rect,
		 * we need to call paint_caret to make sure all of the
		 * caret has been removed.
		 */
		paint_caret(ip, FALSE);

	/* Get the actual characters which will be displayed */
	len = dp->last_char - dp->first_char + 2;
	if (len) {
		str = (CHAR *) xv_malloc(len * sizeof(CHAR));
		for (j = 0, i = dp->first_char; i <= dp->last_char; i++, j++)
			str[j] = dp->value[i];

		str[len - 1] = '\0';

		/* Draw the left scrolling button if needed */
		if (dp->first_char) {
			btnop = OLGX_SCROLL_BACKWARD;
			if (dp->flags & LEFT_SCROLL_BTN_SELECTED)
				btnop |= OLGX_INVOKED;
			draw_scroll_btn(ip, btnop);
		}

		/* Draw the text */
		if (dp->first_char)
			x += dp->scroll_btn_width;

		if (dp->mask == '\0')	/* not masked */
		{
			PANEL_EACH_PAINT_WINDOW(panel, pw)
				panel_paint_text(pw, ip->value_fontset_id,ip->value_font_xid,
							ip->color_index, x, y + dp->font_home, str);
			PANEL_END_EACH_PAINT_WINDOW
		}
		else {	/* masked */
			CHAR *buf;
			int length, i;

			length = dp->last_char - dp->first_char + 2;
			buf = (CHAR *) xv_malloc(length * sizeof(CHAR));
			for (j = 0, i = dp->first_char; i <= dp->last_char; i++, j++)

				buf[j] = dp->mask;
			buf[length - 1] = '\0';

			PANEL_EACH_PAINT_WINDOW(panel, pw)
				panel_paint_text(pw,  ip->value_fontset_id, ip->value_font_xid,
						ip->color_index, x, y + dp->font_home, buf);
			PANEL_END_EACH_PAINT_WINDOW
			xv_free(buf);
		}

		/* Draw the right scrolling button if needed */

		if (dp->last_char < ((int)strlen(dp->value) - 1))
		{
			btnop = OLGX_SCROLL_FORWARD;
			if (dp->flags & RIGHT_SCROLL_BTN_SELECTED)
				btnop |= OLGX_INVOKED;
			draw_scroll_btn(ip, btnop);
		}

		free((char *)str);

	}
	/* Underline the text (optional) */
	if (dp->flags & UNDERLINED) {
		y = rect_bottom(&ip->value_rect);
		if (ip->panel->status.three_d) {
			/* 3D text ledge is 2 pixels high.  (2D is 1 pixel high.) */
			y--;
		}
		PANEL_EACH_PAINT_WINDOW(panel, pw)
			DRAWABLE_INFO_MACRO(pw, info);
			olgx_draw_text_ledge(panel->ginfo, xv_xid(info),
				ip->value_rect.r_left, y, ip->value_rect.r_width);
		PANEL_END_EACH_PAINT_WINDOW
	}
	if (highlight) {
		/* re-highlight if this is a selection item */
		if (ip == panel->sel_holder[PANEL_SEL_PRIMARY])
			text_seln_highlight(panel, ip, PANEL_SEL_PRIMARY);
		if (ip == panel->sel_holder[PANEL_SEL_SECONDARY])
			text_seln_highlight(panel, ip, PANEL_SEL_SECONDARY);
	}

#ifdef PAINT_BOX
	paint_box(ip, pw);	/* ... used to debug painting problems */
#endif /* PAINT_BOX */

	if (panel->kbd_focus_item == ip)
		paint_caret(ip, TRUE);

	if (inactive(ip)) {
		Xv_Screen screen;
		GC *gc_list;

		DRAWABLE_INFO_MACRO(pw, info);
		screen = xv_screen(info);
		gc_list = (GC *) xv_get(screen, SCREEN_OLGC_LIST, pw);
		screen_adjust_gc_color(pw, SCREEN_INACTIVE_GC);
		XFillRectangle(xv_display(info), xv_xid(info),
				gc_list[SCREEN_INACTIVE_GC],
				ip->value_rect.r_left, ip->value_rect.r_top,
				(unsigned)ip->value_rect.r_width,
				(unsigned)ip->value_rect.r_height);
	}
}


static void panel_find_word(Text_info *dp, int *first, int *last)
{
    register int    index;

	/* Find beginning of word */
	index = *first;
	while ((index > dp->first_char) &&
	       !delim_table[(u_char)dp->value[index]])
	    index--;
	if ((index != *first) && delim_table[(u_char)dp->value[index]])
	    index++;		/* don't include word delimiter */
	*first = index;

	/*
	 * Find end of word.  Note that on a single click, sel_last is set equal
	 * to sel_first.
	 */
	index = *last;
	while ((index < dp->last_char) &&
	       !delim_table[(u_char)dp->value[index]])
	    index++;
	if ((index != *last) && delim_table[(u_char)dp->value[index]])
	    index--;		/* don't include word delimiter */
	*last = index;
}


static void panel_multiclick_handler(Item_info *ip, Event *event, int rank) 
	/* PANEL_SEL_PRIMARY or PANEL_SEL_SECONDARY */
{
	register Text_info *dp = TEXT_FROM_ITEM(ip);
	int left, right;	/* left and right pixel coordinates of selection */
	struct pr_size size;

	if (dp->select_click_cnt[rank] == 2) {
		/* Double click received: select displayed word containing caret */
		panel_find_word(dp, &dp->sel_first[rank], &dp->sel_last[rank]);
		/*
		 * Set caret offset to end of selected word or line closest to mouse
		 * position.
		 */

		size = xv_pf_textwidth(dp->sel_first[rank] - dp->first_char,
				ip->value_font, &dp->value[dp->first_char]);

		left = ip->value_rect.r_left + size.x;
		if (dp->first_char) left += dp->scroll_btn_width;

		size = xv_pf_textwidth(dp->sel_last[rank] -
				dp->sel_first[rank] + 1,
				ip->value_font, &dp->value[dp->sel_first[rank]]);

		right = left + size.x;
		if ((event_x(event) - left) < (right - event_x(event)))
			event_x(event) = left;	/* caret will move to left side */
		else
			event_x(event) = right;	/* caret will move to right side */
		dp->caret_offset = event_x(event) - ip->value_rect.r_left;
		if (dp->first_char)
			dp->caret_offset -= dp->scroll_btn_width;
	}
	else {
		/*
		 * Triple click received: select entire line. Repaint the value, with
		 * the first or last character displayed, if the mouse is closer to
		 * the left or right margin, respectively.
		 */
		panel_select_line(ip, event, rank);
		update_text_rect(ip);
		paint_value(ip, PV_HIGHLIGHT);
	}

}


Pkg_private int panel_printable_char(int code)	/* event action code */
{
	return ((code >= ' ' && code <= '~') || (code >= 0xA0 && code <= 0xFF));
}


static void panel_select_line(Item_info *ip, Event *event, int rank)
	/* NULL => position caret at end of line */
	/* PANEL_SEL_PRIMARY or PANEL_SEL_SECONDARY */
{
	register Text_info *dp = TEXT_FROM_ITEM(ip);
	int max_caret = dp->text_rect.r_width;
	int right;	/* horizontal offset of right margin of
				 * selection */
	int left_offset;	/* horizontal offset of cursor from left
						 * margin of text value */
	int right_offset;	/* horizontal offset of cursor from
						 * right margin of text value */
	int x, i, clen;
	struct pr_size size;

	dp->first_char = dp->sel_first[rank] = 0;
	dp->last_char = dp->sel_last[rank] = strlen(dp->value) - 1;
	right = dp->text_rect.r_width;
	size = xv_pf_textwidth((int)strlen(dp->value), ip->value_font, dp->value);
	if (size.x < right)
		right = size.x;
	if (event) {
		left_offset = event_x(event) - dp->text_rect.r_left;
		right_offset = right + dp->text_rect.r_left - event_x(event);
	}
	if (event && left_offset < right_offset) {
		/*
		 * Repaint (later) with first character displayed. dp->last_char =
		 * the last character in the string, starting from first_char, that
		 * can be fully displayed within the rectangle reserved for the text
		 * value string.
		 */
		dp->caret_offset = 0;
		dp->last_char =
				char_position(right, ip->value_font, dp->value, TRUE) - 1;
		update_value_offset(ip, 0, 0, 1);	/* fix dp->last_char to account for
											   scrolling button */
	}
	else {
		if (_xv_is_multibyte) {
			x = 0;
			i = dp->last_char;

			while (i >= 0 && x < max_caret) {
				int char_start = xv_utf8_current_char_offset(dp->value, i);

				clen = (i - char_start) + 1;
				x += XmbTextEscapement(ip->value_fontset_id,
										dp->value + char_start, clen);

				i = char_start - 1;
			}
		}
		else {
			XFontStruct *x_font_info;

			x_font_info = (XFontStruct *) xv_get(ip->value_font, FONT_INFO);
			/* Repaint (later) with last character displayed */
			x = 0;
			for (i = dp->last_char; (i >= 0) && (x < max_caret); i--) {
				if (x_font_info->per_char) {
					x += x_font_info->per_char[(u_char) dp->value[i] -
							x_font_info->min_char_or_byte2].width;
				}
				else
					x += x_font_info->min_bounds.width;
			}
		}
		if (i >= 0)
			dp->first_char = i + 2;
		dp->caret_offset = ip->value_rect.r_width;
		if (dp->first_char)
			dp->caret_offset -= dp->scroll_btn_width;
		/* Caret cannot exceed last character of value */
		if (dp->caret_offset > dp->value_offset)
			dp->caret_offset = dp->value_offset;
	}
}


/*
 * panel_text_caret_on paints the type-in caret if on is true;
 * otherwise it restores the pixels underneath the caret.
 */
Pkg_private void panel_text_caret_on(Panel_info     *panel, int on)
{
    if (!panel->kbd_focus_item)
	return;

    paint_caret(panel->kbd_focus_item, on);
}


Xv_public Panel_setting panel_text_notify(Panel_item client_item, Event *event)
{
	Text_info *dp = TEXT_PRIVATE(client_item);
	if (panel_erase_action(event) ||
			panel_printable_char(event_action(event)) ||
			event_action(event) == ACTION_CUT ||
			event_action(event) == ACTION_PASTE ||
			event_action(event) == ACTION_UNDO)
		return PANEL_INSERT;
	else if (event_is_down(event) &&
			(event_action(event) == dp->next_field
			|| event_action(event) == ACTION_NEXT_ELEMENT
			|| event_action(event) == '\r'
			|| event_action(event) == '\n'))
		return (event_shift_is_down(event) ? PANEL_PREVIOUS : PANEL_NEXT);
	else
		return PANEL_NONE;
}


Pkg_private void panel_text_paint_label(Item_info *ip)
{
	Rect text_label_rect;
	struct pr_size image_size;
	int image_width;

	text_label_rect = ip->label_rect;
	if (ip->label_width) {
		if (is_string(&ip->label)) {

			image_size = xv_pf_textwidth((int)strlen(image_string(&ip->label)),
					image_font(&ip->label), image_string(&ip->label));

			image_width = image_size.x;
		}
		else
			image_width = ((Pixrect *) image_svrim(&ip->label))->pr_width;
		text_label_rect.r_left += ip->label_rect.r_width - image_width;
	}
	panel_paint_image(ip->panel, &ip->label, &text_label_rect, inactive(ip),
			ip->color_index);
}


/*
 * This is to fix the flashing problem during dragging in primary selection
 * of panel text item. Instead of dehiliting the old selection and then
 * hiliting the new one (which is a continuation of the old one), just invert
 * the difference in selection.  That is either hiliting more if selection
 * has grown, or dehiliting if selection has shrunk.
 */
static void text_add_selection(Panel_info *panel, Item_info *ip,
							int old_caret_offset)
{
	Text_info *dp = TEXT_FROM_ITEM(ip);
	Rect rect;
	struct pr_size size;
	int diff_first = 0;	/* index of first character byte to invert */
	int diff_last = 0;	/* index of last character byte to invert */
	int clen;


	rect = ip->value_rect;

	if ((dp->sel_first[PANEL_SEL_PRIMARY] == primary_seln_first) &&
			(dp->sel_last[PANEL_SEL_PRIMARY] == primary_seln_last) &&
			(dp->caret_offset == old_caret_offset))
	{
		return; /* truly no change */
	}

	if ((dp->sel_first[PANEL_SEL_PRIMARY] == primary_seln_first) &&
		(dp->sel_last[PANEL_SEL_PRIMARY] == primary_seln_last))
	{
		diff_first = -1;
		diff_last = -1;
	}
	else if (primary_seln_first == -1 || primary_seln_last == -1) {
		/* completely new selection: invert the whole
		 * region [sel_first ... sel_last]           this is never reached...
		 */
		diff_first = dp->sel_first[PANEL_SEL_PRIMARY];
		diff_last  = dp->sel_last[PANEL_SEL_PRIMARY];
	}
	else if (dp->sel_first[PANEL_SEL_PRIMARY] == primary_seln_first) {
		if (dp->sel_last[PANEL_SEL_PRIMARY] > primary_seln_last) {
			/* more at the end */
			diff_first = xv_utf8_next_char(dp->value, primary_seln_last);
			diff_last = dp->sel_last[PANEL_SEL_PRIMARY];
		}
		else if (dp->sel_last[PANEL_SEL_PRIMARY] < primary_seln_last) {
			/* less at the end */
			diff_first = xv_utf8_next_char(dp->value,
								dp->sel_last[PANEL_SEL_PRIMARY]);
			diff_last = primary_seln_last;
		}
	}
	else if (dp->sel_last[PANEL_SEL_PRIMARY] == primary_seln_last) {
		if (dp->sel_first[PANEL_SEL_PRIMARY] > primary_seln_first) {
			/* less at the beg */
			diff_first = primary_seln_first;
			diff_last = xv_utf8_prev_char_offset(dp->value,
										dp->sel_first[PANEL_SEL_PRIMARY]);
		}
		else if (dp->sel_first[PANEL_SEL_PRIMARY] < primary_seln_first) {
			/* more at the beg */
			diff_first = dp->sel_first[PANEL_SEL_PRIMARY];
			diff_last = xv_utf8_prev_char_offset(dp->value, primary_seln_first);
		}
	}
	else {
		/* beginning and end have changed (??? how ???) */
		diff_first = dp->sel_first[PANEL_SEL_PRIMARY];
		diff_last  = dp->sel_last[PANEL_SEL_PRIMARY];
	}

	/* Highlight characters bounded by diff_first and diff_last */
	if (diff_first >= dp->first_char) {
		size = xv_pf_textwidth((diff_first - dp->first_char),
				ip->value_font, &dp->value[dp->first_char]);

		rect.r_left += size.x;
	}
	if (dp->first_char)
		rect.r_left += dp->scroll_btn_width;

	/* correct byte-length inkluding the full last multibyte character */
	clen = mblen(&dp->value[diff_last], MB_CUR_MAX);
	if (clen < 1)
		clen = 1;

	size = xv_pf_textwidth((diff_last - diff_first) + clen,
			ip->value_font, &dp->value[diff_first]);

	rect.r_width = size.x;
	if (rect.r_width > dp->text_rect.r_width)
		rect.r_width = dp->text_rect.r_width;
	rect.r_height--;	/* don't disturb underlining */

	if (diff_first != -1 && diff_last != -1 && diff_first <= diff_last) {
		sel_highlight = HL_INVERT;
		panel_invert(panel, &rect, ip->color_index);
	}

	/* restore the rect */
	rect = ip->value_rect;

	if (dp->sel_first[PANEL_SEL_PRIMARY] >= dp->first_char) {
		size = xv_pf_textwidth(dp->sel_first[PANEL_SEL_PRIMARY] -
				dp->first_char, ip->value_font, &dp->value[dp->first_char]);

		rect.r_left += size.x;
	}
	if (dp->first_char)
		rect.r_left += dp->scroll_btn_width;

	clen = mblen(&dp->value[dp->sel_last[PANEL_SEL_PRIMARY]], MB_CUR_MAX);
	if (clen < 1)
		clen = 1;

	size = xv_pf_textwidth((dp->sel_last[PANEL_SEL_PRIMARY] -
					dp->sel_first[PANEL_SEL_PRIMARY]) + clen,
			ip->value_font, &dp->value[dp->sel_first[PANEL_SEL_PRIMARY]]);

	rect.r_width = size.x;
	if (rect.r_width > dp->text_rect.r_width)
		rect.r_width = dp->text_rect.r_width;
	rect.r_height--;	/* don't disturb underlining */

	/* update globals */
	primary_seln_panel = panel;
	primary_seln_rect = rect;
	primary_seln_first = dp->sel_first[PANEL_SEL_PRIMARY];
	primary_seln_last = dp->sel_last[PANEL_SEL_PRIMARY];
}

static void text_alarm(Item_info *ip)
{
    struct timeval  wait;

    wait.tv_sec = wait.tv_usec = 0;
    win_bell( PANEL_PUBLIC(ip->panel), wait, XV_NULL );
}


/* Dehighlight whatever was last highlighted */
static void text_seln_dehighlight(Item_info *ip, int rank)
{
	Xv_Drawable_info *info;
	Panel_info *sel_panel = (Panel_info *) 0;
	Rect *sel_rect_handle;
	Text_info *dp = TEXT_FROM_ITEM(ip);
	int sel_first, sel_last;
	char save_seln_last_char;
	Xv_Window pw;

	switch (rank) {
		case PANEL_SEL_PRIMARY:
			sel_panel = primary_seln_panel;
			sel_rect_handle = &primary_seln_rect;
			sel_first = primary_seln_first;
			sel_last = primary_seln_last;
			primary_seln_panel = 0;	/* no longer valid */
			break;
		case PANEL_SEL_SECONDARY:
			sel_panel = secondary_seln_panel;
			sel_rect_handle = &secondary_seln_rect;
			sel_first = secondary_seln_first;
			sel_last = secondary_seln_last;
			secondary_seln_panel = 0;	/* no longer valid */
			break;
	}

	if (sel_panel && dp->flags & TEXT_HIGHLIGHTED) {
		/* Note: The only case where the TEXT_HIGHLIGHTED flag wouldn't be set
		 * is if the PANEL_TEXT item was being hidden (i.e., XV_SHOW being
		 * set to FALSE).  In this case, TEXT_HIGHLIGHTED is cleared in
		 * text_clear.
		 */
		if (sel_highlight == HL_INVERT)
			panel_invert(sel_panel, sel_rect_handle, ip->color_index);
		else if (ip && (sel_highlight == HL_UNDERLINE ||
						sel_highlight == HL_STRIKE_THRU)) {
			int doit = FALSE;

			if (_xv_is_multibyte) {
				/*  the selected characters actually
				 *  taken up, then compare it with
				 *  dp->display_width
				 */
				int x;

				{
					int my_sel_first, my_sel_last;
					my_sel_first = xv_utf8_align_left(dp->value, sel_first);
					my_sel_last  = xv_utf8_align_left(dp->value, sel_last);

					x = XmbTextEscapement(ip->value_fontset_id,
							dp->value + my_sel_first,
							(my_sel_last - my_sel_first +
							mblen(dp->value + my_sel_last, MB_CUR_MAX)));
				}
				if ((sel_first >= dp->first_char) && (x <= dp->display_width))
					doit = TRUE;
			}
			else {
				if ((sel_first >= dp->first_char) &&
					(sel_last - sel_first + 1 <= dp->display_length))
					doit = TRUE;
			}

			if (doit) {
				/* ??? variable-width * ??? */
				/* Entire selection is visible */
				panel_clear_rect(ip->panel, *sel_rect_handle);
				save_seln_last_char = dp->value[sel_last + 1];
				dp->value[sel_last + 1] = 0;	/* terminate substring */

				PANEL_EACH_PAINT_WINDOW(sel_panel, pw)
					panel_paint_text(pw, ip->value_fontset_id,
								ip->value_font_xid, ip->color_index,
								sel_rect_handle->r_left,
								sel_rect_handle->r_top + dp->font_home,
								dp->value + sel_first);
					if (dp->flags & UNDERLINED && ip->panel->status.three_d) {
						/* Redraw the 3D text ledge */
						DRAWABLE_INFO_MACRO(pw, info);
						olgx_draw_text_ledge(ip->panel->ginfo, xv_xid(info),
								ip->value_rect.r_left,
								rect_bottom(&ip->value_rect) - 1,
								ip->value_rect.r_width);
					}
				PANEL_END_EACH_PAINT_WINDOW
				dp->value[sel_last + 1] = save_seln_last_char;
			}
			else
				paint_value(ip, PV_NO_HIGHLIGHT);
		}
		if (sel_highlight != HL_NONE)
			sel_highlight = HL_NONE;
		dp->flags &= ~TEXT_HIGHLIGHTED;
	}
}


static int text_seln_delete(Item_info *ip, int rank)
	/* PANEL_SEL_PRIMARY or PANEL_SEL_SECONDARY */
{
	int caret_shift = 0;
	Text_info *dp;
	Event event;
	int last;	/* position of last valid char in value */
	int new;	/* new position of char to be moved */
	int old;	/* old position of char to be moved */
	struct pr_size size;
	int undo_index;
	int val_change = 0;

	if (!ip)
		return FALSE;
	dp = TEXT_FROM_ITEM(ip);

	if (dp->flags & PTXT_READ_ONLY)
		return FALSE;

	if (rank == PANEL_SEL_DND) {
		rank = PANEL_SEL_PRIMARY;
		dp->sel_first[PANEL_SEL_PRIMARY] = dp->sel_first[PANEL_SEL_DND];
		dp->sel_last[PANEL_SEL_PRIMARY] = dp->sel_last[PANEL_SEL_DND];
	}

	if (rank == PANEL_SEL_PRIMARY)
		dp->delete_pending = FALSE;

	/*
	 * Calculate number of character positions to move displayed value
	 * (val_change) and number of character positions to move caret
	 * (caret_shift).
	 */
	val_change = dp->sel_first[rank] - dp->sel_last[rank] - 1;

	size = xv_pf_textwidth(dp->sel_last[rank] - dp->first_char + 1,
			ip->value_font, &dp->value[dp->first_char]);
	if (dp->caret_offset >= size.x)	/* Is caret at or past right margin of
									   * selection? */
		caret_shift = val_change;	/* Yes: shift caret to left margin of
									   * selection (e.g., ACTION_CUT or
									   * pending delete accept_key), or
									   * account for deleted characters.
									   * (e.g., DRAG_MOVE delete) */

	/* Copy the characters to be deleted to the undo buffer */
	undo_index = 0;
	for (new = dp->sel_first[rank]; new <= dp->sel_last[rank]; new++)

	dp->undo_buffer[undo_index++] = dp->value[new];
	dp->undo_buffer[undo_index] = 0;	/* NULL terminate the undo buffer */
	dp->undo_direction = INSERT;

	/* Delete the selected characters from the value buffer */
	new = dp->sel_first[rank];
	old = dp->sel_last[rank] + 1;

	last = strlen(dp->value);

	for (; new <= dp->stored_length - 1; new++, old++) {
		if (old > last)
			dp->value[new] = 0;
		else
			dp->value[new] = dp->value[old];
	}

	/* Adjust Drag and Drop selection boundaries, if necessary */
	if (dp->dnd_sel_first > dp->sel_first[rank]) {
		dp->dnd_sel_first -= dp->sel_last[rank] - dp->sel_first[rank] + 1;
		dp->dnd_sel_last -= dp->sel_last[rank] - dp->sel_first[rank] + 1;
	}

	/*
	 * Selection has been "used up": no mouse-left clicks or primary
	 * selection pending.
	 */
	dp->select_click_cnt[rank] = 0;
	dp->select_is_word[rank] = FALSE;
	SERVERTRACE((444, "scc[%d] = %d\n", rank, dp->select_click_cnt[rank]));
	dp->flags &= ~TEXT_HIGHLIGHTED;
	if (rank == PANEL_SEL_PRIMARY) {
		primary_seln_panel = NULL;
		dp->flags &= ~TEXT_SELECTED;
	}
	else
		secondary_seln_panel = NULL;

	/* BR# 1073493 */
	xv_set(ip->panel->sel_item[rank],
			SEL_DATA, NULL,
			SEL_LENGTH, 0,
			NULL);

	/* Repaint the value */
	update_value_offset(ip, val_change, 0, 1);
	paint_value(ip, PV_HIGHLIGHT);

	if (ip->panel->kbd_focus_item == ip) {
		/* Repaint the caret */
		paint_caret(ip, FALSE);
		dp->caret_offset = -1;	/* caret_offset is now invalid */
		update_caret_offset(ip, caret_shift, 0);
		paint_caret(ip, TRUE);
	}

/*  Need to fake event->ie_string again for secondary selection ??? */

	if (rank == PANEL_SEL_SECONDARY	/* BUG ALERT: ??? */
			&& dp->notify_level != PANEL_NONE) {
		event_init(&event);
		event_set_up(&event);
		event_set_action(&event, ACTION_CUT);
		(void)(*ip->notify) (ITEM_PUBLIC(ip), &event);
	}

	return TRUE;
}

/*
 * Highlight selection according to its rank.
 */
static void text_seln_highlight(Panel_info *panel, Item_info *ip, int rank)
	/* PANEL_SEL_PRIMARY or PANEL_SEL_SECONDARY */
{
	Text_info *dp = TEXT_FROM_ITEM(ip);
	GC *gc_list;
	Xv_Drawable_info *info;
	Xv_Window pw;
	Rect rect;
	Xv_Screen screen;
	struct pr_size size;
	int start_byte, end_byte, end_char_len, total_byte_len, y;

	rect = ip->value_rect;;

	if (dp->select_click_cnt[rank] == 0 || strlen(dp->value) == 0)
		return;

	/* Highlight characters bounded by sel_first and sel_last */
	if (dp->sel_first[rank] > dp->first_char) {
		size = xv_pf_textwidth((dp->sel_first[rank] - dp->first_char),
				ip->value_font, dp->value + dp->first_char);
		rect.r_left += size.x;
	}
	if (dp->first_char)
		rect.r_left += dp->scroll_btn_width;
	start_byte = dp->sel_first[rank];
	end_byte = dp->sel_last[rank];
	end_char_len = mblen(dp->value + end_byte, MB_CUR_MAX);
	if (end_char_len < 1) end_char_len = 1;
	total_byte_len = (end_byte - start_byte) + end_char_len;
	size = xv_pf_textwidth(total_byte_len, ip->value_font,
						dp->value + dp->sel_first[rank]);
	rect.r_width = size.x;
	if (rect.r_width > dp->text_rect.r_width)
		rect.r_width = dp->text_rect.r_width;
	rect.r_height--;	/* don't disturb underlining */

	switch (rank) {
		case PANEL_SEL_PRIMARY:
			primary_seln_panel = panel;	/* save panel */
			primary_seln_rect = rect;	/* save rectangle coordinates */
			primary_seln_first = dp->sel_first[PANEL_SEL_PRIMARY];
			primary_seln_last = dp->sel_last[PANEL_SEL_PRIMARY];
			sel_highlight = HL_INVERT;

			panel_invert(panel, &rect, ip->color_index);
			break;

		case PANEL_SEL_SECONDARY:
			secondary_seln_panel = panel;	/* save panel */
			secondary_seln_rect = rect;	/* save rectangle coordinates */
			secondary_seln_first = dp->sel_first[PANEL_SEL_SECONDARY];
			secondary_seln_last = dp->sel_last[PANEL_SEL_SECONDARY];
			if (panel->status.quick_move) {
				SERVERTRACE((765, "HL_STRIKE_THRU\n"));
				sel_highlight = HL_STRIKE_THRU;
				y = rect.r_top + (rect.r_height / 2);
			}
			else {
				SERVERTRACE((765, "HL_UNDERLINE\n"));
				sel_highlight = HL_UNDERLINE;
				y = rect_bottom(&rect);
			}
			PANEL_EACH_PAINT_WINDOW(panel, pw)
				if (ip->color_index >= 0) {
					xv_vector(pw, rect.r_left, y, rect.r_left + rect.r_width - 1, y,
							ip->color_index < 0 ? PIX_SET :
							PIX_SRC | PIX_COLOR(ip->color_index), 0);
				}
				else {
					DRAWABLE_INFO_MACRO(pw, info);
					screen = xv_screen(info);
					gc_list = (GC *) xv_get(screen, SCREEN_OLGC_LIST, pw);
					XDrawLine(xv_display(info), xv_xid(info),
							gc_list[SCREEN_SET_GC],
							rect.r_left, y, rect.r_left + rect.r_width - 1, y);
				}
			PANEL_END_EACH_PAINT_WINDOW
			break;
	}
	dp->flags |= TEXT_HIGHLIGHTED;
}

static void note_sel_reply(Selection_requestor sr, Atom target, Atom type,
					Xv_opaque value, unsigned long length, int format)
{
	Panel w = xv_get(sr, XV_OWNER);
	Panel_info *priv = PANEL_PRIVATE(w);

	if (length == SEL_ERROR) {
		if (target == priv->atom.selection_end) {
			/* old selection owner that rejected _SUN_SELECTION_END ?
			 * Try the old _SUN_SELN_YIELD...
			 */
			xv_set(sr, SEL_TYPE, priv->atom.seln_yield, NULL);
			sel_post_req_no_answer(sr);
		}
		return;
	}

	if (target == priv->atom.selection_end) {
		if (value) xv_free(value);
	}
	if (target == priv->atom.seln_yield) {
		if (value) xv_free(value);
	}
}


static void text_seln_init(Panel_info *panel)
{
	Panel panel_public = PANEL_PUBLIC(panel);

	panel->sel_owner[PANEL_SEL_PRIMARY] =
					xv_create(panel_public, SELECTION_OWNER,
							SEL_CONVERT_PROC, text_convert_proc,
							SEL_LOSE_PROC, text_lose_proc,
							XV_KEY_DATA, item_key, panel_public,
							NULL);
	panel->sel_rank[PANEL_SEL_PRIMARY] = XA_PRIMARY;

	panel->sel_item[PANEL_SEL_PRIMARY] =
			xv_create(panel->sel_owner[PANEL_SEL_PRIMARY], SELECTION_ITEM,
					NULL);

	panel->sel_owner[PANEL_SEL_SECONDARY] =
					xv_create(panel_public, SELECTION_OWNER,
							SEL_CONVERT_PROC, text_convert_proc,
							SEL_LOSE_PROC, text_lose_proc,
							SEL_RANK, XA_SECONDARY,
							XV_KEY_DATA, item_key, panel_public,
							NULL);
	panel->sel_rank[PANEL_SEL_SECONDARY] = XA_SECONDARY;

	panel->sel_item[PANEL_SEL_SECONDARY] =
			xv_create(panel->sel_owner[PANEL_SEL_SECONDARY], SELECTION_ITEM,
					NULL);

	panel->sel_owner[PANEL_SEL_CLIPBOARD] =
					xv_create(panel_public, SELECTION_OWNER,
							SEL_CONVERT_PROC, text_convert_proc,
							SEL_LOSE_PROC, text_lose_proc,
							SEL_RANK_NAME, "CLIPBOARD",
							XV_KEY_DATA, item_key, panel_public,
							NULL);
	panel->sel_rank[PANEL_SEL_CLIPBOARD] =
					xv_get(panel->sel_owner[PANEL_SEL_CLIPBOARD], SEL_RANK);

	panel->sel_item[PANEL_SEL_CLIPBOARD] =
			xv_create(panel->sel_owner[PANEL_SEL_CLIPBOARD], SELECTION_ITEM,
					NULL);

	panel->sel_req = xv_create(panel_public, SELECTION_REQUESTOR,
					/* needed because of nonblocking _SUN_SELECTION_END: */
					SEL_REPLY_PROC, note_sel_reply,
					NULL);
}


/* Copy the primary selection data to the clipboard */
static void text_set_clipboard(Panel_info *panel, Item_info *ip, Text_info *dp)
{
	struct timeval tv;
	Panel pan = PANEL_PUBLIC(panel);
	Xv_server srv = XV_SERVER_FROM_WINDOW(pan);

	server_set_timestamp(srv, &tv, 0L);
	xv_set(panel->sel_owner[PANEL_SEL_CLIPBOARD], SEL_TIME, &tv, NULL);
	if (xv_set(panel->sel_owner[PANEL_SEL_CLIPBOARD],
					SEL_OWN, TRUE, NULL) == XV_OK)
	{
		dp->select_is_word[PANEL_SEL_CLIPBOARD]
							= dp->select_is_word[PANEL_SEL_PRIMARY];
		SERVERTRACE((377, "%s: is_word=%d\n", __FUNCTION__,
				dp->select_is_word[PANEL_SEL_CLIPBOARD]));

		xv_set(panel->sel_item[PANEL_SEL_CLIPBOARD],
			SEL_DATA, xv_get(panel->sel_item[PANEL_SEL_PRIMARY], SEL_DATA),
			SEL_LENGTH, xv_get(panel->sel_item[PANEL_SEL_PRIMARY], SEL_LENGTH),
			NULL);

		panel->sel_holder[PANEL_SEL_CLIPBOARD] = ip;
	}
}


static void text_set_sel_data(Selection_item selit, Text_info *dp, int rank)
{
	int start_byte = dp->sel_first[rank];
	int end_byte = dp->sel_last[rank];
	int end_char_len = mblen(dp->value + end_byte, MB_CUR_MAX);
	int total_byte_len;

	if (end_char_len < 1) end_char_len = 1;
	total_byte_len = (end_byte - start_byte) + end_char_len;

	xv_set(selit,
			SEL_DATA, dp->value + start_byte,
			SEL_LENGTH, total_byte_len,
			NULL);
}


/*
 * update_caret_offset computes the caret x offset (dp->caret_offset) and
 * character position (dp->caret_position) for ip.
 *
 * If 'caret_shift' is non-zero, then dp->caret_position is incremented
 * or decremented (depending upon value of 'caret_shift'). However, if
 * 'caret_shift' *is* zero, then dp->caret_position is determined by
 * finding appropriate index by examining offset (dp->caret_offset) from
 * first visible character (dp->first_char).
 *
 * Before exiting this function, caret_offset is calculated from delta
 * between first visible char and caret_position.
 *
 */



static void update_caret_offset(Item_info *ip, int caret_shift,
									int calc_from_caret_pos)
	/* char position delta from caret_position */
{
	register Text_info *dp = TEXT_FROM_ITEM(ip);
	int max_caret_pos;
	struct pr_size size;

	if (caret_shift || calc_from_caret_pos) {
		dp->caret_position += caret_shift;
		if (dp->caret_position < dp->first_char)
			dp->caret_position = dp->first_char;
	}
	else if (dp->caret_offset >= 0)
		dp->caret_position = char_position(dp->caret_offset,
				ip->value_font, &dp->value[dp->first_char], TRUE) +
				dp->first_char;
	max_caret_pos = strlen(dp->value);
	if (dp->caret_position > max_caret_pos)
		dp->caret_position = max_caret_pos;
	size = xv_pf_textwidth(dp->caret_position - dp->first_char,
			ip->value_font, &dp->value[dp->first_char]);
	dp->caret_offset = size.x;

	/* Caret cannot exceed last character of value */
	if (dp->caret_offset > dp->value_offset) {
		dp->caret_offset = dp->value_offset;
		dp->caret_position = dp->last_char + 1;
	}
}


/*
 * Define rectangle containing the text (i.e., value rect less arrows)
 */
static void update_text_rect(Item_info *ip)
{
	register Text_info *dp = TEXT_FROM_ITEM(ip);

	dp->text_rect = ip->value_rect;
	if (dp->first_char) {
		dp->text_rect.r_left += dp->scroll_btn_width;
		dp->text_rect.r_width -= dp->scroll_btn_width;
	}

	if (dp->last_char < (int)strlen(dp->value) - 1)
		dp->text_rect.r_width -= dp->scroll_btn_width;
}


/*
 * update_value updates the text item value and cursor position according to
 * the event action.  Actions fall into four categories: editing, navigation,
 * selection and printable characters.  ok_to_insert determines whether editing
 * actions and printable characters are acted upon.
 */
static void update_value(Item_info *ip, int action,
    int ok_to_insert, int synthetic_event, int *retstatus, char *ev_str,
	int ev_flags)
	/* synthetic_event: FALSE: user-generated,
				       * TRUE: not user-generated */
                        /* misc return status - added to return */
                        /* information about whether DRAG_MOVE was */
                        /* a file name or array of text */
{
	int i;	/* counter */
	Text_info *dp = TEXT_FROM_ITEM(ip);
	char *sp;	/* string value */
	int ascent = 0;
	int caret_shift = 0;	/* number of positions to move caret */
	int char_code;
	int descent = 0;
	int direction = 0;
	Xv_Drawable_info *info;
	int insert_pos;	/* position for character add/delete */
	int j;
	int new_len;	/* new string length */
	int orig_caret_position;	/* original caret position */
	int orig_len;	/* original string length */
	int orig_offset;	/* before caret offset */
	int orig_text_rect_width;	/* original text rectangle width */
	XCharStruct overall;
	Panel_info *panel = ip->panel;
	int pc_adv_x;
	Xv_Window pw;
	int rank = -1;	/* initialize to "invalid" */
	Rect rect;
	int selection_action = FALSE;
	int sel_format;	/* size of data element: 8, 16 or 32 */
	long sel_length;	/* # char's in selection excluding NULL */
	CHAR *sel_string;	/* contents of selection */
	struct pr_size size;
	int undo_cnt;
	int undo_index;
	int val_change = 0;	/* number of characters added (+) or deleted (-) */
	int val_shift = 0;	/* number of characters to shift value display */
	int was_clipped;	/* TRUE if value was clipped */
	int x;	/* left of insert/del point */
	XRectangle overall_logical_extents = { 0 };
	XRectangle overall_ink_extents = { 0 };
	XFontStruct *x_font_info;
	int clen; /* character length */

	/*
	 * Initialize overall to zeros
	 * It is not initialized like overall_ink_extents above because the MIT
	 * build (using cc), complains about "no automatic aggregate initialization"
	 */
	XV_BZERO(&overall, sizeof(XCharStruct));

	*retstatus = 0;

	/* Define rectangle containing the text (i.e., value rect less arrows) */
	update_text_rect(ip);

	/* Get the insert position for character add/delete */
	if (dp->caret_offset == 0)
		insert_pos = dp->first_char;
	else
		insert_pos = dp->caret_position;

	sp = dp->value;
	orig_len = strlen(sp);

	switch (action) {

	  /**********************************************************************
       *     Editing actions                                                *
       **********************************************************************/
		case ACTION_ERASE_CHAR_BACKWARD:
			/* Allow notify_proc to override editting characters. */
			/* Supports read-only text fields. */
			if (!ok_to_insert) {
				text_alarm(ip);
				return;
			}
			/* Nothing to backspace if caret is at left boundary. */
			if (dp->caret_offset == 0)
				return;

			/* Can't show result of backspace if display length exceeded and */
			/* caret is to the right of the panel left arrow.  The moral here */
			/* is that you can't delete what you can't see. */

			if (_xv_is_multibyte) {
				/*  Need to use pixel comparisons,
				 *  instead of character length comparison
				 */
				int x;

				x = XmbTextEscapement(ip->value_fontset_id, sp, orig_len);
				if ((x > dp->display_width) &&
						(dp->first_char) && (dp->caret_offset == 0))
					return;
			}
			else {
				if ((orig_len > dp->display_length) &&
						(dp->first_char) && (dp->caret_offset == 0))
					return;
			}

			if ((*sp) && (insert_pos > 0)) {
				dp->undo_buffer[0] = sp[insert_pos - 1];
				dp->undo_buffer[1] = 0;

				dp->undo_direction = INSERT;
				for (i = insert_pos; i < orig_len; i++)
					sp[i - 1] = sp[i];
				sp[orig_len - 1] = '\0';
				insert_pos--;
				caret_shift = _xv_is_multibyte
							? -xv_utf8_prev_char_len(dp->value, insert_pos) 
					    	: -1;
				val_change = -1;

				/*
				 * If clipped at left boundary, leave caret alone. Characters
				 * will shift in from the left.
				 */
				if (dp->first_char) {
					caret_shift = 0;
					dp->caret_position--;
				}
			}
			break;

		case ACTION_ERASE_CHAR_FORWARD:
			/* Allow notify_proc to override editting characters. */
			/* Supports read-only text fields. */
			if (!ok_to_insert) {
				text_alarm(ip);
				return;
			}
			/* Can't show result of forespace if display length exceeded and */
			/* caret is to the left of the panel right arrow.  The moral here */
			/* is that you can't delete what you can't see. */
			if (dp->caret_offset == dp->text_rect.r_width)
				return;

			size = xv_pf_textwidth(dp->last_char - dp->first_char,
					ip->value_font, &dp->value[dp->first_char]);

			if (dp->caret_offset > size.x)
				return;

			if ((*sp) && (insert_pos >= 0)) {
				dp->undo_buffer[0] = sp[insert_pos];
				dp->undo_buffer[1] = 0;

				dp->undo_direction = INSERT;
				for (i = insert_pos; i < orig_len; i++)
					sp[i] = sp[i + 1];
				sp[orig_len - 1] = '\0';
				caret_shift = 0;
				val_change = 0;
				if ((dp->last_char >= ((int)STRLEN(sp) - 1))
						&& (dp->last_char > 1)) {
					val_change = -1;
					/*
					 * ???  Why was the following line put in here?  This causes
					 * a bug in deleting the next character when the last char is
					 * displayed and the first character is not.  ???
					 */
					/* if (dp->first_char > 2) caret_shift = 1; */
				}
			}
			break;

		case ACTION_ERASE_WORD_BACKWARD:
			/* ACTION_ERASE_WORD_BACKWARD is a SunView1 compatibility action */

			/* Allow notify_proc to override editting characters. */
			/* Supports read-only text fields. */
			if (!ok_to_insert) {
				text_alarm(ip);
				return;
			}
			/* skip back past blanks */
			if (insert_pos > orig_len)
				insert_pos -= (dp->first_char - 1);
			for (i = insert_pos - 1; (i >= 0) && (sp[i] == ' '); i--);

			for (; (i >= 0) && (sp[i] != ' '); i--);

			if (i < 0)
				i = 0;
			if (i > 0)
				i++;
			caret_shift = i - insert_pos;
			val_change = i - insert_pos;
			/* Copy the word to be deleted to the undo buffer */
			undo_index = 0;
			undo_cnt = -caret_shift;

			for (j = 0; j < undo_cnt; j++)
				dp->undo_buffer[undo_index++] = sp[i + j];
			dp->undo_buffer[undo_index] = 0;

			dp->undo_direction = INSERT;
			/* Delete the word */
			for (j = insert_pos; j <= orig_len; j++, i++)
				sp[i] = sp[j];
			insert_pos += caret_shift;
			break;

		case ACTION_ERASE_WORD_FORWARD:
			/* ACTION_ERASE_WORD_FORWARD is a SunView1 compatibility action */

			/* Allow notify_proc to override editting characters. */
			/* Supports read-only text fields. */
			if (!ok_to_insert) {
				text_alarm(ip);
				return;
			}
			/* skip back past blanks */
			for (i = insert_pos; (i < orig_len) && (sp[i] == ' '); i++);

			for (; (i < orig_len) && (sp[i] != ' '); i++);

			if (i >= orig_len)
				i = orig_len - 1;
			if (i < (orig_len - 1))
				i--;
			caret_shift = 0;
			val_change = 0;
			/* Copy the word to be deleted to the undo buffer */
			undo_index = 0;
			undo_cnt = i + 1 - insert_pos;
			for (j = 0; j < undo_cnt; j++)

			dp->undo_buffer[undo_index++] = sp[insert_pos + j];
			dp->undo_buffer[undo_index] = 0;

			dp->undo_direction = INSERT;
			/* Delete the word */
			for (j = insert_pos; i < orig_len; j++, i++)
				sp[j] = sp[i + 1];
			break;

		case ACTION_ERASE_LINE_BACKWARD:
			/* ACTION_ERASE_LINE_BACKWARD is a SunView1 compatibility action */
			/* Allow notify_proc to override editting characters. */
			/* Supports read-only text fields. */
			if (!ok_to_insert) {
				text_alarm(ip);
				return;
			}
			/* sp[0] = '\0'; */
			caret_shift = -insert_pos;
			val_change = -insert_pos;
			/* Copy the characters to be deleted to the undo buffer */
			undo_index = 0;
			undo_cnt = insert_pos;
			for (i = 0; i < undo_cnt; i++)

			dp->undo_buffer[undo_index++] = sp[i];
			dp->undo_buffer[undo_index] = 0;

			dp->undo_direction = INSERT;
			/* Delete the line (backward) */
			for (i = 0, j = insert_pos; j <= orig_len; i++, j++)
				sp[i] = sp[j];
			insert_pos = 0;
			break;

		case ACTION_ERASE_LINE_END:
			/* ACTION_ERASE_LINE_END is a SunView1 compatibility action */

			/* Allow notify_proc to override editting characters. */
			/* Supports read-only text fields. */
			if (!ok_to_insert) {
				text_alarm(ip);
				return;
			}
			caret_shift = 0;
			val_change = 0;
			/* Copy the characters to be deleted to the undo buffer */
			undo_index = 0;
			i = insert_pos;
			for (undo_cnt = STRLEN(sp) - insert_pos; undo_cnt >= 0; undo_cnt--)

			dp->undo_buffer[undo_index++] = sp[i++];
			dp->undo_buffer[undo_index] = 0;

			dp->undo_direction = INSERT;
			/* Delete the line (forward) */
			sp[insert_pos] = '\0';
			if (dp->first_char > 1) {
				val_change = STRLEN(sp) - 1 - dp->last_char;
				if (dp->last_char < (orig_len - 1))
					val_change--;
				caret_shift = -val_change;
			}
			break;

		case ACTION_ERASE_LINE:
			/* Allow notify_proc to override editting characters. */
			/* Supports read-only text fields. */
			if (!ok_to_insert) {
				text_alarm(ip);
				return;
			}
			/* Copy the characters to be deleted to the undo buffer */
			strcpy(sp, dp->undo_buffer);

			dp->undo_direction = INSERT;
			/* Delete the line */
			sp[0] = '\0';
			dp->caret_offset = 0;
			dp->caret_position = 0;
			dp->first_char = 0;
			dp->last_char = 0;
			dp->value_offset = 0;
			update_text_rect(ip);

			if (dp->mask != ' ') paint_value(ip, PV_HIGHLIGHT);
			return;

		case ACTION_DRAG_COPY:
		case ACTION_DRAG_MOVE:
		case ACTION_CUT:
		case ACTION_PASTE:
			/* Allow notify_proc to override editting characters. */
			/* Supports read-only text fields. */
			if (!ok_to_insert) {
				text_alarm(ip);
				return;
			}
			if (action == ACTION_CUT || action == ACTION_PASTE) {
				if (XGetSelectionOwner(XV_DISPLAY_FROM_WINDOW(PANEL_PUBLIC
										(panel)),
								XA_SECONDARY) != None) {
					/* Get the contents of the Secondary Selection */
					rank = PANEL_SEL_SECONDARY;
					xv_set(panel->sel_req, SEL_RANK, XA_SECONDARY, NULL);
				}
				else if (action == ACTION_PASTE) {
					/* Get the contents of the Clipboard Selection */
					rank = PANEL_SEL_CLIPBOARD;
					xv_set(panel->sel_req, SEL_RANK_NAME, "CLIPBOARD", NULL);
				}
				else {
					/* Primary CUT: this has already been processed in
					 * text_accept_key(), so just return.
					 */
					return;
				}
			}
			else {
				/* Get the data from the initiator of the drag and drop */
				rank = PANEL_SEL_PRIMARY;
				dp->dnd_sel_first = insert_pos;
			}
			dp->flags &= ~SELECTION_REQUEST_FAILED;	/* assume we succeed */
			sel_string = (CHAR *)0;

			/* First try to convert the Selection Type "FILE_NAME" */
			if (dp->flags & PTXT_ALLOW_FILE_DROP) {
				if (action == ACTION_DRAG_COPY || action == ACTION_DRAG_MOVE) {
					if (ev_flags & DND_IS_XDND) {
						/* they usually don't know what a FILE_NAME is... */
						xv_set(panel->sel_req,
									SEL_TYPE_NAME, "text/uri-list",
									NULL);
						sel_string = (CHAR *)xv_get(panel->sel_req, SEL_DATA,
												&sel_length, &sel_format);
						if (sel_length == SEL_ERROR) {
							xv_set(panel->sel_req, SEL_TYPE_NAME, "FILE_NAME", NULL);
							sel_string = 0;
						}
						else {
							char *p, *sss = sel_string;
							/* text/uri-list returns funny things: */
							if (! strncmp(sel_string, "file://", 7L)) {
								sel_string = sel_string + 7;
								if (*sel_string != '/') {
									/* hey - looks like a REAL file: url...
								 	* but I still don't know what to do with
								 	* the hostname .....
								 	*/
									p = strchr(sel_string, '/');
									if (p) sel_string = p;
								}
								p = strtok(sel_string, "\r\n");
								sel_string = xv_strsave(p);
								xv_free(sss);
							}
							else {
								/* what now, one of those stupids out there
								 * does not even understand so much....
								 */
								fprintf(stderr, "text/uri-list delivers '%s'\n",
										sel_string);
								xv_free(sel_string);
								sel_string = 0;
							}
						}
					}
					else {
						xv_set(panel->sel_req, SEL_TYPE_NAME, "FILE_NAME", NULL);
					}
				}
				else {
					xv_set(panel->sel_req, SEL_TYPE_NAME, "FILE_NAME", NULL);
				}
			}

			/*  Data came in as compound text and sel_length is
			 *  a byte count
			 */

			if (! sel_string)
				sel_string = (char *) xv_get(panel->sel_req, SEL_DATA,
					&sel_length, &sel_format);

			/* bit 1 => straight text; bit 2 => file name */
			*retstatus |= (sel_length == SEL_ERROR ? DRAG_MOVE_TEXT :
					DRAG_MOVE_FILENAME);

			{
				if (sel_length == SEL_ERROR) {
					/* Data type is ASCII string */
					xv_set(panel->sel_req, SEL_TYPE, XA_STRING, NULL);
					/* Get the contents of the appropriate selection.
					 * Note: the returned string is not NULL-terminated.
					 * 'sel_length' will indicate the string's length.
					 */

					sel_string = (char *)xv_get(panel->sel_req, SEL_DATA,
							&sel_length, &sel_format);

					{

#ifdef NO_XDND
#else /* NO_XDND */
		/* firefox only offers this for CLIPBOARD, but not for XdndSelection
						if (sel_length == SEL_ERROR) {
							xv_set(panel->sel_req, SEL_TYPE_NAME,
										"text/plain;charset=ISO-8859-1",
										NULL);
							sel_string =
									(char *)xv_get(panel->sel_req, SEL_DATA,
									&sel_length, &sel_format);
						}
		*/
						if (sel_length == SEL_ERROR) {
							xv_set(panel->sel_req, SEL_TYPE_NAME, "text/plain",
									NULL);
							sel_string =
									(char *)xv_get(panel->sel_req, SEL_DATA,
									&sel_length, &sel_format);
						}
#endif /* NO_XDND */

						if (sel_length == SEL_ERROR)
							/* Check that the data arrived okay */
						{
							dp->flags |= SELECTION_REQUEST_FAILED;
							if (rank != PANEL_SEL_PRIMARY) {
								text_alarm(ip);
							}
							else {
								Frame frame = xv_get(PANEL_PUBLIC(panel),
																WIN_FRAME);
								Xv_Notice notice = xv_create(frame, NOTICE,
										NOTICE_MESSAGE_STRINGS,
											XV_MSG("Drag and Drop failed:"),
											XV_MSG("Unable to get contents of selection"),
											NULL,
										NOTICE_BUSY_FRAMES, frame, NULL,
										XV_SHOW, TRUE,
										NULL);

								xv_destroy(notice);
							}
							return;
						}

					}
				}
			}

			selection_insert(dp, sel_string, (long)sel_length, ip, panel, sp,
					insert_pos, orig_len, action, retstatus, rank,
					&caret_shift, &val_shift);
			/* If secondary selection, then tell the secondary selection holder
			 * that we are done with the selection.  If Quick Move, first tell
			 * the holder to delete the selection.
			 */
			if (rank == PANEL_SEL_SECONDARY) {
				int format;
				long length;

				if (action == ACTION_CUT) {
					/* Post delete request back to owner */
					xv_set(panel->sel_req, SEL_TYPE, panel->atom.delete, NULL);
					(void)xv_get(panel->sel_req, SEL_DATA, &length, &format);
				}
				/* We're done with the secondary selection */
				xv_set(panel->sel_req,
							SEL_TYPE, panel->atom.selection_end,
							NULL);
				sel_post_req_no_answer(panel->sel_req);
			}
			break;

		case ACTION_UNDO:
			/* Allow notify_proc to override editting characters. */
			/* Supports read-only text fields. */
			if (!ok_to_insert) {
				text_alarm(ip);
				return;
			}
			switch (dp->undo_direction) {
				case INSERT:
					undo_cnt = strlen(dp->undo_buffer);

					/* Shift everything to the right (including the NULL terminator) */
					for (i = orig_len; i >= insert_pos; i--)
						sp[i + undo_cnt] = sp[i];
					/* Insert the characters from the Undo Buffer */
					for (undo_index = 0, i = insert_pos; undo_index < undo_cnt;
							undo_index++, i++)
						sp[i] = dp->undo_buffer[undo_index];

					caret_shift = undo_cnt;
					if (dp->first_char)
						val_shift = undo_cnt;
					dp->undo_direction = DELETE;
					break;
				case DELETE:
					undo_cnt = strlen(dp->undo_buffer);
					for (i = insert_pos; i <= orig_len; i++)
						sp[i - undo_cnt] = sp[i];
					dp->undo_direction = INSERT;
					caret_shift = val_change = -undo_cnt;
					break;
				default: break;
			}

			break;

	  /**********************************************************************
       *     Navigation & Selection actions                                 *
       **********************************************************************/
		case ACTION_SELECT_CHAR_BACKWARD:
			selection_action = TRUE;
			/* ... fall through to ACTION_GO_CHAR_BACKWARD */
		case ACTION_GO_CHAR_BACKWARD:
			caret_shift = _xv_is_multibyte
						? -xv_utf8_prev_char_len(dp->value, dp->caret_position) 
				    	: -1;
			if ((dp->first_char) && (dp->caret_offset == 0))
				val_shift = -1;	/* display will include next char to left */
			dp->undo_direction = INVALID;	/* invalidate undo buffer */
			break;

		case ACTION_SELECT_CHAR_FORWARD:
			selection_action = TRUE;
			/* ... fall through to ACTION_GO_CHAR_FORWARD */
		case ACTION_GO_CHAR_FORWARD:
			caret_shift = _xv_is_multibyte
							? xv_utf8_next_char_len(dp->value, dp->caret_position) 
					    	: 1;
			/* display will include next char to right */
			if (dp->last_char < orig_len - 1
					&& dp->caret_offset == dp->value_offset) val_shift = 1;
			dp->undo_direction = INVALID;	/* invalidate undo buffer */
			break;

			/*  For all actions involving word movement, code was added
			 *  to calculate Kanji, Hiragana, and katakana word
			 *  separations.  For ASCII set we're going through the
			 *  original algorithm plus making sure we're not stepping
			 *  into other code sets.  For non-ASCII sets we do not
			 *  check for delimiters.
			 */

		case ACTION_SELECT_WORD_BACKWARD:
			selection_action = TRUE;
			/* ... fall through to ACTION_GO_WORD_BACKWARD */
		case ACTION_GO_WORD_BACKWARD:
			/* Skip back to start of current or previous word */
			if (insert_pos > orig_len)
				insert_pos -= (dp->first_char - 1);

			for (i = insert_pos - 1; i >= 0 && delim_table[(u_char) sp[i]];
					i--);
			for (; i >= 0 && !delim_table[(u_char) sp[i]]; i--);

			if (i < 0)
				i = 0;
			if (i > 0)
				i++;
			caret_shift = i - insert_pos;
			if (i < dp->first_char)
				/* Shift value so that caret is at correct character */
				val_shift = caret_shift;
			dp->undo_direction = INVALID;	/* invalidate undo buffer */
			break;

		case ACTION_SELECT_WORD_END:
			selection_action = TRUE;
			/* ... fall through to ACTION_GO_WORD_END */
		case ACTION_GO_WORD_END:
			if (insert_pos < orig_len) {
				/* Skip forward to end of current or next word */

				for (i = insert_pos;
						i < orig_len && delim_table[(u_char) sp[i]]; i++);
				for (; i < orig_len && !delim_table[(u_char) sp[i]]; i++);

				caret_shift = i - insert_pos;
				if (i > dp->last_char)
					/* Shift value so that caret is visible */
					val_shift = i - dp->last_char;
				dp->undo_direction = INVALID;	/* invalidate undo buffer */
			}
			break;

		case ACTION_GO_WORD_FORWARD:
			/* ACTION_GO_WORD_FORWARD is a SunView1 compatibility action */
			if (insert_pos < orig_len) {
				/* Skip forward to start of next word */

				for (i = insert_pos;
						i < orig_len && !delim_table[(u_char) sp[i]]; i++);
				for (; i < orig_len && delim_table[(u_char) sp[i]]; i++);

				caret_shift = i - insert_pos;
				if (i > dp->last_char)
					/* Shift value so that caret is visible */
					val_shift = i - dp->last_char;
				dp->undo_direction = INVALID;	/* invalidate undo buffer */
			}
			break;

		case ACTION_SELECT_LINE_START:
			SERVERTRACE((555, "SELECT_LINE_START\n"));
			selection_action = TRUE;
			/* ... fall through to ACTION_LINE_START */
		case ACTION_LINE_START:
			SERVERTRACE((555, "LINE_START\n"));
			if (insert_pos == 0)
				text_alarm(ip);	/* already at the start of the line */
			/* ... fall through */
		case ACTION_GO_LINE_FORWARD:
			SERVERTRACE((555, "GO_LINE_FORWARD\n"));
			/* ACTION_GO_LINE_FORWARD is a SunView1 compatibility action */
			/* Note: For ACTION_GO_LINE_FORWARD, the caret has already been
			 * advanced to the next line.  We now want to position it at
			 * the beginning of this line.
			 */
			caret_shift = -insert_pos;
			if (dp->first_char)
				val_shift = -dp->first_char;
			dp->undo_direction = INVALID;	/* invalidate undo buffer */

			/* adjust caret to point to first character because
			   update_value_offset() will determine first and last
			   character based upon caret position */
			dp->caret_position = dp->first_char;

			break;

		case ACTION_SELECT_LINE_END:
		case ACTION_SELECT_ALL:
			SERVERTRACE((555, "SELECT_LINE_END\n"));
			selection_action = TRUE;
			/* ... fall through to ACTION_LINE_END */
		case ACTION_LINE_END:
			SERVERTRACE((555, "LINE_END, ip=%p\n", ip));
			caret_shift = orig_len - insert_pos;
			if (!synthetic_event && action != ACTION_SELECT_ALL
					&& caret_shift == 0)
				text_alarm(ip);
			/* already at the end of the line */
			if (dp->last_char < orig_len - 1)
				val_shift = orig_len - dp->last_char;
			dp->undo_direction = INVALID;	/* invalidate undo buffer */
			break;

	  /**********************************************************************
       *     Printable characters                                           *
       **********************************************************************/
		default:
			if (panel_printable_char(action)) {
				dp->undo_direction = INVALID;	/* invalidate undo buffer */
				if (ok_to_insert) {	/* insert */

					/* DRA_CHANGED utf-8 */
					if (ev_str && *ev_str && strlen(ev_str) > 1) {
						if (orig_len + strlen(ev_str) - 1 < dp->stored_length) {
							/* there is room */
							char *p;
							int ll = 0;

							for (p = ev_str; *p; ll++, p++) {
								char_code = *p;
								for (i=orig_len+ll; i>insert_pos+ll; i--)
									sp[i] = sp[i - 1];
								sp[insert_pos + ll] = (CHAR) char_code;
							}
							caret_shift = ll;
							val_change = ll;
							sp[orig_len + ll] = '\0';
						}
						else	/* no more room */
							text_alarm(ip);
					}
					else {
						if (orig_len < dp->stored_length) {	/* there is room */
							char_code = action;
							for (i = orig_len; i > insert_pos; i--)
								sp[i] = sp[i - 1];
							sp[insert_pos] = (CHAR) char_code;
							caret_shift = 1;
							val_change = 1;
							sp[orig_len + 1] = '\0';
						}
						else	/* no more room */
							text_alarm(ip);
					}
				}
				else	/* must be read-only */
					text_alarm(ip);
			}
			break;

	}  /* switch (action) */
	SERVERTRACE((555, "after switch\n"));

	if (selection_action)
		orig_caret_position = dp->caret_position;

	/* determine the new caret offset and position */
	orig_offset = dp->value_offset;
	orig_text_rect_width = dp->text_rect.r_width;
	SERVERTRACE((555, "before update_value_offset\n"));
	update_value_offset(ip, val_change, val_shift, 1);
	dp->caret_offset = -1;	/* caret offset is no longer valid */
	update_caret_offset(ip, caret_shift, 0);
	update_text_rect(ip);
	SERVERTRACE((555, "after update_text_rect\n"));

	if (selection_action &&
			/* A selection keyboard command was issued */
			(dp->caret_position != orig_caret_position ||
					/* The caret moved */
					action == ACTION_SELECT_ALL))
	{
		struct timeval tv;
		Panel pan = PANEL_PUBLIC(panel);
		Xv_server srv = XV_SERVER_FROM_WINDOW(pan);

		SERVERTRACE((555, "before switch action\n"));
		/* caret_position references the character following the caret. */
		switch (action) {
			case ACTION_SELECT_ALL:
				dp->sel_first[PANEL_SEL_PRIMARY] = 0;

				dp->sel_last[PANEL_SEL_PRIMARY] = strlen(dp->value) - 1;
				break;
			case ACTION_SELECT_CHAR_FORWARD:
			case ACTION_SELECT_WORD_END:
			case ACTION_SELECT_LINE_END:
				SERVERTRACE((555, "     SELECT_LINE_END\n"));
				/* For forward movements, we don't want to include the character
				 * following the new caret position in the selection.
				 */
				if (dp->select_click_cnt[PANEL_SEL_PRIMARY] == 0) {
					dp->sel_first[PANEL_SEL_PRIMARY] = orig_caret_position;
					dp->sel_last[PANEL_SEL_PRIMARY] = dp->caret_position - 1;
				}
				else {
					if (dp->sel_last[PANEL_SEL_PRIMARY] <
							dp->caret_position - 1)
						/* We're extending the selection to the right */
						dp->sel_last[PANEL_SEL_PRIMARY] =
								dp->caret_position - 1;
					else if (dp->sel_first[PANEL_SEL_PRIMARY] ==
							orig_caret_position)
						/* We're shrinking the selection from the left */
						dp->sel_first[PANEL_SEL_PRIMARY] = dp->caret_position;
				}
				SERVERTRACE((555, "\n"));
				break;
			default:
				SERVERTRACE((555, "     default\n"));
				/*   For backward movements, we don't want to include the character
				 * after the old caret position in the selection.
				 *   Swap dp->sel_first with dp->sel_last so that dp->sel_first
				 * is <= dp->sel_last.
				 */
				if (dp->select_click_cnt[PANEL_SEL_PRIMARY] == 0) {
					dp->sel_last[PANEL_SEL_PRIMARY] = orig_caret_position - 1;
					dp->sel_first[PANEL_SEL_PRIMARY] = dp->caret_position;
				}
				else {
					if (dp->sel_first[PANEL_SEL_PRIMARY] > dp->caret_position)
						/* We're extending the selection to the left */
						dp->sel_first[PANEL_SEL_PRIMARY] = dp->caret_position;
					else if (dp->sel_last[PANEL_SEL_PRIMARY] ==
							orig_caret_position - 1)
						/* We're shrinking the selection from the right */
						dp->sel_last[PANEL_SEL_PRIMARY] =
								dp->caret_position - 1;
				}
				SERVERTRACE((555, "\n"));
				break;
		}
		/* Set variables to indicate that there's an active primary selection */
		dp->delete_pending = TRUE;
		dp->select_click_cnt[PANEL_SEL_PRIMARY] = 1;
		dp->select_is_word[PANEL_SEL_PRIMARY] = FALSE;
		SERVERTRACE((444, "scc[%d] = %d\n", PANEL_SEL_PRIMARY,
									dp->select_click_cnt[PANEL_SEL_PRIMARY]));
		/* Acquire the Primary Selection and highlight the text */

		server_set_timestamp(srv, &tv, 0L);
		xv_set(panel->sel_owner[PANEL_SEL_PRIMARY], SEL_TIME, &tv, NULL);
		if (xv_set(panel->sel_owner[PANEL_SEL_PRIMARY], SEL_OWN, TRUE, NULL)
				== XV_OK) {
			if (panel->sel_holder[PANEL_SEL_PRIMARY])
				text_seln_dehighlight(panel->sel_holder[PANEL_SEL_PRIMARY],
						PANEL_SEL_PRIMARY);
			panel->sel_holder[PANEL_SEL_PRIMARY] = ip;

		}
	}
	SERVERTRACE((555, "after switch2\n"));

	/* update the display */

	if (dp->mask == ' ' || hidden(ip)) return;
	else if (dp->mask ||
			action == ACTION_DRAG_COPY || action == ACTION_DRAG_MOVE ||
			action == ACTION_CUT || action == ACTION_PASTE
			|| action == ACTION_UNDO)
	{
		paint_value(ip, PV_HIGHLIGHT);
	}
	else {
		/* compute the position of the caret */
		x = dp->text_rect.r_left + dp->caret_offset;

		new_len = strlen(sp);

		was_clipped = dp->first_char || dp->last_char < new_len - 1;
		/* erase deleted characters that were displayed */
		if (new_len < orig_len) {
			/* repaint the whole value if needed */
			if (was_clipped || dp->text_rect.r_width != orig_text_rect_width)
				paint_value(ip, PV_HIGHLIGHT);
			else {
				/* clear the deleted characters and everything to the right */
				rect.r_left = x;
				rect.r_top = ip->value_rect.r_top;
				rect.r_width = orig_offset - dp->caret_offset;
				rect.r_height = ip->value_rect.r_height - 2 * LINE_Y;
				panel_clear_rect(panel, rect);
				PANEL_EACH_PAINT_WINDOW(panel, pw)
					panel_paint_text(pw,ip->value_fontset_id,ip->value_font_xid,
						ip->color_index, x,
						ip->value_rect.r_top + dp->font_home, &sp[insert_pos]);
				PANEL_END_EACH_PAINT_WINDOW
			}

		}
		else if (new_len > orig_len) {

			x_font_info = (XFontStruct *) xv_get(ip->value_font, FONT_INFO);

			/* DRA_CHANGED */
			if (val_change > 1) was_clipped = TRUE;

			/* repaint the whole value if it doesn't fit */
			if (was_clipped)
				paint_value(ip, PV_HIGHLIGHT);
			else {
				int dw, dh;
				/* write the new character to the left of the caret */
				if (_xv_is_multibyte) {
					clen = mblen(sp + insert_pos, MB_CUR_MAX);

					if (clen > 0) {
						pc_adv_x = XmbTextEscapement(ip->value_fontset_id,
										sp + insert_pos, clen);
					}
					else {
						pc_adv_x = 8; /* INCOMPLETE  */
					}
					XmbTextExtents(ip->value_fontset_id, &sp[insert_pos],
							(int)strlen(&sp[insert_pos]), &overall_ink_extents,
							&overall_logical_extents);
					dw = overall_logical_extents.width;
					dh = overall_logical_extents.height;
				}
				else {
					if (x_font_info->per_char) {
						pc_adv_x = x_font_info->per_char[(u_char) sp[insert_pos] -
								x_font_info->min_char_or_byte2].width;
					}
					else pc_adv_x = x_font_info->min_bounds.width;

					XTextExtents(x_font_info, &sp[insert_pos],
							(int)strlen(&sp[insert_pos]),
							&direction, &ascent, &descent, &overall);
					dw = overall.width;
					dh = ascent+descent;
				}

				PANEL_EACH_PAINT_WINDOW(panel, pw)
					DRAWABLE_INFO_MACRO(pw, info);
					XClearArea(xv_display(info), xv_xid(info),
							x - pc_adv_x, ip->value_rect.r_top,
							(unsigned)dw, (unsigned)dh, False);
					panel_paint_text(pw, ip->value_fontset_id,
							ip->value_font_xid,
							ip->color_index, x - pc_adv_x,
							ip->value_rect.r_top + dp->font_home,
							&sp[insert_pos]);
				PANEL_END_EACH_PAINT_WINDOW
			}
		}
		else {
			/* Cursor key causes display shift */
			if (val_shift)
				paint_value(ip, PV_HIGHLIGHT);
		}
	}
}

/*
 * Calculate the first and last char that will be visible in
 * the text field.  Note, if an editing action is occuring,
 * we want the cursor to be visible after the action has occured.
 * However, if user is clicking on the scrollbar, ignore the
 * caret position and shift the string in the specified
 * direction. In the future, it might be helpful to rewrite
 * update_value_offset() and update_caret_offset().
 *
 * Algorithm follows as such...
 *
 * Will text fit into field without scrollbuttons?
 *   Yes... then exit.
 *
 * Text will not fit into field without scrollbuttons...
 *   Determine what we feel will be first char that is visible.
 *   If 1st char visible isn't 1st char of text then add
 *     a scrollbutton to the left side.
 *   Calculate last character visible.
 *   If last character isn't last char of text then add
 *     a scrollbutton on the right side.
 *   After we've calculated first and last character that
 *     will be visible, determine if caret is still visible.
 *     If caret isn't visible, then we must make some additional
 *     adjustments to ensure that caret is still visible.
 */


static void update_value_offset(Item_info *ip, int val_change, int val_shift,
								int caret_sensitive)
	/* number of characters added (+) or deleted * (-) */
	/* number of characters to shift value * display */
{
	register Text_info *dp = TEXT_FROM_ITEM(ip);
	XFontStruct *x_font_info;
	int full_len;
	struct pr_size size;
	int max_caret = ip->value_rect.r_width;
	int i, x, y;
	int max_width;
	int cpos, clen;

	full_len = strlen(dp->value);
	size = xv_pf_textwidth(full_len, ip->value_font, dp->value);

	x_font_info = (XFontStruct *) xv_get(ip->value_font, FONT_INFO);

	if (size.x <= max_caret) {
		size = xv_pf_textwidth(full_len, ip->value_font, dp->value);
		dp->first_char = 0;
		dp->last_char = full_len - 1;
		dp->value_offset = size.x;
	}
	else {	/* there are more characters than can be
			   * displayed */

		if (val_change > 0) {
			/* Add a character */

			/*
			 * Inserted characters will always be visible and the caret is
			 * always positioned after the inserted character, unless the
			 * caret is already positioned after the last displayable
			 * character, in which case all the characters to the left of the
			 * inserted character are shifted to the left on the display.
			 */
			if (dp->caret_position > dp->last_char) {
				/*
				 * We are appending characters to the end of the string.
				 * Compute first_char = first character that can be fully
				 * displayed when the current (just-typed) character is the
				 * last character displayed.
				 */
				x = 0;
				max_width = max_caret - dp->scroll_btn_width;

				if (_xv_is_multibyte) {
					if (dp->caret_position < (int)strlen(dp->value) - 1)
						max_width -= dp->scroll_btn_width;
					i = dp->last_char;

					while (x < max_width) {
						int char_start = xv_utf8_current_char_offset(dp->value, i);
						clen = (i - char_start) + 1;
						x += XmbTextEscapement(ip->value_fontset_id,
											dp->value + char_start, clen);

						i = char_start - 1;
					}
				}
				else {
					if (dp->caret_position < (int)strlen(dp->value) - 1)
						max_width -= dp->scroll_btn_width;
					for (i = dp->caret_position; x < max_width; i--) {
						if (x_font_info->per_char) {
							x += x_font_info->per_char[(u_char) dp->value[i] -
									x_font_info->min_char_or_byte2].width;
						}
						else
							x += x_font_info->min_bounds.width;
					}
				}

				dp->first_char = i + 2;
			}
		}
		else if (val_change < 0) {
			/* Delete 1 or more characters */
			dp->first_char += val_change;
			if (dp->first_char < 0)
				dp->first_char = 0;	/* no more clip at left */
		}
		else {
			/* Shift the display */
			dp->first_char += val_shift;
		}

		/*
		 * dp->last_char = the last character in the string, starting from
		 * first_char, that can be fully displayed within the rectangle
		 * reserved for the text value string.  If the last character in the
		 * string cannot be displayed, then recompute dp->last_char to
		 * accomodate the right arrow scroll button.
		 */
		if (dp->first_char)
			max_caret -= dp->scroll_btn_width;

		dp->last_char = char_position(max_caret, ip->value_font,
				&dp->value[dp->first_char], FALSE) - 1 + dp->first_char;
		/*
		 * if last character isn't visible, then right scrollbutton
		 * must be added.
		 */
		if (dp->last_char < (int)strlen(dp->value) - 1) {
			/*
			 * Decrement dp->last_char until enough space has been made to
			 * draw the right arrow scroll button.
			 */
			for (x = 0; x < dp->scroll_btn_width;) {

				if (_xv_is_multibyte) {
					if (! IS_UTF8_CONT(dp->value[dp->last_char])) {
						clen = mblen(dp->value + dp->last_char, MB_CUR_MAX);
						x += XmbTextEscapement(ip->value_fontset_id,
										dp->value + dp->last_char, clen);
					}
					dp->last_char--;
				}
				else {
					if (x_font_info->per_char) {
						x +=
							x_font_info->per_char[(u_char)dp->value[
														dp->last_char--] -
							x_font_info->min_char_or_byte2].width;
					}
					else
						x += x_font_info->min_bounds.width;
				}
			}
			/*
			   After traversing through the above logic, the first and
			   last visible character have been calculated however
			   the caret may no longer be visible.  Therefore, if a char
			   was inserted or deleted, we need to make sure the caret
			   is visible.  If the caret isn't visible, then readjustment
			   of first and last char is necessary. (1063744)

			   Additionally, if no characters are inserted or deleted but
			   caret is moved, then we must also check to make sure that
			   caret falls between first and last char. (1073115)

			   Last argument passed to this function is "caret_sensitive"
			   If we're not caret_sensitive... meaning that we don't
			   care if the caret is visible, then don't do these
			   additional calculations.  Note: the only time we don't
			   care about seeing the cursor is after a scrollbutton
			   click.  After ANY caret navigation OR editing operation
			   the caret should be displayed i.e. fall between first
			   and last character of visible portion of text field.
			 */
			/* 1063744 */
			if (caret_sensitive) {
				cpos = dp->caret_position;
				if (val_shift)
					cpos += val_shift;
				if (val_change < 0)
					cpos += val_change;

				/*
				 * determine the char width between last char
				 * and the char that precedes caret.
				 */
				x = 0;
				if (_xv_is_multibyte) {
					while (cpos > dp->last_char + 1) {
						if (! IS_UTF8_CONT(dp->value[cpos])) {
							clen = mblen(dp->value + cpos, MB_CUR_MAX);
							x += XmbTextEscapement(ip->value_fontset_id,
									dp->value + cpos, clen);
						}
						cpos--;
					}
				}
				else {
					while (cpos > dp->last_char + 1) {
						if (x_font_info->per_char) {
							x += x_font_info->per_char[(u_char) dp->value[cpos]
									- x_font_info->min_char_or_byte2].width;
						}
						else
							x += x_font_info->min_bounds.width;

						cpos--;
					}
				}

				/*
				 * if caret past last character, then we've got some
				 * shifting to do.
				 */


				/*
				 * If 1st character is visible, then we
				 * must take into account the left scrollbutton
				 * width, i.e. before we start shifting the visible
				 * character string to the right, if we haven't
				 * already accounted for the left scrollbutton, then
				 * we must do that now.
				 */
				if (x > 0) {
					if (!dp->first_char) {
						for (y = 0; y < dp->scroll_btn_width;) {

							if (_xv_is_multibyte) {
								clen = mblen(dp->value + dp->first_char,
														MB_CUR_MAX);
								y += XmbTextEscapement(ip->value_fontset_id,
											dp->value + dp->first_char, clen);

								dp->first_char += clen;
							}
							else {
								if (x_font_info->per_char) {
									y +=
										x_font_info->
											per_char[(u_char) dp->value[dp->
									  							first_char] -
	  									x_font_info->min_char_or_byte2].width;
								}
								else y += x_font_info->min_bounds.width;

								dp->first_char++;
							}
						}
					}
				}	/* x>0 */

				/*
				 * okay... we've gotten this far... caret is still
				 * positioned past last char.  Start shifting the
				 * sting we're going to view until the caret is
				 * is visible.
				 */
				while (x > 0) {
					if (_xv_is_multibyte) {
						clen = mblen(dp->value + dp->first_char,
												MB_CUR_MAX);
						x -= XmbTextEscapement(ip->value_fontset_id,
										dp->value + dp->first_char, clen);

						dp->first_char += clen;
					}
					else {
						if (x_font_info->per_char) {
							x -=
									x_font_info->per_char[(u_char) dp->
												value[dp->first_char] -
									x_font_info->min_char_or_byte2].width;
						}
						else x -= x_font_info->min_bounds.width;

						dp->first_char++;
					}
				}

				/*
				 * caret should now be visible because we the first
				 * character we are displaying is now "closer" to the
				 * caret.  Now calculate the last character to be
				 * displayed.
				 */

				dp->last_char = char_position(max_caret, ip->value_font,
						&dp->value[dp->first_char], FALSE) - 1 + dp->first_char;


				/*
				 * And if the last character isn't the last char of
				 * the text field, then we need to put up the right
				 * scroll button
				 */

				if (_xv_is_multibyte) {
					if (dp->last_char < (int)strlen(dp->value) - 1) {
						for (x = 0; x < dp->scroll_btn_width;) {
							if (! IS_UTF8_CONT(dp->value[dp->last_char])) {
								clen = mblen(dp->value + dp->last_char,
												MB_CUR_MAX);
								x += XmbTextEscapement(ip->value_fontset_id,
											dp->value + dp->last_char, clen);
							}
							dp->last_char--;
						}
					}
				}
				else {
					if (dp->last_char < (int)strlen(dp->value) - 1) {
						for (x = 0; x < dp->scroll_btn_width;) {
							if (x_font_info->per_char) {
								x +=
									x_font_info->per_char[(u_char) dp->
									value[dp->last_char] -
									x_font_info->min_char_or_byte2].width;
							}
							else x += x_font_info->min_bounds.width;

							dp->last_char--;
						}
					}
				}


			}	/* caret_sensitive */
		}

		/* 1063744 */

		/* Compute value offset */

		size = xv_pf_textwidth(dp->last_char - dp->first_char + 1,
				ip->value_font, &dp->value[dp->first_char]);
		dp->value_offset = size.x;
	}
}

const Xv_pkg          xv_panel_text_pkg = {
    "Text Item", ATTR_PKG_PANEL,
    sizeof(Xv_panel_text),
    PANEL_ITEM,
    text_init,
    text_set_avlist,
    text_get_attr,
    text_destroy,
    NULL			/* no find proc */
};
