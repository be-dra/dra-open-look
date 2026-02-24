#include <unistd.h>
#include <ctype.h>
#include <pwd.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <xview/xview.h>
#include <xview/panel.h>
#include <xview/cms.h>
#include <xview/help.h>
#include <xview/defaults.h>
#include <xview/permprop.h>
#include <xview/win_notify.h>
#include <X11/Xresource.h>
#include <X11/Xatom.h>
#include <xview_private/svr_impl.h>

char xvwp_c_sccsid[] = "@(#) %M% V%I% %E% %U% $Id: svr_xvwp.c,v 4.25 2026/02/23 18:07:49 dra Exp $";

#define RESCALE_WORKS 0
#define MIN_DEPTH 4

#define DEBUG 0

#define W_ATOM(f,s) (Atom)xv_get(XV_SERVER_FROM_WINDOW(f), SERVER_ATOM, s)

#define FNRES "openwindows.regularfont"
#define FNCLASS "OpenWindows.RegularFont"

/* for delayed_action */
#define BIT_POS 1
#define BIT_SIZE (1<<1)
#define BIT_PIN (1<<2)
#define BIT_SHOW (1<<3)

#define QUEUE_REQUESTS XV_KEY_DATA,SERVER_UI_REGISTRATION_PROC

typedef struct _xvwp_popup {
	char *name;
	int x, y, show, pushpin_in;
	int width, height;
	int win_menu_default;
	int is_incomplete;
	int delayed_action;
	struct _xvwp_popup *next;
	Frame frame;
} *xvwp_popup_t;

typedef struct _xvwp_menu {
	char *name;
	int default_index;
	unsigned long selected;
	int is_pinned, pin_x, pin_y;
	struct _xvwp_menu *next;
	Menu menu;
} *xvwp_menu_t;
#define SEL_UNINIT 0xffffffff

typedef struct {
	int indx;
} color_t;

typedef enum {
	WM_UNKNOWN, WM_OLWM, WM_MWM, WM_OTHER
} wm_type_t;

typedef struct {
	color_t back, fore;
	int closed;
	int measure_in;
	int init_icon_loc_set;
	int icon_x;
	int icon_y;
	int init_loc_set;
	int x;
	int y;
	int width;
	int height;
	int popup_relative;
	int avoid_query_tree_count;
	int manage_wins;
	int base_scale;
	int use_base_scale; /* == base_scale except WIN_SCALE_DEFAULT */
	int popup_scale;
	int use_popup_scale; /* == popup_scale except WIN_SCALE_DEFAULT */
	int win_menu_default;

	int record_popups;
	int record_menus;
	Frame base;
	xvwp_popup_t popups;
	xvwp_menu_t menus;
} xvwp_data_t, *xvwp_data_pt;

#define OFF(field) FP_OFF(xvwp_data_pt,field)

#define _OL_OWN_HELP "_OL_OWN_HELP"

static int xvwp_key = 0;
static int xvwp_popup_key = 0;

extern char *xv_instance_app_name;

typedef struct {
	char scale, fg, pos, size, icon_pos, iconic;
} xvwp_cmdline_t;

typedef struct _xvwp {
	Frame propwin;
	char *appfiles[PERM_NUM_CATS];
	xvwp_data_t data;
	xvwp_cmdline_t argv_contains;
	Xv_opaque xvwp_xrmdb[PERM_NUM_CATS];
	char xvwp_prefix[200]; /* "appname.base." */
	Panel_item p_pos_def_spec, p_initwid, p_inithig, p_x_pos, p_y_pos;
	Panel_item p_pos_icon_def_spec, p_icon_x_pos, p_icon_y_pos;
	char must_handle_delayed_popup, never_again_handle_delayed, installed;
	int cur_measure;
	wm_type_t wm;
	Xv_font font_for_root_and_frame;
	int orig_base_scale; /* Zustand beim letzten reset */
	int orig_popup_scale; /* Zustand beim letzten reset */
	xvwp_popup_t secondaries;
} xvwp_t;

typedef void (*menu_cb_t)(Menu, Menu_item);
typedef Menu (*menu_gen_proc_t)(Menu, Menu_generate);
typedef void (*pin_proc_t)(Menu, int, int);
typedef int (*measfunc)(Xv_server, int);

static Xv_singlecolor *my_colors = 0;
static int fore_start;
static int total_num_colors;

static void conv_win_colors(Display *dpy, Colormap cm,
					char *datptr, Xv_singlecolor **scp, int *numcols)
{
	char *tmp, *p;
	XColor color;
	int i;
	unsigned estimated_num_colors;
	Xv_singlecolor *mycol;

	if (! datptr) {
		*scp = (Xv_singlecolor *)xv_calloc(1, (unsigned)sizeof(Xv_singlecolor));
		*numcols = 1;
	}

	tmp = xv_strsave(datptr);

	estimated_num_colors = strlen(datptr) / 3 + 4; /* be VERY careful */
	mycol = (Xv_singlecolor *)xv_calloc(estimated_num_colors,
										(unsigned)sizeof(Xv_singlecolor));

	/* we start with 1, because at 0 will be the default window color */
	i = 1;

	for (p = strtok(tmp, " ,\n"); p; p = strtok(0, " ,\n")) {

		XParseColor(dpy, cm, p, &color);
		mycol[i].red = color.red >> 8;
		mycol[i].green = color.green >> 8;
		mycol[i].blue = color.blue >> 8;
/* 		DTRACE(DTL_INTERN, "%2d: %3d %3d %3d\n", i, */
/* 								mycol[i].red, mycol[i].green, mycol[i].blue); */

		i++;
	}

	xv_free(tmp);
	*scp = mycol;
	*numcols = i;
}

/*******************************************************************\
 *          File handling and data conversion                      *
\*******************************************************************/

#define WIN_SCALE_DEFAULT ((int)WIN_SCALE_EXTRALARGE + 1)

static Defaults_pairs scale_enum[] = {
	{ "small", WIN_SCALE_SMALL },
	{ "medium", WIN_SCALE_MEDIUM },
	{ "large", WIN_SCALE_LARGE },
	{ "extra_large", WIN_SCALE_EXTRALARGE },
	{ "default", WIN_SCALE_DEFAULT },
	{ (char *)0, WIN_SCALE_DEFAULT }
};

static Permprop_res_res_t res_base[] = {
	{ "xvwp.back_color.index", PRC_B, DAP_int, OFF(back.indx), 0 },
	{ "xvwp.fore_color.index", PRC_B, DAP_int, OFF(fore.indx), 0 },
	{ "frame_closed", PRC_U, DAP_bool, OFF(closed), FALSE },
	{ "xvwp.measure_in", PRC_U, DAP_int, OFF(measure_in), 0 },
	{ "xvwp.loc_set", PRC_D, DAP_bool, OFF(init_loc_set), FALSE },
	{ "xvwp.icon_loc_set", PRC_D, DAP_bool, OFF(init_icon_loc_set), FALSE },
	{ "xvwp.icon_x", PRC_D, DAP_int, OFF(icon_x), 0 },
	{ "xvwp.icon_y", PRC_D, DAP_int, OFF(icon_y), 0 },
	{ "x", PRC_D, DAP_int, OFF(x), 0 },
	{ "y", PRC_D, DAP_int, OFF(y), 0 },
	{ "xvwp.popup_relative",PRC_U, DAP_int,OFF(popup_relative),0 },
	{ "xvwp.manage_windows", PRC_U, DAP_int, OFF(manage_wins), 0 },
	{ "xvwp.base_scale", PRC_D, DAP_enum, OFF(base_scale), (Ppmt)scale_enum },
	{ "xvwp.popup_scale", PRC_D, DAP_enum, OFF(popup_scale), (Ppmt)scale_enum },

	/* must be before the last, see ref (chjbvhjsdfgv) */
	{ "avoidQueryTreeCount",PRC_U, DAP_int,OFF(avoid_query_tree_count), 16 },

	/* special handling: must be last, see ref (drghjbvewrfvdfgh) */
	{ "xvwp.window_menu.default", PRC_D, DAP_int, OFF(win_menu_default), 0 }
	/* das hier nicht auf 2 setzen, Index 2 ist
	 * der 'moveButton' -  siehe windowMenuFullButtons in usermenu.c 
	 */
};

static Permprop_res_res_t res_rc[] = {
	{ "win_columns", PRC_D, DAP_int, OFF(width), 82 },
	{ "win_rows", PRC_D, DAP_int, OFF(height), 34 }
};
static Permprop_res_res_t res_nonrc[] = {
	{ "xv_width", PRC_D, DAP_int, OFF(width), 300 },
	{ "xv_height", PRC_D, DAP_int, OFF(height), 300 }
};

static Permprop_res_res_t res_popup[] = {
	{ "xv_x", PRC_D, DAP_int, PERM_OFF(xvwp_popup_t,x), 0 },
	{ "xv_y", PRC_D, DAP_int, PERM_OFF(xvwp_popup_t,y), 0 },
	{ "xv_show", PRC_U, DAP_bool, PERM_OFF(xvwp_popup_t,show), FALSE },
	{ "frame_cmd_pushpin_in", PRC_U, DAP_bool, PERM_OFF(xvwp_popup_t,pushpin_in),
									FALSE },
	{ "xv_width", PRC_D, DAP_int, PERM_OFF(xvwp_popup_t,width), 0 },
	{ "xv_height", PRC_D, DAP_int, PERM_OFF(xvwp_popup_t,height), 0 }
};

static Permprop_res_res_t res_popup_menu[] = {
	{ "window_menu.default", PRC_D, DAP_int,
				PERM_OFF(xvwp_popup_t,win_menu_default), 0 }
	/* das hier nicht auf 2 setzen, Index 2 ist
	 * der 'moveButton' -  siehe windowMenuFullButtons in usermenu.c 
	 */
};

static Permprop_res_res_t res_menu[] = {
	{ "menu_default_index", PRC_U, DAP_int, PERM_OFF(xvwp_menu_t,default_index), -1},
	{ "is_pinned", PRC_U, DAP_bool, PERM_OFF(xvwp_menu_t,is_pinned), FALSE },
	{ "pin_x", PRC_D, DAP_int, PERM_OFF(xvwp_menu_t,pin_x), 0 },
	{ "pin_y", PRC_D, DAP_int, PERM_OFF(xvwp_menu_t,pin_y), 0 },
	{ "m_selected", PRC_U, DAP_int, PERM_OFF(xvwp_menu_t,selected), SEL_UNINIT }
};

/*******************************************************************\
 *          Unit conversions                                       *
\*******************************************************************/

static int pix_to_mm(Xv_server srv, int pix)
{
	Display *dpy = (Display *)xv_get(srv, XV_DISPLAY);
	Screen *s = DefaultScreenOfDisplay(dpy);

	return (pix * WidthMMOfScreen(s)) / WidthOfScreen(s);
}

static int mm_to_pix(Xv_server srv, int mm)
{
	Display *dpy = (Display *)xv_get(srv, XV_DISPLAY);
	Screen *s = DefaultScreenOfDisplay(dpy);

	return (mm * WidthOfScreen(s)) / WidthMMOfScreen(s);
}

static int identity(Xv_server unused_inst, int val)
{
	return val;
}

static struct { measfunc pixel_to_unit, unit_to_pixel; } measures[] = {
	{ identity, identity }, /* pixels or font units */
	{ pix_to_mm, mm_to_pix } /* millimeters */
};

/*******************************************************************\
 *          Event handling, callbacks                              *
\*******************************************************************/

static char *colorname(int val, int start)
{
	static char buf[30];

	if (start != 0) { /* foreground */
		if (val >= total_num_colors) val = 0;
	}
	else {
		if (val >= fore_start) val = 0;
	}

	sprintf(buf, "#%02x%02x%02x", 
				my_colors[val+start].red,
				my_colors[val+start].green,
				my_colors[val+start].blue);

	return buf;
}

static void keep_on_screen(Frame someframe, Rect *r)
{
	Rect *rootrect;

	rootrect = (Rect *)xv_get(xv_get(someframe, XV_ROOT), XV_RECT);

	/* popup might be off the screen */

	if (rect_right(r) > rect_right(rootrect))
		r->r_left -= rect_right(r) - rect_right(rootrect);

	if (rect_bottom(r) > rect_bottom(rootrect))
		r->r_top -= rect_bottom(r) - rect_bottom(rootrect);

	if (r->r_top < 0) r->r_top = 0;
	if (r->r_left < 0) r->r_left = 0;
}

static void popup_propwin(xvwp_t *inst, Frame fram)
{
	Rect pr_rect, b_rect;

	frame_get_rect(fram, &b_rect);

	if (! inst->p_pos_def_spec) {
		/* not yet filled */
		xv_set(inst->propwin, XV_SHOW, XV_AUTO_CREATE, NULL);
	}

	frame_get_rect(inst->propwin, &pr_rect);
	pr_rect.r_left = 30 + b_rect.r_left;
	pr_rect.r_top = 30 + b_rect.r_top;
	keep_on_screen(fram, &pr_rect);

	xv_set(inst->propwin,
			XV_X, pr_rect.r_left,
			XV_Y, pr_rect.r_top,
			XV_SHOW, TRUE,
			NULL);
}

static void configure_popup(xvwp_popup_t p, int base_x, int base_y)
{
	if ((p->delayed_action & BIT_SIZE) &&
		xv_get(p->frame, FRAME_SHOW_RESIZE_CORNER) &&
		p->width > 0 &&
		p->height > 0)
	{
		xv_set(p->frame,
				XV_WIDTH, p->width,
				XV_HEIGHT, p->height,
				NULL);
	}

	if (p->delayed_action & BIT_POS) {
		Rect r;

		/* hier kommt etwas heraus, was (noch) nicht die XView-Attribute
		 * wiederspiegelt - vielleicht hilft das hier:
		 */
		xv_set(XV_SERVER_FROM_WINDOW(p->frame), 
				SERVER_SYNC_AND_PROCESS_EVENTS,
				NULL);
		/* das hat schon MANCHMAL geholfen, aber nicht verlaesslich */

#ifdef DAS_WAR_MIR_ZU_GEFAEHRLICH
		frame_get_rect(p->frame, &r);
		/* Forschung (dgtrfbedrgtjklgy) */
		DTRACE(DTL_LAYOUT, "%x: frame_get_rect:               [%d,%d]\n",
				p->frame, r.r_width, r.r_height);
#endif

		r.r_left = p->x + base_x;
		r.r_top = p->y + base_y;

		r.r_width = (int)xv_get(p->frame, XV_WIDTH);
		r.r_height = (int)xv_get(p->frame, XV_HEIGHT);

		keep_on_screen(p->frame, &r);

#ifdef DAS_WAR_MIR_ZU_GEFAEHRLICH
		/* Forschung (dgtrfbedrgtjklgy) */
		frame_set_rect(p->frame, &r);
#endif
		xv_set(p->frame,
				XV_X, r.r_left,
				XV_Y, r.r_top,
				NULL);
	}

	if (xv_get(p->frame, XV_IS_SUBTYPE_OF, FRAME_CMD) &&
		(p->delayed_action & BIT_PIN))
	{
		xv_set(p->frame, FRAME_CMD_PIN_STATE, p->pushpin_in, NULL);
	}
}

static void configure_menu(xvwp_menu_t p, Xv_window win, int base_x, int base_y)
{
	menu_gen_proc_t genproc;
	int start, i, numitems;
	unsigned long mask = 1;
	menu_cb_t cb;

	/* in case of a dynamic menu, let it be built */
	genproc = (menu_gen_proc_t)xv_get(p->menu, MENU_GEN_PROC);
	if (genproc) {
		(*genproc)(p->menu, MENU_DISPLAY);
		(*genproc)(p->menu, MENU_DISPLAY_DONE);
	}

	numitems = (int)xv_get(p->menu, MENU_NITEMS);
	if (numitems == 0) {
		/* nothing to do with an empty menu */
		return;
	}

	if (xv_get(p->menu, MENU_PIN)) start = 2;
	else if (xv_get(xv_get(p->menu, MENU_NTH_ITEM, 1), MENU_TITLE)) start = 2;
	else start = 1;

	if (start > numitems) start = numitems;
	if (p->default_index > numitems) p->default_index = numitems;

	xv_set(p->menu,
			MENU_DEFAULT, p->default_index == -1 ? start : p->default_index,
			NULL);

	if (xv_get(p->menu, XV_IS_SUBTYPE_OF, MENU_TOGGLE_MENU)) {

		for (i = 1; i <= numitems; i++) {
			Menu_item it = xv_get(p->menu, MENU_NTH_ITEM, i);

			if (it && i >= start) {
				xv_set(it, MENU_SELECTED,
						p->selected == SEL_UNINIT
						? FALSE
						: ((p->selected & mask) != 0),
						NULL);

				if ((cb = (menu_cb_t)xv_get(it, MENU_NOTIFY_PROC))) {
					(*cb)(p->menu, it);
				}
				else if ((cb = (menu_cb_t)xv_get(p->menu, MENU_NOTIFY_PROC))) {
					(*cb)(p->menu, it);
				}
			}
			mask <<= 1;
		}
	}
	else if (xv_get(p->menu, XV_IS_SUBTYPE_OF, MENU_CHOICE_MENU)) {
		unsigned long sel = p->selected == SEL_UNINIT ? (unsigned long)start : p->selected;
		Menu_item it = xv_get(p->menu, MENU_NTH_ITEM, sel);

		xv_set(p->menu, MENU_SELECTED, sel, NULL);
		if (it) {
			xv_set(it, MENU_SELECTED, TRUE, NULL);
			if ((cb = (menu_cb_t)xv_get(it, MENU_NOTIFY_PROC))) {
				(*cb)(p->menu, it);
			}
			else if ((cb = (menu_cb_t)xv_get(p->menu, MENU_NOTIFY_PROC))) {
				(*cb)(p->menu, it);
			}
		}
	}
	if (p->is_pinned && xv_get(p->menu, MENU_PIN)) {
		Rect r;
		pin_proc_t pinproc;

		r.r_left = p->pin_x + base_x;
		r.r_top = p->pin_y + base_y;
		r.r_width = r.r_height = 100;
		keep_on_screen(win, &r);

		pinproc = (pin_proc_t)xv_get(p->menu, MENU_PIN_PROC);

		if (pinproc) (*pinproc)(p->menu, r.r_left, r.r_top);
	}
}

static void handle_popups_delayed(Frame fram, xvwp_t *inst)
{
	xvwp_popup_t p = inst->data.popups;
	xvwp_menu_t m = inst->data.menus;
	int base_x = 0, base_y = 0;
	Rect r;

	inst->never_again_handle_delayed = TRUE;
	if (inst->data.popup_relative) {
		frame_get_rect(inst->data.base, &r);
		base_x = r.r_left;
		base_y = r.r_top;
	}

	xv_activate_resizing();

	while (p) {
		if (p->is_incomplete) {
			/* an incomplete frame that must be shown must be filled before */
			if ((p->delayed_action & BIT_SHOW) != 0 && p->show) {
				xv_set(p->frame, XV_SHOW, XV_AUTO_CREATE, NULL);
			}
		}
		else configure_popup(p, base_x, base_y);

		if (p->delayed_action & BIT_SHOW)
			xv_set(p->frame, XV_SHOW, p->show, NULL);

		p = p->next;
	}

	while (m) {
		configure_menu(m, fram, base_x, base_y);
		m = m->next;
	}
}

static wm_type_t determine_window_manager(Xv_server srv)
{
	Display *dpy = (Display *)xv_get(srv, XV_DISPLAY);
	Window root = DefaultRootWindow(dpy);
	Atom typeatom;
	int act_format;
	unsigned long nelem, rest;
	unsigned char *data;

	if (XGetWindowProperty(dpy, root, xv_get(srv, SERVER_ATOM,"_MOTIF_WM_INFO"),
					0L, 30L, False, xv_get(srv, SERVER_ATOM, "_MOTIF_WM_INFO"),
					&typeatom, &act_format, &nelem, &rest,
					&data) == Success)
	{
		XFree(data);
		if (typeatom == xv_get(srv, SERVER_ATOM, "_MOTIF_WM_INFO")
			&& act_format == 32)
		{
			return WM_MWM;
		}
	}

	if (XGetWindowProperty(dpy, root,
					xv_get(srv, SERVER_ATOM, "_SUN_WM_PROTOCOLS"),
					0L, 30L, False, XA_ATOM,
					&typeatom, &act_format, &nelem, &rest,
					&data) == Success)
	{
		XFree(data);

		if (typeatom == XA_ATOM && act_format == 32) {
			return WM_OLWM;
		}
	}

	return WM_OTHER;
}

Pkg_private int server_xvwp_is_own_help(Xv_server srv, Frame fr, Event *ev)
{
	XClientMessageEvent *xcl;

	if (event_action(ev) == WIN_CLIENT_MESSAGE &&
		(xcl = (XClientMessageEvent *)event_xevent(ev))) {
		if (xcl->message_type == xv_get(srv, SERVER_ATOM, "WM_PROTOCOLS")) {
			Atom ownHelp = xv_get(srv, SERVER_ATOM, _OL_OWN_HELP);
			if (xcl->data.l[0] == ownHelp) {
				Atom typeatom;
				int act_format;
				unsigned long nelem, rest;
				unsigned char *hlp;
				Event my_event;

				if (Success != XGetWindowProperty(xcl->display, xcl->window,
									ownHelp, 0L, 30L, TRUE, ownHelp, &typeatom,
									&act_format, &nelem, &rest, &hlp))
				{
					return FALSE;
				}
				if (typeatom != ownHelp || act_format != 8) {
					return FALSE;
				}
				event_init(&my_event);
				event_set_action(&my_event, ACTION_HELP);
				event_set_xevent(&my_event, event_xevent(ev));
				event_set_x(&my_event, (int)xcl->data.l[2]);
				event_set_y(&my_event, (int)xcl->data.l[3]);

				xv_help_show(fr, (char *)hlp, &my_event);
				XFree(hlp);
				return TRUE;
			}
		}
		else if (xcl->message_type == xv_get(srv,SERVER_ATOM, _OL_OWN_HELP)) {
			Event my_event;
			Xv_window rt = xv_get(fr, XV_ROOT);
			Window root, rr, wr;
			int rx, ry, wx, wy;
			unsigned mr;

			root = xv_get(rt, XV_XID);
			event_init(&my_event);
			event_set_action(&my_event, ACTION_HELP);
			event_set_xevent(&my_event, event_xevent(ev));
			XQueryPointer(xcl->display, root, &rr, &wr, &rx, &ry, &wx,&wy,&mr);
			event_set_x(&my_event, rx);
			event_set_y(&my_event, ry);

			xv_help_show(fr, xcl->data.b, &my_event);
			return TRUE;
		}
	}
	return FALSE;
}

static Notify_value base_interposer(Frame fram, Notify_event event,
				Notify_arg arg, Notify_event_type type)
{
	Event *ev = (Event *)event;
	XClientMessageEvent *xcl;
	xvwp_t *inst;

	if (event_action(ev) == WIN_CLIENT_MESSAGE &&
		(xcl = (XClientMessageEvent *)event_xevent(ev)))
	{
		Xv_server srv = XV_SERVER_FROM_WINDOW(fram);
		Server_info *srvpriv = SERVER_PRIVATE(srv);

		inst = srvpriv->xvwp;

		if (xcl->message_type == W_ATOM(fram, "WM_PROTOCOLS")) {
			if (xcl->data.l[0] == W_ATOM(fram, "_NET_WM_PING")) {
				xcl->window = xv_get(xv_get(fram, XV_ROOT), XV_XID);
				XSendEvent(xcl->display, xcl->window,
					FALSE, SubstructureRedirectMask | SubstructureNotifyMask,
					event_xevent(ev));
				return NOTIFY_DONE;
			}
			if (xcl->data.l[0] == xv_get(srv, SERVER_ATOM, "_OL_SHOW_PROPS")) {
				popup_propwin(inst, fram);
				return NOTIFY_DONE;
			}
			if (server_xvwp_is_own_help(srv, fram, ev)) return NOTIFY_DONE;
		}
		if (xcl->message_type == W_ATOM(fram, _OL_OWN_HELP)) {
			if (server_xvwp_is_own_help(srv, fram, ev)) return NOTIFY_DONE;
		}
		if (xcl->message_type == W_ATOM(fram, "_MOTIF_WM_MESSAGES") &&
				(Atom)xcl->data.l[0] == W_ATOM(fram, "_OL_SHOW_PROPS"))
		{
			popup_propwin(inst, fram);
			return NOTIFY_DONE;
		}
	}
	else if (event_action(ev)==WIN_REPAINT || event_action(ev)==ACTION_OPEN) {
		Xv_server srv = XV_SERVER_FROM_WINDOW(fram);
		Server_info *srvpriv = SERVER_PRIVATE(srv);

		inst = srvpriv->xvwp;

		if (!inst->never_again_handle_delayed &&
			inst->must_handle_delayed_popup)
		{
			/* we pop them up when the baseframe sees the first expose event */
			inst->must_handle_delayed_popup = FALSE;
			handle_popups_delayed(fram, inst);
		}
	}
	else if (event_action(ev) == ACTION_HELP && event_is_down(ev)) {
		XEvent *xev = event_xevent(ev);
		char *hlp, hlpbuf[100];
		Xv_server srv = XV_SERVER_FROM_WINDOW(fram);
		Server_info *srvpriv = SERVER_PRIVATE(srv);

		inst = srvpriv->xvwp;

		if (xev->xkey.window != (Window)xv_get(fram, XV_XID)) {
			/* help on the footer */
			hlp = (char *)xv_get(fram, XV_KEY_DATA, FRAME_FOOTER_HELP_KEY);
			if (! hlp) {
				hlp = (char *)xv_get(fram, XV_HELP_DATA);
				if (hlp) {
					sprintf(hlpbuf, "%s_footer", hlp);
					hlp = hlpbuf;
				}
				else {
					sprintf(hlpbuf, "%s:base_window_footer", 
						(char *)xv_get(srv, XV_APP_HELP_FILE));

					hlp = hlpbuf;
				}
			}
			else {
				if (hlp[0] == ':') {
					sprintf(hlpbuf, "%s%s", 
						(char *)xv_get(srv, XV_APP_HELP_FILE), hlp);

					hlp = hlpbuf;
				}
			}
			xv_help_show(fram, hlp, ev);
			return NOTIFY_DONE;
		}
		else {
			hlp = (char *)xv_get(fram, XV_HELP_DATA);
			if (! hlp) {
				sprintf(hlpbuf, "%s:base_window",
						(char *)xv_get(xv_default_server, XV_APP_HELP_FILE));
				hlp = hlpbuf;
			}
			xv_help_show(fram, hlp, ev);
			return NOTIFY_DONE;
		}
	}

	return notify_next_event_func(fram, event, arg, type);
}

static void update_measure(Xv_server srv, int new)
{
	char buf[20], *p;
	Server_info *srvpriv = SERVER_PRIVATE(srv);
	int old, pix;
	xvwp_t *inst;

	inst = srvpriv->xvwp;
	old = inst->cur_measure;
	inst->cur_measure = new;

	p = (char *)xv_get(inst->p_x_pos, PANEL_VALUE);
	if (p && *p) {
		pix = (*measures[old].unit_to_pixel)(srv, atoi(p));
		sprintf(buf, "%d", (*measures[new].pixel_to_unit)(srv, pix));
		xv_set(inst->p_x_pos, PANEL_VALUE, buf, NULL);
	}

	p = (char *)xv_get(inst->p_y_pos, PANEL_VALUE);
	if (p && *p) {
		pix = (*measures[old].unit_to_pixel)(srv, atoi(p));
		sprintf(buf, "%d", (*measures[new].pixel_to_unit)(srv, pix));
		xv_set(inst->p_y_pos, PANEL_VALUE, buf, NULL);
	}

	pix = (*measures[old].unit_to_pixel)(srv, (int)xv_get(inst->p_initwid, PANEL_VALUE));
	xv_set(inst->p_initwid, PANEL_VALUE, (*measures[new].pixel_to_unit)(srv, pix), NULL);

	pix = (*measures[old].unit_to_pixel)(srv, (int)xv_get(inst->p_inithig, PANEL_VALUE));
	xv_set(inst->p_inithig, PANEL_VALUE, (*measures[new].pixel_to_unit)(srv, pix), NULL);
}

static void note_measure_in(Panel_item item, int val)
{
	Xv_window p = xv_get(item, XV_OWNER);
	Xv_server srv = XV_SERVER_FROM_WINDOW(p);

	update_measure(srv, val);
}

static void convert_unit_int(int val, int panel_to_data, int *datptr,
						Panel_item it, xvwp_t *inst)
{
	Xv_server srv = XV_SERVER_FROM_WINDOW(xv_get(it, XV_OWNER));
	if (panel_to_data) {
		*datptr = (*measures[inst->data.measure_in].unit_to_pixel)(srv, val);
	}
	else {
		xv_set(it,
			PANEL_VALUE, (*measures[inst->data.measure_in].pixel_to_unit)(srv, *datptr),
			NULL);
	}
}

static void convert_num_string(char *val, int panel_to_data, int *datptr, Panel_item it, xvwp_t * inst)
{
	Xv_server srv = XV_SERVER_FROM_WINDOW(xv_get(it, XV_OWNER));

	if (panel_to_data) {
		*datptr = (*measures[inst->data.measure_in].unit_to_pixel)(srv, atoi(val));
	}
	else {
		char buf[20];

		sprintf(buf, "%d", (*measures[inst->data.measure_in].pixel_to_unit)(srv, *datptr));
		xv_set(it, PANEL_VALUE, buf, NULL);
	}
}

static void internal_write_file(xvwp_t * inst)
{
	int i;

	for (i = 0; i < PERM_NUM_CATS; i++) {
		Permprop_res_store_db(inst->xvwp_xrmdb[i], inst->appfiles[i]);
	}
}

static int get_wm_menu_default(Display *dpy, Frame fram)
{
	Atom typeatom;
	int act_format;
	unsigned long nelem, rest;
	unsigned char *data;
	int retval = 0;

	/*** PROPERTY_DEFINITION ***********************************************
	 * _OL_WIN_MENU_DEFAULT        Type INTEGER        Format 32
	 * Owner: wm, Reader: client
	 */
	if (XGetWindowProperty(dpy, (Window)xv_get(fram, XV_XID),
					W_ATOM(fram, "_OL_WIN_MENU_DEFAULT"),
					0L, 30L, False, XA_INTEGER,
					&typeatom, &act_format, &nelem, &rest,
					&data) == Success)
	{
		if (typeatom == XA_INTEGER && act_format == 32) {
			int *ip = (int *)data;

			retval = *ip;
		}
		XFree(data);
	}
	else {
		/*** PROPERTY_DEFINITION ***********************************************
		 * _OL_SET_WIN_MENU_DEFAULT        Type INTEGER        Format 32
		 * Owner: client, Reader: wm
		 */
		if (XGetWindowProperty(dpy, (Window)xv_get(fram, XV_XID),
						W_ATOM(fram, "_OL_SET_WIN_MENU_DEFAULT"),
						0L, 30L, False, XA_INTEGER,
						&typeatom, &act_format, &nelem, &rest,
						&data) == Success)
		{
			if (typeatom == XA_INTEGER && act_format == 32) {
				int *ip = (int *)data;

				retval = *ip;
			}
			XFree(data);
		}
	}
	return retval;
}

static int note_apply(Frame f)
{
	Rect r;
	int base_x = 0, base_y = 0;
	Xv_server srv = XV_SERVER_FROM_WINDOW(f);
	Server_info *srvpriv = SERVER_PRIVATE(srv);
	xvwp_t *inst = srvpriv->xvwp;
	Display *dpy = (Display *)xv_get(f, XV_DISPLAY);

	if (inst->data.popup_relative) {
		frame_get_rect(inst->data.base, &r);
		base_x = r.r_left;
		base_y = r.r_top;
	}

	if (inst->data.record_menus) {
		xvwp_popup_t f;
		xvwp_menu_t p = inst->data.menus;

		while (p) {
			int i, num;
			unsigned long mask = 1, selval = 0;
			Frame pinfram;

			if (! p->menu) {
				/* schon verschwunden, siehe (jkwgehrfw3f) */
				p = p->next;
				continue;
			}
			p->default_index = (int)xv_get(p->menu, MENU_DEFAULT);

			if (xv_get(p->menu, XV_IS_SUBTYPE_OF, MENU_TOGGLE_MENU)) {
				num = (int)xv_get(p->menu, MENU_NITEMS);

				for (i = 1; i <= num; i++) {
					Menu_item it = xv_get(p->menu, MENU_NTH_ITEM, i);

					if (xv_get(it, MENU_SELECTED)) selval |= mask;
					mask <<= 1;
				}
				p->selected = selval;
			}
			else if (xv_get(p->menu, XV_IS_SUBTYPE_OF, MENU_CHOICE_MENU)) {
				p->selected = (unsigned long)xv_get(p->menu, MENU_SELECTED);
			}

			p->is_pinned = FALSE;
			p->pin_x = p->pin_y = 0;
			if ((pinfram = xv_get(p->menu, MENU_PIN_WINDOW))) {
				if (xv_get(pinfram, XV_SHOW)) {
					p->is_pinned = TRUE;
					frame_get_rect(pinfram, &r);
					p->pin_x = r.r_left - base_x;
					p->pin_y = r.r_top - base_y;
				}
			}

			Permprop_res_update_dbs(inst->xvwp_xrmdb, p->name, (char *)p,
									res_menu, (unsigned)PERM_NUMBER(res_menu));
			p = p->next;
		}

		inst->data.record_menus = FALSE;

		/* jetzt noch das WM-Menue auf dem Baseframe: */
		inst->data.win_menu_default = get_wm_menu_default(dpy, inst->data.base);

		/* ref (drghjbvewrfvdfgh) */
		Permprop_res_update_dbs(inst->xvwp_xrmdb, inst->xvwp_prefix,
					(char *)&inst->data,
					res_base + PERM_NUMBER(res_base) - 1, 1);

		/* jetzt noch die WM-Menues auf den Frames: */
		f = inst->data.popups;

		while (f) {
			if (f->frame) {
				f->win_menu_default = get_wm_menu_default(dpy, f->frame);

				Permprop_res_update_dbs(inst->xvwp_xrmdb, f->name, (char *)f,
						res_popup_menu, (unsigned)PERM_NUMBER(res_popup_menu));
			}
			f = f->next;
		}
	}

	if (inst->data.record_popups) {
		xvwp_popup_t p = inst->data.popups;

		while (p) {
			if (p->frame) {
				frame_get_rect(p->frame, &r);
				p->x = r.r_left - base_x;
				p->y = r.r_top - base_y;
				p->width = (int)xv_get(p->frame, XV_WIDTH);
				p->height = (int)xv_get(p->frame, XV_HEIGHT);
				p->show = (int)xv_get(p->frame, XV_SHOW);

				if (xv_get(p->frame, XV_IS_SUBTYPE_OF, FRAME_CMD)) 
					p->pushpin_in = (int)xv_get(p->frame, FRAME_CMD_PIN_STATE);
				else p->pushpin_in = FALSE;

				Permprop_res_update_dbs(inst->xvwp_xrmdb, p->name, (char *)p,
								res_popup, (unsigned)PERM_NUMBER(res_popup));
			}
			p = p->next;
		}

		inst->data.record_popups = FALSE;
	}

	Permprop_res_update_dbs(inst->xvwp_xrmdb, inst->xvwp_prefix,
						(char *)&inst->data,
						res_base, (unsigned)PERM_NUMBER(res_base) - 1);

	if (xv_get(srv, XV_WANT_ROWS_AND_COLUMNS)) {
		Permprop_res_update_dbs(inst->xvwp_xrmdb, inst->xvwp_prefix,
				(char *)&inst->data, res_rc, (unsigned)PERM_NUMBER(res_rc));
	}
	else {
		Permprop_res_update_dbs(inst->xvwp_xrmdb, inst->xvwp_prefix,
			(char *)&inst->data, res_nonrc, (unsigned)PERM_NUMBER(res_nonrc));
	}

	internal_write_file(inst);

/* 	if (IS_DTRACE(123)) */
	if (! f)
	{
		if (inst->data.base_scale != inst->orig_base_scale) {
			Event scale_event;

			event_init(&scale_event);
			event_set_action(&scale_event, ACTION_RESCALE);
			event_set_id(&scale_event, ACTION_RESCALE);
			event_set_window(&scale_event, inst->data.base);
			win_post_event_arg(inst->data.base, &scale_event, NOTIFY_IMMEDIATE,
							(unsigned long)inst->data.base_scale, NULL, NULL);
		}
	}

	return XV_OK;
}

static int note_reset(Frame f)
{
	Xv_server srv = XV_SERVER_FROM_WINDOW(f);
	Server_info *srvpriv = SERVER_PRIVATE(srv);
	xvwp_t *inst = srvpriv->xvwp;

	xv_set(inst->p_icon_x_pos, XV_SHOW, inst->data.init_icon_loc_set, NULL);
	xv_set(inst->p_icon_y_pos, XV_SHOW, inst->data.init_icon_loc_set, NULL);

	xv_set(inst->p_x_pos, XV_SHOW, inst->data.init_loc_set, NULL);
	xv_set(inst->p_y_pos, XV_SHOW, inst->data.init_loc_set, NULL);
	update_measure(srv, inst->data.measure_in);

	inst->orig_base_scale = inst->data.base_scale;
	inst->orig_popup_scale = inst->data.popup_scale;

	return XV_OK;
}

static void note_record_base(Panel_item item)
{
	char buf[20];
	Rect r;
	int w, h;
	Icon icon;
	Xv_window p = xv_get(item, XV_OWNER);
	Xv_server srv = XV_SERVER_FROM_WINDOW(p);
	Server_info *srvpriv = SERVER_PRIVATE(srv);
	xvwp_t *inst = srvpriv->xvwp;

	frame_get_rect(inst->data.base, &r);

	if (xv_get(srv, XV_WANT_ROWS_AND_COLUMNS)) {
		w = (int)xv_get(inst->data.base, WIN_COLUMNS);
		h = (int)xv_get(inst->data.base, WIN_ROWS);
	}
	else {
		w = (int)xv_get(inst->data.base, XV_WIDTH);
		h = (int)xv_get(inst->data.base, XV_HEIGHT);
	}

	xv_set(inst->p_pos_def_spec, PANEL_VALUE, 1, NULL);
	xv_set(inst->propwin,
			FRAME_PROPS_ITEM_CHANGED, inst->p_pos_def_spec, TRUE,
			NULL);

	sprintf(buf, "%d",
			(*measures[inst->cur_measure].pixel_to_unit)(srv, r.r_left));
	xv_set(inst->p_x_pos,
			PANEL_VALUE, buf,
			XV_SHOW, TRUE,
			NULL);

	sprintf(buf, "%d",
			(*measures[inst->cur_measure].pixel_to_unit)(srv, r.r_top));
	xv_set(inst->p_y_pos,
			PANEL_VALUE, buf,
			XV_SHOW, TRUE,
			NULL);

	xv_set(inst->p_initwid,
			PANEL_VALUE, (*measures[inst->cur_measure].pixel_to_unit)(srv, w),
			NULL);
	xv_set(inst->propwin, FRAME_PROPS_ITEM_CHANGED, inst->p_initwid, TRUE, NULL);
	xv_set(inst->p_inithig,
			PANEL_VALUE, (*measures[inst->cur_measure].pixel_to_unit)(srv, h),
			NULL);
	xv_set(inst->propwin, FRAME_PROPS_ITEM_CHANGED, inst->p_inithig, TRUE, NULL);

	icon = xv_get(inst->data.base, FRAME_ICON);
	if (icon) {
		xv_set(inst->p_pos_icon_def_spec, PANEL_VALUE, 1, NULL);
		xv_set(inst->propwin,
				FRAME_PROPS_ITEM_CHANGED, inst->p_pos_icon_def_spec, TRUE,
				NULL);

		sprintf(buf, "%d", (int)xv_get(icon, XV_X));
		xv_set(inst->p_icon_x_pos,
				PANEL_VALUE, buf,
				XV_SHOW, TRUE,
				NULL);

		sprintf(buf, "%d", (int)xv_get(icon, XV_Y));
		xv_set(inst->p_icon_y_pos,
				PANEL_VALUE, buf,
				XV_SHOW, TRUE,
				NULL);
	}

	xv_set(item, PANEL_NOTIFY_STATUS, XV_ERROR, NULL);
}

static void note_record_popup(Panel_item item)
{
	Xv_window p = xv_get(item, XV_OWNER);
	Xv_server srv = XV_SERVER_FROM_WINDOW(p);
	Server_info *srvpriv = SERVER_PRIVATE(srv);
	xvwp_t *inst = srvpriv->xvwp;

	inst->data.record_popups = TRUE;
	xv_set(item, PANEL_NOTIFY_STATUS, XV_ERROR, NULL);
}

static void note_record_menus(Panel_item item)
{
	Xv_window p = xv_get(item, XV_OWNER);
	Xv_server srv = XV_SERVER_FROM_WINDOW(p);
	Server_info *srvpriv = SERVER_PRIVATE(srv);
	xvwp_t *inst = srvpriv->xvwp;

	inst->data.record_menus = TRUE;
	xv_set(item, PANEL_NOTIFY_STATUS, XV_ERROR, NULL);
}

static void note_pos_def_spec(Panel_item item, int value)
{
	Xv_window p = xv_get(item, XV_OWNER);
	Xv_server srv = XV_SERVER_FROM_WINDOW(p);
	Server_info *srvpriv = SERVER_PRIVATE(srv);
	xvwp_t *inst = srvpriv->xvwp;

	xv_set(inst->p_x_pos, XV_SHOW, value, NULL);
	xv_set(inst->p_y_pos, XV_SHOW, value, NULL);
}

static void note_pos_icon_def_spec(Panel_item item, int value)
{
	Xv_window p = xv_get(item, XV_OWNER);
	Xv_server srv = XV_SERVER_FROM_WINDOW(p);
	Server_info *srvpriv = SERVER_PRIVATE(srv);
	xvwp_t *inst = srvpriv->xvwp;

	xv_set(inst->p_icon_x_pos, XV_SHOW, value, NULL);
	xv_set(inst->p_icon_y_pos, XV_SHOW, value, NULL);
}

static Panel_setting note_numeric_field(Panel_item item, Event *ev)
{
	Xv_window p = xv_get(item, XV_OWNER);
	Xv_server srv = XV_SERVER_FROM_WINDOW(p);
	Server_info *srvpriv = SERVER_PRIVATE(srv);
	xvwp_t *inst = srvpriv->xvwp;

	switch (event_action(ev)) {
		case ACTION_CUT:
		case ACTION_PASTE:
		case ACTION_ERASE_CHAR_BACKWARD:
		case ACTION_ERASE_CHAR_FORWARD:
		case ACTION_ERASE_WORD_BACKWARD:
		case ACTION_ERASE_WORD_FORWARD:
		case ACTION_ERASE_LINE_BACKWARD:
		case ACTION_ERASE_LINE_END:
			xv_set(inst->propwin, FRAME_PROPS_ITEM_CHANGED, item, TRUE, NULL);
			break;
		default:
			if (event_is_iso(ev)) {
				if (! isdigit(event_id(ev))) return PANEL_NONE;
				xv_set(inst->propwin, FRAME_PROPS_ITEM_CHANGED, item, TRUE, NULL);
			}
	}

	return panel_text_notify(item, ev);
}

static void update_changebar(Panel_item item)
{
	Xv_window p = xv_get(item, XV_OWNER);
	Xv_server srv = XV_SERVER_FROM_WINDOW(p);
	Server_info *srvpriv = SERVER_PRIVATE(srv);
	xvwp_t *inst = srvpriv->xvwp;

	xv_set(inst->propwin, FRAME_PROPS_ITEM_CHANGED, item, TRUE, NULL);
}

Pkg_private void server_xvwp_init(Server_info *srv, char **argv)
{
	if (srv->xvwp) return;

	srv->xvwp = xv_alloc(xvwp_t);

	if (! xvwp_key) {
		xvwp_key = xv_unique_key();
		xvwp_popup_key = xv_unique_key();
	}

	if (argv) {
		while (*++argv) {
			if (!strcmp(*argv, "-Wi")) srv->xvwp->argv_contains.iconic = TRUE;
			if (!strcmp(*argv, "+Wi")) srv->xvwp->argv_contains.iconic = TRUE;
			if (!strcmp(*argv, "-Wp")) srv->xvwp->argv_contains.pos = TRUE;
			if (!strcmp(*argv, "-position")) srv->xvwp->argv_contains.pos= TRUE;
			if (!strcmp(*argv, "-WP")) srv->xvwp->argv_contains.icon_pos = TRUE;
			if (!strcmp(*argv, "-Ws")) srv->xvwp->argv_contains.size = TRUE;
			if (!strcmp(*argv, "-size")) srv->xvwp->argv_contains.size = TRUE;
			if (!strcmp(*argv, "-Wf")) srv->xvwp->argv_contains.fg = TRUE;
			if (!strcmp(*argv, "-fg")) srv->xvwp->argv_contains.fg = TRUE;
			if (!strcmp(*argv, "-foreground")) srv->xvwp->argv_contains.fg=TRUE;
			if (!strcmp(*argv, "-foreground_color"))
				srv->xvwp->argv_contains.fg = TRUE;
			if (!strcmp(*argv, "-Wx")) srv->xvwp->argv_contains.scale = TRUE;
			if (!strcmp(*argv, "-scale")) srv->xvwp->argv_contains.scale = TRUE;
		}
	}
}

typedef struct { int h, s, v; } HSV;
#define	MAXRGB	0xff
#define	MAXSV	MAXRGB

static int max3(int x, int y, int z)
{
    if (y > x) x = y;
    if (z > x) x = z;
    return x;
}

static int min3(int x, int y, int z)
{
    if (y < x) x = y;
    if (z < x) x = z;
    return x;
}

static void hsv_to_rgb(HSV *hsv, Xv_singlecolor *rgb)
{
    int h = hsv->h;
    int s = hsv->s;
    int v = hsv->v;
    int r = 0, g = 0, b = 0;
    int i, f;
    int p, q, t;

    s = (s * MAXRGB) / MAXSV;
    v = (v * MAXRGB) / MAXSV;
    if (h == 360) h = 0;

	if (s == 0) {
		h = 0;
		r = g = b = v;
	}
    i = h / 60;
    f = h % 60;
    p = v * (MAXRGB - s) / MAXRGB;
    q = v * (MAXRGB - s * f / 60) / MAXRGB;
    t = v * (MAXRGB - s * (60 - f) / 60) / MAXRGB;

	switch (i) {
		case 0:
			r = v, g = t, b = p;
			break;
		case 1:
			r = q, g = v, b = p;
			break;
		case 2:
			r = p, g = v, b = t;
			break;
		case 3:
			r = p, g = q, b = v;
			break;
		case 4:
			r = t, g = p, b = v;
			break;
		case 5:
			r = v, g = p, b = q;
			break;
	}
    rgb->red = r;
    rgb->green = g;
    rgb->blue = b;
}

static void rgb_to_hsv(Xv_singlecolor *rgb, HSV *hsv)
{
    int r = rgb->red;
    int g = rgb->green;
    int b = rgb->blue;
    register int maxv = max3(r, g, b);
    register int minv = min3(r, g, b);
    int h = 0;
    int s;
    int v;

    v = maxv;

	if (maxv) {
		s = (maxv - minv) * MAXRGB / maxv;
	}
	else {
		s = 0;
	}

	if (s == 0) {
		h = 0;
	}
	else {
		int rc;
		int gc;
		int bc;
		int hex = 0;

		rc = (maxv - r) * MAXRGB / (maxv - minv);
		gc = (maxv - g) * MAXRGB / (maxv - minv);
		bc = (maxv - b) * MAXRGB / (maxv - minv);

		if (r == maxv) {
			h = bc - gc;
			hex = 0;
		}
		else if (g == maxv) {
			h = rc - bc;
			hex = 2;
		}
		else if (b == maxv) {
			h = gc - rc;
			hex = 4;
		}
		h = hex * 60 + (h * 60 / MAXRGB);
		if (h < 0)
			h += 360;
	}
    hsv->h = h;
    hsv->s = (s * MAXSV) / MAXRGB;
    hsv->v = (v * MAXSV) / MAXRGB;
}

#ifdef NOT_NEEDED
static void tell_res(s)
	char *s;
{
	fprintf(stderr, "\n%s:\n", s);
	fprintf(stderr, "window.scale = %s\n", defaults_get_string("window.scale",
												"Window.Scale", "not set"));
	fprintf(stderr, "window.scale.cmdline = %s\n",
			defaults_get_string("window.scale.cmdline",
									"Window.Scale.Cmdline", "not set"));
	fprintf(stderr, "font.name.cmdline = %s\n",
				defaults_get_string("font.name.cmdline",
									"Font.Name.Cmdline", "not set"));
	fprintf(stderr, "font.name = %s\n",
				defaults_get_string("font.name", "Font.Name", "not set"));
	fprintf(stderr, "openwindows.regularfont = %s\n",
				defaults_get_string("openwindows.regularfont",
									"OpenWindows.RegularFont", "not set"));
	fprintf(stderr, "openwindows.boldfont = %s\n",
				defaults_get_string("openwindows.boldfont",
									"OpenWindows.BoldFont", "not set"));
	fprintf(stderr, "openwindows.monospacefont = %s\n",
				defaults_get_string("openwindows.monospacefont",
									"OpenWindows.MonospaceFont", "not set"));
}
#endif /* NOT_NEEDED */

typedef struct _ui_reg {
	struct _ui_reg *next;
	Xv_opaque obj;
	char *name;
} *ui_reg_t;

static ui_reg_t regui = NULL;
static void server_set_any_menu(Menu menu, const char *name,
					Xv_opaque panelitem_window_or_server);

static void internal_register_ui(Xv_opaque obj, const char *name, Frame base)
{
	if (xv_get(obj, XV_IS_SUBTYPE_OF, MENU)) {
		if (name && *name) server_set_any_menu(obj, name, base);
		else server_set_menu(obj, base);
	}
	else if (xv_get(obj, XV_IS_SUBTYPE_OF, FRAME_CMD)) {
		Attr_attribute attrs[2];

		/* als ich das fuer die internen XView-Textsw-Frames eingebaut habe,
		 * hatten die alle ihre 36x80-Groesse...
		 * Forschung (dgtrfbedrgtjklgy)
		 * Erkenntnis: frame_get_rect und frame set_rect sind potentielle
		 * Idioten, sind sehr nah an X und kuemmern sich nicht um
		 * XV_WIDTH und XV_HEIGHT
		 */
		attrs[0] = XV_WIDTH;
		attrs[1] = 0;
		server_set_popup(obj, attrs);
	}
	else {
		fprintf(stderr, "INCOMPLETE internal_register_ui\n");
	}
}

/* Das ist die Default-Funktion fuer das Attribut SERVER_UI_REGISTRATION_PROC.
 * Derzeit (15.7.23) setzt keine Applikation dieses Attribut.
 * Man kommt also hierher über die Funktion server_register_ui.
 * Die wiederum ist Xv_private und wird (auch derzeit) nur aus txt_popup.c
 * aufgerufen.
 */
Pkg_private void server_note_register_ui(Xv_server srv, Xv_opaque obj, const char *name)
{
	int queue_requests = (int)xv_get(srv, QUEUE_REQUESTS);

	if (queue_requests) {
		ui_reg_t n = xv_alloc(struct _ui_reg);

		n->obj = obj;
		if (name && *name) {
			n->name = xv_strsave(name);
		}
		else {
			n->name = NULL;
		}
		n->next = regui;
		regui = n;
	}
	else {
		Server_info *srvpriv = SERVER_PRIVATE(srv);

		/* handle immediately */
		internal_register_ui(obj, name, srvpriv->xvwp->data.base);
	}
}

static void process_ui_registrations(ui_reg_t r, Frame bas)
{
	if (! r) return;
	process_ui_registrations(r->next, bas);

	internal_register_ui(r->obj, r->name, bas);
	if (r->name) xv_free(r->name);
	xv_free(r);
}

static void initialize_popup_frames(Frame_cmd popup, Frame owner)
{
	Xv_server srv;
	Xv_font parents_font, new_font;
	Server_info *srvpriv;

	if (! owner) return;

	srv = XV_SERVER_FROM_WINDOW(owner);
	srvpriv = SERVER_PRIVATE(srv);

	/* we don't need anything to do if the scales are equal.... */
	if (srvpriv->xvwp->data.use_base_scale == srvpriv->xvwp->data.use_popup_scale)
		return;

	parents_font = xv_get(owner, XV_FONT);
	new_font = xv_find(srv, FONT,
			FONT_RESCALE_OF, parents_font, srvpriv->xvwp->data.use_popup_scale,
			NULL);
	xv_set(popup, XV_FONT, new_font, NULL);
}

static void make_startup_name(char *base, char *buf)
{
	char *home;
	struct passwd *pwd;

	pwd = getpwuid(geteuid());
	if (pwd) {
		home = pwd->pw_dir;
	}
	else {
		if (!(home = (char *)getenv("HOME"))) {
			perror("cannot determine home directory");
			exit(1);
		}
	}

	sprintf(buf, "%s/%s", home, base);
}

Pkg_private void server_xvwp_connect(Xv_server srv, char *base_inst_name)
{
	Server_info *srvpriv = SERVER_PRIVATE(srv);
	char res_buf[500], file[1000], appfile[201], xviewdir[100], host[30];
	XColor xcol;
	Display *display;
	Colormap cmap;
	char *def_back_color;
	char *def_fore_color;
	int i, my_scale, global_scale;
	struct utsname my_uname;
	screen_ui_style_t ui_style;

	xv_set_popup_frame_initializer(initialize_popup_frames);
	xv_set(srv, QUEUE_REQUESTS, TRUE, NULL);

	/* otherwise no other font can be used */
	if (! srvpriv->xvwp->argv_contains.scale) {
		defaults_set_string("window.scale.cmdline", "");
	}

	ui_style = (screen_ui_style_t)xv_get(xv_get(srv, SERVER_NTH_SCREEN, 0),
											SCREEN_UI_STYLE);
	srvpriv->xvwp->wm = determine_window_manager(srv);

	/* find the true default window color */
	def_back_color = (char *)defaults_get_string("openWindows.windowColor",
									"OpenWindows.WindowColor", "#cccccc");
	def_back_color = xv_strsave(def_back_color);
/* 	DTRACE(DTL_INTERN, "defaults_get_string('OpenWindows.WindowColor')=%s\n", */
/* 										def_back_color); */
	def_fore_color = (char *)defaults_get_string("window.color.foreground",
									"Window.Color.Foreground", "#000000");
	def_fore_color = xv_strsave(def_fore_color);

	global_scale = defaults_get_enum("openWindows.scale", "OpenWindows.Scale",
												scale_enum);

	display = (Display *)xv_get(srv, XV_DISPLAY);
	cmap = DefaultColormap(display, 0);

	{
		Xv_singlecolor *wincol, *forecol;
		int numfore;

		conv_win_colors(display, cmap,
			defaults_get_string("openWindows.allWindowColors",
						"OpenWindows.AllWindowColors",
						"#B4B4B4\n#C8C8C8\n#D4D4D4\n#E0E0E0\n#F0F0F0\n#B7C2E5\n#CCCCEA\n#CCD6EA\n#CCE0EA\n#E0E4FF\n#E0FEFF\n#B7E5E5\n#CCEAEA\n#CCEAE0\n#E0FFE9\n#B7E5C2\n#D6EACC\n#CCEACC\n#CCEAD6\n#F3FFE0\n#E0EACC\n#E3E5B7\n#BFB498\n#E5D8B7\n#EAE0CC\n#EAEACC\n#EACCCC\n#E5B7B7\n#D5B7E5\n#D6CCEA\n#E0CCEA\n#EACCEA\n#FFBCEA\n#FFE0F9\n#E5B7DA\n#EACCD6\n#EACCE0\n#FFAAAA\n#FFCCCC\n#FFE0E0\n#FFF6E0\n#FFE6B1\n#FFE8BE\n#FAFFB8\n"),
					&wincol, &fore_start);

		conv_win_colors(display, cmap,
			defaults_get_string("openWindows.allForegroundColors",
								"OpenWindows.AllForegroundColors",
								"#FF0000\n#C80000\n#A00000\n#00C800\n#00A000\n#0000FF\n#0000C8\n#0000A0\n#A0A000\n#00A0A0\n#FF00FF\n#A000A0\n#A600FF\n#A65500\n#0055A0\n#A00055\n#5500A0\n#888888\n#000000\n"),
					&forecol, &numfore);

		total_num_colors = fore_start + numfore + 1;  /* the real foreground */
		my_colors = (Xv_singlecolor *)xv_realloc(wincol,
							total_num_colors * sizeof(Xv_singlecolor));
		memcpy(my_colors+fore_start, forecol, numfore*sizeof(Xv_singlecolor));
		xv_free(forecol);
	}

	{
		int i;
		int bright_perc=defaults_get_integer("openWindows.brightnessPercentage",
											"OpenWindows.BrightnessPercentage",
											100);
		int sat_perc = defaults_get_integer("openWindows.saturationPercentage",
											"OpenWindows.SaturationPercentage",
											100);

		for (i = 1; i < fore_start; i++) {
			HSV hsv;

			rgb_to_hsv(my_colors + i, &hsv);

			/* modify saturation and brightness
			 * according to resource database
			 */
			hsv.s = (hsv.s * sat_perc) / 100;
			hsv.v = (hsv.v * bright_perc) / 100;
			if (hsv.s > MAXSV) hsv.s = MAXSV;
			if (hsv.v > MAXSV) hsv.v = MAXSV;

			hsv_to_rgb(&hsv, my_colors + i);
		}
	}

	XParseColor(display, cmap, def_back_color, &xcol);
	my_colors[0].red = (xcol.red >> 8);
	my_colors[0].green = (xcol.green >> 8);
	my_colors[0].blue = (xcol.blue >> 8);

	XParseColor(display, cmap, def_fore_color, &xcol);
	my_colors[fore_start].red = (xcol.red >> 8);
	my_colors[fore_start].green = (xcol.green >> 8);
	my_colors[fore_start].blue = (xcol.blue >> 8);

	xv_free(def_back_color);
	xv_free(def_fore_color);

	memset((char *)&srvpriv->xvwp->data, 0, sizeof(srvpriv->xvwp->data));
	srvpriv->xvwp->data.base_scale = srvpriv->xvwp->data.popup_scale = WIN_SCALE_DEFAULT;
	srvpriv->xvwp->data.popups = (xvwp_popup_t)0;
	srvpriv->xvwp->data.menus = (xvwp_menu_t)0;

    xv_add_custom_attrs(FRAME_CLASS, 
				XV_SHOW, "xv_show",
				NULL);

    xv_add_custom_attrs(FRAME_BASE, 
				FRAME_CLOSED, "frame_closed",
				NULL);

    xv_add_custom_attrs(FRAME_CMD, 
				FRAME_CMD_PUSHPIN_IN, "frame_cmd_pushpin_in",
				NULL);

    xv_add_custom_attrs(MENU, 
				MENU_DEFAULT, "menu_default",
				NULL);

	make_startup_name(".xview", xviewdir);
	if (access(xviewdir, 0)) mkdir(xviewdir, 0755);

	strcat(xviewdir, "/preferences");
	if (access(xviewdir, 0)) mkdir(xviewdir, 0755);

	sprintf(appfile, "%s/%s", xviewdir, xv_instance_app_name);
	srvpriv->xvwp->appfiles[(int)PRC_U] = xv_strsave(appfile);

	uname(&my_uname);
	{
		char *p;

		/* seit 31.07.2023 hat sich folgendes geaendert:
		 * Wenn in /etc/hostname steht "lala.huhu.de", dann liefert uname -n
		 * auch "lala.huhu.de". Vorher kam da "lala"
		 */

		if ((p = strchr(my_uname.nodename, '.'))) *p = '\0';
	}

	sprintf(file, "%s.%s", appfile, my_uname.nodename);
	srvpriv->xvwp->appfiles[(int)PRC_H] = xv_strsave(file);

	xv_get(srv, SERVER_HOST_NAME, host);
	sprintf(file, "%s:%s", appfile, host);
	srvpriv->xvwp->appfiles[(int)PRC_D] = xv_strsave(file);

	snprintf(file, sizeof(file), "%s.%s:%s", appfile, my_uname.nodename, host);
	srvpriv->xvwp->appfiles[(int)PRC_B] = xv_strsave(file);

	for (i = 0; i < PERM_NUM_CATS; i++) {
		srvpriv->xvwp->xvwp_xrmdb[i] = Permprop_res_create_file_db(srvpriv->xvwp->appfiles[i]);
	}

	snprintf(srvpriv->xvwp->xvwp_prefix, sizeof(srvpriv->xvwp->xvwp_prefix),
			"%s.%s.", xv_instance_app_name, base_inst_name);
	Permprop_res_read_dbs(srvpriv->xvwp->xvwp_xrmdb, srvpriv->xvwp->xvwp_prefix,
				(char *)&srvpriv->xvwp->data, res_base, 
				(unsigned)PERM_NUMBER(res_base));

	defaults_set_integer("window.avoidQueryTreeCount",
						srvpriv->xvwp->data.avoid_query_tree_count);

	srvpriv->xvwp->data.use_base_scale = 
		((srvpriv->xvwp->data.base_scale == WIN_SCALE_DEFAULT)
				? global_scale
				: srvpriv->xvwp->data.base_scale);
	srvpriv->xvwp->data.use_popup_scale = 
		((srvpriv->xvwp->data.popup_scale == WIN_SCALE_DEFAULT)
				? global_scale
				: srvpriv->xvwp->data.popup_scale);

	if (xv_get(srv, XV_WANT_ROWS_AND_COLUMNS)) {
		Permprop_res_read_dbs(srvpriv->xvwp->xvwp_xrmdb, srvpriv->xvwp->xvwp_prefix,
						(char *)&srvpriv->xvwp->data, res_rc, 
						(unsigned)PERM_NUMBER(res_rc));
	}
	else {
		Permprop_res_read_dbs(srvpriv->xvwp->xvwp_xrmdb, srvpriv->xvwp->xvwp_prefix,
						(char *)&srvpriv->xvwp->data, res_nonrc, 
						(unsigned)PERM_NUMBER(res_nonrc));
	}

	/* the following is necessary to have menus show
	 * the correctly scaled font !
	 */
	if (srvpriv->xvwp->argv_contains.scale)
		my_scale = global_scale;
	else my_scale = srvpriv->xvwp->data.use_base_scale;

/* 	tell_res("before merging databases"); */
	{
		extern XrmDatabase defaults_rdb;
		Xv_opaque tmp_db;

		for (i = 0; i < PERM_NUM_CATS; i++) {
			tmp_db = Permprop_res_create_file_db(srvpriv->xvwp->appfiles[i]);
			defaults_rdb = (XrmDatabase)Permprop_res_merge_dbs(tmp_db,
											(Xv_opaque)defaults_rdb);
		}
	}
/* 	tell_res("after merging databases"); */

	/* the following is necessary to have menus show
	 * the correctly scaled font !
	 */
	if (global_scale != my_scale) {
		Xv_window root;
		Xv_font font;

		/* no multiscreen support....   >-(  */
		root = xv_get(xv_get(srv, SERVER_NTH_SCREEN, 0), XV_ROOT);

		if (defaults_exists(FNRES, FNCLASS)) {
			char *fontname = defaults_get_string(FNRES, FNCLASS,
							"-*-lucida-medium-r-*-*-*-120-*-*-*-*-iso10646-1");

			font = xv_find(srv, FONT, FONT_NAME, fontname, NULL);

			if (font) {
				font = xv_find(srv, FONT,
						FONT_RESCALE_OF, font, my_scale,
						NULL);
			}
		}
		else {
			font = xv_find(srv, FONT, FONT_SCALE, my_scale, NULL);
		}

		if (font) {
			char *fontname = (char *)xv_get(font, FONT_NAME);

			xv_set(root, XV_FONT, font, NULL);

			/* probably this is too late anyway .... */
			defaults_set_string(FNRES, fontname);

			srvpriv->xvwp->font_for_root_and_frame = font;
		}

		if (defaults_exists("openwindows.boldfont", "OpenWindows.BoldFont")) {
			char *fontname = defaults_get_string("openwindows.boldfont",
							"OpenWindows.BoldFont",
							"-*-lucida-bold-r-*-*-*-120-*-*-*-*-iso10646-1");

			font = xv_find(srv, FONT, FONT_NAME, fontname, NULL);

			if (font) {
				font = xv_find(srv, FONT,
						FONT_RESCALE_OF, font, my_scale,
						NULL);
			}
		}
		else {
			font = xv_find(srv, FONT,
						FONT_STYLE, FONT_STYLE_BOLD,
						FONT_SCALE, my_scale,
						NULL);
		}

		if (font) {
			char *fontname = (char *)xv_get(font, FONT_NAME);

							
			defaults_set_string("OpenWindows.BoldFont", fontname);
		}

		if (defaults_exists("openwindows.monospacefont",
							"OpenWindows.MonospaceFont"))
		{
			char *fontname = defaults_get_string("openwindows.monospacefont",
					"OpenWindows.MonospaceFont",
					"-*-lucidatypewriter-medium-r-*-*-*-120-*-*-*-*-iso10646-1");

			font = xv_find(srv, FONT, FONT_NAME, fontname, NULL);

			if (font) {
				font = xv_find(srv, FONT,
						FONT_RESCALE_OF, font, my_scale,
						NULL);
				if (font) {
					char *fontname = (char *)xv_get(font, FONT_NAME);

					defaults_set_string("OpenWindows.MonospaceFont", fontname);
				}
			}
		}
	}

/* 	tell_res("after font action"); */
	if (srvpriv->xvwp->data.measure_in >= (int)PERM_NUMBER(measures))
		srvpriv->xvwp->data.measure_in = 0;
	if (xv_get(srv, XV_WANT_ROWS_AND_COLUMNS))
		srvpriv->xvwp->data.measure_in = 0;

	srvpriv->xvwp->cur_measure = srvpriv->xvwp->data.measure_in;

	/* now we try to give those command line options that are also
	 * in our Window Properties higher priority than what was
	 * specified in the prop window.
	 */

	if (srvpriv->xvwp->data.back.indx != 0) {
		defaults_set_string("OpenWindows.WindowColor",
							colorname(srvpriv->xvwp->data.back.indx, 0));
	}

	if (srvpriv->xvwp->data.fore.indx!=0 && !srvpriv->xvwp->argv_contains.fg) {
		if (ui_style != SCREEN_UIS_2D_BW) {
			defaults_set_string("window.color.foreground",
						colorname(srvpriv->xvwp->data.fore.indx, fore_start));
			defaults_set_string("Window.Color.Foreground",
						colorname(srvpriv->xvwp->data.fore.indx, fore_start));
		}
	}

	if (! srvpriv->xvwp->argv_contains.scale) {
		int iii;

		for (iii = 0; scale_enum[iii].name; iii++) {
			if (my_scale == scale_enum[iii].value) break;
		}

		if (scale_enum[iii].name) {
/*     		defaults_set_string("window.scale.cmdline", scale_enum[iii].name); */
			defaults_set_string("OpenWindows.Scale", scale_enum[iii].name);
		}
	}

	if (srvpriv->xvwp->argv_contains.iconic &&
		defaults_exists("window.iconic","Window.Iconic"))
	{
		int iconic=defaults_get_boolean("window.iconic","Window.Iconic",FALSE);	
		sprintf(res_buf, "%sframe_closed", srvpriv->xvwp->xvwp_prefix);
		defaults_set_boolean(res_buf, iconic);
	}

	if (! srvpriv->xvwp->argv_contains.icon_pos) {
		if (srvpriv->xvwp->data.init_icon_loc_set) {
			defaults_set_integer("Icon.X", srvpriv->xvwp->data.icon_x);
			defaults_set_integer("Icon.Y", srvpriv->xvwp->data.icon_y);
		}
	}

	if (srvpriv->xvwp->argv_contains.size) {
		if (defaults_exists("window.width", "Window.Width")) {
			int val = defaults_get_integer("window.width", "Window.Width", 1);

			sprintf(res_buf, "%sxv_width", srvpriv->xvwp->xvwp_prefix);
			defaults_set_integer(res_buf, val);
		}

		if (defaults_exists("window.height", "Window.Height")) {
			int val = defaults_get_integer("window.height","Window.Height",1);

			sprintf(res_buf, "%sxv_height", srvpriv->xvwp->xvwp_prefix);
			defaults_set_integer(res_buf, val);
		}
	}

	/* now we fill in the foreground color for this application */
	def_fore_color = (char *)defaults_get_string("window.color.foreground",
									"Window.Color.Foreground", "#000000");

	XParseColor(display, cmap, def_fore_color, &xcol);
	my_colors[total_num_colors-1].red = (xcol.red >> 8);
	my_colors[total_num_colors-1].green = (xcol.green >> 8);
	my_colors[total_num_colors-1].blue = (xcol.blue >> 8);
}

static void xvwp_set_base_position(Frame base)
{
	int set_pos = FALSE, x = 0, y = 0;
	Rect rect;
	Xv_server srv = XV_SERVER_FROM_WINDOW(base);
	Server_info *srvpriv = SERVER_PRIVATE(srv);
	xvwp_t *inst = srvpriv->xvwp;

	if (inst->argv_contains.pos) {
		if (defaults_exists("window.x", "Window.X") ||
			defaults_exists("window.y", "Window.Y"))
		{
			x = defaults_get_integer("window.x", "Window.X", 0);
			y = defaults_get_integer("window.y", "Window.Y", 0);
			set_pos = TRUE;
		}
	}
	else if (inst->data.init_loc_set) {
		x = inst->data.x;
		y = inst->data.y;
		set_pos = TRUE;
	}

	if (set_pos) {
		char res_buf[500];

		/* find out whether we are iconic */
		sprintf(res_buf, "%sframe_closed", inst->xvwp_prefix);
		if (defaults_exists(res_buf, res_buf)) {
			if (defaults_get_boolean(res_buf, res_buf, FALSE)) {
				x -= 5;
				y -= 26;
			}
		}

		frame_get_rect(base, &rect);
		rect.r_left = x;
		rect.r_top = y;
		frame_set_rect(base, &rect);
	}
}

static xvwp_popup_t unlink_popup(xvwp_popup_t list, xvwp_popup_t old)
{
	if (! list) return list;

	if (list == old) {
		return list->next;
	}

	list->next = unlink_popup(list->next, old);
	return list;
}

static void destroy_obj(Xv_object obj, int unused_key, Xv_opaque data)
{
	xv_destroy(data);
}

static void destroy_images(Xv_object obj, int unused_key, char *data)
{
	int i;
	Server_image *images = (Server_image *)data;

	for (i = 0; images[i]; i++) xv_destroy(images[i]);
	xv_free(data);
}

static void note_popup_dying(Frame popup, int unused_key, Xv_opaque dt)
{
	Xv_server srv = xv_default_server;
	Server_info *srvpriv = SERVER_PRIVATE(srv);
	xvwp_t *inst = srvpriv->xvwp;
	xvwp_popup_t data = (xvwp_popup_t)dt;

	/* mit find_instance brauchen wir es hier gar nicht mehr zu versuchen:
	 * Wenn wir hier aufgerufen werden, ist popup kein Frame mehr, sondern
	 * WIRKLICH nur noch ein Object... damit aber funktioniert
	 * find_instance nicht mehr.
	 */
	
	inst->data.popups = unlink_popup(inst->data.popups, data);

	data->frame = XV_NULL;
	if (inst) {
		xv_free(data->name);
		xv_free(data);
	}
}

#define CHIP_HEIGHT	16
#define CHIP_WIDTH	16
static unsigned short chip_data[] = {
	0x7FFE, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0x7FFE
};

static void fill_me(Frame prop)
{
	Rect *r;
	Panel_item relc, popuplab, p_meas, color_choice;
	Xv_server srv = XV_SERVER_FROM_WINDOW(prop);
	Server_info *srvpriv = SERVER_PRIVATE(srv);
	xvwp_t *inst = srvpriv->xvwp;
	Xv_screen screen = XV_SCREEN_FROM_WINDOW(prop);
	int want_color = (int)xv_get(prop, XV_KEY_DATA, XV_DEPTH);
	Panel pan = xv_get(prop, FRAME_PROPS_PANEL);
	int color_columns;
	Cms cms = xv_get(pan, WIN_CMS);
	Display *dpy = srvpriv->xdisplay;
	GC gc = NULL, bggc = NULL;
	Server_image clip, *images = xv_calloc((unsigned)total_num_colors + 1,
										(unsigned)sizeof(Server_image));

	color_columns = (int)sqrt((double)fore_start) - 1;
	if (color_columns < 1) color_columns = 1;

	if (want_color) {
		cms = xv_create(screen, CMS,
				CMS_CONTROL_CMS, TRUE,
				CMS_SIZE, CMS_CONTROL_COLORS + total_num_colors,
				CMS_COLORS, my_colors,
				NULL);
		xv_set(pan, WIN_CMS, cms, NULL);
	}

	xv_set(inst->propwin,
				FRAME_PROPS_CREATE_ITEM,
					FRAME_PROPS_ITEM_SPEC, &color_choice, -1, PANEL_CHOICE,
					PANEL_DISPLAY_LEVEL, PANEL_CURRENT,
					PANEL_INACTIVE, ! want_color,
					PANEL_CHOICE_NCOLS, color_columns,
					PANEL_LABEL_STRING, XV_MSG("Window Color:"),
					XV_HELP_DATA, "windowprops:xvwp_back_color",
					FRAME_PROPS_DATA_OFFSET, OFF(back.indx),
					NULL,
				NULL);

	xv_set(xv_get(color_choice, PANEL_ITEM_MENU),
				XV_HELP_DATA, "windowprops:xvwp_back_color_menu",
				NULL);

	clip = xv_create(screen, SERVER_IMAGE,
			XV_WIDTH, CHIP_WIDTH,
			XV_HEIGHT, CHIP_HEIGHT,
			SERVER_IMAGE_DEPTH, 1L,
			SERVER_IMAGE_BITS, chip_data,
			NULL);

	xv_set(inst->propwin,
			XV_KEY_DATA, SERVER_IMAGE_BITS, clip,
			XV_KEY_DATA_REMOVE_PROC, SERVER_IMAGE_BITS, destroy_obj,
			NULL);

	if (! want_color) {
		xv_set(color_choice, PANEL_CHOICE_IMAGES, clip, 0, NULL);
	}
	else {
		int i;

		gc = XCreateGC(dpy, xv_get(pan, XV_XID), 0L, NULL);
		XSetClipMask(dpy, gc, xv_get(clip, XV_XID));
		bggc = XCreateGC(dpy, xv_get(pan, XV_XID), 0L, NULL);
		XSetForeground(dpy, bggc, xv_get(cms, CMS_PIXEL, 0));

		for (i = 0; i < fore_start; i++) {
			Pixmap pix;

			images[i] = xv_create(screen, SERVER_IMAGE,
					SERVER_IMAGE_CMS, cms,
					XV_WIDTH, CHIP_WIDTH,
					XV_HEIGHT, CHIP_HEIGHT,
					SERVER_IMAGE_DEPTH, xv_get(pan, XV_DEPTH),
					NULL);

			pix = (Pixmap)xv_get(images[i], XV_XID);

			XFillRectangle(dpy, pix, bggc, 0, 0, CHIP_WIDTH, CHIP_HEIGHT);
			XSetForeground(dpy, gc, xv_get(cms,CMS_PIXEL,CMS_CONTROL_COLORS+i));
			XFillRectangle(dpy, pix, gc, 0, 0, CHIP_WIDTH, CHIP_HEIGHT);
			xv_set(color_choice, PANEL_CHOICE_IMAGE, i, images[i], NULL);
		}
	}

	color_columns = (int)sqrt((double)(total_num_colors-fore_start-1)) - 1;
	if (color_columns < 1) color_columns = 1;

	xv_set(inst->propwin,
				FRAME_PROPS_CREATE_ITEM,
					FRAME_PROPS_ITEM_SPEC,
						&color_choice, FRAME_PROPS_MOVE, PANEL_CHOICE,
					PANEL_DISPLAY_LEVEL, PANEL_CURRENT,
					PANEL_INACTIVE, ! want_color,
					PANEL_CHOICE_NCOLS, color_columns,
					PANEL_LABEL_STRING, XV_MSG("Foreground Color:"),
					XV_HELP_DATA, "windowprops:xvwp_fore_color",
					FRAME_PROPS_DATA_OFFSET, OFF(fore.indx),
					NULL,
				NULL);

	xv_set(xv_get(color_choice, PANEL_ITEM_MENU),
				XV_HELP_DATA, "windowprops:xvwp_fore_color_menu",
				NULL);

	if (! want_color) {
		xv_set(color_choice, PANEL_CHOICE_IMAGES, clip, XV_NULL, NULL);
	}
	else {
		int i;

		for (i = fore_start; i < total_num_colors; i++) {
			Pixmap pix;

			images[i] = xv_create(screen, SERVER_IMAGE,
					SERVER_IMAGE_CMS, cms,
					XV_WIDTH, CHIP_WIDTH,
					XV_HEIGHT, CHIP_HEIGHT,
					SERVER_IMAGE_DEPTH, xv_get(pan, XV_DEPTH),
					NULL);

			pix = (Pixmap)xv_get(images[i], XV_XID);

			XFillRectangle(dpy, pix, bggc, 0, 0, CHIP_WIDTH, CHIP_HEIGHT);
			XSetForeground(dpy, gc, xv_get(cms, CMS_PIXEL,
							CMS_CONTROL_COLORS+i));
			XFillRectangle(dpy, pix, gc, 0, 0, CHIP_WIDTH, CHIP_HEIGHT);
			xv_set(color_choice,
					PANEL_CHOICE_IMAGE, i-fore_start, images[i],
					NULL);
		}
		images[i] = XV_NULL;

		xv_set(inst->propwin,
				XV_KEY_DATA, SERVER_IMAGE_BITS, images,
				XV_KEY_DATA_REMOVE_PROC, SERVER_IMAGE_BITS, destroy_images,
				NULL);
		XFreeGC(dpy, gc);
		XFreeGC(dpy, bggc);
	}

	xv_set(inst->propwin,
				FRAME_PROPS_CREATE_ITEM,
					FRAME_PROPS_ITEM_SPEC, NULL, -1, PANEL_CHOICE,
					PANEL_LABEL_STRING, XV_MSG("Initial State:"),
					PANEL_CHOICE_STRINGS,
						XV_MSG("Window"),
						XV_MSG("Icon"),
						NULL,
					XV_HELP_DATA, "windowprops:xvwp_initial_state",
					FRAME_PROPS_DATA_OFFSET, OFF(closed),
					NULL,
				FRAME_PROPS_CREATE_ITEM,
					FRAME_PROPS_ITEM_SPEC, &p_meas, -1, PANEL_CHOICE,
					PANEL_DISPLAY_LEVEL, PANEL_CURRENT,
					PANEL_LABEL_STRING, XV_MSG("Measure in:"),
					PANEL_NOTIFY_PROC, note_measure_in,
					FRAME_PROPS_DATA_OFFSET, OFF(measure_in),
					NULL,
				FRAME_PROPS_CREATE_ITEM,
					FRAME_PROPS_ITEM_SPEC,&inst->p_pos_def_spec,-1,PANEL_CHOICE,
					PANEL_LABEL_STRING, XV_MSG("Initial Location:"),
					XV_HELP_DATA, "windowprops:xvwp_pos_def_spec",
					PANEL_CHOICE_STRINGS,
						XV_MSG("Default"),
						XV_MSG("Specified"),
						NULL,
					PANEL_LAYOUT, PANEL_HORIZONTAL,
					PANEL_CHOICE_NCOLS, 1,
					PANEL_NOTIFY_PROC, note_pos_def_spec,
					FRAME_PROPS_DATA_OFFSET, OFF(init_loc_set),
					NULL,
				FRAME_PROPS_CREATE_ITEM,
					FRAME_PROPS_ITEM_SPEC,
						&inst->p_x_pos, FRAME_PROPS_MOVE, PANEL_TEXT,
					XV_INSTANCE_NAME, "wp_xpos",
					XV_SHOW, FALSE,
					PANEL_VALUE, "0",
					PANEL_VALUE_DISPLAY_LENGTH, 6,
					PANEL_VALUE_STORED_LENGTH, 5,
					PANEL_LABEL_FONT, xv_get(pan, XV_FONT),
					PANEL_LABEL_STRING, "x =",
					PANEL_NOTIFY_PROC, note_numeric_field,
					PANEL_NOTIFY_LEVEL, PANEL_ALL,
					XV_HELP_DATA, "windowprops:xvwp_xpos",
					FRAME_PROPS_CONVERTER, convert_num_string, inst,
					FRAME_PROPS_DATA_OFFSET, OFF(x),
					NULL,
				FRAME_PROPS_CREATE_ITEM,
					FRAME_PROPS_ITEM_SPEC,
						&inst->p_y_pos, FRAME_PROPS_MOVE, PANEL_TEXT,
					XV_INSTANCE_NAME, "wp_ypos",
					XV_SHOW, FALSE,
					PANEL_ITEM_X_GAP, 0L,
					PANEL_VALUE, "0",
					PANEL_VALUE_DISPLAY_LENGTH, 6,
					PANEL_VALUE_STORED_LENGTH, 5,
					PANEL_LABEL_FONT, xv_get(pan, XV_FONT),
					PANEL_LABEL_STRING, " , y =",
					PANEL_NOTIFY_PROC, note_numeric_field,
					PANEL_NOTIFY_LEVEL, PANEL_ALL,
					XV_HELP_DATA, "windowprops:xvwp_ypos",
					FRAME_PROPS_CONVERTER, convert_num_string, inst,
					FRAME_PROPS_DATA_OFFSET, OFF(y),
					NULL,
				FRAME_PROPS_CREATE_ITEM,
					FRAME_PROPS_ITEM_SPEC,
						&inst->p_initwid, -1, PANEL_NUMERIC_TEXT,
					XV_INSTANCE_NAME, "wp_initwid",
					PANEL_LABEL_STRING, XV_MSG("Initial Width:"),
					PANEL_VALUE_DISPLAY_LENGTH, 4,
					PANEL_MIN_VALUE, 0,
					PANEL_MAX_VALUE, 2000,
					XV_HELP_DATA, "windowprops:xvwp_initwid",
					FRAME_PROPS_DATA_OFFSET, OFF(width),
					FRAME_PROPS_CONVERTER, convert_unit_int, inst,
					NULL,
				FRAME_PROPS_CREATE_ITEM,
					FRAME_PROPS_ITEM_SPEC,
						&inst->p_inithig, -1, PANEL_NUMERIC_TEXT,
					XV_INSTANCE_NAME, "wp_initheight",
					PANEL_LABEL_STRING, XV_MSG("Initial Height:"),
					PANEL_VALUE_DISPLAY_LENGTH, 4,
					PANEL_MIN_VALUE, 0,
					PANEL_MAX_VALUE, 2000,
					XV_HELP_DATA, "windowprops:xvwp_inithig",
					FRAME_PROPS_DATA_OFFSET, OFF(height),
					FRAME_PROPS_CONVERTER, convert_unit_int, inst,
					NULL,
				NULL);

	xv_set(inst->propwin,
				FRAME_PROPS_CREATE_ITEM,
					FRAME_PROPS_ITEM_SPEC,&inst->p_pos_icon_def_spec,-1,PANEL_CHOICE,
					PANEL_LABEL_STRING, XV_MSG("Icon Location:"),
					XV_HELP_DATA, "windowprops:xvwp_pos_icon_def_spec",
					PANEL_CHOICE_STRINGS,
						XV_MSG("Default"),
						XV_MSG("Specified"),
						NULL,
					PANEL_LAYOUT, PANEL_HORIZONTAL,
					PANEL_CHOICE_NCOLS, 1,
					PANEL_NOTIFY_PROC, note_pos_icon_def_spec,
					FRAME_PROPS_DATA_OFFSET, OFF(init_icon_loc_set),
					NULL,
				FRAME_PROPS_CREATE_ITEM,
					FRAME_PROPS_ITEM_SPEC,
						&inst->p_icon_x_pos, FRAME_PROPS_MOVE, PANEL_TEXT,
					XV_INSTANCE_NAME, "wp_iconxpos",
					XV_SHOW, FALSE,
					PANEL_VALUE, "0",
					PANEL_VALUE_DISPLAY_LENGTH, 6,
					PANEL_VALUE_STORED_LENGTH, 5,
					PANEL_LABEL_FONT, xv_get(pan, XV_FONT),
					PANEL_LABEL_STRING, "x =",
					PANEL_NOTIFY_PROC, note_numeric_field,
					PANEL_NOTIFY_LEVEL, PANEL_ALL,
					XV_HELP_DATA, "windowprops:xvwp_iconxpos",
					FRAME_PROPS_CONVERTER, convert_num_string, inst,
					FRAME_PROPS_DATA_OFFSET, OFF(icon_x),
					NULL,
				FRAME_PROPS_CREATE_ITEM,
					FRAME_PROPS_ITEM_SPEC,
						&inst->p_icon_y_pos, FRAME_PROPS_MOVE, PANEL_TEXT,
					XV_INSTANCE_NAME, "wp_iconypos",
					XV_SHOW, FALSE,
					PANEL_ITEM_X_GAP, 0L,
					PANEL_VALUE, "0",
					PANEL_VALUE_DISPLAY_LENGTH, 6,
					PANEL_VALUE_STORED_LENGTH, 5,
					PANEL_LABEL_FONT, xv_get(pan, XV_FONT),
					PANEL_LABEL_STRING, " , y =",
					PANEL_NOTIFY_PROC, note_numeric_field,
					PANEL_NOTIFY_LEVEL, PANEL_ALL,
					XV_HELP_DATA, "windowprops:xvwp_iconypos",
					FRAME_PROPS_CONVERTER, convert_num_string, inst,
					FRAME_PROPS_DATA_OFFSET, OFF(icon_y),
					NULL,
				NULL);

	xv_set(inst->propwin,
				FRAME_PROPS_CREATE_ITEM,
					FRAME_PROPS_ITEM_SPEC, NULL, -1, PANEL_MESSAGE,
					PANEL_LABEL_STRING, XV_MSG("Record Current State:"),
					PANEL_LABEL_BOLD, TRUE,
					XV_HELP_DATA, "windowprops:xvwp_record",
					NULL,
				FRAME_PROPS_CREATE_ITEM,
					FRAME_PROPS_ITEM_SPEC, NULL, FRAME_PROPS_MOVE, PANEL_BUTTON,
					PANEL_LABEL_STRING, XV_MSG("Base Window"),
					PANEL_NOTIFY_PROC, note_record_base,
					XV_HELP_DATA, "windowprops:xvwp_record_base",
					NULL,
/* 				FRAME_PROPS_CREATE_ITEM, */
/* 					FRAME_PROPS_ITEM_SPEC, NULL, 6, PANEL_MESSAGE, */
/* 					PANEL_LABEL_STRING, "  ", */
/* 					0, */
				FRAME_PROPS_CREATE_ITEM,
					FRAME_PROPS_ITEM_SPEC, NULL, FRAME_PROPS_MOVE, PANEL_BUTTON,
					PANEL_LABEL_STRING, XV_MSG("Menus"),
					PANEL_NOTIFY_PROC, note_record_menus,
					XV_HELP_DATA, "windowprops:xvwp_record_menus",
					NULL,
				FRAME_PROPS_CREATE_ITEM,
					FRAME_PROPS_ITEM_SPEC, &popuplab, 6, PANEL_MESSAGE,
					PANEL_LABEL_STRING, "  ",
					NULL,
				FRAME_PROPS_CREATE_ITEM,
					FRAME_PROPS_ITEM_SPEC, NULL, FRAME_PROPS_MOVE, PANEL_BUTTON,
					PANEL_LABEL_STRING, XV_MSG("Pop-up Windows"),
					PANEL_NOTIFY_PROC, note_record_popup,
					XV_HELP_DATA, "windowprops:xvwp_record_popup",
					NULL,
				FRAME_PROPS_CREATE_ITEM,
					FRAME_PROPS_ITEM_SPEC, NULL, FRAME_PROPS_MOVE, PANEL_MESSAGE,
					PANEL_LABEL_STRING, XV_MSG("relative to"),
					NULL,
				FRAME_PROPS_CREATE_ITEM,
					FRAME_PROPS_ITEM_SPEC, &relc,FRAME_PROPS_MOVE,PANEL_CHOICE,
					PANEL_CHOICE_NCOLS, 1,
					PANEL_CHOICE_STRINGS,
						XV_MSG("Workspace"),
						XV_MSG("Base Window"),
						NULL,
					PANEL_NOTIFY_PROC, update_changebar,
					XV_HELP_DATA, "windowprops:xvwp_pos_relative",
					FRAME_PROPS_DATA_OFFSET, OFF(popup_relative),
					NULL,
				FRAME_PROPS_CREATE_ITEM,
					FRAME_PROPS_ITEM_SPEC, NULL, -1, PANEL_CHOICE,
					PANEL_LABEL_STRING, XV_MSG("Manage Windows:"),
					PANEL_CHOICE_STRINGS,
						XV_MSG("Independently"),
						XV_MSG("As a Group"),
						NULL,
					XV_HELP_DATA, "windowprops:xvwp_manage",
					FRAME_PROPS_DATA_OFFSET, OFF(manage_wins),
					NULL,
				FRAME_PROPS_CREATE_ITEM,
					FRAME_PROPS_ITEM_SPEC, NULL, -1, PANEL_SLIDER,
					XV_INSTANCE_NAME, "wp_basescale",
					PANEL_LABEL_STRING, XV_MSG("Base Window Scale:"),
					PANEL_MIN_TICK_STRING, XV_MSG("Small"),
					PANEL_MAX_TICK_STRING, XV_MSG("Large   Dflt"),
					PANEL_MIN_VALUE, WIN_SCALE_SMALL,
					PANEL_MAX_VALUE, WIN_SCALE_DEFAULT,
					PANEL_SHOW_VALUE, FALSE,
					PANEL_SHOW_RANGE, FALSE,
					PANEL_TICKS, 5,
					PANEL_VALUE_DISPLAY_LENGTH, 20,
					XV_HELP_DATA, "windowprops:xvwp_base_scale",
					FRAME_PROPS_DATA_OFFSET, OFF(base_scale),
					NULL,
				FRAME_PROPS_CREATE_ITEM,
					FRAME_PROPS_ITEM_SPEC, NULL, -1, PANEL_SLIDER,
					XV_INSTANCE_NAME, "wp_popuscale",
					PANEL_LABEL_STRING, XV_MSG("Pop-up Windows Scale:"),
					PANEL_MIN_TICK_STRING, XV_MSG("Small"),
					PANEL_MAX_TICK_STRING, XV_MSG("Large   Dflt"),
					PANEL_MIN_VALUE, WIN_SCALE_SMALL,
					PANEL_MAX_VALUE, WIN_SCALE_DEFAULT,
					PANEL_SHOW_VALUE, FALSE,
					PANEL_SHOW_RANGE, FALSE,
					PANEL_TICKS, 5,
					PANEL_VALUE_DISPLAY_LENGTH, 20,
					XV_HELP_DATA, "windowprops:xvwp_popup_scale",
					FRAME_PROPS_DATA_OFFSET, OFF(popup_scale),
					NULL,
				FRAME_PROPS_ALIGN_ITEMS,
				NULL);

	if (xv_get(srv, XV_WANT_ROWS_AND_COLUMNS)) {
		xv_set(xv_get(p_meas, PANEL_ITEM_MENU),
				XV_HELP_DATA, "windowprops:xvwp_measure_menu_rc",
				NULL);

		xv_set(p_meas,
				PANEL_CHOICE_STRINGS, XV_MSG("font units"), NULL,
				XV_HELP_DATA, "windowprops:xvwp_measure_in_rc",
				NULL);
	}
	else {
		xv_set(xv_get(p_meas, PANEL_ITEM_MENU),
				XV_HELP_DATA, "windowprops:xvwp_measure_menu",
				NULL);

		xv_set(p_meas,
				XV_HELP_DATA, "windowprops:xvwp_measure_in",
				PANEL_CHOICE_STRINGS,
					XV_MSG("pixels"),
					XV_MSG("millimeters"),
					NULL,
				NULL);
	}

	r = (Rect *)xv_get(inst->p_pos_def_spec, XV_RECT);
	xv_set(inst->p_x_pos,
			XV_Y, r->r_top + r->r_height / 2 + 4,
			FRAME_PROPS_CHANGEBAR,
							xv_get(inst->p_pos_def_spec,FRAME_PROPS_CHANGEBAR),
			NULL);
	xv_set(inst->p_y_pos,
			XV_Y, r->r_top + r->r_height / 2 + 4,
			FRAME_PROPS_CHANGEBAR,
							xv_get(inst->p_pos_def_spec,FRAME_PROPS_CHANGEBAR),
			NULL);

	r = (Rect *)xv_get(inst->p_pos_icon_def_spec, XV_RECT);
	xv_set(inst->p_icon_x_pos,
			XV_Y, r->r_top + r->r_height / 2 + 4,
			FRAME_PROPS_CHANGEBAR,
						xv_get(inst->p_pos_icon_def_spec,FRAME_PROPS_CHANGEBAR),
			NULL);
	xv_set(inst->p_icon_y_pos,
			XV_Y, r->r_top + r->r_height / 2 + 4,
			FRAME_PROPS_CHANGEBAR,
						xv_get(inst->p_pos_icon_def_spec,FRAME_PROPS_CHANGEBAR),
			NULL);

	xv_set(relc,FRAME_PROPS_CHANGEBAR,xv_get(popuplab,FRAME_PROPS_CHANGEBAR),NULL);

	xv_set(inst->propwin,
				FRAME_PROPS_CREATE_BUTTONS,
					FRAME_PROPS_APPLY, "windowprops:xvwp_apply",
					FRAME_PROPS_RESET, "windowprops:xvwp_reset",
					NULL,
				NULL);

	xv_set(inst->propwin, FRAME_PROPS_TRIGGER, FRAME_PROPS_RESET, NULL);

	if (inst->wm != WM_OLWM) {
		XSetTransientForHint((Display *)xv_get(inst->propwin, XV_DISPLAY),
								(Window)xv_get(inst->propwin, XV_XID),
								(Window)xv_get(inst->data.base, XV_XID));
	}

	window_fit(pan);
	window_fit(inst->propwin);
}

Pkg_private void server_xvwp_install(Frame base)
{
	Xv_server srv = XV_SERVER_FROM_WINDOW(base);
	Server_info *srvpriv = SERVER_PRIVATE(srv);
	xvwp_t *inst = srvpriv->xvwp;
	char *app, buf[200];
	Display *dpy;
	Window xid;
	Xv_screen screen;
	screen_ui_style_t ui_style;
	Atom atoms[8], show_props_atom;
	int want_color;
	int cnt = 0;

	if (inst->installed) return;

	inst->installed = TRUE;
    srvpriv->top_level_win = base;

	dpy = (Display *)xv_get(base, XV_DISPLAY);
	xid = (Window)xv_get(base, XV_XID);
	screen = XV_SCREEN_FROM_WINDOW(base);
	ui_style = (screen_ui_style_t)xv_get(screen, SCREEN_UI_STYLE);
	want_color = (ui_style != SCREEN_UIS_2D_BW);


	xvwp_set_base_position(base);

	process_ui_registrations(regui, base);
	xv_set(srv, QUEUE_REQUESTS, FALSE, NULL);

	/* free desktop stuff: */
	atoms[cnt++] = W_ATOM(base, "_NET_WM_PING");

	/* only my modified olwm will understand that ! */
	atoms[cnt++] = show_props_atom = W_ATOM(base, "_OL_SHOW_PROPS");
	atoms[cnt++] = W_ATOM(base, _OL_OWN_HELP);

	if (inst->data.manage_wins) {
		atoms[cnt++] = W_ATOM(base, "_OL_GROUP_MANAGED");
	}

	XChangeProperty(dpy, xid, W_ATOM(base, "WM_PROTOCOLS"),
					XA_ATOM, 32, PropModeAppend, (unsigned char *)atoms, cnt);

	/*** PROPERTY_DEFINITION ***********************************************
	 * _OL_SET_WIN_MENU_DEFAULT        Type INTEGER        Format 32
	 * Owner: client, Reader: wm
	 */
	XChangeProperty(dpy, xid, W_ATOM(base, "_OL_SET_WIN_MENU_DEFAULT"),
						XA_INTEGER, 32, PropModeReplace,
						(unsigned char *)&inst->data.win_menu_default, 1);

	if (want_color && (inst->data.back.indx != 0 ||
		inst->data.fore.indx != 0 || inst->argv_contains.fg))
	{
		int myind;

		/* we have special color requirements ->
		 * set the window colors property on the main window
		 */

		/*************************************************************
		 ***     The following is copied from  <Xol/OlClients.h>   ***
		 ***     If olwm will ever support the window color        ***
		 ***     it will (hopefully) be in this way.               ***
		 *************************************************************/

		typedef struct {
			unsigned long	flags;
			unsigned long	fg_red;
			unsigned long	fg_green;
			unsigned long	fg_blue;
			unsigned long	bg_red;
			unsigned long	bg_green;
			unsigned long	bg_blue;
			unsigned long	bd_red;
			unsigned long	bd_green;
			unsigned long	bd_blue;
		} OLWinColors;

#       define _OL_WC_FOREGROUND (1<<0)
#       define _OL_WC_BACKGROUND (1<<1)
#       define _OL_WC_BORDER (1<<2)

		OLWinColors win_colors;

		memset((char *)&win_colors, 0, sizeof(win_colors));

		if (inst->data.back.indx != 0) {
			myind = inst->data.back.indx;

			win_colors.flags |= _OL_WC_BACKGROUND;
			win_colors.bg_red = (my_colors[myind].red << 8);
			win_colors.bg_green = (my_colors[myind].green << 8);
			win_colors.bg_blue = (my_colors[myind].blue << 8);
		}

		if (inst->argv_contains.fg || inst->data.fore.indx != 0) {
			myind = fore_start + inst->data.fore.indx;
			if (inst->argv_contains.fg) myind = total_num_colors - 1;

			win_colors.flags |= _OL_WC_FOREGROUND;
			win_colors.fg_red = (my_colors[myind].red << 8);
			win_colors.fg_green = (my_colors[myind].green << 8);
			win_colors.fg_blue = (my_colors[myind].blue << 8);
		}

		XChangeProperty(dpy, xid, W_ATOM(base, "_OL_WIN_COLORS"),
					W_ATOM(base, "_OL_WIN_COLORS"), 32, PropModeReplace,
					(unsigned char *)&win_colors,
					(int)(sizeof(win_colors)/sizeof(win_colors.flags)));
	}

	if (inst->wm == WM_MWM) {
		char mwmbuf[100];
		Atom mwmmenu = W_ATOM(base, "_MOTIF_WM_MENU"),
				mwmmess = W_ATOM(base, "_MOTIF_WM_MESSAGES");

		XChangeProperty(dpy, xid, mwmmess,
						XA_ATOM, 32, PropModeAppend,
						(unsigned char *)&show_props_atom, 1);

		sprintf(mwmbuf, "%s f.send_msg %ld", XV_MSG("Properties..."),
						show_props_atom);
		XChangeProperty(dpy, xid, mwmmenu, mwmmenu, 8, PropModeReplace,
						(unsigned char *)mwmbuf, (int)strlen(mwmbuf) + 1);

		XChangeProperty(dpy, xid, W_ATOM(base, "WM_PROTOCOLS"),
						XA_ATOM, 32, PropModeAppend,
						(unsigned char *)&mwmmess, 1);
	}

	inst->data.base = base;

	app = (char *)xv_get(srv, XV_APP_NAME);
	if (! app) app = (char *)xv_get(base, XV_LABEL);
	if (! app) app = "no app name ???";

	sprintf(buf, "%s: %s", app, XV_MSG("Window Properties"));
	inst->propwin = xv_create(base, FRAME_PROPS,
				XV_LABEL, buf,
				XV_INSTANCE_NAME, "windowprops",
				XV_HELP_DATA, "windowprops:winpropwin",
				FRAME_SHOW_FOOTER, TRUE,
				FRAME_PROPS_APPLY_PROC, note_apply,
				FRAME_PROPS_RESET_PROC, note_reset,
				FRAME_PROPS_DATA_ADDRESS, &inst->data,
				FRAME_PROPS_CREATE_CONTENTS_PROC, fill_me,
				XV_KEY_DATA, XV_DEPTH, want_color,
				XV_SET_POPUP, XV_AUTO_CREATE, XV_SHOW, XV_WIDTH, NULL,
				NULL);

	xv_set(base,
				WIN_CONSUME_EVENTS, WIN_REPAINT, ACTION_HELP, NULL,
				WIN_NOTIFY_SAFE_EVENT_PROC, base_interposer,
#if RESCALE_WORKS
				FRAME_SCALE_STATE, inst->data.use_base_scale,
#endif
				/* IMPORTANT to avoid the passive button grab: */
				WIN_IGNORE_X_EVENT_MASK, FocusChangeMask,
				NULL);
}

static void server_xvwp_configure_popup(Frame popup)
{
	Xv_server srv = XV_SERVER_FROM_WINDOW(popup);
	Server_info *srvpriv = SERVER_PRIVATE(srv);
	xvwp_t *inst = srvpriv->xvwp;
	xvwp_popup_t p;
	int base_x = 0, base_y = 0;

	if (popup == inst->propwin) return;

	p = inst->data.popups;
	while (p) {
		if (p->frame == popup) {
			if (inst->data.popup_relative) {

				if (inst->data.base) {
					Rect r;

					frame_get_rect(inst->data.base, &r);
					base_x = r.r_left;
					base_y = r.r_top;
				}
				else {
					base_x = 0;
					base_y = 0;
				}
			}

			configure_popup(p, base_x, base_y);
			return;
		}

		p = p->next;
	}

	fprintf(stderr, "xvwp_configure_popup: cannot find '%s'\n",
									(char *)xv_get(popup, XV_INSTANCE_NAME));
}

/* formerly: xvwp_set_popup(Frame popup, ...); */

Xv_private void server_set_popup(Frame popup, Attr_attribute *attrs)
{
	Xv_server srv = XV_SERVER_FROM_WINDOW(popup);
	Server_info *srvpriv = SERVER_PRIVATE(srv);
	xvwp_t *inst = srvpriv->xvwp;
	xvwp_popup_t p;
	char *name = (char *)xv_get(popup, XV_INSTANCE_NAME);
	char pref[1000];
	Frame top;

	if (popup == inst->propwin) return;

	sprintf(pref, "%s%s.", inst->xvwp_prefix, name);

	p = inst->data.popups;
	while (p) {
		if (p->frame == popup) break;
		p = p->next;
	}
	if (! p) {
		p = (xvwp_popup_t)xv_alloc(struct _xvwp_popup);
		memset((char *)p, 0, sizeof(struct _xvwp_popup));
		p->name = xv_strsave(pref);
		p->frame = popup;

		xv_set(popup,
				XV_KEY_DATA, xvwp_popup_key, p,
				XV_KEY_DATA_REMOVE_PROC, xvwp_popup_key, note_popup_dying,
				NULL);

		p->next = inst->data.popups;
		inst->data.popups = p;

		p->delayed_action = ~0;
	}

	/* *attrs ist Attr_attribute, also 64bit - wir wollen hier
	 * nue 32bit abfragen
	 */
	while ((int)*attrs) {
		switch ((int)*attrs) {
			case XV_SHOW: p->delayed_action &= (~ BIT_SHOW); break;
			case XV_X: case XV_Y: p->delayed_action &= (~ BIT_POS); break;
			case XV_WIDTH: case XV_HEIGHT: p->delayed_action &= (~ BIT_SIZE); break;
			case FRAME_CMD_PIN_STATE: p->delayed_action &= (~ BIT_PIN); break;
			case XV_AUTO_CREATE: p->is_incomplete = TRUE; break;
		}
		++attrs;
	}

	if (! xv_get(popup, FRAME_SHOW_RESIZE_CORNER)) {
		p->delayed_action &= (~ BIT_SIZE);
	}

	Permprop_res_read_dbs(inst->xvwp_xrmdb, pref, (char *)p,
					res_popup, (unsigned)PERM_NUMBER(res_popup));

	if (inst->wm != WM_OLWM && ! xv_get(popup, XV_IS_SUBTYPE_OF, FRAME_BASE)) {
		top = xv_get(popup, XV_OWNER);
		while (top) {
			if (xv_get(top, XV_IS_SUBTYPE_OF, FRAME_BASE)) {
				XSetTransientForHint((Display *)xv_get(popup, XV_DISPLAY),
									(Window)xv_get(popup, XV_XID),
									(Window)xv_get(top, XV_XID));
				break;
			}
			top = xv_get(top, XV_OWNER);
		}
	}

	if (inst->never_again_handle_delayed) {
		server_xvwp_configure_popup(popup);
	}
	else {
		inst->must_handle_delayed_popup = TRUE;
	}
}

static Notify_error sbmenu_destruction(Menu menu, Destroy_status status)
{
	if (status == DESTROY_CLEANUP) {
		xvwp_menu_t p = (xvwp_menu_t)xv_get(menu, XV_KEY_DATA, xvwp_key);

		if (p) {
			/* schon verschwunden, siehe (jkwgehrfw3f) */
			p->menu = XV_NULL;
		}
	}

	return notify_next_destroy_func(menu, status);
}

static void server_set_any_menu(Menu menu, const char *name,
					Xv_opaque panelitem_window_or_server)
{
	Xv_server srv = xv_default_server;
	Server_info *srvpriv;
	xvwp_menu_t p;
	char pref[1000];
	xvwp_t *inst;
	Xv_window win = XV_NULL;

	if (xv_get(panelitem_window_or_server, XV_IS_SUBTYPE_OF, SERVER)) {
		srv = panelitem_window_or_server;
	}
	else if (xv_get(panelitem_window_or_server, XV_IS_SUBTYPE_OF, WINDOW)) {
		win = panelitem_window_or_server;
		srv = XV_SERVER_FROM_WINDOW(win);
	}
	else if (xv_get(panelitem_window_or_server, XV_IS_SUBTYPE_OF, PANEL_ITEM)) {
		win = xv_get(panelitem_window_or_server,XV_OWNER);
		srv =XV_SERVER_FROM_WINDOW(win);
	}

	srvpriv = SERVER_PRIVATE(srv);

	inst = srvpriv->xvwp;
	if (! win) win = inst->data.base;

	sprintf(pref, "%s.%s.", xv_instance_app_name, name);

	for (p = inst->data.menus; p; p = p->next) {
		if (p->menu == menu) {
			if (getenv("XV_DRA_MENU_ABORT")) {
				if (0 == strcmp(p->name, pref)) {
					/* gleiches Menue, gleicher Name */
					fprintf(stderr, "%s: %s-%s(%s) same menu registered twice\n",
							xv_instance_app_name, __FILE__, __FUNCTION__, name);
				}
				else {
					fprintf(stderr, "%s: %s-%s(%s - %s) different names for same menu\n",
							xv_instance_app_name,__FILE__,__FUNCTION__,
							p->name,name);
				}
				abort();
			}
			return;
		}
	}
	/* same menu, but different instance names: no problem, they will
	 * all be configured in the same way.
	 */

	p = inst->data.menus;
	p = (xvwp_menu_t)xv_alloc(struct _xvwp_menu);
	memset((char *)p, 0, sizeof(struct _xvwp_menu));
	p->name = xv_strsave(pref);
	p->menu = menu;

	p->next = inst->data.menus;
	inst->data.menus = p;

	if (strlen(name) > 6) {
		if (0 == strcmp("SBMenu", name + strlen(name) - 6)) {
			/* das ist ein Scrollbar-Menu - die koennen (join views)
			 * einfach verschwinden...
			 */
			
			xv_set(menu, XV_KEY_DATA, xvwp_key, p, NULL);
			notify_interpose_destroy_func(menu, sbmenu_destruction);
		}
	}
	Permprop_res_read_dbs(inst->xvwp_xrmdb, pref, (char *)p,
					res_menu, (unsigned)PERM_NUMBER(res_menu));

	if (inst->never_again_handle_delayed) {
		configure_menu(p, win, 0, 0);
	}
	else {
		inst->must_handle_delayed_popup = TRUE;
	}
}

/* this function is called (at least) from om_set.c`menu_sets from
 * the XV_SET_MENU case 
 */
Xv_private void server_set_menu(Menu menu, Xv_opaque panelitem_window_or_server)
{
	char *instnam = (char *)xv_get(menu, XV_INSTANCE_NAME);

	if (instnam && *instnam) {
		server_set_any_menu(menu, instnam, panelitem_window_or_server);
	}
	else {
		if (getenv("XV_DRA_MENU_ABORT")) {
			fprintf(stderr, "%s: %s: have a menu without instance name\n",
							xv_instance_app_name, __FUNCTION__);
			abort();
		}
		server_set_any_menu(menu, "anonymous_menu", panelitem_window_or_server);
	}
}

Pkg_private void server_show_propwin(Server_info *srvpriv)
{
	xvwp_t *inst = srvpriv->xvwp;

	popup_propwin(inst, xv_get(inst->propwin, XV_OWNER));
}

Pkg_private Xv_opaque *server_xvwp_get_db(Server_info *srvpriv)
{
	return srvpriv->xvwp->xvwp_xrmdb;
}

Pkg_private void server_xvwp_write_file(Server_info *srvpriv)
{
	internal_write_file(srvpriv->xvwp);
}

Pkg_private void server_appl_set_busy(Server_info *srvpriv, int busy, Frame except)
{
	xvwp_t *inst = srvpriv->xvwp;
	xvwp_popup_t p;

	if (inst->data.base && inst->data.base != except) {
		xv_set(inst->data.base, FRAME_BUSY, busy, NULL);
	}

/* MIGHT_BE_FALSE_WHEN_BUSY_IS_FALSE_BUT_WAS_TRUE_BEFORE */
	for (p = inst->data.popups; p; p = p->next) {
		if (except != p->frame
				&& p->frame
				&& xv_get(p->frame, XV_SHOW)
				)
		{
			xv_set(p->frame, FRAME_BUSY, busy, NULL);
		}
	}

	for (p = inst->secondaries; p; p = p->next) {
		if (except != p->frame
				&& p->frame
				&& xv_get(p->frame, XV_SHOW)
				)
		{
			xv_set(p->frame, FRAME_BUSY, busy, NULL);
		}
	}
}

void server_set_base_font(Frame frame)
{
	Xv_server srv = XV_SERVER_FROM_WINDOW(frame);
	Server_info *srvpriv = SERVER_PRIVATE(srv);
	xvwp_t *inst = srvpriv->xvwp;

	if (inst->font_for_root_and_frame) {
		xv_set(frame, XV_FONT, inst->font_for_root_and_frame, NULL);
	}
}

static Notify_value secondary_base_interposer(Frame f, Notify_event nev,
							Notify_arg arg, Notify_event_type type)
{
	if (server_xvwp_is_own_help(XV_SERVER_FROM_WINDOW(f), f, (Event *)nev))
		return NOTIFY_DONE;

	return notify_next_event_func(f, nev, arg, type);
}

Xv_private void server_register_secondary_base(Xv_server srv, Frame sec,
											Frame baseframe)
{
	Display *dpy = (Display *)xv_get(srv, XV_DISPLAY);
	Window basexid, secwin = xv_get(sec, XV_XID);
	Server_info *srvpriv = SERVER_PRIVATE(srv);
	xvwp_t *inst = srvpriv->xvwp;
	Atom at;
	xvwp_popup_t p;

	basexid = (Window)xv_get(baseframe, XV_XID);
	at = xv_get(srv, SERVER_ATOM, "_OL_COLORS_FOLLOW");

	XChangeProperty(dpy, secwin, at, XA_WINDOW, 32, PropModeReplace,
					(unsigned char *)&basexid, 1);

	XChangeProperty(dpy, secwin, xv_get(srv, SERVER_ATOM, "WM_CLIENT_LEADER"),
					XA_WINDOW, 32,PropModeReplace, (unsigned char *)&basexid, 1);

	at = xv_get(srv, SERVER_ATOM, _OL_OWN_HELP);
	XChangeProperty(dpy, secwin, xv_get(srv, SERVER_ATOM, "WM_PROTOCOLS"),
					XA_ATOM, 32, PropModeAppend, (unsigned char *)&at, 1);

	notify_interpose_event_func(sec, secondary_base_interposer, NOTIFY_SAFE);

	p = (xvwp_popup_t)xv_alloc(struct _xvwp_popup);
	p->frame = sec;
	p->name = xv_strsave(" ");  /* wegen note_popup_dying */

	xv_set(sec,
			XV_KEY_DATA, xvwp_popup_key, p,
			XV_KEY_DATA_REMOVE_PROC, xvwp_popup_key, note_popup_dying,
			NULL);

	p->next = inst->secondaries;
	inst->secondaries = p;
}

Xv_private void server_update_aqt(Server_info *server, int count)
{
	xvwp_t *inst = server->xvwp;

	if (! inst) {
		fprintf(stderr, "%s: %s-%d: cannot save avoid_query_tree_count %d\n",
						xv_instance_app_name, __FILE__, __LINE__, count);
		return;
	}

	inst->data.avoid_query_tree_count = count;

	/* ref (chjbvhjsdfgv) */
	Permprop_res_update_dbs(inst->xvwp_xrmdb, inst->xvwp_prefix,
					(char *)&inst->data,
					res_base + PERM_NUMBER(res_base) - 2, 1);
	Permprop_res_store_db(inst->xvwp_xrmdb[PRC_U], inst->appfiles[PRC_U]);
}
