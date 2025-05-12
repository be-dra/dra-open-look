#ifndef lint
char     txt_incl_c_sccsid[] = "@(#)txt_incl.c 1.35 93/06/28 DRA: $Id: txt_incl.c,v 4.7 2025/05/11 12:58:23 dra Exp $";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Text include popup frame creation and support.
 */

#include <xview_private/primal.h>
#include <xview_private/txt_impl.h>
#include <xview_private/ev_impl.h>
#include <sys/time.h>
#include <sys/param.h>
#include <signal.h>
#include <xview/notice.h>
#include <xview/frame.h>
#include <xview/panel.h>
#include <xview/textsw.h>
#include <xview/openmenu.h>
#include <xview/wmgr.h>
#include <xview/pixwin.h>
#include <xview/win_struct.h>
#include <xview/win_screen.h>
#include <xview/file_chsr.h>

#include <unistd.h>
#include <string.h>

#define		MAX_DISPLAY_LENGTH	50

typedef enum {
    FILE_CMD_ITEM = 0,
    DIR_STRING_ITEM = 1,
    FILE_STRING_ITEM = 2
}               File_panel_item_enum;

Pkg_private Panel_item include_panel_items[];


Pkg_private int textsw_include_cmd_proc(Frame fc, CHAR *path, CHAR *file, Xv_opaque client_data)
{

	Textsw_view_private view =
			(Textsw_view_private) window_get(fc, WIN_CLIENT_DATA, NULL);
	Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
	CHAR *dir_str;
	register int locx, locy;
	CHAR curr_dir[MAXPATHLEN];
#ifdef OW_I18N
	char curr_dir_mb[MAXPATHLEN];
#endif
	Frame frame;
	Xv_Notice text_notice;
	int textsw_changed_directory;

#ifdef OW_I18N
	dir_str = (CHAR *) xv_get(fc, FILE_CHOOSER_DIRECTORY_WCS);
#else
	dir_str = (char *)xv_get(fc, FILE_CHOOSER_DIRECTORY);
#endif

	locx = locy = 0;

#ifdef OW_I18N
	if (textsw_expand_filename(priv, dir_str, MAXPATHLEN, locx, locy)) {
		/* error handled inside routine */
		return TRUE;
	}
	if (textsw_expand_filename(priv, file, MAXPATHLEN, locx, locy)) {
		/* error handled inside routine */
		return TRUE;
	}
#else
#endif

	/* if "cd" is not disabled and the "cd" dir is not the current dir */

#ifdef OW_I18N
	(void)getcwd(curr_dir_mb, MAXPATHLEN);
	(void)mbstowcs(curr_dir, curr_dir_mb, MAXPATHLEN);
#else /* OW_I18N */
	(void)getcwd(curr_dir, (long)MAXPATHLEN);
#endif /* OW_I18N */

	textsw_changed_directory = FALSE;
	if (STRCMP(curr_dir, dir_str) != 0) {
		if (!(priv->state & TXTSW_NO_CD)) {
			if (textsw_change_directory(priv, dir_str, FALSE, locx, locy) != 0)
				return TRUE;
			else
				/*
				 * kludge to prevent including file from changing
				 * your directory too.
				 */
				textsw_changed_directory = TRUE;
		}
		else {
			frame = xv_get(TEXTSW_PUBLIC(priv), WIN_FRAME);
			text_notice = xv_get(frame, XV_KEY_DATA, text_notice_key);
			if (!text_notice) {
				text_notice = xv_create(frame, NOTICE, NULL);

				xv_set(frame, XV_KEY_DATA, text_notice_key, text_notice, NULL);
			}
			xv_set(text_notice,
					NOTICE_MESSAGE_STRINGS,
						XV_MSG("Cannot change directory.\n\
Change Directory has been disabled."), 
						NULL,
					NOTICE_BUTTON_YES, XV_MSG("Continue"),
					XV_SHOW, TRUE,
					NULL);
			return TRUE;
		}
	}
	if ((int)STRLEN(file) > 0) {
		if (textsw_file_stuff_from_str(VIEW_PRIVATE(xv_get(XV_PUBLIC(priv),
										OPENWIN_NTH_VIEW, 0)), file, locx,
						locy) == 0) {

			(void)xv_set(fc, XV_SHOW, FALSE, NULL);
			if (textsw_changed_directory)
				textsw_change_directory(priv, curr_dir, FALSE, locx, locy);
			return FALSE;
		}
		if (textsw_changed_directory)
			textsw_change_directory(priv, curr_dir, FALSE, locx, locy);
		return TRUE;
	}

	frame = xv_get(TEXTSW_PUBLIC(priv), WIN_FRAME);
	text_notice = xv_get(frame, XV_KEY_DATA, text_notice_key);
	if (!text_notice) {
		text_notice = xv_create(frame, NOTICE, NULL);

		xv_set(frame, XV_KEY_DATA, text_notice_key, text_notice, NULL);
	}
	xv_set(text_notice,
			NOTICE_MESSAGE_STRINGS,
				XV_MSG("No file name was specified.\n\
Specify a file name to Include File."), 
				NULL,
			NOTICE_BUTTON_YES, XV_MSG("Continue"),
			XV_SHOW, TRUE,
			NULL);
	if (textsw_changed_directory)
		textsw_change_directory(priv, curr_dir, FALSE, locx, locy);
	/* if we made it here then there was an error or notice */
	return TRUE;
}
