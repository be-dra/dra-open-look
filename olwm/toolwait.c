#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <getopt.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/X.h>
#include <X11/Xproto.h>

char toolwait_c_sccsid[] = "@(#) %M% V%I% %E% %U% $Id: toolwait.c,v 1.22 2025/06/20 21:16:34 dra Exp $";

typedef struct _win {
	struct _win *next;
	Window win;
	int events_selected;
} *window_t;

typedef struct _xcl {
	struct _xcl *next;
	unsigned long clid;
	window_t windows;
	int has_wmcmd;
} *client_t;

#define CLID(win) (win >> 20)

static client_t oldc = (client_t)0, /* clients that existed before fork() */
				newc = (client_t)0;
static Atom wmcls, wmcmd, wmstate;
static char *chprog;

static Window found_searched = None;
static char *searched_res_name = 0;

#define INITIAL_ALARM_TIME 10
#define LATE_ALARM_TIME 6
static unsigned alarm_time = 30;
static int trace = 0;
static XErrorHandler orig_handler;

static void potentially_exit(int v)
{
	if (! searched_res_name) exit(0);
}

static void catch(int s)
{
	/* either the timeout has expired or my child is dead */
	if (trace) { 
		fprintf(stderr, "tw %s: exit because of signal %d\n",
									chprog, s);
	}

	potentially_exit(0);
}

static int my_x_error(Display *dpy, XErrorEvent *ev)
{
	if (ev->error_code == BadWindow && ev->request_code == X_QueryTree) {
		return 0;
	}
	return (*orig_handler)(dpy, ev);
}

static void check_client_id(Window w)
{
	unsigned long clid = CLID(w);
	client_t c;

	for (c = oldc; c; c = c->next) if (c->clid == clid) break;
	if (! c) {
		c = (client_t)calloc(1, sizeof(struct _xcl));
		c->next = oldc;
		c->clid = clid;
		oldc = c;
	}
}


/* the 'query' function tries to find out all clients running
 * on the root window - this is used in order to 
 * find out windows that are created by 'already running' clients -
 * such a window might *not* be created by my child process
 */
static void query(Display *dpy, Window win, int level)
{
	Window root, parent, *ch;
	unsigned int num, i;

	if (level <= 0) return;

	if (! XQueryTree(dpy, win, &root, &parent, &ch, &num)) {
		/* failed */
		return;
	}

	for (i = 0; i < num; i++) {
		check_client_id(ch[i]);
		query(dpy, ch[i], level - 1);
	}
	XFree((char *)ch);
}

static Bool check_ev(Display *dpy, XEvent *ev, XPointer arg)
{
	Window mywin = *((Window *)arg);

	if (ev->type != SelectionNotify) return False;
	if (ev->xselection.requestor != mywin) return False;

	return True;
}

static void get_old_clients(Display *dpy, Window root, char *childprog)
{
	Atom sel, tlw;
	XSetWindowAttributes attrs;
	Window mywin;

	/* let's try to avoid those MANY XQueryTree */
	sel = XInternAtom(dpy, "_SUN_DRAGDROP_DSDM", False);
	tlw = XInternAtom(dpy, "_DRA_TOP_LEVEL_WINDOWS", False);
	attrs.override_redirect = 1;
	mywin = XCreateWindow(dpy, root, 0, 0, 1, 1, 0, 
						DefaultDepth(dpy, 0),
						CopyFromParent,
						DefaultVisual(dpy, 0),
						CWOverrideRedirect, &attrs);

	XConvertSelection(dpy, sel, tlw, tlw, mywin, CurrentTime);
	for (;;) {
		XEvent ev;

		XIfEvent(dpy, &ev, check_ev, (XPointer)&mywin);
		if (ev.type == SelectionNotify) {
			XSelectionEvent *xsel = &ev.xselection;
			if (xsel->selection == sel && xsel->target == tlw) {
				if (xsel->property == None) {
					/* no olwm or olwm not ready */
					fprintf(stderr, "toolwait: use XQueryTree for %s (1)\n", childprog);
					query(dpy, root, 3);
					return;
				}
				else {
					Atom act_type;
					int act_format;
					unsigned long nitems, bytes_after;
					Window *prop;

					XGetWindowProperty(dpy, mywin, tlw, 0L, 1000L,
		    				True, XA_WINDOW, &act_type, &act_format,
							&nitems, &bytes_after, (unsigned char **)&prop);
					if (act_format == 32 && nitems > 0 && prop != NULL) {
						int i;

						for (i = 0; i < (int)nitems; i++) {
							if (prop[i]) check_client_id(prop[i]);
						}
						return;
					}
					else {
						fprintf(stderr, "toolwait: use XQueryTree for %s (2)\n", childprog);
						query(dpy, root, 3);
						return;
					}
				}
			}
			else {
				fprintf(stderr, "unexpected sel reply: %ld, %ld\n",
						xsel->selection, xsel->target);
			}
		}
		else {
			fprintf(stderr, "event type %d\n", ev.type);
		}
	}
}

static void check_wm_command(Display *dpy, Window win)
{
	client_t c;
	unsigned long clid = CLID(win);
	int argc;
	char **argv;

	if (XGetCommand(dpy, win, &argv, &argc)) {
		if (argc > 0 && argv && argv[0]) {
			if (!strcmp(argv[0], chprog)) {

				if (trace)
					fprintf(stderr, "tw %s: saw WM_COMMAND on 0x%lx\n",
									chprog, win);

				/* in an earlier version, toolwait exited when it saw
				 * the 'correct' WM_COMMAND
				 */
/* 				exit(0); */

				/* now we wait until the window is also mapped */
				for (c = newc; c; c = c->next) {
					if (c->clid == clid) {
						c->has_wmcmd = 1;

						/* the problem is that the client might be
						 * initially iconic. So, we wait until
						 * one of this client's windows is either
						 * -  mapped OR
						 * -  gets a WM_STATE property OR
						 * -  a rather short time has elapsed
						 */
						if (alarm_time > LATE_ALARM_TIME)
							alarm_time = LATE_ALARM_TIME;
						alarm(alarm_time);
						return;
					}
				}
				fprintf(stderr, "saw correct WM_COMMAND, but from an unknown client\n");
			}
		}
		XFreeStringList(argv);
	}
}

/* check_new_client is called whenever a new root window's child is
 * created
 */
static void check_new_client(Display *dpy, Window win)
{
	unsigned long clid = CLID(win);
	client_t c;
	window_t nw;

	/* did an old client create a window ? */
	for (c = oldc; c; c = c->next) if (c->clid == clid) return;

	if (trace) fprintf(stderr, "tw %s: select Property on 0x%lx\n", chprog, win);

	/* we want to see properties on that window and
	 * we want to know when it becomes mapped
	 */
	XSelectInput(dpy, win, PropertyChangeMask | StructureNotifyMask);

	/* identify the client */
	for (c = newc; c; c = c->next) if (c->clid == clid) break;

	if (! c) {
		c = (client_t)calloc(1, sizeof(struct _xcl));
		c->next = newc;
		c->clid = clid;
		newc = c;
	}

	check_wm_command(dpy, win);

	nw = (window_t)calloc(1, sizeof(struct _win));
	nw->win = win;
	nw->events_selected = True;
	nw->next = c->windows;
	c->windows = nw;

	if (alarm_time > INITIAL_ALARM_TIME) alarm_time = INITIAL_ALARM_TIME;

	alarm(alarm_time);
}

static void found_searched_window(Display *dpy, Window mywindow)
{
	client_t c;
	window_t w;

	exit(0);

	/* alles Quatsch: */
	found_searched = mywindow;
	XSelectInput(dpy, DefaultRootWindow(dpy), 0);
	for (c = newc; c; c = c->next) {
		for (w = c->windows; w; w = w->next) {
			if (w->win == mywindow) {
				XSelectInput(dpy, mywindow, StructureNotifyMask);
			}
			else {
				if (w->events_selected) {
					XSelectInput(dpy, w->win, 0);
				}
			}
		}
	}
}

static void handle_prop(XPropertyEvent *ev)
{
	client_t c;
	window_t w;
	unsigned long clid = CLID(ev->window);

	if (ev->state != PropertyNewValue) return;

	if (trace) {
		fprintf(stderr, "tw %s: prop %s changed on 0x%lx\n", chprog,
						XGetAtomName(ev->display, ev->atom), ev->window);
	}
	for (c = newc; c; c = c->next) {
		if (c->clid == clid) {
			for (w = c->windows; w; w = w->next) {
				if (w->win == ev->window) break;
			}
			break;
		}
	}

	if (! c || ! w) return;

	if (ev->atom == wmcmd) {
		check_wm_command(ev->display, ev->window);
	}
	else if (ev->atom == wmcls) {
		if (trace) fprintf(stderr, "tw %s: wmcls recognized\n", chprog);
		if (searched_res_name) {
			XClassHint *ch = XAllocClassHint();

			if (trace) fprintf(stderr, "tw %s: wmcls search %s\n", chprog, searched_res_name);
			if (XGetClassHint(ev->display, ev->window, ch)) {
				if (trace) fprintf(stderr, "tw %s: %d\n", chprog, __LINE__);
				if (ch->res_name) {
					if (trace) fprintf(stderr, "tw %s: %d: '%s'\n", chprog,
											__LINE__, ch->res_name);
					if (0 == strcmp(searched_res_name, ch->res_name)) {
						found_searched_window(ev->display, ev->window);
					}
					XFree(ch->res_name);
				}
				if (ch->res_class) XFree(ch->res_class);
			}
			XFree(ch);
		}
	}
	else if (ev->atom == wmstate) {
		/* according to the ICCCM,
		 * 'the window manager will place a
		 *  WM_STATE property of type WM_STATE on each top-level client
		 *  window that is not in the Withdrawn state.
		 *  Top-level windows in the Withdrawn state may or may not
		 *  have the WM_STATE property.'
		 */

		/* now, we assume that the WM_STATE indicates that the
		 * client has left the withdrawn state....
		 */

		if (c->has_wmcmd) {
			if (trace)
				fprintf(stderr, "tw %s: exit because 0x%lx has WM_STATE\n",
								chprog, ev->window);
			potentially_exit(0);
		}
	}
}

static void handle_map(Window win)
{
	unsigned long clid = CLID(win);
	client_t c;

	/* win has been mapped, now see if it is one of the good ones...*/
	for (c = newc; c; c = c->next) {
		if (c->clid == clid && c->has_wmcmd) {
			/* this is one of MY clients windows */
			if (trace)
				fprintf(stderr, "tw %s: exit because 0x%lx is mapped\n",
								chprog, win);
			potentially_exit(0);
		}
	}
}

int main(int argc, char **argv)
{
	Display *dpy;
	Window root;
	XEvent ev;
	char buf[1000];
	int option;
	int from_xmon = 0;

	if (argc < 2) exit(0);

	/* that '+' means 'stop at the first non-option */
	while ((option = getopt(argc,argv,"+mtw:")) != EOF) switch (option) {
		case 'w': searched_res_name = optarg; break;
		case 'm': from_xmon = 1; break;
		case 't': trace = 1; break;
	}

	if (! argv[optind]) exit(3);

	chprog = argv[optind];

	dpy = XOpenDisplay((char *)0);
	if (!dpy) {
		perror("cannot open display");
		exit(1);
	}
	orig_handler = XSetErrorHandler(my_x_error);
	wmcmd = XInternAtom(dpy, "WM_COMMAND", False);
	wmcls = XInternAtom(dpy, "WM_CLASS", False);
	wmstate = XInternAtom(dpy, "WM_STATE", False);

	root = DefaultRootWindow(dpy);
	get_old_clients(dpy, root, argv[optind]);
	XSelectInput(dpy, root, SubstructureNotifyMask);
	XFlush(dpy);

	signal(SIGCHLD, catch);
	signal(SIGALRM, catch);

	switch (fork()) {
		case -1:
			perror("toolwait: fork failed");
			exit(1);
		case 0:
    		fcntl(ConnectionNumber(dpy), F_SETFD, 1L);
			if (from_xmon) putenv("DISPLAY=:0");
			execvp(chprog, argv + optind);
			sprintf(buf, "toolwait: cannot exec '%s'", chprog);
			perror(buf);
			exit(2);
		default:
			break;
	}

	alarm_time = 50;
	alarm(alarm_time);
	for (;;) {
		XNextEvent(dpy, &ev);
		switch (ev.type) {
			case CreateNotify:
				if (found_searched) break;
				check_new_client(dpy, ev.xcreatewindow.window);
				break;
			case PropertyNotify:
				if (found_searched) break;
				handle_prop((XPropertyEvent *)&ev);
				break;
			case MapNotify:
				if (found_searched) break;
				handle_map(ev.xmap.window);
				break;
			case DestroyNotify:
				if (found_searched) {
					if (trace)
						fprintf(stderr, "destroy: ev %lx, win %lx, found %lx\n",
							ev.xdestroywindow.event, ev.xdestroywindow.window,
							found_searched);
					if (ev.xdestroywindow.window == found_searched) {
						exit(5);
					}
				}
				break;
			default: break;
		}
	}

	exit(0);
}
