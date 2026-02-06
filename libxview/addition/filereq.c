/*
 * This file is a product of Bernhard Drahota and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify this file without charge, but are not authorized to
 * license or distribute it to anyone else except as part of a product
 * or program developed by the user.
 *
 * THIS FILE IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 *
 * This file is provided with no support and without any obligation on the
 * part of Bernhard Drahota to assist in its use, correction,
 * modification or enhancement.
 *
 * BERNHARD DRAHOTA SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY THIS FILE
 * OR ANY PART THEREOF.
 *
 * In no event will Bernhard Drahota be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even
 * if B. Drahota has been advised of the possibility of such damages.
 */
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/utsname.h>
#ifdef linux
/* but this won't help if <sys/stat.h> has already been included before... -
 * - therefore:
 */
#ifndef __USE_FILE_OFFSET64
#  ifdef	_SYS_STAT_H
#    error sys/stat.h has already been included
#  endif
#  define __USE_FILE_OFFSET64 1
#endif
#endif /* linux */
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <xview/xview.h>
#include <xview/filedrag.h>
#include <xview/filereq.h>
#include <xview_private/svr_impl.h>

#ifndef lint
char filereq_c_sccsid[] = "@(#) %M% V%I% %E% %U% $Id: filereq.c,v 4.10 2026/02/05 19:09:24 dra Exp $";
#endif

/* Xv_private : */
extern void server_set_timestamp(Xv_Server, struct timeval *, unsigned long);
extern Xv_opaque server_get_timestamp(Xv_Server);

typedef struct _loadfile {
	struct _loadfile *next;
	char *remote_appl, *origdropped, *localfile;
	Atom loadsel_atom;
} *loadlist_t;

typedef void (*reply_proc_t)(File_requestor, Atom, Atom, Xv_opaque,
												unsigned long, int);

typedef struct {
	Xv_opaque                   public_self;

	int                         offset;
	Filereq_string_proc         string_proc;
	Filereq_status              status;
	char                        check_access, already_decoded, single,
								use_load, allocate;
	int                         count;
	char                        **files;

	loadlist_t                  loadlist;
	Xv_server                   srv;
	reply_proc_t                appl_reply;
	Filereq_remote_save_proc    inssel_cb;
	int                         fildesc;
	char                        is_string_request, during_incr,
								own_request, write_to_file;
	FileDrag                    selowner;
	Atom                        save_rank;
	struct timeval              last_time;
} Filereq_private;

#define A0 *attrs
#define A1 attrs[1]
#define A2 attrs[2]
#define A3 attrs[3]

#define ADONE ATTR_CONSUME(*attrs);break

#define FRPRIV(_x_) XV_PRIVATE(Filereq_private, Xv_filereq_public, _x_)
#define FRPUB(_x_) XV_PUBLIC(_x_)

static int fr_key = 0;
static int fr_load_key;

static loadlist_t find_loadlist(loadlist_t list, char *localname)
{
	while (list) {
		if (! strcmp(localname, list->localfile)) break;
		list = list->next;
	}

	return list;
}

static loadlist_t release_loaded(Filereq_private *priv, loadlist_t ll,
				char *locname)
{
	if (! ll) return ll;

	if (!strcmp(ll->localfile, locname)) {
		loadlist_t p = ll->next;
		File_requestor self = FRPUB(priv);

		xv_set(self,
				SEL_RANK, ll->loadsel_atom,
				SEL_TYPE_NAME, "_SUN_SELECTION_END",
				NULL);
		sel_post_req(self);

		if (priv->selowner) {
			if (ll==(loadlist_t)xv_get(priv->selowner,XV_KEY_DATA,fr_load_key))
			{
				xv_set(priv->selowner, XV_KEY_DATA, fr_load_key, 0, NULL);
			}
		}

		unlink(ll->localfile);
		xv_free(ll->localfile);
		xv_free(ll->origdropped);
		if (ll->remote_appl) xv_free(ll->remote_appl);
		xv_free((char *)ll);
		return p;
	}

	ll->next = release_loaded(priv, ll->next, locname);
	return ll;
}

static void free_own_files(Filereq_private *priv)
{
	int i;

	if (! priv->files) return;

	for (i = 0; i < priv->count && priv->files[i]; i++) xv_free(priv->files[i]);
	xv_free((char *)priv->files);
	priv->files = (char **)0;
	priv->count = 0;
}

static int my_conversion(Xv_opaque owner, Atom *type, Xv_opaque *value,
				unsigned long *length, int *format)
{
	Xv_server srv = XV_SERVER_FROM_WINDOW(xv_get(owner, XV_OWNER));
	loadlist_t ll = (loadlist_t)xv_get(owner, XV_KEY_DATA, fr_load_key);

	if (!ll) return FALSE;

	if (*type == (Atom)xv_get(srv, SERVER_ATOM, "FILE_NAME")) {
		*type = XA_STRING;
		*length = strlen(ll->origdropped);
		*format = 8;
		*value = (Xv_opaque)ll->origdropped;
		return TRUE;
	}

	return xv_iccc_convert(owner, type, value, length, format);
}

static void request_failed(Filereq_private *priv)
{
	free_own_files(priv);
}

static void inssel_answer(Filereq_private *priv)
{
	File_requestor self = FRPUB(priv);
	loadlist_t ll = (loadlist_t)xv_get(priv->selowner,XV_KEY_DATA,fr_load_key);

	xv_set(self, SEL_RANK, priv->save_rank, NULL);

	if (ll && priv->inssel_cb) {
		(*(priv->inssel_cb))(self, priv->status, ll->localfile);
	}

	xv_set(priv->selowner,
				XV_KEY_DATA, fr_load_key, 0,
				SEL_OWN, FALSE,
				NULL);
}

static void decode_error(Filereq_private *priv, int err)
{
	switch (err) {
		case SEL_BAD_PROPERTY:
			priv->status =  FR_BAD_PROPERTY;
			break;
		case SEL_BAD_CONVERSION:
			priv->status =  FR_CONVERSION_REJECTED;
			break;
		case SEL_BAD_TIME:
			priv->status =  FR_BAD_TIME;
			break;
		case SEL_BAD_WIN_ID:
			priv->status =  FR_BAD_WIN_ID;
			break;
		case SEL_TIMEDOUT:
			priv->status =  FR_TIMEDOUT;
			break;
		default:
			priv->status = FR_UNKNOWN_ERROR;
	}
}

static void filereq_reply(File_requestor self, Atom target, Atom type,
					Xv_opaque value, unsigned long length, int format)
{
	Filereq_private *priv = FRPRIV(self);
	int is_inssel = FALSE;

	if (target == (Atom)xv_get(priv->srv, SERVER_ATOM, "INSERT_SELECTION")) {
		is_inssel = TRUE;
	}

	if (length == SEL_ERROR) {
		int errcode = *(int *)value;

		decode_error(priv, errcode);
		if (is_inssel) inssel_answer(priv);
		if (priv->is_string_request) {
			(*(priv->string_proc))(self, FILE_REQ_STRING_ERROR,
										(char *)0, (unsigned long)priv->status);
			priv->is_string_request = FALSE;
		}

		if (priv->appl_reply) {
			(*priv->appl_reply)(self, target, type, value, length, format);
		}
		return;
	}

	if (is_inssel) {
		inssel_answer(priv);
		if (value) xv_free((char *)value);
		return;
	}

	if (type == (Atom)xv_get(priv->srv, SERVER_ATOM, "INCR")) {
		priv->during_incr = TRUE;
		if (priv->is_string_request) {
			(*(priv->string_proc))(self, FILE_REQ_INCR,
								(char *)0, (unsigned long)(*(int *)value));
		}
		/* if we go ahead here, the file will contain the file length
		 * in the first 4 Bytes..
		 */
		return;
	}

	if (priv->write_to_file) {
		if (priv->fildesc >= 0 && priv->status == FR_OK) {
			size_t bytelen;

			bytelen = (size_t)length * (format / 8);
			if (bytelen > 0) {
				int writelen;

				writelen = write(priv->fildesc, (char *)value, bytelen);
				if (writelen < bytelen) {
					priv->status = FR_WRITE_FAILED;
					close(priv->fildesc);
					priv->fildesc = -1;
				}
			}
			else {
				/* end of INCR */
				priv->during_incr = FALSE;
				close(priv->fildesc);
				priv->fildesc = -1;
			}
		}

		if (value) xv_free((char *)value);
	}

	if (priv->is_string_request && target == XA_STRING) {
		if (priv->during_incr) {
			if (length > 0) {
				(*(priv->string_proc))(self, FILE_REQ_INCR_PACKET,
								(char *)value, (unsigned long)length);
			}
			else {
				priv->during_incr = FALSE;
				(*(priv->string_proc))(self, FILE_REQ_INCR_END, (char *)0, 0L);
				priv->is_string_request = FALSE;
			}
		}
		else {
			(*(priv->string_proc))(self, FILE_REQ_STRING, (char *)value,
										(unsigned long)length);
			priv->is_string_request = FALSE;
		}
	}

	if (priv->appl_reply) {
		(*priv->appl_reply)(self, target, type, value, length, format);
	}
}

static void save_loaded(Filereq_private *priv, char *locfile,
				Filereq_remote_save_proc cb, struct timeval *tim)
{
	File_requestor self = FRPUB(priv);
	loadlist_t ll;
	Atom inssel[2];
	struct timeval srvtim;

	for (ll = priv->loadlist; ll; ll = ll->next) {
		if (! strcmp(ll->localfile, locfile)) break;
	}
	if (! ll) {
/* 		DTRACE(DTL_INTERN, "in save_loaded: %s was not remote\n", locfile); */
		if (cb) (*cb)(self, FR_OK, locfile);
		return;
	}

	if (! priv->selowner) {
		char buf[200];
		Xv_window win = xv_get(self, XV_OWNER);

		/* this is the selection "S" - see the description below */
		sprintf(buf, "_DRA_FILEREQ_%lx_%lx", (unsigned long)xv_get(win, XV_XID),
											(unsigned long)priv);

		SERVERTRACE((311, "%s: create sel owner %s\n", __FUNCTION__, buf));
		priv->selowner = xv_create(win, FILEDRAG,
							SEL_RANK_NAME, buf,
							FILEDRAG_ENABLE_STRING, FILEDRAG_CONTENTS_STRING,
							SEL_CONVERT_PROC, my_conversion,
							XV_KEY_DATA, fr_key, priv,
							NULL);
	}

	if (! tim) {
		Xv_window win = xv_get(self, XV_OWNER);
		Xv_server srv = XV_SERVER_FROM_WINDOW(win);

		server_set_timestamp(srv, &srvtim, server_get_timestamp(srv));
		tim = &srvtim;
	}

	xv_set(priv->selowner,
			XV_KEY_DATA, fr_load_key, ll,
			FILEDRAG_FILES, ll->localfile, 0,
			SEL_TIME, tim,
			SEL_OWN, TRUE,
			NULL);

	inssel[0] = (Atom)xv_get(priv->selowner, SEL_RANK);
	inssel[1] = XA_STRING;
	SERVERTRACE((311, "%s: assign '%s' to sel owner %ld\n", __FUNCTION__,
						ll->localfile, inssel[0]));
	priv->save_rank = (Atom)xv_get(self, SEL_RANK);
	xv_set(self,
			SEL_RANK, ll->loadsel_atom,
			SEL_TIME, tim,
			SEL_TYPE_NAME, "INSERT_SELECTION",
			SEL_TYPE_INDEX, 0,
			SEL_PROP_FORMAT, 32,
			SEL_PROP_LENGTH, 2L,
			SEL_PROP_TYPE_NAME, "ATOM_PAIR",
			SEL_PROP_DATA, inssel,
			NULL);

	priv->inssel_cb = cb;
	SERVERTRACE((311, "%s: requesting INSERT_SELECTION\n", __FUNCTION__));
	sel_post_req(self);
}

/*
 * This text is copied from the Solaris 2.2 Desktop Integration Guide.
 *
 * ====================================================================
 * When a requestor application (an editor of some kind) finds that it
 * needs to obtain the source data through the selection service, and
 * intends to allow the user to edit the data and then put it back, the
 * recipient tries to convert the _DRA_LOAD target. If the selection holder
 * supports the _DRA_LOAD target, it will respond with the name of a
 * selection (called "H" for this discussion) that the holder guarantees
 * will be unique and persistent for the life of the selection holder. This
 * selection is associated with the original data in the drag and drop
 * transfer.
 * The editor may manipulate the data as it sees fit. When it is time
 * to save the data back to the original location, the editor creates a new
 * selection rank and associates it with the data that it is currently
 * holding (call the selection rank "S"). The editor then converts the
 * INSERT_SELECTION target against the selection H. A selection can be
 * guaranteed as unique by converting _SUN_SELECTION_END against H when
 * done. In the property associated with the target conversion, the editor
 * places the name of the selection S.
 * The conversion of INSERT_SELECTION tells the selection holder to replace
 * the contents of the current selection (selection H, representing the
 * original data in storage) with the contents of the new selection
 * (selection S, whose contents represent the edited version of the data).
 * Refer to section 2.6.3.2 of the ICCCM.
 * The original selection may then make target conversions against
 * selection S to get the data back from the editor.
 * ====================================================================
 *
 * Now, in our implementation, the editor loads the data via the original
 * dnd-selection, saves it into a temporary file (see [jgsvergfnkbwrf])
 * which the editor (which is "our caller") loads.
 * When the user wants to "Save" this file, we end up in "save_loaded"
 * and perform that INSERT_SELECTION. The remote entity (say a file manager)
 * fetches the modified data and stores them into the original dragged file.
 *
 * So far, so good. But our editor is still running on the temporary file -
 * a situation that the above text says nothing about. 
 * Can we request _SUN_SELECTION_END now???
 * Should the editor be terminated and the temp file removed?????
 * We will have to leave this to the editor that uses the 
 * attribute FILE_REQ_SAVE... - however, on the application level nothing
 * is known about the selection H, so, we need an attribute that leads
 * to a request of _SUN_SELECTION_END: 
		FILE_REQ_RELEASE:
 */

static char *perform_dra_load(Filereq_private *priv, char *file, char *rem_name)
{
	File_requestor self = FRPUB(priv);
	Atom sl, *adata;
	loadlist_t newl;
	int format;
	long length;
	char *base, buf[500];

	xv_set(self,
			SEL_TIME, &priv->last_time,
			SEL_TYPE_NAME, "_DRA_LOAD",
			NULL);
	adata = (Atom *)xv_get(self, SEL_DATA, &length, &format);
	if (length == SEL_ERROR) {
		/* _DRA_LOAD errors are never kept */
		priv->status = FR_OK;
		return (char *)0;
	}

	if (adata) {
		/* this is the selection "H" - see the description above */
		sl = *adata;
		xv_free(adata);
	}
	else {
		/* _DRA_LOAD errors are never kept */
		priv->status = FR_OK;
		return (char *)0;
	}

	if (length != 1 || format != 32) {
		/* _DRA_LOAD errors are never kept */
		priv->status = FR_OK;
		return (char *)0;
	}

	base = strrchr(file, '/');
	if (base) ++base;
	else base = file;

	/* Reference [jgsvergfnkbwrf] */
	sprintf(buf, "/tmp/%s@%s", base, rem_name);
	priv->fildesc = open(buf, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (priv->fildesc < 0) {
		priv->status = FR_OPEN_FAILED;
		return (char *)0;
	}

	xv_set(self,
			SEL_TIME, &priv->last_time,
			SEL_TYPE, XA_STRING,   /* or SEL_TYPE_NAME, "TEXT" ??? */
			NULL);
	priv->write_to_file = TRUE;
	xv_get(self, SEL_DATA, &length, &format);
	priv->write_to_file = FALSE;
	if (priv->fildesc >= 0) {
		close(priv->fildesc);
		priv->fildesc = -1;
	}
	if (priv->status != FR_OK) {
		unlink(buf);
		return (char *)0;
	}

	newl = (loadlist_t)xv_alloc(struct _loadfile);
	newl->origdropped = xv_strsave(file);
	newl->localfile = xv_strsave(buf);
	newl->loadsel_atom = sl;
	newl->next = priv->loadlist;
	priv->loadlist = newl;

	xv_set(self,
			SEL_TIME, &priv->last_time,
			SEL_TYPE_NAME, "NAME",
			NULL);
	newl->remote_appl = (char *)xv_get(self, SEL_DATA, &length, &format);
	priv->status = FR_OK;

	return xv_strsave(buf);
}

static void fetch_files(Filereq_private *priv,Selection_requestor sr, Event *ev)
{
	long length;
	int owner_is_remote, i, j, format, *idata;
	char *cdata;
	struct utsname name;
	char rem_name[sizeof(name.nodename) + 1];
	struct timeval tim;
	int have_tim = FALSE;
	int ev_flags = 0;

	priv->status = FR_OK;
	tim.tv_sec = 0;
	tim.tv_usec = 0;

	/* wir kennen den remote name noch nicht : */
	rem_name[0] = '\0';
	owner_is_remote = FALSE;

	if (ev) {
		if (event_action(ev) != ACTION_DRAG_COPY &&
			event_action(ev) != ACTION_DRAG_MOVE)
		{
			priv->status = FR_WRONG_EVENT;
			return;
		}
		tim = event_time(ev);
		priv->last_time = tim;
		have_tim = TRUE;
		ev_flags = event_flags(ev);
	}

	if (! have_tim) {
		struct timeval *owntim = (struct timeval *)xv_get(sr, SEL_TIME);

		if (owntim->tv_sec != 0 || owntim->tv_usec != 0) {
			tim = *owntim;
			priv->last_time = tim;
			have_tim = TRUE;
		}
	}

	if (! have_tim) {
		if (priv->last_time.tv_sec != 0 || priv->last_time.tv_usec != 0) {
			tim = priv->last_time;
			have_tim = TRUE;
		}
	}

	if (! have_tim) {
		server_set_timestamp(priv->srv, &tim, server_get_timestamp(priv->srv));
	}

	free_own_files(priv);

	if (!priv->already_decoded) {
		if (dnd_decode_drop(sr, ev) == XV_ERROR) {
			priv->status = FR_CANNOT_DECODE;
			return;
		}
	}

	if (ev_flags & DND_IS_XDND) {
		char *p;

		/*  ohohohohoh weeeeh
		 *  ein Fuzzy-Tool - die koennen nicht viel....
		 * da muss ich mich auch nicht so anstrengen (Stand Feb 2008)
		 */

#ifdef FIREFOX_DND_22_01_2022
	Forschung: was passiert, wenn man von firefox ein Bild mit D&D wirft.

		TARGETS: 
				TIMESTAMP
				TARGETS
				MULTIPLE
				text/x-moz-url
					Len 366
					die kommen mit Format 8, aber jedes 2. Byte ist NUL
					soll aber wohl das gleich wie _NETSCAPE_URL sein.
					Vielleicht meinen die format 16 ???
				_NETSCAPE_URL
					Type _NETSCAPE_URL, Fmt 8, Len 183:
					https://www.historisches-lexikon-bayerns.de/images/0/02/Nuernberg_St_Lorenz_Westfassade.jpg
https://www.historisches-lexikon-bayerns.de/images/0/02/Nuernberg_St_Lorenz_Westfassade.jpg
					Wieso doppelt? Die zweite Zeile ist ein Kommentar...
				text/x-moz-url-data
					Analog text/x-moz-url, Fmt 8/16, aber Len = 182
				text/x-moz-url-desc
					Analog text/x-moz-url, Fmt 8/16, aber Len = 182
				application/x-moz-custom-clipdata
					obskur, fmt 8, len 224
				text/_moz_htmlcontext
					obskur, fmt 8, len 52
				text/_moz_htmlinfo
					obskur, fmt 8, len 6
				text/html
					Fmt 8/16, len 402
				text/unicode
					Fmt 8/16, len 182, die URL
				text/plain;charset=utf-8
					Fmt 8, Len 91: 
					'https://www.historisches-lexikon-bayerns.de/images/0/02/Nuernberg_St_Lorenz_Westfassade.jpg'
				text/plain
					Fmt 8, Len 91: 
					'https://www.historisches-lexikon-bayerns.de/images/0/02/Nuernberg_St_Lorenz_Westfassade.jpg'
				application/x-moz-nativeimage
					Conversion Rejected
				image/png
					INCR.....
				image/jpeg
				image/jpg
				image/gif
				XdndDirectSave0
					Fmt 8, Len 1: 'E'
					meaning 'Error'
				application/x-moz-file-promise
					Fmt 8/16, len 182, die URL
				application/x-moz-file-promise-url
				text/uri-list
					file:///tmp/dnd_file-20/Nuernberg_St_Lorenz_Westfassade.jpg
				application/x-moz-file-promise-dest-filename
				DELETE

#endif /* FIREFOX_DND_22_01_2022 */
		xv_set(sr,
				SEL_TIME, &tim,
				SEL_TYPE_NAME, "text/uri-list",
				NULL);
		cdata = (char *)xv_get(sr, SEL_DATA, &length, &format);
		if (length == SEL_ERROR) {
			request_failed(priv);
			return;
		}

		/* da kommt evtl eine NEWLINE-getrennte Liste, erst mal zaehlen */
		priv->count = 0;
		p = cdata;
		while ((p = strchr(p, '\n'))) {
			++priv->count;
			++p;
		}

		free_own_files(priv);
		priv->files = (char **)xv_alloc_n(char *, (size_t)priv->count + 1);
		/* was da kommt sieht ueblicherweise (sie schnallen es nicht,
		 * was da in der XDND-Spec steht...) so aus: 'file://fullpath\r\n'
		 * also z.B. 'file:///home/username/.cshrc' - idiotisch, das
		 */

		i = 0;
		for (p = strtok(cdata, "\r\n"); p; p = strtok(0, "\r\n")) {
			if (! strncmp(p, "file://", 7L)) {
				char tmpbuf[1000], *filebegin;

				strcpy(tmpbuf, p + 7);

				/* now, in tmpbuf, there is no more 'file://'
				 * so, the rest could look like 
				 *    sun4/home/sun4/dra/tmp/lala.txt
				 * or
				 *    /home/sun4/dra/tmp/lala.txt
				 */
				if (tmpbuf[0] == '/') {
					/* full path, obviously no host */
					filebegin = tmpbuf;
				}
				else {
					filebegin = strchr(tmpbuf, '/');
					if (filebegin) {
						*filebegin = '\0';
						strcpy(rem_name, tmpbuf);
						*filebegin = '/';
						uname(&name);
						owner_is_remote = (strcmp(rem_name,name.nodename) != 0);
					}
					else {
						filebegin = tmpbuf;
					}
				}

				priv->files[i] = xv_strsave(filebegin);
				i++;
			}
			else if (*p) {
				priv->files[i] = xv_strsave(p);
				i++;
			}
		}
		priv->files[i] = 0;
		priv->count = i;
		return;
	}

	priv->status =  FR_OK;
	xv_set(sr,
			SEL_TIME, &tim,
			SEL_TYPE_NAME, "_SUN_ENUMERATION_COUNT",
			NULL);
	idata = (int *)xv_get(sr, SEL_DATA, &length, &format);
	if (length == SEL_ERROR) {
		priv->status = FR_CONVERSION_REJECTED;
		return;
	}

	priv->count = *idata;
	xv_free((char *)idata);

	if (priv->single && priv->count > 1) priv->count = 1;

	free_own_files(priv);
	priv->files = (char **)xv_alloc_n(char *, (size_t)priv->count + 1);

	if (priv->check_access) {
		if (! rem_name[0]) {
			xv_set(sr,
					SEL_TIME, &tim,
					SEL_TYPE_NAME, "_SUN_FILE_HOST_NAME",
					NULL);
			cdata = (char *)xv_get(sr, SEL_DATA, &length, &format);
			if (length == SEL_ERROR) {
				xv_set(sr,
						SEL_TIME, &tim,
						SEL_TYPE_NAME, "HOST_NAME",
						NULL);
				cdata = (char *)xv_get(sr, SEL_DATA, &length, &format);
				if (length == SEL_ERROR) {
					priv->status = FR_CONVERSION_REJECTED;
					return;
				}
			}

			uname(&name);
			owner_is_remote = (strcmp(cdata, name.nodename) != 0);
			strcpy(rem_name, cdata);
			xv_free(cdata);
		}
	}

	for (j = i = 0; i < priv->count; i++) {
		cdata = NULL;

		xv_set(sr,
				SEL_TIME, &tim,
				SEL_TYPE_NAME, "_SUN_ENUMERATION_ITEM",
				SEL_TYPE_INDEX, 0,
				SEL_PROP_FORMAT, 32,
				SEL_PROP_TYPE, XA_INTEGER,
				SEL_PROP_LENGTH, 1L,
				SEL_PROP_DATA, &i,
				NULL);
		idata = (int *)xv_get(sr, SEL_DATA, &length, &format);
		if (length == SEL_ERROR) {
			request_failed(priv);
			return;
		}
		xv_free((char *)idata);

		xv_set(sr, SEL_TYPE_NAME, "FILE_NAME", NULL);
		cdata = (char *)xv_get(sr, SEL_DATA, &length, &format);
		if (length == SEL_ERROR) {
			request_failed(priv);
			return;
		}

		if (priv->check_access && owner_is_remote) {
			struct stat sb;
			char *locnam;

			if (stat(cdata, &sb)) {
				/* stat failed: no such file on local disk */

				if (priv->use_load) {
					locnam = perform_dra_load(priv, cdata, rem_name);
					if (locnam) priv->files[j++] = locnam;
				}
			}
			else {
				Filereq_remote_stat *rs;

				/* stat succeeded */

				xv_set(sr, SEL_TYPE_NAME, "_DRA_FILE_STAT", NULL);
				rs = (Filereq_remote_stat *)xv_get(sr, SEL_DATA, &length,
														&format);
				if (length == sizeof(Filereq_remote_stat)/sizeof(long) &&
					format == 32)
				{
					if ((long)sb.st_size == rs->size &&
						(long)sb.st_ino == rs->inode &&
						(long)sb.st_mtime == rs->mtime)
					{
						/* we seem to talk about the same file */
						priv->files[j++] = cdata;
						/* don't free */
						cdata = NULL;
					}
					else {
						locnam = perform_dra_load(priv, cdata, rem_name);
						if (locnam) priv->files[j++] = locnam;
					}
				}
				else {
					locnam = perform_dra_load(priv, cdata, rem_name);
					if (locnam) priv->files[j++] = locnam;
				}

				if (rs) xv_free((char *)rs);
			}
		}
		else {	
			priv->files[j++] = cdata;
			/* don't free */
			cdata = NULL;
		}

		if (cdata) xv_free(cdata);
	}

	priv->count = j;
}

static void request_delete(Filereq_private *priv, int ind)
{
	char *cdata;
	int format;
	long length;

	xv_set(FRPUB(priv),
			SEL_TIME, &priv->last_time,
			SEL_TYPE_NAME, "_SUN_ENUMERATION_ITEM",
			SEL_TYPE_INDEX, 0,
			SEL_PROP_FORMAT, 32,
			SEL_PROP_TYPE, XA_INTEGER,
			SEL_PROP_LENGTH, 1L,
			SEL_PROP_DATA, &ind,
			NULL);
	cdata = (char *)xv_get(FRPUB(priv), SEL_DATA, &length, &format);
	if (length == SEL_ERROR)  return;
	xv_free(cdata);

	xv_set(FRPUB(priv),
			SEL_TIME, &priv->last_time,
			SEL_TYPE_NAME, "_DRA_FM_DELETE",
			SEL_TYPE_INDEX, 0,
			SEL_PROP_LENGTH, 0L,
			SEL_PROP_DATA, NULL,
			NULL);
	cdata = (char *)xv_get(FRPUB(priv), SEL_DATA, &length, &format);
	if (length == SEL_ERROR) {
		xv_set(FRPUB(priv),
				SEL_TIME, &priv->last_time,
				SEL_TYPE_NAME, "DELETE",
				SEL_TYPE_INDEX, 0,
				SEL_PROP_LENGTH, 0L,
				SEL_PROP_DATA, NULL,
				NULL);
		cdata = (char *)xv_get(FRPUB(priv), SEL_DATA, &length, &format);
		if (length == SEL_ERROR)  return;
	}
	if (cdata) xv_free(cdata);
}

static int filereq_init(Xv_window owner, Xv_opaque xself, Attr_avlist avlist,
							int *unused)
{
	Xv_filereq_public *self = (Xv_filereq_public *)xself;
	Filereq_private *priv = (Filereq_private *)xv_alloc(Filereq_private);
	Attr_attribute attrs[6];

	if (!priv) return XV_ERROR;

	priv->public_self = (Xv_opaque)self;
	self->private_data = (Xv_opaque)priv;

	if (! fr_key) {
		fr_key = xv_unique_key();
		fr_load_key = xv_unique_key();
	}

	priv->srv = XV_SERVER_FROM_WINDOW(owner);
	priv->allocate = TRUE;
	priv->fildesc = -1;

	attrs[0] = (Attr_attribute)SEL_REPLY_PROC;
	attrs[1] = (Attr_attribute)filereq_reply;
	attrs[2] = (Attr_attribute)NULL;
	xv_super_set_avlist((Xv_opaque)self, FILE_REQUESTOR, attrs);

	return XV_OK;
}

static Xv_opaque filereq_set(File_requestor self, Attr_avlist avlist)
{
	Attr_attribute *attrs;
	Filereq_private *priv = FRPRIV(self);

	for (attrs=avlist; *attrs; attrs=attr_next(attrs)) switch ((int)*attrs) {
		case SEL_REPLY_PROC:
			priv->appl_reply = (reply_proc_t)A1;
			ADONE;

		case SEL_TIME:
			if (A1) {
				struct timeval *tim = (struct timeval *)A1;

				if (tim->tv_sec != 0 || tim->tv_usec != 0) {
					priv->last_time = *tim;
				}
			}
			break;

		case FILE_REQ_FETCH:
			priv->own_request = TRUE;
			fetch_files(priv, self, (Event *)A1);
			priv->own_request = FALSE;
			ADONE;

		case FILE_REQ_FETCH_VIA:
			priv->already_decoded = TRUE;
			priv->check_access = FALSE;
			fetch_files(priv, (Selection_requestor)A1, (Event *)0);
			ADONE;

		case FILE_REQ_SINGLE:
			priv->single = (char)A1;
			ADONE;

		case FILE_REQ_OFFSET:
			priv->offset = (int)A1;
			ADONE;

		case FILE_REQ_ALLOCATE:
			priv->allocate = (char)A1;
			ADONE;

		case FILE_REQ_REQUEST_DELETE:
			request_delete(priv, (int)A1);
			ADONE;

		case FILE_REQ_REQUEST_STRING:
			priv->string_proc = (Filereq_string_proc)A1;
			priv->is_string_request = TRUE;
			xv_set(self,
					SEL_TIME, &priv->last_time,
					SEL_TYPE, XA_STRING,
					NULL);
			if (A2) { /* blocking */
				int unused_format;
				unsigned long unused_len;

				xv_get(self, SEL_DATA, &unused_len, &unused_format);
				priv->is_string_request = FALSE;
			}
			else {
				sel_post_req(self);
			}

			ADONE;

		case FILE_REQ_STRING_TO_FILE:
			{
				int unused_format;
				unsigned long unused_len;
				Filereq_status *stp;

				xv_set(self,
						SEL_TIME, &priv->last_time,
						SEL_TYPE, XA_STRING,
						NULL);
				priv->write_to_file = TRUE;
				priv->status = FR_OK;
				priv->fildesc = (int)A1;
				xv_get(self, SEL_DATA, &unused_len, &unused_format);
				priv->fildesc = -1;
				priv->write_to_file = FALSE;
				stp = (Filereq_status *)A2;
				if (stp) *stp = priv->status;
			}
			ADONE;

		case FILE_REQ_CHECK_ACCESS:
			priv->check_access = (char)A1;
			ADONE;

		case FILE_REQ_ALREADY_DECODED:
			priv->already_decoded = (char)A1;
			ADONE;

		case FILE_REQ_USE_LOAD:
			priv->use_load = (char)A1;
			ADONE;

		case FILE_REQ_SAVE:
			priv->own_request = TRUE;
			save_loaded(priv, (char *)A1, (Filereq_remote_save_proc)A2,
										(struct timeval *)A3);
			priv->own_request = FALSE;
			ADONE;

		case FILE_REQ_RELEASE:
			priv->loadlist = release_loaded(priv, priv->loadlist, (char *)A1);
			ADONE;

		case SEL_TYPE:
		case SEL_TYPE_NAME:
		case SEL_TYPES:
		case SEL_TYPE_NAMES:
			if (! priv->own_request) priv->status = FR_OK;
			break;

		default: xv_check_bad_attr(FILE_REQUESTOR, A0);
			break;
	}

	return XV_OK;
}

static Xv_opaque filereq_get(File_requestor self, int *status,
				Attr_attribute attr, va_list vali)
{
	Filereq_private *priv = FRPRIV(self);

	*status = XV_OK;
	switch ((int)attr) {
		case SEL_REPLY_PROC: return (Xv_opaque)priv->appl_reply;
		case FILE_REQ_STATUS: return (Xv_opaque)priv->status;
		case FILE_REQ_SINGLE: return (Xv_opaque)priv->single;
		case FILE_REQ_OFFSET: return (Xv_opaque)priv->offset;
		case FILE_REQ_ALLOCATE: return (Xv_opaque)priv->allocate;
		case FILE_REQ_CHECK_ACCESS: return (Xv_opaque)priv->check_access;
		case FILE_REQ_FILES:
			{
				int *cntp = va_arg(vali, int *);

				if (! priv->files) {
					if (! priv->single && cntp) *cntp = 0;
					return XV_NULL;
				}

				if (priv->single) {
					if (priv->allocate)
						return (Xv_opaque)xv_strsave(priv->files[0]);
					else return (Xv_opaque)priv->files[0];
				}
				else {
					if (cntp) *cntp = priv->count;

					if (priv->allocate) {
						int i;
						char **files = (char **)xv_alloc_n(char *,
									(size_t)(priv->count + priv->offset + 1));

						for (i = 0; priv->files[i]; i++) {
							files[i+priv->offset] = xv_strsave(priv->files[i]);
						}

						return (Xv_opaque)files;
					}
					else return (Xv_opaque)priv->files;
				}
			}
		case FILE_REQ_ALREADY_DECODED: return (Xv_opaque)priv->already_decoded;
		case FILE_REQ_USE_LOAD: return (Xv_opaque)priv->use_load;
		case FILE_REQ_REMOTE_APPL_FOR:
			{
				loadlist_t ll =find_loadlist(priv->loadlist,va_arg(vali,char*));

				if (ll) return (Xv_opaque)ll->remote_appl;
				else return XV_NULL;
			}
		case FILE_REQ_REMOTE_NAME_FOR:
			{
				char *v1 = va_arg(vali,char*);
				loadlist_t ll = find_loadlist(priv->loadlist, v1);

				if (ll) return (Xv_opaque)ll->origdropped;
				else return (Xv_opaque)v1;
			}
		case FILE_REQ_IS_LOADED:
			{
				char *v1 = va_arg(vali,char*);
				loadlist_t ll = find_loadlist(priv->loadlist, v1);

				if (ll) return (Xv_opaque)TRUE;
				else return (Xv_opaque)FALSE;
			}
		case FILE_REQ_TITLE_FOR:
/* 			INCOMPLETE, not yet */
			return XV_NULL;
		default:
			*status = xv_check_bad_attr(FILE_REQUESTOR, attr);
			return (Xv_opaque)XV_OK;
	}
}

static void free_loadlist(loadlist_t ll)
{
	if (! ll) return;
	free_loadlist(ll->next);
	if (ll->remote_appl) xv_free(ll->remote_appl);
	if (ll->origdropped) xv_free(ll->origdropped);
	if (ll->localfile) xv_free(ll->localfile);
}

static int filereq_destroy(File_requestor self, Destroy_status status)
{
	if (status == DESTROY_CLEANUP) {
		Filereq_private *priv = FRPRIV(self);

		free_own_files(priv);
		free_loadlist(priv->loadlist);
		if (priv->selowner) xv_destroy(priv->selowner);
		xv_free(priv);
	}
	return XV_OK;
}

const Xv_pkg xv_filereq_pkg = {
	"FileRequestor",
	ATTR_PKG_FILE_REQ,
	sizeof(Xv_filereq_public),
	SELECTION_REQUESTOR,
	filereq_init,
	filereq_set,
	filereq_get,
	filereq_destroy,
	0
};
