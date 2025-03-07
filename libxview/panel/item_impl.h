/*	@(#)item_impl.h 20.53 92/06/01 SMI  DRA: $Id: item_impl.h,v 4.5 2024/10/05 13:00:21 dra Exp $	*/

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */

#ifndef _xview_private_item_impl_already_included
#define _xview_private_item_impl_already_included

/* panels and panel_items are both of type Xv_panel_or_item so that we
 * can pass them to common routines.
 */
#define ITEM_PRIVATE(i)		XV_PRIVATE(Item_info, Xv_item, i)
#define ITEM_PUBLIC(item)	XV_PUBLIC(item)

/*                    Item status flags.  (Used in ip->flags.)
 *
 *  N.B.: Definitions marked with a "!P!" are ALSO used in the flags variable
 * 	  in the Panel_info structure (i.e., panel->flags).
 */

#define IS_PANEL	0x00000001  /* object is a panel !P! */
#define IS_ITEM		0x00000002  /* object is an item */
#define HIDDEN		0x00000004  /* item is not currently displayed */
#define ITEM_X_FIXED	0x00000008  /* item's x coord fixed by user */
#define ITEM_Y_FIXED	0x00000010  /* item's y coord fixed by user */
#define LABEL_X_FIXED	0x00000020  /* label x coord fixed by user */
#define LABEL_Y_FIXED	0x00000040  /* label y coord fixed by user */
#define VALUE_X_FIXED	0x00000080  /* value x coord fixed by user */
#define VALUE_Y_FIXED	0x00000100  /* value y coord fixed by user */
#define CREATED		0x00000200  /* XV_END_CREATE received */
#define WANTS_KEY	0x00000400  /* item wants keystroke events !P! */
#define WANTS_ADJUST	0x00000800  /* item wants ACTION_ADJUST events */
#define BUSY_MODIFIED	0x00001000  /* the BUSY flag was modified via xv_set */
#define DEAF		0x00002000  /* item doesn't want any events */
#define INVOKED		0x00004000
#define LABEL_INVERTED	0x00020000  /* invert the label !P! */
#define PREVIEWING	0x00080000
#define BUSY		0x00100000
#define INACTIVE	0x00200000
#define IS_MENU_ITEM	0x00400000  /* paint item like a menu item */
#define WANTS_ISO	0x00800000  /* item wants all ISO characters */
#define UPDATE_SCROLL   0x01000000  /* need to update scroll size */
#define POST_EVENTS	0x02000000  /* send events through the notifier */
#ifdef OW_I18N
#define WCHAR_NOTIFY	0x04000000  /* wide char version of notify proc */
#define IC_ACTIVE	0x08000000  /* indicate if item should ignore IM */
#endif /* OW_I18N */

#define hidden(ip)	((ip)->flags & HIDDEN ? TRUE : FALSE)
#define busy(ip)	((ip)->flags & BUSY ? TRUE : FALSE)
#define busy_modified(ip) ((ip)->flags & BUSY_MODIFIED ? TRUE : FALSE)
#define inactive(ip)	((ip)->flags & INACTIVE ? TRUE : FALSE)
#define invoked(ip)	((ip)->flags & INVOKED ? TRUE : FALSE)
#define item_fixed(ip)	((ip)->flags & (ITEM_X_FIXED | ITEM_Y_FIXED) ? TRUE : FALSE)
#define label_fixed(ip)	((ip)->flags & (LABEL_X_FIXED|LABEL_Y_FIXED) ? TRUE : FALSE)
#define value_fixed(ip)	((ip)->flags & (VALUE_X_FIXED|VALUE_Y_FIXED) ? TRUE : FALSE)
#define created(ip)	((ip)->flags & CREATED ? TRUE : FALSE)
#define is_menu_item(ip)	((ip)->flags & IS_MENU_ITEM ? TRUE : FALSE)
#define wants_iso(object)	((object)->flags & WANTS_ISO ? TRUE : FALSE)
#define wants_key(object)	((object)->flags & WANTS_KEY ? TRUE : FALSE)
#define wants_adjust(object)	((object)->flags & WANTS_ADJUST ? TRUE : FALSE)
#define label_inverted_flag(object)	((object)->flags & LABEL_INVERTED ? TRUE : FALSE)
#define deaf(object)		((object)->flags & DEAF ? TRUE : FALSE)
#define is_panel(object)	((object)->flags & IS_PANEL ? TRUE : FALSE)
#define is_item(object)		((object)->flags & IS_ITEM ? TRUE : FALSE)
#define previewing(object)	((object)->flags & PREVIEWING ? TRUE : FALSE)
#define update_scroll(object)	((object)->flags & UPDATE_SCROLL ? TRUE : FALSE)
#define post_events(object)	((object)->flags & POST_EVENTS ? TRUE : FALSE)
#ifdef OW_I18N
#define wchar_notify(object)	((object)->flags & WCHAR_NOTIFY ? TRUE : FALSE)
#define ic_active(object)	((object)->flags & IC_ACTIVE ? TRUE : FALSE)
#endif /* OW_I18N */

/* 			miscellaneous constants                          */

#define	BIG			0x7FFF
#define	KEY_NEXT		KEY_BOTTOMRIGHT

/* 			structures                                      */


/*********************** panel_image **************************************/

typedef enum {
    PIT_SVRIM,
    PIT_STRING
} Panel_image_type;

typedef struct panel_image {
   Panel_image_type im_type;
   unsigned int	inverted : 1;	/* true to invert the image */
   unsigned int boxed : 1;	/* true to enclose image in a box */
   union {
     struct {
	 char           *text;
#ifdef  OW_I18N
         CHAR        *text_wc;
#endif  /* OW_I18N */
	 Xv_font	 font;
	 short 		 bold;	/* TRUE if text should be bold */
	 Graphics_info	*ginfo;
     } t;        		/* PIT_STRING arm */
     Server_image	 svrim;	/* PIT_SVRIM arm */
   } im_value;
   int color;			/* -1 => use foreground color */
} Panel_image;

#define image_type(image)  	((image)->im_type)
#define image_inverted(image)   ((image)->inverted)
#define image_boxed(image)	((image)->boxed)
#define is_string(image)	(image_type(image) == PIT_STRING)
#define is_svrim(image)		(image_type(image) == PIT_SVRIM)
#define image_string(image)  	((image)->im_value.t.text)
#ifdef  OW_I18N
#define image_string_wc(image)  ((image)->im_value.t.text_wc)
#endif  /* OW_I18N */
#define image_font(image)    	((image)->im_value.t.font)
#define image_bold(image)    	((image)->im_value.t.bold)
#define image_ginfo(image)	((image)->im_value.t.ginfo)
#define image_svrim(image) 	((image)->im_value.svrim)
#define image_color(image)	((image)->color)

#define image_set_type(image, type)	  (image_type(image)    = type)
#define image_set_string(image, string)	  (image_string(image)	= (string))
#ifdef OW_I18N
#define image_set_string_wc(image, string) (image_string_wc(image) = (string))
#endif /* OW_I18N */
#define image_set_svrim(image, svrim)     (image_svrim(image)   = (svrim))
#define image_set_bold(image, bold)  	(image_bold(image)	= (bold) != 0)
#define image_set_inverted(image, inverted) (image_inverted(image) = (inverted) != 0)
#define image_set_boxed(image, boxed)	(image_boxed(image) = (boxed) != 0)
#define image_set_color(image, color)	(image_color(image)	= color)

/*************************** panel item ***********************************/
typedef void (*item_layout_t)(Panel_item, int, Attr_attribute *, int *, int *);

/* *** NOTE: The first three fields of the item_info struct must match those
 * of the panel_info struct, since these are used interchangably in some
 * routines.
 */
typedef struct item_info {
    /****  DO NOT CHANGE THE ORDER OR PLACEMENT OF THESE THREE FIELDS ****/
   Panel_ops		ops;		/* item type specific operations */
   unsigned int		flags;		/* boolean attributes */
   Panel_item		public_self;	/* back pointer to object */
    /**** ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ ****/

   Panel_item		child_kbd_focus_item;
					/* NULL: no embedded panel item that
					 * wants the keyboard focus instead
					 * of the parent item.
					 * other: Setting PANEL_CARET_ITEM to
					 * the parent item causes the embedded
					 * panel item 'child_kbd_focus_item'
					 * to receive the input focus. */
   Xv_opaque	        client_data;	/* for client use */
   int                  color_index;    /* for color panel items */
   Panel_item_type	item_type;	/* type of this item */
   Panel_image		label;		/* the label */
   Rect			label_rect;	/* enclosing label rect */
   int			label_width;	/* desired label width
					 * (0= fit to image) */
   Panel_setting	layout;	        /* HORIZONTAL, VERTICAL */
   Xv_opaque		menu;
   struct item_info    *next; 		/* next item */
   int		      (*notify)(Panel_item, Event *);	/* notify proc */
   int			notify_status;	/* notify proc status: XV_OK or
					 * XV_ERROR */
   Panel_item		owner;		/* NULL: created by application
					 * other: item that created this item */
   Rect			painted_rect;	/* painted area in the pw */
   struct panel_info   *panel;		/* panel subwindow for the item */
   struct item_info    *previous;	/* previous item */
   Rect			rect;		/* enclosing item rect */
   Panel_setting        repaint;	/* item's repaint behavior */
   Xv_Font		value_font;	/* = panel->std_font by default */
#ifdef OW_I18N
   XFontSet		value_fontset_id; /* = panel->std_fontset_id by
					       default */
#else
   XID			value_font_xid; /* = panel->std_font_xid by default */
#endif /* OW_I18N */
   Graphics_info       *value_ginfo;	/* OLGX graphics information */
   Rect			value_rect;	/* enclosing value rect */
   int			x_gap;		/* horizontal space to previous item
					 * (-1 = use panel->item_x_offset) */
   int			y_gap;		/* vertical space to previous item
					 * (-1 = use panel->item_y_offset) */
	Panel_item_role layout_role;
	item_layout_t layout_proc;
#ifdef OW_I18N
   int                  (*notify_wc)(); /* wide char version of notify proc */
#endif /* OW_I18N */
} Item_info;


/************************************************************************
 * Panel Package private functions					*
 ************************************************************************/
Pkg_private Xv_opaque item_set_avlist(Panel_item , Attr_avlist );
Pkg_private Xv_opaque item_get_attr(Panel_item item_public, int *status, Attr_attribute which_attr, va_list valist);
Pkg_private void panel_free_choices(Panel_image *choices, int first, int last);
Pkg_private void panel_fix_label_position(register Item_info *ip);

#endif
