char p_msg_sccsid[] = "@(#)p_msg.c 20.30 93/06/28 Copyr 1987 Sun Micro DRA: $Id: p_msg.c,v 4.4 2025/11/01 14:55:10 dra Exp $";

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

#include <xview_private/panel_impl.h>

/* Panel Item Operations */
static void msg_accept_preview(Panel_item item_public, Event *event);
static void msg_paint(Panel_item item_public, Panel_setting u);

static Panel_ops ops = {
    panel_default_handle_event,		/* handle_event() */
    NULL,				/* begin_preview() */
    NULL,				/* update_preview() */
    NULL,				/* cancel_preview() */
    msg_accept_preview,			/* accept_preview() */
    NULL,				/* accept_menu() */
    NULL,				/* accept_key() */
    panel_default_clear_item,		/* clear() */
    msg_paint,				/* paint() */
    NULL,				/* resize() */
    NULL,				/* remove() */
    NULL,				/* restore() */
    NULL,				/* layout() */
    NULL,				/* accept_kbd_focus() */
    NULL,				/* yield_kbd_focus() */
    NULL				/* extension: reserved for future use */
};


/* ========================================================================= */

/* -------------------- XView Functions  -------------------- */
static int panel_message_init(Panel panel_public, Panel_item item_public,
												Attr_avlist avlist, int *u)
{
    Panel_info     *panel = PANEL_PRIVATE(panel_public);
    register Item_info *ip = ITEM_PRIVATE(item_public);

    ip->ops = ops;
    if (panel->event_proc)
		ip->ops.panel_op_handle_event = (void (*)(Panel_item, Event *)) panel->event_proc;
    ip->item_type = PANEL_MESSAGE_ITEM;

    return XV_OK;
}


/* --------------------  Panel Item Operations  -------------------- */
static void msg_accept_preview(Panel_item item_public, Event *event)
{
    Item_info      *ip = ITEM_PRIVATE(item_public);

    (*ip->notify) (item_public, event);
}


static void msg_paint(Panel_item item_public, Panel_setting u)
{
    panel_paint_label(item_public);
}

const Xv_pkg xv_panel_message_pkg = {
    "Message Item", ATTR_PKG_PANEL,
    sizeof(Xv_panel_message),
    PANEL_ITEM,
    panel_message_init,
    NULL,
    NULL,
    NULL,
    NULL			/* no find proc */
};
