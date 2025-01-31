#ifndef lint
char     text_c_sccsid[] = "@(#)text.c 20.31 93/06/28 DRA: $Id: text.c,v 4.13 2024/12/24 22:18:49 dra Exp $";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

#include <xview/pkg.h>
#include <xview/attrol.h>
#include <xview_private/primal.h>
#include <xview_private/txt_impl.h>
#include <xview/textsw.h>
#include <xview/defaults.h>

/* Pkg_private */
int	text_notice_key = 0;


static int textsw_init(Xv_Window parent, Textsw textsw_public,
						Attr_attribute *avlist, int *unused)
{
	Attr_avlist attrs;
	Textsw_status dummy_status;
	Textsw_status *status = &dummy_status;
	Xv_textsw *textsw_object = (Xv_textsw *) textsw_public;
	Textsw_private priv = NEW(struct textsw_object);

	strcpy(priv->backup_pattern, "%s%%");
	if (!text_notice_key) {
		text_notice_key = xv_unique_key();
	}

	for (attrs = avlist; *attrs; attrs = attr_next(attrs)) {
		switch (*attrs) {
			case TEXTSW_STATUS:
				status = (Textsw_status *) attrs[1];
				break;
			case TEXTSW_RESTRICT_MENU:
				priv->restrict_menu = attrs[1];
				ATTR_CONSUME(*attrs);
				break;
			case TEXTSW_RESEARCH_PROC:
				priv->research = (textsw_research_proc_t)attrs[1];
				ATTR_CONSUME(*attrs);
				break;
			default:
				break;
		}
	}

	if (!priv) {
		*status = TEXTSW_STATUS_CANNOT_ALLOCATE;
		return (XV_ERROR);
	}
	/* link to object */
	textsw_object->private_data = (Xv_opaque) priv;
	priv->public_self = textsw_public;

	priv = textsw_init_internal(priv, status, textsw_default_notify, avlist);
	textsw_new_selection_init(textsw_public);

	/*
	 * BUG: Note the priv is not really initialized until the first view is
	 * created.
	 */
	return (priv ? XV_OK : XV_ERROR);
}


Xv_pkg          xv_textsw_pkg = {
    "Textsw",
    (Attr_pkg) ATTR_PKG_TEXTSW,
    sizeof(Xv_textsw),
    &xv_openwin_pkg,
    textsw_init,
    textsw_set,
    textsw_get,
    textsw_destroy,
    NULL			/* no find proc */
};
