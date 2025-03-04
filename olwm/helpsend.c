/* #ident	"@(#)helpsend.c	26.8	93/06/28 SMI" */
char helpsend_c_sccsid[] = "@(#) %M% V%I% %E% %U% $Id: helpsend.c,v 2.1 2024/09/20 19:59:01 dra Exp $";

/*
 *      (c) Copyright 1989 Sun Microsystems, Inc.
 */

/*
 *      Sun design patents pending in the U.S. and foreign countries. See
 *      LEGAL_NOTICE file for terms of the license.
 */

/* ----------------------------------------------------------------------
 *	helpsend.c
 * ---------------------------------------------------------------------*/

#include "helpcmd.h"

extern void SlaveStartOnDemand(void);

/* ----------------------------------------------------------------------
 *	ShowHelpWindow
 * ---------------------------------------------------------------------*/
int ShowHelpWindow(nscreen,mousex,mousey,helpkey)
	int	nscreen;
	int	mousex,mousey;
	char	*helpkey;
{
	SlaveStartOnDemand();

	helpCmdAttr[HW_SCREEN_NO].value.ival 	= nscreen;
	helpCmdAttr[HW_MOUSE_X].value.ival 	= mousex;
	helpCmdAttr[HW_MOUSE_Y].value.ival 	= mousey;
	helpCmdAttr[HW_HELPKEY].value.sval 	= helpkey;
	return (SendCmd(&helpCommand));
}
