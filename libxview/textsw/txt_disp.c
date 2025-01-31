#ifndef lint
char     txt_disp_c_sccsid[] = "@(#)txt_disp.c 20.31 93/06/28 DRA: $Id: txt_disp.c,v 4.2 2024/12/23 09:57:31 dra Exp $";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Initialization and finalization of text subwindows.
 */

#include <xview_private/primal.h>
#include <xview_private/txt_impl.h>
#include <xview_private/ev_impl.h>
#include <xview_private/win_info.h>
#include <xview/win_notify.h>
#include <xview/pixwin.h>
#ifdef OW_I18N
#ifdef FULL_R5
#include <xview/frame.h>
#include <X11/Xlib.h>
#endif /* FULL_R5 */    
#endif /* OW_I18N */    


/* Used as hack to communicate between textsw_display and textsw_display_view
 * to establish who should manage the caret. */
static	int textsw_display_parent;

Pkg_private void textsw_display(Textsw abstract)
{
    Textsw_private    priv = TEXTSW_PRIVATE(abstract);
	Textsw_view vp;

    textsw_hide_caret(priv);
    textsw_display_parent = 1;
    priv->state |= TXTSW_DISPLAYED;
	OPENWIN_EACH_VIEW(abstract, vp)
    	Textsw_view_private view = VIEW_PRIVATE(vp);
		textsw_display_view(vp, &view->rect);
	OPENWIN_END_EACH
    textsw_show_caret(priv);
    textsw_display_parent = 0;
}

Pkg_private void textsw_display_view(Textsw_view v, Rect *rect)
{
    Textsw_view_private view;

	if (xv_get(v, XV_IS_SUBTYPE_OF, OPENWIN_VIEW)) {
		view = VIEW_PRIVATE(v);
	}
	else {
    	view = VIEW_ABS_TO_REP(v);
	}

	if (!textsw_display_parent)
		textsw_hide_caret(TSWPRIV_FOR_VIEWPRIV(view));
	textsw_display_view_margins(view, rect);
	if (rect == 0) {
		rect = &view->rect;
	}
	else if (!rect_intersectsrect(rect, &view->rect)) {
		return;
	}
	ev_display_in_rect(view->e_view, rect);
	textsw_update_scrollbars(TSWPRIV_FOR_VIEWPRIV(view), view);
	if (!textsw_display_parent)
		textsw_show_caret(TSWPRIV_FOR_VIEWPRIV(view));

}

Pkg_private void textsw_display_view_margins(Textsw_view_private view, struct rect *rect)
{
    struct rect     margin;

    margin = view->e_view->rect;
    margin.r_left -= (
	       margin.r_width = (int) ev_get(view->e_view, EV_LEFT_MARGIN, XV_NULL, XV_NULL, XV_NULL));
    /*if (rect == 0) {   || rect_intersectsrect(rect, &margin)) {*/
    /* Always write so will clear up cursor droppings */
    (void) pw_writebackground(PIXWIN_FOR_VIEW(view),
				  margin.r_left, margin.r_top,
				  margin.r_width, margin.r_height,
				  PIX_CLR);
    margin.r_left = rect_right(&view->e_view->rect) + 1;
    margin.r_width = (int) ev_get(view->e_view, EV_RIGHT_MARGIN, XV_NULL, XV_NULL, XV_NULL);
    if (rect == 0 || rect_intersectsrect(rect, &margin)) {
	(void) pw_writebackground(PIXWIN_FOR_VIEW(view),
				  margin.r_left, margin.r_top,
				  margin.r_width, margin.r_height,
				  PIX_CLR);
    }
}

Pkg_private void textsw_repaint(Textsw_view_private view)
{
    if (!(view->view_state & TXTSW_VIEW_DISPLAYED)) {
	view->view_state |= TXTSW_VIEW_DISPLAYED;
	view->view_state |= TXTSW_UPDATE_SCROLLBAR;
    }
    TSWPRIV_FOR_VIEWPRIV(view)->state |= TXTSW_DISPLAYED;

    (EV_PRIVATE(view->e_view))->state |= EV_VS_SET_CLIPPING;
    textsw_display_view(VIEW_PUBLIC(view), &view->rect);
}

Pkg_private void textsw_resize(Textsw_view_private view)
{

#ifdef OW_I18N
#ifdef FULL_R5
    Textsw_private    priv = TSWPRIV_FOR_VIEWPRIV(view);
#endif /* FULL_R5 */    
#endif /* OW_I18N */    

    win_getsize(VIEW_PUBLIC(view), &view->rect);

    /* Cannot trust the x and y from openwin */
    view->rect.r_left = view->e_view->rect.r_left;
    view->rect.r_width -= view->rect.r_left;
    view->rect.r_top = view->e_view->rect.r_top;
    view->rect.r_height -= view->rect.r_top;
    ev_set(view->e_view, EV_RECT, &view->rect, NULL);
	if (view->drop_site) {
    	xv_set(view->drop_site,
					DROP_SITE_DELETE_REGION, NULL,
			    	DROP_SITE_REGION, &view->rect,
			    	NULL);
	}
#ifdef OW_I18N			    
#ifdef FULL_R5
    if (priv->ic) {
	XRectangle	x_rect;
	XVaNestedList   preedit_nested_list;
    	    
	preedit_nested_list = NULL;
    	    
	if  (priv->xim_style & (XIMPreeditPosition | XIMPreeditArea)) {
	    x_rect.x = view->rect.r_left;
	    x_rect.y = view->rect.r_top;
	    x_rect.width = view->rect.r_width;
	    x_rect.height = view->rect.r_height;
      	
            preedit_nested_list = XVaCreateNestedList(NULL, 
					     XNArea, &x_rect, 
					     NULL);
	}
        
	if (preedit_nested_list) {
	    XSetICValues(priv->ic, XNPreeditAttributes, preedit_nested_list, 
        	     NULL);
            XFree(preedit_nested_list);
	}
    }
#endif /* FULL_R5 */
#endif /* OW_I18N */    
	           			    
}

Pkg_private void textsw_do_resize(Textsw_view abstract)
/* This routine only exists for the cmdsw. */
{
	/* ich sah Termsw_view */
    register Textsw_view_private view = VIEW_ABS_TO_REP(abstract);

    textsw_resize(view);
}

Pkg_private Textsw_expand_status textsw_expand(Textsw abstract,
		Textsw_index start, /* Entity to start expanding at */
		Textsw_index stop_plus_one, /* 1st ent not expanded */
		CHAR *out_buf,
		int out_buf_len, int *total_chars)
/*
 * Expand the contents of the textsw from first to stop_plus_one into the set
 * of characters used to paint them, placing the expanded text into out_buf,
 * returning the number of character placed into out_buf in total_chars, and
 * returning status.
 */
{
	Ev_expand_status status;
    Textsw_view_private view;

	/* das wird anscheinend nur aus ttysw/ttyansi.c aufgerufen -
	 * und zwar mit einem Termsw.
	 * Mal wieder: view splitting ? - nie gehoert
	 */
	if (xv_get(abstract, XV_IS_SUBTYPE_OF, OPENWIN)) {
		view = VIEW_PRIVATE(xv_get(abstract, OPENWIN_NTH_VIEW, 0));
	}
	else {
    	view = VIEW_ABS_TO_REP(abstract);
	}

	status = ev_expand(view->e_view,
			start, stop_plus_one, out_buf, out_buf_len, total_chars);
	switch (status) {
		case EV_EXPAND_OKAY:
			return TEXTSW_EXPAND_OK;
		case EV_EXPAND_FULL_BUF:
			return TEXTSW_EXPAND_FULL_BUF;
		case EV_EXPAND_OTHER_ERROR:
		default:
			return TEXTSW_EXPAND_OTHER_ERROR;
	}
}
