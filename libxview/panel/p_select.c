#ifndef lint
#ifdef sccs
char p_select_c_sccsid[] = "@(#)p_select.c 20.81 93/06/28 DRA: $Id: p_select.c,v 4.27 2025/03/25 17:45:15 dra Exp $";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */

#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <xview_private/panel_impl.h>
#include <xview_private/draw_impl.h>
#include <xview/scrollbar.h>
#include <xview/font.h>
#include <xview/help.h>
#include <xview/defaults.h>
#include <xview_private/svr_impl.h>
#include <xview_private/xv_quick.h>

extern int panel_item_destroy_flag;

static int event_in_view_window(Panel_info *panel, Event *event);
static Item_info * panel_find_item(Panel_info *panel, Event *event);

#define	CTRL_D_KEY	'\004'
#define	CTRL_G_KEY	'\007'

#define UP_CURSOR_KEY           (KEY_RIGHT(8))
#define DOWN_CURSOR_KEY         (KEY_RIGHT(14))
#define RIGHT_CURSOR_KEY        (KEY_RIGHT(12))
#define LEFT_CURSOR_KEY         (KEY_RIGHT(10))

static int panel_event_is_drag(Event *event)
{
    switch (event_action(event)) {
      case LOC_DRAG:
      case ACTION_DRAG_COPY:
      case ACTION_DRAG_MOVE:
      case ACTION_DRAG_PREVIEW:
	return TRUE;
      default:
	return FALSE;
    }
}


/*ARGSUSED*/
Pkg_private int panel_duplicate_key_is_down(Panel_info *panel, Event *event)
{
	return (int)xv_get(XV_SERVER_FROM_WINDOW(PANEL_PUBLIC(panel)),
						SERVER_EVENT_HAS_DUPLICATE_MODIFIERS, event);
}


Pkg_private int panel_event_is_xview_semantic(Event *event)
{
    switch (event_action(event)) {
      case ACTION_CANCEL:   /* == ACTION_STOP */
      case ACTION_AGAIN:
      case ACTION_PROPS:
      case ACTION_UNDO:
      case ACTION_FRONT:
      case ACTION_BACK:
      case ACTION_COPY:
      case ACTION_OPEN:
      case ACTION_CLOSE:
      case ACTION_PASTE:
      case ACTION_FIND_BACKWARD:
      case ACTION_FIND_FORWARD:
      case ACTION_CUT:
	return 1;
      default:
	return 0;
    }
}


Pkg_private int panel_erase_action(Event *event)
{
    switch (event_action(event)) {
      case ACTION_ERASE_CHAR_BACKWARD:
      case ACTION_ERASE_CHAR_FORWARD:
      case ACTION_ERASE_WORD_BACKWARD:
      case ACTION_ERASE_WORD_FORWARD:
      case ACTION_ERASE_LINE_BACKWARD:
      case ACTION_ERASE_LINE_END:
      case ACTION_ERASE_LINE:
	return 1;
      default:
	return 0;
    }
}


Pkg_private int panel_navigation_action( Event	 *event)
{
    switch (event_action(event)) {
      case ACTION_GO_CHAR_BACKWARD:
      case ACTION_GO_CHAR_FORWARD:
      case ACTION_GO_WORD_BACKWARD:
      case ACTION_GO_WORD_END:
      case ACTION_GO_WORD_FORWARD:
      case ACTION_GO_LINE_BACKWARD:
      case ACTION_GO_LINE_END:
      case ACTION_GO_LINE_FORWARD:
      case ACTION_UP:
      case ACTION_DOWN:
	return 1;
      default:
	return 0;
    }
}

static int panel_event_is_select_action(Event *event)
{
    switch (event_action(event)) {
		case ACTION_SELECT_ALL:
		case ACTION_SELECT_CHAR_BACKWARD:
		case ACTION_SELECT_CHAR_FORWARD:
		case ACTION_SELECT_LINE_END:
		case ACTION_SELECT_LINE_START:
		case ACTION_SELECT_WORD_BACKWARD:
		case ACTION_SELECT_WORD_END:
			return TRUE;
	}
	return FALSE;
}

Pkg_private Notify_value panel_default_event(Panel p_public, Event *event,
    										Notify_arg arg)
{
	Item_info *ip;
	Item_info *newip = NULL;
	Xv_Window paint_window = event_window(event);
	Panel_info *panel;
	Panel panel_public;

	XV_OBJECT_TO_STANDARD(p_public, "panel_default_event", panel_public);
	panel = PANEL_PRIVATE(panel_public);
	if (!is_panel(panel)) {
		panel = ITEM_PRIVATE(panel_public)->panel;
	}

	if (!panel->status.pointer_grabbed && !panel->status.current_item_active) {

		if (panel->kbd_focus_item && !wants_iso(panel->kbd_focus_item)) {
			xv_translate_iso(event);
		}

		switch (event_action(event)) {
			case ACTION_NEXT_ELEMENT:
				if (event_is_down(event)) {
					ip = panel_next_kbd_focus(panel, FALSE);
					if (ip)
						panel_set_kbd_focus(panel, ip);
					else if (panel->paint_window->view)
						return NOTIFY_IGNORED;	/* let canvas handle event */
					else
						xv_set(xv_get(panel_public, WIN_FRAME),
								FRAME_NEXT_PANE, NULL);
				}
				return (int)NOTIFY_DONE;
			case ACTION_PREVIOUS_ELEMENT:
				if (event_is_down(event)) {
					ip = panel_previous_kbd_focus(panel, FALSE);
					if (ip)
						panel_set_kbd_focus(panel, ip);
					else if (panel->paint_window->view)
						return NOTIFY_IGNORED;	/* let canvas handle event */
					else
						xv_set(xv_get(panel_public, WIN_FRAME),
								FRAME_PREVIOUS_ELEMENT, NULL);
				}
				return (int)NOTIFY_DONE;
			case ACTION_PANEL_START:
				if (event_is_down(event) && panel->items) {
					for (ip = panel->items; ip; ip = ip->next) {
						if (wants_key(ip) && !hidden(ip) && !inactive(ip)) {
							panel_set_kbd_focus(panel, ip);
							return (int)NOTIFY_DONE;
						}
					}
				}
				return (int)NOTIFY_DONE;
			case ACTION_PANEL_END:
				if (event_is_down(event) && panel->items) {
					for (ip = panel->items; ip->next; ip = ip->next);
					for (; ip; ip = ip->previous) {
						if (wants_key(ip) && !hidden(ip) && !inactive(ip)) {
							panel_set_kbd_focus(panel, ip);
							return (int)NOTIFY_DONE;
						}
					}
				}
				return (int)NOTIFY_DONE;
			case ACTION_JUMP_MOUSE_TO_INPUT_FOCUS:
				if (panel->kbd_focus_item) {
					xv_set(panel->focus_pw,
							WIN_MOUSE_XY,
							panel->kbd_focus_item->rect.r_left,
							panel->kbd_focus_item->rect.r_top, NULL);
				}
				return (int)NOTIFY_DONE;

			case LOC_WINEXIT:
				/*
				 * probably there was a reason - the orig libxview didn't 
				 * handle LOC_WINEXIT. But I don't remember...
				 * Now we had a problem when we do DnD from a PANEL_TEXT:
				 * While the drop's sel requests take place, this 'panel_cancel'
				 * sets panel->sel_holder[PANEL_SEL_PRIMARY] = NULL.
				 * But now the solution was to invent
				 * panel->sel_holder[PANEL_SEL_DND]
				 *
				 * But still, when we have a secondary selection in a 
				 * PANEL_TEXT and move out of the panel, we lost the
				 * selection, so, quick duplicate failed...
				 */
				if (event_is_quick_move(event)
					|| event_is_quick_duplicate(event)) 
				{
					/* do not handle LOC_WINEXIT */
					return NOTIFY_DONE;
				}
				if (panel->current) {
					Panel_item item = ITEM_PUBLIC(panel->current);

					if (xv_get(item, XV_IS_SUBTYPE_OF, PANEL_ITEM)) {
						/* This output came frequently - especially with
						 * Abbreviated Menu Buttons, but always 
						 * "grabbed 0, cua 0"
						 * and never with negative effects
						 */
						if (panel->status.pointer_grabbed 
							|| panel->status.current_item_active)
						{
							Xv_pkg *pkg = (Xv_pkg *)xv_get(item, XV_TYPE);

							fprintf(stderr,
								"%s`%s-%d: cancelled because of LOC_WINEXIT\n",
								__FILE__, __FUNCTION__, __LINE__);
							fprintf(stderr, "%s: grabbed %d, cua %d\n",
								pkg->name, panel->status.pointer_grabbed,
								panel->status.current_item_active);
						}
						panel_cancel(item, event);
						return NOTIFY_DONE;
					}
				}
				break;

			case ACTION_DEFAULT_ACTION:
				if (event_is_down(event) && panel->default_item) {


					/* make sure the focus item sees the down event */
					if (panel->kbd_focus_item)
						panel_handle_event(ITEM_PUBLIC(panel->kbd_focus_item),
								event);

					event_set_action(event, ACTION_SELECT);
					newip = ITEM_PRIVATE(panel->default_item);
					if (newip != panel->current) {
						if (panel->current)
							panel_cancel(ITEM_PUBLIC(panel->current), event);
						panel->current = newip;
					}

					panel_handle_event(ITEM_PUBLIC(newip), event);	/* SELECT-down */
					event_set_up(event);
					panel_handle_event(ITEM_PUBLIC(newip), event);	/* SELECT-up */
					return (int)NOTIFY_DONE;
				}
				break;
			case ACTION_ACCELERATOR:	/* only received on down (KeyPress) events */
				event_set_action(event, ACTION_SELECT);
				newip = ITEM_PRIVATE((Panel_item) arg);
				if (newip != panel->current) {
					if (panel->current)
						panel_cancel(ITEM_PUBLIC(panel->current), event);
					panel->current = newip;
				}
				panel_handle_event(ITEM_PUBLIC(newip), event);	/* SELECT-down */
				event_set_up(event);
				panel_handle_event(ITEM_PUBLIC(newip), event);	/* SELECT-up */
				return (int)NOTIFY_DONE;
			case SCROLLBAR_REQUEST:
				newip = (Item_info *) xv_get(arg, XV_KEY_DATA,
						PANEL_LIST_EXTENSION_DATA);
				break;
			case ACTION_DRAG_COPY:
			case ACTION_DRAG_MOVE:
				if (dnd_is_forwarded(event))
					newip = panel->default_drop_site_item;
				else
					newip = panel_find_item(panel, event);
				break;
			case ACTION_PASTE:	/* for quick-duplicate */
				/* panel->current is active item, panel will propagate paste
				 * otherwise, preview gets canceled below
				 */
				newip = (Item_info *) panel;
				break;
			default:
				/* Find out who's under the locator */
				newip = panel_find_item(panel, event);
				break;
		}
	}
	else {
		newip = panel->current;
	}

	/* Use the panel if not over some item */
	if (!newip) {
		newip = (Item_info *) panel;
	}

	/* Set Quick Move status */
	if (event_is_quick_move(event)) {
		panel->status.quick_move = TRUE;
		SERVERTRACE((765, "panel->status.quick_move = TRUE\n"));
	}
	else {
		if (panel->status.quick_move) {
			SERVERTRACE((765, "panel->status.quick_move = FALSE\n"));
		}
		panel->status.quick_move = FALSE;
	}

	/* cancel the old item if needed */
	if (newip != panel->current) {
		if (panel->current &&
				((panel->current != (Item_info *) panel
						&& panel->current->item_type == PANEL_DROP_TARGET_ITEM)
				|| (event_action(event) != ACTION_DRAG_COPY
						&& event_action(event) != ACTION_DRAG_MOVE
						&& event_action(event) != ACTION_DRAG_PREVIEW)
				)
			)
		{
			panel_cancel(ITEM_PUBLIC(panel->current), event);
		}
		panel->current = newip;
	}
	/* If help event, call help request function. */
	if (event_action(event) == ACTION_HELP ||
			event_action(event) == ACTION_MORE_HELP ||
			event_action(event) == ACTION_TEXT_HELP ||
			event_action(event) == ACTION_MORE_TEXT_HELP ||
			event_action(event) == ACTION_INPUT_FOCUS_HELP) {

#ifdef OW_I18N
		/*  For some reason the down event is not passed
		 *  back to panel, so for now we're looking for
		 *  the up event.  When the down event is passed
		 *  to panel again we should remove this code.
		 *  As they say, a hack for now.
		 */
		if (event_is_up(event))
#else
		if (event_is_down(event))
#endif /* OW_I18N */

		{
			char *panel_help = (char *)xv_get(panel_public, XV_HELP_DATA);
			char *item_help = 0;
			char helpbuf[200];

			if (event_action(event) == ACTION_INPUT_FOCUS_HELP)
				newip = panel->kbd_focus_item;
			if (newip && newip != (Item_info *) panel) {
				Panel_item hit = ITEM_PUBLIC(newip);
				item_help = (char *)xv_get(hit, XV_HELP_DATA);
				if (item_help) {
					strcpy(helpbuf, item_help);
					item_help = helpbuf;
				}
				else if (defaults_get_boolean("openWindows.providePanelItemHelp",
							"OpenWindows.ProvidePanelItemHelp", TRUE))
				{
					if (xv_get(hit,XV_IS_SUBTYPE_OF,PANEL_BUTTON)) {
						if (xv_get(hit, PANEL_ITEM_MENU)) {
							strcpy(helpbuf, "xview:Button_Item_with_Menu");
						}
						else {
							strcpy(helpbuf, "xview:Button_Item");
						}
						item_help = helpbuf;
					}
					else if (xv_get(hit, XV_IS_SUBTYPE_OF, PANEL_CHOICE)) {
						if (xv_get(hit, PANEL_CHOOSE_ONE)) {
							if (xv_get(hit,PANEL_DISPLAY_LEVEL)==PANEL_CURRENT)
								strcpy(helpbuf, "xview:Choice_Item_Stack");
							else if (xv_get(hit,PANEL_FEEDBACK)==PANEL_MARKED)
								strcpy(helpbuf,
										"xview:Choice_Item_Exclusive_Checkbox");
							else {
								strcpy(helpbuf, "xview:Choice_Item_Exclusive");
							}
						}
						else {
							if (xv_get(hit,PANEL_FEEDBACK)==PANEL_MARKED)
								strcpy(helpbuf,
									"xview:Choice_Item_Nonexclusive_Checkbox");
							else strcpy(helpbuf, "xview:Choice_Item_Nonexclusive");
						}
						item_help = helpbuf;
					}
					else {
						Xv_pkg *pkg = (Xv_pkg *)xv_get(hit, XV_TYPE);
						char *p;

						sprintf(helpbuf, "xview:%s", pkg->name);
						while ((p = strchr(helpbuf, ' '))) {
							*p = '_';
						}
						item_help = helpbuf;
					}
				}
				if (xv_get(hit, PANEL_INACTIVE)) {
					strcat(helpbuf, "_D");
				}
			}
			if (item_help) {
				panel_help = item_help;
			}
			if (panel_help) {
				xv_help_show(paint_window, panel_help, event);
				return (int)NOTIFY_DONE;
			}
			else
				return (int)NOTIFY_IGNORED;
		}
	}
	else if (!event_is_button(event) &&
			!panel_event_is_drag(event) &&
			event_action(event) != SCROLLBAR_REQUEST) {
		if (panel->kbd_focus_item)
			/* Give the key event to the keyboard focus item */
			panel_handle_event(ITEM_PUBLIC(panel->kbd_focus_item), event);
		else if (wants_key(panel))
			/* Give the key event to the panel background proc */
			panel_handle_event(PANEL_PUBLIC(panel), event);
	}
	else {
		/* Give the non-key event to the item under the pointer */
		panel_handle_event(ITEM_PUBLIC(newip), event);
	}
	return (int)NOTIFY_DONE;
}

/* compare also same variable in frame/fm_input.c ! */

static Attr_attribute quick_dupl_key = 0;

typedef struct {
	Item_info *priv;
} quick_data_t;

extern char *xv_app_name;

static void q_remove_underline(Quick_owner qo)
{
	quick_data_t *qd = (quick_data_t *)xv_get(qo, QUICK_CLIENT_DATA);
	Item_info *priv = qd->priv;
	Xv_window panel = xv_get(qo, XV_OWNER);
	Xv_Drawable_info *info;
	Rect *r = &priv->label_rect;

	DRAWABLE_INFO_MACRO(panel, info);
	/* the following panel_redisplay_item seems to keep the underline
	 * untouched...
	 */
	XClearArea(xv_display(info), xv_xid(info), r->r_left, r->r_top, 
						(unsigned)r->r_width, (unsigned)r->r_height + 3, TRUE);

	/* this is not enough: for PANEL_CHOICEs the underlining would still
	 * be visible. Therefore, we perform a full repaint with PANEL_CLEAR
		panel_paint_label(ITEM_PUBLIC(qd->priv));
	 */
	panel_redisplay_item(qd->priv, PANEL_CLEAR);
	qd->priv = NULL;
}

static XFontStruct *get_fontstruct(Item_info *ip)
{
	Graphics_info *gi;
	XFontStruct *fs;

	gi = image_ginfo(&ip->label);
	if (gi) {
		fs = gi->textfont;
	}
	else {
		Xv_font font = image_font(&ip->label);

		gi = ip->panel->ginfo;
		if (font) {
			fs = (XFontStruct *)xv_get(font, FONT_INFO);
		}
		else {
			fs = gi->textfont;
		}
	}

	return fs;
}

static int is_quick_duplicate_on_label(Panel_item item, Event *ev)
{
	Quick_owner qo;
	quick_data_t *qd;
	Item_info *ip;
	Panel pan;
	char *s;
	int sx, ex;

	if (! item) return FALSE;

	switch (event_action(ev)) {
		case ACTION_SELECT:
			if (! quick_dupl_key) quick_dupl_key = xv_unique_key();

			if (! event_is_quick_duplicate(ev)) return FALSE;
			if (! event_is_down(ev)) return TRUE;

			ip = ITEM_PRIVATE(item);
			if (ip->label.im_type != PIT_STRING) return FALSE;

			if (! rect_includespoint(&ip->label_rect,event_x(ev),event_y(ev))) {
				return FALSE;
			}

			pan = xv_get(item, XV_OWNER);

			qo = xv_get(pan, XV_KEY_DATA,quick_dupl_key);
			if (! qo) {
				qd = xv_alloc(quick_data_t);
				qo = xv_create(pan, QUICK_OWNER,
							QUICK_REMOVE_UNDERLINE_PROC, q_remove_underline,
							QUICK_CLIENT_DATA, qd,
							NULL);
				xv_set(pan, XV_KEY_DATA,quick_dupl_key, qo, NULL);
			}
			else {
				qd = (quick_data_t *)xv_get(qo,	QUICK_CLIENT_DATA);
			}

			if (xv_get(qo, QUICK_NEED_START)) {
				qd->priv = ip;
				s = image_string(&ip->label);
				sx = ip->label_rect.r_left;
				ex = rect_right(&ip->label_rect);

				xv_set(qo,
					QUICK_BASELINE,
								(int)xv_get(item, PANEL_ITEM_LABEL_BASELINE)+2,
					QUICK_FONTINFO, get_fontstruct(ip),
					QUICK_START, s, sx, ex,
					NULL);
			}

			xv_set(qo, QUICK_SELECT_DOWN, ev, NULL);
			return TRUE;

		case LOC_DRAG:
			if (! event_is_quick_duplicate(ev)) return FALSE;
			if (! quick_dupl_key) return FALSE;
			qo = xv_get(xv_get(item, XV_OWNER), XV_KEY_DATA,quick_dupl_key);
			if (! qo) return FALSE;
			if (! xv_get(qo, SEL_OWN)) return FALSE;
			xv_set(qo, QUICK_LOC_DRAG, ev, NULL);
			return TRUE;

		case ACTION_ADJUST:
			if (! event_is_quick_duplicate(ev)) return FALSE;
			if (! quick_dupl_key) {
				xv_set(event_window(ev), WIN_ALARM, NULL);
				return TRUE;
			}
			ip = ITEM_PRIVATE(item);
			if (ip->label.im_type != PIT_STRING) return FALSE;
			if (! rect_includespoint(&ip->label_rect,event_x(ev),event_y(ev))) {
				return FALSE;
			}
			pan = xv_get(item, XV_OWNER);
			qo = xv_get(pan, XV_KEY_DATA, quick_dupl_key);
			if (! qo) {
				xv_set(pan, WIN_ALARM, NULL);
				return TRUE;
			}
			if (event_is_down(ev)) {
				if (! xv_get(qo, SEL_OWN)) xv_set(pan, WIN_ALARM, NULL);
				return TRUE;
			}
			qd = (quick_data_t *)xv_get(qo, QUICK_CLIENT_DATA);
			if (! qd) {
				xv_set(pan, WIN_ALARM, NULL);
				return TRUE;
			}
			if (qd->priv) {
				if (ip != qd->priv) {
					xv_set(qo, SEL_OWN, FALSE, NULL);
					return TRUE;
				}
			}
			qd->priv = ip;

			xv_set(qo, QUICK_ADJUST_UP, ev, NULL);
			return TRUE;
	}

	return FALSE;
}

Xv_public int panel_handle_event(Panel_item client_object, Event *event)
{
	Item_info *object = ITEM_PRIVATE(client_object);

	if (!object) return XV_OK;

	/* this "Panel_item" can also be a panel... */
	if (xv_get(client_object, XV_IS_SUBTYPE_OF, PANEL_ITEM)) {
		if (is_quick_duplicate_on_label(client_object, event)) return XV_OK;
	}

	if (!object->ops.panel_op_handle_event) return XV_OK;

	/* this flag can only be set for panel items */
	if (post_events(object)) {
		notify_post_event(client_object, (Notify_event) event,
				NOTIFY_IMMEDIATE);
	}
	else {
		(*object->ops.panel_op_handle_event) (client_object, event);
	}

	return XV_OK;
}


Sv1_public int panel_begin_preview(Panel_item client_object, Event *event)
{
	Item_info *object = ITEM_PRIVATE(client_object);

	if (!object)
		return XV_OK;

	if (object->ops.panel_op_begin_preview)
		(*object->ops.panel_op_begin_preview) (client_object, event);
	return XV_OK;
}


Sv1_public int panel_update_preview(Panel_item client_object, Event *event)
{
	Item_info *object = ITEM_PRIVATE(client_object);

	if (!object)
		return XV_OK;

	if (object->ops.panel_op_update_preview)
		(*object->ops.panel_op_update_preview) (client_object, event);
	return XV_OK;
}


Sv1_public int panel_accept_preview(Panel_item client_object, Event *event)
{
    Item_info      *object = ITEM_PRIVATE(client_object);
    Panel_info     *panel;

    if (!object)
	return XV_OK;

    panel_item_destroy_flag = 0;
    if (object->ops.panel_op_accept_preview) {
	(*object->ops.panel_op_accept_preview) (client_object, event);
	if (panel_item_destroy_flag == 2) {
	    panel_item_destroy_flag = 0;
	    return XV_OK;
	}
    }
    panel_item_destroy_flag = 0;

    if (is_item(object))
	panel = object->panel;
    else
	panel = PANEL_PRIVATE(client_object);
    panel->current = NULL;
	return XV_OK;
}


Sv1_public
int panel_cancel_preview(Panel_item client_object, Event *event)
{
    Item_info      *object = ITEM_PRIVATE(client_object);
    Panel_info     *panel;

    if (!object)
	return XV_OK;

    if (object->ops.panel_op_cancel_preview)
	(*object->ops.panel_op_cancel_preview) (client_object, event);

    if (is_item(object))
	panel = object->panel;
    else
	panel = PANEL_PRIVATE(client_object);
    panel->current = NULL;
	return XV_OK;
}

static int menu_only_over_value_rect(Item_info *ip, Event *ev)
{
	if (event_action(ev) != ACTION_MENU) return TRUE;
	if (! ip) return TRUE;

	/* this is 0 for PANEL_BUTTONS */
	if (ip->value_rect.r_width == 0) return TRUE;

	return rect_includespoint(&ip->value_rect, event_x(ev), event_y(ev));
}

Sv1_public int panel_accept_menu(Panel_item client_object, Event *ev)
{
	Item_info *ip = ITEM_PRIVATE(client_object);
	Panel_info *panel;

	if (!ip)
		return XV_OK;

	if (event_is_down(ev)) {
		if (ip->ops.panel_op_accept_menu) {
			/* Very important question: do we want a menu if the mouse 
			 * is over the LABEL (and not over the VALUE) ?????
			 * Does the OPEN LOOK spec say anything about that??????
			 *
			 * I have found a hint in Figure 3-20 on page 59: there, the
			 * CATEGORY label belongs to the window background.
			 * And in Figure 10-15 on page 238 you can see that a text field's
			 * Edit menu is ONLY displayed from the 'value_rect'....
			 */
			if (menu_only_over_value_rect(ip, ev)) {
				(*ip->ops.panel_op_accept_menu) (client_object, ev);
			}
		}
	}
	else {
		if (is_item(ip))
			panel = ip->panel;
		else
			panel = PANEL_PRIVATE(client_object);
		panel->current = NULL;
	}
	return XV_OK;
}


Sv1_public int panel_accept_key(Panel_item client_object, Event *event)
{
	Item_info *object = ITEM_PRIVATE(client_object);

	if (!object) return XV_OK;

	if (object->ops.panel_op_accept_key) {
		(*object->ops.panel_op_accept_key) (client_object, event);
	}
	return XV_OK;
}


Sv1_public int panel_cancel(Panel_item client_object, Event *event)
{
	Event cancel_event;

	if (!client_object)
		return XV_OK;

	cancel_event = *event;
	cancel_event.action = PANEL_EVENT_CANCEL;
	(void)panel_handle_event(client_object, &cancel_event);
	return XV_OK;
}


Xv_public void panel_default_handle_event(Panel_item client_object,Event *event)
{
	int accept;
	Item_info *object = ITEM_PRIVATE(client_object);
	Item_info *ip = NULL;
	Panel_info *panel = PANEL_PRIVATE(client_object);

	if (is_item(object)) {
		if (inactive(object) || busy(object))
			return;
		ip = object;
		panel = ip->panel;
	}
	switch (event_action(event)) {
		case ACTION_ADJUST:	/* middle button up or down */
			if (panel->status.current_item_active)
				break;	/* ignore ADJUST during active item operation */
			if (!ip || !wants_adjust(ip)) {
				panel_user_error(object, event);
				if (event_is_up(event))
					panel->current = NULL;
				break;
			}	/* else execute ACTION_SELECT code */

		case ACTION_SELECT:	/* left button up or down */
			if (event_action(event) == ACTION_SELECT &&
					panel->status.select_displays_menu &&
					!panel->status.current_item_active && ip && ip->menu) {
				/* SELECT over menu button => show menu */
				(void)panel_accept_menu(client_object, event);
				break;
			}
			if (event_is_up(event)) {
				if (ip) {
					ip->flags &= ~PREVIEWING;
					/*
					 * If the SELECT button went up inside any of the panel's
					 * view windows, then accept the preview; else, cancel the
					 * preview.
					 */
					accept = !event_is_button(event) ||
							event_in_view_window(panel, event);
				}
				else
					accept = FALSE;
				if (accept)
					panel_accept_preview(client_object, event);
				else
					panel_cancel_preview(client_object, event);
			}
			else if (ip) {
				/* SELECT or ADJUST down over an item: begin preview */
				if (ip->item_type == PANEL_TEXT_ITEM &&
						!panel->status.has_input_focus)
					ip->flags |= PREVIEWING;
				/* Process SELECT or ADJUST down event */

				/* Move kbd focus to this item if we're not doing a secondary
				 * selection (i.e., Quick Move or Quick Copy), and the kbd focus
				 * is not already there.
				 */
				if (event_is_button(event)
					&& (event_is_quick_move(event)||event_is_quick_duplicate(event))
					&& panel->kbd_focus_item != ip
					&& wants_key(ip)
					&& !hidden(ip)
					&& !inactive(ip)
					)
				{
					/* Move Location Cursor to current item */
					if (panel->status.has_input_focus)
						panel_set_kbd_focus(panel, ip);
					else {
						panel->kbd_focus_item = ip;
						panel->status.focus_item_set = TRUE;
					}
				}
				panel_begin_preview(client_object, event);
			}
			else if (panel->sel_holder[PANEL_SEL_PRIMARY]) {
				/* SELECT-down over the panel background when there's a
				 * primary selection: lose the primary selection.
				 */
				xv_set(panel->sel_owner[PANEL_SEL_PRIMARY], SEL_OWN, FALSE,
						NULL);
			}
			break;

		case ACTION_MENU:	/* right button up or down */
			if (panel->status.current_item_active)
				break;	/* ignore MENU during active item operation */

#ifdef NO_BINARY_COMPATIBILITY
			if (!ip || !ip->menu) {
				panel_user_error(object, event);
				if (event_is_up(event))
					panel->current = NULL;
			}
			else
#endif /* NO_BINARY_COMPATIBILITY */

				(void)panel_accept_menu(client_object, event);
			break;

		case LOC_DRAG:	/* left, middle, or right drag */
			if (action_select_is_down(event) || action_adjust_is_down(event))
				(void)panel_update_preview(client_object, event);
			if (action_menu_is_down(event))
				panel_accept_menu(client_object, event);
			break;

		case PANEL_EVENT_CANCEL:
			if (panel->status.current_item_active) {
				event_set_id(event, LOC_DRAG);	/* pass more events */
				(void)panel_update_preview(client_object, event);
				break;
			}
			else
				(void)panel_cancel_preview(client_object, event);
			break;

		default:	/* some other event */
			if (event_is_iso(event)
				|| (event_is_key_right(event) && event_is_down(event))
				|| panel_erase_action(event)
				|| panel_navigation_action(event)
				|| panel_event_is_xview_semantic(event)
				/* DRA_CHANGED: because ACTION_SELECT_LINE_END did not
				 * work in PANEL_TEXT
				 */
				|| panel_event_is_select_action(event)
			)
				(void)panel_accept_key(client_object, event);
			break;
	}
}

static Item_info * panel_find_item(Panel_info *panel, Event *event)
{
	register Item_info *ip = panel->current;
	register int x = event_x(event);
	register int y = event_y(event);

	if (!panel->items || !event_in_view_window(panel, event))
		return NULL;

	if (ip && is_item(ip) && !hidden(ip) &&
			(rect_includespoint(&ip->rect, x, y) ||
					(previewing(ip) && ip->item_type == PANEL_TEXT_ITEM)))
	{
		if (menu_only_over_value_rect(ip, event)) return ip;
	}

	/* this loop does not find PANEL_MULTILINE_TEXT or PANEL_SUBWINDOW
	 * because they are DEAF
	 */
	for (ip = (hidden(panel->items) || deaf(panel->items))
					? panel_successor(panel->items)
					: panel->items;
			ip && !rect_includespoint(&ip->rect, x, y);
			ip = panel_successor(ip)
		);

	if (! ip && event_action(event) == ACTION_SELECT) {
		/* now we look at the label_rect of deaf items */
		Panel pan = PANEL_PUBLIC(panel);
		Panel_item it;

		PANEL_EACH_ITEM(pan, it)
			if (xv_get(it, PANEL_ITEM_DEAF)) {
				Rect *r = (Rect *)xv_get(it, PANEL_ITEM_LABEL_RECT);

				if (rect_includespoint(r, x, y)) {
    				ip = ITEM_PRIVATE(it);

					return ip;
				}
			}
		PANEL_END_EACH
	}

	if (! menu_only_over_value_rect(ip, event)) {
		return NULL;
	}

	return ip;
}


/*
 * If the event occurred inside any of the panel's view windows, then return
 * TRUE, else return FALSE.
 */
static int event_in_view_window(Panel_info *panel, Event *event)
{
    register Xv_Window       pw;

    PANEL_EACH_PAINT_WINDOW(panel, pw)
	if (rect_includespoint(panel_viewable_rect(panel, pw),
			       event_x(event), event_y(event)))
	    return TRUE;
    PANEL_END_EACH_PAINT_WINDOW
    return FALSE;
}


/*
 * Base handler for Item's that request events to be posted
 * (implementing PANEL_POST_EVENTS).
 */
Xv_private Notify_value panel_base_event_handler(Notify_client client,
     Notify_event event, Notify_arg arg, Notify_event_type type)
{
    Item_info *object = ITEM_PRIVATE(client);

    /*
     * do what panel_handle_event() would have, had we not
     * posted the event through the notifier.
     */
    if (object->ops.panel_op_handle_event)
	(*object->ops.panel_op_handle_event)((Panel_item)client, (Event *)event);

    /*
     * ASSume the Panel handled the event, since the Panel
     * doesn't return any useful status information...
     */
    return NOTIFY_DONE;
}
