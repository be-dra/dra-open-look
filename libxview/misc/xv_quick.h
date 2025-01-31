#ifndef xv_quick_h_INCLUDED
#define xv_quick_h_INCLUDED

/*
 * "@(#) %M% V%I% %E% %U% $Id: xv_quick.h,v 1.2 2025/01/18 22:30:51 dra Exp $"
 *
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

/* #include <X11/sel_pkg.h> */
#include <xview/sel_pkg.h>

typedef struct {
	int startx; /* where the ACTION_SELECT down happened */
	int endx;
	Selection_owner sel_owner;
	GC gc;
	int baseline;
	int xpos[500];
	int startindex, endindex;
	char delimtab[256];   /* TRUE= character is a word delimiter */
	unsigned long reply_data;
	char seltext[1000];
    struct timeval last_click_time;
	int select_click_cnt;
} quick_common_data_t;

Xv_private int xvq_note_quick_convert(Selection_owner sel_own,
			quick_common_data_t *qd,
			Atom *type, Xv_opaque *data, unsigned long *length, int *format);

Xv_private void xvq_mouse_to_charpos(quick_common_data_t *qd, XFontStruct *fs,
						int mx, char *s, int *sx, int *startindex);
Xv_private void xvq_adjust_secondary(quick_common_data_t *qd, XFontStruct *fs,
						Event *ev, char *s, int sx);
Xv_private void xvq_select_word(quick_common_data_t *qd, char *s, int sx,
							XFontStruct *fs);
Xv_private int xvq_adjust_wordwise(quick_common_data_t *qd, XFontStruct *fs, 
				char *s, int sx, Event *ev);
#endif /* xv_quick_h_INCLUDED */
