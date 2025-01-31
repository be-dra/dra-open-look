#ifndef lint
char     txt_getkey_c_sccsid[] = "@(#)txt_getkey.c 20.36 93/06/29 DRA: $Id: txt_getkey.c,v 4.16 2025/01/30 08:50:14 dra Exp $";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * GET key processing.
 */

#include <xview/pkg.h>
#include <xview/attrol.h>
#include <xview_private/primal.h>
#include <xview_private/txt_impl.h>
#include <xview_private/svr_impl.h>
#include <xview_private/ev_impl.h>	/* For declaration of ev_add_finder */
#include <errno.h>

Pkg_private void ev_remove_finger(Ev_finger_table *fingers, Ev_mark_object  mark);

Pkg_private     Es_index textsw_read_only_boundary_is_at(Textsw_private priv)
{
    register Es_index result;

    if (EV_MARK_IS_NULL(&priv->read_only_boundary)) {
	result = 0;
    } else {
	result = textsw_find_mark_internal(priv,
					   priv->read_only_boundary);
	if AN_ERROR
	    (result == ES_INFINITY)
		result = 0;
    }
    return (result);
}

Pkg_private     Es_index textsw_insert_pieces(Textsw_view_private view, Es_index pos, Es_handle       pieces)
{
	register Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
	register Ev_chain chain = priv->views;
	Es_index old_insert_pos = 0, old_length = es_get_length(chain->esh),
			new_insert_pos, temp;
	int delta;

	if (pieces == ES_NULL)
		return (pos);
	if (priv->notify_level & TEXTSW_NOTIFY_EDIT)
		old_insert_pos = EV_GET_INSERT(chain);
	EV_SET_INSERT(chain, pos, temp);
	/* Required since es_set(ES_HANDLE_TO_INSERT) bypasses ev code. */
	es_set(chain->esh, ES_HANDLE_TO_INSERT, pieces, NULL);
	new_insert_pos = es_get_position(chain->esh);
	(void)textsw_set_insert(priv, new_insert_pos);
	delta = new_insert_pos - pos;
	/*
	 * The esh may simply swallow the pieces (in the cmdsw), so check to see
	 * if any change actually occurred.
	 */
	if (delta) {
		ev_update_after_edit(chain, pos, delta, old_length, pos);
		if (priv->notify_level & TEXTSW_NOTIFY_EDIT) {
			textsw_notify_replaced(VIEW_PRIVATE(xv_get(XV_PUBLIC(priv),
							OPENWIN_NTH_VIEW, 0)),
					old_insert_pos, old_length, pos, pos, delta);
		}
		textsw_checkpoint(priv);
	}
	return (new_insert_pos);
}

/*
 * ===============================================================
 * 
 * Misc. marking utilities
 * 
 * ===============================================================
 */
Pkg_private Ev_mark_object textsw_add_mark_internal(Textsw_private textsw, Es_index position, unsigned flags)
{
    Ev_mark_object  mark;
    register Ev_mark mark_to_use;

    if (flags & TEXTSW_MARK_READ_ONLY) {
	mark_to_use = &textsw->read_only_boundary;
	textsw_remove_mark_internal(textsw, *mark_to_use);
    } else {
	mark_to_use = &mark;
    }
    EV_INIT_MARK(*mark_to_use);
    if (flags & TEXTSW_MARK_MOVE_AT_INSERT)
	EV_MARK_SET_MOVE_AT_INSERT(*mark_to_use);
    ev_add_finger(&textsw->views->fingers, position, 0, mark_to_use);
    return (*mark_to_use);
}

Xv_public          Textsw_mark
#ifdef OW_I18N
textsw_add_mark_wc(abstract, position, flags)
    Textsw          abstract;
    Textsw_index        position;
    unsigned        flags;
#else
textsw_add_mark(Textsw abstract, Textsw_index position, unsigned flags)
#endif
{
    Textsw_private priv;

	/* hier kann TEXTSW oder TERMSW kommen */
	if (xv_get(abstract, XV_IS_SUBTYPE_OF, OPENWIN)) {
		/* so will ich das */
		priv = TEXTSW_PRIVATE(abstract);
	}
	else {
    	Textsw_view_private view = VIEW_ABS_TO_REP(abstract);
    	priv = TSWPRIV_FOR_VIEWPRIV(view);
		abort();
	}

    return ((Textsw_mark) textsw_add_mark_internal(priv, position, flags));
}

#ifdef OW_I18N
Xv_public          Textsw_mark
textsw_add_mark(abstract, position, flags)
    Textsw          abstract;
    Es_index        position;
    unsigned        flags;
{
    Textsw_view_private view = VIEW_ABS_TO_REP(abstract);
    Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);

    return ((Textsw_mark) textsw_add_mark_internal(priv,
			      textsw_wcpos_from_mbpos(priv, position), flags));
}
#endif /* OW_I18N */

Pkg_private Es_index textsw_find_mark_internal(Textsw_private textsw, Ev_mark_object  mark)
{
    Ev_finger_handle finger;

    finger = ev_find_finger(&textsw->views->fingers, mark);
    return (finger ? finger->pos : ES_INFINITY);
}

Xv_public          Textsw_index
#ifdef OW_I18N
textsw_find_mark_wc(abstract, mark)
#else
textsw_find_mark(abstract, mark)
#endif
    Textsw          abstract;
    Textsw_mark     mark;
{
    Ev_mark_object *dummy_for_compiler = (Ev_mark_object *) & mark;
    Textsw_private priv;

	/* hier kann TEXTSW oder TERMSW kommen */
	if (xv_get(abstract, XV_IS_SUBTYPE_OF, OPENWIN)) {
		/* so will ich das */
		priv = TEXTSW_PRIVATE(abstract);
	}
	else {
    	Textsw_view_private view = VIEW_ABS_TO_REP(abstract);
    	priv = TSWPRIV_FOR_VIEWPRIV(view);
		abort();
	}

    return (Textsw_index)textsw_find_mark_internal(priv, *dummy_for_compiler);
}

#ifdef OW_I18N
Xv_public          Textsw_index
textsw_find_mark(abstract, mark)
    Textsw          abstract;
    Textsw_mark     mark;
{
    Textsw_view_private view = VIEW_ABS_TO_REP(abstract);
    Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
    Ev_mark_object *dummy_for_compiler = (Ev_mark_object *) & mark;

#ifdef	lint
    view->magic = *dummy_for_compiler;	/* To get rid of unused msg */
    return ((Textsw_index) 0);
#else	/* lint */
    return ((Textsw_index) textsw_mbpos_from_wcpos(priv,
		textsw_find_mark_internal(priv, *dummy_for_compiler)));
#endif	/* lint */
}
#endif /* OW_I18N */

Pkg_private void textsw_remove_mark_internal(Textsw_private textsw, Ev_mark_object mark)
{
    if (!EV_MARK_IS_NULL(&mark)) {
	if (EV_MARK_ID(mark) == EV_MARK_ID(textsw->read_only_boundary)) {
	    EV_INIT_MARK(textsw->read_only_boundary);
	}
	ev_remove_finger(&textsw->views->fingers, mark);
    }
}

Xv_public void textsw_remove_mark(Textsw abstract, Textsw_mark mark)
{
    long unsigned  *dummy_for_compiler = (long unsigned *) &mark;
    Textsw_private priv;

	/* hier kann TEXTSW oder TERMSW kommen */
	if (xv_get(abstract, XV_IS_SUBTYPE_OF, OPENWIN)) {
		/* so will ich das */
		priv = TEXTSW_PRIVATE(abstract);
	}
	else {
    	Textsw_view_private view = VIEW_ABS_TO_REP(abstract);
    	priv = TSWPRIV_FOR_VIEWPRIV(view);
		abort();
	}

    textsw_remove_mark_internal(priv, *((Ev_mark) dummy_for_compiler));
}
