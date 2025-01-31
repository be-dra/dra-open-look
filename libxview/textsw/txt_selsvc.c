#ifndef lint
char     txt_selsvc_c_sccsid[] = "@(#)txt_selsvc.c 20.64 93/06/29 DRA: $Id: txt_selsvc.c,v 4.25 2025/01/30 09:11:25 dra Exp $";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Textsw interface to selection service.
 */

#include <errno.h>
#include <unistd.h>
#include <xview_private/portable.h>
#include <xview_private/primal.h>
#include <xview_private/txt_impl.h>
#include <xview_private/ev_impl.h>
#include <xview_private/txt_18impl.h>
/* #include <xview_private/seln_impl.h> */
#include <xview_private/svr_impl.h>
#ifdef SVR4
#include <stdlib.h>
#endif /* SVR4 */

#  define DRA_stderr stderr
#  define _OTHER_TEXTSW_FUNCTIONS 1
#  include <xview/textsw.h>
#  include <assert.h> 
#  include <dirent.h>
#  include <sys/stat.h>

#define	TCAR_CONTINUED	0x40000000
#define	TCARF_ESH	0
#define	TCARF_STRING	1

/*
 * Returns the actual number of entities read into the buffer. Caller is
 * responsible for making sure that the buffer is large enough.
 */
Pkg_private int textsw_es_read(Es_handle esh, CHAR *buf, Es_index first, Es_index last_plus_one)
{
	register int result;
	int count;
	register Es_index current, next;

	result = 0;
	current = first;
	es_set_position(esh, current);
	while (last_plus_one > current) {
		next = es_read(esh, (int)(last_plus_one-current), buf + result, &count);
		if READ_AT_EOF
			(current, next, count)
					break;
		result += count;
		current = next;
	}
	return (result);
}

/*
 * ==========================================================
 * 
 * Routines to support the use of selections.
 * 
 * ==========================================================
 */

Pkg_private void textsw_may_win_exit(Textsw_private    priv)
{
	Textsw_view firstview = xv_get(TEXTSW_PUBLIC(priv), OPENWIN_NTH_VIEW, 0);
	Textsw_view_private fv = VIEW_PRIVATE(firstview);

	textsw_flush_caches(fv, TFC_STD);
}

Pkg_private int textsw_is_seln_nonzero(Textsw_private priv, unsigned type)
{
	Es_index first, last_plus_one;
	ev_get_selection(priv->views, &first, &last_plus_one, type);

	return (first < last_plus_one);
}
