#ifndef lint
#ifdef SCCS
static char     sccsid[] = "@(#)sel_item.c 1.11 91/04/18 DRA: $Id: sel_item.c,v 4.10 2024/12/26 09:54:14 dra Exp $";
#endif
#endif

/*
 *	(c) Copyright 1990 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */

#include <xview/window.h>
#include <xview_private/sel_impl.h>
#ifdef SVR4
#include <stdlib.h>
#endif  /* SVR4 */

static int sel_item_init(Selection_owner parent,
					Selection_item sel_item_public, Attr_avlist avlist, int *u)
{
	Sel_item_info *sel_item;
	Xv_sel_item *sel_item_object = (Xv_sel_item *) sel_item_public;
	Sel_owner_info *sel_owner = SEL_OWNER_PRIVATE(parent);
	XID xid = (XID) xv_get(parent, XV_XID);

	/* Allocate and clear private data */
	sel_item = xv_alloc(Sel_item_info);

	/* Link private and public data */
	sel_item_object->private_data = (Xv_opaque) sel_item;
	sel_item->public_self = sel_item_public;

	/* Append item to end of Selection_owner (== parent) item list */
	if (!sel_owner->first_item) {
		sel_owner->first_item = sel_item;
	}
	else {
		sel_owner->last_item->next = sel_item;
		sel_item->previous = sel_owner->last_item;
	}
	sel_owner->last_item = sel_item;

	/* Initialize private data */
	sel_item->format = 8;
	sel_item->owner = sel_owner;
	sel_item->type = XA_STRING;
	sel_item->reply_type = XA_STRING;
	sel_item->copy = SEL_COPY_YES;   /* TRUE */
	sel_item->blocksize = 1000;
	sel_item->type_name =
			xv_sel_atom_to_str(sel_owner->dpy, sel_item->type, xid);

	return XV_OK;
}


static Xv_opaque sel_item_set_avlist(Selection_item sel_item_public,
											Attr_avlist avlist)
{
	Attr_avlist attrs;
	Xv_opaque data = XV_NULL;
	int data_set = FALSE;
	int length_set = FALSE;
	unsigned nbr_bytes;
	Sel_item_info *priv = SEL_ITEM_PRIVATE(sel_item_public);
	int reply_type_set = FALSE;
	int type_set = FALSE;
	int type_name_set = FALSE;
	Sel_owner_info *sel_owner;
	XID xid;

	for (attrs = avlist; *attrs; attrs = attr_next(attrs)) {
		switch (attrs[0]) {
			case SEL_COPY:
				priv->copy = (Selection_copy_mode) attrs[1];
				break;
			case SEL_DATA:
				data = (Xv_opaque) attrs[1];
				data_set = TRUE;
				break;
			case SEL_FORMAT:
				priv->format = (int)attrs[1];
				break;
			case SEL_LENGTH:
				priv->length = (unsigned long)attrs[1];
				length_set = TRUE;
				break;
			case SEL_BLOCKSIZE:
				priv->blocksize = (int)attrs[1];
				break;
			case SEL_TYPE:
				priv->type = (Atom) attrs[1];
				priv->reply_type = priv->type;
				type_set = TRUE;
				break;
			case SEL_TYPE_NAME:
				priv->type_name = (char *)attrs[1];
				type_name_set = TRUE;
				break;
			case SEL_REPLY_TYPE:
				priv->reply_type = (Atom) attrs[1];
				reply_type_set = TRUE;
				break;
		}
	}

	sel_owner = (Sel_owner_info *) priv->owner;
	xid = (XID) xv_get(sel_item_public, XV_XID);

	if (type_name_set && !type_set) {
		priv->type =
				xv_sel_str_to_atom(sel_owner->dpy, priv->type_name, xid);
		if (! reply_type_set) priv->reply_type = priv->type;
	}
	else if (type_set && !reply_type_set) {
		priv->reply_type = priv->type;
	}

	if (data_set) {
		if (data && !length_set &&
				(!strcmp(priv->type_name, "STRING") ||
						!strcmp(priv->type_name, "FILE_NAME") ||
						!strcmp(priv->type_name, "HOST_NAME"))) {
			priv->length = strlen((char *)data);
		}
		if (priv->copy == SEL_COPY_YES) {
			if (priv->data) {
				XFree((char *)priv->data);
				priv->data = XV_NULL;
			}
			if (data && priv->length > 0) {
				nbr_bytes = BYTE_SIZE(priv->length, priv->format);
				priv->data = (Xv_opaque) xv_malloc((size_t)nbr_bytes);
				XV_BCOPY((char *)data, (char *)priv->data, (size_t)nbr_bytes);
			}
			else if (data && priv->length == 0) {
				/* otherwise we might run into a "invalid pointer" at
				 * the next XFree
				 */
				priv->data = (Xv_opaque) xv_malloc((size_t)1);
			}
			else {
				priv->data = data;
			}
		}
		else if (priv->copy == SEL_COPY_BLOCKED) {
			if (data) {
				size_t needed_bytes = BYTE_SIZE(priv->length,priv->format) + 1;

				if (needed_bytes > priv->buflen) {
					while (needed_bytes > priv->buflen) {
						priv->buflen += priv->blocksize;
					}
					if (priv->buffer) xv_free(priv->buffer);

					priv->buffer = xv_malloc((size_t)priv->buflen);
				}
				memcpy(priv->buffer, (void *)data, needed_bytes - 1);
				/* in case of format 8 : NUL-terminate */
				if (priv->format == 8) {
					priv->buffer[needed_bytes - 1] = '\0';
				}
				priv->data = (Xv_opaque)priv->buffer;
			}
			else {
				priv->data = XV_NULL;
			}
		}
		else {
			priv->data = data;
		}
	}

	return XV_OK;
}


/*ARGSUSED*/
static Xv_opaque sel_item_get_attr(Selection_item sel_item_public,
							int *status, Attr_attribute attr, va_list valist)
{
	Sel_item_info *sel_item = SEL_ITEM_PRIVATE(sel_item_public);

	switch (attr) {
		case SEL_COPY: return (Xv_opaque) sel_item->copy;
		case SEL_DATA: return sel_item->data;
		case SEL_FORMAT: return (Xv_opaque) sel_item->format;
		case SEL_LENGTH: return (Xv_opaque) sel_item->length;
		case SEL_TYPE: return (Xv_opaque) sel_item->type;
		case SEL_REPLY_TYPE: return (Xv_opaque) sel_item->reply_type;
		case SEL_BLOCKSIZE: return (Xv_opaque)sel_item->blocksize;
		case SEL_TYPE_NAME:
			if (!sel_item->type_name) {
				/* part of delayed realisation for type_name */

				Sel_owner_info *sel_owner;
				XID xid;

				sel_owner = (Sel_owner_info *) sel_item->owner;
				xid = (XID) xv_get(sel_item_public, XV_XID);
				sel_item->type_name = xv_sel_atom_to_str(sel_owner->dpy,
														sel_item->type, xid);
			}

			return (Xv_opaque) sel_item->type_name;
		default:
			if (xv_check_bad_attr(&xv_sel_item_pkg, attr) == XV_ERROR)
				*status = XV_ERROR;
			return (Xv_opaque) 0;
	}
}


static int sel_item_destroy(Selection_item sel_item_public,
									Destroy_status status)
{
    Sel_item_info  *sel_item = SEL_ITEM_PRIVATE(sel_item_public);
    Sel_item_info  *previous = sel_item->previous;

    if (status == DESTROY_CHECKING || status == DESTROY_SAVE_YOURSELF
        || status == DESTROY_PROCESS_DEATH)
	return XV_OK;

    /* Remove Selection_item for Selection_owner */
    if (previous) previous->next = sel_item->next;
    else sel_item->owner->first_item = sel_item->next;
    if (sel_item->next) sel_item->next->previous = previous;
    else sel_item->owner->last_item = previous;

    /* Free up malloc'ed storage */
	if (sel_item->copy == SEL_COPY_YES && sel_item->data) {
		xv_free(sel_item->data);
	}
	else if (sel_item->copy == SEL_COPY_BLOCKED && sel_item->buffer) {
		xv_free(sel_item->buffer);
	}
    free(sel_item);

    return XV_OK;
}


Xv_pkg xv_sel_item_pkg = {
    "Selection Item",
    ATTR_PKG_SELECTION,
    sizeof(Xv_sel_item),
    &xv_generic_pkg,
    sel_item_init,
    sel_item_set_avlist,
    sel_item_get_attr,
    sel_item_destroy,
    NULL			/* no find proc */
};
