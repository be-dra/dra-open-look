/*
 *#ident "@(#)olgx_impl.h	1.20 93/06/28 SMI"
 */

/* 
 * Copyright 1990 Sun Microsystems
 */

/*
 * OPEN LOOK object drawing package
 */

#ifndef OL_PRIVATE_DEFINED
#define OL_PRIVATE_DEFINED

#ifdef OW_I18N
/*
 * I18N_Portability: May need to change the following #include to
 * pickup the wchar_t and X11R5(-ish) Xlib functions definitions.
 */
#include <widec.h>
#include <X11/Xlib.h>
#if XlibSpecificationRelease != 5
#include <X11/XlibR5.h>
#endif /* XlibSpecificationRelease != 5 */


#endif
#include <olgx/olgx.h>

#define STRING_SIZE 		128     /* max size of a glyph font string */

#define VARHEIGHT_BUTTON_CORNER_DIMEN  7


#define False                   0
#define True                    1


/*
 * OPEN LOOK constant definitions
 */


/*
 * Macro definitions
 */
#define VARIABLE_LENGTH_MACRO(start_pos, offset)		\
	for (i = 0; i < num_add; i++) {				\
		string[start_pos+i] = offset + add_ins[i];	\
	}

#ifdef OW_I18N
#define	textfontset	utextfont.fontset
#define	textfont	utextfont.fontstruct
#endif

typedef struct _per_disp_res_rec {
  Display * dpy;
  int screen;
  GC_rec * gc_list_ptr;
  Pixmap   busy_stipple;
  Pixmap   grey_stipple;
  struct _per_disp_res_rec * next;
} per_disp_res_rec, *per_disp_res_ptr;

/*
 * Definitions used by the color calculation code 
 */
#define	XRGB	0xffff
#define	MAXRGB	0xff
#define	MAXH	360
#define	MAXSV	1000

#define VMUL		12	/* brighten by 20% (12 = 1.2*10) */
#define SDIV		2	/* unsaturate by 50% (divide by 2) */
#define VMIN		400	/* minimum highlight brightness of 40% */

typedef struct {
    int         r,
                g,
                b;
}           RGB;

typedef struct {
    int         h,
                s,
                v;
}           HSV;

/*
 * Private function declarations
 */

extern int calc_add_ins(int width, short *add_ins);
extern char *olgx_malloc(unsigned int nbytes);
extern void olgx_update_horizontal_slider(Graphics_info  *info, Window win,
    int x, int y, int width, int old_value, int new_value, int state);
extern void olgx_update_vertical_slider(Graphics_info  *info, Window win,
    int x, int y, int height, int old_value, int new_value, int state);
extern void olgx_update_vertical_gauge(Graphics_info *info, Window win, int x,
					int y, int width, int oldval, int newval);
extern void olgx_update_horiz_gauge(Graphics_info  *info, Window win, int x,
					int y, int oldval, int newval);
extern void               olgx_free(void);
extern void olgx_destroy_gcrec(per_disp_res_ptr perdisp_res_ptr, GC_rec *gcrec);
extern void olgx_total_gcs(Display *dpy, int screen);
extern void olgx_initialise_gcrec(Graphics_info  *info, int index);
extern void olgx_draw_elevator(Graphics_info *info, Window win, int x, int y,
							int state);
extern void olgx_error(const char *string);
extern void olgx_draw_pixmap_label(Graphics_info *info, Window win, Pixmap pix, int x, int y, int width, int height, int state);
extern void olgx_draw_varheight_button(Graphics_info  *info, Window win, int x,
								int y, int width, int height, int state);
extern Pixmap olgx_get_busy_stipple(per_disp_res_ptr perdispl_res_ptr);
extern Pixmap olgx_get_grey_stipple(per_disp_res_ptr perdispl_res_ptr);
extern int gc_matches(GC_rec *GCrec, unsigned long valuemask, XGCValues *values);
extern int olgx_cmp_fonts(XFontStruct *font_info1, XFontStruct *font_info2);
extern GC_rec *olgx_get_gcrec(per_disp_res_ptr perdispl_res_ptr, Drawable drawable, int depth, unsigned long valuemask, XGCValues *values);
extern GC_rec *olgx_gcrec_available(per_disp_res_ptr perdispl_res_ptr,
						unsigned long valuemask, XGCValues *values);
extern GC_rec *olgx_set_color_smart(Graphics_info *info, per_disp_res_ptr perdispl_res_ptr, GC_rec *gcrec, int fg_flag, unsigned long pixval, int flag);
extern Graphics_info    * olgx_create_ginfo(void);
extern per_disp_res_ptr olgx_get_perdisplay_list(Display *dpy, int screen);

/* ol_color.c */
extern void hsv_to_rgb(HSV *hsv, RGB *rgb);
extern void rgb_to_hsv(RGB *rgb, HSV *hsv);
extern void rgb_to_xcolor(RGB *r, XColor *x);
extern void hsv_to_xcolor(HSV *h, XColor *x);
extern void xcolor_to_hsv(XColor *x, HSV *h);
extern void olgx_hsv_to_3D(HSV *bg1, XColor *bg2, XColor *bg3, XColor *white);

#endif	/* !OL_PRIVATE_DEFINED */
