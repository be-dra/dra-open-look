#ifndef lint
char     txt_view_c_sccsid[] = "@(#)txt_view.c 1.32 93/06/28 DRA: $Id: txt_view.c,v 4.6 2025/03/08 13:15:23 dra Exp $";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

#include <xview/pkg.h>
#include <xview/attrol.h>
#include <xview/defaults.h>
#include <xview_private/primal.h>
#include <xview_private/txt_impl.h>
#include <xview_private/ev_impl.h>
#include <xview_private/win_info.h>
#include <xview_private/windowimpl.h>
#include <xview/textsw.h>
#include <xview/win_struct.h>
#include <xview/window.h>
#include <xview/text.h>

#define SET_NEW_START(_view, _char_pos) _view->e_view->line_table.seq[0]= (_char_pos == TEXTSW_INFINITY) ? 0 : _char_pos

static int textsw_view_init(Textsw parent, Textsw_view textsw_view_public,
    								Attr_attribute  *avlist, int *unused)
{
	Attr_avlist attrs;
	Textsw_view_private view = (struct textsw_view_object *)calloc(1L,
											sizeof(struct textsw_view_object));
	Textsw_status dummy_status;
	Textsw_status *status = &dummy_status;
	Xv_textsw_view *view_object = (Xv_textsw_view *) textsw_view_public;

	if (!text_notice_key) {
		text_notice_key = xv_unique_key();
	}

	for (attrs = avlist; *attrs; attrs = attr_next(attrs)) {
		switch (*attrs) {
			case TEXTSW_STATUS:
				status = (Textsw_status *) attrs[1];
				break;
			default:
				break;
		}
	}

	if (!view) {
		*status = TEXTSW_STATUS_CANNOT_ALLOCATE;
		return (XV_ERROR);
	}
	/* link to object */
	view_object->private_data = (Xv_opaque) view;
	view->public_self = textsw_view_public;
	view->magic = TEXTSW_VIEW_MAGIC;
	/*
	 * Must initialize rect here else code elsewhere (e.g., textsw_resize)
	 * that tries to compute incremental changes gets the wrong answer.
	 */
	win_getsize(textsw_view_public, &view->rect);

	view->textsw_priv = TEXTSW_PRIVATE(parent);

	view = textsw_view_init_internal(view, status);

	if (!view) {
		return (XV_ERROR);
	}
	if (xv_get(parent, OPENWIN_PW_CLASS)) {
		xv_set(textsw_view_public,
				XV_HELP_DATA, textsw_make_help(parent, "textsw"),
				XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
				NULL);

		/* view->drop_site will be created as child of the paint window */
	}
	else {
		xv_set(textsw_view_public,
				WIN_NOTIFY_SAFE_EVENT_PROC, textsw_view_event,
				WIN_NOTIFY_IMMEDIATE_EVENT_PROC, textsw_view_event,
				XV_HELP_DATA, textsw_make_help(parent, "textsw"),
				XV_KEY_DATA_REMOVE_PROC, XV_HELP, textsw_free_help,
				NULL);

		view->drop_site = xv_create(VIEW_PUBLIC(view), DROP_SITE_ITEM,
				DROP_SITE_REGION, &view->rect,
				NULL);
	}

	/* Grab the keys that can be used to start a ``quick'' selection.  We
	 * need to grab them in order for quick selection to work in follow-
	 * mouse mode.
	 */
	win_grab_quick_sel_keys(VIEW_PUBLIC(view));

	return (XV_OK);
}

Pkg_private	void textsw_split_init_proc(Textsw_view oldview,
									Textsw_view newview, int position)
{
	Textsw_view_private oldviewpriv = VIEW_PRIVATE(oldview);
	Textsw_view_private newviewpriv = VIEW_PRIVATE(newview);
	Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(oldviewpriv);
	int line_pos, char_pos;
	int top, bottom;

	line_pos = ev_line_for_y(oldviewpriv->e_view, position);
	if (line_pos == oldviewpriv->e_view->line_table.last_plus_one)
		line_pos--;
	char_pos = ev_index_for_line(oldviewpriv->e_view, line_pos);
	SET_NEW_START(newviewpriv, char_pos);
	if (priv->notify_level & TEXTSW_NOTIFY_SPLIT_VIEW)
		textsw_notify(oldviewpriv, TEXTSW_ACTION_SPLIT_VIEW, newview, NULL);


	/* wieso war das eigentlich nötig ???? supertoll ist es nicht ... */
	textsw_file_lines_visible(oldview, &top, &bottom);
	xv_set(newview,
			TEXTSW_FIRST, textsw_index_for_file_line(TEXTSW_PUBLIC(priv),
														bottom+1),
			NULL);

	if (priv->orig_split_init_proc) {
		(*(priv->orig_split_init_proc))(oldview, newview, position);
	}
}

const Xv_pkg xv_textsw_view_pkg = {
    "Textsw_view",
    (Attr_pkg) ATTR_PKG_TEXTSW_VIEW,
    sizeof(Xv_textsw_view),
    &xv_openwin_view_pkg,
    textsw_view_init,
    textsw_view_set,
    textsw_view_get,
    textsw_view_destroy,
    NULL			/* no find proc */
};
