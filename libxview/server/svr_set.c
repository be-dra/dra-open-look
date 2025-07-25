#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)svr_set.c 20.56 93/06/28 DRA: $Id: svr_set.c,v 4.10 2025/07/23 17:02:13 dra Exp $";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL_NOTICE 
 *	file for terms of the license.
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <xview/win_event.h>
#include <xview_private/svr_impl.h>
#include <xview_private/win_info.h>

/* ACC_XVIEW */
static void server_semantic_map_offset(Xv_server server_public, unsigned int	modifiers, int *offset);
/* ACC_XVIEW */
static int server_add_xevent_mask(Server_info *server, Xv_opaque xid, Xv_opaque	mask, Xv_opaque pkg_id, int external);
static int server_add_xevent_proc(Server_info *server, Xv_opaque func, Xv_opaque pkg_id, int external);

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

Pkg_private Xv_opaque server_set_avlist(Xv_Server self, Attr_attribute *avlist)
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
				server->extensionProc = (void (*)(Display *,XEvent *, Xv_opaque))attrs[1];
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
				server_xvwp_write_file(server);
				ATTR_CONSUME(*attrs);
				break;

			case SERVER_BASE_FRAME:
				server_xvwp_install((Frame)attrs[1]);
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
				server_xvwp_connect(self, "base");
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
				(void)xv_check_bad_attr(&xv_server_pkg,
						(Attr_attribute) attrs[0]);
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

Pkg_private Server_xid_list * server_xidnode_from_xid(Server_info *server,
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

Pkg_private Server_proc_list *server_procnode_from_id(Server_info *server,
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

Pkg_private Server_mask_list *server_masknode_from_xidid(Server_info *server,
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
