/*	@(#)win_info.h 20.17 93/06/28 SMI  DRA: $Id: win_info.h,v 4.5 2026/02/13 09:18:31 dra Exp $	*/

/****************************************************************************/
/*	
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license. 
 */
/****************************************************************************/

#ifndef _xview_win_visual_h_already_included
#define _xview_win_visual_h_already_included

#include <xview_private/scrn_vis.h>
#include <xview/win_input.h>
#include <xview/rect.h>

typedef struct {
    XID			 xid;
    Screen_visual	*visual;
	/* Flags */
    unsigned		 private_gc	: 1;	/* Should be gc itself? */
} Win_info;

#define	win_xid(info)		((info)->xid)
#define	win_display(info)	((info)->visual->display)
#define	win_server(info)	((info)->visual->server)
#define	win_screen(info)	((info)->visual->screen)
#define	win_root(info)		((info)->visual->root_window)
#define	win_depth(info)		((info)->visual->depth)
#define	win_image(info)		((info)->visual->image)

#define	window_gc(window, info) \
	((info)->private_gc ? window_private_gc((window)) : (info)->visual->gc)

#define	win_set_image(info, im)	(info)->visual->image = im

extern GC window_private_gc(Xv_opaque);
extern Win_info		*window_info(Display *, Window);
extern Xv_object	win_data(Display *, Window);
extern void win_getsize(Xv_object window, Rect *rect);
extern XID win_getlink(Xv_object window, int linkname);
extern void win_setlink(Xv_object window, int linkname, XID number);
extern void win_set_parent(Xv_object window, Xv_object parent,int x,int y);
extern void window_set_parent(Xv_object window, Xv_object parent);
extern void win_insert(Xv_object window);
extern void win_insert_in_front(Xv_object window);
extern void win_remove(Xv_object window);
extern char *win_name_for_qualified_xid(char *name, Display *display, XID xid);
extern Xv_object win_number_to_object(Xv_object window, XID number);
extern XID win_nametonumber(char *name);
extern void win_numbertoname(int winnumber, char *name);
extern char *win_fdtoname(Xv_object window, char *name);
extern XID win_fdtonumber(Xv_object window);
extern void win_free(Xv_object window);
extern int win_is_mapped(Xv_object window);
extern int win_view_state(Display *display, XID xid);
extern void win_change_property(Xv_object, Attr_attribute, Atom, int, unsigned char *, int);
extern void win_get_property(Xv_object, Attr_attribute, long, long, Atom, unsigned long *, unsigned long *, unsigned char **);
extern void win_bell(Xv_object window, struct timeval tv, Xv_object pw);
void win_set_no_focus(Xv_object window, int state);
Xv_private void win_getrect(Xv_object window, Rect *rect);
Xv_private void win_setrect(Xv_object window, Rect *rect);

Xv_private void win_xmask_to_im(unsigned int xevent_mask, Inputmask *im);
Xv_private unsigned int win_im_to_xmask(Xv_object window, Inputmask *im);
Xv_object input_readevent(Xv_object window, Event *event);
void win_refuse_kbd_focus(Xv_object window);
void win_release_event_lock(Xv_object window);
XID win_get_kbd_focus(Xv_object window);
Xv_private int win_translate_xy_internal(Display *display, XID src_id, XID dst_id, int src_x, int src_y, int *dst_x, int *dst_y);
Xv_private void win_set_outer_rect(Xv_object window, Rect *rect);
Xv_private void win_x_getrect(Display *display, XID xid, Rect *rect);
Xv_private void win_get_outer_rect(Xv_object window, Rect *rect);
Xv_private int win_get_retained(Xv_object window);
Xv_private Bool win_check_lang_mode(Xv_opaque server, Display *display, Event *event);
Pkg_private int win_do_expose_event(Display *display, Event *event, XExposeEvent *e, Xv_opaque *window, int collapse_exposures);
Xv_private Notify_value xv_input_pending(Display *, int);
Xv_private void win_set_damage(Xv_object window, Rectlist *rl);
Xv_private void win_clear_damage(Xv_object window);
Xv_private void win_get_cmdline_option(Xv_object window, char *str,
											char *appl_cmdline);
Xv_private void win_set_wm_command_prop(Xv_object window, char **argv,
							char **appl_cmdline_argv, int appl_cmdline_argc);
Xv_private void win_setmouseposition(Xv_object window, int x, int y);
Xv_private void win_getmouseposition(Xv_object window, int *x, int *y);
XID win_findintersect(Xv_object window, int x, int y);
Xv_private XID xv_get_softkey_xid(Xv_object server, Display *display);
Xv_private void win_beep(Display *display, struct timeval tv);

void win_lockdata(Xv_object window);
void win_unlockdata(Xv_object window);
Xv_private int xv_win_grab(Xv_object window, Inputmask *im, Xv_object cursor_window, Xv_object cursor, int grab_pointer, int grab_kbd, int grab_server, int grap_pointer_pointer_async, int	grab_pointer_keyboard_async, int	grap_kbd_pointer_async, int	grab_kbd_keyboard_async, int owner_events, int *status);
Xv_private int xv_win_ungrab(Xv_object window, int ungrab_pointer, int ungrab_kbd, int ungrab_server);
Xv_private int win_grabio(Xv_object window);
Xv_private int win_xgrabio_async(Xv_object window, Inputmask *im, Xv_object cursor_window, Xv_object cursor);
Xv_private int win_xgrabio_sync(Xv_object window, Inputmask *im, Xv_object cursor_window, Xv_object cursor);
Xv_private void win_set_grabio_params(Xv_object window, Inputmask *im, Xv_opaque cursor);
Xv_private void win_releaseio(Xv_object window);
Xv_private void win_private_gc(Xv_object window, int create_private_gc);
Xv_private int win_convert_to_x_rectlist(Rectlist *rl, XRectangle *xrect_array,
										int xrect_count);
Xv_private void win_repaint_application(Display *dpy);
Xv_private void win_dispatch_expose(Display *dpy, XEvent *);
Xv_private void win_dispatch_focus_out(Display *dpy, XEvent *);

#define CONTEXT		1
#endif
