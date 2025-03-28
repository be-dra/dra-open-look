#ifndef lint
#ifdef sccs
static char sccsid[] = "@(#)cms_impl.h 1.10 89/08/18   SMI   DRA: RCS $Id: cms_impl.h,v 2.3 2025/03/08 13:26:50 dra Exp $";
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

#define CMS_PRIVATE(cms_public)	XV_PRIVATE(Cms_info, Xv_cms_struct, cms_public)
#define CMS_PUBLIC(cms)		XV_PUBLIC(cms)

#define XV_CMS_BACKGROUND(cms) (cms)->index_table[0]
#define XV_CMS_FOREGROUND(cms) (cms)->index_table[(cms)->size - 1]

#define XV_TO_X_PIXEL(index, cms) \
        (cms)->index_table[((index) >= (cms)->size) ? (cms)->size - 1:(index)]

#define XV_CMS_IS_STATIC(cms)	(cms->type == XV_STATIC_CMS) ? TRUE : FALSE
#define XV_CMS_IS_DEFAULT(cms)	strcmp(cms->name, XV_DEFAULT_CMS) ? FALSE : TRUE
#define XV_DYNAMIC_VISUAL(class) ((class) % 2)

#define XV_INVALID_PIXEL	-1

#define STATUS(cms, field)           ((cms)->status_bits.field)
#define STATUS_SET(cms, field)       STATUS(cms, field) = TRUE
#define STATUS_RESET(cms, field)     STATUS(cms, field) = FALSE
#define BIT_FIELD(field)             unsigned field : 1

/*
 ***********************************************************************
 *		Typedefs, enumerations, and structs
 ***********************************************************************
 */

typedef struct xv_colormap {
    /* BUG: should have a default for the visual field */
    Colormap	    	id;
    Cmap_type		type;
    struct cms_info 	*cms_list;
    struct xv_colormap  *next;
} Xv_Colormap;

struct screen_visual;

typedef struct cms_info {
    Cms			public_self;
    char	       *name;
    Cms_type		type;
    unsigned 	size;
    unsigned long      *index_table;
    Xv_Colormap	       *cmap;
    Xv_Screen           screen;
    struct screen_visual *visual;
    struct cms_info    *next;
    struct {
	BIT_FIELD(default_cms);
	BIT_FIELD(frame_cms);
	BIT_FIELD(control_cms);
    } status_bits;
} Cms_info;

/*
 *************************************************************************
 *		Private functions
 *************************************************************************
 */
/* Pkg_private void	cms_set_size(); */
Pkg_private void cms_free_colors(Display *display, Cms_info	*cms);
Pkg_private void cms_set_name(Cms_info *cms, char	*name);
Pkg_private void cms_set_unique_name(Cms_info *cms);
Pkg_private int cms_set_colors(Cms_info *cms, Xv_Singlecolor *cms_colors, XColor *cms_x_colors, unsigned long cms_index, unsigned long cms_count);
Pkg_private int cms_set_static_colors(Display *display, Cms_info *cms, XColor *xcolors, unsigned long cms_index, unsigned long cms_count);
Pkg_private XColor *cms_parse_named_colors(Cms_info *cms, char **named_colors);
Pkg_private int cms_set_dynamic_colors(Display *display, Cms_info *cms, XColor *xcolors, unsigned long cms_index, unsigned long cms_count);

Xv_private Xv_opaque cms_default_colormap(Xv_opaque server, Display	*display, int screen_number, XVisualInfo *vinfo);
Pkg_private int cms_get_colors(Cms_info *cms, unsigned long cms_index, unsigned long cms_count, Xv_Singlecolor *cms_colors, XColor *cms_x_colors, unsigned char	*red, unsigned char	*green, unsigned char	*blue);
Pkg_private int cms_alloc_static_colors(Display *display, Cms_info *cms, Xv_Colormap *cmap, XColor *xcolors, unsigned long cms_index, unsigned long cms_count);

#endif	/* ~xview_cms_impl_h_DEFINED */
