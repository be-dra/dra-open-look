#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)seln.c 20.19 93/06/28 DRA: $Id: seln.c,v 4.2 2025/01/26 22:08:57 dra Exp $";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

#include <sys/types.h>
#include <sys/file.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#define xview_other_selection_funcs 1
#include <xview_private/seln_impl.h>
#include <xview_private/i18n_impl.h>
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
