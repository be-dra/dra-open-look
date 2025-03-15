#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)xv_init.c 20.62 92/07/07  DRA: $Id: xv_init.c,v 4.4 2025/03/13 16:05:46 dra Exp $";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL_NOTICE 
 *	file for terms of the license.
 */

#ifdef _XV_DEBUG
#include <xview_private/xv_debug.h>
#else
#include <stdio.h>
#endif
#include <xview_private/i18n_impl.h>
#include <xview/defaults.h>
#include <xview_private/portable.h>
#include <xview/pkg.h>
#include <xview/xview_xvin.h>
#include <X11/Xlib.h>

#include <xview/server.h>
#include <xview/window.h>
#include <xview/canvas.h>
#include <xview/panel.h>
#include <xview/xv_version.h>
#include <xview_private/xview_impl.h>

#define ERROR_MSG "Cannot open connection to window server: "

/* XXX: This should probably become integrated into the get/set paradigm */
int (*xv_error_proc)(Xv_object, Attr_avlist);
XErrorHandler xv_x_error_proc;
XErrorHandler xv_xlib_error_proc;
static int      xv_init_called;	/* = FALSE */

Xv_private void xv_init_x_pr(void);
static int xv_handle_xio_errors(Display *display);
static char *xv_base_name(char *fullname);

#ifdef OW_I18N
Xv_private_data wchar_t *xv_app_name_wcs;
#endif /* OW_I18N */

Xv_private_data char *xv_app_name;
Xv_private_data char 	*xv_instance_app_name = NULL;
Xv_private_data int	_xv_use_locale;
Xv_private_data int	_xv_is_multibyte;
Xv_private int	notify_exclude_fd;
Xv_private Defaults_pairs xv_kbd_cmds_value_pairs[4];

#ifndef XGETTEXT
/*
 * xv_domain: XView libraries text domain name. This value will be
 * overwritten in following xv_init() call.
 */
Xv_private_data CONST char		*xv_domain = XV_TEXT_DOMAIN;
#endif

Xv_public_data Display *xv_default_display;
Xv_public_data Xv_Screen xv_default_screen;
Xv_public_data Xv_Server xv_default_server;

/* nobody uses them inside XView */
#ifdef ORIGINAL
Xv_public_data char xv_iso_cancel;
Xv_public_data char xv_iso_default_action;
Xv_public_data char xv_iso_input_focus_help;
Xv_public_data char xv_iso_next_element;
Xv_public_data char xv_iso_select;
#else /* ORIGINAL */
static char xv_iso_cancel;
static char xv_iso_default_action;
static char xv_iso_input_focus_help;
static char xv_iso_next_element;
static char xv_iso_select;
static int is_mouseless = FALSE;
#endif /* ORIGINAL */

static void init_custom_attrs(void);

/* I want to run the XView3.0 clients (mailtool etc.) with the
 * XView3.2 shared lib on SUNOS41.
 * However, in XView3.2:
 *    XV_INIT_ARGS          = XV_ATTR(ATTR_INT_PAIR, 4),
 *    XV_INIT_ARGC_PTR_ARGV = XV_ATTR(ATTR_INT_PAIR, 7),
 * while in XView3.0:
 *    XV_INIT_ARGS          = XV_ATTR(ATTR_INT_PAIR, 5),
 *    XV_INIT_ARGC_PTR_ARGV = XV_ATTR(ATTR_INT_PAIR, 8),
 *
 * So, I have to be 'backward compatible:
 */

#  ifdef SUNOS41
typedef enum {
	XV_INIT_ARGS_DRA_BACKWARD          = XV_ATTR(ATTR_INT_PAIR, 5),
	XV_INIT_ARGC_PTR_ARGV_DRA_BACKWARD = XV_ATTR(ATTR_INT_PAIR, 8)
} Xv_attr_dra_backward;
#    define DRA_BACKWARD(_attr_) case _attr_:
#  else /* SUNOS41 */
#    define DRA_BACKWARD(_attr_)
#  endif /* SUNOS41 */

/*
 * Initialize XView.
 */
Xv_public Xv_object xv_init(Attr_attribute attr1, ...)
{
	Attr_attribute avarray[ATTR_STANDARD_SIZE];
	Attr_avlist attrs_start = avarray;
	register Attr_avlist attrs;
	va_list valist;
	void (*help_proc) (char *) = xv_usage;
	int parse_result = 0, argc = 0;
	char **argv = (char **)NULL, *server_name = (char *)NULL;
	Xv_object server = XV_NULL;
	extern int _Xdebug;

	/* can only be called once */
	if (xv_init_called)
		return ((Xv_object) NULL);

	/* 
	 * Initialize the version string and number
	 */
	xv_version_number = XV_VERSION_NUMBER;
	xv_version_string = (char *)xv_malloc(strlen(XV_VERSION_STRING) + 1);
	XV_BCOPY(XV_VERSION_STRING, xv_version_string,
			strlen(XV_VERSION_STRING) + 1);
	xv_domain = xv_malloc(sizeof(XV_TEXT_DOMAIN) + 6);
	(void)sprintf(xv_domain, "%s_%04d", XV_TEXT_DOMAIN, xv_version_number);

	/*
	 * Initialize table of customizable attributes
	 */
	init_custom_attrs();

	xv_init_called = TRUE;

	xv_error_proc = (int (*)(Xv_object, Attr_avlist))0;
	xv_x_error_proc = (XErrorHandler)0;

	/* initialize the pixrect-to-x rop op table */
	xv_init_x_pr();

	/* silence the shut-down error messages, can turn on with option flag */
	(void)XSetIOErrorHandler(xv_handle_xio_errors);

	if (attr1) {
		VA_START(valist, attr1);
		copy_va_to_av(valist, attrs_start, attr1);
		va_end(valist);
	}
	else
		attrs_start[0] = 0;

	/*
	 * Get argv, argc for preparsing done below
	 * Also get xv_app_name necessary for server creation
	 */
	for (attrs = attrs_start; *attrs; attrs = attr_next(attrs)) {
		switch ((Xv_attr) attrs[0]) {
			case XV_INIT_ARGS:
				DRA_BACKWARD(XV_INIT_ARGS_DRA_BACKWARD)

						argc = (int)attrs[1];
				argv = (char **)attrs[2];

				if (xv_app_name) {
					xv_free(xv_app_name);
				}

#ifdef OW_I18N
				if (xv_app_name_wcs) {
					xv_free(xv_app_name_wcs);
				}
#endif /* OW_I18N */

				if (argv[0]) {
					xv_app_name = xv_base_name(argv[0]);

#ifdef OW_I18N
					xv_app_name_wcs = _xv_mbstowcsdup(xv_app_name);
#endif /* OW_I18N */
				}
				break;

			case XV_INIT_ARGC_PTR_ARGV:
				DRA_BACKWARD(XV_INIT_ARGC_PTR_ARGV_DRA_BACKWARD)
						argc = *(int *)attrs[1];
				argv = (char **)attrs[2];

				if (xv_app_name) {
					xv_free(xv_app_name);
				}

#ifdef OW_I18N
				if (xv_app_name_wcs) {
					xv_free(xv_app_name_wcs);
				}
#endif /* OW_I18N */

				if (argv[0]) {
					xv_app_name = xv_base_name(argv[0]);

#ifdef OW_I18N
					xv_app_name_wcs = _xv_mbstowcsdup(xv_app_name);
#endif /* OW_I18N */
				}
				break;

			default:
				break;
		}
	}
	/* Preparse "-display <name>" for server creation */
	/*
	 *  Preparse
	 *      "-display <name>" for server creation
	 *  and
	 *      "-name <name>" for application
	 *      "-lc_basiclocale", etc for locale announcement
	 */
	for (; (argv && *argv); argv++) {
		if (strcmp(*argv, "-display") == 0 || strcmp(*argv, "-Wr") == 0) {
			server_name = *++argv;
			break;
		}
		else if (!strncmp(*argv, "-sync", (size_t)5)) {
			_Xdebug = True;
		}
		else if (strcmp(*argv, "-name") == 0) {
			xv_instance_app_name = *++argv;
			continue;
		}
	}

	/*
	 * Check if xv_app_name is set.
	 * If not, set it to "xview"
	 * It might be NULL at this point, if the application
	 * calls xv_init without using any XV_INIT* attributes or
	 * the application calls xv_create without any call to
	 * xv_init.
	 *
	 * Also check the application instance name
	 */
	if (!xv_app_name) {
		xv_app_name = strdup("xview");
	}

	if (xv_instance_app_name == NULL)
		xv_instance_app_name = xv_app_name;

	/*
	 *  Override any command line arguments with xv_init parameters
	 */
	for (attrs = attrs_start; *attrs; attrs = attr_next(attrs)) {
		switch (attrs[0]) {

			case XV_USE_LOCALE:
				_xv_use_locale = (int)attrs[1];
				ATTR_CONSUME(attrs[0]);
				break;

			case XV_USAGE_PROC:
				help_proc = (void (*)(char *))attrs[1];
				ATTR_CONSUME(attrs[0]);
				break;

			case XV_INIT_ARGS:
				DRA_BACKWARD(XV_INIT_ARGS_DRA_BACKWARD)
						argc = (int)attrs[1];
				argv = (char **)attrs[2];

				parse_result =
						xv_parse_cmdline(xv_app_name, &argc, argv, FALSE);


				ATTR_CONSUME(attrs[0]);
				break;

			case XV_INIT_ARGC_PTR_ARGV:
				DRA_BACKWARD(XV_INIT_ARGC_PTR_ARGV_DRA_BACKWARD)
						argv = (char **)attrs[2];

				parse_result = xv_parse_cmdline(xv_app_name,
						(int *)attrs[1], argv, TRUE);
				/* want to see this in server_init */
				break;

			case XV_ERROR_PROC:
				xv_error_proc = (int (*)(Xv_object, Attr_avlist))attrs[1];
				ATTR_CONSUME(attrs[0]);
				break;

			case XV_X_ERROR_PROC:
				xv_x_error_proc = (XErrorHandler)attrs[1];
				ATTR_CONSUME(attrs[0]);
				break;

				/*
				 * Any attribute need to pass on to the server pkg should be
				 * listed in followings (otherwise consumed).
				 */
			case XV_LC_BASIC_LOCALE:
			case XV_LC_DISPLAY_LANG:
			case XV_LC_INPUT_LANG:
			case XV_LC_NUMERIC:
			case XV_LC_TIME_FORMAT:
			case XV_LOCALE_DIR:

#if defined(OW_I18N) && defined(FULL_R5)
			case XV_IM_PREEDIT_STYLE:
			case XV_IM_STATUS_STYLE:
#endif

				break;

			case XV_WANT_ROWS_AND_COLUMNS:
			case XV_APP_HELP_FILE:
			case XV_APP_NAME:
				break;

			default:
				ATTR_CONSUME(attrs[0]);
				break;
		}
	}

	/*
	 * Check if any SERVER object has been created.  If not, then create one
	 * to make sure that we read the defaults database from the correct
	 * server before we parse cmd-line args. (xv_parse_cmdline stores the
	 * parsed flags in the defaults database.)
	 */
	if (!xv_has_been_initialized()) {
		if (server_name) {
			server = xv_create(XV_NULL, SERVER,
					ATTR_LIST, attrs_start,
					XV_NAME, server_name,
					NULL);
		}
		else {
			server = xv_create(XV_NULL, SERVER,
					ATTR_LIST, attrs_start,
					NULL);
		}
		if (!server)
			xv_connection_error(server_name);
		notify_exclude_fd =
				ConnectionNumber((Display *) xv_get(server, XV_DISPLAY));
	}

	/* Note: XSetErrorHandler must be called after the server connection
	 * has been established.  XSetErrorHandler() returns the current
	 * X Error handler, which is now the defualt Xlib X Error Handler address.
	 * Note: The error handler is defined by Xlib to be an int function, but
	 * no use is made of the return value.  So, XView's X Error Handler
	 * has been declared to be a void function.
	 */
	xv_xlib_error_proc = XSetErrorHandler(xv_x_error_handler);

	if (parse_result == -1) {
		/* Following routine often, but not always, calls exit(). */
		help_proc(xv_app_name);
	}

	/* Define unmodified ISO Mouseless Keyboard Commands */
	xv_iso_cancel = (char)defaults_get_integer("keyboard.cancel",
								"Keyboard.Cancel", 0x1b);	/* Escape */
	xv_iso_default_action = (char)defaults_get_integer("keyboard.defaultAction",
								"Keyboard.DefaultAction", '\r');	/* Return */
	xv_iso_input_focus_help =
			(char)defaults_get_integer("keyboard.inputFocusHelp",
										"Keyboard.InputFocusHelp", '?');
	xv_iso_next_element = (char)defaults_get_integer("keyboard.nextElement",
								"Keyboard.NextElement", '\t');	/* Tab */
	xv_iso_select = (char)defaults_get_integer("keyboard.select",
								"Keyboard.Select", ' ');	/* Space */

	is_mouseless = (defaults_get_enum("openWindows.keyboardCommands",
					"OpenWindows.KeyboardCommands",
					xv_kbd_cmds_value_pairs) == KBD_CMDS_FULL);

	return (server);
}

Xv_private int xv_translate_iso(Event *ev)
{
	if (is_mouseless && event_action(ev) == xv_iso_cancel) {
		event_set_action(ev, ACTION_CANCEL);
		return TRUE;
	}
	if (is_mouseless && xv_iso_default_action != '\0') {
		if (event_action(ev) == xv_iso_default_action) {
			event_set_action(ev, ACTION_DEFAULT_ACTION);
			return TRUE;
		}
	}
	if (is_mouseless && event_action(ev) == xv_iso_input_focus_help) {
		event_set_action(ev, ACTION_INPUT_FOCUS_HELP);
		return TRUE;
	}
	if (is_mouseless && event_action(ev) == xv_iso_next_element) {
		/* if I leave that "is_mouseless" away, TAB will no longer traverse */
		if (event_shift_is_down(ev))
			event_set_action(ev, ACTION_PREVIOUS_ELEMENT);
		else
			event_set_action(ev, ACTION_NEXT_ELEMENT);
		return TRUE;
	}
	if (is_mouseless && event_action(ev) == xv_iso_select) {
		event_set_action(ev, ACTION_SELECT);
		return TRUE;
	}

	return FALSE;
}

static void init_custom_attrs(void)
{
    xv_add_custom_attrs(WINDOW, 
        /* window_set.c */
		WIN_DESIRED_WIDTH,	"win_desired_width",
    	WIN_DESIRED_HEIGHT,	"win_desired_height",
    	WIN_COLUMNS, 		"win_columns",
    	WIN_ROWS, 		"win_rows",
    	XV_HEIGHT, 		"xv_height",
    	XV_WIDTH, 		"xv_width",
    	XV_X, 			"xv_x",
    	XV_Y,	 		"xv_y",
		NULL);

    xv_add_custom_attrs(CANVAS, 
    	/* cnvs_set.c */
    	CANVAS_WIDTH, 	     	"canvas_width",
    	CANVAS_HEIGHT, 	     	"canvas_height",
    	CANVAS_MIN_PAINT_WIDTH,	"canvas_min_paint_width",
    	CANVAS_MIN_PAINT_HEIGHT,"canvas_min_paint_height",
    	NULL);

    xv_add_custom_attrs(OPENWIN, 
    	/* ow_set.c */
    	WIN_COLUMNS,	"win_columns",
    	WIN_ROWS,	"win_rows",
    	NULL);

	/* on small displays (e.g. with height 600) we can set
	 * *panel_item_y_gap: 7
	 * in .Xdefaults to have command and property windows not too high
	 */

    xv_add_custom_attrs(PANEL, 
    	/* p_set.c */
    	PANEL_ITEM_X_GAP,		"panel_item_x_gap",
    	PANEL_ITEM_Y_GAP,		"panel_item_y_gap",
    	PANEL_EXTRA_PAINT_WIDTH,	"panel_extra_paint_width",
    	PANEL_EXTRA_PAINT_HEIGHT,	"panel_extra_paint_height",
    	NULL);

    xv_add_custom_attrs(PANEL_ITEM, 
    	/* item_set.c */
    	XV_X, 		   "xv_x",
    	PANEL_ITEM_X, 	   "panel_item_x",
    	XV_Y, 		   "xv_y",
    	PANEL_ITEM_Y, 	   "panel_item_y",
    	PANEL_ITEM_X_GAP,  "panel_item_x_gap",
    	PANEL_ITEM_Y_GAP,  "panel_item_y_gap",
    	PANEL_NEXT_COL,    "panel_next_col",
    	PANEL_NEXT_ROW,    "panel_next_row",
    	PANEL_LABEL_X, 	   "panel_label_x",
    	PANEL_LABEL_Y, 	   "panel_label_y",
    	PANEL_VALUE_X, 	   "panel_value_x",
    	PANEL_VALUE_Y, 	   "panel_value_y",
    	PANEL_LABEL_WIDTH, "panel_label_width",
    	NULL);
	
    xv_add_custom_attrs(PANEL_CHOICE, 
    	/* p_choice.c */
    	PANEL_CHOICE_NROWS, "panel_choice_nrows",
    	PANEL_CHOICE_NCOLS, "panel_choice_ncols",
    	NULL);

    xv_add_custom_attrs(PANEL_GAUGE, 
    	/* p_gauge.c */
    	PANEL_MIN_VALUE,	"panel_min_value",
    	PANEL_MAX_VALUE,	"panel_max_value",
    	PANEL_TICKS,		"panel_ticks",
    	PANEL_GAUGE_WIDTH,	"panel_gauge_width",
    	NULL);

    xv_add_custom_attrs(PANEL_LIST, 
    	/* p_list.c */
    	PANEL_LIST_ROW_HEIGHT, 	 "panel_list_row_height",
    	PANEL_LIST_WIDTH, 	 "panel_list_width",
    	PANEL_LIST_DISPLAY_ROWS, "panel_list_display_rows",
    	PANEL_VALUE_DISPLAY_LENGTH,   "panel_value_display_length",
    	PANEL_VALUE_STORED_LENGTH,  "panel_value_stored_length",
    	NULL);

    xv_add_custom_attrs(PANEL_SLIDER, 
    	/* p_slider.c */
    	PANEL_MIN_VALUE, 	      "panel_min_value",
    	PANEL_MAX_VALUE, 	      "panel_max_value",
    	PANEL_TICKS, 		      "panel_ticks",
    	PANEL_SLIDER_WIDTH, 	      "panel_slider_width",
    	PANEL_VALUE_DISPLAY_LENGTH,   "panel_value_display_length",
    	PANEL_JUMP_DELTA,  "panel_jump_delta",
    	NULL);

    xv_add_custom_attrs(PANEL_TEXT, 
    	/* p_txt.c */
    	PANEL_VALUE_STORED_LENGTH,  "panel_value_stored_length",
    	PANEL_VALUE_DISPLAY_LENGTH, "panel_value_display_length",
    	PANEL_VALUE_DISPLAY_WIDTH,   "panel_value_display_width",
    	NULL);

    xv_add_custom_attrs(PANEL_NUMERIC_TEXT, 
    	/* p_num_txt.c */
    	PANEL_MIN_VALUE, "panel_min_value",
    	PANEL_MAX_VALUE, "panel_max_value",
    	PANEL_VALUE_DISPLAY_LENGTH,   "panel_value_display_length",
    	PANEL_VALUE_DISPLAY_WIDTH,   "panel_value_display_width",
    	PANEL_VALUE_STORED_LENGTH,  "panel_value_stored_length",
    	PANEL_JUMP_DELTA,  "panel_jump_delta",
    	NULL);

    xv_add_custom_attrs(PANEL_MULTILINE_TEXT, 
    	/* p_mlinetxt.c */
    	PANEL_VALUE_DISPLAY_LENGTH,   "panel_value_display_length",
    	PANEL_VALUE_DISPLAY_WIDTH,   "panel_value_display_width",
    	PANEL_VALUE_STORED_LENGTH,  "panel_value_stored_length",
    	NULL);

    xv_add_custom_attrs(PANEL_DROP_TARGET, 
    	/* p_drop.c */
    	PANEL_DROP_WIDTH, "panel_drop_width",
    	PANEL_DROP_HEIGHT, "panel_drop_height",
    	NULL);
}

/*ARGSUSED*/
static int xv_handle_xio_errors(Display *display)
{
    /* do nothing, be quiet */
    exit(0);
}

Xv_private void xv_connection_error(char *server_name)
{
    char *error_string;
 
    server_name = (server_name) ? server_name :
                                (char *) defaults_get_string("server.name",
                                                             "Server.Name",
                                                             getenv("DISPLAY"));

    if (server_name) {
        error_string =xv_malloc(strlen(ERROR_MSG) + strlen(server_name) + 2);
        strcpy(error_string, ERROR_MSG);
        strcat(error_string, server_name);
    } else {
        error_string =xv_malloc(strlen(ERROR_MSG) + 4);
        strcpy(error_string, ERROR_MSG);
        strcat(error_string, ":0");
    }
    xv_error(XV_NULL,
                 ERROR_SEVERITY, ERROR_NON_RECOVERABLE,
                 ERROR_STRING, error_string,
                 ERROR_PKG, SERVER,
                 NULL);
    /* NOTREACHED */
    xv_free(error_string);
}

/*
 * xv_base_name - return the base filename sans the path
 */
static char *xv_base_name(char *fullname)
{
	char *base_name;
	char *start;

	/* Find the beginning of the base name */
	start = fullname + strlen(fullname);
	while ((*start != '/') && (start != fullname))
		start--;
	if (*start == '/')
		start++;
	base_name = xv_malloc(strlen(start) + 1);
	(void)strcpy(base_name, start);
	return (base_name);
}
