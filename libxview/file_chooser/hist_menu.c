#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)hist_menu.c 1.9 93/06/28  DRA: RCS $Id: hist_menu.c,v 4.2 2024/05/22 18:17:59 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1992, 1993 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL_NOTICE file
 *	for terms of the license.
 */


#include <stdio.h>
#include <xview/xview.h>
#include <xview/panel.h>
#include <xview_private/i18n_impl.h>
#include <xview_private/hist_impl.h>

static Attr_attribute hist_key = 0;

static void hist_menu_notify_proc(Menu menu, Menu_item mi);
static Menu hist_menu_gen_proc(Menu menu, Menu_generate op);
static void hist_menu_done_proc(Menu menu, Xv_opaque result);



/*
 * xv_create() method
 */
static int hist_menu_init(Xv_opaque owner, History_menu self, Attr_avlist avlist, int *u)
{
	History_menu_public *public = (History_menu_public *) self;
	History_menu_private *private = xv_alloc(History_menu_private);

	if (!hist_key)
		hist_key = xv_unique_key();


	public->private_data = (Xv_opaque) private;
	private->public_self = (Xv_opaque) public;

	/*
	 * Initialize le' menu
	 */
	private->menu = xv_create(owner, MENU_COMMAND_MENU,
							MENU_NOTIFY_PROC, hist_menu_notify_proc,
							MENU_GEN_PROC, hist_menu_gen_proc,
							MENU_DONE_PROC, hist_menu_done_proc,
							MENU_DEFAULT, 1,
							XV_KEY_DATA, hist_key, private,
							NULL);

	return XV_OK;
}




/*
 * xv_set() method
 */
static Xv_opaque hist_menu_set(History_menu public, Attr_avlist avlist)
{
    History_menu_private *private = HIST_MENU_PRIVATE(public);
    Attr_avlist attrs;

    for (attrs=avlist; *attrs; attrs=attr_next(attrs)) {
	switch ( (int) attrs[0] ) {
	case HISTORY_MENU_OBJECT:
	    xv_error( public,
		     ERROR_CANNOT_SET,	attrs[0],
		     ERROR_PKG,		HISTORY_MENU,
		     NULL );
	    break;

	case HISTORY_NOTIFY_PROC:
	    ATTR_CONSUME(attrs[0]);
	    private->notify_proc = (void (*)(History_menu, char *,char *))attrs[1];
	    break;

#ifdef OW_I18N
	case HISTORY_NOTIFY_PROC_WCS:
	    ATTR_CONSUME(attrs[0]);
	    private->notify_proc_wcs = (void (*)()) attrs[1];
	    break;
#endif

	case HISTORY_MENU_HISTORY_LIST: {
	    ATTR_CONSUME(attrs[0]);

	    /* reference counting, yea! */
	    if ( private->list )
		xv_set(private->list, XV_DECREMENT_REF_COUNT, NULL);
	    private->list = (History_list) attrs[1];
	    if ( private->list )
		xv_set(private->list, XV_INCREMENT_REF_COUNT, NULL);
	    break;
	}

	case XV_END_CREATE:
	    break;

	default:
	    xv_check_bad_attr(HISTORY_MENU, attrs[0]);
	    break;
	} /* switch() */
    } /* for() */

    return XV_OK;
}



/*
 * xv_get() method
 */
static Xv_opaque hist_menu_get(History_menu public, int *status, Attr_attribute attr, va_list args)
{
	History_menu_private *private = HIST_MENU_PRIVATE(public);

	switch ((int)attr) {
		case HISTORY_MENU_OBJECT:
			return (Xv_opaque) private->menu;

		case HISTORY_MENU_HISTORY_LIST:
			return (Xv_opaque) private->list;

		case HISTORY_NOTIFY_PROC:
			return (Xv_opaque) private->notify_proc;

#ifdef OW_I18N
		case HISTORY_NOTIFY_PROC_WCS:
			return (Xv_opaque) private->notify_proc_wcs;
#endif

		default:
			*status = xv_check_bad_attr(HISTORY_MENU, attr);
			return (Xv_opaque) XV_OK;
	}  /* switch */

}



/*
 * xv_destroy() method
 */
static int hist_menu_destroy(History_menu public, Destroy_status status)
{
	History_menu_private *private = HIST_MENU_PRIVATE(public);


	if (status != DESTROY_CLEANUP) return XV_OK;


	/*
	 * Make sure menu is cleared out.  Note that the Panel Button
	 * doesn't call the MENU_DONE_PROC if it generated a menu to
	 * get the default value, but never showed it.  Is this a bug?
	 */
	hist_menu_done_proc(private->menu, XV_NULL);


	if (private->list) {
		xv_set(private->list, XV_DECREMENT_REF_COUNT, NULL);
		xv_destroy(private->list);
	}

	if (private->menu)
		xv_destroy(private->menu);

	xv_free(private);

	return XV_OK;
}


/*******************************************************************************/



/*
 * MENU_NOTIFY_PROC for history menu.  notify user of selection.
 */
static void hist_menu_notify_proc(Menu menu, Menu_item mi)
{
    char *label = (char *)xv_get(mi, MENU_STRING);
    History_menu_private *private = (History_menu_private *)xv_get(menu,
												XV_KEY_DATA, hist_key);
    char *value = (char *)xv_get(private->list, HISTORY_VALUE_FROM_MENUITEM,mi);

#ifdef OW_I18N
    if ( private->notify_proc_wcs ) {
	wchar_t *value_wcs = _xv_mbstowcsdup( value );
	wchar_t *label_wcs = _xv_mbstowcsdup( label );

	(* private->notify_proc)( HIST_MENU_PUBLIC(private), label_wcs, value_wcs );
	xv_free( label_wcs );
	xv_free( value_wcs );
    } else
#endif
    if ( private->notify_proc )
	(* private->notify_proc)( HIST_MENU_PUBLIC(private), label, value );

    xv_set(menu, MENU_NOTIFY_STATUS, XV_ERROR, NULL);
}




/*
 * MENU_DONE_PROC, clean up after display of history menu.
 */
static void hist_menu_done_proc(Menu menu, Xv_opaque result)
{
    int items = (int) xv_get(menu, MENU_NITEMS);
    int ii;


    /*
     * note:  remove items from menu, but don't destroy
     * them, as they may be shared with another menu.
     * destroying the History_list will do this.
     */
    for(ii=items; ii>0; --ii)
	xv_set(menu, MENU_REMOVE, ii, NULL);
}




/*
 * MENU_GEN_PROC for history menu.  menu items are installed
 * and destroyed (see history_menu_done_proc) on the fly so as
 * to implement the OL perscribed behavior of the rolling
 * stack of path names.
 */
static Menu hist_menu_gen_proc(Menu menu, Menu_generate op)
{
	History_menu_private *private
			= (History_menu_private *) xv_get(menu, XV_KEY_DATA, hist_key);


	if (op != MENU_DISPLAY)
		return menu;

	/*
	 * Make sure menu is cleared out.  Note that the Panel Button
	 * doesn't call the MENU_DONE_PROC if it generated a menu to
	 * get the default value, but never showed it.  Is this a bug?
	 */
	hist_menu_done_proc(menu, XV_NULL);

	/* tell History_list to put Menu_items into our Menu */
	if (private->list)
		xv_set(private->list, HISTORY_POPULATE_MENU, private->menu, NULL);

	return menu;
}

Xv_pkg history_menu_pkg = {
    "History Menu",
    ATTR_PKG_HIST,
    sizeof(History_menu_public),
    XV_GENERIC_OBJECT,
    hist_menu_init,
    hist_menu_set,
    hist_menu_get,
    hist_menu_destroy,
    NULL			/* no find */
};
