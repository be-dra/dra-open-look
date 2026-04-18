#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)frame_init.c 1.46 93/06/28 DRA: $Id: frame_init.c,v 4.16 2026/04/18 08:15:36 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */
#include <sys/param.h>
#include <ctype.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <xview_private/draw_impl.h>
#include <xview/cursor.h>
#include <xview_private/fm_impl.h>
#include <xview/server.h>
#include <xview_private/svr_atom.h>
#include <xview/defaults.h>
#include <xview/font.h>
#include <xview_private/windowimpl.h>
#include <xview_private/win_info.h>
#include <xview_private/svr_impl.h>
#include <xview/notice.h>
#include <xview/canvas.h>
#include <pixrect/pixrect.h>
#include <pixrect/memvar.h>
#include <xview_private/om_impl.h>

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 64
#endif

#ifndef NULL
#define NULL 0
#endif

Xv_private int xv_get_hostname(char *buf, int maxlen);

static short focus_up_bits[] = {
#include <images/focus_up.cursor>
};

static short focus_right_bits[] = {
#include <images/focus_right.cursor>
};

/*
 * This counter is incremented each time a frame is created and decremented
 * each time a frame is destroyed.  When the value reaches zero, the notifier
 * is stopped, see frame_free().
 */
/* Pkg_private */
int             frame_notify_count = 0;


static void frame_default_done_func(Frame frame)
{
    Xv_Drawable_info 	*info;

    DRAWABLE_INFO_MACRO(frame, info);
    if (xv_get(frame, XV_OWNER) == xv_get(xv_screen(info), XV_ROOT)) {
         (void) xv_destroy_safe(frame);
    }
    else  {
        (void) xv_set(frame, XV_SHOW, FALSE, NULL);
    }
}

/*
 * Convert geometry flags into a gravity that can be used for
 * setting the window hints.
 */
static int frame_gravity_from_flags(int flags)
{
    switch (flags & (XNegative | YNegative)) {
      case 0:
	return NorthWestGravity;
      case XNegative:
	return NorthEastGravity;
      case YNegative:
	return SouthWestGravity;
      default:
	return SouthEastGravity;
    }
}

int dra_frame_debug = -1;

/*
 * Note: When the size of this struct is changed, please check
 * function frame_update_compose_led() to see if any size related
 * operations will still work.
 */
typedef struct {
    unsigned long	flags;
    unsigned long	state;
} Frame_win_state;

#define WSSemanticState		(1L<<0)
#define WSSemanticCompose	(1L<<0)

static void frame_update_compose_led(Frame_class_info *frame, int state)
{
	Xv_Drawable_info *info;
	Frame_win_state win_state;

	DRAWABLE_INFO_MACRO(FRAME_PUBLIC(frame), info);

	if ((status_get(frame, compose_led) != state) &&
			/* Check to see if the wm supports the compose LED protocol. */
			xv_get(xv_screen(info), SCREEN_SUN_WINDOW_STATE)) {

		win_state.flags = WSSemanticState;
		if (state)
			/* Turn compose key on. */
			win_state.state = WSSemanticCompose;
		else
			/* Turn compose key off. */
			win_state.state = 0;

		status_set(frame, compose_led, state);

		XChangeProperty(xv_display(info), xv_xid(info),
				xv_get(xv_server(info), SERVER_ATOM, "_SUN_WINDOW_STATE"),
				XA_INTEGER, 32, PropModeReplace, (unsigned char *)&win_state,
				/*
				 * We want to specify how many 32 bit elements.
				 * sizeof returns # of bytes, so we have
				 * to divide by bytes per long 
				 */
				(unsigned)(sizeof(Frame_win_state) / sizeof(unsigned long)));
		XFlush(xv_display(info));
	}
}

static int frame_init(Xv_Window owner, Frame frame_public, Attr_avlist avlist,
								int *dummy)
{
	Xv_frame_class *frame_object = (Xv_frame_class *) frame_public;
	Frame_class_info *frame;
	int is_subframe = owner && xv_get(owner, XV_IS_SUBTYPE_OF,
			FRAME_CLASS);
	short userHeight = False, userWidth = False;
	Attr_avlist attrs;
	Xv_Drawable_info *info;
	Atom property_array[3];
	XTextProperty WMMachineName;
	char hostname[MAXHOSTNAMELEN + 1];
	Window xid;

	DRAWABLE_INFO_MACRO(frame_public, info);

	frame = xv_alloc(Frame_class_info);

	/* link to object */
	frame_object->private_data = (Xv_opaque) frame;
	frame->public_self = frame_public;

	if (dra_frame_debug < 0) {
		char *envxvdeb = getenv("DRA_FRAME_DEBUG");

		if (envxvdeb && *envxvdeb) dra_frame_debug = atoi(envxvdeb);
		else dra_frame_debug = 0;
	}

	if (dra_frame_debug > 0) {
		fprintf(stderr, "%s:%d: frame_init(%lx)\n",
				__FILE__, __LINE__, frame_public);
	}

	/* Tell window manager to leave our input focus alone */
	frame->wmhints.input = FALSE;
	frame->wmhints.flags |= InputHint;
	frame->normal_hints.flags = 0;

	property_array[0] = (Atom) xv_get(xv_server(info), SERVER_WM_TAKE_FOCUS);
	property_array[1] = (Atom) xv_get(xv_server(info), SERVER_WM_DELETE_WINDOW);
	property_array[2] = (Atom) xv_get(xv_server(info), SERVER_WM_SAVE_YOURSELF);

	win_change_property(frame_public, (long)SERVER_WM_PROTOCOLS, XA_ATOM, 32,
			(unsigned char *)property_array, 3);

	/* Set WM_CLIENT_MACHINE property */
	WMMachineName.value = (unsigned char *)hostname;
	WMMachineName.encoding = XA_STRING;
	WMMachineName.format = 8;
	WMMachineName.nitems = (int)xv_get_hostname(hostname, MAXHOSTNAMELEN + 1);
	xid = xv_xid(info);
	XSetWMClientMachine(xv_display(info), xid, &WMMachineName);

	XChangeProperty(xv_display(info), xid,
					xv_get(xv_server(info), SERVER_ATOM, "WM_CLIENT_LEADER"),
					XA_WINDOW, 32, PropModeReplace, (unsigned char *)&xid, 1);

	/* Set WM_CLASS property */
	win_set_wm_class(frame_public);

	/* initialize the footer fields */
	status_set(frame, show_footer, FALSE);
	frame->footer = (Xv_Window) NULL;
	frame->ginfo = (Graphics_info *) NULL;
	frame->default_icon = (Icon) NULL;

#ifdef OW_I18N
	frame->left_footer.pswcs.value = (wchar_t *) NULL;
	frame->right_footer.pswcs.value = (wchar_t *) NULL;
#else
	frame->left_footer = (char *)NULL;
	frame->right_footer = (char *)NULL;
#endif

#ifdef OW_I18N
	/* initialize the IMstatus fields */
	status_set(frame, show_imstatus, FALSE);
	status_set(frame, inactive_imstatus, FALSE);
	frame->imstatus = (Xv_Window) NULL;
	frame->left_IMstatus.pswcs.value = (wchar_t *) NULL;
	frame->right_IMstatus.pswcs.value = (wchar_t *) NULL;
#endif

	/* set default frame flags */
	if (is_subframe) {
		Xv_Drawable_info *owner_info;

		DRAWABLE_INFO_MACRO(owner, owner_info);
		frame->wmhints.window_group = xv_xid(owner_info);
		frame->normal_hints.flags = PPosition | PSize;
	}
	else {
		/* Must parse size and position defaults here (instead of in
		 * fm_cmdline.c) to get the sizing hints set correctly.
		 */
		frame->geometry_flags = 0;
		frame->user_rect.r_left = 0;
		frame->user_rect.r_top = 0;
		frame->user_rect.r_width = 1;
		frame->user_rect.r_height = 1;
		frame->wmhints.window_group = xid;

		/* created another frame */
		frame_notify_count++;

		frame_set_cmdline_options(frame_public, TRUE);

		/* Adjust normal_hints to be in sync with geometry_flags */
		if (frame->geometry_flags & (XValue | YValue))
			frame->normal_hints.flags |= USPosition;
		if (frame->geometry_flags & (WidthValue | HeightValue))
			frame->normal_hints.flags |= USSize;

		/* set the bit gravity of the hints */
		if (frame->normal_hints.flags & USPosition) {
			frame->normal_hints.win_gravity =
					frame_gravity_from_flags(frame->geometry_flags);
			frame->normal_hints.flags |= PWinGravity;
		}
	}

	frame->wmhints.flags |= WindowGroupHint;
	frame->default_done_proc = frame_default_done_func;

	/*
	 * OPEN LOOK change: no cofirmer by default
	 */
	status_set(frame, no_confirm, (int)TRUE);
    status_set(frame, show_label, TRUE);

	/* Wmgr default to have resize corner for base and cmd frame */
	status_set(frame, show_resize_corner, TRUE);

	/*
	 * Iconic state and name stripe have to be determined before any size
	 * parameters are interpreted, so the attribute list is mashed and
	 * explicitly interrogated for them here.
	 */
	for (attrs = avlist; *attrs; attrs = attr_next(attrs)) {
		switch ((int)*attrs) {
			case FRAME_FOOTER_PROC:
				frame->footer_proc = (frame_footer_proc_t)attrs[1];
				status_set(frame, show_footer, TRUE);
				ATTR_CONSUME(*attrs);
				break;
			case FRAME_CLOSED:
				status_set(frame, iconic, (int)attrs[1]);
				status_set(frame, initial_state, (int)attrs[1]);
				frame->wmhints.initial_state = ((int)attrs[1] ?
						IconicState : NormalState);
				frame->wmhints.flags |= StateHint;
				ATTR_CONSUME(*attrs);
				break;
			case XV_X:
			case XV_Y:
			case XV_RECT:
				frame->normal_hints.flags |= PPosition;
				break;
			case WIN_ROWS:
			case XV_HEIGHT:
				userHeight = True;
				break;
			case WIN_COLUMNS:
			case XV_WIDTH:
				userWidth = True;
				break;
			default:
				break;
		}
	}

	/*
	 * Initialize the map cache which we use later for determining the
	 * state of the window: mapped, withdrawn, iconic.
	 */
	window_set_map_cache(frame_public, False);

	xv_set(frame_public,
			WIN_INHERIT_COLORS, (long)TRUE,
			WIN_LAYOUT_PROC, frame_layout,
			WIN_NOTIFY_SAFE_EVENT_PROC, frame_input,
			WIN_NOTIFY_IMMEDIATE_EVENT_PROC, frame_input,
			/*
			 * clear rect info since we are the ones setting (rows, columns).
			 * this will let us check later (frame_set_avlist()) if the
			 * client has set the position or size.
			 */
			WIN_RECT_INFO, NULL,
			WIN_CONSUME_EVENTS,
				(long)WIN_MAP_NOTIFY,
				(long)WIN_UNMAP_NOTIFY,
				NULL,
			WIN_IGNORE_EVENT,
				(long)WIN_REPAINT,
				NULL,
			NULL);
	/*
	 * Only if the user has not set the width (columns) or height (rows)
	 * do we set the default rows and columns.  
	 * REMIND: We really need to see about moving this to END_CREATE.
	 */
	/* If both width and height is not set, set it to default in one
	 * xv_set call to avoid making two seperate ConfigureRequests to
	 * the server.
	 */
	if ((!userWidth) && (!userHeight))
		xv_set(frame_public,
				WIN_COLUMNS, 80,
				WIN_ROWS, 34,
				NULL);
	else if (!userHeight)
		xv_set(frame_public,
				WIN_ROWS, 34,
				NULL);
	else if (!userWidth)
		xv_set(frame_public,
				WIN_COLUMNS, 80,
				NULL);

	/* Set cached version of rect */
	(void)win_getsize(frame_public, &frame->rectcache);

	if (!is_subframe) {
		int frame_count;

		frame_count = (int)xv_get(xv_server(info), XV_KEY_DATA, FRAME_COUNT);
		xv_set(xv_server(info),
				XV_KEY_DATA, FRAME_COUNT, ++frame_count,
				NULL);
	}

	/* Initialise default foreground and background */
	frame->fg.red = frame->fg.green = frame->fg.blue = (unsigned short)0;
	frame->bg.red = frame->bg.green = frame->bg.blue = (unsigned short)~0;

	XSetWMHints(xv_display(info), xid, &(frame->wmhints));

	/* Use old XSetNormalHints function for non-ICCCM wm's */
	if (!defaults_get_boolean("xview.icccmcompliant",
					"XView.ICCCMCompliant", TRUE))
		XSetNormalHints(xv_display(info), xid, &frame->normal_hints);
	else {
		frame->normal_hints.flags |= PSize;
		frame->normal_hints.base_width = frame->rectcache.r_width;
		frame->normal_hints.width = frame->rectcache.r_width;
		frame->normal_hints.base_height = frame->rectcache.r_height;
		frame->normal_hints.height = frame->rectcache.r_height;
		frame->normal_hints.x = frame->rectcache.r_left;
		frame->normal_hints.y = frame->rectcache.r_top;
		XSetWMNormalHints(xv_display(info), xid, &frame->normal_hints);
	}

	/* Create the Location Cursor (Focus) Window */
	frame->focus_window = xv_create(xv_root(info), WINDOW,
			WIN_BORDER, (long)FALSE,
			WIN_EVENT_PROC, frame_focus_win_event_proc,
			WIN_FRONT,
			WIN_PARENT, frame_public,
			WIN_RETAINED, (long)FALSE,	/* insure a repaint call on each mapping */
			WIN_SAVE_UNDER, (long)TRUE,
			XV_SHOW, (long)FALSE,
			XV_WIDTH, (long)FRAME_FOCUS_UP_WIDTH,
			XV_HEIGHT, (long)FRAME_FOCUS_UP_HEIGHT,
			XV_KEY_DATA, FRAME_FOCUS_DIRECTION, FRAME_FOCUS_UP,
			XV_KEY_DATA, FRAME_FOCUS_UP_IMAGE,
						xv_create(xv_screen(info), SERVER_IMAGE,
								XV_WIDTH, 16L,
								XV_HEIGHT, 16L,
								SERVER_IMAGE_BITS, focus_up_bits,
								SERVER_IMAGE_DEPTH, 1L,
								NULL),
			XV_KEY_DATA, FRAME_FOCUS_RIGHT_IMAGE,
						xv_create(xv_screen(info), SERVER_IMAGE,
								XV_WIDTH, 16L,
								XV_HEIGHT, 16L,
								SERVER_IMAGE_BITS, focus_right_bits,
								SERVER_IMAGE_DEPTH, 1L,
								NULL),
			NULL);

	frame_update_compose_led(frame, False);

	return XV_OK;
}

static void free_help(Xv_object server, Attr_attribute key, Xv_opaque data){
	if (key == XV_HELP && data) xv_free(data);
}

extern Graphics_info *xv_init_olgx(Xv_Window, int *, Xv_Font);

static Xv_window frame_create_footer(Frame_class_info *frame)
{
	Frame frame_public = FRAME_PUBLIC(frame);
	Frame_rescale_state scale;
	Xv_window footer;
	char *hlpbuf, *hlp;
	screen_ui_style_t ui_style;
	int three_d;

	if (frame->footer_proc) {
		footer = XV_NULL;

		/* the application has to handle EVERYTHING - especially the creation */
		(frame->footer_proc)(frame_public, &footer, FRAME_FOOTER_CREATE, NULL);
	}
	else {

		scale = xv_get(xv_get(frame_public, XV_FONT), FONT_SCALE);

		hlp = (char *)xv_get(frame_public, XV_HELP_DATA);
		if (hlp && *hlp) {
			hlpbuf = malloc(strlen(hlp) + 10);
			sprintf(hlpbuf, "%s_footer", hlp);
		}
		else {
			hlpbuf = xv_strsave("xview:frame_footer");
		}

		footer = xv_create(frame_public, WINDOW,
				WIN_NOTIFY_SAFE_EVENT_PROC, frame_footer_input,
				WIN_NOTIFY_IMMEDIATE_EVENT_PROC, frame_footer_input,
				WIN_BIT_GRAVITY, (long)ForgetGravity,
				/* Originally (XView 3.2), the footer didn't select any events,
				 * so, mouse events 'fell through' to the window decoration.
				 * When we implemented quick duplicate on frame footers, we
				 * had to select for mouse events.
				 * However, all "non-quick" events are still supposed to
				 * to fall through to the WM. We use xv_ol_default_background
				 * for this purpose, see fmcmd_set.c
				 */
				WIN_CONSUME_EVENTS,
					(long)WIN_MOUSE_BUTTONS,
					(long)LOC_DRAG,
					(long)ACTION_HELP,
					NULL,
				WIN_INHERIT_COLORS, (long)TRUE,
				XV_HEIGHT, (long)frame_footer_height(scale),
				XV_KEY_DATA, FRAME_FOOTER_WINDOW, TRUE,
				XV_HELP_DATA, hlpbuf,
				XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help,
				NULL);
	}

	ui_style = (screen_ui_style_t)xv_get(XV_SCREEN_FROM_WINDOW(footer),
											SCREEN_UI_STYLE);
	three_d = (ui_style == SCREEN_UIS_3D_COLOR);
	frame->ginfo = xv_init_olgx(footer, &three_d, xv_get(footer, XV_FONT));

	return footer;
}

#ifdef OW_I18N
Pkg_private Xv_window frame_create_IMstatus(Frame_class_info *frame)
{
    Frame frame_public = FRAME_PUBLIC(frame);
    Frame_rescale_state scale;
    Xv_window IMstatus;
    int three_d;

    scale = xv_get(xv_get(frame_public, XV_FONT), FONT_SCALE);

    IMstatus = (Xv_window)xv_create(frame_public, WINDOW,
        WIN_NOTIFY_SAFE_EVENT_PROC, frame_IMstatus_input,

        WIN_NOTIFY_IMMEDIATE_EVENT_PROC, frame_IMstatus_input,

        WIN_BIT_GRAVITY, ForgetGravity,
        XV_HEIGHT, frame_IMstatus_height(scale),
        XV_KEY_DATA, FRAME_IMSTATUS_WINDOW, TRUE,
        NULL);

    three_d = defaults_get_boolean("OpenWindows.3DLook.Color",
                                   "OpenWindows.3DLook.Color", TRUE);
    frame->ginfo = xv_init_olgx(IMstatus, &three_d, xv_get(IMstatus,
XV_FONT));

    return(IMstatus);
}
#endif

static int frame_fit_direction(Frame_class_info *frame, Window_attribute direction)
{
    Frame           frame_public = FRAME_PUBLIC(frame);
    register Xv_Window sw;
    Rect            rect, rbound;
    register short *value = (direction == WIN_DESIRED_WIDTH) ?
    &rect.r_width : &rect.r_height;

    rbound = rect_null;
    FRAME_EACH_SHOWN_SUBWINDOW(frame, sw)
	(void)win_get_outer_rect(sw, &rect);
    *value += FRAME_BORDER_WIDTH;
    rbound = rect_bounding(&rbound, &rect);
    FRAME_END_EACH
	if (direction == WIN_DESIRED_WIDTH) {
	if (!rbound.r_width)
	    (void) win_getrect(frame_public, &rbound);
	else
        {
            if (rbound.r_left)
                rbound.r_width += rbound.r_left;
            rbound.r_width += FRAME_BORDER_WIDTH;
        }
	return rbound.r_width;
    } else {
	if (!rbound.r_height)
	    (void) win_getrect(frame_public, &rbound);
	else
        {
            if (rbound.r_top)
                rbound.r_height += rbound.r_top;
            rbound.r_height += FRAME_BORDER_WIDTH;
        }
	return rbound.r_height;
    }
}

Xv_private int window_get_map_cache(Xv_Window window);
Xv_private int xv_deaf(Xv_window	parent, Bool on);

/* ACC_XVIEW */
#define		FRAME_MENU_BLOCK	10
/* ACC_XVIEW */
static void frame_adjust_normal_hints(Frame_class_info *frame, int adjustment, Bool *update_hints);

static void frame_set_menu_acc(Frame frame_public, int keycode, unsigned int	state, KeySym keysym, CHAR *keystr, frame_accel_notify_func notify_proc, Xv_opaque data);
static void frame_change_state(Frame_class_info *frame, int to_iconic);
static void frame_set_icon(Frame_class_info *frame, Icon icon, int *set_icon_rect, Rect icon_rect);

/* ACC_XVIEW */
static KeySym xkctks(Display *display, int kc, int idx)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#pragma GCC diagnostic ignored "-Wtraditional-conversion"
	return XKeycodeToKeysym(display, kc, idx);
#pragma GCC diagnostic pop
}

/* ACC_XVIEW */
/*
 * Search for an existing menu accelerator on the frame
 * that matches the passed state and keycode/keysym.
 *
 * This function is used to look up an accelerator to see if
 * it exists on the frame to determine:
 *	- if a key sequence should be considered an accelerator
 *	  when detected on the frame
 *	- whether an accelerator that is to be added to the frame
 *	  is a duplicate of an already existing one. For example,
 *	  "Meta+plus" is a duplicate of "Meta+Shift+equals", at 
 *	  least on a Sun keyboard
 *
 * This function is passed:
 *	keycode
 *	keysym
 *	state of modifier keys
 *
 * The accelerators stored on the frame also contain the above 
 * information. In doing the search, we need to make sure that
 * the following things match i.e. are the same:
 *	"Meta+A", "Meta+Shift+A", "Meta+Shift+a"
 *
 * This is done by:
 *	1. Get unmodified keysym using 
 *		XKeycodeToKeysym(..., keycode, 0)
 *	2. Get shifted keysym using
 *		XKeycodeToKeysym(..., keycode, 1)
 *	   We will perform a search of the above keysyms in the
 *	   frame accelerator list
 *	NOTE: How should ModeSwitch be handled here?
 *	3. If shifted keysym exists, make sure we ignore ShiftMask
 *	   in our searches later on
 *	4. Look at 'state' mask
 *	   If (state | ShiftMask) 
 *		we perform search on shifted keysym only (found in step 2)
 *	   Else 
 *		perform search on unmodified keysym only (found in step 1)
 *	   We selectively perform the search on only one of the keysyms 
 *	   by NULLifying the other with NoSymbol.
 *
 * From the keycode passed in, we can determine what the modified/shifted 
 * keysyms are, and these will be the keysyms used in the search.
 * 
 * However, if the keysym parameter is != NoSymbol, it will be the 
 * (only) keysym used in the search.
 */
static Frame_menu_accelerator *frame_find_menu_acc(Frame frame_public,
			int keycode, unsigned int state, KeySym keysym, int remove)
{
    int				i, ksym_count, shift_ksym_exist, 
				ignore_shift;
    KeySym			keysymlist[2];
    Frame_menu_accelerator 	*menu_accel, *prev;
    Display			*dpy =
			XV_DISPLAY_FROM_WINDOW(frame_public);
    Frame_class_info		*frame = 
			FRAME_CLASS_PRIVATE(frame_public);

    /*
     * Get keysyms for unmodified and Shift modified keycode
     * This has to be done even if the 'keysym' parameter
     * is not NoSymbol because we need to determine if 
     * we want to ignore the Shift mask.
     */

    /*
     * Get keysym for unmodified keycode
     */
    keysymlist[0] = xkctks(dpy, keycode, 0);

    /*
     * Return NULL if no keysym exist for unmodified keycode
     */
    if ((keysymlist[0] == NoSymbol) && (keysym == NoSymbol))  {
        return (Frame_menu_accelerator *) NULL;
    }

    /*
     * Get keysym for shifted keycode
     */
    keysymlist[1] = xkctks(dpy, keycode, 1);

    /*
     * This keycode has a valid entry for Shift/Caps Lock
     * if the unmodified entry and the shifted one are not
     * the same, and the shifted keysym is something meaningful
     */
    shift_ksym_exist = ((keysymlist[0] != keysymlist[1]) && 
				(keysymlist[1] != NoSymbol));

    /*
     * We want to ignore the Shift mask i.e. ignore it if the
     * keysym for it exists for this keycode
     */
    ignore_shift = shift_ksym_exist;

    if (shift_ksym_exist)  {
        /*
         * We want to get a match for the right keysym so we
         * NULLify the undesired one.
         *
         * For example, for "Meta+Shift+equals", we want to search
         * using the keysym "plus", and not "equals". So, in this
         * case, we NULLify the "equals" entry with NoSymbol.
         * We check the state mask (for ShiftMask/LockMask) to
         * determine which keysym to NULLify.
	 *
	 * We check if lockmask is meaningful, by using the
	 * isalpha() macro. This macro only takes 7 bit chars,
	 * so we check for that using isascii(). isalpha() does 
	 * accept 8 bit characters in non C locales, but we don't 
	 * need to go into that here.
         */
        if (  isascii((int)keysymlist[0]) && 
		isalpha((int)keysymlist[0]) )  {
            /*
             * For alpha characters we look for either
             * Shift or Lock mask
             */
            if ((state & ShiftMask) || (state & LockMask))  {
                keysymlist[0] = NoSymbol;
            }
            else  {
                keysymlist[1] = NoSymbol;
            }
        }
        else  {
            /*
             * For non alpha characters, we look only for Shift,
             * since LockMask does not apply to those characters
             */
            if (state & ShiftMask)  {
                keysymlist[0] = NoSymbol;
            }
            else  {
                keysymlist[1] = NoSymbol;
            }
        }
    }

    /*
     * Check if a valid keysym was passed to search for.
     * If yes, use the passed keysym only
     * If no, use the 2 keysyms: unmodified, shifted 
     */
    if (keysym != NoSymbol)  {
        keysymlist[0] = keysym;
        ksym_count = 1;
    }
    else  {
        ksym_count = 2;
    }

    /*
     * Search for matching keysym
     */
    for (i=0; i < ksym_count; ++i)  {

        for (prev = menu_accel = frame->menu_accelerators; 
                menu_accel; 
		prev = menu_accel, menu_accel = menu_accel->next) {

	    /*
	     * Skip if keysym is NoSymbol
	     */
            if (keysymlist[i] == NoSymbol)  {
                continue;
            }

            if (menu_accel->keysym == keysymlist[i])  {
                unsigned int mod = menu_accel->modifiers;

		/*
		 * Keysyms match. Now we compare the state
		 * masks.
		 *	'state' is the state searched for
		 *	'mod' is the state of the current keysym
		 *		that matched
		 */

                if (ignore_shift)  {
                    if (state & ShiftMask)  {
                        mod |= ShiftMask;
                    }
                    else  {
                        mod &= ~ShiftMask;
                    }
                }

                /*
                 * We always ignore LockMask
                 */
                if (state & LockMask)  {
                    mod |= LockMask;
                }
                else  {
                    mod &= ~LockMask;
                }

                if (mod == state)  {
		    /*
		     * If remove flag set, unlink found node.
		     * It is up top the caller to free up any data.
		     */
		    if (remove)  {
			if (frame->menu_accelerators == menu_accel)  {
		            /*
		             * Accelerator was first on list, unlink it
		             */
			    frame->menu_accelerators = menu_accel->next;
			}
			else  {
			    prev->next = menu_accel->next;
			}
		    }
                    return (menu_accel);
                }
            }
        }
    }

    return (Frame_menu_accelerator *) NULL;
}
/* ACC_XVIEW */

/*
 * use the window manager to set the position and trim the size of the base
 * frame.
 */
/* ARGSUSED */
static void frame_set_position(unsigned long parent, Frame_class_info *frame)
{
    Frame           frame_public = FRAME_PUBLIC(frame);
    Rect            rectnormal;
    /*
     * int		rect_info = (int) xv_get(frame_public,
     * WIN_RECT_INFO);
     */

    win_getrect(frame_public, &rectnormal);

    /* if the position is not set, then let the window manager set it */

    /*
     * csk 5/3/89: rect_info is never set to anything but 0.  Should it be
     * removed?  If anything, it should test rectnormal.r_* for set values...
     * This code needs to be reviewed by Mr. Ng!!
     *
     * if (!(rect_info & WIN_X_SET)) rectnormal.r_left = 0; if (!(rect_info &
     * WIN_Y_SET)) rectnormal.r_top = 0;
     */


    win_setrect(frame_public, &rectnormal);
    frame->rectcache = rectnormal;

    /*
     * Cached rect is self relative.
     */
    frame->rectcache.r_left = frame->rectcache.r_top = 0;
}

static const unsigned short    default_frame_icon_image[256] = {
#include        <images/default.icon>
};

mpr_static(default_frame_icon_mpr, ICON_DEFAULT_WIDTH, ICON_DEFAULT_HEIGHT, 1,
	   default_frame_icon_image);

static Xv_opaque frame_set_avlist(Frame frame_public,
										Attr_attribute *avlist)
{
	Attr_avlist attrs;
	Frame_class_info *frame = FRAME_PRIVATE(frame_public);
	int is_subframe, result = XV_OK, paint_footers = FALSE;
	Xv_object owner;
	static int set_icon_rect = FALSE;
	static Rect icon_rect;
	Bool update_hints = FALSE;
	Cms new_frame_cms = (Cms) NULL;
	unsigned long new_frame_fg = XV_NULL;
	int new_frame_fg_set = FALSE;
	Bool update_footer_color = FALSE;
	int add_decor = 0, delete_decor = 0;
	int need_resizing = FALSE;
	int resize_width;
	xv_frame_layout_cb_t layout = NULL;
	void *layout_cldt = NULL;

#ifdef OW_I18N
	int paint_imstatus = FALSE;

	/*
	 * pswcs is used to take advantage of the _xv_pswcs functions
	 * that malloc/convert mb/wcs strings.
	 */
	_xv_pswcs_t pswcs = { 0, NULL, NULL };
#endif

	for (attrs = avlist; *attrs; attrs = attr_next(attrs)) switch (attrs[0]) {
		case FRAME_CLOSED:
		case FRAME_CLOSED_RECT:
			owner = xv_get(frame_public, XV_OWNER);
			is_subframe = (int)xv_get(owner, XV_IS_SUBTYPE_OF, FRAME_CLASS);
			switch (attrs[0]) {
				case FRAME_CLOSED:
					frame_change_state(frame, (int)attrs[1]);
					break;
					/* SunView1 compatibility only */
				case FRAME_CLOSED_RECT:
					/* can't set closed rect if subframe */
					if (!is_subframe) {
						if (frame->icon) {
							Xv_Drawable_info *info;

							DRAWABLE_INFO_MACRO(frame_public, info);

							(void)win_setrect(frame->icon, (Rect *) attrs[1]);
							frame->wmhints.flags |= IconPositionHint;
							frame->wmhints.icon_x = ((Rect *) attrs[1])->r_left;
							frame->wmhints.icon_y = ((Rect *) attrs[1])->r_top;
							XSetWMHints(xv_display(info), xv_xid(info),
									&(frame->wmhints));
						}
						else {
							set_icon_rect = TRUE;
							icon_rect = *(Rect *) attrs[1];
						}
					}
					break;
			}
			break;

			/* SunView1 compatibility only */
		case FRAME_CURRENT_RECT:
			(void)xv_set((frame_is_iconic(frame)) ? frame->icon :
					frame_public, WIN_RECT, (Rect *) attrs[1], NULL);
			break;

		case FRAME_DEFAULT_DONE_PROC:
			frame->default_done_proc = (frame_done_proc_t) attrs[1];
			if (!frame->default_done_proc)
				frame->default_done_proc = frame_default_done_func;
			break;

		case FRAME_DONE_PROC:
			frame->done_proc = (frame_done_proc_t) attrs[1];
			break;

		case FRAME_NO_CONFIRM:
			status_set(frame, no_confirm, (int)attrs[1]);
			break;

		case FRAME_INHERIT_COLORS:
			xv_set(frame_public, WIN_INHERIT_COLORS, TRUE, NULL);
			break;

		case FRAME_FOCUS_DIRECTION:
			if (frame->focus_window) {
				xv_set(frame->focus_window,
						XV_KEY_DATA, FRAME_FOCUS_DIRECTION, attrs[1],
						XV_WIDTH, attrs[1] == FRAME_FOCUS_UP ?
						FRAME_FOCUS_UP_WIDTH : FRAME_FOCUS_RIGHT_WIDTH,
						XV_HEIGHT, attrs[1] == FRAME_FOCUS_UP ?
						FRAME_FOCUS_UP_HEIGHT : FRAME_FOCUS_RIGHT_HEIGHT, NULL);
			}
			break;

		case FRAME_FOREGROUND_COLOR:
			{
				/* 
				 * Create-only attributes must be processed here since it can be 
				 * set through the cmdline.
				 */
				if (!status_get(frame, created)) {
					frame->fg.red = (unsigned short)(((Xv_Singlecolor *)
									attrs[1])->red);
					frame->fg.green = (unsigned short)(((Xv_Singlecolor *)
									attrs[1])->green);
					frame->fg.blue = (unsigned short)(((Xv_Singlecolor *)
									attrs[1])->blue);
					status_set(frame, frame_color, TRUE);
				}
				break;
			}

		case FRAME_BACKGROUND_COLOR:
			{
				if (!status_get(frame, created)) {
					frame->bg.red = (unsigned short)(((Xv_Singlecolor *)
									attrs[1])->red);
					frame->bg.green = (unsigned short)(((Xv_Singlecolor *)
									attrs[1])->green);
					frame->bg.blue = (unsigned short)(((Xv_Singlecolor *)
									attrs[1])->blue);
					status_set(frame, frame_color, TRUE);
				}
				break;
			}

		case FRAME_SUBWINDOWS_ADJUSTABLE:
			status_set(frame, bndrymgr, (int)attrs[1]);
			break;

		case FRAME_ICON:
			frame_set_icon(frame, (Icon) attrs[1], &set_icon_rect, icon_rect);
			break;

		case FRAME_BUSY:
			status_set(frame, busy, FALSE);
			if ((int)attrs[1]) {
				if (xv_deaf(frame_public, TRUE) != XV_OK) {
					xv_error(frame_public,
							ERROR_STRING,
							XV_MSG("Attempt to make frame deaf failed"), NULL);
					result = XV_ERROR;
					break;
				}
				frame_display_busy(frame, WMWindowIsBusy);
				status_set(frame, busy, TRUE);
			}
			else {
				if (xv_deaf(frame_public, FALSE) != XV_OK) {
					xv_error(frame_public,
							ERROR_STRING,
							XV_MSG("Attempt to make frame undeaf failed"), NULL);
					result = XV_ERROR;
					break;
				}
				frame_display_busy(frame, WMWindowNotBusy);
				status_set(frame, busy, FALSE);
			}

			break;

		case FRAME_SCALE_STATE:
			/*
			 * set the local rescale state bit, then tell the WM the current
			 * state, and then set the scale of our subwindows
			 */
			wmgr_set_rescale_state(frame_public, (int)attrs[1]);
			frame_rescale_subwindows(frame_public, (int)attrs[1]);
			break;

		case FRAME_NEXT_PANE:
			{
				Xv_Window pane = frame->focus_subwindow;

				do {
					pane = xv_get(pane, XV_KEY_DATA, FRAME_NEXT_CHILD);
					if (!pane)
						pane = frame->first_subwindow;
					if (pane == frame->focus_subwindow)
						break;
				} while (xv_set(pane, WIN_SET_FOCUS, NULL) != XV_OK);
				xv_set(pane, XV_FOCUS_ELEMENT, 0,	/* first element */
						NULL);
				break;
			}

		case FRAME_PREVIOUS_ELEMENT:
		case FRAME_PREVIOUS_PANE:
			{
				Xv_Window pane = xv_get(frame->focus_subwindow,
						XV_KEY_DATA, FRAME_PREVIOUS_CHILD);

				if (!pane)
					pane = frame_last_child(frame->first_subwindow);
				if (xv_get(pane, XV_IS_SUBTYPE_OF, OPENWIN)) {
					int is_canvas =
							(int)xv_get(pane, XV_IS_SUBTYPE_OF, CANVAS),
							nbr_views = (int)xv_get(pane, OPENWIN_NVIEWS);
					Xv_window last_view = xv_get(pane, OPENWIN_NTH_VIEW, nbr_views - 1);
					Xv_Window sb;

					if (attrs[0] == FRAME_PREVIOUS_ELEMENT && is_canvas) {
						/* FRAME_PREVIOUS_ELEMENT: set focus to
						 *      horizontal scrollbar,
						 *      vertical scrollbar, or
						 *      Openwin
						 */
						sb = xv_get(pane, OPENWIN_HORIZONTAL_SCROLLBAR, last_view);
						if (!sb)
							sb = xv_get(pane, OPENWIN_VERTICAL_SCROLLBAR, last_view);
						if (sb)
							xv_set(sb, WIN_SET_FOCUS, NULL);
						else {
							/* No scrollbars: set focus to Canvas */
							if (pane != frame->focus_subwindow)
								xv_set(pane, WIN_SET_FOCUS, NULL);
							xv_set(pane, XV_FOCUS_ELEMENT, -1,	/* last element */
									NULL);
						}
					}
					else {
						Xv_Window pw;

						/* FRAME_PREVIOUS_PANE, or FRAME_PREVIOUS_ELEMENT on a
						 * non-canvas (e.g., textsw): set focus to last paint or
						 * view window
						 */
						if (is_canvas)
							pw = xv_get(last_view, CANVAS_VIEW_PAINT_WINDOW);
						else
							pw = last_view;
						xv_set(pw, WIN_SET_FOCUS, NULL);
						xv_set(pane, XV_FOCUS_ELEMENT, -1,	/* last element */
								NULL);
					}
				}
				else {
					do {
						if (pane == frame->focus_subwindow ||
								xv_set(pane, WIN_SET_FOCUS, NULL) == XV_OK)
							break;
						pane = xv_get(pane, XV_KEY_DATA, FRAME_PREVIOUS_CHILD);
						if (!pane)
							pane = frame_last_child(frame->first_subwindow);
					} while (TRUE);
					xv_set(pane, XV_FOCUS_ELEMENT, -1,	/* last element */
							NULL);
				}
				break;
			}
		case FRAME_ACCELERATOR:
			{
				Frame_accelerator *new_accel, *accel;
				unsigned int mask;

				mask = (unsigned int)xv_get(frame_public, WIN_CONSUME_X_EVENT_MASK);
				if (!(mask & KeyPressMask) || !(mask & FocusChangeMask)) {
					xv_set(frame_public,
							WIN_CONSUME_X_EVENT_MASK,
							KeyPressMask | FocusChangeMask, NULL);
				}

				new_accel = xv_alloc(Frame_accelerator);
				new_accel->code = (short)attrs[1];
				new_accel->notify_proc = (frame_accel_notify_func) attrs[2];
				new_accel->data = (Xv_opaque) attrs[3];
				if (frame->accelerators) {
					for (accel = frame->accelerators; accel->next; accel = accel->next) {
						if (accel->code == new_accel->code) {
							/* Replace accelerator info */
							accel->notify_proc = (frame_accel_notify_func) attrs[2];
							accel->data = (Xv_opaque) attrs[3];
							xv_free(new_accel);
							break;
						}
					}
					if (!accel->next) {
						/* Add new accelerator */
						accel->next = new_accel;
					}
				}
				else {
					/* Add (first) new accelerator */
					frame->accelerators = new_accel;
				}
				break;
			}
		case FRAME_X_ACCELERATOR:
			{
				Frame_accelerator *new_accel, *accel;
				unsigned int mask;

				mask = (unsigned int)xv_get(frame_public, WIN_CONSUME_X_EVENT_MASK);
				if (!(mask & KeyPressMask) || !(mask & FocusChangeMask)) {
					xv_set(frame_public,
							WIN_CONSUME_X_EVENT_MASK,
							KeyPressMask | FocusChangeMask, NULL);
				}

				new_accel = xv_alloc(Frame_accelerator);
				new_accel->keysym = (KeySym) attrs[1];
				new_accel->notify_proc = (frame_accel_notify_func) attrs[2];
				new_accel->data = (Xv_opaque) attrs[3];
				if (frame->accelerators) {
					for (accel = frame->accelerators; accel->next; accel = accel->next) {
						if (accel->keysym == new_accel->keysym) {
							/* Replace accelerator info */
							accel->notify_proc = (frame_accel_notify_func) attrs[2];
							accel->data = (Xv_opaque) attrs[3];
							xv_free(new_accel);
							break;
						}
					}
					if (!accel->next) {
						/* Add new accelerator */
						accel->next = new_accel;
					}
				}
				else {
					/* Add (first) new accelerator */
					frame->accelerators = new_accel;
				}
				break;
			}

			/* ACC_XVIEW */

#ifdef OW_I18N
		case FRAME_MENU_ACCELERATOR:
		case FRAME_MENU_ACCELERATOR_WCS:
#else
		case FRAME_MENU_ACCELERATOR:
#endif /* OW_I18N */

			{

#ifdef OW_I18N
				Attr_attribute which_attr = attrs[0];
#endif /* OW_I18N */

				Xv_server server_public;
				Frame_menu_accelerator *accel;
				CHAR *keystr = (CHAR *) NULL;
				frame_accel_notify_func notify_proc;
				unsigned int modifiers = 0;
				short keycode;
				KeySym keysym;
				Xv_opaque data;

#ifdef OW_I18N
				/*
				 * Use different macro to duplicate/convert strings depending
				 * on whether mb/wcs attribute was used.
				 */
				if (which_attr == FRAME_MENU_ACCELERATOR) {
					_xv_pswcs_mbsdup(&pswcs, (char *)attrs[1]);
				}
				else {
					_xv_pswcs_wcsdup(&pswcs, (wchar_t *)attrs[1]);
				}

				keystr = pswcs.value;
#else
				keystr = (CHAR *) attrs[1];
#endif /* OW_I18N */

				notify_proc = (frame_accel_notify_func) attrs[2];
				data = (Xv_opaque) attrs[3];

				server_public = XV_SERVER_FROM_WINDOW(frame_public);

				/*
				 * Parse accelerator string 
				 */
				result = server_parse_keystr(server_public, keystr,
						&keysym, &keycode, &modifiers, 0, NULL);


				if (result != XV_OK) {
					result = XV_ERROR;
					break;
				}

				/*
				 * Search for existing menu accelerator with matching
				 * modifier and keycode/keysym
				 */
				accel = frame_find_menu_acc(frame_public, keycode,
						modifiers, keysym, FALSE);

				/*
				 * Break if accelerator found
				 */
				if (accel) {
					result = XV_ERROR;
					break;
				}

				/*
				 * Duplicate key string before calling frame_set_menu_acc()
				 if ((char *)attrs[1])  {
				 keystr = xv_strsave((char *)attrs[1]);
				 }
				 */

				/*
				 * Add accelerator to list on frame
				 */
				frame_set_menu_acc(frame_public, keycode, modifiers, keysym,
						keystr, notify_proc, data);
				break;
			}

#ifdef OW_I18N
		case FRAME_MENU_REMOVE_ACCELERATOR:
		case FRAME_MENU_REMOVE_ACCELERATOR_WCS:
#else
		case FRAME_MENU_REMOVE_ACCELERATOR:
#endif /* OW_I18N */
			{

#ifdef OW_I18N
				Attr_attribute which_attr = attrs[0];
#endif /* OW_I18N */

				Xv_server server_public;
				Frame_menu_accelerator *accel;
				CHAR *keystr;
				unsigned int modifiers = 0;
				short keycode;
				KeySym keysym;

				if (!(CHAR *) attrs[1]) {
					result = XV_ERROR;
					break;
				}

#ifdef OW_I18N
				/*
				 * Use different macro to duplicate/convert strings depending
				 * on whether mb/wcs attribute was used.
				 */
				if (which_attr == FRAME_MENU_REMOVE_ACCELERATOR) {
					_xv_pswcs_mbsdup(&pswcs, (char *)attrs[1]);
				}
				else {
					_xv_pswcs_wcsdup(&pswcs, (wchar_t *)attrs[1]);
				}

				keystr = pswcs.value;
#else
				keystr = (CHAR *) attrs[1];
#endif /* OW_I18N */

				/*
				 * Get server object handle
				 */
				server_public = XV_SERVER_FROM_WINDOW(frame_public);

				/*
				 * Parse accelerator string 
				 */
				result = server_parse_keystr(server_public, keystr,
						&keysym, &keycode, &modifiers, 0, NULL);

				if (result != XV_OK) {
					result = XV_ERROR;
					break;
				}

				/*
				 * Search for existing menu accelerator with matching
				 * keycode/modifier or keysym
				 */
				accel = frame_find_menu_acc(frame_public, keycode,
						modifiers, keysym, TRUE);

				if (accel) {
					/*
					 * Remove/decrement entry in server accelerator map
					 */
					xv_set(server_public, SERVER_REMOVE_ACCELERATOR_MAP,
							accel->keysym, accel->modifiers, NULL);

					/*
					 * Free keystr and accelerator struct
					 * BUG ALERT: What about accel->data ??
					 */

#ifdef OW_I18N
					_xv_free_ps_string_attr_dup(&accel->keystr);
#else
					xv_free(accel->keystr);
#endif /* OW_I18N */

					xv_free(accel);
				}
				break;
			}

		case FRAME_MENU_X_ACCELERATOR:
			{
				Frame_menu_accelerator *accel;
				frame_accel_notify_func notify_proc;
				unsigned int modifiers = 0;
				short keycode;
				KeySym keysym;
				Xv_opaque data;

				/*
				 * Get parameters of xv_set
				 */
				keycode = (short)attrs[1];
				modifiers = (unsigned int)attrs[2];
				keysym = (KeySym) attrs[3];
				notify_proc = (frame_accel_notify_func) attrs[4];
				data = (Xv_opaque) attrs[5];

				/*
				 * Search for existing menu accelerator with matching
				 * keycode/modifier or keysym
				 */
				accel = frame_find_menu_acc(frame_public, keycode,
						modifiers, keysym, FALSE);

				/*
				 * Break if accelerator found
				 */
				if (accel) {
					result = XV_ERROR;
					break;
				}

				/*
				 * Add accelerator to list on frame
				 * Note: string field is NULL
				 */
				frame_set_menu_acc(frame_public, keycode, modifiers, keysym,
						(CHAR *) NULL, notify_proc, data);

				break;
			}
			/* ACC_XVIEW */

		case FRAME_SHOW_FOOTER:
			if (!frame->footer_proc) {
				/* if an application has provided a FRAME_FOOTER_PROC, we
				 * we force the show_footer attribute to TRUE
				 */
				int show_footer;

				attrs[0] = (Frame_attribute) ATTR_NOP(attrs[0]);
				show_footer = (int)attrs[1];

				if (status_get(frame, show_footer) != show_footer) {
					if (status_get(frame, created)) {
						if (show_footer) {
							if (frame->footer == XV_NULL) {
								frame->footer = frame_create_footer(frame);
							}
							else {
								xv_set(frame->footer, XV_SHOW, TRUE, NULL);
							}
							frame_adjust_normal_hints(frame,
									(int)xv_get(frame->footer, XV_HEIGHT),
									&update_hints);
						}
						else {
							if (frame->footer != XV_NULL) {
								xv_set(frame->footer, XV_SHOW, FALSE, NULL);
								frame_adjust_normal_hints(frame,
										-(int)xv_get(frame->footer,
												XV_HEIGHT), &update_hints);
							}
						}
					}
					status_set(frame, show_footer, show_footer);
				}
			}
			ATTR_CONSUME(attrs[0]);
			break;

		case FRAME_LEFT_FOOTER:

#ifdef OW_I18N
			*attrs = (Frame_attribute) ATTR_NOP(*attrs);
			_xv_set_mbs_attr_dup(&frame->left_footer, (char *)attrs[1]);
			if (status_get(frame, show_footer))
				paint_footers = TRUE;
#else
			{
				int length;

				*attrs = (Frame_attribute) ATTR_NOP(*attrs);
				if ((char *)attrs[1] == (char *)NULL)
					length = -1;
				else
					length = strlen((char *)attrs[1]);
				if (frame->left_footer)
					free(frame->left_footer);
				if (length != -1) {
					frame->left_footer = (char *)xv_malloc((unsigned long)length + 1);
					strcpy(frame->left_footer, (char *)attrs[1]);
					if (frame->footer_proc) {
						(frame->footer_proc) (frame_public, &frame->footer,
								FRAME_FOOTER_LEFT, frame->left_footer);
					}
				}
				else {
					frame->left_footer = NULL;
				}
				if (status_get(frame, show_footer))
					paint_footers = TRUE;
			}
#endif /* OW_I18N */

			break;

		case FRAME_RIGHT_FOOTER:

#ifdef OW_I18N
			*attrs = (Frame_attribute) ATTR_NOP(*attrs);
			_xv_set_mbs_attr_dup(&frame->right_footer, (char *)attrs[1]);
			if (status_get(frame, show_footer))
				paint_footers = TRUE;
#else
			{
				int length;

				*attrs = (Frame_attribute) ATTR_NOP(*attrs);
				if ((char *)attrs[1] == (char *)NULL)
					length = -1;
				else
					length = strlen((char *)attrs[1]);
				if (frame->right_footer)
					free(frame->right_footer);
				if (length != -1) {
					frame->right_footer = (char *)xv_malloc((unsigned long)length + 1);
					strcpy(frame->right_footer, (char *)attrs[1]);
					if (frame->footer_proc) {
						(frame->footer_proc) (frame_public, &frame->footer,
								FRAME_FOOTER_RIGHT, frame->right_footer);
					}
				}
				else {
					frame->right_footer = NULL;
				}
				if (status_get(frame, show_footer))
					paint_footers = TRUE;
			}
#endif /* OW_I18N */

			break;

#ifdef OW_I18N
		case FRAME_LEFT_FOOTER_WCS:
			*attrs = (Frame_attribute) ATTR_NOP(*attrs);
			_xv_set_wcs_attr_dup(&frame->left_footer, (wchar_t *)attrs[1]);
			if (status_get(frame, show_footer))
				paint_footers = TRUE;
			break;

		case FRAME_RIGHT_FOOTER_WCS:
			*attrs = (Frame_attribute) ATTR_NOP(*attrs);
			_xv_set_wcs_attr_dup(&frame->right_footer, (wchar_t *)attrs[1]);
			if (status_get(frame, show_footer))
				paint_footers = TRUE;
			break;
#endif /* OW_I18N */

			/* ACC_XVIEW */
		case FRAME_MENUS:{
				Menu_item item;
				int i, k;
				int count = 0, item_number;
				Xv_server server_public;

				server_public = XV_SERVER_FROM_WINDOW(frame_public);

				if (!SERVER_PRIVATE(server_public)->acceleration)
					break;
				/*
				 * Count number of menus on avlist
				 */
				for (i = 1; attrs[i]; ++i, ++count);

				/*
				 * For each menu currently in list, remove
				 * accelerators that exist for it.
				 */
				for (i = 0; i < frame->menu_count; ++i) {
					Menu menu;

					menu = frame->menu_list[i];

					/*
					 * Get number of menu items on this menu
					 */
					item_number = (int)xv_get(menu, MENU_NITEMS);

					/* 
					 * for each menu item remove it's accelerator on the frame
					 */
					for (k = 1; k < item_number + 1; k++) {
						item = (Menu) xv_get(menu, MENU_NTH_ITEM, k);
						menu_set_acc_on_frame(frame_public, menu, item, FALSE);
					}
					/* 
					 * Remove the frame from this menu's list of frames 
					 */
					xv_set(menu, MENU_FRAME_DELETE, frame_public, NULL);
				}

				/*
				 * If new count is more than current list capacity
				 */
				if (count > frame->max_menu_count) {
					xv_free(frame->menu_list);

					frame->max_menu_count += FRAME_MENU_BLOCK;
					frame->menu_list = (Menu *) xv_calloc((unsigned)
							frame->max_menu_count, (unsigned)sizeof(Menu));
				}

				frame->menu_count = count;

				/*
				 * Copy menu list from avlist to frame
				 */
				for (i = 0; i < count; ++i) {
					Menu menu;
					Menu pullright;

					menu = frame->menu_list[i] = attrs[i + 1];

					/*
					 * Get number of menu items
					 */
					item_number = (int)xv_get(menu, MENU_NITEMS);

					/* 
					 * For each menu item set the accelerator on the frame
					 */
					for (k = 1; k < item_number + 1; k++) {
						item = (Menu) xv_get(menu, MENU_NTH_ITEM, k);

						/*
						 * Add accelerator to frame
						 * Check first if this is a pullright. If it is,
						 * use FRAME_MENU_ADD on it.
						 */
						if ((pullright = (Menu) xv_get(item, MENU_PULLRIGHT))) {
							xv_set(frame_public, FRAME_MENU_ADD, pullright, NULL);
						}
						else {
							menu_set_acc_on_frame(frame_public, menu, item, TRUE);
						}
					}

					/* 
					 * add the frame to this menu's list of frames 
					 */
					xv_set(menu, MENU_FRAME_ADD, frame_public, NULL);
				}

				break;
			}

		case FRAME_MENU_ADD:{
				Xv_server server_public;
				int i, found = FALSE, item_number;
				Menu menu = attrs[1];

				server_public = XV_SERVER_FROM_WINDOW(frame_public);

				if (!SERVER_PRIVATE(server_public)->acceleration)
					break;

				/*
				 * Add one menu to menu list in frame
				 */

				/*
				 * Search for menu in current list
				 */
				for (i = 0; i < frame->menu_count; ++i) {
					if (menu == frame->menu_list[i]) {
						found = TRUE;
						break;
					}
				}

				/*
				 * menu not found in current list
				 */
				if (!found) {
					/*
					 * Adding this menu will overflow list
					 * We need to alloc a new list
					 */
					if (frame->menu_count + 1 > frame->max_menu_count) {
						Menu *new_list;

						frame->max_menu_count += FRAME_MENU_BLOCK;

						/*
						 * Alloc new list
						 */
						new_list = (Menu *) xv_calloc((unsigned)
								frame->max_menu_count, (unsigned)sizeof(Menu));

						/*
						 * Copy old list to new list if there was an old list
						 */
						if (frame->menu_count) {
							int j;

							for (j = 0; j < frame->menu_count; ++j) {
								new_list[j] = frame->menu_list[j];
							}

							/*
							 * Free old list
							 */
							xv_free(frame->menu_list);
						}

						frame->menu_list = new_list;
					}

					frame->menu_list[frame->menu_count] = menu;
					frame->menu_count++;
				}

				/*
				 * Regardless of whether the menu was on the current list or not,
				 * we redo it's accelerators
				 */

				/*
				 * Get number of menu items
				 */
				item_number = (int)xv_get(menu, MENU_NITEMS);

				/* 
				 * for each menu item on the menu set the accelerator on the frame
				 * using FRAME_MENU_ACCELERATOR
				 */
				for (i = 1; i < item_number + 1; i++) {
					Menu_item item;
					Menu pullright;

					item = (Menu) xv_get(menu, MENU_NTH_ITEM, i);

					/*
					 * Add accelerator to frame
					 * Check first if this is a pullright. If it is,
					 * use FRAME_MENU_ADD recursively on it.
					 */
					if ((pullright = (Menu) xv_get(item, MENU_PULLRIGHT))) {
						xv_set(frame_public, FRAME_MENU_ADD, pullright, NULL);
					}
					else {
						menu_set_acc_on_frame(frame_public, menu, item, TRUE);
					}
				}

				/* 
				 * Add the frame to this menu's list of frames 
				 */
				xv_set(menu, MENU_FRAME_ADD, frame_public, NULL);

				break;
			}

		case FRAME_MENU_DELETE:{
				int i, found = FALSE;
				Menu menu = attrs[1];

				/*
				 * Remove one menu from the menu list of the frame
				 */

				/*
				 * Search for menu in current list
				 */
				for (i = 0; i < frame->menu_count; ++i) {
					if (menu == frame->menu_list[i]) {
						found = TRUE;
						break;
					}
				}

				if (found) {
					int j, item_number;

					/*
					 * Shift everything by one to fill gap
					 */
					for (j = i + 1; j < frame->menu_count; ++i, ++j) {
						frame->menu_list[i] = frame->menu_list[j];
					}

					frame->menu_count--;

					/*
					 * Get menu item count
					 */
					item_number = (int)xv_get(menu, MENU_NITEMS);

					/* 
					 * for each menu item on the menu unset/remove the accelerator 
					 * on the frame using FRAME_MENU_ACCELERATOR
					 */
					for (i = 1; i < item_number + 1; i++) {
						Menu_item item;
						Menu pullright;

						item = (Menu) xv_get(menu, MENU_NTH_ITEM, i);

						/*
						 * Remove accelerator from frame
						 * Check first if this is a pullright. If it is,
						 * use FRAME_MENU_DELETE recursively on it.
						 */
						if ((pullright = (Menu) xv_get(item, MENU_PULLRIGHT))) {
							xv_set(frame_public, FRAME_MENU_DELETE, pullright, NULL);
						}
						else
							menu_set_acc_on_frame(frame_public, menu, item, FALSE);
					}

				}

				break;
			}
			/* ACC_XVIEW */

		case FRAME_SHOW_LABEL:	/* same as FRAME_SHOW_HEADER */
			attrs[0] = (Frame_attribute) ATTR_NOP(attrs[0]);
			if (status_get(frame, show_label) == (int)attrs[1])
				break;

			status_set(frame, show_label, (int)attrs[1]);

			if ((int)attrs[1])
				add_decor++;
			else
				delete_decor++;

			break;

		case FRAME_SHOW_RESIZE_CORNER:
			attrs[0] = (Frame_attribute) ATTR_NOP(attrs[0]);
			if (status_get(frame, show_resize_corner) == (int)attrs[1])
				break;

			status_set(frame, show_resize_corner, (int)attrs[1]);

			if ((int)attrs[1])
				add_decor++;
			else
				delete_decor++;

			break;

		case FRAME_RESIZE_PANEL:
			attrs[0] = (Frame_attribute) ATTR_NOP(attrs[0]);
			resize_width = (int)attrs[1];
			need_resizing = TRUE;
			break;

		case FRAME_RESIZE_LAYOUT_PROC:
			attrs[0] = (Frame_attribute) ATTR_NOP(attrs[0]);
			layout = (xv_frame_layout_cb_t)attrs[1];
			layout_cldt = (void *)attrs[2];
			break;

		case XV_RECT:
			/* Intercept attempt to set the rect and adjust for footer */
			if (status_get(frame, show_footer) && frame->footer) {
				Rect *rect = (Rect *) attrs[1];
				int footer_height = 0;

				footer_height = (int)xv_get(frame->footer, XV_HEIGHT);
				rect->r_height += footer_height;
				(void)win_setrect(frame_public, rect);
				rect->r_height -= footer_height;
				ATTR_CONSUME(attrs[0]);
			}

#ifdef OW_I18N
			if (status_get(frame, show_imstatus) && frame->imstatus) {
				Rect *rect = (Rect *) attrs[1];
				int IMstatus_height = 0;

				IMstatus_height = (int)xv_get(frame->imstatus, XV_HEIGHT);
				rect->r_height += IMstatus_height;
				(void)win_setrect(frame_public, rect);
				rect->r_height -= IMstatus_height;
				ATTR_CONSUME(attrs[0]);
			}
#endif

			break;

		case XV_HEIGHT:
			/* Intercept attempt to set the height and adjust for footer */
			if (status_get(frame, show_footer) && frame->footer) {
				int height = (int)attrs[1];
				Rect rect;

				height += (int)xv_get(frame->footer, XV_HEIGHT);
				(void)win_getrect(frame_public, &rect);
				rect.r_height = height;
				win_setrect(frame_public, &rect);
				ATTR_CONSUME(attrs[0]);
			}

#ifdef OW_I18N
			if (status_get(frame, show_imstatus) && frame->imstatus) {
				int height = (int)attrs[1];
				Rect rect;

				height += (int)xv_get(frame->imstatus, XV_HEIGHT);
				(void)win_getrect(frame_public, &rect);
				rect.r_height = height;
				win_setrect(frame_public, &rect);
				ATTR_CONSUME(attrs[0]);
			}
#endif

			break;

		case XV_SHOW:
			/*
			 * Map the icon. The FRAME pkg should not let the window package
			 * know about icons and wmgr.
			 */
			if (frame_is_iconic(frame) && (int)attrs[1]) {
				attrs[0] = (Attr_attribute) WIN_MAP;
				wmgr_top(frame_public);
			}

			/* If the frame is going withdrawn, then update the initial
			 * state hint so that when it is mapped again, it will be in
			 * the same state as it was when it was unmapped.
			 */
			if (!(int)attrs[1]) {
				Xv_Drawable_info *info;

				DRAWABLE_INFO_MACRO(frame_public, info);

				frame->wmhints.initial_state =
						(status_get(frame, initial_state) ? IconicState : NormalState);
				frame->wmhints.flags |= StateHint;
				XSetWMHints(xv_display(info), xv_xid(info), &(frame->wmhints));
			}
			/* Don't expect a change of state if we are already in the state */
			/* requested.  Also if our initial state is iconic, don't expect */
			/* the frame to see a change of state and if we are iconic going */
			/* to withdrawn, don't expect a state change.            */
			if ((window_get_map_cache(frame_public) != (int)attrs[1]) &&
					(((int)attrs[1] && !status_get(frame, initial_state)) ||
							(!(int)attrs[1] && !frame_is_iconic(frame))))
				status_set(frame, map_state_change, TRUE);

			break;

		case XV_X:
		case XV_Y:
			if (!(frame->normal_hints.flags & PPosition)) {
				frame->normal_hints.flags |= PPosition;
				update_hints = TRUE;
			}
			break;

		case FRAME_MIN_SIZE:{
				int footer_height = 0;

				if (!(int)attrs[1] && !(int)attrs[2])
					frame->normal_hints.flags &= ~PMinSize;
				else
					frame->normal_hints.flags |= PMinSize;

				if (status_get(frame, show_footer) && frame->footer &&
						(frame->normal_hints.flags & PMinSize))
					footer_height += (int)xv_get(frame->footer, XV_HEIGHT);

#ifdef OW_I18N
				if (status_get(frame, show_imstatus) && frame->imstatus &&
						(frame->normal_hints.flags & PMinSize))
					footer_height += (int)xv_get(frame->imstatus, XV_HEIGHT);
#endif

				frame->normal_hints.min_width = (int)attrs[1];
				frame->normal_hints.min_height = (int)attrs[2] + footer_height;

				update_hints = True;
				break;
			}

		case FRAME_MAX_SIZE:{
				int footer_height = 0;

				if (!(int)attrs[1] && !(int)attrs[2])
					frame->normal_hints.flags &= ~PMaxSize;
				else
					frame->normal_hints.flags |= PMaxSize;

				if (status_get(frame, show_footer) && frame->footer &&
						(frame->normal_hints.flags & PMinSize))
					footer_height += (int)xv_get(frame->footer, XV_HEIGHT);

#ifdef OW_I18N
				if (status_get(frame, show_imstatus) && frame->imstatus &&
						(frame->normal_hints.flags & PMinSize))
					footer_height += (int)xv_get(frame->imstatus, XV_HEIGHT);
#endif

				frame->normal_hints.max_width = (int)attrs[1];
				frame->normal_hints.max_height = (int)attrs[2] + footer_height;

				update_hints = True;
				break;
			}

		case FRAME_COMPOSE_STATE:
			frame_update_compose_led(frame, (int)attrs[1]);
			break;

#ifdef OW_I18N
		case FRAME_SHOW_IMSTATUS:
			{
				int show_imstatus;

				attrs[0] = (Frame_attribute) ATTR_NOP(attrs[0]);
				show_imstatus = (int)attrs[1];

				if (status_get(frame, show_imstatus) != show_imstatus) {
					if (status_get(frame, created)) {
						if (show_imstatus) {
							if (frame->imstatus == NULL) {
								frame->imstatus = frame_create_IMstatus(frame);
							}
							else {
								xv_set(frame->imstatus, XV_SHOW, TRUE, NULL);
							}
							/* Adjust frame's min/max size hints. */
							frame_adjust_normal_hints(frame,
									(int)xv_get(frame->imstatus, XV_HEIGHT),
									&update_hints);
						}
						else {
							if (frame->imstatus != NULL) {
								xv_set(frame->imstatus, XV_SHOW, FALSE, NULL);
								/* Adjust frame's min/max size hints. */
								frame_adjust_normal_hints(frame,
										-(int)xv_get(frame->imstatus,
												XV_HEIGHT), &update_hints);
							}
						}
					}
					status_set(frame, show_imstatus, show_imstatus);
				}
			}
			break;

		case FRAME_INACTIVE_IMSTATUS:
			status_set(frame, inactive_imstatus, (int)attrs[1]);
			if (status_get(frame, show_imstatus))
				paint_imstatus = TRUE;
			break;

		case FRAME_LEFT_IMSTATUS:
			*attrs = (Frame_attribute) ATTR_NOP(*attrs);
			_xv_set_mbs_attr_dup(&frame->left_IMstatus, (char *)attrs[1]);
			if (status_get(frame, show_imstatus))
				paint_imstatus = TRUE;
			break;

		case FRAME_LEFT_IMSTATUS_WCS:
			*attrs = (Frame_attribute) ATTR_NOP(*attrs);
			_xv_set_wcs_attr_dup(&frame->left_IMstatus, (wchar_t *)attrs[1]);
			if (status_get(frame, show_imstatus))
				paint_imstatus = TRUE;
			break;

		case FRAME_RIGHT_IMSTATUS:
			*attrs = (Frame_attribute) ATTR_NOP(*attrs);
			_xv_set_mbs_attr_dup(&frame->right_IMstatus, (char *)attrs[1]);
			if (status_get(frame, show_imstatus))
				paint_imstatus = TRUE;
			break;

		case FRAME_RIGHT_IMSTATUS_WCS:
			*attrs = (Frame_attribute) ATTR_NOP(*attrs);
			_xv_set_wcs_attr_dup(&frame->right_IMstatus, (wchar_t *)attrs[1]);
			if (status_get(frame, show_imstatus))
				paint_imstatus = TRUE;
			break;
#endif /* OW_I18N */

		case WIN_CMS:
			/*
			 * Remember new cms set on frame, and set flag to update
			 * footer/imstatus window color
			 */
			new_frame_cms = (Cms) attrs[1];
			update_footer_color = TRUE;
			break;

		case WIN_FOREGROUND_COLOR:
			/*
			 * Remember new fg color set on frame, set flag to indicate fg 
			 * color was set, and set flag to update footer/imstatus window 
			 * color
			 */
			new_frame_fg = (int)attrs[1];
			new_frame_fg_set = TRUE;
			update_footer_color = TRUE;
			break;

		case XV_END_CREATE:{
				unsigned long icon_id;
				Xv_Drawable_info *info;

				DRAWABLE_INFO_MACRO(frame_public, info);

				owner = xv_get(frame_public, XV_OWNER);
				is_subframe = (int)xv_get(owner, XV_IS_SUBTYPE_OF, FRAME) ||
						(int)xv_get(owner, XV_IS_SUBTYPE_OF, FRAME_CMD) ||
						(int)xv_get(owner, XV_IS_SUBTYPE_OF, FRAME_HELP);

				/*
				 * if this is a subframe, we need to reparent it to be a
				 * child of the root window and not of the main frame
				 */
				if (is_subframe) {
					xv_set(frame_public,
							WIN_PARENT, xv_get(frame_public, XV_ROOT), NULL);
				}
				else if (!frame->icon) {
					/* Create a default icon for a main frame */
					frame->icon = frame->default_icon =
							xv_create(owner, ICON,
							XV_OWNER, frame_public,
							ICON_IMAGE, &default_frame_icon_mpr, NULL);

					icon_id = (unsigned long)xv_get(frame->icon, XV_XID);
					if (set_icon_rect) {
						(void)win_setrect(frame->icon, &icon_rect);
					}
					frame->wmhints.flags |= IconWindowHint;
					frame->wmhints.icon_window = icon_id;
					if (set_icon_rect) {
						frame->wmhints.flags |= IconPositionHint;
						frame->wmhints.icon_x = icon_rect.r_left;
						frame->wmhints.icon_y = icon_rect.r_top;
					}
					XSetWMHints(xv_display(info), xv_xid(info), &(frame->wmhints));
					set_icon_rect = FALSE;
				}
				if (set_icon_rect) {
					set_icon_rect = FALSE;
					(void)win_setrect(frame->icon, &icon_rect);
					frame->wmhints.flags |= IconPositionHint;
					frame->wmhints.icon_x = icon_rect.r_left;
					frame->wmhints.icon_y = icon_rect.r_top;
					XSetWMHints(xv_display(info), xv_xid(info), &(frame->wmhints));
				}

				/*
				 * set the command line options.  This gives the command
				 * line the highest precedence: default, client attrs,
				 * command line.
				 *
				 */

				/*
				 * Set command line options that apply to ALL frames
				 * These include window background/foreground color,
				 * window fonts.
				 */
				if (frame_all_set_cmdline_options(frame_public) != XV_OK) {
					result = XV_ERROR;
				}

				/*
				 * These command line options apply to toplevel frames only
				 */
				if (!is_subframe) {
					if (frame_set_cmdline_options(frame_public, FALSE) != XV_OK)
						result = XV_ERROR;
					if (frame_set_icon_cmdline_options(frame_public) != XV_OK)
						result = XV_ERROR;
				}
				/*
				 * Now position the frame if its position is not fixed yet.
				 */
				frame_set_position(owner, frame);

				/*
				 * On color, set frame colors, if any. Make icon inherit 
				 * frame's fg/bg. 
				 */
				if (status_get(frame, frame_color)) {
					frame_set_color(frame, &frame->fg, &frame->bg);
				}

				if (frame->icon) {
					Cms cms;

					/* 
					 * Make sure we have the same visuals for the icon
					 * and the frame, then inherit colors.
					 */
					if (XVisualIDFromVisual((Visual *) xv_get(frame_public,
											XV_VISUAL)) ==
							XVisualIDFromVisual((Visual *)
									xv_get(frame->icon, XV_VISUAL))) {
						/*
						 * Dont override icon's colormap segment if it has
						 * been set programatically.
						 */
						cms = (Cms) xv_get(frame->icon, WIN_CMS);
						if (xv_get(cms, CMS_DEFAULT_CMS)
								&& (cms != xv_cms(info)))
							xv_set(frame->icon, WIN_CMS, xv_cms(info), NULL);
					}

				}

				/* Create a footer if needed */
				if (status_get(frame, show_footer)) {
					frame->footer = frame_create_footer(frame);
					/* Adjust frame's min/max size hints. */
					frame_adjust_normal_hints(frame,
							(int)xv_get(frame->footer, XV_HEIGHT), &update_hints);
				}

#ifdef OW_I18N
				if (xv_get(frame_public, WIN_USE_IM) == TRUE
						&& (XIM) xv_get((Xv_opaque) xv_server(info), XV_IM) != 0)
					status_set(frame, show_imstatus, TRUE);
				/* Create a IMstatus if needed */
				if (status_get(frame, show_imstatus)) {
					frame->imstatus = frame_create_IMstatus(frame);
					/* Adjust frame's min/max size hints. */
					frame_adjust_normal_hints(frame,
							(int)xv_get(frame->imstatus, XV_HEIGHT), &update_hints);
				}
#endif

				status_set(frame, created, TRUE);
				break;
			}

		default:
			xv_check_bad_attr(FRAME_CLASS, attrs[0]);
			break;
	}

	if (need_resizing) {
		set_frame_resizing(frame_public, resize_width, layout, layout_cldt);
	}

	/* recompute wmgr decorations */
	if (add_decor || delete_decor) {
		Xv_Drawable_info *info;
		Xv_opaque server_public;
		Atom add_decor_list[WM_MAX_DECOR], delete_decor_list[WM_MAX_DECOR];

		add_decor = delete_decor = 0;

		DRAWABLE_INFO_MACRO(frame_public, info);
		server_public = xv_server(info);

		if (xv_get(xv_screen(info), SCREEN_CHECK_SUN_WM_PROTOCOL,
							"_SUN_OL_WIN_ATTR_5"))
		{
			/*
			 * Tell wmgr not to write icon labels - for now this will be done
			 * by XView
			 */
			delete_decor_list[delete_decor++] =
					(Atom) xv_get(server_public, SERVER_ATOM,
					"_OL_DECOR_ICON_NAME");
		}

		if (status_get(frame, show_label))
			add_decor_list[add_decor++] =
					(Atom) xv_get(server_public, SERVER_WM_DECOR_HEADER);
		else
			delete_decor_list[delete_decor++] =
					(Atom) xv_get(server_public, SERVER_WM_DECOR_HEADER);

		if (status_get(frame, show_resize_corner))
			add_decor_list[add_decor++] =
					(Atom) xv_get(server_public, SERVER_WM_DECOR_RESIZE);
		else
			delete_decor_list[delete_decor++] =
					(Atom) xv_get(server_public, SERVER_WM_DECOR_RESIZE);

		wmgr_add_decor(frame_public, add_decor_list, add_decor);

		wmgr_delete_decor(frame_public, delete_decor_list, delete_decor);
	}

	/*
	 * If the frame's color was changed via WIN_CMS or WIN_FOREGROUND_COLOR,
	 * make sure they are inherited by the footer/im status window
	 */
	if (update_footer_color) {
		int repaint_needed = FALSE;

		frame_update_status_win_color(frame_public, frame->footer,
				new_frame_cms, new_frame_fg, new_frame_fg_set, &repaint_needed);

		if (repaint_needed && status_get(frame, show_footer))
			paint_footers = TRUE;

#ifdef OW_I18N
		frame_update_status_win_color(frame_public, frame->imstatus,
				new_frame_cms, new_frame_fg, new_frame_fg_set, &repaint_needed);

		if (repaint_needed && status_get(frame, show_imstatus))
			paint_imstatus = TRUE;
#endif /* OW_I18N */
	}

	if (status_get(frame, created) && paint_footers)
		frame_display_footer(frame_public, TRUE, NULL, NULL, NULL, NULL);

#ifdef OW_I18N
	if (status_get(frame, created) && paint_imstatus)
		frame_display_IMstatus(frame_public, TRUE);
#endif

	if (update_hints) {
		Xv_Drawable_info *info;

		DRAWABLE_INFO_MACRO(frame_public, info);
		XSetWMNormalHints(xv_display(info), xv_xid(info), &frame->normal_hints);
	}

#ifdef OW_I18N
	if (pswcs.storage != NULL)
		xv_free(pswcs.storage);
#endif /* OW_I18N */

	return (Xv_opaque) result;
}


/*
 * Change the state of frame to iconic or open.
 */
static void frame_change_state(Frame_class_info *frame, int to_iconic)
{
    Frame             frame_public = FRAME_PUBLIC(frame);
    Frame	      child;
    Xv_Drawable_info *info;

    DRAWABLE_INFO_MACRO(frame_public, info);
    status_set(frame, iconic, to_iconic);

    /* If the window is not mapped yet, then we just change the intial state  */
    /* so that when the window is mapped, it will appear as the user expected */
    frame->wmhints.initial_state = (to_iconic ? IconicState : NormalState);
    frame->wmhints.flags |= StateHint;
    XSetWMHints(xv_display(info), xv_xid(info), &(frame->wmhints));
    status_set(frame, initial_state, to_iconic);

    /* Insure that unmapped subframes maintain the same initial state as
     * their owners.  The window manager use to enforce this policy but
     * as of 5//91, it does not anymore.
     */
    FRAME_EACH_SUBFRAME(frame, child)
	if (!xv_get(child, XV_SHOW))
	    xv_set(child, FRAME_CLOSED, to_iconic, NULL);
    FRAME_END_EACH

    /* If the window is already mapped, then change it from Normal to Iconic */
    /* or Iconic to Normal.		     				     */
    if (xv_get(frame_public, XV_SHOW)) {
        /* icon is dealt with in wmgr_close/open() */
    	if (to_iconic)
	    XIconifyWindow(xv_display(info), xv_xid(info),
					(int)xv_get(xv_screen(info), SCREEN_NUMBER));
        else {
	    status_set(frame, map_state_change, TRUE);
	    XMapWindow(xv_display(info), xv_xid(info));
	}
    }
}

static void frame_adjust_normal_hints(Frame_class_info *frame, int adjustment, Bool *update_hints)
{
    /* Adjust frame's min/max size hints. */
    /* Typically, we are compensating for the footer. */

    if (frame->normal_hints.flags & PMinSize) {
	frame->normal_hints.min_height += adjustment;
	*update_hints = True;
    }

    if (frame->normal_hints.flags & PMaxSize) {
	frame->normal_hints.max_height += adjustment;
	*update_hints = True;
    }
}

static void frame_set_icon(Frame_class_info *frame, Icon icon, int *set_icon_rect, Rect icon_rect)	
{
    unsigned long   	 icon_id;
    Xv_Drawable_info 	*info;
    short            	 sameicon = FALSE;
    Cms			 cms;

    if ((frame->default_icon) && (frame->default_icon != icon)) {
	xv_destroy(frame->default_icon);
	frame->default_icon = XV_NULL;
    }
		
    if (frame->icon == icon) {
	/*
	 * this will prevent notifying WM with the same icon
	 * window. The current WM will destroy the window
	 * even though it is the same one.  This will cause
	 * no icon to appear.
	 */
	sameicon = TRUE;
    } else if (!icon) {
	/* If they passed in NULL as an icon, revert back to the
	 * default icon.
	 */
	if (!frame->default_icon)
	    frame->default_icon = xv_create(
			xv_get(FRAME_PUBLIC(frame), XV_OWNER), ICON,
				    XV_OWNER,       FRAME_PUBLIC(frame),
				    ICON_IMAGE,     &default_frame_icon_mpr,
				    NULL);

	icon = frame->default_icon;
    }
    frame->icon = icon;
    xv_set(frame->icon, XV_OWNER, FRAME_PUBLIC(frame), NULL);
    icon_id = (unsigned long) xv_get(frame->icon, XV_XID);
    if (*set_icon_rect) {
        (void) win_setrect(frame->icon, &icon_rect);
    }
		
    if (!sameicon) {
        frame->wmhints.flags |= IconWindowHint;
	frame->wmhints.icon_window = icon_id;
	if (*set_icon_rect) {
	    frame->wmhints.flags |= IconPositionHint;
	    frame->wmhints.icon_x = icon_rect.r_left;
	    frame->wmhints.icon_y = icon_rect.r_top;
	}
	DRAWABLE_INFO_MACRO(FRAME_PUBLIC(frame), info);
	XSetWMHints(xv_display(info), xv_xid(info), &(frame->wmhints));

	/* Set the cms if appropriate */
	if (XVisualIDFromVisual((Visual *)xv_get(FRAME_PUBLIC(frame),
							XV_VISUAL)) ==
		XVisualIDFromVisual((Visual *)xv_get(frame->icon, XV_VISUAL))) {
	    /*
	     * Dont override icon's colormap segment if it has
	     * been set programatically.
	     */
	    cms = (Cms) xv_get(frame->icon, WIN_CMS);
	    if (xv_get(cms, CMS_DEFAULT_CMS) && (cms != xv_cms(info)))
	        xv_set(frame->icon, WIN_CMS, xv_cms(info), NULL);
	}
    }
    *set_icon_rect = FALSE;
}

/* ACC_XVIEW */
static void frame_set_menu_acc(Frame frame_public, int keycode, unsigned int	state, KeySym keysym, CHAR *keystr, frame_accel_notify_func notify_proc, Xv_opaque data)
{
    Xv_server			server_public;
    Frame_class_info		*frame = FRAME_CLASS_PRIVATE(frame_public);
    Frame_menu_accelerator	*new_accel;

    new_accel = xv_alloc(Frame_menu_accelerator);
    new_accel->notify_proc = notify_proc;
    new_accel->data = data;
    new_accel->code = keycode;
    new_accel->keysym = keysym;

    new_accel->modifiers = state;

#ifdef OW_I18N
    _xv_set_wcs_attr_dup(&new_accel->keystr, keystr);
#else
    /*
     * Use already malloc'd string
     */
    new_accel->keystr = xv_strsave(keystr);
#endif /* OW_I18N */

    /*
     * Link to current list on frame
     */
    new_accel->next = frame->menu_accelerators;
    
    /*
     * Add (first) new accelerator
     */
    if (!frame->menu_accelerators) {
        unsigned int	mask;


        /*
         * If this is the first accelerator for the
         * frame, make sure it has KeyPress events
         * selected
         */
        mask = (unsigned int)xv_get(frame_public, 
                                    WIN_CONSUME_X_EVENT_MASK);
        if (!(mask & KeyPressMask) || !(mask & FocusChangeMask))  {
            xv_set(frame_public, WIN_CONSUME_X_EVENT_MASK, 
                            KeyPressMask | FocusChangeMask, 
                            NULL);
        }
    }

    /* 
     * Make this new accelerator the first in the list
     */
    frame->menu_accelerators = new_accel;

    /*
     * Get handle to server object
     */
    server_public = XV_SERVER_FROM_WINDOW(frame_public);

    /*
     * Set entry in server accelerator map
     */
    xv_set(server_public, SERVER_ADD_ACCELERATOR_MAP, 
                        keysym, state,
                        NULL);
}
static Xv_opaque frame_get_attr(Frame frame_public, int *status,
									Attr_attribute attr, va_list valist)
{
	register Frame_class_info *frame = FRAME_CLASS_PRIVATE(frame_public);
	static Rect rect_local;

	switch (attr) {
		case XV_RECT:
			{
				static Rect rect;

				win_getrect(frame_public, &rect);
				/* need to account for the possible footer window */
				if (status_get(frame, show_footer)
						&& status_get(frame, created))
					rect.r_height -= (int)xv_get(frame->footer, XV_HEIGHT);

#ifdef OW_I18N
				/* need to account for the possible imstatus window */
				if (status_get(frame, show_imstatus)
						&& status_get(frame, created))
					rect.r_height -= (int)xv_get(frame->imstatus, XV_HEIGHT);
#endif

				return (Xv_opaque) & rect;
			}

		case XV_HEIGHT:
			{
				Rect rect;
				int height;

				win_getrect(frame_public, &rect);
				height = rect.r_height;
				/* need to account for the possible footer window */
				if (status_get(frame, show_footer)
						&& status_get(frame, created))
					height -= (int)xv_get(frame->footer, XV_HEIGHT);

#ifdef OW_I18N
				/* need to account for the possible imstatus window */
				if (status_get(frame, show_imstatus)
						&& status_get(frame, created))
					height -= (int)xv_get(frame->imstatus, XV_HEIGHT);
#endif

				return (Xv_opaque) height;
			}

		case WIN_FIT_WIDTH:
			return (Xv_opaque) frame_fit_direction(frame, WIN_DESIRED_WIDTH);

		case WIN_FIT_HEIGHT:
			{
				int height;

				height = frame_fit_direction(frame, WIN_DESIRED_HEIGHT);
				if (status_get(frame, show_footer)
						&& status_get(frame, created))
					height += (int)xv_get(frame->footer, XV_HEIGHT);

#ifdef OW_I18N
				if (status_get(frame, show_imstatus)
						&& status_get(frame, created))
					height += (int)xv_get(frame->imstatus, XV_HEIGHT);
#endif

				return (Xv_opaque) height;
			}

		case FRAME_ICON:
			return frame->icon;

		case FRAME_NTH_SUBWINDOW:
			{
				register Xv_Window sw;
				register int n = va_arg(valist, int);

				FRAME_EACH_SUBWINDOW(frame, sw)
					if (--n == 0) return sw;
				FRAME_END_EACH return XV_NULL;
			}

		case FRAME_OLD_RECT:
			return ((Xv_opaque) & frame->oldrect);

		case FRAME_DEFAULT_DONE_PROC:
			return (Xv_opaque) frame->default_done_proc;

		case FRAME_DONE_PROC:
			return (Xv_opaque) (frame->done_proc ?
					frame->done_proc : frame->default_done_proc);

		case FRAME_FOCUS_DIRECTION:
			return (Xv_opaque) (frame->focus_window ?
					xv_get(frame->focus_window, XV_KEY_DATA,
							FRAME_FOCUS_DIRECTION) : FRAME_FOCUS_UP);

		case FRAME_FOCUS_WIN:
			return (Xv_opaque) frame->focus_window;

		case FRAME_FOREGROUND_COLOR:
			{
				static Xv_singlecolor fg;

				fg.red = (unsigned char)(frame->fg.red >> 8);
				fg.green = (unsigned char)(frame->fg.green >> 8);
				fg.blue = (unsigned char)(frame->fg.blue >> 8);
				return ((Xv_opaque) (&fg));
			}

		case FRAME_BACKGROUND_COLOR:
			{
				static Xv_singlecolor bg;

				bg.red = (unsigned char)(frame->bg.red >> 8);
				bg.green = (unsigned char)(frame->bg.green >> 8);
				bg.blue = (unsigned char)(frame->bg.blue >> 8);
				return ((Xv_opaque) (&bg));
			}

		case FRAME_SHOW_FOOTER:
			return (Xv_opaque) status_get(frame, show_footer);

		case FRAME_LEFT_FOOTER:

#ifdef OW_I18N
			return (Xv_opaque) _xv_get_mbs_attr_dup(&frame->left_footer);
#else
			return (Xv_opaque) frame->left_footer;
#endif

#ifdef OW_I18N
		case FRAME_LEFT_FOOTER_WCS:
			return (Xv_opaque) _xv_get_wcs_attr_dup(&frame->left_footer);
#endif

		case FRAME_RIGHT_FOOTER:

#ifdef OW_I18N
			return (Xv_opaque) _xv_get_mbs_attr_dup(&frame->right_footer);
#else
			return (Xv_opaque) frame->right_footer;
#endif

#ifdef OW_I18N
		case FRAME_RIGHT_FOOTER_WCS:
			return (Xv_opaque) _xv_get_wcs_attr_dup(&frame->right_footer);
#endif

		case FRAME_LABEL:

#ifdef OW_I18N
			return (Xv_opaque) _xv_get_mbs_attr_dup(&frame->label);
#else
			return (Xv_opaque) frame->label;
#endif

#ifdef OW_I18N
		case XV_LABEL_WCS:
			return (Xv_opaque) _xv_get_wcs_attr_dup(&frame->label);
#endif

		case FRAME_NO_CONFIRM:
			return (Xv_opaque) status_get(frame, no_confirm);

		case FRAME_NTH_SUBFRAME:{
				register Xv_Window sw;
				register int n = va_arg(valist, int);

				FRAME_EACH_SUBFRAME(frame, sw)
						if (--n == 0)
					return sw;
				FRAME_END_EACH return XV_NULL;
			}

		case FRAME_CLOSED:
			/* If the frame is Withdrawn, return the inital_state the frame will
			 * be in when it is mapped.                      */
			if (xv_get(frame_public, XV_SHOW))
				return (Xv_opaque) frame_is_iconic(frame);
			else
				return (Xv_opaque) status_get(frame, initial_state);

		case FRAME_INHERIT_COLORS:
			return (Xv_opaque) xv_get(frame_public, WIN_INHERIT_COLORS);

		case FRAME_SUBWINDOWS_ADJUSTABLE:	/* WIN_BOUNDARY_MGR: */
			return (Xv_opaque) status_get(frame, bndrymgr);

		case WIN_TYPE:
			return (Xv_opaque) FRAME_TYPE;

		case FRAME_BUSY:
			return (Xv_opaque) status_get(frame, busy);

#ifdef OW_I18N
		case FRAME_RIGHT_IMSTATUS:
			return (Xv_opaque) _xv_get_mbs_attr_dup(&frame->right_IMstatus);

		case FRAME_LEFT_IMSTATUS:
			return (Xv_opaque) _xv_get_mbs_attr_dup(&frame->left_IMstatus);

		case FRAME_RIGHT_IMSTATUS_WCS:
			return (Xv_opaque) _xv_get_wcs_attr_dup(&frame->right_IMstatus);

		case FRAME_LEFT_IMSTATUS_WCS:
			return (Xv_opaque) _xv_get_wcs_attr_dup(&frame->left_IMstatus);

		case FRAME_SHOW_IMSTATUS:
			return (Xv_opaque) status_get(frame, show_imstatus);

		case FRAME_INACTIVE_IMSTATUS:
			return (Xv_opaque) status_get(frame, inactive_imstatus);

#ifdef FULL_R5
		case FRAME_IMSTATUS_RECT:{
				static Rect imstatus_rect;

				if (status_get(frame, show_imstatus)
						&& status_get(frame, created)
						&& frame->imstatus) {
					imstatus_rect = *(Rect *) xv_get(frame->imstatus, WIN_RECT);
				}
				else {
					imstatus_rect.r_left = 0;
					imstatus_rect.r_top = 0;
					imstatus_rect.r_width = 0;
					imstatus_rect.r_height = 0;
				}
				return (Xv_opaque) & imstatus_rect;
			}
#endif /* FULL_R5 */
#endif

		case FRAME_ACCELERATOR:
		case FRAME_X_ACCELERATOR:{
				short code = (short)va_arg(valist, int);
				KeySym keysym = va_arg(valist, KeySym);
				Frame_accelerator *accel;

				for (accel = frame->accelerators; accel; accel = accel->next) {
					if (accel->code == code || accel->keysym == keysym)
						break;
				}
				return (Xv_opaque) accel;
			}

			/* ACC_XVIEW */
		case FRAME_MENUS:
			return ((Xv_opaque) frame->menu_list);

		case FRAME_MENU_COUNT:
			return ((Xv_opaque) frame->menu_count);

#ifdef OW_I18N
		case FRAME_MENU_ACCELERATOR:
		case FRAME_MENU_ACCELERATOR_WCS:
			/*
			 * which_attr is to determine which attribute is used.
			 * pswcs is used to take advantage of the _xv_pswcs functions
			 * that malloc/convert mb/wcs strings.
			 */
			Attr_attribute which_attr = attr;
			_xv_pswcs_t pswcs = { 0, NULL, NULL };
#else
		case FRAME_MENU_ACCELERATOR:
#endif /* OW_I18N */

			{

#ifdef OW_I18N
				/*
				 * which_attr is to determine which attribute is used.
				 * pswcs is used to take advantage of the _xv_pswcs functions
				 * that malloc/convert mb/wcs strings.
				 */
				Attr_attribute which_attr = attr;
				_xv_pswcs_t pswcs = { 0, NULL, NULL };
#endif /* OW_I18N */

				short kc;
				KeyCode keycode;
				int result;
				unsigned int state;
				KeySym keysym;
				Frame_menu_accelerator *menu_accel;
				Xv_server server_public;
				CHAR *keystr;

#ifdef OW_I18N
				/*
				 * Use different macro to duplicate/convert strings depending
				 * on whether mb/wcs attribute was used.
				 */
				if (which_attr == FRAME_MENU_ACCELERATOR) {
					_xv_pswcs_mbsdup(&pswcs, va_arg(valist, char *));
				}
				else {
					_xv_pswcs_wcsdup(&pswcs, va_arg(valist, CHAR *));
				}

				keystr = pswcs.value;
#else
				keystr = va_arg(valist, char *);
#endif

				/*
				 * Return NULL if key string passed in is NULL
				 */
				if (!keystr) {
					return (Xv_opaque) NULL;
				}

				/*
				 * Get server handle
				 */
				server_public = XV_SERVER_FROM_WINDOW(frame_public);

				/*
				 * From keystring, get keycode, state, keysym...
				 */
				result = server_parse_keystr(server_public, keystr, &keysym,
						&kc, &state, 0, NULL);
				keycode = kc;

				/*
				 * If get parsing error, return NULL
				 */
				if (result != XV_OK) {

#ifdef OW_I18N
					if (pswcs.storage != NULL)
						xv_free(pswcs.storage);
#endif /* OW_I18N */

					return (Xv_opaque) NULL;
				}

				/*
				 * Search for menu accelerator with matching keycode/state
				 * or keysym
				 */
				menu_accel = frame_find_menu_acc(frame_public, keycode, state,
						keysym, FALSE);

#ifdef OW_I18N
				if (pswcs.storage != NULL)
					xv_free(pswcs.storage);
#endif /* OW_I18N */

				return (Xv_opaque) menu_accel;
			}

		case FRAME_MENU_X_ACCELERATOR:{
				int keycode;
				unsigned int state;
				KeySym keysym;
				Frame_menu_accelerator *menu_accel;

				keycode = va_arg(valist, int);
				state = va_arg(valist, unsigned int);

				keysym = va_arg(valist, KeySym);

				/*
				 * Search for menu accelerator with matching state and 
				 * keycode/keysym
				 */
				menu_accel = frame_find_menu_acc(frame_public, keycode, state,
						keysym, FALSE);

				return (Xv_opaque) menu_accel;

			}
			/* ACC_XVIEW */

		case FRAME_COMPOSE_STATE:
			return ((Xv_opaque) status_get(frame, compose_led));

		case FRAME_MIN_SIZE:{
				int *width = va_arg(valist, int *),
						*height = va_arg(valist, int *), footer_height = 0;

				if (status_get(frame, show_footer) && frame->footer &&
						(frame->normal_hints.flags & PMinSize))
					footer_height += (int)xv_get(frame->footer, XV_HEIGHT);

#ifdef OW_I18N
				if (status_get(frame, show_imstatus) && frame->imstatus &&
						(frame->normal_hints.flags & PMinSize))
					footer_height += (int)xv_get(frame->imstatus, XV_HEIGHT);
#endif

				*width = frame->normal_hints.min_width;
				*height = frame->normal_hints.min_height - footer_height;
				return ((Xv_opaque) 0);
			}

		case FRAME_MAX_SIZE:{
				int *width = va_arg(valist, int *),
						*height = va_arg(valist, int *), footer_height = 0;

				if (status_get(frame, show_footer) && frame->footer &&
						(frame->normal_hints.flags & PMaxSize))
					footer_height += (int)xv_get(frame->footer, XV_HEIGHT);

#ifdef OW_I18N
				if (status_get(frame, show_imstatus) && frame->imstatus &&
						(frame->normal_hints.flags & PMaxSize))
					footer_height += (int)xv_get(frame->imstatus, XV_HEIGHT);
#endif

				*width = frame->normal_hints.max_width;
				*height = frame->normal_hints.max_height - footer_height;
				return ((Xv_opaque) 0);
			}

			/* Here for SunView1 compatibility only. */
		case FRAME_CLOSED_RECT:
			{
				register int is_subframe =
						(int)xv_get(xv_get(frame_public, WIN_OWNER),
						XV_IS_SUBTYPE_OF, FRAME_CLASS);

				/* subframes don't have a closed rect */
				if (is_subframe)
					return XV_NULL;
				(void)win_getrect(frame->icon, &rect_local);
				return (Xv_opaque) & rect_local;
			}

			/* Here for SunView1 compatibility only. */
		case FRAME_CURRENT_RECT:
			if (frame_is_iconic(frame)) {
				(void)win_getrect(frame->icon, &rect_local);
				return (Xv_opaque) & rect_local;
			}
			else {
				return xv_get(frame_public, WIN_RECT);
			}

		case FRAME_SHOW_LABEL:
			attr = (Frame_attribute) ATTR_NOP(attr);
			return (Xv_opaque) status_get(frame, show_label);

		case FRAME_SHOW_RESIZE_CORNER:
			attr = (Frame_attribute) ATTR_NOP(attr);
			return (Xv_opaque) status_get(frame, show_resize_corner);

		default:
			if (xv_check_bad_attr(FRAME_CLASS, attr) == XV_ERROR) {
				*status = XV_ERROR;
			}
			return (Xv_opaque) 0;
	}
}


Pkg_private void window_set_parent_dying(void);
Pkg_private void window_unset_parent_dying(void);

extern int dra_frame_debug;

/*
 * Return XV_OK if confirmation is allowed, and the user says yes, or if
 * confirmation is not allowed. Reset confirmation to be allowed.
 */
static int frame_confirm_destroy(Frame_class_info *frame)
{
	static int frame_notice_key;
	Xv_object window = FRAME_CLASS_PUBLIC(frame);
	Xv_Notice frame_notice;
	int result;

	if (status_get(frame, no_confirm)) {
		status_set(frame, no_confirm, FALSE);
		return XV_OK;
	}

	if (frame_is_iconic(frame))
		window = (Xv_object) frame->icon;

	if (!frame_notice_key) {
		frame_notice_key = xv_unique_key();
	}

	frame_notice = xv_get((Frame) window, XV_KEY_DATA, frame_notice_key, NULL);

	if (!frame_notice) {
		frame_notice = xv_create((Frame) window, NOTICE,
				NOTICE_MESSAGE_STRINGS,
					XV_MSG("Are you sure you want to Quit?"),
					NULL,
				NOTICE_BUTTON_YES, XV_MSG("Confirm"),
				NOTICE_BUTTON_NO, XV_MSG("Cancel"),
				NOTICE_NO_BEEPING, 1,
				NOTICE_STATUS, &result,
				NOTICE_BUSY_FRAMES, window, NULL,
				XV_SHOW, TRUE,
				NULL);

		xv_set((Frame) window,
				XV_KEY_DATA, frame_notice_key, frame_notice, NULL);
	}
	else {
		xv_set(frame_notice,
				NOTICE_MESSAGE_STRINGS,
					XV_MSG("Are you sure you want to Quit?"),
					NULL,
				NOTICE_BUTTON_YES, XV_MSG("Confirm"),
				NOTICE_BUTTON_NO, XV_MSG("Cancel"),
				NOTICE_NO_BEEPING, 1,
				NOTICE_STATUS, &result,
				NOTICE_BUSY_FRAMES, window, NULL,
				XV_SHOW, TRUE,
				NULL);
	}

	/* BUG ALERT Should not abort if alerts failed */
	if (result == NOTICE_FAILED)
		xv_error((Xv_opaque) frame,
				ERROR_STRING, XV_MSG("Notice failed on attempt to destroy frame."),
				ERROR_PKG, FRAME,
				NULL);
	return ((result == NOTICE_YES) ? XV_OK : XV_ERROR);
}


/*
 * free the frame struct and all its resources.
 */
static void frame_free(Frame_class_info *frame)
{
	Frame_accelerator *accel;
	Frame_accelerator *next_accel;

	/* ACC_XVIEW */
	Frame_menu_accelerator *menu_accel;
	Frame_menu_accelerator *next_menu_accel;

	/* ACC_XVIEW */

	/* Free frame struct */

#ifdef OW_I18N
	_xv_free_ps_string_attr_dup(&frame->label);
#else
	if (frame->label)
		xv_free(frame->label);
#endif /* OW_I18N */

	/*
	 * Free window acclelerator info
	 */
	for (accel = frame->accelerators; accel; accel = next_accel) {
		next_accel = accel->next;
		xv_free(accel);
	}
	/* ACC_XVIEW */
	/*
	 * Free menu acclelerator info
	 */
	for (menu_accel = frame->menu_accelerators; menu_accel;
			menu_accel = next_menu_accel) {
		next_menu_accel = menu_accel->next;

#ifdef OW_I18N
		_xv_free_ps_string_attr_dup(&menu_accel->keystr);
#else
		if (menu_accel->keystr) {
			xv_free(menu_accel->keystr);
		}
		xv_free(menu_accel);
#endif /* OW_I18N */
	}

	/*
	 * Free menu list used by menu accelerators
	 */
	if (frame->menu_list) {
		xv_free(frame->menu_list);
	}
	/* ACC_XVIEW */

	free((char *)frame);
}

/*
 * Destroy the frame struct, its subwindows, and its subframes. If any
 * subwindow or subframe vetoes the destroy, return NOTIFY_IGNORED. If
 * confirmation is enabled, prompt the user.
 */
static int frame_destroy(Frame frame_public, Destroy_status status)
{
	Frame_class_info *frame = FRAME_CLASS_PRIVATE(frame_public);
	Xv_Window child;
	int is_subframe = (int)xv_get(xv_get(frame_public, WIN_OWNER),
										XV_IS_SUBTYPE_OF, FRAME_CLASS);

	if (dra_frame_debug > 0) {
		fprintf(stderr, "%s:%d: frame_destroy(%ld, %d)\n",
				__FILE__, __LINE__, frame_public, status);
	}

	/*
	 * Unmapped the frame that is about to be destroyed.;
	 * Added call xv_set( XV_SHOW, FALSE ) for performance
	 * reasons; speeds up deletion of panel text item with
	 * associated dnd drop sites; 1078467
	 */
	if ((status == DESTROY_CLEANUP) || (status == DESTROY_PROCESS_DEATH)) {
		xv_set(frame_public, XV_SHOW, FALSE, NULL);
		win_remove(frame_public);
	}

	/* destroy the subframes */
	FRAME_EACH_SUBFRAME(frame, child)
		if (notify_post_destroy(child, status, NOTIFY_IMMEDIATE) != XV_OK)
			return XV_ERROR;
	FRAME_END_EACH
	/* Since this frame is going away, set a flag to tell
	 * window_destroy_win_struct not to destroy any subwindows...when the
	 * frame window is destroyed the server will destroy all subwindows for us.
	 */
	if ((status != DESTROY_CHECKING) && (status != DESTROY_SAVE_YOURSELF))
		window_set_parent_dying();

	/* destroy the subwindows */
	FRAME_EACH_SUBWINDOW(frame, child)
		if (notify_post_destroy(child, status, NOTIFY_IMMEDIATE) != XV_OK)
			return XV_ERROR;
	FRAME_END_EACH

	if ((status != DESTROY_CHECKING) && (status != DESTROY_SAVE_YOURSELF)) {
		window_unset_parent_dying();
		/*
		 * Conditionally stop the notifier.  This is a special case so that
		 * single tool clients don't have to interpose in from of tool_death
		 * in order to notify_remove(...other clients...) so that
		 * notify_start will return.
		 */
		if (!is_subframe) {
			if (--frame_notify_count == 0)
				(void)notify_stop();
		}
	}
	else if (status != DESTROY_SAVE_YOURSELF) {
		/* subframe does not need confirmation */
		return ((is_subframe || frame_confirm_destroy(frame) == XV_OK)
				? XV_OK : XV_ERROR);
	}
	if (status == DESTROY_CLEANUP) {	/* waste of time if ...PROCESS_DEATH */
		if (frame->footer != XV_NULL) {
			xv_destroy(frame->footer);
		}

#ifdef OW_I18N
		_xv_free_ps_string_attr_dup(&frame->left_footer);
		_xv_free_ps_string_attr_dup(&frame->right_footer);
		if (frame->imstatus != NULL)
			xv_destroy(frame->imstatus);
		_xv_free_ps_string_attr_dup(&frame->left_IMstatus);
		_xv_free_ps_string_attr_dup(&frame->right_IMstatus);
#else
		if (frame->left_footer != NULL)
			free(frame->left_footer);
		if (frame->right_footer != NULL)
			free(frame->right_footer);
#endif /* OW_I18N */

		if (frame->default_icon) {
			if (frame->icon == frame->default_icon) {
				xv_destroy(frame->icon);
				frame->icon = XV_NULL;
				frame->default_icon = XV_NULL;
			}
			else if (frame->icon) {
				xv_destroy(frame->icon);
				frame->icon = XV_NULL;
				xv_destroy(frame->default_icon);
				frame->default_icon = XV_NULL;
			}
		}
		else {
			if (frame->icon) {
				xv_destroy(frame->icon);
				frame->icon = XV_NULL;
			}
		}

		if (frame->focus_window != XV_NULL) {
			Server_image image;
			GC  gc;

			image = xv_get(frame->focus_window,
					XV_KEY_DATA, FRAME_FOCUS_UP_IMAGE);
			if (image) {
				xv_destroy(image);
			}

			image = xv_get(frame->focus_window,
					XV_KEY_DATA, FRAME_FOCUS_RIGHT_IMAGE);
			if (image) {
				xv_destroy(image);
			}

			gc = (GC) xv_get(frame->focus_window, XV_KEY_DATA, FRAME_FOCUS_GC);
			if (gc) {
				Xv_Drawable_info *info;

				DRAWABLE_INFO_MACRO(frame->focus_window, info);
				XFreeGC(xv_display(info), gc);
				xv_set(frame->focus_window, XV_KEY_DATA, FRAME_FOCUS_GC,
						(GC) NULL, NULL);
			}

			xv_destroy(frame->focus_window);
			frame->focus_window = (Xv_window) NULL;
		}

		frame_free(frame);
	}
	return XV_OK;
}

const Xv_pkg xv_frame_class_pkg = {
    "Frame", (Attr_pkg) ATTR_PKG_FRAME,
    sizeof(Xv_frame_class),
    WINDOW,
    frame_init,
    frame_set_avlist,
    frame_get_attr,
    frame_destroy,
    NULL			/* no find proc */
};

