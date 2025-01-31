#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)dnd_pblc.c 1.17 93/06/28 DRA: $Id: dnd_pblc.c,v 4.8 2025/01/07 18:39:52 dra Exp $ ";
#endif
#endif

/*
 *      (c) Copyright 1990 Sun Microsystems, Inc. Sun design patents
 *      pending in the U.S. and foreign countries. See LEGAL NOTICE
 *      file for terms of the license.
 */

#include <X11/Xatom.h>
#include <xview/xview.h>
#include <xview/notify.h>
#include <xview/dragdrop.h>
#include <xview_private/dndimpl.h>
#include <xview_private/draw_impl.h>
#include <xview_private/portable.h>
#ifdef SVR4
#include <stdlib.h>
#endif  /* SVR4 */

#define ADONE ATTR_CONSUME(*attrs);break

static int dnd_init(Xv_Window parent, Xv_drag_drop dnd_public,
									Attr_avlist avlist, int *u)
{
    Dnd_info			*dnd = NULL;
    Xv_dnd_struct		*dnd_object;
    Xv_opaque server;

    dnd = (Dnd_info *)xv_alloc(Dnd_info);
    dnd->public_self = dnd_public;
    dnd_object = (Xv_dnd_struct *)dnd_public;
    dnd_object->private_data = (Xv_opaque)dnd;

    dnd->parent = parent ? parent : xv_get(xv_default_screen, XV_ROOT);
    server = XV_SERVER_FROM_WINDOW(dnd->parent);

    dnd->atom[TRIGGER] = (Atom)xv_get(server,
				        			SERVER_ATOM, "_SUN_DRAGDROP_TRIGGER");
    dnd->atom[PREVIEW] = (Atom)xv_get(server,
									SERVER_ATOM, "_SUN_DRAGDROP_PREVIEW");
    dnd->atom[ACK] = (Atom)xv_get(server, SERVER_ATOM, "_SUN_DRAGDROP_ACK");
    dnd->atom[WMSTATE] = (Atom)xv_get(server, SERVER_ATOM, "WM_STATE");
    dnd->atom[INTEREST] =(Atom)xv_get(server,
									SERVER_ATOM, "_SUN_DRAGDROP_INTEREST");
    dnd->atom[DSDM] = (Atom)xv_get(server, SERVER_ATOM, "_SUN_DRAGDROP_DSDM");
#ifdef NO_XDND
#else /* NO_XDND */
	dnd->atom[XdndAware] = (Atom)xv_get(server, SERVER_ATOM, "XdndAware");
	dnd->atom[XdndSelection] = (Atom)xv_get(server, SERVER_ATOM, "XdndSelection");
	dnd->atom[XdndEnter] = (Atom)xv_get(server, SERVER_ATOM, "XdndEnter");
	dnd->atom[XdndLeave] = (Atom)xv_get(server, SERVER_ATOM, "XdndLeave");
	dnd->atom[XdndPosition] = (Atom)xv_get(server, SERVER_ATOM, "XdndPosition");
	dnd->atom[XdndDrop] = (Atom)xv_get(server, SERVER_ATOM, "XdndDrop");
	dnd->atom[XdndFinished] = (Atom)xv_get(server, SERVER_ATOM, "XdndFinished");
	dnd->atom[XdndStatus] = (Atom)xv_get(server, SERVER_ATOM, "XdndStatus");
	dnd->atom[XdndActionCopy] = (Atom)xv_get(server, SERVER_ATOM, "XdndActionCopy");
	dnd->atom[XdndActionMove] = (Atom)xv_get(server, SERVER_ATOM, "XdndActionMove");
	dnd->atom[XdndActionLink] = (Atom)xv_get(server, SERVER_ATOM, "XdndActionLink");
	dnd->atom[XdndActionAsk] = (Atom)xv_get(server, SERVER_ATOM, "XdndActionAsk");
	dnd->atom[XdndActionPrivate] = (Atom)xv_get(server, SERVER_ATOM, "XdndActionPrivate");
	dnd->atom[XdndTypeList] = (Atom)xv_get(server, SERVER_ATOM, "XdndTypeList");
	dnd->atom[XdndActionList] = (Atom)xv_get(server, SERVER_ATOM, "XdndActionList");
	dnd->atom[XdndActionDescription] = (Atom)xv_get(server, SERVER_ATOM, "XdndActionDescription");
#endif /* NO_XDND */

    dnd->type = DND_MOVE;

    dnd->timeout.tv_sec = xv_get(DND_PUBLIC(dnd), SEL_TIMEOUT_VALUE);
    dnd->timeout.tv_usec = 0;

    return XV_OK;
}

static Xv_opaque dnd_set_avlist(Dnd dnd_public, Attr_attribute *avlist)
{
	Dnd_info *dnd = DND_PRIVATE(dnd_public);
	Attr_avlist attrs;

	for (attrs = avlist; *attrs; attrs = attr_next(attrs)) {
		switch (attrs[0]) {
			case DND_TYPE:
				dnd->type = (DndDragType) attrs[1];
				ADONE;
			case DND_CURSOR:
				dnd->cursor = (Xv_opaque) attrs[1];
				ADONE;
			case DND_X_CURSOR:
				dnd->xCursor = (Cursor) attrs[1];
				ADONE;
			case DND_ACCEPT_CURSOR:
				dnd->affCursor = (Xv_opaque) attrs[1];
				ADONE;
			case DND_ACCEPT_X_CURSOR:
				dnd->affXCursor = (Cursor) attrs[1];
				ADONE;
			case DND_REJECT_CURSOR:
				dnd->rejectCursor = (Xv_opaque) attrs[1];
				ADONE;
			case DND_REJECT_X_CURSOR:
				dnd->rejectXCursor = (Cursor) attrs[1];
				ADONE;
			case DND_TIMEOUT_VALUE:
				XV_BCOPY((struct timeval *)attrs[1], &(dnd->timeout),
						sizeof(struct timeval));
				ADONE;
#ifdef NO_XDND
#else /* NO_XDND */
			case SEL_OWN:
				{
					int val;
					val = (int)attrs[1];
					if (! val) {
						/* about to give up ownership, maybe as a consequence
						 * of _SUN_DRAGDROP_DONE.
						 */
						if (dnd->xdnd_owner)
							xv_set(dnd->xdnd_owner, SEL_OWN, FALSE, NULL);
					}
				}
				/* do not consume it */
				break;
#endif /* NO_XDND */
			case XV_END_CREATE:
				break;
			default:
				(void)xv_check_bad_attr(&xv_dnd_pkg, attrs[0]);
				break;
		}
	}

	return ((Xv_opaque) XV_OK);
}

static Xv_opaque dnd_get_attr(Dnd dnd_public, int *status,
									Attr_attribute attr, va_list args)
{
	Dnd_info *dnd = DND_PRIVATE(dnd_public);
	Xv_opaque value = 0;

	switch (attr) {
		case DND_TYPE:
			value = (Xv_opaque) dnd->type;
			break;
		case DND_CURSOR:
			value = (Xv_opaque) dnd->cursor;
			break;
		case DND_X_CURSOR:
			value = (Xv_opaque) dnd->xCursor;
			break;
		case DND_ACCEPT_CURSOR:
			value = (Xv_opaque) dnd->affCursor;
			break;
		case DND_ACCEPT_X_CURSOR:
			value = (Xv_opaque) dnd->affXCursor;
			break;
		case DND_REJECT_CURSOR:
			value = (Xv_opaque) dnd->rejectCursor;
			break;
		case DND_REJECT_X_CURSOR:
			value = (Xv_opaque) dnd->rejectXCursor;
			break;
		case DND_TIMEOUT_VALUE:
			value = (Xv_opaque) & dnd->timeout;
			break;
		default:
			if (xv_check_bad_attr(&xv_dnd_pkg, attr) == XV_ERROR)
				*status = XV_ERROR;
			break;
	}

	return (value);
}

static int dnd_destroy(Dnd dnd_public, Destroy_status status)
{
	Dnd_info *dnd = DND_PRIVATE(dnd_public);

	if (status == DESTROY_CLEANUP) {
		if (dnd->dsdm_selreq)
			xv_destroy(dnd->dsdm_selreq);
		if (dnd->window)
			xv_destroy(dnd->window);

#ifdef NO_XDND
#else /* NO_XDND */
		if (dnd->xdnd_owner)
			xv_destroy(dnd->xdnd_owner);
		if (dnd->tl_cache) xv_free(dnd->tl_cache);
#endif /* NO_XDND */

		if (dnd->siteRects) {
			xv_free(dnd->siteRects);
			/* It is possible that the dnd object will be destroyed before
			 * dnd_send_drop() returns.
			 */
			dnd->siteRects = NULL;
		}

		xv_free(dnd);
	}

	return (XV_OK);
}

Xv_pkg xv_dnd_pkg = {
    "Drag & Drop", ATTR_PKG_DND,
    sizeof(Xv_dnd_struct),
    &xv_sel_owner_pkg,
    dnd_init,
    dnd_set_avlist,
    dnd_get_attr,
    dnd_destroy,
    NULL		/* BUG: Need find */
};
