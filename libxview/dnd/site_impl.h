#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)site_impl.h 1.5 93/06/28 DRA: $Id: site_impl.h,v 4.4 2026/05/13 14:06:53 dra Exp $ ";
#endif
#endif

/*
 *      (c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *      pending in the U.S. and foreign countries. See LEGAL NOTICE
 *      file for terms of the license.
 */

#ifndef xview_site_impl_DEFINED
#define xview_site_impl_DEFINED

#include <xview/dragdrop.h>


Xv_private int DndStoreSiteData(Xv_drop_site site_public, long **prop);
Xv_private int DndSiteContains(Xv_drop_site site_public, int fx, int fy);

#endif  /* ~xview_site_impl_DEFINED */
