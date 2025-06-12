#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)server.c 20.157 93/04/28 DRA: $Id: server.c,v 4.32 2025/06/10 17:24:32 dra Exp $";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL_NOTICE 
 *	file for terms of the license.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/param.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <langinfo.h>
#include <xview/win_input.h>
#include <xview/win_struct.h>
#include <xview_private/ntfy.h>
#include <xview_private/ndet.h>
#include <xview/notify.h>
#include <xview/win_notify.h>
#include <xview/defaults.h>
#include <X11/Xlib.h>
#include <X11/Xlibint.h>
#include <xview_private/portable.h>
#include <xview_private/svr_atom.h>
#include <xview_private/svr_impl.h>
#include <xview_private/svr_kmdata.h>
#include <xview_private/draw_impl.h>
#include <xview_private/win_info.h>
#include <xview_private/xv_color.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xresource.h>

#define	LIB_LOCALE	"/lib/locale/"

#define	ISO8859_1	"iso_8859_1"

/*
 * Try to save some text segment by allowing to use XV_MSG with
 * variables, however need special care on xgettext process.  While
 * xgettext processing, those XV_MSG should have a constant instead.
 * This method saves 500 bytes in server_get_locale_from_str()
 * function (in PIC object code).
 */
#ifdef XGETTEXT
#define	XV_MSG_CONST(msg)	XV_MSG(msg)
#define	XV_MSG_VAR(msg)		(msg)
#else
#define	XV_MSG_CONST(msg)	(msg)
#define	XV_MSG_VAR(msg)		XV_MSG(msg)
#endif

#ifdef _XV_DEBUG
Xv_private_data int server_gather_stats;
#endif

static void load_kbd_cmds(Server_info *server, Key_binding *kb_table);
static void server_init_atoms(Xv_Server server_public);
static void destroy_atoms(Server_info *server);
static Notify_value scheduler(int n, Notify_client *clients);
static unsigned int string_to_modmask(Server_info *, char *str);
static Server_atom_type save_atom(Server_info *server, Atom	atom, Server_atom_type type);
static void server_yield_modifiers(Server_info	*server);
static int xv_set_scheduler(void);

Xv_private char	*xv_strtok(char *, char *);

Xv_private void 	 xv_do_enqueued_input(char *, char *quatsch);
Xv_private void	xv_merge_cmdline(XrmDatabase *);
#ifdef OS_HAS_LOCALE
static void server_set_locale(Server_info  *server);
static void server_setlocale_to_c(Ollc_item *ollc);
static void server_warning(char *msg);
static void server_setlocale_to_default(Server_info	*server);
#ifdef OW_I18N
static void server_effect_locale(Server_info *server, char	*character_set);
#ifdef FULL_R5
Xv_private XIMStyle	 xv_determine_im_style();
#define XV_SUPPORTED_STYLE_COUNT 12
#endif /* FULL_R5 */
#endif /* OW_I18N */
#endif /* OS_HAS_LOCALE */

/* extern char	    	*setlocale(); */
static Notify_scheduler_func default_scheduler;
extern XrmDatabase  	 defaults_rdb;
extern char				*xv_app_name;
extern int				_xv_is_multibyte;
Xv_private_data char 	*xv_shell_prompt;

/* global default server parameters */
Xv_private Xv_Screen xv_default_screen;
Xv_private Display *xv_default_display;

/* Global data */
Xv_private_data Defaults_pairs xv_kbd_cmds_value_pairs[4] = {
    { "SunView1", KBD_CMDS_SUNVIEW1 },
    { "Basic", KBD_CMDS_BASIC },
    { "Full", KBD_CMDS_FULL },
    { NULL, KBD_CMDS_SUNVIEW1	/* default */ }
};

typedef struct ollc_const_t {
	const Xv_generic_attr	 attr;
	const char		*inst;
	const char		*class;
	const int		 posix;
	const char		*env;
} Ollc_const_t;
static const Ollc_const_t	Ollc_const[] = {
{ XV_LC_BASIC_LOCALE,
	"basicLocale",	"BasicLocale",	LC_CTYPE,	"LC_CTYPE"},
{ XV_LC_DISPLAY_LANG,
	"displayLang",	"DisplayLang",	LC_MESSAGES,	"LC_MESSAGES"},
{ XV_LC_INPUT_LANG,
	"inputLang",	"InputLang",	-1,		NULL},
{ XV_LC_NUMERIC,
	"numericFormat","NumericFormat",LC_NUMERIC,	"LC_NUMERIC"},
{ XV_LC_TIME_FORMAT,
	"timeFormat",	"TimeFormat",	LC_TIME,	"LC_TIME"},
{ 0,
	NULL,		NULL,		-1,		NULL}};

Xv_private int server_sem_map_index(KeySym ks)
{
	/* we expect KeySyms like
	 * XK_End       = 0x0000ff57
	 * osfXK_Copy   = 0x1004ff02
	 * SunXK_Copy   = 0x1005ff72
	 * XF86XK_Copy  = 0x1008ff57
	 */

	unsigned int big = ks >> 16;
	switch (big) {
		case 0: return SEM_INDEX_BASIC;
		case 0x1004:
			/* return SEM_INDEX_HP;
			 * almost no conflicts:
			 * Hangul (ff31) <--> osfAddMode (1004ff31)
			 * Hangul_Start (ff32) <--> osfPrimaryPaste (1004ff32)
			 * Hangul_End (ff33) <--> osfQuickPaste (1004ff33)
			 */
			return SEM_INDEX_BASIC;
		case 0x1005:
			/* return SEM_INDEX_SUN;
			 * almost no conflicts:
			 * Select (ff60) <--> SunSys_Req (1005ff60)
			 */
			return SEM_INDEX_BASIC;
		case 0x1008: return SEM_INDEX_XF86;
		default: break;
	}
	fprintf(stderr, "%s-%d: cannot handle KeySym %lx\n", __FILE__,__LINE__,ks);
	return SEM_INDEX_BASIC;
}

static void load_kbd_cmds(Server_info *server, Key_binding *kb_table)
{
	int i;
	int j;
	KeySym keysym;
	char *keysym_string;
	char *mapping[MAX_NBR_MAPPINGS];

	/* A key mapping for one keyboard command.
	 * Format: KeysymName[+Modifer...]
	 */
	char *modifier;
	int offset;
	char *value;
	char buffer[100];

	/* Load keyboard commands into keymaps */
	for (i = 0; kb_table[i].action; i++) {
		value = defaults_get_string(kb_table[i].name, kb_table[i].name,
				kb_table[i].value);
		strcpy(buffer, value);
		value = buffer;
		mapping[0] = xv_strtok(value, ",");
		for (j = 1; j < MAX_NBR_MAPPINGS; j++) {
			mapping[j] = xv_strtok(NULL, ",");
			if (mapping[j] == NULL)
				break;
		}

		for (j = 0; j < MAX_NBR_MAPPINGS && mapping[j]; j++) {
			offset = 0;
			keysym_string = xv_strtok(mapping[j], "+");
			if (!keysym_string)
				continue;	/* Error in resource value: ignore */
			keysym = XStringToKeysym(keysym_string);
			if (keysym == 0)
				continue;	/* Error in resource value: ignore */
			do {
				modifier = xv_strtok(NULL, "+");
				if (modifier) {
					if (!strcmp(modifier, "Ctrl"))
						offset += 0x100;
					else if (!strcmp(modifier, "Meta"))
						offset += 0x200;
					else if (!strcmp(modifier, "Alt"))
						offset += 0x400;
					else if (!strcmp(modifier, "Shift"))
						offset += 0x800;
				}
			} while (modifier);
			if ((keysym & KEYBOARD_KYSM_MASK) == KEYBOARD_KYSM) {
				/* Problem: Keysyms look like XK_End = 0xff57
				 * but also XK_XF86Copy = 0x1008ff57
				 * they both land in sem_map[0x57] and
				 * THE LAST ACTION WINS!
				 *
				 * Now, we have up to 4 (2) sem_maps and a function
				 * int server_sem_map_index(KeySym)
				 */

				int idx = server_sem_map_index(keysym);

				if (! server->sem_maps[idx]) {
					/* not yet allocated */
    				server->sem_maps[idx] = (unsigned char *)xv_calloc(0x1600,
											(unsigned)sizeof(unsigned char));
				}
				server->sem_maps[idx] [(keysym & 0xFF) + offset] =
						kb_table[i].action & 0xFF;
				if (offset == 0) {
					if (kb_table[i].action == ACTION_CUT)
						server->cut_keysym = keysym;
					if (kb_table[i].action == ACTION_PASTE)
						server->paste_keysym = keysym;
					if (kb_table[i].action == ACTION_HELP)
						server->help_keysym = keysym;
				}
			}
			else
				server->ascii_map[(keysym & 0xFF) + offset] =
						kb_table[i].action & 0xFF;
		}	/* for each mapping */
	}  /* for each key table entry */
}

#define NIL 0

static void server_build_keymap_table(Server_info *server)
{
	Kbd_cmds_value kbd_cmds_value;
	/*************************************************************************
	 *
	 * Keysym -> Event id (ie_code) translation
	 *
	 *************************************************************************/
	static unsigned int win_keymap[] = {
		NIL, NIL, NIL, NIL, NIL, NIL, NIL, NIL,
		/*
		 * TTY Functions, cleverly chosen to map to ascii, for convenience of
		 * programming, but could have been arbitrary (at the cost of lookup
		 * tables in client code.
		 */

		XK_BackSpace,
		XK_Tab,
		XK_Linefeed,
		XK_Clear,
		NIL,
		XK_Return,

		NIL, NIL,

		/* BUG: On X11/NeWS, Keysym F36 and F37 happen to fall into a couple
			of holes in the win_keymap table.  We will use them for
			now, but this needs to be fixed before MIT decides to put
			real keysyms here.
		*/

		/* see also Ref (dsfvkjwbgref) */
		NIL,   /* Index 16 */
		NIL,
		NIL,

		XK_Pause,
#ifndef XK_Scroll_Lock
			NIL,
#else    
			/* BUG: Only in R4. */
			XK_Scroll_Lock,                 		/* XK_Scroll_Lock */
#endif /* XK_Scroll_Lock */
		NIL, NIL, NIL, NIL, NIL, NIL,

		XK_Escape,                   /* index 27  */

		NIL, NIL, NIL, NIL,

		/* International & multi-key character composition */

		XK_Multi_key,
		XK_Kanji,

		NIL, NIL, NIL,                         NIL, NIL, NIL,
		NIL, NIL, NIL, NIL, NIL, NIL, NIL, NIL, NIL, NIL,
		NIL, NIL, NIL, NIL, NIL, NIL, NIL, NIL, NIL, NIL,
		NIL, NIL, NIL, NIL, NIL, NIL, NIL, NIL, NIL, NIL,
		NIL, NIL, NIL, NIL, NIL, NIL, NIL, NIL, NIL, NIL,

		/* Cursor control & motion */

		XK_Home,
		KEY_RIGHT(10),					/* XK_Left  */
		KEY_RIGHT(8),					/* XK_Up    */
		KEY_RIGHT(12),					/* XK_Right */
		KEY_RIGHT(14),					/* XK_Down  */
		XK_Prior,
		XK_Next,
		XK_End,
		XK_Begin,

		NIL, NIL, NIL, NIL, NIL, NIL, NIL,

		/* Misc Functions */
	 
		XK_Select,
		XK_Print,
		XK_Execute,
		XK_Insert,
		NIL,
		XK_Undo,
		XK_Redo,
		XK_Menu,
		XK_Find,
		XK_Cancel,
		XK_Help,
		SHIFT_BREAK,					/* XK_Break */
		
		NIL, NIL, NIL, NIL, NIL, NIL, NIL, NIL, NIL,
		NIL, NIL, NIL, NIL, NIL, NIL, NIL, NIL, NIL,

		SHIFT_ALTG,					/* XK_script_switch */
		SHIFT_NUMLOCK,					/* XK_Num_Lock      */

		/* Keypad Functions, keypad numbers cleverly chosen to map to ascii */

		XK_KP_Space,

		NIL, NIL, NIL, NIL, NIL, NIL, NIL, NIL,

		XK_KP_Tab,

		NIL, NIL, NIL,

		XK_KP_Enter,

		NIL, NIL, NIL,

		XK_KP_F1,
		XK_KP_F2,
		XK_KP_F3,
		XK_KP_F4,

		NIL, NIL, NIL, NIL, NIL, NIL, NIL, NIL, NIL, NIL,
		NIL, NIL, NIL, NIL, NIL, NIL, NIL, NIL, NIL, NIL,
		NIL,

		XK_KP_Multiply,
		XK_KP_Add,
		XK_KP_Separator,
		XK_KP_Subtract,
		XK_KP_Decimal,
		XK_KP_Divide,
		XK_KP_0,
		XK_KP_1,
		XK_KP_2,
		XK_KP_3,
		XK_KP_4,
		XK_KP_5,
		XK_KP_6,
		XK_KP_7,
		XK_KP_8,
		XK_KP_9,

		NIL, NIL, NIL,

		XK_KP_Equal,      /* index 189 */

	  /*
	   * Auxilliary Functions; note the duplicate definitions for left and right
	   * function keys;  Sun keyboards and a few other manufactures have such
	   * function key groups on the left and/or right sides of the keyboard.
	   * We've not found a keyboard with more than 35 function keys total.
	   */
		KEY_TOP(1),					/* XK_F1  */
		KEY_TOP(2),		        		/* XK_F2  */
		KEY_TOP(3),					/* XK_F3  */
		KEY_TOP(4),					/* XK_F4  */
		KEY_TOP(5),					/* XK_F5  */
		KEY_TOP(6),					/* XK_F6  */
		KEY_TOP(7),					/* XK_F7  */
		KEY_TOP(8),					/* XK_F8  */
		KEY_TOP(9),					/* XK_F9  */
		KEY_TOP(10),					/* XK_F10 */
		KEY_LEFT(1),					/* XK_L1  */   /* index 200  */
		KEY_LEFT(2),					/* XK_L2  */
		KEY_LEFT(3),					/* XK_L3  */
		KEY_LEFT(4),					/* XK_L4  */
		KEY_LEFT(5),					/* XK_L5  */
		KEY_LEFT(6),					/* XK_L6  */
		KEY_LEFT(7),					/* XK_L7  */
		KEY_LEFT(8),					/* XK_L8  */
		KEY_LEFT(9),					/* XK_L9  */
		KEY_LEFT(10),					/* XK_L10 */
		KEY_RIGHT(1),					/* XK_R1  */
		KEY_RIGHT(2),					/* XK_R2  */
		KEY_RIGHT(3),					/* XK_R3  */
		KEY_RIGHT(4),					/* XK_R4  */
		KEY_RIGHT(5),					/* XK_R5  */
		KEY_RIGHT(6),					/* XK_R6  */
		KEY_RIGHT(7),					/* XK_R7  */
		KEY_RIGHT(8),					/* XK_R8  */
		KEY_RIGHT(9),					/* XK_R9  */
		KEY_RIGHT(10),					/* XK_R10 */
		KEY_RIGHT(11),					/* XK_R11 */
		KEY_RIGHT(12),					/* XK_R12 */
		KEY_RIGHT(13),					/* XK_R13 */
		KEY_RIGHT(14),					/* XK_R14 */
		KEY_RIGHT(15),					/* XK_R15 */

		/* Modifiers */
	 
		SHIFT_LEFT,					/* XK_Shift_L    */
		SHIFT_RIGHT,					/* XK_Shift_R    */
		SHIFT_CTRL,					/* XK_Control_L  */
		SHIFT_CTRL,					/* XK_Control_R  */
		SHIFT_CAPSLOCK,					/* XK_Caps_Lock  */
		SHIFT_LOCK,					/* XK_Shift_Lock */
		SHIFT_META,					/* XK_Meta_L     */
		SHIFT_META,					/* XK_Meta_R     */
		SHIFT_ALT,					/* XK_Alt_L	 */
		SHIFT_ALTG,					/* XK_Alt_R	 */
		XK_Super_L,
		XK_Super_R,
		XK_Hyper_L,
		XK_Hyper_R,

		NIL, NIL, NIL, NIL, NIL, NIL, NIL, NIL, NIL, NIL,
		NIL, NIL, NIL, NIL, NIL, NIL,

		XK_Delete
	};

	/* Ref (dsfvkjwbgref)
	 * The question 'WHERE on the keyboard is XK_F11=XK_L1' is a question
	 * about the keyboard on the X11-server side....
	 * but we are here on the client side - and today, we talk about
	 * Linux and the 'standard PC keyboard' - and this has
	 * F1 - F12 on the TOP !
	 */
	int f0_index = 0, i, max_top;

	for (i = 0; i < 255; i++) {
		if (win_keymap[i] == XK_KP_Equal) {	/* the entry before KEY_TOP(1) */
			f0_index = i;
			break;
		}
	}
	max_top = defaults_get_integer("OpenWindows.NumberOfTopFkeys",
			"OpenWindows.NumberOfTopFkeys", 12);
	for (i = 1; i <= max_top; i++) {
		win_keymap[f0_index + i] = KEY_TOP(i);
	}

	server->xv_map = win_keymap;

	server->ascii_map =
			(unsigned char *)xv_calloc(0x800, (unsigned)sizeof(unsigned char));

	server_yield_modifiers(server);

	/* Load requested keyboard commands into keymaps */
	kbd_cmds_value = defaults_get_enum("openWindows.keyboardCommands",
			"OpenWindows.KeyboardCommands", xv_kbd_cmds_value_pairs);

	load_kbd_cmds(server, sunview1_kbd_cmds);

	if (kbd_cmds_value >= KBD_CMDS_BASIC) load_kbd_cmds(server, basic_kbd_cmds);
	if (kbd_cmds_value == KBD_CMDS_FULL) load_kbd_cmds(server, full_kbd_cmds);

	/* ACC_XVIEW */
	/* Determine existence of accelerators */
	server->acceleration = defaults_get_boolean("openWindows.menuAccelerators",
										"OpenWindows.MenuAccelerators", FALSE);
	/* ACC_XVIEW */
}

static void server_yield_modifiers(Server_info	*server)
{
	char modifier_string[128], *returned_string, *modifier;

	returned_string = defaults_get_string("ttysw.yieldModifiers",
			"Ttysw.YieldModifiers", (char *)NULL);

	server->pass_thru_modifiers = 0;

	if (!returned_string)
		return;

	strcpy(modifier_string, returned_string);

	modifier = xv_strtok(modifier_string, ", ");
	do {
		if (modifier) {
			if (!strcmp(modifier, "Meta"))
				server->pass_thru_modifiers += 0x200;
			else if (!strcmp(modifier, "Alt"))
				server->pass_thru_modifiers += 0x800;
		}
	} while ((modifier = xv_strtok(NULL, ", ")));
}

static int svr_parse_display(char *display_name)
{
	 /*
	 * The following code stolen form XConnectDisplay to parse the
	 * string and return the default screen number or 0.
	 */
	 char displaybuf[256];       /* Display string buffer */
	 register char *display_ptr; /* Display string buffer pointer */
	 register char *numbuf_ptr;  /* Server number buffer pointer */
	 char *screen_ptr;       /* Pointer for locating screen num */
	 char numberbuf[16];
	 char *dot_ptr = NULL;       /* Pointer to . before screen num */
	 /*
	 * Find the ':' seperator and extract the hostname and the
	 * display number.
	 * NOTE - if DECnet is to be used, the display name is formatted
	 * as "host::number"
	 */
	 strncpy(displaybuf, display_name, sizeof(displaybuf)-1);
	 if ((display_ptr = XV_INDEX(displaybuf,':')) == NULL) return (-1);
	 *(display_ptr++) = '\0';
 
	 /* displaybuf now contains only a null-terminated host name, and
	 * display_ptr points to the display number.
	 * If the display number is missing there is an error. */
		  
	if (*display_ptr == '\0') return(-1);

	/*
	* Build a string of the form <display-number>.<screen-number> in
	* numberbuf, using ".0" as the default.
	*/
	screen_ptr = display_ptr;       /* points to #.#.propname */
	numbuf_ptr = numberbuf;         /* beginning of buffer */
	while (*screen_ptr != '\0') {
		if (*screen_ptr == '.') {       /* either screen or prop */
			if (dot_ptr) {          /* then found prop_name */
				screen_ptr++;
				break;
			}
			dot_ptr = numbuf_ptr;       /* found screen_num */
			*(screen_ptr++) = '\0';
			*(numbuf_ptr++) = '.';
		} else {
			*(numbuf_ptr++) = *(screen_ptr++);
		}
	}
	
	/*
	 * If the spec doesn't include a screen number, add ".0" (or "0" if
         * only "." is present.)
	 */
	if (dot_ptr == NULL) {          /* no screen num or prop */
		dot_ptr = numbuf_ptr;
		*(numbuf_ptr++) = '.';
		*(numbuf_ptr++) = '0';
	} else {
		if (*(numbuf_ptr - 1) == '.')
			*(numbuf_ptr++) = '0';
	}
	*numbuf_ptr = '\0';

	/*
	* Return the screen number
	*/
	return(atoi(dot_ptr + 1));
}

static Notify_value wrap_xv_input_pending(Notify_client cl, int fd)
{
	xv_input_pending((Display *)cl, fd);
	return NOTIFY_DONE;
}

static Defaults_pairs shiftmasks[] = {
	{ "Shift", SHIFTMASK },
	{ "Ctrl", CTRLMASK },
	{ "Meta", META_SHIFT_MASK },
	{ "Alt", ALTMASK },
	{ "Shift+Ctrl", SHIFTMASK | CTRLMASK },
	{ "Ctrl+Shift", SHIFTMASK | CTRLMASK },
	{ "Shift+Meta", SHIFTMASK | META_SHIFT_MASK },
	{ "Meta+Shift", SHIFTMASK | META_SHIFT_MASK },
	{ "Shift+Alt", SHIFTMASK | ALTMASK },
	{ "Alt+Shift", SHIFTMASK | ALTMASK },
	{ "Ctrl+Meta", CTRLMASK | META_SHIFT_MASK },
	{ "Meta+Ctrl", CTRLMASK | META_SHIFT_MASK },
	{ "Ctrl+Alt", CTRLMASK | ALTMASK },
	{ "Alt+Ctrl", CTRLMASK | ALTMASK },
	{ "Meta+Alt", META_SHIFT_MASK | ALTMASK },
	{ "Alt+Meta", META_SHIFT_MASK | ALTMASK },
	{ NULL, 0 }
};

__attribute__((constructor)) static void xview_ctor(void)
{
	char *env = getenv("XVIEW_TELL_XINITTHREADS");

	if (env && *env) {
		fprintf(stderr, "xview_ctor called in %s\n", xv_app_name);
	}
}

static int server_init(Xv_opaque parent, Xv_server server_public,
								Attr_avlist avlist, int *unused)
{
	Server_info *server = (Server_info *) NULL;
	Xv_server_struct *server_object;
	Attr_avlist attrs;
	char *home,	/* pathname to home directory */
	   *server_name = NULL, filename[MAXPATHLEN + 300], *xv_message_dir;
	unsigned char pmap[256];	/* pointer mapping list */
	int default_screen_num;
	Server_atom_list *atom_list;
	XrmDatabase new_db;
	int first_server = FALSE;
	int mopc, fev, fer;

#ifdef OW_I18N
	Bool need_im;
	char *character_set;

#ifdef FULL_R5
	char *value;
#endif
#endif
	extern int _xv_use_locale;
	char **argv = NULL;

	char *xdefaults;
	char *env = getenv("XVIEW_TELL_XINITTHREADS");

	if (env && *env) {
		fprintf(stderr, "server_init called in %s\n", xv_app_name);
	}
	for (attrs = avlist; *attrs; attrs = attr_next(attrs)) {
		switch (attrs[0]) {
			case XV_NAME:
				server_name = (char *)attrs[1];
				*attrs = ATTR_NOP(*attrs);
				break;
			case XV_INIT_ARGC_PTR_ARGV:
				/* unused: argc = *(int *)attrs[1]; */
				argv = (char **)attrs[2];
				ATTR_CONSUME(*attrs);
				break;
			default:
				break;
		}
	}
	if (!server_name)
		server_name = (char *)defaults_get_string("server.name",
				"Server.Name", getenv("DISPLAY"));

	/* Allocate private data and set up forward/backward links. */
	server = (Server_info *) xv_alloc(Server_info);
	server->public_self = server_public;
	server_object = (Xv_server_struct *) server_public;
	server_object->private_data = (Xv_opaque) server;
	server->ui_reg_proc = server_note_register_ui;

	server->display_name = xv_strsave(server_name ? server_name : ":0");

	server_xvwp_init(server, argv);

	if (!(server->xdisplay = (Display *) server_init_x(server->display_name))) {
		goto Error_Return;
	}

	if (XQueryExtension(server->xdisplay, "SHAPE", &mopc, &fev, &fer)) {
		server->shape_available = TRUE;
	}

	if (notify_set_input_func((Notify_client) server->xdisplay,
					wrap_xv_input_pending, XConnectionNumber(server->xdisplay))
			== NOTIFY_IO_FUNC_NULL) {
		notify_perror("server_init");
		goto Error_Return;
	}

	/* Screen creation requires default server to be set. */
	if (!xv_default_server) {
		xv_default_server = server_public;
		xv_default_display = (Display *) server->xdisplay;
		server->svr_dpy_context = XUniqueContext();
		first_server = TRUE;
	}

	/* The following code is used to associate the server object to a display
	 * structure.  In the win code we can get events that do not have a
	 * Window associated with them, only a dpy struct.  The below context
	 * will allow us to get a server object from the display struct.
	 */
	server->svr_dpy_context = (XContext) xv_get(xv_default_server,
			SERVER_DISPLAY_CONTEXT);

	if (XSaveContext(server->xdisplay, (Window) server->xdisplay,
					server->svr_dpy_context,
					(caddr_t) server_public) == XCNOMEM) {
		server_warning(XV_MSG
				("Not enough memory to save context for new server"));
		goto Error_Return;
	}

	/*
	 * Now that a server connection has been established, initialize the
	 * defaults database. Note - This assumes that server_init will be called
	 * only once per X11 server.
	 */
	defaults_init_db();	/* init Resource Manager */
	/*
	 *  BUG ALERT(isa) - Sundae buyback
	 *  The following code replaces:
	 *        defaults_load_db(filename);
	 *        defaults_load_db((char *) NULL);
	 *
	 *  This will set defaults_rdb to always be the merge of
	 *  .Xdefaults and the latest server xv_creat'd
	 *
	 *  Create a database from .Xdefaults and from the server property
	 *  and stash its XID in the server. This used to be done in the
	 *  Xv_database object, which has been removed pending design review
	 */

#if XlibSpecificationRelease >= 5
	xdefaults = XResourceManagerString((Display *) server->xdisplay);
#else
	xdefaults = ((Display *) server->xdisplay)->xdefaults;
#endif

	/* See if defaults have been loaded on server */
	if (xdefaults) {
		server->db = XrmGetStringDatabase(xdefaults);
	}
	else {
		/* Get the resources from the users .Xdefaults file */
		if ((home = getenv("HOME")))
			(void)strcpy(filename, home);
		else
			filename[0] = '\0';
		(void)strcat(filename, "/.Xdefaults");
		server->db = XrmGetFileDatabase(filename);
	}

	/*
	 * Check if this is the first server being created, and if the merged
	 * db already exists.
	 * If yes, then merge the newly created server->db into the existing db,
	 * and make server->db point to that
	 */
	if (first_server && defaults_rdb) {
		XrmMergeDatabases(server->db, &defaults_rdb);
		server->db = defaults_rdb;
	}

	/*
	 * Merge cmdline options into database
	 * Note:
	 * For the first server object created, this is actually
	 * done twice. Once in xv_parse_cmdline() (in xv_init) and
	 * once here.
	 *
	 * xv_merge_cmdline() has to be called in xv_parse_cmdline()
	 * because it is a public function, and whoever calls it
	 * expects the cmdline options to be merged into the
	 * database.
	 *
	 * xv_merge_cmdline() has to be called here to make sure cmdline
	 * options are merged into the server resource database.
	 * (we cannot depend on xv_parse_cmdline() being called before
	 * every server creation)
	 */
	xv_merge_cmdline(&server->db);

	/*
	 * Point defaults_rdb to db of most current server created
	 */
	defaults_rdb = server->db;

	server->localedir = NULL;

#ifdef OW_I18N

#ifdef FULL_R5
	server->supported_im_styles = NULL;
	server->determined_im_style = NULL;

	/* 
	 * Command line options for preedit and status style have 
	 * precedence over X resource settings
	 */

	value = defaults_get_string("openWindows.imPreeditStyle.cmdline",
			"OpenWindows.ImPreeditStyle.cmdline", NULL);
	server->preedit_style = (value) ? strdup(value) : NULL;
	value = defaults_get_string("openWindows.imStatusStyle.cmdline",
			"OpenWindows.ImStatusStyle.cmdline", NULL);
	server->status_style = (value) ? strdup(value) : NULL;
#endif /* FULL_R5 */
#endif /* OW_I18N */

#ifdef OS_HAS_LOCALE
	if (_xv_use_locale) {

#ifdef OW_I18N
#endif /* OW_I18N */

		/*
		 *  First look for the attributes to set locale.
		 */

		for (attrs = avlist; *attrs; attrs = attr_next(attrs)) {
			switch (attrs[0]) {
				case XV_LOCALE_DIR:
					if (attrs[1]) {
						if (server->localedir) {
							xv_free(server->localedir);
						}
						server->localedir = strdup((char *)attrs[1]);
					}
					break;

#ifdef OW_I18N

#ifdef FULL_R5
				case XV_IM_PREEDIT_STYLE:
					if (attrs[1]) {
						if (server->preedit_style) {
							xv_free(server->preedit_style);
						}
						server->preedit_style = strdup((char *)attrs[1]);
					}
					break;

				case XV_IM_STATUS_STYLE:
					if (attrs[1]) {
						if (server->status_style) {
							xv_free(server->status_style);
						}
						server->status_style = strdup((char *)attrs[1]);
					}
					break;
#endif /* FULL_R5 */
#endif /* OW_I18N */

				default:{
						int i;
						const Ollc_const_t *oc;

						/*
						 * Supports locale attributes (such as XV_LC_BASIC_LOCLAE).
						 */
						for (i = 0, oc = Ollc_const; oc->attr != 0; oc++, i++) {
							if (oc->attr == attrs[0]) {
								if (attrs[1])
									server->ollc[i].locale =
											strdup((char *)attrs[1]);
								else
									server->ollc[i].locale = NULL;
								server->ollc[i].from = OLLC_FROM_ATTR;
								break;
							}
						}
						break;
					}
			}
		}

		/*
		 * Now sets all locale categories.
		 */
		server_set_locale(server);


		if ((home = getenv("OPENWINHOME"))) {
			xv_message_dir = xv_malloc(strlen(home) + strlen(LIB_LOCALE) + 1);
			strcpy(xv_message_dir, home);
			strcat(xv_message_dir, LIB_LOCALE);
			xv_bindtextdomain(xv_domain, (unsigned char *)xv_message_dir);
			if (!server->localedir) {
				server->localedir = xv_message_dir;
			}
			else {
				xv_free(xv_message_dir);
			}
		}

		if (server->localedir && xv_app_name) {
			char pathname[MAXPATHLEN];
			DIR *dirp;

			xv_bindtextdomain("", (unsigned char *)server->localedir);
			(void)sprintf(pathname, "%s/%s/app-defaults/%s",
					server->localedir,
					server->ollc[OLLC_BASICLOCALE].locale, xv_app_name);
			strcpy(filename, pathname);
			strcat(filename, ".db");

			if ((new_db = XrmGetFileDatabase(filename))) {
				XrmMergeDatabases(server->db, &new_db);
				server->db = new_db;
				defaults_rdb = server->db;
			}

			/*
			 * If the directory XV_LOCALE_DIR/<basic locale>/app-defaults/xv_app_name
			 * exists, read all the files in it with ".db" suffix
			 */
			if ((dirp = opendir(pathname))) {
				struct dirent *dp;

				/*
				 * Read all files in directory
				 */
				for (dp = readdir(dirp); dp != NULL; dp = readdir(dirp)) {
					struct stat statbuf;
					char *dot;

					if (! *dp->d_name) {
						continue;
					}

					/*
					 * Ignore ".", ".."
					 */
					if ((dp->d_name[0] == '.') &&
							(((dp->d_name[1] == '.') && (dp->d_name[2] == '\0'))
									|| (dp->d_name[1] == '\0'))) {
						continue;
					}

					/*
					 * Read in only files that have .db suffix
					 */
					dot = XV_RINDEX(dp->d_name, '.');
					if (!dot || strcmp(dot, ".db")) {
						continue;
					}

					/*
					 * construct filename
					 */
					sprintf(filename, "%s/%s", pathname, dp->d_name);

					if (!stat(filename, &statbuf)) {
						/*
						 * Read only if ordinary file
						 */
						if ((statbuf.st_mode & S_IFMT) == S_IFREG) {
							if ((new_db = XrmGetFileDatabase(filename))) {
								XrmMergeDatabases(server->db, &new_db);
								server->db = new_db;
								defaults_rdb = server->db;
							}
						}
					}
				}

				closedir(dirp);
			}
		}

#ifdef OW_I18N
		/*
		 * Now that we know the locale, get the local specific
		 * resource files, merge with server->db
		 */
		if ((home = getenv("OPENWINHOME")) != NULL) {
			(void)sprintf(filename, "%s/%s/%s/xview/defaults",
					home, LIB_LOCALE, server->ollc[OLLC_BASICLOCALE].locale);
			if (new_db = XrmGetFileDatabase(filename)) {
				/*
				 * Precedence order of this new_db is lowest!
				 */
				XrmMergeDatabases(server->db, &new_db);
				defaults_rdb = server->db = new_db;
			}
		}


		/*
		 * Get locale characteristic informations from Xrm.
		 */
		defaults_set_locale(server->ollc[OLLC_BASICLOCALE].locale, 
							(Xv_generic_attr)0);
		need_im = defaults_get_boolean("xview.needIm", "Xview.NeedIm", False);
		character_set = defaults_get_string("xview.characterSet",
				"Xview.CharacterSet", ISO8859_1);

#ifdef FULL_R5
		if (server->preedit_style == NULL)
			server->preedit_style =
					strdup(defaults_get_string("openWindows.imPreeditStyle",
							"OpenWindows.ImPreeditStyle", "onTheSpot"));
		if (server->status_style == NULL)
			server->status_style =
					strdup(defaults_get_string("openWindows.imStatusStyle",
							"OpenWindows.ImStatusStyle", "clientDisplays"));
#endif

		defaults_set_locale(NULL, NULL);


		/*
		 * Taking effect the locale setting to the system.
		 */
		server_effect_locale(server, character_set);
#endif /* OW_I18N */

	}
	else {	/* if (_xv_use_locale) */
		server_setlocale_to_default(server);

#ifdef OW_I18N

#ifdef FULL_R5
		server->supported_im_styles = NULL;
		server->preedit_style = strdup("onTheSpot");
		server->status_style = strdup("clientDisplays");
		server->determined_im_style = XIMPreeditCallbacks | XIMStatusCallbacks;
#endif /* FULL_R5 */
#endif /* OW_I18N */
	}
#endif /* OS_HAS_LOCALE */

	/*
	 * End of Sundae buyback code replacement for 
	 *    defaults_load_db(filename);
	 *    defaults_load_db((char *) NULL);
	 */

	/* Used by atom mgr */
	XAllocIDs(server->xdisplay, server->atom_mgr, DATA+1);

	/* Key for XV_KEY_DATA.  Used in local dnd ops. */
	server->dnd_ack_key = xv_unique_key();

	/* Key for XV_KEY_DATA.  Used for storing the atom list struct. */
	server->atom_list_head_key = xv_unique_key();
	server->atom_list_tail_key = xv_unique_key();
	server->atom_list_number = 0;

	/* We allocate the first block of atoms, others may be allocated as
	 * the need arises.
	 */
	atom_list = xv_alloc(Server_atom_list);

	XV_SL_INIT(atom_list);

	/* Store away the block of atom storage.  This will be used by the
	 * atom manager.
	 */
	xv_set(SERVER_PUBLIC(server), XV_KEY_DATA, server->atom_list_head_key,
			atom_list, NULL);
	xv_set(SERVER_PUBLIC(server), XV_KEY_DATA, server->atom_list_tail_key,
			atom_list, NULL);

	server_initialize_atoms(server);
	server_init_atoms(server_public);

	server->idproclist = NULL;
	server->xidlist = NULL;

	default_screen_num = svr_parse_display(server->display_name);

	server->screens[default_screen_num] = xv_create(server_public, SCREEN,
			SCREEN_NUMBER, default_screen_num, NULL);

	if (!server->screens[default_screen_num]) {
		goto Error_Return;
	}

	/* Create keycode maps */
	(void)server_build_keymap_table(server);

	if (xv_default_server != server_public) {
		(void)XV_SL_ADD_AFTER(SERVER_PRIVATE(xv_default_server),
				SERVER_PRIVATE(xv_default_server), server);
	}
	else {
		XV_SL_INIT(server);
		xv_default_screen = server->screens[default_screen_num];
		(void)xv_set_scheduler();
	}

	server_refresh_modifiers(SERVER_PUBLIC(server), TRUE);

	server->shiftmask_duplicate = defaults_get_enum(
								"openWindows.duplicateKey",
			  					"OpenWindows.DuplicateKey", shiftmasks);
	if (server->shiftmask_duplicate == 0)
		server->shiftmask_duplicate = CTRLMASK;

	server->shiftmask_constrain = defaults_get_enum(
								"openWindows.modifier.constrain", 
			  					"OpenWindows.Modifier.Constrain", shiftmasks);
	if (server->shiftmask_constrain == 0)
		server->shiftmask_constrain = CTRLMASK;

	server->shiftmask_pan = defaults_get_enum(
								"openWindows.panKey",
			  					"OpenWindows.PanKey", shiftmasks);
	if (server->shiftmask_pan == 0)
		server->shiftmask_pan = META_SHIFT_MASK;

	server->shiftmask_set_default = defaults_get_enum(
								"openWindows.modifier.setDefault",
			  					"OpenWindows.Modifier.SetDefault", shiftmasks);
	if (server->shiftmask_set_default == 0)
		server->shiftmask_set_default = CTRLMASK;

	server->shiftmask_primary_paste = defaults_get_enum(
								"openWindows.modifier.primaryPasteKey",
			  					"OpenWindows.Modifier.PrimaryPasteKey",
								shiftmasks);
	if (server->shiftmask_primary_paste == 0)
		server->shiftmask_primary_paste = CTRLMASK;


	server->chording_timeout =
			defaults_get_integer("OpenWindows.MouseChordTimeout",
			"OpenWindows.MouseChordTimeout", 100);
	server->chord_menu = defaults_get_boolean("OpenWindows.MouseChordMenu",
			"OpenWindows.MouseChordMenu", FALSE);


	/* Be prepared to handle a mouse with only one or two physical buttons */
	server->nbuttons = XGetPointerMapping(server->xdisplay, pmap, 256);
	if (server->nbuttons < 3)
		server->but_two_mod =
				string_to_modmask(server, 
							defaults_get_string("mouse.modifier.button2",
										"Mouse.Modifier.Button2", "Shift"));

	if (server->nbuttons < 2)
		server->but_three_mod =
				string_to_modmask(server,
							defaults_get_string("mouse.modifier.button3",
										"Mouse.Modifier.Button3", "Ctrl"));
	server->composestatus = (XComposeStatus *) xv_alloc(XComposeStatus);
	server->composestatus->compose_ptr = (char *)NULL;
	server->composestatus->chars_matched = 0;

#ifdef OW_I18N
	/*
	 * Make sure, everything is valid, and then fire up the xim.
	 */
	if (need_im == True) {
		/*
		 * first release only one input method supported, hence this
		 * check; when we support multiple locale, each with an input
		 * method, we should save the LC_CTYPE locale, set it what is
		 * specified by inputlang, then re-set the saved LC_CTYPE
		 * locale
		 */
		if (strcmp(server->ollc[OLLC_INPUTLANG].locale, setlocale(LC_CTYPE,
								NULL)) != 0) {
			server_warning(XV_MSG("Inputlang is different from basiclocale"));
		}
		else {
			/*
			 * Make connection with IM server.
			 */
			server->xim =
					(XIM) XOpenIM(server->xdisplay, server->db,
					"openwindows", "OpenWindows");

#ifdef FULL_R5
			/* 
			 * Query IM styles available from the IM connection 
			 */
			if (server->xim) {

				XIMStyles *imserver_styles = NULL;

				if (!XGetIMValues(server->xim, XNQueryInputStyle,
								&imserver_styles, NULL)) {

					/* Determine supported styles based on intersection
					 * of what is supported by im-server and toolkit.
					 */
					if (imserver_styles) {

						XIMStyle toolkit_style[XV_SUPPORTED_STYLE_COUNT];
						short i, j, k;

						/*  Make a list of toolkit supported styles. */
						toolkit_style[0] =
								(XIMPreeditCallbacks | XIMStatusCallbacks);
						toolkit_style[1] =
								(XIMPreeditCallbacks | XIMStatusArea);
						toolkit_style[2] =
								(XIMPreeditCallbacks | XIMStatusNothing);
						toolkit_style[3] =
								(XIMPreeditCallbacks | XIMStatusNone);
						toolkit_style[4] =
								(XIMPreeditPosition | XIMStatusCallbacks);
						toolkit_style[5] = (XIMPreeditPosition | XIMStatusArea);
						toolkit_style[6] =
								(XIMPreeditPosition | XIMStatusNothing);
						toolkit_style[7] = (XIMPreeditPosition | XIMStatusNone);
						toolkit_style[8] =
								(XIMPreeditNothing | XIMStatusCallbacks);
						toolkit_style[9] = (XIMPreeditNothing | XIMStatusArea);
						toolkit_style[10] =
								(XIMPreeditNothing | XIMStatusNothing);
						toolkit_style[11] = (XIMPreeditNothing | XIMStatusNone);

						server->supported_im_styles =
								(XIMStyles *) xv_alloc(XIMStyles);
						server->supported_im_styles->supported_styles =
								xv_calloc(XV_SUPPORTED_STYLE_COUNT,
								sizeof(XIMStyle));

						/* Find the matching style */
						for (i = 0, k = 0;
								i < (int)imserver_styles->count_styles; i++)
							for (j = 0; j < XV_SUPPORTED_STYLE_COUNT; j++) {
								if (imserver_styles->supported_styles[i] ==
										toolkit_style[j])
									server->supported_im_styles->
											supported_styles[k++] =
											toolkit_style[j];
							}
						server->supported_im_styles->count_styles = k;

						/* Determine the input style based on:
						 *  Supported styles &  Requested styles:
						 *        Locale specific resources
						 *        User specified resources
						 *        User specified commandline options
						 *        Application specified attributes
						 */
						server->determined_im_style =
								xv_determine_im_style(server->xim,
								server->supported_im_styles,
								server->preedit_style, server->status_style);
					}
					else {
						/*
						 * IM server does not support any styles!
						 */
						server->supported_im_styles = NULL;
					}
					XFree(imserver_styles);
				}
			}
#endif /* FULL_R5 */
		}
	}
#endif /* OW_I18N */

	return XV_OK;

  Error_Return:
	if (server) {
		if (xv_default_server == server_public) {
			xv_default_server = (Xv_Server) NULL;
		}
		xv_free((char *)server);
	}
	return XV_ERROR;
}


#ifdef OS_HAS_LOCALE
/*
 * server_get_locale_from_str: Get the translated version of the
 * Ollc_from.  Table driven will not work with xgettext(1) command,
 * therefor have to be function instead.
 */
static char *server_get_locale_from_str(Ollc_from	from)
{
    char	*msg;

    switch (from) {
	/*
	 * STRING_EXTRACTION - Following five (include "Unknown")
	 * messages are short description of the how or where locale
	 * has been sets.
	 */
	case OLLC_FROM_ATTR:
	    msg = XV_MSG_CONST("application (attributes)");
	    break;

	case OLLC_FROM_CMDLINE:
	case OLLC_FROM_RESOURCE:
	    msg = XV_MSG_CONST("command line option or X resources");
	    break;

	case OLLC_FROM_POSIX:
	    msg = XV_MSG_CONST("environment variable(s)");
	    break;

	case OLLC_FROM_C:
	    msg = XV_MSG_CONST("system default (C)");
	    break;

	default:
	    msg = XV_MSG_CONST("Unknown");
	    break;
    }
    return XV_MSG_VAR(msg);
}

static void server_set_locale(Server_info  *server)
{
	int i;
	char *locale;
	Ollc_item *oi;
	char *type;
	XrmValue xrm_value;
	char inst[50], class[50];

	/*
	 * Set all locale categories that exist in the environment in a
	 * POSIX compliant fashion prior to query it.
	 */
	(void)setlocale(LC_ALL, "");

	/*
	 * Traverse all the categories and fillin the data according to
	 * the priority, if data has not being set yet.
	 */
	for (i = 0, oi = server->ollc; i < OLLC_MAX; i++, oi++) {
		if (oi->locale != NULL)
			continue;

		/*
		 * Try Resources (defaults) firsts.
		 */
		xrm_value.size = 0;
		xrm_value.addr = NULL;

		(void)sprintf(inst, "openWindows.%s", Ollc_const[i].inst);
		(void)sprintf(class, "OpenWindows.%s", Ollc_const[i].class);
		if (XrmGetResource(server->db, inst, class, &type, &xrm_value)) {
			oi->locale = strdup((char *)xrm_value.addr);
			oi->from = OLLC_FROM_RESOURCE;

			/* das war, als ich auf das ECHTE dgettext, und das ECHTE msgfmt
			 * umgestiegen bin
			 */
			if (Ollc_const[i].posix >= 0) {
				char *result = setlocale(Ollc_const[i].posix, oi->locale);

				if (!result) {
					fprintf(stderr, "unsuccessfully tried to set %s to %s\n", 
									Ollc_const[i].env , oi->locale);
				}
			}
			continue;
		}

		/*
		 * For 3.1 backwards compatibility of *numeric resource, need to
		 * check if the old resource is being used.
		 */
		if (i == OLLC_NUMERIC && defaults_exists("numeric", "Numeric")) {
			char *old_resource_value;

			old_resource_value =
					strdup(defaults_get_string("numeric", "Numeric", NULL));
			if (old_resource_value) {
				server->ollc[OLLC_NUMERIC].locale = old_resource_value;
				server->ollc[OLLC_NUMERIC].from = OLLC_FROM_RESOURCE;
				continue;
			}
		}

		/*
		 * fallback to setlocale(3).
		 */
		if (Ollc_const[i].posix >= 0
				&& (locale = setlocale(Ollc_const[i].posix, NULL)) != NULL) {
			oi->locale = strdup(locale);
			oi->from = OLLC_FROM_POSIX;
			continue;
		}

		if (i == OLLC_BASICLOCALE) {
			/*
			 * This is actually almost like internal error!.
			 */
			server_warning(XV_MSG
					("Could not obtain the Basic Locale settings! - Defaulting to \"C\""));
			oi->locale = strdup("C");
			oi->from = OLLC_FROM_C;
			continue;
		}

		/*
		 * Final fallback, Basic Locale
		 */
		oi->locale = strdup(server->ollc[OLLC_BASICLOCALE].locale);
		oi->from = server->ollc[OLLC_BASICLOCALE].from;
	}

	/* the plan is to replace "#ifdef OW_I18N" by "if (_xv_is_multibyte)" */
    if (XSupportsLocale()) {
		_xv_is_multibyte = (strncmp(nl_langinfo(CODESET), "UTF", 3L) == 0);
	}
	else {
		for (i = 0, oi = server->ollc; i < OLLC_MAX; i++, oi++) {
			if (oi->locale != NULL)
				continue;

			if (Ollc_const[i].posix >= 0) {
				setlocale(Ollc_const[i].posix, "C");
			}
		}
		_xv_is_multibyte = FALSE;
	}

	if (_xv_is_multibyte) {
		fprintf(stderr, "in server_set_locale:\n");
		for (i = 0, oi = server->ollc; i < OLLC_MAX; i++, oi++) {
			fprintf(stderr, "\t%d. locale = '%s', set from %s\n",
					i, oi->locale, server_get_locale_from_str(oi->from));
		}
	}
}


#ifdef OW_I18N
/*
 * server_get_locale_name_str: Get the translated version of the
 * locale category name.  Table driven will not work with xgettext(1)
 * command, therefor have to be function instead.
 */
static char *server_get_locale_name_str(int	id)
{
    char	*msg;

    switch (id) {
	/*
	 * STRING_EXTRACTION - Following six (includes "Unknown")
	 * messages are name of the OPEN LOOK locale categories.
	 */
	case OLLC_BASICLOCALE:
	    msg = XV_MSG_CONST("Basic Locale");
	    break;

	case OLLC_DISPLAYLANG:
	    msg = XV_MSG_CONST("Display Language");
	    break;

	case OLLC_INPUTLANG:
	    msg = XV_MSG_CONST("Input Language");
	    break;

	case OLLC_NUMERIC:
	    msg = XV_MSG_CONST("Numeric Format");
	    break;

	case OLLC_TIMEFORMAT:
	    msg = XV_MSG_CONST("Time Format");
	    break;

	default:
	    msg = XV_MSG_CONST("Unknown");
	    break;
    }
    return XV_MSG_VAR(msg);
}

static void server_effect_locale(Server_info *server, char	*character_set)
{
    int			 i;
    Ollc_item		*oi;
    char		*lc_all;
    Bool		 is_8859_1_locale;
    Bool		 is_c_locale;
    char		 msg[200];


    /*
     * Sets LC_ALL, so that we can cover the none OPEN LOOK locale
     * categories (such as LC_MONETARY).
     */
    oi = &server->ollc[OLLC_BASICLOCALE];
    if (oi->from != OLLC_FROM_POSIX
     && setlocale(LC_ALL, oi->locale) == NULL) {

	/*
	 * STRING_EXTRACTION - First %s is name of the locale, and
	 * second %s is for where the locale setting was came from
	 * (later in this file has a series of the message for second
	 * %s.  Translater could use printf mechanism to switch the
	 * order of the %s in SunOS 5.x, such as "%2$s" to specify the
	 * second %s (see printf(3S) for more detail).
	 */
	(void) sprintf(msg,
		       XV_MSG("Error when setting all locale categories to \"%s\" (set via %s)"),
		       oi->locale, server_get_locale_from_str(oi->from));
	server_warning(msg);
	lc_all = ""; /* need to set by indivisual category */
    } else
	lc_all = oi->locale;

    is_8859_1_locale = strcmp(character_set, ISO8859_1) == 0;
    is_c_locale = strcmp(server->ollc[OLLC_BASICLOCALE].locale, "C") == 0;

    for (i = 0, oi = server->ollc; i < OLLC_MAX; i++, oi++) {

	/*
	 * DEPEND_ON_EUC: Apply restriction for non latin1 locale (if
	 * it non latin1 locale, all locale categories are should be
	 * same as basic locale or "C").
	 */
	if ((oi != &server->ollc[OLLC_BASICLOCALE] && ! is_8859_1_locale
	   && strcmp(oi->locale, server->ollc[OLLC_BASICLOCALE].locale) != 0
	   && strcmp(oi->locale, "C") != 0)
	  || (is_c_locale && strcmp(oi->locale, "C") != 0)) {
	    /*
	     * STRING_EXTRACTION - first %s is name of the locale,
	     * second %s is name of the locale category, third %s is
	     * where the this locale setting was came from, fourth %s
	     * is translated "Basic Locale", and fifth %s is name of
	     * locale for basic locale.  Again, translater can change
	     * the order of those %s by using "%4$s" nortion of printf
	     * (3S) format in SunOS 5.x.
	     */
	    (void) sprintf(msg, XV_MSG("Can not use \"%s\" as locale category %s (set via %s) while %s is \"%s\" - Defaulting to \"C\""),
				oi->locale,
				server_get_locale_name_str(i),
			        server_get_locale_from_str(oi->from),
				server_get_locale_name_str(OLLC_BASICLOCALE),
				server->ollc[OLLC_BASICLOCALE].locale);
	    server_warning(msg);
	    xv_free(oi->locale);
	    oi->locale = strdup("C");
	    oi->from = OLLC_FROM_C;
	}

	/*
	 * Try not to set unless it is really necessary.
	 */
	if (Ollc_const[i].posix >= 0
	 && oi->from != OLLC_FROM_POSIX
	 && strcmp(oi->locale, lc_all) != 0
         && strcmp(oi->locale, setlocale(Ollc_const[i].posix, NULL)) != 0
	 && setlocale (Ollc_const[i].posix, oi->locale) == NULL) {

	    /*
	     * STRING_EXTRACTION - First %s name of the locale
	     * category, second %s is troubled locale name, and third
	     * %s is where this locale name was sets.
	     */
	    (void) sprintf(msg,
			   XV_MSG("Error when setting locale category (%s) to \"%s\" (set via %s"),
			   server_get_locale_name_str(i),
			   oi->locale,
			   server_get_locale_from_str(oi->from));
	    server_warning(msg);
	    xv_free(oi->locale);
	    oi->locale = strdup(setlocale(Ollc_const[i].posix, NULL));
	}
    }

    
    /*
     * Make sure, the supplied locale is supported, otherwise default
     * to C and continue.
     */
    if (!XSupportsLocale()) {
	oi = &server->ollc[OLLC_BASICLOCALE];
	(void) sprintf(msg, 
		       XV_MSG("Supplied locale \"%s\" (set via %s) is not supported by Xlib - Defaulting to \"C\""),
		       oi->locale,
		       server_get_locale_from_str(oi->from));
	server_warning(msg);
        setlocale(LC_ALL, "C");
	server_setlocale_to_c(server->ollc);
    }

    if(! XSetLocaleModifiers(""))
	server_warning(XV_MSG("Error in setting Xlib locale Modifiers"));


    /*
     * DEPEND_ON_OS: Should change when OS supplies word selection
     * functionality.  Bind locale specific word selection routines.
     */
    _wckind_init();

}
#endif /* OW_I18N */




static void server_setlocale_to_c(Ollc_item *ollc)
{
	Ollc_item *oi;

	for (oi = ollc; oi < &ollc[OLLC_MAX]; oi++) {
		xv_free(oi->locale);
		oi->locale = strdup("C");
		oi->from = OLLC_FROM_C;
	}
}


static void server_setlocale_to_default(Server_info	*server)
{
	char *def_locale;

	server_setlocale_to_c(server->ollc);

	/*
	 * Because of the compatiblity with release prior to none locale
	 * sensitive day of the XView (includes SunView), if XV_USE_LOCALE
	 * not being used, we will still allow 8 bit characters.
	 */
	if ((def_locale = getenv("XVIEW_DEFAULT_LOCALE")) == NULL) {
		/*
		 * "iso_8859_1" locale is 8 bit (LC_CTYPE only) locale exist
		 * in SunOS 4.x and 5.x.
		 */
		def_locale = "iso_8859_1";
	}
	xv_free(server->ollc[OLLC_BASICLOCALE].locale);
	server->ollc[OLLC_BASICLOCALE].locale = strdup(def_locale);
	(void)setlocale(LC_CTYPE, def_locale);

#ifdef OW_I18N
	if (!XSupportsLocale()) {
		char msg[256];
		(void)sprintf(msg,
				XV_MSG
				("Xlib does not support locale \"%s\" (which is for non internationalized program) - Defaulting to \"C\""),
				def_locale);
		server_warning(msg);
		xv_free(server->ollc[OLLC_BASICLOCALE].locale);
		server->ollc[OLLC_BASICLOCALE].locale = strdup("C");
		(void)setlocale(LC_CTYPE, "C");
	}
#endif
}
#endif /* OS_HAS_LOCALE */


static int server_destroy(Xv_Server server_public, Destroy_status status)
{
    /*
     * The Notifier knows about both screen and server objects.  When the
     * entire process is dying, the Notifier calls the destroy routines for
     * the objects in an arbitrary order.  We attempt to change the ordering
     * so that the screen(s) are destroyed before the server(s), so that the
     * screen(s) can always assume that the server(s) are valid. In addition,
     * destruction of a server causes destruction of every object attached to
     * that server.  [BUG ALERT!  Not currently implemented.]
     */
    Server_info    *server = SERVER_PRIVATE(server_public);
    Xv_Server       old_default_server = xv_default_server;
    int             i;
    Ollc_item	   *oi;

    /* Give screens a chance to clean up. */
    for (i = 0; i < MAX_SCREENS; i++)
	if (server->screens[i])
	    if (notify_post_destroy(server->screens[i], status,
		NOTIFY_IMMEDIATE) == XV_ERROR)
		return XV_ERROR;

    switch (status) {
      case DESTROY_PROCESS_DEATH:
	return XV_OK;

      case DESTROY_CLEANUP: {
	/* Remove the client from the notifier. */
	(void) notify_remove((Notify_client)server->xdisplay);
	if (xv_default_server == server_public) {
	    Server_info *new_server;

		/* If we are removing the default server while other
		 * valid server still remain, we must insure that 
		 * a new default server is assigned from the list of 
		 * remaining servers.
		 */
	    if ((new_server = (Server_info *)(XV_SL_SAFE_NEXT(server)))) {
		xv_default_server = SERVER_PUBLIC(new_server);
		xv_default_display = new_server->xdisplay; 
		xv_default_screen = new_server->screens[0];
	    } else {
	        /* Remove our scheduler else will deref null server */
	        notify_set_scheduler_func(default_scheduler);
		xv_default_server = (Xv_Server) NULL;
	        xv_default_display = (Display *) NULL;
	        xv_default_screen = (Xv_Screen) NULL;
	    }
	}
	XV_SL_REMOVE(SERVER_PRIVATE(old_default_server), server);

       destroy_atoms(server);
	xv_free(server->display_name);
	xv_free(server->composestatus);

        /* ACC_XVIEW */
	/*
	 * Free accelerator map if present
	 */
	if (server->acc_map)  {
	    xv_free(server->acc_map);
	    server->acc_map = (unsigned char *)NULL;
	}
        /* ACC_XVIEW */

	/*
	 * Free locale strings
	 */
	for (oi = server->ollc; oi < &server->ollc[OLLC_MAX]; oi++) {
	    if (oi->locale != NULL) {
		xv_free(oi->locale);
	    }
	}
	if (server->localedir)  {
	    xv_free(server->localedir);
	}
#ifdef OW_I18N
#ifdef FULL_R5
	if (server->preedit_style) {
	   xv_free(server->preedit_style);
	}
	if (server->status_style) {
	   xv_free(server->status_style);
	}
	if (server->supported_im_styles) {
	   if (server->supported_im_styles->supported_styles)
	      xv_free(server->supported_im_styles->supported_styles);
	   xv_free(server->supported_im_styles);
	}
#endif /* FULL_R5 */
	if (server->xim) {
            XCloseIM(server->xim);
            server->xim = NULL;
	}
#endif /* OW_I18N */

	XCloseDisplay(server->xdisplay);
	xv_free(server);
	break;
      }
      default:
	break;
    }

    return XV_OK;
}

static void destroy_atoms(Server_info *server)
{
    Server_atom_list    *head, *node;
    unsigned int         number_of_blocks;
    unsigned int         i;
 
    head = (Server_atom_list *)xv_get(SERVER_PUBLIC(server), XV_KEY_DATA,
                                                    server->atom_list_head_key);    node = head;
            
    number_of_blocks = (server->atom_list_number -1)/SERVER_LIST_SIZE;
 
                /* Each atom that is stored by the atom manager has a
                 * string associated with it and two X contexts.  If
                 * the server is being destroyed, we free the strings
                 * and contexts associated to atoms stored on the
                 * server object.
                 */
    for (i = 0; i <= number_of_blocks; i++) {
        unsigned int count,
                     j;
 
        if (i != number_of_blocks)
            count = SERVER_LIST_SIZE;
        else
            count = (server->atom_list_number -1)%SERVER_LIST_SIZE;
 
        for (j = 0; j < count; j++) {
            char        *atomName;
            XrmQuark     quark;
 
            XFindContext(server->xdisplay, server->atom_mgr[NAME],
                        (XContext)node->list[j], &atomName);
            quark = XrmStringToQuark(atomName);
 
            XDeleteContext(server->xdisplay, server->atom_mgr[ATOM],
                          (XContext)quark);
            XDeleteContext(server->xdisplay, server->atom_mgr[NAME],
                          (XContext)node->list[j]);
            xv_free(atomName);
        }
    }
                        /* Free up the atom manager stuff */
    head = (Server_atom_list *)xv_get(SERVER_PUBLIC(server), XV_KEY_DATA,
                                                    server->atom_list_head_key); 

    while ((node = (Server_atom_list *) (XV_SL_SAFE_NEXT(head))))
        xv_free(XV_SL_REMOVE_AFTER(head, head));
    xv_free(head);
}

/*
 * invoke the default scheduler, then flush all servers.
 */
static Notify_value scheduler(int n, Notify_client *clients)
{
	Notify_value status = (default_scheduler) (n, clients);
	Server_info *server;

	/* If xv_default_server is NULL we return because, scheduler()
	 * dereferences it.  The problem is that default_scheduler will
	 * process the xv_destroy(server) (nulling xv_default_server). 
	 * The second problem here is that scheduler assumes that
	 * there will always be an xv_default_server.  This is not true.  In
	 * a multi server env, the xv_default_server could be destroyed but
	 * other server will continue to be around to process events.
	 */
	if (!xv_default_server)
		return status;

	/*
	 * WARNING: we only want to process events from servers when the notifier
	 * is ready to run, not whenever the notifier gets called (e.g. as a
	 * result of xv_destroy() calling notify_post_destroy()). The notifier is
	 * ready to run either after xv_main_loop() calls notify_start(), or
	 * after the client calls notify_do_dispatch() or notify_dispatch().
	 */
	if ((status == NOTIFY_DONE) && xv_default_server &&
			(ndet_flags & (NDET_STARTED | NDET_DISPATCH)))
		XV_SL_TYPED_FOR_ALL(SERVER_PRIVATE(xv_default_server), server,
				Server_info *) {
		if (XPending((Display *) server->xdisplay))
			status = xv_input_pending(server->xdisplay,
										XConnectionNumber(server->xdisplay));
		XFlush(server->xdisplay);
		}

	return status;
}

/* Seit libX11-6-1.8.1-2.1.x86_64 (ca 12.7.2022) haengen ALLE XView-Programme
 * in diesem (jkwgehrwrei) XInternAtom-Aufruf. Das onlyIfExists TRUE scheint
 * das Gefaehrliche zu sein, in diesem Fall wird (vermutlich) irgendwie
 * XLockDisplay rekursiv aufgerufen.
 *
 * Aber das ist nur die erste Stelle..... die Xlib scheint selbst
 * (UNGEFRAGT) XInitThreads aufzurufen. 
 * Hier aber [[ https://x.org/releases/X11R7.5/doc/libX11/libX11.html ]]
 * wird gesagt
 * "It is recommended that single-threaded programs not call this function."
 *
 * Also: sollen die doch XInitThreads aufrufen......
 *
 * In Wirklichkeit ist meine Loesung hier doch nur ein Hack, um um die
 * Bloedheit der Xlib-Fritzen herumzuprogrammieren. Die rufen XInitThreads
 * auf und bremsen damit single thread Programme aus....
 */

int x_init_threads_called = 0;

Status XInitThreads(void)
{
	char *env = getenv("XVIEW_TELL_XINITTHREADS");

	if (env && *env) {
		fprintf(stderr, "XInitThreads called in %s\n", xv_app_name);
	}
	x_init_threads_called = 1;
	return 0;
}

static int xv_set_scheduler(void)
{
    /*
     * register a scheduler and an input handler with the notifier for this
     * process.
     */
    default_scheduler = notify_set_scheduler_func(scheduler);
    if (default_scheduler == NOTIFY_SCHED_FUNC_NULL) {
	notify_perror("xv_set_scheduler");
	return XV_ERROR;
    }
    return XV_OK;
}

static void server_init_atoms(Xv_Server server_public)
{
	Server_info *server = SERVER_PRIVATE(server_public);
	Atom atom;

	if (x_init_threads_called > 0) {
		/* das ist der Zustand ab dem 12.07.2022 - da haben die 
		 * Idioten angefangen, XInitThreads aufzurufen, auch wenn die
		 * Applikation single threaded ist (was die in der Xlib
		 * natuerlich nicht wissen koennen).
		 */
	}
	else {
		/* Aaaaah, anscheinend haben sie aufgehoert, XInitThreads
		 * aufzurufen - das soll der Drahota mitkriegen:
		 */
		fprintf(stderr, "pid %d: %s-%d: x_init_threads_called == 0!!!\n",
				getpid(), __FILE__, __LINE__);
	}

	/* I wonder what that "journalling" is all about.
	 * Obviously, an application starts in "journalling" mode if
	 * the atom JOURNAL_SYNC exists. So "somebody outside" must have 
	 * created it.
	 * [[ for example
	 *       xprop -root -format JOURNAL_SYNC 8s -set JOURNAL_SYNC lala
	 * ]]
	 * And then there seems to be a connection with the shell prompt
	 * (environment variable PROMPT)... see the variable xv_shell_prompt...
	 *
	 * There are a few places in XView where (in the journalling case)
	 * ClientMessage events of type JOURNAL_SYNC (SERVER_JOURNAL_SYNC_ATOM)
	 * are sent to the root window via the function server_journal_sync_event.
	 * 
	 * om_render.c: menu_render, cleanup
	 * notice_pt.c: notice_prepare_for_shadow, notice_block_popup
	 * txt_edit.c: textsw_do_input
	 * csr_change.c: ttysw_pstring
	 * tty_modes.c: ttysw_be_ttysw ttysw_be_termsw
	 * win_cursor.c: win_setmouseposition:
	 *                      XWarpPointer only if NOT in journalling mode
	 * win_global.c: xv_win_grab, xv_win_ungrab, win_xgrabio_sync,
	 *               win_xgrabio_async, win_releaseio
	 * win_input.c: FocusIn
	 * window.c: xv_main_loop
	 *
	 * However, I did not find any software that handles such ClientMessages
	 * on the root window....
	 *
	 * May I assume that this is something similar to vkbd - you can find 
	 * libxview sending client messages to some selection owner for
	 * _OL_SOFT_KEYS_PROCESS ....
	 *
	 * I could imagine some sort of external program (maybe with a fancy
	 * UI that you can use to somehow "trace" the operation of XView
	 * programs.
	 * See Ref (khbdrfvk_mbfrv) : they talk about a "journal process"....
	 *
	 * However, I don't even understand what a "journal" really is.
	 * And I can't see what benefit those client messages have - they do not
	 * carry any useful informations....
	 */

	/*
	 * do not create the SERVER_JOURNAL_ATOM atom if it does not already
	 * exists
	 */
	atom = XInternAtom(server->xdisplay,"JOURNAL_SYNC",TRUE); /* (jkwgehrwrei)*/
	if (atom == BadValue || atom == BadAlloc) {
		xv_error(XV_NULL,
				ERROR_SEVERITY, ERROR_NON_RECOVERABLE,
				ERROR_STRING,
				XV_MSG("Can't create SERVER_JOURNAL_ATOM atom"),
				ERROR_PKG, SERVER, NULL);
	}
	if (atom == None) {	/* not in journalling mode */
		server->journalling = FALSE;
	}
	else {	/* in journalling mode */
		int status, actual_format;
		unsigned long nitems, bytes;
		Atom actual_type;
		unsigned char *data;	/* default prompt */
		char *shell_ptr;
		xv_shell_prompt = (char *)xv_calloc(40, (unsigned)sizeof(char));

		/* check to see if this property hangs of the root window */

		status = XGetWindowProperty(server->xdisplay,
				DefaultRootWindow(server->xdisplay),
				atom, 0L, 2L, False, XA_INTEGER, &actual_type,
				&actual_format, &nitems, &bytes, &data);

		if (status != Success || actual_type == None) {
			server->journalling = FALSE;
			XFree((char *)data);
		}
		else {
			server->journalling = TRUE;
			if ((shell_ptr = getenv("PROMPT")) == NULL) {
				xv_shell_prompt[0] = '%';
			}
			else {
				(void)strcpy(xv_shell_prompt, shell_ptr);
			}
			(void)xv_set(server_public, SERVER_JOURNAL_SYNC_ATOM, atom, NULL);
		}
	}
}

typedef struct {
	const char		*name;
	const Server_atom_type	 type;
} Server_atom2type;

const static Server_atom2type Server_atom2type_tbl[] = {
/*
 * Top to marked line is ordered by frequency of the usage (startup
 * plus some common operation).
 */
	{"_OL_DECOR_DEL",		SERVER_WM_DELETE_DECOR_TYPE},
	{"_OL_WIN_ATTR",		SERVER_WM_WIN_ATTR_TYPE},
	{"_SUN_DRAGDROP_INTEREST",	SERVER_WM_DRAGDROP_INTEREST_TYPE},
	{"_OL_PIN_STATE",		SERVER_WM_PIN_STATE_TYPE},
	{"WM_PROTOCOLS",		SERVER_WM_PROTOCOLS_TYPE},
	{"WM_TAKE_FOCUS",		SERVER_WM_TAKE_FOCUS_TYPE},
	{"WM_DELETE_WINDOW",		SERVER_WM_DELETE_WINDOW_TYPE},
	{"_SUN_DRAGDROP_TRIGGER",	SERVER_WM_DRAGDROP_TRIGGER_TYPE},
	{"_OL_WIN_BUSY",		SERVER_WM_WIN_BUSY_TYPE},
	{"WM_SAVE_YOURSELF",		SERVER_WM_SAVE_YOURSELF_TYPE},
/*
 * Ordered list end.
 */
	{"_OL_DECOR_ADD",		SERVER_WM_ADD_DECOR_TYPE},
	{"_OL_DECOR_CLOSE",		SERVER_WM_DECOR_CLOSE_TYPE},
	{"_OL_DECOR_FOOTER",		SERVER_WM_DECOR_FOOTER_TYPE},
	{"_OL_DECOR_RESIZE",		SERVER_WM_DECOR_RESIZE_TYPE},
	{"_OL_DECOR_HEADER",		SERVER_WM_DECOR_HEADER_TYPE},
	{"_OL_DECOR_OK",		SERVER_WM_DECOR_OK_TYPE},
	{"_OL_DECOR_PIN",		SERVER_WM_DECOR_PIN_TYPE},
	{"_OL_SCALE_SMALL",		SERVER_WM_SCALE_SMALL_TYPE},
	{"_OL_SCALE_MEDIUM",		SERVER_WM_SCALE_MEDIUM_TYPE},
	{"_OL_SCALE_LARGE",		SERVER_WM_SCALE_LARGE_TYPE},
	{"_OL_SCALE_XLARGE",		SERVER_WM_SCALE_XLARGE_TYPE},
	{"_OL_WINMSG_STATE",		SERVER_WM_WINMSG_STATE_TYPE},
	{"_OL_WINMSG_ERROR",		SERVER_WM_WINMSG_ERROR_TYPE},
	{"_OL_WT_BASE",			SERVER_WM_WT_BASE_TYPE},
	{"_OL_WT_CMD",			SERVER_WM_WT_CMD_TYPE},
	{"_OL_WT_PROP",			SERVER_WM_WT_PROP_TYPE},
	{"_OL_WT_HELP",			SERVER_WM_WT_HELP_TYPE},
	{"_OL_WT_NOTICE",		SERVER_WM_WT_NOTICE_TYPE},
	{"_OL_WT_OTHER",		SERVER_WM_WT_OTHER_TYPE},
	{"_OL_MENU_FULL",		SERVER_WM_MENU_FULL_TYPE},
	{"_OL_MENU_LIMITED",		SERVER_WM_MENU_LIMITED_TYPE},
	{"_OL_NONE",			SERVER_WM_NONE_TYPE},
	{"_OL_PIN_IN",			SERVER_WM_PIN_IN_TYPE},
	{"_OL_PIN_OUT",			SERVER_WM_PIN_OUT_TYPE},
	{"XV_DO_DRAG_MOVE",		SERVER_DO_DRAG_MOVE_TYPE},
	{"XV_DO_DRAG_COPY",		SERVER_DO_DRAG_COPY_TYPE},
	{"XV_DO_DRAG_LOAD",		SERVER_DO_DRAG_LOAD_TYPE},
	{"_OL_WIN_DISMISS",		SERVER_WM_DISMISS_TYPE},
	{"WM_CHANGE_STATE",		SERVER_WM_CHANGE_STATE_TYPE},
	{"_OL_DFLT_BTN",		SERVER_WM_DEFAULT_BUTTON_TYPE},
	{"_SUN_DRAGDROP_PREVIEW",	SERVER_WM_DRAGDROP_PREVIEW_TYPE},
	{"_SUN_DRAGDROP_ACK",		SERVER_WM_DRAGDROP_ACK_TYPE},
	{"_SUN_DRAGDROP_DONE",		SERVER_WM_DRAGDROP_DONE_TYPE},
#ifdef OW_I18N
	{"COMPOUND_TEXT",		SERVER_COMPOUND_TEXT_TYPE},
#endif /* OW_I18N */
	{NULL,				0}
};

Xv_private Server_atom_type server_get_atom_type(Xv_Server server_public,
													Atom atom)
{
	XPointer xtype;
    Server_atom_type    type;
    Server_info        *server = SERVER_PRIVATE(server_public);


    if (XFindContext(server->xdisplay, server->atom_mgr[TYPE], 
		     (XContext)atom, &xtype) != XCNOENT)
	{
		return (Server_atom_type)xtype;
	}
    else {
		char *atomName;
		const Server_atom2type *tbl;

		if ((int) atom <= XA_LAST_PREDEFINED)      /* Cache predefined atoms */
		{
			return (save_atom(server, atom, SERVER_WM_UNKNOWN_TYPE));
		}

		atomName = (char *)xv_get(server_public, SERVER_ATOM_NAME, atom);

		for (tbl = Server_atom2type_tbl; tbl->name != NULL; tbl++) {
			if (strcmp(atomName, tbl->name) == 0) {
				type = save_atom(server, atom, tbl->type);
				break;
			}
		}
		if (tbl->name == NULL) {
			type = save_atom(server, atom, SERVER_WM_UNKNOWN_TYPE);
		}

		return type;
	}
}

static Server_atom_type save_atom(Server_info *server, Atom	atom, Server_atom_type type)
{
	(void) XSaveContext(server->xdisplay, server->atom_mgr[TYPE],
			 (XContext) atom, (caddr_t) type);
	return (type); 
}

/*
 * BUG:  use_default_mapping should be set by comparing the default keycode
 * to keysym table.
 */
Xv_private void server_journal_sync_event(Xv_Server server_public, int type)
{
	Server_info *server = SERVER_PRIVATE(server_public);
	Atom sync_atom = (Atom) xv_get(server_public, SERVER_JOURNAL_SYNC_ATOM);
	XEvent send_event;
	XClientMessageEvent *cme = (XClientMessageEvent *) & send_event;
	Display *dpy = (Display *) server->xdisplay;

	/*
	 * Xv_Drawable_info       *info;
	 */

	cme->type = ClientMessage;
	/* get the xid of the root window -- not 100% correct */
	/*
	 * DRAWABLE_INFO(xv_get(xv_get(server_public,SERVER_NTH_SCREEN,0),XV_ROOT),
	 * info); cme->window = xv_xid(info); */
	cme->window = DefaultRootWindow((Display *) server->xdisplay);
	cme->message_type = sync_atom;
	cme->format = 32;
	cme->data.l[0] = type;
#ifdef BEFORE_DRA_CHANGED_IT
#else
	cme->data.l[1] = server->xtime;
	cme->data.l[2] = getpid();
#endif

	/* Ref (khbdrfvk_mbfrv) */
	XSync(dpy, 0);	/* make sure journal process has been 
					 * scheduled and is waiting for the sync
					 * event */
	SERVERTRACE((45, "sending  JOURNAL_SYNC to root %lx\n", cme->window));
	/* I wonder who is supposed to receive this message - according
	 * to "man XSendEvent" :
	 * "If event_mask is the empty set, the event is sent to the client
	 *  that created the destination window. If that client no longer exists,
	 *  no event is sent."
	 * The client who created the root window ????? Does not exists.
	 */
#ifdef BEFORE_DRA_CHANGED_IT
	/* solche Events habe ich niemals gesehen... */
	XSendEvent(dpy, cme->window, 0, 0L, (XEvent *) cme);
#else
	/* at least the window manager might be interested in properties? */
	XSendEvent(dpy, cme->window, 0, PropertyChangeMask, (XEvent *) cme);
#endif
	XSync(dpy, 0);	/* make sure journal event has occurred */
}

Xv_private void xv_string_to_rgb(char *buffer, unsigned char *red, unsigned char *green, unsigned char *blue)
{
        int     hex_buffer;
        unsigned char   *conv_ptr;
        (void) sscanf(buffer, "#%6x", &hex_buffer);

        conv_ptr = (unsigned char *) &hex_buffer;
        *red = conv_ptr[1];
        *green = conv_ptr[2];
        *blue = conv_ptr[3];
}

static unsigned int string_to_modmask(Server_info *srv, char *str)
{
	if (strcmp(str, "Shift") == 0) 
		return ShiftMask;
	else if (strcmp(str, "Ctrl") == 0) 
		return ControlMask;
	else if (strcmp(str, "Meta") == 0) 
		return srv->meta_modmask;
	else  { /* Punt for now, just return Mod1Mask */
		/* What really needs to be done here is look up the 
		   modifier mapping from the server and add the new modifier
		   keys we are now interested in.   			     */
		server_warning(XV_MSG("Only support Shift, Ctrl and Meta as mouse button modifiers"));
		return srv->meta_modmask;
	}
}

#ifdef OW_I18N
#ifdef FULL_R5
static XIMStyle
xv_determine_im_style(im, avail_styles, req_preedit_style, req_status_style)
        XIM             im;
        XIMStyles       *avail_styles;     /* styles supported by IM server & toolkit*/
        char            *req_preedit_style;  /* requested input style         */
        char            *req_status_style; /* requested status style        */
{
 
        XIMStyle        style = NULL;
        XIMStyle        supported_styles[XV_SUPPORTED_STYLE_COUNT];
        int             i,j;
 
        /* 
         * Determine requested IM style
	 */
	if (req_preedit_style) {
           if (!strcmp(req_preedit_style, "onTheSpot"))
                style = XIMPreeditCallbacks;
           else if (!strcmp(req_preedit_style,"overTheSpot"))
                style = XIMPreeditPosition;
           else if (!strcmp(req_preedit_style,"offTheSpot"))
                style = XIMPreeditArea;
           else if (!strcmp(req_preedit_style,"rootWindow"))
                style = XIMPreeditNothing;
           else if (!strcmp(req_preedit_style,"none"))
                style = XIMPreeditNone;
	}
 
	if (req_status_style) {
           if (!strcmp(req_status_style,"clientDisplays"))
                style |= XIMStatusCallbacks;
           else if (!strcmp(req_status_style,"imDisplaysInClient"))
                style |= XIMStatusArea;
           else if (!strcmp(req_status_style,"imDisplaysInRoot"))
                style |= XIMStatusNothing;
           else if (!strcmp(req_status_style,"none"))
                style |= XIMStatusNone;
	}
         
        /* 
	 * Find matching requested and supported style. 
	 */
        for (i=0; i < (int)avail_styles->count_styles; i++) 
           if (style == avail_styles->supported_styles[i])
              return((XIMStyle)style);
 
        /* Requested style is not supported, default to 
	 * XIMPreeditNothing and XIMStatusNothing if it's available.
         */
	server_warning(XV_MSG("Requested input method style not supported."));
	if (style != (XIMPreeditNothing | XIMStatusNothing)) {
            for (i=0; i < (int)avail_styles->count_styles; i++) 
                 if ((XIMPreeditNothing | XIMStatusNothing) == avail_styles->supported_styles[i])
                     return((XIMStyle)(XIMPreeditNothing | XIMStatusNothing));
	} else return (NULL);
}
#endif /* FULL_R5 */
#endif /* OW_I18N */


static void server_warning(char *msg)
{
    xv_error(XV_NULL,
	     ERROR_STRING,	msg,
	     ERROR_PKG,		SERVER,
	     NULL);
}

Xv_private void server_register_ui(Xv_server srv, Xv_opaque uiElem, const char *name)
{
    Server_info *server = SERVER_PRIVATE(srv);

	if (server->ui_reg_proc) {
		(server->ui_reg_proc)(srv, uiElem, name);
	}
}

#define TRACE_STACK_DEPTH 20

typedef struct {
    const char *cur_file;
	int cur_line;
} trace_stack_entry_t;

static trace_stack_entry_t stack[TRACE_STACK_DEPTH];
static trace_stack_entry_t *stackptr = stack;

Xv_private void server_trace_set_file_line(const char *file, int line)
{
	if (++stackptr >= stack + TRACE_STACK_DEPTH) {
		fprintf(stderr, "\n\niso_trace: STACK_DEPTH reached, aborting...\n\n");
		abort();
	}
	stackptr->cur_file = file;
	stackptr->cur_line = line;
}

Xv_private void server_trace(int level, const char *format, ...)
{
	Xv_server srv = xv_default_server; /* das sollte fuer alle Zwecke reichen */

	if (srv) {
    	Server_info *server = SERVER_PRIVATE(srv);

		if (server->trace_proc) {
			char buf[4000];
			va_list pvar;

			va_start(pvar, format);
			vsnprintf(buf, sizeof(buf) - 1, format, pvar);
			va_end(pvar);
			(server->trace_proc)(srv, level, stackptr->cur_file,
									stackptr->cur_line, buf);
		}
	}
	--stackptr;
}

char *server_get_instance_appname(void)
{
	extern char    *xv_instance_app_name;

	return xv_instance_app_name;
}

void server_appl_busy(Frame fram, int busy, Frame except)
{
	Xv_server srv = XV_SERVER_FROM_WINDOW(fram);

	xv_set(srv, SERVER_APPL_BUSY, busy, except, NULL);
}

const Xv_pkg xv_server_pkg = {
    "Server",
    ATTR_PKG_SERVER,
    sizeof(Xv_server_struct),
    &xv_generic_pkg,
    server_init,
    server_set_avlist,
    server_get_attr,
    server_destroy,
    NULL			/* no find proc */
};
