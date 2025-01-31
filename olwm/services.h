#ifndef _OLWM_SERVICES_H
#define _OLWM_SERVICES_H

/* RCSID  "@(#) %M% V%I% %E% %U% $Id: services.h,v 1.2 2008/11/21 08:49:38 dra Exp $"; */

#include "win.h"
#include "menu.h"

extern void ExitFunc(Display *dpy, WinGeneric *winInfo, MenuInfo *menuInfo, int idx);
extern void ExitNoConfirmFunc(Display *dpy, WinGeneric *winInfo, MenuInfo *menuInfo, int idx);
extern int AppMenuFunc(Display *dpy, WinGeneric *winInfo, MenuInfo *menuInfo, int idx);

#endif /* _OLWM_SERVICES_H */
