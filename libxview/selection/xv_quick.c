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

char xv_quick_c_sccsid[] = "@(#) %M% V%I% %E% %U% $Id: xv_quick.c,v 1.9 2026/01/11 12:52:45 dra Exp $";

/* This class is a helper for "quick duplicate".
 *
 * It is (as of 2025-03-07) used by 
 *
 * · frame for quick duplicate on footers (fm_input.c)
 * · panel items for quick duplicate on panel labels (p_select.c)
 * · panel lists for quick duplicate on list lines (p_list.c)
 */

#include <xview/xview.h>
#include <xview/defaults.h>
#include <xview/font.h>
#include <xview/cms.h>
#include <xview_private/svr_impl.h>
#include <xview_private/xv_quick.h>

typedef void (*remove_underline_t)(Quick_owner);

#define MAX_SELECTED 500

typedef struct {
    Quick_owner public_self;  /* back pointer to public object */
	int startx; /* left end of underlining */
	int endx; /* right end of underlining */
	int xpos[MAX_SELECTED];
	int startindex, endindex;
	unsigned long reply_data;
	char seltext[MAX_SELECTED];
    struct timeval last_click_time;
	int select_click_cnt;

	remove_underline_t remove_underline;
	Display *dpy;
	Window xid;
	GC gc;
	int baseline;
	int multiclick_timeout;
	char delimtab[256];   /* TRUE= character is a word delimiter */
	XFontStruct *fs;
	Xv_opaque client_data;
	char full_text[MAX_SELECTED];
	int full_startx;
	int full_endx;
} Quick_private_t;

#define QUICKPRIV(_x_) XV_PRIVATE(Quick_private_t, Xv_quick_owner, _x_)
#define QUICKPUB(_x_) XV_PUBLIC(_x_)

#define A1 attrs[1] 
#define A2 attrs[2]
#define A3 attrs[3]
#define A4 attrs[4]
#define ADONE ATTR_CONSUME(*attrs);break

static int quick_init(Xv_Window parent, Quick_owner self, Attr_avlist avlist,
									int *u)
{
	Quick_private_t *priv;
	Xv_quick_owner *slf = (Xv_quick_owner *) self;
	XGCValues   gcv;
	Cms cms;
	int i, fore_index, back_index;
	unsigned long fg, bg;
	char *delims, delim_chars[256];	/* delimiter characters */

	/* Allocate and clear private data */
	priv = xv_alloc(Quick_private_t);

	/* Link private and public data */
	slf->private_data = (Xv_opaque) priv;
	priv->public_self = self;

	/* Initialize private data */
	priv->dpy = (Display *) xv_get(parent, XV_DISPLAY);
	priv->xid = (XID) xv_get( parent, XV_XID );

	cms = xv_get(parent, WIN_CMS);
	fore_index = (int)xv_get(parent, WIN_FOREGROUND_COLOR);
	back_index = (int)xv_get(parent, WIN_BACKGROUND_COLOR);
	bg = (unsigned long)xv_get(cms, CMS_PIXEL, back_index);
	fg = (unsigned long)xv_get(cms, CMS_PIXEL, fore_index);

	gcv.foreground = fg ^ bg;
	gcv.function = GXxor;

	priv->gc = XCreateGC(priv->dpy, priv->xid, GCForeground | GCFunction, &gcv);
	priv->baseline = (int)xv_get(parent, XV_HEIGHT) - 2;

	priv->multiclick_timeout = 100 *
	    defaults_get_integer_check("openWindows.multiClickTimeout",
		                   "OpenWindows.MultiClickTimeout", 4, 2, 10);

	delims = (char *)defaults_get_string("text.delimiterChars",
			"Text.DelimiterChars", " \t,.:;?!\'\"`*/-+=(){}[]<>\\|~@#$%^&");
	/* Print the string into an array to parse the potential
	 * octal/special characters.
	 */
	strcpy(delim_chars, delims);
	/* Mark off the delimiters specified */
	for (i = 0; i < sizeof(priv->delimtab); i++) priv->delimtab[i] = FALSE;
	for (delims = delim_chars; *delims; delims++) {
		priv->delimtab[(int)*delims] = TRUE;
	}

	priv->fs = (XFontStruct *)xv_get(xv_get(parent, XV_FONT), FONT_INFO);

	return XV_OK;
}

static int note_quick_convert(Quick_owner self, Atom *type,
					Xv_opaque *data, unsigned long *length, int *format)
{
	Quick_private_t *priv = QUICKPRIV(self);
	Xv_window win = xv_get(self, XV_OWNER);
	Xv_Server srv = XV_SERVER_FROM_WINDOW(win);

	if (priv->seltext[0] == '\0') {
		strncpy(priv->seltext, priv->full_text + priv->startindex,
						(size_t)(priv->endindex - priv->startindex + 1));
		priv->seltext[priv->endindex - priv->startindex + 1] = '\0';
	}

	if (*type == xv_get(srv, SERVER_ATOM, "_SUN_SELECTION_END")) {
		/* Lose the Secondary Selection */
		xv_set(self, SEL_OWN, FALSE, NULL);
        *type = xv_get(srv, SERVER_ATOM, "NULL");
        *data = XV_NULL;
        *length = 0;
        *format = 32;
		return TRUE;
	}
	else if (*type == xv_get(srv, SERVER_ATOM, "LENGTH")) {
		/* This is only used by SunView1 selection clients for
		 * clipboard and secondary selections.
		 */
		priv->reply_data = strlen(priv->seltext);
		*data = (Xv_opaque)&priv->reply_data;
		*length = 1;
		*format = 32;
		return TRUE;
	}
	else if (*type == xv_get(srv, SERVER_ATOM, "_SUN_SELN_IS_READONLY")) {
		priv->reply_data = TRUE;
		*data = (Xv_opaque)&priv->reply_data;
		*length = 1;
		*format = 32;
		return TRUE;
	}
	else if (*type == xv_get(srv, SERVER_ATOM, "_OL_SELECTION_IS_WORD")) {
		priv->reply_data = (priv->select_click_cnt == 2);
		*data = (Xv_opaque)&priv->reply_data;
		*length = 1;
		*format = 32;
		*type = XA_INTEGER;
		return TRUE;
	}
	else if (*type == XA_STRING) {
		*data = (Xv_opaque)priv->seltext;
		*length = strlen(priv->seltext);
		*format = 8;
		return TRUE;
	}
	else if (*type == xv_get(srv, SERVER_ATOM, "TARGETS")) {
		static Atom tgts[10];
		int i = 0;

		tgts[i++] = xv_get(srv, SERVER_ATOM, "_SUN_SELECTION_END");
		tgts[i++] = xv_get(srv, SERVER_ATOM, "LENGTH");
		tgts[i++] = xv_get(srv,SERVER_ATOM,"_SUN_SELN_IS_READONLY");
		tgts[i++] = xv_get(srv,SERVER_ATOM,"_OL_SELECTION_IS_WORD");
		tgts[i++] = XA_STRING;
		tgts[i++] = *type;
		tgts[i++] = xv_get(srv,SERVER_ATOM,"TIMESTAMP");
		*data = (Xv_opaque)tgts;
		*length = i;
		*format = 32;
		*type = XA_ATOM;
		return TRUE;
	}

	return sel_convert_proc(self, type, data, length, format);
}

static void note_quick_lose(Quick_owner self)
{
	Quick_private_t *priv = QUICKPRIV(self);

	priv->startx = priv->endx = -1;
	priv->startindex = priv->endindex = 0;
	priv->reply_data = 0;
	priv->seltext[0] = '\0';
	priv->select_click_cnt = 0;
	priv->full_startx = priv->full_endx = -1;
	priv->full_text[0] = '\0';

	SERVERTRACE((300, "%s\n", __FUNCTION__));
	if (priv->remove_underline) {
		priv->remove_underline(self);
	}
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

static void mouse_to_charpos(Quick_private_t *priv, int mx, char *s,
									int *sx, int *startindex)
{
	int i, len = strlen(s);
	int outsx = *sx, outstartidx = *startindex;

	for (i = 0; i < len; i++) {
		priv->xpos[i] = *sx;
		if (priv->fs->per_char)  {
			*sx += priv->fs->per_char[(u_char)s[i]
						- priv->fs->min_char_or_byte2].width;
		}
		else *sx += priv->fs->min_bounds.width;
		if (mx >= priv->xpos[i] && mx < *sx) {
			outsx = priv->xpos[i];
			outstartidx = i;
		}
	}
	priv->xpos[i] = *sx;
	priv->xpos[i+1] = 0;

	*sx = outsx;
	*startindex = outstartidx;
}

static void select_word(Quick_private_t *priv, char *s, int sx)
{
	int i;
	int cwidth;
	XFontStruct *fs = priv->fs;

	/* I need a new startindex <= qd->startindex
	 * and a new startx <= qd->startx
	 * and an endindex and a endx
	 * and NOTHING ELSE in priv may be modified ! !!!
	 */

	i = priv->startindex;
	if (priv->delimtab[(int)s[i]]) {
		if (fs->per_char)  {
			cwidth = fs->per_char[(u_char)s[i] - fs->min_char_or_byte2].width;
		}
		else cwidth = fs->min_bounds.width;
		priv->endindex = priv->startindex;
		priv->endx = priv->startx + cwidth;
	}
	else {
		int len = strlen(s);
		int cwid_sum = 0;
		/* the char at the mouse click is alphanumeric - we walk back
		 * to the beginning of the word
		 */
		if (priv->startindex == 0) {
			/* no need to walk back - there is no 'back' */
		}
		else {
			for (i = priv->startindex-1; i >= 0 && ! priv->delimtab[(int)s[i]]; i--)
			{
				if (fs->per_char)  {
					cwidth = fs->per_char[(u_char)s[i]-fs->min_char_or_byte2].width;
				}
				else cwidth = fs->min_bounds.width;
				cwid_sum += cwidth;
			}
			if (i < 0) priv->startindex = 0;
			else priv->startindex = i + 1;
			priv->startx = priv->startx - cwid_sum;
		}

		/* now we walk forward until the end of the word */
		cwid_sum = 0;
		for (i = priv->startindex; i < len && ! priv->delimtab[(int)s[i]]; i++) {
			if (fs->per_char)  {
				cwidth = fs->per_char[(u_char)s[i]-fs->min_char_or_byte2].width;
			}
			else cwidth = fs->min_bounds.width;
			cwid_sum += cwidth;
		}
		if (i >= len) priv->endindex = len - 1;
		else if (i == 0) priv->endindex = 0;
		else priv->endindex = i - 1;
		priv->endx = priv->startx + cwid_sum;
	}
}

static void select_start(Quick_private_t *priv, char *s, int sx, int ex)
{
	if (strlen(s) >= MAX_SELECTED) {
		xv_error(XV_NULL,
					ERROR_STRING, XV_MSG("Quick duplicate: string too long"),
					ERROR_PKG, QUICK_OWNER,
					NULL);
		return;
	}
	strcpy(priv->full_text, s);
	priv->full_startx = sx;
	priv->full_endx = ex;
	SERVERTRACE((300, "%s\n", __FUNCTION__));
}

static void quick_select_down(Quick_private_t *priv, Event *ev)
{
	int save_startx = priv->startx, save_endx = priv->endx;
	int is_multiclick = determine_multiclick(priv->multiclick_timeout,
					&priv->last_click_time, &event_time(ev));
	priv->last_click_time = event_time(ev);

	priv->xpos[0] = 0;

	priv->startx = priv->full_startx;
	priv->endx = priv->full_endx;
	mouse_to_charpos(priv,event_x(ev), priv->full_text, &priv->startx,
									&priv->startindex);

	SERVERTRACE((300, "%s\n", __FUNCTION__));
	if (is_multiclick) {
		++priv->select_click_cnt;
		if (priv->select_click_cnt == 2) {
			select_word(priv, priv->full_text, priv->startx);
		}
		else if (priv->select_click_cnt == 3) {
			/* whole text */

			/* clean the "word underlining" */
			XDrawLine(priv->dpy, priv->xid, priv->gc,
						save_startx, priv->baseline,
						save_endx, priv->baseline);

			priv->startindex = 0;
			priv->startx = priv->full_startx;
			priv->endindex = strlen(priv->full_text) - 1;
			priv->endx = priv->full_endx;
		}

		XDrawLine(priv->dpy, priv->xid, priv->gc,
						priv->startx, priv->baseline,
						priv->endx, priv->baseline);
	}
	else {
		priv->select_click_cnt = 1;
		priv->endx = priv->startx;

		xv_set(QUICKPUB(priv),
				SEL_OWN, TRUE,
				SEL_TIME, &priv->last_click_time,
				NULL);
		priv->seltext[0] = '\0';
	}
}

static int update_secondary(Quick_private_t *priv, int mx)
{
	int *xp = priv->xpos;
	int len, i;

	XDrawLine(priv->dpy, priv->xid, priv->gc,
				priv->startx, priv->baseline, priv->endx, priv->baseline);
	priv->endx = 0;

	len = strlen(priv->full_text);

	for (i = 0; i < len; i++) {
		if (mx >= xp[i] && mx < xp[i+1]) {
			priv->endx = xp[i+1];
			priv->endindex = i;
			break;
		}
	}

	if (!priv->endx) {
		priv->endx = priv->startx;
	}

	XDrawLine(priv->dpy, priv->xid, priv->gc,
				priv->startx, priv->baseline, priv->endx, priv->baseline);

	return TRUE;
}

static int quick_loc_drag(Quick_private_t *priv, Event *ev)
{
	SERVERTRACE((300, "%s\n", __FUNCTION__));
	if (action_select_is_down(ev)) {
		return update_secondary(priv, event_x(ev));
	}
	if (action_adjust_is_down(ev)) {
		return update_secondary(priv, event_x(ev));
	}
	return FALSE;
}

static int adjust_secondary(Quick_private_t *priv, Event *ev)
{
	int startx = priv->startx;
	int endx = priv->endx;
	int startindex = priv->startindex;
	int endindex = priv->endindex;
	int newstartx, newstartindex;

	newstartx = priv->full_startx;

	mouse_to_charpos(priv, event_x(ev), priv->full_text,
							&newstartx, &newstartindex);

	/* several cases: */
	if (newstartx < startx) { /* adjust left of old start */
		priv->startx = newstartx;
		priv->startindex = newstartindex;
		priv->endx = endx;
		priv->endindex = endindex;
	}
	else if (newstartx > endx) { /* adjust right of old end */
		priv->startx = startx;
		priv->startindex = startindex;
		priv->endx = newstartx;
		priv->endindex = newstartindex;
	}
	else {
		int leftdist, rightdist;

		/* adjust click somewhere in between */
		leftdist = newstartx - startx;
		rightdist = endx - newstartx;
		if (leftdist < rightdist) {
			priv->startx = newstartx;
			priv->startindex = newstartindex;
			priv->endx = endx;
			priv->endindex = endindex;
		}
		else {
			priv->startx = startx;
			priv->startindex = startindex;
			priv->endx = newstartx;
			priv->endindex = newstartindex;
		}
	}
	if (!priv->endx) {
		priv->endx = priv->startx;
	}

	XDrawLine(priv->dpy, priv->xid, priv->gc,
				startx, priv->baseline, endx, priv->baseline);

	XDrawLine(priv->dpy, priv->xid, priv->gc,
				priv->startx, priv->baseline, priv->endx, priv->baseline);
	return TRUE;
}

static int adjust_wordwise(Quick_private_t *priv, Event *ev)
{
	int startx = priv->startx;
	int endx = priv->endx;
	int startindex = priv->startindex;
	int endindex = priv->endindex;

	priv->startx = priv->full_startx;
	mouse_to_charpos(priv, event_x(ev), priv->full_text,
							&priv->startx, &priv->startindex);
	select_word(priv, priv->full_text, priv->startx);

	/* several cases: */
	if (priv->startx < startx) { /* adjust left of old start */
		priv->endx = endx;
		priv->endindex = endindex;
	}
	else if (priv->endx > endx) { /* adjust right of old end */
		priv->startx = startx;
		priv->startindex = startindex;
	}
	else {
		int leftdist, rightdist;

		/* adjust click somewhere in between */
		leftdist = priv->startx - startx;
		rightdist = endx - priv->endx;
		if (leftdist < rightdist) {
			priv->endx = endx;
			priv->endindex = endindex;
		}
		else {
			priv->startx = startx;
			priv->startindex = startindex;
		}
	}

	XDrawLine(priv->dpy, priv->xid, priv->gc,
							startx, priv->baseline, endx, priv->baseline);
	XDrawLine(priv->dpy, priv->xid, priv->gc,
							priv->startx, priv->baseline,
							priv->endx, priv->baseline);
	return TRUE;
}

static int quick_adjust_up(Quick_private_t *priv, Event *ev)
{
	SERVERTRACE((300, "%s click_cnt=%d\n", __FUNCTION__, priv->select_click_cnt));
	if (priv->select_click_cnt == 2) {
		return adjust_wordwise(priv, ev);
	}
	return adjust_secondary(priv, ev);
}

static Xv_opaque quick_set(Quick_owner self, Attr_avlist avlist)
{
	Attr_avlist attrs;
	Quick_private_t *priv = QUICKPRIV(self);

	for (attrs = avlist; *attrs; attrs = attr_next(attrs)) {
		switch (attrs[0]) {
			case QUICK_START:
				select_start(priv, (char *)A1, (int)A2, (int)A3);
				ADONE;

			case QUICK_BASELINE: priv->baseline = (int)A1; ADONE;
			case QUICK_CLIENT_DATA: priv->client_data = (Xv_opaque)A1; ADONE;
			case QUICK_FONTINFO: priv->fs = (XFontStruct *)A1; ADONE;

			case XV_FONT: {
					Xv_font f = (Xv_font)A1;
					priv->fs = (XFontStruct *)xv_get(f, FONT_INFO);
				}
				ADONE;

			case QUICK_REMOVE_UNDERLINE_PROC:
				priv->remove_underline = (remove_underline_t)A1;
				ADONE;

			case QUICK_SELECT_DOWN:
				quick_select_down(priv, (Event *)A1);
				ADONE;

			case QUICK_LOC_DRAG:
				quick_loc_drag(priv, (Event *)A1);
				ADONE;

			case QUICK_ADJUST_UP:
				quick_adjust_up(priv, (Event *)A1);
				ADONE;

			case XV_END_CREATE:
				xv_set(self, 
					SEL_RANK, XA_SECONDARY,
					SEL_CONVERT_PROC, note_quick_convert,
					SEL_LOSE_PROC, note_quick_lose,
					NULL);
				break;
		}
	}

	return XV_OK;
}


static Xv_opaque quick_get(Quick_owner self, int *status, Attr_attribute attr,
										va_list valist)
{
	Quick_private_t *priv = QUICKPRIV(self);

	switch (attr) {
		case QUICK_NEED_START: return (Xv_opaque)(priv->full_text[0] == '\0');
		case QUICK_BASELINE: return (Xv_opaque)priv->baseline;
		case QUICK_CLIENT_DATA: return (Xv_opaque)priv->client_data;
		case QUICK_REMOVE_UNDERLINE_PROC:
			return (Xv_opaque)priv->remove_underline;
		default:
			*status = XV_ERROR;
			return (Xv_opaque) 0;
	}
}

static int quick_destroy(Quick_owner self, Destroy_status status)
{
	Quick_private_t *priv = QUICKPRIV(self);

	if (status == DESTROY_CHECKING
			|| status == DESTROY_SAVE_YOURSELF
			|| status == DESTROY_PROCESS_DEATH)
		return XV_OK;

	xv_free((char *)priv);

	return XV_OK;
}


const Xv_pkg xv_quick_owner_pkg = {
    "Quick Duplicate",
	ATTR_PKG_QUICK,
    sizeof(Xv_quick_owner),
    SELECTION_OWNER,
    quick_init,
    quick_set,
    quick_get,
    quick_destroy,
    NULL			/* no find proc */
};
