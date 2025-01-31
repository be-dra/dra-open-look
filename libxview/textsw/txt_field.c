#ifndef lint
char     txt_field_c_sccsid[] = "@(#)txt_field.c 20.31 93/06/28 DRA: $Id: txt_field.c,v 4.3 2024/12/22 08:44:43 dra Exp $";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Procedures to do field matching in text subwindows.
 */

#include <string.h>
#include <xview_private/primal.h>
#include <xview_private/txt_impl.h>
#include <xview_private/ev_impl.h>
#include <xview_private/txt_18impl.h>
#include <sys/time.h>
#include <xview/panel.h>
#include <xview/wmgr.h>
#include <xview/frame.h>
#include <xview/win_struct.h>
#include <xview/win_screen.h>
#define    ONE_FIELD_LENGTH     2
#define    MAX_SYMBOLS		8
#define    NUM_OF_COL		2
#define    MAX_STR_LENGTH       1024

static void textsw_get_match_symbol(CHAR *buf, unsigned buf_len,CHAR *match_buf,
								unsigned *match_buf_len, unsigned *direction);

Pkg_private int textsw_match_selection_and_normalize(Textsw_view_private view, CHAR *start_marker, unsigned field_flag);

#ifdef OW_I18N

static wchar_t l_curly_brace[] = { '{', 0 };
static wchar_t l_paren[] = { '(', 0 };
static wchar_t dbl_quote[] = { '"', 0 };
static wchar_t sgl_quote[] = { '\'', 0 };
static wchar_t accent_grave[] = { '`', 0 };
static wchar_t l_square_brace[] = { '[', 0 };
static wchar_t bar_gt[] = { '|', '>', 0 };
static wchar_t open_comment[] = { '/', '*', 0 };

static wchar_t r_curly_brace[] = { '}', 0 };
static wchar_t r_paren[] = { ')', 0 };
static wchar_t r_square_brace[] = { ']', 0 };
static wchar_t lt_bar[] = { '<', '|', 0 };
static wchar_t close_comment[] = { '*', '/', 0 };

static CHAR     *match_table[NUM_OF_COL][MAX_SYMBOLS] =
{{ l_curly_brace, l_paren, dbl_quote, sgl_quote, accent_grave, l_square_brace,
   bar_gt, open_comment,},
 { r_curly_brace, r_paren, dbl_quote, sgl_quote, accent_grave, r_square_brace,
   lt_bar, close_comment,}};
#else /* OW_I18N */
static char     *match_table[NUM_OF_COL][MAX_SYMBOLS] =
{{"{", "(", "\"", "'", "`", "[", "|>", "/*",},
{"}", ")", "\"", "'", "`", "]", "<|", "*/",}};
#endif /* OW_I18N */




static int check_selection(CHAR *buf, unsigned buf_len, Es_index *first, Es_index *last_plus_one, CHAR *marker1, unsigned marker1_len, unsigned field_flag)
{
    int             do_search;

    if ((field_flag == TEXTSW_FIELD_FORWARD) ||
	(field_flag == TEXTSW_DELIMITER_FORWARD)) {
	if (buf_len >= marker1_len) {
	    if (STRNCMP(buf, marker1, (size_t)marker1_len) == 0) {

		if (buf_len >= (marker1_len * 2)) {	/* Assuming open and
							 * close markers have
							 * the same size */
		    CHAR            marker2[3];
		    unsigned             marker2_len;
		    unsigned        direction;

		    buf = buf + (buf_len - marker1_len);
		    (void) textsw_get_match_symbol(marker1, marker1_len,
					 marker2, &marker2_len, &direction);
		    if ((STRNCMP(buf, marker2, (size_t)marker2_len) == 0) ||
			(buf_len >= (MAX_STR_LENGTH - 1))) {	/* Buffer overflow case */
			if (*first == *last_plus_one) {
			    *first = (*first - buf_len);
			}
			*first = *first + marker1_len;
			do_search = TRUE;
		    }
		}
	    } else {
		do_search = TRUE;
	    }
	} else {
	    do_search = TRUE;
	}
    } else if ((field_flag & TEXTSW_FIELD_BACKWARD) ||
	       (field_flag & TEXTSW_DELIMITER_BACKWARD)) {
	if (buf_len >= marker1_len) {
	    CHAR           *temp;

	    temp = buf + (buf_len - marker1_len);
	    if (STRNCMP(temp, marker1, (size_t)marker1_len) == 0) {
		if (buf_len >= (marker1_len * 2)) {
		    CHAR            marker2[3];
		    unsigned marker2_len;
		    unsigned        direction;

		    (void) textsw_get_match_symbol(marker1, marker1_len,
					 marker2, &marker2_len, &direction);
		    if (STRNCMP(buf, marker2, (size_t)marker2_len) == 0) {
			*last_plus_one = *last_plus_one - marker2_len;
			*first = *last_plus_one;
			do_search = TRUE;
		    }
		}
	    } else {
		if (buf_len >= (MAX_STR_LENGTH - 1)) {	/* Buffer overflow case */
		    *last_plus_one = *last_plus_one - marker1_len;
		    *first = *last_plus_one;
		}
		do_search = TRUE;
	    }
	} else {
	    do_search = TRUE;
	}

    }
    return (do_search);
}

static Es_index get_start_position(Textsw_private priv, Es_index *first, Es_index *last_plus_one, CHAR *symbol1, unsigned symbol1_len, CHAR *symbol2, unsigned symbol2_len, unsigned field_flag, int do_search)
{
    Es_index        start_at = ES_CANNOT_SET;


    unsigned        direction = ((field_flag == TEXTSW_FIELD_FORWARD) ||
				 (field_flag == TEXTSW_DELIMITER_FORWARD)) ?
    EV_FIND_DEFAULT : EV_FIND_BACKWARD;

    if (do_search) {
	(void) textsw_find_pattern(priv, first, last_plus_one,
				   symbol1, symbol1_len, direction);
    }
    switch (field_flag) {
      case TEXTSW_NOT_A_FIELD:
      case TEXTSW_FIELD_FORWARD:
      case TEXTSW_DELIMITER_FORWARD:
	start_at = *first;
	break;
      case TEXTSW_FIELD_ENCLOSE:
      case TEXTSW_DELIMITER_ENCLOSE:{
	    unsigned        dummy;

	    if (symbol2_len == 0)
		textsw_get_match_symbol(symbol1, symbol1_len,
					symbol2, &symbol2_len, &dummy);

	    start_at = ev_find_enclose_end_marker(priv->views->esh,
						  symbol1, symbol1_len,
						  symbol2, symbol2_len,
						  *first);
	    break;
	}
      case TEXTSW_FIELD_BACKWARD:
      case TEXTSW_DELIMITER_BACKWARD:{
	    start_at = ((*first == ES_CANNOT_SET)
			? ES_CANNOT_SET : *last_plus_one);
	    break;
	}
    }

    return (start_at);
}

static void textsw_get_match_symbol(CHAR *buf, unsigned buf_len, CHAR *match_buf, unsigned *match_buf_len, unsigned *direction)
{
	int i, j, index;

	*match_buf_len = 0;
	*direction = EV_FIND_DEFAULT;
	match_buf[0] = '\0';

	for (i = 0; i < NUM_OF_COL; i++) {
		for (j = 0; j < MAX_SYMBOLS; j++) {
			if (STRNCMP(buf, match_table[i][j], (size_t)buf_len) == 0) {
				index = ((i == 0) ? 1 : 0);
				STRCPY(match_buf, match_table[index][j]);
				*match_buf_len = STRLEN(match_buf);
				if (index == 0)
					*direction = EV_FIND_BACKWARD;
				return;
			}
		}
	}
}


static Es_index textsw_match_same_marker(Textsw_private priv, CHAR *marker,
					unsigned marker_len, Es_index start_pos, unsigned direction)
{
    Es_index        first, last_plus_one;
    Es_index        result_pos;
    int             plus_or_minus_one = (direction == EV_FIND_BACKWARD) ? -1 : 1;

    first = last_plus_one = start_pos + plus_or_minus_one;

    (void) textsw_find_pattern(priv, &first, &last_plus_one,
			       marker, marker_len, direction);

    result_pos = (direction == EV_FIND_BACKWARD) ? last_plus_one : first;
    if (result_pos == start_pos)
	result_pos = ES_CANNOT_SET;
    else if (result_pos != ES_CANNOT_SET)
	result_pos = result_pos + plus_or_minus_one;

    return (result_pos);
}

/* Caller must set *first to be position at which to start the search. */
static void textsw_match_field(Textsw_private textsw, Es_index *first,
				Es_index *last_plus_one, CHAR *symbol1, unsigned symbol1_len,
				CHAR *symbol2, unsigned symbol2_len, unsigned field_flag,
				int do_search)	/* If TRUE, is called by textsw_match_bytes */
{

    Es_handle       esh = textsw->views->esh;
    Es_index        start_at = *first;
    Es_index        result_pos;
    unsigned        direction = ((field_flag == TEXTSW_FIELD_FORWARD) ||
				 (field_flag == TEXTSW_DELIMITER_FORWARD)) ?
    EV_FIND_DEFAULT : EV_FIND_BACKWARD;


    start_at = get_start_position(textsw, first, last_plus_one,
				  symbol1, symbol1_len,
			       symbol2, symbol2_len, field_flag, do_search);

    if ((symbol1_len == 0) || (start_at == ES_CANNOT_SET)) {
	*first = ES_CANNOT_SET;
	return;
    }
    if (symbol2_len == 0)
	textsw_get_match_symbol(symbol1, symbol1_len,
				symbol2, &symbol2_len, &direction);

    if ((symbol2_len == 0) || (symbol2_len != symbol1_len)) {
	*first = ES_CANNOT_SET;
	return;
    }
    if ((direction == EV_FIND_BACKWARD) &&
	(field_flag == TEXTSW_NOT_A_FIELD)) {
	start_at = *last_plus_one;
    }
    if (STRNCMP(symbol1, symbol2, (size_t)symbol1_len) == 0) {

	direction = ((field_flag == TEXTSW_NOT_A_FIELD) ||
		     (field_flag == TEXTSW_FIELD_FORWARD) ||
		     (field_flag == TEXTSW_DELIMITER_FORWARD)) ?
	    EV_FIND_DEFAULT : EV_FIND_BACKWARD;
	result_pos = textsw_match_same_marker(textsw, symbol1, symbol1_len, start_at, direction);
    } else
	result_pos = ev_match_field_in_esh(esh, symbol1, symbol1_len,
					   symbol2, symbol2_len, start_at,
					   direction);

    if ((field_flag == TEXTSW_FIELD_FORWARD) ||
	(field_flag == TEXTSW_DELIMITER_FORWARD) ||
	((field_flag == TEXTSW_NOT_A_FIELD) && (direction != EV_FIND_BACKWARD))) {

	*first = start_at;
	*last_plus_one = ((result_pos >= start_at)
			  ? result_pos : ES_CANNOT_SET);
    } else {
	*first = ((result_pos <= start_at) ? result_pos : ES_CANNOT_SET);
	*last_plus_one = start_at;

    }

}

Pkg_private int textsw_match_field_and_normalize(Textsw_view_private view,
    Es_index *first, Es_index *last_plus_one, CHAR *buf1, unsigned buf_len1,
	unsigned field_flag, int do_search)
{

    register Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
    CHAR            buf2[MAX_STR_LENGTH];
    unsigned  buf_len2 = 0;
    register Es_index ro_bdry;
    register int    want_pending_delete;
    int             matched = FALSE;
    Es_index        save_first = *first;
    Es_index        save_last_plus_one = *last_plus_one;

    textsw_match_field(priv, first, last_plus_one,
			      buf1, (unsigned)buf_len1, buf2, buf_len2,
			      field_flag, do_search);

    if (((*first == save_first) && (*last_plus_one == save_last_plus_one)) ||
	((*first == ES_CANNOT_SET) || (*last_plus_one == ES_CANNOT_SET))) {
	(void) window_bell(XV_PUBLIC(view));
    } else {
	/*
	 * Unfortunately, textsw_possibly_normalize_and_set_selection will
	 * not honor request for pending-delete, so we call
	 * textsw_set_selection explicitly in such cases. WARNING!  However,
	 * we don't allow pending-delete if we overlap the read-only portion
	 * of the window.
	 */
	want_pending_delete = ((field_flag == TEXTSW_FIELD_FORWARD) ||
			       (field_flag == TEXTSW_FIELD_BACKWARD) ||
			       (field_flag == TEXTSW_FIELD_ENCLOSE));

	if (want_pending_delete) {
	    ro_bdry = TXTSW_IS_READ_ONLY(priv) ? *last_plus_one
		: textsw_read_only_boundary_is_at(priv);
	    if (*last_plus_one <= ro_bdry) {
		want_pending_delete = FALSE;
	    }
	}
	(void) textsw_possibly_normalize_and_set_selection(
			      VIEW_PUBLIC(view), *first, *last_plus_one,
			      (unsigned)((want_pending_delete) ? 0 : EV_SEL_PRIMARY));
	if (want_pending_delete) {
#ifdef OW_I18N
	    (void) textsw_set_selection_wcs(
#else
	    (void) textsw_set_selection(
#endif
			      TEXTSW_PUBLIC(priv), *first, *last_plus_one,
				      (EV_SEL_PRIMARY | EV_SEL_PD_PRIMARY));
	}
	(void) textsw_set_insert(priv, *last_plus_one);
	textsw_record_match(priv, field_flag, buf1);
	matched = TRUE;
    }
    return (matched);
}

Pkg_private int textsw_match_selection_and_normalize(Textsw_view_private view, CHAR *start_marker, unsigned field_flag)
{
	register Textsw_private textsw = TSWPRIV_FOR_VIEWPRIV(view);
	Es_index first, last_plus_one;
	CHAR buf[MAX_STR_LENGTH];
	unsigned str_length = MAX_STR_LENGTH;
	int do_search = TRUE;

	/*
	 * This procedure should only be called if field flag is set to
	 * TEXTSW_FIELD_FORWARD, TEXTSW_FIELD_BACKWARD, TEXTSW_DELIMITER_FORWARD,
	 * TEXTSW_DELIMITER_BACKWARD.
	 */

	if (textsw_get_selection(view, &first, &last_plus_one, NULL, 0)) {
		if ((last_plus_one - first) < MAX_STR_LENGTH)
			str_length = (last_plus_one - first);

#ifdef OW_I18N
		xv_get(VIEW_PUBLIC(view), TEXTSW_CONTENTS_WCS, first, buf,
				str_length);
#else
		xv_get(VIEW_PUBLIC(view), TEXTSW_CONTENTS, first, buf, str_length);
#endif

		if (str_length == MAX_STR_LENGTH)
			str_length--;

		buf[str_length] = '\0';

		if (field_flag == TEXTSW_NOT_A_FIELD) {
			if (str_length > ONE_FIELD_LENGTH) {
				(void)window_bell(XV_PUBLIC(view));
				return (FALSE);	/* Not a vaild marker size */
			}
			start_marker = buf;
			do_search = FALSE;
		}
		else {
			do_search = check_selection(buf, str_length, &first, &last_plus_one,
					start_marker, (unsigned)STRLEN(start_marker), field_flag);
		}
	}
	else {
		if (field_flag == TEXTSW_NOT_A_FIELD) {	/* Matching must have a
												 * selection */
			(void)window_bell(XV_PUBLIC(view));
			return (FALSE);
		}
		first = last_plus_one = EV_GET_INSERT(textsw->views);
	}


	return (textsw_match_field_and_normalize(view, &first, &last_plus_one,
					start_marker, (unsigned)STRLEN(start_marker), field_flag, do_search));

}

/*
 * If the pattern is matched, return the index where it is found, else return
 * -1.
 */
Xv_public int
#ifdef OW_I18N
textsw_match_wcs(abstract, first, last_plus_one, start_sym, start_sym_len,
		   end_sym, end_sym_len, field_flag)
    Textsw          abstract;	/* find in this textsw */
    Textsw_index   *first;	/* start here, return start of found pattern
				 * here */
    Textsw_index   *last_plus_one;	/* return end of found pattern */
    CHAR            *start_sym, *end_sym;	/* begin and end pattern */
    int             start_sym_len, end_sym_len;	/* patterns length */
    unsigned        field_flag;
#else
textsw_match_bytes(Textsw abstract,	/* find in this textsw */
    Textsw_index *first,	/* start here, return start of found pattern here */
    Textsw_index *last_plus_one,	/* return end of found pattern */
    CHAR *start_sym, /* begin and end pattern */
    int start_sym_len, /* patterns length */
    CHAR *end_sym,	/* begin and end pattern */
    int end_sym_len,	/* patterns length */
    unsigned field_flag)
#endif
{
	Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(VIEW_ABS_TO_REP(abstract));
	int save_first = *first;
	int save_last_plus_one = *last_plus_one;


	if ((field_flag == TEXTSW_DELIMITER_FORWARD) ||
			(field_flag == TEXTSW_FIELD_FORWARD)) {
		textsw_match_field(priv, first, last_plus_one,
				start_sym, (unsigned)start_sym_len,
				end_sym, (unsigned)end_sym_len, field_flag, TRUE);
	}
	else {
		textsw_match_field(priv, first, last_plus_one,
				end_sym, (unsigned)end_sym_len,
				start_sym, (unsigned)start_sym_len, field_flag,
				((field_flag == TEXTSW_DELIMITER_BACKWARD) ||
						(field_flag == TEXTSW_FIELD_BACKWARD)));
	}

	if ((*first == ES_CANNOT_SET) || (*last_plus_one == ES_CANNOT_SET)) {
		*first = save_first;
		*last_plus_one = save_last_plus_one;
		return -1;
	}
	else {
		return *first;
	}
}

#ifdef OW_I18N
/*
 * If the pattern is matched, return the index where it is found, else return
 * -1.
 */
Xv_public int
textsw_match_bytes(abstract, first, last_plus_one, start_sym, start_sym_len,
		   end_sym, end_sym_len, field_flag)
    Textsw          abstract;	/* find in this textsw */
    Textsw_index   *first;	/* start here, return start of found pattern
				 * here */
    Textsw_index   *last_plus_one;	/* return end of found pattern */
    char           *start_sym, *end_sym;	/* begin and end pattern */
    int             start_sym_len, end_sym_len;	/* patterns length */
    unsigned        field_flag;
{
    Textsw_private    priv = TSWPRIV_FOR_VIEWPRIV(VIEW_ABS_TO_REP(abstract));
    int             save_first = *first;
    int             save_last_plus_one = *last_plus_one;
    CHAR           *start_wsym, *end_wsym;	/* begin and end pattern */
    int             start_wsym_len, end_wsym_len; /* patterns length */
    int             big_len_flag;

    start_wsym = MALLOC(start_sym_len + 1);
    start_wsym_len = textsw_mbstowcs_by_mblen(start_wsym, start_sym,
					      &start_sym_len, &big_len_flag);
    start_wsym[start_wsym_len] = 0;
    /*
     * when original start_sym_len is bigger than length of string in
     * start_sym, do error retrun as the generic textsw's behavior.
     */
    if (big_len_flag) {
	free(start_wsym);
	return -1;
    }
    end_wsym = MALLOC(end_sym_len + 1);
    end_wsym_len = textsw_mbstowcs_by_mblen(end_wsym, end_sym,
					    &end_sym_len, &big_len_flag);
    end_wsym[end_wsym_len] = 0;
    if (big_len_flag) {
	free(start_wsym);
	free(end_wsym);
	return -1;
    }

    *first = textsw_wcpos_from_mbpos(priv, *first);
    if ((field_flag == TEXTSW_DELIMITER_FORWARD) ||
	(field_flag == TEXTSW_FIELD_FORWARD)) {
	textsw_match_field(priv, first, last_plus_one,
				  start_wsym, start_wsym_len,
				  end_wsym, end_wsym_len, field_flag, TRUE);
    } else {
	textsw_match_field(priv, first, last_plus_one,
				  end_wsym, end_wsym_len,
				  start_wsym, start_wsym_len, field_flag,
			       ((field_flag == TEXTSW_DELIMITER_BACKWARD) ||
				(field_flag == TEXTSW_FIELD_BACKWARD)));
    }
    free(start_wsym);
    free(end_wsym);

    if ((*first == ES_CANNOT_SET) || (*last_plus_one == ES_CANNOT_SET)) {
	*first = save_first;
	*last_plus_one = save_last_plus_one;
	return -1;
    } else {
	save_first = *first; /* save character based first */
	*first = textsw_mbpos_from_wcpos(priv, *first);
	*last_plus_one = textsw_mbpos_from_wcpos(priv, *last_plus_one);
	return *first;
    }
}
#endif /* OW_I18N */
