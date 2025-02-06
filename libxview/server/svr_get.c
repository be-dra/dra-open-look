#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)svr_get.c 20.69 93/06/28 DRA: $Id: svr_get.c,v 4.9 2025/02/05 23:31:32 dra Exp $";
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
#include <ctype.h>
#include <netdb.h>
#include <xview/win_event.h>
#include <xview_private/svr_impl.h>
#include <xview_private/portable.h>
#include <xview/font.h>
#include <xview/permprop.h>
#include <X11/Xatom.h>

Xv_private Xv_opaque xv_font_with_name(Xv_server, char *);
static Xv_opaque server_get_attr_tier2(Xv_Server server_public, int *status, Attr_attribute attr, va_list valist);

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

Pkg_private Xv_opaque server_get_attr(Xv_Server server_public, int *status, Attr_attribute attr, va_list valist)
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
			return (Xv_opaque)server_xvwp_get_db(server);

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

				return server_xvwp_is_own_help(server_public, fr, ev);
			}
			break;

		default:
			if (xv_check_bad_attr(&xv_server_pkg,
							(Attr_attribute) attr) == XV_ERROR)
				goto Error;
	}
  Error:
	*status = XV_ERROR;
	return (Xv_opaque) 0;
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
