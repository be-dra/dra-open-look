#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)bitmask.c 20.15 93/06/28 DRA: RCS $Id: bitmask.c,v 4.1 2024/03/28 18:05:27 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Bitmask handling
 */

#define bitmask_c
#include <xview_private/bitmask.h>
#include <xview/base.h>

Bitmask *xv_bitss_new_mask(int max_bits)
{
    Bitmask        *m;
    int             n;

    m = (Bitmask *) xv_malloc(sizeof(Bitmask));
    m->bm_max_bits = max_bits;

    /* Compute number of bytes needed */

    n = (max_bits + BITSPERBYTE - 1) / BITSPERBYTE;

    /* Compute number of ints needed */

    m->bm_mask_size = (n + sizeof(unsigned int) - 1) / sizeof(unsigned int);

    m->bm_mask = (unsigned int *) xv_malloc(m->bm_mask_size * sizeof(unsigned int));
    for (n = 0; n < m->bm_mask_size; n++)
	m->bm_mask[n] = 0;

    return m;
}

Bitmask * xv_bitss_copy_mask(Bitmask *m)
{
    Bitmask        *p;
    register int    i;

    p = (Bitmask *) xv_malloc(sizeof(Bitmask));
    p->bm_max_bits = m->bm_max_bits;
    p->bm_mask_size = m->bm_mask_size;
    p->bm_mask = (unsigned int *) xv_malloc(p->bm_mask_size * sizeof(unsigned int));
    for (i = 0; i < p->bm_mask_size; i++)
	p->bm_mask[i] = m->bm_mask[i];

    return p;
}

void xv_bitss_dispose_mask(Bitmask *m)
{
    free(m->bm_mask);
    free(m);
}

Bitmask * xv_bitss_unset_mask(register Bitmask *m, register int bit)
{
    register int    n, rbit;

    if (bit >= m->bm_max_bits)
	return (Bitmask *) 0;

    n = bit / (sizeof(unsigned int) * BITSPERBYTE);
    rbit = bit - n * (sizeof(unsigned int) * BITSPERBYTE);

    m->bm_mask[n] &= ~((unsigned) 1 << rbit);

    return m;
}

Bitmask * xv_bitss_set_mask(Bitmask *m, int bit)
{
    register int    n, rbit;

    if (bit >= m->bm_max_bits)
	return (Bitmask *) 0;

    n = bit / (sizeof(unsigned int) * BITSPERBYTE);
    rbit = bit - n * (sizeof(unsigned int) * BITSPERBYTE);

    m->bm_mask[n] |= (unsigned) 1 << rbit;

    return m;
}

unsigned int xv_bitss_get_mask(register Bitmask *m, register int bit)
{
    register int    n, rbit;

    if (bit >= m->bm_max_bits)
	return 0;

    n = bit / (sizeof(unsigned int) * BITSPERBYTE);
    rbit = bit - n * (sizeof(unsigned int) * BITSPERBYTE);

    return (m->bm_mask[n] & ((unsigned) 1 << rbit));
}

int xv_bitss_cmp_mask(Bitmask *m1, Bitmask *m2)
{
    register int    i;

    if (m1->bm_mask_size != m2->bm_mask_size)
	return m1->bm_mask_size - m2->bm_mask_size;

    for (i = 0; i < m1->bm_mask_size; i++)
	if (m1->bm_mask[i] != m2->bm_mask[i])
	    return -1;

    return 0;
}


Bitmask * xv_bitss_and_mask(Bitmask *m1, Bitmask *m2, Bitmask *m3)
{
    int             max_bits;
    int             max_words;
    register int    i;

    if (!m1 || !m2)
	return (Bitmask *) 0;

    max_bits = (m1->bm_max_bits > m2->bm_max_bits) ?
	m1->bm_max_bits : m2->bm_max_bits;

    max_words = (m1->bm_mask_size > m2->bm_mask_size) ?
	m1->bm_mask_size : m2->bm_mask_size;

    if (!m3)
	/* if third arg is null mask then create a new mask */
	m3 = xv_bitss_new_mask(max_bits);
    else if (m3->bm_mask_size < max_words)
	/* if third arg not null and not big enough then give up */
	return (Bitmask *) 0;

    m3->bm_max_bits = max_bits;

    for (i = 0; i < max_words; i++)
	m3->bm_mask[i] = m1->bm_mask[i] & m2->bm_mask[i];

    return m3;
}

Bitmask *xv_bitss_or_mask(Bitmask *m1, Bitmask *m2, Bitmask *m3)
{
    int             max_bits;
    int             max_words;
    register int    i;

    if (!m1 || !m2)
	return (Bitmask *) 0;

    max_bits = (m1->bm_max_bits > m2->bm_max_bits) ?
	m1->bm_max_bits : m2->bm_max_bits;

    max_words = (m1->bm_mask_size > m2->bm_mask_size) ?
	m1->bm_mask_size : m2->bm_mask_size;

    if (!m3)
	/* if third arg is null mask then create a new mask */
	m3 = xv_bitss_new_mask(max_bits);
    else if (m3->bm_mask_size < max_words)
	/* if third arg not null and not big enough then give up */
	return (Bitmask *) 0;

    m3->bm_max_bits = max_bits;

    for (i = 0; i < max_words; i++)
	m3->bm_mask[i] = m1->bm_mask[i] | m2->bm_mask[i];

    return m3;
}


Bitmask *xv_bitss_not_mask(Bitmask *m1, Bitmask *m2)
{
    register int    i;

    if (!m1)
	return (Bitmask *) 0;

    if (!m2)
	/* if second arg is null mask then create a new mask */
	m2 = xv_bitss_new_mask(m1->bm_max_bits);
    else if (m1->bm_mask_size > m2->bm_mask_size)
	/* if second arg is not null and not big enough then give up */
	return (Bitmask *) 0;

    for (i = 0; i < m1->bm_mask_size; i++)
	m2->bm_mask[i] = ~m1->bm_mask[i];

    return m2;
}
