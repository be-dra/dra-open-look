/*      @(#)path_impl.h 1.7 93/06/28 SMI  DRA: RCS $Id: path_impl.h,v 4.2 2024/05/22 18:17:49 dra Exp $      */

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL_NOTICE
 *	file for terms of the license.
 */

#ifndef _xview_path_impl_included
#define _xview_path_impl_included 1

#include <xview_private/xv_path_util.h>
#include <xview/path.h>


typedef struct {
    Xv_opaque		public_self;
    Frame		frame;
    Panel_setting	(* client_notify)(Panel_item, Event *, struct stat *);
    char *		valid_path;		/* last valid path name seen */
    char *		relative; 		/* accept paths relative to this one */
    int			notify_status;		/* notify proc succeeded or failed */
    unsigned		is_directory : 1; 	/* allow file or dir names */
    unsigned		use_frame : 1;		/* put messages in frame */
    unsigned		new_file : 1;		/* allow new file name */

#ifdef OW_I18N
    wchar_t *		valid_path_wcs;
    wchar_t *		relative_wcs;
#endif /* OW_I18N */
} Path_private;

#define PATH_PUBLIC(item)	XV_PUBLIC(item)
#define PATH_PRIVATE(item)	XV_PRIVATE(Path_private, Path_public, item)

Pkg_private Panel_setting xv_path_name_notify_proc(Path_name item, Event *event);

#endif /* _xview_path_impl_included */
