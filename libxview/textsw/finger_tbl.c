#ifndef lint
char     finger_tbl_c_sccsid[] = "@(#)finger_tbl.c 20.21 93/06/28 DRA: $Id: finger_tbl.c,v 4.1 2024/03/28 19:06:00 dra Exp $";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */

/*
 * Utilities for manipulation of finger tables.
 *
 * See finger_tbl.h for descriptions of what the routines do.
 */

#include <xview/base.h>
#include <xview/pkg.h>
#include <xview/attrol.h>
#include <xview_private/primal.h>
#include <xview_private/finger_tbl.h>
#ifdef SVR4
#include <stdlib.h>
#endif  /* SVR4 */

static void ft_validate_first_infinity(register ft_handle finger_table)
{
    register Es_index *seq_i_addr = 0;
    register int    addr_delta = finger_table->sizeof_element;
    register int    first_infinity = finger_table->first_infinity;
    int             save_last_bounding_index;

    if (first_infinity < finger_table->last_plus_one) {
	seq_i_addr = FT_ADDR(finger_table, first_infinity, addr_delta);
	if (*seq_i_addr == ES_INFINITY) {
	    while ((first_infinity > 0) &&
		   (seq_i_addr = FT_PREV_ADDR(seq_i_addr, addr_delta)) &&
		   (*seq_i_addr == ES_INFINITY)) {
		first_infinity--;
	    }
	} else if ((seq_i_addr = FT_NEXT_ADDR(seq_i_addr, addr_delta)) &&
		   ((*seq_i_addr) == ES_INFINITY)) {
	    first_infinity++;
	} else {
	    seq_i_addr = 0;
	}
    }
    if (seq_i_addr == 0) {
	save_last_bounding_index = finger_table->last_bounding_index;
	first_infinity =
	    ft_bounding_index(finger_table, ES_INFINITY - 1);
	if (first_infinity < finger_table->last_plus_one)
	    first_infinity++;
	finger_table->last_bounding_index = save_last_bounding_index;
    }
    finger_table->first_infinity = first_infinity;
}

Pkg_private void ft_add_delta(ft_object finger_table, int from, long int delta)
{
    register int    ft_index;
    register Es_index *seq_i_addr;
    register int    addr_delta = finger_table.sizeof_element;

    /*
     * ALERT: this routine assumes 'from' is < finger_table.last_plus_one:
     * calling procedures should check for this before calling this routine
     */

    seq_i_addr = FT_ADDR(&finger_table, from, addr_delta);
    if (*seq_i_addr != ES_INFINITY) {
	for (ft_index = from; ft_index < finger_table.last_plus_one;
	     ft_index++) {
	    if (*seq_i_addr == ES_INFINITY)
		break;
	    *seq_i_addr = *seq_i_addr + delta;
	    seq_i_addr = FT_NEXT_ADDR(seq_i_addr, addr_delta);
	}
    }
}

Pkg_private ft_object ft_create(int last_plus_one, int sizeof_client_data)
{
    ft_object       result;

    /*
     * Guarantee that result.sizeof_element meets all alignment restrictions
     * by adding trailing padding when necessary.
     */
    result.sizeof_element = sizeof(Es_index) + sizeof_client_data;
    while (result.sizeof_element % (sizeof(Es_index)))
	result.sizeof_element++;
    result.last_plus_one = last_plus_one;
    result.seq = (Es_index *) calloc((size_t) last_plus_one + 1,
											(size_t)result.sizeof_element);
    result.last_bounding_index = 0;
    result.first_infinity = 0;
    return (result);
}

Pkg_private void ft_destroy(ft_handle table)
{
    free((char *) table->seq);
    table->seq = 0;
    table->last_plus_one = 0;
}

Pkg_private void ft_expand(ft_handle table, int by)
{
    int             old_last_plus_one = table->last_plus_one;

    /* this routine should not be called if 'by' is == 0 and */
    /* it is the responsibility of the calling procedure to check */
    table->last_plus_one += by;
    table->seq = (Es_index *) realloc((char *) table->seq, (size_t) table->last_plus_one * (unsigned) table->sizeof_element);
    if (by > 0) {
	if (table->last_plus_one > old_last_plus_one)
	    ft_set(*table, old_last_plus_one,
		   table->last_plus_one, ES_INFINITY,
		   (char *) 0);
    }
}

Pkg_private void ft_shift_up(ft_handle table, int first, int last_plus_one, int expand_by)
{
    register int    addr_delta = table->sizeof_element, shift_count,
			stop_plus_one;
    register Es_index *seq_i_addr;

    ft_validate_first_infinity(table);
    if (expand_by > 0) {
	stop_plus_one = table->last_plus_one - (last_plus_one - 1 - first);
	if (table->first_infinity >= stop_plus_one) {
	    if (expand_by != 0)
		ft_expand(table, expand_by);
	}
    }
    shift_count = table->first_infinity - first;
    shift_count = MIN(shift_count, table->last_plus_one - last_plus_one);
    if (shift_count > 0) {
	seq_i_addr = FT_ADDR(table, first, addr_delta);
	XV_BCOPY((char *) seq_i_addr,
	      ((char *) FT_ADDR(table, last_plus_one, addr_delta)),
	      (size_t)(addr_delta * shift_count));
    }
    if (table->first_infinity < table->last_plus_one)
	table->first_infinity += (last_plus_one - first);
}

Pkg_private void ft_shift_out(ft_handle table, int first, int last_plus_one)
{
    register int    addr_delta = table->sizeof_element, to_move;
    register char  *first_addr, *lpo_addr;

    ft_validate_first_infinity(table);
    if (last_plus_one < table->first_infinity) {
	to_move = table->first_infinity - last_plus_one;
	first_addr = ((char *) table->seq + first * addr_delta);
	lpo_addr = ((char *) table->seq + last_plus_one * addr_delta);
	XV_BCOPY(lpo_addr, first_addr, (size_t)(to_move * addr_delta));
    } else
	to_move = 0;
    if (table->last_plus_one > first + to_move)
	ft_set(*table, first + to_move, table->first_infinity,
	       ES_INFINITY, (char *) 0);
    table->first_infinity = first + to_move;
}

Pkg_private void ft_set(ft_object finger_table, int first, int last_plus_one,
    Es_index to, void *client_data)
{
    register int    ft_index;
    register Es_index *seq_i_addr;
    register int    addr_delta = finger_table.sizeof_element;

    /*
     * ALERT: the calling procedure should check to see if 'first' is <
     * 'finger_table.last_plus_one' before calling this prodedure
     */
    seq_i_addr = FT_ADDR(&finger_table, first, addr_delta);
    for (ft_index = first; ft_index < last_plus_one; ft_index++) {
	*seq_i_addr = to;
	if (client_data) {
	    XV_BCOPY(client_data, ((char *) seq_i_addr) + sizeof(Es_index),
		  addr_delta - sizeof(Es_index));
	}
	seq_i_addr = FT_NEXT_ADDR(seq_i_addr, addr_delta);
    }
}

Pkg_private void ft_set_esi_span( ft_object finger_table,
    Es_index first, Es_index last_plus_one, Es_index to,
    char           *client_data)
{
    register int    index_of_first = 0, index_of_last_plus_one;
    register Es_index *seq_i_addr = finger_table.seq;
    register int    addr_delta = finger_table.sizeof_element;

    if AN_ERROR
	(finger_table.last_plus_one == 0) {
	return;
	}
    while (first > *seq_i_addr) {
	if (++index_of_first == finger_table.last_plus_one)
	    return;
	seq_i_addr = FT_NEXT_ADDR(seq_i_addr, addr_delta);
    }
    index_of_last_plus_one = index_of_first;
    while (last_plus_one > *seq_i_addr) {
	if (++index_of_last_plus_one == finger_table.last_plus_one)
	    break;
	seq_i_addr = FT_NEXT_ADDR(seq_i_addr, addr_delta);
    }
    if (finger_table.last_plus_one > index_of_first)
	ft_set(finger_table, index_of_first,
	       index_of_last_plus_one, to, client_data);
}

Pkg_private int ft_bounding_index(Ft_table finger_table, Es_index pos)
/*
 * The 3.0 version of this code used linear search with no caching, but a
 * table of pieces can get to be 100-1000 elements long, and then linear
 * search is way too slow.
 */
{
    register Es_index *seq_i_addr = finger_table->seq;
    register int    addr_delta = finger_table->sizeof_element;
    register int    index, start, stop_plus_one;

    stop_plus_one = finger_table->last_plus_one;
    if (pos < *seq_i_addr || stop_plus_one == 0) {
	index = stop_plus_one;
	finger_table->last_bounding_index = index;
	return (index);
    }
    /* Assert: seq[0] <= pos && 0 < stop_plus_one */
    /* Check the cache */
    index = finger_table->last_bounding_index;
    if (index < stop_plus_one) {
	seq_i_addr = FT_ADDR(finger_table, index, addr_delta);
	if (*seq_i_addr <= pos) {
	    if (index + 1 == stop_plus_one) {
		finger_table->last_bounding_index = index;
		return (index);
	    }
	    seq_i_addr = FT_NEXT_ADDR(seq_i_addr, addr_delta);
	    if (pos < *seq_i_addr) {
		finger_table->last_bounding_index = index;
		return (index);
	    }
	}
    }
    /* No luck, so do the search */
    index = stop_plus_one - 1;
    seq_i_addr = FT_ADDR(finger_table, index, addr_delta);
    if (*seq_i_addr <= pos) {
	finger_table->last_bounding_index = index;
	return (index);
    }
    /*
     * Assert: pos < seq[stop_plus_one-1] && 1 < stop_plus_one (else would
     * have goto'd above)
     */
    start = 0;
    /* Assert: seq[start] <= pos && start+1 == 1 < stop_plus_one */
    FOREVER {
	index = (start + stop_plus_one) / 2;
	/* Assert: start+1 <= index <= stop_plus_one-1 */
	seq_i_addr = FT_ADDR(finger_table, index, addr_delta);
	if (pos < *seq_i_addr) {
	    if (index + 1 == stop_plus_one) {
		/*
		 * Assert: start+1 == index (else contradiction), so
		 * seq[start] <= pos < seq[index], and we are done.
		 */
		index = start;
		finger_table->last_bounding_index = index;
		return (index);
	    }
	    stop_plus_one = index + 1;
	    /* Assert: start+1 < index+1 == (new)stop_plus_one */
	} else {
	    start = index;
	    /*
	     * Assert: (new)start == index < stop_plus_one-1, else seq[index
	     * == stop_plus_one-1] > pos, a contradiction
	     */
	}
	/* Assert: seq[start] <= pos < seq[stop_plus_one-1] */
    }
}

Pkg_private int ft_index_for_position(ft_object finger_table, Es_index pos)
{
    register int    ft_index;
    register Es_index *seq_i_addr = finger_table.seq;
    register int    addr_delta = finger_table.sizeof_element;

    for (ft_index = 0; ft_index < finger_table.last_plus_one; ft_index++) {
	if (*seq_i_addr == pos)
	    return (ft_index);
	if (*seq_i_addr > pos)
	    break;
	seq_i_addr = FT_NEXT_ADDR(seq_i_addr, addr_delta);
    }
    return (finger_table.last_plus_one);
}

Pkg_private     Es_index ft_position_for_index(ft_object finger_table, int index)
{
    register Es_index *seq_i_addr;
    register int    addr_delta = finger_table.sizeof_element;

    if (index >= finger_table.last_plus_one)
	return (ES_CANNOT_SET);
    seq_i_addr = FT_ADDR(&finger_table, index, addr_delta);
    return (*seq_i_addr);
}

#ifdef DEBUG
Pkg_private	int
fprintf_ft(finger_table)
    ft_object       finger_table;
{
    register Es_index *seq_i_addr = finger_table.seq;
    register int    addr_delta = finger_table.sizeof_element;
    register int    i, cd_i;
    register int   *client_data_ptr;
    FILE           *out_file = stderr;

    if (finger_table.last_plus_one > 999) {
	(void) fprintf(out_file,
		       "You passed the ft_handle, not the ft_object!\n");
	return (FALSE);
    }
    (void) fprintf(out_file,
		   "last_plus_one = %d, sizeof_element = %d, seq = 0x%lx\n",
		   finger_table.last_plus_one,
		   finger_table.sizeof_element,
		   finger_table.seq
	);
    (void) fprintf(out_file, "seq[] pos  client_data\n");
    for (i = 0; i < finger_table.last_plus_one; i++) {
	(void) fprintf(out_file, "%2d  ", i);
	switch (*seq_i_addr) {
	  case ES_INFINITY:
	    (void) fprintf(out_file, "  INF  ");
	    break;
	  case ES_CANNOT_SET:
	    (void) fprintf(out_file, " ~SET  ");
	    break;
	  default:
	    if (*seq_i_addr < 100000)
		(void) fprintf(out_file, "%5d  ", *seq_i_addr);
	    else
		(void) fprintf(out_file, "%d  ", *seq_i_addr);
	    break;
	}
	for (cd_i = 1; cd_i < (addr_delta / (sizeof(*client_data_ptr)));
	     cd_i++) {
	    client_data_ptr = ((int *) seq_i_addr);
	    client_data_ptr += cd_i;
	    (void) fprintf(out_file, "%8X  ", *client_data_ptr);
	}
	(void) fprintf(out_file, "\n");
	seq_i_addr += (addr_delta / sizeof(*seq_i_addr));
    }
    return (TRUE);
}

#endif
