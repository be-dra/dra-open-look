/* #ident	"@(#)olwm.c	26.66	93/06/28 SMI" */
char olwm_c_sccsid[] = "@(#) %M% V%I% %E% %U% $Id: olwm.c,v 2.7 2025/06/20 20:37:19 dra Exp $";

/*
 *      (c) Copyright 1989 Sun Microsystems, Inc.
 */

/*
 *      Sun design patents pending in the U.S. and foreign countries. See
 *      LEGAL_NOTICE file for terms of the license.
 */

#include <errno.h>
#ifndef apollo
#include <memory.h>
#endif
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/time.h>
#include <sys/types.h>

#include <sys/param.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xresource.h>

#include "i18n.h"
#include "ollocale.h"
#include "events.h"
#include "mem.h"
#include "olwm.h"
#include "win.h"
#include "menu.h"
#include "defaults.h"
#include "resources.h"
#include "globals.h"
#include "group.h"
#include "selection.h"
#include "slots.h"
#include "error.h"
#include "dsdm.h"
#include "atom.h"


typedef	void	(*VoidFunc)();


/*
 * Globals
 */

char		*ProgramName;		/* pointer to original argv[0] */
char		*AppName;		/* last component of ProgramName */
GlobalResourceVariables GRV;		/* variables settable by rsrcs */
XrmDatabase	OlwmDB;			/* the main resource database */
Display		*DefDpy;		/* the display connection */

#ifdef DEBUG

unsigned char	ScratchBuffer[1024];	/* for use in the debugger */

#endif /* DEBUG */


/*
 * Global Quarks.  "Top" refers to the root of the resource name/instance 
 * hierarchy.
 */

XrmQuark TopClassQ;
XrmQuark TopInstanceQ;
XrmQuark OpenWinQ;

/*
 * Forward declarations.
 */

static void	usage();
static Display *openDisplay();
static void	parseCommandline();
static void	sendSyncSignal(void);
static void	initWinClasses();


/*
 * Command-line option table.  Resources named here must be kept in sync with 
 * the resources probed for in resources.c.
 */
static	XrmOptionDescRec	optionTable[] = {
	/*
	 * Standard Options
	 */
	{ "-display",	".display",	XrmoptionSepArg, NULL },
	{ "-name",	".name",	XrmoptionSepArg, NULL },
	{ "-xrm",	NULL,		XrmoptionResArg, NULL },
	{ "-2d", 	".use3D",	XrmoptionNoArg,  "False" },
	{ "-3d", 	".use3D",	XrmoptionNoArg,  "True" },
	{ "-bd",	"*BorderColor",	XrmoptionSepArg, NULL },
	{ "-bordercolor","*BorderColor",XrmoptionSepArg, NULL },
	{ "-bg",	"*Background",	XrmoptionSepArg, NULL },
	{ "-background","*Background",	XrmoptionSepArg, NULL },
	{ "-fg",	"*Foreground",	XrmoptionSepArg, NULL },
	{ "-foreground","*Foreground",	XrmoptionSepArg, NULL },
	{ "-c", 	".setInput",	XrmoptionNoArg,  "select" },
	{ "-click", 	".setInput",	XrmoptionNoArg,  "select" },
	{ "-f",		".setInput",	XrmoptionNoArg,  "follow" },
	{ "-follow",	".setInput",	XrmoptionNoArg,  "follow" },
	{ "-fn",	"*TitleFont",	XrmoptionSepArg, NULL },
	{ "-font",	"*TitleFont",	XrmoptionSepArg, NULL },
	{ "-single",	".singleScreen",XrmoptionNoArg,  "True" },
	{ "-multi",	".singleScreen",XrmoptionNoArg,  "False" },
	{ "-syncfd",   ".syncFd",     XrmoptionSepArg, NULL },
	{ "-syncpid",   ".syncPid",     XrmoptionSepArg, NULL },
	{ "-syncsignal",".syncSignal",  XrmoptionSepArg, NULL },
	{ "-depth",	"*depth",	XrmoptionSepArg, NULL },
	{ "-visual",	"*visual",	XrmoptionSepArg, NULL },
	{ "-dsdm",	".startDSDM",	XrmoptionNoArg,	 "True" },
	{ "-nodsdm",	".startDSDM",	XrmoptionNoArg,	 "False" },
	/*
	 * Debugging Options
	 */
	{ "-all", 	 ".printAll",	 XrmoptionNoArg, "True" },
	{ "-debug",	 ".printOrphans",XrmoptionNoArg, "True" },
	{ "-orphans", 	 ".printOrphans",XrmoptionNoArg, "True" },
	{ "-synchronize",".synchronize", XrmoptionNoArg, "True" },
#ifdef OW_I18N_L3
	/* 
	 * Internationalization Options
	 */
        { "-basiclocale", "*basicLocale", XrmoptionSepArg,  NULL },
       	{ "-displaylang", "*displayLang", XrmoptionSepArg,  NULL },
       	{ "-inputlang",   "*inputLang", XrmoptionSepArg,  NULL },
       	{ "-numeric",     "*numeric", XrmoptionSepArg,  NULL },
       	{ "-dateformat",  "*dateFormat", XrmoptionSepArg,  NULL },
#endif /* OW_I18N_L3 */
};
#define OPTION_TABLE_ENTRIES (sizeof(optionTable)/sizeof(XrmOptionDescRec))

extern char *DRA_CHANGED_setlocale(int cat, char *val);

/* Child Process Handling */

void ReapChildren();		/* public -- called from events.c */

#ifdef ALLPLANES
Bool AllPlanesExists;		/* server supports the ALLPLANES extension */
#endif

#ifdef SHAPE
Bool ShapeSupported;		/* server supports the SHAPE extension */
int  ShapeEventBase;
int  ShapeErrorBase;
int  ShapeRequestBase;
#endif

int	numbuttons;		/* number of buttons on the pointer */
				/*   REMIND: this shouldn't be global */

static char	**argVec;

/*
 * main	-- parse arguments, perform initialization, call event-loop
 */
int main(int argc, char **argv)
{
	XrmDatabase		commandlineDB = NULL;

#ifdef OW_I18N_L3
	char			*OpenWinHome;
	char			locale_dir[MAXPATHLEN+1];
	extern char		*getenv();
#endif /* OW_I18N_L3 */

#ifdef MALLOCDEBUG
	malloc_debug(MALLOCDEBUG);
#endif /* MALLOCDEBUG */
#ifdef GPROF_HOOKS
	moncontrol(0);
#endif /* GPROF_HOOKS */

#ifdef OW_I18N_L3
       	/*
       	 * Even in the SUNDAE1.0 (first release) we might need the
       	 * dynamic locale change for window manager, since window
       	 * manager is usually never re-start again in one sesstion.
       	 * But leave static for now.
       	 */
       	/*
       	 * We are setting the locale (issuing the setlocale) by
       	 * EffectOLLC() function, but we need to call setlocale here
       	 * to handle command line argument with certain locale.
       	 * FIX_ME! But may not work well, because we did not touch the
       	 * Xlib function XrmParseCommand().
       	 */
       	if (DRA_CHANGED_setlocale(LC_CTYPE, "") == NULL) {
		char		*locale;

		locale = (locale = getenv("LC_CTYPE")) != NULL ? locale
			: (locale = getenv("LANG")) != NULL ? locale
			: "Unknown";
#ifndef linux
		/* 
		 * In the following fprintf, it is point less to use
		 * GetString, since we are saying using "C" locale instead.
		 */
		fprintf(stderr, "\
olwm: Warning: '%s' is invalid locale for the LC_CTYPE category,\n\
               using 'C' locale for the command line parsing.\n",
				locale);
#endif /* linux */
		(void)DRA_CHANGED_setlocale(LC_CTYPE,"C");
	}
	if ((OpenWinHome = getenv("OPENWINHOME")) != 0)
		(void)strcpy(locale_dir,OpenWinHome);
	else
		(void)strcpy(locale_dir,"/usr");
	(void)strcat(locale_dir,"/lib/locale");
	bindtextdomain("olwm_messages",locale_dir);
       	textdomain("olwm_messages");
#endif /* OW_I18N_L3 */

	ProgramName = argv[0];
	argVec = argv;

	/*
	 * Set up signal handlers.  Clean up and exit on SIGHUP, SIGINT, and 
	 * SIGTERM; note child process changes on SIGCHLD.
	 */
#ifdef SYSV
	sigset(SIGHUP, ExitOLWM);
	sigset(SIGINT, ExitOLWM);
	sigset(SIGTERM, ExitOLWM);
#else
	signal(SIGHUP, ExitOLWM);
	signal(SIGINT, ExitOLWM);
	signal(SIGTERM, ExitOLWM);
#endif

	XrmInitialize();

	/* parse the command line arguments into local tmp DB */
	parseCommandline(&argc, argv, &commandlineDB);

	DefDpy = openDisplay(commandlineDB);

#ifdef ALLPLANES
	{
	    int tmp;
	    AllPlanesExists = XAllPlanesQueryExtension(DefDpy, &tmp, &tmp);
	}
#endif /* ALLPLANES */


#ifdef SHAPE
	ShapeSupported = XQueryExtension(DefDpy, "SHAPE",
	    &ShapeRequestBase, &ShapeEventBase, &ShapeErrorBase);
#endif /* SHAPE */


	/*
	 * Determine the number of buttons on the pointer.  Use 3 by default.
	 */
	numbuttons = XGetPointerMapping (DefDpy, (unsigned char *)0, 0);
	if (numbuttons < 1)
		numbuttons = 3;

	/* put all resources into global OlwmDB and set olwm variables */
	GetDefaults(DefDpy, commandlineDB);

	/* Initialize the event handling system. */
	InitEvents(DefDpy);
	InitBindings(DefDpy);
	XSetErrorHandler(ErrorHandler);
	if (GRV.Synchronize)
		XSynchronize(DefDpy, True);

	/* Initialize a variety of olwm subsystems. */
	InitAtoms(DefDpy);
	WIInit(DefDpy);
	initWinClasses(DefDpy);
	InitClients(DefDpy);
	GroupInit();

	/*
	 * Ensure that the X display connection is closed when we exec a 
	 * program.
	 */
	if (fcntl(ConnectionNumber(DefDpy), F_SETFD, 1) == -1) {
		perror(GetString("olwm: child cannot disinherit TCP fd"));
		exit(14);
	}

	/* Init the global menus */
	InitMenus(DefDpy);

	/* init region handling code */
	InitRegions();

	/* Init screen */
	InitScreens(DefDpy);
	GrabKeys(DefDpy, True);
	GrabButtons(DefDpy, True);
	ReparentScreens(DefDpy);
	if (!GRV.FocusFollowsMouse)
	    ClientFocusTopmost(DefDpy, GetFirstScrInfo(), LastEventTime);

	/* Initialize selections. */
	SelectionInit();

	/* Initialize (and then start, if desired) the DSDM function. */
	DragDropInit();
	if (GRV.StartDSDM)
	    DragDropStartDSDM(DefDpy);

 	/* Start olwmslave - using the same args we got. */
	if (GRV.RunSlaveProcess) SlaveStart(argVec);

	dra_init_signals();

	/* Beep to indicate that we're ready. */
	if (GRV.Beep != BeepNever)
	    XBell(DefDpy, 100);
	XSync(DefDpy, False);

	/* Inform anyone who's waiting that we're ready. */
	sendSyncSignal();

	EventLoop(DefDpy);

	/*NOTREACHED*/
	exit(33);
}


/* 
 * parseCommandline - parse the command line arguments into a resource
 *	database
 */
static void
parseCommandline( argc, argv, tmpDB )
int		*argc;
char		*argv[];
XrmDatabase	*tmpDB;
{
	char	instName[MAX_NAME];
	char	namestr[MAX_NAME];
	char	*type, *p;
	XrmValue val;

	/* Extract trailing pathname component of argv[0] into AppName. */

	AppName = strrchr(argv[0], '/');
	if (AppName == NULL)
	    AppName = argv[0];
	else
	    ++AppName;

	XrmParseCommand(tmpDB, optionTable, OPTION_TABLE_ENTRIES,
			AppName, argc, argv );

	/*
	 * Initialize root instance and class quarks.  Create the instance
	 * name by first looking up the "name" resource in the command line
	 * database (for the -name option).  If it's not present, use AppName
	 * (the trailing pathname component of argv[0]).  Then, scan it and
	 * replace all illegal characters with underscores.  Note: we don't
	 * use the ctype functions here, because they are internationalized.
	 * In some locales, isalpha() will return true for characters that are
	 * not valid in resource component names.  Thus, we must fall back to
	 * standard character comparisions.
	 *
	 * REMIND: specifying the -name option changes the name with which 
	 * resources are looked up.  But the command line options were put 
	 * into the database using AppName, which is based on argv[0].  Thus, 
	 * specifying -name causes all command-line args to be ignored, which 
	 * is wrong.
	 */

	(void) strcpy(namestr, AppName);
	(void) strcat(namestr, ".name");
	if (XrmGetResource(*tmpDB, namestr, namestr, &type, &val)) {
	    (void) strncpy(instName, (char *)val.addr, MAX_NAME);
	} else {
	    (void) strncpy(instName, AppName, MAX_NAME);
	}

	instName[MAX_NAME-1] = '\0';
	for (p=instName; *p != '\0'; ++p) {
	    if (!((*p >= 'a' && *p <= 'z') ||
		  (*p >= 'A' && *p <= 'Z') ||
		  (*p >= '0' && *p <= '9') ||
		  *p == '_' || *p == '-')) {
		*p = '_';
	    }
	}
	TopInstanceQ = XrmStringToQuark(instName);
	TopClassQ = XrmStringToQuark("Olwm");
	OpenWinQ = XrmStringToQuark("OpenWindows");

	/* check to see if there are any arguments left unparsed */
	if ( *argc != 1 )
	{
		/* check to see if it's -help */
		if ( argv[1][0] == '-' && argv[1][1] == 'h' ) {
			usage(  GetString("Command line arguments accepted"),
				GetString("are:"));
		} else {
			usage(	GetString("Unknown argument(s)"), 
				GetString("encountered"));
		}
	}
}

static int io_errors(Display *dpy)
{
	exit(55);
}

/*
 * openDisplay - open the connection to the X display.  A probe is done into
 * the command-line resource database in order to pick up the '-display'
 * command-line argument.  If it is found, its value is put into the
 * environment.
 */
static Display *
openDisplay(rdb)
    XrmDatabase rdb;
{
    char namebuf[MAX_NAME];
    char *type;
    XrmValue value;
    char *dpystr = NULL;
    char *envstr;
    Display *dpy;

    (void) strcpy(namebuf, AppName);
    (void) strcat(namebuf, ".display");

    if (XrmGetResource(rdb, namebuf, namebuf, &type, &value)) {
	dpystr = (char *)value.addr;
	envstr = (char *)MemAlloc(8+strlen(dpystr)+1);
	sprintf(envstr, "DISPLAY=%s", dpystr);
	putenv(envstr);
    }

    dpy = XOpenDisplay(dpystr);
    if (dpy == NULL) {
	if (dpystr == NULL)
	    dpystr = GetString("(NULL DISPLAY)");
	fprintf(stderr, GetString("%s: cannot connect to %s\n"),
		ProgramName, dpystr);
	exit(15);
    }
	XSetIOErrorHandler(io_errors);
    return dpy;
}

#ifndef MAXPID
/* frueher: #  define MAXPID 30000	*/	/* max process id */

/*  27.7.2024: cat /proc/sys/kernel/pid_max */
#  define MAXPID 4194304
#endif

static int syncPid = 0;

#ifdef linux
void initiateSystemReboot(dpy)
	Display *dpy;
{
    if (syncPid <= 0 || syncPid > MAXPID) {
		XBell(dpy, 50);
		return;
	}

	kill(syncPid, SIGUSR1);
}

void initiateSystemShutdown(dpy)
	Display *dpy;
{
    if (syncPid <= 0 || syncPid > MAXPID) {
		XBell(dpy, 50);
		return;
	}

	kill(syncPid, SIGUSR2);
}

void initiateLogout(dpy)
	Display *dpy;
{
    if (syncPid <= 0 || syncPid > MAXPID) {
		XBell(dpy, 50);
		return;
	}

	kill(syncPid, SIGHUP);
}
#endif /* linux */

/*
 * sendSyncSignal
 *
 * Send a signal to the process named on the command line (if any).  Values
 * for the process id and signal to send are looked up in the resource 
 * database; they are settable with command-line options.  The resources are 
 * looked up with the names
 * 
 *	<appname>.syncPid		process id
 *	<appname>.syncSignal		signal to send (integer)
 *
 * where <appname> is the trailing pathname component of argv[0].
 */
static void sendSyncSignal(void)
{
	char *type;
	XrmValue value;
	XrmValue fdvalue;
	int pid;
	int sig = SIGALRM;
	int tmp;
	char namebuf[100];

	(void)strcpy(namebuf, AppName);
	(void)strcat(namebuf, ".syncPid");
	if (!XrmGetResource(OlwmDB, namebuf, namebuf, &type, &value))
		return;
	pid = atoi((char *)value.addr);
	if (pid <= 0 || pid > MAXPID)
		return;

	syncPid = pid;

	strcpy(namebuf, AppName);
	strcat(namebuf, ".syncFd");
	if (getenv("OLWM_DEBUG")) fprintf(stderr, "before querying %s\n", namebuf);
	if (XrmGetResource(OlwmDB, namebuf, namebuf, &type, &fdvalue)) {
		int fd = atoi((char *)fdvalue.addr);

		if (getenv("OLWM_DEBUG"))
			fprintf(stderr, "have resource %s: %d\n", namebuf, fd);
		if (fd > 0) {
			/* we have a syncFd - no 'I am ready'-signal will be sent */
			close(fd);
			return;
		}
	}

	strcpy(namebuf, AppName);
	strcat(namebuf, ".syncSignal");
	if (XrmGetResource(OlwmDB, namebuf, namebuf, &type, &value)) {
		tmp = atoi((char *)value.addr);
		if (tmp > 0) sig = tmp;
	}
	kill(pid, sig);
}
 

/*
 * initWinClasses -- initialize all of olwm's class structures.
 */
static void
initWinClasses(dpy)
Display *dpy;
{
	FrameInit(dpy);
	IconInit(dpy);
	ResizeInit(dpy);
	ColormapInit(dpy);
	ButtonInit(dpy);
	BusyInit(dpy);
	MenuInit(dpy);
	PinMenuInit(dpy);
	RootInit(dpy);
	NoFocusInit(dpy);
	PushPinInit(dpy);
	PaneInit(dpy);
	IconPaneInit(dpy);
}


extern void *ClientShutdown();
extern Window NoFocusWin;

/*
 * Exit -- kill the slave process, kill all running applications, then exit.
 */
void Exit(Display *dpy)
{
#ifndef sun
	/* get rid of vkbd on non-suns */
	XSetSelectionOwner(dpy,Atom_OL_SOFT_KEYS_PROCESS,NoFocusWin,LastEventTime);
	XFlush(dpy);
#endif /* ! sun */
	SlaveStop();
	ListApply(ActiveClientList, ClientShutdown, (void *)0);
	XSync(dpy, True);
	exit(0);
	/*NOTREACHED*/
}


extern void *UnparentClient();
/*
 * cleanup -- kill the slave process, destroy pinned menus, and restore all 
 * client windows to the screen.  Does not exit.
 */
static void
cleanup()
{

	/*
 	 * If DefDpy is NULL then we didn't get to the XOpenDisplay()
	 * so basically there is nothing to clean up so return.
	 */
	if (DefDpy == NULL)
		return;

	/*
	 * Stop olwmslave
 	 */
	SlaveStop();

	/*
	 * destroy all pinned menus
	 */
	DestroyPinnedMenuClients();

	/*
	 * Go through the list of windows.  Unmap all icons that are on the
	 * screen.  Reparent all windows back to the root, suitably offset
	 * according to their window-gravities.  Also remap all non-withdrawn
	 * windows, and remove all Withdrawn windows from the save-set (so
	 * they don't get remapped.  REMIND: We have to do this because
	 * Withdrawn windows are still left reparented inside the frame; this
	 * shouldn't be the case.
	 */
	ListApply(ActiveClientList,UnparentClient,NULL);

	/* Destroy the screens - which will restore input focus, colormap,
	 * and background, etc.
	 */
	DestroyScreens(DefDpy);

	XSync(DefDpy, True);
}


/* RestartOLWM -- clean up and then re-exec argv. */
int RestartOLWM(void)
{
    cleanup();
    execvp(argVec[0], argVec);
    ErrorGeneral("cannot restart");
    /*NOTREACHED*/
	return 44;
}


/* Clean up and then exit. */
void ExitOLWM(int sig)
{
#ifdef SYSV
	sigset(SIGCHLD, SIG_IGN);
#else
	signal(SIGCHLD, SIG_IGN);
#endif
	if (sig > 0) {
		char buf[30];

		sprintf(buf, "received signal %d\n", sig);
		write(2, buf, strlen(buf));
	}
    cleanup();
    exit(0);
}



/*
 * usage(s1, s2)	-- print informative message regarding usage
 */
static void
usage(s1, s2)
char	*s1, *s2;
{
	fprintf(stderr, "%s %s\n", s1, s2);
	fprintf(stderr,GetString("usage: %s [options]\n"),ProgramName);

/* STRING_EXTRACTION - do not translate the option (ie -2d, -display)
 *	because those are the actual string names of the command line
 *	option.  Translate the option argument (ie <color>) and
 *	the descriptive text.
 */

#define USAGE(msg)	(void) fprintf(stderr,"%s\n",GetString(msg))

USAGE("Standard Options:");

USAGE(" -2d                         Use two-dimensional look");
USAGE(" -3d                         Use three-dimensional look");
USAGE(" -bd, -bordercolor <color>   Specify the border color");
USAGE(" -bg, -background <color>    Specify the background color");
USAGE(" -c, -click                  Use click-to-focus mode");
USAGE(" -depth <depth>              Specify the depth of the visual to use");
USAGE(" -display <display-string>   Specify the display to manage");
USAGE(" -f, -follow                 Use focus-follows-mouse mode");
USAGE(" -fn, -font <font-name>      Set the font for window titles");
USAGE(" -fg, -foreground <color>    Specify the foreground color");
USAGE(" -multi                      Manage windows on all screens");
USAGE(" -name <resource-name>       Specify resource name for resource db");
USAGE(" -single                     Manage windows for a single screen only");
USAGE(" -syncpid <process-id>       Synchronize with process-id");
USAGE(" -syncsignal <signal>        Signal to send to syncpid");
USAGE(" -xrm <resource-string>      Specify resources on commandline");

USAGE("Debugging Options:");

USAGE(" -all                        Print a message for all events received");
USAGE(" -debug                      Turn on all debugging options");
USAGE(" -orphans                    Print orphaned events");
USAGE(" -synchronize                Run in synchronous mode");
;
USAGE("Internationalization Options:");

USAGE(" -basiclocale <locale-name>  Specify the basic locale for all categories");
USAGE(" -displaylang <locale-name>  Specify the language used for displaying text");
USAGE(" -numeric <locale-name>      Specify the numeric format");

#undef USAGE

	exit(16);
}
