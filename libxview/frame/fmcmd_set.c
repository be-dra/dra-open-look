#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)fmcmd_set.c 1.46 93/06/28 DRA: $Id: fmcmd_set.c,v 4.3 2024/11/22 22:13:51 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <unistd.h>
#include <errno.h>
#include <xview_private/draw_impl.h>
#include <xview_private/wmgr_decor.h>
#include <xview_private/frame_cmd.h>
#include <xview_private/win_info.h>
#include <xview/panel.h>
#include <xview/server.h>
#include <xview/defaults.h>

static int update_default_pin_state(Frame_cmd_info *frame, Xv_opaque server_public);

/* This function can be used as PANEL_BACKGROUND_PROC if you want a 
 * control area behave as specified in the OLSpec: mouse events fall through
 * to the window manager's decoration.
 * This is accomplished by sending ClientMessages of type _OL_PROPAGATE_EVENT
 * to the root window. These ClientMessages have
 * + window = xid of the frame
 * + data.l[0] : type | button | state
 * + data.l[1] : mouse ((x <<16 ) | y)  in frame coord
 * + data.l[2] : time
 *
 * These messages will be understood by an olwm with the atom _DRA_ENHANCED_OLWM
 * in the _SUN_WM_PROTOCOLS....
 */
void xv_ol_default_background(Xv_opaque pan, Event *ev)
{
	static int fallThroughControlArea = -1;
	XEvent *xev;
	Display *dpy;
	XEvent xcl;
	XButtonEvent *xb;
	unsigned frame_x, frame_y;

	if (fallThroughControlArea < 0) {
		fallThroughControlArea = defaults_get_boolean(
									"openWindows.fallThroughControlArea",
									"OpenWindows.FallThroughControlArea",
									TRUE);
	}

	if (! fallThroughControlArea) return;

	/* Danger here: this might happen... */
	switch (event_action(ev)) {
		case PANEL_EVENT_CANCEL:
		case ACTION_DRAG_PREVIEW:   /* just to have silence */
		case LOC_DRAG:   /* just to have silence */
		case ACTION_WHEEL_FORWARD:  /* mouse events! - but not for olwm */
		case ACTION_WHEEL_BACKWARD: /* mouse events! - but not for olwm */
			return;
		case ACTION_SELECT:
		case ACTION_ADJUST:
		case ACTION_MENU:
			break;
		default:
			fprintf(stderr, "%s`%s-%d: event_action = %d\n", __FILE__,
							__FUNCTION__, __LINE__, event_action(ev));
			break;
	}

	xev = event_xevent(ev);
	if (! xev) return;
	if (xev->type != ButtonPress && xev->type != ButtonRelease) return;

	dpy = (Display *)xv_get(pan, XV_DISPLAY);
	xb = &xev->xbutton;

	xcl.type = ClientMessage;
	xcl.xclient.window = (Window)xv_get(xv_get(pan, XV_OWNER), XV_XID);
	xcl.xclient.message_type = (Atom)xv_get(XV_SERVER_FROM_WINDOW(pan),
									SERVER_ATOM, "_OL_PROPAGATE_EVENT");
	xcl.xclient.format = 32;

	xcl.xclient.data.l[0] =
				(((unsigned)xev->type & 0xff) << 24)
				| (((unsigned)xb->button & 0xff) << 16)
				| ((unsigned)xb->state & 0xffff)
				;

	frame_x = (unsigned)(xb->x + (int)xv_get(pan, XV_X));
	frame_y = (unsigned)(xb->y + (int)xv_get(pan, XV_Y));

	xcl.xclient.data.l[1] = (frame_x << 16) | frame_y;
	xcl.xclient.data.l[2] = xb->time;
	xcl.xclient.data.l[3] = 0;
	xcl.xclient.data.l[4] = 0;

	if (xev->type == ButtonPress) {
		/* otherwise the wm will not be able to grab... */
		XUngrabPointer(dpy, xb->time);
	}

	XSendEvent(dpy, (Window)xv_get(xv_get(pan, XV_ROOT), XV_XID),
			FALSE, SubstructureRedirectMask | SubstructureNotifyMask,
			&xcl);
}

Pkg_private Xv_opaque frame_cmd_set_avlist(Frame frame_public, Attr_attribute avlist[])
{
	Attr_avlist attrs;
	Frame_cmd_info *frame = FRAME_CMD_PRIVATE(frame_public);
	Xv_Drawable_info *info;
	Xv_opaque server_public;
	int result = XV_OK;
	int add_decor, delete_decor, set_win_attr;
	Atom add_decor_list[WM_MAX_DECOR], delete_decor_list[WM_MAX_DECOR];

	DRAWABLE_INFO_MACRO(frame_public, info);
	server_public = xv_server(info);
	set_win_attr = add_decor = delete_decor = 0;

	for (attrs = avlist; *attrs; attrs = attr_next(attrs)) switch (attrs[0]) {

		case FRAME_CMD_PUSHPIN_IN:

			/* This attribute is overloaded for 2 different functionalities:
			   the initial state of the pin, and the current state of the
			   pin.  As a consequence it supports neither functionality 
			   accurately.  It is here only for compatibility reasons. Instead,
			   Use FRAME_CMD_DEFAULT_PIN_STATE and FRAME_CMD_PIN_STATE */

			/* only change the state of the pin when the window is not map */
			attrs[0] = (Frame_attribute) ATTR_NOP(attrs[0]);
			if (status_get(frame, pushpin_in) == (int)attrs[1])
				break;

			if ((int)attrs[1])
				frame->win_attr.pin_initial_state = WMPushpinIsIn;
			else
				frame->win_attr.pin_initial_state = WMPushpinIsOut;
			status_set(frame, pushpin_in, (int)attrs[1]);
			set_win_attr = TRUE;
			break;

		case FRAME_CMD_DEFAULT_PIN_STATE:
			/* defines pin state when the frame_cmd becomes mapped */
			attrs[0] = (Frame_attribute) ATTR_NOP(attrs[0]);
			status_set(frame, default_pin_state, (int)attrs[1]);
			status_set(frame, default_pin_state_valid, (int)TRUE);
			if (!xv_get(frame_public, XV_SHOW))
				set_win_attr |=
						update_default_pin_state(frame, server_public);
			break;

		case FRAME_CMD_PANEL_BORDERED:
			attrs[0] = (Frame_attribute) ATTR_NOP(attrs[0]);
			frame->panel_bordered = (int)attrs[1];
			break;

		case FRAME_CMD_PIN_STATE:
			/* changes the current pin state.  Useless when unmapped */

			attrs[0] = (Frame_attribute) ATTR_NOP(attrs[0]);
			if ((int)attrs[1])
				frame->win_attr.pin_initial_state = WMPushpinIsIn;
			else
				frame->win_attr.pin_initial_state = WMPushpinIsOut;

			status_set(frame, pushpin_in, (int)attrs[1]);
			set_win_attr = TRUE;
			break;

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

		case FRAME_SCALE_STATE:
			attrs[0] = (Frame_attribute) ATTR_NOP(attrs[0]);
			/*
			 * set the local rescale state bit, then tell the WM the current
			 * state, and then set the scale of our subwindows
			 */
			/*
			 * WAIT FOR NAYEEM window_set_rescale_state(frame_public,
			 * attrs[1]);
			 */
			wmgr_set_rescale_state(frame_public, (int)attrs[1]);
			frame_rescale_subwindows(frame_public, (int)attrs[1]);
			break;

		case XV_LABEL:
			{

#ifdef OW_I18N
				extern wchar_t *xv_app_name_wcs;
#endif

				extern char *xv_app_name;
				Frame_class_info *frame_class = FRAME_CLASS_FROM_CMD(frame);

				*attrs = (Frame_attribute) ATTR_NOP(*attrs);

#ifdef OW_I18N
				if ((char *)attrs[1]) {
					_xv_set_mbs_attr_dup(&frame_class->label,
							(char *)attrs[1]);
				}
				else {
					_xv_set_wcs_attr_dup(&frame_class->label,
							xv_app_name_wcs);
				}
#else
				if (frame_class->label) {
					free(frame_class->label);
				}
				if ((char *)attrs[1]) {
					frame_class->label = (char *)calloc(1L,
							strlen((char *)attrs[1]) + 1);
					strcpy(frame_class->label, (char *)attrs[1]);
				}
				else {
					if (xv_app_name) {
						frame_class->label = (char *)calloc(1L,
								strlen(xv_app_name) + 1);
						strcpy(frame_class->label, xv_app_name);
					}
					else {
						frame_class->label = NULL;
					}
				}
#endif

				(void)frame_display_label(frame_class);
				break;

#ifdef OW_I18N
		case XV_LABEL_WCS:
				{
					extern wchar_t *xv_app_name_wcs;
					Frame_class_info *frame_class =
							FRAME_CLASS_FROM_CMD(frame);

					*attrs = (Frame_attribute) ATTR_NOP(*attrs);
					_xv_set_wcs_attr_dup(&frame_class->label,
							(wchar_t *)attrs[1]
							? (wchar_t *)attrs[1]
							: xv_app_name_wcs);
					(void)frame_display_label(frame_class);
				}
				break;
#endif

		case XV_SHOW:
				{
					Frame_class_info *frame_class =
							FRAME_CLASS_FROM_CMD(frame);


					/* ignore this if we are mapping the window */
					if (!(int)attrs[1]) {
						/*
						 * ignore this if we are in the midst of dismissing
						 * the window
						 */
						if (status_get(frame_class, dismiss)) {
							status_set(frame_class, dismiss, FALSE);
							set_win_attr |= update_default_pin_state(frame,
									server_public);
							break;
						}

						/* don't unmap the frame if the pushpin is in */

						if (status_get(frame, pushpin_in))
							attrs[0] = (Frame_attribute) ATTR_NOP(attrs[0]);
						else {
							set_win_attr |= update_default_pin_state(frame,
									server_public);
						}
					}
					else {
						unsigned long data[6];
						Rect *rect;
						Panel_item default_panel_item;
						int button_x, button_y;

						/* This deals with the problem where someone does an
						 * XV_SHOW TRUE in the xv_create call.  In this
						 * case the panel has not been created yet, so we do


						 * so here.
						 */
						if (!frame->panel) {
							char *framename, panelname[200];

							framename = (char *)xv_get(frame_public,
									XV_INSTANCE_NAME);
							sprintf(panelname, "%sCmdPanel",
									framename ? framename : "FrameCmd");
							if (frame->panel_bordered) {
								frame->panel =
										xv_create(frame_public, PANEL,
											XV_INSTANCE_NAME, panelname,
											PANEL_BORDER, TRUE,
											XV_USE_DB,
												PANEL_ITEM_X_GAP, 8,
												PANEL_ITEM_Y_GAP, 13,
												NULL,
											NULL);
							}
							else {
								frame->panel =
										xv_create(frame_public, PANEL,
											XV_INSTANCE_NAME, panelname,
											PANEL_BACKGROUND_PROC, xv_ol_default_background,
											XV_USE_DB,
												PANEL_ITEM_X_GAP, 8,
												PANEL_ITEM_Y_GAP, 13,
												NULL,
											NULL);
							}
						}

#ifdef OW_I18N
						if (status_get(frame, warp_pointer) == TRUE) {
#endif

							default_panel_item =
									(Panel_item) xv_get(frame->panel,
									PANEL_DEFAULT_ITEM);
							if (!default_panel_item)
								break;
							rect = (Rect *) xv_get(default_panel_item,
									PANEL_ITEM_RECT);

							win_translate_xy(frame->panel, frame_public,
									rect->r_left, rect->r_top, &button_x,
									&button_y);
							data[0] = button_x + rect->r_width / 2;
							data[1] = button_y + rect->r_height / 2;
							data[2] = button_x;
							data[3] = button_y;
							data[4] = rect->r_width;
							data[5] = rect->r_height;

							win_change_property(frame_public,
									(long)SERVER_WM_DEFAULT_BUTTON,
									XA_INTEGER, 32, (unsigned char *)data,
									6);

#ifdef OW_I18N
						}
#endif
					}
					break;
				}

#ifdef OW_I18N
		case FRAME_CMD_POINTER_WARP:
				*attrs = (Frame_attribute) ATTR_NOP(*attrs);
				status_set(frame, warp_pointer, attrs[1]);
				break;
#endif

		case XV_END_CREATE:
				if (!frame->panel) {
					char *framename, panelname[200];

					framename =
							(char *)xv_get(frame_public,
							XV_INSTANCE_NAME);
					sprintf(panelname, "%sCmdPanel",
							framename ? framename : "FrameCmd");
					if (frame->panel_bordered) {
						frame->panel = xv_create(frame_public, PANEL,
								XV_INSTANCE_NAME, panelname,
								PANEL_BORDER, TRUE,
								XV_USE_DB,
									PANEL_ITEM_X_GAP, 8L,
									PANEL_ITEM_Y_GAP, 13L,
									NULL,
								NULL);
					}
					else {
						frame->panel = xv_create(frame_public, PANEL,
								XV_INSTANCE_NAME, panelname,
								PANEL_BACKGROUND_PROC, xv_ol_default_background,
								XV_USE_DB,
									PANEL_ITEM_X_GAP, 8L,
									PANEL_ITEM_Y_GAP, 13L,
									NULL,
								NULL);
					}
				}
				frame->win_attr.win_type = (Atom)xv_get(frame_public,
															FRAME_WINTYPE);
				set_win_attr = TRUE;
				break;

		default:
				break;

			}
	}

	if (set_win_attr)
		(void)wmgr_set_win_attr(frame_public, &(frame->win_attr));

	/* recompute wmgr decorations */

	if (add_decor || delete_decor) {
		add_decor = delete_decor = 0;

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

	return (Xv_opaque) result;
}

static int update_default_pin_state(Frame_cmd_info *frame, Xv_opaque server_public)
{
	int retval = FALSE;

	if (status_get(frame, default_pin_state_valid)) {
		retval = TRUE;
		if (status_get(frame, default_pin_state))
			frame->win_attr.pin_initial_state = WMPushpinIsIn;
		else
			frame->win_attr.pin_initial_state = WMPushpinIsOut;
	}
	return retval;
}

typedef struct {
	xv_soon_proc_t proc;
	Xv_opaque cldt;
} soon_t;

static Notify_value do_soon(Notify_client wpipptr, int fd)
{
	soon_t soon;
	int len;

	len = read(fd, (char *)&soon, sizeof(soon_t));
	if (len < 0) {
		if (errno == EINTR) return do_soon(wpipptr, fd);
	}
	if (len < (int)sizeof(soon_t)) {
		perror("read from 'soon pipe'");
		return NOTIFY_DONE;
	}

	if (soon.proc) (*soon.proc)(soon.cldt);
	return NOTIFY_DONE;
}

Xv_public void xv_perform_soon(Xv_opaque cldt, xv_soon_proc_t func)
{
	soon_t soon;
	static int wpip = -1;

	if (wpip < 0) {
		int pip[2];

		pipe(pip);
		wpip = pip[1];
		notify_set_input_func((Notify_client)&wpip, do_soon, pip[0]);
	}

	soon.proc = func;
	soon.cldt = cldt;
	write(wpip, (char *)&soon, sizeof(soon_t));
}

static Notify_value note_later(Notify_client xpwp, int unused)
{
	soon_t *soon = (soon_t *)xpwp;

	notify_set_itimer_func(xpwp, NOTIFY_TIMER_FUNC_NULL, ITIMER_REAL,
							(struct itimerval *)0, (struct itimerval *)0);
	if (soon->proc) (*soon->proc)(soon->cldt);
	xv_free(soon);
	return NOTIFY_DONE;
}

Xv_public void xv_perform_later(Xv_opaque cldt, xv_soon_proc_t func, int usec)
{
	struct itimerval timer;
	soon_t *soon = xv_malloc(sizeof(soon_t));

	timer.it_value.tv_sec = 0;
	timer.it_value.tv_usec = usec;
	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = 0;
	soon->proc = func;
	soon->cldt = cldt;
	notify_set_itimer_func((Notify_client)soon, note_later, ITIMER_REAL,
								&timer, (struct itimerval *) 0);
}

/**********************************************************/
/**           (command) frame resizing                   **/
/**********************************************************/

static Attr_attribute fl_key = 0;
static Attr_attribute below_key;
static Attr_attribute rwid_key = 0;
static Attr_attribute right_dist_key;

typedef struct _scroll_t {
	/* needed only during startup */
	struct _scroll_t *next;
	Frame frame;

	/* needed always */
	Panel_item scroller, highest_below;
	int rest_height;
	int frame_width, frame_height;

	/* callback */
	xv_frame_layout_cb_t callback;
	void *client_data;
} scroll_t;

static scroll_t *all_scrolls = (scroll_t *)0;
static int interpose_immediate = FALSE;

static Notify_value resize_interposer(Frame fram, Notify_event event,
								Notify_arg arg, Notify_event_type type)
{
	Event *ev = (Event *)event;

	Notify_value val = notify_next_event_func(fram, event, arg,type);

	if (event_action(ev) == WIN_RESIZE && event_xevent(ev) &&
		! event_xevent(ev)->xany.send_event)
	{
		scroll_t *scr = (scroll_t *)xv_get(fram, XV_KEY_DATA, fl_key);
		/* OLD:  pan = xv_get(fram, FRAME_CMD_PANEL)
		 * but fram does **not** have to be a FRAME_CMD...
		 */
		Panel pan = xv_get(fram, FRAME_NTH_SUBWINDOW, 1);
		Panel_item item;
		int panhig = (int)xv_get(pan, XV_HEIGHT);
		int rowhig;
		int scrbot, rows;
		Attr_attribute row_attr;
		int newwid, newhig;

		if (! scr) return val;

		newwid = (int)xv_get(fram, XV_WIDTH);
		newhig = (int)xv_get(fram, XV_HEIGHT);
		if (scr->frame_width == newwid && scr->frame_height == newhig)
			return val;

		scr->frame_width = newwid;
		scr->frame_height = newhig;

		PANEL_EACH_ITEM(pan, item)
			if (xv_get(item, XV_KEY_DATA, rwid_key)) {
				int right_dist = (int)xv_get(item, XV_KEY_DATA, right_dist_key);

				if (xv_get(item, XV_IS_SUBTYPE_OF, PANEL_LIST)) {
					xv_set(item, PANEL_LIST_WIDTH, newwid - right_dist, NULL);
				}
				else if (xv_get(item, XV_IS_SUBTYPE_OF, PANEL_TEXT)) {
					xv_set(item,
							PANEL_VALUE_DISPLAY_WIDTH, newwid - right_dist,
							NULL);
				}
				else {
					Rect r;

					r = *((Rect *)xv_get(item, PANEL_ITEM_VALUE_RECT));
					r.r_width = newwid - right_dist - r.r_left;

					xv_set(item, PANEL_ITEM_VALUE_RECT, &r, NULL);
				}
			}

			if (item != scr->scroller && xv_get(item, XV_KEY_DATA, below_key)) {
				int yhigoff = (int)xv_get(item, XV_KEY_DATA, fl_key);

				if (yhigoff) xv_set(item, XV_Y, panhig - yhigoff, NULL);
			}
		PANEL_END_EACH

		if (xv_get(scr->scroller, XV_IS_SUBTYPE_OF, PANEL_LIST)) {
			rowhig = 2 + (int)xv_get(scr->scroller, PANEL_LIST_ROW_HEIGHT);
			row_attr = (Attr_attribute)PANEL_LIST_DISPLAY_ROWS;
		}
		else if (xv_get(scr->scroller,XV_IS_SUBTYPE_OF,PANEL_MULTILINE_TEXT)) {
			Xv_font font;
			Font_string_dims dims;

			font = xv_get(pan, XV_FONT);
			xv_get(font, FONT_STRING_DIMS, "W", &dims);
			rowhig = dims.height;
			row_attr = (Attr_attribute)PANEL_DISPLAY_ROWS;
		}
		else if (xv_get(scr->scroller, XV_IS_SUBTYPE_OF, PANEL_SUBWINDOW)) {
			rowhig = 1;
			row_attr = (Attr_attribute)PANEL_WINDOW_HEIGHT;
		}
		else {
			rowhig = 1;
			row_attr = (Attr_attribute)XV_HEIGHT;
		}

		rows = (panhig-scr->rest_height) / rowhig;
		xv_set(scr->scroller, row_attr, rows, NULL);

		scrbot = (int)xv_get(scr->scroller, XV_Y) +
					(int)xv_get(scr->scroller, XV_HEIGHT);
			
		if (scr->highest_below) {
			if (scrbot + 10 > (int)xv_get(scr->highest_below, XV_Y)) {
				xv_set(scr->scroller, row_attr, rows - 1, NULL);
			}
		}

		if (scr->callback) (*(scr->callback))(fram, scr->client_data);

		panel_paint(pan, PANEL_CLEAR);
	}

	return val;
}

static void interpose_me(Xv_opaque cldt)
{
	scroll_t *p;

	if (interpose_immediate) return;
	interpose_immediate = TRUE;
	for (p = all_scrolls; p; p = p->next) {
		notify_interpose_event_func(p->frame, resize_interposer, NOTIFY_SAFE);
	}
}

Attr_attribute xv_get_rwid_key(void)
{
	if (! rwid_key) {
		rwid_key = xv_unique_key();
		right_dist_key = xv_unique_key();
	}
	return rwid_key;
}

static void free_key_data(Xv_opaque obj, int key, char *data)
{
	if (key == fl_key) xv_free(data);
}

Xv_public void xv_set_frame_resizing(Frame frame, int resize_width,
								xv_frame_layout_cb_t cb, void *cldt)
{
	/* OLD:  pan = xv_get(frame, FRAME_CMD_PANEL)
	 * but fram does **not** have to be a FRAME_CMD...
	 */
	Panel pan = xv_get(frame, FRAME_NTH_SUBWINDOW, 1);
	Panel_item item;
	int fwid, miny, y, panheight;
	int scrolltop, scrollbot;
	scroll_t *new;
    Graphics_info  *info;
	int min_scroller;
	int screenwidth;
	int screenheight;

	if (! fl_key) {
		fl_key = xv_unique_key();
		below_key = xv_unique_key();
	}

	if (! rwid_key) {
		rwid_key = xv_unique_key();
		right_dist_key = xv_unique_key();
	}

	new = (scroll_t *)xv_alloc(scroll_t);

	new->callback = cb;
	new->client_data = cldt;

	new->frame_width = (int)xv_get(frame, XV_WIDTH);
	new->frame_height = (int)xv_get(frame, XV_HEIGHT);

	panheight = (int)xv_get(pan, XV_HEIGHT);

	if (panheight >= new->frame_height) {
		/* strange things happen here sometimes */
		panheight = new->frame_height;
	}

	/* first find the **LAST** scroller (list or multi line text) */
	PANEL_EACH_ITEM(pan, item)
		if (xv_get(item, XV_IS_SUBTYPE_OF, PANEL_LIST) ||
			xv_get(item, XV_IS_SUBTYPE_OF, PANEL_SUBWINDOW) ||
			xv_get(item, XV_IS_SUBTYPE_OF, PANEL_MULTILINE_TEXT))
		{
			new->scroller = item;
			new->rest_height = panheight - (int)xv_get(item, XV_HEIGHT);
		}
	PANEL_END_EACH

	if (! new->scroller) {
		xv_free((char *)new);
		return;
	}

	scrolltop = (int)xv_get(new->scroller, XV_Y);
	scrollbot = scrolltop + (int)xv_get(new->scroller, XV_HEIGHT);

	miny = 10000;
	PANEL_EACH_ITEM(pan, item)
		if (xv_get(item, XV_KEY_DATA, rwid_key)) {
			Rect r;
			int right_dist;

			if (xv_get(item, XV_IS_SUBTYPE_OF, PANEL_LIST)) {
				right_dist = new->frame_width - 
								(int)xv_get(item, PANEL_LIST_WIDTH);
			}
			else if (xv_get(item, XV_IS_SUBTYPE_OF, PANEL_TEXT)) {
				right_dist = new->frame_width - 
								(int)xv_get(item, PANEL_VALUE_DISPLAY_WIDTH);
			}
			else {
				r = *((Rect *)xv_get(item, PANEL_ITEM_VALUE_RECT));
				right_dist = new->frame_width - r.r_left - r.r_width;
			}
			xv_set(item, XV_KEY_DATA, right_dist_key, right_dist, NULL);
		}

		if (item != new->scroller) {
			y = (int)xv_get(item, XV_Y);
			if (y >= scrollbot) {
				xv_set(item,
						XV_KEY_DATA, below_key, TRUE,
						XV_KEY_DATA, fl_key, panheight - y,
						NULL);

				if (y < miny) {
					miny = y;
					new->highest_below = item;
				}
			}
		}
	PANEL_END_EACH

	fwid = (int)xv_get(frame, XV_WIDTH);

	xv_set(pan,
			XV_WIDTH, WIN_EXTEND_TO_EDGE,
			XV_HEIGHT, WIN_EXTEND_TO_EDGE,
			NULL);

    info = (Graphics_info *)xv_get(pan, PANEL_GINFO);
	min_scroller = (ScrollbarElevator_Height(info)) +
					2 * Vertsb_Endbox_Height(info);

	screenheight = (int)xv_get(xv_get(frame, XV_ROOT), XV_HEIGHT);
	screenwidth = (int)xv_get(xv_get(frame, XV_ROOT), XV_WIDTH);

	if (resize_width) {
		xv_set(frame,
				XV_KEY_DATA, fl_key, new,
				XV_KEY_DATA_REMOVE_PROC, fl_key, free_key_data,
				/* we rather leave this to the application: */
/* 				FRAME_MIN_SIZE, fwid, new->rest_height + min_scroller, */
				FRAME_MAX_SIZE, screenwidth - 6, screenheight - 31,
				FRAME_SHOW_RESIZE_CORNER, TRUE,
				NULL);
	}
	else {
		xv_set(frame,
				XV_KEY_DATA, fl_key, new,
				XV_KEY_DATA_REMOVE_PROC, fl_key, free_key_data,
				FRAME_MIN_SIZE, fwid, new->rest_height + min_scroller,
				FRAME_MAX_SIZE, fwid, screenheight - 31,
				FRAME_SHOW_RESIZE_CORNER, TRUE,
				NULL);
	}

	if (interpose_immediate) {
		notify_interpose_event_func(frame, resize_interposer, NOTIFY_SAFE);
	}
	else {
		if (! all_scrolls) {
			xv_perform_soon(XV_NULL, interpose_me);
		}
		new->frame = frame;
		new->next = all_scrolls;
		all_scrolls = new;
	}
}

void xv_activate_resizing(void)
{
	interpose_me(XV_NULL);
}
