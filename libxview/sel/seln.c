#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)seln.c 20.19 93/06/28 DRA: $Id: seln.c,v 4.3 2025/02/02 19:10:47 dra Exp $";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

#define xview_other_selection_funcs 1
#include <xview_private/seln_impl.h>
#include <xview/selection.h>

struct selection selnull;

void selection_set(struct selection *sel,
				int (*sel_write)(struct selection *, FILE *),
				int (*sel_clear)(void),
				Xv_object window)
/* Note: sel_clear not used yet */
{
	selection_unsupported(__FUNCTION__);
}

void selection_get(int (*sel_read) (struct selection *, FILE *), Xv_object window)
{
	selection_unsupported(__FUNCTION__);
}

void selection_clear(Xv_object window)
{
	selection_unsupported(__FUNCTION__);
}
