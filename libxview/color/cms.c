#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)cms.c 1.17 91/03/18 DRA: RCS $Id: cms.c,v 2.3 2026/02/09 14:25:53 dra Exp $";
#endif
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <xview_private/i18n_impl.h>
#include <xview_private/scrn_vis.h>
#include <xview/cms.h>
#include <xview/server.h>
#include <xview/screen.h>
#include <xview/defaults.h>
#include <olgx/olgx.h>

/*
 *      (c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *      pending in the U.S. and foreign countries. See LEGAL_NOTICE
 *      file for terms of the license.
 */

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

#define XV_CMS_IS_STATIC(cms)	(cms->type == XV_STATIC_CMS) ? TRUE : FALSE
#define XV_CMS_IS_DEFAULT(cms)	strcmp(cms->name, XV_DEFAULT_CMS) ? FALSE : TRUE
#define XV_DYNAMIC_VISUAL(class) ((class) % 2)

/*
 *	cms_free_colors() frees all the colors in the colormap that 
 *	have been allocated for this colormap segment.
 */
static void cms_free_colors(Display *display, Cms_info	*cms)
{
	register int i;

	if ((cms->index_table == NULL) || (cms->cmap == NULL)) {
		return;
	}

	for (i = 0; i <= cms->size - 1; i++) {
		if (cms->index_table[i] != XV_INVALID_PIXEL) {
			XFreeColors(display, cms->cmap->id, &cms->index_table[i], 1, 0L);
			cms->index_table[i] = XV_INVALID_PIXEL;
		}
	}
}

/*
 *	cms_set_name() sets the secified name for the cms.
 */
static void cms_set_name(Cms_info *cms, char	*name)
{
	if (cms->name != NULL) {
		xv_free(cms->name);
		cms->name = NULL;
	}

	cms->name = (char *)xv_malloc(strlen(name) + 1);
	strcpy(cms->name, name);
}


/* 
 *	cms_set_unique_name() generates & sets a unique name for the cms.
 */
static void cms_set_unique_name(Cms_info *cms)
{
	char buf[20];

	if (cms->name != NULL) {
		xv_free(cms->name);
	}

	sprintf(buf, "%lx", (Xv_opaque) cms);
	cms->name = (char *)xv_malloc(strlen("xv_cms_") + strlen(buf) + 1);
	sprintf(cms->name, "xv_cms_%s", buf);
}

/* 
 * cms_get_colors() returns the colors either as an array of Xv_Singlecolor,
 * as an array of XColors, or as an array of red, green and blue colors.
 */
static int cms_get_colors(Cms_info *cms, unsigned long cms_index,
				unsigned long cms_count, Xv_Singlecolor *cms_colors,
				XColor *cms_x_colors,
				unsigned char *red, unsigned char *green, unsigned char *blue)
{
	register int i;
	XColor *xcolors = NULL;
	Xv_opaque server;
	Display *display;
	unsigned long valid_pixel = XV_INVALID_PIXEL;

	/* 
	 * Check to see if atleast one cell has been allocated among the 
	 * ones being retrieved.
	 */
	for (i = 0; i <= cms_count - 1; i++) {
		if (cms->index_table[cms_index + i] != XV_INVALID_PIXEL) {
			valid_pixel = cms->index_table[cms_index + i];
			break;
		}
	}

	/* none of the pixels being retrieved have been allocated */
	if (valid_pixel == XV_INVALID_PIXEL) {
		return (XV_ERROR);
	}

	server = xv_get(cms->screen, SCREEN_SERVER);
	display = (Display *) xv_get(server, XV_DISPLAY);

	if (!cms_x_colors) {
		if ((xcolors = (XColor *) xv_malloc(cms_count * sizeof(XColor))) ==
				NULL)
			return (XV_ERROR);
	}
	else {
		xcolors = cms_x_colors;
	}

	for (i = 0; i <= cms_count - 1; i++) {
		if (cms->index_table[cms_index + i] != XV_INVALID_PIXEL)
			xcolors[i].pixel = cms->index_table[cms_index + i];
		else
			xcolors[i].pixel = valid_pixel;
	}
	XQueryColors(display, cms->cmap->id, xcolors, (int)cms_count);

	if (cms_colors) {
		for (i = 0; i <= cms_count - 1; i++) {
			cms_colors[i].red = xcolors[i].red >> 8;
			cms_colors[i].green = xcolors[i].green >> 8;
			cms_colors[i].blue = xcolors[i].blue >> 8;
		}
	}
	else if (!cms_x_colors) {
		for (i = 0; i <= cms_count - 1; i++) {
			red[i] = xcolors[i].red >> 8;
			green[i] = xcolors[i].green >> 8;
			blue[i] = xcolors[i].blue >> 8;
		}
	}

	if (xcolors && (xcolors != cms_x_colors))
		xv_free(xcolors);

	return (XV_OK);
}

static int cms_alloc_static_colors(Display *display, Cms_info *cms,
				Xv_Colormap *cmap, XColor *xcolors, unsigned long cms_index,
				unsigned long cms_count)
{
	unsigned long *pixel;
	register int i;

	if (xcolors == NULL)
		return (XV_OK);

	for (i = 0; i <= cms_count - 1; i++) {
		pixel = &cms->index_table[cms_index + i];

		/* static cms pixels are write-once only */
		if (*pixel == XV_INVALID_PIXEL) {
			if (!XAllocColor(display, cmap->id, xcolors + i)) {
				return (XV_ERROR);
			}
			*pixel = (xcolors + i)->pixel;
		}
	}

	return (XV_OK);
}

static Xv_Colormap *cms_allocate_colormap(Display *display, Cms_info *cms)	
{
	Xv_Colormap *cmap;
	int screen_num = (int)xv_get(cms->screen, SCREEN_NUMBER);

	cmap = xv_alloc(Xv_Colormap);
	if (STATUS(cms, default_cms) &&
			(cms->visual->vinfo->visualid ==
					XVisualIDFromVisual(DefaultVisual(display, screen_num))))
		cmap->id = DefaultColormap(display, screen_num);
	else
		cmap->id = XCreateColormap(display,
				RootWindow(display, screen_num),
				cms->visual->vinfo->visual, AllocNone);
	cmap->type = XV_DYNAMIC_VISUAL(cms->visual->vinfo->class) ?
			XV_DYNAMIC_CMAP : XV_STATIC_CMAP;
	cmap->cms_list = cms;
	cmap->next = (Xv_Colormap *) NULL;
	return (cmap);
}

static int cms_set_static_colors(Display *display, Cms_info *cms, XColor *xcolors, unsigned long cms_index, unsigned long cms_count)
{
	int status;
	Xv_Colormap *cmap_list, *cmap = NULL;

	/* 
	 * Always attempt to allocate read-only colors from the default 
	 * colormap. If the allocation fails, try any other colormaps 
	 * that have been previously created. If that fails allocate a 
	 * new colormap and allocate the read-only cells from it.
	 */
	if (cms->cmap == NULL) {
		cmap_list = (Xv_Colormap *) cms->visual->colormaps;

		for (cmap = cmap_list; cmap != NULL; cmap = cmap->next) {
			status = cms_alloc_static_colors(display, cms, cmap, xcolors,
					cms_index, cms_count);
			if (status == XV_ERROR) {
				cms->cmap = cmap;
				cms_free_colors(display, cms);
				cms->cmap = NULL;
			}
			else {
				cms->cmap = cmap;
				cms->next = cmap->cms_list;
				cmap->cms_list = cms;
				break;
			}
		}

		if (cmap == NULL) {
			/* could not use any of the currently available colormaps */
			cmap = cms_allocate_colormap(display, cms);
			cms->cmap = cmap;

			/*
			 * Add colormap to the screen visual's colormap list. The default 
			 * colormap is always the first element in the colormap list.
			 */
			cmap->next = cmap_list->next;
			cmap_list->next = cmap;

			status = cms_alloc_static_colors(display, cms, cmap, xcolors,
					cms_index, cms_count);
			if (status == XV_ERROR) {
				cms_free_colors(display, cms);
				cms->cmap = NULL;
			}
		}
	}
	else {
		status = cms_alloc_static_colors(display, cms, cms->cmap,
				xcolors, cms_index, cms_count);
	}

	return (status);
}

static int cms_set_dynamic_colors(Display *display, Cms_info *cms, XColor *xcolors, unsigned long cms_index, unsigned long cms_count)
{
	Xv_Colormap *cmap_list, *cmap = NULL;
	register int i;

	/* Search for an appropriate colormap to allocate colors from */
	if (cms->cmap == NULL) {
		cmap_list = (Xv_Colormap *) cms->visual->colormaps;
		for (cmap = cmap_list; cmap != NULL; cmap = cmap->next) {
			if (XAllocColorCells(display, cmap->id, True, NULL,
							0, cms->index_table, cms->size))
				break;
		}

		/* Couldn't find one, so create a new colormap for allocation */
		if (cmap == NULL) {
			cmap = cms_allocate_colormap(display, cms);
			cms->cmap = cmap;

			if (!XAllocColorCells(display, cmap->id, True, NULL,
							0, cms->index_table, cms->size)) {
				xv_free(cmap);
				return (XV_ERROR);
			}

			/*
			 * Add colormap to the screen's colormap list. The default colormap  
			 * is always the first element in the colormap list.
			 */
			cmap->next = cmap_list->next;
			cmap_list->next = cmap;
		}
		else {
			cms->cmap = cmap;
			cms->next = cmap->cms_list;
			cmap->cms_list = cms;
		}
	}

	if (xcolors != NULL) {
		for (i = 0; i <= cms_count - 1; i++) {
			(xcolors + i)->pixel = cms->index_table[cms_index + i];
		}
		XStoreColors(display, cms->cmap->id, xcolors, (int)cms_count);
	}

	return (XV_OK);
}

static int cms_set_colors(Cms_info *cms, Xv_Singlecolor *cms_colors, XColor *cms_x_colors, unsigned long cms_index, unsigned long cms_count)
{
	register int i;
	XColor *xcolors = NULL;
	Xv_opaque server;
	Display *display;
	int status;

	if (cms->index_table == NULL) {
		return (XV_ERROR);
	}

	server = xv_get(cms->screen, SCREEN_SERVER);
	display = (Display *) xv_get(server, XV_DISPLAY);

	if (cms_colors) {
		xcolors = xv_alloc_n(XColor, cms_count);
		for (i = 0; i <= cms_count - 1; i++) {
			(xcolors + i)->red = (unsigned short)(cms_colors + i)->red << 8;
			(xcolors + i)->green = (unsigned short)(cms_colors + i)->green << 8;
			(xcolors + i)->blue = (unsigned short)(cms_colors + i)->blue << 8;
			(xcolors + i)->flags = DoRed | DoGreen | DoBlue;
		}
	}
	else if (cms_x_colors) {
		xcolors = cms_x_colors;
	}

	if (cms->type == XV_STATIC_CMS) {
		status = cms_set_static_colors(display, cms, xcolors, cms_index,
				cms_count);
	}
	else {
		status = cms_set_dynamic_colors(display, cms, xcolors, cms_index,
				cms_count);
	}

	if (xcolors != cms_x_colors) {
		xv_free(xcolors);
	}

	return (status);
}


static XColor *cms_parse_named_colors(Cms_info *cms, char **named_colors)
{
	XColor *xcolors;
	int count = 0;
	Display *display;
	int screen_num;

	if ((named_colors == NULL) || (*named_colors == NULL))
		return (NULL);

	while (named_colors[count])
		++count;

	xcolors = (XColor *) xv_malloc(count * sizeof(XColor));

	display =
			(Display *) xv_get(xv_get(cms->screen, SCREEN_SERVER), XV_DISPLAY);
	screen_num = (int)xv_get(cms->screen, SCREEN_NUMBER);

	for (--count; count >= 0; --count) {
		if (!XParseColor(display, DefaultColormap(display, screen_num),
						named_colors[count], xcolors + count)) {
			xv_error(XV_NULL, ERROR_STRING,
					XV_MSG("Unable to find RGB values for a named color"),
					ERROR_PKG, CMS, NULL);
			return (NULL);
		}
	}

	return (xcolors);
}

Xv_private Xv_opaque cms_default_colormap(Xv_Server server, Display	*display,
							int screen_number, XVisualInfo *vinfo);

Xv_private Xv_opaque cms_default_colormap(Xv_Server server, Display	*display,
							int screen_number, XVisualInfo *vinfo)
{
	Bool found_one = FALSE;
	Xv_Colormap *colormap;
	XStandardColormap *std_colormaps;
	int num_cmaps, c;

	colormap = xv_alloc(Xv_Colormap);
	colormap->type =
			XV_DYNAMIC_VISUAL(vinfo->class) ? XV_DYNAMIC_CMAP : XV_STATIC_CMAP;
	colormap->cms_list = (Cms_info *) NULL;
	colormap->next = (Xv_Colormap *) NULL;

	/* Check to see if the screen's default visual matches the
	 * server's default visual
	 */
	if (vinfo->visualid == XVisualIDFromVisual(DefaultVisual(display,
							screen_number))) {
		colormap->id = DefaultColormap(display, screen_number);
		found_one = TRUE;
	}
	else if (colormap->type == XV_DYNAMIC_CMAP) {
		/* Check to see if there is a standard colormap already allocated */
		if (XGetRGBColormaps(display, RootWindow(display, screen_number),
						&std_colormaps, &num_cmaps, XA_RGB_DEFAULT_MAP)) {
			if (num_cmaps) {
				c = 0;
				while ((std_colormaps[c].visualid != vinfo->visualid)
						&& (c < num_cmaps))
					c++;
				if (c < num_cmaps) {
					colormap->id = std_colormaps[c].colormap;
					found_one = TRUE;
				}
			}
		}
	}
	if (!found_one)
		colormap->id =
				XCreateColormap(display, RootWindow(display, screen_number),
				vinfo->visual, AllocNone);

	return ((Xv_opaque) colormap);
}

static int cms_init(Xv_Screen parent, Cms cms_public, Attr_avlist avlist, int *dummy)
{
	Cms_info *cms = NULL;
	Xv_cms_struct *cms_object;
	register Attr_avlist attrs;
	register int i;
	XVisualInfo template;
	long mask = 0;

	cms = (Cms_info *) xv_alloc(Cms_info);
	cms->public_self = cms_public;
	cms_object = (Xv_cms_struct *) cms_public;
	cms_object->private_data = (Xv_opaque) cms;

	cms->size = 0;
	cms->screen = parent ? parent : xv_default_screen;
	cms->type = XV_STATIC_CMS;
	cms->visual = (Screen_visual *) xv_get(cms->screen, SCREEN_DEFAULT_VISUAL);

	for (attrs = avlist; *attrs; attrs = attr_next(attrs)) {
		switch ((int)attrs[0]) {
			case CMS_TYPE:
				cms->type = (Cms_type) attrs[1];
				ATTR_CONSUME(attrs[0]);
				break;

			case CMS_DEFAULT_CMS:
				if (attrs[1]) {
					STATUS_SET(cms, default_cms);
				}
				else {
					STATUS_RESET(cms, default_cms);
				}
				ATTR_CONSUME(attrs[0]);
				break;

			case CMS_CONTROL_CMS:
				if (attrs[1]) {
					STATUS_SET(cms, control_cms);
				}
				else {
					STATUS_RESET(cms, control_cms);
				}
				ATTR_CONSUME(attrs[0]);
				break;

			case CMS_SIZE:
				if (attrs[1] != 0)
					cms->size = (unsigned)attrs[1];
				ATTR_CONSUME(attrs[0]);
				break;

			case XV_VISUAL_CLASS:
				template.class = (int)attrs[1];
				mask |= VisualClassMask;
				ATTR_CONSUME(attrs[0]);
				break;

			case XV_DEPTH:
				template.depth = (int)attrs[1];
				mask |= VisualDepthMask;
				ATTR_CONSUME(attrs[0]);
				break;

			case XV_VISUAL:
				if (attrs[1]) {
					template.visualid =
							XVisualIDFromVisual((Visual *) attrs[1]);
					mask |= VisualIDMask;
				}
				break;

			default:
				break;
		}
	}

	if (mask) {
		Screen_visual *visual;

		visual = (Screen_visual *) xv_get(cms->screen, SCREEN_VISUAL, mask,
				&template);
		if (visual)
			cms->visual = visual;
	}

	/* 
	 * Check to see if they are trying to create a dynamic cms from a 
	 * static visual.
	 */
	if (cms->type == XV_DYNAMIC_CMS
			&& !XV_DYNAMIC_VISUAL(cms->visual->vinfo->class))
	{
		xv_error(XV_NULL, ERROR_STRING,
			XV_MSG("Can not allocate a read/write cms from a static visual"),
			ERROR_PKG, CMS,
			NULL);
		return XV_ERROR;
	}
	else {
		if (cms->size == 0)
			cms->size =
					STATUS(cms,
					control_cms) ? CMS_CONTROL_COLORS : XV_DEFAULT_CMS_SIZE;
		cms->index_table = xv_alloc_n(unsigned long, (size_t)cms->size);

		if (cms->type == XV_STATIC_CMS) {
			for (i = 0; i <= cms->size - 1; i++) {
				cms->index_table[i] = XV_INVALID_PIXEL;
			}
		}
	}
	return (XV_OK);
}

static Xv_opaque cms_set_avlist(Cms cms_public, Attr_attribute avlist[])
{
	register Cms_info *cms = CMS_PRIVATE(cms_public);
	register Attr_avlist attrs;
	unsigned long cms_index, cms_count;
	Xv_Singlecolor *colors = NULL;
	XColor *xcolors = NULL;
	char **named_colors = NULL;

	/* defaults */
	if (STATUS(cms, control_cms)) {
		cms_index = CMS_CONTROL_COLORS;
		cms_count = cms->size - CMS_CONTROL_COLORS;
	}
	else {
		cms_index = 0;
		cms_count = cms->size;
	}

	for (attrs = avlist; *attrs; attrs = attr_next(attrs)) switch (attrs[0]) {
		case CMS_NAME:
			cms_set_name(cms, (char *)attrs[1]);
			ATTR_CONSUME(attrs[0]);
			break;

		case CMS_COLORS:
			colors = (Xv_Singlecolor *) attrs[1];
			ATTR_CONSUME(attrs[0]);
			break;

		case CMS_X_COLORS:
			xcolors = (XColor *) attrs[1];
			ATTR_CONSUME(attrs[0]);
			break;

		case CMS_NAMED_COLORS:
			named_colors = (char **)&attrs[1];
			break;

		case CMS_INDEX:
			cms_index = (int)attrs[1];
			ATTR_CONSUME(attrs[0]);
			break;

		case CMS_COLOR_COUNT:
			cms_count = (int)attrs[1];
			ATTR_CONSUME(attrs[0]);
			break;

		case CMS_TYPE:
			xv_error(XV_NULL,
					ERROR_STRING, XV_MSG("CMS_TYPE is a create-only attribute"),
					ERROR_PKG, CMS,
					NULL);
			return ((Xv_opaque) XV_ERROR);

		case CMS_FRAME_CMS:
			if (attrs[1]) {
				STATUS_SET(cms, frame_cms);
			}
			else {
				STATUS_RESET(cms, frame_cms);
			}
			ATTR_CONSUME(attrs[0]);
			break;

		case XV_END_CREATE:
			if (cms->name == NULL) {
				cms_set_unique_name(cms);
			}

			if (STATUS(cms, control_cms)) {
				int i;
				char *control_color;
				XColor *xcolors = NULL;
				Display *display;
				Colormap cmap;
				screen_ui_style_t ui_style;

				/*
				 * Get the control color from OpenWindows.WindowColor.
				 * Index   color   
				 *   0     control color         (BG1 80% grey by default)
				 *   1     90% of control color. (BG2)
				 *   2     50% of control color  (BG3)
				 *   3     120% of control color (WHITE)
				 *        Note: the white is not the white defined
				 *              by Open Look, but it looks better,
				 *              and is consistent with the way olwm
				 *              handles it.
				 */

				xcolors = xv_alloc_n(XColor, (size_t)4);
				control_color = (char *)
						defaults_get_string("openWindows.windowColor",
						"OpenWindows.WindowColor", "#cccccc");

				display = (Display *)
						xv_get(xv_get(cms->screen, SCREEN_SERVER, 0),
						XV_DISPLAY, 0);

				/* Can't use the cms->cmap, because it may not be allocated */
				cmap = DefaultColormap(display,
						(int)xv_get(cms->screen, SCREEN_NUMBER, 0));
				if (!XParseColor(display, cmap,
								control_color, &(xcolors[0]))) {
					xv_error(XV_NULL,
							ERROR_STRING, "Unable to parse window color",
							ERROR_PKG, CMS,
							NULL);
					xcolors[CMS_CONTROL_BG1].red = 0xcccc;
					xcolors[CMS_CONTROL_BG1].green = 0xcccc;
					xcolors[CMS_CONTROL_BG1].blue = 0xcccc;
				}

				ui_style = (screen_ui_style_t)xv_get(cms->screen,
													SCREEN_UI_STYLE);

				if (ui_style == SCREEN_UIS_2D_COLOR) {
					xcolors[CMS_CONTROL_BG2] = xcolors[CMS_CONTROL_BG1];
					xcolors[CMS_CONTROL_BG3] = xcolors[CMS_CONTROL_BG1];
					xcolors[CMS_CONTROL_HIGHLIGHT] = xcolors[CMS_CONTROL_BG1];
				}
				else {
					olgx_calculate_3Dcolors((XColor *) NULL,
							&(xcolors[CMS_CONTROL_BG1]),
							&(xcolors[CMS_CONTROL_BG2]),
							&(xcolors[CMS_CONTROL_BG3]),
							&(xcolors[CMS_CONTROL_HIGHLIGHT]));
				}

				for (i = CMS_CONTROL_BG1; i <= CMS_CONTROL_HIGHLIGHT; i++)
					xcolors[i].flags = (DoRed | DoGreen | DoBlue);

				if (cms_set_colors(cms, (Xv_Singlecolor *) NULL, xcolors,
								(unsigned long)0,
								(unsigned long)CMS_CONTROL_COLORS) ==
						XV_ERROR) {
					xv_error(XV_NULL, ERROR_STRING,
							XV_MSG("Unable to allocate control colors for colormap segment"),
							ERROR_PKG, CMS,
							NULL);
					xv_free(xcolors);
					return ((Xv_opaque) XV_ERROR);
				}
				xv_free(xcolors);
				xcolors = NULL;
			}

			if (cms->cmap == NULL) {
				/*
				 * Colors for this cms were not specified as part 
				 * of the create. Allocate enough cells for this 
				 * colormap segment however and bind it to a 
				 * X11 colormap.
				 */
				cms_set_colors(cms, NULL, NULL, 0L, (unsigned long)cms->size);
			}
			break;

		default:
			xv_check_bad_attr(CMS, attrs[0]);
			break;
	}

	if (named_colors) {
		xcolors = cms_parse_named_colors(cms, named_colors);
	}

	if (colors || xcolors) {
		if (cms_set_colors(cms, colors, xcolors, cms_index, cms_count)
				== XV_ERROR) {
			return ((Xv_opaque) XV_ERROR);
		}
	}

	if (named_colors && xcolors) {
		/* free up memory that was allocated for ASCII to RGB conversion */
		xv_free(xcolors);
	}

	return ((Xv_opaque) XV_OK);
}


static Xv_opaque cms_get_attr(Cms cms_public, int *status,
								Attr_attribute attr, va_list args)
{
	Xv_singlecolor *singcol;
	XColor *xcol;

	Cms_info *cms = CMS_PRIVATE(cms_public);
	Xv_opaque value;
	int cms_status = 0;

	switch (attr) {
		case CMS_PIXEL:{
				unsigned long index;

				index = va_arg(args, unsigned long);
				if (index >= cms->size) {
					index = cms->size - 1;
				}
				else if (index < 0) {
					index = 0;
				}
				value = (Xv_opaque) cms->index_table[index];
				break;
			}

		case CMS_SIZE:
			value = (Xv_opaque) cms->size;
			break;

		case CMS_STATUS_BITS:
			if (STATUS(cms, default_cms)) {
				cms_status = STATUS(cms, default_cms) << CMS_STATUS_DEFAULT;
			}
			else if (STATUS(cms, control_cms)) {
				cms_status = STATUS(cms, control_cms) << CMS_STATUS_CONTROL;
			}
			else if (STATUS(cms, frame_cms)) {
				cms_status = STATUS(cms, frame_cms) << CMS_STATUS_FRAME;
			}
			value = (Xv_opaque) cms_status;
			break;

		case CMS_INDEX_TABLE:
			value = (Xv_opaque) cms->index_table;
			break;

		case CMS_CONTROL_CMS:
			value = STATUS(cms, control_cms);
			break;

		case CMS_DEFAULT_CMS:
			value = STATUS(cms, default_cms);
			break;

		case CMS_NAME:
			value = (Xv_opaque) cms->name;
			break;

		case CMS_TYPE:
			value = (Xv_opaque) cms->type;
			break;

		case CMS_FOREGROUND_PIXEL:
			value = (Xv_opaque) cms->index_table[cms->size - 1];
			break;

		case CMS_BACKGROUND_PIXEL:
			value = (Xv_opaque) cms->index_table[0];
			break;

		case CMS_SCREEN:
			value = (Xv_opaque) cms->screen;
			break;

		case XV_VISUAL:
			value = (Xv_opaque) cms->visual->vinfo->visual;
			break;

		case XV_VISUAL_CLASS:
			value = (Xv_opaque) cms->visual->vinfo->class;
			break;

		case XV_DEPTH:
			value = (Xv_opaque) cms->visual->vinfo->depth;
			break;

		case XV_XID:	/* public */
		case CMS_CMAP_ID:	/* private that should be removed */
			value = (Xv_opaque) cms->cmap->id;
			break;

		case CMS_COLORS:
			singcol = va_arg(args, Xv_singlecolor *);
			if (cms_get_colors(cms, (unsigned long)0, (unsigned long)cms->size,
							singcol, (XColor *) NULL,
							(unsigned char *)NULL, (unsigned char *)NULL,
							(unsigned char *)NULL) == XV_OK) {
				value = (Xv_opaque) singcol;
			}
			else {
				value = XV_NULL;
			}
			break;

		case CMS_X_COLORS:
			xcol = va_arg(args, XColor *);
			if (cms_get_colors(cms, (unsigned long)0, (unsigned long)cms->size,
							(Xv_singlecolor *) NULL, xcol,
							(unsigned char *)NULL, (unsigned char *)NULL,
							(unsigned char *)NULL) == XV_OK) {
				value = (Xv_opaque) xcol;
			}
			else {
				value = XV_NULL;
			}
			break;

		case CMS_CMS_DATA:
			{
				Xv_cmsdata *cms_data = va_arg(args, Xv_cmsdata *);
				cms_data->type = cms->type;
				cms_data->size = cms->size;
				cms_data->index = 0;
				cms_data->rgb_count = cms->size;
				cms_get_colors(cms, (unsigned long)0, (unsigned long)cms->size,
						(Xv_singlecolor *) NULL, (XColor *) NULL, cms_data->red,
						cms_data->green, cms_data->blue);
				value = (Xv_opaque) cms_data;
				break;
			}

		case CMS_FRAME_CMS:
			value = STATUS(cms, frame_cms);
			break;

		case CMS_CMAP_TYPE:
			value = (Xv_opaque) cms->cmap->type;
			break;

		default:
			value = XV_NULL;
			if (xv_check_bad_attr(CMS, attr) == XV_ERROR) {
				*status = XV_ERROR;
			}
	}

	return value;
}

/* ARGSUSED */
static Xv_object cms_find_cms(Xv_opaque screen_public, const Xv_pkg *pkg,
							Attr_avlist avlist)
{
	Xv_Screen screen;
	register Attr_avlist attrs;
	Xv_Colormap *cmap;
	Cms_info *cms;
	Screen_visual *visual;

	if (!screen_public)
		screen = xv_default_screen;
	else
		screen = screen_public;

	for (attrs = avlist; *attrs; attrs = attr_next(attrs)) {
		switch ((int)attrs[0]) {
			case CMS_NAME:
				visual = (Screen_visual *) xv_get(screen,
						SCREEN_DEFAULT_VISUAL);
				while (visual != (Screen_visual *) NULL) {
					for (cmap = (Xv_Colormap *) visual->colormaps;
							cmap != NULL; cmap = cmap->next) {
						for (cms =cmap->cms_list; cms != NULL; cms=cms->next) {
							if (!strcmp(cms->name, (char *)attrs[1]))
								return (CMS_PUBLIC(cms));
						}
					}
					visual = visual->next;
				}
				break;

			default:
				break;
		}
	}
	return XV_NULL;
}

static int cms_destroy(Cms cms_public, Destroy_status status)
{
	Cms_info *cms = CMS_PRIVATE(cms_public);
	Cms_info *cms_list;
	Xv_Colormap *cmap_list;

	/* default cms cannot be freed until the application is terminated */
	if (STATUS(cms, default_cms))
		return (XV_OK);

	if (status == DESTROY_CLEANUP) {
		Xv_opaque server;
		Display *display;

		server = xv_get(cms->screen, SCREEN_SERVER);
		display = (Display *) xv_get(server, XV_DISPLAY);

		xv_free(cms->name);
		cms_free_colors(display, cms);
		xv_free(cms->index_table);

		/* remove from colormap's cms list */
		if (cms == cms->cmap->cms_list) {
			cms->cmap->cms_list = cms->next;
		}
		else {
			for (cms_list = cms->cmap->cms_list; cms_list->next != NULL;
					cms_list = cms_list->next) {
				if (cms_list->next == cms) {
					cms_list->next = cms_list->next->next;
					break;
				}
			}
		}

		/* check if colormap is to be freed */
		if (cms->cmap->cms_list == NULL) {
			XFreeColormap(display, cms->cmap->id);

			cmap_list = (Xv_Colormap *) cms->visual->colormaps;

			if (cms->cmap == cmap_list) {
				cms->visual->colormaps = (Xv_opaque) cms->cmap->next;
			}
			else {
				for (; cmap_list->next != NULL; cmap_list = cmap_list->next) {
					if (cmap_list->next == cms->cmap) {
						cmap_list->next = cmap_list->next->next;
						break;
					}
				}
			}
			xv_free(cms->cmap);
		}

		xv_free(cms);
	}

	return (XV_OK);
}

const Xv_pkg xv_cms_pkg = {
    "Color", ATTR_PKG_CMS,
    sizeof(Xv_cms_struct),
    XV_GENERIC_OBJECT,
    cms_init,
    cms_set_avlist,
    cms_get_attr,
    cms_destroy,
    cms_find_cms    
};
