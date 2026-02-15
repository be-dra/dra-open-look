#include <sys/param.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <dirent.h>
#include <xview/xview.h>
#include <xview/regexpr.h>
#include <xview/help.h>
#include <xview/funckey.h>
#include <xview/scrollbar.h>
#include <xview/panel.h>
#include <xview/accel.h>
#include <xview/cms.h>
#include <xview/font.h>
#include <xview/dircanv.h>
#include <xview/filedrag.h>
#include <xview/filereq.h>
#include <xview/defaults.h>
#include <xview_private/i18n_impl.h>
#include <xview_private/svr_impl.h>

char dircanv_c_sccsid[] = "@(#) %M% V%I% %E% %U% $Id: dircanv.c,v 1.54 2026/02/14 11:38:00 dra Exp $";

typedef struct _dir_priv *protodirpriv;

typedef void (*menu_cb_t)(Menu, Menu_item);
typedef void (*tell_match_proc_t) (Dircanvas, char *, int);

typedef int (*dirsortfunc)(Dir_entry_t *, Dir_entry_t *);

static int dir_key = 0;
static dirsortfunc act_sorting;

typedef enum {
  SNULL, SEL_BACK, SEL_OBJ, ADJ_BACK, ADJ_OBJ, FRAME_SEL, FRAME_ADJ
} dir_states;

typedef struct _dir_priv {

	/* internals */

	Xv_opaque           public_self;
	int                 file_hig, file_wid, height, width, ascent,
						maxwidth, lineheight, multiclick_time, drag_threshold;
	unsigned short      hide_mode;
	dir_states          state;
	int                 cursor_index, selected_index,
						vlast_x, vlast_y, down_x, down_y, vdown_x, vdown_y,
						lines, cols, num_sel;
	time_t              lasttime;
	unsigned            num, visible_num;
	Dir_entry_t         *files, *visibles;
	time_t              mtime_of_dir;
	dirsortfunc         act_sorting;
	dir_progress_t      progress_proc;
	char                *name_before_text_mode;
	char                *filter_ed_pattern, *filter_sh_pattern, *cur_msh,
						*cur_med, *match_sh_pattern, *match_ed_pattern;
	xv_regexp_context   match_context, filter_context;
	Xv_font             font, boldfont;
	Xv_opaque           panel, renametext, dragdrop;
	Menu                hide_menu, sort_menu;
	GC                  frame_gc, copygc;
	GC                  normal_gc, bold_gc, bold_inv_gc, normal_inv_gc;
	char                initialized, matching, auto_rename, is_rename,
						use_ed_filtering, reverse_sorting, use_icon,
						dir_writable, update_running, full_keyboard;

	/* attributes */

	char                line_oriented, choose_one,
						enable_fileprops, select_full_path,
						dir_always_visible, show_dirs, show_dots;
	int                 col_dist, row_dist, args_offset, update_interval,
						max_name_length, icon_dist;
	long                inhibit;
	dir_rootact_t       root_action;
	dir_find_proc_t    find_proc;
	dir_drop_proc_t     drop_proc;
	dir_multi_proc_t    multi_select_proc;
	tell_match_proc_t   tell_match_proc;
	dir_select_proc_t   select_proc;
	dir_syserr_proc_t   syserr_proc;
	dir_props_proc_t    props_proc;
	Dir_layout          layout;
	Xv_opaque           client_data;
	char                *dirname;
} Dir_private;

#define DIRPRIV(_x_) XV_PRIVATE(Dir_private, Xv_dir_public, _x_)
#define DIRPUB(_x_) XV_PUBLIC(_x_)

#define MAXi(a,b) (((a) > (b)) ? (a) : (b))
#define MINi(a,b) (((a) < (b)) ? (a) : (b))
#define ABSo(a) (((a) < 0) ? -(a) : (a))

#define COL_GAP 10

typedef Dir_private *dir_priv_ptr;
typedef Scrollwin_event_struct *dir_event_ptr;

typedef enum {
  SELECT_DOWN, SELECT_UP, ADJUST_DOWN, ADJUST_UP, DRAG, STOP, UNKNOWN_INPUT
} dir_events;

extern Xv_opaque server_get_timestamp(Xv_Server server_public);

static int dirpw_key = 0;


/********************* dircanvas repainting ***************************/

static void cancel_text_mode(Dir_private *priv)
{
	xv_free(priv->name_before_text_mode);
	priv->name_before_text_mode = (char *)0;
	xv_set(priv->panel, XV_SHOW, FALSE, NULL);
}

static void repaint_file_2D(Dir_private *priv, int this, Display *dpy, Window xid, int x, int y)
{
	char *ending, buf[500];
	int stroffset;
	register Dir_entry_t *vis = priv->visibles + this;

	switch (vis->type) {
		case DIR_DIR_LINK:
		case DIR_FILE_LINK:
		case DIR_BROKEN_LINK: ending = "@"; break;
		default: ending = ""; break;
	}

	strcpy(buf, vis->name);
	if ((int)strlen(buf) > priv->max_name_length) {
		strcpy(buf + priv->max_name_length, "\253");
	}
	strcat(buf, ending);

	if (priv->use_icon)
		stroffset = (priv->file_wid - vis->extent) / 2;
	else stroffset = 0;

	if (vis->selected) {
		XFillRectangle(dpy, xid, priv->normal_gc, x, y,
				(unsigned)priv->file_wid, (unsigned)priv->file_hig);
	}
	else {
		XClearArea(dpy, xid, x, y,
				(unsigned)priv->file_wid, (unsigned)priv->file_hig, FALSE);
	}

	if ((int)vis->type<=(int)DIR_DIR_LINK) {
		XDrawString(dpy, xid,
			vis->selected ? priv->bold_inv_gc : priv->bold_gc,
			x + stroffset, y + priv->ascent, buf, (int)strlen(buf));
	}
	else {
		XDrawString(dpy, xid,
			vis->selected ? priv->normal_inv_gc : priv->normal_gc,
			x + stroffset, y + priv->ascent, buf, (int)strlen(buf));
	}

	if (priv->use_icon) {
		XCopyArea(dpy,
				vis->selected ? vis->dscattrs->sel_icon: vis->dscattrs->icon ,
				xid,
				priv->copygc, 0, 0,
				DIR_ICON_WIDTH, DIR_ICON_HEIGHT,
				x + (priv->file_wid - DIR_ICON_WIDTH) / 2,
				y - (DIR_ICON_HEIGHT + priv->icon_dist));
	}
}

static void make_filepos(Dir_private *priv, int this, int *xp, int *yp, Rect *trect, Rect *irect)
{
	if (priv->line_oriented) {
		*xp = priv->maxwidth * (this%priv->cols) + priv->col_dist;
		*yp = priv->lineheight * (1 + (this/priv->cols)) - priv->file_hig;
	}
	else {
		*xp = priv->maxwidth * (this/priv->lines) + priv->col_dist;
		*yp = priv->lineheight * (1 + (this % priv->lines)) - priv->file_hig;
	}

	if (trect) {
		Dir_entry_t *vis = priv->visibles + this;

		trect->r_left = *xp;
		trect->r_top = *yp;
		trect->r_height = priv->file_hig;
		trect->r_width = vis->extent;
		irect->r_height = irect->r_width = 0;

		if (priv->use_icon) {
			trect->r_left += (priv->file_wid - vis->extent) / 2;

			irect->r_height = DIR_ICON_HEIGHT;
			irect->r_width = DIR_ICON_WIDTH;
			irect->r_left = *xp + (priv->file_wid - DIR_ICON_WIDTH) / 2,
			irect->r_top = *yp - (DIR_ICON_HEIGHT + priv->icon_dist);
		}
	}
}

static void dir_repaint_from_expose(Dir_private *priv, Scrollwin_repaint_struct *rs)
{
	Rect fr;
	int scrx, scry, x, y, xoff, yoff, iconsiz;
	int i;
	Display *dpy = rs->vinfo->dpy;
	Window xid = rs->vinfo->xid;

	if (priv->name_before_text_mode && rs->reason != SCROLLWIN_REASON_EXPOSE) {
		cancel_text_mode(priv);
	}

	scrx = rs->vinfo->scr_x;
	scry = rs->vinfo->scr_y;

	if (priv->use_icon) iconsiz = DIR_ICON_HEIGHT + priv->icon_dist;
	else iconsiz = 0;

	xoff = scrx;
	yoff = scry;

	yoff += iconsiz;

	fr.r_width = priv->file_wid;
	fr.r_height = priv->file_hig + iconsiz;

	for (i = 0; i < priv->visible_num; i++) {
		make_filepos(priv, i, &x, &y, (Rect *)0, (Rect *)0);

		fr.r_top = y - yoff;
		fr.r_left = x - xoff;

		if (rect_intersectsrect(&rs->win_rect, &fr))
			repaint_file_2D(priv, i, dpy, xid, x - scrx, y - scry);
	}

	if (priv->full_keyboard && rs->reason == SCROLLWIN_REASON_SCROLL) {
		Xv_window focuswin;

		if (! dirpw_key) dirpw_key = xv_unique_key();
		if ((focuswin = xv_get(rs->pw, XV_KEY_DATA, dirpw_key))) {
			int foc_x, foc_y;

			make_filepos(priv, priv->cursor_index, &foc_x, &foc_y,
												(Rect *)0, (Rect *)0);

			xv_set(focuswin,
					XV_X, foc_x - rs->vinfo->scr_x - FRAME_FOCUS_RIGHT_WIDTH,
					XV_Y, foc_y - rs->vinfo->scr_y,
					NULL);
		}
	}
}

static char **dir_get_all_selected(Dir_private *priv, int fullpath)
{
	unsigned count = 0;
	char **args;
	unsigned int i;

	for (i = 0; i < priv->visible_num; i++) {
		if (priv->visibles[i].selected) ++count;
	}

	if (count) {
		args = (char **)calloc((size_t)(count + priv->args_offset + 1),
										sizeof(char *));
		if (!args) {
			xv_error(DIRPUB(priv),
				ERROR_PKG, DIRCANVAS,
				ERROR_LAYER, ERROR_SYSTEM,
				ERROR_STRING, XV_MSG("calloc returns NULL"),
				ERROR_SEVERITY, ERROR_NON_RECOVERABLE,
				NULL);
			exit(1);
		}

		for (i = 0; i < (unsigned)priv->args_offset; i++) args[i] = (char *)0;

		for (i = 0, count = priv->args_offset; i < priv->visible_num; i++) {
			if (priv->visibles[i].selected)
				args[count++] = xv_strsave(fullpath ?
										priv->visibles[i].path :
										priv->visibles[i].name);
		}

		args[count] = (char *)0;
	}
	else args = (char **)0;

	return args;
}

static void take_file_on_select_button(Dir_private *priv, int shift, int control)
{
	unsigned i;

	if (!priv->select_proc) return;

	for (i = 0; i < priv->visible_num; i++)  {
		if (priv->visibles[i].selected) {
			(*(priv->select_proc))(DIRPUB(priv),
					priv->select_full_path ?
							priv->visibles[i].path :
							priv->visibles[i].name,
					(int)priv->visibles[i].type,
					shift, control,
					priv->visibles + i);

			return;
		}

	}
}

/********************* dircanvas's event handling ***********************/

static int dir_find_index(Dir_private *priv, int x, int y, char *is_rename)
{
	int fx, fy, that;
	Rect trect, irect;

	if (is_rename) *is_rename = FALSE;

	if (!priv->visible_num) return -1; /* empty window */
	if (y > priv->lineheight * priv->lines) return -1;
	if (x > priv->maxwidth * priv->cols) return -1;
	if (x % priv->maxwidth < priv->col_dist) return -1;
	if (y % priv->lineheight < priv->row_dist) return -1;

	if (priv->line_oriented)
		that = (x / priv->maxwidth) + priv->cols * (y / priv->lineheight);
	else that = (x / priv->maxwidth) * priv->lines + y / priv->lineheight;

	if (that >= (int)priv->visible_num) return -1;

	if (! priv->use_icon) return that;

	make_filepos(priv, that, &fx, &fy, &trect, &irect);
	if (rect_includespoint(&trect, x, y)) {
		if (is_rename) *is_rename = TRUE;
		return that;
	}

	if (rect_includespoint(&irect, x, y)) return that;

	return -1;
}

static void dir_handle_drop(Dir_private *priv, Scrollwin_drop_struct *drop)
{
	int hit,
		x = drop->virt_x,
		y = drop->virt_y;
	Dir_entry_t *dropped_on;

	hit = dir_find_index(priv, x, y, (char *)0);
	if (hit < 0) dropped_on = (Dir_entry_t *)0;
	else dropped_on = priv->visibles + hit;

	xv_set(DIRPUB(priv), DIR_RECEIVE_DROP, drop, dropped_on, NULL);
}

static void perform_frame_catch(Dir_private *priv, dir_event_ptr dev, int toggle)
{
	Rect irect, rect, frame;
	int x, y, i;
	Dir_entry_t *vis;
	int ex = dev->virt_x,
		ey = dev->virt_y;

	frame.r_left = MINi(ex, priv->vdown_x);
	frame.r_top = MINi(ey, priv->vdown_y);

	frame.r_width = ABSo(ex - priv->vdown_x);
	frame.r_height = ABSo(ey - priv->vdown_y);

	for (i = 0, vis = priv->visibles; i < priv->visible_num; i++, vis++) {
		make_filepos(priv, i, &x, &y, &rect, &irect);

		if (irect.r_width) {
			rect = rect_bounding(&rect, &irect);
		}

		if (rect_intersectsrect(&frame, &rect)) {
			if (toggle) vis->selected = ! vis->selected;
			else vis->selected = TRUE;
		}
		else {
			if (! toggle) vis->selected = FALSE;
		}
	}

	priv->num_sel = 0;
	for (i = 0, vis = priv->visibles; i < priv->visible_num; i++, vis++) {
		if (vis->selected) ++priv->num_sel;
	}

	xv_set(DIRPUB(priv), DIR_CHANGE_SELECTED, priv->num_sel, NULL);
	xv_set(DIRPUB(priv), SCROLLWIN_TRIGGER_REPAINT, NULL);
}

static int duplicate_key_is_down(Dir_private *priv, Event *ev)
{
	Xv_server srv = XV_SERVER_FROM_WINDOW(DIRPUB(priv));

	return (int)xv_get(srv, SERVER_EVENT_HAS_DUPLICATE_MODIFIERS, ev);
}

/**********************************************************************/
/****           start actions                                      ****/
/**********************************************************************/

static short beep(Dir_private *priv, dir_event_ptr dev)
{
	xv_set(DIRPUB(priv), WIN_ALARM, NULL);
	return 0;
}

static short check_feature_multiselect(Dir_private *priv, dir_event_ptr dev)
{
	if (priv->choose_one) return 0;

	return 1;
}

static int delegate_conversion(Selection_owner selown, Atom *type,
				Xv_opaque *value, unsigned long *length, int *format)
{
	typedef int (*convert_func)(Selection_owner,Atom *,Xv_opaque *,
										unsigned long *,int *);
	Drag_drop dragdrop = xv_get(selown, XV_KEY_DATA, dirpw_key);
	Xv_opaque server = XV_SERVER_FROM_WINDOW(xv_get(dragdrop, XV_OWNER));
	convert_func cf = (convert_func)xv_get(dragdrop, SEL_CONVERT_PROC);
	int retval;
	Atom requested = *type;
	static char big_filename[1000];

	SERVERTRACE((300, "delegate_conversion called for target '%s'\n",
			(char *)xv_get(server, SERVER_ATOM_NAME, *type)));

	if (*type == (Atom)xv_get(server, SERVER_ATOM, "text/plain")) {
		*type = XA_STRING;
	}
	else if (*type == (Atom)xv_get(server, SERVER_ATOM, "text/uri-list")) {
		char *fn;

		*type =  (Atom)xv_get(server, SERVER_ATOM, "FILE_NAME");

		retval = (*cf)(dragdrop, type, value, length, format);
		if (! retval) return retval;

		fn = (char *)*value;

		sprintf(big_filename, "file://%s\r\n", fn);

		*type = XA_STRING;
		*format = 8;
		*length = strlen(big_filename);
		*value = (Xv_opaque)big_filename;

		return TRUE;
	}
	/* a few requests will be rejected silently */
	else if (*type == (Atom)xv_get(server, SERVER_ATOM, "text/x-moz-url")) {
		/* mozilla expects something like UTF-16 .... */
		return FALSE;
	}
	else if (*type == (Atom)xv_get(server, SERVER_ATOM, "text/unicode")) {
		/* mozilla , funny */
		return FALSE;
	}
	else if (*type == (Atom)xv_get(server, SERVER_ATOM, "application/x-moz-file")) {
		/* mozilla , funny */
		return FALSE;
	}

	retval = (*cf)(dragdrop, type, value, length, format);

	if (! retval) {
		fprintf(stderr, "dnd.c: cannot convert '%s'\n", 
						(char *)xv_get(server, SERVER_ATOM_NAME, requested));
	}
	return retval;
}

static void try_xdnd_drop(Xv_server server, Dir_private *priv, Window pwxid)
{
	long lasttime = server_get_timestamp(server);
	Display *dpy = (Display *)xv_get(server, XV_DISPLAY);
	Window root, ch;
	int rx, ry, x, y;
	unsigned int mask;
	Window dest, frm;
	int nx, ny, sx, sy;

	XQueryPointer(dpy, pwxid, &root, &ch, &rx, &ry, &x, &y, &mask);
	/* now, at least root, sx and sy are correct */

	sx = rx;
	sy = ry;
	dest = root;
	frm = ch = root;
	while (ch && XTranslateCoordinates(dpy,dest,frm,sx,sy,&nx,&ny,&ch)) {
		int act_format = 0;
		Atom act_typeatom;
		unsigned long items, rest;
		unsigned char *bp;
		Atom wmstate = (Atom)xv_get(server, SERVER_ATOM, "WM_STATE");
		Atom XdndAware = (Atom)xv_get(server, SERVER_ATOM, "XdndAware");

		if (XGetWindowProperty(dpy, frm, wmstate,
				0L, 1000L, FALSE, wmstate, &act_typeatom,
				&act_format, &items, &rest, &bp) == Success &&
			act_format == 32)
		{
			dest = frm;
			XFree(bp);

			/* now, dest is a top level window and might have
			 * the XdndAware property
			 */
			if (XGetWindowProperty(dpy, dest, XdndAware, 0L, 1000L, FALSE,
				XA_ATOM, &act_typeatom,&act_format,&items,&rest,&bp)==Success &&
				act_format == 32)
			{
				extern int DndSendEvent(Display *dpy, XEvent *event, const char *nam);
				XClientMessageEvent cM;
				Xv_opaque xdndowner;

				XFree(bp);

				if (! dirpw_key) dirpw_key = xv_unique_key();

				xdndowner = xv_get(priv->dragdrop, XV_KEY_DATA, dirpw_key);
				if (! xdndowner) {
					xdndowner = xv_create(DIRPUB(priv), SELECTION_OWNER,
							SEL_RANK_NAME, "XdndSelection",
							SEL_CONVERT_PROC, delegate_conversion,
							XV_KEY_DATA, dirpw_key, priv->dragdrop,
							NULL);
					xv_set(priv->dragdrop,
							XV_KEY_DATA, dirpw_key, xdndowner,
							NULL);
				}
				xv_set(xdndowner, SEL_OWN, TRUE, NULL);

				cM.type = ClientMessage;
				cM.display = dpy;
				cM.format = 32;
				cM.message_type = (Atom)xv_get(server, SERVER_ATOM, "XdndEnter");
				cM.window = dest;
				cM.data.l[0] = pwxid;
				cM.data.l[1] = (3 << 24);
				cM.data.l[2] = (Atom)xv_get(server, SERVER_ATOM, "text/plain");
				cM.data.l[3] = (Atom)xv_get(server, SERVER_ATOM, "text/uri-list");
				cM.data.l[4] = (Atom)xv_get(server, SERVER_ATOM, "FILE_NAME");

				DndSendEvent(dpy, (XEvent *)&cM, "direvent-enter");

				cM.type = ClientMessage;
				cM.display = dpy;
				cM.format = 32;
				cM.message_type = (Atom)xv_get(server, SERVER_ATOM, "XdndPosition");
				cM.window = dest;
				cM.data.l[0] = pwxid;
				cM.data.l[1] = 0;
				cM.data.l[2] = ((rx << 16) | ry);
				cM.data.l[3] = lasttime;
				cM.data.l[4] = (Atom)xv_get(server, SERVER_ATOM, "XdndActionCopy");

				DndSendEvent(dpy, (XEvent *)&cM, "direvent-position");

				cM.type = ClientMessage;
				cM.display = dpy;
				cM.format = 32;
				cM.message_type = (Atom)xv_get(server, SERVER_ATOM, "XdndDrop");
				cM.window = dest;
				cM.data.l[0] = pwxid;
				cM.data.l[1] = 0;
				cM.data.l[2] = lasttime;
				cM.data.l[3] = 0;
				cM.data.l[4] = 0;

				DndSendEvent(dpy, (XEvent *)&cM, "direvent-drop");
				return;
			}
			break;
		}
		sx = nx;
		sy = ny;
		dest = frm;
		frm = ch;
	}
}

static void repaint_file(Dir_private *priv, int this)
{
	int x, y;
	Xv_opaque pw;
	Scrollpw_info vi;

	make_filepos(priv, this, &x, &y, (Rect *)0, (Rect *)0);

	OPENWIN_EACH_PW(DIRPUB(priv), pw)
		xv_get(pw, SCROLLPW_INFO, &vi);
		repaint_file_2D(priv, this, vi.dpy, vi.xid, x-vi.scr_x, y-vi.scr_y);
	OPENWIN_END_EACH
}

static short drag_and_drop(Dir_private *priv, dir_event_ptr dev)
{
	char **files;
	int allow_drag_move, do_move;
	Dir_entry_t *vis;
	int dropstat, i, cnt = 0, vcnt = priv->visible_num;
	Xv_server server;

	if (priv->inhibit & DIR_INHIBIT_DRAG) return 0;

	for (i = 0, vis = priv->visibles; i < vcnt; i++, vis++)
		if (vis->selected) ++cnt;

	if (! cnt) return 0;

	files = (char **)calloc((size_t)cnt + 1, sizeof(char *));
	if (! files) {
		perror(XV_MSG("calloc in dir_start_dragging returns NULL"));
		abort();
	}

	cnt = 0;
	for (i = 0, vis = priv->visibles; i < vcnt; i++, vis++)
		if (vis->selected) files[cnt++] = vis->path;

	if (priv->inhibit & DIR_INHIBIT_DRAG_MOVE) {
		do_move = FALSE;
		allow_drag_move = FALSE;
	}
	else if ((allow_drag_move = (int)xv_get(DIRPUB(priv), DIR_DO_DRAG_MOVE))) {
		do_move = ! duplicate_key_is_down(priv, dev->event);
	}
	else {
		do_move = FALSE;
	}

	xv_set(priv->dragdrop,
			FILEDRAG_ALLOW_MOVE, allow_drag_move,
			DND_TYPE, do_move ? DND_MOVE : DND_COPY,
			FILEDRAG_FILE_ARRAY, files,
			NULL);

	dropstat = dnd_send_drop(priv->dragdrop);
	switch (dropstat) {
		case DND_ROOT:
			xv_set(DIRPUB(priv),
					DIR_DROP_ON_ROOT, files, cnt,
					NULL);

			/* fall through */
		case XV_OK:
			/* Dropped successfully: deselect all */
			vcnt = priv->visible_num; /* might have changed */
			for (i = 0, vis = priv->visibles; i < vcnt; i++, vis++) {
				if (vis->selected) {
					vis->selected = FALSE;
					repaint_file(priv, i);
				}
			}
			xv_set(DIRPUB(priv), DIR_CHANGE_SELECTED, priv->num_sel = 0, NULL);

			break;

		case DND_ILLEGAL_TARGET:
			server = XV_SERVER_FROM_WINDOW(DIRPUB(priv));
			/* this is our hack (see srv_get.c) to find out where we are
			 * running under an official SUN libxview
			 */
			if (xv_get(server, SERVER_WM_NONE)) {
				/* not 0 meaning an official SUN libxview
				 * which is not 'XdndAware'
				 */
				try_xdnd_drop(server, priv, dev->vinfo->xid);
			}

			break;

		default:
			break;
	}

	free((char *)files);
	priv->selected_index = -1;
	return 0;
}

static short check_catch(Dir_private *priv, dir_event_ptr dev)
{
	priv->selected_index = dir_find_index(priv, dev->virt_x, dev->virt_y,
								&priv->is_rename);
	return (priv->selected_index >= 0);
}

static short handle_sel_down_object(Dir_private *priv, dir_event_ptr dev)
{
	int i;
	Dir_entry_t *vis;

	if (! priv->visibles[priv->selected_index].selected) {
		for (i = 0, vis = priv->visibles; i < priv->visible_num; i++, vis++) {
			if (vis->selected) {
				vis->selected = FALSE;
				repaint_file(priv, i);
			}
		}

		priv->visibles[priv->selected_index].selected = TRUE;
		repaint_file(priv, priv->selected_index);
		xv_set(DIRPUB(priv), DIR_CHANGE_SELECTED, priv->num_sel = 1, NULL);
	}

	return 0;
}

static short remember_down(Dir_private *priv, dir_event_ptr dev)
{
	priv->down_x = (int)event_x(dev->event);
	priv->down_y = (int)event_y(dev->event);
	priv->vdown_x = dev->virt_x;
	priv->vdown_y = dev->virt_y;
	return 0;
}

static void save_time(Dir_private *priv)
{
	Xv_opaque pub = DIRPUB(priv);
	priv->lasttime = server_get_timestamp(XV_SERVER_FROM_WINDOW(pub));
}

static void null_time(Dir_private *priv)
{
	priv->lasttime = 0L;
}

static int is_double_click(Dir_private *priv)
{
	Xv_opaque pub = DIRPUB(priv);
	time_t now = server_get_timestamp(XV_SERVER_FROM_WINDOW(pub));

	return ((now - priv->lasttime) < 100 * priv->multiclick_time);
}

static void paint_frame(Dir_private *priv, Scrollpw_info *vinfo)
{
	XDrawRectangle(vinfo->dpy, vinfo->xid, priv->frame_gc,
				MINi(priv->vdown_x, priv->vlast_x) - vinfo->scr_x,
				MINi(priv->vdown_y, priv->vlast_y) - vinfo->scr_y,
				(unsigned)ABSo(priv->vdown_x - priv->vlast_x),
				(unsigned)ABSo(priv->vdown_y - priv->vlast_y));
}

static short draw_frame(Dir_private *priv, dir_event_ptr dev)
{
	paint_frame(priv, dev->vinfo);
	return 0;
}

static short disable_auto_scroll(Dir_private *priv, dir_event_ptr dev)
{
	xv_set(DIRPUB(priv), SCROLLWIN_ENABLE_AUTO_SCROLL, FALSE, NULL);
	return 0;
}

static short start_frame(Dir_private *priv, dir_event_ptr dev)
{
	priv->vlast_x = dev->virt_x;
	priv->vlast_y = dev->virt_y;
	draw_frame(priv, dev);
	xv_set(DIRPUB(priv), SCROLLWIN_ENABLE_AUTO_SCROLL, TRUE, NULL);
	return 0;
}

static short select_frame(Dir_private *priv, dir_event_ptr dev)
{
	draw_frame(priv, dev);
	priv->vlast_x = dev->virt_x;
	priv->vlast_y = dev->virt_y;
	perform_frame_catch(priv, dev, FALSE);
	return 0;
}

static short adjust_frame(Dir_private *priv, dir_event_ptr dev)
{
	draw_frame(priv, dev);
	priv->vlast_x = dev->virt_x;
	priv->vlast_y = dev->virt_y;
	perform_frame_catch(priv, dev, TRUE);
	return 0;
}

static short update_frame(Dir_private *priv, dir_event_ptr dev)
{
	draw_frame(priv, dev);
	priv->vlast_x = dev->virt_x;
	priv->vlast_y = dev->virt_y;
	draw_frame(priv, dev);
	return 0;
}

static short toggle_entry(Dir_private *priv, dir_event_ptr dev)
{
	if (priv->visibles[priv->selected_index].selected) {
		--priv->num_sel;
	}
	else {
		++priv->num_sel;
	}

	priv->visibles[priv->selected_index].selected =
							! priv->visibles[priv->selected_index].selected;
	repaint_file(priv, priv->selected_index);
	xv_set(DIRPUB(priv), DIR_CHANGE_SELECTED, priv->num_sel, NULL);
	return 0;
}

static short deselect_all(Dir_private *priv, dir_event_ptr dev)
{
	int i;
	Dir_entry_t *vis;

	for (i = 0, vis = priv->visibles; i < priv->visible_num; i++, vis++) {
		if (vis->selected) {
			vis->selected = FALSE;
			repaint_file(priv, i);
		}
	}
	xv_set(DIRPUB(priv), DIR_CHANGE_SELECTED, priv->num_sel = 0, NULL);

	return 0;
}

static short double_select(Dir_private *priv, dir_event_ptr dev)
{
	take_file_on_select_button(priv, FALSE, FALSE);
	return 0;
}

static short double_adjust(Dir_private *priv, dir_event_ptr dev)
{
	if (!priv->multi_select_proc) return 0;

	(*(priv->multi_select_proc))(DIRPUB(priv),
									dir_get_all_selected(priv, FALSE),
									event_shift_is_down(dev->event),
									event_ctrl_is_down(dev->event));
	return 0;
}

static short check_drag_threshold(Dir_private *priv, dir_event_ptr dev)
{
	int dx, dy;

	if ((dx = priv->down_x - (int)event_x(dev->event)) < 0) dx = -dx;
	if ((dy = priv->down_y - (int)event_y(dev->event)) < 0) dy = -dy;

	if (dx > priv->drag_threshold || dy > priv->drag_threshold) return 1;

	return 0;
}

static short handle_select_up(Dir_private *priv, dir_event_ptr dev)
{
	if (event_shift_is_down(dev->event) || event_ctrl_is_down(dev->event)) {
		take_file_on_select_button(priv,
						event_shift_is_down(dev->event),
						event_ctrl_is_down(dev->event));
	}
	return 0;
}

static void scroll_into_view(Dir_private *priv, Xv_window pw, int x, int y)
{
	Dircanvas self = DIRPUB(priv);
	Scrollbar sb;
	Xv_window view = xv_get(pw, XV_OWNER);
	int viewstart, viewlen, scrypos;

	scrypos = y / priv->lineheight;

	viewstart = (int)xv_get(view, SCROLLVIEW_V_START);
	sb = xv_get(self, OPENWIN_VERTICAL_SCROLLBAR, view);
	viewlen = (int)xv_get(sb, SCROLLBAR_VIEW_LENGTH);

	if (scrypos >= viewstart && scrypos < viewstart + viewlen) return;
	if (scrypos < viewstart) {
		xv_set(view, SCROLLVIEW_V_START, scrypos - 1, NULL);
	}
	else {
		xv_set(view, SCROLLVIEW_V_START, scrypos - viewlen + 2, NULL);
	}
}

static void dir_start_text(Dir_private *priv, Xv_window pw, int that, int scroll)
{
	int x, y, px, py;
	Frame fram;
	Scrollpw_info vi;
	Rect trect, irect;

	make_filepos(priv, that, &x, &y, (Rect *)0, (Rect *)0);
	if (scroll) scroll_into_view(priv, pw, x, y);
	xv_get(pw, SCROLLPW_INFO, &vi);
	fram = xv_get(DIRPUB(priv), WIN_FRAME);
	priv->visibles[that].selected = FALSE;
	repaint_file(priv, that);
	make_filepos(priv, that, &x, &y, &trect, &irect);
	trect.r_left -= vi.scr_x;
	trect.r_top -= vi.scr_y;
	win_translate_xy(pw, fram, trect.r_left, trect.r_top, &px, &py);
	xv_set(priv->panel,
			XV_X, px,
			XV_Y, py,
			XV_WIDTH, MAXi(priv->visibles[that].extent + 22, 100),
			XV_SHOW, TRUE,
			WIN_FRONT,
			NULL);

	xv_set(priv->renametext,
			PANEL_VALUE, priv->visibles[that].name,
			PANEL_VALUE_DISPLAY_WIDTH,
						MAXi(priv->visibles[that].extent + 20, 100),
			NULL);
	xv_set(priv->panel, WIN_SET_FOCUS, NULL);
	if (priv->name_before_text_mode) xv_free(priv->name_before_text_mode);
	priv->name_before_text_mode = xv_strsave(priv->visibles[that].path);
	xv_set(priv->renametext, PANEL_TEXT_SELECT_LINE, NULL);
}

static short deselect_all_except(Dir_private *priv, dir_event_ptr dev)
{
	int i, sel_ind = priv->selected_index;
	Dir_entry_t *vis;

	for (i = 0, vis = priv->visibles; i < priv->visible_num; i++, vis++) {
		if (i != sel_ind && vis->selected) {
			vis->selected = FALSE;
			repaint_file(priv, i);
		}
	}

	priv->num_sel = (sel_ind >= 0) ? 1 : 0;
	xv_set(DIRPUB(priv), DIR_CHANGE_SELECTED, priv->num_sel, NULL);

	if (priv->inhibit & DIR_INHIBIT_RENAME) return 0;
	if (priv->auto_rename &&
		sel_ind >= 0 &&
		priv->dir_writable &&
		priv->is_rename)
	{
		/* dev->pw beruecksichtigen ? Problem bei rename in split views */
		dir_start_text(priv, dev->pw, sel_ind, FALSE);
#ifdef BEFORE_2024_04_04
		xv_set(DIRPUB(priv),
				DIR_START_TEXT, priv->visibles[sel_ind].name, FALSE,
				NULL);
#endif /* BEFORE_2024_04_04 */
	}

	return 0;
}

/**********************************************************************/
/****           end actions                                        ****/
/**********************************************************************/

static int have_matches(Dir_private *priv, Xv_window pw)
{
	const char *q;
	char buf[200];
	int have_one = FALSE, i;
	int minrow = 100000, mincol = 100000, x, y;
	Dir_entry_t *vis;

	strcpy(priv->cur_med, ".*$");

	if (priv->match_context) {
		xv_free_regexp(priv->match_context);
		priv->match_context = NULL;
	}
	if ((q = xv_compile_regexp(priv->match_ed_pattern, &priv->match_context))) {
		if (priv->tell_match_proc) {
			sprintf(buf, XV_MSG("Building '%s*': %s"),priv->match_sh_pattern,q);
			(*(priv->tell_match_proc))(DIRPUB(priv), buf, FALSE);
		}
		return TRUE;
	}

	priv->num_sel = 0;
	for (i = 0, vis = priv->visibles; i < priv->visible_num; i++, vis++) {
		if (priv->choose_one && have_one) {
			vis->selected = FALSE;
		}
		else if (xv_match_regexp(vis->name, priv->match_context, 0)) {
			vis->selected = TRUE;
			++priv->num_sel;
			have_one = TRUE;
			if (priv->line_oriented) {
				x = i % priv->cols;
				y = i / priv->cols;
			}
			else {
				x = i / priv->lines;
				y = i % priv->lines;
			}

			if (x < mincol) mincol = x;
			if (y < minrow) minrow = y;
		}
		else {
			vis->selected = FALSE;
		}
	}

	xv_set(DIRPUB(priv), DIR_CHANGE_SELECTED, priv->num_sel, NULL);

	if (have_one) {
		xv_set(xv_get(pw, XV_OWNER),
				SCROLLVIEW_V_START, minrow,
				SCROLLVIEW_H_START, mincol,
				NULL);
	}

	xv_set(DIRPUB(priv), SCROLLWIN_TRIGGER_REPAINT, NULL);

	return have_one;
}

static void dir_reset_matching(Dir_private *priv)
{
	if (!priv->match_sh_pattern) {
		priv->match_sh_pattern = xv_malloc(200L);
		priv->match_ed_pattern = xv_malloc(400L);
	}
	priv->cur_msh = priv->match_sh_pattern;
	priv->cur_med = priv->match_ed_pattern + 1;
	priv->match_ed_pattern[0] = '^';
	priv->matching = FALSE;
}

static char *dir_convert_char_sh_to_ed(char *p, int ch)
{
	switch (ch) {
		case '.': *p++ = '\\'; *p++ = '.'; break;
		case '?': *p++ = '.'; break;
		case '*': *p++ = '.'; *p++ = '*'; break;
		default: *p++ = ch; break;
	}
	return p;
}

static void handle_match(Dir_private *priv, Xv_window pw, int ch)
{
	char buf[200];

	if (! priv->match_sh_pattern) dir_reset_matching(priv);

	if (isprint(ch)) {
		priv->cur_med = dir_convert_char_sh_to_ed(priv->cur_med, ch);
		*(priv->cur_msh)++ = ch;
		*priv->cur_msh = '\0';

		if (! priv->matching && (ch == '*' || ch == '?')) {
			if (priv->tell_match_proc) {
				sprintf(buf, XV_MSG("Building '%s*'"), priv->match_sh_pattern);
				(*(priv->tell_match_proc))(DIRPUB(priv), buf, FALSE);
			}
		}
		else {
			priv->matching = TRUE;
			if (priv->tell_match_proc) {
				sprintf(buf, XV_MSG("Matching '%s*'"), priv->match_sh_pattern);
				(*(priv->tell_match_proc))(DIRPUB(priv), buf, FALSE);
			}
			if (! have_matches(priv, pw)) {
				if (priv->tell_match_proc) {
					sprintf(buf, XV_MSG("No match for '%s*'"),
										priv->match_sh_pattern);
					(*(priv->tell_match_proc))(DIRPUB(priv), buf, TRUE);
				}
				dir_reset_matching(priv);
			}
		}
	}
	else {
		dir_reset_matching(priv);
	}
}

static int handle_navigation(Dir_private *priv, Xv_window pw, Scrollpw_info *vi, Event *ev)
{
	Xv_window view, focuswin;
	int foc_x, foc_y;
	int row, col;
	int maxviewx, maxviewy, minviewx, minviewy;
	int x_scroll = -1, y_scroll = -1;

	if (! priv->full_keyboard)  return FALSE;
	if (event_is_up(ev)) return TRUE;

	focuswin = xv_get(xv_get(DIRPUB(priv), WIN_FRAME), FRAME_FOCUS_WIN);

	if (priv->line_oriented) {
		row = priv->cursor_index / priv->cols;
		col = priv->cursor_index % priv->cols;
	}
	else {
		row = priv->cursor_index % priv->lines;
		col = priv->cursor_index / priv->lines;
	}

	switch (event_action(ev)) {
		case ACTION_UP:
			if (--row < 0) row = 0;
			break;
		case ACTION_DOWN:
			if (++row >= priv->lines) row = priv->lines - 1;
			break;
		case ACTION_LEFT:
			if (--col < 0) col = 0;
			break;
		case ACTION_RIGHT:
			if (++col >= priv->cols) col = priv->cols - 1;
			break;
		case ACTION_LINE_END:
			col = priv->cols - 1;
			break;
		case ACTION_LINE_START:
			col = 0;
			break;
		case ACTION_PANE_DOWN:
			if ((row += vi->height / priv->lineheight) >= priv->lines)
				row = priv->lines - 1;
			break;
		case ACTION_PANE_UP:
			if ((row -= vi->height / priv->lineheight) < 0)
				row = 0;
			break;
		case ACTION_DATA_START: row = col = 0; break;
		case ACTION_DATA_END:
			row = priv->lines - 1;
			col = priv->cols - 1;
			break;
		default: return FALSE;
	}

	row = (row + priv->lines) % priv->lines;
	col = (col + priv->cols) % priv->cols;

	view = xv_get(pw, XV_OWNER);
	minviewx = (int)xv_get(view, SCROLLVIEW_H_START);
	maxviewx = minviewx + vi->width / priv->maxwidth;
	if (col < minviewx) {
		x_scroll = col;
	}
	else if (col > maxviewx) {
		x_scroll = col + minviewx - maxviewx;
	}

	minviewy = (int)xv_get(view, SCROLLVIEW_V_START);
	maxviewy = minviewy + vi->height / priv->lineheight;
	if (row < minviewy) {
		y_scroll = row;
	}
	else if (row >= maxviewy) {
		y_scroll = row + minviewy - maxviewy;
	}

	if (priv->line_oriented) {
		priv->cursor_index = row * priv->cols + col;
	}
	else {
		priv->cursor_index = col * priv->lines + row;
	}

	if (priv->cursor_index < 0) priv->cursor_index = 0;
	if (priv->cursor_index >= priv->visible_num)
		priv->cursor_index = priv->visible_num - 1;

	if (x_scroll < 0 && y_scroll < 0) {
		make_filepos(priv, priv->cursor_index, &foc_x, &foc_y,
											(Rect *)0, (Rect *)0);

		xv_set(focuswin,
					XV_X, foc_x - vi->scr_x - FRAME_FOCUS_RIGHT_WIDTH,
					XV_Y, foc_y - vi->scr_y,
					NULL);
	}
	else {
		if (x_scroll >= 0) xv_set(view, SCROLLVIEW_H_START, x_scroll, NULL);
		if (y_scroll >= 0) xv_set(view, SCROLLVIEW_V_START, y_scroll, NULL);
	}

	return TRUE;
}

static void dircanvas_machine(dir_events ev, dir_states *staptr,
							dir_priv_ptr priv, dir_event_ptr es)
{
	switch (*staptr) {
  		case SNULL:
			switch (ev) {
				case SELECT_DOWN:
			    	remember_down(priv, es);
					if (! check_catch(priv, es)) {
						*staptr = SEL_BACK;
						return;
					}
					handle_sel_down_object(priv, es);
					*staptr = SEL_OBJ;
					break;
				case ADJUST_DOWN:
					if (! check_feature_multiselect(priv, es)) return;
					remember_down(priv, es);
					if (! check_catch(priv, es)) {
						*staptr = ADJ_BACK;
						return;
					}
					*staptr = ADJ_OBJ;
					break;
				default: break;
			}
			break;
		case SEL_BACK:
			switch (ev) {
				case DRAG:
					if (! check_drag_threshold(priv, es)) return;
					null_time(priv);
					start_frame(priv, es);
					*staptr = FRAME_SEL;
					break;

				case SELECT_UP:
					deselect_all(priv, es);
					null_time(priv);
					*staptr = SNULL;
					break;

				case ADJUST_DOWN:
					beep(priv, es);
					null_time(priv);
					*staptr = SNULL;
					break;
				default: break;
			}
			break;
		case SEL_OBJ:
			switch (ev) {
				case DRAG:
					if (! check_drag_threshold(priv, es)) return;
					drag_and_drop(priv, es);
					null_time(priv);
					*staptr = SNULL;
					break;

				case SELECT_UP:
					if (is_double_click(priv)) {
						double_select(priv, es);
						*staptr = SNULL;
						null_time(priv);
						return;
					}
					deselect_all_except(priv, es);
					handle_select_up(priv, es);
					save_time(priv);
					*staptr = SNULL;
					break;

				case ADJUST_DOWN:
					beep(priv, es);
					null_time(priv);
					*staptr = SNULL;
					break;
				default: break;
			}
			break;
		case ADJ_BACK:
			switch (ev) {
				case SELECT_DOWN:
					beep(priv, es);
					*staptr = SNULL;
					break;

				case DRAG:
					if (! check_drag_threshold(priv, es)) return;
					null_time(priv);
					start_frame(priv, es);
					*staptr = FRAME_ADJ;
					break;

				case ADJUST_UP:
					*staptr = SNULL;
					break;

				default: break;
			}
			break;
		case ADJ_OBJ:
			switch (ev) {
				case SELECT_DOWN:
					beep(priv, es);
					*staptr = SNULL;
					null_time(priv);
					break;

				case DRAG:
					if (! check_drag_threshold(priv, es)) return;
					null_time(priv);
					start_frame(priv, es);
					*staptr = FRAME_ADJ;
					break;

				case ADJUST_UP:
					if (is_double_click(priv)) {
						double_adjust(priv, es);
						*staptr = SNULL;
						return;
					}
					toggle_entry(priv, es);
					save_time(priv);
					*staptr = SNULL;
					break;

				default: break;
			}
			break;
		case FRAME_SEL:
			switch (ev) {
				case DRAG:
					update_frame(priv, es);
					break;

				case STOP:
					draw_frame(priv, es);
					disable_auto_scroll(priv, es);
					*staptr = SNULL;
					null_time(priv);
					break;

				case SELECT_UP:
					select_frame(priv, es);
					disable_auto_scroll(priv, es);
					*staptr = SNULL;
					null_time(priv);
					break;

				case ADJUST_DOWN:
					beep(priv, es);
					draw_frame(priv, es);
					disable_auto_scroll(priv, es);
					*staptr = SNULL;
					null_time(priv);
					break;

				default: break;
			}
			break;
		case FRAME_ADJ:
			switch (ev) {
				case SELECT_DOWN:
					beep(priv, es);
					draw_frame(priv, es);
					disable_auto_scroll(priv, es);
					*staptr = SNULL;
					null_time(priv);
					break;

				case DRAG:
					update_frame(priv, es);
					break;

				case STOP:
					draw_frame(priv, es);
					disable_auto_scroll(priv, es);
					*staptr = SNULL;
					null_time(priv);
					break;

				case ADJUST_UP:
					adjust_frame(priv, es);
					disable_auto_scroll(priv, es);
					*staptr = SNULL;
					null_time(priv);
					break;

				default: break;
			}
			break;
	}
}

static void dir_show_propwin(Dir_private *priv)
{
	char **selfiles;

	if (! priv->enable_fileprops) return;

	selfiles = dir_get_all_selected(priv, TRUE);

	if (priv->props_proc) {
		if ((*(priv->props_proc))(DIRPUB(priv), selfiles)) {
			return;
		}
	}

	xv_set(DIRPUB(priv), WIN_ALARM, NULL);
}

static Dir_private *dir_get_priv_from_menu(Xv_opaque menu)
{
	return (Dir_private *)xv_get(menu, XV_KEY_DATA, dir_key);
}

static void note_show_props(Menu menu, Menu_item item)
{
	dir_show_propwin(dir_get_priv_from_menu(item));
}

static char *make_help(char *hf, char *item)
{
	char helpbuf[200];

	sprintf(helpbuf, "%s:dircanv_%s", (hf ? hf : "DirPane"), item);

	return xv_strsave(helpbuf);
}

static int dir_handle_events(Dir_private *priv, Scrollwin_event_struct *es)
{
	int i;
	Dir_entry_t *vis;

	if (priv->name_before_text_mode) {
		if (event_is_button(es->event)) {
			if (event_is_down(es->event)) return TRUE;
			cancel_text_mode(priv);
			return TRUE;
		}
		if (es->action == LOC_DRAG) return TRUE;
	}

	if (priv->full_keyboard && es->event->action == ACTION_NULL_EVENT) {
		char iso_cancel, iso_default_action, iso_select;
		iso_cancel = (char)defaults_get_integer("keyboard.cancel",
								"Keyboard.Cancel", 0x1b);   /* Escape */
		iso_default_action =(char)defaults_get_integer("keyboard.defaultAction",
								"Keyboard.DefaultAction", '\r');    /* Return */
		iso_select = (char)defaults_get_integer("keyboard.select",
								"Keyboard.Select", ' ');    /* Space */
		if (es->action == iso_cancel) {
			event_set_action(es->event, ACTION_CANCEL);
		}
		else if (es->action == iso_default_action) {
			event_set_action(es->event, ACTION_DEFAULT_ACTION);
		}
		else if (es->action == iso_select) {
			event_set_action(es->event, ACTION_SELECT);
		}
	}

	switch (es->action) {
		case ACTION_PROPS:
			if (event_is_up(es->event)) dir_show_propwin(priv);
			return TRUE;

		case ACTION_SELECT:
			dir_reset_matching(priv);
			if (! event_is_button(es->event)) {
				priv->selected_index = priv->cursor_index;
				for (i = 0, vis = priv->visibles;
					i < priv->visible_num;
					i++, vis++) {
					if (i != priv->selected_index && vis->selected) {
						vis->selected = FALSE;
						repaint_file(priv, i);
					}
				}
				priv->visibles[priv->selected_index].selected = TRUE;
				repaint_file(priv, priv->selected_index);
				xv_set(DIRPUB(priv), DIR_CHANGE_SELECTED, priv->num_sel = 1, NULL);
				return TRUE;
			}

			dircanvas_machine(event_is_down(es->event) ? SELECT_DOWN: SELECT_UP,
								&priv->state, priv, es);
			return TRUE;

		case ACTION_ADJUST:
			dir_reset_matching(priv);
			dircanvas_machine(event_is_down(es->event) ? ADJUST_DOWN: ADJUST_UP,
								&priv->state, priv, es);
			return TRUE;

		case LOC_DRAG:
			dircanvas_machine(DRAG, &priv->state, priv, es);
			return TRUE;

		case ACTION_FIND_FORWARD:
		case ACTION_FIND_BACKWARD:
			if (event_is_down(es->event) && priv->find_proc) {
				(*(priv->find_proc))(DIRPUB(priv));
			}
			return TRUE;

		case KBD_USE:
			if (priv->full_keyboard) {
				Frame fram = xv_get(DIRPUB(priv), WIN_FRAME);
				Xv_window focuswin;
				int foc_x, foc_y;

				xv_set(fram, FRAME_FOCUS_DIRECTION, FRAME_FOCUS_RIGHT, NULL);
				focuswin = xv_get(fram, FRAME_FOCUS_WIN);

				if (priv->cursor_index < 0) priv->cursor_index = 0;
				if (priv->cursor_index >= priv->visible_num)
					priv->cursor_index = priv->visible_num - 1;

				make_filepos(priv, priv->cursor_index, &foc_x, &foc_y,
												(Rect *)0, (Rect *)0);

				xv_set(focuswin,
					WIN_PARENT, es->pw,
					XV_X, foc_x - es->vinfo->scr_x - FRAME_FOCUS_RIGHT_WIDTH,
					XV_Y, foc_y - es->vinfo->scr_y,
					XV_SHOW, TRUE,
					NULL);

				if (! dirpw_key) dirpw_key = xv_unique_key();
				xv_set(es->pw, XV_KEY_DATA, dirpw_key, focuswin, NULL);
			}
			break;

		case KBD_DONE:
			if (priv->full_keyboard) {
				Frame fram = xv_get(DIRPUB(priv), WIN_FRAME);
				Xv_window focuswin;

				focuswin = xv_get(fram, FRAME_FOCUS_WIN);

				xv_set(focuswin, XV_SHOW, FALSE, NULL);
				if (! dirpw_key) dirpw_key = xv_unique_key();
				xv_set(es->pw, XV_KEY_DATA, dirpw_key, 0, NULL);
			}
			break;

		case ACTION_UP:
		case ACTION_DOWN:
		case ACTION_LEFT:
		case ACTION_RIGHT:
		case ACTION_ROW_END:
		case ACTION_ROW_START:
		case ACTION_PANE_DOWN:
		case ACTION_PANE_UP:
		case ACTION_DATA_END:
		case ACTION_DATA_START:
			if (handle_navigation(priv, es->pw, es->vinfo, es->event))
				return TRUE;
			break;

		case ACTION_HELP:
			if (event_is_down(es->event)) {
				int selind;

				selind = dir_find_index(priv, es->virt_x,
							es->virt_y, (char *)0);

				xv_set(DIRPUB(priv),
						DIR_HANDLE_HELP, es->pw, es->event,
									selind >= 0 ?
										priv->visibles + selind :
										(Dir_entry_t *)0,
						NULL);
			}
			return TRUE;

		case ACTION_MORE_HELP:
		case ACTION_TEXT_HELP:
		case ACTION_MORE_TEXT_HELP:
		case ACTION_INPUT_FOCUS_HELP:
			if (event_is_down(es->event)) {
				char *help = (char *)xv_get(DIRPUB(priv), XV_HELP_DATA);

				if (help) xv_help_show((Xv_window)es->pw, help, es->event);
				else xv_help_show((Xv_window)es->pw, "DirPane:dir_canvas", es->event);
			}
			return TRUE;

		case ACTION_DEFAULT_ACTION:
			take_file_on_select_button(priv, FALSE, FALSE);
			return TRUE;

		case ACTION_STOP:
			if (event_is_down(es->event)) {
				dir_reset_matching(priv);
				dircanvas_machine(STOP, &priv->state, priv, es);
			}
			return TRUE;

		default:
			if (priv->full_keyboard) {
				
			}
			if (event_is_ascii(es->event) && event_is_down(es->event)) {
				handle_match(priv, es->pw, (char)event_id(es->event));
			}
			break;
	}

	return FALSE;
}

#define A0 *attrs
#define A1 attrs[1]
#define A2 attrs[2]
#define A3 attrs[3]
#define A4 attrs[4]

#define ADONE ATTR_CONSUME(*attrs);break
#define CAREFUL 30

static char my_get_directory[MAXPATHLEN];

static void dir_call_syserr_proc(Dir_private *priv, ...)
{
	va_list ap;
	char *args[20];
	int n = 0;

	if (priv->syserr_proc) {
		va_start(ap, priv);
		while ((args[n++] = va_arg(ap,char*)));
		va_end(ap);
		args[n] = (char *)0;
		(*(priv->syserr_proc))(DIRPUB(priv), args);
	}
}

/************************* directory handling **************************/

static int dir_check_directory(Dir_private *priv, char *path)
{
	struct stat sb;

	if (stat(path, &sb)) {
		dir_call_syserr_proc(priv, XV_MSG("Cannot stat"), path, NULL);
		return FALSE;
	}

	if (!(sb.st_mode & S_IREAD)) {
		dir_call_syserr_proc(priv, path, XV_MSG("is not readable!"), NULL);
		return FALSE;
	}

	if (!(sb.st_mode & S_IFDIR)) {
		dir_call_syserr_proc(priv, path , XV_MSG("is not a directory!"), NULL);
		return FALSE;
	}

	priv->mtime_of_dir = sb.st_mtime;
	return TRUE;
}

static void dir_free_files(Dir_private *priv)
{
	if (priv->files) {
		unsigned int i;

		for (i = 0; i < priv->num; i++) free(priv->files[i].path);
		free((char *)priv->visibles);
		priv->visibles = (Dir_entry_t *)0;
		free((char *)priv->files);
		priv->files = (Dir_entry_t *)0;
		priv->num = 0;
		priv->visible_num = 0;
	}
}

static void make_extents(Dir_private *priv)
{
	Font_string_dims dims;
	int atboldwid, atwid;
	unsigned int i;
	Dir_entry_t *fn;
	char name[MAXPATHLEN];

	xv_get(priv->font, FONT_STRING_DIMS, "@", &dims);
	atwid = dims.width;
	xv_get(priv->boldfont, FONT_STRING_DIMS, "@", &dims);
	atboldwid = dims.width;

	for (i = 0, fn = priv->files; i < priv->num; i++, fn++) {
		strcpy(name, fn->name);

		if ((int)strlen(name) > priv->max_name_length) {
			strcpy(name + priv->max_name_length, "\253");
		}

		switch (fn->type) {
			case DIR_DIR:
				xv_get(priv->boldfont, FONT_STRING_DIMS, name, &dims);
				break;
			case DIR_DIR_LINK:
				xv_get(priv->boldfont, FONT_STRING_DIMS, name, &dims);
				dims.width += atboldwid;
				break;
			case DIR_FILE:
				xv_get(priv->font, FONT_STRING_DIMS, name, &dims);
				break;
			case DIR_FILE_LINK:
			case DIR_BROKEN_LINK:
				xv_get(priv->font, FONT_STRING_DIMS, name, &dims);
				dims.width += atwid;
				break;
		}

		fn->extent = dims.width;
	}
}

static void dir_make_new_files(Dir_private *priv, unsigned count, char **args, char *path, int scroll_to_start)
{
	struct stat sb;
	unsigned int i, j;
	Dir_entry_t *fn;
	uid_t uid;
	gid_t gid;
	char fullname[MAXPATHLEN];
	Xv_window view;
	int pathlen = (int)strlen(path) + 1;

	dir_free_files(priv);

	fn = (Dir_entry_t *)xv_alloc_n(Dir_entry_t, (size_t)count);
	priv->visibles = (Dir_entry_t *)xv_alloc_n(Dir_entry_t, (size_t)count);
	i = 0;

	uid = geteuid();
	gid = getegid();

	xv_set(DIRPUB(priv), DIR_START_SCAN, path, NULL);

	for (i = 0, j = 0; i < count; i++) {
		if (priv->progress_proc) {
			(*(priv->progress_proc))(DIRPUB(priv), (int)i, (int)count);
		}
		sprintf(fullname, "%s/%s", path, args[i]);
		if (lstat(fullname, &sb)) continue;

		fn[j].path = xv_strsave( fullname);
		fn[j].name = fn[j].path + pathlen;

		if ((sb.st_mode & S_IFMT) == S_IFLNK) {
			if (stat(fullname, &sb)) {
				fn[j].type = DIR_BROKEN_LINK;
				sb.st_mode &= (~(S_IRWXU | S_IRWXG | S_IRWXO));
			}
			else if (S_ISDIR(sb.st_mode)) fn[j].type = DIR_DIR_LINK;
			else fn[j].type = DIR_FILE_LINK;
		}
		else {
			fn[j].type = (S_ISDIR(sb.st_mode) ? DIR_DIR : DIR_FILE);
		}
		fn[j].inode = sb.st_ino;
		fn[j].dev = sb.st_dev;

		fn[j].mtime = sb.st_mtime;
		fn[j].size = sb.st_size;

		if (sb.st_uid == uid)
			fn[j].access_mode = ((sb.st_mode & S_IRWXU) >> 6);
		else if (sb.st_gid == gid)
			fn[j].access_mode = ((sb.st_mode & S_IRWXG) >> 3);
		else
			fn[j].access_mode = (sb.st_mode & S_IRWXO);

		if (priv->use_icon) {
			xv_set(DIRPUB(priv), DIR_ASSIGN_FILETYPE, &sb, fn + j, NULL);
		}

		j++;
	}

	priv->num = j;
	priv->files = fn;

	make_extents(priv);

	if (scroll_to_start) {
		OPENWIN_EACH_VIEW(DIRPUB(priv), view)
			xv_set(view,
					SCROLLVIEW_H_START, 0,
					SCROLLVIEW_V_START, 0,
					NULL);
		OPENWIN_END_EACH

		/* trigger scrollbar - update in SCROLLWIN */
		xv_set(DIRPUB(priv), SCROLLWIN_V_UNIT, priv->lineheight, NULL);
	}

	xv_set(DIRPUB(priv), DIR_END_SCAN, NULL);

	if (priv->progress_proc) {
		(*(priv->progress_proc))(DIRPUB(priv), (int)count, (int)count);
	}
}

static void dir_process_files(Dir_private *priv, char **files)
{
	unsigned cnt;

	dir_free_files(priv);
	if (!files) return;
	if (! *files) return;

	priv->use_icon = (char)xv_get(DIRPUB(priv), DIR_USE_ICONS);

	cnt = 0;
	while (files[cnt++]);

	dir_make_new_files(priv, cnt, files, ".", TRUE);
	dir_reset_matching(priv);
}

static void dir_internal_process_dir(Dir_private *priv, char *path, int scr_to_start)
{
	char fullname[MAXPATHLEN];
	unsigned dircnt, cnt;
	char **new_args;
	unsigned i;
	struct stat sb;
	DIR *dirp;
	struct dirent *dp;

	priv->use_icon = (char)xv_get(DIRPUB(priv), DIR_USE_ICONS);

	if (!path) {
		dir_free_files(priv);
		return;
	}

	if (!dir_check_directory(priv, path)) return;

	if (!(dirp = opendir(path))) {
		dir_call_syserr_proc(priv, XV_MSG("Cannot open directory"), path, NULL);
		return;
	}

	if (priv->progress_proc) {
		(*(priv->progress_proc))(DIRPUB(priv), 0, 100);
	}

	dircnt = 0;
	while ((dp = readdir(dirp))) {
		if (strcmp(dp->d_name, ".")) {
#ifdef SB_IS_UNUSED_HERE
			sprintf(fullname, "%s/%s", path, dp->d_name);
			if (stat(fullname, &sb)) continue;
#endif

			++dircnt;
		}
	}
	rewinddir(dirp);

	new_args = (char **)xv_alloc_n(char *, (size_t)(dircnt + CAREFUL));
	cnt = 0;

	while ((dp = readdir(dirp)) && (cnt < dircnt + CAREFUL - 2)) {
		if (strcmp(dp->d_name, ".")) {
			sprintf(fullname, "%s/%s", path, dp->d_name);
			if (lstat(fullname, &sb)) {
				continue;
			}

			new_args[cnt++] = xv_strsave( dp->d_name);
		}
	}
	closedir(dirp);
	new_args[cnt] = (char *)0;

	dir_make_new_files(priv, cnt, new_args, path, scr_to_start);

	for (i = 0; i < cnt; i++) free(new_args[i]);
	free((char *)new_args);
}

static void dir_process_dir(Dir_private *priv, char *path, int scr_to_start)
{
	Frame fram = xv_get(DIRPUB(priv), WIN_FRAME);
	int is_already_busy = (int)xv_get(fram, FRAME_BUSY);

	if (! is_already_busy) server_appl_busy(fram, TRUE, XV_NULL);

	dir_internal_process_dir(priv, path, scr_to_start);

	if (access(path, W_OK)) priv->dir_writable = FALSE;
	else priv->dir_writable = TRUE;

	xv_set(DIRPUB(priv), DIR_NEW_DIRECTORY, path, priv->dir_writable, NULL);

	if (! is_already_busy) server_appl_busy(fram, FALSE, XV_NULL);
}

static char *dir_convert_sh_to_ed(char *pat)
{
	char *p, *q, *new = xv_malloc(strlen(pat) * 2 + 8);

	p = pat;
	q = new;
	*q++ = '^';
	while (*p) {
		q = dir_convert_char_sh_to_ed(q, *p);
		p++;
	}

	*q++ = '$';
	*q = '\0';
	return new;
}

static int check_visibility(Dir_entry_t *e, unsigned hid)
{
	register unsigned mask;

	for (mask = 7; mask; mask >>= 1) {
		if ((hid & mask) > ((unsigned)e->access_mode &mask)) return FALSE;
		if ((hid & mask) == ((unsigned)e->access_mode&mask)) return TRUE;
	}
	return TRUE;
}

static int reverse_sorting;
static int super_sorter(const void *a, const void *b)
{
	return (reverse_sorting* (*act_sorting)((Dir_entry_t *)a,(Dir_entry_t *)b));
}


static void determine_visibility(Dir_private *priv)
{
	register int hide_two_dots = FALSE, maxwid = 0, i;
	register unsigned cnt = 0;
	register Dir_entry_t *p;

	if (priv->use_icon) {
		maxwid = DIR_ICON_WIDTH;
		hide_two_dots = TRUE;
	}

	for (i = 0, p = priv->files; i < priv->num; i++, p++) {

		if (hide_two_dots) {
			if (p->name[0] == '.' && p->name[1] == '.') continue;
		}

		if (!priv->dir_always_visible ||
			!(p->type==DIR_DIR || p->type==DIR_DIR_LINK))
		{

			if ((!priv->show_dirs) && (p->type==DIR_DIR||p->type==DIR_DIR_LINK))
				continue;

			if ((!priv->show_dots) && (p->name[0] == '.')) continue;

			if (priv->filter_ed_pattern) {
				if (! xv_match_regexp(p->name, priv->filter_context, 0))
					continue;
			}

			if (! check_visibility(p, (unsigned short)priv->hide_mode))
				continue;
		}

		priv->visibles[cnt++] = *p;
		if (maxwid < p->extent) maxwid = p->extent;
	}

	priv->visible_num = cnt;
	priv->file_wid = maxwid;
	priv->maxwidth = maxwid + priv->col_dist;

	reverse_sorting = (priv->reverse_sorting ? -1 : 1);
	act_sorting = priv->act_sorting;
	qsort(priv->visibles, (size_t)priv->visible_num, sizeof(Dir_entry_t),
				super_sorter);
}

static void make_layout(Dir_private *priv, int repaint)
{
	int wid, hig, pww, avail_lin, avail_col, lw, lh;
	Xv_opaque view, sb;

	determine_visibility(priv);

	if (!priv->visible_num) { /* empty window */
		if (repaint) {
			OPENWIN_EACH_VIEW(DIRPUB(priv), view)
				xv_set(view,
						SCROLLVIEW_V_START, 0,
						SCROLLVIEW_H_START, 0,
						NULL);
			OPENWIN_END_EACH
			xv_set(DIRPUB(priv),
						SCROLLWIN_V_OBJECT_LENGTH, priv->lines = 1,
						SCROLLWIN_H_OBJECT_LENGTH, priv->cols = 1,
						SCROLLWIN_TRIGGER_REPAINT,
						NULL);
		}
		return;
	}

	wid = (int)xv_get(DIRPUB(priv), XV_WIDTH);
	view = xv_get(DIRPUB(priv), OPENWIN_NTH_VIEW, 0);

	if ((sb = xv_get(DIRPUB(priv), OPENWIN_VERTICAL_SCROLLBAR, view))) {
		if ((wid = wid - (int)xv_get(sb, XV_WIDTH)) <= 0) wid = 1;
	}

	hig = (int)xv_get(DIRPUB(priv), XV_HEIGHT);
	if ((sb = xv_get(DIRPUB(priv), OPENWIN_HORIZONTAL_SCROLLBAR, view))) {
		if ((hig = hig - (int)xv_get(sb, XV_HEIGHT)) <= 0) hig = 1;
	}

	lw = priv->maxwidth;
	lh = priv->lineheight;

	if (!(avail_lin = hig / lh)) avail_lin = 1;
	if (lw) avail_col = wid / lw;
	else avail_col = 1;

	switch (priv->layout) {
		case DIR_LAYOUT_SINGLE_COLUMN:
			priv->cols = 1;
			break;
		case DIR_LAYOUT_TAKE_COLUMNS_FROM_WIDTH:
			priv->cols = avail_col;
			break;
		default:
			if (avail_col * avail_lin >= priv->visible_num) {
				priv->cols = avail_col;
			}
			else {
				priv->cols = priv->visible_num / avail_lin + 1;

				while (priv->cols > avail_col) {
					pww = (priv->visible_num - 1) / priv->cols + 1;
					if (pww * lh >= priv->cols * lw) break;
					priv->cols--;
				}
			}
			break;
	}

	if (!priv->cols) priv->cols = 1;
	priv->lines = (priv->visible_num - 1) / priv->cols + 1;

	xv_set(DIRPUB(priv),
				SCROLLWIN_V_OBJECT_LENGTH, priv->lines,
				SCROLLWIN_H_OBJECT_LENGTH, priv->cols,
				SCROLLWIN_V_UNIT, priv->lineheight,
				SCROLLWIN_H_UNIT, priv->maxwidth,
				NULL);

	OPENWIN_EACH_VIEW(DIRPUB(priv), view)

		if ((int)xv_get(view, SCROLLVIEW_V_START) > priv->lines)
			xv_set(view, SCROLLVIEW_V_START, 0, NULL);

		if ((int)xv_get(view, SCROLLVIEW_H_START) > priv->cols)
			xv_set(view, SCROLLVIEW_H_START, 0, NULL);

	OPENWIN_END_EACH
}

static void dir_update_directory(Dir_private *priv, int scroll_to_start)
{
	char my_dir[MAXPATHLEN];

	if (priv->dirname) {
		dir_process_dir(priv, priv->dirname, scroll_to_start);
	}
	else {
		getcwd(my_dir, sizeof(my_dir));
		dir_process_dir(priv, my_dir, scroll_to_start);
	}
	make_layout(priv, scroll_to_start);
	if (!scroll_to_start) xv_set(DIRPUB(priv), SCROLLWIN_TRIGGER_REPAINT, NULL);
}

static void dir_start_text_from_set(Dir_private *priv, char *name, int scroll)
{
	int i;

	for (i = 0; i < priv->visible_num; i++) {
		if (! strcmp(priv->visibles[i].name, name)) {
			dir_start_text(priv, xv_get(DIRPUB(priv), OPENWIN_NTH_PW, 0), i,
										scroll);
			return;
		}
	}

}

/* ARGSUSED */
static Panel_setting note_renametext(Panel_item item, Event *ev)
{
	Dir_private *priv = (Dir_private *)xv_get(item, PANEL_CLIENT_DATA);
	char newname[MAXPATHLEN], *val;

	if (priv->name_before_text_mode) {
		val = (char *)xv_get(item, PANEL_VALUE);

		if (val && *val) {
			sprintf(newname, "%s/%s", priv->dirname, val);
            if (rename(priv->name_before_text_mode, newname)) {
				char errbuffer[3000];

				sprintf(errbuffer, XV_MSG("Cannot rename\n%s\nto\n%s:\n%s"),
            						priv->name_before_text_mode, newname,
									strerror(errno));

				dir_call_syserr_proc(priv, errbuffer, NULL);

				return PANEL_NEXT;
			}

			dir_update_directory(priv, FALSE);
		}

		xv_free(priv->name_before_text_mode);
		priv->name_before_text_mode = (char *)0;
	}

	xv_set(DIRPUB(priv), WIN_SET_FOCUS, NULL);
	xv_set(priv->panel, XV_SHOW, FALSE, NULL);

	return PANEL_NEXT;
}

static void dir_make_gcs(Dir_private *priv, Scrollpw pw)
{
	unsigned long fg, bg;
	XGCValues   gcv;
	Display *dpy = (Display *)xv_get(pw, XV_DISPLAY);
	Window xid = (Window)xv_get(pw, XV_XID);
	Cms cms;
	int fore_index, back_index;

	if (priv->copygc) XFreeGC(dpy, priv->copygc);
	if (priv->frame_gc) XFreeGC(dpy, priv->frame_gc);
	if (priv->normal_gc) XFreeGC(dpy, priv->normal_gc);
	if (priv->bold_gc) XFreeGC(dpy, priv->bold_gc);
	if (priv->bold_inv_gc) XFreeGC(dpy, priv->bold_inv_gc);
	if (priv->normal_inv_gc) XFreeGC(dpy, priv->normal_inv_gc);

	xid = (Window)xv_get(pw, XV_XID);
	cms = xv_get(pw, WIN_CMS);
	fore_index = (int)xv_get(pw, WIN_FOREGROUND_COLOR);
	back_index = (int)xv_get(pw, WIN_BACKGROUND_COLOR);
	bg = (unsigned long)xv_get(cms, CMS_PIXEL, back_index);
	fg = (unsigned long)xv_get(cms, CMS_PIXEL, fore_index);

	gcv.foreground = fg;
	gcv.background = bg;
	gcv.graphics_exposures = FALSE;
	priv->copygc = XCreateGC(dpy, xid,
					GCGraphicsExposures | GCForeground | GCBackground, &gcv);

	gcv.foreground = fg ^ bg;
	gcv.line_width = 1;
	gcv.function = GXxor;
	priv->frame_gc = XCreateGC(dpy, xid,
					GCFunction | GCForeground | GCLineWidth, &gcv);

	
	gcv.foreground = fg;
	gcv.background = bg;
	gcv.function = GXcopy;
	gcv.font = xv_get(priv->font, XV_XID);
	priv->normal_gc = XCreateGC(dpy, xid,
			GCFunction | GCFont | GCForeground | GCBackground, &gcv);

	gcv.font = xv_get(priv->boldfont, XV_XID);
	priv->bold_gc = XCreateGC(dpy, xid,
			GCFunction | GCFont | GCForeground | GCBackground, &gcv);

	gcv.foreground = bg;
	gcv.background = fg;
	priv->bold_inv_gc = XCreateGC(dpy, xid,
			GCFunction | GCFont | GCForeground | GCBackground, &gcv);

	gcv.font = xv_get(priv->font, XV_XID);
	priv->normal_inv_gc = XCreateGC(dpy, xid,
			GCFunction | GCFont | GCForeground | GCBackground, &gcv);
}

static int sort_dir_first(Dir_entry_t *a, Dir_entry_t *b)
{
	int adir, bdir;

	adir = (a->type == DIR_DIR || a->type == DIR_DIR_LINK);
	bdir = (b->type == DIR_DIR || b->type == DIR_DIR_LINK);

	if (adir && ! bdir) return -1;
	if (! adir && bdir) return 1;
	return strcmp(a->name, b->name);
}

static int sort_type(Dir_entry_t *a, Dir_entry_t *b)
{
	int aord, bord;

	aord = a->dscattrs->ordering;
	bord = b->dscattrs->ordering;

	if (aord < bord) return -1;
	if (aord > bord) return 1;
	return strcmp(a->name, b->name);
}

static int sort_alpha(Dir_entry_t *a, Dir_entry_t *b)
{
	return strcmp(a->name, b->name);
}

static int sort_extension(Dir_entry_t *a, Dir_entry_t *b)
{
	char *pa, *pb;
	int comp_ext;

	pa = strrchr(a->name, '.');
	pb = strrchr(b->name, '.');

	if (pa && pb) {
		if ((comp_ext = strcmp(pa, pb))) return comp_ext;
	}
	else if (pa && ! pb) return 1;
	else if (! pa && pb) return -1;

	return strcmp(a->name, b->name);
}

static int sort_mtime(Dir_entry_t *a, Dir_entry_t *b)
{
	if (a->mtime < b->mtime) return 1;
	else return -1;
}

static int sort_size(Dir_entry_t *a, Dir_entry_t *b)
{
	if (a->size > b->size) return -1;
	else if (a->size < b->size) return 1;
	else return 0;
}

static void free_help_data(Xv_opaque obj, int key, char *data)
{
	if (key == XV_HELP) xv_free(data);
}

static void add_accel(Dircanvas self, char *key, char *desc,dir_find_proc_t func)
{
	Accelerator accel = xv_accel_get_accelerator(self);

	if (! accel) return;
	if (xv_get(self, WIN_FRAME) != xv_get(accel, ACCEL_FRAME)) return;

	xv_set(accel,
			ACCEL_REGISTER,
				ACCEL_DESCRIPTION, desc,
				ACCEL_PROC, func, self,
				ACCEL_KEY, key,
				NULL,
			NULL);
}

/********************* menu notify procedures **********************/

static void note_force_dirs_visible(Menu menu, Menu_item item)
{
	Dir_private *priv = dir_get_priv_from_menu(menu);

	priv->dir_always_visible = (char)xv_get(item, MENU_SELECTED);

	if (priv->initialized) {
		make_layout(priv, FALSE);
		xv_set(DIRPUB(priv), SCROLLWIN_TRIGGER_REPAINT, NULL);
	}
}

static void note_reverse_sorting(Menu menu, Menu_item item)
{
	Dir_private *priv = dir_get_priv_from_menu(menu);

	priv->reverse_sorting = (int)xv_get(item, MENU_SELECTED);

	if (priv->initialized) {
		make_layout(priv, FALSE);
		xv_set(DIRPUB(priv), SCROLLWIN_TRIGGER_REPAINT, NULL);
	}
}

static void note_hiding(Menu menu, Menu_item item)
{
	unsigned short cldt;
	Dir_private *priv = dir_get_priv_from_menu(menu);

	cldt = (unsigned short)xv_get(item, XV_KEY_DATA, dir_key);
	if (xv_get(item, MENU_SELECTED)) priv->hide_mode |= cldt;
	else priv->hide_mode &= ~cldt;

	if (priv->initialized) {
		make_layout(priv, FALSE);
		xv_set(DIRPUB(priv), SCROLLWIN_TRIGGER_REPAINT, NULL);
	}
}

static void action_deselect(Dircanvas self)
{
	Dir_private *priv = DIRPRIV(self);
	int i;

	for (i = 0; i < priv->visible_num; i++) priv->visibles[i].selected = FALSE;
	xv_set(self, DIR_CHANGE_SELECTED, priv->num_sel = 0, NULL);
	xv_set(self, SCROLLWIN_TRIGGER_REPAINT, NULL);
}

static void note_deselect(Menu menu, Menu_item item)
{
	Dir_private *priv = dir_get_priv_from_menu(item);

	action_deselect(DIRPUB(priv));
}

static void action_select(Dircanvas self)
{
	Dir_private *priv = DIRPRIV(self);
	int i;

	for (i = 0; i < priv->visible_num; i++) {
		priv->visibles[i].selected = TRUE;
	}
	xv_set(self, DIR_CHANGE_SELECTED, priv->num_sel = priv->visible_num, NULL);
	xv_set(self, SCROLLWIN_TRIGGER_REPAINT, NULL);
}

static void note_select(Menu menu, Menu_item item)
{
	Dir_private *priv = dir_get_priv_from_menu(item);

	action_select(DIRPUB(priv));
}

static void note_sorting(Menu menu, Menu_item item)
{
	Dir_private *priv = dir_get_priv_from_menu(menu);

	act_sorting = priv->act_sorting = (dirsortfunc)xv_get(item, XV_KEY_DATA, dir_key);
	if (priv->initialized) {
		reverse_sorting = (priv->reverse_sorting ? -1 : 1);
		qsort(priv->visibles, (size_t)priv->visible_num, sizeof(Dir_entry_t),
				super_sorter);
		xv_set(DIRPUB(priv), SCROLLWIN_TRIGGER_REPAINT, NULL);
	}
}

static void note_type_sorting(Menu menu, Menu_item item)
{
	Dir_private *priv = dir_get_priv_from_menu(menu);

	act_sorting = priv->act_sorting = priv->use_icon ? sort_type:sort_dir_first;
	if (priv->initialized) {
		reverse_sorting = (priv->reverse_sorting ? -1 : 1);
		qsort(priv->visibles, (size_t)priv->visible_num, sizeof(Dir_entry_t),
				super_sorter);
		xv_set(DIRPUB(priv), SCROLLWIN_TRIGGER_REPAINT, NULL);
	}
}

static void note_toggle_sorting(Menu m)
{
	int current_selected = (int)xv_get(m, MENU_SELECTED);
	Menu_item item;
	void (*proc)(Menu, Menu_item);

	if (current_selected == 0) current_selected = 2; /* 2 because of pin */

	if (current_selected <= 2) {
		xv_set(m, MENU_SELECTED, 3, NULL);
		item = xv_get(m, MENU_NTH_ITEM, 3);
	}
	else {
		xv_set(m, MENU_SELECTED, 2, NULL);
		item = xv_get(m, MENU_NTH_ITEM, 2);
	}
	proc = (void (*)(Menu, Menu_item))xv_get(item, MENU_NOTIFY_PROC);
	(*proc)(m, item);
}

static void action_update(Dircanvas self)
{
	Dir_private *priv = DIRPRIV(self);

	dir_update_directory(priv, FALSE);
}

static void note_update(Menu menu, Menu_item item)
{
	Dir_private *priv = dir_get_priv_from_menu(item);

	dir_update_directory(priv, FALSE);
}

static void dir_initialize_sorting(Dir_private *priv)
{
	Dircanvas self = DIRPUB(priv);
	char *hlp;
	char h_rename[100];

	hlp = (char *)xv_get(self, XV_HELP_DATA);

	if (hlp) {
		char *save_hlp;

		save_hlp = xv_strsave(hlp);
		hlp = strtok(save_hlp, ":");
		hlp = xv_strsave(hlp);
		xv_free(save_hlp);
	}

	act_sorting = priv->act_sorting = priv->use_icon ? sort_type:sort_dir_first;

	sprintf(h_rename, "%s:rename_text", hlp?hlp:"DirPane");
	xv_set(priv->panel,
				XV_HELP_DATA, xv_strsave(h_rename),
				XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
				NULL);

	if (hlp) xv_free(hlp);

	add_accel(self, "dir_update", XV_MSG("Update Directory"), action_update);
	add_accel(self, "select_all", XV_MSG("Select All"), action_select);
	add_accel(self, "note_deselect", XV_MSG("Deselect All"), action_deselect);
}

static void dir_end_of_creation(Dir_private *priv)
{
	XFontStruct *finfo;
	Frame fram;
	Xv_window cmswin;
	char *inst, instbuf[100];

	priv->font = xv_get(DIRPUB(priv), XV_FONT);
	finfo = (XFontStruct *)xv_get(priv->font, FONT_INFO);
	priv->file_hig = finfo->descent + (priv->ascent = finfo->ascent);

	priv->use_icon = (char)xv_get(DIRPUB(priv), DIR_USE_ICONS);

	priv->lineheight = priv->file_hig + priv->row_dist +
					(priv->use_icon ? (DIR_ICON_HEIGHT+priv->icon_dist) : 0);

	if (! priv->boldfont) {
		Xv_font bold;

		bold = xv_find(DIRPUB(priv), FONT,
					FONT_FAMILY, (char *)xv_get(priv->font, FONT_FAMILY),
					FONT_SIZE, (int)xv_get(priv->font, FONT_SIZE),
					FONT_STYLE, FONT_STYLE_BOLD,
					NULL);

		if (bold) {
			priv->boldfont = bold;
		}
		else {
			priv->boldfont = priv->font;
		}
	}

	if (!(cmswin = xv_get(DIRPUB(priv), OPENWIN_NTH_PW, 0))) 
		cmswin = DIRPUB(priv);

	dir_make_gcs(priv, cmswin);

	xv_create(DIRPUB(priv), SCROLLBAR,
				XV_INSTANCE_NAME, "dirVSB",
				SCROLLBAR_DIRECTION, SCROLLBAR_VERTICAL,
				SCROLLBAR_SPLITTABLE, TRUE,
				NULL);

	if (priv->layout == DIR_LAYOUT_TRY_SQUARE) {
		xv_create(DIRPUB(priv), SCROLLBAR,
					XV_INSTANCE_NAME, "dirHSB",
					SCROLLBAR_DIRECTION, SCROLLBAR_HORIZONTAL,
					SCROLLBAR_SPLITTABLE, TRUE,
					NULL);
	}

	fram = xv_get(DIRPUB(priv), WIN_FRAME);
	priv->panel = xv_create(fram, PANEL,
					XV_INSTANCE_NAME, "dirrenamepanel",
					XV_X, 0,
					XV_Y, 0,
					XV_SHOW, FALSE,
					PANEL_CLIENT_DATA, priv,
					NULL);

	inst = (char *)xv_get(DIRPUB(priv), XV_INSTANCE_NAME);
	if (inst) sprintf(instbuf, "%s_dirrename", inst);
	else sprintf(instbuf, "dirrename");

	priv->renametext = xv_create(priv->panel, PANEL_TEXT,
					XV_INSTANCE_NAME, instbuf,
					XV_X, 1,
					XV_Y, 1,
					PANEL_NOTIFY_PROC, note_renametext,
					PANEL_VALUE_DISPLAY_WIDTH, 30,
					PANEL_VALUE_STORED_LENGTH, 300,
					PANEL_CLIENT_DATA, priv,
					NULL);

	xv_set(priv->panel, WIN_FIT_WIDTH, 1, WIN_FIT_HEIGHT, 1, NULL);

	priv->auto_rename = (char)xv_get(DIRPUB(priv), DIR_AUTO_RENAME);

	dir_initialize_sorting(priv);

	xv_set(DIRPUB(priv), DIR_CONFIG_DRAGDROP, priv->dragdrop, NULL);

	priv->initialized = TRUE;
}

static Notify_value update_timer_event(Xv_opaque self, int unused)
{
	Dir_private *priv = DIRPRIV(self);

	if (priv->update_interval <= 0) {
		priv->update_running = FALSE;
		notify_set_itimer_func(self, NOTIFY_TIMER_FUNC_NULL, ITIMER_REAL,
							(struct itimerval *)0, (struct itimerval *)0);
	}
	else {
		struct stat sb;
		char *path;

		if (priv->dirname) path = priv->dirname;
		else {
			return NOTIFY_DONE;
		}

		if (stat(path, &sb)) {
			return NOTIFY_DONE;
		}

		if (sb.st_mtime != priv->mtime_of_dir) {
			Dir_entry_t *savefiles;
			unsigned savenum;
			unsigned i, j;

			/* save visible files */
			savenum = priv->visible_num;
			savefiles = (Dir_entry_t *)xv_alloc_n(Dir_entry_t, (size_t)savenum);
			memcpy((char *)savefiles,
					(char *)priv->visibles,
					savenum * sizeof(Dir_entry_t));

			dir_process_dir(priv, path, FALSE);
			make_layout(priv, FALSE);

			/* now we try to find those new files
			 * that have been selected before
			 */
			priv->num_sel = 0;

			for (i = 0; i < savenum; i++) {
				if (savefiles[i].selected) {
					for (j = 0; j < priv->visible_num; j++) {
						if (priv->visibles[j].inode == savefiles[i].inode) {
							priv->visibles[j].selected = TRUE;
							++priv->num_sel;
							break;
						}
					}
				}
			}
			free((char *)savefiles);
			xv_set(DIRPUB(priv), SCROLLWIN_TRIGGER_REPAINT, NULL);
			xv_set(DIRPUB(priv), DIR_CHANGE_SELECTED, priv->num_sel, NULL);
		}
	}

	return NOTIFY_DONE;
}

static void start_update_timer(Dir_private *priv)
{
	struct itimerval timer;

	timer.it_value.tv_sec = timer.it_interval.tv_sec = priv->update_interval;
	timer.it_value.tv_usec = timer.it_interval.tv_usec = 0;
	notify_set_itimer_func(DIRPUB(priv), update_timer_event,ITIMER_REAL,
							&timer, (struct itimerval *)0);

	priv->update_running = TRUE;
}

static Xv_opaque get_entry(Dir_private *priv, char *path)
{
	unsigned i;

	if (!path) return XV_NULL;

	for (i = 0; i < priv->visible_num; i++) {
		if (! strcmp(priv->visibles[i].path, path)) 
			return (Xv_opaque)(priv->visibles + i);
	}

	return XV_NULL;
}

static int convert_selection(FileDrag dnd, Atom *type, Xv_opaque *value, unsigned long *length, int *format)
{
	Dir_private *priv = (Dir_private *)xv_get(dnd,XV_KEY_DATA,DIR_CONVERT_SEL);
	int retval;
	Dir_sel_req_t sreq;

	retval = xv_iccc_convert(dnd, type, value, length, format);
	if (retval) return retval;

	sreq.type = type;
	sreq.value = value;
	sreq.length = length;
	sreq.format = format;
	sreq.dnd = dnd;
	sreq.cur_file = (char *)xv_get(dnd, FILEDRAG_CURRENT_FILE);
	sreq.entry = (Dir_entry_t *)get_entry(priv, sreq.cur_file);
	return (int)xv_get(DIRPUB(priv), DIR_CONVERT_SEL, &sreq);
}

static void dir_funckey_register(Dir_private *priv, Function_keys fk)
{
	Menu m = priv->sort_menu;

	if (! m) {
		return;
	}

	xv_set(fk,
			FUNCKEY_MENU, m,
			FUNCKEY_REGISTER,
				FUNCKEY_DESCRIPTION, XV_MSG("Toggle Type/Mtime"),
				FUNCKEY_CODE, "tog_typ_mtim",
				FUNCKEY_PROC, note_toggle_sorting, m,
				NULL,
			NULL);
}

static void dir_do_auto_scroll(Dir_private *priv, Scrollwin_auto_scroll_struct *ass)
{
	if (ass->is_start) {
		paint_frame(priv, ass->vinfo);
	}
	else {
		priv->vlast_x = ass->virt_mouse_x;
		priv->vlast_y = ass->virt_mouse_y;
		paint_frame(priv, ass->vinfo);
	}
}

static void dir_fill_sort_menu(Dir_private *priv, Menu menu)
{
	char *hlp = (char *)xv_get(DIRPUB(priv), XV_HELP_DATA);
	char *hlp_to_be_freed = NULL;

	if (hlp) {
		char *save_hlp;

		save_hlp = xv_strsave(hlp);
		hlp = strtok(save_hlp, ":");
		hlp_to_be_freed = hlp = xv_strsave(hlp);
		xv_free(save_hlp);
	}

	xv_set(menu,
			XV_KEY_DATA, dir_key, priv,
			MENU_NOTIFY_PROC, note_sorting,
			MENU_ITEM,
				MENU_CLASS, MENU_CHOICE,
				MENU_STRING, XV_MSG("Type"),
				MENU_NOTIFY_PROC, note_type_sorting,
				XV_KEY_DATA, FUNCKEY_CODE, "sort_type",
				NULL,
			MENU_ITEM,
				MENU_CLASS, MENU_CHOICE,
				MENU_STRING, XV_MSG("Modification Time"),
				MENU_NOTIFY_PROC, note_sorting,
				XV_KEY_DATA, dir_key, sort_mtime,
				XV_KEY_DATA, FUNCKEY_CODE, "sort_mtime",
				NULL,
			MENU_ITEM,
				MENU_CLASS, MENU_CHOICE,
				MENU_STRING, XV_MSG("Name"),
				MENU_NOTIFY_PROC, note_sorting,
				XV_KEY_DATA, dir_key, sort_alpha,
				NULL,
			MENU_ITEM,
				MENU_CLASS, MENU_CHOICE,
				MENU_STRING, XV_MSG("Extension"),
				MENU_NOTIFY_PROC, note_sorting,
				XV_KEY_DATA, dir_key, sort_extension,
				NULL,
			MENU_ITEM,
				MENU_CLASS, MENU_CHOICE,
				MENU_STRING, XV_MSG("Size"),
				MENU_NOTIFY_PROC, note_sorting,
				XV_KEY_DATA, dir_key, sort_size,
				NULL,
			XV_HELP_DATA, make_help(hlp, "sort_submenu"),
			XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
			NULL);
	if (hlp_to_be_freed) xv_free(hlp_to_be_freed);
}
static Xv_opaque dir_set(Xv_opaque self, Attr_avlist avlist)
{
	register Attr_attribute *attrs;
	register Dir_private *priv = DIRPRIV(self);
	int need_layout = FALSE;
	int need_dir_processing = FALSE;
	int consumed;
	Scrollwin_event_struct *es;

	for (attrs=avlist; *attrs; attrs=attr_next(attrs)) switch ((int)*attrs) {
		case DIR_LAYOUT:
			xv_error(self,
					ERROR_PKG, DIRCANVAS,
					ERROR_SEVERITY, ERROR_RECOVERABLE,
					ERROR_LAYER, ERROR_PROGRAM,
					ERROR_CREATE_ONLY, A0,
					NULL);
			ADONE;
		case DIR_MAX_NAME_LENGTH:
			consumed = (int)A1;
			if (consumed <= 0) consumed = MAXPATHLEN - 3;

			if (priv->max_name_length != consumed) {
				priv->max_name_length = consumed;
				if (priv->initialized) make_extents(priv);
				need_layout = TRUE;
			}
			ADONE;
		case DIR_START_TEXT:
			dir_start_text_from_set(priv, (char *)A1, (int)A2);
			ADONE;
		case DIR_ICON_DIST:
			priv->icon_dist = (int)A1;
			need_layout = TRUE,
			ADONE;
		case DIR_COL_DISTANCE:
			if ((int)A1 < 0) {
				xv_error(self,
					ERROR_SEVERITY, ERROR_RECOVERABLE,
					ERROR_PKG, DIRCANVAS,
					ERROR_LAYER, ERROR_PROGRAM,
					ERROR_BAD_VALUE, A0, A1,
					NULL);
			}
			else {
				need_layout = TRUE,
				priv->col_dist = (int)A1;
				if (priv->full_keyboard) {
					if (priv->col_dist < FRAME_FOCUS_RIGHT_WIDTH)
						priv->col_dist = FRAME_FOCUS_RIGHT_WIDTH;
				}
				priv->maxwidth = priv->file_wid + priv->col_dist;
			}
			ADONE;
		case DIR_DIRECTORY_FONT:
			if (! priv->initialized) priv->boldfont = (Xv_font)A1;
			ADONE;
		case DIR_UPDATE_INTERVAL:
			priv->update_interval = (int)A1;
			if (priv->update_interval > 0) {
				priv->update_interval = (int)A1;
				start_update_timer(priv);
			}
			ADONE;
		case DIR_ROW_DISTANCE:
			if ((int)A1 < 0) {
				xv_error(self,
					ERROR_SEVERITY, ERROR_RECOVERABLE,
					ERROR_PKG, DIRCANVAS,
					ERROR_LAYER, ERROR_PROGRAM,
					ERROR_BAD_VALUE, A0, A1,
					NULL);
			}
			else {
				need_layout = (need_layout || (priv->row_dist != (int)A1));
				priv->row_dist = (int)A1;
				priv->lineheight = priv->file_hig + priv->row_dist +
					(priv->use_icon ? (DIR_ICON_HEIGHT+priv->icon_dist) : 0);
			}
			ADONE;
		case DIR_LINE_ORIENTED:
			need_layout = (need_layout ||
							((priv->line_oriented == 0) != ((short)A1 == 0)));
			priv->line_oriented = (short)A1;
			ADONE;
		case DIR_CHOOSE_ONE:
			priv->choose_one = (short)A1;
			ADONE;
		case DIR_INHIBIT:
			priv->inhibit = (long)A1;
			ADONE;
		case DIR_USE_ED_FILTERING:
			priv->use_ed_filtering = (char)A1;
			ADONE;
		case DIR_FILTER_PATTERN:
			{
				const char *q;
				char *new, *pat = (char *)A1;
				if (priv->filter_sh_pattern) xv_free(priv->filter_sh_pattern);
				priv->filter_sh_pattern = (char *)0;
				if (priv->filter_ed_pattern) xv_free(priv->filter_ed_pattern);
				priv->filter_ed_pattern = (char *)0;
				if (pat && *pat) {
					if (priv->use_ed_filtering) {
						new = xv_strsave(pat);
					}
					else {
						new = dir_convert_sh_to_ed(pat);
					}
					if (priv->filter_context) {
						xv_free_regexp(priv->filter_context);
						priv->filter_context = NULL;
					}
					if ((q = xv_compile_regexp(new, &priv->filter_context))) {
						xv_error(self,
							ERROR_PKG, DIRCANVAS,
							ERROR_STRING, q,
							ERROR_SEVERITY, ERROR_RECOVERABLE,
							NULL);
						xv_free(new);
					}
					else {
						priv->filter_sh_pattern = xv_strsave(pat);
						priv->filter_ed_pattern = new;
					}
				}
			}
			need_layout = TRUE;
			ADONE;
		case DIR_DIRECTORY:
			if (priv->dirname) {
				free(priv->dirname);
				priv->dirname = (char *)0;
			}

			if (A1) {
				char *par = (char *)A1;

				if (!strcmp(par, ".")) {
					getcwd(my_get_directory, sizeof(my_get_directory));
					par = my_get_directory;
				}

				priv->dirname = xv_strsave(par);
				need_dir_processing = TRUE;
			}
			else {
				dir_free_files(priv);
			}
			dir_reset_matching(priv);
			need_layout = TRUE;
			ADONE;
		case DIR_CLIENT_DATA:
			priv->client_data = (Xv_opaque)A1;
			ADONE;
		case DIR_PROGRESS_PROC:
			priv->progress_proc = (dir_progress_t)A1;
			ADONE;
		case DIR_MULTI_SELECT_PROC:
			priv->multi_select_proc = (dir_multi_proc_t)A1;
			ADONE;
		case DIR_PROPS_PROC:
			priv->props_proc = (dir_props_proc_t)A1;
			ADONE;
		case DIR_FIND_PROC:
			priv->find_proc = (dir_find_proc_t)A1;
			ADONE;
		case DIR_TELL_MATCH_PROC:
			priv->tell_match_proc = (tell_match_proc_t)A1;
			ADONE;
		case DIR_SELECT_FULL_PATH:
			priv->select_full_path = (char)A1;
			ADONE;
		case DIR_SELECT_PROC:
			priv->select_proc = (dir_select_proc_t)A1;
			ADONE;
		case DIR_DROP_PROC:
			priv->drop_proc = (dir_drop_proc_t)A1;
			ADONE;
		case DIR_ROOTDROP_PROC:
			priv->root_action = (dir_rootact_t)A1;
			ADONE;
		case DIR_SYSERR_PROC:
			priv->syserr_proc = (dir_syserr_proc_t)A1;
			ADONE;
		case DIR_SHOW_DIRS:
			need_layout = (need_layout ||
							((priv->show_dirs == 0) != ((int)A1 == 0)));
			priv->show_dirs = (int)A1;
			ADONE;
		case DIR_SHOW_DOT_FILES:
			need_layout = (need_layout ||
							((priv->show_dots == 0) != ((int)A1 == 0)));
			priv->show_dots = (int)A1;
			ADONE;
		case DIR_FILE_VECTOR:
			dir_process_files(priv, (char **)A1);
			need_layout = TRUE;
			ADONE;
		case DIR_FILES:
			dir_process_files(priv, (char **)&A1);
			need_layout = TRUE;
			ADONE;
		case DIR_ARGS_OFFSET:
			if ((int)A1 >= 0) {
				priv->args_offset = (int)A1;
			}
			else {
				xv_error(self,
					ERROR_SEVERITY, ERROR_RECOVERABLE,
					ERROR_PKG, DIRCANVAS,
					ERROR_LAYER, ERROR_PROGRAM,
					ERROR_BAD_VALUE, A0, A1,
					NULL);
			}
			ADONE;

		case DIR_FUNCTION_KEYS_REG:
			dir_funckey_register(priv, (Xv_opaque)A1);
			ADONE;

		case DIR_CONFIG_DRAGDROP:
		case DIR_SET_FILEPROPS:
		case DIR_CHANGE_SELECTED:
		case DIR_NEW_DIRECTORY:
		case DIR_ASSIGN_FILETYPE:
			ADONE;

		case DIR_HANDLE_HELP:
			{
				char *help = (char *)xv_get(DIRPUB(priv), XV_HELP_DATA);

				if (help) xv_help_show((Xv_window)A1, help, (Event *)A2);
				else xv_help_show((Xv_window)A1, "DirPane:dir_canvas",
												(Event *)A2);
			}
			ADONE;

		case SCROLLWIN_DROP_EVENT:
				{
					Atom dndsel;
					Scrollwin_drop_struct *drop = (Scrollwin_drop_struct *)A1;

					dndsel = xv_get(drop->sel_req, SEL_RANK);
					xv_set(drop->sel_req,
								FILE_REQ_ALLOCATE, FALSE,
								FILE_REQ_ALREADY_DECODED, TRUE,
								FILE_REQ_FETCH, drop->event,
								NULL);
					drop->files = (char **)xv_get(drop->sel_req, FILE_REQ_FILES,
												&drop->cnt);

					xv_set(self, SCROLLWIN_HANDLE_DROP, drop, NULL);
					xv_set(drop->sel_req, SEL_RANK, dndsel, NULL);

					dnd_done(drop->sel_req);
				}
				ADONE;

		case DIR_RECEIVE_DROP:
			if (! (priv->inhibit & DIR_INHIBIT_DROP) && priv->drop_proc) {
				Dir_entry_t *dropped_on;
				Scrollwin_drop_struct *ds;

				ds = (Scrollwin_drop_struct *)A1;
				dropped_on = (Dir_entry_t *)A2;
				(*(priv->drop_proc))(self, ds->files, ds->cnt,
						dropped_on ? dropped_on->path : (char *)0);
			}
			ADONE;

		case DIR_DROP_ON_ROOT:
			if (! (priv->inhibit & DIR_INHIBIT_DROP) && priv->root_action) {
				(*(priv->root_action))(self, (char **)A1, (int)A2);
			}
			ADONE;

		case DIR_RESCAN:
			dir_update_directory(priv, (int)A1);
			need_dir_processing = FALSE;
			need_layout = FALSE;
			ADONE;

		case DIR_START_SCAN:
		case DIR_END_SCAN:
			ADONE;

		case SCROLLWIN_AUTO_SCROLL:
			dir_do_auto_scroll(priv, (Scrollwin_auto_scroll_struct *)A1);
			ADONE;

		case SCROLLWIN_HANDLE_EVENT:
			es = (Scrollwin_event_struct *)A1;
			consumed = dir_handle_events(priv, es);
			if (consumed) {
				es->consumed = TRUE;
				ADONE;
			}
			break;

		case SCROLLWIN_SCALE_PERCENT:
			ADONE; /* no ! */

		case SCROLLWIN_REPAINT:
			dir_repaint_from_expose(priv, (Scrollwin_repaint_struct *)A1);
			ADONE;

		case SCROLLWIN_HANDLE_DROP:
			if (! (priv->inhibit & DIR_INHIBIT_DROP))
				dir_handle_drop(priv, (Scrollwin_drop_struct *)A1);
			ADONE;

		case SCROLLWIN_PW_CMS_CHANGED:
			dir_make_gcs(priv, (Scrollpw)A1);
			break;

		case DIR_FILL_SORT_MENU:
			dir_fill_sort_menu(priv, (Menu)A1);
			ADONE;

		case XV_END_CREATE:
			dir_end_of_creation(priv);
			need_dir_processing = TRUE;
			break;

		default: xv_check_bad_attr(DIRCANVAS, A0);
	}

	if (need_dir_processing && priv->initialized) {
		if (priv->dirname) {
			dir_process_dir(priv, priv->dirname, TRUE);
		}
		need_layout = TRUE;
	}

	if (need_layout && priv->initialized) {
		make_layout(priv, TRUE);
	}

	return XV_OK;
}

static Menu_item create_item(Dir_private *priv, char *lab, char *help, menu_cb_t func, char *hlpfile)
{
	return xv_create(XV_SERVER_FROM_WINDOW(DIRPUB(priv)), MENUITEM,
							MENU_STRING, lab,
							MENU_NOTIFY_PROC, func,
							XV_HELP_DATA, make_help(hlpfile, help),
							XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
							XV_KEY_DATA, dir_key, priv,
							MENU_RELEASE,
							NULL);
}

static Menu_item create_hide(Dir_private *priv, char *lab, char *help, menu_cb_t func, char *hlpfile)
{
	Menu hide_menu;
	Frame fram = xv_get(DIRPUB(priv), WIN_FRAME);

	if (priv->hide_menu) hide_menu = priv->hide_menu;
	else {
		hide_menu = xv_create(XV_SERVER_FROM_WINDOW(DIRPUB(priv)),
				MENU_TOGGLE_MENU,
				XV_INSTANCE_NAME, "hide_menu",
				MENU_GEN_PIN_WINDOW, fram, lab,
				XV_KEY_DATA, dir_key, priv,
				MENU_ITEM,
					MENU_STRING, XV_MSG("Force Dirs Visible"),
					MENU_NOTIFY_PROC, note_force_dirs_visible,
					XV_HELP_DATA, make_help(hlpfile, "force_dirs_visible"),
					XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
					NULL,
				MENU_ITEM,
					MENU_STRING, XV_MSG("Hide Unwritable"),
					XV_KEY_DATA, dir_key, S_IWOTH,
					MENU_NOTIFY_PROC, note_hiding,
					XV_HELP_DATA, make_help(hlpfile, "hide_unwritable"),
					XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
					NULL,
				MENU_ITEM,
					MENU_STRING, XV_MSG("Hide Unreadable"),
					XV_KEY_DATA, dir_key, S_IROTH,
					MENU_NOTIFY_PROC, note_hiding,
					XV_HELP_DATA, make_help(hlpfile, "hide_unreadable"),
					XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
					NULL,
				MENU_ITEM,
					MENU_STRING, XV_MSG("Hide Unexecutable"),
					XV_KEY_DATA, dir_key, S_IXOTH,
					MENU_NOTIFY_PROC, note_hiding,
					XV_HELP_DATA, make_help(hlpfile, "hide_unexecutable"),
					XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
					NULL,
				MENU_ITEM,
					MENU_STRING, XV_MSG("Reverse Sorting"),
					MENU_NOTIFY_PROC, note_reverse_sorting,
					XV_HELP_DATA, make_help(hlpfile, "reverse_sort"),
					XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
					NULL,
				XV_HELP_DATA, make_help(hlpfile, "hide_sub_menu"),
				XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
				NULL);

		priv->hide_menu = hide_menu;
		/* too early during xv_create ! */
		xv_set(hide_menu, XV_SET_MENU, fram, NULL);
	}

	return xv_create(XV_SERVER_FROM_WINDOW(DIRPUB(priv)), MENUITEM,
				MENU_STRING, lab,
				MENU_PULLRIGHT, hide_menu,
				XV_KEY_DATA, dir_key, priv,
				XV_HELP_DATA, make_help(hlpfile, help),
				XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
				MENU_RELEASE,
				NULL);
}

static Menu_item create_sort(Dir_private *priv, char *lab, char *help,
								menu_cb_t func, char *hlpfile)
{
	Menu sort_menu;
	Frame fram = xv_get(DIRPUB(priv), WIN_FRAME);

	if (priv->sort_menu) sort_menu = priv->sort_menu;
	else {
		sort_menu = xv_create(XV_SERVER_FROM_WINDOW(DIRPUB(priv)),
				MENU_CHOICE_MENU,
				XV_INSTANCE_NAME, "sort_menu",
				MENU_GEN_PIN_WINDOW, fram, lab,
				NULL);

		xv_set(DIRPUB(priv), DIR_FILL_SORT_MENU, sort_menu, NULL);

		priv->sort_menu = sort_menu;
		/* too early during xv_create ! */
		xv_set(sort_menu, XV_SET_MENU, fram, NULL);
	}

	return xv_create(XV_SERVER_FROM_WINDOW(DIRPUB(priv)), MENUITEM,
				MENU_STRING, lab,
				MENU_PULLRIGHT, sort_menu,
				XV_KEY_DATA, dir_key, priv,
				XV_HELP_DATA, make_help(hlpfile, help),
				XV_KEY_DATA_REMOVE_PROC, XV_HELP, free_help_data,
				MENU_RELEASE,
				NULL);
}

static Menu_item dir_create_own_menu_item(Dir_private *priv, int which)
{
	char *hlp;
	Menu_item it;
	static struct {
		char *label, *help;
		menu_cb_t func;
		Menu_item (*create)(Dir_private *, char *, char *,
										menu_cb_t, char *);
	} own[] = {
		{ "Update Directory", "dir_update", note_update, create_item },
		{ "Select All", "select_all", note_select, create_item },
		{ "Deselect All", "note_deselect", note_deselect, create_item },
		{ "Hide/Show", "properties",(menu_cb_t)0, create_hide },
		{ "Sort By", "sorting", (menu_cb_t)0, create_sort },
		{ "File Properties...", "show_fileprops", note_show_props, create_item }
	};

	hlp = (char *)xv_get(DIRPUB(priv), XV_HELP_DATA);

	if (hlp) {
		char *save_hlp;

		save_hlp = xv_strsave(hlp);
		hlp = strtok(save_hlp, ":");
		hlp = xv_strsave(hlp);
		xv_free(save_hlp);
	}

	it = (*(own[which].create))(priv, XV_MSG(own[which].label), own[which].help,
													own[which].func, hlp);

	if (hlp) xv_free(hlp);
	return it;
}

static Xv_opaque dir_get(Xv_opaque self, int *status, Attr_attribute attr, va_list vali)
{
	Dir_private *priv = DIRPRIV(self);

	*status = XV_OK;
	switch ((int)attr) {
		case DIR_LAYOUT: return (Xv_opaque)priv->layout;
		case DIR_CLIENT_DATA: return priv->client_data;
		case DIR_COL_DISTANCE: return (Xv_opaque)priv->col_dist;
		case DIR_ROW_DISTANCE: return (Xv_opaque)priv->row_dist;
		case DIR_UPDATE_INTERVAL: return (Xv_opaque)priv->update_interval;
		case DIR_PROGRESS_PROC: return (Xv_opaque)priv->progress_proc;
		case DIR_MULTI_SELECT_PROC: return (Xv_opaque)priv->multi_select_proc;
		case DIR_DIRECTORY_FONT: return priv->boldfont;
		case DIR_NUM_VISIBLE_FILES: return priv->visible_num;
		case DIR_SELECT_PROC: return (Xv_opaque)priv->select_proc;
		case DIR_SELECT_FULL_PATH: return (Xv_opaque)priv->select_full_path;
		case DIR_DROP_PROC: return (Xv_opaque)priv->drop_proc;
		case DIR_SYSERR_PROC: return (Xv_opaque)priv->syserr_proc;
		case DIR_ROOTDROP_PROC: return (Xv_opaque)priv->root_action;
		case DIR_TELL_MATCH_PROC: return (Xv_opaque)priv->tell_match_proc;
		case DIR_SHOW_DIRS: return (Xv_opaque)priv->show_dirs;
		case DIR_SHOW_DOT_FILES: return (Xv_opaque)priv->show_dots;
		case DIR_PROPS_PROC: return (Xv_opaque)priv->props_proc;
		case DIR_FIND_PROC: return (Xv_opaque)priv->find_proc;
		case DIR_USE_ICONS: return (Xv_opaque)FALSE;
		case DIR_DO_DRAG_MOVE: return (Xv_opaque)FALSE;
		case DIR_ICON_DIST: return (Xv_opaque)priv->icon_dist;
		case DIR_MAX_NAME_LENGTH: return (Xv_opaque)priv->max_name_length;
		case DIR_ARGS_OFFSET: return (Xv_opaque)priv->args_offset;
		case DIR_INHIBIT: return (Xv_opaque)priv->inhibit;
		case DIR_CHOOSE_ONE: return (Xv_opaque)priv->choose_one;
		case DIR_LINE_ORIENTED: return (Xv_opaque)priv->line_oriented;
		case DIR_FILTER_PATTERN: return (Xv_opaque)priv->filter_sh_pattern;
		case DIR_USE_ED_FILTERING: return (Xv_opaque)priv->use_ed_filtering;
		case DIR_CONVERT_SEL: return (Xv_opaque)FALSE;
		case DIR_GET_ENTRY: return get_entry(priv, va_arg(vali, char *));
		case DIR_SELECTED_FILES:
			return (Xv_opaque)dir_get_all_selected(priv, va_arg(vali, int));
		case DIR_CATCH:
			{
				int v1, v2, ind;

				v1 = va_arg(vali, int);
				v2 = va_arg(vali, int);
				ind = dir_find_index(priv, v1, v2, (char *)0);
				if (ind < 0) return XV_NULL;
				return (Xv_opaque)(priv->visibles + ind);
			}
		case DIR_DIRECTORY:
			return (Xv_opaque)strcpy(my_get_directory, priv->dirname);
		case DIR_FILES:
		case DIR_FILE_VECTOR:
		case DIR_OWN_MENU_ITEM: return dir_create_own_menu_item(priv, 
													va_arg(vali, int));
		case DIR_AUTO_RENAME: return (Xv_opaque)FALSE;
		case SCROLLWIN_DROPPABLE:
			if (priv->inhibit & DIR_INHIBIT_DROP)
				return (Xv_opaque)SCROLLWIN_NONE;
			else return (Xv_opaque)SCROLLWIN_DROP;
		case SCROLLWIN_CREATE_SEL_REQ:
			return xv_create(self, FILE_REQUESTOR,
						FILE_REQ_CHECK_ACCESS, TRUE,
						NULL);
		default:
			*status = xv_check_bad_attr(DIRCANVAS, attr);
			return (Xv_opaque)XV_OK;
	}
}

static void dir_initialize_menu(Dir_private *priv, Attr_avlist avlist)
{
	priv->act_sorting = sort_dir_first;

	if (! dir_key) dir_key = xv_unique_key();
}

static Notify_value dircanvas_events(Xv_opaque dir, Event *ev, Notify_arg arg, Notify_event_type type)
{
	Dir_private *priv = DIRPRIV(dir);

	if (event_action(ev) == WIN_RESIZE) {
		Notify_value val;
		int hig, wid;

		val =  notify_next_event_func(dir, (Notify_event)ev, arg, type);

		wid = (int)xv_get(dir, XV_WIDTH);
		hig = (int)xv_get(dir, XV_HEIGHT);
		if (wid != priv->width || hig != priv->height) {
			priv->width = wid;
			priv->height = hig;
			make_layout(priv, TRUE);
		}
		return val;
	}
	else if (event_action(ev) == WIN_CLIENT_MESSAGE) {
		if (event_xevent(ev)->xclient.message_type ==
				(Atom)xv_get(XV_SERVER_FROM_WINDOW(dir), SERVER_ATOM,
										"_DRA_DIR_RESCAN"))
		{
			xv_set(dir, DIR_RESCAN, event_xevent(ev)->xclient.data.l[0], NULL);
			return NOTIFY_DONE;
		}
	}

	return notify_next_event_func(dir, (Notify_event)ev, arg, type);
}

typedef enum { KBD_CMD_SUNVIEW1, KBD_CMD_BASIC, KBD_CMD_FULL } xv_kbd_t;
static int dir_init(Frame owner, Xv_opaque self, Attr_avlist avlist, int *u)
{
	static Defaults_pairs xv_kbd_cmds_value_pairs[] = {
		{ "SunView1", KBD_CMD_SUNVIEW1 },
		{ "Basic", KBD_CMD_BASIC },
		{ "Full", KBD_CMD_FULL },
		{ NULL, KBD_CMD_SUNVIEW1 }
	};

	Attr_attribute *attrs;
	Dir_private *priv;

	priv = (Dir_private *)xv_alloc(Dir_private);
	if (!priv) return XV_ERROR;

	priv->public_self = self;
	((Xv_dir_public *)self)->private_data = (Xv_opaque)priv;

	priv->icon_dist = 4;
	priv->max_name_length = MAXPATHLEN - 3;

	priv->enable_fileprops = TRUE;
	priv->show_dirs = priv->show_dots = TRUE;
	priv->layout = DIR_LAYOUT_TRY_SQUARE;
	priv->selected_index = -1;
	priv->lineheight = 10; /* just to have it != 0 */
	priv->maxwidth = 10; /* just to have it != 0 */

	priv->drag_threshold = defaults_get_integer("openWindows.dragThreshold",
											"OpenWindows.DragThreshold", 5);
	priv->multiclick_time = 
				defaults_get_integer_check("openWindows.multiClickTimeout",
								"OpenWindows.MultiClickTimeout", 4, 2, 10);

	priv->full_keyboard = (defaults_get_enum("openWindows.keyboardCommands",
							"OpenWindows.KeyboardCommands",
							xv_kbd_cmds_value_pairs) == KBD_CMD_FULL);

	priv->col_dist = COL_GAP;
	priv->row_dist = 4;

	if (priv->full_keyboard) {
		if (priv->col_dist < FRAME_FOCUS_RIGHT_WIDTH)
			priv->col_dist = FRAME_FOCUS_RIGHT_WIDTH;
	}

	for (attrs=avlist; *attrs; attrs=attr_next(attrs)) switch ((int)*attrs) {
		case DIR_LAYOUT:
			if ((int)A1 < (int)DIR_LAYOUT_SINGLE_COLUMN ||
				(int)A1 > (int)DIR_LAYOUT_TRY_SQUARE) {
				xv_error(self,
					ERROR_SEVERITY, ERROR_RECOVERABLE,
					ERROR_PKG, DIRCANVAS,
					ERROR_LAYER, ERROR_PROGRAM,
					ERROR_STRING, XV_MSG("DIR_LAYOUT value out of range"),
					NULL);
			}
			else priv->layout = (Dir_layout)A1;
			ADONE;
		default: break;
	}

	dir_initialize_menu(priv, avlist);
	xv_set(self,
				WIN_NOTIFY_IMMEDIATE_EVENT_PROC, dircanvas_events,
				WIN_NOTIFY_SAFE_EVENT_PROC, dircanvas_events,
				NULL);

	priv->dragdrop = xv_create(self, FILEDRAG,
						FILEDRAG_ALLOW_MOVE, FALSE,
						FILEDRAG_ENABLE_STRING, FILEDRAG_NO_STRING,
						SEL_CONVERT_PROC, convert_selection,
						XV_KEY_DATA, DIR_CONVERT_SEL, priv,
						NULL);

	return XV_OK;
}

/*ARGSUSED*/
static int dir_destroy(Xv_opaque self, Destroy_status status)
{
	if (status == DESTROY_CLEANUP) {
		Dir_private *priv = DIRPRIV(self);
		Display *dpy = (Display *)xv_get(self, XV_DISPLAY);

		if (priv->dragdrop) xv_destroy(priv->dragdrop);

		/* we do not destroy priv->panel, because it is our WIN_FRAME's 
		 * child, so it will be destroyed when the frame is destroyed.
		 */

		/* we do not destroy priv->fileprops, because it is our WIN_FRAME's 
		 * child, so it will be destroyed when the frame is destroyed.
		 * Additionally, this is shared between instances of DIRCANVAS
		 */

		if (priv->copygc) XFreeGC(dpy, priv->copygc);
		if (priv->frame_gc) XFreeGC(dpy, priv->frame_gc);
		if (priv->normal_gc) XFreeGC(dpy, priv->normal_gc);
		if (priv->bold_gc) XFreeGC(dpy, priv->bold_gc);
		if (priv->bold_inv_gc) XFreeGC(dpy, priv->bold_inv_gc);
		if (priv->normal_inv_gc) XFreeGC(dpy, priv->normal_inv_gc);

		dir_free_files(priv);
		if (priv->dirname) xv_free(priv->dirname);
		if (priv->name_before_text_mode) xv_free(priv->name_before_text_mode);
		if (priv->match_sh_pattern) free(priv->match_sh_pattern);
		if (priv->match_ed_pattern) free(priv->match_ed_pattern);
		if (priv->filter_sh_pattern) free(priv->filter_sh_pattern);
		if (priv->filter_ed_pattern) free(priv->filter_ed_pattern);
		if (priv->match_context) xv_free_regexp(priv->match_context);
		if (priv->filter_context) xv_free_regexp(priv->filter_context);
		free((char *)priv);
	}
	return XV_OK;
}

const Xv_pkg xv_dircanvas_pkg = {
	"DirCanvas",
	ATTR_PKG_DIR,
	sizeof(Xv_dir_public),
	SCROLLWIN,
	dir_init,
	dir_set,
	dir_get,
	dir_destroy,
	0
};
