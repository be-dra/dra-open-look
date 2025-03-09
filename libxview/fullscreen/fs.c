#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)fs.c 20.45 93/06/28 DRA $Id: fs.c,v 2.4 2025/03/08 13:50:55 dra Exp $";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL_NOTICE 
 *	file for terms of the license.
 */

#include <xview_private/i18n_impl.h>
#include <xview_private/xview_impl.h>
#include <xview_private/fs_impl.h>
#include <xview_private/svr_impl.h>
#include <xview_private/win_info.h>
#include <xview/notify.h>
#include <xview/defaults.h>
#include <xview/screen.h>
#include <xview/window.h>
#include <X11/Xlib.h>

Xv_private char * fullscreen_translate_report_code(int	code);
Pkg_private void fullscreen_update_globals(void);

/*
 * owner of fullscreen object is any "Window". default root window is xv_root
 * of owner, default input window is owner.
 */

static int fullscreen_init_internal(Xv_opaque owner, Xv_opaque slf,
						Attr_avlist avlist, int *offset_ptr)
{
	Xv_fullscreen *fullscreen_public = (Xv_fullscreen *)slf;
    Fullscreen_info	   *fullscreen;
    Attr_avlist 	    attrs;
    Xv_fullscreen	   *fullscreen_object;
    int                 saw_win_attr = FALSE;
    int                 sync_now = FALSE;
    int                 x, y;
    struct  fullscreen *myfullscreenstruct =
				(struct fullscreen *)&fullscreen_public->fullscreen_struct;

    /*
     * Read resource db to see if debug variables have changed
     */
    fullscreen_update_globals();

    /* Allocate private data and set up forward/backward links. */
    fullscreen = (Fullscreen_info *)xv_alloc(Fullscreen_info);
    fullscreen->public_self = (Xv_opaque)myfullscreenstruct;
    fullscreen_object = (Xv_fullscreen *)fullscreen_public;
    fullscreen_object->private_data = (Xv_opaque)fullscreen;

    notify_set_destroy_func(fullscreen->public_self,
						(Notify_destroy_func)xv_destroy_status);

    fullscreen->root_window = owner;
    fullscreen->input_window = owner;
    fullscreen->cursor_window = 0;	/* initialization flag */
    fullscreen->cursor = 0;	        /* initialization flag */
    fullscreen->im_changed = FALSE;
    
    fullscreen->grab_pointer = TRUE;	/* initialization flag */
    fullscreen->grab_kbd = TRUE;	/* initialization flag */
    fullscreen->grab_server = TRUE;	/* initialization flag */
    fullscreen->pointer_ptr_mode = FULLSCREEN_ASYNCHRONOUS;
    fullscreen->pointer_kbd_mode = FULLSCREEN_ASYNCHRONOUS;
    fullscreen->keyboard_ptr_mode = FULLSCREEN_ASYNCHRONOUS;
    fullscreen->keyboard_kbd_mode = FULLSCREEN_ASYNCHRONOUS;
    fullscreen->owner_events = FALSE;

    for (attrs = avlist; *attrs; attrs = attr_next((Xv_opaque *) attrs)) {
	switch ((int) attrs[0]) {
	  case FULLSCREEN_INPUT_WINDOW:
	    fullscreen->input_window = (Xv_opaque) attrs[1];
	    ATTR_CONSUME(attrs[0]);
	    break;

	  case FULLSCREEN_CURSOR_WINDOW:
	    fullscreen->cursor_window = (Xv_opaque) attrs[1];
	    ATTR_CONSUME(attrs[0]);
	    break;
	  case FULLSCREEN_PAINT_WINDOW:
	    fullscreen->root_window = (Xv_opaque) attrs[1];
	    ATTR_CONSUME(attrs[0]);
	    break;
	  case FULLSCREEN_SYNC:
	    sync_now = (int) attrs[1];
	    ATTR_CONSUME(attrs[0]);
	    break;
	  case FULLSCREEN_GRAB_POINTER:
	    fullscreen->grab_pointer = (int) attrs[1];
	    ATTR_CONSUME(attrs[0]);
	    break;
	  case FULLSCREEN_GRAB_KEYBOARD:
	    fullscreen->grab_kbd = (int) attrs[1];
	    ATTR_CONSUME(attrs[0]);
	    break;
	  case FULLSCREEN_GRAB_SERVER:
	    fullscreen->grab_server = (int) attrs[1];
	    ATTR_CONSUME(attrs[0]);
	    break;
	  case FULLSCREEN_KEYBOARD_GRAB_PTR_MODE:
	    fullscreen->keyboard_ptr_mode = (Fullscreen_grab_mode) attrs[1];
	    ATTR_CONSUME(attrs[0]);
	    break;
	  case FULLSCREEN_KEYBOARD_GRAB_KBD_MODE:
	    fullscreen->keyboard_kbd_mode = (Fullscreen_grab_mode) attrs[1];
	    ATTR_CONSUME(attrs[0]);
	    break;
	  case FULLSCREEN_POINTER_GRAB_PTR_MODE:
	    fullscreen->pointer_ptr_mode = (Fullscreen_grab_mode) attrs[1];
	    ATTR_CONSUME(attrs[0]);
	    break;
	  case FULLSCREEN_POINTER_GRAB_KBD_MODE:
	    fullscreen->pointer_kbd_mode = (Fullscreen_grab_mode) attrs[1];
	    ATTR_CONSUME(attrs[0]);
	    break;
	  case FULLSCREEN_OWNER_EVENTS:
	    fullscreen->owner_events = (int) attrs[1];
	    ATTR_CONSUME(attrs[0]);
	    break;
	  case FULLSCREEN_ALLOW_SYNC_EVENT:
	    /* don't consume here, do in _set instead. */
	    break;
	  case WIN_CURSOR:
	    fullscreen->cursor = (Xv_Cursor) attrs[1];
	    ATTR_CONSUME(attrs[0]);
	    break;

	  case WIN_INPUT_MASK:
	  case WIN_CONSUME_EVENT:
	  case WIN_IGNORE_EVENT:
	  case WIN_CONSUME_EVENTS:
	  case WIN_IGNORE_EVENTS:
	    saw_win_attr = TRUE;
	    break;

	  default:
	    break;
	}
    }

    if (!fullscreen->input_window) {
	fullscreen->input_window = xv_get(xv_default_screen, XV_ROOT);
    }
    if (!fullscreen->root_window) {
	fullscreen->root_window = xv_get(xv_default_screen, XV_ROOT);
    }
    if (!fullscreen->cursor) {
	fullscreen->cursor = xv_get(fullscreen->input_window, WIN_CURSOR);
    }
    /* check if we are already in fullscreen mode on this window */
    if (xv_get(fullscreen->input_window, WIN_IS_IN_FULLSCREEN_MODE)) {
	xv_error(XV_NULL,
		 ERROR_STRING, XV_MSG("Already in fullscreen mode!"),
		 ERROR_PKG, FULLSCREEN,
		 NULL);
	return XV_ERROR;
    } else {
	(void) xv_set(fullscreen->input_window, WIN_IS_IN_FULLSCREEN_MODE,
		      TRUE, NULL);
    }

    /*
     * cache the input windows input mask
     */
    win_getinputmask(fullscreen->input_window, &fullscreen->cached_im,
		     (Xv_opaque *)NULL);

    if (saw_win_attr) {
	/*
	 * old inputmask already cached, set new im
	 */
	(void) xv_set_avlist(fullscreen->input_window, avlist);
	fullscreen->im_changed = TRUE;
	for (attrs = avlist; *attrs;
	     attrs = attr_next((Xv_opaque *) attrs)) {
	    switch ((int) attrs[0]) {
	      case WIN_INPUT_MASK:
	      case WIN_CONSUME_EVENT:
	      case WIN_IGNORE_EVENT:
	      case WIN_CONSUME_EVENTS:
	      case WIN_IGNORE_EVENTS:
		ATTR_CONSUME(attrs[0]);
		break;

	      default:
		break;
	    }
	}
    }
    if (fullscreen->cursor)
	(void) xv_set(fullscreen->cursor, XV_INCREMENT_REF_COUNT, NULL);

    if (!fullscreendebug) {
	int             grab_successful, grab_result;
	win_getinputmask(fullscreen->input_window,
			 &fullscreen->inputmask, (Xv_opaque *)NULL);
	grab_result = xv_win_grab(fullscreen->input_window,
			          &fullscreen->inputmask,
			          fullscreen->cursor_window,
			          fullscreen->cursor,
			          fullscreendebugptr ? FALSE : fullscreen->grab_pointer,
			          fullscreendebugkbd ? FALSE : fullscreen->grab_kbd,
			          fullscreendebugserver ? FALSE : fullscreen->grab_server,
			          (int)fullscreen->pointer_ptr_mode,
			          (int)fullscreen->pointer_kbd_mode,
			          (int)fullscreen->keyboard_ptr_mode,
			          (int)fullscreen->keyboard_kbd_mode,
			          fullscreen->owner_events,
			          &grab_successful);
	if (!grab_successful) {
	    (void) xv_error((Xv_opaque)fullscreen,
			    ERROR_STRING,
			      fullscreen_translate_report_code(grab_result),
			    ERROR_PKG, FULLSCREEN,
			    NULL);
	    if (fullscreen->im_changed)	/* reset inputmask */
		(void) xv_set(fullscreen->input_window,
			      WIN_INPUT_MASK, &fullscreen->cached_im, NULL);
	    if (fullscreen->cursor)
		(void) xv_set(fullscreen->cursor, XV_DECREMENT_REF_COUNT, NULL);
	    free((char *) fullscreen);
	    return (XV_ERROR);
	}
	fullscreen->sync_mode_now = sync_now;
    }
    myfullscreenstruct->fs_windowfd = (int) fullscreen->input_window;
    myfullscreenstruct->fs_rootwindow = fullscreen->root_window;
    myfullscreenstruct->inputmask = fullscreen->inputmask;
    myfullscreenstruct->fs_screenrect =
	*(Rect *) xv_get(fullscreen->root_window, WIN_RECT);
    win_translate_xy(fullscreen->root_window, fullscreen->input_window,
		     myfullscreenstruct->fs_screenrect.r_left,
		     myfullscreenstruct->fs_screenrect.r_top, &x, &y);
    myfullscreenstruct->fs_screenrect.r_left = x;
    myfullscreenstruct->fs_screenrect.r_top = y;
    *offset_ptr = xv_set_embedding_data((Xv_opaque)myfullscreenstruct,
					(Xv_opaque)fullscreen_public);

    server_set_fullscreen(xv_get(xv_get(fullscreen->root_window, XV_SCREEN),
				 SCREEN_SERVER),
			  TRUE);
    return XV_OK;
}

static int fullscreen_destroy_internal(Fullscreen fullscreen_public,
									Destroy_status status)
{
    Fullscreen_info *fullscreen = FULLSCREEN_PRIVATE(fullscreen_public);

    if ((status == DESTROY_CHECKING) || (status == DESTROY_SAVE_YOURSELF))
	return XV_OK;

    if (!fullscreendebug)
        (void) xv_win_ungrab(fullscreen->input_window,
        		     fullscreen->grab_pointer,
        		     fullscreen->grab_kbd,
        		     fullscreen->grab_server);

    if (fullscreen->im_changed)	/* reset inputmask */
	(void) xv_set(fullscreen->input_window, WIN_INPUT_MASK,
		      &fullscreen->cached_im, NULL);
    if (fullscreen->cursor)
	(void) xv_set(fullscreen->cursor, XV_DECREMENT_REF_COUNT, NULL);

    server_set_fullscreen(xv_get(xv_get(fullscreen->root_window, XV_SCREEN),
				 SCREEN_SERVER),
			  FALSE);
    (void) xv_set(fullscreen->input_window, WIN_IS_IN_FULLSCREEN_MODE,
		  FALSE, NULL);
    free((char *) fullscreen);
    return XV_OK;
}

Xv_private char * fullscreen_translate_report_code(int	code)
{
    switch (code) {
      case (AlreadyGrabbed):
        return ((char *)
		XV_MSG("Already in fullscreen by another client. Fullscreen Failed."));
        
      case (GrabInvalidTime):
        return ((char *)XV_MSG("Invalid time. Fullscreen failed."));
        
      case (GrabNotViewable):
        return ((char *)XV_MSG("Fullscreen window or confine-to window not mapped. Fullscreen failed."));
        
      case (GrabFrozen):
        return ((char *)XV_MSG("Already frozen by another client. Fullscreen Failed."));
        
      case (GrabSuccess):
      default:
      	return ((char *)NULL);
    }
}

Pkg_private void fullscreen_update_globals(void)
{

    static short	fullscreen_globals_init;

    if (!fullscreen_globals_init)  {
	fullscreen_globals_init = 1;

        fullscreendebug = (int)defaults_get_boolean("fullscreen.debug", 
		"Fullscreen.Debug", fullscreendebug);

        fullscreendebugserver = (int)defaults_get_boolean("fullscreen.debugserver", 
		"Fullscreen.Debugserver", fullscreendebugserver);

        fullscreendebugkbd = (int)defaults_get_boolean("fullscreen.debugkbd", 
   		 "Fullscreen.Debugkbd", fullscreendebugkbd);

        fullscreendebugptr = (int)defaults_get_boolean("fullscreen.debugptr", 
		"Fullscreen.Debugptr", fullscreendebugptr);
    }

}

const Xv_pkg xv_fullscreen_pkg = {
    "Fullscreen", ATTR_PKG_FULLSCREEN,
    sizeof(Xv_fullscreen),
    &xv_generic_pkg,
    fullscreen_init_internal,
    fullscreen_set_avlist,
    fullscreen_get_attr,
    fullscreen_destroy_internal,
    NULL			/* no find proc */
};

/*
 * When not zero will not actually acquire exclusive io lock so that the
 * debugger doesn't get hung.
 */
int             fullscreendebug;

/*
 * When not zero will not grab device server/pointer/keyboard
 */
int             fullscreendebugserver;
int             fullscreendebugptr;
int             fullscreendebugkbd;

