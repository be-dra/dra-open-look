#ifndef lint
char     ev_op_bdry_c_sccsid[] = "@(#)ev_op_bdry.c 20.18 93/06/28 DRA: $Id: ev_op_bdry.c,v 4.2 2024/09/15 16:17:30 dra Exp $";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Maintenance of fingers into an entity stream. Each finger represents a
 * boundary at which an incremental change to the rendering is recorded, e.g.
 * begin/end primary/secondary selection. Each such change may, but need not,
 * change the pixel painting operations used by the rendering. Note that a
 * particular position may occur in multiple fingers.
 */
#include <xview/pkg.h>
#include <xview/attrol.h>
#include <xview_private/primal.h>
#include <xview_private/ev_impl.h>

static Ev_finger_handle ev_insert_finger(Ev_finger_table *fingers, Es_index pos, opaque_t client_data, unsigned before, Ev_mark id_handle);
static Ev_mark_object last_generated_id;
static int ev_find_finger_internal(Ev_finger_table *fingers, Ev_mark mark);

#define FORALL(index_var)						\
	for (index_var = 0; index_var < fingers->last_plus_one; index_var++)

Pkg_private Ev_finger_handle ev_add_finger(Ev_finger_table *fingers, Es_index pos, opaque_t client_data, Ev_mark id_handle)
{
	register Es_index *seq = fingers->seq;
	register int addr_delta = fingers->sizeof_element;
	register int index;

	index = ft_bounding_index((Ft_table) fingers, pos);
	if (index == fingers->last_plus_one) {
		index = 0;
	}
	else if (index < fingers->last_plus_one) {
		index++;
	}
	if (!EV_MARK_MOVE_AT_INSERT(*id_handle)) {
		for (seq = FT_ADDR(fingers, index - 1, addr_delta); index > 0;
				index--, seq = FT_PREV_ADDR(seq, addr_delta)) {
			if (*seq != pos)
				break;
			if (!EV_MARK_MOVE_AT_INSERT(((Ev_finger_handle) seq)->info))
				break;
		}
	}
	return ev_insert_finger(fingers, pos, client_data, (unsigned)index,
											id_handle);
}

#define EXPAND_BY 30
static Ev_finger_handle ev_insert_finger(Ev_finger_table *fingers, Es_index pos, opaque_t client_data, unsigned before, Ev_mark id_handle)
/* insert before this index: */
{
	register Ev_finger_handle finger;
	register int expand_by = EXPAND_BY;

	/* If the table has gotten large, expand faster */
	if (fingers->last_plus_one > 99) {
		expand_by = fingers->last_plus_one >> 1;
	}
	(void)ft_shift_up(fingers, (int)before, (int)before + 1, expand_by);
	finger = (Ev_finger_handle) FT_ADDRESS(fingers, before);
	finger->pos = pos;
	finger->client_data = client_data;
	if (id_handle == 0) {
		last_generated_id++;
		finger->info = last_generated_id;
	}
	else if (EV_MARK_IS_NULL(id_handle)) {
		last_generated_id++;
		*id_handle = EV_MARK_MOVE_AT_INSERT(*id_handle) |
				EV_MARK_ID(last_generated_id);
		finger->info = *id_handle;
	}
	else
		finger->info = *id_handle;
	return (finger);
}

Ev_finger_handle ev_find_finger(Ev_finger_table *fingers, Ev_mark_object  mark)
{
    register int    i = ev_find_finger_internal(fingers, &mark);

    return ((i < fingers->last_plus_one)
	    ? (Ev_finger_handle) FT_ADDRESS(fingers, i)
	    : (Ev_finger_handle) 0);
}

static int ev_find_finger_internal(Ev_finger_table *fingers, Ev_mark mark)
{
    register Ev_finger_handle finger =
    (Ev_finger_handle) fingers->seq;
    register int    i, mark_id;

    if ((mark_id = EV_MARK_ID(*mark)) != 0) {
	if (EV_MARK_ID(finger->info) == mark_id) {
	    return (0);
	}
	i = ft_bounding_index(fingers, ES_INFINITY - 1);
	if (i != fingers->last_plus_one) {
	    finger = (Ev_finger_handle) FT_ADDRESS(fingers, i);
	    if (EV_MARK_ID(finger->info) == mark_id) {
		return (i);
	    } {
		finger = (Ev_finger_handle) fingers->seq;
	    }
	}
	for (i = 1; i < fingers->last_plus_one; i++) {
	    finger = (Ev_finger_handle) FT_NEXT_ADDRESS(fingers, finger);
	    if (EV_MARK_ID(finger->info) == mark_id)
		return (i);
	}
    }
    return (fingers->last_plus_one);
}

static void ev_remove_finger_internal(Ev_finger_table *fingers, int i)
{
    if (i < fingers->last_plus_one) {
	(void) ft_shift_out(fingers, i, i + 1);
    } else
	LINT_IGNORE(ASSUME(0));	/* There should be an entry */
}

Pkg_private void ev_remove_finger(Ev_finger_table *fingers, Ev_mark_object  mark)
{
    int             i = ev_find_finger_internal(fingers, &mark);

    ev_remove_finger_internal(fingers, i);
}

#ifdef SOLL_DAS_ZUSAMMENPASSEN
struct ev_finger_datum {
	Es_index		pos;
	Ev_mark_object		info;
	opaque_t		client_data;
};
typedef struct ev_finger_datum *Ev_finger_handle;

typedef struct op_bdry_datum *	Op_bdry_handle;
struct op_bdry_datum {
	Es_index		pos;
	Ev_mark_object		info;
	unsigned		flags;
	opaque_t		more_info;
};
#endif /* SOLL_DAS_ZUSAMMENPASSEN */

Pkg_private Op_bdry_handle ev_add_op_bdry(Ev_finger_table *op_bdry,
			Es_index pos, unsigned type, Ev_mark mark)
{
    Op_bdry_handle  bdry;

    bdry = (Op_bdry_handle)ev_add_finger(op_bdry, pos, (opaque_t)((long)type), mark);

	/* siehe SOLL_DAS_ZUSAMMENPASSEN: wenn da ein ev_finger_datum * rauskommt,
	 * halte ich es fuer gefaehrlich, da in op_bdry_datum->more_info
	 * etwas reinzuschreiben
	 */
/*  Diese 'dangerous assignment'-Ausgabe kam auch einige Male....
	fprintf(stderr, "%s`%s: dangerous assignment!\n", __FILE__, __FUNCTION__);
	Jetzt habe ich beschlossen, in ev_finger_datum ein zusaetzliches Feld
	'very_careful' einzubauen
*/
    bdry->more_info = (opaque_t) 0;
    return (bdry);
}

Pkg_private	Op_bdry_handle ev_find_op_bdry(Ev_finger_table op_bdry, Ev_mark_object  mark)
{
    return ((Op_bdry_handle) ev_find_finger(&op_bdry, mark));
}

/* Caller must ensure that i is the first of the entries being merged. */
Pkg_private	unsigned ev_op_bdry_info_merge(Ev_finger_table op_bdry, int    i, int            *next_i, unsigned        prior)
{
    register Op_bdry_handle seq = (Op_bdry_handle) op_bdry.seq;
    int             pos = seq[i].pos;
    register unsigned result = prior;

    while (i < op_bdry.last_plus_one) {
	if (seq[i].flags & EV_BDRY_END) {
	    ASSUME(result & seq[i].flags);
	    result &= ~seq[i].flags;
	} else {
	    ASSUME(!(result & seq[i].flags));
	    result |= seq[i].flags;
	}
	if (pos != seq[++i].pos)
	    break;
    }
    if (next_i)
	*next_i = i;
    return (result);
}

Pkg_private	unsigned ev_op_bdry_info( Ev_finger_table op_bdry, Es_index pos, int *next_i)
{
    register Op_bdry_handle seq = (Op_bdry_handle) op_bdry.seq;
    int             i = 0;
    register unsigned result = 0;

    while ((i < op_bdry.last_plus_one) && (seq[i].pos <= pos)) {
	result = ev_op_bdry_info_merge(op_bdry, i, &i, result);
    }
    if (next_i)
	*next_i = i;
    return (result);
}

Pkg_private	void ev_remove_op_bdry(Ev_finger_table *op_bdry, Es_index pos, unsigned        type, unsigned mask)
{
    register Op_bdry_handle seq = (Op_bdry_handle) op_bdry->seq;
    register int    i, last_plus_one = op_bdry->last_plus_one;
    register unsigned masked_type = type & mask;

    i = ft_index_for_position(*op_bdry, pos);
    if (i == last_plus_one)
	goto Return;
    while (i < last_plus_one && seq[i].pos == pos) {
	if (masked_type == (seq[i].flags & mask)) {
	    ev_remove_finger_internal(op_bdry, i);
	    goto Return;
	}
	i++;
    }
    LINT_IGNORE(ASSUME(0));	/* There should be an entry */
Return:
    return;
}

#ifdef OW_I18N
Pkg_private	void
ev_remove_all_op_bdry(chain, start, end, type, mask)
    Ev_chain        	    chain;
    register Es_index start, end;
    unsigned        type;
    register unsigned mask;
{
    register Ev_chain_pd_handle
                    	    private = EV_CHAIN_PRIVATE(chain);

    register Op_bdry_handle seq = (Op_bdry_handle) private->op_bdry.seq;
    register int    i, last_plus_one = private->op_bdry.last_plus_one;
    register unsigned masked_type = type & mask;
    Es_index	pos;
    
    for (pos = start; pos <= end; pos++) {
	i = ft_index_for_position(private->op_bdry, pos);
        if (i != last_plus_one) {
            while ((i < last_plus_one) && (seq[i].pos == pos)) {
	        if (masked_type & seq[i].flags & mask) {
	            ev_remove_finger_internal(&private->op_bdry, i);
	        } else
	            i++;  /* Remove will shift the array, so don't increament */
            }
        }
    }
}
#endif

Pkg_private Ev_mark_object ev_add_glyph(Ev_chain chain, Es_index line_start, Es_index pos, Pixrect *pr, int op, int offset_x, int offset_y, int flags)
{

	Ev_chain_pd_handle private = EV_CHAIN_PRIVATE(chain);
	register Op_bdry_handle bdry;
	register Ev_overlay_handle glyph_info;
	Ev_mark_object result;

	EV_INIT_MARK(result);
	if ((pos < 0) || (pr == 0) || (offset_y != 0)
			|| ((offset_x >= 0) && !(flags & EV_GLYPH_LINE_END))
			|| ((offset_x < 0) && (flags & EV_GLYPH_LINE_END))) {
		LINT_IGNORE(ASSUME(0));	/* Let implementor look at this. */
		return (result);
	}
	bdry = ev_add_op_bdry(&private->op_bdry, line_start,
			EV_BDRY_OVERLAY, &result);
	glyph_info = NEW(Ev_overlay_object);
	bdry->more_info = (opaque_t) glyph_info;
	glyph_info->pr = pr;
	glyph_info->pix_op = op;
	glyph_info->offset_x = offset_x;
	glyph_info->offset_y = offset_y;
	if (flags & EV_GLYPH_LINE_END)
		glyph_info->flags |= EV_OVERLAY_RIGHT_MARGIN;
	EV_INIT_MARK(result);	/* Get the next generated id */
	if (pos == line_start) {
		/*
		 * Force pos > line_start so that ev_display_internal will have an
		 * EI_OP_EV_OVERLAY show up in its range.info.
		 */
		struct ei_process_result ei_process_result;

		pos++;
		ei_process_result = ev_ei_process(chain->first_view, line_start, pos);
		glyph_info->offset_x -= ei_process_result.bounds.r_width;
	}
	bdry = ev_add_op_bdry(&private->op_bdry, pos,
			EV_BDRY_OVERLAY | EV_BDRY_END, &result);
	bdry->more_info = (opaque_t) glyph_info;
	/*
	 * What is painted depends on whether glyph_count is positive, so
	 * increment it before painting.
	 */
	private->glyph_count++;
	if (flags & EV_GLYPH_DISPLAY) {
		(void)ev_display_range(chain, line_start, pos);
	}
	return (result);
}

Pkg_private Ev_mark_object ev_add_glyph_on_line(Ev_chain chain, int line, Pixrect *pr, int op, int offset_x, int offset_y, int flags)
{
    register Es_index line_start;
    Es_index        first, pos;
    Ev_mark_object  result;

    EV_INIT_MARK(result);
    if (line < 0) {
	LINT_IGNORE(ASSUME(0));	/* Let implementor look at this. */
	return (result);
    }
    line_start = ev_position_for_physical_line(chain, line, 0);
    if (line_start == ES_CANNOT_SET) {
	LINT_IGNORE(ASSUME(0));	/* Let implementor look at this. */
	return (result);
    }
    if ((flags & (EV_GLYPH_LINE_START | EV_GLYPH_LINE_END))
	&& !(flags & EV_GLYPH_WORD_START)) {
	pos = line_start;
    } else {
	ev_span(chain, line_start, &first, &pos,
	   EI_SPAN_SP_AND_TAB | EI_SPAN_RIGHT_ONLY | EI_SPAN_IN_CLASS_ONLY);
	if (first == ES_CANNOT_SET)
	    pos = line_start;
    }

    result = ev_add_glyph(chain, line_start, pos, pr, op,
			  offset_x, offset_y, flags);
    return (result);
}

Pkg_private Op_bdry_handle ev_find_glyph(Ev_chain chain, Es_index line_start)
{
	Ev_chain_pd_handle private = EV_CHAIN_PRIVATE(chain);
	Op_bdry_handle bdry = NULL;
	Op_bdry_handle seq = (Op_bdry_handle) private->op_bdry.seq;
	int i, last_plus_one;

	i = ft_index_for_position(private->op_bdry, line_start);
	last_plus_one = private->op_bdry.last_plus_one;
	if (i == last_plus_one)
		goto Return;
	while (i < last_plus_one && seq[i].pos == line_start) {
		if (seq[i].flags & EV_BDRY_OVERLAY && !(seq[i].flags & EV_BDRY_END)) {
			bdry = &seq[i];
			break;
		}
		i++;
	}
	for (i++; i < last_plus_one; i++) {
		if (((seq[i].flags & (EV_BDRY_OVERLAY | EV_BDRY_END)) ==
						(EV_BDRY_OVERLAY | EV_BDRY_END)) &&
				(seq[i].more_info == bdry->more_info)) {
			return (&seq[i]);
		}
	}
  Return:
	LINT_IGNORE(ASSUME(0));	/* There should be an entry */
	return (0);
}

static void ev_clear_margins(register Ev_handle view, register Es_index pos, register int lt_index, register Rect *rect)
/*
 * If rect is not NULL, believe it, else if lt_index is in range, construct
 * the rect from it, else construct the rect for the line containing pos.
 */
{
    Rect            dummy_rect;

    if (rect == 0) {
	rect = &dummy_rect;
	if (lt_index < 0 || lt_index >= view->line_table.last_plus_one) {
	    int             dummy_lt_index;
	    switch (ev_xy_in_view(view, pos, &dummy_lt_index, rect)) {
	      case EV_XY_VISIBLE:
		break;
	      default:
		return;
	    }
	    lt_index = dummy_lt_index;
	} else
	    *rect = ev_rect_for_line(view, lt_index);
    }
    ev_clear_from_margins(view, rect, (Rect *) 0);
}

Pkg_private void ev_remove_glyph(Ev_chain chain, Ev_mark_object mark, unsigned flags)
{
    register Ev_handle view;
    register        Ev_chain_pd_handle
                    private = EV_CHAIN_PRIVATE(chain);
    register int    i;
    Op_bdry_handle  bdry;
    Es_index        line_start, pos;

    i = ev_find_finger_internal(&private->op_bdry, &mark);
    if AN_ERROR
	((i == 0) || (i >= private->op_bdry.last_plus_one))
	    return;
    bdry = (Op_bdry_handle) FT_ADDRESS(&private->op_bdry, i);
    pos = bdry->pos;
    ev_remove_finger_internal(&private->op_bdry, i);
    /*
     * Glyph fingers come in pairs with consecutive ids. The mark argument
     * identifies the second of the pair. Both fingers must be removed.
     */
    mark--;
    /*
     * Usually first glyph finger is just above second, so try to avoid the
     * fully general lookup.
     */
    bdry--;
    i--;
    if (!(((sizeof(Op_bdry_handle) % sizeof(long)) == 0) &&
	  (EV_MARK_ID(bdry->info) == EV_MARK_ID(mark)))) {
	i = ev_find_finger_internal(&private->op_bdry, &mark);
	if AN_ERROR
	    (i >= private->op_bdry.last_plus_one)
		return;
	bdry = (Op_bdry_handle) FT_ADDRESS(&private->op_bdry, i);
    }
    line_start = bdry->pos;
    free(bdry->more_info);
    ev_remove_finger_internal(&private->op_bdry, i);
    /*
     * What is painted depends on whether glyph_count is positive, so
     * decrement it after painting.
     */
    if (flags) {
	/* Clear margin before paint in case glyph extended into it. */
	FORALLVIEWS(chain, view) {
	    ev_clear_margins(view, line_start, -1, (Rect *) 0);
	}
	ev_display_range(chain, line_start, pos);
    }
    private->glyph_count--;
}

Pkg_private void ev_set_glyph_pr(Ev_chain chain, Ev_mark_object mark, struct pixrect *pr)
{
    register Ev_handle view;
    register        Ev_chain_pd_handle
                    private = EV_CHAIN_PRIVATE(chain);
    register int    i;
    register Op_bdry_handle bdry;
    Ev_overlay_handle glyph_info;
    Es_index        line_start, pos, stop_pos;

    i = ev_find_finger_internal(&private->op_bdry, &mark);
    if AN_ERROR
	((i == 0) || (i >= private->op_bdry.last_plus_one))
	    return;
    bdry = (Op_bdry_handle) FT_ADDRESS(&private->op_bdry, i);
    glyph_info = (Ev_overlay_handle) bdry->more_info;
    glyph_info->pr = pr;
    stop_pos = bdry->pos;
    /*
     * Glyph fingers come in pairs with consecutive ids. The mark argument
     * identifies the second of the pair. The position to start repaint from
     * is in first finger.
     */
    mark--;
    /*
     * Usually first glyph finger is just above second, so try to avoid the
     * fully general lookup.
     */
    bdry--;
    if (!(((sizeof(Op_bdry_handle) % sizeof(long)) == 0) &&
	  (EV_MARK_ID(bdry->info) == EV_MARK_ID(mark)))) {
	i = ev_find_finger_internal(&private->op_bdry, &mark);
	if AN_ERROR
	    (i >= private->op_bdry.last_plus_one)
		return;
	bdry = (Op_bdry_handle) FT_ADDRESS(&private->op_bdry, i);
    }
    pos = bdry->pos;
    line_start = ev_line_start(chain->first_view, pos);

    /* Clear margin before paint in case glyph extended into it. */
    FORALLVIEWS(chain, view) {
	ev_clear_margins(view, line_start, -1, (Rect *) 0);
    }
    (void) ev_display_range(chain, line_start, stop_pos);
}
