/* @(#) %M% V%I% %E% %U% $Id: kbdfuncs.h,v 2.1 2024/09/20 19:59:01 dra Exp $ */
/* #ident	"@(#)kbdfuncs.h	1.8	93/06/28 SMI" */

/*
 *      (c) Copyright 1989 Sun Microsystems, Inc.
 */

/*
 *      Sun design patents pending in the U.S. and foreign countries. See
 *      LEGAL_NOTICE file for terms of the license.
 */

#ifndef _OLWM_KBDFUNCS_H
#define _OLWM_KBDFUNCS_H

extern void KeyBackFocus();
extern void KeyBeep();
extern void KeyFocusToPointer();
extern void KeyRaiseLowerPointer();
extern void KeyFrontFocus();
extern void KeyFullRestore();
extern void KeyLockColormap();
extern void KeyMove();
extern void KeyNextApp();
extern void KeyNextWindow();
extern void KeyOpenClosePointer();
extern void KeyOpenCloseFocus();
extern void KeyQuitPointer(Display *dpy, XKeyEvent *ke);
extern void KeyOwner();
extern void KeyPrevApp();
extern void KeyPrevWindow();
extern void KeyProperties();
extern void KeyQuit();
extern void KeyRefresh();
extern void KeyResize();
extern void KeyToggleInput();
extern void KeyTogglePin();
extern void KeyUnlockColormap();
extern void KeyWindowMenu();
extern void KeyWorkspaceMenu();
extern void KeyMakeInvisiblePointer();
extern void KeyMakeInvisibleFocus();
extern void KeyMakeVisibleAll();

extern void dra_pointer_up();
extern void dra_pointer_down();
extern void dra_pointer_left();
extern void dra_pointer_right();

#endif /* _OLWM_KBDFUNCS_H */
