#ifndef lint
char     tty_es_c_sccsid[] = "@(#)tty_es.c 20.23 93/06/28 DRA: $Id: tty_es.c,v 4.3 2024/12/16 22:55:27 dra Exp $";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */

/*
 * Entity stream implementation for permitting veto of insertion into a piece
 * stream.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/signal.h>
#include <string.h>
#include <xview/ttysw.h>
#include <xview/termsw.h>
#include <xview_private/primal.h>
#include <xview_private/es.h>
#include <xview_private/tty_impl.h>
#include <xview_private/term_impl.h>
#include <xview_private/portable.h>
#include <xview_private/i18n_impl.h>

static Es_index ts_replace(Es_handle esh, Es_index last_plus_one, int count,
#ifdef  OW_I18N
    CHAR            *buf,
#else
    char  *buf,
#endif
	int *count_used);
static int ts_set(Es_handle esh, Attr_attribute *attr_argv);

static struct es_ops *ps_ops;
static struct es_ops ts_ops;

#define	iwbp	ttysw->ttysw_ibuf.cb_wbp
#define	irbp	ttysw->ttysw_ibuf.cb_rbp
#define	iebp	ttysw->ttysw_ibuf.cb_ebp
#define	ibuf	ttysw->ttysw_ibuf.cb_buf
#define	owbp	ttysw->ttysw_obuf.cb_wbp
#define	orbp	ttysw->ttysw_obuf.cb_rbp

Pkg_private Xv_opaque
ts_create(ttysw, original, scratch)
    Ttysw          *ttysw;
    Es_handle       original, scratch;
{
    extern Es_handle ps_create(Xv_opaque, Es_handle, Es_handle);
    Es_handle       piece_stream;

    piece_stream = ps_create((Xv_opaque)ttysw, original, scratch);
    if (piece_stream) {
	if (ps_ops == 0) {
	    ps_ops = piece_stream->ops;
	    ts_ops = *ps_ops;
	    ts_ops.replace = ts_replace;
	    ts_ops.set = ts_set;
	}
	piece_stream->ops = &ts_ops;
    }
    return ((Xv_opaque) piece_stream);
}

#define NO_LOCAL_ECHO(_termsw, _textsw, _private, _count) \
	   (!(_termsw)->cooked_echo \
	&& (!(_termsw)->doing_pty_insert) \
	&& ((_termsw)->append_only_log \
	|| ((_count) > 0 \
	&&  es_get_position((_private)) \
	==  textsw_find_mark_i18n((_textsw), (_termsw)->pty_mark) )))

static Es_index ts_replace(Es_handle esh, Es_index last_plus_one, int count,
#ifdef  OW_I18N
    CHAR            *buf,
#else
    char  *buf,
#endif
	int *count_used)

{
    register Ttysw_private ttysw = (Ttysw_private)
    es_get(esh, ES_CLIENT_DATA);
    Textsw          textsw = TEXTSW_FROM_TTY(ttysw);
    Termsw_view_handle termsw_view = TERMSW_VIEW_PRIVATE_FROM_TTY_PRIVATE(ttysw);
    Ttysw_view_handle ttysw_view = TTY_VIEW_PRIVATE_FROM_ANY_VIEW(TERMSW_VIEW_PUBLIC(termsw_view));
    register Termsw_folio folio = TERMSW_FOLIO_FROM_TERMSW_VIEW_HANDLE(termsw_view);

    /*
     * if in ![cooked, echo] mode and the caret is at the pty mark, and the
     * operation is an insertion, then don't locally echo insertions.
     */
    if (NO_LOCAL_ECHO(folio, textsw, esh, count)) {
	/* copy buf into iwbp */
#ifdef  OW_I18N
        XV_BCOPY(buf, iwbp, MIN(count*sizeof(CHAR), (iebp - iwbp)*sizeof(CHAR)));
#else
	XV_BCOPY(buf, iwbp, (size_t)MIN(count, iebp - iwbp));
#endif
	iwbp += MIN(count, iebp - iwbp);
	ttysw_reset_conditions(ttysw_view);
	(void) es_set(esh, ES_STATUS, ES_REPLACE_DIVERTED, NULL);
	return (ES_CANNOT_SET);
    }
    return (ps_ops->replace(esh, last_plus_one, count, buf, count_used));
}

static int ts_set(Es_handle esh, Attr_attribute *attr_argv)
{
    Ttysw_private ttysw = (Ttysw_private) es_get(esh, ES_CLIENT_DATA);
    Textsw          textsw = TEXTSW_FROM_TTY(ttysw);
    Termsw_view_handle termsw_view = TERMSW_VIEW_PRIVATE_FROM_TEXTSW(textsw);
    Ttysw_view_handle ttysw_view = TTY_VIEW_PRIVATE_FROM_ANY_VIEW(TERMSW_VIEW_PUBLIC(termsw_view));
    Termsw_folio folio = TERMSW_FOLIO_FROM_TERMSW_VIEW_HANDLE(termsw_view);
    Attr_attribute *attrs;
    Es_handle       to_insert;
    int           result;

    /* do this only if we're not in cooked echo mode */
    for (attrs = attr_argv; *attrs; attrs = attr_next(attrs)) {
	if ((Es_attribute) * attrs == ES_HANDLE_TO_INSERT) {
	    to_insert = (Es_handle) attrs[1];
	    if (NO_LOCAL_ECHO(folio, textsw, esh,
			      es_get_length(to_insert))) {
		(void) es_set_position(to_insert, 0);
		/* Really should loop, in case esh > iebp-iwbp */
		es_read(to_insert, (int)(iebp - iwbp), iwbp, &result);
		iwbp += result;
		ttysw_reset_conditions(ttysw_view);
		ATTR_CONSUME(*attrs);
	    }
	}
    }
    return (ps_ops->set(esh, attr_argv));
}
