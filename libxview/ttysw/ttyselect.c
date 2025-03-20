#ifndef lint
char     ttyselect_c_sccsid[] = "@(#)ttyselect.c 20.46 93/06/28 DRA $Id: ttyselect.c,v 4.35 2025/03/19 21:33:50 dra Exp $";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */

#include <stdio.h>
#include <ctype.h>

#include <xview/defaults.h>
#include <xview/rect.h>
#include <xview/rectlist.h>
#include <xview/pkg.h>
#include <xview/attrol.h>
#include <xview/ttysw.h>
#include <xview_private/tty_impl.h>
#include <xview_private/svr_impl.h>
#include <xview_private/charimage.h>
#include <xview_private/charscreen.h>

extern char *xv_app_name;

typedef void (*tty_enumeration_t)(Ttysw_private ttysw, int start,int finish, int row, void *count, struct ttyselection *);

#ifdef  OW_I18N
static void     ttycountbytes();
static Seln_result ttysel_copy_out_wchar();
#endif

#define	SEL_NULLPOS	-1

#define	TSE_NULL_EXTENT	{SEL_NULLPOS, SEL_NULLPOS}

static struct textselpos tse_null_extent = TSE_NULL_EXTENT;
static struct ttyselection ttysw_nullttysel = {
    0, 0, SEL_CHAR, FALSE, 0, TSE_NULL_EXTENT, TSE_NULL_EXTENT, {0, 0}
};

static struct timeval maxinterval = {0, 400000};	/* XXX - for now */

static int ttysel_key = 0;

/*
 * init_client:
 */
Pkg_private void ttysel_init_client(Ttysw_private ttysw)
{
	if (!ttysw_getopt(ttysw, TTYOPT_SELSVC)) {
		return;
	}
	SERVERTRACE((333, "%s:\n", __FUNCTION__));
	ttysw->sels[TTY_SEL_PRIMARY] = ttysw_nullttysel;
	ttysw->sels[TTY_SEL_SECONDARY] = ttysw_nullttysel;
	ttysw->sels[TTY_SEL_CLIPBOARD] = ttysw_nullttysel;
}

Pkg_private void ttysel_destroy(Ttysw_private priv)
{
	int i;

	if (priv->selbuffer) xv_free(priv->selbuffer);
	for (i = 0; i < NBR_TTY_SELECTIONS; i++) {
    	xv_destroy(priv->sel_owner[i]);
	}
}

/*
 *	this little chunk of code is used to load and check against
 *	the known delimiters that are available in the ISO world.
 *	the user has a choice of setting the delimiters that are
 *	used depending on the locale. there is another lovely chunk
 *	of this kind of code over in ei_text.c [jcb 5/17/90]
 */
#define	DELIMITERS	 " \t,.:;?!\'\"`*/-+=(){}[]<>\\|~@#$%^&"

static	short		delim_init	= FALSE;
static	char		delim_table[256];

static	void init_delim_table(void)
{
    char	*delims;
    char	delim_chars[256];

    /* changing the logic to use the delimiters that are in the array rather than
       those characters that are simply isalnum(). this is so the delimiters can
       be expanded to include those which are in the ISO latin specification from
       the user defaults.
     */

    /* get the string from the defaults if one exists. */
    delims	= (char*)defaults_get_string( "text.delimiterChars",
				       "Text.DelimiterChars", DELIMITERS );

    /* print the string into an array to parse the potential octal/special characters */
    sprintf( delim_chars, delims );

    /* mark off the delimiters specified */
    for( delims = delim_chars; *delims; delims++ ) {
/*	    printf("%c(%o)", (isprint(*delims) ? *delims : ' '), (int)*delims ); */
	    delim_table[(int)*delims]	= TRUE;
    }
/*    printf("\n"); */

    delim_init	= TRUE;
}


#ifdef OW_I18N
static void ttysel_resolve(Ttysw_private ttysw, tb, te, level, event)
    register struct textselpos *tb, *te;
    int             level;
    struct inputevent *event;
{
    register CHAR  *line;
    int             cwidth;
    int             offset;

    tb->tsp_row = y_to_row(event->ie_locy);
    if (tb->tsp_row >= ttysw->ttysw_bottom)
        tb->tsp_row = MAX(0, ttysw->ttysw_bottom - 1);
    else
    if( tb->tsp_row < 0 )
        tb->tsp_row = 0;

    line = image[tb->tsp_row];
    tb->tsp_col = x_to_col(event->ie_locx);

    if (tb->tsp_col > (int)LINE_LENGTH(line))
        tb->tsp_col = LINE_LENGTH(line);

    *te = *tb;
    switch (level) {
      case SEL_CHAR:
            tty_column_wchar_type( tb->tsp_col, tb->tsp_row, &cwidth, &offset );
            tb->tsp_col -= offset;
            tty_column_wchar_type( te->tsp_col, te->tsp_row, &cwidth, &offset );
            te->tsp_col +=( cwidth - offset -1 );
        break;
      case SEL_WORD:{
            register int     chr,col;
            CHAR             wchr;
            register unsigned char match_mode;
               /*
                *    It is no use if we start at second or latter column of a
                *    character. So we adjust the starting position to
                *    get a correct match_mode.
                */
            tty_column_wchar_type( te->tsp_col, te->tsp_row, &cwidth, &offset );
            te->tsp_col -= offset;
#ifdef  DELIM_TABLE_USE
        /*
         *      SUNDAE;  This compile switch is used if you want to
         *      use delim_table which is created ininit_delim_table
         *      to distinguish a word. When this switch is off,
         *      you use wchar_type() to distinguish a word.
         */
            if( delim_init == FALSE )
                    init_delim_table();
            match_mode  = delim_table[line[te->tsp_col]];
#else
            match_mode  = (unsigned char)wchar_type( &line[te->tsp_col] );
#endif

            for (col = te->tsp_col; col < (int)LINE_LENGTH(line); col++) {
#ifdef  DELIM_TABLE_USE
                chr = (int)line[col];
                if( (CHAR)chr == TTY_NON_WCHAR )
                        continue;
                if ( delim_table[chr] != match_mode )
                        break;
#else
                wchr = line[col];
                if( wchr == TTY_NON_WCHAR )
                        continue;
                if ( wchar_type(&wchr) != match_mode )
                        break;
#endif
            }
            /*
             *  Here, col surely points to the 1st column of a character.
             *  So we can just step one column backwards to get to the
             *  word boundary.
             */
            te->tsp_col = MAX(col - 1, tb->tsp_col);
            for (col = tb->tsp_col; col >= 0; col--) {
#ifdef  DELIM_TABLE_USE
                chr = (int)line[col];
                if( (CHAR)chr == TTY_NON_WCHAR )
                        continue;
                if ( delim_table[chr] != match_mode )
                        break;
#else
                wchr = line[col];
                if( wchr == TTY_NON_WCHAR )
                        continue;
                if ( wchar_type(&wchr) != match_mode )
                        break;
#endif
            }
            /*
             *  We can be sure that current position is the first column
             *  of a character. So offset must be zero.
             */
            tty_column_wchar_type( col, tb->tsp_row, &cwidth, &offset );
            tb->tsp_col = MIN(col + cwidth, te->tsp_col);
            break;
        }
      case SEL_LINE:
        tb->tsp_col = 0;
        te->tsp_col = LINE_LENGTH(line) - 1;
        break;
      case SEL_PARA:{
            register int    row;

            for (row = tb->tsp_row; row >= ttysw->ttysw_top; row--)
                if (LINE_LENGTH(image[row]) == 0)
                    break;
            tb->tsp_row = MIN(tb->tsp_row, row + 1);
            tb->tsp_col = 0;
            for (row = te->tsp_row; row < ttysw->ttysw_bottom; row++)
                if (LINE_LENGTH(image[row]) == 0)
                    break;
            te->tsp_row = MAX(te->tsp_row, row - 1);
            te->tsp_col = LINE_LENGTH(image[te->tsp_row]) - 1;
            break;
        }
    }
}
#else	/* OW_I18N */
static void ttysel_resolve(Ttysw_private ttysw, struct textselpos *tb, struct textselpos *te,
						int level, Event *event)
{
	char *line;

	tb->tsp_row = y_to_row(event->ie_locy);
	if (tb->tsp_row >= ttysw->ttysw_bottom)
		tb->tsp_row = MAX(0, ttysw->ttysw_bottom - 1);
	else if (tb->tsp_row < 0)
		tb->tsp_row = 0;

	line = ttysw->image[tb->tsp_row];
	tb->tsp_col = x_to_col(event->ie_locx);

	if (tb->tsp_col > (int)LINE_LENGTH(line))
		tb->tsp_col = LINE_LENGTH(line);

	*te = *tb;
	switch (level) {
		case SEL_CHAR:
			break;
		case SEL_WORD:
			{
				register int col, chr;
				register unsigned char match_mode;

				if (delim_init == FALSE)
					init_delim_table();

				match_mode = delim_table[(int)line[te->tsp_col]];

				for (col = te->tsp_col; col < (int)LINE_LENGTH(line); col++) {
					chr = line[col];
					if (delim_table[chr] != match_mode)
						break;
				}
				te->tsp_col = MAX(col - 1, tb->tsp_col);
				for (col = tb->tsp_col; col >= 0; col--) {
					chr = line[col];
					if (delim_table[chr] != match_mode)
						break;
				}
				tb->tsp_col = MIN(col + 1, te->tsp_col);
				break;
			}
		case SEL_LINE:
			tb->tsp_col = 0;
			te->tsp_col = LINE_LENGTH(line) - 1;
			break;
		case SEL_PARA:
			{
				register int row;

				for (row = tb->tsp_row; row >= ttysw->ttysw_top; row--)
					if (LINE_LENGTH(ttysw->image[row]) == 0)
						break;
				tb->tsp_row = MIN(tb->tsp_row, row + 1);
				tb->tsp_col = 0;
				for (row = te->tsp_row; row < ttysw->ttysw_bottom; row++)
					if (LINE_LENGTH(ttysw->image[row]) == 0)
						break;
				te->tsp_row = MAX(te->tsp_row, row - 1);
				te->tsp_col = LINE_LENGTH(ttysw->image[te->tsp_row]);
				break;
			}
	}
}
#endif

static void tvsub(struct timeval *tdiff, struct timeval *t1, struct timeval *t0)
{

    tdiff->tv_sec = t1->tv_sec - t0->tv_sec;
    tdiff->tv_usec = t1->tv_usec - t0->tv_usec;
    if (tdiff->tv_usec < 0)
	tdiff->tv_sec--, tdiff->tv_usec += 1000000;
}

/*
 * Is the specified position within the current selection?
 */
static int ttysel_insel(struct ttyselection *ttysel, struct textselpos *tsp)
{
    register struct textselpos *tb = &ttysel->sel_begin;
    register struct textselpos *te = &ttysel->sel_end;

    if (tsp->tsp_row < tb->tsp_row || tsp->tsp_row > te->tsp_row)
	return (0);
    if (tb->tsp_row == te->tsp_row)
	return (tsp->tsp_col >= tb->tsp_col &&
		tsp->tsp_col <= te->tsp_col);
    if (tsp->tsp_row == tb->tsp_row)
	return (tsp->tsp_col >= tb->tsp_col);
    if (tsp->tsp_row == te->tsp_row)
	return (tsp->tsp_col <= te->tsp_col);
    return (1);
}

/*
 * Make a new selection. If multi is set, check for multi-click.
 */
Pkg_private void ttysel_make(Ttysw_private ttysw, Event *event, int multi)
{
	int sel_received;
	struct ttyselection *ttysel;
	struct textselpos tspb, tspe;
	struct timeval td;

	if (event_is_quick_move(event) || event_is_quick_duplicate(event)) {
		sel_received = TTY_SEL_SECONDARY;
		ttysw->current_sel = TTY_SEL_SECONDARY;
		ttysel = ttysw->sels + TTY_SEL_SECONDARY;
	}
	else {
		sel_received = TTY_SEL_PRIMARY;
		ttysw->current_sel = TTY_SEL_PRIMARY;
		ttysel = ttysw->sels + TTY_SEL_PRIMARY;

		xv_set(ttysw->sel_owner[TTY_SEL_SECONDARY], SEL_OWN, FALSE, NULL);
	}

	xv_set(ttysw->sel_owner[ttysw->current_sel],
				SEL_TIME, &event_time(event),
				SEL_OWN, TRUE,
				NULL);

	ttysel_resolve(ttysw, &tspb, &tspe, SEL_CHAR, event);
	if (multi && ttysel->sel_made) {
		tvsub(&td, &event->ie_time, &ttysel->sel_time);
		if (ttysel_insel(ttysel, &tspe) && timercmp(&td, &maxinterval, <)) {
			ttysel_adjust(ttysw, event, TRUE, TRUE);
			return;
		}
	}
	if (ttysel->sel_made)
		ttysel_deselect(ttysw, ttysel, sel_received);
	ttysel->sel_made = TRUE;
	ttysel->sel_begin = tspb;
	ttysel->sel_end = tspe;
	ttysel->sel_time = event->ie_time;
	ttysel->sel_level = SEL_CHAR;
	ttysel->is_word = FALSE;
	ttysel->sel_anchor = 0;
	ttysel->sel_null = FALSE;
	ttyhiliteselection(ttysw, ttysel, (int)sel_received);
}

static char *extend_selbuffer(Ttysw_private priv)
{
	char *newbuf;
	unsigned newsiz;

	newsiz = priv->bufsize + 1000;
	newbuf = xv_calloc(newsiz, 1);

	if (priv->selbuffer) {
		memcpy(newbuf, priv->selbuffer, (size_t)priv->bufsize);
		xv_free(priv->selbuffer);
	}
	priv->selbuffer = newbuf;
	priv->bufsize = newsiz;
	return newbuf;
}

static void supply_sel_item(Ttysw_private priv)
{
	int curr_col, curr_row, index, row_len;
	char *dest;
	int start_col, end_col, last_row;
	struct ttyselection *ttysel;
	char *src;
	int i;

	ttysel = priv->sels + priv->current_sel;
	start_col = ttysel->sel_begin.tsp_col;
	end_col = ttysel->sel_end.tsp_col;
	last_row = ttysel->sel_end.tsp_row;

	if (! priv->selbuffer) dest = extend_selbuffer(priv);
	else dest = priv->selbuffer;

	i = 0;

	curr_col = start_col;
	curr_row = ttysel->sel_begin.tsp_row;
	if (curr_row < 0) {
		/* investigation showed that 
		 * priv->current_sel = 0 (= PRIMARY)
		 * sel_begin = (-1, -1), sel_end = (-1, -1)
		 * ....
		 * I didn't find out, **why** we came here but **how** we came here:
		 *    supply_sel_item      called from
		 *    ttysel_finish        called from
		 *    ttysw_process_select called from
		 *    ttysw_eventstd       ....  
		 *    
		 * and the event was a ACTION_SELECT up. ???
		 */

		/* let us return, otherwise we had a SIGSEGV later.... */
		return;
	}
	while (curr_row < last_row) {
		/* LINE_LENGHT is (priv->image[curr_row])[-1]   */
		row_len = (int)LINE_LENGTH(priv->image[curr_row]) - curr_col;
		if (i + row_len >= priv->bufsize) dest = extend_selbuffer(priv);
		index = row_len;
		src = priv->image[curr_row] + curr_col;
		while (index--) {
			dest[i++] = *src++;
		}
		/* so, here is the place where the following bug happens:
		 * if you have a line that reaches to the right edge of the window
		 * (ttysw_right) and make a selection that stretches over this spot.
		 * When you paste this somewhere else, you have
		 * **lost the NEWLINE**.
		 * 
		 * However, if you have a long line that has been wrapped (e.g.in vi),
		 * this behaviour is correct: it **looks** like a NEWLINE, but 
		 * there is no NEWLINE.....
		 *
		 * My first thought was 'omit the if', but now I think I leave it
		 * as it is - we have no information here about 
		 * "NEWLINE or no NEWLINE"
		 */
		if (row_len + curr_col != priv->ttysw_right) {
			if (i + 10 >= priv->bufsize) dest = extend_selbuffer(priv);
			dest[i++] = '\n';
		}
		curr_col = 0;
		++curr_row;
	}
	/* now handle the last line */
	row_len = end_col + 1 - curr_col;
	if (i + row_len + 10 >= priv->bufsize) dest = extend_selbuffer(priv);
	index = row_len;
	src = priv->image[curr_row] + curr_col;
	while (index--) {
		dest[i++] = *src++;   /* sometimes crashes here */
	}
	if (end_col==LINE_LENGTH(priv->image[curr_row]) && end_col<priv->ttysw_right) {
		dest[i-1] = '\n';
	}
	dest[i] = '\0';

	SERVERTRACE((855, "sel_item[%d] gets '%s'\n", priv->current_sel, dest));
	xv_set(priv->sel_item[priv->current_sel],
				SEL_DATA, dest,
				SEL_LENGTH, i,
				NULL);
}

Pkg_private void ttysel_finish(Ttysw_private priv, Event *ev)
{
	/* supply priv->sel_item[priv->current_sel] with the selected data */
	supply_sel_item(priv);
}

static void highlight(Ttysw_private ttysw, struct ttyselection *ttysel, int rank)
{
	if (rank == TTY_SEL_PRIMARY)
		ttyhiliteselection(ttysw, ttysel, rank);
	else {
		ttysel->dehilite_op = TRUE;
		ttyhiliteselection(ttysw, ttysel, rank);
		ttysel->dehilite_op = FALSE;
	}
}

static void ttyenumerateselection(Ttysw_private ttysw,
			struct ttyselection *ttysel, tty_enumeration_t proc, char *data)
{
    struct textselpos *xbegin, *xend;
    struct textselpos *begin, *end;
    int    row;

    if (!ttysel->sel_made || ttysel->sel_null) return;
    /*
     * Sort extents
     */
    ttysortextents(ttysel, &xbegin, &xend);
    begin = xbegin;
    end = xend;
    /*
     * Process a line at a time
     */
    for (row = begin->tsp_row; row <= end->tsp_row; row++) {
	if (row == begin->tsp_row && row == end->tsp_row) {
	    /*
	     * Partial line hilite in middle
	     */
	    proc(ttysw, begin->tsp_col, end->tsp_col, row, data, ttysel);
	} else if (row == begin->tsp_row) {
	    /*
	     * Partial line hilite from beginning
	     */
#ifdef OW_I18N
            proc(ttysw, begin->tsp_col, LINE_LENGTH(ttysw->image[row]), row, data, ttysel);
#else
	    proc(ttysw, begin->tsp_col, LINE_LENGTH(ttysw->image[row]), row, data, ttysel);
#endif
	} else if (row == end->tsp_row) {
	    /*
	     * Partial line hilite not to end
	     */
	    proc(ttysw, 0, end->tsp_col, row, data, ttysel);
	} else {
	    /*
	     * Full line hilite
	     */
#ifdef OW_I18N
            proc(ttysw, 0, LINE_LENGTH(ttysw->image[row]), row, data, ttysel);
#else
	    proc(ttysw, 0, LINE_LENGTH(ttysw->image[row]), row, data, ttysel);
#endif
	}
    }
}

#ifdef OW_I18N
static void
ttycountchars(Ttysw_private ttysw, start, finish, row, count)
/*
 * Since it does not use the selection rank, it is not include in the
 * argument list
 */
    register int                        row, *count;
    register int                        start, finish;
{
    register int        i;
    register int        char_conut = 0;
    CHAR                *line = ttysw->image[row];

    for( i = start; i<= finish; i++ ) {
        if( line[i] != TTY_NON_WCHAR )
                char_conut++;
    }
    *count += char_conut;
    if (LINE_LENGTH(ttysw->image[row]) == finish &&
            finish == ttysw->ttysw_right) {
        *count -= 1;            /* no CR on wrapped lines        */
    }
}
static void
ttycountbytes(start, finish, row, count)
/*
 * Since it does not use the selection rank, it is not include in the
 * argument list
 */
    register int                        row, *count;
    register int                        start, finish;
{
    register int        i;
    register int        byte_count = 0;
    register int        len;
    CHAR                *line = ttysw->image[row];
    char                dummy[10];

    for( i = start; i<= finish; i++ ) {
        if( line[i] == TTY_NON_WCHAR )
                continue;
        len = wctomb( dummy, line[i] );
        /*
         *       Take care the case of null character
         */
        if( len == 0 )
                len = 1;
        byte_count += len;
    }
    *count += byte_count;
    if (LINE_LENGTH(ttysw->image[row]) == finish &&
            finish == ttysw->ttysw_right) {
        *count -= 1;            /* no CR on wrapped lines        */
    }
}
#else /* OW_I18N */
static void ttycountchars(Ttysw_private ttysw, int start,int finish, int row,void *xcount, struct ttyselection *u)
/*
 * Since it does not use the selection rank, it is not include in the
 * argument list
 */
{
	int *count = (int *)xcount;

    *count += finish + 1 - start;
    if (LINE_LENGTH(ttysw->image[row]) == finish && finish == ttysw->ttysw_right) {
	*count -= 1;		/* no CR on wrapped lines	 */
    }
}
#endif

static int ttysel_eq(struct textselpos *t1, struct textselpos *t2)
{
    return (t1->tsp_row == t2->tsp_row && t1->tsp_col == t2->tsp_col);
}

/*
 * Adjust the current selection according to the event. If multi is set,
 * check for multi-click.
 */
Pkg_private void ttysel_adjust(Ttysw_private priv, Event *event,
									int multi, int ok_to_extend)
{
	register struct textselpos *tb;
	register struct textselpos *te;
	int rank;
	int count;
	int extend = 0;
	struct textselpos tspc, tspb, tspe, tt;
	struct ttyselection *ttysel;
	struct timeval td;

	/* I think this modifies the 'visible selection',
	 * not the primary selection
	 */
	SERVERTRACE((833, "%s: m=%d, ote=%d\n", __FUNCTION__, multi, ok_to_extend));
	if (priv->sels[TTY_SEL_SECONDARY].sel_made) {
		rank = TTY_SEL_SECONDARY;
		ttysel = priv->sels + TTY_SEL_SECONDARY;
	}
	else if (priv->sels[TTY_SEL_PRIMARY].sel_made) {
		rank = TTY_SEL_PRIMARY;
		ttysel = priv->sels + TTY_SEL_PRIMARY;
	}
	else {
		return;
	}
	tb = &ttysel->sel_begin;
	te = &ttysel->sel_end;
	if (!ttysel->sel_made || ttysel->sel_null) return;
	ttysel_resolve(priv, &tspb, &tspc, SEL_CHAR, event);
	if (multi) {
		tvsub(&td, &event->ie_time, &ttysel->sel_time);
		if (ttysel_insel(ttysel, &tspc) && timercmp(&td, &maxinterval, <) &&
				ok_to_extend) {
			extend = 1;
			if (++ttysel->sel_level > SEL_MAX) {
				ttysel->sel_level = SEL_CHAR;
				extend = 0;
			}
			ttysel->is_word = (ttysel->sel_level == SEL_WORD);
		}
		ttysel->sel_time = event->ie_time;
		ttysel->sel_anchor = 0;
	}
	ttysel_resolve(priv, &tspb, &tspe, ttysel->sel_level, event);
	/*
	 * If inside current selection, pull in closest end.
	 */
	if (!extend && ttysel_insel(ttysel, &tspc)) {
		int left_end, right_end;

		if (ttysel->sel_anchor == 0) {
			/* count chars to left */
			count = 0;
			tt = *te;
			*te = tspc;
			ttyenumerateselection(priv, ttysel, ttycountchars, (char *)(&count));
			*te = tt;
			left_end = count;
			/* count chars to right */
			count = 0;
			tt = *tb;
			*tb = tspc;
			ttyenumerateselection(priv, ttysel, ttycountchars, (char *)(&count));
			*tb = tt;
			right_end = count;
			if (right_end <= left_end)
				ttysel->sel_anchor = -1;
			else
				ttysel->sel_anchor = 1;
		}
		if (ttysel->sel_anchor == -1) {
			if (!ttysel_eq(te, &tspe)) {
				/* pull in right end */
				tt = *tb;
				*tb = tspe;
				tb->tsp_col++;
				highlight(priv, ttysel, (int)rank);
				*tb = tt;
				*te = tspe;
			}
		}
		else {
			if (!ttysel_eq(tb, &tspb)) {
				/* pull in left end */
				tt = *te;
				*te = tspb;
				te->tsp_col--;
				highlight(priv, ttysel, (int)rank);
				*te = tt;
				*tb = tspb;
			}
		}
	}
	else {
		/*
		 * Determine which end to extend. Both ends may extend if selection
		 * level has increased.
		 */
		int newanchor = 0;

		if (tspe.tsp_row > te->tsp_row ||
				(tspe.tsp_row == te->tsp_row && tspe.tsp_col > te->tsp_col)) {
			if (ttysel->sel_anchor == 1) {
				/* selection is crossing over anchor point.
				 *  pull in left end before extending right.
				 */
				if (tb->tsp_col != te->tsp_col) {
					tt = *te;
					te->tsp_col--;
					highlight(priv, ttysel, (int)rank);
					*te = tt;
					*tb = *te;
				}
				ttysel->sel_anchor = -1;
			}
			else if (ttysel->sel_anchor == 0)
				newanchor = -1;
			/* extend right end */
			tt = *tb;
			*tb = *te;
			tb->tsp_col++;	/* check for overflow? */
			*te = tspe;
			ttyhiliteselection(priv, ttysel, (int)rank);
			*tb = tt;
		}
		if (tspb.tsp_row < tb->tsp_row ||
				(tspb.tsp_row == tb->tsp_row && tspb.tsp_col < tb->tsp_col)) {
			if (ttysel->sel_anchor == -1) {
				/* selection is crossing over anchor point.
				 *  pull in right end before extending left.
				 */
				if (tb->tsp_col != te->tsp_col) {
					tt = *tb;
					tb->tsp_col++;
					highlight(priv, ttysel, (int)rank);
					*tb = tt;
					*te = *tb;
				}
				ttysel->sel_anchor = 1;
			}
			else if (ttysel->sel_anchor == 0) {
				if (newanchor == 0)
					newanchor = 1;
				else
					newanchor = 0;
			}
			/* extend left end */
			tt = *te;
			*te = *tb;
			te->tsp_col--;	/* check for underflow? */
			*tb = tspb;
			ttyhiliteselection(priv, ttysel, (int)rank);
			*te = tt;
		}
		if (ttysel->sel_anchor == 0)
			ttysel->sel_anchor = newanchor;
	}

	supply_sel_item(priv);
}

/*
 * Clear out the current selection.
 */
static void ttysel_cancel(struct ttysubwindow *ttysw, int ranki)
{
	struct ttyselection *ttysel;

	SERVERTRACE((333, "%s:\n", __FUNCTION__));
	ttysel = ttysw->sels + ranki;
	if (!ttysel->sel_made)
		return;
	ttysel_deselect(ttysw, ttysel, ranki); /* INCOMPLETE */
	ttysel->sel_made = FALSE;
}

/* XXX - compatibility kludge */
/* BUG ALERT: No XView prefix */
Pkg_private void ttynullselection(struct ttysubwindow *ttysw)
{
	SERVERTRACE((333, "%s:\n", __FUNCTION__));
    (void) ttysel_cancel(ttysw, TTY_SEL_PRIMARY);
}

/*
 * Make a selection be empty
 */
static void ttysel_empty(struct ttyselection *ttysel)
{
    ttysel->sel_null = TRUE;
    ttysel->sel_level = SEL_CHAR;
	ttysel->is_word = FALSE;
    ttysel->sel_begin = tse_null_extent;
    ttysel->sel_end = tse_null_extent;
}

/*
 * Remove a selection from the screen
 */
Pkg_private void ttysel_deselect(Ttysw_private ttysw, struct ttyselection *ttysel, int rank)
{
	SERVERTRACE((353, "%s:\n", __FUNCTION__));
	if (!ttysel->sel_made) return;
	ttysel->dehilite_op = TRUE;
	ttyhiliteselection(ttysw, ttysel, rank);
	ttysel->dehilite_op = FALSE;
	if (!ttysel->sel_null)
		ttysel_empty(ttysel);
}

static void my_write_string(Ttysw_private ttysw, int start,int end,int row)
{
	CHAR *str = ttysw->image[row];
	CHAR temp_char = (CHAR) '\0';

#ifdef OW_I18N
	int cwidth;
	int offset;

	tty_column_wchar_type(start, row, &cwidth, &offset);
	start -= offset;
	tty_column_wchar_type(end, row, &cwidth, &offset);
	end += (cwidth - offset - 1);
#endif

	if ((end + 1) < (int)STRLEN(str)) {	/* This is a very dirty trick for
										 * speed */
		temp_char = str[end + 1];
		str[end + 1] = (CHAR) '\0';
		ttysw_pclearline(ttysw, start, (int)strlen(str), row);
	}
	else
		ttysw_pclearline(ttysw, start, (int)strlen(str) + 1, row);

	ttysw_pstring(ttysw, (str + start), ttysw->boldify, start, row, PIX_SRC);

	if (temp_char != '\0') str[end + 1] = temp_char;
}

static void ttyhiliteline(Ttysw_private ttysw, int start, int finish, int row, void *xc, struct ttyselection *ttysel)
{
	struct pr_size *offsets = (struct pr_size *)xc;
	struct rect r;

	rect_construct(&r, col_to_x(start), row_to_y(row) + offsets->x,
			col_to_x(finish + 1) - col_to_x(start), offsets->y);
	if (r.r_width == 0) return;

	if (ttysel->dehilite_op) my_write_string(ttysw, start, finish, row);
	else {
		if (ttysel->selrank == TTY_SEL_SECONDARY)
			my_write_string(ttysw, start, finish, row);

		(void)ttysw_pselectionhilite(&r, ttysel->selrank);
	}
}

/*
 * Hilite a selection. Enumerate all the lines of the selection; hilite each
 * one as appropriate.
 */
Pkg_private void ttyhiliteselection(Ttysw_private ttysw,
							struct ttyselection *ttysel, int rank)
{
    struct pr_size  offsets;

	SERVERTRACE((353, "%s:\n", __FUNCTION__));
    if (!ttysel->sel_made || ttysel->sel_null) {
		return;
    }
    ttysel->selrank = rank;
    offsets.x = 0;
    offsets.y = ttysw->chrheight;

    ttyenumerateselection(ttysw, ttysel, ttyhiliteline, (char *) (&offsets));
}

/* internal (static) routines	 */



Xv_private void ttysortextents(struct ttyselection *ttysel,
				struct textselpos **begin, struct textselpos **end)
{

	if (ttysel->sel_begin.tsp_row == ttysel->sel_end.tsp_row) {
		if (ttysel->sel_begin.tsp_col > ttysel->sel_end.tsp_col) {
			*begin = &ttysel->sel_end;
			*end = &ttysel->sel_begin;
		}
		else {
			*begin = &ttysel->sel_begin;
			*end = &ttysel->sel_end;
		}
	}
	else if (ttysel->sel_begin.tsp_row > ttysel->sel_end.tsp_row) {
		*begin = &ttysel->sel_end;
		*end = &ttysel->sel_begin;
	}
	else {
		*begin = &ttysel->sel_begin;
		*end = &ttysel->sel_end;
	}
}

#ifdef lint
#undef putc
#define putc(_char, _file) \
	_file = (FILE *)(_file ? _file : 0)
#endif				/* lint */

Pkg_private int ttysw_do_copy(Ttysw_private priv)
{
	struct timeval tv;

	server_set_timestamp(XV_SERVER_FROM_WINDOW(TTY_PUBLIC(priv)), &tv, 0L);
	return ttysw_event_copy_down(priv, &tv);
}

Pkg_private int ttysw_do_paste(Ttysw_private priv)
{
	struct timeval tv;

	server_set_timestamp(XV_SERVER_FROM_WINDOW(TTY_PUBLIC(priv)), &tv, 0L);
	ttysw_event_paste_up(priv, &tv);
    return 1;
}

static void tty_lose_proc(Selection_owner sel_own)
{
	/* DANGEROUS: TERMSW ??? Tty tty = xv_get(sr, XV_OWNER); */
	Ttysw_private priv = (Ttysw_private)xv_get(sel_own,XV_KEY_DATA, ttysel_key);
	Atom rank_atom;

	/* Initially, I used     Tty tty = xv_get(sr, XV_OWNER);

	   BUT very careful: if this is in fact a TTY, the public obj looks like
		typedef struct {
			Xv_openwin	parent_data;
			Xv_opaque	private_data;
		} Xv_tty;
	   and TTY_PRIVATE(tty) is this 'private_data'.

	   However, if this is a TERMSW, the public obj looks like
		typedef struct {
			Xv_textsw		parent_data; 
			Xv_opaque    		private_data;
			Xv_opaque		private_text;
			Xv_opaque		private_tty;
		} Xv_termsw;

		which in fact looks like

		typedef struct {
			struct {
				Xv_openwin    parent_data;
				Xv_opaque    private_data;
			} parent_data;
			Xv_opaque    		private_data;
			Xv_opaque		private_text;
			Xv_opaque		private_tty;
		} Xv_termsw;

	   and then TTY_PRIVATE(tty) is this 'parent_data.private_data'
	   which is probably the same as private_text
	*/

	/* mal versuchen: */
    rank_atom = (Atom) xv_get(sel_own, SEL_RANK);
	if (rank_atom == XA_PRIMARY) ttysel_cancel(priv, TTY_SEL_PRIMARY);
	else if (rank_atom == XA_SECONDARY) ttysel_cancel(priv, TTY_SEL_SECONDARY);
}

	
static int tty_convert_proc(Selection_owner sel_own, Atom *type,
					Xv_opaque *data, unsigned long *length, int *format)
{
	/* DANGEROUS: TERMSW ??? Tty tty = xv_get(sr, XV_OWNER); */
	Ttysw_private priv = (Ttysw_private)xv_get(sel_own,XV_KEY_DATA, ttysel_key);
	Atom rank_atom;
	int rank_index;
	Xv_Server server;
	int retval;

	server = XV_SERVER_FROM_WINDOW(xv_get(sel_own, XV_OWNER));

	rank_atom = (Atom) xv_get(sel_own, SEL_RANK);
	SERVERTRACE((765, "%s request for %s\n",
			(char *)xv_get(server, SERVER_ATOM_NAME, rank_atom),
			(char *)xv_get(server, SERVER_ATOM_NAME, *type)));

	if (*type == priv->selection_end && rank_atom == XA_SECONDARY) {
		/* Lose the Secondary Selection */
		xv_set(sel_own, SEL_OWN, FALSE, NULL);
        *type = xv_get(server, SERVER_ATOM, "NULL");
        *data = XV_NULL;
        *length = 0;
        *format = 32;
		return TRUE;
	}
	if (*type == priv->seln_yield && rank_atom == XA_SECONDARY) {
		static long answer;
		/* Lose the Selection - we support this only for SECONDARY */
		xv_set(sel_own, SEL_OWN, FALSE, NULL);
		answer = 1L;
        *data = (Xv_opaque)&answer;
        *length = 1;
        *format = 32;
		return TRUE;
	}
	if (*type == (Atom) xv_get(server, SERVER_ATOM, "LENGTH")) {
		/* This is only used by SunView1 selection clients for
		 * clipboard and secondary selections.
		 */
		if (rank_atom == XA_SECONDARY)
			rank_index = TTY_SEL_SECONDARY;
		else
			rank_index = TTY_SEL_CLIPBOARD;
		priv->sel_reply =
				(unsigned long)xv_get(priv->sel_item[rank_index], SEL_LENGTH);
		*data = (Xv_opaque)&priv->sel_reply;
		*length = 1;
		*format = 32;
		return TRUE;
	}
	if (*type==(Atom)xv_get(server,SERVER_ATOM,"_OL_SELECTION_IS_WORD")) {
		if (rank_atom == XA_SECONDARY)
			rank_index = TTY_SEL_SECONDARY;
		else if (rank_atom == (Atom)xv_get(server, SERVER_ATOM, "CLIPBOARD")) {
			rank_index = TTY_SEL_CLIPBOARD;
		}
		else { /* primary or dnd */
			rank_index = TTY_SEL_PRIMARY;
		}
		/* wie findet man das heraus */
		/* siehe start_seln_tracking */
		priv->sel_reply = priv->sels[TTY_SEL_CLIPBOARD].is_word;
		*data = (Xv_opaque)&priv->sel_reply;
		*length = 1;
		*format = 32;
		*type = XA_INTEGER;
		return TRUE;
	}
	if (*type==(Atom)xv_get(server,SERVER_ATOM,"_SUN_SELN_IS_READONLY")) {
		priv->sel_reply = TRUE;

		*format = 32;
		*length = 1;
		*data = (Xv_opaque) &priv->sel_reply;
		*type = XA_INTEGER;
		return TRUE;
	}
	if (*type == (Atom)xv_get(server, SERVER_ATOM, "_SUN_DRAGDROP_ACK")) {
		/* Test: I have a Textsw that is also able to accept 'non-text data'
		 * and therefore calls dnd_decode_drop and TRIES to convert to, 
		 * say, PIXMAP.
		 * If this fails, the XView Textsw calls dnd_decode_drop AGAIN... and
		 * this second request failed because _SUN_DRAGDROP_ACK was rejected
		 */
		*format = 32;
		*length = 0;
		*data = XV_NULL;
		*type = (Atom)xv_get(server, SERVER_ATOM, "NULL");
		return TRUE;
	}

	/* Use default Selection Package convert procedure */
	retval = sel_convert_proc(sel_own, type, data, (unsigned long *)length,
			format);
#ifdef NO_XDND
#else /* NO_XDND */
	if (! retval) {
		if (*type == (Atom)xv_get(server, SERVER_ATOM, "text/plain")) {
			Atom savat = *type;

			*type = XA_STRING;
			retval = sel_convert_proc(sel_own, type, data,
							(unsigned long *)length, format);

			*type = savat;
		}
	}
#endif /* NO_XDND */

	return retval;
}

static void note_sel_reply(Selection_requestor sr, Atom target, Atom type,
					Xv_opaque value, unsigned long length, int format)
{
	/* DANGEROUS: TERMSW ??? Tty tty = xv_get(sr, XV_OWNER); */
	Ttysw_private priv = (Ttysw_private)xv_get(sr, XV_KEY_DATA, ttysel_key);

	if (length == SEL_ERROR) {
		SERVERTRACE((333, "note_sel_reply SEL_ERROR\n"));
		if (target == priv->selection_end) {
			/* old selection owner? */
			xv_set(sr, SEL_TYPE, priv->seln_yield, NULL);
			sel_post_req(sr);
			return;
		}
		if (target == priv->seln_yield) {
			/* target should be set even in error cases */
			xv_destroy_safe(sr);
			return;
		}
	}

	if (target == priv->selection_end) {
		xv_destroy_safe(sr);
		if (value) xv_free(value);
	}
	if (target == priv->seln_yield) {
		xv_destroy_safe(sr);
		if (value) xv_free(value);
	}
}

Pkg_private void ttysw_event_paste_up(Ttysw_private priv, struct timeval *t)
{
	Tty tty = TTY_PUBLIC(priv);
	unsigned long length;
	int format;
	char *string;
	Selection_requestor sel_req;

	/* could have been a quick duplicate */
	sel_req = xv_create(tty, SELECTION_REQUESTOR,
					SEL_REPLY_PROC, note_sel_reply,
					XV_KEY_DATA, ttysel_key, priv,
					SEL_RANK, XA_SECONDARY,
					SEL_TYPE, XA_STRING,
					SEL_AUTO_COLLECT_INCR, TRUE,
					SEL_TIME, t,
					NULL);
	SERVERTRACE((333, "requesting SECONDARY STRING\n"));
	string = (char *)xv_get(sel_req, SEL_DATA, &length, &format);
	SERVERTRACE((333, "requested SECONDARY STRING, len=%ld, fmt=%d, ptr=%p\n",
						length, format, string));

	if (length != SEL_ERROR) {
		/* yes, it WAS a quick duplicate */

		xv_set(sel_req, SEL_TYPE, priv->selection_end, NULL);
		sel_post_req(sel_req);
	}
	else {
		/* no, it was NOT a quick duplicate */
		if (string) xv_free(string);
		string = NULL;
		xv_destroy(sel_req);
	}

	if (! string) {
		sel_req = xv_create(tty, SELECTION_REQUESTOR,
					SEL_RANK_NAME, "CLIPBOARD",
					SEL_TYPE, XA_STRING,
					SEL_AUTO_COLLECT_INCR, TRUE,
					NULL);
		SERVERTRACE((333, "requesting CLIPBOARD STRING\n"));
		string = (char *)xv_get(sel_req, SEL_DATA, &length, &format);
		SERVERTRACE((333, "requested CLIPBOARD STRING, len=%ld, fmt=%d, ptr=%p\n",
							length, format, string));

		if (length == SEL_ERROR) {
			Tty tty = TTY_PUBLIC(priv);

			if (string) xv_free(string);
			/* PASTE failed, what do we do now? */
/* 			xv_error(tty,ERROR_PKG, TTY,ERROR_STRING, "Paste failed",NULL); */

			/* taken from a Xol reference manual: */
			xv_set(tty, WIN_ALARM, NULL);

			xv_destroy(sel_req);
			return;
		}
		xv_destroy(sel_req);
	}

	/* now, we have the pasted text in 'string' */

	/* In TEXSW: if there is a primary selection, we have to handle it as
	 * "pending delete"...   but not here....
	 */

	ttysw_input_it(priv, string, (int)strlen(string));
	ttysw_reset_conditions(TTY_VIEW_HANDLE_FROM_TTY_FOLIO(priv));
	free((char *)string);
}

Pkg_private void ttysw_event_cut_up(Ttysw_private priv, Event *ev)
{
	Tty tty = TTY_PUBLIC(priv);
	unsigned long length;
	int format;
	char *string;
	Selection_requestor sel_req;

	/* could have been a quick move */
	sel_req = xv_create(tty, SELECTION_REQUESTOR,
					SEL_REPLY_PROC, note_sel_reply,
					XV_KEY_DATA, ttysel_key, priv,
					SEL_RANK, XA_SECONDARY,
					SEL_TYPE, XA_STRING,
					SEL_AUTO_COLLECT_INCR, TRUE,
					SEL_TIME, &event_time(ev),
					NULL);
	SERVERTRACE((333, "requesting SECONDARY STRING\n"));
	string = (char *)xv_get(sel_req, SEL_DATA, &length, &format);
	SERVERTRACE((333, "requested SECONDARY STRING, len=%ld, fmt=%d, ptr=%p\n",
						length, format, string));

	if (length != SEL_ERROR) {
		char *dummy;

		/* yes, it WAS a quick move */
		SERVERTRACE((333, "requesting SECONDARY DELETE\n"));
		xv_set(sel_req, SEL_TYPE_NAME, "DELETE", NULL);
		dummy = (char *)xv_get(sel_req, SEL_DATA, &length, &format);
		if (dummy) xv_free(dummy);
		SERVERTRACE((333, "requested SECONDARY DELETE, len=%ld\n", length));

		xv_set(sel_req, SEL_TYPE, priv->selection_end, NULL);
		sel_post_req(sel_req);

		if (string) {
			/* now, we have the 'quick moved' text in 'string' */

			/* In TEXTSW: if there is a primary selection, we handle it as
			 * "pending delete"...   but not here....
			 */

			ttysw_input_it(priv, string, (int)strlen(string));
			ttysw_reset_conditions(TTY_VIEW_HANDLE_FROM_TTY_FOLIO(priv));
		}
	}
	else {
		/* no, it was NOT a quick duplicate */
		if (string) xv_free(string);
		string = NULL;

		/* it must have been a 'simple CUT' - however, we do not
		 * support CUT in a TTYSW - so: do nothing
		 */

		/* INCOMPLETE: maybe we should beep ? */
	}
}

Pkg_private int ttysw_event_copy_down(Ttysw_private priv, struct timeval *t)
{
	Selection_item primsel = priv->sel_item[TTY_SEL_PRIMARY];

	if (! xv_get(priv->sel_owner[TTY_SEL_PRIMARY], SEL_OWN)) {
		/* no primary selection... */
		return FALSE;
	}

	SERVERTRACE((855, "sel_item[%d] gets primary contents\n", TTY_SEL_CLIPBOARD));
	xv_set(priv->sel_item[TTY_SEL_CLIPBOARD],
						SEL_DATA, xv_get(primsel, SEL_DATA),
						SEL_LENGTH, xv_get(primsel, SEL_LENGTH),
						NULL);

	xv_set(priv->sel_owner[TTY_SEL_CLIPBOARD],
						SEL_TIME, t,
						SEL_OWN, TRUE,
						NULL);

	priv->sels[TTY_SEL_CLIPBOARD].is_word = priv->sels[TTY_SEL_PRIMARY].is_word;

	return TRUE;
}

static void additional_items(Selection_owner so, Atom add1)
{
	/* only to have them contained in TARGETS */
	xv_create(so, SELECTION_ITEM, SEL_TYPE, add1, NULL);
	xv_create(so, SELECTION_ITEM, SEL_TYPE_NAME, "_OL_SELECTION_IS_WORD", NULL);
}

Pkg_private void ttysw_new_sel_init(Ttysw_private priv)
{
	Tty tty = TTY_PUBLIC(priv);
	Xv_server srv = XV_SERVER_FROM_WINDOW(tty);

	if (! ttysel_key) ttysel_key = xv_unique_key();

	priv->selection_end = (Atom)xv_get(srv, SERVER_ATOM, "_SUN_SELECTION_END");
	priv->seln_yield = (Atom)xv_get(srv, SERVER_ATOM, "_SUN_SELN_YIELD");

	priv->sel_owner[TTY_SEL_PRIMARY] = xv_create(tty, SELECTION_OWNER,
							SEL_RANK, XA_PRIMARY,
							SEL_CONVERT_PROC, tty_convert_proc,
							SEL_LOSE_PROC, tty_lose_proc,
							XV_KEY_DATA, ttysel_key, priv,
							NULL);

	priv->sel_item[TTY_SEL_PRIMARY] =
					xv_create(priv->sel_owner[TTY_SEL_PRIMARY], SELECTION_ITEM,
							SEL_COPY, SEL_COPY_BLOCKED,
							NULL);
	additional_items(priv->sel_owner[TTY_SEL_PRIMARY], priv->selection_end);

	xv_create(priv->sel_owner[TTY_SEL_PRIMARY], SELECTION_ITEM,
							SEL_COPY, SEL_COPY_BLOCKED,
							NULL);
	priv->sel_owner[TTY_SEL_SECONDARY] =
					xv_create(tty, SELECTION_OWNER,
							SEL_CONVERT_PROC, tty_convert_proc,
							SEL_LOSE_PROC, tty_lose_proc,
							SEL_RANK, XA_SECONDARY,
							XV_KEY_DATA, ttysel_key, priv,
							NULL);

	priv->sel_item[TTY_SEL_SECONDARY] =
				xv_create(priv->sel_owner[TTY_SEL_SECONDARY], SELECTION_ITEM,
							SEL_COPY, SEL_COPY_BLOCKED,
							NULL);
	additional_items(priv->sel_owner[TTY_SEL_SECONDARY], priv->selection_end);

	priv->sel_owner[TTY_SEL_CLIPBOARD] =
					xv_create(tty, SELECTION_OWNER,
							SEL_CONVERT_PROC, tty_convert_proc,
							SEL_LOSE_PROC, tty_lose_proc,
							SEL_RANK_NAME, "CLIPBOARD",
							XV_KEY_DATA, ttysel_key, priv,
							NULL);

	priv->sel_item[TTY_SEL_CLIPBOARD] =
				xv_create(priv->sel_owner[TTY_SEL_CLIPBOARD], SELECTION_ITEM,
							SEL_COPY, SEL_COPY_BLOCKED,
							NULL);
}
