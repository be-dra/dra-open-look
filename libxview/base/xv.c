#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)xv.c 20.47 91/01/30  DRA: $Id: xv.c,v 4.7 2025/04/21 07:10:32 dra Exp $";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */

/* XView interface layer */

/* ------------------------------------------------------------------------- */

#include <stdio.h>
#include <sys/types.h>
#include <xview_private/i18n_impl.h>
#include <xview_private/attr_impl.h>
#include <xview_private/xview_impl.h>
#include <xview_private/svr_impl.h>
#include <xview_private/portable.h>
#include <xview/attr.h>
#include <xview/pkg_public.h>
#include <xview/pkg.h>
#include <xview/notify.h>
#include <xview/generic.h>
#include <xview/server.h>

#ifndef SVR4
#    include <xview/window.h>
#endif /* SVR4 */

/* ------------------------------------------------------------------------- */

Pkg_private void xv_temporarily_report_seal(Xv_opaque obj, char *func);
/*
 * Private
 */

void xv_temporarily_report_seal(Xv_opaque obj, char *func)
{
/* 	Xv_base *ccom = (Xv_base *) obj; */

/* 	fprintf(stderr, "%s: obj %ld , seal %lx, cls %s\n", func, obj, */
/* 							ccom->seal, ccom->pkg->name); */
}

Xv_private Xv_opaque generic_get(Xv_object, int *, Attr_attribute, va_list);

/* used to automagically call initialze sv */
static	int 	    	xv_initialized;	/* = FALSE */


/* xv_alloc_save_ret is used to store an intermediate value needed by the
   xv_alloc macros. This is not necessary if the macros are implemented as
   functions. Macros were chosen for performance reasons.
*/
void *_no_longer_used_xv_alloc_save_ret;

void xv_alloc_error(void)
{
   xv_error(XV_NULL,
            ERROR_LAYER, ERROR_SYSTEM,
            ERROR_STRING, "call to alloc function returned NULL pointer",
            NULL);
}

void *xv_calloc(unsigned int num, unsigned int size)
{
   void *ptr;

   if(( ptr = calloc( (size_t)num, (size_t)size )))
      return ptr;
   else
   {
      xv_error(XV_NULL,
               ERROR_LAYER, ERROR_SYSTEM,
               ERROR_STRING, "call to calloc function returned NULL pointer",
               NULL);
      return NULL;
   }
}


#define  DELETE_WINDOW	0
#define  SAVE_YOURSELF	1


/*
 * An overview of "objects". Every valid object is of one of two types: 1) A
 * standard object: the pointer passed to and from the Sv client points to a
 * Xv_base.  The first field of that struct contains the value XV_OBJECT_SEAL
 * - a "magic" value that (hopefully) only occurs as the first 4 bytes of a
 * valid object. 2) An embedding object: the client pointer points into the
 * middle of a standard object.  This is required by the need for the pointer
 * to point to an old-style SunWindows object.  In this latter case, the
 * embedded object must have the following structure:
 * --------------------------- | Xv_base		  |
 * --------------------------- | Other "std" fields	  |
 * --------------------------- | Private fields	  |
 * --------------------------- | Embedding seal & offset |
 * --------------------------- -- Client ptr ->	| SunWindow object	  |
 * ---------------------------
 *
 * The "drivers" for the generic Sv operations look to see if the first four
 * bytes of the memory addressed by the pointer given to them contain the
 * value XV_OBJECT_SEAL.  If not, then the 4 bytes preceding should contain
 * the OR of XV_EMBEDDING_SEAL and a number between 12 and 255. This latter
 * number is the sizeof() all of the fields above the SunWindow object in
 * this class of embedding objects.  [WARNING: Note that the pkg must be
 * careful about the alignment restriction between the "Embedding seal &
 * offset" and the "SunWindow object".
 */
#ifndef XV_OBJECT_SEAL
#define XV_OBJECT_SEAL		0xF0A58142
#endif  /* XV_OBJECT_SEAL */
#define XV_EMBEDDING_SEAL	0xF1B69200
#define XV_EMBEDDING_MASK	0xFFFFFF00

/*
 * Extern
 */
extern struct pixrectops server_image_ops;

#ifndef MAX_NESTED_PKGS
#define MAX_NESTED_PKGS	20
#endif

/* ------------------------------------------------------------------------ */


Xv_private int xv_set_embedding_data(Xv_opaque object, Xv_opaque std_object)
{
    Xv_embedding   *lu_ptr = (Xv_embedding *) object;
    char           *c_ptr = (char *) object;
    int             byte_offset = c_ptr - (char *) std_object;

    if (*((unsigned long *) object) == XV_OBJECT_SEAL)
	/*
	 * Argh: data in object looks like seal of standard object. This will
	 * break all future references to object, so give up!
	 */
	xv_error(object,
		 ERROR_SEVERITY, ERROR_NON_RECOVERABLE,
		 ERROR_STRING,
		     XV_MSG("data in object looks like seal of standard object"),
		 NULL);

    if (byte_offset != (byte_offset & (~XV_EMBEDDING_MASK)))
	/* The embedding header is too large for our encoding scheme. */
	xv_error(object,
		 ERROR_SEVERITY, ERROR_NON_RECOVERABLE,
		 ERROR_STRING,
		     XV_MSG("embedding header too large for our encoding scheme"),
		 NULL);

    lu_ptr--;
    *lu_ptr = XV_EMBEDDING_SEAL | byte_offset;
    return (byte_offset);
}


Xv_private Xv_opaque xv_object_to_standard(Xv_opaque object, const char *caller)
{
	/* BUG ALERT!  Should this routine allow multiple levels of embedding? */

	register char *c_ptr;
	register Xv_embedding *lu_ptr;
	register Xv_base *ccom_object = (Xv_base *) object;
	int byte_offset;

	/* Not a standard object: see if embedded object. */
	c_ptr = (char *)object;
	lu_ptr = (Xv_embedding *) object;
	lu_ptr--;
	if ((*lu_ptr & XV_EMBEDDING_MASK) != XV_EMBEDDING_SEAL) {
		/* hier kam immer  0 != f1b69200 
		fprintf(stderr, "%ld: xv_object_to_standard: %lx != %lx\n",
				object, (*lu_ptr & XV_EMBEDDING_MASK),
				(unsigned long)XV_EMBEDDING_SEAL);
		*/
		if (getenv("XVIEW_DESTROY_ABORT")) {
			abort();
		}
		xv_error(object,
				ERROR_CANNOT_GET, SERVER_SYNC_AND_PROCESS_EVENTS,
				ERROR_INVALID_OBJECT, XV_MSG("embedding seal incorrect"),
				ERROR_STRING, caller,
				NULL);
		return ((Xv_opaque) 0);
	}
	/* Embedding seal is okay: extract the offset. */
	byte_offset = *lu_ptr & (~XV_EMBEDDING_MASK);
	if (byte_offset < sizeof(Xv_base) + sizeof(lu_ptr)) {
		xv_error(object,
				ERROR_INVALID_OBJECT,
				XV_MSG("byte offset incorrect"), ERROR_STRING, caller, NULL);
		return ((Xv_opaque) 0);
	}
	/* Offset is okay: see if offset pointer address a standard object. */
	c_ptr -= byte_offset;
	ccom_object = (Xv_base *) c_ptr;
	if (ccom_object->seal != XV_OBJECT_SEAL) {
		xv_error(object,
				ERROR_INVALID_OBJECT,
				XV_MSG("standard seal incorrect"), ERROR_STRING, caller, NULL);
		return ((Xv_opaque) 0);
	}
	/* Offset pointer is okay. */
	return ((Xv_opaque) ccom_object);
}

static Xv_object xv_find_avlist(Xv_opaque parent, const Xv_pkg *pkg,
								Attr_attribute *avlist)
{
	register const Xv_pkg *find_pkg;
	Attr_avlist attrs;
	Xv_object object = XV_NULL;
	int auto_create = TRUE, auto_create_seen = FALSE;

	for (attrs = (Attr_avlist) avlist; *attrs; attrs = attr_next(attrs)) {
		switch ((int)attrs[0]) {
			case XV_AUTO_CREATE:
				auto_create = (int)attrs[1];
				auto_create_seen = TRUE;
				break;
			default:
				break;
		}
		if (auto_create_seen)	/* optimize if at front of list */
			break;
	}
	if (!auto_create && !xv_initialized)
		return object;	/* why bother */

	/*
	 * Now see if this is the first call to xv_find().
	 */
	if (!xv_initialized) {
		xv_initialized = TRUE;
		/* use and xv_init attrs from the avlist */
		xv_init((Attr_attribute) ATTR_LIST, avlist, NULL);
		/* create the default server */
		if (pkg != SERVER)
			if (!xv_create(XV_NULL, SERVER, NULL))
				xv_connection_error((char *)NULL);
	}
	/*
	 * Run through the pkg list looking for object "find" support. If we can
	 * find an object of the same type as pkg, return that rather than
	 * creating a new one.
	 */
	for (find_pkg = pkg; find_pkg; find_pkg = find_pkg->parent_pkg) {
		if (find_pkg->find) {
			object = (find_pkg->find) (parent, pkg, avlist);
			if (object)
				break;
		}
	}
	if (!object && auto_create)
		object = xv_create_avlist(parent, pkg, avlist);

	return object;
}

Xv_public Xv_object xv_find( Xv_opaque parent, const Xv_pkg *pkg, ... )
{
    AVLIST_DECL;
    va_list         args;

    VA_START(args, pkg);
    MAKE_AVLIST( args, avlist );
    va_end(args);

    return xv_find_avlist(parent, pkg, avlist);
}

Xv_public Xv_object xv_create(Xv_opaque parent, const Xv_pkg *pkg, ...)
{
    AVLIST_DECL;
    va_list         args;

    VA_START( args, pkg );
    MAKE_AVLIST ( args, avlist );
    va_end(args);

    return xv_create_avlist(parent, pkg, avlist);
}

Xv_private Xv_object xv_create_avlist(Xv_opaque parent, const Xv_pkg *pkg,
								Attr_attribute *avlist)
{
	Xv_object object = XV_NULL;
	Xv_base *ccom_object;
	const Xv_pkg *pkg_stack[MAX_NESTED_PKGS];
	register const Xv_pkg **pkgp, **loop_pkgp;
	const Xv_pkg *orig_pkg = pkg;
	register int error_code = 0;
	int embedding_offset, total_offset;
	Attr_attribute argv[2];
	Attr_avlist search_avlist = avlist;
	short flag = TRUE;
	char *instance_name = (char *)NULL;
	Attr_attribute avarray[ATTR_STANDARD_SIZE];
	Attr_avlist avlist_copy = avarray;
	Attr_avlist avlist_used = avlist;

	/*
	 * first see if this is the first call to xv_create().
	 */
	if (!xv_initialized) {
		xv_initialized = TRUE;
		/* use and xv_init attrs from the avlist */
		xv_init((Attr_attribute) ATTR_LIST, avlist, NULL);
		/* create the default server */
		if (pkg != SERVER)
			if (!xv_create(XV_NULL, SERVER, NULL))
				xv_connection_error((char *)NULL);
	}

	/*
	 * Create object which begins as instance of no package.  It becomes an
	 * instance of successively more specialized packages as each layer is
	 * initialized.
	 */
	object = (Xv_object) xv_alloc_n(char, (unsigned long)pkg->size_of_object);

	ccom_object = (Xv_base *) object;
	ccom_object->seal = XV_OBJECT_SEAL;
	/*
	 * Stack packages oldest to youngest (base to client-visible pkg).
	 */
	pkgp = pkg_stack;
	while (pkg) {
		*pkgp++ = pkg;
		pkg = pkg->parent_pkg;
	}

	/*
	 * Search for instance name in avlist, to be used in
	 * attribute customization
	 */
	while (flag && *search_avlist) {
		switch (*search_avlist) {
			case XV_INSTANCE_NAME:
				instance_name = (char *)search_avlist[1];
				flag = FALSE;
				break;
		}
		search_avlist = attr_next(search_avlist);
	}

	/*
	 * Flatten out XV_USE_DB lists first
	 */
	avlist_used = attr_customize((Xv_object) NULL, orig_pkg, instance_name,
			parent, avlist_copy, ATTR_STANDARD_SIZE, avlist);

	/*
	 * Execute stacked functions.
	 */
	total_offset = 0;
	loop_pkgp = pkgp;
	while (pkg_stack <= --loop_pkgp && error_code == 0) {
		/* set object up as an instance of *pkgp */
		ccom_object->pkg = *loop_pkgp;
		if ((*loop_pkgp)->init) {	/* Ignore missing functions */
			embedding_offset = 0;
			/* BUG ALERT! Most init routines don't know about the 4th arg. */
			error_code = ((*loop_pkgp)->init) (parent, object, avlist_used,
													&embedding_offset);
			total_offset += embedding_offset;
		}
	}

	xv_temporarily_report_seal(object, "xv_create_avlist");

	if (error_code) {
		if (ccom_object->pkg->parent_pkg) {
			/* Demote object to last successful package and destroy it. */
			ccom_object->pkg = ccom_object->pkg->parent_pkg;
			(void)xv_destroy_status(object, DESTROY_CLEANUP);
		}
		object = XV_NULL;
	}
	else {
		/*
		 * Pass the avlist back to object to allow "normal" xv_set processing
		 * to do most (or all) of the work associated with the avlist.  Then,
		 * make a final pass via XV_END_CREATE to allow the object to do any
		 * "post-create" processing. Finally, set up for notifier destroy
		 * processing, using the public handle returned to client!
		 */
		(void)xv_set_avlist(object, avlist_used);

		/* Call set routines with XV_END_CREATE in init order */
		argv[0] = XV_END_CREATE;
		argv[1] = 0;
		loop_pkgp = pkgp;
		while (pkg_stack <= --loop_pkgp && error_code == 0) {
			if ((*loop_pkgp)->set) {	/* Ignore missing functions */
				/*
				 * some set procs may return XV_SET_DONE, so deal with this.
				 */
				error_code = (int)((*loop_pkgp)->set) (object, argv);
				switch (error_code) {
					case (int)XV_OK:
					case (int)XV_SET_DONE:
						break;

					default:
						(void)xv_destroy_status(object, DESTROY_CLEANUP);
						return XV_NULL;
				}
			}
		}

		object = (Xv_opaque) ((char *)object + total_offset);
	}

	return object;
}

Xv_public Xv_opaque xv_set(Xv_opaque object, ...)
{
	AVLIST_DECL;
	Attr_attribute flat_avlist[ATTR_STANDARD_SIZE];
	va_list args;

	if (object == XV_NULL) {
		xv_error(XV_NULL,
				ERROR_SEVERITY, ERROR_NON_RECOVERABLE,
				ERROR_STRING, XV_MSG("NULL pointer passed to xv_set"),
				NULL);
	}

	VA_START(args, object);
	MAKE_AVLIST(args, avlist);
	va_end(args);

	avlist = attr_customize(object, ((Xv_base *) object)->pkg,
			(char *)NULL, XV_NULL, flat_avlist, ATTR_STANDARD_SIZE, avlist);
	return xv_set_avlist(object, avlist);
}

static Xv_opaque xv_set_pkg_avlist(Xv_object object, const Xv_pkg *pkg,
											Attr_avlist avlist)
/* Caller must guarantee that object is a standard, not embedded, object. */
{
	register Xv_opaque error_code;

	/*
	 * Execute the set procs youngest to oldest (client-visible pkg to base
	 * ). e.g. canvas-window-generic
	 */
	for (; pkg; pkg = pkg->parent_pkg) {
		if (!pkg->set)
			continue;

		/*
		 * we are done if the set proc says so or returns the bad attribute.
		 * If XV_SET_DONE is returned, the rest of the set has been done
		 * (using xv_super_set_avlist()), so return XV_OK now.
		 */
		if ((error_code = (*(pkg->set)) (object, avlist)) != XV_OK) {
			return (error_code == XV_SET_DONE) ? XV_OK : error_code;
		}
	}

	return XV_OK;
}

Xv_private Xv_opaque xv_set_avlist(Xv_opaque passed_object, Attr_avlist avlist)
{
	register Xv_opaque object;

	XV_OBJECT_TO_STANDARD(passed_object, "xv_set", object);
	if (!object)
		return (Xv_opaque) XV_ERROR;

	return xv_set_pkg_avlist(object, ((Xv_base *) object)->pkg, avlist);
}

Xv_public Xv_opaque xv_super_set_avlist(Xv_opaque object, const Xv_pkg *pkg,
											Attr_avlist avlist)
/* Caller must guarantee that object is a standard, not embedded, object. */
{
    return xv_set_pkg_avlist(object, pkg->parent_pkg, avlist);
}

Xv_public Xv_opaque xv_get(Xv_opaque passed_object, Attr32_attribute attr, ...)
{
	register const Xv_pkg *pkg;
	int status;
	Xv_opaque result;
	va_list args;
	Xv_opaque object;

	XV_OBJECT_TO_STANDARD(passed_object, "xv_get", object);
	if (!object)
		return XV_NULL;

	/*
	 * Execute the get procs youngest to oldest (client-visible pkg to base
	 * ). e.g. canvas-window-generic
	 */

	switch (attr) {
		/* fast path: handle these two frequently used attributes directly. */
		case XV_KEY_DATA:
		case XV_IS_SUBTYPE_OF:
			status = XV_OK;
			VA_START(args, attr);
			result = generic_get(object, &status, (Attr_attribute) attr, args);
			va_end(args);
			return result;
		default: break;
	}

	for (pkg = ((Xv_base *) object)->pkg; pkg; pkg = pkg->parent_pkg) {
		if (!pkg->get)
			continue;

		/*
		 * Assume object will handle the get. Object should set status to
		 * XV_ERROR if not handled
		 */
		status = XV_OK;
		/*
		 * Go to the beginning of the varargs list every time to insure each
		 * pkg gets the start of the  varargs.
		 */
		VA_START(args, attr);

		/* ask the object to handle the get */
		result = (*(pkg->get)) (object, &status, (Attr_attribute) attr, args);
		va_end(args);

		if (status == XV_OK) {
			/* result is the answer -- return it */
			return result;
		}
	}

	/*
	 * None of the packages handled the get.  There are three possibilities,
	 * but for all of them xv_get() should return 0: a) Attribute is valid,
	 * but not for this object.  This is suspect, as it implies the caller is
	 * relying on the fact that xv_get() returns 0 if no package recognizes a
	 * valid attribute. b) Attribute is valid, and for this object.  Either
	 * one or more of the packages' get procedures are in error, or there has
	 * been a problem with the .h - .c file correspondence, possibly caused
	 * by compilation phase error where not all of the object files in the
	 * executable were compiled against the same version of the .h files. c)
	 * Attribute is invalid.  Unfortunately, this is undetectable.
	 */
	return XV_NULL;
}



/* if client already has handled varargs list use this procedure */
/* to start the get. We have duplicated the code from xv_get */
/* rather then shared so the generic xv_get doesn't incur */
/* another procedure call, and so the safety value in xv_get */
/* of restarting the varargs list for each package remains */
/* for the generic case. */

/* We should review whether xv_get should call xv_get_avlist */

Xv_private Xv_opaque xv_get_varargs(Xv_opaque passed_object,
							Attr_attribute attr, va_list valist)
{
	register const Xv_pkg *pkg;
	Xv_opaque object;
	int status;
	Xv_opaque result;

	XV_OBJECT_TO_STANDARD(passed_object, "xv_get", object);
	if (!object)
		return XV_NULL;

	/*
	 * Execute the get procs youngest to oldest (client-visible pkg to base
	 * ). e.g. canvas-window-generic
	 */
	for (pkg = ((Xv_base *) object)->pkg; pkg; pkg = pkg->parent_pkg) {
		if (!pkg->get)
			continue;

		/*
		 * Assume object will handle the get. Object should set status to
		 * XV_ERROR if not handled
		 */
		status = XV_OK;
		/*
		 * Assume client has done va_start. xv_get calls va_start() every
		 * time to insure each pkg gets the start of the varargs. we aren't.
		 * This should still work.
		 */
		/* ask the object to handle the get */
		result = (*(pkg->get)) (object, &status, attr, valist);

		if (status == XV_OK) {
			/* result is the answer -- return it */
			return result;
		}
	}

	/*
	 * None of the packages handled the get.  There are three possibilities,
	 * but for all of them xv_get() should return 0: a) Attribute is valid,
	 * but not for this object.  This is suspect, as it implies the caller is
	 * relying on the fact that xv_get() returns 0 if no package recognizes a
	 * valid attribute. b) Attribute is valid, and for this object.  Either
	 * one or more of the packages' get procedures are in error, or there has
	 * been a problem with the .h - .c file correspondence, possibly caused
	 * by compilation phase error where not all of the object files in the
	 * executable were compiled against the same version of the .h files. c)
	 * Attribute is invalid.  Unfortunately, this is undetectable.
	 */
	return XV_NULL;
}


/*
 * A fast overview of destruction. Conforming with SunView 1.X, xv_destroy()
 * is an immediate destroy that goes through the Notifier mechanisms.
 * However, since not all objects are guaranteed to already be registered
 * with the Notifier, xv_destroy() (and xv_destroy_safe()) always register
 * the object with the Notifier.  This causes buggy code that would have
 * worked in SunView 1.X to fail in SunView 2, because a second (incorrect)
 * destroy in SunView 1.X was ignored because the Notifier would no longer
 * know about the object as a side-effect of the first (correct) destroy.
 * xv_destroy_safe() exists to allow internal reference counting to work in
 * cases where the reference count temporarily drops to 0 (thereby triggering
 * a destroy) but then goes back up.  This scenario occurs when the client
 * code has temporarily stashed a reference to a shared object in one of its
 * local frames. xv_destroy_check() allows emulation of SunView 1.X's
 * window_destroy. xv_destroy_immediate() is used internally to SunView 2 to
 * "pre-walk" dependence hierarchies that the Notifier is not able to
 * recognize.
 */
static int xv_destroy_internal(Xv_opaque object,
			Notify_event_type check_when, Notify_event_type destroy_when,
			int destruction_type)
{
	if (!object)
		return XV_ERROR;

	if (destruction_type == DELETE_WINDOW) {
		if (notify_post_destroy(object, DESTROY_CHECKING, check_when) ==
				NOTIFY_DESTROY_VETOED) {
			return XV_ERROR;
		}
		notify_post_destroy(object, DESTROY_CLEANUP, destroy_when);
	}
	else
		/*
		 * SAVE_YOURSELF which cannot be vetoed and is only a one phase
		 * destruction method.
		 */
		notify_post_destroy(object, DESTROY_SAVE_YOURSELF, destroy_when);

	return XV_OK;
}

Xv_public int xv_destroy(Xv_opaque object)
{
    return xv_destroy_internal(object, NOTIFY_IMMEDIATE, NOTIFY_IMMEDIATE,
			       DELETE_WINDOW);
}

Xv_public int xv_destroy_check(Xv_opaque object)
{
    return xv_destroy_internal(object, NOTIFY_IMMEDIATE, NOTIFY_SAFE,
			       DELETE_WINDOW);
}

Xv_public int xv_destroy_immediate(Xv_opaque object)
{
    return xv_destroy_internal(object, NOTIFY_IMMEDIATE, NOTIFY_IMMEDIATE,
			       DELETE_WINDOW);
}

Xv_public int xv_destroy_safe(Xv_opaque object)
{
    return xv_destroy_internal(object, NOTIFY_SAFE, NOTIFY_SAFE,
			       DELETE_WINDOW);
}

Xv_public int xv_destroy_status(Xv_object passed_object, Destroy_status status)
{
	register const Xv_pkg *pkg;
	Xv_opaque object;

	if (status == DESTROY_CLEANUP)
		xv_temporarily_report_seal(passed_object, "xv_destroy_status");
	XV_OBJECT_TO_STANDARD(passed_object, "xv_destroy_status", object);
	if (!object)
		return XV_ERROR;
	/*
	 * Execute the destroy procs, youngest to oldest (client-visible pkg to
	 * base), e.g. canvas-window-generic.
	 */
	for (pkg = ((Xv_base *) object)->pkg; pkg; pkg = pkg->parent_pkg) {
		if (status == DESTROY_CLEANUP)
			SERVERTRACE((800, "%ld = %s\n", object, pkg->name));
		if (!pkg->destroy)	/* Ignore missing functions */
			continue;
		/*
		 * If any one pkg vetoes the destroy, call it quits. BUG ALERT:
		 * quitting when status != DESTROY_CHECKING leaves the object in an
		 * inconsistent state.  Perhaps it should become an instance of the
		 * pkg that incorrectly vetoed.
		 */
		if ((pkg->destroy) (object, status) != XV_OK) {
			if (status == DESTROY_CHECKING) {
				notify_veto_destroy(passed_object);
			}
			else {
				char buffer[128];

				(void)sprintf(buffer, "%s: %s '%s',\n\t%s (%d) - %s.",
						XV_MSG("xv_destroy_status"),
						XV_MSG("internal error in package"),
						pkg->name,
						XV_MSG("attempted veto during wrong phase"),
						(int)status, XV_MSG("send bug report"));
				xv_error(object, ERROR_STRING, buffer, NULL);
				abort();
			}
			return XV_ERROR;
		}
		if (status == DESTROY_CLEANUP) {
			/* unlink package from instance */
			((Xv_base *) object)->pkg = pkg->parent_pkg;
		}
	}
	if (status == DESTROY_CLEANUP) {	/* waste of time if ...PROCESS_DEATH */
		xv_free(object);
	}
	return XV_OK;
}

Xv_private int xv_check_bad_attr(const Xv_pkg *pkg, Attr_attribute attr)
/*
 * At first glance the return values seem to be backwards. However, if the
 * specified package was meant to handle the attribute, it wants to return a
 * zero value and have that accepted as the result, which requires it to not
 * modify *status, and thus we return XV_OK below in that case.
 */
{
    if ((
	 pkg->attr_id == ATTR_PKG(attr)
#ifdef ATTR_CONSUME_BUGS_FIXED
	 || pkg == XV_GENERIC_OBJECT
#endif
	) && !ATTR_CONSUMED(attr)) {
	xv_error(XV_NULL,
		 ERROR_BAD_ATTR, attr,
		 NULL);
	return XV_OK;
    }
    return XV_ERROR;
}

/*
 * This is used to determine whether or not a server object has been created
 * yet.
 */
Xv_private int xv_has_been_initialized(void)
{
    return (int) xv_initialized;
}
