#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)svr_x.c 20.57 93/06/28 DRA: $Id: svr_x.c,v 4.8 2025/02/05 23:31:32 dra Exp $";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL_NOTICE 
 *	file for terms of the license.
 */

#include <stdio.h>
#include <unistd.h>
#include <xview/pkg.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#ifdef _XV_DEBUG
#include <xview_private/xv_debug.h>
#endif /* _XV_DEBUG */
#include <xview_private/i18n_impl.h>
#include <xview/win_event.h>
#include <X11/Xlib.h>
#include <xview/defaults.h>
#include <xview/server.h>
#include <xview_private/svr_impl.h>
#include <X11/keysym.h>

Xv_private Defaults_pairs xv_kbd_cmds_value_pairs[4];

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

Pkg_private     Xv_opaque server_init_x(char *server_name)
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
