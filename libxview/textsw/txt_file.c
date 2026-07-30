#ifndef lint
char     txt_file_c_sccsid[] = "@(#)txt_file.c 20.81 93/06/28 DRA: $Id: txt_file.c,v 4.15 2026/07/30 07:13:04 dra Exp $";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */

/*
 * File load/save/store utilities for text subwindows.
 */

#include <xview_private/primal.h>
#include <xview_private/txt_impl.h>
#include <xview_private/ev_impl.h>
#include <xview_private/txt_18impl.h>
#include <xview_private/svr_impl.h>
#include <unistd.h>
#include <assert.h>
#ifdef SVR4
#include <dirent.h>
#include <string.h>
#ifdef DRA_IRIX
#include <fcntl.h>
#endif /* DRA_IRIX */
#else
#include <sys/dir.h>
#include <sys/file.h>
#endif /* SVR4 */
#include <sys/stat.h>
/*
 * Following two #undefs are because Ultrix sys/param.h doesn't check to see
 * if MIN and MAX are already defined
 */
#undef MIN
#undef MAX
#include <sys/param.h>
#include <xview/notice.h>
#include <xview/frame.h>
#include <errno.h>
#include <xview/expandname.h>

#define SET_BOOL_FLAG(flags, to_test, flag)			\
	if ((unsigned)(to_test) != 0) (flags) |= (flag);	\
	else (flags) &= ~(flag)

/* extern CHAR    *STRCAT(CHAR *, CHAR *); */
/* extern char    *getcwd(); */

Pkg_private void textsw_set_dir_str(int popup_type);
Pkg_private void textsw_create_popup_frame(Textsw_view_private view, int popup_type);

#define ES_BACKUP_FAILED	ES_CLIENT_STATUS(0)
#define ES_BACKUP_OUT_OF_SPACE	ES_CLIENT_STATUS(1)
#define ES_CANNOT_GET_NAME	ES_CLIENT_STATUS(2)
#define ES_CANNOT_OPEN_OUTPUT	ES_CLIENT_STATUS(3)
#define ES_CANNOT_OVERWRITE	ES_CLIENT_STATUS(4)
#define ES_DID_NOT_CHECKPOINT	ES_CLIENT_STATUS(5)
#define ES_PIECE_FAIL		ES_CLIENT_STATUS(6)
#define ES_SHORT_READ		ES_CLIENT_STATUS(7)
#define ES_UNKNOWN_ERROR	ES_CLIENT_STATUS(8)
#define ES_USE_SAVE		ES_CLIENT_STATUS(9)

#define TXTSW_LRSS_MASK		0x00000007

Pkg_private     Es_handle textsw_create_ps(Textsw_private    priv, Es_handle original, Es_handle scratch, Es_status      *status)
{
    register Es_handle result;

    result = priv->es_create(priv->client_data, original, scratch);
    if (result) {
	*status = ES_SUCCESS;
    } else {
	es_destroy(original);
	es_destroy(scratch);
	*status = ES_PIECE_FAIL;
    }
    return (result);
}

static void textsw_make_temp_name(CHAR *in_here)
/* *in_here must be at least MAXNAMLEN characters long */
{
	static unsigned tmtn_counter;
    /*
     * BUG ALERT!  Should be able to specify directory other than /tmp.
     * However, if that directory is not /tmp it should be a local (not NFS)
     * directory and will make finding files that need to be replayed after
     * crash harder - assuming we ever implement replay.
     */
    in_here[0] = '\0';
    SPRINTF(in_here, "%s/Text%d.%d", "/tmp", getpid(), tmtn_counter++);
}

Pkg_private Es_handle textsw_create_file_ps(Textsw_private priv, char *name,
							char *scratch_name, Es_status *status)
/* *scratch_name must be at least MAXNAMLEN characters long, and is modified */
{
    register Es_handle original_esh, scratch_esh, piece_esh;

    original_esh = es_file_create(name, 0, status);
    if (!original_esh)
	return (ES_NULL);
    textsw_make_temp_name(scratch_name);
    scratch_esh = es_file_create(scratch_name,
				 ES_OPT_APPEND | ES_OPT_OVERWRITE,
				 status);
    if (!scratch_esh) {
	es_destroy(original_esh);
	return (ES_NULL);
    }
    (void) es_set(scratch_esh, ES_FILE_MODE, 0600, NULL);
    piece_esh = textsw_create_ps(priv, original_esh, scratch_esh, status);
    (void) unlink(scratch_name);
    return (piece_esh);
}

#define	TXTSW_LFI_CLEAR_SELECTIONS	0x1

Pkg_private Es_status textsw_load_file_internal(Textsw_private priv,
    CHAR *name, CHAR *scratch_name,
    Es_handle *piece_esh,
    Es_index  start_at,
    int flags)
/* *scratch_name must be at least MAXNAMLEN characters long, and is modified */
{
	Es_status status;

	textsw_take_down_caret(priv);
	/* if there is a temp filename, then this is a termsw and therefore
	   we must clean up the temp_filename correctly, so we need to create
	   the file first, then possibly load it in, and then unlink() it
	   to clean it up */
	if (priv->temp_filename) {
		int fd;

		unlink(priv->temp_filename);
		fd = open(priv->temp_filename, O_CREAT | O_RDWR, 0600);
		close(fd);
	}
	*piece_esh = textsw_create_file_ps(priv, name, scratch_name, &status);
	if (priv->temp_filename) {
		unlink(priv->temp_filename);
	}
	if (status == ES_SUCCESS) {
		if (flags & TXTSW_LFI_CLEAR_SELECTIONS) {

			Textsw abstract = TEXTSW_PUBLIC(priv);

			textsw_set_selection(abstract, ES_INFINITY, ES_INFINITY,
					EV_SEL_PRIMARY);
			textsw_set_selection(abstract, ES_INFINITY, ES_INFINITY,
					EV_SEL_SECONDARY);
		}
		textsw_replace_esh(priv, *piece_esh);
		if (start_at != ES_CANNOT_SET) {

			(void)ev_set(priv->views->first_view,
					EV_FOR_ALL_VIEWS,
					EV_DISPLAY_LEVEL, EV_DISPLAY_NONE,
					EV_DISPLAY_START, start_at,
					EV_DISPLAY_LEVEL, EV_DISPLAY, NULL);

			textsw_notify(VIEW_PRIVATE(xv_get(XV_PUBLIC(priv), OPENWIN_NTH_VIEW,
									0)), TEXTSW_ACTION_LOADED_FILE, name, NULL);
			textsw_update_scrollbars(priv, TEXTSW_VIEW_NULL);
		}
	}
	return (status);
}

Pkg_private void textsw_destroy_esh(Textsw_private priv, Es_handle esh)
{
	Es_handle original_esh, scratch_esh;

	if (priv->views->esh == esh)
		priv->views->esh = ES_NULL;
	if ((original_esh = (Es_handle) es_get(esh, ES_PS_ORIGINAL))) {
		scratch_esh = (Es_handle) es_get(esh, ES_PS_SCRATCH);
		es_destroy(original_esh);
		if (scratch_esh)
			es_destroy(scratch_esh);
	}
	es_destroy(esh);
}

Pkg_private void textsw_replace_esh(Textsw_private priv, Es_handle new_esh)
/* Caller is repsonsible for actually repainting the views. */
{
	Es_handle save_esh = priv->views->esh;
	int undo_count = priv->undo_count;

	(void)ev_set(priv->views->first_view,
			EV_DISPLAY_LEVEL, EV_DISPLAY_NONE, EV_CHAIN_ESH, new_esh, NULL);
	priv->state &= ~TXTSW_EDITED;
	textsw_destroy_esh(priv, save_esh);
	/* Following two calls are inefficient textsw_re-init_undo. */
	textsw_init_undo(priv, 0);
	textsw_init_undo(priv, undo_count);

	priv->state &= ~TXTSW_READ_ONLY_ESH;
	if (priv->notify_level & TEXTSW_NOTIFY_SCROLL) {
		Textsw tsw = TEXTSW_PUBLIC(priv);
		Textsw_view vp;

		OPENWIN_EACH_VIEW(tsw, vp)
			textsw_display_view_margins(VIEW_PRIVATE(vp), RECT_NULL);
		OPENWIN_END_EACH
	}
}

Pkg_private     Es_handle textsw_create_mem_ps(Textsw_private priv, Es_handle original)
{
    register Es_handle scratch;
    Es_status       status;
    Es_handle       ps_esh = ES_NULL;

    if (original == ES_NULL)
	goto Return;
    scratch = es_mem_create(priv->es_mem_maximum, "");
    if (scratch == ES_NULL) {
	es_destroy(original);
	goto Return;
    }
    ps_esh = textsw_create_ps(priv, original, scratch, &status);
Return:
    return (ps_esh);
}

static CHAR * textsw_full_pathname(CHAR *name)
{
    CHAR            pathname[MAXPATHLEN];
    register CHAR  *full_pathname;

    if (name == 0)
	return (name);
    if (*name == '/') {
	if ((full_pathname = MALLOC((size_t) (1 + STRLEN(name)))) == 0)
	    return (0);
	(void) STRCPY(full_pathname, name);
	return (full_pathname);
    }

    if (getcwd(pathname, sizeof(pathname)) == 0) return (0);

    if ((full_pathname =
	 MALLOC((2 + STRLEN(pathname) + STRLEN(name)))) == 0)
	return (0);
    (void) STRCPY(full_pathname, pathname);
    (void) strcat(full_pathname, "/");
    (void) STRCAT(full_pathname, name);
    return (full_pathname);
}

/* ARGSUSED */
Pkg_private void textsw_format_load_error(char *msg, Es_status status, CHAR *filename, CHAR *scratch_name)
{
	CHAR *full_pathname;

	switch (status) {
		case ES_PIECE_FAIL:
			(void)sprintf(msg, XV_MSG("Cannot create piece stream."));
			break;
		case ES_SUCCESS:
			/* Caller is being lazy! */
			break;
		default:
			full_pathname = textsw_full_pathname(filename);
			(void)sprintf(msg, XV_MSG("Cannot load; "));

			es_file_append_error(msg, XV_MSG("file"), status);
			es_file_append_error(msg, full_pathname, status);
			free(full_pathname);
			break;
	}
}

static void textsw_format_load_error_quietly(char *msg, Es_status status, CHAR *filename, CHAR *scratch_name)
{
    CHAR           *full_pathname;

    switch (status) {
      case ES_PIECE_FAIL:
	(void) sprintf(msg,
		       XV_MSG("INTERNAL ERROR: Cannot create piece stream."));
	break;
      case ES_SUCCESS:
	/* Caller is being lazy! */
	break;
      default:
	full_pathname = textsw_full_pathname(filename);
	(void) sprintf(msg, XV_MSG("Unable to load file:"));
	es_file_append_error(msg, full_pathname, status);
	free(full_pathname);
	break;
    }
}

/* Returns 0 iff load succeeded. */
Pkg_private int textsw_load_file(Textsw abstract, CHAR *filename,
    int reset_views, int locx, int locy)
{
	char notice_msg_buf[MAXNAMLEN + 100];
	CHAR scratch_name[MAXNAMLEN];
	Es_status status;
	Es_handle new_esh;
	Es_index start_at;
	Frame frame;
	Xv_Notice text_notice;
	Textsw_private priv;

	assert(xv_get(abstract, XV_IS_SUBTYPE_OF, OPENWIN));
	priv = TEXTSW_PRIVATE(abstract);

	start_at = (reset_views) ? 0 : ES_CANNOT_SET;
	status = textsw_load_file_internal(priv, filename, scratch_name, &new_esh,
			start_at, TXTSW_LFI_CLEAR_SELECTIONS);
	if (status == ES_SUCCESS) {
		if (start_at == ES_CANNOT_SET) {
			textsw_notify((Textsw_view_private) priv,	/* Cast for lint */
					TEXTSW_ACTION_LOADED_FILE, filename, NULL);
		}
	}
	else {
		textsw_format_load_error_quietly(notice_msg_buf, status, filename,
				scratch_name);

		frame = xv_get(abstract, WIN_FRAME);
		text_notice = xv_get(frame, XV_KEY_DATA, text_notice_key);

		if (!text_notice) {
			text_notice = xv_create(frame, NOTICE, NULL);

			xv_set(frame, XV_KEY_DATA, text_notice_key, text_notice, NULL);
		}
		xv_set(text_notice,
				NOTICE_MESSAGE_STRINGS, notice_msg_buf, NULL,
				NOTICE_BUTTON_YES, XV_MSG("Continue"),
				NOTICE_BUSY_FRAMES, frame, NULL,
				XV_SHOW, TRUE,
				NULL);
	}

	return (status);
}

#define RELOAD		1
#define NO_RELOAD	0
static Es_status textsw_save_store_common(Textsw_private priv, CHAR *output_name, int reload)
{
	CHAR scratch_name[MAXNAMLEN];
	Es_handle new_esh;
	register Es_handle output;
	Es_status result;
	Es_index length;

	output = es_file_create(output_name, ES_OPT_APPEND, &result);
	if (!output)
		/* BUG ALERT!  For now, don't look at result. */
		return (ES_CANNOT_OPEN_OUTPUT);
	length = es_get_length(priv->views->esh);
	result = es_copy(priv->views->esh, output, TRUE);
	if (result == ES_SUCCESS) {
		es_destroy(output);
		if (priv->checkpoint_name) {

			if (unlink(priv->checkpoint_name) == -1) {
				perror(XV_MSG("removing checkpoint file:"));
			}
			free(priv->checkpoint_name);
			priv->checkpoint_name = NULL;
		}
		if (reload) {
			result = textsw_load_file_internal(priv, output_name, scratch_name,
					&new_esh, ES_CANNOT_SET, 0);
			if ((result == ES_SUCCESS) && (length != es_get_length(new_esh))) {
				/* Added a newline - repaint to fix line tables */
				textsw_display(XV_PUBLIC(priv));
			}
		}
	}
	if (textsw_menu_get(priv)
		&& priv->sub_menu_table
		&& priv->sub_menu_table[(int) TXTSW_FILE_SUB_MENU])
		xv_set(priv->sub_menu_table[(int)TXTSW_FILE_SUB_MENU],
					MENU_DEFAULT, 1,
					NULL);

	return (result);
}

static Es_status textsw_process_save_error(Textsw abstract, char *error_buf,
										Es_status status, int locx, int locy)
{
	Frame frame;
	Xv_Notice text_notice;
	char *msg1, *msg2;
	char msg[200];

	switch (status) {
		case ES_BACKUP_FAILED:
			msg[0] = '\0';
			msg1 = XV_MSG("Unable to Save Current File. ");
			msg2 = XV_MSG("Cannot back-up file:");
			strcat(msg, msg1);
			strcat(msg, msg2);
			goto PostError;
		case ES_BACKUP_OUT_OF_SPACE:
			msg[0] = '\0';
			msg1 = XV_MSG("Unable to Save Current File. ");
			msg2 = XV_MSG("No space for back-up file:");
			strcat(msg, msg1);
			strcat(msg, msg2);
			goto PostError;
		case ES_CANNOT_OPEN_OUTPUT:
			msg[0] = '\0';
			msg1 = XV_MSG("Unable to Save Current File. ");
			msg2 = XV_MSG("Cannot re-write file:");
			strcat(msg, msg1);
			strcat(msg, msg2);
			goto PostError;
		case ES_CANNOT_GET_NAME:
			msg[0] = '\0';
			msg1 = XV_MSG("Unable to Save Current File. ");
			msg2 = XV_MSG("INTERNAL ERROR: Forgot the name of the file.");
			strcat(msg, msg1);
			strcat(msg, msg2);
			goto PostError;
		case ES_UNKNOWN_ERROR:	/* Fall through */
		default:
			goto InternalError;
	}
  InternalError:
	msg[0] = '\0';
	msg1 = XV_MSG("Unable to Save Current File. ");
	msg2 = XV_MSG("An INTERNAL ERROR has occurred.");
	strcat(msg, msg1);
	strcat(msg, msg2);
  PostError:
	frame = (Frame) xv_get(abstract, WIN_FRAME);
	text_notice = xv_get(frame, XV_KEY_DATA, text_notice_key);

	if (!text_notice) {
		text_notice = xv_create(frame, NOTICE, NULL);

		xv_set(frame, XV_KEY_DATA, text_notice_key, text_notice, NULL);
	}
	xv_set(text_notice,
			NOTICE_MESSAGE_STRINGS, msg1, msg2, error_buf, NULL,
			NOTICE_BUTTON_YES, XV_MSG("Continue"),
			NOTICE_BUSY_FRAMES, frame, NULL,
			XV_SHOW, TRUE,
			NULL);

	return (ES_UNKNOWN_ERROR);
}

void textsw_set_backup_pattern(Textsw abstract, const char *pattern)
{
	Textsw_private priv = TEXTSW_PRIVATE(abstract);

	strcpy(priv->backup_pattern, pattern);
}

/* ARGSUSED */
static Es_status textsw_save_internal(Textsw_private priv, char *error_buf, int locx,int locy)
{
	CHAR original_name[MAXNAMLEN], *name;
	register char *msg;
	Es_handle backup, original = ES_NULL;
	int status = 0;
	Es_status es_status;
	Frame frame;
	Xv_Notice text_notice;

	/*
	 * Save overwrites the contents of the original stream, which makes the
	 * call to textsw_give_shelf-to_svc(old) in textsw_destroy_esh (reached via
	 * textsw_save_store_common calling textsw_replace_esh) perform bad
	 * operations on the pieces that are the shelf. To get around this
	 * problem, we first save the shelf explicitly.
	 */
	if (textsw_file_name(priv, &name) != 0)
		return (ES_CANNOT_GET_NAME);
	(void)STRCPY(original_name, name);
	original = (Es_handle) es_get(priv->views->esh, ES_PS_ORIGINAL);
	if (!original) {
		msg = XV_MSG("es_ps_original");
		goto Return_Error_Status;
	}

	if ((backup = es_file_make_backup(original, priv->backup_pattern,
												&es_status))
			== ES_NULL) {
		return (((es_status == ES_CHECK_ERRNO) && (errno == ENOSPC))
				? ES_BACKUP_OUT_OF_SPACE : ES_BACKUP_FAILED);
	}
	(void)es_set(priv->views->esh,
			ES_STATUS_PTR, &es_status, ES_PS_ORIGINAL, backup, NULL);
	if (es_status != ES_SUCCESS) {
		int result;

		frame = xv_get(TEXTSW_PUBLIC(priv), WIN_FRAME);
		text_notice = (Xv_Notice) xv_get(frame, XV_KEY_DATA, text_notice_key);

		if (!text_notice) {
			text_notice = xv_create(frame, NOTICE, NULL);

			xv_set(frame, XV_KEY_DATA, text_notice_key, text_notice, NULL);
		}
		xv_set(text_notice,
				NOTICE_MESSAGE_STRINGS,
					XV_MSG("Unable to Save Current File.\n\
Was the file edited with another editor?."),
					NULL,
				NOTICE_BUTTON_YES, XV_MSG("Continue"),
				NOTICE_STATUS, &result,
				NOTICE_BUSY_FRAMES, frame, NULL,
				XV_SHOW, TRUE,
				NULL);

		if (result == NOTICE_FAILED) {
			(void)es_destroy(backup);
			status = (int)es_status;
			msg = XV_MSG("ps_replace_original");
			goto Return_Error_Status;
		}
		goto Dont_Return_Error_Status;
	}

	switch (status = textsw_save_store_common(priv, original_name, RELOAD)) {
		case ES_SUCCESS:{

				(void)es_destroy(original);
				textsw_notify(VIEW_PRIVATE(xv_get(XV_PUBLIC(priv),
										OPENWIN_NTH_VIEW, 0)),
						TEXTSW_ACTION_LOADED_FILE, original_name, NULL);

				return (ES_SUCCESS);
			}
		case ES_CANNOT_OPEN_OUTPUT:
			if (errno == EACCES)
				goto Return_Error;
			msg = XV_MSG("es_file_create");
			goto Return_Error_Status;
		default:
			msg = XV_MSG("textsw_save_store_common");
			break;
	}
  Return_Error_Status:
	(void)sprintf(error_buf, XV_MSG("  %s; status = 0x%x"), msg, status);
  Dont_Return_Error_Status:
	status = ES_UNKNOWN_ERROR;
  Return_Error:
	if (original)
		(void)es_set(priv->views->esh,
				ES_STATUS_PTR, &es_status, ES_PS_ORIGINAL, original, NULL);
	return (status);
}

Xv_public unsigned textsw_save(Textsw abstract, int locx, int locy)
{
	char error_buf[MAXNAMLEN + 100];
	Es_status status;
	Textsw_private priv;

	if (xv_get(abstract, XV_IS_SUBTYPE_OF, TEXTSW)) {
		priv = TEXTSW_PRIVATE(abstract);
	}
	else {
		Textsw_view_private view = VIEW_ABS_TO_REP(abstract);
		priv = TSWPRIV_FOR_VIEWPRIV(view);
	}

	error_buf[0] = '\0';
	status = textsw_save_internal(priv, error_buf, locx, locy);

	if (status != ES_SUCCESS)
		status = textsw_process_save_error(abstract, error_buf, status, locx,
				locy);
	return ((unsigned)status);
}

static Es_status textsw_get_from_fd(Textsw_view_private view, int fd,
								int print_error_msg)
{
	Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
	int record;
	Es_index old_insert_pos, old_length;
	ssize_t count;
	char buf[2096];
	Es_status result = ES_SUCCESS;
	int status;

	textsw_flush_caches(view, TFC_PD_SEL);	/* Changes length! */
	textsw_input_before(priv, &old_insert_pos, &old_length);
	textsw_take_down_caret(priv);
	for (;;) {
		count = read(fd, buf, sizeof(buf) - 1);
		if (count == 0)
			break;
		if (count < 0) {
			return (ES_UNKNOWN_ERROR);
		}
		buf[count] = '\0';

		status = ev_input_partial(TSWPRIV_FOR_VIEWPRIV(view)->views, buf,
				count);

		if (status) {
			if (print_error_msg)
				textsw_esh_failed_msg(priv, XV_MSG("Insertion failed - "));
			result = (Es_status) es_get(priv->views->esh, ES_STATUS);
			break;
		}
	}
	record = (TXTSW_DO_AGAIN(priv) &&
			((priv->func_state & TXTSW_FUNC_AGAIN) == 0));
	(void)textsw_input_after(view, old_insert_pos, old_length, record);

	return (result);
}

/* static void textsw_cd(Textsw_private textsw, int locx, int  locy) */
/* { */
/*     CHAR            buf[MAXNAMLEN]; */
/*  */
/*     if (0 == textsw_get_selection_as_filename( */
/* 				    textsw, buf, SIZEOF(buf), locx, locy)) { */
/* 	(void) textsw_change_directory(textsw, buf, FALSE, locx, locy); */
/*     } */
/*     return; */
/* } */

Pkg_private Textsw_status textsw_get_from_file(Textsw_view_private view, CHAR *filename, int print_error_msg)
{
    Textsw_private    priv = TSWPRIV_FOR_VIEWPRIV(view);
    int             fd;
    Es_status       status;
    Textsw_status   result = TEXTSW_STATUS_CANNOT_INSERT_FROM_FILE;
    CHAR            buf[MAXNAMLEN];

    if (!TXTSW_IS_READ_ONLY(priv) && ((int)STRLEN(filename) > 0)) {
	STRCPY(buf, filename);
	if (textsw_expand_filename(priv, buf, -1, -1) == 0) {
	    if ((fd = open(buf, 0)) >= 0) {
		textsw_take_down_caret(priv);
		textsw_checkpoint_undo(TEXTSW_PUBLIC(priv),
				       (caddr_t) TEXTSW_INFINITY - 1);
		status = textsw_get_from_fd(view, fd, print_error_msg);
		textsw_checkpoint_undo(TEXTSW_PUBLIC(priv),
				       (caddr_t) TEXTSW_INFINITY - 1);
		textsw_update_scrollbars(priv, TEXTSW_VIEW_NULL);
		(void) close(fd);
		if (status == ES_SUCCESS)
		    result = TEXTSW_STATUS_OKAY;
		else if (TEXTSW_OUT_OF_MEMORY(priv, status))
		    result = TEXTSW_STATUS_OUT_OF_MEMORY;
		textsw_invert_caret(priv);
	    }
	}
    }
    return (result);
}

Pkg_private Textsw_status textsw_file_stuff_from_str(Textsw_view_private view,
    CHAR *buf, int locx, int locy)
{
	Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
	int fd;
	char msg[MAXNAMLEN + 100], *sys_msg;
	char notice_msg1[MAXNAMLEN + 100];
	char *notice_msg2;
	Es_status status = 0;
	int cannot_open = 0;
	Xv_Notice text_notice;
	Frame frame;

	if ((fd = open(buf, 0)) < 0) {
		cannot_open = (fd == -1);
		goto InternalError;
	}
	errno = 0;

	textsw_checkpoint_undo(TEXTSW_PUBLIC(priv), (caddr_t) TEXTSW_INFINITY - 1);

	status = textsw_get_from_fd(view, fd, TRUE);
	textsw_checkpoint_undo(TEXTSW_PUBLIC(priv), (caddr_t) TEXTSW_INFINITY - 1);
	textsw_update_scrollbars(priv, TEXTSW_VIEW_NULL);
	(void)close(fd);
	if (status != ES_SUCCESS && status != ES_SHORT_WRITE)
		goto InternalError;
	return (Textsw_status) status;
  InternalError:
	if (cannot_open) {
		CHAR *full_pathname;

		full_pathname = textsw_full_pathname(buf);

		(void)sprintf(msg, "'%s': ", full_pathname);
		(void)sprintf(notice_msg1, "'%s'", full_pathname);

		notice_msg2 = "  ";
		free(full_pathname);
	}
	else {
		(void)sprintf(msg, "%s",
				XV_MSG("Unable to Include File.  An INTERNAL ERROR has occurred.: "));
		(void)sprintf(notice_msg1, "%s", XV_MSG("Unable to Include File."));
		notice_msg2 = XV_MSG("An INTERNAL ERROR has occurred.");
	}
	sys_msg = strerror(errno);

	frame = xv_get(TEXTSW_PUBLIC(priv), WIN_FRAME);
	text_notice = xv_get(frame, XV_KEY_DATA, text_notice_key);

	if (!text_notice) {
		text_notice = xv_create(frame, NOTICE, NULL);

		xv_set(frame, XV_KEY_DATA, text_notice_key, text_notice, NULL);
	}
	xv_set(text_notice,
			NOTICE_MESSAGE_STRINGS,
				(strlen(sys_msg)) ? sys_msg : notice_msg1,
				(strlen(sys_msg)) ? notice_msg1 : notice_msg2,
				(strlen(sys_msg)) ? notice_msg2 : "",
				NULL,
			NOTICE_BUTTON_YES, XV_MSG("Continue"),
			NOTICE_BUSY_FRAMES, frame, NULL,
			XV_SHOW, TRUE,
			NULL);

	return (Textsw_status) status;
}

Pkg_private Es_status textsw_store_init(Textsw_private textsw, CHAR *filename)
{
    struct stat     stat_buf;
    if (stat(filename, &stat_buf) == 0) {
	Es_handle       original = (Es_handle)
	es_get(textsw->views->esh, ES_PS_ORIGINAL);
	if AN_ERROR
	    (original == ES_NULL) {
	    return (ES_CANNOT_GET_NAME);
	    }
	switch ((Es_enum)
			((long)es_get(original, ES_TYPE))
	) {
	  case ES_TYPE_FILE:
	    if (es_file_copy_status(original, filename) != 0)
		return (ES_USE_SAVE);
	    /* else fall through */
	  default:
	    if ((stat_buf.st_size > 0) &&
		(textsw->state & TXTSW_CONFIRM_OVERWRITE))
		return (ES_CANNOT_OVERWRITE);
	    break;
	}
    } else if (errno != ENOENT) {
	return (ES_CANNOT_OPEN_OUTPUT);
    }
    return (ES_SUCCESS);
}

/* ARGSUSED */
/* CHAR           *filename;	Currently unused */
Pkg_private Es_status textsw_process_store_error(Textsw_private textsw, CHAR *filename, Es_status status, int locx, int locy)
{
	Frame frame;
	Xv_Notice text_notice;
	char *msg1, *msg2;
	char msg[200];
	int result;

	switch (status) {
		case ES_SUCCESS:
			LINT_IGNORE(ASSUME(0));
			return (ES_UNKNOWN_ERROR);
		case ES_CANNOT_GET_NAME:
			msg[0] = '\0';
			msg1 = XV_MSG("Unable to Store as New File. ");
			msg2 = XV_MSG("INTERNAL ERROR: Forgot the name of the file.");
			strcat(msg, msg1);
			strcat(msg, msg2);
			goto PostError;
		case ES_CANNOT_OPEN_OUTPUT:
			msg[0] = '\0';
			msg1 = XV_MSG("Unable to Store as New File. ");
			msg2 = XV_MSG("Problems accessing specified file.");
			strcat(msg, msg1);
			strcat(msg, msg2);
			goto PostError;
		case ES_CANNOT_OVERWRITE:
				frame = xv_get(TEXTSW_PUBLIC(textsw), WIN_FRAME);
				text_notice = xv_get(frame, XV_KEY_DATA, text_notice_key);

				if (!text_notice) {
					text_notice = xv_create(frame, NOTICE, NULL);

					xv_set(frame,
							XV_KEY_DATA, text_notice_key, text_notice, NULL);
				}
				xv_set(text_notice,
						NOTICE_MESSAGE_STRINGS,
							XV_MSG("Please confirm Store as New File:"),
							filename,
							"  ",
							XV_MSG("That file exists and has data in it."),
							NULL,
						NOTICE_BUTTON_YES, XV_MSG("Confirm"),
						NOTICE_BUTTON_NO, XV_MSG("Cancel"),
						NOTICE_STATUS, &result,
						NOTICE_BUSY_FRAMES, frame, NULL,
						XV_SHOW, TRUE,
						NULL);

				return ((result == NOTICE_YES) ? ES_SUCCESS : ES_UNKNOWN_ERROR);


		case ES_FLUSH_FAILED:
		case ES_FSYNC_FAILED:
		case ES_SHORT_WRITE:
			msg[0] = '\0';
			msg1 = XV_MSG("Unable to Store as New File. ");
			msg2 = XV_MSG("File system full.");
			strcat(msg, msg1);
			strcat(msg, msg2);
			goto PostError;
		case ES_USE_SAVE:
			msg[0] = '\0';
			msg1 = XV_MSG("Unable to Store as New File. ");
			msg2 = XV_MSG("Use Save Current File instead.");
			strcat(msg, msg1);
			strcat(msg, msg2);
			goto PostError;
		case ES_UNKNOWN_ERROR:	/* Fall through */
		default:
			goto InternalError;
	}
  InternalError:
	msg[0] = '\0';
	msg1 = XV_MSG("Unable to Store as New File. ");
	msg2 = XV_MSG("An INTERNAL ERROR has occurred.");
	strcat(msg, msg1);
	strcat(msg, msg2);
  PostError:
	frame = xv_get(TEXTSW_PUBLIC(textsw), WIN_FRAME);
	text_notice = (Xv_Notice) xv_get(frame, XV_KEY_DATA, text_notice_key);

	if (!text_notice) {
		text_notice = xv_create(frame, NOTICE, NULL);

		xv_set(frame, XV_KEY_DATA, text_notice_key, text_notice, NULL);
	}
	xv_set(text_notice,
			NOTICE_MESSAGE_STRINGS, msg1, msg2, NULL,
			NOTICE_BUTTON_YES, XV_MSG("Continue"),
			NOTICE_BUSY_FRAMES, frame, NULL,
			XV_SHOW, TRUE,
			NULL);

	return ES_UNKNOWN_ERROR;
}

static unsigned textsw_store_file_internal(Textsw abstract, CHAR *filename, int locx, int locy)
{
	register Es_status status;
	register Textsw_private priv = TEXTSW_PRIVATE(abstract);

	status = textsw_store_init(priv, filename);
	switch (status) {
		case ES_SUCCESS:
			break;
		case ES_USE_SAVE:
			return (textsw_save(abstract, locx, locy));
			/* Fall through */
		default:
			status = textsw_process_store_error(priv, filename, status, locx,
					locy);
			break;
	}

	if (status == ES_SUCCESS) {
		status = textsw_save_store_common(priv, filename,
				(priv->state & TXTSW_STORE_CHANGES_FILE)
				? TRUE : FALSE);
		if (status == ES_SUCCESS) {
			if (priv->state & TXTSW_STORE_CHANGES_FILE) {

				textsw_notify(VIEW_PRIVATE(xv_get(XV_PUBLIC(priv),
										OPENWIN_NTH_VIEW, 0)),
						TEXTSW_ACTION_LOADED_FILE, filename, NULL);
			}
		}
		else {
			status = textsw_process_store_error(priv, filename, status, locx,
					locy);
		}
	}
	return ((unsigned)status);
}

Xv_public unsigned textsw_store_file(Textsw abstract, char *filename, int locx, int locy)
{
	assert(xv_get(abstract, XV_IS_SUBTYPE_OF, OPENWIN));

    return (textsw_store_file_internal(abstract, filename, locx, locy));
}

static Es_status textsw_checkpoint_internal(Textsw_private priv);

/* ARGSUSED */
Pkg_private void textsw_reset_2(Textsw abstract, int locx, int locy, int preserve_memory, int cmd_is_undo_all_edit)
{
    Es_handle       piece_esh, old_original_esh, new_original_esh;
    char           *name, save_name[MAXNAMLEN], scratch_name[MAXNAMLEN], *temp_name;
    int             status;
    Textsw_private priv;
    register int    old_count;
    unsigned old_memory_length = 0;
    long wrap_around_size;
    short is_readonly;

	assert(xv_get(abstract, XV_IS_SUBTYPE_OF, OPENWIN));
	priv = TEXTSW_PRIVATE(abstract);

    old_count = priv->again_count;
    wrap_around_size = (long) es_get(priv->views->esh, ES_PS_SCRATCH_MAX_LEN);
    is_readonly = TXTSW_IS_READ_ONLY(priv);	/* jcb */

    free(priv->again->base);
    if (preserve_memory) {
	/* Get info about original esh before possibly invalidating it. */
	old_original_esh = (Es_handle) es_get(
					 priv->views->esh, ES_PS_ORIGINAL);
	if ((Es_enum)
		((long)es_get(old_original_esh, ES_TYPE))
		== ES_TYPE_MEMORY) {
	    old_memory_length = es_get_length(old_original_esh);
	}
    }
    if (textsw_has_been_modified(abstract) &&
	(status = textsw_file_name(priv, &name)) == 0) {
	/* Have edited a file, so reset to the file, not memory. */
	/* First take a checkpoint, so recovery is possible. */
	if (priv->checkpoint_frequency > 0) {
	    if (textsw_checkpoint_internal(priv) == ES_SUCCESS) {
		priv->checkpoint_number++;
	    }
	}
	/* This is for cmdsw log file */
	/* When empty document load up the original empty tmp file */
	/* instead of the one we did a store in */
	temp_name = cmd_is_undo_all_edit ? NULL :
	    (char *) xv_get(abstract, TEXTSW_TEMP_FILENAME);
	if (temp_name)
	    strcpy(save_name, temp_name);
	else
	    strcpy(save_name, name);

	status = textsw_load_file_internal(priv, save_name, scratch_name,
				&piece_esh, 0, TXTSW_LFI_CLEAR_SELECTIONS);
	/*
	 * It would be nice to preserve the old positioning of the views, but
	 * all of the material in a view may be either new or significantly
	 * rearranged. One possiblity is to get the piece_stream to find the
	 * nearest original stream position and use that if we add yet
	 * another hack into ps_impl.c!
	 */
	if (status == ES_SUCCESS)
	    goto Return;
	/* BUG ALERT: a few diagnostics might be appreciated. */
    }
    if (old_memory_length > 0) {
	/*
	 * We are resetting from memory to memory, and the old memory had
	 * preloaded contents that we should preserve.
	 */
	new_original_esh =
	    es_mem_create(old_memory_length + 1, "");
	if (new_original_esh) {
	    es_copy(old_original_esh, new_original_esh, FALSE);
	}
    } else {
	new_original_esh = es_mem_create(0, "");
    }

    piece_esh = textsw_create_mem_ps(priv, new_original_esh);
    if (piece_esh != ES_NULL) {
	textsw_set_selection(abstract, ES_INFINITY, ES_INFINITY,
			     EV_SEL_PRIMARY);
	textsw_set_selection(abstract, ES_INFINITY, ES_INFINITY,
			     EV_SEL_SECONDARY);
	textsw_replace_esh(priv, piece_esh);
	(void) ev_set(priv->views->first_view,
		      EV_FOR_ALL_VIEWS,
		      EV_DISPLAY_LEVEL, EV_DISPLAY_NONE,
		      EV_DISPLAY_START, 0,
		      EV_DISPLAY_LEVEL, EV_DISPLAY,
		      NULL);

	textsw_update_scrollbars(priv, TEXTSW_VIEW_NULL);

	textsw_notify(VIEW_PRIVATE(xv_get(XV_PUBLIC(priv), OPENWIN_NTH_VIEW, 0)),
						TEXTSW_ACTION_USING_MEMORY, NULL);
    }
Return:
    textsw_set_insert(priv, 0);
    textsw_init_again(priv, 0);
    textsw_init_again(priv, old_count);	/* restore number of again
						 * level */
    (void) es_set(priv->views->esh,
		  ES_PS_SCRATCH_MAX_LEN, wrap_around_size,
		  NULL);
    if (textsw_menu_get(priv)
		&& priv->sub_menu_table
		&& priv->sub_menu_table[(int) TXTSW_FILE_SUB_MENU])
        xv_set(priv->sub_menu_table[(int) TXTSW_FILE_SUB_MENU],
				MENU_DEFAULT, 1,
				NULL);

    if (is_readonly) {		/* jcb -- reset if readonly */
		SET_BOOL_FLAG(priv->state, TRUE, TXTSW_READ_ONLY_ESH);
	}
}

Xv_public void textsw_reset(Textsw abstract, int locx, int locy)
{
    textsw_reset_2(abstract, locx, locy, FALSE, FALSE);
}

static int textsw_filename_is_all_blanks(CHAR *filename)
{
    int             i = 0;

    while ((filename[i] == ' ') || (filename[i] == '\t') || (filename[i] == '\n'))
	i++;
    return (filename[i] == '\0');
}


/* Returns 0 iff a selection exists and it is matched by exactly one name. */

Pkg_private int textsw_expand_filename(Textsw_private priv, char *buf, int locx, int locy)
{
	Frame frame;
	Xv_Notice text_notice;
	struct namelist *nl;

	nl = xv_expand_name(buf);
	if ((buf[0] == '\0') || (nl == NONAMES)) {
		frame = xv_get(TEXTSW_PUBLIC(priv), WIN_FRAME);
		text_notice = xv_get(frame, XV_KEY_DATA, text_notice_key);

		if (!text_notice) {
			text_notice = xv_create(frame, NOTICE, NULL);

			xv_set(frame, XV_KEY_DATA, text_notice_key, text_notice, NULL);
		}
		xv_set(text_notice,
				NOTICE_MESSAGE_STRINGS,
					XV_MSG("Unrecognized file name.\n\
Unable to expand specified pattern:"),
					buf,
					NULL,
				NOTICE_BUTTON_YES, XV_MSG("Continue"),
				NOTICE_BUSY_FRAMES, frame, NULL,
				XV_SHOW, TRUE,
				NULL);

		return (1);
	}
	if (textsw_filename_is_all_blanks(buf)) {
		frame = xv_get(TEXTSW_PUBLIC(priv), WIN_FRAME);
		text_notice = xv_get(frame, XV_KEY_DATA, text_notice_key);

		if (!text_notice) {
			text_notice = xv_create(frame, NOTICE, NULL);

			xv_set(frame, XV_KEY_DATA, text_notice_key, text_notice, NULL);
		}
		xv_set(text_notice,
				NOTICE_MESSAGE_STRINGS,
					XV_MSG("Unrecognized file name.\n\
File name contains only blank or tab characters.\n\
Please use a valid file name."),
					NULL,
				NOTICE_BUTTON_YES, XV_MSG("Continue"),
				NOTICE_BUSY_FRAMES, frame, NULL,
				XV_SHOW, TRUE,
				NULL);
		return (1);
	}
	/*
	 * Below here we have dynamically allocated memory pointed at by nl that
	 * we must make sure gets deallocated.
	 */
	if (nl->count == 0) {
		frame = xv_get(TEXTSW_PUBLIC(priv), WIN_FRAME);
		text_notice = xv_get(frame, XV_KEY_DATA, text_notice_key);

		if (!text_notice) {
			text_notice = xv_create(frame, NOTICE, NULL);

			xv_set(frame, XV_KEY_DATA, text_notice_key, text_notice, NULL);
		}
		xv_set(text_notice,
				NOTICE_MESSAGE_STRINGS,
					XV_MSG("Unrecognized file name.\n\
No files match specified pattern:"),
					buf,
					NULL,
				NOTICE_BUTTON_YES, XV_MSG("Continue"),
				NOTICE_BUSY_FRAMES, frame, NULL,
				XV_SHOW, TRUE,
				NULL);

		return (1);
	}
	else if (nl->count > 1) {
		frame = xv_get(TEXTSW_PUBLIC(priv), WIN_FRAME);
		text_notice = xv_get(frame, XV_KEY_DATA, text_notice_key);

		if (!text_notice) {
			text_notice = xv_create(frame, NOTICE, NULL);

			xv_set(frame, XV_KEY_DATA, text_notice_key, text_notice, NULL);
		}
		xv_set(text_notice,
				NOTICE_MESSAGE_STRINGS,
					XV_MSG("Unrecognized file name.\n\
Too many files match specified pattern:"),
					buf,
					NULL,
				NOTICE_BUTTON_YES, XV_MSG("Continue"),
				NOTICE_BUSY_FRAMES, frame, NULL,
				XV_SHOW, TRUE,
				NULL);

		return (1);
	}
	else
		(void)strcpy(buf, nl->names[0]);

	free_namelist(nl);
	return (0);
}

Pkg_private void textsw_possibly_edited_now_notify(Textsw_private priv)
{
    CHAR           *name;

    if (textsw_has_been_modified(TEXTSW_PUBLIC(priv))) {
	/*
	 * WARNING: client may textsw_reset() during notification, hence edit
	 * flag's state is only known BEFORE the notification and must be set
	 * before the notification.
	 */

	priv->state |= TXTSW_EDITED;

	if (textsw_file_name(priv, &name) == 0) {
	    if (textsw_menu_get(priv)
			&& priv->sub_menu_table
			&& priv->sub_menu_table[(int) TXTSW_FILE_SUB_MENU])
			xv_set(priv->sub_menu_table[(int) TXTSW_FILE_SUB_MENU],
							MENU_DEFAULT, 2,
							NULL );
	    textsw_notify(VIEW_PRIVATE(xv_get(XV_PUBLIC(priv), OPENWIN_NTH_VIEW, 0)),
			  TEXTSW_ACTION_EDITED_FILE, name, NULL);

	} else {
	    textsw_notify(VIEW_PRIVATE(xv_get(XV_PUBLIC(priv), OPENWIN_NTH_VIEW, 0)),
			  TEXTSW_ACTION_EDITED_MEMORY, NULL);
	    if (textsw_menu_get(priv)
			&& priv->sub_menu_table
			&& priv->sub_menu_table[(int) TXTSW_FILE_SUB_MENU])
			xv_set(priv->sub_menu_table[(int) TXTSW_FILE_SUB_MENU],
						MENU_DEFAULT, 3,
						NULL );
	}
    }
}

/* hier wird definitiv ein Textsw erwartet, kein View */
Pkg_private int textsw_has_been_modified(Textsw abstract)
{
	Textsw_private priv = TEXTSW_PRIVATE(abstract);

	if (priv->state & TXTSW_INITIALIZED) {
		return (int)((long)es_get(priv->views->esh, ES_HAS_EDITS));
	}
	return 0;
}

Pkg_private int textsw_file_name(Textsw_private    priv, CHAR **name)
{
	Es_handle original = (Es_handle)es_get(priv->views->esh, ES_PS_ORIGINAL);

	if (original == 0)
		return (1);
	if ((Es_enum) es_get(original, ES_TYPE) != ES_TYPE_FILE)
		return (2);
	if ((*name = (CHAR *) es_get(original, ES_NAME)) == NULL)
		return (3);
	if (*name[0] == '\0')
		return (4);
	return (0);
}

Xv_public int textsw_append_file_name(Textsw abstract, char *name)
/* Returns 0 iff editing a file and name could be successfully acquired. */
{
    Textsw_private    priv = TEXTSW_PRIVATE(abstract);
    CHAR           *internal_name;
    int             result;

    result = textsw_file_name(priv, &internal_name);
    if (result == 0)
	(void) STRCAT(name, internal_name);
    return (result);
}

/* ARGSUSED */
Pkg_private void textsw_post_error(Textsw_private priv, int locx, int locy,
									char *msg1, char *msg2)
{
	char buf[MAXNAMLEN + 1000];
	size_t size_to_use = sizeof(buf);
	Frame frame;
	Xv_Notice text_notice;

	buf[0] = '\0';
	strncat(buf, msg1, size_to_use-1);
	if (msg2) {
		size_t len = strlen(buf);

		if (len < size_to_use) {
			(void)strncat(buf, msg2, size_to_use - len);
		}
	}

	frame = xv_get(TEXTSW_PUBLIC(priv), WIN_FRAME);
	text_notice = xv_get(frame, XV_KEY_DATA, text_notice_key);

	if (!text_notice) {
		text_notice = xv_create(frame, NOTICE, NULL);

		xv_set(frame, XV_KEY_DATA, text_notice_key, text_notice, NULL);
	}
	xv_set(text_notice,
			NOTICE_MESSAGE_STRING, buf,
			NOTICE_BUTTON_YES, XV_MSG("Continue"),
			NOTICE_BUSY_FRAMES, frame, NULL,
			XV_SHOW, TRUE,
			NULL);
}


/*
 * ===================================================================
 *
 * Misc. file system manipulation utilities
 *
 * ===================================================================
 */

/* Returns 0 iff change directory succeeded. */
Pkg_private int
textsw_change_directory(Textsw_private textsw, char *filename,
    int might_not_be_dir, int locx, int locy)
{
	char *sys_msg;
	char *full_pathname;
	char msg[MAXNAMLEN + 100];
	char notice_msg[MAXNAMLEN + 100];
	struct stat stat_buf;
	int result = 0;
	Frame frame;
	Xv_Notice text_notice;

	errno = 0;
	if (stat(filename, &stat_buf) < 0) {
		result = -1;
		goto Error;
	}
	if ((stat_buf.st_mode & S_IFMT) != S_IFDIR) {
		if (might_not_be_dir)
			return (-2);
	}
	if (chdir(filename) < 0) {
		result = errno;
		goto Error;
	}
	textsw_notify(VIEW_PRIVATE(xv_get(XV_PUBLIC(textsw), OPENWIN_NTH_VIEW, 0)),
			TEXTSW_ACTION_CHANGED_DIRECTORY, filename,
			NULL);
	return (result);

  Error:

	full_pathname = textsw_full_pathname(filename);

	(void)sprintf(msg, "%s '%s': ",
			(might_not_be_dir ?
					XV_MSG("Unable to access file") :
					XV_MSG("Unable to cd to directory")), full_pathname);
	(void)sprintf(notice_msg, "%s:",
			(might_not_be_dir ?
					XV_MSG("Unable to access file") :
					XV_MSG("Unable to cd to directory")));
	sys_msg = strerror(errno);

	frame = xv_get(TEXTSW_PUBLIC(textsw), WIN_FRAME);
	text_notice = xv_get(frame, XV_KEY_DATA, text_notice_key);

	if (!text_notice) {
		text_notice = xv_create(frame, NOTICE, NULL);

		xv_set(frame, XV_KEY_DATA, text_notice_key, text_notice, NULL);
	}
	xv_set(text_notice,
			NOTICE_MESSAGE_STRINGS,
				notice_msg,
				full_pathname,
				sys_msg,
				NULL,
			NOTICE_BUTTON_YES, XV_MSG("Continue"),
			NOTICE_BUSY_FRAMES, frame, NULL,
			XV_SHOW, TRUE,
			NULL);

	free(full_pathname);
	return (result);
}

static Es_status textsw_checkpoint_internal(Textsw_private priv)
{
    Es_handle       cp_file;
    Es_status       result;

    if (!priv->checkpoint_name) {
	CHAR           *name;
	if (textsw_file_name(priv, &name) != 0)
	    return (ES_CANNOT_GET_NAME);
	if ((priv->checkpoint_name = malloc((size_t)MAXNAMLEN)) == 0)
	    return (ES_CANNOT_GET_NAME);
	(void) sprintf(priv->checkpoint_name, "%s%%%%", name);
    }
    cp_file = es_file_create(priv->checkpoint_name,
			     ES_OPT_APPEND, &result);
    if (!cp_file) {
	/* BUG ALERT!  For now, don't look at result. */
	return (ES_CANNOT_OPEN_OUTPUT);
    }
    result = es_copy(priv->views->esh, cp_file, TRUE);
    es_destroy(cp_file);

    return (result);
}


/*
 * If the number of edits since the last checkpoint has exceeded the
 * checkpoint frequency, take a checkpoint. Return ES_SUCCESS if we do the
 * checkpoint.
 */
Pkg_private Es_status textsw_checkpoint(Textsw_private priv)
{
	Textsw_view_private view = VIEW_PRIVATE(xv_get(XV_PUBLIC(priv), OPENWIN_NTH_VIEW, 0));
	int edit_number = (int)ev_get((Ev_handle) view->e_view,
			EV_CHAIN_EDIT_NUMBER, XV_NULL, XV_NULL, XV_NULL);
	Es_status result = ES_DID_NOT_CHECKPOINT;

	if (priv->state & TXTSW_IN_NOTIFY_PROC || priv->checkpoint_frequency <= 0)
		return (result);
	if ((edit_number / priv->checkpoint_frequency)
			> priv->checkpoint_number) {
		/* do the checkpoint */
		result = textsw_checkpoint_internal(priv);
		if (result == ES_SUCCESS) {
			priv->checkpoint_number++;
		}
	}
	return (result);
}


/* returns XV_OK or XV_ERROR */
Pkg_private int textsw_empty_document(Textsw abstract, Event *ie)
{
	register Textsw_private textsw = TEXTSW_PRIVATE(abstract);

	int has_been_modified = textsw_has_been_modified(abstract);
	int number_of_resets = 0;
	int is_cmdtool = (textsw->state & TXTSW_NO_RESET_TO_SCRATCH);
	int result;
	int loc_x = (ie) ? ie->ie_locx : 0;
	int loc_y = (ie) ? ie->ie_locy : 0;
	Frame frame = xv_get(abstract, WIN_FRAME);
	Xv_Notice text_notice;

	if (has_been_modified) {
		text_notice = xv_get(frame, XV_KEY_DATA, text_notice_key);

		if (!text_notice) {
			text_notice = xv_create(frame, NOTICE, NULL);

			xv_set(frame, XV_KEY_DATA, text_notice_key, text_notice, NULL);
		}
		xv_set(text_notice,
				NOTICE_MESSAGE_STRINGS,
					XV_MSG("The text has been edited.\n\
Clear Log will discard these edits. Please confirm."),
					NULL,
				NOTICE_BUTTON_YES, XV_MSG("Cancel"),
				NOTICE_BUTTON_NO, XV_MSG("Confirm, discard edits"),
				NOTICE_STATUS, &result,
				NOTICE_BUSY_FRAMES, frame, NULL,
				XV_SHOW, TRUE,
				NULL);

		if (result == NOTICE_YES)
			return XV_ERROR;
		textsw_reset(abstract, loc_x, loc_y);
		number_of_resets++;
	}
	if (is_cmdtool) {
		if ((has_been_modified) && (number_of_resets == 0))
			if (number_of_resets == 0)
				textsw_reset(abstract, loc_x, loc_y);
	}
	else {
		textsw_reset(abstract, loc_x, loc_y);
	}
	return XV_OK;
}

Pkg_private void textsw_post_need_selection(Textsw abstract, Event *ie)
	/* ignored, check for NIL if ever used */
{
	Xv_notice text_notice;
	Frame frame = xv_get(abstract, WIN_FRAME);

	text_notice = xv_get(frame, XV_KEY_DATA, text_notice_key);

	if (!text_notice) {
		text_notice = xv_create(frame, NOTICE, NULL);

		xv_set(frame, XV_KEY_DATA, text_notice_key, text_notice, NULL);
	}
	xv_set(text_notice,
			NOTICE_MESSAGE_STRING,
				XV_MSG("Please select a file name and choose this option again."),
			NOTICE_BUTTON_YES, XV_MSG("Continue"),
			NOTICE_BUSY_FRAMES, frame, NULL,
			XV_SHOW, TRUE,
			NULL);
}

#if defined(DEBUG) && !defined(lint)
static char    *header = "fd      dev: #, type    inode\n";
static void
debug_dump_fds(stream)
    FILE           *stream;
{
    register int    fd;
    struct stat     s;

    if (stream == 0)
	stream = stderr;
    fprintf(stream, header);
    for (fd = 0; fd < 32; fd++) {
	if (fstat(fd, &s) != -1) {
	    fprintf(stream, "%2d\t%6d\t%4d\t%5d\n",
		    fd, s.st_dev, s.st_rdev, s.st_ino);
	}
    }
}
#endif
