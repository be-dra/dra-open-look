#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)generic.c 20.25 91/02/27  DRA: $Id: generic.c,v 4.6 2025/03/08 13:01:51 dra Exp $";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */

/*
 * generic.c: Support for "generic object" - basically, a property list.
 */

#include <xview/pkg.h>
#include <xview/notify.h>
#include <xview_private/gen_impl.h>
#include <xview_private/portable.h>

#include  <X11/X.h>
#include  <X11/Xlib.h>
#include  <X11/Xresource.h>

Xv_private char    *xv_instance_app_name;

/* ------------------------------------------------------------------------- */

/*
 * Public
 */

/* if 0; ignore Key-press/mouse-press synthetic events */
int  defeat_event_security; 


/*
 * Private
 */
/*
 * Escape hatch to cope with SunView and/or client bugs in ref. counting.
 */
static void generic_set_instance_name(Xv_object parent, Xv_object object, char *instance_name);
Xv_private XrmQuarkList db_qlist_from_name(char *, XrmQuarkList);
Xv_private Xv_opaque db_name_from_qlist(XrmQuarkList qlist);
Xv_private_data int xv_free_unreferenced;
static Attr_attribute xv_next_key;	/* = 0 for -A-R */

static Generic_node *add_node(Xv_object object, generic_key_type_t key);
static Generic_node *find_node(Xv_object object, generic_key_type_t key,
								Generic_node **prev);
static void delete_node(Xv_object object, Generic_node *node, Generic_node *prev);


/* ------------------------------------------------------------------------ */


/*
 * PORTABILITY ALERT! On a Sun, an attribute is an unsigned 32-bit quantity
 * with the high-order 8 bits used to hold the package identification.  Thus,
 * a valid attribute never has a 0 in its high-order 8 bits.  The following
 * routine relies on this fact to quickly generate up to 2^24 key values that
 * cannot be mistaken for valid attributes.  Note that 0 is not a valid
 * attribute or key.
 */
Xv_public Attr_attribute xv_unique_key()
{
    return (++xv_next_key);
}

/*ARGSUSED*/
static void generic_data_free(Xv_object object, Attr_attribute key, Xv_opaque data)
{
    if (data) free((char *) data);
}

static int generic_init(Xv_object parent, Xv_object object,
							Attr_avlist avlist, int *unused)
{
	Xv_generic_struct *gen_public = (Xv_generic_struct *) object;
	Generic_info *generic;

	/* performance optimization */
	short flag = TRUE;

	generic = xv_alloc(Generic_info);

	/*
	 * link private to public data and vice versa
	 */
	gen_public->private_data = (Xv_opaque) generic;
	generic->public_self = object;

	/*
	 * Set owner of object
	 */
	generic->owner = parent;

	/*
	 * Set key data nodes, instance_qlist/name to be NULL
	 */
	generic->key_data = (Generic_node *) NULL;
	generic->instance_qlist = (Xv_opaque) NULL;
	generic->instance_name = (char *)NULL;

	/*
	 * Go thru avlist in search for instance name, if any,
	 * quit immediately if/when find it
	 */
	while (flag && *avlist) {
		switch ((Xv_generic_attr) (*avlist)) {
			case XV_INSTANCE_NAME:
				/*
				 * Store instance name in generic object
				 */
				generic_set_instance_name(parent, object, (char *)avlist[1]);
				/* performance optimization */
				flag = FALSE;
				break;

			default:
				break;
		}
		avlist = attr_next(avlist);

	}
	(void)notify_set_destroy_func(object,
			(Notify_destroy_func) xv_destroy_status);
	return XV_OK;
}

Xv_private XrmQuarkList generic_create_instance_qlist(Xv_object, char *);

Xv_private XrmQuarkList generic_create_instance_qlist(Xv_object parent, char *instance_name)
{
/* 	Xv_opaque parent_quarks = XV_NULL, quarks; */
	XrmQuarkList parent_quarks = NULL, quarks;
	short null_parent = FALSE;

	if (instance_name == NULL) {
		return ((XrmQuarkList) NULL);
	}

	if (parent != XV_NULL)
		parent_quarks = (XrmQuarkList)xv_get(parent, XV_INSTANCE_QLIST);
	else {
		null_parent = TRUE;
		parent_quarks = db_qlist_from_name(xv_instance_app_name, NULL);
	}

	quarks = db_qlist_from_name(instance_name, parent_quarks);

	if ((null_parent == TRUE) && (parent_quarks != NULL))
		free((void *)parent_quarks);

	return quarks;
}

static void generic_set_instance_name(Xv_object parent, Xv_object object, char *instance_name)
{
    Xv_opaque       	quarks;

    quarks = (Xv_opaque)generic_create_instance_qlist(parent, instance_name);

    if (quarks != XV_NULL)
        xv_set(object, XV_INSTANCE_QLIST, quarks, NULL);
}

static Xv_opaque generic_set_avlist(Xv_object object, Attr_avlist avlist)
{
	generic_key_type_t key;
	register Generic_node *node;
	Generic_node *prev, *existing_node;
	int ref_count;
	Generic_info *generic = GEN_PRIVATE(object);
	register Xv_opaque error_code = XV_NULL;

	for (; *avlist; avlist = attr_next(avlist)) {
		/* The following check is done for performance reasons.  The check
		 * short-circuits attribute xv_set's for attributes not owned by
		 * this class or its' super-class.
		 */
		if ((ATTR_PKG(*avlist) == ATTR_PKG_GENERIC) ||
				(ATTR_PKG(*avlist) == ATTR_PKG_SV)) {
			switch (*avlist) {
				case XV_REF_COUNT:
					/* PERFORMANCE ALERT: is ref count as property too slow? */
					/*
					 * Object is only destroyed if its reference count has been
					 * decremented to 0; an explicit reset to 0 will not trigger the
					 * destroy.  Since xv_destroy_safe does not really attempt to
					 * destroy the object until the process unwinds back to the
					 * Notifier, the reference count may become non-zero in the
					 * meantime, thus aborting the destroy.
					 */
					key = XV_REF_COUNT;
					existing_node = node = find_node(object, key, &prev);
					if (node) {
						ref_count = (int)node->data;
					}
					else {
						node = add_node(object, key);

#ifdef _XV_DEBUG
						if ((int)avlist[1] != XV_RC_SPECIAL)
							abort();
#endif

						node->remove_proc = NULL;
						ref_count = 0;
					}
					if (node) {
						switch ((int)avlist[1]) {
							case XV_RC_SPECIAL:
								ref_count = 0;
								break;
							case XV_RC_SPECIAL + 1:
								ref_count++;
								break;
							case XV_RC_SPECIAL - 1:
								ref_count--;
								if (xv_free_unreferenced && existing_node &&
										ref_count == 0) {
									(void)xv_destroy_safe(object);
								}
								break;
							default:
								ref_count = (int)avlist[1];
								break;
						}

#ifdef _XV_DEBUG
						if (ref_count < 0)
							abort();
#endif

						node->data = (Xv_opaque) ref_count;
					}
					else {
						error_code = *avlist;
					}
					break;

				case XV_KEY_DATA:
					key = (generic_key_type_t) avlist[1];
					existing_node = node = find_node(object, key, &prev);
					if (!node) {
						node = add_node(object, key);
					}
					if (node) {
						if (existing_node && existing_node->data != avlist[2]) {
							if (existing_node->remove_proc)
								(existing_node->remove_proc) (object,
										existing_node->key,
										existing_node->data);
						}
						node->data = avlist[2];
						node->remove_proc = NULL;
					}
					else {
						error_code = *avlist;
					}
					break;

				case XV_COPY_OF:
				case XV_END_CREATE:
					/* Explicitly ignore these: they are for sub-classes */
					break;

				case XV_KEY_DATA_COPY_PROC:
				case XV_KEY_DATA_REMOVE:
				case XV_KEY_DATA_REMOVE_PROC:

					key = (generic_key_type_t) avlist[1];

					node = find_node(object, key, &prev);
					if (node) {
						switch ((Xv_generic_attr) (*avlist)) {
							case XV_KEY_DATA_COPY_PROC:
								node->copy_proc = (void (*)(int))avlist[2];
								break;
							case XV_KEY_DATA_REMOVE:
								delete_node(object, node, prev);
								break;
							case XV_KEY_DATA_REMOVE_PROC:
								node->remove_proc =
										(void (*)(Xv_opaque, generic_key_type_t,
												Xv_opaque))avlist[2];
								break;
							default:
								break;
						}
					}
					else {
						error_code = *avlist;
					}
					break;

				case XV_LABEL:
				case XV_NAME:
					/* If old data has been set, remove it first. */
					if (xv_get(object, XV_KEY_DATA, avlist[0]))
						(void)xv_set(object, XV_KEY_DATA_REMOVE, avlist[1],
								NULL);
					/* Set new data. */
					(void)xv_set(object,
							XV_KEY_DATA, avlist[0], strdup((char *)avlist[1]),
							XV_KEY_DATA_REMOVE_PROC, avlist[0],
							generic_data_free, NULL);
					break;

				case XV_OWNER:
					generic->owner = (Xv_object) avlist[1];
					break;

				case XV_INSTANCE_QLIST:
					generic->instance_qlist = (Xv_opaque) avlist[1];
					break;

				case XV_STATUS:
					/* PERFORMANCE ALERT: is recursion too slow? */
					if (avlist[1]) {
						(void)xv_set(object, XV_KEY_DATA, avlist[0], avlist[1],
								NULL);
					}
					else {
						(void)xv_set(object, XV_KEY_DATA_REMOVE, avlist[0],
								NULL);
					}
					break;

				case ATTR_NOP0:
				case ATTR_NOP1:
				case ATTR_NOP2:
				case ATTR_NOP3:
				case ATTR_NOP4:
					/* Explicitly ignore these: they are meant to be ignored */
					break;

				case XV_SHOW:	/* XV_SHOW should be handled by individual
								 * packages. */
					break;
				default:
					(void)xv_check_bad_attr(XV_GENERIC_OBJECT,
							(Attr_attribute) * avlist);
					break;
			}
		}
		else {
			(void)xv_check_bad_attr(XV_GENERIC_OBJECT,
					(Attr_attribute) * avlist);
		}
	}

	return error_code;
}

Pkg_private Xv_opaque generic_get(Xv_object object, int *status,
								Attr_attribute attr, va_list args)
{
	generic_key_type_t key;
	register Xv_opaque result = XV_NULL;
	register Generic_node *node;
	Generic_node *prev;
	Generic_info *generic = GEN_PRIVATE(object);

	/* Don't set *status to XV_ERROR unless attribute is unrecognized! */
	switch (attr) {
		case XV_KEY_DATA:
		case XV_KEY_DATA_COPY_PROC:
		case XV_KEY_DATA_REMOVE_PROC:
			key = va_arg(args, generic_key_type_t);
			node = find_node(object, key, &prev);
			if (node) {
				switch (attr) {
					case XV_KEY_DATA:
						result = node->data;
						break;
					case XV_KEY_DATA_COPY_PROC:
						result = (Xv_opaque) node->copy_proc;
						break;
					case XV_KEY_DATA_REMOVE_PROC:
						result = (Xv_opaque) node->remove_proc;
						break;
				}
			}
			else {
				result = 0;
			}
			break;

		case XV_IS_SUBTYPE_OF:{
				const Xv_pkg *pkg = ((Xv_base *) object)->pkg;
				const Xv_pkg *super_pkg;

				super_pkg = va_arg(args, Xv_pkg *);
				while (pkg) {
					if (pkg == super_pkg) {
						return (Xv_opaque) TRUE;
					}
					pkg = pkg->parent_pkg;
				}
				result = FALSE;
				break;
			}

		case XV_REF_COUNT:
			node = find_node(object, XV_REF_COUNT, &prev);
			if (node) {
				result = node->data;
			}
			else {
				result = 0;
			}
			break;

		case XV_OWNER:
			result = (Xv_opaque) generic->owner;
			break;

		case XV_TYPE:
			result = (Xv_opaque) ((Xv_base *) object)->pkg;
			break;

		case XV_INSTANCE_NAME:
			result = (Xv_opaque) generic->instance_name;
			/*
			 * If instance name not set yet, get the quark list,
			 * and obtain the instance name from it
			 */
			if (!result) {
				if (generic->instance_qlist) {
					generic->instance_name =
							(char *)db_name_from_qlist((XrmQuarkList)
							generic->instance_qlist);
					result = (Xv_opaque) generic->instance_name;
				}
			}
			break;

		case XV_INSTANCE_QLIST:
			result = (Xv_opaque) generic->instance_qlist;
			break;

		case XV_LABEL:
		case XV_NAME:
		case XV_STATUS:
			/* PERFORMANCE ALERT!  Is recursion too slow? */
			result = xv_get(object, XV_KEY_DATA, attr);
			break;
		case XV_SELF:
			result = object;
			break;

#ifdef OW_I18N
		case XV_IM:
			result = NULL;
			break;
#endif /* OW_I18N */

		default:
			if (xv_check_bad_attr(XV_GENERIC_OBJECT, attr) == XV_ERROR) {
				*status = XV_ERROR;
			}
			result = 0;
			break;
	}

	return result;
}

static int generic_destroy(Xv_object object, Destroy_status status)
{
	register Generic_node *node;
	Generic_info *generic = GEN_PRIVATE(object);

	switch (status) {
		case DESTROY_CHECKING:
			if (xv_get(object, XV_REF_COUNT))
				/* Assume decrement followed by increment => don't destroy. */
				return (XV_ERROR);
			break;
		case DESTROY_CLEANUP:
			if (generic->instance_qlist) {
				xv_free(generic->instance_qlist);
				generic->instance_qlist = XV_NULL;
			}
			while ((node = HEAD(object))) {
				delete_node(object, node, (Generic_node *) 0);
			}
			notify_remove(object);
			free(generic);
			break;
		case DESTROY_PROCESS_DEATH:
			notify_remove(object);
			free(generic);
			break;
		default:
			break;
	}
	return XV_OK;
}

static Generic_node *add_node(Xv_object object, generic_key_type_t key)
{
	register Generic_node *node;
	Generic_info *gen_private = GEN_PRIVATE(object);;

	node = xv_alloc(Generic_node);
	node->next = HEAD(object);
	gen_private->key_data = node;
	node->key = key;

	return node;
}

static Generic_node *find_node(Xv_object object, generic_key_type_t key,
								Generic_node **prev)
{
	register Generic_node *node;

	if (HEAD(object)) {
		if ((HEAD(object)->key == key)) {
			node = HEAD(object);
			*prev = (Generic_node *) NULL;
		}
		else {
			for (*prev = HEAD(object), node = (*prev)->next; node;
					*prev = node, node = (*prev)->next) {
				if (node->key == key)
					break;
			}
		}
	}
	else {
		*prev = node = (Generic_node *) NULL;
	}
	return node;
}

static void delete_node(Xv_object object, Generic_node *node, Generic_node *prev)
{
	if (prev) {
		prev->next = node->next;
	}
	else {
		Generic_info *gen_private = GEN_PRIVATE(object);;
		gen_private->key_data = node->next;
	}
	if (node->remove_proc)
		(node->remove_proc) (object, node->key, node->data);
	xv_free(node);
}

const Xv_pkg xv_generic_pkg = {
    "Generic",
    ATTR_PKG_GENERIC,
    sizeof(Xv_generic_struct),
    NULL,			/* No parent package */
    generic_init,
    generic_set_avlist,
    generic_get,
    generic_destroy,
    NULL			/* No find procedure */
};
