#ifndef lint
char tty_main_c_sccsid[] = "@(#)tty_main.c 20.93 93/06/28 DRA: $Id: tty_main.c,v 4.23 2026/08/04 19:19:43 dra Exp $";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */


/*
 * Very active terminal emulator subwindow pty code.
 */

#include <ctype.h>
#include <xview_private/portable.h>

#ifdef	XV_USE_SVR4_PTYS
#  include <sys/stropts.h>
#  include <sys/stream.h>
#  include <termio.h>
#else	/* XV_USE_SVR4_PTYS */
#  include <sys/uio.h>
#endif	/* XV_USE_SVR4_PTYS */

#include <sys/stat.h>
#ifdef __linux
#  ifndef TIOCSTI
#    include <sys/ioctl.h>
#  endif
#endif /* __linux */

#include <xview_private/i18n_impl.h>
#include <xview/win_input.h>
#include <xview/win_notify.h>
#include <xview/ttysw.h>
#include <xview/notice.h>
#include <xview_private/tty_impl.h>
#include <xview_private/svr_impl.h>
#include <xview_private/term_impl.h>

#if defined(__linux)
/* martin.buck@bigfoot.com */
#  include <sys/ioctl.h>
#endif

/*
 * jcb	-- remove continual cursor repaint in shelltool windows also known to
 * ttyansi.c
 */

/* shorthand */
#define	iwbp	ttysw->ttysw_ibuf.cb_wbp
#define	irbp	ttysw->ttysw_ibuf.cb_rbp
#define	iebp	ttysw->ttysw_ibuf.cb_ebp
#define	ibuf	ttysw->ttysw_ibuf.cb_buf
#define	owbp	ttysw->ttysw_obuf.cb_wbp
#define	orbp	ttysw->ttysw_obuf.cb_rbp
#define	oebp	ttysw->ttysw_obuf.cb_ebp
#define	obuf	ttysw->ttysw_obuf.cb_buf

static int ttysw_process_select(Ttysw_private ttysw, struct inputevent *ie);
static int ttysw_process_keyboard(Ttysw_private ttysw, struct inputevent *ie);
static int ttysw_process_motion(Ttysw_private ttysw, struct inputevent *ie);
static int ttysw_process_adjust(Ttysw_private ttysw, struct inputevent *ie);

/*
 * handle standard events.
 */

/* #ifdef TERMSW */
/*
 * The basic strategy for building a line-oriented command subwindow
 * (terminal emulator subwindow) on top of the text subwindow is as follows.
 *
 * The idle state has no user input still to be processed, and no outstanding
 * active processes at the other end of the pty (except the shell).
 *
 * When the user starts creating input events, they are passed through to the
 * textsw unless they fall in the class of "activating events" (which right
 * now consists of \n, \r, \033, ^C and ^Z).  In addition, the end of the
 * textsw's backing store is recorded when the first event is created.
 *
 * When an activating event is entered, all of the characters in the textsw
 * from the recorded former end to the current end of the backing store are
 * added to the list of characters to be sent to the pty.  In addition, the
 * current end is set to be the insertion place for response from the pty.
 *
 * If the user has started to enter a command, then in order to avoid messes
 * on the display, the first response from the pty will be suffixed with a \n
 * (unless it ends in a \n), and the pty will be marked as "owing" a \n.
 *
 * In the meantime, if the user continues to create input events, they are
 * appended at the end of the textsw, after the response from the pty.  When
 * an activating event is entered, all of the markers, etc.  are updated as
 * described above.
 *
 * The most general situation is:  Old stuff in the log file ^User editing
 * here More old stuff Completed commands enqueued for the pty Pty inserting
 * here^ (Prompt)Partial user command
 */

/* #endif TERMSW */


/*
 * Main pty processing.
 */

/*
 * Return nonzero iff ttysw is in a state such that the current (partial) line
 * destined to be input to the application should be sent to it.
 *
 * Assumption: the line is nonempty.
 */
Pkg_private int
ttysw_pty_output_ok(ttysw)
    register Ttysw_private ttysw;
{
    CHAR	c;

    /*
     * If the ttysw's pty isn't in remote mode, then the kernel pty code will
     * worry about assembling complete lines, so it's ok for us to send what
     * we have.  (N.B., we assume here that the pty is in remote mode
     * precisely when ttysw is acting as a tty (as opposed to text)
     * subwindow.)
     */
    if (!ttysw_getopt(ttysw, TTYOPT_TEXT))
	return (1);
    /*
     * If the slave side of the pty isn't in canonical mode, then partial
     * lines are ok.
     */
    if (!tty_iscanon(ttysw))
	return (1);
    /*
     * If the line ends with a terminator, it should be sent.
     */
    c = *(iwbp - 1);
    if (c == (CHAR)'\n'
		|| c == (CHAR)tty_geteofc(ttysw)
		|| c == (CHAR)tty_geteolc(ttysw)
		|| c == (CHAR)tty_geteol2c(ttysw))
	return (1);
    /*
     * A pending EOT counts as a terminator.
     */
    {
	Termsw_folio    termsw =
	    TERMSW_FOLIO_FOR_VIEW(TERMSW_VIEW_PRIVATE_FROM_TTY_PRIVATE(ttysw));

	    if (termsw->pty_eot > -1)
		return (1);
    }


    return (0);
}

/*
 * Write window input to pty.
 *
 * A bit of care is required here.  If the pty is currently in remote mode, we
 * have responsibility for implementing tty semantics.  In particular, we must
 * make sure that, when in canonical mode, we don't present partial input
 * lines to the application reading from the slave side of the pty.
 */
#ifdef DEBUG
static void ttysw_print_debug_string(char *cp, int len)
{
    int		    i;

    putchar('"');
    for (i=0; i<len; i++) {
	if (isprint(cp[i]))
	    putchar(cp[i]);
	else if (cp[i] == '\033')
	    printf("<ESC>");
	else if (cp[i] == '\n')
	    printf("<NL>");
	else if (cp[i] == '\r')
	    printf("<CR>");
	else if (cp[i] == '\010')
	    printf("<BS>");
	else if (iscntrl(cp[i]))
	    printf("<^%c>", cp[i]+'A'-1);
	else
	    printf("<0x%x>", cp[i]);
    }
    printf("\"\n");
}
#endif /* DEBUG */

Pkg_private void ttysw_pty_output(Ttysw_private ttysw, int pty)
{
	register int cc;

	if (ttysw_getopt(ttysw, TTYOPT_TEXT)) {
		Termsw_folio termsw =
				TERMSW_FOLIO_FOR_VIEW(TERMSW_VIEW_PRIVATE_FROM_TTY_PRIVATE
				(ttysw));

		if (termsw->pty_eot > -1) {
			char *eot_bp = ibuf + termsw->pty_eot;

			/* write everything up to pty_eot */
			if (eot_bp >= irbp) {	/* was: > */
				cc = write(pty, irbp, (size_t)(eot_bp - irbp));

#ifdef DEBUG
				printf("write to pty: ");
				ttysw_print_debug_string(irbp, eot_bp - irbp);
#endif /* DEBUG */

				if (cc > 0)
					irbp += cc;
				else if (cc < 0)
					perror(XV_MSG("TTYSW pty write failure"));

#ifdef __linux
				if (errno == EBADF) {
					fprintf(stderr, "3: pty = %d, ttysw_pty=%d, ttysw_tty=%d\n",
							pty, ttysw->ttysw_pty, ttysw->ttysw_tty);
					abort();
				}
#endif
			}
			termsw->pty_eot = -1;
		}
		/* only write the rest of the buffer if it doesn't have an eot in it */
		if (termsw->pty_eot > -1)
			return;
	}
	if (iwbp > irbp) {
		/*
		 * Bail out if we need to present a complete input line but don't have
		 * one yet.
		 *
		 * XXX: Need to consider buffer overflows here.
		 * XXX: Tests made and actions taken elsewhere should ensure that this
		 *  test never succeeds; we're just being paranoid here.
		 */
		if (!ttysw_pty_output_ok(ttysw))
			return;
		cc = write(pty, irbp, (size_t)(iwbp - irbp));

#ifdef DEBUG
		printf("write to pty: ");
		ttysw_print_debug_string(irbp, iwbp - irbp);
#endif /* DEBUG */

		if (cc > 0) {
			irbp += cc;
			if (irbp == iwbp)
				irbp = iwbp = ibuf;
		}
		else if (cc < 0)
			perror(XV_MSG("TTYSW pty write failure"));

#ifdef __linux
		if (errno == EBADF) {
			fprintf(stderr, "4: pty = %d, ttysw_pty=%d, ttysw_tty=%d\n",
					pty, ttysw->ttysw_pty, ttysw->ttysw_tty);
			abort();
		}
#endif
	}
}

static void ttysw_process_STI(register Ttysw_private ttysw, register char *cp,
											register int cc)
{
	register short post_id;
	register Textsw textsw;
	Textsw_view textsw_view;
	register Termsw_folio termsw;
	Textsw_index pty_index;
	Textsw_index cmd_start;

#ifdef	DEBUG
	fprintf(stderr, "STI \"%.*s\"\n", cc, cp);
#endif /* DEBUG */

	/*
	 * If we're not in remote mode, then the OS tty line discipline code will
	 * already have handled the TIOCSTI, leaving us with nothing to do here.
	 */
	if (!ttysw_getopt(ttysw, TTYOPT_TEXT))
		return;

	textsw = TEXTSW_FROM_TTY(ttysw);
	textsw_view = TERMSW_VIEW_PUBLIC(TERMSW_VIEW_PRIVATE_FROM_TEXTSW(textsw));
	termsw = TERMSW_FOLIO_FOR_VIEW(TERMSW_VIEW_PRIVATE_FROM_TEXTSW(textsw));
	/* Assume app wants STI text echoed at cursor position */
	if (termsw->cooked_echo) {
		pty_index = textsw_find_mark_i18n(textsw, termsw->pty_mark);
		if (termsw->cmd_started)
			cmd_start = textsw_find_mark_i18n(textsw, termsw->user_mark);
		else
			cmd_start = (Textsw_index) xv_get(textsw, TEXTSW_LENGTH_I18N);
		if (cmd_start > pty_index) {
			if (termsw->append_only_log)
				textsw_remove_mark(textsw, termsw->read_only_mark);
			(void)textsw_delete_i18n(textsw, pty_index, cmd_start);
			if (termsw->append_only_log) {
				termsw->read_only_mark =
						textsw_add_mark_i18n(textsw,
						pty_index, TEXTSW_MARK_READ_ONLY);
			}
			termsw->pty_owes_newline = 0;
		}
	}
	/*
	 * Pretend STI text came in from textsw window fd.
	 *
	 * What we really have to do here is post the STI text as events to the
	 * current textsw view.  This delivers them to ttysw_text_event, which
	 * we've interposed on the view.  That routine either handles the events
	 * directly or dispatches them onward.
	 */
	while (cc > 0) {
		post_id = (short)(*cp);
		(void)win_post_id(textsw_view, post_id, NOTIFY_SAFE);
		cp++;
		cc--;
	}
	/* flush caches */
	(void)xv_get(textsw, TEXTSW_LENGTH_I18N);
}


#ifdef	XV_USE_SVR4_PTYS

/*
 * Read pty's input (which is output from program)
 *
 * Assumptions:
 * 1) SVR4-style ptys, with the pckt module pushed on the master side.
 * 2) pty has been put into nonblocking i/o mode.
 * 3) The slave side has ttcompat and ldterm pushed on it, so that we don't
 *    have to cope with BSD-style tty ioctls here.
 * 4) It's ok to return without delivering any data.
 * 5) The data buffer we're given is large enough to hold an iocblk plus
 *    associated data.
 */
Pkg_private void ttysw_pty_input(Ttysw_private ttysw, int pty)
{
	struct strbuf ctlbuf;
	struct strbuf databuf;
	u_char ctlbyte;	/* buffer for control part of msg */
	int flags = 0;
	register int rv;

	ctlbuf.maxlen = sizeof ctlbyte;
	ctlbuf.len = 0;	/* redundant */
	ctlbuf.buf = (char *)&ctlbyte;

  get_some_more:

	databuf.len = 0;	/* redundant */

	/* Looks like the databuffer ttysw->ttysw_obuf.cb_buf
	 * is totally filled under certain cirumstances and
	 * a terminating NUL is written past the end of the buffer !
	 * The next field, however, is the ttysw_pty, which is usually a
	 * number which is small. So, on machines with the 'usual' byte order
	 * (SPARC, MIPS, etc), this does not harm, because the overwritten byte
	 * is 0 anyway. A totally different situation arises on
	 * VAXen and 3/4/586 processors: the overwritten byte contains
	 * the file descriptor which is then suddenly 0 -> BAD THINGS HAPPEN
	 */

	/* Sorry, but this change didn't fix the bug!
	 * Idiot - das ist der '#ifdef	XV_USE_SVR4_PTYS'-Fall.....
	 * now I'm defining an integer instance variable
	 * NEVER_USED_BUT_OVERWRITTEN_FROM_SOMEWHERE
	 * right between ttysw->ttysw_obuf and ttysw->ttysw_pty !!
	 * One int seems to be enough, because in those cases where ttysw_pty
	 * was overwritten, ttysw_tty has been left intact.
	 */
	databuf.maxlen = oebp - owbp - 1;

	databuf.buf = owbp;

	rv = getmsg(pty, &ctlbuf, &databuf, &flags);

	/*
	 * Check for read error or a false hit from poll/select.
	 *
	 * (The original version of the routine didn't distinguish these
	 * possibilities.  It also didn't check for zero-length reads.  For the
	 * moment, at least, we don't try to tell them apart since the original
	 * didn't.)
	 */
	if (rv < 0)
		return;

	/*
	 * If there's no control part, then we've effectively done a normal read.
	 * This can potentially happen if the last message's data part overflowed
	 * the buffer we provided for it.
	 */
	if (ctlbuf.len <= 0)
		goto m_data;	/* sleazy control transfer... */

#   ifdef notdef
	/*
	 * The packet module only creates messages with the control part
	 * consisting of one byte.  Since it packetizes M_DATA and M_*PROTO
	 * messages and getmsg only passes messages of those types through to us,
	 * the test below can never be satisfied.
	 */
	if ((rv & MORECTL) || ctlbuf.len != sizeof ctlbyte)
		return;
#   endif /* notdef */

	/*
	 * Process the message.  The code below handles only packetized M_DATA and
	 * M_IOCTL messages.  It perhaps should be extended to handle M_START,
	 * M_STOP, M_FLUSH, etc.
	 */
	switch (ctlbyte) {

		case M_DATA:
		  m_data:
			if (databuf.len > 0) {
				owbp += databuf.len;

				/*
				 * the following block of code attempts to
				 * compensate for the lack of consolidation in
				 * 5.0 ptys.
				 *
				 * It is used only if we couldn't config the
				 * new bufmod to provide the buffering for
				 * us...
				 *
				 *
				 */
				{
					struct strpeek peek;
					char byte;

					peek.ctlbuf.maxlen = sizeof byte;
					peek.ctlbuf.buf = &byte;
					peek.databuf.buf = NULL;	/* no data info */
					peek.flags = 0;
					if (ioctl(pty, I_PEEK, &peek) <= 0)
						return;
					if (byte == M_DATA)
						goto get_some_more;
				}
				return;
			}
			/*
			 * Zero-length message ==> slave closed; as noted above, we ignore it.
			 */
			return;

		case M_IOCTL:{
				struct iocblk *ioc = (struct iocblk *)databuf.buf;

#ifdef DEBUG
				int i;
				struct ioctl_name_t
				{
					int value;
					char *name;
				};
				static struct ioctl_name_t ioctl_name[8] = {
					TCSETS, "TCSETS",
					TCSETSW, "TCSETSW",
					TCSETSF, "TCSETSF",
					TCSETA, "TCSETA",
					TCSETAW, "TCSETAW",
					TCSETAF, "TCSETAF",
					TIOCSTI, "TIOCSTI",
					0, 0
				};

				for (i = 0; ioctl_name[i].value; i++) {
					if (ioctl_name[i].value == ioc->ioc_cmd) {
						printf("(ttysw_pty_input) ioctl %s received\n",
								ioctl_name[i].name);
						break;
					}
				}
#endif /* DEBUG */


				/*
				 * Process the ioctl by switching on it and handling all interesting
				 * cases.
				 *
				 * XXX: We're utterly unprepared to handle ioctls that overflow
				 *  databuf.  (There's some chance this could happen with
				 *  TIOCSTI.)
				 */
				switch (ioc->ioc_cmd) {
					case TCSETS:
					case TCSETSW:
					case TCSETSF:
						/*
						 * A termios-style ioctl.  Replace our saved tty state with its
						 * contents.  Then check for interesting mode transitions.
						 */
						ttysw->termios =
								*(struct termios *)(databuf.buf + sizeof *ioc);
						ttysw_getp(TTY_VIEW_HANDLE_FROM_TTY_FOLIO(ttysw));
						break;

					case TCSETA:
					case TCSETAW:
					case TCSETAF:{
							/*
							 * A termio-style ioctl.  (It'll be nice when these are phased
							 * out...)  Fold its contents into our saved tty state.  Then
							 * check for interesting mode transitions.
							 */
							struct termios *tp = &ttysw->termios;
							struct termio *ti;

							ti = (struct termio *)(databuf.buf + sizeof *ioc);
							tp->c_iflag =
									(tp->c_iflag & 0xffff0000) | ti->c_iflag;
							tp->c_oflag =
									(tp->c_oflag & 0xffff0000) | ti->c_oflag;
							tp->c_lflag =
									(tp->c_lflag & 0xffff0000) | ti->c_lflag;
							tp->c_cc[VINTR] = ti->c_cc[VINTR];
							tp->c_cc[VQUIT] = ti->c_cc[VQUIT];
							tp->c_cc[VERASE] = ti->c_cc[VERASE];
							tp->c_cc[VKILL] = ti->c_cc[VKILL];
							tp->c_cc[VEOF] = ti->c_cc[VEOF];
							tp->c_cc[VEOL] = ti->c_cc[VEOL];
							tp->c_cc[VEOL2] = ti->c_cc[VEOL2];
							ttysw_getp(TTY_VIEW_HANDLE_FROM_TTY_FOLIO(ttysw));
							break;
						}

					case TIOCSTI:
						/*
						 * The argument byte(s) for the TIOCSTI imediately follow the
						 * ioctl control block.
						 */
						ttysw_process_STI(ttysw, databuf.buf + sizeof *ioc,
								databuf.len - sizeof *ioc);
						break;

					default:
						/*
						 * XXX: Are there other interesting ioctls than the ones handled
						 * above?  For debugging purposes, we probably need a printf here.
						 */
						break;
				}
				break;
			}

		default:
			break;
	}
}

#else	/* XV_USE_SVR4_PTYS */

static void dump_hex_stream(const char *tag, const char *buf, int len)
{
	int i;

	if (tag) return;

	fprintf(stderr, "--- TRACE [%s] (len=%d) ---\n", tag, len);
	for (i = 0; i < len; i++) {
		unsigned char c = (unsigned char)buf[i];

		if (c >= 32 && c <= 126) {
			fprintf(stderr, "%c", c);
		}
		else if (c == 0x1b) fprintf(stderr, "<ESC>");
		else if (c == '\n') fprintf(stderr, "<NL>");
		else if (c == '\r') fprintf(stderr, "<CR>");
		else {
			fprintf(stderr, "\\x%02X", c);
		}
	}
	fprintf(stderr, "\n--------------------------\n");
}

Pkg_private void ttysw_pty_input(Ttysw_private ttysw, int pty)
{
	static struct iovec iov[2];
	register int cc;
	char ucntl;
	register unsigned int_ucntl;

	/* readv avoids need to shift packet header out of owbp. */
	iov[0].iov_base = &ucntl;
	iov[0].iov_len = 1;
	iov[1].iov_base = owbp;
	iov[1].iov_len = oebp - owbp;

	cc = readv(pty, iov, 2);

	if (cc < 0 && errno == EWOULDBLOCK)
		cc = 0;
	else if (cc <= 0)
		cc = -1;
	if (cc > 0) {
		int_ucntl = (unsigned)ucntl;

		SERVERTRACE((888, "\n\n%s: cc=%d, ucntl=%d\n", __FUNCTION__, cc, int_ucntl));
		if (int_ucntl != 0 && ttysw_getopt(ttysw, TTYOPT_TEXT)) {
			unsigned tiocsti = TIOCSTI;

			if (int_ucntl == (tiocsti & 0xff)) {
				ttysw_process_STI(ttysw, owbp, cc - 1);
			}
			else {
				/* Under linux, we saw int_ucntl = 16 or 32, but
				 * tiocsti & 0xff == 12
				 * Especially: read from pty(16): "<^@><ESC>[>4l"
				 * and I guess that this SHOULD have switched to tty mode
				 * but it didn't.
				 * A little later (in the same cmdtool) I saw
				 * read from pty(0): "<^@><ESC>[>4l"
				 * and this switched to tty mode.
				 * So let us try to do the same as in the int_ucntl==0 case:
				 */
				dump_hex_stream("PTY_READ2", owbp, cc-1);
				owbp += cc - 1;
				return;
			}

/* BEGIN DRA_CHANGED according to linux patch */

#ifndef XV_USE_TERMIOS
			(void)ioctl(ttysw->ttysw_tty, TIOCGETC, &ttysw->tchars);
			(void)ioctl(ttysw->ttysw_tty, TIOCGLTC, &ttysw->ltchars);
#else
			(void)tcgetattr(ttysw->ttysw_tty, &ttysw->termios);
#endif

/* END DRA_CHANGED according to linux patch */
			ttysw_getp(ttysw->view);	/* jcb for nng */
		}
		else {
			dump_hex_stream("PTY_READ", owbp, cc-1);
			owbp += cc - 1;
		}
	}
}

#endif 	/* XV_USE_SVR4_PTYS */

/*
 * Send program output to terminal emulator.
 */
Pkg_private void ttysw_consume_output(Ttysw_view_handle ttysw_view)
{
	register Ttysw_private ttysw = TTY_FOLIO_FROM_TTY_VIEW_HANDLE(ttysw_view);
	short is_not_text;
	int cc;

	/* cache the cursor removal and re-render once in this set -- jcb */
	if ((is_not_text = !ttysw_getopt(ttysw, TTYOPT_TEXT))) {
		ttysw_removeCursor(ttysw);
		ttysw->do_cursor_draw = FALSE;
	}
	while (owbp > orbp && !(ttysw->ttysw_flags & TTYSW_FL_FROZEN)) {
		if (is_not_text) {
			if (ttysw->sels[TTY_SEL_PRIMARY].sel_made) {
				ttysel_deselect(ttysw, ttysw->sels+TTY_SEL_PRIMARY, TTY_SEL_PRIMARY);
			}
			if (ttysw->sels[TTY_SEL_SECONDARY].sel_made) {
				ttysel_deselect(ttysw, ttysw->sels+TTY_SEL_SECONDARY, TTY_SEL_SECONDARY);
			}
		}
		cc = ttysw_output_it(ttysw_view, orbp, (int)(owbp - orbp));

		orbp += cc;
		if (orbp == owbp)
			orbp = owbp = obuf;
	}

	if (is_not_text) {
		(void)ttysw_drawCursor(ttysw, ttysw->tty_new_cursor_row, ttysw->tty_new_cursor_col);
		ttysw->do_cursor_draw = TRUE;

	}
}

/*
 * Do the low-level work of transcribing a string into the ttysw's input
 * queue.  Return number of bytes transcribed.
 */
Pkg_private int ttysw_copy_to_input_buffer(Ttysw_private ttysw, CHAR *addr, int len)
{
	if (iwbp + len >= iebp) {
		/*
		 * Input buffer would overflow, so tell user and discard chars.
		 */
		Frame frame = xv_get(TTY_PUBLIC(ttysw), WIN_FRAME);
		Xv_Notice tty_notice = xv_get(frame, XV_KEY_DATA, tty_notice_key);
		if (!tty_notice) {
			tty_notice = xv_create(frame, NOTICE,
					NOTICE_BUTTON_YES, XV_MSG("Continue"),
					NOTICE_MESSAGE_STRINGS,
						XV_MSG("Too many characters to add to the input buffer.\n\
Wait a few seconds after you click Continue,\n\
then retype the missing characters."),
						NULL,
					NOTICE_BUSY_FRAMES, frame, NULL,
					XV_SHOW, TRUE,
					NULL);
			xv_set(frame, XV_KEY_DATA, tty_notice_key, tty_notice, NULL);
		}
		else {
			xv_set(tty_notice,
					NOTICE_BUTTON_YES, XV_MSG("Continue"),
					NOTICE_MESSAGE_STRINGS,
						XV_MSG("Too many characters to add to the input buffer.\n\
Wait a few seconds after you click Continue,\n\
then retype the missing characters."),
						NULL,
					NOTICE_BUSY_FRAMES, frame, NULL,
					XV_SHOW, TRUE,
					NULL);
		}
		return (0);
	}
	(void)XV_BCOPY(addr, iwbp, len * sizeof(CHAR));
	iwbp += len;
	return (len);
}

/*
 * Add the string to the input queue.
 */
Pkg_private int ttysw_input_it(register Ttysw_private ttysw, char *addr,
										register int len)
{
	if (ttysw_getopt(ttysw, TTYOPT_TEXT)) {
		Textsw textsw = TEXTSW_FROM_TTY(ttysw);

		textsw_insert(textsw, addr, len);
		return (len);
	}
	else {
		int bytes_copied;

		bytes_copied = ttysw_copy_to_input_buffer(ttysw, addr, len);

		if (bytes_copied > 0) {
			/*
			 * The ttysw's input state actually changed.  Arrange to flush the
			 * input out through the pty after updating state relating to page
			 * mode.
			 */
			Ttysw_view_handle ttysw_view;

			ttysw->ttysw_lpp = 0;	/* reset page mode counter */
			ttysw_view = ttysw->view;
			if (ttysw->ttysw_flags & TTYSW_FL_FROZEN) {
				ttysw_freeze(ttysw_view, 0);
			}
			if (!(ttysw->ttysw_flags & TTYSW_FL_IN_PRIORITIZER)) {
				ttysw_reset_conditions(ttysw_view);
			}
		}
		return (bytes_copied);
	}
}


/* #ifndef TERMSW */
Pkg_private void ttysw_handle_itimer(Ttysw_private ttysw)
{
	if (ttysw->sels[TTY_SEL_PRIMARY].sel_made) {
		ttysel_deselect(ttysw, ttysw->sels + TTY_SEL_PRIMARY, TTY_SEL_PRIMARY);
	}
	if (ttysw->sels[TTY_SEL_SECONDARY].sel_made) {
		ttysel_deselect(ttysw, ttysw->sels + TTY_SEL_SECONDARY, TTY_SEL_SECONDARY);
	}
	SERVERTRACE((567, "%s\n", __FUNCTION__));
	ttysw_pdisplayscreen(ttysw, FALSE, TRUE);
}

/* This could be a public ttysw view or termsw view */
Pkg_private int ttysw_eventstd(Tty_view ttysw_view_public, Event *ie)
{
	Frame frame_public;
	Ttysw_private ttysw = TTY_PRIVATE_FROM_ANY_VIEW(ttysw_view_public);
	Tty tty_public = TTY_PUBLIC(ttysw);
	Ttysw_view_handle vpriv = TTY_VIEW_PRIVATE(ttysw_view_public);

	switch (event_action(ie)) {
		case KBD_USE:
		case KBD_DONE:
			frame_public = (Frame) xv_get(tty_public, WIN_OWNER);
			switch (event_action(ie)) {
				case KBD_USE:
					ttysw_restore_cursor(ttysw);
					frame_kbd_use(frame_public, tty_public, tty_public);
					return TTY_DONE;
				case KBD_DONE:
					ttysw_lighten_cursor(ttysw);
					frame_kbd_done(frame_public, tty_public);
					return TTY_DONE;
			}
		case WIN_REPAINT:
		case WIN_GRAPHICS_EXPOSE:
			if (TTY_IS_TERMSW(ttysw)) {
				Termsw_view_handle termsw =
						TERMSW_VIEW_PRIVATE_FROM_TTY_PRIVATE(ttysw);

				if (termsw->folio->cmd_started) {
					ttysw_scan_for_completed_commands(vpriv, -1, 0);
				}
			}
			ttysw_display(ttysw, ie);

			return (TTY_DONE);
		case WIN_VISIBILITY_NOTIFY:
			ttysw_view_obscured = event_xevent(ie)->xvisibility.state;
			return (TTY_DONE);

		case WIN_RESIZE:
			ttysw_resize(vpriv);

			return (TTY_DONE);
		case ACTION_SELECT:
			return ttysw_process_select(ttysw, ie);
		case ACTION_ADJUST:
			return ttysw_process_adjust(ttysw, ie);

		case ACTION_MENU:{
				if (event_is_down(ie)) {
					ttysw_show_walkmenu(ttysw_view_public, ie);
					ttysw->ttysw_butdown = ACTION_MENU;
				}

				return (TTY_DONE);
			}

#ifdef notdef	/* BUG ALERT */
			/*
			 * 11 Sept 87:  Alok found that if we do the exit processing, we turn
			 * off LOC_WINEXIT and thus defeat the auto-generation of KBD_DONE by
			 * xview_x_input_readevent.  Until we incorporate a fix in the lower
			 * input code we comment out this optimization.
			 */
		case LOC_WINEXIT:
			return ttysw_process_exit(ttysw, ie);
#endif

		case LOC_DRAG:
			return ttysw_process_motion(ttysw, ie);
		default:
			return ttysw_process_keyboard(ttysw, ie);
	}
}

static int ttysw_process_select(Ttysw_private priv, Event *ev)
{
	if (event_is_down(ev)) {
		priv->ttysw_butdown = ACTION_SELECT;
		SERVERTRACE((500, "%s: ACTION_SELECT down\n", __FUNCTION__));
		ttysel_make(priv, ev, 1);
	}
	else {
		if (priv->ttysw_butdown == ACTION_SELECT) {
			SERVERTRACE((500, "%s: ACTION_SELECT up\n", __FUNCTION__));
			ttysel_adjust(priv, ev, FALSE, FALSE);
			ttysel_finish(priv, ev);
		}
	}
	return TTY_DONE;
}

static int ttysw_process_adjust(Ttysw_private ttysw, struct inputevent *ie)
{

	if (event_is_down(ie)) {
		SERVERTRACE((500, "%s: ACTION_ADJUST down\n", __FUNCTION__));
		ttysel_adjust(ttysw, ie, TRUE, (ttysw->ttysw_butdown == ACTION_ADJUST));
		/* Very important for this to be set after the call to ttysel_adjust */
		ttysw->ttysw_butdown = ACTION_ADJUST;
	}
	else {
		SERVERTRACE((500, "%s: ACTION_ADJUST up\n", __FUNCTION__));
		if (ttysw->ttysw_butdown == ACTION_ADJUST) {
			ttysel_adjust(ttysw, ie, FALSE, FALSE);
		}
	}
	return TTY_DONE;
}

static int ttysw_process_motion(Ttysw_private ttysw, struct inputevent *ie)
{

	if ((ttysw->ttysw_butdown == ACTION_SELECT) ||
		(ttysw->ttysw_butdown == ACTION_ADJUST))
	{
		SERVERTRACE((520, "%s: LOCDRAG\n", __FUNCTION__));
		ttysel_adjust(ttysw, ie, FALSE, FALSE);
	}
	return TTY_DONE;
}

static int ttysw_process_keyboard(Ttysw_private ttysw, Event *ev)
{
	register int id = event_id(ev);

	switch (event_action(ev)) {
		case ACTION_HELP:
		case ACTION_MORE_HELP:
		case ACTION_TEXT_HELP:
		case ACTION_MORE_TEXT_HELP:
		case ACTION_INPUT_FOCUS_HELP:
			return (ttysw_domap(ttysw, ev));
	}

	/* INCOMPLETE  event_is_string(ev) ??? */
	if ((id >= ASCII_FIRST && id <= ISO_LAST) && (event_is_down(ev))) {
		if (event_is_string(ev)) {
			ttysw_input_it(ttysw, event_string(ev),
								(int)strlen(event_string(ev)));
		}
		else {
			char c = (char)id;
			ttysw_input_it(ttysw, &c, 1);
		}

		return TTY_DONE;
	}
	if (id > ISO_LAST) {
/* BEGIN only for testing: */
		if (event_action(ev) == KEY_TOP(5)) {
			int i;
			for (i = 0; i < ttysw->ttysw_bottom; i++) {
				char *line = ttysw->image[i];

				if (line) {
					fprintf(stderr, "%d: chars=%d, bytes=%d '%.50s\n",
							i, line[-2], line[-1], line);
				}
			}
		}
/* END only for testing: */
		return ttysw_domap(ttysw, ev);
	}
	return TTY_OK;
}

/* #endif TERMSW */

/*
 * After the character array image changes size, this routine must be called
 * so that pty knows about the new size.
 */
Pkg_private void xv_tty_new_size(Ttysw_private ttysw, int cols,int lines)
{
#if defined(sun) && ! defined(SVR4)
    /*
     * The ttysize structure and TIOCSSIZE and TIOCGSIZE ioctls are available
     * only on Suns.
     */

    struct ttysize  ts;
#ifndef SVR4
    struct sigvec vec, ovec;

    vec.sv_handler = SIG_IGN;
    vec.sv_mask = vec.sv_onstack = 0;
    (void) sigvec(SIGTTOU, &vec, &ovec);
#endif

    ts.ts_lines = lines;
    ts.ts_cols = cols;
    if ((ioctl(ttysw->ttysw_tty, TIOCSSIZE, &ts)) == -1)
	perror(XV_MSG("ttysw-TIOCSSIZE"));

#ifndef SVR4
    (void) sigvec(SIGTTOU, &ovec, 0);
#endif
#else /* sun */
    /*
     * Otherwise, we use the winsize struct  and TIOCSWINSZ ioctl.
     */
    struct winsize  ws;
#if !defined(__linux) && ! defined(SVR4)
    struct sigvec vec, ovec;

    vec.sv_handler = SIG_IGN;
    vec.sv_mask = vec.sv_onstack = 0;
    (void) sigvec(SIGTTOU, &vec, &ovec);
#endif

	memset(&ws, 0, sizeof(ws));
    ws.ws_row = lines;
    ws.ws_col = cols;
    if ((ioctl(ttysw->ttysw_tty, (long)TIOCSWINSZ, &ws)) == -1)
		perror(XV_MSG("ttysw-TIOCSWINSZ"));

#if !defined(__linux) && ! defined(SVR4)
    (void) sigvec(SIGTTOU, &ovec, 0);
#endif
#endif /* sun */
}


/*
 * Freeze tty output.
 */
Pkg_private int ttysw_freeze(Ttysw_view_handle ttysw_view, int on)
{
	register Ttysw_private ttysw = ttysw_view->folio;
	Tty_view ttyvp = TTY_PUBLIC(ttysw_view);

	if (!ttysw_view->ttysw_cursor)
		ttysw_view->ttysw_cursor = xv_get(ttyvp, WIN_CURSOR);
	if (!(ttysw->ttysw_flags & TTYSW_FL_FROZEN) && on) {
		/*
		 * Inspect the current tty modes without disturbing other state.  The
		 * fact that this circumlocution is necessary is an indication that
		 * interfaces haven't been defined cleanly here.
		 */
		Ttysw tmp;

		(void)tty_getmode(ttysw->ttysw_tty, (tty_mode_t *) & tmp.tty_mode);
		if (tty_iscanon(&tmp)) {
			if (! ttysw_view->ttysw_stop_cursor) {
				/* in former versions, this was cached on the server via
				 * xv_set(srv, XV_KEY_DATA, CURSOR_STOP_PTR, stopcursor, NULL).
				 * If there were programs with SEVERAL tty subwindows, they
				 * could certainly tolerate the existence of several 
				 * stop cursor instances....
				 */
				ttysw_view->ttysw_stop_cursor = xv_create(ttyvp, CURSOR,
											CURSOR_SRC_CHAR, OLC_STOP_PTR,
											CURSOR_MASK_CHAR, 0,
											NULL);
			}
			xv_set(ttyvp,
					WIN_CURSOR, ttysw_view->ttysw_stop_cursor, NULL);
			ttysw->ttysw_flags |= TTYSW_FL_FROZEN;
		}
		else
			ttysw->ttysw_lpp = 0;
	}
	else if ((ttysw->ttysw_flags & TTYSW_FL_FROZEN) && !on) {
		xv_set(ttyvp, WIN_CURSOR, ttysw_view->ttysw_cursor, NULL);
		ttysw->ttysw_flags &= ~TTYSW_FL_FROZEN;
		ttysw->ttysw_lpp = 0;
	}
	return ((ttysw->ttysw_flags & TTYSW_FL_FROZEN) != 0);
}

/*
 * Set (or reset) the specified option number.
 */
Pkg_private void ttysw_setopt(Ttysw_private ttysw_folio_or_view, int opt, int on)
{
	Tty folio_or_view_public;
	Ttysw_view_handle ttysw_view;
	Ttysw_private ttysw_folio;
	int result = 0;

	folio_or_view_public = TTY_PUBLIC((Ttysw_private) ttysw_folio_or_view);
	if (IS_TTY_VIEW(folio_or_view_public) ||
			IS_TERMSW_VIEW(folio_or_view_public)) {
		ttysw_view = (Ttysw_view_handle) ttysw_folio_or_view;
		ttysw_folio = TTY_FOLIO_FROM_TTY_VIEW_HANDLE(ttysw_view);
	}
	else {
		ttysw_folio = (Ttysw_private) ttysw_folio_or_view;
		ttysw_view = ttysw_folio->view;
	}

	switch (opt) {
		case TTYOPT_TEXT:	/* termsw */
			if (on)
				result = ttysw_be_termsw(ttysw_view);
			else
				result = ttysw_be_ttysw(ttysw_view);
	}
	if (result != -1) {
		if (on)
			ttysw_folio->ttysw_opt |= 1 << opt;
		else
			ttysw_folio->ttysw_opt &= ~(1 << opt);
	}
}


Pkg_private int ttysw_getopt(Ttysw_private ttysw, int opt)
{
    return ((ttysw->ttysw_opt & (1 << opt)) != 0);
}


Pkg_private void
ttysw_flush_input(ttysw)
    Ttysw_private     ttysw;
{
#if !defined(__linux) && ! defined(SVR4)
    struct sigvec   vec, ovec;	/* Sys V compatibility */
    int flushf = 0;

    vec.sv_handler = SIG_IGN;
    vec.sv_mask = vec.sv_onstack = 0;
    (void) sigvec(SIGTTOU, &vec, &ovec);
#else
    struct sigaction   vec, ovec;
    vec.sa_handler = SIG_IGN;
    sigemptyset(&vec.sa_mask);
    vec.sa_flags = SA_RESTART;
    sigaction(SIGTTOU, &vec, &ovec);
#endif

    /*
     * Flush tty input buffer.
     *
     * N.B.: Since SVR4 ==> XV_USE_TERMIOS, this can be simplified.
     */
#   ifdef XV_USE_TERMIOS
    if (tcflush(ttysw->ttysw_tty, TCIFLUSH) < 0)
#   else /* XV_USE_TERMIOS */
#if !defined(__linux) && ! defined(SVR4)
    if (ioctl(ttysw->ttysw_tty, TIOCFLUSH, &flushf))
#   else /* SVR4 */
    if (ioctl(ttysw->ttysw_tty, TIOCFLUSH, 0))
#   endif /* SVR4 */
#   endif /* XV_USE_TERMIOS */
	perror(XV_MSG("TIOCFLUSH"));

#if !defined(__linux) && ! defined(SVR4)
    (void) sigvec(SIGTTOU, &ovec, (struct sigvec *) 0);
#else
    sigaction(SIGTTOU, &ovec, (struct sigaction *) 0);
#endif

    /* Flush ttysw input pending buffer */
    irbp = iwbp = ibuf;
}
