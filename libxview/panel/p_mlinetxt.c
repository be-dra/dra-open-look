#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)p_mlinetxt.c 1.32 93/06/28 DRA: $Id: p_mlinetxt.c,v 4.7 2024/12/02 20:00:49 dra Exp $";
#endif
#endif

/*
 *	(c) Copyright 1990 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL_NOTICE 
 *	file for terms of the license.
 */

/*
 * Multi-line Text Field Panel Item
 */
#include <string.h>
#include <xview_private/panel_impl.h>
#include <xview_private/draw_impl.h>
#include <xview_private/svr_impl.h>
#include <xview_private/txt_impl.h>
#include <xview/defaults.h>
#include <xview/scrollbar.h>
#include <xview/textsw.h>
#include <xview/win_notify.h>

/* Macros */
#define MLTXT_PRIVATE(item)	\
	XV_PRIVATE(Mltxt_info, Xv_panel_multiline_text, item)
#define MLTXT_PUBLIC(item)	XV_PUBLIC(item)

#define	MLTXT_FROM_ITEM(ip)	MLTXT_PRIVATE(ITEM_PUBLIC(ip))

/* Xview functions */
Xv_private void win_grab_quick_sel_keys(Xv_Window window);
Xv_private void win_ungrab_quick_sel_keys(Xv_Window window);

/* Panel Item Operations */
static void mltxt_paint(Panel_item item_public, Panel_setting);
static void mltxt_resize(Panel_item);
static void mltxt_remove(Panel_item);
static void mltxt_restore(Panel_item, Panel_setting u);
static void mltxt_layout(Panel_item, Rect *);
static void mltxt_accept_kbd_focus(Panel_item item_public);
static void mltxt_yield_kbd_focus(Panel_item item_public);

/* Local functions */
static void mltxt_advance_caret( Item_info	   *ip);
static void mltxt_backup_caret(Item_info *ip);
static Notify_value mltxt_notify_proc(Xv_Window	window, Notify_event ev, Notify_arg	arg, Notify_event_type type);
/* static void set_textsw_xy(); */

static int mltxt_key = 0;

static Defaults_pairs line_break_pairs[] = {
    { "Clip", (int) PANEL_WRAP_AT_CHAR },
    { "Wrap_char", (int) PANEL_WRAP_AT_CHAR },
    { "Wrap_word", (int) PANEL_WRAP_AT_WORD },
    { NULL, (int) PANEL_WRAP_AT_WORD }
};

typedef struct {
    Panel_item	    public_self;   /* back pointer to object */
    int		    columns;	   /* textsw width in characters */
    Xv_Window	    focus_pw;	   /* panel->focus_pw when keyboard focus was
				    * accepted */
    int		    length;	   /* Max # of chars to store in textsw */
    Panel_setting   line_break_action;
    Panel_setting   notify_level;
    int		    rows_displayed;
    Scrollbar	    sb;
    CHAR	   *terminators;
    Textsw	    textsw;
    Xv_Window   view;	   /* cache view handle */
    CHAR	   *value;
    int		    value_size;	   /* size of malloc'ed value buffer */
    int		    width;	   /* textsw width in pixels */
    int		    read_only;	   /* is in read-only mode? */
#ifdef OW_I18N
    char	   *mbs_value;
    int		    mbs_value_size;
    char	   *mbs_terminators;
    unsigned	    stored_length_wc : 1; /* Indicate whether the storage
			                   * length is measured in multibyte
				           * or wide chars
				           */
#endif /* OW_I18N */
	Menu_item cutitem, copyitem, deleteitem;
	int restricted_menu;
} Mltxt_info;

static int item_key = 0; 

/* This was necessary to make the 'quick operations' work */
static void mltxt_accept_key(Panel_item item, Event *event)
{
	if (event_action(event) == ACTION_PASTE) {
    	Mltxt_info *dp = MLTXT_PRIVATE(item);

		textsw_process_event(dp->view, event, 0L);
	}
	else if (event_action(event) == ACTION_CUT) {
    	Mltxt_info *dp = MLTXT_PRIVATE(item);

		textsw_process_event(dp->view, event, 0L);
	}
}

static Panel_ops ops = {
    panel_default_handle_event,	/* handle_event() */
    NULL,				/* begin_preview() */
    NULL,				/* update_preview() */
    NULL,				/* cancel_preview() */
    NULL,				/* accept_preview() */
    NULL,				/* accept_menu() */
    mltxt_accept_key,				/* orig: NULL accept_key() */
    panel_default_clear_item,		/* clear() */
    mltxt_paint,			/* paint() */
    mltxt_resize,			/* resize() */
    mltxt_remove,			/* remove() */
    mltxt_restore,			/* restore() Orig NULL */
    mltxt_layout,			/* layout() */
    mltxt_accept_kbd_focus,		/* accept_kbd_focus() */
    mltxt_yield_kbd_focus,		/* yield_kbd_focus() */
    NULL				/* extension: reserved for future use */
};


/* ========================================================================= */

/* -------------------- XView Functions  -------------------- */
static int panel_mltxt_init(Panel panel_public, Panel_item item_public, Attr_avlist avlist, int *unused)
{
	Panel_info *panel = PANEL_PRIVATE(panel_public);
	register Item_info *ip = ITEM_PRIVATE(item_public);
	Xv_panel_multiline_text *item_object =
			(Xv_panel_multiline_text *) item_public;
	Mltxt_info *dp = xv_alloc(Mltxt_info);
	Attr_attribute *attrs;

	if (item_key == 0) {
		item_key = xv_unique_key();
	}

	item_object->private_data = (Xv_opaque) dp;
	dp->public_self = item_public;

	ip->ops = ops;
	if (panel->event_proc)
		ip->ops.panel_op_handle_event =
				(void (*)(Panel_item, Event *))panel->event_proc;

	ip->item_type = PANEL_MULTILINE_TEXT_ITEM;
	ip->flags |= DEAF	/* Item doesn't want any events */
			| WANTS_KEY	/* Item accepts the keyboard focus */
			| WANTS_ISO;	/* Item wants all ISO characters */
	if (ip->notify == panel_nullproc)
		ip->notify = (int (*)(Panel_item, Event *))panel_text_notify;

	panel_set_bold_label_font(ip);

	if (ip->notify == panel_nullproc)
		ip->notify = (int (*)(Panel_item, Event *))panel_text_notify;

	dp->columns = 40;
	dp->line_break_action = (Panel_setting) defaults_get_enum("text.lineBreak",
			"Text.LineBreak", line_break_pairs);
	dp->notify_level = PANEL_SPECIFIED;
	dp->rows_displayed = 5;	/* enough for a full size scrollbar elevator in
							   the default 12 point font */
	dp->restricted_menu = TRUE;

	for (attrs=avlist; *attrs; attrs=attr_next(attrs)) switch ((int)*attrs) {
		case PANEL_KEEP_FULL_MENU:
			dp->restricted_menu = ! (int)attrs[1];
			ATTR_CONSUME(*attrs);
			break;
	}


#ifdef OW_I18N
	dp->stored_length_wc = 0;
	dp->terminators = (wchar_t *)_xv_mbstowcsdup("\n\r\t");
#else
	dp->terminators = (char *)panel_strsave("\n\r\t");
#endif /* OW_I18N */

	dp->textsw = xv_create(panel_public, TEXTSW,
			TEXTSW_DISABLE_CD, TRUE,
			TEXTSW_DISABLE_LOAD, TRUE,
			TEXTSW_IGNORE_LIMIT, TEXTSW_INFINITY,
			TEXTSW_RESTRICT_MENU, dp->restricted_menu,
			WIN_CMS, xv_get(panel_public, WIN_CMS),
			WIN_COLUMNS, dp->columns,
			WIN_ROWS, dp->rows_displayed,
			XV_KEY_DATA, FRAME_ORPHAN_WINDOW, TRUE,
			NULL);

	dp->view = xv_get(dp->textsw, OPENWIN_NTH_VIEW, 0);
	dp->sb = xv_get(dp->textsw, WIN_VERTICAL_SCROLLBAR);
	xv_set(dp->sb, SCROLLBAR_SPLITTABLE, FALSE, NULL);
	dp->length = (int)xv_get(dp->textsw, TEXTSW_MEMORY_MAXIMUM);
	dp->width = (int)xv_get(dp->textsw, XV_WIDTH);
	dp->read_only = FALSE;

	xv_set(dp->view,
			WIN_NOTIFY_SAFE_EVENT_PROC, mltxt_notify_proc,
			XV_KEY_DATA, item_key, item_public,
			WIN_CURSOR, (Cursor) xv_get(panel_public, WIN_CURSOR),
			NULL);

	/* 
	 * let go of grab so that Textsw can get select events 
	 * while parented to the Panel
	 */
	xv_set(panel_public, WIN_UNGRAB_SELECT, NULL);
	win_ungrab_quick_sel_keys(dp->view);

	/* make a First-Class (primary) focus client (1085375) */
	xv_set(item_public,
			PANEL_PAINT, PANEL_NONE,
			XV_FOCUS_RANK, XV_FOCUS_PRIMARY,
			NULL);
	xv_set(panel_public, XV_FOCUS_RANK, XV_FOCUS_PRIMARY, NULL);

	return XV_OK;
}

static Menu note_text_gen(Menu menu, Menu_generate op)
{
	if (op == MENU_DISPLAY) {
		Panel_item it = xv_get(menu, XV_KEY_DATA, mltxt_key);
		Mltxt_info *dp = MLTXT_PRIVATE(it);
		char buf[10];
		int sellen = textsw_get_primary_selection(dp->textsw, buf,
						(unsigned)sizeof(buf), NULL, NULL);

		if (dp->cutitem) xv_set(dp->cutitem, MENU_INACTIVE, sellen == 0, NULL);
		if (dp->copyitem) xv_set(dp->copyitem, MENU_INACTIVE, sellen==0, NULL);
		if (dp->deleteitem) xv_set(dp->deleteitem, MENU_INACTIVE, sellen==0, NULL);
	}
	return menu;
}

static void note_text_delete(Menu menu, Menu_item item)
{
	Textsw tsw = xv_get(item, XV_KEY_DATA, mltxt_key);
	char buf[10];
	Textsw_index s, e;
	int sellen = textsw_get_primary_selection(tsw, buf, (unsigned)sizeof(buf),
										&s, &e);

	if (sellen == 0) return;

	textsw_erase(tsw, s, e);
}

static void prepare_own_menu(Panel_item item, Mltxt_info *dp)
{
	Menu menu; /* of TEXTSW */

	if (! mltxt_key) mltxt_key = xv_unique_key();
	menu = xv_get(dp->textsw, WIN_MENU);
	if (dp->restricted_menu) {
		dp->cutitem = xv_get(menu, MENU_NTH_ITEM, 3),
		dp->copyitem = xv_get(menu, MENU_NTH_ITEM, 4);

		dp->deleteitem = xv_create(XV_NULL, MENUITEM,
				MENU_RELEASE,
				MENU_STRING, XV_MSG("Delete"),
				MENU_NOTIFY_PROC, note_text_delete,
				XV_KEY_DATA, mltxt_key, dp->textsw,
				NULL);

		xv_set(menu,
				MENU_GEN_PROC, note_text_gen,
				XV_KEY_DATA, mltxt_key, item,
				MENU_APPEND_ITEM, dp->deleteitem,
				NULL);

		xv_set(menu, XV_SET_MENU, dp->textsw, NULL);
	}

	xv_set(item, PANEL_ITEM_MENU, menu, NULL);
}

static Xv_opaque panel_mltxt_set_avlist(Panel_item item,
    Attr_avlist avlist)
{
	register Item_info *ip = ITEM_PRIVATE(item);
	register Mltxt_info *dp = MLTXT_PRIVATE(item);
	Attr_avlist attrs;
	Panel_info *panel = ip->panel;
	Xv_opaque result;
	int rows;
	Textsw_enum textsw_line_break_action = TEXTSW_WRAP_AT_WORD;
	Bool xv_end_create = FALSE;

	/*
	 * If a client has called panel_item_parent this item may not have a
	 * parent -- do nothing in this case
	 */
	if (panel == NULL)
		return ((Xv_opaque) XV_ERROR);

	/* Pre-parse PANEL_INACTIVE.  If we have to advance the caret out
	 * of the multiline text item's textsw, then we must call 
	 * mltxt_advance_caret.
	 */
	for (attrs = avlist; *attrs; attrs = attr_next(attrs)) {
		switch (attrs[0]) {
			case PANEL_INACTIVE:
				if ((int)attrs[1] && panel->kbd_focus_item == ip)
					mltxt_advance_caret(ip);
				if (dp->sb)
					xv_set(dp->sb, SCROLLBAR_INACTIVE, avlist[1], NULL);
				break;
		}
	}

	if (*avlist != XV_END_CREATE) {
		/* Parse Panel Item Generic attributes before Text Field attributes.
		 * Prevent panel_redisplay_item from being called in item_set_avlist.
		 */
		panel->no_redisplay_item = TRUE;
		result = xv_super_set_avlist(item, &xv_panel_multiline_text_pkg,
				avlist);
		panel->no_redisplay_item = FALSE;
		if (result != XV_OK)
			return result;
	}

	for (; *avlist; avlist = attr_next(avlist)) {
		switch (avlist[0]) {
			case PANEL_DISPLAY_ROWS:
				dp->rows_displayed = (int)avlist[1];
				xv_set(dp->textsw, WIN_ROWS, avlist[1], NULL);
				break;

			case PANEL_ITEM_COLOR:
				xv_set(dp->textsw, WIN_FOREGROUND_COLOR, avlist[1], NULL);
				break;

			case PANEL_LINE_BREAK_ACTION:
				dp->line_break_action = (Panel_setting) avlist[1];
				switch (dp->line_break_action) {
					case PANEL_WRAP_AT_CHAR:
						textsw_line_break_action = TEXTSW_WRAP_AT_CHAR;
						break;
					case PANEL_WRAP_AT_WORD:
						textsw_line_break_action = TEXTSW_WRAP_AT_WORD;
						break;
					default:
						break;
				}
				xv_set(dp->textsw,
						TEXTSW_LINE_BREAK_ACTION, textsw_line_break_action,
						NULL);
				break;

			case PANEL_NOTIFY_LEVEL:
				dp->notify_level = (Panel_setting) avlist[1];
				break;

#ifdef OW_I18N
			case PANEL_NOTIFY_STRING:
				if (dp->terminators)
					xv_free(dp->terminators);
				dp->terminators =
						(wchar_t *) _xv_mbstowcsdup((char *)avlist[1]);
				break;

			case PANEL_NOTIFY_STRING_WCS:
				if (dp->terminators)
					xv_free(dp->terminators);
				dp->terminators =
						(wchar_t *) panel_strsave_wc((wchar_t *) avlist[1]);
				break;
#else
			case PANEL_NOTIFY_STRING:
				if (dp->terminators)
					free(dp->terminators);
				dp->terminators = (char *)panel_strsave((char *) avlist[1]);
				break;
#endif /* OW_I18N */

			case PANEL_READ_ONLY:
				xv_set(dp->textsw, TEXTSW_READ_ONLY, (int)avlist[1], NULL);
				dp->read_only = (int)avlist[1];
				break;

#ifdef OW_I18N
			case PANEL_ITEM_IC_ACTIVE:
				if ((int)avlist[1])
					ip->flags |= IC_ACTIVE;
				else
					ip->flags &= ~IC_ACTIVE;
				if (ic_active(ip) == FALSE)
					xv_set(dp->textsw, WIN_IC_ACTIVE, FALSE, NULL);
				else if (xv_get(dp->textsw, WIN_IC_ACTIVE) == FALSE)
					xv_set(dp->textsw, WIN_IC_ACTIVE, TRUE, NULL);
				break;

			case PANEL_VALUE:
				if (dp->value)
					xv_free(dp->value);
				dp->value = (wchar_t *) _xv_mbstowcsdup((char *)avlist[1]);
				dp->value_size = wslen(dp->value) + 1;
				if (dp->read_only)
					xv_set(dp->textsw, TEXTSW_READ_ONLY, FALSE, NULL);
				textsw_reset(dp->textsw, 0, 0);
				xv_set(dp->textsw, TEXTSW_CONTENTS_WCS, dp->value, NULL);
				if (dp->read_only)
					xv_set(dp->textsw, TEXTSW_READ_ONLY, TRUE, NULL);
				break;

			case PANEL_VALUE_WCS:
				if (dp->value)
					xv_free(dp->value);
				dp->value = (wchar_t *) panel_strsave_wc((u_char *) avlist[1]);
				dp->value_size = wslen(dp->value) + 1;
				if (dp->read_only)
					xv_set(dp->textsw, TEXTSW_READ_ONLY, FALSE, NULL);
				textsw_reset(dp->textsw, 0, 0);
				xv_set(dp->textsw, TEXTSW_CONTENTS_WCS, dp->value, NULL);
				if (dp->read_only)
					xv_set(dp->textsw, TEXTSW_READ_ONLY, TRUE, NULL);
				break;
#else
			case PANEL_VALUE:
				if (dp->value)
					free((char *)dp->value);
				dp->value = (char *)panel_strsave((char *) avlist[1]);
				dp->value_size = strlen(dp->value) + 1;

				/* 
				 * the Textsw thinks that READ_ONLY applys to the
				 * programmer as well as the user (1066989)...
				 */
				if (dp->read_only)
					xv_set(dp->textsw, TEXTSW_READ_ONLY, FALSE, NULL);

				textsw_reset(dp->textsw, 0, 0);
				xv_set(dp->textsw, TEXTSW_CONTENTS, avlist[1], NULL);

				if (dp->read_only)
					xv_set(dp->textsw, TEXTSW_READ_ONLY, TRUE, NULL);
				break;
#endif /* OW_I18N */

			case PANEL_VALUE_DISPLAY_LENGTH:
				dp->columns = (int)avlist[1];
				dp->width = xv_cols(dp->textsw, dp->columns);
				xv_set(dp->textsw, WIN_COLUMNS, (int)avlist[1], NULL);
				break;

			case PANEL_VALUE_DISPLAY_WIDTH:
				dp->width = (int)avlist[1];
				xv_set(dp->textsw, XV_WIDTH, (int)avlist[1], NULL);
				dp->columns = (int)xv_get(dp->textsw, WIN_COLUMNS);
				break;

#ifdef OW_I18N
			case PANEL_VALUE_STORED_LENGTH:
				dp->length = (int)avlist[1];
				dp->stored_length_wc = 0;
				xv_set(dp->textsw, TEXTSW_MEMORY_MAXIMUM, (int)avlist[1], NULL);
				break;

			case PANEL_VALUE_STORED_LENGTH_WCS:
				dp->length = (int)avlist[1] * sizeof(wchar_t);
				dp->stored_length_wc = 1;
				xv_set(dp->textsw, TEXTSW_MEMORY_MAXIMUM, (int)avlist[1], NULL);
				break;
#else
			case PANEL_VALUE_STORED_LENGTH:
				dp->length = (int)avlist[1];
				xv_set(dp->textsw, TEXTSW_MEMORY_MAXIMUM, (int)avlist[1], NULL);
				break;
#endif /* OW_I18N */

			case XV_END_CREATE:
				xv_end_create = TRUE;
				break;

			case XV_SHOW:
				xv_set(dp->textsw, XV_SHOW, (int)avlist[1], NULL);
				break;

			default:
				break;
		}
	}

	if (xv_end_create) {
		char *hlp;

		prepare_own_menu(item, dp);

		hlp = (char *)xv_get(item, XV_HELP_DATA);
		if (hlp) {
			xv_set(dp->view, XV_HELP_DATA, hlp, NULL);
		}
		mltxt_resize(item);
	}

	if (created(ip)) {
		rows = (int)xv_get(dp->textsw, WIN_ROWS);
		/* BUG ALERT:  Change the following line when textsw supports
		 *         removal of its scrollbar.  This will get rid of
		 *         the extra white space to the left or right of the
		 *         box enclosing the text.  (See bugid 1041326.)
		 */
		xv_set(dp->sb, XV_SHOW, rows * dp->columns < dp->length, NULL);
	}

	ip->value_rect.r_width = (int)xv_get(dp->textsw, XV_WIDTH);
	ip->value_rect.r_height = (int)xv_get(dp->textsw, XV_HEIGHT);
	ip->rect = panel_enclosing_rect(&ip->label_rect, &ip->value_rect);
	panel_check_item_layout(ip);

	return XV_OK;
}


/*ARGSUSED*/
static Xv_opaque panel_mltxt_get_attr(Panel_item item_public, int *status, Attr_attribute which_attr, va_list valist)
{
	register Mltxt_info *dp = MLTXT_PRIVATE(item_public);
	int length;	/* # of chars in textsw, plus NULL terminator */

	switch (which_attr) {
		case PANEL_DISPLAY_ROWS:
			return (Xv_opaque) dp->rows_displayed;

		case PANEL_ITEM_NTH_WINDOW:
			switch (va_arg(valist, int))
			{
				case 0:
					return dp->view;
				case 1:
					if (xv_get(dp->sb, XV_SHOW) == TRUE)
						return (Xv_opaque) dp->sb;
					else
						/* BUG ALERT:  The dead space left by the hidden scrollbar
						 * does not receive events.  xscope does not show any
						 * events when you click SELECT over this area.  What is it?
						 * Who knows.  If you click MENU over this area, you get
						 * the Window Manager's window menu.
						 * For now, let's pass back the textsw (openwin) handle.
						 */
						return (Xv_opaque) dp->textsw;
				default:
					return (Xv_opaque) NULL;
			}

		case PANEL_ITEM_NWINDOWS:
			return (Xv_opaque) 2;

		case PANEL_LINE_BREAK_ACTION:
			return (Xv_opaque) dp->line_break_action;

		case PANEL_NOTIFY_LEVEL:
			return (Xv_opaque) dp->notify_level;

#ifdef OW_I18N
		case PANEL_NOTIFY_STRING:
			if (dp->mbs_terminators)
				xv_free(dp->mbs_terminators);
			dp->mbs_terminators =
					(char *)_xv_wcstombsdup((wchar_t *)dp->terminators);
			return (Xv_opaque) dp->mbs_terminators;

		case PANEL_NOTIFY_STRING_WCS:
			return (Xv_opaque) dp->terminators;
#else
		case PANEL_NOTIFY_STRING:
			return (Xv_opaque) dp->terminators;
#endif /* OW_I18N */

		case PANEL_READ_ONLY:
			return dp->read_only;

#ifdef OW_I18N
		case PANEL_ITEM_IC_ACTIVE:
			{
				register Item_info *ip = ITEM_PRIVATE(item_public);

				return ic_active(ip);
			}

		case PANEL_VALUE:
			length = (int)xv_get(dp->textsw, TEXTSW_LENGTH) + 1;
			if (length > dp->mbs_value_size) {
				if (dp->mbs_value)
					xv_free(dp->mbs_value);
				dp->mbs_value = xv_malloc(length);
				dp->mbs_value_size = length;
			}
			xv_get(dp->textsw, TEXTSW_CONTENTS, 0, dp->mbs_value, length - 1);
			dp->mbs_value[length - 1] = 0;	/* NULL terminate the string */
			return (Xv_opaque) dp->mbs_value;

		case PANEL_VALUE_WCS:
			length = (int)xv_get(dp->textsw, TEXTSW_LENGTH_WC) + 1;
			if (length > dp->value_size) {
				if (dp->value)
					xv_free(dp->value);
				dp->value = xv_malloc(length * sizeof(wchar_t));
				dp->value_size = length;
			}
			xv_get(dp->textsw, TEXTSW_CONTENTS_WCS, 0, dp->value, length - 1);
			dp->value[length - 1] = 0;	/* NULL terminate the string */
			return (Xv_opaque) dp->value;
#else
		case PANEL_VALUE:
			length = (int)xv_get(dp->textsw, TEXTSW_LENGTH) + 1;
			if (length > dp->value_size) {
				if (dp->value)
					free((char *)dp->value);
				dp->value = xv_malloc((size_t)length);
				dp->value_size = length;
			}
			xv_get(dp->textsw, TEXTSW_CONTENTS, 0, dp->value, length - 1);
			dp->value[length - 1] = 0;	/* NULL terminate the string */
			return (Xv_opaque) dp->value;
#endif /* OW_I18N */

		case PANEL_VALUE_DISPLAY_LENGTH:
			return (Xv_opaque) dp->columns;

		case PANEL_VALUE_DISPLAY_WIDTH:
			return (Xv_opaque) dp->width;

#ifdef OW_I18N
		case PANEL_VALUE_STORED_LENGTH:
			if (dp->stored_length_wc == 0)
				return (Xv_opaque) dp->length;
			else
				return (-1);

		case PANEL_VALUE_STORED_LENGTH_WCS:
			if (dp->stored_length_wc == 1)
				return (Xv_opaque) dp->length / sizeof(wchar_t);
			else
				return (-1);
#else
		case PANEL_VALUE_STORED_LENGTH:
			return (Xv_opaque) dp->length;
#endif /* OW_I18N */

		default:
			*status = XV_ERROR;
			return (Xv_opaque) 0;
	}
}


static int panel_mltxt_destroy(Panel_item item_public, Destroy_status status)
{
	Mltxt_info *dp = MLTXT_PRIVATE(item_public);

	if (status == DESTROY_CHECKING || status == DESTROY_SAVE_YOURSELF)
		return XV_OK;

	mltxt_remove(item_public);
	xv_destroy(dp->textsw);

	if (dp->value)
		free((char *)dp->value);

	if (dp->terminators)
		xv_free(dp->terminators);

#ifdef OW_I18N
	if (dp->mbs_value)
		xv_free(dp->mbs_value);
	if (dp->mbs_terminators)
		xv_free(dp->mbs_terminators);
#endif /* OW_I18N */

	free((char *)dp);

	return XV_OK;
}



/* --------------------  Panel Item Operations  -------------------- */
static void mltxt_paint(Panel_item item_public, Panel_setting s)
{
    Item_info      *ip = ITEM_PRIVATE(item_public);
    Mltxt_info	   *dp = MLTXT_PRIVATE(item_public);

    panel_text_paint_label(ip);
    (void) win_post_id( dp->view, WIN_REPAINT, NOTIFY_SAFE );
}


static void mltxt_resize( Panel_item	    item_public)
{
    Mltxt_info	   *dp = MLTXT_PRIVATE(item_public);
    Item_info      *ip = ITEM_PRIVATE(item_public);

    /* Panel window may have been moved: reset TEXTSW's x,y coordinates */
    xv_set(dp->textsw,
	   XV_X, ip->value_rect.r_left,
	   XV_Y, ip->value_rect.r_top,
	   NULL);

    /* work around for 1090245, Textsw doesn't update the
     * position of it's drop site when XV_X/Y change...
     */
   (void) win_post_id(dp->view, WIN_RESIZE, NOTIFY_SAFE);
}


static void mltxt_remove(Panel_item	item_public)
{
	Item_info *ip = ITEM_PRIVATE(item_public);
	Panel_info *panel = ip->panel;
    Mltxt_info *dp = MLTXT_PRIVATE(item_public);

	xv_set(dp->textsw, XV_SHOW, FALSE, NULL);
	/*
	 * Only reassign the keyboard focus to another item if the panel isn't
	 * being destroyed.
	 */
	if (!panel->status.destroying && panel->kbd_focus_item == ip) {
		panel->kbd_focus_item = panel_next_kbd_focus(panel, TRUE);
		panel_accept_kbd_focus(panel);
	}
}

static void mltxt_restore(Panel_item item_public, Panel_setting u)
{
	Item_info *ip = ITEM_PRIVATE(item_public);
	Panel panel = PANEL_PUBLIC(ip->panel);
    Mltxt_info *dp = MLTXT_PRIVATE(item_public);

	xv_set(dp->textsw, XV_SHOW, FALSE, NULL);

	if (! xv_get(panel, PANEL_CARET_ITEM)) {
		xv_set(panel, PANEL_CARET_ITEM, item_public, NULL);
	}
}

static void mltxt_layout(Panel_item item_public, Rect * deltas)
{
    Mltxt_info	   *dp = MLTXT_PRIVATE(item_public);
    Item_info      *ip = ITEM_PRIVATE(item_public);

    if (!created(ip)) return;
    xv_set(dp->textsw,
	   XV_X, xv_get(dp->textsw, XV_X) + deltas->r_left,
	   XV_Y, xv_get(dp->textsw, XV_Y) + deltas->r_top,
	   NULL);
}


static void mltxt_accept_kbd_focus(Panel_item item_public)
{
    Mltxt_info	   *dp = MLTXT_PRIVATE(item_public);
    Item_info      *ip = ITEM_PRIVATE(item_public);

    dp->focus_pw = ip->panel->focus_pw;  /* save focus paint window */
    xv_set(dp->textsw, WIN_SET_FOCUS, NULL);
    win_ungrab_quick_sel_keys( ip->panel->focus_pw );
    win_grab_quick_sel_keys( dp->view );
}


static void mltxt_yield_kbd_focus(Panel_item item_public)
{
    Mltxt_info	   *dp = MLTXT_PRIVATE(item_public);

    xv_set(dp->textsw, WIN_REMOVE_CARET, NULL);
    /* BUG ALERT: Need a way to determine if the textsw has the input focus. */
    /* if (dp->textsw has input focus) */
	if (dp->focus_pw) {
	    xv_set(dp->focus_pw, WIN_SET_FOCUS, NULL);
	    win_ungrab_quick_sel_keys( dp->view );
	    win_grab_quick_sel_keys( dp->focus_pw );
	    dp->focus_pw = XV_NULL;
	}
}



/* --------------------  Local Routines  -------------------- */
static void mltxt_advance_caret( Item_info	   *ip)
{
    Mltxt_info	   *dp = MLTXT_FROM_ITEM(ip);

    ip->panel->focus_pw = dp->focus_pw;
    ip->panel->status.focus_item_set = TRUE;
       /* ... used by panel_show_focus_win, which will be called by
	* panel_accept_kbd_focus before the KBD_USE is received
	* on the panel paint window.  (The KBD_USE happens as a 
	* result of mltxt_yield_kbd_focus doing a WIN_SET_FOCUS.)
	*/
    panel_advance_caret(PANEL_PUBLIC(ip->panel));
}


static void mltxt_backup_caret(Item_info *ip)
{
    Mltxt_info	   *dp = MLTXT_FROM_ITEM(ip);

    ip->panel->focus_pw = dp->focus_pw;
    ip->panel->status.focus_item_set = TRUE;
       /* ... used by panel_show_focus_win, which will be called by
	* panel_accept_kbd_focus before the KBD_USE is received
	* on the panel paint window.  (The KBD_USE happens as a 
	* result of mltxt_yield_kbd_focus doing a WIN_SET_FOCUS.)
	*/
    panel_backup_caret(PANEL_PUBLIC(ip->panel));
}

static int notify_user(Mltxt_info *dp, Event *event);

static Notify_value mltxt_notify_proc(Xv_Window	window, Notify_event ev,
							Notify_arg	arg, Notify_event_type type)
{
    Event *event = (Event	*)ev;
	Panel_item item_public;
	Mltxt_info *dp;
	Item_info *ip;
	int last;
	Panel_setting notify_rtn_code;
	int ok_to_insert;
	Notify_value result;
	Textsw textsw;

	item_public = xv_get(window, XV_KEY_DATA, item_key);
	ip = ITEM_PRIVATE(item_public);
	if (inactive(ip) && event_action(event) != WIN_REPAINT)
		return NOTIFY_DONE;
	dp = MLTXT_PRIVATE(item_public);
	if (event_is_down(event)) {
		switch (event_action(event)) {
			case ACTION_NEXT_ELEMENT:
				mltxt_advance_caret(ip);
				return NOTIFY_DONE;
			case ACTION_PREVIOUS_ELEMENT:
				mltxt_backup_caret(ip);
				return NOTIFY_DONE;
			case ACTION_PANEL_START:
			case ACTION_PANEL_END:
				ip->panel->focus_pw = dp->focus_pw;
				ip->panel->status.focus_item_set = TRUE;
				return panel_default_event(PANEL_PUBLIC(ip->panel), event, arg);
			case ACTION_SELECT:
				if (ip->panel->kbd_focus_item != ip)
					panel_set_kbd_focus(ip->panel, ip);
				break;
		}
	}

#ifdef OW_I18N
	/*  Also need to check whether input is in event->ie_string */

	if (!event_is_string(event) && !event_is_iso(event)
			&& event_action(event) != ACTION_NEXT_ELEMENT &&
#else
	if (!event_is_iso(event) && event_action(event) != ACTION_NEXT_ELEMENT &&
#endif /* OW_I18N */

			event_action(event) != ACTION_PREVIOUS_ELEMENT) {
		result = notify_next_event_func(window, ev, arg, type);
		if ((event_action(event) == ACTION_CUT
						&& dp->notify_level != PANEL_NONE)
				|| (event_action(event) == ACTION_PASTE
						&& dp->notify_level == PANEL_ALL))
		{
			(*ip->notify) (item_public, event);
		}
		return result;
	}
	textsw = xv_get(window, WIN_PARENT);
	if (notify_user(dp, event)) {
		notify_rtn_code = (Panel_setting) (*ip->notify) (item_public, event);
	}
	else {
		notify_rtn_code = panel_text_notify(item_public, event);
	}
	ok_to_insert = notify_rtn_code == PANEL_INSERT;
	if (event_is_down(event)) {
		switch (event_action(event)) {
			case ACTION_GO_CHAR_FORWARD:
			case ACTION_GO_CHAR_BACKWARD:
			case ACTION_GO_WORD_END:
			case ACTION_GO_WORD_FORWARD:
			case ACTION_GO_WORD_BACKWARD:
				notify_rtn_code = PANEL_INSERT;
				ok_to_insert = TRUE;
				break;
			case ACTION_GO_LINE_BACKWARD:
				/* Go to start of this or previous line */

#ifdef OW_I18N
				/* should use TEXTSW_INSERTION_POINT_WC for the performance */
				if (xv_get(textsw, TEXTSW_INSERTION_POINT_WC) == 0)
#else
				if (xv_get(textsw, TEXTSW_INSERTION_POINT) == 0)
#endif

				{
					notify_rtn_code = PANEL_PREVIOUS;
					ok_to_insert = FALSE;
				}
				else
					ok_to_insert = TRUE;
				break;
			case ACTION_GO_LINE_END:
				/* Go to end of this or the next line */

#ifdef OW_I18N
				last = (int)xv_get(textsw, TEXTSW_LENGTH_WC);
#else
				last = (int)xv_get(textsw, TEXTSW_LENGTH);
#endif /* OW_I18N */

#ifdef OW_I18N
				/* should use wide char attribute for the performance */
				if (xv_get(textsw, TEXTSW_INSERTION_POINT_WC) == last) {	/* } */
#else
				if (xv_get(textsw, TEXTSW_INSERTION_POINT) == last) {
#endif

					notify_rtn_code = PANEL_NEXT;
					ok_to_insert = FALSE;
				}
				else
					ok_to_insert = TRUE;
				break;
			case ACTION_GO_LINE_FORWARD:	/* Go to start of next line */
			case ACTION_DOWN:
				notify_rtn_code = PANEL_NEXT;
				ok_to_insert = FALSE;
				break;
			case ACTION_UP:	/* up arrow */
				notify_rtn_code = PANEL_PREVIOUS;
				ok_to_insert = FALSE;
				break;
			default:
				break;
		}
	}
	if (ok_to_insert) {
		return notify_next_event_func(window, ev, arg, type);
	}
	else {
		if (event_is_down(event)) {
			switch (notify_rtn_code) {
				case PANEL_NEXT:
					mltxt_advance_caret(ip);
					break;
				case PANEL_PREVIOUS:
					mltxt_backup_caret(ip);
					break;
				default:
					break;
			}
		}
		return NOTIFY_DONE;
	}
}


static int notify_user(Mltxt_info *dp, Event *event)
{
    switch (dp->notify_level) {
      case PANEL_ALL:
      default:
	return TRUE;

      case PANEL_NONE:
	return FALSE;

#ifdef OW_I18N
      case PANEL_NON_PRINTABLE:
	  if (event_is_string(event)) {
		wchar_t		 w;
		register char	*p;

		p = event->ie_string;

		/*  Need to convert multibyte
		 *  to wide char first
		 */
		while (*p) {
		    /* Even if conversion is successful
		     * still not sure if it's printable
		     */
		    if ((mbtowc(&w, p, MB_CUR_MAX)) >= 0) {
			if (!iswprint((int) w)) return TRUE;
		    }
		    else
			return TRUE;
		    p++;
		}
		return FALSE;
	  }
	  else return !panel_printable_char(event_action(event));

      case PANEL_SPECIFIED:
	if (event_is_string(event)) {
	    wchar_t	*pos;
	    wchar_t 	*ie_string_wc;
	    wchar_t	*p;

	    /*  POSSIBLE BUG: What happens when a multibyte character
	     *  cannot be converted to wide char??
	     *
	     *  Convert the entire input string to wide char first
	     */
	    ie_string_wc = _xv_mbstowcsdup((char *) event_string(event));
	    p = ie_string_wc;

	    /*  Return if A char in input string 
	     *	matches the terminating character?
	     */
	    while (*p) {
		if ((pos = (wschr(dp->terminators, (int) *p))) != 0) 
		    break;
		p++;
	    }
	    xv_free(ie_string_wc);
	    return event_is_down(event) && pos != 0;
	}
	else {
	    char	 tmp_char;
	    wchar_t	*tmp_char_wc;
	    wchar_t	*pos = 0;

	    /* Should I check for ASCII?? */

	    if (iswascii(event_action(event))) {
		tmp_char = event_action(event);
		tmp_char_wc = (wchar_t *)xv_malloc(sizeof(wchar_t));
		mbtowc(tmp_char_wc, &tmp_char, MB_CUR_MAX);
		pos = wschr(dp->terminators, (int)tmp_char_wc);
	    }
	    return event_is_down(event) && pos != 0;
	}
#else
      case PANEL_NON_PRINTABLE:
	return !panel_printable_char(event_action(event));

      case PANEL_SPECIFIED:
	return event_is_down(event) &&
	    strchr(dp->terminators, event_action(event)) != 0;
#endif /* OW_I18N */
    }
}

Xv_pkg xv_panel_multiline_text_pkg = {
    "Multiline Text Item", ATTR_PKG_PANEL,
    sizeof(Xv_panel_multiline_text),
    &xv_panel_item_pkg,
    panel_mltxt_init,
    panel_mltxt_set_avlist,
    panel_mltxt_get_attr,
    panel_mltxt_destroy,
    NULL                        /* no find proc */
};
