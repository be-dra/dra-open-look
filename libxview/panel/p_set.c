#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)p_set.c 20.94 93/06/28 DRA: $Id: p_set.c,v 4.4 2024/11/14 20:14:53 dra Exp $";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL_NOTICE 
 *	file for terms of the license.
 */

#include <xview_private/panel_impl.h>
#include <xview/font.h>
#include <xview/scrollbar.h>
#include <xview/xv_xrect.h>
#include <xview/font.h>
#include <xview/defaults.h>
#include <xview_private/draw_impl.h>
#include <xview_private/font_impl.h>
#include <xview_private/win_info.h>

#define ADONE ATTR_CONSUME(*attrs);break

static void panel_set_fonts(Panel panel_public, Panel_info *panel);
extern Graphics_info *xv_init_olgx(Xv_Window, int *, Xv_Font);

Pkg_private Xv_opaque panel_set_avlist(Panel panel_public, Attr_avlist avlist)
{
	register Attr_attribute *attrs;
	register Panel_info *panel = PANEL_PRIVATE(panel_public);
	Xv_Drawable_info *info;
	Item_info *ip;
	Scrollbar new_h_scrollbar = 0;
	Scrollbar new_v_scrollbar = 0;
	Xv_Window pw;
	int three_d;
	int wants_focus;
	Panel_item item;

	for (attrs=avlist; *attrs; attrs=attr_next(attrs)) switch (*attrs) {
		case PANEL_CLIENT_DATA:
			panel->client_data = attrs[1];
			ADONE;

		case PANEL_BOLD_FONT:
			/* Sunview1 compatibility attribute: not used */
			ADONE;

		case PANEL_BLINK_CARET:
			if ((int)attrs[1])
				panel->status.blinking = TRUE;
			else
				panel->status.blinking = FALSE;
			if (panel->kbd_focus_item &&
					panel->kbd_focus_item->item_type == PANEL_TEXT_ITEM)
				panel_text_caret_on(panel, TRUE);
			ADONE;

		case PANEL_CARET_ITEM:
			if (!attrs[1]) {
				xv_error(XV_NULL,
						ERROR_BAD_VALUE, attrs[1], PANEL_CARET_ITEM,
						ERROR_PKG, PANEL, NULL);
				return XV_ERROR;	/* NULL ptr */
			}
			ip = ITEM_PRIVATE(attrs[1]);
			if (inactive(ip) || hidden(ip) ||
					(!wants_key(ip) && !ip->child_kbd_focus_item)) {
				xv_error(XV_NULL,
						ERROR_BAD_VALUE, attrs[1], PANEL_CARET_ITEM,
						ERROR_PKG, PANEL, NULL);
				return XV_ERROR;	/* item cannot take input focus */
			}
			if (ip->child_kbd_focus_item)
				ip = ITEM_PRIVATE(ip->child_kbd_focus_item);
			panel_yield_kbd_focus(panel);
			panel->kbd_focus_item = ip;
			panel_accept_kbd_focus(panel);
			ADONE;

		case PANEL_EVENT_PROC:
			panel->event_proc = (int (*)(Xv_opaque, Event *))attrs[1];
			ADONE;

		case PANEL_REPAINT_PROC:
			panel->repaint_proc =
					(int (*)(Panel, Xv_Window, Rectlist *))attrs[1];
			ADONE;

		case PANEL_BACKGROUND_PROC:
			panel->ops.panel_op_handle_event =
					(void (*)(Xv_opaque, Event *))attrs[1];
			ADONE;

		case PANEL_ITEM_X_GAP:
			panel->item_x_offset = (int)attrs[1];
			if (panel->item_x_offset < 1)
				panel->item_x_offset = 1;
			ADONE;

		case PANEL_ITEM_Y_GAP:
			panel->item_y_offset = (int)attrs[1];
			if (panel->item_y_offset < 1)
				panel->item_y_offset = 1;
			ADONE;

		case PANEL_EXTRA_PAINT_WIDTH:
			if ((int)attrs[1] >= 0) {
				panel->extra_width = (int)attrs[1];
				panel->flags |= UPDATE_SCROLL;
			}
			ADONE;

		case PANEL_EXTRA_PAINT_HEIGHT:
			if ((int)attrs[1] >= 0) {
				panel->extra_height = (int)attrs[1];
				panel->flags |= UPDATE_SCROLL;
			}
			ADONE;

		case PANEL_LABEL_INVERTED:
			if (attrs[1])
				panel->flags |= LABEL_INVERTED;
			else
				panel->flags &= ~LABEL_INVERTED;
			ADONE;

		case PANEL_LAYOUT:
			switch ((Panel_setting) attrs[1]) {
				case PANEL_HORIZONTAL:
				case PANEL_VERTICAL:
					panel->layout = (Panel_setting) attrs[1];
					break;
				default:	/* invalid layout */
					break;
			}
			ADONE;

		case PANEL_PAINT:
			panel->repaint = (Panel_setting) attrs[1];
			ADONE;

		case PANEL_ITEM_X_POSITION:
			panel->item_x = (int)attrs[1];
			ADONE;

		case PANEL_ITEM_Y_POSITION:
			panel->item_y = (int)attrs[1];
			ADONE;

		case PANEL_PRIMARY_FOCUS_ITEM:
			panel->primary_focus_item =
					ITEM_PRIVATE((Panel_item) attrs[1]);
			ADONE;

		case PANEL_NO_REDISPLAY_ITEM:
			panel->no_redisplay_item = (int)attrs[1];
			ADONE;

		case WIN_VERTICAL_SCROLLBAR:
		case OPENWIN_VERTICAL_SCROLLBAR:
			new_v_scrollbar = (Scrollbar) attrs[1];
			break;

		case WIN_HORIZONTAL_SCROLLBAR:
		case OPENWIN_HORIZONTAL_SCROLLBAR:
			new_h_scrollbar = (Scrollbar) attrs[1];
			break;

		case PANEL_ACCEPT_KEYSTROKE:
			if (attrs[1]) {
				panel->flags |= WANTS_KEY;
			}
			else
				panel->flags &= ~WANTS_KEY;
			wants_focus = panel_wants_focus(panel);
			PANEL_EACH_PAINT_WINDOW(panel, pw)
				win_set_no_focus(pw, !wants_focus);
			PANEL_END_EACH_PAINT_WINDOW
			ADONE;

		case PANEL_DEFAULT_ITEM:
			if ((item = panel->default_item) != (Panel_item) attrs[1]) {

				/* repaint the previous default item */
				if (item) {
					panel->default_item = XV_NULL;
					ip = ITEM_PRIVATE(item);
					panel_redisplay_item(ip, ip->repaint);
					panel->default_item = (Panel_item) attrs[1];
				}

				/* repaint the new default item */
				if ((panel->default_item = (Panel_item) attrs[1])) {
					ip = ITEM_PRIVATE(panel->default_item);
					panel_redisplay_item(ip, ip->repaint);
				}

			}
			ADONE;

		case PANEL_BORDER:
			panel->show_border = (int)attrs[1];
			if (panel->paint_window)
				panel_paint_border(panel_public, panel,
						panel->paint_window->pw);
			ADONE;

		case PANEL_BUTTONS_TO_MENU:
			/* there is a small inconsistency in the Open Look Spec:
			 * on page 19 they say that every region on the screen
			 * that is not a control must have a popup menu
			 * but then all the command window examples have the controls
			 * in a pane - but nothing is said about 'the required popup
			 * menu'. This is in contrast to property windows where the
			 * popup menu resembles the Apply, Reset ... button.
			 *
			 * The idea here is: every PANEL_BUTTON with 
			 * PANEL_ITEM_LAYOUT_ROLE = PANEL_ROLE_CENTER
			 * will produce a corresponding Menu_item in a menu
			 */

			panel_buttons_to_menu(panel_public);
			ADONE;

		case PANEL_DO_LAYOUT:
			panel_layout_items(panel_public, TRUE);
			ADONE;

		case PANEL_ALIGN_LABELS:
			panel_align_labels(panel_public, (Panel_item *)attrs[1]);
			ADONE;

		case WIN_REMOVE_CARET:
			if (panel->kbd_focus_item &&
					panel->kbd_focus_item->item_type == PANEL_TEXT_ITEM) {
				/* Clear caret */
				panel_text_caret_on(panel, FALSE);
			}
			panel->caret = XV_NULL;
			break;

#ifdef VERSION_3
		case WIN_FOREGROUND_COLOR:
		case WIN_BACKGROUND_COLOR:
			if (panel->status.three_d) {
				char error_string[64];

				sprintf(error_string,
						XV_MSG("%s not valid on a 3D Panel"),
						*attrs == WIN_FOREGROUND_COLOR
									? "WIN_FOREGROUND_COLOR"
									: "WIN_BACKGROUND_COLOR");
				xv_error(panel_public, ERROR_STRING, error_string, NULL);
				ATTR_CONSUME(attrs[0]);
			}
			break;
#endif /* VERSION_3 */

		case WIN_SET_FOCUS:
			{
				Xv_Window pw;
				Xv_opaque status;
				int wants_focus;

				ATTR_CONSUME(attrs[0]);

				wants_focus = panel_wants_focus(panel);
				if (!wants_focus)
					return XV_ERROR;	/* no keyboard focus items */

				/*
				 * Find the first paint window that can accept kbd input and
				 * set the input focus to it.  Only do this if we have a
				 * caret/text item or the panel wants the key.  Since panels
				 * always assigns their own input focus, there is no need to
				 * check xv_no_focus().
				 */
				status = XV_ERROR;
				PANEL_EACH_PAINT_WINDOW(panel, pw)
					DRAWABLE_INFO_MACRO(pw, info);
					if (wants_key(panel) ||
						win_getinputcodebit(
								(Inputmask *) xv_get(pw, WIN_INPUT_MASK),
								KBD_USE))
					{
						win_set_kbd_focus(pw, xv_xid(info));
						status = XV_OK;
						break;
					}
				PANEL_END_EACH_PAINT_WINDOW
				if (status == XV_ERROR)
					return XV_ERROR;	/* no paint window wants kbd input */
			}
			break;

		case XV_FOCUS_ELEMENT:
			if (panel->status.destroying)
				return XV_ERROR;	/* can't set focus to this panel */
			if (panel->status.has_input_focus)
				panel_yield_kbd_focus(panel);
			if (attrs[1] == 0) {
				/* Set keyboard focus to first item that wants it. */
				panel->kbd_focus_item = panel->last_item;
				ip = panel_next_kbd_focus(panel, TRUE);
			}
			else {
				/* Set keyboard focus to last item that wants it. */
				panel->kbd_focus_item = panel->items;
				ip = panel_previous_kbd_focus(panel, TRUE);
			}
			if (ip) {
				/* There's more than one kbd focus item */
				panel->kbd_focus_item = ip;
			}
			panel->status.focus_item_set = TRUE;
			if (panel->status.has_input_focus)
				panel_accept_kbd_focus(panel);
			break;

		case XV_FONT:
			panel->std_font = attrs[1];
			panel_set_fonts(panel_public, panel);
			ADONE;

		case XV_END_CREATE:
			/* Set up the fonts */
			panel->std_font = xv_get(panel_public, XV_FONT);
			panel_set_fonts(panel_public, panel);

			/* Set up the Colormap Segment and OLGX */
			three_d = panel->status.three_d ? TRUE : FALSE;
			panel->ginfo = xv_init_olgx(panel_public, &three_d,
					xv_get(panel_public, XV_FONT));
			panel->status.three_d = three_d;

			if (!panel->paint_window) {
				/* PANEL instead of SCROLLABLE_PANEL:
				 *   set up paint_window structure
				 */
				panel_register_view(panel, XV_NULL);
			}
			else {
				Pixmap bg_pixmap = (Pixmap) xv_get(panel_public,
						WIN_BACKGROUND_PIXMAP);

				if (bg_pixmap)
					xv_set(panel->paint_window->pw,
							WIN_BACKGROUND_PIXMAP, bg_pixmap, NULL);
			}

			/* Initialize focus_pw to the first paint window.
			 * panel_show_focus_win depends on panel->focus_pw always
			 * being valid.
			 */
			panel->focus_pw = panel->paint_window->pw;

			xv_set(panel_public,
					WIN_ROW_HEIGHT, panel->ginfo->button_height, NULL);

#ifdef OW_I18N
			DRAWABLE_INFO_MACRO(panel->focus_pw, info);
			if (xv_get(xv_server(info), XV_IM) != NULL &&
					xv_get(panel_public, WIN_USE_IM) == TRUE) {
				/* Create ic on paint window, store ic in panel info */
				panel->ic = (XIC) xv_get(panel_public, WIN_IC);
				if (panel->ic) {

#ifdef FULL_R5
					XGetICValues(panel->ic, XNInputStyle, &panel->xim_style,
							NULL);
#endif /* FULL_R5 */

					(void)xv_set(panel->paint_window->pw, WIN_IC, panel->ic,
							NULL);
				}
			}
#endif /* OW_I18N */

			break;

		default:
			xv_check_bad_attr(&xv_panel_pkg, *attrs);
			break;
	}

	/* set up any scrollbars */
	if (new_v_scrollbar != XV_NULL &&
			xv_get(new_v_scrollbar, SCROLLBAR_NORMALIZE_PROC) == XV_NULL) {
		xv_set(new_v_scrollbar, SCROLLBAR_NORMALIZE_PROC,
				panel_normalize_scroll, NULL);
	}
	if (new_h_scrollbar != XV_NULL &&
			xv_get(new_h_scrollbar, SCROLLBAR_NORMALIZE_PROC) == XV_NULL) {
		xv_set(new_h_scrollbar, SCROLLBAR_NORMALIZE_PROC,
				panel_normalize_scroll, NULL);
	}

	/* if extra width, height was set, update panel scrolling size */
	if (panel->flags & UPDATE_SCROLL && panel->paint_window) {
		panel->flags &= ~UPDATE_SCROLL;
		panel_update_scrolling_size(panel_public);
	}

	return XV_OK;
}


static void panel_set_fonts(Panel panel_public, Panel_info *panel)
{
	extern char *xv_font_regular_cmdline(void);
    XCharStruct	    active_caret_info;
    XFontStruct	   *font_info;
    int		    font_size;
    XCharStruct	    inactive_caret_info;
    Font	    glyph_font;
    char	    *bold_name;
    char	    *save_bold_name;

#ifdef OW_I18N
    panel->std_fontset_id = (XFontSet) 
	xv_get(panel->std_font, FONT_SET_ID);
#else
    panel->std_font_xid = (Font) xv_get(panel->std_font, XV_XID);
#endif /* OW_I18N */

    font_size = (int) xv_get(panel->std_font, FONT_SIZE);

    glyph_font = xv_find_olglyph_font(panel->std_font);

    if (!glyph_font)
	xv_error(XV_NULL,
		 ERROR_STRING, 
		    XV_MSG("Unable to find OPEN LOOK glyph font"),
		 ERROR_SEVERITY, ERROR_NON_RECOVERABLE,
		 ERROR_PKG, PANEL,
		 NULL);
    xv_set(panel_public, WIN_GLYPH_FONT, glyph_font, NULL); 

    /* 
     * Change the way of obtaining font_size, we used to hard code the sizes
     * here. Now, that logic is in the FONT pkg.
     * The glyph font obtained via xv_find_olglyph_font() will have the size 
     * that we want.
     */

    if (font_size == FONT_NO_SIZE)
 	font_size = (int) xv_get(glyph_font, FONT_SIZE);


#ifdef OW_I18N
    /* locale information font.name.<locale> */
    defaults_set_locale(NULL, XV_LC_BASIC_LOCALE);
#endif
    /* 
     * When creating a font via FONT_NAME, all other attributes are
     * ignored.  Therefore, if the user specifies a fontname that's
     * 28 point, then that's what they'll get.  No font size checking
     * is done to determine if the bold font size is about the same
     * as the other fonts that are used.
     */  
    panel->bold_font = XV_NULL;

	/* the xv_font_bold function return non-NULL even if there was 
	 * no command line option: it queries (amongst others) the
	 * "openwindows.boldfont" resource which is probably set....
	 *
	 * This change was made together with the attempt to implement
	 * popup window scale.....
	 */
	if ((save_bold_name = (bold_name = defaults_get_string("font.name.cmdline",
											"Font.Name.Cmdline", NULL))))
	{
        /*
         * cache string obtained from defaults pkg, as it's
         * contents might change
         */
        if (bold_name && strlen( bold_name ))
            bold_name = xv_strsave( save_bold_name );
        else
            bold_name = (char *)NULL;
 
        if (bold_name && !xv_font_regular_cmdline())
        {
#ifdef OW_I18N
            panel->bold_font = xv_find( panel_public,
                FONT, FONT_SET_SPECIFIER, bold_name,
                NULL );
#else
            panel->bold_font = xv_find( panel_public,
                FONT, FONT_NAME, bold_name,
                NULL );
#endif
        }
        else
        {
            panel->bold_font = xv_find(panel_public, FONT,
                FONT_FAMILY, xv_get(panel->std_font, FONT_FAMILY),
                FONT_STYLE, FONT_STYLE_BOLD,
                FONT_SIZE, font_size,
                NULL);
        }

        if (panel->bold_font == XV_NULL)
            xv_error(XV_NULL,
                 ERROR_STRING,
		     XV_MSG("Unable to find bold font"),
                 ERROR_PKG, PANEL,
                 NULL);
        if (bold_name)
            xv_free( bold_name );
    }
    
    if (panel->bold_font == XV_NULL) {
        panel->bold_font = xv_find(panel_public, FONT,
            FONT_FAMILY, xv_get(panel->std_font, FONT_FAMILY),
            FONT_STYLE, FONT_STYLE_BOLD,
            FONT_SIZE, font_size,
            NULL);
    }
 
#ifdef OW_I18N
    defaults_set_locale(NULL, NULL);
#endif

    if (panel->bold_font == XV_NULL) {
        xv_error(XV_NULL,
                 ERROR_STRING,
		     XV_MSG("Unable to find bold font; using standard font"),
                 ERROR_PKG, PANEL,
                 NULL);
        panel->bold_font = panel->std_font;
    }  
#ifdef OW_I18N
    panel->bold_fontset_id = (XFontSet) 
	xv_get(panel->bold_font, FONT_SET_ID);
#else
    panel->bold_font_xid = (Font) xv_get(panel->bold_font, XV_XID);
#endif /* OW_I18N */

    font_info = (XFontStruct *) xv_get(glyph_font, FONT_INFO);
    if (font_info->per_char) {
	active_caret_info = font_info->per_char[OLGX_ACTIVE_CARET];
	inactive_caret_info = font_info->per_char[OLGX_INACTIVE_CARET];
    } else {
	active_caret_info = font_info->min_bounds;
	inactive_caret_info = font_info->min_bounds;
    }
    panel->active_caret_ascent = active_caret_info.ascent;
    panel->active_caret_height = active_caret_info.ascent +
	active_caret_info.descent;
    panel->active_caret_width = active_caret_info.width;
    panel->inactive_caret_ascent = inactive_caret_info.ascent;
    panel->inactive_caret_height = inactive_caret_info.ascent +
	inactive_caret_info.descent;
    panel->inactive_caret_width = inactive_caret_info.width;
}

static int column_from_absolute_x(int x_position, int col_gap, int left_margin, Xv_Font font);
static int row_from_absolute_y(int y_position, int row_gap, int top_margin, Xv_Font font);

Pkg_private void panel_refont(Panel_info *panel, int arg)
{
    register Panel_item item;
    register Panel  panel_public = PANEL_PUBLIC(panel);
    register Item_info *ip;
    register Panel_image *label;
    Xv_Font         panel_font, old_win_font, old_bold_font, new_win_font,
                    new_bold_font;
    int             label_bold, item_x, item_y, row_gap,
                    col_gap, left_margin, top_margin, item_row, item_col;

    old_win_font = xv_get(panel_public, XV_FONT);
    new_win_font = (old_win_font) ?
	xv_find(panel_public, FONT,
		FONT_RESCALE_OF, old_win_font, (int) arg,
		NULL)
	: (Xv_Font) 0;
    if (new_win_font) {
	(void) xv_set(old_win_font, XV_INCREMENT_REF_COUNT, NULL);
	(void) xv_set(panel_public, XV_FONT, new_win_font, NULL);
	panel_font = new_win_font;
    } else
	panel_font = old_win_font;

    old_bold_font = panel->bold_font;
    new_bold_font = (old_bold_font) ?
	xv_find(panel_public, FONT,
		FONT_RESCALE_OF, old_bold_font, (int) arg,
		NULL)
	: (Xv_Font) 0;
    if (new_bold_font) {
	(void) xv_set(panel_public, PANEL_BOLD_FONT, new_bold_font, NULL);
    }

    if ((!new_win_font) && (!new_bold_font))
	return;

    row_gap = (int) xv_get(panel_public, WIN_ROW_GAP);
    col_gap = (int) xv_get(panel_public, WIN_COLUMN_GAP);
    left_margin = (int) xv_get(panel_public, WIN_LEFT_MARGIN);
    top_margin = (int) xv_get(panel_public, WIN_TOP_MARGIN);

    PANEL_EACH_ITEM(panel_public, item)
	ip = ITEM_PRIVATE(item);
    if (new_win_font) {
	item_x = (int) xv_get(ITEM_PUBLIC(ip), PANEL_ITEM_X);
	item_y = (int) xv_get(ITEM_PUBLIC(ip), PANEL_ITEM_Y);
	item_col = column_from_absolute_x(item_x, col_gap, top_margin,
					  new_win_font);
	item_row = row_from_absolute_y(item_y, row_gap, left_margin,
				       new_win_font);
	(void) xv_set(ITEM_PUBLIC(ip),
		      PANEL_ITEM_X, xv_col(panel_public, item_col),
		      PANEL_ITEM_Y, xv_row(panel_public, item_row),
		      PANEL_PAINT, PANEL_NONE,
		      NULL);
    }
    label = &ip->label;
    if (is_string(label)) {

	label_bold = (int) xv_get(
				  ITEM_PUBLIC(ip), PANEL_LABEL_BOLD);

#ifdef OW_I18N
	xv_set(ITEM_PUBLIC(ip),
	       PANEL_PAINT, PANEL_NONE,
	       PANEL_LABEL_FONT, panel_font,
	       PANEL_LABEL_STRING_WCS, image_string_wc(label),
	       NULL);
#else
	xv_set(ITEM_PUBLIC(ip),
	       PANEL_PAINT, PANEL_NONE,
	       PANEL_LABEL_FONT, panel_font,
	       PANEL_LABEL_STRING, image_string(label),
	       NULL);
#endif /* OW_I18N */

	if (label_bold) {
	    xv_set(ITEM_PUBLIC(ip),
		   PANEL_PAINT, PANEL_NONE,
		   PANEL_LABEL_BOLD, label_bold,
		   NULL);
	}
    }
    xv_set(ITEM_PUBLIC(ip),
	   PANEL_PAINT, PANEL_NONE,
	   PANEL_LABEL_FONT, panel_font,
	   NULL);
    switch (ip->item_type) {

      case PANEL_MESSAGE_ITEM:
	break;

#ifdef OW_I18N
      case PANEL_BUTTON_ITEM:{
	    wchar_t        *label = (wchar_t *) xv_get(
				       ITEM_PUBLIC(ip), PANEL_LABEL_STRING_WCS);
	    if (label)		/* don't scale image buttons */
		xv_set(ITEM_PUBLIC(ip),
		       PANEL_PAINT, PANEL_NONE,
		       PANEL_LABEL_STRING_WCS, label,
		       NULL);
	    break;
	}
#else
      case PANEL_BUTTON_ITEM:{
	    char           *label = (char *) xv_get(
				       ITEM_PUBLIC(ip), PANEL_LABEL_STRING);
	    if (label)		/* don't scale image buttons */
		xv_set(ITEM_PUBLIC(ip),
		       PANEL_PAINT, PANEL_NONE,
		       PANEL_LABEL_STRING, label,
		       NULL);
	    break;
	}
#endif /* OW_I18N */

      case PANEL_TOGGLE_ITEM:
	xv_set(ITEM_PUBLIC(ip),
	       PANEL_PAINT, PANEL_NONE,
	       PANEL_CHOICE_FONTS, panel_font, 0,
	       NULL);
	break;

      case PANEL_CHOICE_ITEM:
	xv_set(ITEM_PUBLIC(ip),
	       PANEL_PAINT, PANEL_NONE,
	       PANEL_CHOICE_FONTS, panel_font, 0,
	       NULL);
	break;

      case PANEL_TEXT_ITEM:
      case PANEL_SLIDER_ITEM:
	xv_set(ITEM_PUBLIC(ip),
	       PANEL_PAINT, PANEL_NONE,
	       PANEL_VALUE_FONT, panel_font,
	       NULL);
	break;


      default:
	break;
    }
    /*
     * undecided if we should paint it.  Damage will do it for free when it
     * is resized.
     */
    panel_paint(ITEM_PUBLIC(ip), PANEL_CLEAR);
    PANEL_END_EACH
	if (new_win_font) {
	(void) xv_set(panel_public, XV_FONT, old_win_font, NULL);
	(void) xv_set(old_win_font, XV_DECREMENT_REF_COUNT, NULL);
    }
}


static int column_from_absolute_x(int x_position, int col_gap, int left_margin, Xv_Font font)
{
    int		chrwth;

    x_position -= left_margin;
    chrwth = xv_get(font, FONT_DEFAULT_CHAR_WIDTH);
    return (x_position / (chrwth + col_gap));
}


static int row_from_absolute_y(int y_position, int row_gap, int top_margin, Xv_Font font)
{
    int		chrht;

    y_position -= top_margin;
    chrht = xv_get(font, FONT_DEFAULT_CHAR_HEIGHT);
    return (y_position / (chrht + row_gap));
}
