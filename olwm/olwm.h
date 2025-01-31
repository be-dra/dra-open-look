/* @(#) %M% V%I% %E% %U% $Id: olwm.h,v 2.1 2024/09/20 19:59:01 dra Exp $ */
/* #ident	"@(#)olwm.h	26.27	93/06/28 SMI" */

/*
 *      (c) Copyright 1989 Sun Microsystems, Inc.
 */

/*
 *      Sun design patents pending in the U.S. and foreign countries. See
 *      LEGAL_NOTICE file for terms of the license.
 */

#ifndef _OLWM_OLWM_H
#define _OLWM_OLWM_H

#ifndef ABS
#define ABS(a)		(((a) < 0) ? -(a) : (a))
#endif

#ifndef MAX
#define	MAX(a,b)	(((a) > (b)) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a,b)        ((a) < (b) ? (a) : (b))
#endif

/*
 * path and file name lengths -- if not defined already
 */
#ifndef MAXPATHLEN
#define MAXPATHLEN 1024
#endif
#ifndef MAXNAMELEN
#define MAXNAMELEN 256
#endif

/* Determine the size of an object type in 32bit multiples.
 * Rounds up to make sure the result is large enough to hold the object. */
#define LONG_LENGTH(a)	((long)(( sizeof(a) + 3 ) / 4))

#define	FOOTLEN	50L

/* protocols bits */
#define		TAKE_FOCUS		(1<<0)
#define		SAVE_YOURSELF		(1<<1)
#define		DELETE_WINDOW		(1<<2)

#  define		HAS_PROPWIN		(1<<3)
#  define		GROUP_MANAGED	(1<<4)
#  define		WARP_BACK		(1<<5)
#  define		SECONDARY_BASE	(1<<6)
#  define       NO_WARPING      (1<<7)
#  define       IS_WS_PROPS     (1<<8)
#  define       WARP_TO_PIN     (1<<9)
#  define       OWN_HELP        (1<<10)
#  define       ALLOW_ICON_SIZE (1<<11)

/* Workspace Background Styles */
typedef enum { WkspDefault, WkspColor, WkspPixmap } WorkspaceStyle;

/* Icon positioning modes */
typedef enum { AlongTop, AlongBottom, AlongRight, AlongLeft,
	       AlongTopRL, AlongBottomRL, AlongRightBT, AlongLeftBT
	     } IconPreference;

/* size of icon window */
#define ICON_WIN_WIDTH 60
#define ICON_WIN_HEIGHT 60

/* min/max/inc icon sizes */
#define ICON_MIN_WIDTH 		1
#define ICON_MIN_HEIGHT 	1
#define ICON_MAX_WIDTH		160
#define ICON_MAX_HEIGHT		160
#define ICON_WIDTH_INC		1
#define ICON_HEIGHT_INC		1

/* minimum window size */
#define MINSIZE 5

/* Globals */
extern	char   *ProgramName;

/* adornment pixmaps */
extern	Pixmap	pixIcon;
extern	Pixmap	pixmapGray;
extern	Pixmap	pixGray;

/* miscellaneous functions */
extern void ExitOLWM(int sig);
extern void Exit(Display *dpy);
extern void *GetWindowProperty();
#ifdef OW_I18N_L4
extern void parseApplicationLocaleDefaults();
#endif

/* state functions */
extern struct _client *StateNew();
extern void ReparentTree();
extern void StateNormIcon();
extern void StateIconNorm();
extern void StateWithdrawn();

/* root window functions */
extern struct _winroot *MakeRoot();

/* no-focus window information and functions */
extern Window NoFocusWin;
extern struct _wingeneric *NoFocusWinInfo;

extern struct _wingeneric *MakeNoFocus();
extern void NoFocusTakeFocus();
extern void NoFocusInit();
extern int NoFocusEventBeep();

/* client information and functions */
extern struct _List *ActiveClientList;

extern struct _client *ClientCreate();
extern Window ClientPane();
typedef struct _clientinboxclose {
	Display *dpy;
	int 	screen;
	int 	(*func)();
	short 	bx, by, bw, bh;
	Time 	timestamp;
} ClientInBoxClosure;
extern void *ClientInBox();
extern void ClientInhibitFocus();
extern void ClientSetFocus(struct _client *cli, Bool sendTF, Time evtime);
extern void ClientSetCurrent();
extern struct _client *ClientGetLastCurrent();
extern void ClientActivate(Display *dpy, struct _client *cli, Time time);
extern void ClientFocusTopmost();
struct _client;

extern Bool ClientShowHelp(struct _client *cli, int r_x, int r_y, char *hlp);
extern void ClientShowProps(struct _client *cli);

/* frame functions */
extern struct _winpaneframe *MakeFrame();
extern void FrameSetPosFromPane();
extern void FrameFullSize();
extern void FrameNormSize();
extern void FrameNewFooter();
extern void FrameNewHeader();
extern void FrameSetBusy();
extern void FrameWarpPointer();
extern void FrameUnwarpPointer();

/* generic frame functions */
extern int GFrameFocus();
extern int GFrameSelect();
extern int GFrameSetConfigFunc();
extern void GFrameSetStack();
extern void GFrameSetConfig();
extern int GFrameEventButtonPress();
extern int GFrameEventMotionNotify();
extern int GFrameEventButtonRelease();
extern int GFrameEventFocus();
extern int GFrameEventEnterNotify();

/* icon functions */
extern void IconInit();
extern struct _winiconframe *MakeIcon();
extern void IconChangeName();
extern void DrawIconToWindowLines();
extern void IconShow();
extern void IconHide();
extern void IconSetPos();

/* icon pane functions */
extern struct _winiconpane *MakeIconPane();

/* pane functions */
extern struct _winpane *MakePane();

/* pinned menu functions */
extern struct _winmenu *MakeMenu();

/* colormap functions */
extern struct _wingeneric *MakeColormap();
extern void TrackSubwindows();
extern void UnTrackSubwindows();
extern void ColormapInhibit();
extern void InstallColormap();
extern void InstallPointerColormap();
extern void UnlockColormap();
extern void ColorWindowCrossing();
extern struct _wingeneric *ColormapUnhook();
extern void ColormapTransmogrify();

/* selection functions */
extern Bool IsSelected();
extern struct _client *EnumSelections();
extern Time TimeFresh();
/* extern int AddSelection(); */
extern Bool RemoveSelection();
extern Bool ToggleSelection();
extern void ClearSelections();
extern void SelectionResponse();

/* decoration window functions */
extern struct _winpushpin *MakePushPin();
extern struct _winbutton *MakeButton();
extern struct _winresize *MakeResize();

/* general window functions */
extern void WinCallFocus();
extern void WinRedrawAllWindows();
extern Bool WinShowHelp();

/* general window event functions */
extern int WinEventExpose();
extern int WinNewPosFunc();
extern int WinNewConfigFunc();
extern int WinSetConfigFunc();

/* rubber-banding functions */
extern void UserMoveWindows();
extern void UserResizeWin();
extern void TraceRootBox();

/* busy windows */
extern struct _winbusy *MakeBusy();

#endif /* _OLWM_OLWM_H */
