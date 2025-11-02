#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)p_btn.c 20.110 93/06/28 DRA: $Id: p_btn.c,v 4.7 2025/11/01 14:55:10 dra Exp $";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL_NOTICE 
 *	file for terms of the license.
 */

#include <xview_private/panel_impl.h>
#include <xview/font.h>
#include <xview/openmenu.h>
#include <xview/pixwin.h>
#include <xview/server.h>
#include <xview/svrimage.h>
#include <xview/cms.h>
#include <X11/Xlib.h>
#include <xview_private/draw_impl.h>


#define BUTTON_PRIVATE(item) \
	XV_PRIVATE(Button_info, Xv_panel_button, item)
#define BUTTON_FROM_ITEM(ip)	BUTTON_PRIVATE(ITEM_PUBLIC(ip))


static void     btn_begin_preview(Panel_item item, Event *),
	btn_cancel_preview(Panel_item item, Event *),
	btn_accept_preview(Panel_item item, Event *),
		btn_accept_menu(Panel_item item, Event *),
		btn_accept_key(Panel_item item, Event *),
		btn_paint(Panel_item item, Panel_setting), btn_remove(Panel_item item)
		;
		
static void btn_accept_kbd_focus(Panel_item	    item_public);
static void btn_yield_kbd_focus(Panel_item	item_public);

typedef Menu (*m_gen_proc_t)(Menu, Menu_generate);
typedef Menu (*mi_gen_proc_t)(Menu_item, Menu_generate);

static Panel_ops ops = {
    panel_default_handle_event,		/* handle_event() */
    btn_begin_preview,			/* begin_preview() */
    NULL,				/* update_preview() */
    btn_cancel_preview,			/* cancel_preview() */
    btn_accept_preview,			/* accept_preview() */
    btn_accept_menu,			/* accept_menu() */
    btn_accept_key,			/* accept_key() */
    panel_default_clear_item,		/* clear() */
    btn_paint,				/* paint() */
    NULL,				/* resize() */
    btn_remove,				/* remove() */
    NULL,				/* restore() */
    NULL,				/* layout() */
    btn_accept_kbd_focus,		/* accept_kbd_focus() */
    btn_yield_kbd_focus,		/* yield_kbd_focus() */
    NULL				/* extension: reserved for future use */
};

static int busy_key, done_key, color_key, item_key = 0; 

typedef struct button_info {
    Panel_item      public_self;/* back pointer to object */
    int		    clear_button_rect;
    int		    default_menu_item_inactive;
    Server_image    pin_in_image;
}               Button_info;


/*
 * Function declarations
 */
Xv_private void menu_save_pin_window_rect(Frame);
Xv_private void menu_item_set_parent(Menu_item, Menu);

static void take_down_cmd_frame(Panel panel_public);
static void button_menu_busy_proc(Menu menu);
static void button_menu_done_proc(Menu menu, Xv_opaque result);
static void panel_paint_button_image(Item_info *ip, Panel_image *image, int inactive_button, Xv_opaque menu, int height);
int 		panel_item_destroy_flag;
#ifdef OW_I18N
extern wchar_t _xv_null_string_wc[];
#endif /* OW_I18N */

static Graphics_info *create_revpin_ginfo(Xv_window win)
{
	/* stolen from xv_init_olgx */
	Display *display;
	XFontStruct *glyph_font_struct;
	XFontStruct *text_font_struct;
	Xv_Drawable_info *info;
	unsigned long pixvals[OLGX_NUM_COLORS];
	int screen_number;
	Pixmap stipple_pixmaps[3];

	DRAWABLE_INFO_MACRO(win, info);

	pixvals[OLGX_WHITE] = xv_fg(info);
	pixvals[OLGX_BLACK] = xv_bg(info);
	pixvals[OLGX_BG1] = pixvals[OLGX_BLACK];
	pixvals[OLGX_BG2] = pixvals[OLGX_BLACK];
	pixvals[OLGX_BG3] = pixvals[OLGX_BLACK];

	text_font_struct = (XFontStruct *) xv_get(xv_get(win, XV_FONT), FONT_INFO);
	glyph_font_struct = (XFontStruct *)xv_get(xv_get(win, WIN_GLYPH_FONT),
												FONT_INFO);

	display = xv_display(info);
	screen_number = (int)xv_get(xv_screen(info), SCREEN_NUMBER);
	stipple_pixmaps[0] = stipple_pixmaps[1] = stipple_pixmaps[2] = None;

	return olgx_main_initialize(display, screen_number, xv_depth(info),
			OLGX_2D, glyph_font_struct, text_font_struct, pixvals,
			stipple_pixmaps);
}

/* ========================================================================= */

/* -------------------- XView Functions  -------------------- */
static int panel_button_init(Panel panel_public, Panel_item item_public,
							Attr_avlist avlist, int *unused)
{
	Panel_info *panel = PANEL_PRIVATE(panel_public);
	register Item_info *ip = ITEM_PRIVATE(item_public);
	Xv_panel_button *item_object = (Xv_panel_button *) item_public;
	Button_info *dp;

	if (item_key == 0) {
		item_key = xv_unique_key();
		busy_key = xv_unique_key();
		done_key = xv_unique_key();
		color_key = xv_unique_key();
	}

	dp = xv_alloc(Button_info);

	item_object->private_data = (Xv_opaque) dp;
	dp->public_self = item_public;

	ip->ops = ops;
	if (panel->event_proc)
		ip->ops.panel_op_handle_event =
				(void (*)(Xv_opaque, Event *))panel->event_proc;
	ip->item_type = PANEL_BUTTON_ITEM;

	if (panel->status.mouseless)
		ip->flags |= WANTS_KEY;

	return XV_OK;
}


static int panel_button_destroy(Panel_item item_public, Destroy_status  status)
{
    Button_info	   *dp = BUTTON_PRIVATE(item_public);

    if ((status == DESTROY_CHECKING) || (status == DESTROY_SAVE_YOURSELF))
	return XV_OK;
    btn_remove(item_public);
    free(dp);
    return XV_OK;
}

static Menu generate_menu(Menu menu)
{
	m_gen_proc_t gen_proc;	/* generated menu and procedure */

	if ((gen_proc = (m_gen_proc_t)xv_get(menu, MENU_GEN_PROC))) {
		Menu gen_menu;

		gen_menu = gen_proc(menu, MENU_DISPLAY);
		if (gen_menu == XV_NULL) {
			xv_error(menu,
				ERROR_STRING, XV_MSG
				("begin_preview: menu's gen_proc failed to generate a menu"),
				ERROR_PKG, PANEL,
				NULL);
			return XV_NULL;
		}
		return gen_menu;
	}
	else
		return menu;
}




/* --------------------  Panel Item Operations  -------------------- */
static void btn_begin_preview( Panel_item item_public, Event *event)
{
	Menu default_menu;
	Menu_item default_menu_item;
	Button_info *dp = BUTTON_PRIVATE(item_public);
	int height;
	Panel_image image;
	Xv_Drawable_info *info;
	Item_info *ip = ITEM_PRIVATE(item_public);
	void *label;
	Menu_class menu_class;

	mi_gen_proc_t mi_gen_proc;
	int olgx_state = OLGX_NORMAL;
	int pin_is_default;	/* boolean */
	Pixlabel pixlabel;
	int pushpin_width;
	Xv_Window pw;
	Menu submenu;

	dp->clear_button_rect = FALSE;
	ip->flags |= INVOKED;
	if (ip->menu) {

		/* Get the Server Image or string of the default item */
		submenu = ip->menu;
		if ((default_menu = generate_menu(submenu)) == XV_NULL)
			return;
		default_menu_item = xv_get(default_menu, MENU_DEFAULT_ITEM);
		if (!default_menu_item)
			/* Invalid menu attached: ignore begin_preview request */
			return;
		mi_gen_proc = (mi_gen_proc_t)xv_get(default_menu_item, MENU_GEN_PROC);
		if (mi_gen_proc) {
			if (!xv_get(default_menu_item, MENU_PARENT))
				menu_item_set_parent(default_menu_item, default_menu);
			default_menu_item = mi_gen_proc(default_menu_item, MENU_DISPLAY);
		}
		if (!default_menu_item)
			/* Invalid menu attached: ignore begin_preview request */
			return;
		pin_is_default = xv_get(default_menu, MENU_PIN) &&
				xv_get(default_menu_item, MENU_TITLE);
		height = 0;

#ifdef OW_I18N
		if (pin_is_default) {
			image.im_type = PIT_STRING;
			image_string_wc(&image) = _xv_null_string_wc;
			label = (void *)_xv_null_string_wc;
			olgx_state |= OLGX_LABEL_IS_WCS;
		}
		else if (!(image_string_wc(&image) = (CHAR *)
						xv_get(default_menu_item, MENU_STRING_WCS))) {
			olgx_state |= OLGX_LABEL_IS_PIXMAP;
			image.im_type = PIT_SVRIM;
			image_svrim(&image) = xv_get(default_menu_item, MENU_IMAGE);
			pixlabel.pixmap = (XID) xv_get(image_svrim(&image), XV_XID);
			pixlabel.width = ((Pixrect *) image_svrim(&image))->pr_width;
			pixlabel.height = ((Pixrect *) image_svrim(&image))->pr_height;
			label = (void *)&pixlabel;
			height = Button_Height(ip->panel->ginfo);
			dp->clear_button_rect = TRUE;
		}
		else {
			image.im_type = PIT_STRING;
			image_font(&image) = image_font(&ip->label);
			image_bold(&image) = image_bold(&ip->label);
			label = (void *)image_string_wc(&image);
			olgx_state |= OLGX_LABEL_IS_WCS;
		}
#else
		if (pin_is_default) {
			image.im_type = PIT_STRING;
			image_string(&image) = NULL;
			label = (void *)"";
		}
		else if (!(image_string(&image) = (char *)xv_get(default_menu_item,
								MENU_STRING))) {
			olgx_state |= OLGX_LABEL_IS_PIXMAP;
			image.im_type = PIT_SVRIM;
			image_svrim(&image) = xv_get(default_menu_item, MENU_IMAGE);
			pixlabel.pixmap = (XID) xv_get(image_svrim(&image), XV_XID);
			pixlabel.width = ((Pixrect *) image_svrim(&image))->pr_width;
			pixlabel.height = ((Pixrect *) image_svrim(&image))->pr_height;
			label = (void *)&pixlabel;
			height = Button_Height(ip->panel->ginfo);
			dp->clear_button_rect = TRUE;
		}
		else {
			image.im_type = PIT_STRING;
			image_font(&image) = image_font(&ip->label);
			image_bold(&image) = image_bold(&ip->label);
			label = (void *)image_string(&image);
		}
#endif /* OW_I18N */

		image_ginfo(&image) = image_ginfo(&ip->label);
		/*
		 * Replace the menu button with the default item Server Image or
		 * string.  If it's a string, truncate the string to the width of the
		 * original button stack string.
		 */
		dp->default_menu_item_inactive = (int)xv_get(default_menu_item,
				MENU_INACTIVE);
		menu_class = (Menu_class) xv_get(default_menu, MENU_CLASS);
		if (menu_class == MENU_MIXED) {
			menu_class = (Menu_class)xv_get(default_menu_item, MENU_CLASS);
		}
		switch (menu_class) {
			case MENU_COMMAND:
				panel_paint_button_image(ip, &image,
						dp->default_menu_item_inactive, XV_NULL, height);
				if (pin_is_default) {
					Graphics_info *gis;

					if (xv_get(default_menu_item, MENU_INACTIVE)) {
						olgx_state = OLGX_PUSHPIN_OUT
									| OLGX_DEFAULT
									| OLGX_INACTIVE;
						pushpin_width = 26;	/* ??? point sizes != 12 */
					}
					else {
						olgx_state = OLGX_PUSHPIN_IN;
						pushpin_width = 13;	/* ??? point sizes != 12 */
					}
					if (ip->panel->status.three_d) {
						gis = ip->panel->ginfo;
					}
					else {
						if (! ip->panel->revpin_ginfo) {
							ip->panel->revpin_ginfo =
								create_revpin_ginfo(PANEL_PUBLIC(ip->panel));
						}
						olgx_state &= (~OLGX_INACTIVE);
						gis = ip->panel->revpin_ginfo;
					}
					PANEL_EACH_PAINT_WINDOW(ip->panel, pw)
						DRAWABLE_INFO_MACRO(pw, info);
						olgx_draw_pushpin(gis, xv_xid(info),
								ip->label_rect.r_left +
								(ip->label_rect.r_width - pushpin_width) / 2,
								ip->label_rect.r_top + 1, olgx_state);
					PANEL_END_EACH_PAINT_WINDOW
				}
				break;

			default:
				if (menu_class == MENU_CHOICE ||
						!xv_get(default_menu_item, MENU_SELECTED))
				{
					/* Choice item: always selected
					 * Toggle item: selected if previous state is "not selected"
					 */
					olgx_state |= OLGX_INVOKED;
				}
				olgx_state |= OLGX_MORE_ARROW;
				panel_clear_rect(ip->panel, ip->label_rect);
				PANEL_EACH_PAINT_WINDOW(ip->panel, pw)
					DRAWABLE_INFO_MACRO(pw, info);
					olgx_draw_choice_item(ip->panel->ginfo, xv_xid(info),
						ip->label_rect.r_left, ip->label_rect.r_top,
						ip->label_rect.r_width, ip->label_rect.r_height,
						label, olgx_state);
				PANEL_END_EACH_PAINT_WINDOW
				dp->clear_button_rect = TRUE;
		}
	}
	else
		/* Invert the button interior */
		panel_paint_button_image(ip, &ip->label, inactive(ip), ip->menu, 0);
}


static void btn_cancel_preview(Panel_item	    item_public, Event  *event)
{
    Button_info	   *dp = BUTTON_PRIVATE(item_public);
    Menu	    default_menu;
    Menu_item	    default_menu_item;
    Item_info      *ip = ITEM_PRIVATE(item_public);
    Menu_item	  (*mi_gen_proc)(Menu_item, int);

    if (dp->clear_button_rect) {
	dp->clear_button_rect = FALSE;
	panel_clear_rect(ip->panel, ip->label_rect);
    }

    /*
     * If menu button or event is SELECT-down, then repaint button.
     */
    ip->flags &= ~INVOKED;
    if (ip->menu || action_select_is_down(event))
	panel_paint_button_image(ip, &ip->label, inactive(ip), ip->menu, 0);
    if (ip->menu) {
	if ((default_menu = generate_menu(ip->menu)) == XV_NULL)
	    return;
	default_menu_item = (Menu_item) xv_get(default_menu, MENU_DEFAULT_ITEM);
	if (!default_menu_item)
	    return;
	mi_gen_proc = (Menu_item (*) (Menu_item, int)) xv_get(default_menu_item,
	    MENU_GEN_PROC);
	if (mi_gen_proc)
	    mi_gen_proc(default_menu_item, MENU_DISPLAY_DONE);
    }
}


static void btn_accept_preview(Panel_item item_public, Event *event)
{
    Button_info	   *dp = BUTTON_PRIVATE(item_public);
    Xv_Drawable_info *info;
    Item_info      *ip = ITEM_PRIVATE(item_public);

    if (!invoked(ip)) {
	/* SELECT-down didn't occur over button: ignore SELECT-up */
	return;
    }

    if (dp->clear_button_rect) {
	dp->clear_button_rect = FALSE;
	panel_clear_rect(ip->panel, ip->label_rect);
    }

    /* Set the BUSY flag and clear the INVOKED flag.
     * Clear the BUSY_MODIFIED flag so that we can find out if the notify proc
     * does an xv_set on PANEL_BUSY.
     */
    ip->flags |= BUSY;
    ip->flags &= ~(INVOKED | BUSY_MODIFIED);

    if (ip->menu && dp->default_menu_item_inactive) {
	xv_set(PANEL_PUBLIC(ip->panel), WIN_ALARM, NULL);
    } else {
	/*
	 * Repaint original button image with busy feedback.  Sync server to
	 * force busy pattern to be painted before entering notify procedure.
	 */
	panel_paint_button_image(ip, &ip->label, inactive(ip), ip->menu, 0);
	DRAWABLE_INFO_MACRO(PANEL_PUBLIC(ip->panel), info);
    XFlush(xv_display(info));

	panel_item_destroy_flag = 0;
	panel_btn_accepted(ip, event);
	if (panel_item_destroy_flag == 2)
	    return;
	panel_item_destroy_flag = 0;
    }

    /* Clear the BUSY flag unless the notify proc has modified it by doing an
     * xv_set on PANEL_BUSY.
     */
    if (!busy_modified(ip))
	ip->flags &= ~(BUSY | BUSY_MODIFIED);
    else
	ip->flags &= ~BUSY_MODIFIED;

    if (!hidden(ip) && !busy(ip))
	panel_paint_button_image(ip, &ip->label, inactive(ip), ip->menu, 0);
}


static void btn_accept_menu(Panel_item item_public, Event *event)
{
    Item_info      *ip = ITEM_PRIVATE(item_public);
    Xv_Window       paint_window = event_window(event);
	Rect rect;

    if (ip->menu == XV_NULL || paint_window == XV_NULL)
	return;

    /*
     * Notify the client.  This callback allows the client to dynamically
     * generate the menu.
     */
    (*ip->notify) (ITEM_PUBLIC(ip), event);

    /*
     * Save public panel handle and current menu color, busy proc and done proc.
     * Switch to button's color and menu done proc.
     */
    xv_set(ip->menu,
	   XV_KEY_DATA, item_key, ITEM_PUBLIC(ip),
	   XV_KEY_DATA, busy_key, xv_get(ip->menu, MENU_BUSY_PROC),
	   XV_KEY_DATA, done_key, xv_get(ip->menu, MENU_DONE_PROC),
	   XV_KEY_DATA, color_key, xv_get(ip->menu, MENU_COLOR),
	   MENU_BUSY_PROC, button_menu_busy_proc,
	   MENU_DONE_PROC, button_menu_done_proc,
	   MENU_COLOR, ip->color_index,
	   NULL);

    /* Repaint the button in the invoked state */
    ip->flags |= INVOKED;
    panel_paint_button_image(ip, &ip->label, inactive(ip), ip->menu, 0);

    /* Show menu */
    ip->panel->status.current_item_active = TRUE;
	rect = ip->label_rect;
	rect.r_top += 1;
    menu_show(ip->menu, paint_window, event,
	      MENU_POSITION_RECT, &rect,
	      MENU_PULLDOWN, ip->panel->layout == PANEL_HORIZONTAL,
	      NULL);
}


static void btn_accept_key(Panel_item	    item_public, Event *event)
{
	Item_info *ip = ITEM_PRIVATE(item_public);
	Panel_info *panel = ip->panel;

	if (panel->layout == PANEL_VERTICAL) {
		switch (event_action(event)) {
			case ACTION_RIGHT:
				if (ip->menu)
					panel_accept_menu(ITEM_PUBLIC(ip), event);
				break;
			case ACTION_UP:
				if (event_is_down(event) && is_menu_item(ip))
					panel_set_kbd_focus(panel,
							panel_previous_kbd_focus(panel, TRUE));
				break;
			case ACTION_DOWN:
				if (event_is_down(event) && is_menu_item(ip))
					panel_set_kbd_focus(panel,
							panel_next_kbd_focus(panel, TRUE));
				break;
		}
	}
	else if (ip->menu && event_action(event) == ACTION_DOWN)
		panel_accept_menu(ITEM_PUBLIC(ip), event);
}


static void btn_paint(Panel_item item_public, Panel_setting lala)
{
    Item_info      *ip = ITEM_PRIVATE(item_public);

    panel_paint_button_image(ip, &ip->label, inactive(ip), ip->menu, 0);
}


static void btn_remove(Panel_item	    item_public)
{
    Item_info      *ip = ITEM_PRIVATE(item_public);
    Panel_info	   *panel = ip->panel;

    /*
     * Only reassign the keyboard focus to another item if the panel isn't
     * being destroyed.
     */
    if (!panel->status.destroying && panel->kbd_focus_item == ip) {
	panel->kbd_focus_item = panel_next_kbd_focus(panel, TRUE);
	panel_accept_kbd_focus(panel);
    }

    return;
}


static void btn_accept_kbd_focus(Panel_item	    item_public)
{
    Frame	    frame;
    Item_info      *ip = ITEM_PRIVATE(item_public);
    int		    x;
    int		    y;

    frame = xv_get(PANEL_PUBLIC(ip->panel), WIN_FRAME);
    if (ip->panel->layout == PANEL_HORIZONTAL) {
	xv_set(frame, FRAME_FOCUS_DIRECTION, FRAME_FOCUS_UP, NULL);
	x = ip->rect.r_left +
	    (ip->rect.r_width - FRAME_FOCUS_UP_WIDTH)/2;
	y = ip->rect.r_top + ip->rect.r_height - FRAME_FOCUS_UP_HEIGHT/2;
    } else {
	xv_set(frame, FRAME_FOCUS_DIRECTION, FRAME_FOCUS_RIGHT, NULL);
	x = ip->rect.r_left - FRAME_FOCUS_RIGHT_WIDTH/2;
	y = ip->rect.r_top +
	    (ip->rect.r_height - FRAME_FOCUS_RIGHT_HEIGHT)/2;
    }
    if (x < 0)
	x = 0;
    if (y < 0)
	y = 0;
    panel_show_focus_win(item_public, frame, x, y);
}


static void btn_yield_kbd_focus(Panel_item	item_public)
{
    Xv_Window	    focus_win;
    Frame	    frame;
    Item_info      *ip = ITEM_PRIVATE(item_public);

    frame = xv_get(PANEL_PUBLIC(ip->panel), WIN_FRAME);
    focus_win = xv_get(frame, FRAME_FOCUS_WIN);
    xv_set(focus_win, XV_SHOW, FALSE, NULL);
}



/* --------------------  Local Routines  -------------------- */
static void button_menu_busy_proc(Menu	    menu)
{
    Panel_item	    item = xv_get(menu, XV_KEY_DATA, item_key);
    Item_info	   *ip = ITEM_PRIVATE(item);

    /* MENU-up: Menu is in Click-Move-Click mode.  Show button in busy state. */
    ip->flags &= ~INVOKED;
    ip->flags |= BUSY;
    panel_paint_button_image(ip, &ip->label, inactive(ip), ip->menu, 0);
}


static void button_menu_done_proc(Menu            menu, Xv_opaque result)
{
    void            (*orig_done_proc) (Menu, Xv_opaque);	/* original menu-done
						 * procedure */
    Panel_item	    item = xv_get(menu, XV_KEY_DATA, item_key);
    Item_info	   *ip = ITEM_PRIVATE(item);
    Panel_info     *panel = ip->panel;
    Panel           panel_public = PANEL_PUBLIC(panel);

    /* Restore menu button to normal state */
    ip->flags &= ~(INVOKED | BUSY);
    if (!hidden(ip))
        panel_paint_button_image(ip, &ip->label, inactive(ip), ip->menu, 0);

    if (xv_get(menu, MENU_NOTIFY_STATUS) == XV_OK)
	take_down_cmd_frame(panel_public);

    panel->current = NULL;

    /* Restore menu color and original menu busy and done procs. */
    orig_done_proc = (void (*) (Menu, Xv_opaque)) xv_get(menu, XV_KEY_DATA, done_key);
    xv_set(menu,
				MENU_BUSY_PROC, xv_get(menu, XV_KEY_DATA, busy_key),
				MENU_DONE_PROC, orig_done_proc,
				MENU_COLOR, xv_get(menu, XV_KEY_DATA, color_key),
				NULL);

    /* Invoke original menu done proc (if any) */
    if (orig_done_proc) (orig_done_proc) (menu, result);

    ip->panel->status.current_item_active = FALSE;
}


Pkg_private void panel_btn_accepted(Item_info *ip, Event *event)
{
    Menu            default_menu, submenu, topmenu;
    Menu_item       default_menu_item;
    int             menu_depth = 0;
    void            (*pin_proc) (Menu, int, int);	/* pin procedure for default menu */
    int             pin_is_default;	/* boolean */
    int             notify_status;	/* XV_OK or XV_ERROR */

    /* notify the client */
    ip->notify_status = XV_OK;
    panel_item_destroy_flag = 0;
    if (ip->flags & IS_MENU_ITEM)
        panel_item_destroy_flag = 1;
    (*ip->notify) (ITEM_PUBLIC(ip), event);
    if (panel_item_destroy_flag == 2)
	return; 
    panel_item_destroy_flag = 0;

    if ((ip->menu) && (submenu = topmenu = generate_menu(ip->menu))) {
	/* Select the default entry */
	menu_select_default(submenu);
	do {
	    menu_depth++;
	    default_menu = submenu;
	    default_menu_item = (Menu_item) xv_get(default_menu,
						   MENU_DEFAULT_ITEM);
	} while (default_menu_item &&
		 (submenu = (Menu) xv_get(default_menu_item, MENU_PULLRIGHT)));
	if (default_menu_item) {
	    pin_is_default = xv_get(default_menu, MENU_PIN) &&
		xv_get(default_menu_item, MENU_TITLE);
	    if (pin_is_default) {
		if (xv_get(default_menu_item, MENU_INACTIVE))
		    notify_status = XV_ERROR;
		else {
		    pin_proc = (void (*) (Menu, int, int)) xv_get(default_menu,
						    MENU_PIN_PROC);
		    pin_proc(default_menu, event_x(event), event_y(event));
		    notify_status = XV_OK;
		}
	    } else {
		/*
		 * Invoke the appropriate notify procedure(s) for the menu
		 * selection.
		 */
		menu_return_default(topmenu, menu_depth, event);
		notify_status = (int) xv_get(topmenu, MENU_NOTIFY_STATUS);
	    }
	} else
	    notify_status = XV_ERROR;
    } else
	notify_status = ip->notify_status;

    if (notify_status == XV_OK)
	take_down_cmd_frame(PANEL_PUBLIC(ip->panel));
}


static void panel_paint_button_image(Item_info *ip, Panel_image *image, int inactive_button, Xv_opaque menu, int height)
{
	int default_button;
	Graphics_info *ginfo;
	Xv_Drawable_info *info;
	void *label;
	Panel_info *panel = ip->panel;
	Pixlabel pixlabel;
	Xv_Window pw;
	unsigned long save_black = 0;
	int state;

	default_button = ITEM_PUBLIC(ip) == panel->default_item;
	state = busy(ip) ? OLGX_BUSY :
			invoked(ip) ? OLGX_INVOKED :
			panel->status.three_d ? OLGX_NORMAL : OLGX_NORMAL | OLGX_ERASE;
	if (image_type(image) == PIT_STRING) {
		height = 0;	/* not used by olgx_draw_button */

#ifdef OW_I18N
		label = (void *)image_string_wc(image);
		state |= OLGX_LABEL_IS_WCS;
#else
		label = (void *)image_string(image);
#endif /* OW_I18N */

		state |= OLGX_MORE_ARROW;
	}
	else {
		if (height == 0)
			height = ((Pixrect *) image_svrim(image))->pr_height +
					OLGX_VAR_HEIGHT_BTN_MARGIN;
		pixlabel.pixmap = (XID) xv_get(image_svrim(image), XV_XID);
		pixlabel.width = ((Pixrect *) image_svrim(image))->pr_width;
		pixlabel.height = ((Pixrect *) image_svrim(image))->pr_height;
		label = (void *)&pixlabel;
		state |= OLGX_LABEL_IS_PIXMAP;
	}
	if (is_menu_item(ip)) {
		state |= OLGX_MENU_ITEM;
		if (!busy(ip) && !invoked(ip))
			state |= OLGX_ERASE;
	}
	if (default_button)
		state |= OLGX_DEFAULT;
	if (inactive_button)
		state |= OLGX_INACTIVE;
	if (menu) {
		if (panel->layout == PANEL_VERTICAL)
			state |= OLGX_HORIZ_MENU_MARK;
		else
			state |= OLGX_VERT_MENU_MARK;
	}
	if (image_ginfo(image))
		ginfo = image_ginfo(image);
	else
		ginfo = panel->ginfo;
	PANEL_EACH_PAINT_WINDOW(panel, pw)
		DRAWABLE_INFO_MACRO(pw, info);
		if (ip->color_index >= 0) {
			save_black = olgx_get_single_color(ginfo, OLGX_BLACK);
			olgx_set_single_color(ginfo, OLGX_BLACK,
				xv_get(xv_cms(info), CMS_PIXEL, ip->color_index), OLGX_SPECIAL);
		}
		olgx_draw_button(ginfo, xv_xid(info),
				ip->label_rect.r_left, ip->label_rect.r_top,
				ip->label_rect.r_width, height, label, state);
		if (ip->color_index >= 0)
			olgx_set_single_color(ginfo, OLGX_BLACK, save_black, OLGX_SPECIAL);
	PANEL_END_EACH_PAINT_WINDOW
}


static void take_down_cmd_frame(Panel panel_public)
{
    Frame           frame;

    /*
     * If this is a command frame, and the pin is out, then take down the
     * window.  Note: The frame code checks to see if the pin is out.
     */
    frame = xv_get(panel_public, WIN_FRAME);
    if (xv_get(frame, XV_IS_SUBTYPE_OF, FRAME_CMD)) {
	menu_save_pin_window_rect(frame);
	xv_set(frame, XV_SHOW, FALSE, NULL);
    }
}

const Xv_pkg          xv_panel_button_pkg = {
    "Button Item", ATTR_PKG_PANEL,
    sizeof(Xv_panel_button),
    PANEL_ITEM,
    panel_button_init,
    NULL,
    NULL,
    panel_button_destroy,
    NULL			/* no find proc */
};
