/*	@(#)txt_impl.h 20.73 93/06/28 SMI  DRA: $Id: txt_impl.h,v 4.63 2025/07/24 16:59:35 dra Exp $	*/

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

#define TEXTSW_DO_INSERT_MAKES_VISIBLE(_view)\
{\
     if ((priv->insert_makes_visible == TEXTSW_ALWAYS) &&\
         (priv->state & TXTSW_DOING_EVENT) &&\
         (!EV_INSERT_VISIBLE_IN_VIEW(_view->e_view)))\
        textsw_normalize_internal(_view, EV_GET_INSERT(priv->views),\
	   ES_INFINITY, 0,\
           (int)ev_get((Ev_handle)_view->e_view,\
                        EV_CHAIN_LOWER_CONTEXT, XV_NULL, XV_NULL, XV_NULL),\
           TXTSW_NI_AT_BOTTOM|TXTSW_NI_NOT_IF_IN_VIEW|TXTSW_NI_MARK);\
}

#define TEXTSW_OUT_OF_MEMORY(_folio, _status)\
	((_status == ES_SHORT_WRITE) && (ES_TYPE_MEMORY == (Es_enum)((long)es_get((Es_handle)es_get(priv->views->esh, ES_PS_SCRATCH), ES_TYPE))))

#ifndef textsw_impl_DEFINED
#define textsw_impl_DEFINED
/*
 * Internal structures for textsw implementation.
 */

#				ifndef timerclear
#include <sys/time.h>
#				endif
#include <xview/pkg.h>
#				ifndef NOTIFY_DEFINED
#include <xview/notify.h>
#				endif
#				ifndef scrollbar_DEFINED
#include <xview/scrollbar.h>
#				endif
#				ifndef suntool_entity_view_DEFINED
#include <xview_private/ev.h>
#				endif
#include <sys/stat.h>

#define _OTHER_TEXTSW_FUNCTIONS 1
#include <xview/textsw.h>

#include <xview/attrol.h>
#include <xview/frame.h>
#include <xview/openmenu.h>
#include <xview/panel.h>
#include <xview/dragdrop.h>

#ifdef OW_I18N
#include <xview_private/i18n_impl.h> 
#include <xview_private/convpos.h>
#endif /* OW_I18N */


/* Although not needed to support types used in this header file, the
 * following are needed to support macros defined here.
 */
#				ifndef sunwindow_win_input_DEFINED
#include <xview/win_input.h>
#				endif

#ifndef	NULL
#define	NULL	0
#endif

#define TEXTSW_PRIVATE(t) XV_PRIVATE(Textsw_privstruct, Xv_textsw, t)
#define TEXTSW_PUBLIC(textsw_folio_object) XV_PUBLIC(textsw_folio_object)

#define VIEW_PRIVATE(t) XV_PRIVATE(Textsw_view_privstruct, Xv_textsw_view, t)
#define VIEW_PUBLIC(view_object) XV_PUBLIC(view_object)
		   

#define TXTSW_DO_AGAIN(_textsw)	\
	((_textsw->again_count != 0) && \
	 ((_textsw->state & TXTSW_NO_AGAIN_RECORDING) == 0))
#ifdef OW_I18N		/* Tune off undo for preedit region */
#define TXTSW_DO_UNDO(_textsw) \
        ((_textsw->undo_count != 0) && \
                 ((_textsw->state & TXTSW_NO_UNDO_RECORDING) == 0))
#else
#define TXTSW_DO_UNDO(_textsw)	(_textsw->undo_count != 0)
#endif /* OW_I18N */

#ifdef OW_I18N
#ifdef FULL_R5
#define	textsw_implicit_commit(_folio) \
    if ((_folio->xim_style & XIMPreeditCallbacks) \
		? (_folio->preedit_start ? 1 : 0) \
		: (_folio->ic && \
		   xv_get(TEXTSW_PUBLIC(_folio), WIN_IC_CONVERSION) == TRUE) \
		   ? 1 : 0) \
	textsw_implicit_commit_doit(_folio);
#else /* FULL_R5 */
#define	textsw_implicit_commit(_folio) \
    if (_folio->preedit_start) \
	textsw_implicit_commit_doit(_folio);
#endif /* FULL_R5 */
#endif /* OW_I18N */


typedef struct textsw_string {
	int	max_length;
	CHAR	*base;
	CHAR	*free;
} string_t;
#define TXTSW_STRING_IS_NULL(ptr_to_string_t)				\
	((ptr_to_string_t)->base == null_string.base)
#define TXTSW_STRING_BASE(ptr_to_string_t)				\
	((ptr_to_string_t)->base)
#define TXTSW_STRING_FREE(ptr_to_string_t)				\
	((ptr_to_string_t)->free)
#define TXTSW_STRING_LENGTH(ptr_to_string_t)				\
	(TXTSW_STRING_FREE(ptr_to_string_t)-TXTSW_STRING_BASE(ptr_to_string_t))

typedef struct key_map_object {
	struct key_map_object	*next;
	short			 event_code;
	short unsigned		 type;
	short			 shifts;
	caddr_t			 maps_to;
} *Key_map_handle;
#define	TXTSW_KEY_FILTER	0
	/* maps_to is argv => char[][] */
#define	TXTSW_KEY_SMART_FILTER	1
	/* maps_to is argv => char[][] */
#define	TXTSW_KEY_MACRO		2
	/* maps_to is again script => string_t */
#define	TXTSW_KEY_NULL		32767

typedef struct textsw_selection_object {
	long unsigned		  type;
	Es_index		  first, last_plus_one;
	CHAR			 *buf;
	int			  buf_len;
} Textsw_selection_object;
/*	type includes modes, etc (contains return from textsw_func_selection,
 *	  see below)
 *	first == last_plus_one == ES_INFINITY when object is initialized.
 */
typedef Textsw_selection_object	*Textsw_selection_handle;

typedef struct textsw_view_object {
	Textsw_view			  public_self;
	long unsigned		  magic;
	struct textsw_object *textsw_priv;
	Rect				  rect;
	Ev_handle			  e_view;
	long unsigned		  view_state;
	Xv_drop_site		  drop_site;
	int					  obscured;
	Selection_requestor   selreq;
} Textsw_view_privstruct;

typedef Textsw_view_privstruct *Textsw_view_private;

Pkg_private Xv_opaque textsw_view_get(Textsw_view view_public, int *status, Attr_attribute attribute, va_list args);
Pkg_private int textsw_view_destroy(Textsw_view view_public, Destroy_status status);

Pkg_private int textsw_destroy(Textsw tsw, Destroy_status status);

#define	TEXTSW_VIEW_NULL	((Textsw_view_private)0)
#define	TEXTSW_VIEW_MAGIC	0xF0110A0A
#define	TEXTSW_FOLIO_MAGIC	0xF011A0A0

#define PIXWIN_FOR_VIEW(_view)		((_view)->e_view->pw)

	/* Bit flags for Textsw_view->state */
/* For textsw_do_input() */
#define TXTSW_DONT_UPDATE_SCROLLBAR		0x00000000
#define TXTSW_UPDATE_SCROLLBAR			0x00000001
#define TXTSW_UPDATE_SCROLLBAR_IF_NEEDED	0x00000002
#define TXTSW_SCROLLBAR_DISABLED		0x00000004

#define TXTSW_VIEW_IS_MAPPED    0x00000008
#define TXTSW_DONT_REDISPLAY	0x10000000
#define TXTSW_VIEW_DISPLAYED	0x40000000	
#define TXTSW_VIEW_DYING	0x80000000

typedef void (*split_init_proc_t)(Xv_opaque, Xv_opaque, int);

#define	TEXTSW_MAGIC		0xF2205050
#ifdef OW_I18N
#define TXTSW_UI_BUFLEN	256
#else
#define TXTSW_UI_BUFLEN	12
#endif

#define TSW_SEL_PRIMARY	0
#define TSW_SEL_SECONDARY	1
#define TSW_SEL_CLIPBOARD	2
#define TSW_SEL_CARET	3
#define NBR_TSW_SELECTIONS	4

typedef struct textsw_object {
	long unsigned	  magic;
	struct textsw_object *next;
	Textsw		  public_self;
	Xv_opaque	  menu_accessible_only_via_textsw_menu;
	Ev_chain	  views;
	Es_handle	(*es_create)(Xv_opaque, Es_handle, Es_handle);
	textsw_notify_proc_t notify;
	unsigned	  notify_level;
	CHAR		  to_insert[TXTSW_UI_BUFLEN];
	CHAR		 *to_insert_next_free;
	unsigned	  to_insert_counter;
	Es_handle	  trash;	/* Stuff deleted from piece_source */
	long unsigned	  state,
			  func_state;
	short unsigned	  caret_state,
			  track_state;
	int		  multi_click_space,
			  multi_click_timeout;
	Textsw_enum	  insert_makes_visible;
	unsigned	  span_level;
	struct timeval	  last_ie_time,
			  last_adjust,
			  last_point,
			  selection_died,
			  timer;
	short		  last_click_x,
			  last_click_y;
	short             drag_threshold;
	Es_index	  adjust_pivot;	/* Valid iff TXTSW_TRACK_ADJUST */
	Ev_mark_object	  read_only_boundary;
	Ev_mark_object	  save_insert;	/* Valid iff TXTSW_GET/PUTTING	*/
	unsigned	  again_count;
	unsigned	  undo_count;
	string_t	 *again;
	Es_index	  again_first, again_last_plus_one;
	int		  again_insert_length; /* Offset from again[0]->base */
	caddr_t		 *undo;
	Key_map_handle	  key_maps;
	int		  owed_by_filter;
	unsigned	  es_mem_maximum;
	unsigned	  ignore_limit;
	char		  edit_bk_char, edit_bk_word, edit_bk_line;
	Xv_opaque	  client_data;
	Menu_item 	  *menu_table; 
	Menu		  *sub_menu_table;   /*  Menu handle to all submenus */
	int		  checkpoint_frequency;
	int		  checkpoint_number;
	CHAR		 *checkpoint_name;
	Textsw_view_private  coalesce_with;
	unsigned	  event_code;
	char		 *temp_filename; /* For cmdsw log */
    window_layout_proc_t layout_proc; /* interposed window layout proc */
	Xv_Window	  focus_view;	/* view window with the kbd focus */
	unsigned	  accel_menus:1; /* Are core menu items accelerated */
#ifdef OW_I18N
	int		  blocking_newline; /* This is used for bug# 1090046 */
	int		  need_im; /* TRUE if XV_IM and XV_USE_IM are TRUE */
	XIC		  ic;
	Ev_mark_object	  preedit_start;
	/*
	 * This variable is used for OnTheSpot input style.
	 * preedit_start saves an insertion point for preedit text.
	 * If preedit_start is non NULL, IC exists, conversion mode is ON
	 * and preedit text exists.
	 */
	XIMCallback	  start_pecb_struct; 
	XIMCallback	  draw_pecb_struct;
	XIMCallback	  done_pecb_struct;
#ifdef FULL_R5
        XIMStyle	  xim_style;
#endif /* FULL_R5 */    	
	eucwidth_t	  euc_width;
	int		  locale_is_ale;
	Conv_pos_handle	  cph;	/* conversion pos handle */
#endif /* OW_I18N */
	split_init_proc_t orig_split_init_proc;
	char backup_pattern[1000];
	int restrict_menu;

	int smart_word_handling;
	long sel_reply;
	int current_sel;  /* TSW_SEL_PRIMARY or TSW_SEL_SECONDARY */

	size_t bufsize;
	char *selbuffer;

    Selection_item	sel_item[NBR_TSW_SELECTIONS];
    Selection_owner	sel_owner[NBR_TSW_SELECTIONS];
	int select_is_word[NBR_TSW_SELECTIONS];
    Selection_requestor	sel_req;
	textsw_convert_proc_t convert_proc;

	/* formerly static variables */
	int point_down_within_selection;
	short dnd_last_click_x, dnd_last_click_y;

	struct {
		Atom clipboard;
		Atom delete;
		Atom length;
		Atom null;
		Atom targets;
		Atom text;
		Atom timestamp;
		Atom ol_sel_word;
		Atom dragdrop_ack;
		Atom dragdrop_done;
		Atom selection_end;
		Atom seln_yield;
		Atom seln_is_readonly;
		Atom plain;
		Atom utf8;
		Atom future_use1;
		Atom future_use2;
		Atom future_use3;
	} atoms;
} Textsw_privstruct;
typedef Textsw_privstruct *	Textsw_private;

Pkg_private char *textsw_make_help(Textsw self, const char *str);
Pkg_private void textsw_free_help(Xv_opaque obj, int key, char *data);
Pkg_private	int ev_get_selection(Ev_chain chain, Es_index *first, Es_index *last_plus_one, unsigned type);
Pkg_private Menu textsw_get_unique_menu(Textsw_private );
Pkg_private int textsw_empty_document(Textsw abstract, Event *ie);
Pkg_private int textsw_file_name(Textsw_private    textsw, CHAR **name);
Pkg_private int textsw_has_been_modified(Textsw abstract);
Pkg_private int textsw_match_selection_and_normalize(Textsw_view_private view, CHAR *start_marker, unsigned field_flag);
Pkg_private void ev_remove_finger(Ev_finger_table *fingers, Ev_mark_object  mark);
Pkg_private void textsw_create_popup_frame(Textsw_view_private view, int popup_type);
Pkg_private void textsw_do_menu(Textsw_view_private view, Event *ie);
Pkg_private void textsw_esh_failed_msg(Textsw_private priv, char *preamble);
Pkg_private void textsw_freeze_caret(Textsw_private);
Pkg_private void textsw_get_and_set_selection(Frame popup_frame, Textsw_view_private view, int popup_type);
Pkg_private void textsw_reset_2(Textsw abstract, int locx, int locy, int preserve_memory, int cmd_is_undo_all_edit);
Pkg_private void textsw_set_base_mask(Xv_object win);
Pkg_private void textsw_set_dir_str(int popup_type);
Pkg_private void textsw_stablize(Textsw_private , int blink);
Pkg_private void textsw_thaw_caret(Textsw_private);
Pkg_private void textsw_undo(Textsw_private);
Pkg_private int textsw_open_cmd_proc(Frame fc, CHAR *path, CHAR *file, Xv_opaque client_data);
Pkg_private int textsw_save_cmd_proc(Frame fc, CHAR *path, struct stat *exists);
Pkg_private int textsw_include_cmd_proc(Frame fc, CHAR *path, CHAR *file, Xv_opaque client_data);
Pkg_private void textsw_find_pattern(Textsw_private textsw, Es_index *first, Es_index *last_plus_one, CHAR *buf, unsigned buf_len, unsigned flags);
Pkg_private Menu textsw_menu_get(Textsw_private);
Pkg_private void textsw_menu_set(Textsw_private, Menu);
Pkg_private void textsw_replace_esh(Textsw_private textsw, Es_handle new_esh);
Pkg_private int textsw_load_file(Textsw abstract, CHAR *filename,
    int reset_views, int locx, int locy);


extern string_t	null_string;
Pkg_private     Scrollbar textsw_get_scrollbar(Textsw_view_private view);

#ifdef DEBUG
Pkg_private     Textsw_private textsw_folio_for_view(Textsw_view_private view);
Pkg_private     Textsw textsw_view_rep_to_abs(Textsw_view_private rep);

#  define	TSWPRIV_FOR_VIEWPRIV(_view)		textsw_folio_for_view(_view)
#  define	VIEW_PUBLIC(_private)	textsw_view_rep_to_abs(_private)
#else /* DEBUG */
#  define	TSWPRIV_FOR_VIEWPRIV(_view)		((_view)->textsw_priv)
#endif /* DEBUG */

/* hier kann man ein Textsw_view oder aus ein Textsw uebergeben */
Pkg_private Textsw_view_private textsw_view_abs_to_rep(Textsw_view abstract
			, const char *func, int line);
#define	VIEW_ABS_TO_REP(_public)	textsw_view_abs_to_rep(_public,__FILE__,__LINE__)

#define	VALIDATE_FOLIO(_folio)		ASSERT((_folio)->magic == TEXTSW_MAGIC)

#define	TXTSW_HAS_READ_ONLY_BOUNDARY(folio_formal)	\
	(!EV_MARK_IS_NULL(&folio_formal->read_only_boundary))

	/* Synonyms for input event codes */
	/* Meta A is 225 */

#define TXTSW_LOAD_FILE		033		/* Do not map this */


	/* Bit flags for Textsw_handle->state */
#define TXTSW_AGAIN_HAS_FIND	0x00000001
#define TXTSW_AGAIN_HAS_MATCH   0x00000002
#ifdef OW_I18N
#define TXTSW_NO_UNDO_RECORDING	0x00000004
#endif
#define TXTSW_DELETE_REPLACES_CLIPBOARD         \
                                0x00000008
#define TXTSW_ADJUST_IS_PD	0x00000010
#define TXTSW_AUTO_INDENT	0x00000020
#define TXTSW_CONFIRM_OVERWRITE	0x00000040
#define TXTSW_CONTINUOUS_BUBBLE	0x00000080
#define TXTSW_NO_CD		0x00000100
#define TXTSW_NO_LOAD		0x00000200
#define TXTSW_DIFF_CR_LF        0x00000400	/*1030878*/
#define TXTSW_STORE_CHANGES_FILE		\
				0x00000800
#define TXTSW_READ_ONLY_ESH	0x00001000
#define TXTSW_READ_ONLY_SW	0x00002000
#define TXTSW_RETAINED		0x00008000
#define TXTSW_CAPS_LOCK_ON	0x00010000
#define TXTSW_DISPLAYED		0x00020000
#define TXTSW_EDITED		0x00040000
#define TXTSW_INITIALIZED	0x00080000
#define TXTSW_IN_NOTIFY_PROC	0x00100000
#define TXTSW_DOING_EVENT	0x00200000
#define TXTSW_NO_RESET_TO_SCRATCH		\
				0x00400000
#define TXTSW_NO_AGAIN_RECORDING		\
				0x00800000
#define TXTSW_HAS_FOCUS		0x01000000
#define TXTSW_OPENED_FONT	0x02000000
#define TXTSW_PENDING_DELETE	0x04000000
#define TXTSW_DESTROY_ALL_VIEWS	0x08000000
#define TXTSW_CONTROL_DOWN	0x10000000
#define TXTSW_SHIFT_DOWN	0x20000000
#define TXTSW_DELAY_SEL_INQUIRE 0x40000000

#define TXTSW_MISC_UNUSED	0x8000000c

	/* Bit flags for Textsw_handle->caret_state */
#define TXTSW_CARET_FLASHING	0x0001
#define TXTSW_CARET_ON		0x0002
#define TXTSW_CARET_MUST_SHOW	0x0004
#define TXTSW_CARET_TIMER_ON	0x0008
#define TXTSW_CARET_FROZEN	0x0010
#define	TXTSW_CARET_UNUSED	0xffe

	/* Bit flags for Textsw_handle->func_state */
#define TXTSW_FUNC_AGAIN	0x00000001
/* former TXTSW_FUNC_DELETE	under SunView */
#define TXTSW_FUNC_CUT		0x00000002
/* unused:  #define TXTSW_FUNC_FIELD	0x00000004 */
#define TXTSW_FUNC_FILTER	0x00000008
#define TXTSW_FUNC_FIND		0x00000010
#define TXTSW_FUNC_PASTE		0x00000020
#define TXTSW_FUNC_COPY		0x00000040
#define TXTSW_FUNC_UNDO		0x00000080
#define TXTSW_FUNC_ALL		(TXTSW_FUNC_AGAIN | TXTSW_FUNC_CUT | \
				 TXTSW_FUNC_FILTER | \
				 TXTSW_FUNC_FIND  | TXTSW_FUNC_PASTE    | \
				 TXTSW_FUNC_COPY   | TXTSW_FUNC_UNDO)
#define	TXTSW_FUNC_SVC_SAW(flags)	\
				((flags) << 8)
#define	TXTSW_FUNC_SVC_SAW_ALL	TXTSW_FUNC_SVC_SAW(TXTSW_FUNC_ALL)
#define	TXTSW_FUNC_EXECUTE	0x01000000
#define TXTSW_FUNC_SVC_REQUEST	0x10000000
#define	TXTSW_FUNC_SVC_ALL	(TXTSW_FUNC_SVC_SAW_ALL | \
				 TXTSW_FUNC_SVC_REQUEST)
#define TXTSW_FUNCTION_UNUSED	0xeeff8080


	/* Bit flags for Textsw_handle->track_state */
#define TXTSW_TRACK_ADJUST	0x0001
#define TXTSW_TRACK_ADJUST_END	0x0002
#define TXTSW_TRACK_POINT	0x0004
#define TXTSW_TRACK_SECONDARY	0x0008
#define TXTSW_TRACK_WIPE        0x0010	
#define TXTSW_TRACK_ALL		(TXTSW_TRACK_ADJUST|TXTSW_TRACK_ADJUST_END|\
				 TXTSW_TRACK_POINT|TXTSW_TRACK_SECONDARY)
#define TXTSW_TRACK_UNUSED	0xfff0

#define	TXTSW_IS_BUSY(textsw)				\
	((textsw->state & TXTSW_PENDING_DELETE) ||	\
	 (textsw->func_state & TXTSW_FUNC_ALL) ||	\
	 (textsw->track_state & TXTSW_TRACK_ALL))

#define	TXTSW_IS_READ_ONLY(textsw)			\
	(textsw->state & (TXTSW_READ_ONLY_ESH | TXTSW_READ_ONLY_SW))

	/* Flags for textsw_flush_caches */
#define TFC_INSERT		0x01
#define TFC_DO_PD		0x02
#define	TFC_SEL			0x04
#define TFC_PD_SEL		(TFC_DO_PD|TFC_SEL)
#define TFC_PD_IFF_INSERT	0x08
		/* Delete selection iff chars will be inserted. */
#define TFC_SEL_IFF_INSERT	0x10
		/* Clear selection iff chars will be inserted. */
#define TFC_IFF_INSERTING	(TFC_PD_IFF_INSERT | TFC_SEL_IFF_INSERT | \
				 TFC_INSERT)
#define TFC_ALL			(TFC_IFF_INSERTING|TFC_PD_SEL)
#define TFC_STD			TFC_ALL

	/* Flags for textsw_find_selection_and_normalize */
/* These flags potentially include an EV_SEL_BASE_TYPE */
#define	TFSAN_BACKWARD		EV_SEL_CLIENT_FLAG(0x0001)
#define	TFSAN_REG_EXP		EV_SEL_CLIENT_FLAG(0x0002)
#define	TFSAN_CLIPBOARD_ALSO	EV_SEL_CLIENT_FLAG(0x0004)
#define	TFSAN_TAG		EV_SEL_CLIENT_FLAG(0x0008)

#define SET_TEXTSW_TIMER(_timer_h)					\
	(_timer_h)->tv_sec = 0; (_timer_h)->tv_usec = 500000;
#define TIMER_EXPIRED(timer)						\
	(*timer && ((*timer)->tv_sec == 0) && ((*timer)->tv_usec == 0))
#define SCROLLBAR_ENTER_FEEDBACK	1

/* For caret motion */
typedef enum {
	TXTSW_CHAR_BACKWARD,
 	TXTSW_CHAR_FORWARD,
	TXTSW_DOCUMENT_END,
	TXTSW_DOCUMENT_START,
	TXTSW_LINE_END,
	TXTSW_LINE_START,
	TXTSW_NEXT_LINE_START,
	TXTSW_NEXT_LINE,
	TXTSW_PREVIOUS_LINE,
	TXTSW_WORD_BACKWARD,
	TXTSW_WORD_FORWARD,
	TXTSW_WORD_END
} Textsw_Caret_Direction;

/* Sub menu handles */
typedef enum {
	TXTSW_FILE_SUB_MENU,
	TXTSW_EDIT_SUB_MENU,
	TXTSW_VIEW_SUB_MENU,
	TXTSW_FIND_SUB_MENU,
	TXTSW_EXTRAS_SUB_MENU
} Textsw_sub_menu;

Pkg_private void textsw_flush_caches(Textsw_view_private view, int flags);

Pkg_private void textsw_begin_function(Textsw_view_private view, unsigned function);
Pkg_private void textsw_end_function(Textsw_view_private view,unsigned function);
Pkg_private int textsw_adjust_delete_span(Textsw_private priv, Es_index *first, Es_index *last_plus_one);

#define	TXTSW_PE_ADJUSTED	0x10000
#define	TXTSW_PE_EMPTY_INTERVAL	0x20000

Pkg_private Es_index textsw_delete_span(Textsw_view_private view, Es_index first, Es_index last_plus_one, unsigned flags);
#define	TXTSW_DS_DEFAULT		 EV_SEL_CLIENT_FLAG(0x0)
#define	TXTSW_DS_ADJUST			 EV_SEL_CLIENT_FLAG(0x1)
#define	TXTSW_DS_CLEAR_IF_ADJUST(sel)	(EV_SEL_CLIENT_FLAG(0x2)|(sel))
#define	TXTSW_DS_SHELVE			 EV_SEL_CLIENT_FLAG(0x4)
#define	TXTSW_DS_RECORD			 EV_SEL_CLIENT_FLAG(0x8)
#ifdef OW_I18N
#define	TXTSW_DS_RETURN_BYTES		EV_SEL_CLIENT_FLAG(0x10)
#endif

Pkg_private Es_index textsw_do_input(Textsw_view_private view, CHAR *buf,
						long int buf_len, unsigned flag);
Pkg_private Es_index textsw_wrap_do_input(Textsw tsw, Textsw_view_private view,
						CHAR *buf, long int *buf_len);
Pkg_private Es_index textsw_input_after(Textsw_view_private view, Es_index old_insert_pos, Es_index old_length, int record);
Pkg_private Es_index textsw_do_pending_delete(Textsw_view_private view, unsigned type, int flags);
Pkg_private void textsw_normalize_internal(Textsw_view_private view, Es_index first, Es_index last_plus_one, int upper_context, int lower_context, unsigned flags);

#define	TXTSW_NI_DEFAULT		 EV_SEL_CLIENT_FLAG(0x0)
#define	TXTSW_NI_AT_BOTTOM		 EV_SEL_CLIENT_FLAG(0x1)
#define	TXTSW_NI_MARK			 EV_SEL_CLIENT_FLAG(0x2)
#define	TXTSW_NI_NOT_IF_IN_VIEW		 EV_SEL_CLIENT_FLAG(0x4)
#define	TXTSW_NI_SELECT(sel)		(EV_SEL_CLIENT_FLAG(0x8)|(sel))
#define	TXTSW_NI_DONT_UPDATE_SCROLLBAR	 EV_SEL_CLIENT_FLAG(0x10)

Pkg_private Es_index textsw_set_insert( register Textsw_private priv, register Es_index pos);
Pkg_private Notify_value textsw_timer_expired(Textsw_private priv, int which);
Pkg_private void textsw_remove_timer(Textsw_private priv);
Pkg_private void textsw_invert_caret(Textsw_private textsw);
Pkg_private void textsw_take_down_caret(Textsw_private textsw);
Pkg_private void textsw_may_win_exit(Textsw_private textsw);
Pkg_private void textsw_post_error(Textsw_private priv, int locx, int locy, char *msg1, char *msg2);
Pkg_private void textsw_read_only_msg(Textsw_private textsw, int locx, int locy);
Pkg_private Textsw_status textsw_set_internal(Textsw_private textsw, Textsw_view_private view, Attr_attribute *attrs, int is_folio);
Pkg_private Xv_opaque textsw_view_set(Textsw_view view_public, Attr_attribute *avlist);

EXTERN_FUNCTION( void textsw_notify, (Textsw_view_private view, DOTDOTDOT) ) _X_SENTINEL(0);

#define	TEXTSW_CONSUME_ATTRS	TEXTSW_ATTR(ATTR_BOOLEAN, 240)

Pkg_private void textsw_init_undo(Textsw_private priv, int count);
Pkg_private Es_status textsw_checkpoint(Textsw_private priv);
Pkg_private caddr_t textsw_checkpoint_undo(Textsw abstract, caddr_t undo_mark);
Pkg_private void textsw_display(Textsw abstract);
Pkg_private void textsw_display_view(Textsw_view v, Rect  *rect);
Pkg_private void textsw_display_view_margins(Textsw_view_private view, struct rect *rect);
Pkg_private int textsw_is_seln_nonzero(Textsw_private textsw, unsigned type);
Pkg_private Es_index textsw_find_mark_internal(Textsw_private textsw, Ev_mark_object  mark);
Pkg_private Es_index textsw_read_only_boundary_is_at(Textsw_private priv);
Pkg_private Es_index textsw_insert_pieces(Textsw_view_private view, Es_index pos, Es_handle pieces);
Pkg_private Xv_opaque textsw_set(Textsw abstract, Attr_attribute *avlist);
Pkg_private int textsw_es_read(Es_handle esh, CHAR *buf, Es_index first, Es_index last_plus_one);
Pkg_private void textsw_update_scrollbars(Textsw_private priv, Textsw_view_private only_view);
Pkg_private void textsw_input_before(Textsw_private priv, Es_index *old_insert_pos, Es_index *old_length);
Pkg_private Key_map_handle textsw_do_filter(Textsw_view_private view, Event *ie);
Pkg_private void textsw_notify_replaced(Textsw_view_private view, Es_index insert_before, Es_index length_before, Es_index replaced_from, Es_index replaced_to, Es_index count_inserted);
Pkg_private void textsw_remove_mark_internal(Textsw_private textsw, Ev_mark_object mark);

extern int text_notice_key;

Pkg_private void ev_notify(Ev_handle view, ...);
Pkg_private	void ev_remove_op_bdry(Ev_finger_table *op_bdry, Es_index pos, unsigned type, unsigned mask);
Pkg_private	unsigned ev_op_bdry_info( Ev_finger_table op_bdry, Es_index pos, int *next_i);
Pkg_private	unsigned ev_op_bdry_info_merge(Ev_finger_table op_bdry, int i, int *next_i, unsigned prior);
Pkg_private Es_index ev_line_start(Ev_handle view, Es_index position);
Pkg_private void ev_extend_to_view_bottom(Ev_handle view, struct rect *rect);
Pkg_private	struct ei_process_result ev_display_internal(Ev_handle view, Rect  *rect, int    line, Es_index stop_plus_one, int ei_op, int break_action);
Pkg_private	void ev_display_in_rect(Ev_handle view, Rect  *rect);
Pkg_private int match_in_table(char *to_match, char **table);

Pkg_private Xv_opaque textsw_get(Textsw abstract, int *status, Attr_attribute attribute, va_list args);

Pkg_private Textsw_view_private text_view_frm_p_itm(Xv_opaque item);

Pkg_private int textsw_get_selection(Textsw_view_private view, Es_index *first,
				Es_index *last_plus_one, CHAR *selected_str, int  max_str_len);

Pkg_private Textsw_status textsw_file_stuff_from_str(Textsw_view_private view,
    CHAR *buf, int locx, int locy);

Pkg_private int textsw_change_directory(Textsw_private textsw, char *filename,
    int might_not_be_dir, int locx, int locy);

Pkg_private int textsw_expand_filename(Textsw_private textsw, char *buf, int locx, int locy);

Pkg_private Xv_Window frame_from_panel_item(Xv_opaque panel_item);
Pkg_private void textsw_create_sel_line_panel(Frame frame,
							Textsw_view_private view);
Pkg_private	void textsw_possibly_normalize_and_set_selection(Textsw abstract, Es_index first, Es_index last_plus_one, unsigned type);
Pkg_private int textsw_call_filter(Textsw_view_private view, char **);
Pkg_private Panel textsw_create_match_panel(Frame frame, Textsw_view_private view);
Pkg_private int textsw_match_field_and_normalize(Textsw_view_private view,
    Es_index *first, Es_index *last_plus_one, CHAR *buf1, unsigned buf_len1,
	unsigned field_flag, int do_search);
Pkg_private int textsw_get_selection_as_string(Textsw_private priv,
    unsigned type, CHAR *buf, int buf_max_len);
Pkg_private void textsw_record_caret_motion(Textsw_private textsw,
    unsigned direction, int loc_x);

Pkg_private void textsw_do_save(Textsw abstract, Textsw_private textsw, Textsw_view_private view);
Pkg_private int textsw_process_event(Textsw_view view_public, Event *ie, Notify_arg arg);
Pkg_private void textsw_resize(Textsw_view_private view);
Pkg_private	int textsw_mouseless_scroll_event(Textsw_view_private view, Event *ie, Notify_arg arg);
Pkg_private	int textsw_mouseless_select_event(Textsw_view_private view, Event *ie, Notify_arg arg);
Pkg_private	int textsw_mouseless_misc_event(Textsw_view_private view, Event *event);
Pkg_private int textsw_track_selection(Textsw_view_private view, Event *ie);
Pkg_private void textsw_possibly_edited_now_notify(Textsw_private priv);
Pkg_private void textsw_record_filter(Textsw_private textsw, Event *event);
Pkg_private int textsw_call_smart_filter(Textsw_view_private view, Event *event, char *filter_argv[]);
Pkg_private void textsw_init_again(Textsw_private priv, int count);
Xv_private void textsw_register_view(Textsw textsw, Xv_Window view_public);
Pkg_private void textsw_free_again(Textsw_private textsw,	string_t *again);
Pkg_private Textsw_view_private textsw_view_for_entity_view(Textsw_private priv, Ev_handle e_view);
Pkg_private long textsw_get_from_defaults(Attr32_attribute attribute, Xv_opaque srv);
Pkg_private	void textsw_do_again(Textsw_view_private view, int x,int y);
Pkg_private	void textsw_checkpoint_again(Textsw abstract);
Pkg_private void textsw_again(Textsw_view_private view, int x, int y);
Pkg_private void textsw_scroll(Scrollbar sb);
Pkg_private void textsw_begin_find(Textsw_view_private view);
Pkg_private int textsw_end_find(Textsw_view_private view, int event_code, int x, int y);
Pkg_private int textsw_get_local_selected_text(Textsw_private priv,
					unsigned typ, Es_index *first, Es_index *last_plus_one,
					char **selstr, int *str_len);
Pkg_private void textsw_start_seln_tracking(Textsw_view_private view, Event *ie);
Pkg_private void textsw_do_drag_copy_move(Textsw_view_private view, Event *ie, int is_copy);
Pkg_private int textsw_do_remote_drag_copy_move(Textsw_view_private view, Event *ie, int is_copy);
Pkg_private int textsw_end_copy(Textsw_view_private view);
Pkg_private void textsw_move_caret(Textsw_view_private view, Textsw_Caret_Direction direction);
Pkg_private void textsw_post_need_selection(Textsw abstract, Event *ie);
Pkg_private int textsw_do_edit(Textsw_view_private view, unsigned unit, unsigned direction);
Pkg_private void textsw_record_delete(Textsw_private textsw);
Pkg_private void textsw_record_edit(Textsw_private textsw, unsigned unit, unsigned direction);
Pkg_private void textsw_record_extras(Textsw_private priv, CHAR *cmd_line);
Pkg_private void textsw_record_find(Textsw_private textsw, CHAR *pattern, int pattern_length, int direction);
Pkg_private void textsw_record_input(Textsw_private textsw, CHAR *buffer, long int buffer_length);
Pkg_private void textsw_record_match(Textsw_private textsw, unsigned flag, CHAR *start_marker);
Pkg_private void textsw_record_piece_insert(Textsw_private textsw, Es_handle pieces);
Pkg_private int textsw_get_recorded_x(Textsw_view_private view);
Pkg_private char **textsw_string_to_argv(char *command_line);
Pkg_private void textsw_find_pattern_and_normalize(Textsw_view_private view, int x,int y, Es_index *first, Es_index *last_plus_one, CHAR *buf, unsigned buf_len, unsigned flags);
Pkg_private void textsw_find_selection_and_normalize(Textsw_view_private view, int x,int y, long unsigned options);
Pkg_private Ev_mark_object textsw_add_mark_internal(Textsw_private textsw, Es_index position, unsigned flags);
Pkg_private int textsw_parse_rc(Textsw_private textsw);
Pkg_private void textsw_flush_std_caches(Textsw textsw);
Pkg_private Menu_item textsw_extras_gen_proc(Menu_item mi, Menu_generate operation);
Pkg_private int textsw_build_extras_menu_items(Textsw_view, char *, Menu);
Pkg_private char *textsw_get_extras_filename(Xv_server srv, Menu_item mi);
Pkg_private     Textsw_view textsw_from_menu(Menu menu);
Pkg_private Es_index textsw_get_contents(Textsw_private textsw, Es_index position,
    CHAR *buffer, int buffer_length);
Pkg_private Es_status textsw_load_file_internal(Textsw_private textsw, CHAR *name,
		CHAR *scratch_name, Es_handle *piece_esh, Es_index  start_at,int flags);
Pkg_private Textsw_index textsw_position_for_physical_line(Textsw abstract, int physical_line);	/* Note: 1-origin, not 0! */
Pkg_private void textsw_format_load_error(char *msg, Es_status status, CHAR *filename, CHAR *scratch_name);
Pkg_private void textsw_set_null_view_avlist(Textsw_private priv, Attr_avlist attrs);
Pkg_private void textsw_setup_scrollbar( Scrollbar sb);
Pkg_private Es_handle textsw_create_ps(Textsw_private priv, Es_handle original, Es_handle scratch, Es_status *status);
Pkg_private void textsw_show_caret(Textsw_private textsw);
Pkg_private void textsw_hide_caret(Textsw_private textsw);
Pkg_private void textsw_freeze_caret(Textsw_private textsw);
Pkg_private void textsw_thaw_caret(Textsw_private textsw);
Pkg_private void textsw_invert_caret(Textsw_private textsw);
Pkg_private int textsw_is_typing_pending(Textsw_private priv, Event *event);
Pkg_private Textsw_status textsw_get_from_file(Textsw_view_private view, CHAR *filename, int print_error_msg);
Pkg_private void textsw_repaint(Textsw_view_private view);
Pkg_private void ev_line_info(Ev_handle view, int *top, int *bottom);
Pkg_private	Panel textsw_create_search_panel(Frame frame,
						Textsw_view_private view, int tf_key);
Pkg_private Menu textsw_menu_init(Textsw_private priv);
Pkg_private Textsw_view_private textsw_view_init_internal(
    Textsw_view_private view, Textsw_status  *status);
Pkg_private Textsw_private textsw_init_internal(Textsw_private priv,
			Textsw_status *status, int (*default_notify_proc)(Textsw, Attr_attribute *),
			Attr_attribute *attrs);
Pkg_private	void textsw_split_init_proc(Textsw_view public_view, Textsw_view public_new_view, int position);
Pkg_private Es_handle textsw_create_file_ps(Textsw_private priv, CHAR *name, CHAR *scratch_name, Es_status *status);
Pkg_private     Es_handle textsw_create_mem_ps(Textsw_private priv, Es_handle original);
Pkg_private void textsw_compute_scroll(Scrollbar sb, int pos, int length, Scroll_motion motion, long *offset, long *obj_length);
Pkg_private void textsw_destroy_esh(Textsw_private priv, Es_handle esh);
Pkg_private int textsw_load_selection(Textsw_private priv, int locx, int locy, int no_cd);
Pkg_private int textsw_does_index_not_show(Textsw_view vpub, Es_index index, int *line_index);	/* output only.  if index does not show, not set. */
Pkg_private int textsw_screen_column_count(Textsw abstract);
Pkg_private Textsw_mark textsw_add_glyph_on_line(Textsw abstract, int line, struct pixrect *pr, int op, int offset_x, int offset_y, int flags);
Pkg_private void textsw_remove_glyph(Textsw abstract, Textsw_mark mark, int flags);
Pkg_private void textsw_real_update_scrollbars(Textsw_private priv);
Pkg_private void textsw_do_resize(Textsw_view abstract);
Pkg_private Textsw_index textsw_start_of_display_line(Textsw abstract, Textsw_index pos);
Pkg_private     Es_handle textsw_esh_for_span(Textsw_view_private view, Es_index first, Es_index last_plus_one, Es_handle to_recycle);
Pkg_private Es_index textsw_do_balance_beam(Textsw_view_private view, int x, int y, Es_index first, Es_index last_plus_one);
Pkg_private int textsw_input_partial(Textsw_private priv, CHAR *buf, long int buf_len);
Pkg_private Textsw_private textsw_folio_for_view(Textsw_view_private view);
Pkg_private     Textsw textsw_view_rep_to_abs(Textsw_view_private rep);
Pkg_private Notify_value textsw_view_event(Textsw_view view_public,
				Notify_event ev, Notify_arg arg, Notify_event_type type);
Pkg_private Notify_value textsw_view_event_internal(Textsw_view view_public,
				Notify_event ev, Notify_arg arg, Notify_event_type type);
Pkg_private void textsw_update_replace(Frame, int);

Pkg_private void textsw_stop_blinker(Textsw_private priv);
Pkg_private Es_status textsw_store_init(Textsw_private textsw, CHAR *filename);
Pkg_private Es_status textsw_process_store_error(Textsw_private textsw, CHAR *filename, Es_status status, int locx, int locy);

/* #undef Pkg_private  */
/* #define Pkg_private alter_quatsch */

/* Used as dampers to hand motion when trying to select an insertion point */
#define	TXTSW_X_POINT_SLOP 2
#define TXTSW_Y_POINT_SLOP 1

Pkg_private void textsw_new_selection_init(Textsw tsw);
Pkg_private void textsw_new_selection_destroy(Textsw tsw);
Pkg_private void textsw_new_sel_copy(Textsw_view_private view, Event *ev);
Pkg_private void textsw_new_sel_copy_appending(Textsw_view_private view,
									Event *ev);
Pkg_private void textsw_new_sel_cut(Textsw_view_private view, Event *ev, 
									int canbequick);
Pkg_private int textsw_new_sel_paste(Textsw_view_private view, Event *ev,
									int canbequick);
Pkg_private void textsw_new_sel_select(Textsw_view_private view, Event *ev);
Pkg_private void textsw_new_sel_adjust(Textsw_view_private view, Event *ev);
Pkg_private void textsw_new_sel_drag(Textsw_view_private view, Event *ev);
Pkg_private int textsw_internal_convert(Textsw_private priv,
				Textsw_view_private view, Selection_owner owner, Atom *type,
				Xv_opaque *data, unsigned long *length, int *format);
Pkg_private int textsw_convert_delete(Textsw_private priv,
				Textsw_view_private view, Selection_owner owner,
				int handle_smart_word);
Pkg_private int textsw_selection_is_word(Selection_requestor sr);
Pkg_private Es_index textsw_smart_word_handling(Textsw_private priv,
				int is_word, char *wbuf, Es_index pos, long length);
Pkg_private void textsw_cleanup_termsw_menuitems(Xv_opaque priv);

#endif
