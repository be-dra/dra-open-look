#ifndef lint
char     txt_input_c_sccsid[] = "@(#)txt_input.c 20.109 93/06/28 DRA: $Id: txt_input.c,v 4.41 2025/02/15 13:16:37 dra Exp $";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * User input interpreter for text subwindows.
 */

#include <xview_private/primal.h>
#include <xview_private/txt_impl.h>
#include <xview_private/ev_impl.h>
#include <xview_private/svr_impl.h>
#include <xview_private/txt_18impl.h>
#include <errno.h>
/* Only needed for SHIFTMASK and CTRLMASK */
#include <pixrect/pr_util.h>

#ifdef __STDC__ 
#ifndef CAT
#define CAT(a,b)        a ## b 
#endif 
#endif
#include <pixrect/memvar.h>

#include <xview/notice.h>
#include <xview/help.h>
#include <xview/frame.h>
#include <xview/win_struct.h>
#include <xview/win_input.h>	/* needed for event_shift_is_down() */
#include <xview/cursor.h>
#include <xview_private/win_keymap.h>
#include <xview_private/windowimpl.h>
#ifdef SVR4 
#include <stdlib.h> 
#endif /* SVR4 */

Pkg_private int textsw_timer_active;
static void textsw_do_undo(Textsw_view_private view);
#ifdef OW_I18N
Pkg_private     void textsw_implicit_commit_doit();
#endif

#ifdef OW_I18N
Pkg_private	int
#else
static int
#endif
textsw_do_newline(Textsw_view_private view, int action);
static void textsw_undo_notify(Textsw_private priv, Es_index start, Es_index delta);
static int textsw_scroll_event(Textsw_view_private view, Event *ie, Notify_arg arg);
static int textsw_function_key_event(Textsw_view_private view, Event *ie, int *result);
static int textsw_mouse_event(Textsw_view_private view, Event *ie);
static int textsw_edit_function_key_event(Textsw_view_private view, Event *ie, int *result);
static int textsw_caret_motion_event(Textsw_view_private view, Event *ie);
static int textsw_field_event(Textsw_view_private view, Event *ie);
static int textsw_file_operation(Textsw abstract, Event *ie);
static int textsw_erase_action(Textsw_view v, Event *ie);

extern char *xv_app_name;
/* #define XX(_a_) fprintf(stderr,"%s: %s`%s-%d: %s\n", xv_app_name, __FILE__,__FUNCTION__, __LINE__, _a_) */
#define XX(_a_)

#define SPACE_CHAR 0x20

Pkg_private void textsw_flush_caches(Textsw_view_private view, int flags)
{
	register Textsw_private textsw = TSWPRIV_FOR_VIEWPRIV(view);
	register long count;
	register int end_clear = (flags & TFC_SEL);

	count = (textsw->func_state & TXTSW_FUNC_FILTER)
			? 0 : (textsw->to_insert_next_free - textsw->to_insert);
	if (flags & TFC_DO_PD) {
		if ((count > 0) || ((flags & TFC_PD_IFF_INSERT) == 0)) {
			ev_set(view->e_view, EV_CHAIN_DELAY_UPDATE, TRUE, NULL);
			(void)textsw_do_pending_delete(view, EV_SEL_PRIMARY,
					end_clear | TFC_INSERT);
			ev_set(view->e_view, EV_CHAIN_DELAY_UPDATE, FALSE, NULL);
			end_clear = 0;
		}
	}
	if (end_clear) {
		if ((count > 0) || ((flags & TFC_SEL_IFF_INSERT) == 0)) {
			textsw_set_selection(TEXTSW_PUBLIC(textsw),
					ES_INFINITY, ES_INFINITY, EV_SEL_PRIMARY);
		}
	}
	if (flags & TFC_INSERT) {
		if (count > 0) {
			/*
			 * WARNING!  The cache pointers must be updated BEFORE calling
			 * textsw_do_input, so that if the client is being notified of
			 * edits and it calls textsw_get, it will not trigger an infinite
			 * recursion of textsw_get calling textsw_flush_caches calling
			 * textsw_do_input calling the client calling textsw_get calling
			 * ...
			 */
			textsw->to_insert_next_free = textsw->to_insert;
			textsw_do_input(view, textsw->to_insert, count,
					TXTSW_UPDATE_SCROLLBAR_IF_NEEDED);
		}
	}
}

Pkg_private void textsw_flush_std_caches(Textsw win)
{
    Textsw_view_private view;

	/* hier kann TEXTSW oder TERMSW kommen */
	if (xv_get(win, XV_IS_SUBTYPE_OF, OPENWIN)) {
		/* was ist mit den anderen Views?????   INCOMPLETE */
		view = VIEW_PRIVATE(xv_get(win, OPENWIN_NTH_VIEW, 0));
	}
	/* oder auch ein Termsw_view...  */
	else if (xv_get(win, XV_IS_SUBTYPE_OF, OPENWIN_VIEW)) {
		view = VIEW_PRIVATE(win);
	}
	else {
		view = VIEW_ABS_TO_REP(win);
	}

    textsw_flush_caches(view, TFC_STD);
}


/*ARGSUSED*/
Pkg_private void textsw_read_only_msg(Textsw_private textsw, int locx, int locy)
{
	Frame frame = xv_get(TEXTSW_PUBLIC(textsw), WIN_FRAME);
	Xv_Notice text_notice;

	text_notice = xv_get(frame, XV_KEY_DATA, text_notice_key);
	if (!text_notice) {
		text_notice = xv_create(frame, NOTICE, NULL);

		xv_set(frame, XV_KEY_DATA, text_notice_key, text_notice, NULL);
	}
	xv_set(text_notice,
			NOTICE_MESSAGE_STRINGS,
				XV_MSG("The text is read-only and cannot be edited.\n\
Press \"Continue\" to proceed."),
				NULL,
			NOTICE_BUTTON_YES, XV_MSG("Continue"),
			NOTICE_BUSY_FRAMES, frame, NULL,
			XV_SHOW, TRUE,
			NULL);
}

static int textsw_note_event_shifts(Textsw_private textsw, struct inputevent *ie)
{
	int result = 0;

	if (ie->ie_shiftmask & SHIFTMASK)
		textsw->state |= TXTSW_SHIFT_DOWN;
	else {
		textsw->state &= ~TXTSW_SHIFT_DOWN;
	}
	if (ie->ie_shiftmask & CTRLMASK)
		textsw->state |= TXTSW_CONTROL_DOWN;
	else
		textsw->state &= ~TXTSW_CONTROL_DOWN;
	return (result);
}

#ifdef GPROF
static void
textsw_gprofed_routine(view, ie)
    register Textsw_view_private view;
    register Event *ie;
{
}

#endif

Pkg_private int textsw_process_event(Textsw_view view_public, Event *ev, Notify_arg arg)
{

/* 	int caret_was_up; */
	int result = TEXTSW_PE_USED;
	register Textsw_view_private view = VIEW_PRIVATE(view_public);
	register Textsw_private textsw = TSWPRIV_FOR_VIEWPRIV(view);
	int action = event_action(ev);
	register int down_event = event_is_down(ev);

#ifdef OW_I18N
	char byte;
#endif

/* 	caret_was_up = textsw->caret_state & TXTSW_CARET_ON; */
	/* Watch out for caret turds */
	if ((action == LOC_MOVE || action == LOC_WINENTER || action == LOC_WINEXIT)
			&& !TXTSW_IS_BUSY(textsw)) {
		/* leave caret up */
	}
	else {
		textsw_take_down_caret(textsw);
	}
	textsw_note_event_shifts(textsw, ev);

#ifdef OW_I18N
	if (event_is_string(ev) && down_event && textsw->ic) {
		int num_of_char =
				mbstowcs(textsw->to_insert_next_free, event_string(ev),
				((TXTSW_UI_BUFLEN - (textsw->to_insert_next_free -
										textsw->to_insert)) - 1));

		textsw->to_insert_next_free += num_of_char;

		if (!textsw->preedit_start)
			textsw_flush_caches(view, TFC_STD);
		else {	/* This is for partial commit */
			Es_index mark_pos, old_insert_pos, tmp;

			/* Set the insertion position front of the preedit text */
			old_insert_pos = EV_GET_INSERT(textsw->views);
			mark_pos = textsw_find_mark_internal(textsw, textsw->preedit_start);
			EV_SET_INSERT(textsw->views, mark_pos, tmp);
			ev_remove_all_op_bdry(textsw->views, mark_pos, mark_pos,
					EV_SEL_PRIMARY | EV_SEL_SECONDARY, EV_BDRY_TYPE_ONLY);

			/* Resume undo and again for commited text */
			textsw->state &=
					~(TXTSW_NO_UNDO_RECORDING | TXTSW_NO_AGAIN_RECORDING);
			/* Insert the commited text */
			textsw_flush_caches(view, TFC_STD);

			/* Turn off undo and again */
			textsw->state |=
					(TXTSW_NO_UNDO_RECORDING | TXTSW_NO_AGAIN_RECORDING);
			/* Update preedit_start front of the preedit text */
			textsw_remove_mark_internal(textsw, textsw->preedit_start);
			textsw->preedit_start = textsw_add_mark_internal(textsw,
					EV_GET_INSERT(textsw->views), NULL);

			/* Reset the insertion position after the preedit text */
			EV_SET_INSERT(textsw->views, old_insert_pos + num_of_char, tmp);
		}
		if ((textsw->state & TXTSW_EDITED) == 0)
			textsw_possibly_edited_now_notify(textsw);
		goto Done;
	}
#endif /* OW_I18N */

	/* NOTE: This is just a hack for performance */
	if ((action >= SPACE_CHAR) && (action <= ASCII_LAST))
		goto Process_ASCII;

	if (action == ACTION_HELP || action == ACTION_MORE_HELP ||
			action == ACTION_TEXT_HELP || action == ACTION_MORE_TEXT_HELP ||
			action == ACTION_INPUT_FOCUS_HELP) {
		if (down_event)
			xv_help_show(XV_PUBLIC(view),
					(char *)xv_get(view_public, XV_HELP_DATA), ev);
	}
	else if (event_action(ev) == WIN_RESIZE) {
		textsw_resize(view);
		ev_set(view->e_view, EV_NO_REPAINT_TIL_EVENT, TRUE, NULL);
		textsw_display_view(VIEW_PUBLIC(view), (Rect *) 0);
		ev_set(view->e_view, EV_NO_REPAINT_TIL_EVENT, FALSE, NULL);
	}
	else if (textsw_mouseless_scroll_event(view, ev, arg)) {
		/* taken care of -- new mouseless function  jcb 12/21/90 */
	}
	else if (textsw_mouseless_select_event(view, ev, arg)) {
		/* taken care of -- new mouseless function  jcb 1/2/91 */
	}
	else if (textsw_mouseless_misc_event(view, ev)) {
		/* taken care of -- new mouseless function  jcb 1/2/91 */
	}
	else if (textsw_scroll_event(view, ev, arg)) {
		/* Its already taken care by the procedure */
	}
	else if ((textsw->track_state &
					(TXTSW_TRACK_ADJUST | TXTSW_TRACK_POINT | TXTSW_TRACK_WIPE))
			&& textsw_track_selection(view, ev)) {
		/*
		 * Selection tracking and function keys. textsw_track_selection() always
		 * called if tracking.  It consumes the event unless function key up
		 * while tracking secondary selection, which stops tracking and then
		 * does function.
		 */
	}
	else if (textsw_function_key_event(view, ev, &result)) {
		/* Its already taken care by the procedure */

	}
	else if (textsw_mouse_event(view, ev)) {
		/* Its already taken care by the procedure */
	}
	else if (textsw_edit_function_key_event(view, ev, &result)) {
		if (result & TEXTSW_PE_READ_ONLY)
			goto Read_Only;

	}
	else if (textsw_caret_motion_event(view, ev)) {
		/* Its already taken care by the procedure */

	}
	else if (action == ACTION_CAPS_LOCK) {
		if (TXTSW_IS_READ_ONLY(textsw))
			goto Read_Only;
		if (!down_event) {
			textsw->state ^= TXTSW_CAPS_LOCK_ON;
			textsw_notify(view, TEXTSW_ACTION_CAPS_LOCK,
					(textsw->state & TXTSW_CAPS_LOCK_ON), NULL);
		}
		/*
		 * Type-in
		 */
	}
	else if (textsw->track_state & TXTSW_TRACK_SECONDARY) {
		/* No type-in processing during secondary (function) selections */
	}
	else if (textsw_field_event(view, ev)) {
		/* Its already taken care by the procedure */
	}
	else if ((action == ACTION_EMPTY) && down_event) {
		textsw_empty_document(xv_get(view_public, XV_OWNER), ev);
		goto Done;
	}
	else if (textsw_file_operation(xv_get(view_public, XV_OWNER), ev)) {
		if (action == ACTION_LOAD)
			goto Done;

	}
	else if (textsw_erase_action(view_public, ev)) {
		if (TXTSW_IS_READ_ONLY(textsw))
			goto Read_Only;
		else if ((textsw->state & TXTSW_EDITED) == 0)
			textsw_possibly_edited_now_notify(textsw);
		goto Done;
	}
	if ((action <= ISO_LAST) && down_event) {

		if (textsw->func_state & TXTSW_FUNC_FILTER) {
			if (textsw->to_insert_next_free <
					textsw->to_insert + sizeof(textsw->to_insert)) {
				*textsw->to_insert_next_free++ = (char)action;
			}
		}
		else {
		  Process_ASCII:
			switch (action) {
				case (short)'\r':	/* Fall through */
				case (short)'\n':
					if (down_event) {
						if (TXTSW_IS_READ_ONLY(textsw))
							goto Read_Only;

#ifdef OW_I18N
						/* This is a transaction for bug 1090046. */
						if (textsw->preedit_start)
							textsw->blocking_newline = TRUE;
						else
#endif

						{	/* 1030878 */
							if (textsw->state & TXTSW_DIFF_CR_LF)
								(void)textsw_do_newline(view, action);
							else
								(void)textsw_do_newline(view, '\n');
						}
					}
					break;
				default:
					if (TXTSW_IS_READ_ONLY(textsw))
						goto Read_Only;
					if (!down_event)
						break;
					if (textsw->state & TXTSW_CAPS_LOCK_ON) {
						if ((char)action >= 'a' && (char)action <= 'z')
							ev->ie_code += 'A' - 'a';
						/* BUG ALERT: above may need to set event_id */
					}

#ifdef OW_I18N
					/*
					 * Asian Textsw dosen't handle NULL character as current spec.
					 * If the spec is changed to handle NULL character, following
					 * statement should be removed.
					 */
					if (action == NULL)
						break;
					byte = event_action(ev);

#ifdef sun
					if (isascii(byte))
						/*
						 * Type casting wchar_t is not quite right thing
						 * to do, however we know ASCII works in Sun.
						 */
						*textsw->to_insert_next_free++ = byte;
					else
#endif

						mbtowc(textsw->to_insert_next_free++, &byte, 1);
#else /* OW_I18N */
					if (event_is_string(ev)) {
						char *p = event_string(ev);
						while (*p) {
							*textsw->to_insert_next_free++ = *p++;
						}
						SERVERTRACE((505, "ins '%s'\n", event_string(ev)));
					}
					else {
						SERVERTRACE((505, "ins '%c'\n", event_action(ev)));
						*textsw->to_insert_next_free++ = (char)event_action(ev);
					}
#endif /* OW_I18N */

					if (textsw->to_insert_next_free ==
							textsw->to_insert + sizeof(textsw->to_insert)) {
						textsw_flush_caches(view, TFC_STD);
					}
					break;
			}
		}
		/*
		 * User filters
		 */
	}
	else if (textsw_do_filter(view, ev)) {
		/*
		 * Miscellaneous
		 */
	}
	else {
		result &= ~TEXTSW_PE_USED;
	}
	/*
	 * Cleanup
	 */
	if ((textsw->state & TXTSW_EDITED) == 0)
		textsw_possibly_edited_now_notify(textsw);
  Done:
	if (TXTSW_IS_BUSY(textsw))
		result |= TEXTSW_PE_BUSY;
	return (result);
  Read_Only:
	result |= TEXTSW_PE_READ_ONLY;
	goto Done;
}

static Key_map_handle find_key_map(Textsw_private textsw, Event *ie)
{
    register Key_map_handle current_key = textsw->key_maps;

    while (current_key) {
	if (current_key->event_code == event_action(ie)) {
	    break;
	}
	current_key = current_key->next;
    }
    return (current_key);
}

Pkg_private Key_map_handle textsw_do_filter(Textsw_view_private view, Event *ie)
{
	register Textsw_private textsw = TSWPRIV_FOR_VIEWPRIV(view);
	register Key_map_handle result = find_key_map(textsw, ie);
	Frame frame;
	Xv_Notice text_notice;

	if (result == 0)
		goto Return;
	if (event_is_down(ie)) {
		switch (result->type) {
			case TXTSW_KEY_SMART_FILTER:
			case TXTSW_KEY_FILTER:
				textsw_flush_caches(view, TFC_STD);
				textsw->func_state |= TXTSW_FUNC_FILTER;
				result = 0;
				break;
		}
		goto Return;
	}
	switch (result->type) {
		case TXTSW_KEY_SMART_FILTER:
		case TXTSW_KEY_FILTER:{
				int again_state, filter_result;

				again_state = textsw->func_state & TXTSW_FUNC_AGAIN;
				textsw_record_filter(textsw, ie);
				textsw->func_state |= TXTSW_FUNC_AGAIN;
				/* why do you always intermix a TEXTSW and its views ?
				   textsw_checkpoint_undo(VIEW_PUBLIC(view),
						(caddr_t) TEXTSW_INFINITY - 1);
				*/
				textsw_checkpoint_undo(TEXTSW_PUBLIC(textsw),
						(caddr_t) TEXTSW_INFINITY - 1);
				if (result->type == TXTSW_KEY_SMART_FILTER) {
					filter_result = textsw_call_smart_filter(view, ie,
							(char **)result->maps_to);
				}
				else {
					filter_result =
							textsw_call_filter(view, (char **)result->maps_to);
				}
				textsw_checkpoint_undo(TEXTSW_PUBLIC(textsw),
						(caddr_t) TEXTSW_INFINITY - 1);
				switch (filter_result) {
					case 0:
					default:
						break;
					case 1:{
							char msg[300];

							if (errno == ENOENT) {
								(void)sprintf(msg,
										XV_MSG("Cannot locate filter '%s'."),
										((char **)result->maps_to)[0]);
							}
							else {
								(void)sprintf(msg,
										XV_MSG
										("Unexpected problem with filter '%s'."),
										((char **)result->maps_to)[0]);
							}
							frame = xv_get(VIEW_PUBLIC(view), WIN_FRAME);
							text_notice = xv_get(frame,
									XV_KEY_DATA, text_notice_key);

							if (!text_notice) {
								text_notice = xv_create(frame, NOTICE, NULL);

								xv_set(frame,
										XV_KEY_DATA, text_notice_key,
										text_notice, NULL);
							}
							xv_set(text_notice,
									NOTICE_MESSAGE_STRINGS, msg, NULL,
									NOTICE_BUTTON_YES,
									XV_MSG("Continue"),
									NOTICE_BUSY_FRAMES, frame, NULL,
									XV_SHOW, TRUE,
									NULL);
							break;
						}
				}
				textsw->func_state &= ~TXTSW_FUNC_FILTER;
				textsw->to_insert_next_free = textsw->to_insert;
				if (again_state == 0)
					textsw->func_state &= ~TXTSW_FUNC_AGAIN;
				result = 0;
			}
	}
  Return:
	return (result);
}


#ifdef OW_I18N
Pkg_private	int
#else
static int
#endif
textsw_do_newline(Textsw_view_private view, int action)
{
	register Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
	int delta;
	Es_index first = EV_GET_INSERT(priv->views), last_plus_one, previous;
	CHAR newline_str[2];

	newline_str[0] = action;	/* 1030878 */
	newline_str[1] = 0;

	textsw_flush_caches(view, TFC_INSERT | TFC_PD_SEL);
	if (priv->state & TXTSW_AUTO_INDENT)
		first = EV_GET_INSERT(priv->views);
	delta = textsw_do_input(view, newline_str, 1L, TXTSW_UPDATE_SCROLLBAR);
	if (priv->state & TXTSW_AUTO_INDENT) {
		previous = first;
		textsw_find_pattern(priv, &previous, &last_plus_one,
				newline_str, 1, EV_FIND_BACKWARD);
		if (previous != ES_CANNOT_SET) {
			CHAR buf[100];
			struct es_buf_object esbuf;
			register CHAR *c;

			esbuf.esh = priv->views->esh;
			esbuf.buf = buf;
			esbuf.sizeof_buf = SIZEOF(buf);
			if (es_make_buf_include_index(&esbuf, previous, 0) == 0) {
				if (AN_ERROR(buf[0] != '\n')) {
				}
				else {
					for (c = buf + 1; c < buf + SIZEOF(buf); c++) {
						switch (*c) {
							case '\t':
							case ' ':
								break;
							default:
								goto Did_Scan;
						}
					}
				  Did_Scan:
					if (c != buf + 1) {
						delta += textsw_do_input(view, buf + 1,
								(long)(c - buf - 1),
								TXTSW_UPDATE_SCROLLBAR_IF_NEEDED);
					}
				}
			}
		}
	}
	return (delta);
}

/*
 * ==========================================================
 * 
 * Input mask initialization and setting.
 * 
 * ==========================================================
 */

static struct inputmask basemask_kbd;
#ifdef SUNVIEW1
static struct inputmask basemask_pick;
#endif

static int      masks_have_been_initialized;	/* Defaults to FALSE */

static void setupmasks(void)
{
	register struct inputmask *mask;
	register int i;

	/*
	 * Set up the standard kbd mask.
	 */
	mask = &basemask_kbd;
	input_imnull(mask);
	mask->im_flags |= IM_ASCII | IM_NEGEVENT | IM_META | IM_NEGMETA;
	for (i = 1; i < 17; i++) {
		win_setinputcodebit(mask, KEY_LEFT(i));
		win_setinputcodebit(mask, KEY_TOP(i));
		win_setinputcodebit(mask, KEY_RIGHT(i));
	}
	/* Unset TOP and OPEN because will look for them in pick mask */
	/*
	 * Use win_keymap_*inputcodebig() for events that are semantic, i.e.,
	 * have no actual inputmask bit assignment.  This is done in
	 * textsw_set_base_mask() below since we need the fd to know what keymap
	 * to look in.
	 */
	win_setinputcodebit(mask, KBD_USE);
	win_setinputcodebit(mask, KBD_DONE);

#ifdef SUNVIEW1
	/*
	 * Set up the standard pick mask.
	 */
	mask = &basemask_pick;
	input_imnull(mask);
#endif

	win_setinputcodebit(mask, WIN_STOP);
	/*
	 * LOC_WINENTER & LOC_WINEXIT not needed since click-to-type is the
	 * default. Will be needed when follow-cursor is implemented.
	 */
	/*
	 * win_setinputcodebit(mask, LOC_WINENTER); win_setinputcodebit(mask,
	 * LOC_WINEXIT);
	 */
	win_setinputcodebit(mask, LOC_DRAG);
	win_setinputcodebit(mask, MS_LEFT);
	win_setinputcodebit(mask, MS_MIDDLE);
	win_setinputcodebit(mask, MS_RIGHT);
	win_setinputcodebit(mask, WIN_REPAINT);
	win_setinputcodebit(mask, WIN_VISIBILITY_NOTIFY);
	mask->im_flags |= IM_NEGEVENT;
	masks_have_been_initialized = TRUE;
}

Pkg_private void textsw_set_base_mask(Xv_object win)
{

    if (masks_have_been_initialized == FALSE) {
		setupmasks();
    }
    win_keymap_set_smask_class(win, KEYMAP_FUNCT_KEYS);
    win_keymap_set_smask_class(win, KEYMAP_EDIT_KEYS);
    win_keymap_set_smask_class(win, KEYMAP_MOTION_KEYS);
    win_keymap_set_smask_class(win, KEYMAP_TEXT_KEYS);

    win_setinputmask(win, &basemask_kbd, 0, 0L);

    xv_set(win, WIN_CONSUME_EVENT, ACTION_HELP, NULL);

}

/*
 * ==========================================================
 * 
 * Functions invoked by function keys.
 * 
 * ==========================================================
 */

static void textsw_init_timer(Textsw_private priv)
{
    /*
     * Make last_point/_adjust/_ie_time close (but not too close) to current
     * time to avoid overflow in tests for multi-click.
     */
    priv->last_point.tv_sec -= 1000;
    priv->last_adjust = priv->last_point;
    priv->last_ie_time = priv->last_point;
}


Pkg_private void textsw_begin_function(Textsw_view_private view, unsigned function)
{
	register Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);

	textsw_flush_caches(view, TFC_STD);
	if ((priv->state & TXTSW_CONTROL_DOWN) && !TXTSW_IS_READ_ONLY(priv))
		priv->state |= TXTSW_PENDING_DELETE;
	priv->track_state |= TXTSW_TRACK_SECONDARY;
	priv->func_state |= function | TXTSW_FUNC_EXECUTE;
	ASSUME(EV_MARK_IS_NULL(&priv->save_insert));
	EV_MARK_SET_MOVE_AT_INSERT(priv->save_insert);
	ev_add_finger(&priv->views->fingers, EV_GET_INSERT(priv->views),
			0, &priv->save_insert);
	textsw_init_timer(priv);
	if AN_ERROR
		(priv->func_state & TXTSW_FUNC_SVC_SAW(function))
				/* Following covers up inconsistent state with Seln. Svc. */
				priv->func_state &= ~TXTSW_FUNC_SVC_SAW(function);
}

Pkg_private void textsw_end_function(Textsw_view_private view, unsigned function)
{
	register Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);

	/* restore insertion point */
	if (!EV_MARK_IS_NULL(&priv->save_insert)) {
		ev_remove_finger(&priv->views->fingers, priv->save_insert);
		EV_INIT_MARK(priv->save_insert);
	}
	priv->state &= ~TXTSW_PENDING_DELETE;
	priv->track_state &= ~TXTSW_TRACK_SECONDARY;
	priv->func_state &=
			~(function | TXTSW_FUNC_SVC_SAW(function) | TXTSW_FUNC_EXECUTE);
}

static void textsw_begin_again(Textsw_view_private view)
{
#ifdef OW_I18N
    /*
     * Hopefully, wanted to put it right before textsw_do_again(),
     * but textsw_begin_function() makes another checkpoint
     * by calling textsw_checkpoint_again() for AGAIN action.
     */
    textsw_implicit_commit(TSWPRIV_FOR_VIEWPRIV(view));
#endif
    textsw_begin_function(view, TXTSW_FUNC_AGAIN);
}

static void textsw_end_again(Textsw_view_private view, int x, int y)
{
    textsw_do_again(view, x, y);
    textsw_end_function(view, TXTSW_FUNC_AGAIN);
    /*
     * Make sure each sequence of again will not accumulate. Like find,
     * delete and insert.
     */
    textsw_checkpoint_again(xv_get(VIEW_PUBLIC(view), XV_OWNER));
}

Pkg_private void textsw_again(Textsw_view_private view, int x, int y)
{
    textsw_begin_again(view);
    textsw_end_again(view, x, y);
}

static void textsw_begin_undo(Textsw_view_private view)
{
    textsw_begin_function(view, TXTSW_FUNC_UNDO);
    textsw_flush_caches(view, TFC_SEL);
}

static void textsw_end_undo(Textsw_view_private view)
{
#ifdef OW_I18N
    textsw_implicit_commit(TSWPRIV_FOR_VIEWPRIV(view));
#endif
    textsw_do_undo(view);
    textsw_end_function(view, TXTSW_FUNC_UNDO);
    textsw_update_scrollbars(TSWPRIV_FOR_VIEWPRIV(view), TEXTSW_VIEW_NULL);
}

static void textsw_undo_notify(Textsw_private priv, Es_index start, Es_index delta)
{
    Ev_chain chain = priv->views;
    Es_index old_length = es_get_length(chain->esh) - delta;
    Es_index        old_insert = 0, temp;

    if (priv->notify_level & TEXTSW_NOTIFY_EDIT)
	old_insert = EV_GET_INSERT(chain);
    EV_SET_INSERT(chain, ((delta > 0) ? start + delta : start), temp);
    ev_update_after_edit(chain,
			 (delta > 0) ? start : start - delta,
			 (int)delta, old_length, start);
    if (priv->notify_level & TEXTSW_NOTIFY_EDIT) {
	textsw_notify_replaced(VIEW_PRIVATE(xv_get(XV_PUBLIC(priv),
						OPENWIN_NTH_VIEW, 0)),
			       old_insert, old_length,
			       (delta > 0) ? start : start + delta,
			       (delta > 0) ? start + delta : start,
			       (delta > 0) ? delta : 0);
    }
    textsw_checkpoint(priv);
}

static void textsw_do_undo(Textsw_view_private view)
{
	register Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
	register Ev_finger_handle saved_insert_finger;
	register Ev_chain views = priv->views;
	Ev_mark_object save_insert;

	if (!TXTSW_DO_UNDO(priv))
		return;
	if (priv->undo[0] == es_get(views->esh, ES_UNDO_MARK)) {
		/*
		 * Undo followed immediately by another undo. Note:
		 * textsw_set_internal guarantees that priv->undo_count != 1
		 */
		XV_BCOPY((caddr_t) & priv->undo[1], (caddr_t) & priv->undo[0],
				(size_t)((priv->undo_count - 2) * sizeof(priv->undo[0])));
		priv->undo[priv->undo_count - 1] = ES_NULL_UNDO_MARK;
	}
	if (priv->undo[0] == ES_NULL_UNDO_MARK)
		return;
	/* Undo the changes to the piece source. */
	(void)ev_add_finger(&views->fingers, EV_GET_INSERT(views), 0, &save_insert);

	ev_set(view->e_view, EV_CHAIN_DELAY_UPDATE, TRUE, NULL);
	es_set(views->esh,
			ES_UNDO_NOTIFY_PAIR, textsw_undo_notify, (caddr_t) priv,
			ES_UNDO_MARK, priv->undo[0], NULL);
	ev_set(view->e_view, EV_CHAIN_DELAY_UPDATE, FALSE, NULL);
	ev_update_chain_display(views);
	saved_insert_finger = ev_find_finger(&views->fingers, save_insert);
	if AN_ERROR
		(saved_insert_finger == 0) {
		}
	else {
		(void)textsw_set_insert(priv, saved_insert_finger->pos);
		ev_remove_finger(&views->fingers, save_insert);
	}
	/* Get the new mark. */
	priv->undo[0] = es_get(views->esh, ES_UNDO_MARK);
	/* Check to see if this has undone all edits to the priv. */
	if (textsw_has_been_modified(XV_PUBLIC(priv)) == 0) {
		CHAR *name;

		if (textsw_file_name(priv, &name) == 0) {

#ifdef OW_I18N
			char *name_mb = _xv_wcstombsdup(name);

			textsw_notify(view, TEXTSW_ACTION_LOADED_FILE, name_mb,
					TEXTSW_ACTION_LOADED_FILE_WCS, name, NULL);
			if (name_mb)
				free(name_mb);
#else
			textsw_notify(view, TEXTSW_ACTION_LOADED_FILE, name, NULL);
#endif
		}
		priv->state &= ~TXTSW_EDITED;
		if (textsw_menu_get(priv)
			&& priv->sub_menu_table
			&& priv->sub_menu_table[(int) TXTSW_FILE_SUB_MENU])
			xv_set(priv->sub_menu_table[(int)TXTSW_FILE_SUB_MENU],
					MENU_DEFAULT, 1,
					NULL);

	}
}

Pkg_private void textsw_undo(Textsw_private priv)
{
	Textsw textsw = TEXTSW_PUBLIC(priv);
	Textsw_view firstview = xv_get(textsw, OPENWIN_NTH_VIEW, 0);
	Textsw_view_private fv = VIEW_PRIVATE(firstview);

    textsw_begin_undo(fv);
    textsw_end_undo(fv);
}

static int textsw_scroll_event(Textsw_view_private view, Event *ie, Notify_arg arg)
{
	Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
	int is_scroll_event = FALSE;
	int action = event_action(ie);

	if (action == SCROLLBAR_REQUEST) {
		is_scroll_event = TRUE;

		/*
		 * this does both the scroll and the bar update. The call to
		 * textsw_scroll()
		 */
		/*
		 * was removed, but this breaks the "Last Position" scrollbar logic
		 * !! jcb
		 */
		textsw_scroll((Scrollbar) arg);

		textsw_update_scrollbars(priv, TEXTSW_VIEW_NULL);
	}
	return (is_scroll_event);
}

static int textsw_function_key_event(Textsw_view_private view, Event *ie, int *result)
{
	Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
	int is_function_key_event = FALSE;
	register int action = event_action(ie);
	register int down_event = event_is_down(ie);


	if (action == ACTION_AGAIN) {
		is_function_key_event = TRUE;
		if (down_event) {
			textsw_begin_again(view);
		}
		else if (priv->func_state & TXTSW_FUNC_AGAIN) {
			textsw_end_again(view, ie->ie_locx, ie->ie_locy);
		}

	}

	else if (action == ACTION_UNDO) {
		is_function_key_event = TRUE;
		if (TXTSW_IS_READ_ONLY(priv)) {
			*result |= TEXTSW_PE_READ_ONLY;
		}
		if (down_event) {
			textsw_begin_undo(view);
		}
		else if (priv->func_state & TXTSW_FUNC_UNDO) {
			textsw_end_undo(view);
		}

	}
	else if (action == ACTION_PROPS) {
		is_function_key_event = TRUE;
		if (event_is_up(ie)) {
			textsw_thaw_caret(priv);
		}
		else {
			priv->last_point.tv_sec -= 1000;
			priv->last_adjust = priv->last_point;
			priv->last_ie_time = priv->last_point;
			textsw_freeze_caret(priv);
		}
	}
	else if ((action == ACTION_FRONT) ||
			(action == ACTION_BACK) ||
			(action == ACTION_OPEN) ) {
		is_function_key_event = TRUE;
		/* These key should only work on up event */
		if (!down_event)
			textsw_notify(view, TEXTSW_ACTION_TOOL_MGR, ie, NULL);
	}
	else if ((action == ACTION_FIND_FORWARD) ||
			(action == ACTION_FIND_BACKWARD) ||
			(action == ACTION_REPLACE))
	{
		is_function_key_event = TRUE;

		if (down_event) {
			textsw_begin_find(view);
		}
		else textsw_end_find(view, action, event_x(ie),event_y(ie));
	}
	return (is_function_key_event);
}

static int textsw_mouse_event(Textsw_view_private view, Event *ie)
{
	Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
	int is_mouse_event = TRUE;
	register int action = event_action(ie);
	register int down_event = event_is_down(ie);

	switch (action) {
		case ACTION_SELECT:
			textsw_new_sel_select(view, ie);
			break;
		case ACTION_ADJUST:
			textsw_new_sel_adjust(view, ie);
			break;
		case LOC_DRAG:
			textsw_new_sel_drag(view, ie);
			break;
		case LOC_WINENTER:
			/* This is done for performance improvment */
			priv->state |= TXTSW_DELAY_SEL_INQUIRE;
			break;
		case LOC_WINEXIT:
		case KBD_DONE:
			textsw_may_win_exit(priv);
			break;
			/* Menus */
		case ACTION_MENU:
			if (down_event) {
				textsw_flush_caches(view, TFC_STD);

				textsw_do_menu(view, ie);
			}
			break;
		case ACTION_DRAG_MOVE:

#ifdef OW_I18N
			/* Event is always down event */
			textsw_implicit_commit(priv);
#endif

			textsw_do_remote_drag_copy_move(view, ie, 0);
			break;
		case ACTION_DRAG_COPY:

#ifdef OW_I18N
			/* Event is always down event */
			textsw_implicit_commit(priv);
#endif

			textsw_do_remote_drag_copy_move(view, ie, 1);
			break;
		default:
			is_mouse_event = FALSE;
			break;
	}

	return (is_mouse_event);
}

static int textsw_edit_function_key_event(Textsw_view_private view, Event *ie, int *result)
{
	Textsw_private priv = TSWPRIV_FOR_VIEWPRIV(view);
	int is_edit_function_key_event = FALSE;
	register int action = event_action(ie);
	register int down_event = event_is_down(ie);

	/*
	 *  BIG BUG:  should check for conversion on.
	 */

	if (action == ACTION_CUT) {
		is_edit_function_key_event = TRUE;
		if (event_is_up(ie)) {
			textsw_thaw_caret(priv);
			textsw_new_sel_cut(view, ie, TRUE);
		}
		else {
			textsw_freeze_caret(priv);
		}
	}
	else if (action == ACTION_PASTE) {
		is_edit_function_key_event = TRUE;
		if (event_is_up(ie)) {
			textsw_thaw_caret(priv);
			textsw_new_sel_paste(view, ie, TRUE);
		}
		else {
			/* das macht textsw_init_timer(priv) : */
			priv->last_point.tv_sec -= 1000;
			priv->last_adjust = priv->last_point;
			priv->last_ie_time = priv->last_point;
			textsw_freeze_caret(priv);
		}
	}
	else if (action == ACTION_COPY) {
		is_edit_function_key_event = TRUE;
		if (event_is_up(ie)) textsw_new_sel_copy(view, ie);
	}
	else if (action == ACTION_PASTE_PRIMARY) {
		Es_index ro_bdry, pos, temp;

		is_edit_function_key_event = TRUE;
		if (down_event) {
			Textsw tsw = TEXTSW_PUBLIC(priv);
			char *string;
			int format;
			long length;

			SERVERTRACE((725, "ACTION_PASTE_PRIMARY\n"));
			if (! view->selreq) {
				view->selreq = xv_create(tsw, SELECTION_REQUESTOR, NULL);
			}

			xv_set(view->selreq,
					SEL_RANK, XA_PRIMARY,
					SEL_TIME, &event_time(ie),
					SEL_TYPE, XA_STRING,
					SEL_AUTO_COLLECT_INCR, TRUE,
					NULL);
			string = (char *)xv_get(view->selreq,SEL_DATA, &length, &format);

			if (length == SEL_ERROR) {
				if (string) xv_free(string);
				xv_set(xv_get(tsw, WIN_FRAME), 
						FRAME_LEFT_FOOTER, XV_MSG("Primary paste failed"),
						WIN_ALARM,
						NULL);
			}
			else {
				/* ACTION_PASTE_PRIMARY is a mouse event and has coordinates -
				 * basicaklly stolen from textsw_do_remote_drag_copy_move:
				 */
				ev_set(view->e_view, EV_CHAIN_DELAY_UPDATE, TRUE, NULL);
				ro_bdry = textsw_read_only_boundary_is_at(priv);

				if (ro_bdry + 1 == ES_INFINITY)
					ro_bdry = 0;

				pos = ev_resolve_xy(view->e_view, event_x(ie), event_y(ie));
				if (pos < ro_bdry) {
					Es_index insert;

					insert = EV_GET_INSERT(priv->views);
					if (insert >= ro_bdry)
						pos = insert;
					else {
						xv_free(string);
						return XV_OK;
					}
				}

				/* make sure the click doesn't happen in read-only text */
				if (TXTSW_IS_READ_ONLY(priv)) {
					textsw_read_only_msg(priv, event_x(ie), event_y(ie));
					xv_free(string);
					return XV_OK;
				}

				ev_set(view->e_view, EV_CHAIN_DELAY_UPDATE, FALSE, NULL);
				EV_SET_INSERT(priv->views, pos, temp);
				length = strlen(string);
				textsw_do_input(view, string, length, TXTSW_UPDATE_SCROLLBAR);
				TEXTSW_DO_INSERT_MAKES_VISIBLE(view);
				xv_free(string);
			}
		}
	}

	return (is_edit_function_key_event);
}

static int textsw_caret_motion_event(Textsw_view_private view, Event *ie)
{
	int is_caret_motion_event = TRUE;
	register int action = event_action(ie);

	if (event_is_up(ie))
		return (FALSE);

	switch (action) {
		case ACTION_GO_CHAR_FORWARD:

#ifdef OW_I18N
			textsw_implicit_commit(TSWPRIV_FOR_VIEWPRIV(view));
#endif

			textsw_move_caret(view, TXTSW_CHAR_FORWARD);
			break;
		case ACTION_GO_CHAR_BACKWARD:

#ifdef OW_I18N
			textsw_implicit_commit(TSWPRIV_FOR_VIEWPRIV(view));
#endif

			textsw_move_caret(view, TXTSW_CHAR_BACKWARD);
			break;
		case ACTION_GO_COLUMN_BACKWARD:

#ifdef OW_I18N
			textsw_implicit_commit(TSWPRIV_FOR_VIEWPRIV(view));
#endif

			textsw_move_caret(view, TXTSW_PREVIOUS_LINE);
			break;
		case ACTION_GO_COLUMN_FORWARD:

#ifdef OW_I18N
			textsw_implicit_commit(TSWPRIV_FOR_VIEWPRIV(view));
#endif

			textsw_move_caret(view, TXTSW_NEXT_LINE);
			break;
		case ACTION_GO_WORD_BACKWARD:

#ifdef OW_I18N
			textsw_implicit_commit(TSWPRIV_FOR_VIEWPRIV(view));
#endif

			textsw_move_caret(view, TXTSW_WORD_BACKWARD);
			break;
		case ACTION_GO_WORD_FORWARD:

#ifdef OW_I18N
			textsw_implicit_commit(TSWPRIV_FOR_VIEWPRIV(view));
#endif

			textsw_move_caret(view, TXTSW_WORD_FORWARD);
			break;
		case ACTION_GO_WORD_END:

#ifdef OW_I18N
			textsw_implicit_commit(TSWPRIV_FOR_VIEWPRIV(view));
#endif

			textsw_move_caret(view, TXTSW_WORD_END);
			break;
		case ACTION_GO_LINE_BACKWARD:

#ifdef OW_I18N
			textsw_implicit_commit(TSWPRIV_FOR_VIEWPRIV(view));
#endif

			textsw_move_caret(view, TXTSW_LINE_START);
			break;
		case ACTION_GO_LINE_END:

#ifdef OW_I18N
			textsw_implicit_commit(TSWPRIV_FOR_VIEWPRIV(view));
#endif

			textsw_move_caret(view, TXTSW_LINE_END);
			break;
		case ACTION_GO_LINE_FORWARD:

#ifdef OW_I18N
			textsw_implicit_commit(TSWPRIV_FOR_VIEWPRIV(view));
#endif

			textsw_move_caret(view, TXTSW_NEXT_LINE_START);
			break;
		case ACTION_GO_DOCUMENT_START:

#ifdef OW_I18N
			textsw_implicit_commit(TSWPRIV_FOR_VIEWPRIV(view));
#endif

			textsw_move_caret(view, TXTSW_DOCUMENT_START);
			break;
		case ACTION_GO_DOCUMENT_END:

#ifdef OW_I18N
			textsw_implicit_commit(TSWPRIV_FOR_VIEWPRIV(view));
#endif

			textsw_move_caret(view, TXTSW_DOCUMENT_END);
			break;
		default:
			is_caret_motion_event = FALSE;
			break;

	}
	if (is_caret_motion_event)
		textsw_set_selection(xv_get(VIEW_PUBLIC(view), XV_OWNER),
				ES_INFINITY, ES_INFINITY, EV_SEL_PRIMARY);


	return (is_caret_motion_event);
}

static int textsw_field_event(Textsw_view_private view, Event *ie)
{
    register int    action = event_action(ie);
    register int    down_event = event_is_down(ie);
    int             is_field_event = FALSE;

    if (action == ACTION_SELECT_FIELD_FORWARD) {
#ifdef OW_I18N
	static CHAR bar_gt[] = { '|', '>', 0 };
#endif
	is_field_event = TRUE;

	if (down_event) {
	    textsw_flush_caches(view, TFC_STD);
#ifdef OW_I18N
	    textsw_match_selection_and_normalize(view, bar_gt,
#else
	    textsw_match_selection_and_normalize(view, "|>",
#endif
						      TEXTSW_FIELD_FORWARD);
	}
    } else if (action == ACTION_SELECT_FIELD_BACKWARD) {
#ifdef OW_I18N
	static CHAR bar_lt[] = { '<', '|',  0 };
#endif
	is_field_event = TRUE;

	if (down_event) {
	    textsw_flush_caches(view, TFC_STD);
#ifdef OW_I18N
	    textsw_match_selection_and_normalize(view, bar_lt,
#else
	    textsw_match_selection_and_normalize(view, "<|",
#endif
						     TEXTSW_FIELD_BACKWARD);
	}
    } else if (action == ACTION_MATCH_DELIMITER) {
	CHAR           *start_marker = NULL;

	is_field_event = TRUE;
	if (down_event) {
	    textsw_flush_caches(view, TFC_STD);
	    textsw_match_selection_and_normalize(view, start_marker,
							TEXTSW_NOT_A_FIELD);
	}
    }
    return (is_field_event);
}


static int textsw_file_operation(Textsw abstract, Event *ie)
{
	int is_file_op_event = FALSE;
	register int action = event_action(ie);
	register int down_event = event_is_down(ie);
	Frame frame;
	Xv_Notice text_notice;
    Textsw_private priv;
    Textsw_view_private view;

	/* hier kann TEXTSW oder TERMSW kommen */
	if (xv_get(abstract, XV_IS_SUBTYPE_OF, OPENWIN)) {
		priv = TEXTSW_PRIVATE(abstract);
		view = VIEW_PRIVATE(xv_get(abstract, OPENWIN_NTH_VIEW, 0));
	}
	else {
		view = VIEW_ABS_TO_REP(abstract);
		priv = TSWPRIV_FOR_VIEWPRIV(view);
		abort();
	}

	if (action == ACTION_LOAD) {
		Pkg_private int LOAD_FILE_POPUP_KEY;
		Frame base_frame, popup;

		is_file_op_event = TRUE;
		if (down_event) {
			if (priv->state & TXTSW_NO_LOAD) {
				frame = xv_get(TEXTSW_PUBLIC(priv), WIN_FRAME);
				text_notice = xv_get(frame, XV_KEY_DATA, text_notice_key);
				if (!text_notice) {
					text_notice = xv_create(frame, NOTICE, NULL);

					xv_set(frame,
							XV_KEY_DATA, text_notice_key, text_notice, NULL);
				}
				xv_set(text_notice,
						NOTICE_MESSAGE_STRINGS,
							XV_MSG("Illegal Operation.\n\
Load File has been disabled."),
							NULL,
						NOTICE_BUTTON_YES, XV_MSG("Continue"),
						NOTICE_BUSY_FRAMES, frame, NULL,
						XV_SHOW, TRUE,
						NULL);

				return is_file_op_event;
			}
			base_frame = (Frame) xv_get(abstract, WIN_FRAME);
			popup = (Frame) xv_get(base_frame, XV_KEY_DATA,
					LOAD_FILE_POPUP_KEY);
			if (popup) {
				textsw_set_dir_str((int)TEXTSW_MENU_LOAD);
				textsw_get_and_set_selection(popup, view,
						(int)TEXTSW_MENU_LOAD);
			}
			else {
				textsw_create_popup_frame(view, (int)TEXTSW_MENU_LOAD);
			}
		}
		is_file_op_event = TRUE;

	}
	else if (action == ACTION_STORE) {
		if (down_event) {
			textsw_do_save(abstract, priv, view);
		}
		is_file_op_event = TRUE;

	}
	else if (action == ACTION_INCLUDE_FILE) {
		int primary_selection_exists;
		Pkg_private int FILE_STUFF_POPUP_KEY;
		Frame base_frame, popup;

		if (down_event) {
			primary_selection_exists = textsw_is_seln_nonzero(priv,
					EV_SEL_PRIMARY);
			if (primary_selection_exists) {
				base_frame = (Frame) xv_get(abstract, WIN_FRAME);
				popup = (Frame) xv_get(base_frame, XV_KEY_DATA,
						FILE_STUFF_POPUP_KEY);
				if (popup) {
					(void)textsw_set_dir_str((int)TEXTSW_MENU_FILE_STUFF);
					(void)textsw_get_and_set_selection(popup, view,
							(int)TEXTSW_MENU_FILE_STUFF);
				}
				else {
					(void)textsw_create_popup_frame(view,
							(int)TEXTSW_MENU_FILE_STUFF);
				}
			}
			else {
				(void)textsw_post_need_selection(abstract, ie);
			}
		}
		is_file_op_event = TRUE;
	}
	return (is_file_op_event);
}

static int textsw_erase_action(Textsw_view win, Event *ie)
{
	unsigned edit_unit;
	int direction = 0 /* forward direction */ ;
	register int action = event_action(ie);
	int result;	/* for EDIT_CHAR deletes selection */
	Es_index first, lpo;
    Textsw_private priv;
    Textsw_view_private view;

	if (xv_get(win, XV_IS_SUBTYPE_OF, OPENWIN_VIEW)) {
		view = VIEW_PRIVATE(win);
		priv = TEXTSW_PRIVATE(xv_get(win, XV_OWNER));
	}
	/* hier kann TEXTSW oder TERMSW kommen */
	else if (xv_get(win, XV_IS_SUBTYPE_OF, OPENWIN)) {
		priv = TEXTSW_PRIVATE(win);
		view = VIEW_PRIVATE(xv_get(win, OPENWIN_NTH_VIEW, 0));
		abort();
	}
	else {
		view = VIEW_ABS_TO_REP(win);
		priv = TSWPRIV_FOR_VIEWPRIV(view);
		abort();
	}

	switch (action) {
		case ACTION_ERASE_CHAR_BACKWARD:
			edit_unit = EV_EDIT_CHAR;
			direction = EV_EDIT_BACK;
			break;
		case ACTION_ERASE_CHAR_FORWARD:
			edit_unit = EV_EDIT_CHAR;
			break;
		case ACTION_ERASE_WORD_BACKWARD:
			edit_unit = EV_EDIT_WORD;
			direction = EV_EDIT_BACK;
			break;
		case ACTION_ERASE_WORD_FORWARD:
			edit_unit = EV_EDIT_WORD;
			break;
		case ACTION_ERASE_LINE_BACKWARD:
			edit_unit = EV_EDIT_LINE;
			direction = EV_EDIT_BACK;
			break;
		case ACTION_ERASE_LINE_END:
			edit_unit = EV_EDIT_LINE;
			break;
		default:
			edit_unit = 0;
	}

	if (edit_unit == 0) {
		int id = event_id(ie);

		if (id == (int)priv->edit_bk_char) {
			edit_unit = EV_EDIT_CHAR;
			if ((priv->state & TXTSW_SHIFT_DOWN) == 0)
				direction = EV_EDIT_BACK;
		}
		else if (id == (int)priv->edit_bk_word) {
			edit_unit = EV_EDIT_WORD;
			if ((priv->state & TXTSW_SHIFT_DOWN) == 0)
				direction = EV_EDIT_BACK;
		}
		else if (id == (int)priv->edit_bk_line) {
			edit_unit = EV_EDIT_LINE;
			if ((priv->state & TXTSW_SHIFT_DOWN) == 0)
				direction = EV_EDIT_BACK;
		}
	}
	if ((edit_unit == 0) || (TXTSW_IS_READ_ONLY(priv)) || (event_is_up(ie)))
		return (edit_unit);
	/* Delete selection only for single character delete.
	   1) Delete the selection for single character edits only if
	   there is a primary selection.
	   2) If the delete is not a single character delete, then
	   clear any primary selections and do the erase action.
	   3) Make sure view and scrollbar are updated.
	 */

	result = ev_get_selection(priv->views, &first, &lpo, EV_SEL_PRIMARY);

	/* Delete the selection for single character edits. */
	if ((edit_unit == EV_EDIT_CHAR) &&	/* single character */
			((EV_SEL_PRIMARY & result) || (EV_SEL_PD_PRIMARY & result)) && (first < lpo)) {	/* selection has a valid range */
		textsw_flush_caches(view, TFC_DO_PD);
		/* this makes the selected text actually disappear */
		ev_update_chain_display(priv->views);
	}
	else {	/* don't delete the selection. Do the erase action */
		/* clear any primary selections */
		if (((EV_SEL_PRIMARY & result) || (EV_SEL_PD_PRIMARY & result)) &&
				(first < lpo)) {
			textsw_set_selection(TEXTSW_PUBLIC(priv),
					ES_INFINITY, ES_INFINITY, EV_SEL_PRIMARY);
			/* this makes the selected text actually disappear */
			ev_update_chain_display(priv->views);
		}
		textsw_flush_caches(view, TFC_INSERT | TFC_PD_IFF_INSERT | TFC_SEL);

#ifdef OW_I18N
		(void)textsw_do_edit(view, edit_unit, direction, 0);
#else
		(void)textsw_do_edit(view, edit_unit, (unsigned)direction);
#endif
	}
	/* scrollbar updating needed esp. for large deletes */
	textsw_update_scrollbars(priv, TEXTSW_VIEW_NULL);
	return (TRUE);
}

#ifdef OW_I18N
#define	TXT_INSERT_LEN(x) ((TXTSW_UI_BUFLEN - \
			    ((x)->to_insert_next_free - (x)->to_insert)) - 1)
/*
 * textsw package interface to archive implicit commit.
 * Assumption:  1. IC is valid
 *		2. conversion mode is ON
 *		3. preedit text exist.
 *   So, textsw_implicit_commit_doit() has to be called when
 *   priv->preedit_start is non NULL case.
 */
Pkg_private	void textsw_implicit_commit_doit(register Textsw_private	priv)
{
    register Textsw		textsw_public = TEXTSW_PUBLIC(priv);
    char		       *committed_string = 0;

    xv_set(textsw_public, WIN_IC_RESET, NULL);
    committed_string = (char *)xv_get(textsw_public, WIN_IC_COMMIT_STRING);
    xv_set(textsw_public, WIN_IC_CONVERSION, TRUE, NULL);

    /*
     *  Workaroud for 1092682 and 1094340
     *  If preedit_draw() for erasing the preedit text (after now call it as
     *  preedit_erasing) isn't called yet, call it in order for any processing
     *  after textsw_implicit_commit_doit(). The real preedit_erasing sent by
     *  mle (Htt) will process nothing.
     *  This is a hack for such mle. mle should send the preedit_erasing before
     *  the committed text, when IC is reseted.
     */
    if (priv->preedit_start) { /* preedit text dosen't erase yet. */
	XIMPreeditDrawCallbackStruct	callback_data;

	callback_data.chg_first = 0;
	callback_data.chg_length = EV_GET_INSERT(priv->views) -
			textsw_find_mark_internal(priv, priv->preedit_start);
	callback_data.text = (XIMText *)0;
	textsw_pre_edit_draw(priv->ic, (XPointer)TEXTSW_PUBLIC(priv),
			     &callback_data);
    }

    /* insert committed text into window if exists */
    if (committed_string != (char *)NULL && *committed_string != '\0') {
	int	num_of_char = mbstowcs(priv->to_insert_next_free,
				       committed_string,
				       TXT_INSERT_LEN(priv));
	priv->to_insert_next_free += num_of_char;
	textsw_flush_caches(VIEW_PRIVATE(xv_get(XV_PUBLIC(priv), OPENWIN_NTH_VIEW, 0)), TFC_STD);

	if ((priv->state & TXTSW_EDITED) == 0)
	    textsw_possibly_edited_now_notify(priv);
    }
}
#endif /* OW_I18N */
