#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)frame_base.c 1.31 93/06/28 DRA: $Id: frame_base.c,v 4.7 2026/04/17 08:43:08 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

#include <X11/Xlib.h>
#include <xview_private/fm_impl.h>
#include <xview_private/draw_impl.h>
#include <xview/cursor.h>
#include <xview/server.h>
#include <xview_private/svr_atom.h>
#include <xview_private/wmgr_decor.h>
#include <xview_private/svr_impl.h>

#include <X11/Xatom.h>

#define	FRAME_BASE_PRIVATE(f)	XV_PRIVATE(Frame_base_info, Xv_frame_base, f)
#define FRAME_BASE_PUBLIC(f)	XV_PUBLIC(f)
#define FRAME_CLASS_FROM_BASE(f) FRAME_PRIVATE(FRAME_BASE_PUBLIC(f))

typedef	struct	{
    Frame		 public_self;	/* back pointer to object */
	frame_props_proc_t props_proc;
    WM_Win_Type		 win_attr;	/* _OL_WIN_ATTR */
    char		**cmd_line_strings;
    int			cmd_line_strings_count;
    struct {
		BIT_FIELD(props_active);
    } status_bits;
} Frame_base_info;


#if defined(WITH_3X_LIBC) || defined(vax)
/* 3.x - 4.0 libc transition code; old (pre-4.0) code must define the symbol */
#define jcsetpgrp(p)  setpgrp((p),(p))
#endif

static int frame_base_init(Xv_Window owner, Frame frame_public,
						Attr_attribute avlist[], int *u)
{
    Xv_frame_base  *frame_object = (Xv_frame_base *) frame_public;
    Xv_Drawable_info *info;
    Xv_opaque       server_public;
    Frame_base_info *frame;
    Attr_avlist      attrs;

    DRAWABLE_INFO_MACRO(frame_public, info);
    server_public = xv_server(info);
    frame = xv_alloc(Frame_base_info);

    /* link to object */
    frame_object->private_data = (Xv_opaque) frame;
    frame->public_self = frame_public;

    /* set saved command line strings to NULL */
    frame->cmd_line_strings = (char **)NULL;
    frame->cmd_line_strings_count = 0;

    /* set initial window decoration flags */

    frame->win_attr.flags = WMWinType | WMMenuType;
    frame->win_attr.win_type = (Atom)xv_get(server_public,SERVER_WM_WT_BASE);
    frame->win_attr.menu_type = (Atom)xv_get(server_public,SERVER_WM_MENU_FULL);

    status_set(frame, props_active, FALSE);

    for (attrs = avlist; *attrs; attrs = attr_next(attrs)) {
	switch (*attrs) {

	  case FRAME_SCALE_STATE:
	    /*
	     * change scale and find the apprioprate size of the font Don't
	     * call frame_rescale_subwindows because they have not been
	     * created yet.
	     */
	    wmgr_set_rescale_state(frame_public, (int)attrs[1]);
	    break;
	  default:
	    break;
	}
    }

    /*
     * _SUN_OL_WIN_ATTR_5 is an atom hung off the _SUN_WM_PROTOCOLS 
     * property on the root window. It's presence indicates that the wmgr
	 * is using the 5-length _OL_WIN_ATTR property. XView now uses the
	 * 5-length property by default. The wmgr will detect this, and will draw
	 * the labels in XView icons.
     * The following code is to prevent this from happening.
     */
	if (xv_get(xv_screen(info), SCREEN_CHECK_SUN_WM_PROTOCOL,
						"_SUN_OL_WIN_ATTR_5"))
	{
        int             delete_decor = 0;
        Atom            delete_decor_list[WM_MAX_DECOR];

        /*
         * Tell wmgr not to draw icon labels - for now this will be done
         * by XView
         */
        delete_decor_list[delete_decor++] =
		(Atom) xv_get(server_public, SERVER_ATOM, "_OL_DECOR_ICON_NAME");
        wmgr_delete_decor(frame_public, delete_decor_list, delete_decor);
    }

    return XV_OK;
}

static Xv_opaque frame_base_set_avlist(Frame frame_public,
										Attr_attribute avlist[])
{
	Attr_avlist attrs;
	Frame_base_info *frame = FRAME_BASE_PRIVATE(frame_public);
	int result = XV_OK;
	char **cmd_line = NULL;
	int cmd_line_count = 0;

	for (attrs = avlist; *attrs; attrs = attr_next(attrs)) {
		switch (attrs[0]) {

			case FRAME_WM_COMMAND_STRINGS:
				attrs[0] = (Frame_attribute) ATTR_NOP(attrs[0]);
				if ((int)attrs[1] == -1) {
					cmd_line = (char **)-1;
					cmd_line_count = 0;
				}
				else {
					cmd_line = (char **)&attrs[1];

					/* count strings */
					for (cmd_line_count = 0;
							cmd_line[cmd_line_count]; ++cmd_line_count);

				}
				break;

			case FRAME_WM_COMMAND_ARGC_ARGV:
				attrs[0] = (Frame_attribute) ATTR_NOP(attrs[0]);
				cmd_line_count = (int)attrs[1];
				cmd_line = (char **)attrs[2];
				break;

			case FRAME_PROPERTIES_PROC:
				attrs[0] = (Frame_attribute) ATTR_NOP(attrs[0]);
				frame->props_proc = (frame_props_proc_t)attrs[1];

				/* This props_active is a sunview carry over.
				 * If we decide to add FRAME_PROPS_ACTIVE later, to 
				 * activate the "props" menu item, this placed there
				 */

				status_set(frame, props_active, TRUE);

				break;

			case FRAME_SCALE_STATE:
				attrs[0] = (Frame_attribute) ATTR_NOP(attrs[0]);
				/*
				 * set the local rescale state bit, then tell the WM the current
				 * state, and then set the scale of our subwindows
				 */
				/*
				 * WAIT FOR NAYEEM window_set_rescale_state(frame_public,
				 * attrs[1]);
				 */
				wmgr_set_rescale_state(frame_public, (int)attrs[1]);
				frame_rescale_subwindows(frame_public, (int)attrs[1]);
				break;

			case XV_LABEL:
				{
					Frame_class_info *frame_class =
							FRAME_CLASS_FROM_BASE(frame);

					*attrs = (Frame_attribute) ATTR_NOP(*attrs);

#ifdef OW_I18N
					_xv_set_mbs_attr_dup(&frame_class->label, (char *)attrs[1]);
#else
					if (frame_class->label)
						free(frame_class->label);
					if ((char *)attrs[1]) {
						frame_class->label = (char *)
								xv_calloc(1,
								(unsigned)strlen((char *)attrs[1]) + 1);
						strcpy(frame_class->label, (char *)attrs[1]);
					}
					else {
						frame_class->label = NULL;
					}
#endif /* OW_I18N */

					(void)frame_display_label(frame_class);
				}
				break;

#ifdef OW_I18N
			case XV_LABEL_WCS:
				{
					Frame_class_info *frame_class =
							FRAME_CLASS_FROM_BASE(frame);

					*attrs = (Frame_attribute) ATTR_NOP(*attrs);
					_xv_set_wcs_attr_dup(&frame_class->label,
							(wchar_t *)attrs[1]);
					(void)frame_display_label(frame_class);
				}
				break;
#endif /* OW_I18N */

			case XV_SET_POPUP:
				server_set_popup(frame_public, (Attr_attribute *)&attrs[1]);
				ATTR_CONSUME(*attrs);
				break;

			case XV_END_CREATE:
				(void)wmgr_set_win_attr(frame_public, &(frame->win_attr));
				break;

			default:
				break;

		}
	}


	/* If command line strings specified, cache them on object */
	if (cmd_line) {
		int i;

		/*
		 * If old command line strings exists, free them
		 */
		if (frame->cmd_line_strings_count > 0) {
			char **old_strings = frame->cmd_line_strings;

			for (i = 0; i < frame->cmd_line_strings_count; ++i) {
				if (old_strings[i]) {
					free(old_strings[i]);
				}
			}

#ifndef OW_I18N
			/*
			 * Free array holding strings
			 */
			free(old_strings);
#endif
		}

		/*
		 * Check if special flag -1 passed
		 * If yes, set string count to 0
		 * Otherwise, save passed strings 
		 */
		if ((long)cmd_line == -1) {
			frame->cmd_line_strings_count = 0;
			frame->cmd_line_strings = (char **)-1;
		}
		else {
			/*
			 * Check count
			 */
			if (cmd_line_count < 0) {
				cmd_line_count = 0;
			}

			/*
			 * Set count to new string count
			 */
			frame->cmd_line_strings_count = cmd_line_count;

			/*
			 * Allocate array to hold strings
			 */
			frame->cmd_line_strings =
					(char **)xv_calloc((unsigned)cmd_line_count,
					(unsigned)sizeof(char *));

			/*
			 * Copy strings passed in one by one
			 */
			for (i = 0; i < cmd_line_count; ++i) {
				frame->cmd_line_strings[i] = strdup(cmd_line[i]);
			}
		}
	}

	return (Xv_opaque) result;
}

static Xv_opaque frame_base_get_attr(Frame frame_public, int *status,
								Attr_attribute attr, va_list valist)
{
	register Frame_base_info *frame = FRAME_BASE_PRIVATE(frame_public);

	switch (attr) {

		case FRAME_WM_COMMAND_ARGV:
			attr = (Frame_attribute) ATTR_NOP(attr);
			return (Xv_opaque) frame->cmd_line_strings;

		case FRAME_WM_COMMAND_ARGC:
			attr = (Frame_attribute) ATTR_NOP(attr);
			return (Xv_opaque) frame->cmd_line_strings_count;

		case FRAME_PROPERTIES_PROC:
			attr = (Frame_attribute) ATTR_NOP(attr);
			return (Xv_opaque) frame->props_proc;

		case FRAME_SCALE_STATE:
			attr = (Frame_attribute) ATTR_NOP(attr);
			/*
			 * WAIT FOR NAYEEM return (Xv_opaque)
			 * window_get_rescale_state(frame_public);
			 */
			return (Xv_opaque) 0;

		default:
			*status = XV_ERROR;
			return (Xv_opaque) 0;
	}
}

/*
 * free the frame struct and all its resources.
 */
static void frame_base_free(Frame_base_info *frame)
{
    /* Free frame struct */
    free((char *) frame);
}

/* Destroy the frame struct */
static int frame_base_destroy(Frame frame_public, Destroy_status status)
{
	Frame_base_info *frame = FRAME_BASE_PRIVATE(frame_public);

	if (status == DESTROY_CLEANUP) {	/* waste of time if ...PROCESS_DEATH */
		/*
		 * If have saved command line strings, free them
		 */
		if (frame->cmd_line_strings_count > 0) {
			char **old_strings = frame->cmd_line_strings;
			int i;

			for (i = 0; i < frame->cmd_line_strings_count; ++i) {
				if (old_strings[i]) {
					free(old_strings[i]);
				}
			}

			/*
			 * Free array holding strings
			 */
			free(old_strings);
		}

		frame_base_free(frame);
	}
	return XV_OK;
}

Xv_private void frame_handle_props(Frame frame_public)
{
	Frame_base_info *frame = FRAME_BASE_PRIVATE(frame_public);

	if (frame->props_proc && status_get(frame, props_active)) {
		(frame->props_proc) (frame_public);
	}
}

const Xv_pkg          xv_frame_base_pkg = {
    "Frame_base", (Attr_pkg) ATTR_PKG_FRAME,
    sizeof(Xv_frame_base),
    FRAME_CLASS,
    frame_base_init,
    frame_base_set_avlist,
    frame_base_get_attr,
    frame_base_destroy,
    NULL			/* no find proc */
};
