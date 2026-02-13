#ifndef lint
#ifdef sccs
static char sccsid[] = "@(#)cms_impl.h 1.10 89/08/18   SMI   DRA: RCS $Id: cms_impl.h,v 2.5 2026/02/12 10:55:34 dra Exp $";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL_NOTICE 
 *	file for terms of the license.
 */

#ifndef xview_cms_impl_h_DEFINED
#define	xview_cms_impl_h_DEFINED

#include <xview/cms.h>
#include <xview/screen.h>

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

#define BIT_FIELD(field)             unsigned field : 1
#define XV_INVALID_PIXEL	-1


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

#define STATUS(cms, field)           ((cms)->status_bits.field)
#define STATUS_SET(cms, field)       STATUS(cms, field) = TRUE
#define STATUS_RESET(cms, field)     STATUS(cms, field) = FALSE

#define CMS_PRIVATE(cms_public)	XV_PRIVATE(Cms_info, Xv_cms_struct, cms_public)
#define CMS_PUBLIC(cms)		XV_PUBLIC(cms)

#define XV_CMS_BACKGROUND(cms) (cms)->index_table[0]
#define XV_CMS_FOREGROUND(cms) (cms)->index_table[(cms)->size - 1]

#define XV_TO_X_PIXEL(index, cms) \
        (cms)->index_table[((index) >= (cms)->size) ? (cms)->size - 1:(index)]


#endif	/* ~xview_cms_impl_h_DEFINED */
