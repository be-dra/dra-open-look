#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <X11/Xlib.h>
#include <X11/X.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xresource.h>

char owplaces_c_sccsid[] = "@(#) %M% V%I% %E% %U% $Id: owplaces.c,v 1.12 2025/06/20 21:16:58 dra Exp $";

#ifdef linux
#define GETTIMEOFDAY(p) gettimeofday(p, 0)
#else /* linux */
#define GETTIMEOFDAY(p) gettimeofday(p)
#endif /* linux */

#define C_ALL 0
#define C_LOCAL 1
#define C_REMOTE 2

typedef struct _cllist {
	struct _cllist *next;
	Window xid;
	int responded;
	char **argv;
	int argc;
	char *clientmach;
} *client_t;

static Atom wm_state, wm_command, wm_protocols, wm_save_yourself;

static int outstanding(client_t cl)
{
	while (cl) {
		if (! cl->responded) return True;
		cl = cl->next;
	}

	return False;
}

static void handle_event(Display *dpy, XEvent *ev, client_t cl)
{
	if (ev->type == PropertyNotify &&
		ev->xproperty.state == PropertyNewValue &&
		ev->xproperty.atom == wm_command)
	{
		client_t p;

		for (p = cl; p; p = p->next) {
			if (p->xid == ev->xproperty.window) {
				XGetCommand(dpy, p->xid, &p->argv, &p->argc);
				p->responded = True;
				return;
			}
		}
	}
}

static int check_for_save_yourself(Display *dpy, Window win)
{
	Atom *prots;
	int i, cnt;

	if (! XGetWMProtocols(dpy, win, &prots, &cnt)) return False;

	for (i = 0; i < cnt; i++) {
		if (prots[i] == wm_save_yourself) return True;
	}

	return False;
}

static client_t get_clients(Display *dpy, Window win, client_t list)
{
	Atom act_typeatom;
	int status, act_format;
	unsigned long items, rest;
	unsigned char *wmstat;

	status = XGetWindowProperty(dpy, win, wm_state, 0, 1000, False,
		AnyPropertyType, &act_typeatom, &act_format, &items, &rest, &wmstat);

	if (status == Success && items != 0) {
		XTextProperty clientmach;
		client_t n = (client_t)calloc(1, sizeof(struct _cllist));

		n->next = list;
		n->xid = win;
		list = n;

		if (XGetWMClientMachine(dpy, win, &clientmach)) {
			n->clientmach = (char *)clientmach.value;
		}

		if (! check_for_save_yourself(dpy, win)) {
			XGetCommand(dpy, win, &n->argv, &n->argc);
			n->responded = True;
		}
	}
	else {
		Window root, parent, *ch;
		unsigned int num, i;

		if (! XQueryTree(dpy, win, &root, &parent, &ch, &num)) return list;
		for (i = 0; i < num; i++) {
			list = get_clients(dpy, ch[i], list);
		}
	}

	return list;
}

int main(int argc, char **argv)
{
	static XrmOptionDescRec opts[] = {
		{ "-display", ".display", XrmoptionSepArg, NULL },
		{ "-timeout", ".timeout", XrmoptionSepArg, NULL },
		{ "-output", ".output", XrmoptionSepArg, NULL },
		{ "-host", ".host", XrmoptionSepArg, NULL },
		{ "-all", ".all", XrmoptionNoArg, "true" },
		{ "-remote", ".remote", XrmoptionNoArg, "true" },
		{ "-local", ".local", XrmoptionNoArg, "true" },
		{ "-ampersand", ".ampersand", XrmoptionNoArg, "true" },
		{ "-ampSleepTime", ".ampSleepTime", XrmoptionSepArg, "2" },
		{ "-tw", ".toolWait", XrmoptionNoArg, "true" },
		{ "-silent", ".silent", XrmoptionNoArg, "true" }
	};
	XrmDatabase db, scrdb;
	char *t;
	XrmValue val;
	Display *dpy;
	FILE *out;
	int which, selval, something_sent, screen;
	Window root, mywin;
	client_t p, clients;
	XEvent ev;
	Time timestamp;
	XClientMessageEvent xcl;
	char host[80];
	struct utsname name;
	char *resman;

	XrmInitialize();
	scrdb = db = (XrmDatabase)0;

	XrmParseCommand(&db,
				opts, sizeof(opts)/sizeof(opts[0]),
				"owplaces", &argc, argv);

	if (XrmGetResource(db,"owplaces.display","owplaces.display", &t, &val)) {
		dpy = XOpenDisplay((char *)val.addr);
	}
	else {
		dpy = XOpenDisplay((char *)0);
	}
	if (!dpy) exit(1);

	resman = XResourceManagerString(dpy);
	if (resman) scrdb = XrmGetStringDatabase(resman);

	screen = DefaultScreen(dpy);
	root = RootWindow(dpy,screen);

	wm_state = XInternAtom(dpy, "WM_STATE", False);
	wm_command = XInternAtom(dpy, "WM_COMMAND", False);
	wm_protocols = XInternAtom(dpy, "WM_PROTOCOLS", False);
	wm_save_yourself = XInternAtom(dpy, "WM_SAVE_YOURSELF", False);

	clients = get_clients(dpy, root, (client_t)0);

	uname(&name);
	strcpy(host, name.nodename);
	which = C_LOCAL;
	if (XrmGetResource(db,"owplaces.local","owplaces.local", &t,&val)) {
		which = C_LOCAL;
	}
	else if (XrmGetResource(db, "owplaces.remote","owplaces.remote",&t,&val)) {
		which = C_REMOTE;
	}
	else if (XrmGetResource(db, "owplaces.host", "owplaces.host", &t, &val)) {
		which = C_LOCAL;
		strcpy(host, (char *)val.addr);
	}
		
	for (p = clients; p; p = p->next) {
		switch (which) {
			case C_LOCAL:
				if (p->clientmach) {
					if (strcmp(p->clientmach, host)) {
						p->responded = True;
						p->argv = (char **)0;
					}
				}
				else {
					/* did not tell its machine */
					p->responded = True;
					p->argv = (char **)0;
				}
				break;

			case C_REMOTE:
				if (p->clientmach) {
					if (! strcmp(p->clientmach, host)) {
						p->responded = True;
						p->argv = (char **)0;
					}
				}
				else {
					/* did not tell its machine */
					p->responded = True;
					p->argv = (char **)0;
				}
				break;

			default: break;
		}
	}

	xcl.type = ClientMessage;
	xcl.format = 32;
	xcl.message_type = wm_protocols;
	xcl.data.l[0] = wm_save_yourself;

	mywin = XCreateSimpleWindow(dpy, root, 0, 0, 2, 2, 0,
				BlackPixel(dpy,screen), WhitePixel(dpy,screen));

	XSelectInput(dpy, mywin, PropertyChangeMask);
	XChangeProperty(dpy, mywin, XA_STRING, XA_STRING, 8, PropModeReplace,
						(unsigned char *)&mywin, 1);

	for (;;) {
		XNextEvent(dpy, &ev);
		if (ev.type == PropertyNotify) {
			timestamp = ev.xproperty.time;
			break;
		}
	}

	xcl.data.l[1] = (long)timestamp;
	something_sent = False;
	for (p = clients; p; p = p->next) {
		if (! p->responded) {
			xcl.window = p->xid;
			XSelectInput(dpy, p->xid, PropertyChangeMask);
			XSendEvent(dpy, p->xid, False, 0, (XEvent *)&xcl);
			something_sent = True;
		}
	}

	if (something_sent) {
		struct timeval now, latest;

		GETTIMEOFDAY(&now);
		latest = now;
		if (XrmGetResource(db,"owplaces.timeout","owplaces.timeout",&t,&val)) {
			latest.tv_sec += atoi((char *)val.addr);
		}
		else if (XrmGetResource(scrdb, "olwm.saveWorkspaceTimeout",
					"OpenWindows.SaveWorkspaceTimeout", &t,&val))
		{
			latest.tv_sec += atoi((char *)val.addr);
		}
		else latest.tv_sec += 30;

		XFlush(dpy);
		for (;;) {
			fd_set fds;
			struct timeval seltim;

			while (XCheckMaskEvent(dpy, (~ 0), &ev)) {
				handle_event(dpy, &ev, clients);
			}

			if (! outstanding(clients)) break;

			GETTIMEOFDAY(&now);
			if (now.tv_sec > latest.tv_sec) {
				exit(5); /* recognized by olwm */
				break;
			}

			FD_ZERO(&fds);
			FD_SET(ConnectionNumber(dpy), &fds);

			seltim.tv_sec = 1;
			seltim.tv_usec = 0;
			selval = select(ConnectionNumber(dpy) + 2, &fds,
							(fd_set *)0, (fd_set *)0, &seltim);

			if (selval < 0) {
				perror("select");
				exit(1);
			}
		}
	}

	if (XrmGetResource(db, "owplaces.output", "owplaces.output", &t, &val)) {
		if (! (out = fopen((char *)val.addr, "w"))) {
			fprintf(stderr, "cannot write to ");
			perror((char *)val.addr);
			exit(4); /* recognized by olwm */
		}
	}
	else {
		out = stdout;
	}

	fprintf(out, "#!/bin/sh\nif [ -r $HOME/.xview/root_image ]\nthen\n");
	fprintf(out, "\txli -onroot -fullscreen -quiet -background ");
	fprintf(out, "`xrdb -query | grep WorkspaceColor | cut -c29-` ");
	fprintf(out, "$HOME/.xview/root_image\nfi\n");

	while (clients) {
		if (clients->responded && clients->argv) {
			int i;

			if (XrmGetResource(db,"owplaces.toolWait","owplaces.toolWait",
											&t,&val)) {
				fprintf(out, "toolwait ");
			}
			for (i = 0; i < clients->argc; i++) {
				fprintf(out, "%s ", clients->argv[i]);
			}
			if (XrmGetResource(db,"owplaces.ampersand","owplaces.ampersand",
											&t,&val))
			{
				fprintf(out, "&\n");
				if (XrmGetResource(db,"owplaces.ampSleepTime","owplaces.ampSleepTime",
											&t,&val))
				{
					int sec = atoi((char *)val.addr);
					if (sec > 0) {
						fprintf(out, "sleep %d\n", sec);
					}
				}
				else {
					fprintf(out, "sleep 2\n");
				}
			}
			else {
				fprintf(out, "\n");
			}
		}
		clients = clients->next;
	}

	fclose(out);
	exit(0);
}
