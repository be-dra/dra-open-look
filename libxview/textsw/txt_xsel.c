#ifndef lint
char txt_xsel_c_sccsid[] = "@(#) $Id: txt_xsel.c,v 1.48 2025/02/07 21:32:19 dra Exp $";
#endif

#include <xview/defaults.h>
#include <xview_private/txt_impl.h>
#include <xview_private/svr_impl.h>
#include <xview_private/ev_impl.h>
#include <xview/textsw.h>
#include <ctype.h>
#include <time.h> 

#define XVIEW_CLASS_TARGET future_use1

extern char *xv_app_name;

/* copied basically from textsw_seln_yield */
static void selection_give_up(Textsw_private priv, Atom rank)
{
	Textsw textsw;

	if (rank == XA_PRIMARY) {
		textsw = TEXTSW_PUBLIC(priv);
		textsw_set_selection(textsw, ES_INFINITY, ES_INFINITY, EV_SEL_PRIMARY);
	}
	else if (rank == XA_SECONDARY) {
		textsw = TEXTSW_PUBLIC(priv);
		textsw_set_selection(textsw,ES_INFINITY, ES_INFINITY, EV_SEL_SECONDARY);
		priv->track_state &= ~TXTSW_TRACK_SECONDARY;
		if (!EV_MARK_IS_NULL(&priv->save_insert)) {
			ev_remove_finger(&priv->views->fingers,
					priv->save_insert);
			EV_INIT_MARK(priv->save_insert);
		}
	}
	else { /* can only be CLIPBOARD */
		if (priv->trash) {
			es_destroy(priv->trash);
			priv->trash = ES_NULL;
		}
	}
}

static void text_lose_proc(Selection_owner sel_own)
{
	Textsw tsw;
	Textsw_private priv;
	Atom rank_atom;

	tsw = xv_get(sel_own, XV_OWNER);
	priv = TEXTSW_PRIVATE(tsw);
    rank_atom = (Atom) xv_get(sel_own, SEL_RANK);

	SERVERTRACE((388, "%s for %ld\n", __FUNCTION__, rank_atom));
    selection_give_up(priv, rank_atom);
}

Pkg_private int textsw_convert_delete(Textsw_private priv,
				Textsw_view_private view, Selection_owner owner,
				int handle_smart_word)
{
	Atom rank_atom = (Atom)xv_get(owner, SEL_RANK);
	int rank_index;
	unsigned evsel = EV_SEL_PRIMARY;
	Es_index start, first, last_plus_one, ro_bound;
	Textsw tsw = TEXTSW_PUBLIC(priv);

	if (rank_atom == XA_SECONDARY) {
		rank_index = TSW_SEL_SECONDARY;
		evsel = EV_SEL_SECONDARY;
	}
	else if (rank_atom == priv->atoms.clipboard) {
		/* I don't like the idea that somebody requests DELETE
		 * for the CLIPBOARD selection
		 */
		return FALSE;
	}
	else { /* primary or dnd */
		rank_index = TSW_SEL_PRIMARY;
	}

	/* Destination asked us to delete the selection.
	 * If it is appropriate to do so, we should.
	 */

	if (TXTSW_IS_READ_ONLY(priv)) {
		SERVERTRACE((765, "DELETE request refused: read only\n"));
		return FALSE;
	}

	ev_get_selection(priv->views, &first, &last_plus_one, evsel);
	ro_bound = textsw_read_only_boundary_is_at(priv);
	start =  (first < ro_bound) ? ro_bound : first;
	SERVERTRACE((765, "DELETing from %d to %d\n", start, last_plus_one));
	textsw_delete_span(view, start, last_plus_one, 0);

	if (handle_smart_word
		&& priv->smart_word_handling
		&& priv->select_is_word[rank_index])
	{
		char buf[10];

		SERVERTRACE((377, "%s: is_word[%d]=%d\n", __FUNCTION__,
				priv->current_sel, priv->select_is_word[priv->current_sel]));
		if (start == 0) {
			/* remove one blank */
			xv_get(tsw, TEXTSW_CONTENTS, 0, buf, 1);
			if (buf[0] == ' ') textsw_erase(tsw, 0, 1);
		}
		else {
			/* get the two characters at start-1 and start */
			/* collapse two blanks to one */
			xv_get(tsw, TEXTSW_CONTENTS, start-1, buf, 2);
			buf[2] = '\0';
			if (buf[0] == ' ') {
				if (buf[1] == ' ') textsw_erase(tsw, start, start+1);
				else if (ispunct(buf[1])) textsw_erase(tsw, start-1, start);
			}
		}
	}
	return TRUE;
}	

Pkg_private int textsw_internal_convert(Textsw_private priv,
				Textsw_view_private view, Selection_owner owner, Atom *type,
				Xv_opaque *data, unsigned long *length, int *format)
{
	int retval;

	if (*type == priv->atoms.delete) {
		if (! textsw_convert_delete(priv, view, owner, FALSE)) return FALSE;

		*format = 32;
		*length = 0;
		*data = XV_NULL;
		*type = priv->atoms.null;
		return TRUE;
	}
	else if (*type == priv->atoms.ol_sel_word) {
		Atom rank_atom = (Atom)xv_get(owner, SEL_RANK);
		int rank_index;

		if (rank_atom == XA_SECONDARY)
			rank_index = TSW_SEL_SECONDARY;
		else if (rank_atom == priv->atoms.clipboard) {
			rank_index = TSW_SEL_CLIPBOARD;
		}
		else { /* primary or dnd */
			rank_index = TSW_SEL_PRIMARY;
		}
		/* wie findet man das heraus */
		/* siehe start_seln_tracking */
		priv->sel_reply = priv->select_is_word[rank_index];
		*data = (Xv_opaque)&priv->sel_reply;
		SERVERTRACE((377, "%s: answering %d\n", __FUNCTION__, priv->sel_reply));
		*length = 1;
		*format = 32;
		*type = XA_INTEGER;
		return TRUE;
	}
	else if (*type== priv->atoms.seln_is_readonly) {
		priv->sel_reply = TXTSW_IS_READ_ONLY(priv);

		*format = 32;
		*length = 1;
		*data = (Xv_opaque) & priv->sel_reply;
		*type = XA_INTEGER;
		return TRUE;
	}
	else if (*type == priv->atoms.dragdrop_ack) {
		/* Test: I have a Textsw that is also able to accept 'non-text data'
		 * and therefore calls dnd_decode_drop and TRIES to convert to, 
		 * say, PIXMAP.
		 * If this fails, the XView Textsw calls dnd_decode_drop AGAIN... and
		 * this second request failed because _SUN_DRAGDROP_ACK was rejected
		 */
		*format = 32;
		*length = 0;
		*data = XV_NULL;
		*type = priv->atoms.null;
		return TRUE;
	}
	else if (*type == priv->atoms.XVIEW_CLASS_TARGET) {
		Xv_object obj = xv_get(owner, XV_OWNER);
		Xv_pkg *pkg = (Xv_pkg *)xv_get(obj, XV_TYPE);

		*format = 8;
		*length = strlen(pkg->name);
		/* this will be a memory leak */
		*data = (Xv_opaque)xv_strsave(pkg->name);
		*type = XA_STRING;
		return TRUE;
	}

	/* Use default Selection Package convert procedure */
	retval = sel_convert_proc(owner, type, data, (unsigned long *)length,
			format);
#ifdef NO_XDND
#else /* NO_XDND */
	if (! retval) {
		if (*type == priv->atoms.plain) {
			Atom savat = *type;

			*type = XA_STRING;
			retval = sel_convert_proc(owner, type, data,
							(unsigned long *)length, format);

			*type = savat;
		}
	}
#endif /* NO_XDND */

	return retval;
}

static int text_convert_proc(Selection_owner sel_own, Atom *type,
					Xv_opaque *data, unsigned long *length, int *format)
{
	Textsw tsw;
	Textsw_private priv;
	Atom rank_atom;
	int rank_index;
	Xv_Server server;
	Textsw_view v;
	Textsw_view_private view;

	tsw = xv_get(sel_own, XV_OWNER);
	priv = TEXTSW_PRIVATE(tsw);
	server = XV_SERVER_FROM_WINDOW(tsw);
	v = xv_get(tsw, OPENWIN_NTH_VIEW, 0);
	view = VIEW_PRIVATE(v);

	rank_atom = (Atom) xv_get(sel_own, SEL_RANK);
	SERVERTRACE((765, "%s request for %s\n",
			(char *)xv_get(server, SERVER_ATOM_NAME, rank_atom),
			(char *)xv_get(server, SERVER_ATOM_NAME, *type)));

	if (xv_get(sel_own, SEL_RANK) == XA_SECONDARY 
		&& *type == priv->atoms.selection_end)
	{
		/* Lose the Selection - we support this only for SECONDARY */
		xv_set(sel_own, SEL_OWN, FALSE, NULL);
        *type = priv->atoms.null;
        *data = XV_NULL;
        *length = 0;
        *format = 32;
		return TRUE;
	}

	if (*type == priv->atoms.length) {
		/* This is only used by SunView1 selection clients for
		 * clipboard and secondary selections.
		 */
		if (rank_atom == XA_SECONDARY)
			rank_index = TSW_SEL_SECONDARY;
		else
			rank_index = TSW_SEL_CLIPBOARD;
		priv->sel_reply =
				(unsigned long)xv_get(priv->sel_item[rank_index], SEL_LENGTH);
		*data = (Xv_opaque)&priv->sel_reply;
		*length = 1;
		*format = 32;
		return TRUE;
	}

	return textsw_internal_convert(priv, view, sel_own, type, data,
												length, format);
}

static void caret_lose_proc(Selection_owner sel_own)
{
	/* 08.02.2025: do we ever see this ? */
	fprintf(stderr, "%s: %s`%s-%d: CARET selection is used!!!\n",
			xv_app_name, __FILE__, __FUNCTION__, __LINE__);
	SERVERTRACE((310, "%s for %ld\n", __FUNCTION__,
							xv_get(sel_own, SEL_RANK)));
}

static int caret_convert_proc(Selection_owner sel_own, Atom *type,
					Xv_opaque *data, unsigned long *length, int *format)
{
	Textsw tsw;
	Xv_Server server;

	/* 08.02.2025: do we ever see this ? */
	fprintf(stderr, "%s: %s`%s-%d: CARET selection is used!!!\n",
			xv_app_name, __FILE__, __FUNCTION__, __LINE__);
	tsw = xv_get(sel_own, XV_OWNER);
	server = XV_SERVER_FROM_WINDOW(tsw);

	SERVERTRACE((310, "%s request for %s\n",
			(char *)xv_get(sel_own, SEL_RANK_NAME),
			(char *)xv_get(server, SERVER_ATOM_NAME, *type)));

	/* Use default Selection Package convert procedure */

	return sel_convert_proc(sel_own, type, data, (unsigned long *)length,
										format);
}

static void note_sel_reply(Selection_requestor sr, Atom target, Atom type,
					Xv_opaque value, unsigned long length, int format)
{
	Textsw w = xv_get(sr, XV_OWNER);
	Textsw_private priv = TEXTSW_PRIVATE(w);
	Xv_server server = XV_SERVER_FROM_WINDOW(w);

	if (length == SEL_ERROR) {
		int errval = *((int *)value);
		char *p = "???";

		switch (errval) {
			case SEL_BAD_PROPERTY :
				p = "Bad Property";
				break;
			case SEL_BAD_CONVERSION :
				p = "Conversion Rejected";
				break;
			case SEL_BAD_TIME:
				p = "Bad Time Match";
				break;
			case SEL_BAD_WIN_ID:
				p = "Bad Window Match";
				break;
			case SEL_TIMEDOUT:
				p = "Timeout";
				break;
		}
		SERVERTRACE((333, "SEL_ERROR on %ld for target %ld: %s\n",
					xv_get(sr, SEL_RANK), target, p));
		return;
	}
 
	SERVERTRACE((345, "note_sel_reply: target = %s\n",
				(char *)xv_get(server, SERVER_ATOM_NAME, target)));
	if (target == priv->atoms.selection_end) {
		if (value) xv_free(value);
	}
}

Pkg_private void textsw_new_sel_copy(Textsw_view_private view, Event *ev)
{
	Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
	Selection_item primsel = priv->sel_item[TSW_SEL_PRIMARY];
	unsigned long len = xv_get(primsel, SEL_LENGTH);
	struct timeval tv;

	if (ev) tv = event_time(ev);
	else {
		server_set_timestamp(XV_SERVER_FROM_WINDOW(TEXTSW_PUBLIC(priv)),&tv,0L);
	}

	SERVERTRACE((333, "in textsw_new_sel_copy: prim '%s'\n",
					(char *)xv_get(primsel, SEL_DATA)));
	xv_set(priv->sel_item[TSW_SEL_CLIPBOARD],
						SEL_DATA, xv_get(primsel, SEL_DATA),
						SEL_LENGTH, len,
						NULL);

	xv_set(priv->sel_owner[TSW_SEL_CLIPBOARD],
						SEL_TIME, &tv,
						SEL_OWN, TRUE,
						NULL);

	priv->select_is_word[TSW_SEL_CLIPBOARD] =
									priv->select_is_word[TSW_SEL_PRIMARY];
	SERVERTRACE((377, "%s: is_word=%d\n", __FUNCTION__,
				priv->select_is_word[TSW_SEL_CLIPBOARD]));
}

static void init_sel_req(Textsw_private priv)
{
	if (priv->sel_req) return;

	priv->sel_req = xv_create(TEXTSW_PUBLIC(priv), SELECTION_REQUESTOR,
					/* needed because of nonblocking _SUN_SELECTION_END: */
					SEL_REPLY_PROC, note_sel_reply,
					NULL);
}

Pkg_private void textsw_new_sel_copy_appending(Textsw_view_private view,
						Event *ev)
{
	Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
	Selection_item primsel = priv->sel_item[TSW_SEL_PRIMARY];
	unsigned long len = xv_get(primsel, SEL_LENGTH);
	struct timeval tv;
	unsigned long applen;
	char *oldclip, *appended;
	int format;
	unsigned long oldcliplen;
	int old_is_word, new_is_word;

	if (ev) tv = event_time(ev);
	else {
		server_set_timestamp(XV_SERVER_FROM_WINDOW(TEXTSW_PUBLIC(priv)),&tv,0L);
	}

	init_sel_req(priv);

	/* first, fetch the "old" clipboard contents */
	xv_set(priv->sel_req,
			SEL_RANK_NAME, "CLIPBOARD",
			SEL_TYPE, XA_STRING,
			SEL_AUTO_COLLECT_INCR, TRUE,
			NULL);
	oldclip = (char *)xv_get(priv->sel_req, SEL_DATA, &oldcliplen, &format);
	if (oldcliplen == SEL_ERROR) {
		if (oldclip) xv_free(oldclip);
		oldclip = strdup("");
		old_is_word = FALSE;
		oldcliplen = 0;
	}
	else {
		old_is_word = xv_get(priv->sel_req, SEL_IS_WORD_SELECTION);
	}

	applen = len + oldcliplen;
	appended = xv_malloc(applen + 1);
	sprintf(appended, "%s%s", oldclip, (char *)xv_get(primsel, SEL_DATA));
	SERVERTRACE((333, "in textsw_new_sel_copy: app '%s'\n", appended));
	xv_set(priv->sel_item[TSW_SEL_CLIPBOARD],
						SEL_DATA, appended,
						SEL_LENGTH, applen,
						NULL);

	xv_set(priv->sel_owner[TSW_SEL_CLIPBOARD],
						SEL_TIME, &tv,
						SEL_OWN, TRUE,
						NULL);

	new_is_word = priv->select_is_word[TSW_SEL_PRIMARY];
	priv->select_is_word[TSW_SEL_CLIPBOARD] = (old_is_word && new_is_word);

	SERVERTRACE((377, "%s: is_word=%d\n", __FUNCTION__,
				priv->select_is_word[TSW_SEL_CLIPBOARD]));
}

Pkg_private Es_index textsw_wrap_do_input(Textsw tsw, Textsw_view_private view,
						CHAR *buf, long int *buf_len)
{
	if (xv_get(tsw, TEXTSW_IS_TERMSW)) {
		if (buf[*buf_len - 1] == '\n') {
			buf[*buf_len - 1] = '\r';
		}
	}
	return textsw_do_input(view, buf, *buf_len, TXTSW_UPDATE_SCROLLBAR);
}

Pkg_private void textsw_new_sel_cut(Textsw_view_private view, Event *ev,
									int canbequickmove)
{
	Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
	unsigned long length;
	int format;
	char *string;
	Es_index pos, temp, first, lpo;
	struct timeval tv;
	Textsw tsw = TEXTSW_PUBLIC(priv);

	if (ev) tv = event_time(ev);
	else {
		server_set_timestamp(XV_SERVER_FROM_WINDOW(tsw),&tv,0L);
	}

	SERVERTRACE((333, "in textsw_new_sel_cut\n"));

	init_sel_req(priv);

	if (canbequickmove) {
		/* could have been a quick move */
		xv_set(priv->sel_req,
				SEL_RANK, XA_SECONDARY,
				SEL_TYPE, XA_STRING,
				SEL_TIME, &tv,
				NULL);
		SERVERTRACE((333, "requesting SECONDARY STRING\n"));
		string = (char *)xv_get(priv->sel_req, SEL_DATA, &length, &format);
		SERVERTRACE((333, "requested SECONDARY STRING, len=%ld, '%s'\n",
							length, string));

		if (length != SEL_ERROR) {
			char *dummy;

			/* yes, it WAS a quick move */

			SERVERTRACE((333, "requesting SECONDARY DELETE\n"));
			xv_set(priv->sel_req, SEL_TYPE_NAME, "DELETE", NULL);
			dummy = (char *)xv_get(priv->sel_req, SEL_DATA, &length, &format);
			if (dummy) xv_free(dummy);
			SERVERTRACE((333, "requested SECONDARY DELETE, len=%ld\n", length));

			xv_set(priv->sel_req, SEL_TYPE, priv->atoms.selection_end, NULL);
			sel_post_req(priv->sel_req);

			if (string) {
				long str_len;
				/* now, we have the 'quick moved' text in 'string' */

				/* if there is a primary selection, we have to handle it as
				 * "pending delete"...
				 */
				ev_get_selection(priv->views, &first, &lpo, EV_SEL_PRIMARY);
				if (lpo > first) {
					textsw_erase(TEXTSW_PUBLIC(priv), first, lpo);
				}
				/* now similar to txt_move - dnd */
				/* pos has been in dnd calculated from the "drop coordinates
				 * We handle "quick move" here, so, it must be the
				 * caret position:
				 */
				pos = (Es_index)xv_get(TEXTSW_PUBLIC(priv),
												TEXTSW_INSERTION_POINT);

				ev_set(view->e_view, EV_CHAIN_DELAY_UPDATE, FALSE, NULL);
				EV_SET_INSERT(priv->views, pos, temp);

				str_len = strlen(string);
				textsw_wrap_do_input(TEXTSW_PUBLIC(priv), view, (char *)string,
									&str_len);

				free((char *)string);
				TEXTSW_DO_INSERT_MAKES_VISIBLE(view);
				return;
			}
		}
		SERVERTRACE((333, "\n"));
	}

	/* was no quick move - just a simple 'cut' */
	/* First: the same as 'copy' ... */

	SERVERTRACE((333, "before textsw_new_sel_copy\n"));
	textsw_new_sel_copy(view, ev);
	SERVERTRACE((333, "after textsw_new_sel_copy\n"));

	/* Second: delete the selected stuff */
	ev_get_selection(priv->views, &first, &lpo, EV_SEL_PRIMARY);
	if (lpo > first) {
		if (priv->smart_word_handling
			&& priv->select_is_word[TSW_SEL_CLIPBOARD])
		{
			char tmp[2];
			int buf_len;

			if (first > 0) {
				buf_len = textsw_es_read(priv->views->esh, tmp, first-1, first);
				tmp[buf_len] = '\0';
				if (tmp[0] == ' ') {
					textsw_erase(TEXTSW_PUBLIC(priv), first-1, lpo);
					return;
				}
			}

			buf_len = textsw_es_read(priv->views->esh, tmp, lpo, lpo+1);
			tmp[buf_len] = '\0';
			if (tmp[0] == ' ') {
				textsw_erase(TEXTSW_PUBLIC(priv), first, lpo+1);
				return;
			}
		}

		textsw_erase(TEXTSW_PUBLIC(priv), first, lpo);
	}
}

static void insert_blank_at(Textsw_private priv, Es_index oldpos, long pos)
{
	textsw_set_insert(priv, (Es_index)pos);
	textsw_insert(TEXTSW_PUBLIC(priv), " ", 1);
	textsw_set_insert(priv, oldpos);
}

Pkg_private Es_index textsw_smart_word_handling(Textsw_private priv,
				int is_word, char *wbuf, Es_index pos, long length)
{
	Es_index selpos;

	if (! is_word) return pos;
	if (! priv->smart_word_handling) return pos;

	selpos = pos;

	if (wbuf[0] == '\0') {
		/* insert at the very beginning */
		if (wbuf[1] != ' ') insert_blank_at(priv, pos, pos+length);
	}
	else if (wbuf[0] == '\n') {
		/* insert at a line start */
		if (wbuf[1] != ' ') insert_blank_at(priv, pos, pos+length);
	}
	else if (wbuf[0] == ' ') {
		/* insert after a blank */
		if (wbuf[1] != ' ') insert_blank_at(priv, pos, pos+length);
	}
	else if (ispunct(wbuf[0])) {
		/* insert after a full stop or comma etc */
		if (wbuf[1] != ' ') insert_blank_at(priv, pos, pos+length);
	}
	else {
		if (wbuf[1] == ' ') {
			insert_blank_at(priv, pos, (long)pos);
			++selpos;
		}
		else if (wbuf[1] == '\n') {
			insert_blank_at(priv, pos, (long)pos);
			++selpos;
		}
	}

	return selpos;
}

Pkg_private int textsw_new_sel_paste(Textsw_view_private vp, Event *ev,
									int canbequick)
{
	Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(vp);
	unsigned long length;
	int format, is_word = FALSE;
	char *string, wbuf[3];
	Es_index pos, temp, first, lpo, selpos;
	struct timeval tv;
	Textsw tsw = TEXTSW_PUBLIC(priv);
	long str_len;

	if (ev) tv = event_time(ev);
	else {
		server_set_timestamp(XV_SERVER_FROM_WINDOW(tsw),&tv,0L);
	}

	init_sel_req(priv);

	if (canbequick) {
		/* could have been a quick duplicate */
		xv_set(priv->sel_req,
				SEL_RANK, XA_SECONDARY,
				SEL_TYPE, XA_STRING,
				SEL_AUTO_COLLECT_INCR, TRUE,
				SEL_TIME, &tv,
				NULL);
		SERVERTRACE((333, "requesting SECONDARY STRING\n"));
		string = (char *)xv_get(priv->sel_req, SEL_DATA, &length, &format);
		SERVERTRACE((333,
				"requested SECONDARY STRING, len=%ld, fmt=%d, '%s'\n",
				length, format, string));

		if (length != SEL_ERROR) {

			/* yes, it WAS a quick duplicate */

			is_word = xv_get(priv->sel_req, SEL_IS_WORD_SELECTION);

			xv_set(priv->sel_req, SEL_TYPE, priv->atoms.selection_end, NULL);
			sel_post_req(priv->sel_req);
		}
		else {
			/* no, it was NOT a quick duplicate */
			if (string) xv_free(string);
			string = NULL;
		}
	}
	else {
		string = NULL;
	}

	SERVERTRACE((337, "\n"));
	if (! string) {
		xv_set(priv->sel_req,
				SEL_RANK_NAME, "CLIPBOARD",
				SEL_TYPE, XA_STRING,
				SEL_AUTO_COLLECT_INCR, TRUE,
				NULL);
		SERVERTRACE((333, "requesting CLIPBOARD STRING\n"));
		string = (char *)xv_get(priv->sel_req, SEL_DATA, &length, &format);
		SERVERTRACE((333, "requested CLIPBOARD STRING, len=%ld, fmt=%d, '%s'\n",
							length, format, string));

		if (length == SEL_ERROR) {
			if (string) xv_free(string);
			/* PASTE failed, what do we do now? */
/* 			xv_error(tsw,ERROR_PKG,TEXTSW,ERROR_STRING,"Paste failed",NULL); */

			/* taken from a Xol reference manual: */
			xv_set(tsw, WIN_ALARM, NULL);
			return FALSE;
		}

		is_word = xv_get(priv->sel_req, SEL_IS_WORD_SELECTION);
	}

	/* now, we have the pasted text (from whichever selection) in 'string' */

	/* if there is a primary selection, we have to handle it as
	 * "pending delete"...
	 */
	SERVERTRACE((337, "handling pending delete\n"));
	ev_get_selection(priv->views, &first, &lpo, EV_SEL_PRIMARY);
	if (lpo > first) {
		textsw_erase(tsw, first, lpo);
	}

	/* now similar to txt_move - dnd */
	/* pos has been in dnd calculated from the "drop coordinates
	 * We handle "paste" or "quick duplicate" here, so, it must be the
	 * caret position:
	 */
	pos = (Es_index)xv_get(tsw, TEXTSW_INSERTION_POINT);

	/* preparation of the "word paste" case */
	wbuf[0] = '\0';
	if (pos == 0) xv_get(tsw, TEXTSW_CONTENTS, pos, wbuf+1, 1);
	else xv_get(tsw, TEXTSW_CONTENTS, pos-1, wbuf, 2);

	ev_set(vp->e_view, EV_CHAIN_DELAY_UPDATE, FALSE, NULL);
	EV_SET_INSERT(priv->views, pos, temp);

	str_len = strlen(string);
	textsw_wrap_do_input(tsw, vp, (char *)string, &str_len);

	selpos = textsw_smart_word_handling(priv, is_word, wbuf, pos, str_len);

	EV_SET_INSERT(priv->views, (int)(selpos + str_len), temp);

	free((char *)string);
	TEXTSW_DO_INSERT_MAKES_VISIBLE(vp);
	SERVERTRACE((337, "end paste\n"));

	return TRUE;
}

static void start_seln_tracking(Textsw_private priv, Textsw_view_private view,
									Event*ev)
{
	textsw_start_seln_tracking(view, ev);

	/* now the priv->span_level should have been set */
	priv->select_is_word[priv->current_sel] = (priv->span_level ==EI_SPAN_WORD);

	SERVERTRACE((377, "%s: is_word[%d]=%d\n", __FUNCTION__,
				priv->current_sel, priv->select_is_word[priv->current_sel]));
}

Pkg_private void textsw_new_sel_select(Textsw_view_private view, Event *ev)
{
	Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);

	if (event_is_down(ev)) {
		Es_index pos;
		int delta;

		priv->track_state = 0;
		if (event_is_quick_move(ev) || event_is_quick_duplicate(ev))
			priv->current_sel = TSW_SEL_SECONDARY;
		else
			priv->current_sel = TSW_SEL_PRIMARY;

		pos = ev_resolve_xy(view->e_view, event_x(ev), event_y(ev));
		priv->dnd_last_click_x = ev->ie_locx;
		priv->dnd_last_click_y = ev->ie_locy;
		delta = (ev->ie_time.tv_sec-priv->last_point.tv_sec)*1000;
		delta += ev->ie_time.tv_usec / 1000;
		delta -= priv->last_point.tv_usec / 1000;

		if (priv->current_sel == TSW_SEL_PRIMARY) {
			Es_index first, last_plus_one;

			ev_get_selection(priv->views, &first, &last_plus_one,
					EV_SEL_PRIMARY);
			priv->point_down_within_selection = (
								(pos >= first && pos < last_plus_one)
								&& (delta >= priv->multi_click_timeout));

			if (!priv->point_down_within_selection) {
				start_seln_tracking(priv, view, ev);
			}
		}
		else {
			priv->track_state |= TXTSW_TRACK_SECONDARY;
			start_seln_tracking(priv, view, ev);
		}
	}
	else {
		if (priv->current_sel == TSW_SEL_SECONDARY) {
			Es_index first, last_plus_one;
			char *buffer;

			ev_get_selection(priv->views, &first, &last_plus_one,
					EV_SEL_SECONDARY);
    		buffer = (char *)xv_malloc((size_t)(last_plus_one - first + 1));
    		textsw_get_selection_as_string(priv, EV_SEL_SECONDARY, buffer,
					 (int)(last_plus_one - first + 1));
    		xv_set(priv->sel_item[TSW_SEL_SECONDARY],
						SEL_DATA, buffer,
						SEL_LENGTH, last_plus_one - first,
						NULL);
			xv_free(buffer);
		}
		else {
			if (priv->point_down_within_selection) {
				/* If point down is within a selection, then look at mouse up */
				start_seln_tracking(priv, view, ev);
				textsw_track_selection(view, ev);
				priv->point_down_within_selection = FALSE;
				/* textsw_invert_caret(priv);  */
			}
		}
	}
}

Pkg_private void textsw_new_sel_adjust(Textsw_view_private view, Event *ev)
{
	if (event_is_down(ev)) {
		start_seln_tracking(TSWPRIV_FOR_VIEWPRIV(view), view, ev);
	}
}

Pkg_private void textsw_new_sel_drag(Textsw_view_private view, Event *ev)
{
	Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
	Es_index first, last_plus_one, pos;

	ev_get_selection(priv->views, &first, &last_plus_one, EV_SEL_PRIMARY);
	pos = ev_resolve_xy(view->e_view, event_x(ev), event_y(ev));

	if (pos >= first && pos < last_plus_one) {
		if ((short)(abs(event_x(ev) - priv->dnd_last_click_x))
				> priv->drag_threshold
			|| (short)(abs(event_y(ev) - priv->dnd_last_click_y))
				> priv->drag_threshold)
		{
			Xv_server srv = XV_SERVER_FROM_WINDOW(TEXTSW_PUBLIC(priv));
			int is_copy = xv_get(srv,SERVER_EVENT_HAS_DUPLICATE_MODIFIERS,ev);
			textsw_do_drag_copy_move(view, ev, is_copy);
		}
	}
}

static void additional_items(Selection_owner so, ...)
{
	va_list args;
	Atom at;

	/* only to have them contained in TARGETS */
	va_start(args, so);

	for (at = va_arg(args, Atom); at != 0L; at = va_arg(args, Atom)) {
		xv_create(so, SELECTION_ITEM, SEL_TYPE, at, NULL);
	}
	va_end(args);
}

Pkg_private void textsw_new_selection_init(Textsw tsw)
{
	Textsw_private priv = TEXTSW_PRIVATE(tsw);
	Xv_server srv = XV_SERVER_FROM_WINDOW(tsw);

	priv->atoms.clipboard = xv_get(srv, SERVER_ATOM, "CLIPBOARD");
	priv->atoms.delete = xv_get(srv, SERVER_ATOM, "DELETE");
	priv->atoms.length = xv_get(srv, SERVER_ATOM, "LENGTH");
	priv->atoms.null = xv_get(srv, SERVER_ATOM, "NULL");
	priv->atoms.targets = xv_get(srv, SERVER_ATOM, "TARGETS");
	priv->atoms.text = xv_get(srv, SERVER_ATOM, "TEXT");
	priv->atoms.timestamp = xv_get(srv, SERVER_ATOM, "TIMESTAMP");
	priv->atoms.ol_sel_word = xv_get(srv, SERVER_ATOM, "_OL_SELECTION_IS_WORD");
	priv->atoms.dragdrop_ack = xv_get(srv, SERVER_ATOM, "_SUN_DRAGDROP_ACK");
	priv->atoms.dragdrop_done = xv_get(srv, SERVER_ATOM, "_SUN_DRAGDROP_DONE");
	priv->atoms.selection_end = xv_get(srv, SERVER_ATOM, "_SUN_SELECTION_END");
	priv->atoms.seln_is_readonly = xv_get(srv, SERVER_ATOM, "_SUN_SELN_IS_READONLY");
	priv->atoms.plain = xv_get(srv, SERVER_ATOM, "text/plain");
	priv->atoms.XVIEW_CLASS_TARGET  = xv_get(srv, SERVER_ATOM, "_DRA_XVIEW_CLASS");

	priv->sel_owner[TSW_SEL_PRIMARY] =
					xv_create(tsw, SELECTION_OWNER,
							SEL_CONVERT_PROC, text_convert_proc,
							SEL_LOSE_PROC, text_lose_proc,
							NULL);

	priv->sel_item[TSW_SEL_PRIMARY] =
					xv_create(priv->sel_owner[TSW_SEL_PRIMARY], SELECTION_ITEM,
							SEL_COPY, SEL_COPY_BLOCKED,
							NULL);
	additional_items(priv->sel_owner[TSW_SEL_PRIMARY],
							priv->atoms.ol_sel_word,
							priv->atoms.delete,
							NULL);

	priv->sel_owner[TSW_SEL_SECONDARY] =
					xv_create(tsw, SELECTION_OWNER,
							SEL_CONVERT_PROC, text_convert_proc,
							SEL_LOSE_PROC, text_lose_proc,
							SEL_RANK, XA_SECONDARY,
							NULL);

	priv->sel_item[TSW_SEL_SECONDARY] =
				xv_create(priv->sel_owner[TSW_SEL_SECONDARY], SELECTION_ITEM,
							SEL_COPY, SEL_COPY_BLOCKED,
							NULL);
	additional_items(priv->sel_owner[TSW_SEL_SECONDARY],
							priv->atoms.ol_sel_word,
							priv->atoms.selection_end,
							priv->atoms.seln_is_readonly,
							priv->atoms.delete,
							NULL);

	priv->sel_owner[TSW_SEL_CLIPBOARD] =
					xv_create(tsw, SELECTION_OWNER,
							SEL_CONVERT_PROC, text_convert_proc,
							SEL_LOSE_PROC, text_lose_proc,
							SEL_RANK_NAME, "CLIPBOARD",
							NULL);

	priv->sel_item[TSW_SEL_CLIPBOARD] =
				xv_create(priv->sel_owner[TSW_SEL_CLIPBOARD], SELECTION_ITEM,
							SEL_COPY, SEL_COPY_BLOCKED,
							NULL);
	additional_items(priv->sel_owner[TSW_SEL_CLIPBOARD],
							priv->atoms.ol_sel_word,
							priv->atoms.seln_is_readonly,
							NULL);

	priv->sel_owner[TSW_SEL_CARET] =
					xv_create(tsw, SELECTION_OWNER,
							SEL_CONVERT_PROC, caret_convert_proc,
							SEL_LOSE_PROC, caret_lose_proc,
							SEL_RANK_NAME, "_SUN_SELN_CARET",
							NULL);
}

Pkg_private void textsw_new_selection_destroy(Textsw tsw)
{
	Textsw_private priv = TEXTSW_PRIVATE(tsw);

	xv_destroy(priv->sel_item[TSW_SEL_PRIMARY]);
	xv_destroy(priv->sel_item[TSW_SEL_SECONDARY]);
	xv_destroy(priv->sel_item[TSW_SEL_CLIPBOARD]);

	xv_destroy(priv->sel_owner[TSW_SEL_PRIMARY]);
	xv_destroy(priv->sel_owner[TSW_SEL_SECONDARY]);
	xv_destroy(priv->sel_owner[TSW_SEL_CLIPBOARD]);
	xv_destroy(priv->sel_owner[TSW_SEL_CARET]);

	if (priv->sel_req) xv_destroy(priv->sel_req);
}
