/* @(#) %M% V%I% %E% %U% $Id: selection.h,v 1.5 2025/06/20 20:37:04 dra Exp $ */
/* #ident	"@(#)selection.h	1.3	93/06/28 SMI" */

/*
 *      (c) Copyright 1992 Sun Microsystems, Inc.
 */

/*
 *	Sun design patents pending in the U.S. and foreign countries. See
 *	LEGAL_NOTICE file for terms of the license.
 */

#ifndef _OLWM_SELECTION_H
#define _OLWM_SELECTION_H

extern	Time	SelectionTime;

struct _client;

extern	Bool IsSelected(struct _client *cli);
extern void AddSelection(struct _client *cli, Time timestamp);
extern	Bool RemoveSelection(struct _client *cli);
extern	Bool	ToggleSelection(/*  client, time  */);
extern	void	ClearSelections(/*  dpy  */);
extern	struct _client* EnumSelections(/*  void*  */);

extern	void	SelectionInit();
typedef void (*SelectionHandler)(XEvent *);
extern	void	SelectionRegister(Atom seln, SelectionHandler handler);
extern	void	SelectionResponse(/*  event  */);

#endif /* _OLWM_SELECTION_H */
