#ifndef lint
char     tty_ntfy_c_sccsid[] = "@(#)tty_ntfy.c 20.45 93/06/28 DRA: $Id: tty_ntfy.c,v 4.12 2025/03/21 21:16:25 dra Exp $";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */

/*
 * Notifier related routines for the ttysw.
 */

#include <sys/types.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#ifdef __linux
#  ifndef TIOCSTI
#    include <sys/ioctl.h>
#  endif
#endif /* __linux */
#include <pixrect/pixrect.h>
#include <pixrect/pixfont.h>

#include <xview_private/portable.h>

#include <xview_private/i18n_impl.h>
#include <xview/notify.h>
#include <xview/rect.h>
#include <xview/rectlist.h>
#include <xview/win_input.h>
#include <xview/win_notify.h>
#include <xview/defaults.h>
#include <xview/ttysw.h>
#include <xview/notice.h>
#include <xview/frame.h>
#include <xview/termsw.h>
#include <xview/window.h>
#include <xview/ttysw.h>
#include <xview_private/tty_impl.h>
#include <xview_private/term_impl.h>
#include <xview_private/txt_impl.h>
#include <xview_private/svr_impl.h>
#include <xview_private/ultrix_cpt.h>

#define PTY_OFFSET	(int) &(((Ttysw_private)0)->ttysw_pty)

#include <xview_private/charimage.h>
#include <xview_private/charscreen.h>
#undef length
#define ITIMER_NULL   ((struct itimerval *)0)

/* Duplicate of what's in ttysw_tio.c */

static Notify_value ttysw_pty_output_pending(Tty tty_public, int pty);
static Notify_value ttysw_prioritizer(Tty tty_public, int nfd, fd_set *ibits_ptr, fd_set *obits_ptr, fd_set *ebits_ptr, int nsig, sigset_t *sigbits_ptr, sigset_t *auto_sigbits_ptr, int *event_count_ptr, Notify_event *events, Notify_arg *args);
static Notify_prioritizer_func ttysw_cached_pri;	/* Default prioritizer */

static void cim_resize(Ttysw_view_handle ttysw_view);

/*
 * These three procedures are no longer needed because the pty driver bug
 * that causes the ttysw to lock up is fixed. void add_pty_timer(); void
 * tysw_remove_pty_timer(); Notify_value
 * d();
 */

/* Accelerator to avoid excessive notifier activity */
Xv_private int             ttysw_waiting_for_pty_input;
/* Accelerator to avoid excessive notifier activity */

/* shorthand - Duplicate of what's in ttysw_main.c */

#define	iwbp	ttysw->ttysw_ibuf.cb_wbp
#define	irbp	ttysw->ttysw_ibuf.cb_rbp
#define	iebp	ttysw->ttysw_ibuf.cb_ebp
#define	ibuf	ttysw->ttysw_ibuf.cb_buf
#define	owbp	ttysw->ttysw_obuf.cb_wbp
#define	orbp	ttysw->ttysw_obuf.cb_rbp

Pkg_private void ttysw_interpose(Ttysw_private ttysw_folio)
{
    Tty ttysw_folio_public = TTY_PUBLIC(ttysw_folio);

    notify_set_input_func(ttysw_folio_public,
			   ttysw_pty_input_pending, ttysw_folio->ttysw_pty);
    ttysw_waiting_for_pty_input = 1;
    ttysw_cached_pri = notify_set_prioritizer_func(ttysw_folio_public,
											ttysw_prioritizer);
}


Pkg_private void
ttysw_interpose_on_textsw(textsw)
    Textsw_view     textsw;	/* This is really a termsw view public */
{
    (void) notify_interpose_event_func(
		     (Notify_client) textsw, ttysw_text_event, NOTIFY_SAFE);
    (void) notify_interpose_event_func(
		(Notify_client) textsw, ttysw_text_event, NOTIFY_IMMEDIATE);
#ifdef SUNVIEW1
	extern Notify_value ttysw_text_destroy();	/* Destroy func for termsw */
    (void) notify_interpose_destroy_func(
				(Notify_client) textsw, ttysw_text_destroy);
#endif
}

Pkg_private int ttysw_destroy(Tty self, Destroy_status status)
{
	Ttysw_private priv = TTY_PRIVATE_FROM_ANY_PUBLIC(self);

	if ((status != DESTROY_CHECKING) && (status != DESTROY_SAVE_YOURSELF)) {
		Menu mymenu;
		struct keymaptab *kmt;

		/*
		 * Pty timer is no longer needed because the pty driver bug that
		 * causes the ttysw to lock up is fixed.
		 * ttysw_remove_pty_timer(ttysw);

		 notify_set_itimer_func(self, itimer_expired,
		 ITIMER_REAL, (struct itimerval *) 0, ITIMER_NULL);
		 */

#ifdef SunOS3
		/* Sending both signal is to cover all bases  */
		ttysw_sendsig(ttysw, XV_NULL, SIGTERM);
		ttysw_sendsig(ttysw, XV_NULL, SIGHUP);
#endif
		if ((mymenu = xv_get(self, WIN_MENU))) {
			/* this will be blocked in termsw.c`termsw_set in the
			 * 'destroying'-case
			 */
			xv_set(self, WIN_MENU, XV_NULL, NULL);
			xv_destroy(mymenu);
		}

    	for (kmt = priv->ttysw_kmt; kmt < priv->ttysw_kmtp; kmt++) {
			if (kmt->kmt_to) xv_free(kmt->kmt_to);
			kmt->kmt_to = NULL;
		}

		ttysw_done(priv);
		notify_remove((Notify_client) self);
		notify_remove((Notify_client) priv);
		return XV_OK;
	}
	return XV_OK;
}

Pkg_private void ttysw_sigwinch(Ttysw_private ttysw)
{

    /* if no child, then just return. */
    if (ttysw->ttysw_pidchild==TEXTSW_INFINITY) {
	return;
    }
    /*
     * 2.0 tty based programs relied on getting SIGWINCHes at times other
     * then when the size changed.  Thus, for compatibility, we also do that
     * here.  However, I wish that I could get away with only sending
     * SIGWINCHes on resize.
     */
#ifdef __linux
	{
		Tty pub;
		struct winsize ws;

		pub = ttysw->public_self;
		ws.ws_row = (int)xv_get(pub, WIN_ROWS);
		ws.ws_col = (int)xv_get(pub, WIN_COLUMNS);
		ws.ws_xpixel = (int)xv_get(pub, XV_WIDTH);
		ws.ws_ypixel = (int)xv_get(pub, XV_HEIGHT);

		if (ioctl(ttysw->ttysw_tty, (long)TIOCSWINSZ, &ws))
			perror("ioctl TIOCSWINSZ");
	}
#else /* __linux */
{
    int             pgrp;
    int             sig = SIGWINCH;
    /* Notify process group that terminal has changed. */
    if (ioctl(ttysw->ttysw_tty, TIOCGPGRP, &pgrp) == -1) {
	perror(XV_MSG("ttysw_sigwinch, can't get tty process group"));
	return;
    }
    /*
     * Only killpg when pgrp is not tool's.  This is the case of haven't
     * completed ttysw_fork yet (or even tried to do it yet).
     */
    if (getpgrp(0) != pgrp)
	/*
	 * killpg could return -1 with errno == ESRCH but this is OK.
	 */
#ifndef sun
	(void)kill(-pgrp, SIGWINCH);
#else
	ioctl(ttysw->ttysw_pty, TIOCSIGNAL, &sig);
#endif
}
#endif /* __linux */
    return;
}

Pkg_private void ttysw_sendsig(Ttysw_private ttysw, Termsw_view tswv, int sig)
{
    int             control_pg;

    /* if no child, then just return. */
    if (ttysw->ttysw_pidchild==TEXTSW_INFINITY) {
		return;
    }
    /* Send the signal to the process group of the controlling tty */
#ifdef __linux
    /* Under Linux, we can use this ioctl only on the master pty,
     * otherwise we'll get ENOTTY. It seems to return the right process
     * group nevertheless.
     */
    if (ioctl(ttysw->ttysw_pty, (long)TIOCGPGRP, &control_pg) >= 0) {
#else
    if (ioctl(ttysw->ttysw_tty, TIOCGPGRP, &control_pg) >= 0) {
#endif
	/*
	 * Flush our buffers of completed and partial commands. Be sure to do
	 * this BEFORE killpg, or we'll flush the prompt coming back from the
	 * shell after the process dies.
	 */
	ttysw_flush_input(ttysw);

	if (tswv) {
		Termsw trm = xv_get(tswv, XV_OWNER);
	    Termsw_folio    termsw = TERMSW_PRIVATE(trm);

	    ttysw_move_mark(trm, &termsw->pty_mark,
			    (Textsw_index) xv_get(trm, TEXTSW_LENGTH_I18N),
			    TEXTSW_MARK_DEFAULTS);
	}
	if (TTY_IS_TERMSW(ttysw)) {
	    Termsw_folio    termsw =
	    TERMSW_FOLIO_FOR_VIEW(TERMSW_VIEW_PRIVATE_FROM_TTY_PRIVATE(ttysw));

	    termsw->cmd_started = 0;
	    termsw->pty_owes_newline = 0;
	}
#	if defined(XV_USE_SVR4_PTYS) || defined(sun)
#ifdef TIOCSIGNAL
	if (ioctl(ttysw->ttysw_pty, TIOCSIGNAL, &sig) < 0) {
		if (kill(-control_pg, sig)) perror("after failed ioctl trying kill");
	}
#else /* TIOCSIGNAL */
	killpg(control_pg, sig);
#endif /* TIOCSIGNAL */

#	else
	(void) killpg(control_pg, sig);
#	endif
    }
	else {
		perror(XV_MSG("ioctl TIOCGPGRP"));
#ifndef DRA_TIOCREMOTE
/* 		{ */
/* #include <sys/stat.h> */
/* 			struct stat sb; */
/*  */
/* 			fprintf(stderr, "ttysw_tty = %d\n", ttysw->ttysw_tty); */
/* 			if (fstat(ttysw->ttysw_tty, &sb)) perror(" my fstat"); */
/* 			else { */
/* 				fprintf(stderr, "mode=0%o, dev=0x%x\n", sb.st_mode, sb.st_dev); */
/* 			} */
/* 		} */
#endif /* DRA_TIOCREMOTE */
	}
}

/* ARGSUSED */
/* BUG ALERT: Why was this marked Xv_public in V3.0?  Should be Pkg_private. */
Xv_public Notify_value ttysw_event(Tty_view ttysw_view_public, Notify_event ev,
							Notify_arg arg, Notify_event_type type)
{
	Event *event = (Event *) ev;
	Ttysw_private ttysw_folio_private =
								TTY_PRIVATE_FROM_ANY_VIEW(ttysw_view_public);

	if ((*(ttysw_folio_private)->ttysw_eventop) (ttysw_view_public, event)
											== TTY_DONE)
#ifdef OW_I18N
	{
		/*
		 * window pkg needs those two events to set/unset IC focus.
		 */
		if (event_action(event) == KBD_USE || event_action(event) == KBD_DONE)
			return notify_next_event_func(ttysw_view_public, ev, arg, type);
		else
			return (NOTIFY_DONE);
	}
#else
	{
		return (NOTIFY_DONE);
	}
#endif

	else {
		return (NOTIFY_IGNORED);
	}
}

Pkg_private void ttysw_display(Ttysw_private ttysw, Event *ie)
{
	if (ttysw_getopt(ttysw, TTYOPT_TEXT)) {
		textsw_display(TEXTSW_FROM_TTY(ttysw));
	}
	else {
		(void)ttysw_prepair(event_xevent(ie));
		/* primary selection is repainted in ttysw_prepair. */
		if (ttysw->sels[TTY_SEL_SECONDARY].sel_made)
			ttyhiliteselection(ttysw, ttysw->sels + TTY_SEL_SECONDARY, TTY_SEL_SECONDARY);
	}
}

static Notify_value ttysw_pty_output_pending(Tty tty_public, int pty)
{
    ttysw_pty_output(TTY_PRIVATE_FROM_ANY_PUBLIC(tty_public), pty);
    return (NOTIFY_DONE);
}

Pkg_private Notify_value ttysw_pty_input_pending(Tty tty_public, int pty)
{
    ttysw_pty_input(TTY_PRIVATE_FROM_ANY_PUBLIC(tty_public), pty);
    return NOTIFY_DONE;
}

static Notify_value itimer_expired(Tty tty_public, int which)
{
	SERVERTRACE((567, "%s\n", __FUNCTION__));
    notify_set_itimer_func(tty_public, NOTIFY_TIMER_FUNC_NULL, ITIMER_REAL,
					(struct itimerval *) 0, (struct itimerval *) 0);
    ttysw_handle_itimer(TTY_PRIVATE_FROM_ANY_PUBLIC(tty_public));
    return NOTIFY_DONE;
}

static Termsw_folio from_ttysw_folio(Ttysw_private ttysw)
{
	Tty  tty = XV_PUBLIC(ttysw);
	Termsw_view_handle vh;

/* 	fprintf(stderr, "%s-%d: tty is a %s\n", __FUNCTION__,__LINE__, */
/* 				((Xv_pkg *)xv_get(tty, XV_TYPE))->name); */

	if (IS_TERMSW(tty)) {
		vh = termsw_first_view_private(TERMSW_PRIVATE(tty));
		return vh->folio;
	}
#ifdef DAS_IST_BLOEDSINN
	... aber fuehrt in valgrind zu 'Invalid read of size 8'
	else {
		/* das ist ja schon Quatsch */
		Xv_opaque ttyv = TTY_VIEW_PUBLIC(ttysw);

		/* ttyv ist natuerlich IMMER noch ein Tty - jetzt wird's voellig
		 * abartig:
		 */
		vh = TERMSW_VIEW_PRIVATE(ttyv);
	}
#endif /* DAS_IST_BLOEDSINN */

	/* das ist ein Versuch - der Aufrufer fragt brav ab.... */
	/* damit stuerzt vitool nicht gleich beim Starten ab */
	return NULL;
}

#define	TTYSW_USEC_DELAY 100000

/*
 * Conditionally set conditions
 */
Pkg_private void ttysw_reset_conditions(Ttysw_view_handle ttysw_view)
{
	static int ttysw_waiting_for_pty_output;
	register Ttysw_private ttysw = ttysw_view->folio;
	register int pty = ttysw->ttysw_pty;
	Tty ttypub = TTY_PUBLIC(ttysw);
	Termsw_folio termsw;

	/* Send program output to terminal emulator */
	ttysw_consume_output(ttysw_view);
	/* Toggle between window input and pty output being done */

	termsw = from_ttysw_folio(ttysw);

	if ((iwbp > irbp && ttysw_pty_output_ok(ttysw)) ||
			(ttysw_getopt(ttysw, TTYOPT_TEXT) && termsw != NULL &&
					termsw->pty_eot > -1)) {
		if (!ttysw_waiting_for_pty_output) {
			/* Wait for output to complete on pty */

			notify_set_output_func(ttypub, ttysw_pty_output_pending, pty);
			ttysw_waiting_for_pty_output = 1;
			/*
			 * Pty timer is no longer needed because the pty driver bug that
			 * causes the ttysw to lock up is fixed.
			 * (void)ttysw_add_pty_timer(ttysw, &pty_itimerval);
			 */
			/* what was that 'pty driver bug' ???? Probably something
			 * on SunOS !? Maybe also under Linux? But I don't know
			 * what that ttysw_add_pty_timer did.....
			 */
		}
	}
	else {
		if (ttysw_waiting_for_pty_output) {
			/* Don't wait for output to complete on pty any more */
			notify_set_output_func(ttypub, NOTIFY_IO_FUNC_NULL, pty);
			ttysw_waiting_for_pty_output = 0;
		}
	}
	/* Set pty input pending */
	if (owbp == orbp) {
		if (!ttysw_waiting_for_pty_input) {
			notify_set_input_func(ttypub, ttysw_pty_input_pending, pty);
			ttysw_waiting_for_pty_input = 1;
		}
	}
	else {
		if (ttysw_waiting_for_pty_input) {
			notify_set_input_func(ttypub, NOTIFY_IO_FUNC_NULL, pty);
			ttysw_waiting_for_pty_input = 0;
		}
	}
	/*
	 * Try to optimize displaying by waiting for image to be completely
	 * filled after being cleared (vi(^F ^B) page) before painting.
	 */
	if (!ttysw_getopt(ttysw, TTYOPT_TEXT) && ttysw_delaypainting) {
		static struct itimerval timer = {
			{ 9999 /* linux-bug, seems to be ignored */ , 0 },
			{ 0, 3 * TTYSW_USEC_DELAY }
		};
		SERVERTRACE((567, "%s: start timer\n", __FUNCTION__));
		notify_set_itimer_func(ttypub, itimer_expired,
							ITIMER_REAL, &timer, ITIMER_NULL);
	}
}



static Notify_value ttysw_prioritizer(Tty tty_public, int nfd,
			fd_set *ibits_ptr, fd_set *obits_ptr, fd_set *ebits_ptr,
			int nsig, sigset_t *xsigbits_ptr, sigset_t *xauto_sigbits_ptr,
			int *event_count_ptr, Notify_event *events, Notify_arg *args)
/* Called directly from notify_client(), so tty_public may be termsw! */
{
	Ttysw_private ttysw = TTY_PRIVATE_FROM_ANY_PUBLIC(tty_public);
	Ttysw_view_handle ttysw_view = TTY_VIEW_HANDLE_FROM_TTY_FOLIO(ttysw);
	register int pty = ttysw->ttysw_pty;
	register int i;
	int count = *event_count_ptr;
	int *auto_sigbits_ptr = (int *)xauto_sigbits_ptr;

/* 	fprintf(stderr, "%s-%d: tty_public is a %s\n", __FUNCTION__,__LINE__, */
/* 				((Xv_pkg *)xv_get(tty_public, XV_TYPE))->name); */

	ttysw->ttysw_flags |= TTYSW_FL_IN_PRIORITIZER;
	if (*auto_sigbits_ptr) {
		/* Send itimers */
		if (*auto_sigbits_ptr & SIG_BIT(SIGALRM)) {
			notify_itimer(tty_public, ITIMER_REAL);
			*auto_sigbits_ptr &= ~SIG_BIT(SIGALRM);
		}
	}
	if (FD_ISSET(ttysw->ttysw_tty, obits_ptr)) {
		notify_output(tty_public, ttysw->ttysw_tty);
		FD_CLR(ttysw->ttysw_tty, obits_ptr);
	}
	/*
	 * Post events. This is done in place of calling notify_input with wfd's
	 */
	for (i = 0; i < count; i++) {
		notify_event(tty_public, events[i], args[i]);
	}

	if (FD_ISSET(pty, obits_ptr)) {
		notify_output(tty_public, pty);
		FD_CLR(pty, obits_ptr);
		/*
		 * Pty timer is no longer needed because the
		 * (void)ttysw_remove_pty_timer(ttysw);
		 */
	}
	if (FD_ISSET(pty, ibits_ptr)) {
		/* This is aviod the race condition, created by the timer flush */

		if (IS_TERMSW(tty_public)
				&& (ttysw_getopt(ttysw, TTYOPT_TEXT))) {
			textsw_flush_std_caches(TTY_VIEW_PUBLIC(ttysw_view));
		}
		notify_input(tty_public, pty);
		FD_CLR(pty, ibits_ptr);
	}
	ttysw_cached_pri(tty_public, nfd, ibits_ptr, obits_ptr, ebits_ptr,
			nsig, xsigbits_ptr, (sigset_t *) auto_sigbits_ptr, event_count_ptr,
			events, args);
	ttysw_reset_conditions(ttysw_view);
	ttysw->ttysw_flags &= ~TTYSW_FL_IN_PRIORITIZER;

	return NOTIFY_DONE;
}

Pkg_private void ttysw_resize(Ttysw_view_handle ttysw_view)
{
	Ttysw_private ttysw = TTY_FOLIO_FROM_TTY_VIEW_HANDLE(ttysw_view);
	int pagemode;

	/*
	 * Turn off page mode because send characters through character image
	 * manager during resize.
	 */
	pagemode = ttysw_getopt(ttysw, TTYOPT_PAGEMODE);
	ttysw_setopt(ttysw, TTYOPT_PAGEMODE, 0);
	if (ttysw_getopt(ttysw, TTYOPT_TEXT)) {
		xv_tty_new_size(ttysw,
				textsw_screen_column_count(TTY_VIEW_PUBLIC(ttysw_view)),
				textsw_screen_line_count(TTY_VIEW_PUBLIC(ttysw_view)));
	}
	else {
		/* Have character image update self */
		csr_resize(ttysw_view);
		/* Have screen update any size change parameters */
		cim_resize(ttysw_view);
		if (TTY_IS_TERMSW(ttysw)) {
			Termsw_private *termsw =
					TERMSW_VIEW_PRIVATE_FROM_TTY_PRIVATE(ttysw);

			termsw->folio->ttysw_resized++;
		}
	}
	/* Turn page mode back on */
	ttysw_setopt(ttysw, TTYOPT_PAGEMODE, pagemode);
}

static void cim_resize(Ttysw_view_handle ttysw_view)
{
    struct rectlist rl;

    /* Prevent any screen writing by making clipping null */
    rl = rl_null;
    win_set_clip(TTY_VIEW_PUBLIC(ttysw_view), &rl);
    /* Redo character image */
    (void) ttysw_imagerepair(ttysw_view);
    /* Restore normal clipping */
    win_set_clip(TTY_VIEW_PUBLIC(ttysw_view), RECTLIST_NULL);
}

/* BUG ALERT: No XView prefix */
Pkg_private void
csr_resize(ttysw_view)
    Ttysw_view_handle ttysw_view;
{
    Rect           *r_new = (Rect *) xv_get(TTY_VIEW_PUBLIC(ttysw_view), WIN_RECT);
    Ttysw_private     ttysw = TTY_FOLIO_FROM_TTY_VIEW_HANDLE(ttysw_view);

    /* Update notion of size */
    ttysw->winwidthp = r_new->r_width;
    ttysw->winheightp = r_new->r_height;
    /* Don't currently support selections across size changes */
    ttynullselection(ttysw);
}
