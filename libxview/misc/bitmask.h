/*	@(#)bitmask.h 20.12 93/06/28 SMI	DRA: RCS: $Id: bitmask.h,v 4.1 2024/03/28 18:05:27 dra Exp $ */

/*
 *  Bitmask handling declarations
 */

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */

#ifndef bitmask_h
#define bitmask_h

typedef struct bm_ {
    unsigned int *bm_mask;
    int bm_max_bits;
    int bm_mask_size;
} Bitmask;

#ifndef BITSPERBYTE
#define BITSPERBYTE 8
#endif

extern Bitmask *xv_bitss_new_mask(int max_bits);
extern void xv_bitss_dispose_mask(Bitmask *m);
extern Bitmask * xv_bitss_set_mask(Bitmask *m, int bit);
extern unsigned int xv_bitss_get_mask(register Bitmask *m, register int bit);
extern int xv_bitss_cmp_mask(Bitmask *m1, Bitmask *m2);
extern Bitmask * xv_bitss_copy_mask(Bitmask *m);
extern Bitmask * xv_bitss_unset_mask(register Bitmask *m, register int bit);
extern Bitmask * xv_bitss_and_mask(Bitmask *m1, Bitmask *m2, Bitmask *m3);
extern Bitmask *xv_bitss_or_mask(Bitmask *m1, Bitmask *m2, Bitmask *m3);
extern Bitmask *xv_bitss_not_mask(Bitmask *m1, Bitmask *m2);

#endif  /* bitmask_h */
