#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)icon.c 20.16 90/02/26 DRA: RCS $Id: icon.c,v 4.4 2026/03/29 14:32:24 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL_NOTICE 
 *	file for terms of the license.
 */

/*
 * icon.c - Display icon.
 */

#include <stdio.h>
#include <xview_private/i18n_impl.h>
#include <xview/frame.h>
#include <xview/xview.h>
#include <xview/rect.h>
#include <xview/rectlist.h>
#include <xview/defaults.h>
#include <xview/pixwin.h>
#include <xview/font.h>
#include <xview/icon_load.h>
#include <xview_private/svrim_impl.h>
#include <xview_private/pw_impl.h>

typedef struct {
	Icon		public_self;	/* Back pointer */
	Rect		ic_gfxrect;	/* where the graphic goes */
	struct pixrect *ic_mpr;		/* the graphic (a memory pixrect) */
	Rect		ic_textrect;	/* where text goes */
#ifdef OW_I18N
        wchar_t        *ic_text_wcs;    /* primary text data */
#endif
	char	       *ic_text;	/* the text */
	int		ic_flags;
	Xv_opaque	frame;		/* frame Icon is assoc w/ */
	Server_image    ic_mask;        /* graphic mask (pixmap) */
	unsigned long	workspace_pixel; /* The pixel value of the workspace */
        char           *workspace_color; /* wrk space color string */
} Xv_icon_info;

/* flag values */
#define ICON_PAINTED	 0x20		/* icon window has been painted */
#define ICON_BKGDTRANS   0x40            /* transparent window */
#define ICON_TRANSLABEL  0x80            /* transparent labels */
#define	ICON_FIRSTPRIV	 0x0100		/* start of private flags range */
#define	ICON_LASTPRIV	 0x8000		/* end of private flags range */

/*****************************************************************************/
/* typedefs                                                                  */
/*****************************************************************************/

typedef Xv_icon_info *icon_handle;

/*	Other Macros 	*/
#define ICON_PRIVATE(icon) \
	XV_PRIVATE(Xv_icon_info, Xv_icon, icon)
#define ICON_PUBLIC(icon)	XV_PUBLIC(icon)

#define ICON_IS_TRANSPARENT(icon) \
  ((icon)->icon_mask || ((icon)->ic_flags & ICON_BKGTRANS))

#define NULL_PIXRECT	((struct pixrect *)0)
#define NULL_PIXFONT	((struct pixfont *)0)

FILE *icon_open_header(char *from_file, char *error_msg, Xv_icon_header_info *info)
/* See comments in icon_load.h */
{
#define INVALID	-1
	register int c;
	char c_temp;
	register FILE *result = 0;

	if (*from_file == '\0' || (result = fopen(from_file, "r")) == NULL) {
		sprintf(error_msg, XV_MSG("Cannot open file %s.\n"), from_file);
		if (result) fclose(result);
		return NULL;
	}
	info->depth = INVALID;
	info->height = INVALID;
	info->format_version = INVALID;
	info->valid_bits_per_item = INVALID;
	info->width = INVALID;
	info->last_param_pos = INVALID;
	/*
	 * Parse the file header
	 */
	do {
		if ((c = fscanf(result, "%*[^DFHVW*]")) == EOF)
			break;
		switch (c = getc(result)) {
			case 'D':
				if (info->depth == INVALID) {
					c = fscanf(result, "epth=%d", &info->depth);
					if (c == 0)
						c = 1;
					else
						info->last_param_pos = ftell(result);
				}
				break;
			case 'H':
				if (info->height == INVALID) {
					c = fscanf(result, "eight=%d", &info->height);
					if (c == 0)
						c = 1;
					else
						info->last_param_pos = ftell(result);
				}
				break;
			case 'F':
				if (info->format_version == INVALID) {
					c = fscanf(result, "ormat_version=%d",
							&info->format_version);
					if (c == 0)
						c = 1;
					else
						info->last_param_pos = ftell(result);
				}
				break;
			case 'V':
				if (info->valid_bits_per_item == INVALID) {
					c = fscanf(result, "alid_bits_per_item=%d",
							&info->valid_bits_per_item);
					if (c == 0)
						c = 1;
					else
						info->last_param_pos = ftell(result);
				}
				break;
			case 'W':
				if (info->width == INVALID) {
					c = fscanf(result, "idth=%d", &info->width);
					if (c == 0)
						c = 1;
					else
						info->last_param_pos = ftell(result);
				}
				break;
			case '*':
				if (info->format_version == 1) {
					c = 1;
					c_temp = getc(result);
					if (c_temp == '/')
						c = 0;	/* Force exit */
					else {
						fprintf(stderr, "in '*' case: c_temp='%c'\n", c_temp);
						(void)ungetc(c_temp, result);
					}
				}
				break;
			default:{
					(void)sprintf(error_msg,
							XV_MSG("icon file %s parse failure\n"), from_file);
					if (result) fclose(result);
					return NULL;
				}
		}
	} while (c != 0 && c != EOF);
	if (c == EOF || info->format_version != 1) {
		(void)sprintf(error_msg,
				XV_MSG("%s has invalid header format.\n"), from_file);
		if (result) fclose(result);
		return NULL;
	}
	if (info->depth == INVALID)
		info->depth = 1;
	if (info->height == INVALID)
		info->height = 64;
	if (info->valid_bits_per_item == INVALID)
		info->valid_bits_per_item = 16;
	if (info->width == INVALID)
		info->width = 64;
	if (info->depth != 1) {
		(void)sprintf(error_msg,
				XV_MSG("Cannot handle Depth of %d.\n"), info->depth);
		if (result) fclose(result);
		return NULL;
	}
	if (info->valid_bits_per_item != 16 && info->valid_bits_per_item != 32) {
		(void)sprintf(error_msg,
				XV_MSG("Cannot handle Valid_bits_per_item of %d.\n"),
				info->valid_bits_per_item);
		if (result) fclose(result);
		return NULL;
	}
	if ((info->width % 16) != 0) {
		(void)sprintf(error_msg,
				XV_MSG("Cannot handle Width of %d.\n"), info->width);
		if (result) fclose(result);
		return NULL;
	}
	return (result);

#undef INVALID
}

static void icon_read_pr(FILE *fd, Xv_icon_header_info *header, struct pixrect *pr)
{
    register int    c, i, j, index;
    register struct mpr_data *mprdata;
    long            value;

    mprdata = (struct mpr_data *) (pr->pr_data);

    for (i = 0; i < header->height; i++) {
	for (j = 0; j < header->width / 16; j++) {
	    c = fscanf(fd, " 0x%lx,", &value);
	    if (c == 0 || c == EOF)
		break;

	    index = j + i * mprdata->md_linebytes / 2;
	    switch (header->valid_bits_per_item) {
	      case 16:
		mprdata->md_image[index] = value;
		break;
#ifdef sun
	      case 32:
		mprdata->md_image[index] = (value >> 16);
		mprdata->md_image[index] = (value & 0xFFFF);
		break;
#endif
	      default:
		xv_error(XV_NULL,
			 ERROR_SEVERITY, ERROR_NON_RECOVERABLE,
			 ERROR_STRING,
			     XV_MSG("icon file header valid bits not 16 or 32"),
			 ERROR_PKG, ICON,
			 NULL);
	    }
	}
    }
}

struct pixrect *icon_load_mpr(char *from_file, char *error_msg)
/* See comments in icon_load.h */
{
    register FILE  *fd;
    Xv_icon_header_info header;
    register struct pixrect *result;

    fd = icon_open_header(from_file, error_msg, &header);
    if (fd == NULL)
	return (NULL_PIXRECT);
    /*
     * Allocate the pixrect and read the actual bits making up the icon.
     */
    result = xv_mem_create(header.width, header.height, header.depth);
    if (result == NULL_PIXRECT) {
	(void) sprintf(error_msg, 
	    XV_MSG("Cannot create memory pixrect %dx%dx%d.\n"),
		       header.width, header.height, header.depth);
	goto Return;
    }
    icon_read_pr(fd, &header, result);
Return:
    (void) fclose(fd);
    return (result);
}


Server_image icon_load_svrim(char *from_file, char *error_msg)
{
    register FILE  *fd;
    Display	   *display;
    GC		    gc;
    Xv_icon_header_info header;
    Xv_Drawable_info *info;
    register struct pixrect *mpr;
    Server_image result = XV_NULL;

    fd = icon_open_header(from_file, error_msg, &header);
    if (fd == NULL) return (XV_NULL);
    /*
     * Allocate the memory pixrect and read the actual bits making up the icon.
     */
    mpr = xv_mem_create(header.width, header.height, header.depth);
    if (mpr == NULL_PIXRECT) {
	(void) sprintf(error_msg, 
	    XV_MSG("Cannot create memory pixrect %dx%dx%d.\n"),
		       header.width, header.height, header.depth);
	goto Return;
    }
    icon_read_pr(fd, &header, mpr);
    
    /* 
     * Create the Server Image from the memory pixrect.
     */
    result = xv_create(XV_NULL, SERVER_IMAGE,
	XV_WIDTH,	header.width,
	XV_HEIGHT,	header.height,
	SERVER_IMAGE_DEPTH, header.depth,
	NULL);

    DRAWABLE_INFO_MACRO(result, info);
    display = xv_display(info);
    gc = xv_gc(result, info);
    xv_set_gc_op(display, info, gc, PIX_SRC, XV_USE_CMS_FG, XV_DEFAULT_FG_BG);
    XSetPlaneMask(display, gc, (unsigned long)((0x1 << mpr->pr_depth) - 1));
    xv_rop_mpr_internal(display, xv_xid(info), gc, 0, 0,
				mpr->pr_width, mpr->pr_height, (Xv_opaque)mpr, 0,0, info, TRUE);
#ifdef BEFORE_DRA_CHANGED_IT
    xv_free(mpr);
#else /* BEFORE_DRA_CHANGED_IT */
	xv_mem_destroy(mpr);
#endif /* BEFORE_DRA_CHANGED_IT */

Return:
    (void) fclose(fd);
    return result;
}


int icon_init_from_pr(Icon icon_public, struct pixrect *pr)
/* See comments in icon_load.h */
{
    Xv_icon_info   *icon = ICON_PRIVATE(icon_public);

    icon->ic_mpr = pr;
    /*
     * Set the icon's size and graphics area to match its pixrect's extent.
     */
    icon->ic_gfxrect.r_top = icon->ic_gfxrect.r_left = 0;
    icon->ic_gfxrect.r_width = pr->pr_size.x;
    icon->ic_gfxrect.r_height = pr->pr_size.y;
    /*
     * By default, the icon has no text or associated area.
     */
    icon->ic_textrect = rect_null;
    icon->ic_text = NULL;
    icon->ic_flags = 0;
	return TRUE;
}

int icon_load (Icon icon_public, char *from_file, char *error_msg)
/* See comments in icon_load.h */
{
    register struct pixrect *pr;

    if (!icon_public)
	return (XV_ERROR);

    pr = icon_load_mpr(from_file, error_msg);
    if (pr == NULL_PIXRECT)
	return (XV_ERROR);
    (void) icon_init_from_pr(icon_public, pr);
    return (XV_OK);
}

static void FillRect(Xv_Window win, unsigned long bkg_pixel, int x, int y, int w, int h)
{
    register Xv_Drawable_info  *info;
    Display  *display;
    XID      xid;
    GC       gc;
    XGCValues  val;
    unsigned long  val_mask;
    
    DRAWABLE_INFO_MACRO( win, info );
    display = xv_display( info );
    xid = (XID) xv_xid(info);

    gc = xv_find_proper_gc( display, info, PW_ROP );
    val.function = GXcopy;
    val.foreground = bkg_pixel;
    val.fill_style = FillSolid;
    val.clip_mask = 0;
    val_mask = GCClipMask | GCFillStyle | GCForeground | GCFunction;
    XChangeGC(display, gc, val_mask, &val );
    XFillRectangle( display, xid, gc, x, y, (unsigned)w, (unsigned)h );
}

static void DrawTransparentIcon(Xv_icon_info *icon, Xv_Window pixwin, int x, int y, unsigned long bkg_color)
{
    register Xv_Drawable_info  *info, *src_info;
    Display  *display;
    XID      xid;
    GC       gc;
    XGCValues  val;
    unsigned long  val_mask;
    
    DRAWABLE_INFO_MACRO( pixwin, info );
    display = xv_display( info );
    xid = (XID) xv_xid(info);
    
	
    DRAWABLE_INFO_MACRO( (Xv_opaque) icon->ic_mpr, src_info );
    gc = xv_find_proper_gc( display, info, PW_ROP );
    val.function = GXcopy;
    val.plane_mask = xv_plane_mask(info);
    val.background = bkg_color;
    val.foreground = xv_fg(info);
    val.stipple = xv_xid(src_info);
    val.fill_style = FillOpaqueStippled;
    val.ts_x_origin = 0;
    val.ts_y_origin = 0;	
    val_mask = GCForeground | GCBackground | GCFunction | 
               GCPlaneMask | GCFillStyle | GCTileStipXOrigin | 
               GCTileStipYOrigin | GCStipple;
	
    XChangeGC(display, gc, val_mask, &val );
    XFillRectangle( display, xid, gc, icon->ic_gfxrect.r_left + x,
		   icon->ic_gfxrect.r_top + y,
		   (unsigned)icon->ic_gfxrect.r_width,
		   (unsigned)icon->ic_gfxrect.r_height );
}

static int DrawNonRectIcon(Display *display, XID xid, Xv_icon_info *icon, Xv_Drawable_info *info, int x, int y)
{
    register Xv_Drawable_info  *src_info, *mask_info;
    GC       gc;
    XGCValues  val;
    unsigned long  val_mask;

    DRAWABLE_INFO_MACRO( (Xv_opaque) icon->ic_mask, mask_info );
    gc = xv_find_proper_gc( display, info, PW_ROP );

    val.function = GXcopy;
    val.plane_mask = xv_plane_mask(info);
    val.background = xv_bg(info);
    val.foreground = xv_fg(info);

    val.fill_style = FillOpaqueStippled;
    val.ts_x_origin = 0;
    val.ts_y_origin = 0;	
    val_mask = GCForeground | GCBackground | GCFunction | 
               GCPlaneMask | GCFillStyle | GCTileStipXOrigin | 
               GCTileStipYOrigin;
    XChangeGC(display, gc, val_mask, &val );

    if (PR_NOT_MPR(((Pixrect *) icon->ic_mpr)))  {
	DRAWABLE_INFO_MACRO( (Xv_opaque) icon->ic_mpr, src_info );
	val.clip_mask = xv_xid(mask_info);
	val.stipple = xv_xid(src_info);
	val_mask = GCStipple | GCClipMask;
	XChangeGC(display, gc, val_mask, &val );

	if ( xv_rop_internal( display, xid, gc, icon->ic_gfxrect.r_left + x,
			     icon->ic_gfxrect.r_top + y,
			     icon->ic_gfxrect.r_width, icon->ic_gfxrect.r_height,
			     (Xv_opaque) icon->ic_mpr, 0, 0, info ) == XV_ERROR) {
	    xv_error( XV_NULL, ERROR_STRING, 
		XV_MSG("xv_rop: xv_rop_internal failed"), NULL );
	}
    }
    else {
	if (xv_rop_mpr_internal( display, xid, gc, icon->ic_gfxrect.r_left + x,
			     icon->ic_gfxrect.r_top + y,
			     icon->ic_gfxrect.r_width, icon->ic_gfxrect.r_height,
			     (Xv_opaque)icon->ic_mpr, 0, 0, info, TRUE) == XV_ERROR)
	return(XV_ERROR);
    }

    return XV_OK;
}

#ifdef OW_I18N
static void
DrawWCString( win, frg_pixel, bkg_pixel, x, y, font_set, str )
register Xv_Window  win;
unsigned long       frg_pixel, bkg_pixel;
register int        x, y;
XFontSet            font_set;
wchar_t            *str;
{
    register Xv_Drawable_info  *info;
    Display  *display;
    XID      xid;
    GC       gc;
    XGCValues  val;
    unsigned long  val_mask;
    
    DRAWABLE_INFO_MACRO( win, info );
    display = xv_display( info );
    xid = (XID) xv_xid(info);

    gc = xv_find_proper_gc( display, info, PW_TEXT );
    val.function = GXcopy;
    val.foreground = frg_pixel;
    val.background = bkg_pixel;
    val.clip_mask = None;
    val_mask = GCBackground | GCForeground | GCClipMask;
    XChangeGC(display, gc, val_mask, &val );

    XwcDrawString( display, xid, font_set, gc, x, y, str, wslen(str) );
}
#else 
static void DrawString(Xv_Window win, unsigned long frg_pixel,
			unsigned long bkg_pixel, int x, int y, Xv_opaque pixfont, char *str)
{
    register Xv_Drawable_info  *info;
    Display  *display;
    XID      xid, font;
    GC       gc;
    XGCValues  val;
    unsigned long  val_mask;
    
    DRAWABLE_INFO_MACRO( win, info );
    display = xv_display( info );
    xid = (XID) xv_xid(info);
    font = (XID) xv_get( pixfont, XV_XID );

    gc = xv_find_proper_gc( display, info, PW_TEXT );
    val.function = GXcopy;
    val.foreground = frg_pixel;
    val.background = bkg_pixel;
    val.clip_mask = None;
    val_mask = GCBackground | GCForeground | GCClipMask;
    XChangeGC(display, gc, val_mask, &val );
    XSetFont(display, gc, font );

    XDrawString( display, xid, gc, x, y, str, (int)strlen(str) );
}
#endif


static void icon_draw_label(Xv_icon_info *icon, Xv_Window pixwin,
			Xv_Drawable_info *info, int x, int y, unsigned long wrk_space_pixel)
{	
    PIXFONT        *font = (PIXFONT *) xv_get(pixwin, XV_FONT);
    int            left, top, line_leading = xv_get((Xv_opaque)font, FONT_DEFAULT_CHAR_HEIGHT);
#ifdef OW_I18N
    XFontSet       font_set;
    Display        *dpy;
    XRectangle     overall_ink_extents = {0};
    XRectangle     overall_logical_extents = {0};
#else  
    XFontStruct    *x_font_info;
#endif
    int            descent = 0;
    int            ascent = 0;
    int            direction = 0;
    XCharStruct    overall_return;
    struct rect    textrect;
	
    /*
     * Initialize overall_return to zeros
     * It is not initialized like overall_ink_extents above because the MIT 
     * build (using cc), complains about "no automatic aggregate initialization"
     */
    XV_BZERO(&overall_return, sizeof(XCharStruct));

    if (rect_isnull(&icon->ic_textrect)) 
        /* Set text rect to accomodate 1 line at bottom. */
        rect_construct(&icon->ic_textrect,
		       0, icon->ic_gfxrect.r_height - line_leading,
		       icon->ic_gfxrect.r_width, line_leading);

    if ( (icon->ic_flags & ICON_BKGDTRANS) || icon->ic_mask ) {
	if ( !( icon->ic_flags & ICON_TRANSLABEL) )  /*check for transparent label*/
            FillRect( pixwin, wrk_space_pixel,
		 icon->ic_textrect.r_left + x, icon->ic_textrect.r_top + y - 3,
		 icon->ic_textrect.r_width, icon->ic_textrect.r_height + 3);	
    }
    else
        /* Blank out area onto which text will go. */
        (void) xv_rop(pixwin,
		      icon->ic_textrect.r_left + x, icon->ic_textrect.r_top + y-3,
		      icon->ic_textrect.r_width, icon->ic_textrect.r_height+3,
		      PIX_CLR, (Pixrect *)NULL, 0, 0 );

    /* Format text into textrect */
    textrect = icon->ic_textrect;
    textrect.r_left += x;
    textrect.r_top += y;

#ifdef OW_I18N
    font_set = (XFontSet) xv_get((Xv_opaque)font, FONT_SET_ID);
    dpy = xv_display( info );
 
    XwcTextExtents(font_set, icon->ic_text_wcs, wslen(icon->ic_text_wcs),
	&overall_ink_extents, &overall_logical_extents);
    left = (int)(icon->ic_gfxrect.r_width - overall_logical_extents.width)/2;
    if (left < 0)
        left = 0;
    top = textrect.r_top - overall_logical_extents.y - 3;
#else /* OW_I18N */
    x_font_info = (XFontStruct *) xv_get((Xv_opaque)font, FONT_INFO );

    (void) XTextExtents( x_font_info, icon->ic_text, (int)strlen(icon->ic_text),
			&direction, &ascent, &descent, &overall_return );

    left = (icon->ic_gfxrect.r_width - overall_return.width)/2;
    if (left < 0)  
	left = 0;

    top = textrect.r_top + x_font_info->ascent -3;
#endif

    if ( (icon->ic_flags & ICON_BKGDTRANS) || icon->ic_mask )
#ifdef OW_I18N
	DrawWCString(pixwin,xv_fg(info),wrk_space_pixel,
		   left,top,font_set,icon->ic_text_wcs);
#else
        DrawString(pixwin, xv_fg(info), wrk_space_pixel,
		   left, top, (Xv_opaque)font, icon->ic_text);
#endif
    else
#ifdef OW_I18N
    {
        GC              gc;
        Drawable        d;
 
        gc = xv_find_proper_gc(dpy, info, PW_TEXT);
        d = xv_xid( info );
        xv_set_gc_op(dpy, info, gc, PIX_SRC,
                PIX_OPCOLOR(PIX_SRC) ? XV_USE_OP_FG : XV_USE_CMS_FG,
                XV_DEFAULT_FG_BG);
        (void) XwcDrawString(dpy, d, font_set, gc,
                left, top, icon->ic_text_wcs, wslen(icon->ic_text_wcs));
    }
#else
        (void) xv_text(pixwin, left, top, PIX_SRC, (Xv_opaque)font, icon->ic_text);
#endif
    
}
    

static void icon_display(Icon icon_public, int x, int y)
{
    register Xv_icon_info      *icon = ICON_PRIVATE(icon_public);
    register Xv_Window         pixwin = icon_public;
    register Xv_Drawable_info  *info;
    register Display           *display;
    register XID               xid;
    
    DRAWABLE_INFO_MACRO( pixwin, info );
    display = xv_display( info );
    xid = (XID) xv_xid(info);

    if ( icon->ic_mask )  {   /* we have a icon mask to use */
        FillRect( pixwin, icon->workspace_pixel,
		 icon->ic_gfxrect.r_left, icon->ic_gfxrect.r_top,
		 icon->ic_gfxrect.r_width, icon->ic_gfxrect.r_height);	
	DrawNonRectIcon( display, xid, icon, info, x, y );
    } else {
	if (icon->ic_mpr ) {
	    if ( icon->ic_flags & ICON_BKGDTRANS ) 	
	        DrawTransparentIcon( icon, pixwin, x, y, icon->workspace_pixel );
	    else
	        (void) xv_rop(pixwin,
		      icon->ic_gfxrect.r_left + x, icon->ic_gfxrect.r_top + y,
		      icon->ic_gfxrect.r_width, icon->ic_gfxrect.r_height,
		      PIX_SRC, icon->ic_mpr, 0, 0);
	}
    }
#ifdef OW_I18N
    if (icon->ic_text_wcs && (icon->ic_text_wcs[0] != '\0'))
#else  
    if (icon->ic_text && (icon->ic_text[0] != '\0')) 
#endif
	icon_draw_label( icon, pixwin, info, x, y, icon->workspace_pixel );
    icon->ic_flags |= ICON_PAINTED;
}


Icon icon_create(Attr_attribute attr1, ...)
{
    Attr_attribute  avlist[ATTR_STANDARD_SIZE];
    va_list         valist;

    if( attr1 )
    {
        VA_START(valist, attr1);
        copy_va_to_av( valist, avlist, attr1 );
        va_end(valist);
    }
    else
        avlist[0] = XV_NULL;

    return (Icon) xv_create_avlist(XV_NULL, ICON, avlist);
}

static Notify_value icon_input(Icon icon_public, Event *event,
						Notify_arg arg, Notify_event_type type)
{
	if (event_action(event) == WIN_REPAINT) {
		icon_display(icon_public, 0, 0);
		return NOTIFY_DONE;
	}
	return (NOTIFY_IGNORED);
}

static int icon_init(Xv_opaque parent, Xv_opaque object, Attr_avlist avlist,
							int *unused)
{
	register Xv_icon_info *icon;
	Rect recticon;

	((Xv_icon *) (object))->private_data = (Xv_opaque) xv_alloc(Xv_icon_info);
	if (!(icon = ICON_PRIVATE(object))) {
		xv_error(object,
				ERROR_LAYER, ERROR_SYSTEM,
				ERROR_STRING, XV_MSG("Can't allocate icon structure"),
				ERROR_PKG, ICON,
				NULL);
		return XV_ERROR;
	}
	icon->public_self = object;
	icon->ic_gfxrect.r_width = 64;
	icon->ic_gfxrect.r_height = 64;
	rect_construct(&recticon, 0, 0, 64, 64);
	icon->workspace_color = (char *)xv_calloc((unsigned)sizeof(char), 30);
	xv_set(object,
			XV_SHOW, FALSE,
			WIN_CONSUME_EVENT, WIN_REPAINT,
			WIN_NOTIFY_SAFE_EVENT_PROC, icon_input,
			WIN_NOTIFY_IMMEDIATE_EVENT_PROC, icon_input,
			WIN_RECT, &recticon,
			NULL);
	return XV_OK;
}

/*****************************************************************************/
/* icon_destroy	                                                             */
/*****************************************************************************/
int icon_destroy(Icon icon_public)
{
    return xv_destroy(icon_public);
}


/*****************************************************************************/
/* icon_destroy_internal						     */
/*****************************************************************************/

static int icon_destroy_internal(Icon icon_public, Destroy_status status)
{

	Xv_icon_info *icon = ICON_PRIVATE(icon_public);

	if ((status == DESTROY_CHECKING) || (status == DESTROY_SAVE_YOURSELF))
		return XV_OK;

	if (icon->ic_text) free(icon->ic_text);
	free((char *)(icon->workspace_color));
	free((char *)icon);

	return XV_OK;
}

/*****************************************************************************/
/* icon_set                                                                  */
/*****************************************************************************/

int
#ifdef ANSI_FUNC_PROTO
icon_set(Icon icon_public, ...)
#else
icon_set(icon_public, va_alist)
    Icon            icon_public;
va_dcl
#endif
{
    AVLIST_DECL;
    va_list         valist;

    VA_START(valist, icon_public);
    MAKE_AVLIST( valist, avlist );
    va_end(valist);
    return (int) xv_set_avlist(icon_public, avlist);
}

static void icon_set_wrk_space_color(Icon icon_public)
{
	Xv_icon_info *icon = ICON_PRIVATE(icon_public);
	Display *display;
	XID xid;
	Colormap cmap;
	register Xv_Drawable_info *info;
	char *color_name;
	XColor color;
	int valid_color = FALSE;
	screen_ui_style_t ui_style;

	DRAWABLE_INFO_MACRO(icon_public, info);
	ui_style = (screen_ui_style_t)xv_get(xv_screen(info), SCREEN_UI_STYLE);

	if (ui_style != SCREEN_UIS_2D_BW) {
		color_name = defaults_get_string("openWindows.workspaceColor",
						"OpenWindows.WorkspaceColor", "#cccccc");
	}
	else {
		color_name = "white";
	}

	if (strcmp(color_name, icon->workspace_color) == 0)	/* no change */
		return;
	else strncpy(icon->workspace_color, color_name, 29L);

	display = xv_display(info);
	xid = (XID) xv_xid(info);
	cmap = xv_get(xv_cms(info), XV_XID);

	if (strlen(color_name)) {
		if (!XParseColor(display, cmap, color_name, &color)) {
			char msg[100];

			sprintf(msg,
					XV_MSG("icon: color name \"%s\" not in database"),
					color_name);
			xv_error(XV_NULL, ERROR_SEVERITY, ERROR_RECOVERABLE,
					ERROR_STRING, msg, ERROR_PKG, ICON, NULL);
		}
		else if (!XAllocColor(display, cmap, &color)) {
			xv_error(XV_NULL, ERROR_SEVERITY, ERROR_RECOVERABLE,
					ERROR_STRING,
					XV_MSG("icon: all color cells are allocated"),
					ERROR_PKG, ICON, NULL);
		}
		else {
			valid_color = TRUE;
		}
	}

	if (valid_color)
		icon->workspace_pixel = color.pixel;
	else
		icon->workspace_pixel =
				(unsigned long)xv_get(xv_cms(info), CMS_BACKGROUND_PIXEL);
	XSetWindowBackground(display, xid, icon->workspace_pixel);
}

static Xv_opaque icon_set_internal(Icon icon_public, Attr_avlist avlist)
{
	register Xv_icon_info *icon = ICON_PRIVATE(icon_public);
	register Xv_opaque arg1;
	short repaint = FALSE;
	short label_changed = FALSE;
	short change_color = FALSE;

	for (; *avlist; avlist = attr_next(avlist)) {
		arg1 = avlist[1];
		switch (*avlist) {
			case ICON_WIDTH:
				icon->ic_gfxrect.r_width = (short)arg1;
				repaint = TRUE;
				break;

			case ICON_HEIGHT:
				icon->ic_gfxrect.r_height = (short)arg1;
				repaint = TRUE;
				break;

			case ICON_IMAGE:
				icon->ic_mpr = (struct pixrect *)arg1;
				repaint = TRUE;
				break;

			case ICON_IMAGE_RECT:
				if (arg1) {
					icon->ic_gfxrect = *(Rect *) arg1;
					repaint = TRUE;
				}
				break;

			case ICON_LABEL_RECT:
				if (arg1) {
					icon->ic_textrect = *(Rect *) arg1;
					repaint = TRUE;
				}
				break;

			case XV_LABEL:
				/* Consume attribute so that generic handler not also invoked.*/
				*avlist = (Xv_opaque) ATTR_NOP(*avlist);
#ifdef OW_I18N
				if (icon->ic_text_wcs)
					xv_free(icon->ic_text_wcs);
				if (icon->ic_text) {
					xv_free(icon->ic_text);
					icon->ic_text = NULL;
				}
				if ((char *)arg1)
					icon->ic_text_wcs =
							(wchar_t *)_xv_mbstowcsdup((char *)arg1);
#else
				if (icon->ic_text)
					free(icon->ic_text);
				if ((char *)arg1) {
					icon->ic_text =
							(char *)xv_calloc(1,
							(unsigned)strlen((char *)arg1) + 1);
					strcpy(icon->ic_text, (char *)arg1);
				}
#endif
				label_changed = TRUE;
				repaint = TRUE;
				icon->ic_flags &= (~ICON_TRANSLABEL);	/* set the flag to 0 */
				break;

#ifdef OW_I18N
			case XV_LABEL_WCS:
				/* Consume attribute so that generic handler not also invoked. */
				*avlist = (Xv_opaque) ATTR_NOP(*avlist);
				if (icon->ic_text_wcs)
					xv_free(icon->ic_text_wcs);
				if (icon->ic_text) {
					free(icon->ic_text);
					icon->ic_text = NULL;
				}
				if ((wchar_t *)arg1)
					icon->ic_text_wcs = wsdup((wchar_t *)arg1);
				label_changed = TRUE;
				repaint = TRUE;
				icon->ic_flags &= (~ICON_TRANSLABEL);	/* set the flag to 0 */
				break;
#endif /* OW_I18N */

			case XV_OWNER:{

					/*
					 * Consume attribute so that generic handler not also
					 * invoked.
					 */
					*avlist = (Xv_opaque) ATTR_NOP(*avlist);
					icon->frame = arg1;
					break;
				}

			case WIN_CMS_CHANGE:
				repaint = TRUE;
				break;

			case ICON_TRANSPARENT:
				if (arg1)
					icon->ic_flags |= ICON_BKGDTRANS;
				else
					icon->ic_flags &= (~ICON_BKGDTRANS);
				repaint = TRUE;
				change_color = TRUE;
				break;

			case ICON_MASK_IMAGE:
				icon->ic_mask = (Server_image) arg1;
				change_color = TRUE;
				repaint = TRUE;
				break;

			case ICON_TRANSPARENT_LABEL:

#ifdef OW_I18N
				if (icon->ic_text_wcs)
					xv_free(icon->ic_text_wcs);
				if (icon->ic_text) {
					xv_free(icon->ic_text);
					icon->ic_text = NULL;
				}
				if ((char *)arg1)
					icon->ic_text_wcs =
							(wchar_t *)_xv_mbstowcsdup((char *)arg1);
#else
				if (icon->ic_text)
					free(icon->ic_text);
				if ((char *)arg1) {
					icon->ic_text =
							(char *)xv_calloc(1,
							(unsigned)strlen((char *)arg1) + 1);
					strcpy(icon->ic_text, (char *)arg1);
				}
#endif

				label_changed = TRUE;
				icon->ic_flags |= ICON_TRANSLABEL;
				repaint = TRUE;
				break;

#ifdef OW_I18N
			case ICON_TRANSPARENT_LABEL_WCS:
				if (icon->ic_text_wcs)
					xv_free(icon->ic_text_wcs);
				if (icon->ic_text) {
					xv_free(icon->ic_text);
					icon->ic_text = NULL;
				}
				if ((wchar_t *)arg1)
					icon->ic_text_wcs = wsdup((wchar_t *)arg1);
				label_changed = TRUE;
				icon->ic_flags |= ICON_TRANSLABEL;
				repaint = TRUE;
				break;
#endif /* OW_I18N */

			case XV_END_CREATE:
				/*
				 * reparent the icon to force it to be child of root.
				 */
				xv_set(icon_public, WIN_PARENT, xv_get(icon_public, XV_ROOT),
						NULL);
				break;
			default:
				if (xv_check_bad_attr(ICON, *avlist) == XV_OK) {
					return *avlist;
				}
				break;
		}
	}

	/*
	 * tell the window manager the new icon name, this provides a fall-back
	 * for window managers whose ideas about icons differ widely from those
	 * of the client.
	 */
	if (label_changed && icon->frame) {
		Xv_Drawable_info *info;

		DRAWABLE_INFO_MACRO(icon->frame, info);
		XSetIconName(xv_display(info), xv_xid(info), icon->ic_text);
	}

	if (change_color)
		icon_set_wrk_space_color(icon_public);

	if (repaint && icon->ic_flags & ICON_PAINTED)
		icon_display(icon_public, 0, 0);

	return (Xv_opaque) XV_OK;
}

/*****************************************************************************/
/* icon_get                                                                  */
/*****************************************************************************/

Xv_opaque icon_get(Icon icon_public, Icon_attribute attr)
{
    return xv_get(icon_public, attr);
}


static Xv_opaque icon_get_internal(Icon icon_public, int *status,
								Attr_attribute attr, va_list args)
{
	Xv_icon_info *icon = ICON_PRIVATE(icon_public);

	switch (attr) {

		case ICON_WIDTH:
			return (Xv_opaque) icon->ic_gfxrect.r_width;

		case ICON_HEIGHT:
			return (Xv_opaque) icon->ic_gfxrect.r_height;

		case ICON_IMAGE:
			return (Xv_opaque) icon->ic_mpr;

		case ICON_IMAGE_RECT:
			return (Xv_opaque) & (icon->ic_gfxrect);

		case ICON_LABEL_RECT:
			return (Xv_opaque) & (icon->ic_textrect);

		case XV_LABEL:

#ifdef OW_I18N
			if (icon->ic_text == NULL && icon->ic_text_wcs != NULL)
				icon->ic_text =
						(char *)_xv_wcstombsdup((wchar_t *)icon->ic_text_wcs);
#endif

			return (Xv_opaque) icon->ic_text;

#ifdef OW_I18N
		case XV_LABEL_WCS:
			return (Xv_opaque) icon->ic_text_wcs;
#endif

		case XV_OWNER:
			return (Xv_opaque) icon->frame;

		case ICON_TRANSPARENT_LABEL:

#ifdef OW_I18N
			if (icon->ic_text == NULL && icon->ic_text_wcs != NULL)
				icon->ic_text =
						(char *)_xv_wcstombsdup((wchar_t *)icon->ic_text_wcs);
#endif

			return (Xv_opaque) icon->ic_text;

#ifdef OW_I18N
		case ICON_TRANSPARENT_LABEL_WCS:
			return (Xv_opaque) icon->ic_text_wcs;
#endif

		case ICON_TRANSPARENT:
			return (Xv_opaque) (icon->ic_flags & ICON_BKGDTRANS);

		case ICON_MASK_IMAGE:
			return (Xv_opaque) icon->ic_mask;

		default:
			if (xv_check_bad_attr(ICON, attr) == XV_ERROR) {
				*status = XV_ERROR;
			}
			return (Xv_opaque) NULL;
	}
	/* NOTREACHED */
}

const Xv_pkg xv_icon_pkg = {
    "Icon",			/* seal -> package name */
    (Attr_pkg) ATTR_PKG_ICON,	/* icon attr */
    sizeof(Xv_icon),		/* size of the icon data struct */
    WINDOW,		/* pointer to parent */
    icon_init,			/* init routine for icon */
    icon_set_internal,		/* set routine */
    icon_get_internal,		/* get routine */
    icon_destroy_internal,	/* destroy routine */
    NULL
};
