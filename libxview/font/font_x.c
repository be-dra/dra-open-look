#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)font_x.c 20.33 93/06/28 DRA: RCS $Id: font_x.c,v 4.1 2024/03/28 17:55:25 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */

#include <stdio.h>
#include <sys/types.h>
#include <xview/pkg.h>
#include <X11/Xlib.h>
#include <pixrect/pixrect.h>
#include <xview/font.h>		/* for FONT_INFO */
#include <xview_private/font_impl.h>
#include <xview_private/pw_impl.h>
#ifdef OW_I18N
#include <locale.h>
#include <string.h>
#endif /*OW_I18N*/

/*
 * PERFORMANCE BUG!  [Space traded for time] glyph_pixmap/_gc/_pr are left
 * around for possible re-use.
 */
 /* static Display *glyph_display; */	/* = 0 for -A-R */
 /* static Pixmap   glyph_pixmap; */	/* = 0 for -A-R */
 /* static GC       glyph_gc; */	/* = 0 for -A-R */
 /* static int      gp_height, gp_width; */	/* = 0 for -A-R */
 /* static Pixrect *glyph_pr; */

#ifdef OW_I18N
Pkg_private  XFontSet
xv_load_font_set(dpy, locale, fs_list)
    Display             *dpy;
    char                *locale;
    char                **fs_list;
{
#define		TEMP_NAME_BUF_SIZE	1024
    char                save_locale[125];
    XFontSet            font_set = NULL;
    char                **miss_list = NULL;
    char		*font_name_list;
    int			missing_charset_count;
    char		*def_string;
    char                **temp_fs_list, *temp_list;
    int			str_len = 0;
    char		temp_name_buf[TEMP_NAME_BUF_SIZE];

    if (fs_list == NULL)
       return(NULL);

    /*
     * XCreateFontSet() looks at LC_CTYPE locale to find
     * the charsets corresponding to the codesets.
     */
    strcpy(save_locale, setlocale(LC_CTYPE, (char *)NULL));

    setlocale(LC_CTYPE, (char *)locale);

    temp_fs_list = fs_list;
    while ( *fs_list) {
        str_len += (strlen(*fs_list) + 1);
        *fs_list++;
    }
    fs_list = temp_fs_list;

    font_name_list = temp_list = (str_len > TEMP_NAME_BUF_SIZE) ?
    				      (char *)malloc(str_len + 1) :
    				      temp_name_buf;
    font_name_list[0] = NULL;
    for (;;) {
        strcat(font_name_list, *fs_list);
        *fs_list++;
        if (!*fs_list)
           break;
        temp_list = font_name_list + strlen(font_name_list);
        *temp_list++ = ',';
        *temp_list = NULL;
    }
   font_set = XCreateFontSet(dpy, font_name_list, &miss_list, &missing_charset_count, &def_string);

   if ((font_name_list) && (font_name_list != temp_name_buf))
       free(font_name_list);

    setlocale(LC_CTYPE, (char *)save_locale);

    if (miss_list && (missing_charset_count > 0))
        XFreeStringList(miss_list);

    if ((missing_charset_count > 0) && font_set) {
        XFreeFontSet(dpy, font_set);
        font_set = NULL;
    }
#undef	TEMP_NAME_BUF_SIZE
    return(font_set);
}
#endif /*OW_I18N*/


Pkg_private XID xv_load_x_font(Display *display, char *name, Xv_opaque *font_opaque, int *default_x, int *default_y, int *max_char, int *min_char)
{
    register XFontStruct *font;

#ifdef _XV_DEBUG
#define ERROR	abort()
#else
#define ERROR	goto Error_Return
#endif

    font = XLoadQueryFont(display, name);
    *font_opaque = (Xv_opaque) font;
    if (font) {
	/* Extract the global information from the font */
	/*
	 * default_x = font->max_bounds.lbearing + font->max_bounds.rbearing;
	 */
	*default_x = font->max_bounds.width;
	*default_y = font->ascent + font->descent;
	/* Why should we reject fonts with more than one row? I guess row 0
	 * being there is the only thing we depend on; if there are more rows,
	 * we simply ignore them. With this change, we're able to run on
	 * X-servers that have unicode fonts (aka iso10646-1 encoding) in their
	 * font path in front of the iso8859-1 ones.
	 *
	 * mbuck@debian.org
	 */
	if (font->min_byte1)
		ERROR;
	*max_char = MIN(255, font->max_char_or_byte2);	/* pixfont compat */
	*min_char = MIN(255, font->min_char_or_byte2);	/* pixfont compat */
	return (font->fid);
    } else {
Error_Return:
	if (font) {
	    XFreeFont(display, font);
	}
	return (None);
    }
#undef ERROR
}


Xv_private void xv_x_char_info(XFontStruct *font, int i, int *x_home, int *y_home, int *x_advance, int *y_advance, Pixrect **pr)
/* Caller must guarantee that i is a valid character index for this font. */
{
    register XCharStruct *per_char;

    per_char = (font->per_char) ? &(font->per_char[i]) : &font->max_bounds;
#ifdef XV_DEBUG_OLD
    if (per_char->width != per_char->rbearing || per_char->descent != 0) {
	printf("xv_pf_textbound is screwed up! Tell someone!\n");
    }
#endif				/* XV_DEBUG_OLD */
    *x_advance = per_char->width;
    *y_advance = 0;
    *x_home = per_char->lbearing;
    *y_home = -font->ascent;
    /*
     * PERFORMANCE/BUG:  We are creating a 0-width 0-height memory pixrect,
     * just so pr->pr_height/width can hold the correct size of the
     * character.  This is done because some old SunView1 library code
     * reaches into the height and width of a pixrect representing a
     * character.  It's too much pain at this point to try to rewrite that
     * part of textsw and ttysw, so we have this disgusting hack.  Note that
     * this increases the size of data segment by 20 bytes per character in
     * the font.
     */
    *pr = xv_mem_create(0, 0, 1);
    (*pr)->pr_height = font->ascent + font->descent;
    (*pr)->pr_width = per_char->width;
    /*
     * (*pr)->pr_width = per_char->rbearing - per_char->lbearing;
     */
}

Xv_public void xv_real_baseline_when_using_pf(Xv_opaque font, int ch, int *x_x, int *x_y, int pr_x, int pr_y)
{
    XCharStruct    *per_char;
    XFontStruct    *x_font;

    x_font = (XFontStruct *) xv_get(font, FONT_INFO);
    per_char = (x_font->per_char) ? &(x_font->per_char[ch])
	: &(x_font->max_bounds);
    *x_x = pr_x;
    *x_y = pr_y + per_char->ascent;
}

Pkg_private void xv_unload_x_font(Display *display, Xv_opaque font_opaque)
{
    XFreeFont(display, (XFontStruct *) font_opaque);
}
