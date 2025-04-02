/*      @(#)tty_impl.h 20.37 93/06/28 SMI dra: $Id: tty_impl.h,v 4.26 2025/04/01 12:51:12 dra Exp $ */

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */

#ifndef _xview_private_ttysw_impl_h_already_included
#define _xview_private_ttysw_impl_h_already_included

/*
 * A tty subwindow is a subwindow type that is used to provide a
 * terminal emulation for teletype based programs.
 */

#include <xview_private/portable.h>	/* tty and pty configuration info */

#ifdef	XV_USE_TERMIOS
#  define SNI 1
#include <termios.h>		/* for POSIX-style tty state structure */
#else
#include <sys/ioctl.h>		/* for BSD-style tty state structures */
#endif

#include <xview/tty.h>
#include <pixrect/pixrect.h>
#include <pixrect/pixfont.h>
#include <xview/textsw.h>
#include <xview_private/i18n_impl.h>
#include <xview_private/es.h>
#include <xview/sel_pkg.h>
#include <xview/cursor.h>

#define TTY_PRIVATE(_t)      XV_PRIVATE(Ttysw, Xv_tty, _t)
#define TTY_PUBLIC(_tty_folio)     XV_PUBLIC(_tty_folio)

#define TTY_VIEW_PRIVATE(_t)     	 XV_PRIVATE(Ttysw_view_object, Xv_tty_view, _t)
#define TTY_VIEW_PUBLIC(_tty_view)	 XV_PUBLIC(_tty_view)

#define IS_TTY(_t) \
	(((Xv_base *)(_t))->pkg == TTY)

#define IS_TTY_VIEW(_t) \
	(((Xv_base *)(_t))->pkg == TTY_VIEW)

/* BUG: This could be made cleaner.  See
 * TERMSW_FOLIO_FROM_TERMSW_VIEW_HANDLE in termsw_impl.h
 */
#define TTY_FOLIO_FROM_TTY_VIEW_HANDLE(_tty_view_private) \
	 ((Ttysw_private)(((Ttysw_view_handle)_tty_view_private)->folio))

#define TTY_FOLIO_FROM_TTY_VIEW(_tty_view_public) 	\
	 ((Ttysw_private)					\
	  (((Ttysw_view_handle) TTY_VIEW_PRIVATE(_tty_view_public))->folio))

#define TTY_FROM_TTY_VIEW(_tty_view_public) 		\
	((Tty) TTY_PUBLIC(TTY_FOLIO_FROM_TTY_VIEW(_tty_view_public)))

/*
 * These are the data structures internal to the tty subwindow
 * implementation.  They are considered private to the implementation.
 */

struct cbuf {
    CHAR               *cb_rbp;    /* read pointer */
    CHAR               *cb_wbp;    /* write pointer */
    CHAR               *cb_ebp;    /* end of buffer */
    CHAR                cb_buf[2048];
};

struct input_cbuf {
    CHAR               *cb_rbp;    /* read pointer */
    CHAR               *cb_wbp;    /* write pointer */
    CHAR               *cb_ebp;    /* end of buffer */
    CHAR                cb_buf[8192];
};

struct keymaptab {
    int                 kmt_key;
    int                 kmt_output;
    char               *kmt_to;
};

struct textselpos {
    int			tsp_row;
    int			tsp_col;
#ifdef  OW_I18N
    int			tsp_charpos;
#endif
};

struct ttyselection {
    int                 sel_made;  /* a selection has been made */
    int                 sel_null;  /* the selection is null */
    int                 sel_level; /* see below */
	int 				is_word;
    int                 sel_anchor;/* -1 = left, 0 = none, 1 = right */
    struct textselpos   sel_begin; /* beginning of selection */
    struct textselpos   sel_end;   /* end of selection */
    struct timeval      sel_time;  /* time selection was made */
    int	                selrank;  /* type of selection. primary or secondary */
    int			dehilite_op;  /* Operation for taking down selection */
};

/* selection levels */
#define	SEL_CHAR	0
#define	SEL_WORD	1
#define	SEL_LINE	2
#define	SEL_PARA	3
#define	SEL_MAX		3

#define TTY_SEL_PRIMARY	0
#define TTY_SEL_SECONDARY	1
#define TTY_SEL_CLIPBOARD	2
#define NBR_TTY_SELECTIONS	3
extern struct ttyselection	null_ttyselection;

enum ttysw_hdrstate { HS_BEGIN, HS_HEADER, HS_ICON, HS_ICONFILE, HS_FLUSH };

typedef struct ttysubwindow {
    Tty			public_self;		/* Back pointer to the object*/
    struct ttysw_view_object 			/* View window */
    			*view;			/* (Pure tty has only one view) */
    Tty_view	current_view_public; /* This keep trace of the view become ttysw */
    unsigned	ttysw_flags;
    /* common */
    int                 ttysw_opt;		/* option mask; see ttysw.h */
    struct input_cbuf   ttysw_ibuf;		/* input buffer */
    struct cbuf         ttysw_obuf;		/* output buffer */
	int OVERWRITTEN;/* for the purpose of this: see ttyansi.c`ttysw_output_it */
    /* pty and subprocess */
    int                 ttysw_pty;	/* master (pty) file descriptor */
    int                 ttysw_tty;	/* slave (tty) file descriptor */
    char		tty_name[20];	/* slave (tty) file name */
    int                 ttysw_ttyslot;		/* ttyslot in utmp for tty */
    /* saved tty mode information: see access functions below */
#   ifdef XV_USE_TERMIOS
    struct termios	termios;
#   else /* XV_USE_TERMIOS */
    struct sgttyb	sgttyb;
    struct tchars	tchars;
    struct ltchars	ltchars;
#   endif /* XV_USE_TERMIOS */
    /*
     * The next two fields record the current pty remote mode state and the
     * remote mode state to which it should be set before processing the next
     * input character.
     */
    int			remote;
    int			pending_remote;
    /* page mode */
    int                 ttysw_lpp;		/* page mode: lines per page */
    /* subprocess */
    int                 ttysw_pidchild;		/* pid of the child */
    /* Caps Lock */
    int                 ttysw_capslocked;
#define TTYSW_CAPSLOCKED	0x01	/* capslocked on mask bit */
#define TTYSW_CAPSSAWESC	0x02	/* saw escape while caps locked */
    /* stuff from old ttytlsw */
    enum ttysw_hdrstate	hdrstate;		/* string trying to load */
    CHAR		*nameptr;               /* namebuf ptr */
    CHAR		namebuf[256];           /* accumulates esc string */
    /* selection */
    int                 ttysw_butdown;		/* which button is down */
    struct ttyselection	sels[NBR_TTY_SELECTIONS];
    /* replaceable ops (return TTY_OK or TTY_DONE) */
    int                 (*ttysw_escapeop) (Tty_view, int, int, int *);	/* handle escape sequences */
    int                 (*ttysw_stringop) (Tty, int, int);	/* handle accumulated string */
    int                 (*ttysw_eventop) (Tty, Event *);	/* handle input event */
    /* kbd translation */
    struct keymaptab    ttysw_kmt[3 * 16 + 2];	/* Key map list */
    struct keymaptab   *ttysw_kmtp;		/* next empty ttysw_kmt slot */
    window_layout_proc_t layout_proc; /* interposed window layout proc */
#ifdef  OW_I18N
    int                 im_first_col;
    int                 im_first_row;
    int                 im_len;
    wchar_t             *im_store;
    XIMFeedback         *im_attr;
    Bool                preedit_state;
    XIC                 ic;
    int			implicit_commit;

    XIMCallback     	start_pecb_struct;
    XIMCallback     	draw_pecb_struct;
    XIMCallback     	done_pecb_struct;
#ifdef FULL_R5
    XIMStyle	xim_style;
#endif /* FULL_R5 */
#endif
    int			pass_thru_modifiers;  /* Modifiers we don't interpret */
    int			eight_bit_output; /* Print eight bit characters? */

	int current_sel;  /* TTY_SEL_PRIMARY or TTY_SEL_SECONDARY */

	unsigned bufsize;
	char *selbuffer;

    Selection_item	sel_item[NBR_TTY_SELECTIONS];
    Selection_owner	sel_owner[NBR_TTY_SELECTIONS];
	long sel_reply;
	Atom selection_end;
	Atom seln_yield;

	/* formerly static variables */
	int point_down_within_selection;
	short dnd_last_click_x, dnd_last_click_y;

	/* formerly GLOBAL variables */
	CHAR **image;
	char **screenmode;
	int	ttysw_top, ttysw_bottom, ttysw_left, ttysw_right;
	int	cursrow, curscol;
	int do_cursor_draw;
	int cursor;
	int	chrheight, chrwidth, chrbase;
	int	winheightp, winwidthp;
	int	chrleftmargin;
	char boldify;
	struct pixfont *pixfont;
	int tty_new_cursor_row, tty_new_cursor_col;
}   Ttysw;

typedef Ttysw		*Ttysw_private;

typedef struct ttysw_view_object {
    Tty_view		public_self;
    Ttysw_private	folio;
	Xv_Cursor       ttysw_stop_cursor;	/* stop sign cursor (i.e., CTRL-S) */
	Xv_Cursor       ttysw_cursor;	/* stop sign cursor (i.e., CTRL-S) */
	struct pixfont *pixfont;
} Ttysw_view_object;

typedef Ttysw_view_object* 	Ttysw_view_handle;

/* Values for ttysw_flags */
#define TTYSW_FL_FROZEN			0x1
#define TTYSW_FL_IS_TERMSW		0x2
#define TTYSW_FL_IN_PRIORITIZER		0x4

/*
 * Functions, macros, and typedefs for abstracting away differences between
 * termios and old BSD-style tty mode representations.
 */
/*
 * Access functions for tty characteristics.
 */
#ifdef	XV_USE_TERMIOS
#define	tty_gettabs(t)		((t)->termios.c_oflag & XTABS)
#if !defined(__linux) || defined(VDSUSP)
#define	tty_getdsuspc(t)	((int) ((t)->termios.c_cc[VDSUSP]))
#else
#define	tty_getdsuspc(t)	((int) -1)
#endif
#define	tty_geteofc(t)		((int) ((t)->termios.c_cc[VEOF]))
#define	tty_geteolc(t)		((int) ((t)->termios.c_cc[VEOL]))
#define	tty_geteol2c(t)		((int) ((t)->termios.c_cc[VEOL2]))
#define	tty_getintrc(t)		((int) ((t)->termios.c_cc[VINTR]))
#define	tty_getlnextc(t)	((int) ((t)->termios.c_cc[VLNEXT]))
#define	tty_getquitc(t)		((int) ((t)->termios.c_cc[VQUIT]))
#define	tty_getrprntc(t)	((int) ((t)->termios.c_cc[VREPRINT]))
#define	tty_getstartc(t)	((int) ((t)->termios.c_cc[VSTART]))
#define	tty_getstopc(t)		((int) ((t)->termios.c_cc[VSTOP]))
#define	tty_getsuspc(t)		((int) ((t)->termios.c_cc[VSUSP]))
#else	/* XV_USE_TERMIOS */
#define	tty_gettabs(t)		((t)->sgttyb.sg_flags & XTABS)
#define	tty_getdsuspc(t)	((int) ((t)->ltchars.t_dsuspc))
#define	tty_geteofc(t)		((int) ((t)->tchars.t_eofc))
#define	tty_geteolc(t)		((int) ((t)->tchars.t_brkc))
#define	tty_geteol2c(t)		((int) ((t)->tchars.t_brkc))
#define	tty_getintrc(t)		((int) ((t)->tchars.t_intrc))
#define	tty_getlnextc(t)	((int) ((t)->ltchars.t_lnextc))
#define	tty_getquitc(t)		((int) ((t)->tchars.t_quitc))
#define	tty_getrprntc(t)	((int) ((t)->ltchars.t_rprntc))
#define	tty_getstartc(t)	((int) ((t)->tchars.t_startc))
#define	tty_getstopc(t)		((int) ((t)->tchars.t_stopc))
#define	tty_getsuspc(t)		((int) ((t)->ltchars.t_suspc))
#endif	/* XV_USE_TERMIOS */
/*
 * Predicates for tty characteristics.
 */
#ifdef	XV_USE_TERMIOS
#define	tty_iscanon(t)		(((t)->termios.c_lflag & ICANON) != 0)
#define	tty_isecho(t)		(((t)->termios.c_lflag & ECHO  ) != 0)
#define tty_issig(t)		(((t)->termios.c_lflag & ISIG  ) != 0)
#else	/* XV_USE_TERMIOS */
#define	tty_iscanon(t)		(((t)->sgttyb.sg_flags & (RAW|CBREAK)) == 0)
#define	tty_isecho(t)		(((t)->sgttyb.sg_flags & ECHO) != 0)
#define tty_issig(t)		(((t)->sgttyb.sg_flags & RAW) == 0)
#endif	/* XV_USE_TERMIOS */
/*
 * Capture fd's current tty modes and store them in *mode.
 */
#ifdef	XV_USE_TERMIOS
#define	tty_mode	termios		/* Ttysw field alias (ugh!) */
typedef struct termios	tty_mode_t;
#else	/* XV_USE_TERMIOS */
#define	tty_mode	sgttyb		/* Ttysw field alias (ugh!) */
typedef struct sgttyb	tty_mode_t;
#endif	/* XV_USE_TERMIOS */

/*
 * Determine where to store tty characteristics in the environment.  To avoid
 * possible misinterpretation, we use different locations depending on whether
 * or not XV_USE_TERMIOS is set.
 */
#ifdef	XV_USE_TERMIOS
#define	WE_TTYPARMS	"WINDOW_TERMIOS"
#define	WE_TTYPARMS_E	"WINDOW_TERMIOS="
#else	/* XV_USE_TERMIOS */
#define	WE_TTYPARMS	"WINDOW_TTYPARMS"
#define	WE_TTYPARMS_E	"WINDOW_TTYPARMS="
#endif	/* XV_USE_TERMIOS */


#define TTYSW_NULL      ((Ttysw *)0)

/*
 * Possible return codes from replaceable ops.
 */
#define	TTY_OK		(0)	   /* args should be handled as normal */
#define	TTY_DONE	(1)	   /* args have been fully handled */

#define	ttysw_handleevent(ttysw, ie) \
	(*(ttysw)->ttysw_eventop)(TTY_PUBLIC(ttysw), (ie))
#define	ttysw_handleescape(_ttysw_view, c, ac, av) \
	(*(ttysw)->ttysw_escapeop)(TTY_VIEW_PUBLIC(_ttysw_view), (c), (ac), (av))
#define	ttysw_handlestring(ttysw, strtype, c) \
	(*(ttysw)->ttysw_stringop)(TTY_PUBLIC(ttysw), (strtype), (c))


/*** XView private routines ***/
Xv_private void
	tty_background(Xv_opaque window,int x,int y,int w,int h,int op),
	tty_copyarea(Xv_opaque window, int sX, int sY, int W, int H, int dX,int dY),
	tty_newtext(Xv_opaque window, int xbasew, int ybasew, int op, Xv_opaque pixfont, CHAR *string, int len)
	;

Xv_private void tty_clear_clip_rectangles(Xv_opaque window);

#define MAX_LINES 128
typedef struct {
	int	caret_line_exposed:1;
	int	caret_line;
	int	leftmost;
	char	line_exposed[MAX_LINES];
} Tty_exposed_lines;
Xv_private Tty_exposed_lines *tty_calc_exposed_lines(Xv_window, XEvent *, int);
Xv_private int ttysw_view_obscured;


struct _Termsw_folio_object;

/*** Package private routines ***/

Pkg_private void ttysw_restore_cursor(Ttysw_private ttysw);
Pkg_private void ttysw_clear( Ttysw *ttysw);
Pkg_private void termsw_caret_cleared(void);
Pkg_private void ttysw_interpose(Ttysw_private ttysw_folio);
Pkg_private void ttysw_display(Ttysw_private ttysw, Event *ie);

Pkg_private void csr_resize(Ttysw_view_handle ttysw_view);/* BUG ALERT: No XView prefix */
Pkg_private void ttysel_init_client(Ttysw_private ttysw);
Pkg_private void ttysel_destroy(struct ttysubwindow *ttysw);
Pkg_private void ttysel_make(struct ttysubwindow *ttysw, struct inputevent *event, int multi);
Pkg_private void ttysel_finish(Ttysw_private priv, Event *ev);
Pkg_private void ttysel_adjust(struct ttysubwindow *ttysw, struct inputevent *event, int multi, int ok_to_extend);
Pkg_private void ttysel_deselect(Ttysw_private ttysw, struct ttyselection *ttysel, int rank);
Pkg_private void ttynullselection(struct ttysubwindow *ttysw);
Pkg_private int ttysw_do_copy(Ttysw_private ttysw);
Pkg_private int ttysw_do_paste(Ttysw_private ttysw);
Pkg_private void ttysw_consume_output(Ttysw_view_handle ttysw_view);
Pkg_private void ttysw_handle_itimer(Ttysw_private ttysw);
Pkg_private void ttysw_flush_input(Ttysw_private ttysw);
Pkg_private void termsw_menu_set(void);
Pkg_private void termsw_menu_clr(void);
Pkg_private void ttysw_readrc(struct ttysubwindow *ttysw);
Pkg_private void ttysw_display_capslock(struct ttysubwindow *ttysw);
Pkg_private void ttysw_getp(Ttysw_view_handle ttysw_view);
Pkg_private void ttysw_doing_pty_insert(Textsw textsw, struct _Termsw_folio_object *commandsw, int toggle);
Pkg_private int ttysw_eventstd(Tty_view ttysw_view_public, Event *ie);

Pkg_private void
	ttysw_delete_lines(Ttysw_private ttysw, int where, int n),
	ttysel_getselection(Xv_opaque UNKNOWN),
	ttysel_nullselection(Xv_opaque UNKNOWN),
	ttysel_setselection(Xv_opaque UNKNOWN),
	ttysw_ansiinit(struct ttysubwindow *ttysw),
	ttysw_blinkscreen(void),
	ttysw_bold_mode(Ttysw_private ttysw),
	ttysw_cim_clear(Ttysw_private ttysw, int a, int b),
	ttysw_cim_scroll(Ttysw_private ttysw, int n),
	ttysw_clear_mode(Ttysw_private ttysw),
	ttysw_deleteChar(Ttysw_private ttysw, int fromcol, int tocol, int row),
	ttysw_drawCursor(Ttysw_private ttysw, int yChar, int xChar),
	ttysw_imagerepair(Ttysw_view_handle ttysw_view),
	ttysw_implicit_commit(Xv_opaque UNKNOWN),
	ttysw_insertChar(Ttysw_private ttysw, int fromcol, int tocol, int row),
	ttysw_insert_lines(Ttysw_private ttysw, int where, int n),
	ttysw_inverse_mode(Ttysw_private ttysw),
	ttysw_pcopyscreen(Ttysw_private ttysw, int fromrow, int torow, int count),
	ttysw_pdisplayscreen(Ttysw_private ttysw, int dontrestorecursor,
											int allowclearareawhenempty),
	ttysw_pos(Ttysw_private ttysw, int, int),
	ttysw_prepair(XEvent *eventp),
	ttysw_pselectionhilite (struct rect *r, int sel_rank),
	ttysw_removeCursor(Ttysw_private ttysw),
	ttysw_restoreCursor(Ttysw_private ttysw),/* BUG ALERT: unnecessary routine*/
	ttysw_saveCursor(Xv_opaque UNKNOWN),	/* BUG ALERT: unnecessary routine */
	ttysw_screencomp(void),	/* BUG ALERT: unnecessary routine */
	ttysw_set_inverse_mode(int new_inverse_mode),
	ttysw_set_menu(Xv_opaque UNKNOWN),
	ttysw_set_underline_mode( int new_underline_mode),
	xv_tty_free_image_and_mode(Ttysw_private ttysw),
	xv_tty_imagealloc(Ttysw *ttysw, int for_temp);

Pkg_private void xv_new_tty_chr_font(Ttysw_private ttysw,
#ifdef OW_I18N
    Xv_opaque	font
#else
    Pixfont    	*font
#endif
);

Pkg_private void ttysw_textsw_changed(Textsw textsw, Attr_avlist attributes);
Pkg_private void ttysw_show_walkmenu(Tty_view anysw_view_public, Event *event);
Pkg_private void we_setptyparms(struct termios	*tp);
Pkg_private void ttysw_resize(Ttysw_view_handle ttysw_view);
Pkg_private void ttysw_reset_conditions(Ttysw_view_handle ttysw_view);

Pkg_private void ttysw_pty_input(Ttysw_private	ttysw, int pty);
Pkg_private void ttysw_move_mark(Textsw textsw, Textsw_mark *mark, Textsw_index to, int flags);
Pkg_private void ttysw_sendsig(Ttysw_private ttysw, Xv_window termswview,int sig);

Pkg_private void ttysw_sigwinch(Ttysw_private ttysw);
Pkg_private void ttysw_setopt(Ttysw_private ttysw_folio_or_view, int opt, int on);
Pkg_private void ttysw_lighten_cursor(Ttysw_private ttysw);

Pkg_private void ttysw_writePartialLine(Ttysw_private ttysw, CHAR *s, int curscolStart);
Pkg_private void ttysw_underscore_mode(Ttysw_private ttysw);
Pkg_private void ttysw_vpos(Ttysw_private ttysw, int row, int col);
Pkg_private void xv_tty_new_size(Ttysw_private ttysw, int cols, int lines);

#ifdef OW_I18N
Pkg_private void tty_column_wchar_type(int xChar, int yChar,
		int *cwidth, /* character width (RETURN) */
		int *offset); /* offset of charcter (RETURN) */
#endif

Pkg_private int ttysw_freeze(Ttysw_view_handle ttysw_view, int on);
Pkg_private int ttysw_ansi_escape(Tty_view ttysw_view_public, int c, int ac, int *av);
Pkg_private int ttysw_input_it(Ttysw_private ttysw, char *addr, int len);
Pkg_private int tty_getmode(int fd, tty_mode_t	*mode);
Pkg_private int ttysw_lookup_boldstyle(char *str);
Pkg_private void ttysw_print_bold_options(void);
Pkg_private int ttysw_destroy(Tty ttysw_folio_public, Destroy_status status);
Pkg_private int ttysw_pty_output_ok( Ttysw_private ttysw);
Pkg_private int ttysw_be_ttysw(Ttysw_view_handle ttysw_view);
Pkg_private int ttysw_be_termsw(Ttysw_view_handle ttysw_view);

Pkg_private int
	ttysw_cooked_echo_mode(Xv_opaque UNKNOWN),
	ttysw_domap(Ttysw_private ttysw, struct inputevent *ie),
	ttysw_fork_it(Ttysw_private ttysw0, char **argv, int unused),
	ttysw_getboldstyle(void),
	ttysw_setboldstyle(int new_boldstyle),
	ttytlsw_string(Tty ttysw_public, int type, int c),
	wininit(Ttysw *, Xv_object win, int *,int *);

Pkg_private int ttysw_copy_to_input_buffer(Ttysw_private ttysw, CHAR *addr, int len);
Xv_public Notify_value ttysw_event(Tty_view ttysw_view_public, Notify_event ev, Notify_arg arg, Notify_event_type type);
Pkg_private void ttysw_mapsetim(Ttysw_private ttysw);

Pkg_private int ttysw_scan_for_completed_commands(Ttysw_view_handle ttysw_view, int start_from, int maybe_partial);
Pkg_private int ttysw_saveparms(int ttyfd);
Pkg_private int ttysw_restoreparms(int ttyfd);
Pkg_private void ttysw_done(Ttysw_private ttysw_folio_private);
Pkg_private int ttysw_getopt(Ttysw_private ttysw, int opt);
Pkg_private int ttysw_output_it (Ttysw_view_handle ttysw_view, CHAR *addr, int len0);
Pkg_private int ttysw_ansi_string (Tty data, int type, int c);

Pkg_private int xv_tty_imageinit(Ttysw *ttysw, Xv_object window);

Pkg_private int ttytlsw_escape(Tty_view ttysw_view_public, int c, int ac, int *av);

Pkg_private void ttysw_pty_output(Ttysw_private ttysw, int pty);
Pkg_private Notify_value ttysw_pty_input_pending(Tty tty_public, int pty);
Pkg_private int ttysw_cooked_echo_cmd(Ttysw_view_handle ttysw_view, char *buf, int buflen);

#ifdef OW_I18N
Pkg_private int
	tty_character_size(CHAR),
	tty_get_nchars(int colstart, int colend, int row),
	ttysw_input_it_wcs(Ttysw_private ttysw, CHAR *addr, register int len);
#endif

/* Pkg_private void ttyhiliteselection(struct ttyselection *ttysel, */
/* 										enum __seln_rank rank); */
Pkg_private void ttyhiliteselection(Ttysw_private ttysw,
							struct ttyselection *ttysel, int rank);
Pkg_private Notify_value ttysw_text_event(Xv_window textsw,
    				Notify_event ev, Notify_arg arg, Notify_event_type type);

Pkg_private Xv_opaque ttysw_init_view_internal(Tty parent, Tty_view tty_view_public);
Pkg_private Xv_opaque ts_create(Ttysw *ttysw, Es_handle original, Es_handle scratch);
Pkg_private Xv_opaque
	ttysw_init_folio_internal(Xv_opaque UNKNOWN),
	ttysw_walkmenu(Xv_opaque UNKNOWN);


Xv_private void ttysortextents(struct ttyselection *ttysel, struct textselpos **begin, struct textselpos **end);
Pkg_private void csr_pixwin_set(Xv_window w);
Pkg_private Xv_window csr_pixwin_get(void);
Xv_private void tty_synccopyarea(Xv_opaque window);

Pkg_private void ttysw_new_sel_init(Ttysw_private priv);
Pkg_private void ttysw_event_paste_up(Ttysw_private priv, struct timeval *t);
Pkg_private void ttysw_event_cut_up(Ttysw_private priv, Event *ev);
Pkg_private int ttysw_event_copy_down(Ttysw_private priv, struct timeval *t);

#ifdef	cplus
/*
 * C Library routines specifically related to private ttysw subwindow
 * functions.  ttysw_output and ttysw_input return the number of characters
 * accepted/processed (usually equal to len).
 */
int
ttysw_output(Tty ttysw_public, char *addr, int len);

/* Interpret string in terminal emulator. */
int
ttysw_input(Tty ttysw_public, char *addr, int len);

/* Add string to the input queue. */
#endif	 /* cplus */

#endif  /* _xview_private_ttysw_impl_h_already_included */
