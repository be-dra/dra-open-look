#ifndef lint
char     txt_dbx_c_sccsid[] = "@(#)txt_dbx.c 20.26 93/06/28 DRA: $Id: txt_dbx.c,v 4.5 2025/03/16 13:37:28 dra Exp $";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Support routines for dbx's use of text subwindows.
 * These routines are only supported for dbxtool use.
 */

#include <xview/pkg.h>
#include <xview/attrol.h>
#include <xview/font.h>
#include <xview_private/primal.h>
#include <xview_private/txt_impl.h>
#include <xview_private/ev_impl.h>
#include <pixrect/pixfont.h>
#include <assert.h>

#include <xview/termsw.h>

extern char *xv_app_name;

/* das liefert ein Textsw_view !!!!!  */
Xv_public Textsw textsw_first(Textsw any)
{
	/* use this and I'll kill you */
	/* es ist absolut nicht klar, was das sein soll - mach das lieber
	 * ueber OPENWIN
	 */
	fprintf(stderr, "fatal: use textsw_first and die...\n");
	abort();
}

Xv_public Textsw textsw_next(Textsw previous)
{
	/* use this and I'll kill you */
	abort();
	/* es ist absolut nicht klar, was das sein soll - mach das lieber
	 * ueber OPENWIN
	 */
}

Pkg_private int textsw_does_index_not_show(Textsw_view vpub, Es_index index, int *line_index)	/* output only.  if index does not show, not set. */
{
    Rect            rect;
    int             dummy_line_index;
	Textsw_view_private view;

	if (xv_get(vpub, XV_IS_SUBTYPE_OF, OPENWIN_VIEW)) {
		view = VIEW_PRIVATE(vpub);
	}
	else {
		Xv_pkg *pkg;

		pkg = (Xv_pkg *)xv_get(vpub, XV_TYPE);
		fprintf(stderr, "%s`%s-%d: param is a %s\n", __FILE__,
							__FUNCTION__, __LINE__, pkg->name);
		view = VIEW_ABS_TO_REP(vpub);
		abort();
	}

    if (!line_index)
	line_index = &dummy_line_index;
    switch (ev_xy_in_view(view->e_view, index, line_index, &rect)) {
      case EV_XY_VISIBLE:
	return (0);
      case EV_XY_RIGHT:
	return (0);
      case EV_XY_BELOW:
	return (1);
      case EV_XY_ABOVE:
	return (-1);
      default:			/* should never happen */
	return (-1);
    }
}

static char *pkgname(Xv_object o)
{
	Xv_pkg *pkg = (Xv_pkg *)xv_get(o, XV_TYPE);

	return pkg->name;
}

/* das kann auch ein Tewrmsw oder Termsw_view sein ! */
Xv_public int textsw_screen_line_count(Textsw_view v)
{
	Textsw_view_private view;

	/* urspruenglich stand hier 
	 * Textsw_view_handle view = VIEW_ABS_TO_REP(abstract);
	 *       = textsw_view_abs_to_rep(abstract)
	 */
if (0 != strcmp(pkgname(v), "Textsw_view")
	&& 0 != strcmp(pkgname(v), "Termsw_view")
) {
/* 	fprintf(stderr, "%s: %s-%d: called with %s\n", xv_app_name, __FUNCTION__, __LINE__, pkgname(v)); */
}
	if (xv_get(v, XV_IS_SUBTYPE_OF, OPENWIN_VIEW)) {
		view = VIEW_PRIVATE(v);
	}
	else if (xv_get(v, XV_IS_SUBTYPE_OF, OPENWIN)) {
		/* gern auch mal ein Termsw - sehr obskur: man kommt angeblich
		 * von term_ntfy.c`ttysw_text_event-393. Aber da steht ein Aufruf
		 * von xv_tty_new_size ... ????
		 */
		v = xv_get(v, OPENWIN_NTH_VIEW, 0);
/* 	fprintf(stderr, "%s: %s-%d: first view ist a %s\n", xv_app_name, __FUNCTION__, __LINE__, pkgname(v)); */

		view = VIEW_PRIVATE(v);
	}
	else {
		view = VIEW_ABS_TO_REP(v);
		abort();
	}

	if (view->magic != TEXTSW_VIEW_MAGIC) {
		/* assumption: v is a Termsw_view. Then it looks like
		 * struct {
		 *     Xv_textsw_view  parent_data;
		 *     Xv_opaque       private_data;
		 *     Xv_opaque       private_text;
		 *     Xv_opaque       private_tty;
		 * }
		 * But there are a few statements like
		 * obj->parent_data.private_data = obj->private_text;
		 * or
		 * obj->parent_data.private_data = obj->private_tty;
		 * Let's try...
		 */
/* 		view = (Textsw_view_private)((Xv_termsw_view *)(v))->private_tty; */
/* 		view = (Textsw_view_private)((Xv_termsw_view *)(v))->private_text; */
		view = (Textsw_view_private)((Xv_termsw_view *)(v))->private_data;
		if (view->magic != 0xf011affe) { /* siehe termsw.c */
			fprintf(stderr, "%s: %s-%d: view is a %s, but magic is not %x, but %lx\n", xv_app_name, __FUNCTION__, __LINE__, pkgname(v), TEXTSW_VIEW_MAGIC, view->magic);
		}
		return 0;
	}
	if (! view) return 0;
	if (! view->e_view) return 0;

	return view->e_view->line_table.last_plus_one - 1;
}

/* das kann auch ein Termsw_view sein ! */
Pkg_private int textsw_screen_column_count(Textsw_view v)
{
    PIXFONT        *font = (PIXFONT *) xv_get(v, XV_FONT);
    XFontStruct	*x_font_info = (XFontStruct *)xv_get((Xv_opaque)font,FONT_INFO);
    Textsw_view_private view;

if (0 != strcmp(pkgname(v), "Textsw_view")
	&& 0 != strcmp(pkgname(v), "Termsw_view")
) {
/* 	fprintf(stderr, "%s: %s-%d: called with %s\n", xv_app_name, __FUNCTION__, __LINE__, pkgname(v)); */
}
	if (xv_get(v, XV_IS_SUBTYPE_OF, OPENWIN_VIEW)) {
		view = VIEW_PRIVATE(v);
	}
	else if (xv_get(v, XV_IS_SUBTYPE_OF, OPENWIN)) {
		/* was ist mit den andern Views? IDIOTEN   */
		v = xv_get(v, OPENWIN_NTH_VIEW, 0);
/* 	fprintf(stderr, "%s: %s-%d: first view ist a %s\n", xv_app_name, __FUNCTION__, __LINE__, pkgname(v)); */
		view = VIEW_PRIVATE(v);
	}
	/* termsw does funny things if you ask it for XV_IS_SUBTYPE_OF */
#ifdef NONSENSE
	else if (xv_get(v, XV_IS_SUBTYPE_OF, TEXTSW_VIEW)) {
		view = VIEW_PRIVATE(v);
	}
	else /* if (xv_get(v, XV_IS_SUBTYPE_OF, TERMSW_VIEW)) */ {
		view = VIEW_PRIVATE(v);
	}
#endif /* NONSENSE */
	else {
    	view = VIEW_ABS_TO_REP(v);
		abort();
	}

	if (view->magic != TEXTSW_VIEW_MAGIC) {
		view = (Textsw_view_private)((Xv_termsw_view *)(v))->private_data;
		if (view->magic != 0xf011affe) { /* siehe termsw.c */
			fprintf(stderr, "%s: %s-%d: view is a %s, but magic is not %x, but %lx\n", xv_app_name, __FUNCTION__, __LINE__, pkgname(v), TEXTSW_VIEW_MAGIC, view->magic);
		}
		return 0;
	}
	if (! view) return 0;
	if (! view->e_view) return 0;

    if (x_font_info->per_char)  {
        return (view->e_view->rect.r_width / x_font_info->per_char['m' - x_font_info->min_char_or_byte2].width);
    }
    else  {
        return (view->e_view->rect.r_width / x_font_info->min_bounds.width);
    }
}

Xv_public void textsw_file_lines_visible(Textsw_view v, int *top, int *bottom)
{
    Textsw_view_private view;

	assert(xv_get(v, XV_IS_SUBTYPE_OF, OPENWIN_VIEW));
	view = VIEW_PRIVATE(v);

    ev_line_info(view->e_view, top, bottom);
    *top -= 1;
    *bottom -= 1;
}

Pkg_private int textsw_nop_notify(Textsw abstract, Attr_avlist attrs)
{
    return 0;
}

Xv_public Textsw_index
#ifdef OW_I18N
textsw_index_for_file_line_wc(abstract, line)
    Textsw          abstract;
    int             line;
#else
textsw_index_for_file_line(Textsw abstract, int line)
#endif
{
    Es_index result;
    Textsw_private priv;

	/* Textsw or Termsw */
	assert(xv_get(abstract, XV_IS_SUBTYPE_OF, OPENWIN));
	priv = TEXTSW_PRIVATE(abstract);

    result = ev_position_for_physical_line(priv->views, line, 0);
    return (Textsw_index)result;
}

#ifdef OW_I18N
Xv_public          Textsw_index
textsw_index_for_file_line(abstract, line)
    Textsw          abstract;
    int             line;
{
    Textsw_view_private view = VIEW_ABS_TO_REP(abstract);
    Textsw_private    priv = TSWPRIV_FOR_VIEWPRIV(view);
    Es_index        result;

    result = ev_position_for_physical_line(priv->views, line, 0);
    return ((Textsw_index) textsw_mbpos_from_wcpos(priv, result));
}
#endif /* OW_I18N */

/* Following is for compatibility with old client code. */
Pkg_private Textsw_index textsw_position_for_physical_line(Textsw abstract, int physical_line)	/* Note: 1-origin, not 0! */
{
#ifdef OW_I18N
    return (textsw_index_for_file_line_wc(abstract, physical_line - 1));
#else
    return (textsw_index_for_file_line(abstract, physical_line - 1));
#endif
}

Xv_public void textsw_scroll_lines(Textsw_view v, int count)
{
    Textsw_view_private view;

	assert(xv_get(v, XV_IS_SUBTYPE_OF, TEXTSW_VIEW));
	view = VIEW_PRIVATE(v);

    ev_scroll_lines(view->e_view, count, FALSE);
}

Pkg_private Textsw_mark textsw_add_glyph_on_line(Textsw abstract, int line,
			struct pixrect *pr, int op, int offset_x, int offset_y, int flags)
{
    Ev_mark_object  mark;
    Textsw_private priv;

	assert(xv_get(abstract, XV_IS_SUBTYPE_OF, TEXTSW));
	priv = TEXTSW_PRIVATE(abstract);

    if (flags & TEXTSW_GLYPH_DISPLAY) textsw_take_down_caret(priv);

    /* Assume that TEXTSW_ flags == EV_ flags */
    mark = ev_add_glyph_on_line(priv->views, line - 1, pr, op, offset_x,
												offset_y, flags);
    return (Textsw_mark)mark;
}

Pkg_private void textsw_remove_glyph(Textsw abstract, Textsw_mark mark, int flags)
{
    long unsigned  *dummy_for_compiler = (long unsigned *) &mark;
    Textsw_private priv;

	assert(xv_get(abstract, XV_IS_SUBTYPE_OF, TEXTSW));
	priv = TEXTSW_PRIVATE(abstract);

    textsw_take_down_caret(priv);

    ev_remove_glyph(priv->views, *(Ev_mark)dummy_for_compiler, (unsigned)flags);
}

Pkg_private Textsw_index textsw_start_of_display_line(Textsw abstract,
													Textsw_index pos)
{
    register Textsw_view_private view;

	if (xv_get(abstract, XV_IS_SUBTYPE_OF, OPENWIN)) {
		/* was ist mit den andern Views? IDIOTEN   */
		view = VIEW_PRIVATE(xv_get(abstract, OPENWIN_NTH_VIEW, 0));
	}
	else {
    	view = VIEW_ABS_TO_REP(abstract);
		abort();
	}

    return (Textsw_index)ev_display_line_start(view->e_view, pos);
}
