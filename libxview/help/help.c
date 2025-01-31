#ifndef lint
char help_c_sccsid[] = "@(#)help.c 1.77 93/06/28 RCS: $Id: help.c,v 4.25 2024/12/13 20:38:19 dra Exp $";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL_NOTICE
 *	file for terms of the license.
 */

#include <stdio.h>
#include <string.h>
#include <xview_private/i18n_impl.h>
#include <xview/xview.h>
#include <xview/notice.h>
#include <xview/canvas.h>
#include <xview/help.h>
#include <xview/cursor.h>
#include <xview/defaults.h>
#include <xview/notify.h>
#include <xview/panel.h>
#include <xview/frame.h>
#include <xview/svrimage.h>
#include <xview/sel_pkg.h>
#include <xview/scrollbar.h>
#include <xview/textsw.h>
#include <xview/richtext.h>
#include <xview_private/draw_impl.h>
#include <xview_private/win_info.h>
#include <xview_private/svr_impl.h>
#include <xview_private/panel_impl.h>
#include <unistd.h>
#include <sys/types.h>

extern char *xv_app_name;
extern wchar_t *xv_app_name_wcs;

Xv_private void xv_help_save_image(Xv_Window pw,
		int client_width, int unused_client_height, int mouse_x, int mouse_y);
Xv_private int xv_help_render(Xv_Window client_window, caddr_t client_data, Event *client_event);

/*
 * There is a maximum of 10 lines of text of 50 chars each visible in the
 * help text subwindow.  If the help text exceeds 10 lines, a scrollbar is
 * shown.
 */
#define HELPTEXTCOLS 50
#define HELPTEXTLINES 10
#define HELP_CANVAS_MARGIN 10
#define HELP_IMAGE_X 35
#define HELP_IMAGE_Y 5
#define HELP_IMAGE_WIDTH 80
#define HELP_IMAGE_HEIGHT 73
#define MORE_BUTTON_OFFSET 30


#  define MGLASS_BITMAP_WIDTH 128
#  define MGLASS_BITMAP_HEIGHT 125
#  define HELP_OUTER_WIDTH (MGLASS_BITMAP_WIDTH + 20)
#  define HELP_OUTER_HEIGHT (MGLASS_BITMAP_HEIGHT + 20)

static const unsigned short mglass_data[] = {
#  include "./mgls.icon"
};

static const unsigned short mglass_mask_data[] = {
#  include "./mgls_mask.icon"
};

#ifdef BEFORE_DRA_CHANGED_IT
#  define MAX_HELP_STRING_LENGTH 128
#else /* BEFORE_DRA_CHANGED_IT */
#  define MAX_HELP_STRING_LENGTH 1280
#endif /* BEFORE_DRA_CHANGED_IT */
#define MAX_FILE_KEY_LENGTH 64

#define GEOM_TEXT 1
#define GEOM_BOTPAN 2
#define GEOM_LEFTPAN 3

Xv_private void screen_adjust_gc_color(Xv_Window window, int gc_index);

Xv_private FILE * xv_help_find_file(Xv_server srv, char *filename);
Pkg_private int xv_help_get_arg(Xv_server srv, char *data, char **more_help);
Pkg_private char *xv_help_get_text(int use_textsw);

typedef struct {
     Frame	      help_frame;
     Server_image help_image;
     GC 	      help_stencil_gc;
	 int          use_textsw;
/*      Textsw	  help_textsw; */
	 Xv_window textwin;
     Scrollbar    text_sb;
     Server_image mglass_bitmap; /* magnifying glass only */
     Panel_item   mglass_msg;	/* magnifying glass Message item */
     Server_image mglass_stencil_bitmap; /* magnifying glass stencil */
     Panel_item   more_help_button;
} Help_info;

static Attr_attribute geom_key,
				help_notice_key,
				more_help_key,
				help_info_key = 0;

/*ARGSUSED*/
static void invoke_more_help(Xv_Window clwin, char *sys_command)
{
	char buffer[64];
	char *display_env;
	pid_t pid;

	/* Insure that More Help application comes up on same display as
	 * client application.
	 */
#ifdef BEFORE_DRA_CARED_FOR_MULTIDISPLAY
	display_env = defaults_get_string("server.name", "Server.Name", NULL);
#else /* BEFORE_DRA_CARED_FOR_MULTIDISPLAY */
	display_env = (char *)xv_get(XV_SERVER_FROM_WINDOW(clwin), XV_NAME);
#endif /* BEFORE_DRA_CARED_FOR_MULTIDISPLAY */
	if (display_env) {
		sprintf(buffer, "DISPLAY=%s", display_env);
		putenv(buffer);
	}

	/* Invoke More Help application */
	switch (pid = fork()) {
		case -1:	/* error */
			xv_error(XV_NULL,
				ERROR_LAYER, ERROR_SYSTEM,
				ERROR_STRING, XV_MSG("Help package:  cannot invoke More Help"),
				NULL);
			break;
		case 0:	/* child */
			(void)execl("/bin/sh", "sh", "-c", sys_command, (char *)0);
			_exit(-1);
			break;
		default:	/* parent */
			/* reap child -- do nothing with it... */
			notify_set_wait3_func(clwin, (Notify_func)notify_default_wait3,pid);
			break;
	}
}


static void more_help_proc(Panel_item item, Event *event)
{
	char *sys_command;

	sys_command = (char *)xv_get(item, XV_KEY_DATA, more_help_key);
	if (sys_command)
		invoke_more_help(event_window(event), sys_command);
}


static void help_request_failed(Xv_Window window, char *data, char *str)
{
	char message[256];
	Xv_Window notice_window;
	Xv_Notice help_notice;

	if (data)
		sprintf(message, XV_MSG("%s for %s."), str, data);
	else
		sprintf(message, XV_MSG("%s."), str);
	notice_window = xv_get(window, WIN_FRAME);

	if (!notice_window || !xv_get(notice_window,XV_IS_SUBTYPE_OF,FRAME_CLASS)) {
		/*
		 * No top level frame
		 * May be a menu, try using WIN_FRAME as key data
		 */
		notice_window = xv_get(window, XV_KEY_DATA, WIN_FRAME);

		if (!notice_window) {
			notice_window = window;	/* No frame: must be top level window */
		}
	}

	help_notice = xv_get(notice_window, XV_KEY_DATA, help_notice_key, NULL);

	if (!help_notice) {
		help_notice = xv_create(notice_window, NOTICE,
				NOTICE_MESSAGE_STRINGS, message, NULL,
				NOTICE_BUTTON_YES, XV_MSG("OK"),
				NOTICE_BUSY_FRAMES, notice_window, NULL,
				XV_SHOW, TRUE,
				NULL);

		xv_set(notice_window, XV_KEY_DATA, help_notice_key, help_notice, NULL);
	}
	else {
		xv_set(help_notice,
				NOTICE_MESSAGE_STRINGS, message, NULL,
				XV_SHOW, TRUE,
				NULL);
	}
}


static Notify_error help_frame_destroy_proc(Notify_client client,
										Destroy_status status)
{
	if (status == DESTROY_CLEANUP) {
		Help_info *hi;

		hi = (Help_info *) xv_get(client, XV_KEY_DATA, help_info_key);
		if (hi) {
			if (hi->help_image) xv_destroy(hi->help_image);
			if (hi->mglass_bitmap) xv_destroy(hi->mglass_bitmap);
			if (hi->mglass_stencil_bitmap)
				xv_destroy(hi->mglass_stencil_bitmap);

			XFreeGC((Display *)xv_get(client, XV_DISPLAY), hi->help_stencil_gc);
		}
	}
	return notify_next_destroy_func(client, status);
}

static Notify_value frame_interposer(Frame fram, Notify_event event,
								Notify_arg arg, Notify_event_type type)
{
	Event *ev = (Event *)event;
	Notify_value val;

	val = notify_next_event_func(fram, event, arg, type);

	if (event_action(ev) == WIN_RESIZE) {
		XEvent *xev = event_xevent(ev);

		if (xev && ! xev->xany.send_event) {
			int i;
			Xv_window sub;
			Xv_window text = XV_NULL;
			Panel leftpan = XV_NULL, botpan = XV_NULL;
			int moreheight = (int)xv_get(fram, WIN_CLIENT_DATA);
			int newframeheight = (int)xv_get(fram, XV_HEIGHT);
			int geom;

			for (i = 1; (sub = xv_get(fram, FRAME_NTH_SUBWINDOW, i)); i++) {
				geom = (int)xv_get(sub, XV_KEY_DATA, geom_key);
				if (geom == GEOM_TEXT) text = sub;
				else if (geom == GEOM_BOTPAN) botpan = sub;
				else if (geom == GEOM_LEFTPAN) leftpan = sub;
			}

			/* jetzt haben wir alles beieinander */

			xv_set(text, XV_HEIGHT, newframeheight - moreheight, NULL);
			xv_set(leftpan, XV_HEIGHT, newframeheight - moreheight, NULL);
			xv_set(botpan,
					XV_Y, newframeheight - moreheight,
					XV_HEIGHT, moreheight,
					NULL);
		}
	}

	return val;
}

static void init_keys(void)
{
	if (help_info_key) return;

	help_info_key = xv_unique_key();
	help_notice_key = xv_unique_key();
	more_help_key = xv_unique_key();
	geom_key = xv_unique_key();
}

Xv_private void xv_help_save_image(Xv_Window pw,
		int client_width, int unused_client_height, int mouse_x, int mouse_y)
{
	Help_info *help_info;
	Xv_Drawable_info *info;
	Xv_Screen screen;
	GC *gc_list;
	Window root;
	int root_x, root_y;

	DRAWABLE_INFO_MACRO(pw, info);
	screen = xv_screen(info);
	gc_list = (GC *) xv_get(screen, SCREEN_OLGC_LIST, pw);
	screen_adjust_gc_color(pw, SCREEN_CLR_GC);
	screen_adjust_gc_color(pw, SCREEN_SET_GC);
	root = xv_get(xv_get(screen, XV_ROOT), XV_XID);

	init_keys();
	help_info = (Help_info *) xv_get(screen, XV_KEY_DATA, help_info_key);
	if (!help_info) {
		help_info = xv_alloc(Help_info);
		xv_set(screen, XV_KEY_DATA, help_info_key, help_info, NULL);
	}
	/* destroy the cached help_image if the depth no longer matches */
	if (help_info->help_image &&
		(xv_depth(info) != xv_get(help_info->help_image, SERVER_IMAGE_DEPTH)))
	{
		xv_destroy(help_info->help_image);
		help_info->help_image = XV_NULL;
	}
	if (!help_info->help_image) {
		/* Create a server image for magnifying glass with help target image */
		help_info->help_image = xv_create(screen, SERVER_IMAGE,
				XV_WIDTH, MGLASS_BITMAP_WIDTH,
				XV_HEIGHT, MGLASS_BITMAP_HEIGHT,
				SERVER_IMAGE_DEPTH, xv_depth(info),
				NULL);
		DRAWABLE_INFO_MACRO(help_info->help_image, info);
		XFillRectangle(xv_display(info), xv_xid(info), gc_list[SCREEN_CLR_GC],
						0, 0, MGLASS_BITMAP_WIDTH+1, MGLASS_BITMAP_HEIGHT+1);
	}

	/* Fill magnifying glass with client window's background color */
	DRAWABLE_INFO_MACRO(help_info->help_image, info);
	XFillRectangle(xv_display(info), xv_xid(info),
			gc_list[SCREEN_CLR_GC],
			HELP_IMAGE_X, HELP_IMAGE_Y, HELP_IMAGE_WIDTH, HELP_IMAGE_HEIGHT);

	if (client_width == 0) {
		/* convention - REF (erthlkhbtrgkgc)
		 * This was sent by olwm - with root window coordinates!
		 * See also the Atom _OL_OWN_HELP.
		 */
		root_x = mouse_x - HELP_IMAGE_WIDTH / 2;
		root_y = mouse_y - HELP_IMAGE_HEIGHT / 2;
		if (root_x < 0) root_x = 0;
		if (root_y < 0) root_y = 0;
	}
	else {
		Xv_Drawable_info *src_info;

		DRAWABLE_INFO_MACRO(pw, src_info);
		win_translate_xy_internal(xv_display(src_info),
					xv_xid(src_info), root,
					mouse_x - HELP_IMAGE_WIDTH / 2,
					mouse_y - HELP_IMAGE_HEIGHT / 2,
					&root_x, &root_y);
	}

	XCopyArea(xv_display(info), root, xv_xid(info), gc_list[SCREEN_HELP_GC],
				root_x, root_y,
				HELP_IMAGE_WIDTH, HELP_IMAGE_HEIGHT,
				HELP_IMAGE_X, HELP_IMAGE_Y);
}

Xv_private int xv_help_render(Xv_Window client_window, caddr_t client_data,
									Event *client_event)
{
	char *text;
	CHAR client_name[80];
	XGCValues gc_values;
	int i;
	Xv_Drawable_info *dst_info;	/* destination */
	int length;
	Frame client_frame = XV_NULL;
	Xv_Cursor current_pointer;
	Rect help_frame_rect;
	Help_info *help_info;
	Panel mglass_panel = XV_NULL;	/* magnifying glass Panel */
	char *more_help_cmd;
	Panel more_help_panel;
	Xv_Window root_window;
	Xv_object screen;
	Xv_object server;
	Xv_Drawable_info *src_info;	/* source */
	CHAR *application_name;
	char better_appl[200];
	int textwidth, lineheight, panel_padding;
	char framehead[300], header[200];
	int need_sb = FALSE;
	Xv_Window view;

	SERVERTRACE((200, "help requested for '%s'\n", client_data));

	if (!help_info_key) help_info_key = xv_unique_key();

	DRAWABLE_INFO_MACRO(client_window, src_info);
	screen = xv_screen(src_info);
	server = xv_server(src_info);

	help_info = (Help_info *)xv_get(screen, XV_KEY_DATA, help_info_key);
	if (!help_info) {
		help_info = xv_alloc(Help_info);
		xv_set(screen, XV_KEY_DATA, help_info_key, help_info, NULL);
	}

	if (xv_help_get_arg(server, client_data, &more_help_cmd) == XV_OK)
		text = xv_help_get_text(help_info->use_textsw);
	else
		text = NULL;
	if (!text) {
		/* don't want a notice for nonavailable footer help,
		 * just return XV_ERROR - see also in
		 */
		if (! xv_get(client_window, XV_KEY_DATA, FRAME_FOOTER_WINDOW)) {
			help_request_failed(client_window, client_data,
								XV_MSG("No help is available"));
		}
		return XV_ERROR;
	}
	if (event_action(client_event) == ACTION_MORE_HELP ||
			event_action(client_event) == ACTION_MORE_TEXT_HELP) {
		if (more_help_cmd) {
			invoke_more_help(client_window, more_help_cmd);
			return XV_OK;
		}
		else {
			help_request_failed(client_window, client_data,
								XV_MSG("More help is not available"));
			return XV_ERROR;
		}
	}

#ifdef OW_I18N
	if ((application_name = (CHAR *) xv_get(server, XV_APP_NAME_WCS)) == NULL) {
		application_name = wsdup(xv_app_name_wcs);
	}
#else
	if ((application_name = (CHAR *) xv_get(server, XV_APP_NAME)) == NULL) {
		application_name = xv_strsave(xv_app_name);
	}
#endif

	{
		Xv_opaque fram;

		/* we start from client_window and search for a base frame */
		if (!help_info->help_frame) {
			/* Help frame has not been created yet */
			fram = xv_get(client_window, WIN_FRAME);
			if (!fram || !xv_get(fram, XV_IS_SUBTYPE_OF, FRAME_CLASS)) {
				/* No frame: may be a menu, in which case client_frame
				 * can be found in XV_KEY_DATA WIN_FRAME.
				 */
				fram = xv_get(client_window, XV_KEY_DATA, WIN_FRAME);
			}

			if (fram) {
				char *lab, *beforecolon, labelbuf[200];

				/* now we have found a frame - look for its parent, that might
				 * also be a frame....
				 */
				client_frame = fram;

				while ((fram = xv_get(fram, XV_OWNER))) {
					if (xv_get(fram, XV_IS_SUBTYPE_OF, FRAME)) {
						/* aah, a better frame */
						client_frame = fram;
					}
				}

				lab = (char *)xv_get(client_frame, XV_LABEL);
				if (lab) {
					strcpy(labelbuf, lab);
					if ((beforecolon = strtok(labelbuf, ":,-@#$%^&*()_~`';|{}[]"))){
						strcpy(better_appl, beforecolon);
					}
					else {
						strcpy(better_appl, lab);
					}
					while (better_appl[strlen(better_appl)-1] == ' ') {
						better_appl[strlen(better_appl)-1] = '\0';
					}
					if (0 != strcmp(application_name, better_appl)) {
						fprintf(stderr, "%s`%s-%d: '%s' != '%s'\n",
								__FILE__, __FUNCTION__, __LINE__,
								application_name, better_appl);
					}
/* 					application_name = better_appl; */
				}
			}
			else {
				char *an = (char *)xv_get(server, XV_APP_NAME);

				/* mae hynny yn digwydd mewn olwmslave...
				 * dydw i ddim yn gweld a neges yma
				 */
/* 				fprintf(stderr, "%s`%s-%d: cannor find a FRAME\n", */
/* 									__FILE__, __FUNCTION__, __LINE__); */
				application_name = xv_strsave(an);;
			}
		}
	}

	/* Change to busy pointer */
	current_pointer = xv_get(client_window, WIN_CURSOR);
	xv_set(client_window, WIN_CURSOR, xv_get(screen, SCREEN_BUSY_CURSOR), NULL);

	length = STRLEN(application_name);

	if (length > 73) length = 73;

	STRCPY(client_name, application_name);
	/*strncpy(client_name, application_name, length); */

	client_name[length] = 0;

#ifdef OW_I18N
	SPRINTF(client_name, "%ws: Help", client_name);
#else
/* 	SPRINTF(client_name, "%s%s", client_name, XV_MSG(": Help")); */
#endif

	if (!help_info->help_frame) {
	 	help_info->use_textsw =
				defaults_get_boolean("openWindows.useTextswForHelp",
								"OpenWindows.UseTextswForHelp", FALSE);

		root_window = xv_get(screen, XV_ROOT);
		help_info->help_frame = xv_create(client_frame, FRAME_HELP,
					WIN_PARENT, root_window,
					XV_KEY_DATA, help_info_key, help_info,
#ifdef OW_I18N
					XV_LABEL_WCS, client_name,
					WIN_USE_IM, FALSE,
#else
					XV_LABEL, client_name,
#endif /* OW_I18N */
					NULL);
		help_frame_rect = *(Rect *) xv_get(help_info->help_frame, XV_RECT);
		help_frame_rect.r_left = 0;
		help_frame_rect.r_top = 0;
		frame_set_rect(help_info->help_frame, &help_frame_rect);
		notify_interpose_destroy_func(help_info->help_frame,
									  help_frame_destroy_proc);

	 	if (help_info->use_textsw) {
			Textsw tsw;
			Menu menu, newmenu;
			Menu_item item;

			tsw = help_info->textwin = xv_create(help_info->help_frame, TEXTSW,
					XV_X, HELP_OUTER_WIDTH,
					XV_Y, 0,
					WIN_COLUMNS, HELPTEXTCOLS,
					WIN_ROWS, HELPTEXTLINES,
					TEXTSW_IGNORE_LIMIT, TEXTSW_INFINITY,
					TEXTSW_LINE_BREAK_ACTION, TEXTSW_WRAP_AT_WORD,
					TEXTSW_LOWER_CONTEXT, -1, /* disable automatic scrolling */
					TEXTSW_DISABLE_LOAD, TRUE,
					TEXTSW_READ_ONLY, TRUE,
					XV_KEY_DATA, geom_key, GEOM_TEXT,
					NULL);
			view = xv_get(help_info->textwin, OPENWIN_NTH_VIEW, 0);
			xv_set(view, XV_HELP_DATA, "xview:helpWindow", NULL);
			help_info->text_sb = xv_get(help_info->textwin,
					   OPENWIN_VERTICAL_SCROLLBAR, view);
			xv_set(help_info->text_sb, SCROLLBAR_SPLITTABLE, FALSE, NULL);

			/* the OPEN LOOK UI spec says not much about the text pane
			 * in help windows. Let's restrict the full TEXTSW menu...
			 *
			 * Save as...
			 * Copy
			 * Find and Replace...
			 */
			newmenu = xv_create(XV_NULL, MENU,
						MENU_TITLE_ITEM, XV_MSG("Text Pane"),
						NULL);

			menu = xv_get(tsw, TEXTSW_SUBMENU_FILE);
			if ((item = xv_find(menu, MENUITEM,
						XV_AUTO_CREATE, FALSE,
						MENU_VALUE, TEXTSW_MENU_STORE,
						NULL)))
			{
				xv_set(newmenu, MENU_APPEND_ITEM, item, NULL);
			}

			menu = xv_get(tsw, TEXTSW_SUBMENU_EDIT);
			if ((item = xv_find(menu, MENUITEM,
						XV_AUTO_CREATE, FALSE,
						MENU_VALUE, TEXTSW_MENU_COPY,
						NULL)))
			{
				xv_set(newmenu, MENU_APPEND_ITEM, item, NULL);
			}

			menu = xv_get(tsw, TEXTSW_SUBMENU_FIND);
			if ((item = xv_find(menu, MENUITEM,
						XV_AUTO_CREATE, FALSE,
						MENU_VALUE, TEXTSW_MENU_FIND_AND_REPLACE,
						NULL)))
			{
				xv_set(newmenu, MENU_APPEND_ITEM, item, NULL);
			}

			xv_set(tsw, WIN_MENU, newmenu, NULL);

		}
		else {
			help_info->textwin = xv_create(help_info->help_frame, RICHTEXT,
					XV_X, HELP_OUTER_WIDTH,
					XV_Y, 0,
					WIN_COLUMNS, HELPTEXTCOLS,
					WIN_ROWS, HELPTEXTLINES,
					XV_KEY_DATA, geom_key, GEOM_TEXT,
					XV_HELP_DATA, "xview:helpWindowRT",
					NULL);
			help_info->text_sb = xv_create(help_info->textwin, SCROLLBAR, NULL);
		}
		mglass_panel = xv_create(help_info->help_frame, PANEL,
				XV_X, 0,
				XV_Y, 0,
				XV_WIDTH, HELP_OUTER_WIDTH,
				XV_HEIGHT, xv_get(help_info->textwin, XV_HEIGHT),
				XV_HELP_DATA, "xview:helpWindow",
				XV_KEY_DATA, geom_key, GEOM_LEFTPAN,
				NULL);
		help_info->mglass_msg = xv_create(mglass_panel, PANEL_MESSAGE,
				XV_HELP_DATA, "xview:helpMagnifyingGlass",
				NULL);

		textwidth = (int)xv_get(help_info->textwin, XV_WIDTH);
		lineheight = (int)xv_get(help_info->textwin, XV_HEIGHT)
							/ HELPTEXTLINES;
		panel_padding = lineheight - 6;

		more_help_panel = xv_create(help_info->help_frame, PANEL,
				XV_X, 0,
				PANEL_EXTRA_PAINT_HEIGHT, panel_padding,
				WIN_BELOW, help_info->textwin,
				XV_WIDTH, HELP_OUTER_WIDTH + textwidth,
				XV_HELP_DATA, "xview:helpWindow",
				XV_KEY_DATA, geom_key, GEOM_BOTPAN,
				NULL);
		help_info->more_help_button = xv_create(more_help_panel, PANEL_BUTTON,
					PANEL_ITEM_LAYOUT_ROLE, PANEL_ROLE_CENTER,
					XV_Y, panel_padding,
					PANEL_LABEL_STRING, XV_MSG("More"),
					PANEL_NOTIFY_PROC, more_help_proc,
					XV_HELP_DATA, "xview:moreHelpButton",
					XV_SHOW, FALSE,
					NULL);
		xv_set(more_help_panel, WIN_FIT_HEIGHT, 1, NULL);

		/* the OPEN LOOK spec wants it centered in the help window (p. 90) */
		panel_layout_items(more_help_panel, FALSE);

		/* panel_layout_items determines a default button -
		 * we don't want that
		 */
		xv_set(more_help_panel, PANEL_DEFAULT_ITEM, XV_NULL, NULL);

		window_fit(help_info->help_frame);

		xv_set(help_info->help_frame,
			   WIN_CLIENT_DATA, xv_get(more_help_panel, XV_HEIGHT),
			   FRAME_MIN_SIZE, 300, xv_get(help_info->help_frame, XV_HEIGHT),
			   FRAME_MAX_SIZE, xv_get(help_info->help_frame, XV_WIDTH),
								xv_get(root_window, XV_HEIGHT),
			   NULL);

		notify_interpose_event_func(help_info->help_frame, frame_interposer,
									NOTIFY_SAFE);
		notify_interpose_event_func(help_info->help_frame, frame_interposer,
									NOTIFY_IMMEDIATE);

	}
	else {
		/* Help frame already exists: set help frame header and
		 * empty text subwindow.
		 */
	 	if (help_info->use_textsw) {
			textsw_reset(help_info->textwin, 0, 0);
		}
	}

	/* Draw magnifying glass over help image */
	if (!help_info->mglass_bitmap) {
		help_info->mglass_bitmap = xv_create(screen, SERVER_IMAGE,
						XV_WIDTH, MGLASS_BITMAP_WIDTH,
						XV_HEIGHT, MGLASS_BITMAP_HEIGHT,
						SERVER_IMAGE_DEPTH, 1,
						SERVER_IMAGE_BITS, mglass_data,
						NULL);
		help_info->mglass_stencil_bitmap = xv_create(screen, SERVER_IMAGE,
						XV_WIDTH, MGLASS_BITMAP_WIDTH,
						XV_HEIGHT, MGLASS_BITMAP_HEIGHT,
						SERVER_IMAGE_DEPTH, 1,
						SERVER_IMAGE_BITS, mglass_mask_data,
						NULL);

		/* there was dirt at the bottom of the image ... */
		{
			Xv_Drawable_info *inf;
			XGCValues gcv;
			GC gc;

			DRAWABLE_INFO_MACRO(help_info->mglass_stencil_bitmap, inf);
			gcv.foreground = 1;
			gcv.background = 0;
			gcv.fill_style = FillSolid;

			gc = XCreateGC(xv_display(inf), xv_xid(inf),
							GCForeground | GCBackground | GCFillStyle, &gcv);
			XFillRectangle(xv_display(inf), xv_xid(inf), gc,
							0, HELP_OUTER_HEIGHT - 2, MGLASS_BITMAP_WIDTH+1, 5);

			DRAWABLE_INFO_MACRO(help_info->mglass_bitmap, inf);
			XSetForeground(xv_display(inf), gc, 0L);
			XFillRectangle(xv_display(inf), xv_xid(inf), gc,
							0, HELP_OUTER_HEIGHT - 2, MGLASS_BITMAP_WIDTH+1, 5);
			XFreeGC(xv_display(inf), gc);
		}
	}
	if (!help_info->help_stencil_gc) {
		Xv_Drawable_info *mglsbm_info;
		Xv_Drawable_info *clip_info;

		DRAWABLE_INFO_MACRO(mglass_panel, dst_info);
		DRAWABLE_INFO_MACRO(help_info->mglass_stencil_bitmap, clip_info);
		DRAWABLE_INFO_MACRO(help_info->mglass_bitmap, mglsbm_info);
		gc_values.foreground = xv_fg(dst_info);
		gc_values.background = xv_bg(dst_info);
		gc_values.fill_style = FillOpaqueStippled;
		gc_values.stipple = xv_xid(mglsbm_info);
		gc_values.clip_mask = xv_xid(clip_info);
		help_info->help_stencil_gc = XCreateGC(xv_display(dst_info),
											   xv_xid(dst_info),
											   GCForeground | GCBackground |
											   GCFillStyle | GCStipple |
											   GCClipMask, &gc_values);
	}

	/* check to make sure that server image can actually be displayed in
	   the frame.  If not, then just display the magnifying glass. */

	if (xv_get(help_info->help_image, SERVER_IMAGE_DEPTH) ==
			xv_get(help_info->help_frame, XV_DEPTH)) {
		DRAWABLE_INFO_MACRO(help_info->help_image, dst_info);
		XFillRectangle(xv_display(dst_info), xv_xid(dst_info),
					   help_info->help_stencil_gc, 0, 0,
					   MGLASS_BITMAP_WIDTH + 1, MGLASS_BITMAP_HEIGHT + 1);
		xv_set(help_info->mglass_msg,
			   PANEL_LABEL_IMAGE, help_info->help_image,
			   NULL);
	}
	else {
		xv_set(help_info->mglass_msg,
			   PANEL_LABEL_IMAGE, help_info->mglass_bitmap,
			   NULL);
	}

	xv_set(help_info->more_help_button,
		   XV_SHOW, more_help_cmd ? TRUE : FALSE,
		   XV_KEY_DATA, more_help_key, more_help_cmd,
		   NULL);

	strcpy(header, text);
	text = xv_help_get_text(help_info->use_textsw);
	sprintf(framehead, "%s Help: %s", client_name, strtok(header, "\n"));
	xv_set(help_info->help_frame, XV_LABEL, framehead, NULL);
	if (text) {
		if (strlen(text) <= 2) {
			text = xv_help_get_text(help_info->use_textsw);
		}
	}
	if (help_info->use_textsw) {
		int top, bot;

		for (i = 0; text; i++) {
			textsw_insert(help_info->textwin, text, (int)strlen(text));
			text = xv_help_get_text(help_info->use_textsw);
		}
		xv_set(help_info->textwin, TEXTSW_FIRST, 0, NULL);

		textsw_file_lines_visible(xv_get(help_info->textwin,
											OPENWIN_NTH_VIEW, 0), &top, &bot);
		need_sb = (bot - top + 1 < i);
	}
	else {
		char *p;

		if (text) {
			if ((p = strchr(text, '\n'))) *p = '\0';
			xv_set(help_info->textwin, RICHTEXT_START, text, NULL);
			text = xv_help_get_text(help_info->use_textsw);
		}
		for (i = 1; text; i++) {
			if ((p = strchr(text, '\n'))) *p = '\0';
			xv_set(help_info->textwin, RICHTEXT_APPEND, text, NULL);
			text = xv_help_get_text(help_info->use_textsw);
		}
		xv_set(help_info->textwin, RICHTEXT_FILLED, NULL);

		/* gets the number of 'visible lines' */
		i = (int)xv_get(help_info->textwin, RICHTEXT_FILLED);
		need_sb = (i > 10);
	}

	/* see OL spec, p. 89: When help text has more than 10 lines,
	 * Help windows have a scrollbar and resize corners.
	 */
	xv_set(help_info->help_frame, FRAME_SHOW_RESIZE_CORNER, need_sb, NULL);
	xv_set(help_info->text_sb, XV_SHOW, need_sb, NULL);

	/* Show window, in front of all other windows */
	xv_set(help_info->help_frame, XV_SHOW, TRUE, WIN_FRONT, NULL);

	/* Restore pointer */
	if (current_pointer) {
		xv_set(client_window, WIN_CURSOR, current_pointer, NULL);
	}
	else {
		xv_set(client_window,
				WIN_CURSOR, xv_get(screen, XV_KEY_DATA, WIN_CURSOR),
				NULL);
	}

	return XV_OK;
}


/*
 * Public "show help" routine
 */
Xv_public int xv_help_show(Xv_Window client_window, char *client_data,
									Event *client_event)
/* client_data: "file:key" */
{
	char *err_msg;
	char file_key[MAX_FILE_KEY_LENGTH];	/* from String File */
	FILE *file_ptr;
	char help_string[MAX_HELP_STRING_LENGTH];	/* from String File */
	char *help_string_filename;
	char *msg;
	Xv_Window window;

	init_keys();

	if (event_action(client_event) == ACTION_TEXT_HELP ||
			event_action(client_event) == ACTION_MORE_TEXT_HELP) {
		int format;
		long length;
		char *sel_string;	/* from Primary Selection */
		Selection_requestor sr = xv_create(client_window, SELECTION_REQUESTOR,
						SEL_RANK, XA_PRIMARY,
						SEL_TYPE, XA_STRING,
						SEL_TIME, &event_time(client_event),
						NULL);

		sel_string = (char *)xv_get(sr, SEL_DATA, &length, &format);
		xv_destroy(sr);

		if (length == SEL_ERROR) {
			help_request_failed(client_window, NULL,
					XV_MSG("No Primary Selection"));
			return XV_ERROR;
		}

		/* Get the Help String File name */
		window = client_window;
		do {
			help_string_filename = (char *)xv_get(window, HELP_STRING_FILENAME);
		} while (!help_string_filename && (window = xv_get(window, XV_OWNER)));
		if (!help_string_filename) {
			free(sel_string);
			help_request_failed(client_window, NULL,
					XV_MSG("No Help String Filename specified for window"));
			return XV_ERROR;
		}

		/* Search the Help String File for the Primary Selection */
		file_ptr = xv_help_find_file(XV_SERVER_FROM_WINDOW(client_window),
										help_string_filename);
		if (!file_ptr) {
			free(sel_string);
			help_request_failed(client_window, NULL,
					XV_MSG("Help String File not found"));
			return XV_ERROR;
		}
		client_data = NULL;
		while (fscanf(file_ptr, "%s %s\n", help_string, file_key) != EOF) {
			if (!strcmp(help_string, sel_string)) {
				client_data = file_key;
				break;
			}
		}
		fclose(file_ptr);

		/* This mechanism is a little strange: the programmer has set
		 * HELP_STRING_FILENAME, "myAppl" on some window (maybe even the frame).
		 * Now the user select a piece of text ("lalala"), then enters
		 * Ctrl+Help, and then we come here: assume that xv_help_find_file
		 * has found a file "myAppl" (not myAppl.info) in $HELPPATH.
		 * The "while fscanf" loop searches for a line that looks like
		 * lalala whatever
		 * Now client_data = "whatever" - and this is used for the call
		 * to xv_help_render(client_window, client_data, client_event);
		 *
		 * So, the HELP_STRING_FILENAME contains some sort of translation
		 * "Selected Text" to "Help Token".
		 * What is that good for?
		 */

		if (!client_data) {
			err_msg = XV_MSG("\" not found in Help String File");
			msg = xv_malloc(strlen(sel_string) + strlen(err_msg) + 2);
			sprintf(msg, "\"%s%s", sel_string, err_msg);
			help_request_failed(client_window, NULL, msg);
			free(msg);
			free(sel_string);
			return XV_ERROR;
		}
		free(sel_string);
	}

	if (event_action(client_event) != ACTION_MORE_HELP &&
			event_action(client_event) != ACTION_MORE_TEXT_HELP)
	{
		XEvent *xev = event_xevent(client_event);

		if (xev) {
			if (xev->type == ClientMessage) {
				XClientMessageEvent *xcl = (XClientMessageEvent *)xev;
				int rootx, rooty;

				window = client_window;
				rootx = (int)xcl->data.l[2];
				rooty = (int)xcl->data.l[3];

				/* convention - REF (erthlkhbtrgkgc)
				 * This was sent by olwm - with root window coordinates!
				 * See also the Atom _OL_OWN_HELP.
				 */
				/* this copies (part of) the root window into help_image */
				xv_help_save_image(window, 0,0, rootx, rooty);
			}
			else {
				int client_height = (int)xv_get(client_window, XV_HEIGHT);
				int client_width = (int)xv_get(client_window, XV_WIDTH);

				/* this copies (part of) the root window into help_image */
				xv_help_save_image(client_window, client_width, client_height,
								event_x(client_event), event_y(client_event));
			}
		}
		else {
			xv_error ( XV_NULL,
					ERROR_LAYER, ERROR_SYSTEM,
					ERROR_STRING, XV_MSG("Help requested without XEvent"),
					NULL );
		}
	}
	return xv_help_render(client_window, client_data, client_event);
}
