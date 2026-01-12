#include <stdio.h>
#include <stdlib.h>
#include "win.h"
#include "screen.h"
#include "atom.h"
#include "ollocale.h"
#include "globals.h"
#include <X11/Xatom.h>

char dra_quick_c_sccsid[] = "@(#) %M% V%I% %E% %U% $Id: dra_quick.c,v 1.25 2026/01/11 19:48:08 dra Exp $";

typedef struct _quick_dupl {
	int startx; /* where the ACTION_SELECT down happened */
	int endx;
	GC gc;
	int xpos[500];
	int startindex, endindex;
	char seltext[1000];
	char delimtab[256];   /* TRUE= character is a word delimiter */
	Time last_click_time;
	int select_click_cnt;
	XFontStruct *fs;
	int baseline;
} quick_data_t;

typedef unsigned char uch_t;
	
static quick_data_t *supply_quick_data(Display *dpy, quick_data_t *qd,
						Window root, Client *cli, Graphics_info *gi)
{
	int i;
	XGCValues   gcv;
	char *delims, delim_chars[256]; /* delimiter characters */

	if (! qd) {
		qd = calloc(1, sizeof(quick_data_t));

		/* Print the string into an array to parse the potential
		 * octal/special characters.
		 */
		strcpy(delim_chars, GRV.TextDelimiterChars);
		/* Mark off the delimiters specified */
		for (i = 0; i < sizeof(qd->delimtab); i++) qd->delimtab[i] = False;
		for (delims = delim_chars; *delims; delims++) {
			qd->delimtab[(int)*delims] = True;
		}
	}

	if (Dimension(gi) == OLGX_2D) {
		gcv.foreground =(gi->pixvals[OLGX_BLACK] ^ gi->pixvals[OLGX_WHITE]);
	}
	else {
		gcv.foreground = (gi->pixvals[OLGX_BLACK] ^ gi->pixvals[OLGX_BG1]);
	}
	qd->fs = gi->textfont;
	if (! qd->gc) {
		gcv.function = GXxor;
		qd->gc = XCreateGC(dpy, root, GCForeground | GCFunction, &gcv);
	}
	else {
		XChangeGC(dpy, qd->gc, GCForeground, &gcv);
	}

	qd->baseline = cli->framewin->titley + 2;
	return qd;
}

static void mouse_to_charpos(quick_data_t *qd, XFontStruct *fs,
						int mx, char *s, int *sx, int *startindex)
{
	int i, len = strlen(s);
	int outsx = *sx, outstartidx = *startindex;
		 
	for (i = 0; i < len; i++) {     
		qd->xpos[i] = *sx; 
		if (fs->per_char)  {
			*sx += fs->per_char[(uch_t)s[i] - fs->min_char_or_byte2].width;
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

static void select_word(quick_data_t *qd, char *s, int sx, XFontStruct *fs)
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
		if (fs->per_char) {
			cwidth = fs->per_char[(uch_t) s[i] - fs->min_char_or_byte2].width;
		}
		else
			cwidth = fs->min_bounds.width;
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
			for (i = qd->startindex - 1; i >= 0 && !qd->delimtab[(int)s[i]];
					i--) {
				if (fs->per_char) {
					cwidth = fs->per_char[(uch_t) s[i] -
							fs->min_char_or_byte2].width;
				}
				else
					cwidth = fs->min_bounds.width;
				cwid_sum += cwidth;
			}
			if (i < 0)
				qd->startindex = 0;
			else
				qd->startindex = i + 1;
			qd->startx = qd->startx - cwid_sum;
		}

		/* now we walk forward until the end of the word */
		cwid_sum = 0;
		for (i = qd->startindex; i < len && !qd->delimtab[(int)s[i]]; i++) {
			if (fs->per_char) {
				cwidth = fs->per_char[(uch_t) s[i] -
						fs->min_char_or_byte2].width;
			}
			else
				cwidth = fs->min_bounds.width;
			cwid_sum += cwidth;
		}
		if (i >= len)
			qd->endindex = len - 1;
		else if (i == 0)
			qd->endindex = 0;
		else
			qd->endindex = i - 1;
		qd->endx = qd->startx + cwid_sum;
	}
}

static Bool answer_true(Client *cli, Time t)
{
	FrameAllowEvents(cli, t);
	return True;
}

static Time quickTimestamp;

/* now I finally know the reason why non-XView applications lock up
 * the screen: they usually have a different focusMode and therefore
 * (see FrameSetupGrabs in winframe.c) have a button grab....
 *
 * therefore we need to call FrameAllowEvents whenever we return True here...
 */
Bool dra_quick_duplicate_select(Display *dpy, XEvent *event,
					WinGenericFrame *frameInfo)
{
	Client *cli;
	ScreenInfo *scr;
	quick_data_t *qd;
	Atom notUsed;
	int format;
	unsigned long nitems, bytes_after;
	unsigned char *data;
	char *s;
	int sx;
	int is_multiclick;
	Window oldowner;

	if ((event->xbutton.state & ModMaskMap[MOD_QUICKDUPL]) == 0) return False;

	cli = frameInfo->core.client;

	if (event->type == ButtonRelease)
		return answer_true(cli, event->xbutton.time);
	if (event->type != ButtonPress) return False;

	/* wir sahen auch WIN_ICON... derzeit noch nicht */
	if (frameInfo->core.kind != WIN_FRAME)
		return answer_true(cli, event->xbutton.time);

	/* now, a window kann also have a olwm-owned footer
	 * see _OL_WINMSG_ERROR, _OL_WINMSG_STATE, _OL_DECOR_FOOTER.
	 * In such a situation we see ButtonPress events with y much bigger than 26
	 */
	if (event->xbutton.y > heightTopFrame(frameInfo))
		return answer_true(cli, event->xbutton.time);

	scr = cli->scrInfo;
	FrameAllowEvents(cli, event->xbutton.time);

	data = NULL;
	if (XGetWindowProperty(dpy, scr->rootid, Atom_SUN_QUICK_SELECTION_KEY_STATE,
					0L, 1L, False, XA_ATOM, &notUsed, &format, &nitems,
					&bytes_after, &data) != Success) {
		return True;
	}
	if (!data)
		return True;

	/* key_type == DUPLICATE in this case */
	if (AtomDUPLICATE != *(Atom *) data) {
		/* quick move is not supported */
		XFree((char *)data);
		return True;
	}
	XFree((char *)data);

	scr->qc = supply_quick_data(dpy, scr->qc, scr->rootid, cli,
								WinGI(frameInfo, NORMAL_GINFO));
	qd = scr->qc;

	is_multiclick = ((event->xbutton.time - qd->last_click_time)
			< GRV.DoubleClickTime);
	qd->last_click_time = event->xbutton.time;

	/* tried out: event->xbutton.window == frameInfo->core.self */

	s = frameInfo->fcore.name;
	sx = cli->framewin->titlex;

	qd->xpos[0] = 0;
	if (s) {
		int save_startx = qd->startx, save_endx = qd->endx;

		qd->startx = sx;
		mouse_to_charpos(qd, qd->fs, event->xbutton.x, s,
				&qd->startx, &qd->startindex);

		if (is_multiclick) {

			++qd->select_click_cnt;
			if (qd->select_click_cnt == 2) {
				/* really sx here ? Or rather qd->startx ???? */
				select_word(qd, s, sx, qd->fs);
			}
			else if (qd->select_click_cnt == 3) {
				int u;
				XCharStruct tit;

				/* clean the "word underlining" */
				XDrawLine(dpy, event->xbutton.window, qd->gc,
					save_startx, qd->baseline, save_endx, qd->baseline);
				
				/* whole text */
				qd->startindex = 0;
				qd->startx = cli->framewin->titlex;
				qd->endindex = strlen(s) - 1;
				XTextExtents(qd->fs, frameInfo->fcore.name,
					strlen(frameInfo->fcore.name), &u, &u, &u, &tit);
				qd->endx = qd->startx + tit.width;
			}

			XDrawLine(dpy, event->xbutton.window, qd->gc,
					qd->startx, qd->baseline, qd->endx, qd->baseline);
		}
		else {
			qd->select_click_cnt = 1;
			qd->endx = qd->startx;
		}
	}
	else {
		qd->select_click_cnt = 1;
		qd->endx = qd->startx;
	}

	qd->seltext[0] = '\0';

	oldowner = XGetSelectionOwner(dpy, XA_SECONDARY);
	if (oldowner != None && oldowner != frameInfo->core.self) {
		/* without this we didn't see a SelectionClear event if
		 * the user selected something in frame1 and then in frame2:
		 * the underline in frame1 was not cleared.
		 */
		XSetSelectionOwner(dpy, XA_SECONDARY, None, event->xbutton.time);
	}

	quickTimestamp = event->xbutton.time;
	XSetSelectionOwner(dpy, XA_SECONDARY, frameInfo->core.self, quickTimestamp);

	dra_olwm_trace(300, "own SECONDARY selection\n");

	return True;
}

static void adjust_charwise(quick_data_t *qd, char *s, int sx, XEvent *ev)
{
	int startx = qd->startx;
	int endx = qd->endx;
	int startindex = qd->startindex;
	int endindex = qd->endindex;
	int newstartx, newstartindex;

	newstartx = sx;

	mouse_to_charpos(qd, qd->fs, ev->xbutton.x, s,
						&newstartx, &newstartindex);

	/* several cases: */
	if (newstartx < startx) {	/* adjust left of old start */
		qd->startx = newstartx;
		qd->startindex = newstartindex;
		qd->endx = endx;
		qd->endindex = endindex;
	}
	else if (newstartx > endx) {	/* adjust right of old end */
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

	XDrawLine(ev->xbutton.display, ev->xbutton.window, qd->gc,
			startx, qd->baseline, endx, qd->baseline);

	XDrawLine(ev->xbutton.display, ev->xbutton.window, qd->gc,
			qd->startx, qd->baseline, qd->endx, qd->baseline);
}
            
static void adjust_wordwise(quick_data_t *qd, char *s, int sx, XEvent *ev)
{
    int startx = qd->startx;
    int endx = qd->endx;
    int startindex = qd->startindex;
    int endindex = qd->endindex;
     
    qd->startx = sx;
    mouse_to_charpos(qd, qd->fs, ev->xbutton.x, s,
                            &qd->startx, &qd->startindex);
    /* really sx here ? Or rather qd->startx ???? */
    select_word(qd, s, sx, qd->fs);
    
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

    XDrawLine(ev->xbutton.display, ev->xbutton.window, qd->gc,
                            startx, qd->baseline, endx, qd->baseline);
    XDrawLine(ev->xbutton.display, ev->xbutton.window, qd->gc,
                            qd->startx, qd->baseline, qd->endx, qd->baseline);
}

Bool dra_quick_duplicate_adjust(Display *dpy, XEvent *event,
					WinGenericFrame *frameInfo)
{
	Client *cli;
	ScreenInfo *scr;
	quick_data_t *qd;
	Atom notUsed;
	int format;
	unsigned long nitems, bytes_after;
	unsigned char *data;

	if ((event->xbutton.state & ModMaskMap[MOD_QUICKDUPL]) == 0) return False;

	cli = frameInfo->core.client;
	if (event->type == ButtonRelease)
		return answer_true(cli, event->xbutton.time);
	if (event->type != ButtonPress) return False;
	/* now we know that we are in "quick mode" - we always return True
	 * in order to eat this button event
	 */
	FrameAllowEvents(cli, event->xbutton.time);

	/* wir sahen auch WIN_ICON... derzeit noch nicht */
	if (frameInfo->core.kind != WIN_FRAME) return True;

	/* now, a window kann also have a olwm-owned footer
	 * see _OL_WINMSG_ERROR, _OL_WINMSG_STATE, _OL_DECOR_FOOTER.
	 * In such a situation we see ButtonPress events with y much bigger than 26
	 */
	if (event->xbutton.y > heightTopFrame(frameInfo))
		return True;

	scr = cli->scrInfo;

	if (!scr->qc)
		return True;

	qd = scr->qc;
	data = NULL;
	if (XGetWindowProperty(dpy, scr->rootid, Atom_SUN_QUICK_SELECTION_KEY_STATE,
					0L, 1L, False, XA_ATOM, &notUsed, &format, &nitems,
					&bytes_after, &data) != Success) {
		return True;
	}
	if (!data)
		return True;

	/* key_type == DUPLICATE in this case */
	if (AtomDUPLICATE != *(Atom *) data) {
		/* quick move is not supported */
		XFree((char *)data);
		return True;
	}
	XFree((char *)data);

	if (qd->select_click_cnt == 2) {
		adjust_wordwise(qd, frameInfo->fcore.name, cli->framewin->titlex,event);
	}
	else adjust_charwise(qd,frameInfo->fcore.name, cli->framewin->titlex,event);
	return True;
}

Bool dra_quick_duplicate_update(Display *dpy, XEvent *event,
					WinGenericFrame *frameInfo)
{
	Client *cli = frameInfo->core.client;
	ScreenInfo *scr = cli->scrInfo;
	quick_data_t *qd = scr->qc;
	char *s;
	int baseline, len, i;
	int mx;
	Window w;

	if (event->type != MotionNotify) return False;
	if ((event->xmotion.state & ModMaskMap[MOD_QUICKDUPL])==0) return False;

	/* INCOMPLETE 'select_is_down' !!! */
	if ((event->xmotion.state & Button1Mask) == 0) return True;
	mx = event->xmotion.x;
	w = event->xmotion.window;

	if (! qd) return False;

	baseline = cli->framewin->titley + 2;

	XDrawLine(dpy, w, qd->gc, qd->startx, baseline, qd->endx, baseline);
	qd->endx = 0;

	s = frameInfo->fcore.name;

	if (s) {
		int *xp = qd->xpos;
		len = strlen(s);

		for (i = 0; i < len; i++) {
			if (mx >= xp[i] && mx < xp[i+1]) {
				qd->endx = xp[i+1];
				qd->endindex = i;
				break;
			}
		}
	}
	if (!qd->endx) {
		qd->endx = qd->startx;
	}

	XDrawLine(dpy, w, qd->gc, qd->startx, baseline, qd->endx, baseline);

	return True;
}

int dra_quick_handle_selection(Display *dpy, XEvent *xev,
									WinPaneFrame *frameInfo)
{
	Client *cli = frameInfo->core.client;
	ScreenInfo *scr = cli->scrInfo;
	quick_data_t *qd = scr->qc;

	if (xev->type == SelectionRequest) {
    	XSelectionRequestEvent *scr = &xev->xselectionrequest;
		XSelectionEvent reply;

		dra_olwm_trace(300, "selection request for %ld\n", scr->selection);
    	if (scr->selection != XA_SECONDARY) return 0;

		if (qd->seltext[0] == '\0') {
			char *s;

			/* now we determine the selected string */
			s = frameInfo->fcore.name;
			if (s) {
				/* hier sind wir schon mal (voellig unerwartet) abgestuerzt: */
				strncpy(qd->seltext, s + qd->startindex,
							(size_t)(qd->endindex - qd->startindex + 1));
				qd->seltext[qd->endindex - qd->startindex + 1] = '\0';
			}
		}

		reply.type = SelectionNotify;
		reply.requestor = scr->requestor;
		reply.selection = scr->selection;
		reply.target = scr->target;
		reply.time = scr->time;

		/* preparation of 'reject': */
		reply.property = None;

		if (scr->target == Atom_SUN_SELECTION_END) {
			/* Lose the Secondary Selection */
			unsigned long l = 0L;
			reply.property = scr->property;
    		XChangeProperty(dpy, reply.requestor, reply.property, AtomNULL,
						32, PropModeReplace, (unsigned char *)&l, 0);
			XSetSelectionOwner(dpy, XA_SECONDARY, None, scr->time);
			qd->startindex = qd->endindex = 0;
		}
		else if (scr->target == Atom_OL_SELECTION_IS_WORD) {
			unsigned long l = (qd->select_click_cnt == 2);
			reply.property = scr->property;
    		XChangeProperty(dpy, reply.requestor, reply.property, XA_INTEGER,
						32, PropModeReplace, (unsigned char *)&l, 1);
		}
		else if (scr->target == XA_STRING) {
			reply.property = scr->property;
    		XChangeProperty(dpy, reply.requestor, reply.property, reply.target,
						8, PropModeReplace,
		    			(unsigned char *)qd->seltext, strlen(qd->seltext));
		}
		else if (scr->target == AtomTargets) {
			int i = 0;
			Atom tgts[10];

			tgts[i++] = AtomTargets;
			tgts[i++] = XA_STRING;
			tgts[i++] = Atom_SUN_SELECTION_END;
			tgts[i++] = Atom_OL_SELECTION_IS_WORD;
			tgts[i++] = AtomTimestamp;
			reply.property = scr->property;
    		XChangeProperty(dpy, reply.requestor, reply.property, XA_ATOM,
						32, PropModeReplace, (unsigned char *)tgts, i);
		}
		else if (scr->target == AtomTimestamp) {
			reply.property = scr->property;
    		XChangeProperty(dpy, reply.requestor, reply.property, XA_INTEGER,
					32, PropModeReplace, (unsigned char *)&quickTimestamp, 1);
		}
		dra_olwm_trace(300, "send selection notify for %ld\n", scr->target);
		XSendEvent(dpy, reply.requestor, False, NoEventMask, (XEvent *)&reply);
		XFlush(dpy);
	}
	else if (xev->type == SelectionClear) {
		XSelectionClearEvent *scl = &xev->xselectionclear;

    	if (scl->selection != XA_SECONDARY) return 0;

		qd->startindex = qd->endindex = 0;
		qd->seltext[0] = '\0';
		XClearArea(dpy, frameInfo->core.self, 0, 0, 0, 0, True);
	}
	return 0;
}
