#ifndef lint
char     txt_find_c_sccsid[] = "@(#)txt_find.c 20.27 93/06/28 DRA: $Id: txt_find.c,v 4.9 2024/12/21 21:25:58 dra Exp $";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Procedures to do searching for patterns in text subwindows.
 */

#include <xview_private/primal.h>
#include <xview_private/txt_impl.h>
#include <xview_private/ev_impl.h>
#include <xview_private/txt_18impl.h>

Pkg_private void textsw_create_popup_frame(Textsw_view_private view, int popup_type);

Pkg_private void textsw_begin_find(Textsw_view_private view)
{
    textsw_begin_function(view, TXTSW_FUNC_FIND);
}

Pkg_private int textsw_end_find(Textsw_view_private view, int event_code, int x, int y)
{
	register Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
	Textsw abstract = VIEW_PUBLIC(view);

	if ((priv->func_state & TXTSW_FUNC_FIND) == 0)
		return (ES_INFINITY);
	if ((priv->func_state & TXTSW_FUNC_EXECUTE) == 0)
		goto Done;

	if (event_code == ACTION_REPLACE) {
		extern int SEARCH_POPUP_KEY;
		Frame base_frame = (Frame) xv_get(abstract, WIN_FRAME);
		Frame popup = (Frame) xv_get(base_frame, XV_KEY_DATA,
				SEARCH_POPUP_KEY);

		if (popup) {
			(void)textsw_get_and_set_selection(popup, view,
					(int)TEXTSW_MENU_FIND_AND_REPLACE);
		}
		else {
			(void)textsw_create_popup_frame(view,
					(int)TEXTSW_MENU_FIND_AND_REPLACE);
		}

	}
	else {
		textsw_find_selection_and_normalize(view, x, y,
				(long unsigned)(TFSAN_CLIPBOARD_ALSO | (
								(event_code ==
										ACTION_FIND_BACKWARD) ? TFSAN_BACKWARD :
								0)));
	}
  Done:
	textsw_end_function(view, TXTSW_FUNC_FIND);
	return (0);
}

Pkg_private void textsw_find_selection_and_normalize(Textsw_view_private view, int x,int y, long unsigned options)
{
	Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
	Es_index f, lpo, oldf;
	char *mybuf;
	unsigned len, flags;
	int	buf_len;

	if (! textsw_get_local_selected_text(priv, EV_SEL_PRIMARY, &f, &lpo,
					&mybuf, &buf_len))
	{
		xv_set(TEXTSW_PUBLIC(priv), WIN_ALARM, NULL);
		return;
	}
	len = buf_len;
	if (options & TFSAN_BACKWARD) {
		flags = EV_FIND_BACKWARD;
	}
	else {
		flags = EV_FIND_DEFAULT;
	}
	oldf = f;
	textsw_find_pattern_and_normalize(view, x, y, &f, &lpo, mybuf, len, flags);
	if (oldf == f) {
		window_bell(XV_PUBLIC(view));
	}
}

/* Caller must set *first to be position at which to start the search. */
Pkg_private void textsw_find_pattern(Textsw_private textsw, Es_index *first,
						Es_index *last_plus_one, CHAR *buf, unsigned buf_len,
						unsigned flags)
{
	Es_handle esh = textsw->views->esh;
	Es_index start_at = *first;
	int i;

	/* this was need for ACTION_FIND... to work without sel svc - strange */
	if (flags & EV_FIND_BACKWARD) --start_at;
	else ++start_at;

	if (buf_len == 0) {
		*first = ES_CANNOT_SET;
		return;
	}
	for (i = 0; i < 2; i++) {
		ev_find_in_esh(esh, buf, (int)buf_len, start_at, 1, (int)flags,
				first, last_plus_one);
		if (*first != ES_CANNOT_SET) {
			return;
		}
		if (flags & EV_FIND_BACKWARD) {
			Es_index length = es_get_length(esh);

			if (start_at == length) {
				return;
			}
			start_at = length;
		}
		else {
			if (start_at == 0) {
				return;
			}
			start_at = 0;
		}
	}
}

/* Caller must set *first to be position at which to start the search. */
/* ARGSUSED */
Pkg_private void textsw_find_pattern_and_normalize(Textsw_view_private view, int x,int y, Es_index *first, Es_index *last_plus_one, CHAR *buf, unsigned buf_len, unsigned flags)

{
	register Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
	Es_index pattern_index;

	pattern_index = (flags & EV_FIND_BACKWARD) ? *first : (*first - buf_len);
	textsw_find_pattern(priv, first, last_plus_one, buf, buf_len, flags);
	if (*first == ES_CANNOT_SET) {
		(void)window_bell(XV_PUBLIC(view));
	}
	else {
		if (*first == pattern_index) window_bell(XV_PUBLIC(view));
		textsw_possibly_normalize_and_set_selection(VIEW_PUBLIC(view),
				*first, *last_plus_one, (EV_SEL_PRIMARY | EV_SEL_PD_PRIMARY));
		(void)textsw_set_insert(priv, *last_plus_one);
		textsw_record_find(priv, buf, (int)buf_len, (int)flags);
	}
}

/*
 * If the pattern is found, return the index where it is found, else return
 * -1.
 */
Xv_public int
#ifdef OW_I18N
textsw_find_wcs(abstract, first, last_plus_one, buf, buf_len, flags)
    Textsw          abstract;	/* find in this textsw */
    Es_index       *first;	/* start here, return start of found pattern here */
    Es_index       *last_plus_one;	/* return end of found pattern */
    CHAR           *buf;	/* pattern */
    unsigned        buf_len;	/* pattern length */
    unsigned        flags;	/* 0=forward, !0=backward */
#else
textsw_find_bytes(Textsw abstract,	/* find in this textsw */
    Textsw_index   *first,	/* start here, return start of found pattern here */
    Textsw_index   *last_plus_one,	/* return end of found pattern */
    CHAR           *buf,	/* pattern */
    unsigned        buf_len,	/* pattern length */
    unsigned        flags)	/* 0=forward, !0=backward */
#endif
{
    int save_first = *first;
    Textsw_private priv;

	if (xv_get(abstract, XV_IS_SUBTYPE_OF, OPENWIN)) {
		priv = TEXTSW_PRIVATE(abstract);
	}
	else {
    	priv = TSWPRIV_FOR_VIEWPRIV(VIEW_ABS_TO_REP(abstract));
		abort();
	}

    textsw_find_pattern(priv, first, last_plus_one, buf, buf_len,
			(unsigned) (flags ? EV_FIND_BACKWARD : 0));
    if (*first == ES_CANNOT_SET) {
	*first = save_first;
	return -1;
    } else {
	return *first;
    }
}

#ifdef OW_I18N
/*
 * If the pattern is found, return the index where it is found, else return
 * -1.
 */
Xv_public int
textsw_find_bytes(abstract, first, last_plus_one, buf, buf_len, flags)
    Textsw          abstract;	/* find in this textsw */
    Es_index       *first;	/* start here, return start of found pattern
				 * here */
    Es_index       *last_plus_one;	/* return end of found pattern */
    char           *buf;	/* pattern */
    unsigned        buf_len;	/* pattern length */
    unsigned        flags;	/* 0=forward, !0=backward */
{
    Textsw_private    priv = TSWPRIV_FOR_VIEWPRIV(VIEW_ABS_TO_REP(abstract));
    int             save_first = *first;
    int             save_last_plus_one = *last_plus_one;
    int             unconverted_bytes = buf_len;
    int             wbuf_len, big_len_flag;
    CHAR            *wbuf = MALLOC(buf_len + 1);

    wbuf_len = textsw_mbstowcs_by_mblen(wbuf, buf,
					&unconverted_bytes, &big_len_flag);
    wbuf[wbuf_len] = 0;
    /*
     * When buf_len is bigger than string length in buf,
     * do error retrun as the generic textsw's behavior.
     */
    if (big_len_flag) {
	free(wbuf);
	return -1;
    }
    *first = textsw_wcpos_from_mbpos(priv, *first);
    textsw_find_pattern(priv, first, last_plus_one, wbuf, wbuf_len,
			(unsigned) (flags ? EV_FIND_BACKWARD : 0));
    free(wbuf);
    if (*first == ES_CANNOT_SET) {
	*first = save_first;
	*last_plus_one = save_last_plus_one;
	return -1;
    } else {
	*first = textsw_mbpos_from_wcpos(priv, *first);

	/* dosen't use textsw_mbpos_from_wcpos() for the performance. */
	*last_plus_one = *first + buf_len - unconverted_bytes;
	return *first;
    }
}
#endif /* OW_I18N */
