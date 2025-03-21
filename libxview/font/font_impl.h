/*      @(#)font_impl.h 20.33 93/06/28 SMI   DRA: RCS $Id: font_impl.h,v 4.2 2024/05/23 11:14:24 dra Exp $     */

/***********************************************************************/
/*	                      font_impl.h			       */
/*	
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license. 
 */
/***********************************************************************/

#ifndef font_impl_h_already_defined
#define font_impl_h_already_defined

#include <sys/types.h>
#include <sys/time.h>

#include <xview/pkg.h>
#include <pixrect/pixrect.h>
#include <pixrect/pixfont.h>
#include <xview/font.h>

#ifdef OW_I18N
#include <xview/xv_i18n.h>
#include <X11/Xresource.h>
#endif /*OW_I18N*/

#include <X11/Xlib.h>

#define	FONT_PRIVATE(font)	XV_PRIVATE(Font_info, Xv_font_struct, font)
#define	FONT_PUBLIC(font)	XV_PUBLIC(font)

#ifdef __STDC__
#define  FONT_STANDARD(font, object)	XV_OBJECT_TO_STANDARD(XV_PUBLIC(font), \
							 #font, object)
#else
#define  FONT_STANDARD(font, object)	XV_OBJECT_TO_STANDARD(XV_PUBLIC(font), \
							 "font", object)
#endif

#define FONT_PIXFONT_TO_PUBLIC(pixfont) \
		(((Pixfont_struct *)(pixfont))->public_self)

#define FONT_PIXFONT_STRUCT_TO_PIXFONT(pfs) (*((Pixfont **)pfs))

#define	FONT_PIX(font)		(Pixfont *)XV_PUBLIC(font)

#ifdef OW_I18N

/* definitions for font set file key words */
#define FS_DEF          "definition"
#define FS_DEF_LEN      strlen(FS_DEF)
 
#define FS_ALIAS        "alias"
#define FS_ALIAS_LEN    strlen(FS_ALIAS)
 
#define FS_SMALL_SIZE           "xv_font_set.small"
#define FS_MEDIUM_SIZE          "xv_font_set.medium"
#define FS_LARGE_SIZE           "xv_font_set.large"
#define FS_XLARGE_SIZE          "xv_font_set.extra_large"
#define FS_DEFAULT_FAMILY       "xv_font_set.default_family"
 
#endif /*OW_I18N*/

/***********************************************************************/
/*	        	Structures 				       */
/***********************************************************************/

typedef struct {
    char	*pixfont[2+(5*256)];
    Xv_Font	public_self;
}Pixfont_struct;

typedef struct family_definitions  {
    char	*family;
    char	*translated;
}Family_defs;

typedef struct style_definitions  {
    char	*style;
    char	*weight;
    char	*slant;
    char	*preferred_name;
}Style_defs;

typedef struct font_locale_info {
    char		*locale;

#ifdef OW_I18N
    XrmDatabase		db;
#endif /*OW_I18N*/

    int			small_size;
    int			medium_size;
    int			large_size;
    int			xlarge_size;

    Family_defs		*known_families;
    Style_defs		*known_styles;

    char		*default_family;
    char		*default_fixedwidth_family;
    char		*default_style;
    char		*default_weight;
    char		*default_slant;
    int			default_scale;
    char		*default_scale_str;
    short		default_size;

    char		*default_small_font;
    char		*default_medium_font;
    char		*default_large_font;
    char		*default_xlarge_font;

    struct font_locale_info    *next;
} Font_locale_info;

typedef struct font_info {
    Xv_Font	 	 public_self;	/* back pointer to public struct */
    Attr_pkg		 pkg;
    Xv_opaque	 	 parent;	/* back pointer to screen */
    Xv_opaque	 	 display;
    Xv_opaque	 	 server;
    struct font_info	*next;
    
#ifdef OW_I18N
    char		**names;
    char                *specifier;
    char		*name;
#else
    /* family, style and point-size */
    char		*name;
#endif /*OW_I18N*/
    Font_locale_info	*locale_info;
    char		*foundry;
    char		*family;
    char		*style;
    
    char		*weight;
    char		*slant;
    char		*setwidthname;
    char		*addstylename;

    int			 scale;
    
    int			 size; /* for this scale */
    int			 small_size;
    int			 medium_size;
    int			 large_size;
    int			 extra_large_size;
    
    int			 ref_count;

    Font_type	 	 type;	/* text, glyph or cursor */

    char		*pixfont; /* pixfont for sunview compat */
    int			def_char_width;
    int			def_char_height;

    /* interface to Xlib */
#ifdef OW_I18N
    XFontSet             set_id;
    XFontStruct          **font_structs;
    int			 column_width;
#endif /*OW_I18N*/
    long unsigned	 xid;
    Xv_opaque 		 x_font_info;

    /* flags */
    unsigned		 has_glyph_prs:1;
    unsigned		 overlapping_chars:1;
} Font_info;

/* from font.c */
Pkg_private     Xv_opaque font_set_avlist(Xv_Font font_public, Attr_attribute avlist[]);
Pkg_private     Xv_opaque font_get_attr(Xv_font font_public, int *status, Attr_attribute attr, va_list args);
Xv_private char *xv_font_monospace(void);
Pkg_private XID xv_load_x_font(Display *display, char *name, Xv_opaque *font_opaque, int *default_x, int *default_y, int *max_char, int *min_char);
Pkg_private void xv_unload_x_font(Display *display, Xv_opaque font_opaque);
Pkg_private void font_check_overlapping(int	*neg_left_bearing, XFontStruct	*x_font_info);
Pkg_private void font_check_var_height (int	*variable_height_font, XFontStruct	*x_font_info);
Xv_private Xv_font xv_find_olglyph_font(Xv_font font_public);
Xv_private Xv_Font xv_font_with_name (Xv_opaque server, char *name);
Xv_private char *xv_font_bold(void);
Pkg_private void font_setup_pixfont(Xv_font_struct	*font_public);
Xv_private void xv_x_char_info(XFontStruct *font, int i, int *x_home, int *y_home, int *x_advance, int *y_advance, Pixrect **pr);
Pkg_private int font_init_pixfont(Xv_font_struct *font_public);
Xv_public Pixfont *xv_pf_open(char *fontname, Xv_opaque srv);
Xv_public void xv_pf_textbound(struct pr_subregion *, int , Pixfont *, char *);
Xv_private void xv_x_char_info(XFontStruct *font, int i, int *x_home, int *y_home, int *x_advance, int *y_advance, Pixrect **pr);
Pkg_private XID xv_load_x_font(Display *display, char *name, Xv_opaque *font_opaque, int *default_x, int *default_y, int *max_char, int *min_char);
Xv_public void xv_real_baseline_when_using_pf(Xv_opaque font, int ch, int *x_x, int *x_y, int pr_x, int pr_y);
Pkg_private XID xv_load_x_font(Display *display, char *name, Xv_opaque *font_opaque, int *default_x, int *default_y, int *max_char, int *min_char);

#endif
