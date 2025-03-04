#ifndef	lint
#ifdef sccs
static char     sccsid[] = "@(#)ntfy_node.c 20.16 93/06/28 Copyr 1985 Sun Micro  DRA: $Id: ntfy_node.c,v 4.3 2024/11/02 21:32:55 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Ntfy_node.c - Storage management for the notifier.
 */

#include <xview_private/ntfy.h>

pkg_private_data int ntfy_nodes_avail = 0;	/* Count of nodes in
						 * ntfy_node_free */
pkg_private_data int ntfy_node_blocks = 0;	/* Count of trips to heap for
						 * nodes (used for statistics
						 * & possibly run away
						 * process detection) */

static NTFY_NODE *ntfy_node_free;	/* List of free nodes */

/*
 * Caller must initialize data returned from ntfy_alloc_node. NTFY_NODE_NULL
 * is possible.
 */
pkg_private NTFY_NODE *ntfy_alloc_node(void)
{
    NTFY_NODE      *node;

    if (ntfy_node_free == NTFY_NODE_NULL) {
	if (NTFY_IN_INTERRUPT)
	    return (NTFY_NODE_NULL);
	else
	    ntfy_replenish_nodes();
    }
    NTFY_BEGIN_CRITICAL;	/* Protect node pool */
    if (ntfy_node_free == NTFY_NODE_NULL) {
	ntfy_set_errno(NOTIFY_NOMEM);
	NTFY_END_CRITICAL;
	return (NTFY_NODE_NULL);
    }
    ntfy_assert(ntfy_nodes_avail > 0, 33 /* Node count wrong */);
    node = ntfy_node_free;
    ntfy_node_free = ntfy_node_free->n.next;
    ntfy_nodes_avail--;
    NTFY_END_CRITICAL;
    return (node);
}

pkg_private void ntfy_replenish_nodes(void)
{
	register NTFY_NODE *new_nodes, *node;

	ntfy_assert((!NTFY_IN_INTERRUPT || NTFY_DEAF_INTERRUPT), 34
			/* Interrupt access to heap */ );
	ntfy_assert(ntfy_nodes_avail <= NTFY_PRE_ALLOCED, 35
			/* Unnecessary node replenishment */ );
	new_nodes = (NTFY_NODE *) xv_calloc(1,
						(unsigned)(NTFY_NODES_PER_BLOCK * sizeof(NTFY_NODE)));
	for (node = new_nodes; node < new_nodes + NTFY_NODES_PER_BLOCK; node++) {
		ntfy_free_node(node);
	}
	ntfy_node_blocks++;
}

pkg_private void ntfy_free_node(NTFY_NODE *node)
{
    NTFY_BEGIN_CRITICAL;	/* Protect node pool */
    node->n.next = ntfy_node_free;
    ntfy_node_free = node;
    ntfy_nodes_avail++;
    NTFY_END_CRITICAL;
}
