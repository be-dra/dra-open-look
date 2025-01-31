#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)svr_atom.c 1.7 93/06/28 DRA: $Id: svr_atom.c,v 4.7 2025/01/05 10:48:57 dra Exp $";
#endif
#endif


/*      
 *      (c) Copyright 1990 Sun Microsystems, Inc. Sun design patents 
 *      pending in the U.S. and foreign countries. See LEGAL_NOTICE 
 *      file for terms of the license. 
 */

#include <xview/xview.h>
#include <X11/Xutil.h>
#include <X11/Xresource.h>
#include <xview_private/svr_impl.h>
#include <xview/server.h>

static void update_atom_list(Server_info *server, Atom atom);

#ifndef NULL
#define NULL 0
#endif

Xv_private Atom server_intern_atom(Server_info *server, char *atomName,
							Atom at)
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

Xv_private char *server_get_atom_name(Server_info *server, Atom atom)
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

Xv_private int server_set_atom_data(Server_info *server, Atom atom, Xv_opaque data)
{
    if (XSaveContext(server->xdisplay, server->atom_mgr[DATA],
			   (XContext)atom, (caddr_t) data) == XCNOMEM)
	return(XV_ERROR);
    else
	return(XV_OK);
}


Xv_private Xv_opaque server_get_atom_data(Server_info	*server, Atom atom, int *status)
{
    Xv_opaque data;

    if (XFindContext(server->xdisplay, server->atom_mgr[DATA], (XContext)atom,
		     (caddr_t *)&data) == XCNOENT)
	*status = XV_ERROR;
    else
	*status = XV_OK;

    return(data);
}

Pkg_private void server_initialize_atoms(Server_info *server)
{
	char *atns[] = {
		"ATOM_PAIR",
		"CLIPBOARD",
		"COMPOUND_TEXT",
		"DELETE",
		"DRAG_DROP",
		"DUPLICATE",
		"FILE_NAME",
		"FOUNDRY",
		"INCR",
		"INSERT_SELECTION",
		"LENGTH",
		"LENGTH_CHARS",
		"MOVE",
		"MULTIPLE",
		"NULL",
		"PIXEL",
		"STRING",
		"TARGETS",
		"TEXT",
		"TIMESTAMP",
		"WM_CHANGE_STATE",
		"WM_DELETE_WINDOW",
		"WM_PROTOCOLS",
		"WM_SAVE_YOURSELF",
		"WM_STATE",
		"WM_TAKE_FOCUS",
		"XV_DO_DRAG_COPY",
		"XV_DO_DRAG_LOAD",
		"XV_DO_DRAG_MOVE",
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
		"_DRA_FILE_STAT",
		"_DRA_NEXT_FILE",
		"_DRA_RESCALE",
		"_DRA_STRING_IS_FILENAMES",
		"_DRA_TRACE",
		"_MOTIF_WM_INFO",
		"_NETSCAPE_URL",
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
		"_OL_DFLT_BIN",
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
		"_SUN_DRAGDROP_TRANSIENT_2",
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
		"_SUN_SELN_COMMIT_PENDING_DELETE",
		"_SUN_SELN_CONTENTS_OBJECT",
		"_SUN_SELN_CONTENTS_PIECES",
		"_SUN_SELN_DO_FUNCTION",
		"_SUN_SELN_END_REQUEST",
		"_SUN_SELN_FIRST",
		"_SUN_SELN_FUNC_KEY_STATE",
		"_SUN_SELN_IS_READONLY",
		"_SUN_SELN_LAST",
		"_SUN_SELN_SELECTED_WINDOWS",
		"_SUN_SUNVIEW_ENV",
		"_SUN_WINDOW_STATE",
		"_SUN_WM_PROTOCOLS",
		"_SUN_WM_REREAD_MENU_FILE",
		"_XVIEW_V2_APP",
		"text/plain",
		"text/uri-list",
		NULL
	};
	Atom *atoms;
	int i, num_names;

	for (num_names = 0; atns[num_names]; num_names++);
	atoms = xv_alloc_n(Atom, (unsigned long)num_names);

	XInternAtoms(server->xdisplay, atns, num_names, FALSE, atoms);
	for (i = 0; i < num_names; i++) {
		server_intern_atom(server, atns[i], atoms[i]);
	}
	xv_free(atoms);
}
