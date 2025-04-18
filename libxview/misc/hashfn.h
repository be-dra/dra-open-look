/*	@(#)hashfn.h 20.10 93/06/28 SMI	DRA: RCS: $Id: hashfn.h,v 4.2 2024/09/15 09:03:54 dra Exp $ */

/*
 *    hashfn.h -- external declarations
 */

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */

#ifndef hashfn_h
#define hashfn_h

#include <sys/types.h>

typedef struct he_ HashEntry;

struct he_ {
    HashEntry    *he_next;
    HashEntry    *he_prev;
    caddr_t        he_key;
    caddr_t        he_payload;
};

typedef struct ht_ HashTable;

struct ht_ {
    int            ht_size;

    /* hash func: int f(caddr_t) */
    int            (*ht_hash_fn)(caddr_t);

    /* compare func: int f(caddr_t, caddr_t) returns 0 for equal */
    int            (*ht_cmp_fn)(caddr_t, caddr_t);
    HashEntry    **ht_table;
};

#ifndef hashfn_c

extern HashTable *hashfn_new_table(int size, int (*hash_fn)(caddr_t),
						int (*cmp_fn)(caddr_t, caddr_t));
extern void hashfn_dispose_table(HashTable *h);

extern caddr_t /* payload pointer */ hashfn_lookup(HashTable *h, caddr_t key);
extern caddr_t /* payload pointer */ hashfn_install(HashTable *h, caddr_t key, caddr_t payload);
extern caddr_t /* payload pointer */ hashfn_delete(HashTable *h, caddr_t key);

extern caddr_t /* key pointer */ hashfn_first_key(HashTable *h, caddr_t *payload);
extern caddr_t /* key pointer */ hashfn_next_key(HashTable *h, caddr_t *payload);

#endif  /* hashfn_c */
#endif  /* hashfn_h */
