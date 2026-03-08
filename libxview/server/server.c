#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)server.c 20.157 93/04/28 DRA: $Id: server.c,v 4.41 2026/03/07 19:57:21 dra Exp $";
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
#include <sys/utsname.h>
#include <dirent.h>
#include <math.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <langinfo.h>
#include <ctype.h>
#include <pwd.h>
#include <xview/win_input.h>
#include <xview/win_struct.h>
#include <xview_private/ntfy.h>
#include <xview_private/ndet.h>
#include <xview/notify.h>
#include <xview/win_notify.h>
#include <xview/defaults.h>
#include <xview/font.h>
#include <xview/panel.h>
#include <xview/permprop.h>
#include <xview/icon.h>
#include <xview/openmenu.h>
#include <xview/win_notify.h>
#include <xview/help.h>
#include <X11/Xlib.h>
#include <X11/Xlibint.h>
#include <xview_private/portable.h>
#include <xview_private/svr_atom.h>
#include <xview_private/svr_impl.h>
#include <xview_private/draw_impl.h>
#include <xview_private/win_info.h>
#include <xview_private/xv_color.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xresource.h>
#include <dlfcn.h>

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

Xv_private Xv_opaque xv_font_with_name(Xv_server, char *);

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

#define MAX_NBR_MAPPINGS 6

typedef struct key_binding {
	short	    action;	/* XView semantic action */
	char	   *name;	/* resource name and class */
	char	   *value;	/* default value.  May contain up to
				 * MAX_NBR_MAPPINGS mappings. */
} Key_binding;

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

	/*************************************************************************
	 *
	 * Mouseless Model bindings.  Translates keysyms to XView semantic actions.
	 *
	 *************************************************************************/
	static Key_binding sunview1_kbd_cmds[] = {
		/*
		 * Keyboard Core Functions
		 */
		{ ACTION_STOP, "OpenWindows.KeyboardCommand.Stop", "Cancel" }, /* L1 */
		{ ACTION_AGAIN, "OpenWindows.KeyboardCommand.Again",
													"a+Meta,a+Ctrl+Meta,L2" },
		{ ACTION_PROPS, "OpenWindows.KeyboardCommand.Props", "F13" }, /* L3 */
		{ ACTION_UNDO, "OpenWindows.KeyboardCommand.Undo",
				"u+Meta,Undo" }, /* "u+Meta,L4"  */
		{ ACTION_COPY, "OpenWindows.KeyboardCommand.Copy",
				"c+Meta,F16" }, /* "c+Meta,L6"  */
		{ ACTION_PASTE, "OpenWindows.KeyboardCommand.Paste",
				"v+Meta,F18" }, /* "v+Meta,L8"  */
		{ ACTION_FIND_FORWARD, "OpenWindows.KeyboardCommand.FindForward",
				"f+Meta,Find" },
		{ ACTION_FIND_BACKWARD, "OpenWindows.KeyboardCommand.FindBackward",
				"F+Meta,Find+Shift" },
		{ ACTION_CUT, "OpenWindows.KeyboardCommand.Cut", "x+Meta,F20" }, /* "x+Meta,L10"  */
		{ ACTION_HELP, "OpenWindows.KeyboardCommand.Help", "Help" },
		{ ACTION_MORE_HELP, "OpenWindows.KeyboardCommand.MoreHelp", "Help+Shift" },
		{ ACTION_TEXT_HELP, "OpenWindows.KeyboardCommand.TextHelp", "Help+Ctrl" },
		{ ACTION_MORE_TEXT_HELP, "OpenWindows.KeyboardCommand.MoreTextHelp",
				"Help+Shift+Ctrl" },
		{ ACTION_DEFAULT_ACTION, "OpenWindows.KeyboardCommand.DefaultAction",
				"KP_Enter,Return+Meta" }, /* "Return+Meta"  */
		{ ACTION_COPY_THEN_PASTE, "OpenWindows.KeyboardCommand.CopyThenPaste",
				"p+Meta" },
		{ ACTION_TRANSLATE, "OpenWindows.KeyboardCommand.Translate",
				"F22" }, /* "R2"  */
		/*
		 * Local Navigation Commands
		 */
		{ ACTION_UP, "OpenWindows.KeyboardCommand.Up",
				"p+Ctrl,N+Ctrl,Up,R8,Up+Shift" },
		{ ACTION_DOWN, "OpenWindows.KeyboardCommand.Down",
				"n+Ctrl,P+Ctrl,Down,R14,Down+Shift" },
		{ ACTION_LEFT, "OpenWindows.KeyboardCommand.Left",
				"b+Ctrl,F+Ctrl,Left,R10,Left+Shift" },
		{ ACTION_RIGHT, "OpenWindows.KeyboardCommand.Right",
				"f+Ctrl,B+Ctrl,Right,R12,Right+Shift" },
		{ ACTION_JUMP_LEFT, "OpenWindows.KeyboardCommand.JumpLeft",
				"comma+Ctrl,greater+Ctrl" },
		{ ACTION_JUMP_RIGHT, "OpenWindows.KeyboardCommand.JumpRight", "period+Ctrl" },
		{ ACTION_GO_PAGE_BACKWARD, "OpenWindows.KeyboardCommand.GoPageBackward", "Prior,R9" },
		{ ACTION_GO_PAGE_FORWARD, "OpenWindows.KeyboardCommand.GoPageForward", "Next,R15" },
		{ ACTION_GO_WORD_FORWARD, "OpenWindows.KeyboardCommand.GoWordForward",
				"slash+Ctrl,less+Ctrl" },
		{ ACTION_LINE_START, "OpenWindows.KeyboardCommand.LineStart", "a+Ctrl,E+Ctrl" },
		{ ACTION_LINE_END, "OpenWindows.KeyboardCommand.LineEnd", "e+Ctrl,A+Ctrl" },
		{ ACTION_GO_LINE_FORWARD, "OpenWindows.KeyboardCommand.GoLineForward",
				"apostrophe+Ctrl,R11" },
		{ ACTION_DATA_START, "OpenWindows.KeyboardCommand.DataStart",
				"Home,R7,Return+Shift+Ctrl,Home+Shift" },
		{ ACTION_DATA_END, "OpenWindows.KeyboardCommand.DataEnd",
				"End,R13,Return+Ctrl,End+Shift" },
		/*
		 * Text Editing Commands
		 */
		{ ACTION_SELECT_FIELD_FORWARD,
				"OpenWindows.KeyboardCommand.SelectFieldForward", "Tab+Ctrl" },
		{ ACTION_SELECT_FIELD_BACKWARD,
				"OpenWindows.KeyboardCommand.SelectFieldBackward", "Tab+Shift+Ctrl" },
		{ ACTION_ERASE_CHAR_BACKWARD, "OpenWindows.KeyboardCommand.EraseCharBackward",
				"BackSpace" },
		{ ACTION_ERASE_CHAR_FORWARD, "OpenWindows.KeyboardCommand.EraseCharForward",
				"Delete,BackSpace+Shift" },
		{ ACTION_ERASE_WORD_BACKWARD, "OpenWindows.KeyboardCommand.EraseWordBackward",
				"w+Ctrl" },
		{ ACTION_ERASE_WORD_FORWARD, "OpenWindows.KeyboardCommand.EraseWordForward",
				"W+Ctrl" },
		{ ACTION_ERASE_LINE_BACKWARD, "OpenWindows.KeyboardCommand.EraseLineBackward",
				"u+Ctrl" },
		{ ACTION_ERASE_LINE_END, "OpenWindows.KeyboardCommand.EraseLineEnd", "U+Ctrl"} ,
		{ ACTION_MATCH_DELIMITER, "OpenWindows.KeyboardCommand.MatchDelimiter" ,
				"d+Meta" },
		{ ACTION_EMPTY, "OpenWindows.KeyboardCommand.Empty", "e+Meta,e+Ctrl+Meta" },
		{ ACTION_INCLUDE_FILE, "OpenWindows.KeyboardCommand.IncludeFile", "i+Meta" },
		{ ACTION_INSERT, "OpenWindows.KeyboardCommand.Insert", "Insert" },
		{ ACTION_LOAD, "OpenWindows.KeyboardCommand.Load", "l+Meta" },
		{ ACTION_STORE, "OpenWindows.KeyboardCommand.Store", "s+Meta" },
		{ 0, NULL, NULL }
	};

	static Key_binding basic_kbd_cmds[] = {
		/*
		 * Keyboard Core Functions
		 */
		{ ACTION_QUOTE_NEXT_KEY, "OpenWindows.KeyboardCommand.QuoteNextKey", "q+Alt" },
		/*
		 * Local Navigation Commands
		 */
		{ ACTION_UP, "OpenWindows.KeyboardCommand.Up", "Up" },
		{ ACTION_DOWN, "OpenWindows.KeyboardCommand.Down", "Down" },
		{ ACTION_LEFT, "OpenWindows.KeyboardCommand.Left", "Left" },
		{ ACTION_RIGHT, "OpenWindows.KeyboardCommand.Right", "Right" },
		{ ACTION_JUMP_UP, "OpenWindows.KeyboardCommand.JumpUp", "Up+Ctrl" },
		{ ACTION_JUMP_DOWN, "OpenWindows.KeyboardCommand.JumpDown", "Down+Ctrl" },
		{ ACTION_JUMP_LEFT, "OpenWindows.KeyboardCommand.JumpLeft", "Left+Ctrl" },
		{ ACTION_JUMP_RIGHT, "OpenWindows.KeyboardCommand.JumpRight", "Right+Ctrl" },
		{ ACTION_ROW_START, "OpenWindows.KeyboardCommand.RowStart", "Home,R7" },
		{ ACTION_ROW_END, "OpenWindows.KeyboardCommand.RowEnd", "End,R13" },
		{ ACTION_PANE_UP, "OpenWindows.KeyboardCommand.PaneUp", "Prior,R9" },
		{ ACTION_PANE_DOWN, "OpenWindows.KeyboardCommand.PaneDown", "Next,R15" },
		{ ACTION_PANE_LEFT, "OpenWindows.KeyboardCommand.PaneLeft", "Prior+Ctrl,R9+Ctrl" },
		{ ACTION_PANE_RIGHT, "OpenWindows.KeyboardCommand.PaneRight", "Next+Ctrl,R15+Ctrl" },
		{ ACTION_DATA_START, "OpenWindows.KeyboardCommand.DataStart",
				"Home+Ctrl,R7+Ctrl" },
		{ ACTION_DATA_END, "OpenWindows.KeyboardCommand.DataEnd", "End+Ctrl,R13+Ctrl" },
		/*
		 * Text Editing Commands
		 */
		{ ACTION_SELECT_UP, "OpenWindows.KeyboardCommand.SelectUp", "Up+Shift" },
		{ ACTION_SELECT_DOWN, "OpenWindows.KeyboardCommand.SelectDown", "Down+Shift" },
		{ ACTION_SELECT_LEFT, "OpenWindows.KeyboardCommand.SelectLeft", "Left+Shift" },
		{ ACTION_SELECT_RIGHT, "OpenWindows.KeyboardCommand.SelectRight",
				"Right+Shift" },
		{ ACTION_SELECT_JUMP_UP, "OpenWindows.KeyboardCommand.SelectJumpUp",
				"Up+Shift+Ctrl" },
		{ ACTION_SELECT_JUMP_DOWN, "OpenWindows.KeyboardCommand.SelectJumpDown",
				"Down+Shift+Ctrl" },
		{ ACTION_SELECT_JUMP_LEFT, "OpenWindows.KeyboardCommand.SelectJumpLeft",
				"Left+Shift+Ctrl" },
		{ ACTION_SELECT_JUMP_RIGHT, "OpenWindows.KeyboardCommand.SelectJumpRight",
				"Right+Shift+Ctrl" },
		{ ACTION_SELECT_ROW_START, "OpenWindows.KeyboardCommand.SelectRowStart",
				"Home+Shift,R7+Shift" },
		{ ACTION_SELECT_ROW_END, "OpenWindows.KeyboardCommand.SelectRowEnd",
				"End+Shift,R13+Shift" },
		{ ACTION_SELECT_PANE_UP, "OpenWindows.KeyboardCommand.SelectPaneUp",
				"Prior+Shift,R9+Shift" },
		{ ACTION_SELECT_PANE_DOWN, "OpenWindows.KeyboardCommand.SelectPaneDown",
				"Next+Shift,R15+Shift" },
		{ ACTION_SELECT_PANE_LEFT, "OpenWindows.KeyboardCommand.SelectPaneLeft",
				"Prior+Shift+Ctrl,R9+Shift+Ctrl" },
		{ ACTION_SELECT_PANE_RIGHT, "OpenWindows.KeyboardCommand.SelectPaneRight",
				"Next+Shift+Ctrl,R15+Shift+Ctrl" },
		{ ACTION_SELECT_DATA_START, "OpenWindows.KeyboardCommand.SelectDataStart",
				"Home+Shift+Ctrl,R7+Shift+Ctrl" },
		{ ACTION_SELECT_DATA_END, "OpenWindows.KeyboardCommand.SelectDataEnd",
				"End+Shift+Ctrl,R13+Shift+Ctrl" },
		{ ACTION_SELECT_ALL, "OpenWindows.KeyboardCommand.SelectAll",
				"End+Shift+Meta,R13+Shift+Meta" },
		{ ACTION_SELECT_NEXT_FIELD, "OpenWindows.KeyboardCommand.SelectNextField",
				"Tab+Meta" },
		{ ACTION_SELECT_PREVIOUS_FIELD,
				"OpenWindows.KeyboardCommand.SelectPreviousField", "Tab+Shift+Meta" },
		{ ACTION_SCROLL_UP, "OpenWindows.KeyboardCommand.ScrollUp", "Up+Alt" },
		{ ACTION_SCROLL_DOWN, "OpenWindows.KeyboardCommand.ScrollDown", "Down+Alt" },
		{ ACTION_SCROLL_LEFT, "OpenWindows.KeyboardCommand.ScrollLeft", "Left+Alt" },
		{ ACTION_SCROLL_RIGHT, "OpenWindows.KeyboardCommand.ScrollRight", "Right+Alt" },
		{ ACTION_SCROLL_JUMP_UP, "OpenWindows.KeyboardCommand.ScrollJumpUp",
				"Up+Alt+Ctrl" },
		{ ACTION_SCROLL_JUMP_DOWN, "OpenWindows.KeyboardCommand.ScrollJumpDown",
				"Down+Alt+Ctrl" },
		{ ACTION_SCROLL_JUMP_LEFT, "OpenWindows.KeyboardCommand.ScrollJumpLeft",
				"Left+Alt+Ctrl" },
		{ ACTION_SCROLL_JUMP_RIGHT, "OpenWindows.KeyboardCommand.ScrollJumpRight",
				"Right+Alt+Ctrl" },
		{ ACTION_SCROLL_ROW_START, "OpenWindows.KeyboardCommand.ScrollRowStart",
				"Home+Alt,R7+Alt" },
		{ ACTION_SCROLL_ROW_END, "OpenWindows.KeyboardCommand.ScrollRowEnd",
				"End+Alt,R13+Alt" },
		{ ACTION_SCROLL_PANE_UP, "OpenWindows.KeyboardCommand.ScrollPaneUp",
				"Prior+Alt,R9+Alt" },
		{ ACTION_SCROLL_PANE_DOWN, "OpenWindows.KeyboardCommand.ScrollPaneDown",
				"Next+Alt,R15+Alt" },
		{ ACTION_SCROLL_PANE_LEFT, "OpenWindows.KeyboardCommand.ScrollPaneLeft",
				"Prior+Alt+Ctrl,R9+Alt+Ctrl" },
		{ ACTION_SCROLL_PANE_RIGHT, "OpenWindows.KeyboardCommand.ScrollPaneRight",
				"Next+Alt+Ctrl,R15+Alt+Ctrl" },
		{ ACTION_SCROLL_DATA_START, "OpenWindows.KeyboardCommand.ScrollDataStart",
				"Home+Alt+Ctrl,R7+Alt+Ctrl" },
		{ ACTION_SCROLL_DATA_END, "OpenWindows.KeyboardCommand.ScrollDataEnd",
				"End+Alt+Ctrl,R13+Alt+Ctrl" },
		{ ACTION_ERASE_CHAR_BACKWARD, "OpenWindows.KeyboardCommand.EraseCharBackward",
				"BackSpace" },
		{ ACTION_ERASE_CHAR_FORWARD, "OpenWindows.KeyboardCommand.EraseCharForward",
				"Delete,BackSpace+Shift" },
		{ ACTION_ERASE_LINE, "OpenWindows.KeyboardCommand.EraseLine",
				"Delete+Meta,BackSpace+Meta" },
		{ 0, NULL, NULL }
	};

	static Key_binding full_kbd_cmds[] = {
		/*
		 * Mouseless Core Functions
		 */
		{ ACTION_ADJUST, "OpenWindows.KeyboardCommand.Adjust", "Insert+Alt" },
		{ ACTION_MENU, "OpenWindows.KeyboardCommand.Menu", "space+Alt" },
		{ ACTION_INPUT_FOCUS_HELP, "OpenWindows.KeyboardCommand.InputFocusHelp",
				"question+Ctrl" },
		{ ACTION_SUSPEND_MOUSELESS, "OpenWindows.KeyboardCommand.SuspendMouseless",
				"z+Alt" },
		{ ACTION_RESUME_MOUSELESS, "OpenWindows.KeyboardCommand.ResumeMouseless",
				"Z+Alt" },
		{ ACTION_JUMP_MOUSE_TO_INPUT_FOCUS,
				"OpenWindows.KeyboardCommand.JumpMouseToInputFocus", "j+Alt" },
		/*
		 * Global Navigation Commands
		 */
		{ ACTION_NEXT_ELEMENT, "OpenWindows.KeyboardCommand.NextElement", "Tab+Ctrl" },
		{ ACTION_PREVIOUS_ELEMENT, "OpenWindows.KeyboardCommand.PreviousElement",
				"Tab+Shift+Ctrl" },
		{ ACTION_NEXT_PANE, "OpenWindows.KeyboardCommand.NextPane", "a+Alt" },
		{ ACTION_PREVIOUS_PANE, "OpenWindows.KeyboardCommand.PreviousPane",
				"A+Alt" },
		/*
		 * Miscellaneous Navigation Commands
		 */
		{ ACTION_PANEL_START, "OpenWindows.KeyboardCommand.PanelStart",
				"bracketleft+Ctrl" },
		{ ACTION_PANEL_END, "OpenWindows.KeyboardCommand.PanelEnd",
				"bracketright+Ctrl" },
		{ ACTION_VERTICAL_SCROLLBAR_MENU,
				"OpenWindows.KeyboardCommand.VerticalScrollbarMenu", "v+Alt" },
		{ ACTION_HORIZONTAL_SCROLLBAR_MENU,
				"OpenWindows.KeyboardCommand.HorizontalScrollbarMenu", "h+Alt" },
		{ ACTION_PANE_BACKGROUND, "OpenWindows.KeyboardCommand.PaneBackground",
				"b+Alt" },
		{ 0, NULL, NULL }
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
	max_top = defaults_get_integer("openWindows.numberOfTopFkeys",
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

static void call_real_XInitThreads(void)
{
	typedef void (*initthr_t)(void);
	void *lib = dlopen("libX11.so", RTLD_NOW);
	initthr_t it;

	if (! lib) return;

	it = (initthr_t)dlsym(lib, "XInitThreads");
	if (! it) {
		fprintf(stderr, "cannot find XInitThreads in libX11.so\n");
		return;
	}

	(*it)();
	dlclose(lib);
}

static void update_atom_list(Server_info *server, Atom atom)
{
	unsigned int slot;
	Server_atom_list *atom_list_tail, *atom_list_head;

	/* Our list is made up of blocks of atoms.  When 
	 * a block is full, we allocate a new block.  These
	 * blocks of data are needed when the server is 
	 * destroyed.  We use them to free up all the
	 * XContext manager stuff.
	 */

	/* Get the tail of our list. */
	atom_list_tail = (Server_atom_list *) xv_get(SERVER_PUBLIC(server),
			XV_KEY_DATA, server->atom_list_tail_key);

	/* Figure out what slot in this block is empty. */
	slot = server->atom_list_number % SERVER_LIST_SIZE;

	/* If this is slot 0, we create a new block because we
	 * know we filled up the old one.
	 */
	if (slot == 0 && (server->atom_list_number / SERVER_LIST_SIZE != 0)) {
		Server_atom_list *atom_list = xv_alloc(Server_atom_list);

		atom_list->list[0] = atom;

		atom_list_head = (Server_atom_list *) xv_get(SERVER_PUBLIC(server),
				XV_KEY_DATA, server->atom_list_head_key);
		XV_SL_ADD_AFTER(atom_list_head, atom_list_tail, atom_list);

		xv_set(SERVER_PUBLIC(server), XV_KEY_DATA,
				server->atom_list_tail_key, atom_list, NULL);
	}
	else
		atom_list_tail->list[slot] = atom;

	server->atom_list_number++;
}

static Atom server_intern_atom(Server_info *server, char *atomName, Atom at)
{
	XrmQuark quark;
	Atom atom;

	/* Convert the string into a quark.
	 * The atomName is copied and never freed.  This is acceptable
	 * since the quark can be shared between server objects.
	 */
	quark = XrmStringToQuark(atomName);

	/* See if we have an atom for this quark already */
	if (XFindContext(server->xdisplay, server->atom_mgr[ATOM],
					(XContext) quark, (caddr_t *) & atom) == XCNOENT) {

		if (at) atom = at;
		else atom = XInternAtom(server->xdisplay, atomName, False);

		/* We don't care if SaveContext fails (no mem).  It
		 * just means that FindContext will return XCNOENT and
		 * the atom will need to be interned again.
		 */
		/* Support lookup by atom name */
		(void)XSaveContext(server->xdisplay, server->atom_mgr[ATOM],
				(XContext) quark, (caddr_t) atom);

		/* Support lookup by atom value */
		(void)XSaveContext(server->xdisplay, server->atom_mgr[NAME],
				(XContext) atom, (caddr_t) strdup(atomName));

		update_atom_list(server, atom);

	}
	return ((Atom) atom);
}

static char *server_get_atom_name(Server_info *server, Atom atom)
{
	XrmQuark quark;
	char *atomName;

	if (XFindContext(server->xdisplay, server->atom_mgr[NAME], (XContext) atom,
					&atomName) == XCNOENT) {
		if (!(atomName = XGetAtomName(server->xdisplay, atom)))
			return ((char *)NULL);

		/* Convert the string into a quark */
		quark = XrmStringToQuark(atomName);

		/* Support lookup by atom name */
		(void)XSaveContext(server->xdisplay, server->atom_mgr[ATOM],
				(XContext) quark, (caddr_t) atom);

		/* Support lookup by atom value */
		(void)XSaveContext(server->xdisplay, server->atom_mgr[NAME],
				(XContext) atom, (caddr_t) atomName);

		update_atom_list(server, atom);
	}
	return ((char *)atomName);
}

static int server_set_atom_data(Server_info *server, Atom atom, Xv_opaque data)
{
    if (XSaveContext(server->xdisplay, server->atom_mgr[DATA],
			   (XContext)atom, (caddr_t) data) == XCNOMEM)
	return(XV_ERROR);
    else
	return(XV_OK);
}


static Xv_opaque server_get_atom_data(Server_info	*server, Atom atom, int *status)
{
    Xv_opaque data;

    if (XFindContext(server->xdisplay, server->atom_mgr[DATA], (XContext)atom,
		     (caddr_t *)&data) == XCNOENT)
	*status = XV_ERROR;
    else
	*status = XV_OK;

    return(data);
}

static void server_initialize_atoms(Server_info *server)
{
	char *atns[] = {
		"ATOM_PAIR",
		"BACKGROUND",
		"CLIPBOARD",
		"COLORMAP",
		"COMPOUND_TEXT",
		"DELETE",
		"DRAG_DROP",
		"DUPLICATE",
		"FILE_NAME",
		"FONT",
		"FOREGROUND",
		"FOUNDRY",
		"INCR",
		"INSERT_SELECTION",
		"LENGTH",
		"LENGTH_CHARS",
		"MOVE",
		"MULTIPLE",
		"NAME",
		"OWNER_OS",
		"HOST_NAME",
		"USER",
		"PROCESS",
		"TASK",
		"NULL",
		"PIXEL",
		"TARGETS",
		"TEXT",
		"TIMESTAMP",
		"UTF8_STRING",
		"WM_CHANGE_STATE",
		"WM_COLORMAP_WINDOWS",
		"WM_DELETE_WINDOW",
		"WM_PROTOCOLS",
		"WM_SAVE_YOURSELF",
		"WM_STATE",
		"WM_TAKE_FOCUS",
		"WM_CLIENT_LEADER",
		"XV_DO_DRAG_COPY",
		"XV_DO_DRAG_LOAD",
		"XV_DO_DRAG_MOVE",
		"XV_SELECTION_0",
		"XV_SELECTION_1",
		"XdndActionAsk",
		"XdndActionCopy",
		"XdndActionDescription",
		"XdndActionLink",
		"XdndActionList",
		"XdndActionMove",
		"XdndActionPrivate",
		"XdndAware",
		"XdndDrop",
		"XdndEnter",
		"XdndFinished",
		"XdndLeave",
		"XdndPosition",
		"XdndSelection",
		"XdndStatus",
		"XdndTypeList",
		"_DRA_ENHANCED_OLWM",
		"_DRA_DROP_REJECTED",
		"_DRA_FILE_STAT",
		"_DRA_FILE_NAME",
		"_DRA_NEXT_FILE",
		"_DRA_RESCALE",
		"_DRA_STRING_IS_FILENAMES",
		"_DRA_TALK_DEREGISTER",
		"_DRA_TALK_PATTERN",
		"_DRA_TALK_REGISTER",
		"_DRA_TALK_SERVER",
		"_DRA_TALK_TRIGGER",
		"_DRA_TRACE",
		"_MOTIF_WM_INFO",
		"_MOTIF_WM_MENU",
		"_MOTIF_WM_MESSAGES",
		"_NETSCAPE_URL",
		"_NET_WM_PING",
		"_NET_WM_WINDOW_TYPE",
		"_NET_WM_WINDOW_TYPE_DESKTOP",
		"_NET_WM_WINDOW_TYPE_DIALOG",
		"_NET_WM_WINDOW_TYPE_DOCK",
		"_NET_WM_WINDOW_TYPE_MENU",
		"_NET_WM_WINDOW_TYPE_POPUP_MENU",
		"_OL_ALLOW_ICON_SIZE",
		"_OL_COLORS_FOLLOW",
		"_OL_DECOR_ADD",
		"_OL_DECOR_CLOSE",
		"_OL_DECOR_DEL",
		"_OL_DECOR_FOOTER",
		"_OL_DECOR_HEADER",
		"_OL_DECOR_ICON_NAME",
		"_OL_DECOR_OK",
		"_OL_DECOR_PIN",
		"_OL_DECOR_RESIZE",
		"_OL_DFLT_BTN",
		"_OL_ENTER_LANG_MODE",
		"_OL_EXIT_LANG_MODE",
		"_OL_FUNC_BACK",
		"_OL_FUNC_CLOSE",
		"_OL_FUNC_FULLSIZE",
		"_OL_FUNC_PROPS",
		"_OL_FUNC_QUIT",
		"_OL_FUNC_REFRESH",
		"_OL_GROUP_MANAGED",
		"_OL_IS_WS_PROPS",
		"_OL_LABEL_HOLDER",
		"_OL_MENU_FULL",
		"_OL_MENU_LIMITED",
		"_OL_NONE",
		"_OL_NOTICE_EMANATION",
		"_OL_NO_WARPING",
		"_OL_OWN_HELP",
		"_OL_PIN_IN",
		"_OL_PIN_OUT",
		"_OL_PIN_STATE",
		"_OL_PROPAGATE_EVENT",
		"_OL_RESCALE_STATE",
		"_OL_SCALE_LARGE",
		"_OL_SCALE_MEDIUM",
		"_OL_SCALE_SMALL",
		"_OL_SCALE_XLARGE",
		"_OL_SELECTION_IS_WORD",
		"_OL_SET_WIN_MENU_DEFAULT",
		"_OL_SHOW_PROPS",
		"_OL_SHOW_SFK_WIN",
		"_OL_SOFTKEY_LABELS",
		"_OL_SOFT_KEYS_PROCESS",
		"_OL_TRANSLATED_KEY",
		"_OL_TRANSLATE_KEY",
		"_OL_WARP_BACK",
		"_OL_WARP_TO_PIN",
		"_OL_WINMSG_ERROR",
		"_OL_WINMSG_STATE",
		"_OL_WIN_ATTR",
		"_OL_WIN_BUSY",
		"_OL_WIN_COLORS",
		"_OL_WIN_DISMISS",
		"_OL_WIN_MENU_DEFAULT",
		"_OL_WM_MENU_FILE_NAME",
		"_OL_WT_BASE",
		"_OL_WT_CMD",
		"_OL_WT_HELP",
		"_OL_WT_NOTICE",
		"_OL_WT_OTHER",
		"_OL_WT_PROP",
		"_SUN_ALTERNATE_TRANSPORT_METHODS",
		"_SUN_ATM_FILE_NAME",
		"_SUN_AVAILABLE_TYPES",
		"_SUN_DATA_LABEL",
		"_SUN_DRAGDROP_ACK",
		"_SUN_DRAGDROP_DONE",
		"_SUN_DRAGDROP_DSDM",
		"_SUN_DRAGDROP_INTEREST",
		"_SUN_DRAGDROP_PREVIEW",
		"_SUN_DRAGDROP_SITE_RECTS",
		"_SUN_DRAGDROP_TRANSIENT_0",
		"_SUN_DRAGDROP_TRANSIENT_1",
		"_SUN_DRAGDROP_TRIGGER",
		"_SUN_ENUMERATION_COUNT",
		"_SUN_ENUMERATION_ITEM",
		"_SUN_EXECUTABLE",
		"_SUN_FILE_HOST_NAME",
		"_SUN_LED_MAP",
		"_SUN_LENGTH_TYPE",
		"_SUN_OLWM_NOFOCUS_WINDOW",
		"_SUN_OL_WIN_ATTR_5",
		"_SUN_QUICK_SELECTION_KEY_STATE",
		"_SUN_SELECTION_END",
		"_SUN_SELECTION_ERROR",
		"_SUN_SELN_CARET",
		"_SUN_SELN_IS_READONLY",
		"_SUN_SELN_YIELD",
		"_SUN_SELN_FIRST",
		"_SUN_SELN_LAST",
		"_SUN_WINDOW_STATE",
		"_SUN_WM_PROTOCOLS",
		"_SUN_WM_REREAD_MENU_FILE",
		"_XVIEW_V2_APP",
		"text/plain",
		"text/uri-list"
	};
	Atom *atoms;
	int i, num_names;

	/* this function takes less than a millisecond on a local connection
	 * and about 80 ms through a VPN connection.
	 */

	num_names = sizeof(atns) / sizeof(atns[0]);
	atoms = xv_alloc_n(Atom, (unsigned long)num_names);

	XInternAtoms(server->xdisplay, atns, num_names, FALSE, atoms);
	for (i = 0; i < num_names; i++) {
		server_intern_atom(server, atns[i], atoms[i]);
	}
	xv_free(atoms);
}

/*
 * BUG:  use_default_mapping should be set by comparing the default keycode
 * to keysym table.
 */
static void server_journal_sync_event(Xv_Server server_public, int type)
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

static int x_init_threads_called = 0;

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

/*
 * The following table describes the default key mappings for special 
 * XView keys. The first default key mapping is attempted. If this 
 * fails, then the machine we're on probably doesn't
 * have sufficient function keys, so we try the second mapping, and
 * so on. Right now, the rest of XView only supports the default mapping
 * so we set NUM_KEYSYM_SETS to 1 and only supply one set of keys to use.
 * In the future, this will go away and we'll provide a more elegant
 * and flexible way for the user to map function keys to XView functions.
 */

#define NUM_KEYSYM_SETS	4
#include <X11/Sunkeysym.h>

	/* XXX: XK_F13 is left here to be compatible with V2.  V2 XView
		clients use XK_F13 in the modmap as a trigger that
		tells them the function keys have already been installed.
		In V3, we had changed it such that only F18 and F20 were
		installed.  See Bug 1060242 for more details.
	 */
typedef struct {
	KeySym	ksyms[SELN_FN_NUM];
} fkey_set_t;

#define DYN_KEYSYM_SET 0
static fkey_set_t default_fkey_keysyms[NUM_KEYSYM_SETS] =
{
	{
		{ NoSymbol, NoSymbol, NoSymbol} /* determine dynamically */
	},
	{
		{ XK_F18, XK_F20, XK_F13}  /* previous setting */
	},
	{
		{ XK_F13, XK_F18, XK_F20}
	},
	{
		{ SunXK_Paste, SunXK_Cut, SunXK_Props}
	}
,};

#define MAX_RETRIES	10	/* max number of mapping retries */

static int my_sync(Display *display)
{
    XSync(display, 0);
	return 0;
}

#ifndef NULL
#define NULL 0
#endif

static Xv_opaque server_init_x(char *server_name)
{
    register Display *display;

    if (!(display = XOpenDisplay(server_name))) {
    	return ((Xv_opaque) NULL);
	}

    if (defaults_get_boolean("window.synchronous", "Window.Synchronous", FALSE)
		 			        && !XSynchronize(display, TRUE))
	(void) XSetAfterFunction(display, my_sync);

    return ((Xv_opaque) display);
}

/*
 * keycode_in_map(map, keycode)
 *
 * returns the associated modifier index if the specified keycode is in the 
 * given modifier map. Otherwise, returns -1.
 */

/* als hier noch stand 'KeyCode keycode', kam beim Aufruf noch eine 
 * Warnung 'arg 2 with different width; ...
 *
 * Wenn man genau nachschaut, ist hier ' typedef unsigned char KeyCode; '
 * ??????????
 */
static int keycode_in_map(XModifierKeymap *map, int keycode)
{
	register int i, max;

	if (!keycode) return(0);

	max = 8 * map->max_keypermod;
	for (i = 0; i < max; i++) {
		if (map->modifiermap[i] == keycode) {
			return (i / map->max_keypermod);
		}
	}
	return -1;
}

/* ich will diese Bloedwarnung wenigstens nur EINMAL sehen... */
static XModifierKeymap *insert_modifiermap_entry(XModifierKeymap *modmap,
	int keycode, /* ist jetzt KeyCode = unsigned long oder unsigned char ???? */
	int modifier)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtraditional-conversion"
	return XInsertModifiermapEntry(modmap, keycode, modifier);
#pragma GCC diagnostic pop
}

static int find_free_row(XModifierKeymap *map)
{
	int row, offset, base;

	/*
	 * Find the first unused row in the modifier map.
	 * An unused row will have all zeros in it.
	 */
	for (row = Mod1MapIndex; row <= Mod5MapIndex; row++) {
		base = row * map->max_keypermod;
		for (offset = 0; (offset < map->max_keypermod)  &&
			(map->modifiermap[base + offset] == 0); 
			offset++);
		if (offset == map->max_keypermod) {
			return(row);
		}
	}
	return(-1);
}

static KeySym unmodified_key(const char *defkey)
{
	KeySym ks;
	char *p, buf[100];

	strcpy(buf, defkey);
	ks = NoSymbol;
	/* defkey could be something like "SunPaste,Meta+v" */
	for (p = strtok(buf, ","); p; p = strtok((char *)0, ",")) {
		/* search for an unmodified key, modified keys look like a+Ctrl */
		if (strchr(p, '+') == NULL) {
			if ((ks = XStringToKeysym(p)) != NoSymbol) {
				/* hurray */
				return ks;
			}
		}
	}
	return NoSymbol;
}

static void determine_dynamic_modifier_keysyms(fkey_set_t *keyset)
{
	KeySym pasteks;
	KeySym cutks;
	KeySym propsks;
	char *defkey;

	defkey = defaults_get_string("openWindows.keyboardCommand.paste",
					"OpenWindows.KeyboardCommand.Paste", "F18");
	pasteks = unmodified_key(defkey);

	defkey = defaults_get_string("openWindows.keyboardCommand.cut",
					"OpenWindows.KeyboardCommand.Cut", "F20");
	cutks = unmodified_key(defkey);

	defkey = defaults_get_string("openWindows.keyboardCommand.props",
					"OpenWindows.KeyboardCommand.Props", "F13");
	propsks = unmodified_key(defkey);

	keyset->ksyms[0] = pasteks;
	keyset->ksyms[1] = cutks;

	/* propsks is not really important - the other two are for the 
	 * 'quick' operations
	 */
	keyset->ksyms[2] = propsks; 
}

/*
 * server_refresh_modifiers(server, update_map)
 *
 * 1) Designates the meta keys as a modifier keys.
 * 2) Inserts all the keys in the array default_fkey_keysyms[] into
 * 	the server's modifier map (all under the same modifier; any
 *	of the modifiers Mod2-Mod5 may be used). This function then
 *	sets server->quick_modmask to be the appropriate mask for whatever
 *      modifier the keys were designated as.
 * 3) If update_map is false, do not try to insert new mappings into the
 *    modifier map.  Get the current mapping and update our internal
 *    understanding only.  update_map is false when a user runs xmodmap
 *    and changes the modifier map.  We don't want to override what the
 *    user just changed, so we try to live with it.
 */

Xv_private void server_refresh_modifiers(Xv_opaque server_public, Bool update_map) /* Update the server map */
{
	Server_info *server = SERVER_PRIVATE(server_public);
	Display *display = (Display *) server->xdisplay;
	XModifierKeymap *map;
	int i, modifier, func_modifier, updated = False;
	int keysym_set, result, retry_count;

	/* try to fill default_fkey_keysyms[DYN_KEYSYM_SET] */
	determine_dynamic_modifier_keysyms(default_fkey_keysyms + DYN_KEYSYM_SET);

	for (keysym_set = 0; keysym_set < NUM_KEYSYM_SETS; keysym_set++) {
		fkey_set_t *keyset = default_fkey_keysyms + keysym_set;
		KeySym *ksyms = keyset->ksyms;

		if (!(map = XGetModifierMapping(display))) {
			return;
		}

		/* See if META is already installed. */
		if ((modifier = keycode_in_map(map,
								XKeysymToKeycode(display,
										(KeySym) XK_Meta_L))) == -1) {
			/* Find a free row for META */
			if (update_map && (modifier = find_free_row(map)) != -1) {
				updated = True;
				/* Insert the meta keys as modifiers. */
				map = insert_modifiermap_entry(map,
						XKeysymToKeycode(display, (KeySym) XK_Meta_L),
						modifier);
				map = insert_modifiermap_entry(map,
						XKeysymToKeycode(display, (KeySym) XK_Meta_R),
						modifier);
			}
		}
		if (modifier == -1 || modifier == 0)
			server->meta_modmask = 0;
		else
			server->meta_modmask = 1 << modifier;

		/* See if NUM LOCK is already installed. */
		if ((modifier = keycode_in_map(map,
								XKeysymToKeycode(display,
										(KeySym) XK_Num_Lock))) == -1) {
			/* Find a free row for NUM LOCK */
			if (update_map && (modifier = find_free_row(map)) != -1) {
				updated = True;
				/* Insert the meta keys as modifiers. */
				map = insert_modifiermap_entry(map,
						XKeysymToKeycode(display, (KeySym) XK_Num_Lock),
						modifier);
			}
		}
		if (modifier == -1 || modifier == 0)
			server->num_lock_modmask = 0;
		else
			server->num_lock_modmask = 1 << modifier;

		if (defaults_get_enum("openWindows.keyboardCommands",
						"OpenWindows.KeyboardCommands",
						xv_kbd_cmds_value_pairs) >= KBD_CMDS_BASIC) {
			/* See if ALT is already installed. */
			if ((modifier = keycode_in_map(map,
									XKeysymToKeycode(display,
											(KeySym) XK_Alt_L))) == -1) {
				/* Find a free row for ALT */
				if (update_map && (modifier = find_free_row(map)) != -1) {
					updated = True;
					/* Insert the alt keys as modifiers. */
					map = insert_modifiermap_entry(map,
							XKeysymToKeycode(display, (KeySym) XK_Alt_L),
							modifier);
					map = insert_modifiermap_entry(map,
							XKeysymToKeycode(display, (KeySym) XK_Alt_R),
							modifier);
				}
			}
			if (modifier == -1 || modifier == 0)
				server->alt_modmask = 0;
			else
				server->alt_modmask = 1 << modifier;
		}

		/* See if function keys in map */
		if (((func_modifier = keycode_in_map(map,
					XKeysymToKeycode(display, (KeySym)ksyms[0]))) == -1)
				|| ((func_modifier = keycode_in_map(map,
						XKeysymToKeycode(display, (KeySym)ksyms[1]))) == -1)) {
			/* Find a free row. */
			if (update_map && (func_modifier = find_free_row(map)) != -1) {
				for (i = 0; i < SELN_FN_NUM; i++) {
					updated = True;
					map = insert_modifiermap_entry(map,
							XKeysymToKeycode(display, (KeySym)ksyms[i]),
							func_modifier);
				}
				server->quick_modmask = 1 << func_modifier;
			}
		}
		else {
			server->quick_modmask = 1 << func_modifier;
		}

		if (func_modifier == -1 || func_modifier == 0)	/* no free rows */
			server->quick_modmask = 0;

		/*
		 * Attempt to install the modifier mapping.
		 * If successful, exit this function. If not, try another 
		 * set of keysyms.
		 */
		if (updated) {
			for (retry_count = 0; ((result = XSetModifierMapping(display, map))
							== MappingBusy && retry_count < MAX_RETRIES);
					retry_count++) {
				sleep(1);	/* if busy, wait 1 sec and retry */
			}
			if (result == Success) {
				XFreeModifiermap(map);
				return;
			}
		}
		else {
			XFreeModifiermap(map);
			return;
		}
	}

	/* all our attempts failed */
	xv_error(XV_NULL,
			ERROR_STRING, XV_MSG("Problems setting default modifier mapping"),
			ERROR_PKG, SERVER,
			NULL);

	XFreeModifiermap(map);
}


Xv_private void server_set_seln_function_pending(Xv_Server server_public, int flag)
{
    Server_info    *server = SERVER_PRIVATE(server_public);
    server->sel_function_pending = flag ? TRUE : FALSE;
}

Xv_private int server_get_seln_function_pending(Xv_Server server_public)
{
    return (SERVER_PRIVATE(server_public)->sel_function_pending);
}

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

static int xvwp_is_own_help(Xv_server srv, Frame fr, Event *ev)
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
			int rx, ry;
			char hlpbuf[30];

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtraditional-conversion"
			rx = ntohs((uint16_t)xcl->data.s[0]);
			ry = ntohs((uint16_t)xcl->data.s[1]);
#pragma GCC diagnostic pop
			event_init(&my_event);
			event_set_action(&my_event, ACTION_HELP);
			event_set_xevent(&my_event, event_xevent(ev));
			event_set_x(&my_event, rx);
			event_set_y(&my_event, ry);
			sprintf(hlpbuf, "olwm:%s", xcl->data.b + 4);

			xv_help_show(fr, hlpbuf, &my_event);
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
			if (xvwp_is_own_help(srv, fram, ev)) return NOTIFY_DONE;
		}
		if (xcl->message_type == W_ATOM(fram, _OL_OWN_HELP)) {
			if (xvwp_is_own_help(srv, fram, ev)) return NOTIFY_DONE;
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

static void xvwp_init(Server_info *srv, char **argv)
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
static void server_note_register_ui(Xv_server srv, Xv_opaque obj, const char *name)
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

static void xvwp_connect(Xv_server srv, char *base_inst_name)
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

static void xvwp_install(Frame base)
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

static void xvwp_configure_popup(Frame popup)
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
		xvwp_configure_popup(popup);
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

static void server_show_propwin(Server_info *srvpriv)
{
	xvwp_t *inst = srvpriv->xvwp;

	popup_propwin(inst, xv_get(inst->propwin, XV_OWNER));
}

static void server_appl_set_busy(Server_info *srvpriv, int busy, Frame except)
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
	if (xvwp_is_own_help(XV_SERVER_FROM_WINDOW(f), f, (Event *)nev))
		return NOTIFY_DONE;

	return notify_next_event_func(f, nev, arg, type);
}

static void server_register_secondary_base(Xv_server srv, Frame sec,
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
			case SERVER_SUPPORT_MULTI_THREAD:
				if (attrs[1]) call_real_XInitThreads();
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
	server->xfixEventBase = -1;

	server->display_name = xv_strsave(server_name ? server_name : ":0");

	xvwp_init(server, argv);

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
 * Also: sollen die Multithreadprogramme doch XInitThreads aufrufen......
 *
 * In Wirklichkeit ist meine Loesung hier doch nur ein Hack, um um die
 * Bloedheit der Xlib-Fritzen herumzuprogrammieren. Die rufen XInitThreads
 * auf und bremsen damit single thread Programme aus....
 */

Status XInitThreads(void)
{
	char *env = getenv("XVIEW_TELL_XINITTHREADS");

	if (env && *env) {
		fprintf(stderr, "XInitThreads called in %s\n", xv_app_name);
	}
	x_init_threads_called = 1;
	return 0;
}

/********************************************************************
 The real XInitThreads performs a lot of initializations, especially
 registers _XInitDisplayLock in a variable _XInitDisplayLock_fn.
 _XInitDisplayLock_fn in turn allocates a lot of things to the display,
 especially dpy->lock_fns and dpy->lock. Those fields would be used
 in LockDisplay etc.... , so, my hope is that all that locking and
 thread handling will not be called....

 In fact, dpy->lock_fns and dpy->lock turned out to be nil...
 ********************************************************************/

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

static void server_host_name(Xv_server server, char *buf)
{
	char buffer[500];
	char *p, *host, *snam;
	int alldigits;

	/* Wird in der vnc-Welt gesetzt (vnc_from_remote) */
	host = getenv("DRA_XV_SERVER_HOST");
	if (host && *host) {
		strcpy(buf, host);
		return;
	}

	/* Wird in /usr/openwin/bin/openwin gesetzt und
	 * per AcceptEnv auch auf die andere ssh-Seite uebertragen
	 */
	host = getenv("OL_DISPLAY_HOST");
	if (host && *host) {
		strcpy(buf, host);
		return;
	}

	snam = (char *)xv_get(server, XV_NAME);

	if ((snam && *snam) || ((snam = (char *)getenv("DISPLAY")) && *snam)) {
		snam = xv_strsave(snam);

		if (*snam == ':' || *snam == '.') {
			xv_free(snam);
			snam = (char *)0;
		}
		else if (! strncmp(snam, "unix:", 5L)) {
			xv_free(snam);
			snam = (char *)0;
		}
	}

	if (snam) {
		struct hostent *he;
		char *pp;

		/* now snam can look like:
		 *      gwely:0
		 * or   gwely.lluched.vpn:0
		 * or   192.168.84.5:0
		 */
		strcpy(buffer, snam);
		pp = strtok(buffer, ":");
		if (pp) {
			he = gethostbyname(pp);
		}
		else {
			he = gethostbyname(snam);
		}

		if (he) {
			xv_free(snam);
			snam = xv_strsave(he->h_name);
		}
	}
	else {
		struct utsname name;

		if (uname(&name) == -1) {
			snam = xv_strsave("@unknown");
		}
		else snam = xv_strsave(name.nodename);
	}

	strcpy(buffer, snam);
	host = strtok(buffer, ".:");

	alldigits = TRUE;
	for (p = host; *p; p++) {
		if (! isdigit(*p)) {
			alldigits = FALSE;
		}
	}

	if (alldigits) {
		/* probably something like 192.168.84.5 */
		strcpy(buf, snam);
		return;
	}

	if (host) strcpy(buf, host);
	else strcpy(buf, "@unknown");

	xv_free(snam);

	if (0 == strcmp(buf, "localhost")) {
		char *sshc = getenv("SSH_CLIENT");

		/* 		env: SSH_CLIENT=192.168.184.2 35621 7123 */
		/* 		env: SSH_CONNECTION=192.168.184.2 35621 192.168.184.1 7123 */
		if (!sshc || !*sshc) sshc = getenv("SSH_CONNECTION");
		if (sshc && *sshc) {
			char hostbuf[100];
			struct hostent *hp;

			strcpy(hostbuf, sshc);
			sshc = strtok(hostbuf, " ");
			if ((hp = gethostbyname(sshc))) {
				struct sockaddr_in server;
				char myhost[100];

				server.sin_port = 0;
				server.sin_family = AF_INET;
				memcpy((char *)&server.sin_addr, (char *)hp->h_addr,
											(size_t)hp->h_length);

				if (! getnameinfo((struct sockaddr *)&server,
							(unsigned)sizeof(server),
							myhost, (unsigned)sizeof(myhost),
							NULL, 0, NI_NAMEREQD | NI_NOFQDN))
				{
					char *p = strchr(myhost, '.');
					if (p) *p = '\0';

					strcpy(buf, myhost);
				}
			}
		}
	}
}

static Xv_font try_one_family(char **l0, char **l1, char **l2, char **l3,
		int l0cnt, char *fam, char *weight, int slant, int size, int spacing)
{
	Xv_font font;
	char buf[1000], *p;
	int i;
	size_t len = strlen(fam);

	for (i = 0; i < l0cnt; i++) {
		if (!(p = strchr(l0[i] + 1, '-'))) continue;
		if (strncmp(p+1, fam, len)) continue;
		sprintf(buf, "-*-%s-%s-%c-*-*-*-%d-*-*-%c-*-iso10646-1",
							fam, weight, slant, size, spacing);

		if (!(font = xv_find(XV_NULL, FONT, FONT_NAME, buf, NULL))) continue;

		if (l0) XFreeFontNames(l0);
		if (l1) XFreeFontNames(l1);
		if (l2) XFreeFontNames(l2);
		if (l3) XFreeFontNames(l3);
		return font;
	}

	return (Xv_font)0;
}

static Xv_font find_font_with_preferences(Display *dpy, char *weight,
											int slant, int size, ...)
{
	va_list ap;
	Xv_font font;
	char *name, buf[1000], **lmsiz, **lm0, **lcsiz, **lc0;
	int lmsizcnt, lm0cnt, lcsizcnt, lc0cnt;

	sprintf(buf, "-*-*-%s-%c-*-*-*-%d-*-*-m-*-iso10646-1", weight, slant, size);
	lmsiz = XListFonts(dpy, buf, 1000, &lmsizcnt);
	sprintf(buf, "-*-*-%s-%c-*-*-*-0-*-*-m-*-iso10646-1", weight, slant);
	lm0 = XListFonts(dpy, buf, 1000, &lm0cnt);
	sprintf(buf, "-*-*-%s-%c-*-*-*-%d-*-*-c-*-iso10646-1", weight, slant, size);
	lcsiz = XListFonts(dpy, buf, 1000, &lcsizcnt);
	sprintf(buf, "-*-*-%s-%c-*-*-*-0-*-*-c-*-iso10646-1", weight, slant);
	lc0 = XListFonts(dpy, buf, 1000, &lc0cnt);

	va_start(ap, size);
	while ((name = va_arg(ap, char *))) {
		if ((font = try_one_family(lmsiz, lm0, lcsiz, lc0, lmsizcnt, name,
							weight, slant, size, 'm'))) return font;

		if ((font = try_one_family(lm0, lcsiz, lc0, lmsiz, lm0cnt, name,
							weight, slant, size, 'm'))) return font;

		if ((font = try_one_family(lcsiz, lc0, lmsiz, lm0, lcsizcnt, name,
							weight, slant, size, 'c'))) return font;

		if ((font = try_one_family(lc0, lmsiz, lm0, lcsiz, lc0cnt, name,
							weight, slant, size, 'c'))) return font;

	}
	va_end(ap);
	return (Xv_font)0;
}

static Xv_font server_load_fixedwidth_font(Xv_server server, char *instnam)
{
	Xv_font font;
	char buf[1000], *fontname;
	Xv_opaque *dbs = (Xv_opaque *)xv_get(server, SERVER_RESOURCE_DATABASES);

	sprintf(buf, "%s.font_name", instnam);
	fontname = Permprop_res_get_string(dbs[(int)PRC_D], buf,
							"-*-courier-bold-r-*-*-*-140-*-*-*-*-iso10646-1");

	font = xv_find(XV_NULL, FONT, FONT_NAME, fontname, NULL);

	if (! font)
		font = xv_find(XV_NULL, FONT, FONT_NAME, "screen-bold-14", NULL);

	if (! font) 
		font = find_font_with_preferences((Display *)xv_get(server, XV_DISPLAY),
							"bold", 'r', 140,
							"lucida sans typewriter",
							"lucidatypewriter",
							"courier",
							"terminal",
							"clean",
							NULL);

	if (!font) {
		fprintf(stderr, "%s 'fixed'\n",XV_MSG("instead: trying to load font"));
		if (!(font = xv_find(XV_NULL, FONT, FONT_NAME, "fixed", NULL))) {
			fprintf(stderr, XV_MSG("no font"));
			fprintf(stderr, "\n");
			exit(1);
		}
	}
	
	return font;
}

static void update_resources(Server_info *server, char *xdef)
{
	extern XrmDatabase defaults_rdb;

	server->db = XrmGetStringDatabase(xdef);
	XrmMergeDatabases(server->db, &defaults_rdb);
	server->db = defaults_rdb;
}

static void intern_atoms(Server_info *server, char **names)
{
	Atom *atoms;
	int i, num_names;

	for (num_names = 0; names[num_names]; num_names++);
	atoms = xv_alloc_n(Atom, (unsigned long)num_names);

	XInternAtoms(server->xdisplay, names, num_names, FALSE, atoms);
	for (i = 0; i < num_names; i++) {
		server_intern_atom(server, names[i], atoms[i]);
	}
	xv_free(atoms);
}

static int lock_global_X_resource(Xv_server srv, const char *atomname,
									Window xid)
{
	typedef struct {
		unsigned long pid;
		unsigned long xid;
	} xlockinfo_t;
	xlockinfo_t lock, *dt;
	int pid = (int)getpid();
	Display *dpy = (Display *)xv_get(srv, XV_DISPLAY);
	Window root = DefaultRootWindow(dpy);
	Atom at = xv_get(srv, SERVER_ATOM, atomname);
	Atom typ = xv_get(srv, SERVER_ATOM, "XLOCKINFO");
	int format, succ;
	unsigned long len, rest;
	Atom acttype;

	/* the idea is as follows: we use a property of type XLOCKINFO, format 32
	 * on the default root window of the display.
	 * Every program that wants to lock something chooses an atom name and
	 * calls this function handing it a window ID of an own window.
	 * Then this function APPENDS an xlockinfo_t and afterwards reads the
	 * property. If the first xlockinfo contains the own data, the lock was
	 * successful.
	 */

	lock.pid = pid;
	lock.xid = xid;

	XChangeProperty(dpy, root, at, typ, 32, PropModeAppend, 
				(unsigned char *)&lock, (int)(sizeof(lock)/sizeof(lock.pid)));

	/* now the X server has serialized simultaneous requests! 
	 * We read the first xlockinfo_t and check whether it conatins OUR OWN
	 * data;
	 */

	succ = XGetWindowProperty(dpy, root, at, 0L, sizeof(lock)/sizeof(lock.pid),
				FALSE, typ, &acttype, &format, &len, &rest,
				(unsigned char **)&dt);
	if (succ == Success && acttype == typ && format == 32) {
		lock = *dt;
		XFree(dt);
		if (lock.pid == pid && lock.xid == xid) return TRUE;
	}
	return FALSE;
}

static void unlock_global_X_resource(Xv_server srv, const char *atomname)
{
	Display *dpy = (Display *)xv_get(srv, XV_DISPLAY);
	Window root = DefaultRootWindow(dpy);
	Atom at = xv_get(srv, SERVER_ATOM, atomname);

	XDeleteProperty(dpy, root, at);
}

static Server_proc_list *server_procnode_from_id(Server_info *server,
														Xv_opaque pkg_id)
{
	Server_proc_list *node = 0;

	XV_SL_TYPED_FOR_ALL(server->idproclist, node, Server_proc_list *)
	{
		if (node->id == pkg_id)
		break;
	}

	return node;
}

static int server_add_xevent_proc(Server_info *server, Xv_opaque func,
												Xv_opaque pkg_id, int external)
{
	int error = XV_OK;
	Server_proc_list *node = 0;
	Server_xid_list *xid_node;
	Server_mask_list *mask_node;

	node = (Server_proc_list *) server_procnode_from_id(server, pkg_id);

	if (!node) {
		node = (Server_proc_list *) xv_alloc(Server_proc_list);
		node->id = pkg_id;

		/* add entry at the beginning of the list */
		server->idproclist = (Server_proc_list *)
				XV_SL_ADD_AFTER(server->idproclist, XV_SL_NULL, node);

		/*if mask node exists, link them back to this node */

		XV_SL_TYPED_FOR_ALL(server->xidlist, xid_node, Server_xid_list *) {
			XV_SL_TYPED_FOR_ALL(xid_node->masklist, mask_node,
					Server_mask_list *) {
				if (mask_node->id == pkg_id)
					mask_node->proc = node;
			}
		}
	}
	if (external)
		node->extXeventProc = (server_extension_proc_t)func;
	else
		node->pvtXeventProc = (server_extension_proc_t)func;

#ifdef _XV_DEBUG
	p(server);
#endif /* _XV_DEBUG */

	return error;
}

static Server_xid_list * server_xidnode_from_xid(Server_info *server,
														Xv_opaque xid)
{
	Server_xid_list *node = 0;

	XV_SL_TYPED_FOR_ALL(server->xidlist, node, Server_xid_list *)
	{
		if (node->xid == xid)
		break;
	}

	return node;
}

static Server_mask_list *server_masknode_from_xidid(Server_info *server,
												Xv_opaque xid, Xv_opaque pkg_id)
{
	Server_xid_list *xid_node = 0;
	Server_mask_list *node = 0;

	if ((xid_node = server_xidnode_from_xid(server, xid)))
	XV_SL_TYPED_FOR_ALL(xid_node->masklist, node, Server_mask_list *)
	{
		if (node->id == pkg_id)
		break;
	}

	return node;
}

static int server_add_xevent_mask(Server_info *server, Xv_opaque xid,
							Xv_opaque	mask, Xv_opaque pkg_id, int external)
/* called by app or xview pkg */
{
	int error = XV_OK;
	Server_xid_list *xid_node = 0;
	Server_mask_list *mask_node = 0, *link;

	if ((xid_node = server_xidnode_from_xid(server, xid)))
		mask_node = server_masknode_from_xidid(server, xid, (Xv_opaque) pkg_id);

	if (!mask) {	/* mask is null.  remove node is necessary */

		if (!mask_node)
			return error;

		if (external)
			mask_node->extmask = mask;
		else
			mask_node->pvtmask = mask;

		if (!(mask_node->pvtmask | mask_node->extmask)) {
			/* both masks are null, remove the node */

			if (xid_node->masklist == mask_node)	/* first node */
				xid_node->masklist = (Server_mask_list *) mask_node->next;
			else
				XV_SL_REMOVE(xid_node->masklist, mask_node);

			xv_free(mask_node);
		}

		/* compute new mask */
		XV_SL_TYPED_FOR_ALL(xid_node->masklist, link, Server_mask_list *) {
			mask |= link->extmask | link->pvtmask;
		}

		/* send new mask to server */
		XSelectInput(server->xdisplay, xid, (long)mask);

		/* get rid of xid node */

		if (!xid_node->masklist) {

			if (server->xidlist == xid_node)	/* first node */
				server->xidlist = (Server_xid_list *) xid_node->next;
			else
				XV_SL_REMOVE(server->xidlist, xid_node);

			xv_free(xid_node);
		}

	}
	else {

		if (!xid_node) {	/* create an xid entry */
			xid_node = (Server_xid_list *) xv_alloc(Server_xid_list);
			xid_node->xid = xid;
			server->xidlist = (Server_xid_list *)
					XV_SL_ADD_AFTER(server->xidlist, XV_SL_NULL, xid_node);
		}

		if (!mask_node) {	/* create a mask entry */
			mask_node = (Server_mask_list *) xv_alloc(Server_mask_list);
			mask_node->id = pkg_id;
			mask_node->proc = server_procnode_from_id(server,(Xv_opaque)pkg_id);
			xid_node->masklist = (Server_mask_list *)
					XV_SL_ADD_AFTER(xid_node->masklist, XV_SL_NULL, mask_node);
		}

		if ((external & (mask_node->extmask != mask)) |
				(!external & (mask_node->pvtmask != mask))) {

			if (external)
				mask_node->extmask = mask;
			else
				mask_node->pvtmask = mask;

			/* compute new mask */
			XV_SL_TYPED_FOR_ALL(xid_node->masklist, link, Server_mask_list *) {
				mask |= link->extmask | link->pvtmask;
			}

			/* send new mask to server */
			XSelectInput(server->xdisplay, xid, (long)mask);
		}
	}

#ifdef _XV_DEBUG
	p(server);
#endif /* _XV_DEBUG */

	return error;
}

/* ACC_XVIEW */
static void server_semantic_map_offset(Xv_server server_public,
							unsigned int	modifiers, int *offset)
{
	unsigned int	alt_modmask, meta_modmask;

	/*
	 * Get Meta, Alt masks
	 */
	meta_modmask = (unsigned int)xv_get(server_public, 
						SERVER_META_MOD_MASK);
	alt_modmask = (unsigned int)xv_get(server_public, 
						SERVER_ALT_MOD_MASK);

	*offset = 0;

	/*
	 * Determine offsets into the semantic mapping tables.
	 */
	if (modifiers & ControlMask)
		*offset += 0x100;
	if (modifiers & meta_modmask)
		*offset += 0x200;
	if (modifiers & alt_modmask)
		*offset += 0x400;
	if (modifiers & ShiftMask)
		*offset += 0x800;
}
/* ACC_XVIEW */

static Xv_opaque server_set_avlist(Xv_Server self, Attr_attribute *avlist)
{
	Attr_avlist attrs;
	Server_info *server = SERVER_PRIVATE(self);
	short error = XV_OK;

	for (attrs = avlist; *attrs; attrs = attr_next(attrs)) {
		switch (attrs[0]) {
			case SERVER_NTH_SCREEN:{
					int number = (int)attrs[1];
					Xv_Screen screen = (Xv_Screen) attrs[2];

					if ((number < 0) || (number >= MAX_SCREENS)) {
						error = (Xv_opaque) attrs[0];
						break;	/* parse the other attributes */
					}

					/*
					 * destroy the old screen if overwriting, unless new screen
					 * is null, in which case caller must already have destroyed
					 * old.
					 */
					if (server->screens[number] != screen) {
						if (screen && server->screens[number])
							(void)xv_destroy(server->screens[number]);
						server->screens[number] = screen;
					}
				}
				ATTR_CONSUME(*attrs);
				break;
			case SERVER_SYNC:
				XSync((Display *) server->xdisplay, (int)attrs[1]);
				ATTR_CONSUME(*attrs);
				break;
			case SERVER_SYNC_AND_PROCESS_EVENTS:
				{
					/*
					 * sync with the server to make sure we have all outstanding
					 * events in the queue. Then process the events.
					 */
					Display *display = (Display *) server->xdisplay;

					XSync(display, 0);
					xv_input_pending(display, 0);	/* process pending queued events */
				}
				ATTR_CONSUME(*attrs);
				break;
			case SERVER_JOURNAL_SYNC_ATOM:
				server->journalling_atom = (Xv_opaque) attrs[1];
				ATTR_CONSUME(*attrs);
				break;
			case SERVER_JOURNAL_SYNC_EVENT:
				if (server->journalling)
					server_journal_sync_event(self, (int)attrs[1]);
				ATTR_CONSUME(*attrs);
				break;
			case SERVER_UI_REGISTRATION_PROC:
				server->ui_reg_proc = (server_ui_registration_proc_t)attrs[1];
				ATTR_CONSUME(*attrs);
				break;
			case SERVER_TRACE_PROC:
				server->trace_proc = (server_trace_proc_t)attrs[1];
				ATTR_CONSUME(*attrs);
				break;

			case SERVER_JOURNALLING:
				server->journalling = (int)attrs[1];
				SERVERTRACE((44, "setting SERVER_JOURNALLING = %d\n",
											server->journalling));
				ATTR_CONSUME(*attrs);
				break;
			case SERVER_MOUSE_BUTTONS:
				server->nbuttons = (short)attrs[1];
				ATTR_CONSUME(*attrs);
				break;
			case SERVER_BUTTON2_MOD:
				server->but_two_mod = (unsigned int)attrs[1];
				ATTR_CONSUME(*attrs);
				break;
			case SERVER_BUTTON3_MOD:
				server->but_three_mod = (unsigned int)attrs[1];
				ATTR_CONSUME(*attrs);
				break;
			case SERVER_CHORD_MENU:
				server->chord_menu = (unsigned int)attrs[1];
				ATTR_CONSUME(*attrs);
				break;
			case SERVER_CHORDING_TIMEOUT:
				server->chording_timeout = (int)attrs[1];
				ATTR_CONSUME(*attrs);
				break;
			case SERVER_EXTENSION_PROC:
				server->extensionProc = (server_extension_proc_t)attrs[1];
				ATTR_CONSUME(*attrs);
				break;
			case XV_NAME:
				server->display_name = (char *)attrs[1];
				ATTR_CONSUME(*attrs);
				break;
			case SERVER_DND_ACK_KEY:
				server->dnd_ack_key = (int)attrs[1];
				ATTR_CONSUME(*attrs);
				break;
			case SERVER_ATOM_DATA:
				error = server_set_atom_data(server, (Atom) attrs[1],
						(Xv_opaque) attrs[2]);
				ATTR_CONSUME(*attrs);
				break;
			case SERVER_FOCUS_TIMESTAMP:
				server->focus_timestamp = (unsigned long)attrs[1];
				ATTR_CONSUME(*attrs);
				break;
			case SERVER_EXTERNAL_XEVENT_PROC:
				error = server_add_xevent_proc(server, attrs[1], (Xv_opaque)attrs[2],
						TRUE);
				ATTR_CONSUME(*attrs);
				break;
			case SERVER_PRIVATE_XEVENT_PROC:
				error = server_add_xevent_proc(server, attrs[1], (Xv_opaque)attrs[2],
						FALSE);
				ATTR_CONSUME(*attrs);
				break;
			case SERVER_EXTERNAL_XEVENT_MASK:
				error = server_add_xevent_mask(server, attrs[1], attrs[2],
						(Xv_opaque)attrs[3], TRUE);
				ATTR_CONSUME(*attrs);
				break;
			case SERVER_PRIVATE_XEVENT_MASK:
				error = server_add_xevent_mask(server, attrs[1], attrs[2],
						(Xv_opaque)attrs[3], FALSE);
				ATTR_CONSUME(*attrs);
				break;

#ifdef OW_I18N
			case XV_APP_NAME:
				_xv_set_mbs_attr_dup(&server->app_name_string,
						(char *)attrs[1]);
				break;
			case XV_APP_NAME_WCS:
				_xv_set_wcs_attr_dup(&server->app_name_string,
						(wchar_t *) attrs[1]);
				break;
#else
			case XV_APP_NAME:
				server->app_name_string = xv_strsave((char *)attrs[1]);
				ATTR_CONSUME(*attrs);
				break;
#endif /*OW_I18N */

				/* ACC_XVIEW */
			case SERVER_ADD_ACCELERATOR_MAP:{
					KeySym keysym = (KeySym) attrs[1];
					unsigned int modifiers = (unsigned int)attrs[2];
					int offset;

					/*
					 * If accelerator map not created yet, do it now
					 */
					if (!server->acc_map) {
						server->acc_map =
								(unsigned char *)xv_calloc(0x1600,
								(unsigned)sizeof(unsigned char));
					}

					/*
					 * Determine offsets into the semantic mapping tables.
					 */
					server_semantic_map_offset(self, modifiers,
							&offset);

					/*
					 * Increment ref count in accelerator map
					 */
					++(server->acc_map[(keysym & 0xFF) + offset]);
					break;
				}

			case SERVER_REMOVE_ACCELERATOR_MAP:{
					KeySym keysym = (KeySym) attrs[1];
					unsigned int modifiers = (unsigned int)attrs[2];
					int offset;

					/*
					 * Break if no accelerator map
					 */
					if (!server->acc_map) {
						break;
					}

					/*
					 * Determine offsets into the semantic mapping tables.
					 */
					server_semantic_map_offset(self, modifiers,
							&offset);

					/*
					 * Remove/decrement entry in server accelerator map
					 * if it is not zero
					 */
					if (server->acc_map &&
							server->acc_map[(keysym & 0xFF) + offset]) {
						--(server->acc_map[(keysym & 0xFF) + offset]);
					}

					break;
				}
				/* ACC_XVIEW */

			case XV_APP_HELP_FILE:
				if (server->app_help_file) xv_free(server->app_help_file);
				server->app_help_file = xv_strsave((char *)attrs[1]);
				ATTR_CONSUME(*attrs);
				break;

			case XV_WANT_ROWS_AND_COLUMNS:
				server->want_rows_and_columns = (int)attrs[1];
				ATTR_CONSUME(*attrs);
				break;

			case SERVER_WRITE_RESOURCE_DATABASES:
				internal_write_file(server->xvwp);
				ATTR_CONSUME(*attrs);
				break;

			case SERVER_BASE_FRAME:
				xvwp_install((Frame)attrs[1]);
				ATTR_CONSUME(*attrs);
				break;

			case SERVER_SHOW_PROPWIN:
				server_show_propwin(server);
				ATTR_CONSUME(*attrs);
				break;

			case SERVER_APPL_BUSY:
				server_appl_set_busy(server, (int)attrs[1], (Frame)attrs[2]);
				ATTR_CONSUME(*attrs);
				break;

			case SERVER_REGISTER_SECONDARY_BASE:
				server_register_secondary_base(self,
											(Frame)attrs[1], (Frame)attrs[2]);
				ATTR_CONSUME(*attrs);
				break;


			case XV_END_CREATE:
				if (! server->app_name_string) {
					extern char	*xv_app_name;

					server->app_name_string = xv_strsave(xv_app_name);
				}
				xvwp_connect(self, "base");
				break;

			case SERVER_UPDATE_RESOURCE_DATABASES:
				update_resources(server, (char *)attrs[1]);
				ATTR_CONSUME(*attrs);
				break;

			case SERVER_REPAINT_APPLICATION:
				win_repaint_application(server->xdisplay);
				ATTR_CONSUME(*attrs);
				break;

			case SERVER_INTERN_ATOMS:
				intern_atoms(server, (char **)(attrs + 1));
				ATTR_CONSUME(*attrs);
				break;

			case SERVER_LOCK:
				if (! lock_global_X_resource(self,(char *)attrs[1],
											(Window)attrs[2]))
				{
					error = XV_ERROR;
				}
				ATTR_CONSUME(*attrs);
				break;

			case SERVER_UNLOCK:
				unlock_global_X_resource(self, (char *)attrs[1]);
				ATTR_CONSUME(*attrs);
				break;

			default:
				(void)xv_check_bad_attr(SERVER, (Attr_attribute) attrs[0]);
				break;
		}
	}

	return (Xv_opaque) error;
}

/* you can use this function in the form
 *
 * server_set_timestamp(server, &timval, 0L)
 *
 * to get the current server time as a struct timeval
 */
Xv_private void server_set_timestamp(Xv_Server server_public,
						struct timeval *ev_time, unsigned long xtime)
{
	Server_info *server = SERVER_PRIVATE(server_public);

	/* avoid a backward running clock ... */
	if (xtime > server->xtime) server->xtime = (Xv_opaque) xtime;

	/* Set the time stamp in the event struct */
	if (ev_time) {
		ev_time->tv_sec = ((unsigned long)server->xtime)/1000;
		ev_time->tv_usec = (((unsigned long) server->xtime) % 1000) * 1000;
	}
}

Xv_private void server_set_fullscreen(Xv_Server server_public,int in_fullscreen)
{
	Server_info *server = SERVER_PRIVATE(server_public);
	server->in_fullscreen = in_fullscreen;
}

/* The set of below named functions:
 *
 *        server_*node_from_*,
 * 	  server_add_xevent_*,
 *	  server_do_xevent_callback,
 *       
 * together implement the support X event callback mechanism.
 *
 * Server_info *server->idproclist points to a linked list of entries
 * 	each entry containing an id (=0 for app, =a unique handle otherwise)
 *      and a pointer to a callback function.
 * 
 * Server_info *server->xidlist points a linked list of entries
 * 	each entry containing a window xid, and a pointer (->idmasklist) to a
 *	to linked list of (Server_mask_list *) described next.  
 * 
 * Server_info *server->xidlist->idmasklist  points a linked list of entries
 * 	each entry containing a id (=0 for app, =a unique value otherwise),
 *      an event mask, and a pointer to an item in the first linked list,
 *	namely, server->idproclist.
 *
 * All linked lists are created and used using XV_SL_* macros.  
 * Important note: There is no explicit head for each list.  The first entry
 * in the list, if one exists, is the head.
 *  
 */

#ifdef _XV_DEBUG

static print_struct(Server_info *s)
{
	Server_xid_list *xid_node;
	Server_mask_list *mask_node;
	Server_proc_list *proc_node;

	printf ("idproclist=%d, xidlist=%x\n", s->idproclist, s->xidlist);

	XV_SL_TYPED_FOR_ALL(s->xidlist, xid_node, Server_xid_list *) {
	printf ("xidnode=%d ", xid_node);
	XV_SL_TYPED_FOR_ALL(xid_node->masklist, mask_node, Server_mask_list *)
		printf ("masknode=%d ", mask_node);
	printf ("\n");
	}
}

#endif /* _XV_DEBUG */

Xv_private 	void server_do_xevent_callback(Server_info *server,
								Display *display, XEvent	*xevent)
{
	XAnyEvent *any = (XAnyEvent *) xevent;
	Server_xid_list *xid_node;
	Server_mask_list *mask_node;
	Server_proc_list *proc_node;

	/* get the server object */

#ifdef _XV_DEBUG
	printf("callback:\n");
	p(server);
#endif /* _XV_DEBUG */

	XV_SL_TYPED_FOR_ALL(server->xidlist, xid_node, Server_xid_list *) {

		if (xid_node->xid == any->window) {

			XV_SL_TYPED_FOR_ALL(xid_node->masklist,mask_node,Server_mask_list *)
			{
				proc_node = mask_node->proc;
				if (proc_node && proc_node->extXeventProc)
					proc_node->extXeventProc(SERVER_PUBLIC(server), display,
												xevent, proc_node->id);
				if (proc_node && proc_node->pvtXeventProc)
					proc_node->pvtXeventProc(SERVER_PUBLIC(server), display,
												xevent, proc_node->id);
			}
			break;	/* found a matching xid, get out of the first loop */
		}
	}
}

static Xv_opaque server_get_attr_tier2(Xv_Server server_public, int *status, Attr_attribute attr, va_list valist)
{
	register Server_info *server = SERVER_PRIVATE(server_public);
	Xv_opaque result = 0;

	switch (attr) {
		case SERVER_NTH_SCREEN:{
				register int number = va_arg(valist, int);

				if ((number < 0) || (number >= MAX_SCREENS)) {
					goto Error;
				}
				/* create the screen if it doesn't exist */
				if (!server->screens[number]) {
					server->screens[number] =
							xv_create(server_public, SCREEN, SCREEN_NUMBER,
							number, NULL);
				}
				if (!server->screens[number])
					goto Error;
				return (server->screens[number]);
			}

		case SERVER_FOCUS_TIMESTAMP:
			return (server->focus_timestamp);

		case SERVER_WM_DECOR_OK:
			return server_intern_atom(server, "_OL_DECOR_OK", 0L);
		case SERVER_WM_DECOR_PIN:
			return server_intern_atom(server, "_OL_DECOR_PIN", 0L);
		case SERVER_WM_SCALE_SMALL:
			return server_intern_atom(server, "_OL_SCALE_SMALL", 0L);
		case SERVER_WM_SCALE_MEDIUM:
			return server_intern_atom(server, "_OL_SCALE_MEDIUM", 0L);
		case SERVER_WM_SCALE_LARGE:
			return server_intern_atom(server, "_OL_SCALE_LARGE", 0L);
		case SERVER_WM_SCALE_XLARGE:
			return server_intern_atom(server, "_OL_SCALE_XLARGE", 0L);
		case SERVER_WM_RESCALE_STATE:
			return server_intern_atom(server, "_OL_RESCALE_STATE", 0L);
		case SERVER_WM_PIN_STATE:
			return server_intern_atom(server, "_OL_PIN_STATE", 0L);
		case SERVER_WM_WINMSG_STATE:
			return server_intern_atom(server, "_OL_WINMSG_STATE", 0L);
		case SERVER_WM_WINMSG_ERROR:
			return server_intern_atom(server, "_OL_WINMSG_ERROR", 0L);
		case SERVER_WM_WT_PROP:
			return server_intern_atom(server, "_OL_WT_PROP", 0L);
		case SERVER_WM_WT_HELP:
			return server_intern_atom(server, "_OL_WT_HELP", 0L);
		case SERVER_WM_WT_NOTICE:
			return server_intern_atom(server, "_OL_WT_NOTICE", 0L);
		case SERVER_WM_WT_OTHER:
			return server_intern_atom(server, "_OL_WT_OTHER", 0L);
		case SERVER_WM_NONE:
			/* this can be used to find out (during run time)
			 * whether this is the 'sun xview lib' or the 'dra xview lib'
			 * The official thing returns the atom for _OL_NONE
			 * (which is used in olwm's OlWinAttrs as a menu type)
			 * which is always != 0
			 */
			return 0;
		case SERVER_DO_DRAG_LOAD:
			return server_intern_atom(server, "XV_DO_DRAG_LOAD", 0L);
		case SERVER_WM_CHANGE_STATE:
			return server_intern_atom(server, "WM_CHANGE_STATE", 0L);

		case SERVER_RESOURCE_DB:
			return ((Xv_opaque) server->db);
		case XV_LOCALE_DIR:
			return (Xv_opaque) server->localedir;

		case SERVER_JOURNAL_SYNC_ATOM:
			return ((Xv_opaque) server->journalling_atom);

		case SERVER_EXTENSION_PROC:
			return ((Xv_opaque) server->extensionProc);

		case XV_NAME:
			return ((Xv_opaque) server->display_name);

#ifdef OW_I18N
		case SERVER_COMPOUND_TEXT:
			return server_intern_atom(server, "COMPOUND_TEXT", 0L);

		case XV_APP_NAME_WCS:
			return ((Xv_opaque) _xv_get_wcs_attr_dup(&server->app_name_string));

		case XV_APP_NAME:
			return ((Xv_opaque) _xv_get_mbs_attr_dup(&server->app_name_string));
#else
		case XV_APP_NAME:
			return ((Xv_opaque) server->app_name_string);
#endif

		case SERVER_DND_ACK_KEY:
			return ((Xv_opaque) server->dnd_ack_key);

		case SERVER_ATOM_DATA:{
				register Atom atom = va_arg(valist, Atom);
				register Xv_opaque data;

				data = server_get_atom_data(server, atom, status);
				if (*status == XV_ERROR)
					goto Error;
				else
					return ((Xv_opaque) data);
			}

		case SERVER_EXTERNAL_XEVENT_PROC:{
				Server_proc_list *node;

				if ((node = server_procnode_from_id(server, va_arg(valist,
												Xv_opaque))))
					result = (Xv_opaque) (node->extXeventProc);
				return result;
			}
		case SERVER_PRIVATE_XEVENT_PROC:{
				Server_proc_list *node;

				if ((node = server_procnode_from_id(server, va_arg(valist,
										Xv_opaque))))
					result = (Xv_opaque) (node->pvtXeventProc);
				return result;
			}
		case SERVER_EXTERNAL_XEVENT_MASK:{
				Server_mask_list *node;

				if ((node = server_masknode_from_xidid(server,
								va_arg(valist, Xv_opaque),
								va_arg(valist, Xv_opaque))))
					result = (Xv_opaque) (node->extmask);
				return result;
			}
		case SERVER_PRIVATE_XEVENT_MASK:{
				Server_mask_list *node;

				if ((node = server_masknode_from_xidid(server,
								va_arg(valist, Xv_opaque),
								va_arg(valist, Xv_opaque))))
					result = (Xv_opaque) (node->pvtmask);
				return result;
			}
	}
  Error:
	*status = XV_ERROR;
	return (Xv_opaque) 0;
}

static Xv_opaque server_get_attr(Xv_Server server_public, int *status, Attr_attribute attr, va_list valist)
{
	Server_info *server = SERVER_PRIVATE(server_public);

	switch (attr) {
		case SERVER_ATOM:
			{
				char *name = va_arg(valist, char *);

				return ((Xv_opaque) server_intern_atom(server, name, 0L));
			}

		case SERVER_WM_WIN_BUSY:
			return server_intern_atom(server, "_OL_WIN_BUSY", 0L);

		case SERVER_WM_TAKE_FOCUS:
			return server_intern_atom(server, "WM_TAKE_FOCUS", 0L);
		case SERVER_WM_TRANSIENT_FOR:
			return XA_WM_TRANSIENT_FOR;
		case SERVER_WM_SAVE_YOURSELF:
			return server_intern_atom(server, "WM_SAVE_YOURSELF", 0L);
		case SERVER_WM_PROTOCOLS:
			return server_intern_atom(server, "WM_PROTOCOLS", 0L);
		case SERVER_WM_DELETE_WINDOW:
			return server_intern_atom(server, "WM_DELETE_WINDOW", 0L);

		case SERVER_WM_WIN_ATTR:
			return server_intern_atom(server, "_OL_WIN_ATTR", 0L);

		case SERVER_FONT_WITH_NAME:{
				char *name = va_arg(valist, char *);

				return xv_font_with_name(server_public, name);
			}

		case XV_DISPLAY:
			return (Xv_opaque) (server->xdisplay);

#ifdef OW_I18N
		case XV_IM:
			return (Xv_opaque) (server->xim);
#endif /* OW_I18N */

		case SERVER_WM_ADD_DECOR:
			return server_intern_atom(server, "_OL_DECOR_ADD", 0L);
		case SERVER_WM_DELETE_DECOR:
			return server_intern_atom(server, "_OL_DECOR_DEL", 0L);
		case SERVER_WM_DECOR_CLOSE:
			return server_intern_atom(server, "_OL_DECOR_CLOSE", 0L);
		case SERVER_WM_DECOR_FOOTER:
			return server_intern_atom(server, "_OL_DECOR_FOOTER", 0L);
		case SERVER_WM_DECOR_RESIZE:
			return server_intern_atom(server, "_OL_DECOR_RESIZE", 0L);
		case SERVER_WM_DECOR_HEADER:
			return server_intern_atom(server, "_OL_DECOR_HEADER", 0L);
		case SERVER_WM_WT_BASE:
			return server_intern_atom(server, "_OL_WT_BASE", 0L);
		case SERVER_WM_WT_CMD:
			return server_intern_atom(server, "_OL_WT_CMD", 0L);
		case SERVER_WM_MENU_FULL:
			return server_intern_atom(server, "_OL_MENU_FULL", 0L);
		case SERVER_WM_MENU_LIMITED:
			return server_intern_atom(server, "_OL_MENU_LIMITED", 0L);
		case SERVER_WM_PIN_IN:
			return server_intern_atom(server, "_OL_PIN_IN", 0L);
		case SERVER_WM_PIN_OUT:
			return server_intern_atom(server, "_OL_PIN_OUT", 0L);
		case SERVER_DO_DRAG_MOVE:
			return server_intern_atom(server, "XV_DO_DRAG_MOVE", 0L);
		case SERVER_DO_DRAG_COPY:
			return server_intern_atom(server, "XV_DO_DRAG_COPY", 0L);
		case SERVER_WM_DISMISS:
			return server_intern_atom(server, "_OL_WIN_DISMISS", 0L);
		case SERVER_WM_COMMAND:
			return XA_WM_COMMAND;
		case SERVER_WM_DEFAULT_BUTTON:
			return server_intern_atom(server, "_OL_DFLT_BTN", 0L);

			/*
			 * Sundae buyback
			 */
		case XV_LC_BASIC_LOCALE:
			return (Xv_opaque) server->ollc[OLLC_BASICLOCALE].locale;

		case XV_LC_DISPLAY_LANG:
			return (Xv_opaque) server->ollc[OLLC_DISPLAYLANG].locale;

		case XV_LC_INPUT_LANG:
			return (Xv_opaque) server->ollc[OLLC_INPUTLANG].locale;

		case XV_LC_NUMERIC:
			return (Xv_opaque) server->ollc[OLLC_NUMERIC].locale;

		case XV_LC_TIME_FORMAT:
			return (Xv_opaque) server->ollc[OLLC_TIMEFORMAT].locale;

			/*
			 * End of Sundae buyback
			 */

#ifdef OW_I18N

#ifdef FULL_R5
		case XV_IM_PREEDIT_STYLE:
			return (Xv_opaque) server->preedit_style;

		case XV_IM_STATUS_STYLE:
			return (Xv_opaque) server->status_style;

		case XV_IM_STYLES:
			return (Xv_opaque) server->supported_im_styles;
#endif /* FULL_R5 */
#endif /* OW_I18N */

		case SERVER_XV_MAP:
			return ((Xv_opaque) server->xv_map);

		case SERVER_SEMANTIC_MAP:
			{
				int idx = va_arg(valist, int);
				return ((Xv_opaque) server->sem_maps[idx]);
			}

		case SERVER_ASCII_MAP:
			return ((Xv_opaque) server->ascii_map);

			/* ACC_XVIEW */
		case SERVER_ACCELERATOR_MAP:
			return ((Xv_opaque) server->acc_map);
			/* ACC_XVIEW */

		case SERVER_CUT_KEYSYM:
			return ((Xv_opaque) server->cut_keysym);

		case SERVER_PASTE_KEYSYM:
			return ((Xv_opaque) server->paste_keysym);

		case SERVER_HELP_KEYSYM:
			return ((Xv_opaque) server->help_keysym);

		case SERVER_SHAPE_AVAILABLE:
			return ((Xv_opaque) server->shape_available);

		case SERVER_JOURNALLING:
			return ((Xv_opaque) server->journalling);

		case SERVER_MOUSE_BUTTONS:
			return ((Xv_opaque) server->nbuttons);

		case SERVER_BUTTON2_MOD:
			return ((Xv_opaque) server->but_two_mod);

		case SERVER_BUTTON3_MOD:
			return ((Xv_opaque) server->but_three_mod);

		case SERVER_CHORDING_TIMEOUT:
			return ((Xv_opaque) server->chording_timeout);

		case SERVER_CHORD_MENU:
			return ((Xv_opaque) server->chord_menu);

		case SERVER_ALT_MOD_MASK:
			return ((Xv_opaque) server->alt_modmask);

		case SERVER_META_MOD_MASK:
			return ((Xv_opaque) server->meta_modmask);

		case SERVER_NUM_LOCK_MOD_MASK:
			return ((Xv_opaque) server->num_lock_modmask);

		case SERVER_SEL_MOD_MASK:
			return ((Xv_opaque) server->quick_modmask);

		case SERVER_EVENT_HAS_DUPLICATE_MODIFIERS:
			{
				Event *ev = va_arg(valist, Event *);
				int msk = (event_shiftmask(ev) & server->shiftmask_duplicate);

				return (Xv_opaque)(msk != 0);
			}
		case SERVER_EVENT_HAS_CONSTRAIN_MODIFIERS:
			{
				Event *ev = va_arg(valist, Event *);
				int msk = (event_shiftmask(ev) & server->shiftmask_constrain);

				return (Xv_opaque)(msk != 0);
			}
		case SERVER_EVENT_HAS_PAN_MODIFIERS:
			{
				Event *ev = va_arg(valist, Event *);
				int msk = (event_shiftmask(ev) & server->shiftmask_pan);

				return (Xv_opaque)(msk != 0);
			}
		case SERVER_EVENT_HAS_SET_DEFAULT_MODIFIERS:
			{
				Event *ev = va_arg(valist, Event *);
				int msk = (event_shiftmask(ev) & server->shiftmask_set_default);

				return (Xv_opaque)(msk != 0);
			}

		case SERVER_EVENT_HAS_PRIMARY_PASTE_MODIFIERS:
			{
				Event *ev = va_arg(valist, Event *);
				int msk = (event_shiftmask(ev) & server->shiftmask_primary_paste);

				return (Xv_opaque)(msk != 0);
			}

		case SERVER_ATOM_NAME:{
				register Atom atom = va_arg(valist, Atom);

				return ((Xv_opaque) server_get_atom_name(server, atom));
			}

		case SERVER_COMPOSE_STATUS:
			return ((Xv_opaque) server->composestatus);

		case SERVER_CLEAR_MODIFIERS:
			return ((Xv_opaque) server->pass_thru_modifiers);

		case SERVER_DISPLAY_CONTEXT:
			/* context used by XSaveContext and XFindContext to save/retrieve
			 * server_object from a dpy struct.
			 */
			return ((Xv_opaque) server->svr_dpy_context);

		case SERVER_UI_REGISTRATION_PROC:
			return (Xv_opaque) server->ui_reg_proc;

		case SERVER_TRACE_PROC:
	  		return (Xv_opaque)server->trace_proc;

		case SERVER_NTH_SCREEN:
		case SERVER_FOCUS_TIMESTAMP:
		case SERVER_WM_DECOR_OK:
		case SERVER_WM_DECOR_PIN:
		case SERVER_WM_SCALE_SMALL:
		case SERVER_WM_SCALE_MEDIUM:
		case SERVER_WM_SCALE_LARGE:
		case SERVER_WM_SCALE_XLARGE:
		case SERVER_WM_RESCALE_STATE:
		case SERVER_WM_PIN_STATE:
		case SERVER_WM_WINMSG_STATE:
		case SERVER_WM_WINMSG_ERROR:
		case SERVER_WM_WT_PROP:
		case SERVER_WM_WT_HELP:
		case SERVER_WM_WT_NOTICE:
		case SERVER_WM_WT_OTHER:
		case SERVER_WM_NONE:
		case SERVER_DO_DRAG_LOAD:
		case SERVER_WM_CHANGE_STATE:
		case SERVER_RESOURCE_DB:
		case XV_LOCALE_DIR:
		case SERVER_JOURNAL_SYNC_ATOM:
		case SERVER_EXTENSION_PROC:
		case XV_NAME:

#ifdef OW_I18N
		case SERVER_COMPOUND_TEXT:
		case XV_APP_NAME_WCS:
		case XV_APP_NAME:
#else
		case XV_APP_NAME:
#endif

		case SERVER_DND_ACK_KEY:
		case SERVER_ATOM_DATA:
		case SERVER_EXTERNAL_XEVENT_PROC:
		case SERVER_PRIVATE_XEVENT_PROC:
		case SERVER_EXTERNAL_XEVENT_MASK:
		case SERVER_PRIVATE_XEVENT_MASK:
			return (server_get_attr_tier2(server_public, status, attr, valist));

		case XV_APP_HELP_FILE:
			return (Xv_opaque)server->app_help_file;

		case XV_WANT_ROWS_AND_COLUMNS:
			return (Xv_opaque)server->want_rows_and_columns;

		case SERVER_LOAD_FIXEDWIDTH_FONT:
			return server_load_fixedwidth_font(server_public,
											va_arg(valist, char *));

		case SERVER_RESOURCE_DATABASES:
			return (Xv_opaque)server->xvwp->xvwp_xrmdb;

		case SERVER_HOST_NAME:
			{
				char *buf = va_arg(valist, char *);

				server_host_name(server_public, buf);
			}
			return XV_OK;

		case SERVER_IS_OWN_HELP:
			{
				Frame fr = va_arg(valist, Frame);
				Event *ev = va_arg(valist, Event *);

				return xvwp_is_own_help(server_public, fr, ev);
			}
			break;

		default:
			if (xv_check_bad_attr(SERVER, (Attr_attribute) attr) == XV_ERROR)
				goto Error;
	}
  Error:
	*status = XV_ERROR;
	return (Xv_opaque) 0;
}

Xv_private Xv_opaque server_get_timestamp(Xv_Server server_public)
{
    Server_info    *server = SERVER_PRIVATE(server_public);
    return ((Xv_opaque) server->xtime);
}

Xv_private Xv_opaque server_get_fullscreen(Xv_Server server_public)
{
    Server_info    *server = SERVER_PRIVATE(server_public);
    return ((Xv_opaque) server->in_fullscreen);
}
static Server_atom_type save_atom(Server_info *server, Atom	atom, Server_atom_type type)
{
	(void) XSaveContext(server->xdisplay, server->atom_mgr[TYPE],
			 (XContext) atom, (caddr_t) type);
	return (type); 
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

/*
 * This file contains only one exported function:
 *
 *	Xv_private int
 *	server_parse_keystr(server_public, keystr, keysym, code, 
 *				modifiers, diamond_mask, qual_str)
 *	Xv_server	server_public;
 *	CHAR		*keystr;
 *	KeySym		*keysym;
 *	short		*code;
 *	unsigned int	*modifiers;
 *	unsigned int	diamond_mask;
 *	char		*qual_str;
 *
 * It parses strings that specify key combinations. It was 
 * introduced for Menu Accelerators, but it should be shared by
 * mouseless as well. It recognizes Xt, OLIT, and XView syntax:
 *	Xt: 		[modifier...] '<keypress>' key
 *	OLIT:		[OLITmodifier...] '<'key'>'
 *	XView:		[modifier ['+' modifier] '+'] key
 *	modifier:	'meta' | 'shift' | 'alt' | 'hyper' | 'ctrl' | 
 *			'mod1' | ... | 'mod5'
 *	OLITmodifier:	modifier | 'm' | 's' | 'a' | 'h' | 'c'
 *	key:		all print characters and keysym names (e.g.
 *			'return', 'tab', 'comma', 'period', etc...)
 */

/*
 * START of declarations for parsing engine
 */

typedef struct acceleratorValue {
	KeySym keysym;
	unsigned meta:1,
		 shift:1,
		 alt:1,
		 ctrl:1,
		 super:1,
		 hyper:1,
		 lock:1,
		 modeswitch:1,
		 mod1:1, mod2:1, mod3:1, mod4:1, mod5:1,
		 error:1,
		 none:1,
		 some:1,
		 reserved:16;
} AcceleratorValue;

typedef struct {
	enum { styleNone, styleXView, styleOLIT, styleXt } style;
	AcceleratorValue av;
	char *pos;
} AVState;

typedef enum {
	modifMeta, modifShift, modifAlt, modifCtrl, modifSuper, modifHyper, 
	modifLock, modifModeswitch,
	modifMod1, modifMod2, modifMod3, modifMod4, modifMod5,
	modifNone
} AVModif;

typedef struct {
	CHAR *string;
	AVModif modif;
} AVKeyword;

#ifdef OW_I18N

/*
 * Macro to define Process code string/character literal
 * e.g. widechar string literals should look like:
 *	L"foo"
 * multibyte strings look like:
 *	"foo"
 */
#define XV_PROC_CODE(s)          L ## s

/*
 * General macros that should be moved to misc/i18n_impl.h
 */
#define STRSPN		wsspn
#define SSCANF		wsscanf
#define ISPUNCT		iswpunct
#define ISSPACE		iswspace
#define ISALNUM		iswalnum

#else /* OW_I18N */

#define XV_PROC_CODE(s)         s

/*
 * General macros that should be moved to misc/i18n_impl.h
 */
#define STRSPN		strspn
#define SSCANF		sscanf
#define ISPUNCT		ispunct
#define ISSPACE		isspace
#define ISALNUM		isalnum

#endif /* OW_I18N */

AVKeyword keywordTbl[] = {
{ XV_PROC_CODE("Meta"), modifMeta },
{ XV_PROC_CODE("Shift"), modifShift },
{ XV_PROC_CODE("Alt"), modifAlt },
{ XV_PROC_CODE("Ctrl"), modifCtrl },
{ XV_PROC_CODE("Super"), modifSuper },
{ XV_PROC_CODE("Hyper"), modifHyper },
{ XV_PROC_CODE("Lock"), modifLock },
{ XV_PROC_CODE("ModeSwitch"), modifModeswitch },
{ XV_PROC_CODE("Mod1"), modifMod1 }, 
{ XV_PROC_CODE("Mod2"), modifMod2 },
{ XV_PROC_CODE("Mod3"), modifMod3 },
{ XV_PROC_CODE("Mod4"), modifMod4 },
{ XV_PROC_CODE("Mod5"), modifMod5 },
{ XV_PROC_CODE("None"), modifNone }
};

AVKeyword shortKeywordTbl[] = {
{ XV_PROC_CODE("m"), modifMeta },
{ XV_PROC_CODE("su"), modifSuper },
{ XV_PROC_CODE("s"), modifShift },
{ XV_PROC_CODE("a"), modifAlt },
{ XV_PROC_CODE("c"), modifCtrl },
{ XV_PROC_CODE("h"), modifHyper },
{ XV_PROC_CODE("l"), modifLock },
{ XV_PROC_CODE("1"), modifMod1 },
{ XV_PROC_CODE("2"), modifMod2 },
{ XV_PROC_CODE("3"), modifMod3 },
{ XV_PROC_CODE("4"), modifMod4 },
{ XV_PROC_CODE("5"), modifMod5 },
{ XV_PROC_CODE("n"), modifNone }
};


#define keywordTblEnd \
	(keywordTbl + sizeof(keywordTbl) / sizeof(AVKeyword))

#define shortKeywordTblEnd \
	(shortKeywordTbl + sizeof(shortKeywordTbl) / sizeof(AVKeyword))

/*
 * Functions to implement parsing engine
 */
static AcceleratorValue getAcceleratorValue(CHAR *resourceString, XrmDatabase	db);
static void avGetXtAcceleratorValue(AcceleratorValue	*avp, CHAR *pos);
static void avGetOLITAcceleratorValue(AcceleratorValue	*avp, CHAR *pos);
static void avGetXViewAcceleratorValue(AcceleratorValue	*avp, CHAR *pos);
static CHAR *avAddKey(AcceleratorValue	*avp, CHAR			*pos);
static void avAddModif(AcceleratorValue 	*avp, AVModif			modif);

#define XV_KWRD_KEYPRESS        XV_PROC_CODE("<Key>")

/*
 * END of declarations for parsing engine
 */


/* ACC_XVIEW */
#ifdef OW_I18N
Xv_private int		xv_wsncasecmp();
#else
Xv_private int		xv_strncasecmp(char *, char *, unsigned);
#endif /* OW_I18N */
/* ACC_XVIEW */

extern XrmDatabase defaults_rdb;/* merged defaults database */



static AcceleratorValue getAcceleratorValue(CHAR *resourceString, XrmDatabase	db)
{
	AcceleratorValue av;

	/* if its starts with 'coreset', look for coreset resource */
#ifdef OW_I18N
	if( !xv_wsncasecmp
#else
	if( !xv_strncasecmp
#endif /* OW_I18N */
	( resourceString, XV_PROC_CODE("coreset"), (unsigned)STRLEN(XV_PROC_CODE("coreset"))) ) {

	char funcname[100], resname[200];
	XrmValue value;
	char *strtype;

	*funcname = '\0';
	SSCANF( resourceString, "%*s%s", funcname );

	/*
	 * Put resource name in multibyte buffer to pass to XrmGetResource()
	 */
	sprintf( resname, "OpenWindows.MenuAccelerator.%s", funcname );
	if( False == XrmGetResource( db, resname, "*", &strtype, &value ) )
		av.error = 1;
	else  {
#ifdef OW_I18N
		_xv_pswcs_t     pswcs = {0, NULL, NULL};

		/*
		 * Convert back to widechar before call parsing engine recursively
		 */
		_xv_pswcs_mbsdup(&pswcs, (char *)value.addr);
		av = getAcceleratorValue( pswcs.value, db );
		if (pswcs.storage != NULL)
			xv_free(pswcs.storage);
#else
		av = getAcceleratorValue( value.addr, db );
#endif /* OW_I18N */
	}
	return av;
	}

	/* try the three syntaxes, until one parses resourceString */
	XV_BZERO(&av, sizeof(av));
	avGetXtAcceleratorValue( &av, resourceString );
	if( av.error || !av.keysym ) {
		XV_BZERO(&av, sizeof(av));
		avGetOLITAcceleratorValue( &av, resourceString );
	}
	if( av.error || !av.keysym ) {
		XV_BZERO(&av, sizeof(av));
		avGetXViewAcceleratorValue( &av, resourceString );
	}
	if( !av.keysym )
	av.error = 1;

	return av;
}

static void avGetXtAcceleratorValue(AcceleratorValue	*avp, CHAR *pos)
{
	AVKeyword *kp;

	/* skip blanks */
	pos += STRSPN( pos, XV_PROC_CODE(" \t") );
	if( !*pos )
	return;

	/* look for one of the regular or abbrv. keywords */
	for( kp = keywordTbl ; kp < keywordTblEnd ; kp++ )
	if( !STRNCMP( kp->string, pos, STRLEN( kp->string ) ) )
		break;
	if( kp == keywordTblEnd )
		for( kp = shortKeywordTbl ; kp < shortKeywordTblEnd ; kp++ )
		if( !STRNCMP( kp->string, pos, STRLEN( kp->string ) ) )
		break;
	if( kp != shortKeywordTblEnd ) {
	/* disallow modifs after keysym is known */
	if( avp->keysym ) {
		avp->error = 1;
		return;
	}
	avAddModif( avp, kp->modif );
	avGetXtAcceleratorValue( avp, pos + STRLEN( kp->string ) );
	return;
	}

	/* look for '<' Key '>' <key-spec> and then nothing */
	if( !STRNCMP( XV_KWRD_KEYPRESS, pos, STRLEN( XV_KWRD_KEYPRESS ) ) ) {
	pos += STRLEN( XV_KWRD_KEYPRESS );
	pos += STRSPN( pos, XV_PROC_CODE(" \t") );
	pos = avAddKey( avp, pos );
		pos += STRSPN( pos, XV_PROC_CODE(" \t") );
	if( *pos )
		avp->error = 1;
	return;
	}

	/* an error occured */
	avp->error = 1;
	return;
}

static void avGetOLITAcceleratorValue(AcceleratorValue	*avp, CHAR *pos)
{
	AVKeyword *kp;

	/* skip blanks */
	pos += STRSPN( pos, XV_PROC_CODE(" \t") );
	if( !*pos )
	return;

	/* look for one of the regular or abbrv. keywords */
	for( kp = keywordTbl ; kp < keywordTblEnd ; kp++ )
	if( !STRNCMP( kp->string, pos, STRLEN( kp->string ) ) )
		break;
	if( kp == keywordTblEnd )
		for( kp = shortKeywordTbl ; kp < shortKeywordTblEnd ; kp++ )
		if( !STRNCMP( kp->string, pos, STRLEN( kp->string ) ) )
		break;
	if( kp != shortKeywordTblEnd ) {
	/* disallow modifs after keysym is known */
	if( avp->keysym ) {
		avp->error = 1;
		return;
	}
	avAddModif( avp, kp->modif );
	avGetOLITAcceleratorValue( avp, pos + STRLEN( kp->string ) );
	return;
	}

	/* look for '<' key '>' and then nothing */
	if ( *pos == XV_PROC_CODE('<') ) {
	pos = avAddKey( avp, pos + 1 );
		if( avp->error ) return;
		pos += STRSPN( pos, XV_PROC_CODE(" \t") );
	if( *pos != XV_PROC_CODE('>') )
		avp->error = 1;
	else {
			pos += 1 + STRSPN( pos+1, XV_PROC_CODE(" \t") );
		if( *pos )
		avp->error = 1;
	} 
	return;
	}

	/* an error occured */
	avp->error = 1;
	return;
}

static void avGetXViewAcceleratorValue(AcceleratorValue	*avp, CHAR *pos)
{
	AVKeyword *kp;

	/* skip blanks */
	pos += STRSPN(pos, XV_PROC_CODE(" \t"));
	if (!*pos)
		return;

	/* if mods or keysyms already found, look for '+' */
	if (avp->keysym || avp->some || avp->none) {
		if (*pos != XV_PROC_CODE('+')) {
			avp->error = 1;
			return;
		}
		else
			pos += 1 + STRSPN(pos + 1, XV_PROC_CODE(" \t"));
	}

	/* look for one of the regular keywords */
	for (kp = keywordTbl; kp < keywordTblEnd; kp++)
		if (!STRNCMP(kp->string, pos, STRLEN(kp->string)))
			break;

	if (kp != keywordTblEnd) {
		avAddModif(avp, kp->modif);
		avGetXViewAcceleratorValue(avp, pos + STRLEN(kp->string));
		return;
	}

	/* if no keysym name found yet, look for keysym */
	if (avp->keysym)
		avp->error = 1;
	else {
		pos = avAddKey(avp, pos);
		if (!avp->error)
			avGetXViewAcceleratorValue(avp, pos);
	}

	return;
}


static void avAddModif(AcceleratorValue 	*avp, AVModif			modif)
{
	if( modif == modifNone )
	avp->none = 1;
	else {
	avp->some = 1; 
		switch( modif ) {
	case modifMeta:	avp->meta = 1;	break;
	case modifShift:avp->shift = 1;	break;
	case modifAlt:	avp->alt = 1;	break;
	case modifCtrl:	avp->ctrl = 1;	break;
	case modifSuper:avp->super = 1;	break;
	case modifHyper:avp->hyper = 1;	break;
	case modifLock:	avp->lock = 1;	break;
	case modifModeswitch:	avp->modeswitch = 1;	break;
	case modifMod1:	avp->mod1 = 1;	break;
	case modifMod2:	avp->mod2 = 1;	break;
	case modifMod3:	avp->mod3 = 1;	break;
	case modifMod4:	avp->mod4 = 1;	break;
	case modifMod5:	avp->mod5 = 1;	break;
	default: break;
		}
	}

	if( avp->none && avp->some )
	avp->error = 1;
}


static CHAR *avAddKey(AcceleratorValue	*avp, CHAR			*pos)
{
	CHAR *sp, *dp, strbuf[100];

#ifdef OW_I18N
	char strbuf_mb[100];
#endif /* OW_I18N */

	/* if keysym already set, that's an error */
	if (avp->keysym) {
		avp->error = 1;
		return NULL;
	}

	/* look for 'raw' space or punctuation */
	if (ISPUNCT(*pos) || ISSPACE(*pos)) {
		avp->keysym = *pos;
		pos++;
	}
	else {	/* look for valid keysym name */
		for (sp = pos, dp = strbuf;
				dp < strbuf + sizeof(strbuf) && (ISALNUM(*sp)
						|| *sp == XV_PROC_CODE('_')); *dp++ = *sp++);
		*dp = XV_PROC_CODE('\0');

#ifdef OW_I18N
		/*
		 * Convert to multibyte to pass to XStringToKeysym()
		 */
		sprintf(strbuf_mb, "%ws", strbuf);
		if (avp->keysym = XStringToKeysym(strbuf_mb))
#else
		if ((avp->keysym = XStringToKeysym(strbuf)))
#endif /* OW_I18N */

			pos = sp;
		else
			avp->error = 1;	/* nothing parses as a key */
	}

	return pos;
}

/* ACC_XVIEW */
/*
 * Parses 'keystr' and fills in:
 *	keysym		- keysym of key combination
 *	code		- keycode of key 
 *	modifiers	- e.g. ShiftMask | ControlMask
 *	qual_str	- same as modifiers, but in string form. 
 *			  If any mask in modifiers is the same as
 *			  diamond_mask, it is skipped. e.g.
 *			  For diamond_mask = Meta, the mask
 *			  MetaMask | ShiftMask | ControlMask will
 *			  return "ctrl-shift"
 *
 * XV_OK is returned for successful parsing; XV_ERROR otherwise
 *
 * Does not modify the keystr string
 */
Xv_private int server_parse_keystr(Xv_server server_public, CHAR *keystr, KeySym *keysym, short *code, unsigned int *modifiers, unsigned int diamond_mask, char *qual_str)
{
	Server_info *server;
	Display *dpy;
	unsigned int alt_modmask, meta_modmask;
	KeyCode keycode;
	int ret_val = XV_OK, shift_ksym_exist = FALSE, shifted = FALSE;
	KeySym unmod_keysym, shifted_ksym;
	CHAR *tmp_str = NULL;
	AcceleratorValue av;

#ifdef OW_I18N
	_xv_pswcs_t pswcs = { 0, NULL, NULL };
#endif /* OW_I18N */

	if (!server_public || !keystr || !keysym || !code || !modifiers) {
		return (XV_ERROR);
	}

	/*
	 * Get server private data and display connection handle 
	 */
	server = SERVER_PRIVATE(server_public);
	dpy = server->xdisplay;

	/*
	 * Get Meta, Alt masks
	 */
	meta_modmask = (unsigned int)xv_get(server_public, SERVER_META_MOD_MASK);
	alt_modmask = (unsigned int)xv_get(server_public, SERVER_ALT_MOD_MASK);

	/*
	 * Make duplicate of keystr - our actions here will modify strings
	 */

#ifdef OW_I18N
	_xv_pswcs_wcsdup(&pswcs, keystr);
	tmp_str = pswcs.value;
#else
	tmp_str = xv_strsave(keystr);
#endif /* OW_I18N */

	/*
	 * Parse string
	 */
	av = getAcceleratorValue(tmp_str, defaults_rdb);

	if (av.error) {
		if (tmp_str) {

#ifdef OW_I18N
			if (pswcs.storage != NULL)
				xv_free(pswcs.storage);
#else
			xv_free(tmp_str);
#endif /* OW_I18N */
		}
		return (XV_ERROR);
	}

	/*
	 * The following strings are equivalent:
	 *  Shift+a, Shift+A, A
	 *  Shift+plus, Shift+equals, plus
	 *
	 * It is not recommended to use ShiftMask, but we try to be 
	 * compatible with strings that have shift and (un)shifted 
	 * keysyms in them.
	 *
	 * Basically:
	 *  If keysym is unshifted, and ShiftMask is specified, shift the
	 *  keysym (only if a shifted keysym exists for the keycode).
	 *  If keysym is already shifted and Shift is specified, remove
	 *  ShiftMask.
	 */

	/*
	 * Check if keysym is shifted. This is done by checking if the 
	 * returned keysym is in entry # 1 in the keysym list for the 
	 * keycode.
	 * So, first we get the keycode using XKeysymToKeycode(), then we 
	 * check using XKeycodeToKeysym().
	 */
	keycode = *code = XKeysymToKeycode(dpy, av.keysym);
	if (keycode) {
		/*
		 * Get keysym of shifted keycode. This is obtained by using
		 * index 1.
		 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#pragma GCC diagnostic ignored "-Wtraditional-conversion"

		unmod_keysym = XKeycodeToKeysym(dpy, keycode, 0);
		shifted_ksym = XKeycodeToKeysym(dpy, keycode, 1);
#pragma GCC diagnostic pop
		shift_ksym_exist = ((shifted_ksym != NoSymbol) &&
				(unmod_keysym != shifted_ksym));
		if (shift_ksym_exist) {
			shifted = (shifted_ksym == av.keysym);
		}
	}

	/*
	 * If this is not a shifted keysym, Shift was specified
	 * and a shifted keysym does exist, return the shifted
	 * keysym.
	 * Also, set shifted flag to TRUE
	 */
	if (!shifted && av.shift && shift_ksym_exist) {
		*keysym = shifted_ksym;
		shifted = TRUE;
	}
	else {
		*keysym = av.keysym;
	}

	/*
	 * If the keysym is already shifted, and Shift is specified,
	 * remove Shift
	 */
	if (shifted && av.shift) {
		av.shift = 0;
	}

	/*
	 * Set modifier masks
	 */
	if (av.meta) {
		*modifiers |= meta_modmask;
	}
	if (av.shift) {
		*modifiers |= ShiftMask;
	}
	if (av.alt) {
		*modifiers |= alt_modmask;
	}
	if (av.ctrl) {
		*modifiers |= ControlMask;
	}


	/*
	 * 'modifiers' now contains all the OR'd bits of the modifier masks
	 * We need to return this info in string format i.e. "ctrl-alt" in
	 * 'qual_str'. The modifier 'diamond_mask' is skipped.
	 */
	if (!av.error && qual_str) {
		short empty = TRUE;

		qual_str[0] = '\0';

		/*
		 * CONTROL
		 */
		if (av.ctrl && (diamond_mask != ControlMask)) {
			(void)strcat(qual_str, XV_MSG("ctrl"));
			empty = FALSE;
		}

		/*
		 * SHIFT
		 * Check first if this key is 'Shift'able
		 */
		if (((isascii((int)*keysym) && isalpha((int)*keysym)) ||
						(!shift_ksym_exist)) && (diamond_mask != ShiftMask)) {
			/*
			 * We print Shift if the keysym is a shifted one,
			 * or the modifier mask contains ShiftMask
			 */
			if (shifted || av.shift) {
				if (!empty) {
					(void)strcat(qual_str, "-");
				}
				(void)strcat(qual_str, XV_MSG("shift"));
				empty = FALSE;
			}
		}

		/*
		 * META
		 */
		if (av.meta && (diamond_mask != meta_modmask)) {
			if (!empty) {
				(void)strcat(qual_str, "-");
			}
			(void)strcat(qual_str, XV_MSG("meta"));
			empty = FALSE;
		}

		/*
		 * ALT
		 */
		if (av.alt && (diamond_mask != alt_modmask)) {
			if (!empty) {
				(void)strcat(qual_str, "-");
			}
			(void)strcat(qual_str, XV_MSG("alt"));
			empty = FALSE;
		}
	}

	if (tmp_str) {

#ifdef OW_I18N
		if (pswcs.storage != NULL)
			xv_free(pswcs.storage);
#else
		xv_free(tmp_str);
#endif /* OW_I18N */
	}

	return (ret_val);
}

/* ACC_XVIEW */

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
    XV_GENERIC_OBJECT,
    server_init,
    server_set_avlist,
    server_get_attr,
    server_destroy,
    NULL			/* no find proc */
};
