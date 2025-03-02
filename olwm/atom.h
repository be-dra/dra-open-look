#ifndef _OLWM_ATOM_H
#define _OLWM_ATOM_H

/* @(#) %M% V%I% %E% %U% $Id: atom.h,v 2.14 2025/03/01 12:25:26 dra Exp $ */
enum atom_index_t {
	OL_AtomColorMapWindows,
	OL_AtomWMState,
	OL_AtomChangeState,
	OL_AtomProtocols,
	OL_AtomTakeFocus,
	OL_AtomSaveYourself,
	OL_AtomDeleteWindow,
	OL_AtomWinAttr,
	OL_AtomPushpinState,
	OL_AtomWindowBusy,
	OL_AtomLeftFooter,
	OL_AtomRightFooter,
	OL_AtomPinOut,
	OL_AtomDecorResize,
	OL_AtomWTBase,
	OL_AtomDecorFooter,
	OL_AtomDecorAdd,
	OL_AtomDecorDel,
	OL_AtomDecorPin,
	OL_AtomWTCmd,
	OL_AtomWTProp,
	OL_AtomPinIn,
	OL_AtomNone,
	OL_AtomWTNotice,
	OL_AtomMenuFull,
	OL_AtomDecorHeader,
	OL_AtomWTHelp,
	OL_AtomMenuLimited,
	OL_AtomDecorClose,
	OL_AtomWTOther,
	OL_AtomOlwmNoFocusWin,
	OL_AtomDfltBtn,
	OL_AtomDecorIconName,
#ifdef OW_I18N_L4
	OL_AtomDecorIMStatus,
	OL_AtomLeftIMStatus,
	OL_AtomRightIMStatus,
#endif
	OL_AtomAtomPair,
	OL_AtomClientWindow,
	OL_AtomClass,
	OL_AtomDelete,
	OL_AtomMultiple,
	OL_AtomListLength,
	OL_AtomName,
	OL_AtomTargets,
	OL_AtomTimestamp,
	OL_AtomUser,
#ifdef OW_I18N_L4
	OL_AtomCompoundText,
#endif
	OL_AtomSunViewEnv,
	OL_AtomSunLedMap,
	OL_AtomSunWMProtocols,
	OL_AtomSunWindowState,
	OL_AtomSunOLWinAttr5,
	OL_AtomSunReReadMenuFile,
	OL_AtomEnhancedOlwm,
	OL_AtomShowProperties,
	OL_AtomPropagateEvent,
	OL_AtomWarpBack,
	OL_AtomNoWarping,
	OL_AtomWarpToPin,
	OL_AtomOwnHelp,
	OL_AtomGroupManaged,
	OL_AtomIsWSProps,
	OL_AtomAllowIconSize,
	OL_AtomMenuFileName,
	OL_AtomRereadMenuFile,
	OL_AtomDraTrace,
	OL_AtomWinMenuDefault,
	OL_AtomSetWinMenuDefault,
	OL_AtomWinColors,
	OL_AtomColorsFollow,
	OL_AtomClipBoard,
	OL_AtomPseudoClipBoard,
	OL_AtomPseudoSecondary,
	OL_AtomSelectEnd,
	OL_AtomMenuFunctionClose,
	OL_AtomMenuFunctionFullsize,
	OL_AtomMenuFunctionProps,
	OL_AtomMenuFunctionBack,
	OL_AtomMenuFunctionRefresh,
	OL_AtomMenuFunctionQuit,
	OL_AtomNoticeEmanation,
	_SUN_QUICK_SELECTION_KEY_STATE,
	DUPLICATE,
	_SUN_SELECTION_END,
	my_NULL,
	_OL_SOFT_KEYS_PROCESS,
	_NET_WM_STATE_MAXIMIZED_VERT,
	_NET_WM_STATE_MAXIMIZED_HORZ,
	OL_AtomSunDragDropInterest,
	OL_AtomSunDragDropDSDM,
	OL_AtomSunDragDropSiteRects,
	OL_AtomTopLevels,
	_NET_WM_WINDOW_TYPE,
	_NET_WM_WINDOW_TYPE_UTILITY,
	_NET_WM_STATE,
	OL_AtomVersion,
	OL_AtomShutdown,
	OL_AtomReboot,
	_OL_SELECTION_IS_WORD,
#ifdef ALWAYS_IN_JOURNALLING_MODE_QUESTION
	OL_AtomJOURNAL_SYNC,
#endif
	_NET_WM_ICON,
	_OL_LAST_ATOM
};

extern Atom atoms[];

#define AtomColorMapWindows atoms[OL_AtomColorMapWindows]
#define AtomWMState atoms[OL_AtomWMState]
#define AtomChangeState atoms[OL_AtomChangeState]
#define AtomProtocols atoms[OL_AtomProtocols]
#define AtomTakeFocus atoms[OL_AtomTakeFocus]
#define AtomSaveYourself atoms[OL_AtomSaveYourself]
#define AtomDeleteWindow atoms[OL_AtomDeleteWindow]
#define AtomWinAttr atoms[OL_AtomWinAttr]
#define AtomPushpinState atoms[OL_AtomPushpinState]
#define AtomWindowBusy atoms[OL_AtomWindowBusy]
#define AtomLeftFooter atoms[OL_AtomLeftFooter]
#define AtomRightFooter atoms[OL_AtomRightFooter]
#define AtomPinOut atoms[OL_AtomPinOut]
#define AtomDecorResize atoms[OL_AtomDecorResize]
#define AtomWTBase atoms[OL_AtomWTBase]
#define AtomDecorFooter atoms[OL_AtomDecorFooter]
#define AtomDecorAdd atoms[OL_AtomDecorAdd]
#define AtomDecorDel atoms[OL_AtomDecorDel]
#define AtomDecorPin atoms[OL_AtomDecorPin]
#define AtomWTCmd atoms[OL_AtomWTCmd]
#define AtomWTProp atoms[OL_AtomWTProp]
#define AtomPinIn atoms[OL_AtomPinIn]
#define AtomNone atoms[OL_AtomNone]
#define AtomWTNotice atoms[OL_AtomWTNotice]
#define AtomMenuFull atoms[OL_AtomMenuFull]
#define AtomDecorHeader atoms[OL_AtomDecorHeader]
#define AtomWTHelp atoms[OL_AtomWTHelp]
#define AtomMenuLimited atoms[OL_AtomMenuLimited]
#define AtomDecorClose atoms[OL_AtomDecorClose]
#define AtomWTOther atoms[OL_AtomWTOther]
#define AtomOlwmNoFocusWin atoms[OL_AtomOlwmNoFocusWin]
#define AtomDfltBtn atoms[OL_AtomDfltBtn]
#define AtomDecorIconName atoms[OL_AtomDecorIconName]
#ifdef OW_I18N_L4
#define AtomDecorIMStatus atoms[OL_AtomDecorIMStatus]
#define AtomLeftIMStatus atoms[OL_AtomLeftIMStatus]
#define AtomRightIMStatus atoms[OL_AtomRightIMStatus]
#endif
#define AtomAtomPair atoms[OL_AtomAtomPair]
#define AtomClientWindow atoms[OL_AtomClientWindow]
#define AtomClass atoms[OL_AtomClass]
#define AtomDelete atoms[OL_AtomDelete]
#define AtomMultiple atoms[OL_AtomMultiple]
#define AtomListLength atoms[OL_AtomListLength]
#define AtomName atoms[OL_AtomName]
#define AtomTargets atoms[OL_AtomTargets]
#define AtomTimestamp atoms[OL_AtomTimestamp]
#define AtomUser atoms[OL_AtomUser]
#ifdef OW_I18N_L4
#define AtomCompoundText atoms[OL_AtomCompoundText]
#endif
#define AtomSunViewEnv atoms[OL_AtomSunViewEnv]
#define AtomSunLedMap atoms[OL_AtomSunLedMap]
#define AtomSunWMProtocols atoms[OL_AtomSunWMProtocols]
#define AtomSunWindowState atoms[OL_AtomSunWindowState]
#define AtomSunOLWinAttr5 atoms[OL_AtomSunOLWinAttr5]
#define AtomSunReReadMenuFile atoms[OL_AtomSunReReadMenuFile]
#define AtomEnhancedOlwm atoms[OL_AtomEnhancedOlwm]
#define AtomShowProperties atoms[OL_AtomShowProperties]
#define AtomPropagateEvent atoms[OL_AtomPropagateEvent]
#define AtomWarpBack atoms[OL_AtomWarpBack]
#define AtomNoWarping atoms[OL_AtomNoWarping]
#define AtomWarpToPin atoms[OL_AtomWarpToPin]
#define AtomOwnHelp atoms[OL_AtomOwnHelp]
#define AtomGroupManaged atoms[OL_AtomGroupManaged]
#define AtomIsWSProps atoms[OL_AtomIsWSProps]
#define AtomAllowIconSize atoms[OL_AtomAllowIconSize]
#define AtomMenuFileName atoms[OL_AtomMenuFileName]
#define AtomRereadMenuFile atoms[OL_AtomRereadMenuFile]
#define AtomDraTrace atoms[OL_AtomDraTrace]
#define AtomWinMenuDefault atoms[OL_AtomWinMenuDefault]
#define AtomSetWinMenuDefault atoms[OL_AtomSetWinMenuDefault]
#define AtomWinColors atoms[OL_AtomWinColors]
#define AtomColorsFollow atoms[OL_AtomColorsFollow]
#define AtomClipBoard atoms[OL_AtomClipBoard]
#define AtomPseudoClipBoard atoms[OL_AtomPseudoClipBoard]
#define AtomPseudoSecondary atoms[OL_AtomPseudoSecondary]
#define AtomSelectEnd atoms[OL_AtomSelectEnd]
#define AtomMenuFunctionClose atoms[OL_AtomMenuFunctionClose]
#define AtomMenuFunctionFullsize atoms[OL_AtomMenuFunctionFullsize]
#define AtomMenuFunctionProps atoms[OL_AtomMenuFunctionProps]
#define AtomMenuFunctionBack atoms[OL_AtomMenuFunctionBack]
#define AtomMenuFunctionRefresh atoms[OL_AtomMenuFunctionRefresh]
#define AtomMenuFunctionQuit atoms[OL_AtomMenuFunctionQuit]
#define AtomNoticeEmanation atoms[OL_AtomNoticeEmanation]
#define Atom_SUN_QUICK_SELECTION_KEY_STATE atoms[_SUN_QUICK_SELECTION_KEY_STATE]
#define AtomDUPLICATE atoms[DUPLICATE]
#define Atom_SUN_SELECTION_END atoms[_SUN_SELECTION_END]
#define AtomNULL atoms[my_NULL]
#define Atom_OL_SOFT_KEYS_PROCESS atoms[_OL_SOFT_KEYS_PROCESS]
#define Atom_NET_WM_STATE_MAXIMIZED_VERT atoms[_NET_WM_STATE_MAXIMIZED_VERT]
#define Atom_NET_WM_STATE_MAXIMIZED_HORZ atoms[_NET_WM_STATE_MAXIMIZED_HORZ]
#define AtomSunDragDropInterest atoms[OL_AtomSunDragDropInterest]
#define AtomSunDragDropDSDM atoms[OL_AtomSunDragDropDSDM]
#define AtomSunDragDropSiteRects atoms[OL_AtomSunDragDropSiteRects]
#define AtomTopLevels atoms[OL_AtomTopLevels]
#define Atom_NET_WM_WINDOW_TYPE atoms[_NET_WM_WINDOW_TYPE]
#define Atom_NET_WM_WINDOW_TYPE_UTILITY atoms[_NET_WM_WINDOW_TYPE_UTILITY]
#define Atom_NET_WM_STATE atoms[_NET_WM_STATE]
#define AtomVersion atoms[OL_AtomVersion]
#define AtomShutdown atoms[OL_AtomShutdown]
#define AtomReboot atoms[OL_AtomReboot]
#define Atom_OL_SELECTION_IS_WORD atoms[_OL_SELECTION_IS_WORD]
#ifdef ALWAYS_IN_JOURNALLING_MODE_QUESTION
#define AtomJOURNAL_SYNC atoms[OL_AtomJOURNAL_SYNC]
#endif
#define Atom_NET_WM_ICON atoms[_NET_WM_ICON]

#endif /* _OLWM_ATOM_H */
