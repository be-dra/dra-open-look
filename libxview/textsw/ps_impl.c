#ifndef lint
char     ps_impl_c_sccsid[] = "@(#)ps_impl.c 20.41 93/06/28 DRA: $Id: ps_impl.c,v 4.1 2024/03/28 19:06:00 dra Exp $";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */

/*
 * Entity stream implementation for manager of two other streams.
 *
 * Conceptually, suppose we want the piece source to contain:
 * AB0CD12E345FGH67I8JK9LMN and the original source contains: ABCDEFGHIJKLMN
 * then the operation sequence: replace start:2 stop_plus_one:2 with:"0"
 * replace start:5 stop_plus_one:5 with:"12" etc will generate a scratch
 * source that looks like: xx0xx12xx345xx67xx8xx9 and the piece table will
 * alternate with: virtual pos:0, length:2, source:original, source_pos:0
 * virtual pos:2, length:1, source:scratch, source_pos:sizeof(xx) virtual
 * pos:3, length:2, source:original, source_pos:2 virtual pos:5, length:2,
 * source:scratch, source_pos:2*sizeof(xx)+1 etc
 *
 * It is important that a piece in the piece table represent a contiguous run of
 * entities in the original or scratch stream.  Thus, if a break is detected
 * during the first read of a piece, a new piece must be created.
 *
 * 87Mar23: A major new feature is support for a maximum size that the scratch
 * stream is allowed to grow to.  The basic design: Most of the piece_stream
 * code believes that the scratch stream grows without bound.  However, in
 * reality, if the client sets a maximum size on the scratch stream, the ops
 * vector for the scratch stream is modified so that the logical index space
 * of [0..private->scratch_length) is mapped to the physical index space of
 * [0..private->scratch_max_len), essentially by making all accesses to the
 * logical indices be modulo private->scratch_max_len. Some special care must
 * be taken with UNDO and secondary piece streams, as these both can use
 * logical indices to reference entities that are no longer physically
 * available.
 */

#include <stdio.h>
#include <xview_private/portable.h>
#include <xview_private/ps_impl.h>
#include <xview_private/txt_18impl.h>
#include <xview/pkg.h>
#include <xview/attrol.h>
#include <xview/notice.h>
#include <xview/frame.h>
#include <xview/textsw.h>
#ifdef SVR4
#include <stdlib.h>
#endif /* SVR4 */
#include <unistd.h> 

static Es_status ps_commit(Es_handle esh);
static Es_handle ps_destroy(Es_handle esh);
static Es_handle ps_scratch_destroy(Es_handle esh);
static Es_index ps_get_length(Es_handle esh);
static Es_index ps_scratch_get_length(Es_handle esh);
static Es_index ps_get_position(Es_handle esh);
static Es_index ps_scratch_get_position(Es_handle esh);
static Es_index ps_read(Es_handle esh, int len, CHAR *bufp, int *resultp);
static Es_index ps_set_position(Es_handle esh, Es_index pos);
static Es_index ps_scratch_read(Es_handle esh, int len, CHAR *bufp, int *resultp);
static Es_index ps_replace(Es_handle esh, Es_index last_plus_one, int count, CHAR *buf, int *count_used);
static Es_index ps_scratch_replace(Es_handle esh, Es_index last_plus_one, int count, CHAR *buf, int *count_used);
static int ps_set(Es_handle esh, Attr_attribute *attrs);
static void copy_pieces(ft_handle to_table, int to_index, ft_handle from_table, int first, int last_plus_one);
static int get_current_offset(Piece_table private);

static Es_index write_header_etc(Es_handle esh, Piece_table private, Es_index last_plus_one, long int count, CHAR *buf, long int *count_used, Es_index *contents_start, int *deleted_pieces_length, int first_deleted, int last_plus_one_deleted);

static struct es_ops ps_ops = {
    ps_commit,
    ps_destroy,
    ps_get,
    ps_get_length,
    ps_get_position,
    ps_set_position,
    ps_read,
    ps_replace,
    ps_set
};

static int      max_transcript_alert_displayed;	/* default = 0 */

#define	INVALIDATE_CURRENT(private)			\
	(private)->current = CURRENT_NULL

#define	SET_POSITION(private, to)			\
	INVALIDATE_CURRENT(private); (private)->position = (to)

#define	VALID_PIECE_INDEX(private, index)		\
	((index < (private)->pieces.last_plus_one) &&	\
	(PIECES_IN_TABLE(private)[index].pos != ES_INFINITY))

static char    *wrap_msg = "\n\
*** Text is lost because the maximum edit log size has been exceeded. ***\n\n\n";

#ifdef DEBUG
#define	MAX_PIECE_LENGTH	 100000
#define	MAX_LENGTH		4000000
#endif

static Es_handle ps_NEW(int piece_count)
{
    Es_handle       esh = NEW(Es_object);
    register Piece_table private = NEW(struct piece_table_object);

    if (esh == NULL || private == NULL)
	goto AllocFailed;
    private->magic = PS_MAGIC;
    if (piece_count > 0) {
	private->pieces = FT_CLIENT_CREATE(piece_count, Piece_object);
	if (private->pieces.seq == NULL)
	    goto AllocFailed;
	FT_CLEAR_ALL(private->pieces);
    } else
	private->pieces.last_plus_one = 0;
    esh->data = (caddr_t) private;
    esh->ops = &ps_ops;
    return (esh);

AllocFailed:
    if (private)
	free((char *) private);
    if (esh)
	free((char *) esh);
    return ((Es_handle) NULL);
}

Pkg_private Es_handle ps_create(Xv_opaque client_data, Es_handle original, Es_handle scratch)
{
    Es_handle       esh = ps_NEW(100);
    register Piece_table private;
    register Piece  pieces;

    if (es_set_position(scratch, 0) != 0) {
	xv_error((Xv_opaque)scratch,
		 ERROR_STRING,
		 XV_MSG("ps_create(): cannot reset scratch stream"),
		 ERROR_PKG, TEXTSW,
		 NULL);
	return (NULL);
    }
    if (esh == NULL)
	goto AllocFailed;
    private = ABS_TO_REP(esh);
    SET_POSITION(private, 0);
    private->length = (original != ES_NULL) ? es_get_length(original) : 0;
    pieces = PIECES_IN_TABLE(private);
    if (private->length > 0) {
	pieces[0].pos = es_set_position(original, 0);
	PS_SET_ORIGINAL_SANDP(pieces[0], pieces[0].pos);
    }
    pieces[0].length = private->length;
    private->original = original;
    private->scratch = scratch;
    private->last_write_plus_one = ES_INFINITY;
    private->rec_start = ES_INFINITY;
    private->rec_insert = ES_INFINITY;
    private->oldest_not_undone_mark = ES_INFINITY;
    private->rec_insert_len = 0;
    private->client_data = client_data;
    private->scratch_max_len = ES_INFINITY;
    /* scratch_length need not be valid, but must be < scratch_max_len */
    private->scratch_length = 0;
    private->scratch_ops = (Es_ops) 0;
    private->status = ES_SUCCESS;
    return (esh);

AllocFailed:
    xv_error(XV_NULL,
	     ERROR_STRING,
	     XV_MSG("ps_create(): alloc failure"),
	     ERROR_PKG, TEXTSW,
	     NULL);
    return (NULL);
}

/* ARGSUSED */
static          Es_status ps_commit(Es_handle esh)
{
    return (ES_SUCCESS);
}

static Es_handle ps_destroy(Es_handle esh)
{
    register Piece_table private = ABS_TO_REP(esh);

    free((char *) esh);
    ft_destroy(&private->pieces);
    free((char *) private);
    return NULL;
}

static Es_handle ps_scratch_destroy(Es_handle esh)
{
    register Piece_table private = SCRATCH_TO_REP(esh);

    free((char *) esh->ops);
    esh->ops = private->scratch_ops;
    return (es_destroy(esh));
}

static          Es_index ps_get_length(Es_handle esh)
{
    register Piece_table private = ABS_TO_REP(esh);
    return (private->length);
}

static          Es_index ps_scratch_get_length(Es_handle esh)
{
    register Piece_table private = SCRATCH_TO_REP(esh);
    return (private->scratch_length);
}

static Es_index ps_get_position(Es_handle esh)
{
    register Piece_table private = ABS_TO_REP(esh);
    return (private->position);
}

static Es_index ps_scratch_get_position(Es_handle esh)
{
    register Piece_table private = SCRATCH_TO_REP(esh);
    return (private->scratch_position);
}

static Es_index ps_set_position(Es_handle esh, Es_index pos)
{
	register Piece_table private = ABS_TO_REP(esh);
	register Piece pieces = PIECES_IN_TABLE(private);

	if (pos > private->length) {
		ASSUME(pos == ES_INFINITY || private->parent);
		pos = private->length;
	}
	else if (pos < pieces[0].pos) {
		pos = pieces[0].pos;
		if (pos == ES_INFINITY)	/* Checks for empty source. */
			pos = 0;
	}
	ASSUME(0 <= pos && pos <= MAX_LENGTH);
	/*
	 * Client's often lose track of the position, so check if setting to
	 * current position to avoid invalidating caches unnecessarily.
	 */
	if (pos != private->position) {
		/* Try to maintain private->current */
		if (private->current != CURRENT_NULL) {
			if ((pos < pieces[private->current].pos) ||
					(pos >= pieces[private->current].pos +
							pieces[private->current].length))
				INVALIDATE_CURRENT(private);
		}
		private->position = pos;
	}
	return (private->position);
}

static Es_index ps_scratch_set_position(Es_handle esh, Es_index pos)
{
    register Piece_table private = SCRATCH_TO_REP(esh);

    if (pos > private->scratch_length)
	pos = private->scratch_length;
    private->scratch_position = pos;
    pos = pos % private->scratch_max_len;
    pos = private->scratch_ops->set_position(esh, pos);
    ASSUME(pos == (private->scratch_position % private->scratch_max_len));
    return (private->scratch_position);
}

#ifdef DEBUG
static int
ps_pieces_are_consistent(private)
    register Piece_table private;
{
    register Piece  pieces = PIECES_IN_TABLE(private);
    register Es_index pos = pieces[0].pos;
    register int    current;

    ASSUME(pos == 0 || pos == ES_INFINITY || private->parent);
    for (current = 0; VALID_PIECE_INDEX(private, current); current++) {
	ASSUME(pos == pieces[current].pos);
	ASSUME(0 < pieces[current].length || (current == 0 && pos == 0));
	ASSUME(pieces[current].length < MAX_PIECE_LENGTH);
	pos += pieces[current].length;
	ASSUME(pos <= private->length);
	ASSUME(PS_SANDP_POS(pieces[current]) < MAX_LENGTH);
    }
    return (!0);
}

#endif

static Es_index ps_scratch_read(Es_handle esh, int len, CHAR *bufp, int *resultp)
{
    register Piece_table private = SCRATCH_TO_REP(esh);
    register Es_index first_valid, last_plus_one;
    Es_index        remainder;
    int        count_read;

    if (!SCRATCH_HAS_WRAPPED(private)) {
	private->scratch_position =
	    private->scratch_ops->read(esh, len, bufp, resultp);
    } else {
	first_valid = SCRATCH_FIRST_VALID(private);
	last_plus_one = private->scratch_position + len;
	/*
	 * scratch stream has wrapped around, so valid "logical" scratch
	 * indices are [first_valid..scratch_length). Watch for: a) read of
	 * characters that no longer exist, and b) read that is "split"
	 * around physical scratch end. First, look to see if read begins at
	 * valid position.
	 */
	if (private->scratch_position < first_valid) {
	    /*
	     * Invalid start, but may extend into valid positions. However,
	     * caller does not know that start is invalid; only way to
	     * communicate this is to read nothing this time. Move position
	     * to start of valid range (which changes
	     * private->scratch_position as a side-effect).
	     */
	    es_set_position(esh, first_valid);
	    *resultp = 0;
	    goto Return;
	}
	/* Second, look for "split" read. */
	if ((private->scratch_position / private->scratch_max_len) !=
	    ((last_plus_one - 1) / private->scratch_max_len)) {
	    /* Split => read at end of scratch, then at start. */
	    remainder = private->scratch_max_len -
		private->scratch_ops->get_position(esh);
	    private->scratch_ops->read(esh, (int)remainder, bufp,
					      &count_read);
	    private->scratch_ops->set_position(esh, 0);
	    private->scratch_ops->read(esh, len - count_read,
					      bufp + count_read, resultp);
	    *resultp += count_read;
	} else {
	    (void) private->scratch_ops->read(esh, len, bufp, resultp);
	}
	/*
	 * Update the scratch_position, and watch out for read that ends
	 * exactly at multiple of scratch_max_len, because this requires
	 * repositioning of physical scratch stream to make succeeding calls
	 * work properly.
	 */
	private->scratch_position += *resultp;
	if ((private->scratch_position % private->scratch_max_len) ==
	    0) {
	    private->scratch_ops->set_position(esh, 0);
	}
    }
Return:
    ASSUME(private->scratch_ops->get_position(esh) ==
	   (private->scratch_position % private->scratch_max_len));
    ASSUME(ps_pieces_are_consistent(private));
    return (private->scratch_position);
}

static Es_index ps_scratch_replace(Es_handle esh, Es_index last_plus_one, int count, CHAR *buf, int *count_used)
{
	register Piece_table private = SCRATCH_TO_REP(esh);
	register Es_index first_valid, max_lpo;
	Es_index remainder;
	int count_replaced;

	if (last_plus_one > private->scratch_length) {
		last_plus_one = private->scratch_length;
	}
	max_lpo = private->scratch_position + count;
	if (last_plus_one > max_lpo) {
		max_lpo = last_plus_one;
	}
	ASSUME(last_plus_one >= private->scratch_position);
	if (!SCRATCH_HAS_WRAPPED(private) && (max_lpo <= private->scratch_max_len)) {
		int cu;

		private->scratch_position =
				private->scratch_ops->replace(esh, last_plus_one, count, buf,
				&cu);
		*count_used = cu;
		private->scratch_length = private->scratch_ops->get_length(esh);
	}
	else {
		first_valid = (SCRATCH_HAS_WRAPPED(private))
				? SCRATCH_FIRST_VALID(private) : 0;
		*count_used = count;
		/*
		 * Either scratch stream has wrapped around, or it is about to wrap
		 * around.  In either case, the valid "logical" scratch indices are
		 * [first_valid..scratch_length). Bytes that vanish into hole at
		 * start of scratch stream are treated as used. Watch for: a) range
		 * of characters that no longer exist, and b) "split" around physical
		 * scratch end. First, look to see if replace begins at valid
		 * position.
		 *
		 * 1) look to see if replace begins at valid position.
		 */
		remainder = first_valid - private->scratch_position;
		if (remainder > 0) {
			/*
			 * Invalid start, but may extend into valid positions. Move
			 * position to start of valid range (which changes
			 * private->scratch_position as a side-effect).
			 */
			(void)es_set_position(esh, first_valid);
			if (last_plus_one < first_valid) {
				/* Entire range to replace is invalid. */
				goto Return;
			}
			else if (count > 0) {
				/* Discard any new bytes replacing invalid bytes. */
				if (count > remainder) {
					count -= remainder;
					buf += remainder;
				}
				else {
					count = 0;
				}
			}
		}
		/* 2) look for "split" replace. */
		if ((private->scratch_position / private->scratch_max_len) !=
				((max_lpo - 1) / private->scratch_max_len)) {
			/*
			 * Split => replace at end of scratch, then at start. NOTE:
			 * caller guarantees that count > remainder
			 */
			remainder = private->scratch_max_len -
					private->scratch_ops->get_position(esh);
			ASSUME(count > remainder);
			private->scratch_ops->replace(esh,
					private->scratch_max_len, (int)remainder, buf, &count_replaced);
			private->scratch_ops->set_position(esh, 0);
			private->scratch_ops->replace(esh,
					count - remainder,
					(int)(count - remainder), buf + remainder, &count_replaced);
		}
		else {
			/* If here, then overwrite physical scratch after a wrap. */
			(void)private->scratch_ops->replace(esh,
					(private->scratch_position %
							private->scratch_max_len) + count,
					count, buf, &count_replaced);
		}
		private->scratch_position += count;
		/*
		 * Watch out for replaces that end exactly at a multiple of
		 * scratch_max_len, because this requires repositioning of
		 * physical scratch stream to make succeeding calls work properly.
		 */
		if ((private->scratch_position % private->scratch_max_len) == 0) {
			private->scratch_ops->set_position(esh, 0);
		}
		if (private->scratch_position > private->scratch_length) {
			private->scratch_length = private->scratch_position;
		}
	}
  Return:
	ASSUME(private->scratch_ops->get_position(esh) ==
			(private->scratch_position % private->scratch_max_len));
	return (private->scratch_position);
}

static Piece split_piece(ft_handle pieces, int current, long delta)
/*
 * Splits the piece_object pieces[current] in two with the second piece, the
 * new pieces[current+1], starting at pieces[current].pos+delta. Returns a
 * properly cast sequence, as old cached version may be invalidated by the
 * call to ft_shift_up.
 */
{
    register Piece  old, new;
    register int    is_scratch;
    register Es_index source_pos;

    ft_shift_up(pieces, current + 1, current + 2, 10);
    /* Can invalidate &pieces.seq[i] */
    old = ((Piece) (pieces->seq)) + current;
    new = old + 1;
    is_scratch = PS_SANDP_SOURCE(old[0]);
    source_pos = PS_SANDP_POS(old[0]);
    new->pos = old->pos + delta;
    source_pos += delta;
    new->length = old->length - delta;
    old->length = delta;
    PS_SET_SANDP(new[0], source_pos, is_scratch);
    return ((Piece) (pieces->seq));
}

static Es_index ps_read(Es_handle esh, int len, CHAR *bufp, int *resultp)
{
#ifdef DEBUG_TRACE
    Es_index        next, pos;
    CHAR            temp;
    pos = (ABS_TO_REP(esh))->position;
    next = ps_debug_read(esh, len, bufp, resultp);
    (void) fprintf(stdout,
		   "ps_read at pos %d of %d chars got %d chars; %s %d ...\n",
		   pos, len, *resultp, "next read at", next);
#ifdef	DEBUG_TRACE_CONTENTS
    temp = bufp[*resultp];
    bufp[*resultp] = '\0';
    (void) fprintf(stdout, "%s\n^^^\n", bufp);
    bufp[*resultp] = temp;
#endif
    return (next);
}

static Es_index ps_debug_read(Es_handle esh, int len, CHAR *bufp, int *resultp)
{
#endif

	/*
	 * "current" must be signed, because it can become -1 when deciding
	 * whether to combine pieces containing invalid physical scratch indices.
	 */
	int read_count, save_current;
	Es_index next_pos, original_len;
	register int current, to_read;
	register long int delta;
	register Es_index current_pos;
	register Es_handle current_esh;
	register Piece pieces;
	register Piece_table private = ABS_TO_REP(esh);

#ifdef OW_I18N
	static CHAR *wrap_msg_wcs;
#endif

	if (private->length - private->position < len) {
		len = private->length - private->position;
	}
	pieces = PIECES_IN_TABLE(private);
	*resultp = 0;
	if (private->current == CURRENT_NULL) {
		current = ft_bounding_index(&private->pieces, private->position);
	}
	else
		current = private->current;
	while (VALID_PIECE_INDEX(private, current) && len > 0) {
		delta = private->position - pieces[current].pos;
		ASSERT(*resultp == 0 || delta == 0);
		current_esh = (PS_SANDP_SOURCE(pieces[current]) ?
				private->scratch : private->original);
		current_pos = PS_SANDP_POS(pieces[current]) + delta;
		next_pos = es_set_position(current_esh, current_pos);
		ASSERT(next_pos == current_pos);
		to_read = pieces[current].length - delta;
		if (to_read > len)
			to_read = len;


		next_pos = es_read(current_esh, to_read, bufp, &read_count);

		/*
		 * BUG ALERT!  If we ever support entities that are not bytes, the
		 * following addition must have a "* es_get(current_esh,
		 * ES_SIZE_OF_ENTITY)"
		 */
		bufp += read_count;
		len -= read_count;
		*resultp += read_count;
		private->position += read_count;	/* zaps private->current */
		if (read_count < to_read) {
			if (current_esh == private->original) {
				/*
				 * The original entity stream has holes in it, and the
				 * initialization assumed it was contiguous, so the pieces
				 * and length must be corrected.
				 */
				pieces = split_piece(&private->pieces, current,
						delta + read_count);
				current++;
				/* Compute the gap size */
				delta = next_pos - (current_pos + read_count);
				ASSERT(pieces[current].length > delta);
				pieces[current].length -= delta;
				private->length -= delta;
				PS_SET_ORIGINAL_SANDP(pieces[current], next_pos);
			}
			else {
				/*
				 * The scratch entity stream has wrapped around, making a
				 * hole from [0..scratch_first_valid).
				 */
				ASSUME(SCRATCH_HAS_WRAPPED(private) ||
						(private->parent &&
								SCRATCH_HAS_WRAPPED(ABS_TO_REP(private->
												parent))));
				/*
				 * Caller needs to be told where next successful read is, so
				 * search for piece containing valid scratch indices
				 * (WARNING: there may not be one).
				 */
				for (save_current = current,
						current_pos = private->position;
						VALID_PIECE_INDEX(private, current); current++) {
					ASSERT(PS_SANDP_SOURCE(pieces[current]));
					delta = PS_SANDP_POS(pieces[current]);
					if (delta >= next_pos) {
						/*
						 * next_pos not referenced by pieces => go to start
						 * this piece, which is first beyond next_pos
						 */
						next_pos = delta;
					}
					else if (next_pos >= delta + pieces[current].length) {
						continue;
					}
					private->position =
							(next_pos - delta) + pieces[current].pos;
					/* Allow uniform handling of current below. */
					current++;
					break;
				}
				if (current_pos == private->position)
					private->position = es_get_length(esh);
				/*
				 * Although the pieces are correct it speeds up later reads
				 * if "runs of now invalid scratch references" are combined;
				 * however, only completely invalid pieces can be combined
				 * (else the record headers will be incorrectly included as
				 * part of the visible text). -2 backs over terminating and
				 * partially valid pieces.
				 */
				current -= 2;
				if (save_current < current) {
					delta = PS_LAST_PLUS_ONE(pieces[current]);
					pieces[save_current].length =
							delta - pieces[save_current].pos;
					ft_shift_out(&private->pieces,
							save_current + 1, current + 1);
				}
				/*
				 * A read at the beginning of the scratch stream which tries
				 * to read out of the start of the hole should give the
				 * client (an appropriate part of) wrap_msg. However, don't
				 * do this until AFTER finding out where next valid position
				 * is, otherwise can return too much text.
				 */
				for (current = 0, original_len = 0;
						VALID_PIECE_INDEX(private, current); current++) {
					if (PS_SANDP_SOURCE(pieces[current]))
						break;
					original_len = PS_LAST_PLUS_ONE(pieces[current]);
				}
				current = private->pieces.last_plus_one;
				INVALIDATE_CURRENT(private);
				ASSUME(original_len <= current_pos);

#ifdef OW_I18N
				wrap_msg_wcs = _xv_mbstowcsdup(wrap_msg);
				read_count = STRLEN(wrap_msg_wcs);
#else
				read_count = strlen(wrap_msg);
#endif

				if (*resultp == 0 && current_pos < original_len + read_count) {
					FILE *console_fd;

					if (private->position < original_len + read_count)
						read_count = private->position - original_len;
					read_count -= current_pos - original_len;
					if (len < read_count)
						read_count = len;

#ifdef OW_I18N
					BCOPY(wrap_msg_wcs + current_pos, bufp, read_count);
#else
					XV_BCOPY(wrap_msg + current_pos, bufp, (size_t)read_count);
#endif

					*resultp = read_count;
					/* tell user that they are going to wrap */

					/* Use the same flag for console error message */
					if (max_transcript_alert_displayed != 1) {
						if ((console_fd = fopen("/dev/console", "a"))) {
							fprintf(console_fd,
									XV_MSG
									("Text has been lost in a cmdtool transcript because the maximum edit log size has been exceeded.\n"));
							fflush(console_fd);
							max_transcript_alert_displayed = 1;
							fclose(console_fd);
						}
					}
				}

#ifdef OW_I18N
				if (wrap_msg_wcs)
					free((char *)wrap_msg_wcs);
#endif
			}
			/*
			 * All of the above code is free and easy with local variables
			 * because it expects to exit the loop here, so it will need to
			 * be rewritten if this break is removed!
			 */
			break;
		}
		current++;
	}
	if (VALID_PIECE_INDEX(private, current)) {
		private->current = current;
		if (pieces[current].pos > private->position)
			private->current--;
		/* Short read advances current anyway, so undo the advance. */
	}
	else
		INVALIDATE_CURRENT(private);
	ASSUME(ps_pieces_are_consistent(private));
	return (private->position);
}

/*
 * The routines ps_replace, ps_insert_pieces, and ps_undo_to_mark all perform
 * similar functions and a fix to one needs to be reflected in the others.
 */
static Es_index ps_replace(Es_handle esh, Es_index last_plus_one, int count, CHAR *buf, int *count_used)
{
#ifdef DEBUG_TRACE
    Es_index        next, pos;
    CHAR            temp;
    pos = (ABS_TO_REP(esh))->position;
    next = ps_debug_replace(esh, last_plus_one, count, buf, count_used);
    (void) fprintf(stdout,
	     "ps_replace [%d..%d) by %d chars (used %d); next pos %d ...\n",
	      pos, last_plus_one, count, *count_used, "next read at", next);
    if (buf) {
	temp = buf[count];
	buf[count] = '\0';
	(void) fprintf(stdout, "%s\n^^^\n", buf);
	buf[count] = temp;
    }
    return (next);
}

static          Es_index
ps_debug_replace(esh, last_plus_one, count, buf, count_used)
    Es_handle       esh;
    Es_index        last_plus_one;
    long int        count, *count_used;
    CHAR           *buf;
{
#endif

	/*
	 * Assume that the piece stream looks like: O1O S1S O2O S2S S3S O3O O4O
	 * S4S where XnX means n-th chunk in stream X, and X2X may actually occur
	 * earlier in stream X than X1X. The possible cases are: 1) Replace an
	 * empty region, with subcases: 1a)  Region is at end of previous
	 * replace, hence it can be treated as an extension of the previous
	 * replace, and the associated scratch piece can simply be lengthened.
	 * (S4S) 1b)  Otherwise, a new piece must be created, and the piece that
	 * contains the empty region must be split (XnX -> XnX S5S XmX), unless
	 * the empty region is at the beginning of that piece (XnX -> S5S XnX).
	 * 2) Replace a region completely within one piece: 2a)  If within latest
	 * scratch region, (1a) applies. 2b)  If the region exactly matches the
	 * piece, re-cycle the piece to point at the new contents. 2c) Otherwise,
	 * (1b) applies. 3) Replace a region spanning multiple pieces. 3a)  For
	 * partially enclosed pieces, (1) applies. 3b)  For completely enclosed
	 * pieces, delete them all, except possibly one to use as in (2b) if (3a)
	 * does not occur.
	 *
	 * WARNING!  Changes to the pieces can only be made AFTER calls to
	 * es_replace(scratch, ...) succeed, unless those changes do not break
	 * the assertions about the pieces OR are backed out in case of
	 * es_replace() failing.
	 */
	Es_index scratch_length;
	Es_index es_temp, new_rec_insert;
	long int long_temp;
	int delete_pieces_length, replace_used, old_start;
	unsigned int first_valid;
	register int current, end_index;
	register long int delta;	/* Has 2 meanings! */
	register Es_handle scratch;
	register Piece pieces;
	register Piece_table private = ABS_TO_REP(esh);
	long cu = 0;

	*count_used = 0;
	if (buf == NULL && count != 0) {
		private->status = ES_INVALID_ARGUMENTS;
		return (ES_CANNOT_SET);
	}
	if (private->parent != NULL) {
		private->status = ES_INVALID_HANDLE;
		return (ES_CANNOT_SET);
	}
	scratch = private->scratch;
	scratch_length = es_set_position(scratch, ES_INFINITY);
	new_rec_insert = ES_INFINITY;	/* => extending current */
	if (last_plus_one > private->length) {
		last_plus_one = private->length;
	}
	ASSUME(last_plus_one >= private->position);
	pieces = PIECES_IN_TABLE(private);
	if (private->length == 0) {
		/* Special case occurring after everything replaced by nothing. */
		ASSUME(pieces[0].pos == ES_INFINITY);
		if (count == 0) {
			/* Nothing replaced by nothing => leave well enough alone! */
			return (private->position);
		}
		pieces[current = 0].pos = 0;
		goto Append_Only;
	}
	delta = get_current_offset(private);
	current = private->current;
	if (last_plus_one > private->position) {
		/* Replace a NON-empty region */
		if (delta != 0) {
			/* Split the current piece at the insert point. */
			pieces = split_piece(&private->pieces, current, delta);
			current++;
		}
		if (last_plus_one > PS_LAST_PLUS_ONE(pieces[current])) {
			/* Replace region spanning multiple pieces */
			end_index = ft_bounding_index(&private->pieces, last_plus_one - 1);
			/*
			 * -1 ensures that pieces[end_index] < last_plus_one, preventing
			 * creation of a 0-length piece below.
			 */
			ASSERT(VALID_PIECE_INDEX(private, end_index));
		}
		else {
			/* Replace region within a single piece */
			end_index = current;
		}
		if (last_plus_one < PS_LAST_PLUS_ONE(pieces[end_index])) {
			/* Split the end piece at the last_plus_one point */
			pieces = split_piece(&private->pieces, end_index,
					(long)(last_plus_one - pieces[end_index].pos));
		}
		/*
		 * Replace region now exactly equals [current..end_index] pieces.
		 * Make sure all es_replace(scratch, ...) succeed before modifying
		 * private->... (including pieces) any further.
		 */
		new_rec_insert =
				write_header_etc(scratch, private, last_plus_one,
				(long)count, buf, &cu,
				&es_temp, &delete_pieces_length, current, end_index + 1);
		if (new_rec_insert == ES_CANNOT_SET) {
			goto Error_Return;
		}
		*count_used = cu;
		if (count == 0) {
			current--;	/* Make ft_shift_out & ft_add_delta work */
		}
		/* Recycle current piece, so don't shift it out */
		ft_shift_out(&private->pieces, current + 1, end_index + 1);
		/* Meaning 2: total change to length */
		delta = count - delete_pieces_length;
	}
	else if (count == 0) {
		/*
		 * Empty region replaced by nothing => leave well enough alone! Not
		 * catching this case creates an extra 0-length piece.
		 */
		return (private->position);
	}
	else {
		/* Replace an empty region */
		int at_end_of_stream = PS_IS_AT_END(private, delta, current);

		if (private->position == private->last_write_plus_one) {
			es_temp = es_replace(scratch, ES_INFINITY, count, buf, count_used);
			if (es_temp == ES_CANNOT_SET) {
				goto Error_Return;
			}
			ASSUME(count == *count_used);
			if (private->rec_insert_len == 0) {
				/* Last replace was a delete, so no scratch piece exists */
				/* Split the current piece at the insert point */
				pieces = split_piece(&private->pieces, current, delta);
				if (delta != 0) {
					ASSUME(at_end_of_stream);
					current++;
					delta = 0;
				}
				PS_SET_SCRATCH_SANDP(pieces[current],
						private->rec_insert + SIZEOF(long_temp));
			}
			else if (at_end_of_stream) {
				/* Last replace was at end of stream => don't back up */
			}
			else {
				ASSERT(delta == 0 && current > 0);
				current--;
				delta = private->position - pieces[current].pos;
			}
			ASSERT(delta == pieces[current].length);
			delta = count;	/* Meaning 2: total change to length */
			private->rec_insert_len += delta;
			pieces[current].length += delta;
			/* Correct the insert length (this is very inefficient) */
			long_temp = private->rec_insert_len;
			es_temp = es_set_position(scratch, private->rec_insert);
			ASSERT(es_temp == private->rec_insert);
			/*
			 *  BIG BUG:  sizeof(long_temp) has to be divisable by sizeof(CHAR)
			 */
			es_temp = es_replace(scratch,
					(Es_index)(private->rec_insert + SIZEOF(long_temp)),
					(unsigned)SIZEOF(long_temp), (CHAR *) & long_temp, &replace_used);
		}
		else {
			if (delta != 0) {
				if (SCRATCH_HAS_WRAPPED(private)
						&& (current > private->pieces.last_plus_one - 3)
						&& (private->length > private->scratch_max_len * 2)
						&& (current > 30)) {
					pieces = PIECES_IN_TABLE(private);
					old_start = private->pieces.last_plus_one / 2 - 13;
					first_valid = SCRATCH_FIRST_VALID(private);
					if (PS_SANDP_POS(pieces[old_start]) >= first_valid)
						if (PS_SANDP_POS(pieces[old_start /= 2]) >= first_valid)
							old_start = 0;
					for (old_start++;
							PS_SANDP_POS(pieces[old_start]) < first_valid;
							old_start++);
					old_start -= 2;

					ft_shift_out(&private->pieces, 10, old_start);
					INVALIDATE_CURRENT(private);
					current = current - (old_start - 10);
				}
				/* Split the current piece at the insert point */
				pieces = split_piece(&private->pieces, current, delta);
				current++;
				if (at_end_of_stream) {
					/*
					 * Append at the very end of the stream will create a
					 * zero length piece which is unnecessary, so we detect
					 * this case and recycle the piece.
					 */
					goto Append_Only;
				}
			}
			/* Handle the delta == 0 case.  Create new piece */
			pieces = split_piece(&private->pieces, current, 0L);
		  Append_Only:
			/*
			 * Make sure all es_replace(scratch, ...) succeed before
			 * modifying private->... (including pieces) any further.
			 */
			new_rec_insert =
					write_header_etc(scratch, private,last_plus_one,(long)count,
					buf, &cu, &es_temp, (int *)0, -1, -1);
			*count_used = cu;
			if (new_rec_insert == ES_CANNOT_SET) {
				/* Reverse all damaging changes to pieces */
				if (private->length == 0) {
					pieces[0].pos = ES_INFINITY;
				}
				else if (pieces[current].length == 0) {
					ft_shift_out(&private->pieces, current, current + 1);
				}
				goto Error_Return;
			}
			delta = count;	/* Meaning 2: total change to length */
		}
	}
	/*
	 * Update (if necessary) the fields tracking the insert record. Also fix
	 * up the associated piece fields.
	 */
	if (new_rec_insert != ES_INFINITY) {
		private->rec_insert = new_rec_insert;
		private->rec_insert_len = count;
		private->rec_start = scratch_length;
		if (private->oldest_not_undone_mark == ES_INFINITY)
			private->oldest_not_undone_mark = scratch_length;
		ASSUME(count == *count_used);
		if (count != 0) {
			pieces[current].length = count;
			ASSERT(es_temp == private->rec_insert + SIZEOF(long_temp));
			PS_SET_SCRATCH_SANDP(pieces[current], es_temp);
		}
	}
	/* Adjust position, etc. to reflect the overall replace */
	if ((current + 1) < private->pieces.last_plus_one)
		ft_add_delta(private->pieces, current + 1, delta);
	private->length += delta;
	SET_POSITION(private, private->position + count);
	private->last_write_plus_one = private->position;
	ASSUME(0 <= private->position && private->position <= MAX_LENGTH);
	ASSUME(ps_pieces_are_consistent(private));
	return (private->position);

  Error_Return:
	private->status = (Es_status) es_get(scratch, ES_STATUS);
	/*
	 * Copy scratch status first in case attempt to "back out" failed
	 * es_replace's modify the status.
	 */
	(void)es_set_position(scratch, scratch_length);
	(void)es_replace(scratch, ES_INFINITY, 0, NULL, count_used);
	INVALIDATE_CURRENT(private);
	ASSUME(ps_pieces_are_consistent(private));
	return (ES_CANNOT_SET);
}

static Es_index write_record_header(Es_handle esh, Piece_table private, Es_index last_plus_one, int dp_count)
{
    struct piece_record_header r_header;
    Es_index result;
    int             replace_used;

    r_header.pos_prev_rec = private->rec_start;
    r_header.flags = 0;
    r_header.start = private->position;
    r_header.stop_plus_one = last_plus_one;
    r_header.dp_count = dp_count;
    /*
     *  BIG BUG:  This is a very bad hack.  If sizeof(r_header)
     *  is not divisable by sizeof(CHAR), then we will be in big problem.
     */
    result = es_replace(esh, ES_INFINITY,
			(unsigned)SIZEOF(r_header), (CHAR *) &r_header,
			&replace_used);
    return (result);
}

static int record_deleted_pieces(Es_handle esh, Piece pieces, int first,int last_plus_one, Es_index *next)
{
    struct deleted_piece d_header;
    int             dummy;
    Piece  current, stop_plus_one;
    register int    result = 0;
    register Es_index replace_result = 0;

    stop_plus_one = &pieces[last_plus_one];
    for (current = &pieces[first]; current < stop_plus_one; current++) {
	d_header.source_and_pos = current->source_and_pos;
	result += (d_header.length = current->length);
    /*
     *  BIG BUG:  This is a very bad hack.  If sizeof(d_header)
     *  is not divisable by sizeof(CHAR), then we will be in big problem.
     */
	replace_result = es_replace(esh, ES_INFINITY,
		       (unsigned)SIZEOF(d_header), (CHAR *) &d_header, &dummy);
	if (replace_result == ES_CANNOT_SET)
	    break;
    }
    *next = replace_result;
    return (result);
}

/* Returns the new value for private->rec_insert */
static Es_index write_header_etc(Es_handle esh, Piece_table private, Es_index last_plus_one, long int count, CHAR *buf, long int *count_used, Es_index *contents_start, int *deleted_pieces_length, int first_deleted, int last_plus_one_deleted)
{
	register Es_index result;
	Es_index es_temp;
	int replace_used;

	/* Write the record header (and possibly) deleted pieces. */
	result = write_record_header(esh, private, last_plus_one,
			last_plus_one_deleted - first_deleted);
	if (result == ES_CANNOT_SET)
		return (ES_CANNOT_SET);
	if (first_deleted < last_plus_one_deleted) {
		*deleted_pieces_length =
				record_deleted_pieces(esh, PIECES_IN_TABLE(private),
				first_deleted, last_plus_one_deleted, &es_temp);
		if (es_temp == ES_CANNOT_SET)
			return (ES_CANNOT_SET);
		result = es_temp;
	}
	/*
	 *  BIG BUG:  sizeof(count) has to be divisable by sizeof(CHAR)
	 */
	*contents_start =
			es_replace(esh, ES_INFINITY, (unsigned)SIZEOF(count),
			(CHAR *) & count, &replace_used);
	if (*contents_start == ES_CANNOT_SET)
		return (ES_CANNOT_SET);
	/* Do the insert */
	if (count != 0) {
		int cu = *count_used;

		es_temp = es_replace(esh, ES_INFINITY, (int)count, buf, &cu);
		if (es_temp == ES_CANNOT_SET)
			return (ES_CANNOT_SET);
		*count_used = cu;
	}
	return (result);
}

static Es_handle ps_pieces_for_span(Es_handle esh, Es_index first, Es_index last_plus_one, Es_handle to_recycle)
{
    register Piece_table private = ABS_TO_REP(esh);
    register Piece  result_pieces;
    register int    is_scratch;
    register Es_index delta, source_pos;
    int             first_index, last_index;
    Es_handle       result = (Es_handle) 0;
    Piece_table     r_private;

    if (last_plus_one > private->length)
	last_plus_one = private->length;
    if (first >= last_plus_one) {
	goto Bad_Args;
    }
    ASSUME(allock());
    first_index = ft_bounding_index(&private->pieces, first);
    last_index = ft_bounding_index(&private->pieces, last_plus_one - 1);
    if (to_recycle) {
	result = to_recycle;
	r_private = ABS_TO_REP(result);
	ASSUME(r_private->magic == PS_MAGIC && r_private->parent == esh);
	if (r_private->pieces.last_plus_one <= (last_index - first_index)) {
	    ft_destroy(&r_private->pieces);	/* Zeros .last_plus_one */
	}
    } else {
	result = ps_NEW(0);
	if (result == (Es_handle) 0)
	    goto Out_Of_Memory;
	r_private = ABS_TO_REP(result);
	r_private->parent = esh;
	r_private->original = private->original;
	r_private->scratch = private->scratch;
	r_private->last_write_plus_one = r_private->rec_insert =
	    r_private->rec_start = ES_CANNOT_SET;
	r_private->rec_insert_len = -1;
    }
    if (r_private->pieces.last_plus_one == 0) {
	r_private->pieces =
	    FT_CLIENT_CREATE(last_index - first_index + 1, Piece_object);
	if (r_private->pieces.seq == (Es_index *) 0)
	    goto Out_Of_Memory;
    }
    FT_CLEAR_ALL(r_private->pieces);
    copy_pieces(&r_private->pieces, 0, &private->pieces,
		first_index, last_index + 1);
    /*
     * Fix up the end pieces.
     */
    result_pieces = &PIECES_IN_TABLE(r_private)[last_index - first_index];
    result_pieces->length = last_plus_one - result_pieces->pos;
    result_pieces = &PIECES_IN_TABLE(r_private)[0];
    delta = first - result_pieces->pos;
    if (delta != 0) {
	result_pieces->pos = first;
	is_scratch = PS_SANDP_SOURCE(*result_pieces);
	source_pos = PS_SANDP_POS(*result_pieces);
	source_pos += delta;
	result_pieces->length -= delta;
	PS_SET_SANDP(*result_pieces, source_pos, is_scratch);
    }
    SET_POSITION(r_private, first);
    r_private->length = last_plus_one;
    ASSUME(allock());
    return (result);

Out_Of_Memory:
    if (result) {
	es_destroy(result);
	result = (Es_handle) 0;
    }
Bad_Args:
    if (to_recycle)
	es_destroy(to_recycle);
    return (result);
}

static void copy_pieces(ft_handle to_table, int to_index, ft_handle from_table, int first, int last_plus_one)
{
    register int    sizeof_element = from_table->sizeof_element;
    ASSERT(to_table->sizeof_element == sizeof_element);
    XV_BCOPY((char *) from_table->seq + first * sizeof_element,
	  (char *) to_table->seq + to_index * sizeof_element,
	  (size_t)((last_plus_one - first) * sizeof_element));
}

static int get_current_offset(Piece_table private)
{
    register Piece  current;
    register int    result;

    if (private->current == CURRENT_NULL)
	private->current =
	    ft_bounding_index(&private->pieces, private->position);
    ASSERT(VALID_PIECE_INDEX(private, private->current));
    current = &PIECES_IN_TABLE(private)[private->current];
    result = private->position - current->pos;
    ASSUME(result < current->length ||
	   PS_IS_AT_END(private, result, private->current));
    return (result);
}

static void ps_insert_pieces(Es_handle esh,Es_handle to_insert)
/*
 * Much of this routine is stolen from ps_replace. A special convention
 * established by this routine is that if the scratch stream has a replace
 * record with start == stop_plus_one and a non-zero deleted piece count,
 * then it is a result of inserting pieces, rather than characters.
 */
{
    ft_handle       rep = (ft_handle)
    & (ABS_TO_REP(to_insert))->pieces;
    long int        long_temp;
    int             at_end_of_stream, replace_used;
    long int        delta;
    int             save_last_plus_one;
    Es_index        scratch_length, es_temp;
    register Piece  pieces;
    register int    current, last_valid;
    register Piece_table private = ABS_TO_REP(esh);
    register Es_handle scratch = private->scratch;

    ASSERT(rep != FT_NULL);
    last_valid = ft_bounding_index(rep, ES_INFINITY - 1);
    ASSERT(last_valid != rep->last_plus_one);
    /* Prepare the insertion area. */
    pieces = PIECES_IN_TABLE(private);
    if (private->length == 0 && pieces[0].pos == ES_INFINITY) {
	/* Special case that occurs if replaced everything by nothing. */
	current = 0;
	delta = 0;
	at_end_of_stream = 1;
	pieces[0].pos = 0;
	pieces[0].length = 0;
	PS_SET_SCRATCH_SANDP(pieces[0], 0);
    } else {
	INVALIDATE_CURRENT(private);
	delta = get_current_offset(private);
	current = private->current;
	if (delta == 0) {
	    at_end_of_stream = 0;
	} else {
	    at_end_of_stream = (delta == pieces[current].length);
	    /*
	     * Split the current piece at the insert point. This creates a
	     * zero length piece iff at_end_of_stream.
	     */
	    pieces = split_piece(&private->pieces, current, delta);
	    current++;
	}
    }
#ifdef DEBUG
    long_temp = pieces[current].pos;
#endif
    /* Do the insert and associated bookkeeping */
    ft_shift_up(&private->pieces, current, current + last_valid + 1,
		last_valid + 1);
    pieces = PIECES_IN_TABLE(private);
    copy_pieces(&private->pieces, current, rep, 0, last_valid + 1);
    ASSERT(allock());
    /*
     * Offset inserted pieces so they match the insertion point. BUG ALERT:
     * need version of ft_add_delta that takes stop index, but for now,
     * monkey with the piece table itself.
     */
    save_last_plus_one = private->pieces.last_plus_one;
    private->pieces.last_plus_one = current + last_valid + 1;
    if (current < private->pieces.last_plus_one)
	ft_add_delta(private->pieces, current,
		     (long)(private->position - rep->seq[0]));
    private->pieces.last_plus_one = save_last_plus_one;
    ASSERT(long_temp == pieces[current + last_valid + 1].pos);
    /* Write header info to scratch. */
    scratch_length = es_set_position(scratch, ES_INFINITY);
    es_temp = write_record_header(scratch, private, private->position,
				  last_valid + 1);
    if (es_temp != ES_CANNOT_SET) {
	/* Can modify private->... iff es_replace succeeds */
	private->rec_insert = es_temp;
	private->rec_start = scratch_length;
	if (private->oldest_not_undone_mark == ES_INFINITY)
	    private->oldest_not_undone_mark = scratch_length;
    }
    delta = record_deleted_pieces(
				  scratch, pieces,
				  current, current + last_valid + 1,
				  &private->rec_insert);
    long_temp = 0;
    /*
     * BIG BUG:  sizeof(long_temp) has to be divisable by sizeof(CHAR)
     */
    es_replace(scratch, ES_INFINITY,
		      (unsigned)SIZEOF(long_temp), (CHAR *) &long_temp,
		      &replace_used);
    ASSERT(allock());
    /* Adjust position, etc. to reflect the overall replace */
    if (at_end_of_stream) {
	/* Flush the extraneous zero length piece we created. */
	ASSERT(pieces[current + last_valid + 1].length == 0);
	pieces[current + last_valid + 1].pos = ES_INFINITY;
    } else {
	if ((current + last_valid + 1) < private->pieces.last_plus_one)
	    ft_add_delta(private->pieces, current + last_valid + 1, delta);
    }
    private->last_write_plus_one = ES_INFINITY;
    private->length += delta;
    SET_POSITION(private, private->position + delta);
}

static Es_index adjust_pos_after_edit(Es_index pos, Es_index start, Es_index delta)
{
    if (delta > 0)
	return ((start <= pos) ? pos + delta : pos);
    else if (start < pos)
	return ((start - delta < pos) ? pos + delta : start);
    else
	return (pos);
}

static void ps_undo_to_mark(Es_handle esh, Es_index mark, int (*notify_proc)(caddr_t d, Es_index, Es_index), caddr_t notify_data)
/*
 * The notify_proc gets called with the notify_data, the start of the
 * affected range, and the delta.  Thus a negative delta means
 * [start..start-delta) contains the affected positions, while a positive
 * delta indicates an insertion of entities to fill the range
 * [start..start+delta).
 */
{
	register Piece_table private = ABS_TO_REP(esh);
	register Es_handle scratch = private->scratch;
	register Piece pieces, this_piece;
	register Es_index current_pos, scratch_pos, save_pos = private->position;
	Es_index delta, mark_pos = mark;
	struct piece_record_header r_header;
	struct deleted_piece d_header;
	int current, i, read;
	int insert_len;

	if (es_get_length(scratch) == 0)
		return;
	/*
	 * For a bounded scratch stream, the mark_pos may now be invalid and need
	 * to be adjusted.  The new value need not be exactly at a record header
	 * (luckily, else all of the record headers would have to be read twice).
	 */
	if (SCRATCH_HAS_WRAPPED(private) && mark_pos < SCRATCH_FIRST_VALID(private)) {
		mark_pos = SCRATCH_FIRST_VALID(private);
	}
	/* Read back the information from the scratch source and undo it */
	for (; (private->rec_start != ES_INFINITY) &&
			(private->rec_start >= mark_pos);
			private->rec_start = r_header.pos_prev_rec) {
		(void)es_set_position(scratch, private->rec_start);
		/*
		 *  BIG BUG:  sizeof(r_header) has to be divisable by sizeof(CHAR)
		 */
		es_read(scratch, (unsigned)SIZEOF(r_header),(CHAR *) & r_header, &read);
		ASSUME(read == SIZEOF(r_header));
		/*
		 * Check to see if piece is flagged as already undone. If not, make
		 * sure it is flagged now.
		 */
		if (r_header.flags & PS_ALREADY_UNDONE)
			continue;
		r_header.flags |= PS_ALREADY_UNDONE;
		(void)es_set_position(scratch, private->rec_start);
		/*
		 *  BIG BUG:  sizeof(r_header) has to be divisable by sizeof(CHAR)
		 */
		es_replace(scratch, (Es_index)(private->rec_start + SIZEOF(r_header)),
				(unsigned)SIZEOF(r_header), (CHAR *) & r_header, &read);
		ASSUME(read == SIZEOF(r_header));
		if (private->oldest_not_undone_mark == private->rec_start)
			private->oldest_not_undone_mark = ES_INFINITY;
		current_pos = r_header.start;
		SET_POSITION(private, current_pos);
		if (private->length == 0 &&
				PIECES_IN_TABLE(private)[0].pos == ES_INFINITY) {
			/*
			 * Special case occurs when nothing replaced everything. BUG
			 * ALERT: later on, we depend on the [0].pos being left as
			 * ES_INFINITY, else the ft_shift_up corrupts the piece table.
			 */
			current = 0;
			delta = 0;
		}
		else {
			delta = get_current_offset(private);
			current = private->current;
		}
		if ((r_header.start == r_header.stop_plus_one) &&
				(r_header.dp_count != 0)) {
			/*
			 * Remove the inserted pieces. Since ps_replace does NOT coalesce
			 * pieces, a single inserted piece may be turned into multiple
			 * pieces via sequences of "type-in edit-chars-deleting type-in",
			 * and UNDO of those sequences also does NOT coalesce the pieces,
			 * so when we go to UNDO the original insert we must not rely on
			 * a one-to-one mapping of the pieces.
			 */
			int piece_length, piece_count = 0;

			ASSERT(delta == 0);
			pieces = PIECES_IN_TABLE(private);
			this_piece = &pieces[current];
			for (i = r_header.dp_count; i > 0; i--) {
				/*
				 *  BIG BUG:  sizeof(d_header) has to be divisable
				 *  by sizeof(CHAR)sizeof
				 */
				es_read(scratch, (unsigned)SIZEOF(d_header),
						(CHAR *) & d_header, &read);
				ASSERT(this_piece->pos == current_pos - delta);
				ASSERT(this_piece->source_and_pos == d_header.source_and_pos);
				delta -= d_header.length;
				for (piece_length = d_header.length; piece_length > 0;
						piece_count++) {
					ASSERT(this_piece->length <= piece_length);
					piece_length -= this_piece->length;
					this_piece++;
				}
			}
			ft_shift_out(&private->pieces, current, current + piece_count);
			if (current < private->pieces.last_plus_one)
				ft_add_delta(private->pieces, current, (long)delta);
			save_pos = adjust_pos_after_edit(save_pos, current_pos, delta);
			private->length += delta;
			if (notify_proc) {
				scratch_pos = es_get_position(scratch);
				notify_proc(notify_data, current_pos, delta);
				(void)es_set_position(scratch, scratch_pos);
			}
		}
		else {
			/* Put back the deleted pieces */
			if (delta == 0) {
				if (current < private->pieces.last_plus_one)
					ft_add_delta(private->pieces, current,
							(long)(r_header.stop_plus_one - r_header.start));
			}
			else {
				ASSERT(PS_IS_AT_END(private, delta, current));
				current++;
			}
			ft_shift_up(&private->pieces, current,
					(int)(current + r_header.dp_count), (int)r_header.dp_count);
			pieces = PIECES_IN_TABLE(private);
			this_piece = &pieces[current];
			delta = r_header.stop_plus_one - current_pos;
			save_pos = adjust_pos_after_edit(save_pos, current_pos, delta);
			for (i = 0; i < r_header.dp_count; i++, this_piece++) {
				/*
				 *  BIG BUG:  sizeof(d_header) has to be divisable by
				 *  sizeof(CHAR)sizeof
				 */
				es_read(scratch, (unsigned)SIZEOF(d_header),
						(CHAR *) & d_header, &read);
				this_piece->pos = current_pos;
				this_piece->length = d_header.length;
				this_piece->source_and_pos = d_header.source_and_pos;
				current_pos += d_header.length;
			}
			ASSERT(current_pos == r_header.stop_plus_one);
			if (delta != 0) {	/* 0 iff no deleted pieces. */
				private->length += delta;
				if (notify_proc) {
					scratch_pos = es_get_position(scratch);
					notify_proc(notify_data, (Es_index)r_header.start, delta);
					(void)es_set_position(scratch, scratch_pos);
				}
			}
		}
		/*
		 *  BIG BUG:  sizeof(insert_len) has to be divisable by
		 *  sizeof(CHAR)sizeof
		 */
		es_read(scratch, (unsigned)SIZEOF(insert_len),
				(CHAR *) & insert_len, &read);
		if (insert_len > 0) {
			/*
			 * Remove the inserted text. Note that the user sequence: type-in
			 * edit-char type-in ... can cause this insert to span multiple
			 * pieces in the piece table.
			 */
			current += r_header.dp_count;
			this_piece = &pieces[current];
			for (delta = 0, i = 0; delta < insert_len;
					delta += this_piece->length, this_piece++) {
				ASSERT(this_piece->pos == current_pos + delta);
				ASSERT(this_piece->length <= insert_len - delta);
				i++;
			}
			ft_shift_out(&private->pieces, current, current + i);
			if (current < private->pieces.last_plus_one)
				ft_add_delta(private->pieces, current, -((long)insert_len));
			save_pos =
					adjust_pos_after_edit(save_pos, current_pos, -insert_len);
			private->length -= insert_len;
			if (notify_proc)
				notify_proc(notify_data, current_pos, -insert_len);
		}
		ASSUME(ps_pieces_are_consistent(private));
	}
	(void)es_set_position(scratch, ES_INFINITY);
	SET_POSITION(private, save_pos);
	private->last_write_plus_one = ES_INFINITY;
}

caddr_t
#ifdef ANSI_FUNC_PROTO
ps_get(Es_handle esh, Es_attribute attribute, ...)
#else
ps_get(esh, attribute, va_alist)
    Es_handle       esh;
    Es_attribute    attribute;
va_dcl
#endif
{
    register Piece_table private = ABS_TO_REP(esh);
    Es_index        first, last_plus_one;
    Es_handle       pieces_for_span, to_recycle;
    va_list         args;

    if ((private->magic != PS_MAGIC) && (attribute != ES_TYPE))
	return ((caddr_t) 0);
    switch (attribute) {
      case ES_CLIENT_DATA:
	return ((caddr_t) private->client_data);
      case ES_UNDO_MARK:
	private->last_write_plus_one = ES_INFINITY;
	/* +1 below is because 0 == ES_NULL_UNDO_MARK */
	return (caddr_t) ((long)(es_get_length(private->scratch) + 1));
      case ES_HANDLE_FOR_SPAN:
	VA_START(args, attribute);
#ifdef lint
	first = (args ? 0 : 0);
	last_plus_one = 0;
	to_recycle = 0;
#else
	first = va_arg(args, Es_index);
	last_plus_one = va_arg(args, Es_index);
	to_recycle = va_arg(args, Es_handle);
#endif
	pieces_for_span =
	    ps_pieces_for_span(esh, first, last_plus_one, to_recycle);
	va_end(args);
	return ((caddr_t) pieces_for_span);
      case ES_HAS_EDITS:
	return ((caddr_t) ((long)(private->oldest_not_undone_mark != ES_INFINITY)));
      case ES_PS_ORIGINAL:
	return ((caddr_t) (private->original));
      case ES_PS_SCRATCH:
	return ((caddr_t) (private->scratch));
      case ES_PS_SCRATCH_MAX_LEN:
	return (caddr_t) ((long)(private->scratch_max_len));
      case ES_STATUS:
	return ((caddr_t) (private->status));
      case ES_SIZE_OF_ENTITY:
	return (es_get(private->original, ES_SIZE_OF_ENTITY));
      case ES_TYPE:
	return ((caddr_t) ES_TYPE_PIECE);
      default:
	return (0);
    }
}

static int ps_set(Es_handle esh, Attr_attribute *attrs)
{
    register Piece_table private = ABS_TO_REP(esh);
    int (*notify_proc)(caddr_t d, Es_index, Es_index) = NULL;
    caddr_t         notify_data = 0;
    Es_index        undo_mark;
    Es_index        first;
    Es_handle       to_recycle;
    Es_status       status_dummy = ES_SUCCESS;
    register Es_status *status;

    status = &status_dummy;
    if (private->magic != PS_MAGIC)
	*status = ES_INVALID_TYPE;
    for (; *attrs && (*status == ES_SUCCESS); attrs = attr_next(attrs)) {
	switch ((Es_attribute) * attrs) {
	  case ES_CLIENT_DATA:
	    private->client_data = attrs[1];
	    break;
	  case ES_HANDLE_TO_INSERT:
	    to_recycle = (Es_handle) attrs[1];
	    if (private->scratch_max_len == ES_INFINITY) {
		ps_insert_pieces(esh, to_recycle);
	    } else {
		/* When scratch is bounded, copy contents, not pieces. */
		*status = es_copy(to_recycle, esh, FALSE);
	    }
	    break;
	  case ES_PS_ORIGINAL:
	    /*
	     * Caller should destroy the old private->original iff return
	     * value is ES_SUCCESS, allowing for caller to recover in case of
	     * errors.
	     */
	    to_recycle = (Es_handle) attrs[1];
	    first = es_get_position(private->original);
	    if (to_recycle == ES_NULL) {
		*status = ES_INVALID_HANDLE;
	    } else if (es_get_length(private->original) !=
		       es_get_length(to_recycle)) {
		*status = ES_INCONSISTENT_LENGTH;
	    } else if (first != es_set_position(to_recycle, first)) {
		*status = ES_INCONSISTENT_POS;
	    } else {
		private->original = to_recycle;
	    }
	    break;
	  case ES_PS_SCRATCH_MAX_LEN:
	    first = (Es_index) attrs[1];
	    if (first < SCRATCH_MIN_LEN ||
		first < es_get_length(private->scratch)) {
		*status = ES_INCONSISTENT_LENGTH;
	    } else if (first >= ES_INFINITY) {
		if (private->scratch_max_len != ES_INFINITY) {
		    *status = ES_INCONSISTENT_LENGTH;
		}
	    } else {
		if (private->scratch_max_len == ES_INFINITY) {
		    es_set(private->scratch, ES_CLIENT_DATA, esh, NULL);
		    private->scratch_max_len = first;
		    private->scratch_length =
			es_get_length(private->scratch);
		    private->scratch_position =
			es_get_position(private->scratch);
		    /* Modify the scratch ops vector. */
		    private->scratch_ops = private->scratch->ops;
		    private->scratch->ops =
			(Es_ops) malloc(sizeof(struct es_ops));
		    *private->scratch->ops = *private->scratch_ops;
		    private->scratch->ops->destroy = ps_scratch_destroy;
		    private->scratch->ops->get_length =
			ps_scratch_get_length;
		    private->scratch->ops->get_position =
			ps_scratch_get_position;
		    private->scratch->ops->set_position =
			ps_scratch_set_position;
		    private->scratch->ops->read = ps_scratch_read;
		    private->scratch->ops->replace = ps_scratch_replace;
		}
	    }
	    break;
	  case ES_STATUS:
	    private->status = (Es_status) attrs[1];
	    break;
	  case ES_STATUS_PTR:
	    status = (Es_status *) attrs[1];
	    *status = status_dummy;
	    break;
	  case ES_UNDO_MARK:
	    /* -1 below is because 0 == ES_NULL_UNDO_MARK */
	    undo_mark = ((Es_index) attrs[1]) - 1;
	    ps_undo_to_mark(esh, undo_mark, notify_proc, notify_data);
	    break;
	  case ES_UNDO_NOTIFY_PAIR:
	    notify_proc = (int (*)(caddr_t d, Es_index, Es_index)) attrs[1];
	    notify_data = (char *) attrs[2];
	    break;
	  default:
	    break;
	}
    }
    return ((*status == ES_SUCCESS));
}
