#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>

#include <sys/wait.h>
#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "i18n.h"
#include "ollocale.h"
#include "events.h"
#include "olwm.h"
#include "globals.h"

#ifdef SYSV
#define SET_SIGNAL(s,f) sigset(s, f);
#else
#define SET_SIGNAL(s,f) signal(s, f);
#endif

char dra_signal_c_sccsid[] = "@(#) %M% V%I% %E% %U% $Id: dra_signal.c,v 1.9 2025/03/25 18:51:44 dra Exp $";

typedef struct {
	int signal;
	pid_t pid;
	int status;
} pipe_mess_t;

static int writepipe;

static void handleAlarm(int s)
{
	pipe_mess_t mess;

	mess.signal = s;
	mess.pid = 0;
	write(writepipe, (char *)&mess, sizeof(mess));
	SET_SIGNAL(s, handleAlarm);
}

/*
 * handleChildSignal - keep track of children that have died
 */
static void handleChildSignal(int s)
{
	pipe_mess_t mess;

	mess.signal = s;

	for (;;) {
		mess.pid = waitpid(-1, &mess.status, WNOHANG);

		if (mess.pid == 0) break;

		if (mess.pid == -1) {
		    if (errno == EINTR) continue;
		    if (errno != ECHILD) perror("olwm -- wait");
		    break;
		}

		write(writepipe, (char *)&mess, sizeof(mess));
	}

	SET_SIGNAL(s, handleChildSignal);
}

static int readpipe = -1;
static int wspPid = -1;

extern Window NoFocusWin;

void dra_olws_started(Display *dpy, int pid)
{
	int oldWspPid = wspPid;

	dra_olwm_trace(66, "in dra_olws_started, pid=%d, oldpid=%d\n", pid, wspPid);

	wspPid = pid;
	if (pid > 0) {
		if (XGrabPointer(dpy, NoFocusWin, False, ButtonPressMask,
			 	GrabModeAsync, GrabModeAsync, None,
			 	GRV.BusyPointer, LastEventTime) != GrabSuccess)
		{
	    	ErrorWarning(GetString("failed to grab pointer"));
		}
		SET_SIGNAL(SIGALRM, handleAlarm);
		alarm(20);
	}
	else if (oldWspPid > 0) {
		alarm(0);
		XUngrabPointer(dpy,LastEventTime);
	}
}

void dra_dispatch_signal(Display *dpy)
{
	pipe_mess_t mess;
	int len;

/* 	dra_olwm_trace(400, "in dra_dispatch_signal\n"); */
	dra_olwm_trace(66, "in dra_dispatch_signal\n");

	len = read(readpipe, (char *)&mess, sizeof(mess));
	dra_olwm_trace(66, "len=%d, requested=%d\n", len, sizeof(mess));
	if (len == sizeof(mess)) {
		if (mess.signal == SIGCHLD) {
			SlaveStopped(mess.pid);

			if (mess.pid == wspPid) {
				dra_olwm_trace(66, "olws_props exit detected\n");
				dra_olws_started(dpy, -1);
			}

			if (WIFSIGNALED(mess.status)) {
				char buf[200];

				sprintf(buf, "olwm: child (pid=%d) died from signal %d\n",
								mess.pid, WTERMSIG(mess.status));
				write(2, buf, strlen(buf));
			}
			else if (WIFSTOPPED(mess.status)) kill(mess.pid, SIGKILL);
		}
		else if (mess.signal = SIGALRM) {
			dra_olwm_trace(66, "olws_props timeout\n");
			dra_olws_started(dpy, -1);
		}
	}
	else {
		perror("read messpipe");
	}
	XFlush(dpy);
}

int dra_get_pipe(void)
{
	return readpipe;
}

void dra_init_signals(void)
{
	int pip[2];

	pipe(pip);
	readpipe = pip[0];
	writepipe = pip[1];
	SET_SIGNAL(SIGCHLD, handleChildSignal);
}

void dra_disable_sigchld(void)
{
	/* earlier: SET_SIGNAL(SIGCHLD, SIG_IGN); */
	/* under recent Linuxes, this leads to a system()-return of -1 */
	SET_SIGNAL(SIGCHLD, SIG_DFL);
}

void dra_enable_sigchld(void)
{
	SET_SIGNAL(SIGCHLD, handleChildSignal);
}
