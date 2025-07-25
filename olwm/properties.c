/* #ident	"@(#)properties.c	26.15	93/06/28 SMI" */
char properties_c_sccsid[] = "@(#) %M% V%I% %E% %U% $Id: properties.c,v 2.7 2025/06/20 20:36:52 dra Exp $";

/*
 *      (c) Copyright 1989 Sun Microsystems, Inc.
 */

/*
 *      Sun design patents pending in the U.S. and foreign countries. See
 *      LEGAL_NOTICE file for terms of the license.
 */

/* 
 * properties.c
 */

#include <errno.h>
#include <stdio.h>
#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include "i18n.h"
#include "olwm.h"
#include "win.h"
#include "mem.h"
#include "properties.h"
#include "atom.h"

/***************************************************************************
 * external data
 ***************************************************************************/

/***************************************************************************
 * 	GetWindowProperty
 ***************************************************************************/

/*
 * GetWindowProperty - wrapper around XGetWindowProperty()
 *	Returns NULL on a variety of error states; no such property,
 *	not requested type or format.
 *	Returned memory should be free'd with XFree() since it
 *	is allocated by XGetWindowProperty() internally.
 */
void *GetWindowProperty(Display *dpy, Window w, Atom property, long long_offset,
		long long_length, Atom req_type, int req_fmt, unsigned long *nitems,
		unsigned long *bytes_after)
{
	int status;
	unsigned char *prop;
	Atom act_type;
	int act_format;

	dra_olwm_trace(121, "prop: getting %ld\n", property);
	status = XGetWindowProperty(dpy, w, property, long_offset, long_length,
		    False, req_type, &act_type, &act_format, nitems,
		    bytes_after, &prop);
	if ((status != Success) || (act_type != req_type)) {
		dra_olwm_trace(121, "prop: status %d, typ %d , req_typ %d\n",
			status, act_type, req_type);
		*nitems = 0;
		return NULL;
	} 
	if ((req_fmt != 0) && (act_format != req_fmt)) {
		XFree((char *)prop);
		dra_olwm_trace(121, "prop: fmt %d, req_fmt %d\n", act_format, req_fmt);
		*nitems = 0;
		return NULL;
	}
	return (void *)prop;
}

/*
 * propGetTextProp - returns a display text property
 */
static Bool
propGetTextProp(dpy,win,property,text)
	Display	*dpy;
	Window	win;
	Atom	property;
	Text	**text;			/* RETURN */
{
	XTextProperty	textProp;
	Bool		ret = False;
#ifdef OW_I18N_L4
	wchar_t		**list;
	int		count;
	int		status;
#endif

	if (XGetTextProperty(dpy,win,&textProp,property) == 0) {
		*text = NULL;
		return False;
	}


#ifdef OW_I18N_L4
	if (textProp.format != 8) {
		*text = NULL;
	} else {
		status = XwcTextPropertyToTextList(dpy, &textProp, &list, &count);
		if (count >= 1 && status == Success) {
			*text = MemNewText(list[0]);
			XwcFreeStringList(list);
			ret = True;
		} else
			*text = NULL;
	}
		
#else
	if (
		/* recently, they also come with type = UTF8_STRING - I don't care  */
	    textProp.format   == 8) {
		*text = MemNewText(textProp.value);
		ret = True;
	}
	else {
		*text = NULL;
		ret = False;
	}

	XFree((char *)textProp.value);
#endif
	return ret;
}


/***************************************************************************
 * 	Property Filter Functions
 ***************************************************************************/

/*
 * PropListAvailable - returns a set of flags representing the properties
 *	available on the passed window.
 */
long
PropListAvailable(dpy,win)
	Display	*dpy;
	Window	win;
{
	Atom	*atomList;
	int	i,count;
	long	retFlags;

	if ((atomList = XListProperties(dpy,win,&count)) == NULL)
		return 0L;

	retFlags = 0L;

	for (i = 0; i < count; i++) {
		if (atomList[i] == XA_WM_CLASS)
			retFlags |= WMClassAvail;
		else if (atomList[i] == XA_WM_NAME)
			retFlags |= WMNameAvail;
		else if (atomList[i] == XA_WM_ICON_NAME)
			retFlags |= WMIconNameAvail;
		else if (atomList[i] == XA_WM_NORMAL_HINTS)
			retFlags |= WMNormalHintsAvail;
		else if (atomList[i] == XA_WM_HINTS)
			retFlags |= WMHintsAvail;
		else if (atomList[i] == AtomProtocols)
			retFlags |= WMProtocolsAvail;
		else if (atomList[i] == XA_WM_TRANSIENT_FOR)
			retFlags |= WMTransientForAvail;
		else if (atomList[i] == AtomColorMapWindows)
			retFlags |= WMColormapWindowsAvail;
		else if (atomList[i] == AtomWMState)
			retFlags |= WMStateAvail;
		else if (atomList[i] == AtomWinAttr)
			retFlags |= OLWinAttrAvail;
		else if (atomList[i] == AtomDecorAdd)
			retFlags |= OLDecorAddAvail;
		else if (atomList[i] == AtomDecorDel)
			retFlags |= OLDecorDelAvail;
		else if (atomList[i] == AtomSunWindowState)
			retFlags |= OLWindowStateAvail;
		else if (atomList[i] == AtomLeftFooter)
			retFlags |= OLLeftFooterAvail;
		else if (atomList[i] == AtomRightFooter)
			retFlags |= OLRightFooterAvail;
#ifdef OW_I18N_L4
		else if (atomList[i] == AtomLeftIMStatus)
			retFlags |= OLLeftIMStatusAvail;
		else if (atomList[i] == AtomRightIMStatus)
			retFlags |= OLRightIMStatusAvail;
#endif
		else if (atomList[i] == Atom_NET_WM_ICON)
			retFlags |= NetWMIconAvail;
		else if (atomList[i] == AtomWinColors)
			retFlags |= OLWinColorsAvail;
	}

	XFree((char *)atomList);

	return retFlags;
}

/*
 * Property availability control
 */
static struct {
	Window	win;
	long	flags;
} propAvailable;

#define PropAvailable(w,f) ((propAvailable.win == None) || \
			    (propAvailable.win == (w) && \
			     (propAvailable.flags & (f))))

/*
 * PropSetAvailable - sets the property read filter for that window
 */
void
PropSetAvailable(dpy,win)
	Display	*dpy;
	Window	win;
{
	propAvailable.win = win;
	propAvailable.flags = PropListAvailable(dpy,win);
}

/*
 * PropClearAvailable - turns off the property read filter
 */
void
PropClearAvailable()
{
	propAvailable.win = None;
	propAvailable.flags = ~0;
}


/***************************************************************************
 * 	Property Get Functions
 ***************************************************************************/

/*
 * PropGetWMName - gets the WM_NAME property
 */
Bool
PropGetWMName(dpy,win,winName)
	Display	*dpy;
	Window	win;
	Text	**winName;		/* RETURN */
{
	if (!PropAvailable(win,WMNameAvail))
		return False;

	if (!propGetTextProp(dpy,win,XA_WM_NAME,winName))
		return False;

	return True;
}

/*
 * PropGetWMIconName - gets the WM_ICON_NAME property
 */
Bool
PropGetWMIconName(dpy,win,iconName)
	Display	*dpy;
	Window	win;
	Text	**iconName;		/* RETURN */
{
	if (!PropAvailable(win,WMIconNameAvail))
		return False;

	if (!propGetTextProp(dpy,win,XA_WM_ICON_NAME,iconName))
		return False;

	return True;
}


/*
 * PropGetWMClass - gets the WM_CLASS property with the class and instance 
 *	strings.
 */
Bool
PropGetWMClass(dpy,win,class,instance)
	Display	*dpy;
	Window	win;
	char	**class;		/* RETURN */
	char	**instance;		/* RETURN */
{
	XClassHint	classHint;

	if (!PropAvailable(win,WMClassAvail))
		return False;

	if (XGetClassHint(dpy,win,&classHint) == 0)
		return False;

	if (classHint.res_name) {
		*instance = MemNewString(classHint.res_name);
		XFree(classHint.res_name);
	}

	if (classHint.res_class) {
		*class = MemNewString(classHint.res_class);
		XFree(classHint.res_class);
	}

	return True;
}
/*
 * PropGetWMHints - get the WM_HINTS property
 */
Bool PropGetWMHints(Display *dpy, Window win, XWMHints	*wmHints)
{
	XWMHints *prop;

	if (!PropAvailable(win,WMHintsAvail))
		return False;

	if ((prop = XGetWMHints(dpy,win)) == (XWMHints *)NULL)
		return False;

	/* what I want to know from EACH client (except idiots):
	 * input and initial_state
	 */
	if ((prop->flags & StateHint) == 0) {
		prop->flags |= StateHint;
		prop->initial_state = NormalState;
	}
	if ((prop->flags & InputHint) == 0) {
		prop->flags |= InputHint;
		prop->input = 1;
	}

	*wmHints = *prop;

	XFree((char *)prop);

	return True;
}

/*
 * PropGetWMNormalHints - get the WM_NORMAL_HINTS property.
 *
 *	preICCCM is true if we got a short/old property as indicated
 *	by a supplied flag of PWinGravity (added by ICCCM)
 */
Bool
PropGetWMNormalHints(dpy,win,sizeHints,preICCCM)
	Display		*dpy;
	Window		win;
	XSizeHints	*sizeHints;	/* RETURN */
	Bool		*preICCCM;	/* RETURN */
{
	long		supplied;

	*preICCCM = False;

	if (!PropAvailable(win,WMNormalHintsAvail))
		return False;

	if (XGetWMNormalHints(dpy,win,sizeHints,&supplied) == 0) 
		return False;

	if (!(supplied & PWinGravity))
		*preICCCM = True;

	if (!(sizeHints->flags & PWinGravity)) {
		sizeHints->win_gravity = NorthWestGravity;
		sizeHints->flags |= PWinGravity;
	} else if (sizeHints->win_gravity == 0) {
		sizeHints->win_gravity = NorthWestGravity;
	}

	if (sizeHints->flags & PResizeInc) {
		if (sizeHints->width_inc <= 0 || 
		    sizeHints->height_inc <= 0)
			sizeHints->flags ^= PResizeInc;
	}

	if (sizeHints->flags & PAspect) {
		if (sizeHints->min_aspect.x <= 0 ||
		    sizeHints->min_aspect.y <= 0 ||
		    sizeHints->max_aspect.x <= 0 ||
		    sizeHints->max_aspect.y <= 0)
			sizeHints->flags ^= PAspect;
	}

	return True;
}

/*
 * PropGetWMProtocols - get the protocols in which the client will participate.
 *	Convert the individual atoms into protocol flags.
 */
Bool
PropGetWMProtocols(dpy,win,protocols)
	Display	*dpy;
	Window	win;
	int	*protocols;
{
	Atom	*atomList;
	int	i,count;

	if (!PropAvailable(win,WMProtocolsAvail))
		return False;

	if (XGetWMProtocols(dpy,win,&atomList,&count) == 0)
		return False;

	*protocols = 0;

	for (i = 0; i < count; i++) {
		if (atomList[i] == AtomTakeFocus)
			*protocols |= TAKE_FOCUS;
		else if (atomList[i] == AtomSaveYourself)
			*protocols |= SAVE_YOURSELF;
		else if (atomList[i] == AtomDeleteWindow)
			*protocols |= DELETE_WINDOW;
		else if (atomList[i] == AtomShowProperties)
			*protocols |= HAS_PROPWIN;
		else if (atomList[i] == AtomGroupManaged)
			*protocols |= GROUP_MANAGED;
		else if (atomList[i] == AtomWarpBack)
			*protocols |= WARP_BACK;
		else if (atomList[i] == AtomNoWarping)
			*protocols |= NO_WARPING;
		else if (atomList[i] == AtomWarpToPin)
			*protocols |= WARP_TO_PIN;
		else if (atomList[i] == AtomOwnHelp)
			*protocols |= OWN_HELP;
		else if (atomList[i] == AtomWTBase)
			*protocols |= SECONDARY_BASE;
		else if (atomList[i] == AtomIsWSProps)
			*protocols |= IS_WS_PROPS;
		else if (atomList[i] == AtomAllowIconSize)
			*protocols |= ALLOW_ICON_SIZE;
	}

	XFree((char *)atomList);

	return True;
}

/*
 * PropGetWMTransientFor
 *
 * Get the WM_TRANSIENT_FOR hint.  If the property exists but has a
 * contents of zero, or the window itself, substitute the root's
 * window ID.  This is because some (buggy) clients actually write
 * zero in the WM_TRANSIENT_FOR property, and we want to give them
 * transient window behavior.
 */
Bool
PropGetWMTransientFor(dpy,win,root,transientFor)
	Display	*dpy;
	Window	win;
	Window	root;
	Window	*transientFor;		/* RETURN */
{
	Window	transient;

	if (!PropAvailable(win,WMTransientForAvail))
		return False;

	if (XGetTransientForHint(dpy,win,&transient) == 0) 
		return False;

	if (transient != 0 && transient != win)
		*transientFor = transient;
	else
		*transientFor = root;
	
	return True;
}

/*
 * PropGetWMColormapWindows -
 */
Bool
PropGetWMColormapWindows(dpy,win,wins,count)
	Display	*dpy;
	Window	win;
	Window	**wins;
	int	*count;
{
	if (!PropAvailable(win,WMColormapWindowsAvail))
		return False;

	if (XGetWMColormapWindows(dpy,win,wins,count) == 0)
		return False;

	return True;
}

/*
 * PropGetWMState -- get the contents of the WM_STATE property.
 *	The first datum is the state (NormalState/IconicState/WithdrawnState)
 *	and the second is the icon window
 */
Bool
PropGetWMState(dpy,win,state,iconwin)
	Display	*dpy;
	Window 	win;
	int	*state;			/* RETURN */
	Window	*iconwin;		/* RETURN */
{
	unsigned int nItems,remain;
	long *data;

	if (!PropAvailable(win,WMStateAvail))
		return False;

	data = GetWindowProperty(dpy,win,AtomWMState,0L, 
			LONG_LENGTH(int)+LONG_LENGTH(Window),
			AtomWMState,32,&nItems,&remain);

	if (data == NULL) {
		*state = NormalState;
		*iconwin = None;
		return False;
	}

	if (nItems > 0)
		*state = data[0];
	else
		*state = NormalState;

	if (nItems > 1)
		*iconwin = (Window)data[1];
	else
		*iconwin = None;

	XFree((char *)data);

	return True;
}

/*
 * PropSetWMState - writes the WM_STATE property
 */
void
PropSetWMState(dpy,win,state,iconwin)
	Display	*dpy;
	Window 	win;
	int	state;
	Window	iconwin;
{
	unsigned long data[2];

	data[0] = state;
	data[1] = iconwin;

	XChangeProperty(dpy,win,AtomWMState,AtomWMState,32,
		PropModeReplace,(unsigned char *)data,2);
}

Bool PropGetNetWMIcon(Display *dpy, Window win, unsigned long **much,
									unsigned long *length)
{
	unsigned long *data, rest;

	if (!PropAvailable(win, NetWMIconAvail)) return False;
	data = GetWindowProperty(dpy, win, Atom_NET_WM_ICON, 0L, ENTIRE_CONTENTS,
						XA_CARDINAL, 32, length, &rest);

	if (! data) return False;

	*much = data;

	return True;
}

#define OL_WINDOW_STATE_LENGTH (sizeof(OLWindowState)/sizeof(unsigned long))
/*
 * PropGetOLWindowState - reads the _SUN_WINDOW_STATE property
 */
Bool PropGetOLWindowState(Display *dpy, Window win, OLWindowState	*winState) /* RETURN */
{
	OLWindowState	*newState;
	unsigned int	nItems,remain;

	if (!PropAvailable(win,OLWindowStateAvail))
		return False;

	newState = GetWindowProperty(dpy,win,AtomSunWindowState,
		0L,OL_WINDOW_STATE_LENGTH,XA_INTEGER,32,&nItems,&remain);

	if (newState == NULL)
		return False;

	if (nItems != OL_WINDOW_STATE_LENGTH) {
		XFree((char *)newState);
		return False;
	}

	*winState = *newState;

	XFree((char *)newState);

	return True;
}

/*
 * Old OLWinAttr structure; used for compatability with existing
 * old clients; will convert into new structure.
 */
typedef struct {
	Atom		win_type;
	Atom		menu_type;
	unsigned long	pin_initial_state;
} oldOLWinAttr;
#define OLDOLWINATTRLENGTH (sizeof(oldOLWinAttr)/sizeof(unsigned long))

/*
 * PropGetOLWinAttr
 */
Bool
PropGetOLWinAttr(dpy,win,winAttr,oldVersion)
	Display		*dpy;
	Window		win;
	OLWinAttr	*winAttr;		/* RETURN */
	Bool		*oldVersion;
{
	void		*attrdata;
	unsigned long	nItems,remain;

	*oldVersion = False;

	if (!PropAvailable(win,OLWinAttrAvail))
		return False;

	attrdata = GetWindowProperty(dpy,win,AtomWinAttr,0L,ENTIRE_CONTENTS,
					 AtomWinAttr,0,&nItems,&remain);

	/*
	 * If it's not there
	 */
	if (attrdata == NULL)
		return False;

	/*
	 * It's either old or new size.  If old size then convert 
	 * it to new structure
	 */
	if (nItems == OLDOLWINATTRLENGTH) {

		*oldVersion = True;

		winAttr->flags = WA_WINTYPE | WA_MENUTYPE | WA_PINSTATE;
		winAttr->win_type = ((oldOLWinAttr *)attrdata)->win_type;
		winAttr->menu_type = ((oldOLWinAttr *)attrdata)->menu_type;
		winAttr->pin_initial_state =
				((oldOLWinAttr *)attrdata)->pin_initial_state;

	} else if (nItems == OLWINATTRLENGTH) { 	
		*winAttr = *(OLWinAttr *)attrdata;
	} else {	/* wrong size */
		XFree((char *)attrdata);
		return False;
	}

    	/* 
     	 * Convert the pushpin's initial state.
     	 * 
     	 * There's some backwards compatibility code here.
     	 * Older clients use the _OL_PIN_IN and _OL_PIN_OUT atoms
     	 * here, whereas the OLXCI specifies zero as out and one as
     	 * in.  Convert old into new.
     	 */
    	if (winAttr->flags & WA_PINSTATE) {
		if (winAttr->pin_initial_state == AtomPinIn)
			winAttr->pin_initial_state = PIN_IN;
		else if (winAttr->pin_initial_state == AtomPinOut)
			winAttr->pin_initial_state = PIN_OUT;
	}

	XFree((char *)attrdata);

	return True;
}

/*
 * propGetOLDecor - gets either _OL_DECOR_ADD or _OL_DECOR_DEL list of
 *	decoration atoms and converts it into a set of flags.
 */
static Bool
propGetOLDecor(dpy,win,atom,decorFlags)
	Display	*dpy;
	Window	win;
	Atom	atom;
	int	*decorFlags;
{
	Atom	*atomList;
	unsigned long nItems,remain;
	int	i;

	atomList = (Atom *)GetWindowProperty(dpy,win,atom,
			0L,ENTIRE_CONTENTS,XA_ATOM,0,&nItems,&remain);

	if (!atomList || nItems == 0) {
		if (atomList)
			XFree((char *)atomList);
		return False;
	}

	*decorFlags = 0;

	for (i = 0; i < nItems; i++) {
		if (atomList[i] == AtomDecorClose)
			*decorFlags |= WMDecorationCloseButton;
		else if (atomList[i] == AtomDecorFooter)
			*decorFlags |= WMDecorationFooter;
		else if (atomList[i] == AtomDecorResize)
			*decorFlags |= WMDecorationResizeable;
		else if (atomList[i] == AtomDecorHeader)
			*decorFlags |= WMDecorationHeader;
		else if (atomList[i] == AtomDecorPin)
			*decorFlags |= WMDecorationPushPin;
		else if (atomList[i] == AtomDecorIconName)
			*decorFlags |= WMDecorationIconName;
#ifdef OW_I18N_L4
                else if (atomList[i] == AtomDecorIMStatus)
                        *decorFlags |= WMDecorationIMStatus;
#endif		
	}

	XFree((char *)atomList);

	return True;
}

/*
 * PropGetOLDecorAdd - gets the _OL_DECOR_ADD property
 */
Bool
PropGetOLDecorAdd(dpy,win,decorFlags)
	Display	*dpy;
	Window	win;
	int	*decorFlags;
{
	if (!PropAvailable(win,OLDecorAddAvail))
		return False;

	if (!propGetOLDecor(dpy,win,AtomDecorAdd,decorFlags))
		return False;

	return True;
}

/*
 * PropGetOLDecorDel - gets the _OL_DECOR_DEL property
 */
Bool
PropGetOLDecorDel(dpy,win,decorFlags)
	Display	*dpy;
	Window	win;
	int	*decorFlags;
{
	if (!PropAvailable(win,OLDecorDelAvail))
		return False;

	if (!propGetOLDecor(dpy,win,AtomDecorDel,decorFlags))
		return False;

	return True;
}

/*
 * PropGetOLLeftFooter - gets the left footer string
 */
Bool PropGetOLLeftFooter(Display *dpy, Window win, Text **footer)
{
	if (!PropAvailable(win,OLLeftFooterAvail))
		return False;

	if (!propGetTextProp(dpy,win,AtomLeftFooter,footer))
		return False;

	return True;
}

/*
 * PropGetOLRightFooter - gets the right footer string
 */
Bool PropGetOLRightFooter(Display *dpy, Window win, Text **footer)
{
	if (!PropAvailable(win,OLRightFooterAvail))
		return False;

	if (!propGetTextProp(dpy,win,AtomRightFooter,footer))
		return False;

	return True;
}

#ifdef OW_I18N_L4

/*
 * PropGetOLLeftIMStatus - gets the left IM status string
 */
Bool
PropGetOLLeftIMStatus(dpy,win,status)
	Display	*dpy;
	Window	win;
	Text	**status;			/* RETURN */
{
	if (!PropAvailable(win,OLLeftIMStatusAvail))
		return False;

	if (!propGetTextProp(dpy,win,AtomLeftIMStatus,status))
		return False;

	return True;
}

/*
 * PropGetOLRightIMStatus - gets the right IM status string
 */
Bool
PropGetOLRightIMStatus(dpy,win,status)
	Display	*dpy;
	Window	win;
	Text	**status;			/* RETURN */
{
	if (!PropAvailable(win,OLRightIMStatusAvail))
		return False;

	if (!propGetTextProp(dpy,win,AtomRightIMStatus,status))
		return False;

	return True;
}

#endif
