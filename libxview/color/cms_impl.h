#ifndef lint
#ifdef sccs
static char sccsid[] = "@(#)cms_impl.h 1.10 89/08/18   SMI   DRA: RCS $Id: cms_impl.h,v 2.4 2026/02/09 14:23:22 dra Exp $";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL_NOTICE 
 *	file for terms of the license.
 */

#ifndef xview_cms_impl_h_DEFINED
#define	xview_cms_impl_h_DEFINED

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <xview/screen.h>
#include <xview_private/scrn_vis.h>
#include <xview/cms.h>
#include <xview/pkg.h>
#include <xview/attr.h>

/*
 ***********************************************************************
 *			Definitions and Macros
 ***********************************************************************
 */
#define XV_DEFAULT_CMS          "xv_default_cms"


/*
 *************************************************************************
 *		Private functions
 *************************************************************************
 */
/* Pkg_private void	cms_set_size(); */
/* Pkg_private void cms_free_colors(Display *display, Cms_info	*cms); */
/* Pkg_private void cms_set_name(Cms_info *cms, char	*name); */
/* Pkg_private void cms_set_unique_name(Cms_info *cms); */
/* Pkg_private int cms_set_colors(Cms_info *cms, Xv_Singlecolor *cms_colors, XColor *cms_x_colors, unsigned long cms_index, unsigned long cms_count); */
/* Pkg_private int cms_set_static_colors(Display *display, Cms_info *cms, XColor *xcolors, unsigned long cms_index, unsigned long cms_count); */
/* Pkg_private XColor *cms_parse_named_colors(Cms_info *cms, char **named_colors); */
/* Pkg_private int cms_set_dynamic_colors(Display *display, Cms_info *cms, XColor *xcolors, unsigned long cms_index, unsigned long cms_count); */
/*  */
/* Xv_private Xv_opaque cms_default_colormap(Xv_opaque server, Display	*display, int screen_number, XVisualInfo *vinfo); */
/* Pkg_private int cms_get_colors(Cms_info *cms, unsigned long cms_index, unsigned long cms_count, Xv_Singlecolor *cms_colors, XColor *cms_x_colors, unsigned char	*red, unsigned char	*green, unsigned char	*blue); */
/* Pkg_private int cms_alloc_static_colors(Display *display, Cms_info *cms, Xv_Colormap *cmap, XColor *xcolors, unsigned long cms_index, unsigned long cms_count); */

#endif	/* ~xview_cms_impl_h_DEFINED */
