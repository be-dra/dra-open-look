#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)fm_input.c 20.59 93/06/28 DRA: $Id: fm_input.c,v 4.16 2025/01/18 22:33:25 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL_NOTICE 
 *	file for terms of the license.
 */

#include <xview_private/fm_impl.h>
#include <xview_private/draw_impl.h>
#include <xview_private/svr_impl.h>
#include <xview_private/win_info.h>
#include <xview_private/frame_base.h>
#include <xview_private/xv_quick.h>
#include <xview/notice.h>
#include <xview/help.h>
#include <xview/defaults.h>
#include <xview/sel_pkg.h>
#include <xview/font.h>

/* Returns XV_OK or XV_ERROR */
static int frame_set_focus(Xv_Window sw)
	/* frame subwindow to receive kbd focus */
{
    /* If the subwindow already has the input focus, then don't
     * set the focus on it.  Case in point:  In Click-to-type mode,
     * the second view in a textsw subwindow has the input focus.
     * Clicking SELECT in the window header will generate an
     * ACTION_TAKE_FOCUS event.  If we we're to set WIN_SET_FOCUS on
     * the textsw, the input focus would change to the first view
     * in the textsw.
     */
    if (xv_get(sw, WIN_KBD_FOCUS) == (Xv_opaque) TRUE)
	return XV_OK;

    /* The subwindow doesn't have the input focus, so set the input
     * focus to the subwindow.
     */
    if (xv_set(sw, WIN_SET_FOCUS, NULL) == XV_OK)
	return XV_OK;

    return XV_ERROR;
}


Pkg_private Notify_value frame_input(Frame frame_public, Event *event,
						Notify_arg arg, Notify_event_type type)
{
	Frame_class_info *frame = FRAME_CLASS_PRIVATE(frame_public);
	unsigned int action = event_action(event);
	char *help_data;

	/* Tell the selection service about GET, PUT, FIND, DELETE */

	switch (action) {
		case ACTION_COPY:
		case ACTION_PASTE:
		case ACTION_CUT:
		case ACTION_FIND_FORWARD:
		case ACTION_FIND_BACKWARD:
			fprintf(stderr, "\n\n%s-%d: WE REALLY COME HERE !!!\n\n\n",
						__FILE__, __LINE__);
/* 			selection report event(XV_SERVER_FROM_WINDOW(frame_public), */
/* 							(Seln_client) frame_public, event); */
			break;
		default:
			break;
	}
	switch (action) {
		case ACTION_CUT:
			/* only want up of function keys */
			if (event_is_down(event))
				goto Done;
			break;

		case ACTION_HELP:
		case ACTION_MORE_HELP:
		case ACTION_TEXT_HELP:
		case ACTION_MORE_TEXT_HELP:
		case ACTION_INPUT_FOCUS_HELP:
			if (event_is_down(event)) {
				help_data = (char *)xv_get(frame_public, XV_HELP_DATA);
				if (help_data) {
					xv_help_show(frame_public, help_data, event);
				}
			}
			return NOTIFY_DONE;

		case ACTION_RESCALE:
			SERVERTRACE((790, "rescale to %ld\n", arg));
			frame_rescale_subwindows(frame_public, (int)arg);
			goto Done;

		case WIN_RESIZE:

			(void)win_getsize(frame_public, &frame->rectcache);

			/* Set width and height size hints for backwards 
			 * compatibility with pre-ICCCM window managers  */
			if (!defaults_get_boolean("xview.icccmcompliant",
							"XView.ICCCMCompliant", TRUE)) {
				XSizeHints sizeHints;
				Xv_Drawable_info *info;

				DRAWABLE_INFO_MACRO(frame_public, info);
				sizeHints.flags = PSize;
				sizeHints.width = frame->rectcache.r_width;
				sizeHints.height = frame->rectcache.r_height;
				XSetNormalHints(xv_display(info), xv_xid(info), &sizeHints);
			}

#ifdef OW_I18N
			if (status_get(frame, show_imstatus)) {
				if (status_get(frame, show_footer))
					xv_set(frame->imstatus,
							XV_WIDTH, frame->rectcache.r_width,
							XV_Y, frame->rectcache.r_height -
							xv_get(frame->footer, XV_HEIGHT) -
							xv_get(frame->imstatus, XV_HEIGHT), NULL);
				else
					xv_set(frame->imstatus,
							XV_WIDTH, frame->rectcache.r_width,
							XV_Y, frame->rectcache.r_height -
							xv_get(frame->imstatus, XV_HEIGHT), NULL);

			}
#endif

			if (status_get(frame, show_footer)) {
				xv_set(frame->footer,
						XV_WIDTH, frame->rectcache.r_width,
						XV_Y, frame->rectcache.r_height -
						xv_get(frame->footer, XV_HEIGHT), NULL);
			}
			(void)frame_layout_subwindows(frame_public);
			goto Done;

		case ACTION_TAKE_FOCUS:
			{
				Xv_Window child;
				int subw_found = FALSE;

				/*
				 * Check if empty frame i.e. no subwindows
				 * If yes, set focus to the frame
				 * - ICONs don't really count as subwindows
				 */
				FRAME_EACH_CHILD(frame->first_subwindow, child)
					if (!xv_get(child, XV_IS_SUBTYPE_OF, ICON)) {
						subw_found = TRUE;
						break;
					}
				FRAME_END_EACH
				if (!subw_found) {
					if (frame_set_focus(frame_public) == XV_OK) {
						goto Done;
					}
				}

				/* Set the input focus to the last primary focus subwindow
				 * that had the input focus.  If no primary focus subwindow
				 * ever had the input focus, then set the input focus to the
				 * first primary focus subwindow.  If there are no primary
				 * focus subwindows, then restore the input focus to the last
				 * subwindow that had the input focus.  If no subwindow has
				 * ever had the input focus, set the input focus to the
				 * first subwindow that can take the input focus.
				 * So, in summary, here are the priorities:
				 *    1. Most recent primary focus subwindow
				 *    2. First primary focus subwindow
				 *    3. Most recent (ordinary) focus subwindow
				 *    4. First (ordinary) focus subwindow
				 */

				/* Priority 1: Most recent primary focus subwindow */
				if (frame->primary_focus_sw &&
						frame_set_focus(frame->primary_focus_sw) == XV_OK)
					goto Done;

				/* Priority 2: First primary focus subwindow */
				FRAME_EACH_CHILD(frame->first_subwindow, child)
					if (xv_get(child, XV_FOCUS_RANK) == XV_FOCUS_PRIMARY &&
						frame_set_focus(child) == XV_OK)
					goto Done;
				FRAME_END_EACH

				/* Priority 3: Most recent ordinary focus subwindow */
				if (frame->focus_subwindow &&
					frame_set_focus(frame->focus_subwindow) == XV_OK)
					goto Done;

				/* Priority 4: First ordinary focus subwindow.
				 *
				 * Go through the list of subwindows and make sure none of them
				 * have the focus, then find the first subwindow that can
				 * accept keyboard input and assign input focus to it.
				 */
				FRAME_EACH_CHILD(frame->first_subwindow, child)
					if (xv_get(child, WIN_KBD_FOCUS) == (Xv_opaque) TRUE)
						goto Done;
				FRAME_END_EACH

				FRAME_EACH_CHILD(frame->first_subwindow, child)
					if (xv_set(child, WIN_SET_FOCUS, NULL) == XV_OK)
						goto Done;
				FRAME_END_EACH

				/*
				 * If got this far, focus was not set.
				 * Check if frame should take the default focus.
				 * Done only when:
				 *      accelerators are present
				 *      accept_default_focus flag set
				 */
				if (frame->menu_accelerators || frame->accelerators ||
						status_get(frame, accept_default_focus)) {
					if (frame_set_focus(frame_public) == XV_OK) {
						goto Done;
					}
				}
			}
			break;

		default:
			/* Cannot ignore up events. Forward it */
			if (event_is_up(event))
				goto Done;
			break;
	}

	switch (action) {

		case ACTION_OPEN:
			if (status_get(frame, map_state_change)) {
				status_set(frame, map_state_change, FALSE);
			}
			else {
				status_set(frame, iconic, FALSE);
				status_set(frame, initial_state, FALSE);
			}
			break;

		case ACTION_CLOSE:
			if (status_get(frame, map_state_change)) {
				status_set(frame, map_state_change, FALSE);
			}
			else {
				status_set(frame, iconic, TRUE);
				status_set(frame, initial_state, TRUE);
			}
			break;

		case ACTION_DISMISS:
			status_set(frame, dismiss, TRUE);
			if (frame->done_proc)
				(void)(frame->done_proc) (frame_public);
			else
				(void)(frame->default_done_proc) (frame_public);
			break;

		case ACTION_PROPS:
			frame_handle_props(frame_public);
			break;

		case ACTION_PININ:	/* reset dismiss state, if in it */
			status_set(frame, dismiss, FALSE);
			break;
	}

  Done:
	return notify_next_event_func((Notify_client) frame_public,
			(Notify_event) event, arg, type);
}

/*ARGSUSED*/
Pkg_private void frame_focus_win_event_proc(Xv_Window window, Event *event, Notify_arg arg)
{
    Frame_focus_direction focus_direction;
    GC		    gc;		/* Graphics Context */
    XGCValues	    gc_values;
    int		    height;
    Server_image    image;
    Xv_Drawable_info *image_info;
    Xv_Drawable_info *info;
    int		    width;

    if (event_action(event) == WIN_REPAINT) {
	focus_direction = (Frame_focus_direction) xv_get(window,
	    XV_KEY_DATA, FRAME_FOCUS_DIRECTION);
	if (focus_direction == FRAME_FOCUS_UP) {
	    image = xv_get(window, XV_KEY_DATA, FRAME_FOCUS_UP_IMAGE);
	    width = FRAME_FOCUS_UP_WIDTH;
	    height = FRAME_FOCUS_UP_HEIGHT;
	} else {
	    image = xv_get(window, XV_KEY_DATA, FRAME_FOCUS_RIGHT_IMAGE);
	    width = FRAME_FOCUS_RIGHT_WIDTH;
	    height = FRAME_FOCUS_RIGHT_HEIGHT;
	}
	DRAWABLE_INFO_MACRO(window, info);
	gc = (GC) xv_get(window, XV_KEY_DATA, FRAME_FOCUS_GC);
	if (!gc) {
	    /* Create the Graphics Context for the Focus Window */
	    /* THIS IS ALSO DONE IN notice_show_focus_win() in notice_pt.c */
	    gc_values.fill_style = FillOpaqueStippled;
	    gc = XCreateGC(xv_display(info), xv_xid(info), GCFillStyle,
			   &gc_values);
	    xv_set(window, XV_KEY_DATA, FRAME_FOCUS_GC, gc, NULL);
	}
	DRAWABLE_INFO_MACRO(image, image_info);
	gc_values.background = xv_bg(info);
	gc_values.foreground = xv_fg(info);
	gc_values.stipple = xv_xid(image_info);
	XChangeGC(xv_display(info), gc, GCForeground | GCBackground | GCStipple,
		  &gc_values);
	XFillRectangle(xv_display(info), xv_xid(info), gc, 0, 0,
		       (unsigned)width, (unsigned)height);
    }
}

#ifdef OW_I18N
Pkg_private     Notify_value
frame_IMstatus_input(IMstatus, event, arg, type)
    Xv_Window       IMstatus;
    Event          *event;
    Notify_arg      arg;
    Notify_event_type type;
{
    switch (event_action(event)) {
      case WIN_REPAINT:
	frame_display_IMstatus(xv_get(IMstatus, WIN_PARENT), FALSE);
	break;
      default:
	break;
    }
    return notify_next_event_func(IMstatus, (Notify_event)event, arg, type);
}
#endif

/*
 * XView private function to set the accept_default_focus bit
 * So far, it is used by notices, acceleators.
 */
Xv_private	void frame_set_accept_default_focus(Frame frame_public, int flag)
{
    Frame_class_info *frame = FRAME_CLASS_PRIVATE(frame_public);

    status_set(frame, accept_default_focus, flag);
}

static Attr_attribute quick_dupl_key = 0;

typedef struct {
	quick_common_data_t qc;
	Frame_class_info *priv; /* always the same */
	int is_left; /* boolean: determined when ACTION_SELECT down */
	int multiclick_timeout;
} quick_data_t;

static int note_quick_convert(Selection_owner sel_own, Atom *type,
					Xv_opaque *data, unsigned long *length, int *format)
{
	quick_data_t *qd = (quick_data_t *)xv_get(sel_own, XV_KEY_DATA,
												quick_dupl_key);

	if (qd->qc.seltext[0] == '\0') {
		Frame_class_info *priv = qd->priv;
		char *s;

		/* now we determine the selected string */
		if (qd->is_left) {
			s = priv->left_footer;
		}
		else {
			s = priv->right_footer;
		}
		if (s) {
			strncpy(qd->qc.seltext, s + qd->qc.startindex,
						(size_t)(qd->qc.endindex - qd->qc.startindex + 1));
			qd->qc.seltext[qd->qc.endindex - qd->qc.startindex + 1] = '\0';
		}
	}

	return xvq_note_quick_convert(sel_own, &qd->qc, type, data, length, format);
}

static void note_quick_lose(Selection_owner sel_owner)
{
	quick_data_t *qd = (quick_data_t *)xv_get(sel_owner, XV_KEY_DATA,
												quick_dupl_key);

	qd->qc.startindex = qd->qc.endindex = 0;
	qd->qc.reply_data = 0;
	qd->qc.seltext[0] = '\0';
	qd->qc.select_click_cnt = 0;
	frame_display_footer(xv_get(xv_get(sel_owner, XV_OWNER), XV_OWNER), TRUE);
}

static int update_secondary(Xv_Window footer, int mx)
{
	quick_data_t *qd;
	Xv_Drawable_info *info;
	char *s;
	int len, i;
	Frame_class_info *priv;

	qd = (quick_data_t *)xv_get(footer, XV_KEY_DATA, quick_dupl_key);
	if (! qd) return FALSE;

	priv = qd->priv;

	DRAWABLE_INFO_MACRO(footer, info);

	XDrawLine(xv_display(info), xv_xid(info), qd->qc.gc,
				qd->qc.startx, qd->qc.baseline, qd->qc.endx, qd->qc.baseline);
	qd->qc.endx = 0;

	if (qd->is_left) {
		s = priv->left_footer;
	}
	else {
		s = priv->right_footer;
	}

	if (s) {
		int *xp = qd->qc.xpos;
		len = strlen(s);

		for (i = 0; i < len; i++) {
			if (mx >= xp[i] && mx < xp[i+1]) {
				qd->qc.endx = xp[i+1];
				qd->qc.endindex = i;
				break;
			}
		}
	}
	if (!qd->qc.endx) {
		qd->qc.endx = qd->qc.startx;
	}

	XDrawLine(xv_display(info), xv_xid(info), qd->qc.gc,
				qd->qc.startx, qd->qc.baseline, qd->qc.endx, qd->qc.baseline);

	return TRUE;
}

static XFontStruct *get_fontstruct(Frame_class_info *priv)
{
	return priv->ginfo->textfont;
}

static int adjust_secondary(quick_data_t *qd, Event *ev)
{
	Frame_class_info *priv;
	XFontStruct *fs;
	int sx;
	char *s;

	priv = qd->priv;

	fs = get_fontstruct(priv);
	if (qd->is_left) {
		s = priv->left_footer;
		sx = priv->left_x;
	}
	else {
		s = priv->right_footer;
		sx = priv->right_x;
	}

	xvq_adjust_secondary(&qd->qc, fs, ev, s, sx);
	return TRUE;
}

static int adjust_wordwise(quick_data_t *qd, Event *ev)
{
	Frame_class_info *priv = qd->priv;
	char *s;
	int sx;
	XFontStruct *fs;

	fs = get_fontstruct(priv);
	if (qd->is_left) {
		s = priv->left_footer;
		sx = priv->left_x;
	}
	else {
		s = priv->right_footer;
		sx = priv->right_x;
	}

	return xvq_adjust_wordwise(&qd->qc, fs, s, sx, ev);
}

static quick_data_t *supply_quick_data(Xv_window footer, Frame_class_info *priv)
{
	quick_data_t *qd;
	XGCValues   gcv;
	Xv_Drawable_info *info;
	Cms cms;
	int i, fore_index, back_index;
	unsigned long fg, bg;
	char *delims, delim_chars[256];	/* delimiter characters */

	qd = xv_alloc(quick_data_t);
	xv_set(footer, XV_KEY_DATA, quick_dupl_key, qd, NULL);

	qd->qc.sel_owner = xv_create(footer, SELECTION_OWNER,
					SEL_RANK, XA_SECONDARY,
					SEL_CONVERT_PROC, note_quick_convert,
					SEL_LOSE_PROC, note_quick_lose,
					XV_KEY_DATA, quick_dupl_key, qd,
					NULL);

	qd->priv = priv;

	cms = xv_get(footer, WIN_CMS);
	fore_index = (int)xv_get(footer, WIN_FOREGROUND_COLOR);
	back_index = (int)xv_get(footer, WIN_BACKGROUND_COLOR);
	bg = (unsigned long)xv_get(cms, CMS_PIXEL, back_index);
	fg = (unsigned long)xv_get(cms, CMS_PIXEL, fore_index);

	gcv.foreground = fg ^ bg;
	gcv.function = GXxor;

	DRAWABLE_INFO_MACRO(footer, info);

	qd->qc.gc = XCreateGC(xv_display(info), xv_xid(info),
					GCForeground | GCFunction, &gcv);
	qd->qc.baseline = (int)xv_get(footer, XV_HEIGHT) - 2;

	qd->multiclick_timeout = 100 *
	    defaults_get_integer_check("openWindows.multiClickTimeout",
		                   "OpenWindows.MultiClickTimeout", 4, 2, 10);

	delims = (char *)defaults_get_string("text.delimiterChars",
			"Text.DelimiterChars", " \t,.:;?!\'\"`*/-+=(){}[]<>\\|~@#$%^&");
	/* Print the string into an array to parse the potential
	 * octal/special characters.
	 */
	strcpy(delim_chars, delims);
	/* Mark off the delimiters specified */
	for (i = 0; i < sizeof(qd->qc.delimtab); i++) qd->qc.delimtab[i] = FALSE;
	for (delims = delim_chars; *delims; delims++) {
		qd->qc.delimtab[(int)*delims] = TRUE;
	}

	return qd;
}

static int determine_multiclick(int multitime,
            struct timeval *last_click_time, struct timeval *new_click_time)
{
	int delta;

	if (last_click_time->tv_sec == 0 && last_click_time->tv_usec == 0)
		return FALSE;
	delta = (new_click_time->tv_sec - last_click_time->tv_sec) * 1000;
	delta += new_click_time->tv_usec / 1000;
	delta -= last_click_time->tv_usec / 1000;
	return (delta <= multitime);
}   

static int is_quick_duplicate_on_footer(Xv_window footer, Event *ev)
{
	quick_data_t *qd;
	Frame fram;
	Frame_class_info *priv;
	XFontStruct *fs;
	char *s;
	int sx, mx, width, ex;
	int is_multiclick;

	switch (event_action(ev)) {
		case LOC_DRAG:
			if (! event_is_quick_duplicate(ev)) return FALSE;
			if (action_select_is_down(ev)) {
				return update_secondary(footer, event_x(ev));
			}
			if (action_adjust_is_down(ev)) {
				return update_secondary(footer, event_x(ev));
			}
			break;

		case ACTION_ADJUST:
			if (! event_is_quick_duplicate(ev)) return FALSE;
			if (! event_is_up(ev)) return TRUE;
			fram = xv_get(footer, XV_OWNER);
			priv = FRAME_CLASS_PRIVATE(fram);
			if (! quick_dupl_key) quick_dupl_key = xv_unique_key();
			qd = (quick_data_t *)xv_get(footer, XV_KEY_DATA,quick_dupl_key);
			if (! qd) {
				qd = supply_quick_data(footer, priv);
			}
			if (qd->qc.select_click_cnt == 2) {
				return adjust_wordwise(qd, ev);
			}
			return adjust_secondary(qd, ev);

		case ACTION_SELECT:
			if (! event_is_down(ev)) return FALSE;
			if (! event_is_quick_duplicate(ev)) return FALSE;

			if (! quick_dupl_key) quick_dupl_key = xv_unique_key();

			mx = event_x(ev);
			width = (int)xv_get(footer, XV_WIDTH);
			fram = xv_get(footer, XV_OWNER);
			priv = FRAME_CLASS_PRIVATE(fram);
			qd = (quick_data_t *)xv_get(footer, XV_KEY_DATA,quick_dupl_key);
			if (! qd) {
				qd = supply_quick_data(footer, priv);
			}

			is_multiclick = determine_multiclick(qd->multiclick_timeout,
							&qd->qc.last_click_time, &event_time(ev));
			qd->qc.last_click_time = event_time(ev);

			qd->is_left = (mx < (3 * width) / 4);
			qd->qc.startx = 0; /* uninitialized */
			fs = get_fontstruct(priv);
			if (qd->is_left) {
				s = priv->left_footer;
				sx = priv->left_x;
				ex = priv->right_x;
			}
			else {
				s = priv->right_footer;
				sx = priv->right_x;
				ex = width;
			}

			qd->qc.xpos[0] = 0;
			if (s) {
				qd->qc.startx = sx;
				xvq_mouse_to_charpos(&qd->qc, fs, mx, s,
							&qd->qc.startx, &qd->qc.startindex);

				if (is_multiclick) {
					Xv_Drawable_info *info;

					++qd->qc.select_click_cnt;
					if (qd->qc.select_click_cnt == 2) {
						/* really sx here ? Or rather qd->qc.startx ???? */
						xvq_select_word(&qd->qc, s, sx, fs);
					}
					else if (qd->qc.select_click_cnt > 2) {
						/* whole text */
						qd->qc.startindex = 0;
						qd->qc.startx = sx;
						qd->qc.endindex = strlen(s) - 1;
						qd->qc.endx = ex;
					}
					DRAWABLE_INFO_MACRO(event_window(ev), info);

					XDrawLine(xv_display(info), xv_xid(info), qd->qc.gc,
									qd->qc.startx, qd->qc.baseline,
									qd->qc.endx, qd->qc.baseline);
				}
				else {
					qd->qc.select_click_cnt = 1;
					qd->qc.endx = qd->qc.startx;
				}
			}
			else {
				qd->qc.select_click_cnt = 1;
				qd->qc.endx = qd->qc.startx;
			}

			xv_set(qd->qc.sel_owner,
					SEL_OWN, TRUE,
					SEL_TIME, &qd->qc.last_click_time,
					NULL);
			qd->qc.seltext[0] = '\0';
			return TRUE;
	}

	return FALSE;
}

Pkg_private Notify_value frame_footer_input(Xv_Window footer, Event *ev,
								Notify_arg arg, Notify_event_type type)
{
	if (is_quick_duplicate_on_footer(footer, ev)) return NOTIFY_DONE;

	switch (event_action(ev)) {
		case WIN_REPAINT:
			frame_display_footer(xv_get(footer, WIN_PARENT), FALSE);
			break;

		case ACTION_SELECT:
		case ACTION_ADJUST:
		case ACTION_MENU:
			xv_ol_default_background(footer, ev);
			break;

		case ACTION_HELP:
			if (event_is_down(ev)) {
				char *help_data = (char *)xv_get(footer, XV_HELP_DATA);
				if (help_data) {
					/* we try footer help */
					if (XV_ERROR == xv_help_show(footer, help_data, ev)) {
						Frame f = xv_get(footer, WIN_PARENT);

						/* no footer help, try the frame help */
						help_data = (char *)xv_get(f, XV_HELP_DATA);
						if (help_data) {
							xv_help_show(f, help_data, ev);
						}
					}
				}
			}
			return NOTIFY_DONE;
		default:
			break;
	}
	return notify_next_event_func(footer, (Notify_event)ev, arg, type);
}
