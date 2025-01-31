#ifndef lint
char     txt_attr_c_sccsid[] = "@(#)txt_attr.c 20.127 93/04/28 DRA: $Id: txt_attr.c,v 4.10 2025/01/01 20:52:01 dra Exp $";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Attribute set/get routines for text subwindows.
 */

#include <xview/pkg.h>
#include <xview/attrol.h>
#include <xview_private/primal.h>
#include <xview_private/attr_impl.h>
#include <xview_private/txt_impl.h>
#include <xview_private/txt_18impl.h>
#ifdef SVR4
#include <dirent.h>
#include <string.h>
#else
#include <sys/dir.h>
#endif /* SVR4 */
#include <pixrect/pixfont.h>
#include <xview/window.h>
#include <xview/openmenu.h>
#include <xview/defaults.h>
#include <xview_private/ev_impl.h>
#include <xview_private/windowimpl.h>
#include <xview_private/draw_impl.h>
#include <xview_private/font_impl.h>

Xv_public Pixfont *xv_pf_open(char *fontname, Xv_server srv);
#ifdef OW_I18N
Pkg_private Es_index textsw_get_contents_wcs();
#endif /* OW_I18N */
Xv_public int xv_pf_close(Pixfont *pf);

#ifndef CTRL
#ifndef __STDC__
#define CTRL(c)		('c' & 037)
#else /* __STDC__ */
#define CTRL(c)		(c & 037)
#endif /* __STDC__ */
#endif
#define	DEL		0x7f

#define SET_BOOL_FLAG(flags, to_test, flag)			\
	if ((unsigned)(to_test) != 0) (flags) |= (flag);	\
	else (flags) &= ~(flag)

#define BOOL_FLAG_VALUE(flags, flag)				\
	((flags & flag) ? TRUE : FALSE)


static void textsw_view_cms_change(Textsw_private textsw,Textsw_view_private view);

static Textsw_status set_first(Textsw_view_private view, char *error_msg,
							CHAR *filename, int reset_mode, Es_index first,
							int first_line, int all_views)
{
    char            msg_buf[MAXNAMLEN + 100];
    char           *msg;
    Es_status       load_status;
    Textsw_status   result = TEXTSW_STATUS_OKAY;
    Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
#ifdef OW_I18N
    Pkg_private void     textsw_normalize_view_wc();
#else
#endif

    msg = (error_msg) ? error_msg : msg_buf;
#ifdef OW_I18N
    if (filename && (STRLEN(filename) > 0))
#else
    if (filename)
#endif
	{
		CHAR            scratch_name[MAXNAMLEN];
		Es_handle       new_esh;
#ifdef OW_I18N
		char           *filename_mb;
#endif
		/* Ensure no caret turds will leave behind */
		textsw_take_down_caret(priv);
		load_status =
	    	textsw_load_file_internal(priv, filename, scratch_name, &new_esh,
									ES_CANNOT_SET, 1);
		if (load_status == ES_SUCCESS) {
#ifdef OW_I18N
	    	SET_CONTENTS_UPDATED(priv, TRUE);
	    	if ((int) es_get((Es_handle)
					es_get(new_esh, ES_PS_ORIGINAL), ES_SKIPPED))
				textsw_invalid_data_notice(view, filename, 1);
#endif /* OW_I18N */
	    	if (first_line > -1) {
				first = textsw_position_for_physical_line(
				     		VIEW_PUBLIC(view), first_line + 1);
	    	}
	    	if (reset_mode != TEXTSW_LRSS_CURRENT) {
				(void) ev_set(view->e_view,
			      		EV_FOR_ALL_VIEWS,
			      		EV_DISPLAY_LEVEL, EV_DISPLAY_NONE,
			      		EV_DISPLAY_START, first,
			      		EV_DISPLAY_LEVEL, EV_DISPLAY,
			      		NULL);
	    	}
#ifdef OW_I18N
	    	filename_mb = _xv_wcstombsdup(filename);
	    	textsw_notify(view,
			  	TEXTSW_ACTION_LOADED_FILE, filename_mb,
			  	TEXTSW_ACTION_LOADED_FILE_WCS, filename, NULL);
	    	if (filename_mb)
				free(filename_mb);
#else
	    	textsw_notify(view, TEXTSW_ACTION_LOADED_FILE, filename, NULL);
#endif
		}
		else {
	    	textsw_format_load_error(msg, load_status, filename, scratch_name);
	    	if (error_msg == NULL)
				textsw_post_error(priv, 0, 0, msg, NULL);
	    	result = TEXTSW_STATUS_OTHER_ERROR;
		}
    }
	else {
		if (first_line > -1) {
	    	first = textsw_position_for_physical_line(VIEW_PUBLIC(view),
														first_line + 1);
		}
		if (first != ES_CANNOT_SET) {
	    	if (all_views) {
				Textsw_view vp;

				OPENWIN_EACH_VIEW(XV_PUBLIC(priv), vp)
		    		textsw_normalize_internal(VIEW_PRIVATE(vp), first, first,
											0, 0, TXTSW_NI_DEFAULT);
				OPENWIN_END_EACH
	    	}
			else {
#ifdef OW_I18N
				textsw_normalize_view_wc(VIEW_PUBLIC(view), first);
#else
				textsw_normalize_view(VIEW_PUBLIC(view), first);
#endif
			}
		}
		else {
	    	result = TEXTSW_STATUS_OTHER_ERROR;
		}
    }
    return (result);
}


Pkg_private void textsw_set_null_view_avlist(Textsw_private priv, Attr_avlist attrs)
{
	Attr_attribute avarray[ATTR_STANDARD_SIZE];
	Attr_attribute *view_attrs = avarray;

	/*
	 * consume the view attrs from attrs, consume the non-view attrs from
	 * view_attrs.
	 */
	(void)attr_copy_avlist(view_attrs, attrs);
	for (; *view_attrs; view_attrs = attr_next(view_attrs),
			attrs = attr_next(attrs)) {

		switch (*view_attrs) {
			case TEXTSW_AUTO_SCROLL_BY:
			case TEXTSW_CONTENTS:
			case TEXTSW_FILE_CONTENTS:
			case TEXTSW_FILE:
			case TEXTSW_FIRST:

#ifdef OW_I18N
			case TEXTSW_CONTENTS_WCS:
			case TEXTSW_FILE_CONTENTS_WCS:
			case TEXTSW_FILE_WCS:
			case TEXTSW_INSERT_FROM_FILE_WCS:
			case TEXTSW_FIRST_WC:
#endif /* OW_I18N */

			case TEXTSW_FIRST_LINE:
			case TEXTSW_INSERT_FROM_FILE:
			case TEXTSW_LINE_BREAK_ACTION:
			case TEXTSW_LOWER_CONTEXT:
			case TEXTSW_NO_REPAINT_TIL_EVENT:
			case TEXTSW_RESET_TO_CONTENTS:
			case TEXTSW_UPPER_CONTEXT:
			case XV_LEFT_MARGIN:
			case XV_RIGHT_MARGIN:
				/*
				 * these are all view attrs, so consume from attrs.
				 */
				ATTR_CONSUME(*attrs);
				break;

			case TEXTSW_STATUS:
				/*
				 * this applies to both the view and the priv, so leave it
				 * alone.
				 */
				break;

			default:
				/*
				 * must be a non-view attr, so consume from view_attrs.
				 */
				ATTR_CONSUME(*(Attr_avlist) view_attrs);
				break;
		}
	}
	/* now apply the view attrs to the view */
	xv_set(TEXTSW_PUBLIC(priv),
			OPENWIN_VIEW_ATTRS,
				ATTR_LIST, avarray,
				NULL,
			NULL);

	/* now attrs has no view attrs left uncomsumed in it */
}

#ifdef OW_I18N
#define	IC_ACTIVE_TRUE		1
#define	IC_ACTIVE_FALSE		2
#endif

Pkg_private void textsw_set_internal_tier2(Textsw_private textsw,
	Textsw_view_private view,
    Attr_attribute *attrs, int is_folio, Textsw_status  *status_ptr,
    char **error_msg_addr, CHAR *file, CHAR *name_wc, int *reset_mode,
	int *all_views, int *update_scrollbar, int *read_only_changed);

Pkg_private Textsw_status textsw_set_internal(Textsw_private priv, Textsw_view_private view, Attr_attribute *attrs, int is_folio)
/*
 * TRUE= we're setting attr's on the textsw_folio FALSE= we're setting attr's
 * on the textsw_view
 */
{
	Textsw self = TEXTSW_PUBLIC(priv);
	Textsw_status status, status_dummy;
	Textsw_status *status_ptr = &status_dummy;
	char error_dummy[256];
	char *error_msg = error_dummy;

#ifdef OW_I18N
	CHAR file[MAXPATHLEN];
	CHAR name_wc[MAXPATHLEN];

#ifdef FULL_R5
	XVaNestedList va_nested_list;
#endif /* FULL_R5 */
#else /* OW_I18N */
	char *file = NULL;

	/* Added because it is used on both sides of OW_I18N ifdef. */
	/* When OW_I18n is not defined, it is an unused parameter */
	/* of textsw_set_internal_tier2. */
	char *name_wc = NULL;
#endif /* OW_I18N */
	int reset_mode = -1;
	int temp;
	int all_views = 0;
	int display_views = 0;
	int update_scrollbar = 0;
	int read_only_changed = 0;
	int read_only_start;
	int row_height;
	int set_read_only_esh = 0;
	int str_length = 0;
	Es_handle ps_esh, scratch_esh, mem_esh;
	Es_status es_status;
	Es_index tmp;

/*
 * NOTE: This line of code  is no longer used
 * Remove it after a suitable grace period
    int    consume_attrs = 0;
 */

#ifdef OW_I18N
	int ic_active_changed = 0;
	extern CHAR _xv_null_string_wc[];

	file[0] = NULL;
#endif

	error_dummy[0] = '\0';

	/*
	 * This code will not handle view attr if view is null.
	 */

	status = TEXTSW_STATUS_OKAY;
	for (; *attrs; attrs = attr_next(attrs)) {

		switch (*attrs) {
			case XV_END_CREATE:
				if (view) {
					row_height = ei_plain_text_line_height(priv->views->eih);
					if (is_folio) {
						priv->orig_split_init_proc = (split_init_proc_t)xv_get(self,
													OPENWIN_SPLIT_INIT_PROC);
						xv_set(self,
							OPENWIN_SPLIT,
								OPENWIN_SPLIT_INIT_PROC, textsw_split_init_proc,
								NULL,
							NULL);
						if (defaults_get_boolean("text.enableScrollbar",
										"Text.EnableScrollbar", True))
						{
							Scrollbar sb;
							char instname[200], *owinst;
							Xv_pkg *pkg;

							owinst = (char *)xv_get(self, XV_INSTANCE_NAME);
							pkg = (Xv_pkg *)xv_get(self, XV_TYPE);
							sprintf(instname, "%s%sSB",
										owinst ? owinst : pkg->name,
										"vert");
							/* Create a scrollbar as a child of the textsw. */
							sb = xv_create(self, SCROLLBAR,
									XV_INSTANCE_NAME, instname,
									SCROLLBAR_DIRECTION, SCROLLBAR_VERTICAL,
									NULL);
							textsw_setup_scrollbar(sb);
							view->view_state &= ~TXTSW_SCROLLBAR_DISABLED;
						}
						else {
							view->view_state |= TXTSW_SCROLLBAR_DISABLED;
						}
						/*
						 * Set WIN_ROW_HEIGHT to the height of a line so that
						 * a subsequent xv_set of WIN_ROWS will work.
						 */
						xv_set(self, WIN_ROW_HEIGHT, row_height, NULL);
					}
					else {
						xv_set(VIEW_PUBLIC(view),
								WIN_ROW_HEIGHT, row_height, NULL);
					}

					/* don't want things to autoclear for us! jcb 5/15/90 */
					xv_set(self, OPENWIN_AUTO_CLEAR, FALSE, NULL);

#ifdef OW_I18N
					if (priv->need_im) {
						Textsw_view view_public = VIEW_PUBLIC(view);
						Xv_Drawable_info *info;

						DRAWABLE_INFO_MACRO(view_public, info);

						/*
						 * create IC, even read only case. Because the mode may be
						 * changed to Edit from Read-only mode later on.
						 */
						priv->ic = (XIC) xv_get(self, WIN_IC);

						if (priv->ic) {
							xv_set(view_public, WIN_IC, priv->ic, NULL);

#ifdef FULL_R5
							XGetICValues(priv->ic, XNInputStyle,
									&priv->xim_style, NULL);
							window_set_ic_focus_win(view_public, priv->ic,
									xv_xid(info));
#endif /* FULL_R5 */
						}
						if (!priv->ic || TXTSW_IS_READ_ONLY(priv) ||
								xv_get(self, WIN_IC_ACTIVE) == FALSE)
							xv_set(view_public, WIN_IC_ACTIVE, FALSE, NULL);
					}
					else
						xv_set(VIEW_PUBLIC(view), WIN_IC_ACTIVE, FALSE, NULL);
#endif /* OW_I18N */

					textsw_resize(view);
				}
				break;
			case TEXTSW_AGAIN_LIMIT:
				/* Internally we want one more again slot that client does.  */
				temp = (int)(attrs[1]);
				temp = (temp > 0) ? temp + 1 : 0;
				textsw_init_again(priv, temp);
				break;
			case TEXTSW_AGAIN_RECORDING:
				SET_BOOL_FLAG(priv->state, !attrs[1],
						TXTSW_NO_AGAIN_RECORDING);
				break;
			case TEXTSW_AUTO_INDENT:
				SET_BOOL_FLAG(priv->state, attrs[1], TXTSW_AUTO_INDENT);
				break;
			case TEXTSW_AUTO_SCROLL_BY:
				(void)ev_set(view->e_view,
						EV_CHAIN_AUTO_SCROLL_BY, (int)(attrs[1]), NULL);
				break;
			case TEXTSW_CLIENT_DATA:
				priv->client_data = attrs[1];
				break;
			case TEXTSW_CONTROL_CHARS_USE_FONT:
				(void)ei_set(priv->views->eih,
						EI_CONTROL_CHARS_USE_FONT, attrs[1], NULL);
				break;
			case TEXTSW_DISABLE_CD:
				SET_BOOL_FLAG(priv->state, attrs[1], TXTSW_NO_CD);
				break;
			case TEXTSW_DISABLE_LOAD:
				SET_BOOL_FLAG(priv->state, attrs[1], TXTSW_NO_LOAD);
				if (priv->menu_table) {
					if (priv->menu_table[TEXTSW_MENU_LOAD]) {
						xv_set(priv->menu_table[TEXTSW_MENU_LOAD],
								MENU_INACTIVE, attrs[1],
								NULL);
					}
				}
				break;
			case TEXTSW_ES_CREATE_PROC:
				priv->es_create = (Es_handle(*)(Xv_opaque, Es_handle, Es_handle))attrs[1];
				break;
			case TEXTSW_FILE:

#ifndef OW_I18N
				file = (char *)attrs[1];
#else /* OW_I18N */
				if ((char *)(attrs[1]) == (char *)NULL)
					file[0] = NULL;
				else
					(void)mbstowcs(file, (char *)(attrs[1]), MAXPATHLEN);
#endif /* OW_I18N */

				break;

			case TEXTSW_FILE_CONTENTS:
				textsw_flush_caches(view, TFC_PD_SEL);
				ps_esh = ES_NULL;
				str_length = (attrs[1] ? strlen((char *)attrs[1]) : 0);

#ifdef OW_I18N
				if (str_length > 0)
					(void)mbstowcs(name_wc, (char *)(attrs[1]), MAXPATHLEN);
				goto Do_create_file;

			case TEXTSW_FILE_CONTENTS_WCS:
				textsw_flush_caches(view, TFC_PD_SEL);
				ps_esh = ES_NULL;
				str_length = (attrs[1] ? STRLEN((CHAR *) attrs[1]) : 0);
				if (str_length > 0)
					STRCPY(name_wc, (CHAR *) (attrs[1]));

			  Do_create_file:
				textsw_implicit_commit(priv);
				if (STRLEN(file) == 0) {	/* } for match */
					if (str_length > 0) {	/* } for match */
						scratch_esh = es_file_create(name_wc, 0, &es_status);
#else /* OW_I18 */
				if (!file) {
					if (str_length > 0) {
						scratch_esh = es_file_create((char *)attrs[1], 0, &es_status);
#endif /* OW_I18N */

						/* Ensure no caret turds will leave behind */
						textsw_take_down_caret(priv);
					}
					else {
						scratch_esh = priv->views->esh;
					}

					if (scratch_esh) {

#ifdef OW_I18N
						SET_CONTENTS_UPDATED(priv, TRUE);
						if ((int)es_get(scratch_esh, ES_SKIPPED))
							textsw_invalid_data_notice(view, name_wc, 1);
						mem_esh = es_mem_create(es_get_length(scratch_esh) + 1,
								_xv_null_string_wc);
#else /* OW_I18N */
						mem_esh = es_mem_create((unsigned)es_get_length(scratch_esh) + 1, "");
#endif /* OW_I18N */

						if (mem_esh) {
							if (es_copy(scratch_esh, mem_esh,
											FALSE) != ES_SUCCESS) {
								es_destroy(mem_esh);
								mem_esh = ES_NULL;
							}
						}
						if (str_length > 0) {
							es_destroy(scratch_esh);
						}
						if (mem_esh) {
							scratch_esh = es_mem_create(priv->es_mem_maximum,

#ifdef OW_I18N
									_xv_null_string_wc);
#else
									"");
#endif

							if (scratch_esh) {
								ps_esh = textsw_create_ps(priv, mem_esh,
										scratch_esh, &es_status);
							}
							else {
								es_destroy(mem_esh);
							}
						}
					}
				}
				if (ps_esh) {
					textsw_replace_esh(priv, ps_esh);

					if (str_length > 0) {
						Ev_handle ev_next;
						Ev_impl_line_seq seq;

						EV_SET_INSERT(priv->views, es_get_length(ps_esh),
								tmp);

						FORALLVIEWS(priv->views, ev_next) {
							seq = (Ev_impl_line_seq) ev_next->line_table.seq;
							seq[0].damaged = 0;	/* Set damage bit in line
												 * table to force redisplay  */
							ev_update_view_display(ev_next);
						}
						display_views = 2;	/* TEXTSW_FIRST will set it to 0 to
											 * avoid repaint */
						all_views = TRUE;	/* For TEXTSW_FIRST, or
											 * TEXTSW_FIRST_LINE */
						textsw_invert_caret(priv);
					}
				}
				else {
					*status_ptr = TEXTSW_STATUS_OTHER_ERROR;
				}
				break;

			case TEXTSW_FIRST:

#ifdef OW_I18N
				*status_ptr = set_first(view, error_msg, file, reset_mode,
						textsw_wcpos_from_mbpos(priv, (Es_index) (attrs[1])),
						-1, all_views);
				goto MakeSure;
			case TEXTSW_FIRST_WC:
#endif /* OW_I18N */

				*status_ptr = set_first(view, error_msg, file, reset_mode,
						(Es_index) (attrs[1]), -1, all_views);
				goto MakeSure;
			case TEXTSW_FIRST_LINE:
				*status_ptr = set_first(view, error_msg, file, reset_mode,
						ES_CANNOT_SET, (int)(attrs[1]), all_views);
			  MakeSure:
				/*
				 * Make sure the scrollbar get updated, when this attribute is
				 * called with TEXT_FIRST.
				 */

#ifdef OW_I18N
				if ((STRLEN(file) > 0) && !update_scrollbar)
					update_scrollbar = 2;
				file[0] = NULL;
#else
				if (file && !update_scrollbar)
					update_scrollbar = 2;
				file = NULL;
#endif /* OW_I18N */

				display_views = 0;
				break;
			case TEXTSW_CHECKPOINT_FREQUENCY:
				priv->checkpoint_frequency = (int)(attrs[1]);
				break;
			case TEXTSW_HISTORY_LIMIT:
				/* Internally we want one more again slot that client does.  */
				temp = (int)(attrs[1]);
				temp = (temp > 0) ? temp + 1 : 0;
				textsw_init_undo(priv, temp);
				break;
			case TEXTSW_IGNORE_LIMIT:
				priv->ignore_limit = (unsigned)(attrs[1]);
				break;
			case TEXTSW_INSERTION_POINT:

#ifdef OW_I18N
				textsw_implicit_commit(priv);
				(void)textsw_set_insert(priv,
						textsw_wcpos_from_mbpos(priv, (Es_index) (attrs[1])));
				break;
			case TEXTSW_INSERTION_POINT_WC:
				textsw_implicit_commit(priv);
#endif /* OW_I18N */

				(void)textsw_set_insert(priv, (Es_index) (attrs[1]));
				break;
			case TEXTSW_LINE_BREAK_ACTION:{
					Ev_right_break ev_break_action;

					switch ((Textsw_enum) attrs[1]) {
						case TEXTSW_CLIP:
							ev_break_action = EV_CLIP;
							goto TLBA_Tail;
						case TEXTSW_WRAP_AT_CHAR:
							ev_break_action = EV_WRAP_AT_CHAR;
							goto TLBA_Tail;
						case TEXTSW_WRAP_AT_WORD:
							ev_break_action = EV_WRAP_AT_WORD;
						  TLBA_Tail:
							(void)ev_set(view->e_view,
									(all_views) ?
									EV_FOR_ALL_VIEWS : EV_END_ALL_VIEWS,
									EV_RIGHT_BREAK, ev_break_action, NULL);
							display_views = (all_views) ? 2 : 1;
							break;
						default:
							*status_ptr = TEXTSW_STATUS_BAD_ATTR_VALUE;
							break;
					}
					break;
				}
			case TEXTSW_LOWER_CONTEXT:
				(void)ev_set(view->e_view,
						EV_CHAIN_LOWER_CONTEXT, (int)(attrs[1]), NULL);
				break;

			case TEXTSW_SMART_WORD_HANDLING:
				priv->smart_word_handling = (int)attrs[1];
				break;

			case TEXTSW_MEMORY_MAXIMUM:
				priv->es_mem_maximum = (unsigned)(attrs[1]);
				if (priv->es_mem_maximum == 0) {
					priv->es_mem_maximum = TEXTSW_INFINITY;
				}
				else if (priv->es_mem_maximum < 128)
					priv->es_mem_maximum = 128;
				break;
			case TEXTSW_NO_RESET_TO_SCRATCH:
				SET_BOOL_FLAG(priv->state, attrs[1],
						TXTSW_NO_RESET_TO_SCRATCH);
				break;
			case TEXTSW_NOTIFY_LEVEL:
				priv->notify_level = (int)(attrs[1]);
				break;
			case TEXTSW_NOTIFY_PROC:
				priv->notify = (int (*)(Xv_opaque, Attr_avlist))attrs[1];
				if (priv->notify_level == 0)
					priv->notify_level = TEXTSW_NOTIFY_STANDARD;
				break;
			case TEXTSW_READ_ONLY:
				read_only_start = TXTSW_IS_READ_ONLY(priv);

#ifdef OW_I18N
				/* to Read-only mode from Edit mode */
				if (!read_only_start && attrs[1] == TRUE)
					textsw_implicit_commit(priv);
#endif

				SET_BOOL_FLAG(priv->state, attrs[1], TXTSW_READ_ONLY_ESH);
				set_read_only_esh = (priv->state & TXTSW_READ_ONLY_ESH);
				read_only_changed =
						(read_only_start != TXTSW_IS_READ_ONLY(priv));
				break;
			case TEXTSW_STATUS:
				status_ptr = (Textsw_status *) attrs[1];
				*status_ptr = TEXTSW_STATUS_OKAY;
				break;
			case TEXTSW_TAB_WIDTH:
				(void)ei_set(priv->views->eih, EI_TAB_WIDTH, attrs[1], NULL);
				break;
			case TEXTSW_TEMP_FILENAME:
				if (priv->temp_filename)
					free(priv->temp_filename);
				priv->temp_filename = strdup((char *)attrs[1]);
				break;
			case TEXTSW_UPPER_CONTEXT:
				(void)ev_set(view->e_view,
						EV_CHAIN_UPPER_CONTEXT, (int)(attrs[1]), NULL);
				break;
			case TEXTSW_WRAPAROUND_SIZE:
				es_set(priv->views->esh,
						ES_PS_SCRATCH_MAX_LEN, attrs[1], NULL);
				break;
			case TEXTSW_ACCELERATE_MENUS:
				/*
				 * Do only if the current state is different
				 */
				if (priv->accel_menus != attrs[1]) {
					/*
					 * Do only if the menu has been created
					 */
					if (textsw_menu_get(priv)) {
						/*
						 * Get frame handle
						 */
						Frame frame = (Frame) xv_get(self, WIN_FRAME);

						/*
						 * Do only if frame handle != NULL
						 */
						if (frame) {
							/*
							 * Add/Delete menu to accelerate
							 */
							if (attrs[1]) {
								xv_set(frame, FRAME_MENU_ADD,
										textsw_menu_get(priv), NULL);
							}
							else {
								xv_set(frame, FRAME_MENU_DELETE,
										textsw_menu_get(priv), NULL);
							}

							/*
							 * Update state
							 */
							priv->accel_menus = attrs[1];
						}
					}
				}
				break;

#ifdef XV_DEBUG
			case TEXTSW_MALLOC_DEBUG_LEVEL:
				malloc_debug((int)attrs[1]);
				break;
#endif

/*
 * NOTE: This code no longer used.
 * Remove after suitable grace period
	  case TEXTSW_CONSUME_ATTRS:
	    consume_attrs = (attrs[1] ? 1 : 0);
	    break;
 */

				/* Super-class attributes that we monitor. */
			case WIN_FONT:
				if (attrs[1]) {
					Ev_handle ev_next;

					if (priv->state & TXTSW_INITIALIZED) {
						/* BUG ALERT!  Is this needed any longer? */
						if (priv->state & TXTSW_OPENED_FONT) {
							PIXFONT *old_font;

							old_font = (PIXFONT *)
									ei_get(priv->views->eih, EI_FONT);

							if (old_font == (PIXFONT *) attrs[1])
								break;

							xv_pf_close(old_font);
							priv->state &= ~TXTSW_OPENED_FONT;
						}
						(void)ei_set(priv->views->eih,
								EI_FONT, attrs[1], NULL);

#ifdef FULL_R5

#ifdef OW_I18N
						if (priv->ic
								&& (priv->
										xim_style & (XIMPreeditArea |
												XIMPreeditPosition |
												XIMPreeditNothing))) {
							va_nested_list =
									XVaCreateNestedList(NULL, XNLineSpace,
									(int)ei_get(priv->views->eih,
											EI_LINE_SPACE), NULL);

							XSetICValues(priv->ic, XNPreeditAttributes,
									va_nested_list, NULL);
							XFree(va_nested_list);
						}
#endif /* OW_I18N */
#endif /* FULL_R5 */
					}
					/* Adjust the views to account for the font change */
					FORALLVIEWS(priv->views, ev_next) {
						(void)ev_set(ev_next,
								EV_CLIP_RECT, &ev_next->rect,
								EV_RECT, &ev_next->rect, NULL);
					}
				}
				break;
			case WIN_MENU:
				textsw_menu_set(priv, attrs[1]);
				break;

				/* Super-class attributes that we override. */
			case XV_LEFT_MARGIN:
				*attrs = (Textsw_attribute) ATTR_NOP(*attrs);
				(void)ev_set(view->e_view,
						(all_views) ?
						EV_FOR_ALL_VIEWS : EV_END_ALL_VIEWS,
						EV_LEFT_MARGIN, (int)(attrs[1]), NULL);
				display_views = (all_views) ? 2 : 1;
				break;
			case XV_RIGHT_MARGIN:
				*attrs = (Textsw_attribute) ATTR_NOP(*attrs);
				(void)ev_set(view->e_view,
						(all_views) ?
						EV_FOR_ALL_VIEWS : EV_END_ALL_VIEWS,
						EV_RIGHT_MARGIN, (int)(attrs[1]), NULL);
				display_views = (all_views) ? 2 : 1;
				break;

			case WIN_REMOVE_CARET:
				textsw_hide_caret(priv);
				break;

			case WIN_SET_FOCUS:{
					Xv_Drawable_info *view_info;
					Xv_Window view_public;
					int view_nbr;

					if (!is_folio)
						break;
					/* Set the focus to the first Openwin view */
					*attrs = (Textsw_attribute) ATTR_NOP(*attrs);
					status = TEXTSW_STATUS_OTHER_ERROR;
					for (view_nbr = 0;; view_nbr++) {
						view_public = xv_get(self,
								OPENWIN_NTH_VIEW, view_nbr);
						if (!view_public)
							break;
						DRAWABLE_INFO_MACRO(view_public, view_info);
						if (!xv_no_focus(view_info) &&
								win_getinputcodebit((Inputmask *)
										xv_get(view_public, WIN_INPUT_MASK),
										KBD_USE)) {
							win_set_kbd_focus(view_public, xv_xid(view_info));
							status = TEXTSW_STATUS_OKAY;
							break;
						}
					}
					break;
				}

#ifdef OW_I18N
			case WIN_IC_ACTIVE:
				if (is_folio && priv->need_im) {
					ic_active_changed =
							(attrs[1] ==
							TRUE) ? IC_ACTIVE_TRUE : IC_ACTIVE_FALSE;
					if (attrs[1] == FALSE && priv->ic
							&& (int)xv_get(self, WIN_IC_CONVERSION)) {
						textsw_implicit_commit(priv);
						xv_set(self, WIN_IC_CONVERSION, FALSE, NULL);
					}

					if (TXTSW_IS_READ_ONLY(priv) && attrs[1] == TRUE)
						break;
					FORALL_TEXT_VIEWS(priv, next) {
						xv_set(VIEW_PUBLIC(next), WIN_IC_ACTIVE, attrs[1],
								NULL);
					}
				}
				break;
#endif /* OW_I18N */

			case TEXTSW_COALESCE_WITH:	/* Get only */
			case TEXTSW_BLINK_CARET:
				/*
				 * jcb 4/29/89  SET_BOOL_FLAG(priv->caret_state, attrs[1],
				 * TXTSW_CARET_FLASHING);
				 */
				break;

			case TEXTSW_FOR_ALL_VIEWS:
			case TEXTSW_END_ALL_VIEWS:
			case TEXTSW_ADJUST_IS_PENDING_DELETE:
			case TEXTSW_BROWSING:
			case TEXTSW_CONFIRM_OVERWRITE:
			case TEXTSW_CONTENTS:
			case TEXTSW_DESTROY_ALL_VIEWS:
			case TEXTSW_DIFFERENTIATE_CR_LF:	/*1030878 */
			case TEXTSW_STORE_CHANGES_FILE:
			case TEXTSW_EDIT_BACK_CHAR:
			case TEXTSW_EDIT_BACK_WORD:
			case TEXTSW_EDIT_BACK_LINE:
			case TEXTSW_ERROR_MSG:
			case TEXTSW_INSERT_FROM_FILE:

#ifdef OW_I18N
			case TEXTSW_FILE_WCS:
			case TEXTSW_CONTENTS_WCS:
			case TEXTSW_INSERT_FROM_FILE_WCS:
#endif /* OW_I18N */

			case TEXTSW_INSERT_MAKES_VISIBLE:
			case TEXTSW_MULTI_CLICK_SPACE:
			case TEXTSW_MULTI_CLICK_TIMEOUT:
			case TEXTSW_NO_REPAINT_TIL_EVENT:
			case TEXTSW_RESET_MODE:
			case TEXTSW_RESET_TO_CONTENTS:
			case TEXTSW_TAB_WIDTHS:
			case TEXTSW_UPDATE_SCROLLBAR:
			case WIN_CMS_CHANGE:
				textsw_set_internal_tier2(priv, view, attrs, is_folio,
						status_ptr, &error_msg, file, name_wc, &reset_mode,
						&all_views, &update_scrollbar, &read_only_changed);
				break;

			default:
				(void)xv_check_bad_attr(&xv_textsw_pkg,
						(Attr_attribute) attrs[0]);
				break;
		}
	}

#ifdef OW_I18N
	if (STRLEN(file) > 0) {	/* } for match */
		textsw_implicit_commit(priv);
#else
	if (file) {
#endif

		*status_ptr = set_first(view, error_msg, file, reset_mode,
									ES_CANNOT_SET, -1, all_views);
		/*
		 * This is for resetting the TXTSW_READ_ONLY_ESH flag that got
		 * cleared in textsw_replace_esh
		 */
		if (set_read_only_esh)
			priv->state |= TXTSW_READ_ONLY_ESH;

		display_views = 0;
	}
	if (display_views && (priv->state & TXTSW_DISPLAYED)) {
		Textsw_view vp;

		OPENWIN_EACH_VIEW(self, vp)
			Textsw_view_private tmpview = VIEW_PRIVATE(vp);
			if ((display_views == 1) && (tmpview != view))
				continue;
			textsw_display_view_margins(tmpview, RECT_NULL);
			ev_display_view(tmpview->e_view);
		OPENWIN_END_EACH
		update_scrollbar = display_views;
	}
	if (update_scrollbar) {
		textsw_update_scrollbars(priv,
				(update_scrollbar == 2) ? (Textsw_view_private) 0 : view);
	}
	if (read_only_changed) {
		if (TXTSW_IS_READ_ONLY(priv))
			textsw_hide_caret(priv);
		else
			textsw_show_caret(priv);

#ifdef OW_I18N
		if (priv->need_im) {
			if ((ic_active_changed == IC_ACTIVE_TRUE) ||
					(!ic_active_changed
							&& xv_get(self, WIN_IC_ACTIVE))) {

				FORALL_TEXT_VIEWS(priv, next) {
					xv_set(VIEW_PUBLIC(next), WIN_IC_ACTIVE,
							(TXTSW_IS_READ_ONLY(priv) ? FALSE : TRUE), NULL);
				}
			}
		}
#endif /* OW_I18N */
	}
	return (status);
}

Pkg_private void
textsw_set_internal_tier2(Textsw_private textsw, Textsw_view_private view,
    Attr_attribute *attrs, int is_folio, Textsw_status  *status_ptr,
    char **error_msg_addr, CHAR *file, CHAR *name_wc, int *reset_mode,
	int *all_views, int *update_scrollbar, int *read_only_changed)
/*
 * TRUE= we're setting attr's on the textsw_folio FALSE= we're setting attr's
 * on the textsw_view
 */
{
    int    temp;
    int             read_only_start;
#ifndef LEFT_HAND_SIDE_CAST
    int            *int_ptr;
#endif /* LEFT_HAND_SIDE_CAST */

    switch (*attrs) {
          case TEXTSW_FOR_ALL_VIEWS:
            *all_views = TRUE;
            break;
          case TEXTSW_END_ALL_VIEWS:
            *all_views = FALSE;
            break;

          case TEXTSW_ADJUST_IS_PENDING_DELETE:
            SET_BOOL_FLAG(textsw->state, attrs[1], TXTSW_ADJUST_IS_PD);
            break;
          case TEXTSW_BROWSING:
            read_only_start = TXTSW_IS_READ_ONLY(textsw);
#ifdef OW_I18N
            /* to Read-only mode from Edit mode */
            if (!read_only_start && attrs[1] == TRUE)
                textsw_implicit_commit(textsw);
#endif
            SET_BOOL_FLAG(textsw->state, attrs[1], TXTSW_READ_ONLY_SW);
            *read_only_changed = (read_only_start!=TXTSW_IS_READ_ONLY(textsw));            break;

          case TEXTSW_CONFIRM_OVERWRITE:
            SET_BOOL_FLAG(textsw->state, attrs[1],
                          TXTSW_CONFIRM_OVERWRITE);
            break;
          case TEXTSW_CONTENTS:
            temp = (textsw->state & TXTSW_NO_AGAIN_RECORDING);
            if (!(textsw->state & TXTSW_INITIALIZED))
                textsw->state |= TXTSW_NO_AGAIN_RECORDING;
#ifdef OW_I18N
            (void) textsw_replace_bytes(VIEW_PUBLIC(view), 0,
#else
            textsw_replace(VIEW_PUBLIC(view), 0,
#endif
                   TEXTSW_INFINITY, (char *)attrs[1],
				   (long)strlen((char *)attrs[1]));
			if (!(textsw->state & TXTSW_INITIALIZED)) {
                SET_BOOL_FLAG(textsw->state, temp, TXTSW_NO_AGAIN_RECORDING);
			}
            break;
#ifdef OW_I18N
          case TEXTSW_CONTENTS_WCS:
            temp = (textsw->state & TXTSW_NO_AGAIN_RECORDING);
            if (!(textsw->state & TXTSW_INITIALIZED))
                textsw->state |= TXTSW_NO_AGAIN_RECORDING;
            textsw_implicit_commit(textsw);
            (void) textsw_replace(VIEW_PUBLIC(view), 0,
                   TEXTSW_INFINITY, (CHAR *)attrs[1], STRLEN((CHAR *)attrs[1]));            if (!(textsw->state & TXTSW_INITIALIZED))
                SET_BOOL_FLAG(textsw->state, temp,
                              TXTSW_NO_AGAIN_RECORDING);
            break;
#endif /* OW_I18N */

          case TEXTSW_DESTROY_ALL_VIEWS:
            SET_BOOL_FLAG(textsw->state, attrs[1],
                          TXTSW_DESTROY_ALL_VIEWS);
            break;
          case TEXTSW_DIFFERENTIATE_CR_LF: /*1030878*/
            SET_BOOL_FLAG(textsw->state, attrs[1], TXTSW_DIFF_CR_LF);
            break;
          case TEXTSW_STORE_CHANGES_FILE:
            SET_BOOL_FLAG(textsw->state, attrs[1],
                          TXTSW_STORE_CHANGES_FILE);
            break;
          case TEXTSW_EDIT_BACK_CHAR:
            textsw->edit_bk_char = (char) (attrs[1]);
            break;
          case TEXTSW_EDIT_BACK_WORD:
            textsw->edit_bk_word = (char) (attrs[1]);
            break;
          case TEXTSW_EDIT_BACK_LINE:
            textsw->edit_bk_line = (char) (attrs[1]);
            break;
          case TEXTSW_ERROR_MSG:
            *error_msg_addr = (char *) (attrs[1]);
            (*error_msg_addr)[0] = '\0';
            break;
#ifdef OW_I18N
          case TEXTSW_FILE_WCS:
            if ((CHAR *)(attrs[1]) == (CHAR *)NULL)
                file[0]  = NULL;
            else
                (void) STRCPY(file, (CHAR *) (attrs[1]));
            break;
#endif /* OW_I18N */

          case TEXTSW_INSERT_FROM_FILE:{
#ifdef OW_I18N
                (void) mbstowcs(name_wc, (char *) (attrs[1]), MAXPATHLEN);
                *status_ptr = textsw_get_from_file(view, name_wc, TRUE);
                if (*status_ptr == TEXTSW_STATUS_OKAY)
                    *update_scrollbar = 2;
                break;
            };
          case TEXTSW_INSERT_FROM_FILE_WCS:{
#endif /* OW_I18N */
                *status_ptr = textsw_get_from_file(view, (CHAR *) (attrs[1]),
                                             TRUE);
                if (*status_ptr == TEXTSW_STATUS_OKAY)
                    *update_scrollbar = 2;
                break;
            };
          case TEXTSW_INSERT_MAKES_VISIBLE:
            switch ((Textsw_enum) attrs[1]) {
              case TEXTSW_ALWAYS:
              case TEXTSW_IF_AUTO_SCROLL:
                textsw->insert_makes_visible = (Textsw_enum) attrs[1];
                break;
              default:
                *status_ptr = TEXTSW_STATUS_BAD_ATTR_VALUE;
                break;
            }
            break;

          case TEXTSW_MULTI_CLICK_SPACE:
            if ((int) (attrs[1]) != -1)
                textsw->multi_click_space = (int) (attrs[1]);
            break;
          case TEXTSW_MULTI_CLICK_TIMEOUT:
            if ((int) (attrs[1]) != -1)
                textsw->multi_click_timeout = (int) (attrs[1]);
            break;
          case TEXTSW_NO_REPAINT_TIL_EVENT:
            ev_set(view->e_view, EV_NO_REPAINT_TIL_EVENT, (int) (attrs[1]),
                   NULL);
            break;
           case TEXTSW_RESET_MODE:
            *reset_mode = (int) (attrs[1]);
            break;
          case TEXTSW_RESET_TO_CONTENTS:
            textsw_reset_2(TEXTSW_PUBLIC(textsw), 0, 0, TRUE, FALSE);
            break;
           case TEXTSW_TAB_WIDTHS:
#ifdef LEFT_HAND_SIDE_CAST
            /* XXX cheat here */
            *(int *) attrs = (int) EI_TAB_WIDTHS;
#else
            int_ptr = (int *) attrs;
            *int_ptr = (int) EI_TAB_WIDTHS;
#endif /* LEFT_HAND_SIDE_CAST */
            ei_plain_text_set(textsw->views->eih, attrs);
            /* (void) ei_set(textsw->views->eih, EI_TAB_WIDTHS, attrs[1], 0); */            break;
          case TEXTSW_UPDATE_SCROLLBAR:
            *update_scrollbar = (all_views) ? 2 : 1;
            break;

          case WIN_CMS_CHANGE:
            if (is_folio) {
                Xv_Window       textsw_public = TEXTSW_PUBLIC(textsw);
                Xv_Window       view_public;
/*                 Textsw_view_private view_next; */
                Xv_Drawable_info *info;
                Cms             cms;

                DRAWABLE_INFO_MACRO(textsw_public, info);
                cms = xv_cms(info);
				OPENWIN_EACH_VIEW(textsw_public, view_public)
                    window_set_cms(view_public, cms,
									xv_cms_bg(info), xv_cms_fg(info));
				OPENWIN_END_EACH
            } else {
                textsw_view_cms_change(textsw, view);
            }
            break;
    }
}


static Defaults_pairs insert_makes_visible_pairs[] = {
    { "If_auto_scroll", (int) TEXTSW_IF_AUTO_SCROLL },
    { "Always", (int) TEXTSW_ALWAYS },
    { NULL, (int) TEXTSW_IF_AUTO_SCROLL }
};


static Defaults_pairs line_break_pairs[] = {
    { "Clip", (int) TEXTSW_CLIP },
    { "Wrap_char", (int) TEXTSW_WRAP_AT_CHAR },
    { "Wrap_word", (int) TEXTSW_WRAP_AT_WORD },
    { NULL, (int) TEXTSW_WRAP_AT_WORD }
};


static	void textsw_view_cms_change(Textsw_private textsw, Textsw_view_private view)
{
    ev_set(view->e_view, EV_NO_REPAINT_TIL_EVENT, FALSE, NULL);
    textsw_repaint(view);
    /* if caret was up and we took it down, put it back */
    if ((textsw->caret_state & TXTSW_CARET_ON)
	&& (textsw->caret_state & TXTSW_CARET_ON) == 0) {
	textsw_remove_timer(textsw);
	textsw_timer_expired(textsw, 0);
    }
}

Pkg_private long textsw_get_from_defaults(Attr32_attribute attribute, Xv_opaque srv)
{
	char *def_str;	/* Points to strings owned by defaults. */

	switch (attribute) {
		case TEXTSW_ADJUST_IS_PENDING_DELETE:
			return ((long)True);
		case TEXTSW_AGAIN_LIMIT:
			return ((long)
					defaults_get_integer_check("text.againLimit",
							"Text.AgainLimit", 1, 0, 500));
		case TEXTSW_AGAIN_RECORDING:
			return ((long)
					defaults_get_boolean("text.againRecording",
							"Text.againRecording", True));
		case TEXTSW_AUTO_INDENT:
			return ((long)
					defaults_get_boolean("text.autoIndent",
							"Text.AutoIndent", False));
		case TEXTSW_AUTO_SCROLL_BY:
			return ((long)
					defaults_get_integer_check("text.autoScrollBy",
							"Text.AutoScrollBy", 1, 0, 100));
		case TEXTSW_BLINK_CARET:
/* #ifdef notdef */
			/* BUG: always return FALSE for alpha4 performance */
			return ((long)
					defaults_get_boolean("text.blinkCaret", "Text.BlinkCaret",
							True));
/* #endif */
			return (long)FALSE;
		case TEXTSW_CHECKPOINT_FREQUENCY:
			/* Not generally settable via defaults */
			return ((long)0);
		case TEXTSW_CONFIRM_OVERWRITE:
			return ((long)
					defaults_get_boolean("text.confirmOverwrite",
							"Text.ConfirmOverwrite", True));
		case TEXTSW_CONTROL_CHARS_USE_FONT:
			return ((long)
					defaults_get_boolean("text.displayControlChars",
							"Text.DisplayControlChars", False));
		case TEXTSW_EDIT_BACK_CHAR:
			return ((long)
					defaults_get_character("keyboard.deleteChar", "Keyboard.DeleteChar", DEL));	/* ??? Keymapping  strategy? */
		case TEXTSW_EDIT_BACK_WORD:
			return ((long)
					defaults_get_character("keyboard.deleteWord",

#ifndef __STDC__
							"Keyboard.DeleteWord", CTRL(W)));	/* ??? Keymapping strategy? */
#else /* __STDC__ */
							"Keyboard.DeleteWord", CTRL('W')));	/* ??? Keymapping strategy? */
#endif /* __STDC__ */

		case TEXTSW_EDIT_BACK_LINE:
			return ((long)
					defaults_get_character("keyboard.deleteLine",

#ifndef __STDC__
							"Keyboard.DeleteLine", CTRL(U)));	/* ??? Keymapping strategy? */
#else /* __STDC__ */
							"Keyboard.DeleteLine", CTRL('U')));	/* ??? Keymapping strategy? */
#endif /* __STDC__ */

		case WIN_FONT:{
				PIXFONT *font;

				/* Text.d may have "" rather than NULL, so check for this case.  */

#ifdef OW_I18N
				defaults_set_locale(NULL, XV_LC_BASIC_LOCALE);
				def_str = xv_font_monospace();
				defaults_set_locale(NULL, NULL);
#else
				def_str = xv_font_monospace();
#endif /* OW_I18N */

				font = (def_str && ((int)strlen(def_str) > 0))
						? xv_pf_open(def_str, srv) : 0;
				return ((long)font);
			}
		case TEXTSW_HISTORY_LIMIT:
			return ((long)
					defaults_get_integer_check("text.undoLimit",
							"Text.UndoLimit", 50, 0, 500));
		case TEXTSW_INSERT_MAKES_VISIBLE:
			def_str = defaults_get_string("text.insertMakesCaretVisible",
					"Text.InsertMakesCaretVisible", (char *)0);
			if (def_str && ((int)strlen(def_str) > 0)) {
				return ((long)
						defaults_lookup(def_str, insert_makes_visible_pairs));
			}
			else
				return (long)TEXTSW_IF_AUTO_SCROLL;
		case TEXTSW_LINE_BREAK_ACTION:
			def_str = defaults_get_string("text.lineBreak",
					"Text.LineBreak", (char *)0);
			if (def_str && ((int)strlen(def_str) > 0)) {
				return ((long)
						defaults_lookup(def_str, line_break_pairs));
			}
			else
				return (long)TEXTSW_WRAP_AT_WORD;
		case TEXTSW_LOWER_CONTEXT:
			return ((long)
					defaults_get_integer_check("text.margin.bottom",
							"Text.Margin.Bottom", 0, EV_NO_CONTEXT, 50));
			/* ??? Implement Text.EnableScrolling */
		case TEXTSW_MULTI_CLICK_SPACE:
			return ((long)
					defaults_get_integer_check("mouse.multiclick.space", "Mouse.Multiclick.Space", 4, 0, 500));	/* ??? OL-compliant? */
		case TEXTSW_MULTI_CLICK_TIMEOUT:
			return ((long)(100 *
							defaults_get_integer_check
							("openWindows.multiClickTimeout",
									"OpenWindows.MultiClickTimeout", 4, 2,
									10)));
		case TEXTSW_STORE_CHANGES_FILE:
			return ((long)
					defaults_get_boolean("text.storeChangesFile",
							"Text.StoreChangesFile", True));
		case TEXTSW_UPPER_CONTEXT:
			return ((long)
					defaults_get_integer_check("text.margin.top",
							"Text.Margin.Top", 2, EV_NO_CONTEXT, 50));
		case XV_LEFT_MARGIN:
			return ((long)
					defaults_get_integer_check("text.margin.left",
							"Text.Margin.Left", 8, 0, 2000));
		case XV_RIGHT_MARGIN:
			return ((long)
					defaults_get_integer_check("text.margin.right",
							"Text.Margin.Right", 0, 0, 2000));
		case TEXTSW_TAB_WIDTH:
			return ((long)
					defaults_get_integer_check("text.tabWidth",
							"Text.TabWidth", 8, 0, 50));
		default:
			return ((long)0);
	}
}

/* Caller turns varargs into va_list that has already been va_start'd */
static Xv_opaque textsw_get_internal(Textsw_private priv,
									Textsw_view_private view,
    				int *status, Textsw_attribute attribute, va_list args)
{

	/* If view is not created yet, return zero for this attrs */
	switch ((Attr_attribute)attribute) {
		case TEXTSW_INSERTION_POINT:
		case TEXTSW_LENGTH:
		case TEXTSW_FIRST:
		case TEXTSW_FIRST_LINE:
		case XV_LEFT_MARGIN:
		case XV_RIGHT_MARGIN:
		case WIN_VERTICAL_SCROLLBAR:
		case TEXTSW_CONTENTS:

#ifdef OW_I18N
		case TEXTSW_CONTENTS_WCS:
		case TEXTSW_CONTENTS_NO_COMMIT:
		case TEXTSW_CONTENTS_WCS_NO_COMMIT:
		case TEXTSW_LENGTH_WC:
		case TEXTSW_INSERTION_POINT_WC:
		case TEXTSW_FIRST_WC:
#endif /* OW_I18N */

		case TEXTSW_EDIT_COUNT:
			if (!view) {
				*status = XV_ERROR;
				return ((Xv_opaque) 0);
			}
			break;
	}

	/*
	 * Note that ev_get(chain, EV_CHAIN_xxx) casts chain to be view in order
	 * to keep lint happy.
	 */
	switch ((Attr_attribute)attribute) {
		case TEXTSW_LENGTH:

#ifdef OW_I18N
			textsw_flush_caches(view, TFC_STD);
			return ((Xv_opaque) textsw_get_mb_length(priv));
		case TEXTSW_LENGTH_WC:
#endif /* OW_I18N */

			textsw_flush_caches(view, TFC_STD);
			return ((Xv_opaque) es_get_length(priv->views->esh));
		case TEXTSW_INSERTION_POINT:

#ifdef OW_I18N
			textsw_flush_caches(view, TFC_STD);
			return ((Xv_opaque) textsw_mbpos_from_wcpos(priv,
							EV_GET_INSERT(priv->views)));
		case TEXTSW_INSERTION_POINT_WC:
#endif

			textsw_flush_caches(view, TFC_STD);
			return ((Xv_opaque) EV_GET_INSERT(priv->views));
		case TEXTSW_LOWER_CONTEXT:
			return ((Xv_opaque) ev_get((Ev_handle) (view->e_view),
							EV_CHAIN_LOWER_CONTEXT, XV_NULL, XV_NULL, XV_NULL));
		case TEXTSW_ACCELERATE_MENUS:
			return ((Xv_opaque) priv->accel_menus);
		case TEXTSW_ADJUST_IS_PENDING_DELETE:
			return ((Xv_opaque)
					BOOL_FLAG_VALUE(priv->state, TXTSW_ADJUST_IS_PD));
		case TEXTSW_AGAIN_LIMIT:
			return ((Xv_opaque)
					((priv->again_count) ? (priv->again_count - 1) : 0));
		case TEXTSW_AGAIN_RECORDING:
			return ((Xv_opaque)
					! BOOL_FLAG_VALUE(priv->state, TXTSW_NO_AGAIN_RECORDING));
		case TEXTSW_AUTO_INDENT:
			return ((Xv_opaque)
					BOOL_FLAG_VALUE(priv->state, TXTSW_AUTO_INDENT));
		case TEXTSW_AUTO_SCROLL_BY:
			return ((Xv_opaque) ev_get((Ev_handle) (view->e_view),
							EV_CHAIN_AUTO_SCROLL_BY, XV_NULL, XV_NULL,
							XV_NULL));
		case TEXTSW_BLINK_CARET:
			return ((Xv_opaque)
					BOOL_FLAG_VALUE(priv->caret_state, TXTSW_CARET_FLASHING));
		case TEXTSW_BROWSING:
			return ((Xv_opaque)
					BOOL_FLAG_VALUE(priv->state, TXTSW_READ_ONLY_SW));
		case TEXTSW_CLIENT_DATA:
			return (priv->client_data);
		case TEXTSW_COALESCE_WITH:
			return ((Xv_opaque) priv->coalesce_with);
		case TEXTSW_CONFIRM_OVERWRITE:
			return ((Xv_opaque)
					BOOL_FLAG_VALUE(priv->state, TXTSW_CONFIRM_OVERWRITE));
		case TEXTSW_CONTENTS:{

#ifdef OW_I18N
				textsw_implicit_commit(priv);
			}
		case TEXTSW_CONTENTS_NO_COMMIT:{
				/* This is used by only mailtool for autosave. */
#endif

				/* pos, buf and buf_len are */
				Es_index pos = va_arg(args, Es_index);

				/* temporaries for TEXTSW_CONTENTS */
				char *buf = va_arg(args, char *);
				int buf_len = va_arg(args, int);

				return ((Xv_opaque)
						textsw_get_contents(priv, pos, buf, buf_len));
			}

#ifdef OW_I18N
		case TEXTSW_CONTENTS_WCS:
			textsw_implicit_commit(priv);
		case TEXTSW_CONTENTS_WCS_NO_COMMIT:{
				/* This is used by only Termsw. */
				/* OW_I18N: pos is character based */
				Es_index pos = va_arg(args, Es_index);

				/* temporaries for TEXTSW_CONTENTS */
				CHAR *buf = va_arg(args, CHAR *);

				/* OW_I18N: buf_len is character based */
				int buf_len = va_arg(args, int);

				return ((Xv_opaque)
						textsw_get_contents_wcs(priv, pos, buf, buf_len));
			}
#endif

		case TEXTSW_CONTROL_CHARS_USE_FONT:
			return ((Xv_opaque)
					ei_get(priv->views->eih, EI_CONTROL_CHARS_USE_FONT));
		case TEXTSW_DESTROY_ALL_VIEWS:
			return ((Xv_opaque)
					BOOL_FLAG_VALUE(priv->state, TXTSW_DESTROY_ALL_VIEWS));
		case TEXTSW_DISABLE_CD:
			return ((Xv_opaque)
					BOOL_FLAG_VALUE(priv->state, TXTSW_NO_CD));
		case TEXTSW_DISABLE_LOAD:
			return ((Xv_opaque)
					BOOL_FLAG_VALUE(priv->state, TXTSW_NO_LOAD));
		case TEXTSW_DIFFERENTIATE_CR_LF:	/*1030878 */
			return ((Xv_opaque)
					BOOL_FLAG_VALUE(priv->state, TXTSW_DIFF_CR_LF));
		case TEXTSW_IS_TERMSW: return (Xv_opaque)FALSE;
		case TEXTSW_EDIT_BACK_CHAR: return ((Xv_opaque) priv->edit_bk_char);
		case TEXTSW_EDIT_BACK_WORD: return ((Xv_opaque) priv->edit_bk_word);
		case TEXTSW_EDIT_BACK_LINE: return ((Xv_opaque) priv->edit_bk_line);
		case TEXTSW_EDIT_COUNT:
			return ((Xv_opaque) ev_get((Ev_handle) (view->e_view),
							EV_CHAIN_EDIT_NUMBER, XV_NULL, XV_NULL, XV_NULL));
		case TEXTSW_WRAPAROUND_SIZE:
			return ((Xv_opaque) es_get(priv->views->esh,
							ES_PS_SCRATCH_MAX_LEN));
		case TEXTSW_ES_CREATE_PROC: return ((Xv_opaque) priv->es_create);

#ifdef OW_I18N
		case TEXTSW_FILE_WCS:{
				/* } for match */
#else
		case TEXTSW_FILE:{
#endif

				CHAR *name;

				if (textsw_file_name(priv, &name))
					return ((Xv_opaque) 0);
				else
					return ((Xv_opaque) name);
			}

#ifdef OW_I18N
		case TEXTSW_FILE:{
				CHAR *name;
				char name_mb[MAXPATHLEN];

				if (textsw_file_name(priv, &name))
					return ((Xv_opaque) 0);
				else {
					(void)wcstombs(name_mb, name, MAXPATHLEN);
					return ((Xv_opaque) name_mb);
				}
			}
#endif

		case TEXTSW_SUBMENU_FILE:{
				if (!textsw_menu_get(priv) || !priv->sub_menu_table)
					return XV_NULL;
				else
					return priv->sub_menu_table[TXTSW_FILE_SUB_MENU];
			}
		case TEXTSW_SUBMENU_EDIT:{
				if (!textsw_menu_get(priv) || !priv->sub_menu_table)
					return XV_NULL;
				else
					return priv->sub_menu_table[TXTSW_EDIT_SUB_MENU];
			}
		case TEXTSW_SUBMENU_VIEW:{
				if (!textsw_menu_get(priv) || !priv->sub_menu_table)
					return XV_NULL;
				else
					return priv->sub_menu_table[TXTSW_VIEW_SUB_MENU];
			}
		case TEXTSW_SUBMENU_FIND:{
				if (!textsw_menu_get(priv) || !priv->sub_menu_table)
					return XV_NULL;
				else
					return priv->sub_menu_table[TXTSW_FIND_SUB_MENU];
			}
		case TEXTSW_EXTRAS_CMD_MENU:{
				if (!textsw_menu_get(priv) || !priv->sub_menu_table)
					return XV_NULL;
				else
					return priv->sub_menu_table[TXTSW_EXTRAS_SUB_MENU];
			}
		case TEXTSW_FIRST:

#ifdef OW_I18N
			return ((Xv_opaque) textsw_mbpos_from_wcpos(priv,
							ft_position_for_index(view->e_view->line_table,
									0)));
		case TEXTSW_FIRST_WC:
#endif

			return ((Xv_opaque)
					ft_position_for_index(view->e_view->line_table, 0));
		case TEXTSW_FIRST_LINE:{
				int top, bottom;

				ev_line_info(view->e_view, &top, &bottom);
				return ((Xv_opaque) top - 1);
			}
		case TEXTSW_HISTORY_LIMIT:
			return ((Xv_opaque)
					((priv->undo_count) ? (priv->undo_count - 1) : 0));
		case TEXTSW_IGNORE_LIMIT:
			return ((Xv_opaque) priv->ignore_limit);
		case TEXTSW_INSERT_MAKES_VISIBLE:
			return ((Xv_opaque) priv->insert_makes_visible);
		case TEXTSW_MODIFIED:
			return ((Xv_opaque)
					textsw_has_been_modified(TEXTSW_PUBLIC(priv)));
		case TEXTSW_SMART_WORD_HANDLING:
			return (Xv_opaque)priv->smart_word_handling;
		case TEXTSW_MEMORY_MAXIMUM:
			return ((Xv_opaque) priv->es_mem_maximum);
		case TEXTSW_MULTI_CLICK_SPACE:
			return ((Xv_opaque) priv->multi_click_space);
		case TEXTSW_MULTI_CLICK_TIMEOUT:
			return ((Xv_opaque) priv->multi_click_timeout);
		case TEXTSW_NO_RESET_TO_SCRATCH:
			return ((Xv_opaque)
					BOOL_FLAG_VALUE(priv->state, TXTSW_NO_RESET_TO_SCRATCH));
		case TEXTSW_NOTIFY_LEVEL:
			return ((Xv_opaque) priv->notify_level);
		case TEXTSW_NOTIFY_PROC:
			return ((Xv_opaque) priv->notify);
		case TEXTSW_READ_ONLY:
			return ((Xv_opaque)
					BOOL_FLAG_VALUE(priv->state, TXTSW_READ_ONLY_ESH));
		case TEXTSW_TAB_WIDTH:
			return ((Xv_opaque) ei_get(priv->views->eih, EI_TAB_WIDTH));
		case TEXTSW_TEMP_FILENAME:
			return ((Xv_opaque) priv->temp_filename);
		case TEXTSW_UPPER_CONTEXT:
			return ((Xv_opaque) ev_get((Ev_handle) (view->e_view),
							EV_CHAIN_UPPER_CONTEXT, XV_NULL, XV_NULL, XV_NULL));
		case TEXTSW_LINE_BREAK_ACTION:
			return ((Xv_opaque) ev_get((Ev_handle) (view->e_view),
							EV_RIGHT_BREAK, XV_NULL, XV_NULL, XV_NULL));

			/* Super-class attributes that we override. */
		case WIN_MENU:
			return textsw_get_unique_menu(priv);
		case XV_LEFT_MARGIN:
			return ((Xv_opaque) ev_get(view->e_view, EV_LEFT_MARGIN, XV_NULL,
							XV_NULL, XV_NULL));
		case XV_RIGHT_MARGIN:
			return ((Xv_opaque) ev_get(view->e_view, EV_RIGHT_MARGIN, XV_NULL,
							XV_NULL, XV_NULL));
		case WIN_VERTICAL_SCROLLBAR:
			{
				Textsw tsw = TEXTSW_PUBLIC(priv);
				Textsw_view vp = VIEW_PUBLIC(view);

				return xv_get(tsw, OPENWIN_VERTICAL_SCROLLBAR, vp);
			}
		case WIN_TYPE:	/* SunView1.X compatibility */
			return ((Xv_opaque) TEXTSW_TYPE);
		default:
			if (xv_check_bad_attr(&xv_textsw_pkg, (Attr_attribute)attribute) == XV_ERROR) {
				*status = XV_ERROR;
			}
			return ((Xv_opaque) 0);
	}
}

static Xv_pkg *textsw_paintwindow_class(void);

/* Caller turns varargs into va_list that has already been va_start'd */
Pkg_private Xv_opaque textsw_get(Textsw abstract, int *status, Attr_attribute attr, va_list args)
{
	static int recurs = FALSE;
	Xv_opaque ret;
    Textsw_private priv = TEXTSW_PRIVATE(abstract);
	Textsw_view vp;

	switch (attr) {
		case OPENWIN_NVIEWS:
		case OPENWIN_NTH_VIEW:
		case OPENWIN_VERTICAL_SCROLLBAR:
		case WIN_IS_CLIENT_PANE:
		case WIN_FRAME:
		case XV_SCREEN:
		case XV_FONT:
			*status = XV_ERROR;
			return XV_NULL;

		/* now come attributes thet must be answered by us although
		 * they do belong to other packages - so, avoid the 'default' case!
		 */
		case OPENWIN_VIEW_CLASS: return (Xv_opaque)TEXTSW_VIEW;
		case WIN_MENU:
		case XV_LEFT_MARGIN:
		case XV_RIGHT_MARGIN:
		case WIN_VERTICAL_SCROLLBAR:
			break;
		case OPENWIN_PW_CLASS: 
			{
				static int try_TEXTSW_PAINTWINDOW = -1;

				if (try_TEXTSW_PAINTWINDOW < 0) {
					char *e = getenv("DRA_XVIEW_USE_TEXTSW_PAINTWINDOW");

					if (e && *e) {
						try_TEXTSW_PAINTWINDOW = TRUE;
					}
					else {
						try_TEXTSW_PAINTWINDOW = FALSE;
					}
				}

				if (try_TEXTSW_PAINTWINDOW) {
					return (Xv_opaque)textsw_paintwindow_class();
				}
				else {
					return XV_NULL;
				}
			}

		default:
			/* the attributes above were already recognized to be 
			 * 'recursive-dangerous'. However, we assume that every
			 * non-TEXTSW-attribute should also be ruled out
			 */
			{
				if (ATTR_PKG(attr) != ATTR_PKG_TEXTSW) {
					/* rather do that silently - many many calls
					fprintf(stderr,
						"txt_attr.c: attribute %s filtered out in textsw_get\n",
						attr_name(attr));
					*/

					*status = XV_ERROR;
					return XV_NULL;
				}
			}
			break;
	}

	if (recurs) {
		fprintf(stderr, "txt_attr.c: dying in recursive textsw_get with '%s'\n",
								attr_name(attr));
		abort();
	}

	vp = xv_get(abstract, OPENWIN_NTH_VIEW, 0);
	recurs = TRUE;
	if (vp) {
    	ret = (Xv_opaque)textsw_get_internal(priv, VIEW_PRIVATE(vp),
				    status, (Textsw_attribute)attr, args);
	}
	else {
    	ret = (Xv_opaque)textsw_get_internal(priv, NULL,
				    status, (Textsw_attribute)attr, args);
	}
	recurs = FALSE;
	return ret;
}

/* Caller turns varargs into va_list that has already been va_start'd */
Pkg_private Xv_opaque textsw_view_get(Textsw_view view_public, int *status,
								Attr_attribute attr, va_list args)
{
	Textsw_view_private view = VIEW_PRIVATE(view_public);
    
    return (Xv_opaque) textsw_get_internal(TSWPRIV_FOR_VIEWPRIV(view), view,
					    status, (Textsw_attribute)attr, args);
}

/* VARARGS1 */
Pkg_private Xv_opaque textsw_set(Textsw abstract, Attr_attribute *avlist)
{
    Textsw_private    priv = TEXTSW_PRIVATE(abstract);
	Textsw_view vp = xv_get(abstract, OPENWIN_NTH_VIEW, 0);

	if (vp) {
    	return (Xv_opaque)textsw_set_internal(priv, VIEW_PRIVATE(vp),
					avlist, TRUE);
	}
	else {
    	return (Xv_opaque)textsw_set_internal(priv, NULL, avlist, TRUE);
	}
}

/* VARARGS1 */
Pkg_private Xv_opaque textsw_view_set(Textsw_view view_public, Attr_attribute *avlist)
{
    /*
     * Performance enhancment:  Don't use VIEW_ABS_TO_REP because it calls
     * xv_get(), and we know view_public is a public view handle.
     */

    Textsw_view_private view = VIEW_PRIVATE(view_public);

	/* es gab Faelle, wo zwar bereits textsw_folio_cleanup aufegrufen 
	 * worden war, aber erst danach (auf Ebene OPENWIN) die Views vernichtet
	 * wurden - dabei wurde aber nochmal (indirekt ueber die Vernichtung 
	 * von drop_site) xv_set fuer den View aufgerufen - dann kamen wir
	 * hierher...
	 */
	if (! view->textsw_priv) return XV_OK;

    return (Xv_opaque)textsw_set_internal(TSWPRIV_FOR_VIEWPRIV(view), view, avlist, FALSE);
}

Pkg_private void textsw_notify(Textsw_view_private view, ...)
{
	Textsw_private priv;
	int doing_event;

	if (! view->textsw_priv) return;

	AVLIST_DECL;
	va_list args;

	VA_START(args, view);
	MAKE_AVLIST(args, avlist);
	va_end(args);
	priv = TSWPRIV_FOR_VIEWPRIV(view);
	doing_event = (priv->state & TXTSW_DOING_EVENT);
	priv->state &= ~TXTSW_DOING_EVENT;
	priv->notify(TEXTSW_PUBLIC(priv), avlist);
	if (doing_event)
		priv->state |= TXTSW_DOING_EVENT;
}

Pkg_private void textsw_notify_replaced(Textsw_view_private view, Es_index insert_before, Es_index length_before, Es_index replaced_from, Es_index replaced_to, Es_index count_inserted)
{
    Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
    int             in_notify_proc =
    priv->state & TXTSW_IN_NOTIFY_PROC;

    priv->state |= TXTSW_IN_NOTIFY_PROC;
    textsw_notify(view, TEXTSW_ACTION_REPLACED,
		  insert_before, length_before,
		  replaced_from, replaced_to, count_inserted, NULL);
    if (!in_notify_proc)
	priv->state &= ~TXTSW_IN_NOTIFY_PROC;
}

Pkg_private     Es_index
#ifdef OW_I18N
textsw_get_contents_wcs
#else
textsw_get_contents
#endif
	( Textsw_private textsw, Es_index position,
    CHAR *buffer, int buffer_length)
{
    Es_index        next_read_at;
    int darllenwyd;

    es_set_position(textsw->views->esh, position);
    next_read_at = es_read(textsw->views->esh, buffer_length, buffer,
			   &darllenwyd);
    if AN_ERROR
	(darllenwyd != buffer_length) {
#ifdef OW_I18N
	XV_BZERO(buffer + darllenwyd, ((buffer_length - darllenwyd) * sizeof(CHAR)));
#else
	XV_BZERO(buffer + darllenwyd, (size_t)(buffer_length - darllenwyd));
#endif
	}
    return (next_read_at);
}

#ifdef OW_I18N

Pkg_private Es_index textsw_get_contents(Textsw_private priv, Es_index position, char *buffer,
    int buffer_length)
{
	Es_index next_read_at, wc_position;
	int read;
	CHAR *buf_wcs = MALLOC(buffer_length + 1);

	wc_position = textsw_wcpos_from_mbpos(priv, position);

	/* Make sure whether position is over the end of contents or not */
	textsw_flush_caches(VIEW_PRIVATE(xv_get(XV_PUBLIC(priv), OPENWIN_NTH_VIEW, 0)), TFC_STD);
					,
	if (wc_position == TEXTSW_INFINITY ||
			wc_position >= es_get_length(priv->views->esh)) {
		next_read_at = (Es_index) textsw_get_mb_length(priv);
		read = 0;
	}
	else {
		es_set_position(priv->views->esh, wc_position);
		(void)es_read(priv->views->esh, buffer_length, buf_wcs, &read);
		buf_wcs[read] = NULL;
		read = wcstombs(buffer, buf_wcs, buffer_length);
		next_read_at = (position < 0 ? 0 : position) + read;
	}

	free((char *)buf_wcs);
	if AN_ERROR
		(read != buffer_length) {
		XV_BZERO(buffer + read, buffer_length - read);
		}
	return (next_read_at);
}
#endif /* OW_I18N */

/* originally, TEXTSW (a subclass of OPENWIN) had views and performed
 * all event handling and painting on the views.
 * Especially: there was no 'paint window'.
 *
 * Now, I want also TEXTSWs to be selectable and resizable etc,
 * things that are mostly implemented on OPENWIN - but you need a paint window.
 *
 * The idea is to make the following modifications :
 * + return &xv_textsw_pw_pkg as the answer to OPENWIN_PW_CLASS
 * + do NOT select any events on the views (leave that to OPENWIN)
 * + instead select the same event on the paint window
 * + in the event handler, modify event_window to the view and call the 
 *   original (view) event handler - however, we must prevent the orig handler
 *   from calling notify_next_event_func, because that leads to a
 *   notifier error.
 * + handle drag&drop by using the 'correct' owner of the DRAGDROP object
 * + output (XDrawString etc) is done in ttysw/tty_newtxt.d: use
 *   xv_get(OPENWIN_VIEW_PAINT_WINDOW) to switch to the correct 'output window'
 *
 * It will not be so simple: the whole focus handling and caret-business
 * seems to be a problem - maybe I should have a look at CANVAS.
 * Also, scrolling does not work right - there seems to be a little XCopyArea
 * somewhere - YES, I found it in ttysw/tty_newtxt.c, and now scrolling
 * behaves much better.
 * Copy works, Cut and Paste do not work, Quick Duplicate not, Drag works
 * Drop does not work
 *
 * 14.12.2024: there is work to do    INCOMPLETE
 */

typedef struct {
    Xv_window_struct	parent_data;
    Xv_opaque		private_data;
} Xv_textsw_pw;

typedef struct {
	Xv_opaque public_self;
	Textsw_private priv;
	Textsw_view_privstruct *vp;
} Textsw_pw_private;

typedef Xv_opaque Textsw_pw;

#define PWPRIV(t) XV_PRIVATE(Textsw_pw_private, Xv_textsw_pw, t)

static int textsw_pw_init(Textsw_view view, Textsw_pw xself, Attr_avlist avlist,
				int *unused)
{
	Xv_textsw_pw *self = (Xv_textsw_pw *)xself;
	Textsw_pw_private *pwp = (Textsw_pw_private *)xv_alloc(Textsw_pw_private);
	Textsw_view_privstruct *vp = VIEW_PRIVATE(view);

	if (!pwp) return XV_ERROR;

fprintf(stderr, "%s: pw=%ld, view=%ld\n", __FUNCTION__, xself, view);

	self->private_data = (Xv_opaque)pwp;
	pwp->public_self = xself;

	pwp->priv = vp->textsw_priv;
	pwp->vp = vp;

	xv_set(xself,
			XV_WIDTH, xv_get(view, XV_WIDTH),
			XV_HEIGHT, xv_get(view, XV_HEIGHT),
			NULL);

	vp->drop_site = xv_create(xself, DROP_SITE_ITEM,
				DROP_SITE_REGION, &vp->rect,
				NULL);

	return XV_OK;
}

static int textsw_pw_destroy(Textsw_pw self, Destroy_status status)
{
	if (status == DESTROY_CLEANUP) {
		Textsw_pw_private *pwp = PWPRIV(self);

		memset((char *)pwp, 0, sizeof(*pwp));
		xv_free((char *)pwp);
	}
	return XV_OK;
}

static Notify_value textsw_view_interposer(Xv_opaque view, Notify_event event,
								Notify_arg arg, Notify_event_type type)
{
	Event *ev = (Event *)event;
	Notify_value result = notify_next_event_func(view, event, arg, type);

	switch (event_action(ev)) {
		case WIN_RESIZE:
fprintf(stderr, "%s: view resize, view=%ld\n", __FUNCTION__, view);
			xv_set(xv_get(view, OPENWIN_VIEW_PAINT_WINDOW),
					XV_X, 0,
					XV_Y, 0,
					XV_WIDTH, xv_get(view, XV_WIDTH),
					XV_HEIGHT, xv_get(view, XV_HEIGHT),
					NULL);
			break;
		default:
			break;
	}
	return result;
}

static Notify_value textsw_pw_events(Xv_opaque pw, Notify_event event,
								Notify_arg arg, Notify_event_type type)
{
	Event *ev = (Event *)event;
	Event fakewin_ev;
	Textsw_view v;
	Notify_value result;

	v = xv_get(pw, XV_OWNER);
	fakewin_ev = *ev;
	event_set_window(&fakewin_ev, v);

fprintf(stderr, "%s: pw=%ld, view=%ld, act=%d\n", __FUNCTION__, pw, v, event_action(ev));
	/* what do we NOT hand over to the view ? */
	switch (event_action(ev)) {
		case LOC_WINENTER:
		case LOC_WINEXIT:
			return notify_next_event_func(pw, event, arg, type);
	}

	result = textsw_view_event_internal(v, (Notify_event)&fakewin_ev,arg,type);
	if (result == NOTIFY_UNEXPECTED) {
		return notify_next_event_func(pw, event, arg, type);
	}

	return result;
}

static Xv_opaque textsw_pw_set(Textsw_pw self, Attr_avlist avlist)
{
	register Attr_attribute *attrs;
	Textsw_pw_private *pwp = PWPRIV(self);
/* 	Rect rect; */
	Textsw_private priv = pwp->priv;

	for (attrs=avlist; *attrs; attrs=attr_next(attrs)) switch ((int)*attrs) {
		case XV_END_CREATE:
			{
				Textsw_view view = xv_get(self, XV_OWNER);
				Textsw_pw other_pw;

fprintf(stderr, "%s: end_create: pw=%ld, view=%ld\n", __FUNCTION__,self, view);
				xv_set(view,
						WIN_NOTIFY_IMMEDIATE_EVENT_PROC, textsw_view_interposer,
						WIN_NOTIFY_SAFE_EVENT_PROC, textsw_view_interposer,
						NULL);

				xv_set(self,
						WIN_NOTIFY_IMMEDIATE_EVENT_PROC, textsw_pw_events,
						WIN_NOTIFY_SAFE_EVENT_PROC, textsw_pw_events,
						WIN_CONSUME_EVENTS,
							WIN_NO_EVENTS,
							WIN_REPAINT,
							WIN_GRAPHICS_EXPOSE,
							WIN_RESIZE,
							WIN_MOUSE_BUTTONS,
							WIN_ASCII_EVENTS, /* help !! */
							LOC_DRAG,
							LOC_WINENTER,
							LOC_WINEXIT,
							KBD_USE,
							KBD_DONE,
							NULL,
						NULL);

				OPENWIN_EACH_PW(TEXTSW_PUBLIC(priv), other_pw)
					if (other_pw != self) {
						xv_set(self,
							WIN_COLOR_INFO, xv_get(other_pw, WIN_COLOR_INFO),
							WIN_CURSOR, xv_get(other_pw, WIN_CURSOR),
							NULL);

						break;
					}
				OPENWIN_END_EACH
			}
#ifdef NOT_YET
			rect.r_left = rect.r_top = 0;
			rect.r_width = pwp->vi.width = (int)xv_get(self, XV_WIDTH);
			rect.r_height = pwp->vi.height = (int)xv_get(self, XV_HEIGHT);

			if (priv->created) {    /* is a split */
			}
			xv_set(self,
						WIN_NOTIFY_IMMEDIATE_EVENT_PROC, scrollpw_events,
						WIN_NOTIFY_SAFE_EVENT_PROC, scrollpw_events,
						WIN_CONSUME_EVENTS,
							WIN_NO_EVENTS,
							WIN_REPAINT,
							WIN_GRAPHICS_EXPOSE,
							WIN_RESIZE,
							WIN_MOUSE_BUTTONS,
							WIN_ASCII_EVENTS, /* help !! */
							LOC_DRAG,
							LOC_WINENTER,
							LOC_WINEXIT,
							KBD_USE,
							KBD_DONE,
							NULL,
						NULL);

			if (priv->drop_kind == (Scrollwin_drop_setting)(-1)) {
				priv->drop_kind = xv_get(SCRPUB(pwp->priv),SCROLLWIN_DROPPABLE);
			}

			/* the drop site implementation is rather funny !! */
			switch (priv->drop_kind) {
				case SCROLLWIN_DROP:
					pwp->dropsite = xv_create(self, DROP_SITE_ITEM,
								DROP_SITE_REGION, &rect,
								NULL);
					break;
				case SCROLLWIN_DEFAULT:
					pwp->dropsite = xv_create(self, DROP_SITE_ITEM,
								DROP_SITE_REGION, &rect,
								DROP_SITE_DEFAULT, TRUE,
								NULL);
					break;
				case SCROLLWIN_DROP_WITH_PREVIEW:
					pwp->dropsite = xv_create(self, DROP_SITE_ITEM,
								DROP_SITE_REGION, &rect,
								DROP_SITE_EVENT_MASK, DND_ENTERLEAVE|DND_MOTION,
								NULL);
					break;
				case SCROLLWIN_DEFAULT_WITH_PREVIEW:
					pwp->dropsite = xv_create(self, DROP_SITE_ITEM,
								DROP_SITE_REGION, &rect,
								DROP_SITE_DEFAULT, TRUE,
								DROP_SITE_EVENT_MASK, DND_ENTERLEAVE|DND_MOTION,
								NULL);
					break;
				default: break;
			}
#endif /* NOT_YET */
			break;

		default:
			xv_check_bad_attr(textsw_paintwindow_class(), *attrs);
			break;
	}

	return XV_OK;
}

#ifdef NOT_NEEDED
static Xv_opaque textsw_pw_get(Xv_opaque xself, int *status, Attr_attribute attr, va_list vali)
{
/* 	Xv_textsw_pw *self = (Xv_textsw_pw *)xself; */
/* 	Textsw_pw_private *pwp = (Textsw_pw_private *)self->private_data; */

	*status = XV_OK;
	switch ((int)attr) {
		default:
			*status = xv_check_bad_attr(textsw_paintwindow_class(), attr);
	}
	return (Xv_opaque)XV_OK;
}
#endif /* NOT_NEEDED */

static Xv_pkg xv_textsw_pw_pkg = {
    "Textsw_paint_window",
    (Attr_pkg) ATTR_PKG_TEXTSW_VIEW,
    sizeof(Xv_textsw_pw),
    &xv_window_pkg,
    textsw_pw_init,
    textsw_pw_set,
    NULL,
    textsw_pw_destroy,
    NULL			/* no find proc */
};

static Xv_pkg *textsw_paintwindow_class(void)
{
	return &xv_textsw_pw_pkg;
}
