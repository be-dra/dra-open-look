#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)xv_list.c 20.13 93/06/28  DRA: $Id: xv_list.c,v 4.1 2024/03/28 19:35:11 dra Exp $";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

#include <xview_private/xv_list.h>

Xv_private void xv_sl_init(Xv_sl_head head)
{
    head->next = XV_SL_NULL;
}

Xv_private Xv_sl_link xv_sl_add_after(Xv_sl_head head, Xv_sl_link link, Xv_sl_link newydd)
{
    if (link != XV_SL_NULL) {
	newydd->next = link->next;
	link->next = newydd;
    } else {
	newydd->next = head;
    }
    return (newydd);
}

Xv_private Xv_sl_link xv_sl_remove_after(Xv_sl_head head, Xv_sl_link link)
{
    register Xv_sl_link result;

    if (link != XV_SL_NULL) {
	result = link->next;
	link->next = result->next;
    } else {
	result = head;
    }
    return (result);
}

Xv_private Xv_sl_link xv_sl_remove(Xv_sl_head head, Xv_sl_link link)
{
    register Xv_sl_link prev;

    if ((head == link) || (link == XV_SL_NULL)) {
	prev = XV_SL_NULL;
    } else {
	XV_SL_FOR_ALL(head, prev) {
	    if (prev->next == link)
		break;
	}
#ifdef _XV_DEBUG
	abort();
#endif
    }
    return (xv_sl_remove_after(head, prev));
}
