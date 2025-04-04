#ifndef lint
char     ttytl_c_sccsid[] = "@(#)ttytl.c 20.42 93/06/28 DRA: $Id: ttytl.c,v 4.6 2025/03/31 19:38:57 dra Exp $";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */

/*
 * Flavor of ttysw that knows about tool windows and allows tty based
 * programs to set/get data about the tool window (common routines).
 */

#include <stdio.h>
#ifdef SVR4
#include <sys/types.h>
#endif
#include <sys/file.h>
#include <sys/signal.h>
#include <xview/xview.h>
#include <xview/icon.h>
#include <xview/icon_load.h>
#include <xview/wmgr.h>
#include <xview/frame.h>
#include <xview/ttysw.h>
#include <xview_private/i18n_impl.h>
#ifdef OW_I18N
#include <xview/font.h>
#endif
#include <xview_private/tty_impl.h>
#include <xview_private/term_impl.h>
#include <xview_private/txt_impl.h>
#include <xview_private/charscreen.h>
#include <xview_private/win_info.h>
#include <xview_private/fm_impl.h>


/* BUG ALERT: This entire procedure should be rewritten! */
/* BUG ALERT: No XView prefix */
Pkg_private int ttytlsw_escape(Tty_view ttysw_view_public, int c, int ac, int *av)
{
	Tty ttysw_public;
	Xv_object frame_public;
	char buf[150];
	char *p, *text;
	struct rect rect, orect;
	Ttysw_private ttysw, ttytlsw;

#ifdef OW_I18N
	Xv_Font font;
#endif

	ttytlsw = ttysw = TTY_PRIVATE_FROM_ANY_PUBLIC(ttysw_view_public);
	ttysw_public = TTY_PUBLIC(ttysw);
	if (c != 't')
		return (ttysw_ansi_escape(ttysw_view_public, c, ac, av));
	frame_public = xv_get(ttysw_public, WIN_FRAME);
	switch (av[0]) {
		case 1:	/* open */
			xv_set(frame_public, FRAME_CLOSED, FALSE, NULL);
			break;
		case 2:	/* close */
			xv_set(frame_public, FRAME_CLOSED, TRUE, NULL);
			break;
		case 3:	/* move */
			/*
			 * no more interactive moves if (ac == 1 && !xv_get(frame_public,
			 * FRAME_CLOSED)) { wmgr_move(frame_public); break; }
			 */
			win_getrect(frame_public, &rect);
			orect = rect;
			if (av[1] < 0)
				av[1] = rect.r_top;
			if (ac < 3 || av[2] < 0)
				av[2] = rect.r_left;
			rect.r_top = av[1];
			rect.r_left = av[2];
			if (!xv_get(frame_public, FRAME_CLOSED))
				wmgr_completechangerect(frame_public, &rect, &orect, 0, 0);
			else
				(void)win_setrect(frame_public, &rect);
			break;
		case 4:	/* stretch */
			/*
			 * no more interactive stretches if (ac == 1 && !xv_get(frame_public,
			 * FRAME_CLOSED)) { wmgr_stretch(frame_public); break; }
			 */
			(void)win_getrect(frame_public, &rect);
			orect = rect;
			if (av[1] < 0)
				av[1] = rect.r_height;
			if (ac < 3 || av[2] < 0)
				av[2] = rect.r_width;
			rect.r_width = av[2];
			rect.r_height = av[1];
			if (!xv_get(frame_public, FRAME_CLOSED))
				wmgr_completechangerect(frame_public, &rect, &orect, 0, 0);
			else
				(void)win_setrect(frame_public, &rect);
			break;
		case 5:	/* top */
			wmgr_top(frame_public);
			break;
		case 6:	/* bottom */
			wmgr_bottom(frame_public);
			break;
		case 7:	/* refresh */
			wmgr_refreshwindow(frame_public);
			break;
		case 8:	/* stretch, size in chars */
			/*
			 * no more itneractive stretches if (ac == 1 && !xv_get(frame_public,
			 * FRAME_CLOSED)) { wmgr_stretch(frame_public); break; }
			 */
			(void)win_getrect(frame_public, &rect);
			orect = rect;
			if (av[1] <= 0)
				av[1] = (int)xv_get(frame_public, WIN_ROWS);
			if (ac < 3 || av[2] <= 0)
				av[2] = (int)xv_get(frame_public, WIN_COLUMNS);

#ifdef OW_I18N
			/*
			 * This is actually nothing to do with the I18N issue.  Just
			 * get around the problem with xv_pf_sys.
			 */
			font = (Xv_Font) xv_get(ttysw_public, XV_FONT);
			rect.r_width = (av[2] * (int)xv_get(font, FONT_DEFAULT_CHAR_WIDTH))
					+ (2 * FRAME_BORDER_WIDTH);
			rect.r_height =
					(av[1] * (int)xv_get(font, FONT_DEFAULT_CHAR_HEIGHT))
					+ FRAME_BORDER_WIDTH;
#else
			rect.r_width = frame_width_from_columns(av[2]);

			rect.r_height = frame_height_from_lines(av[1],
								(int)xv_get(frame_public, FRAME_SHOW_LABEL));
#endif

			if (!xv_get(frame_public, FRAME_CLOSED))
				wmgr_completechangerect(frame_public, &rect, &orect, 0, 0);
			else
				(void)win_setrect(frame_public, &rect);
			break;
		case 11:	/* report open or iconic */
			if (!xv_get(frame_public, FRAME_CLOSED))
				p = "\33[1t";
			else
				p = "\33[2t";
			(void)ttysw_input_it(ttysw, p, 4);
			break;
		case 13:	/* report position */
			win_getrect(frame_public, &rect);
			sprintf(buf, "\33[3;%d;%dt", rect.r_top, rect.r_left);
			ttysw_input_it(ttysw, buf, (int)strlen(buf));
			break;
		case 14:	/* report size */
			win_getrect(frame_public, &rect);
			sprintf(buf, "\33[4;%d;%dt", rect.r_height, rect.r_width);
			ttysw_input_it(ttysw, buf, (int)strlen(buf));
			break;
		case 18:	/* report size in chars */
			{
				int rows, columns;

				if (ttysw_getopt(ttysw, TTYOPT_TEXT)) {
					rows = textsw_screen_line_count(TTY_PUBLIC(ttysw));
					columns = textsw_screen_column_count(TTY_PUBLIC(ttysw));
				}
				else {
					rows = y_to_row(ttysw->winheightp);
					columns = x_to_col(ttysw->winwidthp);
				}
				(void)sprintf(buf, "\33[8;%d;%dt", rows, columns);
			}
			ttysw_input_it(ttysw, buf, (int)strlen(buf));
			break;
		case 20:{
				/* report icon label */
				Icon icon = (Icon) xv_get(xv_get(TTY_PUBLIC(ttytlsw),
								WIN_FRAME),
						FRAME_ICON);

				(void)ttysw_input_it(ttysw, "\33]L", 3);
				if (0 == (text = (char *)xv_get(icon, ICON_LABEL))) {
					text = (char *)xv_get(frame_public, FRAME_LABEL);
				}
				if (text)
					ttysw_input_it(ttysw, text, (int)strlen(text));
				(void)ttysw_input_it(ttysw, "\33\\", 2);
				break;
			}
		case 21:	/* report name stripe */
			ttysw_input_it(ttysw, "\33]l", 3);
			if ((text = (char *)xv_get(frame_public, FRAME_LABEL)))
				ttysw_input_it(ttysw, text, (int)strlen(text));
			ttysw_input_it(ttysw, "\33\\", 2);
			break;
		default:
			return (TTY_OK);
	}
	return (TTY_DONE);
}

/* BUG ALERT: No XView prefix */
Pkg_private int ttytlsw_string(Tty ttysw_public, int type, int c)
{
	Ttysw_private ttysw = TTY_PRIVATE_FROM_ANY_PUBLIC(ttysw_public);
	Ttysw *ttytlsw = ttysw;
	CHAR namestripe[150];
	CHAR iconlabel[33];

	if (type != ']')
		return (ttysw_ansi_string(ttysw_public, type, c));
	switch (ttytlsw->hdrstate) {
		case HS_BEGIN:
			switch (c) {
				case 'l':
					ttytlsw->nameptr = ttytlsw->namebuf;
					ttytlsw->hdrstate = HS_HEADER;
					break;
				case 'I':
					ttytlsw->nameptr = ttytlsw->namebuf;
					ttytlsw->hdrstate = HS_ICONFILE;
					break;
				case 'L':
					ttytlsw->nameptr = ttytlsw->namebuf;
					ttytlsw->hdrstate = HS_ICON;
					break;
				case '\0':
					break;
				default:
					ttytlsw->hdrstate = HS_FLUSH;
					break;
			}
			break;
		case HS_HEADER:
		case HS_ICONFILE:
		case HS_ICON:

#ifdef OW_I18N
			/*
			 * wchar is used for the internal processing.
			 */
			if (iswprint(c))
#else
			/* jcb 6/1/90 -- change to let (all) eight bit charactes in */
			if ((c >= ' ' && c <= '~') || ((c & 0x80) != 0))
#endif

			{
				if (ttytlsw->nameptr <
						&ttytlsw->namebuf[sizeof(ttytlsw->namebuf) - 1])
					*ttytlsw->nameptr++ = c;
			}
			else if (c == '\0') {
				*ttytlsw->nameptr = '\0';
				switch (ttytlsw->hdrstate) {
					case HS_HEADER:

#ifdef OW_I18N
						(void)STRNCPY(namestripe, ttytlsw->namebuf,
								sizeof(namestripe) / sizeof(CHAR));
						(void)xv_set(xv_get(TTY_PUBLIC(ttysw), WIN_FRAME),
								XV_LABEL_WCS, namestripe, NULL);
#else
						(void)strncpy(namestripe, ttytlsw->namebuf,
								sizeof(namestripe));
						(void)xv_set(xv_get(TTY_PUBLIC(ttysw), WIN_FRAME),
								FRAME_LABEL, namestripe, NULL);
#endif

						break;
					case HS_ICONFILE:{
							char err[IL_ERRORMSG_SIZE];
							struct pixrect *mpr;
							Icon icon;

#ifdef OW_I18N
							char *mb_filename;

							mb_filename = _xv_wcstombsdup(ttytlsw->namebuf);
							if ((mpr = icon_load_mpr(mb_filename, err)) ==
#else
							if ((mpr = icon_load_mpr(ttytlsw->namebuf, err)) ==
#endif

									(struct pixrect *)0) {
								xv_error((Xv_opaque) mpr,
										ERROR_STRING, err,
										ERROR_PKG, TTY, NULL);
							}
							else {
								Frame frame =
										xv_get(TTY_PUBLIC(ttysw), WIN_FRAME);
								Icon current_icon = xv_get(frame, FRAME_ICON);

								if (current_icon) {
									xv_set(current_icon, ICON_IMAGE, mpr, NULL);
								}
								else {
									icon = xv_create(XV_NULL, ICON,
												ICON_IMAGE, mpr,
												NULL);
									(void)xv_set(xv_get(TTY_PUBLIC(ttysw),
													WIN_FRAME), FRAME_ICON,
											icon, NULL);
								}
							}

#ifdef OW_I18N
							free(mb_filename);
#endif

							break;
						}
					case HS_ICON:{
							Frame frame = xv_get(TTY_PUBLIC(ttysw), WIN_FRAME);

#ifdef OW_I18N
							(void)STRNCPY(iconlabel, ttytlsw->namebuf,
									sizeof(iconlabel) / sizeof(CHAR));
							(void)xv_set(xv_get(frame, FRAME_ICON),
									XV_LABEL_WCS, iconlabel, NULL);
#else
							strncpy(iconlabel, ttytlsw->namebuf,
									sizeof(iconlabel)-1);
							xv_set(xv_get(frame, FRAME_ICON),
									ICON_LABEL, iconlabel,
									NULL);
#endif
						}
						break;
					default:{
						}
				}
				ttytlsw->hdrstate = HS_BEGIN;
			}
			break;
		case HS_FLUSH:
			if (c == '\0')
				ttytlsw->hdrstate = HS_BEGIN;
			break;
		default:
			return (ttysw_ansi_string(ttysw_public, type, c));
	}
	return (TTY_DONE);
}
