#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)win_bell.c 20.20 93/06/28 DRA: $Id: win_bell.c,v 4.1 2024/03/28 19:28:19 dra Exp $";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Win_bell.c: Implement the keyboard bell.
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <pixrect/pixrect.h>
#include <xview/base.h>
#include <xview/pixwin.h>
#include <xview/defaults.h>
#include <xview/rect.h>
#include <X11/Xlib.h>
#include <xview_private/draw_impl.h>
#include <xview_private/win_info.h>
#include <xview_private/ndet.h>

/* Bell cached defaults */
static Bool     win_do_audible_bell;
static Bool     win_do_visible_bell;
static int      win_bell_done_init;

static Defaults_pairs bell_types[] = {
	{ "never",   0 },
	{ "notices", 0 }, 
	{ "always",  1 },
	{ NULL,      1 }
};

/*
 * Ring bell and flash window
 */
void win_bell(Xv_object window, struct timeval tv, Xv_object pw)
{
	Xv_Drawable_info *info;
	Display *display;
	Rect r;

	DRAWABLE_INFO_MACRO(window, info);
	display = xv_display(info);
	/* Get defaults for bell if  first time */
	if (!win_bell_done_init) {
		win_do_audible_bell = defaults_get_enum("openWindows.beep",
				"OpenWindows.Beep", bell_types);
		win_do_visible_bell =
				(Bool) defaults_get_boolean("alarm.visible", "Alarm.Visible",
				(Bool) TRUE);
		win_bell_done_init = 1;
	}
	/* Flash pw */
	if (pw && win_do_visible_bell) {
		(void)win_getsize(window, &r);
		(void)pw_writebackground(pw, 0, 0,
				r.r_width, r.r_height, PIX_NOT(PIX_DST));
	}

	/* Ring bell */
	/* BUG: is this right? */
	if (win_do_audible_bell)
		win_beep(display, tv);

	/* Turn off flash */
	if (pw && win_do_visible_bell) {
		(void)pw_writebackground(pw, 0, 0,
				r.r_width, r.r_height, PIX_NOT(PIX_DST));
	}
}

static void win_blocking_wait(struct timeval wait_tv)
{
	struct timeval start_tv, now_tv, waited_tv;
	fd_set bits, bits2, bits3;

	/* Get starting time */
	(void)gettimeofday(&start_tv, (struct timezone *)0);
	/* Wait */
	while (timerisset(&wait_tv)) {
		/* Wait for awhile in select */
		select(0, &bits, &bits2, &bits3, &wait_tv);
		/* Get current time */
		gettimeofday(&now_tv, (struct timezone *)0);
		/* Compute how long waited */
		waited_tv = ndet_tv_subt(now_tv, start_tv);
		/* Subtract time waited from time left to wait */
		wait_tv = ndet_tv_subt(wait_tv, waited_tv);
	}
}

/*
 * win_beep - actually cause the bell to sound
 */
Xv_private void win_beep(Display *display, struct timeval tv)
{
	XBell(display, 100);
	win_blocking_wait(tv);
}
