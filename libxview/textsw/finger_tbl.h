/*      @(#)finger_tbl.h 20.12 93/06/28 SMI  DRA: $Id: finger_tbl.h,v 4.1 2024/03/28 19:06:00 dra Exp $      */

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

#ifndef finger_table_DEFINED
#define finger_table_DEFINED

/*
 * This is the programmatic interface to the finger table abstraction.
 *
 *     A finger_table is an array of records: .seq[ 0 .. .last_plus_one-1 ].
 *     Each record has an Es_index as its first field, and is
 * .size_of_element bytes long.
 *     The array is ordered by the values of the Es_index field, with the
 * smallest value occurring in the first record of the array.  The
 * implementation supports and requires the ordering.
 *     .last_bounding_index records the last result from ft_bounding_index;
 * it is a cache private to ft_bounding_index used to speed up the common case
 * of a succession of searches for similar positions.
 *     .first_infinity records the first occurrence of an ES_INFINITY;
 * it is a cache private to ft_shift_{up,out} used to speed up a common case
 * of shifts in large tables with many ES_INFINITY's at the end.
 */

#					ifndef suntool_entity_stream_DEFINED
#include <xview_private/es.h>
#					endif

typedef struct ft_table {
	int	 	last_plus_one;
	unsigned	sizeof_element;
	int	 	last_bounding_index;
	int	 	first_infinity;
	Es_index	*seq;
} Ft_table_object;
typedef Ft_table_object	*Ft_table;
/* The following two typedef's are for 3.0 compatibility. */
typedef struct ft_table		ft_object;
typedef struct ft_table *	ft_handle;
#define FT_NULL			((ft_handle)0)

#define FT_CLIENT_CREATE(num_elements, client_type)			\
	ft_create((num_elements), (int)(sizeof(client_type)-sizeof(Es_index)))
#define FT_SET_ALL(finger_table, to, client_data)			\
	ft_set((finger_table), 0, (finger_table).last_plus_one,		\
		(to), (client_data))
#define FT_CLEAR_ALL(finger_table)					\
	FT_SET_ALL((finger_table), ES_INFINITY, (char *)0)

#ifdef lint
#define	FT_ADDR(_handle, _index, _addr_delta)				\
	(Es_index *)( (_handle) && (_index) && (_addr_delta) ? 0 : 0 )
#define	FT_NEXT_ADDR(_addr, _addr_delta)				\
	(Es_index *)( (_addr) && (_addr_delta) ? 0 : 0 )
#define	FT_PREV_ADDR(_addr, _addr_delta)				\
	(Es_index *)( (_addr) && (_addr_delta) ? 0 : 0 )
#else
#define	FT_ADDR(_handle, _index, _addr_delta)				\
	(Es_index *)(							\
	    ((char *)((_handle)->seq)) + ((_index)*(_addr_delta)) )
#define	FT_NEXT_ADDR(_addr, _addr_delta)				\
	(Es_index *)( (char *)(_addr) + (_addr_delta) )
#define	FT_PREV_ADDR(_addr, _addr_delta)				\
	(Es_index *)( (char *)(_addr) - (_addr_delta) )
#endif

#define	FT_ADDRESS(_handle, _index)					\
	FT_ADDR((_handle), (_index), (_handle)->sizeof_element)
#define	FT_NEXT_ADDRESS(_handle, _addr)					\
	FT_NEXT_ADDR((_addr), (_handle)->sizeof_element )
#define	FT_PREV_ADDRESS(_handle, _addr)					\
	FT_PREV_ADDR((_addr), (_handle)->sizeof_element )

#endif



Pkg_private ft_object ft_create(int last_plus_one, int sizeof_client_data);

/* Allocates a seq of last_plus_one elements, each occupying
 *   sizeof(Es_index)+sizeof_client_data bytes, then returns an ft_object
 *   correctly initialized.
 * NOTE: The individual sequence elements are not initialized.
 */

Pkg_private void ft_destroy(ft_handle table);

/* Deallocates the sequence and adjusts *table to remove all references
 *   to the deallocated sequence.
 */

Pkg_private void ft_expand(ft_handle table, int by);

/* Replaces the old sequence with a new sequence that is longer (by > 0) or
 *   shorter (if by < 0), while preserving all appropriate sequence contents.
 * When the sequence increases in size, new elements are at the end.
 * NOTE: If by > 0, the new sequence elements are not initialized.
 */

Pkg_private void ft_shift_up(ft_handle table, int first, int last_plus_one, int expand_by);

/* Shifts the elements in the sequence up so that seq[i],
 *   first <= i < last_plus_one, is unused.
 * The shift treats an element with position ES_INFINITY as the beginning of
 *   a range of empty elements, and stops the shift there.
 * If no empty elements can be found, the table is first expanded by expand_by
 *   and the extra elements have their position set to ES_INFINITY.
 */

Pkg_private void ft_shift_out(ft_handle table, int first, int last_plus_one);

/* Shifts the elements in the sequence down so that seq[first+i] gets the
 *   value from seq[last_plus_one+i].
 * The shift treats an element with position ES_INFINITY as an empty element,
 *   and stops the shift there.
 * Elements that are invalidated at the end of the sequence have their
 *   position set to ES_INFINITY.
 */

Pkg_private void ft_set(ft_object finger_table, int first, int last_plus_one,
    Es_index to, void *client_data);

/* All sequence elements in [first..last_plus_one) have their positions
 *   set to 'to', and if client_data is not 0, their client data area set
 *   to *client_data.
 */

Pkg_private void ft_set_esi_span( ft_object finger_table,
    Es_index first, Es_index last_plus_one, Es_index to,
    char           *client_data);

/* Like ft_set, but sets all elements whose positions are in the range
 *   [first..last_plus_one).
 */

Pkg_private int ft_bounding_index(Ft_table finger_table, Es_index pos);

/* Returns an index for finger_table that is either such that:
 *   finger_table->seq[index] <= pos < finger_table->seq[index+1], with
 *      index+1 < finger_table->last_plus_one, or;
 *   finger_table->seq[index] <= pos, with
 *      index+1 == finger_table->last_plus_one, or;
 *   finger_table->last_plus_one.
 * Note: if there are multiple occurrences of pos in the seq array, the
 *   returned index will be the index of the last occurrence.
 */

Pkg_private int ft_index_for_position(ft_object finger_table, Es_index pos);

/* Returns an index for finger_table that is either:
 *   the smallest index such that .seq[index] = pos, with
 *      index < finger_table.last_plus_one, or;
 *   finger_table.last_plus_one
 */

Pkg_private Es_index ft_position_for_index(ft_object finger_table, int index);
/* Returns the position value of the sequence element specified by index,
 *   unless index is too large, in which case ES_CANNOT_SET is returned.
 * This is most useful when you do not want to typecast the sequence.
 */

Pkg_private void ft_add_delta(ft_object finger_table, int from, long int delta);

/* Beginning at finger_table.seq[from], adds delta to each element until
 *   either the end of the table is reached or a position of ES_INFINITY.
 */
