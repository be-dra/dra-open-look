#ifndef lint
char     txt_load_c_sccsid[] = "@(#)txt_load.c 1.37 93/06/28 DRA: $Id: txt_load.c,v 4.5 2025/01/01 20:35:21 dra Exp $";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Text load popup frame creation and support.
 */

#include <xview_private/primal.h>
#include <xview_private/txt_impl.h>
#include <xview_private/ev_impl.h>
#include <sys/time.h>
#include <sys/param.h>
#include <signal.h>
#include <unistd.h>
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

#ifdef SVR4
#include <string.h>
#endif /* SVR4 */
 
#define		MAX_DISPLAY_LENGTH	50

typedef enum {
    FILE_CMD_ITEM = 0,
    DIR_STRING_ITEM = 1,
    FILE_STRING_ITEM = 2
}               File_panel_item_enum;

Pkg_private int open_cmd_proc(Frame fc, CHAR *path, CHAR *file, Xv_opaque client_data)
{

    Textsw_view_private view  = (Textsw_view_private)xv_get(fc,WIN_CLIENT_DATA,0);
    Textsw_private       priv = TSWPRIV_FOR_VIEWPRIV(view);
    Textsw          textsw = TEXTSW_PUBLIC(priv);
    CHAR           *dir_str;
    int             result;
    register int    locx, locy;
    Frame           frame = xv_get(TEXTSW_PUBLIC(priv), WIN_FRAME);
    Xv_Notice       text_notice;
    char            curr_dir[MAXPATHLEN];
#ifdef OW_I18N
    CHAR            curr_dir_ws[MAXPATHLEN];
#endif


    if (textsw_has_been_modified(textsw)) {
        text_notice = xv_get(frame, XV_KEY_DATA, text_notice_key);
        if (!text_notice)  {
            text_notice = xv_create(frame, NOTICE, NULL);

            xv_set(frame, XV_KEY_DATA, text_notice_key, text_notice, NULL);
        }
		xv_set(text_notice, 
			NOTICE_MESSAGE_STRINGS,
			XV_MSG("The text has been edited.\n\
Load File will discard these edits. Please confirm."),
			NULL,
			NOTICE_BUTTON_YES, 
					XV_MSG("Confirm, discard edits"),
			NOTICE_BUTTON_NO, XV_MSG("Cancel"),
			NOTICE_STATUS, &result,
			XV_SHOW, TRUE, 
			NULL);

        if (result == NOTICE_NO || result == NOTICE_FAILED)
            return XV_OK;
    }
#ifdef OW_I18N
    dir_str = (CHAR *) xv_get(fc, FILE_CHOOSER_DIRECTORY_WCS);
#else
    dir_str = (char *) xv_get(fc, FILE_CHOOSER_DIRECTORY);
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
 
    /* if "cd" is not disabled */
    (void) getcwd(curr_dir, (long)MAXPATHLEN);
#ifdef OW_I18N
    (void) mbstowcs(curr_dir_ws, curr_dir, MAXPATHLEN);
    if (STRCMP(curr_dir_ws, dir_str) != 0) {    /* } for match */
#else
    if (strcmp(curr_dir, dir_str) != 0) {
#endif
        if (!(priv->state & TXTSW_NO_CD)) {
            if (textsw_change_directory(priv, dir_str, FALSE, locx, locy) != 0) {
                /* error or directory does not exist */
                return TRUE;
            }
        } else {
			frame = xv_get(TEXTSW_PUBLIC(priv), WIN_FRAME);
 
            text_notice = (Xv_Notice)xv_get(frame, XV_KEY_DATA,text_notice_key);
 
            if (!text_notice)  {
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
        result = textsw_load_file(textsw, file, TRUE, 0, 0);
        if (result == 0) {
            textsw_set_insert(priv, 0);
            (void) xv_set(fc, XV_SHOW, FALSE, NULL);
            return FALSE;
        }
        /* error */
        return TRUE;
    }
 
    text_notice = (Xv_Notice)xv_get(frame, XV_KEY_DATA, text_notice_key);
 
    if (!text_notice)  {
        text_notice = xv_create(frame, NOTICE, NULL);
 
        xv_set(frame, XV_KEY_DATA, text_notice_key, text_notice, NULL);
    }
	xv_set(text_notice,
		NOTICE_MESSAGE_STRINGS,
		XV_MSG("No file name was specified.\n\
Specify a file name to Load."),
		NULL,
		NOTICE_BUTTON_YES, XV_MSG("Continue"),
		XV_SHOW, TRUE,
		NULL);

    /* if we made it here, there was an error or notice */
    return TRUE;
}
   

Pkg_private int save_cmd_proc(Frame fc, CHAR *path, struct stat * exists)
{
   Textsw_view_private view  = (Textsw_view_private)xv_get(fc,WIN_CLIENT_DATA);
   Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
   int		confirm_state_changed = 0;

   xv_set(fc,
          FRAME_SHOW_FOOTER, TRUE,
	  FRAME_LEFT_FOOTER,"Saved",
	  NULL);

   if (priv->state & TXTSW_CONFIRM_OVERWRITE) {
	priv->state &= ~TXTSW_CONFIRM_OVERWRITE;
	confirm_state_changed = 1;
   }
#ifdef OW_I18N
   textsw_store_file_wcs(VIEW_PUBLIC(view),path,0,0);
#else
   textsw_store_file(TEXTSW_PUBLIC(priv), path, 0, 0);
#endif
    if (confirm_state_changed)
	priv->state |= TXTSW_CONFIRM_OVERWRITE;
    return XV_OK;
}
