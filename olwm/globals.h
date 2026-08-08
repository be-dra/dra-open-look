/* @(#) %M% V%I% %E% %U% $Id: globals.h,v 2.5 2026/08/08 05:04:45 dra Exp $ */
/* #ident	"@(#)globals.h	26.45	93/06/28 SMI" */

/*
 *      (c) Copyright 1989 Sun Microsystems, Inc.
 */

/*
 *      Sun design patents pending in the U.S. and foreign countries. See
 *      LEGAL_NOTICE file for terms of the license.
 */

#ifndef _OLWM_GLOBALS_H
#define _OLWM_GLOBALS_H

#include "list.h"
#include "i18n.h"

typedef struct {
	unsigned int	modmask;
	KeyCode		keycode;
} KeySpec;

typedef enum { BeepAlways, BeepNever, BeepNotices } BeepStatus;

typedef enum { KbdSunView, KbdBasic, KbdFull } MouselessMode;

typedef enum { UIS_2D_BW, UIS_2D_COLOR, UIS_3D_COLOR } UiStyles;

typedef struct _globalResourceVariables {
	char		*WindowColor;
	char		*ForegroundColor;
	char		*BackgroundColor;
	char		*BorderColor;
	WorkspaceStyle	WorkspaceStyle;
	char		*WorkspaceColor;
	char		*WorkspaceBitmapFile;
	char		*WorkspaceBitmapFg;
	char		*WorkspaceBitmapBg;
	Bool		ReverseVideo;
	Bool		PaintWorkspace;
	Bool		PointerWorkspace;
	OlFontSetInfo    TitleFontInfo;
	OlFontSetInfo	TextFontInfo;
	OlFontSetInfo    ButtonFontInfo;
	OlFontSetInfo	IconFontInfo;

	XFontStruct	*GlyphFontInfo;
	Cursor		BasicPointer;
	Cursor		MovePointer;
	Cursor		BusyPointer;
	Cursor		IconPointer;
	Cursor		ResizePointer;
	Cursor		MenuPointer;
	Cursor		QuestionPointer;
	Cursor		TargetPointer;
	Cursor		PanPointer;
	Bool		FocusFollowsMouse;
	Text		*DefaultWinName;
	int		SaveWorkspaceTimeout;
	char		*SaveWorkspaceCmd;
	char		*TextDelimiterChars;
	int		FlashTime;
	Bool		FShowMenuButtons;		/* XXX */
	Bool		FShowPinnedMenuButtons;		/* XXX */
	IconPreference	IconPlacement;
	int			GridSpacing;
	Bool		FSnapToGrid;
	Bool		FocusLenience;
	Bool		DragWindow;
	Bool		AutoRaise;
	int		AutoRaiseDelay;
	Bool		PopupJumpCursor;
	Bool		ColorLocked;
	Bool		PPositionCompat;
	Bool		RefreshRecursively;
	BeepStatus	Beep;
	int		EdgeThreshold;
	int		DragRightDistance;
	int		MoveThreshold;
	int		ClickMoveThreshold;
	int		DoubleClickTime;
	int		RubberBandThickness;
	KeySpec		FrontKey;
	KeySpec		HelpKey;
	KeySpec		OpenKey;
	KeySpec		ConfirmKey;
	KeySpec		CancelKey;
	KeySpec		ColorLockKey;
	KeySpec		ColorUnlockKey;
	List		*Minimals;
	List		*IgnoreMinMax;
	Bool		MouseChordMenu;
	int		MouseChordTimeout;
	Bool		SingleScreen;
	Bool		AutoReReadMenuFile;
	Bool		KeepTransientsAbove;
	Bool		TransientsSaveUnder;
	Bool		TransientsTitled;
	Bool		SelectWindows;
	Bool		ShowMoveGeometry;
	Bool		ShowResizeGeometry;
	Bool		InvertFocusHighlighting;
	Bool		RunSlaveProcess;
	Bool		SelectToggleStacking;
	int		FlashCount;
	int		MaximumIconSize;
	char		*DefaultIconImage;
	char		*DefaultIconMask;
	Bool		ServerGrabs;
	int		IconFlashCount;
	Bool		SelectDisplaysMenu;
	int		SelectionFuzz;
	Bool		AutoInputFocus;
	Bool		AutoColorFocus;
	Bool		ColorTracksInputFocus;
	int		IconFlashOnTime;
	int		IconFlashOffTime;
	MouselessMode	Mouseless;
	Bool		RaiseOnActivate;
	Bool		RestackWhenWithdraw;
	Bool		BoldFontEmulation;
	Bool		RaiseOnMove;
	Bool		RaiseOnResize;
	Bool		StartDSDM;
	int		WindowCacheSize;
	Bool		MenuAccelerators;
	Bool		WindowMenuAccelerators;
    Bool		menuClickExecutesDefault;
	Bool        pureOpenLookWindowMenus;
	Bool        screenSizeRestrictsWindowSize;
	UiStyles    ui_style;
	char		*blackNWhiteWorkspaceColor;
	OLLCItem	LC[OLLC_LC_MAX];
	char		*CharacterSet;
	Bool		PrintOrphans;
	Bool		PrintAll;
	Bool		Synchronize;
	Bool		PrintWarnings;
} GlobalResourceVariables;

extern GlobalResourceVariables	GRV;


/* shortcuts for getting at locale category items */
#define lc_basic		LC[OLLC_LC_BASIC_LOCALE]
#define lc_dlang		LC[OLLC_LC_DISPLAY_LANG]
#define lc_ilang		LC[OLLC_LC_INPUT_LANG]
#define lc_numeric		LC[OLLC_LC_NUMERIC]
#define lc_datefmt		LC[OLLC_LC_DATE_FORMAT]

#endif /* _OLWM_GLOBALS_H */

