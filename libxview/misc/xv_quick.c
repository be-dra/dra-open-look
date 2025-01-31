/*
 * This file is a product of Bernhard Drahota and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify this file without charge, but are not authorized to
 * license or distribute it to anyone else except as part of a product
 * or program developed by the user.
 *
 * THIS FILE IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 *
 * This file is provided with no support and without any obligation on the
 * part of Bernhard Drahota to assist in its use, correction,
 * modification or enhancement.
 *
 * BERNHARD DRAHOTA SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY THIS FILE
 * OR ANY PART THEREOF.
 *
 * In no event will Bernhard Drahota be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even
 * if B. Drahota has been advised of the possibility of such damages.
 */

char xv_quick_c_sccsid[] = "@(#) %M% V%I% %E% %U% $Id: xv_quick.c,v 1.2 2025/01/18 22:30:51 dra Exp $";

#include <xview/xview.h>
#include <xview_private/draw_impl.h>
#include <xview_private/xv_quick.h>

Xv_private int xvq_note_quick_convert(Selection_owner sel_own,
			quick_common_data_t *qd,
			Atom *type, Xv_opaque *data, unsigned long *length, int *format)
{
	Xv_window win = xv_get(sel_own, XV_OWNER);
	Xv_Server srv = XV_SERVER_FROM_WINDOW(win);

	if (*type == xv_get(srv, SERVER_ATOM, "_SUN_SELECTION_END")) {
		/* Lose the Secondary Selection */
		xv_set(sel_own, SEL_OWN, FALSE, NULL);
        *type = xv_get(srv, SERVER_ATOM, "NULL");
        *data = XV_NULL;
        *length = 0;
        *format = 32;
		return TRUE;
	}
	if (*type == xv_get(srv, SERVER_ATOM, "LENGTH")) {
		/* This is only used by SunView1 selection clients for
		 * clipboard and secondary selections.
		 */
		qd->reply_data = strlen(qd->seltext);
		*data = (Xv_opaque)&qd->reply_data;
		*length = 1;
		*format = 32;
		return TRUE;
	}
	if (*type == xv_get(srv, SERVER_ATOM, "_SUN_SELN_IS_READONLY")) {
		qd->reply_data = TRUE; /* our footers are always read only */
		*data = (Xv_opaque)&qd->reply_data;
		*length = 1;
		*format = 32;
		return TRUE;
	}
	if (*type == xv_get(srv, SERVER_ATOM, "_OL_SELECTION_IS_WORD")) {
		qd->reply_data = (qd->select_click_cnt == 2);
		*data = (Xv_opaque)&qd->reply_data;
		*length = 1;
		*format = 32;
		*type = XA_INTEGER;
		return TRUE;
	}
	if (*type == XA_STRING) {
		*data = (Xv_opaque)qd->seltext;
		*length = strlen(qd->seltext);
		*format = 8;
		return TRUE;
	}

	return FALSE;
}

Xv_private void xvq_mouse_to_charpos(quick_common_data_t *qd, XFontStruct *fs,
						int mx, char *s, int *sx, int *startindex)
{
	int i, len = strlen(s);
	int outsx = *sx, outstartidx = *startindex;

	for (i = 0; i < len; i++) {
		qd->xpos[i] = *sx;
		if (fs->per_char)  {
			*sx += fs->per_char[(u_char)s[i] - fs->min_char_or_byte2].width;
		}
		else *sx += fs->min_bounds.width;
		if (mx >= qd->xpos[i] && mx < *sx) {
			outsx = qd->xpos[i];
			outstartidx = i;
		}
	}
	qd->xpos[i] = *sx;
	qd->xpos[i+1] = 0;

	*sx = outsx;
	*startindex = outstartidx;
}

Xv_private void xvq_adjust_secondary(quick_common_data_t *qd, XFontStruct *fs,
						Event *ev, char *s, int sx)
{
	Xv_Drawable_info *info;
	int startx = qd->startx;
	int endx = qd->endx;
	int startindex = qd->startindex;
	int endindex = qd->endindex;
	int newstartx, newstartindex;

	newstartx = sx;

	xvq_mouse_to_charpos(qd,fs, event_x(ev), s, &newstartx, &newstartindex);

	/* several cases: */
	if (newstartx < startx) { /* adjust left of old start */
		qd->startx = newstartx;
		qd->startindex = newstartindex;
		qd->endx = endx;
		qd->endindex = endindex;
	}
	else if (newstartx > endx) { /* adjust right of old end */
		qd->startx = startx;
		qd->startindex = startindex;
		qd->endx = newstartx;
		qd->endindex = newstartindex;
	}
	else {
		int leftdist, rightdist;

		/* adjust click somewhere in between */
		leftdist = newstartx - startx;
		rightdist = endx - newstartx;
		if (leftdist < rightdist) {
			qd->startx = newstartx;
			qd->startindex = newstartindex;
			qd->endx = endx;
			qd->endindex = endindex;
		}
		else {
			qd->startx = startx;
			qd->startindex = startindex;
			qd->endx = newstartx;
			qd->endindex = newstartindex;
		}
	}
	if (!qd->endx) {
		qd->endx = qd->startx;
	}

	DRAWABLE_INFO_MACRO(event_window(ev), info);

	XDrawLine(xv_display(info), xv_xid(info), qd->gc,
				startx, qd->baseline, endx, qd->baseline);

	XDrawLine(xv_display(info), xv_xid(info), qd->gc,
				qd->startx, qd->baseline, qd->endx, qd->baseline);
}

Xv_private void xvq_select_word(quick_common_data_t *qd, char *s, int sx,
							XFontStruct *fs)
{
	int i;
	int cwidth;

	/* I need a new startindex <= qd->startindex
	 * and a new startx <= qd->startx
	 * and an endindex and a endx
	 * and NOTHING ELSE in qd may be modified ! !!!
	 */

	i = qd->startindex;
	if (qd->delimtab[(int)s[i]]) {
		if (fs->per_char)  {
			cwidth = fs->per_char[(u_char)s[i] - fs->min_char_or_byte2].width;
		}
		else cwidth = fs->min_bounds.width;
		qd->endindex = qd->startindex;
		qd->endx = qd->startx + cwidth;
	}
	else {
		int len = strlen(s);
		int cwid_sum = 0;
		/* the char at the mouse click is alphanumeric - we walk back
		 * to the beginning of the word
		 */
		if (qd->startindex == 0) {
			/* no need to walk back - there is no 'back' */
		}
		else {
			for (i = qd->startindex-1; i >= 0 && ! qd->delimtab[(int)s[i]]; i--)
			{
				if (fs->per_char)  {
					cwidth = fs->per_char[(u_char)s[i]-fs->min_char_or_byte2].width;
				}
				else cwidth = fs->min_bounds.width;
				cwid_sum += cwidth;
			}
			if (i < 0) qd->startindex = 0;
			else qd->startindex = i + 1;
			qd->startx = qd->startx - cwid_sum;
		}

		/* now we walk forward until the end of the word */
		cwid_sum = 0;
		for (i = qd->startindex; i < len && ! qd->delimtab[(int)s[i]]; i++) {
			if (fs->per_char)  {
				cwidth = fs->per_char[(u_char)s[i]-fs->min_char_or_byte2].width;
			}
			else cwidth = fs->min_bounds.width;
			cwid_sum += cwidth;
		}
		if (i >= len) qd->endindex = len - 1;
		else if (i == 0) qd->endindex = 0;
		else qd->endindex = i - 1;
		qd->endx = qd->startx + cwid_sum;
	}
}

Xv_private int xvq_adjust_wordwise(quick_common_data_t *qd, XFontStruct *fs, 
				char *s, int sx, Event *ev)
{
	Xv_Drawable_info *info;
	int startx = qd->startx;
	int endx = qd->endx;
	int startindex = qd->startindex;
	int endindex = qd->endindex;

	qd->startx = sx;
	xvq_mouse_to_charpos(qd, fs, event_x(ev), s,
							&qd->startx, &qd->startindex);
	/* really sx here ? Or rather qd->startx ???? */
	xvq_select_word(qd, s, sx, fs);

	/* several cases: */
	if (qd->startx < startx) { /* adjust left of old start */
		qd->endx = endx;
		qd->endindex = endindex;
	}
	else if (qd->endx > endx) { /* adjust right of old end */
		qd->startx = startx;
		qd->startindex = startindex;
	}
	else {
		int leftdist, rightdist;

		/* adjust click somewhere in between */
		leftdist = qd->startx - startx;
		rightdist = endx - qd->endx;
		if (leftdist < rightdist) {
			qd->endx = endx;
			qd->endindex = endindex;
		}
		else {
			qd->startx = startx;
			qd->startindex = startindex;
		}
	}

	DRAWABLE_INFO_MACRO(event_window(ev), info);
	XDrawLine(xv_display(info), xv_xid(info), qd->gc,
							startx, qd->baseline, endx, qd->baseline);
	XDrawLine(xv_display(info), xv_xid(info), qd->gc,
							qd->startx, qd->baseline,
							qd->endx, qd->baseline);
	return TRUE;
}
